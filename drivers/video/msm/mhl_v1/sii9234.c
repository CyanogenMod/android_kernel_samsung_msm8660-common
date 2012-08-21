/********************************************************************************
* (C) COPYRIGHT 2010 SAMSUNG ELECTRONICS				*
* Silicon Image MHL(Mobile HD Link) Transmitter device driver		*
*									*
* File Name : sii9234.c							*
*									*
* Author    : Aakash Manik 						*
* 			aakash.manik@samsung.com			*
*						 			*
*									*
* Version   : V1							*
* Date      : 11/NOV/2010						*
*				Draft under Review			*
*									*
*									*
* Description: Source file for MHL SiI9234 Transciever 			* 
*									*
* Version info								*
* v0.9 : SGH-I997 Project Primitive MHL Driver				*
*		-  Author Kyungrok Min					*
*				<gyoungrok.min@samsung.com>		*
* Version info								*
* v1.0 : 11/Nov/2010 - Driver Restructuring and  			*
* 			Integration for Dempsey Project			*
*									*
********************************************************************************/
/*===========================================================================

                      EDIT HISTORY FOR FILE

when              who                         what, where, why
--------        ---                        ----------------------------------------------------------
2011/04/06    Rajkumar c m            Added support for qualcomm msm8060
===========================================================================*/

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/gpio.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <linux/syscalls.h> 
#include <linux/fcntl.h> 
#include <asm/uaccess.h> 
#include <linux/types.h>
#include <linux/miscdevice.h>

#include <linux/syscalls.h> 
#include <linux/fcntl.h> 
#include <asm/uaccess.h> 
#include <linux/device.h>

#include <linux/sii9234.h>

#include "Common_Def.h"
#include "sii9234_driver.h"
#include <linux/interrupt.h>

#define SUBJECT "MHL_DRIVER"

#define SII_DEV_DBG(format,...)\
	printk ("[ "SUBJECT " (%s,%d) ] " format "\n", __func__, __LINE__, ## __VA_ARGS__);

#define byte u8

/* 
	- Required by fsa9480 for calling sii9234 initialization
*/
static int SII9234_i2cprobe_status = 0;
static int SII9234A_i2cprobe_status = 0;
static int SII9234B_i2cprobe_status = 0;
static int SII9234C_i2cprobe_status = 0;
int SII9234_i2c_status = 0;



static struct regulator *l25; 		//VSIL_1.2A & VSIL_1.2C Connected to PM8058
static struct regulator *l2;		//VCC_3.3V_MHL Connected to PM8901
static struct regulator *mvs0;	//VCC_1.8V_MHL Connected to PM8901
EXPORT_SYMBOL(SII9234_i2c_status);

struct work_struct SiI9234_int_work;
struct workqueue_struct *sii9234_wq = NULL;


struct i2c_driver SII9234_i2c_driver;
struct i2c_client *SII9234_i2c_client = NULL;

struct i2c_driver SII9234A_i2c_driver;
struct i2c_client *SII9234A_i2c_client = NULL;

struct i2c_driver SII9234B_i2c_driver;
struct i2c_client *SII9234B_i2c_client = NULL;

struct i2c_driver SII9234C_i2c_driver;
struct i2c_client *SII9234C_i2c_client = NULL;

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

int MHL_i2c_init = 0;


struct SII9234_state {
	struct i2c_client *client;
};

#define MHL_SWITCH_TEST	1

#ifdef MHL_SWITCH_TEST
void sii9234_cfg_power(bool on);
//static void sii9234_cfg_gpio(void);
extern bool SiI9234_startTPI(void);
static int __init sii9234_init(void);
extern void SiI9234_HW_Reset(void);
extern unsigned int get_hw_rev(void);
struct class *sec_mhl;
EXPORT_SYMBOL(sec_mhl);

struct device *mhl_switch;
EXPORT_SYMBOL(mhl_switch);

static ssize_t check_MHL_command(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;
	int res;

	printk(KERN_ERR "[MHL]: check_MHL_command\n");
	sii9234_cfg_power(1); //As the startTPI will access the page0 address via i2c, sii_9234 should be on.
	SiI9234_HW_Reset();	
	msleep(300);
	res = SiI9234_startTPI();
	//printk("praveen: The value of res = %d\n", res);
	count = sprintf(buf,"%d\n", res );
	sii9234_cfg_power(0);
	//printk("praveen: The value of count = %d\n", count);
	return count;

}

static ssize_t change_switch_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	char *after;
	unsigned long value = simple_strtoul(buf, &after, 10);
	//int i;
	printk(KERN_ERR "[MHL_SWITCH] Change the switch: %ld\n", value);

	if (value == 1) {
		sii9234_cfg_power(1);	//Configure LDOs
	         
	} else {
		sii9234_cfg_power(0);
	}
	return size;
}

void MHL_On(bool on)
{
	if (on == 1) {
		sii9234_cfg_power(1);
		sii9234_init();
	} else {
		sii9234_cfg_power(0);
		gpio_set_value(GPIO_MHL_SEL, 0); 
	}

}
EXPORT_SYMBOL(MHL_On);


static DEVICE_ATTR(mhl_sel, 0665, check_MHL_command, change_switch_store);
#endif


static ssize_t MHD_check_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;
	int res = 0;  //yoking prevent
	#if 0

	s3c_gpio_setpull(GPIO_MHL_SEL, S3C_GPIO_PULL_UP);	//MHL_SEL

	s3c_gpio_setpin(GPIO_MHL_SEL, 1);
	

	//TVout_LDO_ctrl(true);
	
	if(!MHD_HW_IsOn())
	{
		sii9234_tpi_init();
		res = MHD_Read_deviceID();
		MHD_HW_Off();		
	}
	else
	{
		sii9234_tpi_init();
		res = MHD_Read_deviceID();
	}

	I2C_WriteByte(0x72, 0xA5, 0xE1);
	res = 0;
	res = I2C_ReadByte(0x72, 0xA5);

	printk("A5 res %x",res);

	res = 0;
	res = I2C_ReadByte(0x72, 0x1B);

	printk("Device ID res %x",res);

	res = 0;
	res = I2C_ReadByte(0x72, 0x1C);

	printk("Device Rev ID res %x",res);

	res = 0;
	res = I2C_ReadByte(0x72, 0x1D);

	printk("Device Reserved ID res %x",res);

	printk("\n####HDMI_EN1 %x MHL_RST %x GPIO_MHL_SEL %x\n",gpio_get_value(GPIO_HDMI_EN1),gpio_get_value(GPIO_MHL_RST),gpio_get_value(GPIO_MHL_SEL));

	res = I2C_ReadByte(0x7A, 0x3D);

	res = I2C_ReadByte(0x7A, 0xFF);
		
	s3c_gpio_setpull(GPIO_MHL_SEL, S3C_GPIO_PULL_NONE);	//MHL_SEL

	s3c_gpio_setpin(GPIO_MHL_SEL, 0);

#endif
	count = sprintf(buf,"%d\n", res );
	//TVout_LDO_ctrl(false);
	return count;
}

static ssize_t MHD_check_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	printk("input data --> %s\n", buf);

	return size;
}

static DEVICE_ATTR(MHD_file, S_IRUGO , MHD_check_read, MHD_check_write);


struct i2c_client* get_sii9234_client(u8 device_id)
{

	struct i2c_client* client_ptr;

	if(device_id == 0x72)
		client_ptr = SII9234_i2c_client;
	else if(device_id == 0x7A)
		client_ptr = SII9234A_i2c_client;
	else if(device_id == 0x92)
		client_ptr = SII9234B_i2c_client;
	else if(device_id == 0xC8)
		client_ptr = SII9234C_i2c_client;
	else
		client_ptr = NULL;

	return client_ptr;
}
EXPORT_SYMBOL(get_sii9234_client);

u8 SII9234_i2c_read(struct i2c_client *client, u8 reg)
{
	int ret;
	
	if(!MHL_i2c_init)
	{
		SII_DEV_DBG("I2C not ready\n");
		return 0;
	}


	ret = i2c_smbus_write_byte(client, reg);
	ret = i2c_smbus_read_byte(client);
	//printk("#######Read reg %x data %x\n", reg, ret);
	if (ret < 0)
	{
		SII_DEV_DBG("i2c read fail\n");
		return -EIO;
	}
	return ret;

}
EXPORT_SYMBOL(SII9234_i2c_read);


int SII9234_i2c_write(struct i2c_client *client, u8 reg, u8 data)
{
	if(!MHL_i2c_init)
	{
		SII_DEV_DBG("I2C not ready\n");
		return 0;
	}

	//printk("#######Write reg %x data %x\n", reg, data);
	return i2c_smbus_write_byte_data(client, reg, data);

}
EXPORT_SYMBOL(SII9234_i2c_write);


void SiI9234_interrupt_event_work(struct work_struct *p)
{

	printk("SiI9234_interrupt_event_work() is called\n");

	SiI9234_interrupt_event();
}


void mhl_int_irq_handler_sched(void)
{

	printk("mhl_int_irq_handler_sched() is called\n");
	queue_work(sii9234_wq,&SiI9234_int_work);		
}


irqreturn_t mhl_int_irq_handler(int irq, void *dev_id)
{

	printk("mhl_int_irq_handler() is called\n");
	mhl_int_irq_handler_sched();
	return IRQ_HANDLED;
}

irqreturn_t mhl_wakeup_int_irq_handler(int irq, void *dev_id)
{
	printk("mhl_int_irq_wakeup_handler() is called\n");
	mhl_int_irq_handler_sched();
	//SiI9234_interrupt_event();
	return IRQ_HANDLED;
}

irqreturn_t mhl_hpd_irq_handler(int irq, void *dev_id)
{
	printk("mhl_wake_up_irq_handler() is called\n");
	return IRQ_HANDLED;
}

 
irqreturn_t mhl_wake_up_irq_handler(int irq, void *dev_id)
{

	printk("mhl_wake_up_irq_handler() is called\n");

	mhl_int_irq_handler_sched();
	
	return IRQ_HANDLED;
}

static int SII9234_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct SII9234_state *state;

	struct class *mhl_class;
	struct device *mhl_dev;

	SII_DEV_DBG("");

	state = kzalloc(sizeof(struct SII9234_state), GFP_KERNEL);
	if (state == NULL) {		
		printk("failed to allocate memory \n");
		return -ENOMEM;
	}
	
	state->client = client;
	i2c_set_clientdata(client, state);


	
	/* rest of the initialisation goes here. */
	
	printk("SII9234 attach success!!!\n");

	SII9234_i2c_client = client;

	MHL_i2c_init = 1;

	mhl_class = class_create(THIS_MODULE, "mhl");
	if (IS_ERR(mhl_class))
	{
		pr_err("Failed to create class(mhl)!\n");
	}

	mhl_dev = device_create(mhl_class, NULL, 0, NULL, "mhl_dev");
	if (IS_ERR(mhl_dev))
	{
		pr_err("Failed to create device(mhl_dev)!\n");
	}

	if (device_create_file(mhl_dev, &dev_attr_MHD_file) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_MHD_file.attr.name);

	SII9234_i2cprobe_status = 1;
	if(( SII9234_i2cprobe_status == 1) && ( SII9234A_i2cprobe_status == 1) 
		&& ( SII9234B_i2cprobe_status == 1) && ( SII9234C_i2cprobe_status == 1))
	{
		SII9234_i2c_status = 1;
	}
	return 0;

}



static int __devexit SII9234_remove(struct i2c_client *client)
{
	struct SII9234_state *state = i2c_get_clientdata(client);
	kfree(state);

	return 0;
}

static int SII9234A_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct SII9234_state *state;

	SII_DEV_DBG("");

	state = kzalloc(sizeof(struct SII9234_state), GFP_KERNEL);
	if (state == NULL) {		
		printk("failed to allocate memory \n");
		return -ENOMEM;
	}
	
	state->client = client;
	i2c_set_clientdata(client, state);
	
	/* rest of the initialisation goes here. */
	
	printk("SII9234A attach success!!!\n");

	SII9234A_i2c_client = client;

	SII9234A_i2cprobe_status = 1;
	if(( SII9234_i2cprobe_status == 1) && ( SII9234A_i2cprobe_status == 1) 
		&& ( SII9234B_i2cprobe_status == 1) && ( SII9234C_i2cprobe_status == 1))
	{
		SII9234_i2c_status = 1;
	}

	return 0;

}



static int __devexit SII9234A_remove(struct i2c_client *client)
{
	struct SII9234_state *state = i2c_get_clientdata(client);
	kfree(state);
	return 0;
}

static int SII9234B_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct SII9234_state *state;

	SII_DEV_DBG("");

	state = kzalloc(sizeof(struct SII9234_state), GFP_KERNEL);
	if (state == NULL) {		
		printk("failed to allocate memory \n");
		return -ENOMEM;
	}
	
	state->client = client;
	i2c_set_clientdata(client, state);
	
	/* rest of the initialisation goes here. */
	
	printk("SII9234B attach success!!!\n");

	SII9234B_i2c_client = client;

	SII9234B_i2cprobe_status = 1;
	if(( SII9234_i2cprobe_status == 1) && ( SII9234A_i2cprobe_status == 1) 
		&& ( SII9234B_i2cprobe_status == 1) && ( SII9234C_i2cprobe_status == 1))
	{
		SII9234_i2c_status = 1;
	}
	return 0;

}



static int __devexit SII9234B_remove(struct i2c_client *client)
{
	struct SII9234_state *state = i2c_get_clientdata(client);
	kfree(state);
	return 0;
}

static void mhl_i2c_client_info( void )
{
	printk("SII9234_i2c_client name = %s, device_id = 0x%x\n", SII9234_i2c_client->name, SII9234_i2c_client->addr);
	printk("SII9234A_i2c_client name = %s, device_id = 0x%x\n", SII9234A_i2c_client->name, SII9234A_i2c_client->addr);
	printk("SII9234B_i2c_client name = %s, device_id = 0x%x\n", SII9234B_i2c_client->name, SII9234B_i2c_client->addr);
	printk("SII9234C_i2c_client name = %s, device_id = 0x%x\n", SII9234C_i2c_client->name, SII9234C_i2c_client->addr);
}

static int SII9234C_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	struct SII9234_state *state;
	struct sii9234_platform_data *sil9234C_pdata;

	SII_DEV_DBG("");

	state = kzalloc(sizeof(struct SII9234_state), GFP_KERNEL);
	if (state == NULL) {		
		printk("failed to allocate memory \n");
		return -ENOMEM;
	}
	
	state->client = client;
	i2c_set_clientdata(client, state);
	
	/* rest of the initialisation goes here. */
	
	printk("SII9234C attach success!!!\n");

	SII9234C_i2c_client = client;
	sil9234C_pdata = client->dev.platform_data;

	SII9234C_i2cprobe_status = 1;
	if(( SII9234_i2cprobe_status == 1) && ( SII9234A_i2cprobe_status == 1) 
		&& ( SII9234B_i2cprobe_status == 1) && ( SII9234C_i2cprobe_status == 1))
	{
		SII9234_i2c_status = 1;
	}

#if defined (CONFIG_KOR_MODEL_SHV_E110S)
    if( get_hw_rev() < 7)
    	{
			irq_set_irq_type(IRQ_MHL_INT_9, IRQ_TYPE_EDGE_RISING);
			ret = request_threaded_irq( IRQ_MHL_INT_9 , 
			NULL, mhl_int_irq_handler, IRQF_DISABLED	, "mhl_int", (void*)state);
    	}
	else
		{
			irq_set_irq_type(IRQ_MHL_INT_31, IRQ_TYPE_EDGE_RISING);
			ret = request_threaded_irq( IRQ_MHL_INT_31 , 
			NULL, mhl_int_irq_handler, IRQF_DISABLED	, "mhl_int", (void*)state);
		}
#elif defined (CONFIG_KOR_MODEL_SHV_E120S)
	if( get_hw_rev() < 0x8)
		{
			irq_set_irq_type(IRQ_MHL_INT_9, IRQ_TYPE_EDGE_RISING);
			ret = request_threaded_irq( IRQ_MHL_INT_9 , 
			NULL, mhl_int_irq_handler, IRQF_DISABLED	, "mhl_int", (void*)state);
		}
	else
		{
			irq_set_irq_type(IRQ_MHL_INT_31, IRQ_TYPE_EDGE_RISING);
			ret = request_threaded_irq( IRQ_MHL_INT_31 , 
			NULL, mhl_int_irq_handler, IRQF_DISABLED	, "mhl_int", (void*)state);
		}
#elif defined (CONFIG_KOR_MODEL_SHV_E120K)
		if( get_hw_rev() < 8)
			{
				irq_set_irq_type(IRQ_MHL_INT_9, IRQ_TYPE_EDGE_RISING);
				ret = request_threaded_irq( IRQ_MHL_INT_9 , 
				NULL, mhl_int_irq_handler, IRQF_DISABLED	, "mhl_int", (void*)state);
			}
		else
			{
				irq_set_irq_type(IRQ_MHL_INT_31, IRQ_TYPE_EDGE_RISING);
				ret = request_threaded_irq( IRQ_MHL_INT_31 , 
				NULL, mhl_int_irq_handler, IRQF_DISABLED	, "mhl_int", (void*)state);
			}
#elif defined (CONFIG_KOR_MODEL_SHV_E120L)
	if( get_hw_rev() < 2)
		{
			irq_set_irq_type(IRQ_MHL_INT_9, IRQ_TYPE_EDGE_RISING);
			ret = request_threaded_irq( IRQ_MHL_INT_9 , 
			NULL, mhl_int_irq_handler, IRQF_DISABLED	, "mhl_int", (void*)state);
		}
	else
		{
			irq_set_irq_type(IRQ_MHL_INT_31, IRQ_TYPE_EDGE_RISING);
			ret = request_threaded_irq( IRQ_MHL_INT_31 , 
			NULL, mhl_int_irq_handler, IRQF_DISABLED	, "mhl_int", (void*)state);
		}
#elif defined (CONFIG_JPN_MODEL_SC_03D)
	if( get_hw_rev() < 5)
		{
			irq_set_irq_type(IRQ_MHL_INT_9, IRQ_TYPE_EDGE_RISING);
			ret = request_threaded_irq( IRQ_MHL_INT_9 , 
			NULL, mhl_int_irq_handler, IRQF_DISABLED	, "mhl_int", (void*)state);
		}
	else
		{
			irq_set_irq_type(IRQ_MHL_INT_31, IRQ_TYPE_EDGE_RISING);
			ret = request_threaded_irq( IRQ_MHL_INT_31 , 
			NULL, mhl_int_irq_handler, IRQF_DISABLED	, "mhl_int", (void*)state);
		}
#else
{
	int mhl_int_irq;
	mhl_int_irq = sil9234C_pdata->get_irq();
	irq_set_irq_type(mhl_int_irq, IRQ_TYPE_EDGE_RISING);
	ret = request_threaded_irq(mhl_int_irq, 
			NULL, mhl_int_irq_handler, IRQF_DISABLED	, "mhl_int", (void*)state);
}
#endif
	if (ret < 0) 
	{
		printk("unable to request irq mhl_int err:: %d\n", ret);
		return ret;
	}		
	
	sii9234_wq = create_singlethread_workqueue("sii9234_wq");
	INIT_WORK(&SiI9234_int_work,SiI9234_interrupt_event_work);

	gpio_tlmm_config(GPIO_CFG(GPIO_MHL_WAKE_UP, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),GPIO_CFG_ENABLE);
	gpio_set_value_cansleep(GPIO_MHL_WAKE_UP, 0);
/*
	ret = request_irq(IRQ_MHL_WAKE_UP, mhl_wake_up_irq_handler, IRQF_DISABLED, "mhl_wake_up", (void *) state); // check and study here...
	if (ret) 
	{
		printk("unable to request irq mhl_wake_up err:: %d\n", ret);
		return ret;
	}		
	

	ret = request_irq(IRQ_MHL_HPD, mhl_hpd_irq_handler, IRQ_TYPE_EDGE_BOTH  , "mhl_hpd", mhl_hpd_irq_handler);  
	if (ret) 
	{
		printk("unable to request irq mhl_int err:: %d\n", ret);
		return ret;
	}		
	printk("MHL HPD request successful \n");
*/
	//sii9234_cfg_power(1); Unnecessary supply  the current during boot up time.
	mhl_i2c_client_info();
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





static int  sii9234_pre_cfg_power(void)
{
		int rc;
		

		gpio_set_value_cansleep(GPIO_MHL_RST, 0);

		l25 = regulator_get(NULL, "8058_l25");

		if (IS_ERR(l25)) {
			rc = PTR_ERR(l25);
			pr_err("%s: l25 get failed (%d)\n", __func__, rc);
			return rc;
		}
		rc = regulator_set_voltage(l25, 1200000, 1200000);
		if (rc) {
			pr_err("%s: l25 set level failed (%d)\n", __func__, rc);
			return rc;
		}


		l2 = regulator_get(NULL, "8901_l2");

		if (IS_ERR(l2)) {
			rc = PTR_ERR(l2);
			pr_err("%s: l2 get failed (%d)\n", __func__, rc);
			return rc;
		}
		rc = regulator_set_voltage(l2, 3300000, 3300000);
		if (rc) {
			pr_err("%s: l2 set level failed (%d)\n", __func__, rc);
			return rc;
		}

		mvs0 = regulator_get(NULL, "8901_mvs0");

		if (IS_ERR(mvs0)) {
			rc = PTR_ERR(mvs0);
			pr_err("%s: mvs0 get failed (%d)\n", __func__, rc);
			return rc;
		}

/*	
		rc = regulator_set_voltage(mvs0, 1800000, 1800000);
		if (rc) {
			pr_err("%s: mvs0 set level failed (%d)\n", __func__, rc);
			return rc;
		}
*/
		return 0;

}

static bool mhl_power_state = 0;
bool get_mhl_power_state(void)
{
	return mhl_power_state;
}
EXPORT_SYMBOL(get_mhl_power_state);

void sii9234_cfg_power(bool on)
{

	int rc;
	if (mhl_power_state == on) //Rajucm: Avoid unbalanced voltage regulator onoff
	{
		printk("sii_power_state is already %s\n", mhl_power_state?"on":"off");
		return;
	}
	mhl_power_state = on;

	if(on)
	{
		rc = regulator_enable(l25);		//VSIL_1.2A & VSIL_1.2C 	
		if (rc) {
			pr_err("%s: l25 vreg enable failed (%d)\n", __func__, rc);
			return;
		}

		rc = regulator_enable(l2);		//VCC_3.3V_MHL
		if (rc) {
			pr_err("%s: l2 vreg enable failed (%d)\n", __func__, rc);
			return;
		}


		rc = regulator_enable(mvs0);		//VCC_1.8V_MHL
		if (rc) {
			pr_err("%s: l2 vreg enable failed (%d)\n", __func__, rc);
			return;
		}

		gpio_set_value_cansleep(GPIO_MHL_RST, 0);
		msleep(10);
		gpio_set_value_cansleep(GPIO_MHL_RST, 1);

	printk("sii9234_cfg_power on\n");
	}
	else
	{
		rc = regulator_disable(l25);		//VSIL_1.2A & VSIL_1.2C 	
		if (rc) {
			pr_err("%s: l25 vreg enable failed (%d)\n", __func__, rc);
			return;
		}


		rc = regulator_disable(l2);		//VCC_3.3V_MHL
		if (rc) {
			pr_err("%s: l2 vreg enable failed (%d)\n", __func__, rc);
			return;
		}


		rc = regulator_disable(mvs0);		//VCC_1.8V_MHL
		if (rc) {
			pr_err("%s: l2 vreg enable failed (%d)\n", __func__, rc);
			return;
		}

		gpio_set_value_cansleep(GPIO_MHL_RST, 0);
		printk("sii9234_cfg_power off\n");
	}
	return;

}

#if 0//not used
static void sii9234_cfg_gpio( void )
{
	int ret;
	printk( "sii9234_cfg_gpio: request reset gpio \n");
	//Handled in Board specific file
	//gpio_tlmm_config(GPIO_CFG(64/*GPIO_MHL_SDA*/, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),GPIO_CFG_ENABLE);
	//gpio_tlmm_config(GPIO_CFG(65/*GPIO_MHL_SCL*/, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA),GPIO_CFG_ENABLE);	

	gpio_set_value_cansleep(GPIO_MHL_SEL, 0);

}
#endif

static int mhl_open(struct inode *ip, struct file *fp)
{
	printk("[%s] \n",__func__);
	return 0;

}

static int mhl_release(struct inode *ip, struct file *fp)
{
	
	printk("[%s] \n",__func__);
	return 0;
}


#if 0//not used
static int mhl_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	printk("[%s] \n",__func__);

//	byte data;

	switch(cmd)
	{
#if 0
//Disabling //NAGSM_Android_SEL_Kernel_Aakash_20101206
		case MHL_READ_RCP_DATA:
			data = GetCbusRcpData();
			ResetCbusRcpData();
			put_user(data,(byte *)arg);
			printk("MHL_READ_RCP_DATA read");
			break;
#endif
		default:
		break;
	}
		
	return 0;
}
#endif

static struct file_operations mhl_fops = {
	.owner  = THIS_MODULE,
	.open   = mhl_open,
    	.release = mhl_release,
//    	.ioctl = mhl_ioctl,
};
                 
static struct miscdevice mhl_device = {
    .minor  = MISC_DYNAMIC_MINOR,
    .name   = "mhl",
    .fops   = &mhl_fops,
};

#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

static int __init sii9234_init(void)
{
	int ret;

	printk("\npraveen: In sii9234_init [MHL_V1] \n");

#ifdef CONFIG_SAMSUNG_8X60_TABLET // TEMP for bring up
	return 0;
#endif

#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return 0\n", __func__);
		return 0;
	}
#endif

	sii9234_pre_cfg_power();
	//sii9234_cfg_gpio();
	/* sii9234_cfg_power(1);	//Turn On power to SiI9234 
	*/

#ifdef MHL_SWITCH_TEST
	sec_mhl = class_create(THIS_MODULE, "sec_mhl");
	if (IS_ERR(sec_mhl))
		printk(KERN_ERR "[MHL] Failed to create class (sec_mhl)\n");

	mhl_switch = device_create(sec_mhl, NULL, 0, NULL, "switch");
	if (IS_ERR(mhl_switch))
		printk(KERN_ERR "[MHL] Failed to create device (mhl_switch)\n");
	if (device_create_file(mhl_switch, &dev_attr_mhl_sel) < 0)
		printk(KERN_ERR "[MHL] Failed to create file (mhl_sel)");
#endif

	ret = misc_register(&mhl_device);
	if(ret) {
		pr_err(KERN_ERR "misc_register failed - mhl \n");
	}

	ret = i2c_add_driver(&SII9234_i2c_driver);
	if (ret != 0)
		printk("[MHL SII9234] can't add i2c driver\n");	
	else
		printk("[MHL SII9234] add i2c driver\n");
	ret = i2c_add_driver(&SII9234A_i2c_driver);
	if (ret != 0)
		printk("[MHL SII9234A] can't add i2c driver\n");	
	else
		printk("[MHL SII9234A] add i2c driver\n");
	ret = i2c_add_driver(&SII9234B_i2c_driver);
	if (ret != 0)
		printk("[MHL SII9234B] can't add i2c driver\n");	
	else
		printk("[MHL SII9234B] add i2c driver\n");
	ret = i2c_add_driver(&SII9234C_i2c_driver);
	if (ret != 0)
		printk("[MHL SII9234C] can't add i2c driver\n");	
	else
		printk("[MHL SII9234C] add i2c driver\n");

	return ret;	
}
late_initcall(sii9234_init);			
static void __exit sii9234_exit(void)
{
	i2c_del_driver(&SII9234_i2c_driver);
	i2c_del_driver(&SII9234A_i2c_driver);
	i2c_del_driver(&SII9234B_i2c_driver);	
	i2c_del_driver(&SII9234C_i2c_driver);
	
};
module_exit(sii9234_exit);

MODULE_DESCRIPTION("Sii9234 MHL driver");
MODULE_AUTHOR("Aakash Manik");
MODULE_LICENSE("GPL");
