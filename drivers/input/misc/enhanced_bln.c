/*
 * drivers/input/misc/enhanced_bln.c
 *
 * Enhanced Backlight Notifications (EBLN)
 *
 * Copyright (C) 2015, Sultanxda <sultanxda@gmail.com>
 * Copyright (C) 2015, Emmanuel Utomi <emmanuelutomi@gmail.com>
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

#define pr_fmt(fmt) "EBLN: " fmt

#include <linux/device.h>
#include <linux/earlysuspend.h>
#include <linux/enhanced_bln.h>
#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/wakelock.h>

/**
 * struct ebln_config - EBLN working variables
 *
 * @always_on:		Keeps the backlight LEDs on without blinking them, and
 *			disables the wakelock. Internal working variable that
 *			can be triggered indirectly by userspace.
 *
 * @blink_control:	On/off switch for EBLN. Controlled by userspace.
 *
 * @blink_timeout_ms:	Timeout in milliseconds that EBLN will stop blinking the backlights
 *			and switch to always_on mode to conserve power. Controlled
 *			by end user.
 *
 * @off_ms:		Frequency in milliseconds to keep the backlights off while
 *			blinking them. Controlled by userspace.
 *
 * @on_ms:		Frequency in milliseconds to keep the backlights on while
 *			blinking them. Controlled by userspace.
 *
 */
static struct ebln_config {
	bool always_on;
	unsigned int blink_control;
	unsigned int blink_timeout_ms;
	unsigned int off_ms;
	unsigned int on_ms;
	unsigned int override_off_ms;
	unsigned int override_on_ms;
} ebln_conf = {
	.always_on = false,
	.blink_control = 0,
	.blink_timeout_ms = 600000,
	.off_ms = 0,
	.on_ms = 0,
	.override_off_ms = 0,
	.override_on_ms = 0,
};

static struct ebln_implementation *ebln_imp = NULL;
static struct delayed_work ebln_main_work;
static struct wake_lock ebln_wake_lock;

static bool blink_callback;
static bool suspended;

static u64 ebln_start_time;

static void set_ebln_state(unsigned int ebln_state)
{
	switch (ebln_state) {
	case EBLN_OFF:
		if (ebln_conf.blink_control) {
			ebln_conf.blink_control = EBLN_OFF;
			cancel_delayed_work_sync(&ebln_main_work);
			ebln_conf.always_on = false;
			ebln_imp->led_off(EBLN_OFF);
			if (suspended)
				ebln_imp->disable_led_reg();
			if (wake_lock_active(&ebln_wake_lock))
				wake_unlock(&ebln_wake_lock);
		}
		break;
	case EBLN_ON:
		if (!ebln_conf.blink_control) {
			wake_lock(&ebln_wake_lock);
			ebln_start_time = ktime_to_ms(ktime_get());
			ebln_imp->enable_led_reg();
			ebln_conf.blink_control = EBLN_ON;
			blink_callback = false;
			schedule_delayed_work(&ebln_main_work, 0);
		}
		break;
	}
}

static void ebln_main(struct work_struct *work)
{
	unsigned int blink_ms;
	u64 now;

	if (ebln_conf.blink_control) {
		if (blink_callback) {
			blink_callback = false;
			if (ebln_conf.blink_timeout_ms && !ebln_conf.always_on) {
				now = ktime_to_ms(ktime_get());
				if ((now - ebln_start_time) >= ebln_conf.blink_timeout_ms)
					ebln_conf.always_on = true;
			}
			if (ebln_conf.always_on) {
				wake_unlock(&ebln_wake_lock);
				return;
			}
			blink_ms = ((ebln_conf.override_on_ms && ebln_conf.override_off_ms) ? ebln_conf.override_off_ms : ebln_conf.off_ms);
			ebln_imp->led_off(EBLN_BLINK_OFF);
		} else {
			blink_callback = true;
			blink_ms = ((ebln_conf.override_on_ms && ebln_conf.override_off_ms) ? ebln_conf.override_on_ms : ebln_conf.on_ms);
			ebln_imp->led_on();
		}

		schedule_delayed_work(&ebln_main_work, msecs_to_jiffies(blink_ms));
	}
}

static void ebln_early_suspend(struct early_suspend *h)
{
	suspended = true;
}

static void ebln_late_resume(struct early_suspend *h)
{
	suspended = false;
}

static struct early_suspend ebln_early_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
	.suspend = ebln_early_suspend,
	.resume = ebln_late_resume,
};

void register_ebln_implementation(struct ebln_implementation *imp)
{
	ebln_imp = imp;
}

/**************************** SYSFS START ****************************/
static ssize_t blink_control_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;
	int ret = sscanf(buf, "%u", &data);

	if (ret != 1)
		return -EINVAL;

	if (ebln_imp == NULL) {
		pr_err("No EBLN implementation found, EBLN blink failed\n");
		return size;
	}

	/* Only happens when BLN is enabled without any initial notification parameters */
	if(unlikely(!ebln_conf.on_ms && !ebln_conf.off_ms)){
		ebln_conf.always_on = true;
	}

	if (data)
		set_ebln_state(EBLN_ON);
	else
		set_ebln_state(EBLN_OFF);

	return size;
}

static ssize_t blink_interval_ms_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int on_ms, off_ms;
	int ret = sscanf(buf, "%u %u", &on_ms, &off_ms);

	if (ret != 2)
		return -EINVAL;

	ebln_conf.on_ms = on_ms;
	ebln_conf.off_ms = off_ms;

	if (!ebln_conf.off_ms && ebln_conf.on_ms == 1)
		ebln_conf.always_on = true;

	ebln_start_time = ktime_to_ms(ktime_get());

	/* break out of always-on mode */
	if (ebln_conf.always_on && (ebln_conf.off_ms || ebln_conf.on_ms > 1)) {
		cancel_delayed_work_sync(&ebln_main_work);
		wake_lock(&ebln_wake_lock);
		ebln_conf.always_on = false;
		schedule_delayed_work(&ebln_main_work, 0);
	} else if (delayed_work_pending(&ebln_main_work) && ebln_conf.blink_control) {
		cancel_delayed_work_sync(&ebln_main_work);
		schedule_delayed_work(&ebln_main_work, 0);
	}

	return size;
}

static ssize_t blink_timeout_ms_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;
	int ret = sscanf(buf, "%u", &data);

	if (ret != 1)
		return -EINVAL;

	ebln_conf.blink_timeout_ms = data;

	return size;
}

static ssize_t blink_timeout_ms_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", ebln_conf.blink_timeout_ms);
}

static ssize_t override_blink_interval_ms_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%u %u\n", ebln_conf.override_on_ms, ebln_conf.override_off_ms);
}

static ssize_t override_blink_interval_ms_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int on_ms, off_ms;

	int ret = sscanf(buf, "%u %u\n", &on_ms, &off_ms);
	if ((ret == 1 || ret == 2) && on_ms && off_ms) {
		ebln_conf.override_on_ms = on_ms;
		ebln_conf.override_off_ms = off_ms;
	} else {
		ebln_conf.override_on_ms = 0;
		ebln_conf.override_off_ms = 0;
	}

	/* break out of always-on mode */
	if (ebln_conf.always_on && (ebln_conf.override_off_ms || ebln_conf.override_on_ms > 1)) {
		cancel_delayed_work_sync(&ebln_main_work);
		wake_lock(&ebln_wake_lock);
		ebln_conf.always_on = false;
		schedule_delayed_work(&ebln_main_work, 0);
	}

	return size;
}

static DEVICE_ATTR(blink_control, S_IRUGO | S_IWUGO,
		NULL,
		blink_control_write);
static DEVICE_ATTR(blink_interval_ms, S_IRUGO | S_IWUGO,
		NULL,
		blink_interval_ms_write);
static DEVICE_ATTR(blink_override_interval_ms, S_IRUGO | S_IWUGO,
		override_blink_interval_ms_read,
		override_blink_interval_ms_write);
static DEVICE_ATTR(blink_timeout_ms, S_IRUGO | S_IWUGO,
		blink_timeout_ms_read,
		blink_timeout_ms_write);

static struct attribute *ebln_attributes[] = {
	&dev_attr_blink_control.attr,
	&dev_attr_blink_interval_ms.attr,
	&dev_attr_blink_override_interval_ms.attr,
	&dev_attr_blink_timeout_ms.attr,
	NULL
};

static struct attribute_group ebln_attr_group = {
	.attrs  = ebln_attributes,
};

static struct miscdevice ebln_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "enhanced_bln",
};
/**************************** SYSFS END ****************************/

static int __init enhanced_bln_init(void)
{
	int ret;

	INIT_DELAYED_WORK(&ebln_main_work, ebln_main);

	ret = misc_register(&ebln_device);
	if (ret) {
		pr_err("Failed to register misc device!\n");
		goto err;
	}

	ret = sysfs_create_group(&ebln_device.this_device->kobj, &ebln_attr_group);
	if (ret) {
		pr_err("Failed to create sysfs group!\n");
		goto err;
	}

	wake_lock_init(&ebln_wake_lock, WAKE_LOCK_SUSPEND, "ebln_wake_lock");

	register_early_suspend(&ebln_early_suspend_handler);
err:
	return ret;
}
late_initcall(enhanced_bln_init);
