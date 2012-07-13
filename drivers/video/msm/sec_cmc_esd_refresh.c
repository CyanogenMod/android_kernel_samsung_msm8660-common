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

/*
extern int state_lcd_on; //mipi_s6e8ab0_wxga.c
extern int cmc624_I2cRead16(u8 reg, u16 *val); // Pxlte_cmc624.c
extern bool cmc624_I2cWrite16(unsigned char Addr, unsigned long Data);// Pxlte_cmc624.c


extern int mipi_dsi_off(struct platform_device *pdev);//mipi_dsi_p8.c
extern int mipi_dsi_on(struct platform_device *pdev);//mipi_dsi_p8.c
extern struct platform_device *pdev_temp;//mipi_dsi_p8.c


int cmc_timer_esd_refresh = 0;
int error_count = 0;
void cmc_lcd_esd_seq( struct sec_esd_info *pSrc )
{
	unsigned int adc_result = 0;
	u16 compare_data[4] = {0x0000, 0x0914, 0x8000, 0x0000};
	u8 cmc_read_add[4] = {0x82, 0x83, 0xc2, 0xc3};
	u16 data[4] = {0,};
	int i;
	
//	DPRINT("%s : %d\n", __func__, pSrc->esd_cnt);

	if(state_lcd_on)
	{

		for(i = 0 ; i < 4 ; i++)
		{
			cmc624_I2cRead16(cmc_read_add[i], data+i);		
	//		printk("cmc624 add 0x%x = 0x%x\n",cmc_read_add[i], data[i]);
			if(compare_data[i] != data[i])
			{
				if(error_count>2) // 6sec
				{
					printk("lcd reset - cmc esd timer, count : %d : \n", error_count);
					cmc_timer_esd_refresh = 1;	
					mipi_dsi_off(pdev_temp);
					mipi_dsi_on(pdev_temp);
					cmc_timer_esd_refresh = 0;						
				}
				else
				{
					error_count++;
					printk("cmc esd timer - count : %d\n", error_count);
				}

				break; 
			}
			else error_count = 0;
				
		}
	}

}

*/

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
	cmc_lcd_esd_seq(hi);
	hi->esd_ignore = FALSE; // in this block, esd_ignore is must be off 

	return;
}




struct sec_esd_info *temp_hi;//huggac
unsigned int count = 0;
struct timer_list esd_timer;
static void esd_init_func(unsigned long data)
{

   struct sec_esd_info *hi = (struct sec_esd_info *)temp_hi;
   
    esd_timer.expires = jiffies + (HZ*3);// interrupt to 3/Hz
    add_timer(&esd_timer);

    schedule_work(&hi->det_work);

}


static void esd_detect_timer()
{
 	init_timer(&esd_timer); 			 // init kernel timer_list
	esd_timer.expires = jiffies + HZ;
	esd_timer.data = 0;
	esd_timer.function = &esd_init_func;
 
	add_timer(&esd_timer);				 // function add in kernel timer
//	del_timer_sync(&esd_timer);
}



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

	hi->pdata = pdata;
	hi->esd_cnt = 0;
	hi->esd_ignore = FALSE;

	wake_lock_init(&hi->det_wake_lock, WAKE_LOCK_SUSPEND, "cmc_lcd_esd_det");
	INIT_WORK(&hi->det_work, cmc_esd_work_func);

	 temp_hi = hi;
	// esd_detect_timer();
	//dev_set_drvdata(&pdev->dev, hi);
	return 0;
/*
#if 0 
err_enable_irq_wake:
	free_irq(pdata->esd_int, hi);
#endif 
err_request_detect_irq:
	wake_lock_destroy(&hi->det_wake_lock);
	kfree(hi);

	return ret;*/
}


void cmc_esd_disable(void)
{
	int del_error;
	del_error = del_timer(&esd_timer);
	if(!del_error)printk("%s : failed to delete - cmc esd timer!!", __func__);
	else printk("cmc esd timer disable!\n");
}


void cmc_esd_enable(void)
{
	esd_detect_timer();
	printk("cmc esd timer enable!\n");
}



static int cmc_esd_remove(struct platform_device *pdev)
{

	struct sec_esd_info *hi = dev_get_drvdata(&pdev->dev);

	DPRINT( "%s :\n", __func__);

	wake_lock_destroy(&hi->det_wake_lock);
	del_timer_sync(&esd_timer);
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

