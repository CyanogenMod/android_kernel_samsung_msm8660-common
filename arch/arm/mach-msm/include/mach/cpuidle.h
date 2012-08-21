/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_CPUIDLE_H
#define __ARCH_ARM_MACH_MSM_CPUIDLE_H

#include <linux/notifier.h>
#include "../../pm.h"

struct msm_cpuidle_state {
	unsigned int cpu;
	int state_nr;
	char *name;
	char *desc;
	enum msm_pm_sleep_mode mode_nr;
};

#ifdef CONFIG_CPU_IDLE
void msm_cpuidle_set_states(struct msm_cpuidle_state *states,
	int nr_states, struct msm_pm_platform_data *pm_data);

int msm_cpuidle_init(void);
#else
static inline void msm_cpuidle_set_states(struct msm_cpuidle_state *states,
	int nr_states, struct msm_pm_platform_data *pm_data) {}

static inline int msm_cpuidle_init(void)
{ return -ENOSYS; }
#endif

#ifdef CONFIG_MSM_SLEEP_STATS
enum {
	MSM_CPUIDLE_STATE_ENTER,
	MSM_CPUIDLE_STATE_EXIT
};

int msm_cpuidle_register_notifier(unsigned int cpu,
		struct notifier_block *nb);
int msm_cpuidle_unregister_notifier(unsigned int cpu,
		struct notifier_block *nb);
#else
static inline int msm_cpuidle_register_notifier(unsigned int cpu,
		struct notifier_block *nb)
{ return -ENODEV; }
static inline int msm_cpuidle_unregister_notifier(unsigned int cpu,
		struct notifier_block *nb)
{ return -ENODEV; }
#endif

#endif /* __ARCH_ARM_MACH_MSM_CPUIDLE_H */
