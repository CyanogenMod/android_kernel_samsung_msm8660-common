/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
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
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/msm_ssbi.h>
#include <linux/mfd/pmic8058.h>

#include <linux/input/pmic8xxx-keypad.h>
#include <linux/input/pmic8xxx-pwrkey.h>
#include <linux/leds.h>
#include <linux/pmic8058-othc.h>
#include <linux/mfd/pmic8901.h>
#include <linux/regulator/gpio-regulator.h>
#include <linux/regulator/pmic8058-regulator.h>
#include <linux/regulator/pmic8901-regulator.h>
#include <linux/bootmem.h>
#include <linux/pwm.h>
#include <linux/pmic8058-pwm.h>
#include <linux/leds-pmic8058.h>
#include <linux/pmic8058-xoadc.h>
#include <linux/msm_adc.h>
#include <linux/m_adcproc.h>
#include <linux/mfd/marimba.h>
#include <linux/msm-charger.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/i2c/sx150x.h>
#include <linux/smsc911x.h>
#include <linux/spi/spi.h>
#include <linux/input/tdisc_shinetsu.h>
#include <linux/input/cy8c_ts.h>
#include <linux/cyttsp.h>
#include <linux/isa1200_vibrator.h>
#include <linux/dma-mapping.h>
#include <linux/i2c/bq27520.h>
//#include <linux/atmel_mxt1386.h>
#include <mach/subsystem_restart.h>
#include <mach/mdm.h>

#ifdef CONFIG_OPTICAL_BH1721_I957
#include <linux/bh1721.h>
#endif
#ifdef CONFIG_SENSORS_YDA165
#include <linux/i2c/yda165.h>
#endif
#ifdef CONFIG_SENSORS_YDA160
#include <linux/i2c/yda160.h>
#endif
#ifdef CONFIG_SENSORS_MAX9879
#include <linux/i2c/max9879.h>
#endif

#ifdef CONFIG_SENSORS_K3DH_I957
#include <linux/i2c/k3dh.h>
#endif

#ifdef CONFIG_GYRO_K3G_I957
#include <linux/i2c/k3g.h>
#endif

#ifdef CONFIG_SENSORS_AK8975_I957
#include <linux/i2c/ak8975.h>
#endif

#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif

#ifdef CONFIG_BATTERY_MAX17042
#include <linux/power/max17042_battery.h>
#endif

#ifdef CONFIG_BATTERY_P5LTE
#include <linux/power_supply.h>
#include <linux/p5-lte_battery.h>
#endif

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>
#include <asm/hardware/gic.h>

#include <mach/dma.h>
#include <mach/mpp.h>
#include <mach/board.h>
#include <mach/irqs.h>
#include <mach/camera.h>
#include <mach/msm_spi.h>
#include <mach/msm_serial_hs.h>
#include <mach/msm_serial_hs_lite.h>
#include <mach/msm_iomap.h>
#include <mach/msm_memtypes.h>
#include <asm/mach/mmc.h>
#include <mach/msm_battery.h>
#include <mach/msm_hsusb.h>
#include <mach/gpiomux.h>
#ifdef CONFIG_MSM_DSPS
#include <mach/msm_dsps.h>
#endif
#include <mach/msm_xo.h>
#include <mach/msm_bus_board.h>
#include <mach/socinfo.h>
#include <linux/i2c/isl9519.h>
#ifdef CONFIG_USB_G_ANDROID
#include <linux/usb/android.h>
#include <mach/usbdiag.h>
#endif
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <mach/sdio_al.h>
#include <mach/rpm.h>
#include <mach/rpm-regulator.h>
#include <mach/restart.h>
#include <mach/board-msm8660.h>

#include "devices.h"
#include <mach/devices-lte.h>
#include <mach/cpuidle.h>
#include "pm.h"
#include "mpm.h"
#include "spm.h"
#include "rpm_log.h"
#include "timer.h"
#include "gpiomux-8x60.h"
#include "rpm_stats.h"
#include "peripheral-loader.h"
#include <linux/platform_data/qcom_crypto_device.h>
#include "rpm_resources.h"
#include "acpuclock.h"
#include "pm-boot.h"

#include <linux/ion.h>
#include <mach/ion.h>


#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
#include <linux/wlan_plat.h>
#endif

#ifdef CONFIG_SAMSUNG_JACK
#include <linux/sec_jack.h>
#endif
#include <mach/sec_switch.h>
#include <linux/usb/gadget.h>
#include <../../../drivers/bluetooth/bluesleep.c> //SAMSUNG_BT_CONFIG

#ifdef CONFIG_SEC_DEBUG
#include <mach/sec_debug.h>
#endif

#if defined(CONFIG_TOUCHSCREEN_MELFAS)
#include <linux/melfas_ts_tablet.h>
#endif

#ifdef CONFIG_VIDEO_MHL_TABLET_V1
#include <linux/sii9234.h>
#include <linux/30pin_con.h>
#endif

#if defined(CONFIG_SEC_KEYBOARD_DOCK)
#include <linux/sec_keyboard_struct.h>
#endif

#ifdef CONFIG_USB_ANDROID_ACCESSORY
#include <linux/usb/f_accessory.h>
#endif

#define MSM_SHARED_RAM_PHYS 0x40000000

/* Common PMIC GPIO ************************************************************/
#ifdef CONFIG_SAMSUNG_JACK
#define PMIC_GPIO_EAR_DET		PM8058_GPIO(27)  	/* PMIC GPIO Number 27 */
#define PMIC_GPIO_SHORT_SENDEND	PM8058_GPIO(30)  	/* PMIC GPIO Number 30 */
#define PMIC_GPIO_MUTE_CON		PM8058_GPIO(26)  	/* PMIC GPIO Number 26 */
#define PMIC_GPIO_EAR_MICBIAS_EN PM8058_GPIO(17) /* PMIC GPIO Number 17  */
#endif   //CONFIG_SAMSUNG_JACK
#define PMIC_GPIO_MAIN_MICBIAS_EN PM8058_GPIO(28)

#define PM8901_GPIO_BASE			(PM8058_GPIO_BASE + \
						PM8058_GPIOS + PM8058_MPPS)
#define PM8901_GPIO_PM_TO_SYS(pm_gpio)		(pm_gpio + PM8901_GPIO_BASE)
#define PM8901_GPIO_SYS_TO_PM(sys_gpio)		(sys_gpio - PM901_GPIO_BASE)
#define PM8901_IRQ_BASE				(PM8058_IRQ_BASE + \
						NR_PMIC8058_IRQS)
#define TOUCHSCREEN_IRQ 		125  
#define PM_GPIO_TSP_3P3_CONTROL 16
#define GPIO_AVDD_3P3 PM8058_GPIO_PM_TO_SYS(PM_GPIO_TSP_3P3_CONTROL)

#define MDM2AP_SYNC 129

#define DSPS_PIL_GENERIC_NAME		"dsps"
#define DSPS_PIL_FLUID_NAME		"dsps_fluid"

#ifdef CONFIG_ION_MSM
static struct platform_device ion_dev;
#endif

#define GPIO_WLAN_HOST_WAKE 105	//WLAN_HOST_WAKE
#define GPIO_WLAN_REG_ON 45	//WLAN_BT_EN

#define GPIO_BT_WAKE        86
#define GPIO_BT_HOST_WAKE   127

#define GPIO_BT_REG_ON 158	//WLAN_BT_EN

#define GPIO_BT_RESET       46

#define GPIO_BT_UART_RTS    56
#define GPIO_BT_UART_CTS    55
#define GPIO_BT_UART_RXD    54
#define GPIO_BT_UART_TXD    53

#define GPIO_BT_PCM_DOUT    111
#define GPIO_BT_PCM_DIN     112
#define GPIO_BT_PCM_SYNC    113
#define GPIO_BT_PCM_CLK     114

#define GPIO_WLAN_LEVEL_LOW				0
#define GPIO_WLAN_LEVEL_HIGH			1
#define GPIO_WLAN_LEVEL_NONE			2

#ifdef CONFIG_OPTICAL_GP2A
#define SENSOR_ALS_SCL   		139
#define SENSOR_ALS_SDA   		138
#endif

#ifdef CONFIG_OPTICAL_BH1721_I957
#define SENSOR_ALS_SCL   		139
#define SENSOR_ALS_SDA   		138
#endif

/* Audio AMP Driver GPIO */
#define GPIO_AMP_I2C_SCL	154
#define GPIO_AMP_I2C_SDA	155

#if defined(CONFIG_TDMB) || defined(CONFIG_TDMB_MODULE)
#define GPIO_TDMB_EN    130
#define GPIO_TDMB_RST   126
#define GPIO_TDMB_INT   124
#define GPIO_TDMB_SPI_MOSI  33
#define GPIO_TDMB_SPI_MISO  34
#define GPIO_TDMB_SPI_CS    35
#define GPIO_TDMB_SPI_CLK   36
#endif

#ifdef CONFIG_VIDEO_MHL_TABLET_V1
#define GPIO_OTG_EN		71
#define GPIO_MHL_SCL	65
#define GPIO_MHL_SDA	64
#define GPIO_ACCESSORY_INT		123
#define HDMI_HPD_IRQ 			172  
#define PMIC_GPIO_ACCESSORY_CHECK		PM8058_GPIO(4)
#define PMIC_GPIO_ACCESSORY_CHECK_04	PM8058_GPIO(13)
#define PMIC_GPIO_ACCESSORY_EN		PM8058_GPIO(32)
#define PMIC_GPIO_DOCK_INT				PM8058_GPIO(29)
#define PMIC_GPIO_MHL_RST				PM8058_GPIO(15) 
#define GPIO_MHL_RST					PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_RST)
#define PMIC_GPIO_MHL_INT				PM8058_GPIO(9)
#endif

#define UART_SEL_SW          58
#define PMIC_GPIO_UICC_DET		PM8058_GPIO(25)

#define PMIC_HWREV0	PM8058_GPIO(34)
#define PMIC_HWREV1	PM8058_GPIO(35)
#define PMIC_HWREV2	PM8058_GPIO(36)
#define PMIC_HWREV3	PM8058_GPIO(38)

/* check LPM booting  */
extern int charging_mode_from_boot;

struct pm8xxx_mpp_init_info {
	unsigned			mpp;
	struct pm8xxx_mpp_config_data	config;
};

#define PM8058_MPP_INIT(_mpp, _type, _level, _control) \
{ \
	.mpp	= PM8058_MPP_PM_TO_SYS(_mpp), \
	.config	= { \
		.type		= PM8XXX_MPP_TYPE_##_type, \
		.level		= _level, \
		.control	= PM8XXX_MPP_##_control, \
	} \
}

#define PM8901_MPP_INIT(_mpp, _type, _level, _control) \
{ \
	.mpp	= PM8901_MPP_PM_TO_SYS(_mpp), \
	.config	= { \
		.type		= PM8XXX_MPP_TYPE_##_type, \
		.level		= _level, \
		.control	= PM8XXX_MPP_##_control, \
	} \
}

/*
FM GPIO is GPIO 18 on PMIC 8058.
As the index starts from 0 in the PMIC driver, and hence 17
corresponds to GPIO 18 on PMIC 8058.
*/
#define FM_GPIO 17

#define USB_SEL_2	37

typedef enum {
	USB_SEL_ADC =0,
	USB_SEL_MSM,
	USB_SEL_MDM,
} usb_path_type;
usb_path_type get_usb_path(void);
void set_usb_path(usb_path_type usb_path);

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
static void (*sdc2_status_notify_cb)(int card_present, void *dev_id);
static void *sdc2_status_notify_cb_devid;
#endif

#ifdef CONFIG_MMC_MSM_SDC5_SUPPORT
static void (*sdc5_status_notify_cb)(int card_present, void *dev_id);
static void *sdc5_status_notify_cb_devid;
#endif

#define GPIO_CONFIG(gpio)         (MSM_TLMM_BASE + 0x1000 + (0x10 * (gpio)))  // mskang_test
#define GPIO_IN_OUT(gpio)    (MSM_TLMM_BASE + 0x1004 + (0x10 * (gpio)))  // mskang_test  

static struct msm_spm_platform_data msm_spm_data_v1[] __initdata = {
	[0] = {
		.reg_base_addr = MSM_SAW0_BASE,

#ifdef CONFIG_MSM_AVS_HW
		.reg_init_values[MSM_SPM_REG_SAW_AVS_CTL] = 0x586020FF,
#endif
		.reg_init_values[MSM_SPM_REG_SAW_CFG] = 0x0F,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_CTL] = 0x68,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_SLP_TMR_DLY] = 0xFFFFFFFF,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_WAKE_TMR_DLY] = 0xFFFFFFFF,

		.reg_init_values[MSM_SPM_REG_SAW_SLP_CLK_EN] = 0x01,
		.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_PRECLMP_EN] = 0x07,
		.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_POSTCLMP_EN] = 0x00,

		.reg_init_values[MSM_SPM_REG_SAW_SLP_CLMP_EN] = 0x01,
		.reg_init_values[MSM_SPM_REG_SAW_SLP_RST_EN] = 0x00,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_MPM_CFG] = 0x00,

		.awake_vlevel = 0x94,
		.retention_vlevel = 0x81,
		.collapse_vlevel = 0x20,
		.retention_mid_vlevel = 0x94,
		.collapse_mid_vlevel = 0x8C,

		.vctl_timeout_us = 50,
	},

	[1] = {
		.reg_base_addr = MSM_SAW1_BASE,

#ifdef CONFIG_MSM_AVS_HW
		.reg_init_values[MSM_SPM_REG_SAW_AVS_CTL] = 0x586020FF,
#endif
		.reg_init_values[MSM_SPM_REG_SAW_CFG] = 0x0F,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_CTL] = 0x68,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_SLP_TMR_DLY] = 0xFFFFFFFF,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_WAKE_TMR_DLY] = 0xFFFFFFFF,

		.reg_init_values[MSM_SPM_REG_SAW_SLP_CLK_EN] = 0x13,
		.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_PRECLMP_EN] = 0x07,
		.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_POSTCLMP_EN] = 0x00,

		.reg_init_values[MSM_SPM_REG_SAW_SLP_CLMP_EN] = 0x01,
		.reg_init_values[MSM_SPM_REG_SAW_SLP_RST_EN] = 0x00,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_MPM_CFG] = 0x00,

		.awake_vlevel = 0x94,
		.retention_vlevel = 0x81,
		.collapse_vlevel = 0x20,
		.retention_mid_vlevel = 0x94,
		.collapse_mid_vlevel = 0x8C,

		.vctl_timeout_us = 50,
	},
};

static struct msm_spm_platform_data msm_spm_data[] __initdata = {
	[0] = {
		.reg_base_addr = MSM_SAW0_BASE,

#ifdef CONFIG_MSM_AVS_HW
		.reg_init_values[MSM_SPM_REG_SAW_AVS_CTL] = 0x586020FF,
#endif
		.reg_init_values[MSM_SPM_REG_SAW_CFG] = 0x1C,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_CTL] = 0x68,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_SLP_TMR_DLY] = 0x0C0CFFFF,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_WAKE_TMR_DLY] = 0x78780FFF,

		.reg_init_values[MSM_SPM_REG_SAW_SLP_CLK_EN] = 0x01,
		.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_PRECLMP_EN] = 0x07,
		.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_POSTCLMP_EN] = 0x00,

		.reg_init_values[MSM_SPM_REG_SAW_SLP_CLMP_EN] = 0x01,
		.reg_init_values[MSM_SPM_REG_SAW_SLP_RST_EN] = 0x00,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_MPM_CFG] = 0x00,

		.awake_vlevel = 0xA0,
		.retention_vlevel = 0x89,
		.collapse_vlevel = 0x20,
		.retention_mid_vlevel = 0x89,
		.collapse_mid_vlevel = 0x89,

		.vctl_timeout_us = 50,
	},

	[1] = {
		.reg_base_addr = MSM_SAW1_BASE,

#ifdef CONFIG_MSM_AVS_HW
		.reg_init_values[MSM_SPM_REG_SAW_AVS_CTL] = 0x586020FF,
#endif
		.reg_init_values[MSM_SPM_REG_SAW_CFG] = 0x1C,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_CTL] = 0x68,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_SLP_TMR_DLY] = 0x0C0CFFFF,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_WAKE_TMR_DLY] = 0x78780FFF,

		.reg_init_values[MSM_SPM_REG_SAW_SLP_CLK_EN] = 0x13,
		.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_PRECLMP_EN] = 0x07,
		.reg_init_values[MSM_SPM_REG_SAW_SLP_HSFS_POSTCLMP_EN] = 0x00,

		.reg_init_values[MSM_SPM_REG_SAW_SLP_CLMP_EN] = 0x01,
		.reg_init_values[MSM_SPM_REG_SAW_SLP_RST_EN] = 0x00,
		.reg_init_values[MSM_SPM_REG_SAW_SPM_MPM_CFG] = 0x00,

		.awake_vlevel = 0xA0,
		.retention_vlevel = 0x89,
		.collapse_vlevel = 0x20,
		.retention_mid_vlevel = 0x89,
		.collapse_mid_vlevel = 0x89,

		.vctl_timeout_us = 50,
	},
};

/*
 * Consumer specific regulator names:
 *			 regulator name		consumer dev_name
 */
static struct regulator_consumer_supply vreg_consumers_8901_S0[] = {
	REGULATOR_SUPPLY("8901_s0",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_8901_S1[] = {
	REGULATOR_SUPPLY("8901_s1",		NULL),
};

static struct regulator_init_data saw_s0_init_data = {
		.constraints = {
			.name = "8901_s0",
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
			.min_uV = 800000,
			.max_uV = 1325000,
		},
		.consumer_supplies = vreg_consumers_8901_S0,
		.num_consumer_supplies = ARRAY_SIZE(vreg_consumers_8901_S0),
};

static struct regulator_init_data saw_s1_init_data = {
		.constraints = {
			.name = "8901_s1",
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
			.min_uV = 800000,
			.max_uV = 1325000,
		},
		.consumer_supplies = vreg_consumers_8901_S1,
		.num_consumer_supplies = ARRAY_SIZE(vreg_consumers_8901_S1),
};

static struct platform_device msm_device_saw_s0 = {
	.name          = "saw-regulator",
	.id            = 0,
	.dev           = {
		.platform_data = &saw_s0_init_data,
	},
};

static struct platform_device msm_device_saw_s1 = {
	.name          = "saw-regulator",
	.id            = 1,
	.dev           = {
		.platform_data = &saw_s1_init_data,
	},
};

#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)

#define QCE_SIZE		0x10000
#define QCE_0_BASE		0x18500000

#define QCE_HW_KEY_SUPPORT	0
#define QCE_SHA_HMAC_SUPPORT	0
#define QCE_SHARE_CE_RESOURCE	2
#define QCE_CE_SHARED		1

static struct resource qcrypto_resources[] = {
	[0] = {
		.start = QCE_0_BASE,
		.end = QCE_0_BASE + QCE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = "crypto_channels",
		.start = DMOV_CE_IN_CHAN,
		.end = DMOV_CE_OUT_CHAN,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.name = "crypto_crci_in",
		.start = DMOV_CE_IN_CRCI,
		.end = DMOV_CE_IN_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.name = "crypto_crci_out",
		.start = DMOV_CE_OUT_CRCI,
		.end = DMOV_CE_OUT_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[4] = {
		.name = "crypto_crci_hash",
		.start = DMOV_CE_HASH_CRCI,
		.end = DMOV_CE_HASH_CRCI,
		.flags = IORESOURCE_DMA,
	},
};

static struct resource qcedev_resources[] = {
	[0] = {
		.start = QCE_0_BASE,
		.end = QCE_0_BASE + QCE_SIZE - 1,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.name = "crypto_channels",
		.start = DMOV_CE_IN_CHAN,
		.end = DMOV_CE_OUT_CHAN,
		.flags = IORESOURCE_DMA,
	},
	[2] = {
		.name = "crypto_crci_in",
		.start = DMOV_CE_IN_CRCI,
		.end = DMOV_CE_IN_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[3] = {
		.name = "crypto_crci_out",
		.start = DMOV_CE_OUT_CRCI,
		.end = DMOV_CE_OUT_CRCI,
		.flags = IORESOURCE_DMA,
	},
	[4] = {
		.name = "crypto_crci_hash",
		.start = DMOV_CE_HASH_CRCI,
		.end = DMOV_CE_HASH_CRCI,
		.flags = IORESOURCE_DMA,
	},
};

#endif

#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE)

static struct msm_ce_hw_support qcrypto_ce_hw_suppport = {
	.ce_shared = QCE_CE_SHARED,
	.shared_ce_resource = QCE_SHARE_CE_RESOURCE,
	.hw_key_support = QCE_HW_KEY_SUPPORT,
	.sha_hmac = QCE_SHA_HMAC_SUPPORT,
	.bus_scale_table = NULL,
};

static struct platform_device qcrypto_device = {
	.name		= "qcrypto",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(qcrypto_resources),
	.resource	= qcrypto_resources,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &qcrypto_ce_hw_suppport,
	},
};
#endif

#if defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)

static struct msm_ce_hw_support qcedev_ce_hw_suppport = {
	.ce_shared = QCE_CE_SHARED,
	.shared_ce_resource = QCE_SHARE_CE_RESOURCE,
	.hw_key_support = QCE_HW_KEY_SUPPORT,
	.sha_hmac = QCE_SHA_HMAC_SUPPORT,
	.bus_scale_table = NULL,
};

static struct platform_device qcedev_device = {
	.name		= "qce",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(qcedev_resources),
	.resource	= qcedev_resources,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(32),
		.platform_data = &qcedev_ce_hw_suppport,
	},
};
#endif

static struct msm_pm_platform_data msm_pm_data[MSM_PM_SLEEP_MODE_NR * 2] = {
	[MSM_PM_MODE(0, MSM_PM_SLEEP_MODE_POWER_COLLAPSE)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 0,
		.suspend_enabled = 0,
	},

	[MSM_PM_MODE(0, MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 0,
		.suspend_enabled = 0,
	},

	[MSM_PM_MODE(0, MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 1,
		.suspend_enabled = 1,
	},

	[MSM_PM_MODE(1, MSM_PM_SLEEP_MODE_POWER_COLLAPSE)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 0,
		.suspend_enabled = 0,
	},

	[MSM_PM_MODE(1, MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 0,
		.suspend_enabled = 0,
	},

	[MSM_PM_MODE(1, MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT)] = {
		.idle_supported = 1,
		.suspend_supported = 1,
		.idle_enabled = 1,
		.suspend_enabled = 1,
	},
};

static struct msm_cpuidle_state msm_cstates[] __initdata = {
	{0, 0, "C0", "WFI",
		MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT},

	{0, 1, "C1", "STANDALONE_POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE},

	{0, 2, "C2", "POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE},

	{1, 0, "C0", "WFI",
		MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT},

	{1, 1, "C1", "STANDALONE_POWER_COLLAPSE",
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE},
};

static struct msm_rpmrs_level msm_rpmrs_levels[] __initdata = {
	{
		MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT,
		MSM_RPMRS_LIMITS(ON, ACTIVE, MAX, ACTIVE),
		true,
		1, 8000, 100000, 1,
	},

	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE_STANDALONE,
		MSM_RPMRS_LIMITS(ON, ACTIVE, MAX, ACTIVE),
		true,
		1500, 5000, 60100000, 3000,
	},

	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(ON, ACTIVE, MAX, ACTIVE),
		false,
		1800, 5000, 60350000, 3500,
	},
	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(OFF, ACTIVE, MAX, ACTIVE),
		false,
		3800, 4500, 65350000, 5500,
	},

	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(ON, HSFS_OPEN, MAX, ACTIVE),
		false,
		2800, 2500, 66850000, 4800,
	},

	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(OFF, HSFS_OPEN, MAX, ACTIVE),
		false,
		4800, 2000, 71850000, 6800,
	},

	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(OFF, HSFS_OPEN, ACTIVE, RET_HIGH),
		false,
		6800, 500, 75850000, 8800,
	},

	{
		MSM_PM_SLEEP_MODE_POWER_COLLAPSE,
		MSM_RPMRS_LIMITS(OFF, HSFS_OPEN, RET_HIGH, RET_LOW),
		false,
		7800, 0, 76350000, 9800,
	},
};

static struct msm_pm_boot_platform_data msm_pm_boot_pdata __initdata = {
	.mode = MSM_PM_BOOT_CONFIG_TZ,
};


#if defined(CONFIG_USB_GADGET_MSM_72K) || defined(CONFIG_USB_EHCI_MSM_72K)
static struct msm_otg_platform_data msm_otg_pdata;
static struct regulator *ldo6_3p3;
static struct regulator *ldo7_1p8;
static struct regulator *vdd_cx;
#define PMICID_INT		PM8058_GPIO_IRQ(PM8058_IRQ_BASE, 36)
notify_vbus_state notify_vbus_state_func_ptr;
//static int usb_phy_susp_dig_vol = 750000;
static int usb_phy_susp_dig_vol = 500000;
static int pmic_id_notif_supported;

#ifdef CONFIG_USB_EHCI_MSM_72K
#define USB_PMIC_ID_DET_DELAY	msecs_to_jiffies(100)
struct delayed_work pmic_id_det;

static int __init usb_id_pin_rework_setup(char *support)
{
	if (strncmp(support, "true", 4) == 0)
		pmic_id_notif_supported = 1;

	return 1;
}
__setup("usb_id_pin_rework=", usb_id_pin_rework_setup);

static void pmic_id_detect(struct work_struct *w)
{
	int val = gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(36));
	pr_debug("%s(): gpio_read_value = %d\n", __func__, val);

	if (notify_vbus_state_func_ptr)
		(*notify_vbus_state_func_ptr) (val);
}

static irqreturn_t pmic_id_on_irq(int irq, void *data)
{
	/*
	 * Spurious interrupts are observed on pmic gpio line
	 * even though there is no state change on USB ID. Schedule the
	 * work to to allow debounce on gpio
	 */
	schedule_delayed_work(&pmic_id_det, USB_PMIC_ID_DET_DELAY);

	return IRQ_HANDLED;
}

static int msm_hsusb_phy_id_setup_init(int init)
{
	unsigned ret;

	struct pm8xxx_mpp_config_data hsusb_phy_mpp = {
		.type	= PM8XXX_MPP_TYPE_D_OUTPUT,
		.level	= PM8901_MPP_DIG_LEVEL_L5,
	};

	if (init) {
		hsusb_phy_mpp.control = PM8XXX_MPP_DOUT_CTRL_HIGH;
		ret = pm8xxx_mpp_config(PM8901_MPP_PM_TO_SYS(1),
							&hsusb_phy_mpp);
		if (ret < 0)
			pr_err("%s:MPP2 configuration failed\n", __func__);
	} else {
		hsusb_phy_mpp.control = PM8XXX_MPP_DOUT_CTRL_LOW;
		ret = pm8xxx_mpp_config(PM8901_MPP_PM_TO_SYS(1),
							&hsusb_phy_mpp);
		if (ret < 0)
			pr_err("%s:MPP2 un config failed\n", __func__);
	}
	return ret;
}

static int msm_hsusb_pmic_id_notif_init(void (*callback)(int online), int init)
{
	unsigned ret = -ENODEV;

	if (!callback)
		return -EINVAL;

	if (machine_is_msm8x60_fluid())
		return -ENOTSUPP;

	if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) != 2) {
		pr_debug("%s: USB_ID pin is not routed to PMIC"
					"on V1 surf/ffa\n", __func__);
		return -ENOTSUPP;
	}

	if ( !pmic_id_notif_supported) {
		pr_debug("%s: USB_ID is not routed to PMIC"
			"on V2 ffa\n", __func__);
		return -ENOTSUPP;
	}

	struct pm8xxx_mpp_config_data pm8901_mpp1 = {
		.type	= PM8XXX_MPP_TYPE_D_OUTPUT,
		.level	= PM8901_MPP_DIG_LEVEL_L5,
	};

	usb_phy_susp_dig_vol = 500000;

	if (init) {
		notify_vbus_state_func_ptr = callback;
		pm8901_mpp1.control = PM8XXX_MPP_DOUT_CTRL_HIGH;
		ret = pm8xxx_mpp_config(PM8901_MPP_PM_TO_SYS(1), &pm8901_mpp1);
		if (ret) {
			pr_err("%s: MPP2 configuration failed\n", __func__);
			return -ENODEV;
		}
		INIT_DELAYED_WORK(&pmic_id_det, pmic_id_detect);
		ret = request_threaded_irq(PMICID_INT, NULL, pmic_id_on_irq,
			(IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING),
						"msm_otg_id", NULL);
		if (ret) {
			pm8901_mpp1.control = PM8XXX_MPP_DOUT_CTRL_LOW;
			ret = pm8xxx_mpp_config(PM8901_MPP_PM_TO_SYS(1), &pm8901_mpp1);
			pr_err("%s:pmic_usb_id interrupt registration failed",
					__func__);
			return ret;
		}
		/* Notify the initial Id status */
		pmic_id_detect(&pmic_id_det.work);
	} else {
		free_irq(PMICID_INT, 0);
		cancel_delayed_work_sync(&pmic_id_det);
		notify_vbus_state_func_ptr = NULL;
		pm8901_mpp1.control = PM8XXX_MPP_DOUT_CTRL_LOW;
		ret = pm8xxx_mpp_config(PM8901_MPP_PM_TO_SYS(1), &pm8901_mpp1);
		if (ret) {
			pr_err("%s:MPP2 configuration failed\n", __func__);
			return -ENODEV;
		}
	}
	return 0;
}
#endif

#define USB_PHY_OPERATIONAL_MIN_VDD_DIG_VOL	1000000
#define USB_PHY_MAX_VDD_DIG_VOL			1320000
static int msm_hsusb_init_vddcx(int init)
{
	int ret = 0;

	if (init) {
		vdd_cx = regulator_get(NULL, "8058_s1");
		if (IS_ERR(vdd_cx)) {
			return PTR_ERR(vdd_cx);
		}

		ret = regulator_set_voltage(vdd_cx,
				USB_PHY_OPERATIONAL_MIN_VDD_DIG_VOL,
				USB_PHY_MAX_VDD_DIG_VOL);
		if (ret) {
			pr_err("%s: unable to set the voltage for regulator"
				"vdd_cx\n", __func__);
			regulator_put(vdd_cx);
			return ret;
		}

		ret = regulator_enable(vdd_cx);
		if (ret) {
			pr_err("%s: unable to enable regulator"
				"vdd_cx\n", __func__);
			regulator_put(vdd_cx);
		}
	} else {
		ret = regulator_disable(vdd_cx);
		if (ret) {
			pr_err("%s: Unable to disable the regulator:"
				"vdd_cx\n", __func__);
			return ret;
		}

		regulator_put(vdd_cx);
	}

	return ret;
}

static int msm_hsusb_config_vddcx(int high)
{
	int max_vol = USB_PHY_MAX_VDD_DIG_VOL;
	int min_vol;
	int ret;

	if (high)
		min_vol = USB_PHY_OPERATIONAL_MIN_VDD_DIG_VOL;
	else
		min_vol = usb_phy_susp_dig_vol;

	ret = regulator_set_voltage(vdd_cx, min_vol, max_vol);
	if (ret) {
		pr_err("%s: unable to set the voltage for regulator"
			"vdd_cx\n", __func__);
		return ret;
	}

	pr_debug("%s: min_vol:%d max_vol:%d\n", __func__, min_vol, max_vol);

	return ret;
}

#define USB_PHY_3P3_VOL_MIN	3600000 /* uV */
#define USB_PHY_3P3_VOL_MAX	3600000 /* uV */
#define USB_PHY_3P3_HPM_LOAD	50000	/* uA */
#define USB_PHY_3P3_LPM_LOAD	4000	/* uA */

#define USB_PHY_1P8_VOL_MIN	1800000 /* uV */
#define USB_PHY_1P8_VOL_MAX	1800000 /* uV */
#define USB_PHY_1P8_HPM_LOAD	50000	/* uA */
#define USB_PHY_1P8_LPM_LOAD	4000	/* uA */
static int msm_hsusb_ldo_init(int init)
{
	int rc = 0;

	if (init) {
		ldo6_3p3 = regulator_get(NULL, "8058_l6");
		if (IS_ERR(ldo6_3p3))
			return PTR_ERR(ldo6_3p3);

		ldo7_1p8 = regulator_get(NULL, "8058_l7");
		if (IS_ERR(ldo7_1p8)) {
			rc = PTR_ERR(ldo7_1p8);
			goto put_3p3;
		}

		rc = regulator_set_voltage(ldo6_3p3, USB_PHY_3P3_VOL_MIN,
				USB_PHY_3P3_VOL_MAX);
		if (rc) {
			pr_err("%s: Unable to set voltage level for"
				"ldo6_3p3 regulator\n", __func__);
			goto put_1p8;
		}
		rc = regulator_enable(ldo6_3p3);
		if (rc) {
			pr_err("%s: Unable to enable the regulator:"
				"ldo6_3p3\n", __func__);
			goto put_1p8;
		}
		rc = regulator_set_voltage(ldo7_1p8, USB_PHY_1P8_VOL_MIN,
				USB_PHY_1P8_VOL_MAX);
		if (rc) {
			pr_err("%s: Unable to set voltage level for"
				"ldo7_1p8 regulator\n", __func__);
			goto disable_3p3;
		}
		rc = regulator_enable(ldo7_1p8);
		if (rc) {
			pr_err("%s: Unable to enable the regulator:"
				"ldo7_1p8\n", __func__);
			goto disable_3p3;
		}

		return 0;
	}

	regulator_disable(ldo7_1p8);
disable_3p3:
	regulator_disable(ldo6_3p3);
put_1p8:
	regulator_put(ldo7_1p8);
put_3p3:
	regulator_put(ldo6_3p3);
	return rc;
}

static int msm_hsusb_ldo_enable(int on)
{
	int ret = 0;

	if (!ldo7_1p8 || IS_ERR(ldo7_1p8)) {
		pr_err("%s: ldo7_1p8 is not initialized\n", __func__);
		return -ENODEV;
	}

	if (!ldo6_3p3 || IS_ERR(ldo6_3p3)) {
		pr_err("%s: ldo6_3p3 is not initialized\n", __func__);
		return -ENODEV;
	}

	if (on) {
		ret = regulator_set_optimum_mode(ldo7_1p8,
				USB_PHY_1P8_HPM_LOAD);
		if (ret < 0) {
			pr_err("%s: Unable to set HPM of the regulator:"
				"ldo7_1p8\n", __func__);
			return ret;
		}
		ret = regulator_set_optimum_mode(ldo6_3p3,
				USB_PHY_3P3_HPM_LOAD);
		if (ret < 0) {
			pr_err("%s: Unable to set HPM of the regulator:"
				"ldo6_3p3\n", __func__);
			regulator_set_optimum_mode(ldo7_1p8,
				USB_PHY_1P8_LPM_LOAD);
			return ret;
		}
	} else {
		ret = regulator_set_optimum_mode(ldo7_1p8,
				USB_PHY_1P8_LPM_LOAD);
		if (ret < 0)
			pr_err("%s: Unable to set LPM of the regulator:"
				"ldo7_1p8\n", __func__);
		ret = regulator_set_optimum_mode(ldo6_3p3,
				USB_PHY_3P3_LPM_LOAD);
		if (ret < 0)
			pr_err("%s: Unable to set LPM of the regulator:"
				"ldo6_3p3\n", __func__);
	}

	pr_debug("reg (%s)\n", on ? "HPM" : "LPM");
	return ret < 0 ? ret : 0;
 }
#endif
#ifdef CONFIG_USB_EHCI_MSM_72K
#if defined(CONFIG_SMB137B_CHARGER) || defined(CONFIG_SMB137B_CHARGER_MODULE)
static void msm_hsusb_smb137b_vbus_power(unsigned phy_info, int on)
{
	static int vbus_is_on;

	/* If VBUS is already on (or off), do nothing. */
	if (on == vbus_is_on)
		return;
	smb137b_otg_power(on);
	vbus_is_on = on;
}
#endif

#ifdef CONFIG_30PIN_CONN
static void qc_acc_power(u8 token, bool active);
#endif
static void msm_hsusb_vbus_power(unsigned phy_info, int on)
{
	static struct regulator *votg_5v_switch;
	static struct regulator *ext_5v_reg;
	static int vbus_is_on;
#ifdef CONFIG_30PIN_CONN
	pr_info("[OTG] %s: phy_info = %d, on = %d\n", __func__, phy_info, on);
	if (on)
		qc_acc_power(1, true);
	else
		qc_acc_power(1, false);
#else
	/* If VBUS is already on (or off), do nothing. */
	if (on == vbus_is_on)
		return;

	if (!votg_5v_switch) {
		votg_5v_switch = regulator_get(NULL, "8901_usb_otg");
		if (IS_ERR(votg_5v_switch)) {
			pr_err("%s: unable to get votg_5v_switch\n", __func__);
			return;
		}
	}
	if (!ext_5v_reg) {
		ext_5v_reg = regulator_get(NULL, "8901_mpp0");
		if (IS_ERR(ext_5v_reg)) {
			pr_err("%s: unable to get ext_5v_reg\n", __func__);
			return;
		}
	}
	if (on) {
		if (regulator_enable(ext_5v_reg)) {
			pr_err("%s: Unable to enable the regulator:"
					" ext_5v_reg\n", __func__);
			return;
		}
		if (regulator_enable(votg_5v_switch)) {
			pr_err("%s: Unable to enable the regulator:"
					" votg_5v_switch\n", __func__);
			return;
		}
	} else {
		if (regulator_disable(votg_5v_switch))
			pr_err("%s: Unable to enable the regulator:"
				" votg_5v_switch\n", __func__);
		if (regulator_disable(ext_5v_reg))
			pr_err("%s: Unable to enable the regulator:"
				" ext_5v_reg\n", __func__);
	}
#endif
	vbus_is_on = on;
}

static struct msm_usb_host_platform_data msm_usb_host_pdata = {
	.phy_info	= (USB_PHY_INTEGRATED | USB_PHY_MODEL_45NM),
	/*.power_budget	= 390,*/
#ifdef CONFIG_USB_HOST_NOTIFY
	.host_notify = 1,
#endif
#ifdef CONFIG_USB_SEC_WHITELIST
	.sec_whlist_table_num = 1,
#endif
};
#endif

#ifdef CONFIG_BATTERY_MSM8X60
static int msm_hsusb_pmic_vbus_notif_init(void (*callback)(int online),
								int init)
{
	int ret = -ENOTSUPP;

#if defined(CONFIG_SMB137B_CHARGER) || defined(CONFIG_SMB137B_CHARGER_MODULE)
	if (machine_is_msm8x60_fluid()) {
		if (init)
			msm_charger_register_vbus_sn(callback);
		else
			msm_charger_unregister_vbus_sn(callback);
		return  0;
	}
#endif
	/* ID and VBUS lines are connected to pmic on 8660.V2.SURF,
	 * hence, irrespective of either peripheral only mode or
	 * OTG (host and peripheral) modes, can depend on pmic for
	 * vbus notifications
	 */
	if ((SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 2)
			&& (machine_is_msm8x60_surf() ||
				pmic_id_notif_supported)) {
		printk("%s : init value = %d\n", __func__, init);
		if (init)
			ret = msm_charger_register_vbus_sn(callback);
		else {
			msm_charger_unregister_vbus_sn(callback);
			ret = 0;
		}
	} else {
#if !defined(CONFIG_USB_EHCI_MSM_72K)
	if (init)
		ret = msm_charger_register_vbus_sn(callback);
	else {
		msm_charger_unregister_vbus_sn(callback);
		ret = 0;
	}
#endif
	}
	return ret;
}
#endif

static void qc_otg_en(int active)
{
	active = !!active;
	gpio_direction_output(GPIO_OTG_EN, active);
	pr_info("Board : %s = %d\n", __func__, active);
}

static void qc_acc_power(u8 token, bool active)
{
	static bool enable = false;
	static u8 acc_en_token = 0;
	int gpio_acc_en;
	int gpio_acc_5v;
	int try_cnt = 0;

	gpio_acc_en = PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_ACCESSORY_EN);
	gpio_request(gpio_acc_en, "accessory_en");

	if (system_rev < 0x04)
		gpio_acc_5v = PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_ACCESSORY_CHECK);
	else
		gpio_acc_5v = PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_ACCESSORY_CHECK_04);

	/*
		token info
		0 : force power off
		1 : usb otg
		2 : keyboard
	*/
	if (active) {
		acc_en_token |= (1 << token);
		enable = true;
		gpio_direction_output(gpio_acc_en, 1);
		msleep(20);
		while(!gpio_get_value_cansleep(gpio_acc_5v)) {
			gpio_direction_output(gpio_acc_en, 0);
			msleep(20);
			gpio_direction_output(gpio_acc_en, 1);
			if (try_cnt > 10) {
				pr_err("[acc] failed to enable the accessory_en");
				break;
			} else
				try_cnt++;
		}
	} else {
		if (0 == token) {
			gpio_direction_output(gpio_acc_en, 0);
			enable = false;
			while(gpio_get_value_cansleep(gpio_acc_5v)) {
				msleep(10);
				if (try_cnt > 30) {
					pr_err("[acc] failed to disable the accessory_en");
					break;
				} else
					try_cnt++;
			}
		} else {
			acc_en_token &= ~(1 << token);
			if (0 == acc_en_token) {
				gpio_direction_output(gpio_acc_en, 0);
				enable = false;
			}
		}
	}
	gpio_free(gpio_acc_en);

	pr_info("Board : %s token : (%d,%d) %s\n", __func__,
		token, active, enable ? "on" : "off");
}

static void qc_usb_ldo_en(int active)	
{
	struct regulator *l6_u = NULL;
	int ret;
	
	if(l6_u == NULL){
		l6_u = regulator_get(NULL, "8058_l6");
		if (IS_ERR(l6_u))
        			return;
		ret = regulator_set_voltage(l6_u, 3050000, 3050000);
        	if (ret) {
        		printk("%s: error setting l6_u voltage\n", __func__);
				return;
        	}
	}

	if (active) {
		ret = regulator_enable(l6_u);
        	if (ret) {
        		printk("%s: error enabling regulator\n", __func__);
				return;
        		}
		}
	else {
		ret = regulator_disable(l6_u);
        	if (ret) {
        		printk("%s: error enabling regulator\n", __func__);
				return;
        	}
	}
	pr_info("Board : %s = %d\n", __func__, active);
}

int get_accessory_irq_gpio()
{
	pr_info("Board : %s, %d\n", __func__, system_rev);
	if (system_rev < 0x04)
		return PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_ACCESSORY_CHECK);
	else
		return PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_ACCESSORY_CHECK_04);
}

int get_accessory_irq()
{
	pr_info("Board : %s, %d\n", __func__, system_rev);
	if (system_rev < 0x04)
		return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_ACCESSORY_CHECK);
	else
		return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_ACCESSORY_CHECK_04);
}

#if defined(CONFIG_USB_GADGET_MSM_72K) || defined(CONFIG_USB_EHCI_MSM_72K)
static struct msm_otg_platform_data msm_otg_pdata = {
	/* if usb link is in sps there is no need for
	 * usb pclk as dayatona fabric clock will be
	 * used instead
	 */
	.pemp_level		 = PRE_EMPHASIS_WITH_20_PERCENT,
	.cdr_autoreset		 = CDR_AUTO_RESET_DISABLE,
#if defined(CONFIG_KOR_OPERATOR_LGU)	
	.drv_ampl		 = HS_DRV_AMPLITUDE_75_PERCENT,
#endif	
	.se1_gating		 = SE1_GATING_DISABLE,
	.bam_disable		 = 1,
#ifdef CONFIG_USB_EHCI_MSM_72K
	.pmic_id_notif_init = msm_hsusb_pmic_id_notif_init,
	.phy_id_setup_init = msm_hsusb_phy_id_setup_init,
#endif
#ifdef CONFIG_USB_EHCI_MSM_72K
	.vbus_power = msm_hsusb_vbus_power,
#endif
#ifdef CONFIG_BATTERY_MSM8X60
	.pmic_vbus_notif_init	= msm_hsusb_pmic_vbus_notif_init,
#endif
	.ldo_init		 = msm_hsusb_ldo_init,
	.ldo_enable		 = msm_hsusb_ldo_enable,
	.config_vddcx            = msm_hsusb_config_vddcx,
	.init_vddcx              = msm_hsusb_init_vddcx,
#ifdef CONFIG_BATTERY_MSM8X60
	.chg_vbus_draw = msm_charger_vbus_draw,
#endif
#ifdef CONFIG_USB_HOST_NOTIFY
//	.otg_en = qc_otg_en,
#endif
#ifdef CONFIG_30PIN_CONN
	.accessory_irq = get_accessory_irq,
	.accessory_irq_gpio = get_accessory_irq_gpio,
#endif
};
#endif

#ifdef CONFIG_USB_GADGET_MSM_72K
static struct msm_hsusb_gadget_platform_data msm_gadget_pdata = {
	.is_phy_status_timer_on = 1,
};
#endif

#ifdef CONFIG_USB_G_ANDROID

#define PID_MAGIC_ID		0x71432909
#define SERIAL_NUM_MAGIC_ID	0x61945374
#define SERIAL_NUMBER_LENGTH	127
#define DLOAD_USB_BASE_ADD	0x2A05F0C8

struct magic_num_struct {
	uint32_t pid;
	uint32_t serial_num;
};

struct dload_struct {
	uint32_t	reserved1;
	uint32_t	reserved2;
	uint32_t	reserved3;
	uint16_t	reserved4;
	uint16_t	pid;
	char		serial_number[SERIAL_NUMBER_LENGTH];
	uint16_t	reserved5;
	struct magic_num_struct
			magic_struct;
};

static int usb_diag_update_pid_and_serial_num(uint32_t pid, const char *snum)
{
	struct dload_struct __iomem *dload = 0;

	dload = ioremap(DLOAD_USB_BASE_ADD, sizeof(*dload));
	if (!dload) {
		pr_err("%s: cannot remap I/O memory region: %08x\n",
					__func__, DLOAD_USB_BASE_ADD);
		return -ENXIO;
	}

	pr_debug("%s: dload:%p pid:%x serial_num:%s\n",
				__func__, dload, pid, snum);
	/* update pid */
	dload->magic_struct.pid = PID_MAGIC_ID;
	dload->pid = pid;

	/* update serial number */
	dload->magic_struct.serial_num = 0;
	if (!snum)
		return 0;

	dload->magic_struct.serial_num = SERIAL_NUM_MAGIC_ID;
	strncpy(dload->serial_number, snum, SERIAL_NUMBER_LENGTH);
	dload->serial_number[SERIAL_NUMBER_LENGTH - 1] = '\0';

	iounmap(dload);

	return 0;
}

static struct android_usb_platform_data android_usb_pdata = {
	.update_pid_and_serial_num = usb_diag_update_pid_and_serial_num,
};

static struct platform_device android_usb_device = {
	.name	= "android_usb",
	.id	= -1,
	.dev	= {
		.platform_data = &android_usb_pdata,
	},
};


#endif

#ifdef CONFIG_MSM_VPE
static struct resource msm_vpe_resources[] = {
	{
		.start	= 0x05300000,
		.end	= 0x05300000 + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_VPE,
		.end	= INT_VPE,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device msm_vpe_device = {
	.name = "msm_vpe",
	.id   = 0,
	.num_resources = ARRAY_SIZE(msm_vpe_resources),
	.resource = msm_vpe_resources,
};
#endif

#ifdef CONFIG_MSM_BUS_SCALING
static struct msm_bus_vectors cam_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_preview_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 283115520,
		.ib  = 452984832,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_video_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 283115520,
		.ib  = 452984832,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 283115520,
		.ib  = 452984832,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 319610880,
		.ib  = 511377408,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_snapshot_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 566231040,
		.ib  = 905969664,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 69984000,
		.ib  = 111974400,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 320864256,
		.ib  = 513382810,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 320864256,
		.ib  = 513382810,
	},
};

static struct msm_bus_vectors cam_zsl_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 566231040,
		.ib  = 905969664,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 706199040,
		.ib  = 1129918464,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 320864256,
		.ib  = 513382810,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 320864256,
		.ib  = 513382810,
	},
};

static struct msm_bus_vectors cam_stereo_video_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 212336640,
		.ib  = 339738624,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 25090560,
		.ib  = 40144896,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 239708160,
		.ib  = 383533056,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 79902720,
		.ib  = 127844352,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 0,
		.ib  = 0,
	},
};

static struct msm_bus_vectors cam_stereo_snapshot_vectors[] = {
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_VFE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 300902400,
		.ib  = 481443840,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 230307840,
		.ib  = 368492544,
	},
	{
		.src = MSM_BUS_MASTER_VPE,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 245113344,
		.ib  = 392181351,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 106536960,
		.ib  = 170459136,
	},
	{
		.src = MSM_BUS_MASTER_JPEG_ENC,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = 106536960,
		.ib  = 170459136,
	},
};

static struct msm_bus_paths cam_bus_client_config[] = {
	{
		ARRAY_SIZE(cam_init_vectors),
		cam_init_vectors,
	},
	{
		ARRAY_SIZE(cam_preview_vectors),
		cam_preview_vectors,
	},
	{
		ARRAY_SIZE(cam_video_vectors),
		cam_video_vectors,
	},
	{
		ARRAY_SIZE(cam_snapshot_vectors),
		cam_snapshot_vectors,
	},
	{
		ARRAY_SIZE(cam_zsl_vectors),
		cam_zsl_vectors,
	},
	{
		ARRAY_SIZE(cam_stereo_video_vectors),
		cam_stereo_video_vectors,
	},
	{
		ARRAY_SIZE(cam_stereo_snapshot_vectors),
		cam_stereo_snapshot_vectors,
	},
};

static struct msm_bus_scale_pdata cam_bus_client_pdata = {
		cam_bus_client_config,
		ARRAY_SIZE(cam_bus_client_config),
		.name = "msm_camera",
};
#endif

#ifdef CONFIG_MT9E013
static struct msm_camera_sensor_flash_data flash_mt9e013 = {
	.flash_type			= MSM_CAMERA_FLASH_LED,
	.flash_src			= &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_mt9e013_data = {
		.sensor_name	= "mt9e013",
		.sensor_reset	= 106,
		.sensor_pwd		= 85,
		.vcm_pwd		= 1,
		.vcm_enable		= 0,
		.pdata			= &msm_camera_device_data,
		.resource		= msm_camera_resources,
		.num_resources	= ARRAY_SIZE(msm_camera_resources),
		.flash_data		= &flash_mt9e013,
		.strobe_flash_data	= &strobe_flash_xenon,
		.csi_if			= 1
};
struct platform_device msm_camera_sensor_mt9e013 = {
		.name	= "msm_camera_mt9e013",
		.dev	= {
			.platform_data = &msm_camera_sensor_mt9e013_data,
		},
};
#endif

static struct i2c_board_info msm_camera_boardinfo[] __initdata = {
#ifdef CONFIG_MT9E013
	{
		I2C_BOARD_INFO("mt9e013", 0x6C >> 2),
	},
#endif
#ifdef CONFIG_SENSOR_S5K6AAFX
	{
		I2C_BOARD_INFO("s5k6aafx_i2c", 0x78>>1),
	},
#endif
#ifdef CONFIG_SENSOR_S5K5CCAF
	{
		I2C_BOARD_INFO("s5k5ccaf", 0x78>>1),
	},
#endif
#ifdef CONFIG_SENSOR_S5K5BAFX_P5
	{
		I2C_BOARD_INFO("s5k5bafx", 0x5A>>1),
	},
#endif
#ifdef CONFIG_S5K4ECGX
	{
#if ( CONFIG_BOARD_REVISION >= 0x01)
		I2C_BOARD_INFO("s5k4ecgx", 0xAC>>1),		
#else
		I2C_BOARD_INFO("s5k4ecgx", 0x5A>>1),		
#endif
	},
#endif	

};

static struct i2c_gpio_platform_data motor_i2c_gpio_data = {
	.sda_pin	= 116,
	.scl_pin	= 115,
	.udelay 	= 1,
	.timeout	= 0,
};

static struct platform_device motor_i2c_gpio_device = {  
	.name		= "i2c-gpio",
	.id 	=	20,
	.dev		= {
		.platform_data	= &motor_i2c_gpio_data,
	},
};

static struct isa1200_vibrator_platform_data isa1200_vibrator_pdata = {
	.gpio_en = 30,
	.max_timeout = 10000,
	.ctrl0 = CTL0_DIVIDER256 | CTL0_PWM_GEN,
	.ctrl1 = CTL1_DEFAULT | CTL1_EXT_CLOCK,
	.ctrl2 = 0,
	.ctrl4 = 0,
	. pll = 0x23,	//	0x02,		//0x23,
	.duty = 0x41,		//0x71,
	.period = 0x82,	//0x74,
};


static struct i2c_board_info msm_isa1200_board_info[] = {
	{
		I2C_BOARD_INFO("isa1200", 0x90>>1),
		.platform_data = &isa1200_vibrator_pdata,
	},
};

#if 1//def CONFIG_MSM_CAMERA //samsung LTE
#define VFE_CAMIF_TIMER1_GPIO 29
#define VFE_CAMIF_TIMER2_GPIO 30
#define VFE_CAMIF_TIMER3_GPIO_INT 31

static uint32_t camera_off_gpio_table[] = {
	/* parallel CAMERA interfaces */
	GPIO_CFG(57,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /*CAM_PMIC_EN1 */	
	GPIO_CFG(50,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* 5M_RST */
	GPIO_CFG(49,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* 5M_STBY */
	GPIO_CFG(41,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_VGA_RST */	
	GPIO_CFG(42,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_VGA_STBY */		
	GPIO_CFG(137,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_FLASH_EN */
	GPIO_CFG(142,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_MOVIE_EN */

#if	0 // MIPI Interface
	GPIO_CFG(4,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_CLK_N */
	GPIO_CFG(5,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_CLK_P */
	GPIO_CFG(6,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_D0_N */
	GPIO_CFG(7,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_D0_P */
	GPIO_CFG(8,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_D1_N */
	GPIO_CFG(9,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_D0_P */
	
	GPIO_CFG(4,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* SUB_CAM_CLK_N */
	GPIO_CFG(5,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* SUB_CAM_CLK_P */
	GPIO_CFG(6,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* SUB_CAM_D0_N */
	GPIO_CFG(7,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* SUB_CAM_D0_P */	
#endif	
	GPIO_CFG(32, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), /* CAM_MCLK_F */
};

static uint32_t camera_on_gpio_table[] = {
	/* parallel CAMERA interfaces */
	GPIO_CFG(57,  0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), /*CAM_PMIC_EN1 */
	GPIO_CFG(50,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* 5M_RST */
	GPIO_CFG(49,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), /* 5M_STBY */
	GPIO_CFG(41,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_VGA_RST */	
	GPIO_CFG(42,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), /* CAM_VGA_STBY */	
	GPIO_CFG(137,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), /* CAM_FLASH_EN */
	GPIO_CFG(142,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), /* CAM_MOVIE_EN */

#if	0 // MIPI Interface
	GPIO_CFG(4,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_CLK_N */
	GPIO_CFG(5,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_CLK_P */
	GPIO_CFG(6,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_D0_N */
	GPIO_CFG(7,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_D0_P */
	GPIO_CFG(8,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_D1_N */
	GPIO_CFG(9,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_D0_P */
	
	GPIO_CFG(4,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* SUB_CAM_CLK_N */
	GPIO_CFG(5,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* SUB_CAM_CLK_P */
	GPIO_CFG(6,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* SUB_CAM_D0_N */
	GPIO_CFG(7,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* SUB_CAM_D0_P */	
#endif
	GPIO_CFG(32, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_4MA), /* CAM_MCLK */

//	GPIO_CFG(48, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA), /* I2C SCL */
//	GPIO_CFG(47, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA), /* I2C SDA */

};

static void config_gpio_table(uint32_t *table, int len)
{
	int n, rc;
	for (n = 0; n < len; n++) {
		rc = gpio_tlmm_config(table[n], GPIO_CFG_ENABLE);
		if (rc) {
			pr_err("%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, table[n], rc);
			break;
		}
	}
}

static int config_camera_on_gpios(void)
{
	config_gpio_table(camera_on_gpio_table,
		ARRAY_SIZE(camera_on_gpio_table));
	return 0;
}

static void config_camera_off_gpios(void)
{
	config_gpio_table(camera_off_gpio_table,
		ARRAY_SIZE(camera_off_gpio_table));

}


struct resource msm_camera_resources[] = {
	{
		.start	= 0x04500000,
		.end	= 0x04500000 + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= VFE_IRQ,
		.end	= VFE_IRQ,
		.flags	= IORESOURCE_IRQ,
	},
};


struct msm_camera_device_platform_data msm_camera_device_data = {
	.camera_gpio_on  = config_camera_on_gpios,
	.camera_gpio_off = config_camera_off_gpios,
	.ioext.csiphy = 0x04800000,
	.ioext.csisz  = 0x00000400,
	.ioext.csiirq = CSI_0_IRQ,
	.ioclk.mclk_clk_rate = 24000000,
	.ioclk.vfe_clk_rate  = 228570000,
#ifdef CONFIG_MSM_BUS_SCALING
	.cam_bus_scale_table = &cam_bus_client_pdata,
#endif
};

static struct msm_camera_sensor_flash_src msm_flash_src = {
	.flash_sr_type = MSM_CAMERA_FLASH_SRC_PWM,
	._fsrc.pwm_src.freq  = 1000,
	._fsrc.pwm_src.max_load = 300,
	._fsrc.pwm_src.low_load = 30,
	._fsrc.pwm_src.high_load = 100,
	._fsrc.pwm_src.channel = 7,
};

#endif //samsung

#ifdef CONFIG_SENSOR_S5K6AAFX
static struct msm_camera_sensor_platform_info s5k6aafx_sensor_8660_info = {
	.mount_angle = 0
};

static struct msm_camera_sensor_flash_data flash_s5k6aafx = {
//	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_type = MSM_CAMERA_FLASH_NONE,
	.flash_src  = &msm_flash_src
};


struct msm_camera_device_platform_data msm_camera_device_data_sub_cam = {
	.camera_gpio_on  = config_camera_on_gpios,
	.camera_gpio_off = config_camera_off_gpios,
	.ioext.csiphy = 0x04900000,
	.ioext.csisz  = 0x00000400,
	.ioext.csiirq = CSI_1_IRQ,
	.ioclk.mclk_clk_rate = 24000000,
	.ioclk.vfe_clk_rate  = 228570000,
#ifdef CONFIG_MSM_BUS_SCALING
	.cam_bus_scale_table = &cam_bus_client_pdata,
#endif
};


static struct msm_camera_sensor_info msm_camera_sensor_s5k6aafx_data = {
	.sensor_name    = "s5k6aafx",
	.sensor_reset   = 41,
	.sensor_pwd     = 37,
	.vcm_pwd        = 1,
	.vcm_enable	= 0,
	.mclk		= 24000000,
	.pdata          = &msm_camera_device_data_sub_cam,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_s5k6aafx,
	.sensor_platform_info = &s5k6aafx_sensor_8660_info,
	.csi_if         = 1
};
static struct platform_device msm_camera_sensor_s5k6aafx = {
	.name  	= "msm_camera_s5k6aafx",
	.dev   	= {
		.platform_data = &msm_camera_sensor_s5k6aafx_data,
	},
};
#endif




//////////////////////////////////////////////////////////////////////////////
// P5LTE
#ifdef CONFIG_SENSOR_S5K5CCAF
static int camera_power_maincam(int onoff)
{
	printk("main cam fail :%s \n", onoff ? "ON" : "OFF");
	return 0;
}

static struct msm_camera_sensor_platform_info s5k5ccaf_sensor_8660_info = {
	.mount_angle 	= 0,
	.sensor_reset	= 50,
	.sensor_pwd	= 37,
	.vcm_pwd	= 0,
	.vcm_enable	= 0,
	.sensor_power_control = camera_power_maincam,
};

static struct msm_camera_sensor_flash_data flash_s5k5ccaf = {
	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_src  = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k5ccaf_data = {
	.sensor_name    = "s5k5ccaf",
	.sensor_reset   = 50,
	//.sensor_pwd     = 37,
	.vcm_pwd        = 1,
	.vcm_enable	= 0,
	.mclk		= 24000000,
	.pdata          = &msm_camera_device_data,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_s5k5ccaf,
	.sensor_platform_info = &s5k5ccaf_sensor_8660_info,
	.csi_if         = 1
};
static struct platform_device msm_camera_sensor_s5k5ccaf = {
	.name  	= "msm_camera_s5k5ccaf",
	.dev   	= {
		.platform_data = &msm_camera_sensor_s5k5ccaf_data,
	},
};
#endif

#ifdef CONFIG_SENSOR_S5K5BAFX_P5
static int camera_power_subcam(int onoff)
{
	printk("sub cam fail :%s \n", onoff ? "ON" : "OFF");
	return 0;
}

static struct msm_camera_sensor_platform_info s5k5bafx_sensor_8660_info = {
	.mount_angle 	= 0,
	.sensor_reset	= 41,
	.sensor_pwd	= 37,
	.vcm_pwd	= 1,
	.vcm_enable	= 0,
	.sensor_power_control = camera_power_subcam,
};

static struct msm_camera_sensor_flash_data flash_s5k5bafx = {
//	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_type = MSM_CAMERA_FLASH_NONE,
	.flash_src  = &msm_flash_src
};


struct msm_camera_device_platform_data msm_camera_device_data_sub_cam = {
	.camera_gpio_on  = config_camera_on_gpios,
	.camera_gpio_off = config_camera_off_gpios,
	.ioext.csiphy = 0x04900000,
	.ioext.csisz  = 0x00000400,
	.ioext.csiirq = CSI_1_IRQ,
	.ioclk.mclk_clk_rate = 24000000,
	.ioclk.vfe_clk_rate  = 228570000,
#ifdef CONFIG_MSM_BUS_SCALING
	.cam_bus_scale_table = &cam_bus_client_pdata,
#endif	
};


static struct msm_camera_sensor_info msm_camera_sensor_s5k5bafx_data = {
	.sensor_name    = "s5k5bafx",
	.sensor_reset   = 41,
//	.sensor_pwd     = 37,
	.vcm_pwd        = 1,
	.vcm_enable	= 0,
	.mclk		= 24000000,
	.pdata          = &msm_camera_device_data_sub_cam,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_s5k5bafx,
	.sensor_platform_info = &s5k5bafx_sensor_8660_info,
	.csi_if         = 1
};
static struct platform_device msm_camera_sensor_s5k5bafx = {
	.name  	= "msm_camera_s5k5bafx",
	.dev   	= {
		.platform_data = &msm_camera_sensor_s5k5bafx_data,
	},
};
#endif

#ifdef CONFIG_S5K4ECGX
static struct msm_camera_sensor_flash_data flash_s5k4ecgx = {
//	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_type = MSM_CAMERA_FLASH_NONE,
	.flash_src  = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k4ecgx_data = {
	.sensor_name    = "s5k4ecgx",
	.sensor_reset   = 50,
	.sensor_pwd     = 37,
	.vcm_pwd        = 1,
	.vcm_enable	= 0,
	.mclk		= 24000000,
	.pdata          = &msm_camera_device_data,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_s5k4ecgx,
	.csi_if         = 1
};
static struct platform_device msm_camera_sensor_s5k4ecgx = {
	.name  	= "msm_camera_s5k4ecgx",
	.dev   	= {
		.platform_data = &msm_camera_sensor_s5k4ecgx_data,
	},
};
#endif

//////////////////////////////////////////////////////////////////////////////

#ifdef CONFIG_SENSOR_S5K5AAFA
static struct msm_camera_sensor_flash_data flash_s5k5aafa = {
//	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_type = MSM_CAMERA_FLASH_NONE,
	.flash_src  = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k5aafa_data = {
	.sensor_name    = "s5k5aafa",
	.sensor_reset   = 0,
	.sensor_pwd     = 37,
	.vcm_pwd        = 1,
	.vcm_enable		= 0,
	.pdata          = &msm_camera_device_data,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_s5k5aafa,
	.csi_if         = 1
};
static struct platform_device msm_camera_sensor_s5k5aafa = {
	.name  	= "msm_camera_s5k5aafa",
	.dev   	= {
		.platform_data = &msm_camera_sensor_s5k5aafa_data,
	},
};
#endif

#ifdef CONFIG_S5K3E2FX
static struct msm_camera_sensor_flash_data flash_s5k3e2fx = {
	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_src  = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k3e2fx_data = {
	.sensor_name    = "s5k3e2fx",
	.sensor_reset   = 0,
	.sensor_pwd     = 85,
	.vcm_pwd        = 1,
	.vcm_enable     = 0,
	.pdata          = &msm_camera_device_data,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_s5k3e2fx,
	.csi_if         = 0
};

static struct platform_device msm_camera_sensor_s5k3e2fx = {
	.name      = "msm_camera_s5k3e2fx",
	.dev       = {
		.platform_data = &msm_camera_sensor_s5k3e2fx_data,
	},
};
#endif

#ifdef CONFIG_MSM_GEMINI
static struct resource msm_gemini_resources[] = {
	{
		.start  = 0x04600000,
		.end    = 0x04600000 + SZ_1M - 1,
		.flags  = IORESOURCE_MEM,
	},
	{
		.start  = INT_JPEG,
		.end    = INT_JPEG,
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device msm_gemini_device = {
	.name           = "msm_gemini",
	.resource       = msm_gemini_resources,
	.num_resources  = ARRAY_SIZE(msm_gemini_resources),
};
#endif

#ifdef CONFIG_I2C_QUP
#define GSBI7_SDA 59
#define GSBI7_SCL 60

static uint32_t gsbi7_gpio_table[] = {
	GPIO_CFG(GSBI7_SDA, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
	GPIO_CFG(GSBI7_SCL, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
};

static uint32_t gsbi7_i2c_table[] = {
	GPIO_CFG(GSBI7_SDA, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
	GPIO_CFG(GSBI7_SCL, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
};

static void gsbi_qup_i2c_gpio_config(int adap_id, int config_type)
{
}

static void gsbi7_qup_i2c_gpio_config(int adap_id, int config_type)
{
	if (config_type == 0) {
		gpio_tlmm_config(gsbi7_gpio_table[0], GPIO_CFG_ENABLE);
		gpio_tlmm_config(gsbi7_gpio_table[1], GPIO_CFG_ENABLE);
	} else {
		gpio_tlmm_config(gsbi7_i2c_table[0], GPIO_CFG_ENABLE);
		gpio_tlmm_config(gsbi7_i2c_table[1], GPIO_CFG_ENABLE);
	}
}

static struct msm_i2c_platform_data msm_gsbi3_qup_i2c_pdata = {
	.clk_freq = 384000,
	.src_clk_rate = 24000000,
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};

static struct msm_i2c_platform_data msm_gsbi4_qup_i2c_pdata = {
	.clk_freq = 400000, //100000,
	.src_clk_rate = 24000000,
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};

static struct msm_i2c_platform_data msm_gsbi7_qup_i2c_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
	.clk = "gsbi_qup_clk",
	.pclk = "gsbi_pclk",
	.pri_clk = 60,
	.pri_dat = 59,
	.msm_i2c_config_gpio = gsbi7_qup_i2c_gpio_config,
};

static struct msm_i2c_platform_data msm_gsbi8_qup_i2c_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};

static struct msm_i2c_platform_data msm_gsbi9_qup_i2c_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};

static struct msm_i2c_platform_data msm_gsbi12_qup_i2c_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
	.use_gsbi_shared_mode = 1,
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};
#endif

#if defined(CONFIG_SPI_QUP) || defined(CONFIG_SPI_QUP_MODULE)
static struct msm_spi_platform_data msm_gsbi1_qup_spi_pdata = {
	.max_clock_speed = 24000000,
};

static struct msm_spi_platform_data msm_gsbi10_qup_spi_pdata = {
	.max_clock_speed = 24000000,
};
#endif

#if defined(CONFIG_TDMB) || defined(CONFIG_TDMB_MODULE)

static uint32_t tdmb_on_gpio_table[] = {
	GPIO_CFG(GPIO_TDMB_EN,  GPIOMUX_FUNC_GPIO, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_TDMB_RST,  GPIOMUX_FUNC_GPIO, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_TDMB_INT,  GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 
	GPIO_CFG(GPIO_TDMB_SPI_MOSI,  GPIOMUX_FUNC_1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), 
	GPIO_CFG(GPIO_TDMB_SPI_MISO,  GPIOMUX_FUNC_1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), 
	GPIO_CFG(GPIO_TDMB_SPI_CS,  GPIOMUX_FUNC_1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), 
	GPIO_CFG(GPIO_TDMB_SPI_CLK,  GPIOMUX_FUNC_1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), 

};
static uint32_t tdmb_off_gpio_table[] = {
	GPIO_CFG(GPIO_TDMB_EN,  GPIOMUX_FUNC_GPIO, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_TDMB_RST,  GPIOMUX_FUNC_GPIO, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_TDMB_INT,  GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 
	GPIO_CFG(GPIO_TDMB_SPI_MOSI,  GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_8MA), 
	GPIO_CFG(GPIO_TDMB_SPI_MISO,  GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_8MA), 
	GPIO_CFG(GPIO_TDMB_SPI_CS,  GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_8MA), 
	GPIO_CFG(GPIO_TDMB_SPI_CLK,  GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_8MA), 
};

static void tdmb_gpio_on(void)
{
	printk("tdmb_gpio_on\n");

	config_gpio_table(tdmb_on_gpio_table,	ARRAY_SIZE(tdmb_on_gpio_table));

	gpio_set_value(GPIO_TDMB_EN, 1);
	msleep(10);
	gpio_set_value(GPIO_TDMB_RST, 0);
	msleep(2);
	gpio_set_value(GPIO_TDMB_RST, 1);
	msleep(10);

}

static void tdmb_gpio_off(void)
{
	printk("tdmb_gpio_off\n");

	gpio_set_value(GPIO_TDMB_RST, 0);
	msleep(1);
	gpio_set_value(GPIO_TDMB_EN, 0);

	config_gpio_table(tdmb_off_gpio_table,	ARRAY_SIZE(tdmb_off_gpio_table));

}

static struct tdmb_platform_data tdmb_pdata = {
	.gpio_on = tdmb_gpio_on,
	.gpio_off = tdmb_gpio_off,
	.irq = MSM_GPIO_TO_INT(GPIO_TDMB_INT),
};

static struct platform_device tdmb_device = {
	.name			= "tdmb",
	.id 			= -1,
	.dev			= {
		.platform_data = &tdmb_pdata,
	},
};

static struct spi_board_info tdmb_spi_info[] __initdata = {	
    {	
        .modalias       = "tdmbspi",
        .mode           = SPI_MODE_0,
        .bus_num        = 0,
        .chip_select    = 0,
        .max_speed_hz   = 5400000,
    }
};

static int __init tdmb_dev_init(void)
{
	if(charging_mode_from_boot == 1)
		return 0;

	config_gpio_table(tdmb_off_gpio_table,	ARRAY_SIZE(tdmb_off_gpio_table));

	gpio_request(GPIO_TDMB_EN, "TDMB_EN");
	gpio_direction_output(GPIO_TDMB_EN, 0);
	gpio_request(GPIO_TDMB_RST, "TDMB_RST");
	gpio_direction_output(GPIO_TDMB_RST, 0);
	gpio_request(GPIO_TDMB_INT, "TDMB_INT");
	gpio_direction_input(GPIO_TDMB_INT);

	platform_device_register(&tdmb_device);

	if (spi_register_board_info(tdmb_spi_info, ARRAY_SIZE(tdmb_spi_info)) != 0) {
		pr_err("%s: spi_register_board_info returned error\n", __func__);
	}

    return 0;
}
#endif 

#ifdef CONFIG_I2C_SSBI
/* CODEC/TSSC SSBI */
static struct msm_i2c_ssbi_platform_data msm_ssbi3_pdata = {
	.controller_type = MSM_SBI_CTRL_SSBI,
};
#endif

#ifdef CONFIG_BATTERY_MSM
/* Use basic value for fake MSM battery */
static struct msm_psy_batt_pdata msm_psy_batt_data = {
	.avail_chg_sources = AC_CHG,
};

static struct platform_device msm_batt_device = {
	.name              = "msm-battery",
	.id                = -1,
	.dev.platform_data = &msm_psy_batt_data,
};
#endif


#if 1  // TEMP for compile

// TEMP for compile
void yda165_avdd_power_on(void)
{
	return;
}

void yda165_avdd_power_off(void)
{
	return;
}

// TEMP for compile
int usb_access_lock = 0;
EXPORT_SYMBOL(usb_access_lock);

// TEMP for compile
int sec_switch_get_attached_device(void)
{
	return 0;
}
EXPORT_SYMBOL(sec_switch_get_attached_device);


unsigned int is_lpcharging_state(void)
{
	u32 val = charging_mode_from_boot;
	
	pr_info("%s: is_lpm_boot:%d\n", __func__, val);

	return val;
}
EXPORT_SYMBOL(is_lpcharging_state);

void disable_charging_before_reset(void)
{
	// TEMP for compile
}

#endif // TEMP for compile

int64_t PM8058_get_adc(int channel)
{
	int ret;
	void *h;
	struct adc_chan_result adc_chan_result;
	struct completion  conv_complete_evt;

	pr_debug("%s: called for %d\n", __func__, channel);
	ret = adc_channel_open(channel, &h);
	if (ret) {
		pr_err("%s: couldnt open channel %d ret=%d\n",
					__func__, channel, ret);
		goto out;
	}
	init_completion(&conv_complete_evt);
	ret = adc_channel_request_conv(h, &conv_complete_evt);
	if (ret) {
		pr_err("%s: couldnt request conv channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	ret = wait_for_completion_interruptible(&conv_complete_evt);
	if (ret) {
		pr_err("%s: wait interrupted channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	ret = adc_channel_read_result(h, &adc_chan_result);
	if (ret) {
		pr_err("%s: couldnt read result channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
	ret = adc_channel_close(h);
	if (ret) {
		pr_err("%s: couldnt close channel %d ret=%d\n",
						__func__, channel, ret);
	}

	pr_debug("%s: done for %d\n", __func__, channel);

#if 0
	printk("adc_chan_result.chan : %d\n", adc_chan_result.chan);
	printk("adc_chan_result.adc_code : %d\n", adc_chan_result.adc_code);	
	printk("adc_chan_result.measurement : %lld\n", adc_chan_result.measurement);
	printk("adc_chan_result.physical : %lld\n", adc_chan_result.physical);	
#endif 	

	return adc_chan_result.measurement;

out:
	pr_err("%s: go to out, error %d\n", __func__, channel);
	return -EINVAL;

}

#ifdef CONFIG_STMPE811_ADC
extern s16 stmpe811_adc_get_value(u8 channel);

#define GPIO_ADC_SCL_OLD		33
#define GPIO_ADC_SDA_OLD		35
#if defined(CONFIG_KOR_OPERATOR_LGU)
#define GPIO_ADC_SCL		73
#define GPIO_ADC_SDA		169
#else
#define GPIO_ADC_SCL	77
#define GPIO_ADC_SDA	83
#endif

static struct i2c_board_info stmpe811_i2c_borad_info[] = {
	{
		I2C_BOARD_INFO("stmpe811", 0x41),
	},
};

static struct i2c_gpio_platform_data stmpe811_i2c_gpio_data= {
	.sda_pin		= GPIO_ADC_SDA,
	.scl_pin		= GPIO_ADC_SCL,
	.udelay 		= 2,
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

static struct platform_device stmpe811_i2c_gpio_device = {
	.name			= "i2c-gpio",
	.id			= MSM_STMPE811_I2C_BUS_ID,
	.dev.platform_data	= &stmpe811_i2c_gpio_data,
};

void stmpe811_ldo_en(bool enable)
{
	//VIO = vreg 3.3v
	static struct regulator *pm8058_l2;
	int ret = 0;

	pm8058_l2 = regulator_get(NULL, "8058_l2");
	ret = IS_ERR(pm8058_l2);
	if (ret) {
		pr_err("%s: 8058_l2 error = %d\r\n", __func__, ret);
	}

	ret = regulator_set_voltage(pm8058_l2, 3300000, 3300000);
	if (ret) {
		pr_err("%s: unable to set the voltage for regulator"
				"pm8058_l2\n", __func__);
		regulator_put(pm8058_l2);
		return;
	}

	if (enable)
		ret = regulator_enable(pm8058_l2);
	else
		ret = regulator_disable(pm8058_l2);
	
	if (ret) {
		pr_err("%s: unable to enable regulator"
				"pm8058_l2\n", __func__);
		regulator_put(pm8058_l2);
	}
}

void stmpe811_init(void)
{
	pr_info("%s\r\n", __func__);
#if defined(CONFIG_USA_OPERATOR_ATT) || defined(CONFIG_KOR_OPERATOR_SKT)
	if (system_rev == 0x01) {
		stmpe811_i2c_gpio_data.scl_pin = GPIO_ADC_SCL_OLD;
		stmpe811_i2c_gpio_data.sda_pin = GPIO_ADC_SDA_OLD;		
	} 
#elif defined(CONFIG_KOR_OPERATOR_LGU)
	if (system_rev < 0x04){
		stmpe811_i2c_gpio_data.scl_pin = GPIO_ADC_SCL_OLD;
		stmpe811_i2c_gpio_data.sda_pin = GPIO_ADC_SDA_OLD;	
	}
#endif
}

void stmpe811_gpio_init(void)
{
	pr_info("%s\r\n", __func__);

#if defined(CONFIG_USA_OPERATOR_ATT) || defined(CONFIG_KOR_OPERATOR_SKT) 
	if (system_rev == 0x01) {
		gpio_tlmm_config(GPIO_CFG(GPIO_ADC_SCL_OLD,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(GPIO_ADC_SDA_OLD,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),GPIO_CFG_ENABLE);
	} else if (system_rev >= 0x03) {	
		gpio_tlmm_config(GPIO_CFG(GPIO_ADC_SCL,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(GPIO_ADC_SDA,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),GPIO_CFG_ENABLE);
	}
#elif defined(CONFIG_KOR_OPERATOR_LGU)
	if (system_rev >= 0x04) {
		gpio_tlmm_config(GPIO_CFG(GPIO_ADC_SCL,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(GPIO_ADC_SDA,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),GPIO_CFG_ENABLE);
	} else {
		gpio_tlmm_config(GPIO_CFG(GPIO_ADC_SCL_OLD,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(GPIO_ADC_SDA_OLD,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),GPIO_CFG_ENABLE);
	}
#endif
	pr_info("init GPIO%d_CONFIG(0x%x),\r\n",stmpe811_i2c_gpio_data.scl_pin, readl(GPIO_CONFIG(stmpe811_i2c_gpio_data.scl_pin)));
	pr_info("init GPIO%d_CONFIG(0x%x),\r\n",stmpe811_i2c_gpio_data.sda_pin, readl(GPIO_CONFIG(stmpe811_i2c_gpio_data.sda_pin)));
}
#endif //CONFIG_STMPE811_ADC

static int check_using_stmpe811(void)
{
	int ret =0 ;
#ifdef CONFIG_STMPE811_ADC
#if defined(CONFIG_USA_OPERATOR_ATT) || defined(CONFIG_KOR_OPERATOR_SKT) 
	ret = (system_rev >= 0x0003) ? 1 : 0;
#elif defined(CONFIG_KOR_OPERATOR_LGU)
#if 1	/* Using STMPE811 */
	ret = (system_rev >= 0x0004) ? 1 : 0;
#else	/* Using PMIC */
	ret = 0;
#endif
#endif
#endif
	return ret;
}

#ifdef CONFIG_BATTERY_P5LTE
#define TA_DETECT		PM8058_GPIO(8)
#define TA_CURRENT_SEL	PM8058_GPIO(18)
#define CHG_EN			PM8058_GPIO(23)
#define CHG_STATE		PM8058_GPIO(24)

#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_KT) || defined(CONFIG_KOR_OPERATOR_LGU)
static int64_t cable_adc;
#endif
static int usb_connected = 0;
static struct mutex adc_lock;
atomic_t charger_checking = ATOMIC_INIT(0);

#if defined(CONFIG_KOR_OPERATOR_LGU)
static void __init usb_switch_init(void);
#endif
static bool check_samsung_charger(void)
{
	int i=0;
	bool ret =0; //usb
	int64_t adc = 0;
	int sum=0;

	mutex_lock(&adc_lock);

	/* set usb path to ADC */
	set_usb_path(USB_SEL_ADC);
	atomic_set(&charger_checking, 1);

#if defined(CONFIG_KOR_OPERATOR_LGU)
	/* check usb path */
	if (USB_SEL_ADC != get_usb_path()) {
		usb_switch_init();
		set_usb_path(USB_SEL_ADC);
	}
#endif

	mdelay(100);

	/* read ADC value */
	if (check_using_stmpe811()) {
#ifdef CONFIG_STMPE811_ADC
		for(i = 0; i <3; i++) {
			sum += stmpe811_adc_get_value(4);
		}
		pr_info("++ battery adc sum =%d\r\n",sum);
		sum =  sum /3 ;
		pr_info("++ battery adc sum/3=%d\r\n",sum);
#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_KT) || defined(CONFIG_KOR_OPERATOR_LGU)
		cable_adc = sum;
#endif
		if ((sum > 800) && (sum < 1800)) {
			usb_connected = 0;
			ret=1; //TA charging
		} else {
			usb_connected = 1;
			ret=0; //usb charging
		}
		mdelay(50);
#endif
	} else {
		mdelay(150);
		for(i = 0; i <10; i++) {
			adc += PM8058_get_adc(CHANNEL_ADC_CABLE_CHECK);
			pr_debug("[%d]++ battery adc =%lld\r\n", i, adc);
		}
#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_KT) || defined(CONFIG_KOR_OPERATOR_LGU)
		cable_adc = adc;
#endif
		mdelay(30);

		if (adc<=100) {
			usb_connected = 1;
			ret=0; //usb charging
		} else {
			usb_connected = 0;
			ret=1; //TA charging
		}
	}

	/* set usb path to MSM */
	set_usb_path(USB_SEL_MSM);

	atomic_set(&charger_checking, 0);
	mutex_unlock(&adc_lock);

#ifdef __ABS_TIMER_TEST__
	pr_info("ABS_TIMER_TEST: forcely set to TA\r\n");
	return 1;
#endif
	return ret; // TODO..
}

void p5_bat_gpio_init(void)
{
	/* this function is called lately. so pm8058_gpios_init() function includes bat GPIO init code*/
	int cable_detect=0;
	int samsung_ta =0;
	cable_detect = !gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(TA_DETECT));

	if (cable_detect) {	//TA or USB
		samsung_ta = check_samsung_charger();
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(CHG_EN), 0);
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(TA_CURRENT_SEL), samsung_ta);
	} else {	//no cable
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(CHG_EN), 1);
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(TA_CURRENT_SEL), 0);
	}

	pr_info("Battery GPIO initialized\n");	
}

#if defined(CONFIG_TOUCHSCREEN_MELFAS)
static struct tsp_callbacks * charger_callbacks;
static void  p5_inform_charger_connection(int mode)
{
// TEMP	if (charger_callbacks && charger_callbacks->inform_charger)
// TEMP		charger_callbacks->inform_charger(charger_callbacks, mode);
};
#endif

static struct p5_battery_platform_data p5_battery_platform = {
	.charger = {
		.enable_line = PM8058_GPIO_PM_TO_SYS(CHG_EN),
		.connect_line = PM8058_GPIO_PM_TO_SYS(TA_DETECT),
		.fullcharge_line = PM8058_GPIO_PM_TO_SYS(CHG_STATE),
		.currentset_line = PM8058_GPIO_PM_TO_SYS(TA_CURRENT_SEL),
	},
	.check_dedicated_charger = check_samsung_charger,
	.init_charger_gpio = p5_bat_gpio_init,
#if defined(CONFIG_TOUCHSCREEN_MELFAS)
	.inform_charger_connection = p5_inform_charger_connection,
#endif
#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_KT) || defined(CONFIG_KOR_OPERATOR_LGU)
	.check_stmpe811 = check_using_stmpe811,
	.cable_adc_check = &cable_adc,
#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_KT)
	.temp_high_threshold = 58500,
	.temp_high_recovery = 45200,
	.temp_low_recovery = 2500,
	.temp_low_threshold = -2000,
#else	// CONFIG_KOR_OPERATOR_LGU
	.temp_high_threshold = 59000,
	.temp_high_recovery = 45600,
	.temp_low_recovery = 1700,
	.temp_low_threshold = -4000,
#endif
	.charge_duration = 10*60*60,		/* 10 hour */
	.recharge_duration = 2*60*60,		/* 2 hour */
#else
#if defined(CONFIG_EUR_OPERATOR_OPEN)
	.temp_high_threshold = 50000,		/* 50 'C */
	.temp_high_recovery = 43000,		/* 43 'C */
	.temp_low_recovery = 0,			/* 0 'C */
	.temp_low_threshold = -5000,			/* -5 'C */
	.charge_duration = 10*60*60,		/* 10 hour */
	.recharge_duration = 1.5*60*60,		/* 1.5 hour */
#else
	.temp_high_threshold = 50000,		/* 50 'C */
	.temp_high_recovery = 46000,		/* 46 'C */
	.temp_low_recovery = 2000,			/* 2 'C */
	.temp_low_threshold = 0,			/* 0 'C */
	.charge_duration = 10*60*60,		/* 10 hour */
	.recharge_duration = 1.5*60*60,		/* 1.5 hour */
#endif
#endif
	.recharge_voltage = 4150,			/*4.15V */
};

static struct platform_device p5_battery_device = {
	.name = "p5-battery",
	.id = -1,
	.dev = {
		.platform_data = &p5_battery_platform,
	},
};
#endif

#ifdef CONFIG_FB_MSM_LCDC_DSUB
/* VGA = 1440 x 900 x 4(bpp) x 2(pages)
   prim = 1024 x 600 x 4(bpp) x 2(pages)
   This is the difference. */
#define MSM_FB_DSUB_PMEM_ADDER (0xA32000-0x4B0000)
#else
#define MSM_FB_DSUB_PMEM_ADDER (0)
#endif

/* Sensors DSPS platform data */
#ifdef CONFIG_MSM_DSPS

static struct dsps_gpio_info dsps_surf_gpios[] = {
	{
		.name = "compass_rst_n",
		.num = GPIO_COMPASS_RST_N,
		.on_val = 1,	/* device not in reset */
		.off_val = 0,	/* device in reset */
	},
	{
		.name = "gpio_r_altimeter_reset_n",
		.num = GPIO_R_ALTIMETER_RESET_N,
		.on_val = 1,	/* device not in reset */
		.off_val = 0,	/* device in reset */
	}
};

static struct dsps_gpio_info dsps_fluid_gpios[] = {
	{
		.name = "gpio_n_altimeter_reset_n",
		.num = GPIO_N_ALTIMETER_RESET_N,
		.on_val = 1,	/* device not in reset */
		.off_val = 0,	/* device in reset */
	}
};

static void __init msm8x60_init_dsps(void)
{
	struct msm_dsps_platform_data *pdata =
		msm_dsps_device.dev.platform_data;
	/*
	 * On Fluid the Compass sensor Chip-Select (CS) is directly connected
	 * to the power supply and not controled via GPIOs. Fluid uses a
	 * different IO-Expender (north) than used on surf/ffa.
	 */
	if (machine_is_msm8x60_fluid()) {
		/* fluid has different firmware, gpios */
		pdata->pil_name = DSPS_PIL_FLUID_NAME;
		pdata->gpios = dsps_fluid_gpios;
		pdata->gpios_num = ARRAY_SIZE(dsps_fluid_gpios);
	} else {
		pdata->pil_name = DSPS_PIL_GENERIC_NAME;
		pdata->gpios = dsps_surf_gpios;
		pdata->gpios_num = ARRAY_SIZE(dsps_surf_gpios);
	}

	platform_device_register(&msm_dsps_device);
}
#endif /* CONFIG_MSM_DSPS */

#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
/* prim = 1280 x 800 x 4(bpp) x 3(pages) */
#define MSM_FB_PRIM_BUF_SIZE 0xBB8000
#else
/* prim = 1280 x 800 x 4(bpp) x 2(pages) */
#define MSM_FB_PRIM_BUF_SIZE 0x7D0000
#endif

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL

#define MSM_FB_EXT_BUF_SIZE  (1920 * 1080 * 2 * 1) /* 2 bpp x 1 page */
#elif defined(CONFIG_FB_MSM_TVOUT)
#define MSM_FB_EXT_BUF_SIZE  (720 * 576 * 2 * 2) /* 2 bpp x 2 pages */
#else
#define MSM_FB_EXT_BUFT_SIZE	0
#endif

#ifdef CONFIG_FB_MSM_HDMI_AS_PRIMARY
/* 4 bpp x 2 page HDMI case */
#define MSM_FB_SIZE roundup((1920 * 1088 * 4 * 2), 4096)
#else
/* Note: must be multiple of 4096 */
#define MSM_FB_SIZE roundup(MSM_FB_PRIM_BUF_SIZE + MSM_FB_EXT_BUF_SIZE + \
				MSM_FB_DSUB_PMEM_ADDER, 4096)
#endif

#ifdef CONFIG_FB_MSM_HDMI_AS_PRIMARY
#define MSM_PMEM_SF_SIZE 0x8000000 /* 128 Mbytes */
#else
#define MSM_PMEM_SF_SIZE 0x5000000 /* 80 Mbytes */
#endif

#ifdef CONFIG_FB_MSM_OVERLAY0_WRITEBACK
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE roundup((1376 * 768 * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY0_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY0_WRITEBACK */

#ifdef CONFIG_FB_MSM_OVERLAY1_WRITEBACK
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE roundup((1920 * 1088 * 3 * 2), 4096)
#else
#define MSM_FB_OVERLAY1_WRITEBACK_SIZE (0)
#endif  /* CONFIG_FB_MSM_OVERLAY1_WRITEBACK */

//#define MSM_FB_PHYSICAL_BASE			0x40400000
#define MSM_PMEM_MDP_BASE			0x40400000
#define MSM_PMEM_MDP_SIZE				0x01E00000
#define MSM_ION_AUDIO_BASE			0x42300000
#define MSM_PMEM_KERNEL_EBI1_SIZE	0x600000
#define MSM_PMEM_AUDIO_BASE			0x42700000
#define MSM_PMEM_AUDIO_SIZE        		0x300000 // 3MB

#define RAM_CONSOLE_BASE_ADDR	0x42D80000

#define MSM_PMEM_ADSP_SIZE         0x3000000 // 48MB

#define MSM_SMI_BASE          0x38000000
#define MSM_SMI_SIZE          0x4000000

#define KERNEL_SMI_BASE       (MSM_SMI_BASE)
#define KERNEL_SMI_SIZE       0x700000

#define USER_SMI_BASE         (KERNEL_SMI_BASE + KERNEL_SMI_SIZE)
#define USER_SMI_SIZE         (MSM_SMI_SIZE - KERNEL_SMI_SIZE)
#define MSM_PMEM_SMIPOOL_SIZE USER_SMI_SIZE

#define MSM_ION_SF_SIZE		0x5000000 /* 80MB */
#define MSM_ION_CAMERA_SIZE     MSM_PMEM_ADSP_SIZE
#define MSM_ION_MM_FW_SIZE	0x200000 /* (2MB) */
#define MSM_ION_MM_SIZE		0x3600000 /* (54MB) */
#define MSM_ION_MFC_SIZE	SZ_8K
#define MSM_ION_WB_SIZE		0x1E000000 /* 30MB */ 
#define MSM_ION_AUDIO_SIZE	MSM_PMEM_AUDIO_SIZE

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
#define MSM_ION_HEAP_NUM	8
#else
#define MSM_ION_HEAP_NUM	1
#endif

static unsigned fb_size;
static int __init fb_size_setup(char *p)
{
	fb_size = memparse(p, NULL);
	return 0;
}
early_param("fb_size", fb_size_setup);

static unsigned pmem_kernel_ebi1_size = MSM_PMEM_KERNEL_EBI1_SIZE;
static int __init pmem_kernel_ebi1_size_setup(char *p)
{
	pmem_kernel_ebi1_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_kernel_ebi1_size", pmem_kernel_ebi1_size_setup);

#ifdef CONFIG_ANDROID_PMEM
static unsigned pmem_sf_size = MSM_PMEM_SF_SIZE;
static int __init pmem_sf_size_setup(char *p)
{
	pmem_sf_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_sf_size", pmem_sf_size_setup);

static unsigned pmem_adsp_size = MSM_PMEM_ADSP_SIZE;

static int __init pmem_adsp_size_setup(char *p)
{
	pmem_adsp_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_adsp_size", pmem_adsp_size_setup);

static unsigned pmem_audio_size = MSM_PMEM_AUDIO_SIZE;

static int __init pmem_audio_size_setup(char *p)
{
	pmem_audio_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_audio_size", pmem_audio_size_setup);
#endif

static struct resource msm_fb_resources[] = {
	{
		.flags  = IORESOURCE_DMA,
	}
};

#ifdef CONFIG_FB_MSM_LCDC_AUTO_DETECT
static int msm_fb_detect_panel(const char *name)
{
#if defined (CONFIG_FB_MSM_LCDC_P5LTE_WXGA_PANEL)
	if (!strcmp(name, "lcdc_p5lte_wxga"))
		return 0;
#else
	if (machine_is_msm8x60_fluid()) {
		if (!strncmp(name, "lcdc_samsung_oled", 20))
			return 0;
		if (!strncmp(name, "lcdc_samsung_wsvga", 20))
			return -ENODEV;
	} else {
		if (!strncmp(name, "lcdc_samsung_wsvga", 20))
			return 0;
		if (!strncmp(name, "lcdc_samsung_oled", 20))
			return -ENODEV;
	}

#endif		
	pr_warning("%s: not supported '%s'", __func__, name);
	return -ENODEV;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};
#endif /* CONFIG_FB_MSM_LCDC_AUTO_DETECT */

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources     = ARRAY_SIZE(msm_fb_resources),
	.resource          = msm_fb_resources,
#ifdef CONFIG_FB_MSM_LCDC_AUTO_DETECT
	.dev.platform_data = &msm_fb_pdata,
#endif /* CONFIG_FB_MSM_LCDC_AUTO_DETECT */
};

#ifdef CONFIG_ANDROID_PMEM
#ifndef CONFIG_MSM_MULTIMEDIA_USE_ION
static struct android_pmem_platform_data android_pmem_pdata = {
	.name = "pmem",
	.allocator_type = PMEM_ALLOCATORTYPE_ALLORNOTHING,
	.cached = 1,
	.memory_type = MEMTYPE_EBI1,
};

static struct platform_device android_pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = {.platform_data = &android_pmem_pdata},
};

static struct android_pmem_platform_data android_pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
	.memory_type = MEMTYPE_EBI1,
};

static struct platform_device android_pmem_adsp_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &android_pmem_adsp_pdata },
};
#endif
static struct android_pmem_platform_data android_pmem_audio_pdata = {
	.name = "pmem_audio",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
#ifdef CONFIG_SAMSUNG_MEMORY_LAYOUT_ARRANGE
	.memory_type = MEMTYPE_PMEM_AUDIO,
#else
	.memory_type = MEMTYPE_EBI1,
#endif	
};

static struct platform_device android_pmem_audio_device = {
	.name = "android_pmem",
	.id = 4,
	.dev = { .platform_data = &android_pmem_audio_pdata },
};

#define PMEM_BUS_WIDTH(_bw) \
	{ \
		.vectors = &(struct msm_bus_vectors){ \
			.src = MSM_BUS_MASTER_AMPSS_M0, \
			.dst = MSM_BUS_SLAVE_SMI, \
			.ib = (_bw), \
			.ab = 0, \
		}, \
	.num_paths = 1, \
	}

static struct msm_bus_paths mem_smi_table[] = {
	[0] = PMEM_BUS_WIDTH(0), /* Off */
	[1] = PMEM_BUS_WIDTH(1), /* On */
};

static struct msm_bus_scale_pdata smi_client_pdata = {
	.usecase = mem_smi_table,
	.num_usecases = ARRAY_SIZE(mem_smi_table),
	.name = "mem_smi",
};

int request_smi_region(void *data)
{
	int bus_id = (int) data;

	msm_bus_scale_client_update_request(bus_id, 1);
	return 0;
}

int release_smi_region(void *data)
{
	int bus_id = (int) data;

	msm_bus_scale_client_update_request(bus_id, 0);
	return 0;
}

void *setup_smi_region(void)
{
	return (void *)msm_bus_scale_register_client(&smi_client_pdata);
}
#ifndef CONFIG_MSM_MULTIMEDIA_USE_ION
static struct android_pmem_platform_data android_pmem_smipool_pdata = {
	.name = "pmem_smipool",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
	.memory_type = MEMTYPE_SMI,
	.request_region = request_smi_region,
	.release_region = release_smi_region,
	.setup_region = setup_smi_region,
	.map_on_demand = 1,
};
static struct platform_device android_pmem_smipool_device = {
	.name = "android_pmem",
	.id = 7,
	.dev = { .platform_data = &android_pmem_smipool_pdata },
};
#endif
#endif

#ifdef CONFIG_ANDROID_RAM_CONSOLE
static struct resource ram_console_resources[] = {
	{
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ram_console_device = {
	.name		= "ram_console",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(ram_console_resources),
	.resource	= ram_console_resources,
};
#endif

#define GPIO_DONGLE_PWR_EN 258

#ifdef CONFIG_FB_MSM_LCDC_SAMSUNG_OLED_PT
#ifdef CONFIG_SPI_QUP
static struct spi_board_info lcdc_samsung_spi_board_info[] __initdata = {
	{
		.modalias       = "lcdc_samsung_ams367pe02",
		.mode           = SPI_MODE_3,
		.bus_num        = 1,
		.chip_select    = 0,
		.max_speed_hz   = 10800000,
	}
};
#else
static int lcdc_spi_gpio_array_num[] = {
				73, /* spi_clk */
				72, /* spi_cs  */
				70, /* spi_mosi */
				};

static uint32_t lcdc_spi_gpio_config_data[] = {
	/* spi_clk */
	GPIO_CFG(73, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	/* spi_cs0 */
	GPIO_CFG(72, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	/* spi_mosi */
	GPIO_CFG(70, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
};

static void lcdc_config_spi_gpios(int enable)
{
	int n;
	for (n = 0; n < ARRAY_SIZE(lcdc_spi_gpio_config_data); ++n)
		gpio_tlmm_config(lcdc_spi_gpio_config_data[n], 0);
}
#endif

static struct msm_panel_common_pdata lcdc_samsung_oled_panel_data = {
#ifndef CONFIG_SPI_QUP
	.panel_config_gpio = lcdc_config_spi_gpios,
	.gpio_num          = lcdc_spi_gpio_array_num,
#endif
};

static struct platform_device lcdc_samsung_oled_panel_device = {
	.name   = "lcdc_samsung_oled",
	.id     = 0,
	.dev.platform_data = &lcdc_samsung_oled_panel_data,
};
#endif

#ifdef CONFIG_FB_MSM_LCDC_P5LTE_WXGA_PANEL

static int lcdc_gpio_array_num[] = {
				103, /* spi_clk */
				104, /* spi_cs  */
				106, /* spi_mosi */
				28, /* lcd_reset */
};

static struct msm_gpio lcdc_gpio_config_data[] = {
	{ GPIO_CFG(103, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "spi_clk" },
	{ GPIO_CFG(104, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "spi_cs0" },
	{ GPIO_CFG(106, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "spi_mosi" },
	{ GPIO_CFG(28, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "lcd_reset" },
};
extern int msm_gpios_request_enable(const struct msm_gpio *table, int size);
extern void msm_gpios_disable_free(const struct msm_gpio *table, int size);
static void lcdc_config_gpios(int enable)
{
        printk("p5lte : lcdc_config_gpios\n");
	if (enable) 
	{
	        int i;
	        int loop_count= ARRAY_SIZE(lcdc_gpio_config_data);
	        for( i=0; i<loop_count; i++)
	        {
	                gpio_tlmm_config(lcdc_gpio_config_data[i].gpio_cfg, 1);
	        }
#if 0	
		msm_gpios_request_enable(lcdc_gpio_config_data,
					      ARRAY_SIZE(
						      lcdc_gpio_config_data));
#endif						      
	} 
	else
	{
#if 0	
		msm_gpios_disable_free(lcdc_gpio_config_data,
					    ARRAY_SIZE(
						    lcdc_gpio_config_data));
#endif						    
        }
}

static struct msm_panel_common_pdata lcdc_panel_data = {
#ifndef CONFIG_SPI_QSD
	.panel_config_gpio = lcdc_config_gpios,
	.gpio_num          = lcdc_gpio_array_num,
#endif
};
static struct platform_device lcdc_p5lte_panel_device = {
	.name   = "lcdc_p5lte_wxga",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_panel_data,
	}
};
#endif

struct class *sec_class;
EXPORT_SYMBOL(sec_class);
struct device *switch_dev;
EXPORT_SYMBOL(switch_dev);

static void samsung_sys_class_init(void)
{
	pr_info("samsung sys class init.\n");

	sec_class = class_create(THIS_MODULE, "sec");

	if (IS_ERR(sec_class))
		pr_err("Failed to create class(sec)!\n");

	switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");

	if (IS_ERR(switch_dev))
		pr_err("Failed to create device(switch)!\n");

	pr_info("samsung sys class end.\n");
};

static int sec_switch_get_cable_status(void)
{
	if (gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(TA_DETECT)) == 1) {
		return 0; /* no cable */
	} else {
		if (usb_connected)
			return 1; /* USB CABLE */
		else
			return 0; /* Samsung Charger */
	}
}
static void sec_switch_set_usb_gadget_vbus(bool en)
{
	struct usb_gadget *gadget = platform_get_drvdata(&msm_device_gadget_peripheral);

	if (gadget) {
		if (en)
			usb_gadget_vbus_connect(gadget);
		else
			usb_gadget_vbus_disconnect(gadget);
	}
	else 
		pr_warning("%s: failed to get gadget\n", __func__);
}

static struct sec_switch_platform_data sec_switch_pdata = {
	.set_usb_gadget_vbus = sec_switch_set_usb_gadget_vbus,
	.get_cable_status = sec_switch_get_cable_status,
};

struct platform_device sec_device_switch = {
	.name	= "sec_switch",
	.id		= 1,
	.dev		= {
		.platform_data = &sec_switch_pdata,
	}
};

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
static struct resource hdmi_msm_resources[] = {
	{
		.name  = "hdmi_msm_qfprom_addr",
		.start = 0x00700000,
		.end   = 0x007060FF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_hdmi_addr",
		.start = 0x04A00000,
		.end   = 0x04A00FFF,
		.flags = IORESOURCE_MEM,
	},
	{
		.name  = "hdmi_msm_irq",
		.start = HDMI_IRQ,
		.end   = HDMI_IRQ,
		.flags = IORESOURCE_IRQ,
	},
};

static int hdmi_enable_5v(int on);
static int hdmi_core_power(int on, int show);
static int hdmi_cec_power(int on);

static struct msm_hdmi_platform_data hdmi_msm_data = {
	.irq = HDMI_IRQ,
        .gpio = HDMI_HPD_IRQ,
	.enable_5v = hdmi_enable_5v,
	.core_power = hdmi_core_power,
	.cec_power = hdmi_cec_power,
	.bootup_ck = 1,
};

static struct platform_device hdmi_msm_device = {
	.name = "hdmi_msm",
	.id = 0,
	.num_resources = ARRAY_SIZE(hdmi_msm_resources),
	.resource = hdmi_msm_resources,
	.dev.platform_data = &hdmi_msm_data,
};
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */

#ifdef CONFIG_FB_MSM_MIPI_DSI
static struct platform_device mipi_dsi_toshiba_panel_device = {
	.name = "mipi_toshiba",
	.id = 0,
};
#endif

#if defined(CONFIG_SEC_KEYBOARD_DOCK)

static struct sec_keyboard_callbacks *keyboard_callbacks;
static int check_sec_keyboard_dock(bool attached)
{
	if (keyboard_callbacks && keyboard_callbacks->check_keyboard_dock)
		return keyboard_callbacks->
			check_keyboard_dock(keyboard_callbacks, attached);
	return 0;
}

static void check_uart_path(bool en)
{
	if (en) {
		gpio_direction_output(UART_SEL_SW, 0);
		pr_info("[Keyboard] console uart path is switched to AP.\n");
	} else {
		gpio_direction_output(UART_SEL_SW, 1);
		pr_info("[Keyboard] console uart path is switched to CP.\n");
	}
}

static void sec_keyboard_register_cb(struct sec_keyboard_callbacks *cb)
{
	keyboard_callbacks = cb;
}

static struct sec_keyboard_platform_data kbd_pdata = {
	.acc_power = qc_acc_power,
	.accessory_irq_gpio = GPIO_ACCESSORY_INT,
	.check_uart_path = check_uart_path,
	.register_cb = sec_keyboard_register_cb,
	.wakeup_key = NULL,
};

static struct platform_device sec_keyboard = {
	.name	= "sec_keyboard",
	.id	= -1,
	.dev = {
		.platform_data = &kbd_pdata,
	}
};
#endif

static void __init msm8x60_allocate_memory_regions(void)
{
	void *addr;
	unsigned long size;
	int ret;

#ifdef CONFIG_ANDROID_RAM_CONSOLE
	ram_console_resources[0].start = RAM_CONSOLE_BASE_ADDR;
	ram_console_resources[0].end = RAM_CONSOLE_BASE_ADDR + SZ_512K - 1;
#endif	

	size = MSM_FB_SIZE;
#if 0 //defined(CONFIG_SAMSUNG_MEMORY_LAYOUT_ARRANGE)
	msm_fb_resources[0].start = MSM_FB_PHYSICAL_BASE;
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	pr_info("allocating %lu bytes at %lx physical for fb\n",
		size, MSM_FB_PHYSICAL_BASE);
#else
	addr = alloc_bootmem_align(size, 0x1000);
	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for fb\n",
		size, addr, __pa(addr));
#endif

}

static void p3_touch_init_hw(void)
{
	printk("[TSP] %s, %d\n",__func__,__LINE__);
}
static void p3_touch_exit_hw(void)
{
	printk("[TSP] %s, %d\n",__func__,__LINE__);
}

static void p3_touch_suspend_hw(void)
{
	printk("[TSP] %s, %d\n",__func__,__LINE__);
}

static void p3_touch_resume_hw(void)
{
	printk("[TSP] %s, %d\n",__func__,__LINE__);
}

static void p3_register_touch_callbacks(struct mxt_callbacks *cb)
{
	printk("[TSP] %s, %d\n",__func__,__LINE__);
}

#if defined(CONFIG_TOUCHSCREEN_MELFAS)
static void register_tsp_callbacks(struct tsp_callbacks *cb)
{
	charger_callbacks = cb;
}

static void melfas_touch_power_enable(int en)
{
	pr_info("%s %s\n", __func__, (en)?"on":"off");
	if (en) {
		gpio_direction_output(62, 1);
	}
	else {
		gpio_direction_output(62, 0);
	}
}

static struct melfas_tsi_platform_data melfas_touch_platform_data = {
	.power_enable = melfas_touch_power_enable,
	.register_cb = register_tsp_callbacks,
};
#endif

static const struct i2c_board_info sec_i2c_touch_info[] = {
	{
#if defined(CONFIG_USA_OPERATOR_ATT) || defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU) || defined(CONFIG_EUR_OPERATOR_OPEN)
		I2C_BOARD_INFO("sec_touch", 0x48),
#endif
		.irq           =  MSM_GPIO_TO_INT(TOUCHSCREEN_IRQ),			
//#if defined(CONFIG_TOUCHSCREEN_MELFAS)
		.platform_data =&melfas_touch_platform_data,
//#else
//		.platform_data = &p3_touch_platform_data,
//#endif
	},
};

static struct i2c_gpio_platform_data amp_i2c_gpio_data = {
	.sda_pin   = GPIO_AMP_I2C_SDA,
	.scl_pin    = GPIO_AMP_I2C_SCL,
	.udelay    = 5,
};

static struct platform_device amp_i2c_gpio_device = {  
	.name       = "i2c-gpio",
	.id     = MSM_AMP_I2C_BUS_ID,
	.dev        = {
		.platform_data  = &amp_i2c_gpio_data,
	},
};

#ifdef CONFIG_OPTICAL_GP2A
static int __init opt_device_init(void)
{
	gpio_tlmm_config(GPIO_CFG(SENSOR_ALS_SCL,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(SENSOR_ALS_SDA,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1);
	return 0;
}

static struct i2c_gpio_platform_data opt_i2c_gpio_data = {
	.sda_pin    = SENSOR_ALS_SDA,
	.scl_pin    = SENSOR_ALS_SCL,
	.udelay		= 5,
};

static struct platform_device opt_i2c_gpio_device = {  
	.name       = "i2c-gpio",
	.id     = MSM_OPT_I2C_BUS_ID,
	.dev        = {
		.platform_data  = &opt_i2c_gpio_data,
	},
};
static struct i2c_board_info opt_i2c_borad_info[] = {
	{
		I2C_BOARD_INFO("gp2a", 0x88>>1),
      	//.platform_data  = &opt_i2c_gpio_data,
	},
};

static struct platform_device opt_gp2a = {
	.name = "gp2a-opt",
	.id = -1,
};
#endif

#ifdef CONFIG_OPTICAL_BH1721_I957
static int __init opt_device_init(void)
{
	static struct regulator *pm8058_l2;
	int ret = 0;
	
	gpio_tlmm_config(GPIO_CFG(SENSOR_ALS_SCL,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(SENSOR_ALS_SDA,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1);

	pm8058_l2 = regulator_get(NULL, "8058_l2");
	if (IS_ERR(pm8058_l2)) {
		return PTR_ERR(pm8058_l2);
	}

	ret = regulator_set_voltage(pm8058_l2, 3300000, 3300000);
	if (ret) {
		pr_err("%s: unable to set the voltage for regulator"
				"pm8058_l2\n", __func__);
		regulator_put(pm8058_l2);
		return ret;
	}

	ret = regulator_enable(pm8058_l2);
	if (ret) {
		pr_err("%s: unable to enable regulator"
				"pm8058_l2\n", __func__);
		regulator_put(pm8058_l2);
	}

	return 0;
}

static struct i2c_gpio_platform_data opt_i2c_gpio_data = {
	.sda_pin    = SENSOR_ALS_SDA,
	.scl_pin    = SENSOR_ALS_SCL,
	.udelay		= 5,
};

static struct platform_device opt_i2c_gpio_device = {  
	.name       = "i2c-gpio",
	.id     = MSM_OPT_I2C_BUS_ID,
	.dev        = {
		.platform_data  = &opt_i2c_gpio_data,
	},
};

#define ALC_RST	PM8058_GPIO(14)

static void ambient_light_sensor_reset(void)
{
	printk("[ALC] %s, %d - start\n", __func__,__LINE__);

	gpio_request(PM8058_GPIO_PM_TO_SYS(ALC_RST), "alc_rst");
	gpio_direction_output(PM8058_GPIO_PM_TO_SYS(ALC_RST), 0);

	udelay(10);

	gpio_direction_output(PM8058_GPIO_PM_TO_SYS(ALC_RST), 1);
	printk("[ALC] %s, %d - end\n", __func__,__LINE__);

}

static void ambient_light_sensor_resetpin_down(void)
{
	printk("[ALC] %s, %d\n", __func__,__LINE__);

	gpio_request(PM8058_GPIO_PM_TO_SYS(ALC_RST), "alc_rst");
	gpio_direction_output(PM8058_GPIO_PM_TO_SYS(ALC_RST), 0);
}

static struct bh1721_platform_data bh1721_opt_data = {
	.reset = ambient_light_sensor_reset,
	.resetpin_down = ambient_light_sensor_resetpin_down,
};

static struct i2c_board_info opt_i2c_borad_info[] = {
	{
		I2C_BOARD_INFO("bh1721", 0x23),
      	.platform_data  = &bh1721_opt_data,
	},
};
#endif

#ifdef CONFIG_BATTERY_MAX17042
#define GPIO_FG_I2C_SCL	101
#define GPIO_FG_I2C_SDA	107
#define GPIO_FG_ALRAM		61

static unsigned fg_17042_gpio_on[] = {
	GPIO_CFG(GPIO_FG_I2C_SCL,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_FG_I2C_SDA,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),	
	GPIO_CFG(GPIO_FG_ALRAM,  0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
};

void max17042_hw_init(void)
{
	int pin, rc;

	for (pin= 0; pin < ARRAY_SIZE(fg_17042_gpio_on); pin++) {
		rc = gpio_tlmm_config(fg_17042_gpio_on[pin], GPIO_CFG_ENABLE);
		if (rc) {
			pr_err("%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, fg_17042_gpio_on[pin], rc);
		}
	}
}

static struct max17042_platform_data max17042_pdata = {
#if defined(CONFIG_USA_OPERATOR_ATT) || defined(CONFIG_EUR_OPERATOR_OPEN) 
	.sdi_capacity = 0x3138, //6300*2
	.sdi_vfcapacity = 0x41A0, //8400*2
#elif defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_KT) || defined(CONFIG_KOR_OPERATOR_LGU)
	.sdi_capacity = 0x2FA8, //6100*2
	.sdi_vfcapacity = 0x3F8A, //8133*2
#else
	.sdi_capacity = 0x2EE0,
	.sdi_vfcapacity = 0x3E80,
#endif
	.atl_capacity = 0x3022,
	.atl_vfcapacity = 0x4024,
	.fuel_alert_line = GPIO_FG_ALRAM,
	.hw_init = max17042_hw_init,
};

static struct i2c_board_info fg_i2c_devices[] = {
	{
		I2C_BOARD_INFO("max17042", (0x36)),
		.platform_data = &max17042_pdata,
		.irq		= MSM_GPIO_TO_INT(61),
	},
};

static struct i2c_gpio_platform_data fg_i2c_gpio_data = {
	.sda_pin		= GPIO_FG_I2C_SDA,
	.scl_pin		= GPIO_FG_I2C_SCL,
	.udelay 		= 2,
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

static struct platform_device fg_i2c_gpio_device = {
	.name			= "i2c-gpio",
	.id			= MSM_FG_I2C_BUS_ID,
	.dev.platform_data	= &fg_i2c_gpio_data,
};
#endif

#ifdef CONFIG_GYRO_K3G_I957
#define SENSOR_GYRO_SCL 39
#define SENSOR_GYRO_SDA 38
#define GYRO_FIFO_INT 102

struct regulator *k3g_l11;
struct regulator *k3g_l20;
struct regulator *k3g_lvs3;
static int k3g_on(void);
static int k3g_off(void);
static struct k3g_platform_data k3g_data;

static int __init gyro_device_init(void)
{
	int err = 0;	

	k3g_l11 = regulator_get(NULL, "8058_l11");
	if (IS_ERR(k3g_l11)) {
		return PTR_ERR(k3g_l11);
	}

	err = regulator_set_voltage(k3g_l11, 2850000, 2850000);
	if (err) {
		pr_err("%s: unable to set the voltage for regulator"
				"pm8058_l11\n", __func__);
		regulator_put(k3g_l11);
	}
#if defined(CONFIG_KOR_OPERATOR_LGU)
	k3g_lvs3 = regulator_get(NULL, "8901_lvs3");
	if (IS_ERR(k3g_lvs3)) {
		return PTR_ERR(k3g_lvs3);
	}
#elif defined(CONFIG_KOR_OPERATOR_SKT)
	if(system_rev < 0x07){ 
		/* rev 0.0, 0.1, 0.2,  VSENSOR_1.8V from VREG_L20A */
		k3g_l20 = regulator_get(NULL, "8058_l20");
		if (IS_ERR(k3g_l20)) {
			return PTR_ERR(k3g_l20);
		}

		err = regulator_set_voltage(k3g_l20, 1800000, 1800000);
		if (err) {
			pr_err("%s: unable to set the voltage for regulator"
					"pm8058_l20\n", __func__);
			regulator_put(k3g_l20);
			return err;
		}
	}
#else
	if(system_rev < 0x06){ 
		/* rev < 0x06, 
		   VSENSOR_1.8V from VREG_L20A, VSENSOR_2.85V from VREG_L11A */
		k3g_l20 = regulator_get(NULL, "8058_l20");
		if (IS_ERR(k3g_l20)) {
			return PTR_ERR(k3g_l20);
		}

		err = regulator_set_voltage(k3g_l20, 1800000, 1800000);
		if (err) {
			pr_err("%s: unable to set the voltage for regulator"
					"pm8058_l20\n", __func__);
			regulator_put(k3g_l20);
			return err;
		}
	}
	else if(system_rev >= 0x06){
		/* rev >= 0x06, 
		   VSENSOR_1.8V from VIO_P3_1.8V, VSENSOR_2.85V from VREG_L11A */
		printk("[K3G] power_on mapped NULL\n");
		k3g_data.power_on = NULL; 
		//k3g_data.power_off = NULL; 

		err = regulator_enable(k3g_l11);
		if (err) {
			pr_err("%s: unable to enable regulator"
					"pm8058_l11\n", __func__);
			regulator_put(k3g_l11);
			return err;
		}
	}
	else
		printk("[K3G] unknown rev=%%d\n", system_rev );
#endif	
	return 0;
}

static int k3g_on(void)
{
	int err = 0;	

	err = regulator_enable(k3g_l11);
	if (err) {
		pr_err("%s: unable to enable regulator"
				"pm8058_l11\n", __func__);
		regulator_put(k3g_l11);
		goto err_k3g_on;
	}

#if defined(CONFIG_KOR_OPERATOR_LGU)
	err = regulator_enable(k3g_lvs3);
	if (err) {
		pr_err("%s: unable to enable regulator"
				"pm8901_lvs3\n", __func__);
		regulator_put(k3g_lvs3);
		goto err_k3g_on;
	}
#elif defined(CONFIG_KOR_OPERATOR_SKT)	
	if(system_rev < 0x07){ 
		err = regulator_enable(k3g_l20);
		if (err) {
			pr_err("%s: unable to enable regulator"
					"pm8058_l20\n", __func__);
			regulator_put(k3g_l20);
			goto err_k3g_on;
		}
	}
#else
	err = regulator_enable(k3g_l20);
	if (err) {
		pr_err("%s: unable to enable regulator"
				"pm8058_l20\n", __func__);
		regulator_put(k3g_l20);
		goto err_k3g_on;
	}
#endif	
	return err;

err_k3g_on:
	printk("[K3G] %s, error=%d\n", __func__, err );	
	return err;

}

static int k3g_off(void)
{
	int err = 0;	

#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU)
	printk("%s Sensor VDD, I2C LDO off\n",__func__);
#else 
	if(system_rev >= 0x06){
		if(gpio_tlmm_config(GPIO_CFG(SENSOR_GYRO_SCL,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1))
			printk("%s error in making SENSOR_GYRO_SCL Input with PD\n",__func__);
		if(gpio_tlmm_config(GPIO_CFG(SENSOR_GYRO_SDA,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1))
			printk("%s error in making SENSOR_GYRO_SDA Input with PD\n",__func__);
		if(gpio_tlmm_config(GPIO_CFG(GYRO_FIFO_INT,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1))
			printk("%s error in making GYRO_FIFO_INT Input with PD\n",__func__);

		return err;
	}
#endif

	err = regulator_disable(k3g_l11);
	if (err) {
		pr_err("%s: unable to enable regulator"
				"pm8058_l11\n", __func__);
		regulator_put(k3g_l11);
		goto err_k3g_off;
	}
	
#if defined(CONFIG_KOR_OPERATOR_LGU)
	err = regulator_disable(k3g_lvs3);
	if (err) {
		pr_err("%s: unable to disable regulator"
				"pm8901_lvs3\n", __func__);
		regulator_put(k3g_lvs3);
		goto err_k3g_off;
	}
#elif defined(CONFIG_KOR_OPERATOR_SKT)	
	if(system_rev < 0x07){ 
		err = regulator_disable(k3g_l20);
		if (err) {
			pr_err("%s: unable to enable regulator"
					"pm8058_l20\n", __func__);
			regulator_put(k3g_l20);
			goto err_k3g_off;
		}
	}
#else
	err = regulator_disable(k3g_l20);
	if (err) {
		pr_err("%s: unable to enable regulator"
				"pm8058_l20\n", __func__);
		regulator_put(k3g_l20);
		goto err_k3g_off;
	}
#endif	

#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU)
	if(system_rev >= 0x07){
		if(gpio_tlmm_config(GPIO_CFG(SENSOR_GYRO_SCL,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1))
			printk("%s error in making SENSOR_GYRO_SCL Input with PD\n",__func__);
		if(gpio_tlmm_config(GPIO_CFG(SENSOR_GYRO_SDA,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1))
			printk("%s error in making SENSOR_GYRO_SDA Input with PD\n",__func__);
		if(gpio_tlmm_config(GPIO_CFG(GYRO_FIFO_INT,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1))
			printk("%s error in making GYRO_FIFO_INT Input with PD\n",__func__);
	} else {
		if(gpio_tlmm_config(GPIO_CFG(SENSOR_GYRO_SCL,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1))
			printk("%s error in making SENSOR_GYRO_SCL Input with PD\n",__func__);
		if(gpio_tlmm_config(GPIO_CFG(SENSOR_GYRO_SDA,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1))
			printk("%s error in making SENSOR_GYRO_SDA Input with PD\n",__func__);
	}
#else	
	if(gpio_tlmm_config(GPIO_CFG(SENSOR_GYRO_SCL,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1))
		printk("%s error in making SENSOR_GYRO_SCL Input with PD\n",__func__);
	if(gpio_tlmm_config(GPIO_CFG(SENSOR_GYRO_SDA,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1))
		printk("%s error in making SENSOR_GYRO_SDA Input with PD\n",__func__);
#endif
	return err;

err_k3g_off:
	printk("[K3G] %s, error=%d\n", __func__, err );	
	return err;

}

int get_gyro_irq()
{
	if (system_rev < 0x04)
		return -1;
	else
		return GYRO_FIFO_INT;
}

static struct i2c_gpio_platform_data gyro_i2c_gpio_data = {
	.sda_pin    = SENSOR_GYRO_SDA,
	.scl_pin    = SENSOR_GYRO_SCL,
	.udelay		= 5,
};

static struct platform_device gyro_i2c_gpio_device = {  
	.name       = "i2c-gpio",
	.id     = MSM_GYRO_I2C_BUS_ID,
	.dev        = {
		.platform_data  = &gyro_i2c_gpio_data,
	},
};

static struct k3g_platform_data k3g_data = {
	.power_on = k3g_on,
	.power_off = k3g_off,
	.get_irq=get_gyro_irq,
};

static struct i2c_board_info gyro_i2c_borad_info[] = {
	{
		I2C_BOARD_INFO("k3g", 0x69),
      	.platform_data  = &k3g_data,
		.irq = -1,
	},
};

static struct platform_device gyro_k3g = {
	.name = "k3g",
	.id = -1,
};
#endif

#ifdef CONFIG_CMC623_P5LTE
#define CMC623_SDA 156
#define CMC623_SCL 157

static struct i2c_gpio_platform_data cmc623_i2c_gpio_data = {
	.sda_pin    = CMC623_SDA,
	.scl_pin    = CMC623_SCL,
	.udelay		= 5,
};

static struct platform_device cmc623_i2c_gpio_device = {  
	.name       = "i2c-gpio",
	.id     = MSM_CMC623_I2C_BUS_ID,
	.dev        = {
		.platform_data  = &cmc623_i2c_gpio_data,
	},
};

static struct i2c_board_info cmc623_i2c_borad_info[] = {
	{
		I2C_BOARD_INFO("cmc623", 0x38),
	},
};

static struct platform_device cmc623_p5lte = {
	.name = "cmc623",
	.id = -1,
};

#endif

#ifdef CONFIG_WM8994_AMP


static struct i2c_board_info wm8994_i2c_borad_info[] = {
	{
		I2C_BOARD_INFO("wm8994-amp", 0x36 >> 1),
	},
};

/*
static struct platform_device wm8994_p5lte = {
	.name = "wm8994-amp",
	.id = -1,
};
*/
#endif
#ifdef CONFIG_30PIN_CONN
static void acc_int_init(void)
{
	gpio_tlmm_config(GPIO_CFG(GPIO_ACCESSORY_INT,
		GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
}
int64_t acc_get_adc_value(void)
{
	int64_t value =0;

	if (check_using_stmpe811()) {
#ifdef CONFIG_STMPE811_ADC
		value = (int64_t)stmpe811_adc_get_value(7);
		return value;
#endif
	} else {
	value = PM8058_get_adc(CHANNEL_ADC_ACC_CHECK);
	return value;
	}
}
static int get_dock_state(void)
{
	return(gpio_get_value(GPIO_ACCESSORY_INT)) ;
}

static int get_accessory_state(void)
{
	return(gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_DOCK_INT))) ;
}

struct acc_con_platform_data acc_con_pdata = {
	.otg_en = qc_otg_en,
	.acc_power = qc_acc_power,
	.usb_ldo_en = qc_usb_ldo_en,
	.get_acc_state = get_accessory_state,
	.get_dock_state=get_dock_state,
#ifdef CONFIG_SEC_KEYBOARD_DOCK
	.check_keyboard = check_sec_keyboard_dock,
#endif
	.accessory_irq_gpio = GPIO_ACCESSORY_INT,
	.dock_irq =PM8058_GPIO_IRQ(PM8058_IRQ_BASE, (PMIC_GPIO_DOCK_INT)),
	//.mhl_irq_gpio = 0,	//GPIO_MHL_INT,
};

struct platform_device sec_device_connector = {
	.name = "acc_con",
	.id = -1,
	.dev.platform_data = &acc_con_pdata,
};
#endif

#ifdef CONFIG_VIDEO_MHL_TABLET_V1
static struct regulator *l25; 		//VSIL_1.2A & VSIL_1.2C Connected to PM8058
static struct regulator *l2;		//VCC_3.3V_MHL Connected to PM8901
static struct regulator *mvs0;	//VCC_1.8V_MHL Connected to PM8901
int pre_cfg_pw_state = 0; 

static int  sii9234_pre_cfg_power(void)
{
	int rc;
	l25 = regulator_get(NULL, "8058_l25");
	if (IS_ERR(l25)) {
		rc = PTR_ERR(l25);
		pr_err("%s: l25 get failed (%d)\n", __func__, rc);
		return rc;
	}
	rc = regulator_set_voltage(l25, 1200000, 1200000);
	if (rc) {
		pr_err("%s: l25 set level failed (%d)\n", __func__, rc);
		return rc;
	}


	l2 = regulator_get(NULL, "8901_l2");
	if (IS_ERR(l2)) {
		rc = PTR_ERR(l2);
		pr_err("%s: l2 get failed (%d)\n", __func__, rc);
		return rc;
	}
	rc = regulator_set_voltage(l2, 3300000, 3300000);
	if (rc) {
		pr_err("%s: l2 set level failed (%d)\n", __func__, rc);
		return rc;
	}

	mvs0 = regulator_get(NULL, "8901_mvs0");
	if (IS_ERR(mvs0)) {
		rc = PTR_ERR(mvs0);
		pr_err("%s: mvs0 get failed (%d)\n", __func__, rc);
		return rc;
	}
	rc = regulator_set_voltage(mvs0, 1800000, 1800000);
	if (rc) {
		pr_err("%s: mvs0 set level failed (%d)\n", __func__, rc);
		return rc;
	}

	return 0;
}

void sii9234_cfg_power(bool onoff)
{
	int rc;
	static bool sii_power_state = 0;
	if (sii_power_state == onoff) // Avoid unbalanced voltage regulator onoff
	{
		printk("sii_power_state is already %s ", sii_power_state?"on":"off");
		return;
	}
	sii_power_state = onoff;

	if(onoff){
		rc = regulator_enable(l25);		//VSIL_1.2A & VSIL_1.2C 	
		if (rc) {
			pr_err("%s: l25 vreg enable failed (%d)\n", __func__, rc);
			return;
		}

		rc = regulator_enable(l2);		//VCC_3.3V_MHL
		if (rc) {
			pr_err("%s: l2 vreg enable failed (%d)\n", __func__, rc);
			return;
		}

		rc = regulator_enable(mvs0);		//VCC_1.8V_MHL
		if (rc) {
			pr_err("%s: l2 vreg enable failed (%d)\n", __func__, rc);
			return;
		}
		printk("sii9234_cfg_power on\n");
	} else {
		rc = regulator_disable(l25);		//VSIL_1.2A & VSIL_1.2C 	
		if (rc) {
			pr_err("%s: l25 vreg enable failed (%d)\n", __func__, rc);
			return;
		}

		rc = regulator_disable(l2);		//VCC_3.3V_MHL
		if (rc) {
			pr_err("%s: l2 vreg enable failed (%d)\n", __func__, rc);
			return;
		}

		rc = regulator_disable(mvs0);		//VCC_1.8V_MHL
		if (rc) {
			pr_err("%s: l2 vreg enable failed (%d)\n", __func__, rc);
			return;
		}
		printk("sii9234_cfg_power off\n");
	}
	return;
}

static int mhl_sii9234_device_init(void)
{
	if(!pre_cfg_pw_state){
		sii9234_pre_cfg_power();
		pre_cfg_pw_state = 1;
	}
	return 0;
}

void sii9234_hw_reset(void)
{
	sii9234_cfg_power(1);		//Turn On power to SiI9234 
	gpio_request(GPIO_MHL_RST, "mhl_rst");
	gpio_tlmm_config(GPIO_CFG(GPIO_MHL_RST, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	
	usleep_range(10000, 20000);
	if(gpio_direction_output(GPIO_MHL_RST, 1))
		printk("%s error in making GPIO_MHL_RST HIGH\n",__func__);
	
	usleep_range(5000, 20000);
	if(gpio_direction_output(GPIO_MHL_RST, 0))
		printk("%s error in making GPIO_MHL_RST Low\n",__func__);

	usleep_range(10000, 20000);
	if(gpio_direction_output(GPIO_MHL_RST, 1))
		printk("%s error in making GPIO_MHL_RST HIGH\n",__func__);
	msleep(30);
	gpio_free(GPIO_MHL_RST);
}

static void sii9234_hw_off(void)
{	
	sii9234_cfg_power(0);		//Turn Off power to SiI9234 	
	
	usleep_range(10000, 20000);
	
	gpio_request(GPIO_MHL_RST, "mhl_rst");
	if(gpio_direction_output(GPIO_MHL_RST, 0))
		printk("%s error in making GPIO_MHL_RST Low\n",__func__);
	gpio_free(GPIO_MHL_RST);
}

/*MHL IF*/
static struct i2c_gpio_platform_data mhl_i2c_gpio_data = {
	.sda_pin   = GPIO_MHL_SDA,
	.scl_pin    = GPIO_MHL_SCL,
	.udelay    = 3,
};

static struct platform_device mhl_i2c_gpio_device = {  
	.name       = "i2c-gpio",
	.id     = MSM_GSBI8_QUP_I2C_BUS_ID,
	.dev        = {
		.platform_data  = &mhl_i2c_gpio_data,
	},
};

struct sii9234_platform_data p5lte_sii9234_pdata = {
	.hw_reset = sii9234_hw_reset,
	.hw_off = sii9234_hw_off,
	.hw_device_init = mhl_sii9234_device_init,
};

static struct i2c_board_info mhl_i2c_board_info[] = {
	{
		I2C_BOARD_INFO("SII9234", 0x72>>1),
		.platform_data = &p5lte_sii9234_pdata,
	},
	{
		I2C_BOARD_INFO("SII9234A", 0x7A>>1),
	},
	{
		I2C_BOARD_INFO("SII9234B", 0x92>>1),
	},
	{
		I2C_BOARD_INFO("SII9234C", 0xC8>>1),
	},
};
#endif /*CONFIG_VIDEO_MHL_TABLET_V1*/

#ifdef CONFIG_SENSORS_AK8975_I957
#define GPIO_MSENSE_INT PM8058_GPIO(33)		
#define SENSOR_ACCEL_INT	PM8058_GPIO(10)		/* PMIC GPIO Number 32 */
#define SENSOR_AKM_SDA 51
#define SENSOR_AKM_SCL 52

struct regulator *k3dh_akm_l11;
struct regulator *k3dh_akm_l20;
struct regulator *k3dh_akm_lvs3;

static int k3dh_akm_on(void)
{
	int err = 0;	

	err = regulator_enable(k3dh_akm_l11);
	if (err) {
		pr_err("%s: unable to enable regulator"
				"pm8058_l11\n", __func__);
		regulator_put(k3dh_akm_l11);
		goto err_k3dh_akm_on;
	}
	
#if defined(CONFIG_KOR_OPERATOR_LGU)
	err = regulator_enable(k3dh_akm_lvs3);
	if (err) {
		pr_err("%s: unable to enable regulator"
				"pm8901_lvs3\n", __func__);
		regulator_put(k3dh_akm_lvs3);
		goto err_k3dh_akm_on;
	}
#elif defined(CONFIG_KOR_OPERATOR_SKT)
	if(system_rev < 0x07){
		err = regulator_enable(k3dh_akm_l20);
		if (err) {
			pr_err("%s: unable to enable regulator"
					"pm8058_l20\n", __func__);
			regulator_put(k3dh_akm_l20);
			goto err_k3dh_akm_on;
		}
	}
#else
	err = regulator_enable(k3dh_akm_l20);
	if (err) {
		pr_err("%s: unable to enable regulator"
				"pm8058_l20\n", __func__);
		regulator_put(k3dh_akm_l20);
		goto err_k3dh_akm_on;
	}
#endif	
	return err;

err_k3dh_akm_on:
	printk("[K3DH] %s, error=%d\n", __func__, err );	
	return err;

}

static int k3dh_akm_off(void)
{
	int err = 0;	

#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU)
		printk("%s Sensor VDD, I2C LDO off\n",__func__);
#else 
	if(system_rev >= 0x06){
		if(gpio_tlmm_config(GPIO_CFG(SENSOR_AKM_SDA,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1))
			printk("%s error in making SENSOR_AKM_SDA Low\n",__func__);
		if(gpio_tlmm_config(GPIO_CFG(SENSOR_AKM_SCL,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1))
			printk("%s error in making SENSOR_AKM_SCL Low\n",__func__);

		return err;
	}
#endif

	err = regulator_disable(k3dh_akm_l11);
	if (err) {
		pr_err("%s: unable to enable regulator"
				"pm8058_l11\n", __func__);
		regulator_put(k3dh_akm_l11);
		goto err_k3dh_akm_off;
	}

#if defined(CONFIG_KOR_OPERATOR_LGU)
	err = regulator_disable(k3dh_akm_lvs3);
	if (err) {
		pr_err("%s: unable to disable regulator"
				"pm8901_lvs3\n", __func__);
		regulator_put(k3dh_akm_lvs3);
		goto err_k3dh_akm_off;
	}
#elif defined(CONFIG_KOR_OPERATOR_SKT)
	if(system_rev < 0x07){
		err = regulator_disable(k3dh_akm_l20);
		if (err) {
			pr_err("%s: unable to enable regulator"
					"pm8058_l20\n", __func__);
			regulator_put(k3dh_akm_l20);
			goto err_k3dh_akm_off;
		}		
	}
#else
	err = regulator_disable(k3dh_akm_l20);
	if (err) {
		pr_err("%s: unable to enable regulator"
				"pm8058_l20\n", __func__);
		regulator_put(k3dh_akm_l20);
		goto err_k3dh_akm_off;
	}
#endif	

#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU)
	if(system_rev >= 0x07){
		if(gpio_tlmm_config(GPIO_CFG(SENSOR_AKM_SDA,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1))
			printk("%s error in making SENSOR_AKM_SDA Low\n",__func__);
		if(gpio_tlmm_config(GPIO_CFG(SENSOR_AKM_SCL,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), 1))
			printk("%s error in making SENSOR_AKM_SCL Low\n",__func__);
	} else {	
		if(gpio_tlmm_config(GPIO_CFG(SENSOR_AKM_SDA,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1))
			printk("%s error in making SENSOR_AKM_SDA Low\n",__func__);
		if(gpio_tlmm_config(GPIO_CFG(SENSOR_AKM_SCL,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1))
			printk("%s error in making SENSOR_AKM_SCL Low\n",__func__);
	}
#else	
	if(gpio_tlmm_config(GPIO_CFG(SENSOR_AKM_SDA,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1))
		printk("%s error in making SENSOR_AKM_SDA Low\n",__func__);
	if(gpio_tlmm_config(GPIO_CFG(SENSOR_AKM_SCL,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1))
		printk("%s error in making SENSOR_AKM_SCL Low\n",__func__);

#endif

	return err;

err_k3dh_akm_off:
	printk("[K3DH] %s, error=%d\n", __func__, err );	
	return err;
}

static struct i2c_gpio_platform_data akm_i2c_gpio_data = {
	.sda_pin    = SENSOR_AKM_SDA,
	.scl_pin    = SENSOR_AKM_SCL,
	.udelay		= 5,
};

static struct platform_device akm_i2c_gpio_device = {  
	.name       = "i2c-gpio",
	.id     = MSM_MAG_I2C_BUS_ID,
	.dev        = {
		.platform_data  = &akm_i2c_gpio_data,
	},
};

static struct akm8975_platform_data akm8975_pdata = {
	.gpio_data_ready_int = PM8058_GPIO_PM_TO_SYS(GPIO_MSENSE_INT),
};

static struct k3dh_platform_data k3dh_data = {
	.gpio_acc_int = PM8058_GPIO_PM_TO_SYS(SENSOR_ACCEL_INT),
	.power_on = k3dh_akm_on,
	.power_off = k3dh_akm_off,
};

static struct i2c_board_info akm_i2c_borad_info[] = {
	{
		I2C_BOARD_INFO("ak8975", 0x0C),
		.platform_data = &akm8975_pdata,
	},
	{
		I2C_BOARD_INFO("k3dh", 0x19),
      	.platform_data  = &k3dh_data,
//      	.irq = -1,
	},
};
#endif

#ifdef CONFIG_SENSORS_YDA165
static struct snd_set_ampgain init_ampgain[] = {
	[0] = {
		.in1_gain = 2,
		.in2_gain = 2,
		.hp_att = 31,
		.hp_gainup = 0,
		.sp_att = 31,
		.sp_gainup = 0,
	},
	[1] = {
		.in1_gain = 2,
		.in2_gain = 2,
		.hp_att = 15,
		.hp_gainup = 0,
		.sp_att = 15,
		.sp_gainup = 0,
	},
};
static struct yda165_i2c_data yda165_data = {
	.ampgain = init_ampgain,
	.num_modes = ARRAY_SIZE(init_ampgain),
};

static struct i2c_board_info yamahaamp_boardinfo[] = {
	{
		I2C_BOARD_INFO("yda165", 0xD8 >> 1),
		.platform_data = &yda165_data,
	},
};
#endif
#ifdef CONFIG_SENSORS_YDA160
static struct snd_set_ampgain init_ampgain[] = {
	[0] = {
		.in1_gain = 2,
		.in2_gain = 2,
		.hp_att = 31,
		.hp_gainup = 0,
		.sp_att = 31,
		.sp_gainup = 0,
	},
	[1] = {
		.in1_gain = 2,
		.in2_gain = 2,
		.hp_att = 15,
		.hp_gainup = 0,
		.sp_att = 15,
		.sp_gainup = 0,
	},
};
static struct yda160_i2c_data yda160_data = {
	.ampgain = init_ampgain,
	.num_modes = ARRAY_SIZE(init_ampgain),
};

static struct i2c_board_info yamahaamp_boardinfo[] = {
	{
		I2C_BOARD_INFO("yda160", 0xD8 >> 1),
		.platform_data = &yda160_data,
	},
};
#endif

#ifdef CONFIG_SENSORS_MAX9879
static struct i2c_board_info maxamp_boardinfo[] = {
	{
		I2C_BOARD_INFO("max9879_i2c", 0x9A >> 1),
	},
};
#endif

static int __init vibrator_device_gpio_init(void)
{
	printk("[HASHTEST]==============================================================\n");
	gpio_request(30, "vib_en");
	gpio_tlmm_config(GPIO_CFG(30, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(31, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(115, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(116, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	return 0;
}

#ifdef CONFIG_SERIAL_MSM_HS
static int configure_uart_gpios(int on)
{
	int ret = 0, i;
	int uart_gpios[] = {53, 54, 55, 56};
	for (i = 0; i < ARRAY_SIZE(uart_gpios); i++) {
		if (on) {
			ret = msm_gpiomux_get(uart_gpios[i]);
			if (unlikely(ret))
				break;
		} else {
			ret = msm_gpiomux_put(uart_gpios[i]);
			if (unlikely(ret))
				return ret;
		}
	}
	if (ret)
		for (; i >= 0; i--)
			msm_gpiomux_put(uart_gpios[i]);
	return ret;
}
static struct msm_serial_hs_platform_data msm_uart_dm1_pdata = {
       .inject_rx_on_wakeup = 1,
       .rx_to_inject = 0xFD,
       .gpio_config = configure_uart_gpios,
};
#endif

#if defined(CONFIG_MSM_RPM_LOG) || defined(CONFIG_MSM_RPM_LOG_MODULE)

static struct msm_rpm_log_platform_data msm_rpm_log_pdata = {
	.phys_addr_base = 0x00106000,
	.reg_offsets = {
		[MSM_RPM_LOG_PAGE_INDICES] = 0x00000C80,
		[MSM_RPM_LOG_PAGE_BUFFER]  = 0x00000CA0,
	},
	.phys_size = SZ_8K,
	.log_len = 4096,		  /* log's buffer length in bytes */
	.log_len_mask = (4096 >> 2) - 1,  /* length mask in units of u32 */
};

static struct platform_device msm_rpm_log_device = {
	.name	= "msm_rpm_log",
	.id	= -1,
	.dev	= {
		.platform_data = &msm_rpm_log_pdata,
	},
};
#endif

/*
 * Consumer specific regulator names:
 *			 regulator name		consumer dev_name
 */
static struct regulator_consumer_supply vreg_consumers_PM8058_L0[] = {
	REGULATOR_SUPPLY("8058_l0",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L1[] = {
	REGULATOR_SUPPLY("8058_l1",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L2[] = {
	REGULATOR_SUPPLY("8058_l2",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L3[] = {
	REGULATOR_SUPPLY("8058_l3",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L4[] = {
	REGULATOR_SUPPLY("8058_l4",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L5[] = {
	REGULATOR_SUPPLY("8058_l5",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L6[] = {
	REGULATOR_SUPPLY("8058_l6",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L7[] = {
	REGULATOR_SUPPLY("8058_l7",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L8[] = {
	REGULATOR_SUPPLY("8058_l8",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L9[] = {
	REGULATOR_SUPPLY("8058_l9",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L10[] = {
	REGULATOR_SUPPLY("8058_l10",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L11[] = {
	REGULATOR_SUPPLY("8058_l11",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L12[] = {
	REGULATOR_SUPPLY("8058_l12",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L13[] = {
	REGULATOR_SUPPLY("8058_l13",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L14[] = {
	REGULATOR_SUPPLY("8058_l14",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L15[] = {
	REGULATOR_SUPPLY("8058_l15",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L16[] = {
	REGULATOR_SUPPLY("8058_l16",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L17[] = {
	REGULATOR_SUPPLY("8058_l17",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L18[] = {
	REGULATOR_SUPPLY("8058_l18",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L19[] = {
	REGULATOR_SUPPLY("8058_l19",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L20[] = {
	REGULATOR_SUPPLY("8058_l20",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L21[] = {
	REGULATOR_SUPPLY("8058_l21",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L22[] = {
	REGULATOR_SUPPLY("8058_l22",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L23[] = {
	REGULATOR_SUPPLY("8058_l23",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L24[] = {
	REGULATOR_SUPPLY("8058_l24",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L25[] = {
	REGULATOR_SUPPLY("8058_l25",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_S0[] = {
	REGULATOR_SUPPLY("8058_s0",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_S1[] = {
	REGULATOR_SUPPLY("8058_s1",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_S2[] = {
	REGULATOR_SUPPLY("8058_s2",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_S3[] = {
	REGULATOR_SUPPLY("8058_s3",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_S4[] = {
	REGULATOR_SUPPLY("8058_s4",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_LVS0[] = {
	REGULATOR_SUPPLY("8058_lvs0",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_LVS1[] = {
	REGULATOR_SUPPLY("8058_lvs1",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_NCP[] = {
	REGULATOR_SUPPLY("8058_ncp",		NULL),
};

static struct regulator_consumer_supply vreg_consumers_PM8901_L0[] = {
	REGULATOR_SUPPLY("8901_l0",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_L1[] = {
	REGULATOR_SUPPLY("8901_l1",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_L2[] = {
	REGULATOR_SUPPLY("8901_l2",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_L3[] = {
	REGULATOR_SUPPLY("8901_l3",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_L4[] = {
	REGULATOR_SUPPLY("8901_l4",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_L5[] = {
	REGULATOR_SUPPLY("8901_l5",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_L6[] = {
	REGULATOR_SUPPLY("8901_l6",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_S2[] = {
	REGULATOR_SUPPLY("8901_s2",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_S3[] = {
	REGULATOR_SUPPLY("8901_s3",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_S4[] = {
	REGULATOR_SUPPLY("8901_s4",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_LVS0[] = {
	REGULATOR_SUPPLY("8901_lvs0",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_LVS1[] = {
	REGULATOR_SUPPLY("8901_lvs1",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_LVS2[] = {
	REGULATOR_SUPPLY("8901_lvs2",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_LVS3[] = {
	REGULATOR_SUPPLY("8901_lvs3",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_MVS0[] = {
	REGULATOR_SUPPLY("8901_mvs0",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_USB_OTG[] = {
	REGULATOR_SUPPLY("8901_usb_otg",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_HDMI_MVS[] = {
	REGULATOR_SUPPLY("8901_hdmi_mvs",		NULL),
};

/* Pin control regulators */
static struct regulator_consumer_supply vreg_consumers_PM8058_L8_PC[] = {
	REGULATOR_SUPPLY("8058_l8_pc",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L20_PC[] = {
	REGULATOR_SUPPLY("8058_l20_pc",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_L21_PC[] = {
	REGULATOR_SUPPLY("8058_l21_pc",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8058_S2_PC[] = {
	REGULATOR_SUPPLY("8058_s2_pc",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_L0_PC[] = {
	REGULATOR_SUPPLY("8901_l0_pc",		NULL),
};
static struct regulator_consumer_supply vreg_consumers_PM8901_S4_PC[] = {
	REGULATOR_SUPPLY("8901_s4_pc",		NULL),
};

#define RPM_VREG_INIT(_id, _min_uV, _max_uV, _modes, _ops, _apply_uV, \
		      _default_uV, _peak_uA, _avg_uA, _pull_down, _pin_ctrl, \
		      _freq, _pin_fn, _force_mode, _state, _sleep_selectable, \
		      _always_on) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_modes_mask = _modes, \
				.valid_ops_mask = _ops, \
				.min_uV = _min_uV, \
				.max_uV = _max_uV, \
				.input_uV = _min_uV, \
				.apply_uV = _apply_uV, \
				.always_on = _always_on, \
			}, \
			.consumer_supplies	= vreg_consumers_##_id, \
			.num_consumer_supplies	= \
				ARRAY_SIZE(vreg_consumers_##_id), \
		}, \
		.id			= RPM_VREG_ID_##_id, \
		.default_uV = _default_uV, \
		.peak_uA = _peak_uA, \
		.avg_uA = _avg_uA, \
		.pull_down_enable = _pull_down, \
		.pin_ctrl = _pin_ctrl, \
		.freq			= RPM_VREG_FREQ_##_freq, \
		.pin_fn = _pin_fn, \
		.force_mode		= _force_mode, \
		.state = _state, \
		.sleep_selectable = _sleep_selectable, \
	}

/* Pin control initialization */
#define RPM_PC(_id, _always_on, _pin_fn, _pin_ctrl) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
				.always_on	= _always_on, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id##_PC), \
			.consumer_supplies	= vreg_consumers_##_id##_PC, \
		}, \
		.id	  = RPM_VREG_ID_##_id##_PC, \
		.pin_fn	  = RPM_VREG_PIN_FN_8660_##_pin_fn, \
		.pin_ctrl = _pin_ctrl, \
	}

/*
 * The default LPM/HPM state of an RPM controlled regulator can be controlled
 * via the peak_uA value specified in the table below.  If the value is less
 * than the high power min threshold for the regulator, then the regulator will
 * be set to LPM.  Otherwise, it will be set to HPM.
 *
 * This value can be further overridden by specifying an initial mode via
 * .init_data.constraints.initial_mode.
 */

#define RPM_LDO(_id, _always_on, _pd, _sleep_selectable, _min_uV, _max_uV, \
		_init_peak_uA) \
	RPM_VREG_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_FAST | \
		      REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE | \
		      REGULATOR_MODE_STANDBY, REGULATOR_CHANGE_VOLTAGE | \
		      REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		      REGULATOR_CHANGE_DRMS, 0, _min_uV, _init_peak_uA, \
		      _init_peak_uA, _pd, RPM_VREG_PIN_CTRL_NONE, NONE, \
		      RPM_VREG_PIN_FN_8660_ENABLE, \
		      RPM_VREG_FORCE_MODE_8660_NONE, RPM_VREG_STATE_OFF, \
		      _sleep_selectable, _always_on)

#define RPM_SMPS(_id, _always_on, _pd, _sleep_selectable, _min_uV, _max_uV, \
		 _init_peak_uA, _freq) \
	RPM_VREG_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_FAST | \
		      REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE | \
		      REGULATOR_MODE_STANDBY, REGULATOR_CHANGE_VOLTAGE | \
		      REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE | \
		      REGULATOR_CHANGE_DRMS, 0, _min_uV, _init_peak_uA, \
		      _init_peak_uA, _pd, RPM_VREG_PIN_CTRL_NONE, _freq, \
		      RPM_VREG_PIN_FN_8660_ENABLE, \
		      RPM_VREG_FORCE_MODE_8660_NONE, RPM_VREG_STATE_OFF, \
		      _sleep_selectable, _always_on)

#define RPM_VS(_id, _always_on, _pd, _sleep_selectable) \
	RPM_VREG_INIT(_id, 0, 0, REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE, \
		      REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE, 0, 0, \
		      1000, 1000, _pd, RPM_VREG_PIN_CTRL_NONE, NONE, \
		      RPM_VREG_PIN_FN_8660_ENABLE, \
		      RPM_VREG_FORCE_MODE_8660_NONE, RPM_VREG_STATE_OFF, \
		      _sleep_selectable, _always_on)

#define RPM_NCP(_id, _always_on, _pd, _sleep_selectable, _min_uV, _max_uV) \
	RPM_VREG_INIT(_id, _min_uV, _max_uV, REGULATOR_MODE_NORMAL, \
		      REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS, 0, \
		      _min_uV, 1000, 1000, _pd, RPM_VREG_PIN_CTRL_NONE, NONE, \
		      RPM_VREG_PIN_FN_8660_ENABLE, \
		      RPM_VREG_FORCE_MODE_8660_NONE, RPM_VREG_STATE_OFF, \
		      _sleep_selectable, _always_on)

#define LDO50HMIN	RPM_VREG_8660_LDO_50_HPM_MIN_LOAD
#define LDO150HMIN	RPM_VREG_8660_LDO_150_HPM_MIN_LOAD
#define LDO300HMIN	RPM_VREG_8660_LDO_300_HPM_MIN_LOAD
#define SMPS_HMIN	RPM_VREG_8660_SMPS_HPM_MIN_LOAD
#define FTS_HMIN	RPM_VREG_8660_FTSMPS_HPM_MIN_LOAD

/* RPM early regulator constraints */
static struct rpm_regulator_init_data rpm_regulator_early_init_data[] = {
	/*	 ID       a_on pd ss min_uV   max_uV   init_ip    freq */
	RPM_SMPS(PM8058_S0, 0, 1, 1,  500000, 1325000, SMPS_HMIN, 1p60),
	RPM_SMPS(PM8058_S1, 0, 1, 1,  500000, 1250000, SMPS_HMIN, 1p60),
};

/* RPM regulator constraints */
static struct rpm_regulator_init_data rpm_regulator_init_data[] = {
	/*	ID        a_on pd ss min_uV   max_uV   init_ip */
	RPM_LDO(PM8058_L0,  0, 1, 0, 1200000, 1200000, LDO150HMIN),
	RPM_LDO(PM8058_L1,  0, 1, 0, 1200000, 1200000, LDO300HMIN),
	RPM_LDO(PM8058_L2,  0, 1, 0, 3300000, 3300000, LDO300HMIN),
	RPM_LDO(PM8058_L3,  0, 1, 0, 1800000, 1800000, LDO150HMIN),
	RPM_LDO(PM8058_L4,  0, 1, 0, 2850000, 2850000,  LDO50HMIN),
	RPM_LDO(PM8058_L5,  0, 1, 0, 2850000, 2850000, LDO300HMIN),
	RPM_LDO(PM8058_L6,  0, 1, 0, 3000000, 3600000,  LDO50HMIN),
	RPM_LDO(PM8058_L7,  0, 1, 0, 1800000, 1800000,  LDO50HMIN),
#if defined(CONFIG_USA_OPERATOR_ATT) || defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU) || defined(CONFIG_EUR_OPERATOR_OPEN)
	RPM_LDO(PM8058_L8,  0, 1, 0, 3300000, 3300000, LDO300HMIN),
#else
	RPM_LDO(PM8058_L8,  0, 1, 0, 2900000, 3050000, LDO300HMIN),
#endif

#if defined(CONFIG_USA_OPERATOR_ATT) || defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU) || defined(CONFIG_EUR_OPERATOR_OPEN)
	RPM_LDO(PM8058_L9,  0, 1, 0, 1500000, 3000000, LDO300HMIN),
#else
	RPM_LDO(PM8058_L9,  0, 1, 0, 1800000, 1800000, LDO300HMIN),
#endif
	RPM_LDO(PM8058_L10, 0, 1, 0, 2600000, 2600000, LDO300HMIN),
#if defined(CONFIG_USA_MODEL_SGH_I957) || defined(CONFIG_KOR_MODEL_SHV_E140S) || defined(CONFIG_KOR_MODEL_SHV_E140K) || defined(CONFIG_KOR_MODEL_SHV_E140L)
	RPM_LDO(PM8058_L11, 0, 1, 0, 2850000, 2850000, LDO150HMIN),
#else
	RPM_LDO(PM8058_L11, 0, 1, 0, 1500000, 1500000, LDO150HMIN),
#endif
	RPM_LDO(PM8058_L12, 0, 1, 0, 2900000, 2900000, LDO150HMIN),
	RPM_LDO(PM8058_L13, 0, 1, 0, 2050000, 2050000, LDO300HMIN),
	RPM_LDO(PM8058_L14, 0, 0, 0, 2850000, 2850000, LDO300HMIN),
	RPM_LDO(PM8058_L15, 0, 1, 0, 2850000, 2850000, LDO300HMIN),
	RPM_LDO(PM8058_L16, 1, 1, 0, 1800000, 1800000, LDO300HMIN),
#if defined(CONFIG_USA_OPERATOR_ATT) || defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_EUR_OPERATOR_OPEN)
	RPM_LDO(PM8058_L17, 0, 1, 0, 1800000, 2850000, LDO150HMIN),
#else
	RPM_LDO(PM8058_L17, 0, 1, 0, 2600000, 2600000, LDO150HMIN),
#endif
	RPM_LDO(PM8058_L18, 0, 1, 0, 2200000, 2200000, LDO150HMIN),
#if defined(CONFIG_EUR_OPERATOR_OPEN)
	RPM_LDO(PM8058_L19, 0, 1, 0, 3300000, 3300000, LDO150HMIN),
#else
	RPM_LDO(PM8058_L19, 0, 1, 0, 2500000, 2500000, LDO150HMIN),
#endif
#if defined(CONFIG_KOR_OPERATOR_SKT)
	RPM_LDO(PM8058_L20, 0, 1, 0, 1800000, 3000000, LDO150HMIN),
#else	
	RPM_LDO(PM8058_L20, 0, 1, 0, 1800000, 1800000, LDO150HMIN),
#endif	
	RPM_LDO(PM8058_L21, 1, 1, 0, 1200000, 1200000, LDO150HMIN),
	RPM_LDO(PM8058_L22, 0, 1, 0, 1150000, 1150000, LDO300HMIN),
	RPM_LDO(PM8058_L23, 0, 1, 0, 1200000, 1200000, LDO300HMIN),
	RPM_LDO(PM8058_L24, 0, 1, 0, 1200000, 1200000, LDO150HMIN),
	RPM_LDO(PM8058_L25, 0, 1, 0, 1200000, 1200000, LDO150HMIN),

	/*	 ID       a_on pd ss min_uV   max_uV   init_ip    freq */
	RPM_SMPS(PM8058_S2, 0, 1, 1, 1200000, 1400000, SMPS_HMIN, 1p60),
	RPM_SMPS(PM8058_S3, 1, 1, 0, 1800000, 1800000, SMPS_HMIN, 1p60),
	RPM_SMPS(PM8058_S4, 1, 1, 0, 2200000, 2200000, SMPS_HMIN, 1p60),

	/*     ID         a_on pd ss */
	RPM_VS(PM8058_LVS0, 0, 1, 0),
	RPM_VS(PM8058_LVS1, 0, 1, 0),

	/*	ID        a_on pd ss min_uV   max_uV */
	RPM_NCP(PM8058_NCP, 0, 1, 0, 1800000, 1800000),

	/*	ID        a_on pd ss min_uV   max_uV   init_ip */
	RPM_LDO(PM8901_L0,  0, 1, 0, 1200000, 1200000, LDO300HMIN),
	RPM_LDO(PM8901_L1,  0, 1, 0, 3300000, 3300000, LDO300HMIN),
	RPM_LDO(PM8901_L2,  0, 1, 0, 2850000, 3300000, LDO300HMIN),
	RPM_LDO(PM8901_L3,  0, 1, 0, 3300000, 3300000, LDO300HMIN),
	RPM_LDO(PM8901_L4,  0, 1, 0, 2600000, 2600000, LDO300HMIN),
#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_KT)
	RPM_LDO(PM8901_L5,  0, 1, 0, 2850000, 2950000, LDO300HMIN),
#else
	RPM_LDO(PM8901_L5,  0, 1, 0, 2850000, 2850000, LDO300HMIN),
#endif
	RPM_LDO(PM8901_L6,  0, 1, 0, 2200000, 2200000, LDO300HMIN),

	/*	 ID       a_on pd ss min_uV   max_uV   init_ip   freq */
	RPM_SMPS(PM8901_S2, 0, 1, 0, 1200000, 1300000, FTS_HMIN, 1p60),
	RPM_SMPS(PM8901_S3, 0, 1, 0, 1100000, 1100000, FTS_HMIN, 1p60),
	RPM_SMPS(PM8901_S4, 0, 1, 0, 1225000, 1225000, FTS_HMIN, 1p60),

	/*	ID        a_on pd ss */
	RPM_VS(PM8901_LVS0, 1, 1, 0),
	RPM_VS(PM8901_LVS1, 0, 1, 0),
	RPM_VS(PM8901_LVS2, 0, 1, 0),
	RPM_VS(PM8901_LVS3, 0, 1, 0),
	RPM_VS(PM8901_MVS0, 0, 1, 0),

	/*     ID         a_on pin_func pin_ctrl */
	RPM_PC(PM8058_L8,   0, SLEEP_B, RPM_VREG_PIN_CTRL_NONE),
	RPM_PC(PM8058_L20,  0, SLEEP_B, RPM_VREG_PIN_CTRL_NONE),
	RPM_PC(PM8058_L21,  1, SLEEP_B, RPM_VREG_PIN_CTRL_NONE),
	RPM_PC(PM8058_S2,   0, ENABLE,  RPM_VREG_PIN_CTRL_PM8058_A0),
	RPM_PC(PM8901_L0,   0, ENABLE,  RPM_VREG_PIN_CTRL_PM8901_A0),
	RPM_PC(PM8901_S4,   0, ENABLE,  RPM_VREG_PIN_CTRL_PM8901_A0),
};

static struct rpm_regulator_platform_data rpm_regulator_early_pdata = {
	.init_data		= rpm_regulator_early_init_data,
	.num_regulators		= ARRAY_SIZE(rpm_regulator_early_init_data),
	.version		= RPM_VREG_VERSION_8660,
	.vreg_id_vdd_mem	= RPM_VREG_ID_PM8058_S0,
	.vreg_id_vdd_dig	= RPM_VREG_ID_PM8058_S1,
};

static struct rpm_regulator_platform_data rpm_regulator_pdata = {
	.init_data		= rpm_regulator_init_data,
	.num_regulators		= ARRAY_SIZE(rpm_regulator_init_data),
	.version		= RPM_VREG_VERSION_8660,
};

static struct platform_device rpm_regulator_early_device = {
	.name	= "rpm-regulator",
	.id	= 0,
	.dev	= {
		.platform_data = &rpm_regulator_early_pdata,
	},
};

static struct platform_device rpm_regulator_device = {
	.name	= "rpm-regulator",
	.id	= 1,
	.dev	= {
		.platform_data = &rpm_regulator_pdata,
	},
};

static struct platform_device *early_regulators[] __initdata = {
	&msm_device_saw_s0,
	&msm_device_saw_s1,
	&rpm_regulator_early_device,
};

static struct platform_device *early_devices[] __initdata = {
#ifdef CONFIG_MSM_BUS_SCALING
	&msm_bus_apps_fabric,
	&msm_bus_sys_fabric,
	&msm_bus_mm_fabric,
	&msm_bus_sys_fpb,
	&msm_bus_cpss_fpb,
#endif
	&msm_device_dmov_adm0,
	&msm_device_dmov_adm1,
};

#if (defined(CONFIG_MARIMBA_CORE)) && \
	(defined(CONFIG_MSM_BT_POWER) || defined(CONFIG_MSM_BT_POWER_MODULE))
static struct resource bluesleep_resources[] = {
	{    
		.name = "gpio_host_wake",    
		.start = GPIO_BT_HOST_WAKE,    
		.end = GPIO_BT_HOST_WAKE,    
		.flags = IORESOURCE_IO,    
	},    
	{    
		.name = "gpio_ext_wake",    
		.start = GPIO_BT_WAKE,//81,//35,    
		.end = GPIO_BT_WAKE,//81, //35,    
		.flags = IORESOURCE_IO,    
	},    
	{    
		.name = "host_wake",    
		.start = MSM_GPIO_TO_INT(GPIO_BT_HOST_WAKE),    
		.end = MSM_GPIO_TO_INT(GPIO_BT_HOST_WAKE),    
		.flags = IORESOURCE_IRQ,    
	},
};

static struct platform_device msm_bt_power_device = {
	.name = "bt_power",
};

static struct platform_device msm_bluesleep_device = {    
	.name = "bluesleep",    
	.id = -1,    
	.num_resources = ARRAY_SIZE(bluesleep_resources),    
	.resource = bluesleep_resources,
};

#endif

static struct platform_device msm_tsens_device = {
	.name   = "tsens-tm",
	.id = -1,
};

#ifdef CONFIG_SENSORS_MSM_ADC

static struct adc_access_fn xoadc_fn = {
	pm8058_xoadc_select_chan_and_start_conv,
	pm8058_xoadc_read_adc_code,
	pm8058_xoadc_get_properties,
	pm8058_xoadc_slot_request,
	pm8058_xoadc_restore_slot,
	pm8058_xoadc_calibrate,
};

static struct msm_adc_channels msm_adc_channels_data[] = {
	{"vbatt", CHANNEL_ADC_VBATT, 0, &xoadc_fn, CHAN_PATH_TYPE2,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE3, scale_default},
	{"vcoin", CHANNEL_ADC_VCOIN, 0, &xoadc_fn, CHAN_PATH_TYPE1,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_default},
	{"vcharger_channel", CHANNEL_ADC_VCHG, 0, &xoadc_fn, CHAN_PATH_TYPE3,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE4, scale_default},
	{"charger_current_monitor", CHANNEL_ADC_CHG_MONITOR, 0, &xoadc_fn,
		CHAN_PATH_TYPE4,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE1, scale_default},
	{"vph_pwr", CHANNEL_ADC_VPH_PWR, 0, &xoadc_fn, CHAN_PATH_TYPE5,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE3, scale_default},
	{"usb_vbus", CHANNEL_ADC_USB_VBUS, 0, &xoadc_fn, CHAN_PATH_TYPE11,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE3, scale_default},
	{"pmic_therm", CHANNEL_ADC_DIE_TEMP, 0, &xoadc_fn, CHAN_PATH_TYPE12,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE1, scale_pmic_therm},
	{"pmic_therm_4K", CHANNEL_ADC_DIE_TEMP_4K, 0, &xoadc_fn,
		CHAN_PATH_TYPE12,
		ADC_CONFIG_TYPE1, ADC_CALIB_CONFIG_TYPE7, scale_pmic_therm},
	{"xo_therm", CHANNEL_ADC_XOTHERM, 0, &xoadc_fn, CHAN_PATH_TYPE_NONE,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE5, tdkntcgtherm},
	{"xo_therm_4K", CHANNEL_ADC_XOTHERM_4K, 0, &xoadc_fn,
		CHAN_PATH_TYPE_NONE,
		ADC_CONFIG_TYPE1, ADC_CALIB_CONFIG_TYPE6, tdkntcgtherm},
	{"hdset_detect", CHANNEL_ADC_HDSET, 0, &xoadc_fn, CHAN_PATH_TYPE6,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE1, scale_default},
	{"chg_batt_amon", CHANNEL_ADC_BATT_AMON, 0, &xoadc_fn, CHAN_PATH_TYPE10,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE1,
		scale_xtern_chgr_cur},
	{"msm_therm", CHANNEL_ADC_MSM_THERM, 0, &xoadc_fn, CHAN_PATH_TYPE8,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_msm_therm},
	{"batt_therm", CHANNEL_ADC_BATT_THERM, 0, &xoadc_fn, CHAN_PATH_TYPE7,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_batt_therm},
	{"batt_id", CHANNEL_ADC_BATT_ID, 0, &xoadc_fn, CHAN_PATH_TYPE9,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_default},
	{"ref_625mv", CHANNEL_ADC_625_REF, 0, &xoadc_fn, CHAN_PATH_TYPE15,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_default},
	{"ref_1250mv", CHANNEL_ADC_1250_REF, 0, &xoadc_fn, CHAN_PATH_TYPE13,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_default},
	{"ref_325mv", CHANNEL_ADC_325_REF, 0, &xoadc_fn, CHAN_PATH_TYPE14,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_default},
#ifdef CONFIG_BATTERY_P5LTE
	{"cable_check", CHANNEL_ADC_CABLE_CHECK, 0, &xoadc_fn, CHAN_PATH_TYPE7,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_default},
#endif
	{"acc_detect", CHANNEL_ADC_ACC_CHECK, 0, &xoadc_fn, CHAN_PATH_TYPE10,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_default},



};

static char *msm_adc_fluid_device_names[] = {
	"ADS_ADC1",
	"ADS_ADC2",
};

static struct msm_adc_platform_data msm_adc_pdata = {
	.channel = msm_adc_channels_data,
	.num_chan_supported = ARRAY_SIZE(msm_adc_channels_data),
};

static struct platform_device msm_adc_device = {
	.name   = "msm_adc",
	.id = -1,
	.dev = {
		.platform_data = &msm_adc_pdata,
	},
};

static void pmic8058_xoadc_mpp_config(void)
{
	int rc, i;

	struct pm8xxx_mpp_init_info xoadc_mpps[] = {
		PM8901_MPP_INIT(XOADC_MPP_4, D_OUTPUT, PM8901_MPP_DIG_LEVEL_S4,
							DOUT_CTRL_LOW),
		PM8058_MPP_INIT(XOADC_MPP_2, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH9,
							AOUT_CTRL_DISABLE),
		PM8058_MPP_INIT(XOADC_MPP_3, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH5,
							AOUT_CTRL_DISABLE),
		PM8058_MPP_INIT(XOADC_MPP_8, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH8,
							AOUT_CTRL_DISABLE),
		PM8058_MPP_INIT(XOADC_MPP_10, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH7,
							AOUT_CTRL_DISABLE),
	};

	for (i = 0; i < ARRAY_SIZE(xoadc_mpps); i++) {
		rc = pm8xxx_mpp_config(xoadc_mpps[i].mpp,
					&xoadc_mpps[i].config);
		if (rc) {
			pr_err("%s: Config MPP %d of PM8058 failed\n",
					__func__, xoadc_mpps[i].mpp);
		}
	}

#if CONFIG_BATTERY_P5LTE
	struct pm8xxx_mpp_config_data pm8058_xoadc_mpp4 = {
		.type	= PM8XXX_MPP_TYPE_A_INPUT,
		.level	= PM8XXX_MPP_AIN_AMUX_CH6,
		.control	= PM8XXX_MPP_AOUT_CTRL_DISABLE,
	};

	if  (!check_using_stmpe811()) {
			rc = pm8xxx_mpp_config(PM8058_MPP_PM_TO_SYS(XOADC_MPP_4), &pm8058_xoadc_mpp4);
			if (rc)
				pr_err("%s: Config mpp4 on pmic 8058 failed\n", __func__);
	}
#endif
}

static struct regulator *vreg_ldo18_adc;

static int pmic8058_xoadc_vreg_config(int on)
{
	int rc;

	if (on) {
		rc = regulator_enable(vreg_ldo18_adc);
		if (rc)
			pr_err("%s: Enable of regulator ldo18_adc "
						"failed\n", __func__);
	} else {
		rc = regulator_disable(vreg_ldo18_adc);
		if (rc)
			pr_err("%s: Disable of regulator ldo18_adc "
						"failed\n", __func__);
	}

	return rc;
}

static int pmic8058_xoadc_vreg_setup(void)
{
	int rc;

	vreg_ldo18_adc = regulator_get(NULL, "8058_l18");
	if (IS_ERR(vreg_ldo18_adc)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
			__func__, PTR_ERR(vreg_ldo18_adc));
		rc = PTR_ERR(vreg_ldo18_adc);
		goto fail;
	}

	rc = regulator_set_voltage(vreg_ldo18_adc, 2200000, 2200000);
	if (rc) {
		pr_err("%s: unable to set ldo18 voltage to 2.2V\n", __func__);
		goto fail;
	}

	return rc;
fail:
	regulator_put(vreg_ldo18_adc);
	return rc;
}

static void pmic8058_xoadc_vreg_shutdown(void)
{
	regulator_put(vreg_ldo18_adc);
}

/* usec. For this ADC,
 * this time represents clk rate @ txco w/ 1024 decimation ratio.
 * Each channel has different configuration, thus at the time of starting
 * the conversion, xoadc will return actual conversion time
 * */
static struct adc_properties pm8058_xoadc_data = {
	.adc_reference          = 2200, /* milli-voltage for this adc */
	.bitresolution         = 15,
	.bipolar                = 0,
	.conversiontime         = 54,
};

static struct xoadc_platform_data pm8058_xoadc_pdata = {
	.xoadc_prop = &pm8058_xoadc_data,
	.xoadc_mpp_config = pmic8058_xoadc_mpp_config,
	.xoadc_vreg_set = pmic8058_xoadc_vreg_config,
	.xoadc_num = XOADC_PMIC_0,
	.xoadc_vreg_setup = pmic8058_xoadc_vreg_setup,
	.xoadc_vreg_shutdown = pmic8058_xoadc_vreg_shutdown,
};
#endif

#ifdef CONFIG_SAMSUNG_JACK

void *adc_handle;
static struct regulator *vreg_earmic_bias;
bool earmic_bias = 0;

static struct sec_jack_zone jack_zones[] = {
	[0] = {
		.adc_high	= 1230, // spec  : 1161
		.delay_ms	= 10,
		.check_count	= 5,
		.jack_type	= SEC_HEADSET_3POLE,
	},
	[1] = {
		.adc_high	= 4000, // spec :  1307
		.delay_ms	= 10,
		.check_count	= 5,
		.jack_type	= SEC_HEADSET_4POLE,
	},
};

#if 0 // TEMP
static struct sec_button_zone button_zones[] = {
		[0] = {
			.adc_high	= 178, // spec : 168
			.adc_low	= 0,	
			.key 		= KEY_MEDIA,
		},
		[1] = {
			.adc_high	= 415, // spec : 391
			.adc_low	= 179, // spec : 189
			.key		= KEY_VOLUMEUP,
		},	
		[2] = {
			.adc_high	= 1000, // spec :  832
			.adc_low	= 416, // spec : 438
			.key		= KEY_VOLUMEDOWN,
		},		
};
#endif

static int get_p5lte_det_jack_state(void)
{
	return(gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_DET))) ^ 1;
}

static int get_p5lte_send_key_state(void)
{
	return(gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SHORT_SENDEND))) ^ 1;
}

static void set_p5lte_micbias_state(bool state)
{
#if defined(CONFIG_USA_OPERATOR_ATT) || defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU) || defined(CONFIG_EUR_OPERATOR_OPEN)
	if(system_rev >= 0x4)
	{
		pr_info("sec_jack: ear micbias %s\n", state?"on":"off");
		gpio_set_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_MICBIAS_EN), state);	
	}
        else
        {
		int rc;
			
		if(state && !earmic_bias)
		{
			rc = regulator_enable(vreg_earmic_bias);
			if (rc) {
				pr_err("%s: Enable regulator 8901_L4 failed\n", __func__);
			}
			earmic_bias = true;
		}
		else if(!state && earmic_bias)
		{
			rc = regulator_disable(vreg_earmic_bias);
			if (rc) {
				pr_err("%s: Disable regulator 8901_L4 failed\n", __func__);
			}
			earmic_bias = false;
		}
        }
#else
	int rc;
	
	if(state && !earmic_bias)
	{
		rc = regulator_enable(vreg_earmic_bias);
		if (rc) {
			pr_err("%s: Enable regulator 8901_L4 failed\n", __func__);
		}
		earmic_bias = true;
	}
	else if(!state && earmic_bias)
	{
		rc = regulator_disable(vreg_earmic_bias);
		if (rc) {
			pr_err("%s: Disable regulator 8901_L4 failed\n", __func__);
		}
		earmic_bias = false;
	}
#endif

	return;
}

static int sec_jack_get_adc_value(void)
{
	//DECLARE_COMPLETION_ONSTACK(sec_adc_wait);
	struct completion sec_adc_wait;
	int rc;
	u32 res = 0;
	struct adc_chan_result adc_result;
#if defined(CONFIG_KOR_OPERATOR_LGU)
	if(system_rev < 0x6) 
#else
	if(system_rev < 0x7) 
#endif
	{
		init_completion(&sec_adc_wait);	
		rc = adc_channel_request_conv(adc_handle, &sec_adc_wait);
		if (rc) {
			pr_err("%s: adc_channel_request_conv failed\n", __func__);
			return 0;
		}
		rc = wait_for_completion_interruptible(&sec_adc_wait);
		if (rc) {
			pr_err("%s: wait_for_completion_interruptible failed\n", __func__);
			return 0;
		}
		rc = adc_channel_read_result(adc_handle, &adc_result);
		if (rc) {
			pr_err("%s: adc_channel_read_result failed\n", __func__);
			return 0;
		}

		/* rc = adc_channel_close(adc_handle);
		 * if (rc) {
			 * pr_err("%s: couldnt close channel %d ret=%d\n",
							 * __func__, CHANNEL_ADC_HDSET, rc);
		 * } */
		res = adc_result.physical;
		
		mdelay(10);
	} else {
#ifdef CONFIG_STMPE811_ADC
		res = stmpe811_adc_get_value(6);
		printk("stmpe811 sec_jack adc read : %d\n",res);
#endif
	}
		
	return res;
}


static void sec_jack_gpio_init(void)
{
#if defined(CONFIG_USA_OPERATOR_ATT) || defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU) || defined(CONFIG_EUR_OPERATOR_OPEN)
	int rc;
	if(system_rev <= 0x3)
	{	

		vreg_earmic_bias = regulator_get(NULL, "8901_l4");
		if (IS_ERR(vreg_earmic_bias)) {
			pr_err("%s: Unable to get 8901_l4\n", __func__);
			regulator_put(vreg_earmic_bias);
			return ;
		}

		rc = regulator_set_voltage(vreg_earmic_bias, 2850000, 2850000);
		if (rc) {
			pr_err("%s: unable to set L4 voltage to 2.8V\n", __func__);
		}
	}

	rc = adc_channel_open(CHANNEL_ADC_HDSET,&adc_handle); 
	if (rc) {
		pr_err("%s: Unable to open ADC channel\n", __func__);
	}
	else{
		pr_info("%s: open ADC channel\n", __func__); 
	}
#else
	int rc;
	
	vreg_earmic_bias = regulator_get(NULL, "8901_l4");
	if (IS_ERR(vreg_earmic_bias)) {
		pr_err("%s: Unable to get 8901_l4\n", __func__);
		regulator_put(vreg_earmic_bias);
		return ;
	}

	rc = regulator_set_voltage(vreg_earmic_bias, 2850000, 2850000);
	if (rc) {
		pr_err("%s: unable to set L4 voltage to 2.8V\n", __func__);
	}

	rc = adc_channel_open(CHANNEL_ADC_HDSET,&adc_handle); 
	if (rc) {
		pr_err("%s: Unable to open ADC channel\n", __func__);
	}
	else {
		pr_info("%s: open ADC channel\n", __func__); 
	}
	
#endif
	return;
}

static struct sec_jack_platform_data sec_jack_data = {
	.get_det_jack_state	= get_p5lte_det_jack_state,
	.get_send_key_state	= get_p5lte_send_key_state,
	.set_micbias_state	= set_p5lte_micbias_state,
// TEMP	.init = sec_jack_gpio_init,
	.get_adc_value	= sec_jack_get_adc_value,
	.zones		= jack_zones,
// TEMP	.button_zone = button_zones,
	.num_zones	= ARRAY_SIZE(jack_zones),
// TEMP	.num_button_zones = ARRAY_SIZE(button_zones),
	.det_int	= PM8058_GPIO_IRQ(PM8058_IRQ_BASE, (PMIC_GPIO_EAR_DET)),
	.send_int	= PM8058_GPIO_IRQ(PM8058_IRQ_BASE, (PMIC_GPIO_SHORT_SENDEND)),
};

static struct platform_device sec_device_jack = {
	.name           = "sec_jack",
	.id             = -1,
	.dev            = {
		.platform_data  = &sec_jack_data,
	},
};
#endif

#ifdef CONFIG_MSM_SDIO_AL

static unsigned mdm2ap_status = 140;

static int configure_mdm2ap_status(int on)
{
	int ret = 0;
	if (on)
		ret = msm_gpiomux_get(mdm2ap_status);
	else
		ret = msm_gpiomux_put(mdm2ap_status);

	if (ret)
		pr_err("%s: mdm2ap_status config failed, on = %d\n", __func__,
		       on);

	return ret;
}


static int get_mdm2ap_status(void)
{
	return gpio_get_value(mdm2ap_status);
}

static struct sdio_al_platform_data sdio_al_pdata = {
	.config_mdm2ap_status = configure_mdm2ap_status,
	.get_mdm2ap_status = get_mdm2ap_status,
	.allow_sdioc_version_major_2 = 0,
	.peer_sdioc_version_minor = 0x0202,
	.peer_sdioc_version_major = 0x0004,
	.peer_sdioc_boot_version_minor = 0x0001,
	.peer_sdioc_boot_version_major = 0x0003
};

struct platform_device msm_device_sdio_al = {
	.name = "msm_sdio_al",
	.id = -1,
	.dev		= {
		.parent = &msm_charm_modem.dev,
		.platform_data	= &sdio_al_pdata,
	},
};

#endif /* CONFIG_MSM_SDIO_AL */

#define GPIO_VREG_ID_EXT_5V		0

static struct regulator_consumer_supply vreg_consumers_EXT_5V[] = {
	REGULATOR_SUPPLY("ext_5v",	NULL),
};

#define GPIO_VREG_INIT(_id, _reg_name, _gpio_label, _gpio, _active_low) \
	[GPIO_VREG_ID_##_id] = { \
		.init_data = { \
			.constraints = { \
				.valid_ops_mask	= REGULATOR_CHANGE_STATUS, \
			}, \
			.num_consumer_supplies	= \
					ARRAY_SIZE(vreg_consumers_##_id), \
			.consumer_supplies	= vreg_consumers_##_id, \
		}, \
		.regulator_name	= _reg_name, \
		.active_low	= _active_low, \
		.gpio_label	= _gpio_label, \
		.gpio		= _gpio, \
	}

/* GPIO regulator constraints */
static struct gpio_regulator_platform_data msm_gpio_regulator_pdata[] = {
	GPIO_VREG_INIT(EXT_5V, "ext_5v", "ext_5v_en",
					PM8901_MPP_PM_TO_SYS(0), 0),
};

/* GPIO regulator */
static struct platform_device msm8x60_8901_mpp_vreg __devinitdata = {
	.name	= GPIO_REGULATOR_DEV_NAME,
	.id	= PM8901_MPP_PM_TO_SYS(0),
	.dev	= {
		.platform_data =
			&msm_gpio_regulator_pdata[GPIO_VREG_ID_EXT_5V],
	},
};

static void __init pm8901_vreg_mpp0_init(void)
{
	int rc;

	struct pm8xxx_mpp_init_info pm8901_vreg_mpp0 = {
		.mpp	= PM8901_MPP_PM_TO_SYS(0),
		.config =  {
			.type	= PM8XXX_MPP_TYPE_D_OUTPUT,
			.level	= PM8901_MPP_DIG_LEVEL_VPH,
		},
	};

	/*
	 * Set PMIC 8901 MPP0 active_high to 0 for surf and charm_surf. This
	 * implies that the regulator connected to MPP0 is enabled when
	 * MPP0 is low.
	 */
	if (machine_is_msm8x60_surf() || machine_is_msm8x60_fusion()) {
		msm_gpio_regulator_pdata[GPIO_VREG_ID_EXT_5V].active_low = 1;
		pm8901_vreg_mpp0.config.control = PM8XXX_MPP_DOUT_CTRL_HIGH;
	} else {
		msm_gpio_regulator_pdata[GPIO_VREG_ID_EXT_5V].active_low = 0;
		pm8901_vreg_mpp0.config.control = PM8XXX_MPP_DOUT_CTRL_LOW;
	}

	rc = pm8xxx_mpp_config(pm8901_vreg_mpp0.mpp, &pm8901_vreg_mpp0.config);
	if (rc)
		pr_err("%s: pm8xxx_mpp_config: rc=%d\n", __func__, rc);
}

static struct platform_device *charm_devices[] __initdata = {
	&msm_charm_modem,
#ifdef CONFIG_MSM_SDIO_AL
	&msm_device_sdio_al,
#endif
};

#ifdef CONFIG_SND_SOC_MSM8660_APQ
static struct platform_device *dragon_alsa_devices[] __initdata = {
	&msm_pcm,
	&msm_pcm_routing,
	&msm_cpudai0,
	&msm_cpudai1,
	&msm_cpudai_hdmi_rx,
	&msm_cpudai_bt_rx,
	&msm_cpudai_bt_tx,
	&msm_cpudai_fm_rx,
	&msm_cpudai_fm_tx,
	&msm_cpu_fe,
	&msm_stub_codec,
	&msm_lpa_pcm,
};
#endif

static struct platform_device *asoc_devices[] __initdata = {
	&asoc_msm_pcm,
	&asoc_msm_dai0,
	&asoc_msm_dai1,
};

static struct platform_device *surf_devices[] __initdata = {
	&msm_device_smd,
	&msm_device_uart_dm12,
	&msm_pil_q6v3,
	&msm_pil_modem,
	&msm_pil_tzapps,
#ifdef CONFIG_I2C_QUP
	&msm_gsbi3_qup_i2c_device,
	&msm_gsbi4_qup_i2c_device,
	&msm_gsbi7_qup_i2c_device,
	&msm_gsbi8_qup_i2c_device,
#ifndef CONFIG_KOR_OPERATOR_LGU
	&msm_gsbi12_qup_i2c_device,
#endif
#endif
#ifdef CONFIG_SERIAL_MSM_HS
	&msm_device_uart_dm1,
#endif
#ifdef CONFIG_MSM_SSBI
	&msm_device_ssbi_pmic1,
	&msm_device_ssbi_pmic2,
#endif
#ifdef CONFIG_I2C_SSBI
	&msm_device_ssbi3,
#endif
#ifdef CONFIG_STMPE811_ADC
	&stmpe811_i2c_gpio_device,
#endif
#ifdef CONFIG_BATTERY_MAX17042
	&fg_i2c_gpio_device,
#endif
#ifdef CONFIG_BATTERY_P5LTE
	&p5_battery_device,
#endif

#if defined (CONFIG_MSM_8x60_VOIP)
	&asoc_msm_mvs,
	&asoc_mvs_dai0,
	&asoc_mvs_dai1,
#endif
#if defined(CONFIG_USB_GADGET_MSM_72K) || defined(CONFIG_USB_EHCI_HCD)
	&msm_device_otg,
#endif
#ifdef CONFIG_USB_GADGET_MSM_72K
	&msm_device_gadget_peripheral,
#endif
#ifdef CONFIG_USB_G_ANDROID
	&android_usb_device,
#endif
#ifdef CONFIG_ANDROID_PMEM
#ifndef CONFIG_MSM_MULTIMEDIA_USE_ION
	&android_pmem_device,
	&android_pmem_adsp_device,
	&android_pmem_smipool_device,
#endif
	&android_pmem_audio_device,
#endif
#ifdef CONFIG_MSM_ROTATOR
	&msm_rotator_device,
#endif
	&msm_fb_device,
	&msm_kgsl_3d0,
	&msm_kgsl_2d0,
	&msm_kgsl_2d1,
#ifdef CONFIG_FB_MSM_LCDC_P5LTE_WXGA_PANEL
	&lcdc_p5lte_panel_device,
#else	
	&lcdc_samsung_panel_device,
#endif
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
	&hdmi_msm_device,
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */
#if 0 // TEMP def CONFIG_MSM_CAMERA
#ifdef CONFIG_MT9E013
	&msm_camera_sensor_mt9e013,
#endif
#ifdef CONFIG_MT9E013
	&msm_camera_sensor_mt9e013,
#endif
#ifdef CONFIG_SENSOR_S5K6AAFX
	&msm_camera_sensor_s5k6aafx,		
#endif
#endif
#ifdef CONFIG_SENSOR_S5K5CCAF
	&msm_camera_sensor_s5k5ccaf,		
#endif
#ifdef CONFIG_SENSOR_S5K5BAFX_P5
	&msm_camera_sensor_s5k5bafx,		
#endif
#if 0 // TEMP def CONFIG_MSM_CAMERA
#ifdef CONFIG_S5K4ECGX
	&msm_camera_sensor_s5k4ecgx,		
#endif
//	&camera_i2c_gpio_device,	
#endif
#ifdef CONFIG_MSM_GEMINI
	&msm_gemini_device,
#endif
#ifdef CONFIG_MSM_VPE
	&msm_vpe_device,
#endif

#if defined(CONFIG_MSM_RPM_LOG) || defined(CONFIG_MSM_RPM_LOG_MODULE)
	&msm_rpm_log_device,
#endif
#if defined(CONFIG_MSM_RPM_STATS_LOG)
	&msm_rpm_stat_device,
#endif
	&msm_device_vidc,
#if (defined(CONFIG_MARIMBA_CORE)) && \
	(defined(CONFIG_MSM_BT_POWER) || defined(CONFIG_MSM_BT_POWER_MODULE))
	&msm_bt_power_device,
	&msm_bluesleep_device, 
#endif
#ifdef CONFIG_SENSORS_MSM_ADC
	&msm_adc_device,
#endif
	&rpm_regulator_device,
	
#if defined(CONFIG_CRYPTO_DEV_QCRYPTO) || \
		defined(CONFIG_CRYPTO_DEV_QCRYPTO_MODULE)
	&qcrypto_device,
#endif

#if defined(CONFIG_CRYPTO_DEV_QCEDEV) || \
		defined(CONFIG_CRYPTO_DEV_QCEDEV_MODULE)
	&qcedev_device,
#endif

#ifdef CONFIG_HW_RANDOM_MSM
	&msm_device_rng,
#endif
	&msm_tsens_device,
	&msm_rpm_device,
#ifdef CONFIG_OPTICAL_GP2A
// TEMP	&opt_i2c_gpio_device,
// TEMP	&opt_gp2a,
#endif
#ifdef CONFIG_OPTICAL_BH1721_I957
	&opt_i2c_gpio_device,
#endif

#if defined(CONFIG_SEC_KEYBOARD_DOCK)
	&sec_keyboard,
#endif

#ifdef CONFIG_BROADCOM_WIFI_RESERVED_MEM
// TEMP	&sec_device_wifi,
#endif
#ifdef CONFIG_SENSORS_AK8975_I957
	&akm_i2c_gpio_device,
#endif
#ifdef CONFIG_30PIN_CONN
	&sec_device_connector,
#endif
#ifdef CONFIG_VIDEO_MHL_TABLET_V1
	&mhl_i2c_gpio_device,
#endif
#ifdef CONFIG_GYRO_K3G_I957
	&gyro_i2c_gpio_device,
#endif
#ifdef CONFIG_CMC623_P5LTE
	&cmc623_i2c_gpio_device,
#endif
#ifdef CONFIG_SAMSUNG_JACK
	&sec_device_jack,
#endif
#ifdef CONFIG_HAPTIC_ISA1200
	&motor_i2c_gpio_device,
#endif
	&sec_device_switch,
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	&ram_console_device,
#endif	
#ifdef CONFIG_ION_MSM
	&ion_dev,
#endif
	&msm8660_device_watchdog,
};

#ifdef CONFIG_ION_MSM
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
static struct ion_cp_heap_pdata cp_mm_ion_pdata = {
	.permission_type = IPT_TYPE_MM_CARVEOUT,
	.align = PAGE_SIZE,
	.request_region = request_smi_region,
	.release_region = release_smi_region,
	.setup_region = setup_smi_region,
};

static struct ion_cp_heap_pdata cp_mfc_ion_pdata = {
	.permission_type = IPT_TYPE_MFC_SHAREDMEM,
	.align = PAGE_SIZE,
	.request_region = request_smi_region,
	.release_region = release_smi_region,
	.setup_region = setup_smi_region,
};

static struct ion_cp_heap_pdata cp_wb_ion_pdata = {
	.permission_type = IPT_TYPE_MDP_WRITEBACK,
	.align = PAGE_SIZE,
};

static struct ion_co_heap_pdata fw_co_ion_pdata = {
	.adjacent_mem_id = ION_CP_MM_HEAP_ID,
	.align = SZ_128K,
};

static struct ion_co_heap_pdata co_ion_pdata = {
	.adjacent_mem_id = INVALID_HEAP_ID,
	.align = PAGE_SIZE,
};
#endif

/**
 * These heaps are listed in the order they will be allocated. Due to
 * video hardware restrictions and content protection the FW heap has to
 * be allocated adjacent (below) the MM heap and the MFC heap has to be
 * allocated after the MM heap to ensure MFC heap is not more than 256MB
 * away from the base address of the FW heap.
 * However, the order of FW heap and MM heap doesn't matter since these
 * two heaps are taken care of by separate code to ensure they are adjacent
 * to each other.
 * Don't swap the order unless you know what you are doing!
 */
static struct ion_platform_data ion_pdata = {
	.nr = MSM_ION_HEAP_NUM,
	.heaps = {
		{
			.id	= ION_SYSTEM_HEAP_ID,
			.type	= ION_HEAP_TYPE_SYSTEM,
			.name	= ION_VMALLOC_HEAP_NAME,
		},
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
		{
			.id	= ION_CP_MM_HEAP_ID,
			.type	= ION_HEAP_TYPE_CP,
			.name	= ION_MM_HEAP_NAME,
			.size	= MSM_ION_MM_SIZE,
			.memory_type = ION_SMI_TYPE,
			.extra_data = (void *) &cp_mm_ion_pdata,
//			.request_region = request_smi_region,
//			.release_region = release_smi_region,
//			.setup_region = setup_smi_region,
		},
		{
			.id	= ION_MM_FIRMWARE_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_MM_FIRMWARE_HEAP_NAME,
			.size	= MSM_ION_MM_FW_SIZE,
			.memory_type = ION_SMI_TYPE,
			.extra_data = (void *) &fw_co_ion_pdata,
		},
		{
			.id	= ION_CP_MFC_HEAP_ID,
			.type	= ION_HEAP_TYPE_CP,
			.name	= ION_MFC_HEAP_NAME,
			.size	= MSM_ION_MFC_SIZE,
			.memory_type = ION_SMI_TYPE,
			.extra_data = (void *) &cp_mfc_ion_pdata,
		},
		{
			.id	= ION_SF_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_SF_HEAP_NAME,
			.size	= MSM_ION_SF_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *)&co_ion_pdata,
		},
		{
			.id	= ION_CAMERA_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_CAMERA_HEAP_NAME,
			.size	= MSM_ION_CAMERA_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = &co_ion_pdata,
		},
		{
			.id	= ION_CP_WB_HEAP_ID,
			.type	= ION_HEAP_TYPE_CP,
			.name	= ION_WB_HEAP_NAME,
#if defined (CONFIG_SAMSUNG_MEMORY_LAYOUT_ARRANGE)
			.base	= MSM_PMEM_MDP_BASE,		// should be physical addr
#endif			
			.size	= MSM_ION_WB_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *) &cp_wb_ion_pdata,
		},
		{
			.id	= ION_AUDIO_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_AUDIO_HEAP_NAME,
#if defined (CONFIG_SAMSUNG_MEMORY_LAYOUT_ARRANGE)
			.base	= MSM_ION_AUDIO_BASE,		// should be physical addr
#endif			
			.size	= MSM_ION_AUDIO_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *)&co_ion_pdata,
		},
#endif
	}
};

static struct platform_device ion_dev = {
	.name = "ion-msm",
	.id = 1,
	.dev = { .platform_data = &ion_pdata },
};
#endif


static struct memtype_reserve msm8x60_reserve_table[] __initdata = {
	/* Kernel SMI memory pool for video core, used for firmware */
	/* and encoder, decoder scratch buffers */
	/* Kernel SMI memory pool should always precede the user space */
	/* SMI memory pool, as the video core will use offset address */
	/* from the Firmware base */
	[MEMTYPE_SMI_KERNEL] = {
		.start	=	KERNEL_SMI_BASE,
		.limit	=	KERNEL_SMI_SIZE,
		.size	=	KERNEL_SMI_SIZE,
		.flags	=	MEMTYPE_FLAGS_FIXED,
	},
	/* User space SMI memory pool for video core */
	/* used for encoder, decoder input & output buffers  */
	[MEMTYPE_SMI] = {
		.start	=	USER_SMI_BASE,
		.limit	=	USER_SMI_SIZE,
		.flags	=	MEMTYPE_FLAGS_FIXED,
	},
#if defined (CONFIG_SAMSUNG_MEMORY_LAYOUT_ARRANGE)
	[MEMTYPE_PMEM_MDP] = {
		.start	=	MSM_PMEM_MDP_BASE,
		.limit	=	MSM_PMEM_MDP_SIZE,
		.flags	=	MEMTYPE_FLAGS_FIXED,
	},
	[MEMTYPE_PMEM_AUDIO] = {
		.start	=	MSM_PMEM_AUDIO_BASE,
		.limit	=	MSM_PMEM_AUDIO_SIZE,
		.flags	=	MEMTYPE_FLAGS_FIXED,
	},
#endif
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static void reserve_ion_memory(void)
{
#if defined(CONFIG_ION_MSM) && defined(CONFIG_MSM_MULTIMEDIA_USE_ION)
	msm8x60_reserve_table[MEMTYPE_EBI1].size += MSM_ION_SF_SIZE;
	msm8x60_reserve_table[MEMTYPE_SMI].size += MSM_ION_MM_FW_SIZE;
	msm8x60_reserve_table[MEMTYPE_SMI].size += MSM_ION_MM_SIZE;
	msm8x60_reserve_table[MEMTYPE_SMI].size += MSM_ION_MFC_SIZE;
	msm8x60_reserve_table[MEMTYPE_EBI1].size += MSM_ION_CAMERA_SIZE;
#if !defined (CONFIG_SAMSUNG_MEMORY_LAYOUT_ARRANGE)
	msm8x60_reserve_table[MEMTYPE_EBI1].size += MSM_ION_WB_SIZE;
	msm8x60_reserve_table[MEMTYPE_EBI1].size += MSM_ION_AUDIO_SIZE;
#endif
#endif
}

static void __init size_pmem_devices(void)
{
#ifdef CONFIG_ANDROID_PMEM
#ifndef CONFIG_MSM_MULTIMEDIA_USE_ION
	android_pmem_adsp_pdata.size = pmem_adsp_size;
	android_pmem_smipool_pdata.size = MSM_PMEM_SMIPOOL_SIZE;
	android_pmem_pdata.size = pmem_sf_size;
#endif
	android_pmem_audio_pdata.size = MSM_PMEM_AUDIO_SIZE;
#endif
}

static void __init reserve_memory_for(struct android_pmem_platform_data *p)
{
	msm8x60_reserve_table[p->memory_type].size += p->size;
}

static void __init reserve_pmem_memory(void)
{
#ifdef CONFIG_ANDROID_PMEM
#ifndef CONFIG_MSM_MULTIMEDIA_USE_ION
	reserve_memory_for(&android_pmem_adsp_pdata);
	reserve_memory_for(&android_pmem_smipool_pdata);
	reserve_memory_for(&android_pmem_pdata);
#endif
	reserve_memory_for(&android_pmem_audio_pdata);
	msm8x60_reserve_table[MEMTYPE_EBI1].size += pmem_kernel_ebi1_size;
#endif
}

static void __init reserve_mdp_memory(void);

static void __init msm8x60_calculate_reserve_sizes(void)
{
	size_pmem_devices();
	reserve_pmem_memory();
	reserve_ion_memory();
	reserve_mdp_memory();
}

static int msm8x60_paddr_to_memtype(unsigned int paddr)
{
#ifdef CONFIG_SAMSUNG_MEMORY_LAYOUT_ARRANGE
	if (paddr >= 0x40000000 && paddr < 0x80000000)
		return MEMTYPE_EBI1;
#else
	if (paddr >= 0x40000000 && paddr < 0x60000000)
		return MEMTYPE_EBI1;
#endif	
	if (paddr >= 0x38000000 && paddr < 0x40000000)
		return MEMTYPE_SMI;
	return MEMTYPE_NONE;
}

static struct reserve_info msm8x60_reserve_info __initdata = {
	.memtype_reserve_table = msm8x60_reserve_table,
	.calculate_reserve_sizes = msm8x60_calculate_reserve_sizes,
	.paddr_to_memtype = msm8x60_paddr_to_memtype,
};

static void __init msm8x60_reserve(void)
{
	reserve_info = &msm8x60_reserve_info;
	msm_reserve();
}

static int check_jig_gpio(void)
{
#if defined(CONFIG_USA_OPERATOR_ATT) || defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU) || defined(CONFIG_EUR_OPERATOR_OPEN)
	if(system_rev >= 0x2)
		return 1;
#else
	if(system_rev >= 0x2)
		return 1;
#endif
	return 0;
}

static int get_jig_gpio(void)
{
	if(system_rev >= 0x4)
		return PM8058_GPIO(6);
	else
		return PM8058_GPIO(2);
}

int check_jig_on(void)
{	
	int ret = 0;
	if (check_jig_gpio()) {
		ret = gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(get_jig_gpio()));
		pr_debug("%s : jig on = %d\r\n", __func__, ret);
	}

#if defined(CONFIG_USA_OPERATOR_ATT) || defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU) || defined(CONFIG_EUR_OPERATOR_OPEN)
	if(system_rev>=0x03)
		return !ret; //jig on=low 
#endif
	return ret; //jig on=high
}

// P5 KOR : sh1.back 11. 11. 21  - add commercial dump fuctionality
#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_KT) || defined(CONFIG_KOR_OPERATOR_LGU)	
EXPORT_SYMBOL(check_jig_on);
#endif

static void JIG_init(void)
{
	int rc = 0;	
	struct pm_gpio jig_onoff = {
		.direction		= PM_GPIO_DIR_IN,
		.pull			= PM_GPIO_PULL_NO,
		.vin_sel		= 2,
		.function		= PM_GPIO_FUNC_NORMAL,
		.out_strength	= PM_GPIO_STRENGTH_NO,			
		.inv_int_pol	= 0,
	};
	
	if(check_jig_gpio()) {
		if (pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(get_jig_gpio()), &jig_onoff))
			pr_err("%s jig_onoff config failed\n", __func__);
		
		rc = gpio_request(PM8058_GPIO_PM_TO_SYS(get_jig_gpio()), "ifconsens");
		if (rc) {
			pr_err("%s:Failed to request GPIO %d\n",
						__func__, get_jig_gpio());
		}
		rc = gpio_direction_input(PM8058_GPIO_PM_TO_SYS(get_jig_gpio()));
		if (rc) {
			pr_err("%s:Failed to direction to input, GPIO %d, rc=%d\n",
						__func__, get_jig_gpio(), rc);
		} else{
			rc = gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(get_jig_gpio()));
			pr_err("%s : IF_CON_SENSE level = %d\r\n", __func__, rc);
		}
	}
}

#ifdef CONFIG_PMIC8058
#define PMIC_GPIO_SDC3_DET 22
#define PM_GPIO_CDC_RST_N 	PM8058_GPIO(21)
#define GPIO_CDC_RST_N 		PM8058_GPIO_PM_TO_SYS(PM_GPIO_CDC_RST_N)


static int pm8058_gpios_init(void)
{
	int i;
	int rc;
	struct pm8058_gpio_cfg {
		int                gpio;
		struct pm_gpio cfg;
	};

	/* Initialize JIG GPIO */
	JIG_init();
	
#ifdef CONFIG_BATTERY_P5LTE
	/* battery gpio have to be inialized firstly to remain charging current from boot */
	extern int charging_mode_by_TA;
	struct pm8058_gpio_cfg batt_cfgs[] = {
		{
			PM8058_GPIO_PM_TO_SYS(TA_DETECT),
			{
				.direction	= PM_GPIO_DIR_IN,
				.pull			= PM_GPIO_PULL_NO,
				.vin_sel		= 2,
				.function		= PM_GPIO_FUNC_NORMAL,
				.inv_int_pol	= 0,
			}
		},
		{
			PM8058_GPIO_PM_TO_SYS(TA_CURRENT_SEL),
			{
				.direction	= PM_GPIO_DIR_OUT,
				.pull			= PM_GPIO_PULL_NO,
				.vin_sel		= 2,
				.function		= PM_GPIO_FUNC_NORMAL,
				.inv_int_pol	= 0,
			}
		},
		{
			PM8058_GPIO_PM_TO_SYS(CHG_EN),
			{
				.direction	= PM_GPIO_DIR_OUT,
				.pull			= PM_GPIO_PULL_NO,
				.vin_sel		= 2,
				.function		= PM_GPIO_FUNC_NORMAL,
				.inv_int_pol	= 0,
			}
		},
		{
			PM8058_GPIO_PM_TO_SYS(CHG_STATE),
			{
				.direction	= PM_GPIO_DIR_IN,
				.pull			= PM_GPIO_PULL_NO,
				.vin_sel		= 2,
				.function		= PM_GPIO_FUNC_NORMAL,
				.inv_int_pol	= 0,
			}
		},
	};
	for (i = 0; i < ARRAY_SIZE(batt_cfgs); ++i) {
		rc = pm8xxx_gpio_config(batt_cfgs[i].gpio,
				&batt_cfgs[i].cfg);
		if (rc < 0) {
			pr_err("%s batt gpio config failed\n",
				__func__);
			return rc;
		}
	}

	gpio_request(PM8058_GPIO_PM_TO_SYS(TA_DETECT), "ta_detect");
	gpio_direction_input(PM8058_GPIO_PM_TO_SYS(TA_DETECT));

	gpio_request(PM8058_GPIO_PM_TO_SYS(CHG_STATE), "chg_state");
	gpio_direction_input(PM8058_GPIO_PM_TO_SYS(CHG_STATE));

	gpio_request(PM8058_GPIO_PM_TO_SYS(CHG_EN), "chg_en");
	gpio_request(PM8058_GPIO_PM_TO_SYS(TA_CURRENT_SEL), "ta_cur_sel");

	printk("charging_mode_by_TA = %d\r\n", charging_mode_by_TA);
	if (charging_mode_by_TA==1)
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(TA_CURRENT_SEL), 1);
	else
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(TA_CURRENT_SEL), 0);

#endif


	struct pm8058_gpio_cfg gpio_cfgs[] = {
#ifdef CONFIG_MMC_MSM_CARD_HW_DETECTION
		{
			PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SDC3_DET - 1),
			{
				.direction      = PM_GPIO_DIR_IN,
				.pull           = PM_GPIO_PULL_UP_30,
				.vin_sel        = 2,
				.function       = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol    = 0,
			},
		},
#endif
		{ /* Timpani Reset */
			PM8058_GPIO_PM_TO_SYS(PM_GPIO_CDC_RST_N),
			{
				.direction	= PM_GPIO_DIR_OUT,
				.output_value	= 1,
				.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
				.pull		= PM_GPIO_PULL_DN,
				.out_strength	= PM_GPIO_STRENGTH_HIGH,
				.function	= PM_GPIO_FUNC_NORMAL,
				.vin_sel	= 2,
				.inv_int_pol	= 0,
			}
		},
#ifdef CONFIG_SENSORS_AK8975_I957
		{
			PM8058_GPIO_PM_TO_SYS(GPIO_MSENSE_INT),
			{
				.direction	= PM_GPIO_DIR_IN,
				.pull			= PM_GPIO_PULL_NO,
				.vin_sel		= 2,
				.function		= PM_GPIO_FUNC_NORMAL,
				.inv_int_pol	= 0,
			}
		},
#endif
#ifdef CONFIG_GYRO_K3G_I957
		{
			PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(11))	/*GYRO_FIFO_INT*/,
			{
				.direction	= PM_GPIO_DIR_IN,
				.pull			= PM_GPIO_PULL_NO,
				.vin_sel		= 2,
				.function		= PM_GPIO_FUNC_NORMAL,
				.inv_int_pol	= 0,
			}
		},
#endif
#ifdef CONFIG_OPTICAL_BH1721_I957
		{
			PM8058_GPIO_PM_TO_SYS(ALC_RST),
			{
				.direction	= PM_GPIO_DIR_OUT,
				.pull			= PM_GPIO_PULL_NO,
				.vin_sel		= 2,
				.function		= PM_GPIO_FUNC_NORMAL,
				.inv_int_pol	= 0,
			}
		},
#endif
#ifdef CONFIG_VIDEO_MHL_TABLET_V1
		{
			PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT),
			{
				.direction	= PM_GPIO_DIR_IN,
				.pull			= PM_GPIO_PULL_NO,
				.vin_sel		= 2,
				.function		= PM_GPIO_FUNC_NORMAL,
				.inv_int_pol	= 0,
			}
		},
		{
			PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_RST),
			{
				.direction	= PM_GPIO_DIR_OUT,
				.pull			= PM_GPIO_PULL_NO,
				.vin_sel		= 4,
				.function		= PM_GPIO_FUNC_NORMAL,
				.inv_int_pol	= 0,
			}
		},
#endif				
		{
			PM8058_GPIO_PM_TO_SYS(PMIC_HWREV0),
			{
				.direction	= PM_GPIO_DIR_IN,
				.pull			= PM_GPIO_PULL_DN,
				.vin_sel		= 2,
				.function		= PM_GPIO_FUNC_NORMAL,
				.inv_int_pol	= 0,
			}
		},
		{
			PM8058_GPIO_PM_TO_SYS(PMIC_HWREV1),
			{
				.direction	= PM_GPIO_DIR_IN,
				.pull			= PM_GPIO_PULL_DN,
				.vin_sel		= 2,
				.function		= PM_GPIO_FUNC_NORMAL,
				.inv_int_pol	= 0,
			}
		},
		{
			PM8058_GPIO_PM_TO_SYS(PMIC_HWREV2),
			{
				.direction	= PM_GPIO_DIR_IN,
				.pull			= PM_GPIO_PULL_DN,
				.vin_sel		= 2,
				.function		= PM_GPIO_FUNC_NORMAL,
				.inv_int_pol	= 0,
			}
		},
		{
			PM8058_GPIO_PM_TO_SYS(PMIC_HWREV3),
			{
				.direction	= PM_GPIO_DIR_IN,
				.pull			= PM_GPIO_PULL_DN,
				.vin_sel		= 2,
				.function		= PM_GPIO_FUNC_NORMAL,
				.inv_int_pol	= 0,
			}
		},
#ifdef CONFIG_30PIN_CONN
		{
			PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_DOCK_INT),
			{
				.direction      = PM_GPIO_DIR_IN,
				.pull           = PM_GPIO_PULL_NO,
				.vin_sel        = 2,
				.function       = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol    = 0,
			}
		},
		{
			PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_ACCESSORY_EN),
			{
				.direction	= PM_GPIO_DIR_OUT,
				.output_value	= 0,
				.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
				.pull		= PM_GPIO_PULL_NO,
				.out_strength	= PM_GPIO_STRENGTH_HIGH,
				.function	= PM_GPIO_FUNC_NORMAL,
				.vin_sel	= 2,
				.inv_int_pol	= 0,
			},
		},
#endif
	};

struct pm_gpio main_micbiase = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = 2,
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol    = 0,
	};

	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MAIN_MICBIAS_EN), &main_micbiase);
	if (rc) 
	{
		pr_err("%s PMIC_GPIO_MAIN_MICBIAS_EN config failed\n", __func__);
		return rc;
	}

#ifdef CONFIG_SAMSUNG_JACK
	struct pm_gpio ear_det = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = 2,
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol    = 0,
	};

	struct pm_gpio short_sendend = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = 2,
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol    = 0,
	};
	struct pm_gpio ear_amp_en = {
		.direction      = PM_GPIO_DIR_OUT,
		.output_value	= 1,
		.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
		.pull			= PM_GPIO_PULL_NO,
		.out_strength	= PM_GPIO_STRENGTH_HIGH,
		.function		= PM_GPIO_FUNC_NORMAL,
		.vin_sel		= 2,
		.inv_int_pol	= 0,
	};
	struct pm_gpio ear_micbiase = {
		.direction      = PM_GPIO_DIR_OUT,
		.pull           = PM_GPIO_PULL_NO,
		.out_strength   = PM_GPIO_STRENGTH_HIGH,
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol    = 0,
		.vin_sel        = 2,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 0,
	};

	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_DET), &ear_det);
	if (rc) {
		pr_err("%s PMIC_GPIO_EAR_DET config failed\n", __func__);
		return rc;
	}
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SHORT_SENDEND), &short_sendend);
	if (rc) {
		pr_err("%s PMIC_GPIO_SHORT_SENDEND config failed\n", __func__);
		return rc;
	}

#if defined(CONFIG_USA_OPERATOR_ATT) || defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU) || defined(CONFIG_EUR_OPERATOR_OPEN)
	if(system_rev >= 0x4)
        {
	 	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_MICBIAS_EN), &ear_micbiase);
		if (rc) {
			pr_err("%s PMIC_GPIO_EAR_MICBIAS_EN config failed\n", __func__);
			return rc;
		}
        } 
#endif

	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MUTE_CON), &ear_amp_en);
	if (rc) {
		pr_err("%s PMIC_GPIO_MUTE_CON config failed\n", __func__);
		return rc;
	}

	rc =  gpio_request(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MUTE_CON), "EAR_AMP_EN");
	if (rc) {
		pr_err("%s:Failed to request GPIO %d\n", __func__, PMIC_GPIO_MUTE_CON);
		return rc;
	} 

	rc = gpio_request(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MAIN_MICBIAS_EN), "MAINMIC_BIAS_EN");
	if (rc) {
		pr_err("%s:Failed to request GPIO %d\n", __func__, PMIC_GPIO_MAIN_MICBIAS_EN);
		return rc;
	} 
	
	if(system_rev>=0x04)
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MUTE_CON), 1);
	else
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MUTE_CON), 0);
#endif

	for (i = 0; i < ARRAY_SIZE(gpio_cfgs); ++i) {
		rc = pm8xxx_gpio_config(gpio_cfgs[i].gpio,
				&gpio_cfgs[i].cfg);
		if (rc < 0) {
			pr_err("%s pmic gpio config failed\n",
				__func__);
			return rc;
		}
	}

#ifdef CONFIG_30PIN_CONN
	struct pm_gpio accessory_check = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = 2,
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol    = 0,
	};
	int gpio_acc_check;

	if (system_rev < 0x04)
		gpio_acc_check = PMIC_GPIO_ACCESSORY_CHECK;
	else
		gpio_acc_check = PMIC_GPIO_ACCESSORY_CHECK_04;

	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(gpio_acc_check), &accessory_check);
	if (rc < 0) {
		pr_err("%s accessory_check gpio config failed\n",
			__func__);
		return rc;
	}
#endif

	struct pm8058_gpio_cfg uicc_det = 
	{
		PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_UICC_DET),
		{
			.direction	= PM_GPIO_DIR_IN,
			.pull		= PM_GPIO_PULL_NO,
			.vin_sel	= 2,
			.function 	= PM_GPIO_FUNC_NORMAL,
			.inv_int_pol	= 0, 
		},	
	};
		
	rc = pm8xxx_gpio_config( uicc_det.gpio, &uicc_det.cfg );
	if( rc < 0 ){
		pr_err("%s uicc_det gpio config failed\n", __func__ );
		return rc;
	}
	
	return 0;
}

static const unsigned int ffa_keymap[] = {
	KEY(0, 0, KEY_VOLUMEUP),
	KEY(0, 1, KEY_VOLUMEDOWN),
	KEY(0, 2, KEY_RESERVED),		
	KEY(0, 3, KEY_RESERVED),
	KEY(0, 4, KEY_RESERVED),
};

#if 0
static struct resource resources_keypad[] = {
	{
		.start	= PM8058_GPIO_PM_TO_SYS(VOLUME_DOWN),
		.end	= PM8058_GPIO_PM_TO_SYS(VOLUME_DOWN),
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= PM8058_GPIO_PM_TO_SYS(VOLUME_UP),
		.end	= PM8058_GPIO_PM_TO_SYS(VOLUME_UP),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct resource resources_keypad_rev04[] = {
	{
		.start	= PM8058_KEYPAD_IRQ(PM8058_IRQ_BASE),
		.end	= PM8058_KEYPAD_IRQ(PM8058_IRQ_BASE),
		.flags	= IORESOURCE_IRQ,
	},
	{
		.start	= PM8058_KEYSTUCK_IRQ(PM8058_IRQ_BASE),
		.end	= PM8058_KEYSTUCK_IRQ(PM8058_IRQ_BASE),
		.flags	= IORESOURCE_IRQ,
	},
};
#endif

static struct matrix_keymap_data ffa_keymap_data = {
	.keymap_size	= ARRAY_SIZE(ffa_keymap),
	.keymap		= ffa_keymap,
};

static struct pm8xxx_keypad_platform_data ffa_keypad_data = {
	.input_name		= "sec_key",
	.input_phys_device	= "sec_key/input0",
	.num_rows		= 1,
	.num_cols		= 5,
	.rows_gpio_start	= PM8058_GPIO_PM_TO_SYS(8),
	.cols_gpio_start	= PM8058_GPIO_PM_TO_SYS(0),
	.debounce_ms		= 15,
	.scan_delay_ms	= 32,
	.row_hold_ns		= 91500,
	.wakeup			= 1,
	.keymap_data		= &ffa_keymap_data,
};

static struct pm8xxx_rtc_platform_data pm8058_rtc_pdata = {
	.rtc_write_enable       = true,
	.rtc_alarm_powerup	= false,
};

static struct pm8xxx_pwrkey_platform_data pm8058_pwrkey_pdata = {
	.pull_up		= 1,
	.kpd_trigger_delay_us   = 15625,
	.wakeup			= 1,
};

#if defined(CONFIG_PMIC8058_OTHC) || defined(CONFIG_PMIC8058_OTHC_MODULE)
#define PM8058_OTHC_CNTR_BASE0	0xA0
#define PM8058_OTHC_CNTR_BASE1	0x134
#define PM8058_OTHC_CNTR_BASE2	0x137
#define PM8058_LINE_IN_DET_GPIO	PM8058_GPIO_PM_TO_SYS(18)

static struct othc_accessory_info othc_accessories[]  = {
	{
		.accessory = OTHC_SVIDEO_OUT,
		.detect_flags = OTHC_MICBIAS_DETECT | OTHC_SWITCH_DETECT
							| OTHC_ADC_DETECT,
		.key_code = SW_VIDEOOUT_INSERT,
		.enabled = false,
		.adc_thres = {
				.min_threshold = 20,
				.max_threshold = 40,
			},
	},
	{
		.accessory = OTHC_ANC_HEADPHONE,
		.detect_flags = OTHC_MICBIAS_DETECT | OTHC_GPIO_DETECT |
							OTHC_SWITCH_DETECT,
		.gpio = PM8058_LINE_IN_DET_GPIO,
		.active_low = 1,
		.key_code = SW_HEADPHONE_INSERT,
		.enabled = true,
	},
	{
		.accessory = OTHC_ANC_HEADSET,
		.detect_flags = OTHC_MICBIAS_DETECT | OTHC_GPIO_DETECT,
		.gpio = PM8058_LINE_IN_DET_GPIO,
		.active_low = 1,
		.key_code = SW_HEADPHONE_INSERT,
		.enabled = true,
	},
	{
		.accessory = OTHC_HEADPHONE,
		.detect_flags = OTHC_MICBIAS_DETECT | OTHC_SWITCH_DETECT,
		.key_code = SW_HEADPHONE_INSERT,
		.enabled = true,
	},
	{
		.accessory = OTHC_MICROPHONE,
		.detect_flags = OTHC_GPIO_DETECT,
		.gpio = PM8058_LINE_IN_DET_GPIO,
		.active_low = 1,
		.key_code = SW_MICROPHONE_INSERT,
		.enabled = true,
	},
	{
		.accessory = OTHC_HEADSET,
		.detect_flags = OTHC_MICBIAS_DETECT,
		.key_code = SW_HEADPHONE_INSERT,
		.enabled = true,
	},
};

static struct othc_switch_info switch_info[] = {
	{
		.min_adc_threshold = 0,
		.max_adc_threshold = 100,
		.key_code = KEY_PLAYPAUSE,
	},
	{
		.min_adc_threshold = 100,
		.max_adc_threshold = 200,
		.key_code = KEY_REWIND,
	},
	{
		.min_adc_threshold = 200,
		.max_adc_threshold = 500,
		.key_code = KEY_FASTFORWARD,
	},
};

static struct othc_n_switch_config switch_config = {
	.voltage_settling_time_ms = 0,
	.num_adc_samples = 3,
	.adc_channel = CHANNEL_ADC_HDSET,
	.switch_info = switch_info,
	.num_keys = ARRAY_SIZE(switch_info),
	.default_sw_en = true,
	.default_sw_idx = 0,
};

static struct hsed_bias_config hsed_bias_config = {
	/* HSED mic bias config info */
	.othc_headset = OTHC_HEADSET_NO,
	.othc_lowcurr_thresh_uA = 100,
	.othc_highcurr_thresh_uA = 700, //SAMSUNG 2011.03.21     600,
	.othc_hyst_prediv_us = 7800,
	.othc_period_clkdiv_us = 62500,
	.othc_hyst_clk_us = 121000,
	.othc_period_clk_us = 312500,
	.othc_wakeup = 1,
};

static struct othc_hsed_config hsed_config_1 = {
	.hsed_bias_config = &hsed_bias_config,
	/*
	 * The detection delay and switch reporting delay are
	 * required to encounter a hardware bug (spurious switch
	 * interrupts on slow insertion/removal of the headset).
	 * This will introduce a delay in reporting the accessory
	 * insertion and removal to the userspace.
	 */
	.detection_delay_ms = 1500,
	/* Switch info */
	.switch_debounce_ms = 1500,
	.othc_support_n_switch = false,
	.switch_config = &switch_config,
	.ir_gpio = -1,
	/* Accessory info */
	.accessories_support = true,
	.accessories = othc_accessories,
	.othc_num_accessories = ARRAY_SIZE(othc_accessories),
};

static struct othc_regulator_config othc_reg = {
	.regulator	 = "8058_l5",
	.max_uV		 = 2850000,
	.min_uV		 = 2850000,
};

/* MIC_BIAS0 is configured as normal MIC BIAS */
static struct pmic8058_othc_config_pdata othc_config_pdata_0 = {
	.micbias_select = OTHC_MICBIAS_0,
	.micbias_capability = OTHC_MICBIAS,
	.micbias_enable = OTHC_SIGNAL_OFF,
	.micbias_regulator = &othc_reg,
};

#if defined(CONFIG_SAMSUNG_JACK)
static struct pmic8058_othc_config_pdata othc_config_pdata_1 = {
	.micbias_select = OTHC_MICBIAS_1,
	.micbias_capability = OTHC_MICBIAS,
	.micbias_enable = OTHC_SIGNAL_OFF,
	.micbias_regulator = &othc_reg,
};
#else
/* MIC_BIAS1 is configured as HSED_BIAS for OTHC */
static struct pmic8058_othc_config_pdata othc_config_pdata_1 = {
	.micbias_select = OTHC_MICBIAS_1,
	.micbias_capability = OTHC_MICBIAS_HSED,
	.micbias_enable = OTHC_SIGNAL_PWM_TCXO,
	.micbias_regulator = &othc_reg,
	.hsed_config = &hsed_config_1,
	.hsed_name = "8660_handset",
};
#endif

/* MIC_BIAS2 is configured as normal MIC BIAS */
static struct pmic8058_othc_config_pdata othc_config_pdata_2 = {
	.micbias_select = OTHC_MICBIAS_2,
	.micbias_capability = OTHC_MICBIAS,
	.micbias_enable = OTHC_SIGNAL_OFF,
	.micbias_regulator = &othc_reg,
};

static void __init msm8x60_init_pm8058_othc(void)
{
	int i;

	if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 2) {
		/* 3-switch headset supported only by V2 FFA and FLUID */
		hsed_config_1.accessories_adc_support = true,
		/* ADC based accessory detection works only on V2 and FLUID */
		hsed_config_1.accessories_adc_channel = CHANNEL_ADC_HDSET,
		hsed_config_1.othc_support_n_switch = true;
	}
}
#endif

#ifdef CONFIG_PMIC8058_PWM
static int pm8058_pwm_config(struct pwm_device *pwm, int ch, int on)
{
	struct pm_gpio pwm_gpio_config = {
		.direction      = PM_GPIO_DIR_OUT,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 0,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = PM8058_GPIO_VIN_VPH,
		.out_strength   = PM_GPIO_STRENGTH_HIGH,
		.function       = PM_GPIO_FUNC_2,
	};

	int rc = -EINVAL;
	int id, mode, max_mA;

	id = mode = max_mA = 0;
	switch (ch) {
	case 0:
	case 1:
	case 2:
		if (on) {
			id = 24 + ch;
			rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(id - 1),
							&pwm_gpio_config);
			if (rc)
				pr_err("%s: pm8xxx_gpio_config(%d): rc=%d\n",
					__func__, id, rc);
		}
		break;

	case 6:
		id = PM_PWM_LED_FLASH;
		mode = PM_PWM_CONF_PWM1;
		max_mA = 300;
		break;

	case 7:
		id = PM_PWM_LED_FLASH1;
		mode = PM_PWM_CONF_PWM1;
		max_mA = 300;
		break;

	default:
		break;
	}

	if (ch >= 6 && ch <= 7) {
		if (!on) {
			mode = PM_PWM_CONF_NONE;
			max_mA = 0;
		}
		rc = pm8058_pwm_config_led(pwm, id, mode, max_mA);
		if (rc)
			pr_err("%s: pm8058_pwm_config_led(ch=%d): rc=%d\n",
			       __func__, ch, rc);
	}
	return rc;

}

static struct pm8058_pwm_pdata pm8058_pwm_data = {
	.config		= pm8058_pwm_config,
};
#endif

#define PM8058_GPIO_INT           88

static struct pmic8058_led pmic8058_flash_leds[] = {
	[0] = {
		.name		= "camera:flash0",
		.max_brightness = 15,
		.id		= PMIC8058_ID_FLASH_LED_0,
	},
	[1] = {
		.name		= "camera:flash1",
		.max_brightness = 15,
		.id		= PMIC8058_ID_FLASH_LED_1,
	},
};

static struct pmic8058_leds_platform_data pm8058_flash_leds_data = {
	.num_leds = ARRAY_SIZE(pmic8058_flash_leds),
	.leds	= pmic8058_flash_leds,
};

static struct pmic8058_led pmic8058_fluid_flash_leds[] = {
	[0] = {
		.name		= "led:drv0",
		.max_brightness = 15,
		.id		= PMIC8058_ID_FLASH_LED_0,
	},/* 300 mA flash led0 drv sink */
	[1] = {
		.name		= "led:drv1",
		.max_brightness = 15,
		.id		= PMIC8058_ID_FLASH_LED_1,
	},/* 300 mA flash led1 sink */
	[2] = {
		.name		= "led:drv2",
		.max_brightness = 20,
		.id		= PMIC8058_ID_LED_0,
	},/* 40 mA led0 sink */
	[3] = {
		.name		= "keypad:drv",
		.max_brightness = 15,
		.id		= PMIC8058_ID_LED_KB_LIGHT,
	},/* 300 mA keypad drv sink */
};

static struct pmic8058_leds_platform_data pm8058_fluid_flash_leds_data = {
	.num_leds = ARRAY_SIZE(pmic8058_fluid_flash_leds),
	.leds	= pmic8058_fluid_flash_leds,
};

static struct pm8xxx_misc_platform_data pm8058_misc_pdata = {
	.priority		= 0,
};

static struct pm8xxx_irq_platform_data pm8058_irq_pdata = {
	.irq_base		= PM8058_IRQ_BASE,
	.devirq			= MSM_GPIO_TO_INT(PM8058_GPIO_INT),
	.irq_trigger_flag	= IRQF_TRIGGER_LOW,
};

static struct pm8xxx_gpio_platform_data pm8058_gpio_pdata = {
	.gpio_base	= PM8058_GPIO_PM_TO_SYS(0),
};

static struct pm8xxx_mpp_platform_data pm8058_mpp_pdata = {
	.mpp_base	= PM8058_MPP_PM_TO_SYS(0),
};

static struct pm8058_platform_data pm8058_platform_data = {
	.irq_pdata		= &pm8058_irq_pdata,
	.gpio_pdata		= &pm8058_gpio_pdata,
	.mpp_pdata		= &pm8058_mpp_pdata,
	.rtc_pdata		= &pm8058_rtc_pdata,
	.pwrkey_pdata	= &pm8058_pwrkey_pdata,
	.othc0_pdata		= &othc_config_pdata_0,
	.othc1_pdata		= &othc_config_pdata_1,
	.othc2_pdata		= &othc_config_pdata_2,
#ifdef CONFIG_PMIC8058_PWM
	.pwm_pdata		= &pm8058_pwm_data,
#endif	
	.misc_pdata		= &pm8058_misc_pdata,
#ifdef CONFIG_SENSORS_MSM_ADC
	.xoadc_pdata		= &pm8058_xoadc_pdata,
#endif
};

#ifdef CONFIG_MSM_SSBI
static struct msm_ssbi_platform_data msm8x60_ssbi_pm8058_pdata __devinitdata = {
	.controller_type = MSM_SBI_CTRL_PMIC_ARBITER,
	.slave	= {
		.name			= "pm8058-core",
		.platform_data		= &pm8058_platform_data,
	},
};
#endif
#endif /* CONFIG_PMIC8058 */

static struct regulator *vreg_timpani_1;
static struct regulator *vreg_timpani_2;

static unsigned int msm_timpani_setup_power(void)
{
	int rc;

	pr_err("%s L0 DIG_1.2V S3 IO_1.8V\n", __func__);

	vreg_timpani_1 = regulator_get(NULL, "8058_l0");
	if (IS_ERR(vreg_timpani_1)) {
		pr_err("%s: Unable to get 8058_l0\n", __func__);
		return -ENODEV;
	}

	vreg_timpani_2 = regulator_get(NULL, "8058_s3");
	if (IS_ERR(vreg_timpani_2)) {
		pr_err("%s: Unable to get 8058_s3\n", __func__);
		regulator_put(vreg_timpani_1);
		return -ENODEV;
	}

	rc = regulator_set_voltage(vreg_timpani_1, 1200000, 1200000);
	if (rc) {
		pr_err("%s: unable to set L0 voltage to 1.2V\n", __func__);
		goto fail;
	}

	rc = regulator_set_voltage(vreg_timpani_2, 1800000, 1800000);
	if (rc) {
		pr_err("%s: unable to set S3 voltage to 1.8V\n", __func__);
		goto fail;
	}

	rc = regulator_enable(vreg_timpani_1);
	if (rc) {
		pr_err("%s: Enable regulator 8058_l0 failed\n", __func__);
		goto fail;
	}

	/* The settings for LDO0 should be set such that
	*  it doesn't require to reset the timpani. */
	rc = regulator_set_optimum_mode(vreg_timpani_1, 5000);
	if (rc < 0) {
		pr_err("Timpani regulator optimum mode setting failed\n");
		goto fail;
	}

	rc = regulator_enable(vreg_timpani_2);
	if (rc) {
		pr_err("%s: Enable regulator 8058_s3 failed\n", __func__);
		regulator_disable(vreg_timpani_1);
		goto fail;
	}

	rc = gpio_request(GPIO_CDC_RST_N, "CDC_RST_N");
	if (rc) {
		pr_err("%s: GPIO Request %d failed\n", __func__,
			GPIO_CDC_RST_N);
		regulator_disable(vreg_timpani_1);
		regulator_disable(vreg_timpani_2);
		goto fail;
	} else {
		gpio_direction_output(GPIO_CDC_RST_N, 1);
		usleep_range(1000, 1050);
		gpio_direction_output(GPIO_CDC_RST_N, 0);
		usleep_range(1000, 1050);
		gpio_direction_output(GPIO_CDC_RST_N, 1);
		gpio_free(GPIO_CDC_RST_N);
	}
	return rc;

fail:
	regulator_put(vreg_timpani_1);
	regulator_put(vreg_timpani_2);
	return rc;
}

static void msm_timpani_shutdown_power(void)
{
	int rc;

	rc = regulator_disable(vreg_timpani_1);
	if (rc)
		pr_err("%s: Disable regulator 8058_l0 failed\n", __func__);

	regulator_put(vreg_timpani_1);

	rc = regulator_disable(vreg_timpani_2);
	if (rc)
		pr_err("%s: Disable regulator 8058_s3 failed\n", __func__);

	regulator_put(vreg_timpani_2);
}

/* Power analog function of codec */
static struct regulator *vreg_timpani_cdc_apwr;
static int msm_timpani_codec_power(int vreg_on)
{
	int rc = 0;

	pr_err("%s S4 AUD_2.2V %s\n", __func__ , vreg_on ? "on":"off");
	if (!vreg_timpani_cdc_apwr) {

		vreg_timpani_cdc_apwr = regulator_get(NULL, "8058_s4");

		if (IS_ERR(vreg_timpani_cdc_apwr)) {
			pr_err("%s: vreg_get failed (%ld)\n",
			__func__, PTR_ERR(vreg_timpani_cdc_apwr));
			rc = PTR_ERR(vreg_timpani_cdc_apwr);
			return rc;
		}
	}

	if (vreg_on) {

		rc = regulator_set_voltage(vreg_timpani_cdc_apwr,
				2200000, 2200000);
		if (rc) {
			pr_err("%s: unable to set 8058_s4 voltage to 2.2 V\n",
					__func__);
			goto vreg_fail;
		}

		rc = regulator_enable(vreg_timpani_cdc_apwr);
		if (rc) {
			pr_err("%s: vreg_enable failed %d\n", __func__, rc);
			goto vreg_fail;
		}
	} else {
		rc = regulator_disable(vreg_timpani_cdc_apwr);
		if (rc) {
			pr_err("%s: vreg_disable failed %d\n",
			__func__, rc);
			goto vreg_fail;
		}
	}

	return 0;

vreg_fail:
	regulator_put(vreg_timpani_cdc_apwr);
	vreg_timpani_cdc_apwr = NULL;
	return rc;
}

static struct marimba_codec_platform_data timpani_codec_pdata = {
	.marimba_codec_power =  msm_timpani_codec_power,
};

#define TIMPANI_SLAVE_ID_CDC_ADDR		0x77
#define TIMPANI_SLAVE_ID_QMEMBIST_ADDR		0x66

static struct marimba_platform_data timpani_pdata = {
	.slave_id[MARIMBA_SLAVE_ID_CDC]	= TIMPANI_SLAVE_ID_CDC_ADDR,
	.slave_id[MARIMBA_SLAVE_ID_QMEMBIST] = TIMPANI_SLAVE_ID_QMEMBIST_ADDR,
	.marimba_setup = msm_timpani_setup_power,
	.marimba_shutdown = msm_timpani_shutdown_power,
	.codec = &timpani_codec_pdata,
	.tsadc_ssbi_adap = MARIMBA_SSBI_ADAP,
};

#define TIMPANI_I2C_SLAVE_ADDR	0xD

static struct i2c_board_info msm_i2c_gsbi7_timpani_info[] = {
	{
		I2C_BOARD_INFO("timpani", TIMPANI_I2C_SLAVE_ADDR),
		.platform_data = &timpani_pdata,
	},
};

#ifdef CONFIG_PMIC8901

#define PM8901_GPIO_INT           91
/*
 * Consumer specific regulator names:
 *			 regulator name		consumer dev_name
 */
static struct regulator_consumer_supply vreg_consumers_8901_MPP0[] = {
	REGULATOR_SUPPLY("8901_mpp0",	NULL),
};
static struct regulator_consumer_supply vreg_consumers_8901_USB_OTG[] = {
	REGULATOR_SUPPLY("8901_usb_otg",	NULL),
};
static struct regulator_consumer_supply vreg_consumers_8901_HDMI_MVS[] = {
	REGULATOR_SUPPLY("8901_hdmi_mvs",	NULL),
	REGULATOR_SUPPLY("8901_mpp0",	NULL),
};

#define PM8901_VREG_INIT(_id, _min_uV, _max_uV, _modes, _ops, _apply_uV, \
			 _always_on) \
	{ \
		.init_data = { \
			.constraints = { \
				.valid_modes_mask = _modes, \
				.valid_ops_mask = _ops, \
				.min_uV = _min_uV, \
				.max_uV = _max_uV, \
				.input_uV = _min_uV, \
				.apply_uV = _apply_uV, \
				.always_on = _always_on, \
			}, \
			.consumer_supplies = vreg_consumers_8901_##_id, \
			.num_consumer_supplies = \
				ARRAY_SIZE(vreg_consumers_8901_##_id), \
		}, \
		.id = PM8901_VREG_ID_##_id, \
	}

#define PM8901_VREG_INIT_VS(_id) \
	PM8901_VREG_INIT(_id, 0, 0, REGULATOR_MODE_NORMAL, \
			REGULATOR_CHANGE_STATUS, 0, 0)

static struct pm8901_vreg_pdata pm8901_vreg_init[] = {
	PM8901_VREG_INIT_VS(USB_OTG),
	PM8901_VREG_INIT_VS(HDMI_MVS),
};

static struct pm8xxx_misc_platform_data pm8901_misc_pdata = {
	.priority		= 1,
};

static struct pm8xxx_irq_platform_data pm8901_irq_pdata = {
	.irq_base		= PM8901_IRQ_BASE,
	.devirq			= MSM_GPIO_TO_INT(PM8901_GPIO_INT),
	.irq_trigger_flag	= IRQF_TRIGGER_LOW,
};

static struct pm8xxx_mpp_platform_data pm8901_mpp_pdata = {
	.mpp_base		= PM8901_MPP_PM_TO_SYS(0),
};

static struct pm8901_platform_data pm8901_platform_data = {
	.irq_pdata		= &pm8901_irq_pdata,
	.mpp_pdata		= &pm8901_mpp_pdata,
	.regulator_pdatas	= pm8901_vreg_init,
	.num_regulators		= ARRAY_SIZE(pm8901_vreg_init),
	.misc_pdata		= &pm8901_misc_pdata,
};

static struct msm_ssbi_platform_data msm8x60_ssbi_pm8901_pdata __devinitdata = {
	.controller_type = MSM_SBI_CTRL_PMIC_ARBITER,
	.slave	= {
		.name = "pm8901-core",
		.platform_data = &pm8901_platform_data,
	},
};
#endif /* CONFIG_PMIC8901 */

#if defined(CONFIG_MARIMBA_CORE) 

static struct regulator *vreg_bahama;

struct bahama_config_register{
	u8 reg;
	u8 value;
	u8 mask;
};

enum version{
	VER_1_0,
	VER_2_0,
	VER_UNSUPPORTED = 0xFF
};

static u8 read_bahama_ver(void)
{
	int rc;
	struct marimba config = { .mod_id = SLAVE_ID_BAHAMA };
	u8 bahama_version;

	rc = marimba_read_bit_mask(&config, 0x00,  &bahama_version, 1, 0x1F);
	if (rc < 0) {
		printk(KERN_ERR
			 "%s: version read failed: %d\n",
			__func__, rc);
			return VER_UNSUPPORTED;
	} else {
		printk(KERN_INFO
		"%s: version read got: 0x%x\n",
		__func__, bahama_version);
	}

	switch (bahama_version) {
	case 0x08: /* varient of bahama v1 */
	case 0x10:
	case 0x00:
		return VER_1_0;
	case 0x09: /* variant of bahama v2 */
		return VER_2_0;
	default:
		return VER_UNSUPPORTED;
	}
}

static int msm_bahama_setup_power_enable;
static unsigned int msm_bahama_setup_power(void)
{
	int rc = 0;
	const char *msm_bahama_regulator = "8058_s3";

	vreg_bahama = regulator_get(NULL, msm_bahama_regulator);

	if (IS_ERR(vreg_bahama)) {
		rc = PTR_ERR(vreg_bahama);
		pr_err("%s: regulator_get %s = %d\n", __func__,
			msm_bahama_regulator, rc);
		return rc;
	}

		rc = regulator_set_voltage(vreg_bahama, 1800000, 1800000);
	if (rc) {
		pr_err("%s: regulator_set_voltage %s = %d\n", __func__,
			msm_bahama_regulator, rc);
		goto unget;
	}

	rc = regulator_enable(vreg_bahama);
	if (rc) {
		pr_err("%s: regulator_enable %s = %d\n", __func__,
			msm_bahama_regulator, rc);
		goto unget;
	}

	msm_bahama_setup_power_enable = 1;
	return rc;

unenable:
	regulator_disable(vreg_bahama);
unget:
	regulator_put(vreg_bahama);
	return rc;
};

static unsigned int msm_bahama_shutdown_power(int value)
{
	if (msm_bahama_setup_power_enable) {
		regulator_disable(vreg_bahama);
		regulator_put(vreg_bahama);
		msm_bahama_setup_power_enable = 0;
	}

	return 0;
};

static unsigned int msm_bahama_core_config(int type)
{
	int rc = 0;

	if (type == BAHAMA_ID) {

		int i;
		struct marimba config = { .mod_id = SLAVE_ID_BAHAMA };

		const struct bahama_config_register v20_init[] = {
			/* reg, value, mask */
			{ 0xF4, 0x84, 0xFF }, /* AREG */
			{ 0xF0, 0x04, 0xFF } /* DREG */
		};

		if (read_bahama_ver() == VER_2_0) {
			for (i = 0; i < ARRAY_SIZE(v20_init); i++) {
				u8 value = v20_init[i].value;
				rc = marimba_write_bit_mask(&config,
					v20_init[i].reg,
					&value,
					sizeof(v20_init[i].value),
					v20_init[i].mask);
				if (rc < 0) {
					printk(KERN_ERR
						"%s: reg %d write failed: %d\n",
						__func__, v20_init[i].reg, rc);
					return rc;
				}
				printk(KERN_INFO "%s: reg 0x%02x value 0x%02x"
					" mask 0x%02x\n",
					__func__, v20_init[i].reg,
					v20_init[i].value, v20_init[i].mask);
			}
		}
	}
	printk(KERN_INFO "core type: %d\n", type);

	return rc;
}

static struct regulator *fm_regulator_s3;
static struct msm_xo_voter *fm_clock;

static int fm_radio_setup(struct marimba_fm_platform_data *pdata)
{
	int rc = 0;
	struct pm_gpio cfg = {
				.direction      = PM_GPIO_DIR_IN,
				.pull           = PM_GPIO_PULL_NO,
				.vin_sel        = PM8058_GPIO_VIN_S3,
				.function       = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol    = 0,
				};

	if (!fm_regulator_s3) {
		fm_regulator_s3 = regulator_get(NULL, "8058_s3");
		if (IS_ERR(fm_regulator_s3)) {
			rc = PTR_ERR(fm_regulator_s3);
			printk(KERN_ERR "%s: regulator get s3 (%d)\n",
				__func__, rc);
			goto out;
			}
	}


	rc = regulator_set_voltage(fm_regulator_s3, 1800000, 1800000);
	if (rc < 0) {
		printk(KERN_ERR "%s: regulator set voltage failed (%d)\n",
				__func__, rc);
		goto fm_fail_put;
	}

	rc = regulator_enable(fm_regulator_s3);
	if (rc < 0) {
		printk(KERN_ERR "%s: regulator s3 enable failed (%d)\n",
					__func__, rc);
		goto fm_fail_put;
	}

	/*Vote for XO clock*/
	fm_clock = msm_xo_get(MSM_XO_TCXO_D0, "fm_power");

	if (IS_ERR(fm_clock)) {
		rc = PTR_ERR(fm_clock);
		printk(KERN_ERR "%s: Couldn't get TCXO_D0 vote for FM (%d)\n",
					__func__, rc);
		goto fm_fail_switch;
	}

	rc = msm_xo_mode_vote(fm_clock, MSM_XO_MODE_ON);
	if (rc < 0) {
		printk(KERN_ERR "%s:  Failed to vote for TCX0_D0 ON (%d)\n",
					__func__, rc);
		goto fm_fail_vote;
	}

	/*GPIO 18 on PMIC is FM_IRQ*/
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(FM_GPIO), &cfg);
	if (rc) {
		printk(KERN_ERR "%s: return val of pm8xxx_gpio_config: %d\n",
						__func__,  rc);
		goto fm_fail_clock;
	}
	goto out;

fm_fail_clock:
		msm_xo_mode_vote(fm_clock, MSM_XO_MODE_OFF);
fm_fail_vote:
		msm_xo_put(fm_clock);
fm_fail_switch:
		regulator_disable(fm_regulator_s3);
fm_fail_put:
		regulator_put(fm_regulator_s3);
out:
	return rc;
};

static void fm_radio_shutdown(struct marimba_fm_platform_data *pdata)
{
	int rc = 0;
	if (fm_regulator_s3 != NULL) {
		rc = regulator_disable(fm_regulator_s3);
		if (rc < 0) {
			printk(KERN_ERR "%s: regulator s3 disable (%d)\n",
							__func__, rc);
		}
		regulator_put(fm_regulator_s3);
		fm_regulator_s3 = NULL;
	}
	printk(KERN_ERR "%s: Voting off for XO", __func__);

	if (fm_clock != NULL) {
		rc = msm_xo_mode_vote(fm_clock, MSM_XO_MODE_OFF);
		if (rc < 0) {
			printk(KERN_ERR "%s: Voting off XO clock (%d)\n",
					__func__, rc);
		}
		msm_xo_put(fm_clock);
	}
	printk(KERN_ERR "%s: coming out of fm_radio_shutdown", __func__);
}

/* Slave id address for FM/CDC/QMEMBIST
 * Values can be programmed using Marimba slave id 0
 * should there be a conflict with other I2C devices
 * */
#define BAHAMA_SLAVE_ID_FM_ADDR         0x2A
#define BAHAMA_SLAVE_ID_QMEMBIST_ADDR   0x7B

static struct marimba_fm_platform_data marimba_fm_pdata = {
	.fm_setup =  fm_radio_setup,
	.fm_shutdown = fm_radio_shutdown,
	.irq = PM8058_GPIO_IRQ(PM8058_IRQ_BASE, FM_GPIO),
	.is_fm_soc_i2s_master = false,
	.config_i2s_gpio = NULL,
};

/*
Just initializing the BAHAMA related slave
*/
static struct marimba_platform_data marimba_pdata = {
	.slave_id[SLAVE_ID_BAHAMA_FM]        = BAHAMA_SLAVE_ID_FM_ADDR,
	.slave_id[SLAVE_ID_BAHAMA_QMEMBIST]  = BAHAMA_SLAVE_ID_QMEMBIST_ADDR,
	.bahama_setup = msm_bahama_setup_power,
	.bahama_shutdown = msm_bahama_shutdown_power,
	.bahama_core_config = msm_bahama_core_config,
	.fm = &marimba_fm_pdata,
	.tsadc_ssbi_adap = MARIMBA_SSBI_ADAP,
};


static struct i2c_board_info msm_marimba_board_info[] = {
	{
		I2C_BOARD_INFO("marimba", 0xc),
		.platform_data = &marimba_pdata,
	}
};
#endif /* CONFIG_MAIMBA_CORE */

#ifdef CONFIG_I2C
#define I2C_SURF 1
#define I2C_FFA  (1 << 1)
#define I2C_RUMI (1 << 2)
#define I2C_SIM  (1 << 3)
#define I2C_FLUID (1 << 4)
#define I2C_DRAGON (1 << 5)
#define I2C_P5_LTE (1 << 5)

struct i2c_registry {
	u8                     machs;
	int                    bus;
	struct i2c_board_info *info;
	int                    len;
};

static struct i2c_registry msm8x60_i2c_devices[] __initdata = {
#ifdef CONFIG_OPTICAL_GP2A
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_OPT_I2C_BUS_ID,
		opt_i2c_borad_info,
		ARRAY_SIZE(opt_i2c_borad_info),
	},
#endif
#ifdef CONFIG_OPTICAL_BH1721_I957
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_OPT_I2C_BUS_ID,
		opt_i2c_borad_info,
		ARRAY_SIZE(opt_i2c_borad_info),
	},
#endif
#ifdef CONFIG_BATTERY_MAX17042
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_FG_I2C_BUS_ID,
		fg_i2c_devices,
		ARRAY_SIZE(fg_i2c_devices),
	},
#endif
	
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_GSBI3_QUP_I2C_BUS_ID,
		sec_i2c_touch_info,
		ARRAY_SIZE(sec_i2c_touch_info),
	},
#ifdef CONFIG_MSM_CAMERA
    {
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_GSBI4_QUP_I2C_BUS_ID,
		msm_camera_boardinfo,
		ARRAY_SIZE(msm_camera_boardinfo),
	},
#endif
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		20,
		msm_isa1200_board_info,
		ARRAY_SIZE(msm_isa1200_board_info),
	},	
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_GSBI7_QUP_I2C_BUS_ID,
		msm_i2c_gsbi7_timpani_info,
		ARRAY_SIZE(msm_i2c_gsbi7_timpani_info),
	},
#if defined(CONFIG_MARIMBA_CORE)
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_GSBI7_QUP_I2C_BUS_ID,
		msm_marimba_board_info,
		ARRAY_SIZE(msm_marimba_board_info),
	},
#endif /* CONFIG_MARIMBA_CORE */
#ifdef CONFIG_SENSORS_AK8975_I957
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_MAG_I2C_BUS_ID,
		akm_i2c_borad_info,
		ARRAY_SIZE(akm_i2c_borad_info),
	},
#endif
#ifdef CONFIG_VIDEO_MHL_TABLET_V1
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_GSBI8_QUP_I2C_BUS_ID,
		mhl_i2c_board_info,
		ARRAY_SIZE(mhl_i2c_board_info),		
	},
#endif
#ifdef CONFIG_GYRO_K3G_I957
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_GYRO_I2C_BUS_ID,
		gyro_i2c_borad_info,
		ARRAY_SIZE(gyro_i2c_borad_info),
	},
#endif
#ifdef CONFIG_CMC623_P5LTE
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_CMC623_I2C_BUS_ID,
		cmc623_i2c_borad_info,
		ARRAY_SIZE(cmc623_i2c_borad_info),
	},
#endif
#ifdef CONFIG_STMPE811_ADC
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_STMPE811_I2C_BUS_ID,
		stmpe811_i2c_borad_info,
		ARRAY_SIZE(stmpe811_i2c_borad_info),
	},
#endif
};

static struct i2c_registry p5lte_rev02_i2c_devices[] __initdata = {
#if defined (CONFIG_SENSORS_YDA165) || defined (CONFIG_SENSORS_YDA160)
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_AMP_I2C_BUS_ID,
		yamahaamp_boardinfo,
		ARRAY_SIZE(yamahaamp_boardinfo),
	},
#endif
#ifdef CONFIG_SENSORS_MAX9879
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_AMP_I2C_BUS_ID,
		maxamp_boardinfo,
		ARRAY_SIZE(maxamp_boardinfo),
	},
#endif

};

static struct i2c_registry p5lte_rev03_i2c_devices[] __initdata = {
#ifdef CONFIG_WM8994_AMP
	{
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_P5_LTE,
		MSM_AMP_I2C_BUS_ID,
		wm8994_i2c_borad_info,
		ARRAY_SIZE(wm8994_i2c_borad_info),
	},
#endif
};

#endif /* CONFIG_I2C */

static void fixup_i2c_configs(void)
{
}

static void register_i2c_devices(void)
{
#ifdef CONFIG_I2C
	u8 mach_mask = 0;
	int i;

	/* Build the matching 'supported_machs' bitmask */
	if (machine_is_msm8x60_surf() || machine_is_msm8x60_fusion())
		mach_mask = I2C_SURF;
	else if (machine_is_msm8x60_ffa() || machine_is_msm8x60_fusn_ffa())
		mach_mask = I2C_FFA;
	else if (machine_is_msm8x60_rumi3())
		mach_mask = I2C_RUMI;
	else if (machine_is_msm8x60_sim())
		mach_mask = I2C_SIM;
	else if (machine_is_msm8x60_fluid())
		mach_mask = I2C_FLUID;
	else if (machine_is_msm8x60_dragon())
		mach_mask = I2C_P5_LTE;
	else
		pr_err("unmatched machine ID in register_i2c_devices\n");

#if 0
	if ( system_rev >= 0x4 ) {
		pm8058_subdevs[0].num_resources = ARRAY_SIZE(resources_keypad_rev04);
		pm8058_subdevs[0].resources = resources_keypad_rev04;
	}
#endif

	/* Run the array and install devices as appropriate */
	for (i = 0; i < ARRAY_SIZE(msm8x60_i2c_devices); ++i) {
		if (msm8x60_i2c_devices[i].machs & mach_mask)
			i2c_register_board_info(msm8x60_i2c_devices[i].bus,
						msm8x60_i2c_devices[i].info,
						msm8x60_i2c_devices[i].len);
	}

	if ( system_rev >= 3 ) {
		for (i = 0; i < ARRAY_SIZE(p5lte_rev03_i2c_devices); ++i) {
			if (p5lte_rev03_i2c_devices[i].machs & mach_mask)
				i2c_register_board_info(p5lte_rev03_i2c_devices[i].bus,
							p5lte_rev03_i2c_devices[i].info,
							p5lte_rev03_i2c_devices[i].len);
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(p5lte_rev02_i2c_devices); ++i) {
			if (p5lte_rev02_i2c_devices[i].machs & mach_mask)
				i2c_register_board_info(p5lte_rev02_i2c_devices[i].bus,
							p5lte_rev02_i2c_devices[i].info,
							p5lte_rev02_i2c_devices[i].len);
		}
	}
#endif
}

static void __init msm8x60_init_uart12dm(void)
{
	/* 0x1D000000 now belongs to EBI2:CS3 i.e. USB ISP Controller */
	void *fpga_mem = ioremap_nocache(0x1D000000, SZ_4K);

	if (!fpga_mem)
		pr_err("%s(): Error getting memory\n", __func__);

	/* Advanced mode */
	writew(0xFFFF, fpga_mem + 0x15C);
	/* FPGA_UART_SEL */
	writew(0, fpga_mem + 0x172);
	/* FPGA_GPIO_CONFIG_117 */
	writew(1, fpga_mem + 0xEA);
	/* FPGA_GPIO_CONFIG_118 */
	writew(1, fpga_mem + 0xEC);
	mb();
	iounmap(fpga_mem);
}

#define MSM_GSBI9_PHYS		0x19900000
#define GSBI_DUAL_MODE_CODE	0x60

static void __init msm8x60_init_buses(void)
{
#ifdef CONFIG_I2C_QUP
	void *gsbi_mem = ioremap_nocache(0x19C00000, 4);
	/* Setting protocol code to 0x60 for dual UART/I2C in GSBI12 */
	writel_relaxed(0x6 << 4, gsbi_mem);
	/* Ensure protocol code is written before proceeding further */
	mb();
	iounmap(gsbi_mem);

	msm_gsbi3_qup_i2c_device.dev.platform_data = &msm_gsbi3_qup_i2c_pdata;
	msm_gsbi4_qup_i2c_device.dev.platform_data = &msm_gsbi4_qup_i2c_pdata;	// camera
	msm_gsbi7_qup_i2c_device.dev.platform_data = &msm_gsbi7_qup_i2c_pdata;
	msm_gsbi8_qup_i2c_device.dev.platform_data = &msm_gsbi8_qup_i2c_pdata;

#ifdef CONFIG_MSM_GSBI9_UART
	/* Setting protocol code to 0x60 for dual UART/I2C in GSBI9 */
	gsbi_mem = ioremap_nocache(MSM_GSBI9_PHYS, 4);
	writel_relaxed(GSBI_DUAL_MODE_CODE, gsbi_mem);
	iounmap(gsbi_mem);
	msm_gsbi9_qup_i2c_pdata.use_gsbi_shared_mode = 1;
#endif
	msm_gsbi9_qup_i2c_device.dev.platform_data = &msm_gsbi9_qup_i2c_pdata;
#ifndef CONFIG_KOR_OPERATOR_LGU
	msm_gsbi12_qup_i2c_device.dev.platform_data = &msm_gsbi12_qup_i2c_pdata;
#endif
#endif
#if defined(CONFIG_SPI_QUP) || defined(CONFIG_SPI_QUP_MODULE)
	msm_gsbi1_qup_spi_device.dev.platform_data = &msm_gsbi1_qup_spi_pdata;
#endif

#ifdef CONFIG_I2C_SSBI
	msm_device_ssbi3.dev.platform_data = &msm_ssbi3_pdata;
#endif

#ifdef CONFIG_MSM_SSBI
	msm_device_ssbi_pmic1.dev.platform_data =
				&msm8x60_ssbi_pm8058_pdata;
	msm_device_ssbi_pmic2.dev.platform_data =
				&msm8x60_ssbi_pm8901_pdata;
#endif

#if defined(CONFIG_USB_GADGET_MSM_72K) || defined(CONFIG_USB_EHCI_HCD)
	/*
	 * We can not put USB regulators (8058_l6 and 8058_l7) in LPM
	 * when we depend on USB PHY for VBUS/ID notifications. VBUS
	 * and ID notifications are available only on V2 surf and FFA
	 * with a hardware workaround.
	 */
	if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 2 &&
			(machine_is_msm8x60_surf() ||
			(machine_is_msm8x60_ffa() &&
			pmic_id_notif_supported)))
		msm_otg_pdata.phy_can_powercollapse = 1;
	msm_device_otg.dev.platform_data = &msm_otg_pdata;
#endif

#ifdef CONFIG_USB_GADGET_MSM_72K
	msm_device_gadget_peripheral.dev.platform_data = &msm_gadget_pdata;
#endif

#ifdef CONFIG_SERIAL_MSM_HS
	msm_uart_dm1_pdata.wakeup_irq = gpio_to_irq(54); /* GSBI6(2) */
	msm_device_uart_dm1.dev.platform_data = &msm_uart_dm1_pdata;
#endif

#ifdef CONFIG_MSM_GSBI9_UART
	msm_device_uart_gsbi9 = msm_add_gsbi9_uart();
	if (IS_ERR(msm_device_uart_gsbi9))
		pr_err("%s(): Failed to create uart gsbi9 device\n",
							__func__);
#endif

#ifdef CONFIG_MSM_BUS_SCALING

	/* RPM calls are only enabled on V2 */
	if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 2) {
		msm_bus_apps_fabric_pdata.rpm_enabled = 1;
		msm_bus_sys_fabric_pdata.rpm_enabled = 1;
		msm_bus_mm_fabric_pdata.rpm_enabled = 1;
		msm_bus_sys_fpb_pdata.rpm_enabled = 1;
		msm_bus_cpss_fpb_pdata.rpm_enabled = 1;
	}

	msm_bus_apps_fabric.dev.platform_data = &msm_bus_apps_fabric_pdata;
	msm_bus_sys_fabric.dev.platform_data = &msm_bus_sys_fabric_pdata;
	msm_bus_mm_fabric.dev.platform_data = &msm_bus_mm_fabric_pdata;
	msm_bus_sys_fpb.dev.platform_data = &msm_bus_sys_fpb_pdata;
	msm_bus_cpss_fpb.dev.platform_data = &msm_bus_cpss_fpb_pdata;
#endif
}

static void __init msm8x60_map_io(void)
{
	msm_shared_ram_phys = MSM_SHARED_RAM_PHYS;
	msm_map_msm8x60_io();

	if (socinfo_init() < 0)
		pr_err("socinfo_init() failed!\n");

#ifdef CONFIG_SEC_DEBUG
	sec_getlog_supply_meminfo(0x40000000, 0x40000000, 0x00, 0x00);
#endif
}

static void __init msm8x60_init_tlmm(void)
{
	if (machine_is_msm8x60_rumi3())
		msm_gpio_install_direct_irq(0, 0, 1);
}

#if (defined(CONFIG_MMC_MSM_SDC1_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC2_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC3_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC4_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC5_SUPPORT))

/* 8x60 has 5 SDCC controllers */
#define MAX_SDCC_CONTROLLER	5

struct msm_sdcc_gpio {
	/* maximum 10 GPIOs per SDCC controller */
	s16 no;
	/* name of this GPIO */
	const char *name;
	bool always_on;
	bool is_enabled;
};

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
static struct msm_sdcc_gpio sdc1_gpio_cfg[] = {
	{159, "sdc1_dat_0"},
	{160, "sdc1_dat_1"},
	{161, "sdc1_dat_2"},
	{162, "sdc1_dat_3"},
#ifdef CONFIG_MMC_MSM_SDC1_8_BIT_SUPPORT
	{163, "sdc1_dat_4"},
	{164, "sdc1_dat_5"},
	{165, "sdc1_dat_6"},
	{166, "sdc1_dat_7"},
#endif
	{167, "sdc1_clk"},
	{168, "sdc1_cmd"}
};
#endif

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
static struct msm_sdcc_gpio sdc2_gpio_cfg[] = {
	{143, "sdc2_dat_0"},
	{144, "sdc2_dat_1", 1},
	{145, "sdc2_dat_2"},
	{146, "sdc2_dat_3"},
#ifdef CONFIG_MMC_MSM_SDC2_8_BIT_SUPPORT
	{147, "sdc2_dat_4"},
	{148, "sdc2_dat_5"},
	{149, "sdc2_dat_6"},
	{150, "sdc2_dat_7"},
#endif
	{151, "sdc2_cmd"},
	{152, "sdc2_clk", 1}
};
#endif

#ifdef CONFIG_MMC_MSM_SDC5_SUPPORT
static struct msm_sdcc_gpio sdc5_gpio_cfg[] = {
	{95, "sdc5_cmd"},
	{96, "sdc5_dat_3"},
	{97, "sdc5_clk", 1},
	{98, "sdc5_dat_2"},
	{99, "sdc5_dat_1", 1},
	{100, "sdc5_dat_0"}
};
#endif

struct msm_sdcc_pad_pull_cfg {
	enum msm_tlmm_pull_tgt pull;
	u32 pull_val;
};

struct msm_sdcc_pad_drv_cfg {
	enum msm_tlmm_hdrive_tgt drv;
	u32 drv_val;
};

#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
static struct msm_sdcc_pad_drv_cfg sdc3_pad_on_drv_cfg[] = {
	{TLMM_HDRV_SDC3_CLK, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC3_CMD, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC3_DATA, GPIO_CFG_8MA}
};

static struct msm_sdcc_pad_pull_cfg sdc3_pad_on_pull_cfg[] = {
	{TLMM_PULL_SDC3_CMD, GPIO_CFG_PULL_UP},
	{TLMM_PULL_SDC3_DATA, GPIO_CFG_PULL_UP}
};

static struct msm_sdcc_pad_drv_cfg sdc3_pad_off_drv_cfg[] = {
	{TLMM_HDRV_SDC3_CLK, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC3_CMD, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC3_DATA, GPIO_CFG_2MA}
};

static struct msm_sdcc_pad_pull_cfg sdc3_pad_off_pull_cfg[] = {
	{TLMM_PULL_SDC3_CMD, GPIO_CFG_PULL_DOWN},
	{TLMM_PULL_SDC3_DATA, GPIO_CFG_PULL_DOWN}
};
#endif

#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
static struct msm_sdcc_pad_drv_cfg sdc4_pad_on_drv_cfg[] = {
	{TLMM_HDRV_SDC4_CLK, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC4_CMD, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC4_DATA, GPIO_CFG_8MA}
};

static struct msm_sdcc_pad_pull_cfg sdc4_pad_on_pull_cfg[] = {
	{TLMM_PULL_SDC4_CMD, GPIO_CFG_PULL_UP},
	{TLMM_PULL_SDC4_DATA, GPIO_CFG_PULL_UP}
};

static struct msm_sdcc_pad_drv_cfg sdc4_pad_off_drv_cfg[] = {
	{TLMM_HDRV_SDC4_CLK, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC4_CMD, GPIO_CFG_2MA},
	{TLMM_HDRV_SDC4_DATA, GPIO_CFG_2MA}
};

static struct msm_sdcc_pad_pull_cfg sdc4_pad_off_pull_cfg[] = {
	{TLMM_PULL_SDC4_CMD, GPIO_CFG_PULL_DOWN},
	{TLMM_PULL_SDC4_DATA, GPIO_CFG_PULL_DOWN}
};
#endif

struct msm_sdcc_pin_cfg {
	/*
	 * = 1 if controller pins are using gpios
	 * = 0 if controller has dedicated MSM pins
	 */
	u8 is_gpio;
	u8 cfg_sts;
	u8 gpio_data_size;
	struct msm_sdcc_gpio *gpio_data;
	struct msm_sdcc_pad_drv_cfg *pad_drv_on_data;
	struct msm_sdcc_pad_drv_cfg *pad_drv_off_data;
	struct msm_sdcc_pad_pull_cfg *pad_pull_on_data;
	struct msm_sdcc_pad_pull_cfg *pad_pull_off_data;
	u8 pad_drv_data_size;
	u8 pad_pull_data_size;
	u8 sdio_lpm_gpio_cfg;
};


static struct msm_sdcc_pin_cfg sdcc_pin_cfg_data[MAX_SDCC_CONTROLLER] = {
#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	[0] = {
		.is_gpio = 1,
		.gpio_data_size = ARRAY_SIZE(sdc1_gpio_cfg),
		.gpio_data = sdc1_gpio_cfg
	},
#endif
#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
	[1] = {
		.is_gpio = 1,
		.gpio_data_size = ARRAY_SIZE(sdc2_gpio_cfg),
		.gpio_data = sdc2_gpio_cfg
	},
#endif
#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
	[2] = {
		.is_gpio = 0,
		.pad_drv_on_data = sdc3_pad_on_drv_cfg,
		.pad_drv_off_data = sdc3_pad_off_drv_cfg,
		.pad_pull_on_data = sdc3_pad_on_pull_cfg,
		.pad_pull_off_data = sdc3_pad_off_pull_cfg,
		.pad_drv_data_size = ARRAY_SIZE(sdc3_pad_on_drv_cfg),
		.pad_pull_data_size = ARRAY_SIZE(sdc3_pad_on_pull_cfg)
	},
#endif
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	[3] = {
		.is_gpio = 0,
		.pad_drv_on_data = sdc4_pad_on_drv_cfg,
		.pad_drv_off_data = sdc4_pad_off_drv_cfg,
		.pad_pull_on_data = sdc4_pad_on_pull_cfg,
		.pad_pull_off_data = sdc4_pad_off_pull_cfg,
		.pad_drv_data_size = ARRAY_SIZE(sdc4_pad_on_drv_cfg),
		.pad_pull_data_size = ARRAY_SIZE(sdc4_pad_on_pull_cfg)
	},
#endif
#ifdef CONFIG_MMC_MSM_SDC5_SUPPORT
	[4] = {
		.is_gpio = 1,
		.gpio_data_size = ARRAY_SIZE(sdc5_gpio_cfg),
		.gpio_data = sdc5_gpio_cfg
	}
#endif
};

static int msm_sdcc_setup_gpio(int dev_id, unsigned int enable)
{
	int rc = 0;
	struct msm_sdcc_pin_cfg *curr;
	int n;

	curr = &sdcc_pin_cfg_data[dev_id - 1];
	if (!curr->gpio_data)
		goto out;

	for (n = 0; n < curr->gpio_data_size; n++) {
		if (enable) {
		
			if (curr->gpio_data[n].always_on &&
				curr->gpio_data[n].is_enabled)
				continue;
			pr_debug("%s: enable: %s\n", __func__,
					curr->gpio_data[n].name);
			rc = gpio_request(curr->gpio_data[n].no,
				curr->gpio_data[n].name);
			if (rc) {
				pr_err("%s: gpio_request(%d, %s)"
					"failed", __func__,
					curr->gpio_data[n].no,
					curr->gpio_data[n].name);
				goto free_gpios;
			}
			/* set direction as output for all GPIOs */
			rc = gpio_direction_output(
				curr->gpio_data[n].no, 1);
			if (rc) {
				pr_err("%s: gpio_direction_output"
					"(%d, 1) failed\n", __func__,
					curr->gpio_data[n].no);
				goto free_gpios;
			}
			curr->gpio_data[n].is_enabled = 1;
		} else {
			/*
			 * now free this GPIO which will put GPIO
			 * in low power mode and will also put GPIO
			 * in input mode
			 */
 			if (curr->gpio_data[n].always_on)
				continue;
			pr_debug("%s: disable: %s\n", __func__,
					curr->gpio_data[n].name);
			gpio_free(curr->gpio_data[n].no);
			curr->gpio_data[n].is_enabled = 0;
		}
	}
	curr->cfg_sts = enable;
	goto out;

free_gpios:
	for (; n >= 0; n--)
		gpio_free(curr->gpio_data[n].no);
out:
	return rc;
}

int msm_sdcc_setup_pad(int dev_id, unsigned int enable)
{
	int rc = 0;
	struct msm_sdcc_pin_cfg *curr;
	int n;

	curr = &sdcc_pin_cfg_data[dev_id - 1];
	if (!curr->pad_drv_on_data || !curr->pad_pull_on_data)
		goto out;

	if (enable) {
		/*
		 * set up the normal driver strength and
		 * pull config for pads
		 */
		for (n = 0; n < curr->pad_drv_data_size; n++) {
			if (curr->sdio_lpm_gpio_cfg) {
				if (curr->pad_drv_on_data[n].drv ==
						TLMM_HDRV_SDC4_DATA)
					continue;
			}
			msm_tlmm_set_hdrive(curr->pad_drv_on_data[n].drv,
				curr->pad_drv_on_data[n].drv_val);
		}
		for (n = 0; n < curr->pad_pull_data_size; n++) {
			if (curr->sdio_lpm_gpio_cfg) {
				if (curr->pad_pull_on_data[n].pull ==
						TLMM_PULL_SDC4_DATA)
					continue;
			}
			msm_tlmm_set_pull(curr->pad_pull_on_data[n].pull,
				curr->pad_pull_on_data[n].pull_val);
		}
	} else {
		/* set the low power config for pads */
		for (n = 0; n < curr->pad_drv_data_size; n++) {
			if (curr->sdio_lpm_gpio_cfg) {
				if (curr->pad_drv_off_data[n].drv ==
						TLMM_HDRV_SDC4_DATA)
					continue;
			}
			msm_tlmm_set_hdrive(
				curr->pad_drv_off_data[n].drv,
				curr->pad_drv_off_data[n].drv_val);
		}
		for (n = 0; n < curr->pad_pull_data_size; n++) {
			if (curr->sdio_lpm_gpio_cfg) {
				if (curr->pad_pull_off_data[n].pull ==
						TLMM_PULL_SDC4_DATA)
					continue;
			}
			msm_tlmm_set_pull(
				curr->pad_pull_off_data[n].pull,
				curr->pad_pull_off_data[n].pull_val);
		}
	}
	curr->cfg_sts = enable;
out:
	return rc;
}

struct sdcc_reg {
	/* VDD/VCC/VCCQ regulator name on PMIC8058/PMIC8089*/
	const char *reg_name;
	/*
	 * is set voltage supported for this regulator?
	 * 0 = not supported, 1 = supported
	 */
	unsigned char set_voltage_sup;
	/* voltage level to be set */
	unsigned int level;
	/* VDD/VCC/VCCQ voltage regulator handle */
	struct regulator *reg;
	/* is this regulator enabled? */
	bool enabled;
	/* is this regulator needs to be always on? */
	bool always_on;
	/* is operating power mode setting required for this regulator? */
	bool op_pwr_mode_sup;
	/* Load values for low power and high power mode */
	unsigned int lpm_uA;
	unsigned int hpm_uA;
};
/* all SDCC controllers requires VDD/VCC voltage */
static struct sdcc_reg sdcc_vdd_reg_data[MAX_SDCC_CONTROLLER];
/* only SDCC1 requires VCCQ voltage */
static struct sdcc_reg sdcc_vccq_reg_data[1];
/* all SDCC controllers may require voting for VDD PAD voltage */
static struct sdcc_reg sdcc_vddp_reg_data[MAX_SDCC_CONTROLLER];

struct sdcc_reg_data {
	struct sdcc_reg *vdd_data; /* keeps VDD/VCC regulator info */
	struct sdcc_reg *vccq_data; /* keeps VCCQ regulator info */
	struct sdcc_reg *vddp_data; /* keeps VDD Pad regulator info */
	unsigned char sts; /* regulator enable/disable status */
};
/* msm8x60 has 5 SDCC controllers */
static struct sdcc_reg_data sdcc_vreg_data[MAX_SDCC_CONTROLLER];

static int msm_sdcc_vreg_init_reg(struct sdcc_reg *vreg)
{
	int rc = 0;

	/* Get the regulator handle */
	vreg->reg = regulator_get(NULL, vreg->reg_name);
	if (IS_ERR(vreg->reg)) {
		rc = PTR_ERR(vreg->reg);
		pr_err("%s: regulator_get(%s) failed. rc=%d\n",
			__func__, vreg->reg_name, rc);
		goto out;
	}

	/* Set the voltage level if required */
	if (vreg->set_voltage_sup) {
		rc = regulator_set_voltage(vreg->reg, vreg->level,
					vreg->level);
		if (rc) {
			pr_err("%s: regulator_set_voltage(%s) failed rc=%d\n",
				__func__, vreg->reg_name, rc);
			goto vreg_put;
		}
	}
	goto out;

vreg_put:
	regulator_put(vreg->reg);
out:
	return rc;
}

static inline void msm_sdcc_vreg_deinit_reg(struct sdcc_reg *vreg)
{
	regulator_put(vreg->reg);
}

/* this init function should be called only once for each SDCC */
static int msm_sdcc_vreg_init(int dev_id, unsigned char init)
{
	int rc = 0;
	struct sdcc_reg *curr_vdd_reg, *curr_vccq_reg, *curr_vddp_reg;
	struct sdcc_reg_data *curr;

	curr = &sdcc_vreg_data[dev_id - 1];
	curr_vdd_reg = curr->vdd_data;
	curr_vccq_reg = curr->vccq_data;
	curr_vddp_reg = curr->vddp_data;

	if (init) {
		/*
		 * get the regulator handle from voltage regulator framework
		 * and then try to set the voltage level for the regulator
		 */
		if (curr_vdd_reg) {
			rc = msm_sdcc_vreg_init_reg(curr_vdd_reg);
			if (rc)
				goto out;
		}
		if (curr_vccq_reg) {
			rc = msm_sdcc_vreg_init_reg(curr_vccq_reg);
			if (rc)
				goto vdd_reg_deinit;
		}
		if (curr_vddp_reg) {
			rc = msm_sdcc_vreg_init_reg(curr_vddp_reg);
			if (rc)
				goto vccq_reg_deinit;
		}
		goto out;
	} else
		/* deregister with all regulators from regulator framework */
		goto vddp_reg_deinit;

vddp_reg_deinit:
	if (curr_vddp_reg)
		msm_sdcc_vreg_deinit_reg(curr_vddp_reg);
vccq_reg_deinit:
	if (curr_vccq_reg)
		msm_sdcc_vreg_deinit_reg(curr_vccq_reg);
vdd_reg_deinit:
	if (curr_vdd_reg)
		msm_sdcc_vreg_deinit_reg(curr_vdd_reg);
out:
	return rc;
}

static int msm_sdcc_vreg_enable(struct sdcc_reg *vreg)
{
	int rc = 0;

	if (!vreg->enabled) {
		rc = regulator_enable(vreg->reg);
		if (rc) {
			pr_err("%s: regulator_enable(%s) failed. rc=%d\n",
				__func__, vreg->reg_name, rc);
			goto out;
		}
		vreg->enabled = 1;
	}

	/* Put always_on regulator in HPM (high power mode) */
	if (vreg->always_on && vreg->op_pwr_mode_sup) {
		rc = regulator_set_optimum_mode(vreg->reg, vreg->hpm_uA);
		if (rc < 0) {
			pr_err("%s: reg=%s: HPM setting failed"
				" hpm_uA=%d, rc=%d\n",
				__func__, vreg->reg_name,
				vreg->hpm_uA, rc);
			goto vreg_disable;
		}
		rc = 0;
	}
	goto out;

vreg_disable:
	regulator_disable(vreg->reg);
	vreg->enabled = 0;
out:
	return rc;
}

static int msm_sdcc_vreg_disable(struct sdcc_reg *vreg)
{
	int rc = 0;

	/* Never disable always_on regulator */
	if (!vreg->always_on) {
		rc = regulator_disable(vreg->reg);
		if (rc) {
			pr_err("%s: regulator_disable(%s) failed. rc=%d\n",
				__func__, vreg->reg_name, rc);
			goto out;
		}
		vreg->enabled = 0;
	}

	/* Put always_on regulator in LPM (low power mode) */
	if (vreg->always_on && vreg->op_pwr_mode_sup) {
		rc = regulator_set_optimum_mode(vreg->reg, vreg->lpm_uA);
		if (rc < 0) {
			pr_err("%s: reg=%s: LPM setting failed"
				" lpm_uA=%d, rc=%d\n",
				__func__,
				vreg->reg_name,
				vreg->lpm_uA, rc);
			goto out;
		}
		rc = 0;
	}

out:
	return rc;
}

static int msm_sdcc_setup_vreg(int dev_id, unsigned char enable)
{
	int rc = 0;
	struct sdcc_reg *curr_vdd_reg, *curr_vccq_reg, *curr_vddp_reg;
	struct sdcc_reg_data *curr;

	curr = &sdcc_vreg_data[dev_id - 1];
	curr_vdd_reg = curr->vdd_data;
	curr_vccq_reg = curr->vccq_data;
	curr_vddp_reg = curr->vddp_data;

	/* check if regulators are initialized or not? */
	if ((curr_vdd_reg && !curr_vdd_reg->reg) ||
		(curr_vccq_reg && !curr_vccq_reg->reg) ||
		(curr_vddp_reg && !curr_vddp_reg->reg)) {
		/* initialize voltage regulators required for this SDCC */
		rc = msm_sdcc_vreg_init(dev_id, 1);
		if (rc) {
			pr_err("%s: regulator init failed = %d\n",
				__func__, rc);
			goto out;
		}
	}

	if (curr->sts == enable)
		goto out;

	if (curr_vdd_reg) {
		if (enable)
			rc = msm_sdcc_vreg_enable(curr_vdd_reg);
		else
			rc = msm_sdcc_vreg_disable(curr_vdd_reg);
		if (rc)
			goto out;
	}

	if (curr_vccq_reg) {
		if (enable)
			rc = msm_sdcc_vreg_enable(curr_vccq_reg);
		else
			rc = msm_sdcc_vreg_disable(curr_vccq_reg);
		if (rc)
			goto out;
	}

	if (curr_vddp_reg) {
		if (enable)
			rc = msm_sdcc_vreg_enable(curr_vddp_reg);
		else
			rc = msm_sdcc_vreg_disable(curr_vddp_reg);
		if (rc)
			goto out;
	}
	curr->sts = enable;

out:
	return rc;
}

static u32 msm_sdcc_setup_power(struct device *dv, unsigned int vdd)
{
	u32 rc_pin_cfg = 0;
	u32 rc_vreg_cfg = 0;
	u32 rc = 0;
	struct platform_device *pdev;
	struct msm_sdcc_pin_cfg *curr_pin_cfg;

	pdev = container_of(dv, struct platform_device, dev);

	/* setup gpio/pad */
	curr_pin_cfg = &sdcc_pin_cfg_data[pdev->id - 1];
	if (curr_pin_cfg->cfg_sts == !!vdd)
		goto setup_vreg;

	if (curr_pin_cfg->is_gpio)
		rc_pin_cfg = msm_sdcc_setup_gpio(pdev->id, !!vdd);
	else
		rc_pin_cfg = msm_sdcc_setup_pad(pdev->id, !!vdd);

setup_vreg:
	/* setup voltage regulators */
	rc_vreg_cfg = msm_sdcc_setup_vreg(pdev->id, !!vdd);

	if (rc_pin_cfg || rc_vreg_cfg)
		rc = rc_pin_cfg ? rc_pin_cfg : rc_vreg_cfg;

	return rc;
}

#if (defined(CONFIG_MMC_MSM_SDC2_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC5_SUPPORT))
static void msm_sdcc_sdio_lpm_gpio(struct device *dv, unsigned int active)
{
	struct msm_sdcc_pin_cfg *curr_pin_cfg;
	struct platform_device *pdev;

	pdev = container_of(dv, struct platform_device, dev);
	/* setup gpio/pad */
	curr_pin_cfg = &sdcc_pin_cfg_data[pdev->id - 1];

	if (curr_pin_cfg->cfg_sts == active)
		return;

	curr_pin_cfg->sdio_lpm_gpio_cfg = 1;
	if (curr_pin_cfg->is_gpio)
		msm_sdcc_setup_gpio(pdev->id, active);
	else
		msm_sdcc_setup_pad(pdev->id, active);
	curr_pin_cfg->sdio_lpm_gpio_cfg = 0;
}
#endif

#ifdef CONFIG_MMC_MSM_SDC5_SUPPORT
int sdc5_register_status_notify(void (*callback)(int, void *),
	void *dev_id)
{
	sdc5_status_notify_cb = callback;
	sdc5_status_notify_cb_devid = dev_id;
	return 0;
}
#endif

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
int sdc2_register_status_notify(void (*callback)(int, void *),
	void *dev_id)
{
	sdc2_status_notify_cb = callback;
	sdc2_status_notify_cb_devid = dev_id;
	return 0;
}
#endif

/* Interrupt handler for SDC2 and SDC5 detection
 * This function uses dual-edge interrputs settings in order
 * to get SDIO detection when the GPIO is rising and SDIO removal
 * when the GPIO is falling */

extern int mdm_bootloader_done;
extern int get_charm_ready(void);
static irqreturn_t msm8x60_multi_sdio_slot_status_irq(int irq, void *dev_id)
{
	int status;

	status = gpio_get_value(MDM2AP_SYNC);
	pr_info("%s: MDM2AP_SYNC Status = %d\n",
		 __func__, status);

	if (( status == 0 ) && ( mdm_bootloader_done && !get_charm_ready() )){
		if ((gpio_get_value(MDM2AP_STATUS) == 0)) {			
			pr_info("%s : MDM2AP_SYNC went low after mdm bootloader done\n", __func__);
			pr_info("%s : but we did not get event normal boot done. \n", __func__);
			subsystem_restart("external_modem");
		}
	}

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
	if (sdc2_status_notify_cb) {
		pr_info("%s: calling sdc2_status_notify_cb\n", __func__);
		sdc2_status_notify_cb(status,
			sdc2_status_notify_cb_devid);
	}
#endif

#ifdef CONFIG_MMC_MSM_SDC5_SUPPORT
	if (sdc5_status_notify_cb) {
		pr_info("%s: calling sdc5_status_notify_cb\n", __func__);
		sdc5_status_notify_cb(status,
			sdc5_status_notify_cb_devid);
	}
#endif
	return IRQ_HANDLED;
}

static int msm8x60_multi_sdio_init(void)
{
	int ret, irq_num;

	if(charging_mode_from_boot == 1)
		return 0;

	ret = msm_gpiomux_get(MDM2AP_SYNC);
	if (ret) {
		pr_err("%s:Failed to request GPIO %d, ret=%d\n",
					__func__, MDM2AP_SYNC, ret);
		return ret;
	}

	irq_num = gpio_to_irq(MDM2AP_SYNC);

	ret = request_irq(irq_num,
		msm8x60_multi_sdio_slot_status_irq,
		IRQ_TYPE_EDGE_BOTH,
		"sdio_multidetection", NULL);
	
	if (ret) {
		pr_err("%s:Failed to request irq, ret=%d\n",
					__func__, ret);
		return ret;
	}

	return ret;
}

#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
#ifdef CONFIG_MMC_MSM_CARD_HW_DETECTION
static unsigned int msm8x60_sdcc_slot_status(struct device *dev)
{
	int status;

	status = gpio_request(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SDC3_DET - 1)
				, "SD_HW_Detect");
	if (status) {
		pr_err("%s:Failed to request GPIO %d\n", __func__,
				PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SDC3_DET - 1));
	} else {
		status = gpio_direction_input(
				PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SDC3_DET - 1));
		if (!status)
			status = !(gpio_get_value_cansleep(
				PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SDC3_DET - 1)));
		gpio_free(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SDC3_DET - 1));
	}
	return (unsigned int) status;
}
#endif
#endif

#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
static int msm_sdcc_cfg_mpm_sdiowakeup(struct device *dev, unsigned mode)
{
	struct platform_device *pdev;
	enum msm_mpm_pin pin;
	int ret = 0;

	pdev = container_of(dev, struct platform_device, dev);

	/* Only SDCC4 slot connected to WLAN chip has wakeup capability */
	if (pdev->id == 4)
		pin = MSM_MPM_PIN_SDC4_DAT1;
	else
		return -EINVAL;

	switch (mode) {
	case SDC_DAT1_DISABLE:
		ret = msm_mpm_enable_pin(pin, 0);
		break;
	case SDC_DAT1_ENABLE:
		ret = msm_mpm_set_pin_type(pin, IRQ_TYPE_LEVEL_LOW);
		ret = msm_mpm_enable_pin(pin, 1);
		break;
	case SDC_DAT1_ENWAKE:
		ret = msm_mpm_set_pin_wake(pin, 1);
		break;
	case SDC_DAT1_DISWAKE:
		ret = msm_mpm_set_pin_wake(pin, 0);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
#endif
#endif

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
static struct mmc_platform_data msm8x60_sdc1_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.translate_vdd  = msm_sdcc_setup_power,
#ifdef CONFIG_MMC_MSM_SDC1_8_BIT_SUPPORT
	.mmc_bus_width  = MMC_CAP_8_BIT_DATA,
#else
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
#endif
	.msmsdcc_fmin	= 400000,
	.msmsdcc_fmid	= 24000000,
	.msmsdcc_fmax	= 48000000,
	.nonremovable	= 1,
	.pclk_src_dfab	= 1,
#ifdef CONFIG_MMC_MSM_SDC1_DUMMY52_REQUIRED
	.dummy52_required = 1,
#endif
};
#endif

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
static struct mmc_platform_data msm8x60_sdc2_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_165_195,
	.translate_vdd  = msm_sdcc_setup_power,
	.sdio_lpm_gpio_setup = msm_sdcc_sdio_lpm_gpio,
	.mmc_bus_width  = MMC_CAP_8_BIT_DATA,
	.msmsdcc_fmin	= 400000,
	.msmsdcc_fmid	= 24000000,
	.msmsdcc_fmax	= 48000000,
	.nonremovable	= 0,
	.pclk_src_dfab  = 1,
	.register_status_notify = sdc2_register_status_notify,
#ifdef CONFIG_MMC_MSM_SDC2_DUMMY52_REQUIRED
	.dummy52_required = 1,
#endif
#ifdef CONFIG_MSM_SDIO_AL
	.is_sdio_al_client = 1,
#endif
};
#endif

#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
static struct mmc_platform_data msm8x60_sdc3_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.translate_vdd  = msm_sdcc_setup_power,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
#ifdef CONFIG_MMC_MSM_CARD_HW_DETECTION
	.status      = msm8x60_sdcc_slot_status,
	.status_irq  = PM8058_GPIO_IRQ(PM8058_IRQ_BASE,
				       PMIC_GPIO_SDC3_DET - 1),
	.irq_flags   = IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
#endif
	.msmsdcc_fmin	= 400000,
	.msmsdcc_fmid	= 24000000,
	.msmsdcc_fmax	= 48000000,
	.nonremovable	= 0,
	.pclk_src_dfab  = 1,
#ifdef CONFIG_MMC_MSM_SDC3_DUMMY52_REQUIRED
	.dummy52_required = 1,
#endif
};
#endif

#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
static struct mmc_platform_data msm8x60_sdc4_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.translate_vdd  = msm_sdcc_setup_power,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.msmsdcc_fmin	= 400000,
	.msmsdcc_fmid	= 24000000,
	.msmsdcc_fmax	= 48000000,
	.nonremovable	= 0,
	.pclk_src_dfab  = 1,
	.cfg_mpm_sdiowakeup = msm_sdcc_cfg_mpm_sdiowakeup,
#ifdef CONFIG_MMC_MSM_SDC4_DUMMY52_REQUIRED
	.dummy52_required = 1,
#endif
};
#endif

#ifdef CONFIG_MMC_MSM_SDC5_SUPPORT
static struct mmc_platform_data msm8x60_sdc5_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29 | MMC_VDD_165_195,
	.translate_vdd  = msm_sdcc_setup_power,
	.sdio_lpm_gpio_setup = msm_sdcc_sdio_lpm_gpio,	
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.msmsdcc_fmin	= 400000,
	.msmsdcc_fmid	= 24000000,
	.msmsdcc_fmax	= 48000000,
	.nonremovable	= 0,
	.pclk_src_dfab  = 1,
	.register_status_notify = sdc5_register_status_notify,
#ifdef CONFIG_MMC_MSM_SDC5_DUMMY52_REQUIRED
	.dummy52_required = 1,
#endif
#ifdef CONFIG_MSM_SDIO_AL
	.is_sdio_al_client = 1,
#endif
};
#endif

static void __init msm8x60_init_mmc(void)
{
#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	/* SDCC1 : eMMC card connected */
	sdcc_vreg_data[0].vdd_data = &sdcc_vdd_reg_data[0];
	sdcc_vreg_data[0].vdd_data->reg_name = "8901_l5";
	sdcc_vreg_data[0].vdd_data->set_voltage_sup = 1;
#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_KT)	
	sdcc_vreg_data[0].vdd_data->level = 2950000;
#else
	sdcc_vreg_data[0].vdd_data->level = 2850000;
#endif
	sdcc_vreg_data[0].vdd_data->always_on = 1;
	sdcc_vreg_data[0].vdd_data->op_pwr_mode_sup = 1;
	sdcc_vreg_data[0].vdd_data->lpm_uA = 9000;
	sdcc_vreg_data[0].vdd_data->hpm_uA = 200000;

	sdcc_vreg_data[0].vccq_data = &sdcc_vccq_reg_data[0];
	sdcc_vreg_data[0].vccq_data->reg_name = "8901_lvs0";
	sdcc_vreg_data[0].vccq_data->set_voltage_sup = 0;
	sdcc_vreg_data[0].vccq_data->always_on = 1;

	msm_add_sdcc(1, &msm8x60_sdc1_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
	/*
	 * MDM SDIO client is connected to SDC2 on charm SURF/FFA
	 * and no card is connected on 8660 SURF/FFA/FLUID.
	 */
	sdcc_vreg_data[1].vdd_data = &sdcc_vdd_reg_data[1];
	sdcc_vreg_data[1].vdd_data->reg_name = "8058_s3";
	sdcc_vreg_data[1].vdd_data->set_voltage_sup = 1;
	sdcc_vreg_data[1].vdd_data->level = 1800000;

	sdcc_vreg_data[1].vccq_data = NULL;

	if (machine_is_msm8x60_fusion())
		msm8x60_sdc2_data.msmsdcc_fmax = 24000000;

	if(charging_mode_from_boot == 1) {
		pr_info("%s : skip sdcc2 config for lpm boot\n", __func__);
	} else { /* charging_mode_from_boot != 1 */
#ifdef CONFIG_MMC_MSM_SDIO_SUPPORT
		msm8x60_sdc2_data.sdiowakeup_irq = gpio_to_irq(144);
		msm_sdcc_setup_gpio(2, 1);
#endif
		msm_add_sdcc(2, &msm8x60_sdc2_data);
#endif
	} /* charging_mode_from_boot != 1 */

#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
	/* SDCC3 : External card slot connected */
	sdcc_vreg_data[2].vdd_data = &sdcc_vdd_reg_data[2];
	sdcc_vreg_data[2].vdd_data->reg_name = "8058_l14";
	sdcc_vreg_data[2].vdd_data->set_voltage_sup = 1;
	sdcc_vreg_data[2].vdd_data->level = 2850000;
	sdcc_vreg_data[2].vdd_data->always_on = 1;
	sdcc_vreg_data[2].vdd_data->op_pwr_mode_sup = 1;
	sdcc_vreg_data[2].vdd_data->lpm_uA = 9000;
	sdcc_vreg_data[2].vdd_data->hpm_uA = 200000;

	sdcc_vreg_data[2].vccq_data = NULL;

	sdcc_vreg_data[2].vddp_data = &sdcc_vddp_reg_data[2];
	sdcc_vreg_data[2].vddp_data->reg_name = "8058_l5";
	sdcc_vreg_data[2].vddp_data->set_voltage_sup = 1;
	sdcc_vreg_data[2].vddp_data->level = 2850000;
	sdcc_vreg_data[2].vddp_data->always_on = 1;
	sdcc_vreg_data[2].vddp_data->op_pwr_mode_sup = 1;
	/* Sleep current required is ~300 uA. But min. RPM
	 * vote can be in terms of mA (min. 1 mA).
	 * So let's vote for 2 mA during sleep.
	 */
	sdcc_vreg_data[2].vddp_data->lpm_uA = 2000;
	/* Max. Active current required is 16 mA */
	sdcc_vreg_data[2].vddp_data->hpm_uA = 16000;

	if (machine_is_msm8x60_fluid())
		msm8x60_sdc3_data.wpswitch = NULL;
	msm_add_sdcc(3, &msm8x60_sdc3_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	/* SDCC4 : WLAN WCN1314 chip is connected */
	sdcc_vreg_data[3].vdd_data = &sdcc_vdd_reg_data[3];
	sdcc_vreg_data[3].vdd_data->reg_name = "8058_s3";
	sdcc_vreg_data[3].vdd_data->set_voltage_sup = 1;
	sdcc_vreg_data[3].vdd_data->level = 1800000;

	sdcc_vreg_data[3].vccq_data = NULL;

	msm_add_sdcc(4, &msm8x60_sdc4_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC5_SUPPORT
	/*
	 * MDM SDIO client is connected to SDC5 on charm SURF/FFA
	 * and no card is connected on 8660 SURF/FFA/FLUID.
	 */
	sdcc_vreg_data[4].vdd_data = &sdcc_vdd_reg_data[4];
	sdcc_vreg_data[4].vdd_data->reg_name = "8058_s3";
	sdcc_vreg_data[4].vdd_data->set_voltage_sup = 1;
	sdcc_vreg_data[4].vdd_data->level = 1800000;

	sdcc_vreg_data[4].vccq_data = NULL;

	if (machine_is_msm8x60_fusion())
		msm8x60_sdc5_data.msmsdcc_fmax = 24000000;

	if(charging_mode_from_boot == 1) {
		pr_info("%s : skip sdcc5 config for lpm boot\n", __func__);
	} else { /* charging_mode_from_boot != 1 */
#ifdef CONFIG_MMC_MSM_SDIO_SUPPORT
		msm8x60_sdc5_data.sdiowakeup_irq = gpio_to_irq(99);
		msm_sdcc_setup_gpio(5, 1);
#endif
		msm_add_sdcc(5, &msm8x60_sdc5_data);
#endif
	} /* charging_mode_from_boot != 1 */
}

static int mipi_dsi_panel_power(int on);
#define LCDC_NUM_GPIO 28
#define LCDC_GPIO_START 0

static void lcdc_samsung_panel_power(int on)
{
	int n, ret = 0;

	for (n = 0; n < LCDC_NUM_GPIO; n++) {
		if (on) {
			ret = gpio_request(LCDC_GPIO_START + n, "LCDC_GPIO");
			if (unlikely(ret)) {
				pr_err("%s not able to get gpio\n", __func__);
				break;
			}
		} else
			gpio_free(LCDC_GPIO_START + n);
	}

	if (ret) {
		for (n--; n >= 0; n--)
			gpio_free(LCDC_GPIO_START + n);
	}

	mipi_dsi_panel_power(0); /* set 8058_ldo0 to LPM */
}

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
#define _GET_REGULATOR(var, name) do {				\
	var = regulator_get(NULL, name);			\
	if (IS_ERR(var)) {					\
		pr_err("'%s' regulator not found, rc=%ld\n",	\
			name, IS_ERR(var));			\
		var = NULL;					\
		return -ENODEV;					\
	}							\
} while (0)

static int hdmi_enable_5v(int on)
{
	static struct regulator *reg_8901_hdmi_mvs;	/* HDMI_5V */
	static struct regulator *reg_8901_mpp0;		/* External 5V */
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (!reg_8901_hdmi_mvs)
		_GET_REGULATOR(reg_8901_hdmi_mvs, "8901_hdmi_mvs");
	if (!reg_8901_mpp0)
		_GET_REGULATOR(reg_8901_mpp0, "8901_mpp0");

	if (on) {
		rc = regulator_enable(reg_8901_mpp0);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"reg_8901_mpp0", rc);
			return rc;
		}
		rc = regulator_enable(reg_8901_hdmi_mvs);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"8901_hdmi_mvs", rc);
			return rc;
		}
		pr_info("%s(on): success\n", __func__);
	} else {
		rc = regulator_disable(reg_8901_hdmi_mvs);
		if (rc)
			pr_warning("'%s' regulator disable failed, rc=%d\n",
				"8901_hdmi_mvs", rc);
		rc = regulator_disable(reg_8901_mpp0);
		if (rc)
			pr_warning("'%s' regulator disable failed, rc=%d\n",
				"reg_8901_mpp0", rc);
		pr_info("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;
}

static int hdmi_core_power(int on, int show)
{
	static struct regulator *reg_8058_l16;		/* VDD_HDMI */
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (!reg_8058_l16)
		_GET_REGULATOR(reg_8058_l16, "8058_l16");

	if (on) {
		rc = regulator_set_voltage(reg_8058_l16, 1800000, 1800000);
		if (!rc)
			rc = regulator_enable(reg_8058_l16);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"8058_l16", rc);
			return rc;
		}
		gpio_request(170, "HDMI_DDC_CLK");
		gpio_request(171, "HDMI_DDC_DATA");
		gpio_request(172, "HDMI_HPD");
		pr_info("%s(on): success\n", __func__);
	} else {
		gpio_free(170);
		gpio_free(171);
		gpio_free(172);
		rc = regulator_disable(reg_8058_l16);
		if (rc)
			pr_warning("'%s' regulator disable failed, rc=%d\n",
				"8058_l16", rc);
		pr_info("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;
}

static int hdmi_cec_power(int on)
{
	static struct regulator *reg_8901_l3;		/* HDMI_CEC */
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

	if (!reg_8901_l3)
		_GET_REGULATOR(reg_8901_l3, "8901_l3");

	if (on) {
		rc = regulator_set_voltage(reg_8901_l3, 3300000, 3300000);
		if (!rc)
			rc = regulator_enable(reg_8901_l3);
		if (rc) {
			pr_err("'%s' regulator enable failed, rc=%d\n",
				"8901_l3", rc);
			return rc;
		}
		rc = gpio_request(169, "HDMI_CEC_VAR");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_CEC_VAR", 169, rc);
			goto error;
		}
		pr_info("%s(on): success\n", __func__);
	} else {
		gpio_free(169);
		rc = regulator_disable(reg_8901_l3);
		if (rc)
			pr_warning("'%s' regulator disable failed, rc=%d\n",
				"8901_l3", rc);
		pr_info("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;
error:
	regulator_disable(reg_8901_l3);
	return rc;
}

#undef _GET_REGULATOR

#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */

#ifdef CONFIG_SENSORS_AK8975_I957
struct regulator {
	struct device *dev;
	struct list_head list;
	int uA_load;
	int min_uV;
	int max_uV;
	int enabled;
	char *supply_name;
	struct device_attribute dev_attr;
	struct regulator_dev *rdev;
};

static int __init magnetic_device_init(void)
{
	int err = 0;	

	k3dh_akm_l11 = regulator_get(NULL, "8058_l11");
	if (IS_ERR(k3dh_akm_l11)) {
		return PTR_ERR(k3dh_akm_l11);
	}

	err = regulator_set_voltage(k3dh_akm_l11, 2850000, 2850000);
	if (err) {
		pr_err("%s: unable to set the voltage for regulator"
				"pm8058_l11\n", __func__);
		regulator_put(k3dh_akm_l11);
	}

#if defined(CONFIG_KOR_OPERATOR_LGU)
	k3dh_akm_lvs3 = regulator_get(NULL, "8901_lvs3");
	if (IS_ERR(k3dh_akm_lvs3)) {
		return PTR_ERR(k3dh_akm_lvs3);
	}
#elif defined(CONFIG_KOR_OPERATOR_SKT)
	if(system_rev < 0x07){ 
		/* rev 0.0, 0.1, 0.2,  VSENSOR_1.8V from VREG_L20A */	
		k3dh_akm_l20 = regulator_get(NULL, "8058_l20");
		if (IS_ERR(k3dh_akm_l20)) {
			return PTR_ERR(k3dh_akm_l20);
		}

		err = regulator_set_voltage(k3dh_akm_l20, 1800000, 1800000);
		if (err) {
			pr_err("%s: unable to set the voltage for regulator"
					"pm8058_l20\n", __func__);
			regulator_put(k3dh_akm_l20);
			return err;
		}
	} else if(system_rev >= 0x07) {
		printk("[K3DH]magnetic_device_init REV07\n");	
		/* rev 0.0, 0.1, 0.2,  VSENSOR_1.8V from VREG_L20A */	
		k3dh_akm_l20 = regulator_get(NULL, "8058_l20");
		if (IS_ERR(k3dh_akm_l20)) {
			return PTR_ERR(k3dh_akm_l20);
		}

		err = regulator_set_voltage(k3dh_akm_l20, 2850000, 2850000);
		if (err) {
			pr_err("%s: unable to set the voltage for regulator"
					"pm8058_l20\n", __func__);
			regulator_put(k3dh_akm_l20);
			return err;
		}
		
		err = regulator_enable(k3dh_akm_l20);
		if (err) {
			pr_err("%s: unable to enable regulator"
					"pm8058_l20\n", __func__);
			regulator_put(k3dh_akm_l20);
			return err;
		}
		
	} else {
		printk("[K3DH] unknown rev=%d\n", system_rev );
	}
#else
	if(system_rev < 0x06){ 
		/* rev < 0x06, 
		   VSENSOR_1.8V from VREG_L20A, VSENSOR_2.85V from VREG_L11A */
		k3dh_akm_l20 = regulator_get(NULL, "8058_l20");
		if (IS_ERR(k3dh_akm_l20)) {
			return PTR_ERR(k3dh_akm_l20);
		}

		err = regulator_set_voltage(k3dh_akm_l20, 1800000, 1800000);
		if (err) {
			pr_err("%s: unable to set the voltage for regulator"
					"pm8058_l20\n", __func__);
			regulator_put(k3dh_akm_l20);
			return err;
		}
	}
	else if(system_rev >= 0x6){
		/* rev >= 0x06, 
		   VSENSOR_1.8V from VIO_P3_1.8V, VSENSOR_2.85V from VREG_L11A */
		printk("[K3DH] power_on mapped NULL\n");
		k3dh_data.power_on = NULL; 
		//k3dh_data.power_off = NULL; 

		err = regulator_enable(k3dh_akm_l11);
		if (err) {
			pr_err("%s: unable to enable regulator"
					"pm8058_l11\n", __func__);
			regulator_put(k3dh_akm_l11);
			return err;
		}

		/* VSENSOR_2.85V, turn it on/off, requested from HW - start */
		msleep(1);
		err = regulator_disable(k3dh_akm_l11);
		if (err) {
			pr_err("%s: unable to enable regulator"
					"pm8058_l11\n", __func__);
			regulator_put(k3dh_akm_l11);
			return err;
		}
		msleep(10);
		err = regulator_enable(k3dh_akm_l11);
		if (err) {
			pr_err("%s: unable to enable regulator"
					"pm8058_l11\n", __func__);
			regulator_put(k3dh_akm_l11);
			return err;
		}
		/* VSENSOR_2.85V, turn it on/off, requested from HW - end */
	}
	else
		printk("[K3DH] unknown rev=%d\n", system_rev );
#endif
	gpio_tlmm_config(GPIO_CFG(SENSOR_AKM_SDA, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(SENSOR_AKM_SCL, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

	if(system_rev >= 0x4)
		gpio_tlmm_config(GPIO_CFG(102/*GYRO_FIFO_INT*/, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

	return 0;
}
#endif

/* Turn off cam core power of default on status to fix sleep current issue */
static void cam_power_init(void)
{
	struct regulator *i_core12;
	int ret;

	i_core12 = regulator_get(NULL, "8901_s2"); //CORE 1.2V
	if (IS_ERR(i_core12)) {
		printk("%s:i_core12 error regulator get\n", __func__);
		return;
	}

	ret = regulator_set_voltage(i_core12, 1200000, 1200000);

	if (ret)
		printk("%s:i_core12 error setting voltage\n", __func__);

	ret = regulator_enable(i_core12);
	if (ret)
		printk("%s:i_core12 error enabling regulator\n", __func__);

	mdelay(1);

	if (regulator_is_enabled(i_core12)) {
		ret = regulator_disable(i_core12);
		if (ret)
			printk("%s:i_core12 error disabling regulator\n", __func__);
	}

	regulator_put(i_core12);
}

void cam_power_on(void)
{
	struct regulator *l8, *l15, *l24, *lvs0,  *s2, *lvs1;
	int ret;

	printk(" ##### cam_power_on #####\n");
	mdelay(50);

	s2 = regulator_get(NULL, "8901_s2");
	//		   if (IS_ERR(l24))
	//			return -1;

	ret = regulator_set_voltage(s2, 1200000, 1200000);
	if (ret) {
	printk("%s: error setting voltage\n", __func__);
	}

	ret = regulator_enable(s2);
	if (ret) {
	printk("%s: error enabling regulator\n", __func__);
	}
	mdelay(100);
	
	l15 = regulator_get(NULL, "8058_l15");
	//		   if (IS_ERR(l17))
	//			return -1;

	ret = regulator_set_voltage(l15, 2850000, 2850000);
	if (ret) {
	printk("%s: error setting voltage\n", __func__);
	}

	ret = regulator_enable(l15);
	if (ret) {
	printk("%s: error enabling regulator\n", __func__);
	}
	mdelay(50);

	l8 = regulator_get(NULL, "8058_l8");
	//		   if (IS_ERR(l8))
	//			return -1;

	ret = regulator_set_voltage(l8, 2850000, 2850000);
	if (ret) {
	printk("%s: error setting voltage\n", __func__);
	}

	ret = regulator_enable(l8);
	if (ret) {
	printk("%s: error enabling regulator\n", __func__);
	}
	mdelay(50);
	
#if 0
	l24 = regulator_get(&rpm_vreg_device[RPM_VREG_ID_PM8058_L24].dev, "8058_l24");
	//		   if (IS_ERR(l24))
	//			return -1;

	ret = regulator_set_voltage(l24, 1500000, 1500000);
	if (ret) {
	printk("%s: error setting voltage\n", __func__);
	}

	ret = regulator_enable(l24);
	if (ret) {
	printk("%s: error enabling regulator\n", __func__);
	}
	mdelay(50);
#endif

	
	lvs0 = regulator_get(NULL, "8058_lvs0");
	//		   if (IS_ERR(l17))
	//			return -1;

	ret = regulator_enable(lvs0);
	if (ret) {
	printk("%s: error enabling regulator\n", __func__);
	}
	mdelay(50);

	lvs1 = regulator_get(NULL, "8901_lvs1");
	//		   if (IS_ERR(l17))
	//			return -1;

	ret = regulator_enable(lvs1);
	if (ret) {
	printk("%s: error enabling regulator\n", __func__);
	}
	mdelay(50);
		
}

usb_path_type get_usb_path(void)
{
	usb_path_type curr_usb = USB_SEL_MSM ;
	int usb_sel2 = gpio_get_value_cansleep(USB_SEL_2);
	
#if defined(CONFIG_USA_OPERATOR_ATT) || defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU) || defined(CONFIG_EUR_OPERATOR_OPEN)
	curr_usb = (usb_sel2 == 0) ? USB_SEL_MSM : USB_SEL_ADC;
#endif

	pr_info("%s : current usb path =%d\r\n", __func__, curr_usb);
	return curr_usb;
}
 
void set_usb_path(usb_path_type usb_path)
{
	int val=0;
	printk("%s : set usb path to %d\r\n", __func__, usb_path);
	
#if defined(CONFIG_USA_OPERATOR_ATT) || defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_LGU) || defined(CONFIG_EUR_OPERATOR_OPEN)
	val = (usb_path == USB_SEL_MSM) ? 0: 1 ;
#endif
	pr_debug("%s: GPIO37_CONFIG(0x%x),\r\n",  __func__, readl(GPIO_CONFIG(USB_SEL_2)));
	gpio_set_value(USB_SEL_2, val);
} 
 
static void __init usb_switch_init(void)
{
	pr_info("%s\n", __func__);

	gpio_request(USB_SEL_2, "usb_sel_2");
	gpio_tlmm_config(GPIO_CFG(USB_SEL_2, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_direction_output(USB_SEL_2, 0); // set to MSM USB

	pr_info("%s: GPIO37_CONFIG(0x%x),\r\n",  __func__, readl(GPIO_CONFIG(USB_SEL_2)));
}

static void __init usb_host_init(void)
{
	pr_info("%s\n", __func__);

	gpio_request(GPIO_OTG_EN, "usb_host");
	gpio_tlmm_config(GPIO_CFG(GPIO_OTG_EN, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_direction_output(GPIO_OTG_EN, 0);
#ifdef CONFIG_BATTERY_P5LTE
	mutex_init(&adc_lock);
#endif
}
static int tsp_power_on(void)
{
	int ret = 0;

	gpio_request(62, "tsp_pwr_en");
	gpio_tlmm_config(GPIO_CFG(62, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	gpio_direction_output(62, 1);
	
#if !defined(CONFIG_USA_OPERATOR_ATT) && \
	!defined(CONFIG_KOR_OPERATOR_SKT) && \
	!defined(CONFIG_KOR_OPERATOR_LGU) && \
	!defined(CONFIG_EUR_OPERATOR_OPEN)

	ret = gpio_request( 63 , "TOUCH_RST");
	if (ret < 0) {
		printk(KERN_ERR "%s: TOUCH_RST gpio %d request"
			"failed\n", __func__, 63 );
		return ret;
	}
	gpio_direction_output(63 , 0);
	msleep(20);
	gpio_set_value_cansleep(63 , 1);
	msleep(100);
	gpio_set_value_cansleep(63 , 0);
	msleep(100);
	gpio_set_value_cansleep(63 , 1);
	msleep(200);
#endif
	return ret;
}

static int lcdc_panel_power(int on)
{
	int flag_on = !!on;
	static int lcdc_power_save_on;

	if (lcdc_power_save_on == flag_on)
		return 0;

	lcdc_power_save_on = flag_on;

	lcdc_samsung_panel_power(on);

	return 0;
}

#ifdef CONFIG_MSM_BUS_SCALING

static struct msm_bus_vectors rotator_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_SMI,
#if 1	
		.ab = 0,
		.ib = 0,
#else	
		.ab = (1024 * 600 * 4 * 2 * 60),
		.ib = (1024 * 600 * 4 * 2 * 60 * 1.5),
#endif	
	},
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors rotator_ui_vectors[] = {
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = 0,
		.ib  = 0,
	},
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = (1024 * 600 * 4 * 2 * 60),
		.ib  = (1024 * 600 * 4 * 2 * 60 * 1.5),
	},
};

static struct msm_bus_vectors rotator_vga_vectors[] = {
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = (640 * 480 * 2 * 2 * 30),
		.ib  = (640 * 480 * 2 * 2 * 30 * 1.5),
	},
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = (640 * 480 * 2 * 2 * 30),
		.ib  = (640 * 480 * 2 * 2 * 30 * 1.5),
	},
};

static struct msm_bus_vectors rotator_720p_vectors[] = {
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = (1280 * 736 * 2 * 2 * 30),
		.ib  = (1280 * 736 * 2 * 2 * 30 * 1.5),
	},
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = (1280 * 736 * 2 * 2 * 30),
		.ib  = (1280 * 736 * 2 * 2 * 30 * 1.5),
	},
};

static struct msm_bus_vectors rotator_1080p_vectors[] = {
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab  = (1920 * 1088 * 2 * 2 * 30),
		.ib  = (1920 * 1088 * 2 * 2 * 30 * 1.5),
	},
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab  = (1920 * 1088 * 2 * 2 * 30),
		.ib  = (1920 * 1088 * 2 * 2 * 30 * 1.5),
	},
};

static struct msm_bus_paths rotator_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(rotator_init_vectors),
		rotator_init_vectors,
	},
	{
		ARRAY_SIZE(rotator_ui_vectors),
		rotator_ui_vectors,
	},
	{
		ARRAY_SIZE(rotator_vga_vectors),
		rotator_vga_vectors,
	},
	{
		ARRAY_SIZE(rotator_720p_vectors),
		rotator_720p_vectors,
	},
	{
		ARRAY_SIZE(rotator_1080p_vectors),
		rotator_1080p_vectors,
	},
};

struct msm_bus_scale_pdata rotator_bus_scale_pdata = {
	rotator_bus_scale_usecases,
	ARRAY_SIZE(rotator_bus_scale_usecases),
	.name = "rotator",
};

static struct msm_bus_vectors mdp_init_vectors[] = {
	/* For now, 0th array entry is reserved.
	 * Please leave 0 as is and don't use it
	 */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 0,
		.ib = 0,
	},
	/* Master and slaves can be from different fabrics */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

#ifdef CONFIG_FB_MSM_HDMI_AS_PRIMARY
static struct msm_bus_vectors hdmi_as_primary_vectors[] = {
	/* If HDMI is used as primary */
	 {
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 2000000000,
		.ib = 2000000000,
	 },
	 /* Master and slaves can be from different fabrics */
	 {
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 2000000000,
		.ib = 2000000000,
	 },
};

static struct msm_bus_paths mdp_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(mdp_init_vectors),
		mdp_init_vectors,
	},
	{
		ARRAY_SIZE(hdmi_as_primary_vectors),
		hdmi_as_primary_vectors,
	},
	{
		ARRAY_SIZE(hdmi_as_primary_vectors),
		hdmi_as_primary_vectors,
	},
	{
		ARRAY_SIZE(hdmi_as_primary_vectors),
		hdmi_as_primary_vectors,
	},
	{
		ARRAY_SIZE(hdmi_as_primary_vectors),
		hdmi_as_primary_vectors,
	},
	{
		ARRAY_SIZE(hdmi_as_primary_vectors),
		hdmi_as_primary_vectors,
	},
};
#else
#ifdef CONFIG_FB_MSM_LCDC_DSUB
static struct msm_bus_vectors mdp_sd_smi_vectors[] = {
	/* Default case static display/UI/2d/3d if FB SMI */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 388800000,
		.ib = 486000000,
	},
	/* Master and slaves can be from different fabrics */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors mdp_sd_ebi_vectors[] = {
	/* Default case static display/UI/2d/3d if FB SMI */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 0,
		.ib = 0,
	},
	/* Master and slaves can be from different fabrics */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 388800000,
		.ib = 486000000 * 2,
	},
};
static struct msm_bus_vectors mdp_vga_vectors[] = {
	/* VGA and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 458092800,
		.ib = 572616000,
	},
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 458092800,
		.ib = 572616000 * 2,
	},
};
static struct msm_bus_vectors mdp_720p_vectors[] = {
	/* 720p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 471744000,
		.ib = 589680000,
	},
	/* Master and slaves can be from different fabrics */
       {
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
	       .ab = 471744000,
	       .ib = 589680000 * 2,
	},
};

static struct msm_bus_vectors mdp_1080p_vectors[] = {
	/* 1080p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 575424000,
		.ib = 719280000,
	},
	/* Master and slaves can be from different fabrics */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 575424000,
		.ib = 719280000 * 2,
	},
};

#else
static struct msm_bus_vectors mdp_sd_smi_vectors[] = {
	/* Default case static display/UI/2d/3d if FB SMI */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		/*
		.ab = 175110000,
		.ib = 218887500,
		.ab = (1280 * 800 * 4 * 60) + (1920 * 1080 * 1.5 * 60),
		.ib = ab * 1.25 * 2,		
		*/
		.ab = 432384000,
		.ib = 540480000 * 2,
	},
	/* Master and slaves can be from different fabrics */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};

static struct msm_bus_vectors mdp_sd_ebi_vectors[] = {
	/* Default case static display/UI/2d/3d if FB SMI */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 0,
		.ib = 0,
	},
	/* Master and slaves can be from different fabrics */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		/*
		.ab = 216000000,
		.ib = 270000000 * 2,
		.ab = (1280 * 800 * 4 * 60) + (1920 * 1080 * 1.5 * 60),
		.ib = ab * 1.25 * 2,
		*/
		.ab = 432384000,
		.ib = 540480000 * 2,
	},
};
static struct msm_bus_vectors mdp_vga_vectors[] = {
	/* VGA and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		/*
		.ab = 216000000,
		.ib = 270000000,
		.ab = (1280 * 800 * 4 * 60) + (640 * 480 * 1.5 * 60),
		.ib = ab * 1.25 * 2,
		*/
		.ab = 273408000,
		.ib = 341760000 * 2,
	},
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		/*
		.ab = 216000000,
		.ib = 270000000 * 2,
		.ab = (1280 * 800 * 4 * 60) + (640 * 480 * 1.5 * 60),
		.ib = ab * 1.25 * 2,
		*/
		.ab = 273408000,
		.ib = 341760000 * 2,
	},
};

static struct msm_bus_vectors mdp_720p_vectors[] = {
	/* 720p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		/*
		.ab = 230400000,
		.ib = 288000000,
		.ab = (1280 * 800 * 4 * 60) + (1280 * 720 * 1.5 * 60),
		.ib = ab * 1.25 * 2,
		*/
		.ab = 328704000,
		.ib = 410880000 *2,
	},
	/* Master and slaves can be from different fabrics */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		/*
		.ab = 230400000,
		.ib = 288000000 * 2,
		.ab = (1280 * 800 * 4 * 60) + (1280 * 720 * 1.5 * 60),
		.ib = ab * 1.25 * 2,
		*/
		.ab = 328704000,
		.ib = 410880000 *2,
	},
};

static struct msm_bus_vectors mdp_1080p_vectors[] = {
	/* 1080p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		/*
		.ab = 334080000,
		.ib = 417600000,
		.ab = (1280 * 800 * 4 * 60) + (1920 * 1080 * 1.5 * 60),
		.ib = ab * 1.25 * 2,
		*/
		.ab = 432384000,
		.ib = 540480000 * 2,
	},
	/* Master and slaves can be from different fabrics */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		/*
		.ab = 334080000,
		.ib = 550000000 * 2,
		.ab = (1280 * 800 * 4 * 60) + (1920 * 1080 * 1.5 * 60),
		.ib = ab * 1.25 * 2,
		*/
		.ab = 432384000,
		.ib = 540480000 * 2,
	},
};

#endif
static struct msm_bus_paths mdp_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(mdp_init_vectors),
		mdp_init_vectors,
	},
	{
		ARRAY_SIZE(mdp_sd_smi_vectors),
		mdp_sd_smi_vectors,
	},
	{
		ARRAY_SIZE(mdp_sd_ebi_vectors),
		mdp_sd_ebi_vectors,
	},
	{
		ARRAY_SIZE(mdp_vga_vectors),
		mdp_vga_vectors,
	},
	{
		ARRAY_SIZE(mdp_720p_vectors),
		mdp_720p_vectors,
	},
	{
		ARRAY_SIZE(mdp_1080p_vectors),
		mdp_1080p_vectors,
	},
};
#endif
static struct msm_bus_scale_pdata mdp_bus_scale_pdata = {
	mdp_bus_scale_usecases,
	ARRAY_SIZE(mdp_bus_scale_usecases),
	.name = "mdp",
};

#endif
#ifdef CONFIG_MSM_BUS_SCALING
static struct msm_bus_vectors dtv_bus_init_vectors[] = {
	/* For now, 0th array entry is reserved.
	 * Please leave 0 as is and don't use it
	 */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 0,
		.ib = 0,
	},
	/* Master and slaves can be from different fabrics */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 0,
		.ib = 0,
	},
};
#ifdef CONFIG_FB_MSM_HDMI_AS_PRIMARY
static struct msm_bus_vectors dtv_bus_def_vectors[] = {
	/* For now, 0th array entry is reserved.
	 * Please leave 0 as is and don't use it
	 */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 2000000000,
		.ib = 2000000000,
	},
	/* Master and slaves can be from different fabrics */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 2000000000,
		.ib = 2000000000,
	},
};
#else
static struct msm_bus_vectors dtv_bus_def_vectors[] = {
	/* For now, 0th array entry is reserved.
	 * Please leave 0 as is and don't use it
	 */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 566092800,
		.ib = 707616000,
	},
	/* Master and slaves can be from different fabrics */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 566092800,
		.ib = 707616000,
	},
};
#endif
static struct msm_bus_paths dtv_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(dtv_bus_init_vectors),
		dtv_bus_init_vectors,
	},
	{
		ARRAY_SIZE(dtv_bus_def_vectors),
		dtv_bus_def_vectors,
	},
};
static struct msm_bus_scale_pdata dtv_bus_scale_pdata = {
	dtv_bus_scale_usecases,
	ARRAY_SIZE(dtv_bus_scale_usecases),
	.name = "dtv",
};

static struct lcdc_platform_data dtv_pdata = {
	.bus_scale_table = &dtv_bus_scale_pdata,
};
#endif


static struct lcdc_platform_data lcdc_pdata = {
	.lcdc_power_save   = lcdc_panel_power,
};


#define MDP_VSYNC_GPIO 28

/*
 * MIPI_DSI only use 8058_LDO0 which need always on
 * therefore it need to be put at low power mode if
 * it was not used instead of turn it off.
 */
static int mipi_dsi_panel_power(int on)
{
	int flag_on = !!on;
	static int mipi_dsi_power_save_on;
	static struct regulator *ldo0;
	int rc = 0;

	if (mipi_dsi_power_save_on == flag_on)
		return 0;

	mipi_dsi_power_save_on = flag_on;

	if (ldo0 == NULL) {	/* init */
		ldo0 = regulator_get(NULL, "8058_l0");
		if (IS_ERR(ldo0)) {
			pr_debug("%s: LDO0 failed\n", __func__);
			rc = PTR_ERR(ldo0);
			return rc;
		}

		rc = regulator_set_voltage(ldo0, 1200000, 1200000);
		if (rc)
			goto out;

		rc = regulator_enable(ldo0);
		if (rc)
			goto out;
	}

	if (on) {
		/* set ldo0 to HPM */
		rc = regulator_set_optimum_mode(ldo0, 100000);
		if (rc < 0)
			goto out;
	} else {
		/* set ldo0 to LPM */
		rc = regulator_set_optimum_mode(ldo0, 9000);
		if (rc < 0)
			goto out;
	}

	return 0;
out:
	regulator_disable(ldo0);
	regulator_put(ldo0);
	ldo0 = NULL;
	return rc;
}

static struct mipi_dsi_platform_data mipi_dsi_pdata = {
	.vsync_gpio = MDP_VSYNC_GPIO,
	.dsi_power_save   = mipi_dsi_panel_power,
};

#ifdef CONFIG_FB_MSM_MIPI_DSI
int mdp_core_clk_rate_table[] = {
	85330000,
	85330000,
	160000000,
	200000000,
};
#elif defined(CONFIG_FB_MSM_HDMI_AS_PRIMARY)
int mdp_core_clk_rate_table[] = {
	200000000,
	200000000,
	200000000,
	200000000,
};
#else
int mdp_core_clk_rate_table[] = {
	128000000, //0922 QC recommend : from 85330000
	128000000, //0922 QC recommend : from 85330000
	177780000, //0922 QC recommend : from -> 177780000
	200000000,
};
#endif

static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = MDP_VSYNC_GPIO,
#ifdef CONFIG_FB_MSM_HDMI_AS_PRIMARY
	.mdp_core_clk_rate = 200000000,
#else
	.mdp_core_clk_rate = 128000000, // 85330000,
#endif
	.mdp_core_clk_table = mdp_core_clk_rate_table,
	.num_mdp_clk = ARRAY_SIZE(mdp_core_clk_rate_table),
#ifdef CONFIG_MSM_BUS_SCALING
	.mdp_bus_scale_table = &mdp_bus_scale_pdata,
#endif
	.mdp_rev = MDP_REV_41,
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	.mem_hid = ION_CP_WB_HEAP_ID,
#else
#ifdef CONFIG_SAMSUNG_MEMORY_LAYOUT_ARRANGE
	.mem_hid = MEMTYPE_PMEM_MDP,
#else	
	.mem_hid = MEMTYPE_EBI1,
#endif	
#endif
};

static void __init reserve_mdp_memory(void)
{
	mdp_pdata.ov0_wb_size = MSM_FB_OVERLAY0_WRITEBACK_SIZE;
	mdp_pdata.ov1_wb_size = MSM_FB_OVERLAY1_WRITEBACK_SIZE;
#if defined(CONFIG_ANDROID_PMEM) && !defined(CONFIG_MSM_MULTIMEDIA_USE_ION)
	msm8x60_reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov0_wb_size;
	msm8x60_reserve_table[mdp_pdata.mem_hid].size +=
		mdp_pdata.ov1_wb_size;
#endif
}

static void __init msm_fb_add_devices(void)
{
	msm_fb_register_device("mdp", &mdp_pdata);

	msm_fb_register_device("lcdc", &lcdc_pdata);
#ifdef CONFIG_MSM_BUS_SCALING
	msm_fb_register_device("dtv", &dtv_pdata);
#endif
}

extern int bluesleep_start(void);
extern void bluesleep_stop(void);

/*
  GPIO_CFG(gpio, func, dir, pull, drvstr) 

  func -  0 : gpio_oe_reg //GPIO Output Enable
 		1 : Alternate functional interface
 		( gsbi (General Serial Bus Interface) for (HS-UART/UIM/SPI/I2C)
 		  GPIO53 : gsbi6(3) : UART_TX
 		  GPIO54 : gsbi6(2) : UART_RX
  		  GPIO55 : gsbi6(1) : UART_CTS
  		  GPIO56 : gsbi6(0) : UART_RTS

 		  GPIO111 : audio_pcm_dout
 		  GPIO112 : audio_pcm_din
  		  GPIO113 : audio_pcm_sync
  		  GPIO114 : audio_pcm_clk
*/

static uint32_t bt_config_power_on[] = {
	 GPIO_CFG(GPIO_BT_RESET, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_REG_ON, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_WAKE, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_UART_RTS, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_UART_CTS, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_UART_RXD, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_UART_TXD, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_PCM_DOUT, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_PCM_DIN, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 
	 GPIO_CFG(GPIO_BT_PCM_SYNC, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_PCM_CLK, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 
	 GPIO_CFG(GPIO_BT_HOST_WAKE, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
};

static uint32_t bt_config_power_off[] = {
	 GPIO_CFG(GPIO_BT_RESET, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_REG_ON, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_WAKE, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 
	 GPIO_CFG(GPIO_BT_UART_RTS, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_UART_CTS, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_UART_RXD, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_UART_TXD, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_PCM_DOUT, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_PCM_DIN, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 
	 GPIO_CFG(GPIO_BT_PCM_SYNC, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_PCM_CLK, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	 GPIO_CFG(GPIO_BT_HOST_WAKE, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 
};

static int bluetooth_power(int on)
{
	printk(KERN_DEBUG "%s\n", __func__);

	if (on) {
		printk(KERN_DEBUG "config_gpio_table bt pwr on\n");
		config_gpio_table(bt_config_power_on, ARRAY_SIZE(bt_config_power_on));

		pr_info("bluetooth_power BT_WAKE:%d, HOST_WAKE:%d, REG_ON:%d\n", gpio_get_value(GPIO_BT_WAKE), gpio_get_value(GPIO_BT_HOST_WAKE), gpio_get_value(GPIO_BT_REG_ON));
		gpio_direction_output(GPIO_BT_WAKE, GPIO_WLAN_LEVEL_HIGH);
		gpio_direction_output(GPIO_BT_REG_ON, GPIO_WLAN_LEVEL_HIGH);

		msleep(50);
		gpio_direction_output(GPIO_BT_RESET, GPIO_WLAN_LEVEL_HIGH);

		pr_info("bluetooth_power BT_WAKE:%d, HOST_WAKE:%d, REG_ON:%d\n", gpio_get_value(GPIO_BT_WAKE), gpio_get_value(GPIO_BT_HOST_WAKE), gpio_get_value(GPIO_BT_REG_ON));        

		bluesleep_start();
	} else {
		bluesleep_stop();
		gpio_direction_output(GPIO_BT_RESET, GPIO_WLAN_LEVEL_LOW);/* BT_VREG_CTL */

		gpio_direction_output(GPIO_BT_REG_ON, GPIO_WLAN_LEVEL_LOW);/* BT_RESET */
		msleep(10);
		gpio_direction_output(GPIO_BT_WAKE, GPIO_WLAN_LEVEL_LOW);/* BT_VREG_CTL */

		printk(KERN_DEBUG "config_gpio_table bt pwr off\n");
		config_gpio_table(bt_config_power_off, ARRAY_SIZE(bt_config_power_off));
	}
	return 0;
}

static void __init bt_power_init(void)
{
	int rc = 0;

#ifdef CONFIG_SAMSUNG_LPM_MODE
#if defined(CONFIG_TARGET_LOCALE_KOR_SKT) || defined(CONFIG_TARGET_LOCALE_KOR_KT) || defined(CONFIG_TARGET_LOCALE_KOR_LGU) \
	|| defined(CONFIG_TARGET_LOCALE_USA_ATT) || defined (CONFIG_TARGET_LOCALE_JPN_NTT) || defined(CONFIG_TARGET_LOCALE_EUR_OPEN)
	if(charging_mode_from_boot == 1)
		return;
#endif
#endif

	pr_info("bt_power_init \n");

	msm_bt_power_device.dev.platform_data = &bluetooth_power;

	pr_info("bt_gpio_init : low \n");

	config_gpio_table(bt_config_power_off, ARRAY_SIZE(bt_config_power_off));
	
	rc = gpio_request(GPIO_BT_RESET, "BT_RST");
	if (!rc)
		gpio_direction_output(GPIO_BT_RESET, 0);
	else {
		pr_err("%s: gpio_direction_output %d = %d\n", __func__,
			GPIO_BT_RESET, rc);
		gpio_free(GPIO_BT_RESET);
	}
	
	rc = gpio_request(GPIO_BT_REG_ON, "BT_REGON");
	if (!rc)
		gpio_direction_output(GPIO_BT_REG_ON, 0);
	else {
		pr_err("%s: gpio_direction_output %d = %d\n", __func__,
			GPIO_BT_REG_ON, rc);
		gpio_free(GPIO_BT_REG_ON);
	}

	pr_err("%s: gpio_direction_output success (GPIO: %d , %d)\n", __func__,
			GPIO_BT_RESET, GPIO_BT_REG_ON);
}

#ifdef CONFIG_MSM_RPM
static struct msm_rpm_platform_data msm_rpm_data = {
	.reg_base_addrs = {
		[MSM_RPM_PAGE_STATUS] = MSM_RPM_BASE,
		[MSM_RPM_PAGE_CTRL] = MSM_RPM_BASE + 0x400,
		[MSM_RPM_PAGE_REQ] = MSM_RPM_BASE + 0x600,
		[MSM_RPM_PAGE_ACK] = MSM_RPM_BASE + 0xa00,
	},

	.irq_ack = RPM_SCSS_CPU0_GP_HIGH_IRQ,
	.irq_err = RPM_SCSS_CPU0_GP_LOW_IRQ,
	.irq_vmpm = RPM_SCSS_CPU0_GP_MEDIUM_IRQ,
	.msm_apps_ipc_rpm_reg = MSM_GCC_BASE + 0x008,
	.msm_apps_ipc_rpm_val = 4,
};
#endif

#ifdef CONFIG_KOR_OPERATOR_LGU
void msm_fusion_setup_pinctrl(void)
{
        struct msm_xo_voter *a1;     

	/*
	 * Vote for the A1 clock to be in pin control mode before
	* the external images are loaded.
	*/
	a1 = msm_xo_get(MSM_XO_TCXO_A1, "mdm");
	BUG_ON(!a1);
	msm_xo_mode_vote(a1, MSM_XO_MODE_PIN_CTRL);
}
#endif

struct msm_board_data {
	struct msm_gpiomux_configs *gpiomux_cfgs;
};

static struct msm_board_data p5_lte_board_data __initdata = {
	.gpiomux_cfgs = msm8x60_p5_lte_gpiomux_cfgs,
};

#ifdef CONFIG_SEC_DEBUG
static ssize_t show_sec_debug_level(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	int sec_debug_level = kernel_sec_get_debug_level();
	char buffer[5];
	memcpy(buffer, &sec_debug_level, 4);
	buffer[4] = '\0';
	return sprintf(buf, "%s\n", buffer);
}

static ssize_t store_sec_debug_level(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf,
				  size_t count)
{
	int sec_debug_level = 0;
	memcpy(&sec_debug_level, buf, 4);

	printk("%s %x\n", __func__, sec_debug_level);

	if (!(sec_debug_level == KERNEL_SEC_DEBUG_LEVEL_LOW
			|| sec_debug_level == KERNEL_SEC_DEBUG_LEVEL_MID
			|| sec_debug_level == KERNEL_SEC_DEBUG_LEVEL_HIGH))
		return -EINVAL;

	kernel_sec_set_debug_level(sec_debug_level);

	return count;
}

static DEVICE_ATTR(sec_debug_level, S_IRUGO | S_IWUGO, show_sec_debug_level, store_sec_debug_level);
#endif /*CONFIG_SEC_DEBUG*/

#ifdef CONFIG_BROADCOM_WIFI
int __init brcm_wlan_init(void);
#endif
#ifdef CONFIG_KOR_OPERATOR_LGU
extern int no_uart_console; 
#endif

static void __init msm8x60_init(struct msm_board_data *board_data)
{
	uint32_t soc_platform_version;

#ifdef CONFIG_SEC_DEBUG
	sec_debug_init();
#endif

	printk("## Samsung P5 LTE Board Revision from ATAG = REV_%d ##\n", system_rev);

	pmic_reset_irq = PM8058_IRQ_BASE + PM8058_RESOUT_IRQ;
	
	/*
	 * Initialize RPM first as other drivers and devices may need
	 * it for their initialization.
	 */
#ifdef CONFIG_MSM_RPM
	BUG_ON(msm_rpm_init(&msm_rpm_data));
#endif
	BUG_ON(msm_rpmrs_levels_init(msm_rpmrs_levels,
				ARRAY_SIZE(msm_rpmrs_levels)));
	if (msm_xo_init())
		pr_err("Failed to initialize XO votes\n");

	msm8x60_check_2d_hardware();

	/* Change SPM handling of core 1 if PMM 8160 is present. */
	soc_platform_version = socinfo_get_platform_version();
	if (SOCINFO_VERSION_MAJOR(soc_platform_version) == 1 &&
			SOCINFO_VERSION_MINOR(soc_platform_version) >= 2) {
		struct msm_spm_platform_data *spm_data;

		spm_data = &msm_spm_data_v1[1];
		spm_data->reg_init_values[MSM_SPM_REG_SAW_CFG] &= ~0x0F00UL;
		spm_data->reg_init_values[MSM_SPM_REG_SAW_CFG] |= 0x0100UL;

		spm_data = &msm_spm_data[1];
		spm_data->reg_init_values[MSM_SPM_REG_SAW_CFG] &= ~0x0F00UL;
		spm_data->reg_init_values[MSM_SPM_REG_SAW_CFG] |= 0x0100UL;
	}

	/*
	 * Initialize SPM before acpuclock as the latter calls into SPM
	 * driver to set ACPU voltages.
	 */
	if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) != 1)
		msm_spm_init(msm_spm_data, ARRAY_SIZE(msm_spm_data));
	else
		msm_spm_init(msm_spm_data_v1, ARRAY_SIZE(msm_spm_data_v1));

	/*
	 * Disable regulator info printing so that regulator registration
	 * messages do not enter the kmsg log.
	 */
	regulator_suppress_info_printing();

	/* Initialize regulators needed for clock_init. */
	platform_add_devices(early_regulators, ARRAY_SIZE(early_regulators));

	msm_clock_init(&msm8x60_clock_init_data);

	/* Buses need to be initialized before early-device registration
	 * to get the platform data for fabrics.
	 */
	msm8x60_init_buses();
	platform_add_devices(early_devices, ARRAY_SIZE(early_devices));

	/* CPU frequency control is not supported on simulated targets. */
	acpuclk_init(&acpuclk_8x60_soc_data);

	msm8x60_init_tlmm();

	if(charging_mode_from_boot == 1) 
		msm8x60_init_gpiomux_cfg_for_lpm(board_data->gpiomux_cfgs);
	else
		msm8x60_init_gpiomux(board_data->gpiomux_cfgs);

#ifdef CONFIG_KOR_OPERATOR_LGU
	if (system_rev >= 0x09) {
		gpio_tlmm_config(GPIO_CFG(128, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
		gpio_tlmm_config(GPIO_CFG(141, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
	}
	if (!no_uart_console)
#endif

	msm8x60_init_uart12dm();
	msm8x60_init_mmc();
	bt_power_init();
	usb_switch_init();
	usb_host_init();
	samsung_sys_class_init();
#ifdef CONFIG_STMPE811_ADC
	stmpe811_init();
#endif

#if defined(CONFIG_PMIC8058_OTHC) || defined(CONFIG_PMIC8058_OTHC_MODULE)
	msm8x60_init_pm8058_othc();
#endif

	pm8058_platform_data.keypad_pdata = &ffa_keypad_data;

#ifndef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
#ifdef CONFIG_USB_ANDROID
	android_usb_pdata.product_id = 0x9032;
	android_usb_pdata.functions = charm_usb_functions_all,
	android_usb_pdata.num_functions =
			ARRAY_SIZE(charm_usb_functions_all),
	android_usb_pdata.products = charm_usb_products;
	android_usb_pdata.num_products =
			ARRAY_SIZE(charm_usb_products);
#endif
#endif
	if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) != 1)
		platform_add_devices(msm_footswitch_devices,
				     msm_num_footswitch_devices);

#ifdef CONFIG_KOR_OPERATOR_LGU
	/* skip uart_dm12 device initialization if uart console is disabled. */
	if (no_uart_console) {
		pr_info("%s(): disabled uart console\n", __func__);
		platform_device_register(&msm_device_smd);
		platform_add_devices(&surf_devices[2],
			ARRAY_SIZE(surf_devices) - 2);
	} else
		platform_add_devices(surf_devices,
			     ARRAY_SIZE(surf_devices));
#else
	platform_add_devices(surf_devices,
			     ARRAY_SIZE(surf_devices));
#endif	

#ifdef CONFIG_MSM_DSPS
	if (machine_is_msm8x60_fluid()) {
		platform_device_unregister(&msm_gsbi12_qup_i2c_device);
		msm8x60_init_dsps();
	}
#endif

	pm8901_vreg_mpp0_init();

	platform_device_register(&msm8x60_8901_mpp_vreg);

#ifdef CONFIG_USB_EHCI_MSM_72K
	msm_add_host(0, &msm_usb_host_pdata);
#endif

	platform_add_devices(asoc_devices,
		ARRAY_SIZE(asoc_devices));

	tsp_power_on();

//	if(charging_mode_from_boot == 1)
//		platform_add_devices(&charm_devices[1], ARRAY_SIZE(charm_devices) - 1);
//	else
		platform_add_devices(charm_devices, ARRAY_SIZE(charm_devices));

	msm_fb_add_devices();
	fixup_i2c_configs();
#if defined (CONFIG_SENSORS_YDA165) || defined (CONFIG_SENSORS_YDA160) || defined (CONFIG_SENSORS_MAX9879) || defined(CONFIG_WM8994_AMP)
	platform_device_register(&amp_i2c_gpio_device);
#endif
	register_i2c_devices();

#if defined(CONFIG_TDMB) || defined(CONFIG_TDMB_MODULE)
	tdmb_dev_init();
#endif

	msm_pm_set_platform_data(msm_pm_data, ARRAY_SIZE(msm_pm_data));
	msm_pm_set_rpm_wakeup_irq(RPM_SCSS_CPU0_WAKE_UP_IRQ);
	msm_cpuidle_set_states(msm_cstates, ARRAY_SIZE(msm_cstates),
				msm_pm_data);
	BUG_ON(msm_pm_boot_init(&msm_pm_boot_pdata));

	pm8058_gpios_init();

#ifdef CONFIG_SENSORS_MSM_ADC
	if (machine_is_msm8x60_fluid()) {
		msm_adc_pdata.dev_names = msm_adc_fluid_device_names;
		msm_adc_pdata.num_adc = ARRAY_SIZE(msm_adc_fluid_device_names);
		if (SOCINFO_VERSION_MAJOR(soc_platform_version) < 3)
			msm_adc_pdata.gpio_config = APROC_CONFIG;
		else
			msm_adc_pdata.gpio_config = MPROC_CONFIG;
	}
	msm_adc_pdata.target_hw = MSM_8x60;
#endif

#ifdef CONFIG_MSM8X60_AUDIO
	msm_snddev_init();
#endif

	msm8x60_multi_sdio_init();


#ifdef CONFIG_KOR_OPERATOR_LGU
	msm_fusion_setup_pinctrl();
#endif

#ifdef CONFIG_SENSORS_AK8975_I957
	magnetic_device_init();
#endif
#ifdef CONFIG_GYRO_K3G_I957
	gyro_device_init();
#endif
#ifdef CONFIG_OPTICAL_GP2A
// TEMP	opt_device_init();
#endif
#ifdef CONFIG_OPTICAL_BH1721_I957
	opt_device_init();
#endif

#ifdef CONFIG_BROADCOM_WIFI
    brcm_wlan_init();
#endif

#ifdef CONFIG_30PIN_CONN
	acc_int_init();
#endif

	vibrator_device_gpio_init();

#ifdef CONFIG_SEC_DEBUG
	/* Add debug level node */
	int ret = 0;
	struct device *platform = surf_devices[0]->dev.parent;
	ret = device_create_file(platform, &dev_attr_sec_debug_level);
	if (ret)
		printk("Fail to create sec_debug_level file\n");
#endif /*CONFIG_KERNEL_DEBUG_SEC*/

}

unsigned int get_hw_rev(void)
{
	return system_rev;
}
EXPORT_SYMBOL(get_hw_rev);

static void __init msm8x60_p5_lte_init_early(void)
{
	msm8x60_allocate_memory_regions();
}

static void __init msm8x60_p5_lte_init(void)
{
	msm8x60_init(&p5_lte_board_data);
}

#define to_string(x) #x
#define TARGET_DEVICE_NAME(x)   to_string(x)

MACHINE_START(MSM8X60_FUSN_FFA, TARGET_DEVICE_NAME(TARGET_PRODUCT))
	.map_io = msm8x60_map_io,
	.reserve = msm8x60_reserve,
	.init_irq = msm8x60_init_irq,
	.handle_irq = gic_handle_irq,
	.init_machine = msm8x60_p5_lte_init,
	.timer = &msm_timer,
	.init_early = msm8x60_p5_lte_init_early,
MACHINE_END
