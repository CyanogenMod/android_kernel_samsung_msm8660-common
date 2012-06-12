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
#include <linux/i2c/mxt540e.h>
#include <asm/unaligned.h>
#include <linux/firmware.h>

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
#define MXT540E_VER_10			0x10

/* Slave addresses */
#define MXT540E_APP_LOW		0x4C
#define MXT540E_APP_HIGH		0x4D
#define MXT540E_BOOT_LOW		0x26
#define MXT540E_BOOT_HIGH		0x27

/* FIRMWARE NAME */
#define MXT540E_FW_NAME	    "mXT540E.fw"

#define MXT540E_FWRESET_TIME		500	/* msec */
#define MXT540E_RESET_TIME		300	/* msec */

#define MXT540E_BOOT_VALUE		0xa5
#define MXT540E_BACKUP_VALUE		0x55

/* Bootloader mode status */
#define MXT540E_WAITING_BOOTLOAD_CMD	0xc0	/* valid 7 6 bit only */
#define MXT540E_WAITING_FRAME_DATA	0x80	/* valid 7 6 bit only */
#define MXT540E_FRAME_CRC_CHECK	0x02
#define MXT540E_FRAME_CRC_FAIL		0x03
#define MXT540E_FRAME_CRC_PASS		0x04
#define MXT540E_APP_CRC_FAIL		0x40	/* valid 7 8 bit only */
#define MXT540E_BOOT_STATUS_MASK	0x3f

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
	u8 calcfg_batt_e;
	u8 calcfg_charging_e;
	u8 atchfrccalthr_e;
	u8 atchfrccalratio_e;
	const u8 *t48_config_batt_e;
	const u8 *t48_config_chrg_e;
	struct delayed_work  config_dwork;
	void (*power_on)(void);
	void (*power_off)(void);
	void (*register_cb)(void *);
	void (*read_ta_status)(void *);
	int num_fingers;
	struct finger_info fingers[];
};

struct mxt540e_data *copy_data;
extern struct class *sec_class;
int touch_is_pressed;
EXPORT_SYMBOL(touch_is_pressed);
static int mxt540e_enabled;
static bool g_debug_switch;
static u8 threshold;
static int firm_status_data;
static bool deepsleep;
//static bool lock_status;

static int read_mem(struct mxt540e_data *data, u16 reg, u8 len, u8 *buf)
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

static int write_mem(struct mxt540e_data *data, u16 reg, u8 len, const u8 *buf)
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

static int get_object_info(struct mxt540e_data *data, u8 object_type, u16 *size,
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

static int write_config(struct mxt540e_data *data, u8 type, const u8 *cfg)
{
	int ret;
	u16 address = 0;
	u16 size = 0;

	ret = get_object_info(data, type, &size, &address);

	if(size ==0 && address == 0) return 0;
	else return write_mem(data, address, size, cfg);
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
	enable_irq(data->client->irq);
	return;
}

uint8_t calibrate_chip(void)
{
	u8 cal_data = 1;
	int ret = 0;
	/* send calibration command to the chip */
	ret = write_mem(copy_data, copy_data->cmd_proc + CMD_CALIBRATE_OFFSET, 1, &cal_data);
	/* set flag for calibration lockup recovery if cal command was successful */
	if (!ret)
	{
		printk("[TSP] calibration success!!!\n");
	}
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
	get_object_info(data, GEN_ACQUISITIONCONFIG_T8, &size, &obj_address);
	if (ta_status) {
		value = data->chrgtime_charging;
		error = write_mem(data, obj_address, 1, &value);
		get_object_info(data, PROCG_NOISESUPPRESSION_T48, &size, &obj_address);
		error |= write_config(data, data->t48_config_chrg_e[0], data->t48_config_chrg_e + 1);
		value = data->calcfg_charging_e | 0x20;
		threshold = data->tchthr_charging;
		error |= write_mem(data, obj_address+2, 1, &value);
	} else {
		value = data->chrgtime_batt;
		error = write_mem(data, obj_address, 1, &value);
		get_object_info(data, PROCG_NOISESUPPRESSION_T48, &size, &obj_address);
		error |= write_config(data, data->t48_config_batt_e[0], data->t48_config_batt_e + 1);
		value = data->calcfg_batt_e | 0x20;
		threshold = data->tchthr_batt;
		error |= write_mem(data, obj_address+2, 1, &value);
	}
	if (error < 0) printk(KERN_ERR "[TSP] mxt TA/USB mxt_noise_suppression_config Error!!\n");
	printk("[TSP] ta_status : %d", ta_status);
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

static int __devinit mxt540e_init_touch_driver(struct mxt540e_data *data)
{
	struct object_t *object_table;
	u32 read_crc = 0;
	u32 calc_crc;
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
	printk(KERN_ERR"family = %#02x, variant = %#02x, version "
			"= %#02x, build = %d\n", id[0], id[1], id[2], id[3]);
	dev_dbg(&data->client->dev, "matrix X size = %d\n", id[4]);
	dev_dbg(&data->client->dev, "matrix Y size = %d\n", id[5]);

	data->family_id = id[0];
	data->tsp_version = id[2];
	data->tsp_build = id[3];
	data->objects_len = id[6];

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

static void report_input_data(struct mxt540e_data *data)
{
	int i;
	int count = 0;

	for (i = 0; i < data->num_fingers; i++) {
		if (data->fingers[i].state == MXT540E_STATE_INACTIVE)
			continue;

		input_report_abs(data->input_dev, ABS_MT_POSITION_X, data->fingers[i].x);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y, data->fingers[i].y);
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, data->fingers[i].z);
		input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, data->fingers[i].w);
		input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, i);

#if defined(CONFIG_SHAPE_TOUCH)
		// input_report_abs(data->input_dev, ABS_MT_COMPONENT, data->fingers[i].component);
		input_report_abs(data->input_dev, 0x3b, data->fingers[i].component);
		/* printk(KERN_ERR"the component data is %d\n",data->fingers[i].component); */
#endif

		input_mt_sync(data->input_dev);

		if (data->fingers[i].state == MXT540E_STATE_PRESS || data->fingers[i].state == MXT540E_STATE_RELEASE) {
			printk(KERN_DEBUG "[TSP] id[%d],x=%d,y=%d,z=%d,w=%d\n",
				i , data->fingers[i].x, data->fingers[i].y, data->fingers[i].z, data->fingers[i].w);
		}
		if (data->fingers[i].state == MXT540E_STATE_RELEASE) {
			data->fingers[i].state = MXT540E_STATE_INACTIVE;
		}
		else {
			data->fingers[i].state = MXT540E_STATE_MOVE;
			count++;
		}
	}
	input_sync(data->input_dev);

	if (count) touch_is_pressed = 1;
	else touch_is_pressed = 0;

	data->finger_mask = 0;
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
	int error;
	
	do {
		touch_message_flag = 0;
		if (read_mem(data, data->msg_proc, sizeof(msg), msg))
			return IRQ_HANDLED;

		if (msg[0] == 0x1) {
			if (msg[1] == 0x00) /* normal mode */
			{
				printk("[TSP] normal mode\n");
			}
			if ((msg[1]&0x04) == 0x04) /* I2C checksum error */
			{
				printk("[TSP] I2C checksum error\n");
			}
			if ((msg[1]&0x10) == 0x08) /* config error */
			{
				printk("[TSP] config error\n");
			}
			if ((msg[1]&0x10) == 0x10) /* calibration */
			{
				printk("[TSP] calibration is on going\n");
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

		if ( msg[0] == 14) {
			if ((msg[1] & 0x01) == 0x00) { /* Palm release */
				printk("[TSP] palm touch released\n");
				touch_is_pressed = 0;
			} else if ((msg[1] & 0x01) == 0x01) { /* Palm Press */
				printk("[TSP] palm touch detected\n");
				touch_is_pressed = 1;
				touch_message_flag = 1;
			}
		}

		if ( msg[0] == 18) {
			if (msg[4] == 5) { /* Median filter error */
				printk("[TSP] Median filter Error\n");
				get_object_info(data, PROCG_NOISESUPPRESSION_T48, &size, &obj_address);
				value = data->calcfg_batt_e;
				error = write_mem(data, obj_address+2, 1, &value);
				msleep(5);
				value |= 0x20;
				error |= write_mem(data, obj_address+2, 1, &value);
				if(error) printk(KERN_ERR "[TSP] failed to reenable CHRGON\n");
			}
		}
		
		if (msg[0] > 1 && msg[0] < 12) {

			id = msg[0] - data->finger_type;

			/* If not a touch event, then keep going */
			if (id < 0 || id >= data->num_fingers){
					continue;
				}

			if (data->finger_mask & (1U << id))
				report_input_data(data);

			if (msg[1] & RELEASE_MSG_MASK) {
				data->fingers[id].z = 0;
				data->fingers[id].w = msg[5];
				data->finger_mask |= 1U << id;
				data->fingers[id].state = MXT540E_STATE_RELEASE;
			} else if ((msg[1] & DETECT_MSG_MASK) && (msg[1] &
					(PRESS_MSG_MASK | MOVE_MSG_MASK))) {
				touch_message_flag = 1;
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
	} while (!gpio_get_value(data->gpio_read_done));

	if (data->finger_mask)
		report_input_data(data);

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

	calibrate_chip();
	schedule_delayed_work(&data->config_dwork, HZ*5);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
#define mxt540e_suspend	NULL
#define mxt540e_resume	NULL

static void mxt540e_early_suspend(struct early_suspend *h)
{
	struct mxt540e_data *data = container_of(h, struct mxt540e_data,
								early_suspend);
	bool ta_status = 0;
	mxt540e_enabled = 0;
	touch_is_pressed = 0;

	if (data->read_ta_status) {
		data->read_ta_status(&ta_status);
		mxt540e_ta_probe(ta_status);
	}

	if (ta_status) {
		mxt540e_deepsleep(data);
	}

	if (!deepsleep) disable_irq(data->client->irq);
	mxt540e_internal_suspend(data);
}

static void mxt540e_late_resume(struct early_suspend *h)
{
	bool ta_status = 0;
	struct mxt540e_data *data = container_of(h, struct mxt540e_data,
								early_suspend);
	mxt540e_internal_resume(data);
	if (!deepsleep) enable_irq(data->client->irq);

	mxt540e_enabled = 1;
	if (data->read_ta_status) {
		data->read_ta_status(&ta_status);
		printk("[TSP] ta_status is %d\n", ta_status);
		if (!(deepsleep && ta_status)) mxt540e_ta_probe(ta_status);
	}
	if(deepsleep) deepsleep = 0;
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
	report_input_data(data);
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
static u8 firmware_latest =0x10; /* mxt540E : 0x10 */
static u8 build_latest = 0xAA;

struct device *mxt540e_noise_test;
/*
	botton_right, botton_left, center, top_right, top_left
*/
unsigned char test_node[5] = {12, 20, 104, 188, 196};

void diagnostic_chip(u8 mode)
{
	int error;
	u16 t6_address = 0;
	u16 size_one;
	int ret;
	u8 value;
	u16 t37_address = 0;

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

void read_dbg_data(uint8_t dbg_mode , uint8_t node, uint16_t *dbg_data)
{
	u8 read_page, read_point;
	uint8_t mode,page;
	u16 size;
	u16 diagnostic_addr = 0;

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

static int mxt540e_check_bootloader(struct i2c_client *client,
					unsigned int state)
{
	u8 val;

recheck:
	if (i2c_master_recv(client, &val, 1) != 1) {
		dev_err(&client->dev, "%s: i2c recv failed\n", __func__);
		return -EIO;
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
	const struct firmware *fw = NULL;
	unsigned int frame_size;
	unsigned int pos = 0;
	int ret;
	u16 obj_address=0;
	u16 size_one;
	u8 value;
	unsigned int object_register;

	printk("[TSP] mxt540e_load_fw start!!!\n");

	ret = request_firmware(&fw, fn, &client->dev);
	if (ret) {
		dev_err(dev, "Unable to open firmware %s\n", fn);
		printk(KERN_ERR "[TSP] Unable to open firmware %s\n", fn);
		return ret;
	}

	/* Change to the bootloader mode */
	object_register = 0;
	value = (u8)MXT540E_BOOT_VALUE;
	ret = get_object_info(data, GEN_COMMANDPROCESSOR_T6, &size_one, &obj_address);
	if (ret)
	{
		printk(KERN_ERR"[TSP] fail to get object_info\n");
		return ret;
	}
	size_one = 1;
	write_mem(data, obj_address+(u16)object_register, (u8)size_one, &value);
	msleep(MXT540E_RESET_TIME);

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
		if (ret)
			goto out;

		frame_size = ((*(fw->data + pos) << 8) | *(fw->data + pos + 1));

		/* We should add 2 at frame size as the the firmware data is not
		* included the CRC bytes.
		*/
		frame_size += 2;

		/* Write one frame to device */
		mxt540e_fw_write(client, fw->data + pos, frame_size);

		ret = mxt540e_check_bootloader(client,
						MXT540E_FRAME_CRC_PASS);
		if (ret)
			goto out;

		pos += frame_size;

		dev_dbg(dev, "Updated %d bytes / %zd bytes\n", pos, fw->size);
		printk("[TSP] Updated %d bytes / %zd bytes\n", pos, fw->size);
	}

out:
	release_firmware(fw);

	/* Change to slave address of application */
	if (client->addr == MXT540E_BOOT_LOW)
		client->addr = MXT540E_APP_LOW;
	else
		client->addr = MXT540E_APP_HIGH;

	return ret;
}

static ssize_t set_refer0_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt540e_data *data = copy_data;
	uint16_t mxt_reference=0;
	read_dbg_data(MXT_REFERENCE_MODE, test_node[0],&mxt_reference);
	mxt_reference -= 16384;
	return sprintf(buf, "%u\n", mxt_reference);
}

static ssize_t set_refer1_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt540e_data *data = copy_data;
	uint16_t mxt_reference=0;
	read_dbg_data(MXT_REFERENCE_MODE, test_node[1], &mxt_reference);
	mxt_reference -= 16384;
	return sprintf(buf, "%u\n", mxt_reference);
}

static ssize_t set_refer2_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt540e_data *data = copy_data;
	uint16_t mxt_reference=0;
	read_dbg_data(MXT_REFERENCE_MODE, test_node[2], &mxt_reference);
	mxt_reference -= 16384;
	return sprintf(buf, "%u\n", mxt_reference);
}

static ssize_t set_refer3_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt540e_data *data = copy_data;
	uint16_t mxt_reference=0;
	read_dbg_data(MXT_REFERENCE_MODE, test_node[3], &mxt_reference);
	mxt_reference -= 16384;
	return sprintf(buf, "%u\n", mxt_reference);
}

static ssize_t set_refer4_mode_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt540e_data *data = copy_data;
	uint16_t mxt_reference=0;
	read_dbg_data(MXT_REFERENCE_MODE, test_node[4], &mxt_reference);
	mxt_reference -= 16384;
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

static ssize_t set_mxt_firm_update_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct mxt540e_data *data = dev_get_drvdata(dev);
	int error = 0;
	printk("[TSP] set_mxt_update_show start!!\n");
	if (*buf != 'S' && *buf != 'F') {
		printk(KERN_ERR"Invalid values\n");
		dev_err(dev, "Invalid values\n");
		return -EINVAL;
	}
	
	disable_irq(data->client->irq);
	firm_status_data = 1;
	if (*buf != 'F' && data->tsp_version >= firmware_latest && data->tsp_build >= build_latest) {
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
		msleep(MXT540E_FWRESET_TIME);

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

	msleep(MXT540E_RESET_TIME);
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

static ssize_t key_threshold_store(struct device *dev, struct device_attribute *attr,
								   const char *buf, size_t size)
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
	struct mxt540e_data *data = dev_get_drvdata(dev);

	pr_info("Atmel Latest firmware version is %d\n", firmware_latest);
	return sprintf(buf, "%#02x\n", firmware_latest);
}

static ssize_t set_mxt_firm_version_read_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mxt540e_data *data = dev_get_drvdata(dev);
	return sprintf(buf, "%#02x\n", data->tsp_version);
}

static DEVICE_ATTR(set_refer0, S_IRUGO, set_refer0_mode_show, NULL);
static DEVICE_ATTR(set_delta0, S_IRUGO, set_delta0_mode_show, NULL);
static DEVICE_ATTR(set_refer1, S_IRUGO, set_refer1_mode_show, NULL);
static DEVICE_ATTR(set_delta1, S_IRUGO, set_delta1_mode_show, NULL);
static DEVICE_ATTR(set_refer2, S_IRUGO, set_refer2_mode_show, NULL);
static DEVICE_ATTR(set_delta2, S_IRUGO, set_delta2_mode_show, NULL);
static DEVICE_ATTR(set_refer3, S_IRUGO, set_refer3_mode_show, NULL);
static DEVICE_ATTR(set_delta3, S_IRUGO, set_delta3_mode_show, NULL);
static DEVICE_ATTR(set_refer4, S_IRUGO, set_refer4_mode_show, NULL);
static DEVICE_ATTR(set_delta4, S_IRUGO, set_delta4_mode_show, NULL);
static DEVICE_ATTR(set_threshold, S_IRUGO, set_threshold_mode_show, NULL);

static DEVICE_ATTR(tsp_firm_update, S_IWUSR | S_IWGRP, NULL, set_mxt_firm_update_store);		/* firmware update */
static DEVICE_ATTR(tsp_firm_update_status, S_IRUGO, set_mxt_firm_status_show, NULL);	/* firmware update status return */
static DEVICE_ATTR(tsp_threshold, S_IRUGO | S_IWUSR | S_IWGRP, key_threshold_show, key_threshold_store);	/* touch threshold return, store */
static DEVICE_ATTR(tsp_firm_version_phone, S_IRUGO, set_mxt_firm_version_show, NULL);/* PHONE*/	/* firmware version resturn in phone driver version */
static DEVICE_ATTR(tsp_firm_version_panel, S_IRUGO, set_mxt_firm_version_read_show, NULL);/*PART*/	/* firmware version resturn in TSP panel version */
static DEVICE_ATTR(object_show, S_IWUSR | S_IWGRP, NULL, mxt540e_object_show);
static DEVICE_ATTR(object_write, S_IWUSR | S_IWGRP, NULL, mxt540e_object_setting);
static DEVICE_ATTR(dbg_switch, S_IWUSR | S_IWGRP, NULL, mxt540e_debug_setting);


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

	touch_is_pressed = 0;

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

	set_bit(EV_ABS, input_dev->evbit);

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

#if defined (CONFIG_SHAPE_TOUCH)
	// input_set_abs_params(input_dev, ABS_MT_COMPONENT, 0, 255, 0, 0);
    input_set_abs_params(input_dev, 0x3b, 0, 255, 0, 0);
#endif
	ret = input_register_device(input_dev);
	if (ret) {
		input_free_device(input_dev);
		goto err_reg_dev;
	}

	data->gpio_read_done = pdata->gpio_read_done;

	data->power_on();

	ret = mxt540e_init_touch_driver(data);

	copy_data = data;

	data->register_cb(mxt540e_ta_probe);

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

		printk("[TSP] TSP chip is MXT540E\n");
		if (!(data->tsp_version >= firmware_latest && data->tsp_build >= build_latest)) {
			printk("[TSP] mxt540E force firmware update\n");
			if (mxt540e_load_fw(NULL, MXT540E_FW_NAME)) goto err_config;
			else {
				msleep(MXT540E_FWRESET_TIME);
				mxt540e_init_touch_driver(data);
			}
		}
		INIT_DELAYED_WORK(&data->config_dwork, mxt_reconfigration_normal);
	}
	else  {
		printk(KERN_ERR"ERROR : There is no valid TSP ID\n");
		goto err_config;
	}

	for (i = 0; tsp_config[i][0] != RESERVED_T255; i++) {
		ret = write_config(data, tsp_config[i][0],
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

	msleep(MXT540E_RESET_TIME);
	calibrate_chip();

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

	mxt540e_noise_test = device_create(sec_class, NULL, 0, NULL, "tsp_noise_test");

	if (IS_ERR(mxt540e_noise_test))
		printk(KERN_ERR "Failed to create device(mxt540e_noise_test)!\n");

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_refer0) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_refer0.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_delta0) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_delta0.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_refer1) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_refer1.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_delta1) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_delta1.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_refer2) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_refer2.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_delta2) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_delta2.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_refer3) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_refer3.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_delta3) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_delta3.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_refer4) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_refer4.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_delta4) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_delta4.attr.name);

	if (device_create_file(mxt540e_noise_test, &dev_attr_set_threshold) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_threshold.attr.name);

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = mxt540e_early_suspend;
	data->early_suspend.resume = mxt540e_late_resume;
	register_early_suspend(&data->early_suspend);
#endif
	schedule_delayed_work(&data->config_dwork, HZ*30);

	mxt540e_enabled = 1;

	if (data->read_ta_status) {
		data->read_ta_status(&ta_status);
		printk("[TSP] ta_status is %d\n", ta_status);
		mxt540e_ta_probe(ta_status);
	}

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

static int __init mxt540e_init(void)
{
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
