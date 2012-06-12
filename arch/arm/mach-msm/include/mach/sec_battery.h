/*
 * Copyright (C) 2010 Samsung Electronics, Inc.
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

#ifndef __MACH_SEC_BATTERY_H
#define __MACH_SEC_BATTERY_H __FILE__

//#define MPP_CHECK_FEATURE

/**
 * struct sec_bat_adc_table_data - adc to temperature table for sec battery
 * driver
 * @adc: adc value
 * @temperature: temperature(C) * 10
 */
struct sec_bat_adc_table_data {
	int adc;
	int temperature;
};

/**
 * struct sec_bat_plaform_data - init data for sec batter driver
 * @fuel_gauge_name: power supply name of fuel gauge
 * @charger_name: power supply name of charger
 * @sub_charger_name: power supply name of sub-charger
 * @adc_table: array of adc to temperature data
 * @adc_arr_size: size of adc_table
 * @irq_topoff: IRQ number for top-off interrupt
 * @irq_lowbatt: IRQ number for low battery alert interrupt
 */
struct sec_bat_platform_data {
	char *fuel_gauge_name;
	char *charger_name;
	unsigned int (*get_lpcharging_state) (void);
	int hwrev_has_2nd_therm;
#ifdef MPP_CHECK_FEATURE
	int (*mpp_cblpwr_config) (void);
	int mpp_get_cblpwr;
#endif
	void (*chg_shutdown_cb) (void);
};

extern unsigned int get_hw_rev(void);
#if defined(CONFIG_TOUCHSCREEN_QT602240) || defined(CONFIG_TOUCHSCREEN_MXT768E)
extern void tsp_set_unknown_charging_cable(bool);
#endif
#endif /* __MACH_SEC_BATTERY_H */
