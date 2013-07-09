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

#include <linux/leds.h>
#include <linux/pmic8058-othc.h>
#include <linux/mfd/pmic8901.h>
#include <linux/regulator/gpio-regulator.h>
#include <linux/regulator/pmic8058-regulator.h>
#include <linux/regulator/pmic8901-regulator.h>
#include <linux/bootmem.h>
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
#include <linux/i2c/isa1200.h>
#include <linux/dma-mapping.h>
#include <linux/i2c/bq27520.h>

#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT540E)
#include <linux/i2c/mxt540e_q1.h>
#endif


#ifdef CONFIG_SENSORS_YDA165
#ifdef CONFIG_USA_MODEL_SGH_I577
#include <linux/i2c/yda165_integ.h>
#else
#include <linux/i2c/yda165.h>
#endif
#endif

//#undef CONFIG_SAMSUNG_JACK
#ifdef CONFIG_SAMSUNG_JACK
#include <linux/sec_jack.h>
#endif
#ifdef CONFIG_SAMSUNG_EARJACK
#include <linux/sec_earjack.h>
#endif

#ifdef CONFIG_SENSORS_K3DH
#include <linux/i2c/k3dh.h>
#endif

#ifdef CONFIG_SENSORS_AK8975
#include <linux/i2c/ak8975.h>
#endif

#ifdef CONFIG_GYRO_K3G
#include <linux/i2c/k3g.h>
#endif

#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif
#ifdef CONFIG_BATTERY_MAX17040
#include <linux/max17040_battery.h>
#endif
#ifdef CONFIG_CHARGER_SMB328A
#include <linux/smb328a_charger.h>
#endif

#if defined(CONFIG_SMB137B_CHARGER) || defined(CONFIG_SMB137B_CHARGER_MODULE)
#include <linux/i2c/smb137b.h>
#endif
#ifdef CONFIG_SND_SOC_WM8903
#include <sound/wm8903.h>
#endif
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>
#include <asm/hardware/gic.h>

#include <mach/dma.h>
#if defined (CONFIG_JPN_MODEL_SC_05D)
#include <mach/mpp.h>
#endif
#include <mach/board.h>
#include <mach/irqs.h>
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
#include <mach/devices-lte.h>
#include <mach/iommu_domains.h>
#ifdef CONFIG_ANDROID_RAM_CONSOLE
#include <linux/memblock.h>
#endif

#include "devices.h"
#include "devices-msm8x60.h"
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
#include "board-storage-common-a.h"

#include <linux/ion.h>
#include <mach/ion.h>

#include <linux/power_supply.h>
#include <mach/sec_battery.h>
#include <linux/i2c/fsa9480.h>
#include <mach/sec_debug.h>
#include <mach/sec_switch.h>
#include <linux/usb/gadget.h>
#if defined(CONFIG_VIDEO_MHL_V1) || defined(CONFIG_VIDEO_MHL_V2)
#include <linux/sii9234.h>
#endif
#include <linux/switch.h>

#ifdef CONFIG_EPEN_WACOM_G5SP
#include <linux/wacom_i2c.h>
static struct wacom_g5_callbacks *wacom_callbacks;
#endif 


#ifdef CONFIG_PN544_NFC
#include <linux/nfc/pn544.h>
#endif 
#ifdef CONFIG_VP_A2220
#include <linux/a2220.h>
#endif
#if defined(CONFIG_TDMB) || defined(CONFIG_TDMB_MODULE)
#include <mach/tdmb_pdata.h>
#endif
#if defined(CONFIG_ISDBT)
#include <mach/isdbt_pdata.h>
#endif
#ifdef CONFIG_FB_MSM_MIPI_DSI_ESD_REFRESH
#if !defined(CONFIG_JPN_MODEL_SC_05D)	
#include <linux/video/sec_mipi_lcd_esd_refresh.h>
#endif
#endif

#define MSM_SHARED_RAM_PHYS 0x40000000

#ifdef CONFIG_OPTICAL_GP2A
#define PMIC_GPIO_PS_VOUT		PM8058_GPIO(14) 	/* PMIC GPIO Number 14 */
#endif
#ifdef CONFIG_SENSORS_AK8975
#define PMIC_GPIO_MSENSE_RST	PM8058_GPIO(33) 	/* PMIC GPIO Number 33 */
#endif
#ifdef CONFIG_GYRO_K3G
#define PMIC_GPIO_GYRO_FIFO_INT	PM8058_GPIO(11) 	/* PMIC GPIO Number 11 */
#define MSM_GPIO_GYRO_FIFO_INT	102 	               /* MSM GPIO Number 102 */
#define PMIC_GPIO_GYRO_INT	PM8058_GPIO(12) 	/* PMIC GPIO Number 11 */
#endif
#ifdef CONFIG_SENSORS_AK8975
#define PMIC_GPIO_ACCEL_INT	PM8058_GPIO(10) 	/* PMIC GPIO Number 10 */
#define PMIC_GPIO_ACCEL_INT_11	PM8058_GPIO(11) 	/* PMIC GPIO Number 11 */
#endif
#ifdef CONFIG_KEYPAD_CYPRESS_TOUCH
#define PMIC_GPIO_TKEY_INT	PM8058_GPIO(13) 	/* PMIC GPIO Number 13 */
#endif

#ifdef CONFIG_KEYPAD_CYPRESS_TOUCH
#define GPIO_TKEY_I2C_SCL	157
#define GPIO_TKEY_I2C_SDA	156
#endif
#if defined(CONFIG_VIDEO_MHL_V1) || defined(CONFIG_VIDEO_MHL_V2)
#define PMIC_GPIO_MHL_RST       PM8058_GPIO(15)
#define PMIC_GPIO_MHL_SEL		PM8058_GPIO(16)
#define PMIC_GPIO_MHL_INT_9		PM8058_GPIO(9)	/* PMIC GPIO Number 9 */
#define PMIC_GPIO_MHL_INT_31    PM8058_GPIO(31)
#define PMIC_GPIO_MHL_WAKE_UP	PM8058_GPIO(17)
#if defined(CONFIG_VIDEO_MHL_V1)
#define GPIO_MHL_I2C_SCL	65
#define GPIO_MHL_I2C_SDA	64
#elif defined(CONFIG_VIDEO_MHL_V2)
#define GPIO_MHL_RST PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(15))
#define GPIO_MHL_SEL PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(16))
#endif
#endif
/* Common PMIC GPIO ***************************/
#ifdef CONFIG_OPTICAL_GP2A
#define PMIC_GPIO_PS_VOUT		PM8058_GPIO(14) 	/* PMIC GPIO Number 14 */
#endif
#ifdef CONFIG_OPTICAL_GP2AP020A00F
#define PMIC_GPIO_ALS_INT		PM8058_GPIO(3) 	/* PMIC GPIO Number 3 */
#define PMIC_GPIO_ALS_INT_REV08		PM8058_GPIO(14) 	/* PMIC GPIO Number 3 */
#endif
#ifdef CONFIG_OPTICAL_TAOS
#define PMIC_GPIO_ALS_INT		PM8058_GPIO(14) 	/* PMIC GPIO Number 14 */
#endif
#if !defined(CONFIG_JPN_MODEL_SC_05D) // duplicated code?
#ifdef CONFIG_KEYPAD_CYPRESS_TOUCH
#define GPIO_TKEY_I2C_SCL	157
#define GPIO_TKEY_I2C_SDA	156
#endif
#endif
#ifdef CONFIG_USB_SWITCH_FSA9480
#define PMIC_GPIO_TA_CURRENT_SEL PM8058_GPIO(18)
#endif

#ifdef CONFIG_USA_MODEL_SGH_I717
#define PMIC_GPIO_MAIN_MICBIAS_EN	PM8058_GPIO(25)
#define PMIC_GPIO_SUB_MICBIAS_EN	PM8058_GPIO(26)
#endif
#if defined(CONFIG_SAMSUNG_JACK) || defined (CONFIG_SAMSUNG_EARJACK)
#define PMIC_GPIO_EAR_DET		PM8058_GPIO(27)  	/* PMIC GPIO Number 27 */
#define PMIC_GPIO_SHORT_SENDEND	PM8058_GPIO(28)  	/* PMIC GPIO Number 28 */
#define PMIC_GPIO_EAR_MICBIAS_EN PM8058_GPIO(29) /* PMIC GPIO Number 29  */
#if defined (CONFIG_JPN_MODEL_SC_05D)
#define PMIC_GPIO_EAR_SEND_END PM8058_GPIO(25) /* PMIC GPIO Number 25  */
#endif
#ifdef CONFIG_KOR_MODEL_SHV_E160L	// add EAR_VOL_KEY 
#define PMIC_GPIO_EAR_SENDEND_STATE PM8058_GPIO(28) 
#define PMIC_GPIO_VOLUME_KEY	PM8058_GPIO(25)  
#endif
#endif

#if defined(CONFIG_CHARGER_SMB328A) || defined(CONFIG_CHARGER_SMB136)
#define PMIC_GPIO_CHG_EN		PM8058_GPIO(23)
#define PMIC_GPIO_CHG_STAT		PM8058_GPIO(24)
#endif
#define MDM2AP_SYNC 129

#ifdef CONFIG_MSM_CAMERA
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
#define GPIO_FLASH_SEL		106
#endif
#endif

#define GPIO_ETHERNET_RESET_N_DRAGON	30
#define LCDC_SPI_GPIO_CLK				73
#define LCDC_SPI_GPIO_CS				72
#define LCDC_SPI_GPIO_MOSI				70
#define LCDC_AUO_PANEL_NAME				"lcdc_auo_wvga"
#define LCDC_SAMSUNG_OLED_PANEL_NAME	"lcdc_samsung_oled"
#define LCDC_SAMSUNG_WSVGA_PANEL_NAME	"lcdc_samsung_wsvga"
#define LCDC_SAMSUNG_SPI_DEVICE_NAME	"lcdc_samsung_ams367pe02"
#define LCDC_AUO_SPI_DEVICE_NAME		"lcdc_auo_nt35582"
#define LCDC_NT35582_PANEL_NAME			"lcdc_nt35582_wvga"

#define MIPI_CMD_NOVATEK_QHD_PANEL_NAME	"mipi_cmd_novatek_qhd"
#define MIPI_VIDEO_NOVATEK_QHD_PANEL_NAME	"mipi_video_novatek_qhd"
#define MIPI_VIDEO_TOSHIBA_WVGA_PANEL_NAME	"mipi_video_toshiba_wvga"
#define HDMI_PANEL_NAME	"hdmi_msm"
#define TVOUT_PANEL_NAME	"tvout_msm"

#define DSPS_PIL_GENERIC_NAME		"dsps"
#define DSPS_PIL_FLUID_NAME		"dsps_fluid"

#if defined(CONFIG_PN544_NFC)
#define PMIC_GPIO_NFC_IRQ		PM8058_GPIO_PM_TO_SYS(7) //8
#define PMIC_GPIO_NFC_EN 		PM8058_GPIO_PM_TO_SYS(29) //30
#define GPIO_NFC_FIRM		71 
#endif

#ifdef CONFIG_ION_MSM
static struct platform_device ion_dev;
#endif

#if defined (CONFIG_OPTICAL_GP2A) || defined(CONFIG_OPTICAL_TAOS)
#define SENSOR_ALS_SCL   		139
#define SENSOR_ALS_SDA   		138
#endif

#ifdef CONFIG_SENSORS_YDA165
/* Audio AMP Driver GPIO */
#define GPIO_AMP_I2C_SCL	154
#define GPIO_AMP_I2C_SDA	155
#endif
#if defined(CONFIG_TDMB) || defined(CONFIG_TDMB_MODULE)
#define GPIO_TDMB_EN    130
#define GPIO_TDMB_RST   126
#define GPIO_TDMB_INT   128
#define GPIO_TDMB_SPI_MOSI	  33
#define GPIO_TDMB_SPI_MISO	  34
#define GPIO_TDMB_SPI_CS 	  35
#define GPIO_TDMB_SPI_CLK	  36

enum {
	TDMB_PMIC_CLK_INIT,
	TDMB_PMIC_CLK_ON,
	TDMB_PMIC_CLK_OFF,
};
#endif

#if defined(CONFIG_ISDBT)
#define GPIO_ISDBT_EN    130
#define GPIO_ISDBT_RST   126
#define GPIO_ISDBT_INT   128
#define GPIO_ISDBT_SPI_MOSI	  33
#define GPIO_ISDBT_SPI_MISO	  34
#define GPIO_ISDBT_SPI_CS 	  35
#define GPIO_ISDBT_SPI_CLK	  36
#endif

enum {
	GPIO_EXPANDER_IRQ_BASE  = PM8901_IRQ_BASE + NR_PMIC8901_IRQS,
	GPIO_EXPANDER_GPIO_BASE = PM8901_MPP_BASE + PM8901_MPPS,
	/* CORE expander */
	GPIO_CORE_EXPANDER_BASE = GPIO_EXPANDER_GPIO_BASE,
	GPIO_CLASS_D1_EN        = GPIO_CORE_EXPANDER_BASE,
	GPIO_WLAN_DEEP_SLEEP_N,
	GPIO_LVDS_SHUTDOWN_N,
	GPIO_DISP_RESX_N        = GPIO_LVDS_SHUTDOWN_N,
	GPIO_MS_SYS_RESET_N,
	GPIO_CAP_TS_RESOUT_N,
	GPIO_CAP_GAUGE_BI_TOUT,
	GPIO_ETHERNET_PME,
	GPIO_EXT_GPS_LNA_EN,
	GPIO_MSM_WAKES_BT,
	GPIO_ETHERNET_RESET_N,
	GPIO_HEADSET_DET_N,
	GPIO_USB_UICC_EN,
	GPIO_BACKLIGHT_EN,
	GPIO_EXT_CAMIF_PWR_EN,
	GPIO_BATT_GAUGE_INT_N,
	GPIO_BATT_GAUGE_EN,
	/* DOCKING expander */
	GPIO_DOCKING_EXPANDER_BASE = GPIO_EXPANDER_GPIO_BASE + 16,
	GPIO_MIPI_DSI_RST_N        = GPIO_DOCKING_EXPANDER_BASE,
	GPIO_AUX_JTAG_DET_N,
	GPIO_DONGLE_DET_N,
	GPIO_SVIDEO_LOAD_DET,
	GPIO_SVID_AMP_SHUTDOWN1_N,
	GPIO_SVID_AMP_SHUTDOWN0_N,
	GPIO_SDC_WP,
	GPIO_IRDA_PWDN,
	GPIO_IRDA_RESET_N,
	GPIO_DONGLE_GPIO0,
	GPIO_DONGLE_GPIO1,
	GPIO_DONGLE_GPIO2,
	GPIO_DONGLE_GPIO3,
	GPIO_DONGLE_PWR_EN,
	GPIO_EMMC_RESET_N,
	GPIO_TP_EXP2_IO15,
	/* SURF expander */
	GPIO_SURF_EXPANDER_BASE = GPIO_EXPANDER_GPIO_BASE + (16 * 2),
	GPIO_SD_CARD_DET_1      = GPIO_SURF_EXPANDER_BASE,
	GPIO_SD_CARD_DET_2,
	GPIO_SD_CARD_DET_4,
	GPIO_SD_CARD_DET_5,
	GPIO_UIM3_RST,
	GPIO_SURF_EXPANDER_IO5,
	GPIO_SURF_EXPANDER_IO6,
	GPIO_ADC_I2C_EN,
	GPIO_SURF_EXPANDER_IO8,
	GPIO_SURF_EXPANDER_IO9,
	GPIO_SURF_EXPANDER_IO10,
	GPIO_SURF_EXPANDER_IO11,
	GPIO_SURF_EXPANDER_IO12,
	GPIO_SURF_EXPANDER_IO13,
	GPIO_SURF_EXPANDER_IO14,
	GPIO_SURF_EXPANDER_IO15,
	/* LEFT KB IO expander */
	GPIO_LEFT_KB_EXPANDER_BASE = GPIO_EXPANDER_GPIO_BASE + (16 * 3),
	GPIO_LEFT_LED_1            = GPIO_LEFT_KB_EXPANDER_BASE,
	GPIO_LEFT_LED_2,
	GPIO_LEFT_LED_3,
	GPIO_LEFT_LED_WLAN,
	GPIO_JOYSTICK_EN,
	GPIO_CAP_TS_SLEEP,
	GPIO_LEFT_KB_IO6,
	GPIO_LEFT_LED_5,
	/* RIGHT KB IO expander */
	GPIO_RIGHT_KB_EXPANDER_BASE = GPIO_EXPANDER_GPIO_BASE + (16 * 3) + 8,
	GPIO_RIGHT_LED_1            = GPIO_RIGHT_KB_EXPANDER_BASE,
	GPIO_RIGHT_LED_2,
	GPIO_RIGHT_LED_3,
	GPIO_RIGHT_LED_BT,
	GPIO_WEB_CAMIF_STANDBY,
	GPIO_COMPASS_RST_N,
	GPIO_WEB_CAMIF_RESET_N,
	GPIO_RIGHT_LED_5,
	GPIO_R_ALTIMETER_RESET_N,
	/* FLUID S IO expander */
	GPIO_SOUTH_EXPANDER_BASE,
	GPIO_MIC2_ANCR_SEL = GPIO_SOUTH_EXPANDER_BASE,
	GPIO_MIC1_ANCL_SEL,
	GPIO_HS_MIC4_SEL,
	GPIO_FML_MIC3_SEL,
	GPIO_FMR_MIC5_SEL,
	GPIO_TS_SLEEP,
	GPIO_HAP_SHIFT_LVL_OE,
	GPIO_HS_SW_DIR,
	/* FLUID N IO expander */
	GPIO_NORTH_EXPANDER_BASE,
	GPIO_EPM_3_3V_EN = GPIO_NORTH_EXPANDER_BASE,
	GPIO_EPM_5V_BOOST_EN,
	GPIO_AUX_CAM_2P7_EN,
	GPIO_LED_FLASH_EN,
	GPIO_LED1_GREEN_N,
	GPIO_LED2_RED_N,
	GPIO_FRONT_CAM_RESET_N,
	GPIO_EPM_LVLSFT_EN,
	GPIO_N_ALTIMETER_RESET_N,
	/* EPM expander */
	GPIO_EPM_EXPANDER_BASE,
	GPIO_PWR_MON_START = GPIO_EPM_EXPANDER_BASE,
	GPIO_PWR_MON_RESET_N,
	GPIO_ADC1_PWDN_N,
	GPIO_ADC2_PWDN_N,
	GPIO_EPM_EXPANDER_IO4,
	GPIO_ADC1_MUX_SPI_INT_N_3_3V,
	GPIO_ADC2_MUX_SPI_INT_N,
	GPIO_EPM_EXPANDER_IO7,
	GPIO_PWR_MON_ENABLE,
	GPIO_EPM_SPI_ADC1_CS_N,
	GPIO_EPM_SPI_ADC2_CS_N,
	GPIO_EPM_EXPANDER_IO11,
	GPIO_EPM_EXPANDER_IO12,
	GPIO_EPM_EXPANDER_IO13,
	GPIO_EPM_EXPANDER_IO14,
	GPIO_EPM_EXPANDER_IO15,
};

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


#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT540E)
#define TOUCHSCREEN_IRQ 		125  
#define TSP_SDA					43
#define TSP_SCL					44
#endif

#ifdef CONFIG_MSM_CAMERA
#define	GPIO_CAM_IO_EN		37	
#define GPIO_ISP_INT		49
#define	GPIO_CAM_MAIN_RST	50
#define	GPIO_CAM_SUB_RST	41
#define	GPIO_CAM_SUB_EN		42
#endif

/*
 * The UI_INTx_N lines are pmic gpio lines which connect i2c
 * gpio expanders to the pm8058.
 */
#define UI_INT1_N 25
#define UI_INT2_N 34
#define UI_INT3_N 14
/*
FM GPIO is GPIO 18 on PMIC 8058.
As the index starts from 0 in the PMIC driver, and hence 17
corresponds to GPIO 18 on PMIC 8058.
*/
#define FM_GPIO 17
#define REV_GPIO_BASE 34 // 1 based numbering

static void sensor_power_on(void);
static void sensor_power_off(void);
static void sensor_power_on_mag(void);
static void sensor_power_off_mag(void);

#if defined (CONFIG_USA_MODEL_SGH_I757) || defined (CONFIG_CAN_MODEL_SGH_I757M)
static void taos_power_on(void);
static void taos_power_off(void);
#endif

static unsigned int HWREV;
static unsigned char is_checked_hwrev = 0;
#ifdef CONFIG_BATTERY_SEC
static unsigned int is_lpm_boot = 0;
#endif

static int target_baseband = 0; // default : csfb

static int __init target_baseband_setup(char *baseband)
{
	if (!strcmp(baseband, "csfb"))
		target_baseband = 0;
	else if (!strcmp(baseband, "svlte2a"))
		target_baseband = 1;

	return 1;
}
__setup("androidboot.baseband=", target_baseband_setup);

static int __init hw_rev_setup(char *str)
{
	if (!str)
		return 0;

	printk("%s : HW rev value is intialized first\n", __func__);

	HWREV = simple_strtoul(str, &str, 0);

	printk("HW rev value : 0x%X\n", HWREV);

	is_checked_hwrev = 1;

	return 1;
}
__setup("hw_rev=", hw_rev_setup);

#if defined (CONFIG_USA_MODEL_SGH_I717)
typedef struct _hw_rev_mapping {
     unsigned int real_hw_rev;
     unsigned int hw_rev;

} hw_rev_mapping;
#endif

unsigned int get_hw_rev(void)
{
	int i = 0;                
	char str_rev[10];
	int return_value;
	unsigned int temp_rev = 0;

#if defined (CONFIG_USA_MODEL_SGH_I717)
    const hw_rev_mapping hw_rev_map[] = {
        { .real_hw_rev = 0x05, .hw_rev = 0x00 },
        { .real_hw_rev = 0x03, .hw_rev = 0x01 },
        { .real_hw_rev = 0x02, .hw_rev = 0x02 },
        { .real_hw_rev = 0x04, .hw_rev = 0x03 },
        { .real_hw_rev = 0x06, .hw_rev = 0x04 },
        { .real_hw_rev = 0x07, .hw_rev = 0x05 },
        { .real_hw_rev = 0x08, .hw_rev = 0x07 },
        { .real_hw_rev = 0x09, .hw_rev = 0x0B },
        { .real_hw_rev = 0x0A, .hw_rev = 0x0C },
    };
#endif
	
	if(!is_checked_hwrev)
	{
		printk("%s : HW rev value is intialized first\n", __func__);


#if defined (CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
		for(i = 0; i < 3; i++)
		{
			sprintf(str_rev, "HW_REV_%d",i);
			return_value = gpio_request(PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(REV_GPIO_BASE+i)), str_rev);       
			return_value = gpio_direction_input(PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(REV_GPIO_BASE+i)));
			temp_rev = temp_rev | (gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(REV_GPIO_BASE+i)))<< (2-i));
		}

		if (temp_rev == 0) temp_rev = 9;
		else if (temp_rev == 1) temp_rev = 10;
#elif defined (CONFIG_USA_MODEL_SGH_I717)
		for(i = 0; i < 3; i++)
		{
			sprintf(str_rev, "HW_REV_%d",i);
			return_value = gpio_request(PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(REV_GPIO_BASE+i)), str_rev);
			return_value = gpio_direction_input(PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(REV_GPIO_BASE+i)));
			temp_rev = temp_rev | (gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(REV_GPIO_BASE+i)))<<i);
		}

		sprintf(str_rev, "HW_REV_%d", 3);
		return_value = gpio_request(PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(32)), str_rev);
		return_value = gpio_direction_input(PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(32)));

		temp_rev = temp_rev | (gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(32)))<<3);

        for(i=0; i < (sizeof(hw_rev_map) / sizeof(hw_rev_mapping)); i++) {
            if(temp_rev == hw_rev_map[i].real_hw_rev) {
                temp_rev = hw_rev_map[i].hw_rev;
                break;
            }
        }
#else
		temp_rev = 0xFFFFFFFF; // abnormal revision
#endif

#if defined (CONFIG_KOR_MODEL_SHV_E160L)
        temp_rev += 4;
#endif
		printk("HW rev value : 0x%X\n", temp_rev);
		HWREV = temp_rev;

		is_checked_hwrev = 1;
	}
	return HWREV;
}
EXPORT_SYMBOL(get_hw_rev);

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
static void (*sdc2_status_notify_cb)(int card_present, void *dev_id);
static void *sdc2_status_notify_cb_devid;
#endif

#ifdef CONFIG_MMC_MSM_SDC5_SUPPORT
static void (*sdc5_status_notify_cb)(int card_present, void *dev_id);
static void *sdc5_status_notify_cb_devid;
#endif

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
#if defined(CONFIG_JPN_MODEL_SC_05D)
			.max_uV = 1325000,
#else
			.max_uV = 1250000,
#endif
		},
		.consumer_supplies = vreg_consumers_8901_S0,
		.num_consumer_supplies = ARRAY_SIZE(vreg_consumers_8901_S0),
};

static struct regulator_init_data saw_s1_init_data = {
		.constraints = {
			.name = "8901_s1",
			.valid_ops_mask = REGULATOR_CHANGE_VOLTAGE,
			.min_uV = 800000,
#if defined(CONFIG_JPN_MODEL_SC_05D)
			.max_uV = 1325000,
#else
			.max_uV = 1250000,
#endif
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

/*
 * The smc91x configuration varies depending on platform.
 * The resources data structure is filled in at runtime.
 */
static struct resource smc91x_resources[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.flags = IORESOURCE_IRQ,
	},
};

static struct platform_device smc91x_device = {
	.name          = "smc91x",
	.id            = 0,
	.num_resources = ARRAY_SIZE(smc91x_resources),
	.resource      = smc91x_resources,
};

static struct resource smsc911x_resources[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		.start = 0x1b800000,
		.end   = 0x1b8000ff
	},
	[1] = {
		.flags = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWLEVEL,
	},
};

static struct smsc911x_platform_config smsc911x_config = {
	.irq_polarity	= SMSC911X_IRQ_POLARITY_ACTIVE_LOW,
	.irq_type	= SMSC911X_IRQ_TYPE_PUSH_PULL,
	.flags		= SMSC911X_USE_16BIT,
	.has_reset_gpio	= 1,
	.reset_gpio	= GPIO_ETHERNET_RESET_N
};

static struct platform_device smsc911x_device = {
	.name          = "smsc911x",
	.id            = 0,
	.num_resources = ARRAY_SIZE(smsc911x_resources),
	.resource      = smsc911x_resources,
	.dev           = {
		.platform_data = &smsc911x_config
	}
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

#endif /* CONFIG_CRYPTO_DEV_x */

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

#if defined(CONFIG_HAPTIC_ISA1200) || \
		defined(CONFIG_HAPTIC_ISA1200_MODULE)

static const char *vregs_isa1200_name[] = {
	"8058_s3",
	"8901_l4",
};

static const int vregs_isa1200_val[] = {
	1800000,/* uV */
	2600000,
};
static struct regulator *vregs_isa1200[ARRAY_SIZE(vregs_isa1200_name)];
static struct msm_xo_voter *xo_handle_a1;

static int isa1200_power(int vreg_on)
{
	int i, rc = 0;

	for (i = 0; i < ARRAY_SIZE(vregs_isa1200_name); i++) {
		rc = vreg_on ? regulator_enable(vregs_isa1200[i]) :
			regulator_disable(vregs_isa1200[i]);
		if (rc < 0) {
			pr_err("%s: vreg %s %s failed (%d)\n",
				__func__, vregs_isa1200_name[i],
				vreg_on ? "enable" : "disable", rc);
			goto vreg_fail;
		}
	}

	rc = vreg_on ? msm_xo_mode_vote(xo_handle_a1, MSM_XO_MODE_ON) :
			msm_xo_mode_vote(xo_handle_a1, MSM_XO_MODE_OFF);
	if (rc < 0) {
		pr_err("%s: failed to %svote for TCXO A1 buffer%d\n",
				__func__, vreg_on ? "" : "de-", rc);
		goto vreg_fail;
	}
	return 0;

vreg_fail:
	while (i--)
		!vreg_on ? regulator_enable(vregs_isa1200[i]) :
			regulator_disable(vregs_isa1200[i]);
	return rc;
}

static int isa1200_dev_setup(bool enable)
{
	int i, rc;

	if (enable == true) {
		for (i = 0; i < ARRAY_SIZE(vregs_isa1200_name); i++) {
			vregs_isa1200[i] = regulator_get(NULL,
						vregs_isa1200_name[i]);
			if (IS_ERR(vregs_isa1200[i])) {
				pr_err("%s: regulator get of %s failed (%ld)\n",
					__func__, vregs_isa1200_name[i],
					PTR_ERR(vregs_isa1200[i]));
				rc = PTR_ERR(vregs_isa1200[i]);
				goto vreg_get_fail;
			}
			rc = regulator_set_voltage(vregs_isa1200[i],
				vregs_isa1200_val[i], vregs_isa1200_val[i]);
			if (rc) {
				pr_err("%s: regulator_set_voltage(%s) failed\n",
					__func__, vregs_isa1200_name[i]);
				goto vreg_get_fail;
			}
		}

		rc = gpio_request(GPIO_HAP_SHIFT_LVL_OE, "haptics_shft_lvl_oe");
		if (rc) {
			pr_err("%s: unable to request gpio %d (%d)\n",
					__func__, GPIO_HAP_SHIFT_LVL_OE, rc);
			goto vreg_get_fail;
		}

		rc = gpio_direction_output(GPIO_HAP_SHIFT_LVL_OE, 1);
		if (rc) {
			pr_err("%s: Unable to set direction\n", __func__);;
			goto free_gpio;
		}

		xo_handle_a1 = msm_xo_get(MSM_XO_TCXO_A1, "isa1200");
		if (IS_ERR(xo_handle_a1)) {
			rc = PTR_ERR(xo_handle_a1);
			pr_err("%s: failed to get the handle for A1(%d)\n",
							__func__, rc);
			goto gpio_set_dir;
		}
	} else {
		gpio_set_value(GPIO_HAP_SHIFT_LVL_OE, 0);
		gpio_free(GPIO_HAP_SHIFT_LVL_OE);

		for (i = 0; i < ARRAY_SIZE(vregs_isa1200_name); i++)
			regulator_put(vregs_isa1200[i]);

		msm_xo_put(xo_handle_a1);
	}

	return 0;
gpio_set_dir:
	gpio_set_value(GPIO_HAP_SHIFT_LVL_OE, 0);
free_gpio:
	gpio_free(GPIO_HAP_SHIFT_LVL_OE);
vreg_get_fail:
	while (i)
		regulator_put(vregs_isa1200[--i]);
	return rc;
}

#define PMIC_GPIO_HAP_ENABLE   18  /* PMIC GPIO Number 19 */
#define PMIC_GPIO_HAP_LDO_ENABLE   5  /* PMIC GPIO Number 6 */
static struct isa1200_platform_data isa1200_1_pdata = {
	.name = "vibrator",
	.power_on = isa1200_power,
	.dev_setup = isa1200_dev_setup,
	/*gpio to enable haptic*/
	.hap_en_gpio = PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_HAP_ENABLE),
	.hap_len_gpio = PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_HAP_LDO_ENABLE),
	.max_timeout = 15000,
	.mode_ctrl = PWM_GEN_MODE,
	.pwm_fd = {
		.pwm_div = 256,
	},
	.is_erm = false,
	.smart_en = true,
	.ext_clk_en = true,
	.chip_en = 1,
};

static struct i2c_board_info msm_isa1200_board_info[] = {
	{
		I2C_BOARD_INFO("isa1200_1", 0x90>>1),
		.platform_data = &isa1200_1_pdata,
	},
};
#endif /* CONFIG_HAPTIC_ISA1200 */

#if defined(CONFIG_BATTERY_BQ27520) || \
		defined(CONFIG_BATTERY_BQ27520_MODULE)
static struct bq27520_platform_data bq27520_pdata = {
	.name		= "fuel-gauge",
	.vreg_name	= "8058_s3",
	.vreg_value	= 1800000,
	.soc_int	= GPIO_BATT_GAUGE_INT_N,
	.bi_tout	= GPIO_CAP_GAUGE_BI_TOUT,
	.chip_en	= GPIO_BATT_GAUGE_EN,
	.enable_dlog	= 0, /* if enable coulomb counter logger */
};

static struct i2c_board_info msm_bq27520_board_info[] = {
	{
		I2C_BOARD_INFO("bq27520", 0xaa>>1),
		.platform_data = &bq27520_pdata,
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

#if defined(CONFIG_USB_PEHCI_HCD) || defined(CONFIG_USB_PEHCI_HCD_MODULE)

#define ISP1763_INT_GPIO		117
#define ISP1763_RST_GPIO		152
static struct resource isp1763_resources[] = {
	[0] = {
		.flags	= IORESOURCE_MEM,
		.start	= 0x1D000000,
		.end	= 0x1D005FFF,		/* 24KB */
	},
	[1] = {
		.flags	= IORESOURCE_IRQ,
	},
};
static void __init msm8x60_cfg_isp1763(void)
{
	isp1763_resources[1].start = gpio_to_irq(ISP1763_INT_GPIO);
	isp1763_resources[1].end = gpio_to_irq(ISP1763_INT_GPIO);
}

static int isp1763_setup_gpio(int enable)
{
	int status = 0;

	if (enable) {
		status = gpio_request(ISP1763_INT_GPIO, "isp1763_usb");
		if (status) {
			pr_err("%s:Failed to request GPIO %d\n",
						__func__, ISP1763_INT_GPIO);
			return status;
		}
		status = gpio_direction_input(ISP1763_INT_GPIO);
		if (status) {
			pr_err("%s:Failed to configure GPIO %d\n",
					__func__, ISP1763_INT_GPIO);
			goto gpio_free_int;
		}
		status = gpio_request(ISP1763_RST_GPIO, "isp1763_usb");
		if (status) {
			pr_err("%s:Failed to request GPIO %d\n",
						__func__, ISP1763_RST_GPIO);
			goto gpio_free_int;
		}
		status = gpio_direction_output(ISP1763_RST_GPIO, 1);
		if (status) {
			pr_err("%s:Failed to configure GPIO %d\n",
					__func__, ISP1763_RST_GPIO);
			goto gpio_free_rst;
		}
		pr_debug("\nISP GPIO configuration done\n");
		return status;
	}

gpio_free_rst:
	gpio_free(ISP1763_RST_GPIO);
gpio_free_int:
	gpio_free(ISP1763_INT_GPIO);

	return status;
}
static struct isp1763_platform_data isp1763_pdata = {
	.reset_gpio	= ISP1763_RST_GPIO,
	.setup_gpio	= isp1763_setup_gpio
};

static struct platform_device isp1763_device = {
	.name          = "isp1763_usb",
	.num_resources = ARRAY_SIZE(isp1763_resources),
	.resource      = isp1763_resources,
	.dev           = {
		.platform_data = &isp1763_pdata
	}
};
#endif /* CONFIG_USB_PEHCI_HCD */

#if defined(CONFIG_USB_GADGET_MSM_72K) || defined(CONFIG_USB_EHCI_MSM_72K)
static struct msm_otg_platform_data msm_otg_pdata;
static struct regulator *ldo6_3p3;
static struct regulator *ldo7_1p8;
static struct regulator *vdd_cx;
#define PMICID_INT		PM8058_GPIO_IRQ(PM8058_IRQ_BASE, 36)
#define PMIC_ID_GPIO		36
notify_vbus_state notify_vbus_state_func_ptr;
static int usb_phy_susp_dig_vol = 750000;
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

	struct pm_gpio pmic_id_cfg = {
		.direction	= PM_GPIO_DIR_IN,
		.pull		= PM_GPIO_PULL_UP_1P5,
		.function	= PM_GPIO_FUNC_NORMAL,
		.vin_sel	= 2,
		.inv_int_pol	= 0,
	};
	struct pm_gpio pmic_id_uncfg = {
		.direction	= PM_GPIO_DIR_IN,
		.pull		= PM_GPIO_PULL_NO,
		.function	= PM_GPIO_FUNC_NORMAL,
		.vin_sel	= 2,
		.inv_int_pol	= 0,
	};
	if (!callback)
		return -EINVAL;

	if (machine_is_msm8x60_fluid())
		return -ENOTSUPP;

	if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) != 2) {
		pr_debug("%s: USB_ID pin is not routed to PMIC"
					"on V1 surf/ffa\n", __func__);
		return -ENOTSUPP;
	}

	if ((machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa() ||
			machine_is_msm8x60_ffa()) && !pmic_id_notif_supported) {
		pr_debug("%s: USB_ID is not routed to PMIC"
			"on V2 ffa\n", __func__);
		return -ENOTSUPP;
	}

#if defined (CONFIG_KOR_MODEL_SHV_E110S)
	if(get_hw_rev() < 6) {
		pr_err("%s: USB_ID is not routed to PMIC\n", __func__);
		return -ENOTSUPP;
	}
#else
	pr_err("%s: USB_ID is not routed to PMIC\n", __func__);
	return -ENOTSUPP;
#endif

	usb_phy_susp_dig_vol = 500000;

	if (init) {
		notify_vbus_state_func_ptr = callback;
		INIT_DELAYED_WORK(&pmic_id_det, pmic_id_detect);
		ret = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_ID_GPIO),
							&pmic_id_cfg);
		if (ret) {
			pr_err("%s:return val of pm8xxx_gpio_config: %d\n",
						__func__,  ret);
			return ret;
		}
		ret = request_threaded_irq(PMICID_INT, NULL, pmic_id_on_irq,
			(IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING),
						"msm_otg_id", NULL);
		if (ret) {
			pr_err("%s:pmic_usb_id interrupt registration failed",
					__func__);
			return ret;
		}
		msm_otg_pdata.pmic_id_irq = PMICID_INT;
	} else {
		usb_phy_susp_dig_vol = 750000;
		free_irq(PMICID_INT, 0);
		ret = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_ID_GPIO),
							&pmic_id_uncfg);
		if (ret) {
			pr_err("%s: return val of pm8xxx_gpio_config: %d\n",
						__func__,  ret);
			return ret;
		}
		msm_otg_pdata.pmic_id_irq = 0;
		cancel_delayed_work_sync(&pmic_id_det);
		notify_vbus_state_func_ptr = NULL;
	}
	return 0;
}
#endif /* CONFIG_USB_EHCI_MSM_72K (nested in CONFIG_USB_GADGET_MSM_72K || CONFIG_USB_EHCI_MSM_72K) */

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

#define USB_PHY_3P3_VOL_MIN	3600000 // 11.06.25 QC recommended for stability test //3050000 /* uV */
#define USB_PHY_3P3_VOL_MAX	3600000 // 11.06.25 QC recommended for stability test //3050000 /* uV */
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
static int msm_hsusb_ldo_set_voltage(int mV)
{
	static int cur_voltage = 3600000;

	if (!ldo6_3p3 || IS_ERR(ldo6_3p3))
		return -ENODEV;

	if (cur_voltage == mV)
		return 0;

	cur_voltage = mV;

	pr_debug("%s: (%d)\n", __func__, mV);

	return regulator_set_voltage(ldo6_3p3, mV,mV);
}
#endif /* CONFIG_USB_GADGET_MSM_72K || CONFIG_USB_EHCI_MSM_72K */

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
#if defined(CONFIG_JPN_MODEL_SC_05D)
static void msm_hsusb_vbus_power(unsigned phy_info, int on)
{
	static struct regulator *votg_5v_switch;
	static struct regulator *ext_5v_reg;
	static int vbus_is_on;

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

	vbus_is_on = on;
}
#endif

static struct msm_usb_host_platform_data msm_usb_host_pdata = {
	.phy_info	= (USB_PHY_INTEGRATED | USB_PHY_MODEL_45NM),
	.power_budget	= 500,  // for samsung otg 390 -> 500
#ifdef CONFIG_USB_HOST_NOTIFY
	.host_notify = 1,
#endif
#ifdef CONFIG_USB_SEC_WHITELIST
	.sec_whlist_table_num = 1,
#endif	
};
#endif /* CONFIG_USB_EHCI_MSM_72K */

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
#endif /* CONFIG_BATTERY_MSM8X60 */

static void msm_hsusb_smb328a_vbus_power(unsigned phy_info,int attached)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;
	int ret = 0;

	static int vbus_is_on;
	
	printk(KERN_ERR "msm_hsusb_smb328a_vbus_power attached %d\n", attached);
	/* If VBUS is already on (or off), do nothing. */
	if (attached == vbus_is_on)
		return;

	if (attached)
		value.intval = POWER_SUPPLY_CAPACITY_OTG_ENABLE;
	else
		value.intval = POWER_SUPPLY_CAPACITY_OTG_DISABLE;

	if (psy) {
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_OTG, &value);
		if (ret) {
			pr_err("%s: fail to set power_suppy otg property(%d)\n",
				__func__, ret);
			goto fail;
		}
	} else {
		pr_err("%s : psy is null!\n", __func__);
		goto fail;
	}
	vbus_is_on = attached;
	return;
fail:
	vbus_is_on = false;
	return;
}

#if defined(CONFIG_USB_GADGET_MSM_72K) || defined(CONFIG_USB_EHCI_MSM_72K)
static struct msm_otg_platform_data msm_otg_pdata = {
	/* if usb link is in sps there is no need for
	 * usb pclk as dayatona fabric clock will be
	 * used instead
	 */
	.pemp_level		 = PRE_EMPHASIS_WITH_20_PERCENT,
	.cdr_autoreset		 = CDR_AUTO_RESET_DISABLE,
	.drv_ampl			 = HS_DRV_AMPLITUDE_75_PERCENT, // for voltage driving
	.se1_gating		 = SE1_GATING_DISABLE,
	.bam_disable		 = 1,
#ifdef CONFIG_USB_EHCI_MSM_72K
	.pmic_id_notif_init = msm_hsusb_pmic_id_notif_init,
	.phy_id_setup_init = msm_hsusb_phy_id_setup_init,
#endif
#ifdef CONFIG_USB_EHCI_MSM_72K
//	.vbus_power = msm_hsusb_vbus_power,
	.vbus_power = msm_hsusb_smb328a_vbus_power,
#endif
#ifdef CONFIG_BATTERY_MSM8X60
//	.pmic_vbus_notif_init	= msm_hsusb_pmic_vbus_notif_init,
	.pmic_vbus_notif_init	= NULL,
#endif
	.ldo_init		 = msm_hsusb_ldo_init,
	.ldo_enable		 = msm_hsusb_ldo_enable,
	.config_vddcx            = msm_hsusb_config_vddcx,
	.init_vddcx              = msm_hsusb_init_vddcx,
#ifdef CONFIG_BATTERY_MSM8X60
//	.chg_vbus_draw = msm_charger_vbus_draw,
	.chg_vbus_draw = NULL,
#endif
	.ldo_set_voltage=msm_hsusb_ldo_set_voltage,
};
#endif /* CONFIG_USB_GADGET_MSM_72K || CONFIG_USB_EHCI_MSM_72K */

#ifdef CONFIG_USB_GADGET_MSM_72K
static struct msm_hsusb_gadget_platform_data msm_gadget_pdata = {
	.is_phy_status_timer_on = 1,
#ifdef CONFIG_USB_SWITCH_FSA9480
	.check_microusb = fsa9480_check_device, // Add for fsa9485 device check (Samsung)
#endif
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

#endif /* CONFIG_USB_G_ANDROID */

#ifdef CONFIG_MSM_VPE
#ifndef CONFIG_MSM_CAMERA_V4L2
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
#endif


#ifdef CONFIG_MSM_CAMERA // Huge code block, more than 1500 lines long (what were they thinking?!?)
#ifndef CONFIG_MSM_CAMERA_V4L2
#ifdef CONFIG_MSM_CAMERA_FLASH
#define VFE_CAMIF_TIMER1_GPIO 29
#define VFE_CAMIF_TIMER2_GPIO 30
#define VFE_CAMIF_TIMER3_GPIO_INT 31
#define FUSION_VFE_CAMIF_TIMER1_GPIO 42
static struct msm_camera_sensor_flash_src msm_flash_src = {
	.flash_sr_type = MSM_CAMERA_FLASH_SRC_PMIC,
	._fsrc.pmic_src.num_of_src = 2,
	._fsrc.pmic_src.low_current  = 100,
	._fsrc.pmic_src.high_current = 300,
	._fsrc.pmic_src.led_src_1 = PMIC8058_ID_FLASH_LED_0,
	._fsrc.pmic_src.led_src_2 = PMIC8058_ID_FLASH_LED_1,
	._fsrc.pmic_src.pmic_set_current = pm8058_set_flash_led_current,
};
#ifdef CONFIG_IMX074
static struct msm_camera_sensor_strobe_flash_data strobe_flash_xenon = {
	.flash_trigger = VFE_CAMIF_TIMER2_GPIO,
	.flash_charge = VFE_CAMIF_TIMER1_GPIO,
	.flash_charge_done = VFE_CAMIF_TIMER3_GPIO_INT,
	.flash_recharge_duration = 50000,
	.irq = MSM_GPIO_TO_INT(VFE_CAMIF_TIMER3_GPIO_INT),
};
#endif
#endif

int msm_cam_gpio_tbl[] = {
	32,/*CAMIF_MCLK*/
	47,/*CAMIF_I2C_DATA*/
	48,/*CAMIF_I2C_CLK*/
	105,/*STANDBY*/
};

enum msm_cam_stat{
	MSM_CAM_OFF,
	MSM_CAM_ON,
};

static int config_gpio_table(enum msm_cam_stat stat)
{
	int rc = 0, i = 0;
	if (stat == MSM_CAM_ON) {
		for (i = 0; i < ARRAY_SIZE(msm_cam_gpio_tbl); i++) {
			rc = gpio_request(msm_cam_gpio_tbl[i], "CAM_GPIO");
			if (unlikely(rc < 0)) {
				pr_err("%s not able to get gpio\n", __func__);
				for (i--; i >= 0; i--)
					gpio_free(msm_cam_gpio_tbl[i]);
				break;
			}
		}
	} else {
		for (i = 0; i < ARRAY_SIZE(msm_cam_gpio_tbl); i++)
			gpio_free(msm_cam_gpio_tbl[i]);
	}
	return rc;
}

#ifdef CONFIG_IMX074
static struct msm_camera_sensor_platform_info sensor_board_info = {
	.mount_angle = 0
};
#endif

#ifdef CONFIG_QS_S5K4E1

/*external regulator VREG_5V*/
static struct regulator *reg_flash_5V;

static int config_camera_on_gpios_fluid(void)
{
	int rc = 0;

	reg_flash_5V = regulator_get(NULL, "8901_mpp0");
	if (IS_ERR(reg_flash_5V)) {
		pr_err("'%s' regulator not found, rc=%ld\n",
				"8901_mpp0", IS_ERR(reg_flash_5V));
		return -ENODEV;
	}

	rc = regulator_enable(reg_flash_5V);
	if (rc) {
		pr_err("'%s' regulator enable failed, rc=%d\n",
			"8901_mpp0", rc);
		regulator_put(reg_flash_5V);
		return rc;
	}

#ifdef CONFIG_IMX074
	sensor_board_info.mount_angle = 90;
#endif
	rc = config_gpio_table(MSM_CAM_ON);
	if (rc < 0) {
		printk(KERN_ERR "%s: CAMSENSOR gpio table request"
		"failed\n", __func__);
		return rc;
	}

	rc = gpio_request(GPIO_EXT_CAMIF_PWR_EN, "CAM_EN");
	if (rc < 0) {
		printk(KERN_ERR "%s: CAMSENSOR gpio %d request"
			"failed\n", __func__, GPIO_EXT_CAMIF_PWR_EN);
		regulator_disable(reg_flash_5V);
		regulator_put(reg_flash_5V);
		return rc;
	}
	gpio_direction_output(GPIO_EXT_CAMIF_PWR_EN, 0);
	msleep(20);
	gpio_set_value_cansleep(GPIO_EXT_CAMIF_PWR_EN, 1);


	/*Enable LED_FLASH_EN*/
	rc = gpio_request(GPIO_LED_FLASH_EN, "LED_FLASH_EN");
	if (rc < 0) {
		printk(KERN_ERR "%s: CAMSENSOR gpio %d request"
			"failed\n", __func__, GPIO_LED_FLASH_EN);

		regulator_disable(reg_flash_5V);
		regulator_put(reg_flash_5V);
		config_gpio_table(MSM_CAM_OFF);
		gpio_set_value_cansleep(GPIO_EXT_CAMIF_PWR_EN, 0);
		gpio_free(GPIO_EXT_CAMIF_PWR_EN);
		return rc;
	}
	gpio_direction_output(GPIO_LED_FLASH_EN, 1);
	msleep(20);
	return rc;
}


static void config_camera_off_gpios_fluid(void)
{
	regulator_disable(reg_flash_5V);
	regulator_put(reg_flash_5V);

	gpio_direction_output(GPIO_LED_FLASH_EN, 0);
	gpio_free(GPIO_LED_FLASH_EN);

	config_gpio_table(MSM_CAM_OFF);

	gpio_set_value_cansleep(GPIO_EXT_CAMIF_PWR_EN, 0);
	gpio_free(GPIO_EXT_CAMIF_PWR_EN);
}
#endif

static uint32_t camera_off_gpio_table[] = {
	/* parallel CAMERA interfaces */
	GPIO_CFG(50,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* ISP_RST */
	GPIO_CFG(37,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), /* CAM_IO_EN */
	GPIO_CFG(41,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_VGA_RST */	
	GPIO_CFG(42,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_VGA_EN */	
	GPIO_CFG(32,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_MCLK_F */
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	GPIO_CFG(106, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /*FLASH_SEL*/
#endif
};

static uint32_t camera_on_gpio_table[] = {
	/* parallel CAMERA interfaces */
	GPIO_CFG(50,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* ISP_RST */
	GPIO_CFG(41,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), /* CAM_VGA_RST */	
	GPIO_CFG(37,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_IO_EN */
	GPIO_CFG(42,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /* CAM_VGA_EN */	
	GPIO_CFG(32,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), /* CAM_MCLK */
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	GPIO_CFG(106, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), /*FLASH_SEL*/
#endif
};

static void config_gpio_tables(uint32_t *table, int len)
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
	config_gpio_tables(camera_on_gpio_table,
		ARRAY_SIZE(camera_on_gpio_table));
	return 0;
}

static void config_camera_off_gpios(void)
{
	config_gpio_tables(camera_off_gpio_table,
		ARRAY_SIZE(camera_off_gpio_table));

}


#ifdef CONFIG_QS_S5K4E1

#define QS_CAM_HC37_CAM_PD PM8058_GPIO_PM_TO_SYS(26)

static int config_camera_on_gpios_qs_cam_fluid(void)
{
	int rc = 0;

	/* request QS_CAM_HC37_CAM_PD as an output to HC37 ASIC pin CAM_PD */
	rc = gpio_request(QS_CAM_HC37_CAM_PD, "QS_CAM_HC37_CAM_PD");
	if (rc < 0) {
		printk(KERN_ERR "%s: QS_CAM_HC37_CAM_PD gpio %d request"
			" failed\n", __func__, QS_CAM_HC37_CAM_PD);
		return rc;
	}
	gpio_direction_output(QS_CAM_HC37_CAM_PD, 0);
	msleep(20);
	gpio_set_value_cansleep(QS_CAM_HC37_CAM_PD, 1);
	msleep(20);

	/*
	 * Set GPIO_AUX_CAM_2P7_EN to 1 on North Expander IO2
	 * to enable 2.7V power to Camera
	 */
	rc = gpio_request(GPIO_AUX_CAM_2P7_EN, "CAM_2P7_EN");
	if (rc < 0) {
		printk(KERN_ERR "%s: CAMSENSOR gpio %d request"
			" failed\n", __func__, GPIO_AUX_CAM_2P7_EN);
		gpio_set_value_cansleep(QS_CAM_HC37_CAM_PD, 0);
		gpio_free(QS_CAM_HC37_CAM_PD);
		return rc;
	}
	gpio_direction_output(GPIO_AUX_CAM_2P7_EN, 0);
	msleep(20);
	gpio_set_value_cansleep(GPIO_AUX_CAM_2P7_EN, 1);
	msleep(20);

	rc = config_camera_on_gpios_fluid();
	if (rc < 0) {
		printk(KERN_ERR "%s: config_camera_on_gpios_fluid"
		" failed\n", __func__);
		gpio_set_value_cansleep(QS_CAM_HC37_CAM_PD, 0);
		gpio_free(QS_CAM_HC37_CAM_PD);
		gpio_set_value_cansleep(GPIO_AUX_CAM_2P7_EN, 0);
		gpio_free(GPIO_AUX_CAM_2P7_EN);
		return rc;
	}
	return rc;
}

static void config_camera_off_gpios_qs_cam_fluid(void)
{
	/*
	 * Set GPIO_AUX_CAM_2P7_EN to 0 on North Expander IO2
	 * to disable 2.7V power to Camera
	 */
	gpio_set_value_cansleep(GPIO_AUX_CAM_2P7_EN, 0);
	gpio_free(GPIO_AUX_CAM_2P7_EN);

	/* set QS_CAM_HC37_CAM_PD to 0 to power off HC37 ASIC*/
	gpio_set_value_cansleep(QS_CAM_HC37_CAM_PD, 0);
	gpio_free(QS_CAM_HC37_CAM_PD);

	config_camera_off_gpios_fluid();
	return;
}

static int config_camera_on_gpios_qs_cam(void)
{
	int rc = 0;

	if (machine_is_msm8x60_fluid())
		return config_camera_on_gpios_qs_cam_fluid();

	rc = config_camera_on_gpios();
	return rc;
}

static void config_camera_off_gpios_qs_cam(void)
{
	if (machine_is_msm8x60_fluid())
		return config_camera_off_gpios_qs_cam_fluid();

	config_camera_off_gpios();
	return;
}
#endif

static int config_camera_on_gpios_web_cam(void)
{
	int rc = 0;
#if !defined (CONFIG_JPN_MODEL_SC_05D)
	rc = config_gpio_table(MSM_CAM_ON);
	if (rc < 0) {
		printk(KERN_ERR "%s: CAMSENSOR gpio table request"
		"failed\n", __func__);
		return rc;
	}

	if (!(machine_is_msm8x60_fluid() || machine_is_msm8x60_dragon())) {
		rc = gpio_request(GPIO_WEB_CAMIF_STANDBY, "CAM_EN");
		if (rc < 0) {
			config_gpio_table(MSM_CAM_OFF);
			pr_err(KERN_ERR "%s: CAMSENSOR gpio %d request"
				"failed\n", __func__, GPIO_WEB_CAMIF_STANDBY);
			return rc;
		}
		gpio_direction_output(GPIO_WEB_CAMIF_STANDBY, 0);
	}
#endif
	return rc;
}

static void config_camera_off_gpios_web_cam(void)
{
#if !defined (CONFIG_JPN_MODEL_SC_05D)
	config_gpio_table(MSM_CAM_OFF);
	if (!(machine_is_msm8x60_fluid() || machine_is_msm8x60_dragon())) {
		gpio_set_value_cansleep(GPIO_WEB_CAMIF_STANDBY, 1);
		gpio_free(GPIO_WEB_CAMIF_STANDBY);
	}
	return;
#endif
}

static struct regulator *i_core12, *s_core12, *s_io18, *i_host18, *af28, *vt_core15;
static int camera_power_maincam(int onoff)
{
	int ret = 0;
	printk("%s :%s\n", __func__, onoff ? "ON" : "OFF");

	//gpio_set_value_cansleep(GPIO_CAM_IO_EN, LOW);  //HOST 1.8V
	//mdelay(5);
	if (onoff) {
		//ISP CORE 1.2V		
		i_core12 = regulator_get(NULL, "8901_s2"); //CORE 1.2V
		if (IS_ERR(i_core12))
			goto cam_power_fail;
			
		ret = regulator_set_voltage(i_core12, 1200000, 1200000);
		if (ret) {
			printk("%s:i_core12 error setting voltage\n", __func__);
		}
		ret = regulator_enable(i_core12);
		if (ret) {
			printk("%s:i_core12 error enabling regulator\n", __func__);
		}
		usleep(1*1000);


		//SENSOR CORE 1.2V
		s_core12 = regulator_get(NULL, "8901_lvs1"); 
		if (IS_ERR(s_core12))
			goto cam_power_fail;

		ret = regulator_enable(s_core12);
		if (ret) {
			printk("%s:s_core12 error enabling regulator\n", __func__);
		}
		usleep(1*1000);
		
		//SENSOR A2.8V
		gpio_set_value_cansleep(GPIO_CAM_IO_EN, 1);  
		usleep(1*1000);  //min 20us

#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) // flash select
		if (get_hw_rev()>=0x0A) {
			if (gpio_get_value(62)==0)
				gpio_set_value_cansleep(GPIO_FLASH_SEL, 1);
			usleep(1*1000);
		}
#elif defined(CONFIG_KOR_MODEL_SHV_E160L)
		if (get_hw_rev()>=0x08) {
			if (gpio_get_value(62)==0)
				gpio_set_value_cansleep(GPIO_FLASH_SEL, 1);
			usleep(1*1000);
		}
#elif defined(CONFIG_JPN_MODEL_SC_05D)
			if (gpio_get_value(62)==0)
				gpio_set_value_cansleep(GPIO_FLASH_SEL, 1);
			usleep(1*1000);
#endif

		//DVDD 1.5V (sub)
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_KOR_MODEL_SHV_E160K)  || defined (CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D) //QuincyS
		if (get_hw_rev() >= 0x02)
			vt_core15 = regulator_get(NULL, "8058_l24"); 
		else
#elif defined (CONFIG_USA_MODEL_SGH_I717)// Q1 ATT
		if (get_hw_rev() >= 0x01) //Q1_REV01
			vt_core15 = regulator_get(NULL, "8058_l24"); 
		else
#endif
		{
			printk("DVDD1.5V : 8058_l10\n");
			vt_core15 = regulator_get(NULL, "8058_l10"); 
		}
		if (IS_ERR(vt_core15))
			goto cam_power_fail;

		ret = regulator_set_voltage(vt_core15, 1500000, 1500000);
		if (ret) {
			printk("%s:vt_core15 error setting voltage\n", __func__);
		}
		ret = regulator_enable(vt_core15);
		if (ret) {
			printk("%s:vt_core15 error enabling regulator\n", __func__);
		}
		usleep(1*1000);   //min 15us


		//AF 2.8V
		printk("AF 2.8V\n");
		af28 = regulator_get(NULL, "8058_l15"); //AF 2.8V

		if (IS_ERR(af28))
			goto cam_power_fail;
			
		ret = regulator_set_voltage(af28, 2850000, 2850000);
		if (ret) {
			printk("%s:af28 error setting voltage\n", __func__);
		}
		ret = regulator_enable(af28);
		if (ret) {
			printk("%s:af28 error enabling regulator\n", __func__);
		}
		mdelay(5);  // min 5ms~max 10ms, 

		
		//HOST 1.8V
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_KOR_MODEL_SHV_E160K)  || defined (CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
		if (get_hw_rev() >= 0x02)//dali LGT REV02
			i_host18 = regulator_get(NULL, "8901_usb_otg");
		else
#elif defined (CONFIG_USA_MODEL_SGH_I717)
		if (get_hw_rev() >= 0x01) //Q1_REV01
			i_host18 = regulator_get(NULL, "8901_usb_otg");
		else
#endif
		{
			printk("Host1.8V : 8058_l8\n");
			i_host18 = regulator_get(NULL, "8058_l8");  
			if (IS_ERR(i_host18))
				goto cam_power_fail;
				
			ret = regulator_set_voltage(i_host18, 1800000, 1800000);
			if (ret) {
				printk("%s:i_host18 error setting voltage\n", __func__);
			}
				
		}
		if (IS_ERR(i_host18))
			goto cam_power_fail;

		ret = regulator_enable(i_host18);
		if (ret) {
			printk("%s:i_host18 error enabling regulator\n", __func__);
		}
		usleep(1*1000); 
			
			
		//SENSOR IO 1.8V  - ISP
		printk("SENSOR IO 1.8V\n");
		s_io18 = regulator_get(NULL, "8058_lvs0");
		if (IS_ERR(s_io18))
			goto cam_power_fail;
			
		ret = regulator_enable(s_io18);
		if (ret) {
			printk("%s:s_io18 error enabling regulator\n", __func__);
		}
		usleep(1*1000); 

	}
	else {

		usleep(1*1000); 

		if (regulator_is_enabled(af28)) {
			ret=regulator_disable(af28);
			if (ret) {
				printk("%s:af28 error disabling regulator\n", __func__);
			}
		}
		usleep(1*1000); 

		//SENSOR IO 1.8V  - ISP
		if (regulator_is_enabled(s_io18)) {
			ret = regulator_disable(s_io18);
			if (ret) {
				printk("%s:s_io18 error disabling regulator\n", __func__);
			}
		}
		usleep(1*1000); 
		
		//HOST 1.8V	
		if (regulator_is_enabled(i_host18)) {
			ret=regulator_disable(i_host18);
			if (ret) {
				printk("%s:i_host18 error disabling regulator\n", __func__);
			}
		}
		usleep(1*1000); 
		
		//DVDD 1.5V (sub)		
		if (regulator_is_enabled(vt_core15)) {
			ret=regulator_disable(vt_core15);
			if (ret) {
				printk("%s:vt_core15 error disabling regulator\n", __func__);
			}
		}	
		usleep(1*1000); 
		
		//SENSOR A2.8V
		gpio_set_value_cansleep(GPIO_CAM_IO_EN, 0);  //HOST 1.8V
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
		gpio_set_value_cansleep(GPIO_FLASH_SEL, 0);
#endif
		usleep(1*1000); 
		
		//SENSOR CORE 1.2V	
		if (regulator_is_enabled(s_core12)) {
			ret=regulator_disable(s_core12);
			if (ret) {
				printk("%s:s_core12 error disabling regulator\n", __func__);
			}
		}
		usleep(1*1000); 

		//ISP CORE 1.2V
		if (regulator_is_enabled(i_core12)) {
			ret=regulator_disable(i_core12);
			if (ret) {
				printk("%s:i_core12 error disabling regulator\n", __func__);
			}
		}
		mdelay(5);

	}
	
	return 0;

	cam_power_fail:
		printk("main cam fail :%s \n", onoff ? "ON" : "OFF");
		return -1;
}



static int camera_power_vtcam(int onoff)
{
	int ret = 0;
	int enable_io18 = 0; // i2c pullup (0;sensorIO 1.8V  -> 1;host 1.8V)
	printk("%s :%s\n", __func__, onoff ? "ON" : "OFF");

	
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_KOR_MODEL_SHV_E160K)  || defined (CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	if (get_hw_rev()< 0x2) //celoxS_REV03, DaliS_REV02
		enable_io18 = 1;
#elif defined (CONFIG_USA_MODEL_SGH_I717)
	if (get_hw_rev()< 0x01) //Q1_REV01			
                 enable_io18 = 1;
#endif
	if (onoff) {
		//ISP CORE 1.2V		
		i_core12 = regulator_get(NULL, "8901_s2"); //CORE 1.2V
		if (IS_ERR(i_core12))
			goto cam_power_fail;
			
		ret = regulator_set_voltage(i_core12, 1200000, 1200000);
		if (ret) {
			printk("%s:i_core12 error setting voltage\n", __func__);
		}
		ret = regulator_enable(i_core12);
		if (ret) {
			printk("%s:i_core12 error enabling regulator\n", __func__);
		}
		usleep(1*1000);


		//SENSOR A2.8V
		gpio_set_value_cansleep(GPIO_CAM_IO_EN, 1);  
		usleep(1*1000);  //min 20us


		//DVDD 1.5V (sub)
#if defined(CONFIG_KOR_MODEL_SHV_E160S)  || defined (CONFIG_KOR_MODEL_SHV_E160K)  || defined (CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
		if (get_hw_rev() >= 0x02) //dali LGT REV02, QuincyS_REV02
			vt_core15 = regulator_get(NULL, "8058_l24"); 
		else
#elif defined (CONFIG_USA_MODEL_SGH_I717)
		if (get_hw_rev() >= 0x01) //Q1_REV01
			vt_core15 = regulator_get(NULL, "8058_l24"); 
		else
#endif
		{
			printk("DVDD1.5V : 8058_l10\n");
			vt_core15 = regulator_get(NULL, "8058_l10"); 
		}
		if (IS_ERR(vt_core15))
			goto cam_power_fail;

		ret = regulator_set_voltage(vt_core15, 1500000, 1500000);
		if (ret) {
			printk("%s:vt_core15 error setting voltage\n", __func__);
		}
		ret = regulator_enable(vt_core15);
		if (ret) {
			printk("%s:vt_core15 error enabling regulator\n", __func__);
		}
		usleep(1*1000);   //min 15us

		
		//HOST 1.8V
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_KOR_MODEL_SHV_E160K)  || defined (CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
		if (get_hw_rev() >= 0x02)//dali LGT REV02
			i_host18 = regulator_get(NULL, "8901_usb_otg");
		else
#elif defined (CONFIG_USA_MODEL_SGH_I717)
		if (get_hw_rev() >= 0x01) //Q1_REV01
			i_host18 = regulator_get(NULL, "8901_usb_otg");
		else
#endif
		{
			printk("Host1.8V : 8058_l8\n");
			i_host18 = regulator_get(NULL, "8058_l8");  
			if (IS_ERR(i_host18))
				goto cam_power_fail;
				
			ret = regulator_set_voltage(i_host18, 1800000, 1800000);
			if (ret) {
				printk("%s:i_host18 error setting voltage\n", __func__);
			}
				
		}
		if (IS_ERR(i_host18))
			goto cam_power_fail;

		ret = regulator_enable(i_host18);
		if (ret) {
			printk("%s:i_host18 error enabling regulator\n", __func__);
		}
		usleep(1*1000); 
			
			
		//SENSOR IO 1.8V  - ISP
		// i2c pullup (1;sensorIO 1.8V  -> 0;host 1.8V)
		if (enable_io18) {
			printk("SENSOR IO 1.8V\n");
			s_io18 = regulator_get(NULL, "8058_lvs0");
			if (IS_ERR(s_io18))
				goto cam_power_fail;
			
			ret = regulator_enable(s_io18);
			if (ret) {
				printk("%s:s_io18 error enabling regulator\n", __func__);
			}
			usleep(1*1000); 
		}

		gpio_set_value_cansleep(GPIO_CAM_SUB_EN, 1); // STBY		
		usleep(1*1000); 

	}
	else {
		gpio_set_value_cansleep(GPIO_CAM_SUB_EN, 0); // STBY		
		usleep(1*1000); 
		
		if (enable_io18) {
			//SENSOR IO 1.8V  - ISP
			if (regulator_is_enabled(s_io18)) {
				ret = regulator_disable(s_io18);
				if (ret) {
					printk("%s:s_io18 error disabling regulator\n", __func__);
				}
			}
			usleep(1*1000); 
		}
		
		//HOST 1.8V	
		if (regulator_is_enabled(i_host18)) {
			ret=regulator_disable(i_host18);
			if (ret) {
				printk("%s:i_host18 error disabling regulator\n", __func__);
			}
		}
		usleep(1*1000); 
		
		//DVDD 1.5V (sub)		
		if (regulator_is_enabled(vt_core15)) {
			ret=regulator_disable(vt_core15);
			if (ret) {
				printk("%s:vt_core15 error disabling regulator\n", __func__);
			}
		}	
		usleep(1*1000); 
		
		//SENSOR A2.8V
		gpio_set_value_cansleep(GPIO_CAM_IO_EN, 0);  //HOST 1.8V
		usleep(1*1000); 
		
		
		//ISP CORE 1.2V
		if (regulator_is_enabled(i_core12)) {
			ret=regulator_disable(i_core12);
			if (ret) {
				printk("%s:i_core12 error disabling regulator\n", __func__);
			}
		}
		mdelay(5);
	}
	return 0;
	
	cam_power_fail:
		printk("vt cam fail :%s \n", onoff ? "ON" : "OFF");
		return -1;


}






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

#ifdef CONFIG_QS_S5K4E1
struct msm_camera_device_platform_data msm_camera_device_data_qs_cam = {
	.camera_gpio_on  = config_camera_on_gpios_qs_cam,
	.camera_gpio_off = config_camera_off_gpios_qs_cam,
	.ioext.csiphy = 0x04800000,
	.ioext.csisz  = 0x00000400,
	.ioext.csiirq = CSI_0_IRQ,
	.ioclk.mclk_clk_rate = 24000000,
	.ioclk.vfe_clk_rate  = 228570000,
#ifdef CONFIG_MSM_BUS_SCALING
	.cam_bus_scale_table = &cam_bus_client_pdata,
#endif
};
#endif

struct msm_camera_device_platform_data msm_camera_device_data_web_cam = {
	.camera_gpio_on  = config_camera_on_gpios_web_cam,
	.camera_gpio_off = config_camera_off_gpios_web_cam,
	.ioext.csiphy = 0x04900000,
	.ioext.csisz  = 0x00000400,
	.ioext.csiirq = CSI_1_IRQ,
	.ioclk.mclk_clk_rate = 24000000,
	.ioclk.vfe_clk_rate  = 228570000,
#ifdef CONFIG_MSM_BUS_SCALING
	.cam_bus_scale_table = &cam_bus_client_pdata,
#endif
};

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
#ifdef CONFIG_MT9E013
static struct msm_camera_sensor_platform_info mt9e013_sensor_8660_info = {
	.mount_angle = 0
};

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
		.sensor_platform_info = &mt9e013_sensor_8660_info,
		.csi_if			= 1
};
struct platform_device msm_camera_sensor_mt9e013 = {
		.name	= "msm_camera_mt9e013",
		.dev	= {
			.platform_data = &msm_camera_sensor_mt9e013_data,
		},
};
#endif

#ifdef CONFIG_IMX074
static struct msm_camera_sensor_platform_info imx074_sensor_board_info = {
	.mount_angle = 180
};

static struct msm_camera_sensor_flash_data flash_imx074 = {
	.flash_type		= MSM_CAMERA_FLASH_LED,
	.flash_src		= &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_imx074_data = {
	.sensor_name		= "imx074",
	.sensor_reset		= 106,
	.sensor_pwd		= 85,
	.vcm_pwd		= GPIO_AUX_CAM_2P7_EN,
	.vcm_enable		= 1,
	.pdata			= &msm_camera_device_data,
	.resource		= msm_camera_resources,
	.num_resources		= ARRAY_SIZE(msm_camera_resources),
	.flash_data		= &flash_imx074,
	.strobe_flash_data	= &strobe_flash_xenon,
	.sensor_platform_info = &imx074_sensor_board_info,
	.csi_if			= 1
};
struct platform_device msm_camera_sensor_imx074 = {
	.name	= "msm_camera_imx074",
	.dev	= {
		.platform_data = &msm_camera_sensor_imx074_data,
	},
};
#endif
#ifdef CONFIG_WEBCAM_OV9726

static struct msm_camera_sensor_platform_info ov9726_sensor_8660_info = {
	.mount_angle = 0
};

static struct msm_camera_sensor_flash_data flash_ov9726 = {
	.flash_type	= MSM_CAMERA_FLASH_LED,
	.flash_src	= &msm_flash_src
};
static struct msm_camera_sensor_info msm_camera_sensor_ov9726_data = {
	.sensor_name	= "ov9726",
	.sensor_reset_enable = 1,
	.sensor_reset	= GPIO_FRONT_CAM_RESET_N,
	.sensor_pwd	= 85,
	.vcm_pwd	= 1,
	.vcm_enable	= 0,
	.pdata		= &msm_camera_device_data_web_cam,
	.resource	= msm_camera_resources,
	.num_resources	= ARRAY_SIZE(msm_camera_resources),
	.flash_data	= &flash_ov9726,
	.sensor_platform_info = &ov9726_sensor_8660_info,
	.csi_if		= 1
};
struct platform_device msm_camera_sensor_webcam_ov9726 = {
	.name	= "msm_camera_ov9726",
	.dev	= {
		.platform_data = &msm_camera_sensor_ov9726_data,
	},
};
#endif
#ifdef CONFIG_WEBCAM_OV7692
static struct msm_camera_sensor_flash_data flash_ov7692 = {
	.flash_type		= MSM_CAMERA_FLASH_LED,
	.flash_src		= &msm_flash_src
};
static struct msm_camera_sensor_info msm_camera_sensor_ov7692_data = {
	.sensor_name	= "ov7692",
	.sensor_reset	= GPIO_WEB_CAMIF_RESET_N,
	.sensor_pwd	= 85,
	.vcm_pwd	= 1,
	.vcm_enable	= 0,
	.pdata		= &msm_camera_device_data_web_cam,
	.resource	= msm_camera_resources,
	.num_resources	= ARRAY_SIZE(msm_camera_resources),
	.flash_data	= &flash_ov7692,
	.csi_if		= 1
};

static struct platform_device msm_camera_sensor_webcam_ov7692 = {
	.name	= "msm_camera_ov7692",
	.dev	= {
		.platform_data = &msm_camera_sensor_ov7692_data,
	},
};
#endif
#ifdef CONFIG_VX6953
static struct msm_camera_sensor_platform_info vx6953_sensor_8660_info = {
	.mount_angle = 270
};

static struct msm_camera_sensor_flash_data flash_vx6953 = {
	.flash_type		= MSM_CAMERA_FLASH_NONE,
	.flash_src		= &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_vx6953_data = {
	.sensor_name	= "vx6953",
	.sensor_reset	= 63,
	.sensor_pwd		= 63,
	.vcm_pwd		= GPIO_AUX_CAM_2P7_EN,
	.vcm_enable		= 1,
	.pdata			= &msm_camera_device_data,
	.resource		= msm_camera_resources,
	.num_resources	= ARRAY_SIZE(msm_camera_resources),
	.flash_data		= &flash_vx6953,
	.sensor_platform_info = &vx6953_sensor_8660_info,
	.csi_if			= 1
};
struct platform_device msm_camera_sensor_vx6953 = {
	.name	= "msm_camera_vx6953",
	.dev	= {
		.platform_data = &msm_camera_sensor_vx6953_data,
	},
};
#endif
#ifdef CONFIG_QS_S5K4E1

static struct msm_camera_sensor_platform_info qs_s5k4e1_sensor_8660_info = {
#ifdef CONFIG_FB_MSM_MIPI_NOVATEK_CMD_QHD_PT
	.mount_angle = 90
#else
	.mount_angle = 0
#endif
};

static char eeprom_data[864];
static struct msm_camera_sensor_flash_data flash_qs_s5k4e1 = {
	.flash_type		= MSM_CAMERA_FLASH_LED,
	.flash_src		= &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_qs_s5k4e1_data = {
	.sensor_name	= "qs_s5k4e1",
	.sensor_reset	= 106,
	.sensor_pwd		= 85,
	.vcm_pwd		= 1,
	.vcm_enable		= 0,
	.pdata			= &msm_camera_device_data_qs_cam,
	.resource		= msm_camera_resources,
	.num_resources	= ARRAY_SIZE(msm_camera_resources),
	.flash_data		= &flash_qs_s5k4e1,
	.strobe_flash_data	= &strobe_flash_xenon,
	.sensor_platform_info = &qs_s5k4e1_sensor_8660_info,
	.csi_if			= 1,
	.eeprom_data	= eeprom_data,
};
struct platform_device msm_camera_sensor_qs_s5k4e1 = {
	.name	= "msm_camera_qs_s5k4e1",
	.dev	= {
		.platform_data = &msm_camera_sensor_qs_s5k4e1_data,
	},
};
#endif

#ifdef CONFIG_SENSOR_M5MO
static struct msm_camera_sensor_platform_info m5mo_sensor_8660_info = {
	.mount_angle 	= 0,
	.sensor_reset	= GPIO_CAM_MAIN_RST,
	.sensor_pwd	= GPIO_CAM_IO_EN,
	.vcm_pwd	= 1,
	.vcm_enable	= 0,
	.sensor_power_control = camera_power_maincam,
};




static struct msm_camera_sensor_flash_data flash_m5mo = {
	.flash_type = MSM_CAMERA_FLASH_LED,
//	.flash_type = MSM_CAMERA_FLASH_NONE,
	.flash_src  = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_m5mo_data = {
	.sensor_name    = "m5mo",
	.sensor_reset   = 50,
	.sensor_pwd     = 37,
	.vcm_pwd        = 1,
	.vcm_enable		= 0,
	.mclk		= 24000000,
	.pdata          = &msm_camera_device_data,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_m5mo,
	.sensor_platform_info = &m5mo_sensor_8660_info,
	.csi_if         = 1,
	.irq 		= MSM_GPIO_TO_INT(GPIO_ISP_INT),
};
static struct platform_device msm_camera_sensor_m5mo = {
	.name  	= "msm_camera_m5mo",
	.dev   	= {
		.platform_data = &msm_camera_sensor_m5mo_data,
	},
};
#endif


#ifdef CONFIG_SENSOR_ISX012
static struct msm_camera_sensor_platform_info isx012_sensor_8660_info = {
	.mount_angle 	= 0,
	.sensor_reset	= GPIO_CAM_MAIN_RST,
	.sensor_pwd	= GPIO_CAM_IO_EN,
	.vcm_pwd	= 1,
	.vcm_enable	= 0,
	.sensor_power_control = camera_power_maincam,
};

static struct msm_camera_sensor_flash_data flash_isx012 = {
	.flash_type	= MSM_CAMERA_FLASH_LED,
};

static struct msm_camera_sensor_info msm_camera_sensor_isx012_data = {
	.sensor_name	= "isx012",
	.sensor_reset	= 50,
	.sensor_pwd	= 37,
	.vcm_pwd	= 1,
	.vcm_enable	= 0,
	.mclk		= 24000000,
	.pdata		= &msm_camera_device_data,
	.resource	= msm_camera_resources,
	.num_resources	= ARRAY_SIZE(msm_camera_resources),
	.flash_data 	= &flash_isx012,
	.sensor_platform_info = &isx012_sensor_8660_info,
	.csi_if 	= 1,
	.irq 		= MSM_GPIO_TO_INT(GPIO_ISP_INT),
};

static struct platform_device msm_camera_sensor_isx012 = {
	.name	= "msm_camera_isx012",
	.dev	= {
		.platform_data = &msm_camera_sensor_isx012_data,
	},
};
#endif

#ifdef CONFIG_SENSOR_S5K6AAFX
static struct msm_camera_sensor_platform_info s5k6aafx_sensor_8660_info = {
	.mount_angle 	= 0,
	.sensor_reset	= GPIO_CAM_SUB_RST,
	.sensor_pwd	= GPIO_CAM_IO_EN,
	.vcm_pwd	= 0,
	.vcm_enable	= 0,
	.sensor_power_control = camera_power_vtcam,
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

#ifdef CONFIG_SENSOR_S5K5BAFX
static struct msm_camera_sensor_platform_info s5k5bafx_sensor_8660_info = {
	.mount_angle 	= 0,
	.sensor_reset	= GPIO_CAM_SUB_RST,
	.sensor_pwd	= GPIO_CAM_IO_EN,
	.vcm_pwd	= 0,
	.vcm_enable	= 0,
	.sensor_power_control = camera_power_vtcam,
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
	.sensor_pwd     = 37,
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

#ifdef CONFIG_SENSOR_SR200PC20M
static struct msm_camera_sensor_platform_info sr200pc20m_sensor_8660_info = {
	.mount_angle 	= 0,
	.sensor_reset	= GPIO_CAM_SUB_RST,
	.sensor_pwd	= GPIO_CAM_IO_EN,
	.vcm_pwd	= 0,
	.vcm_enable	= 0,
	.sensor_power_control = camera_power_vtcam,
};

static struct msm_camera_sensor_flash_data flash_sr200pc20m = {
//	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_type = MSM_CAMERA_FLASH_NONE,
	.flash_src  = &msm_flash_src
};

#ifndef CONFIG_CAMERA_VE
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
#endif

static struct msm_camera_sensor_info msm_camera_sensor_sr200pc20m_data = {
	.sensor_name    = "sr200pc20m",
	.sensor_reset   = 41,
	.sensor_pwd     = 37,
	.vcm_pwd        = 1,
	.vcm_enable	= 0,
	.mclk		= 24000000,
	.pdata          = &msm_camera_device_data_sub_cam,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_sr200pc20m,
	.sensor_platform_info = &sr200pc20m_sensor_8660_info,
	.csi_if         = 1
};
static struct platform_device msm_camera_sensor_sr200pc20m = {
	.name  	= "msm_camera_sr200pc20m",
	.dev   	= {
		.platform_data = &msm_camera_sensor_sr200pc20m_data,
	},
};
#endif

static struct i2c_board_info msm_camera_boardinfo[] __initdata = {
	#ifdef CONFIG_SENSOR_M5MO
	{
			I2C_BOARD_INFO("m5mo_i2c", 0x3F>>1),
	},
	#endif
	#ifdef CONFIG_SENSOR_ISX012
	{
			I2C_BOARD_INFO("isx012_i2c", 0x3D), 
	},
	#endif
	#ifdef CONFIG_SENSOR_S5K6AAFX
	{
			I2C_BOARD_INFO("s5k6aafx_i2c", 0x78>>1),
	},
	#endif
	#ifdef CONFIG_SENSOR_S5K5BAFX
	{
			I2C_BOARD_INFO("s5k5bafx_i2c", 0x5A>>1),
	},
	#endif
	#ifdef CONFIG_SENSOR_SR200PC20M
	{
			I2C_BOARD_INFO("sr200pc20m_i2c", 0x40>>1),
	},
	#endif
};


static struct i2c_board_info msm_camera_dragon_boardinfo[] __initdata = {
	#ifdef CONFIG_WEBCAM_OV9726
	{
		I2C_BOARD_INFO("ov9726", 0x10),
	},
	#endif
	#ifdef CONFIG_VX6953
	{
		I2C_BOARD_INFO("vx6953", 0x20),
	},
	#endif
};
#endif
#endif /* CONFIG_MSM_CAMERA (This is more than 1500 lines long!) */


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
#if defined(CONFIG_JPN_MODEL_SC_05D)
/*	QC patch for case 00580204 , I2C QTR failure
* GSBI7 GPIO configuration for recovery of QTR I2C lines
* in case they are pulled down */

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

//QC patch for case 00580204 , I2C QTR failure	//rohbt
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
#endif

#if defined (CONFIG_TARGET_LOCALE_USA)
static struct msm_i2c_platform_data msm_gsbi1_qup_i2c_pdata = {
	.clk_freq = 400000,
	.src_clk_rate = 24000000,
	.clk = "gsbi_qup_clk",
	.pclk = "gsbi_pclk",
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};
#else
#if defined(CONFIG_SPI_QUP) || defined(CONFIG_SPI_QUP_MODULE)
static struct msm_spi_platform_data msm_gsbi1_qup_spi_pdata = {
	.max_clock_speed = 24000000,
};
#endif
#endif
static struct msm_i2c_platform_data msm_gsbi3_qup_i2c_pdata = {
	.clk_freq = 384000,
	.src_clk_rate = 24000000,
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};

static struct msm_i2c_platform_data msm_gsbi4_qup_i2c_pdata = {
	.clk_freq = 400000,//100000,
	.src_clk_rate = 24000000,
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};

static struct msm_i2c_platform_data msm_gsbi7_qup_i2c_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
#if defined (CONFIG_JPN_MODEL_SC_05D)
	/*QC patch for case 00580204 , I2C QTR failure*/
	.pri_clk = 60,
	.pri_dat = 59,
	.msm_i2c_config_gpio = gsbi7_qup_i2c_gpio_config,
#else
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
#endif
};

static struct msm_i2c_platform_data msm_gsbi8_qup_i2c_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
#if defined (CONFIG_JPN_MODEL_SC_05D)
	.pri_clk = 65,
	.pri_dat = 64,
#endif
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};

static struct msm_i2c_platform_data msm_gsbi9_qup_i2c_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};
#if defined (CONFIG_EPEN_WACOM_G5SP)
static struct msm_i2c_platform_data msm_gsbi11_qup_i2c_pdata = {
	.clk_freq = 400000,
	.src_clk_rate = 24000000,
	.clk = "gsbi_qup_clk",
	.pclk = "gsbi_pclk",	
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};
#endif
#if !defined(CONFIG_KOR_MODEL_SHV_E160L)
static struct msm_i2c_platform_data msm_gsbi12_qup_i2c_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
	.use_gsbi_shared_mode = 1,
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};
#endif
#if defined(CONFIG_PN544_NFC)
static struct msm_i2c_platform_data msm_gsbi10_qup_i2c_pdata = {
	.clk_freq =100000,
	.src_clk_rate = 24000000,
//	.clk = "gsbi_qup_clk",
//	.pclk = "gsbi_pclk",
	.msm_i2c_config_gpio = gsbi_qup_i2c_gpio_config,
};
#endif
#endif

#if !defined(CONFIG_PN544_NFC)
static struct msm_spi_platform_data msm_gsbi10_qup_spi_pdata = {
	.max_clock_speed = 24000000,
};
#endif

#if defined(CONFIG_TDMB) || defined(CONFIG_TDMB_MODULE)
static uint32_t tdmb_on_gpio_table[] = {
	GPIO_CFG(GPIO_TDMB_EN, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_TDMB_RST, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_TDMB_INT, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_TDMB_SPI_MOSI, GPIOMUX_FUNC_1,
		GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
	GPIO_CFG(GPIO_TDMB_SPI_MISO, GPIOMUX_FUNC_1,
		GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
	GPIO_CFG(GPIO_TDMB_SPI_CS, GPIOMUX_FUNC_1,
		GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
	GPIO_CFG(GPIO_TDMB_SPI_CLK, GPIOMUX_FUNC_1,
		GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA),
};

static uint32_t tdmb_off_gpio_table[] = {
	GPIO_CFG(GPIO_TDMB_EN, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_TDMB_RST, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_TDMB_INT, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_TDMB_SPI_MOSI, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_8MA),
	GPIO_CFG(GPIO_TDMB_SPI_MISO, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_8MA),
	GPIO_CFG(GPIO_TDMB_SPI_CS, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_8MA),
	GPIO_CFG(GPIO_TDMB_SPI_CLK, GPIOMUX_FUNC_GPIO,
		GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_8MA),
};

static struct regulator *vreg_l2a = NULL; // pmic8058 xo out d0 internal power
static struct msm_xo_voter *tdmb_clock = NULL;

static int tdmb_pmic_clk_on(void)
{
	int rc;
	if (!vreg_l2a && !IS_ERR(vreg_l2a)) {
		rc = regulator_set_voltage(vreg_l2a, 2600000, 2600000);
		if (rc) {
			pr_err("%s: error setting voltage\n", __func__);
		}
		
		rc = regulator_enable(vreg_l2a);
		if (rc) {
			pr_err("%s: error enabling regulator\n", __func__);
		}
	}

	if(!tdmb_clock) {
		tdmb_clock = msm_xo_get(MSM_XO_TCXO_D0, "tdmb_power");
		if (IS_ERR(tdmb_clock)) {
			pr_err("Couldn't get TCXO_D0 voter\n");
			goto fail_switch;
		}

		rc = msm_xo_mode_vote(tdmb_clock, MSM_XO_MODE_ON);
		if (rc < 0) {
			pr_err("Failed to vote for TCXO_DO ON\n");
			goto fail_vote;
		}

		rc = msm_xo_mode_vote(tdmb_clock, MSM_XO_MODE_PIN_CTRL);
		if (rc < 0) {
			pr_err("Failed to vote for TCXO_DO pin control\n");
			goto fail_vote;
		}
	}
	return 0;
	
fail_vote:
	msm_xo_put(tdmb_clock);
	tdmb_clock = NULL;
fail_switch:
	if(regulator_is_enabled(vreg_l2a)) {
		if (regulator_disable(vreg_l2a))
			pr_err("%s: Unable to disable the regulator\n"
				, __func__);
	}
	return -1;
}

static void tdmb_pmic_clk_off(void)
{
	if(tdmb_clock) {
		msm_xo_mode_vote(tdmb_clock, MSM_XO_MODE_OFF);
		msm_xo_put(tdmb_clock);
		tdmb_clock = NULL;
	}
	if (!vreg_l2a && !IS_ERR(vreg_l2a)) {
		if(regulator_is_enabled(vreg_l2a)) {
			if (regulator_disable(vreg_l2a))
				pr_err("%s: Unable to disable the regulator\n" , __func__);
		}
	}
}

static int tdmb_pmic_clk_init(void)
{
	vreg_l2a = regulator_get(NULL, "8058_l2");
	if (IS_ERR(vreg_l2a)) {
		pr_err("%s: regulator get of 8058_l2 failed (%ld)\n", __func__, PTR_ERR(vreg_l2a));
		return 1;
	}
	return 0;
}
static int tdmb_pmic_clk_select(char select)
{
	int ret = 0;
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_JPN_MODEL_SC_05D)
	if (get_hw_rev() > 7) {
		if(select == TDMB_PMIC_CLK_INIT) {
			ret = tdmb_pmic_clk_init();
		} else if(select == TDMB_PMIC_CLK_ON) {
			ret = tdmb_pmic_clk_on();
		} else {		/* TDMB_PMIC_CLK_OFF */
			tdmb_pmic_clk_off();
		}
	}
#else	/* CONFIG_KOR_MODEL_SHV_E160L */
	if(select == TDMB_PMIC_CLK_INIT) {
		ret = tdmb_pmic_clk_init();
	} else if(select == TDMB_PMIC_CLK_ON) {
		ret = tdmb_pmic_clk_on();
	} else {				/* TDMB_PMIC_CLK_OFF */
		tdmb_pmic_clk_off();
	}
#endif
	return ret;
}

static void tdmb_gpio_on(void)
	{
	int n, rc;
	printk(KERN_DEBUG "tdmb_gpio_on\n");

	for (n = 0; n < ARRAY_SIZE(tdmb_on_gpio_table); n++) {
		rc = gpio_tlmm_config(tdmb_on_gpio_table[n], GPIO_CFG_ENABLE);
		if (rc)
			break;
	}

	if(tdmb_pmic_clk_select(TDMB_PMIC_CLK_ON)) {
		printk("tdmb_pmic_clk_select TDMB_PMIC_CLK_ON fail\n");
		return;
	}		

	gpio_set_value(GPIO_TDMB_EN, 1);
	usleep_range(10000, 10000);
	gpio_set_value(GPIO_TDMB_RST, 0);
	usleep_range(2000, 2000);
	gpio_set_value(GPIO_TDMB_RST, 1);
	usleep_range(10000, 10000);

	return;
}
static void tdmb_gpio_off(void)
	{
	int n, rc;
	printk(KERN_DEBUG "tdmb_gpio_off\n");

	gpio_set_value(GPIO_TDMB_RST, 0);
	usleep_range(1000, 1000);
	gpio_set_value(GPIO_TDMB_EN, 0);

	tdmb_pmic_clk_select(TDMB_PMIC_CLK_OFF);

	for (n = 0; n < ARRAY_SIZE(tdmb_off_gpio_table); n++) {
		rc = gpio_tlmm_config(tdmb_off_gpio_table[n], GPIO_CFG_ENABLE);
		if (rc)
			break;
	}
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
	int n, rc;

#ifdef CONFIG_BATTERY_SEC
	if(is_lpm_boot)
		return 0;
#endif

	platform_device_register(&tdmb_device);

	rc = gpio_request(GPIO_TDMB_EN, "TDMB_EN");
	if (rc < 0) {
		printk(KERN_ERR "%s: GPIO_TDMB_EN gpio %d request"
			" failed\n", __func__, GPIO_TDMB_EN);
		return 1;
	}
	gpio_direction_output(GPIO_TDMB_EN, 0);
	
	rc = gpio_request(GPIO_TDMB_RST, "TDMB_RST");
	if (rc < 0) {
		printk(KERN_ERR "%s: GPIO_TDMB_RST gpio %d request"
			" failed\n", __func__, GPIO_TDMB_RST);
		return 1;
	}
	gpio_direction_output(GPIO_TDMB_RST, 0);

	gpio_free(GPIO_TDMB_EN);
	gpio_free(GPIO_TDMB_RST);

	for (n = 0; n < ARRAY_SIZE(tdmb_off_gpio_table); n++) {
		rc = gpio_tlmm_config(tdmb_off_gpio_table[n], GPIO_CFG_ENABLE);
		if (rc)
			break;
	}

	if (spi_register_board_info(tdmb_spi_info, ARRAY_SIZE(tdmb_spi_info))
		!= 0)
		pr_err("%s: spi_register_board_info returned error\n",
			__func__);

	if(tdmb_pmic_clk_select(TDMB_PMIC_CLK_INIT)) {
	    printk("tdmb_pmic_clk_select TDMB_PMIC_CLK_INIT fail\n");	
		return 1;
	}

    return 0;
}
#endif /* CONFIG_TDMB || CONFIG_TDMB_MODULE */


#if defined(CONFIG_ISDBT)
static uint32_t isdbt_on_gpio_table[] = {
	GPIO_CFG(GPIO_ISDBT_EN,  GPIOMUX_FUNC_GPIO, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_ISDBT_RST,  GPIOMUX_FUNC_GPIO, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_ISDBT_INT,  GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 
	GPIO_CFG(GPIO_ISDBT_SPI_MOSI,  GPIOMUX_FUNC_1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), 
	GPIO_CFG(GPIO_ISDBT_SPI_MISO,  GPIOMUX_FUNC_1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), 
	GPIO_CFG(GPIO_ISDBT_SPI_CS,  GPIOMUX_FUNC_1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), 
	GPIO_CFG(GPIO_ISDBT_SPI_CLK,  GPIOMUX_FUNC_1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), 

};
static uint32_t isdbt_off_gpio_table[] = {
	GPIO_CFG(GPIO_ISDBT_EN,  GPIOMUX_FUNC_GPIO, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_ISDBT_RST,  GPIOMUX_FUNC_GPIO, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_ISDBT_INT,  GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 
	GPIO_CFG(GPIO_ISDBT_SPI_MOSI,  GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), 
	GPIO_CFG(GPIO_ISDBT_SPI_MISO,  GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), 
	GPIO_CFG(GPIO_ISDBT_SPI_CS,  GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), 
	GPIO_CFG(GPIO_ISDBT_SPI_CLK,  GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), 
};

static void isdbt_gpio_on(void)
{
	int n, rc;
	printk(KERN_DEBUG "isdbt_gpio_on\n");

	for (n = 0; n < ARRAY_SIZE(isdbt_on_gpio_table); n++) {
		rc = gpio_tlmm_config(isdbt_on_gpio_table[n], GPIO_CFG_ENABLE);
		if (rc)
			break;
	}

	gpio_set_value(GPIO_ISDBT_EN, 1);
	usleep_range(40000, 40000);
	gpio_set_value(GPIO_ISDBT_RST, 0);
	usleep_range(2000, 2000);
	gpio_set_value(GPIO_ISDBT_RST, 1);
	usleep_range(10000, 10000);

	return;
}

static void isdbt_gpio_off(void)
{
	int n, rc;
	printk(KERN_DEBUG "isdbt_gpio_off\n");

	gpio_set_value(GPIO_ISDBT_RST, 0);
	usleep_range(1000, 1000);
	gpio_set_value(GPIO_ISDBT_EN, 0);

	for (n = 0; n < ARRAY_SIZE(isdbt_off_gpio_table); n++) {
		rc = gpio_tlmm_config(isdbt_off_gpio_table[n], GPIO_CFG_ENABLE);
		if (rc)
			break;
	}
}

static struct isdbt_platform_data isdbt_pdata = {
	.gpio_on = isdbt_gpio_on,
	.gpio_off = isdbt_gpio_off,
	.irq = MSM_GPIO_TO_INT(GPIO_ISDBT_INT),
};

static struct platform_device isdbt_device = {
	.name			= "isdbt",
	.id 			= -1,
	.dev			= {
		.platform_data = &isdbt_pdata,
	},
};

static struct spi_board_info isdbt_spi_info[] __initdata = {	
    {	
        .modalias       = "isdbtspi",
        .mode           = SPI_MODE_0,
        .bus_num        = 0,
        .chip_select    = 0,
        .max_speed_hz   = 5400000,// 8500000,
    }
};

static int __init isdbt_dev_init(void)
{
	int n, rc;

#ifdef CONFIG_BATTERY_SEC
	if(is_lpm_boot)
		return 0;
#endif
	platform_device_register(&isdbt_device);

	rc = gpio_request(GPIO_ISDBT_EN, "ISDBT_EN");
	if (rc < 0) {
		printk(KERN_ERR "%s: GPIO_ISDBT_EN gpio %d request"
			" failed\n", __func__, GPIO_ISDBT_EN);
		return 1;
	}
	gpio_direction_output(GPIO_ISDBT_EN, 0);

	rc = gpio_request(GPIO_ISDBT_RST, "ISDBT_RST");
	if (rc < 0) {
		printk(KERN_ERR "%s: GPIO_ISDBT_RST gpio %d request"
			" failed\n", __func__, GPIO_ISDBT_RST);
		return 1;
	}
	gpio_direction_output(GPIO_ISDBT_RST, 0);

	gpio_free(GPIO_ISDBT_EN);
	gpio_free(GPIO_ISDBT_RST);

	for (n = 0; n < ARRAY_SIZE(isdbt_off_gpio_table); n++) {
		rc = gpio_tlmm_config(isdbt_off_gpio_table[n], GPIO_CFG_ENABLE);
		if (rc)
			break;
	}

	if (spi_register_board_info(isdbt_spi_info, ARRAY_SIZE(isdbt_spi_info))
		!= 0)
		pr_err("%s: spi_register_board_info returned error\n",
			__func__);

	return 0;
}
#endif /* CONFIG_ISDBT */


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

#ifdef CONFIG_BATTERY_SEC
unsigned int is_lpcharging_state(void)
{
	u32 val = is_lpm_boot;
	
	pr_info("%s: is_lpm_boot:%d\n", __func__, val);

	return val;
}
EXPORT_SYMBOL(is_lpcharging_state);

static unsigned int sec_bat_get_lpcharging_state(void)
{
	u32 val = is_lpm_boot;
	
	pr_info("%s: LP charging:%d\n", __func__, val);

	return val;
}

void disable_charging_before_reset(void);
static struct sec_bat_platform_data sec_bat_pdata = {
	.fuel_gauge_name	= "fuelgauge",
	.charger_name 		= "sec-charger",
	.get_lpcharging_state	= sec_bat_get_lpcharging_state,
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || \
	defined(CONFIG_KOR_MODEL_SHV_E160K) || \
	defined(CONFIG_KOR_MODEL_SHV_E160L)
	.hwrev_has_2nd_therm	= 0x7,
#elif defined(CONFIG_JPN_MODEL_SC_05D)
	.hwrev_has_2nd_therm	= 0,
#else
	.hwrev_has_2nd_therm	= -1,
#endif
	.chg_shutdown_cb = disable_charging_before_reset,
};

static struct platform_device sec_device_battery = {
	.name = "sec-battery",
	.id = -1,
	.dev.platform_data = &sec_bat_pdata,
};

static unsigned int get_fuelgauge_voltage_now(void)
{
	struct power_supply *psy = power_supply_get_by_name(sec_bat_pdata.fuel_gauge_name);
	union power_supply_propval value;
	if (!psy) { 
		pr_err("%s: psy is null!!\n", __func__);
		return 0;
	}
	if (psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value) < 0) {
		pr_err("%s: failed to get voltage now\n", __func__);
		return 0;
	}
	return value.intval;
}

/* 
* For reducing battery voltage level before reset behavior in case of high-voltage charging,
* (High voltage level might cause the error of FuelGauge compensation algorithm in boot)
* Called by sec_bat_shutdown(), sec_debug_hw_reset()
*/
void disable_charging_before_reset(void)
{
	struct power_supply *psy = power_supply_get_by_name(sec_bat_pdata.charger_name);
	union power_supply_propval val_status, val_type;
	unsigned int volt_now;

	if (!psy) {
		pr_err("%s: psy is null!!!", __func__);
		return;
	}

	if (psy->get_property(psy, POWER_SUPPLY_PROP_STATUS, &val_status) < 0) {
		pr_err("%s: fail to get charging status\n", __func__);
		return;
	} else {
		if (val_status.intval == POWER_SUPPLY_STATUS_DISCHARGING) {
			pr_err("%s: skip charging control\n", __func__);
			return;
		}
	}

	volt_now = get_fuelgauge_voltage_now();
	pr_info("%s: check voltage(%d)\n", __func__, volt_now);
	if (volt_now > 4150000) {
		val_type.intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		if (psy->set_property(psy, POWER_SUPPLY_PROP_STATUS, &val_type) < 0) {
			pr_err("%s: fail to disable charging\n", __func__);
			return;
		}
		mdelay(500);
		volt_now = get_fuelgauge_voltage_now();
		pr_info("%s: check voltage(%d) after disable charging.\n", __func__, volt_now);
	}
}
#endif /* CONFIG_BATTERY_SEC */

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
#define MSM_FB_PRIM_BUF_SIZE_MULTIPLIER (3)
#else
#define MSM_FB_PRIM_BUF_SIZE_MULTIPLIER (2)
#endif 
#if defined (CONFIG_FB_MSM_MIPI_S6E8AA0_HD720_PANEL)
/* prim = 736 x 1280 x 4(bpp) x 2(pages) */ 
#define MSM_FB_PRIM_BUF_SIZE \
		(roundup((736 * 1280 * 4), 4096) * MSM_FB_PRIM_BUF_SIZE_MULTIPLIER)
#elif defined (CONFIG_FB_MSM_MIPI_S6E8AA0_WXGA_Q1_PANEL)
/* prim = 800 x 1280 x 4(bpp) x 2(pages) */ 
#define MSM_FB_PRIM_BUF_SIZE \
		(roundup((800 * 1280 * 4), 4096) * MSM_FB_PRIM_BUF_SIZE_MULTIPLIER)
#else
// ICS original src 
#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
#if defined (CONFIG_JPN_MODEL_SC_05D)
#define MSM_FB_PRIM_BUF_SIZE \
		(roundup((1024 * 600 * 4), 4096) * 3) /* 4 bpp x 3 pages */
#else
#define MSM_FB_PRIM_BUF_SIZE \
		(roundup((800 * 480 * 4), 4096) * 3) /* 4 bpp x 3 pages */
#endif
#else
#if defined (CONFIG_JPN_MODEL_SC_05D)
#define MSM_FB_PRIM_BUF_SIZE \
		(roundup((1024 * 600 * 4), 4096) * 2) /* 4 bpp x 2 pages */
#else
#define MSM_FB_PRIM_BUF_SIZE \
		(roundup((800 * 480 * 4), 4096) * 2) /* 4 bpp x 2 pages */
#endif
#endif
#endif

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
#define MSM_FB_EXT_BUF_SIZE  \
		(roundup((1920 * 1080 * 2), 4096) * 2) /* 2 bpp x 2 page */
#elif defined(CONFIG_FB_MSM_TVOUT)
#define MSM_FB_EXT_BUF_SIZE  \
		(roundup((720 * 576 * 2), 4096) * 2) /* 2 bpp x 2 pages */
#else
#define MSM_FB_EXT_BUF_SIZE	0
#endif

/* Note: must be multiple of 4096 */
#define MSM_FB_SIZE roundup(MSM_FB_PRIM_BUF_SIZE + MSM_FB_EXT_BUF_SIZE + \
				MSM_FB_DSUB_PMEM_ADDER, 4096)

#define MSM_PMEM_SF_SIZE 0x4000000 /* 64 Mbytes */
#define MSM_HDMI_PRIM_PMEM_SF_SIZE 0x4000000 /* 64 Mbytes */

#ifdef CONFIG_FB_MSM_HDMI_AS_PRIMARY
unsigned char hdmi_is_primary = 1;
#else
unsigned char hdmi_is_primary;
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

#if defined(CONFIG_JPN_MODEL_SC_05D)
#define MSM_PMEM_KERNEL_EBI1_SIZE  0x600000
#else
#define MSM_PMEM_KERNEL_EBI1_SIZE  0x0 //  0x600000 -> 0x0 Not used region
#endif
#ifndef CONFIG_SEC_KERNEL_REBASE_FOR_PMEM_OPTIMIZATION
#if defined(CONFIG_JPN_MODEL_SC_05D)
#define MSM_PMEM_ADSP_SIZE         0x2900000
# else
#define MSM_PMEM_ADSP_SIZE         0x4200000
#endif
#else
#define MSM_PMEM_ADSP_BASE         0x40400000
#if defined(CONFIG_JPN_MODEL_SC_05D)
#define MSM_PMEM_ADSP_SIZE         0x02900000
# else
#define MSM_PMEM_ADSP_SIZE         0x02A00000 /* 42MB */
#endif
#endif
#define MSM_PMEM_AUDIO_SIZE        0x28B000

#ifdef CONFIG_ANDROID_RAM_CONSOLE
#if defined(CONFIG_JPN_MODEL_SC_05D)
#define RAM_CONSOLE_START            0x42E00000
#define RAM_CONSOLE_SIZE            SZ_512K
# else
#define RAM_CONSOLE_START          0x77800000
#define RAM_CONSOLE_SIZE            SZ_1M
#endif
#endif

#define MSM_SMI_BASE          0x38000000
#define MSM_SMI_SIZE          0x4000000

#define KERNEL_SMI_BASE       (MSM_SMI_BASE)
#define KERNEL_SMI_SIZE       0x600000

#define USER_SMI_BASE         (KERNEL_SMI_BASE + KERNEL_SMI_SIZE)
#define USER_SMI_SIZE         (MSM_SMI_SIZE - KERNEL_SMI_SIZE)
#define MSM_PMEM_SMIPOOL_SIZE USER_SMI_SIZE

#if defined (CONFIG_SEC_KERNEL_REBASE_FOR_PMEM_OPTIMIZATION) && defined(CONFIG_USA_MODEL_SGH_I717) \
    && !defined(CONFIG_USA_MODEL_SGH_T879)
#define MSM_ION_SF_BASE		0x7a000000
#endif
#if defined(CONFIG_JPN_MODEL_SC_05D)
#define MSM_ION_SF_BASE		0x7b000000
#define MSM_ION_SF_SIZE		0x05000000 
#else
#if defined(CONFIG_JPN_MODEL_SC_05D)
#define MSM_ION_SF_SIZE		0x06000000 /* 64MB -> 96MB same as Gingerbread of ATT Quincy */
else
#define MSM_ION_SF_SIZE		0x5000000 /* 64MB -> 80MB */
#endif
#endif
#define MSM_ION_CAMERA_SIZE	0x1200000 /* 18MB */
#define MSM_ION_MM_FW_SIZE	0x200000 /* (2MB) */
#define MSM_ION_MM_SIZE		0x3600000 /* (54MB) Must be a multiple of 64K */
#define MSM_ION_MFC_SIZE	SZ_8K
#define MSM_ION_WB_SIZE		(MSM_PMEM_ADSP_SIZE-MSM_ION_CAMERA_SIZE) /* 24MB, ICS : 30MB */
#define MSM_ION_QSECOM_SIZE	0x600000 /* (6MB) */
#define MSM_ION_AUDIO_SIZE	MSM_PMEM_AUDIO_SIZE

#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
#ifdef CONFIG_QSEECOM
#define MSM_ION_HEAP_NUM	9
#else
#define MSM_ION_HEAP_NUM	8
#endif
#define MSM_HDMI_PRIM_ION_SF_SIZE MSM_HDMI_PRIM_PMEM_SF_SIZE
#if !defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_USA_MODEL_SGH_T879) || defined(CONFIG_JPN_MODEL_SC_05D)
static unsigned msm_ion_sf_size = MSM_ION_SF_SIZE;
#endif
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

static void set_mdp_clocks_for_wuxga(void);

static int msm_fb_detect_panel(const char *name)
{
// kmj_ea25.lcd		
#if defined (CONFIG_FB_MSM_LCDC_LD9040_WVGA_PANEL)
	if (!strcmp(name, "lcdc_ld9040_wvga"))
		return 0;
#elif defined (CONFIG_FB_MSM_LCDC_S6E63M0_WVGA_PANEL)	
	if (!strcmp(name, "lcdc_S6E63M0_wvga"))
		return 0;
#else
	if (machine_is_msm8x60_fluid()) {
		uint32_t soc_platform_version = socinfo_get_platform_version();
		if (SOCINFO_VERSION_MAJOR(soc_platform_version) < 3) {
#ifdef CONFIG_FB_MSM_LCDC_SAMSUNG_OLED_PT
			if (!strncmp(name, LCDC_SAMSUNG_OLED_PANEL_NAME,
					strnlen(LCDC_SAMSUNG_OLED_PANEL_NAME,
						PANEL_NAME_MAX_LEN)))
				return 0;
#endif
		} else { /*P3 and up use AUO panel */
#ifdef CONFIG_FB_MSM_LCDC_AUO_WVGA
			if (!strncmp(name, LCDC_AUO_PANEL_NAME,
					strnlen(LCDC_AUO_PANEL_NAME,
						PANEL_NAME_MAX_LEN)))
				return 0;
#endif
		}
#ifdef CONFIG_FB_MSM_LCDC_NT35582_WVGA
	} else if machine_is_msm8x60_dragon() {
	    if (!strncmp(name, LCDC_NT35582_PANEL_NAME,
				strnlen(LCDC_NT35582_PANEL_NAME,
					PANEL_NAME_MAX_LEN)))
			return 0;
#endif
	} else {
		if (!strncmp(name, LCDC_SAMSUNG_WSVGA_PANEL_NAME,
				strnlen(LCDC_SAMSUNG_WSVGA_PANEL_NAME,
					PANEL_NAME_MAX_LEN)))
			return 0;

#if !defined(CONFIG_FB_MSM_LCDC_AUTO_DETECT) && \
	!defined(CONFIG_FB_MSM_MIPI_PANEL_AUTO_DETECT) && \
	!defined(CONFIG_FB_MSM_LCDC_MIPI_PANEL_AUTO_DETECT)
		if (!strncmp(name, MIPI_VIDEO_TOSHIBA_WVGA_PANEL_NAME,
				strnlen(MIPI_VIDEO_TOSHIBA_WVGA_PANEL_NAME,
					PANEL_NAME_MAX_LEN)))
			return 0;

		if (!strncmp(name, MIPI_VIDEO_NOVATEK_QHD_PANEL_NAME,
				strnlen(MIPI_VIDEO_NOVATEK_QHD_PANEL_NAME,
					  PANEL_NAME_MAX_LEN)))
			return 0;

		if (!strncmp(name, MIPI_CMD_NOVATEK_QHD_PANEL_NAME,
				strnlen(MIPI_CMD_NOVATEK_QHD_PANEL_NAME,
					PANEL_NAME_MAX_LEN)))
			return 0;
#endif
	}

#endif
	if (!strncmp(name, HDMI_PANEL_NAME,
			strnlen(HDMI_PANEL_NAME,
				PANEL_NAME_MAX_LEN))) {
		if (hdmi_is_primary)
			set_mdp_clocks_for_wuxga();
		return 0;
	}

	if (!strncmp(name, TVOUT_PANEL_NAME,
			strnlen(TVOUT_PANEL_NAME,
				PANEL_NAME_MAX_LEN)))
		return 0;

	pr_warning("%s: not supported '%s'", __func__, name);
	return -ENODEV;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources     = ARRAY_SIZE(msm_fb_resources),
	.resource          = msm_fb_resources,
	.dev.platform_data = &msm_fb_pdata,
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
#ifdef CONFIG_SEC_KERNEL_REBASE_FOR_PMEM_OPTIMIZATION
	.memory_type = MEMTYPE_PMEM_ADSP,
#else
	.memory_type = MEMTYPE_EBI1,
#endif
};

static struct platform_device android_pmem_adsp_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &android_pmem_adsp_pdata },
};
#endif /*CONFIG_MSM_MULTIMEDIA_USE_ION*/
#if defined (CONFIG_USA_MODEL_SGH_I717) || !defined (CONFIG_MSM_MULTIMEDIA_USE_ION) || defined(CONFIG_JPN_MODEL_SC_05D)
static struct android_pmem_platform_data android_pmem_audio_pdata = {
	.name = "pmem_audio",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
	.memory_type = MEMTYPE_EBI1,
};

static struct platform_device android_pmem_audio_device = {
	.name = "android_pmem",
	.id = 4,
	.dev = { .platform_data = &android_pmem_audio_pdata },
};
#endif
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
#endif /*CONFIG_MSM_MULTIMEDIA_USE_ION*/
#endif /*CONFIG_ANDROID_PMEM*/

#ifdef CONFIG_ANDROID_RAM_CONSOLE
static struct resource ram_console_resource[] = {
	{
#if defined(CONFIG_JPN_MODEL_SC_05D)
		.start  = RAM_CONSOLE_START,
		.end    = RAM_CONSOLE_START + RAM_CONSOLE_SIZE - 1,
#endif
		.flags  = IORESOURCE_MEM,
	},
};

static struct platform_device ram_console_device = {
	.name           = "ram_console",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(ram_console_resource),
	.resource       = ram_console_resource,
};
#endif

#define GPIO_DONGLE_PWR_EN 258
#if !defined(CONFIG_FB_MSM_LCDC_LD9040_WVGA_PANEL) \
	&& !defined(CONFIG_FB_MSM_LCDC_S6E63M0_WVGA_PANEL)\
	&& !defined(CONFIG_FB_MSM_MIPI_S6D6AA0_WXGA_PANEL) \
	&& !defined(CONFIG_FB_MSM_MIPI_S6E8AA0_HD720_PANEL) \
	&& !defined(CONFIG_FB_MSM_MIPI_S6E8AA0_WXGA_Q1_PANEL)
	
static void setup_display_power(void);
static int lcdc_vga_enabled;
static int vga_enable_request(int enable)
{
	if (enable)
		lcdc_vga_enabled = 1;
	else
		lcdc_vga_enabled = 0;
	setup_display_power();

	return 0;
}

#define GPIO_BACKLIGHT_PWM0 0
#define GPIO_BACKLIGHT_PWM1 1

static int pmic_backlight_gpio[2]
	= { GPIO_BACKLIGHT_PWM0, GPIO_BACKLIGHT_PWM1 };
static struct msm_panel_common_pdata lcdc_samsung_panel_data = {
	.gpio_num = pmic_backlight_gpio, /* two LPG CHANNELS for backlight */
	.vga_switch = vga_enable_request,
};

static struct platform_device lcdc_samsung_panel_device = {
	.name = LCDC_SAMSUNG_WSVGA_PANEL_NAME,
	.id = 0,
	.dev = {
		.platform_data = &lcdc_samsung_panel_data,
	}
};
#endif
struct class *sec_class;
EXPORT_SYMBOL(sec_class);
#if (!defined(CONFIG_SPI_QUP)) && \
	(defined(CONFIG_FB_MSM_LCDC_SAMSUNG_OLED_PT) || \
	defined(CONFIG_FB_MSM_LCDC_AUO_WVGA))

static int lcdc_spi_gpio_array_num[] = {
	LCDC_SPI_GPIO_CLK,
	LCDC_SPI_GPIO_CS,
	LCDC_SPI_GPIO_MOSI,
};

static uint32_t lcdc_spi_gpio_config_data[] = {
	GPIO_CFG(LCDC_SPI_GPIO_CLK, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(LCDC_SPI_GPIO_CS, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(LCDC_SPI_GPIO_MOSI, 0,
			GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
};

static void lcdc_config_spi_gpios(int enable)
{
	int n;
	for (n = 0; n < ARRAY_SIZE(lcdc_spi_gpio_config_data); ++n)
		gpio_tlmm_config(lcdc_spi_gpio_config_data[n], 0);
}
#endif

#ifdef CONFIG_FB_MSM_LCDC_SAMSUNG_OLED_PT
#ifdef CONFIG_SPI_QUP
static struct spi_board_info lcdc_samsung_spi_board_info[] __initdata = {
	{
		.modalias       = LCDC_SAMSUNG_SPI_DEVICE_NAME,
		.mode           = SPI_MODE_3,
		.bus_num        = 1,
		.chip_select    = 0,
		.max_speed_hz   = 10800000,
	}
};
#endif /* CONFIG_SPI_QUP */

static struct msm_panel_common_pdata lcdc_samsung_oled_panel_data = {
#ifndef CONFIG_SPI_QUP
	.panel_config_gpio = lcdc_config_spi_gpios,
	.gpio_num          = lcdc_spi_gpio_array_num,
#endif
};

static struct platform_device lcdc_samsung_oled_panel_device = {
	.name   = LCDC_SAMSUNG_OLED_PANEL_NAME,
	.id     = 0,
	.dev.platform_data = &lcdc_samsung_oled_panel_data,
};
#endif /*CONFIG_FB_MSM_LCDC_SAMSUNG_OLED_PT */

#ifdef CONFIG_FB_MSM_LCDC_AUO_WVGA
#ifdef CONFIG_SPI_QUP
static struct spi_board_info lcdc_auo_spi_board_info[] __initdata = {
	{
		.modalias       = LCDC_AUO_SPI_DEVICE_NAME,
		.mode           = SPI_MODE_3,
		.bus_num        = 1,
		.chip_select    = 0,
		.max_speed_hz   = 10800000,
	}
};
#endif

static struct msm_panel_common_pdata lcdc_auo_wvga_panel_data = {
#ifndef CONFIG_SPI_QUP
	.panel_config_gpio = lcdc_config_spi_gpios,
	.gpio_num          = lcdc_spi_gpio_array_num,
#endif
};

static struct platform_device lcdc_auo_wvga_panel_device = {
	.name   = LCDC_AUO_PANEL_NAME,
	.id     = 0,
	.dev.platform_data = &lcdc_auo_wvga_panel_data,
};
#endif /*CONFIG_FB_MSM_LCDC_AUO_WVGA*/

#if defined (CONFIG_FB_MSM_LCDC_LD9040_WVGA_PANEL) || defined (CONFIG_FB_MSM_LCDC_S6E63M0_WVGA_PANEL)

static int lcdc_gpio_array_num[] = {
				103, /* spi_clk */
				104, /* spi_cs  */
				106, /* spi_mosi */
				28, /* lcd_reset */
};

static struct msm_gpio lcdc_gpio_config_data[] = {
#if defined (CONFIG_JPN_MODEL_SC_05D)
	{ GPIO_CFG(103, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "spi_clk" },
	{ GPIO_CFG(104, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "spi_cs0" },
	{ GPIO_CFG(106, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "spi_mosi" },
	{ GPIO_CFG(28, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "lcd_reset" },

#else
	{ GPIO_CFG(103, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "spi_clk" },
	{ GPIO_CFG(104, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "spi_cs0" },
	{ GPIO_CFG(106, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "spi_mosi" },
	{ GPIO_CFG(28, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "lcd_reset" },
#endif
};
static struct msm_gpio lcdc_gpio_off_config_data[] = {
	{ GPIO_CFG(103, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), "spi_clk" },
	{ GPIO_CFG(104, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), "spi_cs0" },
	{ GPIO_CFG(106, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), "spi_mosi" },
	{ GPIO_CFG(28, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), "lcd_reset" },
};
extern int msm_gpios_request_enable(const struct msm_gpio *table, int size);
extern void msm_gpios_disable_free(const struct msm_gpio *table, int size);
static void lcdc_config_gpios(int enable)
{
		printk("ld9040 : lcdc_config_gpios [%d]\n", enable);
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
			int i;
			int loop_count= ARRAY_SIZE(lcdc_gpio_off_config_data);
			for( i=0; i<loop_count; i++)
			{
					gpio_tlmm_config(lcdc_gpio_off_config_data[i].gpio_cfg, 1);
			}
	
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

#if defined (CONFIG_FB_MSM_LCDC_S6E63M0_WVGA_PANEL)
static struct platform_device lcdc_ld9040_panel_device = {
	.name   = "lcdc_S6E63M0_wvga",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_panel_data,
	}
#else
static struct platform_device lcdc_ld9040_panel_device = {
	.name   = "lcdc_ld9040_wvga",
	.id     = 0,
	.dev    = {
		.platform_data = &lcdc_panel_data,
	}
#endif
};
#endif /* CONFIG_FB_MSM_LCDC_LD9040_WVGA_PANEL */


#ifdef CONFIG_USB_SWITCH_FSA9480
static int fsa9480_init_flag = 0;
static enum cable_type_t set_cable_status;
static int device_attached;
//struct class *sec_class;
//EXPORT_SYMBOL(sec_class);
struct device *switch_dev;
EXPORT_SYMBOL(switch_dev);
int usb_access_lock = 0;
EXPORT_SYMBOL(usb_access_lock);

#if defined(CONFIG_KOR_MODEL_SHV_E160L)
#define GPIO_USB_I2C_SCL	115
#define GPIO_USB_I2C_SDA	116
#endif
#if 1
static unsigned fsa9480_gpio_on[] = {
	GPIO_CFG(58, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), // uart_sel
#if defined(CONFIG_KOR_MODEL_SHV_E160L)
	GPIO_CFG(115,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
	GPIO_CFG(116,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
#endif
};

static void __init fsa9480_gpio_init(void)
{
	int pin, rc;
	for (pin= 0; pin < ARRAY_SIZE(fsa9480_gpio_on); pin++) {
		rc = gpio_tlmm_config(fsa9480_gpio_on[pin], GPIO_CFG_ENABLE);
		if (rc) {
			pr_err("%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, fsa9480_gpio_on[pin], rc);
		}
		
	}
}
#endif

 static void fsa9480_otg_cb(bool attached)
{
#ifdef CONFIG_BATTERY_SEC
	union power_supply_propval value;
	int ret = 0;
	struct power_supply *psy = power_supply_get_by_name("battery");
	printk(KERN_ERR "fsa9480_otg_cb attached %d\n", attached);
	
	if (attached)
		value.intval = POWER_SUPPLY_CAPACITY_OTG_ENABLE;
	else
		value.intval = POWER_SUPPLY_CAPACITY_OTG_DISABLE;

	if (psy) {
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_OTG, &value);
		if (ret) {
			pr_err("%s: fail to set power_suppy otg property(%d)\n",
				__func__, ret);
		}
	} else {
		pr_err("%s : psy is null!\n", __func__);
	}
#endif
}

/*-----------------------MXT224  TOUCH DRIVER by Xtopher-----------------------*/
static struct c1_charging_status_callbacks {
	void	(*tsp_set_charging_cable) (int type);
} charging_cbs;

static bool is_cable_attached;
/*-----------------------MXT224  TOUCH DRIVER by Xtopher-----------------------*/

static void fsa9480_usb_cb(bool attached)
{
#ifdef CONFIG_BATTERY_SEC	
	union power_supply_propval value;
	int ret = 0;
#endif

	struct power_supply *psy = power_supply_get_by_name("battery");
	struct usb_gadget *gadget = platform_get_drvdata(&msm_device_gadget_peripheral);
	printk(KERN_ERR "fsa9480_usb_cb attached %d\n", attached);

	if(!psy) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return;
	}

	if (gadget) {
		if (!usb_access_lock) {
			if (attached)
				usb_gadget_vbus_connect(gadget);
			else
				usb_gadget_vbus_disconnect(gadget);
		}
		else  // for usb lock
		{
			usb_gadget_vbus_disconnect(gadget);
		}
	}

	device_attached = attached ? DEV_TYPE_USB : DEV_TYPE_NONE;
	set_cable_status = attached ? CABLE_TYPE_USB : CABLE_TYPE_NONE;

#ifdef CONFIG_BATTERY_MSM8X60
	if (set_cable_status == CABLE_TYPE_USB)
		msm_charger_vbus_draw(450);
#endif

#ifdef CONFIG_BATTERY_SEC
	switch(set_cable_status) {
		case CABLE_TYPE_USB:
			value.intval = POWER_SUPPLY_TYPE_USB;
			break;
		case CABLE_TYPE_NONE:
			value.intval = POWER_SUPPLY_TYPE_BATTERY;
			break;
		default:
			pr_err("%s: invalid cable status:%d\n", __func__, set_cable_status);
			return;
	}
	
	if (charging_cbs.tsp_set_charging_cable)
		charging_cbs.tsp_set_charging_cable(set_cable_status);
	if(set_cable_status)
		is_cable_attached = true;
	else 
		is_cable_attached =false;

	if (psy) {
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
		if (ret) {
			pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
				__func__, ret);
		}
	} else {
		pr_err("%s : psy is null!\n", __func__);
	}
#endif
}

static void fsa9480_charger_cb(bool attached)
{
#ifdef CONFIG_BATTERY_SEC	
	union power_supply_propval value;
	int ret = 0;
#endif

	struct power_supply *psy = power_supply_get_by_name("battery");
	printk(KERN_ERR "fsa9480_charger_cb attached %d\n", attached);

	if(!psy) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return;
	}
	
	device_attached = attached ? DEV_TYPE_CHARGER : DEV_TYPE_NONE;
	set_cable_status = attached ? CABLE_TYPE_AC : CABLE_TYPE_NONE;

#ifdef CONFIG_BATTERY_MSM8X60
	if (set_cable_status == CABLE_TYPE_AC)
		msm_charger_vbus_draw(650);
#endif

#ifdef CONFIG_BATTERY_SEC
	switch(set_cable_status) {
		case CABLE_TYPE_AC:
			value.intval = POWER_SUPPLY_TYPE_MAINS;
			is_cable_attached = true;			
			break;
		case CABLE_TYPE_NONE:
			value.intval = POWER_SUPPLY_TYPE_BATTERY;
			is_cable_attached = false;			
			break;
		default:
			pr_err("%s: invalid cable status:%d\n", __func__, set_cable_status);
			return;
	}
	
	if (charging_cbs.tsp_set_charging_cable)
		charging_cbs.tsp_set_charging_cable(set_cable_status);
	if(set_cable_status)
		is_cable_attached = true;
	else 
		is_cable_attached =false;

	if (psy) {
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
		if (ret) {
			pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
				__func__, ret);
		}
	} else {
		pr_err("%s : psy is null!\n", __func__);
	}
#endif
}

static void fsa9480_jig_cb(bool attached)
{
#ifdef CONFIG_BATTERY_SEC	
	union power_supply_propval value;
	int ret = 0;
#endif
	struct power_supply *psy = power_supply_get_by_name("battery");

	int jig_status = 0;

	if(!psy) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return;
	}

	printk(KERN_ERR "fsa9480_jig_cb attached %d\n", attached);
	jig_status = attached ? 1 : 0;
	device_attached = attached ? DEV_TYPE_JIG : DEV_TYPE_NONE;
	set_cable_status = attached ? CABLE_TYPE_UARTOFF : CABLE_TYPE_NONE;

#ifdef CONFIG_BATTERY_SEC
	switch(set_cable_status) {
		case CABLE_TYPE_UARTOFF:
			value.intval = POWER_SUPPLY_TYPE_UARTOFF;
			break;
		case CABLE_TYPE_NONE:
			value.intval = POWER_SUPPLY_TYPE_BATTERY;
			break;
		default:
			pr_err("%s: invalid cable status:%d\n", __func__, set_cable_status);
			return;
	}
	
	if (charging_cbs.tsp_set_charging_cable)
		charging_cbs.tsp_set_charging_cable(set_cable_status);
	if(set_cable_status)
		is_cable_attached = true;
	else 
		is_cable_attached =false;

	if (psy) {	
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
		if (ret) {
			pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
				__func__, ret);
		}
	} else {
		pr_err("%s : psy is null!\n", __func__);
	}
#endif
}

static struct switch_dev switch_dock = {
	.name = "dock",
};

static void fsa9480_mhl_cb(bool attached)
{
#ifdef CONFIG_BATTERY_SEC	
	union power_supply_propval value;
	int ret = 0;
#endif
	struct power_supply *psy = power_supply_get_by_name("battery");
	if(!psy) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return;
	}
	printk(KERN_ERR "fsa9480_mhl_cb attached %d\n", attached);
	
	device_attached = attached ? DEV_TYPE_MHL : DEV_TYPE_NONE;
	set_cable_status = attached ? CABLE_TYPE_MISC : CABLE_TYPE_NONE;

#ifdef CONFIG_BATTERY_SEC
	switch(set_cable_status) {
		case CABLE_TYPE_USB:
			value.intval = POWER_SUPPLY_TYPE_USB;
			break;
		case CABLE_TYPE_MISC:
			value.intval = POWER_SUPPLY_TYPE_MISC;
			break;
		case CABLE_TYPE_NONE:
			value.intval = POWER_SUPPLY_TYPE_BATTERY;
			break;		
		default:
			pr_err("%s: invalid cable status:%d\n", __func__, set_cable_status);
			return;
	}

	if (charging_cbs.tsp_set_charging_cable)
		charging_cbs.tsp_set_charging_cable(set_cable_status);
	if(set_cable_status)
		is_cable_attached = true;
	else 
		is_cable_attached =false;

	if (psy) {
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
		if (ret) {
			pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
				__func__, ret);
		}
	} else {
		pr_err("%s : psy is null!\n", __func__);
	}
#endif
}


static void fsa9480_deskdock_cb(bool attached)
{
#ifdef CONFIG_BATTERY_SEC	
	union power_supply_propval value;
	int ret = 0;
#endif
	// static struct regulator *ldo1_1p2;
	// int rc = 0;
	struct power_supply *psy = power_supply_get_by_name("battery");
	if(!psy) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return;
	}

	printk(KERN_ERR "fsa9480_deskdock_cb attached %d\n", attached);
	
	if (attached)
		switch_set_state(&switch_dock, 1);
	else
		switch_set_state(&switch_dock, 0);
	
	device_attached = attached ? DEV_TYPE_DESKDOCK : DEV_TYPE_NONE;
	set_cable_status = attached ? CABLE_TYPE_CARDOCK : CABLE_TYPE_NONE;

#ifdef CONFIG_BATTERY_SEC
	switch(set_cable_status) {
		case CABLE_TYPE_USB:
			value.intval = POWER_SUPPLY_TYPE_USB;
			break;
		case CABLE_TYPE_MISC:
			value.intval = POWER_SUPPLY_TYPE_MISC;
			break;
		case CABLE_TYPE_CARDOCK:
			value.intval = POWER_SUPPLY_TYPE_CARDOCK;
			break;
		case CABLE_TYPE_NONE:
			value.intval = POWER_SUPPLY_TYPE_BATTERY;
			break;
		default:
			pr_err("%s: invalid cable status:%d\n", __func__, set_cable_status);
			return;
	}
		
	if (charging_cbs.tsp_set_charging_cable)
		charging_cbs.tsp_set_charging_cable(set_cable_status);
	if(set_cable_status)
		is_cable_attached = true;
	else 
		is_cable_attached =false;

	if (psy) {	
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
		if (ret) {
			pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
				__func__, ret);
		}
	} else {
		pr_err("%s : psy is null!\n", __func__);
	}
#endif
}


static void fsa9480_cardock_cb(bool attached)
{
#ifdef CONFIG_BATTERY_SEC	
	union power_supply_propval value;
	int ret = 0;
#endif
	// static struct regulator *ldo1_1p2;
	// int rc = 0;
	struct power_supply *psy = power_supply_get_by_name("battery");
	if(!psy) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return;
	}

	printk(KERN_ERR "fsa9480_deskdock_cb attached %d\n", attached);
		
	if (attached)
		switch_set_state(&switch_dock, 2);
	else
		switch_set_state(&switch_dock, 0);
	
	device_attached = attached ? DEV_TYPE_CARDOCK : DEV_TYPE_NONE;
	set_cable_status = attached ? CABLE_TYPE_CARDOCK : CABLE_TYPE_NONE;

#ifdef CONFIG_BATTERY_SEC
	switch(set_cable_status) {
		case CABLE_TYPE_CARDOCK:
			value.intval = POWER_SUPPLY_TYPE_CARDOCK;
			break;
		case CABLE_TYPE_NONE:
			value.intval = POWER_SUPPLY_TYPE_BATTERY;
			break;
		default:
			pr_err("%s: invalid cable status:%d\n", __func__, set_cable_status);
			return;
	}
	
	if (charging_cbs.tsp_set_charging_cable)
		charging_cbs.tsp_set_charging_cable(set_cable_status);
	if(set_cable_status)
		is_cable_attached = true;
	else 
		is_cable_attached =false;

	if (psy) {	
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
		if (ret) {
			pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
				__func__, ret);
		}
	} else {
		pr_err("%s : psy is null!\n", __func__);
	}
#endif
}

static void fsa9480_reset_cb(void)
{
	int ret;

	/* for CarDock, DeskDock */
	ret = switch_dev_register(&switch_dock);
	if (ret < 0)
		pr_err("Failed to register dock switch. %d\n", ret);
}

static void fsa9480_set_init_flag(void)
{
	fsa9480_init_flag = 1;
}

static struct fsa9480_platform_data fsa9480_pdata = {
	.otg_cb = fsa9480_otg_cb,
	.usb_cb = fsa9480_usb_cb,
	.charger_cb = fsa9480_charger_cb,
	.jig_cb = fsa9480_jig_cb,
	.deskdock_cb = fsa9480_deskdock_cb,
	.cardock_cb = fsa9480_cardock_cb,
	.reset_cb = fsa9480_reset_cb,
	.set_init_flag = fsa9480_set_init_flag,
	.mhl_cb = fsa9480_mhl_cb,
};

static struct i2c_board_info micro_usb_i2c_devices_info[] __initdata = {
	{
		I2C_BOARD_INFO("fsa9480", 0x4A >> 1),
		.platform_data = &fsa9480_pdata,
		.irq = MSM_GPIO_TO_INT(123),
	},
};

#if defined(CONFIG_KOR_MODEL_SHV_E160L)
static struct i2c_gpio_platform_data usb_i2c_gpio_data = {
	.sda_pin		= GPIO_USB_I2C_SDA,
	.scl_pin		= GPIO_USB_I2C_SCL,
	.udelay 		= 2,
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

static struct platform_device usb_i2c_gpio_device = {
	.name			= "i2c-gpio",
	.id			= MSM_GSBI12_QUP_I2C_BUS_ID,
	.dev.platform_data	= &usb_i2c_gpio_data,
};
#endif

static int sec_switch_get_cable_status(void)
{
	return set_cable_status;
}

static int sec_switch_get_phy_init_status(void)
{
	return fsa9480_init_flag;
}

int sec_switch_get_attached_device(void)
{
	return device_attached;
}
EXPORT_SYMBOL(sec_switch_get_attached_device);

static void sec_switch_set_vbus_status(u8 mode)
{
	if (mode == USB_VBUS_AP_ON) {
		printk(KERN_ERR "sec_switch_set_vbus_status USB_VBUS_AP_ON %d\n", mode);
	}
	else {
		printk(KERN_ERR "sec_switch_set_vbus_status USB_VBUS_AP_OFF %d\n", mode);
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
}

static struct sec_switch_platform_data sec_switch_pdata = {
	.set_vbus_status = sec_switch_set_vbus_status,
	.set_usb_gadget_vbus = sec_switch_set_usb_gadget_vbus,
	.get_cable_status = sec_switch_get_cable_status,
	.get_phy_init_status = sec_switch_get_phy_init_status,
	.get_attached_device = sec_switch_get_attached_device,
};

struct platform_device sec_device_switch = {
	.name	= "sec_switch",
	.id	= 1,
	.dev	= {
		.platform_data	= &sec_switch_pdata,
	}
};

#if 1
static void LTE_switch_init(void)
{
	sec_class = class_create(THIS_MODULE, "sec");
	pr_err("LTE_switch_init!\n");

	if (IS_ERR(sec_class))
		pr_err("Failed to create class(sec)!\n");

	switch_dev = device_create(sec_class, NULL, 0, NULL, "switch");

	if (IS_ERR(switch_dev))
		pr_err("Failed to create device(switch)!\n");

		pr_err("LTE_switch_init end!\n");

	
};
#endif
#endif /* CONFIG_USB_SWITCH_FSA9480 */


#ifdef CONFIG_FB_MSM_LCDC_NT35582_WVGA

#define GPIO_NT35582_RESET			94
#define GPIO_NT35582_BL_EN_HW_PIN	24
#define GPIO_NT35582_BL_EN	\
	PM8058_GPIO_PM_TO_SYS(GPIO_NT35582_BL_EN_HW_PIN - 1)

static int lcdc_nt35582_pmic_gpio[] = {GPIO_NT35582_BL_EN };

static struct msm_panel_common_pdata lcdc_nt35582_panel_data = {
	.gpio_num = lcdc_nt35582_pmic_gpio,
};

static struct platform_device lcdc_nt35582_panel_device = {
	.name = LCDC_NT35582_PANEL_NAME,
	.id = 0,
	.dev = {
		.platform_data = &lcdc_nt35582_panel_data,
	}
};

static struct spi_board_info lcdc_nt35582_spi_board_info[] __initdata = {
	{
		.modalias     = "lcdc_nt35582_spi",
		.mode         = SPI_MODE_0,
		.bus_num      = 0,
		.chip_select  = 0,
		.max_speed_hz = 1100000,
	}
};
#endif

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
static int hdmi_gpio_config(int on);
static int hdmi_cec_power(int on);
static int hdmi_panel_power(int on);

static struct msm_hdmi_platform_data hdmi_msm_data = {
	.irq = HDMI_IRQ,
	.enable_5v = hdmi_enable_5v,
	.core_power = hdmi_core_power,
	.cec_power = hdmi_cec_power,
	.panel_power = hdmi_panel_power,
	.gpio_config = hdmi_gpio_config,
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

#define FPGA_3D_GPIO_CONFIG_ADDR	0x1D00017A

static struct mipi_dsi_panel_platform_data novatek_pdata = {
	.fpga_3d_config_addr  = FPGA_3D_GPIO_CONFIG_ADDR,
	.fpga_ctrl_mode = FPGA_EBI2_INTF,
};

static struct platform_device mipi_dsi_novatek_panel_device = {
	.name = "mipi_novatek",
	.id = 0,
	.dev = {
		.platform_data = &novatek_pdata,
	}
};
#ifdef CONFIG_FB_MSM_MIPI_S6D6AA0_WXGA_PANEL
static struct platform_device mipi_dsi_s6d6aa0_wxga_panel_device = {
	.name = "mipi_s6d6aa0_wxga",
	.id = 0,
};
#endif
#ifdef CONFIG_FB_MSM_MIPI_S6E8AA0_HD720_PANEL
static struct platform_device mipi_dsi_s6e8aa0_hd720_panel_device = {
	.name = "mipi_s6e8aa0_hd720",
	.id = 0,
};
#endif
#ifdef CONFIG_FB_MSM_MIPI_S6E8AA0_WXGA_Q1_PANEL
static struct platform_device mipi_dsi_s6e8aa0_wxga_q1_panel_device = {
	.name = "mipi_s6e8aa0_wxga_q1",
	.id = 0,
};
#endif
#endif /* CONFIG_FB_MSM_MIPI_DSI */


#ifndef CONFIG_SEC_KERNEL_REBASE_FOR_PMEM_OPTIMIZATION /* onlyjazz.ub02 : workaournd for kernel memory crash by TZBSP XPU */

#define TZBSP_EXEC_BASE 0x42E20000
#define TZBSP_EXEC_SIZE 0x000E0000

#endif /* onlyjazz.ub02 : end */

static void __init msm8x60_allocate_memory_regions(void)
{
	void *addr;
	unsigned long size;
	
#ifndef CONFIG_SEC_KERNEL_REBASE_FOR_PMEM_OPTIMIZATION /* onlyjazz.ub02 : workaournd for kernel memory crash by TZBSP XPU */
    int ret;
	 ret = reserve_bootmem(TZBSP_EXEC_BASE, TZBSP_EXEC_SIZE, BOOTMEM_EXCLUSIVE);
	 if (ret < 0) {
		  printk(KERN_ERR "reserve_bootmem for TZBSP EXEC BASE failed\n");
	 }

#endif /* onlyjazz.ub02 : end */
	if (hdmi_is_primary)
		size = roundup((1920 * 1088 * 4 * 2), 4096);
	else
		size = MSM_FB_SIZE;

	addr = alloc_bootmem_align(size, 0x1000);
	msm_fb_resources[0].start = __pa(addr);
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for fb\n",
		size, addr, __pa(addr));

}

void __init msm8x60_set_display_params(char *prim_panel, char *ext_panel)
{
	if (strnlen(prim_panel, PANEL_NAME_MAX_LEN)) {
		strlcpy(msm_fb_pdata.prim_panel_name, prim_panel,
			PANEL_NAME_MAX_LEN);
		pr_debug("msm_fb_pdata.prim_panel_name %s\n",
			msm_fb_pdata.prim_panel_name);

		if (!strncmp((char *)msm_fb_pdata.prim_panel_name,
			HDMI_PANEL_NAME, strnlen(HDMI_PANEL_NAME,
				PANEL_NAME_MAX_LEN))) {
			pr_debug("HDMI is the primary display by"
				" boot parameter\n");
			hdmi_is_primary = 1;
			set_mdp_clocks_for_wuxga();
		}
	}
	if (strnlen(ext_panel, PANEL_NAME_MAX_LEN)) {
		strlcpy(msm_fb_pdata.ext_panel_name, ext_panel,
			PANEL_NAME_MAX_LEN);
		pr_debug("msm_fb_pdata.ext_panel_name %s\n",
			msm_fb_pdata.ext_panel_name);
	}
}

#ifdef CONFIG_KEYPAD_CYPRESS_TOUCH // touchkey
#define MSM_TKEY_I2C_BUS_ID		13

/* I2C2 */
static struct i2c_board_info tkey_i2c_devices[] = {
    {
#if defined(CONFIG_KOR_MODEL_SHV_E160S) ||defined(CONFIG_KOR_MODEL_SHV_E160K) \
	||defined(CONFIG_KOR_MODEL_SHV_E160L) || defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_JPN_MODEL_SC_05D)
        I2C_BOARD_INFO("sec_touchkey", 0x20),
#else
        I2C_BOARD_INFO("melfas_touchkey", 0x20),
#endif
    },
};

static  struct  i2c_gpio_platform_data  tkey_i2c_gpio_data = {
        .sda_pin                = GPIO_TKEY_I2C_SDA,
        .scl_pin                = GPIO_TKEY_I2C_SCL,
        .udelay                 = 0,    /* 250KHz */
};
static struct platform_device tkey_i2c_gpio_device = {
        .name                           = "i2c-gpio",
        .id                                     = MSM_TKEY_I2C_BUS_ID,
        .dev.platform_data      = &tkey_i2c_gpio_data,
};
#endif

#if defined(CONFIG_TOUCHSCREEN_CYTTSP_I2C) || \
		defined(CONFIG_TOUCHSCREEN_CYTTSP_I2C_MODULE)
/*virtual key support */
static ssize_t tma300_vkeys_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
	__stringify(EV_KEY) ":" __stringify(KEY_BACK) ":60:900:90:120"
	":" __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":180:900:90:120"
	":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":300:900:90:120"
	":" __stringify(EV_KEY) ":" __stringify(KEY_SEARCH) ":420:900:90:120"
	"\n");
}

static struct kobj_attribute tma300_vkeys_attr = {
	.attr = {
		.mode = S_IRUGO,
	},
	.show = &tma300_vkeys_show,
};

static struct attribute *tma300_properties_attrs[] = {
	&tma300_vkeys_attr.attr,
	NULL
};

static struct attribute_group tma300_properties_attr_group = {
	.attrs = tma300_properties_attrs,
};

static struct kobject *properties_kobj;



#define CYTTSP_TS_GPIO_IRQ	61
static int cyttsp_platform_init(struct i2c_client *client)
{
	int rc = -EINVAL;
	struct regulator *pm8058_l5 = NULL, *pm8058_s3;

	if (machine_is_msm8x60_fluid()) {
		pm8058_l5 = regulator_get(NULL, "8058_l5");
		if (IS_ERR(pm8058_l5)) {
			pr_err("%s: regulator get of 8058_l5 failed (%ld)\n",
				__func__, PTR_ERR(pm8058_l5));
			rc = PTR_ERR(pm8058_l5);
			return rc;
		}
		rc = regulator_set_voltage(pm8058_l5, 2850000, 2850000);
		if (rc) {
			pr_err("%s: regulator_set_voltage of 8058_l5 failed(%d)\n",
				__func__, rc);
			goto reg_l5_put;
		}

		rc = regulator_enable(pm8058_l5);
		if (rc) {
			pr_err("%s: regulator_enable of 8058_l5 failed(%d)\n",
				__func__, rc);
			goto reg_l5_put;
		}
	}
	/* vote for s3 to enable i2c communication lines */
	pm8058_s3 = regulator_get(NULL, "8058_s3");
	if (IS_ERR(pm8058_s3)) {
		pr_err("%s: regulator get of 8058_s3 failed (%ld)\n",
			__func__, PTR_ERR(pm8058_s3));
		rc = PTR_ERR(pm8058_s3);
		goto reg_l5_disable;
	}

	rc = regulator_set_voltage(pm8058_s3, 1800000, 1800000);
	if (rc) {
		pr_err("%s: regulator_set_voltage() = %d\n",
			__func__, rc);
		goto reg_s3_put;
	}

	rc = regulator_enable(pm8058_s3);
	if (rc) {
		pr_err("%s: regulator_enable of 8058_l5 failed(%d)\n",
			__func__, rc);
		goto reg_s3_put;
	}

	/* wait for vregs to stabilize */
	usleep_range(10000, 10000);

	/* check this device active by reading first byte/register */
	rc = i2c_smbus_read_byte_data(client, 0x01);
	if (rc < 0) {
		pr_err("%s: i2c sanity check failed\n", __func__);
		goto reg_s3_disable;
	}

	/* virtual keys */
	if (machine_is_msm8x60_fluid()) {
		tma300_vkeys_attr.attr.name = "virtualkeys.cyttsp-i2c";
		properties_kobj = kobject_create_and_add("board_properties",
					NULL);
		if (properties_kobj)
			rc = sysfs_create_group(properties_kobj,
				&tma300_properties_attr_group);
		if (!properties_kobj || rc)
			pr_err("%s: failed to create board_properties\n",
					__func__);
	}
	return CY_OK;

reg_s3_disable:
	regulator_disable(pm8058_s3);
reg_s3_put:
	regulator_put(pm8058_s3);
reg_l5_disable:
	if (machine_is_msm8x60_fluid())
		regulator_disable(pm8058_l5);
reg_l5_put:
	if (machine_is_msm8x60_fluid())
		regulator_put(pm8058_l5);
	return rc;
}

#if !defined(CONFIG_JPN_MODEL_SC_05D)
/* TODO: Put the regulator to LPM / HPM in suspend/resume*/
static int cyttsp_platform_suspend(struct i2c_client *client)
{
	msleep(20);

	return CY_OK;
}

static int cyttsp_platform_resume(struct i2c_client *client)
{
	/* add any special code to strobe a wakeup pin or chip reset */
	msleep(10);

	return CY_OK;
}

static struct cyttsp_platform_data cyttsp_fluid_pdata = {
	.flags = 0x04,
	.gen = CY_GEN3,	/* or */
	.use_st = CY_USE_ST,
	.use_mt = CY_USE_MT,
	.use_hndshk = CY_SEND_HNDSHK,
	.use_trk_id = CY_USE_TRACKING_ID,
	.use_sleep = CY_USE_DEEP_SLEEP_SEL | CY_USE_LOW_POWER_SEL,
	.use_gestures = CY_USE_GESTURES,
	/* activate up to 4 groups
	 * and set active distance
	 */
	.gest_set = CY_GEST_GRP1 | CY_GEST_GRP2 |
				CY_GEST_GRP3 | CY_GEST_GRP4 |
				CY_ACT_DIST,
	/* change act_intrvl to customize the Active power state
	 * scanning/processing refresh interval for Operating mode
	 */
	.act_intrvl = CY_ACT_INTRVL_DFLT,
	/* change tch_tmout to customize the touch timeout for the
	 * Active power state for Operating mode
	 */
	.tch_tmout = CY_TCH_TMOUT_DFLT,
	/* change lp_intrvl to customize the Low Power power state
	 * scanning/processing refresh interval for Operating mode
	 */
	.lp_intrvl = CY_LP_INTRVL_DFLT,
	.sleep_gpio = -1,
	.resout_gpio = -1,
	.irq_gpio = CYTTSP_TS_GPIO_IRQ,
	.resume = cyttsp_platform_resume,
	.suspend = cyttsp_platform_suspend,
	.init = cyttsp_platform_init,
};

static struct cyttsp_platform_data cyttsp_tmg240_pdata = {
	.panel_maxx = 1083,
	.panel_maxy = 659,
	.disp_minx = 30,
	.disp_maxx = 1053,
	.disp_miny = 30,
	.disp_maxy = 629,
	.correct_fw_ver = 8,
	.fw_fname = "cyttsp_8660_ffa.hex",
	.flags = 0x00,
	.gen = CY_GEN2,	/* or */
	.use_st = CY_USE_ST,
	.use_mt = CY_USE_MT,
	.use_hndshk = CY_SEND_HNDSHK,
	.use_trk_id = CY_USE_TRACKING_ID,
	.use_sleep = CY_USE_DEEP_SLEEP_SEL | CY_USE_LOW_POWER_SEL,
	.use_gestures = CY_USE_GESTURES,
	/* activate up to 4 groups
	 * and set active distance
	 */
	.gest_set = CY_GEST_GRP1 | CY_GEST_GRP2 |
				CY_GEST_GRP3 | CY_GEST_GRP4 |
				CY_ACT_DIST,
	/* change act_intrvl to customize the Active power state
	 * scanning/processing refresh interval for Operating mode
	 */
	.act_intrvl = CY_ACT_INTRVL_DFLT,
	/* change tch_tmout to customize the touch timeout for the
	 * Active power state for Operating mode
	 */
	.tch_tmout = CY_TCH_TMOUT_DFLT,
	/* change lp_intrvl to customize the Low Power power state
	 * scanning/processing refresh interval for Operating mode
	 */
	.lp_intrvl = CY_LP_INTRVL_DFLT,
	.sleep_gpio = -1,
	.resout_gpio = -1,
	.irq_gpio = CYTTSP_TS_GPIO_IRQ,
	.resume = cyttsp_platform_resume,
	.suspend = cyttsp_platform_suspend,
	.init = cyttsp_platform_init,
	.disable_ghost_det = true,
};
static void cyttsp_set_params(void)
{
	if (SOCINFO_VERSION_MAJOR(socinfo_get_platform_version()) < 3) {
		cyttsp_fluid_pdata.fw_fname = "cyttsp_8660_fluid_p2.hex";
		cyttsp_fluid_pdata.panel_maxx = 539;
		cyttsp_fluid_pdata.panel_maxy = 994;
		cyttsp_fluid_pdata.disp_minx = 30;
		cyttsp_fluid_pdata.disp_maxx = 509;
		cyttsp_fluid_pdata.disp_miny = 60;
		cyttsp_fluid_pdata.disp_maxy = 859;
		cyttsp_fluid_pdata.correct_fw_ver = 4;
	} else {
		cyttsp_fluid_pdata.fw_fname = "cyttsp_8660_fluid_p3.hex";
		cyttsp_fluid_pdata.panel_maxx = 550;
		cyttsp_fluid_pdata.panel_maxy = 1013;
		cyttsp_fluid_pdata.disp_minx = 35;
		cyttsp_fluid_pdata.disp_maxx = 515;
		cyttsp_fluid_pdata.disp_miny = 69;
		cyttsp_fluid_pdata.disp_maxy = 869;
		cyttsp_fluid_pdata.correct_fw_ver = 5;
	}

}

static struct i2c_board_info cyttsp_fluid_info[] __initdata = {
	{
		I2C_BOARD_INFO(CY_I2C_NAME, 0x24),
		.platform_data = &cyttsp_fluid_pdata,
#ifndef CY_USE_TIMER
		.irq = MSM_GPIO_TO_INT(CYTTSP_TS_GPIO_IRQ),
#endif /* CY_USE_TIMER */
	},
};

static struct i2c_board_info cyttsp_ffa_info[] __initdata = {
	{
		I2C_BOARD_INFO(CY_I2C_NAME, 0x3b),
		.platform_data = &cyttsp_tmg240_pdata,
#ifndef CY_USE_TIMER
		.irq = MSM_GPIO_TO_INT(CYTTSP_TS_GPIO_IRQ),
#endif /* CY_USE_TIMER */
	},
};
#endif
#endif /* CONFIG_TOUCHSCREEN_CYTTSP_I2C */


#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT540E)
#define MXT540E_MAX_MT_FINGERS		10
#define MXT540E_CHRGTIME_BATT		48
#define MXT540E_CHRGTIME_CHRG		48
#if defined(CONFIG_USA_MODEL_SGH_I717)
#define MXT540E_THRESHOLD_BATT		35
#else
#define MXT540E_THRESHOLD_BATT		50
#endif
#define MXT540E_THRESHOLD_CHRG		40
#if defined (CONFIG_JPN_MODEL_SC_05D)
#define MXT540E_ACTVSYNCSPERX_BATT	24
#define MXT540E_ACTVSYNCSPERX_CHRG	28
#else
#define MXT540E_ACTVSYNCSPERX_BATT		34
#define MXT540E_ACTVSYNCSPERX_CHRG		34
#endif
#if defined(CONFIG_USA_MODEL_SGH_I717)
#define MXT540E_CALCFG_BATT		242
#else
#define MXT540E_CALCFG_BATT		98
#endif
#define MXT540E_CALCFG_CHRG		114
#define MXT540E_ATCHFRCCALTHR_WAKEUP		8
#define MXT540E_ATCHFRCCALRATIO_WAKEUP		180
#define MXT540E_ATCHFRCCALTHR_NORMAL		40
#define MXT540E_ATCHFRCCALRATIO_NORMAL		55

#define TSP_IRQ_READY_DELAY 130

static u8 t7_config_540e[] = {GEN_POWERCONFIG_T7,
		48, 255, 25};

static u8 t8_config_540e[] = {GEN_ACQUISITIONCONFIG_T8,
		MXT540E_CHRGTIME_BATT, 0, 5, 1, 0, 0, 4, 20, MXT540E_ATCHFRCCALTHR_WAKEUP, MXT540E_ATCHFRCCALRATIO_WAKEUP};

static u8 t9_config_540e[] = {TOUCH_MULTITOUCHSCREEN_T9,
		131, 0, 0, 16, 26, 0, 192, MXT540E_THRESHOLD_BATT, 2, 6,
#if defined(CONFIG_JPN_MODEL_SC_05D)
		10, 10, 10, 80, MXT540E_MAX_MT_FINGERS, 20, 40, 20, 31, 3,
#else
		10, 15, 10, 80, MXT540E_MAX_MT_FINGERS, 20, 40, 20, 31, 3,
#endif
		255, 4, 3, 3, 2, 2, 136, 60, 136, 40,
		18, 15, 0, 0, 0};

static u8 t15_config_540e[] = {TOUCH_KEYARRAY_T15,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static u8 t18_config_540e[] = {SPT_COMCONFIG_T18,
		0, 0};

static u8 t19_config_540e[] = {SPT_GPIOPWM_T19,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static u8 t25_config_540e[] = {SPT_SELFTEST_T25,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static u8 t38_config_540e[] = {SPT_USERDATA_T38,
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
		0,  0,  0,  0};

static u8 t40_config_540e[] = {PROCI_GRIPSUPPRESSION_T40,
		0, 0, 0, 0, 0};

static u8 t42_config_540e[] = {PROCI_TOUCHSUPPRESSION_T42,
		0, 0, 0, 0, 0, 0, 0, 0};

static u8 t43_config_540e[] = {SPT_DIGITIZER_T43,
		0, 0, 0, 0, 0, 0, 0};

static u8 t46_config_540e[] = {SPT_CTECONFIG_T46,
		0, 0, 16, MXT540E_ACTVSYNCSPERX_BATT, 0, 0, 1, 0};

static u8 t47_config_540e[] = {PROCI_STYLUS_T47,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static u8 t48_config_540e[] = {PROCG_NOISESUPPRESSION_T48,
		3, 132, MXT540E_CALCFG_BATT, 0, 0, 0, 0, 0, 1, 2,
		0, 0, 0, 6, 6, 0, 0, 28, 4, 64,
		10, 0, 20, 6, 0, 30, 0, 0, 0, 0,
#if defined(CONFIG_USA_MODEL_SGH_I717)
		0, 0, 0, 0, 128, MXT540E_THRESHOLD_BATT, 2, 10, 10, 80,
#elif defined(CONFIG_JPN_MODEL_SC_05D)
		0, 0, 0, 0, 192, MXT540E_THRESHOLD_BATT, 2, 10, 10, 47,
#else
		0, 0, 0, 0, 192, MXT540E_THRESHOLD_BATT, 2, 15, 10, 47,
#endif
		MXT540E_MAX_MT_FINGERS, 5, 20, 253, 0, 7, 7, 160, 55, 136,
		0, 18, 5, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0};

static u8 t48_config_chrg_540e[] = {PROCG_NOISESUPPRESSION_T48,
		3, 132, MXT540E_CALCFG_CHRG, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 6, 6, 0, 0, 36, 4, 64,
		10, 0, 10, 6, 0, 20, 0, 0, 0, 0,
		0, 0, 0, 0, 112, MXT540E_THRESHOLD_CHRG, 2, 10, 5, 47,
		MXT540E_MAX_MT_FINGERS, 5, 20, 253, 0, 7, 7, 160, 55, 136,
		0, 18, 10, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0};

static u8 t52_config_540e[] = {TOUCH_PROXKEY_T52,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static u8 t55_config_540e[] = {ADAPTIVE_T55,
		0, 0, 0, 0, 0, 0};

static u8 t57_config_540e[] = {SPT_GENERICDATA_T57,
		131, 25, 0};

static u8 end_config_540e[] = {RESERVED_T255};

static const u8 *mxt540e_config[] = {
	t7_config_540e,
	t8_config_540e,
	t9_config_540e,
	t15_config_540e,
	t18_config_540e,
	t19_config_540e,
	t25_config_540e,
	t38_config_540e,
	t40_config_540e,
	t42_config_540e,
	t43_config_540e,
	t46_config_540e,
	t47_config_540e,
	t48_config_540e,
	t52_config_540e,
	t55_config_540e,
	t57_config_540e,
	end_config_540e,
};

static void config_gpio_table_tsp(uint32_t *table, int len)
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

static void mxt540e_power_on(void)
{
	int rc;
	struct regulator *L1;
	struct regulator *L4;	
#if defined (CONFIG_JPN_MODEL_SC_05D)
	static struct regulator *tsp_pannel_pwr_reg;
#endif

	printk(KERN_INFO "[TSP] %s: enter\n", __func__);
	{
#if defined(CONFIG_KOR_MODEL_SHV_E160L)
					L4 = regulator_get(NULL, "8058_l3");
#else
					L4 = regulator_get(NULL, "8901_l4");
#endif
				if (IS_ERR(L4)) {
					rc = PTR_ERR(L4);
					pr_err("%s: L4 get failed (%d)\n",
						   __func__, rc);
					return;
				}
			    rc = regulator_set_voltage(L4, 2800000, 2800000);
				if (rc) {
					printk("%s: debug p1\n", __func__);
					return;
				}

				L1 = regulator_get(NULL, "8901_l1");
				if (IS_ERR(L1)) {
					rc = PTR_ERR(L1);
					pr_err("%s: L1 get failed (%d)\n",
						   __func__, rc);
					return;
				}
				rc = regulator_set_voltage(L1, 3300000, 3300000);
				if (rc) {
					pr_err("%s: L1 set level failed (%d)\n",
						   __func__, rc);
					return;
	}

				if(regulator_is_enabled(L4)){
					printk(KERN_ERR "[TSP POWER ON   L4 2.8 was already enabled\n");
				} else {
					rc = regulator_enable(L4);
						printk(KERN_ERR "[TSP POWER ON  L4 2.8 enable\n");
					if (rc) {
                        pr_err("%s: L4 vreg enable failed (%d)\n",  __func__, rc);
						return;
					}
	}
	
				//	mdelay(110);  // don't use delay between ITO and LOGIG

				if(regulator_is_enabled(L1)){
					printk(KERN_ERR "[TSP POWER ON	L1 3.3 was already enabled\n");
				} else {
					rc = regulator_enable(L1);
					printk(KERN_ERR "[TSP POWER ON	L1 3.3 enable\n");
					if (rc) {
                        pr_err("%s: L1 vreg enable failed (%d)\n", __func__, rc);
						return;
					}
				}

				regulator_put(L4);
				regulator_put(L1);
				printk("%s: exit\n", __func__);
	}
	msleep(TSP_IRQ_READY_DELAY);

	rc = gpio_request(TOUCHSCREEN_IRQ, "tsp_irq");
	if(!rc)
		rc = gpio_direction_input(TOUCHSCREEN_IRQ);
	if (rc) {
		printk(KERN_NOTICE "%s: failed to set  TOUCHSCREEN_IRQ to INPUT\n", __func__);
	}
	gpio_free(TOUCHSCREEN_IRQ);
	printk("mxt540E_power_on is finished\n");
}


static uint32_t TSP_INT_off_gpio_table[] = {
    GPIO_CFG(TOUCHSCREEN_IRQ,  GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 
};

static void mxt540e_power_off(void)
{
	int rc;
	struct regulator *L1;
	struct regulator *L4;	

	printk("%s: enter\n", __func__);

	#if 0
	rc = gpio_request(TOUCHSCREEN_IRQ, "tsp_irq");
	if(!rc)
		rc = gpio_direction_output(TOUCHSCREEN_IRQ, 1);
	if (rc) {
		printk(KERN_NOTICE "%s: failed to set  TOUCHSCREEN_IRQ to OUTPUT(1)\n");
	}
	gpio_free(TOUCHSCREEN_IRQ);
#else
	config_gpio_table_tsp(TSP_INT_off_gpio_table, ARRAY_SIZE(TSP_INT_off_gpio_table));
#endif

		{

#if defined (CONFIG_KOR_MODEL_SHV_E160L)
		L4 = regulator_get(NULL, "8058_l3");
#else
			L4 = regulator_get(NULL, "8901_l4");
#endif
		if (IS_ERR(L4)) {
			rc = PTR_ERR(L4);
			pr_err("%s: L4 get failed (%d)\n",
				   __func__, rc);
			return;
		}
		rc = regulator_set_voltage(L4, 2800000, 2800000);
		if (rc) {
			pr_err("%s: L4 set level failed (%d)\n",
				   __func__, rc);
			return;
	}
		if(regulator_is_enabled(L4)){
			rc = regulator_disable(L4);
			printk(KERN_ERR "[TSP POWER OFF	 L4 2.8 disable\n");
			if (rc) {
				pr_err("%s: L4 vreg enable failed (%d)\n",
					   __func__, rc);
				return;
			}
	}
		regulator_put(L4);

		L1 = regulator_get(NULL, "8901_l1");
		if (IS_ERR(L1)) {
			rc = PTR_ERR(L1);
			pr_err("%s: L1 get failed (%d)\n",
				   __func__, rc);
			return;
		}
		rc = regulator_set_voltage(L1,3300000, 3300000);
		if (rc){
			printk("%s: debug p1\n", __func__);
			return;
		}
		if(regulator_is_enabled(L1)){
			rc = regulator_disable(L1);
			printk(KERN_ERR "[TSP POWER OFF	 L1 3.3 disable\n");
			if (rc) {
				pr_err("%s: L1 vreg enable failed (%d)\n",
					   __func__, rc);
				return;
			}
	}
		regulator_put(L1);
	}
	printk("mxt540E_power_off is finished\n"); 
}


static void mxt540e_register_callback(void *function)
{
	charging_cbs.tsp_set_charging_cable = function;
}

static void mxt540e_read_ta_status(bool *ta_status)
{
	*ta_status = is_cable_attached;
}

static struct mxt540e_platform_data mxt540e_data = {
	.max_finger_touches = MXT540E_MAX_MT_FINGERS,
	.gpio_read_done = TOUCHSCREEN_IRQ,
	.config_e = mxt540e_config,
	.min_x = 0,
	.max_x = 799,
	.min_y = 0,
	.max_y = 1279,
	.min_z = 0,
	.max_z = 255,
	.min_w = 0,
	.max_w = 30,
	.chrgtime_batt = MXT540E_CHRGTIME_BATT,
	.chrgtime_charging = MXT540E_CHRGTIME_CHRG,
	.tchthr_batt = MXT540E_THRESHOLD_BATT,
	.tchthr_charging = MXT540E_THRESHOLD_CHRG,
    .actvsyncsperx_batt = MXT540E_ACTVSYNCSPERX_BATT,
    .actvsyncsperx_charging = MXT540E_ACTVSYNCSPERX_CHRG,
	.calcfg_batt_e = MXT540E_CALCFG_BATT,
	.calcfg_charging_e = MXT540E_CALCFG_CHRG,
	.atchfrccalthr_e = MXT540E_ATCHFRCCALTHR_NORMAL,
	.atchfrccalratio_e = MXT540E_ATCHFRCCALRATIO_NORMAL,
	.t48_config_batt_e = t48_config_540e,
	.t48_config_chrg_e = t48_config_chrg_540e,
	.power_on = mxt540e_power_on,
	.power_off = mxt540e_power_off,
	.register_cb = mxt540e_register_callback,
	.read_ta_status = mxt540e_read_ta_status
};

static struct i2c_board_info mxt540e_board_info[] = {
	{
		I2C_BOARD_INFO(MXT540E_DEV_NAME, 0x4c),
		.irq           =  MSM_GPIO_TO_INT(TOUCHSCREEN_IRQ),			
		.platform_data = &mxt540e_data
	}
};
#endif /* defined (CONFIG_TOUCHSCREEN_ATMEL_MXT540E) */

#if defined (CONFIG_EPEN_WACOM_G5SP)
#define GPIO_PEN_IRQ	4
#define GPIO_PEN_SLP	5
#define GPIO_PEN_PDCT	6
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
#define GPIO_PEN_LDO_EN	7 //Xtopher_WACOM
#endif
#define GPIO_PEN_RESET	8
#define GPIO_PEN_SCL 103
#define GPIO_PEN_SDA 104

static int wacom_init_hw(void);
static int wacom_exit_hw(void);
static int wacom_suspend_hw(void);
static int wacom_resume_hw(void);
static int wacom_early_suspend_hw(void);
static int wacom_late_resume_hw(void);
static int wacom_reset_hw(void);
static void wacom_register_callbacks(struct wacom_g5_callbacks *cb);
static struct regulator *epen_ldo = NULL;

static struct wacom_g5_platform_data wacom_platform_data = {
	.x_invert = 1,
	.y_invert = 0,
	.xy_switch = 1,
	.min_x = 0,
	.max_x = WACOM_POSX_MAX,
	.min_y = 0,
	.max_y = WACOM_POSY_MAX,
	.min_pressure = 0,
	.max_pressure = WACOM_PRESSURE_MAX,
	.gpio_pendct = GPIO_PEN_PDCT,
	.init_platform_hw = wacom_init_hw,
	.exit_platform_hw =wacom_exit_hw,	
	.suspend_platform_hw = wacom_suspend_hw,
	.resume_platform_hw = wacom_resume_hw,
	.early_suspend_platform_hw = wacom_early_suspend_hw,
	.late_resume_platform_hw = wacom_late_resume_hw,
	.reset_platform_hw = wacom_reset_hw,
	.register_cb = wacom_register_callbacks,
};

static struct i2c_board_info wacom_g5sp_i2c_devices_info[] = {
	{
		I2C_BOARD_INFO("wacom_g5sp_i2c", 0x56),		
		.irq = MSM_GPIO_TO_INT(GPIO_PEN_IRQ), /* ANGRYBIRD */
		.platform_data = &wacom_platform_data
	}
};

static unsigned wacom_gpio_on[] = {
	GPIO_CFG(GPIO_PEN_IRQ, 	0, GPIO_CFG_INPUT, 	GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_PEN_SLP, 	0, GPIO_CFG_OUTPUT, 	GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_PEN_PDCT, 	0, GPIO_CFG_INPUT, 	GPIO_CFG_PULL_UP, GPIO_CFG_2MA),	
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	GPIO_CFG(GPIO_PEN_LDO_EN,0, GPIO_CFG_OUTPUT, 	GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
#endif
	GPIO_CFG(GPIO_PEN_RESET, 	0, GPIO_CFG_OUTPUT, 	GPIO_CFG_NO_PULL, GPIO_CFG_2MA),

	GPIO_CFG(GPIO_PEN_SCL,	2, GPIO_CFG_OUTPUT, 	GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_PEN_SDA, 	2, GPIO_CFG_OUTPUT, 	GPIO_CFG_NO_PULL, GPIO_CFG_2MA),	
};

#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
static unsigned wacom_gpio_sleep[] = {
	GPIO_CFG(GPIO_PEN_IRQ, 	0, GPIO_CFG_INPUT, 	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_PEN_PDCT, 0, GPIO_CFG_INPUT,	GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),			
};

static unsigned wacom_gpio_wake[] = {
	GPIO_CFG(GPIO_PEN_IRQ, 	0, GPIO_CFG_INPUT, 	GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(GPIO_PEN_PDCT, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),		
};
#endif
int wacom_vdd_enable(int onoff) // to turn off the wacom in sleep
{
	int rc;
	
	printk("[E-PEN] p6_wacom_vdd_enable or disable %d\n", onoff);

	if(epen_ldo == NULL) {
		epen_ldo = regulator_get(NULL, "8058_l8");
		//epen_ldo = regulator_get(&rpm_vreg_device[RPM_VREG_ID_PM8058_L8].dev, "8058_l8");
		if (IS_ERR(epen_ldo)) {
			rc = PTR_ERR(epen_ldo);
			printk("[E-PEN] %s: epen_ldo get failed (%d)\n", __func__, rc);
			return rc;
		}
	}
#if defined(CONFIG_USA_MODEL_SGH_I717)
	rc = regulator_set_voltage(epen_ldo, 3300000, 3300000);
#else
	rc = regulator_set_voltage(epen_ldo, 3000000, 3000000);
#endif
	if (rc) {
		printk("[E-PEN] %s: epen_ldo set level failed (%d)\n", __func__, rc);
		return rc;
	}
	
	if(onoff)
	{   
	    if(regulator_is_enabled(epen_ldo)){
		printk("[E-PEN] %s: [E-PEN is already ON (epen_ldo : 3.3 V)!!! \n", __func__);
	    }
	    else {
		    rc = regulator_enable(epen_ldo);
		    if (rc) {
			    printk("[E-PEN] %s: epen_ldovreg enable failed (%d)\n", __func__, rc);
			    return rc;
		    }
		else 
		    printk("[E-PEN] %s:epen_ldo 3.3 V on \n", __func__);
	    }
	}
	else 
	{
	    if(regulator_is_enabled(epen_ldo)){
		    rc = regulator_disable(epen_ldo);
		    if (rc) {
			    printk("[E-PEN] %s: epen_ldovreg disable failed (%d)\n", __func__, rc);
			    return rc;
		    }
		else 
		    printk("[E-PEN] %s: epen_ldo 3.3 V OFF \n", __func__);
	    }
	    else {
		printk("[E-PEN] %s: E-PEN is already OFF (epen_ldo : 3.3 V)!!! \n", __func__);
	    }
	}


	return 0;
}
EXPORT_SYMBOL(wacom_vdd_enable);



static void  wacom_gpio_init(void)
{
	int pin, rc;

	printk(KERN_INFO "[E-PEN] %s  \n", __FUNCTION__);
	
	for (pin= 0; pin < ARRAY_SIZE(wacom_gpio_on); pin++) {
		rc = gpio_tlmm_config(wacom_gpio_on[pin], GPIO_CFG_ENABLE);
		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",	__func__, wacom_gpio_on[pin], rc);
		}
	}
}

static void wacom_settle_down_befor_poweron(void)
{
#if !defined(CONFIG_KOR_MODEL_SHV_E160S) && !defined(CONFIG_KOR_MODEL_SHV_E160K) && !defined(CONFIG_KOR_MODEL_SHV_E160L)
	int ret;
#endif
	// int rc;
    	//static struct regulator *epen_ldo = NULL;
	
		printk(KERN_INFO "[E-PEN] %s \n", __FUNCTION__);
		wacom_gpio_init();
	
		gpio_set_value(GPIO_PEN_SLP, 0);
		gpio_set_value(GPIO_PEN_RESET, 0);
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
		gpio_set_value(GPIO_PEN_LDO_EN, 0);
#else //I717
    if(epen_ldo == NULL) {
    	epen_ldo = regulator_get(NULL, "8058_l8");
        //epen_ldo = regulator_get(&rpm_vreg_device[RPM_VREG_ID_PM8058_L8].dev, "8058_l8");

        if (IS_ERR(epen_ldo)) {
            printk(KERN_ERR "%s: can't get wacom ldo\b", __FUNCTION__);
            return;//SISO QCBSP modified to fix build break            
        }
#if defined(CONFIG_USA_MODEL_SGH_I717)
        ret = regulator_set_voltage(epen_ldo, 3300000, 3300000);
#else
	ret = regulator_set_voltage(epen_ldo, 3000000, 3000000);
#endif
        if(ret) {
		printk("%s: error setting voltage - epen_ldo\n", __FUNCTION__);
        }
	
	if(regulator_is_enabled(epen_ldo)){
		printk("[E-PEN] %s: [E-PEN is already ON (epen_ldo : 3.3 V)!!! \n", __func__);
	}
	else {
       	ret = regulator_enable(epen_ldo);
        	if(ret) {
        		printk("%s: error enabling epen_ldo regulator\n", __FUNCTION__);
		}
	}

/*
	ret = regulator_disable(epen_ldo);
        if(ret) {
		printk("%s: error disbling epen_ldo regulator\n", __FUNCTION__);
        }
*/	
    }
#endif 

}

static int wacom_init_hw(void)
{
#if !defined(CONFIG_KOR_MODEL_SHV_E160S) && !defined(CONFIG_KOR_MODEL_SHV_E160K) && !defined(CONFIG_KOR_MODEL_SHV_E160L)
	int ret;
#endif
	// int rc;
    //static struct regulator *epen_ldo = NULL;

	printk(KERN_INFO "[E-PEN] %s : Power On \n", __FUNCTION__);
	
#if !defined(CONFIG_KOR_MODEL_SHV_E160S) && !defined(CONFIG_KOR_MODEL_SHV_E160K) && !defined(CONFIG_KOR_MODEL_SHV_E160L) && !defined (CONFIG_JPN_MODEL_SC_05D)
	if(epen_ldo == NULL) {
	    epen_ldo = regulator_get(NULL, "8058_l8");
	    //epen_ldo = regulator_get(&rpm_vreg_device[RPM_VREG_ID_PM8058_L8].dev, "8058_l8");

        if (IS_ERR(epen_ldo)) {
            printk(KERN_ERR "%s: can't get wacom ldo\b", __FUNCTION__);
            return -1;            
        }
#if defined(CONFIG_USA_MODEL_SGH_I717)
        ret = regulator_set_voltage(epen_ldo, 3300000, 3300000);
#else
	ret = regulator_set_voltage(epen_ldo, 3000000, 3000000);
#endif
        if(ret) {
            printk("%s: error setting voltage - epen_ldo\n", __FUNCTION__);
        }

        ret = regulator_enable(epen_ldo);
        if(ret) {
            printk("%s: error enabling epen_ldo regulator\n", __FUNCTION__);
        }
    }
#else
	gpio_set_value(GPIO_PEN_LDO_EN, 1);
#endif


#if defined (CONFIG_USA_MODEL_SGH_I717)
    	wacom_gpio_init();

    	gpio_set_value(GPIO_PEN_SLP, 0);
    	gpio_set_value(GPIO_PEN_RESET, 0);

    	msleep(200);
    
    	gpio_set_value(GPIO_PEN_RESET, 1); 

    	printk("[E-PEN] (%s) GPIO_PEN_RESET= [%d] \n", __FUNCTION__, gpio_get_value(GPIO_PEN_RESET) );
    	printk("[E-PEN] (%s) GPIO_PEN_SLP  = [%d] \n", __FUNCTION__, gpio_get_value(GPIO_PEN_SLP) );
    	printk("[E-PEN] (%s) GPIO_PEN_PDCT = [%d] \n", __FUNCTION__, gpio_get_value(GPIO_PEN_PDCT) );
    	printk("[E-PEN] (%s) GPIO_PEN_IRQ  = [%d] \n", __FUNCTION__, gpio_get_value(GPIO_PEN_IRQ) );
#else	
	msleep(110);  // spec. min 100ms
	
	gpio_set_value(GPIO_PEN_RESET, 1); 
#endif
	printk("[E-PEN] (%s) Power Logig init end\n", __FUNCTION__);

	return 0;
}

static int wacom_exit_hw(void)
{
	return 0;
}

static int wacom_suspend_hw(void)
{
	return(wacom_early_suspend_hw());
}

static int wacom_resume_hw(void)
{
	return(wacom_late_resume_hw());
}

static int wacom_early_suspend_hw(void)
{
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	int pin, rc;
#endif
	printk(KERN_INFO "[E-PEN] %s  \n", __FUNCTION__);

	gpio_set_value(GPIO_PEN_RESET, 0);
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	gpio_set_value(GPIO_PEN_LDO_EN, 0);
#else
	wacom_vdd_enable(0);
#endif	
	
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	for (pin= 0; pin < ARRAY_SIZE(wacom_gpio_sleep); pin++) {
		rc = gpio_tlmm_config(wacom_gpio_sleep[pin], GPIO_CFG_ENABLE);
		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",	__func__, wacom_gpio_sleep[pin], rc);
		}
	}
#endif

	return 0;
}

static int wacom_late_resume_hw(void)
{
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	int pin, rc;
#endif

	printk(KERN_INFO "[E-PEN] %s  \n", __FUNCTION__);
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	gpio_set_value(GPIO_PEN_LDO_EN, 1);
	msleep(200); //angrybird : to be modified.
#else
	wacom_vdd_enable(1);
#endif
	gpio_set_value(GPIO_PEN_RESET, 1);  // angrybird

#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	for (pin= 0; pin < ARRAY_SIZE(wacom_gpio_wake); pin++) {
		rc = gpio_tlmm_config(wacom_gpio_wake[pin], GPIO_CFG_ENABLE);
		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",	__func__, wacom_gpio_wake[pin], rc);
		}
	}
#endif
	
	return 0;
}

static int wacom_reset_hw(void)
{
	printk(KERN_INFO "[E-PEN] %s  \n", __FUNCTION__);

#if (WACOM_HAVE_RESET_CONTROL)
	gpio_set_value(GPIO_PEN_RESET, 0);
	msleep(200);
	gpio_set_value(GPIO_PEN_RESET, 1);
#endif
	printk(KERN_INFO "[E-PEN] : wacom warm reset(%d).\n", WACOM_HAVE_RESET_CONTROL);
	return 0;
}

static void wacom_register_callbacks(struct wacom_g5_callbacks *cb)
{
	printk(KERN_INFO "[E-PEN] %s  \n", __FUNCTION__);

	wacom_callbacks = cb;
};

static int __init wacom_init(void)
{
	wacom_settle_down_befor_poweron();
	printk(KERN_INFO "[E-PEN] : initialized.\n");
	return 0;
}

#endif /* CONFIG_EPEN_WACOM_G5SP */



#if 0
static struct regulator *vreg_tmg200;

#define TS_PEN_IRQ_GPIO 61
static int tmg200_power(int vreg_on)
{
	int rc = -EINVAL;

	if (!vreg_tmg200) {
		printk(KERN_ERR "%s: regulator 8058_s3 not found (%d)\n",
			__func__, rc);
		return rc;
	}

	rc = vreg_on ? regulator_enable(vreg_tmg200) :
		  regulator_disable(vreg_tmg200);
	if (rc < 0)
		printk(KERN_ERR "%s: vreg 8058_s3 %s failed (%d)\n",
				__func__, vreg_on ? "enable" : "disable", rc);

	/* wait for vregs to stabilize */
	msleep(20);

	return rc;
}

static int tmg200_dev_setup(bool enable)
{
	int rc;

	if (enable) {
		vreg_tmg200 = regulator_get(NULL, "8058_s3");
		if (IS_ERR(vreg_tmg200)) {
			pr_err("%s: regulator get of 8058_s3 failed (%ld)\n",
				__func__, PTR_ERR(vreg_tmg200));
			rc = PTR_ERR(vreg_tmg200);
			return rc;
		}

		rc = regulator_set_voltage(vreg_tmg200, 1800000, 1800000);
		if (rc) {
			pr_err("%s: regulator_set_voltage() = %d\n",
				__func__, rc);
			goto reg_put;
		}
	} else {
		/* put voltage sources */
		regulator_put(vreg_tmg200);
	}
	return 0;
reg_put:
	regulator_put(vreg_tmg200);
	return rc;
}

static struct cy8c_ts_platform_data cy8ctmg200_pdata = {
	.ts_name = "msm_tmg200_ts",
	.dis_min_x = 0,
	.dis_max_x = 1023,
	.dis_min_y = 0,
	.dis_max_y = 599,
	.min_tid = 0,
	.max_tid = 255,
	.min_touch = 0,
	.max_touch = 255,
	.min_width = 0,
	.max_width = 255,
	.power_on = tmg200_power,
	.dev_setup = tmg200_dev_setup,
	.nfingers = 2,
	.irq_gpio = TS_PEN_IRQ_GPIO,
	.resout_gpio = GPIO_CAP_TS_RESOUT_N,
};

static struct i2c_board_info cy8ctmg200_board_info[] = {
	{
		I2C_BOARD_INFO("cy8ctmg200", 0x2),
		.platform_data = &cy8ctmg200_pdata,
	}
};

static struct regulator *vreg_tma340;

static int tma340_power(int vreg_on)
{
	int rc = -EINVAL;

	if (!vreg_tma340) {
		pr_err("%s: regulator 8901_l2 not found (%d)\n",
			__func__, rc);
		return rc;
	}

	rc = vreg_on ? regulator_enable(vreg_tma340) :
			regulator_disable(vreg_tma340);
	if (rc < 0)
		pr_err("%s: vreg 8901_l2 %s failed (%d)\n",
				__func__, vreg_on ? "enable" : "disable", rc);

	/* wait for vregs to stabilize */
	msleep(100);

	return rc;
}

static struct kobject *tma340_prop_kobj;

static int tma340_dragon_dev_setup(bool enable)
{
	int rc;

	if (enable) {
		vreg_tma340 = regulator_get(NULL, "8901_l2");
		if (IS_ERR(vreg_tma340)) {
			pr_err("%s: regulator get of 8901_l2 failed (%ld)\n",
				__func__, PTR_ERR(vreg_tma340));
			rc = PTR_ERR(vreg_tma340);
			return rc;
		}

		rc = regulator_set_voltage(vreg_tma340, 3300000, 3300000);
		if (rc) {
			pr_err("%s: regulator_set_voltage() = %d\n",
				__func__, rc);
			goto reg_put;
		}
		tma300_vkeys_attr.attr.name = "virtualkeys.cy8ctma340";
		tma340_prop_kobj = kobject_create_and_add("board_properties",
					NULL);
		if (tma340_prop_kobj) {
			rc = sysfs_create_group(tma340_prop_kobj,
				&tma300_properties_attr_group);
			if (rc) {
				kobject_put(tma340_prop_kobj);
				pr_err("%s: failed to create board_properties\n",
					__func__);
				goto reg_put;
			}
		}

	} else {
		/* put voltage sources */
		regulator_put(vreg_tma340);
		/* destroy virtual keys */
		if (tma340_prop_kobj) {
			sysfs_remove_group(tma340_prop_kobj,
				&tma300_properties_attr_group);
			kobject_put(tma340_prop_kobj);
		}
	}
	return 0;
reg_put:
	regulator_put(vreg_tma340);
	return rc;
}


static struct cy8c_ts_platform_data cy8ctma340_dragon_pdata = {
	.ts_name = "cy8ctma340",
	.dis_min_x = 0,
	.dis_max_x = 479,
	.dis_min_y = 0,
	.dis_max_y = 799,
	.min_tid = 0,
	.max_tid = 255,
	.min_touch = 0,
	.max_touch = 255,
	.min_width = 0,
	.max_width = 255,
	.power_on = tma340_power,
	.dev_setup = tma340_dragon_dev_setup,
	.nfingers = 2,
	.irq_gpio = TS_PEN_IRQ_GPIO,
	.resout_gpio = -1,
};

static struct i2c_board_info cy8ctma340_dragon_board_info[] = {
	{
		I2C_BOARD_INFO("cy8ctma340", 0x24),
		.platform_data = &cy8ctma340_dragon_pdata,
	}
};
#endif

#if defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_JPN_MODEL_SC_05D)
#define JACK_WATERPROOF
#endif

#if defined(CONFIG_SAMSUNG_JACK) || defined (CONFIG_SAMSUNG_EARJACK)
static struct sec_jack_zone jack_zones[] = {
        [0] = {
#if defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_I577)
                .adc_high       = 0,
#else
                .adc_high       = 3,
#endif
                .delay_ms       = 10,
                .check_count    = 10,
                .jack_type      = SEC_HEADSET_3POLE,
        },
        [1] = {
#if defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_I577)
                .adc_high       = 988,
#elif defined (CONFIG_KOR_MODEL_SHV_E110S) && defined (CONFIG_PMIC8058_XOADC_CAL)
                .adc_high       = 985,
#elif defined (CONFIG_USA_MODEL_SGH_I717)
                .adc_high       = 700,               
#else
                .adc_high       = 980,
#endif
                .delay_ms       = 10,
                .check_count    = 10,
                .jack_type      = SEC_HEADSET_3POLE,
        },
        [2] = {
#if defined(JACK_WATERPROOF)
#if defined (CONFIG_JPN_MODEL_SC_05D)
                .adc_high       = 2780,
#else
                .adc_high       = 1900,
#endif
                .delay_ms       = 10,
                .check_count    = 10,
                .jack_type      = SEC_HEADSET_4POLE,
        },
        [3] = {
#endif
                .adc_high       = 9999,
                .delay_ms       = 10,
                .check_count    = 10,
                .jack_type      = SEC_HEADSET_4POLE,
        }, 
};

#if defined (CONFIG_USA_MODEL_SGH_I577) || defined (CONFIG_USA_MODEL_SGH_T769)
/* To support 3-buttons earjack */
static struct sec_jack_buttons_zone jack_buttons_zones[] = {
	{
		.code		= KEY_MEDIA,
		.adc_low		= 0,
		.adc_high		= 148,
	},
	{
		.code		= KEY_VOLUMEUP,
		.adc_low		= 149,
		.adc_high		= 311,
	},
	{
		.code		= KEY_VOLUMEDOWN,
		.adc_low		= 312,
		.adc_high		= 680,
	},
};
#endif

int get_sec_det_jack_state(void)
{
#if defined (CONFIG_KOR_MODEL_SHV_E110S)
	if(get_hw_rev()==0x05) //only for celox_REV05
		return(gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_DET)));
	else		
#endif
		return(gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_DET))) ^ 1;

}
EXPORT_SYMBOL(get_sec_det_jack_state);

#if defined(CONFIG_KOR_MODEL_SHV_E160L)	// add EAR_VOL_KEY 
static int get_ear_key_gpio(void)
{
	if (system_rev >= 0x08) 
	{
		return (PMIC_GPIO_VOLUME_KEY);	
	}
	else
	{
		return (PMIC_GPIO_SHORT_SENDEND);	
	}
}
#endif

static int get_sec_send_key_state(void)
{
	// int state;

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

	if(get_sec_det_jack_state())
	{
//		pm8058_gpio_config(PMIC_GPIO_EAR_MICBIAS_EN, &ear_micbiase);
		pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_MICBIAS_EN), &ear_micbiase);

		gpio_set_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_MICBIAS_EN),1);	
	}

#if defined(CONFIG_KOR_MODEL_SHV_E160L)	// add EAR_VOL_KEY 
		return(gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(get_ear_key_gpio()))) ^ 1;
#else
#if defined(CONFIG_KOR_MODEL_SHV_E110S)
	if(get_hw_rev()==0x05) //only for celox_REV05
	{
		return(gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SHORT_SENDEND)));
	}else		
#endif
		return(gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SHORT_SENDEND))) ^ 1;
#endif

/*
#if defined (CONFIG_KOR_MODEL_SHV_E110S)
	if(get_hw_rev()==0x05) //only for celox_REV05
		state = (gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SHORT_SENDEND)));
	else
#endif
	state = (gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SHORT_SENDEND))) ^ 1;
	pr_info("sec_jack: send key state %d\n", state);
	return state;*/
}

static void set_sec_micbias_state(bool state)
{
#if defined(CONFIG_KOR_MODEL_SHV_E160L)
	struct regulator *L4;
	int rc;
	bool bOff = true;
	L4 = regulator_get(NULL, "8901_l4");

	if (IS_ERR(L4)) {
		rc = PTR_ERR(L4);
		pr_err("%s: L4 get failed (%d)\n", __func__, rc);
		bOff= false;
		goto out;
	}
	rc = regulator_set_voltage(L4, 2800000, 2800000);
	if (rc) {
		pr_err("%s: L4 set level failed (%d)\n", __func__, rc);
		goto out;
	}

	if(state)
	{
		rc = regulator_enable(L4);
		if (rc) {
			pr_err("%s: L4 enable failed (%d)\n",  __func__, rc);
		}
	}
	else
	{
		rc = regulator_disable(L4);
		if (rc) {
			pr_err("%s: L4 disable failed (%d)\n",  __func__, rc);
			rc = regulator_force_disable(L4); // force disable run after L4 disable fail
			pr_err("%s: L4 force disable state (%d)\n",  __func__, rc);
		}		
	}

out:
	if(bOff)
		regulator_put(L4);
#else
	/* split MICBIAS and EAR_MICBIAS from Rev05 */
    //if(get_hw_rev()==0x1)
	{
		pr_info("sec_jack: ear micbias %s\n", state?"on":"off");
		gpio_set_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_MICBIAS_EN), state);	
	}
#endif
}

static int sec_jack_read_adc(int channel, int *adc_data)
{
	int ret;
	void *h;
	struct adc_chan_result adc_chan_result;
	struct completion  conv_complete_evt;

	pr_info("%s: called for %d\n", __func__, channel);
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
//	wait_for_completion(&conv_complete_evt);
	ret = wait_for_completion_timeout(&conv_complete_evt, 10*HZ);
	if (!ret) {
		pr_err("%s: wait interrupted channel %d ret=%d\n",
						__func__, channel, ret);
		pm8058_xoadc_clear_recentQ(h);
		adc_channel_close(h);
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
	if(adc_data)
		*adc_data = adc_chan_result.measurement;

	pr_info("%s ADC : %lld\n", __func__, adc_chan_result.physical);
	return adc_chan_result.physical;
out:
	pr_err("%s: Failure for %d\n", __func__, channel);
	return 0;//to prevent infinite loop in determine_jack_type()		-EINVAL;
}

static int sec_jack_get_adc_value(void)
{
	int temp_adc = 0;
	int adc_data = 0;

	//return(gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SHORT_SENDEND))) ^ 1;
	temp_adc = sec_jack_read_adc(CHANNEL_ADC_HDSET, &adc_data);
	
	return temp_adc;
}

void sec_jack_gpio_init(void)
{
	/* TODO : EAR_MICBIAS_EN initialization */
	/* if (gpio_is_valid(GPIO_MICBIAS_EN1)) {
		 * if (gpio_request(GPIO_MICBIAS_EN1, "EAR_MICBIAS_EN"))
			 * pr_err("[JACK] Failed to GPIO_EAR_MICBIAS_EN!\n");
		 * gpio_direction_output(GPIO_MICBIAS_EN1, 0);
	 * }
	 * s3c_gpio_slp_cfgpin(GPIO_MICBIAS_EN1, S3C_GPIO_SLP_PREV); */

}

static struct sec_jack_platform_data sec_jack_data = {
	.get_det_jack_state	= get_sec_det_jack_state,
	.get_send_key_state	= get_sec_send_key_state,
	.set_micbias_state	= set_sec_micbias_state,
	.get_adc_value	= sec_jack_get_adc_value,
	.zones		= jack_zones,
	.num_zones	= ARRAY_SIZE(jack_zones),
#ifdef CONFIG_SAMSUNG_EARJACK
	.buttons_zones = jack_buttons_zones,
	.num_buttons_zones = ARRAY_SIZE(jack_buttons_zones),
#endif
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
#endif /* CONFIG_SAMSUNG_JACK */


#if defined(CONFIG_BATTERY_MAX17040) || defined(CONFIG_CHARGER_SMB328A)
#define GPIO_FG_SMB_I2C_SCL	101
#define GPIO_FG_SMB_I2C_SDA	107
#ifdef CONFIG_CHARGER_SMB328A
void smb328a_hw_init(void)
{
	gpio_tlmm_config(GPIO_CFG(GPIO_FG_SMB_I2C_SCL,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),1);
	gpio_tlmm_config(GPIO_CFG(GPIO_FG_SMB_I2C_SDA,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),1);
	gpio_set_value(GPIO_FG_SMB_I2C_SCL,1);
	gpio_set_value(GPIO_FG_SMB_I2C_SDA,1);
}

static int smb328a_intr_trigger(int status)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;

	if (!psy) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return -ENODEV;
	}

	if (status)
		value.intval = POWER_SUPPLY_STATUS_CHARGING;
	else
		value.intval = POWER_SUPPLY_STATUS_DISCHARGING;
	return psy->set_property(psy, POWER_SUPPLY_PROP_STATUS, &value);
}

static struct smb328a_platform_data smb328a_pdata = {
	.hw_init = smb328a_hw_init,
	.chg_intr_trigger = smb328a_intr_trigger,
};
#endif
#ifdef CONFIG_CHARGER_SMB136
void smb136_hw_init(void)
{
	gpio_tlmm_config(GPIO_CFG(GPIO_FG_SMB_I2C_SCL,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),1);
	gpio_tlmm_config(GPIO_CFG(GPIO_FG_SMB_I2C_SDA,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),1);
	gpio_set_value(GPIO_FG_SMB_I2C_SCL,1);
	gpio_set_value(GPIO_FG_SMB_I2C_SDA,1);
}

static int smb136_intr_trigger(int status)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;

	if (!psy) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return -ENODEV;
	}

	if (status)
		value.intval = POWER_SUPPLY_STATUS_CHARGING;
	else
		value.intval = POWER_SUPPLY_STATUS_DISCHARGING;
	return psy->set_property(psy, POWER_SUPPLY_PROP_STATUS, &value);
}

static struct smb136_platform_data smb136_pdata = {
	.hw_init = smb136_hw_init,
	.chg_intr_trigger = smb136_intr_trigger,
};
#endif

#ifdef CONFIG_BATTERY_MAX17040
void max17040_hw_init(void)
{
	gpio_tlmm_config(GPIO_CFG(GPIO_FG_SMB_I2C_SCL,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),1);
	gpio_tlmm_config(GPIO_CFG(GPIO_FG_SMB_I2C_SDA,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),1);
	//gpio_tlmm_config(GPIO_CFG(61,  0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),1);
	gpio_set_value(GPIO_FG_SMB_I2C_SCL,1);
	gpio_set_value(GPIO_FG_SMB_I2C_SDA,1);
}

static int max17040_low_batt_cb(void)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;

	if (!psy) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return -ENODEV;
	}

	value.intval = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	return psy->set_property(psy, POWER_SUPPLY_PROP_CAPACITY_LEVEL, &value);
}

/* RCOMP is related to fuelgauge accuracy */
/* you should fix these values for your MODEL */
static struct max17040_platform_data max17040_pdata = {
	.hw_init = max17040_hw_init,
	.low_batt_cb = max17040_low_batt_cb,
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_JPN_MODEL_SC_05D)
	.rcomp_value = 0xe71f,
#elif defined(CONFIG_KOR_MODEL_SHV_E160K)
	.rcomp_value = 0xff1f,
#elif defined(CONFIG_KOR_MODEL_SHV_E160L)
	.rcomp_value = 0xff1f,
#elif defined(CONFIG_EUR_MODEL_GT_I9210)
	.rcomp_value = 0xf71f,
#elif defined(CONFIG_USA_MODEL_SGH_I727)
	.rcomp_value = 0xc71f,
#elif defined(CONFIG_USA_MODEL_SGH_I717)
	.rcomp_value = 0xd01f,
#elif defined(CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577)
	.rcomp_value = 0xc01f,
#else
	.rcomp_value = 0xe71f,
#endif
};
#endif

static struct i2c_gpio_platform_data fg_smb_i2c_gpio_data = {
	.sda_pin		= GPIO_FG_SMB_I2C_SDA,
	.scl_pin		= GPIO_FG_SMB_I2C_SCL,
	.udelay 		= 2,
	.sda_is_open_drain	= 0,
	.scl_is_open_drain	= 0,
	.scl_is_output_only	= 0,
};

static struct platform_device fg_smb_i2c_gpio_device = {
	.name			= "i2c-gpio",
	.id			= MSM_FG_SMB_I2C_BUS_ID,
	.dev.platform_data	= &fg_smb_i2c_gpio_data,
};

static struct i2c_board_info fg_smb_i2c_devices[] = {
#ifdef CONFIG_CHARGER_SMB328A
	{
		I2C_BOARD_INFO("smb328a", (0x69 >> 1)),
		.platform_data = &smb328a_pdata,
		.irq	= PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_CHG_STAT),
	},
#endif
#ifdef CONFIG_CHARGER_SMB136
	{
		I2C_BOARD_INFO("smb136", (0x9A >> 1)),
		.platform_data = &smb136_pdata,
		.irq	= PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_CHG_STAT),
	},
#endif

#ifdef CONFIG_BATTERY_MAX17040
	{
		I2C_BOARD_INFO("max17040", (0x6D >> 1)),
		.platform_data = &max17040_pdata,
		.irq		= MSM_GPIO_TO_INT(61),
	},
#endif
};
#endif /* defined(CONFIG_BATTERY_MAX17040) || defined(CONFIG_CHARGER_SMB328A) */

#if defined(CONFIG_VIDEO_MHL_V1)
int get_mhl_int_irq(void)
{
	unsigned int hw_rev = get_hw_rev();
	pr_info("Board : %s, %d\n", __func__, hw_rev);
#if defined(CONFIG_USA_MODEL_SGH_I727)
	if (hw_rev < 0x0b)
		return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_9);
	else
		return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_31);
#elif defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
	if (hw_rev < 0x0e)
		return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_9);
	else
		return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_31);
#elif defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_USA_MODEL_SGH_I577)
	return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_31);
#elif defined (CONFIG_USA_MODEL_SGH_I717)
	if(hw_rev == 1)
		return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_9);
	else
		return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_31);
#else
	return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_9);
#endif
}
struct sii9234_platform_data sii9234_pdata = {
	.get_irq = get_mhl_int_irq,
#ifdef CONFIG_USA_MODEL_SGH_I757
		.swing_level = 0xFD,
#else
		.swing_level = 0xEB,
#endif
};
static struct i2c_board_info mhl_i2c_borad_info[] = {
	{
		I2C_BOARD_INFO("SII9234", 0x72>>1),
	},
	{
		I2C_BOARD_INFO("SII9234A", 0x7A>>1),
	},
	{
		I2C_BOARD_INFO("SII9234B", 0x92>>1),
	},
	{
		I2C_BOARD_INFO("SII9234C", 0xC8>>1),
			.platform_data = &sii9234_pdata,
	},
};
#elif defined(CONFIG_VIDEO_MHL_V2)
static void msm8960_mhl_gpio_init(void)
{
	return;//gpio init done
}

static void cfg_mhl_sel(bool onoff)
{
	gpio_set_value_cansleep(GPIO_MHL_SEL, onoff);
}


static void mhl_gpio_config(void)
{
	return;/*gpio config done*/
}

/*
 * gpio_interrupt pin is very changable each different h/w_rev or  board.
 */
int get_mhl_int_irq(void)
{
	unsigned int hw_rev = get_hw_rev();

	struct pm_gpio mhl_int = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel		= 2,
		.function		= PM_GPIO_FUNC_NORMAL,
		.inv_int_pol	= 0,
	};

		pr_info("mhl_v2, Board : %s, rev:%d\n", __func__, hw_rev);

#if defined(CONFIG_KOR_MODEL_SHV_E110S)
	if (hw_rev < 7){
		pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_9), &mhl_int);
		return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_9);
		} else {
		pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_31), &mhl_int);
		return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_31);
		}
#elif defined(CONFIG_JPN_MODEL_SC_03D)
	if (hw_rev < 5)
		return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_9);
	else
		return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_31);
#elif defined (CONFIG_EUR_MODEL_GT_I9210)
	if (hw_rev < 0x08) {
		pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_9), &mhl_int);
			return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_9);
	} else {
		pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_31), &mhl_int);
			return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_31);
	}
#elif defined(CONFIG_USA_MODEL_SGH_I727)
	if (hw_rev < 0x0b) {
		pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_9), &mhl_int);
			return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_9);
	} else {
		pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_31), &mhl_int);
			return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_31);
	}
#elif defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
		if (hw_rev < 0x0e) {
		pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_9), &mhl_int);
			return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_9);
	} else {
		pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_31), &mhl_int);
			return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_31);
	}
#elif defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_USA_MODEL_SGH_I577)
	pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_31), &mhl_int);
	return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_31);
#elif defined(CONFIG_KOR_MODEL_SHV_E160L)
	pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_31), &mhl_int);
	return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_31);
#elif defined (CONFIG_USA_MODEL_SGH_I717)
		if(hw_rev == 1) {
		pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_9), &mhl_int);
			return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_9);
		} else {
		pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_31), &mhl_int);
			return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_31);
			}
#else
	pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_9), &mhl_int);
		return PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MHL_INT_9);
#endif
}

#define TEST_L18		//jgk.20120620
static struct regulator *l25;		/*VSIL_1.2A & VSIL_1.2C Connected to PM8058*/
static struct regulator *l2;		/*VCC_3.3V_MHL Connected to PM8901*/
static struct regulator *mvs0;	/*VCC_1.8V_MHL Connected to PM8901*/
#ifdef TEST_L18
static struct regulator *l18_xoadc; 	// L18 is power source of xoadc in PM8058
#endif

static void sii9234_hw_onoff(bool onoff)
{
	int rc;

	l25 = regulator_get(NULL, "8058_l25");

	if (IS_ERR(l25)) {
		rc = PTR_ERR(l25);
		pr_err("%s: l25 get failed (%d)\n", __func__, rc);
		return;
	}
	rc = regulator_set_voltage(l25, 1200000, 1200000);
	if (rc) {
		pr_err("%s: l25 set level failed (%d)\n", __func__, rc);
		return;
	}

	l2 = regulator_get(NULL, "8901_l2");

	if (IS_ERR(l2)) {
		rc = PTR_ERR(l2);
		pr_err("%s: l2 get failed (%d)\n", __func__, rc);
		return;
	}
	rc = regulator_set_voltage(l2, 3300000, 3300000);
	if (rc) {
		pr_err("%s: l2 set level failed (%d)\n", __func__, rc);
		return;
	}

	mvs0 = regulator_get(NULL, "8901_mvs0");

	if (IS_ERR(mvs0)) {
		rc = PTR_ERR(mvs0);
		pr_err("%s: mvs0 get failed (%d)\n", __func__, rc);
		return;
	}
#ifdef TEST_L18
	l18_xoadc = regulator_get(NULL, "8058_l18");
	if (IS_ERR(l18_xoadc)) {
		rc = PTR_ERR(l18_xoadc);
		pr_err("%s: l18_xoadc get failed (%d)\n", __func__, rc);
		return;
	}
	rc = regulator_set_voltage(l18_xoadc, 2200000, 2200000);
	if (rc) {
		pr_err("%s: unable to set l18_xoadc voltage to 2.2V\n", __func__);
		return;
	}
	pr_info("%s: [jgk] success - set l18_xoadc voltage to 2.2V\n", __func__);
#endif

	if (onoff)
	{
		rc = regulator_enable(l25);		/*VSIL_1.2A & VSIL_1.2C*/
		if (rc) {
			pr_err("%s: l25 vreg enable failed (%d)\n", __func__, rc);
			return;
		}

		rc = regulator_enable(l2);		/*VCC_3.3V_MHL*/
		if (rc) {
			pr_err("%s: l2 vreg enable failed (%d)\n", __func__, rc);
			return;
		}

		rc = regulator_enable(mvs0);		/*VCC_1.8V_MHL*/
		if (rc) {
			pr_err("%s: l2 vreg enable failed (%d)\n", __func__, rc);
			return;
		}
#ifdef TEST_L18
		rc = regulator_enable(l18_xoadc);
		if (rc) {
			pr_err("%s: l18_xoadc vreg enable failed (%d)\n", __func__, rc);
			return;
		}
		pr_info("%s: [jgk] success - l18_xoadc vreg enable\n", __func__);
#endif

		/*gpio_set_value_cansleep(PMIC_GPIO_MHL_RST, 0);
		msleep(10);
		gpio_set_value_cansleep(PMIC_GPIO_MHL_RST, 1);*/

		printk("sii9234_cfg_power on\n");
	} else {
		rc = regulator_disable(l25);		/*VSIL_1.2A & VSIL_1.2C*/
		if (rc) {
			pr_err("%s: l25 vreg enable failed (%d)\n", __func__, rc);
			return;
		}

		rc = regulator_disable(l2);		/*VCC_3.3V_MHL*/
		if (rc) {
			pr_err("%s: l2 vreg enable failed (%d)\n", __func__, rc);
			return;
		}

		rc = regulator_disable(mvs0);		/*VCC_1.8V_MHL*/
		if (rc) {
			pr_err("%s: l2 vreg enable failed (%d)\n", __func__, rc);
			return;
		}

#ifdef TEST_L18
		rc = regulator_disable(l18_xoadc);
		if (rc) {
			pr_err("%s: l18_xoadc vreg disable failed (%d)\n", __func__, rc);
			return;
		}
		pr_info("%s: [jgk] success - l18_xoadc vreg disable\n", __func__);
#endif
		gpio_set_value_cansleep(GPIO_MHL_RST, 0);
		printk("sii9234_cfg_power off\n");
	}

	return;
}

static void sii9234_hw_reset(void)
{
	gpio_set_value_cansleep(GPIO_MHL_RST, 0);
	msleep(10);
	gpio_set_value_cansleep(GPIO_MHL_RST, 1);

	return;
}

struct sii9234_platform_data sii9234_pdata = {
	.get_irq = get_mhl_int_irq,
	.hw_onoff = sii9234_hw_onoff,
	.hw_reset = sii9234_hw_reset,
	.gpio =  GPIO_MHL_SEL,
	.gpio_cfg = mhl_gpio_config,
#if defined(CONFIG_VIDEO_MHL_V2)
	.mhl_sel = cfg_mhl_sel,
	.vbus_present = fsa9480_mhl_cb,
#endif
#ifdef CONFIG_USA_MODEL_SGH_I757
	.swing_level = 0xFD,
#else
	.swing_level = 0xEB,
#endif
};

static struct i2c_board_info mhl_i2c_borad_info[] = {
	{
		I2C_BOARD_INFO("sii9234_mhl_tx", 0x72>>1),
		.platform_data = &sii9234_pdata,
	},
	{
		I2C_BOARD_INFO("sii9234_tpi", 0x7A>>1),
		.platform_data = &sii9234_pdata,
	},
	{
		I2C_BOARD_INFO("sii9234_hdmi_rx", 0x92>>1),
		.platform_data = &sii9234_pdata,
	},
	{
		I2C_BOARD_INFO("sii9234_cbus", 0xC8>>1),
		.platform_data = &sii9234_pdata,
	},
};
#endif /* CONFIG_VIDEO_MHL_V1, CONFIG_VIDEO_MHL_V2 */


#ifdef CONFIG_GYRO_K3G
#define SENSOR_GYRO_SCL 39
#define SENSOR_GYRO_SDA 38
#define SENSOR_GYRO_INT	PM8058_GPIO_IRQ(PM8058_IRQ_BASE, (PM8058_GPIO(11)))		/* PMIC GPIO Number 32 */

static struct i2c_gpio_platform_data gyro_i2c_gpio_data = {
	.sda_pin    = SENSOR_GYRO_SDA,
	.scl_pin    = SENSOR_GYRO_SCL,
	.udelay		= 1,
};

static struct platform_device gyro_i2c_gpio_device = {  
	.name       = "i2c-gpio",
	.id     = MSM_GYRO_I2C_BUS_ID,
	.dev        = {
		.platform_data  = &gyro_i2c_gpio_data,
	},
};

static struct k3g_platform_data k3g_pdata = {
	.power_on = sensor_power_on,
	.power_off = sensor_power_off,
};

static struct i2c_board_info gyro_i2c_borad_info[] = {
	{
		I2C_BOARD_INFO("k3g", 0x69),
		.platform_data	= &k3g_pdata,
      	.irq = /*SENSOR_GYRO_INT*/ -1,
	},
};
#endif /* CONFIG_GYRO_K3G */


#ifdef CONFIG_SENSORS_AK8975
#define SENSOR_ACCEL_INT	PM8058_GPIO_IRQ(PM8058_IRQ_BASE, (PM8058_GPIO(10)))		/* PMIC GPIO Number 32 */
#define SENSOR_AKM_SDA 51
#define SENSOR_AKM_SCL 52

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
	.gpio_data_ready_gpio = PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MSENSE_RST),
    .gpio_data_ready_int = PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PMIC_GPIO_MSENSE_RST),
	.power_on = sensor_power_on,
	.power_off = sensor_power_off,
	.power_on_mag = sensor_power_on_mag,
	.power_off_mag = sensor_power_off_mag,
};

static struct k3dh_platform_data k3dh_data = {
	.gpio_acc_int = SENSOR_ACCEL_INT,
	.power_on = sensor_power_on,
	.power_off = sensor_power_off,
};

static struct i2c_board_info akm_i2c_borad_info[] = {
	{
		I2C_BOARD_INFO("ak8975", 0x0C),
		.platform_data = &akm8975_pdata,
	},
	{
		I2C_BOARD_INFO("k3dh", 0x19),
      	.platform_data  = &k3dh_data,
	},
#if defined (CONFIG_INPUT_BMP180)
	{
		I2C_BOARD_INFO("bmp180", 0x77),
	},
#elif defined (CONFIG_INPUT_LPS331AP)
	{
		I2C_BOARD_INFO("lps331ap", 0x5D),
		.platform_data = &lps331ap_pdata,
	},
#endif
};
#endif /* CONFIG_SENSORS_AK8975 */


#ifdef CONFIG_SENSORS_YDA165
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

static struct snd_set_ampgain init_ampgain[] = {
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_JPN_MODEL_SC_05D)
	// SPK
	[0] = {
	.in1_gain = 2,
	.in2_gain = 2,
	.hp_att = 0,
	.hp_gainup = 0,
	.sp_att = 28,
	.sp_gainup = 0,
	},
	// HEADSET
	[1] = {
	.in1_gain = 2,
	.in2_gain = 2,
	.hp_att = 29,
	.hp_gainup = 0,
	.sp_att = 0,
	.sp_gainup = 0,
	},
	// SPK + HEADSET
	[2] = {
	.in1_gain = 2,
	.in2_gain = 2,
	.hp_att = 2,
	.hp_gainup = 1,
	.sp_att = 29,
	.sp_gainup = 0,
	},	
#elif defined(CONFIG_KOR_MODEL_SHV_E160K)
	// SPK
	[0] = {
	.in1_gain = 2,
	.in2_gain = 2,
	.hp_att = 0,
	.hp_gainup = 0,
	.sp_att = 28,
	.sp_gainup = 0,
	},
	// HEADSET
	[1] = {
	.in1_gain = 2,
	.in2_gain = 2,
	.hp_att = 29,
	.hp_gainup = 0,
	.sp_att = 0,
	.sp_gainup = 0,
	},
	// SPK + HEADSET
	[2] = {
	.in1_gain = 2,
	.in2_gain = 2,
	.hp_att = 2,
	.hp_gainup = 1,
	.sp_att = 29,
	.sp_gainup = 0,
	},	
#elif defined(CONFIG_KOR_MODEL_SHV_E160L)
	// SPK
	[0] = {
	.in1_gain = 2,
	.in2_gain = 2,
	.hp_att = 0,
	.hp_gainup = 0,
	.sp_att = 28,
	.sp_gainup = 0,
	},
	// HEADSET
	[1] = {
	.in1_gain = 2,
	.in2_gain = 2,
	.hp_att = 29,
	.hp_gainup = 0,
	.sp_att = 0,
	.sp_gainup = 0,
	},
	// SPK + HEADSET
	[2] = {
	.in1_gain = 2,
	.in2_gain = 2,
	.hp_att = 1, // 2 -> 1
	.hp_gainup = 2, // 1 -> 2
	.sp_att = 29,
	.sp_gainup = 0,
	},	
#elif defined (CONFIG_USA_MODEL_SGH_I717)
    // SPK
	[0] = {
		.in1_gain = 2,
		.in2_gain = 2,
		.hp_att = 0,
		.hp_gainup = 0,
		.sp_att = 28,
		.sp_gainup = 0,
		.bSpNg_DetectionLv = 0,
		.bSpNg_AttackTime = 1,
		.bSpNcpl_NonClipRatio = 0,
		.bSpNcpl_PowerLimit = 0,
		.bSpNcpl_AttackTime = 1,
		.bSpNcpl_ReleaseTime = 1,
	},
    // HEADSET
	[1] = {
		.in1_gain = 2,
		.in2_gain = 2,
		.hp_att = 29,
		.hp_gainup = 0,
		.sp_att = 0,
		.sp_gainup = 0,
		.bSpNg_DetectionLv = 0,
		.bSpNg_AttackTime = 1,
		.bSpNcpl_NonClipRatio = 0,
		.bSpNcpl_PowerLimit = 0,
		.bSpNcpl_AttackTime = 1,
		.bSpNcpl_ReleaseTime = 1,
	},
    // SPK + HEADSET
	[2] = {
		.in1_gain = 2,
		.in2_gain = 2,
		.hp_att = 2,
		.hp_gainup = 2,
		.sp_att = 29,
		.sp_gainup = 0,
		.bSpNg_DetectionLv = 0,
		.bSpNg_AttackTime = 1,
		.bSpNcpl_NonClipRatio = 0,
		.bSpNcpl_PowerLimit = 0,
		.bSpNcpl_AttackTime = 1,
		.bSpNcpl_ReleaseTime = 1,
	},
	// SPK_CALL
	[3] = {
		.in1_gain = 2,
		.in2_gain = 2,
		.hp_att = 0,
		.hp_gainup = 0,
		.sp_att = 31,
		.sp_gainup = 2,
		.bSpNg_DetectionLv = 0,
		.bSpNg_AttackTime = 1,
		.bSpNcpl_NonClipRatio = 0,
		.bSpNcpl_PowerLimit = 0,
		.bSpNcpl_AttackTime = 1,
		.bSpNcpl_ReleaseTime = 1,
	},
	// HEADSET_CALL
	[4] = {
		.in1_gain = 2,
		.in2_gain = 2,
		.hp_att = 31,
		.hp_gainup = 2,
		.sp_att = 0,
		.sp_gainup = 0,
		.bSpNg_DetectionLv = 0,
		.bSpNg_AttackTime = 1,
		.bSpNcpl_NonClipRatio = 1,
		.bSpNcpl_PowerLimit = 0,
		.bSpNcpl_AttackTime = 1,
		.bSpNcpl_ReleaseTime = 1,
	},
#else
	    // SPK
        [0] = {
                .in1_gain = 2,
                .in2_gain = 2,
                .hp_att = 0,
                .hp_gainup = 0,
                .sp_att = 31,
                .sp_gainup = 2,
        },
    // HEADSET
        [1] = {
                .in1_gain = 2,
                .in2_gain = 2,
                .hp_att = 31,
                .hp_gainup = 0,
                .sp_att = 0,
                .sp_gainup = 0,
        },
    // SPK + HEADSET
        [2] = {
                .in1_gain = 2,
                .in2_gain = 2,
                .hp_att = 2,
                .hp_gainup = 1,
                .sp_att = 31,
                .sp_gainup = 2,
        },
#endif
};

void yda165_avdd_power_on(void);
void yda165_avdd_power_off(void);

static struct yda165_i2c_data yda165_data = {
	.ampgain = init_ampgain,
	.num_modes = ARRAY_SIZE(init_ampgain),
	.power_on = yda165_avdd_power_on,
	.power_off = yda165_avdd_power_off,
};

static struct i2c_board_info yamahaamp_boardinfo[] = {
	{
		I2C_BOARD_INFO("yda165", 0xD8 >> 1),
		.platform_data = &yda165_data,
	},
};
#endif /* CONFIG_SENSORS_YDA165 */


#if defined(CONFIG_KOR_MODEL_SHV_E160L)
#define VIB_MOTOR_SCL   		141
#define VIB_MOTOR_SDA   		142
#else
#define VIB_MOTOR_SCL   		94
#define VIB_MOTOR_SDA   		70
#endif
static struct i2c_gpio_platform_data motor_i2c_gpio_data = {
	.sda_pin	= VIB_MOTOR_SDA,
	.scl_pin	= VIB_MOTOR_SCL,
	.udelay 	= 5,
};

static struct platform_device motor_i2c_gpio_device = {  
	.name		= "i2c-gpio",
	.id 	= MSM_MOTOR_I2C_BUS_ID,
	.dev		= {
		.platform_data	= &motor_i2c_gpio_data,
	},
};
static struct i2c_board_info motor_i2c_borad_info[] = {
	{
		I2C_BOARD_INFO("isa1200", 0x48),
	},
};

#ifdef CONFIG_SERIAL_MSM_HS
#if 0 //SAMSUNG_BT_CONFIG
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
#endif
static struct msm_serial_hs_platform_data msm_uart_dm1_pdata = {
       .inject_rx_on_wakeup = 1,
       .rx_to_inject = 0xFD,
#if 0 //SAMSUNG_BT_CONFIG
       .gpio_config = configure_uart_gpios,
#endif
};
#endif


#if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)

static struct gpio_led gpio_exp_leds_config[] = {
	{
		.name = "left_led1:green",
		.gpio = GPIO_LEFT_LED_1,
		.active_low = 1,
		.retain_state_suspended = 0,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
	{
		.name = "left_led2:red",
		.gpio = GPIO_LEFT_LED_2,
		.active_low = 1,
		.retain_state_suspended = 0,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
	{
		.name = "left_led3:green",
		.gpio = GPIO_LEFT_LED_3,
		.active_low = 1,
		.retain_state_suspended = 0,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
	{
		.name = "wlan_led:orange",
		.gpio = GPIO_LEFT_LED_WLAN,
		.active_low = 1,
		.retain_state_suspended = 0,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
	{
		.name = "left_led5:green",
		.gpio = GPIO_LEFT_LED_5,
		.active_low = 1,
		.retain_state_suspended = 0,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
	{
		.name = "right_led1:green",
		.gpio = GPIO_RIGHT_LED_1,
		.active_low = 1,
		.retain_state_suspended = 0,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
	{
		.name = "right_led2:red",
		.gpio = GPIO_RIGHT_LED_2,
		.active_low = 1,
		.retain_state_suspended = 0,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
	{
		.name = "right_led3:green",
		.gpio = GPIO_RIGHT_LED_3,
		.active_low = 1,
		.retain_state_suspended = 0,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
	{
		.name = "bt_led:blue",
		.gpio = GPIO_RIGHT_LED_BT,
		.active_low = 1,
		.retain_state_suspended = 0,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
	{
		.name = "right_led5:green",
		.gpio = GPIO_RIGHT_LED_5,
		.active_low = 1,
		.retain_state_suspended = 0,
		.default_state = LEDS_GPIO_DEFSTATE_OFF,
	},
};

static struct gpio_led_platform_data gpio_leds_pdata = {
	.num_leds = ARRAY_SIZE(gpio_exp_leds_config),
	.leds = gpio_exp_leds_config,
};

static struct platform_device gpio_leds = {
	.name          = "leds-gpio",
	.id            = -1,
	.dev           = {
		.platform_data = &gpio_leds_pdata,
	},
};

static struct gpio_led fluid_gpio_leds[] = {
	{
		.name			= "dual_led:green",
		.gpio			= GPIO_LED1_GREEN_N,
		.default_state		= LEDS_GPIO_DEFSTATE_OFF,
		.active_low		= 1,
		.retain_state_suspended = 0,
	},
	{
		.name			= "dual_led:red",
		.gpio			= GPIO_LED2_RED_N,
		.default_state		= LEDS_GPIO_DEFSTATE_OFF,
		.active_low		= 1,
		.retain_state_suspended = 0,
	},
};

static struct gpio_led_platform_data gpio_led_pdata = {
	.leds		= fluid_gpio_leds,
	.num_leds	= ARRAY_SIZE(fluid_gpio_leds),
};

static struct platform_device fluid_leds_gpio = {
	.name	= "leds-gpio",
	.id	= -1,
	.dev	= {
		.platform_data	= &gpio_led_pdata,
	},
};

#endif /* CONFIG_GPIO_SX150X || CONFIG_GPIO_SX150X_MODULE */


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

#ifdef CONFIG_BATTERY_MSM8X60
static struct msm_charger_platform_data msm_charger_data = {
	.safety_time = 180,
	.update_time = 1,
	.max_voltage = 4200,
	.min_voltage = 3200,
};

static struct platform_device msm_charger_device = {
	.name = "msm-charger",
	.id = -1,
	.dev = {
		.platform_data = &msm_charger_data,
	}
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
	REGULATOR_SUPPLY("cam_vana",		"1-001a"),
	REGULATOR_SUPPLY("cam_vana",		"1-006c"),
	REGULATOR_SUPPLY("cam_vana",		"1-0078"),
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
	REGULATOR_SUPPLY("cam_vdig",		"1-001a"),
	REGULATOR_SUPPLY("cam_vdig",		"1-006c"),
	REGULATOR_SUPPLY("cam_vdig",		"1-0078"),
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
	REGULATOR_SUPPLY("cam_vio",			"1-001a"),
	REGULATOR_SUPPLY("cam_vio",			"1-006c"),
	REGULATOR_SUPPLY("cam_vio",			"1-0078"),
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
				.valid_modes_mask	= _modes, \
				.valid_ops_mask		= _ops, \
				.min_uV			= _min_uV, \
				.max_uV			= _max_uV, \
				.input_uV		= _min_uV, \
				.apply_uV		= _apply_uV, \
				.always_on		= _always_on, \
			}, \
			.consumer_supplies	= vreg_consumers_##_id, \
			.num_consumer_supplies	= \
				ARRAY_SIZE(vreg_consumers_##_id), \
		}, \
		.id			= RPM_VREG_ID_##_id, \
		.default_uV		= _default_uV, \
		.peak_uA		= _peak_uA, \
		.avg_uA			= _avg_uA, \
		.pull_down_enable	= _pull_down, \
		.pin_ctrl		= _pin_ctrl, \
		.freq			= RPM_VREG_FREQ_##_freq, \
		.pin_fn			= _pin_fn, \
		.force_mode		= _force_mode, \
		.state			= _state, \
		.sleep_selectable	= _sleep_selectable, \
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
#if defined(CONFIG_JPN_MODEL_SC_05D)
	RPM_SMPS(PM8058_S0, 0, 1, 1,  500000, 1325000, SMPS_HMIN, 1p60),
#else
	RPM_SMPS(PM8058_S0, 0, 1, 1,  500000, 1250000, SMPS_HMIN, 1p60),
#endif
	RPM_SMPS(PM8058_S1, 0, 1, 1,  500000, 1250000, SMPS_HMIN, 1p60),
};

/* RPM regulator constraints */
static struct rpm_regulator_init_data rpm_regulator_init_data[] = {
	/*	ID        a_on pd ss min_uV   max_uV   init_ip */
	RPM_LDO(PM8058_L0,  0, 1, 0, 1200000, 1200000, LDO150HMIN),
	RPM_LDO(PM8058_L1,  0, 1, 0, 1200000, 1200000, LDO300HMIN),
#if defined (CONFIG_TARGET_LOCALE_USA)
	RPM_LDO(PM8058_L2,  0, 1, 0, 2600000, 2600000, LDO300HMIN),
#else
	RPM_LDO(PM8058_L2,  0, 1, 0, 1800000, 2600000, LDO300HMIN),
#endif
#if defined(CONFIG_KOR_MODEL_SHV_E160L)	
	RPM_LDO(PM8058_L3,  0, 1, 0, 1800000, 2800000, LDO150HMIN),
#else
	RPM_LDO(PM8058_L3,  0, 1, 0, 1800000, 2200000, LDO150HMIN),
#endif	
	RPM_LDO(PM8058_L4,  0, 1, 0, 2850000, 2850000,  LDO50HMIN),
	RPM_LDO(PM8058_L5,  0, 1, 0, 2850000, 2850000, LDO300HMIN),
	RPM_LDO(PM8058_L6,  0, 1, 0, 3000000, 3600000,  LDO50HMIN),
	RPM_LDO(PM8058_L7,  0, 1, 0, 1800000, 1800000,  LDO50HMIN),
//	RPM_LDO(PM8058_L8,  0, 1, 0, 2850000, 3050000, LDO300HMIN),
#if defined(CONFIG_USA_MODEL_SGH_I717) && defined (CONFIG_EPEN_WACOM_G5SP)
	RPM_LDO(PM8058_L8,  0, 1, 0, 1800000, 3300000, LDO300HMIN),
#else	
	RPM_LDO(PM8058_L8,  0, 1, 0, 1800000, 3050000, LDO300HMIN),
#endif
	RPM_LDO(PM8058_L9,  0, 1, 0, 3000000, 3000000, LDO300HMIN),
//	RPM_LDO(PM8058_L10, 0, 1, 0, 2600000, 2600000, LDO300HMIN),
	RPM_LDO(PM8058_L10, 0, 1, 0, 1500000, 2600000, LDO300HMIN),
//	RPM_LDO(PM8058_L11, 0, 1, 0, 1500000, 1500000, LDO150HMIN),
	RPM_LDO(PM8058_L11, 0, 1, 0, 2850000, 2850000, LDO150HMIN),
	RPM_LDO(PM8058_L12, 0, 1, 0, 2000000, 3300000, LDO150HMIN),	
	RPM_LDO(PM8058_L13, 0, 1, 0, 2050000, 2050000, LDO300HMIN),
	RPM_LDO(PM8058_L14, 0, 1, 0, 2850000, 2850000, LDO300HMIN),
	RPM_LDO(PM8058_L15, 0, 1, 0, 2850000, 2850000, LDO300HMIN),
	RPM_LDO(PM8058_L16, 1, 1, 0, 1800000, 1800000, LDO300HMIN),
#if 1 // SAMSUNG LDO SETTING
	RPM_LDO(PM8058_L17, 0, 1, 0, 1800000, 2600000, LDO150HMIN),
#else
	RPM_LDO(PM8058_L17, 1, 1, 0, 1800000, 2600000, LDO150HMIN),
#endif
	RPM_LDO(PM8058_L18, 0, 1, 0, 2200000, 2200000, LDO150HMIN),
#if defined(CONFIG_FB_MSM_MIPI_S6E8AA0_HD720_PANEL) || defined(CONFIG_FB_MSM_MIPI_S6E8AA0_WXGA_Q1_PANEL)
	RPM_LDO(PM8058_L19, 0, 1, 0, 3000000, 3300000, LDO150HMIN),
#else
	RPM_LDO(PM8058_L19, 0, 1, 0, 2500000, 2500000, LDO150HMIN),
#endif
#if defined(CONFIG_USA_MODEL_SGH_I717)
	RPM_LDO(PM8058_L20, 0, 1, 0, 1800000, 3050000, LDO150HMIN),
#else
	RPM_LDO(PM8058_L20, 0, 1, 0, 1800000, 1800000, LDO150HMIN),
#endif
	RPM_LDO(PM8058_L21, 1, 1, 0, 1200000, 1200000, LDO150HMIN),
	RPM_LDO(PM8058_L22, 0, 1, 0, 1150000, 1150000, LDO300HMIN),
	RPM_LDO(PM8058_L23, 0, 1, 0, 1200000, 1200000, LDO300HMIN),
	RPM_LDO(PM8058_L24, 0, 1, 0, 1200000, 1500000, LDO150HMIN),
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
#ifdef CONFIG_SENSORS_YDA165
	RPM_LDO(PM8901_L3,  0, 1, 0, 2600000, 3300000, LDO300HMIN),
#else
	RPM_LDO(PM8901_L3,  0, 1, 0, 3300000, 3300000, LDO300HMIN),
#endif
	RPM_LDO(PM8901_L4,  0, 1, 0, 1800000, 2800000, LDO300HMIN),
	RPM_LDO(PM8901_L5,  0, 1, 0, 2850000, 2850000, LDO300HMIN),
	RPM_LDO(PM8901_L6,  0, 1, 0, 2200000, 2200000, LDO300HMIN),

	/*	 ID       a_on pd ss min_uV   max_uV   init_ip   freq */
//	RPM_SMPS(PM8901_S2, 0, 1, 0, 1300000, 1300000, FTS_HMIN, 1p60),
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
#if 1 // SAMSUNG LDO SETTING
	RPM_PC(PM8901_L0,   0, SLEEP_B, RPM_VREG_PIN_CTRL_NONE),
#else
	RPM_PC(PM8901_L0,   0, ENABLE,  RPM_VREG_PIN_CTRL_PM8901_A0),
#endif
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

#if 0 //(defined(CONFIG_MARIMBA_CORE)) && (defined(CONFIG_MSM_BT_POWER) || defined(CONFIG_MSM_BT_POWER_MODULE))

static int bluetooth_power(int);
static struct platform_device msm_bt_power_device = {
	.name	 = "bt_power",
	.id	 = -1,
	.dev	 = {
		.platform_data = &bluetooth_power,
	},
};
#endif

/* Bluetooth */
#ifdef CONFIG_BT_BCM4330
static struct platform_device bcm4330_bluetooth_device = {
	.name = "bcm4330_bluetooth",
	.id = -1,
};
#endif

static struct platform_device msm_tsens_device = {
	.name   = "tsens-tm",
	.id = -1,
};

#ifdef CONFIG_VP_A2220
extern int a2220_ctrl(unsigned int cmd , unsigned long arg);

#define MSM_AUD_A2220_WAKEUP 	33
#define MSM_AUD_A2220_RESET 	126
#define MSM_AUD_A2220_CLK	0

#if !defined (CONFIG_TARGET_LOCALE_USA)
static struct i2c_gpio_platform_data a2220_i2c_gpio_data = {
	.sda_pin    = 35, // check done
	.scl_pin    = 36, //check done
	.udelay	= 1,
};

static struct platform_device a2220_i2c_device = {
	.name       = "i2c-gpio",
	.id     	= MSM_A2220_I2C_BUS_ID,
	.dev        = {
		.platform_data  = &a2220_i2c_gpio_data,
	},
};
#endif

static struct a2220_platform_data a2220_data = {
	.gpio_a2220_micsel = 0, // ??
	.gpio_a2220_wakeup = MSM_AUD_A2220_WAKEUP,
	.gpio_a2220_reset = MSM_AUD_A2220_RESET,
	.gpio_a2220_clk = MSM_AUD_A2220_CLK,
	.bypass = A2220_BYPASS_DATA_ONLY,
	.use_cases = {
#if defined(CONFIG_JPN_MODEL_SC_05D)
		1, /*A2220_PATH_SUSPEND*/
		1, /*A2220_PATH_INCALL_RECEIVER_NSON*/
		0, /*A2220_PATH_INCALL_RECEIVER_NSOFF*/
		0, /*A2220_PATH_INCALL_HEADSET*/
		0, /*A2220_PATH_INCALL_SPEAKER*/
#else
		0, /*A2220_PATH_SUSPEND*/
		1, /*A2220_PATH_INCALL_RECEIVER_NSON*/
		1, /*A2220_PATH_INCALL_RECEIVER_NSOFF*/
		0, /*A2220_PATH_INCALL_HEADSET*/
		1, /*A2220_PATH_INCALL_SPEAKER*/
#endif
		0, /*A2220_PATH_INCALL_BT*/
		0, /*A2220_PATH_VR_NO_NS_RECEIVER*/
		0, /*A2220_PATH_VR_NO_NS_HEADSET*/
		0, /*A2220_PATH_VR_NO_NS_SPEAKER*/
		0, /*A2220_PATH_VR_NO_NS_BT*/
		0, /*A2220_PATH_VR_NS_RECEIVER*/
		0, /*A2220_PATH_VR_NS_HEADSET*/
		0, /*A2220_PATH_VR_NS_SPEAKER*/
		0, /*A2220_PATH_VR_NS_BT*/
		0, /*A2220_PATH_RECORD_RECEIVER*/
		0, /*A2220_PATH_RECORD_HEADSET*/
		0, /*A2220_PATH_RECORD_SPEAKER*/
		0, /*A2220_PATH_RECORD_BT*/
		0, /*A2220_PATH_CAMCORDER*/
		0, /*A2220_PATH_INCALL_TTY*/
		1, /*A2220_PATH_BYPASS_MULTIMEDIA*/
		1, /*A2220_PATH_BYPASS_MULTIMEDIA_ALL*/
	}
	/*.gpio_a2220_int = MAHIMAHI_AUD_A2220_INT,*/
};

static struct i2c_board_info rev1_i2c_a2220_devices[] = {
	{
		I2C_BOARD_INFO("audience_a2220", 0x3E),
		.platform_data = &a2220_data,
		/*.irq = MSM_GPIO_TO_INT(MAHIMAHI_AUD_A2220_INT)*/
	},
};
#endif /* CONFIG_VP_A2220 */


static struct platform_device *rumi_sim_devices[] __initdata = {
	&smc91x_device,
	&msm_device_uart_dm12,
#ifdef CONFIG_I2C_QUP
#if defined (CONFIG_TARGET_LOCALE_USA)
        &msm_gsbi1_qup_i2c_device,
#else
#if defined(CONFIG_SPI_QUP) || defined(CONFIG_SPI_QUP_MODULE)
	&msm_gsbi1_qup_spi_device,
#endif
#endif
	&msm_gsbi3_qup_i2c_device,
	&msm_gsbi4_qup_i2c_device,
	&msm_gsbi7_qup_i2c_device,
	&msm_gsbi8_qup_i2c_device,
	&msm_gsbi9_qup_i2c_device,
#if !defined(CONFIG_KOR_MODEL_SHV_E160L)
	&msm_gsbi12_qup_i2c_device,
#endif
#endif

#ifdef CONFIG_I2C_SSBI
	&msm_device_ssbi3,
#endif
#ifdef CONFIG_ANDROID_PMEM
#ifndef CONFIG_MSM_MULTIMEDIA_USE_ION
	&android_pmem_device,
	&android_pmem_adsp_device,
	&android_pmem_smipool_device,
	&android_pmem_audio_device,
#endif /*CONFIG_MSM_MULTIMEDIA_USE_ION*/
#endif /*CONFIG_ANDROID_PMEM*/
#ifdef CONFIG_MSM_ROTATOR
	&msm_rotator_device,
#endif
	&msm_fb_device,
	&msm_kgsl_3d0,
	&msm_kgsl_2d0,
	&msm_kgsl_2d1,
#if 1 // def CONFIG_FB_MSM_MIPI_S6E8AA0_WXGA_Q1_PANEL
	&mipi_dsi_s6e8aa0_wxga_q1_panel_device,
#else		
	&lcdc_samsung_panel_device,
#endif
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
	&hdmi_msm_device,
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */
#ifdef CONFIG_MSM_CAMERA
#ifndef CONFIG_MSM_CAMERA_V4L2
#ifdef CONFIG_SENSOR_M5MO
	&msm_camera_sensor_m5mo,
#endif
#ifdef CONFIG_SENSOR_ISX012
	&msm_camera_sensor_isx012,
#endif
#ifdef CONFIG_SENSOR_S5K6AAFX
	&msm_camera_sensor_s5k6aafx,		
#endif
#ifdef CONFIG_SENSOR_S5K5BAFX
	&msm_camera_sensor_s5k5bafx,		
#endif
#ifdef CONFIG_SENSOR_SR200PC20M
	&msm_camera_sensor_sr200pc20m,
#endif
#endif
#endif
#ifdef CONFIG_MSM_GEMINI
	&msm_gemini_device,
#endif
#ifdef CONFIG_MSM_VPE
#ifndef CONFIG_MSM_CAMERA_V4L2
	&msm_vpe_device,
#endif
#endif
	&msm_device_vidc,
};

#if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)
enum {
	SX150X_CORE,
	SX150X_DOCKING,
	SX150X_SURF,
	SX150X_LEFT_FHA,
	SX150X_RIGHT_FHA,
	SX150X_SOUTH,
	SX150X_NORTH,
	SX150X_CORE_FLUID,
};

static struct sx150x_platform_data sx150x_data[] __initdata = {
	[SX150X_CORE] = {
		.gpio_base         = GPIO_CORE_EXPANDER_BASE,
		.oscio_is_gpo      = false,
		.io_pullup_ena     = 0x0c08,
		.io_pulldn_ena     = 0x4060,
		.io_open_drain_ena = 0x000c,
		.io_polarity       = 0,
		.irq_summary       = -1, /* see fixup_i2c_configs() */
		.irq_base          = GPIO_EXPANDER_IRQ_BASE,
	},
	[SX150X_DOCKING] = {
		.gpio_base         = GPIO_DOCKING_EXPANDER_BASE,
		.oscio_is_gpo      = false,
		.io_pullup_ena     = 0x5e06,
		.io_pulldn_ena     = 0x81b8,
		.io_open_drain_ena = 0,
		.io_polarity       = 0,
		.irq_summary       = PM8058_GPIO_IRQ(PM8058_IRQ_BASE,
						     UI_INT2_N),
		.irq_base          = GPIO_EXPANDER_IRQ_BASE +
				     GPIO_DOCKING_EXPANDER_BASE -
				     GPIO_EXPANDER_GPIO_BASE,
	},
	[SX150X_SURF] = {
		.gpio_base         = GPIO_SURF_EXPANDER_BASE,
		.oscio_is_gpo      = false,
		.io_pullup_ena     = 0,
		.io_pulldn_ena     = 0,
		.io_open_drain_ena = 0,
		.io_polarity       = 0,
		.irq_summary       = PM8058_GPIO_IRQ(PM8058_IRQ_BASE,
						     UI_INT1_N),
		.irq_base          = GPIO_EXPANDER_IRQ_BASE +
				     GPIO_SURF_EXPANDER_BASE -
				     GPIO_EXPANDER_GPIO_BASE,
	},
	[SX150X_LEFT_FHA] = {
		.gpio_base         = GPIO_LEFT_KB_EXPANDER_BASE,
		.oscio_is_gpo      = false,
		.io_pullup_ena     = 0,
		.io_pulldn_ena     = 0x40,
		.io_open_drain_ena = 0,
		.io_polarity       = 0,
		.irq_summary       = PM8058_GPIO_IRQ(PM8058_IRQ_BASE,
						     UI_INT3_N),
		.irq_base          = GPIO_EXPANDER_IRQ_BASE +
				     GPIO_LEFT_KB_EXPANDER_BASE -
				     GPIO_EXPANDER_GPIO_BASE,
	},
	[SX150X_RIGHT_FHA] = {
		.gpio_base         = GPIO_RIGHT_KB_EXPANDER_BASE,
		.oscio_is_gpo      = true,
		.io_pullup_ena     = 0,
		.io_pulldn_ena     = 0,
		.io_open_drain_ena = 0,
		.io_polarity       = 0,
		.irq_summary       = PM8058_GPIO_IRQ(PM8058_IRQ_BASE,
						     UI_INT3_N),
		.irq_base          = GPIO_EXPANDER_IRQ_BASE +
				     GPIO_RIGHT_KB_EXPANDER_BASE -
				     GPIO_EXPANDER_GPIO_BASE,
	},
	[SX150X_SOUTH] = {
		.gpio_base    = GPIO_SOUTH_EXPANDER_BASE,
		.irq_base     = GPIO_EXPANDER_IRQ_BASE +
				GPIO_SOUTH_EXPANDER_BASE -
				GPIO_EXPANDER_GPIO_BASE,
		.irq_summary  = PM8058_GPIO_IRQ(PM8058_IRQ_BASE, UI_INT3_N),
	},
	[SX150X_NORTH] = {
		.gpio_base    = GPIO_NORTH_EXPANDER_BASE,
		.irq_base     = GPIO_EXPANDER_IRQ_BASE +
				GPIO_NORTH_EXPANDER_BASE -
				GPIO_EXPANDER_GPIO_BASE,
		.irq_summary  = PM8058_GPIO_IRQ(PM8058_IRQ_BASE, UI_INT3_N),
		.oscio_is_gpo = true,
		.io_open_drain_ena = 0x30,
	},
	[SX150X_CORE_FLUID] = {
		.gpio_base         = GPIO_CORE_EXPANDER_BASE,
		.oscio_is_gpo      = false,
		.io_pullup_ena     = 0x0408,
		.io_pulldn_ena     = 0x4060,
		.io_open_drain_ena = 0x0008,
		.io_polarity       = 0,
		.irq_summary       = -1, /* see fixup_i2c_configs() */
		.irq_base          = GPIO_EXPANDER_IRQ_BASE,
	},
};

#ifdef CONFIG_SENSORS_MSM_ADC
/* Configuration of EPM expander is done when client
 * request an adc read
 */
static struct sx150x_platform_data sx150x_epmdata = {
	.gpio_base         = GPIO_EPM_EXPANDER_BASE,
	.irq_base	   = GPIO_EXPANDER_IRQ_BASE +
				GPIO_EPM_EXPANDER_BASE -
				GPIO_EXPANDER_GPIO_BASE,
	.irq_summary       = -1,
};
#endif

/* sx150x_low_power_cfg
 *
 * This data and init function are used to put unused gpio-expander output
 * lines into their low-power states at boot. The init
 * function must be deferred until a later init stage because the i2c
 * gpio expander drivers do not probe until after they are registered
 * (see register_i2c_devices) and the work-queues for those registrations
 * are processed.  Because these lines are unused, there is no risk of
 * competing with a device driver for the gpio.
 *
 * gpio lines whose low-power states are input are naturally in their low-
 * power configurations once probed, see the platform data structures above.
 */
struct sx150x_low_power_cfg {
	unsigned gpio;
	unsigned val;
};

static struct sx150x_low_power_cfg
common_sx150x_lp_cfgs[] __initdata = {
	{GPIO_WLAN_DEEP_SLEEP_N, 0},
	{GPIO_EXT_GPS_LNA_EN,    0},
	{GPIO_MSM_WAKES_BT,      0},
	{GPIO_USB_UICC_EN,       0},
	{GPIO_BATT_GAUGE_EN,     0},
};

static struct sx150x_low_power_cfg
surf_ffa_sx150x_lp_cfgs[] __initdata = {
	{GPIO_MIPI_DSI_RST_N,      0},
	{GPIO_DONGLE_PWR_EN,       0},
	{GPIO_CAP_TS_SLEEP,        1},
	{GPIO_WEB_CAMIF_RESET_N,   0},
};

#if !defined (CONFIG_JPN_MODEL_SC_05D)
static void __init
cfg_gpio_low_power(struct sx150x_low_power_cfg *cfgs, unsigned nelems)
{
	unsigned n;
	int rc;

	for (n = 0; n < nelems; ++n) {
		rc = gpio_request(cfgs[n].gpio, NULL);
		if (!rc) {
			rc = gpio_direction_output(cfgs[n].gpio, cfgs[n].val);
			gpio_free(cfgs[n].gpio);
		}

		if (rc) {
			printk(KERN_NOTICE "%s: failed to sleep gpio %d: %d\n",
			       __func__, cfgs[n].gpio, rc);
		}
	}
}

static int __init cfg_sx150xs_low_power(void)
{
	cfg_gpio_low_power(common_sx150x_lp_cfgs,
		ARRAY_SIZE(common_sx150x_lp_cfgs));
	if (!machine_is_msm8x60_fluid())
		cfg_gpio_low_power(surf_ffa_sx150x_lp_cfgs,
			ARRAY_SIZE(surf_ffa_sx150x_lp_cfgs));
	return 0;
}
module_init(cfg_sx150xs_low_power);
#endif

#ifdef CONFIG_I2C
#if defined (CONFIG_JPN_MODEL_SC_05D) // removed gpio expander, #if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)
static struct i2c_board_info core_expander_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO("sx1509q", 0x3e),
		.platform_data = &sx150x_data[SX150X_CORE]
	},
};

static struct i2c_board_info docking_expander_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO("sx1509q", 0x3f),
		.platform_data = &sx150x_data[SX150X_DOCKING]
	},
};

static struct i2c_board_info surf_expanders_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO("sx1509q", 0x70),
		.platform_data = &sx150x_data[SX150X_SURF]
	}
};

static struct i2c_board_info fha_expanders_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO("sx1508q", 0x21),
		.platform_data = &sx150x_data[SX150X_LEFT_FHA]
	},
	{
		I2C_BOARD_INFO("sx1508q", 0x22),
		.platform_data = &sx150x_data[SX150X_RIGHT_FHA]
	}
};

static struct i2c_board_info fluid_expanders_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO("sx1508q", 0x23),
		.platform_data = &sx150x_data[SX150X_SOUTH]
	},
	{
		I2C_BOARD_INFO("sx1508q", 0x20),
		.platform_data = &sx150x_data[SX150X_NORTH]
	}
};

static struct i2c_board_info fluid_core_expander_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO("sx1509q", 0x3e),
		.platform_data = &sx150x_data[SX150X_CORE_FLUID]
	},
};
#endif

#ifdef CONFIG_SENSORS_MSM_ADC
static struct i2c_board_info fluid_expanders_i2c_epm_info[] = {
	{
		I2C_BOARD_INFO("sx1509q", 0x3e),
		.platform_data = &sx150x_epmdata
	},
};
#endif
#endif
#endif /* CONFIG_GPIO_SX150X || CONFIG_GPIO_SX150X_MODULE */


#ifdef CONFIG_SENSORS_MSM_ADC

static struct adc_access_fn xoadc_fn = {
	pm8058_xoadc_select_chan_and_start_conv,
	pm8058_xoadc_read_adc_code,
	pm8058_xoadc_get_properties,
	pm8058_xoadc_slot_request,
	pm8058_xoadc_restore_slot,
	pm8058_xoadc_calibrate,
};

#if defined(CONFIG_I2C) && \
	(defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE))
static struct regulator *vreg_adc_epm1;

static struct i2c_client *epm_expander_i2c_register_board(void)

{
	struct i2c_adapter *i2c_adap;
	struct i2c_client *client = NULL;
	i2c_adap = i2c_get_adapter(0x0);

	if (i2c_adap == NULL)
		printk(KERN_ERR "\nepm_expander_i2c_adapter is NULL\n");

	if (i2c_adap != NULL)
		client = i2c_new_device(i2c_adap,
			&fluid_expanders_i2c_epm_info[0]);
	return client;

}

static unsigned int msm_adc_gpio_configure_expander_enable(void)
{
	int rc = 0;
	static struct i2c_client *epm_i2c_client;

	printk(KERN_DEBUG "Enter msm_adc_gpio_configure_expander_enable\n");

	vreg_adc_epm1 = regulator_get(NULL, "8058_s3");

	if (IS_ERR(vreg_adc_epm1)) {
		printk(KERN_ERR "%s: Unable to get 8058_s3\n", __func__);
		return 0;
	}

	rc = regulator_set_voltage(vreg_adc_epm1, 1800000, 1800000);
	if (rc)
		printk(KERN_ERR "msm_adc_gpio_configure_expander_enable: "
				"regulator set voltage failed\n");

	rc = regulator_enable(vreg_adc_epm1);
	if (rc) {
		printk(KERN_ERR "msm_adc_gpio_configure_expander_enable: "
			"Error while enabling regulator for epm s3 %d\n", rc);
		return rc;
	}

	printk(KERN_DEBUG "msm_adc_gpio_configure_expander_enable: Start"
			" setting the value of the EPM 3.3, 5v and lvlsft\n");

	msleep(1000);

	rc = gpio_request(GPIO_EPM_5V_BOOST_EN, "boost_epm_5v");
	if (!rc) {
		printk(KERN_DEBUG "msm_adc_gpio_configure_expander_enable: "
				"Configure 5v boost\n");
		gpio_direction_output(GPIO_EPM_5V_BOOST_EN, 1);
	} else {
		printk(KERN_ERR "msm_adc_gpio_configure_expander_enable: "
				"Error for epm 5v boost en\n");
		goto exit_vreg_epm;
	}

	msleep(500);

	rc = gpio_request(GPIO_EPM_3_3V_EN, "epm_3_3v");
	if (!rc) {
		gpio_direction_output(GPIO_EPM_3_3V_EN, 1);
		printk(KERN_DEBUG "msm_adc_gpio_configure_expander_enable: "
				"Configure epm 3.3v\n");
	} else {
		printk(KERN_ERR "msm_adc_gpio_configure_expander_enable: "
				"Error for gpio 3.3ven\n");
		goto exit_vreg_epm;
	}
	msleep(500);

	printk(KERN_DEBUG "msm_adc_gpio_configure_expander_enable: "
			"Trying to request EPM LVLSFT_EN\n");
	rc = gpio_request(GPIO_EPM_LVLSFT_EN, "lvsft_en");
	if (!rc) {
		gpio_direction_output(GPIO_EPM_LVLSFT_EN, 1);
		printk(KERN_DEBUG "msm_adc_gpio_configure_expander_enable: "
				"Configure the lvlsft\n");
	} else {
		printk(KERN_ERR "msm_adc_gpio_configure_expander_enable: "
				"Error for epm lvlsft_en\n");
		goto exit_vreg_epm;
	}

	msleep(500);

	if (!epm_i2c_client)
		epm_i2c_client = epm_expander_i2c_register_board();

	rc = gpio_request(GPIO_PWR_MON_ENABLE, "pwr_mon_enable");
	if (!rc)
		rc = gpio_direction_output(GPIO_PWR_MON_ENABLE, 1);
		if (rc) {
			printk(KERN_ERR "msm_adc_gpio_configure_expander_enable"
					": GPIO PWR MON Enable issue\n");
			goto exit_vreg_epm;
		}

	msleep(1000);

	rc = gpio_request(GPIO_ADC1_PWDN_N, "adc1_pwdn");
	if (!rc) {
		rc = gpio_direction_output(GPIO_ADC1_PWDN_N, 1);
		if (rc) {
			printk(KERN_ERR "msm_adc_gpio_configure_expander_enable"
					": ADC1_PWDN error direction out\n");
			goto exit_vreg_epm;
		}
	}

	msleep(100);

	rc = gpio_request(GPIO_ADC2_PWDN_N, "adc2_pwdn");
	if (!rc) {
		rc = gpio_direction_output(GPIO_ADC2_PWDN_N, 1);
		if (rc) {
			printk(KERN_ERR "msm_adc_gpio_configure_expander_enable"
					": ADC2_PWD error direction out\n");
			goto exit_vreg_epm;
		}
	}

	msleep(1000);

	rc = gpio_request(GPIO_PWR_MON_START, "pwr_mon_start");
	if (!rc) {
		rc = gpio_direction_output(GPIO_PWR_MON_START, 0);
		if (rc) {
			printk(KERN_ERR "msm_adc_gpio_configure_expander_enable"
				"Gpio request problem %d\n", rc);
			goto exit_vreg_epm;
		}
	}

	rc = gpio_request(GPIO_EPM_SPI_ADC1_CS_N, "spi_adc1_cs");
	if (!rc) {
		rc = gpio_direction_output(GPIO_EPM_SPI_ADC1_CS_N, 0);
		if (rc) {
			printk(KERN_ERR "msm_adc_gpio_configure_expander_enable"
					": EPM_SPI_ADC1_CS_N error\n");
			goto exit_vreg_epm;
		}
	}

	rc = gpio_request(GPIO_EPM_SPI_ADC2_CS_N, "spi_adc2_cs");
	if (!rc) {
		rc = gpio_direction_output(GPIO_EPM_SPI_ADC2_CS_N, 0);
		if (rc) {
			printk(KERN_ERR "msm_adc_gpio_configure_expander_enable"
					": EPM_SPI_ADC2_Cs_N error\n");
			goto exit_vreg_epm;
		}
	}

	printk(KERN_DEBUG "msm_adc_gpio_configure_expander_enable: Set "
			"the power monitor reset for epm\n");

	rc = gpio_request(GPIO_PWR_MON_RESET_N, "pwr_mon_reset_n");
	if (!rc) {
		gpio_direction_output(GPIO_PWR_MON_RESET_N, 0);
		if (rc)	{
			printk(KERN_ERR "msm_adc_gpio_configure_expander_enable"
					": Error in the power mon reset\n");
			goto exit_vreg_epm;
		}
	}

	msleep(1000);

	gpio_set_value_cansleep(GPIO_PWR_MON_RESET_N, 1);

	msleep(500);

	gpio_set_value_cansleep(GPIO_EPM_SPI_ADC1_CS_N, 1);

	gpio_set_value_cansleep(GPIO_EPM_SPI_ADC2_CS_N, 1);

	return rc;

exit_vreg_epm:
	regulator_disable(vreg_adc_epm1);

	printk(KERN_ERR "msm_adc_gpio_configure_expander_enable: Exit."
			" rc = %d.\n", rc);
	return rc;
};

static unsigned int msm_adc_gpio_configure_expander_disable(void)
{
	int rc = 0;

	gpio_set_value_cansleep(GPIO_PWR_MON_RESET_N, 0);
	gpio_free(GPIO_PWR_MON_RESET_N);

	gpio_set_value_cansleep(GPIO_EPM_SPI_ADC1_CS_N, 0);
	gpio_free(GPIO_EPM_SPI_ADC1_CS_N);

	gpio_set_value_cansleep(GPIO_EPM_SPI_ADC2_CS_N, 0);
	gpio_free(GPIO_EPM_SPI_ADC2_CS_N);

	gpio_set_value_cansleep(GPIO_PWR_MON_START, 0);
	gpio_free(GPIO_PWR_MON_START);

	gpio_direction_output(GPIO_ADC1_PWDN_N, 0);
	gpio_free(GPIO_ADC1_PWDN_N);

	gpio_direction_output(GPIO_ADC2_PWDN_N, 0);
	gpio_free(GPIO_ADC2_PWDN_N);

	gpio_set_value_cansleep(GPIO_PWR_MON_ENABLE, 0);
	gpio_free(GPIO_PWR_MON_ENABLE);

	gpio_set_value_cansleep(GPIO_EPM_LVLSFT_EN, 0);
	gpio_free(GPIO_EPM_LVLSFT_EN);

	gpio_set_value_cansleep(GPIO_EPM_5V_BOOST_EN, 0);
	gpio_free(GPIO_EPM_5V_BOOST_EN);

	gpio_set_value_cansleep(GPIO_EPM_3_3V_EN, 0);
	gpio_free(GPIO_EPM_3_3V_EN);

	rc = regulator_disable(vreg_adc_epm1);
	if (rc)
		printk(KERN_DEBUG "msm_adc_gpio_configure_expander_disable: "
			"Error while enabling regulator for epm s3 %d\n", rc);
	regulator_put(vreg_adc_epm1);

	printk(KERN_DEBUG "Exi msm_adc_gpio_configure_expander_disable\n");
	return rc;
};

unsigned int msm_adc_gpio_expander_enable(int cs_enable)
{
	int rc = 0;

	printk(KERN_DEBUG "msm_adc_gpio_expander_enable: cs_enable = %d",
		cs_enable);

	if (cs_enable < 16) {
		gpio_set_value_cansleep(GPIO_EPM_SPI_ADC1_CS_N, 0);
		gpio_set_value_cansleep(GPIO_EPM_SPI_ADC2_CS_N, 1);
	} else {
		gpio_set_value_cansleep(GPIO_EPM_SPI_ADC2_CS_N, 0);
		gpio_set_value_cansleep(GPIO_EPM_SPI_ADC1_CS_N, 1);
	}
	return rc;
};

unsigned int msm_adc_gpio_expander_disable(int cs_disable)
{
	int rc = 0;

	printk(KERN_DEBUG "Enter msm_adc_gpio_expander_disable.\n");

	gpio_set_value_cansleep(GPIO_EPM_SPI_ADC1_CS_N, 1);

	gpio_set_value_cansleep(GPIO_EPM_SPI_ADC2_CS_N, 1);

	return rc;
};
#endif

static struct msm_adc_channels msm_adc_channels_data[] = {
#if defined (CONFIG_JPN_MODEL_SC_05D)
	{"head_set", CHANNEL_ADC_HDSET, 0, &xoadc_fn, CHAN_PATH_TYPE6,
#else
	{"head_set", CHANNEL_ADC_HDSET, 0, &xoadc_fn, CHAN_PATH_TYPE9,
#endif
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_default},
	{"light_lux", CHANNEL_ADC_LIGHT_LUX, 0, &xoadc_fn, CHAN_PATH_TYPE7,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_default},
	{"vichg", CHANNEL_ADC_CHG_MONITOR, 0, &xoadc_fn, CHAN_PATH_TYPE10,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_xtern_chgr_cur},
#if defined (CONFIG_JPN_MODEL_SC_05D)
	{"batt_id", CHANNEL_ADC_BATT_ID, 0, &xoadc_fn, CHAN_PATH_TYPE9,
#else
	{"batt_id", CHANNEL_ADC_BATT_ID, 0, &xoadc_fn, CHAN_PATH_TYPE6,
#endif
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_default},
#if defined (CONFIG_TARGET_LOCALE_USA)
	{"batt_therm", CHANNEL_ADC_BATT_THERM, 0, &xoadc_fn, CHAN_PATH_TYPE8,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_default},
#else
	{"batt_therm", CHANNEL_ADC_BATT_THERM, 0, &xoadc_fn, CHAN_PATH_TYPE8,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_sec_settherm},
#endif
	{"pmic_therm", CHANNEL_ADC_PMIC_THERM, 0, &xoadc_fn, CHAN_PATH_TYPE12,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_pmic_therm},
	{"xo_therm", CHANNEL_ADC_XOTHERM, 0, &xoadc_fn, CHAN_PATH_TYPE_NONE,
		ADC_CONFIG_TYPE2, ADC_CALIB_CONFIG_TYPE2, scale_sec_settherm},
	{"xo_therm_4K", CHANNEL_ADC_XOTHERM_4K, 0, &xoadc_fn, CHAN_PATH_TYPE_NONE,
		ADC_CONFIG_TYPE1, ADC_CALIB_CONFIG_TYPE2, scale_sec_settherm}
};

static char *msm_adc_fluid_device_names[] = {
	"ADS_ADC1",
	"ADS_ADC2",
};

static struct msm_adc_platform_data msm_adc_pdata = {
	.channel = msm_adc_channels_data,
	.num_chan_supported = ARRAY_SIZE(msm_adc_channels_data),
#if defined(CONFIG_I2C) && \
	(defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE))
	.adc_gpio_enable = msm_adc_gpio_expander_enable,
	.adc_gpio_disable   = msm_adc_gpio_expander_disable,
	.adc_fluid_enable = msm_adc_gpio_configure_expander_enable,
	.adc_fluid_disable  = msm_adc_gpio_configure_expander_disable,
#endif
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
#if defined (CONFIG_JPN_MODEL_SC_05D)
		PM8058_MPP_INIT(XOADC_MPP_3, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH5,
#else
		PM8058_MPP_INIT(XOADC_MPP_3, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH8,
#endif
							AOUT_CTRL_DISABLE),
		PM8058_MPP_INIT(XOADC_MPP_5, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH9,
							AOUT_CTRL_DISABLE),
		PM8058_MPP_INIT(XOADC_MPP_4, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH6,
							AOUT_CTRL_DISABLE),
#if defined (CONFIG_JPN_MODEL_SC_05D)
		PM8058_MPP_INIT(XOADC_MPP_8, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH8,
#else
		PM8058_MPP_INIT(XOADC_MPP_8, A_INPUT, PM8XXX_MPP_AIN_AMUX_CH5,
#endif
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
#endif /* CONFIG_SENSORS_MSM_ADC */


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
#if !defined (CONFIG_JPN_MODEL_SC_05D) //if this is enabled, hdmi_enable_5v() fails so we move it to "vreg_consumers_8901_HDMI_MVS" below
	REGULATOR_SUPPLY("8901_mpp0",	NULL),
#endif
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

#if defined (CONFIG_OPTICAL_GP2A)
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

struct opt_gp2a_platform_data {
	void	(*gp2a_led_on) (void);
	void	(*gp2a_led_off) (void);
	void	(*power_on) (void);
	void	(*power_off) (void);
    int gp2a_irq;
    int gp2a_gpio;
};

static void proximity_LED_on(void);
static void proximity_LED_off(void);

static struct opt_gp2a_platform_data opt_gp2a_data = {
	.gp2a_led_on	= proximity_LED_on,
	.gp2a_led_off	= proximity_LED_off,
	.power_on = sensor_power_on,
	.power_off = sensor_power_off,
	.gp2a_irq = PM8058_GPIO_IRQ(PM8058_IRQ_BASE, (PMIC_GPIO_PS_VOUT)),
	.gp2a_gpio = PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_PS_VOUT),
};

static struct platform_device opt_gp2a = {
	.name = "gp2a-opt",
	.id = -1,
	.dev        = {
		.platform_data  = &opt_gp2a_data,
	},
};
#endif /* CONFIG_OPTICAL_GP2A */

#if defined (CONFIG_OPTICAL_TAOS)
static struct i2c_gpio_platform_data taos_i2c_gpio_data = {	
	.sda_pin    = SENSOR_ALS_SDA,
	.scl_pin    = SENSOR_ALS_SCL,
};

static struct platform_device taos_i2c_gpio_device = {  
	.name       = "i2c-gpio",
	.id     = MSM_OPT_I2C_BUS_ID,
	.dev        = {		
		.platform_data  = &taos_i2c_gpio_data,	
	},
};

struct taos_platform_data {	
	void	(*led_on) (void);	
	void	(*led_off) (void);	
	void	(*power_on) (void);	
	void	(*power_off) (void);
#if defined (CONFIG_USA_MODEL_SGH_I757) || defined (CONFIG_CAN_MODEL_SGH_I757M)	
	void 	(*light_on) (void);	
	void 	(*light_off) (void);
#endif
};

static void proximity_LED_on(void);
static void proximity_LED_off(void);

static struct taos_platform_data taos_pdata = {	
	.led_on = proximity_LED_on,	
	.led_off = proximity_LED_off,	
	.power_on = sensor_power_on,	
	.power_off = sensor_power_off,
#if defined (CONFIG_USA_MODEL_SGH_I757) || defined (CONFIG_CAN_MODEL_SGH_I757M)	
	.light_on = taos_power_on,	
	.light_off = taos_power_off,
#endif
};

static struct i2c_board_info taos_i2c_board_info[] = {	
	{		
		I2C_BOARD_INFO("taos", 0x39),		
		.platform_data	= &taos_pdata,	
	},
};
#endif

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
#if defined (CONFIG_TARGET_LOCALE_USA)
        &msm_gsbi1_qup_i2c_device,
#else
#if defined(CONFIG_SPI_QUP) || defined(CONFIG_SPI_QUP_MODULE)
	&msm_gsbi1_qup_spi_device,
#endif
#endif
	&msm_gsbi3_qup_i2c_device,
	&msm_gsbi4_qup_i2c_device,
	&msm_gsbi7_qup_i2c_device,
	&msm_gsbi8_qup_i2c_device,
	&msm_gsbi9_qup_i2c_device,
#if defined (CONFIG_EPEN_WACOM_G5SP)
    &msm_gsbi11_qup_i2c_device,
#endif		
#if !defined(CONFIG_KOR_MODEL_SHV_E160L)
	&msm_gsbi12_qup_i2c_device,
#endif
#endif
#if defined(CONFIG_PN544_NFC)
	&msm_gsbi10_qup_i2c_device,
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
#if defined(CONFIG_USB_PEHCI_HCD) || defined(CONFIG_USB_PEHCI_HCD_MODULE)
	&isp1763_device,
#endif
#if defined(CONFIG_BATTERY_MAX17040) || defined(CONFIG_CHARGER_SMB328A)
	&fg_smb_i2c_gpio_device,
#endif
#if defined (CONFIG_MSM_8x60_VOIP)
	&asoc_msm_mvs,
	&asoc_mvs_dai0,
	&asoc_mvs_dai1,
#endif
#ifdef CONFIG_BATTERY_SEC
	&sec_device_battery,
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
#ifdef CONFIG_BATTERY_MSM
	&msm_batt_device,
#endif
#ifdef CONFIG_USB_SWITCH_FSA9480
#if defined(CONFIG_KOR_MODEL_SHV_E160L)
	&usb_i2c_gpio_device,
#endif
	&sec_device_switch,  // samsung switch driver
#endif
#ifdef CONFIG_ANDROID_PMEM
#ifndef CONFIG_MSM_MULTIMEDIA_USE_ION
	&android_pmem_device,
	&android_pmem_adsp_device,
	&android_pmem_smipool_device,
#endif /*CONFIG_MSM_MULTIMEDIA_USE_ION*/
#if !defined(CONFIG_MSM_MULTIMEDIA_USE_ION) || defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_JPN_MODEL_SC_05D)
	&android_pmem_audio_device,
#endif
#endif /*CONFIG_ANDROID_PMEM*/
#ifdef CONFIG_MSM_ROTATOR
	&msm_rotator_device,
#endif
	&msm_fb_device,
	&msm_kgsl_3d0,
	&msm_kgsl_2d0,
	&msm_kgsl_2d1,

#if 1// def CONFIG_FB_MSM_MIPI_S6E8AA0_WXGA_Q1_PANEL
	&mipi_dsi_s6e8aa0_wxga_q1_panel_device,
#else	
	&lcdc_samsung_panel_device,
#endif
#ifdef CONFIG_FB_MSM_LCDC_NT35582_WVGA
	&lcdc_nt35582_panel_device,
#endif
#ifdef CONFIG_FB_MSM_LCDC_SAMSUNG_OLED_PT
	&lcdc_samsung_oled_panel_device,
#endif
#ifdef CONFIG_FB_MSM_LCDC_AUO_WVGA
	&lcdc_auo_wvga_panel_device,
#endif
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
	&hdmi_msm_device,
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */
#ifdef CONFIG_FB_MSM_MIPI_DSI
	&mipi_dsi_toshiba_panel_device,
	&mipi_dsi_novatek_panel_device,
#endif
#ifdef CONFIG_MSM_CAMERA
#ifndef CONFIG_MSM_CAMERA_V4L2
#ifdef CONFIG_SENSOR_M5MO
	&msm_camera_sensor_m5mo,
#endif
#ifdef CONFIG_SENSOR_ISX012
	&msm_camera_sensor_isx012,
#endif
#ifdef CONFIG_SENSOR_S5K6AAFX
	&msm_camera_sensor_s5k6aafx,		
#endif
#ifdef CONFIG_SENSOR_S5K5BAFX
	&msm_camera_sensor_s5k5bafx,		
#endif
#ifdef CONFIG_SENSOR_SR200PC20M
	&msm_camera_sensor_sr200pc20m,
#endif
#endif
#endif
#ifdef CONFIG_MSM_GEMINI
	&msm_gemini_device,
#endif
#ifdef CONFIG_MSM_VPE
#ifndef CONFIG_MSM_CAMERA_V4L2
	&msm_vpe_device,
#endif
#endif

#if defined(CONFIG_MSM_RPM_LOG) || defined(CONFIG_MSM_RPM_LOG_MODULE)
	&msm_rpm_log_device,
#endif
#if defined(CONFIG_MSM_RPM_STATS_LOG)
	&msm_rpm_stat_device,
#endif
	&msm_device_vidc,
#if 0 //(defined(CONFIG_MARIMBA_CORE)) && (defined(CONFIG_MSM_BT_POWER) || defined(CONFIG_MSM_BT_POWER_MODULE))
	&msm_bt_power_device,
#endif

#if defined (CONFIG_BT_BCM4330)
    &bcm4330_bluetooth_device,
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


#if defined(CONFIG_TSIF) || defined(CONFIG_TSIF_MODULE)
#ifdef CONFIG_MSM_USE_TSIF1
	&msm_device_tsif[1],
#else
	&msm_device_tsif[0],
#endif /* CONFIG_MSM_USE_TSIF1 */
#endif /* CONFIG_TSIF */

#ifdef CONFIG_HW_RANDOM_MSM
	&msm_device_rng,
#endif

	&msm_tsens_device,
	&msm_rpm_device,
#ifdef CONFIG_ION_MSM
	&ion_dev,
#endif
	&msm8660_device_watchdog,
#if defined (CONFIG_SAMSUNG_JACK) || defined (CONFIG_SAMSUNG_EARJACK)
	&sec_device_jack,
#endif
#ifdef CONFIG_FB_MSM_MIPI_DSI_ESD_REFRESH
#if !defined(CONFIG_JPN_MODEL_SC_05D)	
	&sec_device_mipi_esd,
#endif
#endif
#if defined (CONFIG_OPTICAL_GP2A)
	&opt_i2c_gpio_device,
	&opt_gp2a,
#endif
#ifdef CONFIG_OPTICAL_TAOS	
	&taos_i2c_gpio_device,
#endif
#ifdef CONFIG_KEYPAD_CYPRESS_TOUCH
	&tkey_i2c_gpio_device,
#endif
#ifdef CONFIG_VP_A2220
#if !defined (CONFIG_TARGET_LOCALE_USA)
	&a2220_i2c_device,
#endif
#endif
#ifdef CONFIG_GYRO_K3G
	&gyro_i2c_gpio_device,
#endif
#ifdef CONFIG_SENSORS_AK8975
	&akm_i2c_gpio_device,
#endif
	&motor_i2c_gpio_device,
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	&ram_console_device,
#endif
};

#ifdef CONFIG_ION_MSM
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
static struct ion_cp_heap_pdata cp_mm_ion_pdata = {
	.permission_type = IPT_TYPE_MM_CARVEOUT,
	.align = SZ_64K,
	.request_region = request_smi_region,
	.release_region = release_smi_region,
	.setup_region = setup_smi_region,
	.iommu_map_all = 1,
	.iommu_2x_map_domain = VIDEO_DOMAIN,
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
#if defined (CONFIG_SEC_KERNEL_REBASE_FOR_PMEM_OPTIMIZATION) && defined(CONFIG_USA_MODEL_SGH_I717) \
            && !defined (CONFIG_USA_MODEL_SGH_T879)
			.base = MSM_ION_SF_BASE,
#endif
#if defined(CONFIG_JPN_MODEL_SC_05D)
			.base = MSM_ION_SF_BASE,
#endif
			.size	= MSM_ION_SF_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *)&co_ion_pdata,
		},
		{
			.id	= ION_CP_WB_HEAP_ID,
			.type	= ION_HEAP_TYPE_CP,
			.name	= ION_WB_HEAP_NAME,
			.size	= MSM_ION_WB_SIZE,
#ifdef CONFIG_SEC_KERNEL_REBASE_FOR_PMEM_OPTIMIZATION
			.memory_type = ION_ADSP_TYPE,
#else
			.memory_type = ION_EBI_TYPE,
#endif
			.extra_data = (void *) &cp_wb_ion_pdata,
		},
		{
			.id	= ION_CAMERA_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_CAMERA_HEAP_NAME,
			.size	= MSM_ION_CAMERA_SIZE,
#ifdef CONFIG_SEC_KERNEL_REBASE_FOR_PMEM_OPTIMIZATION
			.memory_type = ION_ADSP_TYPE,
#else
			.memory_type = ION_EBI_TYPE,
#endif
			.extra_data = &co_ion_pdata,
		},
#ifdef CONFIG_QSEECOM
		{
			.id	= ION_QSECOM_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_QSECOM_HEAP_NAME,
			.size	= MSM_ION_QSECOM_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *) &co_ion_pdata,
		},
#endif
		{
			.id	= ION_AUDIO_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_AUDIO_HEAP_NAME,
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
#endif /* CONFIG_ION_MSM */


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
#ifdef CONFIG_SEC_KERNEL_REBASE_FOR_PMEM_OPTIMIZATION
	[MEMTYPE_PMEM_ADSP] = {
		.start	=	MSM_PMEM_ADSP_BASE,
		.limit	=	MSM_PMEM_ADSP_SIZE,
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
#if !defined (CONFIG_SEC_KERNEL_REBASE_FOR_PMEM_OPTIMIZATION) || !defined(CONFIG_USA_MODEL_SGH_I717) \
	|| defined (CONFIG_USA_MODEL_SGH_T879)
#if !defined(CONFIG_JPN_MODEL_SC_05D)
	unsigned int i;

	if (hdmi_is_primary) {
		msm_ion_sf_size = MSM_HDMI_PRIM_ION_SF_SIZE;
		for (i = 0; i < ion_pdata.nr; i++) {
			if (ion_pdata.heaps[i].id == ION_SF_HEAP_ID) {
				ion_pdata.heaps[i].size = msm_ion_sf_size;
				pr_debug("msm_ion_sf_size 0x%x\n",
					msm_ion_sf_size);
				break;
			}
		}
	}

	/* Verify size of heap is a multiple of 64K */
	for (i = 0; i < ion_pdata.nr; i++) {
		struct ion_platform_heap *heap = &(ion_pdata.heaps[i]);

		if (heap->extra_data && heap->type == ION_HEAP_TYPE_CP) {
			int map_all = ((struct ion_cp_heap_pdata *)
				heap->extra_data)->iommu_map_all;

			if (map_all && (heap->size & (SZ_64K-1))) {
				heap->size = ALIGN(heap->size, SZ_64K);
				pr_err("Heap %s size is not a multiple of 64K. Adjusting size to %x\n",
					heap->name, heap->size);

			}
		}
	}

	msm8x60_reserve_table[MEMTYPE_EBI1].size += msm_ion_sf_size;
#endif
#endif
	msm8x60_reserve_table[MEMTYPE_SMI].size += MSM_ION_MM_FW_SIZE;
	msm8x60_reserve_table[MEMTYPE_SMI].size += MSM_ION_MM_SIZE;
	msm8x60_reserve_table[MEMTYPE_SMI].size += MSM_ION_MFC_SIZE;
#ifndef CONFIG_SEC_KERNEL_REBASE_FOR_PMEM_OPTIMIZATION
	msm8x60_reserve_table[MEMTYPE_EBI1].size += MSM_ION_CAMERA_SIZE;
	msm8x60_reserve_table[MEMTYPE_EBI1].size += MSM_ION_WB_SIZE;
#else
	msm8x60_reserve_table[MEMTYPE_PMEM_ADSP].size += MSM_ION_CAMERA_SIZE;
	msm8x60_reserve_table[MEMTYPE_PMEM_ADSP].size += MSM_ION_WB_SIZE;
#endif
#ifdef CONFIG_QSEECOM
	msm8x60_reserve_table[MEMTYPE_EBI1].size += MSM_ION_QSECOM_SIZE;
#endif
	msm8x60_reserve_table[MEMTYPE_EBI1].size += MSM_ION_AUDIO_SIZE;
#endif
}

static void __init size_pmem_devices(void)
{
#ifdef CONFIG_ANDROID_PMEM
#ifndef CONFIG_MSM_MULTIMEDIA_USE_ION
	android_pmem_adsp_pdata.size = pmem_adsp_size;
	android_pmem_smipool_pdata.size = MSM_PMEM_SMIPOOL_SIZE;

	if (hdmi_is_primary)
		pmem_sf_size = MSM_HDMI_PRIM_PMEM_SF_SIZE;
	android_pmem_pdata.size = pmem_sf_size;
#endif /*CONFIG_MSM_MULTIMEDIA_USE_ION*/
#if !defined (CONFIG_MSM_MULTIMEDIA_USE_ION) || defined (CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_JPN_MODEL_SC_05D)
	android_pmem_audio_pdata.size = MSM_PMEM_AUDIO_SIZE;
#endif
#endif /*CONFIG_ANDROID_PMEM*/
}

#if defined (CONFIG_ANDROID_PMEM) || defined (CONFIG_USA_MODEL_SGH_I717)
#if !defined (CONFIG_MSM_MULTIMEDIA_USE_ION) || defined (CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_JPN_MODEL_SC_05D)
static void __init reserve_memory_for(struct android_pmem_platform_data *p)
{
	msm8x60_reserve_table[p->memory_type].size += p->size;
}
#endif /*CONFIG_MSM_MULTIMEDIA_USE_ION*/
#endif /*CONFIG_ANDROID_PMEM*/

static void __init reserve_pmem_memory(void)
{
#ifdef CONFIG_ANDROID_PMEM
#ifndef CONFIG_MSM_MULTIMEDIA_USE_ION
	reserve_memory_for(&android_pmem_adsp_pdata);
	reserve_memory_for(&android_pmem_smipool_pdata);
	reserve_memory_for(&android_pmem_pdata);
#endif
#if !defined (CONFIG_MSM_MULTIMEDIA_USE_ION) || defined (CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_JPN_MODEL_SC_05D)
	reserve_memory_for(&android_pmem_audio_pdata);
#endif
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
#if defined (CONFIG_SEC_KERNEL_REBASE_FOR_PMEM_OPTIMIZATION)
	if (paddr >= 0x40000000 && paddr < 0x80000000)
#else
	if (paddr >= 0x40000000 && paddr < 0x60000000)
#endif
		return MEMTYPE_EBI1;
	if (paddr >= 0x38000000 && paddr < 0x40000000)
		return MEMTYPE_SMI;
	return MEMTYPE_NONE;
}

static struct reserve_info msm8x60_reserve_info __initdata = {
	.memtype_reserve_table = msm8x60_reserve_table,
	.calculate_reserve_sizes = msm8x60_calculate_reserve_sizes,
	.paddr_to_memtype = msm8x60_paddr_to_memtype,
};

static char prim_panel_name[PANEL_NAME_MAX_LEN];
static char ext_panel_name[PANEL_NAME_MAX_LEN];
static int __init prim_display_setup(char *param)
{
	if (strnlen(param, PANEL_NAME_MAX_LEN))
		strlcpy(prim_panel_name, param, PANEL_NAME_MAX_LEN);
	return 0;
}
early_param("prim_display", prim_display_setup);

static int __init ext_display_setup(char *param)
{
	if (strnlen(param, PANEL_NAME_MAX_LEN))
		strlcpy(ext_panel_name, param, PANEL_NAME_MAX_LEN);
	return 0;
}
early_param("ext_display", ext_display_setup);

static void __init msm8x60_reserve(void)
{
	msm8x60_set_display_params(prim_panel_name, ext_panel_name);
	reserve_info = &msm8x60_reserve_info;
	msm_reserve();

#ifdef CONFIG_ANDROID_RAM_CONSOLE
	if (memblock_remove(RAM_CONSOLE_START, RAM_CONSOLE_SIZE) == 0) {
		ram_console_resource[0].start = RAM_CONSOLE_START;
		ram_console_resource[0].end = RAM_CONSOLE_START+RAM_CONSOLE_SIZE-1;
	}
#endif
}

#define EXT_CHG_VALID_MPP 10
#define EXT_CHG_VALID_MPP_2 11

#if defined(CONFIG_SMB137B_CHARGER) || defined(CONFIG_SMB137B_CHARGER_MODULE)
static struct pm8xxx_mpp_init_info isl_mpp[] = {
	PM8058_MPP_INIT(EXT_CHG_VALID_MPP, D_INPUT,
		PM8058_MPP_DIG_LEVEL_S3, DIN_TO_INT),
	PM8058_MPP_INIT(EXT_CHG_VALID_MPP_2, D_BI_DIR,
		PM8058_MPP_DIG_LEVEL_S3, BI_PULLUP_10KOHM),
};
#endif // #if defined(CONFIG_SMB137B_CHARGER) || defined(CONFIG_SMB137B_CHARGER_MODULE)

#ifdef CONFIG_ISL9519_CHARGER
static int isl_detection_setup(void)
{
	int ret = 0, i;

	for (i = 0; i < ARRAY_SIZE(isl_mpp); i++) {
		ret = pm8xxx_mpp_config(isl_mpp[i].mpp,
					&isl_mpp[i].config);
		if (ret) {
			pr_err("%s: Config MPP %d of PM8058 failed\n",
						 __func__, isl_mpp[i].mpp);
			return ret;
		}
	}

	return ret;
}

static struct isl_platform_data isl_data __initdata = {
	.chgcurrent		= 700,
	.valid_n_gpio		= PM8058_MPP_PM_TO_SYS(10),
	.chg_detection_config	= isl_detection_setup,
	.max_system_voltage	= 4200,
	.min_system_voltage	= 3200,
	.term_current		= 120,
	.input_current		= 2048,
};

static struct i2c_board_info isl_charger_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO("isl9519q", 0x9),
		.irq = PM8058_IRQ_BASE + PM8058_CBLPWR_IRQ,
		.platform_data = &isl_data,
	},
};
#endif

#if defined(CONFIG_SMB137B_CHARGER) || defined(CONFIG_SMB137B_CHARGER_MODULE)
static int smb137b_detection_setup(void)
{
	int ret = 0, i;

	for (i = 0; i < ARRAY_SIZE(isl_mpp); i++) {
		ret = pm8xxx_mpp_config(isl_mpp[i].mpp,
					&isl_mpp[i].config);
		if (ret) {
			pr_err("%s: Config MPP %d of PM8058 failed\n",
						 __func__, isl_mpp[i].mpp);
			return ret;
		}
	}

	return ret;
}

static struct smb137b_platform_data smb137b_data __initdata = {
	.chg_detection_config = smb137b_detection_setup,
	.valid_n_gpio = PM8058_MPP_PM_TO_SYS(10),
	.batt_mah_rating = 950,
};

static struct i2c_board_info smb137b_charger_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO("smb137b", 0x08),
		.irq = PM8058_IRQ_BASE + PM8058_CBLPWR_IRQ,
		.platform_data = &smb137b_data,
	},
};
#endif

#if defined(CONFIG_PN544_NFC)
static struct pn544_i2c_platform_data pn544_pdata __initdata = {
	.irq_gpio = PMIC_GPIO_NFC_IRQ,
	.ven_gpio = PMIC_GPIO_NFC_EN,
	.firm_gpio = GPIO_NFC_FIRM,
};

static struct i2c_board_info pn544_i2c_info[] __initdata = {
	{
		I2C_BOARD_INFO("pn544", 0x2b),
		.irq = PM8058_GPIO_IRQ(PM8058_IRQ_BASE, PM8058_GPIO(8)),
		.platform_data = &pn544_pdata,
	},
};

static struct msm_gpio nfc_init_gpio_table[] = {
	{ GPIO_CFG(GPIO_NFC_FIRM, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "nfc_firm" },
};

static void config_gpio_table_for_nfc(void)
{
    int i;

    for( i=0; i<ARRAY_SIZE(nfc_init_gpio_table); i++) {
            gpio_tlmm_config(nfc_init_gpio_table[i].gpio_cfg, GPIO_CFG_ENABLE);
    }

	return;
}
#endif

#if defined(CONFIG_VIDEO_MHL_V1) || defined(CONFIG_VIDEO_MHL_V2)
/*MHL pin configuration*/
	struct pm_gpio mhl_sel = {
		.direction		= PM_GPIO_DIR_OUT,
		.pull			= PM_GPIO_PULL_NO,
		.vin_sel		= 4,
		.function		= PM_GPIO_FUNC_NORMAL,
		.inv_int_pol	= 0,
	};
	struct pm_gpio mhl_wake_up = {
		.direction		= PM_GPIO_DIR_OUT,
		.pull			= PM_GPIO_PULL_DN,
		.vin_sel		= 2,
		.function		= PM_GPIO_FUNC_NORMAL,
		.inv_int_pol	= 0,
	};
	struct pm_gpio mhl_int = {
		.direction		= PM_GPIO_DIR_IN,
		.pull			= PM_GPIO_PULL_NO,
		.vin_sel		= 2,
		.function		= PM_GPIO_FUNC_NORMAL,
		.inv_int_pol	= 0,
	};
	struct pm_gpio mhl_rst = {
		.direction		= PM_GPIO_DIR_OUT,
		.pull			= PM_GPIO_PULL_NO,
		.vin_sel		= 4,
		.function		= PM_GPIO_FUNC_NORMAL,
		.inv_int_pol	= 0,
	};
/*
    struct pm_gpio hdmi_en= {
	    .direction		= PM_GPIO_DIR_OUT,
	    .pull			= PM_GPIO_PULL_NO,
	    .vin_sel		= 4,
	    .function		= PM_GPIO_FUNC_NORMAL,
	    .inv_int_pol	= 0,
    };
*/
#endif


#ifdef CONFIG_PMIC8058 /* ~1200 lines segment */
#define PMIC_GPIO_SDC3_DET 22
#define PMIC_GPIO_TOUCH_DISC_INTR 5

#ifdef CONFIG_OPTICAL_GP2A
static struct pm_gpio ps_vout = {
	.direction 	 = PM_GPIO_DIR_IN,
	.pull			 = PM_GPIO_PULL_NO,
	.vin_sel		 = 2,
	.function		 = PM_GPIO_FUNC_NORMAL,
	.inv_int_pol	 = 0,
	};
#endif
#ifdef CONFIG_SENSORS_AK8975
static struct pm_gpio msense_nRST = {
	.direction      = PM_GPIO_DIR_IN,
	.pull           = PM_GPIO_PULL_NO,
	.vin_sel        = 2,
	.function       = PM_GPIO_FUNC_NORMAL,
	.inv_int_pol    = 0,
};
#endif
#ifdef CONFIG_GYRO_K3G
static struct pm_gpio gyro_int = {
	.direction      = PM_GPIO_DIR_IN,
	.pull           = PM_GPIO_PULL_DN,
	.vin_sel        = 2,
	.function       = PM_GPIO_FUNC_NORMAL,
	.inv_int_pol    = 0,
};
#endif
#ifdef CONFIG_SENSORS_AK8975
static struct pm_gpio accel_int = {
	.direction      = PM_GPIO_DIR_IN,
	.pull           = PM_GPIO_PULL_DN,
	.vin_sel        = 2,
	.function       = PM_GPIO_FUNC_NORMAL,
	.inv_int_pol    = 0,
};
#endif
#if defined(CONFIG_OPTICAL_TAOS)
static struct pm_gpio als_int = {		
	.direction	 = PM_GPIO_DIR_IN,		
	.pull			 = PM_GPIO_PULL_NO,		
	.vin_sel		 = 2,		
	.function		 = PM_GPIO_FUNC_NORMAL,		
	.inv_int_pol	 = 0,	
};
#endif

#if defined (CONFIG_SAMSUNG_JACK) || defined (CONFIG_SAMSUNG_EARJACK)
#if defined (CONFIG_KOR_MODEL_SHV_E110S)
static struct pm_gpio ear_det_new = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = PM8058_GPIO_VIN_L5,//PM_GPIO_VIN_L5,//1.8v -> 2.85v  : rev >= 05
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol    = 0,
};
#elif defined (CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined(CONFIG_JPN_MODEL_SC_05D)
static struct pm_gpio ear_det_new = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = PM8058_GPIO_VIN_L5,//PM_GPIO_VIN_L5,//1.8v -> 2.85v  : rev >= 05
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol    = 0,
};

#if defined (CONFIG_JPN_MODEL_SC_05D)
static struct pm_gpio ear_send_end = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = PM8058_GPIO_VIN_L5,
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol    = 0,
};

static struct pm_gpio ear_det = {
	.direction      = PM_GPIO_DIR_IN,
	.pull           = PM_GPIO_PULL_NO,
	.vin_sel        = 2,
	.function       = PM_GPIO_FUNC_NORMAL,
	.inv_int_pol    = 0,
};
#endif

#else
static struct pm_gpio ear_det = {
	.direction      = PM_GPIO_DIR_IN,
	.pull           = PM_GPIO_PULL_NO,
	.vin_sel        = 2,
	.function       = PM_GPIO_FUNC_NORMAL,
	.inv_int_pol    = 0,
};
#endif
static struct pm_gpio short_sendend = {
	.direction      = PM_GPIO_DIR_IN,
	.pull           = PM_GPIO_PULL_NO,
	.vin_sel        = 2,
	.function       = PM_GPIO_FUNC_NORMAL,
	.inv_int_pol    = 0,
};

static struct pm_gpio ear_micbiase = {
	.direction      = PM_GPIO_DIR_OUT,
	.pull           = PM_GPIO_PULL_NO,
	.out_strength   = PM_GPIO_STRENGTH_HIGH,
	.function       = PM_GPIO_FUNC_NORMAL,
	.inv_int_pol    = 0,
	.vin_sel        = 2,
	.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
	.output_value   = 0,
};
#endif

#if defined (CONFIG_KOR_MODEL_SHV_E160L)	// add EAR_VOL_KEY 
static struct pm_gpio short_sendend_state = {
	.direction      = PM_GPIO_DIR_IN,
	.pull           = PM_GPIO_PULL_NO,
	.vin_sel        = 6,
	.function       = PM_GPIO_FUNC_NORMAL,
	.inv_int_pol    = 0,
};
static struct pm_gpio ear_detect = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = 8, //PM_GPIO_VIN_L5,//1.8v -> 2.85v  : rev >= 05
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol    = 0,
};
#endif

#ifdef CONFIG_USA_MODEL_SGH_I717

static struct pm_gpio main_micbiase = {
	.direction      = PM_GPIO_DIR_OUT,
	.pull           = PM_GPIO_PULL_NO,
	.out_strength   = PM_GPIO_STRENGTH_HIGH,
	.function       = PM_GPIO_FUNC_NORMAL,
	.inv_int_pol    = 0,
	.vin_sel        = 2,
	.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
	.output_value   = 0,
};

static struct pm_gpio sub_micbiase = {
	.direction      = PM_GPIO_DIR_OUT,
	.pull           = PM_GPIO_PULL_NO,
	.out_strength   = PM_GPIO_STRENGTH_HIGH,
	.function       = PM_GPIO_FUNC_NORMAL,
	.inv_int_pol    = 0,
	.vin_sel        = 2,
	.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
	.output_value   = 0,
};

#endif
#if defined(CONFIG_CHARGER_SMB328A) || defined(CONFIG_CHARGER_SMB136)
/* not used, set intput */
static struct pm_gpio chg_en = {
	.direction      = PM_GPIO_DIR_IN,
	.pull           = PM_GPIO_PULL_NO,
	.vin_sel        = 2,
	.function       = PM_GPIO_FUNC_NORMAL,
	.inv_int_pol    = 0,
};

/* not used */
static struct pm_gpio chg_stat = {
	.direction      = PM_GPIO_DIR_IN,
	.pull           = PM_GPIO_PULL_NO,
	.vin_sel        = 2,
	.function       = PM_GPIO_FUNC_NORMAL,
	.inv_int_pol    = 0,
};
#endif

static int pm8058_gpios_init(void)
{
	int i;
	int rc;
	struct pm8058_gpio_cfg {
		int                gpio;
		struct pm_gpio	   cfg;
	};

	struct pm8058_gpio_cfg gpio_cfgs[] = {
		{ /* FFA ethernet */
			PM8058_GPIO_PM_TO_SYS(6),
			{
				.direction      = PM_GPIO_DIR_IN,
				.pull           = PM_GPIO_PULL_DN,
				.vin_sel        = 2,
				.function       = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol    = 0,
			},
		},
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
		{ /* core&surf gpio expander */
			PM8058_GPIO_PM_TO_SYS(UI_INT1_N),
			{
				.direction      = PM_GPIO_DIR_IN,
				.pull           = PM_GPIO_PULL_NO,
				.vin_sel        = PM8058_GPIO_VIN_S3,
				.function       = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol    = 0,
			},
		},
		{ /* docking gpio expander */
			PM8058_GPIO_PM_TO_SYS(UI_INT2_N),
			{
				.direction      = PM_GPIO_DIR_IN,
				.pull           = PM_GPIO_PULL_NO,
				.vin_sel        = PM8058_GPIO_VIN_S3,
				.function       = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol    = 0,
			},
		},
		{ /* FHA/keypad gpio expanders */
			PM8058_GPIO_PM_TO_SYS(UI_INT3_N),
			{
				.direction      = PM_GPIO_DIR_IN,
				.pull           = PM_GPIO_PULL_NO,
				.vin_sel        = PM8058_GPIO_VIN_S3,
				.function       = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol    = 0,
			},
		},
		{ /* Timpani Reset */
			PM8058_GPIO_PM_TO_SYS(20),
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
		{
			PM8058_GPIO_PM_TO_SYS(16),
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
		{ /* PMIC ID interrupt */
			PM8058_GPIO_PM_TO_SYS(36),
			{
				.direction	= PM_GPIO_DIR_IN,
				.pull		= PM_GPIO_PULL_NO,
				.function	= PM_GPIO_FUNC_NORMAL,
				.vin_sel	= 2,
				.inv_int_pol	= 0,
			}
		},
#if defined(CONFIG_PN544_NFC)
		{
			PM8058_GPIO_PM_TO_SYS(7),
			{
				.direction	= PM_GPIO_DIR_IN,
				.pull			= PM_GPIO_PULL_DN,
				.vin_sel		= 2,
				.function		= PM_GPIO_FUNC_NORMAL,
				.inv_int_pol	= 0,
			}
		},
		{
			PM8058_GPIO_PM_TO_SYS(29),
			{
				.direction	= PM_GPIO_DIR_OUT,
				.pull			= PM_GPIO_PULL_NO,
				.vin_sel		= 2,
				.function		= PM_GPIO_FUNC_NORMAL,
				.inv_int_pol	= 0,
			}
		},
#endif	
		
	};

#ifdef CONFIG_KEYPAD_CYPRESS_TOUCH
	struct pm_gpio tkey_int = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = 2,
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol    = 0,
	};
#endif
#if defined(CONFIG_TOUCHDISC_VTD518_SHINETSU) || \
		defined(CONFIG_TOUCHDISC_VTD518_SHINETSU_MODULE)
	struct pm_gpio touchdisc_intr_gpio_cfg = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_UP_1P5,
		.vin_sel        = 2,
		.function       = PM_GPIO_FUNC_NORMAL,
	};
#endif

#if defined(CONFIG_HAPTIC_ISA1200) || \
			defined(CONFIG_HAPTIC_ISA1200_MODULE)
	struct pm_gpio en_hap_gpio_cfg = {
		.direction      = PM_GPIO_DIR_OUT,
		.pull           = PM_GPIO_PULL_NO,
		.out_strength   = PM_GPIO_STRENGTH_HIGH,
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol    = 0,
		.vin_sel        = 2,
		.output_buffer  = PM_GPIO_OUT_BUF_CMOS,
		.output_value   = 0,
	};
#endif

#if defined(CONFIG_PMIC8058_OTHC) || defined(CONFIG_PMIC8058_OTHC_MODULE)
	struct pm8058_gpio_cfg line_in_gpio_cfg = {
			PM8058_GPIO_PM_TO_SYS(18),
			{
				.direction	= PM_GPIO_DIR_IN,
				.pull           = PM_GPIO_PULL_UP_1P5,
				.vin_sel        = 2,
				.function       = PM_GPIO_FUNC_NORMAL,
				.inv_int_pol    = 0,
			}
	};
#endif

#if defined(CONFIG_QS_S5K4E1)
		{
			struct pm8058_gpio_cfg qs_hc37_cam_pd_gpio_cfg = {
			PM8058_GPIO_PM_TO_SYS(26),
			{
				.direction		= PM_GPIO_DIR_OUT,
				.output_value	= 0,
				.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
				.pull			= PM_GPIO_PULL_DN,
				.out_strength	= PM_GPIO_STRENGTH_HIGH,
				.function		= PM_GPIO_FUNC_NORMAL,
				.vin_sel		= 2,
				.inv_int_pol	= 0,
			}
		};
#endif
#ifdef CONFIG_FB_MSM_LCDC_NT35582_WVGA
	struct pm8058_gpio_cfg pmic_lcdc_nt35582_gpio_cfg = {
		PM8058_GPIO_PM_TO_SYS(GPIO_NT35582_BL_EN_HW_PIN - 1),
		{
			.direction		= PM_GPIO_DIR_OUT,
			.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
			.output_value	= 1,
			.pull			= PM_GPIO_PULL_UP_30,
			/* 2.9V  PM_GPIO_VIN_L2, which gives 2.6V */
			.vin_sel		= PM8058_GPIO_VIN_L5,
			.out_strength	= PM_GPIO_STRENGTH_HIGH,
			.function		= PM_GPIO_FUNC_NORMAL,
			.inv_int_pol	= 0,
		}
	};
#endif

#if defined (CONFIG_SAMSUNG_JACK) || defined (CONFIG_SAMSUNG_EARJACK)
#if defined (CONFIG_KOR_MODEL_SHV_E110S)
		pr_info("%s PMIC_GPIO_EAR_DET : ear_det_new\n", __func__);
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_DET), &ear_det_new);
#elif  defined (CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
#ifdef CONFIG_KOR_MODEL_SHV_E160L	// add EAR_VOL_KEY 
		if(get_hw_rev() >= 0x08)  {
			pr_info("%s PMIC_GPIO_EAR_DET : ear_detect\n", __func__);
			rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_DET), &ear_detect); 	
		}
		else
#endif	
		{
			pr_info("%s PMIC_GPIO_EAR_DET : ear_det_new\n", __func__);
			rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_DET), &ear_det_new);
#if defined (CONFIG_JPN_MODEL_SC_05D)
			pr_info("%s PMIC_GPIO_EAR_SEND_END: ear_send_end\n", __func__);
			rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_SEND_END), &ear_send_end);
#endif	
		}
#else
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_DET), &ear_det);
#endif
	if (rc) {
		pr_err("%s PMIC_GPIO_EAR_DET config failed\n", __func__);
		return rc;
	}

#ifdef CONFIG_KOR_MODEL_SHV_E160L	// add EAR_VOL_KEY 
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(get_ear_key_gpio()), &short_sendend);
#else
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SHORT_SENDEND), &short_sendend);
#endif
	if (rc) {
		pr_err("%s PMIC_GPIO_SHORT_SENDEND config failed\n", __func__);
		return rc;
	}

#ifdef CONFIG_KOR_MODEL_SHV_E160L	// add EAR_VOL_KEY 
	if(get_hw_rev() >= 0x08)  {
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_SENDEND_STATE), &short_sendend_state);
		if (rc) {
			pr_err("%s PMIC_GPIO_EAR_SENDEND_STATE config failed\n", __func__);
			return rc;
		}
	}
#endif
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_EAR_MICBIAS_EN), &ear_micbiase);
	if (rc) {
		pr_err("%s PMIC_GPIO_EAR_MICBIAS_EN config failed\n", __func__);
		return rc;
	}
pr_err("%s PMIC_GPIO_EAR_DET : OK \n", __func__);
	sec_jack_gpio_init();
#endif /* nested CONFIG_SAMSUNG_JACK */


#if defined (CONFIG_USA_MODEL_SGH_I717)
	if( get_hw_rev() >= 0x03 ){
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MAIN_MICBIAS_EN), &main_micbiase);
		if (rc) {
			pr_err("%s PMIC_GPIO_MAIN_MICBIAS_EN config failed\n", __func__);
			return rc;
		}
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SUB_MICBIAS_EN), &sub_micbiase);
		if (rc) {
			pr_err("%s PMIC_GPIO_SUB_MICBIAS_EN config failed\n", __func__);
			return rc;
		}
		
		rc = gpio_request(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MAIN_MICBIAS_EN), "MAIN_MICBIAS_EN");
		if (rc) {
			pr_err("%s: PMIC_GPIO_MAIN_MICBIAS_EN request failed\n", __func__);
			return rc;
		}
		rc = gpio_request(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SUB_MICBIAS_EN), "SUB_MICBIAS_EN");
		if (rc) {
			pr_err("%s: PMIC_GPIO_SUB_MICBIAS_EN request failed\n", __func__);
			return rc;
		}

	}
	else{
		/* NOT USE PM8058 25, 26 GPIO PIN */
	}
#endif
#ifdef CONFIG_FB_MSM_MIPI_DSI_ESD_REFRESH
#if !defined(CONFIG_JPN_MODEL_SC_05D)
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_ESD_DET), &sec_mipi_esd_det_gpio_cfg);
	if (rc) {
		pr_err("%s PMIC_GPIO_ESD_DET config failed\n", __func__);
		return rc;
	}
#endif
#endif


#if defined(CONFIG_HAPTIC_ISA1200) || \
			defined(CONFIG_HAPTIC_ISA1200_MODULE)
	if (machine_is_msm8x60_fluid()) {
		rc = pm8xxx_gpio_config(
			PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_HAP_ENABLE),
			&en_hap_gpio_cfg);
		if (rc < 0) {
			pr_err("%s: pmic haptics gpio config failed\n",
							__func__);
		}
		rc = pm8xxx_gpio_config(
			PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_HAP_LDO_ENABLE),
			&en_hap_gpio_cfg);
		if (rc < 0) {
			pr_err("%s: pmic haptics ldo gpio config failed\n",
							__func__);
		}

	}
#endif

#if defined(CONFIG_TOUCHDISC_VTD518_SHINETSU) || \
		defined(CONFIG_TOUCHDISC_VTD518_SHINETSU_MODULE)
	if (machine_is_msm8x60_ffa() || machine_is_msm8x60_surf() ||
		machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa()) {
		rc = pm8xxx_gpio_config(
			PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_TOUCH_DISC_INTR),
			&touchdisc_intr_gpio_cfg);
		if (rc < 0) {
			pr_err("%s: Touchdisc interrupt gpio config failed\n",
							__func__);
		}
	}
#endif

#if defined(CONFIG_PMIC8058_OTHC) || defined(CONFIG_PMIC8058_OTHC_MODULE)
	/* Line_in only for 8660 ffa & surf */
	if (machine_is_msm8x60_ffa() || machine_is_msm8x60_surf() ||
		machine_is_msm8x60_fusion() || machine_is_msm8x60_dragon() ||
		machine_is_msm8x60_fusn_ffa()) {
		rc = pm8xxx_gpio_config(line_in_gpio_cfg.gpio,
				&line_in_gpio_cfg.cfg);
		if (rc < 0) {
			pr_err("%s pmic line_in gpio config failed\n",
							__func__);
			return rc;
		}
	}
#endif

#ifdef CONFIG_FB_MSM_LCDC_NT35582_WVGA
	if (machine_is_msm8x60_dragon()) {
		rc = pm8xxx_gpio_config(pmic_lcdc_nt35582_gpio_cfg.gpio,
				&pmic_lcdc_nt35582_gpio_cfg.cfg);
		if (rc < 0) {
			pr_err("%s pmic gpio config failed\n", __func__);
			return rc;
		}
	}
#endif

#if defined(CONFIG_QS_S5K4E1)
		/* qs_cam_hc37_cam_pd only for 8660 fluid qs camera*/
		if (machine_is_msm8x60_fluid()) {
			rc = pm8xxx_gpio_config(qs_hc37_cam_pd_gpio_cfg.gpio,
					&qs_hc37_cam_pd_gpio_cfg.cfg);
			if (rc < 0) {
				pr_err("%s pmic qs_hc37_cam_pd gpio config failed\n",
								__func__);
				return rc;
			}
		}
	}
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

#ifdef CONFIG_KEYPAD_CYPRESS_TOUCH
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_TKEY_INT), &tkey_int);
	if (rc) {
		pr_err("%s PMIC_GPIO_TKEY_INT config failed\n", __func__);
		return rc;
	}
#endif
#if defined(CONFIG_VIDEO_MHL_V1) || defined(CONFIG_VIDEO_MHL_V2)
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_SEL), &mhl_sel);
		if (rc) {
			pr_err("%s PMIC_GPIO_MHL_SEL config failed\n", __func__);
			return rc;
		}
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_RST), &mhl_rst);
		if (rc) {
			pr_err("%s PMIC_GPIO_MHL_RST config failed\n", __func__);
			return rc;
		}
#if defined(CONFIG_KOR_MODEL_SHV_E110S)
	if (get_hw_rev() < 7)
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_9), &mhl_int);
	else
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_31), &mhl_int);
#elif defined(CONFIG_JPN_MODEL_SC_03D)
	if (get_hw_rev() < 5)
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_9), &mhl_int);
	else
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_31), &mhl_int);

#elif defined(CONFIG_EUR_MODEL_GT_I9210)
	if (get_hw_rev() < 0x8)
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_9), &mhl_int);
	else
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_31), &mhl_int);
#elif defined (CONFIG_USA_MODEL_SGH_I727)
		if (get_hw_rev() < 0x0b)
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_9), &mhl_int);
	else
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_31), &mhl_int);
#elif defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)
		if (get_hw_rev() < 0x0e)
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_9), &mhl_int);
	else
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_31), &mhl_int);
#elif defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_USA_MODEL_SGH_I577)
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_31), &mhl_int);
#elif defined (CONFIG_USA_MODEL_SGH_I717)
		if(get_hw_rev() == 1)
			rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_9), &mhl_int);
		else
			rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_31), &mhl_int);
#else
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_INT_9), &mhl_int);
#endif
		if (rc) {
			pr_err("%s PMIC_GPIO_MHL_INT config failed\n", __func__);
			return rc;
		}
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MHL_WAKE_UP), &mhl_wake_up);
	if (rc) {
		pr_err("%s PMIC_GPIO_MHL_WAKE_UP config failed\n", __func__);
		return rc;
	}
#endif
#ifdef CONFIG_OPTICAL_GP2A 
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_PS_VOUT), &ps_vout);
	if (rc) {
		pr_err("%s PMIC_GPIO_PS_VOUT config failed\n", __func__);
		return rc;
	}
#endif
#if defined(CONFIG_OPTICAL_TAOS)	
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_ALS_INT), &als_int);	
	if (rc) {		
		pr_err("%s PMIC_GPIO_ALS_INT config failed\n", __func__);		
		return rc;	
	}
#endif
#ifdef CONFIG_SENSORS_AK8975
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MSENSE_RST), &msense_nRST);
	if (rc) {
		pr_err("%s PMIC_GPIO_MSENSE_RST config failed\n", __func__);
			return rc;
		}
	gpio_set_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MSENSE_RST), 0);	
#endif

#ifdef CONFIG_GYRO_K3G
#if defined (CONFIG_KOR_MODEL_SHV_E110S)
	if( get_hw_rev() >= 0x07 ){
		gpio_tlmm_config(GPIO_CFG(MSM_GPIO_GYRO_FIFO_INT, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
		gpio_set_value_cansleep(MSM_GPIO_GYRO_FIFO_INT, 0);
	}
	else

#endif    
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_GYRO_FIFO_INT), &gyro_int);
	if (rc) {
		pr_err("%s PMIC_GPIO_GYRO_FIFO_INT config failed\n", __func__);
		return rc;
	}
	gpio_set_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_GYRO_FIFO_INT), 0);	
	
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_GYRO_INT), &gyro_int);
	if (rc) {
		pr_err("%s PMIC_GPIO_GYRO_INT config failed\n", __func__);
		return rc;
	}
	gpio_set_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_GYRO_INT), 0);	
#endif // CONFIG_GYRO_K3G

#ifdef CONFIG_SENSORS_AK8975
	{
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_ACCEL_INT), &accel_int);
		if (rc) {
			pr_err("%s PMIC_GPIO_ACCEL_INT config failed\n", __func__);
			return rc;
		}
		gpio_set_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_ACCEL_INT), 0);		
	}
#endif


#if defined(CONFIG_OPTICAL_GP2AP020A00F)
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_ALS_INT), &als_int);
	if (rc) {
		pr_err("%s PMIC_GPIO_ALS_INT config failed\n", __func__);
		return rc;
	}
#endif

#if defined(CONFIG_CHARGER_SMB328A) || defined(CONFIG_CHARGER_SMB136)
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_CHG_EN), &chg_en);
	if (rc) {
		pr_err("%s PMIC_GPIO_CHG_EN config failed\n", __func__);
		return rc;
	}
	
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_CHG_STAT), &chg_stat);
	if (rc) {
		pr_err("%s PMIC_GPIO_CHG_STAT config failed\n", __func__);
		return rc;
	}
#endif

	return 0;
}

static const unsigned int ffa_keymap[] = {
	KEY(0, 0, KEY_SEARCH),
	KEY(0, 1, KEY_BACK),
	KEY(0, 2, KEY_MENU),		
	KEY(0, 3, KEY_HOME),
	KEY(0, 4, KEY_VOLUMEDOWN),
	KEY(0, 5, KEY_VOLUMEUP),	
#if 0
	KEY(0, 0, KEY_FN_F1),	 /* LS - PUSH1 */
	KEY(0, 1, KEY_UP),	 /* NAV - UP */
	KEY(0, 2, KEY_LEFT),	 /* NAV - LEFT */
	KEY(0, 3, KEY_VOLUMEUP), /* Shuttle SW_UP */

	KEY(1, 0, KEY_FN_F2), 	 /* LS - PUSH2 */
	KEY(1, 1, KEY_RIGHT),    /* NAV - RIGHT */
	KEY(1, 2, KEY_DOWN),     /* NAV - DOWN */
	KEY(1, 3, KEY_VOLUMEDOWN),

	KEY(2, 3, KEY_ENTER),     /* SW_PUSH key */

	KEY(4, 0, KEY_CAMERA_FOCUS), /* RS - PUSH1 */
	KEY(4, 1, KEY_UP),	  /* USER_UP */
	KEY(4, 2, KEY_LEFT),	  /* USER_LEFT */
	KEY(4, 3, KEY_HOME),	  /* Right switch: MIC Bd */
	KEY(4, 4, KEY_FN_F3),	  /* Reserved MIC */

	KEY(5, 0, KEY_CAMERA), /* RS - PUSH2 */
	KEY(5, 1, KEY_RIGHT),	  /* USER_RIGHT */
	KEY(5, 2, KEY_DOWN),	  /* USER_DOWN */
	KEY(5, 3, KEY_BACK),	  /* Left switch: MIC */
	KEY(5, 4, KEY_MENU),	  /* Center switch: MIC */
#endif
};

static const unsigned int dragon_keymap[] = {
	KEY(0, 0, KEY_MENU),
	KEY(0, 2, KEY_1),
	KEY(0, 3, KEY_4),
	KEY(0, 4, KEY_7),

	KEY(1, 0, KEY_UP),
	KEY(1, 1, KEY_LEFT),
	KEY(1, 2, KEY_DOWN),
	KEY(1, 3, KEY_5),
	KEY(1, 4, KEY_8),

	KEY(2, 0, KEY_HOME),
	KEY(2, 1, KEY_REPLY),
	KEY(2, 2, KEY_2),
	KEY(2, 3, KEY_6),
	KEY(2, 4, KEY_0),

	KEY(3, 0, KEY_VOLUMEUP),
	KEY(3, 1, KEY_RIGHT),
	KEY(3, 2, KEY_3),
	KEY(3, 3, KEY_9),
	KEY(3, 4, KEY_SWITCHVIDEOMODE),

	KEY(4, 0, KEY_VOLUMEDOWN),
	KEY(4, 1, KEY_BACK),
	KEY(4, 2, KEY_CAMERA),
	KEY(4, 3, KEY_KBDILLUMTOGGLE),
};

static struct matrix_keymap_data ffa_keymap_data = {
	.keymap_size	= ARRAY_SIZE(ffa_keymap),
	.keymap		= ffa_keymap,
};

static struct pm8xxx_keypad_platform_data ffa_keypad_data = {
	.input_name		= "ffa-keypad",
	.input_phys_device	= "ffa-keypad/input0",
	.num_rows		= 1,
	.num_cols		= 6,
#if  defined(CONFIG_KOR_MODEL_SHV_E160L) || defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_JPN_MODEL_SC_05D)
	.rows_gpio_start	= PM8058_GPIO_PM_TO_SYS(8),
#else
	.rows_gpio_start	= PM8058_GPIO_PM_TO_SYS(24),
#endif
	.cols_gpio_start	= PM8058_GPIO_PM_TO_SYS(0),
	.debounce_ms		= 15,
	.scan_delay_ms		= 32,
	.row_hold_ns            = 91500,
	.wakeup			= 1,
	.keymap_data		= &ffa_keymap_data,
};

static struct matrix_keymap_data dragon_keymap_data = {
	.keymap_size = ARRAY_SIZE(dragon_keymap),
	.keymap = dragon_keymap,
};

static struct pm8xxx_keypad_platform_data dragon_keypad_data = {
	.input_name = "dragon-keypad",
	.input_phys_device = "dragon-keypad/input0",
	.num_rows = 6,
	.num_cols = 5,
	.rows_gpio_start	= PM8058_GPIO_PM_TO_SYS(8),
	.cols_gpio_start	= PM8058_GPIO_PM_TO_SYS(0),
	.debounce_ms		= 15,
	.scan_delay_ms = 32,
	.row_hold_ns = 91500,
	.wakeup = 1,
	.keymap_data = &dragon_keymap_data,
};

static const unsigned int fluid_keymap[] = {
	KEY(0, 0, KEY_FN_F1),	 /* LS - PUSH1 */
	KEY(0, 1, KEY_UP),	 /* NAV - UP */
	KEY(0, 2, KEY_LEFT),	 /* NAV - LEFT */
	KEY(0, 3, KEY_VOLUMEDOWN), /* Shuttle SW_UP */

	KEY(1, 0, KEY_FN_F2),	 /* LS - PUSH2 */
	KEY(1, 1, KEY_RIGHT),    /* NAV - RIGHT */
	KEY(1, 2, KEY_DOWN),     /* NAV - DOWN */
	KEY(1, 3, KEY_VOLUMEUP),

	KEY(2, 3, KEY_ENTER),     /* SW_PUSH key */

	KEY(4, 0, KEY_CAMERA_FOCUS), /* RS - PUSH1 */
	KEY(4, 1, KEY_UP),	  /* USER_UP */
	KEY(4, 2, KEY_LEFT),	  /* USER_LEFT */
	KEY(4, 3, KEY_HOME),	  /* Right switch: MIC Bd */
	KEY(4, 4, KEY_FN_F3),	  /* Reserved MIC */

	KEY(5, 0, KEY_CAMERA), /* RS - PUSH2 */
	KEY(5, 1, KEY_RIGHT),	  /* USER_RIGHT */
	KEY(5, 2, KEY_DOWN),	  /* USER_DOWN */
	KEY(5, 3, KEY_BACK),	  /* Left switch: MIC */
	KEY(5, 4, KEY_MENU),	  /* Center switch: MIC */
};

static struct matrix_keymap_data fluid_keymap_data = {
	.keymap_size	= ARRAY_SIZE(fluid_keymap),
	.keymap		= fluid_keymap,
};

static struct pm8xxx_keypad_platform_data fluid_keypad_data = {
	.input_name		= "fluid-keypad",
	.input_phys_device	= "fluid-keypad/input0",
	.num_rows		= 6,
	.num_cols		= 5,
	.rows_gpio_start	= PM8058_GPIO_PM_TO_SYS(8),
	.cols_gpio_start	= PM8058_GPIO_PM_TO_SYS(0),
	.debounce_ms		= 15,
	.scan_delay_ms		= 32,
	.row_hold_ns            = 91500,
	.wakeup			= 1,
	.keymap_data		= &fluid_keymap_data,
};

static struct pm8xxx_vibrator_platform_data pm8058_vib_pdata = {
	.initial_vibrate_ms  = 500,
	.level_mV = 3000,
	.max_timeout_ms = 15000,
};

static struct pm8xxx_rtc_platform_data pm8058_rtc_pdata = {
#if defined(CONFIG_JPN_MODEL_SC_05D)
	.rtc_write_enable       = true,
#else /* if fusion csfb, enable rtc_write */
	.rtc_write_enable       = false,
#endif
	.rtc_alarm_powerup	= false,
};

static struct pm8xxx_pwrkey_platform_data pm8058_pwrkey_pdata = {
	.pull_up		= 1,
	.kpd_trigger_delay_us   = 15625,
	.wakeup			= 1,
};

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
	.othc_highcurr_thresh_uA = 600,
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

#if (defined(CONFIG_SAMSUNG_JACK) || defined (CONFIG_SAMSUNG_EARJACK)) && (defined(CONFIG_TARGET_LOCALE_KOR) || defined (CONFIG_TARGET_LOCALE_USA) || defined(CONFIG_TARGET_LOCALE_JPN))
/* MIC_BIAS1 is configured as normal MIC BIAS */
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

	if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) == 2 ||
		machine_is_msm8x60_fluid() || machine_is_msm8x60_fusion() ||
		machine_is_msm8x60_fusn_ffa()) {
		/* 3-switch headset supported only by V2 FFA and FLUID */
		hsed_config_1.accessories_adc_support = true,
		/* ADC based accessory detection works only on V2 and FLUID */
		hsed_config_1.accessories_adc_channel = CHANNEL_ADC_HDSET,
		hsed_config_1.othc_support_n_switch = true;
	}

	/* IR GPIO is absent on FLUID */
	if (machine_is_msm8x60_fluid())
		hsed_config_1.ir_gpio = -1;

	for (i = 0; i < ARRAY_SIZE(othc_accessories); i++) {
		if (machine_is_msm8x60_fluid()) {
			switch (othc_accessories[i].accessory) {
			case OTHC_ANC_HEADPHONE:
			case OTHC_ANC_HEADSET:
				othc_accessories[i].gpio = GPIO_HEADSET_DET_N;
				break;
			case OTHC_MICROPHONE:
				othc_accessories[i].enabled = false;
				break;
			case OTHC_SVIDEO_OUT:
				othc_accessories[i].enabled = true;
				hsed_config_1.video_out_gpio = GPIO_HS_SW_DIR;
				break;
			}
		}
	}
}


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

static struct pmic8058_led pmic8058_dragon_leds[] = {
	[0] = {
		/* RED */
		.name		= "led_drv0",
		.max_brightness = 15,
		.id		= PMIC8058_ID_LED_0,
	},/* 300 mA flash led0 drv sink */
	[1] = {
		/* Yellow */
		.name		= "led_drv1",
		.max_brightness = 15,
		.id		= PMIC8058_ID_LED_1,
	},/* 300 mA flash led0 drv sink */
	[2] = {
		/* Green */
		.name		= "led_drv2",
		.max_brightness = 15,
		.id		= PMIC8058_ID_LED_2,
	},/* 300 mA flash led0 drv sink */
	[3] = {
		.name		= "led_psensor",
		.max_brightness = 15,
		.id		= PMIC8058_ID_LED_KB_LIGHT,
	},/* 300 mA flash led0 drv sink */
};

static struct pmic8058_leds_platform_data pm8058_dragon_leds_data = {
	.num_leds = ARRAY_SIZE(pmic8058_dragon_leds),
	.leds	= pmic8058_dragon_leds,
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

static struct pmic8058_charger_data pmic8058_charger_dragon = {
		.charger_data_valid = true,
		.max_source_current = 1800,
		.charger_type = CHG_TYPE_AC,
};

static struct pmic8058_charger_data pmic8058_charger_ffa_surf = {
		.charger_data_valid = false,
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
	.pwrkey_pdata		= &pm8058_pwrkey_pdata,
	.othc0_pdata		= &othc_config_pdata_0,
	.othc1_pdata		= &othc_config_pdata_1,
	.othc2_pdata		= &othc_config_pdata_2,
	.pwm_pdata		= &pm8058_pwm_data,
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
#endif  /* CONFIG_PMIC8058 ~1200 lines segment */


#if defined(CONFIG_TOUCHDISC_VTD518_SHINETSU) || \
		defined(CONFIG_TOUCHDISC_VTD518_SHINETSU_MODULE)
#define TDISC_I2C_SLAVE_ADDR	0x67
#define PMIC_GPIO_TDISC		PM8058_GPIO_PM_TO_SYS(5)
#define TDISC_INT		PM8058_GPIO_IRQ(PM8058_IRQ_BASE, 5)

static const char *vregs_tdisc_name[] = {
	"8058_l5",
	"8058_s3",
};

static const int vregs_tdisc_val[] = {
	2850000,/* uV */
	1800000,
};
static struct regulator *vregs_tdisc[ARRAY_SIZE(vregs_tdisc_name)];

static int tdisc_shinetsu_setup(void)
{
	int rc, i;

	rc = gpio_request(PMIC_GPIO_TDISC, "tdisc_interrupt");
	if (rc) {
		pr_err("%s: gpio_request failed for PMIC_GPIO_TDISC\n",
								__func__);
		return rc;
	}

	rc = gpio_request(GPIO_JOYSTICK_EN, "tdisc_oe");
	if (rc) {
		pr_err("%s: gpio_request failed for GPIO_JOYSTICK_EN\n",
							__func__);
		goto fail_gpio_oe;
	}

	rc = gpio_direction_output(GPIO_JOYSTICK_EN, 1);
	if (rc) {
		pr_err("%s: gpio_direction_output failed for GPIO_JOYSTICK_EN\n",
								__func__);
		gpio_free(GPIO_JOYSTICK_EN);
		goto fail_gpio_oe;
	}

	for (i = 0; i < ARRAY_SIZE(vregs_tdisc_name); i++) {
		vregs_tdisc[i] = regulator_get(NULL, vregs_tdisc_name[i]);
		if (IS_ERR(vregs_tdisc[i])) {
			printk(KERN_ERR "%s: regulator get %s failed (%ld)\n",
				__func__, vregs_tdisc_name[i],
				PTR_ERR(vregs_tdisc[i]));
			rc = PTR_ERR(vregs_tdisc[i]);
			goto vreg_get_fail;
		}

		rc = regulator_set_voltage(vregs_tdisc[i],
				vregs_tdisc_val[i], vregs_tdisc_val[i]);
		if (rc) {
			printk(KERN_ERR "%s: regulator_set_voltage() = %d\n",
				__func__, rc);
			goto vreg_set_voltage_fail;
		}
	}

	return rc;
vreg_set_voltage_fail:
	i++;
vreg_get_fail:
	while (i)
		regulator_put(vregs_tdisc[--i]);
fail_gpio_oe:
	gpio_free(PMIC_GPIO_TDISC);
	return rc;
}

static void tdisc_shinetsu_release(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vregs_tdisc_name); i++)
		regulator_put(vregs_tdisc[i]);

	gpio_free(PMIC_GPIO_TDISC);
	gpio_free(GPIO_JOYSTICK_EN);
}

static int tdisc_shinetsu_enable(void)
{
	int i, rc = -EINVAL;

	for (i = 0; i < ARRAY_SIZE(vregs_tdisc_name); i++) {
		rc = regulator_enable(vregs_tdisc[i]);
		if (rc < 0) {
			printk(KERN_ERR "%s: vreg %s enable failed (%d)\n",
				__func__, vregs_tdisc_name[i], rc);
			goto vreg_fail;
		}
	}

	/* Enable the OE (output enable) gpio */
	gpio_set_value_cansleep(GPIO_JOYSTICK_EN, 1);
	/* voltage and gpio stabilization delay */
	msleep(50);

	return 0;
vreg_fail:
	while (i)
		regulator_disable(vregs_tdisc[--i]);
	return rc;
}

static int tdisc_shinetsu_disable(void)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(vregs_tdisc_name); i++) {
		rc = regulator_disable(vregs_tdisc[i]);
		if (rc < 0) {
			printk(KERN_ERR "%s: vreg %s disable failed (%d)\n",
				__func__, vregs_tdisc_name[i], rc);
			goto tdisc_reg_fail;
		}
	}

	/* Disable the OE (output enable) gpio */
	gpio_set_value_cansleep(GPIO_JOYSTICK_EN, 0);

	return 0;

tdisc_reg_fail:
	while (i)
		regulator_enable(vregs_tdisc[--i]);
	return rc;
}

static struct tdisc_abs_values tdisc_abs = {
	.x_max = 32,
	.y_max = 32,
	.x_min = -32,
	.y_min = -32,
	.pressure_max = 32,
	.pressure_min = 0,
};

static struct tdisc_platform_data tdisc_data = {
	.tdisc_setup = tdisc_shinetsu_setup,
	.tdisc_release = tdisc_shinetsu_release,
	.tdisc_enable = tdisc_shinetsu_enable,
	.tdisc_disable = tdisc_shinetsu_disable,
	.tdisc_wakeup  = 0,
	.tdisc_gpio = PMIC_GPIO_TDISC,
	.tdisc_report_keys = true,
	.tdisc_report_relative = true,
	.tdisc_report_absolute = false,
	.tdisc_report_wheel = false,
	.tdisc_reverse_x = false,
	.tdisc_reverse_y = true,
	.tdisc_abs  = &tdisc_abs,
};

static struct i2c_board_info msm_i2c_gsbi3_tdisc_info[] = {
	{
		I2C_BOARD_INFO("vtd518", TDISC_I2C_SLAVE_ADDR),
		.irq =  TDISC_INT,
		.platform_data = &tdisc_data,
	},
};
#endif /* CONFIG_TOUCHDISC_VTD518_SHINETSU */


#define PM_GPIO_CDC_RST_N 20
#define GPIO_CDC_RST_N PM8058_GPIO_PM_TO_SYS(PM_GPIO_CDC_RST_N)

static struct regulator *vreg_timpani_1;
static struct regulator *vreg_timpani_2;

static unsigned int msm_timpani_setup_power(void)
{
	int rc;

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

#ifdef CONFIG_VP_A2220
void __ref init_a2220(void)
{
#ifdef CONFIG_BATTERY_SEC
	if(is_lpm_boot)
		return ;
#endif
#if defined(CONFIG_USA_MODEL_SGH_I577)
        if (get_hw_rev() <= 0x0D) { /* HW Rev 0.0 */
                a2220_data.bypass = A2220_BYPASS_ALL;
                a2220_data.use_cases[A2220_PATH_INCALL_RECEIVER_NSON] = 0;
                a2220_data.use_cases[A2220_PATH_INCALL_RECEIVER_NSOFF] = 0;
                a2220_data.use_cases[A2220_PATH_INCALL_SPEAKER] = 0;
                a2220_data.use_cases[A2220_PATH_BYPASS_MULTIMEDIA] = 0;
        }
#endif

#ifdef CONFIG_USE_A2220_B
	a2220_ctrl(A2220_BOOTUP_INIT , 0);
#else
	a2220_ioctl2(A2220_BOOTUP_INIT , 0);
#endif
	printk("init_a2220 - \n");
}
#endif
static struct marimba_codec_platform_data timpani_codec_pdata = {
	.marimba_codec_power =  msm_timpani_codec_power,
#ifdef CONFIG_VP_A2220
	.snddev_profile_init = init_a2220,
#endif
};

#define TIMPANI_SLAVE_ID_CDC_ADDR		0X77
#define TIMPANI_SLAVE_ID_QMEMBIST_ADDR		0X66

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

#ifdef CONFIG_SND_SOC_WM8903
static struct wm8903_platform_data wm8903_pdata = {
	.gpio_cfg[2] = 0x3A8,
};

#define WM8903_I2C_SLAVE_ADDR 0x34
static struct i2c_board_info wm8903_codec_i2c_info[] = {
	{
		I2C_BOARD_INFO("wm8903", WM8903_I2C_SLAVE_ADDR >> 1),
		.platform_data = &wm8903_pdata,
	},
};
#endif
#ifdef CONFIG_PMIC8901

#define PM8901_GPIO_INT           91
/*
 * Consumer specific regulator names:
 *			 regulator name		consumer dev_name
 */
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
		.pull_down_enable = 1, \
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

#if defined(CONFIG_MARIMBA_CORE) \
	|| defined(CONFIG_GPIO_SX150X_MODULE)

static struct regulator *vreg_bahama;
static int msm_bahama_sys_rst = GPIO_MS_SYS_RESET_N;

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

	if (machine_is_msm8x60_dragon())
		msm_bahama_sys_rst = GPIO_CDC_RST_N;

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

	rc = gpio_request(msm_bahama_sys_rst, "bahama sys_rst_n");
	if (rc) {
		pr_err("%s: gpio_request %d = %d\n", __func__,
			msm_bahama_sys_rst, rc);
		goto unenable;
	}

	gpio_direction_output(msm_bahama_sys_rst, 0);
	usleep_range(1000, 1050);
	gpio_set_value_cansleep(msm_bahama_sys_rst, 1);
	usleep_range(1000, 1050);
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
		gpio_set_value_cansleep(msm_bahama_sys_rst, 0);
		gpio_free(msm_bahama_sys_rst);
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

struct i2c_registry {
	u8                     machs;
	int                    bus;
	struct i2c_board_info *info;
	int                    len;
};

static struct i2c_registry msm8x60_i2c_devices[] __initdata = {
#if 0 // removed gpio expander, #if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_GSBI8_QUP_I2C_BUS_ID,
		core_expander_i2c_info,
		ARRAY_SIZE(core_expander_i2c_info),
	},
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_GSBI8_QUP_I2C_BUS_ID,
		docking_expander_i2c_info,
		ARRAY_SIZE(docking_expander_i2c_info),
	},
	{
		I2C_SURF,
		MSM_GSBI8_QUP_I2C_BUS_ID,
		surf_expanders_i2c_info,
		ARRAY_SIZE(surf_expanders_i2c_info),
	},
	{
		I2C_SURF | I2C_FFA | I2C_DRAGON,
		MSM_GSBI3_QUP_I2C_BUS_ID,
		fha_expanders_i2c_info,
		ARRAY_SIZE(fha_expanders_i2c_info),
	},
	{
		I2C_FLUID,
		MSM_GSBI3_QUP_I2C_BUS_ID,
		fluid_expanders_i2c_info,
		ARRAY_SIZE(fluid_expanders_i2c_info),
	},
	{
		I2C_FLUID,
		MSM_GSBI8_QUP_I2C_BUS_ID,
		fluid_core_expander_i2c_info,
		ARRAY_SIZE(fluid_core_expander_i2c_info),
	},
#endif
#if defined(CONFIG_VIDEO_MHL_V1) || defined(CONFIG_VIDEO_MHL_V2)
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_GSBI8_QUP_I2C_BUS_ID,
		mhl_i2c_borad_info,
		ARRAY_SIZE(mhl_i2c_borad_info),
	},
#endif

#ifdef CONFIG_KEYPAD_CYPRESS_TOUCH
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_TKEY_I2C_BUS_ID,
		tkey_i2c_devices,
		ARRAY_SIZE(tkey_i2c_devices),
	},
#endif

#ifdef CONFIG_VP_A2220
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_A2220_I2C_BUS_ID,
		rev1_i2c_a2220_devices,
		ARRAY_SIZE(rev1_i2c_a2220_devices),
	},
#endif
#if defined (CONFIG_OPTICAL_GP2A)
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_OPT_I2C_BUS_ID,
		opt_i2c_borad_info,
		ARRAY_SIZE(opt_i2c_borad_info),
	},
#endif
#if defined(CONFIG_BATTERY_MAX17040) || defined(CONFIG_CHARGER_SMB328A)
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_FG_SMB_I2C_BUS_ID,
		fg_smb_i2c_devices,
		ARRAY_SIZE(fg_smb_i2c_devices),
	},
#endif
#ifdef CONFIG_SENSORS_YDA165
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_AMP_I2C_BUS_ID,
		yamahaamp_boardinfo,
		ARRAY_SIZE(yamahaamp_boardinfo),
	},
#endif
#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT540E)
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_GSBI3_QUP_I2C_BUS_ID,
		mxt540e_board_info,
		ARRAY_SIZE(mxt540e_board_info),
	},
#endif /* defined(CONFIG_TOUCHSCREEN_ATMEL_MXT540E) */
#ifdef CONFIG_KEYPAD_CYPRESS_TOUCH
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_TKEY_I2C_BUS_ID,
		tkey_i2c_devices,
		ARRAY_SIZE(tkey_i2c_devices),
	},
#endif
#ifdef CONFIG_MSM_CAMERA
#ifndef CONFIG_MSM_CAMERA_V4L2
	{
		I2C_SURF | I2C_FFA | I2C_FLUID ,
		MSM_GSBI4_QUP_I2C_BUS_ID,
		msm_camera_boardinfo,
		ARRAY_SIZE(msm_camera_boardinfo),
	},
	{
		I2C_DRAGON,
		MSM_GSBI4_QUP_I2C_BUS_ID,
		msm_camera_dragon_boardinfo,
		ARRAY_SIZE(msm_camera_dragon_boardinfo),
	},
#endif
#endif
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_GSBI7_QUP_I2C_BUS_ID,
		msm_i2c_gsbi7_timpani_info,
		ARRAY_SIZE(msm_i2c_gsbi7_timpani_info),
	},
#if defined(CONFIG_MARIMBA_CORE)
	{
		I2C_SURF | I2C_FFA | I2C_FLUID | I2C_DRAGON,
		MSM_GSBI7_QUP_I2C_BUS_ID,
		msm_marimba_board_info,
		ARRAY_SIZE(msm_marimba_board_info),
	},
#endif /* CONFIG_MARIMBA_CORE */
#ifdef CONFIG_ISL9519_CHARGER
	{
		I2C_SURF | I2C_FFA,
		MSM_GSBI8_QUP_I2C_BUS_ID,
		isl_charger_i2c_info,
		ARRAY_SIZE(isl_charger_i2c_info),
	},
#endif
#if defined(CONFIG_HAPTIC_ISA1200) || \
		defined(CONFIG_HAPTIC_ISA1200_MODULE)
	{
		I2C_FLUID,
		MSM_GSBI8_QUP_I2C_BUS_ID,
		msm_isa1200_board_info,
		ARRAY_SIZE(msm_isa1200_board_info),
	},
#endif
#if defined(CONFIG_SMB137B_CHARGER) || defined(CONFIG_SMB137B_CHARGER_MODULE)
	{
		I2C_FLUID,
		MSM_GSBI8_QUP_I2C_BUS_ID,
		smb137b_charger_i2c_info,
		ARRAY_SIZE(smb137b_charger_i2c_info),
	},
#endif
#if defined(CONFIG_BATTERY_BQ27520) || \
		defined(CONFIG_BATTERY_BQ27520_MODULE)
	{
		I2C_FLUID,
		MSM_GSBI8_QUP_I2C_BUS_ID,
		msm_bq27520_board_info,
		ARRAY_SIZE(msm_bq27520_board_info),
	},
#endif
#if defined (CONFIG_EPEN_WACOM_G5SP)
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_GSBI11_QUP_I2C_BUS_ID,
		wacom_g5sp_i2c_devices_info,
		ARRAY_SIZE(wacom_g5sp_i2c_devices_info),
	},
#endif
#ifdef CONFIG_USB_SWITCH_FSA9480
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_GSBI12_QUP_I2C_BUS_ID,
		micro_usb_i2c_devices_info,
		ARRAY_SIZE(micro_usb_i2c_devices_info),
	},
#endif
#if defined(CONFIG_PN544_NFC)
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_GSBI10_QUP_I2C_BUS_ID,
		pn544_i2c_info,
		ARRAY_SIZE(pn544_i2c_info),
	},
#endif
#if defined(CONFIG_SND_SOC_WM8903) || defined(CONFIG_SND_SOC_WM8903_MODULE)
	{
		I2C_DRAGON,
		MSM_GSBI8_QUP_I2C_BUS_ID,
		wm8903_codec_i2c_info,
		ARRAY_SIZE(wm8903_codec_i2c_info),
	},
#endif
#if defined (CONFIG_OPTICAL_TAOS)	
	{		
		I2C_SURF | I2C_FFA | I2C_FLUID,		
		MSM_OPT_I2C_BUS_ID,		
		taos_i2c_board_info,		
		ARRAY_SIZE(taos_i2c_board_info),	
	},
#endif
#ifdef CONFIG_GYRO_K3G
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_GYRO_I2C_BUS_ID,
		gyro_i2c_borad_info,
		ARRAY_SIZE(gyro_i2c_borad_info),
	},
#endif
#ifdef CONFIG_SENSORS_AK8975
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_MAG_I2C_BUS_ID,
		akm_i2c_borad_info,
		ARRAY_SIZE(akm_i2c_borad_info),
	},
#endif
	{
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_MOTOR_I2C_BUS_ID,
		motor_i2c_borad_info,
		ARRAY_SIZE(motor_i2c_borad_info),
	},
};
#endif /* CONFIG_I2C */

static void fixup_i2c_configs(void)
{
#ifdef CONFIG_I2C
#if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)
	if (machine_is_msm8x60_surf() || machine_is_msm8x60_fusion())
		sx150x_data[SX150X_CORE].irq_summary =
			PM8058_GPIO_IRQ(PM8058_IRQ_BASE, UI_INT2_N);
	else if (machine_is_msm8x60_ffa() || machine_is_msm8x60_fusn_ffa() ||
		machine_is_msm8x60_dragon())
		sx150x_data[SX150X_CORE].irq_summary =
			PM8058_GPIO_IRQ(PM8058_IRQ_BASE, UI_INT1_N);
	else if (machine_is_msm8x60_fluid())
		sx150x_data[SX150X_CORE_FLUID].irq_summary =
			PM8058_GPIO_IRQ(PM8058_IRQ_BASE, UI_INT1_N);
#endif
#endif
}

static void register_i2c_devices(void)
{
#ifdef CONFIG_I2C
	u8 mach_mask = 0;
	int i;
#ifdef CONFIG_MSM_CAMERA_V4L2
	struct i2c_registry msm8x60_camera_i2c_devices = {
		I2C_SURF | I2C_FFA | I2C_FLUID,
		MSM_GSBI4_QUP_I2C_BUS_ID,
		msm8x60_camera_board_info.board_info,
		msm8x60_camera_board_info.num_i2c_board_info,
	};
#endif

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
		mach_mask = I2C_DRAGON;
	else
		pr_err("unmatched machine ID in register_i2c_devices\n");

	/* Run the array and install devices as appropriate */
	for (i = 0; i < ARRAY_SIZE(msm8x60_i2c_devices); ++i) {
		if (msm8x60_i2c_devices[i].machs & mach_mask)
			i2c_register_board_info(msm8x60_i2c_devices[i].bus,
						msm8x60_i2c_devices[i].info,
						msm8x60_i2c_devices[i].len);
	}
#ifdef CONFIG_MSM_CAMERA_V4L2
	if (msm8x60_camera_i2c_devices.machs & mach_mask)
		i2c_register_board_info(msm8x60_camera_i2c_devices.bus,
			msm8x60_camera_i2c_devices.info,
			msm8x60_camera_i2c_devices.len);
#endif
#endif
}

static void __init msm8x60_init_uart12dm(void)
{
#if !defined(CONFIG_USB_PEHCI_HCD) && !defined(CONFIG_USB_PEHCI_HCD_MODULE)
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
#endif
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

#if defined (CONFIG_TARGET_LOCALE_USA)
	msm_gsbi1_qup_i2c_device.dev.platform_data = &msm_gsbi1_qup_i2c_pdata;
#else
#if defined(CONFIG_SPI_QUP) || defined(CONFIG_SPI_QUP_MODULE)
	msm_gsbi1_qup_spi_device.dev.platform_data = &msm_gsbi1_qup_spi_pdata;
#endif
#endif
	msm_gsbi3_qup_i2c_device.dev.platform_data = &msm_gsbi3_qup_i2c_pdata;
	msm_gsbi4_qup_i2c_device.dev.platform_data = &msm_gsbi4_qup_i2c_pdata;
	msm_gsbi7_qup_i2c_device.dev.platform_data = &msm_gsbi7_qup_i2c_pdata;
	msm_gsbi8_qup_i2c_device.dev.platform_data = &msm_gsbi8_qup_i2c_pdata;
#if defined(CONFIG_PN544_NFC)
	msm_gsbi10_qup_i2c_device.dev.platform_data = &msm_gsbi10_qup_i2c_pdata;
#endif
#if defined (CONFIG_EPEN_WACOM_G5SP)
	msm_gsbi11_qup_i2c_device.dev.platform_data = &msm_gsbi11_qup_i2c_pdata;
#endif

#ifdef CONFIG_MSM_GSBI9_UART
	if (machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa()) {
		/* Setting protocol code to 0x60 for dual UART/I2C in GSBI9 */
		gsbi_mem = ioremap_nocache(MSM_GSBI9_PHYS, 4);
		writel_relaxed(GSBI_DUAL_MODE_CODE, gsbi_mem);
		iounmap(gsbi_mem);
		msm_gsbi9_qup_i2c_pdata.use_gsbi_shared_mode = 1;
	}
#endif
	msm_gsbi9_qup_i2c_device.dev.platform_data = &msm_gsbi9_qup_i2c_pdata;
#if !defined(CONFIG_KOR_MODEL_SHV_E160L)
	msm_gsbi12_qup_i2c_device.dev.platform_data = &msm_gsbi12_qup_i2c_pdata;
#endif
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

	if (machine_is_msm8x60_fluid()) {
#if (defined(CONFIG_USB_EHCI_MSM_72K) && \
	(defined(CONFIG_SMB137B_CHARGER) || \
	 defined(CONFIG_SMB137B_CHARGER_MODULE)))
		msm_otg_pdata.vbus_power = msm_hsusb_smb137b_vbus_power;
#endif
#if !defined(CONFIG_PN544_NFC)
#if defined(CONFIG_SPI_QUP) || defined(CONFIG_SPI_QUP_MODULE)
		msm_gsbi10_qup_spi_device.dev.platform_data =
					&msm_gsbi10_qup_spi_pdata;
#endif
#endif
	}

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
	if (machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa()) {
		msm_device_uart_gsbi9 = msm_add_gsbi9_uart();
		if (IS_ERR(msm_device_uart_gsbi9))
			pr_err("%s(): Failed to create uart gsbi9 device\n",
								__func__);
	}
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
	/* onlyjazz : now sec_debug is ready to run */
#if 0
	sec_getlog_supply_meminfo(meminfo.bank[0].size, meminfo.bank[0].start,
				  meminfo.bank[1].size, meminfo.bank[1].start);
#endif

	sec_getlog_supply_meminfo(0x40000000, 0x40000000, 0x00, 0x00);
#endif
}

/*
 * Most segments of the EBI2 bus are disabled by default.
 */
static void __init msm8x60_init_ebi2(void)
{
	uint32_t ebi2_cfg;
	void *ebi2_cfg_ptr;
	struct clk *mem_clk = clk_get_sys("msm_ebi2", "mem_clk");

	if (IS_ERR(mem_clk)) {
		pr_err("%s: clk_get_sys(%s,%s), failed", __func__,
					"msm_ebi2", "mem_clk");
		return;
	}
	clk_enable(mem_clk);
	clk_put(mem_clk);

	ebi2_cfg_ptr = ioremap_nocache(0x1a100000, sizeof(uint32_t));
	if (ebi2_cfg_ptr != 0) {
		ebi2_cfg = readl_relaxed(ebi2_cfg_ptr);

		if (machine_is_msm8x60_surf() || machine_is_msm8x60_ffa() ||
			machine_is_msm8x60_fluid() ||
			machine_is_msm8x60_dragon())
			ebi2_cfg |= (1 << 4) | (1 << 5); /* CS2, CS3 */
		else if (machine_is_msm8x60_sim())
			ebi2_cfg |= (1 << 4); /* CS2 */
		else if (machine_is_msm8x60_rumi3())
			ebi2_cfg |= (1 << 5); /* CS3 */

		writel_relaxed(ebi2_cfg, ebi2_cfg_ptr);
		iounmap(ebi2_cfg_ptr);
	}

	if (machine_is_msm8x60_surf() || machine_is_msm8x60_ffa() ||
	    machine_is_msm8x60_fluid() || machine_is_msm8x60_dragon()) {
		ebi2_cfg_ptr = ioremap_nocache(0x1a110000, SZ_4K);
		if (ebi2_cfg_ptr != 0) {
			/* EBI2_XMEM_CFG:PWRSAVE_MODE off */
			writel_relaxed(0UL, ebi2_cfg_ptr);

			/* CS2: Delay 9 cycles (140ns@64MHz) between SMSC
			 * LAN9221 Ethernet controller reads and writes.
			 * The lowest 4 bits are the read delay, the next
			 * 4 are the write delay. */
			writel_relaxed(0x031F1C99, ebi2_cfg_ptr + 0x10);
#if defined(CONFIG_USB_PEHCI_HCD) || defined(CONFIG_USB_PEHCI_HCD_MODULE)
			/*
			 * RECOVERY=5, HOLD_WR=1
			 * INIT_LATENCY_WR=1, INIT_LATENCY_RD=1
			 * WAIT_WR=1, WAIT_RD=2
			 */
			writel_relaxed(0x51010112, ebi2_cfg_ptr + 0x14);
			/*
			 * HOLD_RD=1
			 * ADV_OE_RECOVERY=0, ADDR_HOLD_ENA=1
			 */
			writel_relaxed(0x01000020, ebi2_cfg_ptr + 0x34);
#else
			/* EBI2 CS3 muxed address/data,
			* two cyc addr enable */
			writel_relaxed(0xA3030020, ebi2_cfg_ptr + 0x34);

#endif
			iounmap(ebi2_cfg_ptr);
		}
	}
}

static void __init msm8x60_configure_smc91x(void)
{
	if (machine_is_msm8x60_sim()) {

		smc91x_resources[0].start = 0x1b800300;
		smc91x_resources[0].end   = 0x1b8003ff;

		smc91x_resources[1].start = (NR_MSM_IRQS + 40);
		smc91x_resources[1].end   = (NR_MSM_IRQS + 40);

	} else if (machine_is_msm8x60_rumi3()) {

		smc91x_resources[0].start = 0x1d000300;
		smc91x_resources[0].end   = 0x1d0003ff;

		smc91x_resources[1].start = TLMM_MSM_DIR_CONN_IRQ_0;
		smc91x_resources[1].end   = TLMM_MSM_DIR_CONN_IRQ_0;
	}
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
#if defined(CONFIG_KOR_MODEL_SHV_E160L)
static struct msm_sdcc_pad_drv_cfg sdc3_pad_on_drv_cfg[] = {
	{TLMM_HDRV_SDC3_CLK, GPIO_CFG_6MA},
	{TLMM_HDRV_SDC3_CMD, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC3_DATA, GPIO_CFG_8MA}
};
#else
static struct msm_sdcc_pad_drv_cfg sdc3_pad_on_drv_cfg[] = {
	{TLMM_HDRV_SDC3_CLK, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC3_CMD, GPIO_CFG_8MA},
	{TLMM_HDRV_SDC3_DATA, GPIO_CFG_8MA}
};
#endif
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
/* all SDCC controllers require VDD/VCC voltage */
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
	int rc;

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
	int rc;

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

static int msm_sdc3_get_wpswitch(struct device *dev)
{
	struct platform_device *pdev;
	int status;
	pdev = container_of(dev, struct platform_device, dev);

	status = gpio_request(GPIO_SDC_WP, "SD_WP_Switch");
	if (status) {
		pr_err("%s:Failed to request GPIO %d\n",
					__func__, GPIO_SDC_WP);
	} else {
		status = gpio_direction_input(GPIO_SDC_WP);
		if (!status) {
			status = gpio_get_value_cansleep(GPIO_SDC_WP);
			pr_info("%s: WP Status for Slot %d = %d\n",
				 __func__, pdev->id, status);
		}
		gpio_free(GPIO_SDC_WP);
	}
	return status;
}

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
static irqreturn_t msm8x60_multi_sdio_slot_status_irq(int irq, void *dev_id)
{
	int status;

	if (!machine_is_msm8x60_fusion() &&
	    !machine_is_msm8x60_fusn_ffa())
		return IRQ_NONE;

	status = gpio_get_value(MDM2AP_SYNC);
	pr_info("%s: MDM2AP_SYNC Status = %d\n",
		 __func__, status);

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

#ifdef CONFIG_BATTERY_SEC
	if(is_lpm_boot)
		return 0;
#endif

	if (!machine_is_msm8x60_fusion() &&
	    !machine_is_msm8x60_fusn_ffa())
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

#ifdef	CONFIG_MMC_MSM_SDC4_SUPPORT
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
#endif /* CONFIG_MMC_MSM_SDCx_SUPPORT */


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
	.msm_bus_voting_data = &sps_to_ddr_bus_voting_data,
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
#ifdef CONFIG_MSM_SDIO_AL
	.is_sdio_al_client = 1,
#endif
	.msm_bus_voting_data = &sps_to_ddr_bus_voting_data,
};
#endif

#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
static struct mmc_platform_data msm8x60_sdc3_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.translate_vdd  = msm_sdcc_setup_power,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.wpswitch  	= msm_sdc3_get_wpswitch,
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
	.msm_bus_voting_data = &sps_to_ddr_bus_voting_data,
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
	.msm_bus_voting_data = &sps_to_ddr_bus_voting_data,
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
#ifdef CONFIG_MSM_SDIO_AL
	.is_sdio_al_client = 1,
#endif
	.msm_bus_voting_data = &sps_to_ddr_bus_voting_data,
};
#endif

static void __init msm8x60_init_mmc(void)
{
#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	/* SDCC1 : eMMC card connected */
	sdcc_vreg_data[0].vdd_data = &sdcc_vdd_reg_data[0];
	sdcc_vreg_data[0].vdd_data->reg_name = "8901_l5";
	sdcc_vreg_data[0].vdd_data->set_voltage_sup = 1;
	sdcc_vreg_data[0].vdd_data->level = 2850000;
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
#ifdef CONFIG_BATTERY_SEC
	if(is_lpm_boot) {
		pr_info("%s : skip sdcc2 config for lpm boot\n", __func__);
	} else
#endif
	if (machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa()) {
#ifdef CONFIG_MMC_MSM_SDIO_SUPPORT
		msm8x60_sdc2_data.sdiowakeup_irq = gpio_to_irq(144);
		msm_sdcc_setup_gpio(2, 1);
#endif
		msm_add_sdcc(2, &msm8x60_sdc2_data);
	}
#endif
#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
	/* SDCC3 : External card slot connected */
	sdcc_vreg_data[2].vdd_data = &sdcc_vdd_reg_data[2];
	sdcc_vreg_data[2].vdd_data->reg_name = "8058_l14";
	sdcc_vreg_data[2].vdd_data->set_voltage_sup = 1;
	sdcc_vreg_data[2].vdd_data->level = 2850000;
	sdcc_vreg_data[2].vdd_data->always_on = 0;
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
#ifdef CONFIG_BATTERY_SEC
	if(is_lpm_boot) {
		pr_info("%s : skip sdcc5 config for lpm boot\n", __func__);
	} else
#endif
	if (machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa()) {
#ifdef CONFIG_MMC_MSM_SDIO_SUPPORT
		msm8x60_sdc5_data.sdiowakeup_irq = gpio_to_irq(99);
		msm_sdcc_setup_gpio(5, 1);
#endif
		msm_add_sdcc(5, &msm8x60_sdc5_data);
	}
#endif
}


#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT540E)
static void tsp_mxt540e_power_init(void)
{
		int ret;
		int rc;
		struct regulator *l1;
		struct regulator *L4;
	
#ifdef CONFIG_BATTERY_SEC
	if(is_lpm_boot){
				printk("%s: MXT540E Power On skipped by LPM\n", __func__);
		return;
       }
#endif

	{

	rc = gpio_request(TOUCHSCREEN_IRQ, "TOUCHSCREEN_IRQ");
	if (rc) {
		pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
			"TOUCHSCREEN_IRQ", TOUCHSCREEN_IRQ, rc);
		return;
	}
	rc = gpio_request(TSP_SDA, "TSP_SDA");
	if (rc) {
		pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
			"TSP_SDA", TSP_SDA, rc);
		goto error1;
	}
	rc = gpio_request(TSP_SCL, "TSP_SCL");
	if (rc) {
		pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
			"TSP_SCL", TSP_SCL, rc);
		goto error2;
	}

	gpio_direction_output(TOUCHSCREEN_IRQ, 0);
	gpio_direction_output(TSP_SDA, 0);
	gpio_direction_output(TSP_SCL, 0);
	
	gpio_free(TOUCHSCREEN_IRQ);
	gpio_free(TSP_SDA);
	gpio_free(TSP_SCL);	


#if defined(CONFIG_KOR_MODEL_SHV_E160L)
		L4 = regulator_get(NULL, "8058_l3");
#else        
	L4 = regulator_get(NULL, "8901_l4");
#endif
	if (IS_ERR(L4)) {
		rc = PTR_ERR(L4);
		pr_err("%s: L4 get failed (%d)\n",
			   __func__, rc);
		return;
	}
		ret = regulator_set_voltage(L4, 2800000, 2800000);
				
	if (rc) {
		pr_err("%s: L4 set level failed (%d)\n",
			   __func__, rc);
		return;
	}
	if (regulator_is_enabled(L4)) {
		rc = regulator_disable(L4);
			printk(KERN_ERR "[TSP POWER OFF	 L4 2.8 disable\n");
		if (rc) {
				pr_err("%s: L4 vreg enable failed (%d)\n",
				   __func__, rc);
			return;
		}
	}
	regulator_put(L4);

	l1 = regulator_get(NULL, "8901_l1");
	if (IS_ERR(l1)) {
		rc = PTR_ERR(l1);
		pr_err("%s: l1 get failed (%d)\n",
			   __func__, rc);
		return;
	}
		rc = regulator_set_voltage(l1,3300000, 3300000);
	if (rc) {
			printk("%s: debug p1\n", __func__);
		return;
	}
	if (regulator_is_enabled(l1)) {
			rc = regulator_disable(l1);
			printk(KERN_ERR "[TSP POWER OFF	 L1 3.3 disable\n");
	if (rc) {
				pr_err("%s: L1 vreg enable failed (%d)\n",
			   __func__, rc);
		return;
	}
	}
		regulator_put(l1);
	}

//	is_cable_attached = false;


	printk("[TSP] %s: success\n", __func__);

	return;

error2:
	gpio_free(TSP_SDA);
error1:
	gpio_free(TOUCHSCREEN_IRQ);

}
#endif /* CONFIG_TOUCHSCREEN_ATMEL_MXT540E */


#if !defined(CONFIG_GPIO_SX150X) && !defined(CONFIG_GPIO_SX150X_MODULE)
static inline void display_common_power(int on) {}
#if defined(CONFIG_JPN_MODEL_SC_05D)
static inline void setup_display_power(void) {}
#endif
#else
#if !defined(CONFIG_FB_MSM_LCDC_LD9040_WVGA_PANEL) \
	&& !defined(CONFIG_FB_MSM_LCDC_S6E63M0_WVGA_PANEL)\
	&& !defined(CONFIG_FB_MSM_MIPI_S6D6AA0_WXGA_PANEL) \
	&& !defined(CONFIG_FB_MSM_MIPI_S6E8AA0_HD720_PANEL) \
	&& !defined(CONFIG_FB_MSM_MIPI_S6E8AA0_WXGA_Q1_PANEL)

#define _GET_REGULATOR(var, name) do {					\
	if (var == NULL) {						\
		var = regulator_get(NULL, name);			\
		if (IS_ERR(var)) {					\
			pr_err("'%s' regulator not found, rc=%ld\n",	\
				name, PTR_ERR(var));			\
			var = NULL;					\
		}							\
	}								\
} while (0)

static int dsub_regulator(int on)
{
	static struct regulator *dsub_reg;
	static struct regulator *mpp0_reg;
	static int dsub_reg_enabled;
	int rc = 0;

	_GET_REGULATOR(dsub_reg, "8901_l3");
	if (IS_ERR(dsub_reg)) {
		printk(KERN_ERR "%s: failed to get reg 8901_l3 err=%ld",
		       __func__, PTR_ERR(dsub_reg));
		return PTR_ERR(dsub_reg);
	}

	_GET_REGULATOR(mpp0_reg, "8901_mpp0");
	if (IS_ERR(mpp0_reg)) {
		printk(KERN_ERR "%s: failed to get reg 8901_mpp0 err=%ld",
		       __func__, PTR_ERR(mpp0_reg));
		return PTR_ERR(mpp0_reg);
	}

	if (on && !dsub_reg_enabled) {
		rc = regulator_set_voltage(dsub_reg, 3300000, 3300000);
		if (rc) {
			printk(KERN_ERR "%s: failed to set reg 8901_l3 voltage"
			       " err=%d", __func__, rc);
			goto dsub_regulator_err;
		}
		rc = regulator_enable(dsub_reg);
		if (rc) {
			printk(KERN_ERR "%s: failed to enable reg 8901_l3"
			       " err=%d", __func__, rc);
			goto dsub_regulator_err;
		}
		rc = regulator_enable(mpp0_reg);
		if (rc) {
			printk(KERN_ERR "%s: failed to enable reg 8901_mpp0"
			       " err=%d", __func__, rc);
			goto dsub_regulator_err;
		}
		dsub_reg_enabled = 1;
	} else if (!on && dsub_reg_enabled) {
		rc = regulator_disable(dsub_reg);
		if (rc)
			printk(KERN_WARNING "%s: failed to disable reg 8901_l3"
			       " err=%d", __func__, rc);
		rc = regulator_disable(mpp0_reg);
		if (rc)
			printk(KERN_WARNING "%s: failed to disable reg "
			       "8901_mpp0 err=%d", __func__, rc);
		dsub_reg_enabled = 0;
	}

	return rc;

dsub_regulator_err:
	regulator_put(mpp0_reg);
	regulator_put(dsub_reg);
	return rc;
}

static int display_power_on;
static void setup_display_power(void)
{
	if (display_power_on)
		if (lcdc_vga_enabled) {
			dsub_regulator(1);
			gpio_set_value_cansleep(GPIO_LVDS_SHUTDOWN_N, 0);
			gpio_set_value_cansleep(GPIO_BACKLIGHT_EN, 0);
			if (machine_is_msm8x60_ffa() ||
			    machine_is_msm8x60_fusn_ffa())
				gpio_set_value_cansleep(GPIO_DONGLE_PWR_EN, 1);
		} else {
			dsub_regulator(0);
			gpio_set_value_cansleep(GPIO_LVDS_SHUTDOWN_N, 1);
			gpio_set_value_cansleep(GPIO_BACKLIGHT_EN, 1);
			if (machine_is_msm8x60_ffa() ||
			    machine_is_msm8x60_fusn_ffa())
				gpio_set_value_cansleep(GPIO_DONGLE_PWR_EN, 0);
		}
	else {
		dsub_regulator(0);
		if (machine_is_msm8x60_ffa() || machine_is_msm8x60_fusn_ffa())
			gpio_set_value_cansleep(GPIO_DONGLE_PWR_EN, 0);
		/* BACKLIGHT */
		gpio_set_value_cansleep(GPIO_BACKLIGHT_EN, 0);
		/* LVDS */
		gpio_set_value_cansleep(GPIO_LVDS_SHUTDOWN_N, 0);
	}
}

#define _GET_REGULATOR(var, name) do {					\
	if (var == NULL) {						\
		var = regulator_get(NULL, name);			\
		if (IS_ERR(var)) {					\
			pr_err("'%s' regulator not found, rc=%ld\n",	\
				name, PTR_ERR(var));			\
			var = NULL;					\
		}							\
	}								\
} while (0)

#define GPIO_RESX_N (GPIO_EXPANDER_GPIO_BASE + 2)

static void display_common_power(int on)
{
	int rc;
	static struct regulator *display_reg;

	if (machine_is_msm8x60_surf() || machine_is_msm8x60_ffa() ||
	    machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa()) {
		if (on) {
			/* LVDS */
			_GET_REGULATOR(display_reg, "8901_l2");
			if (!display_reg)
				return;
			rc = regulator_set_voltage(display_reg,
				3300000, 3300000);
			if (rc)
				goto out;
			rc = regulator_enable(display_reg);
			if (rc)
				goto out;
			rc = gpio_request(GPIO_LVDS_SHUTDOWN_N,
				"LVDS_STDN_OUT_N");
			if (rc) {
				printk(KERN_ERR "%s: LVDS gpio %d request"
					"failed\n", __func__,
					 GPIO_LVDS_SHUTDOWN_N);
				goto out2;
			}

			/* BACKLIGHT */
			rc = gpio_request(GPIO_BACKLIGHT_EN, "BACKLIGHT_EN");
			if (rc) {
				printk(KERN_ERR "%s: BACKLIGHT gpio %d request"
					"failed\n", __func__,
					 GPIO_BACKLIGHT_EN);
				goto out3;
			}

			if (machine_is_msm8x60_ffa() ||
			    machine_is_msm8x60_fusn_ffa()) {
				rc = gpio_request(GPIO_DONGLE_PWR_EN,
						  "DONGLE_PWR_EN");
				if (rc) {
					printk(KERN_ERR "%s: DONGLE_PWR_EN gpio"
					       " %d request failed\n", __func__,
					       GPIO_DONGLE_PWR_EN);
					goto out4;
				}
			}

			gpio_direction_output(GPIO_LVDS_SHUTDOWN_N, 0);
			gpio_direction_output(GPIO_BACKLIGHT_EN, 0);
			if (machine_is_msm8x60_ffa() ||
			    machine_is_msm8x60_fusn_ffa())
				gpio_direction_output(GPIO_DONGLE_PWR_EN, 0);
			mdelay(20);
			display_power_on = 1;
			setup_display_power();
		} else {
			if (display_power_on) {
				display_power_on = 0;
				setup_display_power();
				mdelay(20);
				if (machine_is_msm8x60_ffa() ||
				    machine_is_msm8x60_fusn_ffa())
					gpio_free(GPIO_DONGLE_PWR_EN);
				goto out4;
			}
		}
	}
#if defined(CONFIG_FB_MSM_LCDC_SAMSUNG_OLED_PT) || \
	defined(CONFIG_FB_MSM_LCDC_AUO_WVGA)
	else if (machine_is_msm8x60_fluid()) {
		static struct regulator *fluid_reg;
		static struct regulator *fluid_reg2;

		if (on) {
			_GET_REGULATOR(fluid_reg, "8901_l2");
			if (!fluid_reg)
				return;
			_GET_REGULATOR(fluid_reg2, "8058_s3");
			if (!fluid_reg2) {
				regulator_put(fluid_reg);
				return;
			}
			rc = gpio_request(GPIO_RESX_N, "RESX_N");
			if (rc) {
				regulator_put(fluid_reg2);
				regulator_put(fluid_reg);
				return;
			}
			regulator_set_voltage(fluid_reg, 2850000, 2850000);
			regulator_set_voltage(fluid_reg2, 1800000, 1800000);
			regulator_enable(fluid_reg);
			regulator_enable(fluid_reg2);
			msleep(20);
			gpio_direction_output(GPIO_RESX_N, 0);
			udelay(10);
			gpio_set_value_cansleep(GPIO_RESX_N, 1);
			display_power_on = 1;
			setup_display_power();
		} else {
			gpio_set_value_cansleep(GPIO_RESX_N, 0);
			gpio_free(GPIO_RESX_N);
			msleep(20);
			regulator_disable(fluid_reg2);
			regulator_disable(fluid_reg);
			regulator_put(fluid_reg2);
			regulator_put(fluid_reg);
			display_power_on = 0;
			setup_display_power();
			fluid_reg = NULL;
			fluid_reg2 = NULL;
		}
	}
#endif
#if defined(CONFIG_FB_MSM_LCDC_NT35582_WVGA)
	else if (machine_is_msm8x60_dragon()) {
		static struct regulator *dragon_reg;
		static struct regulator *dragon_reg2;

		if (on) {
			_GET_REGULATOR(dragon_reg, "8901_l2");
			if (!dragon_reg)
				return;
			_GET_REGULATOR(dragon_reg2, "8058_l16");
			if (!dragon_reg2) {
				regulator_put(dragon_reg);
				dragon_reg = NULL;
				return;
			}

			rc = gpio_request(GPIO_NT35582_BL_EN, "lcdc_bl_en");
			if (rc) {
				pr_err("%s: gpio %d request failed with rc=%d\n",
					__func__, GPIO_NT35582_BL_EN, rc);
				regulator_put(dragon_reg);
				regulator_put(dragon_reg2);
				dragon_reg = NULL;
				dragon_reg2 = NULL;
				return;
			}

			if (gpio_tlmm_config(GPIO_CFG(GPIO_NT35582_RESET, 0,
				GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN,
				GPIO_CFG_16MA), GPIO_CFG_ENABLE)) {
				pr_err("%s: config gpio '%d' failed!\n",
					__func__, GPIO_NT35582_RESET);
				gpio_free(GPIO_NT35582_BL_EN);
				regulator_put(dragon_reg);
				regulator_put(dragon_reg2);
				dragon_reg = NULL;
				dragon_reg2 = NULL;
				return;
			}

			rc = gpio_request(GPIO_NT35582_RESET, "lcdc_reset");
			if (rc) {
				pr_err("%s: unable to request gpio %d (rc=%d)\n",
					__func__, GPIO_NT35582_RESET, rc);
				gpio_free(GPIO_NT35582_BL_EN);
				regulator_put(dragon_reg);
				regulator_put(dragon_reg2);
				dragon_reg = NULL;
				dragon_reg2 = NULL;
				return;
			}

			regulator_set_voltage(dragon_reg, 3300000, 3300000);
			regulator_set_voltage(dragon_reg2, 1800000, 1800000);
			regulator_enable(dragon_reg);
			regulator_enable(dragon_reg2);
			msleep(20);

			gpio_set_value_cansleep(GPIO_NT35582_RESET, 1);
			msleep(20);
			gpio_set_value_cansleep(GPIO_NT35582_RESET, 0);
			msleep(20);
			gpio_set_value_cansleep(GPIO_NT35582_RESET, 1);
			msleep(50);

			gpio_set_value_cansleep(GPIO_NT35582_BL_EN, 1);

			display_power_on = 1;
		} else if ((dragon_reg != NULL) && (dragon_reg2 != NULL)) {
			gpio_free(GPIO_NT35582_RESET);
			gpio_free(GPIO_NT35582_BL_EN);
			regulator_disable(dragon_reg2);
			regulator_disable(dragon_reg);
			regulator_put(dragon_reg2);
			regulator_put(dragon_reg);
			display_power_on = 0;
			dragon_reg = NULL;
			dragon_reg2 = NULL;
		}
	}
#endif
	return;

out4:
	gpio_free(GPIO_BACKLIGHT_EN);
out3:
	gpio_free(GPIO_LVDS_SHUTDOWN_N);
out2:
	regulator_disable(display_reg);
out:
	regulator_put(display_reg);
	display_reg = NULL;
}
#undef _GET_REGULATOR
#else
static inline void display_common_power(int on) {}
static inline void setup_display_power(void) {}
#endif
#endif /* !CONFIG_GPIO_SX150X && !CONFIG_GPIO_SX150X_MODULE */


static int mipi_dsi_panel_power(int on);

#ifdef CONFIG_FB_MSM_MIPI_S6D6AA0_WXGA_PANEL
 
static struct msm_gpio wxga_gpio_config_data[] = {
    { GPIO_CFG(28, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "lcd_reset" },
};
 
static void mipi_S6D6AA0_panel_gpio_init(void)
{
    int i;
    int loop_count= ARRAY_SIZE(wxga_gpio_config_data);
    for( i=0; i<loop_count; i++)
    {
        gpio_tlmm_config(wxga_gpio_config_data[i].gpio_cfg, 1);
        printk("[mipi lcd] : mipi_S6D6AA0_panel_gpio_init\n");
    }
}

static void mipi_S6D6AA0_panel_reset(void)
{
    mipi_S6D6AA0_panel_gpio_init();

    msleep(50);
    gpio_set_value(28, 1);
    msleep(20);
    gpio_set_value(28, 0);
    msleep(20);
    gpio_set_value(28, 1);
    msleep(20);
    printk("[mipi lcd] : mipi_S6D6AA0_panel_reset\n");
}
 
static int mipi_S6D6AA0_panel_power(int enable)
{
    static struct regulator *l19 = NULL;
    int ret;

    printk("[Mipi_LCD] %s:enable:%d\n", __FUNCTION__, enable);

    if(l19 == NULL)
    {			  
        l19 = regulator_get(&rpm_vreg_device[RPM_VREG_ID_PM8058_L19].dev, "8058_l19");
        if (IS_ERR(l19))
        return -1;

        ret = regulator_set_voltage(l19, 3000000, 3000000);
        if (ret) {
            printk("%s: error setting voltage\n", __func__);
        }
    }

    if (enable) {
        ret = regulator_enable(l19);
        if (ret) {
            printk("%s: error enabling regulator\n", __func__);
        }

        usleep(10);
        mipi_S6D6AA0_panel_reset();
        usleep(10);
    } else {
        ret = regulator_disable(l19);
        if (ret) {
            printk("%s: error enabling regulator\n", __func__);
        }				 
    }
    return ret;
}
#endif /* CONFIG_FB_MSM_MIPI_S6D6AA0_WXGA_PANEL */


#if defined(CONFIG_FB_MSM_MIPI_S6E8AA0_HD720_PANEL) || defined(CONFIG_FB_MSM_MIPI_S6E8AA0_WXGA_Q1_PANEL)

#define MIPI_STR "[Mipi_LCD] "
#define LCD_GPIO_RESET	    (28)
#define LCD_GPIO_OLED_ID	(7)

struct pm8058_gpio_cfg {
	int                gpio;
	struct pm_gpio	   cfg;
};

#if defined(CONFIG_USA_MODEL_SGH_I717)
#define LCD_GPIO_2_2V_EN    (142)
#endif 

static struct msm_gpio hd720_gpio_config_data[] = {
	 { GPIO_CFG(LCD_GPIO_RESET, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "lcd_reset" },
};

#if defined(CONFIG_JPN_MODEL_SC_05D)
static int hd720_pm8058_gpio_config_gpio[] = { LCD_GPIO_OLED_ID };
#endif
struct pm8058_gpio_cfg hd720_pm8058_gpio_config_data[] = {
	{
#if defined(CONFIG_JPN_MODEL_SC_05D)
		PM8058_GPIO_PM_TO_SYS(18),
#else
		LCD_GPIO_OLED_ID,
#endif
		{
			.direction	= PM_GPIO_DIR_IN,
#if defined(CONFIG_JPN_MODEL_SC_05D)
			.pull           = PM_GPIO_PULL_UP_1P5,
#else
			.pull           = PM_GPIO_PULL_DN,
#endif
			.vin_sel        = 2,
			.function       = PM_GPIO_FUNC_NORMAL,
			.inv_int_pol    = 0,
		}
	},
};

static void mipi_S6E8AA0_panel_gpio_init(void)
{
        int i;
	int loop_count;
	int rc;
	printk(MIPI_STR "%s\n", __func__);

	loop_count= ARRAY_SIZE(hd720_gpio_config_data);
	for( i=0; i<loop_count; i++)
	{
		rc = gpio_tlmm_config(hd720_gpio_config_data[i].gpio_cfg, 1);
		if (rc) {
			pr_err("%s: gpio_tlmm_config FAIL(%dth) = %d\n", __func__, i, rc);
		}
	}
 
	loop_count= ARRAY_SIZE(hd720_pm8058_gpio_config_data);
	for( i=0; i<loop_count; i++)
	{
		rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(hd720_pm8058_gpio_config_data[i].gpio)),
				&(hd720_pm8058_gpio_config_data[i].cfg));
		if (rc < 0) {
			pr_err("%s: pm8058_gpio_config FAIL(gpio:%d) = %d\n", __func__, hd720_pm8058_gpio_config_data[i].gpio, rc);
		}
	}
#if defined (CONFIG_USA_MODEL_SGH_I717)	
	if (get_hw_rev()>=3)
	{	
		gpio_tlmm_config(GPIO_CFG(LCD_GPIO_2_2V_EN, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);
	}
#endif	
}
 
static void mipi_S6E8AA0_panel_reset_up(void)
{
	gpio_set_value(LCD_GPIO_RESET, 1);
}

static void mipi_S6E8AA0_panel_reset_down(void)
{
	gpio_set_value(LCD_GPIO_RESET, 0);
}
  
static int mipi_S6E8AA0_panel_power(int enable)
{
    static struct regulator *l19 = NULL;
    static struct regulator *l17 = NULL;
    static struct regulator *l3 = NULL;	 
    static struct regulator *l12 = NULL;
    int ret;
#if defined (CONFIG_KOR_SHV_E120L_HD720) || defined(CONFIG_JPN_MODEL_SC_05D)
    int	isDaliLgtRev01 = false;
#endif

    int isUse_LDO3 = false;
    int LDO3_voltage = 2200000;
    int isUse_LDO12 = false;
    int LDO12_voltage = 2200000;
    int isUse_LDO19 = false;
    int LDO19_voltage = 3100000;
    int isUse_LDO17 = false;
    int LDO17_voltage = 2200000;

#if defined (CONFIG_FB_MSM_MIPI_S6E8AA0_WXGA_Q1_PANEL)
#if defined(CONFIG_USA_MODEL_SGH_I717)
	printk( MIPI_STR "%s: Enable=%d, Q1_ATT seq\n", __func__, enable);
	isUse_LDO3 = false;
	isUse_LDO12 = false;
	if (get_hw_rev()>=3)	{
		isUse_LDO17 = 0;
	}
	else	{
		isUse_LDO17 = true;
		LDO17_voltage = 2200000;
	}
	isUse_LDO19 = true;
	LDO19_voltage = 3100000;
#elif defined(CONFIG_KOR_MODEL_SHV_E160L)
	printk( MIPI_STR "%s: Enable=%d, LGT seq\n", __func__, enable);
	isUse_LDO3 = false;
	isUse_LDO12 = true;
	LDO12_voltage = 2100000;
	isUse_LDO19 = true;
	LDO19_voltage = 3100000;
#elif defined(CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_JPN_MODEL_SC_05D)
	printk( MIPI_STR "%s: Enable=%d, SKT seq\n", __func__, enable);
	isUse_LDO3 = false;
	isUse_LDO12 = false;
	isUse_LDO17 = true;
	LDO17_voltage = 2200000;
	isUse_LDO19 = true;
	LDO19_voltage = 3100000;
#elif defined(CONFIG_KOR_MODEL_SHV_E160K)
	printk( MIPI_STR "%s: Enable=%d, KT seq\n", __func__, enable);
	isUse_LDO3 = false;
	isUse_LDO12 = false;
	if (get_hw_rev()>=10){
		isUse_LDO17 = false;
	}
	else
	{	
		isUse_LDO17 = true;
		LDO17_voltage = 2200000;
	}
	isUse_LDO19 = true;
	LDO19_voltage = 3100000;
#else
	printk( MIPI_STR "%s: Enable=%d, seq\n", __func__, enable);
	isUse_LDO3 = false;
	isUse_LDO12 = false;
	isUse_LDO17 = true;
	LDO17_voltage = 2200000;
	isUse_LDO19 = true;
	LDO19_voltage = 3100000;
#endif

#elif defined (CONFIG_KOR_SHV_E120L_HD720)
	 if(get_hw_rev() > 0) {
		printk( MIPI_STR "%s: Enable=%d, LGT Rev01 seq\n", __func__, enable );
	 	isDaliLgtRev01 = true;

		isUse_LDO12 = true;
		LDO12_voltage = 2200000;
	 } else
	 {			  
		printk( MIPI_STR "%s: Enable=%d, LGT Rev00 seq\n", __func__, enable);
	 	isDaliLgtRev01 = false;
		isUse_LDO12 = false;
	 }
	isUse_LDO3 = false;
	isUse_LDO19 = true;
	LDO19_voltage = 3100000;

#elif defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K) || defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_USA_MODEL_SGH_I577)\
 || defined (CONFIG_JPN_MODEL_SC_05D)
	printk( MIPI_STR "%s: Enable=%d, SKT/KT seq\n", __func__, enable);
	isUse_LDO3 = true;
	LDO3_voltage = 2200000;
	isUse_LDO12 = false;
	isUse_LDO19 = true;
	LDO19_voltage = 3100000;
#else
	isUse_LDO3 = true;
	LDO3_voltage = 2200000;
	isUse_LDO12 = true;
	LDO12_voltage = 2200000;
	isUse_LDO19 = true;
	LDO19_voltage = 3100000;
	printk("[Mipi_LCD] %s: Enable=%d, Unknown Model Case\n", __FUNCTION__, enable);
#endif	 

	 if(l3 == NULL && isUse_LDO3)	 {			  
		 l3 = regulator_get(NULL, "8058_l3");
		 if (IS_ERR(l3)) {
		 	printk( MIPI_STR "Fail to get handle LDO3\n" );
					 return -1;
		}
	 
		 ret = regulator_set_voltage(l3, LDO3_voltage, LDO3_voltage);
				 if (ret) {
		 	printk( MIPI_STR "Fail to control LDO3\n" );
		 } else {
		 	printk( MIPI_STR "Use LDO3, volt=%d\n", LDO3_voltage);
		 }
	 }

	 if(l12 == NULL && isUse_LDO12)	 {			  
		 l12 = regulator_get(NULL, "8058_l12");
		 if (IS_ERR(l12)) {
		 	printk( MIPI_STR "Fail to get handle LDO12\n" );
		 return -1;
		}

		 ret = regulator_set_voltage(l12, LDO12_voltage, LDO12_voltage);
		 if (ret) {
		 	printk( MIPI_STR "Fail to control LDO12\n" );
		 } else {
		 	printk( MIPI_STR "Use LDO12, volt=%d\n", LDO12_voltage);
		 }
	 }

	 if(l17 == NULL && isUse_LDO17)	 {			  
		 l17 = regulator_get(NULL, "8058_l17");
		 if (IS_ERR(l17)) {
		 	printk( MIPI_STR "Fail to get handle LDO17\n" );
		 return -1;
		}

		 ret = regulator_set_voltage(l17, LDO17_voltage, LDO17_voltage);
		 if (ret) {
		 	printk( MIPI_STR "Fail to control LDO17\n" );
		 } else {
		 	printk( MIPI_STR "Use LDO17, volt=%d\n", LDO17_voltage);
		 }
	}     

	if(l19 == NULL && isUse_LDO19 )	 {
		 l19 = regulator_get(NULL, "8058_l19");
		 if (IS_ERR(l19)) {
		 	printk( MIPI_STR "Fail to get handle LDO19\n" );
				 return -1;
		}

		 ret = regulator_set_voltage(l19, LDO19_voltage, LDO19_voltage);
			 if (ret) {
		 	printk( MIPI_STR "Fail to control LDO19\n" );
		 } else {
		 	printk( MIPI_STR "Use LDO19, volt=%d\n", LDO19_voltage);
		 }
	}
 	 
	if (enable) {
		if( l3 != NULL ) {
			 ret = regulator_enable(l3);
			 if (ret) {
			 	printk( MIPI_STR "Fail to enable LDO3\n" );
			 }
	        }
	 
		if( l12 != NULL ) {
			 ret = regulator_enable(l12);
                         if (ret) {
			 	printk( MIPI_STR "Fail to enable LDO12\n" );
			 }
		}

		if( l17 != NULL ) {
			 ret = regulator_enable(l17);
			 if (ret) {
			 	printk( MIPI_STR "Fail to enable LDO17\n" );
			 }
		}

#if defined (CONFIG_USA_MODEL_SGH_I717)	
	if (get_hw_rev()>=3)	{
		gpio_set_value(LCD_GPIO_2_2V_EN, 1);		
	}
#endif
		msleep(5);

		if( l19 != NULL ) {
		         ret = regulator_enable(l19);
			 if (ret) {
			 	printk( MIPI_STR "Fail to enable LDO19\n" );
			 }
		}
			 
		 mipi_S6E8AA0_panel_gpio_init();
		 msleep(25);
		 mipi_S6E8AA0_panel_reset_up();
		 msleep(5);

         } else {

		mipi_S6E8AA0_panel_reset_down();
			
		msleep(5);

		if( l19 != NULL ) {
			ret = regulator_disable(l19);
	                if (ret) {
			 	printk( MIPI_STR "Fail to disable LDO19\n" );
			}
		}

		msleep(5);

#if 0 // kyNam_110831_ Do not turn off LDO3 : HW request
		if( l3 != NULL ) {
			ret = regulator_disable(l3);
			if (ret) {
			 	printk( MIPI_STR "Fail to disable LDO3\n" );
			}
		}
#endif 
		if( l12 != NULL ) {
			ret = regulator_disable(l12);
		        if (ret) {
			 	printk( MIPI_STR "Fail to disable LDO12\n" );
			}
		}	
#if !defined(CONFIG_KOR_MODEL_SHV_E160S) && !defined (CONFIG_JPN_MODEL_SC_05D)
                if( l17 != NULL ) {
                        ret = regulator_disable(l17);
                        if (ret) {
                                printk( MIPI_STR "Fail to disable LDO17\n" );
                        }
                }
#endif				
#if defined (CONFIG_USA_MODEL_SGH_I717)	
	if (get_hw_rev()>=3)
	{
		gpio_set_value(LCD_GPIO_2_2V_EN, 0);		
	}
#endif
        
    }
    return ret;
}
#endif /* CONFIG_FB_MSM_MIPI_S6E8AA0_HD720_PANEL || CONFIG_FB_MSM_MIPI_S6E8AA0_WXGA_Q1_PANEL */

#if !defined(CONFIG_JPN_MODEL_SC_05D)
 static int lcdc_LD9040_panel_power(int enable)
{
	static struct regulator *l3 = NULL;
    static struct regulator *l19 = NULL;
	int ret;

	printk("[kmj] %s:enable:%d\n", __FUNCTION__, enable);

    if(l3 == NULL)
    {
        	l3 = regulator_get(NULL, "8058_l3");
        	if (IS_ERR(l3))
        		return -1;

        	ret = regulator_set_voltage(l3, 1800000, 1800000);
        	if (ret) {
        		printk("%s: error setting voltage\n", __func__);
        	}
    }
            
    if(l19 == NULL)
    {            
        	l19 = regulator_get(NULL, "8058_l19");
        	if (IS_ERR(l19))
        		return -1;

        	ret = regulator_set_voltage(l19, 3000000, 3000000);
        	if (ret) {
        		printk("%s: error setting voltage\n", __func__);
        	}
    }
    
	if (enable) {

        	ret = regulator_enable(l3);
        	if (ret) {
        		printk("%s: error enabling regulator\n", __func__);
        	}
        	ret = regulator_enable(l19);
        	if (ret) {
        		printk("%s: error enabling regulator\n", __func__);
        	}        	
	} else {
        	ret = regulator_disable(l3);
        	if (ret) {
        		printk("%s: error enabling regulator\n", __func__);
        	}
        	ret = regulator_disable(l19);
        	if (ret) {
        		printk("%s: error enabling regulator\n", __func__);
        	}        		
        }
        
	return ret;
}

#define LCDC_NUM_GPIO 28
#define LCDC_GPIO_START 0

static void lcdc_samsung_panel_power(int on)
{
	int n, ret = 0;

//	display_common_power(on);
    lcdc_LD9040_panel_power(on);

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
#endif

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
		pr_info("%s(on): success\n", __func__);
	} else {
		rc = regulator_disable(reg_8058_l16);
		if (rc)
			pr_err("'%s' regulator disable failed, rc=%d\n",
				"8058_l16", rc);
		pr_info("%s(off): success\n", __func__);
	}

	prev_on = on;

	return 0;
}

static int hdmi_gpio_config(int on)
{
	int rc = 0;
	static int prev_on;

	if (on == prev_on)
		return 0;

	if (on) {
		rc = gpio_request(170, "HDMI_DDC_CLK");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_CLK", 170, rc);
			goto error1;
		}
		rc = gpio_request(171, "HDMI_DDC_DATA");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_DDC_DATA", 171, rc);
			goto error2;
		}
		rc = gpio_request(172, "HDMI_HPD");
		if (rc) {
			pr_err("'%s'(%d) gpio_request failed, rc=%d\n",
				"HDMI_HPD", 172, rc);
			goto error3;
		}
		pr_info("%s(on): success\n", __func__);
	} else {
		gpio_free(170);
		gpio_free(171);
		gpio_free(172);
		pr_info("%s(off): success\n", __func__);
	}

	prev_on = on;
	return 0;

error3:
	gpio_free(171);
error2:
	gpio_free(170);
error1:
	return rc;
}

static int hdmi_cec_power(int on)
{
#if !defined(CONFIG_VIDEO_MHL_V1) && !defined(CONFIG_VIDEO_MHL_V2)
	static struct regulator *reg_8901_l3;		/* HDMI_CEC */
	static int prev_on;
	int rc;

	if (on == prev_on)
		return 0;

#if !defined(CONFIG_JPN_MODEL_SC_05D)
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
#endif
	prev_on = on;

	return 0;
error:
	regulator_disable(reg_8901_l3);
	return rc;
#else/*CONFIG_VIDEO_MHL_V1 || CONFIG_VIDEO_MHL_V2*/
	printk("%s: skipped when MHL is being used\n", __func__);
	return 0;
#endif
}

static int hdmi_panel_power(int on)
{
	int rc;

	pr_info("%s: HDMI Core: %s\n", __func__, (on ? "ON" : "OFF"));
	rc = hdmi_core_power(on, 1);
	if (rc)
		rc = hdmi_cec_power(on);

	pr_info("%s: HDMI Core: %s Success\n", __func__, (on ? "ON" : "OFF"));
	return rc;
}
#undef _GET_REGULATOR

#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL */


#ifdef CONFIG_KEYPAD_CYPRESS_TOUCH
static uint32_t Touch_Key_off_gpio_table[] = {
    GPIO_CFG(GPIO_TKEY_I2C_SCL,  GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
    GPIO_CFG(GPIO_TKEY_I2C_SDA,  GPIOMUX_FUNC_GPIO, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
};

struct regulator *TKEY_L12 = NULL;
struct regulator *TKEY_L3B = NULL;
static int __init tkey_device_init(void)
{
	int rc;
	struct regulator *lvs2;

#ifdef CONFIG_BATTERY_SEC
	if(is_lpm_boot)
		return 0;
#endif


		lvs2 = regulator_get(NULL, "8901_lvs2");
		rc = regulator_enable(lvs2);
        if(rc)	
			printk("[TKEY] %s: error enabling regulator\n", __func__);
        else 
            printk("[TKEY] %s: 8901_lvs2 On \n", __func__);
		regulator_put(lvs2);

	//TKEY_L12 = regulator_get(&rpm_vreg_device[RPM_VREG_ID_PM8058_L12].dev, "8058_l12");
	TKEY_L12 = regulator_get(NULL, "8058_l12");


#if !defined(CONFIG_KOR_MODEL_SHV_E160L)
	//TKEY_L12 = regulator_get(&rpm_vreg_device[RPM_VREG_ID_PM8058_L12].dev, "8058_l12");
	TKEY_L12 = regulator_get(NULL, "8058_l12");

		if (IS_ERR(TKEY_L12)) {
			rc = PTR_ERR(TKEY_L12);
		    pr_err("%s: L12 get failed (%d)\n", __func__, rc);
			return rc;
		}
		rc = regulator_set_voltage(TKEY_L12, 3300000, 3300000);
		if (rc) {
			pr_err("%s: L12 set level failed (%d)\n", __func__, rc);
			return rc;
		}
		rc = regulator_enable(TKEY_L12);
		if (rc) {
			pr_err("%s: L12vreg enable failed (%d)\n", __func__, rc);
			return rc;
		}
		regulator_put(TKEY_L12);
		printk("[TKEY] %s: TKEY_L12 3.3 V Set \n", __func__);
#endif
		return 0;
}


#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_JPN_MODEL_SC_05D)
int tkey_vdd_enable(int onoff)
{
	struct regulator *lvs2;	
		struct regulator *l12;	
	int ret;
		int rc;

		//==================================================//
		//			8901_lvs2 CONTROL						//
		//==================================================//
	lvs2 = regulator_get(NULL, "8901_lvs2");
		if (IS_ERR(lvs2))	 {	 
			printk("[TKEY] 160S %s: Return \n", __func__);
		return -1;
		}
	
		if(onoff)  {
			if(regulator_is_enabled(lvs2)){
				printk("[TKEY] 160S %s: TKEY is already ON (lvs2) !!!\n", __func__);
			}
			else{
				ret = regulator_enable(lvs2);
				if (ret) 
					printk("[TKEY] 160S %s: error enabling regulator\n", __func__);
				else 
					printk("[TKEY] 160S %s: lvs2 is ON !!!\n", __func__);
			}
		}
		else   {
			if(regulator_is_enabled(lvs2)){
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_JPN_MODEL_SC_05D)
				//config_gpio_table(Touch_Key_off_gpio_table, ARRAY_SIZE(Touch_Key_off_gpio_table));
#endif
				ret = regulator_disable(lvs2);
				if (ret) 
					printk("[TKEY] 160S %s: error enabling regulator (lvs2)\n", __func__);
				else 
					printk("[TKEY] 160S %s: lvs2 is OFF  !!!\n", __func__);
			}
			else{
				printk("[TKEY] 160S %s: TKEY is already OFF (lvs2) !!!\n", __func__);
			}
		}
		regulator_put(lvs2);
		//=================================================//
		//			8058_l12 CONTROL  (3.3V)			   //
		//=================================================//
		// l12 = regulator_get(&rpm_vreg_device[RPM_VREG_ID_PM8058_L12].dev, "8058_l12");
		l12 = regulator_get(NULL, "8058_l12");
	
		if (IS_ERR(l12)) {
			rc = PTR_ERR(l12);
			printk("[TKEY] 160S %s: L12 get failed (%d)\n", __func__, rc);
			return rc;
		}
		
		rc = regulator_set_voltage(l12, 3300000, 3300000);
		if (rc) {
			printk("[TKEY] 160S %s: L12 set level failed (%d)\n", __func__, rc);
			return rc;
		}

	if(onoff)
	{
			if(regulator_is_enabled(l12)){
				printk("[TKEY] 160S %s: TKEY is already ON (TKEY_L12 : 3.3 V)!!! \n", __func__);
			}
			else {
				rc = regulator_enable(l12);
				if (rc) {
					printk("[TKEY] 160S %s: L12vreg enable failed (%d)\n", __func__, rc);
					return rc;
				}
				else 
					printk("[TKEY] 160S %s: TKEY_L12 3.3 V on \n", __func__);
			}
		}
		else 
		{
			if(regulator_is_enabled(l12)){
				rc = regulator_disable(l12);
				if (rc) {
					printk("[TKEY] 160S %s: L12vreg disable failed (%d)\n", __func__, rc);
					return rc;
				}
				else 
					printk("[TKEY] 160S %s: TKEY_L12 3.3 V OFF \n", __func__);
			}
			else {
				printk("[TKEY] 160S %s: TKEY is already OFF (TKEY_L12 : 3.3 V)!!! \n", __func__);
			}
		}
		regulator_put(l12);
		return 0;
	}
#elif defined(CONFIG_KOR_MODEL_SHV_E160L)
int tkey_vdd_enable(int onoff)
{
	struct regulator *lvs2;	
	int ret;
	printk("%s: onoff = %d \n", __func__,onoff);
		lvs2 = regulator_get(NULL, "8901_lvs2");
		if (IS_ERR(lvs2))
			return -1;
		if(onoff==1)	{
			if(regulator_is_enabled(lvs2)){
				printk(KERN_ERR "[TSP POWER ON	lvs2 is enabled\n");
			}
			else{
		ret = regulator_enable(lvs2);
		if (ret) {
			printk("%s: error enabling regulator\n", __func__);
		}
	}
		}
		else	{
			if(regulator_is_enabled(lvs2)){
		ret = regulator_disable(lvs2);
		if (ret) {
			printk("%s: error enabling regulator\n", __func__);
		}
	}
		}
	regulator_put(lvs2);
	return 0;
}
#endif
EXPORT_SYMBOL(tkey_vdd_enable);


#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K)  || defined (CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_JPN_MODEL_SC_05D)
int tkey_led_vdd_enable(int onoff) 
{
	struct regulator *l12;
	int ret;
		printk("[TKEY] tkey_led_vdd_enable %d\n", onoff);
		
#if !defined (CONFIG_USA_MODEL_SGH_I717)		
		if(get_hw_rev()>=0x02)
#endif
	{
		l12 = regulator_get(NULL, "8058_l12");
		if (IS_ERR(l12))
			return -1;

		if(onoff)
		{
			ret = regulator_set_voltage(l12, 3300000, 3300000);

			if (ret) {
				printk("%s: error setting voltage\n", __func__);
			}

			if(!regulator_is_enabled(l12)) {
			ret = regulator_enable(l12);
			if (ret) {
				printk("%s: error enabling regulator\n", __func__);
				}
			}
		}
		else
		{
			ret = regulator_set_voltage(l12, 3300000, 3300000);

			if (ret) {
				printk("%s: error setting voltage\n", __func__);
			}

			if(regulator_is_enabled(l12)) {
			ret = regulator_disable(l12);
			if (ret) {
				printk("%s: error enabling regulator\n", __func__);
				}
			}
		}
		regulator_put(l12);
	}
	return 0;
}

#elif defined(CONFIG_KOR_MODEL_SHV_E160L)
int tkey_led_vdd_enable(int onoff) 
{
	struct regulator *l3;
	struct regulator *l12;
	int ret;
	printk("tkey_led_vdd_enable %d\n", onoff);

	if(get_hw_rev()>=0x01)
	{
		l3 = regulator_get(NULL, "8901_l3");
		if (IS_ERR(l3))
			return -1;
		if(onoff==1)
		{
			ret = regulator_set_voltage(l3, 3300000, 3300000);
			if (ret) {
				printk("%s: error setting voltage\n", __func__);
			}
			if(!regulator_is_enabled(l3)) {
				ret = regulator_enable(l3);
				if (ret) {
					printk("%s: error enabling regulator\n", __func__);
				}
			}
		}
		else if(onoff==2) //for init
		{
			//l3 = regulator_get(&rpm_vreg_device[RPM_VREG_ID_PM8901_L3].dev, "8901_l3");
			l3 = regulator_get(NULL, "8901_13");
			if (IS_ERR(l3)) {
				ret = PTR_ERR(l3);
				pr_err("%s: l3 get failed (%d)\n",__func__, ret);
				return ret;
			}
			ret = regulator_set_voltage(l3, 3300000, 3300000);
			if (ret) {
				printk("%s: error setting voltage\n", __func__);
			}
			if(!regulator_is_enabled(l3)) {
				ret = regulator_enable(l3);
				if (ret) {
					printk("%s: error enabling regulator\n", __func__);
					}
			}
		}
		else
		{
			ret = regulator_set_voltage(l3, 3300000, 3300000);
			if (ret) {
				printk("%s: error setting voltage\n", __func__);
			}
			if(regulator_is_enabled(l3)) {
				ret = regulator_disable(l3);
				if (ret) {
					printk("%s: error enabling regulator\n", __func__);
				}
			}
		}
		regulator_put(l3);
	}else{
		l12 = regulator_get(NULL, "8058_l12");
		if (IS_ERR(l12))
			return -1;
		if(onoff)
		{
			ret = regulator_set_voltage(l12, 3300000, 3300000);
			if (ret) {
				printk("%s: error setting voltage\n", __func__);
			}
			if(!regulator_is_enabled(l12)) {
				ret = regulator_enable(l12);
				if (ret) {
					printk("%s: error enabling regulator\n", __func__);
				}
			}
		}
		else if(onoff==2) //for init
		{
			//l12 = regulator_get(&rpm_vreg_device[RPM_VREG_ID_PM8058_L12].dev, "8058_l12");
			l12 = regulator_get(NULL, "8058_l12");
			if (IS_ERR(l12)) {
				ret = PTR_ERR(l12);
				pr_err("%s: L12 get failed (%d)\n",
					__func__, ret);
				return ret;
			}
			ret = regulator_set_voltage(l12, 3300000, 3300000);
			if (ret) {
				printk("%s: error setting voltage\n", __func__);
			}
			if(!regulator_is_enabled(l12)) {
				ret = regulator_enable(l12);
				if (ret) {
					printk("%s: error enabling regulator\n", __func__);
					}
			}
		}
		else
		{
			ret = regulator_set_voltage(l12, 3300000, 3300000);
			if (ret) {
				printk("%s: error setting voltage\n", __func__);
			}
			if(regulator_is_enabled(l12)) {
				ret = regulator_disable(l12);
				if (ret) {
					printk("%s: error enabling regulator\n", __func__);
					}
			}
		}
		regulator_put(l12);
	}
	return 0;  
}
#endif
EXPORT_SYMBOL(tkey_led_vdd_enable);
#endif /* CONFIG_KEYPAD_CYPRESS_TOUCH */


static struct regulator *vsensor_2p85 = NULL;
#if defined(CONFIG_KOR_MODEL_SHV_E110S) || defined(CONFIG_EUR_MODEL_GT_I9210) || defined(CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
static struct regulator *vsensor_2p85_magnetic = NULL;
#endif
#if defined(CONFIG_USA_MODEL_SGH_I757) || defined (CONFIG_CAN_MODEL_SGH_I757M)
static struct regulator *vsensor_3p_proximity = NULL;
static int sensor_power_3p_prox_cnt = 0;
#endif
#if defined (CONFIG_OPTICAL_GP2AP020A00F)
static struct regulator *vsensor_2p4 = NULL;
#endif
#ifndef CONFIG_USA_MODEL_SGH_I717
static struct regulator *vsensor_1p8 = NULL;
#else
static struct regulator *vsensor_2p85_magnetic = NULL;
#endif

static int sensor_power_2p85_cnt = 0;
static int sensor_power_2p4_cnt = 0;
static int sensor_power_1p8_cnt = 0;
static int sensor_power_2p85_mag_cnt = 0;

#if defined (CONFIG_USA_MODEL_SGH_I757) || defined (CONFIG_CAN_MODEL_SGH_I757M)
static void taos_power_on(void)
{
	int ret=0;
	if(vsensor_3p_proximity == NULL) {
		vsensor_3p_proximity = regulator_get(NULL, "8901_l6");
		if (IS_ERR(vsensor_3p_proximity))
			return -1;

		ret = regulator_set_voltage(vsensor_3p_proximity, 3000000, 3000000);
		if (ret)
			printk("%s: error vsensor_3p setting voltage ret=%d\n", __func__, ret);
	}


	sensor_power_3p_prox_cnt++;
	ret = regulator_enable(vsensor_3p_proximity);
	if (ret) {
		printk("%s: error enabling regulator\n", __func__);
	}
	pr_info("[TAOS] %s: sensor_power_3p_prox_cnt=%d\n", __func__, sensor_power_3p_prox_cnt);
}
static void taos_power_off(void)
{
	int ret=0;

	sensor_power_3p_prox_cnt--;
	if(regulator_is_enabled(vsensor_3p_proximity)) {
		ret = regulator_disable(vsensor_3p_proximity);
		if (ret) {
			printk("%s: error vsensor_2p85 enabling regulator\n", __func__);
		}
	}
	pr_info("[TAOS] %s: sensor_power_3p_prox_cnt=%d\n", __func__, sensor_power_3p_prox_cnt);
}
#endif

static void sensor_power_on_vdd(int vdd_2p85_on, int vdd_1p8_on, int vdd_2p4_on, int vdd_2p85_mag_on)
{
	int ret;

	if(vsensor_2p85 == NULL) {
	vsensor_2p85 = regulator_get(NULL, "8058_l11");
	if (IS_ERR(vsensor_2p85))
		return;

	ret = regulator_set_voltage(vsensor_2p85, 2850000, 2850000);
		if (ret)
			printk("%s: error vsensor_2p85 setting voltage ret=%d\n", __func__, ret);
	}

	if(vdd_2p85_on) {
		sensor_power_2p85_cnt++;
		ret = regulator_enable(vsensor_2p85);
		if (ret) {
			printk("%s: error enabling regulator\n", __func__);
		}
	}

#ifndef CONFIG_USA_MODEL_SGH_I717
	if(vsensor_1p8 == NULL) {
	vsensor_1p8 = regulator_get(NULL, "8058_l20");
	if (IS_ERR(vsensor_1p8))
			return;

	ret = regulator_set_voltage(vsensor_1p8, 1800000, 1800000);
			if (ret)
		printk("%s: error vsensor_1p8 setting voltage ret=%d\n", __func__, ret);
		}

	if(vdd_1p8_on) {
		sensor_power_1p8_cnt++;
		ret = regulator_enable(vsensor_1p8);
		if (ret)
				printk("%s: error enabling regulator\n", __func__);
			}
#endif

#if defined (CONFIG_OPTICAL_GP2AP020A00F)
	if(vsensor_2p4 == NULL) {
		vsensor_2p4 = regulator_get(NULL, "8901_l6");
		if (IS_ERR(vsensor_2p4))
			return;

		ret = regulator_set_voltage(vsensor_2p4, 2400000, 2400000);
		if (ret)
			printk("%s: error vsensor_2p4 setting voltage ret=%d\n", __func__, ret);
	}

	if(vdd_2p4_on) {
		sensor_power_2p4_cnt++;
		ret = regulator_enable(vsensor_2p4);
		if (ret) {
			printk("%s: error enabling regulator\n", __func__);
		}
	}
#endif

#if defined(CONFIG_KOR_MODEL_SHV_E110S) || defined(CONFIG_EUR_MODEL_GT_I9210)
	if(vsensor_2p85_magnetic == NULL) {
		vsensor_2p85_magnetic = regulator_get(NULL, "8058_l8");
		if (IS_ERR(vsensor_2p85_magnetic))
		return;

		ret = regulator_set_voltage(vsensor_2p85_magnetic, 2850000, 2850000);
		if (ret)
			printk("%s: error vsensor_2p85_magnetic setting voltage ret=%d\n", __func__, ret);
	}

	if(vdd_2p85_mag_on) {
		sensor_power_2p85_mag_cnt++;
		ret = regulator_enable(vsensor_2p85_magnetic);
		if (ret) {
			printk("%s: error enabling regulator\n", __func__);
		}
	}
#elif defined(CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_KOR_MODEL_SHV_E160K)  || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	if(get_hw_rev() >= 0x04) {
    	if(vsensor_2p85_magnetic == NULL) {
    		vsensor_2p85_magnetic = regulator_get(NULL, "8058_l8");
    		if (IS_ERR(vsensor_2p85_magnetic))
    		return;
    
    		ret = regulator_set_voltage(vsensor_2p85_magnetic, 2800000, 2800000);
    		if (ret)
    			printk("[MAG] %s: error vsensor_2p85_magnetic setting voltage ret=%d\n", __func__, ret);
    	}
    
    	if(vdd_2p85_mag_on) {
    		sensor_power_2p85_mag_cnt++;
    		ret = regulator_enable(vsensor_2p85_magnetic);
    		if (ret) {
    			printk("[MAG] %s: error enabling regulator\n", __func__);
    		}
    	}
	}
#elif defined(CONFIG_USA_MODEL_SGH_I717)
	if (get_hw_rev() >= 0x5) {
		if(vsensor_2p85_magnetic == NULL) {
			vsensor_2p85_magnetic = regulator_get(NULL, "8058_l20");
			if (IS_ERR(vsensor_2p85_magnetic))
			return ;

			ret = regulator_set_voltage(vsensor_2p85_magnetic, 2850000, 2850000);
			if (ret)
				printk("%s: error vsensor_2p85_magnetic setting voltage ret=%d\n", __func__, ret);
			}

		if(vdd_2p85_mag_on) {
			sensor_power_2p85_mag_cnt++;
			ret = regulator_enable(vsensor_2p85_magnetic);
			if (ret) {
				printk("%s: error enabling regulator\n", __func__);
			}
		}
	}
#endif

	printk("%s: vdd_2p85=%d, vdd_1p8=%d, vdd_2p_4=%d, vdd_2p85_mag=%d\n", __func__,	sensor_power_2p85_cnt, sensor_power_1p8_cnt, sensor_power_2p4_cnt, sensor_power_2p85_mag_cnt);
}

static void sensor_power_off_vdd(int vdd_2p85_off, int vdd_1p8_off, int vdd_2p4_off, int vdd_2p85_mag_off)
{
	int ret;

	if(vdd_2p85_off) {
		sensor_power_2p85_cnt--;
		if(regulator_is_enabled(vsensor_2p85)) {
			ret = regulator_disable(vsensor_2p85);
			if (ret) {
				printk("%s: error vsensor_2p85 enabling regulator\n", __func__);
			}
		}
	}
	
#ifndef CONFIG_USA_MODEL_SGH_I717
	if(vdd_1p8_off) {
		sensor_power_1p8_cnt--;
		if(regulator_is_enabled(vsensor_1p8)) {
			ret = regulator_disable(vsensor_1p8);
			if (ret) {
				printk("%s: error vsensor_1p8 enabling regulator\n", __func__);
			}
		}
	}
#endif

#if defined (CONFIG_OPTICAL_GP2AP020A00F)
	if(vdd_2p4_off) {
		sensor_power_2p4_cnt--;
		if(regulator_is_enabled(vsensor_2p4)) {
			ret = regulator_disable(vsensor_2p4);
			if (ret) {
				printk("%s: error vsensor_2p4 enabling regulator\n", __func__);
			}
		}
	}
#endif

#if defined(CONFIG_KOR_MODEL_SHV_E110S) || defined(CONFIG_EUR_MODEL_GT_I9210)
		if(vdd_2p85_mag_off) {
			sensor_power_2p85_mag_cnt--;
			if(regulator_is_enabled(vsensor_2p85_magnetic)) {
				ret = regulator_disable(vsensor_2p85_magnetic);
			if (ret) {
					printk("%s: error vsensor_2p85_magnetic enabling regulator\n", __func__);
			}
		}
	}
#elif defined(CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
        if(get_hw_rev() >= 0x04) {
                if(vdd_2p85_mag_off) {
                    sensor_power_2p85_mag_cnt--;
                    if(regulator_is_enabled(vsensor_2p85_magnetic)) {
                        ret = regulator_disable(vsensor_2p85_magnetic);
                    if (ret) {
                            printk("[MAG] %s: error vsensor_2p85_magnetic enabling regulator\n", __func__);
                    }
                }
            }
        }
#elif defined(CONFIG_USA_MODEL_SGH_I717)
	if (get_hw_rev() >= 0x5) {
		if(vdd_2p85_mag_off) {
        	sensor_power_2p85_mag_cnt--;
       		if(regulator_is_enabled(vsensor_2p85_magnetic)) {
            	ret = regulator_disable(vsensor_2p85_magnetic);
            	if (ret) {
        			printk("[MAG] %s: error vsensor_2p85_magnetic enabling regulator\n", __func__);
            	}
        	}
        }
	}
#endif

	printk("%s: vdd_2p85=%d, vdd_1p8=%d, vdd_2p_4=%d, vdd_2p85_mag=%d\n", __func__,	sensor_power_2p85_cnt, sensor_power_1p8_cnt, sensor_power_2p4_cnt, sensor_power_2p85_mag_cnt);
}

#if !defined(CONFIG_JPN_MODEL_SC_05D)
static int __init sensor_device_init(void)
{
#ifdef CONFIG_BATTERY_SEC
	if(is_lpm_boot)
		return 0;
#endif
#if defined (CONFIG_OPTICAL_GP2A) || defined(CONFIG_OPTICAL_TAOS)
#if defined(CONFIG_JPN_MODEL_SC_05D)
	gpio_tlmm_config(GPIO_CFG(SENSOR_AKM_SDA, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),1);
	gpio_tlmm_config(GPIO_CFG(SENSOR_AKM_SCL, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),1);

#else
	gpio_tlmm_config(GPIO_CFG(SENSOR_ALS_SDA, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),1);
	gpio_tlmm_config(GPIO_CFG(SENSOR_ALS_SCL, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),1);
#endif
#endif

	return 0;
}
#endif

#if defined (CONFIG_OPTICAL_GP2A) || defined (CONFIG_OPTICAL_TAOS)
static int LED_onoff(u8 onoff)
{
	int rc;
	struct regulator *l9;

	pr_info("%d\n", __LINE__);

	printk("%s: LED_onff = %d\n", __func__, onoff);

	l9 = regulator_get(NULL, "8058_l9");

	if (IS_ERR(l9)) {
		rc = PTR_ERR(l9);
		pr_err("%s: l9 get failed (%d)\n", __func__, rc);
		return rc;
	}

	if(onoff) {
		rc = regulator_set_voltage(l9, 3000000, 3000000);
		if (rc) {
			pr_err("%s: l9 set level failed (%d)\n", __func__, rc);
			regulator_put(l9);
			return rc;
		}
		rc = regulator_enable(l9);
		if (rc) {
			pr_err("%s: l9 vreg enable failed (%d)\n", __func__, rc);
			regulator_put(l9);
			return rc;
		}
	} else {
		rc = regulator_disable(l9);
		if (rc) {
			pr_err("%s: l9 vreg enable failed (%d)\n", __func__, rc);
			regulator_put(l9);
			return rc;
		}
	}

	regulator_put(l9);

	return 0;
}

static void proximity_LED_on(void)
{
	LED_onoff(1);
}
static void proximity_LED_off(void)
{
	LED_onoff(0);
}
#endif /* CONFIG_OPTICAL_GP2A || CONFIG_OPTICAL_GP2AP020A00F */

static void sensor_power_on(void)
{
#if defined(CONFIG_KOR_MODEL_SHV_E110S) || defined(CONFIG_EUR_MODEL_GT_I9210)
	if(get_hw_rev() >= 0x08)
		sensor_power_on_vdd(1, 0, 0, 1);
	else if(get_hw_rev() >= 0x05)
		sensor_power_on_vdd(1, 0, 0, 0);
	else
		sensor_power_on_vdd(1, 1, 0, 0);
#elif defined(CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	if(get_hw_rev()>=0x04)
		sensor_power_on_vdd(1, 1, 0, 1);
	else if(get_hw_rev()>=0x02)
		sensor_power_on_vdd(1, 1, 0, 0);
	else
		sensor_power_on_vdd(1, 1, 1, 0);
#elif defined(CONFIG_USA_MODEL_SGH_I757) || defined (CONFIG_CAN_MODEL_SGH_I757M)
	sensor_power_on_vdd(1, 0, 0, 0);
#elif defined (CONFIG_USA_MODEL_SGH_I717)
	if (get_hw_rev() >= 0x5)
	    sensor_power_on_vdd(1, 0, 0, 1);
	else
		sensor_power_on_vdd(1, 0, 0, 0);
#else
	sensor_power_on_vdd(1, 1, 0, 0);
#endif
}

static void sensor_power_off(void)
{
#if defined(CONFIG_KOR_MODEL_SHV_E110S) || defined(CONFIG_EUR_MODEL_GT_I9210)
	if(get_hw_rev() >= 0x08)
		sensor_power_off_vdd(1, 0, 0, 1);
	else if(get_hw_rev() >= 0x05)
		sensor_power_off_vdd(1, 0, 0, 0);
	else
		sensor_power_off_vdd(1, 1, 0, 0);
#elif defined(CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	if(get_hw_rev()>=0x04)
		sensor_power_off_vdd(1, 1, 0, 1);
	else if(get_hw_rev()>=0x02)
		sensor_power_off_vdd(1, 1, 0, 0);
	else
		sensor_power_off_vdd(1, 1, 1, 0);
#elif defined(CONFIG_USA_MODEL_SGH_I757) || defined (CONFIG_CAN_MODEL_SGH_I757M)
		sensor_power_off_vdd(1, 0, 0, 0);
#elif defined (CONFIG_USA_MODEL_SGH_I717)
	if (get_hw_rev() >= 0x5)
	    sensor_power_off_vdd(1, 0, 0, 1);
	else
		sensor_power_off_vdd(1, 0, 0, 0);
#else
	sensor_power_off_vdd(1, 1, 0, 0);
#endif
}
static void sensor_power_on_mag(void)
{
	sensor_power_on_vdd(0, 0, 0, 1);
}

static void sensor_power_off_mag(void)
{
	sensor_power_off_vdd(0, 0, 0, 1);
}
/*
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
*/
static uint32_t vibrator_device_gpio_config[] = {
	GPIO_CFG(30, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(31, 2, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),

};

static void config_gpio_tables_forvib(uint32_t *table, int len)
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

static int __init vibrator_device_gpio_init(void)
{
	config_gpio_tables_forvib(vibrator_device_gpio_config,	ARRAY_SIZE(vibrator_device_gpio_config));
	return 0;
}


#ifdef CONFIG_MSM_BUS_SCALING
// Celox SKT
static struct msm_bus_vectors rotator_init_vectors[] = {
	{
		.src = MSM_BUS_MASTER_ROTATOR,
		.dst = MSM_BUS_SLAVE_SMI,
#if 1	/* onlyjazz.el26 : temporarilly use non-zero bandwidth in order to avoid mmfab rate change during smi_clk is disabled */		
		.ab = 0,
		.ib = 0,
#else	/* onlyjazz.el26 : temporarilly use non-zero bandwidth in order to avoid mmfab rate change during smi_clk is disabled */			
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

static struct msm_bus_vectors mdp_sd_smi_vectors[] = {
	/* Default case static display/UI/2d/3d if FB SMI */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 432384000,
		.ib = 540480000,
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
		
#if 1	/* onlyjazz.el26 : temporarilly use non-zero bandwidth in order to avoid mmfab rate change during smi_clk is disabled */
		.ab = 0,
		.ib = 0,
#else	/* onlyjazz.el26 : temporarilly use non-zero bandwidth in order to avoid mmfab rate change during smi_clk is disabled */
#if defined (CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_JPN_MODEL_SC_05D)
		.ab = 432384000,
		.ib = 540480000 * 2,
#else
		.ab = 328704000,
		.ib = 410880000 * 2,
#endif
#endif		
	},
	/* Master and slaves can be from different fabrics */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
#if defined (CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_JPN_MODEL_SC_05D)
		.ab = 432384000,
		.ib = 540480000 * 2,
#else
		.ab = 328704000,
		.ib = 410880000 * 2,
#endif
	},
};
static struct msm_bus_vectors mdp_vga_vectors[] = {
	/* VGA and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 328704000, 
		.ib = 410880000,
	},
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 328704000,
		.ib = 410880000 * 2,
	},
};
static struct msm_bus_vectors mdp_720p_vectors[] = {
	/* 720p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 432384000,
		.ib = 540480000,
	},
	/* Master and slaves can be from different fabrics */
       {
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 432384000,
		.ib = 540480000 * 2,
	},
};

static struct msm_bus_vectors mdp_1080p_vectors[] = {
	/* 1080p and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 432384000,
#if defined (CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_JPN_MODEL_SC_05D)
		.ib = 540480000,
#else
		.ib = 648576000,//540480000,
#endif
	},
	/* Master and slaves can be from different fabrics */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 432384000,
#if defined (CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_JPN_MODEL_SC_05D)
		.ib = 540480000 * 2,
#else
		.ib = 648576000*2,//540480000 * 2,
#endif
	},
};

#ifdef CONFIG_FB_MSM_MDP_ADDITIONAL_BUS_SCALING //Adding the block 
static struct msm_bus_vectors mdp_vga_special_case[] = { 
	/* VGA and less video */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 432384000, 
		.ib = 540480000,
	},
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
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
#ifdef CONFIG_FB_MSM_MDP_ADDITIONAL_BUS_SCALING 	// Adding the block 
	{ 
		ARRAY_SIZE(mdp_vga_special_case), 
		mdp_vga_special_case, 
	}, 
#endif 
};

static struct msm_bus_scale_pdata mdp_bus_scale_pdata = {
	mdp_bus_scale_usecases,
	ARRAY_SIZE(mdp_bus_scale_usecases),
	.name = "mdp",
};

#endif /* CONFIG_MSM_BUS_SCALING */


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

static struct msm_bus_vectors dtv_bus_hdmi_prim_vectors[] = {
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
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL
	.lcdc_power_save = hdmi_panel_power,
#endif
};

static struct msm_bus_paths dtv_hdmi_prim_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(dtv_bus_init_vectors),
		dtv_bus_init_vectors,
	},
	{
		ARRAY_SIZE(dtv_bus_hdmi_prim_vectors),
		dtv_bus_hdmi_prim_vectors,
	},
};

static struct msm_bus_scale_pdata dtv_hdmi_prim_bus_scale_pdata = {
	dtv_hdmi_prim_bus_scale_usecases,
	ARRAY_SIZE(dtv_hdmi_prim_bus_scale_usecases),
	.name = "dtv",
};

static struct lcdc_platform_data dtv_hdmi_prim_pdata = {
	.bus_scale_table = &dtv_hdmi_prim_bus_scale_pdata,
};
#endif /* CONFIG_MSM_BUS_SCALING */


//static struct lcdc_platform_data lcdc_pdata = {
//	.lcdc_power_save   = lcdc_panel_power,
//};

#if !defined(CONFIG_FB_MSM_MIPI_S6D6AA0_WXGA_PANEL) \
	&& !defined(CONFIG_FB_MSM_MIPI_S6E8AA0_HD720_PANEL) \
	&& !defined(CONFIG_FB_MSM_MIPI_S6E8AA0_WXGA_Q1_PANEL)

#define MDP_VSYNC_GPIO			28
#else
#define MDP_VSYNC_GPIO (-1)
#endif

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

#ifdef CONFIG_FB_MSM_MIPI_S6D6AA0_WXGA_PANEL
    mipi_S6D6AA0_panel_power(on);
#endif    
#ifdef CONFIG_FB_MSM_MIPI_S6E8AA0_HD720_PANEL
    mipi_S6E8AA0_panel_power(on);
#endif    
#ifdef CONFIG_FB_MSM_MIPI_S6E8AA0_WXGA_Q1_PANEL
    mipi_S6E8AA0_panel_power(on);
#endif    

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

#ifdef CONFIG_FB_MSM_TVOUT
static struct regulator *reg_8058_l13;

static int atv_dac_power(int on)
{
	int rc = 0;
	#define _GET_REGULATOR(var, name) do {				\
		var = regulator_get(NULL, name);			\
		if (IS_ERR(var)) {					\
			pr_info("'%s' regulator not found, rc=%ld\n",	\
				name, IS_ERR(var));			\
			var = NULL;					\
			return -ENODEV;					\
		}							\
	} while (0)

	if (!reg_8058_l13)
		_GET_REGULATOR(reg_8058_l13, "8058_l13");
	#undef _GET_REGULATOR

	if (on) {
		rc = regulator_set_voltage(reg_8058_l13, 2050000, 2050000);
		if (rc) {
			pr_info("%s: '%s' regulator set voltage failed,\
				rc=%d\n", __func__, "8058_l13", rc);
			return rc;
		}

		rc = regulator_enable(reg_8058_l13);
		if (rc) {
			pr_err("%s: '%s' regulator enable failed,\
				rc=%d\n", __func__, "8058_l13", rc);
			return rc;
		}
	} else {
		rc = regulator_force_disable(reg_8058_l13);
		if (rc)
			pr_warning("%s: '%s' regulator disable failed, rc=%d\n",
				__func__, "8058_l13", rc);
	}
	return rc;

}
#endif /* CONFIG_FB_MSM_TVOUT */


static struct msm_panel_common_pdata mdp_pdata = {
	.gpio = MDP_VSYNC_GPIO,
	.mdp_max_clk = 200000000,
#ifdef CONFIG_MSM_BUS_SCALING
	.mdp_bus_scale_table = &mdp_bus_scale_pdata,
#endif
	.mdp_rev = MDP_REV_41,
#ifdef CONFIG_MSM_MULTIMEDIA_USE_ION
	.mem_hid = BIT(ION_CP_WB_HEAP_ID),
#else
	.mem_hid = MEMTYPE_EBI1,
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

#ifdef CONFIG_FB_MSM_TVOUT

#ifdef CONFIG_MSM_BUS_SCALING
static struct msm_bus_vectors atv_bus_init_vectors[] = {
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
static struct msm_bus_vectors atv_bus_def_vectors[] = {
	/* For now, 0th array entry is reserved.
	 * Please leave 0 as is and don't use it
	 */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_SMI,
		.ab = 236390400,
		.ib = 265939200,
	},
	/* Master and slaves can be from different fabrics */
	{
		.src = MSM_BUS_MASTER_MDP_PORT0,
		.dst = MSM_BUS_SLAVE_EBI_CH0,
		.ab = 236390400,
		.ib = 265939200,
	},
};
static struct msm_bus_paths atv_bus_scale_usecases[] = {
	{
		ARRAY_SIZE(atv_bus_init_vectors),
		atv_bus_init_vectors,
	},
	{
		ARRAY_SIZE(atv_bus_def_vectors),
		atv_bus_def_vectors,
	},
};
static struct msm_bus_scale_pdata atv_bus_scale_pdata = {
	atv_bus_scale_usecases,
	ARRAY_SIZE(atv_bus_scale_usecases),
	.name = "atv",
};
#endif

static struct tvenc_platform_data atv_pdata = {
	.poll		 = 0,
	.pm_vid_en	 = atv_dac_power,
#ifdef CONFIG_MSM_BUS_SCALING
	.bus_scale_table = &atv_bus_scale_pdata,
#endif
};
#endif /* CONFIG_FB_MSM_TVOUT */


static void __init msm_fb_add_devices(void)
{
#ifdef CONFIG_FB_MSM_LCDC_DSUB
	mdp_pdata.mdp_max_clk = 200000000;
#endif
	if (machine_is_msm8x60_rumi3())
		msm_fb_register_device("mdp", NULL);
	else
		msm_fb_register_device("mdp", &mdp_pdata);

//	msm_fb_register_device("lcdc", &lcdc_pdata);
	msm_fb_register_device("mipi_dsi", &mipi_dsi_pdata);
#ifdef CONFIG_MSM_BUS_SCALING
	if (hdmi_is_primary)
		msm_fb_register_device("dtv", &dtv_hdmi_prim_pdata);
	else
		msm_fb_register_device("dtv", &dtv_pdata);
#endif
#ifdef CONFIG_FB_MSM_TVOUT
	msm_fb_register_device("tvenc", &atv_pdata);
	msm_fb_register_device("tvout_device", NULL);
#endif
}

/**
 * Set MDP clocks to high frequency to avoid underflow when
 * using high resolution 1200x1920 WUXGA/HDMI as primary panels
 */
static void set_mdp_clocks_for_wuxga(void)
{
	mdp_sd_smi_vectors[0].ab = 2000000000;
	mdp_sd_smi_vectors[0].ib = 2000000000;
	mdp_sd_smi_vectors[1].ab = 2000000000;
	mdp_sd_smi_vectors[1].ib = 2000000000;

	mdp_sd_ebi_vectors[0].ab = 2000000000;
	mdp_sd_ebi_vectors[0].ib = 2000000000;
	mdp_sd_ebi_vectors[1].ab = 2000000000;
	mdp_sd_ebi_vectors[1].ib = 2000000000;

	mdp_vga_vectors[0].ab = 2000000000;
	mdp_vga_vectors[0].ib = 2000000000;
	mdp_vga_vectors[1].ab = 2000000000;
	mdp_vga_vectors[1].ib = 2000000000;

	mdp_720p_vectors[0].ab = 2000000000;
	mdp_720p_vectors[0].ib = 2000000000;
	mdp_720p_vectors[1].ab = 2000000000;
	mdp_720p_vectors[1].ib = 2000000000;

	mdp_1080p_vectors[0].ab = 2000000000;
	mdp_1080p_vectors[0].ib = 2000000000;
	mdp_1080p_vectors[1].ab = 2000000000;
	mdp_1080p_vectors[1].ib = 2000000000;

	mdp_pdata.mdp_max_clk = 200000000;
}

#if 0 // (defined(CONFIG_MARIMBA_CORE)) && (defined(CONFIG_MSM_BT_POWER) || defined(CONFIG_MSM_BT_POWER_MODULE))

static const struct {
	char *name;
	int vmin;
	int vmax;
} bt_regs_info[] = {
	{ "8058_s3", 1800000, 1800000 },
	{ "8058_s2", 1300000, 1300000 },
	{ "8058_l8", 2900000, 3050000 },
};

static struct {
	bool enabled;
} bt_regs_status[] = {
	{ false },
	{ false },
	{ false },
};
static struct regulator *bt_regs[ARRAY_SIZE(bt_regs_info)];

static int bahama_bt(int on)
{
	int rc;
	int i;
	struct marimba config = { .mod_id =  SLAVE_ID_BAHAMA};

	struct bahama_variant_register {
		const size_t size;
		const struct bahama_config_register *set;
	};

	const struct bahama_config_register *p;

	u8 version;

	const struct bahama_config_register v10_bt_on[] = {
		{ 0xE9, 0x00, 0xFF },
		{ 0xF4, 0x80, 0xFF },
		{ 0xE4, 0x00, 0xFF },
		{ 0xE5, 0x00, 0x0F },
#ifdef CONFIG_WLAN
		{ 0xE6, 0x38, 0x7F },
		{ 0xE7, 0x06, 0xFF },
#endif
		{ 0xE9, 0x21, 0xFF },
		{ 0x01, 0x0C, 0x1F },
		{ 0x01, 0x08, 0x1F },
	};

	const struct bahama_config_register v20_bt_on_fm_off[] = {
		{ 0x11, 0x0C, 0xFF },
		{ 0x13, 0x01, 0xFF },
		{ 0xF4, 0x80, 0xFF },
		{ 0xF0, 0x00, 0xFF },
		{ 0xE9, 0x00, 0xFF },
#ifdef CONFIG_WLAN
		{ 0x81, 0x00, 0x7F },
		{ 0x82, 0x00, 0xFF },
		{ 0xE6, 0x38, 0x7F },
		{ 0xE7, 0x06, 0xFF },
#endif
		{ 0xE9, 0x21, 0xFF },
	};

	const struct bahama_config_register v20_bt_on_fm_on[] = {
		{ 0x11, 0x0C, 0xFF },
		{ 0x13, 0x01, 0xFF },
		{ 0xF4, 0x86, 0xFF },
		{ 0xF0, 0x06, 0xFF },
		{ 0xE9, 0x00, 0xFF },
#ifdef CONFIG_WLAN
		{ 0x81, 0x00, 0x7F },
		{ 0x82, 0x00, 0xFF },
		{ 0xE6, 0x38, 0x7F },
		{ 0xE7, 0x06, 0xFF },
#endif
		{ 0xE9, 0x21, 0xFF },
	};

	const struct bahama_config_register v10_bt_off[] = {
		{ 0xE9, 0x00, 0xFF },
	};

	const struct bahama_config_register v20_bt_off_fm_off[] = {
		{ 0xF4, 0x84, 0xFF },
		{ 0xF0, 0x04, 0xFF },
		{ 0xE9, 0x00, 0xFF }
	};

	const struct bahama_config_register v20_bt_off_fm_on[] = {
		{ 0xF4, 0x86, 0xFF },
		{ 0xF0, 0x06, 0xFF },
		{ 0xE9, 0x00, 0xFF }
	};
	const struct bahama_variant_register bt_bahama[2][3] = {
		{
			{ ARRAY_SIZE(v10_bt_off), v10_bt_off },
			{ ARRAY_SIZE(v20_bt_off_fm_off), v20_bt_off_fm_off },
			{ ARRAY_SIZE(v20_bt_off_fm_on), v20_bt_off_fm_on }
		},
		{
			{ ARRAY_SIZE(v10_bt_on), v10_bt_on },
			{ ARRAY_SIZE(v20_bt_on_fm_off), v20_bt_on_fm_off },
			{ ARRAY_SIZE(v20_bt_on_fm_on), v20_bt_on_fm_on }
		}
	};

	u8 offset = 0; /* index into bahama configs */

	on = on ? 1 : 0;
	version = read_bahama_ver();

	if (version ==  VER_UNSUPPORTED) {
		dev_err(&msm_bt_power_device.dev,
			"%s: unsupported version\n",
			__func__);
		return -EIO;
	}

	if (version == VER_2_0) {
		if (marimba_get_fm_status(&config))
			offset = 0x01;
	}

	/* Voting off 1.3V S2 Regulator,BahamaV2 used in Normal mode */
	if (on && (version == VER_2_0)) {
		for (i = 0; i < ARRAY_SIZE(bt_regs_info); i++) {
			if ((!strcmp(bt_regs_info[i].name, "8058_s2"))
				&& (bt_regs_status[i].enabled == true)) {
				if (regulator_disable(bt_regs[i])) {
					dev_err(&msm_bt_power_device.dev,
						"%s: regulator disable failed",
						__func__);
				}
				bt_regs_status[i].enabled = false;
				break;
			}
		}
	}

	p = bt_bahama[on][version + offset].set;

	dev_info(&msm_bt_power_device.dev,
		"%s: found version %d\n", __func__, version);

	for (i = 0; i < bt_bahama[on][version + offset].size; i++) {
		u8 value = (p+i)->value;
		rc = marimba_write_bit_mask(&config,
			(p+i)->reg,
			&value,
			sizeof((p+i)->value),
			(p+i)->mask);
		if (rc < 0) {
			dev_err(&msm_bt_power_device.dev,
				"%s: reg %d write failed: %d\n",
				__func__, (p+i)->reg, rc);
			return rc;
		}
		dev_dbg(&msm_bt_power_device.dev,
			"%s: reg 0x%02x write value 0x%02x mask 0x%02x\n",
				__func__, (p+i)->reg,
				value, (p+i)->mask);
	}
	/* Update BT Status */
	if (on)
		marimba_set_bt_status(&config, true);
	else
		marimba_set_bt_status(&config, false);

	return 0;
}

static int bluetooth_use_regulators(int on)
{
	int i, recover = -1, rc = 0;

	for (i = 0; i < ARRAY_SIZE(bt_regs_info); i++) {
		bt_regs[i] = on ? regulator_get(&msm_bt_power_device.dev,
						bt_regs_info[i].name) :
				(regulator_put(bt_regs[i]), NULL);
		if (IS_ERR(bt_regs[i])) {
			rc = PTR_ERR(bt_regs[i]);
			dev_err(&msm_bt_power_device.dev,
				"regulator %s get failed (%d)\n",
				bt_regs_info[i].name, rc);
			recover = i - 1;
			bt_regs[i] = NULL;
			break;
		}

		if (!on)
			continue;

		rc = regulator_set_voltage(bt_regs[i],
					  bt_regs_info[i].vmin,
					  bt_regs_info[i].vmax);
		if (rc < 0) {
			dev_err(&msm_bt_power_device.dev,
				"regulator %s voltage set (%d)\n",
				bt_regs_info[i].name, rc);
			recover = i;
			break;
		}
	}

	if (on && (recover > -1))
		for (i = recover; i >= 0; i--) {
			regulator_put(bt_regs[i]);
			bt_regs[i] = NULL;
		}

	return rc;
}

static int bluetooth_switch_regulators(int on)
{
	int i, rc = 0;

	for (i = 0; i < ARRAY_SIZE(bt_regs_info); i++) {
		if (on && (bt_regs_status[i].enabled == false)) {
			rc = regulator_enable(bt_regs[i]);
			if (rc < 0) {
				dev_err(&msm_bt_power_device.dev,
					"regulator %s %s failed (%d)\n",
					bt_regs_info[i].name,
					"enable", rc);
				if (i > 0) {
					while (--i) {
						regulator_disable(bt_regs[i]);
						bt_regs_status[i].enabled
								 = false;
					}
					break;
				}
			}
			bt_regs_status[i].enabled = true;
		} else if (!on && (bt_regs_status[i].enabled == true)) {
			rc = regulator_disable(bt_regs[i]);
			if (rc < 0) {
				dev_err(&msm_bt_power_device.dev,
					"regulator %s %s failed (%d)\n",
					bt_regs_info[i].name,
					"disable", rc);
				break;
			}
			bt_regs_status[i].enabled = false;
		}
	}
	return rc;
}

static struct msm_xo_voter *bt_clock;

static int bluetooth_power(int on)
{
	int rc = 0;
	int id;

	/* In case probe function fails, cur_connv_type would be -1 */
	id = adie_get_detected_connectivity_type();
	if (id != BAHAMA_ID) {
		pr_err("%s: unexpected adie connectivity type: %d\n",
			__func__, id);
		return -ENODEV;
	}

	if (on) {

		rc = bluetooth_use_regulators(1);
		if (rc < 0)
			goto out;

		rc = bluetooth_switch_regulators(1);

		if (rc < 0)
			goto fail_put;

		bt_clock = msm_xo_get(MSM_XO_TCXO_D0, "bt_power");

		if (IS_ERR(bt_clock)) {
			pr_err("Couldn't get TCXO_D0 voter\n");
			goto fail_switch;
		}

		rc = msm_xo_mode_vote(bt_clock, MSM_XO_MODE_ON);

		if (rc < 0) {
			pr_err("Failed to vote for TCXO_DO ON\n");
			goto fail_vote;
		}

		rc = bahama_bt(1);

		if (rc < 0)
			goto fail_clock;

		msleep(10);

		rc = msm_xo_mode_vote(bt_clock, MSM_XO_MODE_PIN_CTRL);

		if (rc < 0) {
			pr_err("Failed to vote for TCXO_DO pin control\n");
			goto fail_vote;
		}
	} else {
		/* check for initial RFKILL block (power off) */
		/* some RFKILL versions/configurations rfkill_register */
		/* calls here for an initial set_block */
		/* avoid calling i2c and regulator before unblock (on) */
		if (platform_get_drvdata(&msm_bt_power_device) == NULL) {
			dev_info(&msm_bt_power_device.dev,
				"%s: initialized OFF/blocked\n", __func__);
			goto out;
		}

		bahama_bt(0);

fail_clock:
		msm_xo_mode_vote(bt_clock, MSM_XO_MODE_OFF);
fail_vote:
		msm_xo_put(bt_clock);
fail_switch:
		bluetooth_switch_regulators(0);
fail_put:
		bluetooth_use_regulators(0);
	}

out:
	if (rc < 0)
		on = 0;
	dev_info(&msm_bt_power_device.dev,
		"Bluetooth power switch: state %d result %d\n", on, rc);

	return rc;
}

#endif /*CONFIG_MARIMBA_CORE, CONFIG_MSM_BT_POWER, CONFIG_MSM_BT_POWER_MODULE*/

#if  defined (CONFIG_KOR_MODEL_SHV_E110S) || defined (CONFIG_TARGET_LOCALE_USA) || defined (CONFIG_JPN_MODEL_SC_05D) \
  || defined(CONFIG_KOR_MODEL_SHV_E160S)  || defined (CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L)
/* YDA165 AVDD regulator */
static struct regulator *amp_reg = NULL;
static int amp_reg_ref_cnt = 0;

void yda165_avdd_power_on(void)
{
	int ret;

#ifdef CONFIG_BATTERY_SEC
	if(is_lpm_boot)
		return;
#endif

	amp_reg_ref_cnt++;
	pr_info("%s : amp_reg_ref_cnt = %d\n", __func__, amp_reg_ref_cnt);
	
	{
		if(!amp_reg) {
#if defined (CONFIG_KOR_MODEL_SHV_E110S)
			if(get_hw_rev()>=0x06)
				amp_reg = regulator_get(NULL, "8901_l3");
			else
#endif
#if defined(CONFIG_TDMB) || defined(CONFIG_TDMB_MODULE)
#if defined(CONFIG_KOR_MODEL_SHV_E110S)
			amp_reg = vreg_l2a;
#elif defined (CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_KOR_MODEL_SHV_E160K)
			amp_reg = regulator_get(NULL, "8901_l3");
#elif defined (CONFIG_KOR_MODEL_SHV_E160L)
			amp_reg = regulator_get(NULL, "8058_l5");
#else
			amp_reg = regulator_get(NULL, "8058_l2");
#endif
#else
#if defined (CONFIG_JPN_MODEL_SC_05D)
			amp_reg = regulator_get(NULL, "8901_l3");
#else
			amp_reg = regulator_get(NULL, "8058_l2");
#endif
#endif
			if (IS_ERR(amp_reg)) {
				pr_err("%s: regulator get failed (%ld)\n", __func__, PTR_ERR(amp_reg));
				return;
			}
		}

#if defined (CONFIG_KOR_MODEL_SHV_E160L) //kks_111020
		ret = regulator_set_voltage(amp_reg, 2850000, 2850000);
#else    
		ret = regulator_set_voltage(amp_reg, 2600000, 2600000);
#endif

		if (ret) {
			pr_err("%s: error setting voltage\n", __func__);
		}
		
		ret = regulator_enable(amp_reg);
		if (ret) {
			pr_err("%s: error enabling regulator\n", __func__);
		}

		pr_info("YDA165 AVDD set to 2.6V for %d revison.\n", get_hw_rev());
	}

}

void yda165_avdd_power_off(void)
{
	int ret;

	amp_reg_ref_cnt--;
	pr_info("%s : amp_reg_ref_cnt = %d\n", __func__, amp_reg_ref_cnt);

	{
		if (!amp_reg)
			return;

		if(regulator_is_enabled(amp_reg)) {
			ret = regulator_disable(amp_reg);
			if (ret < 0)
				pr_err("%s: amp regulator_disable failed (%d)\n",
						__func__, ret);
		}

		if(!amp_reg_ref_cnt)
			amp_reg = NULL;

	}
}
#else
void yda165_avdd_power_on(void)
{
	return;
}

void yda165_avdd_power_off(void)
{
	return;
}
#endif /* CONFIG_KOR_MODEL_SHV_E160S || CONFIG_KOR_MODEL_SHV_E160K || CONFIG_KOR_MODEL_SHV_E160L || CONFIG_USA_MODEL_SGH_I717 || CONFIG_JPN_MODEL_SC_05D */


static void __init msm8x60_cfg_smsc911x(void)
{
	smsc911x_resources[1].start =
		PM8058_GPIO_IRQ(PM8058_IRQ_BASE, 6);
	smsc911x_resources[1].end =
		PM8058_GPIO_IRQ(PM8058_IRQ_BASE, 6);
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

void msm_fusion_setup_pinctrl(void)
{
	struct msm_xo_voter *a1;

	if (socinfo_get_platform_subtype() == 0x3) {
		/*
		 * Vote for the A1 clock to be in pin control mode before
		* the external images are loaded.
		*/
		a1 = msm_xo_get(MSM_XO_TCXO_A1, "mdm");
		BUG_ON(!a1);
		msm_xo_mode_vote(a1, MSM_XO_MODE_PIN_CTRL);
	}
}

struct msm_board_data {
	struct msm_gpiomux_configs *gpiomux_cfgs;
};

static struct msm_board_data msm8x60_rumi3_board_data __initdata = {
	.gpiomux_cfgs = msm8x60_surf_ffa_gpiomux_cfgs,
};

static struct msm_board_data msm8x60_sim_board_data __initdata = {
	.gpiomux_cfgs = msm8x60_surf_ffa_gpiomux_cfgs,
};

static struct msm_board_data msm8x60_surf_board_data __initdata = {
	.gpiomux_cfgs = msm8x60_surf_ffa_gpiomux_cfgs,
};

static struct msm_board_data msm8x60_ffa_board_data __initdata = {
	.gpiomux_cfgs = msm8x60_surf_ffa_gpiomux_cfgs,
};

static struct msm_board_data msm8x60_fluid_board_data __initdata = {
	.gpiomux_cfgs = msm8x60_fluid_gpiomux_cfgs,
};

static struct msm_board_data msm8x60_charm_surf_board_data __initdata = {
	.gpiomux_cfgs = msm8x60_charm_gpiomux_cfgs,
};

static struct msm_board_data msm8x60_charm_ffa_board_data __initdata = {
	.gpiomux_cfgs = msm8x60_charm_gpiomux_cfgs,
};

static struct msm_board_data msm8x60_dragon_board_data __initdata = {
	.gpiomux_cfgs = msm8x60_dragon_gpiomux_cfgs,
};

#if defined(CONFIG_KOR_MODEL_SHV_E160L)
extern int no_console;
static int platform_add_devices_for_MSM(struct platform_device **devs, int num)
{
	int i, ret = 0;

	for (i = 0; i < num; i++) {
		if (!no_console || devs[i] != &msm_device_uart_dm12)	{
			ret = platform_device_register(devs[i]);
			if (ret) {
				while (--i >= 0)
					platform_device_unregister(devs[i]);
				break;
			}
		}
	}
	return ret;
}
#endif

/* turn off unused ldo */
static void pmic_regulator_force_off(void)
{
	struct regulator *reg;
	int ret;
	int rc;

	reg = regulator_get(NULL, "8901_l0");

	if (IS_ERR(reg)) {
		rc = PTR_ERR(reg);
		pr_err("%s: 8901_l0 get failed (%d)\n", __func__, rc);
		return;
	}

	if(regulator_is_enabled(reg)) {
		ret = regulator_disable(reg);
		if (ret) {
			printk("%s: error disabling regulator\n", __func__);
		}
	}

	regulator_put(reg);
}

#if defined(CONFIG_SEC_DEBUG) && defined(CONFIG_SEC_MISC)
static ssize_t show_sec_debug_level(struct device *dev, struct device_attribute *attr, char *buf)
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
#ifdef CONFIG_BATTERY_SEC
extern unsigned int sec_get_lpm_mode(void);
#endif
static void __init msm8x60_init(struct msm_board_data *board_data)
{
	uint32_t soc_platform_version;
#ifdef CONFIG_USB_EHCI_MSM_72K
	struct pm8xxx_mpp_config_data hsusb_phy_mpp = {
		.type		= PM8XXX_MPP_TYPE_D_OUTPUT,
		.level		= PM8901_MPP_DIG_LEVEL_L5,
		.control	= PM8XXX_MPP_DOUT_CTRL_HIGH,
	};
#endif

#ifdef CONFIG_BATTERY_SEC
	is_lpm_boot = sec_get_lpm_mode();
#endif

#ifdef CONFIG_SEC_DEBUG
	sec_debug_init();
#endif

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

#ifdef CONFIG_KOR_MODEL_SHV_E160L	// add EAR_VOL_KEY 
	if(system_rev >= 0x08)
		sec_jack_data.send_int = PM8058_GPIO_IRQ(PM8058_IRQ_BASE, (PMIC_GPIO_VOLUME_KEY));
#endif
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
	 * Set regulators 8901_l4 and 8901_l6 to be always on in HPM for SURF
	 * devices so that the RPM doesn't drop into a low power mode that an
	 * un-reworked SURF cannot resume from.
	 */
	if (machine_is_msm8x60_surf()) {
		int i;

		for (i = 0; i < ARRAY_SIZE(rpm_regulator_init_data); i++)
			if (rpm_regulator_init_data[i].id
				== RPM_VREG_ID_PM8901_L4
			    || rpm_regulator_init_data[i].id
				== RPM_VREG_ID_PM8901_L6)
				rpm_regulator_init_data[i]
					.init_data.constraints.always_on = 1;
	}

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
	if (!machine_is_msm8x60_rumi3() && !machine_is_msm8x60_sim())
		acpuclk_init(&acpuclk_8x60_soc_data);

	/*
	 * Enable EBI2 only for boards which make use of it. Leave
	 * it disabled for all others for additional power savings.
	 */
	if (machine_is_msm8x60_surf() || machine_is_msm8x60_ffa() ||
			machine_is_msm8x60_rumi3() ||
			machine_is_msm8x60_sim() ||
			machine_is_msm8x60_fluid() ||
			machine_is_msm8x60_dragon())
		msm8x60_init_ebi2();
	msm8x60_init_tlmm();
#ifdef CONFIG_BATTERY_SEC
	if(is_lpm_boot)
		msm8x60_init_gpiomux_cfg_for_lpm(board_data->gpiomux_cfgs);
	else
#endif
	msm8x60_init_gpiomux(board_data->gpiomux_cfgs);
#if defined(CONFIG_KOR_MODEL_SHV_E160L)
	if (!no_console)
#endif
	msm8x60_init_uart12dm();
#ifdef CONFIG_MSM_CAMERA_V4L2
	msm8x60_init_cam();
#endif
	msm8x60_init_mmc();
#ifdef CONFIG_USB_SWITCH_FSA9480
		LTE_switch_init();
		fsa9480_gpio_init();
#endif

#if defined(CONFIG_PN544_NFC)
	config_gpio_table_for_nfc();
#endif

#if defined(CONFIG_PMIC8058_OTHC) || defined(CONFIG_PMIC8058_OTHC_MODULE)
	msm8x60_init_pm8058_othc();
#endif

	if (machine_is_msm8x60_fluid())
		pm8058_platform_data.keypad_pdata = &fluid_keypad_data;
	else if (machine_is_msm8x60_dragon())
		pm8058_platform_data.keypad_pdata = &dragon_keypad_data;
	else
		pm8058_platform_data.keypad_pdata = &ffa_keypad_data;
#if 0//ndef CONFIG_MSM_CAMERA_V4L2
	/* Specify reset pin for OV9726 */
	if (machine_is_msm8x60_dragon()) {
		msm_camera_sensor_ov9726_data.sensor_reset = 62;
		ov9726_sensor_8660_info.mount_angle = 270;
	}
#endif
#ifdef CONFIG_BATTERY_MSM8X60
	if (machine_is_msm8x60_surf() || machine_is_msm8x60_ffa() ||
		machine_is_msm8x60_fusion() || machine_is_msm8x60_dragon() ||
		machine_is_msm8x60_fusn_ffa() || machine_is_msm8x60_fluid())
		platform_device_register(&msm_charger_device);
#endif

	if (machine_is_msm8x60_dragon())
		pm8058_platform_data.charger_pdata = &pmic8058_charger_dragon;
	if (!machine_is_msm8x60_fluid())
		pm8058_platform_data.charger_pdata = &pmic8058_charger_ffa_surf;

	/* configure pmic leds */
	if (machine_is_msm8x60_fluid())
		pm8058_platform_data.leds_pdata = &pm8058_fluid_flash_leds_data;
	else if (machine_is_msm8x60_dragon())
		pm8058_platform_data.leds_pdata = &pm8058_dragon_leds_data;
	else
		pm8058_platform_data.leds_pdata = &pm8058_flash_leds_data;

	if (machine_is_msm8x60_ffa() || machine_is_msm8x60_fusn_ffa() ||
		machine_is_msm8x60_dragon()) {
		pm8058_platform_data.vibrator_pdata = &pm8058_vib_pdata;
	}

	/* if csfb, enable rtc write */
	if (target_baseband == 0) {
		pm8058_rtc_pdata.rtc_write_enable = true;
	}

	if (machine_is_msm8x60_surf() || machine_is_msm8x60_ffa() ||
	    machine_is_msm8x60_fluid() || machine_is_msm8x60_fusion() ||
	    machine_is_msm8x60_fusn_ffa() || machine_is_msm8x60_dragon()) {
		msm8x60_cfg_smsc911x();
		if (SOCINFO_VERSION_MAJOR(socinfo_get_version()) != 1)
			platform_add_devices(msm_footswitch_devices,
					     msm_num_footswitch_devices);
#if !defined(CONFIG_KOR_MODEL_SHV_E160L)
		platform_add_devices(surf_devices, ARRAY_SIZE(surf_devices));
#else
		platform_add_devices_for_MSM(surf_devices, ARRAY_SIZE(surf_devices));
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
	/*
	 * Drive MPP2 pin HIGH for PHY to generate ID interrupts on 8660
	 * fluid
	 */
	if (machine_is_msm8x60_fluid())
		pm8xxx_mpp_config(PM8901_MPP_PM_TO_SYS(1), &hsusb_phy_mpp);
	msm_add_host(0, &msm_usb_host_pdata);
#endif

#ifdef CONFIG_SND_SOC_MSM8660_APQ
		if (machine_is_msm8x60_dragon())
			platform_add_devices(dragon_alsa_devices,
					ARRAY_SIZE(dragon_alsa_devices));
		else
#endif
			platform_add_devices(asoc_devices,
					ARRAY_SIZE(asoc_devices));
	} else {
		msm8x60_configure_smc91x();
		platform_add_devices(rumi_sim_devices,
				     ARRAY_SIZE(rumi_sim_devices));
	}
#if defined(CONFIG_USB_PEHCI_HCD) || defined(CONFIG_USB_PEHCI_HCD_MODULE)
	if (machine_is_msm8x60_surf() || machine_is_msm8x60_ffa() ||
		machine_is_msm8x60_dragon())
		msm8x60_cfg_isp1763();
#endif
#ifdef CONFIG_BATTERY_SEC
	if(is_lpm_boot) {
		pr_info("%s : skip charm devices for lpm boot\n", __func__);
	} else
#endif
	if (machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa())
		platform_add_devices(charm_devices, ARRAY_SIZE(charm_devices));

#if !defined(CONFIG_PN544_NFC)
#if defined(CONFIG_SPI_QUP) || defined(CONFIG_SPI_QUP_MODULE)
	if (machine_is_msm8x60_fluid())
		platform_device_register(&msm_gsbi10_qup_spi_device);
	else
#if !defined(CONFIG_TARGET_LOCALE_USA)
		platform_device_register(&msm_gsbi1_qup_spi_device);
#endif
#endif
#endif
#if defined(CONFIG_TDMB) || defined(CONFIG_TDMB_MODULE)
	tdmb_dev_init();
#endif

#if defined(CONFIG_ISDBT)
	isdbt_dev_init();
#endif

#if defined(CONFIG_TOUCHSCREEN_CYTTSP_I2C) || \
		defined(CONFIG_TOUCHSCREEN_CYTTSP_I2C_MODULE)
	if (machine_is_msm8x60_fluid())
		cyttsp_set_params();
#endif
	if (!machine_is_msm8x60_sim())
		msm_fb_add_devices();
	fixup_i2c_configs();
#ifdef CONFIG_SENSORS_YDA165
	platform_device_register(&amp_i2c_gpio_device); // YDA165WORK
#endif
	register_i2c_devices();

	if (machine_is_msm8x60_dragon())
		smsc911x_config.reset_gpio
			= GPIO_ETHERNET_RESET_N_DRAGON;

	platform_device_register(&smsc911x_device);

#if (defined(CONFIG_SPI_QUP)) && \
	(defined(CONFIG_FB_MSM_LCDC_SAMSUNG_OLED_PT) || \
	defined(CONFIG_FB_MSM_LCDC_AUO_WVGA) || \
	defined(CONFIG_FB_MSM_LCDC_NT35582_WVGA))

	if (machine_is_msm8x60_fluid()) {
#ifdef CONFIG_FB_MSM_LCDC_SAMSUNG_OLED_PT
		if (SOCINFO_VERSION_MAJOR(soc_platform_version) < 3) {
			spi_register_board_info(lcdc_samsung_spi_board_info,
				ARRAY_SIZE(lcdc_samsung_spi_board_info));
		} else
#endif
		{
#ifdef CONFIG_FB_MSM_LCDC_AUO_WVGA
			spi_register_board_info(lcdc_auo_spi_board_info,
				ARRAY_SIZE(lcdc_auo_spi_board_info));
#endif
		}
#ifdef CONFIG_FB_MSM_LCDC_NT35582_WVGA
	} else if (machine_is_msm8x60_dragon()) {
		spi_register_board_info(lcdc_nt35582_spi_board_info,
			ARRAY_SIZE(lcdc_nt35582_spi_board_info));
#endif
	}
#endif

	msm_pm_set_platform_data(msm_pm_data, ARRAY_SIZE(msm_pm_data));
	msm_pm_set_rpm_wakeup_irq(RPM_SCSS_CPU0_WAKE_UP_IRQ);
	msm_cpuidle_set_states(msm_cstates, ARRAY_SIZE(msm_cstates),
				msm_pm_data);
	BUG_ON(msm_pm_boot_init(&msm_pm_boot_pdata));

	pm8058_gpios_init();

#if 1 // SAMSUNG LDO SETTING
	/* turn off unused ldo */
	pmic_regulator_force_off();
#endif

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
#ifdef CONFIG_BATTERY_SEC
	if(!is_lpm_boot)
#endif
		msm_snddev_init();
#endif

#if defined(CONFIG_GPIO_SX150X) || defined(CONFIG_GPIO_SX150X_MODULE)
	if (machine_is_msm8x60_fluid())
		platform_device_register(&fluid_leds_gpio);
	else
		platform_device_register(&gpio_leds);
#endif

#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT540E)
	tsp_mxt540e_power_init();
#endif
#if defined (CONFIG_EPEN_WACOM_G5SP)
	wacom_init();
#endif

	msm8x60_multi_sdio_init();

	if (machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa())
		msm_fusion_setup_pinctrl();
#if !defined(CONFIG_JPN_MODEL_SC_05D)
	sensor_device_init();
#endif
#ifdef CONFIG_KEYPAD_CYPRESS_TOUCH
	tkey_device_init();
#endif
#ifdef CONFIG_BROADCOM_WIFI
	brcm_wlan_init();
#endif
vibrator_device_gpio_init();

#if defined(CONFIG_SEC_DEBUG) && defined(CONFIG_SEC_MISC)	
	{
		/* Add debug level node */
		int ret = 0;
		struct device *platform = surf_devices[0]->dev.parent;
		ret = device_create_file(platform, &dev_attr_sec_debug_level);
		if (ret)
			printk("Fail to create sec_debug_level file\n");
	}
#endif /*CONFIG_KERNEL_DEBUG_SEC*/

}

static void __init msm8x60_rumi3_init(void)
{
	msm8x60_init(&msm8x60_rumi3_board_data);
}

static void __init msm8x60_sim_init(void)
{
	msm8x60_init(&msm8x60_sim_board_data);
}

static void __init msm8x60_surf_init(void)
{
	msm8x60_init(&msm8x60_surf_board_data);
}

static void __init msm8x60_ffa_init(void)
{
	msm8x60_init(&msm8x60_ffa_board_data);
}

static void __init msm8x60_fluid_init(void)
{
	msm8x60_init(&msm8x60_fluid_board_data);
}

static void __init msm8x60_charm_surf_init(void)
{
	msm8x60_init(&msm8x60_charm_surf_board_data);
}

static void __init msm8x60_charm_ffa_init(void)
{
	msm8x60_init(&msm8x60_charm_ffa_board_data);
}

static void __init msm8x60_charm_init_early(void)
{
	msm8x60_allocate_memory_regions();
}

static void __init msm8x60_dragon_init(void)
{
	msm8x60_init(&msm8x60_dragon_board_data);
}

MACHINE_START(MSM8X60_RUMI3, "QCT MSM8X60 RUMI3")
	.map_io = msm8x60_map_io,
	.reserve = msm8x60_reserve,
	.init_irq = msm8x60_init_irq,
	.handle_irq = gic_handle_irq,
	.init_machine = msm8x60_rumi3_init,
	.timer = &msm_timer,
	.init_early = msm8x60_charm_init_early,
MACHINE_END

MACHINE_START(MSM8X60_SIM, "QCT MSM8X60 SIMULATOR")
	.map_io = msm8x60_map_io,
	.reserve = msm8x60_reserve,
	.init_irq = msm8x60_init_irq,
	.handle_irq = gic_handle_irq,
	.init_machine = msm8x60_sim_init,
	.timer = &msm_timer,
	.init_early = msm8x60_charm_init_early,
MACHINE_END

MACHINE_START(MSM8X60_SURF, "QCT MSM8X60 SURF")
	.map_io = msm8x60_map_io,
	.reserve = msm8x60_reserve,
	.init_irq = msm8x60_init_irq,
	.handle_irq = gic_handle_irq,
	.init_machine = msm8x60_surf_init,
	.timer = &msm_timer,
	.init_early = msm8x60_charm_init_early,
MACHINE_END

MACHINE_START(MSM8X60_FFA, "QCT MSM8X60 FFA")
	.map_io = msm8x60_map_io,
	.reserve = msm8x60_reserve,
	.init_irq = msm8x60_init_irq,
	.handle_irq = gic_handle_irq,
	.init_machine = msm8x60_ffa_init,
	.timer = &msm_timer,
	.init_early = msm8x60_charm_init_early,
MACHINE_END

MACHINE_START(MSM8X60_FLUID, "QCT MSM8X60 FLUID")
	.map_io = msm8x60_map_io,
	.reserve = msm8x60_reserve,
	.init_irq = msm8x60_init_irq,
	.handle_irq = gic_handle_irq,
	.init_machine = msm8x60_fluid_init,
	.timer = &msm_timer,
	.init_early = msm8x60_charm_init_early,
MACHINE_END

MACHINE_START(MSM8X60_FUSION, "QCT MSM8X60 FUSION SURF")
	.map_io = msm8x60_map_io,
	.reserve = msm8x60_reserve,
	.init_irq = msm8x60_init_irq,
	.handle_irq = gic_handle_irq,
	.init_machine = msm8x60_charm_surf_init,
	.timer = &msm_timer,
	.init_early = msm8x60_charm_init_early,
MACHINE_END

MACHINE_START(MSM8X60_FUSN_FFA, "QCT MSM8X60 FUSION FFA")
	.map_io = msm8x60_map_io,
	.reserve = msm8x60_reserve,
	.init_irq = msm8x60_init_irq,
	.handle_irq = gic_handle_irq,
	.init_machine = msm8x60_charm_ffa_init,
	.timer = &msm_timer,
	.init_early = msm8x60_charm_init_early,
MACHINE_END

MACHINE_START(MSM8X60_DRAGON, "QCT MSM8X60 DRAGON")
	.map_io = msm8x60_map_io,
	.reserve = msm8x60_reserve,
	.init_irq = msm8x60_init_irq,
	.handle_irq = gic_handle_irq,
	.init_machine = msm8x60_dragon_init,
	.timer = &msm_timer,
	.init_early = msm8x60_charm_init_early,
MACHINE_END
