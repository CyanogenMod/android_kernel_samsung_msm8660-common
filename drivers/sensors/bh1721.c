
/*
 *  bh1721fvc.c - Ambient Light Sensor IC
 *
 *  Copyright (C) 2009 Samsung Electronics
 *  Donggeun Kim <dg77.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
 
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/bh1721.h>

#define ON              1
#define OFF				0

static bool light_enable = OFF;
	
/* set sysfs for light sensor test mode*/
extern int sensors_register(struct device *dev, void * drvdata, struct device_attribute *attributes[], char *name);
static struct device *light_sensor_device;

#define NUM_OF_BYTES_WRITE	1
#define NUM_OF_BYTES_READ	2

#define MAX_LEVEL 	8
#define MAX_LUX		65528

#define ALS_BUFFER_NUM	10

const unsigned char POWER_DOWN = 0x00;
const unsigned char POWER_ON = 0x01;
const unsigned char AUTO_RESOLUTION_1 = 0x10;
const unsigned char AUTO_RESOLUTION_2 = 0x20;
const unsigned char H_RESOLUTION_1 = 0x12;
const unsigned char H_RESOLUTION_2 = 0x22;
const unsigned char L_RESOLUTION_1 = 0x13;
const unsigned char L_RESOLUTION_2 = 0x23;
const unsigned char L_RESOLUTION_3 = 0x16;
const unsigned char L_RESOLUTION_4 = 0x26;

enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
};

typedef enum 
{
				DOWNWARD = 0,
				UPSIDE,
				EQUAL
}Direction;

struct bh1721_data;

/* driver data */
struct bh1721_data {
	struct input_dev *light_input_dev;
	struct bh1721_platform_data *pdata;
	void (*reset) (void);
	void (*resetpin_down) (void);	
	struct i2c_client *i2c_client;
	struct work_struct work_light;
	struct hrtimer timer;
	ktime_t light_poll_delay;
	u8 power_state;
	struct mutex power_lock;
	struct workqueue_struct *wq;
	unsigned char illuminance_data[2];
	bool als_buf_initialized;
	int als_value_buf[ALS_BUFFER_NUM];
	int als_index_count;
};

static int bh1721_write_command(struct i2c_client *client, const char *command)
{
	return i2c_master_send(client, command, NUM_OF_BYTES_WRITE);
}

static int bh1721_read_value(struct i2c_client *client, char *buf)
{
	return i2c_master_recv(client, buf, NUM_OF_BYTES_READ);
}

static void bh1721_light_enable(struct bh1721_data *bh1721)
{
	printk("[Light Sensor] starting poll timer, delay %lldns\n",
		    ktime_to_ns(bh1721->light_poll_delay));
	hrtimer_start(&bh1721->timer, bh1721->light_poll_delay, HRTIMER_MODE_REL);

	if((bh1721_write_command(bh1721->i2c_client, &POWER_ON))>0)
		printk("[Light Sensor] Power ON");
	
	if((bh1721_write_command(bh1721->i2c_client, &AUTO_RESOLUTION_1 ))>0)
		printk("[Light Sensor] auto resoulution ON");
}

static void bh1721_light_disable(struct bh1721_data *bh1721)
{
	printk("[Light Sensor] cancelling poll timer\n");
	hrtimer_cancel(&bh1721->timer);
	cancel_work_sync(&bh1721->work_light);

	if((bh1721_write_command(bh1721->i2c_client, &POWER_DOWN))>0)
		printk("[Light Sensor] Power off");
}

static ssize_t poll_delay_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct bh1721_data *bh1721 = dev_get_drvdata(dev);
	return sprintf(buf, "%lld\n", ktime_to_ns(bh1721->light_poll_delay));
}


static ssize_t poll_delay_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct bh1721_data *bh1721 = dev_get_drvdata(dev);
	int64_t new_delay;
	int err;

	err = strict_strtoll(buf, 10, &new_delay);
	if (err < 0)
		return err;

	printk("[Light Sensor] new delay = %lldns, old delay = %lldns\n",
		    new_delay, ktime_to_ns(bh1721->light_poll_delay));
	mutex_lock(&bh1721->power_lock);
	if (new_delay != ktime_to_ns(bh1721->light_poll_delay)) {
		bh1721->light_poll_delay = ns_to_ktime(new_delay);
		if (bh1721->power_state & LIGHT_ENABLED) {
			bh1721_light_disable(bh1721);
			bh1721_light_enable(bh1721);
		}
	}
	mutex_unlock(&bh1721->power_lock);

	return size;
}

static ssize_t light_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct bh1721_data *bh1721 = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", (bh1721->power_state & LIGHT_ENABLED) ? 1 : 0);
}


static ssize_t light_enable_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct bh1721_data *bh1721 = dev_get_drvdata(dev);
	bool new_value;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	mutex_lock(&bh1721->power_lock);
	printk("[Light Sensor] new_value = %d, old state = %d\n", new_value, (bh1721->power_state & LIGHT_ENABLED) ? 1 : 0);
	if (new_value && !(bh1721->power_state & LIGHT_ENABLED)) {
		bh1721->power_state |= LIGHT_ENABLED;
		bh1721_light_enable(bh1721);
		bh1721->als_buf_initialized = false;
	} else if (!new_value && (bh1721->power_state & LIGHT_ENABLED)) {
		bh1721_light_disable(bh1721);
		bh1721->power_state &= ~LIGHT_ENABLED;
	}
	mutex_unlock(&bh1721->power_lock);
	return size;
}

static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   poll_delay_show, poll_delay_store);

static struct device_attribute dev_attr_light_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       light_enable_show, light_enable_store);


/*For Factory Test Mode*/
static ssize_t bh1721_show_illuminance(struct device *dev, struct device_attribute *attr, char *buf)
{	
	struct bh1721_data *bh1721 = dev_get_drvdata(dev);
	unsigned int result;

	/* 
	 * POWER ON command is possible to omit.
	 */
	if((bh1721_write_command(bh1721->i2c_client, &POWER_ON))>0)
		light_enable = ON;
	bh1721_write_command(bh1721->i2c_client, &AUTO_RESOLUTION_1);

	/* Maximum measurement time */
	msleep(180);
	bh1721_read_value(bh1721->i2c_client, bh1721->illuminance_data);
	if((bh1721_write_command(bh1721->i2c_client, &POWER_DOWN))>0)
		light_enable = OFF;
	result = bh1721->illuminance_data[0] << 8 | bh1721->illuminance_data[1];
	return sprintf(buf, "%d\n", result);
}
static DEVICE_ATTR(illuminance, S_IRUGO | S_IWUSR | S_IWGRP, 
		bh1721_show_illuminance, NULL);

static ssize_t lightsensor_lux_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{	
	unsigned int result;
	struct bh1721_data *bh1721 = dev_get_drvdata(dev);

	/* 
	 * POWER ON command is possible to omit.
	 */
	if (!(bh1721->power_state & LIGHT_ENABLED)){
		if((bh1721_write_command(bh1721->i2c_client, &POWER_ON))>0)
			light_enable = ON;
		bh1721_write_command(bh1721->i2c_client, &AUTO_RESOLUTION_1);

		/* Maximum measurement time */
		msleep(180);
	}
	bh1721_read_value(bh1721->i2c_client, bh1721->illuminance_data);

	if ( !(bh1721->power_state & LIGHT_ENABLED) ){
		if((bh1721_write_command(bh1721->i2c_client, &POWER_DOWN))>0)
			light_enable = OFF;
	}

	result = bh1721->illuminance_data[0] << 8 | bh1721->illuminance_data[1];

	result = (result * 10) / 12;
	result = result * 139 / 13;

	return sprintf(buf, "%d\n", result);
}

static ssize_t lightsensor_adc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{	
	unsigned int result;
	struct bh1721_data *bh1721 = dev_get_drvdata(dev);

	/* 
	 * POWER ON command is possible to omit.
	 */
	if((bh1721_write_command(bh1721->i2c_client, &POWER_ON))>0)
		light_enable = ON;
	bh1721_write_command(bh1721->i2c_client, &AUTO_RESOLUTION_1);

	/* Maximum measurement time */
	msleep(180);
	bh1721_read_value(bh1721->i2c_client, bh1721->illuminance_data);
	if((bh1721_write_command(bh1721->i2c_client, &POWER_DOWN))>0)
		light_enable = OFF;
	result = bh1721->illuminance_data[0] << 8 | bh1721->illuminance_data[1];
	return sprintf(buf, "%d\n", result);
}

static DEVICE_ATTR(lightsensor_lux, S_IRUGO | S_IWUSR | S_IWGRP, 
		lightsensor_lux_show,NULL);
static DEVICE_ATTR(lightsensor_adc, S_IRUGO | S_IWUSR | S_IWGRP, 
		lightsensor_adc_show,NULL);

static struct device_attribute *light_sensor_attrs[] = {
	&dev_attr_lightsensor_lux,
	&dev_attr_lightsensor_adc,
	NULL,
};

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_light_enable.attr,
	&dev_attr_poll_delay.attr,
	&dev_attr_illuminance.attr,
	NULL
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

static int bh1721_get_luxvalue(struct bh1721_data *bh1721, u16 *value)
{
	int retry;
	int i = 0;
	int j = 0;
	unsigned int als_total = 0;
	unsigned int als_index = 0;
	unsigned int als_max = 0;
	unsigned int als_min = 0;
	
	for (retry = 0; retry < 10; retry++)
	{
		if  (i2c_master_recv( bh1721->i2c_client, (u8 *)value, 2) == 2) {
			be16_to_cpus(value);
			break;
		}			
	}
	
	if(retry == 10)
	{
		printk("I2C read failed.. retry %d\n", retry);
		return -EIO;
	}
		
	als_index = (bh1721->als_index_count++) % ALS_BUFFER_NUM;

	/*ALS buffer initialize (light sensor off ---> light sensor on) */
	if (!bh1721->als_buf_initialized) {
		bh1721->als_buf_initialized = true;
		for (j = 0; j < ALS_BUFFER_NUM; j++)
			bh1721->als_value_buf[j] = *value;
	} else
		bh1721->als_value_buf[als_index] = *value;

	als_max = bh1721->als_value_buf[0];
	als_min = bh1721->als_value_buf[0];

	for (i = 0; i < ALS_BUFFER_NUM; i++) {
		als_total += bh1721->als_value_buf[i];

		if (als_max < bh1721->als_value_buf[i])
			als_max = bh1721->als_value_buf[i];

		if (als_min > bh1721->als_value_buf[i])
			als_min = bh1721->als_value_buf[i];
	}
	*value = (als_total-(als_max+als_min))/(ALS_BUFFER_NUM-2);

	if (bh1721->als_index_count >= ALS_BUFFER_NUM)
		bh1721->als_index_count = 0;
	
	return 0;
}

static void bh1721_work_func_light(struct work_struct *work)
{
	int err;
	u16 lux;
#if defined(CONFIG_TARGET_LOCALE_EUR_OPEN) || defined(CONFIG_TARGET_LOCALE_KOR_LGU) || defined(CONFIG_TARGET_LOCALE_USA_ATT)
	static int cnt = 0;
#endif
	unsigned int result;
	
	struct bh1721_data *bh1721 = container_of(work,
					struct bh1721_data, work_light);

	err = bh1721_get_luxvalue(bh1721, &lux);
	if (!err) {
		result = (lux * 10) / 12;
		result = result * 139 / 13;
		if(result > 89999) result = 89999;
			
		//printk("[Light sensor] lux 0x%0X (%d)\n", result, result);
		input_report_abs(bh1721->light_input_dev, ABS_MISC, result);
		input_sync(bh1721->light_input_dev);
	} else {
		pr_err("%s: read word failed! (errno=%d)\n", __func__, err);
	}

#if defined(CONFIG_TARGET_LOCALE_EUR_OPEN) || defined(CONFIG_TARGET_LOCALE_KOR_LGU) || defined(CONFIG_TARGET_LOCALE_USA_ATT)
	if(result == 0) cnt++;
	else cnt =0;
	
	if(cnt > 25)
	{
		cnt = 0;
		printk("Lux Value : 0 during 5 sec...\n");
		if (bh1721->reset)
		{
			bh1721->reset();
		}

		msleep(10);
		if (bh1721->power_state & LIGHT_ENABLED)
		{
			bh1721_light_enable(bh1721);
		}
	}
#endif
}

/* This function is for light sensor.  It operates every a few seconds.
 * It asks for work to be done on a thread because i2c needs a thread
 * context (slow and blocking) and then reschedules the timer to run again.
 */
static enum hrtimer_restart bh1721_timer_func(struct hrtimer *timer)
{
	struct bh1721_data *bh1721 = container_of(timer, struct bh1721_data, timer);
	queue_work(bh1721->wq, &bh1721->work_light);
	hrtimer_forward_now(&bh1721->timer, bh1721->light_poll_delay);
	return HRTIMER_RESTART;
}

static int bh1721_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct input_dev *input_dev;
	struct bh1721_data *bh1721;
	struct bh1721_platform_data *pdata = client->dev.platform_data;

	if (!pdata) {
		pr_err("%s: missing pdata!\n", __func__);
		return ret;
	}
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: i2c functionality check failed!\n", __func__);
		return ret;
	}

	bh1721 = kzalloc(sizeof(struct bh1721_data), GFP_KERNEL);
	if (!bh1721) {
		pr_err("%s: failed to alloc memory for module data\n",
		       __func__);
		return -ENOMEM;
	}

	
	bh1721->pdata = pdata;
	bh1721->reset = pdata->reset;
	bh1721->resetpin_down = pdata->resetpin_down;	
	bh1721->i2c_client = client;
	i2c_set_clientdata(client, bh1721);
	
	if (bh1721->reset)
		bh1721->reset();

	mutex_init(&bh1721->power_lock);

	/* hrtimer settings.  we poll for light values using a timer. */
	hrtimer_init(&bh1721->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	bh1721->light_poll_delay = ns_to_ktime(200 * NSEC_PER_MSEC);
	bh1721->timer.function = bh1721_timer_func;

	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	bh1721->wq = create_singlethread_workqueue("bh1721_wq");
	if (!bh1721->wq) {
		ret = -ENOMEM;
		pr_err("%s: could not create workqueue\n", __func__);
		goto err_create_workqueue;
	}
	/* this is the thread function we run on the work queue */
	INIT_WORK(&bh1721->work_light, bh1721_work_func_light);

	/* allocate lightsensor-level input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		ret = -ENOMEM;
		goto err_input_allocate_device_light;
	}
	input_set_drvdata(input_dev, bh1721);
	input_dev->name = "light_sensor";
	input_set_capability(input_dev, EV_ABS, ABS_MISC);
	input_set_abs_params(input_dev, ABS_MISC, 0, 1, 0, 0);

	ret = input_register_device(input_dev);
	if (ret < 0) {
		pr_err("%s: could not register input device\n", __func__);
		input_free_device(input_dev);
		goto err_input_register_device_light;
	}
	bh1721->light_input_dev = input_dev;

	ret = sysfs_create_group(&input_dev->dev.kobj,&light_attribute_group);
	if (ret) {
		printk("Creating bh1721 attribute group failed");
		goto error_device;
	}
	
	/* set sysfs for light sensor test mode*/
	ret = sensors_register(light_sensor_device, bh1721, light_sensor_attrs, "light_sensor");
	if(ret) {
		printk(KERN_ERR "%s: cound not register gyro sensor device(%d).\n", __func__, ret);
	}
	
	printk("[%s]: Light Sensor probe complete.", __func__);
	
	goto done;

error_device:
	sysfs_remove_group(&client->dev.kobj, &light_attribute_group);
err_input_register_device_light:
	input_unregister_device(bh1721->light_input_dev);
err_input_allocate_device_light:
	destroy_workqueue(bh1721->wq);
err_create_workqueue:
	mutex_destroy(&bh1721->power_lock);
	kfree(bh1721);
done:
	return ret;
}

static int bh1721_suspend(struct device *dev)
{
	/* We disable power only if proximity is disabled.  If proximity
	   is enabled, we leave power on because proximity is allowed
	   to wake up device.  We remove power without changing
	   bh1721->power_state because we use that state in resume
	*/
	struct i2c_client *client = to_i2c_client(dev);
	struct bh1721_data *bh1721 = i2c_get_clientdata(client);
	if (bh1721->power_state & LIGHT_ENABLED) bh1721_light_disable(bh1721);
#if defined(CONFIG_TARGET_LOCALE_KOR_LGU)
	if (bh1721->resetpin_down)
		bh1721->resetpin_down();
#endif	
	return 0;
}

static int bh1721_resume(struct device *dev)
{	
	/* Turn power back on if we were before suspend. */
	struct i2c_client *client = to_i2c_client(dev);
	struct bh1721_data *bh1721 = i2c_get_clientdata(client);

	if (bh1721->reset)
		bh1721->reset();
	if (bh1721->power_state & LIGHT_ENABLED)
		bh1721_light_enable(bh1721);
	return 0;

}

static int bh1721_i2c_remove(struct i2c_client *client)
{
	struct bh1721_data *bh1721 = i2c_get_clientdata(client);
	sysfs_remove_group(&bh1721->light_input_dev->dev.kobj, &light_attribute_group);
	input_unregister_device(bh1721->light_input_dev);

	if (bh1721->power_state) 
	{
		bh1721->power_state = 0;
		if (bh1721->power_state & LIGHT_ENABLED)
			bh1721_light_disable(bh1721);
	}
	destroy_workqueue(bh1721->wq);
	mutex_destroy(&bh1721->power_lock);
	kfree(bh1721);
	return 0;
}

static const struct i2c_device_id bh1721_device_id[] = 
{
	{"bh1721", 0},
	{}
}
MODULE_DEVICE_TABLE(i2c, bh1721_device_id);

static const struct dev_pm_ops bh1721_pm_ops = 
{	
	.suspend = bh1721_suspend,
	.resume = bh1721_resume
};


static struct i2c_driver bh1721_i2c_driver = {
	.driver = {
		.name = "bh1721",
		.owner = THIS_MODULE,
		.pm = &bh1721_pm_ops
	},
	.probe		= bh1721_i2c_probe,
	.remove		= bh1721_i2c_remove,
	.id_table	= bh1721_device_id,
};

static int __init bh1721_init(void)
{
	return i2c_add_driver(&bh1721_i2c_driver);
}

static void __exit bh1721_exit(void)
{
	i2c_del_driver(&bh1721_i2c_driver);
}
module_init(bh1721_init);
module_exit(bh1721_exit);

MODULE_AUTHOR("mjchen@sta.samsung.com");
MODULE_DESCRIPTION("Optical Sensor driver for bh1721");
MODULE_LICENSE("GPL");

