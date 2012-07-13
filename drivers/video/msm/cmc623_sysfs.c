/*
 * =====================================================================================
 *
 *       Filename:  cmc623_sysfs.c
 *
 *    Description:  SYSFS Node control driver
 *
 *        Version:  1.0
 *        Created:  2011 05/30 15:04:45
 *       Revision:  none
 *       Compiler:  arm-linux-gcc
 *
 *         Author:  Park Gyu Tae (), 
 *        Company:  Samsung Electronics 
 *
 * =====================================================================================

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
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/wakelock.h>
#include <linux/blkdev.h>
#include <linux/i2c.h>
#include <mach/gpio.h>
 
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/earlysuspend.h>
 
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include "pxlte_cmc623.h"
static struct class *mdnie_class;
struct device *tune_cmc623_dev;

#define FALSE 0
#define TRUE  1
#define DUMP_CMC623_REGISTER 0


/* ##########################################################
 * #
 * # BYPASS On / Off Sysfs node
 * # 
 * ##########################################################
 * */
static int current_bypass_onoff = FALSE;
extern struct cmc623_state_type cmc623_state;

static ssize_t bypass_onoff_file_cmd_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	pr_info("called %s \n",__func__);

	return sprintf(buf,"%u\n",current_bypass_onoff);
}

static ssize_t bypass_onoff_file_cmd_store(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int value;
	
	sscanf(buf, "%d", &value);

	pr_info("[bypass set] in bypass_onoff_file_cmd_store, input value = %d \n",value);

	if(cmc623_state.suspended == FALSE) {
		if((current_bypass_onoff==0) && (value == 1)) {
			bypass_onoff_ctrl(value);
			current_bypass_onoff = 1;
		} else if((current_bypass_onoff==1) &&(value == 0)) {
			bypass_onoff_ctrl(value);
			current_bypass_onoff = 0;
		} else
			pr_info("[bypass set] bypass is already = %d \n",current_bypass_onoff);
	} else
		pr_info("[bypass set] LCD is suspend = %d \n",cmc623_state.suspended);

	return size;
}

static DEVICE_ATTR(bypassonoff, 0664, bypass_onoff_file_cmd_show, bypass_onoff_file_cmd_store);


/* ##########################################################
 * #
 * # CABC On / Off Sysfs node
 * # 
 * ##########################################################*/

static ssize_t mdnie_cabc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", cmc623_state.cabc_mode);
}


static ssize_t mdnie_cabc_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	int value;

#ifdef ENABLE_CMC623_TUNING
    if(tuning_enable) {
        printk("[CMC623:INFO]:%s:Tuning Mode Enabled\n",__func__);
        return size;
    }
#endif

	sscanf(buf, "%d", &value);

	printk("[CMC623:INFO]:set cabc on/off mode : %d\n",value);
    
    if (value < CABC_OFF_MODE || value >= MAX_CABC_MODE) {
        printk("[CMC623:ERROR] : wrong cabc mode value : %d\n",value);
        return size;
    }

    if (cmc623_state.suspended == TRUE) {
        pr_err("[CMC623:%s:ERROR] : Can't writing value while cmc623 suspend \n",__func__);
        cmc623_state.cabc_mode = value;
        return size;
    }

    cabc_onoff_ctrl(value);
//    set_backlight_pwm(cmc623_state.brightness);
    return size;
}

static DEVICE_ATTR(mdnie_cabc, 0664, mdnie_cabc_show,mdnie_cabc_store);



/* ##########################################################
 * #
 * # LCD POWER on / Off Sysfs node
 * # 
 * ##########################################################*/

static ssize_t lcd_power_file_cmd_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	printk("called %s \n", __func__);
	return sprintf(buf, "%u\n", cmc623_state.suspended);
}



static ssize_t lcd_power_file_cmd_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	int value;

	sscanf(buf, "%d", &value);

	pr_info("[lcd_power] in lcd_power_file_cmd_store, input value = %d \n", value);

	if ((cmc623_state.suspended == TRUE) && (value == 1)) {
		p5lte_cmc623_on(value);
		pr_info("[lcd_power on] <= value : %d \n", value);
		cmc623_state.suspended = FALSE;
	} else if ((cmc623_state.suspended == FALSE) && (value == 0)) {
		p5lte_cmc623_on(value);
		pr_info("[lcd_power off] <= value : %d \n", value);
		cmc623_state.suspended = TRUE;
	} else
		pr_info("[lcd_power] lcd is already = %d \n", cmc623_state.suspended);

	return size;
}

static DEVICE_ATTR(lcd_power, 0664, lcd_power_file_cmd_show, lcd_power_file_cmd_store);

/* ##########################################################
 * #
 * # LCD Type Sysfs node
 * # 
 * ##########################################################*/

#if defined (CONFIG_TARGET_SERIES_P4LTE)
static const char lcdtype_name[][64] = {
		"SEC_LTN101AL03-001",
		};
#else
static const char lcdtype_name[][64] = {
		"SEC_LTN089AL03-802",
		};
#endif

static ssize_t lcdtype_file_cmd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("type: %s\n",lcdtype_name[0]);
	
	return sprintf(buf,lcdtype_name[0]);
}

static DEVICE_ATTR(lcdtype, 0664, lcdtype_file_cmd_show, NULL);

/* ##########################################################
 * #
 * # Scenario change Sysfs node
 * # 
 * ##########################################################*/
static ssize_t mdnie_scenario_show(struct device *dev,
				struct device_attribute *attr, char *buf)

{
    printk("[CMC623:info] : %s called \n",__func__);
    return sprintf(buf,"Current Scenario Mode : %d\n",cmc623_state.scenario);
}


static ssize_t mdnie_scenario_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	int value;

	sscanf(buf, "%d", &value);
	printk("[CMC623:INFO] set scenario mode : %d\n",value);

	if(value < mDNIe_UI_MODE || value >= MAX_mDNIe_MODE){
		printk("[CMC623:ERROR] : wrong scenario mode value : %d\n",value);
		return size;
	}

	if (cmc623_state.suspended == TRUE) {
		cmc623_state.scenario = value;
		return size;
	}

	ret = apply_main_tune_value(value, cmc623_state.background, cmc623_state.cabc_mode, 0);
	if (ret != 0) {
		printk("[CMC623:ERROR] ERROR : set main tune value faild \n");
	}

	return size;
}
static DEVICE_ATTR(scenario, 0664, mdnie_scenario_show, mdnie_scenario_store);

/* ##########################################################
 * #
 * # Tuning Sysfs node
 * # 
 * ##########################################################*/
#define MAX_FILE_NAME 128
static int tuning_enable = 0;
static char tuning_filename[MAX_FILE_NAME];
static ssize_t tuning_show(struct device *dev,
        struct device_attribute *attr, char *buf)

{
    int ret = 0;
    ret = sprintf(buf,"Tunned File Name : %s\n",tuning_filename);

    return ret;
}


static ssize_t tuning_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
    memset(tuning_filename,0,sizeof(tuning_filename));
    sprintf(tuning_filename,"%s%s",TUNING_FILE_PATH,buf);
    
    printk("[CMC623:INFO]:%s:%s\n",__func__,tuning_filename);

    if (load_tuning_data(tuning_filename) <= 0) {
        printk("[CMC623:ERROR]:load_tunig_data() failed\n");
        return size;
    }
    tuning_enable = 1;
    return size;
}

static DEVICE_ATTR(tuning, 0664, tuning_show, tuning_store);

/* ##########################################################
 * #
 * # MDNIE OVE Sysfs node
 * # 
 * ##########################################################*/
static ssize_t mdnie_ove_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    return sprintf(buf,"Current OVE Value : %s\n",(cmc623_state.ove==0) ? "Disabled" : "Enabled");
}


static ssize_t mdnie_ove_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
    int ret;
    int value;
    
	sscanf(buf, "%d", &value);
    printk("[CMC623:INFO] set ove : %d\n",value);

    if(value < OUTDOOR_OFF_MODE || value >= MAX_OUTDOOR_MODE) {
        printk("[CMC623:ERROR] : wrong ove mode value : %d\n",value);
        return size;
    }

    if (cmc623_state.suspended == TRUE) {
        cmc623_state.ove = value;
        return size;
    }
    
    ret = apply_sub_tune_value(cmc623_state.temperature, value, cmc623_state.cabc_mode, 0);
    if (ret != 0) {
        printk("[CMC623:ERROR] ERROR : set sub tune value faild \n");
    }
    
    return size;
}

static DEVICE_ATTR(outdoor, 0664, mdnie_ove_show, mdnie_ove_store);

/* ##########################################################
 * #
 * # MDNIE Temprature Sysfs node
 * # 
 * ##########################################################*/
static ssize_t mdnie_temp_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    const char temp_name[MAX_TEMP_MODE][16] = {
        "STANDARD",
        "WARM",
        "COLD",
    };

    if(cmc623_state.temperature >= MAX_TEMP_MODE) {
        printk("[CMC623:ERROR] : wrong color temperature mode value : %d\n",cmc623_state.temperature);
        return 0; 
    }   

    return sprintf(buf,"Current Color Temperature Mode : %s\n",temp_name[cmc623_state.temperature]);
}


static ssize_t mdnie_temp_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
    int ret;
    int value;
	
	sscanf(buf, "%d", &value);
    printk("[CMC623:INFO] set color temperature : %d\n",value);
    
    if(value < TEMP_STANDARD ||value >= MAX_TEMP_MODE){
        printk("[CMC623:ERROR] : wrong color temperature mode value : %d\n",value);
        return size;
    }

    if (cmc623_state.suspended == TRUE) {
        cmc623_state.temperature = value;
        return size;
    }

    ret = apply_sub_tune_value(value, cmc623_state.ove, cmc623_state.cabc_mode ,0);
    if (ret != 0) {
        printk("[CMC623:ERROR] ERROR : set sub tune value faild \n");
    }

#if 0     
    cmc623_state.temperature = value;

    printk("[CMC623:INFO] Current Sub tunning : %s\n",
        sub_tuning_value[cmc623_state.temperature][cmc623_state.ove].tuning[cmc623_state.cabc_mode].name);
#endif     
    return size;
}

static DEVICE_ATTR(mdnie_temp, 0664, mdnie_temp_show, mdnie_temp_store);

/* ##########################################################
 * #
 * # MDNIE BG Sysfs node
 * # 
 * ##########################################################*/
static ssize_t mdnie_bg_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    const char background_name[MAX_BACKGROUND_MODE][16] = {
        "STANDARD",
        "DYNAMIC",
        "MOVIE",
    };

    if(cmc623_state.background >= MAX_BACKGROUND_MODE) {
        printk("[CMC623:ERROR] : Undefined Background Mode : %d\n",cmc623_state.background);
        return 0; 
    }   
	printk("%s, cmc623_state.backgroudn : %d\n",__func__,cmc623_state.background);
    return sprintf(buf,"Current Background Mode : %s\n",background_name[cmc623_state.background]);
}


static ssize_t mdnie_bg_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
    int ret;
    int value;
	
	sscanf(buf, "%d", &value);
    printk("[CMC623:INFO] set background mode : %d\n",value);
    
    if(value < DYNAMIC_MODE || value >= MAX_BACKGROUND_MODE){
        printk("[CMC623:ERROR] : wrong backgound mode value : %d\n",value);
        return size;
    }

    if (cmc623_state.suspended == TRUE) {
        cmc623_state.background = value;
        return size;
    }

    ret = apply_main_tune_value(cmc623_state.scenario, value, cmc623_state.cabc_mode, 0);
    if (ret != 0) {
        printk("[CMC623:ERROR] ERROR : set main tune value faild \n");
    }

    return size;
}

static DEVICE_ATTR(mode, 0664, mdnie_bg_show, mdnie_bg_store);


#if defined(CONFIG_TARGET_LOCALE_KOR_SKT) || defined(CONFIG_TARGET_LOCALE_KOR_LGU)
static ssize_t mdnie_roi_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	printk("called %s \n",__func__);

	return sprintf(buf,"%u\n",0);
}

static ssize_t mdnie_roi_show_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
	int value,x1,y1,x2,y2;

	printk(KERN_INFO "[mdnie] %s called\n", __func__);
	
 	sscanf(buf, "%d%d%d%d%d", &value,&x1,&x2,&y1,&y2);

	printk(KERN_INFO "[mdnie] region:%d  x:%d~%d y:%d~%d\n", value, x1, x2, y1, y2);

	cmc623_Set_Region_Ext(value,x1,x2,y1,y2);
			
	return size;
}

static DEVICE_ATTR(mdnie_roi,0664, mdnie_roi_show, mdnie_roi_show_store);

#endif


#if DUMP_CMC623_REGISTER
/* ##########################################################
 * #
 * # MDNIE DUMP Sysfs node
 * #   - dump cmc623 register
 * ##########################################################*/
static ssize_t mdnie_dump_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	printk("##################################\n");
	printk(" CMC623 Register DUMP\n");
	printk("##################################\n");

	dump_cmc623_register();

    return 0;
}


static ssize_t mdnie_dump_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
    int ret;
    int value;
    
    printk("[CMC623:INFO] dump\n");

    return size;
}

static DEVICE_ATTR(mdnie_dump, 0664, mdnie_dump_show, mdnie_dump_store);
#endif

int cmc623_sysfs_init(void)
{

	int ret = 0;
	/*  1. CLASS Create
	 *  2. Device Create
	 *  3. node create
	 *   - bypass on/off node
	 *   - cabc on/off node
	 *   - lcd_power node
	 *   - scenario node
	 *   - tuning node
	 *   - mdnie_ove node
	 *   - mdnie_bg node*/
	mdnie_class = class_create(THIS_MODULE, "mdnie");
	if(IS_ERR(mdnie_class)) {
		printk("Failed to create class(mdnie_class)!!\n");
		ret = -1;
	}

	tune_cmc623_dev = device_create(mdnie_class, NULL, 0, NULL, "mdnie");
	if(IS_ERR(tune_cmc623_dev)) {
		printk("Failed to create device(tune_cmc623_dev)!!");
		ret = -1;
	}

	if (device_create_file(tune_cmc623_dev, &dev_attr_bypassonoff) < 0) {
		printk("Failed to create device file!(%s)!\n", dev_attr_bypassonoff.attr.name);
		ret = -1;
	}
	if (device_create_file(tune_cmc623_dev, &dev_attr_mdnie_cabc) < 0) {
		pr_err("Failed to create device file!(%s)!\n",
			dev_attr_mdnie_cabc.attr.name);
		ret = -1;
	}
	if (device_create_file(tune_cmc623_dev, &dev_attr_lcd_power) < 0) {
		pr_err("Failed to create device file!(%s)!\n", dev_attr_lcd_power.attr.name);
		ret = -1;
	}
	if (device_create_file(tune_cmc623_dev, &dev_attr_lcdtype) < 0) {
		pr_err("Failed to create device file!(%s)!\n", dev_attr_lcdtype.attr.name);
		ret = -1;
	}
    if (device_create_file(tune_cmc623_dev, &dev_attr_scenario) < 0) {
        pr_err("Failed to create device file!(%s)!\n", dev_attr_scenario.attr.name);
		ret = -1;
    }
	 if (device_create_file(tune_cmc623_dev, &dev_attr_tuning) < 0) {
        pr_err("Failed to create device file(%s)!\n",dev_attr_tuning.attr.name);
        ret = -1;
    }
	if (device_create_file(tune_cmc623_dev, &dev_attr_outdoor) < 0) {
        printk("[CMC623:ERROR] device_crate_filed(%s) \n", dev_attr_outdoor.attr.name);
        ret = -1;
    }

    if(device_create_file(tune_cmc623_dev, &dev_attr_mdnie_temp) < 0) {
        printk("[CMC623:ERROR] device_crate_filed(%s) \n", dev_attr_mdnie_temp.attr.name);
        ret = -1;
    }

    if(device_create_file(tune_cmc623_dev, &dev_attr_mode) < 0) {
        printk("[CMC623:ERROR] device_crate_filed(%s) \n", dev_attr_mode.attr.name);
        ret = -1;
    }
#if defined(CONFIG_TARGET_LOCALE_KOR_SKT)
		if (device_create_file(tune_cmc623_dev, &dev_attr_mdnie_roi) < 0){
		pr_err("[CMC623:ERROR] device_crate_filed(%s) \n", dev_attr_mdnie_roi.attr.name);
		 ret = -1;
		}
#endif
	
#if DUMP_CMC623_REGISTER
	if(device_create_file(tune_cmc623_dev, &dev_attr_mdnie_dump) < 0) {
        printk("[CMC623:ERROR] device_crate_filed(%s) \n", dev_attr_mdnie_dump.attr.name);
        ret = -1;
    }
#endif
	return 0;

}
