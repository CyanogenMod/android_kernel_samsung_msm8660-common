/*
 *  /driver/sensors/sensors_core.c
 *
 *  Copyright (C) 2011 Samsung Electronics Co.Ltd
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/i2c/sensors_core.h>

struct class *sensors_class;

#ifdef SENSORS_LOG_DUMP
enum {
SENSORS_STATUS_ACCEL = 0,
SENSORS_STATUS_GYRO,
SENSORS_STATUS_MGNETIC,
SENSORS_STATUS_LIGHT,
SENSORS_STATUS_PROXI,
SENSORS_STATUS_PRESSURE,
SENSORS_STATUS_MAX,
};
static struct sensors_status_data{
	int use;
	int error;
	int reason;
}ss_data[SENSORS_STATUS_MAX] = {{0,},};

void sensors_status_set_accel(int error, int reason)
{
	ss_data[SENSORS_STATUS_ACCEL].use = 1;
	ss_data[SENSORS_STATUS_ACCEL].error = error;
	ss_data[SENSORS_STATUS_ACCEL].reason = reason;
}
void sensors_status_set_gyro(int error, int reason)
{
	ss_data[SENSORS_STATUS_GYRO].use = 1;
	ss_data[SENSORS_STATUS_GYRO].error = error;
	ss_data[SENSORS_STATUS_GYRO].reason = reason;
}
void sensors_status_set_magnetic(int error, int reason)
{
	ss_data[SENSORS_STATUS_MGNETIC].use = 1;
	ss_data[SENSORS_STATUS_MGNETIC].error = error;
	ss_data[SENSORS_STATUS_MGNETIC].reason = reason;
}
void sensors_status_set_light(int error, int reason)
{
	ss_data[SENSORS_STATUS_LIGHT].use = 1;
	ss_data[SENSORS_STATUS_LIGHT].error = error;
	ss_data[SENSORS_STATUS_LIGHT].reason = reason;
}
void sensors_status_set_proximity(int error, int reason)
{
	ss_data[SENSORS_STATUS_PROXI].use = 1;
	ss_data[SENSORS_STATUS_PROXI].error = error;
	ss_data[SENSORS_STATUS_PROXI].reason = reason;
}
void sensors_status_set_pressure(int error, int reason)
{
	ss_data[SENSORS_STATUS_PRESSURE].use = 1;
	ss_data[SENSORS_STATUS_PRESSURE].error = error;
	ss_data[SENSORS_STATUS_PRESSURE].reason = reason;
}


ssize_t sensors_status_show(struct class *class, struct class_attribute *attr,
				char *buf)
{
	int i, len = 0;
	char info[512] = {0,};

	for (i = 0; i < SENSORS_STATUS_MAX;i++) {
		if(ss_data[i].use)
			len += snprintf(info + len, 128,"[%d] status:0x%04x, reason:%d\n",i,ss_data[i].error, ss_data[i].reason);
		else
			len += snprintf(info + len, 128,"[%d] not used\n",i);
	}

	len = snprintf(buf, PAGE_SIZE,"%s\n",info);

	return len;
}

static CLASS_ATTR(sensors_status, S_IRUGO, sensors_status_show, NULL);
#endif



/**
* sensors_classdev_register - create new sensor device in sensors_class.
* @dev: The device to register.
*/

static void set_sensor_attr(struct device *dev,
			    struct device_attribute *attributes[])
{
	int i;

	for (i = 0; attributes[i] != NULL; i++) {
		if ((device_create_file(dev, attributes[i])) < 0) {
			printk(KERN_ERR"[SENSOR CORE] create_file attributes %d\n",
			     i);
		}
	}
}

struct device *sensors_classdev_register(char *sensors_name)
{
	struct device *dev;
	int retval = -ENODEV;

	dev = device_create(sensors_class, NULL, 0,
					NULL, "%s", sensors_name);
	if (IS_ERR(dev))
		return ERR_PTR(retval);

	printk(KERN_INFO "Registered sensors device: %s\n", sensors_name);
	return dev;
}
EXPORT_SYMBOL_GPL(sensors_classdev_register);

/**
* sensors_classdev_unregister - unregisters a object of sensor device.
*
*/
void sensors_classdev_unregister(struct device *dev)
{
	device_unregister(dev);
}
EXPORT_SYMBOL_GPL(sensors_classdev_unregister);

int sensors_register(struct device *dev, void *drvdata,
		     struct device_attribute *attributes[], char *name)
{
	int ret = 0;
	if (!sensors_class) {
		sensors_class = class_create(THIS_MODULE, "sensors");
		if (IS_ERR(sensors_class))
			return PTR_ERR(sensors_class);
	}

	dev = device_create(sensors_class, NULL, 0, drvdata, "%s", name);

	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		printk(KERN_ERR "[SENSORS CORE] device_create failed! [%d]\n",
		       ret);
		return ret;
	}

	set_sensor_attr(dev, attributes);

	return 0;
}
EXPORT_SYMBOL_GPL(sensors_register);

void sensors_unregister(struct device *dev)
{
	device_unregister(dev);
}
EXPORT_SYMBOL_GPL(sensors_unregister);

static int __init sensors_class_init(void)
{
	int err;
	sensors_class = class_create(THIS_MODULE, "sensors");
	if (IS_ERR(sensors_class)) {
		pr_err("Failed to create class(sensors)!\n");
		return PTR_ERR(sensors_class);
	}
	
#ifdef SENSORS_LOG_DUMP
	err = class_create_file(sensors_class, &class_attr_sensors_status);
    if (err) {
	    pr_err("%s: couldn't create class_attr_accel.\n", __func__);
	        }
#endif

	return 0;
}

static void __exit sensors_class_exit(void)
{
	class_destroy(sensors_class);
}

subsys_initcall(sensors_class_init);
module_exit(sensors_class_exit);

MODULE_DESCRIPTION("Universal sensors core class");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
