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

#ifndef _SEC_KEYBOARD_DATA_H_
#define _SEC_KEYBOARD_DATA_H_


struct kbd_callbacks {
	void (*get_data)(struct kbd_callbacks *cb, unsigned int val);
	int (*check_keyboard_dock)(struct kbd_callbacks *cb, int val);
};

struct dock_keyboard_platform_data {
	int (*enable)(struct device *dev);
	int (*wakeup_key)(void);
	void (*disable)(struct device *dev);
	void	(*register_cb)(struct kbd_callbacks *cb);
	void	(*acc_power)(u8 token, bool active);
	int gpio_accessory_enable;
	int accessory_irq_gpio;
};


#endif  //_SEC_KEYBOARD_DATA_H_
