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

#ifndef __ASM_ARCH_ACC_CONN_H
#define __ASM_ARCH_ACC_CONN_H

struct acc_con_platform_data {
	void	(*otg_en) (int active);
	void	(*acc_power) (u8 token, bool active);
	void    (*usb_ldo_en) (int active);
	int (*get_dock_state)(void);
	int     accessory_irq_gpio;
	int     dock_irq;
	int     mhl_irq_gpio;
	int     hdmi_hpd_gpio;
};

#endif
