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


#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/i2c/gp2ap020.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
//#include <linux/m_adc.h>
#include <linux/msm_adc.h>

#include <mach/vreg.h>
#include <linux/workqueue.h>


/* for debugging */
#define DEBUG 0

/*********** for debug **********************************************************/
#if 1 
#define gprintk(fmt, x... ) printk( "%s(%d): " fmt, __FUNCTION__ ,__LINE__, ## x)
#else
#define gprintk(x...) do { } while (0)
#endif
/*******************************************************************************/


#define SENSOR_NAME "light_sensor"

#define SENSOR_DEFAULT_DELAY            (200)   /* 200 ms */
#define SENSOR_MAX_DELAY                (2000)  /* 2000 ms */
#define ABS_STATUS                      (ABS_BRAKE)
#define ABS_WAKE                        (ABS_MISC)
#define ABS_CONTROL_REPORT              (ABS_THROTTLE)

#define LIGHT_BUFFER_NUM	5

struct sensor_data {
	struct mutex mutex;
	struct delayed_work work;
	struct class *lightsensor_class;
	struct device *switch_cmd_dev;

	int enabled;
	int delay;

	state_type light_data;
#if DEBUG
	int suspend;
#endif
	int testmode;
	int light_buffer;
	int light_count;
};

static const int adc_table[4] = {
	15, //15 lux match adc value
	140, //150 lux match adc value
	1490, //1500 lux match adc value
	15000, //15000 lux match adc value
};

struct workqueue_struct *light_workqueue;

/* global var */
static struct platform_device *sensor_pdev = NULL;
static struct input_dev *this_data = NULL;

static state_type cur_state = LIGHT_INIT;
static int cur_adc_value = 0;

static int adc_value_buf[ADC_BUFFER_NUM] = {0};

int autobrightness_mode = OFF;
int LightSensor_Log_Cnt =0; 
extern int proximity_enable;

u8 lightsensor_mode = 0; // 0 = low, 1 = high

static int StateToLux(state_type state);
int lightsensor_get_adc(void);
static int lightsensor_onoff(u8 onoff);

extern unsigned int get_hw_rev(void);

//extern int backlight_level;
#ifdef CONFIG_FB_S3C_MDNIE_TUNINGMODE_FOR_BACKLIGHT
static int pre_val = -1;
extern int current_gamma_value;
extern u16 *pmDNIe_Gamma_set[];

typedef enum
{
	mDNIe_UI_MODE,
	mDNIe_VIDEO_MODE,
	mDNIe_VIDEO_WARM_MODE,
	mDNIe_VIDEO_COLD_MODE,
	mDNIe_CAMERA_MODE,
	mDNIe_NAVI
}Lcd_mDNIe_UI;

extern Lcd_mDNIe_UI current_mDNIe_UI;

extern void mDNIe_Mode_set_for_backlight(u16 *buf);

int value_buf[4] = {0};


int IsChangedADC(int val)
{
	int j = 0;
	int ret = 0;

	int adc_index = 0;
	static int adc_index_count = 0;

	adc_index = (adc_index_count)%4;
	adc_index_count++;

	if(pre_val == -1) { //ADC buffer initialize
		for(j = 0; j<4; j++)
			value_buf[j] = val;

		pre_val = 0;
	} else {
		value_buf[adc_index] = val;
	}

	ret = ((value_buf[0] == value_buf[1])&& \
		(value_buf[1] == value_buf[2])&& \
		(value_buf[2] == value_buf[3]))? 1 : 0;

	if(adc_index_count == 4)
		adc_index_count = 0;

	return ret;
}
#endif

static int StateToLux(state_type state)
{
	int lux = 0;

	if(state== LIGHT_LEVEL5) {
		lux = 15000;
	} else if(state == LIGHT_LEVEL4) {
		lux = 9000;
	} else if(state == LIGHT_LEVEL3) {
		lux = 5000;
	} else if(state == LIGHT_LEVEL2) {
		lux = 1000;
	} else if(state == LIGHT_LEVEL1) {
		lux = 6;
	} else {
		lux = 5000;
	}

	return lux;
}

static ssize_t lightsensor_file_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
//	struct input_dev *input_data = to_input_dev(dev);
//	struct sensor_data *data = input_get_drvdata(input_data);
	int adc = 0;

//	if (!(data->power_state & LIGHT_ENABLED))
//		cm3663_light_enable(data);

	adc = lightsensor_get_adcvalue();

//	if (!(data->power_state & LIGHT_ENABLED))
//		cm3663_light_disable(data);

	return sprintf(buf, "%d\n", adc);
}

/* Light Sysfs interface */
static ssize_t
light_delay_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);
	int delay;

	delay = data->delay;

	return sprintf(buf, "%d\n", delay);
}

static ssize_t
light_delay_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);
	#if 0
	int delay = simple_strtoul(buf, NULL, 10);

	if (delay < 0) {
		return count;
	}


	#else
	int delay;
	int err = 0;

	err = kstrtoint(buf, 10, &delay);
	if (err)
		pr_err("%s, kstrtoint failed.", __func__);
		
	if (delay < 0) {
		return count;
	}

	delay = delay / 1000000;	//ns to msec 

	#endif
	
	pr_info("%s, new_delay = %d, old_delay = %d", __func__, delay,
	       data->delay);
	       
	if (SENSOR_MAX_DELAY < delay) {
		delay = SENSOR_MAX_DELAY;
	}

	data->delay = delay;

	mutex_lock(&data->mutex);

	if( data->enabled)
	{
		cancel_delayed_work_sync(&data->work);
		queue_delayed_work(light_workqueue,&data->work,msecs_to_jiffies(delay));
	}

	input_report_abs(input_data, ABS_CONTROL_REPORT, (data->delay<<16) | delay);

	mutex_unlock(&data->mutex);

	return count;
}

static ssize_t
light_enable_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);
	int enabled;

	enabled = data->enabled;

	return sprintf(buf, "%d\n", enabled);
}

static ssize_t
light_enable_store(struct device *dev,
        struct device_attribute *attr,
        const char *buf,
        size_t count)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);
	int value = simple_strtoul(buf, NULL, 10);

	if (value != 0 && value != 1) {
		return count;
	}

	mutex_lock(&data->mutex);

	if (data->enabled && !value) {
		lightsensor_onoff(0);
		data->enabled= value; // sync with gp2a_work_func_light function
		cancel_delayed_work_sync(&data->work);
		gprintk("timer canceled.\n");
	}
	if (!data->enabled && value) {
		lightsensor_onoff(1);
		data->enabled= value; // sync with gp2a_work_func_light function
		queue_delayed_work(light_workqueue,&data->work,0);
		gprintk("timer started.\n");
	}

	input_report_abs(input_data, ABS_CONTROL_REPORT, (value<<16) | data->delay);

	mutex_unlock(&data->mutex);

	return count;
}

static ssize_t
light_wake_store(struct device *dev,
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
light_data_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);
	//unsigned long flags;
	int light_lux;

	mutex_lock(&data->mutex);
	light_lux = StateToLux(data->light_data);
	mutex_unlock(&data->mutex);	

	return sprintf(buf, "%d\n", light_lux);
}

static ssize_t
light_status_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	//struct input_dev *input_data = to_input_dev(dev);
	//struct sensor_data *data = input_get_drvdata(input_data);

	printk(KERN_INFO "%s : cur_state(%d)\n", __func__, cur_state);

	return sprintf(buf, "%d\n", cur_state);
}

static ssize_t light_autobrightness_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);

	int adc = 0;
	int sum = 0;
	int i = 0;

	gprintk("called %s \n", __func__);

	if(data->enabled) {
#if defined(CONFIG_KOR_MODEL_SHV_E120L)
		if(get_hw_rev() >= 0x01) { // HW_REV00 does not work gp2a sensor 
			for(i = 0; i < 10; i++) {
				adc = lightsensor_get_adcvalue();
				msleep(20);
				sum += adc;
			}
			adc = sum/10;
		}
		else
			adc = 700;
#else
		for(i = 0; i < 10; i++) {
			adc = lightsensor_get_adcvalue();
			msleep(20);
			sum += adc;
		}
		adc = sum/10;
#endif

		gprintk("called %s  - subdued alarm(adc : %d)\n", __func__, adc);
		return sprintf(buf,"%d\n", adc);
	} else {
		gprintk("called %s  - *#0589#\n", __func__);
		return sprintf(buf,"%d\n", cur_adc_value);
	}
}

static ssize_t light_autobrightness_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
	int value;

	sscanf(buf, "%d", &value);

	gprintk("called %s \n", __func__);

	if(value == 1) {
		autobrightness_mode = ON;
		printk(KERN_DEBUG "[brightness_mode] BRIGHTNESS_MODE_SENSOR\n");
	} else if(value == 0) {
		autobrightness_mode = OFF;
		printk(KERN_DEBUG "[brightness_mode] BRIGHTNESS_MODE_USER\n");
#ifdef MDNIE_TUNINGMODE_FOR_BACKLIGHT
		if(pre_val == 1) {
			mDNIe_Mode_set_for_backlight(pmDNIe_Gamma_set[2]);
		}
		pre_val = -1;
#endif
	}

	return size;
}

static ssize_t light_testmode_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t size)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);
	int value;

	sscanf(buf, "%d", &value);

	if(value == 1) {
		data->testmode = 1;
		gprintk("lightsensor testmode ON.\n");
	} else if(value == 0) {
		data->testmode  = 0;
		gprintk("lightsensor testmode OFF.\n");
	}

	return size;
}

static ssize_t light_testmode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct sensor_data *data = input_get_drvdata(input_data);

	int testmode;

	testmode = data->testmode;

  	gprintk(" : %d \n", testmode);

	return sprintf(buf, "%d\n", testmode);
}


static DEVICE_ATTR(poll_delay, S_IRUGO|S_IWUSR|S_IWGRP, light_delay_show, light_delay_store);
static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP, light_enable_show, light_enable_store);
static DEVICE_ATTR(wake, S_IWUSR|S_IWGRP, NULL, light_wake_store);
static DEVICE_ATTR(data, S_IRUGO, light_data_show, NULL);
static DEVICE_ATTR(status, S_IRUGO, light_status_show, NULL);
static DEVICE_ATTR(autobrightness, S_IRUGO|S_IWUSR|S_IWGRP, light_autobrightness_show, light_autobrightness_store);
static DEVICE_ATTR(testmode, S_IRUGO|S_IWUSR|S_IWGRP, light_testmode_show, light_testmode_store);
static DEVICE_ATTR(lightsensor_file_state, 0644, lightsensor_file_state_show, NULL);


static struct attribute *lightsensor_attributes[] = {
	&dev_attr_poll_delay.attr,
	&dev_attr_enable.attr,
	&dev_attr_wake.attr,
	&dev_attr_data.attr,
	&dev_attr_status.attr,
	&dev_attr_autobrightness.attr,
	&dev_attr_testmode.attr,
	&dev_attr_lightsensor_file_state.attr,
	NULL
};

static struct attribute_group lightsensor_attribute_group = {
	.attrs = lightsensor_attributes
};

static int
lightsensor_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct sensor_data *data = input_get_drvdata(this_data);


	int rt = 0;

	mutex_lock(&data->mutex);

	if (data->enabled) {
		rt = cancel_delayed_work_sync(&data->work);
		gprintk(": The timer is cancled.\n");
	}

	mutex_unlock(&data->mutex);

	return rt;
}

static int
lightsensor_resume(struct platform_device *pdev)
{
	struct sensor_data *data = input_get_drvdata(this_data);
	int rt = 0;

	data->light_count = 0;
	data->light_buffer = 0;

	mutex_lock(&data->mutex);

	if (data->enabled) {
		rt = queue_delayed_work(light_workqueue,&data->work,0);
		gprintk(": The timer is started.\n");
	}

	mutex_unlock(&data->mutex);

	return rt;
}

extern char proximity_sensor_detection;

int lightsensor_get_adc(void)
{
	unsigned char get_data[4]={0,};//test
	int D0_raw_data;
	int D1_raw_data;
	int D0_data;
	int D1_data;
	int lx =0;
	u8 value;
	int light_alpha;
	int light_beta;
	static int lx_prev=0;
	int ret=0;
	int d0_boundary = 93;

#if defined(CONFIG_KOR_MODEL_SHV_E120K)
	d0_boundary = 93;
#elif defined(CONFIG_KOR_MODEL_SHV_E120S)
	if(get_hw_rev() >= 0x09)
		d0_boundary = 93;
	else
		d0_boundary = 90;		
#else // defined(CONFIG_KOR_MODEL_SHV_E120L)
	if(get_hw_rev() >= 0x03)
		d0_boundary = 93;
	else
		d0_boundary = 90;
#endif
	ret = opt_i2c_read(0x0C, get_data, sizeof(get_data));
	if(ret < 0)
		return lx_prev;
	D0_raw_data =(get_data[1] << 8) | get_data[0]; // clear
	D1_raw_data =(get_data[3] << 8) | get_data[2]; // IR

#if defined(CONFIG_KOR_MODEL_SHV_E120K)
	if(lightsensor_mode) { // HIGH_MODE
		if(100 * D1_raw_data <= 32 * D0_raw_data) {
			light_alpha = 800;
			light_beta = 0;
		}
		else if(100 * D1_raw_data <= 67 * D0_raw_data) {
			light_alpha = 2015;
			light_beta = 2925;
		} else if (100 * D1_raw_data <= d0_boundary * D0_raw_data) {
			light_alpha = 56;
			light_beta = 12;
		} else {
			light_alpha = 0;
			light_beta = 0;
		}
	} else { // LOW_MODE
		if(100 * D1_raw_data <= 32 * D0_raw_data) {
			light_alpha = 800;
			light_beta = 0;
		}
		else if(100 * D1_raw_data <= 67 * D0_raw_data) {
			light_alpha = 2015;
			light_beta = 2925;
		} else if (100 * D1_raw_data <= d0_boundary * D0_raw_data) {
			light_alpha = 547;
			light_beta = 599;
		} else {
			light_alpha = 0;
			light_beta = 0;
		}
	}
#elif defined(CONFIG_KOR_MODEL_SHV_E120S)
	if(get_hw_rev() >= 0x09) {
		if(lightsensor_mode) { // HIGH_MODE
			if(100 * D1_raw_data <= 32 * D0_raw_data) {
				light_alpha = 800;
				light_beta = 0;
			}
			else if(100 * D1_raw_data <= 67 * D0_raw_data) {
				light_alpha = 2015;
				light_beta = 2925;
			} else if (100 * D1_raw_data <= d0_boundary * D0_raw_data) {
				light_alpha = 56;
				light_beta = 12;
			} else {
				light_alpha = 0;
				light_beta = 0;
			}
		} else { // LOW_MODE
			if(100 * D1_raw_data <= 32 * D0_raw_data) {
				light_alpha = 800;
				light_beta = 0;
			}
			else if(100 * D1_raw_data <= 67 * D0_raw_data) {
				light_alpha = 2015;
				light_beta = 2925;
			} else if (100 * D1_raw_data <= d0_boundary * D0_raw_data) {
				light_alpha = 547;
				light_beta = 599;
			} else {
				light_alpha = 0;
				light_beta = 0;
			}
		}
	} else { // (get_hw_rev() >= 0x09)
		if(lightsensor_mode) { // HIGH_MODE
			if(100 * D1_raw_data <= 67 * D0_raw_data) {
				light_alpha = 2015;
				light_beta = 2925;
			} else if (100 * D1_raw_data <= d0_boundary * D0_raw_data) {
				light_alpha = 56;
				light_beta = 12;
			} else {
				light_alpha = 0;
				light_beta = 0;
			}
		} else { // LOW_MODE
			if(100 * D1_raw_data <= 60 * D0_raw_data) {
				light_alpha = 2015;
				light_beta = 2925;
			} else if (100 * D1_raw_data <= d0_boundary * D0_raw_data) {
				light_alpha = 800;
				light_beta = 876;
			} else {
				light_alpha = 0;
				light_beta = 0;
			}
		}
	}// (get_hw_rev() >= 0x09)
#else // defined(CONFIG_KOR_MODEL_SHV_E120L)
	if(lightsensor_mode) { // HIGH_MODE
		if(100 * D1_raw_data <= 67 * D0_raw_data) {
			light_alpha = 2015;
			light_beta = 2925;
		} else if (100 * D1_raw_data <= d0_boundary * D0_raw_data) {
			light_alpha = 56;
			light_beta = 12;
		} else {
			light_alpha = 0;
			light_beta = 0;
		}
	} else { // LOW_MODE
		if(100 * D1_raw_data <= 60 * D0_raw_data) {
			light_alpha = 2015;
			light_beta = 2925;
		} else if (100 * D1_raw_data <= d0_boundary * D0_raw_data) {
			light_alpha = 800;
			light_beta = 876;
		} else {
			light_alpha = 0;
			light_beta = 0;
		}
	}
#endif

	if(lightsensor_mode) { // HIGH_MODE
		D0_data = D0_raw_data * 16;
		D1_data = D1_raw_data * 16;
	} else { // LOW_MODE
		D0_data = D0_raw_data;
		D1_data = D1_raw_data;
	}

	if((D0_data == 0 || D1_data == 0) && (D0_data < 300 && D1_data < 300)) {
		lx = 0;
//		printk("%s: lx is 0 : D0=%d, D1=%d\n", __func__, D0_raw_data, D1_raw_data);
	}
	else if(lightsensor_mode==0 && (D0_raw_data >= 16000 || D1_raw_data >= 16000) && (D0_raw_data <= 16383 && D1_raw_data <= 16383)) {
		printk("%s: need to changed HIGH_MODE D0=%d, D1=%d\n", __func__, D0_raw_data, D1_raw_data);
		lx = lx_prev;
	}
	else if((100 * D1_data > d0_boundary * D0_data) || (100 * D1_data < 15 * D0_data)) {
		lx = lx_prev;
//		printk("%s: Data range over so ues prev_lx value=%d D0=%d, D1=%d mode=%d\n",
//			__func__, lx, D0_data, D1_data, lightsensor_mode);
		return lx;
	}
	else {
		lx = (int)((light_alpha / 10 * D0_data * 33) - (light_beta / 10 * D1_data * 33)) / 1000;
//		printk("%s: D0=%d, D1=%d, lx=%d mode=%d a=%d, b=%d prev_lx=%d\n",__func__,
//			D0_raw_data, D1_raw_data, lx, lightsensor_mode, light_alpha, light_beta, lx_prev);
	}

	lx_prev = lx;

	if(lightsensor_mode) { // HIGH MODE
		if(D0_raw_data < 1000) {
			printk("%s: change to LOW_MODE detection=%d\n", __func__, proximity_sensor_detection);
			lightsensor_mode = 0; // change to LOW MODE

			value = 0x0C;
			opt_i2c_write(COMMAND1,&value);

			if(proximity_sensor_detection)
				value = 0x23;
			else
				value = 0x63;
			opt_i2c_write(COMMAND2,&value);

			if(proximity_enable)
				value = 0xCC;
			else
				value = 0xDC;
			opt_i2c_write(COMMAND1,&value);
		}
	} else { // LOW MODE
		if(D0_raw_data > 16000 || D1_raw_data > 16000) {
			printk("%s: change to HIGH_MODE detection=%d\n", __func__, proximity_sensor_detection);
			lightsensor_mode = 1; // change to HIGH MODE

			value = 0x0C;
			opt_i2c_write(COMMAND1,&value);

			if(proximity_sensor_detection)
				value = 0x27;
			else
				value = 0x67;
			opt_i2c_write(COMMAND2,&value);

			if(proximity_enable)
				value = 0xCC;
			else
				value = 0xDC;
			opt_i2c_write(COMMAND1,&value);
		}
	}

	return lx;
}


int lightsensor_get_adcvalue(void)
{
	int i = 0;
	int j = 0;
	unsigned int adc_total = 0;
	static int adc_avr_value = 0;
	unsigned int adc_index = 0;
	static unsigned int adc_index_count = 0;
	unsigned int adc_max = 0;
	unsigned int adc_min = 0;
	int value =0;

	value = lightsensor_get_adc();


	cur_adc_value = value;

	adc_index = (adc_index_count++)%ADC_BUFFER_NUM;		

	if(cur_state == LIGHT_INIT) { //ADC buffer initialize (light sensor off ---> light sensor on)
		for(j = 0; j < ADC_BUFFER_NUM; j++)
			adc_value_buf[j] = value;
	} else {
		adc_value_buf[adc_index] = value;
	}

	adc_max = adc_value_buf[0];
	adc_min = adc_value_buf[0];

	for(i = 0; i < ADC_BUFFER_NUM; i++) {
		adc_total += adc_value_buf[i];

		if(adc_max < adc_value_buf[i])
			adc_max = adc_value_buf[i];
					
		if(adc_min > adc_value_buf[i])
			adc_min = adc_value_buf[i];
	}
	adc_avr_value = (adc_total-(adc_max+adc_min))/(ADC_BUFFER_NUM-2);
	
	if(adc_index_count == ADC_BUFFER_NUM-1)
		adc_index_count = 0;

	return adc_avr_value;
}

static int lightsensor_onoff(u8 onoff)
{
	u8 value;
        // int i;

	printk("%s : light_sensor onoff = %d\n", __func__, onoff);

	if(onoff)
	{
       if(proximity_enable == 0){//in calling, must turn on proximity sensor
		value = 0x01;
		opt_i2c_write(COMMAND4,&value);

		value = 0x63;
       	opt_i2c_write(COMMAND2,&value);
		value = 0xD0;//OP3 : 1(operating mode) OP2 :1(coutinuous operating mode) OP1 : 01(ALS mode) TYPE=0(auto)
       	opt_i2c_write(COMMAND1,&value);
	// other setting have defualt value. 
    	}
	}
	else
	{
      if(proximity_enable == 0){//in calling, must turn on proximity sensor
		value = 0x00; //shutdown mode
		opt_i2c_write((u8)(COMMAND1),&value);
	}
	}
	
	return 0;
}


static void gp2a_work_func_light(struct work_struct *work)
{
	struct sensor_data *data = container_of((struct delayed_work *)work,
							struct sensor_data, work);

	int i;
	int adc = 0;

#if defined(CONFIG_KOR_MODEL_SHV_E120L)
	if(get_hw_rev() >= 0x01) // HW_REV00 does not work gp2a sensor 
		adc = lightsensor_get_adcvalue();
	else
		adc = 700;		
#else
	adc = lightsensor_get_adcvalue();
#endif

	for (i = 0; ARRAY_SIZE(adc_table); i++)
		if (adc <= adc_table[i])
			break;

	if (data->light_buffer == i) {
		if (data->light_count++ == LIGHT_BUFFER_NUM) {
				input_report_abs(this_data,	ABS_MISC, adc);
				input_sync(this_data);
				data->light_count = 0;
			if(LightSensor_Log_Cnt++ == 10) {
				gprintk("realmode : adc(%d)\n", adc);
                    LightSensor_Log_Cnt = 0; 
                }
		}
	} else {
		data->light_buffer = i;
		data->light_count = 0;
    }

	if(data->enabled)
		queue_delayed_work(light_workqueue,&data->work, msecs_to_jiffies(data->delay));
}


static int
lightsensor_probe(struct platform_device *pdev)
{
	struct sensor_data *data = NULL;
	struct input_dev *input_data = NULL;
	int input_registered = 0, sysfs_created = 0;
	int rt;

	printk("%s : probe start!\n", __func__);

	data = kzalloc(sizeof(struct sensor_data), GFP_KERNEL);
	if (!data) {
		rt = -ENOMEM;
		goto err;
	}
	data->enabled = 0;
	data->delay = SENSOR_DEFAULT_DELAY;
	data->testmode = 0;

	light_workqueue = create_singlethread_workqueue("klightd");
	if (!light_workqueue) {
		rt = -ENOMEM;
		printk(KERN_ERR "%s: Failed to allocate work queue\n", __func__);
		goto err;
	}

	INIT_DELAYED_WORK(&data->work, gp2a_work_func_light);

	input_data = input_allocate_device();
	if (!input_data) {
		rt = -ENOMEM;
		printk(KERN_ERR
			"sensor_probe: Failed to allocate input_data device\n");
		goto err;
	}

	set_bit(EV_ABS, input_data->evbit);
	input_set_capability(input_data, EV_ABS, ABS_X);
	input_set_capability(input_data, EV_ABS, ABS_MISC); 
	input_set_capability(input_data, EV_ABS, ABS_WAKE); /* wake */
	input_set_capability(input_data, EV_ABS, ABS_CONTROL_REPORT); /* enabled/delay */
	input_set_abs_params(input_data, ABS_MISC, 0, 1, 0, 0);
	input_set_abs_params(input_data, ABS_WAKE, 0, 163840, 0, 0);
	input_set_abs_params(input_data, ABS_CONTROL_REPORT, 0, 98432, 0, 0);
	input_data->name = SENSOR_NAME;

	rt = input_register_device(input_data);
	if (rt) {
		printk(KERN_ERR
			"sensor_probe: Unable to register input_data device: %s\n",
			input_data->name);
		goto err;
	}
	input_set_drvdata(input_data, data);
	input_registered = 1;

	rt = sysfs_create_group(&input_data->dev.kobj,
				&lightsensor_attribute_group);
	if (rt) {
		printk(KERN_ERR
			"sensor_probe: sysfs_create_group failed[%s]\n",
			input_data->name);
		goto err;
	}
	sysfs_created = 1;
	mutex_init(&data->mutex);
	this_data = input_data;

	data->lightsensor_class = class_create(THIS_MODULE, "lightsensor");
	if (IS_ERR(data->lightsensor_class)) {
		pr_err("%s: could not create lightsensor_class\n", __func__);
		goto err;
	}

	data->switch_cmd_dev = device_create(data->lightsensor_class,
						NULL, 0, NULL, "switch_cmd");
	if (IS_ERR(data->switch_cmd_dev)) {
		pr_err("%s: could not create switch_cmd_dev\n", __func__);
		goto err_light_device_create;
	}

	if (device_create_file(data->switch_cmd_dev,
		&dev_attr_lightsensor_file_state) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
			dev_attr_lightsensor_file_state.attr.name);
		goto err_light_device_create_file;
	}
	dev_set_drvdata(data->switch_cmd_dev, data);
	
	printk("%s : probe success!\n", __func__);

	return 0;

err_light_device_create_file:
	device_destroy(data->lightsensor_class, 0);
err_light_device_create:
	class_destroy(data->lightsensor_class);
err:
	if (data != NULL) {
		if (input_data != NULL) {
			if (sysfs_created) {
				sysfs_remove_group(&input_data->dev.kobj,
						&lightsensor_attribute_group);
			}
			if (input_registered)
				input_unregister_device(input_data);
			else
				input_free_device(input_data);
			input_data = NULL;
		}
		kfree(data);
	}

	return rt;
}

static int
lightsensor_remove(struct platform_device *pdev)
{
	struct sensor_data *data;
	int rt;

	rt = 0;
	if (this_data != NULL) {
		data = input_get_drvdata(this_data);
		data->enabled = 0;
		sysfs_remove_group(&this_data->dev.kobj,
				&lightsensor_attribute_group);
		if (data != NULL) {
			cancel_delayed_work(&data->work);
			flush_workqueue(light_workqueue);
			destroy_workqueue(light_workqueue);
			kfree(data);
		}
		input_unregister_device(this_data);
	}

	//gprintk("\n");

	return rt;
}

static void
lightsensor_shutdown(struct platform_device *pdev)
{
	struct sensor_data *data;
	int rt;

	rt = 0;
	if (this_data != NULL) {
		data = input_get_drvdata(this_data);
		data->enabled = 0;
		sysfs_remove_group(&this_data->dev.kobj,
				&lightsensor_attribute_group);
		if (data != NULL) {
			cancel_delayed_work(&data->work);
			flush_workqueue(light_workqueue);
			kfree(data);
		}
		input_unregister_device(this_data);
	}

	//gprintk("\n");
}


/*
 * Module init and exit
 */
static struct platform_driver lightsensor_driver = {
	.probe      = lightsensor_probe,
	.remove     = lightsensor_remove,
	.suspend    = lightsensor_suspend,
	.resume     = lightsensor_resume,
	.shutdown   = lightsensor_shutdown,
	.driver = {
		.name   = SENSOR_NAME,
		.owner  = THIS_MODULE,
	},
};

#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

static int __init lightsensor_init(void)
{
#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return 0\n", __func__);
		return 0;
	}
#endif

	sensor_pdev = platform_device_register_simple(SENSOR_NAME, 0, NULL, 0);
	if (IS_ERR(sensor_pdev)) {
		return -1;
	}
	return platform_driver_register(&lightsensor_driver);
}
module_init(lightsensor_init);

static void __exit lightsensor_exit(void)
{
	platform_driver_unregister(&lightsensor_driver);
	platform_device_unregister(sensor_pdev);
}
module_exit(lightsensor_exit);

MODULE_AUTHOR("SAMSUNG");
MODULE_DESCRIPTION("Optical Sensor driver for GP2AP020A00F");
MODULE_LICENSE("GPL");
