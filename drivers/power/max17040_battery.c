/*
 *  max17040_battery.c
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 *
 *  Copyright (C) 2009 Samsung Electronics
 *  Minkyu Kang <mk7.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/max17040_battery.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#ifdef CONFIG_SEC_DEBUG_FUELGAUGE_LOG
#include <mach/sec_debug.h>
#endif

#ifdef CONFIG_USA_MODEL_SGH_I717
#define FG_MAX17048_ENABLED
#define ADJUST_RCOMP_WITH_CHARGING_STATUS
#endif

#define MAX17040_VCELL_MSB	0x02
#define MAX17040_VCELL_LSB	0x03
#define MAX17040_SOC_MSB	0x04
#define MAX17040_SOC_LSB	0x05
#define MAX17040_MODE_MSB	0x06
#define MAX17040_MODE_LSB	0x07
#define MAX17040_VER_MSB	0x08
#define MAX17040_VER_LSB	0x09
#define MAX17040_RCOMP_MSB	0x0C
#define MAX17040_RCOMP_LSB	0x0D
#define MAX17040_CMD_MSB	0xFE
#define MAX17040_CMD_LSB	0xFF

#define MAX17040_LONG_DELAY		5000 /* msec */
#define LOG_DELTA_VOLTAGE	(150 * 1000) /* 150 mV */
#define MAX17040_FAST_DELAY		500 /* msec */
#define FAST_LOG_COUNT		60	/* 30 sec */

/* fuelgauge tuning */
/* SOC accuracy depends on RCOMP and Adjusted SOC Method(below values) */
/* you should fix these values for your MODEL */
#if defined(CONFIG_KOR_MODEL_SHV_E110S)
#define EMPTY_COND_SOC 		100
#define EMPTY_SOC 			20
//#define FULL_SOC		9450
#define FULL_SOC_DEFAULT	9350
#define FULL_SOC_LOW		9250
#define FULL_SOC_HIGH		9680
#define FULL_KEEP_SOC		30
#elif defined(CONFIG_KOR_MODEL_SHV_E120S) ||  defined(CONFIG_KOR_MODEL_SHV_E120K)
#define EMPTY_COND_SOC 		100
#define EMPTY_SOC 			20
//#define FULL_SOC		9450
#define FULL_SOC_DEFAULT	9350
#define FULL_SOC_LOW		9250
#define FULL_SOC_HIGH		9680
#define FULL_KEEP_SOC		30
#elif defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
#define EMPTY_COND_SOC 		100
#define EMPTY_SOC 			20
//#define FULL_SOC		9450
#define FULL_SOC_DEFAULT	9450
#define FULL_SOC_LOW		9350
#define FULL_SOC_HIGH		9780
#define FULL_KEEP_SOC		30
#elif defined(CONFIG_USA_MODEL_SGH_I717)
#define EMPTY_COND_SOC		100
#define EMPTY_SOC		0
//#define FULL_SOC		9450
#define FULL_SOC_DEFAULT	9500
#define FULL_SOC_LOW		9500
#define FULL_SOC_HIGH		9530
#define FULL_KEEP_SOC		30
#define RCOMP_2ND		0xF01F
#elif defined(CONFIG_USA_MODEL_SGH_T769) || \
	defined(CONFIG_USA_MODEL_SGH_I577) || \
	defined(CONFIG_CAN_MODEL_SGH_I577R)
#define EMPTY_COND_SOC		100
#define EMPTY_SOC		20
//#define FULL_SOC		9450
#define FULL_SOC_DEFAULT	9510
#define FULL_SOC_LOW		9510
#define FULL_SOC_HIGH		9540
#define FULL_KEEP_SOC		30
#elif defined(CONFIG_USA_MODEL_SGH_T989) || \
	defined(CONFIG_USA_MODEL_SGH_I727)
#define EMPTY_COND_SOC		100
#define EMPTY_SOC		50
//#define FULL_SOC		9400
#define FULL_SOC_DEFAULT	9400
#define FULL_SOC_LOW		9400
#define FULL_SOC_HIGH		9430
#define FULL_KEEP_SOC		30

#else
#define EMPTY_COND_SOC		100
#define EMPTY_SOC		20
//#define FULL_SOC		9450
#define FULL_SOC_DEFAULT	9350
#define FULL_SOC_LOW		9250
#define FULL_SOC_HIGH		9680
#define FULL_KEEP_SOC		30
#endif

#if defined(CONFIG_KOR_MODEL_SHV_E160S) ||  defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_JPN_MODEL_SC_05D)
#define ADJUST_SOC_OFFSET
#endif

/* default disable : TBT */
/* #define ADJUST_RCOMP_WITH_TEMPER */
#define RCOMP0_TEMP 	20 /* 'C */

static ssize_t sec_fg_show_property(struct device *dev,
				    struct device_attribute *attr, char *buf);

static ssize_t sec_fg_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count);

struct max17040_chip {
	struct i2c_client		*client;
	struct delayed_work		work;
	struct power_supply		battery;
	struct max17040_platform_data	*pdata;
	struct wake_lock	lowbat_wake_lock;
	struct mutex		mutex;

	/* battery voltage */
	int vcell;
	int prevcell;
	/* normal soc (adjust) */
	int soc;
	/* raw soc */
	int raw_soc;
	/* work interval */
	unsigned int fg_interval;
	/* log count */
	unsigned int fast_log_count;
	/* current temperature */
	int temperature;
	/* current rcomp */
	u16 rcomp;
	/* new rcomp */
	u16 new_rcomp;
	/* adjust full soc */
	int full_soc;
};

#ifdef FG_MAX17048_ENABLED
static int max17048_read_word_reg(struct i2c_client *client, int reg)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	int ret = 0;

	mutex_lock(&chip->mutex);

	ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	mutex_unlock(&chip->mutex);

	return ret;
}
#endif


static int max17040_write_reg(struct i2c_client *client, int reg, u8 value)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mutex);
	
	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	mutex_unlock(&chip->mutex);
	
	return ret;
}

static int max17040_read_reg(struct i2c_client *client, int reg)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mutex);
	
	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	mutex_unlock(&chip->mutex);
	
	return ret;
}

static void max17040_get_vcell(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	u8 msb;
	u8 lsb;

#ifdef FG_MAX17048_ENABLED
	int data;

	data = max17048_read_word_reg(client, MAX17040_VCELL_MSB);
	msb = data & 0xff;
	lsb = (data >> 8) & 0xff;
#else
	msb = max17040_read_reg(client, MAX17040_VCELL_MSB);
	lsb = max17040_read_reg(client, MAX17040_VCELL_LSB);
#endif
	chip->prevcell = chip->vcell;
	chip->vcell = ((msb << 4) + (lsb >> 4)) * 1250;
	if (chip->prevcell == 0)
		chip->prevcell = chip->vcell;
}

static void max17040_get_soc(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	u8 msb;
	u8 lsb;

	unsigned int soc, psoc;
	int temp_soc;
	static int fg_zcount = 0;

#ifdef FG_MAX17048_ENABLED
	int data;

	data = max17048_read_word_reg(client, MAX17040_SOC_MSB);
	msb = data & 0xff;
	lsb = (data >> 8) & 0xff;
#else
	msb = max17040_read_reg(client, MAX17040_SOC_MSB);
	lsb = max17040_read_reg(client, MAX17040_SOC_LSB);
#endif
	psoc = msb * 100 + (lsb * 100) / 256;
	chip->raw_soc = psoc;

	if(psoc > EMPTY_COND_SOC) {
		//temp_soc = ((psoc - EMPTY_SOC)*10000)/(FULL_SOC - EMPTY_SOC);
		temp_soc = ((psoc - EMPTY_SOC)*10000)/(chip->full_soc - EMPTY_SOC);

#ifdef ADJUST_SOC_OFFSET
		if (temp_soc < 2100)
			temp_soc = temp_soc;
		else if (temp_soc < 3100)
			temp_soc = temp_soc - ((temp_soc * 15) / 100 - 300);
		else if (temp_soc < 7100)
			temp_soc = temp_soc - 150;
		else if (temp_soc < 8100)
			temp_soc = temp_soc - ((temp_soc * -15)/100 + 1200);
		else
			temp_soc = temp_soc;
#endif

	} else
		temp_soc = 0;
	
	soc = temp_soc/100;
	soc = min(soc, (uint)100);

	if(soc == 0) {
		fg_zcount++;
		if(fg_zcount >= 3) {
			printk("[fg] real 0%%\n");
			soc = 0;
			fg_zcount = 0;
		} else
			soc = chip->soc;
	} else
		fg_zcount = 0;

	/*
	temp_soc = psoc;
	soc = temp_soc/100;
	soc = min(soc, (uint)100);
	*/

	chip->soc = soc;

	//printk("%s: SOC = %d, RAW_SOC = %d\n", __func__, chip->soc, chip->raw_soc);
}

static void max17040_get_version(struct i2c_client *client)
{
	u8 msb;
	u8 lsb;

	printk("%s : \n", __func__);

#ifdef FG_MAX17048_ENABLED
	int data;

	data = max17048_read_word_reg(client, MAX17040_VER_MSB);
	msb = data & 0xff;
	lsb = (data >> 8) & 0xff;
#else
	msb = max17040_read_reg(client, MAX17040_VER_MSB);
	lsb = max17040_read_reg(client, MAX17040_VER_LSB);
#endif
	dev_info(&client->dev, "MAX17040 Fuel-Gauge Ver %d%d\n", msb, lsb);
}

static u16 max17040_get_rcomp(struct i2c_client *client)
{
	u8 msb;
	u8 lsb;
	u16 ret = 0;

	//printk("%s : \n", __func__);
#ifdef FG_MAX17048_ENABLED
	int data;

	data = max17048_read_word_reg(client, MAX17040_RCOMP_MSB);
	msb = data & 0xff;
	lsb = (data >> 8) & 0xff;
#else
	msb = max17040_read_reg(client, MAX17040_RCOMP_MSB);
	lsb = max17040_read_reg(client, MAX17040_RCOMP_LSB);
#endif

	ret = (u16)(msb<<8 | lsb);
	//pr_info("MAX17040 Fuel-Gauge RCOMP 0x%x%x\n", msb, lsb);
	pr_info("%s : current rcomp = 0x%x(%d)\n", __func__, ret, msb);

	return ret;
}

static void max17040_set_rcomp(struct i2c_client *client, u16 new_rcomp)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	
	mutex_lock(&chip->mutex);
	
	//printk("%s : \n", __func__);
	//pr_info("%s : new rcomp = 0x%x(%d)\n", __func__,
	//				new_rcomp, new_rcomp>>8);
	
	i2c_smbus_write_word_data(client, MAX17040_RCOMP_MSB,
							swab16(new_rcomp));

	mutex_unlock(&chip->mutex);
}

static void max17040_reset(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

	pr_info("%s :\n", __func__);

	/* POR : CMD,5400h */
	max17040_write_reg(client, MAX17040_CMD_MSB, 0x54);
	max17040_write_reg(client, MAX17040_CMD_LSB, 0x00);
	msleep(300);

	max17040_set_rcomp(client, chip->new_rcomp);
	chip->rcomp = max17040_get_rcomp(client);

	/* Quick Start : MODE,4000h : TBT */
	/*
	max17040_write_reg(client, MAX17040_MODE_MSB, 0x40);
	max17040_write_reg(client, MAX17040_MODE_LSB, 0x00);
	*/
}

static void max17040_adjust_fullsoc(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	int prev_full_soc = chip->full_soc;
	int temp_soc = chip->raw_soc;

	if (temp_soc < FULL_SOC_LOW) {
		chip->full_soc = FULL_SOC_LOW;
	} else if(temp_soc > FULL_SOC_HIGH) {
		chip->full_soc = (FULL_SOC_HIGH - FULL_KEEP_SOC);
	} else {
		if (temp_soc > (FULL_SOC_LOW + FULL_KEEP_SOC)) {
			chip->full_soc = temp_soc - FULL_KEEP_SOC;
		} else {
			chip->full_soc = FULL_SOC_LOW;
		}
	}

	if (prev_full_soc != chip->full_soc)
		pr_info("%s : full_soc = %d\n", __func__, chip->full_soc);
}

static void max17040_work(struct work_struct *work)
{
	struct max17040_chip *chip;

	chip = container_of(work, struct max17040_chip, work.work);

	max17040_get_vcell(chip->client);
	max17040_get_soc(chip->client);

	//printk("%s : %d, %d \n", __func__, chip->vcell, chip->soc);
	
	/* DEBUG : for RAM log */
#ifdef CONFIG_SEC_DEBUG_FUELGAUGE_LOG
	sec_debug_fuelgauge_log(chip->vcell, (unsigned short)chip->soc, 0);
	if ((chip->fg_interval != MAX17040_FAST_DELAY) &&
		((chip->prevcell > (chip->vcell + LOG_DELTA_VOLTAGE)) ||
		(chip->vcell > (chip->prevcell + LOG_DELTA_VOLTAGE)))) {
		printk("%s : fast ram logging\n", __func__);
		chip->fg_interval = MAX17040_FAST_DELAY;
		chip->fast_log_count = FAST_LOG_COUNT;
	} else {
		if (chip->fast_log_count > 0) {
			chip->fast_log_count--;
		} else {
			if (chip->fast_log_count != 0)
				chip->fast_log_count = 0;
			if (chip->fg_interval != MAX17040_LONG_DELAY)
				chip->fg_interval = MAX17040_LONG_DELAY;
		}
	}
#endif

#ifdef ADJUST_RCOMP_WITH_TEMPER
	/* rcomp update */
	if (chip->rcomp != chip->new_rcomp) {
		max17040_set_rcomp(chip->client, chip->new_rcomp);
		chip->rcomp = max17040_get_rcomp(chip->client);
	}
#endif

	schedule_delayed_work(&chip->work, msecs_to_jiffies(chip->fg_interval));
}

static enum power_supply_property max17040_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
};

#define SEC_FG_ATTR(_name)			\
{						            \
	.attr = { .name = #_name,		\
		  .mode = 0664 },	\
	.show = sec_fg_show_property,	\
	.store = sec_fg_store,			\
}

static struct device_attribute sec_fg_attrs[] = {
	SEC_FG_ATTR(fg_reset_soc),
	SEC_FG_ATTR(fg_read_soc),
	SEC_FG_ATTR(fg_read_rcomp),
	SEC_FG_ATTR(fg_read_fsoc),
};

enum {
	FG_RESET_SOC = 0,
	FG_READ_SOC,
	FG_READ_RCOMP,
	FG_READ_FSOC,
};

static ssize_t sec_fg_show_property(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max17040_chip *chip = container_of(psy,
						  struct max17040_chip,
						  battery);

	int i = 0;
	const ptrdiff_t off = attr - sec_fg_attrs;

	switch (off) {
	case FG_READ_SOC:
		max17040_get_soc(chip->client);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			chip->soc);
		break;
	case FG_READ_RCOMP:
		chip->rcomp = max17040_get_rcomp(chip->client);
		i += scnprintf(buf + i, PAGE_SIZE - i, "0x%04x\n",
			chip->rcomp);
		break;
	case FG_READ_FSOC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			chip->full_soc);
		break;
	default:
		i = -EINVAL;
	}

	return i;
}

static ssize_t sec_fg_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max17040_chip *chip = container_of(psy,
						  struct max17040_chip,
						  battery);

	int x = 0;
	int ret = 0;
	const ptrdiff_t off = attr - sec_fg_attrs;

	switch (off) {
	case FG_RESET_SOC:
		if (sscanf(buf, "%d\n", &x) == 1) {
			if (x == 1)
				max17040_reset(chip->client);
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int fuelgauge_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(sec_fg_attrs); i++) {
		rc = device_create_file(dev, &sec_fg_attrs[i]);
		if (rc)
			goto fg_attrs_failed;
	}
	goto succeed;

fg_attrs_failed:
	while (i--)
		device_remove_file(dev, &sec_fg_attrs[i]);
succeed:
	return rc;
}

static irqreturn_t max17040_int_work_func(int irq, void *max_chip)
{
	struct max17040_chip *chip = max_chip;

	printk("%s\n", __func__);
	
	wake_lock_timeout(&chip->lowbat_wake_lock, 20 * HZ);
    max17040_get_soc(chip->client);
	max17040_get_soc(chip->client);
	max17040_get_soc(chip->client);
	max17040_get_vcell(chip->client);

	if(chip->pdata->low_batt_cb)
		chip->pdata->low_batt_cb();

	return IRQ_HANDLED;
}

#ifdef ADJUST_RCOMP_WITH_TEMPER
static void max17040_rcomp_update(struct i2c_client *client, int temp)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);
	int rcomp0 = 0;
	int new_rcomp = 0;
	
	rcomp0 = (int)(chip->pdata->rcomp_value >> 8);
	rcomp0 *= 10;
	
	if (temp < RCOMP0_TEMP) {
		new_rcomp = (rcomp0 + 50*(RCOMP0_TEMP-temp))/10;
		new_rcomp = max(new_rcomp, 0);
		new_rcomp = min(new_rcomp, 255);
		chip->new_rcomp = ((u8)new_rcomp << 8) | (chip->rcomp&0xFF);
	} else if (temp > RCOMP0_TEMP) {
		new_rcomp = (rcomp0 - 18*(temp-RCOMP0_TEMP))/10;
		new_rcomp = max(new_rcomp, 0);
		new_rcomp = min(new_rcomp, 255);
		chip->new_rcomp = ((u8)new_rcomp << 8) | (chip->rcomp&0xFF);
	} else
		chip->new_rcomp = chip->pdata->rcomp_value;

	if (chip->rcomp != chip->new_rcomp) {
		pr_info("%s : 0x%x -> 0x%x (%d)\n", __func__,
						chip->rcomp, chip->new_rcomp, chip->new_rcomp>>8);
	}
}
#endif

static int max17040_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17040_chip *chip = container_of(psy,
				struct max17040_chip, battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		max17040_get_vcell(chip->client);
		val->intval = chip->vcell;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		switch (val->intval) {
		case 0:	/*normal soc */
			val->intval = chip->soc;
			break;
		case 1: /*raw soc */
			val->intval = chip->raw_soc;
			break;
		case 2: /*rcomp */
			val->intval = chip->rcomp;
			break;
		case 3: /*full soc  */
			val->intval = chip->full_soc;
			break;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max17040_set_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val)
{
	struct max17040_chip *chip = container_of(psy,
				struct max17040_chip, battery);
	
	switch (psp) {
	case POWER_SUPPLY_PROP_TEMP:
		chip->temperature = val->intval;
#ifdef ADJUST_RCOMP_WITH_TEMPER
		pr_info("%s: current temperature = %d\n", __func__, chip->temperature);
		max17040_rcomp_update(chip->client, chip->temperature);
#endif
		break;
	case POWER_SUPPLY_PROP_STATUS:
#ifdef ADJUST_RCOMP_WITH_CHARGING_STATUS
		switch (val->intval) {
		case POWER_SUPPLY_STATUS_FULL:
			pr_info("%s: charger full state!\n", __func__);
			/* adjust full soc */
			max17040_adjust_fullsoc(chip->client);
			break;

		case POWER_SUPPLY_STATUS_CHARGING:
			max17040_set_rcomp(chip->client, RCOMP_2ND);
			max17040_get_rcomp(chip->client);
			break;

		case POWER_SUPPLY_STATUS_DISCHARGING:
			max17040_set_rcomp(chip->client, chip->rcomp);
			max17040_get_rcomp(chip->client);
			break;

		default :
			return -EINVAL;
		}
		break;
#else
		pr_info("%s: charger full state!\n", __func__);
		if (val->intval != POWER_SUPPLY_STATUS_FULL)
			return -EINVAL;
		/* adjust full soc */
		max17040_adjust_fullsoc(chip->client);
		break;
#endif
	default:
		return -EINVAL;
	}
	return 0;
}

static int __devinit max17040_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17040_chip *chip;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	printk("%s: MAX17043 driver Loading! \n", __func__);
	
	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->pdata = client->dev.platform_data;

	if (!chip->pdata) {
		pr_err("%s: no fuel gauge platform data\n",	__func__);
		goto err_kfree;
	}

	chip->fg_interval = MAX17040_LONG_DELAY;
	chip->fast_log_count = 0;
	chip->temperature = RCOMP0_TEMP;
	chip->full_soc = FULL_SOC_DEFAULT;
	chip->prevcell = chip->vcell = 0;

	i2c_set_clientdata(client, chip);

	mutex_init(&chip->mutex);

	chip->pdata->hw_init(); /* important */
	max17040_get_version(client);
	chip->rcomp = max17040_get_rcomp(client);

	chip->rcomp = chip->pdata->rcomp_value;
	chip->new_rcomp = chip->pdata->rcomp_value;
	max17040_set_rcomp(client, chip->new_rcomp);
	chip->rcomp = max17040_get_rcomp(client);

	chip->battery.name		= "fuelgauge",
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY,
	chip->battery.get_property	= max17040_get_property,
	chip->battery.set_property	= max17040_set_property,
	chip->battery.properties	= max17040_battery_props,
	chip->battery.num_properties	= ARRAY_SIZE(max17040_battery_props),
	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		goto err_psy_register;
	}

	wake_lock_init(&chip->lowbat_wake_lock, WAKE_LOCK_SUSPEND,
										"fuelgague-lowbat");

	ret = request_threaded_irq(chip->client->irq, NULL,
			max17040_int_work_func, IRQF_TRIGGER_FALLING, "max17040", chip);
	if (ret) {
		pr_err("%s : Failed to request fuelgauge irq\n", __func__);
		goto err_request_irq;
	}

	ret = enable_irq_wake(chip->client->irq);
	if (ret) {
		pr_err("%s : Failed to enable fuelgauge irq wake\n", __func__);
		goto err_irq_wake;
	}

	/* update rcomp test */
	/*
	static int itemp;
	for (itemp=100; itemp>-100; itemp--) {
		max17040_rcomp_update(client, itemp);
	}
	*/
	
	/* create fuelgauge attributes */
	fuelgauge_create_attrs(chip->battery.dev);

	INIT_DELAYED_WORK_DEFERRABLE(&chip->work, max17040_work);
	schedule_delayed_work(&chip->work, 0);

	return 0;

err_irq_wake:
	free_irq(chip->client->irq, NULL);
err_request_irq:
	wake_lock_destroy(&chip->lowbat_wake_lock);
	power_supply_unregister(&chip->battery);
err_psy_register:
	mutex_destroy(&chip->mutex);
err_kfree:
	kfree(chip);
	return ret;
}

static int __devexit max17040_remove(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

	wake_lock_destroy(&chip->lowbat_wake_lock);
	power_supply_unregister(&chip->battery);
	cancel_delayed_work(&chip->work);
	mutex_destroy(&chip->mutex);
	kfree(chip);
	return 0;
}

#ifdef CONFIG_PM

static int max17040_suspend(struct i2c_client *client,
		pm_message_t state)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

	cancel_delayed_work(&chip->work);
	return 0;
}

static int max17040_resume(struct i2c_client *client)
{
	struct max17040_chip *chip = i2c_get_clientdata(client);

	schedule_delayed_work(&chip->work, 0);
	return 0;
}

#else

#define max17040_suspend NULL
#define max17040_resume NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id max17040_id[] = {
	{ "max17040", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17040_id);

static struct i2c_driver max17040_i2c_driver = {
	.driver	= {
		.name	= "max17040",
	},
	.probe		= max17040_probe,
	.remove		= __devexit_p(max17040_remove),
	.suspend	= max17040_suspend,
	.resume		= max17040_resume,
	.id_table	= max17040_id,
};

static int __init max17040_init(void)
{
	return i2c_add_driver(&max17040_i2c_driver);
}
module_init(max17040_init);

static void __exit max17040_exit(void)
{
	i2c_del_driver(&max17040_i2c_driver);
}
module_exit(max17040_exit);

MODULE_AUTHOR("Minkyu Kang <mk7.kang@samsung.com>");
MODULE_DESCRIPTION("MAX17040 Fuel Gauge");
MODULE_LICENSE("GPL");
