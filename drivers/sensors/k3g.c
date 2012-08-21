/*
 *  Copyright (C) 2010, Samsung Electronics Co. Ltd. All Rights Reserved.
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
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <asm/div64.h>
#include <linux/i2c/k3g.h>
#include <linux/delay.h>
#include <mach/gpio.h>

/* k3g chip id */
#define DEVICE_ID	0xD3
/* k3g gyroscope registers */
#define WHO_AM_I	0x0F
#define CTRL_REG1	0x20  /* power control reg */
#define CTRL_REG2	0x21  /* power control reg */
#define CTRL_REG3	0x22  /* power control reg */
#define CTRL_REG4	0x23  /* interrupt control reg */
#define CTRL_REG5	0x24  /* interrupt control reg */
#define OUT_TEMP	0x26  /* Temperature data */
#define STATUS_REG	0x27
#define AXISDATA_REG	0x28
#define OUT_Y_L		0x2A
#define FIFO_CTRL_REG	0x2E
#define FIFO_SRC_REG	0x2F
#define PM_OFF		0x00
#define PM_NORMAL	0x08
#define ENABLE_ALL_AXES	0x07
#define BYPASS_MODE	0x00
#define FIFO_MODE	0x20
#define FIFO_SELFTEST_MODE 0x3F
#define MIN_ZERO_RATE -30
#define MAX_ZERO_RATE 30

#define FIFO_EMPTY	0x20
#define FSS_MASK	0x1F
#define WTM_MASK	0x80
#define ODR_MASK	0xF0
#define ODR105_BW12_5	0x00  /* ODR = 105Hz; BW = 12.5Hz */
#define ODR105_BW25	0x10  /* ODR = 105Hz; BW = 25Hz   */
#define ODR210_BW12_5	0x40  /* ODR = 210Hz; BW = 12.5Hz */
#define ODR210_BW25	0x50  /* ODR = 210Hz; BW = 25Hz   */
#define ODR210_BW50	0x60  /* ODR = 210Hz; BW = 50Hz   */
#define ODR210_BW70	0x70  /* ODR = 210Hz; BW = 70Hz   */
#define ODR420_BW20	0x80  /* ODR = 420Hz; BW = 20Hz   */
#define ODR420_BW25	0x90  /* ODR = 420Hz; BW = 25Hz   */
#define ODR420_BW50	0xA0  /* ODR = 420Hz; BW = 50Hz   */
#define ODR420_BW110	0xB0  /* ODR = 420Hz; BW = 110Hz  */
#define ODR840_BW30	0xC0  /* ODR = 840Hz; BW = 30Hz   */
#define ODR840_BW35	0xD0  /* ODR = 840Hz; BW = 35Hz   */
#define ODR840_BW50	0xE0  /* ODR = 840Hz; BW = 50Hz   */
#define ODR840_BW110	0xF0  /* ODR = 840Hz; BW = 110Hz  */

#define DPS_FACTORY
#ifdef DPS_FACTORY
#define CTRL_REG4_DPS_SHIFT 4
#define CTRL_REG4_250DPS	0
#define CTRL_REG4_500DPS	1
#define CTRL_REG4_2000DPS	2
#endif

#define MIN_ST		175
#define MAX_ST		875
#define AC		(1 << 7) /* register auto-increment bit */
#define MAX_ENTRY	20
#define MAX_DELAY	(MAX_ENTRY * 9523809LL)

#define K3G_MAJOR	102
#define K3G_MINOR	4

#define CALIBRATION_FILE_PATH	"/efs/gyro_calibration_data"
#define CALIBRATION_DATA_AMOUNT 32	

/* default register setting for device init */
static const char default_ctrl_regs[] = {
	0x3F,	/* 105HZ, PM-normal, xyz enable */
	0x00,	/* normal mode */
	0x04,	/* fifo wtm interrupt on */
	0x90,	/* block data update, 500d/s */
	0x40,	/* fifo enable */
};

static const struct odr_delay {
	u8 odr; /* odr reg setting */
	u32 delay_ns; /* odr in ns */
} odr_delay_table[] = {
/*	{  ODR840_BW110, 1190476LL }, */ /* 840Hz */
/*	{  ODR420_BW110, 2380952LL }, */ /* 420Hz */
	{   ODR210_BW70, 4761904LL }, /* 210Hz */
	{   ODR105_BW25, 9523809LL }, /* 105Hz */
};

/*
 * K3G gyroscope data
 * brief structure containing gyroscope values for yaw, pitch and roll in
 * signed short
 */
struct k3g_t {
	s16 x;
	s16 y;
	s16 z;
};

struct k3g_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct mutex lock;
	struct workqueue_struct *k3g_wq;
	struct work_struct work;
	struct hrtimer timer;
	struct class *k3g_gyro_dev_class;
	struct device *dev;
	struct k3g_t cal_data;
	bool enable;
	bool drop_next_event;
	bool interruptible;	/* interrupt or polling? */
	int entries;		/* number of fifo entries */
	u8 ctrl_regs[5];	/* saving register settings */
	u32 time_to_read;	/* time needed to read one entry */
	ktime_t polling_delay;	/* polling time for timer */
#ifdef DPS_FACTORY
	int factory_dps;
#endif
	int 	(*power_on) (void);
	int 	(*power_off) (void);
};

/* set sysfs for light sensor test mode*/
extern int sensors_register(struct device *dev, void * drvdata, struct device_attribute *attributes[], char *name);
static struct device *gyro_sensor_device;

static int k3g_open_calibration(struct k3g_data *k3g_data)
{
	struct file *cal_filp = NULL;
	int err = 0;
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	cal_filp = filp_open(CALIBRATION_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(cal_filp)) {
		pr_err("%s: Can't open calibration file\n", __func__);
		set_fs(old_fs);
		err = PTR_ERR(cal_filp);
		return err;
	}

	err = cal_filp->f_op->read(cal_filp,
		(char *)&k3g_data->cal_data, 3 * sizeof(s16), &cal_filp->f_pos);
	if (err != 3 * sizeof(s16)) {
		pr_err("%s: Can't read the cal data from file\n", __func__);
		err = -EIO;
	}

	printk("%s: (%u,%u,%u)\n", __func__,
		k3g_data->cal_data.x, k3g_data->cal_data.y, k3g_data->cal_data.z);

	filp_close(cal_filp, current->files);
	set_fs(old_fs);

	return err;
}

static int k3g_read_fifo_status(struct k3g_data *k3g_data)
{
	int fifo_status;

	fifo_status = i2c_smbus_read_byte_data(k3g_data->client, FIFO_SRC_REG);
	if (fifo_status < 0) {
		pr_err("%s: failed to read fifo source register\n",
							__func__);
		return fifo_status;
	}
	return (fifo_status & FSS_MASK) + !(fifo_status & FIFO_EMPTY);
}

static int k3g_restart_fifo(struct k3g_data *k3g_data)
{
	int res = 0;

	res = i2c_smbus_write_byte_data(k3g_data->client,
			FIFO_CTRL_REG, BYPASS_MODE);
	if (res < 0) {
		pr_err("%s : failed to set bypass_mode\n", __func__);
		return res;
	}

	res = i2c_smbus_write_byte_data(k3g_data->client,
			FIFO_CTRL_REG, FIFO_MODE | (k3g_data->entries - 1));

	if (res < 0)
		pr_err("%s : failed to set fifo_mode\n", __func__);

	return res;
}

static void set_polling_delay(struct k3g_data *k3g_data, int res)
{
	s64 delay_ns;

	delay_ns = k3g_data->entries + 1 - res;
	if (delay_ns < 0)
		delay_ns = 0;

	delay_ns = delay_ns * k3g_data->time_to_read;
	k3g_data->polling_delay = ns_to_ktime(delay_ns);
}

/* gyroscope data readout */
static int k3g_read_gyro_values(struct i2c_client *client,
				struct k3g_t *data, int total_read)
{
	int err;
	struct i2c_msg msg[2];
	u8 reg_buf;
	u8 gyro_data[sizeof(*data) * (total_read ? (total_read - 1) : 1)];

	msg[0].addr = client->addr;
	msg[0].buf = &reg_buf;
	msg[0].flags = 0;
	msg[0].len = 1;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = gyro_data;

	if (total_read > 1) {
		reg_buf = AXISDATA_REG | AC;
		msg[1].len = sizeof(gyro_data);

		err = i2c_transfer(client->adapter, msg, 2);
		if (err != 2)
			return (err < 0) ? err : -EIO;
	}

	reg_buf = AXISDATA_REG;
	msg[1].len = 1;
	err = i2c_transfer(client->adapter, msg, 2);
	if (err != 2)
		return (err < 0) ? err : -EIO;

	reg_buf = OUT_Y_L | AC;
	msg[1].len = sizeof(*data);
	err = i2c_transfer(client->adapter, msg, 2);
	if (err != 2)
		return (err < 0) ? err : -EIO;

	data->y = (gyro_data[1] << 8) | gyro_data[0];
	data->z = (gyro_data[3] << 8) | gyro_data[2];
	data->x = (gyro_data[5] << 8) | gyro_data[4];

	return 0;
}

static int k3g_report_gyro_values(struct k3g_data *k3g_data)
{
	int res;
	struct k3g_t data;

	res = k3g_read_gyro_values(k3g_data->client, &data,
				k3g_data->entries + k3g_data->drop_next_event);
	if (res < 0)
		return res;

	res = k3g_read_fifo_status(k3g_data);

	k3g_data->drop_next_event = !res;

	if (res >= 31 - k3g_data->entries) {
		/* reset fifo to start again - data isn't trustworthy,
		 * our locked read might not have worked and we
		 * could have done i2c read in mid register update
		 */
		return k3g_restart_fifo(k3g_data);
	}

	data.x -= k3g_data->cal_data.x;
	data.y -= k3g_data->cal_data.y;
	data.z -= k3g_data->cal_data.z;

	input_report_rel(k3g_data->input_dev, REL_RX, data.x);
	input_report_rel(k3g_data->input_dev, REL_RY, data.y);
	input_report_rel(k3g_data->input_dev, REL_RZ, data.z);
	input_sync(k3g_data->input_dev);

//	printk(KERN_INFO "k3g report gyro value x=%d, y=%d, z=%d\n", data.x, data.y, data.z);
	return res;
}

static enum hrtimer_restart k3g_timer_func(struct hrtimer *timer)
{
	struct k3g_data *k3g_data = container_of(timer, struct k3g_data, timer);
	queue_work(k3g_data->k3g_wq, &k3g_data->work);
	return HRTIMER_NORESTART;
}

static void k3g_work_func(struct work_struct *work)
{
	int res;
	struct k3g_data *k3g_data = container_of(work, struct k3g_data, work);

	do {
		res = k3g_read_fifo_status(k3g_data);
		if (res < 0)
			return;

		if (res < k3g_data->entries) {
			pr_warn("%s: fifo entries are less than we want\n",
					__func__);

			goto timer_set;
		}

		res = k3g_report_gyro_values(k3g_data);
		if (res < 0)
			return;
timer_set:
		set_polling_delay(k3g_data, res);

	} while (!ktime_to_ns(k3g_data->polling_delay));

	hrtimer_start(&k3g_data->timer,
		k3g_data->polling_delay, HRTIMER_MODE_REL);
}

static irqreturn_t k3g_interrupt_thread(int irq, void *k3g_data_p)
{
	int res;
	struct k3g_data *k3g_data = k3g_data_p;
	res = k3g_report_gyro_values(k3g_data);
	if (res < 0)
		pr_err("%s: failed to report gyro values\n", __func__);

	return IRQ_HANDLED;
}

#ifdef DPS_FACTORY
static ssize_t k3g_selftest_dps_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct k3g_data *k3g_data  = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", k3g_data->factory_dps);
}

static ssize_t k3g_selftest_dps_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct k3g_data *k3g_data= dev_get_drvdata(dev);
	int data_buf = 0;
	int value = 0;
	int ret;

	sscanf(buf, "%d", &data_buf);
	k3g_data->factory_dps=data_buf;

	if( k3g_data->factory_dps == 250)
	{
		k3g_data->ctrl_regs[3] = (k3g_data->ctrl_regs[3] & 0xCF) | (CTRL_REG4_250DPS << CTRL_REG4_DPS_SHIFT);
		pr_err("%s: dps = %d CTRL_REG4 = 0x%x \n", __func__, k3g_data->factory_dps, k3g_data->ctrl_regs[3]);
	}
	else if( k3g_data->factory_dps == 500)
	{
		k3g_data->ctrl_regs[3] = (k3g_data->ctrl_regs[3] & 0xCF) | (CTRL_REG4_500DPS << CTRL_REG4_DPS_SHIFT);
		pr_err("%s: dps = %d CTRL_REG4 = 0x%x \n", __func__, k3g_data->factory_dps, k3g_data->ctrl_regs[3]);
	}
	else if(k3g_data->factory_dps == 2000)
	{
		k3g_data->ctrl_regs[3] = (k3g_data->ctrl_regs[3] & 0xCF) | (CTRL_REG4_2000DPS << CTRL_REG4_DPS_SHIFT);
		pr_err("%s: dps = %d CTRL_REG4 = 0x%x \n", __func__, k3g_data->factory_dps, k3g_data->ctrl_regs[3]);
	}
	else
		pr_err("%s: dps = %d not Support!! CTRL_REG4 = 0x%x \n", __func__, k3g_data->factory_dps, k3g_data->ctrl_regs[3]);

	pr_err("%s: %d dps stored\n", __func__, k3g_data->factory_dps);
	
	return count;
}
#endif

static ssize_t k3g_show_enable(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct k3g_data *k3g_data  = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", k3g_data->enable);
}

static ssize_t k3g_set_enable(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err = 0;
	struct k3g_data *k3g_data  = dev_get_drvdata(dev);
	bool new_enable;

	if (sysfs_streq(buf, "1"))
		new_enable = true;
	else if (sysfs_streq(buf, "0"))
		new_enable = false;
	else {
		pr_debug("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}

	if (new_enable == k3g_data->enable)
		return size;

	mutex_lock(&k3g_data->lock);
	if (new_enable) {
		/* Calibration for gyro - start */
		err = k3g_open_calibration(k3g_data);
		if (err < 0)
			pr_err("%s: k3g_open_calibration() failed\n",
					__func__);
		/* Calibration for gyro - end */

		/* turning on */
		err = i2c_smbus_write_i2c_block_data(k3g_data->client,
			CTRL_REG1 | AC, sizeof(k3g_data->ctrl_regs),
						k3g_data->ctrl_regs);
		if (err < 0) {
			err = -EIO;
			goto unlock;
		}

		/* reset fifo entries */
		err = k3g_restart_fifo(k3g_data);
		if (err < 0) {
			err = -EIO;
			goto turn_off;
		}

		if (k3g_data->interruptible)
			enable_irq(k3g_data->client->irq);
		else {
			set_polling_delay(k3g_data, 0);
			hrtimer_start(&k3g_data->timer,
				k3g_data->polling_delay, HRTIMER_MODE_REL);
		}
	} else {
		if (k3g_data->interruptible)
			disable_irq(k3g_data->client->irq);
		else {
			hrtimer_cancel(&k3g_data->timer);
			cancel_work_sync(&k3g_data->work);
		}
		/* turning off */
		err = i2c_smbus_write_byte_data(k3g_data->client,
						CTRL_REG1, 0x00);
		if (err < 0)
			goto unlock;
	}
	k3g_data->enable = new_enable;

turn_off:
	if (err < 0)
		i2c_smbus_write_byte_data(k3g_data->client,
						CTRL_REG1, 0x00);
unlock:
	mutex_unlock(&k3g_data->lock);

	return err ? err : size;
}

static ssize_t k3g_show_delay(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct k3g_data *k3g_data  = dev_get_drvdata(dev);
	u64 delay;

	delay = k3g_data->time_to_read * k3g_data->entries;
	delay = ktime_to_ns(ns_to_ktime(delay));

	return sprintf(buf, "%lld\n", delay);
}

static ssize_t k3g_set_delay(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct k3g_data *k3g_data  = dev_get_drvdata(dev);
	int odr_value = ODR105_BW25;
	int res = 0;
	int i;
	u64 delay_ns;
	u8 ctrl;

	res = strict_strtoll(buf, 10, &delay_ns);
	if (res < 0)
		return res;

	mutex_lock(&k3g_data->lock);
	if (!k3g_data->interruptible)
		hrtimer_cancel(&k3g_data->timer);
	else
		disable_irq(k3g_data->client->irq);

	/* round to the nearest supported ODR that is less than
	 * the requested value
	 */
	for (i = 0; i < ARRAY_SIZE(odr_delay_table); i++)
		if (delay_ns <= odr_delay_table[i].delay_ns) {
			odr_value = odr_delay_table[i].odr;
			delay_ns = odr_delay_table[i].delay_ns;
			k3g_data->time_to_read = delay_ns;
			k3g_data->entries = 1;
			break;
		}

	if (delay_ns >= odr_delay_table[1].delay_ns) {
		if (delay_ns >= MAX_DELAY) {
			k3g_data->entries = MAX_ENTRY;
			delay_ns = MAX_DELAY;
		} else {
			do_div(delay_ns, odr_delay_table[1].delay_ns);
			k3g_data->entries = delay_ns;
		}
		k3g_data->time_to_read = odr_delay_table[1].delay_ns;
	}

	if (odr_value != (k3g_data->ctrl_regs[0] & ODR_MASK)) {
		ctrl = (k3g_data->ctrl_regs[0] & ~ODR_MASK);
		ctrl |= odr_value;
		k3g_data->ctrl_regs[0] = ctrl;
		res = i2c_smbus_write_byte_data(k3g_data->client,
						CTRL_REG1, ctrl);
	}

	/* (re)start fifo */
	k3g_restart_fifo(k3g_data);

	if (!k3g_data->interruptible) {
		delay_ns = k3g_data->entries * k3g_data->time_to_read;
		k3g_data->polling_delay = ns_to_ktime(delay_ns);
		if (k3g_data->enable)
			hrtimer_start(&k3g_data->timer,
				k3g_data->polling_delay, HRTIMER_MODE_REL);
	} else
		enable_irq(k3g_data->client->irq);

	mutex_unlock(&k3g_data->lock);

	return size;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
			k3g_show_enable, k3g_set_enable);
static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
			k3g_show_delay, k3g_set_delay);


/*************************************************************************/
/* K3G Sysfs                                                             */
/*************************************************************************/

/* Device Initialization  */
static int device_init(struct k3g_data *data)
{
	int err;
	u8 buf[5];

	buf[0] = 0x6f;
	buf[1] = 0x00;
	buf[2] = 0x00;
	buf[3] = 0x00;
	buf[4] = 0x00;
	err = i2c_smbus_write_i2c_block_data(data->client,
					CTRL_REG1 | AC, sizeof(buf), buf);
	if (err < 0)
		pr_err("%s: CTRL_REGs i2c writing failed\n", __func__);

	return err;
}

/* TEST */
/* #define POWER_ON_TEST */
static ssize_t k3g_power_on(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct k3g_data *data = dev_get_drvdata(dev);
	int err;
#ifdef POWER_ON_TEST
	s16 raw[3];
	u8 gyro_data[6];
#endif

	printk("[K3G] %s, %d\n", __func__, __LINE__);
	err = device_init(data);
	if (err < 0) {
		pr_err("%s: device_init() failed\n", __func__);
		return 0;
	}

#ifdef POWER_ON_TEST /* For Test */
	err = i2c_smbus_read_i2c_block_data(data->client, AXISDATA_REG | AC,
						sizeof(gyro_data), gyro_data);
	if (err < 0) {
		pr_err("%s: CTRL_REGs i2c reading failed\n", __func__);
		return 0;
	}

	raw[0] = (gyro_data[1]) << 8 | gyro_data[0];
	raw[1] = (gyro_data[3]) << 8 | gyro_data[2];
	raw[2] = (gyro_data[5]) << 8 | gyro_data[4];

	printk(KERN_INFO "[gyro_self_test] raw[0] = %d\n", raw[0]);
	printk(KERN_INFO "[gyro_self_test] raw[1] = %d\n", raw[1]);
	printk(KERN_INFO "[gyro_self_test] raw[2] = %d\n\n", raw[2]);
#endif

	printk(KERN_INFO "[%s] result of device init = %d\n", __func__, err);

	return sprintf(buf, "%d\n", (err < 0 ? 0 : 1));
}

static ssize_t k3g_get_temp(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct k3g_data *data = dev_get_drvdata(dev);
	char temp;

	temp = i2c_smbus_read_byte_data(data->client, OUT_TEMP);
	if (temp < 0) {
		pr_err("%s: STATUS_REGS i2c reading failed\n", __func__);
		return 0;
	}

	printk(KERN_INFO "[%s] read temperature : %d\n", __func__, temp);

	return sprintf(buf, "%d\n", temp);
}

static ssize_t k3g_self_test(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct k3g_data *data = dev_get_drvdata(dev);
	int NOST[3], ST[3];
	int differ_x = 0, differ_y = 0, differ_z = 0;
	int err;
	int i, j, k;
	int res = 0;
	int fifo_ready_status;
	int fifo_ctrl_reg_backup;
	int retry = 10;
	s16 raw[3];
	s16 fifo_raw[3*32]={0,};
	s16 sum_raw[3]={0,};
	s16 fifo_data[3*32]={0,};
	s16 zero_rate_data[3]={0,};
	s16 zero_rate_data_2nd[3]={0,};
	int zero_rate_check = 0;
	u8 gyro_fifo_test_data[8][4*6]={0,};
	u8 temp = 0;
	u8 gyro_data[6];
	u8 backup_regs[5];
	u8 reg[5];
	u8 bZYXDA = 0;
	u8 pass = 2;
	u8 cal_pass = 0;
	u8 zero_rate_test= 0;
	struct file *cal_filp = NULL;
	mm_segment_t old_fs;

	memset(NOST, 0, sizeof(NOST));
	memset(ST, 0, sizeof(ST));

	/* before starting self-test, backup register */
	for (i = 0; i < 10; i++) {
		err = i2c_smbus_read_i2c_block_data(data->client, CTRL_REG1 | AC, sizeof(backup_regs), backup_regs);
		if (err >= 0)
			break;
	}
	if (err < 0) {
		pr_err("%s: CTRL_REGs i2c reading failed\n", __func__);
		goto exit;
	}

	for (i = 0; i < 5; i++)
		printk(KERN_INFO "[gyro_self_test] " "backup reg[%d] = %2x\n", i, backup_regs[i]);

	/* Initialize Sensor, turn on sensor, enable P/R/Y */
	/*CTRL_REG1= 0x6F (210Hz with Fc=50Hz, normal mode)
	  CTRL_REG2= 0x00
	  CTRL_REG3= 0x04 (Setting I2_WTM to 1)
	  CTRL_REG4= 0x90 (BDU enable, FS=500dps)
	  CTRL_REG5= 0x40 (FIFO enable)
	  FIFO_CTRL_REG= 0x20|0x1F (FIFO MODE, WTM=0x1F)*/

	reg[0] = 0x6f;
	reg[1] = 0x00;
	reg[2] = 0x00;
	reg[3] = 0x90;
	reg[4] = 0x40;

	for (i = 0; i < 10; i++) {
		err = i2c_smbus_write_i2c_block_data(data->client,
				CTRL_REG1 | AC,	sizeof(reg), reg);
		if (err >= 0)
			break;
	}
	if (err < 0) {
		pr_err("%s: CTRL_REGs i2c writing failed\n", __func__);
		goto exit;
	}

	fifo_ctrl_reg_backup = i2c_smbus_read_byte_data(data->client, FIFO_CTRL_REG);
	printk(KERN_INFO "[FIFO_SELFTEST] FIFO_CTRL_REG = %d\n", fifo_ctrl_reg_backup);
	if (err < 0) {
		pr_err("%s : failed to read FIFO_CTRL_REG\n", __func__);
		goto exit;
	}

	msleep(800);

check_zero_rate_again:

	for (i = 0; i < 10; i++) {
		err = i2c_smbus_write_byte_data(data->client,
				FIFO_CTRL_REG, BYPASS_MODE);
		if (err >= 0)
			break;
	}

	if (err < 0) {
		pr_err("%s : failed to set bypass_mode\n", __func__);
		goto exit;
	}

	for (i = 0; i < 10; i++) {
		err = i2c_smbus_write_byte_data(data->client, FIFO_CTRL_REG, FIFO_SELFTEST_MODE);
		if (err >= 0)
			break;
	}

	if (res < 0) {
		pr_err("%s : failed to set FIFO_SELFTEST_MODE\n", __func__);
		goto exit;
	}

	while(retry--){
		fifo_ready_status = i2c_smbus_read_byte_data(data->client, FIFO_SRC_REG);
		if (fifo_ready_status < 0) {
			pr_err("%s: failed to read FIFO_SRC_REG\n",
					__func__);
			goto exit;
		}
		fifo_ready_status=(fifo_ready_status & WTM_MASK);
		printk(KERN_INFO "[FIFO_SELFTEST] fifo_ready_status = %d\n", fifo_ready_status);
		if(fifo_ready_status==0x80)
			break;
		msleep(20);
	}

	for (k = 0; k < 8; k++) {//fifo ¸Þ¸ð¸® read 8È¸ ¹Ýº¹
		for (j = 0; j < 10; j++) {//reg_read_ fail½Ã  retry 10È¸
			err = i2c_smbus_read_i2c_block_data(data->client,
					AXISDATA_REG | AC,
					sizeof(gyro_fifo_test_data[k]), gyro_fifo_test_data[k]);
			if (err >= 0)
				break;
		}
		if (err < 0) {
			pr_err("%s: CTRL_REGs i2c reading failed\n", __func__);
			goto exit;
		}
	}

	/* print out fifo data */
	for (k = 0; k< 8; k++){
		for(i = 0; i<4; i++){
			fifo_raw[4*k+i]   = (gyro_fifo_test_data[k][6*i+1] << 8) | gyro_fifo_test_data[k][6*i];
			fifo_raw[4*k+i+1] = (gyro_fifo_test_data[k][6*i+3] << 8) | gyro_fifo_test_data[k][6*i+2];
			fifo_raw[4*k+i+2] = (gyro_fifo_test_data[k][6*i+5] << 8) | gyro_fifo_test_data[k][6*i+4];

			sum_raw[0] += fifo_raw[4*k+i];
			sum_raw[1] += fifo_raw[4*k+i+1];
			sum_raw[2] += fifo_raw[4*k+i+2];

			fifo_data[4*k+i]   = fifo_raw[4*k+i]*175/10000;
			fifo_data[4*k+i+1] = fifo_raw[4*k+i+1]*175/10000;
			fifo_data[4*k+i+2] = fifo_raw[4*k+i+2]*175/10000;

			zero_rate_data[0]=fifo_data[4*k+i];
			zero_rate_data[1]=fifo_data[4*k+i+1];
			zero_rate_data[2]=fifo_data[4*k+i+2];

			printk(KERN_INFO "[GYRO] %2dth: %8d %8d %8d\n", 4*k+i, fifo_data[4*k+i], fifo_data[4*k+i+1], fifo_data[4*k+i+2]);

			if ((fifo_data[4*k+i] < MIN_ZERO_RATE || fifo_data[4*k+i] > MAX_ZERO_RATE)
					||(fifo_data[4*k+i+1] < MIN_ZERO_RATE || fifo_data[4*k+i+1] > MAX_ZERO_RATE)
					||(fifo_data[4*k+i+2] < MIN_ZERO_RATE || fifo_data[4*k+i+2] > MAX_ZERO_RATE))
			{
				zero_rate_test = 0;
				pass=0;
				goto exit;	
			}
		}
	}
	
	if ( zero_rate_check ){ /* check zero_rate second time, check the value */
		zero_rate_data_2nd[0] -= zero_rate_data[0];
		zero_rate_data_2nd[1] -= zero_rate_data[1];
		zero_rate_data_2nd[2] -= zero_rate_data[2];
		printk(KERN_INFO "[GYRO] zero rate second: %8d %8d %8d\n", 
				zero_rate_data[0],zero_rate_data[1],zero_rate_data[2]);
		printk(KERN_INFO "[GYRO] zero rate result: %8d %8d %8d\n", 
				zero_rate_data_2nd[0],zero_rate_data_2nd[1],zero_rate_data_2nd[2]);

		if((-5 < zero_rate_data_2nd[0] && zero_rate_data_2nd[0] < 5 ) &&
				(-5 < zero_rate_data_2nd[1] && zero_rate_data_2nd[1] < 5 ) &&
				(-5 < zero_rate_data_2nd[2] && zero_rate_data_2nd[2] < 5 )) {
			
			data->cal_data.x = sum_raw[0] / CALIBRATION_DATA_AMOUNT;
			data->cal_data.y = sum_raw[1] / CALIBRATION_DATA_AMOUNT;
			data->cal_data.z = sum_raw[2] / CALIBRATION_DATA_AMOUNT;
			printk(KERN_INFO "%s: cal data (%d,%d,%d)\n", __func__,
					data->cal_data.x, data->cal_data.y, data->cal_data.z);

			/* Calibration for gyro - start */
			old_fs = get_fs();
			set_fs(KERNEL_DS);

			cal_filp = filp_open(CALIBRATION_FILE_PATH,
					O_CREAT | O_TRUNC | O_WRONLY, 0666);
			if (IS_ERR(cal_filp)) {
				pr_err("%s: Can't open calibration file\n", __func__);
				set_fs(old_fs);
				err = PTR_ERR(cal_filp);
				return err;
			}

			err = cal_filp->f_op->write(cal_filp,
					(char *)&data->cal_data, 3 * sizeof(s16), &cal_filp->f_pos);
			if (err != 3 * sizeof(s16)) {
				pr_err("%s: Can't write the cal data to file\n", __func__);
				err = -EIO;
			}

			filp_close(cal_filp, current->files);
			set_fs(old_fs);
			cal_pass = 1; /* calibration pass */
			/* Calibration for gyro - end */
		}

	} else { 				/* check zero_rate first time, go to check again */
		zero_rate_check = 1;
		sum_raw[0] = 0;
		sum_raw[1] = 0;
		sum_raw[2] = 0;
		zero_rate_data_2nd[0] = zero_rate_data[0];
		zero_rate_data_2nd[1] = zero_rate_data[1];
		zero_rate_data_2nd[2] = zero_rate_data[2];
		printk(KERN_INFO "[GYRO] zero rate first: %8d %8d %8d\n", 
				zero_rate_data[0],zero_rate_data[1],zero_rate_data[2]);
		goto check_zero_rate_again;
	}
	
	zero_rate_test = 1;
	printk("[FIFO_SELFTEST] zero_rate_test result =%d\n",zero_rate_test);

	res = i2c_smbus_write_byte_data(data->client, FIFO_CTRL_REG, fifo_ctrl_reg_backup);

	if (err < 0) {
		pr_err("%s: CTRL_REGs i2c writing failed\n", __func__);
		goto exit;
	}

	/* Initialize Sensor, turn on sensor, enable P/R/Y */
	/* Set BDU=1, Set ODR=200Hz, Cut-Off Frequency=50Hz, FS=2000dps */
	reg[0] = 0x6f;
	reg[1] = 0x00;
	reg[2] = 0x00;
	reg[3] = 0xA0;
	reg[4] = 0x02;

	for (i = 0; i < 10; i++) {
		err = i2c_smbus_write_i2c_block_data(data->client,
				CTRL_REG1 | AC,	sizeof(reg), reg);
		if (err >= 0)
			break;
	}
	if (err < 0) {
		pr_err("%s: CTRL_REGs i2c writing failed\n", __func__);
		goto exit;
	}
	/* Power up, wait for 800ms for stable output */
	msleep(800);

	/* Read 5 samples output before self-test on */
	for (i = 0; i < 5; i++) {

		/* check ZYXDA ready bit */
		for (j = 0; j < 10; j++) {
			temp = i2c_smbus_read_byte_data(data->client, STATUS_REG);
			if (temp >= 0) {
				bZYXDA = temp & 0x08;
				if (!bZYXDA) {
					msleep(10);
					pr_err("%s: %d,%d: no_data_ready", __func__, i, j);
					continue;
				} else
					break;
			}
		}
		if (temp < 0) {
			pr_err("%s: STATUS_REGS i2c reading failed\n", __func__);
			goto exit;
		}

		for (j = 0; j < 10; j++) {
			err = i2c_smbus_read_i2c_block_data(data->client, AXISDATA_REG | AC, sizeof(gyro_data), gyro_data);
			if (err >= 0)
				break;
		}
		if (err < 0) {
			pr_err("%s: CTRL_REGs i2c reading failed\n", __func__);
			goto exit;
		}

		raw[0] = (gyro_data[1] << 8) | gyro_data[0];
		raw[1] = (gyro_data[3] << 8) | gyro_data[2];
		raw[2] = (gyro_data[5] << 8) | gyro_data[4];

		NOST[0] += raw[0];
		NOST[1] += raw[1];
		NOST[2] += raw[2];

		printk(KERN_INFO "[gyro_self_test] raw[0] = %d\n", raw[0]);
		printk(KERN_INFO "[gyro_self_test] raw[1] = %d\n", raw[1]);
		printk(KERN_INFO "[gyro_self_test] raw[2] = %d\n", raw[2]);
	}

	for (i = 0; i < 3; i++)
		printk(KERN_INFO "[gyro_self_test] " "SUM of NOST[%d] = %d\n", i, NOST[i]);

	/* calculate average of NOST and covert from ADC to DPS */
	for (i = 0; i < 3; i++) {
		NOST[i] = (NOST[i] / 5) * 70 / 1000;
		printk(KERN_INFO "[gyro_self_test] " "AVG of NOST[%d] = %d\n", i, NOST[i]);
	}
	// printk(KERN_INFO "\n");

	/* Enable Self Test */
	reg[0] = 0xA2;
	for (i = 0; i < 10; i++) {
		err = i2c_smbus_write_byte_data(data->client, CTRL_REG4, reg[0]);
		if (err >= 0)
			break;
	}
	if (err < 0) {
		pr_err("%s: CTRL_REG4 i2c writing failed\n", __func__);
		goto exit;
	}

	msleep(100);

	/* Read 5 samples output after self-test on */
	for (i = 0; i < 5; i++) {
		/* check ZYXDA ready bit */
		for (j = 0; j < 10; j++) {
			temp = i2c_smbus_read_byte_data(data->client,
					STATUS_REG);
			if (temp >= 0) {
				bZYXDA = temp & 0x08;
				if (!bZYXDA) {
					msleep(10);
					pr_err("[GYRO] %s: %d,%d: no_data_ready\n", __func__, i, j);
					continue;
				} else
					break;
			}

		}
		if (temp < 0) {
			pr_err("[GYRO] %s: STATUS_REGS i2c reading failed\n", __func__);
			goto exit;
		}

		for (j = 0; j < 10; j++) {
			err = i2c_smbus_read_i2c_block_data(data->client, AXISDATA_REG | AC, sizeof(gyro_data), gyro_data);
			if (err >= 0)
				break;
		}
		if (err < 0) {
			pr_err("%s: CTRL_REGs i2c reading failed\n", __func__);
			goto exit;
		}

		raw[0] = (gyro_data[1] << 8) | gyro_data[0];
		raw[1] = (gyro_data[3] << 8) | gyro_data[2];
		raw[2] = (gyro_data[5] << 8) | gyro_data[4];

		ST[0] += raw[0];
		ST[1] += raw[1];
		ST[2] += raw[2];

		printk(KERN_INFO "[gyro_self_test] raw[0] = %d\n", raw[0]);
		printk(KERN_INFO "[gyro_self_test] raw[1] = %d\n", raw[1]);
		printk(KERN_INFO "[gyro_self_test] raw[2] = %d\n\n", raw[2]);
	}

	for (i = 0; i < 3; i++)
		printk(KERN_INFO "[gyro_self_test] "
				"SUM of ST[%d] = %d\n", i, ST[i]);

	/* calculate average of ST and convert from ADC to dps */
	for (i = 0; i < 3; i++)	{
		/* When FS=2000, 70 mdps/digit */
		ST[i] = (ST[i] / 5) * 70 / 1000;
		printk(KERN_INFO "[gyro_self_test] "
				"AVG of ST[%d] = %d\n", i, ST[i]);
	}

	/* check whether pass or not */
	if (ST[0] >= NOST[0]) /* for x */
		differ_x = ST[0] - NOST[0];
	else
		differ_x = NOST[0] - ST[0];

	if (ST[1] >= NOST[1]) /* for y */
		differ_y = ST[1] - NOST[1];
	else
		differ_y = NOST[1] - ST[1];

	if (ST[2] >= NOST[2]) /* for z */
		differ_z = ST[2] - NOST[2];
	else
		differ_z = NOST[2] - ST[2];

	printk(KERN_INFO "[gyro_self_test] differ x:%d, y:%d, z:%d\n",
			differ_x, differ_y, differ_z);

	if ((MIN_ST <= differ_x && differ_x <= MAX_ST)
			&& (MIN_ST <= differ_y && differ_y <= MAX_ST)
			&& (MIN_ST <= differ_z && differ_z <= MAX_ST))
		pass = 1;
	else
		pass = 0;

	/* restore backup register */
	for (i = 0; i < 10; i++) {
		err = i2c_smbus_write_i2c_block_data(data->client,
				CTRL_REG1 | AC, sizeof(backup_regs), backup_regs);
		if (err >= 0)
			break;
	}
	if (err < 0)
		pr_err("%s: CTRL_REGs i2c writing failed\n", __func__);

exit:
	if (pass == 2)
		printk(KERN_INFO "[gyro_self_test] self-test result : retry\n");
	else
		printk(KERN_INFO "[gyro_self_test] self-test result : %s\n",
				pass ? "pass" : "fail");

	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
			NOST[0], NOST[1], NOST[2], ST[0], ST[1], ST[2], pass,
			zero_rate_test,zero_rate_data[0],zero_rate_data[1],zero_rate_data[2],cal_pass);

}

static DEVICE_ATTR(power_on, 0664,
	k3g_power_on, NULL);
static DEVICE_ATTR(temperature, 0664,
	k3g_get_temp, NULL);
static DEVICE_ATTR(selftest, 0664,
	k3g_self_test, NULL);
#ifdef DPS_FACTORY
static DEVICE_ATTR(selftest_dps, 0664,
	k3g_selftest_dps_show, k3g_selftest_dps_store);
#endif

static struct device_attribute *gyro_sensor_attrs[] = {
	&dev_attr_power_on,
	&dev_attr_temperature,
	&dev_attr_selftest,
#ifdef DPS_FACTORY
	&dev_attr_selftest_dps,
#endif
	NULL,
};

static const struct file_operations k3g_fops = {
	.owner = THIS_MODULE,
};

/*************************************************************************/
/* End of K3G Sysfs                                                      */
/*************************************************************************/


static int k3g_probe(struct i2c_client *client,
			       const struct i2c_device_id *devid)
{
	int ret;
	int err = 0;
	struct k3g_data *data;
	struct input_dev *input_dev;
	struct k3g_platform_data *pdata = client->dev.platform_data;
	printk("[K3G] %s, %d - start\n", __func__,__LINE__);
	
	if (!pdata) {
		pr_err("%s: missing pdata!\n", __func__);
		return -ENODEV;
	}


	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		err = -ENOMEM;
		goto exit;
	}

	if(pdata->power_on)
		data->power_on = pdata->power_on;
	if(pdata->power_off)
		data->power_off = pdata->power_off;

	if(data->power_on)
		data->power_on();

	data->client = client;
#ifdef DPS_FACTORY
	data->factory_dps = 500;//defualt DPS
#endif

	/* read chip id */
	ret = i2c_smbus_read_byte_data(client, WHO_AM_I);
	if (ret != DEVICE_ID) {
		if (ret < 0) {
			pr_err("%s: i2c for reading chip id failed\n",
								__func__);
			err = ret;
		} else {
			pr_err("%s : Device identification failed\n",
								__func__);
			err = -ENODEV;
		}
		goto err_read_reg;
	}

	mutex_init(&data->lock);

	/* allocate gyro input_device */
	input_dev = input_allocate_device();
	if (!input_dev) {
		pr_err("%s: could not allocate input device\n", __func__);
		err = -ENOMEM;
		goto err_input_allocate_device;
	}

	data->input_dev = input_dev;
	input_set_drvdata(input_dev, data);
	input_dev->name = "gyro_sensor";
	/* X */
	input_set_capability(input_dev, EV_REL, REL_RX);
	input_set_abs_params(input_dev, REL_RX, -2048, 2047, 0, 0);
	/* Y */
	input_set_capability(input_dev, EV_REL, REL_RY);
	input_set_abs_params(input_dev, REL_RY, -2048, 2047, 0, 0);
	/* Z */
	input_set_capability(input_dev, EV_REL, REL_RZ);
	input_set_abs_params(input_dev, REL_RZ, -2048, 2047, 0, 0);

	err = input_register_device(input_dev);
	if (err < 0) {
		pr_err("%s: could not register input device\n", __func__);
		input_free_device(data->input_dev);
		goto err_input_register_device;
	}

	memcpy(&data->ctrl_regs, &default_ctrl_regs, sizeof(default_ctrl_regs));

	data->client->irq=pdata->get_irq();
	printk("[K3G] %s: get irq=%d\n", __func__, data->client->irq);
	
	if ( data->client->irq >= 0) { /* interrupt */
		/* added for Qualcomm Board - start */
		int irq;
		err = gpio_request(data->client->irq, "gyro_fifo_int");
		if (err < 0) {
			pr_err("%s: gpio %d request failed (%d)\n",
					__func__, data->client->irq, err);
			goto err_request_irq;
		}

		err = gpio_direction_input(data->client->irq);
		if (err < 0) {
			pr_err("%s: failed to set gpio %d as input (%d)\n",
					__func__, data->client->irq, err);
			goto err_request_irq;
		}

		irq = MSM_GPIO_TO_INT(data->client->irq);
		data->client->irq = irq;

		/* added for Qualcomm Board - end */
		data->interruptible = true;
		err = request_threaded_irq(irq, NULL,
				k3g_interrupt_thread, IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				"k3g", data);
		if (err < 0) {
			pr_err("[K3G] %s: can't allocate irq.\n", __func__);
			goto err_request_irq;
		}
		else {
			pr_err("[K3G] %s: allocate irq=%d\n", __func__, irq);
		}
		
		disable_irq(irq);

	} else { /* polling */
		u64 delay_ns;
		data->ctrl_regs[2] = 0x00; /* disable interrupt */
		/* hrtimer settings.  we poll for gyro values using a timer. */
		hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		data->polling_delay = ns_to_ktime(200 * NSEC_PER_MSEC);
		data->time_to_read = 10000000LL;
		delay_ns = ktime_to_ns(data->polling_delay);
		do_div(delay_ns, data->time_to_read);
		data->entries = delay_ns;
		data->timer.function = k3g_timer_func;

		/* the timer just fires off a work queue request.
		   We need a thread to read i2c (can be slow and blocking). */
		data->k3g_wq = create_singlethread_workqueue("k3g_wq");
		if (!data->k3g_wq) {
			err = -ENOMEM;
			pr_err("%s: could not create workqueue\n", __func__);
			goto err_create_workqueue;
		}
		else {
			pr_err("[K3G] %s: create workqueue for polling\n", __func__);
		}
		/* this is the thread function we run on the work queue */
		INIT_WORK(&data->work, k3g_work_func);
	}

	if (device_create_file(&input_dev->dev,
				&dev_attr_enable) < 0) {
		pr_err("%s: Failed to create device file(%s)!\n", __func__,
				dev_attr_enable.attr.name);
		goto err_device_create_file;
	}

	if (device_create_file(&input_dev->dev,
				&dev_attr_poll_delay) < 0) {
		pr_err("%s: Failed to create device file(%s)!\n", __func__,
				dev_attr_poll_delay.attr.name);
		goto err_device_create_file2;
	}

	i2c_set_clientdata(client, data);
	dev_set_drvdata(&input_dev->dev, data);

	/* register a char dev */
	err = register_chrdev(K3G_MAJOR, "k3g", &k3g_fops);
	if (err < 0) {
		pr_err("%s: Failed to register chrdev(k3g)\n", __func__);
		goto err_register_chrdev;
	}

	/* set sysfs for light sensor test mode*/
	err = sensors_register(gyro_sensor_device, data, gyro_sensor_attrs, "gyro_sensor");
	if(err) {
		printk(KERN_ERR "%s: cound not register gyro sensor device(%d).\n", __func__, err);
	}

/*	dev_set_drvdata(data->dev, data);*/

	return 0;

err_register_chrdev:
	device_remove_file(&input_dev->dev, &dev_attr_poll_delay);
err_device_create_file2:
	device_remove_file(&input_dev->dev, &dev_attr_enable);
err_device_create_file:
	if (data->interruptible) {
		enable_irq(data->client->irq);
		free_irq(data->client->irq, data);
	} else
		destroy_workqueue(data->k3g_wq);
	input_unregister_device(data->input_dev);
err_create_workqueue:
err_request_irq:
err_input_register_device:
err_input_allocate_device:
	mutex_destroy(&data->lock);
err_read_reg:
	kfree(data);
exit:
	return err;
}

static int k3g_remove(struct i2c_client *client)
{
	int err = 0;
	struct k3g_data *k3g_data = i2c_get_clientdata(client);
#if 0//This will be removed
	device_remove_file(k3g_data->dev, &dev_attr_gyro_get_temp);
	device_remove_file(k3g_data->dev, &dev_attr_gyro_power_on);
	device_destroy(k3g_data->k3g_gyro_dev_class, MKDEV(K3G_MAJOR, 0));
	class_destroy(k3g_data->k3g_gyro_dev_class);
#endif
	unregister_chrdev(K3G_MAJOR, "k3g");
	device_remove_file(&k3g_data->input_dev->dev, &dev_attr_enable);
	device_remove_file(&k3g_data->input_dev->dev, &dev_attr_poll_delay);

	if (k3g_data->enable)
		err = i2c_smbus_write_byte_data(k3g_data->client,
					CTRL_REG1, 0x00);
	if (k3g_data->interruptible) {
		if (!k3g_data->enable) /* no disable_irq before free_irq */
			enable_irq(k3g_data->client->irq);
		free_irq(k3g_data->client->irq, k3g_data);

	} else {
		hrtimer_cancel(&k3g_data->timer);
		cancel_work_sync(&k3g_data->work);
		destroy_workqueue(k3g_data->k3g_wq);
	}

	input_unregister_device(k3g_data->input_dev);
	mutex_destroy(&k3g_data->lock);
	kfree(k3g_data);

	return err;
}

static int k3g_suspend(struct device *dev)
{
	int err = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct k3g_data *k3g_data = i2c_get_clientdata(client);

	if (k3g_data->enable) {
		mutex_lock(&k3g_data->lock);
		if (!k3g_data->interruptible) {
			hrtimer_cancel(&k3g_data->timer);
			cancel_work_sync(&k3g_data->work);
		}
		err = i2c_smbus_write_byte_data(k3g_data->client,
						CTRL_REG1, 0x00);
		mutex_unlock(&k3g_data->lock);
		if(err)
			goto err_k3g_suspend;
	}

	if(k3g_data->power_off){
		err = k3g_data->power_off();
		if(err)
			goto err_k3g_suspend;
	}

	return err;

err_k3g_suspend:	
	printk("[K3G] %s,%d, error=%d\n",__func__,__LINE__,err );
	return err;
}

static int k3g_resume(struct device *dev)
{
	int err = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct k3g_data *k3g_data = i2c_get_clientdata(client);

	if(k3g_data->power_on){
		err = k3g_data->power_on();
		if(err){
			printk("[K3G] %s,%d, error=%d\n",__func__,__LINE__,err );
			goto err_k3g_resume;
		}
	}

	if (k3g_data->enable) {
		mutex_lock(&k3g_data->lock);
		if (!k3g_data->interruptible)
			hrtimer_start(&k3g_data->timer,
				k3g_data->polling_delay, HRTIMER_MODE_REL);
		err = i2c_smbus_write_i2c_block_data(client,
				CTRL_REG1 | AC, sizeof(k3g_data->ctrl_regs),
							k3g_data->ctrl_regs);
		mutex_unlock(&k3g_data->lock);
		if(err){
			printk("[K3G] %s,%d, error=%d\n",__func__,__LINE__,err );
			goto err_k3g_resume;
		}
	}

	return err;

err_k3g_resume:	
	printk("[K3G] %s,%d, error=%d\n",__func__,__LINE__,err );
	return err;

}

static const struct dev_pm_ops k3g_pm_ops = {
	.suspend = k3g_suspend,
	.resume = k3g_resume
};

static const struct i2c_device_id k3g_id[] = {
	{ "k3g", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, k3g_id);

static struct i2c_driver k3g_driver = {
	.probe = k3g_probe,
	.remove = __devexit_p(k3g_remove),
	.id_table = k3g_id,
	.driver = {
		.pm = &k3g_pm_ops,
		.owner = THIS_MODULE,
		.name = "k3g"
	},
};

static int __init k3g_init(void)
{
	return i2c_add_driver(&k3g_driver);
}

static void __exit k3g_exit(void)
{
	i2c_del_driver(&k3g_driver);
}

module_init(k3g_init);
module_exit(k3g_exit);

MODULE_DESCRIPTION("k3g digital gyroscope driver");
MODULE_AUTHOR("tim.sk.lee@samsung.com");
MODULE_LICENSE("GPL");
