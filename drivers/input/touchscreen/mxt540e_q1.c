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
#include <linux/i2c/mxt540e_q1.h>
#include <asm/unaligned.h>
#include <linux/firmware.h>

#if defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_KOR_MODEL_SHV_E160L)  || defined (CONFIG_JPN_MODEL_SC_05D)
#include "mXT540e__APP_V1-3-AA_.h"
#else
#include "mXT540e__APP_V1-2-AA_.h"
#endif


#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_KOR_MODEL_SHV_E160L) ||defined(CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_JPN_MODEL_SC_05D)
#include <linux/input/mt.h>
#endif

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
#define VECTOR_MSG_MASK			0x08
#define AMP_MSG_MASK			0x04
#define SUPPRESS_MSG_MASK		0x02
#define UNGRIP_MSG_MASK			0x01

/* Version */
#define MXT540E_VER_10			0x10

/* Slave addresses */
#define MXT540E_APP_LOW		0x4C
#define MXT540E_APP_HIGH		0x4D
#define MXT540E_BOOT_LOW		0x26
#define MXT540E_BOOT_HIGH		0x27

/* FIRMWARE NAME */
#define MXT540E_FW_NAME         "mXT540E.fw"
#define MXT540E_BOOT_VALUE      0xa5
#define MXT540E_BACKUP_VALUE    0x55

/* Bootloader mode status */
#define MXT540E_WAITING_BOOTLOAD_CMD    0xc0	/* valid 7 6 bit only */
#define MXT540E_WAITING_FRAME_DATA      0x80	/* valid 7 6 bit only */
#define MXT540E_FRAME_CRC_CHECK         0x02
#define MXT540E_FRAME_CRC_FAIL          0x03
#define MXT540E_FRAME_CRC_PASS          0x04
#define MXT540E_APP_CRC_FAIL            0x40	/* valid 7 8 bit only */
#define MXT540E_BOOT_STATUS_MASK        0x3f

/* Command to unlock bootloader */
#define MXT540E_UNLOCK_CMD_MSB		0xaa
#define MXT540E_UNLOCK_CMD_LSB		0xdc

#define ID_BLOCK_SIZE			7

#define DRIVER_FILTER

#define MXT540E_STATE_INACTIVE		-1
#define MXT540E_STATE_RELEASE		0
#define MXT540E_STATE_PRESS		1
#define MXT540E_STATE_MOVE		2

#define MAX_USING_FINGER_NUM 10

#define CLEAR_MEDIAN_FILTER_ERROR


/* Cut out ghost ... Xtopher */
#define MAX_GHOSTCHECK_FINGER 		10
#define MAX_GHOSTTOUCH_COUNT		400		// 4s, 125Hz
#define MAX_COUNT_TOUCHSYSREBOOT	4
#define MAX_GHOSTTOUCH_BY_PATTERNTRACKING		3
#define PATTERN_TRACKING_DISTANCE 3


#undef ITDEV //hmink 
#ifdef ITDEV
static int driver_paused;//itdev
static int debug_enabled;//itdev
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
	s8 state;
	int16_t component;
};

typedef struct
{
    u8 object_type;     /*!< Object type. */
    u8 instance;        /*!< Instance number. */
} report_id_map_t;

u8 max_report_id;
report_id_map_t *rid_map;
static bool rid_map_alloc;


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

struct mxt540e_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct early_suspend early_suspend;
	u8 family_id;
	u32 finger_mask;
	int gpio_read_done;
	struct object_t *objects;
	u8 objects_len;
	u8 tsp_version;
	u8 tsp_build;
	const u8 *power_cfg;
	u8 finger_type;
	u16 msg_proc;
	u16 cmd_proc;
	u16 msg_object_size;
	u32 x_dropbits:2;
	u32 y_dropbits:2;
	u8 chrgtime_batt;
	u8 chrgtime_charging;
	u8 atchcalst;
	u8 atchcalsthr;
	u8 tchthr_batt;
	u8 tchthr_charging;
	u8 actvsyncsperx_batt;
	u8 actvsyncsperx_charging;
	u8 calcfg_batt_e;
	u8 calcfg_charging_e;
	u8 atchfrccalthr_e;
	u8 atchfrccalratio_e;
	const u8 *t48_config_batt_e;
	const u8 *t48_config_chrg_e;
	struct delayed_work  config_dwork;
	struct delayed_work  resume_check_dwork;
	struct delayed_work  cal_check_dwork;
	void (*power_on)(void);
	void (*power_off)(void);
	void (*register_cb)(void *);
	void (*read_ta_status)(void *);
	int num_fingers;
#ifdef ITDEV
	u16 last_read_addr;
	u16 msg_proc_addr;
#endif
	struct finger_info fingers[];
};

struct mxt540e_data *copy_data;
extern struct class *sec_class;
extern int sec_debug_level(void);
int touch_is_pressed;
EXPORT_SYMBOL(touch_is_pressed);
static int mxt540e_enabled;
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_KOR_MODEL_SHV_E160L)|| defined(CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_JPN_MODEL_SC_05D)
#if defined (SEC_TSP_POSITION_DEBUG_XTOPHER)
static bool g_debug_switch = true;
#else
static bool g_debug_switch = false;
#endif
#else
static bool g_debug_switch = false;
#endif
static u8 tsp_version_disp;
static u8 threshold;
static int firm_status_data;
static bool deepsleep;
#ifdef TOUCH_CPU_LOCK
static bool touch_cpu_lock_status;
#endif
static int check_resume_err;
static int check_resume_err_count;
static int check_calibrate;
static int config_dwork_flag;
int16_t sumsize;


/* Below is used for clearing ghost touch or for checking to system reboot.  by Xtopher */
static int cghost_clear = 0;  /* ghost touch clear count  by Xtopher */
static int ftouch_reboot = 0; 
static int tcount_finger[MAX_GHOSTCHECK_FINGER] = {0,0,0,0,0,0,0,0,0,0};
static int touchbx[MAX_GHOSTCHECK_FINGER] = {0,0,0,0,0,0,0,0,0,0};
static int touchby[MAX_GHOSTCHECK_FINGER] = {0,0,0,0,0,0,0,0,0,0};
static int ghosttouchcount = 0;
static int tsp_reboot_count = 0;
static int cFailbyPattenTracking = 0;
static void report_input_data(struct mxt540e_data *data);
static void Mxt540e_force_released(void);
//static void TSP_forced_release_for_call(void);
static int tsp_pattern_tracking(int fingerindex, s16 x, s16 y);
static void TSP_forced_reboot(void);
static int is_drawingmode = 0;


/* median filter test */
#define	MedianError_Max_Batt		5
#define	MedianError_Max_Ta			10
u8 Medianfilter_Err_cnt_Batt;	
u8 Medianfilter_Err_cnt_Ta;
u8 MedianFirst_Flag;
u8 Median_Error_Table_TA[4]={33,20,15,0};
u8 Median_Error_Table_Batt[4] = {20,10,30,10};
u8 table_cnt =0 ;
u8 MedianError_cnt;


#if defined(CONFIG_USA_MODEL_SGH_I717)
static unsigned int gIgnoreReport_flag;
static unsigned int gForceCalibration_flag;
#endif


static int read_mem(struct mxt540e_data *data, u16 reg, u8 len, u8 *buf)
{
	int ret;
	int retry = 5;	
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

	if (ret < 0) {
		while (retry--) {
			printk(KERN_DEBUG"[TSP] read_mem retry %d\n", retry);

			msleep(5);
			ret = i2c_transfer(data->client->adapter, msg, 2);
			if (ret == 2 || retry <= 0)
				break;
		}
	}		

	
	if (ret < 0)
		return ret;

	return ret == 2 ? 0 : -EIO;
}

static int write_mem(struct mxt540e_data *data, u16 reg, u8 len, const u8 *buf)
{
	int ret;
	int retry = 5;	
	u8 tmp[len + 2];

	put_unaligned_le16(cpu_to_le16(reg), tmp);
	memcpy(tmp + 2, buf, len);

	ret = i2c_master_send(data->client, tmp, sizeof(tmp));

	if (ret < 0) {
		while (retry--) {
			printk(KERN_DEBUG"[TSP] write_mem retry %d\n", retry);

			msleep(5);
			ret = i2c_master_send(data->client, tmp, sizeof(tmp));
			if ((ret >= 0) || (retry <= 0))
				break;
		}
	}		

	if (ret < 0)
		return ret;
	/*
	if (reg==(data->cmd_proc + CMD_CALIBRATE_OFFSET))
		printk(KERN_ERR"[TSP] write calibrate_command ret is %d, size is %d\n",ret,sizeof(tmp));
	*/

	return ret == sizeof(tmp) ? 0 : -EIO;
}

static int __devinit mxt540e_reset(struct mxt540e_data *data)
{
	u8 buf = 1u;
	return write_mem(data, data->cmd_proc + CMD_RESET_OFFSET, 1, &buf);
}

static int __devinit mxt540e_backup(struct mxt540e_data *data)
{
	u8 buf = 0x55u;
	return write_mem(data, data->cmd_proc + CMD_BACKUP_OFFSET, 1, &buf);
}

static int get_object_info(struct mxt540e_data *data, u8 object_type, u16 *size, u16 *address)
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

static int write_config(struct mxt540e_data *data, u8 type, const u8 *cfg)
{
	int ret;
	u16 address = 0;
	u16 size = 0;

	ret = get_object_info(data, type, &size, &address);

	if(size ==0 && address == 0) return 0;
	else return write_mem(data, address, size, cfg);
}

static int check_instance(struct mxt540e_data *data, u8 object_type)
{
	int i;

	for (i = 0; i < data->objects_len; i++) {
		if (data->objects[i].object_type == object_type) {
			return (data->objects[i].instances);
		}
	}
	return 0;
}

static int init_write_config(struct mxt540e_data *data, u8 type, const u8 *cfg)
{
	int ret;
	u16 address = 0;
	u16 size = 0;
	u8 *temp;

	ret = get_object_info(data, type, &size, &address);

	if((size == 0) || (address == 0)) return 0;

	ret = write_mem(data, address, size, cfg);
	if (check_instance(data, type)) {
		printk(KERN_DEBUG"[TSP] exist instance1 object (%d)\n", type);
		temp = kmalloc(size * sizeof(u8), GFP_KERNEL);
		memset(temp, 0, size);
		ret |= write_mem(data, address+size, size, temp);
		if (ret < 0)
			printk(KERN_ERR "[TSP] %s, %d Error!!\n", __func__, __LINE__);
		kfree(temp);
	}
	return ret;
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

static int __devinit calculate_infoblock_crc(struct mxt540e_data *data,
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

/* mxt540e reconfigration */
static void mxt_reconfigration_normal(struct work_struct *work)
{
	int error, id;
	u16 size;
	
	struct mxt540e_data *data = container_of(work, struct mxt540e_data, config_dwork.work);
	u16 obj_address = 0;
	if (mxt540e_enabled) {
		disable_irq(data->client->irq);

		for (id = 0 ; id < MAX_USING_FINGER_NUM ; ++id) {
			if ( data->fingers[id].state == MXT540E_STATE_INACTIVE )
				continue;
			schedule_delayed_work(&data->config_dwork, HZ*5);
			printk("[TSP] touch pressed!! %s didn't execute!!\n", __func__);
			enable_irq(data->client->irq);
			return;
		}

		get_object_info(data, GEN_ACQUISITIONCONFIG_T8, &size, &obj_address);
		error = write_mem(data, obj_address+8, 1, &data->atchfrccalthr_e);
		if (error < 0) printk(KERN_ERR "[TSP] %s, %d Error!!\n", __func__, __LINE__);
		error = write_mem(data, obj_address+9, 1, &data->atchfrccalratio_e);	
		if (error < 0) printk(KERN_ERR "[TSP] %s, %d Error!!\n", __func__, __LINE__);
		printk(KERN_DEBUG"[TSP] %s execute!!\n", __func__);
		enable_irq(data->client->irq);
	}
	config_dwork_flag = 0;
	return;
}

static void resume_check_dworker(struct work_struct *work)
{
	check_resume_err = 0;
	check_resume_err_count = 0;
}

static void cal_check_dworker(struct work_struct *work)
{
	struct mxt540e_data *data =
		container_of(work, struct mxt540e_data, cal_check_dwork.work);
	int error;
	u16 size;
	u8 value;
	u16 obj_address = 0;
	if (mxt540e_enabled) {
		check_calibrate = 0;
		get_object_info(data, GEN_POWERCONFIG_T7, &size, &obj_address);
		value = 25;
		error = write_mem(data, obj_address+2, 1, &value);
		if (error < 0) printk(KERN_ERR "[TSP] %s, %d Error!!\n", __func__, __LINE__);
	}
	return;
}

uint8_t calibrate_chip(void)
{
	u8 cal_data = 1;
	int ret = 0;
	/* send calibration command to the chip */
	ret = write_mem(copy_data, copy_data->cmd_proc + CMD_CALIBRATE_OFFSET, 1, &cal_data);
	/* set flag for calibration lockup recovery if cal command was successful */
	if (!ret) {
		printk("[TSP] calibration success!!!\n");
		if (check_resume_err == 2) {
			check_resume_err = 1;
			schedule_delayed_work(&copy_data->resume_check_dwork,
							msecs_to_jiffies(2500));
		} else if (check_resume_err == 1) {
			cancel_delayed_work(&copy_data->resume_check_dwork);
			schedule_delayed_work(&copy_data->resume_check_dwork,
							msecs_to_jiffies(2500));
		}
	}
#if defined(CONFIG_USA_MODEL_SGH_I717)
	gForceCalibration_flag = 1;
#endif
	return ret;
}

static void mxt540e_ta_probe(int ta_status)
{
	u16 obj_address = 0;
	u16 size;
	u8 value;
	int error;
    struct mxt540e_data *data = copy_data;

	if (!mxt540e_enabled) {
		printk(KERN_ERR"mxt540e_enabled is 0\n");
		return;
	}

	error = 0;
	obj_address = 0;
/* medina filter error */

	Medianfilter_Err_cnt_Ta = 0;
	Medianfilter_Err_cnt_Batt = 0;
	MedianFirst_Flag = 1;
	table_cnt = 0;
	MedianError_cnt = 20;

	if (ta_status) {
		get_object_info(data, SPT_CTECONFIG_T46, &size, &obj_address);
		value = data->actvsyncsperx_charging;
		error |= write_mem(data, obj_address+3, 1, &value);
		get_object_info(data, PROCG_NOISESUPPRESSION_T48, &size, &obj_address);
		error |= write_config(data, data->t48_config_chrg_e[0], data->t48_config_chrg_e + 1);
		threshold = data->tchthr_charging;
	} else {
		get_object_info(data, TOUCH_MULTITOUCHSCREEN_T9, &size, &obj_address);
		
		value = 192;	
		error |= write_mem(data, obj_address+6, 1, &value);
		value = 50;
		error |= write_mem(data, obj_address+7, 1, &value);
		value = 80;
		error |= write_mem(data, obj_address+13, 1, &value);
		get_object_info(data, SPT_GENERICDATA_T57, &size, &obj_address);
		value = 25;
		error |= write_mem(data, obj_address+1, 1, &value);
		get_object_info(data, SPT_CTECONFIG_T46, &size, &obj_address);
		value = data->actvsyncsperx_batt;
		error |= write_mem(data, obj_address+3, 1, &value);
		get_object_info(data, PROCG_NOISESUPPRESSION_T48, &size, &obj_address);
		error |= write_config(data, data->t48_config_batt_e[0], data->t48_config_batt_e + 1);
		threshold = data->tchthr_batt;
	}
	if (error < 0)
		printk(KERN_ERR"[TSP] %s Error!!\n", __func__);
};

#if defined(DRIVER_FILTER)
static void equalize_coordinate(bool detect, u8 id, u16 *px, u16 *py)
{
	static int tcount[MAX_USING_FINGER_NUM] = { 0, };
	static u16 pre_x[MAX_USING_FINGER_NUM][4] = {{0}, };
	static u16 pre_y[MAX_USING_FINGER_NUM][4] = {{0}, };
	int coff[4] = {0,};
	int distance = 0;

	if (detect)
	{
		tcount[id] = 0;
	}

	pre_x[id][tcount[id]%4] = *px;
	pre_y[id][tcount[id]%4] = *py;

	if (tcount[id] > 3)
	{
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
#endif  /* DRIVER_FILTER */

uint8_t reportid_to_type(struct mxt540e_data *data, u8 report_id, u8 *instance)
{
	report_id_map_t *report_id_map;
	report_id_map = rid_map;

	if (report_id <= max_report_id) {
		*instance = report_id_map[report_id].instance;
		return (report_id_map[report_id].object_type);
	} else
		return 0;
}

static int __devinit mxt540e_init_touch_driver(struct mxt540e_data *data)
{
	struct object_t *object_table;
	u32 read_crc = 0;
	u32 calc_crc;
	u16 crc_address;
	u16 dummy;
	int i, j;
	u8 id[ID_BLOCK_SIZE];
	int ret;
	u8 type_count = 0;
	u8 tmp;
	int current_report_id, start_report_id;

	ret = read_mem(data, 0, sizeof(id), id);
	if (ret)
		return ret;

	dev_info(&data->client->dev, "family = %#02x, variant = %#02x, version "
			"= %#02x, build = %d\n", id[0], id[1], id[2], id[3]);
	printk(KERN_ERR"family = %#02x, variant = %#02x, version "
			"= %#02x, build = %d\n", id[0], id[1], id[2], id[3]);
	dev_dbg(&data->client->dev, "matrix X size = %d\n", id[4]);
	dev_dbg(&data->client->dev, "matrix Y size = %d\n", id[5]);

	data->family_id = id[0];
	data->tsp_version = id[2];
	data->tsp_build = id[3];
	data->objects_len = id[6];

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

	max_report_id = 0;

	for (i = 0; i < data->objects_len; i++) {
		object_table[i].i2c_address = le16_to_cpu(object_table[i].i2c_address);
		max_report_id += object_table[i].num_report_ids *
							(object_table[i].instances + 1);
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
#ifdef ITDEV
			data->msg_proc_addr = object_table[i].i2c_address;
#endif			
			data->msg_object_size = object_table[i].size + 1;
			dev_dbg(&data->client->dev, "Message object size = "
						"%d\n", data->msg_object_size);
			break;
		}
	}
	if (rid_map_alloc) {
		rid_map_alloc = false;
		kfree(rid_map);
	}
	rid_map = kmalloc((sizeof(report_id_map_t) * max_report_id + 1),
				GFP_KERNEL);
	if (!rid_map) {
		kfree(object_table);
		return -ENOMEM;
	}
	rid_map_alloc = true;
	rid_map[0].instance = 0;
	rid_map[0].object_type = 0;
	current_report_id = 1;

	for (i = 0; i < data->objects_len; i++) {
		if (object_table[i].num_report_ids != 0) {
			for (j = 0; j <= object_table[i].instances; j++) {
				for (start_report_id = current_report_id; current_report_id < 
						(start_report_id + object_table[i].num_report_ids);
						current_report_id++) {
					rid_map[current_report_id].instance = j;
					rid_map[current_report_id].object_type = object_table[i].object_type;
				}
			}
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

extern void set_lcd_esd_ignore( bool src );

static void resume_cal_err_func(struct mxt540e_data *data)
{
	int i;
	bool ta_status;
	int count = 0;
set_lcd_esd_ignore(1);
	printk("[TSP] %s\n", __func__);

	data->power_off();

	for (i = 0; i < data->num_fingers; i++) {
		if (data->fingers[i].state == MXT540E_STATE_INACTIVE)
			continue;
		data->fingers[i].z = 0;
		data->fingers[i].state = MXT540E_STATE_RELEASE;
		
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_KOR_MODEL_SHV_E160L) || defined(CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_JPN_MODEL_SC_05D)
	if (data->fingers[i].state == MXT540E_STATE_RELEASE) {
		input_mt_slot(data->input_dev, i);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
	} else {
		input_mt_slot(data->input_dev, i);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);
		
		input_report_abs(data->input_dev, ABS_MT_POSITION_X, data->fingers[i].x);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y, data->fingers[i].y);
		input_report_abs(data->input_dev, ABS_MT_PRESSURE, data->fingers[i].z);
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, data->fingers[i].w);
		
		#if defined(CONFIG_SHAPE_TOUCH)
		input_report_abs(data->input_dev, ABS_MT_COMPONENT, data->fingers[i].component);
		input_report_abs(data->input_dev, ABS_MT_SUMSIZE, sumsize);
		#endif
	}
#else
	input_report_abs(data->input_dev, ABS_MT_POSITION_X, data->fingers[i].x);
	input_report_abs(data->input_dev, ABS_MT_POSITION_Y, data->fingers[i].y);
	input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, data->fingers[i].z);
	input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, data->fingers[i].w);
	input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, i);
	#if defined(CONFIG_SHAPE_TOUCH)
	input_report_abs(data->input_dev, ABS_MT_COMPONENT, data->fingers[i].component);
	input_report_abs(data->input_dev, ABS_MT_SUMSIZE, sumsize);
	#endif
	input_mt_sync(data->input_dev);
#endif

#if 0
#if defined(CONFIG_SHAPE_TOUCH)
		if (sec_debug_level() != 0)
			printk(KERN_DEBUG "[TSP] id[%d],x=%d,y=%d,z=%d,w=%d,com=%d\n",
				i , data->fingers[i].x, data->fingers[i].y, data->fingers[i].z,
				data->fingers[i].w, data->fingers[i].component);
		else
			printk(KERN_DEBUG "[TSP] id[%d] status:%d\n", i, data->fingers[i].z);
#else
		if (sec_debug_level() != 0)
			printk(KERN_DEBUG "[TSP] id[%d],x=%d,y=%d,z=%d,w=%d\n",
				i , data->fingers[i].x, data->fingers[i].y, data->fingers[i].z,
				data->fingers[i].w);
		else
			printk(KERN_DEBUG "[TSP] id[%d] status:%d\n", i, data->fingers[i].z);
#endif
#else
		if (data->fingers[i].z == 0)
			printk(KERN_DEBUG "[TSP] released\n");
		else
			printk(KERN_DEBUG "[TSP] pressed\n");
#endif
		count++;
	}

	if (count)
		input_sync(data->input_dev);
	msleep(50);
	data->power_on();

	if (data->read_ta_status) {
		data->read_ta_status(&ta_status);
		printk("[TSP] ta_status is %d\n", ta_status);
		mxt540e_ta_probe(ta_status);
	}
	set_lcd_esd_ignore(0);
	calibrate_chip();
	cancel_delayed_work(&data->config_dwork);
	schedule_delayed_work(&data->config_dwork, HZ*5);
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

	if(distance < PATTERN_TRACKING_DISTANCE) return 1;
	else return 0;
}


static void report_input_data_torelease(struct mxt540e_data *data)
{
	int i;
	int count = 0;
	int report_count = 0;
	int press_count = 0;
	int move_count = 0;

	for (i = 0; i < data->num_fingers; i++) {
		if (data->fingers[i].state == MXT540E_STATE_INACTIVE)
			continue;
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_KOR_MODEL_SHV_E160L) || defined(CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_JPN_MODEL_SC_05D)
		if (data->fingers[i].state == MXT540E_STATE_RELEASE) {
			input_mt_slot(data->input_dev, i);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
		} else {
			input_mt_slot(data->input_dev, i);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);
			
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, data->fingers[i].x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, data->fingers[i].y);
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, data->fingers[i].z);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, data->fingers[i].w);
			}
#else
		input_report_abs(data->input_dev, ABS_MT_POSITION_X, data->fingers[i].x);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y, data->fingers[i].y);
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, data->fingers[i].z);
		input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, data->fingers[i].w);
		input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, i);

		input_mt_sync(data->input_dev);
#endif		
		report_count++;

		// debug-log all   press/move/release    
		if (g_debug_switch){
			#if defined(CONFIG_USA_MODEL_SGH_I717)
			if (data->fingers[i].state == MXT540E_STATE_PRESS || data->fingers[i].state == MXT540E_STATE_RELEASE) {
				printk(KERN_ERR "[TSP] ID-%d, %4d,%4d  UD:%d \n", i, data->fingers[i].x, data->fingers[i].y, data->fingers[i].state);
			}
			#else
			printk(KERN_ERR "[TSP] ID-%d, %4d,%4d  UD:%d \n", i, data->fingers[i].x, data->fingers[i].y, data->fingers[i].state);
			#endif
		}
		else
		{
			if (data->fingers[i].state == MXT540E_STATE_PRESS || data->fingers[i].state == MXT540E_STATE_RELEASE) {
				printk(KERN_ERR "[TSP-R] ID-%d, UD:%d \n", i, data->fingers[i].state);
			}
		}

	}

	if (report_count > 0)
		input_sync(data->input_dev);

}



/* Forced reboot sequence.  
    Don't use arbitraily. 
    if you meet special case that this routine has to be used, ask Xtopher's advice.
*/
static void TSP_forced_reboot(void)
{
    struct mxt540e_data *data = copy_data;

	int i;
	bool ta_status=0;

	if(ftouch_reboot == 1) return;
	ftouch_reboot  = 1;
	printk(KERN_ERR "[TSP] Reboot-Touch by Pattern Tracking S\n");
	cghost_clear = 0;

	cancel_delayed_work(&data->config_dwork);
	set_lcd_esd_ignore(1);

	/* Below is reboot sequence   */
	//disable_irq(copy_data->client->irq);		
	copy_data->power_off();
	
#if 0
	for (i = 0; i < copy_data->num_fingers; i++) {
		if (copy_data->fingers[i].z == -1)
			continue;
	
		//touch_is_pressed_arr[i] = 0;
		copy_data->fingers[i].z = 0;
	}
	report_input_data(copy_data);
#else
	for (i = 0; i < data->num_fingers; i++) {
		if (data->fingers[i].state == MXT540E_STATE_INACTIVE)
			continue;

		data->fingers[i].z = -1; // 0;
		data->fingers[i].state = MXT540E_STATE_RELEASE;
	}
	report_input_data_torelease(data);
#endif

	msleep(100);
	copy_data->power_on();
	msleep(50);
	set_lcd_esd_ignore(0);

	//enable_irq(copy_data->client->irq);
	mxt540e_enabled = 1;
	
	if(copy_data->read_ta_status) {
		copy_data->read_ta_status(&ta_status);
		printk(KERN_ERR "[TSP] ta_status is %d", ta_status);
		mxt540e_ta_probe(ta_status);
	}

	check_resume_err = 2;
	calibrate_chip();
	check_calibrate = 3;
	schedule_delayed_work(&data->config_dwork, HZ*5);
	config_dwork_flag = 3;
	
	ftouch_reboot  = 0;
	printk(KERN_ERR "[TSP] Reboot-Touch by Pattern Tracking E\n");

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
				printk(KERN_ERR "[TSP] SUNFLOWER (PATTERN TRACKING) %d\n",ghosttouchcount);
				clear_tcount();

				cFailbyPattenTracking++;
				if(cFailbyPattenTracking > MAX_GHOSTTOUCH_BY_PATTERNTRACKING)
				{
					cFailbyPattenTracking = 0;
					TSP_forced_reboot();
				}
				else
				{
					Mxt540e_force_released();
				}
			}
		}
	}
	return ghosttouch;
}



static void report_input_data(struct mxt540e_data *data)
{
	int i;
	int count = 0;
	int report_count = 0;
	int press_count = 0;
	int move_count = 0;

	for (i = 0; i < data->num_fingers; i++) {
		if (data->fingers[i].state == MXT540E_STATE_INACTIVE)
			continue;

#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_KOR_MODEL_SHV_E160L) || defined(CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_JPN_MODEL_SC_05D)
		if (data->fingers[i].state == MXT540E_STATE_RELEASE) {
			input_mt_slot(data->input_dev, i);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
		} else {
			input_mt_slot(data->input_dev, i);
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);
			
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, data->fingers[i].x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, data->fingers[i].y);
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, data->fingers[i].z);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, data->fingers[i].w);
			
			#if defined(CONFIG_SHAPE_TOUCH)
			input_report_abs(data->input_dev, ABS_MT_COMPONENT, data->fingers[i].component);
			input_report_abs(data->input_dev, ABS_MT_SUMSIZE, sumsize);
			#endif
		}
#else
		input_report_abs(data->input_dev, ABS_MT_POSITION_X, data->fingers[i].x);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y, data->fingers[i].y);
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, data->fingers[i].z);
		input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, data->fingers[i].w);
		input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, i);

		#if defined(CONFIG_SHAPE_TOUCH)
		input_report_abs(data->input_dev, ABS_MT_COMPONENT, data->fingers[i].component);
		input_report_abs(data->input_dev, ABS_MT_SUMSIZE, sumsize);
		#endif
		input_mt_sync(data->input_dev);
#endif

		report_count++;
		if (data->fingers[i].state == MXT540E_STATE_PRESS || data->fingers[i].state == MXT540E_STATE_RELEASE) {
#if 0
#if defined(CONFIG_SHAPE_TOUCH)
			if (sec_debug_level() != 0)
				printk(KERN_DEBUG "[TSP] id[%d],x=%d,y=%d,z=%d,w=%d,com=%d\n",
					i , data->fingers[i].x, data->fingers[i].y, data->fingers[i].z,
					data->fingers[i].w, data->fingers[i].component);
			else
				printk(KERN_DEBUG "[TSP] id[%d] status:%d\n", i, data->fingers[i].z);
#else
			if (sec_debug_level() != 0)
				printk(KERN_DEBUG "[TSP] id[%d],x=%d,y=%d,z=%d,w=%d\n",
					i , data->fingers[i].x, data->fingers[i].y, data->fingers[i].z,
					data->fingers[i].w);
			else
				printk(KERN_DEBUG "[TSP] id[%d] status:%d\n", i, data->fingers[i].z);
#endif
#else
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_KOR_MODEL_SHV_E160L) || defined(CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_JPN_MODEL_SC_05D)
		// use all debug-log below.
#else
		if (data->fingers[i].z == 0)
			printk(KERN_DEBUG "[TSP] released\n");
		else
			printk(KERN_DEBUG "[TSP] pressed\n");
#endif
#endif
		}

		// debug-log all   press/move/release    
		if (g_debug_switch){
			#if defined(CONFIG_USA_MODEL_SGH_I717)
			if (data->fingers[i].state == MXT540E_STATE_PRESS || data->fingers[i].state == MXT540E_STATE_RELEASE) {
				printk(KERN_ERR "[TSP] ID-%d, %4d,%4d  UD:%d \n", i, data->fingers[i].x, data->fingers[i].y, data->fingers[i].state);
			}
			#else
			printk(KERN_ERR "[TSP] ID-%d, %4d,%4d  UD:%d \n", i, data->fingers[i].x, data->fingers[i].y, data->fingers[i].state);
			#endif
		}
		else
		{
			if (data->fingers[i].state == MXT540E_STATE_PRESS || data->fingers[i].state == MXT540E_STATE_RELEASE) {
				printk(KERN_ERR "[TSP] ID-%d, UD:%d \n", i, data->fingers[i].state);
			}
		}

		//tsp_pattern_tracking(i, data->fingers[i].x, data->fingers[i].y);


		if (check_resume_err != 0) {
			if (data->fingers[i].state == MXT540E_STATE_MOVE)
				move_count++;
			if (data->fingers[i].state == MXT540E_STATE_PRESS)
				press_count++;
		}

		if (data->fingers[i].state == MXT540E_STATE_RELEASE) {
			data->fingers[i].state = MXT540E_STATE_INACTIVE;
		}
		else {
			data->fingers[i].state = MXT540E_STATE_MOVE;
			count++;
		}
	}
#ifdef ITDEV
	if (!driver_paused)//itdev
#else
	if (report_count > 0)
#endif
		input_sync(data->input_dev);

	if (count) touch_is_pressed = 1;
	else touch_is_pressed = 0;

	if (count == 0) {
		sumsize = 0;
#ifdef TOUCH_CPU_LOCK        
		if (touch_cpu_lock_status) {
			s5pv310_cpufreq_lock_free(DVFS_LOCK_ID_TSP);
			touch_cpu_lock_status = 0;
		}
#endif        
	}
	data->finger_mask = 0;

	if (check_resume_err != 0) {
		if ((press_count > 0) && (move_count > 0)) {
			check_resume_err_count++;
			if (check_resume_err_count > 4) {
				check_resume_err_count = 0;
				resume_cal_err_func(data);
			}
		}
	}

	if (check_calibrate == 1) {
		if (touch_is_pressed)
			cancel_delayed_work(&data->cal_check_dwork);
		else
			schedule_delayed_work(&data->cal_check_dwork, msecs_to_jiffies(1400));
	}		
}


ERR_RTN_CONTIOIN Check_Err_Condition(void)
{
	ERR_RTN_CONTIOIN rtn;

		switch(gErrCondition)
		{
			case ERR_RTN_CONDITION_IDLE:
				rtn = ERR_RTN_CONDITION_T9;
				break;

//			case ERR_RTN_CONDITION_T9:
//				rtn = ERR_RTN_CONDITION_T48;
//				break;

//			case ERR_RTN_CONDITION_T48:
//				rtn = ERR_RTN_CONDITION_IDLE;
//				break;
			default:
			break;
		}
	return rtn;
}


static irqreturn_t mxt540e_irq_thread(int irq, void *ptr)
{
	struct mxt540e_data *data = ptr;
	int id;
	u8 msg[data->msg_object_size];
	u8 touch_message_flag = 0;
	u16 obj_address = 0;
	u16 size;
	u8 value;
	int error = 0;
	u8 object_type, instance;
	bool ta_status = 0;
    u8 pending_message_count = 0;    

	do {
		touch_message_flag = 0;
		if (read_mem(data, data->msg_proc, sizeof(msg), msg)) {
            printk("[TSP] immediately return IRQ_HANDLED\n");   
			return IRQ_HANDLED;
		}
#ifdef ITDEV//itdev
				if (debug_enabled) 
					print_hex_dump(KERN_DEBUG, "MXT MSG:", DUMP_PREFIX_NONE, 16, 1, msg, sizeof(msg), false); 
#endif

		object_type = reportid_to_type(data, msg[0] , &instance);
		if (object_type == GEN_COMMANDPROCESSOR_T6) {
			if (msg[1] == 0x00) /* normal mode */
			{
				printk("[TSP] normal mode\n");
#if defined(CONFIG_USA_MODEL_SGH_I717)				
				if ((gForceCalibration_flag == 1)&& (gIgnoreReport_flag == 1)) {
					gIgnoreReport_flag = 0;
					printk(KERN_DEBUG"[TSP] Now! Enable Touch Report!!!\n");
				}
#endif
			
			}
			if ((msg[1]&0x04) == 0x04) /* I2C checksum error */
			{
				printk("[TSP] I2C checksum error\n");
			}
			if ((msg[1]&0x08) == 0x08) /* config error */
			{
				printk("[TSP] config error\n");
			}
			if ((msg[1]&0x10) == 0x10) /* calibration */
			{
				printk("[TSP] calibration is on going\n");
				if (check_calibrate == 3)
					check_calibrate = 0;
				else if (check_calibrate == 1) {
					cancel_delayed_work(&data->cal_check_dwork);
					schedule_delayed_work(&data->cal_check_dwork,
							msecs_to_jiffies(1400));
				} else {
					check_calibrate = 1;
					value = 6;
					get_object_info(data, GEN_POWERCONFIG_T7, &size, &obj_address);
					error = write_mem(data, obj_address+2, 1, &value);
					if (error < 0)
						printk(KERN_ERR "[TSP] %s, %d Error!!\n", __func__, __LINE__);
					schedule_delayed_work(&data->cal_check_dwork,
							msecs_to_jiffies(1400));
				}

				if (config_dwork_flag == 3)
					config_dwork_flag = 1;
				else if (config_dwork_flag == 1) {
					cancel_delayed_work(&data->config_dwork);
					schedule_delayed_work(&data->config_dwork, HZ*5);
				} else {
					config_dwork_flag = 1;
					get_object_info(data, GEN_ACQUISITIONCONFIG_T8, &size, &obj_address);
					value = 8;
					error = write_mem(data, obj_address+8, 1, &value);
					value = 180;
					error |= write_mem(data, obj_address+9, 1, &value);
					if (error < 0)
						printk(KERN_ERR "[TSP] %s, %d Error!!\n", __func__, __LINE__);
					schedule_delayed_work(&data->config_dwork, HZ*5);
				}
			}
			if ((msg[1]&0x20) == 0x20) /* signal error */
			{
				printk("[TSP] signal error\n");
			}
			if ((msg[1]&0x40) == 0x40) /* overflow */
			{
				printk("[TSP] overflow detected\n");
			}
			if ((msg[1]&0x80) == 0x80) /* reset */
			{
				printk("[TSP] reset is ongoing\n");
			}
		}

		if (object_type == PROCI_TOUCHSUPPRESSION_T42) {
			if ((msg[1] & 0x01) == 0x00) { /* Palm release */
				printk("[TSP] palm touch released\n");
				touch_is_pressed = 0;
			} else if ((msg[1] & 0x01) == 0x01) { /* Palm Press */
				printk("[TSP] palm touch detected\n");
				touch_is_pressed = 1;
				touch_message_flag = 1;
			}
		}

		if (object_type == SPT_GENERICDATA_T57) {
			sumsize =  msg[1] + (msg[2]<<8);
		}

		if (object_type == PROCG_NOISESUPPRESSION_T48) {
			if (msg[4] == 5) { /* Median filter error */
					printk("[TSP] Median filter Error\n");
				if (data->read_ta_status) {
					data->read_ta_status(&ta_status);
					printk("[TSP] ta_status is %d\n", ta_status);
					if (ta_status) {
						get_object_info(data, PROCG_NOISESUPPRESSION_T48, &size, &obj_address);
#if 1 /* median filter error */

						if(Medianfilter_Err_cnt_Ta>=MedianError_cnt){		
							
							Medianfilter_Err_cnt_Ta = 0;
							if(table_cnt>3){
								table_cnt = 0;
								MedianError_cnt = 20;
							}
							else{
								table_cnt++;
								MedianError_cnt = 2;
							}
							
							value = Median_Error_Table_TA[table_cnt];
							error = write_mem(data, obj_address+3, 1, &value);		// Base Freq
							printk("[TSP]  median base_Freq_ta %d\n", Median_Error_Table_TA[table_cnt]);
						}
						else{
							Medianfilter_Err_cnt_Ta++;
							printk("[TSP]  median error_cnt_ta %d\n", Medianfilter_Err_cnt_Ta);
						}
						if(MedianFirst_Flag){									// When first MedianfillterError, Setup
							MedianFirst_Flag = 0;
							value = Median_Error_Table_TA[0];		// 33
							error = write_mem(data, obj_address+3, 1, &value);		// Base Freq
						}
#else
						value = 33;
						error = write_mem(data, obj_address+3, 1, &value);
#endif
						value = 1;
						error |= write_mem(data, obj_address+8, 1, &value);
						value = 2;
						error |= write_mem(data, obj_address+9, 1, &value);
						value = 100;
						error |= write_mem(data, obj_address+17, 1, &value);
						value = 20;
						error |= write_mem(data, obj_address+22, 1, &value);
						value = 2;
						error |= write_mem(data, obj_address+23, 1, &value);
						value = 46;
						error |= write_mem(data, obj_address+25, 1, &value);
						value = 80;
						error |= write_mem(data, obj_address+34, 1, &value);
						value = 35;
						error |= write_mem(data, obj_address+35, 1, &value);
						value = 15;
						error |= write_mem(data, obj_address+37, 1, &value);
						value = 5;
						error |= write_mem(data, obj_address+38, 1, &value);
						value = 65;
						error |= write_mem(data, obj_address+39, 1, &value);
						value = 30;
						error |= write_mem(data, obj_address+41, 1, &value);
						value = 50;
						error |= write_mem(data, obj_address+42, 1, &value);
						value = 7;
						error |= write_mem(data, obj_address+45, 1, &value);
						value = 7;
						error |= write_mem(data, obj_address+46, 1, &value);
						value = 40;
						error |= write_mem(data, obj_address+50, 1, &value);
						value = 32;
						error |= write_mem(data, obj_address+51, 1, &value);
						value = 15;
						error |= write_mem(data, obj_address+52, 1, &value);
						get_object_info(data, SPT_CTECONFIG_T46, &size, &obj_address);
						value = 32;
						error |= write_mem(data, obj_address+3, 1, &value);
						get_object_info(data, SPT_GENERICDATA_T57, &size, &obj_address);
						value = 22;
						error |= write_mem(data, obj_address+1, 1, &value);
					} else {
#if !defined(CONFIG_USA_MODEL_SGH_I717)
						get_object_info(data, TOUCH_MULTITOUCHSCREEN_T9, &size, &obj_address);
						value = 160;
						error = write_mem(data, obj_address+6, 1, &value);
						value = 45;
						error |= write_mem(data, obj_address+7, 1, &value);
						value = 80;
						error |= write_mem(data, obj_address+13, 1, &value);
						value = 3;
						error |= write_mem(data, obj_address+22, 1, &value);
						value = 2;
						error |= write_mem(data, obj_address+24, 1, &value);
#endif
						get_object_info(data, PROCG_NOISESUPPRESSION_T48, &size, &obj_address);
						value = 242;
						error |= write_mem(data, obj_address+2, 1, &value);
#if 1 /* median filter error */

						if(Medianfilter_Err_cnt_Batt>=MedianError_cnt){		
							Medianfilter_Err_cnt_Batt = 0;

							if(table_cnt>3){
								table_cnt = 0;
								MedianError_cnt = 20;
							}
							else{
								table_cnt++;
								MedianError_cnt = 2;
							}
							
							value = Median_Error_Table_Batt[table_cnt];
							error = write_mem(data, obj_address+3, 1, &value);		// Base Freq
							printk("[TSP]  median base_Freq_Batt %d\n", value);
							
						}
						else{
							Medianfilter_Err_cnt_Batt++;
							printk("[TSP]  median error_cnt_Batt %d\n", Medianfilter_Err_cnt_Batt);
						}
						
						if(MedianFirst_Flag){									// When first MedianfillterError, Setup
							MedianFirst_Flag = 0;
							value = Median_Error_Table_Batt[0];					// 20
							error = write_mem(data, obj_address+3, 1, &value);		// Base Freq
						}
#else
						value = 20;
						error |= write_mem(data, obj_address+3, 1, &value);
#endif
						value = 100;
						error |= write_mem(data, obj_address+17, 1, &value);
						value = 25;
						error |= write_mem(data, obj_address+22, 1, &value);
						value = 46;
						error |= write_mem(data, obj_address+25, 1, &value);
						#if defined (CONFIG_USA_MODEL_SGH_I717)
						value = 128;
						#else
						value = 112;
						#endif
						error |= write_mem(data, obj_address+34, 1, &value);
						value = 35;
						error |= write_mem(data, obj_address+35, 1, &value);
						value = 0;
						error |= write_mem(data, obj_address+39, 1, &value);
						value = 40;
						error |= write_mem(data, obj_address+42, 1, &value);
						get_object_info(data, SPT_CTECONFIG_T46, &size, &obj_address);
						#if defined (CONFIG_USA_MODEL_SGH_I717)
						value = 24;
						#else
						value = 32;
						#endif
						error |= write_mem(data, obj_address+3, 1, &value);
						get_object_info(data, SPT_GENERICDATA_T57, &size, &obj_address);
						value = 15;
						error |= write_mem(data, obj_address+1, 1, &value);
					}
					if (error)
						printk(KERN_ERR"[TSP] fail median filter err setting\n");
					else
						printk(KERN_DEBUG"[TSP] success median filter err setting\n");
				} else {
					get_object_info(data, PROCG_NOISESUPPRESSION_T48, &size, &obj_address);
					value = 0;
					error = write_mem(data, obj_address+2, 1, &value);
					msleep(15);
					value = data->calcfg_batt_e;
					error |= write_mem(data, obj_address+2, 1, &value);
					if (error)
						printk(KERN_ERR "[TSP] failed to reenable CHRGON\n");
				}
					}
		}


#if defined (CONFIG_USA_MODEL_SGH_I717)
		if (gIgnoreReport_flag == 1) {
			printk(KERN_DEBUG"[TSP] gIgnore touch\n");
			return IRQ_HANDLED;
		}
#endif
		if (object_type == TOUCH_MULTITOUCHSCREEN_T9) {
			id = msg[0] - data->finger_type;

			/* If not a touch event, then keep going */
			if (id < 0 || id >= data->num_fingers){
					continue;
            }

//			if (data->finger_mask & (1U << id))
//				report_input_data(data);

			if (msg[1] & RELEASE_MSG_MASK) {
				data->fingers[id].z = 0;
				data->fingers[id].w = msg[5];
				data->finger_mask |= 1U << id;
				data->fingers[id].state = MXT540E_STATE_RELEASE;
			} else if ((msg[1] & DETECT_MSG_MASK) &&
				(msg[1] & (PRESS_MSG_MASK | MOVE_MSG_MASK | VECTOR_MSG_MASK))) {
#ifdef TOUCH_CPU_LOCK				
				if (touch_cpu_lock_status == 0) {
#ifdef CONFIG_S5PV310_HI_ARMCLK_THAN_1_2GHZ
					s5pv310_cpufreq_lock(DVFS_LOCK_ID_TSP, CPU_L4);
#else
					s5pv310_cpufreq_lock(DVFS_LOCK_ID_TSP, CPU_L3);
#endif
					touch_cpu_lock_status = 1;
				}
#endif                
              //printk(KERN_DEBUG "AFTER  [MID_TSP] msg[0] = (0x%x), msg[1] = (0x%x), id = (%d), finger_mask = (%d)\n", msg[0], msg[1], id, data->finger_mask );
				touch_message_flag = 1;
				data->fingers[id].component = msg[7];
				data->fingers[id].z = msg[6];
				data->fingers[id].w = msg[5];
				data->fingers[id].x = ((msg[2] << 4) | (msg[4] >> 4)) >>
								data->x_dropbits;
				data->fingers[id].y = ((msg[3] << 4) |
						(msg[4] & 0xF)) >> data->y_dropbits;
				data->finger_mask |= 1U << id;
#if defined(DRIVER_FILTER)
				if (msg[1] & PRESS_MSG_MASK) {
					equalize_coordinate(1, id, &data->fingers[id].x, &data->fingers[id].y);
					data->fingers[id].state = MXT540E_STATE_PRESS;
				}
				else if (msg[1] & MOVE_MSG_MASK) {
					equalize_coordinate(0, id, &data->fingers[id].x, &data->fingers[id].y);
				}
#else
				if (msg[1] & PRESS_MSG_MASK) {
					data->fingers[id].state = MXT540E_STATE_PRESS;
				}
#endif
#if defined(CONFIG_SHAPE_TOUCH)
					data->fingers[id].component= msg[7];
#endif

			} else if ((msg[1] & SUPPRESS_MSG_MASK) && (data->fingers[id].state != MXT540E_STATE_INACTIVE)) {
				data->fingers[id].z = 0;
				data->fingers[id].w = msg[5];
				data->fingers[id].state = MXT540E_STATE_RELEASE;
				data->finger_mask |= 1U << id;
			} else {
				dev_dbg(&data->client->dev, "Unknown state %#02x %#02x\n", msg[0], msg[1]);
				continue;
			}
		}

	if (data->finger_mask)
		report_input_data(data);


	} while (pending_message_count--);


	return IRQ_HANDLED;
}

static void mxt540e_deepsleep(struct mxt540e_data *data)
{
	u8 power_cfg[3] = {0, };
	write_config(data, GEN_POWERCONFIG_T7, power_cfg);
	deepsleep = 1;
}

static void mxt540e_wakeup(struct mxt540e_data *data)
{
	write_config(data, GEN_POWERCONFIG_T7, data->power_cfg);
}

static int mxt540e_internal_suspend(struct mxt540e_data *data)
{
	int i;
	cancel_delayed_work(&data->config_dwork);
	cancel_delayed_work(&data->resume_check_dwork);
	cancel_delayed_work(&data->cal_check_dwork);

	for (i = 0; i < data->num_fingers; i++) {
		if (data->fingers[i].state == MXT540E_STATE_INACTIVE)
			continue;
		data->fingers[i].z = 0;
		data->fingers[i].state = MXT540E_STATE_RELEASE;
	}
	report_input_data(data);

	if (!deepsleep) data->power_off();

	return 0;
}

static int mxt540e_internal_resume(struct mxt540e_data *data)
{
	if (!deepsleep) data->power_on();
	else mxt540e_wakeup(data);
#if 0
	calibrate_chip();
	schedule_delayed_work(&data->config_dwork, HZ*5);
#endif
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
#define mxt540e_suspend	NULL
#define mxt540e_resume	NULL

static void mxt540e_early_suspend(struct early_suspend *h)
{
	struct mxt540e_data *data = container_of(h, struct mxt540e_data, early_suspend);

//	set_lcd_esd_ignore(1);  // remove to use changed linux level
	if (mxt540e_enabled) {
		printk("[TSP] %s\n", __func__);
		mxt540e_enabled = 0;
		touch_is_pressed = 0;
		is_drawingmode = 0;


#if defined (CONFIG_USA_MODEL_SGH_I717)
		gIgnoreReport_flag = 0;
		gForceCalibration_flag = 0;
#endif

		disable_irq(data->client->irq);
		mxt540e_internal_suspend(data);
	} else {
		printk("[TSP] %s, but already off\n", __func__);
	}
}

static void mxt540e_late_resume(struct early_suspend *h)
{
	bool ta_status = 0;
	u8 id[ID_BLOCK_SIZE];    
	int ret = 0;    
	int retry = 3;

	struct mxt540e_data *data = container_of(h, struct mxt540e_data,
								early_suspend);
	if (mxt540e_enabled == 0) {
		printk("[TSP] %s\n", __func__);
		mxt540e_internal_resume(data);

		mxt540e_enabled = 1;

#if defined (CONFIG_USA_MODEL_SGH_I717)
		gIgnoreReport_flag = 1;
#endif        
        ret = read_mem(data, 0, sizeof(id), id);
        
        if (ret) {
            while (retry--) {
                printk(KERN_DEBUG"[TSP] chip boot failed retry %d\n", retry);
				set_lcd_esd_ignore(1);
                data->power_off();
                msleep(200);
                data->power_on();
				set_lcd_esd_ignore(0);
    
                ret = read_mem(data, 0, sizeof(id), id);
                if (ret == 0 || retry <= 0)
                    break;
            }
        }       

		if (data->read_ta_status) {
			data->read_ta_status(&ta_status);
			printk("[TSP] ta_status is %d\n", ta_status);
			mxt540e_ta_probe(ta_status);
		}
		if(deepsleep) deepsleep = 0;

		check_resume_err = 2;
		calibrate_chip();
		check_calibrate = 3;
		schedule_delayed_work(&data->config_dwork, HZ*5);
		config_dwork_flag = 3;
		enable_irq(data->client->irq);
	} else {
		printk("[TSP] %s, but already on\n", __func__);
	}
//	set_lcd_esd_ignore(0); // remove to use changed linux level
}
#else
static int mxt540e_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt540e_data *data = i2c_get_clientdata(client);

	mxt540e_enabled = 0;
	touch_is_pressed = 0;
	return mxt540e_internal_suspend(data);
}

static int mxt540e_resume(struct device *dev)
{
	int ret = 0;
	bool ta_status = 0;
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt540e_data *data = i2c_get_clientdata(client);

	ret = mxt540e_internal_resume(data);

	mxt540e_enabled = 1;

	if (data->read_ta_status) {
		data->read_ta_status(&ta_status);
		printk("[TSP] ta_status is %d\n", ta_status);
		mxt540e_ta_probe(ta_status);
	}
	return ret;
}
#endif

void Mxt540e_force_released(void)
{
	struct mxt540e_data *data = copy_data;
	int i;

	if (!mxt540e_enabled) {
		printk(KERN_ERR"[TSP] mxt540e_enabled is 0\n");
		return;
	}

	for (i = 0; i < data->num_fingers; i++) {
		if (data->fingers[i].state == MXT540E_STATE_INACTIVE)
			continue;
		data->fingers[i].z = 0;
		data->fingers[i].state = MXT540E_STATE_RELEASE;
	}
	report_input_data_torelease(data);
	calibrate_chip();
};
EXPORT_SYMBOL(Mxt540e_force_released);

static ssize_t mxt540e_debug_setting(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	g_debug_switch = !g_debug_switch;
	return 0;
}

static ssize_t mxt540e_object_setting(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct mxt540e_data *data = dev_get_drvdata(dev);
	unsigned int object_type;
	unsigned int object_register;
	unsigned int register_value;
	u8 value;
	u8 val;
	int ret;
	u16 address;
	u16 size;
	sscanf(buf, "%u%u%u", &object_type, &object_register, &register_value);
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
	write_mem(data, address+(u16)object_register, size, &value);
	read_mem(data, address+(u16)object_register, (u8)size, &val);

	printk(KERN_ERR "[TSP] T%d Byte%d is %d\n", object_type, object_register, val);
	return count;
}

static ssize_t mxt540e_object_show(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct mxt540e_data *data = dev_get_drvdata(dev);
	unsigned int object_type;
	u8 val;
	int ret;
	u16 address;
	u16 size;
	u16 i;
	sscanf(buf, "%u", &object_type);
	printk("[TSP] object type T%d\n", object_type);
	ret = get_object_info(data, (u8)object_type, &size, &address);
	if (ret) {
		printk(KERN_ERR "[TSP] fail to get object_info\n");
		return count;
	}
	for (i = 0; i < size; i++) {
		read_mem(data, address+i, 1, &val);
		printk("[TSP] Byte %u --> %u\n", i, val);
	}
	return count;
}

struct device *sec_touchscreen;
#if defined(CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
static u8 firmware_latest =0x13; /* mxt540E : 0x10 */
#else
static u8 firmware_latest =0x12; /* mxt540E : 0x10 */
#endif
static u8 build_latest = 0xAA;

struct device *mxt540e_noise_test;

/*
	top_left, top_right, center, bottom_left, bottom_right
*/
unsigned int test_node[5] = {443, 53, 253, 422, 32};
uint16_t qt_refrence_node[540] = { 0 };
uint16_t qt_delta_node[540] = { 0 };

void diagnostic_chip(u8 mode)
{
	int error;
	u16 t6_address = 0;
	u16 size_one;
	int ret;

	ret = get_object_info(copy_data, GEN_COMMANDPROCESSOR_T6, &size_one, &t6_address);

	size_one = 1;
	error = write_mem(copy_data, t6_address+5, (u8)size_one, &mode);

	if (error < 0) {
		printk(KERN_ERR "[TSP] error %s: write_object\n", __func__);
	}
}

uint8_t read_uint16_t(struct mxt540e_data *data, uint16_t address, uint16_t *buf )
{
	uint8_t status;
	uint8_t temp[2];

	status = read_mem(data, address, 2, temp);
	*buf= ((uint16_t)temp[1]<<8)+ (uint16_t)temp[0];

	return (status);
}

void read_dbg_data(uint8_t dbg_mode , uint16_t node, uint16_t *dbg_data)
{
	u8 read_page, read_point;
	uint8_t mode,page;
	u16 size;
	u16 diagnostic_addr = 0;

	if (!mxt540e_enabled) {
		printk(KERN_ERR "[TSP ]read_dbg_data. mxt540e_enabled is 0\n");
		return;
	}

	get_object_info(copy_data, DEBUG_DIAGNOSTIC_T37, &size, &diagnostic_addr);

	read_page = node / 64;
	node %= 64;
	read_point = (node * 2) + 2;

	/* Page Num Clear */
	diagnostic_chip(MXT_CTE_MODE);
	msleep(10);

	do {
		if(read_mem(copy_data, diagnostic_addr, 1, &mode))
		{
			printk(KERN_INFO "[TSP] READ_MEM_FAILED \n");
			return;
		}
	} while(mode != MXT_CTE_MODE);

	diagnostic_chip(dbg_mode);
	msleep(10);

	do {
		if(read_mem(copy_data, diagnostic_addr, 1, &mode))
		{
			printk(KERN_INFO "[TSP] READ_MEM_FAILED \n");
			return;
		}
	} while(mode != dbg_mode);

    for(page = 1; page <= read_page;page++)
	{
		diagnostic_chip(MXT_PAGE_UP);
		msleep(10);
		do {
			if(read_mem(copy_data, diagnostic_addr + 1, 1, &mode))
			{
				printk(KERN_INFO "[TSP] READ_MEM_FAILED \n");			
				return;
			}
		} while(mode != page);
	}

	if(read_uint16_t(copy_data, diagnostic_addr + read_point, dbg_data))
	{
		printk(KERN_INFO "[TSP] READ_MEM_FAILED \n");			
		return;
	}
}

#if defined (CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined(CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_JPN_MODEL_SC_05D) // For Kor's Factory test Mode 
#define MIN_VALUE 19744
#define MAX_VALUE 28864

#define COMP_VALUE 22304

int read_all_data(uint16_t dbg_mode)
{
	u8 read_page, read_point;
	u16 max_value = MIN_VALUE, min_value = MAX_VALUE;
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
	diagnostic_chip(MXT_CTE_MODE);
	msleep(30);/* msleep(20);  */

	diagnostic_chip(dbg_mode);
	msleep(30);/* msleep(20);  */

	ret = get_object_info(copy_data, DEBUG_DIAGNOSTIC_T37, &size, &object_address);
	msleep(50); /* msleep(20);  */

	for (read_page = 0 ; read_page < 9; read_page++) {
		for (node = 0; node < 64; node++) {
			read_point = (node * 2) + 2;
			read_mem(copy_data, object_address+(u16)read_point, 2, data_buffer);
			qt_refrence_node[num] = ((uint16_t)data_buffer[1]<<8)+ (uint16_t)data_buffer[0];
            
			if(qt_refrence_node[num] == 0)	qt_refrence_node[num] = min_value;
			if(ta_state)  /* (dual-x) all node */
			{	
				if( (read_page == 7) ||(read_page == 8) )
					break;
			}
			else  /* all node  */
			{	
			}
			
			/* q1 use x=16 line, y=26 line */
			if ((num%30 == 26) || (num%30 == 27) || (num%30 == 28) || (num%30 == 29)) {
                //qt_refrence_node[num] = 21429;  // Temporary code 
				num++;
                
				if (num == 480)
					break;
				else
					continue;
			}

            printk(KERN_ERR"[TSP] read_all_data :: qt_refrence_node[%3d] = %5d \n", num, qt_refrence_node[num]);
            
			if ((qt_refrence_node[num] < MIN_VALUE) || (qt_refrence_node[num] > MAX_VALUE)) {
				state = 1;
                printk(KERN_ERR"[TSP] read_all_data :: over limitted :: qt_refrence_node[%3d] = %5d \n", num, qt_refrence_node[num]);
			}

			if (data_buffer[0] != 0) {
				if(qt_refrence_node[num] > max_value)	max_value = qt_refrence_node[num];
				if(qt_refrence_node[num] < min_value)	min_value = qt_refrence_node[num];
			}
			num++;
			if (num == 480)
				break;
		}
		diagnostic_chip(MXT_PAGE_UP);
		msleep(35);
		if (num == 480)
			break;
	}

	if ((max_value - min_value) > 4500) {
		printk(KERN_ERR "[TSP] read_all_data diff = %d, max_value = %d, min_value = %d\n", (max_value - min_value), max_value, min_value);
		state = 1;
	}
	return state;
}

#else 
#define MIN_VALUE 19744
#define MAX_VALUE 28864

int read_all_data(uint16_t dbg_mode)
{
	u8 read_page, read_point;
	u16 max_value = MIN_VALUE, min_value = MAX_VALUE;
	u16 object_address = 0;
	u8 data_buffer[2] = { 0 };
	u8 node = 0;
	int state = 0;
	int num = 0;
	int ret;
	u16 size;

	/* Page Num Clear */
	diagnostic_chip(MXT_CTE_MODE);
	msleep(30);/* msleep(20);  */

	diagnostic_chip(dbg_mode);
	msleep(30);/* msleep(20);  */

	ret = get_object_info(copy_data, DEBUG_DIAGNOSTIC_T37, &size, &object_address);
	msleep(50); /* msleep(20);  */

	for (read_page = 0 ; read_page < 9; read_page++) {
		for (node = 0; node < 64; node++) {
			read_point = (node * 2) + 2;
			read_mem(copy_data, object_address+(u16)read_point, 2, data_buffer);
			qt_refrence_node[num] = ((uint16_t)data_buffer[1]<<8)+ (uint16_t)data_buffer[0];
            
			/* q1 use x=16 line, y=26 line */
			if ((num%30 == 26) || (num%30 == 27) || (num%30 == 28) || (num%30 == 29)) {
				num++;
				if (num == 480)
					break;
				else
					continue;
			}
            
			if ((qt_refrence_node[num] < MIN_VALUE) || (qt_refrence_node[num] > MAX_VALUE)) {
				state = 1;
				printk(KERN_ERR"[TSP] Mxt540E qt_refrence_node[%3d] = %5d \n", num, qt_refrence_node[num]);
			}

			if (data_buffer[0] != 0) {
				if(qt_refrence_node[num] > max_value)	max_value = qt_refrence_node[num];
				if(qt_refrence_node[num] < min_value)	min_value = qt_refrence_node[num];
			}
			num++;
			if (num == 480)
				break;
            
			/* all node => 18 * 30 = 540 => (8page * 64)  + 28*/
			if ((read_page == 8) && (node == 28))
				break;
		}
		diagnostic_chip(MXT_PAGE_UP);
		msleep(35);
		if (num == 480)
			break;
	}

	if ((max_value - min_value) > 4500) {
		printk(KERN_ERR "[TSP] read_all_data diff = %d, max_value = %d, min_value = %d\n", (max_value - min_value), max_value, min_value);
		state = 1;
	}
	return state;
}
#endif 

int read_all_delta_data(uint16_t dbg_mode)
{
	u8 read_page, read_point;
	u16 object_address = 0;
	u8 data_buffer[2] = { 0 };
	u8 node = 0;
	int state = 0;
	int num = 0;
	int ret;
	u16 size;

	/* Page Num Clear */
	diagnostic_chip(MXT_CTE_MODE);
	msleep(30);
	diagnostic_chip(dbg_mode);
	msleep(30);

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

	for (read_page = 0 ; read_page < 9; read_page++) {
		for (node = 0; node < 64; node++) {
			read_point = (node * 2) + 2;
			read_mem(copy_data, object_address+(u16)read_point, 2, data_buffer);
				qt_delta_node[num] = ((uint16_t)data_buffer[1]<<8)+ (uint16_t)data_buffer[0];

			num++;

			/* all node => 18 * 30 = 540 => (8page * 64)  + 28*/
			if ((read_page == 8) && (node == 28))
				break;
		}
		diagnostic_chip(MXT_PAGE_UP);
		msleep(35);
	}

	return state;
}

static int mxt540e_check_bootloader(struct i2c_client *client,
					unsigned int state)
{
	u8 val;
	u8 temp;

recheck:
	if (i2c_master_recv(client, &val, 1) != 1) {
		dev_err(&client->dev, "%s: i2c recv failed\n", __func__);
		return -EIO;
	}

	if(val & 0x20)
	{

		if (i2c_master_recv(client, &temp, 1) != 1) {
			dev_err(&client->dev, "%s: i2c recv failed\n", __func__);
			return -EIO;
		}

		if (i2c_master_recv(client, &temp, 1) != 1) {
			dev_err(&client->dev, "%s: i2c recv failed\n", __func__);
			return -EIO;
		}

		val &= ~0x20;
	}

	if((val & 0xF0)== MXT540E_APP_CRC_FAIL)
	{
		printk("[TOUCH] MXT540E_APP_CRC_FAIL\n");
		if (i2c_master_recv(client, &val, 1) != 1) {
			dev_err(&client->dev, "%s: i2c recv failed\n", __func__);
			return -EIO;
		}

		if(val & 0x20)
		{
			if (i2c_master_recv(client, &temp, 1) != 1) {
				dev_err(&client->dev, "%s: i2c recv failed\n", __func__);
				return -EIO;
			}

			if (i2c_master_recv(client, &temp, 1) != 1) {
				dev_err(&client->dev, "%s: i2c recv failed\n", __func__);
				return -EIO;
			}

			val &= ~0x20;
		}
	}

	switch (state) {
	case MXT540E_WAITING_BOOTLOAD_CMD:
	case MXT540E_WAITING_FRAME_DATA:
		val &= ~MXT540E_BOOT_STATUS_MASK;
		break;
	case MXT540E_FRAME_CRC_PASS:
		if (val == MXT540E_FRAME_CRC_CHECK)
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

static int mxt540e_unlock_bootloader(struct i2c_client *client)
{
	u8 buf[2];

	buf[0] = MXT540E_UNLOCK_CMD_LSB;
	buf[1] = MXT540E_UNLOCK_CMD_MSB;

	if (i2c_master_send(client, buf, 2) != 2) {
		dev_err(&client->dev, "%s: i2c send failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int mxt540e_fw_write(struct i2c_client *client,
				const u8 *data, unsigned int frame_size)
{
	if (i2c_master_send(client, data, frame_size) != frame_size) {
		dev_err(&client->dev, "%s: i2c send failed\n", __func__);
		return -EIO;
	}

	return 0;
}

static int mxt540e_load_fw(struct device *dev, const char *fn)
{
    struct mxt540e_data *data = copy_data;
    struct i2c_client *client = copy_data->client;
    struct firmware *fw = NULL;
    unsigned int frame_size;
    unsigned int pos = 0;
    int ret = 0;
    u16 obj_address=0;
    u16 size_one;
    u8 value;
    unsigned int object_register;
    int check_frame_crc_error = 0;
    int check_wating_frame_data_error = 0;

    printk("[TSP] mxt540e_load_fw start!!!\n");

    /* ret = request_firmware(&fw, fn, &client->dev);
	if (ret) {
		dev_err(dev, "Unable to open firmware %s\n", fn);
		printk(KERN_ERR "[TSP] Unable to open firmware %s\n", fn);
		return ret;
	}*/
	
    fw = kmalloc(sizeof(struct firmware), GFP_KERNEL);
    if(!fw) {
        printk("[TSP] mxt540e_load_fw kmalloc Failed !!!\n");
        return -ENOMEM;
    }
    
#if defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_KOR_MODEL_SHV_E160L)  || defined (CONFIG_JPN_MODEL_SC_05D)
    fw->size = ARRAY_SIZE(mXT540e__APP_V1_3_AA);
    fw->data = mXT540e__APP_V1_3_AA;
#else    
    fw->size = ARRAY_SIZE(mXT540e__APP_V1_2_AA);
    fw->data = mXT540e__APP_V1_2_AA;
#endif    

	/* Change to the bootloader mode */
	object_register = 0;
	value = (u8)MXT540E_BOOT_VALUE;
	ret = get_object_info(data, GEN_COMMANDPROCESSOR_T6, &size_one, &obj_address);
	if (ret)	{
		printk(KERN_ERR"[TSP] fail to get object_info\n");
        kfree(fw);
        fw = NULL;
		return ret;
	}
	size_one = 1;
	write_mem(data, obj_address+(u16)object_register, (u8)size_one, &value);
	msleep(MXT540E_SW_RESET_TIME);

	/* Change to slave address of bootloader */
	if (client->addr == MXT540E_APP_LOW)
		client->addr = MXT540E_BOOT_LOW;
	else
		client->addr = MXT540E_BOOT_HIGH;

	ret = mxt540e_check_bootloader(client, MXT540E_WAITING_BOOTLOAD_CMD);
	if (ret)
		goto out;

	/* Unlock bootloader */
	mxt540e_unlock_bootloader(client);

	while (pos < fw->size) {
		ret = mxt540e_check_bootloader(client,
						MXT540E_WAITING_FRAME_DATA);
		if (ret) {
			check_wating_frame_data_error++;
			if (check_wating_frame_data_error > 10) {
				printk(KERN_ERR"[TSP] firm update fail. wating_frame_data err\n");
				goto out;
			} else {
				printk("[TSP]check_wating_frame_data_error = %d, retry\n",
					check_wating_frame_data_error);
				continue;
			}
		}

		frame_size = ((*(fw->data + pos) << 8) | *(fw->data + pos + 1));

		/* We should add 2 at frame size as the the firmware data is not included the CRC bytes.	*/
		frame_size += 2;

		/* Write one frame to device */
		mxt540e_fw_write(client, fw->data + pos, frame_size);

		ret = mxt540e_check_bootloader(client,
						MXT540E_FRAME_CRC_PASS);
		if (ret) {
			check_frame_crc_error++;
			if (check_frame_crc_error > 10) {
				printk(KERN_ERR"[TSP] firm update fail. frame_crc err\n");
				goto out;
			} else {
				printk("[TSP]check_frame_crc_error = %d, retry\n",
					check_frame_crc_error);
				continue;
			}
		}

		pos += frame_size;

		dev_dbg(dev, "Updated %d bytes / %zd bytes\n", pos, fw->size);
		printk("[TSP] Updated %d bytes / %zd bytes\n", pos, fw->size);

		msleep(1);
	}

out:
    kfree(fw);
    fw = NULL;
    release_firmware(fw);

	/* Change to slave address of application */
	if (client->addr == MXT540E_BOOT_LOW)
		client->addr = MXT540E_APP_LOW;
	else
		client->addr = MXT540E_APP_HIGH;

    printk("[TSP] mxt540e_load_fw end !!!\n");
    return ret;
}

static int mxt540e_load_fw_bootmode(struct device *dev, const char *fn)
{
	struct mxt540e_data *data = copy_data;
	struct i2c_client *client = copy_data->client;
	struct firmware *fw = NULL;
	unsigned int frame_size;
	unsigned int pos = 0;
	int ret;
	int check_frame_crc_error = 0;
	int check_wating_frame_data_error = 0;

	printk("[TSP] mxt540e_load_fw_bootmode start!!!\n");

	fw = kmalloc(sizeof(struct firmware), GFP_KERNEL);
    if(!fw) {
        printk("[TSP] mxt540e_load_fw_bootmode kmalloc Failed !!!\n");
        return -ENOMEM;
    }
    
#if defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_KOR_MODEL_SHV_E160L)  || defined (CONFIG_JPN_MODEL_SC_05D)
    fw->size = ARRAY_SIZE(mXT540e__APP_V1_3_AA);
    fw->data = mXT540e__APP_V1_3_AA;
#else    
    fw->size = ARRAY_SIZE(mXT540e__APP_V1_2_AA);
    fw->data = mXT540e__APP_V1_2_AA;
#endif    

	/* Unlock bootloader */
	mxt540e_unlock_bootloader(client);

	while (pos < fw->size) {
		ret = mxt540e_check_bootloader(client, MXT540E_WAITING_FRAME_DATA);
		if (ret) {
			check_wating_frame_data_error++;
			if (check_wating_frame_data_error > 10) {
				printk(KERN_ERR"[TSP] firm update fail. wating_frame_data err\n");
				goto out;
			} else {
				printk("[TSP]check_wating_frame_data_error = %d, retry\n", check_wating_frame_data_error);
				continue;
			}
		}

		frame_size = ((*(fw->data + pos) << 8) | *(fw->data + pos + 1));

		/* We should add 2 at frame size as the the firmware data is not included the CRC bytes.	*/
		frame_size += 2;

		/* Write one frame to device */
		mxt540e_fw_write(client, fw->data + pos, frame_size);

		ret = mxt540e_check_bootloader(client, MXT540E_FRAME_CRC_PASS);
		if (ret) {
			check_frame_crc_error++;
			if (check_frame_crc_error > 10) {
				printk(KERN_ERR"[TSP] firm update fail. frame_crc err\n");
				goto out;
			} else {
				printk("[TSP]check_frame_crc_error = %d, retry\n", check_frame_crc_error);
				continue;
			}
		}

		pos += frame_size;

		dev_dbg(dev, "Updated %d bytes / %zd bytes\n", pos, fw->size);
		printk("[TSP] Updated %d bytes / %zd bytes\n", pos, fw->size);

		msleep(1);
	}

out:
    kfree(fw);
    fw = NULL;
    release_firmware(fw);

    /* Change to slave address of application */
    if (client->addr == MXT540E_BOOT_LOW)
        client->addr = MXT540E_APP_LOW;
    else
        client->addr = MXT540E_APP_HIGH;

    printk("[TSP] mxt540e_load_fw_bootmode end!!!\n");
    return ret;
}

static ssize_t set_refer0_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t mxt_reference=0;
	read_dbg_data(MXT_REFERENCE_MODE, test_node[0],&mxt_reference);
	return sprintf(buf, "%u\n", mxt_reference);
}

static ssize_t set_refer1_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t mxt_reference=0;
	read_dbg_data(MXT_REFERENCE_MODE, test_node[1], &mxt_reference);
	return sprintf(buf, "%u\n", mxt_reference);
}

static ssize_t set_refer2_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t mxt_reference=0;
	read_dbg_data(MXT_REFERENCE_MODE, test_node[2], &mxt_reference);
	return sprintf(buf, "%u\n", mxt_reference);
}

static ssize_t set_refer3_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t mxt_reference=0;
	read_dbg_data(MXT_REFERENCE_MODE, test_node[3], &mxt_reference);
	return sprintf(buf, "%u\n", mxt_reference);
}

static ssize_t set_refer4_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t mxt_reference=0;
	read_dbg_data(MXT_REFERENCE_MODE, test_node[4], &mxt_reference);
	return sprintf(buf, "%u\n", mxt_reference);
}

static ssize_t set_delta0_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t mxt_delta=0;
	read_dbg_data(MXT_DELTA_MODE, test_node[0], &mxt_delta);
	if (mxt_delta < 32767)
		return sprintf(buf, "%u\n", mxt_delta);
	else
		mxt_delta = 65535 - mxt_delta;

	if(mxt_delta) return sprintf(buf, "-%u\n", mxt_delta);
	else return sprintf(buf, "%u\n", mxt_delta);
}

static ssize_t set_delta1_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t mxt_delta=0;
	read_dbg_data(MXT_DELTA_MODE, test_node[1], &mxt_delta);
	if (mxt_delta < 32767)
		return sprintf(buf, "%u\n", mxt_delta);
	else
		mxt_delta = 65535 - mxt_delta;

	if(mxt_delta) return sprintf(buf, "-%u\n", mxt_delta);
	else return sprintf(buf, "%u\n", mxt_delta);
}

static ssize_t set_delta2_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t mxt_delta=0;
	read_dbg_data(MXT_DELTA_MODE, test_node[2], &mxt_delta);
	if (mxt_delta < 32767)
		return sprintf(buf, "%u\n", mxt_delta);
	else
		mxt_delta = 65535 - mxt_delta;

	if(mxt_delta) return sprintf(buf, "-%u\n", mxt_delta);
	else return sprintf(buf, "%u\n", mxt_delta);
}

static ssize_t set_delta3_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t mxt_delta=0;
	read_dbg_data(MXT_DELTA_MODE, test_node[3], &mxt_delta);
	if (mxt_delta < 32767)
		return sprintf(buf, "%u\n", mxt_delta);
	else
		mxt_delta = 65535 - mxt_delta;

	if(mxt_delta) return sprintf(buf, "-%u\n", mxt_delta);
	else return sprintf(buf, "%u\n", mxt_delta);
}

static ssize_t set_delta4_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	uint16_t mxt_delta=0;
	read_dbg_data(MXT_DELTA_MODE, test_node[4], &mxt_delta);
	if (mxt_delta < 32767)
		return sprintf(buf, "%u\n", mxt_delta);
	else
		mxt_delta = 65535 - mxt_delta;

	if(mxt_delta) return sprintf(buf, "-%u\n", mxt_delta);
	else return sprintf(buf, "%u\n", mxt_delta);
}

static ssize_t set_threshold_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", threshold);
}

static ssize_t set_all_refer_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int status = 0;

	status = read_all_data(MXT_REFERENCE_MODE);

	return sprintf(buf, "%u\n", status);
}


ssize_t set_tsp_for_drawing_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	u16 obj_address = 0;
	u16 size_one;
	int ret;
	u8 value;
	int jump_limit = 0;
	u8 val = 0;
	unsigned int register_address = 0;

	if (*buf == '1' && (!is_drawingmode)) {
		is_drawingmode = 1;
		jump_limit = 0;
		printk(KERN_ERR "[TSP] Set TSP drawing IN\n");

		ret = get_object_info(copy_data, TOUCH_MULTITOUCHSCREEN_T9, &size_one, &obj_address);
		register_address = 30;				
		value = (u8)jump_limit;
		size_one = 1;
		write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
		read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
		printk(KERN_ERR "T%d Byte%d is %d\n", 9, register_address, val);
	} else if (*buf == '0' && (is_drawingmode)) {
		is_drawingmode = 0;
		jump_limit  = 18;
		printk(KERN_ERR "[TSP] Set TSP drawing OUT\n");

		ret = get_object_info(copy_data, TOUCH_MULTITOUCHSCREEN_T9, &size_one, &obj_address);
		register_address = 30;				
		value = (u8)jump_limit;
		size_one = 1;
		write_mem(copy_data, obj_address+(u16)register_address, size_one, &value);
		read_mem(copy_data, obj_address+(u16)register_address, (u8)size_one, &val);
		printk(KERN_ERR "T%d Byte%d is %d\n", 9, register_address, val);
	}

	return 1;
}


static int index_reference;

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
    return sprintf(buf, "%u\n",  qt_refrence_node[index_reference]);
}

ssize_t disp_all_refdata_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	index_reference = atoi(buf);
	return size;
}

static ssize_t set_all_delta_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int status = 0;
	status = read_all_delta_data(MXT_DELTA_MODE);
	return sprintf(buf, "%u\n", status);
}

static int index_delta = 0;

ssize_t disp_all_deltadata_show(struct device *dev, struct device_attribute *attr, char *buf) // Jun : Diff from qt602...
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
	index_delta = atoi(buf);
	return size;
}

static ssize_t set_firm_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	return sprintf(buf, "%#02x\n", tsp_version_disp);

}

static ssize_t set_module_off_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt540e_data *data = copy_data;
	int count;

	mxt540e_enabled = 0;
	touch_is_pressed = 0;

	disable_irq(data->client->irq);
	mxt540e_internal_suspend(data);

	count = sprintf(buf, "tspoff\n");

	return count;
}

static ssize_t set_module_on_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt540e_data *data = copy_data;
	int count;

	bool ta_status = 0;

	mxt540e_internal_resume(data);
	enable_irq(data->client->irq);

	mxt540e_enabled = 1;

	if (data->read_ta_status) {
		data->read_ta_status(&ta_status);
		printk(KERN_ERR "[TSP] ta_status is %d", ta_status);
		mxt540e_ta_probe(ta_status);
	}
	calibrate_chip();

	count = sprintf(buf, "tspon\n");

	return count;
}

static ssize_t set_mxt_firm_update_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct mxt540e_data *data = dev_get_drvdata(dev);
	int error = 0;
	printk("[TSP] set_mxt_update_show start!!\n");
    
	if (*buf != 'S' && *buf != 'F') {
		printk(KERN_ERR "[TSP] Invalid values\n");
		dev_err(dev, "Invalid values\n");
		return -EINVAL;
	}
	
	disable_irq(data->client->irq);
	firm_status_data = 1;
	if (*buf != 'F' && data->tsp_version >= firmware_latest && data->tsp_build != build_latest) {
		printk(KERN_ERR"[TSP] mxt540E has latest firmware\n");
		firm_status_data =2;
		enable_irq(data->client->irq);
		return size;
	}
	printk("[TSP] mxt540E_fm_update\n");
	error = mxt540e_load_fw(dev, MXT540E_FW_NAME);
	
	if (error) {
		dev_err(dev, "The firmware update failed(%d)\n", error);
		firm_status_data =3;
		printk(KERN_ERR"[TSP]The firmware update failed(%d)\n", error);
		return error;
	} else {
		dev_dbg(dev, "The firmware update succeeded\n");
		firm_status_data =2;
		printk("[TSP] The firmware update succeeded\n");

		/* Wait for reset */
		msleep(MXT540E_SW_RESET_TIME);

		mxt540e_init_touch_driver(data);
	}

	enable_irq(data->client->irq);
	error = mxt540e_backup(data);
	if (error)
	{
		printk(KERN_ERR"[TSP]mxt540e_backup fail!!!\n");
		return error;
	}

	/* reset the touch IC. */
	error = mxt540e_reset(data);
	if (error)
	{
		printk(KERN_ERR"[TSP]mxt540e_reset fail!!!\n");
		return error;
	}

	msleep(MXT540E_SW_RESET_TIME);
	return size;
}

static ssize_t set_mxt_firm_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	int count;
	printk("Enter firmware_status_show by Factory command \n");

	if (firm_status_data == 1) {
		count = sprintf(buf, "DOWNLOADING\n");
	} else if (firm_status_data == 2) {
		count = sprintf(buf, "PASS\n");
	} else if (firm_status_data == 3) {
		count = sprintf(buf, "FAIL\n");
	} else
		count = sprintf(buf, "PASS\n");

	return count;
}

static ssize_t key_threshold_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", threshold);
}

static ssize_t key_threshold_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	/*TO DO IT*/
	unsigned int object_register=7;
	u8 value;
	u8 val;
	int ret;
	u16 address = 0;
	u16 size_one;
	int num;
	if (sscanf(buf, "%d", &num) == 1)
	{
		threshold = num;
		printk("threshold value %d\n",threshold);
		ret = get_object_info(copy_data, TOUCH_MULTITOUCHSCREEN_T9, &size_one, &address);
		size_one = 1;
		value = (u8)threshold;
		write_mem(copy_data, address+(u16)object_register, size_one, &value);
		read_mem(copy_data, address+(u16)object_register, (u8)size_one, &val);
		printk(KERN_ERR"T9 Byte%d is %d\n", object_register, val);
	}
	return size;
}

static ssize_t set_mxt_firm_version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	pr_info("Atmel Latest firmware version is %d\n", firmware_latest);
	return sprintf(buf, "%#02x\n", firmware_latest);
}

static ssize_t set_mxt_firm_version_read_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt540e_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%#02x\n", data->tsp_version);
}

static ssize_t set_mxt_config_version_read_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "1007\n");
}

static ssize_t mxt_touchtype_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char temp[15];

	sprintf(temp, "TSP : MXT540E\n");
	strcat(buf, temp);

	return strlen(buf);
}

#ifdef ITDEV //itdev

/* Functions for mem_access interface */
struct bin_attribute mem_access_attr;

static int mxt_read_block(struct i2c_client *client,
		   u16 addr,
		   u16 length,
		   u8 *value)
{
	struct i2c_adapter *adapter = client->adapter;
	struct i2c_msg msg[2];
	__le16	le_addr;
	struct mxt540e_data *mxt;

	mxt = i2c_get_clientdata(client);

	if (mxt != NULL) {
		if ((mxt->last_read_addr == addr) &&
			(addr == mxt->msg_proc_addr)) {
			if  (i2c_master_recv(client, value, length) == length)
			{
#ifdef ITDEV
                if (debug_enabled)//itdev
                    print_hex_dump(KERN_DEBUG, "MXT RX:", DUMP_PREFIX_NONE, 16, 1, value, length, false);//itdev
#endif 					
				return 0;
			}	
			else
				return -EIO;
		} else {
			mxt->last_read_addr = addr;
		}
	}

	le_addr = cpu_to_le16(addr);
	msg[0].addr  = client->addr;
	msg[0].flags = 0x00;
	msg[0].len   = 2;
	msg[0].buf   = (u8 *) &le_addr;

	msg[1].addr  = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = length;
	msg[1].buf   = (u8 *) value;
	if  (i2c_transfer(adapter, msg, 2) == 2)
	{	
#ifdef ITDEV
        if (debug_enabled) {//itdev
            print_hex_dump(KERN_DEBUG, "MXT TX:", DUMP_PREFIX_NONE, 16, 1, msg[0].buf, msg[0].len, false);//itdev
            print_hex_dump(KERN_DEBUG, "MXT RX:", DUMP_PREFIX_NONE, 16, 1, msg[1].buf, msg[1].len, false);//itdev
        }	
#endif        			
		return 0;
	}	
	else
		return -EIO;

}

/* Writes a block of bytes (max 256) to given address in mXT chip. */

int mxt_write_block(struct i2c_client *client,
		    u16 addr,
		    u16 length,
		    u8 *value)
{
	int i;
	struct {
		__le16	le_addr;
		u8	data[256];

	} i2c_block_transfer;

	struct mxt540e_data *mxt;

	if (length > 256)
		return -EINVAL;

	mxt = i2c_get_clientdata(client);
	if (mxt != NULL)
		mxt->last_read_addr = -1;



	for (i = 0; i < length; i++)
		i2c_block_transfer.data[i] = *value++;


	i2c_block_transfer.le_addr = cpu_to_le16(addr);

	i = i2c_master_send(client, (u8 *) &i2c_block_transfer, length + 2);

	if (i == (length + 2))
	{
#ifdef ITDEV
        if (debug_enabled)//itdev
            print_hex_dump(KERN_DEBUG, "MXT TX:", DUMP_PREFIX_NONE, 16, 1, &i2c_block_transfer, length+2, false);//itdev	
#endif
		return length;
	}	
	else
		return -EIO;
}


static ssize_t mem_access_read(struct file *filp, struct kobject *kobj,
                               struct bin_attribute *bin_attr,
                               char *buf, loff_t off, size_t count)
{
  int ret = 0;
  struct i2c_client *client;

  pr_info("mem_access_read p=%p off=%lli c=%zi\n", buf, off, count);

  if (off >= 32768)
    return -EIO;

  if (off + count > 32768)
    count = 32768 - off;

  if (count > 256)
    count = 256;

  if (count > 0)
  {
    client = to_i2c_client(container_of(kobj, struct device, kobj));
    ret = mxt_read_block(client, off, count, buf);
  }

  return ret >= 0 ? count : ret;
}

static ssize_t mem_access_write(struct file *filp, struct kobject *kobj,
                                  struct bin_attribute *bin_attr,
                                  char *buf, loff_t off, size_t count)
{
  int ret = 0;
  struct i2c_client *client;

  pr_info("mem_access_write p=%p off=%lli c=%zi\n", buf, off, count);

  if (off >= 32768)
    return -EIO;

  if (off + count > 32768)
    count = 32768 - off;

  if (count > 256)
    count = 256;

  if (count > 0) {
    client = to_i2c_client(container_of(kobj, struct device, kobj));
    ret = mxt_write_block(client, off, count, buf);
  }

  return ret >= 0 ? count : 0;
}

static ssize_t pause_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    int count = 0;

    count += sprintf(buf + count, "%d", driver_paused);
    count += sprintf(buf + count, "\n");

    return count;
}

static ssize_t pause_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    int i;
    if (sscanf(buf, "%u", &i) == 1 && i < 2) {
        driver_paused = i;

        printk("%s\n", i ? "paused" : "unpaused");
    } else {
        printk("pause_driver write error\n");
    }

    return count;
}

static ssize_t debug_enable_show(struct device *dev,
    struct device_attribute *attr, char *buf)
{
    int count = 0;

    count += sprintf(buf + count, "%d", debug_enabled);
    count += sprintf(buf + count, "\n");

    return count;
}

static ssize_t debug_enable_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    int i;
    if (sscanf(buf, "%u", &i) == 1 && i < 2) {
        debug_enabled = i;

        printk("%s\n", i ? "debug enabled" : "debug disabled");
    } else {
        printk("debug_enabled write error\n");
    }

    return count;
}

static ssize_t command_calibrate_store(struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    struct i2c_client *client;
    struct mxt540e_data   *mxt;
    int ret;

#if 0
    client = to_i2c_client(dev);
    mxt = i2c_get_clientdata(client);

    ret = mxt_write_byte(client,
        MXT_BASE_ADDR(MXT_GEN_COMMANDPROCESSOR_T6)
        + MXT_ADR_T6_CALIBRATE,
           0x1);
#else
	ret= calibrate_chip();
#endif          

    return (ret < 0) ? ret : count;
}

static ssize_t command_reset_store(struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    struct i2c_client *client;
    struct mxt540e_data   *mxt;
    int ret;

    client = to_i2c_client(dev);
    mxt = i2c_get_clientdata(client);

#if 0
    ret = reset_chip(mxt, RESET_TO_NORMAL);
#else
	ret = mxt540e_reset(mxt);
#endif
    return (ret < 0) ? ret : count;
}

static ssize_t command_backup_store(struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
 	struct i2c_client *client;
    struct mxt540e_data   *mxt;
    int ret;

    client = to_i2c_client(dev);
    mxt = i2c_get_clientdata(client);

#if 0
    ret = backup_to_nv(mxt);
#else
	ret = mxt540e_backup(mxt);
#endif

    return (ret < 0) ? ret : count;
}

#endif //itdev

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
static DEVICE_ATTR(set_all_refer,      S_IRUGO | S_IWUSR | S_IWGRP, set_all_refer_mode_show, NULL);
static DEVICE_ATTR(disp_all_refdata,   S_IRUGO | S_IWUSR | S_IWGRP, disp_all_refdata_show, disp_all_refdata_store);
static DEVICE_ATTR(set_all_delta,      S_IRUGO | S_IWUSR | S_IWGRP, set_all_delta_mode_show, NULL);
static DEVICE_ATTR(disp_all_deltadata, S_IRUGO | S_IWUSR | S_IWGRP, disp_all_deltadata_show, disp_all_deltadata_store);

static DEVICE_ATTR(set_threshold,    S_IRUGO | S_IWUSR | S_IWGRP, set_threshold_mode_show, NULL);
static DEVICE_ATTR(set_firm_version, S_IRUGO | S_IWUSR | S_IWGRP, set_firm_version_show, NULL);
static DEVICE_ATTR(set_module_off,   S_IRUGO | S_IWUSR | S_IWGRP, set_module_off_show, NULL);
static DEVICE_ATTR(set_module_on,    S_IRUGO | S_IWUSR | S_IWGRP, set_module_on_show, NULL);

static DEVICE_ATTR(tsp_firm_update,        S_IRUGO | S_IWUSR | S_IWGRP, NULL, set_mxt_firm_update_store);		  /* firmware update */
static DEVICE_ATTR(tsp_firm_update_status, S_IRUGO | S_IWUSR | S_IWGRP, set_mxt_firm_status_show, NULL);	      /* firmware update status return */
static DEVICE_ATTR(tsp_threshold,          S_IRUGO | S_IWUSR | S_IWGRP, key_threshold_show, key_threshold_store); /* touch threshold return, store */
static DEVICE_ATTR(tsp_firm_version_phone, S_IRUGO | S_IWUSR | S_IWGRP, set_mxt_firm_version_show, NULL);         /* PHONE*/	/* firmware version resturn in phone driver version */
static DEVICE_ATTR(tsp_firm_version_panel, S_IRUGO | S_IWUSR | S_IWGRP, set_mxt_firm_version_read_show, NULL);    /*PART*/	/* firmware version resturn in TSP panel version */
static DEVICE_ATTR(tsp_config_version,     S_IRUGO | S_IWUSR | S_IWGRP, set_mxt_config_version_read_show, NULL);    /*PART*/	/* firmware version resturn in TSP panel version */

static DEVICE_ATTR(mxt_touchtype, S_IRUGO | S_IWUSR | S_IWGRP,	mxt_touchtype_show, NULL);
static DEVICE_ATTR(object_show,   S_IRUGO | S_IWUSR | S_IWGRP, NULL, mxt540e_object_show);
static DEVICE_ATTR(object_write,  S_IRUGO | S_IWUSR | S_IWGRP, NULL, mxt540e_object_setting);
static DEVICE_ATTR(dbg_switch,    S_IRUGO | S_IWUSR | S_IWGRP, NULL, mxt540e_debug_setting);

static DEVICE_ATTR(set_tsp_for_drawing, S_IRUGO | S_IWUSR | S_IWGRP, NULL, set_tsp_for_drawing_store);

#ifdef ITDEV
/* Sysfs files for libmaxtouch interface */
static DEVICE_ATTR(pause_driver, 0666, pause_show, pause_store);//itdev
static DEVICE_ATTR(debug_enable, 0666, debug_enable_show, debug_enable_store);//itdev
static DEVICE_ATTR(command_calibrate, 0666, NULL, command_calibrate_store);//itdev
static DEVICE_ATTR(command_reset, 0666, NULL, command_reset_store);//itdev
static DEVICE_ATTR(command_backup, 0666, NULL, command_backup_store);//itdev

static struct attribute *libmaxtouch_attributes[] = {
	&dev_attr_pause_driver.attr,//itdev
	&dev_attr_debug_enable.attr,//itdev
    &dev_attr_command_calibrate.attr,//itdev
    &dev_attr_command_reset.attr,//itdev
    &dev_attr_command_backup.attr,//itdev
	NULL,
};

static struct attribute_group libmaxtouch_attr_group = {
    .attrs = libmaxtouch_attributes,
};
#endif

static struct attribute *mxt540e_attrs[] = {
	&dev_attr_object_show.attr,
	&dev_attr_object_write.attr,
	&dev_attr_dbg_switch.attr,
	NULL
};

static const struct attribute_group mxt540e_attr_group = {
	.attrs = mxt540e_attrs,
};

static int __devinit mxt540e_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct mxt540e_platform_data *pdata = client->dev.platform_data;
	struct mxt540e_data *data;
	struct input_dev *input_dev;
	int ret;
	int i;
	bool ta_status = 0;
	u8 **tsp_config;
	int retry = 5;

tsp_reinit:;
	input_dev = NULL;
	touch_is_pressed = 0;
    	printk("[TSP] mxt540e_probe START\n");
     
	if (!pdata) {
		dev_err(&client->dev, "missing platform data\n");
		return -ENODEV;
	}

	if (pdata->max_finger_touches <= 0)
		return -EINVAL;

	data = kzalloc(sizeof(*data) + pdata->max_finger_touches *
					sizeof(*data->fingers), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->num_fingers = pdata->max_finger_touches;
	data->power_on = pdata->power_on;
	data->power_off = pdata->power_off;
	data->register_cb = pdata->register_cb;
	data->read_ta_status = pdata->read_ta_status;

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

#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_KOR_MODEL_SHV_E160L) || defined(CONFIG_USA_MODEL_SGH_I717) || defined (CONFIG_JPN_MODEL_SC_05D)
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(MT_TOOL_FINGER, input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_mt_init_slots(input_dev, MAX_USING_FINGER_NUM);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->min_x, pdata->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->min_y, pdata->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, pdata->min_z, pdata->max_z, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, pdata->min_w, pdata->max_w, 0, 0);
#else	    
 	set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->min_x, pdata->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->min_y, pdata->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, pdata->min_z, pdata->max_z, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, pdata->min_w, pdata->max_w, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, data->num_fingers - 1, 0, 0);
#endif
#if defined (CONFIG_SHAPE_TOUCH)
	input_set_abs_params(input_dev, ABS_MT_COMPONENT, 0, 255, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_SUMSIZE, 0, 16*26, 0, 0);
#endif

 	ret = input_register_device(input_dev);
	if (ret) {
		input_free_device(input_dev);
		goto err_reg_dev;
	}

	data->gpio_read_done = pdata->gpio_read_done;
	data->power_on();
	msleep(130);

	copy_data = data;

	if (client->addr == MXT540E_APP_LOW)
		client->addr = MXT540E_BOOT_LOW;
	else
		client->addr = MXT540E_BOOT_HIGH;

	ret = mxt540e_check_bootloader(client, MXT540E_WAITING_BOOTLOAD_CMD);
	if (ret >= 0) {
		printk("[TSP] boot mode. firm update excute\n");
		mxt540e_load_fw_bootmode(NULL, MXT540E_FW_NAME);
		msleep(MXT540E_SW_RESET_TIME);
	} else {
		if (client->addr == MXT540E_BOOT_LOW)
			client->addr = MXT540E_APP_LOW;
		else
			client->addr = MXT540E_APP_HIGH;
	}

	data->register_cb(mxt540e_ta_probe);

	while(retry--){
	ret = mxt540e_init_touch_driver(data);

		if( ret == 0 || retry <= 0)
			break;

		printk(KERN_DEBUG "[TSP] chip initialization failed. retry(%d)\n", retry);

		set_lcd_esd_ignore(1);
		data->power_off();
		msleep(50);
		data->power_on();
		msleep(80);
		set_lcd_esd_ignore(0);		
	}

 	if (ret) {
		dev_err(&client->dev, "chip initialization failed\n");
		goto err_init_drv;
	}

 	if (data->family_id == 0xA1)  {	/* tsp_family_id - 0xA1 : MXT-540E */
		tsp_config = (u8 **)pdata->config_e;
		data->t48_config_batt_e = pdata->t48_config_batt_e;
		data->t48_config_chrg_e = pdata->t48_config_chrg_e;
		data->chrgtime_batt = pdata->chrgtime_batt;
		data->chrgtime_charging = pdata->chrgtime_charging;
		data->tchthr_batt = pdata->tchthr_batt;
		data->tchthr_charging = pdata->tchthr_charging;
		data->calcfg_batt_e = pdata->calcfg_batt_e;
		data->calcfg_charging_e = pdata->calcfg_charging_e;
		data->atchfrccalthr_e = pdata->atchfrccalthr_e;
		data->atchfrccalratio_e = pdata->atchfrccalratio_e;
		data->actvsyncsperx_batt = pdata->actvsyncsperx_batt;
		data->actvsyncsperx_charging = pdata->actvsyncsperx_charging;

		printk("[TSP] TSP chip is MXT540E\n");
		if( (data->tsp_version < firmware_latest) || (data->tsp_build != build_latest))
		{
			printk("[TSP] mxt540E force firmware update\n");
			if (mxt540e_load_fw(NULL, MXT540E_FW_NAME)) {
				printk(KERN_ERR"[TSP] firm update fail\n");
				goto err_config;
			} else {
				msleep(MXT540E_SW_RESET_TIME);
				mxt540e_init_touch_driver(data);
			}
		}
		INIT_DELAYED_WORK(&data->config_dwork, mxt_reconfigration_normal);
		INIT_DELAYED_WORK(&data->resume_check_dwork, resume_check_dworker);
		INIT_DELAYED_WORK(&data->cal_check_dwork, cal_check_dworker);
	}
	else  {
		printk(KERN_ERR"ERROR : There is no valid TSP ID\n");
		goto err_config;
	}

	for (i = 0; tsp_config[i][0] != RESERVED_T255; i++) {
		ret = init_write_config(data, tsp_config[i][0],
							tsp_config[i] + 1);
		if (ret)
			goto err_config;

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
	}

 	ret = mxt540e_backup(data);
	if (ret)
		goto err_backup;

	/* reset the touch IC. */
	ret = mxt540e_reset(data);
	if (ret)
		goto err_reset;

	msleep(MXT540E_SW_RESET_TIME);

	mxt540e_enabled = 1;

	if (data->read_ta_status) {
		data->read_ta_status(&ta_status);
		printk("[TSP] ta_status is %d\n", ta_status);
		mxt540e_ta_probe(ta_status);
	}
	check_resume_err = 2;
	calibrate_chip();
	schedule_delayed_work(&data->config_dwork, HZ*30);

	for (i = 0; i < data->num_fingers; i++) {
		data->fingers[i].state = MXT540E_STATE_INACTIVE;
	}

	ret = request_threaded_irq(client->irq, NULL, mxt540e_irq_thread,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, "mxt540e_ts", data);

	if (ret < 0)
		goto err_irq;

	ret = sysfs_create_group(&client->dev.kobj, &mxt540e_attr_group);
	if (ret)
		printk(KERN_ERR"[TSP] sysfs_create_group()is falled\n");

#ifdef ITDEV //itdev
		ret = sysfs_create_group(&client->dev.kobj, &libmaxtouch_attr_group);
		if (ret) {
			pr_err("Failed to create libmaxtouch sysfs group\n");
			goto err_irq;
		}
		
		sysfs_bin_attr_init(&mem_access_attr);
		mem_access_attr.attr.name = "mem_access";
		mem_access_attr.attr.mode = S_IRUGO | S_IWUGO;
		mem_access_attr.read = mem_access_read;
		mem_access_attr.write = mem_access_write;
		mem_access_attr.size = 65535;
	
	   if (sysfs_create_bin_file(&client->dev.kobj, &mem_access_attr) < 0) {
		   pr_err("Failed to create device file(%s)!\n", mem_access_attr.attr.name);
		   goto err_irq;
		}
#endif

	sec_touchscreen = device_create(sec_class, NULL, 0, NULL, "sec_touchscreen");
	dev_set_drvdata(sec_touchscreen, data);
	if (IS_ERR(sec_touchscreen))
		printk(KERN_ERR "[TSP] Failed to create device(sec_touchscreen)!\n");

	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_update) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_tsp_firm_update.attr.name);

	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_update_status) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_tsp_firm_update_status.attr.name);

	if (device_create_file(sec_touchscreen, &dev_attr_tsp_threshold) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_tsp_threshold.attr.name);

	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_version_phone) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_tsp_firm_version_phone.attr.name);

	if (device_create_file(sec_touchscreen, &dev_attr_tsp_firm_version_panel) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_tsp_firm_version_panel.attr.name);

	if (device_create_file(sec_touchscreen, &dev_attr_tsp_config_version) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_tsp_config_version.attr.name);

	if (device_create_file(sec_touchscreen, &dev_attr_mxt_touchtype) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_mxt_touchtype.attr.name);

	if (device_create_file(sec_touchscreen, &dev_attr_set_tsp_for_drawing) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_tsp_for_drawing.attr.name);



	mxt540e_noise_test = device_create(sec_class, NULL, 0, NULL, "tsp_noise_test");

	if (IS_ERR(mxt540e_noise_test))
		printk(KERN_ERR "[TSP] Failed to create device(mxt540e_noise_test)!\n");

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_refer0) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_refer0.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_delta0) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_delta0.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_refer1) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_refer1.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_delta1) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_delta1.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_refer2) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_refer2.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_delta2) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_delta2.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_refer3) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_refer3.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_delta3) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_delta3.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_refer4) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_refer4.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_delta4) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_delta4.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_all_refer) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_all_refer.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_disp_all_refdata)< 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_disp_all_refdata.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_all_delta) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_all_delta.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_disp_all_deltadata)< 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_disp_all_deltadata.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_threshold) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_threshold.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_firm_version) < 0)
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_firm_version.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_module_off) < 0)  
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_module_off.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_module_on) < 0)    
		printk(KERN_ERR "[TSP] Failed to create device file(%s)!\n", dev_attr_set_module_on.attr.name);
   
#ifdef CONFIG_HAS_EARLYSUSPEND
	//	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB+1;
	data->early_suspend.suspend = mxt540e_early_suspend;
	data->early_suspend.resume = mxt540e_late_resume;
	register_early_suspend(&data->early_suspend);
#endif
	return 0;

err_irq:
err_reset:
err_backup:
err_config:
	kfree(data->objects);
err_init_drv:
	gpio_free(data->gpio_read_done);
/* err_gpio_req:
	data->power_off();
	input_unregister_device(input_dev); */
err_reg_dev:
err_alloc_dev:
		kfree(data);
	return ret;
}

static int __devexit mxt540e_remove(struct i2c_client *client)
{
	struct mxt540e_data *data = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	free_irq(client->irq, data);
	kfree(data->objects);
	gpio_free(data->gpio_read_done);
	data->power_off();
	input_unregister_device(data->input_dev);
	kfree(data);

	return 0;
}

static struct i2c_device_id mxt540e_idtable[] = {
	{MXT540E_DEV_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, mxt540e_idtable);

static const struct dev_pm_ops mxt540e_pm_ops = {
	.suspend = mxt540e_suspend,
	.resume = mxt540e_resume,
};

static struct i2c_driver mxt540e_i2c_driver = {
	.id_table = mxt540e_idtable,
	.probe = mxt540e_probe,
	.remove = __devexit_p(mxt540e_remove),
	.driver = {
		.owner	= THIS_MODULE,
		.name	= MXT540E_DEV_NAME,
		.pm	= &mxt540e_pm_ops,
	},
};

#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

static int __init mxt540e_init(void)
{
#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return ENODEV!\n", __func__);
		return 0;
	}
#endif
	return i2c_add_driver(&mxt540e_i2c_driver);
}

static void __exit mxt540e_exit(void)
{
	i2c_del_driver(&mxt540e_i2c_driver);
}
module_init(mxt540e_init);
module_exit(mxt540e_exit);

MODULE_DESCRIPTION("Atmel MaXTouch 540E driver");
MODULE_AUTHOR("Heetae Ahn <heetae82.ahn@samsung.com>");
MODULE_LICENSE("GPL");
