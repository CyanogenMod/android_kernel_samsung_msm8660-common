/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
//#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
//#include <mach/regs-gpio.h>
//#include <plat/gpio-cfg.h>
#include <asm/gpio.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/earlysuspend.h>
#include <asm/io.h>
#if defined(CONFIG_GENERIC_BLN)
#include <linux/bln.h>
#endif
#ifdef CONFIG_CPU_FREQ
//#include <mach/cpu-freq-v210.h>  //temp ks
#endif
//#include <mach/max8998_function.h>
#include <mach/board-msm8660.h>

#include "cypress-touchkey.h"
#include <linux/regulator/consumer.h>

/*
Cypress touchkey register
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
#define DEVICE_NAME "sec_touchkey"

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
static int touchkey_pressed = 0;
static int vol_mv_level = 33;

#if defined(CONFIG_KOR_MODEL_SHV_E160L)
unsigned char data_mdule_rev;
#endif

//NAGSM_Android_SEL_Kernel_Aakash_20100320
#ifdef CONFIG_S5PC110_T959_BOARD
static int melfas_evt_enable_status = 1;
static ssize_t melfas_evt_status_show(struct device *dev, struct device_attribute *attr, char *sysfsbuf)
{
	return sprintf(sysfsbuf, "%d\n", melfas_evt_enable_status);
}

static ssize_t melfas_evt_status_store(struct device *dev, struct device_attribute *attr,const char *sysfsbuf, size_t size)
{
	mutex_lock(&touchkey_driver->mutex);
	sscanf(sysfsbuf, "%d", &melfas_evt_enable_status);
	mutex_unlock(&touchkey_driver->mutex);
	return size;
}

static DEVICE_ATTR(melfasevtcntrl, S_IRUGO | S_IWUSR, melfas_evt_status_show, melfas_evt_status_store);
#endif
//NAGSM_Android_SEL_Kernel_Aakash_20100320

static u16 menu_sensitivity = 0;
static u16 home_sensitivity = 0;
static u16 back_sensitivity = 0;
static u16 search_sensitivity = 0;
static u16 raw_data0 = 0;
static u16 raw_data1 = 0;
static u16 raw_data2 = 0;
static u16 raw_data3 = 0;
static u8 idac0 = 0;
static u8 idac1 = 0;
static u8 idac2 = 0;
static u8 idac3 = 0;
static int touchkey_enable = 0;
extern int touch_is_pressed;

#if defined (CONFIG_EPEN_WACOM_G5SP)
extern int wacom_is_pressed;
#endif

#if defined (CONFIG_JPN_MODEL_SC_03D)
static u8 firm_version = 0;
#endif

struct i2c_touchkey_driver {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct early_suspend early_suspend;
	struct mutex mutex;
	bool is_bln_active;
};
struct i2c_touchkey_driver *touchkey_driver = NULL;
struct work_struct touchkey_work;
struct workqueue_struct *touchkey_wq;

struct work_struct touch_update_work;
struct delayed_work touch_resume_work;
#if defined(CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_KOR_MODEL_SHV_E160L)
static int touchkey_auto_calibration(int autocal_on_off);
#else
static void touchkey_auto_calibration(int autocal_on_off);
#endif

#if defined (DEBUG_TKEY_RELEASE_DATA)
static bool g_debug_switch = true;
#else
static bool g_debug_switch = false;
#endif

#if defined(CONFIG_USA_MODEL_SGH_I717)
static bool Q1_debug_msg = false;
#endif


static const struct i2c_device_id sec_touchkey_id[] = {
	{"sec_touchkey", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sec_touchkey_id);

static void init_hw(void);
static int i2c_touchkey_probe(struct i2c_client *client, const struct i2c_device_id *id);
extern int get_touchkey_firmware(char *version);
static int touchkey_led_status = 0;
static int touchled_cmd_reversed=0;
extern int tkey_vdd_enable(int onoff);
extern int tkey_led_vdd_enable(int onoff);
#if defined (CONFIG_KOR_MODEL_SHV_E160L)\
     || defined (CONFIG_KOR_MODEL_SHV_E110S)
static int press_check = 0;
#endif
#if defined (CONFIG_KOR_MODEL_SHV_E160L)
static int touchkey_connected = 0;
#endif

struct i2c_driver touchkey_i2c_driver = {
	.driver = {
		.name = "sec_touchkey_driver",
		.owner	= THIS_MODULE,
	},
	.id_table = sec_touchkey_id,
	.probe = i2c_touchkey_probe,
};

static int touchkey_debug_count = 0;
static char touchkey_debug[104];
static int touch_version = 0;
static void set_touchkey_debug(char value)
{
	if (touchkey_debug_count == 100)
		touchkey_debug_count = 0;
	touchkey_debug[touchkey_debug_count] = value;
	touchkey_debug_count++;
}

static int i2c_touchkey_read(u8 reg, u8 * val, unsigned int len)
{
	int err   = 0;
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
		if (err >= 0) {
			return 0;
		}
		printk(KERN_DEBUG "[TKEY] %s %d i2c transfer error \n", __func__, __LINE__);	/* add by inter.park */
		mdelay(10);
	}
	return err;
}

static int i2c_touchkey_write(u8 * val, unsigned int len)
{
	int err = 0;
	struct i2c_msg msg[1];
	int retry = 2;

	if ((touchkey_driver == NULL || touchkey_enable != 1)
#if defined(CONFIG_GENERIC_BLN)
		&& (!touchkey_driver->is_bln_active)
#endif
	) {
		printk(KERN_DEBUG "[TKEY] touchkey is not enabled.W\n");
		return -ENODEV;
	}

	while (retry--) {
		msg->addr = touchkey_driver->client->addr;
		msg->flags = I2C_M_WR;
		msg->len = len;
		msg->buf = val;
		err = i2c_transfer(touchkey_driver->client->adapter, msg, 1);
		if (err >= 0)
			return 0;

		printk(KERN_DEBUG "[TKEY] [%s] %d i2c transfer error\n", __func__, __LINE__);
		mdelay(10);
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

//extern unsigned int touch_state_val;
//extern void TSP_forced_release(void);
#if defined (CONFIG_KOR_MODEL_SHV_E110S)|| defined (CONFIG_JPN_MODEL_SC_03D) \
|| defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_I727) \
|| defined (CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_KOR_MODEL_SHV_E160L) \
|| defined(CONFIG_USA_MODEL_SGH_I757) || defined (CONFIG_USA_MODEL_SGH_T769) \
|| defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined(CONFIG_CAN_MODEL_SGH_I757M) || defined(CONFIG_JPN_MODEL_SC_05D)
extern unsigned int  get_hw_rev(void);
#endif

int is_touchkey_available(void)
{
	int ret = 1;
#if defined (CONFIG_EPEN_WACOM_G5SP)
if( (wacom_is_pressed == 1)||(touch_is_pressed == 1)) ret = 0;
#else
	if( touch_is_pressed == 1 ) ret = 0;
#endif
	return ret;
}

void touchkey_work_func(struct work_struct *p)
{
	u8 data[3];
	int ret;
	int retry = 10;

	mutex_lock(&touchkey_driver->mutex);
	set_touchkey_debug('a');
//	printk("[TKEY] INPIN %d\n",gpio_get_value_cansleep(GPIO_TOUCHKEY));

		ret = i2c_touchkey_read(KEYCODE_REG, data, 1);
		printk("[0]%d [1]%d [2]%d\n", data[0],data[1], data[2]);
		set_touchkey_debug(data[0]);

		if ((data[0] & ESD_STATE_BIT) || (ret != 0)) {
			printk("[TKEY] ESD_STATE_BIT set or I2C fail: data: %d, retry: %d\n", data[0], retry);
			//releae key
			input_report_key(touchkey_driver->input_dev, touchkey_keycode[1], 0);
			input_report_key(touchkey_driver->input_dev, touchkey_keycode[2], 0);
			retry = 10;

			while (retry--) {
				mdelay(300);
				init_hw();
				if (i2c_touchkey_read(KEYCODE_REG, data, 3) >= 0) {
					printk("[TKEY] %s touchkey init success\n", __func__);
					set_touchkey_debug('O');
					enable_irq(IRQ_TOUCHKEY_INT);
                                        mutex_unlock(&touchkey_driver->mutex);
					return;
				}
				printk("[TKEY] %s %d i2c transfer error retry = %d\n", __func__, __LINE__, retry);
			}

			//touchkey die , do not enable touchkey
			//enable_irq(IRQ_TOUCH_INT);
			touchkey_enable = -1;
			printk("[TKEY] %s touchkey died\n", __func__);
			set_touchkey_debug('D');
                        mutex_unlock(&touchkey_driver->mutex);
			return;
		}

		if (data[0] & UPDOWN_EVENT_BIT) {
			input_report_key(touchkey_driver->input_dev, touchkey_keycode[data[0] & KEYCODE_BIT], 0);
			input_sync(touchkey_driver->input_dev);
			#ifndef CONFIG_USA_MODEL_SGH_T989
			printk(KERN_DEBUG "[TKEY] touchkey release keycode:%d \n", touchkey_keycode[data[0] & KEYCODE_BIT]);
			#endif
		} else {
			if (touch_is_pressed) {
				printk(KERN_DEBUG "[TKEY] touchkey pressed but don't send event because touch is pressed. \n");
				set_touchkey_debug('P');
			} else {
				if ((data[0] & KEYCODE_BIT) == 2) {	// if back key is pressed, release multitouch
				}
				input_report_key(touchkey_driver->input_dev, touchkey_keycode[data[0] & KEYCODE_BIT], 1);
				input_sync(touchkey_driver->input_dev);
				#ifndef CONFIG_USA_MODEL_SGH_T989
				printk(KERN_DEBUG "[TKEY] touchkey press keycode:%d \n", touchkey_keycode[data[0] & KEYCODE_BIT]);
				#endif
			}
		}

	//clear interrupt
	printk("[TKEY] %s: END \n", __func__);
	set_touchkey_debug('A');
        mutex_unlock(&touchkey_driver->mutex);
	enable_irq(IRQ_TOUCHKEY_INT);
}

void touchkey_resume_func(struct work_struct *p)
{
//	int err = 0;
//	int rc = 0;

	enable_irq(IRQ_TOUCHKEY_INT);
	touchkey_enable = 1;
	msleep(50);

#if defined (CONFIG_USA_MODEL_SGH_T989)||defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_I717)\
	|| defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)\
	|| defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
    touchkey_auto_calibration(1/*on*/);
#elif defined (CONFIG_KOR_MODEL_SHV_E110S)
	if (get_hw_rev() >= 0x02)
		touchkey_auto_calibration(1/*on*/);
#elif defined (CONFIG_JPN_MODEL_SC_03D)
	if (get_hw_rev() >= 0x02)
		touchkey_auto_calibration(1/*on*/);
#endif

#if 0
	{
		// temporary code for touchkey led
		int int_data = 0x10;
		msleep(100);
		printk("[TKEY] i2c_touchkey_write : key backligh on\n");
		i2c_touchkey_write((u8*)&int_data, 1);
	}
#endif
}

static irqreturn_t touchkey_interrupt(int irq, void *dummy)  // ks 79 - threaded irq(becuase of pmic gpio int pin)-> when reg is read in work_func, data0 is always release. so temporarily move the work_func to threaded irq.
{
    u8 data[3];
    int ret;
    int retry = 10;

    mutex_lock(&touchkey_driver->mutex);

    set_touchkey_debug('I');
    disable_irq_nosync(IRQ_TOUCHKEY_INT);

    #if defined(CONFIG_KOR_MODEL_SHV_E160L) // Temporary Code
    //if (get_hw_rev() <= 0x04){
        tkey_vdd_enable(1);
    //}
    #endif

	set_touchkey_debug('a');
	ret = i2c_touchkey_read(KEYCODE_REG, data, 1);

//	if(g_debug_switch)
		printk("[TKEY] DATA0 %d\n", data[0]);

	#if defined(CONFIG_USA_MODEL_SGH_I717)
	if(Q1_debug_msg)
		printk("[TKEY] DATA0 %d\n", data[0]);
	#endif

    #if defined(CONFIG_KOR_MODEL_SHV_E160L) // Temporary Code
	if (get_hw_rev() <= 0x04){
        if (data[0] > 80)  {
            data[0] = data[0] - 80;
            printk("[TKEY] DATA0 change [%d] \n", data[0]);
        }
    }
    #endif

	set_touchkey_debug(data[0]);
	if ((data[0] & ESD_STATE_BIT) || (ret != 0)) {
		printk("[TKEY] ESD_STATE_BIT set or I2C fail: data: %d, retry: %d\n", data[0], retry);

		//releae key
		input_report_key(touchkey_driver->input_dev, touchkey_keycode[1], 0);
		input_report_key(touchkey_driver->input_dev, touchkey_keycode[2], 0);
		retry = 10;

		while (retry--) {
			mdelay(300);
			init_hw();
            if (i2c_touchkey_read(KEYCODE_REG, data, 3) >= 0) {
                printk("[TKEY] %s touchkey init success\n", __func__);
				set_touchkey_debug('O');
                                mutex_unlock(&touchkey_driver->mutex);
				enable_irq(IRQ_TOUCHKEY_INT);
				return IRQ_NONE;
			}
            printk("[TKEY] %s %d i2c transfer error retry = %d\n", __func__, __LINE__, retry);
		}
		//touchkey die , do not enable touchkey
		//enable_irq(IRQ_TOUCH_INT);
		touchkey_enable = -1;
		printk("[TKEY] %s touchkey died\n", __func__);
		set_touchkey_debug('D');
                mutex_unlock(&touchkey_driver->mutex);
		return IRQ_NONE;
	}

#if defined (CONFIG_KOR_MODEL_SHV_E160L)\
|| defined (CONFIG_KOR_MODEL_SHV_E110S)
	if (data[0] & UPDOWN_EVENT_BIT) {
		if(press_check == touchkey_keycode[data[0] & KEYCODE_BIT]){
			input_report_key(touchkey_driver->input_dev, touchkey_keycode[data[0] & KEYCODE_BIT], 0);
			touchkey_pressed &= ~(1 << (data[0] & KEYCODE_BIT));
			input_sync(touchkey_driver->input_dev);
//			if(g_debug_switch)
#ifndef CONFIG_USA_MODEL_SGH_T989
				printk(KERN_DEBUG "touchkey release keycode:%d \n", touchkey_keycode[data[0] & KEYCODE_BIT]);
#endif
				
		}else{
			input_report_key(touchkey_driver->input_dev, press_check, 0);
	        }
			press_check = 0;
	} else {
		if (touch_is_pressed) {
			printk(KERN_DEBUG "touchkey pressed but don't send event because touch is pressed. \n");
			set_touchkey_debug('P');
		} else {
			if ((data[0] & KEYCODE_BIT) == 2) {	// if back key is pressed, release multitouch
			}
			input_report_key(touchkey_driver->input_dev, touchkey_keycode[data[0] & KEYCODE_BIT], 1);
			touchkey_pressed |= (1 << (data[0] & KEYCODE_BIT));
			input_sync(touchkey_driver->input_dev);
			press_check = touchkey_keycode[data[0] & KEYCODE_BIT];
//			if(g_debug_switch)
                                #ifndef CONFIG_USA_MODEL_SGH_T989
				printk(KERN_DEBUG "touchkey press keycode:%d \n", touchkey_keycode[data[0] & KEYCODE_BIT]);
				#endif
		}
	}
#else
	if (data[0] & UPDOWN_EVENT_BIT) {
		input_report_key(touchkey_driver->input_dev, touchkey_keycode[data[0] & KEYCODE_BIT], 0);
		touchkey_pressed &= ~(1 << (data[0] & KEYCODE_BIT));
		input_sync(touchkey_driver->input_dev);

//		if(g_debug_switch)
#ifndef CONFIG_USA_MODEL_SGH_T989
			printk(KERN_DEBUG "touchkey release keycode:%d \n", touchkey_keycode[data[0] & KEYCODE_BIT]);
#endif
	#if defined(CONFIG_USA_MODEL_SGH_I717)
	#ifndef CONFIG_USA_MODEL_SGH_T989
		if(Q1_debug_msg)
			printk(KERN_DEBUG "touchkey release keycode:%d \n", touchkey_keycode[data[0] & KEYCODE_BIT]);
	#endif
	#endif

	} else {
		if (touch_is_pressed) {
			printk(KERN_DEBUG "touchkey pressed but don't send event because touch is pressed. \n");
			set_touchkey_debug('P');
		} else {
			if ((data[0] & KEYCODE_BIT) == 2) {	// if back key is pressed, release multitouch
			}
			input_report_key(touchkey_driver->input_dev, touchkey_keycode[data[0] & KEYCODE_BIT], 1);
			touchkey_pressed |= (1 << (data[0] & KEYCODE_BIT));
			input_sync(touchkey_driver->input_dev);

//			if(g_debug_switch)
                                #ifndef CONFIG_USA_MODEL_SGH_T989 
				printk(KERN_DEBUG "touchkey press keycode:%d \n", touchkey_keycode[data[0] & KEYCODE_BIT]);
				#endif

		#if defined(CONFIG_USA_MODEL_SGH_I717)
//			if(Q1_debug_msg)      
                                #ifndef CONFIG_USA_MODEL_SGH_T989
				printk(KERN_DEBUG "touchkey press keycode:%d \n", touchkey_keycode[data[0] & KEYCODE_BIT]);
				#endif
		#endif
		}
	}
#endif
	set_touchkey_debug('A');
        mutex_unlock(&touchkey_driver->mutex);
	enable_irq(IRQ_TOUCHKEY_INT);
    //queue_work(touchkey_wq, &touchkey_work);
	return IRQ_HANDLED;
}

#if defined(CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_KOR_MODEL_SHV_E160L)
// mutex is assumed to be locked before calling this function
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
			printk(KERN_ERR"[TouchKey]i2c read fail.\n");
			return ret;
		}
		printk(KERN_DEBUG "[TouchKey] touchkey_autocalibration :data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",data[0],data[1],data[2],data[3]);

		/* Send autocal Command */
		data[0] = 0x50;
		data[3] = 0x01;

		count = i2c_touchkey_write(data, 4);

		msleep(100);

		/* Check autocal status*/
		ret = i2c_touchkey_read(KEYCODE_REG, data, 6);

		if((data[5] & 0x80)) {
			printk(KERN_DEBUG "[Touchkey] autocal Enabled\n");
			break;
		}
		else
			printk(KERN_DEBUG "[Touchkey] autocal disabled, retry %d\n", retry);

		retry = retry + 1;
	}

	if( retry == 3 )
		printk(KERN_DEBUG "[Touchkey] autocal failed\n");

	return count;


}
#else
// mutex is assumed to be locked before calling this function
static void touchkey_auto_calibration(int autocal_on_off)
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
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sec_touchkey_early_suspend(struct early_suspend *h)
{
    int index =0;
#if defined (CONFIG_USA_MODEL_SGH_I717)
    int ret = 0;
    /*signed char int_data[] ={0x80};*/
#endif
    mutex_lock(&touchkey_driver->mutex);

    touchkey_enable = 0;
    set_touchkey_debug('S');
    printk(KERN_DEBUG "sec_touchkey_early_suspend\n");

    disable_irq(IRQ_TOUCHKEY_INT);

#if defined (CONFIG_USA_MODEL_SGH_I717)
    ret = cancel_work_sync(&touchkey_work);
    if (ret) {
	    printk(KERN_DEBUG "[Touchkey] enable_irq ret = %d\n", ret);
	    enable_irq(IRQ_TOUCHKEY_INT);
    }
#endif

#if defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)
	if (get_hw_rev() >= 0x0d){
		tkey_vdd_enable(0);
		tkey_led_vdd_enable(0);
		gpio_direction_output(GPIO_TOUCHKEY_SCL, 0);
		gpio_free(GPIO_TOUCHKEY_SCL);
		gpio_direction_output(GPIO_TOUCHKEY_SDA, 0);
		gpio_free(GPIO_TOUCHKEY_SDA);
		}
#elif defined(CONFIG_USA_MODEL_SGH_I577)|| defined(CONFIG_CAN_MODEL_SGH_I577R)
		tkey_vdd_enable(0);
		tkey_led_vdd_enable(0);
		gpio_direction_output(GPIO_TOUCHKEY_SCL, 0);
		gpio_free(GPIO_TOUCHKEY_SCL);
		gpio_direction_output(GPIO_TOUCHKEY_SDA, 0);
		gpio_free(GPIO_TOUCHKEY_SDA);
#elif defined (CONFIG_EUR_MODEL_GT_I9210)
	if (get_hw_rev() >= 0x07){
		tkey_vdd_enable(0);
		tkey_led_vdd_enable(0);
		gpio_direction_output(GPIO_TOUCHKEY_SCL, 0);
		gpio_free(GPIO_TOUCHKEY_SCL);
		gpio_direction_output(GPIO_TOUCHKEY_SDA, 0);
		gpio_free(GPIO_TOUCHKEY_SDA);
		}
#elif defined (CONFIG_USA_MODEL_SGH_I727)
	if (get_hw_rev() >= 0x0a){
		tkey_vdd_enable(0);
		tkey_led_vdd_enable(0);
		gpio_direction_output(GPIO_TOUCHKEY_SCL, 0);
		gpio_free(GPIO_TOUCHKEY_SCL);
		gpio_direction_output(GPIO_TOUCHKEY_SDA, 0);
		gpio_free(GPIO_TOUCHKEY_SDA);
		}
#elif defined (CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
		tkey_vdd_enable(0);
		tkey_led_vdd_enable(0);
		gpio_direction_output(GPIO_TOUCHKEY_SCL, 0);
		gpio_free(GPIO_TOUCHKEY_SCL);
		gpio_direction_output(GPIO_TOUCHKEY_SDA, 0);
		gpio_free(GPIO_TOUCHKEY_SDA);
#elif defined (CONFIG_KOR_MODEL_SHV_E110S)
	if (get_hw_rev() >= 0x04){
		tkey_vdd_enable(0);
		tkey_led_vdd_enable(0);
		gpio_direction_output(GPIO_TOUCHKEY_SCL, 0);
		gpio_free(GPIO_TOUCHKEY_SCL);
		gpio_direction_output(GPIO_TOUCHKEY_SDA, 0);
		gpio_free(GPIO_TOUCHKEY_SDA);
		}
#elif defined(CONFIG_KOR_MODEL_SHV_E160L)
		tkey_vdd_enable(0);
		gpio_direction_output(GPIO_TOUCHKEY_SCL, 0);
		gpio_free(GPIO_TOUCHKEY_SCL);
		gpio_direction_output(GPIO_TOUCHKEY_SDA, 0);
		gpio_free(GPIO_TOUCHKEY_SDA);
#elif defined (CONFIG_JPN_MODEL_SC_03D)
	if (get_hw_rev() >= 0x02){
		tkey_vdd_enable(0);
		tkey_led_vdd_enable(0);
		gpio_direction_output(GPIO_TOUCHKEY_SCL, 0);
		gpio_free(GPIO_TOUCHKEY_SCL);
		gpio_direction_output(GPIO_TOUCHKEY_SDA, 0);
		gpio_free(GPIO_TOUCHKEY_SDA);
		}
#elif defined(CONFIG_JPN_MODEL_SC_05D)
		tkey_vdd_enable(0);
		gpio_direction_output(GPIO_TOUCHKEY_SCL, 0);
		gpio_free(GPIO_TOUCHKEY_SCL);
		gpio_direction_output(GPIO_TOUCHKEY_SDA, 0);
		gpio_free(GPIO_TOUCHKEY_SDA);		
#endif
	for (index = 1; index< sizeof(touchkey_keycode)/sizeof(*touchkey_keycode); index++)
	{
		if(touchkey_pressed & (1<<index))
		{
			input_report_key(touchkey_driver->input_dev, touchkey_keycode[index], 0);
			input_sync(touchkey_driver->input_dev);
			printk ("[TEKY] suspend: release unreleased keycode: [%d]\n", touchkey_keycode[index]);
		}
	}
	touchkey_pressed = 0;
	touchkey_enable = 0;

#if defined (CONFIG_KOR_MODEL_SHV_E160L)\
|| defined (CONFIG_KOR_MODEL_SHV_E110S)
	press_check = 0;
#endif
    mutex_unlock(&touchkey_driver->mutex);
}

static void sec_touchkey_early_resume(struct early_suspend *h)
{
#if defined (CONFIG_EUR_MODEL_GT_I9210) || defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_T769) || defined (CONFIG_USA_MODEL_SGH_T989)
 	int ret =0;
#endif

	set_touchkey_debug('R');
	printk(KERN_DEBUG "[TKEY] sec_touchkey_early_resume\n");

	if (touchkey_enable < 0) {
		printk("[TKEY] %s touchkey_enable: %d\n", __FUNCTION__, touchkey_enable);
		return;
	}

	mutex_lock(&touchkey_driver->mutex);

#if defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)
	if (get_hw_rev() >= 0x0d){
		tkey_vdd_enable(1);
		ret = gpio_request(GPIO_TOUCHKEY_SCL, "TKEY_SCL");
		if (ret) {
				pr_err("%s: request GPIO %s err %d.", __func__, "TKEY_SCL", ret);
		}
		gpio_direction_input(GPIO_TOUCHKEY_SCL);
		ret = gpio_request(GPIO_TOUCHKEY_SDA, "TKEY_SDA");
		if (ret) {
				pr_err("%s: request GPIO %s err %d.", __func__, "TKEY_SDA", ret);
		}
		gpio_direction_input(GPIO_TOUCHKEY_SDA);
		}
#elif defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)

			tkey_vdd_enable(1);
			ret = gpio_request(GPIO_TOUCHKEY_SCL, "TKEY_SCL");
			if (ret) {
				pr_err("%s: request GPIO %s err %d.", __func__, "TKEY_SCL", ret);
			}
			gpio_direction_input(GPIO_TOUCHKEY_SCL);
			ret = gpio_request(GPIO_TOUCHKEY_SDA, "TKEY_SDA");
			if (ret) {
				pr_err("%s: request GPIO %s err %d.", __func__, "TKEY_SDA", ret);
			}
			gpio_direction_input(GPIO_TOUCHKEY_SDA);
#elif defined (CONFIG_EUR_MODEL_GT_I9210)
	if (get_hw_rev() >= 0x07){
		tkey_vdd_enable(1);
		ret = gpio_request(GPIO_TOUCHKEY_SCL, "TKEY_SCL");
		if (ret) {
			pr_err("%s: request GPIO %s err %d.", __func__, "TKEY_SCL", ret);
		}
		gpio_direction_input(GPIO_TOUCHKEY_SCL);
		ret = gpio_request(GPIO_TOUCHKEY_SDA, "TKEY_SDA");
		if (ret) {
			pr_err("%s: request GPIO %s err %d.", __func__, "TKEY_SDA", ret);
		}
		gpio_direction_input(GPIO_TOUCHKEY_SDA);
	}
#elif defined (CONFIG_USA_MODEL_SGH_I727)
	if (get_hw_rev() >= 0x06){
		tkey_vdd_enable(1);
		gpio_request(GPIO_TOUCHKEY_SCL, "TKEY_SCL");
		gpio_direction_input(GPIO_TOUCHKEY_SCL);
		gpio_request(GPIO_TOUCHKEY_SDA, "TKEY_SDA");
		gpio_direction_input(GPIO_TOUCHKEY_SDA);
		}
#elif defined (CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)

		tkey_vdd_enable(1);
		gpio_request(GPIO_TOUCHKEY_SCL, "TKEY_SCL");
		gpio_direction_input(GPIO_TOUCHKEY_SCL);
		gpio_request(GPIO_TOUCHKEY_SDA, "TKEY_SDA");
		gpio_direction_input(GPIO_TOUCHKEY_SDA);

#elif defined (CONFIG_KOR_MODEL_SHV_E110S)
	if (get_hw_rev() >= 0x04){
		tkey_vdd_enable(1);
		gpio_request(GPIO_TOUCHKEY_SCL, "TKEY_SCL");
		gpio_direction_input(GPIO_TOUCHKEY_SCL);
		gpio_request(GPIO_TOUCHKEY_SDA, "TKEY_SDA");
		gpio_direction_input(GPIO_TOUCHKEY_SDA);
		}
#elif defined(CONFIG_KOR_MODEL_SHV_E160L) || defined(CONFIG_JPN_MODEL_SC_05D)
		tkey_vdd_enable(1);
		gpio_request(GPIO_TOUCHKEY_SCL, "TKEY_SCL");
		gpio_direction_input(GPIO_TOUCHKEY_SCL);
		gpio_request(GPIO_TOUCHKEY_SDA, "TKEY_SDA");
		gpio_direction_input(GPIO_TOUCHKEY_SDA);
#elif defined (CONFIG_JPN_MODEL_SC_03D)
	if (get_hw_rev() >= 0x02){
		tkey_vdd_enable(1);
		gpio_request(GPIO_TOUCHKEY_SCL, "TKEY_SCL");
		gpio_direction_input(GPIO_TOUCHKEY_SCL);
		gpio_request(GPIO_TOUCHKEY_SDA, "TKEY_SDA");
		gpio_direction_input(GPIO_TOUCHKEY_SDA);
		}
#endif // defined (CONFIG_USA_MODEL_SGH_T989)||defined (CONFIG_USA_MODEL_SGH_I727)
	init_hw();

if(touchled_cmd_reversed) {
			touchled_cmd_reversed = 0;
#if defined (CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577)\
	|| defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_KOR_MODEL_SHV_E160L)
			msleep(100);
#endif
	//		msleep(300);
			if(!touchkey_enable )
				touchkey_enable = 1;
			i2c_touchkey_write((u8*)&touchkey_led_status, 1);
			printk("[TKEY] LED RESERVED !! LED returned on touchkey_led_status = %d\n", touchkey_led_status);
	}
#if defined (CONFIG_USA_MODEL_SGH_I717)
	else {
		tkey_vdd_enable(0);
		msleep(100);
		tkey_vdd_enable(1);
		gpio_request(GPIO_TOUCHKEY_SCL, "TKEY_SCL");
		gpio_direction_input(GPIO_TOUCHKEY_SCL);
		gpio_request(GPIO_TOUCHKEY_SDA, "TKEY_SDA");
		gpio_direction_input(GPIO_TOUCHKEY_SDA);
		init_hw();

		msleep(100);
		if(!touchkey_enable )
			touchkey_enable = 1;
		i2c_touchkey_write((u8*)&touchkey_led_status, 1);
		printk("[TKEY] NOT RESERVED!! LED returned on touchkey_led_status = %d\n", touchkey_led_status);
	}
#endif

#if defined (CONFIG_KOR_MODEL_SHV_E110S)
			if (get_hw_rev() >=0x04 ){
				tkey_led_vdd_enable(1);
			}
#elif defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
				tkey_led_vdd_enable(1);

#elif defined(CONFIG_KOR_MODEL_SHV_E160L)
			if (get_hw_rev() >=0x02){
				tkey_led_vdd_enable(1);
			}
#elif defined (CONFIG_JPN_MODEL_SC_03D)
			if (get_hw_rev() >=0x02){
				tkey_led_vdd_enable(1);
			}
#elif defined (CONFIG_EUR_MODEL_GT_I9210)
			if (get_hw_rev() >=0x07){
				tkey_led_vdd_enable(1);
			}
#elif defined (CONFIG_USA_MODEL_SGH_I727)
			if (get_hw_rev() >=0x0a){
				tkey_led_vdd_enable(1);
			}
#elif defined (CONFIG_USA_MODEL_SGH_I717) 
			if (false){
				tkey_led_vdd_enable(1);
			}

#elif defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
				tkey_led_vdd_enable(1);

#elif defined (CONFIG_USA_MODEL_SGH_T989)|| defined (CONFIG_USA_MODEL_SGH_T769)
			if (get_hw_rev() >=0x0d){		
				tkey_led_vdd_enable(1); 	
			}
#endif

#if defined (CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_KOR_MODEL_SHV_E160L)\
	|| defined (CONFIG_USA_MODEL_SGH_T769)|| defined(CONFIG_USA_MODEL_SGH_I577)|| defined(CONFIG_CAN_MODEL_SGH_I577R)\
	|| defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
		enable_irq(IRQ_TOUCHKEY_INT);
		touchkey_enable = 1;
		msleep(50);
		touchkey_auto_calibration(1/*on*/);
#else
	schedule_delayed_work(&touch_resume_work, msecs_to_jiffies(500));
#endif

        mutex_unlock(&touchkey_driver->mutex);
}
#endif				// End of CONFIG_HAS_EARLYSUSPEND

#if defined(CONFIG_GENERIC_BLN)
static void cypress_touchkey_enable_backlight(void) {
    signed char int_data[] ={0x10};
    mutex_lock(&touchkey_driver->mutex);
    i2c_touchkey_write(int_data, 1);

    mutex_unlock(&touchkey_driver->mutex);
}

static void cypress_touchkey_disable_backlight(void) {
    signed char int_data[] ={0x20};
    mutex_lock(&touchkey_driver->mutex);
    i2c_touchkey_write(int_data, 1);

    mutex_unlock(&touchkey_driver->mutex);
}

static bool cypress_touchkey_enable_led_notification(void) {
    if (touchkey_enable)
        return false;

    mutex_lock(&touchkey_driver->mutex);
    tkey_vdd_enable(1);
    msleep(50);
    tkey_led_vdd_enable(1);

    touchkey_driver->is_bln_active = true;
    mutex_unlock(&touchkey_driver->mutex);
    return true;
}

static void cypress_touchkey_disable_led_notification(void) {
    mutex_lock(&touchkey_driver->mutex);

    tkey_vdd_enable(0);
    tkey_led_vdd_enable(0);

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
	printk("[TKEY] sec i2c_touchkey_probe\n");

	touchkey_driver =
	    kzalloc(sizeof(struct i2c_touchkey_driver), GFP_KERNEL);
	if (touchkey_driver == NULL) {
		dev_err(dev, "failed to create our state\n");
		return -ENOMEM;
	}

	touchkey_driver->client = client;
	touchkey_driver->client->irq = IRQ_TOUCHKEY_INT;
	strlcpy(touchkey_driver->client->name, "sec-touchkey", I2C_NAME_SIZE);

	// i2c_set_clientdata(client, state);
	input_dev = input_allocate_device();

	if (!input_dev)
		return -ENOMEM;

	touchkey_driver->input_dev = input_dev;

	input_dev->name = DEVICE_NAME;
	input_dev->phys = "sec-touchkey/input0";
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

        // initialize the mutex and lock to allow initialization to finish uninterrupted
	mutex_init(&touchkey_driver->mutex);
	mutex_lock(&touchkey_driver->mutex);

    //	gpio_pend_mask_mem = ioremap(INT_PEND_BASE, 0x10);  //temp ks
    INIT_DELAYED_WORK(&touch_resume_work, touchkey_resume_func);

#ifdef CONFIG_HAS_EARLYSUSPEND
    touchkey_driver->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 10;
    touchkey_driver->early_suspend.suspend = sec_touchkey_early_suspend;
    touchkey_driver->early_suspend.resume = sec_touchkey_early_resume;
    register_early_suspend(&touchkey_driver->early_suspend);
#endif

	touchkey_enable = 1;

	err= request_threaded_irq( IRQ_TOUCHKEY_INT, NULL, touchkey_interrupt, IRQF_DISABLED	, "touchkey_int", NULL);

	if (err) {
		printk(KERN_ERR "%s Can't allocate irq .. %d\n", __FUNCTION__, err);
                mutex_unlock(&touchkey_driver->mutex);
		return -EBUSY;
	}
#if defined(CONFIG_USA_MODEL_SGH_T989)||defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577)\
	|| defined(CONFIG_CAN_MODEL_SGH_I577R) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
    {
      touchkey_auto_calibration(1/*on*/);
    }
    mdelay(30);
    i2c_touchkey_read	(0x00, data, 6);
    touch_auto_calibration_on_off = (data[5] & 0x80)>>7;
    printk("[TKEY] after touchkey_auto_calibration result = %d \n",touch_auto_calibration_on_off);

#elif defined (CONFIG_USA_MODEL_SGH_I717)

	    err = touchkey_auto_calibration(1/*on*/);
	    if (err < 0) {
		    printk(KERN_ERR"[TouchKey] probe autocalibration fail\n");
		    return err;
	    }
	    mdelay(30);
	    i2c_touchkey_read	    (0x00, data, 6);
	    touch_auto_calibration_on_off = (data[5] & 0x80)>>7;
	    printk("[TKEY] after touchkey_auto_calibration result = %d \n",touch_auto_calibration_on_off);

#elif defined (CONFIG_KOR_MODEL_SHV_E110S)
if (get_hw_rev() >=0x02) {
    touchkey_auto_calibration(1/*on*/);
	mdelay(30);
	i2c_touchkey_read	(0x00, data, 6);
    touch_auto_calibration_on_off = (data[5] & 0x80)>>7;
    printk("[TKEY] after touchkey_auto_calibration result = %d \n",touch_auto_calibration_on_off);
}
#elif defined(CONFIG_KOR_MODEL_SHV_E160L)
if (get_hw_rev() >=0x02) {
    touchkey_auto_calibration(1/*on*/);
	mdelay(30);
	i2c_touchkey_read	(0x00, data, 6);
    touch_auto_calibration_on_off = (data[5] & 0x80)>>7;
    printk("[TKEY] after touchkey_auto_calibration result = %d \n",touch_auto_calibration_on_off);
}
#elif defined (CONFIG_JPN_MODEL_SC_03D)
if (get_hw_rev() >=0x02) {
    touchkey_auto_calibration(1/*on*/);
	mdelay(30);
	i2c_touchkey_read	(0x00, data, 6);
    touch_auto_calibration_on_off = (data[5] & 0x80)>>7;
    printk("[TKEY] after touchkey_auto_calibration result = %d \n",touch_auto_calibration_on_off);
}
#endif
	set_touchkey_debug('K');

#if defined(CONFIG_GENERIC_BLN)
	register_bln_implementation(&cypress_touchkey_bln);
#endif

	mutex_unlock(&touchkey_driver->mutex);
	return 0;
}

static void init_hw(void)
{
	int rc;
	struct pm8058_gpio_cfg {
		int                gpio;
		struct pm_gpio cfg;
	};

	struct pm8058_gpio_cfg touchkey_int_cfg =
	{
	  PM8058_GPIO_PM_TO_SYS(12), // id-1
		{
			.direction	  = PM_GPIO_DIR_IN,
			.pull		   = PM_GPIO_PULL_NO,//PM_GPIO_PULL_NO,
			.vin_sel		= 2,
			.function	   = PM_GPIO_FUNC_NORMAL,
			.inv_int_pol	= 0,
		},
	};

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
	.name = "sec_touchkey",
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

static ssize_t touch_version_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	//buf[size]=0;
	printk("input data --> %s\n", buf);
	return size;
}

static ssize_t touch_recommend_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	//buf[size]=0;
	printk("input data --> %s\n", buf);
	return size;
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
        printk("touchkey_update succeeded\n");
        enable_irq(IRQ_TOUCHKEY_INT);
        return;
	}
	touchkey_update_status = -1;
	printk("touchkey_update failed\n");
	return;
}

static ssize_t touch_update_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	printk("touchkey firmware update \n");
	if (*buf == 'S') {
		disable_irq(IRQ_TOUCHKEY_INT);
		INIT_WORK(&touch_update_work, touchkey_update_func);
		queue_work(touchkey_wq, &touch_update_work);
	}
	return size;
}

static ssize_t touch_update_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;

	printk("[TKEY] touch_update_read: touchkey_update_status %d\n", touchkey_update_status);

	if (touchkey_update_status == 0) {
		count = sprintf(buf, "PASS\n");
	} else if (touchkey_update_status == 1) {
		count = sprintf(buf, "Downloading\n");
	} else if (touchkey_update_status == -1) {
		count = sprintf(buf, "Fail\n");
	}

	return count;
}
#if 0
static int atoi(const char *name)
{
	int val = 0;

	for (;; name++) {
		switch (*name) {
		case '0' ... '9':
			val = 10*val+(*name-'0');
			break;
		default:
			return val;
		}
	}
}
#endif
static ssize_t touch_led_control(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int int_data = 0;
	int errnum = 0;

	mutex_lock(&touchkey_driver->mutex);

#if defined(CONFIG_KOR_MODEL_SHV_E160L)
	if(touchkey_connected==0){
		printk(KERN_ERR "[TKEY] led_control return connect_error\n");
		goto unlock;
		}
	if( touchkey_downloading_status ){
		printk(KERN_ERR "[TKEY] led_control return update_status_error or downloading now! \n");
		goto unlock;
	}
#endif
	if(buf != NULL){
		///int_data = atoi(&data);
		if(buf[0] == '1'){
			int_data =1;
		}else if(buf[0] =='2'){
			int_data = 2;
		}else{
			printk(KERN_ERR "[TKEY] led_control_err data =%c \n",buf[0]);
		}
#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
		int_data = int_data *0x10;
#elif defined(CONFIG_KOR_MODEL_SHV_E110S) || defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_JPN_MODEL_SC_03D)\
	|| defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T769)
#if defined(CONFIG_EUR_MODEL_GT_I9210)
	if (get_hw_rev() >=0x05 )
#elif defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_I727)
	if (get_hw_rev() >=0x05 )
#elif defined (CONFIG_JPN_MODEL_SC_03D)
	if (get_hw_rev() >=0x02 )
#else //E110S
	if (get_hw_rev() >=0x02 )
#endif
      {
		int_data = int_data *0x10;
	}
#elif defined(CONFIG_KOR_MODEL_SHV_E160L)\
   || defined (CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
		int_data = int_data *0x10;
#else

#endif

#if defined (CONFIG_JPN_MODEL_SC_05D)
	int_data = int_data * 0x10;
#endif

		if(g_debug_switch)
			printk(KERN_DEBUG "touch_led_control int_data: %d  \n", int_data);

		#if defined(CONFIG_USA_MODEL_SGH_I717)
			if(Q1_debug_msg)
				printk(KERN_DEBUG "touch_led_control int_data: %d  \n", int_data);
		#endif

		errnum = i2c_touchkey_write((u8*)&int_data, 1);
		if(errnum==-ENODEV) {
			touchled_cmd_reversed = 1;
		}
		touchkey_led_status = int_data;
	} else
		printk("touch_led_control Error\n");

unlock:
	mutex_unlock(&touchkey_driver->mutex);
	return size;
}

static ssize_t touchkey_enable_disable(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
#if 0
	printk("touchkey_enable_disable %c \n", *buf);
	if (*buf == '0') {
		set_touchkey_debug('d');
		disable_irq(IRQ_TOUCH_INT);
		gpio_direction_output(_3_GPIO_TOUCH_EN, 0);
#if !defined(CONFIG_ARIES_NTT)
		gpio_direction_output(_3_GPIO_TOUCH_CE, 0);
#endif
		touchkey_enable = -2;
	} else if (*buf == '1') {
		if (touchkey_enable == -2) {
			set_touchkey_debug('e');
			gpio_direction_output(_3_GPIO_TOUCH_EN, 1);
#if !defined(CONFIG_ARIES_NTT)
			gpio_direction_output(_3_GPIO_TOUCH_CE, 1);
#endif
			touchkey_enable = 1;
			enable_irq(IRQ_TOUCH_INT);
		}
	} else {
		printk("touchkey_enable_disable: unknown command %c \n", *buf);
	}
#endif
	return size;
}

static ssize_t touchkey_menu_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    u8 data[18] = {0, };
    int ret;

    mutex_lock(&touchkey_driver->mutex);
    ret = i2c_touchkey_read(KEYCODE_REG, data, 18);

    #if defined(CONFIG_KOR_MODEL_SHV_E160L)||defined(CONFIG_JPN_MODEL_SC_05D)
    printk("[TKEY] %s data[12] =%d,data[13] = %d\n",__func__,data[12],data[13]);
    menu_sensitivity = ((0x00FF&data[12])<<8)|data[13];
    #else
    printk("[TKEY] %s data[10] =%d,data[11] = %d\n",__func__,data[10],data[11]);
    menu_sensitivity = ((0x00FF&data[10])<<8)|data[11];
    #endif

    mutex_unlock(&touchkey_driver->mutex);

    return sprintf(buf,"%d\n",menu_sensitivity);
}

static ssize_t touchkey_home_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 data[18] = {0, };
	int ret;

        mutex_lock(&touchkey_driver->mutex);
	ret = i2c_touchkey_read(KEYCODE_REG, data, 18);
	printk("[TKEY] %s data[12] =%d,data[13] = %d\n",__func__,data[12],data[13]);
        mutex_unlock(&touchkey_driver->mutex);
	home_sensitivity = ((0x00FF&data[12])<<8)|data[13];
	return sprintf(buf,"%d\n",home_sensitivity);
}

static ssize_t touchkey_back_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 data[18] = {0, };
	int ret;

        mutex_lock(&touchkey_driver->mutex);
	ret = i2c_touchkey_read(KEYCODE_REG, data, 18);

#if defined (CONFIG_KOR_MODEL_SHV_E110S)
	if (get_hw_rev() >= 0x05){
		printk("called %s data[12] =%d,data[13] = %d\n",__func__,data[12],data[13]);
		back_sensitivity = ((0x00FF&data[12])<<8)|data[13];
	} else {
		printk("called %s data[14] =%d,data[15] = %d\n",__func__,data[14],data[15]);
		back_sensitivity = ((0x00FF&data[14])<<8)|data[15];
	}
#elif defined(CONFIG_KOR_MODEL_SHV_E160L) || defined(CONFIG_JPN_MODEL_SC_05D)
    {
        printk("[TKEY] %s data[10] =%d,data[11] = %d\n",__func__,data[10],data[11]);
        back_sensitivity = ((0x00FF&data[10])<<8)|data[11];
    }
#elif defined(CONFIG_JPN_MODEL_SC_03D)
	{
		if (get_hw_rev() >= 0x04){
			printk("called %s data[12] =%d,data[13] = %d\n",__func__,data[12],data[13]);
			back_sensitivity = ((0x00FF&data[12])<<8)|data[13];
		} else {
			printk("called %s data[14] =%d,data[15] = %d\n",__func__,data[14],data[15]);
			back_sensitivity = ((0x00FF&data[14])<<8)|data[15];
		}
	}
#elif defined(CONFIG_EUR_MODEL_GT_I9210)
	{
		if (get_hw_rev() >= 0x06){
			printk("called %s data[12] =%d,data[13] = %d\n",__func__,data[12],data[13]);
			back_sensitivity = ((0x00FF&data[12])<<8)|data[13];
		} else {
			printk("called %s data[14] =%d,data[15] = %d\n",__func__,data[14],data[15]);
			back_sensitivity = ((0x00FF&data[14])<<8)|data[15];
		}
	}
#else
	printk("called %s data[14] =%d,data[15] = %d\n",__func__,data[14],data[15]);
	back_sensitivity = ((0x00FF&data[14])<<8)|data[15];
#endif
        mutex_unlock(&touchkey_driver->mutex);

	return sprintf(buf,"%d\n",back_sensitivity);
}

static ssize_t touchkey_search_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 data[18] = {0, };
	int ret;

        mutex_lock(&touchkey_driver->mutex);
	printk("called %s \n",__func__);
	ret = i2c_touchkey_read(KEYCODE_REG, data, 18);
	printk("called %s data[16] =%d,data[17] = %d\n",__func__,data[16],data[17]);
        mutex_unlock(&touchkey_driver->mutex);
	search_sensitivity = ((0x00FF&data[16])<<8)|data[17];
	return sprintf(buf,"%d\n",search_sensitivity);
}

static ssize_t touchkey_threshold_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 data[18];
	int ret;

        mutex_lock(&touchkey_driver->mutex);
	printk("called %s \n",__func__);
	ret = i2c_touchkey_read(KEYCODE_REG, data, 18);
	printk("called %s data[4] =%d\n",__func__,data[4]);
        mutex_unlock(&touchkey_driver->mutex);
	return sprintf(buf,"%d\n",data[4]);
}

static ssize_t touchkey_raw_data0_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 data[26];
	int ret;

        mutex_lock(&touchkey_driver->mutex);
	printk("called %s \n",__func__);
	ret = i2c_touchkey_read(KEYCODE_REG, data, 26);
	printk("called %s data[18] =%d,data[19] = %d\n",__func__,data[18],data[19]);
        mutex_unlock(&touchkey_driver->mutex);
	raw_data0 = ((0x00FF&data[18])<<8)|data[19];
	return sprintf(buf,"%d\n",raw_data0);
}

static ssize_t touchkey_raw_data1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 data[26];
	int ret;

        mutex_lock(&touchkey_driver->mutex);
	printk("called %s \n",__func__);
	ret = i2c_touchkey_read(KEYCODE_REG, data, 26);
	printk("called %s data[20] =%d,data[21] = %d\n",__func__,data[20],data[21]);
        mutex_unlock(&touchkey_driver->mutex);
	raw_data1 = ((0x00FF&data[20])<<8)|data[21];
	return sprintf(buf,"%d\n",raw_data1);
}

static ssize_t touchkey_raw_data2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 data[26];
	int ret;

        mutex_lock(&touchkey_driver->mutex);
	printk("called %s \n",__func__);
	ret = i2c_touchkey_read(KEYCODE_REG, data, 26);
	printk("called %s data[22] =%d,data[23] = %d\n",__func__,data[22],data[23]);
        mutex_unlock(&touchkey_driver->mutex);
	raw_data2 = ((0x00FF&data[22])<<8)|data[23];
	return sprintf(buf,"%d\n",raw_data2);
}

static ssize_t touchkey_raw_data3_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 data[26];
	int ret;

        mutex_lock(&touchkey_driver->mutex);
	printk("called %s \n",__func__);
	ret = i2c_touchkey_read(KEYCODE_REG, data, 26);
	printk("called %s data[24] =%d,data[25] = %d\n",__func__,data[24],data[25]);
        mutex_unlock(&touchkey_driver->mutex);
	raw_data3 = ((0x00FF&data[24])<<8)|data[25];
	return sprintf(buf,"%d\n",raw_data3);
}

static ssize_t touchkey_idac0_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 data[10];
	int ret;

        mutex_lock(&touchkey_driver->mutex);
	printk("called %s \n",__func__);
	ret = i2c_touchkey_read(KEYCODE_REG, data, 10);
	printk("called %s data[6] =%d\n",__func__,data[6]);
        mutex_unlock(&touchkey_driver->mutex);
	idac0 = data[6];
	return sprintf(buf,"%d\n",idac0);
}

static ssize_t touchkey_idac1_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 data[10];
	int ret;

        mutex_lock(&touchkey_driver->mutex);
	printk("called %s \n",__func__);
	ret = i2c_touchkey_read(KEYCODE_REG, data, 10);
	printk("called %s data[7] = %d\n",__func__,data[7]);
        mutex_unlock(&touchkey_driver->mutex);
	idac1 = data[7];
	return sprintf(buf,"%d\n",idac1);
}

static ssize_t touchkey_idac2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 data[10];
	int ret;

        mutex_lock(&touchkey_driver->mutex);
	printk("called %s \n",__func__);
	ret = i2c_touchkey_read(KEYCODE_REG, data, 10);
	printk("called %s data[8] =%d\n",__func__,data[8]);
        mutex_unlock(&touchkey_driver->mutex);
	idac2 = data[8];
	return sprintf(buf,"%d\n",idac2);
}

static ssize_t touchkey_idac3_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 data[10];
	int ret;

        mutex_lock(&touchkey_driver->mutex);
	printk("called %s \n",__func__);
	ret = i2c_touchkey_read(KEYCODE_REG, data, 10);
	printk("called %s data[9] = %d\n",__func__,data[9]);
        mutex_unlock(&touchkey_driver->mutex);
	idac3 = data[9];
	return sprintf(buf,"%d\n",idac3);
}

#if defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)\
 || defined (CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
static ssize_t autocalibration_enable(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
        int data;

	mutex_lock(&touchkey_driver->mutex);
        sscanf(buf, "%d\n", &data);

        if(data == 1)
                touchkey_auto_calibration(1/*on*/);
	mutex_unlock(&touchkey_driver->mutex);

        return size;
}

static ssize_t autocalibration_status(struct device *dev, struct device_attribute *attr, char *buf)
{
        u8 data[6];
        int ret;

        printk("called %s \n",__func__);


        mutex_lock(&touchkey_driver->mutex);
        ret = i2c_touchkey_read(KEYCODE_REG, data, 6);
        mutex_unlock(&touchkey_driver->mutex);
        if((data[5] & 0x80))
                return sprintf(buf,"Enabled\n");
        else
                return sprintf(buf,"Disabled\n");

}
#endif


static ssize_t touch_sensitivity_control(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned char data = 0x40;

	mutex_lock(&touchkey_driver->mutex);
#if defined (CONFIG_KOR_MODEL_SHV_E160L)
//	int ret;
//	unsigned char data_buf[2]={0,};
//	int int_data = 0;

    if (buf && (buf[0] =='2'))
    {
		printk( "%s enable_irq\n",__func__);
		touchkey_enable = 1;
		enable_irq(IRQ_TOUCHKEY_INT);
    }
/*
	if (sscanf(buf, "%c\n", data_buf[0]) == 1) {
		int_data = atoi(data_buf);
	}
	if(int_data == 2){
		printk( "%s enable_irq\n",__func__);
		touchkey_enable = 1;
		enable_irq(IRQ_TOUCHKEY_INT);
	}
*/
#endif
	printk("[TKEY] called %s \n",__func__);
	i2c_touchkey_write(&data, 1);
	mutex_unlock(&touchkey_driver->mutex);
	return size;
}

#if defined (CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727) || defined(CONFIG_USA_MODEL_SGH_I717) \
|| defined (CONFIG_KOR_MODEL_SHV_E110S)|| defined(CONFIG_KOR_MODEL_SHV_E160L) || defined(CONFIG_CAN_MODEL_SGH_I757M)\
|| defined(CONFIG_USA_MODEL_SGH_I757) || defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined(CONFIG_JPN_MODEL_SC_05D)
static ssize_t touch_recommend_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	char data[3] = { 0, };
	int count;
	mutex_lock(&touchkey_driver->mutex);
#if defined (CONFIG_KOR_MODEL_SHV_E110S)
		if ((get_hw_rev() == 0x03) || (get_hw_rev() == 0x04)){
			data[1] = 0x08;
		} else if (get_hw_rev() == 0x05){
			data[1] = 0x00;
		} else if (get_hw_rev() == 0x06){
			data[1] = 0x02;
		} else if (get_hw_rev() >= 0x07){
			data[1] = 0x07;
		}
#elif defined(CONFIG_KOR_MODEL_SHV_E160L)
    printk(KERN_ERR "data_mdule_rev = %x\n",data_mdule_rev);
	if (get_hw_rev() >=0x02 ){
		if(data_mdule_rev ==0x02)
			data[1] = 0x03;
		else
			data[1] = 0x07;
        } else{
		data[1] = 0x00;
	}
#elif defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
		data[1] = BUIL_FW_VER;
#elif defined (CONFIG_EUR_MODEL_GT_I9210)
	if (get_hw_rev() >=0x07)
		data[1] = 0x07;
	else
		data[1] = 0x00;
#elif defined (CONFIG_USA_MODEL_SGH_I727)
        if (get_hw_rev() >=0x0a)
                data[1] = 0x12;
		else
                data[1] = 0x07;
#elif defined(CONFIG_USA_MODEL_SGH_I757)  || defined(CONFIG_CAN_MODEL_SGH_I757M)
	if (get_hw_rev() >= 0x04)
		data[1] = 0x06;
	else
		data[1] = 0x03;
#elif defined (CONFIG_USA_MODEL_SGH_I717)
                data[1] = 0x04;
#elif defined (CONFIG_USA_MODEL_SGH_T769)
        data[1] = BUILTIN_FW_VER;
#elif defined (CONFIG_USA_MODEL_SGH_T989)
        if (get_hw_rev() >= 0x0d)
                data[1] = 0x13;
      else if (get_hw_rev() >= 0x09)
	  	data[1] = 0x11;
         else if (get_hw_rev() == 0x08)
		   data[1] = 0x0f;
         else if (get_hw_rev() == 0x05)
		   data[1] = 0x0c;
#elif defined (CONFIG_JPN_MODEL_SC_03D)
	data[1] = firm_version;
#endif

	count = sprintf(buf, "0x%x\n", data[1]);
	mutex_unlock(&touchkey_driver->mutex);

	printk("touch_recommend_read 0x%x\n", data[1]);
	return count;
}
#else
static ssize_t set_touchkey_firm_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/*TO DO IT */
	int count;
#if defined (CONFIG_JPN_MODEL_SC_03D)
	count = sprintf(buf, "0x%x\n", firm_version);
#elif defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
	count = sprintf(buf, "0x%x\n", BUIL_FW_VER);
#else
	count = sprintf(buf, "0x%x\n", FIRMWARE_VERSION);
#endif
	return count;
}
#endif

#if defined(CONFIG_JPN_MODEL_SC_05D)
static ssize_t set_touchkey_firm_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/*TO DO IT */
	int count;
	count = sprintf(buf, "0x%x\n", FIRMWARE_VERSION);
	return count;
}
#endif

static ssize_t set_touchkey_update_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	/*TO DO IT */
	int count=0;
	int retry=3;
	touchkey_update_status = 1;

	mutex_lock(&touchkey_driver->mutex);
#ifdef TEST_JIG_MODE
	unsigned char get_touch = 0x40;
#endif

	while (retry--) {
#if defined (CONFIG_JPN_MODEL_SC_03D)
			if (ISSP_main(get_hw_rev()) == 0) {
#else
			if (ISSP_main(TOUCHKEY_PBA_REV_NA) == 0) {
#endif
				printk(KERN_ERR"[TOUCHKEY]Touchkey_update succeeded\n");
				touchkey_update_status = 0;
				count=1;
				break;
			}
#if defined (CONFIG_KOR_MODEL_SHV_E160L)\
	|| defined (CONFIG_USA_MODEL_SGH_T769) || defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
			init_hw();
#endif
			printk(KERN_ERR"touchkey_update failed... retry...[From set_touchkey_update_show()] \n");
	}
	if (retry <= 0) {
			count=0;
			printk(KERN_ERR"[TOUCHKEY]Touchkey_update fail\n");
			touchkey_update_status = -1;
			return count;
	}

	init_hw();	/* after update, re initalize. */

#ifdef TEST_JIG_MODE
	i2c_touchkey_write(&get_touch, 1);
#endif
	mutex_unlock(&touchkey_driver->mutex);

	return count;
}

static ssize_t set_touchkey_autocal_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count0, count1 = 0;

	/*TO DO IT */

	mutex_lock(&touchkey_driver->mutex);
	printk("called %s \n",__func__);
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

	printk(KERN_DEBUG "[TouchKey] touch_version_read 0x%x\n", data[1]);
        mutex_unlock(&touchkey_driver->mutex);
	return count;
}

static ssize_t set_touchkey_firm_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;

        mutex_lock(&touchkey_driver->mutex);
	printk(KERN_DEBUG
	       "[TouchKey] touch_update_read: touchkey_update_status %d\n",
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
		pr_err("%s: failed to get resource %s\n", __func__,
				"touch_led");
		return;
	}
	ret = regulator_set_voltage(tled_regulator, vol_mv * 100000, vol_mv * 100000);
	if ( ret ) {
		printk("%s: error setting voltage\n", __func__);
	}

	regulator_put(tled_regulator);
}

static ssize_t brightness_control(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int data;

	mutex_lock(&touchkey_driver->mutex);
	if (sscanf(buf, "%d\n", &data) == 1) {
		printk(KERN_ERR "[TouchKey] touch_led_brightness: %d \n", data);
		change_touch_key_led_voltage(data);
	} else {
		printk(KERN_ERR "[TouchKey] touch_led_brightness Error\n");
	}
	mutex_unlock(&touchkey_driver->mutex);
	return size;
}

static ssize_t brightness_level_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;

	count = sprintf(buf, "%d\n", vol_mv_level);

	printk(KERN_DEBUG "[TouchKey] Touch LED voltage = %d\n", vol_mv_level);
	return count;
}

static DEVICE_ATTR(touch_version, S_IRUGO | S_IWUSR | S_IWGRP, touch_version_read, touch_version_write);
static DEVICE_ATTR(touch_recommend, S_IRUGO | S_IWUSR | S_IWGRP, touch_recommend_read, touch_recommend_write);
static DEVICE_ATTR(touch_update, S_IRUGO | S_IWUSR | S_IWGRP, touch_update_read, touch_update_write);
static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWGRP, NULL, touch_led_control);
static DEVICE_ATTR(enable_disable, S_IRUGO | S_IWUSR | S_IWGRP, NULL, touchkey_enable_disable);
static DEVICE_ATTR(touchkey_menu, S_IRUGO, touchkey_menu_show, NULL);
static DEVICE_ATTR(touchkey_home, S_IRUGO, touchkey_home_show, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, touchkey_back_show, NULL);
static DEVICE_ATTR(touchkey_search, S_IRUGO, touchkey_search_show, NULL);
static DEVICE_ATTR(touchkey_threshold, S_IRUGO, touchkey_threshold_show, NULL);
static DEVICE_ATTR(touchkey_raw_data0, S_IRUGO, touchkey_raw_data0_show, NULL);
static DEVICE_ATTR(touchkey_raw_data1, S_IRUGO, touchkey_raw_data1_show, NULL);
static DEVICE_ATTR(touchkey_raw_data2, S_IRUGO, touchkey_raw_data2_show, NULL);
static DEVICE_ATTR(touchkey_raw_data3, S_IRUGO, touchkey_raw_data3_show, NULL);
static DEVICE_ATTR(touchkey_idac0, S_IRUGO, touchkey_idac0_show, NULL);
static DEVICE_ATTR(touchkey_idac1, S_IRUGO, touchkey_idac1_show, NULL);
static DEVICE_ATTR(touchkey_idac2, S_IRUGO, touchkey_idac2_show, NULL);
static DEVICE_ATTR(touchkey_idac3, S_IRUGO, touchkey_idac3_show, NULL);
static DEVICE_ATTR(touch_sensitivity, S_IRUGO | S_IWUSR | S_IWGRP, NULL, touch_sensitivity_control);
static DEVICE_ATTR(touchkey_firm_update, S_IRUGO | S_IWUSR | S_IWGRP, set_touchkey_update_show, NULL);		/* firmware update */
static DEVICE_ATTR(touchkey_autocal_start, S_IRUGO | S_IWUSR | S_IWGRP, set_touchkey_autocal_show, NULL);		/* Autocal Start */
static DEVICE_ATTR(touchkey_firm_update_status, S_IRUGO | S_IWUSR | S_IWGRP, set_touchkey_firm_status_show, NULL);	/* firmware update status return */
#if defined (CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727) || defined(CONFIG_USA_MODEL_SGH_I717) \
|| defined (CONFIG_KOR_MODEL_SHV_E110S)|| defined(CONFIG_KOR_MODEL_SHV_E160L) || defined(CONFIG_CAN_MODEL_SGH_I757M)\
|| defined(CONFIG_USA_MODEL_SGH_I757) || defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
static DEVICE_ATTR(touchkey_firm_version_phone, S_IRUGO | S_IWUSR | S_IWGRP, touch_recommend_read, NULL);/* PHONE*/	/* firmware version resturn in phone driver version */
#else
static DEVICE_ATTR(touchkey_firm_version_phone, S_IRUGO | S_IWUSR | S_IWGRP, set_touchkey_firm_version_show, NULL);/* PHONE*/	/* firmware version resturn in phone driver version */
#endif
static DEVICE_ATTR(touchkey_firm_version_panel, S_IRUGO | S_IWUSR | S_IWGRP, set_touchkey_firm_version_read_show, NULL);/*PART*/	/* firmware version resturn in touchkey panel version */
static DEVICE_ATTR(touchkey_brightness, S_IRUGO | S_IWUSR | S_IWGRP, brightness_level_show, brightness_control);

#if defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)\
 || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
static DEVICE_ATTR(autocal_enable, S_IRUGO | S_IWUSR | S_IWGRP, NULL, autocalibration_enable);
static DEVICE_ATTR(autocal_stat, S_IRUGO | S_IWUSR | S_IWGRP, autocalibration_status, NULL);
#endif


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

#if defined (CONFIG_USA_MODEL_SGH_I727)
	extern int ISSP_main(int touchkey_pba_rev);
#endif
    printk("[TKEY] touchkey_init START \n");

#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return ENODEV!\n", __func__);
		return 0;
	}
#endif

#if defined (CONFIG_KOR_MODEL_SHV_E160L)
	tkey_led_vdd_enable(1);
#endif

	ret = misc_register(&touchkey_update_device);
	if (ret) {
		printk("%s misc_register fail\n", __FUNCTION__);
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

	if (device_create_file(touchkey_update_device.this_device, &dev_attr_touchkey_menu) < 0) {
		printk("%s device_create_file fail dev_attr_touchkey_menu\n" ,__FUNCTION__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_menu.attr.name);
	}

	if (device_create_file (touchkey_update_device.this_device, &dev_attr_touchkey_home) < 0) {
		printk("%s device_create_file fail dev_attr_touchkey_home\n" ,__FUNCTION__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_home.attr.name);
	}

	if (device_create_file (touchkey_update_device.this_device, &dev_attr_touchkey_back) < 0) {
		printk("%s device_create_file fail dev_attr_touchkey_back\n", __FUNCTION__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_back.attr.name);
	}

	if (device_create_file(touchkey_update_device.this_device, &dev_attr_touchkey_search) < 0) {
		printk("%s device_create_file fail dev_attr_touchkey_search\n",__FUNCTION__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_search.attr.name);
	}
	if (device_create_file (touchkey_update_device.this_device, &dev_attr_touchkey_threshold) < 0) {
		printk("%s device_create_file fail dev_attr_touchkey_threshold\n",__FUNCTION__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_search.attr.name);
	}
	if (device_create_file(touchkey_update_device.this_device, &dev_attr_touchkey_raw_data0) < 0) {
		printk(KERN_ERR "%s device_create_file fail dev_attr_touchkey_raw_data0\n",	__func__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_raw_data0.attr.name);
	}

	if (device_create_file (touchkey_update_device.this_device, &dev_attr_touchkey_raw_data1) < 0) {
		printk(KERN_ERR "%s device_create_file fail dev_attr_touchkey_raw_data1\n",	__func__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_raw_data1.attr.name);
	}

	if (device_create_file (touchkey_update_device.this_device, &dev_attr_touchkey_raw_data2) < 0) {
		printk(KERN_ERR "%s device_create_file fail dev_attr_touchkey_raw_data2\n",	__func__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_raw_data2.attr.name);
	}

	if (device_create_file (touchkey_update_device.this_device, &dev_attr_touchkey_raw_data3) < 0) {
		printk(KERN_ERR "%s device_create_file fail dev_attr_touchkey_raw_data3\n",	__func__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_raw_data3.attr.name);
	}

	if (device_create_file (touchkey_update_device.this_device, &dev_attr_touchkey_idac0) < 0) {
		printk(KERN_ERR "%s device_create_file fail dev_attr_touchkey_idac0\n",	__func__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_idac0.attr.name);
	}

	if (device_create_file(touchkey_update_device.this_device, &dev_attr_touchkey_idac1) < 0) {
		printk(KERN_ERR "%s device_create_file fail dev_attr_touchkey_idac1\n",	__func__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_idac1.attr.name);
	}

	if (device_create_file (touchkey_update_device.this_device, &dev_attr_touchkey_idac2) < 0) {
		printk(KERN_ERR "%s device_create_file fail dev_attr_touchkey_idac2\n",	__func__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_idac2.attr.name);
	}

	if (device_create_file (touchkey_update_device.this_device, &dev_attr_touchkey_idac3) < 0) {
		printk(KERN_ERR "%s device_create_file fail dev_attr_touchkey_idac3\n",	__func__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touchkey_idac3.attr.name);
	}

	if (device_create_file (touchkey_update_device.this_device, &dev_attr_touch_sensitivity) < 0) {
		printk("%s device_create_file fail dev_attr_touch_sensitivity\n", __FUNCTION__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_touch_sensitivity.attr.name);
	}

#if defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)\
 || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
	if (device_create_file (touchkey_update_device.this_device, &dev_attr_autocal_enable) < 0) {
		printk(KERN_ERR "%s device_create_file fail dev_attr_autocal_enable\n",__func__);
		pr_err("Failed to create device file(%s)!\n",dev_attr_autocal_enable.attr.name);
	}

	if (device_create_file(touchkey_update_device.this_device, &dev_attr_autocal_stat) < 0) {
		printk(KERN_ERR "%s device_create_file fail dev_attr_autocal_stat\n",__func__);
		pr_err("Failed to create device file(%s)!\n",dev_attr_autocal_stat.attr.name);
	}
#endif

	sec_touchkey= device_create(sec_class, NULL, 0, NULL, "sec_touchkey");

	if (IS_ERR(sec_touchkey)) {
			printk("Failed to create device(sec_touchkey)!\n");
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

#ifdef CONFIG_S5PC110_T959_BOARD //NAGSM_Android_SEL_Kernel_Aakash_20100320
	if (device_create_file(touchkey_update_device.this_device, &dev_attr_melfasevtcntrl) < 0)
	{
		printk("%s device_create_file fail dev_attr_melfasevtcntrl\n",__FUNCTION__);
		pr_err("Failed to create device file(%s)!\n", dev_attr_melfasevtcntrl.attr.name);
	}
#endif

	touchkey_wq = create_singlethread_workqueue("sec_touchkey_wq");
	if (!touchkey_wq)
		return -ENOMEM;

	INIT_WORK(&touchkey_work, touchkey_work_func);
    // init_hw();

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
#elif defined (CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_KOR_MODEL_SHV_E160L)\
   || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);
#elif defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)
	if (get_hw_rev() >= 0x0d){
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_RISING);
	} else {
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);
	}
#elif defined (CONFIG_JPN_MODEL_SC_05D)
	irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_EDGE_FALLING);

#else
		irq_set_irq_type(IRQ_TOUCHKEY_INT, IRQ_TYPE_LEVEL_LOW);
#endif

	while (retry--) {
		if (get_touchkey_firmware(data) == 0)	//melfas need delay for multiple read
			break;
		else
			printk("f/w read fail retry %d\n", retry);
	}

#if !defined (CONFIG_KOR_MODEL_SHV_E110S) && !defined(CONFIG_KOR_MODEL_SHV_E160L) && !defined(CONFIG_EUR_MODEL_GT_I9210)
    if(retry < 0)
        return 0;
#endif

	printk("[TKEY] %s F/W version: 0x%x, Module version:0x%x, HW_REV: 0x%x\n", __FUNCTION__, data[1], data[2], get_hw_rev());
	touch_version = data[1];
	retry = 3;

#if defined (CONFIG_USA_MODEL_SGH_T769)
	if(data[1] > 0x03 && data[1] < BUILTIN_FW_VER) {
		extern int ISSP_main(int touchkey_pba_rev);
		set_touchkey_debug('U');
		while (retry--) {
			if (ISSP_main(touchkey_pba_revision) == 0) {
				printk("touchkey_update succeeded_new\n");
				set_touchkey_debug('C');
				break;
			}
			printk("touchkey_update failed... retry...\n");
			set_touchkey_debug('f');
		}
		init_hw();	//after update, re initalize.
	}

#elif defined(CONFIG_USA_MODEL_SGH_T989)//new touchkey fpcb
	//update version "eclair/vendor/samsung/apps/Lcdtest/src/com/sec/android/app/lcdtest/touch_firmware.java"
	if ((data[1] == 0x01) && (data[2] < 0x05)) {
		set_touchkey_debug('U');
		while (retry--) {
			if (ISSP_main(TOUCHKEY_PBA_REV_NA) == 0) {
				printk("touchkey_update succeeded\n");
				set_touchkey_debug('C');
				break;
			}
			printk("touchkey_update failed... retry...\n");
			set_touchkey_debug('f');
		}
		init_hw();	//after update, re initalize.
	}
	else if (((data[1] != 0x0c) && (data[2] == 0x02) ) || ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))&& (get_hw_rev() ==0x05 )))
		{
		touchkey_pba_revision = TOUCHKEY_PBA_REV_02;
		set_touchkey_debug('U');
		while (retry--) {
			if (ISSP_main(TOUCHKEY_PBA_REV_02) == 0) {
				printk("touchkey_update succeeded_new\n");
				set_touchkey_debug('C');
				break;
			}
			printk("touchkey_update failed... retry...\n");
			set_touchkey_debug('f');
		}
		init_hw();	//after update, re initalize.
	}
	else if (((data[1] < 0x0f) && (data[2] == 0x03) )  || ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))&& (get_hw_rev() ==0x08 )))
		{
		touchkey_pba_revision = TOUCHKEY_PBA_REV_03;
		set_touchkey_debug('U');
		while (retry--) {
			if (ISSP_main(TOUCHKEY_PBA_REV_03) == 0) {
				printk("touchkey_update succeeded_new\n");
				set_touchkey_debug('C');
				break;
			}
			printk("touchkey_update failed... retry...\n");
			set_touchkey_debug('f');
		}
		init_hw();	//after update, re initalize.
	}
	else if (((data[1] < 0x11) && (data[2] == 0x04) )  || ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))&& (get_hw_rev() >=0x09 )&&(get_hw_rev() <0x0d )))
		{
		touchkey_pba_revision = TOUCHKEY_PBA_REV_04;
		set_touchkey_debug('U');
		while (retry--) {
			if (ISSP_main(TOUCHKEY_PBA_REV_04) == 0) {
				printk("touchkey_update succeeded_new\n");
				set_touchkey_debug('C');
				break;
			}
			printk("touchkey_update failed... retry...\n");
			set_touchkey_debug('f');
		}
		init_hw();	//after update, re initalize.
	}
	else if (((data[1] < 0x13) && (data[2] == 0x05) )  || ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))&& (get_hw_rev() >=0x0d )))
		{
		touchkey_pba_revision = TOUCHKEY_PBA_REV_05;
		set_touchkey_debug('U');
		while (retry--) {
			if (ISSP_main(TOUCHKEY_PBA_REV_05) == 0) {
				printk("touchkey_update succeeded_new\n");
				set_touchkey_debug('C');
				break;
			}
			printk("touchkey_update failed... retry...\n");
			set_touchkey_debug('f');
		}
		init_hw();	//after update, re initalize.
	}
#elif defined (CONFIG_EUR_MODEL_GT_I9210)
	if (get_hw_rev()<0x7 ) // Don't use update under H/W rev0.2
	{
		printk("%s : I9210 update tkey...\n",__func__);

		init_hw();	//after update, re initalize.

		get_touchkey_firmware(data);
		printk("%s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__,
		data[1], data[2]);
	}
	else if ((data[1] != 0x00) && (data[1] != 0x07) && (get_hw_rev() >=0x07)) // H/W rev0.3
	{
/*		extern int ISSP_main(int touchkey_pba_rev); */ /* remove because  alreadry declared*/
		printk("%s : I9210 update 3 tkey...\n",__func__);
		set_touchkey_debug('U');
		while (retry--) {
			if (ISSP_main(0) == 0) {
				printk("touchkey_update succeeded\n");
				set_touchkey_debug('C');
				break;
			}
			printk("touchkey_update failed... retry...\n");
			set_touchkey_debug('f');
		}

		init_hw();	//after update, re initalize.

		get_touchkey_firmware(data);
		printk("%s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__,
		data[1], data[2]);
	}
#elif defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
	if (data[1] < BUIL_FW_VER) {
		extern int ISSP_main(int touchkey_pba_rev);
		printk("%s : update 577 tkey...\n",__func__);
		set_touchkey_debug('U');
		while (retry--) {
			if (ISSP_main(0) == 0) {
				printk("touchkey_update succeeded\n");
				set_touchkey_debug('C');
				break;
			}
			printk("touchkey_update failed... retry...\n");
			set_touchkey_debug('f');
		}
		init_hw();	//after update, re initalize.
		//get_touchkey_firmware(data);
	//	printk("%s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__, data[1], data[2]);

	}

#elif defined (CONFIG_USA_MODEL_SGH_I727)
	if (((data[1] < 0x07) && (data[2] == 0x15))|| ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))&& ((get_hw_rev() >=0x05 )&& (get_hw_rev()<0x0a))))
{
		printk("%s : update 727 tkey...\n",__func__);
		set_touchkey_debug('U');
		while (retry--) {
			if (ISSP_main(0) == 0) {
				printk("touchkey_update succeeded\n");
				set_touchkey_debug('C');
				break;
			}
			printk("touchkey_update failed... retry...\n");
			set_touchkey_debug('f');
		}
		init_hw();	//after update, re initalize.
		get_touchkey_firmware(data);
		printk("%s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__, data[1], data[2]);
	}
		else if (((data[1] < 0x09) && (data[2] == 0x18))|| ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))&& (get_hw_rev() >=0x0a )))
{
		printk("%s : update 727 tkey...\n",__func__);
		set_touchkey_debug('U');
		while (retry--) {
			if (ISSP_main(0) == 0) {
				printk("touchkey_update succeeded\n");
				set_touchkey_debug('C');
				break;
			}
			printk("touchkey_update failed... retry...\n");
			set_touchkey_debug('f');
		}
		init_hw();	//after update, re initalize.
		get_touchkey_firmware(data);
		printk("%s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__, data[1], data[2]);
	}
#elif defined (CONFIG_USA_MODEL_SGH_I717)
	if (((data[1] != 0x04) && (data[2] <= 0x2))|| ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))))
    {
            printk("%s : update 727 tkey...\n",__func__);
            //extern int ISSP_main(int touchkey_pba_rev);
            set_touchkey_debug('U');
            while (retry--) {
                if (ISSP_main(0) == 0) {
                    printk("touchkey_update succeeded\n");
                    set_touchkey_debug('C');
                    break;
                }
                printk("touchkey_update failed... retry...\n");
                set_touchkey_debug('f');
            }
            init_hw();  //after update, re initalize.
            get_touchkey_firmware(data);
            printk("%s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__, data[1], data[2]);
        }

	else if (((data[1] < 0x09) && (data[2] == 0x18))|| ((((data[1] == 0x0) && (data[2] == 0x0) )||((data[1] == 0xff) && (data[2] == 0xff) ))&& (get_hw_rev() >=0x0a )))
{
		printk("%s : update 727 tkey...\n",__func__);
		//extern int ISSP_main(int touchkey_pba_rev);
		set_touchkey_debug('U');
		while (retry--) {
			if (ISSP_main(0) == 0) {
				printk("touchkey_update succeeded\n");
				set_touchkey_debug('C');
				break;
			}
			printk("touchkey_update failed... retry...\n");
			set_touchkey_debug('f');
		}
		init_hw();	//after update, re initalize.
		get_touchkey_firmware(data);
		printk("%s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__, data[1], data[2]);
	}
#elif defined (CONFIG_KOR_SHV_E120L_WXGA)
	if ((data[1] != 0x05) || (data[2] != 0x00)) {
printk("%s : update SHV_E120L_WXGA tkey...\n",__func__);
		extern int ISSP_main(int touchkey_pba_rev);
		set_touchkey_debug('U');
		while (retry--) {
			if (ISSP_main(NULL) == 0) {
				printk("touchkey_update succeeded\n");
				set_touchkey_debug('C');
				break;
			}
			printk("touchkey_update failed... retry...\n");
			set_touchkey_debug('f');
		}
		init_hw();	//after update, re initalize.
		get_touchkey_firmware(data);
		printk("%s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__, data[1], data[2]);
	}
#elif defined(CONFIG_KOR_MODEL_SHV_E160L)
    data_mdule_rev = data[2];
    printk("[TKEY] %s : SHV_E160S F/W version : %x, module_rev : %x\n",__func__,data[1],data[2]);
/*
    if (get_hw_rev() >= 0x05) {
        if(data[1] < 0x07)  {
            extern int ISSP_main(int touchkey_pba_rev);
            set_touchkey_debug('U');
            if (data[1] != 0x00) {
                while (retry--) {
                    if (ISSP_main(NULL) == 0) {
                        printk("[TKEY] touchkey_init :: touchkey_update succeeded\n");
                        set_touchkey_debug('C');
                        break;
                    }
				init_hw();
                }
			init_hw();	//after update, re initalize.
			get_touchkey_firmware(data);
			printk("[TKEY] %s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__, data[1], data[2]);
            }
            else {
                printk("[TKEY] touchkey_init ::(Check Cable!!) \n");
            }
            set_touchkey_debug('f');
        }
        else {
            printk("[TKEY] %s : SHV_E160S F/W version is newest !!! \n",__func__ );
        }
    }
    */
#if defined(CONFIG_KOR_MODEL_SHV_E160L)
	if (data[1] != 0x00)
		touchkey_connected = 1;
	else{
		touchkey_connected = 0;
		 printk("[TKEY] touchkey_connect error \n");
		}
#endif
#elif defined (CONFIG_KOR_MODEL_SHV_E110S)
 // E110S Touch key F/W version Fix. Update routine Remove.
/*	if (((get_hw_rev() == 0x03) && (data[1] != 0x08) && (data[2] == 0x02))
	|| ((get_hw_rev() == 0x04) && (data[1] != 0x08))// && (data[2] == 0x02))
	|| ((get_hw_rev() == 0x05) && (data[1] != 0x00)) //&& (data[2] == 0x02))
	|| ((get_hw_rev() == 0x06) && (data[1] != 0x02)) //&& (data[2] == 0x03))
	|| ((get_hw_rev() >= 0x07) && (data[1] != 0x07))) //&& (data[2] == 0x03)))
		{
		   printk("%s : update E110S %d!!!...\n",__func__, get_hw_rev());
			extern int ISSP_main(int touchkey_pba_rev);
			set_touchkey_debug('U');
			while (retry--) {
				if (ISSP_main(NULL) == 0) {
					printk("touchkey_update succeeded\n");
					set_touchkey_debug('C');
					break;
				}
				printk("touchkey_update failed... retry...\n");
				set_touchkey_debug('f');
			}

			init_hw();	//after update, re initalize.

			get_touchkey_firmware(data);
			printk("%s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__,
			   data[1], data[2]);
		}
	else
		{
		   printk("%s : Touch Key version is latest. E110S %d!!!...\n",__func__, get_hw_rev());
		}*/
#elif defined (CONFIG_JPN_MODEL_SC_03D)
		if (get_hw_rev() ==0x01 ){
			firm_version = 0x02;
			if ((data[1] != 0x02) || (data[2] != 0x00))
			{
			   printk("%s : update SD-03D...\n",__func__);
				extern int ISSP_main(int touchkey_pba_rev);
				set_touchkey_debug('U');
				while (retry--) {
					if (ISSP_main(NULL) == 0) {
						printk("touchkey_update succeeded\n");
						set_touchkey_debug('C');
						break;
					}
					printk("touchkey_update failed... retry...\n");
					set_touchkey_debug('f');
				}

				init_hw();	//after update, re initalize.

				get_touchkey_firmware(data);
				printk("%s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__,
				   data[1], data[2]);
			}
		}else if (get_hw_rev() ==0x02 ){
			firm_version = 0x08;
			if ((data[1] != 0x08) || (data[2] != 0x02))
			{
			   printk("%s : REV02 update SD-03D...\n",__func__);
				extern int ISSP_main(int touchkey_pba_rev);
				set_touchkey_debug('U');
				while (retry--) {
					if (ISSP_main(TOUCHKEY_PBA_REV_02) == 0) {
						printk("touchkey_update succeeded\n");
						set_touchkey_debug('C');
						break;
					}
					printk("touchkey_update failed... retry...\n");
					set_touchkey_debug('f');
				}

				init_hw();	//after update, re initalize.

				get_touchkey_firmware(data);
				printk("%s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__,
				   data[1], data[2]);
			}
		} else if (get_hw_rev() ==0x04){	// PBA rev0.3
			firm_version = 0x02;
			if ((data[1] != 0x02) || (data[2] != 0x00))
			{
			   printk("%s : REV04 update SD-03D...\n",__func__);
				extern int ISSP_main(int touchkey_pba_rev);
				set_touchkey_debug('U');
				while (retry--) {
					if (ISSP_main(TOUCHKEY_PBA_REV_04) == 0) {
						printk("touchkey_update succeeded\n");
						set_touchkey_debug('C');
						break;
					}
					printk("touchkey_update failed... retry...\n");
					set_touchkey_debug('f');
				}

				init_hw();	//after update, re initalize.

				get_touchkey_firmware(data);
				printk("%s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__,
				   data[1], data[2]);
			}
		}  else if (get_hw_rev() >= 0x05){	// PBA rev0.4
			firm_version = 0x07;
			if ((data[1] != 0x07) || (data[2] != 0x03)) // need to change
			{
				  printk("%s : after PCB REV04, update SD-03D...\n",__func__);
				extern int ISSP_main(int touchkey_pba_rev);
				set_touchkey_debug('U');
				while (retry--) {
					 if (ISSP_main(TOUCHKEY_PBA_REV_05) == 0) {
						  printk("touchkey_update succeeded\n");
						  set_touchkey_debug('C');
						  break;
					 }
					 printk("touchkey_update failed... retry...\n");
					 set_touchkey_debug('f');
				}
				init_hw(); //after update, re initalize.
				get_touchkey_firmware(data);
				printk("%s change to F/W version: 0x%x, Module version:0x%x\n", __FUNCTION__, data[1], data[2]);
			}
		}
#endif
	ret = i2c_add_driver(&touchkey_i2c_driver);

	if (ret) {
		printk ("sec touch keypad registration failed, module not inserted.ret= %d\n", ret);
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
MODULE_DESCRIPTION("sec touch keypad");
