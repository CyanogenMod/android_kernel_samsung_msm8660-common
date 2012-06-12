/*
 *  p5_battery.h
 *  charger systems for lithium-ion (Li+) batteries
 *
 *  Copyright (C) 2010 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_P5_BATTERY_H
#define _LINUX_P5_BATTERY_H

#define __TEST_DEVICE_DRIVER__
//#define __ABS_TIMER_TEST__
#define INTENSIVE_LOW_COMPENSATION

struct max8903_charger_data {
	int enable_line;
	int connect_line;
	int fullcharge_line;
	int currentset_line;
};

struct p5_battery_platform_data {
	struct max8903_charger_data charger;
	bool	(*check_dedicated_charger) (void);
	void	(*init_charger_gpio) (void);
	void 	(*inform_charger_connection) (int);
	void 	(*get_batt_level)(unsigned int *);
#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_KT) || defined(CONFIG_KOR_OPERATOR_LGU)
	int		(*check_stmpe811)(void);
	int64_t *cable_adc_check;
#endif
	int temp_high_threshold;
	int temp_high_recovery;
	int temp_low_recovery;
	int temp_low_threshold;
	int charge_duration;
	int recharge_duration;
	int recharge_voltage;
};

#endif
