/*
 * Driver model for sensor
 *
 * Copyright (C) 2008 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __LINUX_SENSORS_CORE_H_INCLUDED
#define __LINUX_SENSORS_CORE_H_INCLUDED

#ifdef SENSORS_LOG_DUMP
extern void sensors_status_set_accel(int error, int reason);
extern void sensors_status_set_gyro(int error, int reason);
extern void sensors_status_set_magnetic(int error, int reason);
extern void sensors_status_set_light(int error, int reason);
extern void sensors_status_set_proximity(int error, int reason);
extern void sensors_status_set_pressure(int error, int reason);
#endif

extern struct device *sensors_classdev_register(char *sensors_name);
extern void sensors_classdev_unregister(struct device *dev);
extern int sensors_register(struct device *dev,
	void *drvdata, struct device_attribute *attributes[], char *name);
extern void sensors_unregister(struct device *dev);
#endif	/* __LINUX_SENSORS_CORE_H_INCLUDED */
