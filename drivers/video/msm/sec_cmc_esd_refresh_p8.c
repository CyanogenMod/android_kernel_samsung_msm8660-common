/*
 *  headset/ear-jack device detection driver.
 *
 *  Copyright (C) 2010 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

//#define __ASM_ARCH_SEC_MIPI_LCD_ESD_FRESH_C

#include <linux/module.h>
#include <linux/sysdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h> 
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
//#include <linux/video/sec_mipi_lcd_esd_refresh.h>

#include "msm_fb.h"
#include "mipi_dsi.h"

#define DET_CHECK_TIME_MS	50		
#define WAKE_LOCK_TIME		(HZ /2)	/* 0.5 sec */


#if 1 // def LCDC_DEBUG
#define DPRINT(x...)	printk("[Mipi_LCD_ESD] " x)
#else
#define DPRINT(x...)	
#endif

struct sec_esd_info {
	struct sec_esd_platform_data *pdata;
	//struct delayed_work jack_detect_work;
	struct wake_lock det_wake_lock;
	struct work_struct  det_work;
	int esd_cnt;
	boolean esd_ignore;
};


struct sec_esd_info *temp_hi;
unsigned int count = 0;
 struct timer_list *esd_timer = NULL;
extern int lcd_esd_refresh; // backlight flag (mipi_s6e8ab0_wxga.c)

extern void cmc_lcd_esd_seq( struct sec_esd_info *pSrc );

static void cmc_esd_work_func(struct work_struct *work)
{
	struct sec_esd_info *hi =
		container_of(work, struct sec_esd_info, det_work);

//	DPRINT( "%s\n", __func__);

	hi->esd_cnt++;
	/* threaded irq can sleep */
	wake_lock_timeout(&hi->det_wake_lock, WAKE_LOCK_TIME);
	hi->esd_ignore = TRUE;
	
 	lcd_esd_refresh = 1;
	cmc_lcd_esd_seq(hi);
	hi->esd_ignore = FALSE; // in this block, esd_ignore is must be off 
	 lcd_esd_refresh = 0;

	return;
}
 
static void esd_init_func(unsigned long data)
{

   struct sec_esd_info *hi = (struct sec_esd_info *)temp_hi;
   
    esd_timer->expires = jiffies + (HZ*3);// interrupt to 3/Hz
    add_timer(esd_timer);
     schedule_work(&hi->det_work);

}

int esd_timer_flag = 0;

static void esd_detect_timer()
{
	init_timer(esd_timer); 			 // init kernel timer_list
	esd_timer->expires = jiffies + HZ*3;
	esd_timer->data = 0;
	esd_timer->function = &esd_init_func;
 	esd_timer_flag = true;
	
	add_timer(esd_timer);				 // function add in kernel timer
}

void cmc_esd_enable(void);
extern int state_lcd_on;
static int cmc_esd_probe(struct platform_device *pdev)
{
	struct sec_esd_info *hi;
	struct sec_esd_platform_data *pdata = pdev->dev.platform_data;
	int ret;

	DPRINT( "%s : Registering esd timer driver\n", __func__);

	hi = kzalloc(sizeof(struct sec_esd_info), GFP_KERNEL);
	if (hi == NULL) {
		DPRINT("%s : Failed to allocate memory.\n", __func__);
		return -ENOMEM;
	}

	esd_timer = kzalloc(sizeof(struct timer_list), GFP_KERNEL);
	if (esd_timer == NULL) {
		pr_err("%s : Failed to allocate memory.\n", __func__);
		return -ENOMEM;
	}

	hi->pdata = pdata;
	hi->esd_cnt = 0;
	hi->esd_ignore = FALSE;

	wake_lock_init(&hi->det_wake_lock, WAKE_LOCK_SUSPEND, "cmc_lcd_esd_det");
	INIT_WORK(&hi->det_work, cmc_esd_work_func);

	 temp_hi = hi;

	return 0;

}


void cmc_esd_disable(void)
{
	int del_error;

	if (esd_timer == NULL) printk("ESD timer data is NULL!!\n");
	else if(esd_timer->function)
	{
		del_error = del_timer(esd_timer);	
		if(!del_error)printk("%s : failed to delete - cmc esd timer!!", __func__);
		else {
			printk("cmc esd timer : disable!\n");
			esd_timer_flag = false;
			}
	}
}


void cmc_esd_enable(void)
{

	if(esd_timer == NULL){
		printk("ESD timer data is NULL!!\n");
		return;
	}
	else {
		 if(esd_timer_flag) printk("cmc esd timer : already enable!");
		 else {
			esd_detect_timer();	
			printk("cmc esd timer : enable!\n");
		 	}
	}
}



static int cmc_esd_remove(struct platform_device *pdev)
{

	struct sec_esd_info *hi = dev_get_drvdata(&pdev->dev);
	int del_error;
	
	DPRINT( "%s :\n", __func__);

	wake_lock_destroy(&hi->det_wake_lock);
	if (!esd_timer) printk("ESD timer data is NULL!!\n");
	else if(esd_timer->function)
	{
		del_error = del_timer(esd_timer);	
		if(!del_error)printk("%s : failed to delete - cmc esd timer!!", __func__);
		else { printk("cmc esd timer  : remove!\n");
			   esd_timer = NULL;
			}
	}	
	kfree(hi);

	return 0;
}

static struct platform_driver cmc_esd_driver = {
	.probe = cmc_esd_probe,
	.remove = cmc_esd_remove,
	.driver = {
		.name = "cmc_esd_refresh",
		.owner = THIS_MODULE,
	},
};

static int __init cmc_esd_init(void)
{
	return platform_driver_register(&cmc_esd_driver);
}

static void __exit cmc_esd_exit(void)
{
	platform_driver_unregister(&cmc_esd_driver);
}


module_init(cmc_esd_init);
module_exit(cmc_esd_exit);



MODULE_AUTHOR("kiyong.nam@samsung.com");
MODULE_DESCRIPTION("Samsung Electronics Corp ESD(Mipi-LCD) detection driver");
MODULE_LICENSE("GPL");

