/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

// 1/5" s5k5ccaf

#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>


#include "s5k5ccaf.h"
#include <mach/camera.h>
#include <mach/vreg.h>
#include <linux/io.h>


#include "sec_cam_pmic.h"

#include "sec_cam_dev.h"

#if defined(CONFIG_TARGET_SERIES_P5LTE)
#include "s5k5ccaf_regs_p5.h"
#elif defined(CONFIG_TARGET_SERIES_P8LTE)
#include "s5k5ccaf_regs_p8_skt.h"
#else
#include "s5k5ccgx_regs.h"
#endif

#define	USE_VIDEO_SIZE

//#define CONFIG_LOAD_FILE

#ifdef CONFIG_LOAD_FILE
#define S5K5CCAF_WRITE_LIST(A)	s5k5ccaf_sensor_write_list(A,(sizeof(A) / sizeof(A[0])),#A);
#else
#define S5K5CCAF_WRITE_LIST(A)	s5k5ccaf_sensor_burst_write_list(A,(sizeof(A) / sizeof(A[0])),#A);
#endif


#define CHECK_ERR(x)   \
		do {\
		if ((x) < 0) { \
			printk("i2c falied, err %d\n", x); \
			x = -1; \
			return x; \
				}	\
		} while(0)

		
#define CAM_DELAY(x)   \
		printk("[S5K5CCAF] %d ms delay\n", x);  \
		msleep(x);
		


struct s5k5ccaf_work {
	struct work_struct work;
};

static struct  s5k5ccaf_work *s5k5ccaf_sensorw;
static struct  i2c_client *s5k5ccaf_client;

static unsigned int config_csi;
static struct s5k5ccaf_ctrl *s5k5ccaf_ctrl;
int af_low_lux;
int torch_light_on;


static DECLARE_WAIT_QUEUE_HEAD(s5k5ccaf_wait_queue);
DECLARE_MUTEX(s5k5ccaf_sem);


/*=============================================================
	EXTERNAL DECLARATIONS
==============================================================*/

#ifdef CONFIG_LOAD_FILE
static int s5k5ccaf_regs_table_write(char *name);
#endif

static int s5k5ccaf_sensor_read(unsigned short subaddr, unsigned short *data)
{
	int ret;
	unsigned char buf[2];
	struct i2c_msg msg = { s5k5ccaf_client->addr, 0, 2, buf };
	
	buf[0] = (subaddr >> 8);
	buf[1] = (subaddr & 0xFF);

	ret = i2c_transfer(s5k5ccaf_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	if (ret == -EIO) 
		goto error;

	msg.flags = I2C_M_RD;
	
	ret = i2c_transfer(s5k5ccaf_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	if (ret == -EIO) 
		goto error;

	*data = ((buf[0] << 8) | buf[1]);

error:
	return ret;
}

static int s5k5ccaf_sensor_write(unsigned short subaddr, unsigned short val)
{
	unsigned char buf[4];
	struct i2c_msg msg = { s5k5ccaf_client->addr, 0, 4, buf };

	//CAM_DEBUG("addr = %4X, value = %4X", subaddr, val);

	buf[0] = (subaddr >> 8);
	buf[1] = (subaddr & 0xFF);
	buf[2] = (val >> 8);
	buf[3] = (val & 0xFF);

	return i2c_transfer(s5k5ccaf_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
}

#ifdef CONFIG_LOAD_FILE
static int s5k5ccaf_sensor_write_list(const u32 *list, int size, char *name)
{
	int ret = 0;	
	int i = 0;
	
	unsigned short subaddr = 0, next_subaddr = 0, value = 0;

	printk("s5k5ccaf_sensor_write_list : %s\n", name);	
	
#ifdef CONFIG_LOAD_FILE	
	ret = s5k5ccaf_regs_table_write(name);
#else
	subaddr = (list[i]>> 16); //address
	if(subaddr == 0x0F12) next_subaddr= (list[i+1]>> 16); //address
		value = (list[i] & 0xFFFF); //value

	for (i = 0; i < size; i++) {
		if(subaddr == 0xffff)	{
			msleep(value);
			printk("sensor delay %d ms\n" , value);
		} else {
		    if(s5k5ccaf_sensor_write(subaddr, value) < 0)   {
			    printk("sensor_write_list fail...\n");
			    return -1;
		    }
		}
	}
#endif
	return ret;
}
#endif

#define BURST_MODE_BUFFER_MAX_SIZE 2700
unsigned char s5k5ccaf_buf_for_burstmode[BURST_MODE_BUFFER_MAX_SIZE];
static int s5k5ccaf_sensor_burst_write_list(const u32 *list, int size, char *name)
{
	int err = 0;
	int i = 0;
	int idx = 0;

	unsigned short subaddr = 0, next_subaddr = 0, value = 0;

	struct i2c_msg msg = { s5k5ccaf_client->addr, 0, 0, s5k5ccaf_buf_for_burstmode };
	
	cam_info("burst_write_list : %s", name);

	for (i = 0; i < size; i++) {
		if(idx > (BURST_MODE_BUFFER_MAX_SIZE-10)) {
			cam_err("BURST MODE buffer overflow!!!");
			return err;
		}

		subaddr = (list[i]>> 16); //address
		if(subaddr == 0x0F12) 
			next_subaddr= (list[i+1]>> 16); //address
		value = (list[i] & 0xFFFF); //value
		

		switch(subaddr) {
		case 0x0F12:
			// make and fill buffer for burst mode write
			if(idx ==0) {
				s5k5ccaf_buf_for_burstmode[idx++] = 0x0F;
				s5k5ccaf_buf_for_burstmode[idx++] = 0x12;
			}
			s5k5ccaf_buf_for_burstmode[idx++] = value>> 8;
			s5k5ccaf_buf_for_burstmode[idx++] = value & 0xFF;

		
		 	//write in burstmode	
			if(next_subaddr != 0x0F12) 	{
				msg.len = idx;
				err = i2c_transfer(s5k5ccaf_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
				//printk("s5k4ecgx_sensor_burst_write, idx = %d\n",idx);
				idx=0;
			}
			break;

		case 0xFFFF:
			CAM_DELAY(value);
			break;
	
		default:
		    idx = 0;
		    err = s5k5ccaf_sensor_write(subaddr, value);
			break;
		}	
	}

	if (unlikely(err < 0)) {
		cam_err("%s: register set failed\n", __func__);
		return err;
	}

	return 0;
}

int s5k5ccaf_get_light_level(void)
{
	unsigned short	msb, lsb;
	unsigned short cur_lux = 0;
	
	msb = lsb = 0;
	s5k5ccaf_sensor_write(0xFCFC, 0xD000);
	s5k5ccaf_sensor_write(0x002C, 0x7000);
	s5k5ccaf_sensor_write(0x002E, 0x2A3C);
	s5k5ccaf_sensor_read(0x0F12, &lsb);
	s5k5ccaf_sensor_read(0x0F12, &msb);

	cur_lux = (msb<<16) | lsb;

	cam_info("cur_lux = %d", cur_lux);

	return cur_lux;
}

int s5k5ccaf_set_flash(int mode, int onoff)
{
	int rc = 0;
	
	if(torch_light_on) {
		cam_info("can't control Flash in torch mode ON");
		return 0;
	}
	
	if(onoff)
		onoff = 1;
	else
		onoff = 0;
	
	if(mode == CAPTURE_FLASH) {	// flash mode
		cam_info("CAM_FLASH_EN : %d", onoff);
		gpio_set_value_cansleep(CAM_MOVIE_EN, 0);
		gpio_set_value_cansleep(CAM_FLASH_EN, onoff);

	} else {	// movie mode
		cam_info("CAM_MOVIE_EN : %d", onoff);
		gpio_set_value_cansleep(CAM_FLASH_EN, 0);
		gpio_set_value_cansleep(CAM_MOVIE_EN, onoff);
	}

	return rc;
}

int s5k5ccaf_set_effect(int effect)
{
	int rc = 0;
	
	switch(effect) {
		case CAMERA_EFFECT_OFF:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_effect_off);
			break;
			
		case CAMERA_EFFECT_NEGATIVE:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_effect_negative);
			break;
			
		case CAMERA_EFFECT_MONO:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_effect_mono);
			break;
			
		case CAMERA_EFFECT_SEPIA:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_effect_sepia);
			break;
			
		default:
			printk("Invalid effect (%d) !!!\n", effect);
			return rc;
	}
	
	s5k5ccaf_ctrl->settings.effect = effect;

	return rc;
}


int s5k5ccaf_set_whitebalance(int wb)
{
	int rc = 0;
#if defined(CONFIG_MACH_P5_LTE)
	if (s5k5ccaf_ctrl->hd_enabled) {
		CAM_DEBUG("hd_enabled");
		
		switch(wb) {
			case WHITE_BALANCE_AUTO:
				S5K5CCAF_WRITE_LIST(s5k5ccaf_wb_auto_HD_Camera);
				break;

			case WHITE_BALANCE_SUNNY:
				S5K5CCAF_WRITE_LIST(s5k5ccaf_wb_daylight_HD_Camera);
				break;

			case WHITE_BALANCE_CLOUDY:
				S5K5CCAF_WRITE_LIST(s5k5ccaf_wb_cloudy_HD_Camera);
				break;

			case WHITE_BALANCE_FLUORESCENT:
				S5K5CCAF_WRITE_LIST(s5k5ccaf_wb_fluorescent_HD_Camera);
				break;

			case WHITE_BALANCE_INCANDESCENT:
				S5K5CCAF_WRITE_LIST(s5k5ccaf_wb_incandescent_HD_Camera);
				break;
				
			default:
				printk("Invalid wb (%d) !!!\n", wb);
				return rc;		
		}
	} else
#endif
	{
		switch(wb) {
			case WHITE_BALANCE_AUTO:
				S5K5CCAF_WRITE_LIST(s5k5ccaf_wb_auto);
				break;

			case WHITE_BALANCE_SUNNY:
				S5K5CCAF_WRITE_LIST(s5k5ccaf_wb_daylight);
				break;

			case WHITE_BALANCE_CLOUDY:
				S5K5CCAF_WRITE_LIST(s5k5ccaf_wb_cloudy);
				break;

			case WHITE_BALANCE_FLUORESCENT:
				S5K5CCAF_WRITE_LIST(s5k5ccaf_wb_fluorescent);
				break;

			case WHITE_BALANCE_INCANDESCENT:
				S5K5CCAF_WRITE_LIST(s5k5ccaf_wb_incandescent);
				break;
			
			default:
				printk("Invalid wb (%d) !!!\n", wb);
				return rc;		
		}
	}

	s5k5ccaf_ctrl->settings.wb = wb;
	
	return rc;
}

int s5k5ccaf_set_brightness(int brightness)
{
	int rc = 0;

	switch(brightness) {
		case EV_PLUS_4 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_brightness_p_4);
			break;
			
		case EV_PLUS_3 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_brightness_p_3);
			break;

		case EV_PLUS_2 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_brightness_p_2);
			break;

		case EV_PLUS_1 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_brightness_p_1);
			break;

		case EV_DEFAULT :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_brightness_0);
			break;

		case EV_MINUS_1 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_brightness_m_1);
			break;

		case EV_MINUS_2 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_brightness_m_2);
			break;

		case EV_MINUS_3 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_brightness_m_3);
			break;

		case EV_MINUS_4 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_brightness_m_4);
			break;

		default:
			printk("Invalid brightness (%d) !!!\n", brightness);
			return rc;
	}

	s5k5ccaf_ctrl->settings.brightness = brightness;
	
	return rc;
	
}


int s5k5ccaf_set_iso(int iso)
{
	int rc = 0;
	
	switch(iso) {
		case ISO_AUTO :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_iso_auto);
			break;

		case ISO_50 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_iso_50);
			break;

		case ISO_100 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_iso_100);
			break;

		case ISO_200 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_iso_200);
			break;

		case ISO_400 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_iso_400);
			break;

		default:
			printk("Invalid iso (%d) !!!\n", iso);
			return rc;
	}
	
	s5k5ccaf_ctrl->settings.iso = iso;
	
	return rc;

}

int s5k5ccaf_set_metering(char metering)
{
	int rc = 0;

	switch(metering) {
		case METERING_MATRIX :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_metering_normal);
			break;
			
		case METERING_SPOT :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_metering_spot);
			break;

		case METERING_CENTER :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_metering_center);
			break;

		default:
			printk("Invalid metering (%d) !!!\n", metering);
			return rc;
	}
	
	s5k5ccaf_ctrl->settings.metering = metering;
	
	return rc;
}

int s5k5ccaf_set_scene(int scene)
{
	int rc = 0;
	
	switch(scene) {
		case SCENE_MODE_NONE :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_off);
			break;

		case SCENE_MODE_PORTRAIT :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_off);
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_portrait);
			break;

		case SCENE_MODE_LANDSCAPE :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_off);
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_landscape);
			break;

		case SCENE_MODE_SPORTS :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_off);
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_sports);
			break;

		case SCENE_MODE_PARTY_INDOOR :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_off);
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_party);
			break;

		case SCENE_MODE_BEACH_SNOW :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_off);
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_beach);
			break;

		case SCENE_MODE_SUNSET :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_off);
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_sunset);
			break;

		case SCENE_MODE_DUSK_DAWN :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_off);
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_dawn);
			break;

		case SCENE_MODE_FALL_COLOR :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_off);
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_fall);
			break;

		case SCENE_MODE_NIGHTSHOT :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_off);
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_nightshot);
			break;

		case SCENE_MODE_BACK_LIGHT :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_off);
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_backlight);
			break;

		case SCENE_MODE_FIREWORKS :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_off);
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_firework);
			break;

		case SCENE_MODE_TEXT :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_off);
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_text);
			break;

		case SCENE_MODE_CANDLE_LIGHT:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_off);
			S5K5CCAF_WRITE_LIST(s5k5ccaf_scene_candle);
			break;

		default:
			printk("Invalid scene (%d) !!!\n", scene);
			return rc;
	}

	s5k5ccaf_ctrl->settings.scene = scene;
	
	return rc;
}

int s5k5ccaf_set_contrast(int contrast)
{
	int rc = 0;

	switch(contrast) {
		case S5K5CCGX_CONTRAST_MINUS_2:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_contrast_m_2);
			break;

		case S5K5CCGX_CONTRAST_MINUS_1:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_contrast_m_1);
			break;

		case S5K5CCGX_CONTRAST_DEFAULT:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_contrast_0);
			break;
			
		case S5K5CCGX_CONTRAST_PLUS_1:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_contrast_p_1);
			break;

		case S5K5CCGX_CONTRAST_PLUS_2:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_contrast_p_2);
			break;

		default:
			printk("Invalid contrast (%d) !!!\n", contrast);
			return rc;
	}

	s5k5ccaf_ctrl->settings.contrast = contrast;
	
	return rc;
}


int s5k5ccaf_set_saturation(int saturation)
{
	int rc = 0;
	
	switch(saturation) {
		case S5K5CCGX_SATURATION_MINUS_2:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_saturation_m_2);
			break;

		case S5K5CCGX_SATURATION_MINUS_1:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_saturation_m_1);
			break;

		case S5K5CCGX_SATURATION_DEFAULT:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_saturation_0);
			break;

		case S5K5CCGX_SATURATION_PLUS_1:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_saturation_p_1);
			break;

		case S5K5CCGX_SATURATION_PLUS_2:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_saturation_p_2);
			break;

		default :
			printk("Invalid saturation (%d) !!!\n", saturation);
			return rc;
	}

	s5k5ccaf_ctrl->settings.saturation = saturation;
	
	return rc;
}

int s5k5ccaf_set_fps(unsigned int mode, unsigned int fps)
{
	int rc = 0;
	
	CAM_DEBUG(": mode = %d, fps = %d", mode, fps);

	if(mode){
		switch(fps) {
#if defined(CONFIG_TARGET_SERIES_P8LTE)
		case S5K5CCGX_8_FPS:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_fps_8fix);
			break;
#endif			
		case S5K5CCGX_15_FPS:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_fps_15fix);
			break;
			
		case S5K5CCGX_30_FPS:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_fps_30fix);
			break;
			
		default:
			printk("Invalid fps set %d. Change default fps - 30!!!\n", fps);
			S5K5CCAF_WRITE_LIST(s5k5ccaf_fps_30fix);
			break;
		}
	} else {
		S5K5CCAF_WRITE_LIST(s5k5ccaf_fps_auto);
	}


	s5k5ccaf_ctrl->settings.fps_mode = mode;
	s5k5ccaf_ctrl->settings.fps = fps;

	return rc;
}
int s5k5ccaf_set_sharpness(int sharpness)
{
	int rc = 0;

	switch(sharpness) {
		case S5K5CCGX_SHARPNESS_MINUS_2 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_sharpness_m_2);
			break;

		case S5K5CCGX_SHARPNESS_MINUS_1 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_sharpness_m_1);
			break;

		case S5K5CCGX_SHARPNESS_DEFAULT :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_sharpness_0);
			break;

		case S5K5CCGX_SHARPNESS_PLUS_1 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_sharpness_p_1);
			break;

		case S5K5CCGX_SHARPNESS_PLUS_2 :
			S5K5CCAF_WRITE_LIST(s5k5ccaf_sharpness_p_2);
			break;

		default:
			printk("Invalid sharpness (%d) !!!\n", sharpness);
			return rc;		
	}
	
	s5k5ccaf_ctrl->settings.sharpness = sharpness;
		
	return rc;
}

int s5k5ccaf_set_awb(int lock)
{	
	if(s5k5ccaf_ctrl->app_mode == S5K5CCGX_3RD_PARTY_APP) {
		return 0; //for barcode scanner, QRcode
	}

	if(lock) {
		CAM_DEBUG("AWB_LOCK");
		S5K5CCAF_WRITE_LIST(s5k5ccaf_awb_lock);

	} else {
		CAM_DEBUG("AWB_UNLOCK");
		S5K5CCAF_WRITE_LIST(s5k5ccaf_awb_unlock);

	}

	return 0;
}

int s5k5ccaf_set_ae_awb(int lock)
{	
	if(s5k5ccaf_ctrl->app_mode == S5K5CCGX_3RD_PARTY_APP) {
		return 0; //for barcode scanner, QRcode
	}

	if(s5k5ccaf_ctrl->touchaf_enable == 1) {
		cam_info("s5k5ccaf_set_ae_awb : return , touch af");
		return 0; //for touch AF
	}

	if(lock) {
		if(s5k5ccaf_ctrl->settings.ae_awb_lock == 0) {
			cam_info("AWB_AE_LOCK");
			if(s5k5ccaf_ctrl->settings.wb == WHITE_BALANCE_AUTO) {
				S5K5CCAF_WRITE_LIST(s5k5ccaf_ae_lock);
				S5K5CCAF_WRITE_LIST(s5k5ccaf_awb_lock);
			} else {
				S5K5CCAF_WRITE_LIST(s5k5ccaf_ae_lock);
			}
			s5k5ccaf_ctrl->settings.ae_awb_lock = 1;
		}
	} else {
		if(s5k5ccaf_ctrl->settings.ae_awb_lock == 1) {
			cam_info("AWB_AE_UNLOCK");
			if(s5k5ccaf_ctrl->settings.wb == WHITE_BALANCE_AUTO) {
				S5K5CCAF_WRITE_LIST(s5k5ccaf_ae_unlock);
				S5K5CCAF_WRITE_LIST(s5k5ccaf_awb_unlock);
			} else {
				S5K5CCAF_WRITE_LIST(s5k5ccaf_ae_unlock);
			}
			s5k5ccaf_ctrl->settings.ae_awb_lock = 0;
		}
	}

	return 0;
}

int s5k5ccaf_set_af_mode(int mode)
{
	int rc = 0;

#if defined(CONFIG_TARGET_SERIES_P8LTE)
	if( s5k5ccaf_ctrl->cam_mode == S5K5CCGX_CAMERA_MODE &&
		s5k5ccaf_ctrl->op_mode != S5K5CCGX_MODE_PREVIEW ){ 
		CAM_DEBUG("ignore af [%d] : s5k5ccaf_ctrl->op_mode = 0x%X",mode, s5k5ccaf_ctrl->op_mode);
		s5k5ccaf_ctrl->settings.focus_mode = mode;
		return rc;
	}
#endif
	switch(mode) {
		case S5K5CCGX_AF_MODE_AUTO:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_af_normal_on); 
			break;

		case S5K5CCGX_AF_MODE_MACRO:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_af_macro_on); 
			break;

		default:
			S5K5CCAF_WRITE_LIST(s5k5ccaf_af_normal_on); 
			return rc;
	}

	s5k5ccaf_ctrl->settings.focus_mode = mode;
	
	return rc;
}

static u16 s5k5ccaf_get_ae_stable(void)
{
	unsigned short read_value = 0;

	S5K5CCAF_WRITE_LIST(s5k5ccaf_get_ae_stable_reg);	

	if(s5k5ccaf_sensor_read(0x0F12, &read_value) < 0)   {
		printk("sensor read fail...!\n");
		goto out;
	}	

	CAM_DEBUG("read_value (ae stable) = 0x%X", read_value);
	
	return read_value;

out:
	/* restore write mode */
	s5k5ccaf_sensor_write(0x0028, 0x7000);
	return read_value;
}

int s5k5ccaf_set_af_status(int status, int initial_pos)
{
	int rc = 0;
	unsigned int cur_lux = 0;
	unsigned short ae_stable_count = 10;
		
	// auto focus
	if(status) {	// AF start
		CAM_DEBUG("S5K5CCGX_AF_START : flash mode = %d",s5k5ccaf_ctrl->settings.flash_mode);

		cur_lux = s5k5ccaf_get_light_level();
		CAM_DEBUG("AF light level = %d",cur_lux);
		if(cur_lux <= LOW_LIGHT_LEVEL) {
			cam_info("LOW LUX AF ");
			af_low_lux = 1;
		} else {
			cam_info("NORMAL LUX AF ");
			af_low_lux = 0;
		}
		
		/* !720p camcording && low light level && AUTO/ON MODE */
		if((!s5k5ccaf_ctrl->hd_enabled) && ((af_low_lux && s5k5ccaf_ctrl->settings.flash_mode == S5K5CCGX_FLASH_AUTO) || (s5k5ccaf_ctrl->settings.flash_mode == S5K5CCGX_FLASH_ON))) {
			S5K5CCAF_WRITE_LIST(s5k5ccaf_preflash_start);
			S5K5CCAF_WRITE_LIST(s5k5ccaf_flash_ae_set);
	
			s5k5ccaf_set_flash(MOVIE_FLASH,1);
			s5k5ccaf_ctrl->settings.flash_state = 1;

			/* check AE stable */
			while(ae_stable_count-- > 0) {
				if(s5k5ccaf_get_ae_stable() == 0x01) {
					break;
				}
				msleep(50);
			}
		}

		// AE/AWB lock
		s5k5ccaf_set_ae_awb(1);	

		if(s5k5ccaf_ctrl->hd_enabled) {
			//S5K5CCAF_WRITE_LIST(s5k5ccaf_1st_720P_af_do);
			// set AF operation value for 720P
			s5k5ccaf_sensor_write(0x0028, 0x7000);
			s5k5ccaf_sensor_write(0x002A, 0x0226);
			s5k5ccaf_sensor_write(0x0F12, 0x0010); 

			// 720P 1frame delay
			CAM_DELAY(50);

			// set AF start cmd value for 720P
			s5k5ccaf_sensor_write(0x002A, 0x0224);
			s5k5ccaf_sensor_write(0x0F12, 0x0006);
			printk("\n\n%s : 720P Auto Focus Operation \n\n", __func__);
		} else {
			S5K5CCAF_WRITE_LIST(s5k5ccaf_af_do);		
			if(af_low_lux) {
				CAM_DEBUG("200ms delay for Low Lux AF");
				CAM_DELAY(200);	//200ms delay after AF Start(from 5CC guide)
			}
		}
	} else {	// AF stop(abort)
		CAM_DEBUG("S5K5CCGX_AF_ABORT\n");

		// moving base position 
		// 0 : only AF stop
		// 1 : only lenz move base position
		// 2 : AF stop and lenz move base posizion
		if(initial_pos == 2) {		
			S5K5CCAF_WRITE_LIST(s5k5ccaf_af_abort);
			s5k5ccaf_set_af_mode(s5k5ccaf_ctrl->settings.focus_mode);			
		} else if (initial_pos == 1) {
			s5k5ccaf_set_af_mode(s5k5ccaf_ctrl->settings.focus_mode);			
		} else {
			S5K5CCAF_WRITE_LIST(s5k5ccaf_af_abort);
		}

		// AE/AWB unlock
		s5k5ccaf_set_ae_awb(0); 
		
		if((!s5k5ccaf_ctrl->hd_enabled) && (s5k5ccaf_ctrl->settings.flash_state == 1)) {
			S5K5CCAF_WRITE_LIST(s5k5ccaf_preflash_end);
			s5k5ccaf_set_flash(MOVIE_FLASH,0);
			s5k5ccaf_ctrl->settings.flash_state = 0;
		}
	
		af_low_lux = 0;
		
		s5k5ccaf_ctrl->touchaf_enable = false;
	}	

	s5k5ccaf_ctrl->settings.focus_status = status;

	return rc;
}

//modify to TouchAF by Teddy
int s5k5ccaf_reset_AF_region(void)
{
	u16 mapped_x = 512;
	u16 mapped_y = 384;
	u16 inner_window_start_x = 0;
	u16 inner_window_start_y = 0;
	u16 outer_window_start_x = 0;
	u16 outer_window_start_y = 0;

	s5k5ccaf_ctrl->touchaf_enable = false;
	// mapping the touch position on the sensor display
	mapped_x = (mapped_x * 1024) / 1066;
	mapped_y = (mapped_y * 768) / 800;
	//CAM_DEBUG("mapped xPos = %d, mapped yPos = %d", mapped_x, mapped_y);

	inner_window_start_x    = mapped_x - (INNER_WINDOW_WIDTH_1024_768 / 2);
	outer_window_start_x    = mapped_x - (OUTER_WINDOW_WIDTH_1024_768 / 2);
	//CAM_DEBUG("boxes are in the sensor window. in_Sx = %d, out_Sx= %d", inner_window_start_x, outer_window_start_x);

	inner_window_start_y    = mapped_y - (INNER_WINDOW_HEIGHT_1024_768 / 2);
	outer_window_start_y    = mapped_y - (OUTER_WINDOW_HEIGHT_1024_768 / 2);
	//CAM_DEBUG("boxes are in the sensor window. in_Sy = %d, out_Sy= %d", inner_window_start_y, outer_window_start_y);


	//calculate the start position value
	inner_window_start_x = inner_window_start_x * 1024 /1024;
	outer_window_start_x = outer_window_start_x * 1024 / 1024;
	inner_window_start_y = inner_window_start_y * 1024 / 768;
	outer_window_start_y = outer_window_start_y * 1024 / 768;
	//CAM_DEBUG("calculated value inner_window_start_x = %d", inner_window_start_x);
	//CAM_DEBUG("calculated value inner_window_start_y = %d", inner_window_start_y);
	//CAM_DEBUG("calculated value outer_window_start_x = %d", outer_window_start_x);
	//CAM_DEBUG("calculated value outer_window_start_y = %d", outer_window_start_y);


	//Write register
	s5k5ccaf_sensor_write(0x0028, 0x7000);

	// inner_window_start_x
	s5k5ccaf_sensor_write(0x002A, 0x0234);
	s5k5ccaf_sensor_write(0x0F12, inner_window_start_x);

	// outer_window_start_x
	s5k5ccaf_sensor_write(0x002A, 0x022C);
	s5k5ccaf_sensor_write(0x0F12, outer_window_start_x);

	// inner_window_start_y
	s5k5ccaf_sensor_write(0x002A, 0x0236);
	s5k5ccaf_sensor_write(0x0F12, inner_window_start_y);

	// outer_window_start_y
	s5k5ccaf_sensor_write(0x002A, 0x022E);
	s5k5ccaf_sensor_write(0x0F12, outer_window_start_y);

	// Update AF window
	s5k5ccaf_sensor_write(0x002A, 0x023C);
	s5k5ccaf_sensor_write(0x0F12, 0x0001);

    return 0;
}


int s5k5ccaf_set_touchaf_pos(int x, int y)
{
	u16 mapped_x = 0;
	u16 mapped_y = 0;
	u16 inner_window_start_x = 0;
	u16 inner_window_start_y = 0;
	u16 outer_window_start_x = 0;
	u16 outer_window_start_y = 0;

	u16 sensor_width    =0;
	u16 sensor_height   =0;
	u16 inner_window_width = 0;
	u16 inner_window_height= 0;
	u16 outer_window_width= 0;
	u16 outer_window_height= 0;

	u16 touch_width= 0;
	u16 touch_height= 0;

	if (s5k5ccaf_ctrl->hd_enabled) {
		sensor_width        = 1280;
		sensor_height       = 720;
		inner_window_width  = INNER_WINDOW_WIDTH_720P;
		inner_window_height = INNER_WINDOW_HEIGHT_720P;
		outer_window_width  = OUTER_WINDOW_WIDTH_720P;
		outer_window_height = OUTER_WINDOW_HEIGHT_720P;
		touch_width         = 1280;
		touch_height        = 720;
	} else  {
		sensor_width        = 1024;
		sensor_height       = 768;
		inner_window_width  = INNER_WINDOW_WIDTH_1024_768;
		inner_window_height = INNER_WINDOW_HEIGHT_1024_768;
		outer_window_width  = OUTER_WINDOW_WIDTH_1024_768;
		outer_window_height = OUTER_WINDOW_HEIGHT_1024_768;
		touch_width         = 1000;
		touch_height        = 750;          
	}

	s5k5ccaf_ctrl->touchaf_enable = true;
	CAM_DEBUG("xPos = %d, yPos = %d", x, y);


	// mapping the touch position on the sensor display
	mapped_x = (x * sensor_width) / touch_width;
	mapped_y = (y * sensor_height) / touch_height;
	CAM_DEBUG("mapped xPos = %d, mapped yPos = %d", mapped_x, mapped_y);

	// set X axis
	if ( mapped_x  <=  (inner_window_width / 2) ) {
		inner_window_start_x    = 0;
		outer_window_start_x    = 0;
		//CAM_DEBUG("inbox over the left side. boxes are left side align in_Sx = %d, out_Sx= %d", inner_window_start_x, outer_window_start_x);
	} else if ( mapped_x  <=  (outer_window_width / 2) ) {
		inner_window_start_x    = mapped_x - (inner_window_width / 2);
		outer_window_start_x    = 0;
		//CAM_DEBUG("outbox only over the left side. outbox is only left side align in_Sx = %d, out_Sx= %d", inner_window_start_x, outer_window_start_x);
	} else if ( mapped_x  >=  ( (sensor_width - 1) - (inner_window_width / 2) ) ) {
		inner_window_start_x    = (sensor_width - 1) - inner_window_width;
		outer_window_start_x    = (sensor_width - 1) - outer_window_width;
		//CAM_DEBUG("inbox over the right side. boxes are rightside align in_Sx = %d, out_Sx= %d", inner_window_start_x, outer_window_start_x);
	} else if ( mapped_x  >=  ( (sensor_width - 1) - (outer_window_width / 2) ) ) {
		inner_window_start_x    = mapped_x - (inner_window_width / 2);
		outer_window_start_x    = (sensor_width - 1) - outer_window_width;
		//CAM_DEBUG("outbox only over the right side. out box is only right side align in_Sx = %d, out_Sx= %d", inner_window_start_x, outer_window_start_x);
	} else {
		inner_window_start_x    = mapped_x - (inner_window_width / 2);
		outer_window_start_x    = mapped_x - (outer_window_width / 2);
		//CAM_DEBUG("boxes are in the sensor window. in_Sx = %d, out_Sx= %d\n\n", inner_window_start_x, outer_window_start_x);
	}


	// set Y axis
	if ( mapped_y  <=  (inner_window_height / 2) ) {
		inner_window_start_y    = 0;
		outer_window_start_y    = 0;
		//CAM_DEBUG("inbox over the top side. boxes are top side align in_Sy = %d, out_Sy= %d", inner_window_start_y, outer_window_start_y);
	} else if ( mapped_y  <=  (outer_window_height / 2) ) {
		inner_window_start_y    = mapped_y - (inner_window_height / 2);
		outer_window_start_y    = 0;
		//CAM_DEBUG("outbox only over the top side. outbox is only top side align in_Sy = %d, out_Sy= %d", inner_window_start_y, outer_window_start_y);
	} else if ( mapped_y  >=  ( (sensor_height - 1) - (inner_window_height / 2) ) ) {
		inner_window_start_y    = (sensor_height - 1) - inner_window_height;
		outer_window_start_y    = (sensor_height - 1) - outer_window_height;
		//CAM_DEBUG("inbox over the bottom side. boxes are bottom side align in_Sy = %d, out_Sy= %d", inner_window_start_y, outer_window_start_y);
	} else if ( mapped_y  >=  ( (sensor_height - 1) - (outer_window_height / 2) ) ) {
		inner_window_start_y    = mapped_y - (inner_window_height / 2);
		outer_window_start_y    = (sensor_height - 1) - outer_window_height;
		//CAM_DEBUG("outbox only over the bottom side. out box is only bottom side align in_Sy = %d, out_Sy= %d", inner_window_start_y, outer_window_start_y);
	} else {
		inner_window_start_y    = mapped_y - (inner_window_height / 2);
		outer_window_start_y    = mapped_y - (outer_window_height / 2);
		//CAM_DEBUG("boxes are in the sensor window. in_Sy = %d, out_Sy= %d\n\n", inner_window_start_y, outer_window_start_y);
	}

	//calculate the start position value
	inner_window_start_x = inner_window_start_x * 1024 /sensor_width;
	outer_window_start_x = outer_window_start_x * 1024 / sensor_width;
	inner_window_start_y = inner_window_start_y * 1024 / sensor_height;
	outer_window_start_y = outer_window_start_y * 1024 / sensor_height;
	CAM_DEBUG("calculated value inner_window_start_x = %d", inner_window_start_x);
	CAM_DEBUG("calculated value inner_window_start_y = %d", inner_window_start_y);
	CAM_DEBUG("calculated value outer_window_start_x = %d", outer_window_start_x);
	CAM_DEBUG("calculated value outer_window_start_y = %d", outer_window_start_y);


	//Write register
	s5k5ccaf_sensor_write(0x0028, 0x7000);

	// inner_window_start_x
	s5k5ccaf_sensor_write(0x002A, 0x0234);
	s5k5ccaf_sensor_write(0x0F12, inner_window_start_x);

	// outer_window_start_x
	s5k5ccaf_sensor_write(0x002A, 0x022C);
	s5k5ccaf_sensor_write(0x0F12, outer_window_start_x);

	// inner_window_start_y
	s5k5ccaf_sensor_write(0x002A, 0x0236);
	s5k5ccaf_sensor_write(0x0F12, inner_window_start_y);

	// outer_window_start_y
	s5k5ccaf_sensor_write(0x002A, 0x022E);
	s5k5ccaf_sensor_write(0x0F12, outer_window_start_y);

	// Update AF window
	s5k5ccaf_sensor_write(0x002A, 0x023C);
	s5k5ccaf_sensor_write(0x0F12, 0x0001);

	CAM_DEBUG("update AF window and sleep 100ms");
	CAM_DELAY(100);

	return 0;
}

int s5k5ccaf_get_af_status(int is_search_status)
{
	unsigned short af_status =0;

	switch(is_search_status) {
		case 0:
			s5k5ccaf_sensor_write(0x002C, 0x7000);
			s5k5ccaf_sensor_write(0x002E, 0x2D12);
			s5k5ccaf_sensor_read(0x0F12, &af_status);
			CAM_DEBUG("1st AF status : %x", af_status);			
			break;
			
		case 1:
			if(s5k5ccaf_ctrl->hd_enabled)
				return 0;	// do not excute 2nd Search in HD mode
			s5k5ccaf_sensor_write(0x002C, 0x7000);
			s5k5ccaf_sensor_write(0x002E, 0x1F2F);
			s5k5ccaf_sensor_read(0x0F12, &af_status);
			CAM_DEBUG("2nd AF status : %x", af_status);
			if((!s5k5ccaf_ctrl->hd_enabled) && (af_status == 0) && (s5k5ccaf_ctrl->settings.flash_state == 1)) {
				S5K5CCAF_WRITE_LIST(s5k5ccaf_preflash_end);
				s5k5ccaf_set_flash(MOVIE_FLASH,0);
				s5k5ccaf_ctrl->settings.flash_state = 0;
			}			
			break;
		default:
			CAM_DEBUG("unexpected mode is comming from hal\n");
			break;
	}

	return  af_status;
}

int s5k5ccaf_get_exif_iso(void)
{
	u16 iso_gain_table[] = {10, 15, 25, 35};
	u16 iso_table[] = {0, 50, 100, 200, 400};
	u16 gain = 0, val = 0;
	s32 index = 0;		

	s5k5ccaf_sensor_write(0xFCFC, 0xD000);
	s5k5ccaf_sensor_write(0x002C, 0x7000);
	s5k5ccaf_sensor_write(0x002E, 0x2A18);
	s5k5ccaf_sensor_read(0x0F12, &val); //0x2A8

	gain = val * 10 / 256;
	for (index = 0; index < 4; index++) {
		if (gain < iso_gain_table[index])
			break;
	}
	
	CAM_DEBUG("iso = %d, val = %d, index = %d\n", iso_table[index], val, index); 
	
	return iso_table[index];	// ISO

}

int s5k5ccaf_get_exif_flash(void)
{
	int cur_lux;
	int temp = 0x0010;	//Flash did not fire, compulsory flash mode
		
	cur_lux = s5k5ccaf_get_light_level();

	// flash settings
	switch(s5k5ccaf_ctrl->settings.flash_mode) {
		case S5K5CCGX_FLASH_AUTO:		
			if(cur_lux > LOW_LIGHT_LEVEL)
				temp = 0x0018;	//(auto) Flash did not fire			
			else 
				temp = 0x0019;	//(auto) Flash fired
			break;
		case S5K5CCGX_FLASH_ON:
			temp = 0x0009;		//Flash fired, compulsory flash mode
			break;		
	}

	CAM_DEBUG("flash_mode = %d, temp = %x\n", s5k5ccaf_ctrl->settings.flash_mode, temp); 

	return temp;

}

static void s5k5ccaf_get_exif_exposure(void)
{
	unsigned short read_value_lsb, read_value_msb;

	s5k5ccaf_sensor_write(0xFCFC, 0xD000);
	s5k5ccaf_sensor_write(0x002C, 0x7000);
	s5k5ccaf_sensor_write(0x002E, 0x2A14);
	s5k5ccaf_sensor_read(0x0F12, &read_value_lsb); 	
	s5k5ccaf_sensor_read(0x0F12, &read_value_msb); 		

	s5k5ccaf_ctrl->settings.exif_shutterspeed = 400000/(read_value_lsb+(read_value_msb<<16));

	CAM_DEBUG("read_value_lsb = %x, read_value_msb = %x\n", read_value_lsb, read_value_msb); 
}


/* temp -kidggang
#define S5K5CCGX_PREVIEW_VGA            0       // 640x480
#define S5K5CCGX_PREVIEW_D1             1       // 720x480
#define S5K5CCGX_PREVIEW_SVGA           2       // 800x600
#define S5K5CCGX_PREVIEW_XGA            3       // 1024x768
#define S5K5CCGX_PREVIEW_PVGA           4       // 1280*720
#define S5K5CCGX_PREVIEW_528x432        5       // 528*432 
*/
int s5k5ccaf_set_preview_index(int width, int height) {
	int rc = 0;

	if (width == 528 && height == 432) {
		printk("[kidggang][%s:%d] S5K5CCGX_PREVIEW_528x432 !!!\n", __func__, __LINE__);
		s5k5ccaf_ctrl->settings.preview_size_idx = S5K5CCGX_PREVIEW_528x432; 
	} else if (width == 720 && height == 480) {
		printk("[kidggang][%s:%d] S5K5CCGX_PREVIEW_D1 !!!\n", __func__, __LINE__);
		s5k5ccaf_ctrl->settings.preview_size_idx = S5K5CCGX_PREVIEW_D1; 
	} else if (width == 640 && height == 480) {
		printk("[kidggang][%s:%d] S5K5CCGX_PREVIEW_VGA !!!\n", __func__, __LINE__);
		s5k5ccaf_ctrl->settings.preview_size_idx = S5K5CCGX_PREVIEW_VGA; 
	} else if (width == 1024 && height == 768) {
		printk("[kidggang][%s:%d] S5K5CCGX_PREVIEW_XGA !!!\n", __func__, __LINE__);
		s5k5ccaf_ctrl->settings.preview_size_idx = S5K5CCGX_PREVIEW_XGA; 
	} else if (width == 1280 && height == 720) {
		printk("[kidggang][%s:%d] S5K5CCGX_PREVIEW_PVGA !!!\n", __func__, __LINE__);
		s5k5ccaf_ctrl->settings.preview_size_idx = S5K5CCGX_PREVIEW_PVGA; 
	} else {
		printk("Invalid preview size (%dx%d) !!!\n", width, height);
		rc = -1;
	}
	return rc;
}

int s5k5ccaf_set_preview_size(int size_index)
{
	int rc = 0;
	
	switch(size_index) {
		case S5K5CCGX_PREVIEW_528x432:	// 528x432
			CAM_DEBUG("528x432");
			S5K5CCAF_WRITE_LIST(s5k5ccaf_528_432_Preview);
			break;
			
		case S5K5CCGX_PREVIEW_VGA:	// 640x480
			CAM_DEBUG("640x480");
			S5K5CCAF_WRITE_LIST(s5k5ccaf_640_480_Preview);
			break;
			
		case S5K5CCGX_PREVIEW_D1:	// 720x480
			CAM_DEBUG("720x480");
			S5K5CCAF_WRITE_LIST(s5k5ccaf_720_480_Preview);
			break;
			
		case S5K5CCGX_PREVIEW_XGA:	// 1024x768
			CAM_DEBUG("1024x768");
			S5K5CCAF_WRITE_LIST(s5k5ccaf_1024_768_Preview);
			break;

		case S5K5CCGX_PREVIEW_PVGA:	// 1280x720
			CAM_DEBUG("1024x768");
			S5K5CCAF_WRITE_LIST(s5k5ccaf_1280_720_Preview);
			break;

		default:
			printk("Invalid preview size (%d) !!!\n", size_index);
			return rc;
	}

	S5K5CCAF_WRITE_LIST(s5k5ccaf_update_preview_setting);

	return rc;
}

int s5k5ccaf_mipi_mode(int mode)
{
	int rc = 0;
	struct msm_camera_csi_params s5k5ccaf_csi_params;
	
	CAM_DEBUG("E");

	if (!config_csi) {
		s5k5ccaf_csi_params.lane_cnt = 1;
		s5k5ccaf_csi_params.data_format = CSI_8BIT;
		s5k5ccaf_csi_params.lane_assign = 0xe4;
		s5k5ccaf_csi_params.dpcm_scheme = 0;
		s5k5ccaf_csi_params.settle_cnt = 24;// 0x14; //0x7; //0x14;
		rc = msm_camio_csi_config(&s5k5ccaf_csi_params);
		if (rc < 0)
			cam_err("config csi controller failed");
		config_csi = 1;
	}
	
	CAM_DELAY(100);
	CAM_DEBUG("X");
	
	return rc;
}

static int s5k5ccaf_start(void)
{
	int rc = 0;
	unsigned short	id = 0;
	
	CAM_DEBUG("%s E", __FUNCTION__);
	
	if(s5k5ccaf_ctrl->started) {
		CAM_DEBUG("%s X : already started", __FUNCTION__);
		return rc;
	}

	s5k5ccaf_mipi_mode(1);

	/* read firmware id */
	s5k5ccaf_sensor_write(0x002C, 0x0000);
	s5k5ccaf_sensor_write(0x002E, 0x0040);
	s5k5ccaf_sensor_read(0x0F12, &id);
	
	if(id != 0x05CC) {
		printk("[S5K5CCAF] WRONG SENSOR FW => id 0x%x \n", id);
		printk("[S5K5CCAF] PINON/OFF : %d\n", gpio_get_value(0));
		rc = -1;
	} else {
		printk("[S5K5CCAF] CURRENT SENSOR FW => id 0x%x \n", id);
	}

	s5k5ccaf_ctrl->started = 1;

	return 0;
}

int wait_sensor_mode(int mode, int interval, int cnt)
{
	int rc = 0;
	unsigned short sensor_mode;
	
	cam_info("wait_sensor_mode E %d", sensor_mode);
	do {
		s5k5ccaf_sensor_write(0x002C, 0x7000);
		s5k5ccaf_sensor_write(0x002E, 0x1E86);
		s5k5ccaf_sensor_read(0x0F12, &sensor_mode);	
		msleep(interval);
	} while((--cnt) > 0 && !(sensor_mode == mode));

	cam_info("wait_sensor_mode X %d (wait = %d)", sensor_mode, cnt);
	
	if(cnt == 0) {
		cam_err("!! MODE CHANGE ERROR to %d !!\n", mode);
		rc = -1;
	}
	
	return rc;
}

void s5k5ccaf_set_preview(void)
{
	cam_info("s5k5ccaf_ctrl->settings.preview_size_idx = %d", s5k5ccaf_ctrl->settings.preview_size_idx);

	if(s5k5ccaf_ctrl->dtp_mode == 1) {
		cam_info("start DTP mode ");
		s5k5ccaf_ctrl->op_mode = S5K5CCGX_MODE_DTP;
		S5K5CCAF_WRITE_LIST(s5k5ccaf_DTP_init0);
		s5k5ccaf_ctrl->hd_enabled = 0;
	} else {
		if(s5k5ccaf_ctrl->op_mode == S5K5CCGX_MODE_CAPTURE) {	// return to preview mode after capture
			cam_info("return to preview mode");
#ifndef USE_VIDEO_SIZE
			S5K5CCAF_WRITE_LIST(s5k5ccaf_init0);
#endif				
			s5k5ccaf_ctrl->op_mode = S5K5CCGX_MODE_PREVIEW;
			s5k5ccaf_set_preview_size(s5k5ccaf_ctrl->settings.preview_size_idx);
			wait_sensor_mode(S5K5CCGX_PREVIEW_MODE, 10, 50);	//20 -> 50 : recommended by SEM
			s5k5ccaf_ctrl->hd_enabled = 0;
		} else {
			if(s5k5ccaf_ctrl->settings.preview_size_idx == S5K5CCGX_PREVIEW_PVGA) { // 720p 	
				cam_info("change to 720P preview");
				s5k5ccaf_ctrl->op_mode = S5K5CCGX_MODE_PREVIEW;
				S5K5CCAF_WRITE_LIST(s5k5ccaf_1280_720_Preview); 
				wait_sensor_mode(S5K5CCGX_PREVIEW_MODE, 10, 20);
				s5k5ccaf_ctrl->hd_enabled = 1;
			} else {		
				cam_info("change preview size");
#ifndef CONFIG_LOAD_FILE				
				S5K5CCAF_WRITE_LIST(s5k5ccaf_init0);
#endif		
				s5k5ccaf_ctrl->op_mode = S5K5CCGX_MODE_PREVIEW;
				if(s5k5ccaf_ctrl->settings.preview_size_idx != S5K5CCGX_PREVIEW_XGA) {
					s5k5ccaf_set_preview_size(s5k5ccaf_ctrl->settings.preview_size_idx);	// change size and update setting		
				}
				wait_sensor_mode(S5K5CCGX_PREVIEW_MODE, 10, 20);
				s5k5ccaf_ctrl->hd_enabled = 0;
			}			

			cam_info("s5k5ccaf_ctrl->cam_mode = %d", s5k5ccaf_ctrl->cam_mode);
			if(s5k5ccaf_ctrl->cam_mode == S5K5CCGX_MOVIE_MODE || s5k5ccaf_ctrl->cam_mode == S5K5CCGX_MMS_MODE) {
				s5k5ccaf_set_whitebalance(s5k5ccaf_ctrl->settings.wb);
				s5k5ccaf_set_effect(s5k5ccaf_ctrl->settings.effect);
				s5k5ccaf_set_brightness(s5k5ccaf_ctrl->settings.brightness);
				if(s5k5ccaf_ctrl->hd_enabled == 0) {
					s5k5ccaf_set_fps(s5k5ccaf_ctrl->cam_mode, s5k5ccaf_ctrl->settings.fps); //fixed fps
				}
#if defined(CONFIG_TARGET_SERIES_P8LTE)
				else { // start first af
					s5k5ccaf_ctrl->first_af_running = 1;
					S5K5CCAF_WRITE_LIST(s5k5ccaf_1st_720P_af_do); 
				}
#endif				
				CAM_DELAY(200);
			} else {
				if(s5k5ccaf_ctrl->settings.scene == SCENE_MODE_NONE) {					
					s5k5ccaf_set_whitebalance(s5k5ccaf_ctrl->settings.wb);
					s5k5ccaf_set_effect(s5k5ccaf_ctrl->settings.effect);
					s5k5ccaf_set_brightness(s5k5ccaf_ctrl->settings.brightness);
					s5k5ccaf_set_af_mode(s5k5ccaf_ctrl->settings.focus_mode);
					if(s5k5ccaf_ctrl->vtcall_mode) { //for Qik, HDVT
						s5k5ccaf_set_fps(s5k5ccaf_ctrl->vtcall_mode, s5k5ccaf_ctrl->settings.fps); //fixed fps
					} else {
						s5k5ccaf_set_fps(s5k5ccaf_ctrl->settings.fps_mode, s5k5ccaf_ctrl->settings.fps);		
					}
					CAM_DELAY(200);
				} else {
					s5k5ccaf_set_af_mode(s5k5ccaf_ctrl->settings.focus_mode);
					s5k5ccaf_set_scene(s5k5ccaf_ctrl->settings.scene);	
					// wait for stable AE/AWB
					if(s5k5ccaf_ctrl->settings.scene == SCENE_MODE_NIGHTSHOT) {
						CAM_DELAY(500);
						cam_info("additional delay 500ms (NIGHTSHOT)"); 
					} else if(s5k5ccaf_ctrl->settings.scene == SCENE_MODE_FIREWORKS)  {
						CAM_DELAY(800);
						cam_info("additional delay 800ms (FIREWORK)");	
					} else {
						CAM_DELAY(200);
					}							
				}
			}
		}
	}
}

void s5k5ccaf_set_capture(void)
{
	int cur_lux;
	
	CAM_DEBUG("");
	
	s5k5ccaf_ctrl->op_mode = S5K5CCGX_MODE_CAPTURE;
	
	cur_lux = s5k5ccaf_get_light_level();
	cam_info("CAPTURE light level = %d", cur_lux);

	// flash settings
	switch(s5k5ccaf_ctrl->settings.flash_mode) {
	case S5K5CCGX_FLASH_AUTO:		
		if(cur_lux > LOW_LIGHT_LEVEL)
			break;
	case S5K5CCGX_FLASH_ON:
		cam_info("CAPTURE FLASH ON");
		s5k5ccaf_set_flash(CAPTURE_FLASH,1);
		mdelay(10);
		S5K5CCAF_WRITE_LIST(s5k5ccaf_mainflash_start);
		s5k5ccaf_ctrl->settings.flash_state = 1;
		break;		
	}

	// capture sequence
	if(cur_lux > 0xFFFE) {
		cam_info("HighLight Snapshot!");
		S5K5CCAF_WRITE_LIST(s5k5ccaf_highlight_snapshot);
		if(af_low_lux) {
			cam_info("additional delay for Low Lux AF");
			//remove duplicated delay	msleep(200);
		}	
	} else if(cur_lux < LOW_LIGHT_LEVEL) {
		if((s5k5ccaf_ctrl->settings.scene == SCENE_MODE_NIGHTSHOT) ||(s5k5ccaf_ctrl->settings.scene == SCENE_MODE_FIREWORKS)) {
			cam_info("Night or Firework  Snapshot!");
			S5K5CCAF_WRITE_LIST(s5k5ccaf_night_snapshot);
			//remove duplicated delay	msleep(300);	
		} else {
			cam_info("LowLight Snapshot delay!");
			S5K5CCAF_WRITE_LIST(s5k5ccaf_lowlight_snapshot);
			//remove duplicated delay	msleep(250);	
		}
	} else {
		cam_info("Normal Snapshot !");
		S5K5CCAF_WRITE_LIST(s5k5ccaf_snapshot);
		if(af_low_lux) {
			cam_info("additional delay for Low Lux AF");
			//remove duplicated delay	msleep(200);
		}	
	}	

	wait_sensor_mode(S5K5CCGX_CAPTURE_MODE, 10, 40);	//remove duplicated delay : 20 -> 40

	s5k5ccaf_get_exif_exposure();

	if(s5k5ccaf_ctrl->settings.flash_state == 1) {
		//CAM_DEBUG("CAPTURE FLASH OFF - after 700ms delay");			
		S5K5CCAF_WRITE_LIST(s5k5ccaf_mainflash_end);				
		S5K5CCAF_WRITE_LIST(s5k5ccaf_flash_ae_clear);
		s5k5ccaf_ctrl->settings.flash_state = 0;
	}

	af_low_lux = 0;
}

static long s5k5ccaf_set_sensor_mode(int mode)
{
	CAM_DEBUG("s5k5ccaf_set_sensor_mode : %d", mode);
	
	switch (mode) {
		case SENSOR_PREVIEW_MODE:
			s5k5ccaf_start();
			s5k5ccaf_set_preview();
			break;
			
		case SENSOR_SNAPSHOT_MODE:
		case SENSOR_RAW_SNAPSHOT_MODE:
			s5k5ccaf_set_capture();
			s5k5ccaf_reset_AF_region();
			break;
			
		default:
			return -EINVAL;
	}

	s5k5ccaf_ctrl->sensor_mode = mode;
		
	return 0;
}

int s5k5ccaf_set_dtp(int* onoff)
{
	int rc = 0;
	switch(*onoff) {
	case S5K5CCGX_DTP_OFF:
		cam_info("DTP OFF");
		/* set ACK value */
		(*onoff) = DTP_OFF_ACK;
		s5k5ccaf_ctrl->dtp_mode = 0;
		break;

	case S5K5CCGX_DTP_ON:
		cam_info("DTP ON");
		/* set ACK value */
		(*onoff) = DTP_ON_ACK;
		s5k5ccaf_ctrl->dtp_mode = 1;
		break;

	default:
		cam_info("Invalid DTP mode (%d) !!!", *onoff);
		return rc;
	}
	
	return rc;
}

int s5k5ccaf_esd_reset(void)
{
	cam_info("reset for recovery from ESD");
	
	cam_ldo_power_off();
	mdelay(5);
	cam_ldo_power_on("s5k5ccaf");
	
	gpio_set_value_cansleep(CAM_3M_STBY, HIGH);
	udelay(20);
	gpio_set_value_cansleep(CAM_3M_RST, HIGH);
	mdelay(10);

	s5k5ccaf_set_preview();

	return 0;
}

int s5k5ccaf_sensor_ext_config(void __user *arg)
{
	int rc = 0;
	sensor_ext_cfg_data cfg_data;
	unsigned short read_value1;	

	if (copy_from_user((void *)&cfg_data, (const void *)arg, sizeof(cfg_data))) {
		cam_err(" %s fail copy_from_user!", __func__);
	}

	switch(cfg_data.cmd) {
		case EXT_CFG_ESD_RESET:
			s5k5ccaf_esd_reset();
			break;
			
		case EXT_CFG_SET_SCENE:
			s5k5ccaf_set_scene(cfg_data.value_1);
			break;
			
		case EXT_CFG_SET_FLASH:
			s5k5ccaf_set_flash(cfg_data.value_1, cfg_data.value_2);
			break;
			
		case EXT_CFG_SET_FLASH_MODE:
			s5k5ccaf_ctrl->settings.flash_mode = cfg_data.value_1;
			break;
			
		case EXT_CFG_SET_EFFECT:
			rc = s5k5ccaf_set_effect(cfg_data.value_1);
			break;

		case EXT_CFG_SET_SATURATION:
			rc = s5k5ccaf_set_saturation(cfg_data.value_1);
			break;

		case EXT_CFG_SET_ISO:
			rc = s5k5ccaf_set_iso(cfg_data.value_1);
			break;

		case EXT_CFG_SET_WB:
			rc = s5k5ccaf_set_whitebalance(cfg_data.value_1);
			break;

		case EXT_CFG_SET_CONTRAST:
			rc = s5k5ccaf_set_contrast(cfg_data.value_1);
			break;

		case EXT_CFG_SET_BRIGHTNESS:
			rc = s5k5ccaf_set_brightness(cfg_data.value_1);
			break;

		case EXT_CFG_SET_FPS:
			rc = s5k5ccaf_set_fps(cfg_data.value_1,cfg_data.value_2);
			break;
			
		case EXT_CFG_SET_METERING:	// auto exposure mode
			rc = s5k5ccaf_set_metering(cfg_data.value_1);
			break;

		case EXT_CFG_SET_PREVIEW_SIZE:	
			cam_info("set preview size %dx%d", cfg_data.value_1, cfg_data.value_2);
			s5k5ccaf_ctrl->settings.preview_size_width = cfg_data.value_1;	// just set width
			s5k5ccaf_ctrl->settings.preview_size_height = cfg_data.value_2;	// just set height
			rc = s5k5ccaf_set_preview_index(s5k5ccaf_ctrl->settings.preview_size_width, s5k5ccaf_ctrl->settings.preview_size_height);
			break;

		case EXT_CFG_SET_AE_AWB:
			rc = s5k5ccaf_set_ae_awb(cfg_data.value_1);
			break;

		case EXT_CFG_SET_AWB:
			rc = s5k5ccaf_set_awb(cfg_data.value_1);
			break;

		case EXT_CFG_SET_AF_MODE:
			rc = s5k5ccaf_set_af_mode(cfg_data.value_1);
			break;

		case EXT_CFG_SET_AF_STATUS:
			rc = s5k5ccaf_set_af_status(cfg_data.value_1, cfg_data.value_2);
			break;

		case EXT_CFG_GET_AF_STATUS:
			rc = s5k5ccaf_get_af_status(cfg_data.value_1);
			cfg_data.value_1 = rc;
			break;
			
		case EXT_CFG_SET_TOUCHAF_POS:	
#if defined(CONFIG_TARGET_SERIES_P8LTE)
			if( s5k5ccaf_ctrl->first_af_running ) {
				int first_af_status;
				int wait_count=0;
				first_af_status = s5k5ccaf_get_af_status(0);
				msleep(50);
				while((first_af_status == 1) && (wait_count < 100)) {
					first_af_status = s5k5ccaf_get_af_status(0);
					wait_count++;
					msleep(50);
				}
				s5k5ccaf_ctrl->first_af_running = 0;
			}
#endif		
			rc = s5k5ccaf_set_touchaf_pos(cfg_data.value_1,cfg_data.value_2);
			break;
			
		case EXT_CFG_SET_DTP:
			rc = s5k5ccaf_set_dtp(&cfg_data.value_1);
			break;

		case EXT_CFG_SET_VT_MODE:
			cam_info("VTCall mode : %d", cfg_data.value_1);
			s5k5ccaf_ctrl->vtcall_mode = cfg_data.value_1;
			break;

		case EXT_CFG_SET_MOVIE_MODE:
			cam_info("Movie mode : %d", cfg_data.value_1);
			s5k5ccaf_ctrl->cam_mode = cfg_data.value_1;
			break;
			
		case EXT_CFG_SET_VENDOR:
			cam_info("Vendor app mode : %d", cfg_data.value_1);
			s5k5ccaf_ctrl->app_mode = cfg_data.value_1;
			break;

		case EXT_CFG_GET_EXIF_EXPOSURE:
			cfg_data.value_1 = s5k5ccaf_ctrl->settings.exif_shutterspeed;
			break;

		case EXT_CFG_GET_EXIF_ISO:
			rc = s5k5ccaf_get_exif_iso();
			cfg_data.value_1 = rc;
			break;

		case EXT_CFG_GET_EXIF_FlASH:
			rc = s5k5ccaf_get_exif_flash();		
			cfg_data.value_1 = rc;
			break;

		case EXT_CFG_GET_STATUS:
			s5k5ccaf_sensor_write(0x002C, 0x0000);
			s5k5ccaf_sensor_write(0x002E, 0x0040);
			s5k5ccaf_sensor_read(0x0F12, &read_value1);	//CAM FOR FW
				
//kidggang-???			cfg_data.value_3 = read_value1;
				
//kidggang-???			printk("check current module status : %x\n", cfg_data.value_3);
			cam_info("PINON/OFF : %d", gpio_get_value(0));
			break;
			
		default:
			break;
	
	}	

	if(copy_to_user((void *)arg, (const void *)&cfg_data, sizeof(cfg_data))) {
		cam_err(" %s : copy_to_user Failed", __func__);
	}

	return rc;
	
}

static int s5k5ccaf_sensor_pre_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

//#ifndef CONFIG_LOAD_FILE
	rc = S5K5CCAF_WRITE_LIST(s5k5ccaf_pre_init0);
	if(rc < 0)
		printk("Error in s5k5ccaf_sensor_pre_init!");
//#endif
	mdelay(1);

	return rc;
}

static int s5k5ccaf_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
	int rc = 0;
	printk("[S5K5CCAF] sensor_init_probe()\n");

	gpio_set_value_cansleep(CAM_3M_RST, LOW);
	gpio_set_value_cansleep(CAM_VGA_RST, LOW);	
	gpio_set_value_cansleep(CAM_VGA_STBY, LOW);

	gpio_set_value_cansleep(CAM_3M_STBY, HIGH);	// modify Jeonhk 20110812
	mdelay(1);	// 5-> 1
	gpio_set_value_cansleep(CAM_3M_RST, HIGH);
	mdelay(10);	// 10-> 1

	rc = s5k5ccaf_sensor_pre_init(data);

#ifdef CONFIG_LOAD_FILE
	mdelay(100);
	S5K5CCAF_WRITE_LIST(s5k5ccaf_init0);
#endif

	return rc;
}

#ifdef CONFIG_LOAD_FILE

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <asm/uaccess.h>

static char *s5k5ccaf_regs_table = NULL;

static int s5k5ccaf_regs_table_size;

void s5k5ccaf_regs_table_init(void)
{
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int ret;
	mm_segment_t fs = get_fs();

	printk("%s %d\n", __func__, __LINE__);

	set_fs(get_ds());
#if defined(CONFIG_MACH_P5_LTE)
	filp = filp_open("/sdcard/s5k5ccgx_regs_p5.h", O_RDONLY, 0);
#else
	filp = filp_open("/sdcard/s5k5ccgx_regs.h", O_RDONLY, 0);
#endif
	if (IS_ERR(filp)) {
		printk("file open error\n");
		return;
	}
	l = filp->f_path.dentry->d_inode->i_size;	
	printk("l = %ld\n", l);
	dp = kmalloc(l, GFP_KERNEL);
	if (dp == NULL) {
		printk("Out of Memory\n");
		filp_close(filp, current->files);
	}
	pos = 0;
	memset(dp, 0, l);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	if (ret != l) {
		printk("Failed to read file ret = %d\n", ret);
		kfree(dp);
		dp = NULL;
		filp_close(filp, current->files);
		return;
	}

	filp_close(filp, current->files);
	
	set_fs(fs);

	s5k5ccaf_regs_table = dp;
	
	s5k5ccaf_regs_table_size = l;

	*((s5k5ccaf_regs_table + s5k5ccaf_regs_table_size) - 1) = '\0';

	//printk("s5k5ccaf_regs_table 0x%x, %ld\n", dp, l);
}

void s5k5ccaf_regs_table_exit(void)
{
	printk("%s %d\n", __func__, __LINE__);
	if (s5k5ccaf_regs_table) {
		kfree(s5k5ccaf_regs_table);
		s5k5ccaf_regs_table = NULL;
	}	
}

static int s5k5ccaf_regs_table_write(char *name)
{
	char *start, *end, *reg;
	unsigned short addr, value;
	char reg_buf[7], data_buf[7];

	int idx=0, err=0;
	struct i2c_msg msg = {  s5k5ccaf_client->addr, 0, 0, s5k5ccaf_buf_for_burstmode};
	
	printk("s5k5ccaf_regs_table_write start: %s\n",name);
	
	addr = value = 0;

	*(reg_buf + 4) = '\0';
	*(data_buf + 4) = '\0';

	start = strstr(s5k5ccaf_regs_table, name);
	
	end = strstr(start, "};");

	while (1) {
		/* Find Address */	
		reg = strstr(start,"0x");
		if (reg)
			start = (reg + 10);
		if ((reg == NULL) || (reg > end))
			break;
		/* Write Value to Address */	
		if (reg != NULL) {
			memcpy(reg_buf, (reg + 2), 4);
			memcpy(data_buf, (reg + 6), 4);
			addr = (unsigned short)simple_strtoul(reg_buf, NULL, 16); 
			value = (unsigned short)simple_strtoul(data_buf, NULL, 16); 
			//printk("addr 0x%x, value 0x%x\n", addr, value);

			switch(addr) {
			case 0x0F12:
				// make and fill buffer for burst mode write
				if(idx ==0) {
					s5k5ccaf_buf_for_burstmode[idx++] = 0x0F;
					s5k5ccaf_buf_for_burstmode[idx++] = 0x12;
				}
				s5k5ccaf_buf_for_burstmode[idx++] = value>> 8;
				s5k5ccaf_buf_for_burstmode[idx++] = value & 0xFF;
				break;

			case 0xFFFF:
				msleep(value);
				break;
		
			default:
				if(idx > 0) 	{
					msg.len = idx;
					err = i2c_transfer(s5k5ccaf_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
					//printk("s5k5ccaf_regs_table_write, idx = %d\n",idx);
					idx=0;
					msleep(1);
				}
				err = s5k5ccaf_sensor_write(addr, value);		// send stored data and current data
				//printk("addr 0x%x, value 0x%x\n", addr, value);
				break;
			}
		} else {
			printk("EXCEPTION! reg value : %c  addr : 0x%x,  value : 0x%x\n", *reg, addr, value);
		}
	}

	if(idx > 0) {
		msg.len = idx;
		err = i2c_transfer(s5k5ccaf_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
		//printk("s5k5ccaf_regs_table_write, idx = %d\n",idx);
		idx=0;
		msleep(1);
	}	

	printk("s5k5ccaf_regs_table_write end : %s\n",name);
	
	return 0;
}
#endif

//////////////////////////////////////////////////////////////
//#ifdef FACTORY_TEST
extern struct class *sec_class;
struct device *s5k5ccgx_dev;

static ssize_t cameratype_file_cmd_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char sensor_info[30] = "S5K5CCGX";
	return sprintf(buf, "%s\n", sensor_info);
}

static ssize_t cameratype_file_cmd_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t size)
{
	/*Reserved*/
	return size;
}

//static DEVICE_ATTR(cameratype, 0660, cameratype_file_cmd_show, cameratype_file_cmd_store);
static struct device_attribute s5k5ccaf_camtype_attr = {
	.attr = {
		.name = "camtype",
		.mode = 0660},// (S_IRUGO | S_IWUGO)},
	.show = cameratype_file_cmd_show,
	.store = cameratype_file_cmd_store
};

static ssize_t cameraflash_file_cmd_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	/*Reserved*/
	return 0;
}

static ssize_t cameraflash_file_cmd_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t size)
{
	int value;

	sscanf(buf, "%d", &value);

	if (value == 0) {
		printk(KERN_INFO "[Factory flash]OFF\n");
		//s5k5ccaf_torch(0);
		torch_light_on = 0;
		s5k5ccaf_set_flash(MOVIE_FLASH,0);
	} else {
		printk(KERN_INFO "[Factory flash]ON\n");
		//s5k5ccaf_torch(1);
		s5k5ccaf_set_flash(MOVIE_FLASH,1);
		torch_light_on = 1;
	}

	return size;
}

//static DEVICE_ATTR(cameraflash, 0660, cameraflash_file_cmd_show, cameraflash_file_cmd_store);
static struct device_attribute s5k5ccaf_cameraflash_attr = {
	.attr = {
		.name = "cameraflash",
		.mode = 0660},// (S_IRUGO | S_IWUGO)},
	.show = cameraflash_file_cmd_show,
	.store = cameraflash_file_cmd_store
};
//#endif

//////////////////////////////////////////////////////////////
int s5k5ccaf_sensor_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	CDBG("E");

	s5k5ccaf_ctrl = kzalloc(sizeof(struct s5k5ccaf_ctrl), GFP_KERNEL);
	if (!s5k5ccaf_ctrl) {
		printk("s5k5ccaf_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	if (data)
		s5k5ccaf_ctrl->sensordata = data;

	s5k5ccaf_ctrl->settings.preview_size_idx = S5K5CCGX_PREVIEW_XGA;
	s5k5ccaf_ctrl->settings.effect = CAMERA_EFFECT_OFF;
	s5k5ccaf_ctrl->settings.wb = WHITE_BALANCE_AUTO;
	s5k5ccaf_ctrl->settings.brightness = EV_DEFAULT;
	
	config_csi = 0;
#ifdef CONFIG_LOAD_FILE
	s5k5ccaf_regs_table_init();
#endif	

	rc = s5k5ccaf_sensor_init_probe(data);
	if (rc < 0) {
		printk("s5k5ccaf_sensor_init failed!\n");
		goto init_fail;
	}

init_done:
	return rc;

init_fail:
	kfree(s5k5ccaf_ctrl);
	s5k5ccaf_ctrl = NULL;
	return rc;
}

static int s5k5ccaf_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&s5k5ccaf_wait_queue);
	return 0;
}

int s5k5ccaf_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	long rc = 0;

	if (copy_from_user(&cfg_data, (void *)argp,
			sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	CDBG("s5k5ccaf_ioctl, cfgtype = %d, mode = %d\n",
		cfg_data.cfgtype, cfg_data.mode);

	switch (cfg_data.cfgtype) {
	case CFG_SET_MODE:
		rc = s5k5ccaf_set_sensor_mode(
					cfg_data.mode);
		break;

	case CFG_SET_EFFECT:
		//rc = s5k5ccaf_set_effect(cfg_data.mode, cfg_data.cfg.effect);
		break;

	case CFG_GET_AF_MAX_STEPS:
	default:
		rc = -EINVAL;
		printk("s5k5ccaf_sensor_config : Invalid cfgtype ! %d\n",cfg_data.cfgtype);
		break;
	}

	return rc;
}

int s5k5ccaf_sensor_release(void)
{
	int rc = 0;

	CAM_DEBUG("E");
	
	S5K5CCAF_WRITE_LIST(s5k5ccaf_af_off);
	CAM_DELAY(100);

	s5k5ccaf_set_flash(CAPTURE_FLASH,0);

	kfree(s5k5ccaf_ctrl);
	s5k5ccaf_ctrl = NULL;

#ifdef CONFIG_LOAD_FILE
	s5k5ccaf_regs_table_exit();
#endif
	gpio_set_value_cansleep(CAM_3M_RST, LOW);
	mdelay(1);

	CAM_DEBUG("X");
	return rc;
}

static int s5k5ccaf_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	s5k5ccaf_sensorw =
		kzalloc(sizeof(struct s5k5ccaf_work), GFP_KERNEL);

	if (!s5k5ccaf_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, s5k5ccaf_sensorw);
	s5k5ccaf_init_client(client);
	s5k5ccaf_client = client;


	CDBG("s5k5ccaf_probe succeeded!\n");

	return 0;

probe_failure:
	kfree(s5k5ccaf_sensorw);
	s5k5ccaf_sensorw = NULL;
	printk("s5k5ccaf_probe failed!\n");
	return rc;
}

static const struct i2c_device_id s5k5ccaf_i2c_id[] = {
	{ "s5k5ccaf", 0},
	{ },
};

static struct i2c_driver s5k5ccaf_i2c_driver = {
	.id_table = s5k5ccaf_i2c_id,
	.probe  = s5k5ccaf_i2c_probe,
	.remove = __exit_p(s5k5ccaf_i2c_remove),
	.driver = {
		.name = "s5k5ccaf",
	},
};

static int s5k5ccaf_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{
	int rc = i2c_add_driver(&s5k5ccaf_i2c_driver);
	if (rc < 0 || s5k5ccaf_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

#if 0
	rc = s5k5ccaf_sensor_init_probe(info);
	if (rc < 0)
		goto probe_done;
#endif

	s->s_init = s5k5ccaf_sensor_init;
	s->s_release = s5k5ccaf_sensor_release;
	s->s_config  = s5k5ccaf_sensor_config;
	s->s_ext_config	= s5k5ccaf_sensor_ext_config;
	s->s_camera_type = BACK_CAMERA_2D;
	s->s_mount_angle = 0;

probe_done:
	CDBG("%s %s:%d\n", __FILE__, __func__, __LINE__);
	return rc;
}

static int __s5k5ccaf_probe(struct platform_device *pdev)
{
	printk("############# S5K5CCGX probe ##############\n");

	s5k5ccgx_dev = device_create(sec_class, NULL, 0, NULL, "sec_s5k5ccgx");

	if (IS_ERR(s5k5ccgx_dev)) {
		printk("Failed to create device!");
		return -1;
	}

	if (device_create_file(s5k5ccgx_dev, &s5k5ccaf_cameraflash_attr) < 0) {
		printk("Failed to create device file!(%s)!\n", s5k5ccaf_cameraflash_attr.attr.name);
		return -1;
	}

	if (device_create_file(s5k5ccgx_dev, &s5k5ccaf_camtype_attr) < 0) {
		printk("Failed to create device file!(%s)!\n", s5k5ccaf_camtype_attr.attr.name);
		return -1;
	}
	
	return msm_camera_drv_start(pdev, s5k5ccaf_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __s5k5ccaf_probe,
	.driver = {
		.name = "msm_camera_s5k5ccaf",
		.owner = THIS_MODULE,
	},
};

static int __init s5k5ccaf_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(s5k5ccaf_init);
