/*
 * =====================================================================================
 *
 *       Filename:  p5lte_cmc623.c
 *
 *    Description:  cmc623, lvds control driver
 *
 *        Version:  1.0
 *        Created:  2011y 04m 08d 16h 45m 50s
 *       Compiler:  arm-linux-gcc
 *
 *         Author:  Park Gyu Tae, 
 *        Company:  Samsung Electronics
 *
 * =====================================================================================
 */
/*  
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2011, Samsung Electronics. All rights reserved.

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
#include <linux/delay.h>
#include <mach/gpio.h>
#include "msm_fb.h"
#include <linux/regulator/pmic8058-regulator.h>
#include <linux/regulator/pmic8901-regulator.h>
#include <linux/mfd/pmic8058.h>
#include "pxlte_cmc623.h"

/*
	 * V_IMA_1.8V		VREG_L3A
	 * DISPLAY_3.3V		VREG_L8A
	 * V_IMA_1.2V		GPIO LCD_PWR_EN 
	 * LCD_VOUT			GPIO LCD_PWR_EN
*/
static struct regulator *v_ima_1_8v = NULL;
static struct regulator *display_3_3v = NULL;
#define LCD_PWR_EN 70

struct cmc623_data {
	struct i2c_client *client;
};


static struct cmc623_data *p_cmc623_data;
static struct i2c_client *g_client;
#define I2C_M_WR 0 /* for i2c */
#define I2c_M_RD 1 /* for i2c */
unsigned long last_cmc623_Bank = 0xffff;
unsigned long last_cmc623_Algorithm = 0xffff;



#define CMC623_BRIGHTNESS_MAX_LEVEL 1600
static int current_gamma_level = CMC623_BRIGHTNESS_MAX_LEVEL;
static int current_cabc_onoff = 1;

#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU)
static int cmc623_current_region_enable = false; //region mode added
#endif

#if defined (CONFIG_TARGET_SERIES_P4LTE)
/* APP brightness level (0~255)*/
#define MAX_BRIGHTNESS_LEVEL 145 	/*57%*/
//#define MID_BRIGHTNESS_LEVEL 105 	/*41%*/ - HW requested value
#define MID_BRIGHTNESS_LEVEL 90 	/*35%*/ // tmp value for low current
#define LOW_BRIGHTNESS_LEVEL 25		/*10%*/
#define DIM_BRIGHTNESS_LEVEL 25		/*10%*/

/* BLU brightness level (1~1600) */
#define MAX_BACKLIGHT_VALUE 912 	/*57%*/
//#define MID_BACKLIGHT_VALUE 656	/*41%*/ - HW requested value  	
#define MID_BACKLIGHT_VALUE 560		/*35%*/ // tmp value for low current
#define LOW_BACKLIGHT_VALUE 160		/*10%*/
#define DIM_BACKLIGHT_VALUE 160		/*10%*/
#endif

struct cmc623_state_type cmc623_state = {
	.cabc_mode = CABC_ON_MODE,
	.brightness = 42,
	.suspended = 0,
    .scenario = mDNIe_UI_MODE,
    .background = STANDARD_MODE,
    .temperature = TEMP_STANDARD,
    .ove = OUTDOOR_OFF_MODE,
    .sub_tune = NULL,
    .main_tune = NULL,
};


static DEFINE_MUTEX(tuning_mutex);
/*
* functions for I2C transactions
*/
unsigned char cmc623_Power_LUT[NUM_POWER_LUT][NUM_ITEM_POWER_LUT] = {
	{ 0x42, 0x47, 0x3E, 0x52, 0x42, 0x3F, 0x3A, 0x37, 0x3F },      	
    { 0x38, 0x3d, 0x34, 0x48, 0x38, 0x35, 0x30, 0x2d, 0x35 },
};

static int is_cmc623_on = 0;
/*  ###############################################################
 *  #
 *  #       TUNE VALUE
 *  #
 *  ############################################################### */
unsigned char cmc623_default_plut[NUM_ITEM_POWER_LUT] = {
     0x42, 0x47, 0x3E, 0x52, 0x42, 0x3F, 0x3A, 0x37, 0x3F      	    
};


unsigned char cmc623_video_plut[NUM_ITEM_POWER_LUT] = {
    0x38, 0x3d, 0x34, 0x48, 0x38, 0x35, 0x30, 0x2d, 0x35 
};

const struct str_sub_tuning sub_tune_value[MAX_TEMP_MODE][MAX_OUTDOOR_MODE] = {
    {   
        {   
            .value[CABC_OFF_MODE] = {.name = "STANDARD,OUTDOOR:OFF,CABC:OFF",  .value = NULL, .size = 0}, 
            .value[CABC_ON_MODE] = {.name = "STANDARD,OUTDOOR:OFF,CABC:ON",   .value = NULL, .size = 0} 
        },
        { 
            .value[CABC_OFF_MODE] = {.name = "STANDARD,OUTDOOR:ON,CABC:OFF",   .value = ove_cabcoff, .size = ARRAY_SIZE(ove_cabcoff)},
            .value[CABC_ON_MODE] = {.name = "STANDARD,OUTDOOR:ON,CABC:ON",    .value = ove_cabcon, .size = ARRAY_SIZE(ove_cabcon)}
        } 
    },
    {   
        {
            .value[CABC_OFF_MODE] = {.name = "WARM,OUTDOOR:OFF,CABC:OFF",      .value = warm_cabcoff, .size = ARRAY_SIZE(warm_cabcoff)},      
            .value[CABC_ON_MODE] = {.name = "WARM,OUTDOOR:OFF,CABC:ON",       .value = warm_cabcon, .size = ARRAY_SIZE(warm_cabcon)}
        },
        {
            .value[CABC_OFF_MODE] = {.name = "WARM,OUTDOOR:ON,CABC:OFF",       .value = warm_ove_cabcoff, .size = ARRAY_SIZE(warm_ove_cabcoff)},     
            .value[CABC_ON_MODE] = {.name = "WARM,OUTDOOR:ON,CABC:ON",        .value = warm_ove_cabcon, .size = ARRAY_SIZE(warm_ove_cabcon)}
        }
    },
    {   
        {
            .value[CABC_OFF_MODE] = {.name = "COLD,OUTDOOR:OFF,CABC:OFF",      .value = cold_cabcoff, .size = ARRAY_SIZE(cold_cabcoff)},      
            .value[CABC_ON_MODE] = {.name = "COLD,OUTDOOR:OFF,CABC:ON",       .value = cold_cabcon, .size = ARRAY_SIZE(cold_cabcon)}
    },
        {
            .value[CABC_OFF_MODE] = {.name = "COLD,OUTDOOR:ON,CABC:OFF",       .value = cold_ove_cabcoff, .size = ARRAY_SIZE(cold_ove_cabcoff)},     
            .value[CABC_ON_MODE] = {.name = "COLD,OUTDOOR:ON,CABC:ON",        .value = cold_ove_cabcon, .size = ARRAY_SIZE(cold_ove_cabcon)}
        }
    },
};

const struct str_main_tuning tune_value[MAX_BACKGROUND_MODE][MAX_mDNIe_MODE] = {
    
    {{.value[CABC_OFF_MODE] = {.name = "DYN_UI_OFF",        .flag = 0, .tune = dynamic_ui_cabcoff, .plut = NULL, .size = ARRAY_SIZE(dynamic_ui_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "DYN_UI_ON",         .flag = 0, .tune = dynamic_ui_cabcon, .plut = NULL, .size = ARRAY_SIZE(dynamic_ui_cabcon)}},
     {.value[CABC_OFF_MODE] = {.name = "DYN_VIDEO_OFF",     .flag = 0, .tune = dynamic_video_cabcoff, .plut = cmc623_video_plut , .size = ARRAY_SIZE(dynamic_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "DYN_VIDEO_ON",      .flag = 0, .tune = dynamic_video_cabcon, .plut = cmc623_video_plut , .size = ARRAY_SIZE(dynamic_video_cabcon)}},
     {.value[CABC_OFF_MODE] = {.name = "DYN_VIDEO_W_OFF",   .flag = 0, .tune = dynamic_video_cabcoff, .plut = cmc623_video_plut , .size = ARRAY_SIZE(dynamic_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "DYN_VIDEO_W_ON",    .flag = 0, .tune = dynamic_video_cabcon, .plut = cmc623_video_plut , .size = ARRAY_SIZE(dynamic_video_cabcon)}},
     {.value[CABC_OFF_MODE] = {.name = "DYN_VIDEO_C_OFF",   .flag = 0, .tune = dynamic_video_cabcoff, .plut = cmc623_video_plut , .size = ARRAY_SIZE(dynamic_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "DYN_VIDEO_C_ON",    .flag = 0, .tune = dynamic_video_cabcon, .plut = cmc623_video_plut , .size = ARRAY_SIZE(dynamic_video_cabcon)}},
     {.value[CABC_OFF_MODE] = {.name = "DYN_CAMERA_OFF",    .flag = TUNE_FLAG_CABC_ALWAYS_OFF, .tune = camera, .plut = NULL , .size = ARRAY_SIZE(camera)},
      .value[CABC_ON_MODE]  = {.name = "DYN_CAMERA_ON",     .flag = TUNE_FLAG_CABC_ALWAYS_OFF, .tune = camera, .plut = NULL , .size = ARRAY_SIZE(camera)}},
     {.value[CABC_OFF_MODE] = {.name = "DYN_NAVI_OFF",      .flag = 0, .tune = NULL, .plut = NULL , .size = 0},
      .value[CABC_ON_MODE]  = {.name = "DYN_NAVI_ON",       .flag = 0, .tune = NULL, .plut = NULL , .size = 0}},
     {.value[CABC_OFF_MODE] = {.name = "DYN_GALLERY_OFF",   .flag = 0, .tune = dynamic_gallery_cabcoff, .plut = NULL , .size = ARRAY_SIZE(dynamic_gallery_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "DYN_GALLERY_ON",    .flag = 0, .tune = dynamic_gallery_cabcon, .plut = NULL , .size = ARRAY_SIZE(dynamic_gallery_cabcon)}},
      {.value[CABC_OFF_MODE] = {.name = "DYN_DMB_OFF",     .flag = 0, .tune = dynamic_dmb_cabcoff, .plut = cmc623_video_plut , .size = ARRAY_SIZE(dynamic_dmb_cabcoff)},
    .value[CABC_ON_MODE]  = {.name = "DYN_DMB_ON",      .flag = 0, .tune = dynamic_dmb_cabcon, .plut = cmc623_video_plut , .size = ARRAY_SIZE(dynamic_dmb_cabcon)}}},
      
    {{.value[CABC_OFF_MODE] = {.name = "STD_UI_OFF",        .flag = 0, .tune = standard_ui_cabcoff, .plut = NULL , .size = ARRAY_SIZE(standard_ui_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "STD_UI_ON",         .flag = 0, .tune = standard_ui_cabcon, .plut = NULL , .size = ARRAY_SIZE(standard_ui_cabcon)}},
     {.value[CABC_OFF_MODE] = {.name = "STD_VIDEO_OFF",     .flag = 0, .tune = standard_video_cabcoff, .plut = cmc623_video_plut , .size = ARRAY_SIZE(standard_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "STD_VIDEO_ON",      .flag = 0, .tune = standard_video_cabcon, .plut = cmc623_video_plut , .size = ARRAY_SIZE(standard_video_cabcon)}},
     {.value[CABC_OFF_MODE] = {.name = "STD_VIDEO_W_OFF",   .flag = 0, .tune = standard_video_cabcoff, .plut = cmc623_video_plut , .size = ARRAY_SIZE(standard_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "STD_VIDEO_W_ON",    .flag = 0, .tune = standard_video_cabcon, .plut = cmc623_video_plut , .size = ARRAY_SIZE(standard_video_cabcon)}},
     {.value[CABC_OFF_MODE] = {.name = "STD_VIDEO_C_OFF",   .flag = 0, .tune = standard_video_cabcoff, .plut = cmc623_video_plut , .size = ARRAY_SIZE(standard_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "STD_VIDEO_C_ON",    .flag = 0, .tune = standard_video_cabcon, .plut = cmc623_video_plut , .size = ARRAY_SIZE(standard_video_cabcon) }},
     {.value[CABC_OFF_MODE] = {.name = "STD_CAMERA_OFF",    .flag = TUNE_FLAG_CABC_ALWAYS_OFF, .tune = camera, .plut = NULL , .size = ARRAY_SIZE(camera)},
      .value[CABC_ON_MODE]  = {.name = "STD_CAMERA_ON",     .flag = TUNE_FLAG_CABC_ALWAYS_OFF, .tune = camera, .plut = NULL , .size = ARRAY_SIZE(camera)}},
     {.value[CABC_OFF_MODE] = {.name = "STD_NAVI_OFF",      .flag = 0, .tune = NULL, .plut = NULL, .size = 0},
      .value[CABC_ON_MODE]  = {.name = "STD_NAVI_ON",       .flag = 0, .tune = NULL, .plut = NULL , .size = 0}},
     {.value[CABC_OFF_MODE] = {.name = "STD_GALLERY_OFF",   .flag = 0, .tune = standard_gallery_cabcoff, .plut = NULL , .size = ARRAY_SIZE(standard_gallery_cabcoff) },
      .value[CABC_ON_MODE]  = {.name = "STD_GALLERY_ON",    .flag = 0, .tune = standard_gallery_cabcon, .plut = NULL , .size = ARRAY_SIZE(standard_gallery_cabcon)}},
      {.value[CABC_OFF_MODE] = {.name = "STD_DMB_OFF",     .flag = 0, .tune = standard_dmb_cabcoff, .plut = cmc623_video_plut , .size = ARRAY_SIZE(standard_dmb_cabcoff)},
    .value[CABC_ON_MODE]  = {.name = "STD_DMB_ON",      .flag = 0, .tune = standard_dmb_cabcon, .plut = cmc623_video_plut , .size = ARRAY_SIZE(standard_dmb_cabcon)}}},
     
    {{.value[CABC_OFF_MODE] = {.name = "MOV_UI_OFF",        .flag = 0, .tune = movie_ui_cabcoff, .plut = NULL , .size = ARRAY_SIZE(movie_ui_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "MOV_UI_ON",         .flag = 0, .tune = movie_ui_cabcon, .plut = NULL , .size = ARRAY_SIZE(movie_ui_cabcon)}},
     {.value[CABC_OFF_MODE] = {.name = "MOV_VIDEO_OFF",     .flag = 0, .tune = movie_video_cabcoff, .plut = cmc623_video_plut , .size = ARRAY_SIZE(movie_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "MOV_VIDEO_ON",      .flag = 0, .tune = movie_video_cabcon, .plut = cmc623_video_plut , .size = ARRAY_SIZE(movie_video_cabcon)}},
     {.value[CABC_OFF_MODE] = {.name = "MOV_VIDEO_W_OFF",   .flag = 0, .tune = movie_video_cabcoff, .plut = cmc623_video_plut , .size = ARRAY_SIZE(movie_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "MOV_VIDEO_W_ON",    .flag = 0, .tune = movie_video_cabcon, .plut = cmc623_video_plut , .size = ARRAY_SIZE(movie_video_cabcon)}},
     {.value[CABC_OFF_MODE] = {.name = "MOV_VIDEO_C_OFF",   .flag = 0, .tune = movie_video_cabcoff, .plut = cmc623_video_plut , .size = ARRAY_SIZE(movie_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "MOV_VIDEO_C_ON",    .flag = 0, .tune = movie_video_cabcon, .plut = cmc623_video_plut , .size = ARRAY_SIZE(movie_video_cabcon)}},
     {.value[CABC_OFF_MODE] = {.name = "MOV_CAMERA_OFF",    .flag = TUNE_FLAG_CABC_ALWAYS_OFF, .tune = camera, .plut = NULL , .size = ARRAY_SIZE(camera)},
      .value[CABC_ON_MODE]  = {.name = "MOV_CAMERA_ON",     .flag = TUNE_FLAG_CABC_ALWAYS_OFF, .tune = camera, .plut = NULL , .size = ARRAY_SIZE(camera)}},
     {.value[CABC_OFF_MODE] = {.name = "MOV_NAVI_OFF",      .flag = 0, .tune = NULL, .plut = NULL , .size = 0},
      .value[CABC_ON_MODE]  = {.name = "MOV_NAVI_ON",       .flag = 0, .tune = NULL, .plut = NULL , .size = 0}},
     {.value[CABC_OFF_MODE] = {.name = "MOV_GALLERY_OFF",   .flag = 0, .tune = movie_gallery_cabcoff, .plut = NULL , .size = ARRAY_SIZE(movie_gallery_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "MOV_GALLERY_ON",    .flag = 0, .tune = movie_gallery_cabcon, .plut = NULL , .size = ARRAY_SIZE(movie_gallery_cabcon)}},
      {.value[CABC_OFF_MODE] = {.name = "MOV_DMB_OFF",     .flag = 0, .tune = movie_dmb_cabcoff, .plut = cmc623_video_plut , .size = ARRAY_SIZE(movie_dmb_cabcoff)},
    .value[CABC_ON_MODE]  = {.name = "MOV_DMB_ON",      .flag = 0, .tune = movie_dmb_cabcon, .plut = cmc623_video_plut , .size = ARRAY_SIZE(movie_dmb_cabcon)}}},      	
};
/*  end of TUNE VALUE
 *  ########################################################## */

bool cmc623_I2cWrite16(unsigned char Addr, unsigned long Data)
{
	int err = -1000;
	struct i2c_msg msg[1];
	unsigned char data[3];

	if (!p_cmc623_data) {
		pr_err("p_cmc623_data is NULL\n");
		return -ENODEV;
	}

	if(!is_cmc623_on) {
		pr_err("cmc623 power down..\n");
		return -ENODEV;
	}
	g_client = p_cmc623_data->client;

	if ((g_client == NULL)) {
		pr_err("cmc623_I2cWrite16 g_client is NULL\n");
		return -ENODEV;
	}

	if (!g_client->adapter) {
		pr_err("cmc623_I2cWrite16 g_client->adapter is NULL\n");
		return -ENODEV;
	}
	
#if defined(CONFIG_KOR_OPERATOR_SKT)|| defined(CONFIG_KOR_OPERATOR_LGU)
		if(Addr == 0x0000)
		{
		if(Data == last_cmc623_Bank)
			{
			return 0;
			}
		last_cmc623_Bank = Data;
		}
	else if(Addr == 0x0001 && last_cmc623_Bank==0)
		{
		last_cmc623_Algorithm = Data;
		}
#endif

	data[0] = Addr;
	data[1] = ((Data >> 8)&0xFF);
	data[2] = (Data)&0xFF;
	msg->addr = g_client->addr;
	msg->flags = I2C_M_WR;
	msg->len = 3;
	msg->buf = data;

	err = i2c_transfer(g_client->adapter, msg, 1);

	if (err >= 0) {
		pr_debug("%s %d i2c transfer OK\n", __func__, __LINE__);
		return 0;
	}

	pr_err("%s i2c transfer error:%d(a:%d)\n",
			__func__, err, Addr);
	return err;
}

int cmc623_I2cRead16(u8 reg, u16 *val)
{
	int 	 err;
	struct	 i2c_msg msg[2];
	u8 regaddr = reg;
	u8 data[2];

	if (!p_cmc623_data) {
		pr_err("%s p_cmc623_data is NULL\n", __func__);
		return -ENODEV;
	}
	g_client = p_cmc623_data->client;

	if ((g_client == NULL)) {
		pr_err("%s g_client is NULL\n", __func__);
		return -ENODEV;
	}

	if (!g_client->adapter) {
		pr_err("%s g_client->adapter is NULL\n", __func__);
		return -ENODEV;
	}

	if (regaddr == 0x0001) {
		*val = last_cmc623_Algorithm;
		return 0;
	}

	msg[0].addr   = g_client->addr;
	msg[0].flags  = I2C_M_WR;
	msg[0].len	  = 1;
	msg[0].buf	  = &regaddr;
	msg[1].addr   = g_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len	 = 2;
	msg[1].buf	 = &data[0];
	err = i2c_transfer(g_client->adapter, &msg[0], 2);

	if (err >= 0) {
		*val = (data[0]<<8) | data[1];
		return 0;
	}
	/* add by inter.park */
	pr_err("%s %d i2c transfer error: %d\n",
			__func__, __LINE__, err);

	return err;
}


/*#####################################################
 *  	CMC623 Backlight Control function
  #####################################################
*/

/* value: 0 ~ 1600*/
static void cmc623_cabc_pwm_brightness_reg(int value)
{
	int reg;
	unsigned char *p_plut;
	u16 min_duty;

	if (!p_cmc623_data) {
		pr_err("%s cmc623 is not initialized\n", __func__);
		return;
	}
    
    if (cmc623_state.scenario == mDNIe_VIDEO_MODE) {
        p_plut = cmc623_Power_LUT[1];
        pr_debug("[CMC623:Info] : %s : video mode PLUT : %d\n",__func__,p_plut[0]);
    }
    else { 
	    p_plut = cmc623_Power_LUT[0];
        pr_debug("[CMC623:Info] : %s : normal mode PLUT : %d\n",__func__,p_plut[0]);
    }
	min_duty = p_plut[7] * value / 100;
	if (min_duty < 4) {
		reg = 0xc000 | (max(1, (value*p_plut[3]/100)));
	} 
    else {
		 /*PowerLUT*/
		cmc623_I2cWrite16(0x76, (p_plut[0] * value / 100) << 8
				| (p_plut[1] * value / 100));
		cmc623_I2cWrite16(0x77, (p_plut[2] * value / 100) << 8
				| (p_plut[3] * value / 100));
		cmc623_I2cWrite16(0x78, (p_plut[4] * value / 100) << 8
				| (p_plut[5] * value / 100));
		cmc623_I2cWrite16(0x79, (p_plut[6] * value / 100) << 8
				| (p_plut[7] * value / 100));
		cmc623_I2cWrite16(0x7a, (p_plut[8] * value / 100) << 8);
		reg = 0x5000 | (value<<4);
	}
	cmc623_I2cWrite16(0xB4, reg);
}

int Islcdonoff(void)
{
//	return 0;
	return cmc623_state.suspended; //tmps solution
}

/*value: 0 ~ 100*/
static void cmc623_cabc_pwm_brightness(int value)
{
	pr_debug("[CMC623:Info] : %s : value : %d\n",__func__,value);
	if (!p_cmc623_data) {
		pr_err("%s cmc623 is not initialized\n", __func__);
		return;
	}
	cmc623_I2cWrite16(0x00, 0x0000);
	cmc623_cabc_pwm_brightness_reg(value);
	cmc623_I2cWrite16(0x28, 0x0000);
}

/*value: 0 ~ 100*/
static void cmc623_manual_pwm_brightness(int value)
{
	int reg;

	if (!p_cmc623_data) {
		pr_err("%s cmc623 is not initialized\n", __func__);
		return;
	}

	reg = 0x4000 | (value<<4);

	cmc623_I2cWrite16(0x00, 0x0000);
	cmc623_I2cWrite16(0xB4, reg);
	cmc623_I2cWrite16(0x28, 0x0000);

}

/* value: 0 ~ 1600*/
void cmc623_pwm_brightness(int value)
{
	int data;

	pr_debug("[CMC623:Info] : %s : value : %d\n",__func__,value);
	if (value < 0)
		data = 0;
	else if (value > 1600)
		data = 1600;
	else
		data = value;

	if (data < 16)
		data = 1; /*Range of data 0~1600, min value 0~15 is same as 0*/
	else
		data = data >> 4;
    
    cmc623_cabc_pwm_brightness(data);

}

void cmc623_pwm_brightness_bypass(int value)
{
		int data;
		pr_debug("%s : BL brightness level = %d\n", __func__, value);

		if(value<0)
				data = 0;
		else if(value>1600)
				data = 1600;
		else
				data = value;

		if (data < 16)
				data = 1; /* Range of data 0~1600, min value 0~15 is same as 0*/
		else
				data = data >> 4;

		cmc623_state.brightness = data;
		cmc623_manual_pwm_brightness(data);
}

#define VALUE_FOR_1600	16
void set_backlight_pwm(int level)
{

	mutex_lock(&tuning_mutex);
	if (!Islcdonoff()) {
		if(cmc623_state.main_tune == NULL) {
				/*
				 * struct cmc623_state_type cmc623_state = {
				 * .cabc_mode = CABC_ON_MODE,
				 * .brightness = 42,
				 * .suspended = 0,
				 * .scenario = mDNIe_UI_MODE,
				 * .background = STANDARD_MODE,
				 * .temperature = TEMP_STANDARD,
				 * .ove = OUTDOOR_OFF_MODE,
				 * .sub_tune = NULL,
				 * .main_tune = NULL,
				 *};
				 */
			printk("======================================================\n");
			printk("[CMC623] cmc623_state.main_tune is NULL?...\n");
			printk("[CMC623] DUMP :\n");
			printk("[CMC623] cabc_mode : %d\n",cmc623_state.cabc_mode);
			printk("[CMC623] brightness : %d\n",cmc623_state.brightness);
			printk("[CMC623] suspended : %d\n",cmc623_state.suspended);
			printk("[CMC623] scenario : %d\n",cmc623_state.scenario);
			printk("[CMC623] background : %d\n",cmc623_state.background);
			printk("[CMC623] temperature : %d\n",cmc623_state.temperature);
			printk("[CMC623] ove : %d\n",cmc623_state.ove);
//			printk("[CMC623] sub_tune : %d\n",cmc623_state.sub_tune);
//			printk("[CMC623] main_tune : %d\n",cmc623_state.main_tune);
			printk("======================================================\n");
			mutex_unlock(&tuning_mutex);
			return;
		}
		if(cmc623_state.cabc_mode == CABC_OFF_MODE || 
						(cmc623_state.main_tune->flag & TUNE_FLAG_CABC_ALWAYS_OFF))
			cmc623_manual_pwm_brightness(level);		
		else 
			cmc623_pwm_brightness(level * VALUE_FOR_1600);
	}
	mutex_unlock(&tuning_mutex);
}

/*#####################################################
 *  	CMC623 common function
 *
 * void bypass_onoff_ctrl(int value);
 * void cabc_onoff_ctrl(int value);
 * int set_mdnie_scenario_mode(unsigned int mode);
 * int load_tuning_data(char *filename);
 * int apply_main_tune_value(enum eLcd_mDNIe_UI ui, enum eBackground_Mode bg, enum eCabc_Mode cabc, int force);
 * int apply_sub_tune_value(enum eCurrent_Temp temp, enum eOutdoor_Mode ove, enum eCabc_Mode cabc, int force);
  #####################################################
*/

void bypass_onoff_ctrl(int value)
{
	if(value == 1) {
		int i = 0;
		int num = ARRAY_SIZE(cmc623_bypass);

		for (i=0; i<num; i++)
			cmc623_I2cWrite16(cmc623_bypass[i].RegAddr, cmc623_bypass[i].Data);

		cmc623_pwm_brightness_bypass(current_gamma_level);
	} else if(value == 0) {
		cabc_onoff_ctrl(current_cabc_onoff);
		cmc623_pwm_brightness(current_gamma_level);
	}

}

void cabc_onoff_ctrl(int value)
{

    if (apply_main_tune_value(cmc623_state.scenario, cmc623_state.background, value, 0) != 0){
        pr_debug("[CMC623:ERROR]:%s: apply main tune value faile \n",__func__);
        return ;
    }
}

static int cmc623_set_tune_value(const struct Cmc623RegisterSet *value, int array_size)
{
    int ret = 0; 
    unsigned int num;
	unsigned int i = 0;

    mutex_lock(&tuning_mutex);    

    num = array_size;

	for (i = 0; i < num; i++) {
        ret = cmc623_I2cWrite16(value[i].RegAddr,value[i].Data);
        if (ret != 0) {
            pr_debug("[CMC623:ERROR]:cmc623_I2cWrite16 failed : %d\n",ret);
            goto set_error;
        }
    }
    
set_error:     
    mutex_unlock(&tuning_mutex);
    return ret;
}


static struct Cmc623RegisterSet Cmc623_TuneSeq[CMC623_MAX_SETTINGS];
static int parse_text(char *src, int len)
{
	int i,count, ret;
	int index=0;
	char *str_line[CMC623_MAX_SETTINGS];
	char *sstart;
	char *c;
	unsigned int data1, data2;

	c = src;
	count = 0;
	sstart = c;

	for(i=0; i<len; i++,c++){
		char a = *c;
		if(a=='\r' || a=='\n'){
			if(c > sstart){
				str_line[count] = sstart;
				count++;
			}
			*c='\0';
			sstart = c+1;
		}
	}

	if(c > sstart){
		str_line[count] = sstart;
		count++;
	}


	for(i=0; i<count; i++){
		ret = sscanf(str_line[i], "0x%x,0x%x\n", &data1, &data2);
		pr_debug("Result => [0x%2x 0x%4x] %s\n", data1, data2, (ret==2)?"Ok":"Not available");
		if(ret == 2) {   
			Cmc623_TuneSeq[index].RegAddr = (unsigned char)data1;
			Cmc623_TuneSeq[index++].Data  = (unsigned long)data2;
		}
	}
	return index;
}


bool cmc623_tune(unsigned long num) 
{
	unsigned int i;

    pr_debug("[CMC623:INFO] Start tuning CMC623\n");

	for (i=0; i<num; i++) {
        pr_debug("[CMC623:Tuning][%2d] : reg : 0x%2x, data: 0x%4x\n",i+1, Cmc623_TuneSeq[i].RegAddr, Cmc623_TuneSeq[i].Data);

		if (cmc623_I2cWrite16(Cmc623_TuneSeq[i].RegAddr, Cmc623_TuneSeq[i].Data) != 0) {
			pr_err("[CMC623:ERROR] : I2CWrite failed\n");
			return 0;
		}
        pr_debug("[CMC623:Tunig] : Write Done\n");        

		if (Cmc623_TuneSeq[i].RegAddr == CMC623_REG_SWRESET && Cmc623_TuneSeq[i].Data == 0xffff ){
			mdelay(3);
		}
	}
	pr_debug("[CMC623:INFO] End tuning CMC623\n");
	return 1;
}

int load_tuning_data(char *filename)
{
    struct file *filp;
	char	*dp;
	long	l;
	loff_t  pos;
	int     ret, num;
	mm_segment_t fs;

	pr_debug("[CMC623:INFO]:%s called loading file name : %s\n",__func__,filename);

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if(IS_ERR(filp)) 
	{
		pr_debug("[CMC623:ERROR]:File open failed\n");
		return -1;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	pr_debug("[CMC623:INFO]: Loading File Size : %ld(bytes)", l);

	dp = kmalloc(l+10, GFP_KERNEL);	
	if(dp == NULL){
		pr_debug("[CMC623:ERROR]:Can't not alloc memory for tuning file load\n");
		filp_close(filp, current->files);
		return -1;
	}
	pos = 0;
	memset(dp, 0, l);
	pr_debug("[CMC623:INFO] : before vfs_read()\n");
	ret = vfs_read(filp, (char __user *)dp, l, &pos);   
	pr_debug("[CMC623:INFO] : after vfs_read()\n");

	if(ret != l) {
        pr_debug("[CMC623:ERROR] : vfs_read() filed ret : %d\n",ret);
        kfree(dp);
		filp_close(filp, current->files);
		return -1;
	}

	filp_close(filp, current->files);

	set_fs(fs);
	num = parse_text(dp, l);

	if(!num) {
		pr_debug("[CMC623:ERROR]:Nothing to parse\n");
		kfree(dp);
		return -1;
	}

	pr_debug("[CMC623:INFO] : Loading Tuning Value's Count : %d", num);
	cmc623_tune(num);


	kfree(dp);
	return num;
}

int apply_sub_tune_value(enum eCurrent_Temp temp, enum eOutdoor_Mode ove, enum eCabc_Mode cabc, int force)
{

	int register_size;
#if 0
    if(tuning_enable){
        pr_debug("[CMC623:INFO]:%s:Tuning mode Enabled\n",__func__);
        return 0;
    }
#endif
    
	if (force == 0) {
        if((cmc623_state.temperature == temp) && (cmc623_state.ove == ove)) {
            pr_debug("[CMC623:INFO]:%s:already setted temp : %d, over : %d\n",__func__ ,temp ,ove);
            return 0; 
        }
    }
	pr_debug("=================================================\n");
	pr_debug(" CMC623 Mode Change.sub tune\n");
	pr_debug("==================================================\n");
    pr_debug("[CMC623:INFO]:%s:curr sub tune : %s\n",__func__,sub_tune_value[cmc623_state.temperature][cmc623_state.ove].value[cmc623_state.cabc_mode].name);
    pr_debug("[CMC623:INFO]:%s: sub tune : %s\n",__func__,sub_tune_value[temp][ove].value[cabc].name);

    if((ove == OUTDOOR_OFF_MODE) || (temp == TEMP_STANDARD)) {
        pr_debug("[CMC623:INFO]:%s:set default main tune\n",__func__);
		register_size = tune_value[cmc623_state.background][cmc623_state.scenario].value[cmc623_state.cabc_mode].size;       
        if (cmc623_set_tune_value(tune_value[cmc623_state.background][cmc623_state.scenario].value[cmc623_state.cabc_mode].tune,register_size) != 0){
            pr_debug("[CMC623:ERROR]:%s: set tune value falied \n",__func__);
            return -1; 
        	}
        
        if((ove != OUTDOOR_OFF_MODE) || (temp != TEMP_STANDARD)) {
            goto set_sub_tune;
       	}			
        
    }
    else {
set_sub_tune :         
		  register_size = sub_tune_value[temp][ove].value[cabc].size;
		  if(cmc623_set_tune_value(sub_tune_value[temp][ove].value[cabc].value,register_size)) {
				pr_debug("[CMC623:ERROR]:%s: set tune value falied \n",__func__);   
				return -1;
		  }
	}
    cmc623_state.temperature = temp; 
    cmc623_state.ove = ove;
    return 0;
}

int apply_main_tune_value(enum eLcd_mDNIe_UI ui, enum eBackground_Mode bg, enum eCabc_Mode cabc, int force)
{
    enum eCurrent_Temp temp = 0;

	pr_debug("==================================================\n");
	pr_debug(" CMC623 Mode Change. Main tune\n");
	pr_debug("==================================================\n");

    if (force == 0) {
        if((cmc623_state.scenario == ui) && (cmc623_state.background == bg) && (cmc623_state.cabc_mode == cabc)) {
            pr_debug("[CMC623:INFO]:%s:already setted ui : %d, bg : %d\n",__func__ ,ui ,bg);
            return 0; 
        }
    }
    pr_debug("[CMC623:INFO]:%s:curr main tune : %s\n",__func__, tune_value[cmc623_state.background][cmc623_state.scenario].value[cmc623_state.cabc_mode].name);
    pr_debug("[CMC623:INFO]:%s: main tune : %s\n",__func__,tune_value[bg][ui].value[cabc].name);

	if ((ui == mDNIe_VIDEO_WARM_MODE) || (ui == mDNIe_VIDEO_COLD_MODE)){
		if((ui == mDNIe_VIDEO_MODE)|| (ui == mDNIe_DMB_MODE)) {
			
			if (ui == mDNIe_VIDEO_WARM_MODE) {
					temp = TEMP_WARM;
				}
			else if (ui == mDNIe_VIDEO_COLD_MODE){
				temp = TEMP_COLD;
			}
			else {
				temp = TEMP_STANDARD;
			}
			if (apply_sub_tune_value(temp, cmc623_state.ove, cabc, 0) != 0){
				pr_debug("[CMC623:ERROR]:%s:apply_sub_tune_value() faield \n",__func__);
			}
			goto rest_set;
			}
	}		

	pr_debug("[CMC623:INFO]:%s set, size : %d\n",tune_value[bg][ui].value[cabc].name,tune_value[bg][ui].value[cabc].size);
    if (cmc623_set_tune_value(tune_value[bg][ui].value[cabc].tune,tune_value[bg][ui].value[cabc].size) != 0){
        pr_err("[CMC623:ERROR]:%s: set tune value falied \n",__func__);
        return -1; 
    }


     if ((ui == mDNIe_VIDEO_WARM_MODE) || (ui == mDNIe_VIDEO_COLD_MODE) || (ui == mDNIe_VIDEO_MODE)|| (ui == mDNIe_DMB_MODE)) {
        
        if (ui == mDNIe_VIDEO_WARM_MODE) {
                temp = TEMP_WARM;
            }
        else if (ui == mDNIe_VIDEO_COLD_MODE){
            temp = TEMP_COLD;
        }
        else {
            temp = TEMP_STANDARD;
        }
        if (apply_sub_tune_value(temp, cmc623_state.ove, cabc, 0) != 0){
            pr_debug("[CMC623:ERROR]:%s:apply_sub_tune_value() faield \n",__func__);
        }
     }       
        
rest_set:    
    
    cmc623_state.scenario = ui;
    cmc623_state.background = bg;
    cmc623_state.cabc_mode = cabc;
    cmc623_state.main_tune = &tune_value[bg][ui].value[cabc];

   if(ui == mDNIe_UI_MODE) {
        cmc623_state.temperature = TEMP_STANDARD;
        cmc623_state.ove = OUTDOOR_OFF_MODE;
    }
    
    if (ui == mDNIe_VIDEO_WARM_MODE) 
        cmc623_state.temperature = TEMP_WARM;
    else if (ui == mDNIe_VIDEO_COLD_MODE)
        cmc623_state.temperature = TEMP_COLD;

	set_backlight_pwm(cmc623_state.brightness);
    
	return 0;
}

/* ################# END of SYSFS Function ############################# */


static bool CMC623_SetUp(void)
{
	int i = 0;
	int num = ARRAY_SIZE(CMC623_INITSEQ);
	/*pr_err("%s num is %d\n", __func__, num);*/

	for (i = 0; i < num; i++) {
		if (cmc623_I2cWrite16(CMC623_INITSEQ[i].RegAddr,
		CMC623_INITSEQ[i].Data)) {
			pr_err("why return false??!!!\n");
			return FALSE;
		}
		if (CMC623_INITSEQ[i].RegAddr == CMC623_REG_SWRESET &&
		CMC623_INITSEQ[i].Data == 0xffff)
			msleep(3);
		}
		return TRUE;
}

int p5lte_cmc623_on(int enable)
{

	int ret;

	pr_debug("[LCD] %s:enable:%d\n", __FUNCTION__, enable);
	if(!v_ima_1_8v || !display_3_3v ) {
		pr_debug("PM regulator get failed.\n");
		return -1;
	}

	if (enable) {
		#if defined(CONFIG_KOR_OPERATOR_SKT)
		gpio_set_value(CMC623_nRST, 1);
		ret = gpio_get_value(CMC623_nRST);
		pr_debug("%s, CMC623_nRST : %d\n",__func__,ret);
		udelay(20);
		#endif
			/*  LCD_PWR_EN GPIO set to HIGH for V_IMA_1.2V on */
			pr_debug("LCD PM POWER ON!!\n");
			gpio_set_value(LCD_PWR_EN, 1);
			ret = gpio_get_value(LCD_PWR_EN);
			pr_debug("%s, LCD_PWR_EN : %d\n",__func__,ret);
			
			#if defined(CONFIG_KOR_OPERATOR_SKT)
			udelay(10);
			#else
			udelay(20);
			#endif

        	ret = regulator_enable(display_3_3v);
        	if (ret) {
        		pr_debug("%s: error enabling regulator\n", __func__);
				return -1;
        	}

        	ret = regulator_enable(v_ima_1_8v);
        	if (ret) {
        		pr_debug("%s: error enabling regulator\n", __func__);
				return -1;
        	} 
			#if defined(CONFIG_KOR_OPERATOR_SKT)
			gpio_set_value(MLCD_ON, 1);
			ret = gpio_get_value(MLCD_ON);
			pr_debug("%s, MLCD_ON : %d\n",__func__,ret);
			udelay(200);
			#endif
	
			/*  CMC623 GPIO up */
			gpio_tlmm_config(GPIO_CFG(CMC623_SDA,  0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),GPIO_CFG_ENABLE);
			gpio_tlmm_config(GPIO_CFG(CMC623_SCL,  0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),GPIO_CFG_ENABLE);
	
			is_cmc623_on = 1;
#if BYPASS_MODE_ON
			p5lte_cmc623_bypass_mode();
#else // BYPASS_MODE_OFF
			p5lte_cmc623_normal_mode();
			mdelay(1);
			CMC623_SetUp();
			cmc623_state.suspended = 0;
			msleep(10);
			apply_main_tune_value(cmc623_state.scenario, cmc623_state.background, cmc623_state.cabc_mode, 1);

#if (defined(CONFIG_TARGET_SERIES_P5LTE) || defined(CONFIG_TARGET_SERIES_P8LTE)) && defined(CONFIG_TARGET_LOCALE_EUR)
			if ( cmc623_state.brightness == 0 )
			{
				if(cmc623_state.cabc_mode == CABC_OFF_MODE ||(cmc623_state.main_tune->flag & TUNE_FLAG_CABC_ALWAYS_OFF))
				{
					cmc623_pwm_brightness(cmc623_state.brightness);
				}
			}
#endif

//			set_backlight_pwm(cmc623_state.brightness);	
#endif
			msleep(5);

	
			ret = p5lte_lvds_on(enable);
			if ( ret < 0) {
					pr_debug("LVDS turn on ERROR\n");
					return ret;
			}

			msleep(300); /*  H/W limitation. */	

			ret = p5lte_backlight_en(enable);
			if (ret<0) {
					pr_debug("BACKLIGHT turn on ERROR\n");
					return ret;
			}


	} else {
			pr_debug("LCD PM POWER OFF!!\n");
			
			/* sleep seq 
			 * 1. Backlight off
			 * 2. CMC623 GPIO off
			 * 3. display power down
   			 * 4. CMC623 I2C GPIO down
 			 * 5. LCD_PWR_EN off
			 * 6. LVDS off
			*/
			ret = p5lte_backlight_en(enable);
			if (ret<0) {
					pr_debug("BACKLIGHT turn on ERROR\n");
					return ret;
			}

			ret = gpio_get_value(CMC623_FAILSAFE);
			if(ret)
				gpio_set_value(CMC623_FAILSAFE, 0);
			udelay(20);
			ret = gpio_get_value(CMC623_BYPASS);
			if(ret)
				gpio_set_value(CMC623_BYPASS, 0);
			udelay(20);

			ret = gpio_get_value(CMC623_SLEEP);
			if(ret)
				gpio_set_value(CMC623_SLEEP, 0);
			udelay(20);

			ret = gpio_get_value(CMC623_nRST);
			if(ret)
				gpio_set_value(CMC623_nRST, 0);
			msleep(4);

			#if defined(CONFIG_KOR_OPERATOR_SKT)
			gpio_set_value(MLCD_ON, 0);
			ret = gpio_get_value(MLCD_ON);
			pr_debug("%s, MLCD_ON : %d\n",__func__,ret);
			#endif
			
        	ret = regulator_disable(v_ima_1_8v);
        	if (ret) {
        		pr_debug("%s: error disabling regulator\n", __func__);
				return -1;
        	}
        	ret = regulator_disable(display_3_3v);
        	if (ret) {
        		pr_debug("%s: error disabling regulator\n", __func__);
				return -1;
        	} 
			is_cmc623_on = 0;
			/*  CMC623 GPIO down */
			gpio_tlmm_config(GPIO_CFG(CMC623_SDA,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),GPIO_CFG_DISABLE);
			gpio_tlmm_config(GPIO_CFG(CMC623_SCL,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),GPIO_CFG_DISABLE);

			gpio_set_value(LCD_PWR_EN, 0);
			ret = gpio_get_value(LCD_PWR_EN);
			pr_debug("%s, LCD_PWR_EN : %d\n",__func__,ret);
			cmc623_state.suspended = 1;
#if defined (CONFIG_TARGET_SERIES_P4LTE)
			msleep(500); /* This is H/W Limitation (about LVDS) */
#else
			msleep(200); /* This is H/W Limitation (about LVDS) */
#endif


    }

	return ret;

}

int p5lte_cmc623_bypass_mode(void)
{
		int ret;
		udelay(20);
		gpio_set_value(CMC623_FAILSAFE, 1);
		ret = gpio_get_value(CMC623_FAILSAFE);
		pr_debug("%s, CMC623_FAILSAFE : %d\n",__func__,ret);
		udelay(20);
		ret = gpio_get_value(CMC623_BYPASS);
		pr_debug("%s, CMC623_BYPASS : %d\n",__func__,ret);
		udelay(20);
		gpio_set_value(CMC623_SLEEP, 1);
		ret = gpio_get_value(CMC623_SLEEP);
		pr_debug("%s, CMC623_SLEEP : %d\n",__func__,ret);
		udelay(20);

		gpio_set_value(CMC623_nRST, 0);
		ret = gpio_get_value(CMC623_nRST);
		pr_debug("%s, CMC623_nRST : %d\n",__func__,ret);
		msleep(4);
		gpio_set_value(CMC623_nRST, 1);
		ret = gpio_get_value(CMC623_nRST);
		pr_debug("%s, CMC623_nRST : %d\n",__func__,ret);

		return ret;

}

int p5lte_cmc623_normal_mode(void)
{
		int ret;
		udelay(20);
		gpio_set_value(CMC623_FAILSAFE, 1);
		ret = gpio_get_value(CMC623_FAILSAFE);
		pr_debug("%s, CMC623_FAILSAFE : %d\n",__func__,ret);
		udelay(20);
		gpio_set_value(CMC623_BYPASS, 1);
		ret = gpio_get_value(CMC623_BYPASS);
		pr_debug("%s, CMC623_BYPASS : %d\n",__func__,ret);
		udelay(20);

		gpio_set_value(CMC623_SLEEP, 1);
		ret = gpio_get_value(CMC623_SLEEP);
		pr_debug("%s, CMC623_SLEEP : %d\n",__func__,ret);
		udelay(20);

		gpio_set_value(CMC623_nRST, 0);
		ret = gpio_get_value(CMC623_nRST);
		pr_debug("%s, CMC623_nRST : %d\n",__func__,ret);
		msleep(4);
		gpio_set_value(CMC623_nRST, 1);
		ret = gpio_get_value(CMC623_nRST);
		pr_debug("%s, CMC623_nRST : %d\n",__func__,ret);

		return ret;

}

/*  Platform Range : 0 ~ 255
 *  CMC623   Range : 0 ~ 100 
 *  User Platform Range : 
 *   - MIN : 30
 *   - MAX : 255
 *  if under 30, CMC623 val : 2%
 *  if 30, CMC623 val : 3%
 *  if default, CMC623 val : 49%
 *  if 255, CMC623 val : 100%
 */

#define DIMMING_BRIGHTNESS_VALUE    20
#define MINIMUM_BRIGHTNESS_VALUE    30
#define MAXIMUM_BRIGHTNESS_VALUE    255
#define DEFAULT_BRIGHTNESS_VALUE    144
#define QUATER_BRIGHTNESS_VALUE     87
 
#define DIMMING_CMC623_VALUE    1
#define MINIMUM_CMC623_VALUE    1   /*  0% progress bar */
#define MAXIMUM_CMC623_VALUE    100 /*  100% progress bar */
#define DEFAULT_CMC623_VALUE    49 /* 50% progress bar */
#define QUATER_CMC623_VALUE     9 /*  25% progress bar */
 
#define QUATER_DEFAUT_MAGIC_VALUE   3
#define UNDER_DEFAULT_MAGIC_VALUE   52
#define OVER_DEFAULT_MAGIC_VALUE    17

#if defined (CONFIG_TARGET_SERIES_P4LTE)
void cmc623_manual_brightness(int bl_lvl)
{
	/*  Platform Range : 0 ~ 255
	 *  CMC623   Range : 0 ~ 100 
	 *  User Platform Range : 
	 *   - MIN : 30
	 *   - MAX : 255
	 *
	 *  if 30, CMC623 val : 5
	 *  if 255, CMC623 val : 100
	 *  */
	int value;

	if(bl_lvl < 30) {
		value = bl_lvl / 6;
	} else if(bl_lvl <= 255) {
		value = ((95 * bl_lvl) / 225 ) - 7;
	} else {
		pr_debug("[CMC623] Wrong Backlight scope!!!!!! [%d] \n",bl_lvl);
		value = 50;
	}

	if(value < 0 || value > 100) {
		pr_debug("[CMC623] Wrong value scope [%d] \n",value);
	}
	pr_debug("[CMC623] BL : %d Value : %d\n",bl_lvl, value);
	set_backlight_pwm(value);
	cmc623_state.brightness = value;	
}

#else
void cmc623_manual_brightness(int bl_lvl)
{
	int value;

     if(bl_lvl < MINIMUM_BRIGHTNESS_VALUE) {
        if(bl_lvl == 0)
            value = 0;
        else
            value = DIMMING_CMC623_VALUE;
    } else if(bl_lvl <= QUATER_BRIGHTNESS_VALUE) {
        value = ((QUATER_CMC623_VALUE-MINIMUM_CMC623_VALUE) * bl_lvl / (QUATER_BRIGHTNESS_VALUE - MINIMUM_BRIGHTNESS_VALUE)) - QUATER_DEFAUT_MAGIC_VALUE;
    } else if(bl_lvl <= DEFAULT_BRIGHTNESS_VALUE) {
        value = ((DEFAULT_CMC623_VALUE-QUATER_CMC623_VALUE) * bl_lvl / (DEFAULT_BRIGHTNESS_VALUE - QUATER_BRIGHTNESS_VALUE)) - UNDER_DEFAULT_MAGIC_VALUE;
    } else if(bl_lvl <= MAXIMUM_BRIGHTNESS_VALUE) {
        value = ((MAXIMUM_CMC623_VALUE-DEFAULT_CMC623_VALUE) * bl_lvl / (MAXIMUM_BRIGHTNESS_VALUE - DEFAULT_BRIGHTNESS_VALUE)) - OVER_DEFAULT_MAGIC_VALUE;
    } else {
        pr_debug("[CMC623] Wrong Backlight scope!!!!!! [%d] \n",bl_lvl);
        value = 50;
    }

	if(value < 0 || value > 100) {
		pr_debug("[CMC623] Wrong value scope [%d] \n",value);
		value = 50;
	}

	pr_debug("[CMC623] BL : %d Value : %d\n",bl_lvl, value);
	set_backlight_pwm(value);
	cmc623_state.brightness = value;
}
#endif

#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU)
void cmc623_Set_Region_Ext(int enable, int hStart, int hEnd, int vStart, int vEnd)
{
	u16 data=0;

	mutex_lock(&tuning_mutex);  

	cmc623_I2cWrite16(0x0000,0x0000);
	cmc623_I2cRead16(0x0001, &data);

	data &= 0x00ff;

	if(enable)
	{
//		cmc623_I2cWrite16_Region(0x0001,0x0300 | data);
		cmc623_I2cWrite16(0x0001,0x0700 | data);
		
		cmc623_I2cWrite16(0x0002,hStart);
		cmc623_I2cWrite16(0x0003,hEnd);
		cmc623_I2cWrite16(0x0004,vStart);
		cmc623_I2cWrite16(0x0005,vEnd);
	}
	else
	{
		cmc623_I2cWrite16(0x0001,0x0000 | data);
	}
	cmc623_I2cWrite16(0x0028,0x0000);

	mutex_unlock(&tuning_mutex);
	
	cmc623_current_region_enable = enable;
}
EXPORT_SYMBOL(cmc623_Set_Region_Ext);

#endif

int p5lte_gpio_init(void)
{
	int status;

	struct pm_gpio lvds_cfg = {
				.direction      = PM_GPIO_DIR_OUT,
				.pull           = PM_GPIO_PULL_NO,
				.vin_sel        = 2,
				.function       = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol    = 0,
	};

	struct pm_gpio backlight_cfg = {
				.direction      = PM_GPIO_DIR_OUT,
				.pull           = PM_GPIO_PULL_NO,
				.vin_sel        = 2,
				.function       = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol    = 0,

	};

	if(pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_LVDS_nSHDN),&lvds_cfg)) {
		pr_err("%s PMIC_GPIO_LVDS_nSHDN config failed\n", __func__);
	}
	status = gpio_request(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_LVDS_nSHDN), "LVDS_nSHDN");
	if (status) {
			pr_debug(KERN_ERR "%s: LVS_nSHDN gpio"
							" %d request failed\n", __func__,
							PMIC_GPIO_LVDS_nSHDN);
	}


	if(pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_BACKLIGHT_RST),&backlight_cfg)) {
		pr_err("%s PMIC_GPIO_BACKLIGHT_RST config failed\n", __func__);
	}
	status = gpio_request(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_BACKLIGHT_RST), "BACKLIGHT_EN");
	if (status) {
			pr_debug(KERN_ERR "%s: BACKLIGHT_EN gpio"
							" %d request failed\n", __func__,
							PMIC_GPIO_BACKLIGHT_RST);
	}

	return status;

}
int p5lte_lvds_on(int enable)
{
	int status;


	if(enable)
		status = gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_LVDS_nSHDN),PMIC_GPIO_LCD_LEVEL_HIGH);
	else
		status = gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_LVDS_nSHDN),PMIC_GPIO_LCD_LEVEL_LOW);

	status = gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_LVDS_nSHDN)); 
	
	pr_debug("gpio_get_value result. LVDS_nSHDN : %d\n",status);

	return (unsigned int) status;

}

int p5lte_backlight_en(int enable)
{
	int status;

	if(enable)
		status = gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_BACKLIGHT_RST),PMIC_GPIO_LCD_LEVEL_HIGH);
	else
		status = gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_BACKLIGHT_RST),PMIC_GPIO_LCD_LEVEL_LOW);
	status = gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_BACKLIGHT_RST)); 
	pr_debug("gpio_get_value. BACKLIGHT_EN : %d\n",status);

	return (unsigned int) status;
}

static int cmc623_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	struct cmc623_data *data;

	pr_debug("==============================\n");
	pr_debug("cmc623 attach START!!!\n");
	pr_debug("==============================\n");

	data = kzalloc(sizeof(struct cmc623_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	data->client = client;
	i2c_set_clientdata(client, data);

	dev_info(&client->dev, "cmc623 i2c probe success!!!\n");

	p_cmc623_data = data;

	pr_debug("==============================\n");
	pr_debug("LCD GPIO Init (LVDS, BACKLIGHT!!!\n");
	pr_debug("==============================\n");

	p5lte_gpio_init();

	pr_debug("==============================\n");
	pr_debug("CMC623 SYSFS INIT!!!\n");
	pr_debug("==============================\n");
	ret = cmc623_sysfs_init();
	if(ret < 0) {
		pr_debug("CMC623 sysfs initialize FAILED\n");
	}
	return 0;
}

static int __devexit cmc623_i2c_remove(struct i2c_client *client)
{
	struct cmc623_data *data = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);

	kfree(data);

	dev_info(&client->dev, "cmc623 i2c remove success!!!\n");

	return 0;
}

void cmc623_shutdown(struct i2c_client *client)
{
	pr_debug("-0- %s called -0-\n", __func__);
}

static const struct i2c_device_id cmc623[] = {
	{ "cmc623", 0 },
};
MODULE_DEVICE_TABLE(i2c, cmc623);

struct i2c_driver cmc623_i2c_driver = {
	.driver	= {
	.name	= "cmc623",
	.owner = THIS_MODULE,
	},
	.probe 		= cmc623_i2c_probe,
	.remove 	= __devexit_p(cmc623_i2c_remove),
	.id_table	= cmc623,
	.shutdown = cmc623_shutdown,
};

int p5lte_cmc623_init(void)
{
	int ret;

	/* register I2C driver for CMC623 */

	pr_debug("<cmc623_i2c_driver Add START>\n");
	ret = i2c_add_driver(&cmc623_i2c_driver);
	pr_debug("cmc623_init Return value  (%d)\n", ret);
	pr_debug("<cmc623_i2c_driver Add END>\n");


	pr_debug("%s\n",__func__);
	ret = gpio_request(CMC623_nRST, "CMC623_nRST");
	if(ret < 0) {
		pr_debug(KERN_ERR "%s CMC gpio %d request failed\n",__func__,CMC623_nRST);
	}
	ret = gpio_request(CMC623_SLEEP, "CMC623_SLEEP");
	if(ret < 0) {
		pr_debug(KERN_ERR "%s CMC gpio %d request failed\n",__func__,CMC623_SLEEP);
	}
	ret = gpio_request(CMC623_BYPASS, "CMC623_BYPASS");
	if(ret < 0) {
		pr_debug(KERN_ERR "%s CMC gpio %d request failed\n",__func__,CMC623_BYPASS);
	}
	ret = gpio_request(CMC623_FAILSAFE, "CMC623_FAILSAFE");
	if(ret < 0) {
		pr_debug(KERN_ERR "%s CMC gpio %d request failed\n",__func__,CMC623_FAILSAFE);
	}
	ret = gpio_request(LCD_PWR_EN, "LCD_PWR_EN");
	if(ret < 0) {
		pr_debug(KERN_ERR "%s CMC gpio %d request failed\n",__func__,LCD_PWR_EN);
	}
	#if defined(CONFIG_KOR_OPERATOR_SKT)
	ret = gpio_request(MLCD_ON, "MLCD_ON");
	if(ret < 0) {
		pr_debug(KERN_ERR "%s CMC gpio %d request failed\n",__func__,MLCD_ON);
	}
	ret = gpio_request(CMC623_SCL, "CMC623_SCL");
	if(ret < 0) {
		pr_debug(KERN_ERR "%s CMC gpio %d request failed\n",__func__,CMC623_SCL);
	}
	ret = gpio_request(CMC623_SDA, "CMC623_SDA");
	if(ret < 0) {
		pr_debug(KERN_ERR "%s CMC gpio %d request failed\n",__func__,CMC623_SDA);
	}
	#endif

	gpio_direction_output(CMC623_nRST, 1);
	gpio_direction_output(CMC623_SLEEP, 0);
	gpio_direction_output(CMC623_BYPASS, 0);
	gpio_direction_output(CMC623_FAILSAFE, 0);
	#if defined(CONFIG_KOR_OPERATOR_SKT)
	gpio_direction_output(MLCD_ON, 0);
	gpio_direction_output(CMC623_SDA, 0);
	gpio_direction_output(CMC623_SCL, 0);
	#endif
#if defined (CONFIG_TARGET_SERIES_P4LTE)
	gpio_direction_output(LCD_PWR_EN, 0); //check this!!!!
	msleep(500);
#else
	gpio_direction_output(LCD_PWR_EN, 0);
#endif


	if(v_ima_1_8v == NULL)
    {
			pr_debug("[CMC] PMIC8058 L3 get\n");
        	v_ima_1_8v = regulator_get(NULL, "8058_l3");
        	if (IS_ERR(v_ima_1_8v))
        		return -1;

        	ret = regulator_set_voltage(v_ima_1_8v, 1800000, 1800000);
        	if (ret) {
        		pr_debug("%s: error setting v_ima_1_8v voltage\n", __func__);
				return -1;
        	}
    }
            
    if(display_3_3v == NULL)
    {            
			pr_debug("[CMC] PMIC8058 L8 get\n");
        	display_3_3v = regulator_get(NULL, "8058_l8");
        	if (IS_ERR(display_3_3v))
        		return -1;

        	ret = regulator_set_voltage(display_3_3v, 3300000, 3300000);
        	if (ret) {
        		pr_debug("%s: error setting display_3_3v voltage\n", __func__);
				return -1;
        	}
    }


	return 0;
	
}



/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("P5LTE CMC623 image converter");
MODULE_LICENSE("GPL");
