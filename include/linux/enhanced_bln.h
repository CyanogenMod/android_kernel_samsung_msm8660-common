/*
 * include/linux/enhanced_bln.h
 *
 * Copyright (C) 2015, Sultanxda <sultanxda@gmail.com>
 * Rewrote driver and core logic from scratch
 *
 * Based on the original BLN implementation by:
 * Copyright 2011  Michael Richter (alias neldar)
 * Copyright 2011  Adam Kent <adam@semicircular.net>
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

#ifndef _LINUX_ENHANCED_BLN_H
#define _LINUX_ENHANCED_BLN_H
enum {
	EBLN_OFF,
	EBLN_BLINK_OFF,
	EBLN_ON,
};

struct ebln_implementation {
    void (*disable_led_reg)(void);
    void (*enable_led_reg)(void);
    void (*led_off)(int ebln_state);
    void (*led_on)(void);
};

#ifdef CONFIG_ENHANCED_BLN
void register_ebln_implementation(struct ebln_implementation *imp);
#else
static inline void register_ebln_implementation(struct ebln_implementation *imp) { }
#endif
#endif /* _LINUX_ENHANCED_BLN_H */
