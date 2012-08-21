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

#ifndef __ASM_ARCH_SEC_SWITCH_H
#define __ASM_ARCH_SEC_SWITCH_H

struct sec_switch_platform_data {
	void (*set_vbus_status) (u8 mode);
	void (*set_usb_gadget_vbus) (bool en);
	int (*get_cable_status) (void);
	int (*get_phy_init_status) (void);
	int (*get_attached_device) (void);
};

#define SWITCH_MODEM	0
#define SWITCH_PDA	1

#define USB_VBUS_ALL_OFF 0
#define USB_VBUS_CP_ON	 1
#define USB_VBUS_AP_ON	 2
#define USB_VBUS_ALL_ON	 3

enum {
	DEV_TYPE_NONE = 0,
	DEV_TYPE_USB,
	DEV_TYPE_UART,
	DEV_TYPE_CHARGER,
	DEV_TYPE_JIG,
	DEV_TYPE_DESKDOCK,
	DEV_TYPE_CARDOCK,
	DEV_TYPE_MHL,
};

enum cable_type_t{
	CABLE_TYPE_NONE = 0,
	CABLE_TYPE_USB,
	CABLE_TYPE_AC,
	CABLE_TYPE_MISC,
	CABLE_TYPE_CARDOCK,
	CABLE_TYPE_UARTOFF,
	CABLE_TYPE_UNKNOWN,
};

#endif
