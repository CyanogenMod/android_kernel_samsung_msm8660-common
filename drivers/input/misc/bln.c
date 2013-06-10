/*
 * drivers/misc/bln.c
 *
 */

// Enable the pr_debug() prints
#define DEBUG 1

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
static bool bln_led_state = false;
static bool bln_blink_state = false;
static bool bln_suspended = false;		/* is system suspended */

static uint32_t blink_count;
static uint32_t blink_on_msec = 500;
static uint32_t blink_off_msec = 500;
static uint32_t max_blink_count = 300;
static struct mutex bln_mutex;

static struct bln_implementation *bln_imp = NULL;
static struct wake_lock bln_wake_lock;

void bl_timer_callback(unsigned long data);
static struct timer_list blink_timer =
		TIMER_INITIALIZER(bl_timer_callback, 0, 0);
static void blink_callback(struct work_struct *blink_work);
static DECLARE_WORK(blink_work, blink_callback);

#define BACKLIGHTNOTIFICATION_VERSION 11

static void bln_led_enable(bool enable)
{
	if (enable == 0 && !bln_led_state) {
		pr_debug("%s: already off\n", __FUNCTION__);
		return;
	}

	else if (enable == 1 && bln_led_state) {
		pr_debug("%s: already on\n", __FUNCTION__);
		return;
	}

	switch (enable) {

		// LED off
		case 0:

			bln_led_state = false;
			bln_imp->off();
			pr_debug("%s: false\n", __FUNCTION__);
			break;

		// LED on
		case 1:

			bln_led_state = true;
			bln_imp->on();
			pr_debug("%s: true\n", __FUNCTION__);
			break;
	}

}

static void bln_blink_enable(bool enable)
{
	pr_debug("%s: %s\n", __FUNCTION__, enable ? "true" : "false");

	if (enable == 0 && !bln_blink_state) {
		pr_debug("%s: already disabled\n", __FUNCTION__);
		return;
	}

	else if (enable == 1 && bln_blink_state && !bln_ongoing) {
		pr_debug("%s: already enabled\n", __FUNCTION__);
		return;
	}

	switch (enable) {

		// disable blink
		case 0:

			bln_blink_state = false;

			if (timer_pending(&blink_timer))
				del_timer_sync(&blink_timer);

			if (wake_lock_active(&bln_wake_lock))
				wake_unlock(&bln_wake_lock);

			pr_debug("%s: disable complete\n", __FUNCTION__);
			break;

		// enable blink
		case 1:

			if (!timer_pending(&blink_timer) && !wake_lock_active(&bln_wake_lock)) {

				bln_blink_state = true;
				wake_lock(&bln_wake_lock);

				blink_timer.expires = jiffies +
					msecs_to_jiffies(blink_on_msec);

				blink_count = max_blink_count;
				add_timer(&blink_timer);
				pr_debug("%s: enable complete\n", __FUNCTION__);
			}
			break;
	}

	pr_debug("%s: end\n", __FUNCTION__);
}

static void bln_led_notification_enable(bool enable)
{
	pr_debug("%s: %s\n", __FUNCTION__, enable ? "true" : "false");

	if (enable == 0 && !bln_ongoing) {
		pr_debug("%s: already disabled\n", __FUNCTION__);
		return;
	}

	else if (enable == 1 && !bln_enabled && bln_ongoing
			&& !bln_suspended && !bln_blink_state) {
		pr_debug("%s: already enabled\n", __FUNCTION__);
		return;
	}

	switch (enable) {

	// disable led notification
	case 0:

		bln_ongoing = false;
		bln_blink_enable(false);
		bln_led_enable(false);

		if (bln_suspended)
			bln_imp->disable();

		pr_debug("%s: disable complete\n", __FUNCTION__);
		break;

	// enable led notification
	case 1:

		if (bln_imp->enable()) {

			bln_ongoing = true;
			bln_led_enable(true);
			pr_debug("%s: enable complete\n", __FUNCTION__);
		}
		break;
	}

	pr_debug("%s: end\n", __FUNCTION__);
}

static void bln_early_suspend(struct early_suspend *h)
{
	pr_notice("[BLN] early suspend\n");
	bln_suspended = true;
}

static void bln_late_resume(struct early_suspend *h)
{
	pr_notice("[BLN] late resume\n");
	mutex_lock(&bln_mutex);
	bln_suspended = false;

	// cancel bln led activity upon resume
	if (bln_ongoing)
		bln_led_notification_enable(false);

	mutex_unlock(&bln_mutex);
}

static struct early_suspend bln_suspend_data = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 5,
	.suspend = bln_early_suspend,
	.resume = bln_late_resume,
};

static ssize_t enabled_status_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bln_enabled);
}

static ssize_t enabled_status_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	mutex_lock(&bln_mutex);

	if(sscanf(buf, "%u\n", &data) == 1) {

		pr_debug("%s: %u \n", __FUNCTION__, data);

		if (data == 1) {
			bln_enabled = true;
			pr_debug("%s: bln support enabled\n", __FUNCTION__);
		}

		else if (data == 0) {
			bln_enabled = false;
			bln_led_notification_enable(false);
			pr_debug("%s: bln support disabled\n", __FUNCTION__);
		}

		else
			pr_err("%s: invalid input %u\n", __FUNCTION__,
					data);
	}

	else
		pr_err("%s: invalid input\n", __FUNCTION__);

	mutex_unlock(&bln_mutex);

	return size;
}

static ssize_t notification_led_status_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf,"%d\n", bln_ongoing);
}

static ssize_t notification_led_status_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	mutex_lock(&bln_mutex);

	if (sscanf(buf, "%u\n", &data) == 1) {

		pr_debug("%s: %u\n", __FUNCTION__, data);

		if (data == 1) {
			bln_led_notification_enable(true);
			pr_debug("%s: notification led enabled\n", __FUNCTION__);
		}

		else if (data == 0) {
			bln_led_notification_enable(false);
			pr_debug("%s: notification led disabled\n", __FUNCTION__);
		}

		else
			pr_err("%s: wrong input %u\n", __FUNCTION__, data);
	}

	else
		pr_err("%s: input error\n", __FUNCTION__);

	mutex_unlock(&bln_mutex);

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

	mutex_lock(&bln_mutex);

	c = sscanf(buf, "%u %u\n", &ms_on, &ms_off);

	pr_debug("%s: ms_on:%u ms_off:%u \n", __FUNCTION__, ms_on, ms_off);

	if (c == 1 || c == 2) {

		blink_on_msec = ms_on;
		blink_off_msec = (c == 2) ? ms_off : ms_on;
	}

	else
		pr_err("%s: invalid input\n", __FUNCTION__);

	mutex_unlock(&bln_mutex);

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

	mutex_lock(&bln_mutex);

	if (sscanf(buf, "%u\n", &data) == 1) {

		max_blink_count = data;
		pr_debug("%s: %u \n", __FUNCTION__, data);
	}

	else
		pr_err("%s: invalid input\n", __FUNCTION__);

	mutex_unlock(&bln_mutex);

	return size;
}

static ssize_t blink_control_read(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", bln_blink_state);
}

static ssize_t blink_control_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int data;

	mutex_lock(&bln_mutex);

	if (sscanf(buf, "%u\n", &data) == 1) {

		if (data == 1) {
			bln_blink_enable(true);
			pr_debug("%s: bln blink enabled\n", __FUNCTION__);
		}

		else if (data == 0) {
			bln_blink_enable(false);
			pr_debug("%s: bln blink disabled\n", __FUNCTION__);
		}

		else
			pr_err("%s: input error %u\n", __FUNCTION__, data);
	}

	else
		pr_err("%s: input error\n", __FUNCTION__);

	mutex_unlock(&bln_mutex);

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

static void blink_callback(struct work_struct *blink_work)
{
	mutex_lock(&bln_mutex);

	if (bln_led_state) {

		if (--blink_count == 0) {

			pr_notice("%s: notification led time out\n", __FUNCTION__);
			bln_led_notification_enable(false);
			goto unlock;
		}

		bln_led_enable(false);
	}

	else
		bln_led_enable(true);

	unlock:
	mutex_unlock(&bln_mutex);
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

	mutex_init(&bln_mutex);

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
