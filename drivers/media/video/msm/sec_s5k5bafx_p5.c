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


#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>


#include "sec_s5k5bafx_p5.h"
#include <mach/camera.h>
#include <mach/vreg.h>
#include <linux/io.h>


#include "sec_cam_pmic.h"


#if defined(CONFIG_TARGET_SERIES_P5LTE)
#include "sec_s5k5bafx_reg_p5.h"
#elif defined(CONFIG_TARGET_SERIES_P8LTE)
#include "sec_s5k5bafx_reg_p8_skt.h"
#else
#include "s5k5bafx_regs.h"
#endif

#define SENSOR_DEBUG 0

//#define CONFIG_LOAD_FILE	

#ifdef CONFIG_LOAD_FILE
#define S5K5BAFX_WRITE_LIST(A)	s5k5bafx_sensor_write_list(A,(sizeof(A) / sizeof(A[0])),#A);
#else
#define S5K5BAFX_WRITE_LIST(A)	s5k5bafx_sensor_burst_write_list(A,(sizeof(A) / sizeof(A[0])),#A);
#endif

struct s5k5bafx_work {
	struct work_struct work;
};

static struct  s5k5bafx_work *s5k5bafx_sensorw;
static struct  i2c_client *s5k5bafx_client;

static unsigned int config_csi2;
static struct s5k5bafx_ctrl *s5k5bafx_ctrl;

static DECLARE_WAIT_QUEUE_HEAD(s5k5bafx_wait_queue);
DECLARE_MUTEX(s5k5bafx_sem);


/*=============================================================
	EXTERNAL DECLARATIONS
==============================================================*/

#ifdef CONFIG_LOAD_FILE
static int s5k5bafx_regs_table_write(char *name);
#endif

static int s5k5bafx_sensor_read(unsigned short subaddr, unsigned short *data)
{
	int ret;
	unsigned char buf[2];
	struct i2c_msg msg = { s5k5bafx_client->addr, 0, 2, buf };
	
	buf[0] = (subaddr >> 8);
	buf[1] = (subaddr & 0xFF);

	ret = i2c_transfer(s5k5bafx_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	if (ret == -EIO) 
		goto error;

	msg.flags = I2C_M_RD;
	
	ret = i2c_transfer(s5k5bafx_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	if (ret == -EIO) 
		goto error;

	*data = ((buf[0] << 8) | buf[1]);

error:
	return ret;
}

static int s5k5bafx_sensor_write(unsigned short subaddr, unsigned short val)
{
	unsigned char buf[4];
	struct i2c_msg msg = { s5k5bafx_client->addr, 0, 4, buf };

	//CAM_DEBUG("addr = %4X, value = %4X", subaddr, val);

	buf[0] = (subaddr >> 8);
	buf[1] = (subaddr & 0xFF);
	buf[2] = (val >> 8);
	buf[3] = (val & 0xFF);

	return i2c_transfer(s5k5bafx_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
}

#ifdef CONFIG_LOAD_FILE
static int s5k5bafx_sensor_write_list(const u32 *list, int size, char *name)
{
	int ret = 0;
	unsigned short subaddr = 0;
	unsigned short value = 0;
	int i = 0;

	printk("s5k5bafx_sensor_write_list : %s\n", name);
	
#ifdef CONFIG_LOAD_FILE	
	ret = s5k5bafx_regs_table_write(name);
#else
	for (i = 0; i < size; i++) {

		subaddr = (list[i]>> 16); //address
		value = (list[i] & 0xFFFF); //value
		
		if(subaddr == 0xffff)	{
			msleep(value);
		} else {
		    if(s5k5bafx_sensor_write(subaddr, value) < 0)   {
			    printk("<=PCAM=> sensor_write_list fail...-_-\n");
			    return -1;
		    }
		}
	}
#endif
	return ret;
}
#endif

#define BURST_MODE_BUFFER_MAX_SIZE 2700
unsigned char s5k5bafx_buf_for_burstmode[BURST_MODE_BUFFER_MAX_SIZE];
static int s5k5bafx_sensor_burst_write_list(const u32 *list, int size, char *name)
{
	int err = -EINVAL;
	int i = 0;
	int idx = 0;

	unsigned short subaddr = 0, next_subaddr = 0;
	unsigned short value = 0;

	struct i2c_msg msg = {  s5k5bafx_client->addr, 0, 0, s5k5bafx_buf_for_burstmode};
	
	printk("s5k5bafx_sensor_burst_write_list : %s\n", name);

	for (i = 0; i < size; i++) {
		if(idx > (BURST_MODE_BUFFER_MAX_SIZE-10)) {
			printk("<=PCAM=> BURST MODE buffer overflow!!!\n");
			return err;
		}

		subaddr = (list[i]>> 16); //address
		if(subaddr == 0x0F12) next_subaddr= (list[i+1]>> 16); //address
		value = (list[i] & 0xFFFF); //value
		

		switch(subaddr) {
		case 0x0F12:
			// make and fill buffer for burst mode write
			if(idx ==0) {
				s5k5bafx_buf_for_burstmode[idx++] = 0x0F;
				s5k5bafx_buf_for_burstmode[idx++] = 0x12;
			}
			s5k5bafx_buf_for_burstmode[idx++] = value>> 8;
			s5k5bafx_buf_for_burstmode[idx++] = value & 0xFF;

		
		 	//write in burstmode	
			if(next_subaddr != 0x0F12) 	{
				msg.len = idx;
				err = i2c_transfer(s5k5bafx_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
				//printk("s5k4ecgx_sensor_burst_write, idx = %d\n",idx);
				idx=0;
			}			
			break;

		case 0xFFFF:
			msleep(value);
			break;
	
		default:
		    idx = 0;
		    err = s5k5bafx_sensor_write(subaddr, value);
			break;			
		}		
	}

	if (unlikely(err < 0)) {
		printk("<=PCAM=>%s: register set failed\n",__func__);
		return err;
	}
	
	return 0;
}

int s5k5bafx_set_brightness(int brightness)
{
	int rc = 0;
	
	switch(brightness) {
	case S5K5BAFX_BR_PLUS_4 :
		CAM_DEBUG("S5K5BAFX_BR_PLUS_4");
		S5K5BAFX_WRITE_LIST(s5k5bafx_bright_p4);
		break;
		
	case S5K5BAFX_BR_PLUS_3 :
		CAM_DEBUG("S5K5BAFX_BR_PLUS_3");
		S5K5BAFX_WRITE_LIST(s5k5bafx_bright_p3);
		break;

	case S5K5BAFX_BR_PLUS_2 :
		CAM_DEBUG("S5K5BAFX_BR_PLUS_2");
		S5K5BAFX_WRITE_LIST(s5k5bafx_bright_p2);
		break;

	case S5K5BAFX_BR_PLUS_1 :
		CAM_DEBUG("S5K5BAFX_BR_PLUS_1");
		S5K5BAFX_WRITE_LIST(s5k5bafx_bright_p1);
		break;

	case S5K5BAFX_BR_DEFAULT :
		CAM_DEBUG("S5K5BAFX_BR_DEFAULT");
		S5K5BAFX_WRITE_LIST(s5k5bafx_bright_default);		
		break;

	case S5K5BAFX_BR_MINUS_1 :
		CAM_DEBUG("S5K5BAFX_BR_MINUS_1");
		S5K5BAFX_WRITE_LIST(s5k5bafx_bright_m1);
		break;

	case S5K5BAFX_BR_MINUS_2 :
		CAM_DEBUG("S5K5BAFX_BR_MINUS_2");
		S5K5BAFX_WRITE_LIST(s5k5bafx_bright_m2);
		break;

	case S5K5BAFX_BR_MINUS_3 :
		CAM_DEBUG("S5K5BAFX_BR_MINUS_3");
		S5K5BAFX_WRITE_LIST(s5k5bafx_bright_m3);
		break;

	case S5K5BAFX_BR_MINUS_4 :
		CAM_DEBUG("S5K5BAFX_BR_MINUS_4");
		S5K5BAFX_WRITE_LIST(s5k5bafx_bright_m4);
		break;

	default:
		printk("Invalid brightness (%d) !!!\n", brightness);

	}
	
	s5k5bafx_ctrl->settings.brightness = brightness;
	
	return rc;	
}

int s5k5bafx_set_blur(unsigned int blur)
{
	int rc = 0;
	
	CAM_DEBUG(": %d", blur);
	
	if(s5k5bafx_ctrl->dtp_mode)
		return 0;
	
	switch(blur) {
	case S5K5BAFX_BLUR_LEVEL_0:
		S5K5BAFX_WRITE_LIST(s5k5bafx_vt_pretty_default);
		break;
		
	case S5K5BAFX_BLUR_LEVEL_1:
		S5K5BAFX_WRITE_LIST(s5k5bafx_vt_pretty_default);
		break;
		
	case S5K5BAFX_BLUR_LEVEL_2:
		S5K5BAFX_WRITE_LIST(s5k5bafx_vt_pretty_default);
		break;
		
	case S5K5BAFX_BLUR_LEVEL_3:
		S5K5BAFX_WRITE_LIST(s5k5bafx_vt_pretty_default);
		break;
		
	default:
		printk("Invalid blur (%d) !!!\n", blur);
	}

	return rc;
}

int s5k5bafx_set_fps(unsigned int mode, unsigned int fps)
{
	int rc = 0;
	
#if defined(CONFIG_TARGET_SERIES_P8LTE)
	if(s5k5bafx_ctrl->vtcall_mode) {
		CAM_DEBUG("VT setfile include fps setting, don't set fps mode!\n");
		return rc;
	}
#endif
	CAM_DEBUG(": fps_mode = %d, fps = %d", mode, fps);

	if(mode){
		switch(fps) {
		case S5K5BAFX_7_FPS:
			S5K5BAFX_WRITE_LIST(s5k5bafx_vt_7fps);
			break;

		case S5K5BAFX_10_FPS:
			S5K5BAFX_WRITE_LIST(s5k5bafx_vt_10fps);
			break;
			
		case S5K5BAFX_12_FPS:
			S5K5BAFX_WRITE_LIST(s5k5bafx_vt_12fps);
			break;
			
		case S5K5BAFX_15_FPS:
			S5K5BAFX_WRITE_LIST(s5k5bafx_vt_15fps);
			break;
		
		case S5K5BAFX_30_FPS:
			S5K5BAFX_WRITE_LIST(s5k5bafx_vt_30fps);
			break;
		
		default:
			printk("Invalid fps set %d. Change default fps - 15!!!\n", fps);
			S5K5BAFX_WRITE_LIST(s5k5bafx_vt_15fps);
			break;
		}
	}
	

	s5k5bafx_ctrl->settings.fps_mode = mode;
	s5k5bafx_ctrl->settings.fps = fps;
	
	return rc;
}

int s5k5bafx_set_preview_index(int width, int height) {
	int rc = 0;

	if (width == 640 && height == 480) {
		printk("[kidggang][%s:%d] S5K5CCGX_PREVIEW_VGA !!!\n", __func__, __LINE__);
		s5k5bafx_ctrl->settings.preview_size_idx = S5K5BAFX_PREVIEW_VGA; 
	} else if (width == 800 && height == 600) {
		printk("[kidggang][%s:%d] S5K5CCGX_PREVIEW_SVGA !!!\n", __func__, __LINE__);
		s5k5bafx_ctrl->settings.preview_size_idx = S5K5BAFX_PREVIEW_SVGA;
#if defined(CONFIG_TARGET_SERIES_P8LTE)
	} else if (width == 528 && height == 432) {
		printk("[kidggang][%s:%d] S5K5BAFX_PREVIEW_528x432 !!!\n", __func__, __LINE__);
		s5k5bafx_ctrl->settings.preview_size_idx = S5K5BAFX_PREVIEW_528x432;
	} else if (width == 1024 && height == 768) {
		printk("[kidggang][%s:%d] S5K5CCGX_PREVIEW_XGA !!!\n", __func__, __LINE__);
		s5k5bafx_ctrl->settings.preview_size_idx = S5K5BAFX_PREVIEW_XGA;
#endif
	} else {
		printk("Invalid preview size (%dx%d) !!!\n", width, height);
		rc = -1;
	}
	return rc;
}

int s5k5bafx_set_preview_size(int size)
{
	int rc =0;

	switch(size) {
	case S5K5BAFX_PREVIEW_VGA:	// 640x480
		CAM_DEBUG("640x480");
		S5K5BAFX_WRITE_LIST(s5k5bafx_preview_640_480);		
		break;
		
	case S5K5BAFX_PREVIEW_SVGA:	// 800x600
		CAM_DEBUG("800x600");
		S5K5BAFX_WRITE_LIST(s5k5bafx_preview_800_600);
		break;
#if defined(CONFIG_TARGET_SERIES_P8LTE)
	case S5K5BAFX_PREVIEW_528x432:	// 528x432
		CAM_DEBUG("528x432");
		S5K5BAFX_WRITE_LIST(s5k5bafx_preview_528_432);
		break;
	case S5K5BAFX_PREVIEW_XGA:	// 1024x768
		CAM_DEBUG("1024x768");
		S5K5BAFX_WRITE_LIST(s5k5bafx_preview_1024_768);
		break;
#endif

	default:
		printk("Invalid preview size!!\n");
		break;
	}

	return rc;
}

int s5k5bafx_get_exif_iso(void)
{
	u16 iso_gain_table[] = {10, 18, 23, 28};
	u16 iso_table[] = {0, 50, 100, 200, 400};
	u16 gain = 0, val = 0;
	s32 index = 0;		

	s5k5bafx_sensor_write(0xFCFC, 0xD000);					
	s5k5bafx_sensor_write(0x002C, 0x7000);
	s5k5bafx_sensor_write(0x002E, 0x14C8);		
	s5k5bafx_sensor_read(0x0F12, &val); //0x2A8

	gain = val * 10 / 256;
	for (index = 0; index < 4; index++) {
		if (gain < iso_gain_table[index])
			break;
	}
	
	CAM_DEBUG("iso = %d, val = %d, index = %d\n", iso_table[index], val, index); 
	
	return iso_table[index];	// ISO

}

static void s5k5bafx_get_exif_exposure(void)
{
	unsigned short read_exposureTime;

	s5k5bafx_sensor_write(0xFCFC, 0xD000);					
	s5k5bafx_sensor_write(0x002C, 0x7000);
	s5k5bafx_sensor_write(0x002E, 0x14D0);
	s5k5bafx_sensor_read(0x0F12, &read_exposureTime); 

	s5k5bafx_ctrl->settings.exif_shutterspeed = 500000/read_exposureTime;

	CAM_DEBUG("read_exposureTime = %x \n",read_exposureTime);
}

int s5k5bafx_mipi_mode(int mode)
{
	int rc = 0;
	struct msm_camera_csi_params s5k5bafx_csi_params;
	
	CAM_DEBUG("%s E\n", __FUNCTION__);

	if (!config_csi2) {
		s5k5bafx_csi_params.lane_cnt = 1;
		s5k5bafx_csi_params.data_format = CSI_8BIT;
		s5k5bafx_csi_params.lane_assign = 0xe4;
		s5k5bafx_csi_params.dpcm_scheme = 0;
		s5k5bafx_csi_params.settle_cnt = 24;// 0x14; //0x7; //0x14;
		rc = msm_camio_csi_config(&s5k5bafx_csi_params);
		if (rc < 0)
			printk(KERN_ERR "config csi controller failed \n");
		config_csi2 = 1;
	}
	
	CAM_DEBUG("%s X\n", __FUNCTION__);
	
	return rc;
}

static int s5k5bafx_start(void)
{
	int rc=0;
	unsigned short	id = 0;
	
	CAM_DEBUG("%s E\n", __FUNCTION__);
	CAM_DEBUG("I2C address : 0x%x\n", s5k5bafx_client->addr);
	
#if defined(CONFIG_TARGET_SERIES_P8LTE)
	if(s5k5bafx_ctrl->started && s5k5bafx_ctrl->vtcall_mode == 0 ) { // don't skip during VT test mode (*#7353#)
#else
	if(s5k5bafx_ctrl->started) {
#endif
		CAM_DEBUG("%s X : already started\n", __FUNCTION__);
		return rc;
	}

	s5k5bafx_mipi_mode(1);
	msleep(100); //=> Please add some delay 

	/* read firmware id */
	s5k5bafx_sensor_write(0x002C, 0x0000);
	s5k5bafx_sensor_write(0x002E, 0x0040);
	s5k5bafx_sensor_read(0x0F12, &id);
	printk("CURRENT SENSOR FW => id 0x%x \n", id);

	if (s5k5bafx_ctrl->cam_mode == S5K5BAFX_MOVIE_MODE || s5k5bafx_ctrl->cam_mode == S5K5BAFX_MMS_MODE) {
		CAM_DEBUG("SELF RECORD");
		S5K5BAFX_WRITE_LIST(s5k5bafx_recording_60Hz_common);
	} else {
		if(s5k5bafx_ctrl->vtcall_mode == 0) {
			CAM_DEBUG("SELF CAMERA");
			S5K5BAFX_WRITE_LIST(s5k5bafx_common);
		} else if(s5k5bafx_ctrl->vtcall_mode == 1) {
			CAM_DEBUG("VT CALL");
			S5K5BAFX_WRITE_LIST(s5k5bafx_vt_common);
		} else {
			CAM_DEBUG("WIFI VT CALL");
			S5K5BAFX_WRITE_LIST(s5k5bafx_vt_wifi_common);
		}	
	}
	
	s5k5bafx_ctrl->started = 1;

	return rc;
}

void s5k5bafx_set_preview(void)
{
	CAM_DEBUG("s5k5bafx_ctrl->settings.preview_size_idx = %d", s5k5bafx_ctrl->settings.preview_size_idx);
	if(s5k5bafx_ctrl->dtp_mode == 0) {
		s5k5bafx_ctrl->op_mode = S5K5BAFX_MODE_PREVIEW;	
#if defined(CONFIG_TARGET_SERIES_P8LTE)
		if( s5k5bafx_ctrl->vtcall_mode == 0 &&
			(s5k5bafx_ctrl->sensor_mode != 0 || s5k5bafx_ctrl->app_mode==S5K5BAFX_3RD_PARTY_APP || s5k5bafx_ctrl->cam_mode != S5K5BAFX_CAMERA_MODE ))
#endif
		{
		s5k5bafx_set_preview_size(s5k5bafx_ctrl->settings.preview_size_idx);
		}
		s5k5bafx_set_brightness(s5k5bafx_ctrl->settings.brightness);
		if(s5k5bafx_ctrl->cam_mode == S5K5BAFX_MOVIE_MODE || s5k5bafx_ctrl->cam_mode == S5K5BAFX_MMS_MODE) {
			s5k5bafx_set_fps(s5k5bafx_ctrl->cam_mode, s5k5bafx_ctrl->settings.fps); // temp - fixed fps
		} else { //for Qik, HDVT
			if(s5k5bafx_ctrl->vtcall_mode) {
				s5k5bafx_set_fps(s5k5bafx_ctrl->vtcall_mode, s5k5bafx_ctrl->settings.fps); // temp - fixed fps
			} else {
				s5k5bafx_set_fps(s5k5bafx_ctrl->settings.fps_mode, s5k5bafx_ctrl->settings.fps);
			}
		}
	} else {
		printk("start DTP mode!\n");
		s5k5bafx_ctrl->op_mode = S5K5BAFX_MODE_DTP;	
		S5K5BAFX_WRITE_LIST(s5k5bafx_pattern_on);
	}	
}

void s5k5bafx_set_capture(void)
{
	CAM_DEBUG("");	
	s5k5bafx_ctrl->op_mode = S5K5BAFX_MODE_CAPTURE;	
	S5K5BAFX_WRITE_LIST(s5k5bafx_capture);

	s5k5bafx_get_exif_exposure();	
}

static long s5k5bafx_set_sensor_mode(int mode)
{
	CAM_DEBUG("s5k5bafx_set_sensor_mode : %d\n", mode);
	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		s5k5bafx_start();
		s5k5bafx_set_preview();
		break;
		
	case SENSOR_SNAPSHOT_MODE:
	case SENSOR_RAW_SNAPSHOT_MODE:
		s5k5bafx_set_capture();
		break;
		
	default:
		return -EINVAL;
	}

	s5k5bafx_ctrl->sensor_mode = mode;
		
	return 0;
}

int s5k5bafx_set_dtp(int* onoff)
{
	int rc = 0;
	switch(*onoff) {
	case S5K5BAFX_DTP_OFF:
		printk("DTP OFF\n");
		S5K5BAFX_WRITE_LIST(s5k5bafx_pattern_off);
		s5k5bafx_ctrl->dtp_mode = 0;
		break;

	case S5K5BAFX_DTP_ON:
		printk("DTP ON\n");
		//S5K5BAFX_WRITE_LIST(s5k5bafx_pattern_on);
		/* set ACK value */
		(*onoff) = DTP_ON_ACK;
		s5k5bafx_ctrl->dtp_mode = 1;
		break;

	default:
		printk("Invalid DTP mode!!\n");
	}

	return rc;
}

int s5k5bafx_esd_reset(void)
{
	printk("reset for recovery from ESD\n");
	gpio_set_value_cansleep(CAM_3M_RST, LOW);
	gpio_set_value_cansleep(CAM_3M_STBY, LOW);	
	gpio_set_value_cansleep(CAM_VGA_RST, LOW);
	gpio_set_value_cansleep(CAM_VGA_STBY, LOW);	

	cam_ldo_power_off();	
	mdelay(5);
	cam_ldo_power_on("s5k5bafx");

	gpio_set_value_cansleep(CAM_VGA_RST, HIGH);

	S5K5BAFX_WRITE_LIST(s5k5bafx_common);
	
	s5k5bafx_set_preview();
	
	return 0;
}

int s5k5bafx_sensor_ext_config(void __user *arg)
{
	int rc = 0;
	sensor_ext_cfg_data cfg_data;
	
	if(copy_from_user((void *)&cfg_data, (const void *)arg, sizeof(cfg_data))) {
		printk("%s fail copy_from_user!\n", __func__);
	}

	switch(cfg_data.cmd) {
	case EXT_CFG_ESD_RESET:
		s5k5bafx_esd_reset();
		break;
		
	case EXT_CFG_SET_BRIGHTNESS:
		rc = s5k5bafx_set_brightness(cfg_data.value_1);
		break;

	case EXT_CFG_SET_BLUR:
		rc = s5k5bafx_set_blur(cfg_data.value_1);
		break;
		
	case EXT_CFG_SET_FPS:
		rc = s5k5bafx_set_fps(cfg_data.value_1,cfg_data.value_2);
		break;
		
	case EXT_CFG_SET_DTP:
		rc = s5k5bafx_set_dtp(&cfg_data.value_1);
		break;

	case EXT_CFG_SET_VT_MODE:
		CAM_DEBUG("VTCall mode : %d",cfg_data.value_1);
		s5k5bafx_ctrl->vtcall_mode = cfg_data.value_1;
		break;
		
	case EXT_CFG_SET_MOVIE_MODE:						
		CAM_DEBUG("Movie mode : %d",cfg_data.value_1);
		s5k5bafx_ctrl->cam_mode = cfg_data.value_1;
		break;

	case EXT_CFG_SET_VENDOR:						
		CAM_DEBUG("Vendor app mode : %d",cfg_data.value_1);
		s5k5bafx_ctrl->app_mode = cfg_data.value_1;
		break;

	case EXT_CFG_GET_EXIF_EXPOSURE:	
		cfg_data.value_1 = s5k5bafx_ctrl->settings.exif_shutterspeed;		
		break;

	case EXT_CFG_GET_EXIF_ISO:	
		rc = s5k5bafx_get_exif_iso();
		cfg_data.value_1 = rc;
		break;

	case EXT_CFG_SET_PREVIEW_SIZE:	
		CAM_DEBUG("set preview size index = %dx%d", cfg_data.value_1, cfg_data.value_2);
		s5k5bafx_set_preview_index(cfg_data.value_1, cfg_data.value_2);	//just set index
		break;

	default:
		break;	
	}
	
	if(copy_to_user((void *)arg, (const void *)&cfg_data, sizeof(cfg_data))) {
		printk(" %s : copy_to_user Failed \n", __func__);
	}

	return rc;
	
}

static int s5k5bafx_sensor_pre_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

#ifndef CONFIG_LOAD_FILE
	rc = S5K5BAFX_WRITE_LIST(s5k5bafx_pre_init0);
	if(rc < 0)
		printk("Error in s5k5bafx_sensor_pre_init!\n");
#endif
	mdelay(10);

	return rc;
}
static int s5k5bafx_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
	int rc = 0;
	printk("[S5K5BAFX] sensor_init_probe()\n");

	gpio_set_value_cansleep(CAM_3M_RST, LOW);
	gpio_set_value_cansleep(CAM_3M_STBY, LOW);	
	gpio_set_value_cansleep(CAM_VGA_RST, LOW);
	
	//cam_ldo_power_on();	// move to msm_camera.c (Jeonhk 20110622)

	mdelay(1);	// 10 -> 1	
	
	gpio_set_value_cansleep(CAM_VGA_RST, HIGH);
	mdelay(10);

	rc = s5k5bafx_sensor_pre_init(data);

	return rc;
}

#ifdef CONFIG_LOAD_FILE

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>

#include <asm/uaccess.h>

static char *s5k5bafx_regs_table = NULL;

static int s5k5bafx_regs_table_size;

void s5k5bafx_regs_table_init(void)
{
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int ret;
	mm_segment_t fs = get_fs();

	printk("%s %d\n", __func__, __LINE__);

	set_fs(get_ds());
	filp = filp_open("/sdcard/s5k5bafx_regs.h", O_RDONLY, 0);
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

	s5k5bafx_regs_table = dp;
	
	s5k5bafx_regs_table_size = l;

	*((s5k5bafx_regs_table + s5k5bafx_regs_table_size) - 1) = '\0';

	//printk("s5k5bafx_regs_table 0x%x, %ld\n", dp, l);
}

void s5k5bafx_regs_table_exit(void)
{
	printk("%s %d\n", __func__, __LINE__);
	if (s5k5bafx_regs_table) {
		kfree(s5k5bafx_regs_table);
		s5k5bafx_regs_table = NULL;
	}	
}

static int s5k5bafx_regs_table_write(char *name)
{
	char *start, *end, *reg;
	unsigned short addr, value;
	char reg_buf[7], data_buf[7];

	int idx = 0, err = 0;
	struct i2c_msg msg = {  s5k5bafx_client->addr, 0, 0, s5k5bafx_buf_for_burstmode};
	
	printk("s5k5bafx_regs_table_write : %s\n", name);
	
	addr = value = 0;

	*(reg_buf + 4) = '\0';
	*(data_buf + 4) = '\0';

	start = strstr(s5k5bafx_regs_table, name);
	
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
#ifndef CONFIG_LOAD_FILE
			case 0x0F12:
				// make and fill buffer for burst mode write
				if(idx ==0) {
					s5k5bafx_buf_for_burstmode[idx++] = 0x0F;
					s5k5bafx_buf_for_burstmode[idx++] = 0x12;
				}
				s5k5bafx_buf_for_burstmode[idx++] = value>> 8;
				s5k5bafx_buf_for_burstmode[idx++] = value & 0xFF;
				break;
#endif
			case 0xFFFF:
				msleep(value);
				break;
		
			default:
#ifndef CONFIG_LOAD_FILE
				if(idx > 0) 	{
					msg.len = idx;
					err = i2c_transfer(s5k5bafx_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
					//printk("s5k5bafx_regs_table_write, idx = %d\n",idx);
					idx=0;
					msleep(1);
				}
#endif
				err = s5k5bafx_sensor_write(addr, value);		// send stored data and current data
				//printk("addr 0x%x, value 0x%x\n", addr, value);
				break;			
			}

		}
		else
			printk("<=PCAM=> EXCEPTION! reg value : %c  addr : 0x%x,  value : 0x%x\n", *reg, addr, value);
	}

	if(idx > 0) {
		msg.len = idx;
		err = i2c_transfer(s5k5bafx_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
		//printk("s5k5bafx_regs_table_write, idx = %d\n",idx);
		idx=0;
		msleep(1);
	}	

	return 0;
}
#endif

int s5k5bafx_sensor_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	CDBG("s5k5bafx_sensor_init E\n");

	s5k5bafx_ctrl = kzalloc(sizeof(struct s5k5bafx_ctrl), GFP_KERNEL);
	if (!s5k5bafx_ctrl) {
		printk("s5k5bafx_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	if (data)
		s5k5bafx_ctrl->sensordata = data;

	s5k5bafx_ctrl->settings.preview_size_idx = S5K5BAFX_PREVIEW_VGA;
	s5k5bafx_ctrl->settings.brightness = S5K5BAFX_BR_DEFAULT;

	config_csi2 = 0;
#ifdef CONFIG_LOAD_FILE
	s5k5bafx_regs_table_init();
#endif	

	rc = s5k5bafx_sensor_init_probe(data);
	if (rc < 0) {
		printk("s5k5bafx_sensor_init failed!\n");
		goto init_fail;
	}

init_done:
	return rc;

init_fail:
	kfree(s5k5bafx_ctrl);
	s5k5bafx_ctrl = NULL;
	return rc;
}

static int s5k5bafx_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&s5k5bafx_wait_queue);
	return 0;
}

int s5k5bafx_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	long   rc = 0;

	if (copy_from_user(&cfg_data,
			(void *)argp,
			sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	CDBG("s5k5bafx_ioctl, cfgtype = %d, mode = %d\n",
		cfg_data.cfgtype, cfg_data.mode);

	switch (cfg_data.cfgtype) {
	case CFG_SET_MODE:
		rc = s5k5bafx_set_sensor_mode(cfg_data.mode);
		break;

	case CFG_SET_EFFECT:
		//rc = s5k5bafx_set_effect(cfg_data.mode, cfg_data.cfg.effect);
		break;

	case CFG_GET_AF_MAX_STEPS:
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

int s5k5bafx_sensor_release(void)
{
	int rc = 0;

	printk("<=PCAM=> s5k5bafx_sensor_release\n");

	kfree(s5k5bafx_ctrl);
	s5k5bafx_ctrl = NULL;

	gpio_set_value_cansleep(CAM_VGA_RST, LOW);	
	mdelay(1);
	//gpio_set_value_cansleep(CAM_VGA_STBY, LOW);

#ifdef CONFIG_LOAD_FILE
	s5k5bafx_regs_table_exit();
#endif

	//cam_ldo_power_off();
	return rc;
}

static int s5k5bafx_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	s5k5bafx_sensorw =
		kzalloc(sizeof(struct s5k5bafx_work), GFP_KERNEL);

	if (!s5k5bafx_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, s5k5bafx_sensorw);
	s5k5bafx_init_client(client);
	s5k5bafx_client = client;

	CDBG("s5k5bafx_probe succeeded!\n");

	return 0;

probe_failure:
	kfree(s5k5bafx_sensorw);
	s5k5bafx_sensorw = NULL;
	printk("s5k5bafx_probe failed!\n");
	return rc;
}

static const struct i2c_device_id s5k5bafx_i2c_id[] = {
	{ "s5k5bafx", 0 },
	{ },
};

static struct i2c_driver s5k5bafx_i2c_driver = {
	.id_table = s5k5bafx_i2c_id,
	.probe  = s5k5bafx_i2c_probe,
	.remove = __exit_p(s5k5bafx_i2c_remove),
	.driver = {
		.name = "s5k5bafx",
	},
};


//////////////////////////////////////////////////////////////
//#ifdef FACTORY_TEST
extern struct class *sec_class;
struct device *s5k5bafx_dev;

static ssize_t cameratype_file_cmd_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	char sensor_info[30] = "S5K5BAFX";
	return sprintf(buf, "%s\n", sensor_info);
}

static ssize_t cameratype_file_cmd_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t size)
{
	/*Reserved*/
	return size;
}

//static DEVICE_ATTR(cameratype, 0660, cameratype_file_cmd_show, cameratype_file_cmd_store);
static struct device_attribute s5k5bafx_camtype_attr = {
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
#if 0
	int value = 0;
	sscanf(buf, "%d", &value);

	if (value == 0) {
		printk(KERN_INFO "[Factory flash]OFF\n");
		//s5k5ccaf_torch(0);
		s5k5ccaf_set_flash(MOVIE_FLASH,0);
	} else {
		printk(KERN_INFO "[Factory flash]ON\n");
		//s5k5ccaf_torch(1);
		s5k5ccaf_set_flash(MOVIE_FLASH,1);
	}
#endif
	return size;
}

//static DEVICE_ATTR(cameraflash, 0660, cameraflash_file_cmd_show, cameraflash_file_cmd_store);
static struct device_attribute s5k5bafx_cameraflash_attr = {
	.attr = {
		.name = "cameraflash",
		.mode = 0660},// (S_IRUGO | S_IWUGO)},
	.show = cameraflash_file_cmd_show,
	.store = cameraflash_file_cmd_store
};
//#endif

//////////////////////////////////////////////////////////////
static int s5k5bafx_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{

	int rc = i2c_add_driver(&s5k5bafx_i2c_driver);
	if (rc < 0 || s5k5bafx_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

#if 0
	rc = s5k5bafx_sensor_init_probe(info);
	if (rc < 0)
		goto probe_done;
#endif

	s->s_init = s5k5bafx_sensor_init;
	s->s_release = s5k5bafx_sensor_release;
	s->s_config  = s5k5bafx_sensor_config;
	s->s_ext_config	= s5k5bafx_sensor_ext_config;
	s->s_camera_type = FRONT_CAMERA_2D;
	s->s_mount_angle = 0;

probe_done:
	CDBG("%s %s:%d\n", __FILE__, __func__, __LINE__);
	return rc;
}

static int __s5k5bafx_probe(struct platform_device *pdev)
{
	printk("############# S5K5BAFX probe ##############\n");

	s5k5bafx_dev = device_create(sec_class, NULL, 0, NULL, "sec_s5k5bafx");

	if (IS_ERR(s5k5bafx_dev)) {
		printk("Failed to create device!");
		return -1;
	}

	if (device_create_file(s5k5bafx_dev, &s5k5bafx_camtype_attr) < 0) {
		printk("Failed to create device file!(%s)!\n", s5k5bafx_camtype_attr.attr.name);
		return -1;
	}

	return msm_camera_drv_start(pdev, s5k5bafx_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __s5k5bafx_probe,
	.driver = {
		.name = "msm_camera_s5k5bafx",
		.owner = THIS_MODULE,
	},
};

static int __init s5k5bafx_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(s5k5bafx_init);
