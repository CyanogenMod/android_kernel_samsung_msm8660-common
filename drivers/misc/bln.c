/* drivers/misc/bln.c
 *
 * Copyright 2011  Michael Richter (alias neldar)
 * Copyright 2011  Adam Kent <adam@semicircular.net>
 * Copyright 2012  Jeffrey Clark <h0tw1r3@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/earlysuspend.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/bln.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/wakelock.h>

static bool bln_enabled = false;		/* bln available */
static bool bln_ongoing = false;		/* notification currently active */
static int bln_led_state = 0;
static int bln_blink_state = 0;
static bool bln_suspended = false;		/* is system suspended */
static uint32_t blink_count;
static uint32_t blink_on_msec = 500;
static uint32_t blink_off_msec = 500;
static uint32_t max_blink_count = 300;

static struct bln_implementation *bln_imp = NULL;
static struct wake_lock bln_wake_lock;

void bl_timer_callback(unsigned long data);
static struct timer_list blink_timer =
		TIMER_INITIALIZER(bl_timer_callback, 0, 0);
static void blink_callback(struct work_struct *blink_work);
static DECLARE_WORK(blink_work, blink_callback);

#define BACKLIGHTNOTIFICATION_VERSION 10

static void bln_led_on(void)
{
	bln_imp->on();
	bln_led_state = 1;
}

static void bln_led_off(void)
{
	bln_imp->off();
	bln_led_state = 0;
}

static void bln_early_suspend(struct early_suspend *h)
{
	bln_suspended = true;
}

static void bln_late_resume(struct early_suspend *h)
{
	bln_suspended = false;
}

static struct early_suspend bln_suspend_data = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1,
	.suspend = bln_early_suspend,
	.resume = bln_late_resume,
};

static void bln_blink_start(void)
{
	if (bln_ongoing && !bln_blink_state) {
		wake_lock(&bln_wake_lock);

		bln_blink_state = 1;

		blink_timer.expires = jiffies +
			msecs_to_jiffies(blink_on_msec);
		blink_count = max_blink_count;

		add_timer(&blink_timer);

		pr_info("%s: complete\n", __FUNCTION__);
	} else {
		pr_notice("%s: fail (ongoing=%c, blink_state=%c)\n", __FUNCTION__, bln_ongoing, bln_blink_state);
	}
}

static void bln_blink_stop(void)
{
	if (bln_blink_state) {
		if (timer_pending(&blink_timer))
			del_timer_sync(&blink_timer);

		if (wake_lock_active(&bln_wake_lock))
			wake_unlock(&bln_wake_lock);

		bln_blink_state = 0;

		if (bln_ongoing)
			bln_led_on();

		pr_info("%s: complete\n", __FUNCTION__);
	} else {
		pr_notice("%s: fail (blink_state=%c)\n", __FUNCTION__, bln_blink_state);
	}
}

static void enable_led_notification(void)
{
	if ( ! (!bln_enabled || bln_ongoing || !bln_suspended) ) {
		if (bln_imp->enable()) {
			bln_led_on();

			bln_ongoing = true;

			pr_info("%s: success\n", __FUNCTION__);
			return;
		}
	}

	pr_notice("%s: fail (enabled=%c ongoing=%c suspended=%c\n", __FUNCTION__, bln_enabled, bln_ongoing, bln_suspended);
}

static void disable_led_notification(void)
{
	if (bln_ongoing) {
		bln_blink_stop();
		bln_led_off();
		bln_imp->disable();

		bln_ongoing = false;

		pr_debug("%s: success\n", __FUNCTION__);
	} else {
		pr_notice("%s: fail (ongoing=%c\n", __FUNCTION__, bln_ongoing);
	}
}

static ssize_t enabled_status_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", (bln_enabled ? 1 : 0));
}

static ssize_t enabled_status_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;
	if(sscanf(buf, "%u\n", &data) == 1) {
		pr_devel("%s: %u \n", __FUNCTION__, data);
		if (data == 1) {
			pr_debug("%s: bln support enabled\n", __FUNCTION__);
			bln_enabled = true;
		} else if (data == 0) {
			bln_enabled = false;
			disable_led_notification();
			pr_debug("%s: bln support disabled\n", __FUNCTION__);
		} else {
			pr_err("%s: invalid input %u\n", __FUNCTION__,
					data);
		}
	} else {
		pr_err("%s: invalid input\n", __FUNCTION__);
	}

	return size;
}

static ssize_t notification_led_status_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%u\n", (bln_ongoing ? 1 : 0));
}

static ssize_t notification_led_status_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (sscanf(buf, "%u\n", &data) == 1) {
		if (data == 1)
			enable_led_notification();
		else if (data == 0)
			disable_led_notification();
		else
			pr_err("%s: wrong input %u\n", __FUNCTION__, data);
	} else {
		pr_err("%s: input error\n", __FUNCTION__);
	}

	return size;
}

static ssize_t blink_interval_status_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%u %u\n", blink_on_msec, blink_off_msec);
}

static ssize_t blink_interval_status_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int ms_on, ms_off;
	int c;

	c = sscanf(buf, "%u %u\n", &ms_on, &ms_off);
	if (c == 1 || c == 2) {
		blink_on_msec = ms_on;
		blink_off_msec = (c == 2) ? ms_off : ms_on;
	} else {
		pr_err("%s: invalid input\n", __FUNCTION__);
	}

	return size;
}

static ssize_t max_blink_count_status_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%u\n", max_blink_count);
}

static ssize_t max_blink_count_status_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (sscanf(buf, "%u\n", &data) == 1)
		max_blink_count = data;
	else
		pr_err("%s: invalid input\n", __FUNCTION__);

	return size;
}

static ssize_t blink_control_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", bln_blink_state);
}

static ssize_t blink_control_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	if (sscanf(buf, "%u\n", &data) == 1) {
		if (data == 1) {
			bln_blink_start();
		} else if (data == 0) {
			bln_blink_stop();
		} else {
			pr_err("%s: input error %u\n", __FUNCTION__, data);
		}
	} else {
		pr_err("%s: input error\n", __FUNCTION__);
	}

	return size;
}

static ssize_t blink_count_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", blink_count);
}

static ssize_t version_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", BACKLIGHTNOTIFICATION_VERSION);
}

static DEVICE_ATTR(blink_control, S_IRUGO | S_IWUGO, blink_control_read,
		blink_control_write);
static DEVICE_ATTR(enabled, S_IRUGO | S_IWUGO,
		enabled_status_read,
		enabled_status_write);
static DEVICE_ATTR(notification_led, S_IRUGO | S_IWUGO,
		notification_led_status_read,
		notification_led_status_write);
static DEVICE_ATTR(blink_interval, S_IRUGO | S_IWUGO,
		blink_interval_status_read,
		blink_interval_status_write);
static DEVICE_ATTR(max_blink_count, S_IRUGO | S_IWUGO,
		max_blink_count_status_read,
		max_blink_count_status_write);
static DEVICE_ATTR(blink_count, S_IRUGO , blink_count_read, NULL);
static DEVICE_ATTR(version, S_IRUGO , version_read, NULL);

static struct attribute *bln_notification_attributes[] = {
	&dev_attr_blink_control.attr,
	&dev_attr_enabled.attr,
	&dev_attr_notification_led.attr,
	&dev_attr_blink_interval.attr,
	&dev_attr_max_blink_count.attr,
	&dev_attr_blink_count.attr,
	&dev_attr_version.attr,
	NULL
};

static struct attribute_group bln_notification_group = {
	.attrs  = bln_notification_attributes,
};

static struct miscdevice bln_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "backlightnotification",
};

void register_bln_implementation(struct bln_implementation *imp)
{
	bln_imp = imp;
}
EXPORT_SYMBOL(register_bln_implementation);

void cancel_bln_activity(void)
{
	disable_led_notification();
}
EXPORT_SYMBOL(cancel_bln_activity);

static void blink_callback(struct work_struct *blink_work)
{
	if (bln_led_state) {
		if (--blink_count == 0) {
			pr_notice("%s: notification led time out\n", __FUNCTION__);
			disable_led_notification();
			return;
		}
		bln_led_off();
	} else {
		bln_led_on();
	}
}

void bl_timer_callback(unsigned long data)
{
	schedule_work(&blink_work);
	mod_timer(&blink_timer, jiffies + msecs_to_jiffies((bln_led_state) ? blink_off_msec : blink_on_msec));
}

static int __init bln_control_init(void)
{
	int ret;

	pr_info("%s misc_register(%s)\n", __FUNCTION__, bln_device.name);
	ret = misc_register(&bln_device);
	if (ret) {
		pr_err("%s misc_register(%s) fail\n", __FUNCTION__,
				bln_device.name);
		return 1;
	}

	/* add the bln attributes */
	if (sysfs_create_group(&bln_device.this_device->kobj,
				&bln_notification_group) < 0) {
		pr_err("%s sysfs_create_group fail\n", __FUNCTION__);
		pr_err("Failed to create sysfs group for device (%s)!\n",
				bln_device.name);
	}

	register_early_suspend(&bln_suspend_data);

	/* Initialize wake locks */
	wake_lock_init(&bln_wake_lock, WAKE_LOCK_SUSPEND, "bln_wake");

	return 0;
}

device_initcall(bln_control_init);
