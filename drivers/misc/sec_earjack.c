/*
 *  headset/ear-jack device detection driver.
 *
 *  Copyright (C) 2010 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/sysdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/timer.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/sec_earjack.h>

#define MODULE_NAME "[sec_earjack] "
#define MAX_ZONE_LIMIT		10
#define SEND_KEY_CHECK_TIME_MS	40		/* 40ms, for double ket checking*/
#define DET_CHECK_TIME_MS	50			/* 200ms -> 50ms */
#define WAKE_LOCK_TIME		(HZ * 5)	/* 5 sec */
#define WAKE_LOCK_TIME_IN_SENDKEY (HZ * 1)
#define SUPPORT_PBA

#ifdef SUPPORT_PBA
struct class *jack_class;
EXPORT_SYMBOL(jack_class);

/* Sysfs device, this is used for communication with Cal App. */
static struct device *jack_selector_fs;
EXPORT_SYMBOL(jack_selector_fs);

static struct device *jack_reselector_fs;     
EXPORT_SYMBOL(jack_reselector_fs);

bool recheck_jack = false;
#endif

extern int get_hw_rev(void);
#ifdef CONFIG_SENSORS_YDA165
extern void yda165_avdd_power_on(void);
extern void yda165_avdd_power_off(void);
#endif

struct sec_jack_info {
	struct sec_jack_platform_data *pdata;
	struct input_dev *input;
	struct wake_lock det_wake_lock;
	struct sec_jack_zone *zone;
	int keypress_code;
	bool send_key_pressed;
	bool send_key_irq_enabled;
	unsigned int cur_jack_type;
	struct work_struct  det_work;
	struct work_struct  sendkey_work;
	struct delayed_work  powerup_work;
	bool is_ready;
};

/* sysfs name HeadsetObserver.java looks for to track headset state
 */
struct switch_dev switch_jack_detection = {
	.name = "h2w",
};

/* To support samsung factory test */
struct switch_dev switch_sendend = {
	.name = "send_end",
};

static void set_send_key_state(struct sec_jack_info *hi, int state)
{
	struct sec_jack_platform_data *pdata = hi->pdata;
	struct sec_jack_buttons_zone *btn_zones = pdata->buttons_zones;
	int adc;
	int i;

	adc = pdata->get_adc_value();
	pr_info(MODULE_NAME "%s adc=%d, state=%d\n", __func__, adc, state);

	if (state != 0) {
		for (i = 0; i < pdata->num_buttons_zones; i++)
			if (adc >= btn_zones[i].adc_low &&
			    adc <= btn_zones[i].adc_high) {
				hi->keypress_code = btn_zones[i].code;
				input_report_key(hi->input, btn_zones[i].code, state);
				input_sync(hi->input);
				switch_set_state(&switch_sendend, state);
				hi->send_key_pressed = state;
				pr_info(MODULE_NAME "%s: keycode=%d, is pressed\n", __func__,
					btn_zones[i].code);
				return;
			}
	} else {
		input_report_key(hi->input, hi->keypress_code, state);
		input_sync(hi->input);
		switch_set_state(&switch_sendend, state);
		hi->send_key_pressed = state;
	}
#if 0
	/*check this adc value from H/W team*/
	if (state != 0) {
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_JPN_MODEL_SC_05D)
		if (((get_hw_rev() > 8)
			&& (adc == KEY_MEDIA_ADC_MIN))
				|| ((get_hw_rev() <= 8)
					&& (adc >= KEY_MEDIA_ADC_MIN
					&& adc < KEY_MEDIA_ADC_MAX)))
#else
		if ((adc >= KEY_MEDIA_ADC_MIN && adc < KEY_MEDIA_ADC_MAX))
#endif
		{
			hi->keypress_code = KEY_MEDIA;
			pr_info(MODULE_NAME "Key PRESSED is:%d  ADC VALUE = %d \n", hi->keypress_code, adc);
			input_report_key(hi->input, KEY_MEDIA, state);
			input_sync(hi->input);
			switch_set_state(&switch_sendend, state);
			hi->send_key_pressed = state;
		}

		if (adc >= KEY_VOLUMEUP_ADC_MIN && adc < KEY_VOLUMEUP_ADC_MAX) {
			hi->keypress_code = KEY_VOLUMEUP;
			pr_info(MODULE_NAME "Key PRESSED is:%d  ADC VALUE = %d \n", hi->keypress_code, adc);
			input_report_key(hi->input, KEY_VOLUMEUP, state);
			input_sync(hi->input);
			switch_set_state(&switch_sendend, state);
			hi->send_key_pressed = state;
		}

		if (adc >= KEY_VOLUMEDOWN_ADC_MIN && adc < KEY_VOLUMEDOWN_ADC_MAX) {
			hi->keypress_code = KEY_VOLUMEDOWN;
			pr_info(MODULE_NAME "Key PRESSED is:%d  ADC VALUE = %d \n", hi->keypress_code, adc);
			input_report_key(hi->input, KEY_VOLUMEDOWN, state);
			input_sync(hi->input);
			switch_set_state(&switch_sendend, state);
			hi->send_key_pressed = state;
		}
	} else {
		input_report_key(hi->input, hi->keypress_code, state);
		input_sync(hi->input);
		switch_set_state(&switch_sendend, state);
		hi->send_key_pressed = state;
	}
#endif
}

static void sec_jack_set_type(struct sec_jack_info *hi, int jack_type)
{
	struct sec_jack_platform_data *pdata = hi->pdata;

	/* this can happen during slow inserts where we think we identified
	 * the type but then we get another interrupt and do it again
	 */
	if (jack_type == hi->cur_jack_type) {
		//pr_debug(MODULE_NAME "%s return, same type reason\n", __func__);
	pr_info(MODULE_NAME "%s return, same type reason\n", __func__);
		pr_info(MODULE_NAME "%s return, jack_type = %d, hi->cur_jack_type = %d\n", __func__, jack_type, hi->cur_jack_type);

	return;
	}

	if (jack_type == SEC_HEADSET_4POLE) {
		/* for a 4 pole headset, enable irq
		   for detecting send/end key presses */
		if (!hi->send_key_irq_enabled) {
			pr_info(MODULE_NAME "%s send_int enabled\n", __func__);
			enable_irq(pdata->send_int);
			enable_irq_wake(pdata->send_int);
			hi->send_key_irq_enabled = 1;
		}
	} else {
		/* for all other jacks, disable send/end irq */
		if (hi->send_key_irq_enabled) {
			pr_info(MODULE_NAME "%s send_int disabled\n", __func__);
			disable_irq(pdata->send_int);
			disable_irq_wake(pdata->send_int);
			hi->send_key_irq_enabled = 0;
		}
		if (hi->send_key_pressed) {
			set_send_key_state(hi, 0);
			pr_info(MODULE_NAME "%s : BTN set released by jack switch to %d\n",
					__func__, jack_type);
		}
	}

	pr_info(MODULE_NAME "%s : jack_type = %d\n", __func__, jack_type);
	/* prevent suspend to allow user space to respond to switch */
	wake_lock_timeout(&hi->det_wake_lock, WAKE_LOCK_TIME);

	hi->cur_jack_type = jack_type;
	switch_set_state(&switch_jack_detection, jack_type);

	/* micbias is left enabled for 4pole and disabled otherwise */
	pdata->set_micbias_state(hi->send_key_irq_enabled);

}

static void handle_jack_not_inserted(struct sec_jack_info *hi)
{
	struct sec_jack_platform_data *pdata = hi->pdata;

	sec_jack_set_type(hi, SEC_JACK_NO_DEVICE);
	hi->pdata->set_micbias_state(false);
}

static void determine_jack_type(struct sec_jack_info *hi)
{
	struct sec_jack_zone *zones = hi->pdata->zones;
	int size = hi->pdata->num_zones;
	int count[MAX_ZONE_LIMIT] = {0};
	int adc;
	int i;

	while (hi->pdata->get_det_jack_state()) {
		adc = hi->pdata->get_adc_value();
		pr_info(MODULE_NAME "determine_jack_type adc = %d\n", adc);

		/* determine the type of headset based on the
		 * adc value.  An adc value can fall in various
		 * ranges or zones.  Within some ranges, the type
		 * can be returned immediately.  Within others, the
		 * value is considered unstable and we need to sample
		 * a few more types (up to the limit determined by
		 * the range) before we return the type for that range.
		 */
		for (i = 0; i < size; i++) {
			if (adc <= zones[i].adc_high) {
				if (++count[i] > zones[i].check_count) {
					pr_debug(MODULE_NAME "determine_jack_type %d, %d, %d\n",
						zones[i].adc_high, count[i], zones[i].check_count);
						if(recheck_jack == true && i == 3) {
						pr_info(MODULE_NAME "something worng connection!\n");
						handle_jack_not_inserted(hi);

						recheck_jack = false;
						return;
					}
     sec_jack_set_type(hi,zones[i].jack_type);
					/* mic_bias remains enabled in race condition. */
					if (hi->cur_jack_type != SEC_HEADSET_4POLE) {
						hi->pdata->set_micbias_state(false);
						pr_info(MODULE_NAME "forced mic_bias disable\n");
					}
	recheck_jack = false;
					return;
				}
				msleep(zones[i].delay_ms);
				break;
			}
		}
	}
	/* jack removed before detection complete */

	recheck_jack = false;
	handle_jack_not_inserted(hi);
}

#ifdef SUPPORT_PBA
static ssize_t select_jack_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_info("%s : operate nothing\n", __func__);

	return 0;
}

static ssize_t select_jack_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_jack_info *hi = dev_get_drvdata(dev);
	struct sec_jack_platform_data *pdata = hi->pdata;
	int value = 0;


	sscanf(buf, "%d", &value);
	pr_err("%s: User  selection : 0X%x", __func__, value);
	if (value == SEC_HEADSET_4POLE) {
		pdata->set_micbias_state(true);
		msleep(100);
	}

	sec_jack_set_type(hi, value);

	return size;
}
static DEVICE_ATTR(select_jack, S_IRUGO | S_IWUSR | S_IWGRP, select_jack_show, select_jack_store);

static ssize_t reselect_jack_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_info("%s : operate nothing\n", __func__);

	return 0;
}    

static ssize_t reselect_jack_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_jack_info *hi = dev_get_drvdata(dev);
	struct sec_jack_platform_data *pdata = hi->pdata;
	int value = 0;

	sscanf(buf, "%d", &value);
	pr_err("%s: User  selection : 0x%x\n", __func__, value);

	if(value == 1) {
		recheck_jack = true;
		determine_jack_type(hi);
	}
	
	return size;
}
static DEVICE_ATTR(reselect_jack, S_IRUGO | S_IWUSR | S_IWGRP, reselect_jack_show, reselect_jack_store);
#endif

static irqreturn_t sec_jack_send_key_irq_handler(int irq, void *handle)
{
	struct sec_jack_info *hi = (struct sec_jack_info *)handle;

	pr_info(MODULE_NAME "%s : irq is %d.\n", __func__, irq);

	if (hi->is_ready) {
		schedule_work(&hi->sendkey_work);
	}

	return IRQ_HANDLED;
}

static void sec_jack_send_key_work_func(struct work_struct *work)
{
	struct sec_jack_info *hi =
		container_of(work, struct sec_jack_info, sendkey_work);
	struct sec_jack_platform_data *pdata = hi->pdata;
	int time_left_ms = SEND_KEY_CHECK_TIME_MS;
	int send_key_state = 0;

	wake_lock_timeout(&hi->det_wake_lock, WAKE_LOCK_TIME_IN_SENDKEY);
	/* debounce send/end key */
	while (time_left_ms > 0 && !hi->send_key_pressed) {
		send_key_state = pdata->get_send_key_state();
		if (!send_key_state || !pdata->get_det_jack_state() ||
				hi->cur_jack_type != SEC_HEADSET_4POLE) {
			/* button released or jack removed or more
			 * strangely a non-4pole headset
			 */
			pr_info(MODULE_NAME "%s : ignored button (%d %d %d)\n", __func__,
					!send_key_state, !pdata->get_det_jack_state(),
					hi->cur_jack_type != SEC_HEADSET_4POLE);
			return;
		}
		msleep(10);
		time_left_ms -= 10;
	}

	/* report state change of the send_end_key */
	if (hi->send_key_pressed != send_key_state) {
		set_send_key_state(hi, send_key_state);
		pr_info(MODULE_NAME "%s : BTN is %s.\n",
				__func__, send_key_state ? "pressed" : "released");
	}

	return;
}

static irqreturn_t sec_jack_det_irq_handler(int irq, void *handle)
{
	struct sec_jack_info *hi = (struct sec_jack_info *)handle;

	pr_info(MODULE_NAME "%s : ready = %d.\n", __func__, hi->is_ready);

	if (hi->is_ready) {
		schedule_work(&hi->det_work);
	}
	return IRQ_HANDLED;
}

static void sec_jack_det_work_func(struct work_struct *work)
{
	struct sec_jack_info *hi =
		container_of(work, struct sec_jack_info, det_work);

	struct sec_jack_platform_data *pdata = hi->pdata;
	int time_left_ms = DET_CHECK_TIME_MS;

	pr_debug(MODULE_NAME "%s\n", __func__);

	/* threaded irq can sleep */
	wake_lock_timeout(&hi->det_wake_lock, WAKE_LOCK_TIME);

	/* debounce headset jack.  don't try to determine the type of
	 * headset until the detect state is true for a while.
	 */
	while (time_left_ms > 0) {
		if (!pdata->get_det_jack_state()) {
			/* jack not detected. */
			/* pr_debug(MODULE_NAME "%s. false detection\n", __func__); */
			handle_jack_not_inserted(hi);
			return;
		}
		msleep(10);
		time_left_ms -= 10;
	}

#ifdef CONFIG_SENSORS_YDA165
	/* Temperaliy insertion code for removing L-detection noise when earjack is entering 3.5PI connector. */
	yda165_avdd_power_on();
	msleep(10);
#endif

	/* set mic bias to enable adc */
	pdata->set_micbias_state(true);

	/* to reduce noise in earjack when attaching */
	/* msleep(200); */

	/* jack presence was detected the whole time, figure out which type */
	determine_jack_type(hi);

#ifdef CONFIG_SENSORS_YDA165
	/* Temperaliy insertion code for removing L-detection noise when earjack is entering 3.5PI connector. */
	yda165_avdd_power_off();
#endif

	return;
}

static void sec_jack_powerup_work_func(struct work_struct *work)
{
	struct sec_jack_info *hi =
		container_of(work, struct sec_jack_info, powerup_work.work);

	hi->is_ready = true;
	sec_jack_det_irq_handler(hi->pdata->det_int, hi);

	return;
}

static int sec_jack_probe(struct platform_device *pdev)
{
	struct sec_jack_info *hi;
	struct sec_jack_platform_data *pdata = pdev->dev.platform_data;
	int ret;
	int sec_jack_keycode[] = {KEY_MEDIA, KEY_VOLUMEUP, KEY_VOLUMEDOWN};
	int i;

	printk("[#################################################################]\n");
	printk("[#################################################################]\n");
	printk("[#################################################################]\n");
	pr_info(MODULE_NAME "%s : Registering jack driver\n", __func__);
	if (!pdata) {
		pr_err("%s : pdata is NULL.\n", __func__);
		return -ENODEV;
	}
	if (!pdata->get_adc_value || !pdata->get_det_jack_state	||
			!pdata->get_send_key_state || !pdata->zones ||
			!pdata->set_micbias_state || pdata->num_zones > MAX_ZONE_LIMIT) {
		pr_err("%s : need to check pdata\n", __func__);
		return -ENODEV;
	}
#ifdef CONFIG_MACH_OLIVER
	pdata->rpc_init();
#endif

	hi = kzalloc(sizeof(struct sec_jack_info), GFP_KERNEL);
	if (hi == NULL) {
		pr_err("%s : Failed to allocate memory.\n", __func__);
		return -ENOMEM;
	}

	hi->is_ready = false;
	hi->pdata = pdata;
	hi->input = input_allocate_device();
	if (hi->input == NULL) {
		ret = -ENOMEM;
		pr_err("%s : Failed to allocate input device.\n", __func__);
		goto err_request_input_dev;
	}

	hi->input->name = "sec_jack";

	for (i = 0 ; i < 3; i++)
		input_set_capability(hi->input, EV_KEY, sec_jack_keycode[i]);
	ret = input_register_device(hi->input);

	if (ret) {
		pr_err("%s : Failed to register driver\n", __func__);
		goto err_register_input_dev;
	}

	ret = switch_dev_register(&switch_jack_detection);
	if (ret < 0) {
		pr_err("%s : Failed to register switch device\n", __func__);
		goto err_switch_dev_register;
	}

	ret = switch_dev_register(&switch_sendend);
	if (ret < 0) {
		pr_err("%s : Failed to register switch device\n", __func__);
		goto err_switch_dev_register;
	}

	wake_lock_init(&hi->det_wake_lock, WAKE_LOCK_SUSPEND, "sec_jack_det");

#ifdef SUPPORT_PBA
	/* Create JACK Device file in Sysfs */
	jack_class = class_create(THIS_MODULE, "jack");
	if (IS_ERR(jack_class)) {
		pr_err("%s : Failed to create class(sec_jack)\n", __func__);
	}

	jack_selector_fs = device_create(jack_class, NULL, 0, hi, "jack_selector");
	if (IS_ERR(jack_selector_fs))
		pr_err("%s : Failed to create device(sec_jack)!= %ld\n", __func__, IS_ERR(jack_selector_fs));

	if (device_create_file(jack_selector_fs, &dev_attr_select_jack) < 0)
		pr_err("%s : Failed to create device file(%s)!\n", __func__, dev_attr_select_jack.attr.name);

	jack_reselector_fs = device_create(jack_class, NULL, 0, hi, "jack_reselector");
	if (IS_ERR(jack_reselector_fs))
		pr_err("%s : Failed to create device(sec_jack)!= %ld\n", __func__, IS_ERR(jack_reselector_fs));

	if (device_create_file(jack_reselector_fs, &dev_attr_reselect_jack) < 0)
		pr_err("%s : Failed to create device file(%s)!\n", __func__, dev_attr_reselect_jack.attr.name);	
#endif

	INIT_WORK(&hi->det_work, sec_jack_det_work_func);

	ret = request_threaded_irq(pdata->det_int, NULL,
			sec_jack_det_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
			IRQF_ONESHOT, "sec_headset_detect", hi);

	if (ret) {
		pr_err("%s : Failed to request_irq.\n", __func__);
		goto err_request_detect_irq;
	}

	/* to handle insert/removal when we're sleeping in a call */
	ret = enable_irq_wake(pdata->det_int);
	if (ret) {
		pr_err("%s : Failed to enable_irq_wake.\n", __func__);
		goto err_enable_irq_wake;
	}

	INIT_WORK(&hi->sendkey_work, sec_jack_send_key_work_func);

	ret = request_threaded_irq(pdata->send_int, NULL,
			sec_jack_send_key_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING |
			IRQF_ONESHOT,
			"sec_headset_send_key", hi);
	if (ret) {
		pr_err("%s : Failed to request_irq.\n", __func__);

		goto err_request_send_key_irq;
	}

	/* start with send/end interrupt disable. we only enable it
	 * when we detect a 4 pole headset
	 */
	disable_irq(pdata->send_int);
	dev_set_drvdata(&pdev->dev, hi);

	/* call irq_thread forcely because of missing interrupt when booting.
	 * 2000ms delay is enough to waiting for adc driver registration.
	 */
	INIT_DELAYED_WORK(&hi->powerup_work, sec_jack_powerup_work_func);
	schedule_delayed_work(&hi->powerup_work, msecs_to_jiffies(2000));

	return 0;

err_request_send_key_irq:
	disable_irq_wake(pdata->det_int);
err_enable_irq_wake:
	free_irq(pdata->det_int, hi);
err_request_detect_irq:
	wake_lock_destroy(&hi->det_wake_lock);
	switch_dev_unregister(&switch_jack_detection);
	switch_dev_unregister(&switch_sendend);
err_switch_dev_register:
	input_unregister_device(hi->input);
	goto err_request_input_dev;
err_register_input_dev:
	input_free_device(hi->input);
err_request_input_dev:
	kfree(hi);

	return ret;
}

static int sec_jack_remove(struct platform_device *pdev)
{

	struct sec_jack_info *hi = dev_get_drvdata(&pdev->dev);

	pr_info(MODULE_NAME "%s :\n", __func__);
	/* rebalance before free */
	if (hi->send_key_irq_enabled)
		disable_irq_wake(hi->pdata->send_int);
	else
		enable_irq(hi->pdata->send_int);
	free_irq(hi->pdata->send_int, hi);
	disable_irq_wake(hi->pdata->det_int);
	free_irq(hi->pdata->det_int, hi);
	wake_lock_destroy(&hi->det_wake_lock);
	switch_dev_unregister(&switch_jack_detection);
	switch_dev_unregister(&switch_sendend);
	input_unregister_device(hi->input);
	kfree(hi);

	return 0;
}

static struct platform_driver sec_jack_driver = {
	.probe = sec_jack_probe,
	.remove = sec_jack_remove,
	.driver = {
		.name = "sec_jack",
		.owner = THIS_MODULE,
	},
};
static int __init sec_jack_init(void)
{
	return platform_driver_register(&sec_jack_driver);
}

static void __exit sec_jack_exit(void)
{
	platform_driver_unregister(&sec_jack_driver);
}

module_init(sec_jack_init);
module_exit(sec_jack_exit);

MODULE_AUTHOR("ms17.kim@samsung.com");
MODULE_DESCRIPTION("Samsung Electronics Corp Ear-Jack detection driver");
MODULE_LICENSE("GPL");
