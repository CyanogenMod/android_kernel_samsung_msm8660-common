/* 
 *  Title : Optical Sensor(proximity sensor) driver for TAOSP002S00F   
 *  Date  : 2009.02.27
 *  Name  : ms17.kim
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <asm/uaccess.h>
#include <linux/unistd.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <mach/gpio.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/wakelock.h>
#include <linux/mfd/pmic8058.h>
#include <linux/i2c/taos.h>
#include <linux/errno.h>
#include <linux/mutex.h>

/*********** for debug **********************************************************/
#define gprintk(fmt, x... ) printk( "%s(%d): " fmt, __FUNCTION__ ,__LINE__, ## x)
/*******************************************************************************/

#define PMIC8058_IRQ_BASE				(NR_MSM_IRQS + NR_GPIO_IRQS)
#define TAOS_INT PM8058_GPIO_IRQ(PMIC8058_IRQ_BASE, (PM8058_GPIO(14)))  
#define TAOS_INT_GPIO PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(14))


#define TAOS_DEBUG 1
#define IRQ_WAKE 1

/* Triton register offsets */
#define CNTRL				0x00
#define ALS_TIME			0X01
#define PRX_TIME			0x02
#define WAIT_TIME			0x03
#define ALS_MINTHRESHLO			0X04
#define ALS_MINTHRESHHI			0X05
#define ALS_MAXTHRESHLO			0X06
#define ALS_MAXTHRESHHI			0X07
#define PRX_MINTHRESHLO			0X08
#define PRX_MINTHRESHHI			0X09
#define PRX_MAXTHRESHLO			0X0A
#define PRX_MAXTHRESHHI			0X0B
#define INTERRUPT			0x0C
#define PRX_CFG				0x0D
#define PRX_COUNT			0x0E
#define GAIN				0x0F
#define REVID				0x11
#define CHIPID				0x12
#define STATUS				0x13
#define ALS_CHAN0LO			0x14
#define ALS_CHAN0HI			0x15
#define ALS_CHAN1LO			0x16
#define ALS_CHAN1HI			0x17
#define PRX_LO				0x18
#define PRX_HI				0x19
#define TEST_STATUS			0x1F

/* Triton cmd reg masks */
#define CMD_REG				0X80
#define CMD_BYTE_RW			0x00
#define CMD_WORD_BLK_RW			0x20
#define CMD_SPL_FN			0x60
#define CMD_PROX_INTCLR			0X05
#define CMD_ALS_INTCLR			0X06
#define CMD_PROXALS_INTCLR		0X07
#define CMD_TST_REG			0X08
#define CMD_USER_REG			0X09

/* Triton cntrl reg masks */
#define CNTL_REG_CLEAR			0x00
#define CNTL_PROX_INT_ENBL		0X20
#define CNTL_ALS_INT_ENBL		0X10
#define CNTL_WAIT_TMR_ENBL		0X08
#define CNTL_PROX_DET_ENBL		0X04
#define CNTL_ADC_ENBL			0x02
#define CNTL_PWRON			0x01
#define CNTL_ALSPON_ENBL		0x03
#define CNTL_INTALSPON_ENBL		0x13
#define CNTL_PROXPON_ENBL		0x0F
#define CNTL_INTPROXPON_ENBL		0x2F

/* Triton status reg masks */
#define STA_ADCVALID			0x01
#define STA_PRXVALID			0x02
#define STA_ADC_PRX_VALID		0x03
#define STA_ADCINTR			0x10
#define STA_PRXINTR			0x20

/* Lux constants */
#define SCALE_MILLILUX			3
#define FILTER_DEPTH			3
#define	GAINFACTOR			9

/* Thresholds */
#define ALS_THRESHOLD_LO_LIMIT		0x0000
#define ALS_THRESHOLD_HI_LIMIT		0xFFFF
#define PROX_THRESHOLD_LO_LIMIT		0x0000
#define PROX_THRESHOLD_HI_LIMIT		0xFFFF

/* Device default configuration */
#define CALIB_TGT_PARAM			300000
//#define ALS_TIME_PARAM			100
#define SCALE_FACTOR_PARAM		1
#define GAIN_TRIM_PARAM			512
#define GAIN_PARAM			1
#define ALS_THRSH_HI_PARAM		0xFFFF
#define ALS_THRSH_LO_PARAM		0
#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
#define PRX_THRSH_HI_PARAM		0x26C // 700 -> 650 -> 620
#define PRX_THRSH_LO_PARAM		0x1F4 // 550 -> 520 -> 500
#elif defined(CONFIG_USA_MODEL_SGH_T769)
#define PRX_THRSH_HI_PARAM		0x29E // 700 -> 650 -> 550 -> 670
#define PRX_THRSH_LO_PARAM		0x21C // 550 -> 520 -> 420 -> 540
#elif defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
#define PRX_THRSH_HI_PARAM		580 //580->650->750->580
#define PRX_THRSH_LO_PARAM		430 //480->520->650->480
#else
#define PRX_THRSH_HI_PARAM		0x28A // 700 -> 650
#define PRX_THRSH_LO_PARAM		0x208 // 550 -> 520
#endif
#define ALS_TIME_PARAM			0xF6	// 50ms -> 27ms
#define PRX_ADC_TIME_PARAM		0xFF // [HSS] Original value : 0XEE
#define PRX_WAIT_TIME_PARAM		0xFF // 2.73ms
#define INTR_FILTER_PARAM		0x63	// 33 -> 63
#define PRX_CONFIG_PARAM		0x00
#define PRX_PULSE_CNT_PARAM		0x0A
#define PRX_GAIN_PARAM			0x28	// 21
#define LIGHT_BUFFER_NUM 5

#define SENSOR_DEFAULT_DELAY		(200)
#define SENSOR_MAX_DELAY			(2000)
#define ABS_STATUS                      (ABS_BRAKE)
#define ABS_WAKE                        (ABS_MISC)
#define ABS_CONTROL_REPORT              (ABS_THROTTLE)

/* global var */
struct taos_data *taos;
static struct i2c_client *opt_i2c_client = NULL;
struct device *switch_cmd_dev;
static ktime_t light_polling_time; 
static ktime_t prox_polling_time;	
static bool light_enable = 0;
static bool proximity_enable = OFF;
static short proximity_value = 0;
static struct wake_lock prx_wake_lock;
static short  isTaosSensor = 1;
static int irdata = 0;		//Ch[1]
static int cleardata = 0;	//Ch[0]
static u16 chipID=0;
static int count = 0;

static TAOS_ALS_FOPS_STATUS taos_als_status = TAOS_ALS_CLOSED;
static TAOS_PRX_FOPS_STATUS taos_prx_status = TAOS_PRX_CLOSED;
static TAOS_CHIP_WORKING_STATUS taos_chip_status = TAOS_CHIP_UNKNOWN;
#if defined(CONFIG_USA_MODEL_SGH_T769)
static const int adc_table[4] = {
	15,
	150,
	1350,
	13000,
};
#elif defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
static const int adc_table[4] = {
	15,
	163,
	1650,
	15700,
};
#elif defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
static const int adc_table[4] = {
	15,
	160,
	1600,
	16000,
};
#else
static const int adc_table[4] = {
	15,
	150,
	1450,
	13000,
};
#endif

/*  i2c write routine for taos */
static int opt_i2c_write( u8 reg, u8 *val )
{
	int err;
	struct i2c_msg msg[1];
	unsigned char data[2];
		if( opt_i2c_client == NULL )
		return -ENODEV;
	data[0] = reg;
	data[1] = *val;

    msg->addr = opt_i2c_client->addr;
    msg->flags = I2C_M_WR;
    msg->len = 2;
    msg->buf = data;

    err = i2c_transfer(opt_i2c_client->adapter, msg, 1);

    if (err >= 0) return 0;

    printk("[TAOS] %s %d i2c transfer error : reg = [%X], err is %d\n", __func__, __LINE__, reg, err);
    return err;
}
/*static int opt_i2c_read(u8 reg , u8 *val)
{
	int ret;
	udelay(10);

    if( opt_i2c_client == NULL )
        return -ENODEV;
	i2c_smbus_write_byte(taos->client, (CMD_REG | reg));
	ret = i2c_smbus_read_byte(taos->client);
	*val = ret;

    return ret;
}*/

/*************************************************************************/
/*		TAOS sysfs						*/
/*************************************************************************/

short taos_get_proximity_value(void)
{
	return ((proximity_value==1)? 0:1);
}

EXPORT_SYMBOL(taos_get_proximity_value);

short IsTaosSensorOn(void)
{
	return isTaosSensor;
}

EXPORT_SYMBOL(IsTaosSensorOn);

#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
extern unsigned int get_hw_rev(void);
#endif

static void taos_light_enable(struct taos_data *taos)
{
	light_enable = 1;
	taos->light_count = 0;
	taos->light_buffer = 0;
	hrtimer_start(&taos->timer,light_polling_time,HRTIMER_MODE_REL);
}

static void taos_light_disable(struct taos_data *taos)
{
	hrtimer_cancel(&taos->timer);
	cancel_work_sync(&taos->work_light);
	light_enable = 0;
}
static void taos_ptime_disable(struct taos_data *taos)
{
	hrtimer_cancel(&taos->ptimer);
	cancel_work_sync(&taos->work_ptime);
	proximity_enable = 0;
}
static ssize_t poll_delay_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%lld\n", ktime_to_ns(light_polling_time));
}

static ssize_t poll_delay_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	int64_t new_delay;
	int err;

	err = strict_strtoll(buf, 10, &new_delay);
	if (err < 0)
		return err;
#if TAOS_DEBUG
	printk("[TAOS] %s: new delay = %lldns, old delay = %lldns\n", __func__, new_delay, ktime_to_ns(light_polling_time));
#endif
	mutex_lock(&taos->power_lock);
	if(new_delay != ktime_to_ns(light_polling_time)) {
		light_polling_time = ns_to_ktime(new_delay);
		if(light_enable) {
			taos_light_disable(taos);
			taos_light_enable(taos);
		}
	}
	mutex_unlock(&taos->power_lock);

	return size;
}

static ssize_t light_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", (light_enable) ? 1 : 0);
}

static ssize_t proximity_enable_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", (proximity_enable) ? 1 : 0);
}

static ssize_t light_enable_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int value;
    sscanf(buf, "%d", &value);
#if TAOS_DEBUG
	printk(KERN_INFO "[TAOS_LIGHT_SENSOR] %s: in lightsensor_file_cmd_store, input value = %d \n",__func__, value);
#endif
	if(value==1 && light_enable == OFF)
	{
#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
	if(get_hw_rev() < 0xF) 
	{
	}
	else
	{
		if(taos->power_on)
			taos->power_on();
	}
#elif defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
		if(taos->light_on)
			taos->light_on();
		msleep(20);
#else	
		if(taos->power_on)
			taos->power_on();
#endif	
		taos_on(taos,TAOS_LIGHT);
		value = ON;
		printk(KERN_INFO "[TAOS_LIGHT_SENSOR] enabled \n");
	}
	else if(value==0 && light_enable == ON) 
	{
		taos_off(taos,TAOS_LIGHT);
#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
	if(get_hw_rev() < 0xF) 
	{		
	}
	else 
	{
		if(taos->power_off)
			taos->power_off();
	}
#elif defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
		if(taos->light_off)
			taos->light_off();
#else
		if(taos->power_off)
			taos->power_off();
#endif		
		value = OFF;
		printk(KERN_INFO "[TAOS_LIGHT_SENSOR] disabled \n");
	}
	return size;
}

static ssize_t proximity_enable_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct taos_data *taos = dev_get_drvdata(dev);
	int value;
    sscanf(buf, "%d", &value);
#if TAOS_DEBUG
	printk(KERN_INFO "[TAOS_PROXIMITY] %s: input value = %d \n",__func__, value);
#endif
	if(value==1 && proximity_enable == OFF)
	{
#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
		if(get_hw_rev() < 0xF)
		{

		}
		else
		{
			if(taos->power_on)
				taos->power_on();
			if(taos->led_on)
				taos->led_on();
		}	
#elif defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
		if(taos->light_on)
			taos->light_on();
		if(taos->led_on)
			taos->led_on();
		msleep(20);
#else
		if(taos->power_on)
			taos->power_on();
		if(taos->led_on)
			taos->led_on();
#endif
		/* reset Interrupt pin */
		/* to active Interrupt, TMD2771x Interuupt pin shoud be reset. */
		i2c_smbus_write_byte(opt_i2c_client,(CMD_REG|CMD_SPL_FN|CMD_PROXALS_INTCLR));
		
		taos_on(taos,TAOS_PROXIMITY);
		input_report_abs(taos->proximity_input_dev,ABS_DISTANCE,!proximity_value);
		input_sync(taos->proximity_input_dev);
		printk("[TAOS_PROXIMITY] Temporary : Power ON, chip ID = %X\n", chipID);
	}
	else if(value==0 && proximity_enable == ON)
	{
		taos_off(taos,TAOS_PROXIMITY);
#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
		if(get_hw_rev() < 0xF)
		{

		}
		else
		{
			if(taos->led_off)
				taos->led_off();
			if(taos->power_off)
				taos->power_off();
		}
#elif defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
		if(taos->led_off)
			taos->led_off();
		if(taos->light_off)
			taos->light_off();
#else
		if(taos->led_off)
			taos->led_off();
		if(taos->power_off)
			taos->power_off();
#endif
	printk("[TAOS_PROXIMITY] Temporary : Power OFF\n");
	}
	return 0;
}

static ssize_t proximity_delay_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct taos_data *data = input_get_drvdata(input_data);
	int delay;

	delay = data->delay;
	return sprintf(buf, "%d\n", delay);
}

static ssize_t proximity_delay_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct taos_data *data = input_get_drvdata(input_data);
	int delay = simple_strtoul(buf, NULL, 10);

	if (delay < 0) {
		return size;
	}

	if (SENSOR_MAX_DELAY < delay) {
		delay = SENSOR_MAX_DELAY;
	}

	data->delay = delay;

	input_report_abs(input_data, ABS_CONTROL_REPORT, proximity_enable<<16 | delay);

	return size;
}

static ssize_t proximity_wake_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input_data = to_input_dev(dev);
	static int cnt = 1;

	input_report_abs(input_data, ABS_WAKE, cnt++);

	return count;
}

static ssize_t proximity_data_show(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	//struct input_dev *input_data = to_input_dev(dev);
	//struct taos_data *data = input_get_drvdata(input_data);

	return sprintf(buf, "%d\n", proximity_value);
}

static ssize_t proximity_adc_show(struct device *dev,	struct device_attribute *attr, char *buf)
{
	u16 proximity_value = 0;
	proximity_value = i2c_smbus_read_word_data(opt_i2c_client, CMD_REG | PRX_LO);
	if(proximity_value > TAOS_PROX_MAX)
		proximity_value = TAOS_PROX_MAX;
	return sprintf(buf,"%d\n", proximity_value);
}

static ssize_t proximity_avg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct input_dev *input_data = to_input_dev(dev);
	struct taos_data *data = input_get_drvdata(input_data);

	return sprintf(buf, "%d,%d,%d\n", data->avg[0], data->avg[1], data->avg[2]);
}

static ssize_t proximity_avg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	return proximity_enable_store(dev, attr, buf, size);
}	

static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   poll_delay_show, poll_delay_store);

static DEVICE_ATTR(proximity_avg, 0644,   proximity_avg_show, proximity_avg_store);
static DEVICE_ATTR(proximity_state, 0644, proximity_adc_show, NULL);

static struct device_attribute dev_attr_light_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       light_enable_show, light_enable_store);

static struct device_attribute dev_attr_proximity_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
	       proximity_enable_show, proximity_enable_store);

static struct device_attribute dev_attr_proximity_delay =
	__ATTR(delay, S_IRUGO | S_IWUSR | S_IWGRP,
	       proximity_delay_show, proximity_delay_store);

static struct device_attribute dev_attr_proximity_wake =
	__ATTR(wake, S_IRUGO | S_IWUSR | S_IWGRP,
	       NULL, proximity_wake_store);

static struct device_attribute dev_attr_proximity_data =
	__ATTR(data, S_IRUGO | S_IWUSR | S_IWGRP,
	       proximity_data_show, NULL);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_light_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_proximity_enable.attr,
	&dev_attr_proximity_delay.attr,
	&dev_attr_proximity_wake.attr,
	&dev_attr_proximity_data.attr,
	&dev_attr_proximity_state.attr,
	&dev_attr_proximity_avg.attr,
	NULL
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};

static ssize_t lightsensor_file_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int adc = 0;

	if(light_enable)
	{
		adc = taos_get_lux();
		return sprintf(buf,"%d\n", adc);
	}
	else
	{
		return sprintf(buf,"%d\n", adc);
	}
}

static DEVICE_ATTR(lightsensor_file_state, 0644, lightsensor_file_state_show,
	NULL);


/*****************************************************************************************
 *  
 *  function    : taos_work_func_prox 
 *  description : This function is for proximity sensor (using B-1 Mode ). 
 *                when INT signal is occured , it gets value from VO register.   
 *
 *                 
 */
#if USE_INTERRUPT
static void taos_work_func_prox(struct work_struct *work) 
{
	struct taos_data *taos = container_of(work, struct taos_data, work_prox);
	u16 adc_data;
	u16 threshold_high;
	u16 threshold_low;
	u8 prox_int_thresh[4];
	int i;
	
	/* change Threshold */ 
	adc_data = i2c_smbus_read_word_data(opt_i2c_client, CMD_REG | 0x18);
	threshold_high= i2c_smbus_read_word_data(opt_i2c_client, (CMD_REG | PRX_MAXTHRESHLO) );
	threshold_low= i2c_smbus_read_word_data(opt_i2c_client, (CMD_REG | PRX_MINTHRESHLO) );
	printk("[PROXIMITY] %s %d, high(%x), low(%x) \n",__FUNCTION__, adc_data, threshold_high, threshold_low);

	/* this is protection code for saturation */
	if ( taos_get_lux() >= 1500) {

#if TAOS_DEBUG			
		printk("[PROXIMITY] [%s] saturation. adc_data=[%X], threshold_high=[%X],  threshold_min=[%X]\n", __func__, adc_data, threshold_high, threshold_low);
#endif
		proximity_value = 0;
		prox_int_thresh[0] = (0x0000) & 0xFF;
		prox_int_thresh[1] = (0x0000 >> 8) & 0xFF;
		prox_int_thresh[2] = (PRX_THRSH_HI_PARAM) & 0xFF;
		prox_int_thresh[3] = (PRX_THRSH_HI_PARAM >> 8) & 0xFF; 
		for (i = 0; i < 4; i++)
		{
			opt_i2c_write((CMD_REG|(PRX_MINTHRESHLO + i)),&prox_int_thresh[i]);
		}
	} // end of protection code for saturation
	else if ( (threshold_high ==  (PRX_THRSH_HI_PARAM)) && (adc_data >=  (PRX_THRSH_HI_PARAM) ) )
	{
#if TAOS_DEBUG	
		printk("[PROXIMITY] [%s] +++ adc_data=[%X], threshold_high=[%X],  threshold_min=[%X]\n", __func__, adc_data, threshold_high, threshold_low);
#endif
		proximity_value = 1;
		prox_int_thresh[0] = (PRX_THRSH_LO_PARAM) & 0xFF;
		prox_int_thresh[1] = (PRX_THRSH_LO_PARAM >> 8) & 0xFF;
		prox_int_thresh[2] = (0xFFFF) & 0xFF;
		prox_int_thresh[3] = (0xFFFF >> 8) & 0xFF; 
		for (i = 0; i < 4; i++)
		{
			opt_i2c_write((CMD_REG|(PRX_MINTHRESHLO + i)),&prox_int_thresh[i]);
		}
	}
	else if ( (threshold_high ==  (0xFFFF)) && (adc_data <=  (PRX_THRSH_LO_PARAM) ) )
	{
#if TAOS_DEBUG	
		printk("[PROXIMITY] [%s] --- adc_data=[%X], threshold_high=[%X],  threshold_min=[%X]\n", __func__, adc_data, threshold_high, threshold_low);
#endif
		proximity_value = 0;
		prox_int_thresh[0] = (0x0000) & 0xFF;
		prox_int_thresh[1] = (0x0000 >> 8) & 0xFF;
		prox_int_thresh[2] = (PRX_THRSH_HI_PARAM) & 0xFF;
		prox_int_thresh[3] = (PRX_THRSH_HI_PARAM >> 8) & 0xFF; 
		for (i = 0; i < 4; i++)
		{
			opt_i2c_write((CMD_REG|(PRX_MINTHRESHLO + i)),&prox_int_thresh[i]);
		}
	}
    else
    {
        printk("[PROXIMITY] [%s] Error! Not Common Case!adc_data=[%X], threshold_high=[%X],  threshold_min=[%X]\n", __func__, adc_data, threshold_high, threshold_low);
    }
      
	if(USE_INPUT_DEVICE)
	{
		input_report_abs(taos->proximity_input_dev,ABS_DISTANCE,!proximity_value);
		printk("[PROXIMITY] [%s] proximity_value is %d (1: close, 0: far) \n", __func__, proximity_value);
		input_sync(taos->proximity_input_dev);
		mdelay(1);
	}

	/* reset Interrupt pin */
	/* to active Interrupt, TMD2771x Interuupt pin shoud be reset. */
	i2c_smbus_write_byte(opt_i2c_client,(CMD_REG|CMD_SPL_FN|CMD_PROXALS_INTCLR));

	/* enable INT */
	enable_irq(taos->irq);	
}


static irqreturn_t taos_irq_handler(int irq, void *dev_id)
{
	struct taos_data *taos = dev_id;

#ifdef TAOS_DEBUG
	printk("[PROXIMITY] taos->irq = %d\n",taos->irq);
#endif

	if(taos->irq !=-1)
	{
	    wake_lock_timeout(&prx_wake_lock,3*HZ);
		disable_irq_nosync(taos->irq);
		queue_work(taos_wq, &taos->work_prox);
	}
	
	return IRQ_HANDLED;
}

static void taos_work_func_light(struct work_struct *work)
{
	int i=0;
	struct taos_data *taos = container_of(work, struct taos_data, work_light);
	int lux=0;
	//state_type level_state = LIGHT_INIT;
	//read value
	lux =  taos_get_lux();

	for (i=0; ARRAY_SIZE(adc_table); i++)
		if(lux <= adc_table[i])
			break;

	if(taos->light_buffer == i) {
		if(taos->light_count++ == LIGHT_BUFFER_NUM) {
#if defined(CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577)
			input_report_abs(taos->light_input_dev, ABS_MISC, lux + 1);
#else
			input_report_rel(taos->light_input_dev, REL_MISC, lux + 1);
#endif
			input_sync(taos->light_input_dev);
			taos->light_count = 0;
			count++;
			if (count == 10) {
				printk("[TAOS](%d) Lux=%d, Ch[0]=%d, Ch[1]=%d\n", count, lux, cleardata, irdata);
				count = 0;
			}
		}
	} else {
		taos->light_buffer = i;
		taos->light_count = 0;
	}
}

static enum hrtimer_restart taos_timer_func(struct hrtimer *timer)
{
	struct taos_data *taos = container_of(timer, struct taos_data, timer);
			
	queue_work(taos_wq, &taos->work_light);
	light_polling_time = ktime_set(0,0);
	light_polling_time = ktime_add_us(light_polling_time,200000);
	hrtimer_start(&taos->timer,light_polling_time,HRTIMER_MODE_REL);
	
	return HRTIMER_NORESTART;

}


static void taos_work_func_ptime(struct work_struct *work) 
{
	struct taos_data *taos = container_of(work, struct taos_data, work_ptime);
	u16 proximity_value = 0;
	int min = 0, max = 0, avg = 0;
 	int i = 0;
 	
 	for(i =0 ; i < PROX_READ_NUM ; i++) {
 		proximity_value = i2c_smbus_read_word_data(opt_i2c_client, CMD_REG | PRX_LO);
		if(proximity_value > TAOS_PROX_MIN) {
			if(proximity_value > TAOS_PROX_MAX)
				proximity_value = TAOS_PROX_MAX;
			avg += proximity_value;
			if(!i)
				min = proximity_value;
			if (proximity_value < min)
				min = proximity_value;
			if (proximity_value > max)
				max = proximity_value;
		}
		else {
			proximity_value = TAOS_PROX_MIN;
			break;
		}
		msleep(40);
 	}
 	avg /= i;
 	taos->avg[0] = min;
 	taos->avg[1] = avg;
 	taos->avg[2] = max;
}

static enum hrtimer_restart taos_ptimer_func(struct hrtimer *timer)
{
	queue_work(taos_test_wq, &taos->work_ptime);  
	hrtimer_forward_now(&taos->ptimer,prox_polling_time);
	return HRTIMER_RESTART;
}

#endif

int taos_get_lux(void)
{
	int i=0;
	int integration_time=27; //49->27
	int als_gain = 1;
	int CPL = 0;
	int Lux1=0, Lux2=0;
	int calculated_lux = 0;
	u8 prox_int_thresh[4];
	// add for returning -1 if the Lisght sensor had bee closed : wonjun 0816
	if(taos_chip_status == TAOS_CHIP_SLEEP)
	{
		printk("!!! Light sensor had been off return 0\n");
		return 0;
	}
	//
	cleardata = i2c_smbus_read_word_data(opt_i2c_client, CMD_REG | ALS_CHAN0LO);
	irdata = i2c_smbus_read_word_data(opt_i2c_client, CMD_REG | ALS_CHAN1LO);

#if defined(CONFIG_USA_MODEL_SGH_T769)
	CPL = (integration_time * als_gain * 1000) / 105;
	Lux1 = (int)((1000*cleardata - 2000 * irdata) / CPL);
	Lux2 = (int)((660 * cleardata - 1250 * irdata) / CPL);

	if(Lux1 > Lux2)
		calculated_lux = Lux1;
	else if (Lux2 >= Lux1)
		calculated_lux = Lux2;

	if(calculated_lux < 0)
		calculated_lux = 0;
#elif defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
	CPL = (integration_time * als_gain * 1000) / 100;
	Lux1 = (int)((1000*cleardata - 1850 * irdata) / CPL);
	Lux2 = (int)((660 * cleardata - 1130 * irdata) / CPL);

	if(Lux1 > Lux2)
		calculated_lux = Lux1;
	else if (Lux2 >= Lux1)
		calculated_lux = Lux2;

	if(calculated_lux < 0)
		calculated_lux = 0;
#elif defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
	CPL = (integration_time * als_gain * 1000) / 97;
	Lux1 = (int)((1000 * cleardata - 1720 * irdata) / CPL);
	Lux2 = (int)((700 * cleardata - 1150 * irdata) / CPL);

	if(Lux1 > Lux2)
		calculated_lux = Lux1;
	else if (Lux2 >= Lux1)
		calculated_lux = Lux2;

	if(calculated_lux < 0)
		calculated_lux = 0;
#else
	int irfactor, irfactor2=0;
	irfactor = 1000 * cleardata - 2000 * irdata;
	irfactor2 = 600 * cleardata - 1131 * irdata;

	if( irfactor2 > irfactor)
		irfactor = irfactor2;

	if(0 > irfactor)
		irfactor = 0;

	calculated_lux = 125 * irfactor / (integration_time * als_gain);
#endif	

	/* QUICK FIX : Code for saturation case */
	//Saturation value seems to be affected by setting ALS_TIME, 0xEE (=238) which sets max count to 20480.
	if ( cleardata >= 9000 || irdata >= 9000)  // change ATIME value from 50ms to 27ms, so i changed condition 18000 -> 9000 
	{
#if defined(CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined(CONFIG_CAN_MODEL_SGH_I757M)
		calculated_lux = MAX_LUX; // Coefficient 1000 for milli lux 
#else
		calculated_lux = MAX_LUX * 1000;
#endif		
		if(proximity_value == 1)
		{
			proximity_value = 0;
			prox_int_thresh[0] = (0x0000) & 0xFF;
			prox_int_thresh[1] = (0x0000 >> 8) & 0xFF;
			prox_int_thresh[2] = (PRX_THRSH_HI_PARAM) & 0xFF;
			prox_int_thresh[3] = (PRX_THRSH_HI_PARAM >> 8) & 0xFF; 
			for (i = 0; i < 4; i++)
			{
				opt_i2c_write((CMD_REG|(PRX_MINTHRESHLO + i)),&prox_int_thresh[i]);
			}

			if(USE_INPUT_DEVICE)
			{
				input_report_abs(taos->proximity_input_dev,ABS_DISTANCE,!proximity_value);
				printk("[PROXIMITY] [%s] saturation. proximity_value is %d (1: close, 0: far) \n", __func__, proximity_value);
				input_sync(taos->proximity_input_dev);
				mdelay(1);
			}
		}
	}	//if ( cleardata >= 9000 || irdata >= 9000)
#if defined(CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined(CONFIG_CAN_MODEL_SGH_I757M)
	calculated_lux = calculated_lux;
#else
	calculated_lux = calculated_lux / 1000;
#endif

	return (calculated_lux);
}

#if 0
#define	ADC_BUFFER_NUM			8
static int adc_value_buf[ADC_BUFFER_NUM] = {0, 0, 0, 0, 0, 0};

static int taos_read_als_data(void)
{
	int ret = 0;
	int lux_val=0;
	
	u8 reg_val;

	int i=0;
	int j=0;
	unsigned int adc_total = 0;
	static int adc_avr_value = 0;
	unsigned int adc_index = 0;
	static unsigned int adc_index_count = 0;
	unsigned int adc_max = 0;
	unsigned int adc_min = 0;


	if ((ret = opt_i2c_read((CMD_REG | CNTRL), &reg_val))< 0)
	{
		gprintk( "opt_i2c_read to cmd reg failed in taos_read_als_data\n");			
		return 0; //r0 return (ret);
	}

	if ((reg_val & (CNTL_ADC_ENBL | CNTL_PWRON)) != (CNTL_ADC_ENBL | CNTL_PWRON))
	{
		return 0; //r0 return (-ENODATA);
	}

	if ((lux_val = taos_get_lux()) < 0)
	{
		gprintk( "taos_get_lux() returned error %d in taos_read_als_data\n", lux_val);
	}

         //gprintk("** Returned lux_val  = [%d]\n", lux_val);

	/* QUICK FIX : multiplu lux_val by 4 to make the value similar to ADC value. */
	lux_val = lux_val * 5;
	
	if (lux_val >= MAX_LUX)
	{
		lux_val = MAX_LUX; //return (MAX_LUX);
	}
	
	cur_adc_value = lux_val; 
	//gprintk("taos_read_als_data cur_adc_value = %d (0x%x)\n", cur_adc_value, cur_adc_value);
	
		adc_index = (adc_index_count++)%ADC_BUFFER_NUM;		

		if(cur_state == LIGHT_INIT) //ADC buffer initialize (light sensor off ---> light sensor on)
		{
			for(j = 0; j<ADC_BUFFER_NUM; j++)
				adc_value_buf[j] = lux_val; // value;
		}
	    else
	    {
	    	adc_value_buf[adc_index] = lux_val; //value;
		}
		
		adc_max = adc_value_buf[0];
		adc_min = adc_value_buf[0];

		for(i = 0; i <ADC_BUFFER_NUM; i++)
		{
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
#endif
void taos_chip_init(void)
{

}

void taos_chip_on(void)
{
	u8 value;
	u8 prox_int_thresh[4];
	int err = 0;
	int i;
       int fail_num = 0;
    
	printk("[TAOS] %s \n",__func__);
	value = CNTL_REG_CLEAR;
	if ((err = (opt_i2c_write((CMD_REG|CNTRL),&value))) < 0){
			printk("[diony] i2c_smbus_write_byte_data to clr ctrl reg failed in ioctl prox_on\n");
                     fail_num++;
		}
	value = ALS_TIME_PARAM;
	if ((err = (opt_i2c_write((CMD_REG|ALS_TIME), &value))) < 0) {
			printk("[diony] i2c_smbus_write_byte_data to als time reg failed in ioctl prox_on\n");
                     fail_num++;            
		}
	value = PRX_ADC_TIME_PARAM;
	if ((err = (opt_i2c_write((CMD_REG|PRX_TIME), &value))) < 0) {
			printk("[diony] i2c_smbus_write_byte_data to prox time reg failed in ioctl prox_on\n");
                     fail_num++;            
		}
	value = PRX_WAIT_TIME_PARAM;
	if ((err = (opt_i2c_write((CMD_REG|WAIT_TIME), &value))) < 0){
			printk("[diony] i2c_smbus_write_byte_data to wait time reg failed in ioctl prox_on\n");
                     fail_num++;            
		}
	value = INTR_FILTER_PARAM;
	if ((err = (opt_i2c_write((CMD_REG|INTERRUPT), &value))) < 0) {
			printk("[diony] i2c_smbus_write_byte_data to interrupt reg failed in ioctl prox_on\n");
                     fail_num++;            
		}
	value = PRX_CONFIG_PARAM;
	if ((err = (opt_i2c_write((CMD_REG|PRX_CFG), &value))) < 0) {
			printk("[diony] i2c_smbus_write_byte_data to prox cfg reg failed in ioctl prox_on\n");
                     fail_num++;            
		}
#if defined(CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
	if (chipID == 0x39)
		value = 0x08;
	else if(chipID == 0x29)
		value = 0x0A;
	else
		value = 0x08;
#elif defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
	value = 0x08;
#else		
	value = PRX_PULSE_CNT_PARAM;
#endif
	if ((err = (opt_i2c_write((CMD_REG|PRX_COUNT), &value))) < 0){
			printk("[diony] i2c_smbus_write_byte_data to prox cnt reg failed in ioctl prox_on\n");
                     fail_num++;            
		}
	if (chipID == 0x39)
		value = PRX_GAIN_PARAM; // 100mA, ch1, pgain 4x, again 1x
	else if(chipID == 0x29)
		value = 0x20; // 100mA, ch1, pgain 1x, again 1x
	else
		value = PRX_GAIN_PARAM;	
	if ((err = (opt_i2c_write((CMD_REG|GAIN), &value))) < 0) {
			printk("[diony] i2c_smbus_write_byte_data to prox gain reg failed in ioctl prox_on\n");
                     fail_num++;            
		}
		prox_int_thresh[0] = (0x0000) & 0xFF;
		prox_int_thresh[1] = (0x0000 >> 8) & 0xFF;
		prox_int_thresh[2] = (PRX_THRSH_HI_PARAM) & 0xFF;
		prox_int_thresh[3] = (PRX_THRSH_HI_PARAM >> 8) & 0xFF; 
	for (i = 0; i < 4; i++) {
		if ((err = (opt_i2c_write((CMD_REG|(PRX_MINTHRESHLO + i)),&prox_int_thresh[i]))) < 0) {
				printk("[diony]i2c_smbus_write_byte_data to prox int thrsh regs failed in ioctl prox_on\n");
                            fail_num++;                
			}
	}
	value = CNTL_INTPROXPON_ENBL;
	if ((err = (opt_i2c_write((CMD_REG|CNTRL), &value))) < 0) {
			printk("[diony]i2c_smbus_write_byte_data to ctrl reg "
						"failed in ioctl prox_on\n");
                     fail_num++;            
		}

    	mdelay(12);    

	if ( fail_num == 0) 
	{			
#if IRQ_WAKE    
        	err = enable_irq_wake(taos ->irq);
	    	printk("[TAOS] register wakeup source = %d\n",err);
        	if (err) 
        		printk("[TAOS] register wakeup source failed\n");
#endif    
		taos_chip_status = TAOS_CHIP_WORKING;
	}
	else
	{
		printk( "I2C failed in taos_chip_on, # of fail I2C=[%d]\n", fail_num);
	}

}    

static int taos_chip_off(void)
{
	int ret = 0;
	u8 reg_cntrl;
	int err = 0;

	gprintk("[TAOS] %s \n",__func__);
	
#if IRQ_WAKE    
	err = disable_irq_wake(taos ->irq);
	if (err) 
		printk("[TAOS] register wakeup source failed\n");
#endif

	reg_cntrl = CNTL_REG_CLEAR;
	
	if ((ret = opt_i2c_write((CMD_REG | CNTRL), &reg_cntrl))< 0)
	{
		gprintk( "opt_i2c_write to ctrl reg failed in taos_chip_off\n");
		return (ret);
	}

	taos_chip_status = TAOS_CHIP_SLEEP;
	return (ret);
}
/*****************************************************************************************
*  function    : taos_on
*  description : This function is power-on function for optical sensor.
*
*  int type    : Sensor type. Two values is available (PROXIMITY,LIGHT).
*                it support power-on function separately.
*/

void taos_on(struct taos_data *taos, int type)
{
	printk("%s : type=%d (0:light, 1:prox, 2:all)\n", __func__,type);

       if(1)//taos_chip_status != TAOS_CHIP_WORKING)
       {
            taos_chip_on();
       }

	if(type == TAOS_PROXIMITY || type == TAOS_ALL)
	{
		printk("enable irq for proximity\n");
		enable_irq(taos ->irq);
		proximity_enable = 1;     
        taos_prx_status = TAOS_PRX_OPENED;
              
		printk("[TAOS_PROXIMITY] %s: timer start for prox sensor\n",__func__);
		hrtimer_start(&taos->ptimer,prox_polling_time,HRTIMER_MODE_REL);
            
	}
	if(type == TAOS_LIGHT || type == TAOS_ALL)
	{
		printk(KERN_INFO "[TAOS_LIGHT] %s: timer start for light sensor\n",__func__);
		light_polling_time = ktime_set(0,0);
		light_polling_time = ktime_add_us(light_polling_time,200000);

        hrtimer_start(&taos->timer,light_polling_time,HRTIMER_MODE_REL);
		light_enable = 1;
		msleep(50);  // wonjun 0816 : add for making sure validation of first polling value		
        taos_als_status = TAOS_ALS_OPENED;
	}
}

/*****************************************************************************************
 *  function    : taos_off
 *  description : This function is power-off function for optical sensor.
 *
 *  int type    : Sensor type. Three values is available (PROXIMITY,LIGHT,ALL).
 *                it support power-on function separately.
 */
void taos_off(struct taos_data *taos, int type)
{
#ifdef TAOS_DEBUG
	printk("%s : type=%d (0:light, 1:prox, 2:all)\n",__FUNCTION__, type);
#endif

	if(type == TAOS_PROXIMITY || type==TAOS_ALL)
	{
		printk("[TAOS] %s: disable irq for proximity \n",__func__);
	        disable_irq(taos->irq);
		hrtimer_cancel(&taos->ptimer);              
    	    proximity_enable = 0;
        	taos_prx_status = TAOS_PRX_CLOSED;
		proximity_value = 0;  // wonjun initialize proximity_value
	}
    
    	if(type ==TAOS_LIGHT || type==TAOS_ALL)
	{
		printk("[TAOS] %s: timer cancel for light sensor\n",__func__);
		hrtimer_cancel(&taos->timer);
		light_enable = 0;
        taos_als_status = TAOS_ALS_CLOSED;
	}

       if(taos_prx_status == TAOS_PRX_CLOSED && taos_als_status == TAOS_ALS_CLOSED && taos_chip_status == TAOS_CHIP_WORKING)
       {
            taos_chip_off();

       }

}

/*************************************************************************/
/*		TAOS file operations  				         */
/*************************************************************************/
    
static struct file_operations light_fops = 
{
	.owner = THIS_MODULE,
};         

static struct miscdevice light_device = 
{
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "light",
    .fops   = &light_fops,
};

static int taos_opt_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int err = 0;
	struct input_dev *input_dev;
	struct taos_platform_data *pdata = client->dev.platform_data;

	printk(KERN_INFO "%s isTaosSensor %d\n",__FUNCTION__, isTaosSensor);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		printk(KERN_INFO "[TAOS] i2c_check_functionality error\n");
		err = -ENODEV;
		goto exit;
	}
	if ( !i2c_check_functionality(client->adapter,I2C_FUNC_SMBUS_BYTE_DATA) ) {
		printk(KERN_INFO "[TAOS] byte op is not permited.\n");
		goto exit;
	}

	/* OK. For now, we presume we have a valid client. We now create the
	client structure, even though we cannot fill it completely yet. */
	if (!(taos = kzalloc(sizeof(struct taos_data), GFP_KERNEL)))
	{
		err = -ENOMEM;
		goto exit;
	}
	memset(taos, 0, sizeof(struct taos_data));
	taos->client = client;
	i2c_set_clientdata(client, taos);
	opt_i2c_client = client;

    mdelay(12); 

if(pdata) {
		if(pdata->power_on)
			taos->power_on = pdata->power_on;
		if(pdata->power_off)
			taos->power_off = pdata->power_off;
		if(pdata->led_on)
			taos->led_on = pdata->led_on;
		if(pdata->led_off)
			taos->led_off = pdata->led_off;
#if defined (CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
		if(pdata->light_on)
			taos->light_on = pdata->light_on;
		if(pdata->light_off)
			taos->light_off = pdata->light_off;
#endif
	}
#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
	if(get_hw_rev() < 0xF) {	
		if(taos->led_on)
			taos->led_on();
		if(taos->power_on)
			taos->power_on();
		msleep(1);
	}
#elif defined (CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
	if(taos->light_on)
		taos->light_on();
	msleep(20);
#else
	if(taos->power_on)
		taos->power_on();
#endif
	
    /* wake lock init */
	wake_lock_init(&prx_wake_lock, WAKE_LOCK_SUSPEND, "prx_wake_lock");
	mutex_init(&taos->power_lock);
	
	/* allocate proximity input_device */
	if(USE_INPUT_DEVICE)
	{
		input_dev = input_allocate_device();
		if (input_dev == NULL) 
		{
			pr_err("Failed to allocate input device\n");
			err = -ENOMEM;
			goto err_input_allocate_device_proximity;
		}
		taos->proximity_input_dev = input_dev;
		input_set_drvdata(input_dev,taos);
		input_dev->name = "proximity_sensor";
		input_set_capability(input_dev, EV_ABS, ABS_DISTANCE);
		input_set_abs_params(input_dev, ABS_DISTANCE, 0, 1, 0, 0);
	
		err = input_register_device(input_dev);
		if (err) 
		{
			pr_err("Unable to register %s input device\n", input_dev->name);
			goto err_input_register_device_proximity;
		}

		err = sysfs_create_group(&input_dev->dev.kobj,
				 &proximity_attribute_group);
		if (err) {
			pr_err("%s: could not create sysfs group\n", __func__);
			goto err_sysfs_create_group_proximity;
		}
	}

#if USE_INTERRUPT
	/* WORK QUEUE Settings */
	taos_wq = create_singlethread_workqueue("taos_wq");
	if (!taos_wq) {
		err = -ENOMEM;
		pr_err("%s: could not create workqueue\n", __func__);
		goto err_create_workqueue;
	}
	INIT_WORK(&taos->work_prox, taos_work_func_prox);
   	INIT_WORK(&taos->work_light, taos_work_func_light); 

   	taos_test_wq = create_singlethread_workqueue("taos_test_wq");
   	if (!taos_test_wq) {
		err = -ENOMEM;
		pr_err("%s: could not create workqueue\n", __func__);
		goto err_create_workqueue;
	}
 	INIT_WORK(&taos->work_ptime, taos_work_func_ptime);    
#endif

	/* hrtimer settings.  we poll for light values using a timer. */
	hrtimer_init(&taos->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	taos->timer.function = taos_timer_func;

	hrtimer_init(&taos->ptimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	prox_polling_time = ns_to_ktime(2000 * NSEC_PER_MSEC);
	taos->ptimer.function = taos_ptimer_func;

	/* allocate lightsensor-level input_device */
	if(USE_INPUT_DEVICE)
	{
	    input_dev = input_allocate_device();
		if (!input_dev) {
			pr_err("%s: could not allocate input device\n", __func__);
			err = -ENOMEM;
			goto err_input_allocate_device_light;
		}
		input_set_drvdata(input_dev, taos);
		input_dev->name = "light_sensor";
#if defined(CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577)
		input_set_capability(input_dev, EV_ABS, ABS_MISC);
		input_set_abs_params(input_dev, ABS_MISC, 0, 1, 0, 0);
#else
		set_bit(EV_REL, input_dev->evbit);
		input_set_capability(input_dev, EV_REL, REL_MISC);
		//input_set_abs_params(input_dev, ABS_MISC, 0, 1, 0, 0);
#endif

		err = input_register_device(input_dev);
		if (err) 
		{
			pr_err("Unable to register %s input device\n", input_dev->name);
			goto err_input_register_device_light;
		}
		taos->light_input_dev = input_dev;
		err = sysfs_create_group(&input_dev->dev.kobj,
				 &light_attribute_group);
		if (err) {
			pr_err("%s: could not create sysfs group\n", __func__);
			goto err_sysfs_create_group_light;
		}
	}
	
	/* misc device Settings */
	err = misc_register(&light_device);
	if(err) 
	{
		pr_err(KERN_ERR "[TAOS] %s: misc_register failed - light\n",__func__);
	}

	/* set sysfs for proximity sensor */
	taos->proximity_class = class_create(THIS_MODULE, "proximity");
	if(IS_ERR(taos->proximity_class))
		pr_err("%s: could not create proximity_class\n", __func__);

	taos->proximity_dev = device_create(taos->proximity_class,
		NULL, 0, NULL, "proximity");
	if(IS_ERR(taos->proximity_dev))
		pr_err("%s: could not create proximity_dev\n", __func__);

	if (device_create_file(taos->proximity_dev,
		&dev_attr_proximity_state) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
			dev_attr_proximity_state.attr.name);
	}

	if (device_create_file(taos->proximity_dev,
		&dev_attr_proximity_avg) < 0) {
		pr_err("%s: could not create device file(%s)!\n", __func__,
			dev_attr_proximity_avg.attr.name);
	}
	dev_set_drvdata(taos->proximity_dev, taos);
	
	/* set sysfs for light sensor */
	taos->lightsensor_class = class_create(THIS_MODULE, "lightsensor");
	if(IS_ERR(taos->lightsensor_class))
		pr_err("Failed to create class(lightsensor)!\n");

	taos->switch_cmd_dev = device_create(taos->lightsensor_class,
		NULL, 0, NULL, "switch_cmd");
	if(IS_ERR(taos->switch_cmd_dev))
		pr_err("Failed to create device(switch_cmd_dev)!\n");

	if(device_create_file(taos->switch_cmd_dev,
		&dev_attr_lightsensor_file_state) < 0)
		pr_err("Failed to create device file(%s)!\n",
			dev_attr_lightsensor_file_state.attr.name);

	dev_set_drvdata(taos->switch_cmd_dev, taos);
	mdelay(2);
#if USE_INTERRUPT
	/* INT Settings */	
	taos->irq = -1;

	err = request_threaded_irq(TAOS_INT, NULL, taos_irq_handler, IRQF_DISABLED|IRQ_TYPE_EDGE_FALLING, "taos_int", taos);
	if (err)
	{
		printk("[TAOS] request_irq failed for taos\n");
	}

	printk("[TAOS] register irq = %d\n",TAOS_INT);
	taos->irq = TAOS_INT;
#endif

	chipID = i2c_smbus_read_byte_data(opt_i2c_client, CMD_REG | CHIPID);
    printk("[TAOS] %s: chipID[%X]\n", __func__, chipID);

	// maintain power-down mode before using sensor
	taos_off(taos,TAOS_ALL);
	printk("%s: done!!\n",__func__);
#if defined (CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
	if(taos->light_off)
		taos->light_off();	
#endif
	return 0;
	
err_sysfs_create_group_light:
	input_unregister_device(taos->light_input_dev);	
err_input_register_device_light:
err_input_allocate_device_light:
	destroy_workqueue(taos_wq);
	destroy_workqueue(taos_test_wq);
err_create_workqueue:
	sysfs_remove_group(&taos->proximity_input_dev->dev.kobj,
			   &proximity_attribute_group);
err_sysfs_create_group_proximity:
	input_unregister_device(taos->proximity_input_dev);
err_input_register_device_proximity:
err_input_allocate_device_proximity:
	mutex_destroy(&taos->power_lock);
	wake_lock_destroy(&prx_wake_lock);
	kfree(taos);
    taos = NULL;
    opt_i2c_client = NULL;
    isTaosSensor = 0;
exit:
    taos = NULL;
    opt_i2c_client = NULL;
    isTaosSensor = 0;    
	return err;
}


static int taos_opt_remove(struct i2c_client *client)
{
	struct taos_data *taos = i2c_get_clientdata(client);
#ifdef TAOS_DEBUG
	printk(KERN_INFO "%s\n",__FUNCTION__);
#endif	
	if(USE_INPUT_DEVICE) {
		sysfs_remove_group(&taos->light_input_dev->dev.kobj,
			   &light_attribute_group);
		input_unregister_device(taos->light_input_dev);
		sysfs_remove_group(&taos->proximity_input_dev->dev.kobj,
			   &proximity_attribute_group);
		input_unregister_device(taos->proximity_input_dev);
	}
	if (light_enable)
		taos_light_disable(taos);
	if (proximity_enable) {
		taos_ptime_disable(taos);
		disable_irq(taos->irq);
	}
	if (taos_wq)
		destroy_workqueue(taos_wq);
	if (taos_test_wq)
		destroy_workqueue(taos_test_wq);
	mutex_destroy(&taos->power_lock);
	wake_lock_destroy(&prx_wake_lock);
	kfree(taos);
	return 0;
}

static void taos_opt_shutdown(struct i2c_client *client)
{
	struct taos_data *taos = i2c_get_clientdata(client);
#ifdef TAOS_DEBUG
	printk(KERN_INFO "%s\n",__FUNCTION__);
#endif
	if(USE_INPUT_DEVICE) {
		sysfs_remove_group(&taos->light_input_dev->dev.kobj,
			   &light_attribute_group);
		input_unregister_device(taos->light_input_dev);
		sysfs_remove_group(&taos->proximity_input_dev->dev.kobj,
			   &proximity_attribute_group);
		input_unregister_device(taos->proximity_input_dev);
	}
	if (light_enable)
		taos_light_disable(taos);
	if (proximity_enable) {
		taos_ptime_disable(taos);
		disable_irq(taos->irq);
	}
	if (taos_wq)
		destroy_workqueue(taos_wq);
	if (taos_test_wq)
		destroy_workqueue(taos_test_wq);
	mutex_destroy(&taos->power_lock);
	wake_lock_destroy(&prx_wake_lock);
	kfree(taos);
}
#ifdef CONFIG_PM
static int taos_opt_suspend(struct device *dev)
{
	/* We disable power only if proximity is disabled.  If proximity
	 * is enabled, we leave power on because proximity is allowed
	 * to wake up device.  We remove power without changing
	 * gp2a->power_state because we use that state in resume
	*/

#ifdef TAOS_DEBUG
	printk(KERN_INFO "[%s] TAOS !!suspend mode!!\n",__FUNCTION__);
#endif
	return 0;
}

static int taos_opt_resume(struct device *dev)
{
#ifdef TAOS_DEBUG
	printk(KERN_INFO "[%s] TAOS !!resume mode!!\n",__FUNCTION__);
#endif
	return 0;
}
#else
#define taos_opt_suspend NULL
#define taos_opt_resume NULL
#endif


static const struct i2c_device_id taos_id[] = {
	{ "taos", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, taos_id);

static const struct dev_pm_ops taos_pm_ops = {
	.suspend	= taos_opt_suspend,
	.resume		= taos_opt_resume,
};

static struct i2c_driver taos_opt_driver = {	
	.driver = {
		.owner	= THIS_MODULE,	
		.name	= "taos",
		.pm = &taos_pm_ops
	},
	.probe		= taos_opt_probe,
	.remove		= taos_opt_remove,
	.shutdown	= taos_opt_shutdown,
	.id_table	= taos_id,
};

static int __init taos_opt_init(void)
{

#ifdef TAOS_DEBUG
	printk(KERN_INFO "%s\n",__FUNCTION__);
#endif
	return i2c_add_driver(&taos_opt_driver);
}

static void __exit taos_opt_exit(void)
{
	i2c_del_driver(&taos_opt_driver);
#ifdef TAOS_DEBUG
	printk(KERN_INFO "%s\n",__FUNCTION__);
#endif
}

module_init( taos_opt_init );
module_exit( taos_opt_exit );

MODULE_AUTHOR("SAMSUNG");
MODULE_DESCRIPTION("Optical Sensor driver for taosp002s00f");
MODULE_LICENSE("GPL");

