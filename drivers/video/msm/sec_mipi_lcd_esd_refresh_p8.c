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

#define __ASM_ARCH_SEC_MIPI_LCD_ESD_FRESH_C

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
#include <linux/video/sec_mipi_lcd_esd_refresh_p8.h>

#include "msm_fb.h"
#include "mipi_dsi.h"

#define DET_CHECK_TIME_MS	50		
#define WAKE_LOCK_TIME		(HZ /2)	/* 0.5 sec */

int	use_vsyncLPmode = FALSE;

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

static struct sec_esd_info *p_sec_esd_info = NULL;

void set_lcd_esd_ignore( boolean src );
int sec_esd_enable(void);
int sec_esd_disable(void);
	

#if defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K)
#define ESD_EXCEPT_CNT (1)
#else
#define ESD_EXCEPT_CNT (0)
#endif 

void set_lcd_esd_ignore( boolean src )
{
	if( p_sec_esd_info == NULL ) {
		DPRINT( "%s : not initialized\n", __func__ );
		return;
	}

	p_sec_esd_info->esd_ignore = src;
//	DPRINT( "%s : %d\n", __func__, p_sec_esd_info->esd_ignore );
}	

extern int state_lcd_on;
extern struct platform_device *pdev_temp;//mipi_dsi_p8.c
extern int mipi_dsi_off(struct platform_device *pdev);
extern int mipi_dsi_on(struct platform_device *pdev);

extern int lcd_esd_refresh; // backlight flag (mipi_s6e8ab0_wxga.c)
int test_irq_num = 0;
void lcd_esd_seq( struct sec_esd_info *pSrc )
{
	uint32 dsi_lane_ctrl, dsi_video_mode_ctrl;
	struct sec_esd_info *pESD;

	if( pSrc )	pESD = pSrc;
	else pESD = p_sec_esd_info;

	DPRINT("%s : %d\n", __func__, (pESD?pESD->esd_cnt:-1) );

if(state_lcd_on)
{

	if(p_sec_esd_info->pdata->esd_int == test_irq_num){printk("LCD RESET : detect cmc624 EXT-INT(esd)\n");}
	else	 if(p_sec_esd_info->pdata->esd_vgh == test_irq_num){printk("LCD RESET : detect LCD VGH EXT-INT(esd)\n");}

	lcd_esd_refresh = 1;	
	mipi_dsi_off(pdev_temp);
	mipi_dsi_on(pdev_temp);
	lcd_esd_refresh = 0;		

/*
	dsi_lane_ctrl = MIPI_INP(MIPI_DSI_BASE + 0x00A8);
	dsi_video_mode_ctrl = MIPI_INP(MIPI_DSI_BASE + 0x00C);
	MIPI_OUTP( MIPI_DSI_BASE + 0x000C, dsi_video_mode_ctrl | 0x11110000 ); // PULSE_MODE_OPT + HSA_PWR_MODE
	MIPI_OUTP( MIPI_DSI_BASE + 0x00A8, dsi_lane_ctrl &0x0FFFFFFF );
	wmb();
	msleep(3);
	MIPI_OUTP( MIPI_DSI_BASE + 0x00A8, dsi_lane_ctrl );
	MIPI_OUTP( MIPI_DSI_BASE + 0x000C, dsi_video_mode_ctrl  );
	wmb();

	msleep(1000);
	
	dsi_lane_ctrl = MIPI_INP(MIPI_DSI_BASE + 0x00A8);
	dsi_video_mode_ctrl = MIPI_INP(MIPI_DSI_BASE + 0x00C);
	MIPI_OUTP( MIPI_DSI_BASE + 0x000C, dsi_video_mode_ctrl | 0x11110000 ); // PULSE_MODE_OPT + HSA_PWR_MODE
	MIPI_OUTP( MIPI_DSI_BASE + 0x00A8, dsi_lane_ctrl &0x0FFFFFFF );
	wmb();
	msleep(3);
	MIPI_OUTP( MIPI_DSI_BASE + 0x00A8, dsi_lane_ctrl );
	MIPI_OUTP( MIPI_DSI_BASE + 0x000C, dsi_video_mode_ctrl  );
	wmb();

	msleep(2000);
	
	dsi_lane_ctrl = MIPI_INP(MIPI_DSI_BASE + 0x00A8);
	dsi_video_mode_ctrl = MIPI_INP(MIPI_DSI_BASE + 0x00C);
	MIPI_OUTP( MIPI_DSI_BASE + 0x000C, dsi_video_mode_ctrl | 0x11110000 ); // PULSE_MODE_OPT + HSA_PWR_MODE
	MIPI_OUTP( MIPI_DSI_BASE + 0x00A8, dsi_lane_ctrl &0x0FFFFFFF );
	wmb();
	msleep(3);
	MIPI_OUTP( MIPI_DSI_BASE + 0x00A8, dsi_lane_ctrl );
	MIPI_OUTP( MIPI_DSI_BASE + 0x000C, dsi_video_mode_ctrl  );
	wmb();
	*/
}
}


static irqreturn_t sec_esd_irq_handler(int irq, void *handle)
{
	struct sec_esd_info *hi = (struct sec_esd_info *)handle;
	test_irq_num = irq;
	DPRINT( "%s\n", __func__);
	disable_irq_nosync(hi->pdata->esd_int);
	disable_irq_nosync(hi->pdata->esd_vgh);	
	schedule_work(&hi->det_work);
	
	return IRQ_HANDLED;
}

static void sec_esd_work_func(struct work_struct *work)
{
	struct sec_esd_info *hi =
		container_of(work, struct sec_esd_info, det_work);
//	struct sec_esd_platform_data *pdata = hi->pdata;

	//DPRINT( "%s\n", __func__);
	//printk("interrupt ------------------------------");

	hi->esd_cnt++;
	if( hi->esd_cnt <= ESD_EXCEPT_CNT ) DPRINT( "%s : %d ignore Cnt(%d)\n", __func__, hi->esd_cnt, ESD_EXCEPT_CNT );
	else if(hi->esd_ignore) DPRINT( "%s : %d ignore FLAG\n", __func__, hi->esd_cnt );
	else 
	{
	/* threaded irq can sleep */
		wake_lock_timeout(&hi->det_wake_lock, WAKE_LOCK_TIME);
		hi->esd_ignore = TRUE;
		lcd_esd_seq(hi);
		hi->esd_ignore = FALSE; // in this block, esd_ignore is must be off 
	}
	
	enable_irq(hi->pdata->esd_int);
	enable_irq(hi->pdata->esd_vgh);	
	return;
}


static int sec_esd_probe(struct platform_device *pdev)
{
	struct sec_esd_info *hi;
	struct sec_esd_platform_data *pdata = pdev->dev.platform_data;
	int ret;

	DPRINT( "%s : Registering esd driver\n", __func__);
	if (!pdata) {
		DPRINT("%s : pdata is NULL.\n", __func__);
		return -ENODEV;
	}

	hi = kzalloc(sizeof(struct sec_esd_info), GFP_KERNEL);
	if (hi == NULL) {
		DPRINT("%s : Failed to allocate memory.\n", __func__);
		return -ENOMEM;
	}

	p_sec_esd_info = hi;
	hi->pdata = pdata;
	hi->esd_cnt = 0;
	hi->esd_ignore = FALSE;

	wake_lock_init(&hi->det_wake_lock, WAKE_LOCK_SUSPEND, "sec_mipi_lcd_esd_det");

	INIT_WORK(&hi->det_work, sec_esd_work_func);
	
	ret = request_threaded_irq(pdata->esd_int, NULL,
			sec_esd_irq_handler,
			IRQF_TRIGGER_RISING |
			IRQF_ONESHOT, "esd_detect", hi);//IRQF_ONESHOT
	if (ret) {
		pr_err("%s : Failed to request_irq.\n", __func__);
		goto err_request_detect_irq;
	}
				
	ret = request_threaded_irq(pdata->esd_vgh, NULL,
			sec_esd_irq_handler,
			IRQF_TRIGGER_FALLING |
			IRQF_ONESHOT, "esd_detect2", hi);//IRQF_ONESHOT
	if (ret) {
		pr_err("%s : Failed to request_irq.\n", __func__);
		goto err_request_detect_irq;
	}
	
	disable_irq_nosync(p_sec_esd_info->pdata->esd_int);
	disable_irq_nosync(p_sec_esd_info->pdata->esd_vgh);
	dev_set_drvdata(&pdev->dev, hi);

	return 0;

#if 0 
err_enable_irq_wake:
	free_irq(pdata->esd_int, hi);
#endif 
err_request_detect_irq:
	wake_lock_destroy(&hi->det_wake_lock);
	kfree(hi);

	return ret;
}

int sec_esd_disable(void)
{
	printk("lcd esd INT : disable!\n");
	disable_irq_nosync(p_sec_esd_info->pdata->esd_int);
	disable_irq_nosync(p_sec_esd_info->pdata->esd_vgh);	
}

int sec_esd_enable(void)
{
	printk("lcd esd INT : enable!\n");
	enable_irq(p_sec_esd_info->pdata->esd_int);
	enable_irq(p_sec_esd_info->pdata->esd_vgh);	
}

EXPORT_SYMBOL(sec_esd_disable);
EXPORT_SYMBOL(sec_esd_enable);

static int sec_esd_remove(struct platform_device *pdev)
{

	struct sec_esd_info *hi = dev_get_drvdata(&pdev->dev);

	DPRINT( "%s :\n", __func__);

	free_irq(hi->pdata->esd_int, hi);
	disable_irq_wake(hi->pdata->esd_int);
	free_irq(hi->pdata->esd_vgh, hi);
	disable_irq_wake(hi->pdata->esd_vgh);	
	wake_lock_destroy(&hi->det_wake_lock);

	kfree(hi);

	return 0;
}

static struct platform_driver sec_esd_driver = {
	.probe = sec_esd_probe,
	.remove = sec_esd_remove,
	.driver = {
		.name = SEC_MIPI_LCD_ESD_REFRESH_MODULE_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init sec_esd_init(void)
{
	return platform_driver_register(&sec_esd_driver);
}

static void __exit sec_esd_exit(void)
{
	platform_driver_unregister(&sec_esd_driver);
}

module_init(sec_esd_init);
module_exit(sec_esd_exit);

MODULE_AUTHOR("kiyong.nam@samsung.com");
MODULE_DESCRIPTION("Samsung Electronics Corp ESD(Mipi-LCD) detection driver");
MODULE_LICENSE("GPL");
