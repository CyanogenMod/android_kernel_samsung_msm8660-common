/*
 * Copyright (c) 2010 SAMSUNG
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
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
#include <mach/hardware.h>
#include <linux/wakelock.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <asm/uaccess.h>
#include <linux/mfd/pmic8058.h>
#include <linux/i2c/gp2a.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>


/*********** for debug **********************************************************/
#if 1 
#define gprintk(fmt, x... ) printk( "%s(%d): " fmt, __FUNCTION__ ,__LINE__, ## x)
#else
#define gprintk(x...) do { } while (0)
#endif
/*******************************************************************************/

#if 1 //defined (CONFIG_MACH_STEALTH)
#define PMIC8058_IRQ_BASE				(NR_MSM_IRQS + NR_GPIO_IRQS)
#define IRQ_GP2A_INT PM8058_GPIO_IRQ(PMIC8058_IRQ_BASE, (PM8058_GPIO(14)))  
#define GPIO_PS_VOUT PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(14))
#endif

//#define PROX_MODE_A
#define PROX_MODE_B

#define SENSOR_DEFAULT_DELAY            (200)   /* 200 ms */
#define SENSOR_MAX_DELAY                (2000)  /* 2000 ms */
#define ABS_STATUS                      (ABS_BRAKE)
#define ABS_WAKE                        (ABS_MISC)
#define ABS_CONTROL_REPORT              (ABS_THROTTLE)

/* global var */
static struct wake_lock prx_wake_lock;

static struct i2c_driver opt_i2c_driver;
static struct i2c_client *opt_i2c_client = NULL;
static spinlock_t prox_lock;
/* driver data */
struct opt_gp2a_platform_data {
	void	(*gp2a_led_on) (void);
	void	(*gp2a_led_off) (void);
	void	(*power_on) (void);
	void	(*power_off) (void);
};

struct gp2a_data {
	struct input_dev *input_dev;
	struct work_struct work;  /* for proximity sensor */
	struct mutex enable_mutex;
	struct mutex data_mutex;
	struct class *proximity_class;
	struct device *proximity_dev;

	int		wakeup;		/* configure the button as a wake-up source */
	int   enabled;
	int   delay;
	int   prox_data;
	int   irq;

  	struct kobject *uevent_kobj;

	void	(*gp2a_led_on) (void);
	void	(*gp2a_led_off) (void);
	void	(*power_on) (void);
	void	(*power_off) (void);
};

static struct gp2a_data *prox_data = NULL;

struct opt_state{
	struct i2c_client	*client;	
};

struct opt_state *opt_state;

/* initial value for sensor register */
static u8 gp2a_original_image[8] =
{
#ifdef PROX_MODE_A
	0x00,  
	0x08,  
	0xC2,  
	0x04,
	0x01,
#else
#if defined(CONFIG_KOR_MODEL_SHV_E150S)
	0x00,  
	0x00,  
	0x40,  
	0x04,
	0x03,
#else
	0x00,  
	0x08,  
	0x40,  
	0x04,
	0x03,
#endif //E150S
#endif //PROX_MODE_A	
};

#ifdef PROX_MODE_B
/*INT clear bit*/
#define INT_CLEAR    (1 << 7)

static inline u8 INT_CLR(u8 reg)
{
	return (reg | INT_CLEAR);
}

static inline u8 NOT_INT_CLR(u8 reg)
{
	return (reg & ~(INT_CLEAR));
}
#endif

static int proximity_onoff(u8 onoff);
	
                 
/* Proximity Sysfs interface */
static ssize_t
proximity_delay_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct gp2a_data *data = input_get_drvdata(input_data);
    int delay;

    delay = data->delay;

    return sprintf(buf, "%d\n", delay);
}

static ssize_t
proximity_delay_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct gp2a_data *data = input_get_drvdata(input_data);
    int delay = simple_strtoul(buf, NULL, 10);

    if (delay < 0) {
        return count;
    }

    if (SENSOR_MAX_DELAY < delay) {
        delay = SENSOR_MAX_DELAY;
    }

    data->delay = delay;

    input_report_abs(data->input_dev, ABS_CONTROL_REPORT, (data->enabled<<16) | delay);
	input_sync(data->input_dev);
    return count;
}

static ssize_t
proximity_enable_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct gp2a_data *data = input_get_drvdata(input_data);
    int enabled;

    enabled = data->enabled;

    return sprintf(buf, "%d\n", enabled);
}

static ssize_t
proximity_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct gp2a_data *data = input_get_drvdata(input_data);
    int value = simple_strtoul(buf, NULL, 10);
	char input;
	unsigned long flags;

    if (value != 0 && value != 1) {
        return count;
    }

    if (data->enabled && !value) { 			/* Proximity power off */
        disable_irq(IRQ_GP2A_INT);

		proximity_onoff(0);
		disable_irq_wake(IRQ_GP2A_INT);
		if(data->gp2a_led_off)
			data->gp2a_led_off();
		if(data->power_off)
			data->power_off();
    }
    if (!data->enabled && value) {			/* proximity power on */
		if(data->power_on)
			data->power_on();
		if(data->gp2a_led_on)
			data->gp2a_led_on();
		msleep(1);
		proximity_onoff(1);

		input = 0x01;
		opt_i2c_write((u8)(REGS_OPMOD),&input);

		msleep(50);
		enable_irq_wake(IRQ_GP2A_INT);
		input = gpio_get_value_cansleep(GPIO_PS_VOUT);
		input_report_abs(data->input_dev, ABS_DISTANCE,  input);
		input_sync(data->input_dev);
		gprintk("[PROX] Start proximity = %d\n",input); //Temp
		spin_lock_irqsave(&prox_lock, flags);
		input = 0x03;
		opt_i2c_write((u8)(REGS_OPMOD),&input);

        enable_irq(IRQ_GP2A_INT);
		spin_unlock_irqrestore(&prox_lock, flags);
    }

	data->enabled = value;
    input_report_abs(data->input_dev, ABS_CONTROL_REPORT, (value<<16) | data->delay);
	input_sync(data->input_dev);
    return count;
}

static ssize_t proximity_wake_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
	struct gp2a_data *data = input_get_drvdata(input_data);
    static int cnt = 1;

    input_report_abs(data->input_dev, ABS_WAKE, cnt++);
	input_sync(data->input_dev);
    return count;
}

static ssize_t
proximity_data_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    struct input_dev *input_data = to_input_dev(dev);
	struct gp2a_data *data = input_get_drvdata(input_data);
    int x;

	mutex_lock(&data->data_mutex);
	x = data->prox_data;
	mutex_unlock(&data->data_mutex);
	
    return sprintf(buf, "%d\n", x);
}

static ssize_t proximity_state_show(struct device *dev,	struct device_attribute *attr, char *buf)
{
	u8 proximity_value = 0;
	// char x;

	pr_err("[PROX] %s(%d)\n", __func__, __LINE__);
	proximity_data_show(dev, attr, buf);
	// pr_err("[PROX] raw=%s\n", buf);
    
	if(*buf=='1') {
        proximity_value = sprintf(buf, "1.0\n");
        // x=9;
	} else {
        proximity_value = sprintf(buf, "0.0\n");
        // x=7;
	}

    //	proximity_value = sprintf(buf, "%d\n", x);
	pr_err("[PROX] result=%s\n", buf);
	return proximity_value;
}

static ssize_t proximity_avg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "11,12,13\n");
}

static ssize_t proximity_avg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	return proximity_enable_store(dev, attr, buf, size);
}

static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP,   proximity_delay_show, proximity_delay_store);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,  proximity_enable_show, proximity_enable_store);
static DEVICE_ATTR(wake, S_IWUSR|S_IWGRP,            NULL, proximity_wake_store);
static DEVICE_ATTR(data, S_IRUGO, proximity_data_show, NULL);
static DEVICE_ATTR(proximity_avg, 0644,   proximity_avg_show, proximity_avg_store);
static DEVICE_ATTR(proximity_state, 0644, proximity_state_show, NULL);

static struct attribute *proximity_attributes[] = {
    &dev_attr_delay.attr,
    &dev_attr_enable.attr,
    &dev_attr_wake.attr,
    &dev_attr_data.attr,
	&dev_attr_proximity_state.attr,
	&dev_attr_proximity_avg.attr,
    NULL
};

static struct attribute_group proximity_attribute_group = {
    .attrs = proximity_attributes
};

static char get_ps_vout_value(void)
{
  char value = 0;

#ifdef PROX_MODE_A
#if 1 //defined (CONFIG_MACH_STEALTH)
  value = gpio_get_value_cansleep(GPIO_PS_VOUT);
#else
  value = gpio_get_value(GPIO_PS_VOUT);
#endif
#else //PROX_MODE_A
  opt_i2c_read(0x00, &value, 2);
  value &= 0x01;
  value ^= 0x01;
#endif //PROX_MODE_A

  return value;
}

/*****************************************************************************************
 *  
 *  function    : gp2a_work_func_prox 
 *  description : This function is for proximity sensor (using B-1 Mode ). 
 *                when INT signal is occured , it gets value from VO register.   
 *
 *                 
 */
static void gp2a_work_func_prox(struct work_struct *work)
{
	struct gp2a_data *gp2a = container_of((struct work_struct *)work,
							struct gp2a_data, work);
	
	char value;
#ifdef PROX_MODE_B
    u8 reg = 0;
#endif

    disable_irq(IRQ_GP2A_INT);

    value = get_ps_vout_value(); 

    input_report_abs(gp2a->input_dev, ABS_DISTANCE,  value);
    input_sync(gp2a->input_dev);

	gp2a->prox_data= value;
	gprintk("proximity = %d\n",value); //Temp

#ifdef PROX_MODE_B
    if(value == 1) //VO == 0
    {
      reg = 0x40;
      opt_i2c_write(NOT_INT_CLR(REGS_HYS), &reg);
    }
    else
    {
#if defined(CONFIG_KOR_MODEL_SHV_E150S)
		reg = 0x20;
#else
		reg = 0x27;
#endif
      opt_i2c_write(NOT_INT_CLR(REGS_HYS), &reg);
    }

    reg = 0x18;
    opt_i2c_write(NOT_INT_CLR(REGS_CON), &reg);
#endif //PROX_MODE_B

    enable_irq(IRQ_GP2A_INT);

#ifdef PROX_MODE_B
    reg = 0x00;
    opt_i2c_write(INT_CLR(REGS_CON), &reg);
#endif //PROX_MODE_B
}

irqreturn_t gp2a_irq_handler(int irq, void *dev_id)
{
	
	wake_lock_timeout(&prx_wake_lock, 3*HZ);

    gprintk("\n");

	
	schedule_work(&prox_data->work);

	printk("[PROX] IRQ_HANDLED.\n");
	return IRQ_HANDLED;
}

static int opt_i2c_init(void) 
{
	if( i2c_add_driver(&opt_i2c_driver))
	{
		printk("i2c_add_driver failed \n");
		return -ENODEV;
	}
	return 0;
}


int opt_i2c_read(u8 reg, u8 *val, unsigned int len )
{

	int err;
	u8 buf[2];
	struct i2c_msg msg[2];

	buf[0] = reg; 

	msg[0].addr = opt_i2c_client->addr;
	msg[0].flags = 1;
	
	msg[0].len = len;
	msg[0].buf = buf;
	err = i2c_transfer(opt_i2c_client->adapter, msg, 1);

	*val = buf[1];

    gprintk(": 0x%x, 0x%x \n", reg, *val);
    if (err >= 0) return 0;

    printk("%s %d i2c transfer error\n", __func__, __LINE__);
    return err;
}

int opt_i2c_write( u8 reg, u8 *val )
{
    int err = 0;
    struct i2c_msg msg[1];
    unsigned char data[2];
    int retry = 10;

    if( (opt_i2c_client == NULL) || (!opt_i2c_client->adapter) ){
        return -ENODEV;
    }

    while(retry--)
    {
        data[0] = reg;
        data[1] = *val;

        msg->addr = opt_i2c_client->addr;
        msg->flags = I2C_M_WR;
        msg->len = 2;
        msg->buf = data;

        err = i2c_transfer(opt_i2c_client->adapter, msg, 1);
        gprintk(": 0x%x, 0x%x\n", reg, *val);

        if (err >= 0) return 0;
    }
    printk("%s %d i2c transfer error\n", __func__, __LINE__);
    return err;
}



void gp2a_chip_init(void)
{
  /* Power On */
  gprintk("\n");
}

static int proximity_input_init(struct gp2a_data *data)
	{
	struct input_dev *dev;
	int err;

	dev = input_allocate_device();
	if (!dev) {
		return -ENOMEM;
	}

	set_bit(EV_ABS, dev->evbit);
	input_set_capability(dev, EV_ABS, ABS_DISTANCE);
	input_set_capability(dev, EV_ABS, ABS_STATUS); /* status */
	input_set_capability(dev, EV_ABS, ABS_WAKE); /* wake */
	input_set_capability(dev, EV_ABS, ABS_CONTROL_REPORT); /* enabled/delay */
	input_set_abs_params(dev, ABS_DISTANCE, 0, 128, 0, 0);
	input_set_abs_params(dev, ABS_STATUS, 0, 1, 0, 0);
	input_set_abs_params(dev, ABS_WAKE, 0, 163840, 0, 0);
	input_set_abs_params(dev, ABS_CONTROL_REPORT, 0, 98432, 0, 0);

	dev->name = "proximity_sensor";
	input_set_drvdata(dev, data);

	err = input_register_device(dev);
	if (err < 0) {
		input_free_device(dev);
		return err;
	}
	data->input_dev = dev;
	
	return 0;
}

static int gp2a_opt_probe( struct platform_device* pdev )
{
    struct gp2a_data *gp2a;
	struct opt_gp2a_platform_data *pdata = pdev->dev.platform_data;
    u8 value;
    int err = 0;
	int wakeup = 0;

	/* allocate driver_data */
	gp2a = kzalloc(sizeof(struct gp2a_data),GFP_KERNEL);
	if(!gp2a)
	{
		pr_err("kzalloc error\n");
		return -ENOMEM;
	}

	gp2a->enabled = 0;
	gp2a->delay = SENSOR_DEFAULT_DELAY;

    prox_data = gp2a;

	if(pdata) {
		if(pdata->power_on)
			gp2a->power_on = pdata->power_on;
		if(pdata->power_off)
			gp2a->power_off = pdata->power_off;
		if(pdata->gp2a_led_on)
			gp2a->gp2a_led_on = pdata->gp2a_led_on;
		if(pdata->gp2a_led_off)
			gp2a->gp2a_led_off = pdata->gp2a_led_off;
	}

	if(gp2a->power_on)
		gp2a->power_on();

	/* init i2c */
	opt_i2c_init();
	/* Check if the device is there or not. */
	err = opt_i2c_read(0x00, &value, 2);
	if (err != 0) {
		pr_err("%s: check i2c line or prox/als sensor chip\n", __func__);
		goto error_setup_reg;
	}

	mutex_init(&gp2a->enable_mutex);
	mutex_init(&gp2a->data_mutex);

//	if(gp2a->wake) {
		wakeup = 1;
//	}

	INIT_WORK(&gp2a->work, gp2a_work_func_prox);
	
	err = proximity_input_init(gp2a);
	if(err < 0) {
		goto error_setup_reg;
	}

	err = sysfs_create_group(&gp2a->input_dev->dev.kobj,
				&proximity_attribute_group);
	if(err < 0)
	{
		goto err_sysfs_create_group_proximity;
	}

	/* set platdata */
	platform_set_drvdata(pdev, gp2a);

    gp2a->uevent_kobj = &pdev->dev.kobj;

	/* wake lock init */
	wake_lock_init(&prx_wake_lock, WAKE_LOCK_SUSPEND, "prx_wake_lock");
	spin_lock_init(&prox_lock);

	if(opt_i2c_client == NULL)
	{
		pr_err("opt_probe failed : i2c_client is NULL\n"); 
		return -ENODEV;
	}
	else
		printk("opt_i2c_client : (0x%p)\n",opt_i2c_client);
	

	/* GP2A Regs INIT SETTINGS */
#ifdef PROX_MODE_A
	value = 0x00;
#else
	value = 0x02;
#endif //PROX_MODE_A
	opt_i2c_write((u8)(REGS_OPMOD),&value);

	printk("gp2a_opt_probe is OK!!\n");
	
	/* INT Settings */	
  	err = request_threaded_irq( IRQ_GP2A_INT , 
		NULL, gp2a_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING	, "proximity_int", gp2a);

	if (err < 0) {
		printk(KERN_ERR "failed to request proximity_irq\n");
		goto error_setup_reg;
	}

    disable_irq(IRQ_GP2A_INT);
	
	/* set sysfs for proximity sensor */
	gp2a->proximity_class = class_create(THIS_MODULE, "proximity");
	if (IS_ERR(gp2a->proximity_class)) {
		pr_err("%s: could not create proximity_class\n", __func__);
		goto err_proximity_class_create;
	}

	gp2a->proximity_dev = device_create(gp2a->proximity_class,
						NULL, 0, NULL, "proximity");
	if (IS_ERR(gp2a->proximity_dev)) {
		pr_err("%s: could not create proximity_dev\n", __func__);
		goto err_proximity_device_create;
	}

	if (device_create_file(gp2a->proximity_dev,
		&dev_attr_proximity_state) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
			dev_attr_proximity_state.attr.name);
		goto err_proximity_device_create_file1;
	}

	if (device_create_file(gp2a->proximity_dev,
		&dev_attr_proximity_avg) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
			dev_attr_proximity_avg.attr.name);
		goto err_proximity_device_create_file2;
	}
	dev_set_drvdata(gp2a->proximity_dev, gp2a);

	device_init_wakeup(&pdev->dev, wakeup);
	return 0;

err_proximity_device_create_file2:
	device_remove_file(gp2a->proximity_dev, &dev_attr_proximity_state);
err_proximity_device_create_file1:
	device_destroy(gp2a->proximity_class, 0);
err_proximity_device_create:
	class_destroy(gp2a->proximity_class);
err_proximity_class_create:
	sysfs_remove_group(&gp2a->input_dev->dev.kobj,
			   &proximity_attribute_group);
err_sysfs_create_group_proximity:
	input_unregister_device(gp2a->input_dev);
	input_free_device(gp2a->input_dev);
error_setup_reg:
	if(gp2a->power_off)
		gp2a->power_off();
	kfree(gp2a);
	pr_err("%s: failed\n", __func__);
	return err;
}

static int gp2a_opt_remove( struct platform_device* pdev )
{
  struct gp2a_data *gp2a = platform_get_drvdata(pdev);

	if (gp2a->proximity_class!= NULL) {
		device_remove_file(gp2a->proximity_dev,
			&dev_attr_proximity_avg);
		device_remove_file(gp2a->proximity_dev, &dev_attr_proximity_avg);
		device_remove_file(gp2a->proximity_dev, &dev_attr_proximity_state);
		device_destroy(gp2a->proximity_class, 0);
		class_destroy(gp2a->proximity_class);
	}
  if (gp2a->input_dev!= NULL) {
    sysfs_remove_group(&gp2a->input_dev->dev.kobj,
				&proximity_attribute_group);
    input_unregister_device(gp2a->input_dev);
    if (gp2a->input_dev != NULL) {
        kfree(gp2a->input_dev);
    }
  }
	device_init_wakeup(&pdev->dev, 0);

  kfree(gp2a);

  return 0;
}

static int gp2a_opt_suspend( struct platform_device* pdev, pm_message_t state )
{
	struct gp2a_data *gp2a = platform_get_drvdata(pdev);
	
	gprintk("\n");

	if(gp2a->enabled) // calling
	{
      if (device_may_wakeup(&pdev->dev))
        enable_irq_wake(IRQ_GP2A_INT);

	   gprintk("The timer is cancled.\n");
	}

	if(gp2a->power_off)
		gp2a->power_off();

	return 0;
}

static int gp2a_opt_resume( struct platform_device* pdev )
{
	struct gp2a_data *gp2a = platform_get_drvdata(pdev);

	gprintk("\n");

	if(gp2a->power_on)
		gp2a->power_on();
	if(gp2a->enabled) //calling
	{
      if (device_may_wakeup(&pdev->dev))
			enable_irq_wake(IRQ_GP2A_INT);

      gprintk("The timer is cancled.\n");
	}

    return 0;
}

static int proximity_onoff(u8 onoff)
{
	u8 value;
    int i;
       
	if(onoff)
	{
       	for(i=1;i<5;i++)
       	{
       		opt_i2c_write((u8)(i),&gp2a_original_image[i]);
    	}
	}
	else
	{
#ifdef PROX_MODE_A
		value = 0x00;
#else
		value = 0x02;
#endif //PROX_MODE_A	
		opt_i2c_write((u8)(REGS_OPMOD),&value);
	}
	
	return 0;
}
static int opt_i2c_remove(struct i2c_client *client)
{
    struct opt_state *data = i2c_get_clientdata(client);

	kfree(data);
	opt_i2c_client = NULL;

	return 0;
}

static int opt_i2c_probe(struct i2c_client *client,  const struct i2c_device_id *id)
{
	struct opt_state *opt;

    gprintk("\n");
	pr_err("%s, %d : start!!!\n", __func__, __LINE__);

	opt = kzalloc(sizeof(struct opt_state), GFP_KERNEL);
	if (opt == NULL) {		
		printk("failed to allocate memory \n");
		pr_err("kzalloc error\n");
		pr_err("%s, %d : error!!!\n", __func__, __LINE__);
		return -ENOMEM;
	}
	
	opt->client = client;
	i2c_set_clientdata(client, opt);
	
	/* rest of the initialisation goes here. */
	
	printk("GP2A opt i2c attach success!!!\n");
	pr_err("GP2A opt i2c attach success!!!\n");

	opt_i2c_client = client;
	pr_err("%s, %d : end!!!\n", __func__, __LINE__);

	return 0;
}


static const struct i2c_device_id opt_device_id[] = {
	{"gp2a", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, opt_device_id);

static struct i2c_driver opt_i2c_driver = {
	.driver = {
		.name = "gp2a",
		.owner= THIS_MODULE,
	},
	.probe		= opt_i2c_probe,
	.remove		= opt_i2c_remove,
	.id_table	= opt_device_id,
};


static struct platform_driver gp2a_opt_driver = {
	.probe 	 = gp2a_opt_probe,
    .remove = gp2a_opt_remove,
	.suspend = gp2a_opt_suspend,
	.resume  = gp2a_opt_resume,
	.driver  = {
		.name = "gp2a-opt",
		.owner = THIS_MODULE,
	},
};

#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

static int __init gp2a_opt_init(void)
{
	int ret;
	
#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return 0\n", __func__);
		return 0;
	}
#endif
	
	ret = platform_driver_register(&gp2a_opt_driver);
	return ret;
	
	
}
static void __exit gp2a_opt_exit(void)
{
	platform_driver_unregister(&gp2a_opt_driver);
}

module_init( gp2a_opt_init );
module_exit( gp2a_opt_exit );

MODULE_AUTHOR("SAMSUNG");
MODULE_DESCRIPTION("Optical Sensor driver for GP2AP002A00F");
MODULE_LICENSE("GPL");

