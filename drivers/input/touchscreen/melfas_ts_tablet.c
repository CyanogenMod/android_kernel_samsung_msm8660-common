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

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/melfas_ts_tablet.h>
#include <mach/gpio.h>
//CHJ #include <mach/gpio-sec.h>


#define TS_MAX_X_COORD 799
#define TS_MAX_Y_COORD 1279

#define TS_READ_START_ADDR 0x10
#define TS_READ_REGS_LEN 5
#define TS_WRITE_REGS_LEN 16
#define P5_MAX_TOUCH	10
#define P5_MAX_I2C_FAIL	50
#define P5_THRESHOLD 0x70

//#define I2C_RETRY_CNT	10

//#define PRESS_KEY	1
//#define RELEASE_KEY	0
//#define DEBUG_PRINT 	0

#define DEBUG_MODE		1
#define SET_DOWNLOAD_BY_GPIO	1
#define COOD_ROTATE_270
#define TSP_FACTORY_TEST	1
#define ENABLE_NOISE_TEST_MODE

#define REPORT_MT(touch_number, x, y, amplitude) \
do {     \
	input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);             \
	input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);             \
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, amplitude);         \
	input_report_abs(ts->input_dev, ABS_MT_PRESSURE, amplitude); \
} while (0)

#if SET_DOWNLOAD_BY_GPIO
#include "mcs8000_download_tablet.h"
#endif // SET_DOWNLOAD_BY_GPIO

int firmware_downloading = 0;
int tsp_id = 0;

struct melfas_ts_data {
	uint16_t addr;
	struct i2c_client *client; 
	struct input_dev *input_dev;
	struct work_struct  work;
	struct melfas_tsi_platform_data *pdata;
	struct early_suspend early_suspend;
	struct tsp_callbacks callbacks;
	uint32_t flags;
	bool charging_status;
	bool tsp_status;
	int (*power)(int on);
	void (*power_enable)(int en);
};

extern struct class *sec_class;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h);
static void melfas_ts_late_resume(struct early_suspend *h);
#endif

static struct muti_touch_info g_Mtouch_info[P5_MAX_TOUCH];
static bool debug_print = false;

#if DEBUG_MODE
static bool debug_on = false;
static tCommandInfo_t tCommandInfo[] = {
	{ '?', "Help" },
	{ 'T', "Go to LOGGING mode" },
	{ 'M', "Go to MTSI_1_2_0 mode" },
	{ 'R', "Toggle LOG ([R]awdata)" },
	{ 'F', "Toggle LOG (Re[f]erence)" },
	{ 'I', "Toggle LOG ([I]ntensity)" },
	{ 'G', "Toggle LOG ([G]roup Image)" },
	{ 'D', "Toggle LOG ([D]elay Image)" },
	{ 'P', "Toggle LOG ([P]osition)" },
	{ 'B', "Toggle LOG (De[b]ug)" },
	{ 'V', "Toggle LOG (Debug2)" },
	{ 'L', "Toggle LOG (Profi[l]ing)" },
	{ 'O', "[O]ptimize Delay" },
	{ 'N', "[N]ormalize Intensity" }
};

static bool vbLogType[LT_LIMIT] = {0, };
static const char mcLogTypeName[LT_LIMIT][20] = {
	"LT_DIAGNOSIS_IMG",
	"LT_RAW_IMG",
	"LT_REF_IMG",
	"LT_INTENSITY_IMG",
	"LT_GROUP_IMG",
	"LT_DELAY_IMG",
	"LT_POS",
	"LT_DEBUG",
	"LT_DEBUG2",
	"LT_PROFILING",
};

static void toggle_log(struct melfas_ts_data *ts, eLogType_t _eLogType);
static void print_command_list(void);
static int melfas_i2c_read(struct i2c_client *client, u16 addr, u16 length, u8 *value);

static void debug_i2c_read(struct i2c_client *client, u16 addr, u8 *value, u16 length)
{
	melfas_i2c_read(client, addr, length, value);
}

static int debug_i2c_write(struct i2c_client *client, u8 *value, u16 length)
{
	return i2c_master_send(client, value, length);
}

static void key_handler(struct melfas_ts_data *ts, char key_val)
{
	u8 write_buf[2];
//	u8 read_buf[2];
//	int try_cnt = 0;
	pr_info("[TSP] %s - %c\n", __func__, key_val);
	switch (key_val) {
	case '?':
	case '/':
		print_command_list();
		break;
	case 'T':
	case 't':
		write_buf[0] = ADDR_ENTER_LOGGING;
		write_buf[1] = 1;
		debug_i2c_write(ts->client, write_buf, 2);
		debug_on = true;
#if 0
		for ( try_cnt = 1; try_cnt - 1 < 10; try_cnt++) {
			msleep(100);
			/* verify the register was written */
			i2c_master_recv(ts->client, read_buf, 1);
			if (read_buf[0] == 72) {
				pr_info("[TSP] success - %c \n", key_val);
				break;
			} else {
				pr_info("[TSP] try again : val : %d , cnt : %d\n", read_buf[0], try_cnt);
				debug_i2c_write(ts->client, write_buf, 2);
			}
		}
#endif
		break;
	case 'M':
	case 'm':
		write_buf[0] = ADDR_CHANGE_PROTOCOL;
		write_buf[1] = 7;//PTC_STSI_1_0_0;              //0830 HASH
		debug_i2c_write(ts->client, write_buf, 2);
		debug_on = false;
		msleep(200);
		ts->power_enable(0);
		msleep(200);
		ts->power_enable(1);
		break;
	case 'R':
	case 'r':
		toggle_log(ts, LT_RAW_IMG);
		break;
	case 'F':
	case 'f':
		toggle_log(ts, LT_REF_IMG);
		break;
	case 'I':
	case 'i':
		toggle_log(ts, LT_INTENSITY_IMG);
		break;
	case 'G':
	case 'g':
		toggle_log(ts, LT_GROUP_IMG);
		break;
	case 'D':
	case 'd':
		toggle_log(ts, LT_DELAY_IMG);
		break;
	case 'P':
	case 'p':
		toggle_log(ts, LT_POS);
		break;
	case 'B':
	case 'b':
		toggle_log(ts, LT_DEBUG);
		break;
	case 'V':
	case 'v':
		toggle_log(ts, LT_DEBUG2);
		break;
	case 'L':
	case 'l':
		toggle_log(ts, LT_PROFILING);
		break;
	case 'O':
	case 'o':
		pr_info("Enter 'Optimize Delay' mode!!!\n");
		write_buf[0] = ADDR_CHANGE_OPMODE;
		write_buf[1] = OM_OPTIMIZE_DELAY;
		if (!debug_i2c_write(ts->client, write_buf, 2))
			goto ERROR_HANDLE;
		break;
	case 'N':
	case 'n':
		pr_info("Enter 'Normalize Intensity' mode!!!\n");
		write_buf[0] = ADDR_CHANGE_OPMODE;
		write_buf[1] = OM_NORMALIZE_INTENSITY;
		if (!debug_i2c_write(ts->client, write_buf, 2))
			goto ERROR_HANDLE;
		break;
	default:
		;
	}
	return;
ERROR_HANDLE:
	pr_info("ERROR!!! \n");
}

static void print_command_list(void)
{
	int i;
	pr_info("######################################################\n");
	for (i = 0; i < sizeof(tCommandInfo) / sizeof(tCommandInfo_t); i++) {
		pr_info("[%c]: %s\n", tCommandInfo[i].cCommand, tCommandInfo[i].sDescription);
	}
	pr_info("######################################################\n");
}

static void toggle_log(struct melfas_ts_data *ts, eLogType_t _eLogType)
{
	u8 write_buf[2];
	vbLogType[_eLogType] ^= 1;
	if (vbLogType[_eLogType]) {
		write_buf[0] = ADDR_LOGTYPE_ON;
		pr_info("%s ON\n", mcLogTypeName[_eLogType]);
	} else {
		write_buf[0] = ADDR_LOGTYPE_OFF;
		pr_info("%s OFF\n", mcLogTypeName[_eLogType]);
	}
	write_buf[1] = _eLogType;
	debug_i2c_write(ts->client, write_buf, 2);
}

static void logging_function(struct melfas_ts_data *ts)
{
	u8 read_buf[100];
	u8 read_mode, read_num;
	int FingerX, FingerY, FingerID;
	int i;
	static int past_read_mode = HEADER_NONE;
	static char *ps;
	static char s[500];

	debug_i2c_read(ts->client, LOG_READ_ADDR, read_buf, 2);

	read_mode = read_buf[0];
	read_num = read_buf[1];

	//pr_info("[TSP] read_mode : %d,  read_num : %d\n", read_mode, read_num);

	switch (read_mode) {
	case HEADER_U08://Unsigned character
	{
		unsigned char* p = (unsigned char*) &read_buf[2];
		i2c_master_recv(ts->client, read_buf, read_num + 2);
		ps = s;
		s[0] = '\0';

		for (i = 0; i < read_num - 1; i++)
		{
			sprintf(ps, "%4d,", p[i]);
			ps = s + strlen(s);
		}
		sprintf(ps, "%4d\n", p[i]);
		ps = s + strlen(s);
		printk(KERN_DEBUG "%s", s);
		break;
	}
	case HEADER_S08://Unsigned character
	{
		signed char* p = (signed char*) &read_buf[2];
		i2c_master_recv(ts->client, read_buf, read_num + 2);
		ps = s;
		s[0] = '\0';

		for (i = 0; i < read_num - 1; i++)
		{
			sprintf(ps, "%4d,", p[i]);
			ps = s + strlen(s);
		}
		sprintf(ps, "%4d\n", p[i]);
		ps = s + strlen(s);
		printk(KERN_DEBUG "%s", s);
		break;
	}
	case HEADER_U16://Unsigned short
	{
		unsigned short* p = (unsigned short*) &read_buf[2];
		i2c_master_recv(ts->client, read_buf, read_num * 2 + 2);
		if (past_read_mode != HEADER_U16_NOCR)
		{
			ps = s;
			s[0] = '\0';
		}

		for (i = 0; i < read_num - 1; i++)
		{
			sprintf(ps, "%5d,", p[i]);
			ps = s + strlen(s);
		}
		sprintf(ps, "%5d\n", p[i]);
		ps = s + strlen(s);
		printk(KERN_DEBUG "%s", s);
		break;
	}
	case HEADER_U16_NOCR:
	{
		unsigned short* p = (unsigned short*) &read_buf[2];
		i2c_master_recv(ts->client, read_buf, read_num * 2 + 2);

		if (past_read_mode != HEADER_U16_NOCR)
		{
			ps = s;
			s[0] = '\0';
		}
		for (i = 0; i < read_num; i++)
		{
			sprintf(ps, "%5d,", p[i]);
			ps = s + strlen(s);
		}
		break;
	}
	case HEADER_S16://Unsigned short
	{
		signed short* p = (signed short*) &read_buf[2];
		i2c_master_recv(ts->client, read_buf, read_num * 2 + 2);

		if (past_read_mode != HEADER_S16_NOCR)
		{
			ps = s;
			s[0] = '\0';
		}

		for (i = 0; i < read_num - 1; i++)
		{
			sprintf(ps, "%5d,", p[i]);
			ps = s + strlen(s);
		}
		sprintf(ps, "%5d\n", p[i]);
		ps = s + strlen(s);
		printk(KERN_DEBUG "%s", s);
		break;
	}
	case HEADER_S16_NOCR:
	{
		signed short* p = (signed short*) &read_buf[2];
		i2c_master_recv(ts->client, read_buf, read_num * 2 + 2);

		if (past_read_mode != HEADER_S16_NOCR)
		{
			ps = s;
			s[0] = '\0';
		}
		for (i = 0; i < read_num; i++)
		{
			sprintf(ps, "%5d,", p[i]);
			ps = s + strlen(s);
		}
		break;
	}
	case HEADER_U32://Unsigned short
	{
		unsigned long* p = (unsigned long*) &read_buf[2];
		i2c_master_recv(ts->client, read_buf, read_num * 4 + 4);

		if (past_read_mode != HEADER_U32_NOCR)
		{
			ps = s;
			s[0] = '\0';
		}

		for (i = 0; i < read_num - 1; i++)
		{
			sprintf(ps, "%10ld,", p[i]);
			ps = s + strlen(s);
		}
		sprintf(ps, "%10ld\n", p[i]);
		ps = s + strlen(s);
		printk(KERN_DEBUG "%s", s);
		break;
	}
	case HEADER_U32_NOCR://Unsigned short
	{
		unsigned long* p = (unsigned long*) &read_buf[2];
		i2c_master_recv(ts->client, read_buf, read_num * 4 + 4);

		if (past_read_mode != HEADER_U32_NOCR)
		{
			ps = s;
			s[0] = '\0';
		}
		for (i = 0; i < read_num; i++)
		{
			sprintf(ps, "%10ld,", p[i]);
			ps = s + strlen(s);
		}
		break;
	}
	case HEADER_S32://Unsigned short
	{
		signed long* p = (signed long*) &read_buf[2];
		i2c_master_recv(ts->client, read_buf, read_num * 4 + 4);

		if (past_read_mode != HEADER_S32_NOCR)
		{
			ps = s;
			s[0] = '\0';
		}

		for (i = 0; i < read_num - 1; i++)
		{
			sprintf(ps, "%10ld,", p[i]);
			ps = s + strlen(s);
		}
		sprintf(ps, "%10ld\n", p[i]);
		ps = s + strlen(s);
		printk(KERN_DEBUG "%s", s);
		break;
	}
	case HEADER_S32_NOCR://Unsigned short
	{
		signed long* p = (signed long*) &read_buf[2];
		i2c_master_recv(ts->client, read_buf, read_num * 4 + 4);

		if (past_read_mode != HEADER_S32_NOCR)
		{
			ps = s;
			s[0] = '\0';
		}
		for (i = 0; i < read_num; i++)
		{
			sprintf(ps, "%10ld,", p[i]);
			ps = s + strlen(s);
		}
		break;
	}
	case HEADER_TEXT://Text
	{
		i2c_master_recv(ts->client, read_buf, read_num + 2);

		ps = s;
		s[0] = '\0';

		for (i = 2; i < read_num + 2; i++)
		{
			sprintf(ps, "%c", read_buf[i]);
			ps = s + strlen(s);
		}
		printk(KERN_DEBUG "%s\n", s);
		break;
	}
	case HEADER_FINGER:
	{
		i2c_master_recv(ts->client, read_buf, read_num * 4 + 2);

		ps = s;
		s[0] = '\0';
		for (i = 2; i < read_num * 4 + 2; i = i + 4)
		{
			//log_printf( device_idx, " %5ld", read_buf[i]  , 0,0);
			FingerX = (read_buf[i + 1] & 0x07) << 8 | read_buf[i];
			FingerY = (read_buf[i + 3] & 0x07) << 8 | read_buf[i + 2];

			FingerID = (read_buf[i + 1] & 0xF8) >> 3;
			sprintf(ps, "%2d (%4d,%4d) | ", FingerID, FingerX, FingerY);
			ps = s + strlen(s);
		}
		printk(KERN_DEBUG "%s\n", s);
		break;
	}
	case HEADER_S12://Unsigned short
	{
		signed short* p = (signed short*) &read_buf[2];
		i2c_master_recv(ts->client, read_buf, read_num * 2 + 2);

		if (past_read_mode != HEADER_S12_NOCR)
		{
			ps = s;
			s[0] = '\0';
		}
		for (i = 0; i < read_num; i++)
		{
			if (p[i] > 4096 / 2)
			p[i] -= 4096;
		}

		for (i = 0; i < read_num - 1; i++)
		{
			sprintf(ps, "%5d,", p[i]);
			ps = s + strlen(s);
		}
		sprintf(ps, "%5d\n", p[i]);
		ps = s + strlen(s);
		printk(KERN_DEBUG "%s", s);
		break;
	}
	case HEADER_S12_NOCR:
	{
		signed short* p = (signed short*) &read_buf[2];
		i2c_master_recv(ts->client, read_buf, read_num * 2 + 2);

		if (past_read_mode != HEADER_S12_NOCR)
		{
			ps = s;
			s[0] = '\0';
		}
		for (i = 0; i < read_num; i++)
		{
			if (p[i] > 4096 / 2)
			p[i] -= 4096;
		}
		for (i = 0; i < read_num; i++)
		{
			sprintf(ps, "%5d,", p[i]);
			ps = s + strlen(s);
		}
		break;
	}
	case HEADER_PRIVATE://Unsigned character
	{
		unsigned char* p = (unsigned char*) &read_buf[2];
		i2c_master_recv(ts->client, read_buf, read_num + 2 + read_num % 2);

		ps = s;
		s[0] = '\0';
		sprintf(ps, "################## CUSTOM_PRIVATE LOG: ");
		ps = s + strlen(s);
		for (i = 0; i < read_num - 1; i++)
		{
			sprintf(ps, "%5d,", p[i]);
			ps = s + strlen(s);
		}
		sprintf(ps, "%5d\n", p[i]);
		ps = s + strlen(s);
		printk(KERN_DEBUG "%s", s);
		break;
	}
	default:
		break;
	}

	past_read_mode = read_mode;
}
#endif /* DEBUG_MODE */

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
	else{
		pr_err("[TSP] melfas_i2c_write error : [%d]",i);
		return -EIO;
	}
}

static void release_all_fingers(struct melfas_ts_data *ts)
{
	int i;
	for(i=0; i<P5_MAX_TOUCH; i++){
		if(-1 == g_Mtouch_info[i].strength){
			g_Mtouch_info[i].posX = 0;
			g_Mtouch_info[i].posY = 0;
			continue;
		}

		input_mt_slot(ts->input_dev, i);
		input_mt_report_slot_state(ts->input_dev,
			MT_TOOL_FINGER, 0);
		
		g_Mtouch_info[i].strength = 0;
		g_Mtouch_info[i].posX = 0;
		g_Mtouch_info[i].posY = 0;

		if(0 == g_Mtouch_info[i].strength)
			g_Mtouch_info[i].strength = -1;
	}
	input_sync(ts->input_dev);
}

static void reset_tsp(struct melfas_ts_data *ts)
{
	int	ret = 0;
	char	buf[2];
	
	buf[0] = 0x60;
	buf[1] = ts->charging_status;
	
	release_all_fingers(ts);
	
	ts->power_enable(0);
	msleep(700);
	ts->power_enable(1);
	msleep(500);
	
		pr_info("[TSP] TSP reset & TA/USB %sconnect\n",ts->charging_status?"":"dis");
	melfas_i2c_write(ts->client, (char *)buf, 2);
}

static int read_input_info(struct melfas_ts_data *ts, u8 *val)
{
	return melfas_i2c_read(ts->client, TS_READ_START_ADDR, TS_READ_REGS_LEN, val);
}

static int check_firmware_master(struct melfas_ts_data *ts, u8 *val)
{
	return melfas_i2c_read(ts->client, MCSTS_FIRMWARE_VER_REG_MASTER, 1, val);
}

static int check_firmware_slave(struct melfas_ts_data *ts, u8 *val)
{
	return melfas_i2c_read(ts->client, MCSTS_FIRMWARE_VER_REG_SLAVE, 1, val);
}
static int check_firmware_core(struct melfas_ts_data *ts, u8 *val)
{
	return melfas_i2c_read(ts->client, MCSTS_FIRMWARE_VER_REG_CORE, 1, val);
}

static int check_firmware_private(struct melfas_ts_data *ts, u8 *val)
{
	return melfas_i2c_read(ts->client, MCSTS_FIRMWARE_VER_REG_PRIVATE_CUSTOM, 1, val);
}

static int check_firmware_public(struct melfas_ts_data *ts, u8 *val)
{
	return melfas_i2c_read(ts->client, MCSTS_FIRMWARE_VER_REG_PUBLIC_CUSTOM, 1, val);
}

static int check_slave_boot(struct melfas_ts_data *ts, u8 *val)
{
	return melfas_i2c_read(ts->client, 0xb1, 1, val);
}

static int firmware_update(struct melfas_ts_data *ts)
{
	int i=0;
	u8 	val = 0;
	u8 	val_slv = 0;
	u8 	fw_ver = 0;
	int ret = 0;
	int ret_slv = 0;
	bool empty_chip = true;
	INT8 dl_enable_bit = 0x00;
	u8 version_info = 0;
	
#if (defined(CONFIG_TARGET_SERIES_P5LTE) || defined(CONFIG_TARGET_SERIES_P8LTE)) && defined(CONFIG_TARGET_LOCALE_KOR)
	if(system_rev < 0x07){
		pr_err("[TSP] P5LTE_KOR : This revision cannot TSP firmware update");
		return 0;
	}
#endif

#if !MELFAS_ISP_DOWNLOAD
	ret = check_firmware_master(ts, &val);
	ret_slv = check_firmware_slave(ts, &val_slv);

	for(i=0 ; i <5 ; i++)
	{
		if (ret || ret_slv) {
			pr_err("[TSP] ISC mode : Failed to check firmware version(ISP) : %d , %d : %0x0 , %0x0 \n",ret, ret_slv,val,val_slv);
			ret = check_firmware_master(ts, &val);
			ret_slv = check_firmware_slave(ts, &val_slv);			
		}else{
			empty_chip = false;
			break;
		}
	}
	if(MELFAS_CORE_FIRWMARE_UPDATE_ENABLE) 
	{
		check_firmware_core(ts,&version_info);
//		printk("<TSP> CORE_VERSION : 0x%2X\n",version_info);
		if(version_info < CORE_FW_VER || version_info==0xFF)
			dl_enable_bit |= 0x01;
	}
	if(MELFAS_PRIVATE_CONFIGURATION_UPDATE_ENABLE)
	{
		check_firmware_private(ts,&version_info);
//		printk("<TSP> PRIVATE_CUSTOM_VERSION : 0x%2X\n",version_info);
		if(version_info < PRIVATE_FW_VER || version_info==0xFF)
			dl_enable_bit |= 0x02;
	}
	if(MELFAS_PUBLIC_CONFIGURATION_UPDATE_ENABLE)
	{
		check_firmware_public(ts,&version_info);
//		printk("<TSP> PUBLIC_CUSTOM_VERSION : 0x%2X\n",version_info);
		if(version_info < PUBLIC_FW_VER || version_info==0xFF
			|| (tsp_id == 0 && (val < MELFAS_FW_VER || val > 0x50 ))
			|| (tsp_id == 1 && val < ILJIN_FW_VER ))
			dl_enable_bit |= 0x04;
	}
	pr_info("dl_enable_bit  = [0x%X],%s pennel",dl_enable_bit, tsp_id ? "ILJIN" : "MELFAS" );
//	dl_enable_bit = 0x07;
	
#endif
	
#if SET_DOWNLOAD_BY_GPIO
	disable_irq(ts->client->irq);
	/* enable gpio */

	gpio_tlmm_config(GPIO_CFG( 43, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG( 44, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_request(43, "TSP_SDA");
	gpio_request(44, "TSP_SCL");

	msleep(100);	//5000

#if MELFAS_ISP_DOWNLOAD
	ret = mcsdl_download_binary_data(tsp_id);
	if (ret)
		printk(KERN_ERR "SET Download Fail - error code [%d]\n", ret);			
#else

	/* (current version  < binary verion) */
	if (empty_chip || val != val_slv
			|| val < MELFAS_BASE_FW_VER
			|| (val > 0x50  && val < ILJIN_BASE_FW_VER) )
	{
		pr_info("[TSP] ISC mode enter but ISP mode download start : 0x%X, 0x%X\n",val,val_slv);
		mcsdl_download_binary_data(tsp_id); 	//ISP mode download
	}
	else
	{

		ret = mms100_ISC_download_binary_data(tsp_id, dl_enable_bit);
		if (ret)
		{
			pr_err("[TSP] SET Download ISC Fail - error code [%d]\n", ret);
			ret = mcsdl_download_binary_data(tsp_id); 	//ISP mode download 
			if (ret)
				pr_err("[TSP] SET Download ISC & ISP ALL Fail - error code [%d]\n", ret);
		}
	}
#endif

	gpio_free(43);
	gpio_free(44);

//	msleep(50);
	/* disable gpio */
//	gpio_tlmm_config(GPIO_CFG( 43, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_DISABLE);
//	gpio_tlmm_config(GPIO_CFG( 44, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_DISABLE);
		
	/* disable gpio */

#endif /* SET_DOWNLOAD_BY_GPIO */
//	msleep(100);

	/* reset chip */
	gpio_request(62, "TOUCH_EN");
	gpio_tlmm_config(GPIO_CFG(62, 0, GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
//	msleep(100);
	gpio_direction_output(62, 0);
	msleep(100);	//300
	gpio_direction_output(62, 1);    //0830 HASH

	ts->power_enable(0);
	msleep(200);

	ts->power_enable(1);
	msleep(100);

	enable_irq(ts->client->irq);
		
	return 0;

}

static void inform_charger_connection(struct tsp_callbacks *cb, int mode)
{
	struct melfas_ts_data *ts = container_of(cb, struct melfas_ts_data, callbacks);
	char buf[2];

	buf[0] = 0x60;
	buf[1] = !!mode;
	ts->charging_status = !!mode;

	if(ts->tsp_status){
		pr_info("[TSP] TA/USB is %sconnected\n", !!mode ? "" : "dis");
		melfas_i2c_write(ts->client, (char *)buf, 2);
	}
}

static void melfas_ts_work_func(struct melfas_ts_data *ts)
{
//	struct melfas_ts_data *ts = container_of(work, struct melfas_ts_data, work);
	int ret = 0, i;
	u8 buf[TS_READ_REGS_LEN];
	int touchType=0, touchState =0, touchID=0, posX=0, posY=0, strength=0, keyID = 0, reportID = 0;

#if DEBUG_MODE
	if (debug_on) {
		logging_function(ts);
		return ;
	}
#endif

	ret = read_input_info(ts, buf);
	if (ret < 0) {
		pr_err("[TSP] Failed to read the touch info\n");

		for(i=0; i<P5_MAX_I2C_FAIL; i++ )
			read_input_info(ts, buf);

		if(i == P5_MAX_I2C_FAIL){	// ESD Detection - I2c Fail
			pr_err("[TSP] Melfas_ESD I2C FAIL \n");
			reset_tsp(ts);
//			release_all_fingers(ts);
//			ts->power_enable(0);

//			msleep(700);
//			ts->power_enable(1);
		}

		return ;
	}
	else{
		touchType  = (buf[0]>>6)&0x03;
		touchState = (buf[0]>>4)&0x01;
		reportID = (buf[0]&0x0f);
#if defined(COOD_ROTATE_90)
		posY = ((buf[1]& 0x0F) << 8) | buf[2];
		posX  = ((buf[1]& 0xF0) << 4) | buf[3];
		posX = TS_MAX_Y_COORD - posX;
#elif defined(COOD_ROTATE_270)
		posY = ((buf[1]& 0x0F) << 8) | buf[2];
		posX  = ((buf[1]& 0xF0) << 4) | buf[3];
		posY = TS_MAX_X_COORD - posY;
#else
		posX = ((buf[1]& 0x0F) << 8) | buf[2];
		posY = ((buf[1]& 0xF0) << 4) | buf[3];
#endif
		keyID = strength = buf[4]; 	

		if(reportID == 0x0F) { // ESD Detection
			pr_info("[TSP] MELFAS_ESD Detection");
			reset_tsp(ts);
			return ;
		}
		else if(reportID == 0x0E)// abnormal Touch Detection
		{
			pr_info("[TSP] MELFAS abnormal Touch Detection");
			reset_tsp(ts);
			return ;
		}
		else if(reportID == 0x0D)// 	clear reference
		{
			pr_info("[TSP] MELFAS Clear Reference Detection");
			return ;
		}
		else if(reportID == 0x0C)//Impulse Noise TA
		{
			pr_info("[TSP] MELFAS Impulse Noise TA Detection");
			return ;
		}

		touchID = reportID-1;

		if (debug_print)
			pr_info("[TSP] reportID: %d\n", reportID);

		if(reportID > P5_MAX_TOUCH || reportID < 1) {
			pr_err("[TSP] Invalid touch id.\n");
			release_all_fingers(ts);
			return ;
		}

		if(touchType == TOUCH_SCREEN) {
			g_Mtouch_info[touchID].posX= posX;
			g_Mtouch_info[touchID].posY= posY;

			if(touchState) {
#if 0//def CONFIG_KERNEL_DEBUG_SEC
				if (0 >= g_Mtouch_info[touchID].strength)
					pr_info("[TSP] Press    - ID : %d  [%d,%d] WIDTH : %d",
						touchID,
						g_Mtouch_info[touchID].posX,
						g_Mtouch_info[touchID].posY,
						strength);
#else
				if (0 >= g_Mtouch_info[touchID].strength)
					pr_info("[TSP] P : %d\n", touchID);
#endif
				g_Mtouch_info[touchID].strength= (strength+1)/2;
			} else {
#if 0//def CONFIG_KERNEL_DEBUG_SEC
				if (g_Mtouch_info[touchID].strength)
					pr_info("[TSP] Release - ID : %d [%d,%d]",
						touchID,
						g_Mtouch_info[touchID].posX,
						g_Mtouch_info[touchID].posY);
#else
				if (g_Mtouch_info[touchID].strength)
					pr_info("[TSP] R : %d\n", touchID);

#endif
				g_Mtouch_info[touchID].strength = 0;
			}

			for(i=0; i<P5_MAX_TOUCH; i++) {
				if(g_Mtouch_info[i].strength== -1)
					continue;

				input_mt_slot(ts->input_dev, i);
				input_mt_report_slot_state(ts->input_dev,
					MT_TOOL_FINGER,
					!!g_Mtouch_info[i].strength);

				if(g_Mtouch_info[i].strength == 0)
					g_Mtouch_info[i].strength = -1;
				else
				REPORT_MT(i, g_Mtouch_info[i].posX,
						g_Mtouch_info[i].posY, g_Mtouch_info[i].strength);

			if (debug_print){
					pr_info("[TSP] Touch ID: %d, State : %d, x: %d, y: %d, z: %d\n",
						i, touchState, g_Mtouch_info[i].posX,
						g_Mtouch_info[i].posY, g_Mtouch_info[i].strength);
			}

			}
		}
		input_sync(ts->input_dev);
	}
}


#if 0	//START melfas_ts_work_func
static void melfas_ts_work_func(struct work_struct *work)
{
	struct melfas_ts_data *ts = container_of(work, struct melfas_ts_data, work);
	int ret = 0, i;
	uint8_t buf[TS_READ_REGS_LEN];
	int touchType=0, touchState =0, touchID=0, posX=0, posY=0, strength=0, keyID = 0, reportID = 0;

#if DEBUG_PRINT
	printk(KERN_ERR "[TSP] melfas_ts_work_func\n");

	if(ts ==NULL)
			printk(KERN_ERR "[TSP] melfas_ts_work_func : TS NULL\n");
#endif 


#if 0
	/** 
	SMBus Block Read:
		S Addr Wr [A] Comm [A]
				S Addr Rd [A] [Data] A [Data] A ... A [Data] NA P
	*/
	ret = i2c_smbus_read_i2c_block_data(ts->client, TS_READ_START_ADDR, TS_READ_REGS_LEN, buf);
	if (ret < 0)
	{
		printk(KERN_ERR "melfas_ts_work_func: i2c_smbus_read_i2c_block_data(), failed\n");
	}
#else
	/**
	Simple send transaction:
		S Addr Wr [A]  Data [A] Data [A] ... [A] Data [A] P
	Simple recv transaction:
		S Addr Rd [A]  [Data] A [Data] A ... A [Data] NA P
	*/

	buf[0] = TS_READ_START_ADDR;

	for(i=0; i<I2C_RETRY_CNT; i++)
	{
		ret = i2c_master_send(ts->client, buf, 1);
#if DEBUG_PRINT
		printk(KERN_ERR "[TSP] melfas_ts_work_func : i2c_master_send [%d]\n", ret);
#endif 
		if(ret >=0)
		{
			ret = i2c_master_recv(ts->client, buf, TS_READ_REGS_LEN);
#if DEBUG_PRINT			
			printk(KERN_ERR "[TSP] melfas_ts_work_func : i2c_master_recv [%d]\n", ret);			
#endif


			if(ret >=0)
			{
				break; // i2c success
			}
		}
	}
#endif

//	printk("[TSP] ====<%s>==========================ln: %d \n",__func__, __LINE__);

	if (ret < 0)
	{
		printk(KERN_ERR "[TSP] melfas_ts_work_func: i2c failed\n");
		return ;	
	}
	else 
	{
		touchType  = (buf[0]>>6)&0x03;
		touchState = (buf[0]>>4)&0x01;
		reportID = (buf[0]&0x0f);
		posX = (  ( buf[1]& 0x0F) << (8)  ) +  buf[2];
		posY = (( (buf[1]& 0xF0 ) >> 4 ) << (8)) +  buf[3];		
		keyID = strength = buf[4]; 	
		
		touchID = reportID-1;

		if(touchID > P5_MAX_TOUCH-1)	//ESD
		{
#if DEBUG_PRINT
			printk(KERN_ERR "[TSP] melfas_ts_work_func: Touch ID: %d\n",  touchID);
#endif			
//			release_all_fingers(ts);
			disable_irq(ts->client->irq);
			release_all_fingers(ts);

			ts->power_enable(0);
			msleep(300);
			printk("[TSP] TSP RESET\n");

			ts->power_enable(1);

			return ;
		}
		
		if(touchType == TOUCH_SCREEN)
		{
			g_Mtouch_info[touchID].posX= posX;
			g_Mtouch_info[touchID].posY= posY;

			if(touchState)
				g_Mtouch_info[touchID].strength= strength;
			else
				g_Mtouch_info[touchID].strength = 0;


			for(i=0; i<P5_MAX_TOUCH; i++)
			{
				if(g_Mtouch_info[i].strength== -1)
					continue;
				
				input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, i);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_X, g_Mtouch_info[i].posX);
				input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, g_Mtouch_info[i].posY);
				input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, g_Mtouch_info[i].strength );
				input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 5);      				
				input_mt_sync(ts->input_dev);          
#if DEBUG_PRINT
		printk(KERN_ERR "[TSP] melfas_ts_work_func: Touch ID: %d, State : %d, x: %d, y: %d, z: %d\n", i, touchState, g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].strength);
#endif					
				if(g_Mtouch_info[i].strength == 0)
					g_Mtouch_info[i].strength = -1;
				
			}
			
		}
		else if(touchType == TOUCH_KEY)
		{
			if (keyID == 0x1)
				input_report_key(ts->input_dev, KEY_MENU, touchState ? PRESS_KEY : RELEASE_KEY);		
			if (keyID == 0x2)
				input_report_key(ts->input_dev, KEY_HOME, touchState ? PRESS_KEY : RELEASE_KEY);
			if (keyID == 0x4)
				input_report_key(ts->input_dev, KEY_BACK, touchState ? PRESS_KEY : RELEASE_KEY);
#if DEBUG_PRINT
		printk(KERN_ERR "[TSP] melfas_ts_work_func: keyID : %d, touchState: %d\n", keyID, touchState);
#endif				
		}
		input_sync(ts->input_dev);
	}
			
	enable_irq(ts->client->irq);
}
#endif	//END melfas_ts_work_func



static irqreturn_t melfas_ts_irq_handler(int irq, void *handle)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)handle;
	if (debug_print)
		printk(KERN_ERR "[TSP] mefas_ts_irq_handler\n");

	melfas_ts_work_func(ts);
	
	return IRQ_HANDLED;
}

static ssize_t show_firmware_dev(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 ver = 0;

	check_firmware_master(ts, &ver);

	if (!tsp_id)
		return snprintf(buf, PAGE_SIZE,	"MEL_%Xx%d\n", ver, 0x0);
	else
		return snprintf(buf, PAGE_SIZE,	"ILJ_%Xx%d\n", ver, 0x0);
}

static ssize_t store_firmware(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t count)
{
	int i ;
	struct melfas_ts_data *ts = dev_get_drvdata(dev);

	if (sscanf(buf, "%d", &i) != 1)
		return -EINVAL;

#if SET_DOWNLOAD_BY_GPIO	//HASH 0831
	disable_irq(ts->client->irq);
	/* enable gpio */

	gpio_tlmm_config(GPIO_CFG( 43, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG( 44, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_request(43, "TSP_SDA");
	gpio_request(44, "TSP_SCL");

	firmware_downloading = 1;
	msleep(500);	//5000
	
	mcsdl_download_binary_file();
	firmware_downloading = 0;

	gpio_free(43);
	gpio_free(44);


	msleep(100);
	/* reset chip */
	gpio_request(62, "TOUCH_EN");
	gpio_tlmm_config(GPIO_CFG(62, 0, GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	msleep(100);
	gpio_direction_output(62, 0);
	msleep(200);	//300
	gpio_direction_output(62, 1);    //0830 HASH

	ts->power_enable(0);
	msleep(200);
	ts->power_enable(1);
	msleep(100);      //0830 HASH

	enable_irq(ts->client->irq);
#endif

	return count;
}

static ssize_t show_firmware_bin(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	if (!tsp_id)
		return snprintf(buf, PAGE_SIZE,	"MEL_%Xx%d\n",
					MELFAS_FW_VER, 0x0);
	else
		return snprintf(buf, PAGE_SIZE,	"ILJ_%Xx%d\n",
					ILJIN_FW_VER, 0x0);
}
static ssize_t show_firmware_bin_slv(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 val_slv = 0;
	int ret_slv = 0;
	ret_slv = check_firmware_slave(ts, &val_slv);
	if (ret_slv) {
		pr_err("[TSP] Failed to check firmware slave version : %d \n",ret_slv);
	}
	if (!tsp_id)
		return snprintf(buf, PAGE_SIZE,	"MEL_%Xx%d\n",
					val_slv, 0x0);
	else
		return snprintf(buf, PAGE_SIZE,	"ILJ_%Xx%d\n",
					val_slv, 0x0);
}
static ssize_t show_firmware_bin_detail(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);

	u8 	val_core 	= 0;
	u8 	val_private = 0;
	u8 	val_public 	= 0;
	int ret_core 	= 0;
	int ret_private = 0;
	int ret_public 	= 0;

	ret_core 	= check_firmware_core(ts, &val_core);
	ret_private = check_firmware_private(ts, &val_private);
	ret_public 	= check_firmware_public(ts, &val_public);


	if (!tsp_id)
		return snprintf(buf, PAGE_SIZE,	"MEL_%Xx%Xx%X\n",
					val_core, val_private, val_public);
	else
		return snprintf(buf, PAGE_SIZE,	"ILJ_%Xx%Xx%X\n",
					val_core, val_private, val_public);
}

static ssize_t store_debug_mode(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t count)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	char ch;

	if (sscanf(buf, "%c", &ch) != 1)
		return -EINVAL;

	key_handler(ts, ch);

	return count;
}

static ssize_t show_debug_mode(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	return sprintf(buf, debug_on ? "ON\n" : "OFF\n");
}

static ssize_t store_debug_log(struct device *dev,
						struct device_attribute *attr,
						const char *buf,
						size_t count)
{
	int i;

	if (sscanf(buf, "%d", &i) != 1)
		return -EINVAL;

	if (i)
		debug_print = true;
	else
		debug_print = false;

	return count;
}

static ssize_t show_threshold(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct melfas_ts_data *ts = dev_get_drvdata(dev);
	u8 threshold;
	melfas_i2c_read(ts->client, P5_THRESHOLD, 1, &threshold);

	return sprintf(buf, "%d\n", threshold);
}

static DEVICE_ATTR(fw_dev, S_IWUSR|S_IRUGO, show_firmware_dev, store_firmware);
static DEVICE_ATTR(fw_bin, S_IRUGO, show_firmware_bin, NULL);
static DEVICE_ATTR(fw_bin_slv, S_IRUGO, show_firmware_bin_slv, NULL);
static DEVICE_ATTR(fw_bin_detail, S_IRUGO, show_firmware_bin_detail, NULL);
static DEVICE_ATTR(debug_mode, S_IWUSR|S_IRUGO, show_debug_mode, store_debug_mode);
static DEVICE_ATTR(debug_log, S_IWUSR|S_IRUGO, NULL, store_debug_log);
#ifdef ENABLE_NOISE_TEST_MODE
static DEVICE_ATTR(set_threshould, S_IRUGO, show_threshold, NULL);
#else
static DEVICE_ATTR(threshold, S_IRUGO, show_threshold, NULL);
#endif

#if TSP_FACTORY_TEST
static u16 inspection_data[1134] = { 0, };
#ifdef DEBUG_LOW_DATA
static u16 low_data[1134] = { 0, };
#endif
static u16 lntensity_data[1134] = { 0, };
static void check_debug_data(struct melfas_ts_data *ts)
{
	u8 write_buffer[6];
	u8 read_buffer[2];
	int sensing_line, exciting_line;

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
	while (gpio_get_value(125)) {
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
	for (sensing_line = 0; sensing_line < 27; sensing_line++) {
		for (exciting_line =0; exciting_line < 42; exciting_line++) {
			write_buffer[2] = exciting_line;
			write_buffer[3] = sensing_line;
			melfas_i2c_write(ts->client, (char *)write_buffer, 6);
			melfas_i2c_read(ts->client, 0xA8, 2, read_buffer);
			inspection_data[exciting_line + sensing_line * 42] =
				(read_buffer[1] & 0xf) << 8 | read_buffer[0];
		}
	}
#ifdef DEBUG_LOW_DATA
	if (debug_print)
		pr_info("[TSP] read low data\n");
	write_buffer[5] = 0x03;
	for (sensing_line = 0; sensing_line < 27; sensing_line++) {
		for (exciting_line =0; exciting_line < 42; exciting_line++) {
			write_buffer[2] = exciting_line;
			write_buffer[3] = sensing_line;
			melfas_i2c_write(ts->client, (char *)write_buffer, 6);
			melfas_i2c_read(ts->client, 0xA8, 2, read_buffer);
			low_data[exciting_line + sensing_line * 42] =
				(read_buffer[1] & 0xf) << 8 | read_buffer[0];
		}
	}
#endif
	release_all_fingers(ts);
	ts->power_enable(0);

	msleep(200);
	ts->power_enable(1);
	enable_irq(ts->client->irq);
}

static ssize_t all_refer_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int status = 0;
	int i;
	struct melfas_ts_data *ts  = dev_get_drvdata(dev);

	check_debug_data(ts);

	if (debug_print)
		pr_info("[TSP] inspection data\n");
	for (i = 0; i < 1134; i++) {
		/* out of range */
		if (inspection_data[i] < 30) {
			status = 1;
			break;
		}

		if (debug_print) {
			if (0 == i % 27)
				printk("\n");
			printk("%5u  ", inspection_data[i]);
		}
	}

#if DEBUG_LOW_DATA
	pr_info("[TSP] low data\n");
	for (i = 0; i < 1134; i++) {
		if (0 == i % 27)
			printk("\n");
		printk("%5u  ", low_data[i]);
	}
#endif
	return sprintf(buf, "%u\n", status);
}

static void check_intesity_data(struct melfas_ts_data *ts)
{

	u8 write_buffer[6];
	u8 read_buffer[2];
	int sensing_line, exciting_line;

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
		while (gpio_get_value(125)) {
			printk(".");
			udelay(100);
		}

		/* read the dummy data */
		melfas_i2c_read(ts->client, 0xA8, 2, read_buffer);

		write_buffer[5] = 0x02;
		for (sensing_line = 0; sensing_line < 27; sensing_line++) {
			for (exciting_line =0; exciting_line < 42; exciting_line++) {
				write_buffer[2] = exciting_line;
				write_buffer[3] = sensing_line;
				melfas_i2c_write(ts->client, (char *)write_buffer, 6);
				melfas_i2c_read(ts->client, 0xA8, 2, read_buffer);
				inspection_data[exciting_line + sensing_line * 42] =
					(read_buffer[1] & 0xf) << 8 | read_buffer[0];
			}
		}

		release_all_fingers(ts);
		ts->power_enable(0);
		msleep(700);
		ts->power_enable(1);
	}

	write_buffer[0] = 0xA0;
	write_buffer[1] = 0x1A;
	write_buffer[4] = 0x0;
	write_buffer[5] = 0x04;
	for (sensing_line = 0; sensing_line < 27; sensing_line++) {
		for (exciting_line =0; exciting_line < 42; exciting_line++) {
			write_buffer[2] = exciting_line;
			write_buffer[3] = sensing_line;
			melfas_i2c_write(ts->client, (char *)write_buffer, 6);
			melfas_i2c_read(ts->client, 0xA8, 2, read_buffer);
			lntensity_data[exciting_line + sensing_line * 42] =
				(read_buffer[1] & 0xf) << 8 | read_buffer[0];
		}
	}
#if 1
	pr_info("[TSP] lntensity data");
	int i;
	for (i = 0; i < 1134; i++) {
		if (0 == i % 27)
			printk("\n");
		printk("%5u  ", lntensity_data[i]);
	}
#endif

}

static ssize_t set_refer0_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 refrence = 0;
	struct melfas_ts_data *ts = dev_get_drvdata(dev);

	check_intesity_data(ts);

	refrence = inspection_data[927];
	return sprintf(buf, "%u\n", refrence);
}

static ssize_t set_refer1_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 refrence = 0;
	refrence = inspection_data[172];
	return sprintf(buf, "%u\n", refrence);
}

static ssize_t set_refer2_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 refrence = 0;
	refrence = inspection_data[608];
	return sprintf(buf, "%u\n", refrence);
}

static ssize_t set_refer3_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 refrence = 0;
	refrence = inspection_data[1003];
	return sprintf(buf, "%u\n", refrence);
}

static ssize_t set_refer4_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 refrence = 0;
	refrence = inspection_data[205];
	return sprintf(buf, "%u\n", refrence);
}

static ssize_t set_intensity0_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 intensity = 0;
	intensity = lntensity_data[927];
	return sprintf(buf, "%u\n", intensity);
}

static ssize_t set_intensity1_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 intensity = 0;
	intensity = lntensity_data[172];
	return sprintf(buf, "%u\n", intensity);
}

static ssize_t set_intensity2_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 intensity = 0;
	intensity = lntensity_data[608];
	return sprintf(buf, "%u\n", intensity);
}

static ssize_t set_intensity3_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 intensity = 0;
	intensity = lntensity_data[1003];
	return sprintf(buf, "%u\n", intensity);
}

static ssize_t set_intensity4_mode_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	u16 intensity = 0;
	intensity = lntensity_data[205];
	return sprintf(buf, "%u\n", intensity);
}

/* noise test */
static DEVICE_ATTR(set_all_refer, S_IRUGO, all_refer_show, NULL);
static DEVICE_ATTR(set_refer0, S_IRUGO, set_refer0_mode_show, NULL);
static DEVICE_ATTR(set_delta0, S_IRUGO, set_intensity0_mode_show, NULL);
static DEVICE_ATTR(set_refer1, S_IRUGO, set_refer1_mode_show, NULL);
static DEVICE_ATTR(set_delta1, S_IRUGO, set_intensity1_mode_show, NULL);
static DEVICE_ATTR(set_refer2, S_IRUGO, set_refer2_mode_show, NULL);
static DEVICE_ATTR(set_delta2, S_IRUGO, set_intensity2_mode_show, NULL);
static DEVICE_ATTR(set_refer3, S_IRUGO, set_refer3_mode_show, NULL);
static DEVICE_ATTR(set_delta3, S_IRUGO, set_intensity3_mode_show, NULL);
static DEVICE_ATTR(set_refer4, S_IRUGO, set_refer4_mode_show, NULL);
static DEVICE_ATTR(set_delta4, S_IRUGO, set_intensity4_mode_show, NULL);
#endif

static struct attribute *sec_touch_attributes[] = {
	&dev_attr_fw_dev.attr,
	&dev_attr_fw_bin.attr,
	&dev_attr_fw_bin_slv.attr,
	&dev_attr_fw_bin_detail.attr,
	&dev_attr_debug_mode.attr,
	&dev_attr_debug_log.attr,
#ifndef ENABLE_NOISE_TEST_MODE
	&dev_attr_threshold.attr,
	&dev_attr_set_all_refer.attr,
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
#endif
	NULL,
};

static struct attribute_group sec_touch_attr_group = {
	.attrs = sec_touch_attributes,
};

#ifdef ENABLE_NOISE_TEST_MODE
static struct attribute *sec_touch_facotry_attributes[] = {
	&dev_attr_set_all_refer.attr,
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

static int melfas_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct melfas_ts_data *ts;
	struct device *tsp_dev;
#ifdef ENABLE_NOISE_TEST_MODE
	struct device *test_dev;
#endif

	bool empty_chip = false;
	u8 val;
	u8 val_slv;
	u8 fw_ver = 0;
	int ret 	= 0;
	int ret_slv = 0;

	u8 	val_core 	= 0;
	u8 	val_private = 0;
	u8 	val_public 	= 0;
	int ret_core 	= 0;
	int ret_private = 0;
	int ret_public 	= 0;

	int i 		= 0;
	int irq 	= 0;
	int val_firm = -1;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "[TSP] melfas_ts_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kmalloc(sizeof(struct melfas_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		printk(KERN_ERR "[TSP] melfas_ts_probe: failed to create a state of melfas-ts\n");
		ret = -ENOMEM;
		goto err_alloc_failed;
	}
	
	ts->pdata = client->dev.platform_data;
	if (ts->pdata->power_enable)
		ts->power_enable = ts->pdata->power_enable;

	ts->callbacks.inform_charger = inform_charger_connection;
	if (ts->pdata->register_cb)
		ts->pdata->register_cb(&ts->callbacks);
	ts->tsp_status = true;

	ts->client = client;
	i2c_set_clientdata(client, ts);

// F/W Verion Read - Start
	
//	buf = 0x31; // F/W Version RMI Addr.
	gpio_tlmm_config(GPIO_CFG(GPIO_TOUCH_ID, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

	ret = check_firmware_master(ts, &val);
	ret_slv = check_firmware_slave(ts, &val_slv);

	tsp_id = gpio_get_value(GPIO_TOUCH_ID);
	if(system_rev==0x04)
		tsp_id = 1;

#if MELFAS_ISP_DOWNLOAD
// ISP mode start
	if (ret || ret_slv) {
		empty_chip = true;
		pr_err("[TSP] Failed to check firmware version : %d , %d", ret, ret_slv);
	}

	fw_ver = tsp_id ? ILJIN_FW_VER : MELFAS_FW_VER;

	if(val > 0x0 && val < 0x50 )
		val_firm = 0;
	else if (val > 0x50 )
		val_firm = 1;
	else
		val_firm = -1;

	if (val != val_slv ||  val_firm != tsp_id || val < fw_ver || empty_chip){
		firmware_downloading = 1;
		firmware_update(ts);
	}
	firmware_downloading = 0;

	ret = check_firmware_master(ts, &val);
	ret_slv = check_firmware_slave(ts, &val_slv);

	pr_info("[TSP] %s panel - current firmware version : 0x%x , 0x%x\n",
		tsp_id ? "Iljin" : "Melfas", val, val_slv);

	ret = check_slave_boot(ts, &val);
	if (ret)
		pr_err("[TSP] Failed to check slave boot : %d", ret);
#else
// ISC mode start
	pr_info("[TSP] ISC mode Before update %s panel - current firmware version : 0x%x , 0x%x\n",
		tsp_id ? "Iljin" : "Melfas", val, val_slv);

	firmware_update(ts);

	ret_core 	= check_firmware_core(ts, &val_core);
	ret_private = check_firmware_private(ts, &val_private);
	ret_public 	= check_firmware_public(ts, &val_public);

	if (ret_core || ret_private || ret_public) {
		pr_err("[TSP] Failed to check firmware version : %d , %d, %d\n",
			ret_core, ret_private , ret_public);
	}
	pr_info("[TSP] %s panel - current firmware version(ISC mode) : 0x%x , 0x%x, 0x%x\n",
		tsp_id ? "Iljin" : "Melfas", val_core, val_private , val_public);
#endif

	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		printk(KERN_ERR "[TSP] melfas_ts_probe: Not enough memory\n");
		ret = -ENOMEM;
		goto err_alloc_failed;
	} 

	ts->input_dev->name = "sec_touchscreen" ;
#if 0
//	ts->input_dev->evbit[0] = BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);
	

//	ts->input_dev->keybit[BIT_WORD(KEY_MENU)] |= BIT_MASK(KEY_MENU);
//	ts->input_dev->keybit[BIT_WORD(KEY_HOME)] |= BIT_MASK(KEY_HOME);
//	ts->input_dev->keybit[BIT_WORD(KEY_BACK)] |= BIT_MASK(KEY_BACK);		

	__set_bit(EV_SYN, ts->input_dev->evbit); 
	__set_bit(EV_KEY, ts->input_dev->evbit);	
	__set_bit(BTN_TOUCH, ts->input_dev->keybit);
	__set_bit(EV_ABS,  ts->input_dev->evbit);
#endif
  
 	input_mt_init_slots(ts->input_dev, P5_MAX_TOUCH-1); 
	__set_bit(EV_ABS,  ts->input_dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
#if defined(COOD_ROTATE_90) || defined(COOD_ROTATE_270)
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, TS_MAX_Y_COORD, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, TS_MAX_X_COORD, 0, 0);
#else
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, TS_MAX_X_COORD, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, TS_MAX_Y_COORD, 0, 0);
#endif
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
//	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, P5_MAX_TOUCH-1, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
	
	ret = input_register_device(ts->input_dev);	
	if (ret) {
		printk(KERN_ERR "[TSP] melfas_ts_probe: Failed to register device\n");
		ret = -ENOMEM;
		goto err_input_register_device_failed;
	}

	if (debug_print)
		printk(KERN_ERR "[TSP] melfas_ts_probe: succeed to register input device\n");

	if (ts->client->irq) {
		if (debug_print)
			printk(KERN_ERR "[TSP] melfas_ts_probe: trying to request irq: %s-%d\n", ts->client->name, ts->client->irq);

//HASH0701		ret = request_irq(client->irq, melfas_ts_irq_handler, IRQF_TRIGGER_FALLING, ts->client->name, ts);
		ret = request_threaded_irq(client->irq, NULL, melfas_ts_irq_handler,
					IRQF_ONESHOT | IRQF_TRIGGER_LOW,
					ts->client->name, ts);

		if (ret) {
			printk(KERN_ERR "[TSP] melfas_ts_probe: Can't allocate irq %d, ret %d\n", ts->client->irq, ret);
			ret = -EBUSY;
			goto err_request_irq;			
		}
	}

//	schedule_work(&ts->work);
	for (i = 0; i < P5_MAX_TOUCH ; i++)  /* _SUPPORT_MULTITOUCH_ */
		g_Mtouch_info[i].strength = -1;	

	tsp_dev  = device_create(sec_class, NULL, 0, ts, "sec_touch");
	if (IS_ERR(tsp_dev)) 
		pr_err("[TSP] Failed to create device for the sysfs\n");
	
	ret = sysfs_create_group(&tsp_dev->kobj, &sec_touch_attr_group);
	if (ret) 
		pr_err("[TSP] Failed to create sysfs group\n");


#ifdef ENABLE_NOISE_TEST_MODE
	test_dev = device_create(sec_class, NULL, 0, ts, "qt602240_noise_test");
	if (IS_ERR(test_dev)) {
		pr_err("Failed to create device for the factory test\n");
		ret = -ENODEV;
	}

	ret = sysfs_create_group(&test_dev->kobj, &sec_touch_factory_attr_group);
	if (ret) {
		pr_err("Failed to create sysfs group for the factory test\n");
	}
#endif

#if CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = melfas_ts_early_suspend;
	ts->early_suspend.resume = melfas_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif
	
	return 0;

err_request_irq:
	free_irq(client->irq, ts);
err_input_register_device_failed:
	input_free_device(ts->input_dev);
err_alloc_failed:
	kfree(ts);
err_check_functionality_failed:
	return ret;
}

static int melfas_ts_remove(struct i2c_client *client)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	pr_warning("[TSP] %s\n", __func__);

	unregister_early_suspend(&ts->early_suspend);
	free_irq(client->irq, ts);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	return 0;
}

static int melfas_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	disable_irq(client->irq);

	ret = cancel_work_sync(&ts->work);
	if (ret) /* if work was pending disable-count is now 2 */
		enable_irq(client->irq);

	ret = i2c_smbus_write_byte_data(client, 0x01, 0x00); /* deep sleep */
	if (ret < 0)
		printk(KERN_ERR "[TSP] melfas_ts_suspend: i2c_smbus_write_byte_data failed\n");

	return 0;
}

static int melfas_ts_resume(struct i2c_client *client)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	enable_irq(client->irq);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h)
{
//	int ret;
	struct melfas_ts_data *ts = container_of(h, struct melfas_ts_data, early_suspend);
//	melfas_ts_suspend(ts->client, PMSG_SUSPEND);//HASH TEST

	pr_info("[TSP] %s\n", __func__);

	release_all_fingers(ts);

	disable_irq(ts->client->irq);
	
	if (ts->power_enable)
		ts->power_enable(0);
	ts->tsp_status = false;
}

static void melfas_ts_late_resume(struct early_suspend *h)
{
	struct melfas_ts_data *ts = container_of(h, struct melfas_ts_data, early_suspend);
	//melfas_ts_resume(ts->client);

	char buf[2];

	pr_info("[TSP] %s : TA/USB %sconnect\n", __func__,ts->charging_status?"":"dis");

	if (ts->power_enable)
		ts->power_enable(1);
	ts->tsp_status = true;

	buf[0] = 0x60;
	buf[1] = ts->charging_status;
	msleep(100);
	melfas_i2c_write(ts->client, (char *)buf, 2);

	enable_irq(ts->client->irq);
}
#endif

static void melfas_ts_shutdown(struct i2c_client *client)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	free_irq(client->irq, ts);
	ts->power_enable(0);

	gpio_direction_output(GPIO_TOUCH_INT, 0);
}

static const struct i2c_device_id melfas_ts_id[] = {
	{ "sec_touch" , 0 },
	{ }
};


static struct i2c_driver melfas_ts_driver = {
	.driver		= {
		.name	= "sec_touch",
	},
	.id_table		= melfas_ts_id,
	.probe		= melfas_ts_probe,
	.remove		= __devexit_p (melfas_ts_remove),
	.shutdown	= melfas_ts_shutdown,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend		= melfas_ts_suspend,
	.resume		= melfas_ts_resume,
#endif
};

static int __devinit melfas_ts_init(void)
{
#ifdef CONFIG_SAMSUNG_LPM_MODE
#if defined(CONFIG_TARGET_LOCALE_KOR_SKT) || defined(CONFIG_TARGET_LOCALE_KOR_KT) || defined(CONFIG_TARGET_LOCALE_KOR_LGU)
	extern int charging_mode_from_boot;
	if (charging_mode_from_boot == 1) {
		pr_info("%s : LPM Charging Mode! returen ENODEV!\n", __func__);
		return 0;
	}
#endif
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
