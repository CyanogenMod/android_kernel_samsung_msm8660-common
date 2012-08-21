 /* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#ifndef __ARCH_ARM_MACH_MSM_DEVICES_MSM8X60_H
#define __ARCH_ARM_MACH_MSM_DEVICES_MSM8X60_H

#define MSM_GSBI3_QUP_I2C_BUS_ID 0
#define MSM_GSBI4_QUP_I2C_BUS_ID 1
#define MSM_GSBI9_QUP_I2C_BUS_ID 2
#define MSM_GSBI8_QUP_I2C_BUS_ID 3
#define MSM_GSBI7_QUP_I2C_BUS_ID 4
#define MSM_GSBI12_QUP_I2C_BUS_ID 5
#define MSM_SSBI1_I2C_BUS_ID     6
#define MSM_SSBI2_I2C_BUS_ID     7
#define MSM_SSBI3_I2C_BUS_ID     8
#define MSM_AMP_I2C_BUS_ID        9
#define MSM_OPT_I2C_BUS_ID		10
#define MSM_GYRO_I2C_BUS_ID		11
#define MSM_MAG_I2C_BUS_ID		12
#define MSM_TKEY_I2C_BUS_ID		13

#if defined(CONFIG_PN544_NFC)
#define MSM_GSBI10_QUP_I2C_BUS_ID		14
#endif
#if defined(CONFIG_BATTERY_MAX17040) || defined(CONFIG_CHARGER_SMB328A)
#define MSM_FG_SMB_I2C_BUS_ID		15
#endif
#if defined(CONFIG_SAMSUNG_8X60_TABLET) && defined(CONFIG_BATTERY_MAX17042)
#define MSM_FG_I2C_BUS_ID		15
#endif
#ifdef CONFIG_VP_A2220
#define MSM_A2220_I2C_BUS_ID		16	
#endif

#if defined(CONFIG_SAMSUNG_8X60_TABLET) && defined (CONFIG_CMC623_P5LTE)
#define MSM_CMC623_I2C_BUS_ID   17
#endif
#if defined(CONFIG_SAMSUNG_8X60_TABLET) && defined (CONFIG_CMC624_P8LTE)
#define MSM_CMC624_I2C_BUS_ID   17
#endif

#define MSM_MOTOR_I2C_BUS_ID		17

#if defined (CONFIG_EPEN_WACOM_G5SP)
#define MSM_GSBI11_QUP_I2C_BUS_ID	18
#endif 

#if defined(CONFIG_SAMSUNG_8X60_TABLET) && defined (CONFIG_STMPE811_ADC)
#define MSM_STMPE811_I2C_BUS_ID		19
#endif

#ifdef CONFIG_SPI_QUP
extern struct platform_device msm_gsbi1_qup_spi_device;
extern struct platform_device msm_gsbi10_qup_spi_device;
#endif

#ifdef CONFIG_MSM_BUS_SCALING
extern struct platform_device msm_bus_apps_fabric;
extern struct platform_device msm_bus_sys_fabric;
extern struct platform_device msm_bus_mm_fabric;
extern struct platform_device msm_bus_sys_fpb;
extern struct platform_device msm_bus_cpss_fpb;
#endif

extern struct platform_device msm_device_smd;
extern struct platform_device msm_kgsl_3d0;
#ifdef CONFIG_MSM_KGSL_2D
extern struct platform_device msm_kgsl_2d0;
extern struct platform_device msm_kgsl_2d1;
#endif
extern struct platform_device msm_device_gpio;
extern struct platform_device msm_device_vidc;

extern struct platform_device msm_charm_modem;

#ifdef CONFIG_HW_RANDOM_MSM
extern struct platform_device msm_device_rng;
#endif

#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
/* soonyong.cho : Define samsung product id and config string.
 *                Sources such as 'android.c' and 'devs.c' refered below define
 */
#define SAMSUNG_VENDOR_ID		0x04e8

#ifdef CONFIG_USB_ANDROID_SAMSUNG_ESCAPE
	/* USE DEVGURU HOST DRIVER */
	/* 0x6860 : MTP(0) + MS Composite (UMS) */
	/* 0x685E : UMS(0) + MS Composite (ADB) */
#define SAMSUNG_KIES_PRODUCT_ID		0x685E	/* UMS(0) + MS Composite */
#define SAMSUNG_DEBUG_PRODUCT_ID	0x685E	/* UMS(0) + MS Composite (ADB) */
#define SAMSUNG_UMS_PRODUCT_ID		0x685B  /* UMS Only */
#define SAMSUNG_MTP_PRODUCT_ID		0x685C  /* MTP Only */
#ifdef CONFIG_USB_ANDROID_SAMSUNG_RNDIS_WITH_MS_COMPOSITE
#define SAMSUNG_RNDIS_PRODUCT_ID	0x6861  /* RNDIS(0,1) + UMS (2) + MS Composite */
#else
#define SAMSUNG_RNDIS_PRODUCT_ID	0x6863  /* RNDIS only */
#endif
#else 
	/* USE MCCI HOST DRIVER */
#define SAMSUNG_KIES_PRODUCT_ID		0x6877	/* Shrewbury ACM+MTP*/
#define SAMSUNG_DEBUG_PRODUCT_ID	0x681C	/* Shrewbury ACM+UMS+ADB*/
#define SAMSUNG_UMS_PRODUCT_ID		0x681D
#define SAMSUNG_MTP_PRODUCT_ID		0x68A9
#define SAMSUNG_RNDIS_PRODUCT_ID	0x6881
#endif
#define ANDROID_DEBUG_CONFIG_STRING	 "ACM + UMS + ADB (Debugging mode)"
#define ANDROID_KIES_CONFIG_STRING	 "ACM + UMS (SAMSUNG KIES mode)"
#define ANDROID_UMS_CONFIG_STRING	 "UMS Only (Not debugging mode)"
#define ANDROID_MTP_CONFIG_STRING	 "MTP Only (Not debugging mode)"
#ifdef CONFIG_USB_ANDROID_SAMSUNG_RNDIS_WITH_MS_COMPOSITE
#define ANDROID_RNDIS_CONFIG_STRING	 "RNDIS + UMS (Not debugging mode)"
#else
#define ANDROID_RNDIS_CONFIG_STRING	 "RNDIS Only (Not debugging mode)"
#endif
	/* Refered from S1, P1 */
#define USBSTATUS_UMS				0x0
#define USBSTATUS_SAMSUNG_KIES 		0x1
#define USBSTATUS_MTPONLY			0x2
#define USBSTATUS_ASKON				0x4
#define USBSTATUS_VTP				0x8
#define USBSTATUS_ADB				0x10
#define USBSTATUS_DM				0x20
#define USBSTATUS_ACM				0x40
#define USBSTATUS_SAMSUNG_KIES_REAL		0x80
#endif /* CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE */


void __init msm8x60_init_irq(void);
#ifdef CONFIG_MSM_KGSL_2D
void __init msm8x60_check_2d_hardware(void);
#endif

#ifdef CONFIG_MSM_DSPS
extern struct platform_device msm_dsps_device;
#endif

#if defined(CONFIG_MSM_RPM_STATS_LOG)
extern struct platform_device msm_rpm_stat_device;
#endif
#endif
