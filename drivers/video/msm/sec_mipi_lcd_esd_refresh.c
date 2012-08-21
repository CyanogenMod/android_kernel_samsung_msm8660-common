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
#include <linux/video/sec_mipi_lcd_esd_refresh.h>

#include "msm_fb.h"
#include "mipi_dsi.h"

#define DET_CHECK_TIME_MS	50		
#if defined (CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)  //except celoxhd
#define WAKE_LOCK_TIME		(6 * HZ)	/* 0.5 sec */

#else
#define WAKE_LOCK_TIME		(HZ /2)	/* 0.5 sec */
#endif
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
	boolean esd_force_ignore;
};
static struct sec_esd_info *p_sec_esd_info = NULL;

int	use_vsyncLPmode = FALSE;

void set_lcd_esd_ignore( boolean src );

#if defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K) || defined (CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
#define ESD_EXCEPT_CNT (1)
#else
#define ESD_EXCEPT_CNT (0)
#endif 

void set_lcd_esd_ignore( boolean src )
{
	use_vsyncLPmode = FALSE;
	
	if( p_sec_esd_info == NULL ) {
		DPRINT( "%s : not initialized\n", __func__ );
		return;
	}

	p_sec_esd_info->esd_ignore = src;
//	DPRINT( "%s : %d\n", __func__, p_sec_esd_info->esd_ignore );
}	
void set_lcd_esd_forced_ignore( boolean src )
{
	use_vsyncLPmode = FALSE;
	
	if( p_sec_esd_info == NULL ) {
		DPRINT( "%s : not initialized\n", __func__ );
		return;
	}

	p_sec_esd_info->esd_force_ignore = src;
	DPRINT( "%s : %d\n", __func__, p_sec_esd_info->esd_force_ignore );
}	


#if defined(CONFIG_FB_MSM_MIPI_S6E8AA0_WXGA_Q1_PANEL) || defined(CONFIG_FB_MSM_MIPI_S6E8AB0_WXGA_PANEL)

void lcd_esd_seq( struct sec_esd_info *pSrc )
{
	uint32 dsi_lane_ctrl, dsi_video_mode_ctrl;
	struct sec_esd_info *pESD;

	if( pSrc )	pESD = pSrc;
	else pESD = p_sec_esd_info;

	DPRINT("%s : %d\n", __func__, (pESD?pESD->esd_cnt:-1) );

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
}

#else
extern void TSP_ESD_seq(void);

void lcd_LP11_signal( void )
{
	uint32 dsi_lane_ctrl, dsi_video_mode_ctrl;

	use_vsyncLPmode = FALSE;
	dsi_lane_ctrl = MIPI_INP(MIPI_DSI_BASE + 0x00A8);
	dsi_video_mode_ctrl = MIPI_INP(MIPI_DSI_BASE + 0x00C);
	MIPI_OUTP( MIPI_DSI_BASE + 0x000C, dsi_video_mode_ctrl | 0x11110000 ); // PULSE_MODE_OPT + HSA_PWR_MODE
	MIPI_OUTP( MIPI_DSI_BASE + 0x00A8, dsi_lane_ctrl &0x0FFFFFFF ); // generate LP11 when Vsync, Hsync 
	wmb();
	msleep(3);
	MIPI_OUTP( MIPI_DSI_BASE + 0x00A8, dsi_lane_ctrl );
	MIPI_OUTP( MIPI_DSI_BASE + 0x000C, dsi_video_mode_ctrl  );
	wmb();
}

void lcd_esd_seq( struct sec_esd_info *pSrc )
{
	struct sec_esd_info *pESD;

	if( pSrc )	pESD = pSrc;
	else pESD = p_sec_esd_info;

	DPRINT("%s : %d\n", __func__, (pESD?pESD->esd_cnt:-1) );

	lcd_LP11_signal();
	use_vsyncLPmode = TRUE;
	TSP_ESD_seq();
#if !defined (CONFIG_USA_MODEL_SGH_I757) && !defined(CONFIG_CAN_MODEL_SGH_I757M)  //except celoxhd
	msleep(1000);
	
	lcd_LP11_signal();
	use_vsyncLPmode = TRUE;
	TSP_ESD_seq();
	msleep(2000);
	
	lcd_LP11_signal();
	use_vsyncLPmode = TRUE;
	TSP_ESD_seq();

#endif

	use_vsyncLPmode = TRUE; // in this code, LCD must be hit by ESD
}

#endif 

static irqreturn_t sec_esd_irq_handler(int irq, void *handle)
{
	struct sec_esd_info *hi = (struct sec_esd_info *)handle;

//	DPRINT( "%s\n", __func__);

	disable_irq_nosync(hi->pdata->esd_int);
	schedule_work(&hi->det_work);
	
	use_vsyncLPmode = TRUE;

	return IRQ_HANDLED;
}

static void sec_esd_work_func(struct work_struct *work)
{
	struct sec_esd_info *hi =
		container_of(work, struct sec_esd_info, det_work);
//	struct sec_esd_platform_data *pdata = hi->pdata;

	DPRINT( "%s\n", __func__);

	hi->esd_cnt++;
	if( hi->esd_cnt <= ESD_EXCEPT_CNT ) DPRINT( "%s : %d ignore Cnt(%d)\n", __func__, hi->esd_cnt, ESD_EXCEPT_CNT );
	else if(hi->esd_ignore) DPRINT( "%s : %d ignore FLAG\n", __func__, hi->esd_cnt );
	else if(hi->esd_force_ignore) DPRINT( "%s : %d esd_force_ignore FLAG\n", __func__, hi->esd_force_ignore );
	else 
	{
	/* threaded irq can sleep */
	wake_lock_timeout(&hi->det_wake_lock, WAKE_LOCK_TIME);
		hi->esd_ignore = TRUE;
		lcd_esd_seq(hi);
		hi->esd_ignore = FALSE; // in this block, esd_ignore is must be off 
	}
	
	enable_irq(hi->pdata->esd_int);
	return;
}

#if defined (CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_JPN_MODEL_SC_05D)
extern unsigned int get_hw_rev();
#endif

static int sec_esd_probe(struct platform_device *pdev)
{
	struct sec_esd_info *hi;
	struct sec_esd_platform_data *pdata = pdev->dev.platform_data;
	int ret;
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_JPN_MODEL_SC_05D)
	if( get_hw_rev() < 0x05 ){
		DPRINT( "%s : Esd driver END (HW REV < 05)\n", __func__);
		return 0;
	}
#endif		

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
//			IRQF_TRIGGER_RISING |
			IRQF_TRIGGER_FALLING |
			IRQF_ONESHOT, "esd_detect", hi);

	if (ret) {
		pr_err("%s : Failed to request_irq.\n", __func__);
		goto err_request_detect_irq;
	}

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

static int sec_esd_remove(struct platform_device *pdev)
{

	struct sec_esd_info *hi = dev_get_drvdata(&pdev->dev);

	DPRINT( "%s :\n", __func__);

	free_irq(hi->pdata->esd_int, hi);
	disable_irq_wake(hi->pdata->esd_int);
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
