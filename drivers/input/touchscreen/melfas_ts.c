/* drivers/input/touchscreen/melfas_ts.c
 *
 * Copyright (C) 2010 Melfas, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define SEC_TSP
#ifdef SEC_TSP
#define ENABLE_NOISE_TEST_MODE
#define TSP_FACTORY_TEST
#if defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K) || defined (CONFIG_KOR_MODEL_SHV_E120L) \
    || defined (CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L)  || defined (CONFIG_JPN_MODEL_SC_05D)
#define TSP_BOOST
#else 
#undef TSP_BOOST
#endif
#endif

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/input/mt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/melfas_ts.h>
#include <linux/mfd/pmic8058.h>
#ifdef SEC_TSP
#include <linux/gpio.h>
#endif

#include <../mach-msm/smd_private.h>
#include <../mach-msm/smd_rpcrouter.h>

#undef TOUCH_NON_SLOT

#define TS_MAX_Z_TOUCH			255
#define TS_MAX_W_TOUCH		100


#define TS_MAX_X_COORD 		720
#define TS_MAX_Y_COORD 		1280

#ifdef SEC_TSP
#define P5_THRESHOLD 			0x05
#define TS_READ_REGS_LEN		5
#define TS_WRITE_REGS_LEN		16
#endif

#define TS_READ_REGS_LEN 		66
#define MELFAS_MAX_TOUCH		11

#define DEBUG_PRINT 			1


#define SET_DOWNLOAD_BY_GPIO	1

// TSP registors.
#define MIP_CONTACT_ON_EVENT_THRES	0x05	// (VC25_02, _04 �� 110, VC25_03,_05� 40)
#define MIP_MOVING_EVENT_THRES		0x06	// jump limit (���� 1, ~ 255���� 1mm 12pixel �Դϴ�.)
#define MIP_ACTIVE_REPORT_RATE		0x07	// ��� hz (default 60,  30~ 255)
#define MIP_POSITION_FILTER_LEVEL	0x08	// = weight filter (default 40, ��� f/w�� 80, 1~ 255 ������)

#define TS_READ_START_ADDR			0x0F
#define TS_READ_START_ADDR2			0x10

#define MIP_TSP_REVISION				0xF0
#define MIP_HARDWARE_REVISION		0xF1
#define MIP_COMPATIBILITY_GROUP		0xF2
#define MIP_CORE_VERSION				0xF3
#define MIP_PRIVATECUSTOM_VERSION	0xF4
#define MIP_PUBLICCUSTOM_VERSION		0xF5
#define MIP_PRODUCT_CODE				0xF6

#define SET_TSP_CONFIG
#define TSP_PATTERN_TRACTKING

#if SET_DOWNLOAD_BY_GPIO
#include <mcs8000_download.h>
#endif // SET_DOWNLOAD_BY_GPIO

unsigned long saved_rate;					
static bool lock_status;

static int tsp_enabled;
int touch_is_pressed;
static unsigned char is_inputmethod = 0;
#ifdef TSP_BOOST
static unsigned char is_boost = 0;
#endif

struct muti_touch_info
{
	int strength;
	int width;	
	int posX;
	int posY;
};

struct melfas_ts_data 
{
	uint16_t addr;
	struct i2c_client *client; 
	struct input_dev *input_dev;
	struct work_struct  work;
	struct melfas_version *version;
	uint32_t flags;
	int (*power)(int on);
	int (*gpio)(void);
#ifdef TA_DETECTION
	void (*register_cb)(void*);
	void (*read_ta_status)(void*);
#endif
	struct early_suspend early_suspend;
};

struct melfas_ts_data *ts_data;

#ifdef SEC_TSP
extern struct class *sec_class;
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h);
static void melfas_ts_late_resume(struct early_suspend *h);
#endif
#ifdef SEC_TSP
static int melfas_ts_suspend(struct i2c_client *client, pm_message_t mesg);
static int melfas_ts_resume(struct i2c_client *client);
static void release_all_fingers(struct melfas_ts_data *ts);
static int melfas_set_config(struct i2c_client *client, u8 reg, u8 value);
static int melfas_i2c_write(struct i2c_client *client, char *buf, int length);
static void TSP_reboot(void);
#endif

static struct muti_touch_info g_Mtouch_info[MELFAS_MAX_TOUCH];


static int melfas_init_panel(struct melfas_ts_data *ts)
{
	int buf = 0x00;
	int ret;
	ret = i2c_master_send(ts->client, &buf, 1);

	ret = i2c_master_send(ts->client, &buf, 1);

	if(ret <0)
	{
		printk(KERN_ERR "%s : i2c_master_send() failed\n [%d]", __func__, ret);	
		return 0;
	}

	return true;
}

#ifdef TA_DETECTION
static void tsp_ta_probe(int ta_status)
{
	u8 write_buffer[3];

	printk(KERN_ERR"[TSP] %s : TA is %s. \n", __func__, ta_status ? "on" : "off");
	if (tsp_enabled == false) {
		printk(KERN_ERR"[TSP] tsp_enabled is 0\n");
		return;
	}

	write_buffer[0] = 0xB0;
	write_buffer[1] = 0x11;

	if (ta_status)
		write_buffer[2] = 1;
	else
		write_buffer[2] = 0;

	melfas_i2c_write(ts_data->client, (char *)write_buffer, 3);
}
#endif

#ifdef TSP_BOOST
static void TSP_boost(struct melfas_ts_data *ts, bool onoff)
{
	printk(KERN_ERR "[TSP] TSP_boost %s\n", is_boost ? "ON" : "Off" );
	if (onoff) {
		melfas_set_config(ts->client, MIP_POSITION_FILTER_LEVEL, 2);
	} else {
		melfas_set_config(ts->client, MIP_POSITION_FILTER_LEVEL, 80);
	}
}
#endif

#ifdef TSP_PATTERN_TRACTKING
/* To do forced calibration when ghost touch occured at the same point
    for several second. */
#define MAX_GHOSTCHECK_FINGER 				10
#define MAX_GHOSTTOUCH_COUNT					300		// 5s, 60Hz
#define MAX_GHOSTTOUCH_BY_PATTERNTRACKING	5
static int tcount_finger[MAX_GHOSTCHECK_FINGER] = {0,0,0,0,0,0,0,0,0,0};
static int touchbx[MAX_GHOSTCHECK_FINGER] = {0,0,0,0,0,0,0,0,0,0};
static int touchby[MAX_GHOSTCHECK_FINGER] = {0,0,0,0,0,0,0,0,0,0};
static int ghosttouchcount = 0;
static int cFailbyPattenTracking = 0;

static void clear_tcount(void)
{
	int i;
	for(i=0;i<MAX_GHOSTCHECK_FINGER;i++){
		tcount_finger[i] = 0;
		touchbx[i] = 0;
		touchby[i] = 0;
	}		
}

static int diff_two_point(int x, int y, int oldx, int oldy)
{
	int diffx,diffy;
	int distance;
	
	diffx = x-oldx;
	diffy = y-oldy;
	distance = abs(diffx) + abs(diffy);

	if(distance < 3) return 1;
	else return 0;
}

static int tsp_pattern_tracking(struct melfas_ts_data *ts, int fingerindex, int x, int y)
{
	int i;
	int ghosttouch = 0;

	for( i = 0; i< MAX_GHOSTCHECK_FINGER; i++)
	{
		if( i == fingerindex){
			//if((touchbx[i] == x)&&(touchby[i] == y))
			if(diff_two_point(x,y, touchbx[i], touchby[i]))
			{
				tcount_finger[i] = tcount_finger[i]+1;
			}
			else
			{
				tcount_finger[i] = 0;
			}

			touchbx[i] = x;
			touchby[i] = y;

			if(tcount_finger[i]> MAX_GHOSTTOUCH_COUNT){
				ghosttouch = 1;
				ghosttouchcount++;
				printk(KERN_DEBUG "[TSP] SUNFLOWER (PATTERN TRACKING) %d\n",ghosttouchcount);
				clear_tcount();

				cFailbyPattenTracking++;
				if(cFailbyPattenTracking > MAX_GHOSTTOUCH_BY_PATTERNTRACKING){
					cFailbyPattenTracking = 0;
					printk("[TSP] Reboot.\n");
					TSP_reboot();
				}
				else{
					/* Do something for calibration */
				}
			}
		}
	}
	return ghosttouch;
}
#endif

static void melfas_ts_get_data(struct work_struct *work)
{
	struct melfas_ts_data *ts = container_of(work, struct melfas_ts_data, work);
	int ret = 0, i;
	uint8_t buf[TS_READ_REGS_LEN];
	int read_num, FingerID;
	int _touch_is_pressed, line;

	if (tsp_enabled == false) {
		printk(KERN_ERR "[TSP ]%s. tsp_disabled.\n", __func__);
		return 1;
	}
#if DEBUG_PRINT
	printk(KERN_ERR "%s start\n", __func__);

	if(ts ==NULL)
			printk(KERN_ERR "%s : TS NULL\n", __func__);
#endif 
	buf[0] = TS_READ_START_ADDR;

	ret = i2c_master_send(ts->client, buf, 1);
	if(ret < 0)
	{
		line = __LINE__;
		goto tsp_error;
	}
	ret = i2c_master_recv(ts->client, buf, 1);
	if(ret < 0)
	{
		line = __LINE__;
		goto tsp_error;
	}

	read_num = buf[0];
	
	if(read_num>0 && read_num < TS_READ_REGS_LEN)
	{
		buf[0] = TS_READ_START_ADDR2;

		ret = i2c_master_send(ts->client, buf, 1);
		if(ret < 0)
		{
			line = __LINE__;
			goto tsp_error;
		}
		ret = i2c_master_recv(ts->client, buf, read_num);
		if(ret < 0)
		{
			line = __LINE__;
			goto tsp_error;
		}

		if(buf[0] == 0x0F){
			printk(KERN_ERR "[TSP] ESD protection. (%d)\n", __LINE__);
			TSP_reboot();
			return;
		}

		for(i=0; i<read_num; i=i+6)
		{
			FingerID = (buf[i] & 0x0F) - 1;
			g_Mtouch_info[FingerID].posX= (uint16_t)(buf[i+1] & 0x0F) << 8 | buf[i+2];
			g_Mtouch_info[FingerID].posY= (uint16_t)(buf[i+1] & 0xF0) << 4 | buf[i+3];	
			if(g_Mtouch_info[FingerID].posX > TS_MAX_X_COORD	|| g_Mtouch_info[FingerID].posY > TS_MAX_Y_COORD)
			{
				printk(KERN_ERR "[TSP_ERR] Coordinate data error(%d). X:%d, Y:%d\n", __LINE__, g_Mtouch_info[FingerID].posX, g_Mtouch_info[FingerID].posY);
				line = __LINE__;
				goto tsp_data_error;
			}

			if((buf[i] & 0x80)==0)
				g_Mtouch_info[FingerID].strength = 0;
			else
				g_Mtouch_info[FingerID].strength = buf[i+4];
			
			g_Mtouch_info[FingerID].width= buf[i+5];					
		}
	
	}
	else
	{
		printk(KERN_ERR "[TSP_ERR] Size of data error(%d). read_num:%d\n", __LINE__, read_num);
		line = __LINE__;
		goto tsp_data_error;
	}

	_touch_is_pressed=0;

	for(i=0; i<MELFAS_MAX_TOUCH; i++)
	{
		if(g_Mtouch_info[i].strength== -1)
			continue;
		
#ifdef TSP_PATTERN_TRACTKING
		tsp_pattern_tracking(ts, i, g_Mtouch_info[i].posX, g_Mtouch_info[i].posY);
#endif

#if TOUCH_NON_SLOT
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, i);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, g_Mtouch_info[i].posX);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, g_Mtouch_info[i].posY);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, g_Mtouch_info[i].strength );
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, g_Mtouch_info[i].width);
		input_mt_sync(ts->input_dev);
#else
		input_mt_slot(ts->input_dev, i); 
 		input_mt_report_slot_state(ts->input_dev, 
 					MT_TOOL_FINGER, !!g_Mtouch_info[i].strength); 
    
 		if(g_Mtouch_info[i].strength != 0){
 			input_report_abs(ts->input_dev, ABS_MT_POSITION_X,  g_Mtouch_info[i].posX); 
 			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,  g_Mtouch_info[i].posY); 
 			input_report_abs(ts->input_dev, ABS_MT_PRESSURE,    g_Mtouch_info[i].strength); 
 			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, g_Mtouch_info[i].strength); 
 		} 
#endif
#if DEBUG_PRINT
		printk(KERN_ERR "[TSP] ID: %d, State : %d, x: %d, y: %d, z: %d w: %d\n", 
			i, (g_Mtouch_info[i].strength>0), g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].strength, g_Mtouch_info[i].width);
#endif

		if(g_Mtouch_info[i].strength == 0)
			g_Mtouch_info[i].strength = -1;

		if(g_Mtouch_info[i].strength > 0)
			_touch_is_pressed = 1;
        
	}
	input_sync(ts->input_dev);
	touch_is_pressed = _touch_is_pressed;

	return ;

tsp_error:
	printk(KERN_ERR "[TSP_ERR] %s: i2c failed(%d)\n", __func__, __LINE__);
	TSP_reboot();
	return;
tsp_data_error:
	printk(KERN_ERR "[TSP_ERR] %s: tsp_data_error(%d)\n", __func__, __LINE__);
	TSP_reboot();
	return;

}

static irqreturn_t melfas_ts_irq_handler(int irq, void *handle)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)handle;
#if DEBUG_PRINT
	printk(KERN_ERR "melfas_ts_irq_handler\n");
#endif
		
	melfas_ts_get_data(&ts->work);
	
	return IRQ_HANDLED;
}

#ifdef SEC_TSP
static int melfas_i2c_read(struct i2c_client *client, u16 addr, u16 length, u8 *value)
{
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg[2];

	msg[0].addr  = client->addr;
	msg[0].flags = 0x00;
	msg[0].len   = 2;
	msg[0].buf   = (u8 *) &addr;

	msg[1].addr  = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = length;
	msg[1].buf   = (u8 *) value;

	if  (i2c_transfer(adapter, msg, 2) == 2)
		return 0;
	else
		return -EIO;

}


static int melfas_i2c_write(struct i2c_client *client, char *buf, int length)
{
	int i;
	char data[TS_WRITE_REGS_LEN];

	if (length > TS_WRITE_REGS_LEN) {
		pr_err("[TSP] size error - %s\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < length; i++)
		data[i] = *buf++;

	i = i2c_master_send(client, (char *)data, length);

	if (i == length)
		return length;
	else
		return -EIO;
}

static ssize_t set_tsp_firm_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);

	return sprintf(buf, "%#02x, %#02x, %#02x\n", ts->version->core, ts->version->private, ts->version->public);
}

static ssize_t set_tsp_firm_version_read_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 fw_latest_version, privatecustom_version, publiccustom_version;
	int ret;
	uint8_t buff[7] = {0,};

	buff[0] = MIP_TSP_REVISION;
	ret = i2c_master_send(ts->client, &buff, 1);
	if(ret < 0)
	{
		printk(KERN_ERR "%s : i2c_master_send [%d]\n", __func__, ret);
	}

	ret = i2c_master_recv(ts->client, &buff, 7);
	if(ret < 0)
	{
		printk(KERN_ERR "%s : i2c_master_recv [%d]\n", __func__, ret);
	}
	fw_latest_version 		= buff[3];
	privatecustom_version 	= buff[4];
	publiccustom_version 	= buff[5];

	return sprintf(buf, "%#02x, %#02x, %#02x\n", fw_latest_version, privatecustom_version, publiccustom_version);
}

static ssize_t set_tsp_threshold_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 threshold;

	melfas_i2c_read(ts->client, P5_THRESHOLD, 1, &threshold);

	return sprintf(buf, "%d\n", threshold);
}

ssize_t set_tsp_for_inputmethod_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk(KERN_ERR "[TSP] %s is called.. is_inputmethod=%d\n", __func__, is_inputmethod);
	if (is_inputmethod)
		*buf = '1';
	else
		*buf = '0';

	return 0;
}
ssize_t set_tsp_for_inputmethod_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u16 obj_address = 0;
	u16 size_one;
	int ret;
	u8 value;
	int jump_limit = 0;
	int mrgthr = 0;
	u8 val = 0;
	unsigned int register_address = 0;

	if (tsp_enabled == false) {
		printk(KERN_ERR "[TSP ]%s. tsp_enabled is 0\n", __func__);
		return 1;
	}


	if (*buf == '1' && (!is_inputmethod)) {
		is_inputmethod = 1;
		printk(KERN_ERR "[TSP] Set TSP inputmethod IN\n");
		/* to do */
	} else if (*buf == '0' && (is_inputmethod)) {
		is_inputmethod = 0;
		printk(KERN_ERR "[TSP] Set TSP inputmethod OUT\n");
		/* to do */
	}
	return 1;
}

static ssize_t tsp_call_release_touch(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);

	printk(" %s is called\n", __func__);
	TSP_reboot();

	return sprintf(buf,"0\n");
}

static ssize_t tsp_touchtype_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char temp[15];

	sprintf(temp, "TSP : MMS144\n");
	strcat(buf, temp);
	return strlen(buf);
}

#ifdef TSP_BOOST
ssize_t set_tsp_for_boost_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk(KERN_ERR "[TSP] %s is called.. is_inputmethod=%d\n", __func__, is_boost);
	if (is_boost)
		*buf = '1';
	else
		*buf = '0';

	return 0;
}
ssize_t set_tsp_for_boost_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u16 obj_address = 0;
	u16 size_one;
	int ret;
	u8 value;
	int jump_limit = 0;
	int mrgthr = 0;
	u8 val = 0;
	unsigned int register_address = 0;

	if (tsp_enabled == false) {
		printk(KERN_ERR "[TSP ]%s. tsp_enabled is 0\n", __func__);
		return 1;
	}

	if (*buf == '1' && (!is_boost)) {
		is_boost = 1;
	} else if (*buf == '0' && (is_boost)) {
		is_boost = 0;
	}
	printk(KERN_ERR "[TSP] set_tsp_for_boost_store() called. %s!\n", is_boost ? "On" : "Off" );
	TSP_boost(ts, is_boost);

	return 1;
}
#endif

static DEVICE_ATTR(tsp_firm_version_phone, S_IRUGO | S_IWUSR | S_IWGRP, set_tsp_firm_version_show, NULL);/* PHONE*/	/* firmware version resturn in phone driver version */
static DEVICE_ATTR(tsp_firm_version_panel, S_IRUGO | S_IWUSR | S_IWGRP, set_tsp_firm_version_read_show, NULL);/*PART*/	/* firmware version resturn in TSP panel version */
static DEVICE_ATTR(tsp_threshold, S_IRUGO | S_IWUSR | S_IWGRP, set_tsp_threshold_mode_show, NULL);
static DEVICE_ATTR(set_tsp_for_inputmethod, S_IRUGO | S_IWUSR | S_IWGRP, set_tsp_for_inputmethod_show, set_tsp_for_inputmethod_store); /* For 3x4 Input Method, Jump limit changed API */
static DEVICE_ATTR(call_release_touch, S_IRUGO | S_IWUSR | S_IWGRP, tsp_call_release_touch, NULL);
static DEVICE_ATTR(mxt_touchtype, S_IRUGO | S_IWUSR | S_IWGRP,	tsp_touchtype_show, NULL);
#ifdef TSP_BOOST
static DEVICE_ATTR(set_tsp_boost, S_IRUGO | S_IWUSR | S_IWGRP, set_tsp_for_boost_show, set_tsp_for_boost_store); /* Control wait_filter to boost response. */
#endif

static struct attribute *sec_touch_attributes[] = {
	&dev_attr_tsp_firm_version_phone.attr,
	&dev_attr_tsp_firm_version_panel.attr,
	&dev_attr_tsp_threshold.attr,
	&dev_attr_set_tsp_for_inputmethod.attr,
	&dev_attr_call_release_touch.attr,
	&dev_attr_mxt_touchtype.attr,
#ifdef TSP_BOOST
	&dev_attr_set_tsp_boost.attr,
#endif
	NULL,
};

static struct attribute_group sec_touch_attr_group = {
	.attrs = sec_touch_attributes,
};
#endif

#ifdef TSP_FACTORY_TEST
static bool debug_print = true;
static u16 inspection_data[370] = { 0, };
static u16 lntensity_data[370] = { 0, };

static ssize_t set_tsp_module_on_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	struct melfas_ts_data *ts = dev_get_drvdata(dev);

	ret = melfas_ts_resume(ts->client);
	
	if (ret  == 0)
		*buf = '1';
	else
		*buf = '0';

	msleep(500);
	return 0;
}

static ssize_t set_tsp_module_off_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;
	struct melfas_ts_data *ts = dev_get_drvdata(dev);

	ret = melfas_ts_suspend(ts->client, PMSG_SUSPEND);

	if (ret  == 0)
		*buf = '1';
	else
		*buf = '0';

	return 0;
}

static int check_debug_data(struct melfas_ts_data *ts)
{
	u8 write_buffer[6];
	u8 read_buffer[2];
	int sensing_line, exciting_line;
	int ret = 0;
	int gpio = ts->client->irq - NR_MSM_IRQS;

	disable_irq(ts->client->irq);

	/* enter the debug mode */

	write_buffer[0] = 0xA0;
	write_buffer[1] = 0x1A;
	write_buffer[2] = 0x0;
	write_buffer[3] = 0x0;
	write_buffer[4] = 0x0;

	if (debug_print)
		pr_info("[TSP] read inspenction data\n");
	write_buffer[5] = 0x03;
	for (sensing_line = 0; sensing_line < 14; sensing_line++) {
		printk("sensing_line %02d ==> ", sensing_line);
		for (exciting_line =0; exciting_line < 26; exciting_line++) {
			write_buffer[2] = exciting_line;
			write_buffer[3] = sensing_line;
			melfas_i2c_write(ts->client, (char *)write_buffer, 6);
			melfas_i2c_read(ts->client, 0xA8, 2, read_buffer);
			lntensity_data[exciting_line + sensing_line * 26] =
				(read_buffer[1] & 0xf) << 8 | read_buffer[0];
			printk("%d.", lntensity_data[exciting_line + sensing_line * 26]);
			if(lntensity_data[exciting_line + sensing_line * 26] < 900 
			|| lntensity_data[exciting_line + sensing_line * 26] > 3000)
				ret = -1;
		}
		printk(" \n");
	}
	pr_info("[TSP] Reading data end.\n");

	enable_irq(ts->client->irq);

	return ret;
}

static ssize_t set_all_refer_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int status;
	struct melfas_ts_data *ts = dev_get_drvdata(dev);

	status = check_debug_data(ts);

	return sprintf(buf, "%u\n", status);
}

static int index =0;

static int atoi(char *str)
{
	int result = 0;
	int count = 0;
	if( str == NULL ) 
		return -1;
	while( str[count] != NULL && str[count] >= '0' && str[count] <= '9' )
	{		
		result = result * 10 + str[count] - '0';
		++count;
	}
	return result;
}

ssize_t disp_all_refdata_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%u\n",  lntensity_data[index]);
}

ssize_t disp_all_refdata_store(struct device *dev, struct device_attribute *attr,
								   const char *buf, size_t size)
{
	index = atoi(buf);

	printk(KERN_ERR "%s : value %d\n", __func__, index);

  	return size;
}

static int check_delta_data(struct melfas_ts_data *ts)
{
	u8 write_buffer[6];
	u8 read_buffer[2];
	int sensing_line, exciting_line;
	int gpio = ts->client->irq - NR_MSM_IRQS;
	int ret = 0;
	disable_irq(ts->client->irq);
	/* enter the debug mode */
	write_buffer[0] = 0xA0;
	write_buffer[1] = 0x1A;
	write_buffer[2] = 0x0;
	write_buffer[3] = 0x0;
	write_buffer[4] = 0x0;
	write_buffer[5] = 0x01;
	melfas_i2c_write(ts->client, (char *)write_buffer, 6);

	/* wating for the interrupt*/
	while (gpio_get_value(gpio)) {
		printk(".");
		udelay(100);
	}

	if (debug_print)
		pr_info("[TSP] read dummy\n");

	/* read the dummy data */
	melfas_i2c_read(ts->client, 0xA8, 2, read_buffer);

	if (debug_print)
		pr_info("[TSP] read inspenction data\n");
	write_buffer[5] = 0x02;
	for (sensing_line = 0; sensing_line < 14; sensing_line++) {
		for (exciting_line =0; exciting_line < 26; exciting_line++) {
			write_buffer[2] = exciting_line;
			write_buffer[3] = sensing_line;
			melfas_i2c_write(ts->client, (char *)write_buffer, 6);
			melfas_i2c_read(ts->client, 0xA8, 2, read_buffer);
			inspection_data[exciting_line + sensing_line * 26] =
				(read_buffer[1] & 0xf) << 8 | read_buffer[0];
			printk("%d.", inspection_data[exciting_line + sensing_line * 26]);
			if(inspection_data[exciting_line + sensing_line * 26] < 100 
			|| inspection_data[exciting_line + sensing_line * 26] > 900)
				ret = -1;
		}
		printk(" \n");
	}
	pr_info("[TSP] Reading data end.\n");

//	release_all_fingers(ts);

	msleep(200);
	release_all_fingers(ts);
	touch_is_pressed = 0;
	ts->gpio();
	ts->power(false);

	msleep(200);
	ts->power(true);
#ifdef TSP_BOOST
	TSP_boost(ts, is_boost);
#endif
	enable_irq(ts->client->irq);

	return ret;
}

static ssize_t set_all_delta_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int status = 0;
	struct melfas_ts_data *ts = dev_get_drvdata(dev);

	status = check_delta_data(ts);

	set_tsp_module_off_show(dev, attr, buf);
	set_tsp_module_on_show(dev, attr, buf);

	return sprintf(buf, "%u\n", status);
}

ssize_t disp_all_deltadata_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%u\n",  inspection_data[index]);
}


ssize_t disp_all_deltadata_store(struct device *dev, struct device_attribute *attr,
								   const char *buf, size_t size)
{
	index = atoi(buf);
	printk(KERN_ERR "Delta data %d", index);
  	return size;
}

static void check_intensity_data(struct melfas_ts_data *ts)
{

	u8 write_buffer[6];
	u8 read_buffer[2];
	int sensing_line, exciting_line;
	int gpio = ts->client->irq - NR_MSM_IRQS;

	disable_irq(ts->client->irq);
	if (0 == inspection_data[0]) {
		/* enter the debug mode */
		write_buffer[0] = 0xA0;
		write_buffer[1] = 0x1A;
		write_buffer[2] = 0x0;
		write_buffer[3] = 0x0;
		write_buffer[4] = 0x0;
		write_buffer[5] = 0x01;
		melfas_i2c_write(ts->client, (char *)write_buffer, 6);

		/* wating for the interrupt*/
		while (gpio_get_value(gpio)) {
			printk(".");
			udelay(100);
		}

		/* read the dummy data */
		melfas_i2c_read(ts->client, 0xA8, 2, read_buffer);

		write_buffer[5] = 0x02;
		for (sensing_line = 0; sensing_line < 14; sensing_line++) {
			for (exciting_line =0; exciting_line < 26; exciting_line++) {
				write_buffer[2] = exciting_line;
				write_buffer[3] = sensing_line;
				melfas_i2c_write(ts->client, (char *)write_buffer, 6);
				melfas_i2c_read(ts->client, 0xA8, 2, read_buffer);
				inspection_data[exciting_line + sensing_line * 26] =
					(read_buffer[1] & 0xf) << 8 | read_buffer[0];
			}
		}
		melfas_ts_suspend(ts->client, PMSG_SUSPEND);
		msleep(200);
		melfas_ts_resume(ts->client);
	}

	write_buffer[0] = 0xA0;
	write_buffer[1] = 0x1A;
	write_buffer[4] = 0x0;
	write_buffer[5] = 0x04;
	for (sensing_line = 0; sensing_line < 14; sensing_line++) {
		for (exciting_line =0; exciting_line < 26; exciting_line++) {
			write_buffer[2] = exciting_line;
			write_buffer[3] = sensing_line;
			melfas_i2c_write(ts->client, (char *)write_buffer, 6);
			melfas_i2c_read(ts->client, 0xA8, 2, read_buffer);
			lntensity_data[exciting_line + sensing_line * 26] =
				(read_buffer[1] & 0xf) << 8 | read_buffer[0];
		}
	}
	enable_irq(ts->client->irq);
/*
	pr_info("[TSP] lntensity data");
	int i;
	for (i = 0; i < 14*16; i++) {
		if (0 == i % 26)
			printk("\n");
		printk("%2u, ", lntensity_data[i]);
	}
*/
}

static ssize_t set_refer0_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 refrence = 0;
	struct melfas_ts_data *ts = dev_get_drvdata(dev);

	check_intensity_data(ts);

	refrence = inspection_data[28];
	return sprintf(buf, "%u\n", refrence);
}

static ssize_t set_refer1_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 refrence = 0;
	refrence = inspection_data[288];
	return sprintf(buf, "%u\n", refrence);
}

static ssize_t set_refer2_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 refrence = 0;
	refrence = inspection_data[194];
	return sprintf(buf, "%u\n", refrence);
}

static ssize_t set_refer3_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 refrence = 0;
	refrence = inspection_data[49];
	return sprintf(buf, "%u\n", refrence);
}

static ssize_t set_refer4_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 refrence = 0;
	refrence = inspection_data[309];
	return sprintf(buf, "%u\n", refrence);
}

static ssize_t set_intensity0_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 intensity = 0;
	intensity = lntensity_data[28];
	return sprintf(buf, "%u\n", intensity);
}

static ssize_t set_intensity1_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 intensity = 0;
	intensity = lntensity_data[288];
	return sprintf(buf, "%u\n", intensity);
}

static ssize_t set_intensity2_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 intensity = 0;
	intensity = lntensity_data[194];
	return sprintf(buf, "%u\n", intensity);
}

static ssize_t set_intensity3_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 intensity = 0;
	intensity = lntensity_data[49];
	return sprintf(buf, "%u\n", intensity);
}

static ssize_t set_intensity4_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 intensity = 0;
	intensity = lntensity_data[309];
	return sprintf(buf, "%u\n", intensity);
}


static DEVICE_ATTR(set_module_on, S_IRUGO | S_IWUSR | S_IWGRP, set_tsp_module_on_show, NULL);
static DEVICE_ATTR(set_module_off, S_IRUGO | S_IWUSR | S_IWGRP, set_tsp_module_off_show, NULL);
static DEVICE_ATTR(set_all_refer, S_IRUGO | S_IWUSR | S_IWGRP, set_all_refer_mode_show, NULL);
static DEVICE_ATTR(disp_all_refdata, S_IRUGO | S_IWUSR | S_IWGRP, disp_all_refdata_show, disp_all_refdata_store);
static DEVICE_ATTR(set_all_delta, S_IRUGO | S_IWUSR | S_IWGRP, set_all_delta_mode_show, NULL);
static DEVICE_ATTR(disp_all_deltadata, S_IRUGO | S_IWUSR | S_IWGRP, disp_all_deltadata_show, disp_all_deltadata_store);
static DEVICE_ATTR(set_refer0, S_IRUGO | S_IWUSR | S_IWGRP, set_refer0_mode_show, NULL);
static DEVICE_ATTR(set_delta0, S_IRUGO | S_IWUSR | S_IWGRP, set_intensity0_mode_show, NULL);
static DEVICE_ATTR(set_refer1, S_IRUGO | S_IWUSR | S_IWGRP, set_refer1_mode_show, NULL);
static DEVICE_ATTR(set_delta1, S_IRUGO | S_IWUSR | S_IWGRP, set_intensity1_mode_show, NULL);
static DEVICE_ATTR(set_refer2, S_IRUGO | S_IWUSR | S_IWGRP, set_refer2_mode_show, NULL);
static DEVICE_ATTR(set_delta2, S_IRUGO | S_IWUSR | S_IWGRP, set_intensity2_mode_show, NULL);
static DEVICE_ATTR(set_refer3, S_IRUGO | S_IWUSR | S_IWGRP, set_refer3_mode_show, NULL);
static DEVICE_ATTR(set_delta3, S_IRUGO | S_IWUSR | S_IWGRP, set_intensity3_mode_show, NULL);
static DEVICE_ATTR(set_refer4, S_IRUGO | S_IWUSR | S_IWGRP, set_refer4_mode_show, NULL);
static DEVICE_ATTR(set_delta4, S_IRUGO | S_IWUSR | S_IWGRP, set_intensity4_mode_show, NULL);
static DEVICE_ATTR(set_threshould, S_IRUGO | S_IWUSR | S_IWGRP, set_tsp_threshold_mode_show, NULL);	/* touch threshold return */

static struct attribute *sec_touch_facotry_attributes[] = {
	&dev_attr_set_module_on.attr,
	&dev_attr_set_module_off.attr,
	&dev_attr_set_all_refer.attr,
	&dev_attr_disp_all_refdata.attr,
	&dev_attr_set_all_delta.attr,
	&dev_attr_disp_all_deltadata.attr,
	&dev_attr_set_refer0.attr,
	&dev_attr_set_delta0.attr,
	&dev_attr_set_refer1.attr,
	&dev_attr_set_delta1.attr,
	&dev_attr_set_refer2.attr,
	&dev_attr_set_delta2.attr,
	&dev_attr_set_refer3.attr,
	&dev_attr_set_delta3.attr,
	&dev_attr_set_refer4.attr,
	&dev_attr_set_delta4.attr,
	&dev_attr_set_threshould.attr,
	NULL,
};

static struct attribute_group sec_touch_factory_attr_group = {
	.attrs = sec_touch_facotry_attributes,
};
#endif


static void release_all_fingers(struct melfas_ts_data *ts)
{
	int i;

	printk(KERN_ERR "%s start.\n", __func__);
	for(i=0; i<MELFAS_MAX_TOUCH; i++) {
		if(-1 == g_Mtouch_info[i].strength) {
			g_Mtouch_info[i].posX = 0;
			g_Mtouch_info[i].posY = 0;
			continue;
		}
	printk(KERN_ERR "%s %s(%d)\n", __func__, ts->input_dev->name, i);

		g_Mtouch_info[i].strength = 0;
#if TOUCH_NON_SLOT
		input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, i);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_X, g_Mtouch_info[i].posX);
		input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, g_Mtouch_info[i].posY);
		input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, g_Mtouch_info[i].strength);
		input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, g_Mtouch_info[i].width);
		input_mt_sync(ts->input_dev);
#else
		input_mt_slot(ts->input_dev, i); 
 		input_mt_report_slot_state(ts->input_dev, 
 					MT_TOOL_FINGER, !!g_Mtouch_info[i].strength); 
    
 		if(g_Mtouch_info[i].strength != 0){
 			input_report_abs(ts->input_dev, ABS_MT_POSITION_X,  g_Mtouch_info[i].posX); 
 			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,  g_Mtouch_info[i].posY); 
 			input_report_abs(ts->input_dev, ABS_MT_PRESSURE,    g_Mtouch_info[i].strength); 
 			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, g_Mtouch_info[i].strength); 
 		} 
#endif

		g_Mtouch_info[i].posX = 0;
		g_Mtouch_info[i].posY = 0;

		if(0 == g_Mtouch_info[i].strength)
			g_Mtouch_info[i].strength = -1;
	}
	input_sync(ts->input_dev);
}

static void TSP_reboot(void)
{
	if(tsp_enabled == false)
		return;

	printk(KERN_ERR "%s satrt!\n", __func__);

	disable_irq_nosync(ts_data->client->irq);
	tsp_enabled = false;
	touch_is_pressed = 0;

	release_all_fingers(ts_data);
	ts_data->gpio();
	ts_data->power(false);
	msleep(200);
	ts_data->power(true);
#ifdef TSP_BOOST
	TSP_boost(ts_data, is_boost);
#endif
	enable_irq(ts_data->client->irq);

	tsp_enabled = true;
};

void TSP_force_released(void)
{
	printk(KERN_ERR "%s satrt!\n", __func__);

	if (tsp_enabled == false) {
		printk(KERN_ERR "[TSP] Disabled\n");
		return;
	}
	release_all_fingers(ts_data);

	touch_is_pressed = 0;
};
EXPORT_SYMBOL(TSP_force_released);

void TSP_ESD_seq(void)
{
	TSP_reboot();
	printk(KERN_ERR "%s satrt!\n", __func__);
};
EXPORT_SYMBOL(tsp_call_release_touch);

#ifdef SET_TSP_CONFIG
static int melfas_set_config(struct i2c_client *client, u8 reg, u8 value)
{
	u8 buffer[2];
	int ret;
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	buffer[0] = reg;
	buffer[1] = value;
	ret = melfas_i2c_write(ts->client, (char *)buffer, 2);

	return ret;	
}
#endif

static int tsp_reboot_count = 0;
static int melfas_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct melfas_ts_data *ts;
	struct melfas_tsi_platform_data *data;
#ifdef SEC_TSP
	struct device *sec_touchscreen;
	struct device *qt602240_noise_test;
#ifdef TA_DETECTION
	bool ta_status;
#endif
#endif

	int ret = 0, i; 
	
	uint8_t buf[7] = {0,};
	
init_again:
	tsp_enabled = false;
#if DEBUG_PRINT
	printk(KERN_ERR "%s start.\n", __func__);
#endif

#ifndef CONFIG_USA_MODEL_SGH_I757
    if (!gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(7))))
    {
        printk(KERN_ERR "%s: No TSP!\n", __func__);
        ret = -ENODEV;
        goto err_check_functionality_failed;
    }
#endif

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        printk(KERN_ERR "%s: need I2C_FUNC_I2C\n", __func__);
        ret = -ENODEV;
        goto err_check_functionality_failed;
    }

    ts = kmalloc(sizeof(struct melfas_ts_data), GFP_KERNEL);
    if (ts == NULL)
    {
        printk(KERN_ERR "%s: failed to create a state of melfas-ts\n", __func__);
        ret = -ENOMEM;
        goto err_alloc_data_failed;
    }
	ts_data = ts;
	data = client->dev.platform_data;
	ts->power = data->power;
	ts->gpio = data->gpio;
	ts->version = data->version;
#ifdef TA_DETECTION
	ts->register_cb = data->register_cb;
	ts->read_ta_status = data->read_ta_status;
#endif
	ts->client = client;
	i2c_set_clientdata(client, ts);
	ts->power(true);
	ret = i2c_master_send(ts->client, &buf, 1);

#ifdef CONFIG_USA_MODEL_SGH_I757
	if (ret < 0){
		if (!gpio_get_value_cansleep(PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(7)))){
			printk(KERN_ERR "%s: No TSP!\n", __func__);
			ts->power(false);
			kfree(ts);
			ret = -ENODEV;
			goto err_check_functionality_failed;
		}
	}
#endif

#if DEBUG_PRINT
	printk(KERN_ERR "%s: i2c_master_send() [%d], Add[%d]\n", __func__, ret, ts->client->addr);
#endif

#if SET_DOWNLOAD_BY_GPIO
	buf[0] = MIP_TSP_REVISION;
	ret = i2c_master_send(ts->client, &buf, 1);
	if(ret < 0)
	{
		printk(KERN_ERR "%s: i2c_master_send [%d]\n", __func__, ret);
	}

	ret = i2c_master_recv(ts->client, &buf, 7);
	if(ret < 0)
	{
		printk(KERN_ERR "%s: i2c_master_recv [%d]\n", __func__, ret);
	}
	printk(KERN_ERR "[TSP] FW_VERSION CORE: 0x%02x, PRIVATE: 0x%02x, PUBLIC: 0x%02x, temp: %d\n", buf[3], buf[4], buf[5], tsp_reboot_count);

	if(buf[3] != ts->version->core || buf[4] != ts->version->private || buf[5] != ts->version->public \
	|| tsp_reboot_count > 0)
	{
		printk(KERN_ERR "FW Upgrading... FW_VERSION CORE: 0x%02x, PRIVATE: 0x%02x, PUBLIC: 0x%02x\n", ts->version->core, ts->version->private, ts->version->public);
		ret = mcsdl_download_binary_data(data);
		if(ret == 0)
		{
			printk(KERN_ERR "SET Download Fail - error code [%d]\n", ret);
			goto err_detect_failed;
		} else {
			printk(KERN_ERR "FW Upgrading... success\n");
		}
	}	
#endif // SET_DOWNLOAD_BY_GPIO
	
	ts->input_dev = input_allocate_device();
    if (!ts->input_dev)
    {
		printk(KERN_ERR "%s: Not enough memory\n", __func__);
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	} 

	ts->input_dev->name = "sec_touchscreen" ;

	ts->input_dev->evbit[0] = BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);
	

	ts->input_dev->keybit[BIT_WORD(KEY_MENU)] |= BIT_MASK(KEY_MENU);
	ts->input_dev->keybit[BIT_WORD(KEY_HOME)] |= BIT_MASK(KEY_HOME);
	ts->input_dev->keybit[BIT_WORD(KEY_BACK)] |= BIT_MASK(KEY_BACK);		
	ts->input_dev->keybit[BIT_WORD(KEY_SEARCH)] |= BIT_MASK(KEY_SEARCH);			

#if TOUCH_NON_SLOT
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, TS_MAX_X_COORD, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, TS_MAX_Y_COORD, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, TS_MAX_Z_TOUCH, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, MELFAS_MAX_TOUCH-1, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, TS_MAX_W_TOUCH, 0, 0);
#else
	set_bit(MT_TOOL_FINGER, ts->input_dev->keybit); 
 	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit); 
  
 	input_mt_init_slots(ts->input_dev, MELFAS_MAX_TOUCH-1); 
  
 	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,  0, TS_MAX_X_COORD, 0, 0);
 	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,  0, TS_MAX_Y_COORD, 0, 0);
 	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE,    0, TS_MAX_Z_TOUCH, 0, 0);
 	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, TS_MAX_Z_TOUCH, 0, 0);
#endif

    ret = input_register_device(ts->input_dev);
    if (ret)
    {
        printk(KERN_ERR "%s: Failed to register device\n", __func__);
        ret = -ENOMEM;
        goto err_input_register_device_failed;
    }

    if (ts->client->irq)
    {
#if DEBUG_PRINT
        printk(KERN_ERR "%s: trying to request irq: %s-%d\n", __func__, ts->client->name, ts->client->irq);
#endif
		ret = request_threaded_irq(client->irq, NULL, melfas_ts_irq_handler, IRQF_ONESHOT | IRQF_TRIGGER_LOW, ts->client->name, ts);
        if (ret > 0)
        {
            printk(KERN_ERR "%s: Can't allocate irq %d, ret %d\n", __func__, ts->client->irq, ret);
            ret = -EBUSY;
            goto err_request_irq;
        }
    }
	for (i = 0; i < MELFAS_MAX_TOUCH ; i++)  /* _SUPPORT_MULTITOUCH_ */
		g_Mtouch_info[i].strength = -1;	

	tsp_enabled = true;

#ifdef TA_DETECTION
	printk(KERN_ERR "[TSP] tsp_enabled is %d", tsp_enabled);
	data->register_cb(tsp_ta_probe);
	if (data->read_ta_status) {
		data->read_ta_status(&ta_status);
		printk(KERN_ERR "[TSP] ta_status is %d", ta_status);
		tsp_ta_probe(ta_status);
	}
#endif
#if DEBUG_PRINT	
	printk(KERN_ERR "%s: succeed to register input device\n", __func__);
#endif

#ifdef SEC_TSP
	sec_touchscreen = device_create(sec_class, NULL, 0, ts, "sec_touchscreen");
	if (IS_ERR(sec_touchscreen))
		pr_err("[TSP] Failed to create device for the sysfs\n");

	ret = sysfs_create_group(&sec_touchscreen->kobj, &sec_touch_attr_group);
	if (ret)
		pr_err("[TSP] Failed to create sysfs group\n");
#endif

#ifdef TSP_FACTORY_TEST
	qt602240_noise_test = device_create(sec_class, NULL, 0, ts, "qt602240_noise_test");
	if (IS_ERR(qt602240_noise_test))
		pr_err("[TSP] Failed to create device for the sysfs\n");

	ret = sysfs_create_group(&qt602240_noise_test->kobj, &sec_touch_factory_attr_group);
	if (ret)
		pr_err("[TSP] Failed to create sysfs group\n");
#endif

#if CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = melfas_ts_early_suspend;
	ts->early_suspend.resume = melfas_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif
#ifdef TSP_BOOST
	TSP_boost(ts, is_boost);
#endif
#if DEBUG_PRINT
	printk(KERN_INFO "%s: Start touchscreen. name: %s, irq: %d\n", __func__, ts->client->name, ts->client->irq);
#endif
	return 0;

err_request_irq:
	printk(KERN_ERR "melfas-ts: err_request_irq failed\n");
	free_irq(client->irq, ts);
err_input_register_device_failed:
	printk(KERN_ERR "melfas-ts: err_input_register_device failed\n");
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
	printk(KERN_ERR "melfas-ts: err_input_dev_alloc failed\n");
err_alloc_data_failed:
	printk(KERN_ERR "melfas-ts: err_alloc_data failed_\n");	
err_detect_failed:
	ts->power(false);
	printk(KERN_ERR "melfas-ts: err_detect failed\n");
	kfree(ts);
	if(tsp_reboot_count < 3){
		tsp_reboot_count++;
		goto init_again;
	}
err_check_functionality_failed:
	printk(KERN_ERR "melfas-ts: err_check_functionality failed_\n");

	return ret;
}

static int melfas_ts_remove(struct i2c_client *client)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	unregister_early_suspend(&ts->early_suspend);
	free_irq(client->irq, ts);
	ts->power(false);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	return 0;
}

static int melfas_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	int i;    
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	disable_irq(client->irq);
	tsp_enabled = false;
	release_all_fingers(ts);
	touch_is_pressed = 0;
	ts->gpio();
	ts->power(false);

	return 0;
}

static int melfas_ts_resume(struct i2c_client *client)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);
#ifdef TA_DETECTION
	bool ta_status=0;
#endif
	ts->power(true);
#ifdef TSP_BOOST
	TSP_boost(ts, is_boost);
#endif
	enable_irq(client->irq); // scl wave
	tsp_enabled = true;

#ifdef TA_DETECTION
	if (ts->read_ta_status) {
		ts->read_ta_status(&ta_status);
		printk(KERN_ERR "[TSP] ta_status is %d", ta_status);
		tsp_ta_probe(ta_status);
	}
#endif
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h)
{
	struct melfas_ts_data *ts;

	ts = container_of(h, struct melfas_ts_data, early_suspend);
	melfas_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void melfas_ts_late_resume(struct early_suspend *h)
{
	struct melfas_ts_data *ts;
	ts = container_of(h, struct melfas_ts_data, early_suspend);
	melfas_ts_resume(ts->client);
}
#endif

static const struct i2c_device_id melfas_ts_id[] =
{
    { MELFAS_TS_NAME, 0 },
    { }
};

static struct i2c_driver melfas_ts_driver =
{
    .driver = {
    .name = MELFAS_TS_NAME,
    },
    .id_table = melfas_ts_id,
    .probe = melfas_ts_probe,
    .remove = __devexit_p(melfas_ts_remove),
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend		= melfas_ts_suspend,
	.resume		= melfas_ts_resume,
#endif
};

#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

static int __devinit melfas_ts_init(void)
{
#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return 0\n", __func__);
		return 0;
	}
#endif

	return i2c_add_driver(&melfas_ts_driver);
}

static void __exit melfas_ts_exit(void)
{
	i2c_del_driver(&melfas_ts_driver);
}

MODULE_DESCRIPTION("Driver for Melfas MTSI Touchscreen Controller");
MODULE_AUTHOR("MinSang, Kim <kimms@melfas.com>");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");

module_init(melfas_ts_init);
module_exit(melfas_ts_exit);
