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
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/i2c/mxt224_celox.h>
#include <asm/unaligned.h>
#include <linux/firmware.h>
//#include <mach/cpufreq.h>
#ifdef CONFIG_TOUCHSCREEN_MXT768E
#include "mxt768e.h"
#endif
//imsi blocked by Xtopher #include <mach/sec_debug.h>
#include <linux/input/mt.h>   // SLOT

#include <../mach-msm/smd_private.h>
#include <../mach-msm/smd_rpcrouter.h>


#undef TOUCH_NON_SLOT // Use only for GB

#define OBJECT_TABLE_START_ADDRESS	7
#define OBJECT_TABLE_ELEMENT_SIZE	6

#define CMD_RESET_OFFSET		0
#define CMD_BACKUP_OFFSET		1
#define CMD_CALIBRATE_OFFSET    2
#define CMD_REPORTATLL_OFFSET   3
#define CMD_DEBUG_CTRL_OFFSET   4
#define CMD_DIAGNOSTIC_OFFSET   5


#define DETECT_MSG_MASK			0x80
#define PRESS_MSG_MASK			0x40
#define RELEASE_MSG_MASK		0x20
#define MOVE_MSG_MASK			0x10
#define SUPPRESS_MSG_MASK		0x02

/* Version */
#define QT602240_VER_20			20
#define QT602240_VER_21			21
#define QT602240_VER_22			22

/* Slave addresses */
#define QT602240_APP_LOW		0x4a
#define QT602240_APP_HIGH		0x4b
#define QT602240_BOOT_LOW		0x24
#define QT602240_BOOT_HIGH		0x25

/*FIRMWARE NAME */
#define MXT224_ECHO_FW_NAME	    "mXT224e.fw"
#define MXT224_FW_NAME		    "qt602240.fw"

#define QT602240_FWRESET_TIME		175	/* msec */
#define QT602240_RESET_TIME		65	/* msec */

#define QT602240_BOOT_VALUE		0xa5
#define QT602240_BACKUP_VALUE		0x55

/* Bootloader mode status */
#define QT602240_WAITING_BOOTLOAD_CMD	0xc0	/* valid 7 6 bit only */
#define QT602240_WAITING_FRAME_DATA	0x80	/* valid 7 6 bit only */
#define QT602240_FRAME_CRC_CHECK	0x02
#define QT602240_FRAME_CRC_FAIL		0x03
#define QT602240_FRAME_CRC_PASS		0x04
#define QT602240_APP_CRC_FAIL		0x40	/* valid 7 8 bit only */
#define QT602240_BOOT_STATUS_MASK	0x3f

/* Command to unlock bootloader */
#define QT602240_UNLOCK_CMD_MSB		0xaa
#define QT602240_UNLOCK_CMD_LSB		0xdc

#define ID_BLOCK_SIZE			7

#define DRIVER_FILTER
#define U1_EUR_TARGET

#define MAX_USING_FINGER_NUM 10

#define MXT224_AUTOCAL_WAIT_TIME		2000


#define T48_CALCFG_TA     0x52

#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
#define	T48_CALCFG                0x42
#else
#if defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
#define	T48_CALCFG                0x72 //20120418
#else
#define	T48_CALCFG                0x40
#endif
#endif

#define CPURATE_FOR_TOUCH_BOOSTER		1512000

/* Cut out ghost ... Xtopher */
#define MAX_GHOSTCHECK_FINGER 		10
#define MAX_GHOSTTOUCH_COUNT		625		// 5s, 125Hz
#define MAX_COUNT_TOUCHSYSREBOOT	4
#define MAX_GHOSTTOUCH_BY_PATTERNTRACKING		5

#define CLEAR_MEDIAN_FILTER_ERROR

#undef CPURATE_DEBUG_FOR_TSP

#ifdef CPURATE_DEBUG_FOR_TSP
	#define DbgOut(_x_) printk _x_
#else   /* VIBE_DEBUG */
	#define DbgOut(_x_)
#endif 



#if defined (U1_EUR_TARGET)
static bool gbfilter;
#endif

#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)|| defined (CONFIG_USA_MODEL_SGH_T989)
#define MXT_REG_VERSION "1021"
#elif defined (CONFIG_USA_MODEL_SGH_I727)
#define MXT_REG_VERSION "I9210_At_0220"
#endif

struct object_t {
	u8 object_type;
	u16 i2c_address;
	u8 size;
	u8 instances;
	u8 num_report_ids;
} __packed;

struct finger_info {
	s16 x;
	s16 y;
	s16 z;
	u16 w;
	int16_t component;
};


#ifdef CLEAR_MEDIAN_FILTER_ERROR
typedef enum
{
	ERR_RTN_CONDITION_T9,
	ERR_RTN_CONDITION_T48,
	ERR_RTN_CONDITION_IDLE,
	ERR_RTN_CONDITION_MAX
}ERR_RTN_CONTIOIN;

ERR_RTN_CONTIOIN gErrCondition = ERR_RTN_CONDITION_IDLE;
#endif



struct mxt224_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct early_suspend early_suspend;
	u8 family_id;
	u32 finger_mask;
	int gpio_read_done;
	struct object_t *objects;
	u8 objects_len;
	u8 tsp_version;
	const u8 *power_cfg;
	const u8 *noise_suppression_cfg_ta;
	const u8 *noise_suppression_cfg;
	u8 finger_type;
	u16 msg_proc;
	u16 cmd_proc;
	u16 msg_object_size;
	u32 x_dropbits:2;
	u32 y_dropbits:2;
	void (*power_on)(void);
	void (*power_off)(void);
	void (*orient_barnch)(int orient_swap);	
	void (*register_cb)(void*);
	void (*read_ta_status)(void*);
	int num_fingers;
	struct finger_info fingers[MXT224_MAX_MT_FINGERS];
	struct timer_list autocal_timer;
};

static struct mutex lock;
static bool lock_needs_init = 1;
struct mxt224_data *copy_data;
extern struct class *sec_class;
int touch_is_pressed;
EXPORT_SYMBOL(touch_is_pressed);
int touch_is_pressed_arr[MAX_USING_FINGER_NUM];
static int mxt224_enabled;
#if defined (CONFIG_KOR_MODEL_SHV_E110S) || defined (CONFIG_USA_MODEL_SGH_I727) ||defined (CONFIG_USA_MODEL_SGH_T989)
#if defined (SEC_TSP_POSITION_DEBUG_XTOPHER)
static bool g_debug_switch = true;
#else
static bool g_debug_switch = false;
#endif
#else
static bool g_debug_switch = false;
#endif
static u8 mxt_version_disp;
static u8 tsp_version_disp;
static int threshold = 55;
static int threshold_e = 50; 
// static int optiacl_gain;
static int firm_status_data;
static bool lock_status;
static int touch_state; /* 1:release, 2:press, 3:others */
static bool boot_or_resume = 1;/*1: boot_or_resume,0: others*/

static int palm_chk_flag;
// static bool auto_cal_flag; /* 1: enabled,0: disabled*/
static bool ta_status_pre = 0;
static unsigned char is_inputmethod = 0;
#ifdef CLEAR_MEDIAN_FILTER_ERROR
static t48_median_config_t noise_median = {0}; //110927 gumi noise
#endif

/* Below is used for clearing ghost touch or for checking to system reboot.  by Xtopher */
static int t48_jump_limit = 0;
static int t48_mrgthr = 0;
static int cghost_clear = 0;  /* ghost touch clear count  by Xtopher */
static int ftouch_reboot = 0; 
static int tcount_finger[MAX_GHOSTCHECK_FINGER] = {0,0,0,0,0,0,0,0,0,0};
static int touchbx[MAX_GHOSTCHECK_FINGER] = {0,0,0,0,0,0,0,0,0,0};
static int touchby[MAX_GHOSTCHECK_FINGER] = {0,0,0,0,0,0,0,0,0,0};
static int ghosttouchcount = 0;
static int tsp_reboot_count = 0;
static int cFailbyPattenTracking = 0;

#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727)|| defined (CONFIG_USA_MODEL_SGH_T989)
static unsigned int gResume_flag = 0;
static unsigned char not_yet_count = 0;

#endif
// static void mxt224_optical_gain(u8 family_id, uint16_t dbg_mode);
static void TSP_forced_release_for_call(void);
static int tsp_pattern_tracking(int fingerindex, s16 x, s16 y);
static void report_input_data(struct mxt224_data *data);
static void TSP_forced_reboot(void);
static void TSP_clear_unused_slots(void);


extern unsigned int  get_hw_rev(void);

static int read_mem(struct mxt224_data *data, u16 reg, u8 len, u8 *buf)
{
	int ret;
	u16 le_reg = cpu_to_le16(reg);
	struct i2c_msg msg[2] = {
		{
			.addr = data->client->addr,
			.flags = 0,
			.len = 2,
			.buf = (u8 *)&le_reg,
		},
		{
			.addr = data->client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	ret = i2c_transfer(data->client->adapter, msg, 2);
	if (ret < 0)
		return ret;

	return ret == 2 ? 0 : -EIO;
}

static int write_mem(struct mxt224_data *data, u16 reg, u8 len, const u8 *buf)
{
	int ret;
	u8 tmp[len + 2];

	put_unaligned_le16(cpu_to_le16(reg), tmp);
	memcpy(tmp + 2, buf, len);

	ret = i2c_master_send(data->client, tmp, sizeof(tmp));

	if (ret < 0)
		return ret;
	/*
	if (reg==(data->cmd_proc + CMD_CALIBRATE_OFFSET))
		printk(KERN_ERR"[TSP] write calibrate_command ret is %d, size is %d\n",ret,sizeof(tmp));
	*/

	return ret == sizeof(tmp) ? 0 : -EIO;
}

static int __devinit mxt224_reset(struct mxt224_data *data)
{
	u8 buf = 1u;
	return write_mem(data, data->cmd_proc + CMD_RESET_OFFSET, 1, &buf);
}

static int __devinit mxt224_backup(struct mxt224_data *data)
{
	u8 buf = 0x55u;
	return write_mem(data, data->cmd_proc + CMD_BACKUP_OFFSET, 1, &buf);
}

static int get_object_info(struct mxt224_data *data, u8 object_type, u16 *size,
				u16 *address)
{
	int i;

	for (i = 0; i < data->objects_len; i++) {
		if (data->objects[i].object_type == object_type) {
			*size = data->objects[i].size + 1;
			*address = data->objects[i].i2c_address;
			return 0;
		}
	}

	return -ENODEV;
}

static int write_config(struct mxt224_data *data, u8 type, const u8 *cfg)
{
	int ret;
	u16 address;
	u16 size;

	ret = get_object_info(data, type, &size, &address);

	if (ret)
		return ret;

	return write_mem(data, address, size, cfg);
}


static u32 __devinit crc24(u32 crc, u8 byte1, u8 byte2)
{
	static const u32 crcpoly = 0x80001B;
	u32 res;
	u16 data_word;

	data_word = (((u16)byte2) << 8) | byte1;
	res = (crc << 1) ^ (u32)data_word;

	if (res & 0x1000000)
		res ^= crcpoly;

	return res;
}

static int __devinit calculate_infoblock_crc(struct mxt224_data *data,
							u32 *crc_pointer)
{
	u32 crc = 0;
	u8 mem[7 + data->objects_len * 6];
	int status;
	int i;

	status = read_mem(data, 0, sizeof(mem), mem);

	if (status)
		return status;

	for (i = 0; i < sizeof(mem) - 1; i += 2)
		crc = crc24(crc, mem[i], mem[i + 1]);

	*crc_pointer = crc24(crc, mem[i], 0) & 0x00FFFFFF;

	return 0;
}

#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
void NoiseEntryStartFunc(void)
{
	u16 obj_address=0;
	u16 size = 1;
	int ret = 0;
	u8 value;


	//object_address = get_object_address(TOUCH_MULTITOUCHSCREEN_T9, 0);

	ret = get_object_info(copy_data, TOUCH_MULTITOUCHSCREEN_T9, &size, &obj_address);

#if 0	
	if (object_address == OBJECT_NOT_FOUND) {
		dprintk("\n[TSP][ERROR] TOUCH_MULTITOUCHSCREEN_T9 object_address : %d\n", __LINE__);
		return -1;
	}
#endif	

	if(gResume_flag)
	{

		// Threshold
		value = 90;	
		//status = write_mem(object_address+7, 1, &data);
		write_mem(copy_data, obj_address+7, 1, &value); 

		// TouchDI
		value = 3;
		//status = write_mem(object_address+8, 1, &data);
		write_mem(copy_data, obj_address+8, 1, &value); 
		printk(KERN_ERR"[TSP] Noise Start Flag In t(80) di(3)\n");



	}
	else
	{
		
		// Threshold
		value = 40; 
		//status = write_mem(object_address+7, 1, &data);
		write_mem(copy_data, obj_address+7, 1, &value); 


		// TouchDI
		value = 2;//
		//status = write_mem(object_address+8, 1, &data);
		write_mem(copy_data, obj_address+8, 1, &value); 

		printk(KERN_ERR"[TSP] Noise Start Flag Out t(default) di(default)\n");
		


	}
}
#endif

static unsigned int qt_time_point;
static unsigned int qt_time_diff;
static unsigned int qt_timer_state;
static unsigned int good_check_flag;
static u8 atchcalst = 9;
static u8 atchcalsthr = 30;

static u8 atchfrccalthr = 40;
static u8 atchfrccalratio = 55;

static u8 cal_check_flag;

 static u8 Doing_calibration_falg =0; 
uint8_t calibrate_chip(void)
{
    u8 cal_data = 1;
    int ret = 0;
    u8 atchcalst_tmp, atchcalsthr_tmp;
	u16 obj_address=0;
	u16 size = 1;
	int ret1 = 0;

/*	printk(KERN_ERR"[TSP]ret is %d,ret1 is %d\n",ret,ret1); */

    if(cal_check_flag == 0) {

		ret = get_object_info(copy_data, GEN_ACQUISITIONCONFIG_T8, &size, &obj_address);


        /* change calibration suspend settings to zero until calibration confirmed good */
        /* store normal settings */
		/* read_mem(copy_data, obj_address+6, (u8)size, &atchcalst); */
		/* read_mem(copy_data, obj_address+7, (u8)size, &atchcalsthr); */

        /* resume calibration must be performed with zero settings */
        atchcalst_tmp = 0;
        atchcalsthr_tmp = 0;
	
#if !defined (CONFIG_USA_MODEL_SGH_I577) && !defined(CONFIG_CAN_MODEL_SGH_I577R) && !defined(CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
		ret = write_mem(copy_data, obj_address+4, 1, &atchcalst_tmp);	/* TCHAUTOCAL */
#endif
		ret = write_mem(copy_data, obj_address+6, 1, &atchcalst_tmp);      /* atchcalst */
		ret1 = write_mem(copy_data, obj_address+7, 1, &atchcalsthr_tmp);  /*atchcalsthr */

		if(copy_data->family_id == 0x81) { /* mxT224E */
			ret = write_mem(copy_data, obj_address+8, 1, &atchcalst_tmp);	  /* forced cal thr  */
			ret1 = write_mem(copy_data, obj_address+9, 1, &atchcalsthr_tmp); /* forced cal thr ratio */
		}
	

		/* Write temporary acquisition config to chip. */
		/*
        if (write_acquisition_config(acquisition_config) != CFG_WRITE_OK) {
			"Acquisition config write failed!\n"
			printk("\n[TSP][ERROR] line : %d\n", __LINE__);
			ret = WRITE_MEM_FAILED; calling function should retry calibration call
        }*/

        /* restore settings to the local structure so that when we confirm the
		 * cal is good we can correct them in the chip
		 * this must be done before returning */
		/*
		printk(KERN_ERR"[TSP] acquition restore! atchcalst=%d, atchcalsthr=%d\n", atchcalst, atchcalsthr );
		write_mem(copy_data, obj_address+6, size, &atchcalst);
		write_mem(copy_data, obj_address+7, size, &atchcalsthr);
		*/
    }

    /* send calibration command to the chip */
    if(!ret && !ret1 && !Doing_calibration_falg) {
        /* change calibration suspend settings to zero until calibration confirmed good */
		ret = write_mem(copy_data, copy_data->cmd_proc + CMD_CALIBRATE_OFFSET, 1, &cal_data);
		/* mdelay(5); */

		/*read_mem(copy_data, copy_data->cmd_proc+2, (u8)size, &value);
		printk(KERN_ERR"[TSP] calibration data is %d\n",value);*/

	    /* set flag for calibration lockup recovery if cal command was successful */
		if (!ret) {
            /* set flag to show we must still confirm if calibration was good or bad */
            cal_check_flag = 1u;
			 Doing_calibration_falg = 1; 
			printk(KERN_ERR"[TSP]calibration success!!!\n");
        }

    }
#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
    else
    {
		printk(KERN_ERR"[TSP]calibration ignore!!!\n");
    }
#endif
    return ret;
}

static int check_abs_time(void)
{
    if (!qt_time_point)
        return 0;

    qt_time_diff = jiffies_to_msecs(jiffies) - qt_time_point;

    if(qt_time_diff > 0)
        return 1;
    else
        return 0;

}

static void mxt224_ta_probe(int ta_status)
{
	u16 obj_address = 0;
	u16 size_one;
	int ret;
	u8 value;
	u8 val = 0;
	unsigned int register_address = 7;
	u8 noise_threshold;
	u8 movfilter;	
	u8 blen;	
	u8 calcfg_dis;
	u8 calcfg_en;
	// u16 i;
	u8 size;	
	u8 active_depth;
	
#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
        u8 charge_time;
#endif

	printk(KERN_ERR"[TSP] mxt224_ta_probe \n");
	if (!mxt224_enabled) {
		printk(KERN_ERR"[TSP] mxt224_enabled is 0\n");
		return;
	}

	if (lock_needs_init) {
		mutex_init(&lock);
		lock_needs_init = 0;
	}

	if (ta_status) {
		threshold = 70;
		threshold_e = 40;		
		calcfg_dis = T48_CALCFG_TA;
		calcfg_en = T48_CALCFG_TA | 0x20;
		
		#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
		noise_threshold = 32;
		active_depth = 35;
		#else		
		noise_threshold = 40;
		active_depth = 38;	
		#endif

		movfilter = 46;
		blen = 16;

		#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
                charge_time = 22;
		#endif		

		#ifdef CLEAR_MEDIAN_FILTER_ERROR
		gErrCondition =  ERR_RTN_CONDITION_MAX;
		#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
                noise_median.mferr_setting = false;
		#endif
		#endif


	} else {
		#if !defined (CONFIG_USA_MODEL_SGH_I577) && !defined(CONFIG_CAN_MODEL_SGH_I577R) && !defined (CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
		if (boot_or_resume==1)
			threshold = 55;
		else
		    threshold = 40;
		threshold_e = 50;
		#endif

		calcfg_dis = T48_CALCFG;
		calcfg_en = T48_CALCFG | 0x20;
		noise_threshold = 30;
		movfilter = 11;		
		blen = 32;
		#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined(CONFIG_USA_MODEL_SGH_I727)
		active_depth = 26;//20120418
		charge_time = 22;
		#elif defined (CONFIG_USA_MODEL_SGH_T989)
		active_depth = 24;
		#else
		active_depth = 56;
		#endif
		#ifdef CLEAR_MEDIAN_FILTER_ERROR
		gErrCondition =  ERR_RTN_CONDITION_IDLE;
		noise_median.mferr_count=0;
		noise_median.mferr_setting = false;
		#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
		noise_median.median_on_flag = false;
		#endif
		#endif

	}
	
	if (copy_data->family_id==0x81) {

		#if !defined (CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
		#ifdef CLEAR_MEDIAN_FILTER_ERROR
		if(!ta_status)
		{
			ret = get_object_info(copy_data, TOUCH_MULTITOUCHSCREEN_T9, &size_one, &obj_address);
			size_one = 1;
			//blen
			value = 32;
			register_address=6;
			write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
			//threshold
			value = 50;
			register_address=7;
			write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
			// move Filter
			#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
			value = 81;
			#else
			value = 46;
			#endif
			register_address=13;
			write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
		}
		#endif
		#endif

		#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
		value = 26;
		#else
		value = active_depth;
		#endif
		ret = get_object_info(copy_data, SPT_CTECONFIG_T46, &size_one, &obj_address);
		write_mem(copy_data, obj_address+3, 1, &value);
		
		ret = get_object_info(copy_data, GEN_ACQUISITIONCONFIG_T8, &size_one, &obj_address);
		size_one = 1;

		#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
	        value = charge_time;
		write_mem(copy_data, obj_address, size_one, &value);
		#else
		if (ta_status)
		{
			value = 22;
			write_mem(copy_data, obj_address, size_one, &value);
		}
		else
		{
			value = 27;
			write_mem(copy_data, obj_address, size_one, &value);
		}
		#endif
	
		value = calcfg_dis;
		register_address=2;
		ret = get_object_info(copy_data, PROCG_NOISESUPPRESSION_T48, &size_one, &obj_address);
	    size = size_one;
		size_one = 1;
		write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
//		read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
//		printk(KERN_ERR"[TSP]TA_probe MXT224E T%d Byte%d is %d\n",48,register_address,val);
		
		if (ta_status)
		write_config(copy_data, PROCG_NOISESUPPRESSION_T48, copy_data->noise_suppression_cfg_ta);
		else
		write_config(copy_data, PROCG_NOISESUPPRESSION_T48, copy_data->noise_suppression_cfg);

		value = calcfg_en;
		write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
		read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
		printk(KERN_ERR"[TSP]TA_probe MXT224E T%d Byte%d is %d\n",48,register_address,val);

		if(is_inputmethod == 1)   /* T48 Config change for TA connection */
		{
			value = t48_jump_limit;
			register_address=51;
			ret = get_object_info(copy_data, PROCG_NOISESUPPRESSION_T48, &size_one, &obj_address);
		size_one = 1;
		write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
		read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
			printk(KERN_ERR"[TSP]TA_probe MXT224E T%d Byte%d is %d\n",48,register_address,val);
			
			value = t48_mrgthr;
			register_address=42;
			write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
			read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
			printk(KERN_ERR"[TSP]TA_probe MXT224E T%d Byte%d is %d\n",48,register_address,val);
		}
		
		/*for (i = 0; i < size; i++) {
		read_mem(copy_data, obj_address+i, 1, &val);
		printk(KERN_ERR "[TSP] Byte %u --> %u\n", i, val);
		}*/
	} else {
		ret = get_object_info(copy_data, TOUCH_MULTITOUCHSCREEN_T9, &size_one, &obj_address);
		size_one = 1;
		value = (u8)threshold;
		write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
		read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
		printk(KERN_ERR"[TSP]TA_probe MXT224 T%d Byte%d is %d\n", 9, register_address, val);

		value = (u8)movfilter;
		register_address=13;		
		write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
		read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
		printk(KERN_ERR"[TSP]TA_probe MXT224 T%d Byte%d is %d\n", 9, register_address, val);

		value = noise_threshold;
		register_address = 8;
		ret = get_object_info(copy_data, PROCG_NOISESUPPRESSION_T22, &size_one, &obj_address);
		size_one = 1;
		write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
		read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
		printk(KERN_ERR"[TSP]TA_probe MXT224 T%d Byte%d is %d\n", 22, register_address, val);		

		if(is_inputmethod == 1)   /* T48 Config change for TA connection */
		{
		ret = get_object_info(copy_data, TOUCH_MULTITOUCHSCREEN_T9, &size_one, &obj_address);
			register_address = 16;		
			value = (u8)t48_mrgthr;
		size_one = 1;
		write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
		read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
			printk(KERN_ERR "T%d Byte%d is %d\n", 9, register_address, val);
			
			register_address = 30;				
			value = (u8)t48_jump_limit;
			size_one = 1;
			write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
			read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
			printk(KERN_ERR "T%d Byte%d is %d\n", 9, register_address, val);

			value = t48_jump_limit;
			register_address=51;
			ret = get_object_info(copy_data, PROCG_NOISESUPPRESSION_T48, &size_one, &obj_address);
			size_one = 1;
			write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
			read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
			printk(KERN_ERR"[TSP] MXT224E T%d Byte%d is %d\n",48,register_address,val);
			
			value = t48_mrgthr;
			register_address=42;
			write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
			read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
			printk(KERN_ERR"[TSP] MXT224E T%d Byte%d is %d\n",48,register_address,val);
	}

	}
	ta_status_pre = (bool)ta_status;
};

void check_chip_calibration(unsigned char one_touch_input_flag)
{
	u8 data_buffer[100] = { 0 };
	u8 try_ctr = 0;
	u8 data_byte = 0xF3; /* dianostic command to get touch flags */
	u8 tch_ch = 0, atch_ch = 0;
	/* u8 atchcalst, atchcalsthr; */
	u8 check_mask;
	u8 i,j =0;
	u8 x_line_limit;
	int ret;
	u16 size;
	u16 object_address=0;
	
	#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
	bool ta_status_check;
	#endif


        /* we have had the first touchscreen or face suppression message
         * after a calibration - check the sensor state and try to confirm if
         * cal was good or bad */

        /* get touch flags from the chip using the diagnostic object */
        /* write command to command processor to get touch flags - 0xF3 Command required to do this */
	/* write_mem(command_processor_address + DIAGNOSTIC_OFFSET, 1, &data_byte); */
		write_mem(copy_data, copy_data->cmd_proc + CMD_DIAGNOSTIC_OFFSET, 1, &data_byte);


        /* get the address of the diagnostic object so we can get the data we need */
	/* diag_address = get_object_address(DEBUG_DIAGNOSTIC_T37,0); */
		ret = get_object_info(copy_data, DEBUG_DIAGNOSTIC_T37, &size, &object_address);

        mdelay(10);

        /* read touch flags from the diagnostic object - clear buffer so the while loop can run first time */
        memset(data_buffer , 0xFF, sizeof( data_buffer));

        /* wait for diagnostic object to update */
        while(!((data_buffer[0] == 0xF3) && (data_buffer[1] == 0x00))) {
            /* wait for data to be valid  */
		if (try_ctr > 10) { /* 0318 hugh 100-> 10 */

                /* Failed! */
                printk(KERN_ERR"[TSP] Diagnostic Data did not update!!\n");
			qt_timer_state = 0;/* 0430 hugh */
                break;
            }

		mdelay(2); /* 0318 hugh  3-> 2 */
            try_ctr++; /* timeout counter */
		/* read_mem(diag_address, 2,data_buffer); */

		read_mem(copy_data, object_address, 2, data_buffer);
        }


        /* data is ready - read the detection flags */
	/* read_mem(diag_address, 82,data_buffer); */
		read_mem(copy_data, object_address, 82, data_buffer);


        /* data array is 20 x 16 bits for each set of flags, 2 byte header, 40 bytes for touch flags 40 bytes for antitouch flags*/

        /* count up the channels/bits if we recived the data properly */
        if((data_buffer[0] == 0xF3) && (data_buffer[1] == 0x00)) {

            /* mode 0 : 16 x line, mode 1 : 17 etc etc upto mode 4.*/
		/* x_line_limit = 16 + cte_config.mode; */
            x_line_limit = 16 + 3;

            if(x_line_limit > 20) {
                /* hard limit at 20 so we don't over-index the array */
                x_line_limit = 20;
            }

            /* double the limit as the array is in bytes not words */
            x_line_limit = x_line_limit << 1;

            /* count the channels and print the flags to the log */
            for(i = 0; i < x_line_limit; i+=2) { /* check X lines - data is in words so increment 2 at a time */

                /* print the flags to the log - only really needed for debugging */

                /* count how many bits set for this row */
                for(j = 0; j < 8; j++) {
                    /* create a bit mask to check against */
                    check_mask = 1 << j;

                    /* check detect flags */
				if (data_buffer[2+i] & check_mask)
                        tch_ch++;

				if (data_buffer[3+i] & check_mask)
                        tch_ch++;

                    /* check anti-detect flags */
				if (data_buffer[42+i] & check_mask)
                        atch_ch++;

				if (data_buffer[43+i] & check_mask)
                        atch_ch++;

                    }
                }

			printk(KERN_ERR"[TSP] t: %d, a: %d\n",tch_ch,atch_ch);

            /* send page up command so we can detect when data updates next time,
             * page byte will sit at 1 until we next send F3 command */
            data_byte = 0x01;

		/* write_mem(command_processor_address + DIAGNOSTIC_OFFSET, 1, &data_byte); */
			write_mem(copy_data, copy_data->cmd_proc + CMD_DIAGNOSTIC_OFFSET, 1, &data_byte);


            /* process counters and decide if we must re-calibrate or if cal was good */
		#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
		    if (tch_ch+atch_ch >= 25) {
				/* cal was bad - must recalibrate
				and check afterwards */
				printk(KERN_ERR"[TSP] tch_ch+atch_ch  calibration was bad\n");
				calibrate_chip();
				qt_timer_state=0;
	            qt_time_point = jiffies_to_msecs(jiffies);
		    }
	        else 
		#endif
		if ((tch_ch > 0) && (atch_ch == 0)) {  /* jwlee change. */
                /* cal was good - don't need to check any more */
		
				#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
				not_yet_count = 0;
				#endif
			#if 0  // Removed  by Xtopher
			/* hugh 0312 */
			if(auto_cal_flag == 0) {
				u8 val;
				data_byte = 5;
				ret = get_object_info(copy_data, GEN_ACQUISITIONCONFIG_T8, &size, &object_address);
				write_mem(copy_data, object_address+4, 1, &data_byte);	/* TCHAUTOCAL 1sec */
				read_mem(copy_data, object_address+4, 1, &val);
				printk(KERN_DEBUG"[TSP] auto calibration enable state is %d!!!\n",val);
				auto_cal_flag = 1;
			}
			#endif			
                if(!check_abs_time())
				qt_time_diff = 301; /* original:qt_time_diff = 501 */

                if(qt_timer_state == 1) {
				if (qt_time_diff > 300) { /* originalqt_time_diff = 500 */
			#ifndef CONFIG_USA_MODEL_SGH_T989	
                        printk(KERN_ERR"[TSP] calibration was good\n");
                        #endif
                        cal_check_flag = 0;
                        good_check_flag = 0;
                        qt_timer_state =0;
                        qt_time_point = jiffies_to_msecs(jiffies);

						ret = get_object_info(copy_data, GEN_ACQUISITIONCONFIG_T8, &size, &object_address);

						/* change calibration suspend settings to zero until calibration confirmed good */
						/* store normal settings */
						size = 1;
#if 0
					data_byte = 5;
					write_mem(copy_data, object_address+4, 1, &data_byte);	/* TCHAUTOCAL 1sec */
					palm_chk_flag = 1; 
					/* enable 4sec timer */
					/* tsp_set_timer(4000); */
					ret = mod_timer(&copy_data->autocal_timer, jiffies + msecs_to_jiffies(MXT224_AUTOCAL_WAIT_TIME) );

					if (ret)
						printk(KERN_ERR"Error in mod_timer\n");
#else
                    palm_chk_flag = 2; 
					// removed by Xtopher,  auto_cal_flag = 0;
#endif

					if(copy_data->family_id == 0x80) {
						write_mem(copy_data, object_address+6, 1, &atchcalst);
						write_mem(copy_data, object_address+7, 1, &atchcalsthr);
						
					} else if(copy_data->family_id == 0x81) {
						write_mem(copy_data, object_address+6, 1, &atchcalst);
						write_mem(copy_data, object_address+7, 1, &atchcalsthr);
						write_mem(copy_data, object_address+8, 1, &atchfrccalthr);
						write_mem(copy_data, object_address+9, 1, &atchfrccalratio);

					}
				#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
					if (copy_data->read_ta_status) {
						copy_data->read_ta_status(&ta_status_check);
						if(!ta_status_check)
						{
							get_object_info(copy_data, PROCG_NOISESUPPRESSION_T48, &size, &object_address);
							data_byte = 5;
							write_mem(copy_data, object_address+27, 1, &data_byte);
							}
						}
				#endif

					// Clear all unused slots after calibration
					TSP_clear_unused_slots();

					#if 0  // Xtopher blocked, it causes trouble when system wake-up             
					if ((copy_data->read_ta_status)&&(boot_or_resume == 1)) {
                    boot_or_resume = 0;
						copy_data->read_ta_status(&ta_status_check);
						printk(KERN_ERR "[TSP] ta_status in (check_chip_calibration) is %d\n", ta_status_check);
	
						if (ta_status_check==0){
							printk(KERN_ERR "[TSP] mxt224_ta_probe is called in (check_chip_calibration)\n");
			            	mxt224_ta_probe(ta_status_check);
		            }
					}
					#endif
					/* dprintk("[TSP] reset acq atchcalst=%d, atchcalsthr=%d\n", acquisition_config.atchcalst, acquisition_config.atchcalsthr ); */

					/*
					Write normal acquisition config back to the chip.
                        if (write_acquisition_config(acquisition_config) != CFG_WRITE_OK) {
						"Acquisition config write failed!\n"
                            printk(KERN_DEBUG "\n[TSP][ERROR] line : %d\n", __LINE__);
						MUST be fixed
                    }
					*/

				} else  {
                        cal_check_flag = 1;
                    }
			} else {
                    qt_timer_state=1;
                    qt_time_point = jiffies_to_msecs(jiffies);
                    cal_check_flag=1;
                }

		} else if (atch_ch >= 5) {     /* jwlee add 0325 */
                /* cal was bad - must recalibrate and check afterwards */
				printk(KERN_ERR"[TSP] calibration was bad\n");
                calibrate_chip();
                qt_timer_state=0;
                qt_time_point = jiffies_to_msecs(jiffies);
		
		#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
		not_yet_count = 0;
		#endif

		} else {
			#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
				if(atch_ch >= 1)					
				{	not_yet_count++;
					printk(KERN_ERR"[TSP] not_yet_count : %d\n",not_yet_count);
				}
				if (not_yet_count > 25) {											// KYLE
					  printk(KERN_ERR"[TSP] not_yet_count calibration was bad\n");
					  //not_yet_count
					  calibrate_chip();
					  qt_timer_state = 0;/* 0430 hugh 1 --> 0 */
					  qt_time_point = jiffies_to_msecs(jiffies);
					  not_yet_count = 0;
				}
				else
			#endif
				{
		/* we cannot confirm if good or bad - we must wait for next touch  message to confirm */
		printk(KERN_ERR"[TSP] calibration was not decided yet\n");
                cal_check_flag = 1u;
                /* Reset the 100ms timer */
		qt_timer_state = 0;/* 0430 hugh 1 --> 0 */
                qt_time_point = jiffies_to_msecs(jiffies);
            }

        }

	}

}



#if defined(DRIVER_FILTER)
#if defined (U1_EUR_TARGET)
static void equalize_coordinate(bool detect, u8 id, u16 *px, u16 *py)
{
    static int tcount[MAX_USING_FINGER_NUM] = { 0, };
    static u16 pre_x[MAX_USING_FINGER_NUM][4] = {{0}, };
    static u16 pre_y[MAX_USING_FINGER_NUM][4] = {{0}, };
    int coff[4] = {0,};
    int distance = 0;

    if(detect)
        tcount[id] = 0;

    pre_x[id][tcount[id]%4] = *px;
    pre_y[id][tcount[id]%4] = *py;

	if (gbfilter) {
		if (tcount[id] > 3) {
            *px = (u16)((*px + pre_x[id][(tcount[id]-1)%4] + pre_x[id][(tcount[id]-2)%4] + pre_x[id][(tcount[id]-3)%4] )/4);
            *py = (u16)((*py+ pre_y[id][(tcount[id]-1)%4] + pre_y[id][(tcount[id]-2)%4]+ pre_y[id][(tcount[id]-3)%4])/4);
		} else {
			switch (tcount[id]) {
            case 2:
            {
                *px = (u16)((*px + pre_x[id][(tcount[id]-1)%4])>>1);
                *py = (u16)((*py + pre_y[id][(tcount[id]-1)%4])>>1);
                break;
            }

            case 3:
            {
                *px = (u16)((*px + pre_x[id][(tcount[id]-1)%4] + pre_x[id][(tcount[id]-2)%4])/3);
                *py = (u16)((*py + pre_y[id][(tcount[id]-1)%4] + pre_y[id][(tcount[id]-2)%4])/3);
                break;
            }

            default:
                break;
        }
    }

	} else if (tcount[id] > 3) {
        {
            distance = abs(pre_x[id][(tcount[id]-1)%4] - *px) + abs(pre_y[id][(tcount[id]-1)%4] - *py);

            coff[0] = (u8)(2 + distance/5);
			if (coff[0] < 8) {
                coff[0] = max(2, coff[0]);
                coff[1] = min((8 - coff[0]), (coff[0]>>1)+1);
                coff[2] = min((8 - coff[0] - coff[1]), (coff[1]>>1)+1);
                coff[3] = 8 - coff[0] - coff[1] - coff[2];

				/* printk(KERN_DEBUG "[TSP] %d, %d, %d, %d", coff[0], coff[1], coff[2], coff[3]); */

                *px = (u16)((*px*(coff[0]) + pre_x[id][(tcount[id]-1)%4]*(coff[1])
                    + pre_x[id][(tcount[id]-2)%4]*(coff[2]) + pre_x[id][(tcount[id]-3)%4]*(coff[3]))/8);
                *py = (u16)((*py*(coff[0]) + pre_y[id][(tcount[id]-1)%4]*(coff[1])
                    + pre_y[id][(tcount[id]-2)%4]*(coff[2]) + pre_y[id][(tcount[id]-3)%4]*(coff[3]))/8);
			} else {
                *px = (u16)((*px*4 + pre_x[id][(tcount[id]-1)%4])/5);
                *py = (u16)((*py*4 + pre_y[id][(tcount[id]-1)%4])/5);
            }
        }
     }
    tcount[id]++;
}

#else   /* CONFIG_TARGET_LOCALE_KOR */
static void equalize_coordinate(bool detect, u8 id, u16 *px, u16 *py)
{
    static int tcount[MAX_USING_FINGER_NUM] = { 0, };
    static u16 pre_x[MAX_USING_FINGER_NUM][4] = {{0}, };
    static u16 pre_y[MAX_USING_FINGER_NUM][4] = {{0}, };
    int coff[4] = {0,};
    int distance = 0;

    if(detect)
        tcount[id] = 0;

    pre_x[id][tcount[id]%4] = *px;
    pre_y[id][tcount[id]%4] = *py;

	if (tcount[id] > 3) {
        distance = abs(pre_x[id][(tcount[id]-1)%4] - *px) + abs(pre_y[id][(tcount[id]-1)%4] - *py);

        coff[0] = (u8)(4 + distance/5);
		if (coff[0] < 8) {
            coff[0] = max(4, coff[0]);
            coff[1] = min((10 - coff[0]), (coff[0]>>1)+1);
            coff[2] = min((10 - coff[0] - coff[1]), (coff[1]>>1)+1);
            coff[3] = 10 - coff[0] - coff[1] - coff[2];

			/* printk(KERN_DEBUG "[TSP] %d, %d, %d, %d", coff[0], coff[1], coff[2], coff[3]); */

            *px = (u16)((*px*(coff[0]) + pre_x[id][(tcount[id]-1)%4]*(coff[1])
                + pre_x[id][(tcount[id]-2)%4]*(coff[2]) + pre_x[id][(tcount[id]-3)%4]*(coff[3]))/10);
            *py = (u16)((*py*(coff[0]) + pre_y[id][(tcount[id]-1)%4]*(coff[1])
                + pre_y[id][(tcount[id]-2)%4]*(coff[2]) + pre_y[id][(tcount[id]-3)%4]*(coff[3]))/10);
		} else {
            *px = (u16)((*px*4 + pre_x[id][(tcount[id]-1)%4])/5);
            *py = (u16)((*py*4 + pre_y[id][(tcount[id]-1)%4])/5);
        }
    }
#if 0
	else {
		switch (tcount[id]) {
        case 2:
        {
            *px = (u16)((*px + pre_x[id][(tcount[id]-1)%4])>>1);
            *py = (u16)((*py + pre_y[id][(tcount[id]-1)%4])>>1);
            break;
        }

        case 3:
        {
            *px = (u16)((*px + pre_x[id][(tcount[id]-1)%4] + pre_x[id][(tcount[id]-2)%4])/3);
            *py = (u16)((*py + pre_y[id][(tcount[id]-1)%4] + pre_y[id][(tcount[id]-2)%4])/3);
            break;
        }

        default:
            break;
    }
	}
#endif

    tcount[id]++;
}
#endif
#endif  /* DRIVER_FILTER */

static int __devinit mxt224_init_touch_driver(struct mxt224_data *data)
{
	struct object_t *object_table;
	u32 read_crc = 0;
	u32 calc_crc = 0;
	u16 crc_address;
	u16 dummy;
	int i;
	u8 id[ID_BLOCK_SIZE];
	int ret;
	u8 type_count = 0;
	u8 tmp;

	ret = read_mem(data, 0, sizeof(id), id);
	if (ret)
		return ret;

	dev_info(&data->client->dev, "family = %#02x, variant = %#02x, version "
			"= %#02x, build = %d\n", id[0], id[1], id[2], id[3]);
	printk(KERN_ERR"[TSP] family = %#02x, variant = %#02x, version "
			"= %#02x, build = %d\n", id[0], id[1], id[2], id[3]);
	dev_dbg(&data->client->dev, "matrix X size = %d\n", id[4]);
	dev_dbg(&data->client->dev, "matrix Y size = %d\n", id[5]);

	data->family_id = id[0];
	data->tsp_version = id[2];
	data->objects_len = id[6];

	mxt_version_disp = data->family_id;
	tsp_version_disp = data->tsp_version;

	object_table = kmalloc(data->objects_len * sizeof(*object_table),
				GFP_KERNEL);
	if (!object_table)
		return -ENOMEM;

	ret = read_mem(data, OBJECT_TABLE_START_ADDRESS,
			data->objects_len * sizeof(*object_table),
			(u8 *)object_table);
	if (ret)
		goto err;

	for (i = 0; i < data->objects_len; i++) {
		object_table[i].i2c_address = le16_to_cpu(object_table[i].i2c_address);
		tmp = 0;
		if (object_table[i].num_report_ids) {
			tmp = type_count + 1;
			type_count += object_table[i].num_report_ids *
						(object_table[i].instances + 1);
		}
		switch (object_table[i].object_type) {
		case TOUCH_MULTITOUCHSCREEN_T9:
			data->finger_type = tmp;
			dev_dbg(&data->client->dev, "Finger type = %d\n",
						data->finger_type);
			break;
		case GEN_MESSAGEPROCESSOR_T5:
			data->msg_object_size = object_table[i].size + 1;
			dev_dbg(&data->client->dev, "Message object size = "
						"%d\n", data->msg_object_size);
			break;
		}
	}

	data->objects = object_table;

	/* Verify CRC */
	crc_address = OBJECT_TABLE_START_ADDRESS +
			data->objects_len * OBJECT_TABLE_ELEMENT_SIZE;

#ifdef __BIG_ENDIAN
#error The following code will likely break on a big endian machine
#endif
	ret = read_mem(data, crc_address, 3, (u8 *)&read_crc);
	if (ret)
		goto err;

	read_crc = le32_to_cpu(read_crc);

	ret = calculate_infoblock_crc(data, &calc_crc);
	if (ret)
		goto err;

	if (read_crc != calc_crc) {
		dev_err(&data->client->dev, "CRC error\n");
		ret = -EFAULT;
		goto err;
	}

	ret = get_object_info(data, GEN_MESSAGEPROCESSOR_T5, &dummy,
					&data->msg_proc);
	if (ret)
		goto err;

	ret = get_object_info(data, GEN_COMMANDPROCESSOR_T6, &dummy,
					&data->cmd_proc);
	if (ret)
		goto err;

	return 0;

err:
	kfree(object_table);
	return ret;
}



static void clear_tcount(void)
{
	int i;
	for(i=0;i<MAX_GHOSTCHECK_FINGER;i++){
		tcount_finger[i] = 0;
		touchbx[i] = 0;
		touchby[i] = 0;
	}		
}

static int diff_two_point(s16 x, s16 y, s16 oldx, s16 oldy)
{
	s16 diffx,diffy;
	s16 distance;
	
	diffx = x-oldx;
	diffy = y-oldy;
	distance = abs(diffx) + abs(diffy);

	if(distance < 3) return 1;
	else return 0;
}

/* Forced reboot sequence.  
    Don't use arbitraily. 
    if you meet special case that this routine has to be used, ask Xtopher's advice.
*/
static void TSP_forced_reboot(void)
{
	int i;
	bool ta_status=0;

	if(ftouch_reboot == 1) return;
	ftouch_reboot  = 1;
	printk(KERN_DEBUG "[TSP] Reboot-Touch by Pattern Tracking S\n");
	cghost_clear = 0;

	/* Below is reboot sequence   */
	//disable_irq(copy_data->client->irq);		
	copy_data->power_off();

	for (i = 0; i < copy_data->num_fingers; i++) {
		if (copy_data->fingers[i].z == -1)
			continue;
	
		touch_is_pressed_arr[i] = 0;
		copy_data->fingers[i].z = 0;
	}
	report_input_data(copy_data);
	msleep(100);
	copy_data->power_on();
	msleep(10);

	//enable_irq(copy_data->client->irq);
	boot_or_resume = 1;
	mxt224_enabled = 1;
	qt_timer_state = 0; 
	
	if(copy_data->read_ta_status) {
		copy_data->read_ta_status(&ta_status);
		printk(KERN_ERR "[TSP] ta_status in (mxt224_late_resume) is %d", ta_status);
		mxt224_ta_probe(ta_status);
	}
	calibrate_chip();
	ftouch_reboot  = 0;
	printk(KERN_DEBUG "[TSP] Reboot-Touch by Pattern Tracking E\n");

}


/* To do forced calibration when ghost touch occured at the same point
    for several second.   Xtopher */
static int tsp_pattern_tracking(int fingerindex, s16 x, s16 y)
{
	int i;
	int ghosttouch  = 0;

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
				if(cFailbyPattenTracking > MAX_GHOSTTOUCH_BY_PATTERNTRACKING)
				{
					cFailbyPattenTracking = 0;
					TSP_forced_reboot();
				}
				else
				{
				TSP_forced_release_for_call();
			}
		}
	}
	}
	return ghosttouch;
}


static void report_input_data(struct mxt224_data *data)
{
	int i;
	int movecount=0, presscount=0;
	bool ta_status=0;

	touch_is_pressed = 0;
	
	for (i = 0; i < data->num_fingers; i++) {

		if (data->fingers[i].z == -1)
			continue;
#if defined(TOUCH_NON_SLOT)
		input_report_abs(data->input_dev, ABS_MT_POSITION_X, data->fingers[i].x);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y, data->fingers[i].y);
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, data->fingers[i].z);
		input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, data->fingers[i].w);
		input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, i);

		#ifdef _SUPPORT_SHAPE_TOUCH_
		input_report_abs(data->input_dev, ABS_MT_COMPONENT, data->fingers[i].component);
		/* printk(KERN_ERR"the component data is %d\n",data->fingers[i].component); */
		#endif

		input_mt_sync(data->input_dev);
#else   // SLOT
		input_mt_slot(data->input_dev, i);
		input_mt_report_slot_state(data->input_dev,
					MT_TOOL_FINGER, !!data->fingers[i].z);


		if(data->fingers[i].z != 0){
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, data->fingers[i].x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, data->fingers[i].y);
			
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, data->fingers[i].w);
			
			//input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, data->fingers[i].w);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, data->fingers[i].z);
		}
		//input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, i);

		#ifdef _SUPPORT_SHAPE_TOUCH_
		input_report_abs(data->input_dev, ABS_MT_COMPONENT, data->fingers[i].component);
		/* printk(KERN_ERR"the component data is %d\n",data->fingers[i].component); */
		#endif

		//input_mt_sync(data->input_dev);
#endif

//		if (touch_is_pressed_arr[i] < 2) {
		if (g_debug_switch)
				printk(KERN_DEBUG "[TSP] ID-%d, %4d,%4d  UD:%d \n", i, data->fingers[i].x, data->fingers[i].y, touch_is_pressed_arr[i]);
			else 
			      {
			        #ifndef CONFIG_USA_MODEL_SGH_T989
				printk(KERN_DEBUG "[TSP] ID-%d,UD:%d \n", i, 
					touch_is_pressed_arr[i]);  
				#endif
			      }		
//		}

		tsp_pattern_tracking(i, data->fingers[i].x, data->fingers[i].y);

		if(touch_is_pressed_arr[i] ==  1)
			presscount++;
		else if(touch_is_pressed_arr[i] == 2)
			movecount++;
		
			
		if (touch_is_pressed_arr[i]!=0)
			touch_is_pressed = 1;

		/* logging */
		/*
		if (touch_is_pressed_arr[i]==0)
			printk(KERN_ERR "[TSP] Up[%d] %4d,%4d\n", i, data->fingers[i].x, data->fingers[i].y);
		else if (touch_is_pressed_arr[i]==1)
			printk(KERN_ERR "[TSP] Dn[%d] %4d,%4d\n", i, data->fingers[i].x, data->fingers[i].y);
		*/

		if (data->fingers[i].z == 0)
			data->fingers[i].z = -1;
	}
	
	/* 
	Forced-calibration or Touch kernel reboot at power on or system wake-up.
	This routine must be escaped from recursive calling by external ghost touch that should be
	occured continuously.
	Press event during move event by other finger is considered as ghost at resume.     Xtopher
	*/
	if((cal_check_flag ==1)&& ( ftouch_reboot  == 0)&&(presscount >= 1)&&(movecount >= 1))
	{
		cghost_clear++;
		if( cghost_clear > MAX_COUNT_TOUCHSYSREBOOT ) 
	{
			ftouch_reboot  = 1;
			printk(KERN_DEBUG "[TSP] Reboot-Touch S\n");
			cghost_clear = 0;

			/* Below is reboot sequence   */
			//disable_irq(copy_data->client->irq);		
			copy_data->power_off();

			for (i = 0; i < copy_data->num_fingers; i++) {
				if (copy_data->fingers[i].z == -1)
					continue;
			
				touch_is_pressed_arr[i] = 0;
				copy_data->fingers[i].z = 0;
			}
			report_input_data(copy_data);
			msleep(100);
			copy_data->power_on();
			msleep(10);
	
			//enable_irq(copy_data->client->irq);
			boot_or_resume = 1;
			mxt224_enabled = 1;
			is_inputmethod = 0;   // I know it's fault, but app couldn't solve the issue wighout this known fault.
			qt_timer_state = 0; 
			
			if(copy_data->read_ta_status) {
				copy_data->read_ta_status(&ta_status);
				printk(KERN_ERR "[TSP] ta_status in (mxt224_late_resume) is %d", ta_status);
				mxt224_ta_probe(ta_status);
			}
			calibrate_chip();
			ftouch_reboot  = 0;
			printk(KERN_DEBUG "[TSP] Reboot-Touch E \n");
		}
		else
		{
			printk(KERN_DEBUG "[TSP] Clear G-Touch at Boot/Wake-up \n");
		TSP_forced_release_for_call();
	}
		
	}
	data->finger_mask = 0;
    touch_state=0;
	input_sync(data->input_dev);
}

void palm_recovery(void)
{
	int ret = 0;
	u8 atchcalst_tmp = 0, atchcalsthr_tmp = 0;
	u16 obj_address = 0;
	u16 size = 1;
	int ret1 = 0;



	if(palm_chk_flag == 2) { 
		palm_chk_flag = 0;
		ret = get_object_info(copy_data, GEN_ACQUISITIONCONFIG_T8, &size, &obj_address);
		size = 1;

		/* TCHAUTOCAL Disable */
		ret = write_mem(copy_data, obj_address+4, 1, &atchcalst_tmp);	/* TCHAUTOCAL */
        printk(KERN_DEBUG"[TSP] auto calibration disable!!!\n");
		
	} else {
		/*	printk(KERN_ERR"[TSP]ret is %d,ret1 is %d\n",ret,ret1); */
	
		if (cal_check_flag == 0) {
	
			ret = get_object_info(copy_data, GEN_ACQUISITIONCONFIG_T8, &size, &obj_address);
	
			/* change calibration suspend settings to zero until calibration confirmed good */
			/* store normal settings */
			/* read_mem(copy_data, obj_address+6, (u8)size, &atchcalst); */
			/* read_mem(copy_data, obj_address+7, (u8)size, &atchcalsthr); */
	
			/* resume calibration must be performed with zero settings */
			atchcalst_tmp = 0;
			atchcalsthr_tmp = 0;
	
			ret = write_mem(copy_data, obj_address+4, 1, &atchcalst_tmp);	/* TCHAUTOCAL */
	
			ret = write_mem(copy_data, obj_address+6, 1, &atchcalst_tmp);
			ret1 = write_mem(copy_data, obj_address+7, 1, &atchcalsthr_tmp);

			if(copy_data->family_id == 0x81) { /* mxT224E */
				ret = write_mem(copy_data, obj_address+8, 1, &atchcalst_tmp);	  /* forced cal thr  */
				ret1 = write_mem(copy_data, obj_address+9, 1, &atchcalsthr_tmp); /* forced cal thr ratio */
			}

			/* Write temporary acquisition config to chip. */
			/*
			if (write_acquisition_config(acquisition_config) != CFG_WRITE_OK) {
				"Acquisition config write failed!\n"
				printk("\n[TSP][ERROR] line : %d\n", __LINE__);
				ret = WRITE_MEM_FAILED; calling function should retry calibration call
			}*/
	
			/* restore settings to the local structure so that when we confirm the
			 * cal is good we can correct them in the chip
			 * this must be done before returning */
			/*
			printk(KERN_ERR"[TSP] acquition restore! atchcalst=%d, atchcalsthr=%d\n", atchcalst, atchcalsthr );
			write_mem(copy_data, obj_address+6, size, &atchcalst);
			write_mem(copy_data, obj_address+7, size, &atchcalsthr);
			*/
		}
	}
}


#ifdef CLEAR_MEDIAN_FILTER_ERROR
ERR_RTN_CONTIOIN Check_Err_Condition(void)
{
	ERR_RTN_CONTIOIN rtn;

		switch(gErrCondition)
		{
			case ERR_RTN_CONDITION_IDLE:
		#if !defined (CONFIG_USA_MODEL_SGH_I577) && !defined(CONFIG_CAN_MODEL_SGH_I577R) && !defined (CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
				rtn = ERR_RTN_CONDITION_T9;
				break;

			case ERR_RTN_CONDITION_T9:
				rtn = ERR_RTN_CONDITION_T48;
				break;

			case ERR_RTN_CONDITION_T48:
				rtn = ERR_RTN_CONDITION_IDLE;
				break;
		#endif	
			default:
				rtn = ERR_RTN_CONDITION_T9;
				break;
		}
	return rtn;
}


//110926 zero charger & palne noise apply
static int median_err_setting(void)
{
	u16 obj_address;
	u16 size_one;
	u8 value, state;
	bool ta_status_check;
	int ret = 0;

	copy_data->read_ta_status(&ta_status_check);
	if(!ta_status_check)
	{
		gErrCondition = Check_Err_Condition();
			
		switch(gErrCondition)
		{
			case ERR_RTN_CONDITION_T9:
			{
				
			#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)				
				get_object_info(copy_data, GEN_ACQUISITIONCONFIG_T8, &size_one, &obj_address);
				value = 22;
				write_mem(copy_data, obj_address, 1, &value);				
				ret = get_object_info(copy_data, SPT_CTECONFIG_T46, &size_one, &obj_address);
				value = 32;
				write_mem(copy_data, obj_address+3, 1, &value);
				ret = get_object_info(copy_data, PROCG_NOISESUPPRESSION_T48, &size_one, &obj_address);
				value = 114;
				write_mem(copy_data, obj_address+2, 1, &value); 					
#if defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)			
				value = 20;	
#else
				value = 2;	
#endif
				write_mem(copy_data, obj_address+3, 1, &value);
				value = 1;
				write_mem(copy_data, obj_address+8, 1, &value);
				value = 2;
				write_mem(copy_data, obj_address+9, 1, &value); 				
				value = 100;
				write_mem(copy_data, obj_address+22, 1, &value);
				value = 100;
				write_mem(copy_data, obj_address+25, 1, &value);
				value = 0;
				write_mem(copy_data, obj_address+34, 1, &value);
				value = 40;											
				write_mem(copy_data, obj_address+35, 1, &value);
				value = 81;
				write_mem(copy_data, obj_address+39, 1, &value);
				
				break;
			#else			
				ret = get_object_info(copy_data, TOUCH_MULTITOUCHSCREEN_T9, &size_one, &obj_address);
				value = 16;
				write_mem(copy_data, obj_address+6, 1, &value);
				value = 40;
				write_mem(copy_data, obj_address+7, 1, &value);
				value = 47;
				write_mem(copy_data, obj_address+13, 1, &value);
			#endif		
			}
			#if !defined (CONFIG_USA_MODEL_SGH_I577) && !defined(CONFIG_CAN_MODEL_SGH_I577R) && !defined (CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
				break;
			
			case ERR_RTN_CONDITION_T48:
			{
				ret = get_object_info(copy_data, PROCG_NOISESUPPRESSION_T48, &size_one, &obj_address);
				value = 29;
				write_mem(copy_data, obj_address+3, 1, &value); 
			}
				break;
		
			case ERR_RTN_CONDITION_IDLE:
			{
				ret = get_object_info(copy_data, TOUCH_MULTITOUCHSCREEN_T9, &size_one, &obj_address);
				value = 32;
				write_mem(copy_data, obj_address+6, 1, &value);
				value = 50;
				write_mem(copy_data, obj_address+7, 1, &value);
				value = 46;
				write_mem(copy_data, obj_address+13, 1, &value);
				ret = get_object_info(copy_data, PROCG_NOISESUPPRESSION_T48, &size_one, &obj_address);
				value = 0;
				write_mem(copy_data, obj_address+3, 1, &value); 
			}
			break;
			#endif

			default:
			break;
		}
	#if !defined (CONFIG_USA_MODEL_SGH_I577) && !defined(CONFIG_CAN_MODEL_SGH_I577R) && !defined (CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
		ret = get_object_info(copy_data, PROCG_NOISESUPPRESSION_T48, &size_one, &obj_address);
		read_mem(copy_data, obj_address+2, 1, &value);
		printk(KERN_ERR"[TSP]TA_probe MXT224E T%d Byte%d is %d\n",48,2,value);
		value = value & 0xDF;
		write_mem(copy_data, obj_address+2, 1, &value);
		mdelay(5);
		value = value | 0x20;
		write_mem(copy_data, obj_address+2, 1, &value);
	#endif		
	}
	else
	{
	#if !defined (CONFIG_USA_MODEL_SGH_I577) && !defined(CONFIG_CAN_MODEL_SGH_I577R) && !defined (CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
	  get_object_info(copy_data, SPT_USERDATA_T38, &size_one, &obj_address);
	  read_mem(copy_data, obj_address+1, 1, &value);
	  printk(KERN_ERR"[TSP]info value is %d\n",value);
	#endif	
	  value= 1;
	  if(noise_median.mferr_count < 3)
	   noise_median.mferr_count++;
	  if (!(noise_median.mferr_count%value)&&(noise_median.mferr_count < 3)) //3 
	  {
	   printk(KERN_DEBUG"[TSP] median thr noise level too high. %d\n", noise_median.mferr_count/value);
	   state= noise_median.mferr_count/value;
	   
	#if !defined (CONFIG_USA_MODEL_SGH_I577) && !defined(CONFIG_CAN_MODEL_SGH_I577R) && !defined (CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
	   get_object_info(copy_data, SPT_USERDATA_T38, &size_one, &obj_address);
	   read_mem(copy_data, obj_address+2, 1, & noise_median.t48_mfinvlddiffthr_for_mferr);
	   printk(KERN_ERR"[TSP]mfinvlddiffthr value is %d\n", noise_median.t48_mfinvlddiffthr_for_mferr);
	   read_mem(copy_data, obj_address+3, 1, & noise_median.t48_mferrorthr_for_mferr);
	   printk(KERN_ERR"[TSP]mferrorthr value is %d\n",noise_median.t48_mferrorthr_for_mferr);
	   read_mem(copy_data, obj_address+4, 1, &noise_median.t46_actvsyncsperx_for_mferr);
	   printk(KERN_ERR"[TSP]actvsyncsperx value is %d\n",noise_median.t46_actvsyncsperx_for_mferr);
	   read_mem(copy_data, obj_address+5, 1, &noise_median.t48_thr_for_mferr);
	   printk(KERN_ERR"[TSP]t48_thr_for_mferr value is %d\n",noise_median.t48_thr_for_mferr);
	   read_mem(copy_data, obj_address+6, 1, &noise_median.t48_movfilter_for_mferr);
	   printk(KERN_ERR"[TSP]t48_movfilter_for_mferr value is %d\n",noise_median.t48_movfilter_for_mferr);
	#endif
	
	#if !defined (CONFIG_USA_MODEL_SGH_I577) && !defined(CONFIG_CAN_MODEL_SGH_I577R) && !defined (CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
	   get_object_info(copy_data, PROCG_NOISESUPPRESSION_T48, &size_one, &obj_address);
	#else
	   ret |= get_object_info(copy_data, PROCG_NOISESUPPRESSION_T48, &size_one, &obj_address);
	#endif
	   
	   if(state == 1)
	   {
		value = noise_median.t48_mfinvlddiffthr_for_mferr;
		write_mem(copy_data, obj_address+22, 1, &value);
		value = noise_median.t48_mferrorthr_for_mferr; 
		write_mem(copy_data, obj_address+25, 1, &value);
		value = noise_median.t48_thr_for_mferr;
		write_mem(copy_data, obj_address+35, 1, &value);
		value = noise_median.t48_movfilter_for_mferr;
		write_mem(copy_data, obj_address+39, 1, &value);
		
	#if !defined (CONFIG_USA_MODEL_SGH_I577) && !defined(CONFIG_CAN_MODEL_SGH_I577R) && !defined (CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
		get_object_info(copy_data, SPT_CTECONFIG_T46, &size_one, &obj_address);
	#else
		ret |= get_object_info(copy_data, SPT_CTECONFIG_T46, &size_one, &obj_address);
	#endif
		value = noise_median.t46_actvsyncsperx_for_mferr;
		write_mem(copy_data, obj_address+3, 1, &value); }
	   else if(state >= 2)
	   {
		value = 10; //basefreq
		write_mem(copy_data, obj_address+3, 1, &value);
		value = 0; //secondmf
		write_mem(copy_data, obj_address+8, 1, &value);
		value = 0; //thirdmf
		write_mem(copy_data, obj_address+9, 1, &value);
		value = 20; //mfinvlddiffthr
		write_mem(copy_data, obj_address+22, 1, &value);
		value = 38; //mferrorthr
		write_mem(copy_data, obj_address+25, 1, &value);
		value = 45; //thr
		write_mem(copy_data, obj_address+35, 1, &value);
		value = 65; //movfilter
		write_mem(copy_data, obj_address+39, 1, &value);
		
	#if !defined (CONFIG_USA_MODEL_SGH_I577) && !defined(CONFIG_CAN_MODEL_SGH_I577R) && !defined (CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
		get_object_info(copy_data, SPT_CTECONFIG_T46, &size_one, &obj_address);
	#else
		ret |= get_object_info(copy_data, SPT_CTECONFIG_T46, &size_one, &obj_address);
	#endif
		value = 63; //actvsyncsperx
		write_mem(copy_data, obj_address+3, 1, &value);
	   }
	   noise_median.mferr_setting = true;
	  }
	 }	
#if !defined (CONFIG_USA_MODEL_SGH_I577) && !defined(CONFIG_CAN_MODEL_SGH_I577R) && !defined (CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
	return 0;
#else
	return ret;
#endif
}



#endif

static irqreturn_t mxt224_irq_thread(int irq, void *ptr)
{
	struct mxt224_data *data = ptr;
	int id;
	u8 msg[data->msg_object_size];
	u8 touch_message_flag = 0;

#if !defined (CONFIG_USA_MODEL_SGH_I577) && !defined(CONFIG_CAN_MODEL_SGH_I577R) &&  !defined (CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
	u16 size_one;
	u8 value, ret;
	bool ta_status=0;
	u16 obj_address = 0;

	#ifndef CLEAR_MEDIAN_FILTER_ERROR
		unsigned int register_address = 0;
	#else
		bool ta_status=0;
	#endif

#endif
	
	mutex_lock(&lock);

	disable_irq_nosync(irq);

#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)		
    if((palm_chk_flag == 2) && (data->family_id==0x80)) { //20120213
#else
    if(palm_chk_flag == 2) {
#endif
		palm_recovery();
	}

	do {

		touch_message_flag = 0;

		if(gpio_get_value(data->gpio_read_done))
			break;
		
		if (read_mem(data, data->msg_proc, sizeof(msg), msg)) {
			enable_irq(irq);
			goto unlock;
		}

		/*
		printk(KERN_ERR"[TSP] Starting irq with 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x (%3d)", msg[0], msg[1], msg[2], msg[3], msg[4], msg[5], msg[6], msg[7], LoopFlag++);
		*/

		if ((msg[0] == 0x1) && ((msg[1]&0x10) == 0x10)) /* caliration */
			printk(KERN_ERR"[TSP] Calibration!!!!!!");
		else if ((msg[0] == 0x1) && ((msg[1]&0x40) == 0x40)) /* overflow */
			printk(KERN_ERR"[TSP] Overflow!!!!!!");
		else if ((msg[0] == 0x1) && ((msg[1]&0x10) == 0x00)) {/* caliration */
			 Doing_calibration_falg = 0; 
			printk(KERN_ERR"[TSP] Calibration End!!!!!!");

			// Clear all unused slots after calibration
			TSP_clear_unused_slots();
			
			#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)		
			if(cal_check_flag == 1)
		    	{
			    	qt_timer_state=0;
			    	qt_time_point = jiffies_to_msecs(jiffies);
		    	}
			#else
			if(cal_check_flag == 0)
			{
				palm_recovery();
				cal_check_flag = 1u;
			}
			else if(cal_check_flag == 1)
			{
				qt_timer_state=0;
	                	qt_time_point = jiffies_to_msecs(jiffies);
			}
			#endif		
			}
		
	#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)	
		if ((msg[0] == 14)&&(data->family_id==0x80)) { /* Palm release */
	#else
		if (msg[0] == 14) { /* Palm release */
	#endif			
			if((msg[1]&0x01)== 0x00) {
				touch_is_pressed = 0;
			} else if ((msg[1]&0x01) == 0x01) { /* Palm Press */
				touch_is_pressed = 1;
				touch_message_flag = 1;
			} else {
				/* printk(KERN_ERR"[TSP] palm error msg[1] is %d!!!\n",msg[1]); */
			}
		}

	#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)		
		if ((msg[0] == 0xf)&& (data->family_id==0x80)) { /* freq error release */
	#else
		if (msg[0] == 0xf) { /* freq error release */
	#endif			
			printk(KERN_ERR"[TSP] Starting irq with 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x", msg[0], msg[1], msg[2], msg[3], msg[4], msg[5], msg[6], msg[7]);
			if ((msg[1]&0x08) == 0x08)
				calibrate_chip();
		}


	#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)		
		#ifdef CLEAR_MEDIAN_FILTER_ERROR
		if((msg[0] == 18) && (data->family_id==0x81)) {
			//printk(KERN_ERR"[TSP] Starting irq with 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x", msg[0], msg[1], msg[2], msg[3], msg[4], msg[5], msg[6], msg[7]);
			if((msg[4]&0x5) == 0x5){
				printk(KERN_ERR"[TSP] median filter state error!!!\n");
				median_err_setting();
				}
			}		
		#endif
	#else
		#ifdef CLEAR_MEDIAN_FILTER_ERROR
		if((msg[0] == 18) && (data->family_id==0x81))
		{
			//printk(KERN_ERR"[TSP] Starting irq with 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x, 0x%2x", msg[0], msg[1], msg[2], msg[3], msg[4], msg[5], msg[6], msg[7]);
			if((msg[4]&0x5) == 0x5){
				printk(KERN_ERR"[TSP] median filter state error!!!\n");
				median_err_setting();
					}
			else if((msg[4]&0x4) == 0x4)
				{
				printk(KERN_ERR"[TSP] median filter ON!!!\n");
				copy_data->read_ta_status(&ta_status);
				if(!ta_status)
						{
					ret = get_object_info(copy_data, SPT_CTECONFIG_T46, &size_one, &obj_address);
					value = 38;
					write_mem(copy_data, obj_address+3, 1, &value); 
				}
			}
		}
		#else // ORIGINAL
		if((msg[0] == 18) && (data->family_id==0x81)){
			if((msg[4]&0x5) == 0x5){
				printk(KERN_ERR"[TSP] median filter state error!!!\n");
				register_address=2;
				ret = get_object_info(data, PROCG_NOISESUPPRESSION_T48, &size_one, &obj_address);
				size_one = 1;
				read_mem(data, obj_address+(u16)register_address, size_one, &value);
				printk(KERN_ERR"[TSP]TA_probe MXT224E T%d Byte%d is %d\n",48,register_address,value);
				value = value & 0xDF; 
				write_mem(data, obj_address+(u16)register_address, size_one, &value);
				mdelay(5);
				value = value | 0x20; 
				write_mem(data, obj_address+(u16)register_address, size_one, &value);
					
			}
		}		
		#endif
	#endif		

		
		
		if (msg[0] > 1 && msg[0] <12){

			if ( (touch_is_pressed_arr[msg[0]-2] == 1) && (msg[1]&0x40) ) {
				printk(KERN_ERR "[TSP] Calibrate on Ghost touch");
				calibrate_chip();
				touch_is_pressed_arr[msg[0]-2] = 0;
			}
			
			if ((msg[1] & 0x20) == 0x20) { /* Release */
				/* touch_is_pressed = 0; */
				/* touch_is_pressed_arr[msg[0]-2] = 0; */
				 				
			} else if ((msg[1] & 0x90) == 0x90) { /* Detect & Move */
				touch_message_flag = 1;
			} else	if ((msg[1] & 0xC0) == 0xC0) { /* Detect & Press */
				/* touch_is_pressed = 1; */
				/* touch_is_pressed_arr[msg[0]-2] = 1; */
				touch_message_flag = 1;
			}

			id = msg[0] - data->finger_type;

			/* tyoony.yoon do not need anymore */
			/* If not a touch event, then keep going */
			/*
			if (id < 0 || id >= data->num_fingers){
					continue;
				}
			*/
		#if !defined (CONFIG_USA_MODEL_SGH_I577) && !defined(CONFIG_CAN_MODEL_SGH_I577R) && !defined (CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
			if (data->finger_mask & (1U << id))
				report_input_data(data);
		#endif
			if (msg[1] & RELEASE_MSG_MASK) {
				data->fingers[id].z = 0;
				data->fingers[id].w = msg[5];
				data->finger_mask |= 1U << id;
				
				touch_is_pressed_arr[msg[0]-2] = 0;
				lock_status=0;
				touch_state=1;
				/*printk(KERN_ERR "[TSP] lock_status is %d",lock_status);
			    pr_emerg("%s: dvfs unlock\n", __func__);*/
#if defined (CONFIG_JPN_MODEL_SC_03D)
				/* for debugging log */
				if (g_debug_switch)
				printk(KERN_ERR "[TSP] Finger[%d] Up  (%d,%d) size : %d", id, data->fingers[id].x, data->fingers[id].y, data->fingers[id].w);
#endif
			} else if ((msg[1] & DETECT_MSG_MASK) && (msg[1] & (PRESS_MSG_MASK|MOVE_MSG_MASK))) {
					/*pr_emerg("%s: dvfs lock\n", __func__);*/

				if (lock_status == 0) {
						 lock_status=1;
						}

					if(msg[1] & PRESS_MSG_MASK)
					touch_is_pressed_arr[id] = 1;
					
			#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)		
				else if(msg[1] & MOVE_MSG_MASK) {
					touch_is_pressed_arr[id] = 2;					
					if(gResume_flag) {
						gResume_flag = 0;
						//NoiseEntryStartFunc();
						}
					}
			#else	
				else if(msg[1] & MOVE_MSG_MASK)
					touch_is_pressed_arr[id] = 2;
			#endif

#ifdef CONFIG_TOUCHSCREEN_MXT768E
				data->fingers[id].z = 40;
#else
				data->fingers[id].z = msg[6];
#endif
				data->fingers[id].w = msg[5];
				data->fingers[id].x = ((msg[2] << 4) | (msg[4] >> 4)) >>
								data->x_dropbits;
				data->fingers[id].y = ((msg[3] << 4) |
						(msg[4] & 0xF)) >> data->y_dropbits;
#ifdef CONFIG_KOR_SHV_E120L_WXGA
/*
				// 1.66 => 800
				data->fingers[id].x *= 2;   data->fingers[id].x /= 10;				
				data->fingers[id].x *= 83; data->fingers[id].x /= 10;
				// 1.7 => 1360
				data->fingers[id].y *= 17; data->fingers[id].y /= 10;							
*/
				// 1.7 => 800
				data->fingers[id].x *= 18; data->fingers[id].x /= 10;				
				// 1.7 => 1360
				data->fingers[id].y *= 17; data->fingers[id].y /= 10;
#endif
				data->finger_mask |= 1U << id;
#if defined(DRIVER_FILTER)
                if(msg[1] & PRESS_MSG_MASK){
					/* printk(KERN_ERR "[TSP] Before Finger[%d] Down  (%d,%d) size : %d", id, data->fingers[id].x, data->fingers[id].y, data->fingers[id].w); */
                equalize_coordinate(1, id, &data->fingers[id].x, &data->fingers[id].y);
					/* printk(KERN_ERR "[TSP] Finger[%d] Down  (%d,%d) size : %d", id, data->fingers[id].x, data->fingers[id].y, data->fingers[id].w); */
#if defined (CONFIG_JPN_MODEL_SC_03D)
					/* for debugging log */
					if (g_debug_switch)
					printk(KERN_ERR "[TSP] Finger[%d] Down  (%d,%d) size : %d", id, data->fingers[id].x, data->fingers[id].y, data->fingers[id].w);
#endif
				} else if (msg[1] & MOVE_MSG_MASK) {
					/* printk(KERN_ERR "[TSP] Before Finger[%d] MOVE  (%d,%d) size : %d", id, data->fingers[id].x, data->fingers[id].y, data->fingers[id].w); */
				equalize_coordinate(0, id, &data->fingers[id].x, &data->fingers[id].y);
					/* printk(KERN_ERR "[TSP] Finger[%d] MOVE  (%d,%d) size : %d", id, data->fingers[id].x, data->fingers[id].y, data->fingers[id].w); */
				}
#endif
				#ifdef _SUPPORT_SHAPE_TOUCH_
					data->fingers[id].component= msg[7];
				#endif

			} else if ((msg[1] & SUPPRESS_MSG_MASK) && (data->fingers[id].z != -1)) {
				data->fingers[id].z = 0;
				data->fingers[id].w = msg[5];
				data->finger_mask |= 1U << id;
			} else {
				dev_dbg(&data->client->dev, "Unknown state %#02x %#02x", msg[0], msg[1]);
				continue;
			}
		}


	#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)				
		if (data->finger_mask)
			report_input_data(data);
	#endif
		// Verified by Xtopher, check_chip_cal shoud be called at here
	    if(touch_message_flag && (cal_check_flag/*==2*/)) {
			/* printk(KERN_ERR"[TSP]check chip calibration is called\n"); */
	            check_chip_calibration(1);
	    }

		
#ifdef CONFIG_KOR_SHV_E120L_WXGA		
	} while (gpio_get_value(data->gpio_read_done));
#else
	} while (!gpio_get_value(data->gpio_read_done));
#endif	

#if !defined (CONFIG_USA_MODEL_SGH_I577) && !defined(CONFIG_CAN_MODEL_SGH_I577R) && !defined (CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
	if (data->finger_mask)
		report_input_data(data);
#endif
	#if 0 // Xtopher blocked, chaning of optical gain doesn't need after applying config at machine
	if(!optiacl_gain) {
		mxt224_optical_gain(data->family_id,QT_REFERENCE_MODE);
		optiacl_gain = 1;
	}
	#endif

	enable_irq(irq);

unlock:
	mutex_unlock(&lock);
	return IRQ_HANDLED;
}

static int mxt224_internal_suspend(struct mxt224_data *data)
{
	static const u8 sleep_power_cfg[3];
	int ret;
	int i;

#if defined (CONFIG_KOR_MODEL_SHV_E110S)
		if(get_hw_rev()<0x04){
	/* Set power config. */
	/* Set Idle Acquisition Interval to 32 ms. */
	/*power_config_sleep.idleacqint = 0; */
	/* Set Active Acquisition Interval to 16 ms. */
	/*power_config_sleep.actvacqint = 0;*/

	ret = write_config(data, GEN_POWERCONFIG_T7, sleep_power_cfg);
	if (ret)
		return ret;
		}
#elif defined(CONFIG_EUR_MODEL_GT_I9210)
	if(get_hw_rev()<0x05){
		ret = write_config(data, GEN_POWERCONFIG_T7, sleep_power_cfg);
		if (ret)
			return ret;
	}
#elif defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_I727)
#if defined (CONFIG_USA_MODEL_SGH_I727)
	if(get_hw_rev()<0x06)
#elif defined (CONFIG_USA_MODEL_SGH_T989)
	if(get_hw_rev()<0x09)
#endif
		{

			ret = write_config(data, GEN_POWERCONFIG_T7, sleep_power_cfg);
			if (ret)
				return ret;
		}
	#else
	/* Set power config. */
	/* Set Idle Acquisition Interval to 32 ms. */
	/*power_config_sleep.idleacqint = 0; */
	/* Set Active Acquisition Interval to 16 ms. */
	/*power_config_sleep.actvacqint = 0;*/

	ret = write_config(data, GEN_POWERCONFIG_T7, sleep_power_cfg);
	if (ret)
		return ret;
#endif

	for (i = 0; i < data->num_fingers; i++) {
		if (data->fingers[i].z == -1)
			continue;

		touch_is_pressed_arr[i] = 0;
		data->fingers[i].z = 0;
	}
	report_input_data(data);

	data->power_off();

	return 0;
}

static int mxt224_internal_resume(struct mxt224_data *data)
{
	int ret = 0;
	int i;

	if(get_hw_rev()<0x04){
		i = 0;
			do {
				ret = write_config(data, GEN_POWERCONFIG_T7, data->power_cfg);
				msleep(20);
				i++;
			} while (ret && i < 10);
			msleep(20);
		}
		else
		{
			data->power_on();
		}

	boot_or_resume = 1;
	#if defined(CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
		noise_median.mferr_count = 0;
		noise_median.mferr_setting = false;
	#endif
	return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
#define mxt224_suspend	NULL
#define mxt224_resume	NULL

static void mxt224_early_suspend(struct early_suspend *h)
{
	struct mxt224_data *data = container_of(h, struct mxt224_data,
								early_suspend);
	mutex_lock(&lock);

	mxt224_enabled = 0;
	touch_is_pressed = 0;
	qt_timer_state = 0;	
	cghost_clear = 0;

	#if defined(CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
			#ifdef CLEAR_MEDIAN_FILTER_ERROR
		noise_median.mferr_count=0;
		noise_median.mferr_setting = false;
		#endif	
		not_yet_count = 0;
		gResume_flag = 0;
		//NoiseEntryStartFunc();
	#endif
	
	disable_irq(data->client->irq);
	mxt224_internal_suspend(data);

	mutex_unlock(&lock);
	printk(KERN_ERR "[TSP] mxt224_early_suspend \n");
}

static void mxt224_late_resume(struct early_suspend *h)
{
	bool ta_status=0;
	struct mxt224_data *data = container_of(h, struct mxt224_data,
								early_suspend);
	mutex_lock(&lock);
	mxt224_internal_resume(data);
	enable_irq(data->client->irq);

	mxt224_enabled = 1;
	is_inputmethod = 0;   // I know it's fault, but app couldn't solve the issue wighout this known fault.
	qt_timer_state = 0;
	#if defined(CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
		gResume_flag = 1;
	#endif
	
	#ifdef CLEAR_MEDIAN_FILTER_ERROR
	noise_median.mferr_count=0;
	noise_median.mferr_setting = false;	
	#if defined(CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
		noise_median.median_on_flag = false;	
	#endif
	#endif	
	if(data->read_ta_status) {
		data->read_ta_status(&ta_status);
		printk(KERN_ERR "[TSP] ta_status in (mxt224_late_resume) is %d", ta_status);
		mxt224_ta_probe(ta_status);
	}
	calibrate_chip();
	mutex_unlock(&lock);
	printk(KERN_ERR "[TSP] mxt224_late_resume \n");
}
#else
static int mxt224_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt224_data *data = i2c_get_clientdata(client);

	mxt224_enabled = 0;
	touch_is_pressed = 0;
	 Doing_calibration_falg = 0; 
	return mxt224_internal_suspend(data);
}

static int mxt224_resume(struct device *dev)
{
	int ret = 0;
	bool ta_status=0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt224_data *data = i2c_get_clientdata(client);

	ret = mxt224_internal_resume(data);

	mxt224_enabled = 1;

	if(data->read_ta_status) {
		data->read_ta_status(&ta_status);
		printk(KERN_ERR "[TSP] ta_status is %d", ta_status);
		mxt224_ta_probe(ta_status);
	}
	return ret;
}
#endif

void Mxt224_force_released(void)
{
/*
    struct mxt224_data*data = copy_data;
	int i;
*/
	if(!mxt224_enabled) {
		printk(KERN_ERR "[TSP] mxt224_enabled is 0\n");
		return;
	}

	calibrate_chip();

	touch_is_pressed = 0;
};
EXPORT_SYMBOL(Mxt224_force_released);

static void TSP_clear_unused_slots(void)
{
#if !defined(TOUCH_NON_SLOT)
	// It is possible for the state of the framework and the driver to get out
	// of sync resulting in "stuck" touch points from the framework's perspective.
	// For all unused slots, send an "unused" message (-1 for ABS_MT_TRACKING_ID).
	// But, the input_handle_abs_event suppresses any message that it thinks won't
	// result in a state change. Send a valid ID first then a -1 to trick it.
	// Since the driver thinks that it has already sent the "unused" message
	// (a -1 for ABS_MT_TRACKING_ID) but the framework could have missed it,
	// set the ID to a real number and then immediate set it to -1.
	//
	// This *seems* to only be needed after calibration and after error cases.

	int i;
	printk(KERN_ERR "[TSP] Clearing unused slots\n");
	for (i = 0; i < copy_data->num_fingers; i++) {
		if (copy_data->fingers[i].z == -1) {
			//printk(KERN_ERR "[TSP] TSP_clear_unused_slots clearing slot %d\n", i);
			input_mt_slot(copy_data->input_dev, i);
			input_event(copy_data->input_dev, EV_ABS, ABS_MT_TRACKING_ID, i);
			input_event(copy_data->input_dev, EV_ABS, ABS_MT_TRACKING_ID, -1);
		}
	}

	input_sync(copy_data->input_dev);
#endif
}

void TSP_forced_release_for_call(void)
{
	int i=0;

	if (!mxt224_enabled) {
		printk(KERN_ERR "[TSP] mxt224_enabled is 0\n");
		return;
	}

	touch_is_pressed = 0;
	

#if 1 // Just only for JellyBean
	for (i = 0; i < MXT224_MAX_MT_FINGERS; i++) {
		copy_data->fingers[i].z = 0;
		input_mt_slot(copy_data->input_dev, i);
		input_mt_report_slot_state(copy_data->input_dev, MT_TOOL_FINGER,
									false);

		touch_is_pressed_arr[i] = 0;
	}
#else
	for (i = 0; i < copy_data->num_fingers; i++) {

		if (copy_data->fingers[i].z == -1) {
#if !defined(TOUCH_NON_SLOT)
			// It is possible for the state of the framework and the driver to get out
			// of sync resulting in "stuck" touch points from the framework's perspective.
			// For all unused slots, send an "unused" message (-1 for ABS_MT_TRACKING_ID).
			// But, the input_handle_abs_event suppresses any message that it thinks won't
			// result in a state change. Send a valid ID first then a -1 to trick it.
			// Since the driver thinks that it has already sent the "unused" message
			// (a -1 for ABS_MT_TRACKING_ID) but the framework could have missed it,
			// set the ID to a real number and then immediate set it to -1.
			//
			// This *seems* to only be needed after calibration and after error cases.

			//printk(KERN_ERR "[TSP] TSP_forced_release_for_call clearing slot %d\n", i);
			input_mt_slot(copy_data->input_dev, i);
			input_event(copy_data->input_dev, EV_ABS, ABS_MT_TRACKING_ID, i);
			input_event(copy_data->input_dev, EV_ABS, ABS_MT_TRACKING_ID, -1);
#endif
			continue;
		}

		copy_data->fingers[i].z = 0;

#if defined(TOUCH_NON_SLOT)
		input_report_abs(copy_data->input_dev, ABS_MT_POSITION_X, copy_data->fingers[i].x);
		input_report_abs(copy_data->input_dev, ABS_MT_POSITION_Y, copy_data->fingers[i].y);
		input_report_abs(copy_data->input_dev, ABS_MT_TOUCH_MAJOR, copy_data->fingers[i].z);
		input_report_abs(copy_data->input_dev, ABS_MT_WIDTH_MAJOR, copy_data->fingers[i].w);
		input_report_abs(copy_data->input_dev, ABS_MT_TRACKING_ID, i);

		input_mt_sync(copy_data->input_dev);

#else   // SLOT
		input_mt_slot(copy_data->input_dev, i);
		input_mt_report_slot_state(copy_data->input_dev,	MT_TOOL_FINGER, !!copy_data->fingers[i].z);

		input_report_abs(copy_data->input_dev, ABS_MT_POSITION_X, copy_data->fingers[i].x);
		input_report_abs(copy_data->input_dev, ABS_MT_POSITION_Y, copy_data->fingers[i].y);

		input_report_abs(copy_data->input_dev, ABS_MT_PRESSURE, copy_data->fingers[i].w);

		input_report_abs(copy_data->input_dev, ABS_MT_TOUCH_MAJOR, copy_data->fingers[i].z);
#endif
		touch_is_pressed_arr[i] = 0;
	
		if (copy_data->fingers[i].z == 0)
			copy_data->fingers[i].z = -1;
	}
#endif

	//calibrate_chip();
	msleep(20); 

	copy_data->finger_mask = 0;
	touch_state = 0;
	input_sync(copy_data->input_dev);

}

/*
static ssize_t mxt224_debug_setting(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	g_debug_switch = !g_debug_switch;
	return 0;
}
*/

static ssize_t qt602240_object_setting(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	  struct mxt224_data*data = dev_get_drvdata(dev);
	  unsigned int object_type;
	  unsigned int object_register;
	  unsigned int register_value;
	  u8 value;
	  u8 val;
	  int ret;
	  u16 address;
	  u16 size;
	  sscanf(buf,"%u%u%u",&object_type,&object_register,&register_value);
	printk(KERN_ERR "[TSP] object type T%d", object_type);
	printk(KERN_ERR "[TSP] object register ->Byte%d\n", object_register);
	printk(KERN_ERR "[TSP] register value %d\n", register_value);
	ret = get_object_info(data, (u8)object_type, &size, &address);
	if (ret) {
		printk(KERN_ERR "[TSP] fail to get object_info\n");
		return count;
	}

	size = 1;
	value = (u8)register_value;
	mutex_lock(&lock);
	write_mem(data, address+(u16)object_register, size, &value);
	read_mem(data, address+(u16)object_register, (u8)size, &val);
	mutex_unlock(&lock);

	printk(KERN_ERR "[TSP] T%d Byte%d is %d\n", object_type, object_register, val);
#if !defined (CONFIG_USA_MODEL_SGH_I577) && !defined(CONFIG_CAN_MODEL_SGH_I577R) && !defined (CONFIG_USA_MODEL_SGH_I727) && !defined (CONFIG_USA_MODEL_SGH_T989)
	/*test program*/
	printk(KERN_ERR "[TSP] mxt224_ta_probe(1) called by qt602240_object_setting \n");
	mxt224_ta_probe(1);
#endif
	return count;

}

static ssize_t qt602240_object_show(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct mxt224_data *data = dev_get_drvdata(dev);
	unsigned int object_type;
	u8 val;
	int ret;
	u16 address;
	u16 size;
	u16 i;
	sscanf(buf, "%u", &object_type);
	printk(KERN_ERR "[TSP] object type T%d\n", object_type);
	ret = get_object_info(data, (u8)object_type, &size, &address);
	if (ret) {
		printk(KERN_ERR "[TSP] fail to get object_info\n");
		return count;
	}
	mutex_lock(&lock);
	for (i = 0; i < size; i++) {
		read_mem(data, address+i, 1, &val);
		printk(KERN_ERR "[TSP] Byte %u --> %u\n", i, val);
	}
	mutex_unlock(&lock);
	return count;
}

struct device *sec_touchscreen;
struct device *qt602240_noise_test;
/*
	botton_right, botton_left, center, top_right, top_left
*/
unsigned char test_node[5] = {12, 20, 104, 188, 196};
uint16_t qt_refrence_node[209] = { 0 };
uint16_t qt_delta_node[209] = { 0 };


void diagnostic_chip(u8 mode)
{
	int error;
	u16 t6_address = 0;
	u16 size_one;
	int ret;
	u8 value;
	u16 t37_address = 0;
	int retry = 3;

	ret = get_object_info(copy_data, GEN_COMMANDPROCESSOR_T6, &size_one, &t6_address);

	size_one = 1;
	while(retry--){
	error = write_mem(copy_data, t6_address+5, (u8)size_one, &mode);
		/* qt602240_write_object(p_qt602240_data, QT602240_GEN_COMMAND, */
		/* QT602240_COMMAND_DIAGNOSTIC, mode); */
	if (error < 0) {
		printk(KERN_ERR "[TSP] error %s: write_object\n", __func__);
	} else {
		get_object_info(copy_data, DEBUG_DIAGNOSTIC_T37, &size_one, &t37_address);
		size_one = 1;
		/* printk(KERN_ERR"diagnostic_chip setting success\n"); */
		read_mem(copy_data, t37_address, (u8)size_one, &value);
			return;
		/* printk(KERN_ERR"dianostic_chip mode is %d\n",value); */
	}
}
	printk(KERN_ERR "[TSP] error %s: write_object fail!!\n", __func__);
	mxt224_reset(copy_data);
	return;
}

void read_dbg_data(uint8_t dbg_mode , uint8_t node, uint16_t *dbg_data)
{
	u8 read_page, read_point;
	u8 data_buffer[2] = { 0 };
	int i, ret;
	u16 size;
	u16 object_address = 0;

	read_page = node / 64;
	node %= 64;
	read_point = (node * 2) + 2;

	if (!mxt224_enabled) {
		printk(KERN_ERR "[TSP ]read_dbg_data. mxt224_enabled is 0\n");
		return;
	}

	

	/* Page Num Clear */
	diagnostic_chip(QT_CTE_MODE);
	msleep(20);

	diagnostic_chip(dbg_mode);
	msleep(20);

	ret = get_object_info(copy_data, DEBUG_DIAGNOSTIC_T37, &size, &object_address);
#if 0
	for (i = 0; i < 5; i++) {
		/* qt602240_read_diagnostic(0, data_buffer, 1); */
		if (data_buffer[0] == dbg_mode)
			break;

		msleep(20);
	}
#else
	msleep(20);
#endif
	printk(KERN_DEBUG "[TSP] page clear\n");

	for (i = 1; i <= read_page; i++) {
		diagnostic_chip(QT_PAGE_UP);
		msleep(20);
		/* qt602240_read_diagnostic(1, data_buffer, 1); */
		read_mem(copy_data, object_address+1, 1, data_buffer);
		if (data_buffer[0] != i) {
			if (data_buffer[0] >= 0x4)
				break;
			i--;
		}
	}

	/* qt602240_read_diagnostic(read_point, data_buffer, 2); */
	read_mem(copy_data, object_address+(u16)read_point, 2, data_buffer);
	*dbg_data = ((uint16_t)data_buffer[1]<<8) + (uint16_t)data_buffer[0];
}

#define MAX_VALUE 3680
#define MIN_VALUE 13200

int read_all_data(uint16_t dbg_mode)
{
	u8 read_page, read_point;
	u16 max_value = MAX_VALUE, min_value = MIN_VALUE;
	// uint16_t qt_refrence;
	u16 object_address = 0;
	u8 data_buffer[2] = { 0 };
	u8 node = 0;
	int state = 0;
	int num = 0;	
	int ret;
	u16 size;
	bool ta_state=false;
	if(copy_data->read_ta_status) 
		copy_data->read_ta_status(&ta_state);

	/* Page Num Clear */
	diagnostic_chip(QT_CTE_MODE);
	msleep(30);/* msleep(20);  */

	diagnostic_chip(dbg_mode);
	msleep(30);/* msleep(20);  */

	ret = get_object_info(copy_data, DEBUG_DIAGNOSTIC_T37, &size, &object_address);
/*jerry no need to leave it */
#if 0
	for (i = 0; i < 5; i++) {
		if (data_buffer[0] == dbg_mode)
			break;

		msleep(20);
	}
#else
	msleep(50); /* msleep(20);  */
#endif
	if (copy_data->family_id==0x81) {

		max_value = max_value + 16384;
		min_value = min_value + 16384;
	}

	for (read_page = 0 ; read_page < 4; read_page++) {
		for (node = 0; node < 64; node++) {
			read_point = (node * 2) + 2;
			read_mem(copy_data, object_address+(u16)read_point, 2, data_buffer);
				qt_refrence_node[num] = ((uint16_t)data_buffer[1]<<8)+ (uint16_t)data_buffer[0];
			if (copy_data->family_id==0x81) {
				if ((qt_refrence_node[num] > MIN_VALUE + 16384) || (qt_refrence_node[num] < MAX_VALUE + 16384)) {
				state = 1;
					printk(KERN_ERR"[TSP] Mxt224-E qt_refrence_node[%3d] = %5d \n", num, qt_refrence_node[num]);				
				//	break;
				}
			} else {
				if((qt_refrence_node[num] > MIN_VALUE)||(qt_refrence_node[num] < MAX_VALUE)) {
				state = 1;
					printk(KERN_ERR"[TSP] Mxt224 qt_refrence_node[%3d] = %5d \n", num, qt_refrence_node[num]);
				//	break;
			}
			}

			if (data_buffer[0] != 0) {
				if(qt_refrence_node[num] > max_value)
					max_value = qt_refrence_node[num];
				if(qt_refrence_node[num] < min_value)
					min_value = qt_refrence_node[num];
			}
			num = num+1;

			if(ta_state)  /* (dual-x) all node => 18 * 11 = 198 => (3page * 64) + 6 */
			{	
				if ((read_page == 3) && (node == 5))
					break;
			}
			else  /* all node => 19 * 11 = 209 => (3page * 64) + 17 */
			{  	
			if ((read_page == 3) && (node == 16))
				break;
			}

		}
		diagnostic_chip(QT_PAGE_UP);
		msleep(10);
	}

	if ((max_value - min_value) > 4500) {
		printk(KERN_ERR "[TSP] diff = %d, max_value = %d, min_value = %d\n", (max_value - min_value), max_value, min_value);
		state = 1;
	} 


	return state;
}


int read_all_delta_data(uint16_t dbg_mode)
{
	u8 read_page, read_point;
	// u16 max_value = MAX_VALUE, min_value = MIN_VALUE;
	// uint16_t qt_refrence;
	u16 object_address = 0;
	u8 data_buffer[2] = { 0 };
	u8 node = 0;
	int state = 0;
	int num = 0;	
	int ret;
	u16 size;

	/* Page Num Clear */
	diagnostic_chip(QT_CTE_MODE);
	msleep(30);/* msleep(20);  */

	diagnostic_chip(dbg_mode);
	msleep(30);/* msleep(20);  */

	ret = get_object_info(copy_data, DEBUG_DIAGNOSTIC_T37, &size, &object_address);
/*jerry no need to leave it */
#if 0
	for (i = 0; i < 5; i++) {
		if (data_buffer[0] == dbg_mode)
			break;

		msleep(20);
	}
#else
	msleep(50); /* msleep(20);  */
#endif

	for (read_page = 0 ; read_page < 4; read_page++) {
		for (node = 0; node < 64; node++) {
			read_point = (node * 2) + 2;
			read_mem(copy_data, object_address+(u16)read_point, 2, data_buffer);
				qt_delta_node[num] = ((uint16_t)data_buffer[1]<<8)+ (uint16_t)data_buffer[0];

			num = num+1;

			/* all node => 19 * 11 = 209 => (3page * 64) + 17 */
			if ((read_page == 3) && (node == 16))
				break;

		}
		diagnostic_chip(QT_PAGE_UP);
		msleep(10);
	}

	return state;
}

#if 0
static void mxt224_optical_gain(u8 family_id, uint16_t dbg_mode)
{
	u8 read_page, read_point;
	uint16_t qt_refrence;
	u16 object_address = 0;
	u8 data_buffer[2] = { 0 };
	u8 node = 0;
	int ret, reference_over = 0;
	u16 size;
	u16 size_one;
	u8 value;
	int gain = 0;
	u8 val = 0;
	unsigned int register_address = 6;

	printk(KERN_ERR "[TSP] +mxt224_optical_gain()\n");

	/* Page Num Clear */
	diagnostic_chip(QT_CTE_MODE);
	msleep(5);

	diagnostic_chip(dbg_mode);
	msleep(5);

	ret = get_object_info(copy_data, DEBUG_DIAGNOSTIC_T37, &size, &object_address);
/*jerry no need of it*/
#if 0
	for (i = 0; i < 5; i++) {
		if (data_buffer[0] == dbg_mode)
			break;

		msleep(5);
	}
#else
	msleep(5);
#endif

	for (read_page = 0; read_page < 4; read_page++) {
		for (node = 0; node < 64; node++) {
			read_point = (node * 2) + 2;
			read_mem(copy_data, object_address+(u16)read_point, 2, data_buffer);
			qt_refrence = ((uint16_t)data_buffer[1]<<8) + (uint16_t)data_buffer[0];

			if (family_id == 0x81) 
			{
				qt_refrence = qt_refrence - 16384;
			}
			else
			{

			}
			
			if (qt_refrence > 14080)
				reference_over = 1;

			if ((read_page == 3) && (node == 16))
				break;
		}
		diagnostic_chip(QT_PAGE_UP);
		msleep(5);
	}

	if (reference_over)
		gain = 16;
	else
		gain = 32;

	value = (u8)gain;
	ret = get_object_info(copy_data, TOUCH_MULTITOUCHSCREEN_T9, &size_one, &object_address);
	size_one = 1;
	write_mem(copy_data, object_address+(u16)register_address, size_one, &value);
	read_mem(copy_data, object_address+(u16)register_address, (u8)size_one, &val);

	printk(KERN_ERR "[TSP] -mxt224_optical_gain()\n");
};
#endif

static int mxt224_check_bootloader(struct i2c_client *client,
					unsigned int state)
{
	u8 val;

recheck:
	if (i2c_master_recv(client, &val, 1) != 1) {
		dev_err(&client->dev, "%s: i2c recv failed\n", __func__);
		return -EIO;
	}

	switch (state) {
	case QT602240_WAITING_BOOTLOAD_CMD:
	case QT602240_WAITING_FRAME_DATA:
		val &= ~QT602240_BOOT_STATUS_MASK;
		break;
	case QT602240_FRAME_CRC_PASS:
		if (val == QT602240_FRAME_CRC_CHECK)
			goto recheck;
		break;
	default:
		return -EINVAL;
	}

	if (val != state) {
		dev_err(&client->dev, "Unvalid bootloader mode state\n");
		printk(KERN_ERR "[TSP] Unvalid bootloader mode state\n");
		return -EINVAL;
	}

	return 0;
}

static int mxt224_unlock_bootloader(struct i2c_client *client)
{
	u8 buf[2];

	buf[0] = QT602240_UNLOCK_CMD_LSB;
	buf[1] = QT602240_UNLOCK_CMD_MSB;

	if (i2c_master_send(client, buf, 2) != 2) {
		dev_err(&client->dev, "%s: i2c send failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int mxt224_fw_write(struct i2c_client *client,
				const u8 *data, unsigned int frame_size)
{
	if (i2c_master_send(client, data, frame_size) != frame_size) {
		dev_err(&client->dev, "%s: i2c send failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int mxt224_load_fw(struct device *dev, const char *fn)
{

	struct mxt224_data *data = copy_data;
	struct i2c_client *client = copy_data->client;
	const struct firmware *fw = NULL;
	unsigned int frame_size;
	unsigned int pos = 0;
	int ret;
	u16 obj_address = 0;
	u16 size_one;
	u8 value;
	unsigned int object_register;

	printk(KERN_ERR "[TSP] mxt224_load_fw start!!!\n");

	ret = request_firmware(&fw, fn, &client->dev);
	if (ret) {
		dev_err(dev, "Unable to open firmware %s\n", fn);
		printk(KERN_ERR "[TSP] Unable to open firmware %s\n", fn);
		return ret;
	}

	/* Change to the bootloader mode */
	/* qt602240_write_object(data, QT602240_GEN_COMMAND, QT602240_COMMAND_RESET, QT602240_BOOT_VALUE); */
	object_register = 0;
	value = (u8)QT602240_BOOT_VALUE;
	ret = get_object_info(data, GEN_COMMANDPROCESSOR_T6, &size_one, &obj_address);
	if (ret) {
		printk(KERN_ERR "[TSP] fail to get object_info\n");
		goto out;
	}

	size_one = 1;
	write_mem(data, obj_address+(u16)object_register, (u8)size_one, &value);
	msleep(QT602240_RESET_TIME);

	/* Change to slave address of bootloader */
	if (client->addr == QT602240_APP_LOW)
		client->addr = QT602240_BOOT_LOW;
	else
		client->addr = QT602240_BOOT_HIGH;

	ret = mxt224_check_bootloader(client, QT602240_WAITING_BOOTLOAD_CMD);
	if (ret)
		goto out;

	/* Unlock bootloader */
	mxt224_unlock_bootloader(client);

	while (pos < fw->size) {
		ret = mxt224_check_bootloader(client,
						QT602240_WAITING_FRAME_DATA);
		if (ret)
			goto out;

		frame_size = ((*(fw->data + pos) << 8) | *(fw->data + pos + 1));

		/* We should add 2 at frame size as the the firmware data is not
		* included the CRC bytes.
		*/
		frame_size += 2;

		/* Write one frame to device */
		/* qt602240_fw_write(client, fw->data + pos, frame_size);  */
		mxt224_fw_write(client, fw->data + pos, frame_size);

		ret = mxt224_check_bootloader(client,
						QT602240_FRAME_CRC_PASS);
		if (ret)
			goto out;

		pos += frame_size;

		dev_dbg(dev, "Updated %d bytes / %zd bytes\n", pos, fw->size);
		printk(KERN_ERR "[TSP] Updated %d bytes / %zd bytes\n", pos, fw->size);
	}

out:
	release_firmware(fw);

	/* Change to slave address of application */
	if (client->addr == QT602240_BOOT_LOW)
		client->addr = QT602240_APP_LOW;
	else
		client->addr = QT602240_APP_HIGH;

	return ret;
}

static ssize_t set_tsp_module_off_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret;

	mxt224_enabled = 0;
	touch_is_pressed = 0;
	Doing_calibration_falg = 0; 

	mutex_lock(&lock);
	disable_irq(copy_data->client->irq);
	ret = mxt224_internal_suspend(copy_data);
	mutex_unlock(&lock);

	if (ret == 0)
		*buf = '1';
	else
		*buf = '0';

	return 0;
}

static ssize_t set_tsp_module_on_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	bool ta_status=0;
	/*struct i2c_client *client = to_i2c_client(dev);
	struct mxt224_data *data = i2c_get_clientdata(client);*/

	mutex_lock(&lock);
	copy_data->power_on();
	msleep(70);

	ret = mxt224_internal_resume(copy_data);
	enable_irq(copy_data->client->irq);

	mxt224_enabled = 1;

	if(copy_data->read_ta_status) {
		copy_data->read_ta_status(&ta_status);
		printk(KERN_ERR "[TSP] ta_status is %d", ta_status);
		mxt224_ta_probe(ta_status);
	}
	calibrate_chip();
	mutex_unlock(&lock);

	
	if (ret == 0)
		*buf = '1';
	else
		*buf = '0';

	return 0;
}

static ssize_t set_refer0_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t qt_refrence = 0;
	mutex_lock(&lock);
	read_dbg_data(QT_REFERENCE_MODE, test_node[0], &qt_refrence);
	mutex_unlock(&lock);
	return sprintf(buf, "%u\n", qt_refrence);
}

static ssize_t set_refer1_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t qt_refrence = 0;
	mutex_lock(&lock);
	read_dbg_data(QT_REFERENCE_MODE, test_node[1], &qt_refrence);
	mutex_unlock(&lock);
	return sprintf(buf, "%u\n", qt_refrence);
}

static ssize_t set_refer2_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t qt_refrence = 0;
	mutex_lock(&lock);
	read_dbg_data(QT_REFERENCE_MODE, test_node[2], &qt_refrence);
	mutex_unlock(&lock);
	return sprintf(buf, "%u\n", qt_refrence);
}

static ssize_t set_refer3_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t qt_refrence = 0;
	mutex_lock(&lock);
	read_dbg_data(QT_REFERENCE_MODE, test_node[3], &qt_refrence);
	mutex_unlock(&lock);
	return sprintf(buf, "%u\n", qt_refrence);
}

static ssize_t set_refer4_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t qt_refrence = 0;
	mutex_lock(&lock);
	read_dbg_data(QT_REFERENCE_MODE, test_node[4], &qt_refrence);
	mutex_unlock(&lock);
	return sprintf(buf, "%u\n", qt_refrence);
}

static ssize_t set_delta0_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t qt_delta = 0;
	mutex_lock(&lock);
	read_dbg_data(QT_DELTA_MODE, test_node[0], &qt_delta);
	mutex_unlock(&lock);
	if (qt_delta < 32767)
		return sprintf(buf, "%u\n", qt_delta);
	else
		qt_delta = 65535 - qt_delta;

	return sprintf(buf, "-%u\n", qt_delta);
}

static ssize_t set_delta1_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t qt_delta = 0;
	mutex_lock(&lock);
	read_dbg_data(QT_DELTA_MODE, test_node[1], &qt_delta);
	mutex_unlock(&lock);
	if (qt_delta < 32767)
		return sprintf(buf, "%u\n", qt_delta);
	else
		qt_delta = 65535 - qt_delta;

	return sprintf(buf, "-%u\n", qt_delta);
}

static ssize_t set_delta2_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t qt_delta = 0;
	mutex_lock(&lock);
	read_dbg_data(QT_DELTA_MODE, test_node[2], &qt_delta);
	mutex_unlock(&lock);
	if (qt_delta < 32767)
		return sprintf(buf, "%u\n", qt_delta);
	else
		qt_delta = 65535 - qt_delta;

	return sprintf(buf, "-%u\n", qt_delta);
}

static ssize_t set_delta3_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t qt_delta = 0;
	mutex_lock(&lock);
	read_dbg_data(QT_DELTA_MODE, test_node[3], &qt_delta);
	mutex_unlock(&lock);
	if (qt_delta < 32767)
		return sprintf(buf, "%u\n", qt_delta);
	else
		qt_delta = 65535 - qt_delta;

	return sprintf(buf, "-%u\n", qt_delta);
}

static ssize_t set_delta4_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t qt_delta = 0;
	mutex_lock(&lock);
	read_dbg_data(QT_DELTA_MODE, test_node[4], &qt_delta);
	mutex_unlock(&lock);
	if (qt_delta < 32767)
		return sprintf(buf, "%u\n", qt_delta);
	else
		qt_delta = 65535 - qt_delta;

	return sprintf(buf, "-%u\n", qt_delta);
}

static ssize_t set_threshold_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char temp[15];
	if(mxt_version_disp == 0x80) {
		sprintf(temp, "%u\n", threshold);
		strcat(buf, temp);
	} else if(mxt_version_disp == 0x81) {
		sprintf(temp, "%u\n", threshold_e);
		strcat(buf, temp);
	} else {
		sprintf(temp, "error\n");
		strcat(buf, temp);
	}
	return strlen(buf);
}

static ssize_t set_all_refer_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int status = 0;

	mutex_lock(&lock);
	status = read_all_data(QT_REFERENCE_MODE);
	mutex_unlock(&lock);

	return sprintf(buf, "%u\n", status);
}

static int index =0;

static int atoi(char *str)
{
	int result = 0;
	int count = 0;
	if( str == NULL ) 
		return -1;
	while( str[count] != 0 && str[count] >= '0' && str[count] <= '9' )
	{		
		result = result * 10 + str[count] - '0';
		++count;
	}
	return result;
}

ssize_t disp_all_refdata_show(struct device *dev, struct device_attribute *attr, char *buf)
{

//int status = 0;
//	char tempStr[5*209 + 1] = { 0 };
//nt i = 0;
    return sprintf(buf, "%u\n",  qt_refrence_node[index]);
}

ssize_t disp_all_refdata_store(struct device *dev, struct device_attribute *attr,
								   const char *buf, size_t size)
{

	index = atoi((char *)buf);
  	return size;
}


static ssize_t set_all_delta_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int status = 0;

	mutex_lock(&lock);
	status = read_all_delta_data(QT_DELTA_MODE);
	mutex_unlock(&lock);

	return sprintf(buf, "%u\n", status);
}

static int index_delta =0;


ssize_t disp_all_deltadata_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (qt_delta_node[index_delta] < 32767)
		return sprintf(buf, "%u\n", qt_delta_node[index_delta]);
	else
		qt_delta_node[index_delta] = 65535 - qt_delta_node[index_delta];

	return sprintf(buf, "-%u\n", qt_delta_node[index_delta]);
}


ssize_t disp_all_deltadata_store(struct device *dev, struct device_attribute *attr,
								   const char *buf, size_t size)
{

	index_delta = atoi((char *)buf);
  	return size;
}



static ssize_t set_firm_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	return sprintf(buf, "%#02x\n", tsp_version_disp);

}

static ssize_t set_mxt_update_show(struct device *dev, struct device_attribute *attr, char *buf)
{
		struct mxt224_data *data = copy_data;
		int error = 0;
		int count = 0;
		printk(KERN_ERR "[TSP] set_mxt_update_show start!!\n");

		/*if (sscanf(buf, "%u", &version) != 1) {
			printk(KERN_ERR"Invalid values\n");
			dev_err(dev, "Invalid values\n");
			return -EINVAL;
		}

		if ( tsp_version_disp < QT602240_VER_21 || version < QT602240_VER_21) {
			dev_err(dev, "FW update supported starting with version 21\n");
			printk(KERN_ERR"[TSP]Wrong FW version\n");
			return -EINVAL;
		}*/

		mutex_lock(&lock);
		disable_irq(data->client->irq);
		firm_status_data = 1;
		if (data->family_id == 0x80) {	/*  : MXT-224 */
			printk(KERN_ERR"[TSP] mxt224_fm_update\n");
			error = mxt224_load_fw(dev, MXT224_FW_NAME);
		} else if (data->family_id == 0x81)  {	/* tsp_family_id - 0x81 : MXT-224E */
			printk(KERN_ERR"[TSP] mxt224E_fm_update\n");
			error = mxt224_load_fw(dev, MXT224_ECHO_FW_NAME);
		}
		/*jerry no need of it*/
		/* error = mxt224_load_fw(dev, QT602240_FW_NAME);  */
		if (error) {
			dev_err(dev, "The firmware update failed(%d)\n", error);
			firm_status_data = 3;
			printk(KERN_ERR"[TSP The firmware update failed(%d)\n", error);
			goto err;
		} else {
			dev_dbg(dev, "The firmware update succeeded\n");
			firm_status_data = 2;
			printk(KERN_ERR "[TSP] The firmware update succeeded\n");

			/* Wait for reset */
			mdelay(QT602240_FWRESET_TIME);
            /* initialize the TSP*/
			mxt224_init_touch_driver(data);
			/*jerry no need of it*/
			/* qt602240_initialize(data);  */
		}

		enable_irq(data->client->irq);
		error = mxt224_backup(data);
		if (error) {
			printk(KERN_ERR "[TSP] mxt224_backup fail!!!\n");
			goto err;
		}

	/* reset the touch IC. */
	error = mxt224_reset(data);
	if (error) {
			printk(KERN_ERR"[TSP] mxt224_reset fail!!!\n");
			goto err;
	}

	msleep(60);
	mutex_unlock(&lock);
	return count;

err:
	mutex_unlock(&lock);
	return error;
}

static ssize_t set_mxt_firm_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	int count;
	/* struct mxt_data *mxt = dev_get_drvdata(dev); */
	pr_info("Enter firmware_status_show by Factory command\n");

	if (firm_status_data == 1)
		count = sprintf(buf, "Downloading\n");
	else if (firm_status_data == 2)
		count = sprintf(buf, "PASS\n");
	else if (firm_status_data == 3)
		count = sprintf(buf, "FAIL\n");
	else
		count = sprintf(buf, "PASS\n");

	return count;

}

static ssize_t key_threshold_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", threshold);
}

static ssize_t key_threshold_store(struct device *dev, struct device_attribute *attr,
								const char *buf, size_t size)
{
	/*TO DO IT*/
	unsigned int object_register = 7;
	u8 value;
	u8 val;
	int ret;
	u16 address = 0;
	u16 size_one;
	if (sscanf(buf, "%d", &threshold) == 1) {
		printk(KERN_ERR "[TSP] threshold value %d\n", threshold);
		ret = get_object_info(copy_data, TOUCH_MULTITOUCHSCREEN_T9, &size_one, &address);
		size_one = 1;
		value = (u8)threshold;
		mutex_lock(&lock);
		write_mem(copy_data, address+(u16)object_register, size_one, &value);
		read_mem(copy_data, address+(u16)object_register, (u8)size_one, &val);
		mutex_unlock(&lock);

		printk(KERN_ERR "[TSP] T%d Byte%d is %d\n", TOUCH_MULTITOUCHSCREEN_T9, object_register, val);
	}

	return size;
}

static ssize_t set_mxt_firm_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	u8 fw_latest_version;
	fw_latest_version = tsp_version_disp;
	pr_info("Atmel Last firmware version is %d\n", fw_latest_version);
	return sprintf(buf, "%#02x\n", fw_latest_version);
}

static ssize_t set_mxt_firm_version_read_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%#02x\n", tsp_version_disp);

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
	u16 obj_address = 0;
	u16 size_one;
	int ret;
	u8 value;
	int jump_limit = 0;
	int mrgthr = 0;
	u8 val = 0;
	unsigned int register_address = 0;

	if (!mxt224_enabled) {
		printk(KERN_ERR "[TSP ]set_tsp_for_inputmethod_store. mxt224_enabled is 0\n");
		return 1;
	}

	mutex_lock(&lock);
	if (*buf == '1' && (!is_inputmethod)) {
		is_inputmethod = 1;
		jump_limit = 5;
		mrgthr = 5;
		printk(KERN_ERR "[TSP] Set TSP inputmethod IN\n");

		t48_jump_limit = jump_limit;   
		t48_mrgthr = mrgthr;


		ret = get_object_info(copy_data, TOUCH_MULTITOUCHSCREEN_T9, &size_one, &obj_address);
		register_address = 16;		
		value = (u8)mrgthr;
		size_one = 1;
		write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
		read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
		printk(KERN_ERR "T%d Byte%d is %d\n", 9, register_address, val);

		register_address = 30;				
		value = (u8)jump_limit;
		size_one = 1;
		write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
		read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
		printk(KERN_ERR "T%d Byte%d is %d\n", 9, register_address, val);

		//if (ta_status)   /* T48 Config change for TA connection */
		{
			value = jump_limit;
			register_address=51;
			ret = get_object_info(copy_data, PROCG_NOISESUPPRESSION_T48, &size_one, &obj_address);
			size_one = 1;
			write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
			read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
			printk(KERN_ERR"[TSP] MXT224E T%d Byte%d is %d\n",48,register_address,val);
			
			value = mrgthr;
			register_address=42;
			write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
			read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
			printk(KERN_ERR"[TSP] MXT224E T%d Byte%d is %d\n",48,register_address,val);
		}
	} else if (*buf == '0' && (is_inputmethod)) {
		is_inputmethod = 0;
		jump_limit  = 18;
		mrgthr = 40;
		printk(KERN_ERR "[TSP] Set TSP inputmethod OUT\n");

		ret = get_object_info(copy_data, TOUCH_MULTITOUCHSCREEN_T9, &size_one, &obj_address);
		register_address = 16;		
		value = (u8)mrgthr;
		size_one = 1;
		write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
		read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
		printk(KERN_ERR "T%d Byte%d is %d\n", 9, register_address, val);

		register_address = 30;				
		value = (u8)jump_limit;
		size_one = 1;
		write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
		read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
		printk(KERN_ERR "T%d Byte%d is %d\n", 9, register_address, val);

		/* T48 Config recovery for TA disconnection */
		{
			value = jump_limit;
			register_address=51;
			ret = get_object_info(copy_data, PROCG_NOISESUPPRESSION_T48, &size_one, &obj_address);
			size_one = 1;
			write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
			read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
			printk(KERN_ERR"[TSP] MXT224E T%d Byte%d is %d\n",48,register_address,val);
			
			value = mrgthr;
			register_address=42;
			write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
			read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
			printk(KERN_ERR"[TSP] MXT224E T%d Byte%d is %d\n",48,register_address,val);
	}

	}
	mutex_unlock(&lock);


	return 1;
}

static ssize_t mxt224_call_release_touch(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk(" %s is called\n", __func__);
	mutex_lock(&lock);
	TSP_forced_release_for_call();
	mutex_unlock(&lock);
	return sprintf(buf,"0\n");
}
static ssize_t mxt_touchtype_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char temp[15];
	if(mxt_version_disp == 0x80) {
		sprintf(temp, "TSP : MXT224\n");
		strcat(buf, temp);
	} else if(mxt_version_disp == 0x81) {
		sprintf(temp, "TSP : MXT224E\n");
		strcat(buf, temp);
	} else {
		sprintf(temp, "error\n");
		strcat(buf, temp);
		dev_info(dev, "read mxt TSP type read failed. \n");
	}
	return strlen(buf);
}

#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)	
static ssize_t mxt_reg_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_info("Atmel Last register version is %s\n", MXT_REG_VERSION);
	return sprintf(buf, "%s\n", MXT_REG_VERSION);
}
#endif
static DEVICE_ATTR(set_module_off, S_IRUGO | S_IWUSR | S_IWGRP, set_tsp_module_off_show, NULL);
static DEVICE_ATTR(set_module_on, S_IRUGO | S_IWUSR | S_IWGRP, set_tsp_module_on_show, NULL);
static DEVICE_ATTR(set_refer0, S_IRUGO | S_IWUSR | S_IWGRP, set_refer0_mode_show, NULL);
static DEVICE_ATTR(set_delta0, S_IRUGO | S_IWUSR | S_IWGRP, set_delta0_mode_show, NULL);
static DEVICE_ATTR(set_refer1, S_IRUGO | S_IWUSR | S_IWGRP, set_refer1_mode_show, NULL);
static DEVICE_ATTR(set_delta1, S_IRUGO | S_IWUSR | S_IWGRP, set_delta1_mode_show, NULL);
static DEVICE_ATTR(set_refer2, S_IRUGO | S_IWUSR | S_IWGRP, set_refer2_mode_show, NULL);
static DEVICE_ATTR(set_delta2, S_IRUGO | S_IWUSR | S_IWGRP, set_delta2_mode_show, NULL);
static DEVICE_ATTR(set_refer3, S_IRUGO | S_IWUSR | S_IWGRP, set_refer3_mode_show, NULL);
static DEVICE_ATTR(set_delta3, S_IRUGO | S_IWUSR | S_IWGRP, set_delta3_mode_show, NULL);
static DEVICE_ATTR(set_refer4, S_IRUGO | S_IWUSR | S_IWGRP, set_refer4_mode_show, NULL);
static DEVICE_ATTR(set_delta4, S_IRUGO | S_IWUSR | S_IWGRP, set_delta4_mode_show, NULL);
static DEVICE_ATTR(set_all_refer, S_IRUGO | S_IWUSR | S_IWGRP, set_all_refer_mode_show, NULL);
static DEVICE_ATTR(disp_all_refdata, S_IRUGO | S_IWUSR | S_IWGRP, disp_all_refdata_show, disp_all_refdata_store);
static DEVICE_ATTR(set_all_delta, S_IRUGO | S_IWUSR | S_IWGRP, set_all_delta_mode_show, NULL);
static DEVICE_ATTR(disp_all_deltadata, S_IRUGO | S_IWUSR | S_IWGRP, disp_all_deltadata_show, disp_all_deltadata_store);
static DEVICE_ATTR(set_threshould, S_IRUGO | S_IWUSR | S_IWGRP, set_threshold_mode_show, NULL);
static DEVICE_ATTR(set_firm_version, S_IRUGO | S_IWUSR | S_IWGRP, set_firm_version_show, NULL);
/*
	20110222 N1 firmware sync
*/
static DEVICE_ATTR(tsp_firm_update, S_IRUGO | S_IWUSR | S_IWGRP, set_mxt_update_show, NULL);		/* firmware update */
static DEVICE_ATTR(tsp_firm_update_status, S_IRUGO | S_IWUSR | S_IWGRP, set_mxt_firm_status_show, NULL);	/* firmware update status return */
static DEVICE_ATTR(tsp_threshold, S_IRUGO | S_IWUSR | S_IWGRP, key_threshold_show, key_threshold_store);	/* touch threshold return, store */
static DEVICE_ATTR(tsp_firm_version_phone, S_IRUGO | S_IWUSR | S_IWGRP, set_mxt_firm_version_show, NULL);/* PHONE*/	/* firmware version resturn in phone driver version */
static DEVICE_ATTR(tsp_firm_version_panel, S_IRUGO | S_IWUSR | S_IWGRP, set_mxt_firm_version_read_show, NULL);/*PART*/	/* firmware version resturn in TSP panel version */
#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)	
static DEVICE_ATTR(tsp_reg_version, S_IRUGO, mxt_reg_version_show, NULL);/*REGISTER*/
#endif
static DEVICE_ATTR(set_tsp_for_inputmethod, S_IRUGO | S_IWUSR | S_IWGRP, set_tsp_for_inputmethod_show, set_tsp_for_inputmethod_store); /* For 3x4 Input Method, Jump limit changed API */
static DEVICE_ATTR(call_release_touch, S_IRUGO | S_IWUSR | S_IWGRP, mxt224_call_release_touch, NULL);
static DEVICE_ATTR(mxt_touchtype, S_IRUGO | S_IWUSR | S_IWGRP,	mxt_touchtype_show, NULL);

static DEVICE_ATTR(object_show, S_IRUGO | S_IWUSR | S_IWGRP, NULL, qt602240_object_show);
static DEVICE_ATTR(object_write, S_IRUGO | S_IWUSR | S_IWGRP, NULL, qt602240_object_setting);
//static DEVICE_ATTR(dbg_switch, S_IRUGO | S_IWUSR | S_IWGRP, NULL, mxt224_debug_setting);


static struct attribute *qt602240_attrs[] = {
	&dev_attr_object_show.attr,
	&dev_attr_object_write.attr,
/*	&dev_attr_dbg_switch,  Xtopher Imsi blocked */
	NULL
};

static const struct attribute_group qt602240_attr_group = {
	.attrs = qt602240_attrs,
};

#ifdef CONFIG_TOUCHSCREEN_MXT768E
#ifdef QT_FIRMUP_ENABLE
#define I2C_M_WR 0 /* for i2c */
#define I2C_MAX_SEND_LENGTH     300

int boot_qt602240_i2c_write(struct mxt224_data *data, u16 reg, u8 *read_val, unsigned int len)
{
	struct i2c_msg wmsg;
	/* unsigned char wbuf[3]; */
	unsigned char data_buf[I2C_MAX_SEND_LENGTH];
	int ret, i;

	if (len+2 > I2C_MAX_SEND_LENGTH) {
		printk(KERN_ERR "[TSP]  %s() data length error\n", __func__);
		return -ENODEV;
	}

	wmsg.addr = 0x26;
	wmsg.flags = I2C_M_WR;
	wmsg.len = len;
	wmsg.buf = data_buf;


	for (i = 0; i < len; i++)
		data_buf[i] = *(read_val+i);

	/* printk("%s, %s\n",__func__,wbuf); */
	ret = i2c_transfer(data->client->adapter, &wmsg, 1);
	return ret;
}



int boot_write_mem(struct mxt224_data *data, u16 start, u16 size, u8 *mem)
{
	int ret;

	ret = boot_qt602240_i2c_write(data, start, mem, size);

	if (ret < 0) {
		printk(KERN_ERR "[TSP] boot write mem fail: %d\n", ret);
		return ret;
	} else {
		return ret;
	}
}
int boot_read_mem(struct mxt224_data *data, u16 start, u8 size, u8 *mem)
{
	struct i2c_msg rmsg;
	int ret;

	rmsg.addr = 0x26;
	rmsg.flags = I2C_M_RD;
	rmsg.len = size;
	rmsg.buf = mem;
	ret = i2c_transfer(data->client->adapter, &rmsg, 1);

	return ret;
}
int boot_unlock(struct mxt224_data *data)
{

	int ret;
	unsigned char data_buf[2];

	/* read_buf = (char *)kmalloc(size, GFP_KERNEL | GFP_ATOMIC); */
	data_buf[0] = 0xDC;
	data_buf[1] = 0xAA;

	ret = boot_qt602240_i2c_write(data, 0, data_buf, 2);
	if (ret < 0) {
		printk(KERN_ERR "[TSP] %s : i2c write failed\n", __func__);
		return WRITE_MEM_FAILED;
	}

	return WRITE_MEM_OK;

}
int QT_Boot_no_reset(struct mxt224_data *data)
{
	unsigned char boot_status;
	unsigned char boot_ver;
	unsigned char retry_cnt;
	unsigned short character_position;
	unsigned short frame_size = 0;
	unsigned short next_frame;
	unsigned short crc_error_count;
	unsigned char size1, size2;
	unsigned short j, read_status_flag;
	/* uint8_t data = 0xA5; */

	unsigned char  *firmware_data ;

	firmware_data = MXT768_firmware;

	crc_error_count = 0;
	character_position = 0;
	next_frame = 0;

		for (retry_cnt = 0; retry_cnt < 30; retry_cnt++) {

			if ((boot_read_mem(data, 0, 1, &boot_status) == READ_MEM_OK) && (boot_status & 0xC0) == 0xC0) {
				boot_ver = boot_status & 0x3F;
				crc_error_count = 0;
				character_position = 0;
				next_frame = 0;

				while (1) {
					for (j = 0; j < 5; j++) {
						mdelay(60);
						if (boot_read_mem(data, 0, 1, &boot_status) == READ_MEM_OK) {
							read_status_flag = 1;
							break;
						} else
							read_status_flag = 0;
					}

					if (read_status_flag == 1) {
						retry_cnt  = 0;
						printk(KERN_ERR "[TSP] TSP boot status is %x stage 2\n", boot_status);
						if ((boot_status & QT_WAITING_BOOTLOAD_COMMAND) == QT_WAITING_BOOTLOAD_COMMAND) {
							if (boot_unlock(data) == WRITE_MEM_OK) {
								mdelay(10);
								printk(KERN_ERR "[TSP] Unlock OK\n");
							} else {
								printk(KERN_ERR "[TSP] Unlock fail\n");
							}
						} else if ((boot_status & 0xC0) == QT_WAITING_FRAME_DATA) {
							/* Add 2 to frame size, as the CRC bytes are not included */
							size1 =  *(firmware_data+character_position);
							size2 =  *(firmware_data+character_position+1)+2;
							frame_size = (size1<<8) + size2;

							printk(KERN_ERR "[TSP] Frame size:%d\n", frame_size);
							printk(KERN_ERR "[TSP] Firmware pos:%d\n", character_position);
							/* Exit if frame data size is zero */
							if (0 == frame_size) {
								if (QT_i2c_address == I2C_BOOT_ADDR_0)
									QT_i2c_address = I2C_APPL_ADDR_0;

								printk(KERN_ERR "[TSP] 0 == frame_size\n");
								return 1;
							}
							next_frame = 1;
							boot_write_mem(data, 0, frame_size, (firmware_data + character_position));
							mdelay(10);
							printk(KERN_ERR "[TSP] .");

						} else if (boot_status == QT_FRAME_CRC_CHECK) {
							printk(KERN_ERR "[TSP] CRC Check\n");
						} else if (boot_status == QT_FRAME_CRC_PASS) {
							if (next_frame == 1) {
								printk(KERN_ERR "[TSP] CRC Ok\n");
								character_position += frame_size;
								next_frame = 0;
							} else {
								printk(KERN_ERR "[TSP] next_frame != 1\n");
							}
						} else if (boot_status  == QT_FRAME_CRC_FAIL) {
							printk(KERN_ERR "[TSP] CRC Fail\n");
							crc_error_count++;
						} if (crc_error_count > 10) { //tyoony ?????
							return QT_FRAME_CRC_FAIL;
						}

					} else {
						return 0;
					}
				}
			} else {
				printk(KERN_ERR "[TSP] read_boot_state() or (boot_status & 0xC0) == 0xC0) is fail!!!\n");
			}
		}

		printk(KERN_ERR "[TSP] QT_Boot_no_reset end\n");
		return 0;

}

void QT_reprogram(struct mxt224_data *data)
{
	uint8_t version, build;
	unsigned char rxdata;


	printk(KERN_ERR "[TSP] QT_reprogram check\n");

	if (boot_read_mem(data, 0, 1, &rxdata) == READ_MEM_OK) {
		printk(KERN_ERR "[TSP] Enter to new firmware : boot mode\n");
			if (!QT_Boot_no_reset(data)) {
				data->power_off();
				mdelay(300);
				data->power_on();
			}

			printk(KERN_ERR "[TSP] Reprogram done : boot mode\n");
	}
#if 0
	quantum_touch_probe();  /* find and initialise QT device */

	get_version(&version);
	get_build_number(&build);

	if ((version != 0x14) && (version < 0x16) || ((version == 0x16) && (build == 0xAA))) {
		printk(KERN_ERR "[TSP] Enter to new firmware : ADDR = Other Version\n");
		if (!QT_Boot()) {
			TSP_Restart();
			quantum_touch_probe();
		}
		printk(KERN_ERR "[TSP] Reprogram done : ADDR = Other Version\n");
	}
#endif
}
#endif
#endif




void mxt224_orient_hw_barnch(struct mxt224_data *data)
{
#if defined (CONFIG_KOR_MODEL_SHV_E110S)
	if (get_hw_rev() ==0x01 ){
		if( data->family_id == 0x81 )
		{
			data->orient_barnch(MXT224_ORIENT_SWAP_NN);
		}
		else 
		{
		data->orient_barnch(MXT224_ORIENT_SWAP_XY);
		}
		
	}else{			
		data->orient_barnch(MXT224_ORIENT_SWAP_NN);
	}
#elif defined (CONFIG_JPN_MODEL_SC_03D)
		data->orient_barnch(MXT224_ORIENT_SWAP_NN);
#elif defined (CONFIG_USA_MODEL_SGH_I727) ||defined (CONFIG_USA_MODEL_SGH_T989)
		data->orient_barnch(MXT224_ORIENT_SWAP_NN);
#elif defined (CONFIG_KOR_MODEL_SHV_E120L)
	#if defined(CONFIG_KOR_SHV_E120L_WXGA)
		data->orient_barnch(MXT224_ORIENT_SWAP_NN);
    #else
		data->orient_barnch(MXT224_ORIENT_SWAP_XY);
    #endif
#endif
}

static int tsp_probe_debug_value = 0;

static int __devinit mxt224_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct mxt224_platform_data *pdata = client->dev.platform_data;
	struct mxt224_data *data;
	struct input_dev *input_dev;
	int ret;

	int i;
	bool ta_status;
	u8 **tsp_config;
	u16 size_one;
	u8 user_info_value;
	u16 obj_address = 0;


tsp_reinit:;


//	if( 0==sec_debug_is_enabled() )
//		g_debug_switch = 0;

	touch_is_pressed = 0;
	printk(KERN_ERR "[TSP] mxt224_probe   \n");

	if (!pdata) {
		dev_err(&client->dev, "missing platform data\n");
		tsp_probe_debug_value = -1;
		return -ENODEV;
	}

	if (pdata->max_finger_touches <= 0)
	{
		tsp_probe_debug_value = -2;
		return -EINVAL;
	}

	data = kzalloc(sizeof(*data) + pdata->max_finger_touches *
					sizeof(*data->fingers), GFP_KERNEL);
	if (!data)
		{
		tsp_probe_debug_value = -3;
		return -ENOMEM;
		}

	data->num_fingers = pdata->max_finger_touches;
	data->power_on = pdata->power_on;
	data->power_off = pdata->power_off;
	data->register_cb = pdata->register_cb;
	data->read_ta_status = pdata->read_ta_status;
	data->orient_barnch = pdata->orient_barnch;




	data->client = client;
	i2c_set_clientdata(client, data);

	input_dev = input_allocate_device();
	if (!input_dev) {
		ret = -ENOMEM;
		dev_err(&client->dev, "input device allocation failed\n");
		goto err_alloc_dev;
	}
	data->input_dev = input_dev;
	input_set_drvdata(input_dev, data);
	input_dev->name = "sec_touchscreen";

	set_bit(EV_ABS, input_dev->evbit);
#if defined(TOUCH_NON_SLOT)
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->min_x,
			pdata->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->min_y,
			pdata->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, pdata->min_z,
			pdata->max_z, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, pdata->min_w,
			pdata->max_w, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0,
			data->num_fingers - 1, 0, 0);
#else // SLOT
	set_bit(MT_TOOL_FINGER, input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_mt_init_slots(input_dev, data->num_fingers);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->min_x,
			pdata->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->min_y,
			pdata->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, pdata->min_w,
			pdata->max_w, 0, 0);
	//input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, pdata->min_w,
	//		pdata->max_w, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, pdata->min_z,
			 pdata->max_z, 0, 0);

	//	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0,
	//			data->num_fingers - 1, 0, 0);
#endif

#ifdef _SUPPORT_SHAPE_TOUCH_
	input_set_abs_params(input_dev, ABS_MT_COMPONENT, 0, 255, 0, 0);
#endif
	ret = input_register_device(input_dev);
	if (ret) {
		input_free_device(input_dev);
		goto err_reg_dev;
	}

	data->gpio_read_done = pdata->gpio_read_done;

	data->power_on();

	mdelay(40);

#ifdef CONFIG_TOUCHSCREEN_MXT768E
#ifdef QT_FIRMUP_ENABLE
{
	QT_i2c_address = 0x26;
	QT_reprogram(data);
}
#endif
#endif

	ret = mxt224_init_touch_driver(data);

	data->register_cb(mxt224_ta_probe);

    data->noise_suppression_cfg_ta = pdata->t48_ta_cfg + 1;

	copy_data = data;

	if (ret) {
		dev_err(&client->dev, "chip initialization failed\n");
		goto err_init_drv;
	}

	if (data->family_id == 0x80) {	/*  : MXT-224 */
		tsp_config = (u8 **)pdata->config;
		printk(KERN_ERR "[TSP] TSP chip is MXT224\n");
	} else if (data->family_id == 0x81)  {	/* tsp_family_id - 0x81 : MXT-224E */
		tsp_config = (u8 **)pdata->config_e;
		printk(KERN_ERR "[TSP] TSP chip is MXT224-E\n");
		atchcalst = 5;/*8*/
		atchcalsthr = 35;/*8*/
		get_object_info(data, SPT_USERDATA_T38, &size_one, &obj_address);
		size_one = 1;
		read_mem(data, obj_address, (u8)size_one, &user_info_value);
		printk(KERN_ERR"[TSP]user_info_value is %d\n",user_info_value);
	} else  {
		printk(KERN_ERR "[TSP] ERROR : There is no valid TSP ID\n");
		goto err_config;
	}


	mxt224_orient_hw_barnch(data);

	if ((data->family_id == 0x81) && (user_info_value == 165)) { /*mxt224's user info =165 -> don't write config*/
		for (i = 0; tsp_config[i][0] != RESERVED_T255; i++) {
			if (tsp_config[i][0] == GEN_POWERCONFIG_T7)
				data->power_cfg = tsp_config[i] + 1;

			if (tsp_config[i][0] == TOUCH_MULTITOUCHSCREEN_T9) {
			/* Are x and y inverted? */
				if (tsp_config[i][10] & 0x1) {
					data->x_dropbits =
						(!(tsp_config[i][22] & 0xC)) << 1;
					data->y_dropbits =
						(!(tsp_config[i][20] & 0xC)) << 1;
				} else {
					data->x_dropbits =
						(!(tsp_config[i][20] & 0xC)) << 1;
					data->y_dropbits =
						(!(tsp_config[i][22] & 0xC)) << 1;
				}
			}
			if (tsp_config[i][0] == PROCG_NOISESUPPRESSION_T48)
				data->noise_suppression_cfg = tsp_config[i] + 1;			
		}
	} else {
		for (i = 0; tsp_config[i][0] != RESERVED_T255; i++) {
			ret = write_config(data, tsp_config[i][0],
							tsp_config[i] + 1);
			if (ret)
			{
				ret = write_config(data, tsp_config[i][0],
							tsp_config[i] + 1);
				if (ret)
				goto err_config;
			}

			if (tsp_config[i][0] == GEN_POWERCONFIG_T7)
				data->power_cfg = tsp_config[i] + 1;

			if (tsp_config[i][0] == TOUCH_MULTITOUCHSCREEN_T9) {
				/* Are x and y inverted? */
				if (tsp_config[i][10] & 0x1) {
					data->x_dropbits = (!(tsp_config[i][22] & 0xC)) << 1;
					data->y_dropbits = (!(tsp_config[i][20] & 0xC)) << 1;
				} else {
					data->x_dropbits = (!(tsp_config[i][20] & 0xC)) << 1;
					data->y_dropbits = (!(tsp_config[i][22] & 0xC)) << 1;
				}
			}
			if (tsp_config[i][0] == PROCG_NOISESUPPRESSION_T48)
				data->noise_suppression_cfg = tsp_config[i] + 1;			
		}
	}

	ret = mxt224_backup(data);
	if (ret)
		goto err_backup;

	/* reset the touch IC. */
	ret = mxt224_reset(data);
	if (ret)
		goto err_reset;

	msleep(60);
	mxt224_enabled = 1;
	if (data->read_ta_status) {
		data->read_ta_status(&ta_status);
		printk(KERN_ERR "[TSP] ta_status is %d", ta_status);
		mxt224_ta_probe(ta_status);
	}
	
	calibrate_chip();

	for (i = 0; i < data->num_fingers; i++)
		data->fingers[i].z = -1;
	
	ret = request_threaded_irq(client->irq, NULL, mxt224_irq_thread,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, "mxt224_ts", data);

	if (ret < 0)
		goto err_irq;

	ret = sysfs_create_group(&client->dev.kobj, &qt602240_attr_group);
	if (ret)
		printk(KERN_ERR "[TSP] sysfs_create_group()is falled\n");
/*
	20110222 N1_firmware_sync
*/

	sec_touchscreen = device_create(sec_class, NULL, 0, NULL, "sec_touchscreen");

	if (IS_ERR(sec_touchscreen))
		printk(KERN_ERR "[TSP] Failed to create device(sec_touchscreen)!\n");

	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_update) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_tsp_firm_update.attr.name);

	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_update_status) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_tsp_firm_update_status.attr.name);

	if (device_create_file(sec_touchscreen, &dev_attr_tsp_threshold) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_tsp_threshold.attr.name);

	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_version_phone) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_tsp_firm_version_phone.attr.name);

	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_version_panel) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_tsp_firm_version_panel.attr.name);
#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)
	if (device_create_file(sec_touchscreen, &dev_attr_tsp_reg_version) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_tsp_reg_version.attr.name);
#endif

	if (device_create_file(sec_touchscreen, &dev_attr_set_tsp_for_inputmethod) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_tsp_for_inputmethod.attr.name);
	if (device_create_file(sec_touchscreen, &dev_attr_call_release_touch) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_call_release_touch.attr.name);
if (device_create_file(sec_touchscreen, &dev_attr_mxt_touchtype) < 0)
	printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_mxt_touchtype.attr.name);

/*
	end N1_firmware_sync
*/
	qt602240_noise_test = device_create(sec_class, NULL, 0, NULL, "qt602240_noise_test");

	if (IS_ERR(qt602240_noise_test))
		printk(KERN_ERR "Failed to create device(qt602240_noise_test)!\n");

	if (device_create_file(qt602240_noise_test, &dev_attr_set_module_off) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_module_off.attr.name);

	if (device_create_file(qt602240_noise_test, &dev_attr_set_module_on) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_module_on.attr.name);


	if (device_create_file(qt602240_noise_test, &dev_attr_set_refer0) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_refer0.attr.name);

	if (device_create_file(qt602240_noise_test, &dev_attr_set_delta0) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_delta0.attr.name);

	if (device_create_file(qt602240_noise_test, &dev_attr_set_refer1) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_refer1.attr.name);

	if (device_create_file(qt602240_noise_test, &dev_attr_set_delta1) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_delta1.attr.name);

	if (device_create_file(qt602240_noise_test, &dev_attr_set_refer2) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_refer2.attr.name);

	if (device_create_file(qt602240_noise_test, &dev_attr_set_delta2) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_delta2.attr.name);

	if (device_create_file(qt602240_noise_test, &dev_attr_set_refer3) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_refer3.attr.name);

	if (device_create_file(qt602240_noise_test, &dev_attr_set_delta3) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_delta3.attr.name);

	if (device_create_file(qt602240_noise_test, &dev_attr_set_refer4) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_refer4.attr.name);

	if (device_create_file(qt602240_noise_test, &dev_attr_set_delta4) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_delta4.attr.name);

	if (device_create_file(qt602240_noise_test, &dev_attr_set_all_refer) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_all_refer.attr.name);
		if (device_create_file(qt602240_noise_test, &dev_attr_disp_all_refdata)< 0)
			printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_disp_all_refdata.attr.name);		

	if (device_create_file(qt602240_noise_test, &dev_attr_set_all_delta) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_all_delta.attr.name);

	if (device_create_file(qt602240_noise_test, &dev_attr_disp_all_deltadata)< 0)
			printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_disp_all_deltadata.attr.name);		


	if (device_create_file(qt602240_noise_test, &dev_attr_set_threshould) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_threshould.attr.name);

	if (device_create_file(qt602240_noise_test, &dev_attr_set_firm_version) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_firm_version.attr.name);

#ifdef CONFIG_HAS_EARLYSUSPEND
#if defined (CONFIG_KOR_MODEL_SHV_E110S) || defined (CONFIG_JPN_MODEL_SC_03D) ||  defined (CONFIG_TARGET_LOCALE_USA)
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB+3;
#else
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
#endif

	data->early_suspend.suspend = mxt224_early_suspend;
	data->early_suspend.resume = mxt224_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

#ifdef CLEAR_MEDIAN_FILTER_ERROR
#if defined (CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R) || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_T989)	
	noise_median.median_on_flag = false;
	noise_median.mferr_setting = false;
	noise_median.mferr_count = 0;
	noise_median.t46_actvsyncsperx_for_mferr = 48;//20120225
	noise_median.t48_mfinvlddiffthr_for_mferr = 20;//20120225
	noise_median.t48_mferrorthr_for_mferr = 22;//20120225
	noise_median.t48_thr_for_mferr = 45;//20120225
	noise_median.t48_movfilter_for_mferr = 81;//20120225
#else	
	noise_median.mferr_setting = false;
	noise_median.mferr_count = 0;
	noise_median.t46_actvsyncsperx_for_mferr = 63;
	noise_median.t48_mfinvlddiffthr_for_mferr = 20;
	noise_median.t48_mferrorthr_for_mferr = 22;
	noise_median.t48_thr_for_mferr = 45;
	noise_median.t48_movfilter_for_mferr = 0;
#endif
#endif
	
	printk(KERN_ERR "[TSP] mxt224_probe   e\n");

	return 0;

err_irq:
	tsp_probe_debug_value = -4;
err_reset:
	if(tsp_probe_debug_value ==0)
		tsp_probe_debug_value = -5;
err_backup:
	if(tsp_probe_debug_value ==0)
		tsp_probe_debug_value = -6;
err_config:
	if(tsp_probe_debug_value ==0)
		tsp_probe_debug_value = -7;
	kfree(data->objects);
err_init_drv:
	if(tsp_probe_debug_value ==0)
		tsp_probe_debug_value = -8;
	gpio_free(data->gpio_read_done);
	/* err_gpio_req: */
	data->power_off(); 
	input_unregister_device(input_dev);
err_reg_dev:
	if(tsp_probe_debug_value ==0)
		tsp_probe_debug_value = -9;
err_alloc_dev:
	if(tsp_probe_debug_value ==0)
		tsp_probe_debug_value = -10;
	kfree(data);

	if(	tsp_reboot_count < 3 )
	{
		tsp_reboot_count++;
		mdelay(20);
		printk(KERN_ERR "[TSP] retry probe %d\n",tsp_reboot_count);
		goto tsp_reinit;
	}
	
	return ret;
}

static int __devexit mxt224_remove(struct i2c_client *client)
{
	struct mxt224_data *data = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	free_irq(client->irq, data);
	kfree(data->objects);
	gpio_free(data->gpio_read_done);
	data->power_off();
	input_unregister_device(data->input_dev);
	kfree(data);

	printk(KERN_ERR "[TSP] mxt224_remove \n");
	dump_stack();

	return 0;
}

static struct i2c_device_id mxt224_idtable[] = {
	{MXT224_DEV_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, mxt224_idtable);

static const struct dev_pm_ops mxt224_pm_ops = {
	.suspend = mxt224_suspend,
	.resume = mxt224_resume,
};

static struct i2c_driver mxt224_i2c_driver = {
	.id_table = mxt224_idtable,
	.probe = mxt224_probe,
	.remove = __devexit_p(mxt224_remove),
	.driver = {
		.owner	= THIS_MODULE,
		.name	= MXT224_DEV_NAME,
		.pm	= &mxt224_pm_ops,
	},
};

#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

static int __init mxt224_init(void)
{
#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return ENODEV!\n", __func__);
		return 0;
	}
#endif

	return i2c_add_driver(&mxt224_i2c_driver);
}

static void __exit mxt224_exit(void)
{
	i2c_del_driver(&mxt224_i2c_driver);
}
module_init(mxt224_init);
module_exit(mxt224_exit);

MODULE_DESCRIPTION("Atmel MaXTouch 224 driver");
MODULE_LICENSE("GPL");
