/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 * Copyright 2011 Michael Richter (alias neldar)
 * Copyright 2012 Jeffrey Clark <h0tw1r3@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 * BLN hack oriignally by neldar for SGS. Adapted for SGSII by creams
 * 			addapted for samsung-msm8660-common by Mr. X
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <asm/gpio.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/earlysuspend.h>
#include <asm/io.h>
#if defined(CONFIG_GENERIC_BLN)
#include <linux/bln.h>
#endif

#include "cypress-touchkey.h"
#include <linux/regulator/consumer.h>
#include <linux/mfd/pmic8058.h>

/*
Melfas touchkey register
*/
#define KEYCODE_REG 0x00
#define FIRMWARE_VERSION 0x01
#define TOUCHKEY_MODULE_VERSION 0x02
#define TOUCHKEY_ADDRESS	0x20

#define UPDOWN_EVENT_BIT 0x08
#define KEYCODE_BIT 0x07
#define ESD_STATE_BIT 0x10

/* keycode value */
#define RESET_KEY 0x01
#define SWTICH_KEY 0x02
#define OK_KEY 0x03
#define END_KEY 0x04

#define I2C_M_WR 0		/* for i2c */
#define DEVICE_NAME "melfas_touchkey"

#if defined (CONFIG_USA_MODEL_SGH_T769)
#define BUILTIN_FW_VER	0x0F
#endif
#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
#define BUIL_FW_VER	0x05
#endif


/*sec_class sysfs*/
extern struct class *sec_class;
struct device *sec_touchkey;


#if defined(CONFIG_S5PC110_T959_BOARD)
static int touchkey_keycode[] = {NULL, KEY_BACK, KEY_ENTER, KEY_MENU, KEY_END}; // BEHOLD3
#else
#if /*defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989) ||*/ defined (CONFIG_JPN_MODEL_SC_03D)\
|| defined (CONFIG_KOR_MODEL_SHV_E110S) /*|| defined (CONFIG_USA_MODEL_SGH_I717)*/ || defined(CONFIG_KOR_MODEL_SHV_E160L)
static int touchkey_keycode[5] = {0,KEY_MENU , KEY_HOME, KEY_BACK, KEY_SEARCH};
#else
#if defined (CONFIG_USA_MODEL_SGH_I727)|| defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_I717)\
	|| defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)\
	|| defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
static int touchkey_keycode[5] = {0,KEY_MENU , KEY_HOMEPAGE, KEY_BACK, KEY_SEARCH};
#elif defined (CONFIG_JPN_MODEL_SC_05D)
static int touchkey_keycode[5] = {0,KEY_MENU , KEY_HOME, KEY_BACK, 0};
#else
static int touchkey_keycode[3] = { 0, KEY_BACK, KEY_MENU };
#endif
#endif 
#endif 
#if defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)//new touchkey fpcb
static int touchkey_pba_revision = TOUCHKEY_PBA_REV_NA;
#endif
static int vol_mv_level = 33;

extern int touch_is_pressed;

#if defined (CONFIG_EPEN_WACOM_G5SP)
extern int wacom_is_pressed;
#endif

#if defined (CONFIG_JPN_MODEL_SC_03D)
static u8 firm_version = 0;
#endif

#define BACKLIGHT_ON		0x10
#define BACKLIGHT_OFF		0x20

#define OLD_BACKLIGHT_ON	0x1
#define OLD_BACKLIGHT_OFF	0x2

struct i2c_touchkey_driver {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct early_suspend early_suspend;
	u8 backlight_on;
	u8 backlight_off;
	bool is_dead;
	bool is_powering_on;
	bool is_delay_led_on;
	bool is_backlight_on;
	bool is_key_pressed;
	bool is_bl_disabled;
	bool is_bln_active;
	struct mutex mutex;
};
struct i2c_touchkey_driver *touchkey_driver = NULL;
struct workqueue_struct *touchkey_wq;

struct work_struct touch_update_work;

#if defined (DEBUG_TKEY_RELEASE_DATA)
static bool g_debug_switch = true;
#else
static bool g_debug_switch = false;
#endif

static const struct i2c_device_id melfas_touchkey_id[] = {
	{"melfas_touchkey", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, melfas_touchkey_id);

extern unsigned int get_hw_rev(void);
static void init_hw(void);
static int i2c_touchkey_probe(struct i2c_client *client, const struct i2c_device_id *id);
extern int get_touchkey_firmware(char *version);
static int touchkey_led_status = 0;
static int touchled_cmd_reversed=0;
extern int tkey_vdd_enable(int onoff);
extern int tkey_led_vdd_enable(int onoff);
static int tkey_vdd_enabled = 1;
static int tkey_led_vdd_enabled = 1;
int my_tkey_vdd_enable(int onoff) {
  printk(KERN_ERR "[TKEY] %s(%d) tkey_vdd_enabled=%d tkey_led_vdd_enabled=%d\n", __func__, onoff, tkey_vdd_enabled, tkey_led_vdd_enabled);
  tkey_vdd_enabled = onoff;
  return tkey_vdd_enable(onoff);
}
int my_tkey_led_vdd_enable(int onoff) {
  printk(KERN_ERR "[TKEY] %s(%d) tkey_vdd_enabled=%d tkey_led_vdd_enabled=%d\n", __func__, onoff, tkey_vdd_enabled, tkey_led_vdd_enabled);
  tkey_led_vdd_enabled = onoff;
  return tkey_led_vdd_enable(onoff);
}

struct i2c_driver touchkey_i2c_driver = {
	.driver = {
		.name = "melfas_touchkey_driver",
		.owner	= THIS_MODULE,
	},
	.id_table = melfas_touchkey_id,
	.probe = i2c_touchkey_probe,
};

static int touch_version = 0;

static int i2c_touchkey_write(u8 * val, unsigned int len);
static int touchkey_auto_calibration(int autocal_on_off);

static void set_backlight_onoff_values(void) {
#if defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_I727)
	if (get_hw_rev() >=0x05 )
	{
		touchkey_driver->backlight_on  = BACKLIGHT_ON;
		touchkey_driver->backlight_off = BACKLIGHT_OFF;
	} else {
		touchkey_driver->backlight_on  = OLD_BACKLIGHT_ON;
		touchkey_driver->backlight_off = OLD_BACKLIGHT_OFF;
	}
#elif defined (CONFIG_USA_MODEL_SGH_I717)
	touchkey_driver->backlight_on  = BACKLIGHT_ON;
	touchkey_driver->backlight_off = BACKLIGHT_OFF;
#else
	touchkey_driver->backlight_on  = OLD_BACKLIGHT_ON;
	touchkey_driver->backlight_off = OLD_BACKLIGHT_OFF;
#endif
}

static void touchkey_off(void) {
#if defined (CONFIG_USA_MODEL_SGH_T989)
	if (get_hw_rev() >= 0x0d){
		my_tkey_vdd_enable(0);
		my_tkey_led_vdd_enable(0);
		gpio_direction_output(GPIO_TOUCHKEY_SCL, 0);
		gpio_free(GPIO_TOUCHKEY_SCL);
		gpio_direction_output(GPIO_TOUCHKEY_SDA, 0);
		gpio_free(GPIO_TOUCHKEY_SDA);
	}
#elif defined (CONFIG_USA_MODEL_SGH_I727)
	if (get_hw_rev() >= 0x0a){
		my_tkey_vdd_enable(0);
		my_tkey_led_vdd_enable(0);
		gpio_direction_output(GPIO_TOUCHKEY_SCL, 0);
		gpio_free(GPIO_TOUCHKEY_SCL);
		gpio_direction_output(GPIO_TOUCHKEY_SDA, 0);
		gpio_free(GPIO_TOUCHKEY_SDA);
	}
#elif defined (CONFIG_USA_MODEL_SGH_I717)
	my_tkey_vdd_enable(0);
	my_tkey_led_vdd_enable(0);
	gpio_direction_output(GPIO_TOUCHKEY_SCL, 0);
	gpio_free(GPIO_TOUCHKEY_SCL);
	gpio_direction_output(GPIO_TOUCHKEY_SDA, 0);
	gpio_free(GPIO_TOUCHKEY_SDA);
#endif
}

static void touchkey_on(void) {
#if defined (CONFIG_USA_MODEL_SGH_T989)
	if (get_hw_rev() >= 0x0d){
		my_tkey_vdd_enable(1);
		gpio_request(GPIO_TOUCHKEY_SCL, "TKEY_SCL");
		gpio_direction_input(GPIO_TOUCHKEY_SCL);
		gpio_request(GPIO_TOUCHKEY_SDA, "TKEY_SDA");
		gpio_direction_input(GPIO_TOUCHKEY_SDA);
	}
#elif defined (CONFIG_USA_MODEL_SGH_I727)
	if (get_hw_rev() >= 0x06){
		my_tkey_vdd_enable(1);
		gpio_request(GPIO_TOUCHKEY_SCL, "TKEY_SCL");
		gpio_direction_input(GPIO_TOUCHKEY_SCL);
		gpio_request(GPIO_TOUCHKEY_SDA, "TKEY_SDA");
		gpio_direction_input(GPIO_TOUCHKEY_SDA);
	}
#elif defined (CONFIG_USA_MODEL_SGH_I717)
	my_tkey_vdd_enable(1);
	gpio_request(GPIO_TOUCHKEY_SCL, "TKEY_SCL");
	gpio_direction_input(GPIO_TOUCHKEY_SCL);
	gpio_request(GPIO_TOUCHKEY_SDA, "TKEY_SDA");
	gpio_direction_input(GPIO_TOUCHKEY_SDA);
#endif
	init_hw();

	if(touchled_cmd_reversed) {
		touchled_cmd_reversed = 0;
#if defined (CONFIG_USA_MODEL_SGH_I717)	|| defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined(CONFIG_USA_MODEL_SGH_I727) || defined(CONFIG_USA_MODEL_SGH_T989)
		msleep(100);
#endif
		i2c_touchkey_write((u8*)&touchkey_led_status, 1);
		printk("[TKEY] LED RESERVED !! LED returned on touchkey_led_status = %d\n", touchkey_led_status);
	}
#if defined (CONFIG_USA_MODEL_SGH_I717)
	else {
		my_tkey_vdd_enable(0);
		msleep(100);
		my_tkey_vdd_enable(1);
		gpio_request(GPIO_TOUCHKEY_SCL, "TKEY_SCL");
		gpio_direction_input(GPIO_TOUCHKEY_SCL);
		gpio_request(GPIO_TOUCHKEY_SDA, "TKEY_SDA");
		gpio_direction_input(GPIO_TOUCHKEY_SDA);
		init_hw();

		msleep(100);
		//if(!touchkey_enable )
			//touchkey_enable = 1;
		i2c_touchkey_write(&touchkey_led_status, 1);
		printk("[TKEY] NOT RESERVED!! LED returned on touchkey_led_status = %d\n", touchkey_led_status);
	}
#endif

#if defined (CONFIG_USA_MODEL_SGH_I727)
	if (get_hw_rev() >=0x0a){
		my_tkey_led_vdd_enable(1);
	}
#elif defined (CONFIG_USA_MODEL_SGH_I717)
	if (false){
		my_tkey_led_vdd_enable(1);
	}
#elif defined (CONFIG_USA_MODEL_SGH_T989)
	if (get_hw_rev() >=0x0d){
		my_tkey_led_vdd_enable(1);
	}
#endif

	msleep(50);
	touchkey_auto_calibration(1/*on*/);
}

static int i2c_touchkey_read(u8 reg, u8 * val, unsigned int len)
{
	int err = 0;
	int retry = 5;
	struct i2c_msg msg[1];

	if ((touchkey_driver == NULL)) {
		printk(KERN_DEBUG "[TKEY] touchkey is not enabled.R\n");
		return -ENODEV;
	}

	while (retry--) {
		msg->addr = touchkey_driver->client->addr;
		msg->flags = I2C_M_RD;
		msg->len = len;
		msg->buf = val;
		err = i2c_transfer(touchkey_driver->client->adapter, msg, 1);
		if (err >= 0)
			return 0;
		printk(KERN_ERR "[TKEY] %s %d i2c transfer error\n", __func__,
		       __LINE__);
		msleep(10);
	}
	return err;
}

static int i2c_touchkey_write(u8 * val, unsigned int len)
{
	int err = 0;
	struct i2c_msg msg[1];
	unsigned char data[2];
	int retry = 2;

	printk(KERN_ERR "[TKEY] %s tkey_vdd_enabled=%d tkey_led_vdd_enabled=%d\n", __func__, tkey_vdd_enabled, tkey_led_vdd_enabled);

	if ((tkey_vdd_enabled == 0) || (tkey_led_vdd_enabled == 0)) {
		WARN(1, "[TKEY] %s suppressing due to tkey being powered off!\n", __func__);
		return -ENODEV;
	}

	if (touchkey_driver == NULL) {
		printk(KERN_ERR "[TKEY] touchkey is not enabled.W\n");
		return -ENODEV;
	}

	while (retry--) {
		data[0] = *val;
		msg->addr = touchkey_driver->client->addr;
		msg->flags = I2C_M_WR;
		msg->len = len;
		msg->buf = data;
		err = i2c_transfer(touchkey_driver->client->adapter, msg, 1);
		if (err >= 0)
			return 0;
		printk(KERN_DEBUG "[TKEY] %s %d i2c transfer error\n", __func__,
		       __LINE__);
		msleep(10);
	}
	return err;
}

int cypress_write_register(u8 addr, u8 w_data)
{
	if (i2c_smbus_write_byte_data(touchkey_driver->client, addr, w_data) < 0) {
		pr_err("[TKEY] %s: Failed to write addr=[0x%x], data=[0x%x]\n", __func__, addr, w_data);
		return -1;
	}

	return 0;
}

/* Mutex must be locked when calling. */
static void all_keys_up(void) {
	int index =0;
	for (index = 1; index< sizeof(touchkey_keycode)/sizeof(*touchkey_keycode); index++)
	{
		input_report_key(touchkey_driver->input_dev, touchkey_keycode[index], 0);
		input_sync(touchkey_driver->input_dev);
	}
	touchkey_driver->is_key_pressed = false;
}

static irqreturn_t touchkey_interrupt_thread(int irq, void *dummy)  // ks 79 - threaded irq(becuase of pmic gpio int pin)-> when reg is read in work_func, data0 is always release. so temporarily move the work_func to threaded irq.
{
	u8 data[3];
	int ret;
	int retry = 10;

	if (touchkey_driver->is_powering_on)
		goto unlock;

	mutex_lock(&touchkey_driver->mutex);

	ret = i2c_touchkey_read(KEYCODE_REG, data, 1);

	if(g_debug_switch)
		printk("[TKEY] DATA0 %d\n", data[0]);

	if ((data[0] & ESD_STATE_BIT) || (ret != 0)) {
		printk("[TKEY] ESD_STATE_BIT set or I2C fail: data: %d, retry: %d\n", data[0], retry);

		//releae key 
		input_report_key(touchkey_driver->input_dev, touchkey_keycode[1], 0);
		input_report_key(touchkey_driver->input_dev, touchkey_keycode[2], 0);
		retry = 10;

		disable_irq_nosync(IRQ_TOUCHKEY_INT);
		while (retry--) {
			msleep(300);
			init_hw();
			if (i2c_touchkey_read(KEYCODE_REG, data, 3) >= 0) {
				printk("[TKEY] %s touchkey init success\n", __func__);
				enable_irq(IRQ_TOUCHKEY_INT);
				goto unlock;
			}
			printk("[TKEY] %s %d i2c transfer error retry = %d\n", __func__, __LINE__, retry);
		}
		//touchkey die , do not enable touchkey
		//enable_irq(IRQ_TOUCH_INT);
		touchkey_driver->is_dead = 1;
		printk("[TKEY] %s touchkey died\n", __func__);
		goto err;
	}

	if (data[0] & UPDOWN_EVENT_BIT) {
		input_report_key(touchkey_driver->input_dev, touchkey_keycode[data[0] & KEYCODE_BIT], 0);
		input_sync(touchkey_driver->input_dev);
		touchkey_driver->is_key_pressed = false;
		
		if(g_debug_switch)			
			printk(KERN_DEBUG "[TKEY] touchkey release keycode:%d \n", touchkey_keycode[data[0] & KEYCODE_BIT]);
	} else {
		if (touch_is_pressed) {   
			printk(KERN_DEBUG "[TKEY] touchkey pressed but don't send event because touch is pressed. \n");
		} else {
			if ((data[0] & KEYCODE_BIT) == 2) {	// if back key is pressed, release multitouch
			}
			input_report_key(touchkey_driver->input_dev, touchkey_keycode[data[0] & KEYCODE_BIT], 1);
			input_sync(touchkey_driver->input_dev);
			
			if(g_debug_switch)				
				printk(KERN_DEBUG "[TKEY] touchkey press keycode:%d \n", touchkey_keycode[data[0] & KEYCODE_BIT]);
		}
		touchkey_driver->is_key_pressed = true;
	}
err:
unlock:
	mutex_unlock(&touchkey_driver->mutex);
	return IRQ_HANDLED;
}

static irqreturn_t touchkey_interrupt_handler(int irq, void *dummy) {
	/* Can't lock the mutex in interrupt context, but should be OK. */
	if (touchkey_driver->is_powering_on) {
		dev_dbg(&touchkey_driver->client->dev, "%s: ignoring spurious boot "
					"interrupt\n", __func__);
		return IRQ_HANDLED;
	}

	return IRQ_WAKE_THREAD;
}

#if defined(CONFIG_USA_MODEL_SGH_I717)
static int touchkey_auto_calibration(int autocal_on_off)
{
	
	u8 data[6]={0,};
	int count = 0;
	int ret = 0;
	unsigned short retry = 0;

	while( retry < 3 )
	{
		ret = i2c_touchkey_read(KEYCODE_REG, data, 4);
		if (ret < 0) {
			printk(KERN_ERR"[TKEY] i2c read fail.\n");
			return ret;
		}
		printk(KERN_DEBUG "[TKEY] touchkey_autocalibration :data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",data[0],data[1],data[2],data[3]);

		/* Send autocal Command */
		data[0] = 0x50;
		data[3] = 0x01;

		count = i2c_touchkey_write(data, 4);

		msleep(100);

		/* Check autocal status*/
		ret = i2c_touchkey_read(KEYCODE_REG, data, 6);

		if((data[5] & 0x80)) {
			printk(KERN_DEBUG "[TKEY] autocal Enabled\n");
			break;
		}
		else
			printk(KERN_DEBUG "[TKEY] autocal disabled, retry %d\n", retry);

		retry = retry + 1;
	}

	if( retry == 3 )
		printk(KERN_DEBUG "[TKEY] autocal failed\n");

	return count;


}
#else
static int touchkey_auto_calibration(int autocal_on_off)
{
	signed char int_data[] ={0x50,0x00,0x00,0x01};
	signed char int_data1[] ={0x50,0x00,0x00,0x08};	
//	signed char data[0];
	
	printk("[TKEY] enter touchkey_auto_calibration\n");
		
	if (autocal_on_off == 1)
		i2c_touchkey_write(int_data, 4);	
	else
		i2c_touchkey_write(int_data1, 4);

	msleep(10);
	// i2c_touchkey_read	(0x05, data, 1);
	// printk("[TKEY] end touchkey_auto_calibration result = %d",data[0]);
	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_touchkey_early_suspend(struct early_suspend *h)
{
	printk(KERN_DEBUG "[TKEY] melfas_touchkey_early_suspend\n");

	mutex_lock(&touchkey_driver->mutex);

	touchkey_driver->is_powering_on = true;

	if (unlikely(touchkey_driver->is_dead))
		goto unlock;

	disable_irq_nosync(IRQ_TOUCHKEY_INT);

	touchkey_off();
	all_keys_up();

unlock:
	mutex_unlock(&touchkey_driver->mutex);
}

static void melfas_touchkey_early_resume(struct early_suspend *h)
{
	printk(KERN_DEBUG "[TKEY] melfas_touchkey_early_resume\n");

#if defined(CONFIG_GENERIC_BLN)
	if (touchkey_driver->is_bln_active) {
	printk(KERN_DEBUG "[TKEY] %s:%d\n", __func__, __LINE__);
		cancel_bln_activity();
		touchkey_driver->is_bln_active = false;
	}
#endif

	mutex_lock(&touchkey_driver->mutex);

	touchkey_on();

	touchkey_driver->is_dead = false;
	enable_irq(IRQ_TOUCHKEY_INT);
	touchkey_driver->is_powering_on = false;

	if (touchkey_driver->is_delay_led_on){
		int ret;
		if (touchkey_driver->is_backlight_on) {
			ret = i2c_touchkey_write(&touchkey_driver->backlight_on, 1);
			dev_err(&touchkey_driver->client->dev,"%s: [TKEY] Touch Key led ON ret = %d\n",__func__, ret);
		} else {
			ret = i2c_touchkey_write(&touchkey_driver->backlight_off, 1);
			dev_err(&touchkey_driver->client->dev,"%s: [TKEY] Touch Key led OFF ret = %d\n",__func__, ret);
		}
	}
	touchkey_driver->is_delay_led_on = false;

	mutex_unlock(&touchkey_driver->mutex);
}
#endif				// End of CONFIG_HAS_EARLYSUSPEND

static ssize_t touchleds_disabled_show(struct device *dev,
                                       struct device_attribute *attr,
                                       char *buf)
{
	int res;

	mutex_lock(&touchkey_driver->mutex);
	res = snprintf(buf, PAGE_SIZE, "%u\n",
	               (unsigned int)touchkey_driver->is_bl_disabled);
	mutex_unlock(&touchkey_driver->mutex);

	return res;
}

static ssize_t touchleds_disabled_store(struct device *dev,
                                        struct device_attribute *attr,
                                        const char *buf, size_t count)
{
	unsigned long val;
	int res;

	if ((res = strict_strtoul(buf, 10, &val)) < 0)
		return res;

	mutex_lock(&touchkey_driver->mutex);
	touchkey_driver->is_bl_disabled = val;
	mutex_unlock(&touchkey_driver->mutex);

	return count;
}

#if defined(CONFIG_GENERIC_BLN)
static void cypress_touchkey_enable_backlight(void) {
	printk("[TKEY] BLN %s\n", __func__);
	mutex_lock(&touchkey_driver->mutex);
	if (touchkey_driver->is_bln_active) {
		i2c_touchkey_write(&touchkey_driver->backlight_on, 1);
	}
	mutex_unlock(&touchkey_driver->mutex);
}

static void cypress_touchkey_disable_backlight(void) {
	printk("[TKEY] BLN %s\n", __func__);
	mutex_lock(&touchkey_driver->mutex);
	if (touchkey_driver->is_bln_active) {
		i2c_touchkey_write(&touchkey_driver->backlight_off, 1);
	}
	mutex_unlock(&touchkey_driver->mutex);
}

static bool cypress_touchkey_enable_led_notification(void) {
	printk("[TKEY] BLN %s\n", __func__);

	mutex_lock(&touchkey_driver->mutex);

	if (touchkey_driver->is_bl_disabled || touchkey_driver->is_dead)
		return false;

	touchkey_on();
	touchkey_driver->is_bln_active = true;

	mutex_unlock(&touchkey_driver->mutex);

	return true;
}

static void cypress_touchkey_disable_led_notification(void) {
	printk("[TKEY] BLN %s\n", __func__);

	mutex_lock(&touchkey_driver->mutex);
	touchkey_driver->is_bln_active = false;
	mutex_unlock(&touchkey_driver->mutex);
}

static struct bln_implementation cypress_touchkey_bln = {
	.enable = cypress_touchkey_enable_led_notification,
	.disable = cypress_touchkey_disable_led_notification,
	.on = cypress_touchkey_enable_backlight,
	.off = cypress_touchkey_disable_backlight,
};
#endif

extern int mcsdl_download_binary_data(void);
static int i2c_touchkey_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct input_dev *input_dev;
	int err = 0;
#if defined (CONFIG_USA_MODEL_SGH_T989)||defined (CONFIG_USA_MODEL_SGH_I727)||defined (CONFIG_KOR_MODEL_SHV_E110S)\
|| defined (CONFIG_JPN_MODEL_SC_03D) || defined (CONFIG_USA_MODEL_SGH_I717) \
|| defined(CONFIG_KOR_MODEL_SHV_E160L)\
|| defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)\
|| defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)

	int touch_auto_calibration_on_off = 0;
	u8 data[6];
#endif
	printk("[TKEY] melfas i2c_touchkey_probe\n");

	touchkey_driver =
		kzalloc(sizeof(struct i2c_touchkey_driver), GFP_KERNEL);
	if (touchkey_driver == NULL) {
		dev_err(dev, "failed to create our state\n");
		return -ENOMEM;
	}

	touchkey_driver->client = client;
	touchkey_driver->client->irq = IRQ_TOUCHKEY_INT;
	strlcpy(touchkey_driver->client->name, "melfas-touchkey", I2C_NAME_SIZE);

	// i2c_set_clientdata(client, state);
	input_dev = input_allocate_device();

	if (!input_dev)
		return -ENOMEM;

	touchkey_driver->input_dev = input_dev;

	input_dev->name = DEVICE_NAME;
	input_dev->phys = "melfas-touchkey/input0";
	input_dev->id.bustype = BUS_HOST;

#if defined (CONFIG_KOR_MODEL_SHV_E110S)
	if(get_hw_rev() == 0x01) {	
	  	touchkey_keycode[1] = KEY_BACK;
	  	touchkey_keycode[2] = KEY_MENU;  	
  	} else if ((get_hw_rev() == 0x03)||(get_hw_rev() == 0x04)){
		touchkey_keycode[1] = KEY_MENU;
		touchkey_keycode[2] = KEY_HOME; 	
		touchkey_keycode[3] = KEY_BACK; 	
		touchkey_keycode[4] = KEY_SEARCH;	
	} else {
	  	touchkey_keycode[1] = KEY_MENU;
	  	touchkey_keycode[2] = KEY_BACK;	 
	}
#elif defined (CONFIG_KOR_SHV_E120L_WXGA)
	  	touchkey_keycode[1] = KEY_MENU;
	  	touchkey_keycode[2] = KEY_BACK; 
#elif defined(CONFIG_KOR_MODEL_SHV_E160L)
	if(get_hw_rev() >= 0x02) {	
		touchkey_keycode[1] = KEY_MENU;
		touchkey_keycode[2] = KEY_BACK; 	
	} else {
		touchkey_keycode[1] = KEY_MENU;
		touchkey_keycode[2] = KEY_BACK; 	
		}		
#elif  defined (CONFIG_JPN_MODEL_SC_03D)
	if (get_hw_rev() == 0x02)
	{
	  	touchkey_keycode[1] = KEY_MENU;
	  	touchkey_keycode[2] = KEY_HOME;  	
		touchkey_keycode[3] = KEY_BACK;  	
		touchkey_keycode[4] = KEY_SEARCH;  	
	}
	else
	{
		touchkey_keycode[1] = KEY_MENU;
		touchkey_keycode[2] = KEY_BACK; 	
	}
#elif  defined (CONFIG_EUR_MODEL_GT_I9210)
	if (get_hw_rev() != 0x05 )
	{
		touchkey_keycode[1] = KEY_MENU;
		touchkey_keycode[2] = KEY_BACK; 	
	}
#elif defined (CONFIG_JPN_MODEL_SC_05D)
//TODO Check HW REV for JPN
		touchkey_keycode[1] = KEY_MENU;
		touchkey_keycode[2] = KEY_BACK; 	
#endif

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_LED, input_dev->evbit);
	set_bit(LED_MISC, input_dev->ledbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(touchkey_keycode[1], input_dev->keybit);
	set_bit(touchkey_keycode[2], input_dev->keybit);
#if defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_JPN_MODEL_SC_03D) \
|| defined (CONFIG_KOR_MODEL_SHV_E110S) || defined (CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_USA_MODEL_SGH_T769)\
|| defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)\
|| defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
	set_bit(touchkey_keycode[3], input_dev->keybit);
	set_bit(touchkey_keycode[4], input_dev->keybit);
#endif	

	err = input_register_device(input_dev);
	if (err) {
		input_free_device(input_dev);
		return err;
	}

	touchkey_driver->is_powering_on = true;
	touchkey_driver->is_delay_led_on = false;
	touchkey_driver->is_backlight_on = true;
	touchkey_driver->is_key_pressed = false;
	touchkey_driver->is_bl_disabled = false;
	mutex_init(&touchkey_driver->mutex);
	set_backlight_onoff_values();

	//	gpio_pend_mask_mem = ioremap(INT_PEND_BASE, 0x10);  //temp ks

#ifdef CONFIG_HAS_EARLYSUSPEND
	//touchkey_driver->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING + 1;  -- works, but may be unstable
	//touchkey_driver->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;  -- not late enough
	//touchkey_driver->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING + 2;
	touchkey_driver->early_suspend.suspend = melfas_touchkey_early_suspend;
	touchkey_driver->early_suspend.resume = melfas_touchkey_early_resume;
	register_early_suspend(&touchkey_driver->early_suspend);
#endif

	err= request_threaded_irq( IRQ_TOUCHKEY_INT, touchkey_interrupt_handler, touchkey_interrupt_thread, IRQF_TRIGGER_FALLING, "touchkey_int", NULL);

	if (err) {
		printk(KERN_ERR "[TKEY] %s Can't allocate irq .. %d\n", __FUNCTION__, err);
		return -EBUSY;
	}

	touchkey_on();

	msleep(30);
	i2c_touchkey_read	(0x00, data, 6);
	touch_auto_calibration_on_off = (data[5] & 0x80)>>7;
	printk("[TKEY] after touchkey_auto_calibration result = %d \n",touch_auto_calibration_on_off);
	
#if defined(CONFIG_GENERIC_BLN)
	register_bln_implementation(&cypress_touchkey_bln);
#endif
	mutex_lock(&touchkey_driver->mutex);
	touchkey_driver->is_powering_on = false;
	mutex_unlock(&touchkey_driver->mutex);

	return 0;
}

static void init_hw(void)
{
	int rc;
	struct pm8058_gpio_cfg {
		int            gpio;
		struct pm_gpio cfg;
	};

#if defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)

	struct pm8058_gpio_cfg touchkey_int_cfg = 
	{
	  PM8058_GPIO_PM_TO_SYS(12), // id-1		
		{
			.direction      = PM_GPIO_DIR_IN,
			.pull           = PM_GPIO_PULL_NO,//PM_GPIO_PULL_NO,
			.vin_sel        = 2,
			.function       = PM_GPIO_FUNC_NORMAL,
			.inv_int_pol    = 0,
		},
	};
#else
	struct pm8058_gpio_cfg touchkey_int_cfg = 
	{
		13,
		{
			.direction		= PM_GPIO_DIR_IN,
			.pull			= PM_GPIO_PULL_NO,//PM_GPIO_PULL_NO,
			.vin_sel		= 2,
			.function		= PM_GPIO_FUNC_NORMAL,
			.inv_int_pol	= 0,
		},
	};
#endif

	#if defined(CONFIG_KOR_MODEL_SHV_E160L)
	msleep(200);
	#endif 

	rc = pm8xxx_gpio_config(touchkey_int_cfg.gpio, &touchkey_int_cfg.cfg);
	if (rc < 0) {
		pr_err("%s pmic gpio config failed\n", __func__);
		return;
	}
	
#if defined (CONFIG_KOR_MODEL_SHV_E110S)
	if (get_hw_rev() >= 0x06){
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_RISING);	
	} else { 
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);
	}
#elif defined (CONFIG_JPN_MODEL_SC_03D)
	if (get_hw_rev() >= 0x05){
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_RISING);
	} else { 
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);
	}
#elif defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
				
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);
					
#elif defined (CONFIG_EUR_MODEL_GT_I9210)
	if (get_hw_rev() >= 0x07){
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_RISING);	
	} else { 
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);
	}
#elif defined (CONFIG_USA_MODEL_SGH_I727)
	if (get_hw_rev() >= 0x0a){
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_RISING);	
	} else { 
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);
	}
#elif defined (CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)

		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);

#elif defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)
	if (get_hw_rev() >= 0x0d){
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_RISING);	
	} else { 
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);  
	}
#elif defined(CONFIG_KOR_MODEL_SHV_E160L)
	irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);

#elif defined (CONFIG_JPN_MODEL_SC_05D) 
	irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);

#else
	irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_LEVEL_LOW);
#endif
}

int touchkey_update_open(struct inode *inode, struct file *filp)
{
	return 0;
}

ssize_t touchkey_update_read(struct file * filp, char *buf, size_t count, loff_t * f_pos)
{
	char data[3] = { 0, };

	get_touchkey_firmware(data);
	put_user(data[1], buf);

	return 1;
}

int touchkey_update_release(struct inode *inode, struct file *filp)
{
	return 0;
}

struct file_operations touchkey_update_fops = {
	.owner = THIS_MODULE,
	.read = touchkey_update_read,
	.open = touchkey_update_open,
	.release = touchkey_update_release,
};

static struct miscdevice touchkey_update_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "melfas_touchkey",
	.fops = &touchkey_update_fops,
};

static ssize_t touch_version_read(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	char data[3] = { 0, };
	int count;

	mutex_lock(&touchkey_driver->mutex);
	init_hw();
	if (get_touchkey_firmware(data) != 0)
		i2c_touchkey_read(KEYCODE_REG, data, 3);
	count = sprintf(buf, "0x%x\n", data[1]);
	mutex_unlock(&touchkey_driver->mutex);

	printk("[TKEY] touch_version_read 0x%x\n", data[1]);
	return count;
}

static ssize_t touch_recommend_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	char data[3] = { 0, };
	int count;
	mutex_lock(&touchkey_driver->mutex);
#if defined (CONFIG_USA_MODEL_SGH_I727)
	if (get_hw_rev() >=0x0a)
		data[1] = 0x12;
	else
		data[1] = 0x07;
#elif defined (CONFIG_USA_MODEL_SGH_I717)
	data[1] = 0x04;
#elif defined (CONFIG_USA_MODEL_SGH_T989)
	if (get_hw_rev() >= 0x0d)
		data[1] = 0x13;
	else if (get_hw_rev() >= 0x09)
		data[1] = 0x11;
	else if (get_hw_rev() == 0x08)
		data[1] = 0x0f;
	else if (get_hw_rev() == 0x05)
		data[1] = 0x0c;
#endif

	count = sprintf(buf, "0x%x\n", data[1]);
	mutex_unlock(&touchkey_driver->mutex);

	printk("[TKEY] touch_recommend_read 0x%x\n", data[1]);
	return count;
}

extern int ISSP_main(int touchkey_pba_rev);
static int touchkey_update_status = 0;
#if defined(CONFIG_KOR_MODEL_SHV_E160L)
static int touchkey_downloading_status = 0;
#endif

void touchkey_update_func(struct work_struct *p)
{
	int retry = 10;
	touchkey_update_status = 1;
	printk("[TKEY] %s start\n", __FUNCTION__);
	return ;  //celox_01 temp

	while (retry--) {
		touchkey_update_status = 0;
		printk("[TKEY] touchkey_update succeeded\n");
		enable_irq(IRQ_TOUCHKEY_INT);
		return;
	}
	touchkey_update_status = -1;
	printk("[TKEY] touchkey_update failed\n");
	return;
}

static ssize_t touch_update_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	printk("[TKEY] touchkey firmware update \n");
	mutex_lock(&touchkey_driver->mutex);
	if (*buf == 'S') {
		disable_irq(IRQ_TOUCHKEY_INT);
		INIT_WORK(&touch_update_work, touchkey_update_func);
		queue_work(touchkey_wq, &touch_update_work);
	}
	mutex_unlock(&touchkey_driver->mutex);
	return size;
}

static ssize_t touch_update_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;

	mutex_lock(&touchkey_driver->mutex);
	printk("[TKEY] touch_update_read: touchkey_update_status %d\n", touchkey_update_status);

	if (touchkey_update_status == 0) {
		count = sprintf(buf, "PASS\n");
	} else if (touchkey_update_status == 1) {
		count = sprintf(buf, "Downloading\n");
	} else if (touchkey_update_status == -1) {
		count = sprintf(buf, "Fail\n");
	}
	mutex_unlock(&touchkey_driver->mutex);

	return count;
}

static ssize_t touch_led_control(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;

	mutex_lock(&touchkey_driver->mutex);

	if (strncmp(buf, "1", 1) == 0)
	{
		if (touchkey_driver->is_bl_disabled)
			goto unlock;

		touchkey_driver->is_backlight_on = true;
		if (touchkey_driver->is_powering_on || touchkey_driver->is_key_pressed) {
			dev_err(dev, "%s: delay led on \n", __func__);
			touchkey_driver->is_delay_led_on = true;
			goto unlock;
		}
		ret = i2c_touchkey_write(&touchkey_driver->backlight_on, 1);
		printk("[TKEY] Touch Key led ON\n");
	}
	else
	{
		touchkey_driver->is_backlight_on = false;
		if (touchkey_driver->is_powering_on || touchkey_driver->is_key_pressed) {
			dev_err(dev, "%s: delay led off\n", __func__);
			touchkey_driver->is_delay_led_on = true;
			goto unlock;
		}
		ret = i2c_touchkey_write(&touchkey_driver->backlight_off, 1);
		printk("[TKEY] Touch Key led OFF\n");
	}

	if (ret)
		dev_err(dev, "%s: touchkey led i2c failed\n", __func__);

unlock:
	mutex_unlock(&touchkey_driver->mutex);
	return size;
}

static ssize_t touchkey_enable_disable(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	// Intentionally not implemented
	return size;
}

static ssize_t set_touchkey_update_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/*TO DO IT */
	int count=0;
	int retry=3;

	mutex_lock(&touchkey_driver->mutex);
	touchkey_update_status = 1;

	while (retry--) {
#if defined (CONFIG_USA_MODEL_SGH_T989)
			if (ISSP_main(TOUCHKEY_PBA_REV_05) == 0) {
#else
			if (ISSP_main(TOUCHKEY_PBA_REV_NA) == 0) {
#endif
				printk(KERN_ERR"[TKEY] Touchkey_update succeeded\n");
				touchkey_update_status = 0;
				count=1;
				break;
			}
			printk(KERN_ERR"[TKEY] touchkey_update failed... retry...[From set_touchkey_update_show()] \n");
	}
	if (retry <= 0) {
			count=0;
			printk(KERN_ERR"[TKEY]Touchkey_update fail\n");
			touchkey_update_status = -1;
			return count;
	}

	init_hw();	/* after update, re initalize. */

	mutex_unlock(&touchkey_driver->mutex);

	return count;
}

static ssize_t set_touchkey_autocal_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count0, count1 = 0;

	/*TO DO IT */

	mutex_lock(&touchkey_driver->mutex);
	printk("[TKEY] called %s \n",__func__);	
	count0 = cypress_write_register(0x00, 0x50);
	count1 = cypress_write_register(0x03, 0x01);
	mutex_unlock(&touchkey_driver->mutex);

	// init_hw();	/* after update, re initalize. */
	return (count0&&count1);
}

static ssize_t set_touchkey_firm_version_read_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char data[3] = { 0, };
	int count;

	mutex_lock(&touchkey_driver->mutex);
	init_hw();
	if (get_touchkey_firmware(data) != 0) {
		i2c_touchkey_read(KEYCODE_REG, data, 3);
	}
	count = sprintf(buf, "0x%x\n", data[1]);
	mutex_unlock(&touchkey_driver->mutex);

	printk(KERN_DEBUG "[TKEY] touch_version_read 0x%x\n", data[1]);
	return count;
}

static ssize_t set_touchkey_firm_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;

	mutex_lock(&touchkey_driver->mutex);
	printk(KERN_DEBUG
	       "[TKEY] touch_update_read: touchkey_update_status %d\n",
	       touchkey_update_status);

	if (touchkey_update_status == 0) {
		count = sprintf(buf, "PASS\n");
	} else if (touchkey_update_status == 1) {
		count = sprintf(buf, "Downloading\n");
	} else if (touchkey_update_status == -1) {
		count = sprintf(buf, "Fail\n");
	}
	mutex_unlock(&touchkey_driver->mutex);
	return count;
}

static void change_touch_key_led_voltage(int vol_mv)
{
	struct regulator *tled_regulator;
	int ret ;

	vol_mv_level = vol_mv;

	tled_regulator = regulator_get(NULL, "8058_l12");
	if (IS_ERR(tled_regulator)) {
		pr_err("[TKEY] %s: failed to get resource %s\n", __func__,
				"touch_led");
		return;
	}
	ret = regulator_set_voltage(tled_regulator, vol_mv * 100000, vol_mv * 100000);
	if ( ret ) {
		printk("[TKEY] %s: error setting voltage\n", __func__);
	}

	regulator_put(tled_regulator);
}

static ssize_t brightness_control(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int data;

	mutex_lock(&touchkey_driver->mutex);
	if (sscanf(buf, "%d\n", &data) == 1) {
		printk(KERN_ERR "[TKEY] touch_led_brightness: %d \n", data);
		change_touch_key_led_voltage(data);
	} else {
		printk(KERN_ERR "[TKEY] touch_led_brightness Error\n");
	}
	mutex_unlock(&touchkey_driver->mutex);
	return size;
}

static ssize_t brightness_level_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;

	mutex_lock(&touchkey_driver->mutex);
	count = sprintf(buf, "%d\n", vol_mv_level);

	printk(KERN_DEBUG "[TKEY] Touch LED voltage = %d\n", vol_mv_level);
	mutex_unlock(&touchkey_driver->mutex);
	return count;
}

static DEVICE_ATTR(touchleds_disabled, S_IRUGO | S_IWUSR, touchleds_disabled_show, touchleds_disabled_store);
static DEVICE_ATTR(touch_version, S_IRUGO | S_IWUSR | S_IWGRP, touch_version_read, NULL);
static DEVICE_ATTR(touch_recommend, S_IRUGO | S_IWUSR | S_IWGRP, touch_recommend_read, NULL);
static DEVICE_ATTR(touch_update, S_IRUGO | S_IWUSR | S_IWGRP, touch_update_read, touch_update_write);
static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWGRP, NULL, touch_led_control);
static DEVICE_ATTR(enable_disable, S_IRUGO | S_IWUSR | S_IWGRP, NULL, touchkey_enable_disable);
static DEVICE_ATTR(touchkey_firm_update, S_IRUGO | S_IWUSR | S_IWGRP, set_touchkey_update_show, NULL);		/* firmware update */
static DEVICE_ATTR(touchkey_autocal_start, S_IRUGO | S_IWUSR | S_IWGRP, set_touchkey_autocal_show, NULL);		/* Autocal Start */
static DEVICE_ATTR(touchkey_firm_update_status, S_IRUGO | S_IWUSR | S_IWGRP, set_touchkey_firm_status_show, NULL);	/* firmware update status return */
static DEVICE_ATTR(touchkey_firm_version_phone, S_IRUGO | S_IWUSR | S_IWGRP, touch_recommend_read, NULL);/* PHONE*/	/* firmware version resturn in phone driver version */
static DEVICE_ATTR(touchkey_firm_version_panel, S_IRUGO | S_IWUSR | S_IWGRP, set_touchkey_firm_version_read_show, NULL);/*PART*/	/* firmware version resturn in touchkey panel version */
static DEVICE_ATTR(touchkey_brightness, S_IRUGO | S_IWUSR | S_IWGRP, brightness_level_show, brightness_control);


#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

static int __init touchkey_init(void)
{
	int ret = 0;
#if defined (CONFIG_KOR_MODEL_SHV_E110S) || defined(CONFIG_KOR_MODEL_SHV_E160L)
	int retry = 3;
#else
	int retry = 10;
#endif

	char data[3] = { 0, };

	printk("[TKEY] touchkey_init START \n");

#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("[TKEY] %s : LPM Charging Mode! return ENODEV!\n", __func__);
		return 0;
	}
#endif

	ret = misc_register(&touchkey_update_device);
	if (ret) {
		printk("[TKEY] %s misc_register fail\n", __FUNCTION__);
	}

	if (device_create_file(touchkey_update_device.this_device, &dev_attr_touch_version) < 0) {
		printk("%s device_create_file fail dev_attr_touch_version\n", __FUNCTION__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touch_version.attr.name);
	}
	if (device_create_file(touchkey_update_device.this_device, &dev_attr_touch_recommend) < 0) {
		printk("%s device_create_file fail dev_attr_touch_recommend\n", __FUNCTION__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touch_recommend.attr.name);
	}		

	if (device_create_file (touchkey_update_device.this_device, &dev_attr_touch_update) < 0) {
		printk("%s device_create_file fail dev_attr_touch_update\n", __FUNCTION__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touch_update.attr.name);
	}

	if (device_create_file(touchkey_update_device.this_device, &dev_attr_brightness) < 0) {
		printk("%s device_create_file fail dev_attr_touch_update\n", __FUNCTION__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_brightness.attr.name);
	}

	if (device_create_file(touchkey_update_device.this_device,&dev_attr_enable_disable) < 0) {
		printk("%s device_create_file fail dev_attr_touch_update\n", __FUNCTION__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_enable_disable.attr.name);
	}

	if (device_create_file(touchkey_update_device.this_device,&dev_attr_touchleds_disabled) < 0) {
		printk("%s device_create_file fail dev_attr_touchleds_disabled\n", __FUNCTION__);
		pr_err("Failed to create device file (%s)!\n", dev_attr_touchleds_disabled.attr.name);
	}

	sec_touchkey= device_create(sec_class, NULL, 0, NULL, "sec_touchkey");

	if (IS_ERR(sec_touchkey)) {
			printk("[TKEY] Failed to create device(sec_touchkey)!\n");
	}
	if (device_create_file(sec_touchkey, &dev_attr_touchkey_firm_update)< 0) {
		printk("Failed to create device file(%s)!\n", dev_attr_touchkey_firm_update.attr.name);
	}
	if (device_create_file(sec_touchkey, &dev_attr_touchkey_autocal_start)< 0) {
		printk("Failed to create device file(%s)!\n", dev_attr_touchkey_autocal_start.attr.name);
	}
	if (device_create_file(sec_touchkey, &dev_attr_touchkey_firm_update_status)< 0)	{
		printk("Failed to create device file(%s)!\n", dev_attr_touchkey_firm_update_status.attr.name);
	}
	if (device_create_file(sec_touchkey, &dev_attr_touchkey_firm_version_phone)< 0)	{
		printk("Failed to create device file(%s)!\n", dev_attr_touchkey_firm_version_phone.attr.name);
	}
	if (device_create_file(sec_touchkey, &dev_attr_touchkey_firm_version_panel)< 0)	{
		printk("Failed to create device file(%s)!\n", dev_attr_touchkey_firm_version_panel.attr.name);
	}	
	if (device_create_file(sec_touchkey, &dev_attr_touchkey_brightness)< 0)	{
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_touchkey_brightness.attr.name);
	}

	touchkey_wq = create_singlethread_workqueue("melfas_touchkey_wq");
	if (!touchkey_wq)
		return -ENOMEM;

	// init_hw();

#if defined (CONFIG_USA_MODEL_SGH_I727)
	if (get_hw_rev() >= 0x0a){
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_RISING);	
	} else { 
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);
	}
#elif defined (CONFIG_USA_MODEL_SGH_I717)
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);
#elif defined (CONFIG_USA_MODEL_SGH_T989)
	if (get_hw_rev() >= 0x0d){
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_RISING);	
	} else { 
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);
	}
	
	irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);
#else
	irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_LEVEL_LOW);
#endif

	while (retry--) {
		if (get_touchkey_firmware(data) == 0)	//melfas need delay for multiple read
			break;
		else
			printk("[TKEY] f/w read fail retry %d\n", retry);
	}

//	printk("[TKEY] %s F/W version: 0x%x, Module version:0x%x, HW_REV: 0x%x\n", __FUNCTION__, data[1], data[2], get_hw_rev());
	touch_version = data[1];
	retry = 3;
	
#if defined (CONFIG_USA_MODEL_SGH_T989)//new touchkey fpcb
	//update version "eclair/vendor/samsung/apps/Lcdtest/src/com/sec/android/app/lcdtest/touch_firmware.java"
	if ((data[1] == 0x01) && (data[2] < 0x05)) {
		while (retry--) {
			if (ISSP_main(TOUCHKEY_PBA_REV_NA) == 0) {
				printk("[TKEY] touchkey_update succeeded\n");
				break;
			}
			printk("[TKEY] touchkey_update failed... retry...\n");
		}
		init_hw();	//after update, re initalize.
	}
	else if (((data[1] != 0x0c) && (data[2] == 0x02) ) || ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))&& (get_hw_rev() ==0x05 )))
		{
		touchkey_pba_revision = TOUCHKEY_PBA_REV_02;
		while (retry--) {
			if (ISSP_main(TOUCHKEY_PBA_REV_02) == 0) {
				printk("[TKEY] touchkey_update succeeded_new\n");
				break;
			}
			printk("[TKEY] touchkey_update failed... retry...\n");
		}
		init_hw();	//after update, re initalize.
	}
	else if (((data[1] < 0x0f) && (data[2] == 0x03) )  || ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))&& (get_hw_rev() ==0x08 )))
		{
		touchkey_pba_revision = TOUCHKEY_PBA_REV_03;
		while (retry--) {
			if (ISSP_main(TOUCHKEY_PBA_REV_03) == 0) {
				printk("[TKEY] touchkey_update succeeded_new\n");
				break;
			}
			printk("[TKEY] touchkey_update failed... retry...\n");
		}
		init_hw();	//after update, re initalize.
	}
	else if (((data[1] < 0x11) && (data[2] == 0x04) )  || ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))&& (get_hw_rev() >=0x09 )&&(get_hw_rev() <0x0d )))
		{
		touchkey_pba_revision = TOUCHKEY_PBA_REV_04;
		while (retry--) {
			if (ISSP_main(TOUCHKEY_PBA_REV_04) == 0) {
				printk("[TKEY] touchkey_update succeeded_new\n");
				break;
			}
			printk("[TKEY] touchkey_update failed... retry...\n");
		}
		init_hw();	//after update, re initalize.
	}
	else if (((data[1] < 0x13) && (data[2] == 0x05) )  || ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))&& (get_hw_rev() >=0x0d )))
		{
		touchkey_pba_revision = TOUCHKEY_PBA_REV_05;
		while (retry--) {
			if (ISSP_main(TOUCHKEY_PBA_REV_05) == 0) {
				printk("[TKEY] touchkey_update succeeded_new\n");
				break;
			}
			printk("[TKEY] touchkey_update failed... retry...\n");
		}
		init_hw();	//after update, re initalize.
	}
#elif defined (CONFIG_USA_MODEL_SGH_I727)
	 if (((data[1] < 0x07) && (data[2] == 0x15))|| ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))&& ((get_hw_rev() >=0x05 )&& (get_hw_rev()<0x0a))))
{
		printk("[TKEY] %s : update 727 tkey...\n",__func__);	
		while (retry--) {
			if (ISSP_main(0) == 0) {
				printk("[TKEY] touchkey_update succeeded\n");
				break;
			}
			printk("[TKEY] touchkey_update failed... retry...\n");
		}
		init_hw();	//after update, re initalize.
		get_touchkey_firmware(data);
		printk("[TKEY] %s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__, data[1], data[2]);
	}
		else if (((data[1] == 0x09) && (data[2] == 0x18))|| ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))&& (get_hw_rev() >=0x0a )))
{
		printk("[TKEY] %s : update 727 tkey...\n",__func__);	
		while (retry--) {
			if (ISSP_main(0) == 0) {
				printk("[TKEY] touchkey_update succeeded\n");
				break;
			}
			printk("[TKEY] touchkey_update failed... retry...\n");
		}
		init_hw();	//after update, re initalize.
		get_touchkey_firmware(data);
		printk("[TKEY] %s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__, data[1], data[2]);
	}

#elif defined (CONFIG_USA_MODEL_SGH_I717)
	if (data[1] != 0x04)//(((data[1] != 0x04) && (data[2] <= 0x2))|| ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))))
	{
		printk("[TKEY] %s : update 727 tkey...\n",__func__);   
		while (retry--) {
			if (ISSP_main(0) == 0) {
				printk("[TKEY] touchkey_update succeeded\n");
				break;
			}
			printk("[TKEY] touchkey_update failed... retry...\n");
		}
		init_hw();  //after update, re initalize.
		get_touchkey_firmware(data);
		printk("[TKEY] %s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__, data[1], data[2]);
	}
	else if (((data[1] < 0x09) && (data[2] == 0x18))|| ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))&& (get_hw_rev() >=0x0a )))
	{
		printk("[TKEY] %s : update 727 tkey...\n",__func__);	
		while (retry--) {
			if (ISSP_main(0) == 0) {
				printk("[TKEY] touchkey_update succeeded\n");
				break;
			}
			printk("[TKEY] touchkey_update failed... retry...\n");
		}
		init_hw();	//after update, re initalize.
		get_touchkey_firmware(data);
		printk("[TKEY] %s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__, data[1], data[2]);
	}
#endif
	ret = i2c_add_driver(&touchkey_i2c_driver);

	if (ret) {
		printk ("[TKEY] melfas touch keypad registration failed, module not inserted.ret= %d\n", ret);
	}
	printk("[TKEY] touchkey_init END \n");
	return ret;
}

static void __exit touchkey_exit(void)
{
	printk("[TKEY] %s \n", __FUNCTION__);
	i2c_del_driver(&touchkey_i2c_driver);
	misc_deregister(&touchkey_update_device);
	if (touchkey_wq)
		destroy_workqueue(touchkey_wq);
}

late_initcall(touchkey_init);
module_exit(touchkey_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("@@@");
MODULE_DESCRIPTION("melfas touch keypad");
