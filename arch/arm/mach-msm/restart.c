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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/reboot.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/mfd/pmic8058.h>
#include <linux/mfd/pmic8901.h>
#include <linux/mfd/pm8xxx/misc.h>

#include <asm/mach-types.h>
#include <mach/sec_debug.h>  // onlyjazz
#include <linux/notifier.h> // klaatu
#include <linux/ftrace.h> // klaatu

#include <mach/msm_iomap.h>
#include <mach/restart.h>
#include <mach/socinfo.h>
#include <mach/irqs.h>
#include <mach/scm.h>
#include "msm_watchdog.h"
#include "timer.h"

#define WDT0_RST	0x38
#define WDT0_EN		0x40
#define WDT0_BARK_TIME	0x4C
#define WDT0_BITE_TIME	0x5C

#define PSHOLD_CTL_SU (MSM_TLMM_BASE + 0x820)

#define RESTART_REASON_ADDR 0x65C
#define DLOAD_MODE_ADDR     0x0

#define RESTART_LPM_BOOT_MODE		0x77665506
#define RESTART_ARM11FOTA_MODE  	0x77665503
#define RESTART_RECOVERY_MODE   	0x77665502
#define RESTART_OTHERBOOT_MODE		0x77665501
#define RESTART_FASTBOOT_MODE   	0x77665500
#ifdef CONFIG_SEC_DEBUG
#define RESTART_SECDEBUG_MODE   	0x776655EE
#define RESTART_SECCOMMDEBUG_MODE   	0x7766DEAD
#endif
// NOT USE 0x776655FF~0x77665608 command
#define RESTART_HOMEDOWN_MODE       	0x776655FF
#define RESTART_HOMEDOWN_MODE_END   	0x77665608

#define SCM_IO_DISABLE_PMIC_ARBITER	1

static int restart_mode;
static void *restart_reason;

int pmic_reset_irq;
static void __iomem *msm_tmr0_base;

#ifdef CONFIG_MSM_DLOAD_MODE
static int in_panic;
static void *dload_mode_addr;

#if 0	/* onlyjazz.ef24 : intentionally remove it */
/* Download mode master kill-switch */
static int dload_set(const char *val, struct kernel_param *kp);
static int download_mode = 1;
module_param_call(download_mode, dload_set, param_get_int,
			&download_mode, 0644);
#endif	/* onlyjazz.ef24 : intentionally remove it */

static int panic_prep_restart(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	if(!sec_debug_is_enabled()) {
		printk(KERN_NOTICE "panic_prep_restart\n");
		return NOTIFY_DONE;
	}
	printk(KERN_NOTICE "panic_prep_restart, in_panic = 1\n");
	in_panic = 1;
	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call	= panic_prep_restart,
};

static void set_dload_mode(int on)
{
	if (dload_mode_addr) {
		__raw_writel(on ? 0xE47B337D : 0, dload_mode_addr);
		__raw_writel(on ? 0xCE14091A : 0,
		       dload_mode_addr + sizeof(unsigned int));
		mb();
		
		// klaatu
		pr_err("set_dload_mode <%d> ( %x )\n", on, CALLER_ADDR0);
	}
}

#if 0	/* onlyjazz.ef24 : intentionally remove it */
static int dload_set(const char *val, struct kernel_param *kp)
{
	int ret;
	int old_val = download_mode;

	ret = param_set_int(val, kp);

	if (ret)
		return ret;

	/* If download_mode is not zero or one, ignore. */
	if (download_mode >> 1) {
		download_mode = old_val;
		return -EINVAL;
	}

	set_dload_mode(download_mode);

	return 0;
}
#endif	/* onlyjazz.ef24 : intentionally remove it */
#else
#define set_dload_mode(x) do {} while (0)
#endif

void msm_set_restart_mode(int mode)
{
	restart_mode = mode;
}
EXPORT_SYMBOL(msm_set_restart_mode);

static void __msm_power_off(int lower_pshold)
{
	local_irq_disable();
	printk(KERN_CRIT "Powering off the SoC\n");
#ifdef CONFIG_MSM_DLOAD_MODE
	set_dload_mode(0);
#endif
	pm8xxx_reset_pwr_off(0);

	if (lower_pshold) {
		__raw_writel(0, PSHOLD_CTL_SU);
		mdelay(10000);
		printk(KERN_ERR "Powering off has failed\n");
	}
	local_irq_enable();
	return;
}

static void msm_power_off(void)
{
	/* MSM initiated power off, lower ps_hold */
	__msm_power_off(1);
}

static void cpu_power_off(void *data)
{
	int rc;

	pr_err("PMIC Initiated shutdown %s cpu=%d\n", __func__,
						smp_processor_id());
	if (smp_processor_id() == 0) {
		/*
		 * PMIC initiated power off, do not lower ps_hold, pmic will
		 * shut msm down
		 */
		__msm_power_off(0);

		pet_watchdog();
		pr_err("Calling scm to disable arbiter\n");
		/* call secure manager to disable arbiter and never return */
		rc = scm_call_atomic1(SCM_SVC_PWR,
						SCM_IO_DISABLE_PMIC_ARBITER, 1);

		pr_err("SCM returned even when asked to busy loop rc=%d\n", rc);
		pr_err("waiting on pmic to shut msm down\n");
	}

	preempt_disable();
	while (1)
		;
}

static irqreturn_t resout_irq_handler(int irq, void *dev_id)
{
	pr_warn("%s PMIC Initiated shutdown\n", __func__);
	oops_in_progress = 1;
	smp_call_function_many(cpu_online_mask, cpu_power_off, NULL, 0);
	if (smp_processor_id() == 0)
		cpu_power_off(NULL);
	preempt_disable();
	while (1)
		;
	return IRQ_HANDLED;
}

void arch_reset(char mode, const char *cmd)
{
#ifdef CONFIG_MSM_DLOAD_MODE

#ifdef CONFIG_SEC_DEBUG // klaatu
	if( sec_debug_is_enabled() && ((restart_mode == RESTART_DLOAD) || in_panic) )
		set_dload_mode(1);
	else
		set_dload_mode(0);
#else
	/* This looks like a normal reboot at this point. */
	set_dload_mode(0);

	/* Write download mode flags if we're panic'ing */
	set_dload_mode(in_panic);

	/* Write download mode flags if restart_mode says so */
	if (restart_mode == RESTART_DLOAD)
		set_dload_mode(1);
#endif

	#if 0	/* onlyjazz.ef24 : intentionally remove it */
	/* Kill download mode if master-kill switch is set */
	if (!download_mode)
		set_dload_mode(0);
	#endif 	/* onlyjazz.ef24 : intentionally remove it */

#endif

	printk(KERN_NOTICE "Going down for restart now\n");

	pm8xxx_reset_pwr_off(1);

        // TODO:  Never use RESTART_LPM_BOOT_MODE/0x77665506 as another restart reason instead of LPM mode.
        // RESTART_LPM_BOOT_MODE/0x77665506 is reserved for LPM mode.
	if (cmd != NULL) {
		if (!strncmp(cmd, "bootloader", 10)) {
			__raw_writel(RESTART_FASTBOOT_MODE, restart_reason);
		} else if (!strncmp(cmd, "recovery", 8)) {
			__raw_writel(RESTART_RECOVERY_MODE, restart_reason);
		} else if (!strncmp(cmd, "download", 8)) {
			unsigned long code = 0;
			strict_strtoul(cmd + 8, 16, &code);
			code = code & 0xff;
			__raw_writel(RESTART_HOMEDOWN_MODE + code, restart_reason);
		} else if (!strncmp(cmd, "oem-", 4)) {
			unsigned long code = 0;
			strict_strtoul(cmd + 4, 16, &code);
			code = code & 0xff;
			__raw_writel(0x6f656d00 | code, restart_reason);
#ifdef CONFIG_SEC_DEBUG
		} else if (!strncmp(cmd, "sec_debug_hw_reset", 18)) {
			__raw_writel(0x776655ee, restart_reason);
		} else if (!strncmp(cmd, "sec_debug_comm_dump", 19)) {
			__raw_writel(0x7766DEAD, restart_reason);
#endif
		} else if (!strncmp(cmd, "arm11_fota", 10)) {
			__raw_writel(RESTART_ARM11FOTA_MODE, restart_reason);
		} else {
			__raw_writel(RESTART_OTHERBOOT_MODE, restart_reason);
		}
	}
#ifdef CONFIG_SEC_DEBUG
	else {
		writel(0x12345678, restart_reason);    /* clear abnormal reset flag */
	}
#endif

#if 0 /* onlyjazz.el20 : it was commented out in celox gingerbread, according to the request from waro.park */
	__raw_writel(0, msm_tmr0_base + WDT0_EN);
	if (!(machine_is_msm8x60_fusion() || machine_is_msm8x60_fusn_ffa())) {
		mb();
		__raw_writel(0, PSHOLD_CTL_SU); /* Actually reset the chip */
		mdelay(5000);
		pr_notice("PS_HOLD didn't work, falling back to watchdog\n");
	}
#endif

#if 0  /* onlyjazz.el20 : give time for MDM to prepare upload mode to prevent mdm dump corruption. still required ?  */
	__raw_writel(0, msm_tmr0_base + WDT0_EN);
	mdelay(1000);
#endif

	__raw_writel(1, msm_tmr0_base + WDT0_RST);
	__raw_writel(5*0x31F3, msm_tmr0_base + WDT0_BARK_TIME);
	__raw_writel(0x31F3, msm_tmr0_base + WDT0_BITE_TIME);
	__raw_writel(1, msm_tmr0_base + WDT0_EN);

	mdelay(10000);
	printk(KERN_ERR "Restarting has failed\n");
}

#ifdef CONFIG_SEC_DEBUG // klaatu

static int dload_mode_normal_reboot_handler(struct notifier_block *nb,
				unsigned long l, void *p)
{
	set_dload_mode(0);

	return 0;
}

static struct notifier_block dload_reboot_block = {
	.notifier_call = dload_mode_normal_reboot_handler
};
#endif

static int __init msm_restart_init(void)
{
	int rc;

#ifdef CONFIG_MSM_DLOAD_MODE
	atomic_notifier_chain_register(&panic_notifier_list, &panic_blk);
	dload_mode_addr = MSM_IMEM_BASE + DLOAD_MODE_ADDR;

#ifdef CONFIG_SEC_DEBUG // klaatu
	register_reboot_notifier(&dload_reboot_block);
#endif
	/* Reset detection is switched on below.*/
	if( sec_debug_is_enabled() )
		set_dload_mode(1);
	else
		set_dload_mode(0);
#endif
	msm_tmr0_base = msm_timer_get_timer0_base();
	restart_reason = MSM_IMEM_BASE + RESTART_REASON_ADDR;
	pm_power_off = msm_power_off;

	if (pmic_reset_irq != 0) {
		rc = request_any_context_irq(pmic_reset_irq,
					resout_irq_handler, IRQF_TRIGGER_HIGH,
					"restart_from_pmic", NULL);
		if (rc < 0)
			pr_err("pmic restart irq fail rc = %d\n", rc);
	} else {
		pr_warn("no pmic restart interrupt specified\n");
	}

	return 0;
}

late_initcall(msm_restart_init);
