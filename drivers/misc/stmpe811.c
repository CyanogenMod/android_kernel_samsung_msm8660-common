#include <linux/types.h>
#include <linux/irq.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/wakelock.h>
#include <linux/blkdev.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <mach/gpio.h>

#include <mach/msm_iomap.h>

// Rami.Jung
#if 0
#include <mach/gpio-sec.h>
#endif

#define STMPE811_CHIP_ID	0x00
#define STMPE811_ID_VER	0x02
#define STMPE811_SYS_CTRL1	0x03
#define STMPE811_SYS_CTRL2	0x04
#define STMPE811_INT_CTRL		0x09
#define STMPE811_INT_EN		0x0A
#define STMPE811_INT_STA		0x0B
#define STMPE811_ADC_INT_EN	0x0E
#define STMPE811_ADC_INT_STA	0x0F
#define STMPE811_ADC_CTRL1		0x20
#define STMPE811_ADC_CTRL2		0x21
#define STMPE811_ADC_CAPT		0x22
#define STMPE811_ADC_DATA_CH0	0x30
#define STMPE811_ADC_DATA_CH1	0x32
#define STMPE811_ADC_DATA_CH2	0x34
#define STMPE811_ADC_DATA_CH3	0x36
#define STMPE811_ADC_DATA_CH4	0x38
#define STMPE811_ADC_DATA_CH5	0x3A
#define STMPE811_ADC_DATA_CH6	0x3C
#define STMPE811_ADC_DATA_CH7	0x3E
#define STMPE811_GPIO_AF 		0x17
#define STMPE811_TSC_CTRL		0x40

#define klogi(fmt, arg...)  printk(KERN_INFO "%s: " fmt "\n" , __func__, ## arg)
#define kloge(fmt, arg...)  printk(KERN_ERR "%s(%d): " fmt "\n" , __func__, __LINE__, ## arg)

static struct device *sec_adc_dev;
extern struct class *sec_class;

static struct file_operations stmpe811_fops =
{
	.owner = THIS_MODULE,
};

static struct miscdevice stmpe811_adc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sec_adc",
	.fops = &stmpe811_fops,
};

static struct i2c_driver stmpe811_adc_i2c_driver;
static struct i2c_client *stmpe811_adc_i2c_client = NULL;


struct stmpe811_adc_state{
	struct i2c_client	*client;
	struct mutex adc_lock;
};
struct stmpe811_adc_state *stmpe811_adc_state;


static int stmpe811_i2c_read(struct i2c_client *client, u8 reg, u8 *data, u8 length)
{
	s32 rc=0;
	
	rc = i2c_smbus_read_i2c_block_data(client, (u8)reg, length, data);
	if (rc < 0 || rc!= length) {
		printk("%s: Failed to stmpe811_i2c_read, rc=%d\n", __func__, rc);
		return -1;
	}

	return 0;
}

static int stmpe811_i2c_write_1byte(u8 addr, u16 w_data)
{
	struct i2c_client *client = stmpe811_adc_i2c_client;

	s32 rc =0;
	u8 length = 1;
	u8 data[1];
	
	data[0] = w_data & 0xFF;

	rc = i2c_smbus_write_i2c_block_data(client, (u8)addr, length, data);
	if (rc < 0) {
		pr_err("%s: Failed to stmpe811_i2c_write, rc=%d\n", __func__, rc);
		return -1;
	}

	return 0;
}

static int stmpe811_i2c_write(struct i2c_client *client, u8 reg, u8 *data, u8 length)
{
	s32 rc =0;
	u16 value =0;

	value=(*(data)<< 8) | (*(data+1));

	rc = i2c_smbus_write_word_data(client, (u8)reg, swab16(value));
	if (rc < 0) {
		pr_err("%s: Failed to stmpe811_i2c_write, rc=%d\n", __func__, rc);
		return -1;
	}

	return 0;
}

int stmpe811_write_register(u8 addr, u16 w_data)
{
	struct i2c_client *client = stmpe811_adc_i2c_client;
	u8 data[2] = {0,0};

	data[0] = w_data & 0xFF;
	data[1] = (w_data >> 8);

	if (stmpe811_i2c_write(client, addr, data, (u8)2) < 0) {
		printk("%s: stmpe811_write_register addr(0x%x)\n", __func__, addr);
		return -1;
	}
	return 0;
}

s16 stmpe811_adc_get_value(u8 channel)
{
	struct i2c_client *client = stmpe811_adc_i2c_client;
	struct stmpe811_adc_state *adc = i2c_get_clientdata(client);
	s16 ret;
	u8 data[2] = {0, 0};
	u16 w_data = 0;
	int data_channel_addr = 0;
	int count = 0;

	mutex_lock(&adc->adc_lock);
	stmpe811_i2c_write_1byte(STMPE811_ADC_CAPT, (1 << channel));

	/* check for conversion completion */
	for(count=0; count<10; count++) {
 		stmpe811_i2c_read(client, STMPE811_ADC_CAPT, data, (u8)1);
		
		if(data[0] & (1 << channel)) {
			pr_debug("%s: Confirmed new data in channel(%d), count=%d \n", __func__, channel, count);
			break;
		}
		pr_debug("count[%d]  data[0]= 0x%x\r\n", count,data[0]);
		msleep(5);
	}
	
	if(count==10)
		pr_err("%s : adc conversion fail, count=%d \r\n", __func__, count);

	/* read value from ADC */
	data_channel_addr = STMPE811_ADC_DATA_CH0 + (channel * 2);
	pr_debug("%s: data_channel_addr = 0x%x, channel = 0x%x\n", __func__,data_channel_addr, (1 << channel));
	msleep(10);

	if (stmpe811_i2c_read(client, data_channel_addr, data, (u8)2) < 0) {
		pr_err("%s: Failed to read ADC_DATA_CH(%d).\n", __func__,channel);
		return -1;
	}

	w_data = ((data[0]<<8) | data[1]) & 0x0FFF;
	pr_info("%s: ADC_DATA_CH(%d) = 0x%x, %d\n", __func__,channel, w_data, w_data);

	stmpe811_i2c_write_1byte(STMPE811_ADC_CAPT, (1 << channel));

	ret = w_data;
	mutex_unlock(&adc->adc_lock);
	
	return ret;
}
EXPORT_SYMBOL(stmpe811_adc_get_value);

static ssize_t adc_test_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", "adc_test_show");
}

static ssize_t adc_test_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int mode;
	s16 val;
	sscanf(buf, "%d", &mode);

	if (mode <0 || mode >3) {
		kloge("invalid channel: %d", mode);
		return -EINVAL;
	}

	val = stmpe811_adc_get_value((u8)mode);
	klogi("value from ch%d: %d", mode, val);
	
	return count;		

}
static DEVICE_ATTR(adc_test, S_IRUGO | S_IWUSR | S_IWGRP,
		adc_test_show, adc_test_store);

static int __init stmpe811_adc_init(void)
{
	int ret =0;

	klogi("start!!");

	/*misc device registration*/
	ret = misc_register(&stmpe811_adc_device);
	if (ret < 0) {
		kloge("misc_register failed");
		return ret; 	  	
	}
	klogi("step 01");

	/* set sysfs for adc test mode*/
	sec_adc_dev = device_create(sec_class, NULL, 0, NULL, "sec_adc");
	if (IS_ERR(sec_adc_dev)) {
		kloge("failed to create device!\n");
		goto  DEREGISTER_MISC;
	}

	klogi("step 02");	
	ret = device_create_file(sec_adc_dev, &dev_attr_adc_test);
	if (ret < 0) {
		kloge("failed to create device file(%s)!\n", dev_attr_adc_test.attr.name);
		goto DESTROY_DEVICE;
	}

	klogi("step 03");
	if (i2c_add_driver(&stmpe811_adc_i2c_driver)!=0)
		kloge("%s: Can't add fg i2c drv\n", __func__);

	klogi("step 04");

	return 0;

DESTROY_DEVICE:
	device_destroy(sec_class, 0);
DEREGISTER_MISC:
	misc_deregister(&stmpe811_adc_device);


klogi("step fail");
	return ret;
}

static void __init stmpe811_adc_exit(void)
{
	klogi("start!");

	i2c_del_driver(&stmpe811_adc_i2c_driver);

	device_remove_file(sec_adc_dev, &dev_attr_adc_test);
	device_destroy(sec_class, 0);

	misc_deregister(&stmpe811_adc_device);
}


static int stmpe811_adc_i2c_remove(struct i2c_client *client)
{
	struct stmpe811_adc_state *adc = i2c_get_clientdata(client);
	mutex_destroy(&adc->adc_lock);
	kfree(adc);

	return 0;
}

#define GPIO_CONFIG(gpio)         (MSM_TLMM_BASE + 0x1000 + (0x10 * (gpio)))  // mskang_test
#define GPIO_IN_OUT(gpio)    (MSM_TLMM_BASE + 0x1004 + (0x10 * (gpio)))  // mskang_test 

extern void stmpe811_gpio_init(void);
extern void stmpe811_ldo_en(bool enable);

static int stmpe811_adc_i2c_probe(struct i2c_client *client,  const struct i2c_device_id *id)
{
	struct stmpe811_adc_state *adc;
	u8 data[2] ={0,0};
	u16 w_data = 0;

	pr_info("%s\n", __func__);

	adc = kzalloc(sizeof(struct stmpe811_adc_state), GFP_KERNEL);
	if (adc == NULL) {		
		printk("failed to allocate memory \n");
		return -ENOMEM;
	}
	
	adc->client = client;
	i2c_set_clientdata(client, adc);

	stmpe811_adc_i2c_client = client;
	mutex_init(&adc->adc_lock);
	
	/* stmpe811 VIO power enable (VREG_3.3V, 8058_l2) */
	stmpe811_ldo_en(1);
	stmpe811_gpio_init();

	/* intialize stmpe811 control register */
	if (stmpe811_i2c_read(client, STMPE811_CHIP_ID, data, (u8)2) < 0) { //00
		printk("%s: Failed to read STMPE811_CHIP_ID.\n", __func__);
		return -1;
	}
	w_data = (data[0]<<8) | data[1];
	printk("%s: CHIP_ID = 0x%x. (spec=0x0811)\n", __func__, w_data);

	stmpe811_i2c_write_1byte(STMPE811_SYS_CTRL1, 0x02); //soft rest
	msleep(10);

	stmpe811_i2c_write_1byte(STMPE811_SYS_CTRL2, 0x0a); // clock on:GPIO&ADC,  off:TS&TSC
	stmpe811_i2c_read(client, STMPE811_SYS_CTRL2, data, (u8)1);
	printk("STMPE811_SYS_CTRL2 = 0x%x..\n", data[0]);

	stmpe811_i2c_write_1byte(STMPE811_INT_EN, 0x00); // disable interrupt
	stmpe811_i2c_read(client, STMPE811_INT_EN, data, (u8)1);
	printk("STMPE811_INT_EN = 0x%x..\n", data[0]);
	
	stmpe811_i2c_write_1byte(STMPE811_ADC_CTRL1, 0x3c); //adc conversion time:64, 12bit ADC oper, internal
	stmpe811_i2c_read(client, STMPE811_ADC_CTRL1, data, (u8)1);
	printk("STMPE811_ADC_CTRL1 = 0x%x..\n", data[0]);

	stmpe811_i2c_write_1byte(STMPE811_ADC_CTRL2, 0x03); //clock speed 6.5MHz
	stmpe811_i2c_read(client, STMPE811_ADC_CTRL2, data, (u8)1);
	printk("STMPE811_ADC_CTRL2 = 0x%x..\n", data[0]);

//   It should be ADC settings. So the value should be 0x00 instead of 0xFF 
//   2011.05.05 by Rami.Jung 
	stmpe811_i2c_write_1byte(STMPE811_GPIO_AF, 0x00); // gpio 0-3 -> ADC
	stmpe811_i2c_read(client, STMPE811_GPIO_AF, data, (u8)1);
	printk("STMPE811_GPIO_AF = 0x%x..\n", data[0]);

	stmpe811_i2c_write_1byte(STMPE811_ADC_CAPT, 0x90); //init Ch[0]=ADC_CHECK(battery), ch[3]=Accessory

	pr_info("%s success\n", __func__);
	return 0;
}


static const struct i2c_device_id stmpe811_adc_device_id[] = {
	{"stmpe811", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, stmpe811_adc_device_id);


static struct i2c_driver stmpe811_adc_i2c_driver = {
	.driver = {
		.name = "stmpe811",
		.owner = THIS_MODULE,
	},
	.probe	= stmpe811_adc_i2c_probe,
	.remove	= stmpe811_adc_i2c_remove,
	.id_table	= stmpe811_adc_device_id,
};

module_init(stmpe811_adc_init);
module_exit(stmpe811_adc_exit);

MODULE_AUTHOR("Samsung");
MODULE_DESCRIPTION("Samsung STMPE811 ADC driver");
MODULE_LICENSE("GPL");
