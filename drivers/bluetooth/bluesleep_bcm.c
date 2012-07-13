/*

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2 as
   published by the Free Software Foundation.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.


   Copyright (C) 2006-2007 - Motorola
   Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.

   Date         Author           Comment
   -----------  --------------   --------------------------------
   2006-Apr-28	Motorola	 The kernel module for running the Bluetooth(R)
				 Sleep-Mode Protocol from the Host side
   2006-Sep-08  Motorola         Added workqueue for handling sleep work.
   2007-Jan-24  Motorola         Added mbm_handle_ioi() call to ISR.

*/

#include <linux/module.h>	/* kernel module definitions */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>

#include <linux/irq.h>
#include <linux/param.h>
#include <linux/bitops.h>
#include <linux/termios.h>
#include <linux/wakelock.h>
#include <mach/gpio.h>
#include <mach/msm_serial_hs.h>

#include <net/bluetooth/bluetooth.h>
#include <net/bluetooth/hci_core.h> /* event notifications */
#include "hci_uart.h"

#define BT_SLEEP_DBG
#ifndef BT_SLEEP_DBG
#define BT_DBG(fmt, arg...)
#endif
/*
 * Defines
 */

#define VERSION		"1.1"
#define PROC_DIR	"bluetooth/sleep"

struct bluesleep_info {
	unsigned host_wake;
	unsigned ext_wake;
	unsigned host_wake_irq;
	struct uart_port *uport;
    struct wake_lock wake_lock;
	char wake_lock_name[100];
};

/* work function */
static void bluesleep_sleep_work(struct work_struct *work);

/* work queue */
DECLARE_DELAYED_WORK(sleep_workqueue, bluesleep_sleep_work);

/* Macros for handling sleep work */
#define bluesleep_rx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_busy()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_rx_idle()     schedule_delayed_work(&sleep_workqueue, 0)
#define bluesleep_tx_idle()     schedule_delayed_work(&sleep_workqueue, 0)

/* 1 second timeout */
#define TX_TIMER_INTERVAL	3

/* state variable names and bit positions */
#define BT_ASLEEP	0x01

/* global pointer to a single hci device. */
static struct hci_dev *bluesleep_hdev;

static struct bluesleep_info *bsi;

/*
 * Local function prototypes
 */

static int bluesleep_hci_event(struct notifier_block *this,
			    unsigned long event, void *data);

/*
 * Global variables
 */

/** Global state flags */
static unsigned long flags;

/** Tasklet to respond to change in hostwake line */
static struct tasklet_struct hostwake_task;

/** Transmission timer */
static struct timer_list tx_timer;

/** Lock for state transitions */
static spinlock_t rw_lock;

/** Notifier block for HCI events */
struct notifier_block hci_event_nblock = {
	.notifier_call = bluesleep_hci_event,
};

/*
 * Local functions
 */

static void hsuart_power(int on)
{
	if (bsi->uport == NULL ){
			printk(KERN_ERR "[BT] hsuart_power...but bsi->uport == NULL , so return");
		return ;
	}

	printk(KERN_ERR "[BT] hsuart_power ON set (%d)\n",on);
	if (on) {
		msm_hs_request_clock_on(bsi->uport);
		msm_hs_set_mctrl(bsi->uport, TIOCM_RTS);
	} else {
/* SS_BLUETOOTH(john.lim) 2012.05.31 */ 
/* P120531-0686 kernel panic */
		if (bsi->uport != NULL )
 		{
			msm_hs_set_mctrl(bsi->uport, 0);
			msm_hs_request_clock_off(bsi->uport);
		}
	}
}

/**
 * @return 1 if the Host can go to sleep, 0 otherwise.
 */
static inline int bluesleep_can_sleep(void)
{
	/* check if BT_WAKE_GPIO is deasserted */
	return !gpio_get_value(bsi->host_wake) &&
		(bsi->uport != NULL);
}

void bluesleep_sleep_wakeup(void)
{
	if (test_bit(BT_ASLEEP, &flags) && bsi->uport != NULL) {
		printk(KERN_ERR "[BT] waking up...");
        /* Start the timer */
        mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
                clear_bit(BT_ASLEEP, &flags);
                /*Activating UART */
                hsuart_power(1);
		wake_lock(&bsi->wake_lock);
	} else
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
}

/**
 * @brief@  main sleep work handling function which update the flags
 * and activate and deactivate UART ,check FIFO.
 */
static void bluesleep_sleep_work(struct work_struct *work)
{
	if (bluesleep_can_sleep()) {
		if (test_bit(BT_ASLEEP, &flags)) {
			printk(KERN_ERR "[BT] already asleep");
			return;
		}

            mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));
	} else
		bluesleep_sleep_wakeup();
	}

/**
 * A tasklet function that runs in tasklet context and reads the value
 * of the HOST_WAKE GPIO pin and further defer the work.
 * @param data Not used.
 */
static void bluesleep_hostwake_task(unsigned long data)
{
    printk(KERN_ERR "[BT] bluesleep_hostwake_task-hostwake -> %u", gpio_get_value(bsi->host_wake));

	spin_lock(&rw_lock);

	if (gpio_get_value(bsi->host_wake))
		bluesleep_rx_busy();
	else
		bluesleep_rx_idle();

	spin_unlock(&rw_lock);
}

static void bluesleep_lpm_exit_lpm_locked(struct hci_dev *hdev)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	gpio_set_value(bsi->ext_wake, 1);
	bluesleep_sleep_wakeup();

	spin_unlock_irqrestore(&rw_lock, irq_flags);
}

/**
 * Handles HCI device events.
 * @param this Not used.
 * @param event The event that occurred.
 * @param data The HCI device associated with the event.
 * @return <code>NOTIFY_DONE</code>.
 */
static int bluesleep_hci_event(struct notifier_block *this,
				unsigned long event, void *data)
{
	struct hci_dev *hdev = (struct hci_dev *) data;
	struct hci_uart *hu;
	struct uart_state *state;

	if (!hdev)
		return NOTIFY_DONE;

	switch (event) {
	case HCI_DEV_REG:
		if (!bluesleep_hdev) {
			bluesleep_hdev = hdev;
			hu  = (struct hci_uart *) hdev->driver_data;
			state = (struct uart_state *) hu->tty->driver_data;
			bsi->uport = state->uart_port;
			hdev->wake_peer = bluesleep_lpm_exit_lpm_locked;
			printk(KERN_ERR "[BT] wake_peer is registered.\n");
		}
		break;
	case HCI_DEV_UNREG:
		printk(KERN_ERR "[BT] wake_peer is unregistered.\n");
		gpio_set_value(bsi->ext_wake, 0);
		wake_lock_timeout(&bsi->wake_lock, HZ);
		bluesleep_hdev = NULL;
		bsi->uport = NULL;
		break;
	}

	return NOTIFY_DONE;
}

/**
 * Handles transmission timer expiration.
 * @param data Not used.
 */
static void bluesleep_tx_timer_expire(unsigned long data)
{
	unsigned long irq_flags;

	spin_lock_irqsave(&rw_lock, irq_flags);

	if (bluesleep_can_sleep()) {
		printk(KERN_ERR "[BT] going to sleep...\n");
		set_bit(BT_ASLEEP, &flags);
		gpio_set_value(bsi->ext_wake, 0);
		/*Deactivating UART */
		hsuart_power(0);
		wake_lock_timeout(&bsi->wake_lock, HZ);
	} else
		mod_timer(&tx_timer, jiffies + (TX_TIMER_INTERVAL * HZ));

	spin_unlock_irqrestore(&rw_lock, irq_flags);
}

/**
 * Schedules a tasklet to run when receiving an interrupt on the
 * <code>HOST_WAKE</code> GPIO pin.
 * @param irq Not used.
 * @param dev_id Not used.
 */
static irqreturn_t bluesleep_hostwake_isr(int irq, void *dev_id)
{
	tasklet_schedule(&hostwake_task);
	return IRQ_HANDLED;
}

static int __init bluesleep_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	bsi = kzalloc(sizeof(struct bluesleep_info), GFP_KERNEL);
	if (!bsi)
		return -ENOMEM;

	snprintf(bsi->wake_lock_name, sizeof(bsi->wake_lock_name),
			"BTLowPower");
	wake_lock_init(&bsi->wake_lock, WAKE_LOCK_SUSPEND,
			 bsi->wake_lock_name);

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_host_wake");
	if (!res) {
		printk(KERN_ERR "[BT] couldn't find host_wake gpio\n");
		ret = -ENODEV;
		goto free_bsi;
	}
	bsi->host_wake = res->start;

	ret = gpio_request(bsi->host_wake, "bt_host_wake");
	if (ret)
		goto free_bsi;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
				"gpio_ext_wake");
	if (!res) {
		printk(KERN_ERR "[BT] couldn't find ext_wake gpio\n");
		ret = -ENODEV;
		goto free_bt_host_wake;
	}
	bsi->ext_wake = res->start;

	ret = gpio_request(bsi->ext_wake, "bt_ext_wake");
	if (ret)
		goto free_bt_host_wake;

	bsi->host_wake_irq = platform_get_irq_byname(pdev, "host_wake");
	if (bsi->host_wake_irq < 0) {
		printk(KERN_ERR "[BT] couldn't find host_wake irq\n");
		ret = -ENODEV;
		goto free_bt_ext_wake;
	}

	ret = request_irq(bsi->host_wake_irq, bluesleep_hostwake_isr,
				IRQF_DISABLED | IRQF_TRIGGER_FALLING |
				IRQF_TRIGGER_RISING,
				"bt host_wake", NULL);
	if (ret < 0) {
		printk(KERN_ERR "[BT] Couldn't acquire BT_HOST_WAKE IRQ");
		ret = -ENODEV;
		goto free_bt_ext_wake;
	}

	ret = enable_irq_wake(bsi->host_wake_irq);
	if (ret < 0) {
		printk(KERN_ERR "[BT] Set_irq_wake failed.\n");
		goto free_bt_host_wake_irq;
	}

	gpio_tlmm_config(GPIO_CFG(bsi->ext_wake, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(bsi->host_wake, 0, GPIO_CFG_INPUT,
		GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE);

	gpio_direction_input(bsi->host_wake);
	gpio_direction_output(bsi->ext_wake, 0);

	return 0;

free_bt_host_wake_irq:
	free_irq(bsi->host_wake_irq, NULL);
free_bt_ext_wake:
	gpio_free(bsi->ext_wake);
free_bt_host_wake:
	gpio_free(bsi->host_wake);
free_bsi:
	kfree(bsi);
	return ret;
}

static int bluesleep_remove(struct platform_device *pdev)
{
	/* assert bt wake */
	gpio_set_value(bsi->ext_wake, 0);
		if (disable_irq_wake(bsi->host_wake_irq))
			printk(KERN_ERR "[BT] Couldn't disable hostwake IRQ wakeup mode \n");
		free_irq(bsi->host_wake_irq, NULL);
		del_timer(&tx_timer);
		if (test_bit(BT_ASLEEP, &flags))
			hsuart_power(1);
	gpio_free(bsi->host_wake);
	gpio_free(bsi->ext_wake);
	kfree(bsi);
	return 0;
}

static struct platform_driver bluesleep_driver = {
	.remove = bluesleep_remove,
	.driver = {
		.name = "bluesleep_bcm",
		.owner = THIS_MODULE,
	},
};
/**
 * Initializes the module.
 * @return On success, 0. On error, -1, and <code>errno</code> is set
 * appropriately.
 */
static int __init bluesleep_init(void)
{
	int retval;

	printk(KERN_ERR "[BT] bluesleep_init Ver %s", VERSION);

	retval = platform_driver_probe(&bluesleep_driver, bluesleep_probe);
	if (retval)
		return retval;

	bluesleep_hdev = NULL;

	flags = 0; /* clear all status bits */

	/* Initialize spinlock. */
	spin_lock_init(&rw_lock);

	/* Initialize timer */
	init_timer(&tx_timer);
	tx_timer.function = bluesleep_tx_timer_expire;
	tx_timer.data = 0;

	/* initialize host wake tasklet */
	tasklet_init(&hostwake_task, bluesleep_hostwake_task, 0);

	hci_register_notifier(&hci_event_nblock);

	return 0;
}

/**
 * Cleans up the module.
 */
static void __exit bluesleep_exit(void)
{
	hci_unregister_notifier(&hci_event_nblock);
	platform_driver_unregister(&bluesleep_driver);
}

module_init(bluesleep_init);
module_exit(bluesleep_exit);

MODULE_DESCRIPTION("Bluetooth Sleep Mode Driver ver %s " VERSION);
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif
