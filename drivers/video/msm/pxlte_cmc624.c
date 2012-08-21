/*
 * =====================================================================================
 *
 *       Filename:  p8lte_cmc624.c
 *
 *    Description:  cmc624, lvds control driver
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
#define DEBUG			/* uncomment if you want debugging output */

#include <linux/delay.h>
#include <mach/gpio.h>
#include "msm_fb.h"
#include <linux/regulator/pmic8058-regulator.h>
#include <linux/regulator/pmic8901-regulator.h>
#include <linux/mfd/pmic8058.h>
#include "pxlte_cmc624.h"

/*
	 * V_IMA_1.8V		VREG_L3A
	 * DISPLAY_3.3V		VREG_L8A
	 * V_IMA_1.2V		GPIO LCD_PWR_EN 
	 * LCD_VOUT			GPIO LCD_PWR_EN
*/
static struct regulator *v_ima_1_8v = NULL;
static struct regulator *display_3_3v = NULL;
#define LCD_PWR_EN 70

struct cmc624_data {
	struct i2c_client *client;
};


static struct cmc624_data *p_cmc624_data;
static struct i2c_client *g_client;
#define I2C_M_WR 0 /* for i2c */
#define I2c_M_RD 1 /* for i2c */
unsigned long last_cmc624_Bank = 0xffff;
unsigned long last_cmc624_Algorithm = 0xffff;

static struct Cmc624RegisterSet Cmc624_TuneSeq[CMC624_MAX_SETTINGS];
static int Cmc624_TuneSeqLen = 0;

#define CMC624_BRIGHTNESS_MAX_LEVEL 1600
static int current_gamma_level = CMC624_BRIGHTNESS_MAX_LEVEL;
static int current_cabc_onoff = 1;

#if defined(CONFIG_TARGET_LOCALE_KOR_SKT) || defined(CONFIG_TARGET_LOCALE_KOR_LGU)
//static int cmc624_current_region_enable = false; //region mode added
#endif

struct cmc624_state_type cmc624_state = {
	.cabc_mode = CABC_OFF_MODE,
	.brightness = 42,
	.suspended = 0,
    .scenario = mDNIe_UI_MODE,
    .browser_scenario = COLOR_TONE_1,
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
unsigned char cmc624_Power_LUT[NUM_POWER_LUT][NUM_ITEM_POWER_LUT] = {
	{ 0x42, 0x47, 0x3E, 0x52, 0x42, 0x3F, 0x3A, 0x37, 0x3F },      	
    { 0x38, 0x3d, 0x34, 0x48, 0x38, 0x35, 0x30, 0x2d, 0x35 },
};

static int is_cmc624_on = 0;
/*  ###############################################################
 *  #
 *  #       TUNE VALUE
 *  #
 *  ############################################################### */
unsigned char cmc624_default_plut[NUM_ITEM_POWER_LUT] = {
     0x42, 0x47, 0x3E, 0x52, 0x42, 0x3F, 0x3A, 0x37, 0x3F      	    
};


unsigned char cmc624_video_plut[NUM_ITEM_POWER_LUT] = {
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
            .value[CABC_ON_MODE] = {.name = "STANDARD,OUTDOOR:ON,CABC:ON",    .value = ove_cabcoff, .size = ARRAY_SIZE(ove_cabcoff)}
        } 
    },
    {   
        {
            .value[CABC_OFF_MODE] = {.name = "WARM,OUTDOOR:OFF,CABC:OFF",      .value = warm_cabcoff, .size = ARRAY_SIZE(warm_cabcoff)},      
            .value[CABC_ON_MODE] = {.name = "WARM,OUTDOOR:OFF,CABC:ON",       .value = warm_cabcoff, .size = ARRAY_SIZE(warm_cabcoff)}
        },
        {
            .value[CABC_OFF_MODE] = {.name = "WARM,OUTDOOR:ON,CABC:OFF",       .value = warm_ove_cabcoff, .size = ARRAY_SIZE(warm_ove_cabcoff)},     
            .value[CABC_ON_MODE] = {.name = "WARM,OUTDOOR:ON,CABC:ON",        .value = warm_ove_cabcoff, .size = ARRAY_SIZE(warm_ove_cabcoff)}
        }
    },
    {   
        {
            .value[CABC_OFF_MODE] = {.name = "COLD,OUTDOOR:OFF,CABC:OFF",      .value = cold_cabcoff, .size = ARRAY_SIZE(cold_cabcoff)},      
            .value[CABC_ON_MODE] = {.name = "COLD,OUTDOOR:OFF,CABC:ON",       .value = cold_cabcoff, .size = ARRAY_SIZE(cold_cabcoff)}
    },
        {
            .value[CABC_OFF_MODE] = {.name = "COLD,OUTDOOR:ON,CABC:OFF",       .value = cold_ove_cabcoff, .size = ARRAY_SIZE(cold_ove_cabcoff)},     
            .value[CABC_ON_MODE] = {.name = "COLD,OUTDOOR:ON,CABC:ON",        .value = cold_ove_cabcoff, .size = ARRAY_SIZE(cold_ove_cabcoff)}
        }
    },
};

const struct str_main_tuning tune_value[MAX_BACKGROUND_MODE][MAX_mDNIe_MODE] = {
    
    {{.value[CABC_OFF_MODE] = {.name = "DYN_UI_OFF",        .flag = 0, .tune = dynamic_ui_cabcoff, .plut = NULL, .size = ARRAY_SIZE(dynamic_ui_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "DYN_UI_ON",         .flag = 0, .tune = dynamic_ui_cabcoff, .plut = NULL, .size = ARRAY_SIZE(dynamic_ui_cabcoff)}},
     {.value[CABC_OFF_MODE] = {.name = "DYN_VIDEO_OFF",     .flag = 0, .tune = dynamic_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(dynamic_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "DYN_VIDEO_ON",      .flag = 0, .tune = dynamic_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(dynamic_video_cabcoff)}},
     {.value[CABC_OFF_MODE] = {.name = "DYN_VIDEO_W_OFF",   .flag = 0, .tune = dynamic_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(dynamic_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "DYN_VIDEO_W_ON",    .flag = 0, .tune = dynamic_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(dynamic_video_cabcoff)}},
     {.value[CABC_OFF_MODE] = {.name = "DYN_VIDEO_C_OFF",   .flag = 0, .tune = dynamic_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(dynamic_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "DYN_VIDEO_C_ON",    .flag = 0, .tune = dynamic_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(dynamic_video_cabcoff)}},
     {.value[CABC_OFF_MODE] = {.name = "DYN_CAMERA_OFF",    .flag = TUNE_FLAG_CABC_ALWAYS_OFF, .tune = camera, .plut = NULL , .size = ARRAY_SIZE(camera)},
      .value[CABC_ON_MODE]  = {.name = "DYN_CAMERA_ON",     .flag = TUNE_FLAG_CABC_ALWAYS_OFF, .tune = camera, .plut = NULL , .size = ARRAY_SIZE(camera)}},
     {.value[CABC_OFF_MODE] = {.name = "DYN_NAVI_OFF",      .flag = 0, .tune = NULL, .plut = NULL , .size = 0},
      .value[CABC_ON_MODE]  = {.name = "DYN_NAVI_ON",       .flag = 0, .tune = NULL, .plut = NULL , .size = 0}},
     {.value[CABC_OFF_MODE] = {.name = "DYN_GALLERY_OFF",   .flag = 0, .tune = dynamic_gallery_cabcoff, .plut = NULL , .size = ARRAY_SIZE(dynamic_gallery_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "DYN_GALLERY_ON",    .flag = 0, .tune = dynamic_gallery_cabcoff, .plut = NULL , .size = ARRAY_SIZE(dynamic_gallery_cabcoff)}},
      {.value[CABC_OFF_MODE] = {.name = "DYN_DMB_OFF",     .flag = 0, .tune = dynamic_dmb_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(dynamic_dmb_cabcoff)},
    .value[CABC_ON_MODE]  = {.name = "DYN_DMB_ON",      .flag = 0, .tune = dynamic_dmb_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(dynamic_dmb_cabcoff)}}},
      
    {{.value[CABC_OFF_MODE] = {.name = "STD_UI_OFF",        .flag = 0, .tune = standard_ui_cabcoff, .plut = NULL , .size = ARRAY_SIZE(standard_ui_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "STD_UI_ON",         .flag = 0, .tune = standard_ui_cabcoff, .plut = NULL , .size = ARRAY_SIZE(standard_ui_cabcoff)}},
     {.value[CABC_OFF_MODE] = {.name = "STD_VIDEO_OFF",     .flag = 0, .tune = standard_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(standard_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "STD_VIDEO_ON",      .flag = 0, .tune = standard_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(standard_video_cabcoff)}},
     {.value[CABC_OFF_MODE] = {.name = "STD_VIDEO_W_OFF",   .flag = 0, .tune = standard_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(standard_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "STD_VIDEO_W_ON",    .flag = 0, .tune = standard_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(standard_video_cabcoff)}},
     {.value[CABC_OFF_MODE] = {.name = "STD_VIDEO_C_OFF",   .flag = 0, .tune = standard_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(standard_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "STD_VIDEO_C_ON",    .flag = 0, .tune = standard_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(standard_video_cabcoff) }},
     {.value[CABC_OFF_MODE] = {.name = "STD_CAMERA_OFF",    .flag = TUNE_FLAG_CABC_ALWAYS_OFF, .tune = camera, .plut = NULL , .size = ARRAY_SIZE(camera)},
      .value[CABC_ON_MODE]  = {.name = "STD_CAMERA_ON",     .flag = TUNE_FLAG_CABC_ALWAYS_OFF, .tune = camera, .plut = NULL , .size = ARRAY_SIZE(camera)}},
     {.value[CABC_OFF_MODE] = {.name = "STD_NAVI_OFF",      .flag = 0, .tune = NULL, .plut = NULL, .size = 0},
      .value[CABC_ON_MODE]  = {.name = "STD_NAVI_ON",       .flag = 0, .tune = NULL, .plut = NULL , .size = 0}},
     {.value[CABC_OFF_MODE] = {.name = "STD_GALLERY_OFF",   .flag = 0, .tune = standard_gallery_cabcoff, .plut = NULL , .size = ARRAY_SIZE(standard_gallery_cabcoff) },
      .value[CABC_ON_MODE]  = {.name = "STD_GALLERY_ON",    .flag = 0, .tune = standard_gallery_cabcoff, .plut = NULL , .size = ARRAY_SIZE(standard_gallery_cabcoff)}},
      {.value[CABC_OFF_MODE] = {.name = "STD_DMB_OFF",     .flag = 0, .tune = standard_dmb_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(standard_dmb_cabcoff)},
    .value[CABC_ON_MODE]  = {.name = "STD_DMB_ON",      .flag = 0, .tune = standard_dmb_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(standard_dmb_cabcoff)}}},
// natural mode  
  {{.value[CABC_OFF_MODE] = {.name = "NAT_UI_OFF",        .flag = 0, .tune = natural_ui_cabcoff, .plut = NULL , .size = ARRAY_SIZE(natural_ui_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "NAT_UI_ON",         .flag = 0, .tune =natural_ui_cabcoff, .plut = NULL , .size = ARRAY_SIZE(natural_ui_cabcoff)}},
     {.value[CABC_OFF_MODE] = {.name = "NAT_VIDEO_OFF",     .flag = 0, .tune = natural_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(natural_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "NAT_VIDEO_ON",      .flag = 0, .tune = natural_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(natural_video_cabcoff)}},
     {.value[CABC_OFF_MODE] = {.name = "NAT_VIDEO_W_OFF",   .flag = 0, .tune = natural_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(natural_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "NAT_VIDEO_W_ON",    .flag = 0, .tune = natural_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(natural_video_cabcoff)}},
     {.value[CABC_OFF_MODE] = {.name = "NAT_VIDEO_C_OFF",   .flag = 0, .tune = natural_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(natural_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "NAT_VIDEO_C_ON",    .flag = 0, .tune = natural_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(natural_video_cabcoff)}},
     {.value[CABC_OFF_MODE] = {.name = "NAT_CAMERA_OFF",    .flag = TUNE_FLAG_CABC_ALWAYS_OFF, .tune = camera, .plut = NULL , .size = ARRAY_SIZE(camera)},
      .value[CABC_ON_MODE]  = {.name = "NAT_CAMERA_ON",     .flag = TUNE_FLAG_CABC_ALWAYS_OFF, .tune = camera, .plut = NULL , .size = ARRAY_SIZE(camera)}},
     {.value[CABC_OFF_MODE] = {.name = "NAT_NAVI_OFF",      .flag = 0, .tune = NULL, .plut = NULL , .size = 0},
      .value[CABC_ON_MODE]  = {.name = "NAT_NAVI_ON",       .flag = 0, .tune = NULL, .plut = NULL , .size = 0}},
     {.value[CABC_OFF_MODE] = {.name = "NAT_GALLERY_OFF",   .flag = 0, .tune = natural_gallery_cabcoff, .plut = NULL , .size = ARRAY_SIZE(natural_gallery_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "NAT_GALLERY_ON",    .flag = 0, .tune = natural_gallery_cabcoff, .plut = NULL , .size = ARRAY_SIZE(natural_gallery_cabcoff)}},
      {.value[CABC_OFF_MODE] = {.name = "NAT_DMB_OFF",     .flag = 0, .tune = natural_dmb_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(natural_dmb_cabcoff)},
    .value[CABC_ON_MODE]  = {.name = "NAT_DMB_ON",      .flag = 0, .tune = natural_dmb_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(natural_dmb_cabcoff)}}}, 
     
    {{.value[CABC_OFF_MODE] = {.name = "MOV_UI_OFF",        .flag = 0, .tune = movie_ui_cabcoff, .plut = NULL , .size = ARRAY_SIZE(movie_ui_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "MOV_UI_ON",         .flag = 0, .tune = movie_ui_cabcoff, .plut = NULL , .size = ARRAY_SIZE(movie_ui_cabcoff)}},
     {.value[CABC_OFF_MODE] = {.name = "MOV_VIDEO_OFF",     .flag = 0, .tune = movie_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(movie_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "MOV_VIDEO_ON",      .flag = 0, .tune = movie_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(movie_video_cabcoff)}},
     {.value[CABC_OFF_MODE] = {.name = "MOV_VIDEO_W_OFF",   .flag = 0, .tune = movie_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(movie_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "MOV_VIDEO_W_ON",    .flag = 0, .tune = movie_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(movie_video_cabcoff)}},
     {.value[CABC_OFF_MODE] = {.name = "MOV_VIDEO_C_OFF",   .flag = 0, .tune = movie_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(movie_video_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "MOV_VIDEO_C_ON",    .flag = 0, .tune = movie_video_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(movie_video_cabcoff)}},
     {.value[CABC_OFF_MODE] = {.name = "MOV_CAMERA_OFF",    .flag = TUNE_FLAG_CABC_ALWAYS_OFF, .tune = camera, .plut = NULL , .size = ARRAY_SIZE(camera)},
      .value[CABC_ON_MODE]  = {.name = "MOV_CAMERA_ON",     .flag = TUNE_FLAG_CABC_ALWAYS_OFF, .tune = camera, .plut = NULL , .size = ARRAY_SIZE(camera)}},
     {.value[CABC_OFF_MODE] = {.name = "MOV_NAVI_OFF",      .flag = 0, .tune = NULL, .plut = NULL , .size = 0},
      .value[CABC_ON_MODE]  = {.name = "MOV_NAVI_ON",       .flag = 0, .tune = NULL, .plut = NULL , .size = 0}},
     {.value[CABC_OFF_MODE] = {.name = "MOV_GALLERY_OFF",   .flag = 0, .tune = movie_gallery_cabcoff, .plut = NULL , .size = ARRAY_SIZE(movie_gallery_cabcoff)},
      .value[CABC_ON_MODE]  = {.name = "MOV_GALLERY_ON",    .flag = 0, .tune = movie_gallery_cabcoff, .plut = NULL , .size = ARRAY_SIZE(movie_gallery_cabcoff)}},
      {.value[CABC_OFF_MODE] = {.name = "MOV_DMB_OFF",     .flag = 0, .tune = movie_dmb_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(movie_dmb_cabcoff)},
    .value[CABC_ON_MODE]  = {.name = "MOV_DMB_ON",      .flag = 0, .tune = movie_dmb_cabcoff, .plut = cmc624_video_plut , .size = ARRAY_SIZE(movie_dmb_cabcoff)}}},      

 };

const struct str_sub_tuning browser_tune_value[COLOR_TONE_MAX] = {
	// browser tone
        {
            .value[CABC_OFF_MODE] = {.name = "BROWSER_TONE1,CABC:OFF",  .value = browser_tone1_tune, .size = ARRAY_SIZE(browser_tone1_tune)}, 
            .value[CABC_ON_MODE] = {.name = "BROWSER_TONE1,CABC:ON",   .value = browser_tone1_tune, .size = ARRAY_SIZE(browser_tone1_tune)} 
        },
        {   
            .value[CABC_OFF_MODE] = {.name = "BROWSER_TONE2,CABC:OFF",  .value = browser_tone2_tune, .size = ARRAY_SIZE(browser_tone2_tune)}, 
            .value[CABC_ON_MODE] = {.name = "BROWSER_TONE2,CABC:ON",   .value = browser_tone2_tune, .size = ARRAY_SIZE(browser_tone2_tune)} 
        },
        {   
            .value[CABC_OFF_MODE] = {.name = "BROWSER_TONE3,CABC:OFF",  .value = browser_tone3_tune, .size = ARRAY_SIZE(browser_tone3_tune)}, 
            .value[CABC_ON_MODE] = {.name = "BROWSER_TONE3,CABC:ON",   .value = browser_tone3_tune, .size = ARRAY_SIZE(browser_tone3_tune)} 
        },
};
/*  end of TUNE VALUE
 *  ########################################################## */

bool cmc624_I2cWrite16(unsigned char Addr, unsigned long Data)
{
	int err = -1000;
	struct i2c_msg msg[1];
	unsigned char data[3];

	if (!p_cmc624_data) {
		printk("p_cmc624_data is NULL\n");
		return -ENODEV;
	}

	if(!is_cmc624_on) {
		printk("cmc624 power down..\n");
		return -ENODEV;
	}
	g_client = p_cmc624_data->client;

	if ((g_client == NULL)) {
		printk("cmc624_I2cWrite16 g_client is NULL\n");
		return -ENODEV;
	}

	if (!g_client->adapter) {
		printk("cmc624_I2cWrite16 g_client->adapter is NULL\n");
		return -ENODEV;
	}
	
#if defined(CONFIG_TARGET_LOCALE_KOR_SKT)|| defined(CONFIG_TARGET_LOCALE_KOR_LGU)
		if(Addr == 0x0000)
		{
		if(Data == last_cmc624_Bank)
			{
			return 0;
			}
		last_cmc624_Bank = Data;
		}
	else if(Addr == 0x0001 && last_cmc624_Bank==0)
		{
		last_cmc624_Algorithm = Data;
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
		//printk("%s %d i2c transfer OK\n", __func__, __LINE__);
		return 0;
	}

	printk("%s i2c transfer error:%d(a:%d)\n",
			__func__, err, Addr);
	return err;
}

int cmc624_I2cRead16(u8 reg, u16 *val)
{
	int 	 err;
	struct	 i2c_msg msg[2];
	u8 regaddr = reg;
	u8 data[2];

	if (!p_cmc624_data) {
		pr_err("%s p_cmc624_data is NULL\n", __func__);
		return -ENODEV;
	}
	g_client = p_cmc624_data->client;

	if ((g_client == NULL)) {
		pr_err("%s g_client is NULL\n", __func__);
		return -ENODEV;
	}

	if (!g_client->adapter) {
		pr_err("%s g_client->adapter is NULL\n", __func__);
		return -ENODEV;
	}

	if (regaddr == 0x0001) {
		*val = last_cmc624_Algorithm;
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
 *  	CMC624 Backlight Control function
  #####################################################
*/

/* value: 0 ~ 1600*/
static void cmc624_cabc_pwm_brightness_reg(int value)
{
	int reg;
	unsigned char *p_plut;
	u16 min_duty;

	if (!p_cmc624_data) {
		pr_err("%s cmc624 is not initialized\n", __func__);
		return;
	}
    
    if (cmc624_state.scenario == mDNIe_VIDEO_MODE) {
        p_plut = cmc624_Power_LUT[1];
        pr_debug("[CMC624:Info] : %s : video mode PLUT : %d\n",__func__,p_plut[0]);
    }
    else { 
	    p_plut = cmc624_Power_LUT[0];
        pr_debug("[CMC624:Info] : %s : normal mode PLUT : %d\n",__func__,p_plut[0]);
    }
	min_duty = p_plut[7] * value / 100;
	if (min_duty < 4) {
		reg = 0xc000 | (max(1, (value*p_plut[3]/100)));
	} 
    else {
		 /*PowerLUT*/
		cmc624_I2cWrite16(0x76, (p_plut[0] * value / 100) << 8
				| (p_plut[1] * value / 100));
		cmc624_I2cWrite16(0x77, (p_plut[2] * value / 100) << 8
				| (p_plut[3] * value / 100));
		cmc624_I2cWrite16(0x78, (p_plut[4] * value / 100) << 8
				| (p_plut[5] * value / 100));
		cmc624_I2cWrite16(0x79, (p_plut[6] * value / 100) << 8
				| (p_plut[7] * value / 100));
		cmc624_I2cWrite16(0x7a, (p_plut[8] * value / 100) << 8);
		reg = 0x5000 | (value<<4);
	}
	cmc624_I2cWrite16(0xB4, reg);
}

int Islcdonoff(void)
{
//	return 0;
	return cmc624_state.suspended; //tmps solution
}

/*value: 0 ~ 100*/
static void cmc624_cabc_pwm_brightness(int value)
{
	pr_debug("[CMC624:Info] : %s : value : %d\n",__func__,value);
return;//
/*	if (!p_cmc624_data) {
		pr_err("%s cmc624 is not initialized\n", __func__);
		return;
	}
	cmc624_I2cWrite16(0x00, 0x0000);
	cmc624_cabc_pwm_brightness_reg(value);
	cmc624_I2cWrite16(0x28, 0x0000);*/
}

/*value: 0 ~ 100*/
static void cmc624_manual_pwm_brightness(int value)
{
	int reg;


    pr_debug("[CMC624:INFO]: %s value : %d\n",__func__,value);

return;//
/*	if (!p_cmc624_data) {
		pr_err("%s cmc624 is not initialized\n", __func__);
		return;
	}

	reg = 0x4000 | (value<<4);

	cmc624_I2cWrite16(0x00, 0x0000);
	cmc624_I2cWrite16(0xB4, reg);
	cmc624_I2cWrite16(0x28, 0x0000);
*/
}

/* value: 0 ~ 1600*/
void cmc624_pwm_brightness(int value)
{
	int data;

	pr_debug("[CMC624:Info] : %s : value : %d\n",__func__,value);
return;//
/*	if (value < 0)
		data = 0;
	else if (value > 1600)
		data = 1600;
	else
		data = value;

	if (data < 16)
		data = 1; //Range of data 0~1600, min value 0~15 is same as 0
	else
		data = data >> 4;
    
    cmc624_cabc_pwm_brightness(data);
*/
}

void cmc624_pwm_brightness_bypass(int value)
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

		cmc624_state.brightness = data;
		cmc624_manual_pwm_brightness(data);
}

#define VALUE_FOR_1600	16
void set_backlight_pwm(int level)
{

	mutex_lock(&tuning_mutex);
	if (!Islcdonoff()) {
		if(cmc624_state.main_tune == NULL) {
				/*
				 * struct cmc624_state_type cmc624_state = {
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
			printk("[CMC624] cmc624_state.main_tune is NULL?...\n");
			printk("[CMC624] DUMP :\n");
			printk("[CMC624] cabc_mode : %d\n",cmc624_state.cabc_mode);
			printk("[CMC624] brightness : %d\n",cmc624_state.brightness);
			printk("[CMC624] suspended : %d\n",cmc624_state.suspended);
			printk("[CMC624] scenario : %d\n",cmc624_state.scenario);
			printk("[CMC624] background : %d\n",cmc624_state.background);
			printk("[CMC624] temperature : %d\n",cmc624_state.temperature);
			printk("[CMC624] ove : %d\n",cmc624_state.ove);
//			printk("[CMC624] sub_tune : %d\n",cmc624_state.sub_tune);
//			printk("[CMC624] main_tune : %d\n",cmc624_state.main_tune);
			printk("======================================================\n");
			mutex_unlock(&tuning_mutex);
			return;
		}
		if(cmc624_state.cabc_mode == CABC_OFF_MODE || 
						(cmc624_state.main_tune->flag & TUNE_FLAG_CABC_ALWAYS_OFF))
			cmc624_manual_pwm_brightness(level);		
		else 
			cmc624_pwm_brightness(level * VALUE_FOR_1600);
	}
	mutex_unlock(&tuning_mutex);
}

/*#####################################################
 *  	CMC624 common function
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
	pr_debug("[CMC624:Info] : %s : value : %d\n",__func__,value);
return;//
/*
	if(value == 1) {
		int i = 0;
		int num = ARRAY_SIZE(cmc624_bypass);

		for (i=0; i<num; i++)
			cmc624_I2cWrite16(cmc624_bypass[i].RegAddr, cmc624_bypass[i].Data);

		cmc624_pwm_brightness_bypass(current_gamma_level);
	} else if(value == 0) {
		cabc_onoff_ctrl(current_cabc_onoff);
		cmc624_pwm_brightness(current_gamma_level);
	}
*/
}

void cabc_onoff_ctrl(int value)
{

    if (apply_main_tune_value(cmc624_state.scenario, cmc624_state.background, value, 0) != 0){
        pr_debug("[CMC624:ERROR]:%s: apply main tune value faile \n",__func__);
        return ;
    }
}

static int cmc624_set_tune_value(const struct Cmc624RegisterSet *value, int array_size)
{
    int ret = 0; 
    unsigned int num;
	unsigned int i = 0;

	if(!p8lte_has_cmc624()) return 0;
	if(Cmc624_TuneSeqLen) return 0;

    mutex_lock(&tuning_mutex);    

    num = array_size;

	for (i = 0; i < num; i++) {
        ret = cmc624_I2cWrite16(value[i].RegAddr,value[i].Data);
        if (ret != 0) {
            pr_debug("[CMC624:ERROR]:cmc624_I2cWrite16 failed : %d\n",ret);
            goto set_error;
        }
    }
    
set_error:     
    mutex_unlock(&tuning_mutex);
    return ret;
}

static int parse_text(char *src, int len)
{
	int i,count, ret;
	int index=0;
	char *str_line[CMC624_MAX_SETTINGS];
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
			Cmc624_TuneSeq[index].RegAddr = (unsigned char)data1;
			Cmc624_TuneSeq[index++].Data  = (unsigned long)data2;
		}
	}
	return index;
}


bool cmc624_tune(unsigned long num) 
{
	unsigned int i;

    pr_debug("[CMC624:INFO] Start tuning CMC624\n");

	for (i=0; i<num; i++) {
        pr_debug("[CMC624:Tuning][%2d] : reg : 0x%2x, data: 0x%4x\n",i+1, Cmc624_TuneSeq[i].RegAddr, Cmc624_TuneSeq[i].Data);

		if (cmc624_I2cWrite16(Cmc624_TuneSeq[i].RegAddr, Cmc624_TuneSeq[i].Data) != 0) {
			pr_err("[CMC624:ERROR] : I2CWrite failed\n");
			return 0;
		}
        pr_debug("[CMC624:Tunig] : Write Done\n");        

		if (Cmc624_TuneSeq[i].RegAddr == CMC624_REG_SWRESET && Cmc624_TuneSeq[i].Data == 0xffff ){
			mdelay(3);
		}
	}
	pr_debug("[CMC624:INFO] End tuning CMC624\n");
	return 1;
}

int load_tuning_data(char *filename)
{
    struct file *filp;
	char	*dp;
	long	l;
	loff_t  pos;
	int     ret;
	mm_segment_t fs;

	pr_debug("[CMC624:INFO]:%s called loading file name : %s\n",__func__,filename);
	Cmc624_TuneSeqLen = 0;

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if(IS_ERR(filp)) 
	{
		pr_debug("[CMC624:ERROR]:File open failed\n");
		return -1;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	pr_debug("[CMC624:INFO]: Loading File Size : %ld(bytes)", l);

	dp = kmalloc(l+10, GFP_KERNEL);	
	if(dp == NULL){
		pr_debug("[CMC624:ERROR]:Can't not alloc memory for tuning file load\n");
		filp_close(filp, current->files);
		return -1;
	}
	pos = 0;
	memset(dp, 0, l);
	pr_debug("[CMC624:INFO] : before vfs_read()\n");
	ret = vfs_read(filp, (char __user *)dp, l, &pos);   
	pr_debug("[CMC624:INFO] : after vfs_read()\n");

	if(ret != l) {
        pr_debug("[CMC624:ERROR] : vfs_read() filed ret : %d\n",ret);
        kfree(dp);
		filp_close(filp, current->files);
		return -1;
	}

	filp_close(filp, current->files);

	set_fs(fs);
	Cmc624_TuneSeqLen = parse_text(dp, l);

	if(!Cmc624_TuneSeqLen) {
		pr_debug("[CMC624:ERROR]:Nothing to parse\n");
		kfree(dp);
		return -1;
	}

	pr_debug("[CMC624:INFO] : Loading Tuning Value's Count : %d", Cmc624_TuneSeqLen);
	cmc624_tune(Cmc624_TuneSeqLen);


	kfree(dp);
	return Cmc624_TuneSeqLen;
}
#define SUB_TUNE	0
#define MAIN_TUNE	1
#define BROW_TUNE	2
int init_tune_flag[3] = {0,};

int apply_sub_tune_value(enum eCurrent_Temp temp, enum eOutdoor_Mode ove, enum eCabc_Mode cabc, int force)
{

	int register_size;
#if 0
    if(tuning_enable){
        pr_debug("[CMC624:INFO]:%s:Tuning mode Enabled\n",__func__);
        return 0;
    }
#endif
      if(!init_tune_flag[SUB_TUNE]){init_tune_flag[SUB_TUNE] = 1; init_tune_flag[BROW_TUNE]=0; force = 1;}
    
	if (force == 0) {
        if((cmc624_state.temperature == temp) && (cmc624_state.ove == ove)) {
            pr_debug("[CMC624:INFO]:%s:already setted temp : %d, over : %d\n",__func__ ,temp ,ove);
            return 0; 
        }
    }
	pr_debug("=================================================\n");
	pr_debug(" CMC624 Mode Change.sub tune\n");
	pr_debug("==================================================\n");
    pr_debug("[CMC624:INFO]:%s:curr sub tune : %s\n",__func__,sub_tune_value[cmc624_state.temperature][cmc624_state.ove].value[cmc624_state.cabc_mode].name);
    pr_debug("[CMC624:INFO]:%s: sub tune : %s\n",__func__,sub_tune_value[temp][ove].value[cabc].name);

    if((ove == OUTDOOR_OFF_MODE) || (temp == TEMP_STANDARD)) {
        pr_debug("[CMC624:INFO]:%s:set default main tune\n",__func__);
		register_size = tune_value[cmc624_state.background][cmc624_state.scenario].value[cmc624_state.cabc_mode].size;       
        if (cmc624_set_tune_value(tune_value[cmc624_state.background][cmc624_state.scenario].value[cmc624_state.cabc_mode].tune,register_size) != 0){
            pr_debug("[CMC624:ERROR]:%s: set tune value falied \n",__func__);
            return -1; 
        	}
        
        if((ove != OUTDOOR_OFF_MODE) || (temp != TEMP_STANDARD)) {
            goto set_sub_tune;
       	}			
        
    }
    else {
set_sub_tune :         
		  register_size = sub_tune_value[temp][ove].value[cabc].size;
		  if(cmc624_set_tune_value(sub_tune_value[temp][ove].value[cabc].value,register_size)) {
				pr_debug("[CMC624:ERROR]:%s: set tune value falied \n",__func__);   
				return -1;
		  }
	}
    cmc624_state.temperature = temp; 
    cmc624_state.ove = ove;
    return 0;
}

int apply_main_tune_value(enum eLcd_mDNIe_UI ui, enum eBackground_Mode bg, enum eCabc_Mode cabc, int force)
{
    enum eCurrent_Temp temp = 0;

	//pr_debug("==================================================\n");
	//pr_debug(" CMC624 Mode Change. Main tune\n");
	//pr_debug("==================================================\n");

    if(!init_tune_flag[MAIN_TUNE]){init_tune_flag[MAIN_TUNE] = 1; init_tune_flag[BROW_TUNE]=0; force = 1;}
    if (force == 0) {
        if((cmc624_state.scenario == ui) && (cmc624_state.background == bg) && (cmc624_state.cabc_mode == cabc)) {
            pr_debug("[CMC624:INFO]:%s:already setted ui : %d, bg : %d\n",__func__ ,ui ,bg);
            return 0; 
        }
    }
    pr_debug("[CMC624:INFO]:%s:curr main tune : %s\n",__func__, tune_value[cmc624_state.background][cmc624_state.scenario].value[cmc624_state.cabc_mode].name);
    pr_debug("[CMC624:INFO]:%s: main tune : %s\n",__func__,tune_value[bg][ui].value[cabc].name);

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
			if (apply_sub_tune_value(temp, cmc624_state.ove, cabc, 0) != 0){
				pr_debug("[CMC624:ERROR]:%s:apply_sub_tune_value() faield \n",__func__);
			}
			goto rest_set;
			}
	}		

	pr_debug("[CMC624:INFO]:%s set, size : %d\n",tune_value[bg][ui].value[cabc].name,tune_value[bg][ui].value[cabc].size);
    if (cmc624_set_tune_value(tune_value[bg][ui].value[cabc].tune,tune_value[bg][ui].value[cabc].size) != 0){
        pr_err("[CMC624:ERROR]:%s: set tune value falied \n",__func__);
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
        if (apply_sub_tune_value(temp, cmc624_state.ove, cabc, 0) != 0){
            pr_debug("[CMC624:ERROR]:%s:apply_sub_tune_value() faield \n",__func__);
        }
     }       
        
rest_set:    
    
    cmc624_state.scenario = ui;
    cmc624_state.background = bg;
    cmc624_state.cabc_mode = cabc;
    cmc624_state.main_tune = &tune_value[bg][ui].value[cabc];

   if(ui == mDNIe_UI_MODE) {
        cmc624_state.temperature = TEMP_STANDARD;
        cmc624_state.ove = OUTDOOR_OFF_MODE;
    }
    
    if (ui == mDNIe_VIDEO_WARM_MODE) 
        cmc624_state.temperature = TEMP_WARM;
    else if (ui == mDNIe_VIDEO_COLD_MODE)
        cmc624_state.temperature = TEMP_COLD;

	set_backlight_pwm(cmc624_state.brightness);
    
	return 0;
}



int apply_browser_tune_value(enum SCENARIO_COLOR_TONE browser_mode, int force)
{
    enum eCurrent_Temp temp = 0;
    browser_mode -= COLOR_TONE_1;

	pr_debug("==================================================\n");
	pr_debug(" CMC624 Mode Change. brower tune\n");
	pr_debug("==================================================\n");

    if(!init_tune_flag[BROW_TUNE]){init_tune_flag[MAIN_TUNE] = 0; init_tune_flag[SUB_TUNE] = 0; init_tune_flag[BROW_TUNE]=1; force = 1;}

    if ((force == 0) && (cmc624_state.browser_scenario == browser_mode))  {
            pr_debug("[CMC624:INFO]:%s:already setted browser tone : %d\n",__func__ , browser_mode);
            return 0; 
        }
    pr_debug("[CMC624:INFO]:%s curr browser tune : %s\n",__func__, browser_tune_value[cmc624_state.browser_scenario].value[cmc624_state.cabc_mode].name);
    pr_debug("[CMC624:INFO]:%s browser tune : %s\n",__func__, browser_tune_value[browser_mode].value[cmc624_state.cabc_mode].name);
    pr_debug("[CMC624:INFO]:%s set, size : %d\n", browser_tune_value[browser_mode].value[cmc624_state.cabc_mode].name, browser_tune_value[browser_mode].value[cmc624_state.cabc_mode].size);

	
    if (cmc624_set_tune_value(browser_tune_value[browser_mode].value[cmc624_state.cabc_mode].value,  browser_tune_value[browser_mode].value[cmc624_state.cabc_mode].size) != 0)
	{
       	 pr_err("[CMC624:ERROR]:%s: set tune value falied \n",__func__);
	        return -1; 
	}

    
    cmc624_state.browser_scenario = browser_mode;
    
	return 0;
}

#if 0
void p8lte_test_tune_dmb(void)
{
	int i = 0;

	printk("[%s]\n", __func__);	
	for (i = 0; i < ARRAY_SIZE(cmc624_tune_dmb_test); i++) {
		if (cmc624_I2cWrite16(cmc624_tune_dmb_test[i].RegAddr, cmc624_tune_dmb_test[i].Data)) {
			pr_err("why return false??!!!\n");
			return -1;
		}
	}
}
#endif

#define LDI_MTP_ADDR 0xd3
#define LDI_ID_ADDR 0xd1
u8 mtp_read_data[24] = {0,};
u8 id_buffer[8] = {0,};

int p8lte_get_reg_data(int reg, unsigned char *buf, int size)
{
	if(reg == LDI_MTP_ADDR && size<=24)
	{
		memcpy(buf, mtp_read_data, size);
	}
	else if(reg == LDI_ID_ADDR && size<=8)
	{
		memcpy(buf, id_buffer, size);
	}
	else
	{
		printk("[%s] Fail: Unknow reg!!\n", __func__);
		return -1;
	}

	printk("[%s] reg[%x] success!!\n", __func__, reg);
	return 0;
}
EXPORT_SYMBOL(p8lte_get_reg_data);

static void read_ldi_reg(unsigned long src_add, unsigned char *read_data, unsigned char read_length)
{
	int i;
	u16 data=0;

		// LP Operation2 (Read IDs)
		cmc624_I2cWrite16( 0xaa, read_length );		
		
		cmc624_I2cWrite16( 0x02, 0x1100 ); // GENERIC WRITE 1 PARA
		cmc624_I2cWrite16( 0x04, 0x00b0 ); // WRITE B0
		
		cmc624_I2cWrite16( 0x02, 0x1501 ); // GENERIC READ 1 PARA
		cmc624_I2cWrite16( 0x04, src_add ); // WRITE d1/ d3 (Address of MTP)

		if(src_add == LDI_ID_ADDR)printk("Read IDs from D1h\n");
		else printk("READ Bright Gamma Offset from MTP(D3h)\n");
		
		for(i=0; i<read_length; i++)
		{
			cmc624_I2cRead16( 0x08, &data ); // Read Data Valid Register
			
			if( data&0x1000 != 0x1000 ) {
				printk("[%d] invalid data %x!!!\n", i, data); // Data Valid Check 0x1000?
			} 
			else {
				printk("[%d] 0x08 data %x!!!\n", i, data); // Data Valid Check 0x1000?
			}

			cmc624_I2cRead16( 0x06, &data ); // Read Nst Parameter
			read_data[i] = data; //Store Nst Parameter

			printk("[%d] IDs = %x\n", i, read_data[i]);
			
		}
		cmc624_I2cRead16( 0x08, &data ); // Read Data Valid Register
		if( data&0x1000 != 0x1000 )
		{
			printk("End of read IDs %x!!!\n", data); // Data Valid Check 0x1000?
		} else {
			printk("0x08 data %x!!!\n", data); // Data Valid Check 0x1000?
		}
}

/* ################# END of SYSFS Function ############################# */

static bool CMC624_SetUp(void)
{
	u16 data=0;
	int i = 0;
	static int bInit = 0, bRead = 0;
	
	mutex_lock(&tuning_mutex);
	if( !bInit ) {
//		bInit = 1;
		for (i = 0; i < ARRAY_SIZE(cmc624_init); i++) {
			if (cmc624_I2cWrite16(cmc624_init[i].RegAddr, cmc624_init[i].Data)) {
				pr_err("why return false??!!!\n");
				mutex_unlock(&tuning_mutex);
				return FALSE;
			}
			if (cmc624_init[i].RegAddr == 0x3E && cmc624_init[i].Data == 0x2223) {
				msleep(1);
			}
		}

		if(!bRead){
			bRead = 1;

			read_ldi_reg(LDI_ID_ADDR, id_buffer, 8);
			read_ldi_reg(LDI_MTP_ADDR, mtp_read_data, 24);
		}
#if 0
		cmc624_I2cRead16( 0x00, &data );
		printk("0x00 data %x!!!\n", data);
		cmc624_I2cRead16( 0x82, &data );
		printk("0x82 data %x!!!\n", data);
		cmc624_I2cRead16( 0x83, &data );
		printk("0x83 data %x!!!\n", data);
		cmc624_I2cRead16( 0xC2, &data );
		printk("0xC2 data %x!!!\n", data);
		cmc624_I2cRead16( 0xC3, &data );
		printk("0xC3 data %x!!!\n", data);
#endif

		//CONV
		cmc624_I2cWrite16( 0x00, 0x0003 );	// BANK 3
		cmc624_I2cWrite16( 0x01, 0x0001 );	// MIPI TO MIPI

//		cmc624_I2cRead16( 0x01, &data );
//		printk("0x01 data %x!!!\n", data);

		cmc624_I2cWrite16( 0x00, 0x0002 );	// BANK 2
		cmc624_I2cWrite16( 0x52, 0x0001 );	// RGB IF ENABLE

        cmc624_I2cWrite16( 0x00, 0x0003 );	// BANK 3 goooooood
//		cmc624_I2cRead16( 0x52, &data );
//		printk("0x52 data %x!!!\n", data);
	} else {

		printk("cmc624_init3!!!\n");

		for (i = 0; i < ARRAY_SIZE(cmc624_wakeup); i++) {
			if (cmc624_I2cWrite16(cmc624_wakeup[i].RegAddr, cmc624_wakeup[i].Data)) {
				mutex_unlock(&tuning_mutex);
				pr_err("why return false??!!!\n");
				return FALSE;
			}
		}
	}
	//p8lte_test_tune_dmb();
	mutex_unlock(&tuning_mutex);
	
	return TRUE;
}

void Check_Prog(void)
{
	u16 bank=0;
	u16 data=0;
	
	printk("++++++%s++++\n", __func__);
	mutex_lock(&tuning_mutex);
	cmc624_I2cWrite16( 0x00, 0x0003 );	// BANK 3

	cmc624_I2cRead16( 0x00, &data ); // Read Nst Parameter
	printk("0x00 data %x!!!\n", data);
	cmc624_I2cRead16( 0x82, &data ); // Read Nst Parameter
	printk("0x82 data %x!!!\n", data);
	cmc624_I2cRead16( 0x83, &data ); // Read Nst Parameter
	printk("0x83 data %x!!!\n", data);
	cmc624_I2cRead16( 0xC2, &data ); // Read Nst Parameter
	printk("0xC2 data %x!!!\n", data);
	cmc624_I2cRead16( 0xC3, &data ); // Read Nst Parameter
	printk("0xC3 data %x!!!\n", data);

	cmc624_I2cRead16( 0x01, &data );
	cmc624_I2cRead16( 0x00, &bank );
	printk("[bank %d] 0x01 data %x!!!\n", bank, data);

	cmc624_I2cWrite16( 0x00, 0x0002 );	// BANK 2
	cmc624_I2cRead16( 0x52, &data );
	cmc624_I2cRead16( 0x00, &bank );
	printk("[bank %d] 0x52 data %x!!!\n", bank, data);
	
	cmc624_I2cWrite16( 0x00, 0x0005 );	// BANK 5
	cmc624_I2cRead16( 0x0B, &data );
	cmc624_I2cRead16( 0x00, &bank );
	printk("[bank %d] 0x0B data %x!!!\n", bank, data);

	mutex_unlock(&tuning_mutex);
	printk("-----%s-----\n", __func__);
}

void change_mon_clk(void)
{
	printk("++++++%s++++\n", __func__);
	cmc624_I2cWrite16( 0x00, 0x0002 );	// BANK 2
	cmc624_I2cWrite16( 0x3F, 0x0107 );	// 
	printk("-----%s-----\n", __func__);
}

int p8lte_cmc624_on(int enable)
{

	int ret = 0;

	printk("[LCD] +%s:enable:%d\n", __FUNCTION__, enable);
#if 0
	if(!v_ima_1_8v || !display_3_3v ) {
		pr_debug("PM regulator get failed.\n");
		return -1;
	}
#endif

	if (enable) {
#if 1
		is_cmc624_on = 1;
		CMC624_SetUp();
		cmc624_state.suspended = 0;
//		msleep(1);
		if(Cmc624_TuneSeqLen) {
//			cmc624_tune(Cmc624_TuneSeqLen);
		} else {
			apply_main_tune_value(cmc624_state.scenario, cmc624_state.background, cmc624_state.cabc_mode, 1);
		}
#else
		gpio_set_value(IMA_nRST, 1);
		ret = gpio_get_value(IMA_nRST);
		pr_debug("%s, IMA_nRST : %d\n",__func__,ret);
		udelay(20);

			/*  LCD_PWR_EN GPIO set to HIGH for V_IMA_1.2V on */
			pr_debug("LCD PM POWER ON!!\n");
			gpio_set_value(LCD_PWR_EN, 1);
			ret = gpio_get_value(LCD_PWR_EN);
			pr_debug("%s, LCD_PWR_EN : %d\n",__func__,ret);
			
			#if defined(CONFIG_TARGET_LOCALE_KOR_SKT)
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
			#if defined(CONFIG_TARGET_LOCALE_KOR_SKT)
			gpio_set_value(MLCD_ON, 1);
			ret = gpio_get_value(MLCD_ON);
			pr_debug("%s, MLCD_ON : %d\n",__func__,ret);
			udelay(200);
			#endif
	
			/*  CMC624 GPIO up */
			gpio_tlmm_config(GPIO_CFG(IMA_I2C_SDA,  0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),GPIO_CFG_ENABLE);
			gpio_tlmm_config(GPIO_CFG(IMA_I2C_SCL,  0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),GPIO_CFG_ENABLE);

			is_cmc624_on = 1;
#if BYPASS_MODE_ON
			p8lte_cmc624_bypass_mode();
#else // BYPASS_MODE_OFF
			p8lte_cmc624_normal_mode();
			mdelay(1);
			CMC624_SetUp();
			cmc624_state.suspended = 0;
			msleep(10);
			apply_main_tune_value(cmc624_state.scenario, cmc624_state.background, cmc624_state.cabc_mode, 1);
//			set_backlight_pwm(cmc624_state.brightness);	
#endif
			msleep(5);

	
			ret = p8lte_lvds_on(enable);
			if ( ret < 0) {
					pr_debug("LVDS turn on ERROR\n");
					return ret;
			}

			msleep(300); /*  H/W limitation. */	

			ret = p8lte_backlight_en(enable);
			if (ret<0) {
					pr_debug("BACKLIGHT turn on ERROR\n");
					return ret;
			}
#endif

	} else {
			pr_debug("LCD PM POWER OFF!!\n");
			
			/* sleep seq 
			 * 1. Backlight off
			 * 2. CMC624 GPIO off
			 * 3. display power down
   			 * 4. CMC624 I2C GPIO down
 			 * 5. LCD_PWR_EN off
			 * 6. LVDS off
			*/
#if 1

			is_cmc624_on = 0;
			cmc624_state.suspended = 1;
#else
			ret = p8lte_backlight_en(enable);
			if (ret<0) {
					pr_debug("BACKLIGHT turn on ERROR\n");
					return ret;
			}
			ret = gpio_get_value(CMC624_FAILSAFE);
			if(ret)
				gpio_set_value(CMC624_FAILSAFE, 0);
			udelay(20);

			ret = gpio_get_value(CMC624_BYPASS);
			if(ret)
				gpio_set_value(CMC624_BYPASS, 0);
			udelay(20);

			ret = gpio_get_value(IMA_SLEEP);
			if(ret)
				gpio_set_value(IMA_SLEEP, 0);
			udelay(20);

			ret = gpio_get_value(IMA_nRST);
			if(ret)
				gpio_set_value(IMA_nRST, 0);
			msleep(4);

			#if defined(CONFIG_TARGET_LOCALE_KOR_SKT)
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
			is_cmc624_on = 0;
			/*  CMC624 GPIO down */
			gpio_tlmm_config(GPIO_CFG(IMA_I2C_SDA,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),GPIO_CFG_DISABLE);
			gpio_tlmm_config(GPIO_CFG(IMA_I2C_SCL,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),GPIO_CFG_DISABLE);

			gpio_set_value(LCD_PWR_EN, 0);
			ret = gpio_get_value(LCD_PWR_EN);
			pr_debug("%s, LCD_PWR_EN : %d\n",__func__,ret);
			cmc624_state.suspended = 1;
#if defined (CONFIG_MACH_P4_LTE) && defined (CONFIG_TARGET_LOCALE_JPN_NTT)
			msleep(500); /* This is H/W Limitation (about LVDS) */
#else
			msleep(200); /* This is H/W Limitation (about LVDS) */
#endif
#endif
    }

	printk("[LCD] -%s\n", __FUNCTION__);

	return ret;

}

int p8lte_cmc624_bypass_mode(void)
{
		int ret;
#if 0
		udelay(20);
		gpio_set_value(CMC624_FAILSAFE, 1);
		ret = gpio_get_value(CMC624_FAILSAFE);
		pr_debug("%s, CMC624_FAILSAFE : %d\n",__func__,ret);
		udelay(20);

		ret = gpio_get_value(CMC624_BYPASS);
		pr_debug("%s, CMC624_BYPASS : %d\n",__func__,ret);
		udelay(20);
#endif
		gpio_set_value(IMA_SLEEP, 1);
		ret = gpio_get_value(IMA_SLEEP);
		pr_debug("%s, IMA_SLEEP : %d\n",__func__,ret);
		udelay(20);

		gpio_set_value(IMA_nRST, 0);
		ret = gpio_get_value(IMA_nRST);
		pr_debug("%s, IMA_nRST : %d\n",__func__,ret);
		msleep(4);
		gpio_set_value(IMA_nRST, 1);
		ret = gpio_get_value(IMA_nRST);
		pr_debug("%s, IMA_nRST : %d\n",__func__,ret);

		return ret;

}

int p8lte_cmc624_normal_mode(void)
{
		int ret;
#if 0
		udelay(20);
		gpio_set_value(CMC624_FAILSAFE, 1);
		ret = gpio_get_value(CMC624_FAILSAFE);
		pr_debug("%s, CMC624_FAILSAFE : %d\n",__func__,ret);
		udelay(20);

		gpio_set_value(CMC624_BYPASS, 1);
		ret = gpio_get_value(CMC624_BYPASS);
		pr_debug("%s, CMC624_BYPASS : %d\n",__func__,ret);
		udelay(20);
#endif
		gpio_set_value(IMA_SLEEP, 1);
		ret = gpio_get_value(IMA_SLEEP);
		pr_debug("%s, IMA_SLEEP : %d\n",__func__,ret);
		udelay(20);

		gpio_set_value(IMA_nRST, 0);
		ret = gpio_get_value(IMA_nRST);
		pr_debug("%s, IMA_nRST : %d\n",__func__,ret);
		msleep(4);
		gpio_set_value(IMA_nRST, 1);
		ret = gpio_get_value(IMA_nRST);
		pr_debug("%s, IMA_nRST : %d\n",__func__,ret);

		return ret;

}

/*  Platform Range : 0 ~ 255
 *  CMC624   Range : 0 ~ 100 
 *  User Platform Range : 
 *   - MIN : 30
 *   - MAX : 255
 *  if under 30, CMC624 val : 2%
 *  if 30, CMC624 val : 3%
 *  if default, CMC624 val : 49%
 *  if 255, CMC624 val : 100%
 */

#define DIMMING_BRIGHTNESS_VALUE    20
#define MINIMUM_BRIGHTNESS_VALUE    30
#define MAXIMUM_BRIGHTNESS_VALUE    255
#define DEFAULT_BRIGHTNESS_VALUE    144
#define QUATER_BRIGHTNESS_VALUE     87
 
#define DIMMING_CMC624_VALUE    1
#define MINIMUM_CMC624_VALUE    1   /*  0% progress bar */
#define MAXIMUM_CMC624_VALUE    100 /*  100% progress bar */
#define DEFAULT_CMC624_VALUE    49 /* 50% progress bar */
#define QUATER_CMC624_VALUE     9 /*  25% progress bar */
 
#define QUATER_DEFAUT_MAGIC_VALUE   3
#define UNDER_DEFAULT_MAGIC_VALUE   52
#define OVER_DEFAULT_MAGIC_VALUE    17

void cmc624_Set_Region_Ext(int enable, int hStart, int hEnd, int vStart, int vEnd)
{
	u16 data=0;
	u16 ratio=0x1000;
	int width = hEnd-hStart;
	int height = vEnd-vStart;

	if(!p8lte_has_cmc624()) return;
	if(Cmc624_TuneSeqLen) return 0;
	
	mutex_lock(&tuning_mutex);  

#if 0
	cmc624_I2cWrite16(0x0000,0x0000);
	cmc624_I2cRead16(0x0001, &data);

	data &= 0x00ff;
#endif
	if(width>height) ratio = 0x1000 * width / 1280;
	else ratio = 0x1000 * height / 1280;

	printk("[%s] (%d, %d) - (%d, %d) ratio %x!!!\n", __func__, hStart, vStart, hEnd, vEnd, ratio);

	if(enable)
	{
		cmc624_I2cWrite16(0x00,0x0000); //BANK 0
		cmc624_I2cWrite16(0x0c,0x5555); //ROI on
		cmc624_I2cWrite16(0x0d,0x1555); //ROI on
		cmc624_I2cWrite16(0x0e,hStart); //ROI x start 0
		cmc624_I2cWrite16(0x0f,hEnd); //ROI x end 1279
		cmc624_I2cWrite16(0x10,vStart+1); //ROI y start 1
		cmc624_I2cWrite16(0x11,vEnd+1); //ROI y end 800
		cmc624_I2cWrite16(0xb8,ratio); //DE Max Ratio
		cmc624_I2cWrite16(0xff,0x0000); //Mask Release
	}
	else
	{
		cmc624_I2cWrite16(0x00,0x0000); //BANK 0
		cmc624_I2cWrite16(0x0c,0x0000); //ROI off
		cmc624_I2cWrite16(0x0d,0x0000); //ROI off
		cmc624_I2cWrite16(0xff,0x0000); //Mask Release
	}
	mutex_unlock(&tuning_mutex);
	
//	cmc624_current_region_enable = enable;
}
//EXPORT_SYMBOL(p8lte_cmc624_set_region);

#if 0
int p8lte_gpio_init(void)
{
	int status;

	struct pm8058_gpio lvds_cfg = {
				.direction      = PM_GPIO_DIR_OUT,
				.pull           = PM_GPIO_PULL_NO,
				.vin_sel        = 2,
				.function       = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol    = 0,
	};

	struct pm8058_gpio backlight_cfg = {
				.direction      = PM_GPIO_DIR_OUT,
				.pull           = PM_GPIO_PULL_NO,
				.vin_sel        = 2,
				.function       = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol    = 0,

	};

	if(pm8058_gpio_config(PMIC_GPIO_LVDS_nSHDN,&lvds_cfg)) {
		pr_err("%s PMIC_GPIO_LVDS_nSHDN config failed\n", __func__);
	}
	status = gpio_request(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_LVDS_nSHDN), "LVDS_nSHDN");
	if (status) {
			pr_debug(KERN_ERR "%s: LVS_nSHDN gpio"
							" %d request failed\n", __func__,
							PMIC_GPIO_LVDS_nSHDN);
	}


	if(pm8058_gpio_config(PMIC_GPIO_BACKLIGHT_RST,&backlight_cfg)) {
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

int p8lte_lvds_on(int enable)
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

int p8lte_backlight_en(int enable)
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
#endif

static int cmc624_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	struct cmc624_data *data;

	printk("%s +\n", __func__);
	pr_debug("==============================\n");
	pr_debug("cmc624 attach START!!!\n");
	pr_debug("==============================\n");

	data = kzalloc(sizeof(struct cmc624_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	data->client = client;
	i2c_set_clientdata(client, data);

	dev_info(&client->dev, "cmc624 i2c probe success!!!\n");

	p_cmc624_data = data;

#if 0
	pr_debug("==============================\n");
	pr_debug("LCD GPIO Init (LVDS, BACKLIGHT!!!\n");
	pr_debug("==============================\n");

	p8lte_gpio_init();
#endif
	pr_debug("==============================\n");
	pr_debug("CMC624 SYSFS INIT!!!\n");
	pr_debug("==============================\n");
	ret = cmc624_sysfs_init();
	if(ret < 0) {
		pr_debug("CMC624 sysfs initialize FAILED\n");
	}
	printk("%s -\n", __func__);

	return 0;
}

static int __devexit cmc624_i2c_remove(struct i2c_client *client)
{
	struct cmc624_data *data = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);

	kfree(data);

	dev_info(&client->dev, "cmc624 i2c remove success!!!\n");

	return 0;
}

void cmc624_shutdown(struct i2c_client *client)
{
	pr_debug("-0- %s called -0-\n", __func__);
}

static const struct i2c_device_id cmc624[] = {
	{ "cmc624", 0 },
};
MODULE_DEVICE_TABLE(i2c, cmc624);

struct i2c_driver cmc624_i2c_driver = {
	.driver	= {
	.name	= "cmc624",
	.owner = THIS_MODULE,
	},
	.probe 		= cmc624_i2c_probe,
	.remove 	= __devexit_p(cmc624_i2c_remove),
	.id_table	= cmc624,
	.shutdown = cmc624_shutdown,
};

int p8lte_cmc624_init(void)
{
	int ret;

	/* register I2C driver for CMC624 */

	pr_debug("<cmc624_i2c_driver Add START>\n");
	ret = i2c_add_driver(&cmc624_i2c_driver);
	pr_debug("cmc624_init Return value  (%d)\n", ret);
	pr_debug("<cmc624_i2c_driver Add END>\n");

#if 0
	pr_debug("%s\n",__func__);
	ret = gpio_request(IMA_PWR_EN, "IMA_PWR_EN");
	if(ret < 0) {
		pr_debug(KERN_ERR "%s CMC gpio %d request failed\n",__func__,IMA_PWR_EN);
	}
	ret = gpio_request(IMA_nRST, "IMA_nRST");
	if(ret < 0) {
		pr_debug(KERN_ERR "%s CMC gpio %d request failed\n",__func__,IMA_nRST);
	}
	ret = gpio_request(IMA_CMC_EN, "IMA_CMC_EN");
	if(ret < 0) {
		pr_debug(KERN_ERR "%s CMC gpio %d request failed\n",__func__,IMA_CMC_EN);
	}
	ret = gpio_request(IMA_SLEEP, "IMA_SLEEP");
	if(ret < 0) {
		pr_debug(KERN_ERR "%s CMC gpio %d request failed\n",__func__,IMA_SLEEP);
	}
	ret = gpio_request(MLCD_ON, "MLCD_ON");
	if(ret < 0) {
		pr_debug(KERN_ERR "%s CMC gpio %d request failed\n",__func__,MLCD_ON);
	}

	// RESETB->hi, FAILSAFE->lo, SLEEPB->hi
	gpio_direction_output(IMA_nRST, 1);
	gpio_direction_output(IMA_CMC_EN, 0);
	gpio_direction_output(IMA_SLEEP, 1);
	udelay(20);

	// V_IMA_1.1V on
	gpio_direction_output(IMA_PWR_EN, 1);
	udelay(18);

	// V_IMA_1.8V on
	gpio_direction_output(MLCD_ON, 1);
	udelay(20);

	// FAILSAFE->hi
	gpio_direction_output(IMA_CMC_EN, 1);

	gpio_tlmm_config(GPIO_CFG(IMA_I2C_SDA,  0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(IMA_I2C_SCL,  0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),GPIO_CFG_ENABLE);
#endif
	return 0;
	
}

#if 0
int p8lte_cmc624_setup(void)
{
	int i = 0;
	int num = ARRAY_SIZE(cmc624_init);
	/*pr_err("%s num is %d\n", __func__, num);*/

	printk("%s +\n", __func__);
	for (i = 0; i < num; i++) {
		if (cmc624_I2cWrite16(cmc624_init[i].RegAddr, cmc624_init[i].Data)) {
			pr_err("why return false??!!!\n");
			return -1;
		}
		if (cmc624_init[i].RegAddr == CMC624_REG_SWRESET &&	cmc624_init[i].Data == 0xffff) {
			msleep(3);
		}
	}
	printk("%s -\n", __func__);
	return 0;
}
#endif

extern int state_lcd_on; //mipi_s6e8ab0_wxga.c
extern int mipi_dsi_off(struct platform_device *pdev);//mipi_dsi_p8.c
extern int mipi_dsi_on(struct platform_device *pdev);//mipi_dsi_p8.c
extern struct platform_device *pdev_temp;//mipi_dsi_p8.c


int cmc_timer_esd_refresh = 0;
int error_count = 0;

void cmc_lcd_esd_seq( struct sec_esd_info *pSrc )
{
	unsigned int adc_result = 0;
	/*
	u16 compare_data[4] = {0x0000, 0x0914, 0x8000, 0x0000};
	u8 cmc_read_add[4] = {0x82, 0x83, 0xc2, 0xc3};
	u16 data[4] = {0,};

	*/
	u16 compare_data[3] = {0x0000, 0x0914, 0x0000};
	u8 cmc_read_add[3] = {0x82, 0x83,  0xc3};
	u16 data[3] = {0,};
	int i;
	
//	DPRINT("%s : %d\n", __func__, pSrc->esd_cnt);
	mutex_lock(&tuning_mutex);
	if(state_lcd_on)
	{

		cmc624_I2cWrite16( 0x00, 0x0003 );	// BANK 3
		for(i = 0 ; i < 3 ; i++)
		{
			cmc624_I2cRead16(cmc_read_add[i], data+i);		
	//		printk("cmc624 add 0x%x = 0x%x\n",cmc_read_add[i], data[i]);
			if(compare_data[i] != data[i])// &&(0x0934 != data[1]))
			{
				if(error_count >= 4) // sec
				{
					mutex_unlock(&tuning_mutex);					
					error_count ++;
					printk("cmc624 add 0x%x = 0x%x\n",cmc_read_add[i], data[i]);
					printk("cmc esd timer, count : %d ------LCD RESET \n", error_count);
					cmc_timer_esd_refresh = 1;	
					mipi_dsi_off(pdev_temp);
					mipi_dsi_on(pdev_temp);
					cmc_timer_esd_refresh = 0;	
					error_count = 0;
					mutex_lock(&tuning_mutex);
				}
				else
				{
					error_count++;
					printk("cmc624 add 0x%x = 0x%x\n",cmc_read_add[i], data[i]);
					printk("cmc esd timer - count : %d\n", error_count);
				}

				break; 
			}
			else { if((compare_data[2] == data[2])&&(i == 2))
				    {	error_count = 0;
				 //   	printk("cmc esd timer - count : %d\n", error_count);
					 }
				//	printk("cmc624 add 0x%x = 0x%x\n", cmc_read_add[i], data[i]);
				}
			
				
		}
	}
	mutex_unlock(&tuning_mutex);
}

int p8lte_has_cmc624(void)
{
	if(system_rev >= 0x09) return 1;
	return 0;
}

EXPORT_SYMBOL(p8lte_has_cmc624);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("p8lte CMC624 image converter");
MODULE_LICENSE("GPL");
