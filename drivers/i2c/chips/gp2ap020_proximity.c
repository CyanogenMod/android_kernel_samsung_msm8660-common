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
#include <linux/i2c/gp2ap020.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>


/*********** for debug **********************************************************/
#if 1 
#define gprintk(fmt, x... ) printk( "%s(%d): " fmt, __FUNCTION__ ,__LINE__, ## x)
#else
#define gprintk(x...) do { } while (0)
#endif
/*******************************************************************************/

#define SENSOR_DEFAULT_DELAY            (200)   /* 200 ms */
#define SENSOR_MAX_DELAY                (2000)  /* 2000 ms */
#define ABS_STATUS                      (ABS_BRAKE)
#define ABS_WAKE                        (ABS_MISC)
#define ABS_CONTROL_REPORT              (ABS_THROTTLE)
#define PROX_READ_NUM	10



/* global var */
static struct wake_lock prx_wake_lock;

static struct i2c_driver opt_i2c_driver;
static struct i2c_client *opt_i2c_client = NULL;

int proximity_enable = 0;
/* driver data */
struct opt_gp2a_platform_data {
	void	(*gp2a_led_on) (void);
	void	(*gp2a_led_off) (void);
	void	(*power_on) (void);
	void	(*power_off) (void);
    int gp2a_irq;
    int gp2a_gpio;
#if defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K)
    int gp2a_irq_rev08;
    int gp2a_gpio_rev08;
#endif
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
	int   average[PROX_READ_NUM]; //for proximity adc average
  	struct kobject *uevent_kobj;

	void	(*gp2a_led_on) (void);
	void	(*gp2a_led_off) (void);
	void	(*power_on) (void);
	void	(*power_off) (void);
    int gp2a_irq;
    int gp2a_gpio;
};


static struct gp2a_data *prox_data = NULL;

struct opt_state{
	struct i2c_client	*client;	
};

struct opt_state *opt_state;

/* initial value for sensor register */
#define COL 8
static u8 gp2a_original_image[COL][2] =
{
  //{Regster, Value}
 	{0x01 , 0x63},   //PRST :01(4 cycle at Detection/Non-detection),ALSresolution :16bit, range *128   //0x1F -> 5F by sharp 
	{0x02 , 0x72},   //ALC : 0, INTTYPE : 1, PS mode resolution : 12bit, range*1
	{0x03 , 0x3C},   //LED drive current 110mA, Detection/Non-detection judgment output
//	{0x04 , 0x00},
//	{0x05 , 0x00},
//	{0x06 , 0xFF},
//	{0x07 , 0xFF},
	{0x08 , 0x07},  //PS mode LTH(Loff):  (??mm)
	{0x09 , 0x00},  //PS mode LTH(Loff) : 
	{0x0A , 0x08},  //PS mode HTH(Lon) : (??mm)
	{0x0B , 0x00},  // PS mode HTH(Lon) :
//  	{0x13 , 0x08}, // by sharp // for internal calculation (type:0)
	{0x00 , 0xC0}   //alternating mode (PS+ALS), TYPE=1(0:externel 1:auto calculated mode) //umfa.cal
};

static int alt_int = 0;
extern u8 lightsensor_mode; // 0 = low, 1 = high
extern unsigned int get_hw_rev(void);

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

    input_report_abs(input_data, ABS_CONTROL_REPORT, (data->enabled<<16) | delay);

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
proximity_enable_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    struct gp2a_data *data = input_get_drvdata(input_data);
    int value = simple_strtoul(buf, NULL, 10);
	char input;
	//signed char get_buf[18]={0,};//test

    if (value != 0 && value != 1) {
        return count;
    }

    if (data->enabled && !value) { 			/* Proximity power off */
		disable_irq(alt_int);

		proximity_enable = value;
		
		proximity_onoff(0);
		disable_irq_wake(alt_int);
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

		proximity_enable = value;
		
		proximity_onoff(1);
		enable_irq_wake(alt_int);
		msleep(160);
#if defined(CONFIG_KOR_MODEL_SHV_E120L)
		if(get_hw_rev() >= 0x01) // HW_REV00 does not work gp2a sensor 
			input = gpio_get_value_cansleep(data->gp2a_gpio);
		else
			input = 1;
#else
		if(get_hw_rev() >= 0x08) 
            input = gpio_get_value_cansleep(data->gp2a_gpio);
        else
            input = !gpio_get_value_cansleep(data->gp2a_gpio);
#endif
	    input_report_abs(data->input_dev, ABS_DISTANCE,  input);
	    input_sync(data->input_dev);

        enable_irq(alt_int);

		printk("[PROXIMITY] enabled proximity = %d [0=closed, 1=far]\n", input);
    }
    data->enabled = value;

    input_report_abs(input_data, ABS_CONTROL_REPORT, (value<<16) | data->delay);

    return count;
}

static ssize_t
proximity_wake_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
    struct input_dev *input_data = to_input_dev(dev);
    static int cnt = 1;

    input_report_abs(input_data, ABS_WAKE, cnt++);

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


static int count = 0; //count for proximity average 

static ssize_t proximity_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{

    struct input_dev *input_data = to_input_dev(dev);
	struct gp2a_data *data = input_get_drvdata(input_data);
	// int value;

	int D2_data;
    unsigned char get_D2_data[2]={0,};//test

	msleep(20);
	opt_i2c_read(0x10, get_D2_data, sizeof(get_D2_data));
    D2_data =(get_D2_data[1] << 8) | get_D2_data[0];
	
    data->average[count]=D2_data;
	count++;
	if(count == PROX_READ_NUM) count=0;

	return sprintf(buf, "%d\n", D2_data);
}

static ssize_t proximity_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
 struct input_dev *input_data = to_input_dev(dev);
 struct gp2a_data *data = input_get_drvdata(input_data);

 int min = 0, max = 0, avg = 0;
 int i;
 int proximity_value = 0;

     for (i = 0; i < PROX_READ_NUM; i++) {
 	   proximity_value = data->average[i]; 
       if(proximity_value > 0){

		 avg += proximity_value;
 
		 if (!i)
			 min = proximity_value;
		 else if (proximity_value < min)
			 min = proximity_value;
 
		 if (proximity_value > max)
			 max = proximity_value;
      	}
	 }
	 avg /= i;

	return sprintf(buf, "%d, %d, %d\n",min,avg,max);
}



static ssize_t proximity_avg_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	return proximity_enable_store(dev, attr, buf, size);
}

static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP,
        proximity_delay_show, proximity_delay_store);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
        proximity_enable_show, proximity_enable_store);
static DEVICE_ATTR(wake, S_IWUSR|S_IWGRP,
        NULL, proximity_wake_store);
static DEVICE_ATTR(data, S_IRUGO, proximity_data_show, NULL);
static DEVICE_ATTR(proximity_avg, 0644,
		   proximity_avg_show, proximity_avg_store);
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


static char get_ps_vout_value(int gp2a_gpio)
{
	char value = 0;

#if defined(CONFIG_KOR_MODEL_SHV_E120L)
	if(get_hw_rev() >= 0x01) // HW_REV00 does not work gp2a sensor 
		value = gpio_get_value_cansleep(gp2a_gpio);
	else
		value = 1;
#else
    if(get_hw_rev() >= 0x08) 
    	value = gpio_get_value_cansleep(gp2a_gpio);
    else
    	value = !gpio_get_value_cansleep(gp2a_gpio);
#endif

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

char proximity_sensor_detection = 0;

static void gp2a_work_func_prox(struct work_struct *work)
{
	struct gp2a_data *gp2a = container_of((struct work_struct *)work,
							struct gp2a_data, work);
	
	unsigned char value;
	char result;
	int ret;

	result = get_ps_vout_value(gp2a->gp2a_gpio); // 0 : proximity, 1 : away
	proximity_sensor_detection = !result;

	input_report_abs(gp2a->input_dev, ABS_DISTANCE,  result);
	input_sync(gp2a->input_dev);

    disable_irq(alt_int);

	value = 0x0C;
	ret = opt_i2c_write(COMMAND1, &value); //Software reset

	if(result == 0) { // detection = Falling Edge
		if(lightsensor_mode == 0) // Low mode
			value = 0x23;
		else // High mode
			value = 0x27;
		ret = opt_i2c_write(COMMAND2, &value);
	}
	else { // none Detection
		if(lightsensor_mode == 0) // Low mode
			value = 0x63;
		else // High mode
			value = 0x67;
		ret = opt_i2c_write(COMMAND2, &value);
	}

	enable_irq(alt_int);

	value = 0xCC;
	ret = opt_i2c_write(COMMAND1, &value);

	gp2a->prox_data= result;
	gprintk("proximity = %d[0=close, 1=far], lightsensor_mode=%d\n",result, lightsensor_mode); //Temp
}

irqreturn_t gp2a_irq_handler(int irq, void *dev_id)
{
	wake_lock_timeout(&prx_wake_lock, 3*HZ);

    gprintk("\n");

	schedule_work(&prox_data->work);

	printk("[PROXIMITY] IRQ_HANDLED.\n");
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


int opt_i2c_read(u8 reg, unsigned char *rbuf, int len )
{

	int ret=-1;
	// int i;
	struct i2c_msg msg;
	uint8_t start_reg;

	msg.addr = opt_i2c_client->addr;
	msg.flags = 0;//I2C_M_WR;
	msg.len = 1;
	msg.buf = &start_reg;
	start_reg = reg;
	ret = i2c_transfer(opt_i2c_client->adapter, &msg, 1);

	if(ret>=0) {
		msg.flags = I2C_M_RD;
		msg.len = len;
		msg.buf = rbuf;
		ret = i2c_transfer(opt_i2c_client->adapter, &msg, 1 );
	}

	if( ret < 0 )
	{
    	printk("%s %d i2c transfer error ret=%d\n", __func__, __LINE__, ret);

	}

  /*
	for(i=0;i<len;i++)
	{
        printk("%s : 0x%x, 0x%x\n", __func__,reg++,rbuf[i]);
	}
  */
    return ret;
}

int opt_i2c_write( u8 reg, u8 *val )
{
    int err = 0;
    struct i2c_msg msg[1];
    unsigned char data[2];
    int retry = 3;

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
    printk("%s %d i2c transfer error(%d)\n", __func__, __LINE__, err);
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
	// int ret;
    int err = 0;
	int wakeup = 0;

	printk("%s : probe start!\n", __func__);
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
#if defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K)
		if(get_hw_rev() >= 0x08) {
			if(pdata->gp2a_irq_rev08)
				gp2a->gp2a_irq = pdata->gp2a_irq_rev08;
			if(pdata->gp2a_gpio_rev08)
				gp2a->gp2a_gpio = pdata->gp2a_gpio_rev08;
		} else {
			if(pdata->gp2a_irq)
				gp2a->gp2a_irq = pdata->gp2a_irq;
			if(pdata->gp2a_gpio)
				gp2a->gp2a_gpio = pdata->gp2a_gpio;
		}
#else
		if(pdata->gp2a_irq)
			gp2a->gp2a_irq = pdata->gp2a_irq;
		if(pdata->gp2a_gpio)
			gp2a->gp2a_gpio = pdata->gp2a_gpio;
#endif
	}

	if(gp2a->power_on)
		gp2a->power_on();

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
	
	/* init i2c */
	opt_i2c_init();

	if(opt_i2c_client == NULL)
	{
		pr_err("opt_probe failed : i2c_client is NULL\n"); 
		return -ENODEV;
	}
	else
		printk("opt_i2c_client : (0x%p)\n",opt_i2c_client);
	

	/* GP2A Regs INIT SETTINGS  and Check I2C communication*/

//	value = 0x01;
	
//	ret = opt_i2c_write((u8)(COMMAND4),&value); //Software reset
//	if(ret == 0) printk("gp2a_opt_probe is OK!!\n");

	value = 0x00;
    opt_i2c_write((u8)(COMMAND1),&value); //shutdown mode op[3]=0
	
	
	alt_int = gp2a->gp2a_irq;
	
	/* INT Settings */	
  	err = request_threaded_irq( alt_int , 
		NULL, gp2a_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING	, "proximity_int", gp2a);

	if (err < 0) {
		printk(KERN_ERR "failed to request proximity_irq\n");
		goto error_setup_reg;
	}
    disable_irq(alt_int);
	
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
	
	printk("%s : probe success!\n", __func__);

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
	kfree(gp2a);
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

#define ALS_SDA 138
#define ALS_SCL 139

static int gp2a_opt_suspend( struct platform_device* pdev, pm_message_t state )
{
	struct gp2a_data *gp2a = platform_get_drvdata(pdev);
	
	gprintk("\n");

	if(gp2a->enabled) // calling
	{

      if (device_may_wakeup(&pdev->dev))
        enable_irq_wake(alt_int);

	   gprintk("The timer is cancled.\n");
	}
#if defined(CONFIG_KOR_MODEL_SHV_E120L) ||defined(CONFIG_KOR_MODEL_SHV_E120S) ||defined(CONFIG_KOR_MODEL_SHV_E120K)
	else {
		gpio_direction_input(ALS_SDA);
		gpio_direction_input(ALS_SCL);
		gpio_free(ALS_SDA);
		gpio_free(ALS_SCL);
	}
#endif

	if(gp2a->power_off)
		gp2a->power_off();

	return 0;
}

static int gp2a_opt_resume( struct platform_device* pdev )
{
	int ret=0;
	struct gp2a_data *gp2a = platform_get_drvdata(pdev);

	gprintk("\n");
#if defined(CONFIG_KOR_MODEL_SHV_E120L) ||defined(CONFIG_KOR_MODEL_SHV_E120S) ||defined(CONFIG_KOR_MODEL_SHV_E120K)
	if(!gp2a->enabled) { //calling
		ret = gpio_request(ALS_SDA, "gp2a_sda");
		if(ret)
			pr_err("%s, %d : err=%d gpio_request fail. \n", __func__, __LINE__, ret);
		ret = gpio_request(ALS_SCL, "gp2a_scl");
		if(ret)
			pr_err("%s, %d : err=%d gpio_request fail. \n", __func__, __LINE__, ret);
	}
#endif

	if(gp2a->power_on)
		gp2a->power_on();

	if(gp2a->enabled) //calling
	{
      if (device_may_wakeup(&pdev->dev))
			enable_irq_wake(alt_int);

      gprintk("The timer is cancled.\n");
	}

    return 0;
}


static int proximity_onoff(u8 onoff)
{
	u8 value;
    int i;
	// unsigned char get_data[1];//test
	int err = 0;

	printk("%s : proximity turn on/off = %d\n", __func__, onoff);
       
	if(onoff)
	{// already on light sensor, so must simultaneously  turn on light sensor and proximity sensor 

//	    opt_i2c_read(COMMAND1, get_data, sizeof(get_data));
//	    if(get_data == 0xC1) return 0;
       	for(i=0;i<COL;i++)
       	{
       		err = opt_i2c_write(gp2a_original_image[i][0],&gp2a_original_image[i][1]);
			if(err < 0)
				printk("%s : turnning on error i = %d, err=%d\n", __func__, i, err);
			lightsensor_mode = 0;
    	}
	}
	else
	{// light sensor turn on and proximity turn off
	
//	    opt_i2c_read(COMMAND1, get_data, sizeof(get_data));
//	    if(get_data == 0xD1) return 0;
	
		if(lightsensor_mode)
			value = 0x67;//resolution :16bit, range: *128
		else
			value = 0x63;//resolution :16bit, range: *128
       	opt_i2c_write(COMMAND2,&value);
		value = 0xD0;//OP3 : 1(operating mode) OP2 :1(coutinuous operating mode) OP1 : 01(ALS mode)
       	opt_i2c_write(COMMAND1,&value);
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
MODULE_DESCRIPTION("Optical Sensor driver for GP2AP020A00F");
MODULE_LICENSE("GPL");
