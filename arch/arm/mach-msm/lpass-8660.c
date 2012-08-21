/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/stringify.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <mach/irqs.h>
#include <mach/scm.h>
#include <mach/peripheral-loader.h>
#include <mach/subsystem_restart.h>
#include <mach/subsystem_notif.h>

#include "smd_private.h"
#include "modem_notifier.h"
#include "ramdump.h"

#define Q6SS_WDOG_ENABLE		0x28882024
#define Q6SS_SOFT_INTR_WAKEUP		0x288A001C
#define MODULE_NAME			"lpass_8x60"
#define SCM_Q6_NMI_CMD			0x1
void __iomem *q6_wakeup_intr;  /* CONFIG_SEC_DEBUG */

/* Subsystem restart: QDSP6 data, functions */
static void *q6_ramdump_dev;
static void q6_fatal_fn(struct work_struct *);
static DECLARE_WORK(q6_fatal_work, q6_fatal_fn);

static void q6_fatal_fn(struct work_struct *work)
{
	pr_err("%s: Watchdog bite received from Q6!\n", MODULE_NAME);
	subsystem_restart("lpass");
	enable_irq(LPASS_Q6SS_WDOG_EXPIRED);
}

static void send_q6_nmi(void)
{
	/* Send NMI to QDSP6 via an SCM call. */
	uint32_t cmd = 0x1;
	//void __iomem *q6_wakeup_intr;  /* CONFIG_SEC_DEBUG */

	scm_call(SCM_SVC_UTIL, SCM_Q6_NMI_CMD,
	&cmd, sizeof(cmd), NULL, 0);

	/* Wakeup the Q6 */
    /* CONFIG_SEC_DEBUG start*/
	//q6_wakeup_intr = ioremap_nocache(Q6SS_SOFT_INTR_WAKEUP, 8);
	if (q6_wakeup_intr)
		writel_relaxed(0x2000, q6_wakeup_intr);
	//iounmap(q6_wakeup_intr);
    /* CONFIG_SEC_DEBUG end*/
	mb();

	/* Q6 requires atleast 100ms to dump caches etc.*/
	if (in_interrupt() | in_atomic()) { /* CONFIG_SEC_DEBUG */
	    unsigned long __ms=(100); 
	    while (__ms--) udelay(1000);
	}else {
		msleep(100);
 	}
	pr_info("subsystem-fatal-8x60: Q6 NMI was sent.\n");
}

int subsys_q6_shutdown(const struct subsys_data *crashed_subsys)
{
	void __iomem *q6_wdog_addr =
		ioremap_nocache(Q6SS_WDOG_ENABLE, 8);

	send_q6_nmi();
	writel_relaxed(0x0, q6_wdog_addr);
	/* The write needs to go through before the q6 is shutdown. */
	mb();
	iounmap(q6_wdog_addr);

	pil_force_shutdown("q6");
	disable_irq_nosync(LPASS_Q6SS_WDOG_EXPIRED);

	if (get_restart_level() == RESET_SUBSYS_MIXED)
		smsm_reset_modem(SMSM_RESET);

	return 0;
}

int subsys_q6_powerup(const struct subsys_data *crashed_subsys)
{
	int ret = pil_force_boot("q6");
	enable_irq(LPASS_Q6SS_WDOG_EXPIRED);
	return ret;
}

/* FIXME: Get address, size from PIL */
static struct ramdump_segment q6_segments[] = { {0x46700000, 0x47F00000 -
					0x46700000}, {0x28400000, 0x12800} };
static int subsys_q6_ramdump(int enable,
				const struct subsys_data *crashed_subsys)
{
	if (enable)
		return do_ramdump(q6_ramdump_dev, q6_segments,
				ARRAY_SIZE(q6_segments));
	else
		return 0;
}

void subsys_q6_crash_shutdown(const struct subsys_data *crashed_subsys)
{
	send_q6_nmi();
}

static irqreturn_t lpass_wdog_bite_irq(int irq, void *dev_id)
{
	int ret;

	ret = schedule_work(&q6_fatal_work);
	disable_irq_nosync(LPASS_Q6SS_WDOG_EXPIRED);

	return IRQ_HANDLED;
}

static struct subsys_data subsys_8x60_q6 = {
	.name = "lpass",
	.shutdown = subsys_q6_shutdown,
	.powerup = subsys_q6_powerup,
	.ramdump = subsys_q6_ramdump,
	.crash_shutdown = subsys_q6_crash_shutdown
};

static void __exit lpass_fatal_exit(void)
{
	iounmap(q6_wakeup_intr);  /* CONFIG_SEC_DEBUG */
	free_irq(LPASS_Q6SS_WDOG_EXPIRED, NULL);
}

static int __init lpass_fatal_init(void)
{
	int ret;

	ret = request_irq(LPASS_Q6SS_WDOG_EXPIRED, lpass_wdog_bite_irq,
			IRQF_TRIGGER_RISING, "q6_wdog", NULL);

	if (ret < 0) {
		pr_err("%s: Unable to request LPASS_Q6SS_WDOG_EXPIRED irq.",
			__func__);
		goto out;
	}
	q6_wakeup_intr = ioremap_nocache(Q6SS_SOFT_INTR_WAKEUP, 8); /* CONFIG_SEC_DEBUG */
	if (!q6_wakeup_intr)
		pr_err("%s: Unable to request q6 wakeup interrupt\n", __func__);

	q6_ramdump_dev = create_ramdump_device("lpass");

	if (!q6_ramdump_dev) {
		ret = -ENOMEM;
		iounmap(q6_wakeup_intr);  /* CONFIG_SEC_DEBUG */
		goto out;
	}

	ret = ssr_register_subsystem(&subsys_8x60_q6);
out:
	return ret;
}

module_init(lpass_fatal_init);
module_exit(lpass_fatal_exit);

