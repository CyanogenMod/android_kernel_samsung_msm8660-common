#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <asm/irq.h>
#include <linux/delay.h>
#include <linux/sii9234.h>

#include "MHD_SiI9234.h"
#include "sii9234_tpi_regs.h"


#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>

#include <linux/slab.h>


#define SUBJECT "MHL_DRIVER"

#define SII_DEV_DBG(format, ...)\
	pr_info("[ "SUBJECT " (%s,%d) ] " format "\n",\
		__func__, __LINE__, ## __VA_ARGS__);

struct i2c_client *SII9234_i2c_client;
struct i2c_client *SII9234A_i2c_client;
struct i2c_client *SII9234B_i2c_client;
struct i2c_client *SII9234C_i2c_client;
static mhl_power=0;

static struct i2c_device_id SII9234_id[] = {
	{"SII9234", 0},
	{}
};

static struct i2c_device_id SII9234A_id[] = {
	{"SII9234A", 0},
	{}
};

static struct i2c_device_id SII9234B_id[] = {
	{"SII9234B", 0},
	{}
};

static struct i2c_device_id SII9234C_id[] = {
	{"SII9234C", 0},
	{}
};

struct SII9234_state {
	struct i2c_client *client;
};

static struct timer_list MHL_reg_check;

static u8 SII9234_i2c_read(struct i2c_client *client, u8 reg)
{
	u8 ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		SII_DEV_DBG("i2c read fail");
		return -EIO;
	}
	return ret;

}


static int SII9234_i2c_write(struct i2c_client *client, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(client, reg, data);
}

void SII9234_HW_Device_init(struct i2c_client *client)	//hmyun
{
	struct sii9234_platform_data *pdata = client->dev.platform_data;
	SII_DEV_DBG("");
	pdata->hw_device_init();
	mhl_power = 1;
}

void SII9234_HW_Reset(struct i2c_client *client)
{
	struct sii9234_platform_data *pdata = client->dev.platform_data;
	SII_DEV_DBG("");
	pdata->hw_reset();
}

void SII9234_HW_Off(struct i2c_client *client)
{
	struct sii9234_platform_data *pdata = client->dev.platform_data;
	SII_DEV_DBG("");
	pdata->hw_off();
	mhl_power = 0;
}

int SII9234_HW_IsOn(void)
{
#warning - this needs fixing
/*	int IsOn = gpio_get_value(GPIO_HDMI_EN1); */
/*	if(IsOn) */
		return true;
/*	else */
		return false;
}

#include "MHD_SiI9234.c"

void sii_9234_monitor(unsigned long arg)
{
	SII_DEV_DBG("");
	/*sii9234_polling(); */
	ReadIndexedRegister(INDEXED_PAGE_0, 0x81);
#if 0
	pr_info("SII9234_i2c_read  INDEXED_PAGE_0: 0x%02x\n", data);

	MHL_reg_check.expires = get_jiffies_64() + (HZ*3);
	add_timer(&MHL_reg_check);
#endif
}

static void check_HDMI_signal(unsigned long arg)
{
	SII_DEV_DBG("");

#if 0
	u8 data;

	MHL_HW_Reset();
	sii9234_initial_registers_set();
	startTPI();
	mhl_output_enable();
#endif
	sii9234_tpi_init();

	MHL_reg_check.function = sii_9234_monitor;
	MHL_reg_check.expires = get_jiffies_64() + (HZ*3);
	add_timer(&MHL_reg_check);
#if 0
	data = ReadIndexedRegister(INDEXED_PAGE_0, 0x81);
	pr_info("SII9234_i2c_read  INDEXED_PAGE_0: 0x%02x\n", data);
#endif
}


static ssize_t sysfs_store_mhl_power(struct device *dev,
                                       struct device_attribute *attr,
                                       const char *buf, size_t len)
{
        int rc, onoff = 0;
	struct sii9234_platform_data *pdata = dev->platform_data;

	printk("sysfs_mhl_power\n");
        rc = strict_strtoul(buf, 0, (unsigned long *)&onoff);
        if (rc < 0)
                return rc;
	if (onoff)
		sii9234_tpi_init();
	else
		pdata->hw_off();

        return len;
}

static ssize_t sysfs_show_mhl_power(struct device *dev,
                                       struct device_attribute *attr,
                                       const char *buf, size_t len)
{
		
	return sprintf(buf, "mhl_power:%d\n", mhl_power);
}


static DEVICE_ATTR(mhl_power, S_IRUGO|S_IWUSR|S_IWGRP,
                sysfs_show_mhl_power, sysfs_store_mhl_power);


static int SII9234_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct SII9234_state *state;
        int rc = 0;

	SII_DEV_DBG("");

	state = kzalloc(sizeof(struct SII9234_state), GFP_KERNEL);
	if (!state) {
		pr_err("failed to allocate memory\n");
		return -ENOMEM;
	}

	state->client = client;
	i2c_set_clientdata(client, state);

	/* rest of the initialisation goes here. */

	pr_info("SII9234 attach success!!!\n");

        rc = device_create_file(&client->dev, &dev_attr_mhl_power);
        if (rc < 0) {
                pr_err("failed to add mhl_power sysfs entries\n");
		goto err_out;
	}


	SII9234_i2c_client = client;

err_out:
	kfree(state);

	return 0;
}



static int __devexit SII9234_remove(struct i2c_client *client)
{
	struct SII9234_state *state = i2c_get_clientdata(client);
	kfree(state);

	return 0;
}

static int SII9234A_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct SII9234_state *state;

	SII_DEV_DBG("");

	state = kzalloc(sizeof(struct SII9234_state), GFP_KERNEL);
	if (!state) {
		pr_err("failed to allocate memory\n");
		return -ENOMEM;
	}

	state->client = client;
	i2c_set_clientdata(client, state);

	/* rest of the initialisation goes here. */


	pr_info("SII9234A attach success!!!\n");

	SII9234A_i2c_client = client;

	return 0;

}



static int __devexit SII9234A_remove(struct i2c_client *client)
{
	struct SII9234_state *state = i2c_get_clientdata(client);
	kfree(state);
	return 0;
}

static int SII9234B_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct SII9234_state *state;

	SII_DEV_DBG("");

	state = kzalloc(sizeof(struct SII9234_state), GFP_KERNEL);
	if (!state) {
		pr_err("failed to allocate memory\n");
		return -ENOMEM;
	}

	state->client = client;
	i2c_set_clientdata(client, state);

	/* rest of the initialisation goes here. */

	pr_info("SII9234B attach success!!!\n");

	SII9234B_i2c_client = client;

	return 0;
}



static int __devexit SII9234B_remove(struct i2c_client *client)
{
	struct SII9234_state *state = i2c_get_clientdata(client);
	kfree(state);
	return 0;
}

static int SII9234C_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct SII9234_state *state;

	SII_DEV_DBG("");

	state = kzalloc(sizeof(struct SII9234_state), GFP_KERNEL);
	if (!state) {
		pr_err("failed to allocate memory\n");
		return -ENOMEM;
	}

	state->client = client;
	i2c_set_clientdata(client, state);

	/* rest of the initialisation goes here. */
	pr_info("SII9234C attach success!!!\n");

	SII9234C_i2c_client = client;

	return 0;

}



static int __devexit SII9234C_remove(struct i2c_client *client)
{
	struct SII9234_state *state = i2c_get_clientdata(client);
	kfree(state);
	return 0;
}


struct i2c_driver SII9234_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "SII9234",
	},
	.id_table	= SII9234_id,
	.probe	= SII9234_i2c_probe,
	.remove	= __devexit_p(SII9234_remove),
	.command = NULL,
};

struct i2c_driver SII9234A_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "SII9234A",
	},
	.id_table	= SII9234A_id,
	.probe	= SII9234A_i2c_probe,
	.remove	= __devexit_p(SII9234A_remove),
	.command = NULL,
};

struct i2c_driver SII9234B_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "SII9234B",
	},
	.id_table	= SII9234B_id,
	.probe	= SII9234B_i2c_probe,
	.remove	= __devexit_p(SII9234B_remove),
	.command = NULL,
};

struct i2c_driver SII9234C_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "SII9234C",
	},
	.id_table	= SII9234C_id,
	.probe	= SII9234C_i2c_probe,
	.remove	= __devexit_p(SII9234C_remove),
	.command = NULL,
};


