/*
 * ak8975.c - ak8975 compass driver
 *
 * Copyright (C) 2008-2009 HTC Corporation.
 * Author: viral wang <viralwang@gmail.com>
 *
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c/ak8975.h>
#include <linux/completion.h>
#include <linux/mfd/pmic8058.h>
#include "ak8975-reg.h"

#ifdef SENSORS_LOG_DUMP
#include <linux/i2c/sensors_core.h>
#endif


struct akm8975_data {
	struct i2c_client *this_client;
	struct akm8975_platform_data *pdata;
	struct mutex lock;
	struct miscdevice akmd_device;
	struct completion data_ready;
#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined(CONFIG_CAN_MODEL_SGH_I757M) ||  defined(CONFIG_USA_MODEL_SGH_T769)
	struct class *akm8975_class;
	struct device *akm8975_dev;
#endif
	wait_queue_head_t state_wq;
	int irq;
	void	(*power_on) (void);
	void	(*power_off) (void);
};
#if defined (CONFIG_KOR_MODEL_SHV_E120L)||defined (CONFIG_KOR_MODEL_SHV_E120S)||defined(CONFIG_KOR_MODEL_SHV_E120K)
#define SENSE_SCL	52
#define SENSE_SDA	51
extern int gpio_request_count;
extern int gpio_free_count;
#endif


#define PMIC8058_IRQ_BASE				(NR_MSM_IRQS + NR_GPIO_IRQS)

static s32 akm8975_ecs_set_mode_power_down(struct akm8975_data *akm)
{
	s32 ret;
	ret = i2c_smbus_write_byte_data(akm->this_client,
			AK8975_REG_CNTL, AK8975_MODE_POWER_DOWN);
	return ret;
}

static int akm8975_ecs_set_mode(struct akm8975_data *akm, char mode)
{
	s32 ret;
	
	switch (mode) {
	case AK8975_MODE_SNG_MEASURE:
		ret = i2c_smbus_write_byte_data(akm->this_client,
				AK8975_REG_CNTL, AK8975_MODE_SNG_MEASURE);
		if (ret < 0)
			pr_err("%s: fail to set AK8975_MODE_SNG_MEASURE ret=%d\n", __func__, ret);
		break;
	case AK8975_MODE_FUSE_ACCESS:
		ret = i2c_smbus_write_byte_data(akm->this_client,
				AK8975_REG_CNTL, AK8975_MODE_FUSE_ACCESS);
		if (ret < 0)
			pr_err("%s: fail to set AK8975_MODE_FUSE_ACCESS ret=%d\n", __func__, ret);
		else
			printk("%s: ak8975 set AK8975_MODE_FUSE_ACCESS\n", __func__);
		break;
	case AK8975_MODE_POWER_DOWN:
		ret = akm8975_ecs_set_mode_power_down(akm);
		if (ret < 0)
			pr_err("%s: fail to set AK8975_MODE_POWER_DOWN ret=%d\n", __func__, ret);
		else
			printk("%s: ak8975 set AK8975_MODE_POWER_DOWN\n", __func__);
		break;
	case AK8975_MODE_SELF_TEST:
		ret = i2c_smbus_write_byte_data(akm->this_client,
				AK8975_REG_CNTL, AK8975_MODE_SELF_TEST);
		if (ret < 0)
			pr_err("%s: fail to set AK8975_MODE_SELF_TEST ret=%d\n", __func__, ret);
		else
			printk("%s: ak8975 set AK8975_MODE_SELF_TEST\n", __func__);
		break;
	default:
		return -EINVAL;
	}

	if (ret < 0)
		return ret;

	/* Wait at least 300us after changing mode. */
	udelay(300);

	return 0;
}

static int akmd_copy_in(unsigned int cmd, void __user *argp,
			void *buf, size_t buf_size)
{
	if (!(cmd & IOC_IN))
		return 0;
	if (_IOC_SIZE(cmd) > buf_size)
		return -EINVAL;
	if (copy_from_user(buf, argp, _IOC_SIZE(cmd)))
		return -EFAULT;
	return 0;
}

static int akmd_copy_out(unsigned int cmd, void __user *argp,
			 void *buf, size_t buf_size)
{
	if (!(cmd & IOC_OUT))
		return 0;
	if (_IOC_SIZE(cmd) > buf_size)
		return -EINVAL;
	if (copy_to_user(argp, buf, _IOC_SIZE(cmd)))
		return -EFAULT;
	return 0;
}

static void akm8975_disable_irq(struct akm8975_data *akm)
{
	disable_irq(akm->irq);
	if (try_wait_for_completion(&akm->data_ready)) {
		/* we actually got the interrupt before we could disable it
		 * so we need to enable again to undo our disable since the
		 * irq_handler already disabled it
		 */
		enable_irq(akm->irq);
	}
}

static irqreturn_t akm8975_irq_handler(int irq, void *data)
{
	struct akm8975_data *akm = data;
	disable_irq_nosync(irq);
	complete(&akm->data_ready);
	return IRQ_HANDLED;
}

static int akm8975_wait_for_data_ready(struct akm8975_data *akm)
{
    int data_ready = gpio_get_value_cansleep(akm->pdata->gpio_data_ready_gpio);
	int err;

	if (data_ready)
		return 0;

	enable_irq(akm->irq);

	err = wait_for_completion_timeout(&akm->data_ready, 5*HZ);
	if (err > 0)
		return 0;

	akm8975_disable_irq(akm);

	if (err == 0) {
		pr_err("akm: wait timed out\n");
		akm8975_ecs_set_mode_power_down(akm); /* It will be restart next time */
		
		/* Wait at least 300us after changing mode. */
		udelay(300);
//		return -ETIMEDOUT;
	}

	pr_err("akm: wait restart\n");
	return err;
}

#if defined (CONFIG_EUR_MODEL_GT_I9210)
extern unsigned int get_hw_rev(void);
#endif

static ssize_t akmd_read(struct file *file, char __user *buf,
					size_t count, loff_t *pos)
{
	struct akm8975_data *akm = container_of(file->private_data,
			struct akm8975_data, akmd_device);
	short x = 0, y = 0, z = 0;
#if defined (CONFIG_EUR_MODEL_GT_I9210)
	short tmp = 0;
#endif
	int ret;
	u8 data[8];

	mutex_lock(&akm->lock);
	ret = akm8975_ecs_set_mode(akm, AK8975_MODE_SNG_MEASURE);
	if (ret) {
		mutex_unlock(&akm->lock);
		goto done;
	}
	ret = akm8975_wait_for_data_ready(akm);
	if (ret) {
		mutex_unlock(&akm->lock);
		goto done;
	}
	ret = i2c_smbus_read_i2c_block_data(akm->this_client, AK8975_REG_ST1,
						sizeof(data), data);
	mutex_unlock(&akm->lock);

	if (ret != sizeof(data)) {
		pr_err("%s: failed to read %d bytes of mag data\n",
		       __func__, sizeof(data));
		goto done;
	}

	if (data[0] & 0x01) {
		x = (data[2] << 8) + data[1];
		y = (data[4] << 8) + data[3];
		z = (data[6] << 8) + data[5];
#if defined (CONFIG_EUR_MODEL_GT_I9210)
		if (get_hw_rev() >= 6 )
		{
			tmp = x;
			x = y;
			y = tmp;
		}
#endif
	} else
		pr_err("%s: invalid raw data(st1 = %d)\n",
					__func__, data[0] & 0x01);

done:
	return sprintf(buf, "%d,%d,%d\n", x, y, z);
}

#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined(CONFIG_CAN_MODEL_SGH_I757M) || defined(CONFIG_USA_MODEL_SGH_T769)  
/* sysfs for logging Power line noise */
static ssize_t akm8975_rawdata_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct akm8975_data *akm = dev_get_drvdata(dev);
	short x = 0, y = 0, z = 0;
	int ret;
	u8 data[8];

	mutex_lock(&akm->lock);
	ret = akm8975_ecs_set_mode(akm, AK8975_MODE_SNG_MEASURE);
	if (ret) {
		mutex_unlock(&akm->lock);
		goto done;
	}
	ret = akm8975_wait_for_data_ready(akm);
	if (ret) {
		mutex_unlock(&akm->lock);
		goto done;
	}
	ret = i2c_smbus_read_i2c_block_data(akm->this_client, AK8975_REG_ST1,
						sizeof(data), data);
	mutex_unlock(&akm->lock);

	if (ret != sizeof(data)) {
		pr_err("%s: failed to read %d bytes of mag data\n",
		       __func__, sizeof(data));
		goto done;
	}

	if (data[0] & 0x01) {
		x = (data[2] << 8) + data[1];
		y = (data[4] << 8) + data[3];
		z = (data[6] << 8) + data[5];
	} else
		pr_err("%s: invalid raw data(st1 = %d)\n",
					__func__, data[0] & 0x01);

done:
	return sprintf(buf, "%d,%d,%d\n", x, y, z);
}

static DEVICE_ATTR(raw_data,0664,akm8975_rawdata_show,NULL);
#endif

static long akmd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct akm8975_data *akm = container_of(file->private_data,
			struct akm8975_data, akmd_device);
	int ret;
	union {
		char raw[RWBUF_SIZE];
		int status;
		char mode;
		u8 data[8];
	} rwbuf;
	
	ret = akmd_copy_in(cmd, argp, rwbuf.raw, sizeof(rwbuf));
	if (ret)
		return ret;

	switch (cmd) {
	case ECS_IOCTL_WRITE:
		if ((rwbuf.raw[0] < 2) || (rwbuf.raw[0] > (RWBUF_SIZE - 1)))
			return -EINVAL;
		if (copy_from_user(&rwbuf.raw[2], argp+2, rwbuf.raw[0]-1))
			return -EFAULT;

		ret = i2c_smbus_write_i2c_block_data(akm->this_client,
						     rwbuf.raw[1],
						     rwbuf.raw[0] - 1,
						     &rwbuf.raw[2]);
		break;
	case ECS_IOCTL_READ:
		if ((rwbuf.raw[0] < 1) || (rwbuf.raw[0] > (RWBUF_SIZE - 1)))
			return -EINVAL;

		ret = i2c_smbus_read_i2c_block_data(akm->this_client,
						    rwbuf.raw[1],
						    rwbuf.raw[0],
						    &rwbuf.raw[1]);
		if (ret < 0)
			return ret;
		if (copy_to_user(argp+1, rwbuf.raw+1, rwbuf.raw[0]))
			return -EFAULT;
		return 0;
	case ECS_IOCTL_SET_MODE:
		mutex_lock(&akm->lock);
		ret = akm8975_ecs_set_mode(akm, rwbuf.mode);
		mutex_unlock(&akm->lock);
		break;
	case ECS_IOCTL_GETDATA:
		mutex_lock(&akm->lock);
		ret = akm8975_wait_for_data_ready(akm);
		if (ret) {
			mutex_unlock(&akm->lock);
			return ret;
		}
		ret = i2c_smbus_read_i2c_block_data(akm->this_client,
						    AK8975_REG_ST1,
						    sizeof(rwbuf.data),
						    rwbuf.data);
		mutex_unlock(&akm->lock);
		if (ret != sizeof(rwbuf.data)) {
			pr_err("%s : failed to read %d bytes of mag data\n",
			       __func__, sizeof(rwbuf.data));
			return -EIO;
		}
		break;
	default:
		return -ENOTTY;
	}

	if (ret < 0)
		return ret;

	return akmd_copy_out(cmd, argp, rwbuf.raw, sizeof(rwbuf));
}

static int akmd_suspend(struct device *dev)
{
	struct akm8975_data *data = dev_get_drvdata(dev);
	int res = 0;

	akm8975_ecs_set_mode_power_down(data);

	/* Wait at least 300us after changing mode. */
	udelay(300);

	if(data->power_off)
		data->power_off();
#if defined (CONFIG_KOR_MODEL_SHV_E120L)||defined (CONFIG_KOR_MODEL_SHV_E120S)||defined(CONFIG_KOR_MODEL_SHV_E120K)
	if(gpio_free_count != 0){
		 gpio_direction_input(SENSE_SCL);
		 gpio_direction_input(SENSE_SDA);
		 gpio_free(SENSE_SCL);
		 gpio_free(SENSE_SDA);
		 gpio_free_count = 0;
		}
	else{
		gpio_free_count =1;
		}
#endif

	return res;
}

static int akmd_resume(struct device *dev)
{
	int res = 0;
	struct akm8975_data *data = dev_get_drvdata(dev);

#if defined (CONFIG_KOR_MODEL_SHV_E120L)||defined (CONFIG_KOR_MODEL_SHV_E120S)||defined(CONFIG_KOR_MODEL_SHV_E120K)
	if(gpio_request_count == 0){
		gpio_request(SENSE_SCL,"SENSE_SCL");
		gpio_request(SENSE_SDA,"SENSE_SDA");
		gpio_request_count =1;
		}
	else{
		gpio_request_count = 0;
		}
#endif
	if(data->power_on)
		data->power_on();

	akm8975_ecs_set_mode_power_down(data);

	/* Wait at least 300us after changing mode. */
	udelay(300);

	return res;
}

static const struct dev_pm_ops akm8975_pm_ops = {
	.suspend = akmd_suspend,
	.resume = akmd_resume,
};

static const struct file_operations akmd_fops = {
	.owner = THIS_MODULE,
	.open = nonseekable_open,
	.read = akmd_read,
	.unlocked_ioctl = akmd_ioctl,
};

static int akm8975_setup_irq(struct akm8975_data *akm)
{
	int rc = -EIO;
	struct akm8975_platform_data *pdata = akm->pdata;
	int irq;

#if 0
	rc = gpio_request(pdata->gpio_data_ready_int, "gpio_akm_int");
	if (rc < 0) {
		pr_err("%s: gpio %d request failed (%d)\n",
			__func__, pdata->gpio_data_ready_int, rc);
		return rc;
	}

	rc = gpio_direction_input(pdata->gpio_data_ready_int);
	if (rc < 0) {
		pr_err("%s: failed to set gpio %d as input (%d)\n",
			__func__, pdata->gpio_data_ready_int, rc);
		goto err_gpio_direction_input;
	}

	irq = gpio_to_irq(pdata->gpio_data_ready_int);
#else
	irq = pdata->gpio_data_ready_int;
#endif

	/* trigger high so we don't miss initial interrupt if it
	 * is already pending
	 */
	rc = request_threaded_irq(irq, NULL, akm8975_irq_handler, IRQF_TRIGGER_RISING,
			 "akm_int", akm);
	if (rc < 0) {
		pr_err("%s: request_irq(%d) failed for gpio %d (%d)\n",
			__func__, irq,
			pdata->gpio_data_ready_int, rc);
		goto err_request_irq;
	}

	/* start with interrupt disabled until the driver is enabled */
	akm->irq = irq;
	akm8975_disable_irq(akm);

	goto done;

err_request_irq:
//err_gpio_direction_input:
//	gpio_free(pdata->gpio_data_ready_int);
done:
	return rc;
}

extern unsigned int get_hw_rev(void);

int akm8975_probe(struct i2c_client *client,
		const struct i2c_device_id *devid)
{
	struct akm8975_data *akm;
	int err = 0, which = 0;
	struct akm8975_platform_data *pdata = client->dev.platform_data;

	printk("ak8975 probe start!\n");

	if (pdata == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		which = 0x01;
		goto exit_platform_data_null;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C check failed, exiting.\n");
		err = -ENODEV;
		which = 0x02;
		goto exit_check_functionality_failed;
	}

	akm = kzalloc(sizeof(struct akm8975_data), GFP_KERNEL);
	if (!akm) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		err = -ENOMEM;
		which = 0x03;
		goto exit_alloc_data_failed;
	}

	akm->pdata = pdata;

	if(pdata->power_on)
		akm->power_on = pdata->power_on;
	if(pdata->power_off)
		akm->power_off = pdata->power_off;

#if defined (CONFIG_KOR_MODEL_SHV_E110S) || defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined(CONFIG_EUR_MODEL_GT_I9210) \
     ||	 defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_USA_MODEL_SGH_T769) || defined (CONFIG_JPN_MODEL_SC_05D)
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	if (get_hw_rev() >= 0x04 )
#elif  defined(CONFIG_USA_MODEL_SGH_I577)
	if (get_hw_rev() >= 0x06 )
#else
	if (get_hw_rev() >= 0x08 )
#endif
	{
	/* For Magnetic sensor POR condition */
	if(pdata->power_on_mag)
		pdata->power_on_mag();
	msleep(1);
	if(pdata->power_off_mag)
		pdata->power_off_mag();
	msleep(10);
	/* For Magnetic sensor POR condition */ 
	}
#endif

	if(akm->power_on)
		akm->power_on();

	mutex_init(&akm->lock);
	init_completion(&akm->data_ready);

	i2c_set_clientdata(client, akm);
	akm->this_client = client;

	err = akm8975_ecs_set_mode_power_down(akm);
	if (err < 0) {
		which = 0x04;
		goto exit_set_mode_power_down_failed;
	}

	err = akm8975_setup_irq(akm);
	if (err) {
		pr_err("%s: could not setup irq\n", __func__);
		which = 0x05;
		goto exit_setup_irq;
	}

	akm->akmd_device.minor = MISC_DYNAMIC_MINOR;
	akm->akmd_device.name = "akm8975";
	akm->akmd_device.fops = &akmd_fops;

	err = misc_register(&akm->akmd_device);
	if (err) {
		which = 0x06;
		goto exit_akmd_device_register_failed;
	}

	#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined(CONFIG_CAN_MODEL_SGH_I757M) || defined(CONFIG_USA_MODEL_SGH_T769)
	/* creating class/device for test */
	akm->akm8975_class = class_create(THIS_MODULE, "magnetometer");
	if(IS_ERR(akm->akm8975_class)) {
		pr_err("%s: class create failed(magnetometer)\n", __func__);
		err = PTR_ERR(akm->akm8975_class);
		goto exit_class_create_failed;
	}

	akm->akm8975_dev = device_create(akm->akm8975_class, NULL, 0, "%s", "magnetometer");
	if(IS_ERR(akm->akm8975_dev)) {
		pr_err("%s: device create failed(magnetometer)\n", __func__);
		err = PTR_ERR(akm->akm8975_dev);
		goto exit_device_create_failed;
	}

	err = device_create_file(akm->akm8975_dev, &dev_attr_raw_data);
	if (err < 0) {
		pr_err("%s: failed to create device file(%s)\n", __func__, dev_attr_raw_data.attr.name);
		goto exit_device_create_file_failed;
	}

	dev_set_drvdata(akm->akm8975_dev, akm);
#endif

	init_waitqueue_head(&akm->state_wq);

	printk("ak8975 probe success!\n");
#ifdef SENSORS_LOG_DUMP
	sensors_status_set_magnetic(0, 0);
#endif

	return 0;
#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined(CONFIG_CAN_MODEL_SGH_I757M) || defined(CONFIG_USA_MODEL_SGH_T769)
exit_device_create_file_failed:
	device_destroy(akm->akm8975_class, 0);
exit_device_create_failed:
	class_destroy(akm->akm8975_class);
exit_class_create_failed:
	misc_deregister(&akm->akmd_device);
#endif	
exit_akmd_device_register_failed:
	free_irq(akm->irq, akm);
//	gpio_free(akm->pdata->gpio_data_ready_int);
exit_setup_irq:
exit_set_mode_power_down_failed:
	if(akm->power_off)
		akm->power_off();
	mutex_destroy(&akm->lock);
	kfree(akm);
exit_alloc_data_failed:
exit_check_functionality_failed:
exit_platform_data_null:
#ifdef SENSORS_LOG_DUMP
	sensors_status_set_magnetic(which, err);
#endif
	return err;
}

static int __devexit akm8975_remove(struct i2c_client *client)
{
	struct akm8975_data *akm = i2c_get_clientdata(client);

	misc_deregister(&akm->akmd_device);
	free_irq(akm->irq, akm);
//	gpio_free(akm->pdata->gpio_data_ready_int);
	mutex_destroy(&akm->lock);
	kfree(akm);
	return 0;
}

static const struct i2c_device_id akm8975_id[] = {
	{AKM8975_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver akm8975_driver = {
	.probe		= akm8975_probe,
	.remove		= akm8975_remove,
	.id_table	= akm8975_id,
	.driver = {
		.pm = &akm8975_pm_ops,
		.name = AKM8975_I2C_NAME,
	},
};

#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

static int __init akm8975_init(void)
{
#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return 0\n", __func__);
		return 0;
	}
#endif

	return i2c_add_driver(&akm8975_driver);
}

static void __exit akm8975_exit(void)
{
	i2c_del_driver(&akm8975_driver);
}

module_init(akm8975_init);
module_exit(akm8975_exit);

MODULE_DESCRIPTION("AKM8975 compass driver");
MODULE_LICENSE("GPL");
