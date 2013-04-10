/*
 * Copyright (C) 2008 Samsung Electronics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ASM_ARCH_SEC_MIPI_LCD_ESD_FRESH_H
#define __ASM_ARCH_SEC_MIPI_LCD_ESD_FRESH_H

#ifdef __KERNEL__
struct sec_esd_platform_data {
	int	esd_int;
};
#endif

#define SEC_MIPI_LCD_ESD_REFRESH_MODULE_NAME "mipi_lcd_esd_refresh"
#define PMIC_GPIO_ESD_DET		PM8058_GPIO(19)  	/* PMIC GPIO Number 19 */

#if 0 // board file 
static int pm8058_gpios_init(void)
....
	// kyNam_110824_
	rc = pm8058_gpio_config(PMIC_GPIO_ESD_DET, &esd_det);
	if (rc) {
		pr_err("%s PMIC_GPIO_EAR_DET config failed\n", __func__);
		return rc;
	}


static struct platform_device *surf_devices[] __initdata = {
...
#ifdef CONFIG_SAMSUNG_JACK
	&sec_device_esd,
#endif

#endif 

#ifndef __ASM_ARCH_SEC_MIPI_LCD_ESD_FRESH_C
static struct pm_gpio sec_mipi_esd_det_gpio_cfg = {
		.direction      = PM_GPIO_DIR_IN,
		.pull           = PM_GPIO_PULL_NO,
		.vin_sel        = PM_GPIO_VIN_L5, 
		.function       = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol    = 0,
};

static struct sec_esd_platform_data sec_mipi_esd_refresh_data = {
	.esd_int	= PM8058_GPIO_IRQ((NR_MSM_IRQS + NR_GPIO_IRQS), (PMIC_GPIO_ESD_DET)), // (NR_MSM_IRQS + NR_GPIO_IRQS) = PM8058_IRQ_BASE
};

static struct platform_device sec_device_mipi_esd = {
	.name           = SEC_MIPI_LCD_ESD_REFRESH_MODULE_NAME,
	.id             = -1,
	.dev            = {
		.platform_data  = &sec_mipi_esd_refresh_data,
	},
};
#endif 

#endif // __ASM_ARCH_SEC_MIPI_LCD_ESD_FRESH_H
