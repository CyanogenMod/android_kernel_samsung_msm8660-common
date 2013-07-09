/*
 *  SMB328A-charger.c
 *  SMB328A charger interface driver
 *
 *  Copyright (C) 2011 Samsung Electronics
 *
 *  <jongmyeong.ko@samsung.com>
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
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/regulator/machine.h>
#include <linux/smb328a_charger.h>
#include <linux/i2c/fsa9480.h>

/* Register define */
#define SMB328A_INPUT_AND_CHARGE_CURRENTS		0x00
#define	SMB328A_CURRENT_TERMINATION			0x01
#define SMB328A_FLOAT_VOLTAGE				0x02
#define SMB328A_FUNCTION_CONTROL_A1			0x03
#define SMB328A_FUNCTION_CONTROL_A2			0x04
#define SMB328A_FUNCTION_CONTROL_B			0x05
#define SMB328A_OTG_PWR_AND_LDO_CONTROL			0x06
#define SMB328A_VARIOUS_CONTROL_FUNCTION_A		0x07
#define SMB328A_CELL_TEMPERATURE_MONITOR		0x08
#define SMB328A_INTERRUPT_SIGNAL_SELECTION		0x09
#define SMB328A_I2C_BUS_SLAVE_ADDRESS			0x0A

#define SMB328A_CLEAR_IRQ				0x30
#define SMB328A_COMMAND					0x31
#define SMB328A_INTERRUPT_STATUS_A			0x32
#define SMB328A_BATTERY_CHARGING_STATUS_A		0x33
#define SMB328A_INTERRUPT_STATUS_B			0x34
#define SMB328A_BATTERY_CHARGING_STATUS_B		0x35
#define SMB328A_BATTERY_CHARGING_STATUS_C		0x36
#define SMB328A_INTERRUPT_STATUS_C			0x37
#define SMB328A_BATTERY_CHARGING_STATUS_D		0x38
#define SMB328A_AUTOMATIC_INPUT_CURRENT_LIMMIT_STATUS	0x39

/* fast charging current defines */
#define FAST_500mA		500
#define FAST_600mA		600
#define FAST_700mA		700
#define FAST_800mA		800
#define FAST_900mA		900
#define FAST_1000mA		1000
#define FAST_1100mA		1100
#define FAST_1200mA		1200

/* input current limit defines */
#define ICL_275mA		275
#define ICL_450mA		450
#define ICL_600mA		600
#define ICL_700mA		700
#define ICL_800mA		800
#define ICL_900mA		900
#define ICL_1000mA		1000
#define ICL_1100mA		1100
#define ICL_1200mA		1200

enum {
	BAT_NOT_DETECTED,
	BAT_DETECTED
};

enum {
	CHG_MODE_NONE,
	CHG_MODE_AC,
	CHG_MODE_USB,
	CHG_MODE_MISC,
	CHG_MODE_UNKNOWN
};

enum {
	OTG_DISABLE,
	OTG_ENABLE
};

struct smb328a_chip {
	struct i2c_client		*client;
	struct delayed_work		work;
	struct power_supply		psy_bat;
	struct smb328a_platform_data	*pdata;
	struct mutex		mutex;

	int chg_mode;
	unsigned int batt_vcell;
	int chg_set_current; /* fast charging current */
	int chg_icl; /* input current limit */
	int lpm_chg_mode;
	unsigned int float_voltage; /* float voltage */
	int aicl_current;
	int aicl_status;
	int otg_check;
};

static enum power_supply_property smb328a_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
};

static int smb328a_write_reg(struct i2c_client *client, int reg, u8 value)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mutex);

	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0) {
		pr_err("%s: err %d, try again!\n", __func__, ret);
		ret = i2c_smbus_write_byte_data(client, reg, value);
		if (ret < 0)
			pr_err("%s: err %d\n", __func__, ret);
	}

	mutex_unlock(&chip->mutex);

	return ret;
}

static int smb328a_read_reg(struct i2c_client *client, int reg)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);
	int ret;

	mutex_lock(&chip->mutex);

	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0) {
		pr_err("%s: err %d, try again!\n", __func__, ret);
		ret = i2c_smbus_read_byte_data(client, reg);
		if (ret < 0)
			pr_err("%s: err %d\n", __func__, ret);
	}

	mutex_unlock(&chip->mutex);

	return ret;
}

#if 0 /* for test */
static void smb328a_print_reg(struct i2c_client *client, int reg)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);
	int data = 0;

	mutex_lock(&chip->mutex);

	data = i2c_smbus_read_byte_data(client, reg);

	if (data < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, data);
	else
		pr_info("%s : reg (0x%x) = 0x%x\n", __func__, reg, data);

	mutex_unlock(&chip->mutex);
}

static void smb328a_print_all_regs(struct i2c_client *client)
{
	smb328a_print_reg(client, 0x31);
	smb328a_print_reg(client, 0x32);
	smb328a_print_reg(client, 0x33);
	smb328a_print_reg(client, 0x34);
	smb328a_print_reg(client, 0x35);
	smb328a_print_reg(client, 0x36);
	smb328a_print_reg(client, 0x37);
	smb328a_print_reg(client, 0x38);
	smb328a_print_reg(client, 0x39);
	smb328a_print_reg(client, 0x00);
	smb328a_print_reg(client, 0x01);
	smb328a_print_reg(client, 0x02);
	smb328a_print_reg(client, 0x03);
	smb328a_print_reg(client, 0x04);
	smb328a_print_reg(client, 0x05);
	smb328a_print_reg(client, 0x06);
	smb328a_print_reg(client, 0x07);
	smb328a_print_reg(client, 0x08);
	smb328a_print_reg(client, 0x09);
	smb328a_print_reg(client, 0x0a);
}
#endif

static void smb328a_allow_volatile_writes(struct i2c_client *client)
{
	int val, reg;
	u8 data;

	reg = SMB328A_COMMAND;
	val = smb328a_read_reg(client, reg);
	if ((val >= 0) && !(val&0x80)) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		data |= (0x1 << 7);
		if (smb328a_write_reg(client, reg, data) < 0)
			pr_err("%s : error!\n", __func__);
		val = smb328a_read_reg(client, reg);
		if (val >= 0) {
			data = (u8)data;
			pr_info("%s : => reg (0x%x) = 0x%x\n",
						__func__, reg, data);
		}
	}
}

static void smb328a_set_command_reg(struct i2c_client *client)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);
	int val, reg;
	u8 data;

	reg = SMB328A_COMMAND;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		if (chip->chg_mode == CHG_MODE_AC ||
			chip->chg_mode == CHG_MODE_MISC ||
			chip->chg_mode == CHG_MODE_UNKNOWN)
			data = 0xad;
		else
			data = 0xa9; /* usb */
		if (smb328a_write_reg(client, reg, data) < 0)
			pr_err("%s : error!\n", __func__);
		val = smb328a_read_reg(client, reg);
		if (val >= 0) {
			data = (u8)data;
			pr_info("%s : => reg (0x%x) = 0x%x\n",
						__func__, reg, data);
		}
	}
}

static void smb328a_clear_irqs(struct i2c_client *client)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);
	int reg;

	reg = SMB328A_CLEAR_IRQ;
	if (smb328a_write_reg(chip->client, reg, 0xff) < 0)
		pr_err("%s : irq clear error!\n", __func__);
}

static void smb328a_charger_function_conrol(struct i2c_client *client)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);
	int val, reg;
	u8 data, set_data;

	smb328a_allow_volatile_writes(client);

	reg = SMB328A_INPUT_AND_CHARGE_CURRENTS;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		if (chip->chg_mode == CHG_MODE_AC) {
#if defined(CONFIG_USA_MODEL_SGH_I717)
			set_data = 0xB7; /* fast 1A */
#else
			set_data = 0x97; /* fast 900mA */
#endif
		} else if (chip->chg_mode == CHG_MODE_MISC) {
			set_data = 0x57; /* fast 700mA */
		} else
			set_data = 0x17; /* fast 500mA */
		/* this can be changed with top-off setting */
		if (data != set_data) {
			data = set_data;
			if (smb328a_write_reg(client, reg, data) < 0)
				pr_err("%s : error!\n", __func__);
			val = smb328a_read_reg(client, reg);
			if (val >= 0) {
				data = (u8)val;
				dev_info(&client->dev,
					"%s : => reg (0x%x) = 0x%x\n",
					__func__, reg, data);
			}
		}
	}

	reg = SMB328A_CURRENT_TERMINATION;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		if (chip->chg_mode == CHG_MODE_AC)
			set_data = 0xb0; /* input 1A */
		else if (chip->chg_mode == CHG_MODE_MISC)
			set_data = 0x50; /* input 700mA */
		else
			set_data = 0x10; /* input 450mA */
		if (data != set_data) { /* AICL enable */
			data = set_data;
			if (smb328a_write_reg(client, reg, data) < 0)
				pr_err("%s : error!\n", __func__);
			val = smb328a_read_reg(client, reg);
			if (val >= 0) {
				data = (u8)val;
				dev_info(&client->dev,
					"%s : => reg (0x%x) = 0x%x\n",
					__func__, reg, data);
			}
		}
	}

	reg = SMB328A_FLOAT_VOLTAGE;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		if (data != 0xca) {
			data = 0xca; /* 4.2V float voltage, for 4.22V : 0xcc */
			if (smb328a_write_reg(client, reg, data) < 0)
				pr_err("%s : error!\n", __func__);
			val = smb328a_read_reg(client, reg);
			if (val >= 0) {
				data = (u8)val;
				dev_info(&client->dev,
					"%s : => reg (0x%x) = 0x%x\n",
					__func__, reg, data);
			}
		}
	}

	reg = SMB328A_FUNCTION_CONTROL_A1;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		if (data != 0xda) {
			data = 0xda;
			if (smb328a_write_reg(client, reg, data) < 0)
				pr_err("%s : error!\n", __func__);
			val = smb328a_read_reg(client, reg);
			if (val >= 0) {
				data = (u8)val;
				dev_info(&client->dev,
					"%s : => reg (0x%x) = 0x%x\n",
					__func__, reg, data);
			}
		}
	}

	reg = SMB328A_FUNCTION_CONTROL_A2;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		if (data != 0x4d) {
			data = 0x4d;
			if (smb328a_write_reg(client, reg, data) < 0)
				pr_err("%s : error!\n", __func__);
			val = smb328a_read_reg(client, reg);
			if (val >= 0) {
				data = (u8)val;
				dev_info(&client->dev,
					"%s : => reg (0x%x) = 0x%x\n",
					__func__, reg, data);
			}
		}
	}

	reg = SMB328A_FUNCTION_CONTROL_B;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		if (data != 0x0) {
			data = 0x0;
			if (smb328a_write_reg(client, reg, data) < 0)
				pr_err("%s : error!\n", __func__);
			val = smb328a_read_reg(client, reg);
			if (val >= 0) {
				data = (u8)val;
				dev_info(&client->dev,
					"%s : => reg (0x%x) = 0x%x\n",
					__func__, reg, data);
			}
		}
	}

	reg = SMB328A_OTG_PWR_AND_LDO_CONTROL;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
			
#if defined(CONFIG_EUR_MODEL_GT_I9210) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_USA_MODEL_SGH_I717)
	set_data = 0xd5;
#else
	
#if defined(CONFIG_TARGET_LOCALE_USA)
		set_data = 0x4d;
#else
		set_data = 0xd5;
#endif
#endif
		if (data != set_data) {
			data = set_data;
			if (smb328a_write_reg(client, reg, data) < 0)
				pr_err("%s : error!\n", __func__);
			val = smb328a_read_reg(client, reg);
			if (val >= 0) {
				data = (u8)val;
				dev_info(&client->dev,
					"%s : => reg (0x%x) = 0x%x\n",
					__func__, reg, data);
			}
		}
	}

	reg = SMB328A_VARIOUS_CONTROL_FUNCTION_A;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		/* this can be changed with top-off setting */
		if (data != 0xf6) {
			data = 0xf6;
			if (smb328a_write_reg(client, reg, data) < 0)
				pr_err("%s : error!\n", __func__);
			val = smb328a_read_reg(client, reg);
			if (val >= 0) {
				data = (u8)val;
				dev_info(&client->dev,
					"%s : => reg (0x%x) = 0x%x\n",
					__func__, reg, data);
			}
		}
	}

	reg = SMB328A_CELL_TEMPERATURE_MONITOR;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		if (data != 0x0) {
			data = 0x0;
			if (smb328a_write_reg(client, reg, data) < 0)
				pr_err("%s : error!\n", __func__);
			val = smb328a_read_reg(client, reg);
			if (val >= 0) {
				data = (u8)val;
				dev_info(&client->dev,
					"%s : => reg (0x%x) = 0x%x\n",
					__func__, reg, data);
			}
		}
	}

	reg = SMB328A_INTERRUPT_SIGNAL_SELECTION;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		if (data != 0x1) {
			data = 0x1;
			if (smb328a_write_reg(client, reg, data) < 0)
				pr_err("%s : error!\n", __func__);
			val = smb328a_read_reg(client, reg);
			if (val >= 0) {
				data = (u8)val;
				dev_info(&client->dev,
					"%s : => reg (0x%x) = 0x%x\n",
					__func__, reg, data);
			}
		}
	}
}

static int smb328a_watchdog_control(struct i2c_client *client, bool enable)
{
	int val, reg;
	u8 data;

	dev_info(&client->dev, "%s : (%d)\n", __func__, enable);

	smb328a_allow_volatile_writes(client);

	reg = SMB328A_FUNCTION_CONTROL_A2;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);

		if (enable)
			data |= (0x1 << 1);
		else
			data &= ~(0x1 << 1);

		if (smb328a_write_reg(client, reg, data) < 0)
			pr_err("%s : error!\n", __func__);
		val = smb328a_read_reg(client, reg);
		if (val >= 0) {
			data = (u8)val;
			dev_info(&client->dev,
				"%s : => reg (0x%x) = 0x%x\n",
				__func__, reg, data);
		}
	}

	return 0;
}

static int smb328a_check_charging_status(struct i2c_client *client)
{
	int val, reg;
	u8 data = 0;
	int ret = -EIO;

	reg = SMB328A_BATTERY_CHARGING_STATUS_C;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);

		ret = (data&(0x3<<1))>>1;
		dev_info(&client->dev, "%s : status = 0x%x\n",
			__func__, data);
	}

	return ret;
}

static bool smb328a_check_is_charging(struct i2c_client *client)
{
	int val, reg;
	u8 data = 0;
	bool ret = false;

	reg = SMB328A_BATTERY_CHARGING_STATUS_C;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);

		if (data&0x1)
			ret = true; /* charger enabled */
	}

	return ret;
}

static bool smb328a_check_bat_full(struct i2c_client *client)
{
	int val, reg;
	u8 data = 0;
	bool ret = false;

	reg = SMB328A_BATTERY_CHARGING_STATUS_C;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		/* pr_info("%s : reg (0x%x) = 0x%x\n",
			__func__, SMB328A_BATTERY_CHARGING_STATUS_C, data); */

		if (data&(0x1<<6))
			ret = true; /* full */
	}

	return ret;
}

/* vf check */
static bool smb328a_check_bat_missing(struct i2c_client *client)
{
	int val, reg;
	u8 data = 0;
	bool ret = false;

	reg = SMB328A_BATTERY_CHARGING_STATUS_B;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		if (data&0x1) {
			pr_info("%s : reg (0x%x) = 0x%x\n",
						__func__, reg, data);
			ret = true; /* missing battery */
		}
	}

	return ret;
}

/* whether valid dcin or not */
static bool smb328a_check_vdcin(struct i2c_client *client)
{
	int val, reg;
	u8 data = 0;
	bool ret = false;

	reg = SMB328A_BATTERY_CHARGING_STATUS_A;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		if (data&(0x1<<1))
			ret = true;
	}

	return ret;
}

static bool smb328a_check_bmd_disabled(struct i2c_client *client)
{
	int val, reg;
	u8 data = 0;
	bool ret = false;

	reg = SMB328A_FUNCTION_CONTROL_B;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		if (data&(0x1<<7)) {
			ret = true;
			pr_info("%s : return ture : reg(0x%x)=0x%x (0x%x)\n",
				__func__, reg, data, data&(0x1<<7));
		}
	}

#if !defined(CONFIG_TARGET_LOCALE_USA)
	reg = SMB328A_OTG_PWR_AND_LDO_CONTROL;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		if ((data&(0x1<<7)) == 0) {
			ret = true;
			pr_info("%s : return ture : reg(0x%x)=0x%x (0x%x)\n",
				__func__, reg, data, data&(0x1<<7));
		}
	}
#endif

	return ret;
}

static int smb328a_chg_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct smb328a_chip *chip = container_of(psy,
				struct smb328a_chip, psy_bat);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (smb328a_check_vdcin(chip->client))
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		if (smb328a_check_bat_missing(chip->client))
			val->intval = BAT_NOT_DETECTED;
		else
			val->intval = BAT_DETECTED;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* pr_info("%s : check bmd available\n", __func__); */
		/* smb328a_print_all_regs(chip->client); */
		/* check VF check available */
		if (smb328a_check_bmd_disabled(chip->client))
			val->intval = 1;
		else
			val->intval = 0;
		/*
		pr_info("smb328a_check_bmd_disabled is %d\n", val->intval);
		*/
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (smb328a_check_bat_full(chip->client))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		switch (smb328a_check_charging_status(chip->client)) {
		case 0:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
			break;
		case 1:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
			break;
		case 2:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
			break;
		case 3:
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			break;
		default:
			pr_err("%s : get charge type error!\n", __func__);
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CHARGE_NOW:
		if (smb328a_check_is_charging(chip->client))
			val->intval = 1;
		else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_ADJ:
		if (chip->chg_set_current != 0)
			val->intval = chip->chg_set_current;
		else
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int smb328a_set_top_off(struct i2c_client *client, int top_off)
{
	int val, reg, set_val = 0;
	u8 data;

	smb328a_allow_volatile_writes(client);

	set_val = top_off/25;
	set_val -= 1;

	if (set_val < 0 || set_val > 7) {
		pr_err("%s: invalid topoff set value(%d)\n", __func__, set_val);
		return -EINVAL;
	}

	reg = SMB328A_INPUT_AND_CHARGE_CURRENTS;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		data &= ~(0x7 << 0);
		data |= (set_val << 0);
		if (smb328a_write_reg(client, reg, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -EIO;
		}
		data = smb328a_read_reg(client, reg);
		dev_info(&client->dev, "%s : => reg (0x%x) = 0x%x\n",
			__func__, reg, data);
	}

	reg = SMB328A_VARIOUS_CONTROL_FUNCTION_A;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		data &= ~(0x7 << 5);
		data |= (set_val << 5);
		if (smb328a_write_reg(client, reg, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -EIO;
		}
		data = smb328a_read_reg(client, reg);
		dev_info(&client->dev, "%s : => reg (0x%x) = 0x%x\n",
			__func__, reg, data);
	}

	return 0;
}

static int smb328a_set_charging_current(struct i2c_client *client,
					int chg_current)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);

	dev_info(&client->dev, "%s :\n", __func__);

	if (chg_current < 450 || chg_current > 1200)
		return -EINVAL;

	chip->chg_set_current = chg_current;

	if (chg_current == 500) {
		chip->chg_mode = CHG_MODE_USB;
	} else if (chg_current == 900) {
		chip->chg_mode = CHG_MODE_AC;
	} else if (chg_current == 700) {
		chip->chg_mode = CHG_MODE_MISC;
	} else if (chg_current == 450) {
		chip->chg_mode = CHG_MODE_UNKNOWN;
	} else {
		pr_err("%s : error! invalid setting current (%d)\n",
			__func__, chg_current);
		chip->chg_mode = CHG_MODE_NONE;
		chip->chg_set_current = 0;
		return -EINVAL;
	}

	return 0;
}

static int smb328a_set_fast_current(struct i2c_client *client,
					int fast_current)
{
	int val, reg, set_val = 0;
	u8 data;

	smb328a_allow_volatile_writes(client);

	set_val = fast_current/100;
	set_val -= 5;

	if (set_val < 0 || set_val > 7) {
		pr_err("%s: invalid fast_current set value(%d)\n",
					__func__, set_val);
		return -EINVAL;
	}

	reg = SMB328A_INPUT_AND_CHARGE_CURRENTS;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev,
			"%s : reg (0x%x) = 0x%x, set_val = 0x%x\n",
			__func__, reg, data, set_val);
		data &= ~(0x7 << 5);
		data |= (set_val << 5);
		dev_info(&client->dev, "%s : write data = 0x%x\n",
						__func__, data);
		if (smb328a_write_reg(client, reg, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -EIO;
		}
		data = smb328a_read_reg(client, reg);
		dev_info(&client->dev, "%s : => reg (0x%x) = 0x%x\n",
			__func__, reg, data);
	}

	return 0;
}

static int smb328a_set_input_current_limit(struct i2c_client *client,
						int input_current)
{
	int val, reg, set_val = 0;
	u8 data;

	smb328a_allow_volatile_writes(client);

	if (input_current == ICL_450mA) {
		set_val = 0;
	} else {
		set_val = input_current/100;
		set_val -= 5;
	}

	if (set_val < 0 || set_val > 7) {
		pr_err("%s: invalid input_current set value(%d)\n",
			__func__, set_val);
		return -EINVAL;
	}

	reg = SMB328A_CURRENT_TERMINATION;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev,
			"%s : reg (0x%x) = 0x%x, set_val = 0x%x\n",
			__func__, reg, data, set_val);
		data &= ~(0x7 << 5);
		data |= (set_val << 5);
		dev_info(&client->dev, "%s : write data = 0x%x\n",
						__func__, data);
		if (smb328a_write_reg(client, reg, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -EIO;
		}
		data = smb328a_read_reg(client, reg);
		dev_info(&client->dev, "%s : => reg (0x%x) = 0x%x\n",
			__func__, reg, data);
	}

	return 0;
}

static int smb328a_adjust_charging_current(struct i2c_client *client,
						int chg_current)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);
	int ret = 0;

	dev_info(&client->dev, "%s :\n", __func__);

	if (chg_current < 500 || chg_current > 1200)
		return -EINVAL;

	chip->chg_set_current = chg_current;

	switch (chg_current) {
	case FAST_500mA:
	case FAST_600mA:
	case FAST_700mA:
	case FAST_800mA:
	case FAST_900mA:
	case FAST_1000mA:
	case FAST_1100mA:
	case FAST_1200mA:
		ret = smb328a_set_fast_current(client, chg_current);
		break;
	default:
		pr_err("%s : error! invalid setting current (%d)\n",
				__func__, chg_current);
		chip->chg_set_current = 0;
		return -EINVAL;
	}

	return ret;
}

static int smb328a_adjust_input_current_limit(struct i2c_client *client,
						int chg_current)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);
	int ret = 0;

	dev_info(&client->dev, "%s :\n", __func__);

	if (chg_current < 450 || chg_current > 1200)
		return -EINVAL;

	chip->chg_icl = chg_current;

	switch (chg_current) {
	case ICL_450mA:
	case ICL_600mA:
	case ICL_700mA:
	case ICL_800mA:
	case ICL_900mA:
	case ICL_1000mA:
	case ICL_1100mA:
	case ICL_1200mA:
		ret = smb328a_set_input_current_limit(client,
						chg_current);
		break;
	default:
		pr_err("%s : error! invalid setting current (%d)\n",
				__func__, chg_current);
		chip->chg_icl = 0;
		return -EINVAL;
	}

	return ret;
}

static int smb328a_get_input_current_limit(struct i2c_client *client)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);
	int val, reg = 0;
	u8 data = 0;

	dev_info(&client->dev, "%s :\n", __func__);

	reg = SMB328A_CURRENT_TERMINATION;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);

		data &= (0x7<<5);
		data >>= 5;

		if (data > 7) {
			pr_err("%s: invalid icl value(%d)\n", __func__, data);
			return -EINVAL;
		}

		if (data == 0)
			chip->chg_icl = ICL_450mA;
		else
			chip->chg_icl = (data+5)*100;

		dev_info(&client->dev, "%s : get icl = %d, data = %d\n",
			__func__, chip->chg_icl, data);
	} else {
		pr_err("%s: get icl failed\n", __func__);
		chip->chg_icl = 0;
		return -EINVAL;
	}

	return 0;
}

static int smb328a_get_AICL_status(struct i2c_client *client)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);
	int val, reg = 0;
	u8 data = 0;

	dev_dbg(&client->dev, "%s :\n", __func__);

	reg = SMB328A_AUTOMATIC_INPUT_CURRENT_LIMMIT_STATUS;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_dbg(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);

		chip->aicl_status = (data&0x8)>>3;

		data &= (0xf<<4);
		data >>= 4;

		if (data > 8) {
			pr_err("%s: invalid aicl value(%d)\n", __func__, data);
			return -EINVAL;
		}

		if (data == 0)
			chip->aicl_current = ICL_450mA;
		else if (data == 8)
			chip->aicl_current = ICL_275mA;
		else
			chip->aicl_current = (data+5)*100;

		dev_dbg(&client->dev,
			"%s : get aicl = %d, status = %d, data = %d\n",
			__func__, chip->aicl_current, chip->aicl_status, data);
	} else {
		pr_err("%s: get aicl failed\n", __func__);
		chip->aicl_current = 0;
		return -EINVAL;
	}

	return 0;
}

#if 0 /* for test */
static int smb328a_adjust_float_voltage(struct i2c_client *client,
					int float_voltage)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);
	int val, reg, set_val = 0;
	u8 rdata, wdata;

	dev_info(&client->dev, "%s :\n", __func__);

	if (chip->chg_mode != CHG_MODE_AC) {
		pr_err("%s: not AC, return!\n", __func__);
		return -EINVAL;
	}

	if (float_voltage < 3460 || float_voltage > 4720) {
		pr_err("%s: invalid set data\n", __func__);
		return -EINVAL;
	}
	/* support only pre-defined range */
	if (float_voltage == 4200)
		wdata = 0xca;
	else if (float_voltage == 4220)
		wdata = 0xcc;
	else if (float_voltage == 4240)
		wdata = 0xce;
	else {
		wdata = 0xca;
		pr_err("%s: it's not supported\n", __func__);
		return -EINVAL;
	}

	reg = SMB328A_FLOAT_VOLTAGE;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		rdata = (u8)val;
		dev_info(&client->dev,
			"%s : reg (0x%x) = 0x%x, set_val = 0x%x\n",
			__func__, reg, rdata, set_val);
		if (rdata != wdata) {
			smb328a_allow_volatile_writes(client);
			dev_info(&client->dev, "%s : write data = 0x%x\n",
							__func__, wdata);
			if (smb328a_write_reg(client, reg, wdata) < 0) {
				pr_err("%s : error!\n", __func__);
				return -EIO;
			}
			rdata = smb328a_read_reg(client, reg);
			dev_info(&client->dev, "%s : => reg (0x%x) = 0x%x\n",
				__func__, reg, rdata);
			chip->float_voltage = float_voltage;
		}
	}

	return 0;
}
#endif


static unsigned int smb328a_get_float_voltage(struct i2c_client *client)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);
	unsigned int float_voltage = 0;
	int val, reg;
	u8 data;

	dev_dbg(&client->dev, "%s :\n", __func__);

	reg = SMB328A_FLOAT_VOLTAGE;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		float_voltage = 3460 + ((data&0x7F)/2)*20;
		chip->float_voltage = float_voltage;
		dev_info(&client->dev,
			"%s : reg (0x%x) = 0x%x, float vol = %d\n",
			__func__, reg, data, float_voltage);
	} else {
		/* for re-setting, set to zero */
		chip->float_voltage = 0;
	}

	return 0;
}

static int smb328a_enable_otg(struct i2c_client *client)
{
	int val, reg;
	u8 data;
	struct smb328a_chip *chip = i2c_get_clientdata(client);
	dev_info(&client->dev, "%s :\n", __func__);

	reg = SMB328A_COMMAND;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		if (data != 0xbb) {
			dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
				__func__, reg, data);
			data = 0xbb;
			if (smb328a_write_reg(client, reg, data) < 0) {
				pr_err("%s : error!\n", __func__);
				return -EIO;
			}
			data = smb328a_read_reg(client, reg);
			dev_info(&client->dev, "%s : => reg (0x%x) = 0x%x\n",
				__func__, reg, data);
		}
		chip->otg_check = OTG_ENABLE;
	}
	return 0;
}

static int smb328a_disable_otg(struct i2c_client *client)
{
	int val, reg;
	u8 data;
	struct smb328a_chip *chip = i2c_get_clientdata(client);

	dev_info(&client->dev, "%s :\n", __func__);

	reg = SMB328A_FUNCTION_CONTROL_B;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		data = 0x0c;
		if (smb328a_write_reg(client, reg, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -EIO;
		}
		mdelay(50);
		data = smb328a_read_reg(client, reg);
		dev_info(&client->dev, "%s : => reg (0x%x) = 0x%x\n",
			__func__, reg, data);

	}

	reg = SMB328A_COMMAND;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		data = 0xb9;
		if (smb328a_write_reg(client, reg, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -EIO;
		}
		mdelay(50);
		data = smb328a_read_reg(client, reg);
		dev_info(&client->dev, "%s : => reg (0x%x) = 0x%x\n",
			__func__, reg, data);
		chip->otg_check = OTG_DISABLE;
	}
	return 0;
}

static void smb328a_ldo_disable(struct i2c_client *client)
{
	int val, reg;
	u8 data;

	dev_info(&client->dev, "%s :\n", __func__);

	smb328a_allow_volatile_writes(client);

	reg = SMB328A_OTG_PWR_AND_LDO_CONTROL;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
					__func__, reg, data);

		data |= (0x1 << 5);
		if (smb328a_write_reg(client, reg, data) < 0)
			pr_err("%s : error!\n", __func__);
		val = smb328a_read_reg(client, reg);
		if (val >= 0) {
			data = (u8)val;
			dev_info(&client->dev, "%s : => reg (0x%x) = 0x%x\n",
						__func__, reg, data);
		}
	}
}

#if defined(CONFIG_TARGET_LOCALE_KOR)
#if defined(CONFIG_KOR_MODEL_SHV_E110S) || defined(CONFIG_KOR_MODEL_SHV_E120L) || defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)
static int smb328a_chgen_bit_control(struct i2c_client *client, bool enable)
{
	int val, reg;
	u8 data;

	dev_info(&client->dev, "%s :\n", __func__);

	reg = SMB328A_COMMAND;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
					__func__, reg, data);
		if (enable)
			data &= ~(0x1 << 4); /* "0" turn off the charger */
		else
			data |= (0x1 << 4); /* "1" turn off the charger */
		if (smb328a_write_reg(client, reg, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -EIO;
		}
		data = smb328a_read_reg(client, reg);
		pr_info("%s : => reg (0x%x) = 0x%x\n", __func__, reg, data);
	}

	return 0;
}
#endif
#endif

static int smb328a_enable_charging(struct i2c_client *client)
{
	int val, reg;
	u8 data;
	struct smb328a_chip *chip = i2c_get_clientdata(client);

	dev_info(&client->dev, "%s :\n", __func__);

	reg = SMB328A_COMMAND;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
					__func__, reg, data);
		if (chip->chg_mode == CHG_MODE_AC ||
			chip->chg_mode == CHG_MODE_MISC ||
			chip->chg_mode == CHG_MODE_UNKNOWN)
			data = 0xad;
		else if (chip->chg_mode == CHG_MODE_USB)
			data = 0xa9;
		else
			data = 0xb9;
		if (smb328a_write_reg(client, reg, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -EIO;
		}
		data = smb328a_read_reg(client, reg);
		pr_info("%s : => reg (0x%x) = 0x%x\n", __func__, reg, data);
	}

	return 0;
}

static int smb328a_disable_charging(struct i2c_client *client)
{
	int val, reg;
	u8 data;
	struct smb328a_chip *chip = i2c_get_clientdata(client);

	dev_info(&client->dev, "%s :\n", __func__);

	reg = SMB328A_COMMAND;
	val = smb328a_read_reg(client, reg);
	if (val >= 0) {
		data = (u8)val;
		dev_info(&client->dev, "%s : reg (0x%x) = 0x%x\n",
					__func__, reg, data);
		data = 0xb9;
		if (smb328a_write_reg(client, reg, data) < 0) {
			pr_err("%s : error!\n", __func__);
			return -EIO;
		}
		data = smb328a_read_reg(client, reg);
		pr_info("%s : => reg (0x%x) = 0x%x\n", __func__, reg, data);
	}

	chip->chg_mode = CHG_MODE_NONE;
	chip->chg_set_current = 0;
	chip->chg_icl = 0;

	return 0;
}

static int smb328a_chg_set_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val)
{
	struct smb328a_chip *chip = container_of(psy,
				struct smb328a_chip, psy_bat);
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_CURRENT_NOW: /* step1) Set charging current */
		ret = smb328a_set_charging_current(chip->client, val->intval);
		smb328a_set_command_reg(chip->client);
		smb328a_charger_function_conrol(chip->client);
		/* TEST */
		/* smb328a_adjust_float_voltage(chip->client, 4240); */
		smb328a_get_input_current_limit(chip->client);
		smb328a_get_float_voltage(chip->client);
		/* smb328a_print_all_regs(chip->client); */
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL: /* step2) Set top-off current */
		if (val->intval < 25 || val->intval > 200) {
			pr_err("%s: invalid topoff current(%d)\n",
					__func__, val->intval);
			return -EINVAL;
		}
		ret = smb328a_set_top_off(chip->client, val->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW: /* step3) Notify Vcell Now */
		chip->batt_vcell = val->intval;
		/* pr_info("%s : vcell(%d)\n", __func__, chip->batt_vcell); */
		/* TEST : at the high voltage threshold,
			set normal float voltage */
		/*
		if (chip->float_voltage != 4200 &&
			chip->batt_vcell >= 4200000) {
			smb328a_adjust_float_voltage(chip->client, 4200);
		}
		*/
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_STATUS: /* step4) Enable/Disable charging */
		if (val->intval == POWER_SUPPLY_STATUS_CHARGING) {
			if ((chip->chg_mode != CHG_MODE_USB) ||
				chip->lpm_chg_mode) {
				smb328a_ldo_disable(chip->client);
			}
			ret = smb328a_enable_charging(chip->client);
			smb328a_watchdog_control(chip->client, true);
#if defined(CONFIG_TARGET_LOCALE_KOR) || defined(CONFIG_TARGET_LOCALE_JPN)
#if defined(CONFIG_KOR_MODEL_SHV_E110S) || \
	defined(CONFIG_KOR_MODEL_SHV_E120L) || \
	defined(CONFIG_KOR_MODEL_SHV_E120S) || \
	defined(CONFIG_KOR_MODEL_SHV_E120K)
			/* W/A for hw_rev00(celox-kor), default value issue */
			/* celox-kor, dali-lgt, dali-skt, dali-kt */
			if (get_hw_rev() <= 0x03) {
				if (chip->batt_vcell > 3900000) {
					smb328a_chgen_bit_control(chip->client,
									false);
					smb328a_chgen_bit_control(chip->client,
									true);
				}
			}
#endif
#endif /* CONFIG_TARGET_LOCALE_KOR */
		} else if (val->intval == POWER_SUPPLY_STATUS_NOT_CHARGING) {
			ret = smb328a_disable_charging(chip->client);
		} else {
			ret = smb328a_disable_charging(chip->client);
			smb328a_watchdog_control(chip->client, false);
		}

		/* smb328a_print_all_regs(chip->client); */
		break;
	case POWER_SUPPLY_PROP_OTG:
		if (val->intval == POWER_SUPPLY_CAPACITY_OTG_ENABLE) {
			smb328a_charger_function_conrol(chip->client);
			ret = smb328a_enable_otg(chip->client);
		} else {
			ret = smb328a_disable_otg(chip->client);
			fsa9480_otg_detach();
		}
		break;
	case POWER_SUPPLY_PROP_CURRENT_ADJ:
		pr_info("%s : adjust charging current from %d to %d\n",
			__func__, chip->chg_set_current, val->intval);
		if (chip->chg_mode == CHG_MODE_AC) {
			ret = smb328a_adjust_charging_current(chip->client,
								val->intval);
		} else {
			pr_info("%s : not AC mode,"
				" skip fast current adjusting\n", __func__);
		}
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static ssize_t sec_smb328a_show_property(struct device *dev,
				    struct device_attribute *attr, char *buf);
static ssize_t sec_smb328a_store_property(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

#define SEC_SMB328A_ATTR(_name)				\
{							\
	.attr = { .name = #_name,			\
		  .mode = 0664 },			\
	.show = sec_smb328a_show_property,		\
	.store = sec_smb328a_store_property,		\
}

static struct device_attribute sec_smb328a_attrs[] = {
	SEC_SMB328A_ATTR(smb_read_36h),
	SEC_SMB328A_ATTR(smb_wr_icl),
	SEC_SMB328A_ATTR(smb_wr_fast),
	SEC_SMB328A_ATTR(smb_read_fv),
	SEC_SMB328A_ATTR(smb_read_aicl),
};

enum {
	SMB_READ_36H = 0,
	SMB_WR_ICL,
	SMB_WR_FAST,
	SMB_READ_FV,
	SMB_READ_AICL,
};

static ssize_t sec_smb328a_show_property(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct smb328a_chip *chip = container_of(psy,
						  struct smb328a_chip,
						  psy_bat);

	int i = 0;
	const ptrdiff_t off = attr - sec_smb328a_attrs;
	int val, reg;
	u8 data = 0;

	switch (off) {
	case SMB_READ_36H:
		reg = SMB328A_BATTERY_CHARGING_STATUS_C;
		val = smb328a_read_reg(chip->client, reg);
		if (val >= 0) {
			data = (u8)val;
			i += scnprintf(buf + i, PAGE_SIZE - i,
					"0x%x (bit6 : %d)\n",
					data, (data&0x40)>>6);
		} else {
			i = -EINVAL;
		}
		break;
	case SMB_WR_ICL:
		reg = SMB328A_CURRENT_TERMINATION;
		val = smb328a_read_reg(chip->client, reg);
		if (val >= 0) {
			data = (u8)val;
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d (0x%x)\n",
					chip->chg_icl, data);
		} else {
			i = -EINVAL;
		}
		break;
	case SMB_WR_FAST:
		reg = SMB328A_INPUT_AND_CHARGE_CURRENTS;
		val = smb328a_read_reg(chip->client, reg);
		if (val >= 0) {
			data = (u8)val;
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d (0x%x)\n",
					chip->chg_set_current, data);
		} else {
			i = -EINVAL;
		}
		break;
	case SMB_READ_FV:
		reg = SMB328A_FLOAT_VOLTAGE;
		val = smb328a_read_reg(chip->client, reg);
		if (val >= 0) {
			data = (u8)val;
			i += scnprintf(buf + i, PAGE_SIZE - i, "0x%x (%dmV)\n",
					data, 3460+((data&0x7F)/2)*20);
		} else {
			i = -EINVAL;
		}
		break;
	case SMB_READ_AICL:
		val = smb328a_get_AICL_status(chip->client);
		if (val >= 0) {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%dmA (%d)\n",
					chip->aicl_current, chip->aicl_status);
		} else {
			i = -EINVAL;
		}
		break;
	default:
		i = -EINVAL;
	}

	return i;
}

static ssize_t sec_smb328a_store_property(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct smb328a_chip *chip = container_of(psy,
						  struct smb328a_chip,
						  psy_bat);

	int x = 0;
	int ret = 0;
	const ptrdiff_t off = attr - sec_smb328a_attrs;

	switch (off) {
	case SMB_WR_ICL:
		if (sscanf(buf, "%d\n", &x) == 1) {
			if (chip->chg_mode == CHG_MODE_AC) {
				ret = smb328a_adjust_input_current_limit(
							chip->client, x);
			} else {
				pr_info("%s : not AC mode,"
					" skip icl adjusting\n", __func__);
				ret = count;
			}
		}
		break;
	case SMB_WR_FAST:
		if (sscanf(buf, "%d\n", &x) == 1) {
			if (chip->chg_mode == CHG_MODE_AC) {
				ret = smb328a_adjust_charging_current(
					chip->client, x);
			} else {
				pr_info("%s : not AC mode,"
					" skip fast current adjusting\n",
					__func__);
				ret = count;
			}
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int smb328a_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(sec_smb328a_attrs); i++) {
		rc = device_create_file(dev, &sec_smb328a_attrs[i]);
		if (rc)
			goto smb328a_attrs_failed;
	}
	goto succeed;

smb328a_attrs_failed:
	while (i--)
		device_remove_file(dev, &sec_smb328a_attrs[i]);
succeed:
	return rc;
}

static irqreturn_t smb328a_int_work_func(int irq, void *smb_chip)
{
	struct smb328a_chip *chip = smb_chip;
	int val, reg;
	/*
	u8 intr_a = 0;
	u8 intr_b = 0;
	*/
	u8 intr_c = 0;
	u8 chg_status = 0;

	pr_info("%s\n", __func__);

	/*
	reg = SMB328A_INTERRUPT_STATUS_A;
	val = smb328a_read_reg(chip->client, reg);
	if (val >= 0) {
		intr_a = (u8)val;
		pr_info("%s : reg (0x%x) = 0x%x\n", __func__, reg, intr_a);
	}
	reg = SMB328A_INTERRUPT_STATUS_B;
	val = smb328a_read_reg(chip->client, reg);
	if (val >= 0) {
		intr_b = (u8)val;
		pr_info("%s : reg (0x%x) = 0x%x\n", __func__, reg, intr_b);
	}
	*/
	reg = SMB328A_INTERRUPT_STATUS_C;
	val = smb328a_read_reg(chip->client, reg);
	if (val >= 0) {
		intr_c = (u8)val;
		pr_info("%s : reg (0x%x) = 0x%x\n", __func__, reg, intr_c);
	}

	reg = SMB328A_BATTERY_CHARGING_STATUS_C;
	val = smb328a_read_reg(chip->client, reg);
	if (val >= 0) {
		chg_status = (u8)val;
		pr_info("%s : reg (0x%x) = 0x%x\n", __func__, reg, chg_status);
	}

#if defined(CONFIG_KOR_MODEL_SHV_E160S) || \
	defined(CONFIG_KOR_MODEL_SHV_E160K) || \
	defined(CONFIG_KOR_MODEL_SHV_E160L) || \
	defined(CONFIG_KOR_MODEL_SHV_E120L) || \
	defined(CONFIG_KOR_MODEL_SHV_E120S) || \
	defined(CONFIG_KOR_MODEL_SHV_E120K) || \
	defined(CONFIG_KOR_MODEL_SHV_E110S) || \
	defined (CONFIG_JPN_MODEL_SC_05D)
	if (intr_c & 0x80) {
		pr_info("%s : charger watchdog intr triggerd!\n", __func__);
		/* panic("charger watchdog intr triggerd!"); */
	}
#endif

	if (chip->pdata->chg_intr_trigger)
		chip->pdata->chg_intr_trigger((int)(chg_status&0x1));

	/* clear IRQ */
	smb328a_clear_irqs(chip->client);

	return IRQ_HANDLED;
}

static int __devinit smb328a_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct smb328a_chip *chip;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	pr_info("%s: SMB328A driver Loading!\n", __func__);

#if defined(CONFIG_KOR_MODEL_SHV_E160S) || \
	defined(CONFIG_KOR_MODEL_SHV_E160K) || \
	defined(CONFIG_KOR_MODEL_SHV_E160L) || \
	defined (CONFIG_JPN_MODEL_SC_05D)
	if (get_hw_rev() < 0x4) {
		pr_info("%s: SMB328A driver Loading SKIP!!!\n", __func__);
		return 0;
	}
#endif

#if defined(CONFIG_USA_MODEL_SGH_I717)
	if (get_hw_rev() == 0x1) {
		pr_info("%s: SMB328A driver Loading SKIP!!!\n", __func__);
		return 0;
	}
#endif

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->pdata = client->dev.platform_data;
	if (!chip->pdata) {
		pr_err("%s: no charger platform data\n", __func__);
		goto err_kfree;
	}

	i2c_set_clientdata(client, chip);

	chip->pdata->hw_init(); /* important */
	chip->chg_mode = CHG_MODE_NONE;
	chip->chg_set_current = 0;
	chip->chg_icl = 0;
	chip->lpm_chg_mode = 0;
	chip->float_voltage = 0;
	if (is_lpcharging_state()) {
		chip->lpm_chg_mode = 1;
		pr_info("%s : is lpm charging mode (%d)\n",
			__func__, chip->lpm_chg_mode);
	}

	mutex_init(&chip->mutex);

	chip->psy_bat.name = "sec-charger",
	chip->psy_bat.type = POWER_SUPPLY_TYPE_BATTERY,
	chip->psy_bat.properties = smb328a_battery_props,
	chip->psy_bat.num_properties = ARRAY_SIZE(smb328a_battery_props),
	chip->psy_bat.get_property = smb328a_chg_get_property,
	chip->psy_bat.set_property = smb328a_chg_set_property,
	ret = power_supply_register(&client->dev, &chip->psy_bat);
	if (ret) {
		pr_err("Failed to register power supply psy_bat\n");
		goto err_psy_register;
	}

	/* clear IRQ */
	smb328a_clear_irqs(chip->client);

	ret = request_threaded_irq(chip->client->irq, NULL,
				smb328a_int_work_func,
				IRQF_TRIGGER_FALLING, "smb328a", chip);
	if (ret) {
		pr_err("%s : Failed to request smb328a charger irq\n",
							__func__);
		goto err_request_irq;
	}

	ret = enable_irq_wake(chip->client->irq);
	if (ret) {
		pr_err("%s : Failed to enable smb328a charger irq wake\n",
							__func__);
		goto err_irq_wake;
	}

	/* smb328a_charger_function_conrol(client); */
	/* smb328a_print_all_regs(client); */

	/* create smb328a attributes */
	smb328a_create_attrs(chip->psy_bat.dev);

	return 0;

err_irq_wake:
	free_irq(chip->client->irq, NULL);
err_request_irq:
	power_supply_unregister(&chip->psy_bat);
err_psy_register:
	mutex_destroy(&chip->mutex);
err_kfree:
	kfree(chip);
	return ret;
}

static int __devexit smb328a_remove(struct i2c_client *client)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);

	power_supply_unregister(&chip->psy_bat);
	mutex_destroy(&chip->mutex);
	kfree(chip);
	return 0;
}

#ifdef CONFIG_PM
static int smb328a_suspend(struct i2c_client *client,
		pm_message_t state)
{
	return 0;
}

static int smb328a_resume(struct i2c_client *client)
{
	return 0;
}
#else
#define smb328a_suspend NULL
#define smb328a_resume NULL
#endif /* CONFIG_PM */

static void smb328a_shutdown(struct i2c_client *client)
{
	struct smb328a_chip *chip = i2c_get_clientdata(client);

	if (chip == NULL)
		return;

	if (chip->otg_check == OTG_ENABLE) {
		smb328a_disable_otg(chip->client);
		fsa9480_otg_detach();
	}
	return;
}

static const struct i2c_device_id smb328a_id[] = {
	{ "smb328a", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, smb328a_id);

static struct i2c_driver smb328a_i2c_driver = {
	.driver	= {
		.name	= "smb328a",
	},
	.probe		= smb328a_probe,
	.remove		= __devexit_p(smb328a_remove),
	.suspend	= smb328a_suspend,
	.resume		= smb328a_resume,
	.shutdown = smb328a_shutdown,
	.id_table	= smb328a_id,
};

static int __init smb328a_init(void)
{
	return i2c_add_driver(&smb328a_i2c_driver);
}
module_init(smb328a_init);

static void __exit smb328a_exit(void)
{
	i2c_del_driver(&smb328a_i2c_driver);
}
module_exit(smb328a_exit);


MODULE_DESCRIPTION("SMB328A charger control driver");
MODULE_AUTHOR("<jongmyeong.ko@samsung.com>");
MODULE_LICENSE("GPL");
