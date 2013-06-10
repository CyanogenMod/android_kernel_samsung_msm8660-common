/*
 * drivers/misc/bln.c
 *
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

#define BACKLIGHTNOTIFICATION_VERSION 10

static void bln_led_enable(boolean enable)
{
	// return if led is already off
	if (enable == 0 && !bln_led_state) {

		pr_err("%s: already off\n", __FUNCTION__);

		return;
	}

	// return if led is already on
	if (enable == 1 && bln_led_state) {

		pr_err("%s: already on\n", __FUNCTION__);

		return;
	}

	switch (enable) {

	// LED off
	case 0:

		bln_led_state = false;

		bln_imp->off();

		break;

	// LED on
	case 1:

		bln_led_state = true;

		bln_imp->on();

		break;

	// invalid state
	default:

		pr_err("%s: invalid state\n", __FUNCTION__);

		break;
	}
}

static void bln_blink_enable(boolean enable)
{

	// return if blink is already disabled
	if (enable == 0 && !bln_blink_state) {

		pr_err("%s: OFF fail (blink_state=%d)\n", __FUNCTION__, bln_blink_state);

		return;
	}

	// return if blink is already enabled
	if (enable == 1 && bln_blink_state && !bln_ongoing) {

		pr_err("%s: ON fail (ongoing=%d, blink_state=%d)\n", __FUNCTION__, bln_ongoing, bln_blink_state);

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

		pr_info("%s: disable complete\n", __FUNCTION__);

		break;

	// enable blink
	case 1:

		bln_blink_state = true;

		wake_lock(&bln_wake_lock);

		blink_timer.expires = jiffies +
			msecs_to_jiffies(blink_on_msec);

		blink_count = max_blink_count;

		add_timer(&blink_timer);

		pr_info("%s: enable complete\n", __FUNCTION__);

		break;

	// invalid state
	default:

		pr_err("%s: invalid state\n", __FUNCTION__);

		break;
	}
}

static void bln_led_notification_enable(boolean enable)
{
	// return if led notification is already disabled
	if (enable == 0  && !bln_ongoing) {

		pr_err("%s: disable fail (ongoing=%d\n", __FUNCTION__, bln_ongoing);

		return;
	}

	// return if led notification is already enabled
	if (enable == 1 && !bln_enabled && bln_ongoing && !bln_suspended) {

		pr_err("%s: enable fail (enabled=%d ongoing=%d suspended=%d\n", __FUNCTION__, bln_enabled, bln_ongoing, bln_suspended);

		return;
	}

	switch (enable) {

	// disable led notification
	case 0:

		bln_ongoing = false;

                bln_blink_enable(false);

                bln_led_enable(false);

		if (!bln_suspended)
			bln_imp->disable();

                pr_info("%s: disable success\n", __FUNCTION__);

		break;

	// enable led notification
	case 1:

                if (bln_imp->enable()) {

			bln_ongoing = true;

                        bln_led_enable(true);

                        pr_info("%s: enable success\n", __FUNCTION__);
                }

		break;

	// invalid state
	default:

		pr_err("%s: invalid state\n", __FUNCTION__);

		break;
	}
}

static void bln_early_suspend(struct early_suspend *h)
{
	pr_info("[BLN] early suspend\n");

	bln_suspended = true;
}

static void bln_late_resume(struct early_suspend *h)
{
	pr_info("[BLN] late resume\n");

	mutex_lock(&bln_mutex);

	bln_suspended = false;

	// cancel bln led activity upon resume
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

		pr_devel("%s: %u \n", __FUNCTION__, data);

		if (data == 1) {

			pr_debug("%s: bln support enabled\n", __FUNCTION__);

			bln_enabled = true;
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

		if (data == 1)

			bln_led_notification_enable(true);

		else if (data == 0)

			bln_led_notification_enable(false);

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

	if (sscanf(buf, "%u\n", &data) == 1)

		max_blink_count = data;

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

		if (data == 1)

			bln_blink_enable(true);

		else if (data == 0)

			bln_blink_enable(false);

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
