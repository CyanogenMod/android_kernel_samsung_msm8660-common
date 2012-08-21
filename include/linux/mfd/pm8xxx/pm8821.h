/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
/*
 * Qualcomm PMIC 8821 driver header file
 *
 */

#ifndef __MFD_PM8821_H
#define __MFD_PM8821_H

#include <linux/device.h>
#include <linux/mfd/pm8xxx/irq.h>
#include <linux/mfd/pm8xxx/mpp.h>

#define PM8821_NR_IRQS		(64)
#define PM8821_NR_MPPS		(4)

#define PM8821_MPP_BLOCK_START	(16)
#define PM8821_IRQ_BLOCK_BIT(block, bit) ((block) * 8 + (bit))

/* MPPs [1,N] */
#define PM8821_MPP_IRQ(base, mpp)	((base) + \
		PM8821_IRQ_BLOCK_BIT(PM8821_MPP_BLOCK_START, (mpp)-1))

/* PMIC Interrupts */

struct pm8821_platform_data {
	int					irq_base;
	struct pm8xxx_irq_platform_data		*irq_pdata;
	struct pm8xxx_mpp_platform_data		*mpp_pdata;
};

#endif
