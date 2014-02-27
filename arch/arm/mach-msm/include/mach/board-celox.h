/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/mfd/pmic8058.h>

/* HW_REV GPIOS */
#define REV_GPIO_BASE 34
#if defined (CONFIG_USA_MODEL_SGH_I727) \
	|| defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_USA_MODEL_SGH_I577)
#define REV_GPIO_NUM PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(26))
#else
#define REV_GPIO_NUM PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(32))
#endif

/* Common GPIO */
#define MSM_GPIO_GYRO_FIFO_INT	102
#define PMIC_GPIO_ALS_INT	PM8058_GPIO(3)
#define PMIC_GPIO_ACCEL_INT	PM8058_GPIO(10)
#define PMIC_GPIO_GYRO_FIFO_INT	PM8058_GPIO(11)
#define PMIC_GPIO_GYRO_INT	PM8058_GPIO(12)
#define PMIC_GPIO_MSENSE_RST	PM8058_GPIO(33)
#define PMIC_GPIO_TKEY_INT	PM8058_GPIO(13)
#define PMIC_GPIO_PS_VOUT	PM8058_GPIO(14)
#define PMIC_GPIO_TA_CURRENT_SEL PM8058_GPIO(18)
#define PMIC_GPIO_CHG_EN	PM8058_GPIO(23)
#define PMIC_GPIO_CHG_STAT	PM8058_GPIO(24)
#define PMIC_GPIO_EAR_DET	PM8058_GPIO(27)
#define PMIC_GPIO_SHORT_SENDEND	PM8058_GPIO(28)
#define PMIC_GPIO_EAR_MICBIAS_EN PM8058_GPIO(29)

/* NFC GPIO */
#define GPIO_NFC_FIRM		71
#define PMIC_GPIO_NFC_IRQ	PM8058_GPIO_PM_TO_SYS(7)
#define PMIC_GPIO_NFC_EN 	PM8058_GPIO_PM_TO_SYS(29)

/* MHL GPIOS */
#define PMIC_GPIO_MHL_RST       PM8058_GPIO(15)
#define PMIC_GPIO_MHL_SEL	PM8058_GPIO(16)
#define PMIC_GPIO_MHL_INT_9	PM8058_GPIO(9)
#define PMIC_GPIO_MHL_INT_31    PM8058_GPIO(31)
#define PMIC_GPIO_MHL_WAKE_UP	PM8058_GPIO(17)
#define GPIO_MHL_RST		PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(15))
#define GPIO_MHL_SEL		PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(16))

/* SCL/SDA GPIOS */
#if defined (CONFIG_OPTICAL_GP2A) || defined(CONFIG_OPTICAL_TAOS)
#define SENSOR_ALS_SCL   	139
#define SENSOR_ALS_SDA   	138
#endif
#ifdef CONFIG_SENSORS_YDA165
#define GPIO_AMP_I2C_SCL	154
#define GPIO_AMP_I2C_SDA	155
#endif
#ifdef CONFIG_KEYPAD_CYPRESS_TOUCH
#define GPIO_TKEY_I2C_SCL	157
#define GPIO_TKEY_I2C_SDA	156
#endif
#if defined(CONFIG_TOUCHSCREEN_QT602240) \
	|| defined(CONFIG_TOUCHSCREEN_MXT768E) || defined(CONFIG_TOUCHSCREEN_MELFAS)
#define TOUCHSCREEN_IRQ 	125
#define TSP_SDA			43
#define TSP_SCL			44
#endif

/* TDMB GPIOS */
#if defined(CONFIG_TDMB) || defined(CONFIG_TDMB_MODULE)
#define GPIO_TDMB_EN		130
#define GPIO_TDMB_RST		126
#define GPIO_TDMB_INT		128
#define GPIO_TDMB_SPI_MOSI	33
#define GPIO_TDMB_SPI_MISO	34
#define GPIO_TDMB_SPI_CS	35
#define GPIO_TDMB_SPI_CLK	36

enum {
	TDMB_PMIC_CLK_INIT,
	TDMB_PMIC_CLK_ON,
	TDMB_PMIC_CLK_OFF,
};
#endif

#ifdef CONFIG_MSM_CAMERA
#define	GPIO_CAM_IO_EN		37
#define GPIO_ISP_INT		49
#define	GPIO_CAM_MAIN_RST	50
#define	GPIO_CAM_SUB_RST	41
#define	GPIO_CAM_SUB_EN		42
#endif
