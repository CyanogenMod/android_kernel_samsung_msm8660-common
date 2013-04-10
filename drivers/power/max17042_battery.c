/*
 * Fuel gauge driver for Maxim 17042 / 8966 / 8997
 *  Note that Maxim 8966 and 8997 are mfd and this is its subdevice.
 *
 * Copyright (C) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * This driver is based on max17040_battery.c
 */

#include <linux/types.h>
#include <linux/irq.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
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
#include <linux/rtc.h>
#include <mach/gpio.h>
#include <linux/power_supply.h>
#include <linux/power/max17042_battery.h>

#define PRINT_COUNT	10
//#define USE_TEMP_TABLE

static struct i2c_client *fg_i2c_client;
struct max17042_chip *max17042_chip_data;
extern int check_jig_on(void);

static int fg_i2c_read(struct i2c_client *client, u8 reg, u8 *data, u8 length)
{
	s32 value;

	value = i2c_smbus_read_i2c_block_data(client, reg, length, data);
	if (value < 0 || value != length) {
		pr_err("%s: Failed to fg_i2c_read. status = %d\n", __func__, value);
		return -1;
	}

	return 0;
}

static int fg_i2c_write(struct i2c_client *client, u8 reg, u8 *data, u8 length)
{
	s32 value;

	value = i2c_smbus_write_i2c_block_data(client, reg, length, data);
	if (value < 0) {
		pr_err("%s: Failed to fg_i2c_write, error code=%d\n", __func__, value);
		return -1;
	}

	return 0;
}

static int fg_read_register(u8 addr)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];

	if (fg_i2c_read(client, addr, data, 2) < 0) {
		pr_err("%s: Failed to read addr(0x%x)\n", __func__, addr);
		return -1;
	}

	return (data[1] << 8) | data[0];
}

static int fg_write_register(u8 addr, u16 w_data)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];

	data[0] = w_data & 0xFF;
	data[1] = w_data >> 8;

	if (fg_i2c_write(client, addr, data, 2) < 0) {
		pr_err("%s: Failed to write addr(0x%x)\n", __func__, addr);
		return -1;
	}

	return 0;
}

static int fg_read_16register(u8 addr, u16 *r_data)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[32];
	int i = 0;

	if (fg_i2c_read(client, addr, data, 32) < 0) {
		pr_err("%s: Failed to read addr(0x%x)\n", __func__, addr);
		return -1;
	}

	for (i = 0; i < 16; i++)
		r_data[i] = (data[2 * i + 1] << 8) | data[2 * i];

	return 0;
}

void fg_write_and_verify_register(u8 addr, u16 w_data)
{
	u16 r_data;
	u8 retry_cnt = 2;

	while (retry_cnt) {
		fg_write_register(addr, w_data);
		r_data = fg_read_register(addr);

		if (r_data != w_data) {
			pr_err("%s: verification failed (addr : 0x%x, w_data : 0x%x, r_data : 0x%x)\n",
				__func__, addr, w_data, r_data);
			retry_cnt--;
		}
		else
			break;
	}
}

static void fg_test_print(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];
	u32 average_vcell;
	u16 w_data;
	u32 temp;
	u32 temp2;
	u16 reg_data;

	if (fg_i2c_read(client, AVR_VCELL_REG, data, 2) < 0) {
		pr_err("%s: Failed to read VCELL\n", __func__);
		return;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	average_vcell = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	average_vcell += (temp2 << 4);

	pr_info("%s : AVG_VCELL(%d), data(0x%04x)\n", __func__, average_vcell, (data[1]<<8) | data[0]);

	reg_data = fg_read_register(FULLCAP_REG);
	pr_info("%s : FULLCAP(%d), data(0x%04x)\n", __func__, reg_data/2, reg_data);

	reg_data = fg_read_register(REMCAP_REP_REG);
	pr_info("%s : REMCAP_REP(%d), data(0x%04x)\n", __func__, reg_data/2, reg_data);

	reg_data = fg_read_register(REMCAP_MIX_REG);
	pr_info("%s : REMCAP_MIX(%d), data(0x%04x)\n", __func__, reg_data/2, reg_data);

	reg_data = fg_read_register(REMCAP_AV_REG);
	pr_info("%s : REMCAP_AV(%d), data(0x%04x)\n", __func__, reg_data/2, reg_data);

	reg_data = fg_read_register(VFSOC_REG);
	pr_info("%s : VFSOC_REG(%d), data(0x%04x)\n", __func__, reg_data/2, reg_data);

#if defined(CONFIG_TARGET_LOCALE_KOR_SKT) || defined(CONFIG_TARGET_LOCALE_KOR_KT) || defined(CONFIG_TARGET_LOCALE_KOR_LGU)
	reg_data = fg_read_register(CGAIN_REG);
	pr_info("%s : CGAIN_REG(%d), data(0x%04x)\n", __func__, reg_data/2, reg_data);
#endif
}

static int fg_read_vcell(void)
{
	struct i2c_client *client = fg_i2c_client;
//	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data[2];
	u32 vcell;
	u16 w_data;
	u32 temp;
	u32 temp2;

	if (fg_i2c_read(client, VCELL_REG, data, 2) < 0) {
		pr_err("%s: Failed to read VCELL\n", __func__);
		return -1;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	vcell = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	vcell += (temp2 << 4);

//	if (!(chip->info.pr_cnt % PRINT_COUNT))
		pr_info("%s : VCELL(%d), data(0x%04x)\n",
			__func__, vcell, (data[1]<<8) | data[0]);

	return vcell;
}

static int fg_read_vfocv(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];
	u32 vfocv = 0;
	u16 w_data;
	u32 temp;
	u32 temp2;

	if (fg_i2c_read(client, VFOCV_REG, data, 2) < 0) {
		pr_err("%s: Failed to read VFOCV\n", __func__);
		return -1;
	}

	w_data = (data[1]<<8) | data[0];

	temp = (w_data & 0xFFF) * 78125;
	vfocv = temp / 1000000;

	temp = ((w_data & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	vfocv += (temp2 << 4);

	return vfocv;
}

static int fg_check_battery_present(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 status_data[2];
	int ret = 1;

	/* 1. Check Bst bit */
	if (fg_i2c_read(client, STATUS_REG, status_data, 2) < 0) {
		pr_err("%s: Failed to read STATUS_REG\n", __func__);
		return 0;
	}

	if (status_data[0] & (0x1 << 3)) {
		pr_info("%s - addr(0x01), data(0x%04x)\n", __func__, (status_data[1]<<8) | status_data[0]);
		pr_info("%s : battery is absent!!\n", __func__);
		ret = 0;
	}

	return ret;
}

#ifdef USE_TEMP_TABLE
static int temp_table[][2] = {
	/* Temp('C), 	  ADC	*/
	{ -5000,	-6000 },
	{ -4000,	-5000 },
	{ -3000,	-4000 },
	{ -2000,	-3000 },
	{ -1000,	-2000 },
	{ 	  0,	-1000 },
	{  5000,	5000 },
	{ 10000,	10000 },
	{ 15000,	15000 },
	{ 20000,	20000 },
	{ 25000,	25000 },
	{ 40000,	40000 },
	{ 42000,	42000 },
	{ 45000,	44500 },
	{ 50000,	48000 },
	{ 55000,	52000 },
	{ 60000,	55500 },
	{ 65000,	58300 },
	{ 70000,	61000 }
};

static int fg_compensation_temp(int temp)
{
	int retval = 0;
	int offset = 0;
	int i = 0, array_size = 0;
	array_size = ARRAY_SIZE(temp_table);
	for (i=0; i<array_size-1; i++) {
		// overflow
		if (i == 0) {
			if (temp < temp_table[0][1]) {
				offset = temp_table[0][0] - temp_table[0][1];
				break;
			} else if (temp > temp_table[array_size-1][1]) {
				offset = temp + temp_table[array_size-1][0] - 2 * temp_table[array_size-1][1];
				break;
			}
		}

		// normal 
		if (temp_table[i][1] <= temp && temp_table[i+1][1] > temp) {
			offset = temp_table[i][0] - temp_table[i][1] + ((temp - temp_table[i][1]) * (temp_table[i+1][0] - temp_table[i][0])) / (temp_table[i+1][1] - temp_table[i][1]);
			break;
		}
	}

	retval = temp + offset;
	pr_info("%s: temp(%d), offset(%d), retval(%d)\n", __func__, temp, offset, retval);
	return retval;
}
#endif
static int fg_read_temp(void)
{
	struct i2c_client *client = fg_i2c_client;
//	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data[2];
	int temper = 0;

	if (fg_check_battery_present()) {
		if (fg_i2c_read(client, TEMPERATURE_REG, data, 2) < 0) {
			pr_err("%s: Failed to read TEMPERATURE_REG\n", __func__);
			return -1;
		}

		if (data[1]&(0x1 << 7)) {
			temper = ((~(data[1]))&0xFF)+1;
			temper *= (-1000);
		} else {
			temper = data[1] & 0x7f;
			temper *= 1000;
			temper += data[0] * 39 / 10;
		}
	} else
		temper = 20000;

//	if (!(chip->info.pr_cnt % PRINT_COUNT))
		pr_info("%s : TEMPERATURE(%d), data(0x%04x)\n", __func__, temper, (data[1]<<8) | data[0]);

#ifdef USE_TEMP_TABLE
	temper = fg_compensation_temp(temper);
#endif

	return temper;
}

static int fg_read_soc(void)
{
	struct i2c_client *client = fg_i2c_client;
//	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data[2];
	u32 soc = 0;

	if (fg_i2c_read(client, SOCREP_REG, data, 2) < 0) {
		pr_err("%s: Failed to read SOCREP\n", __func__);
		return -1;
	}

	soc = data[1];

//	if (!(chip->info.pr_cnt % PRINT_COUNT))
		pr_info("%s : SOC(%d), data(0x%04x)\n", __func__, soc, (data[1]<<8) | data[0]);

	return soc;
}

static int fg_read_vfsoc(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];

	if (fg_i2c_read(client, VFSOC_REG, data, 2) < 0) {
		pr_err("%s: Failed to read VFSOC\n", __func__);
		return -1;
	}

	return (int)data[1];
}

static int fg_read_current(void)
{
#if defined(CONFIG_TARGET_LOCALE_KOR_SKT) || defined(CONFIG_TARGET_LOCALE_KOR_KT) || defined(CONFIG_TARGET_LOCALE_KOR_LGU)
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data[2];
	u32 temp, sign;
	s32 i_current;

	if (fg_i2c_read(client, CURRENT_REG, data, 2) < 0) {
		pr_err("%s: Failed to read CURRENT\n", __func__);
		return -1;
	}

	temp = ((data[1]<<8) | data[0]) & 0xFFFF;
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else
		sign = POSITIVE;

	i_current = temp * MAX17042_CURRENT_UNIT;
	if (sign)
		i_current *= -1;

//	if (!(chip->info.pr_cnt++ % PRINT_COUNT)) {
		fg_test_print();
		pr_info("%s : CURRENT(%dmA)\n", __func__, i_current);
		chip->info.pr_cnt = 1;
		/* Read max17042's all registers every 5 minute. */
		fg_periodic_read();
//	}
#else
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data1[2], data2[2];
	u32 temp, sign;
	s32 i_current;
	s32 avg_current;

	if (fg_i2c_read(client, CURRENT_REG, data1, 2) < 0) {
		pr_err("%s: Failed to read CURRENT\n", __func__);
		return -1;
	}

	if (fg_i2c_read(client, AVG_CURRENT_REG, data2, 2) < 0) {
		pr_err("%s: Failed to read AVERAGE CURRENT\n", __func__);
		return -1;
	}

	temp = ((data1[1]<<8) | data1[0]) & 0xFFFF;
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else
		sign = POSITIVE;

	i_current = temp * MAX17042_CURRENT_UNIT;
	if (sign)
		i_current *= -1;

	temp = ((data2[1]<<8) | data2[0]) & 0xFFFF;
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else
		sign = POSITIVE;

	avg_current = temp * MAX17042_CURRENT_UNIT;
	if (sign)
		avg_current *= -1;

//	if (!(chip->info.pr_cnt++ % PRINT_COUNT)) {
		fg_test_print();
		pr_info("%s : CURRENT(%dmA), AVG_CURRENT(%dmA)\n", __func__, i_current, avg_current);
		chip->info.pr_cnt = 1;
		/* Read max17042's all registers every 5 minute. */
		fg_periodic_read();
//	}
#endif
	return i_current;
}


static int fg_read_avg_current(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8  data2[2];
	u32 temp, sign;
	s32 avg_current;

	if (fg_i2c_read(client, AVG_CURRENT_REG, data2, 2) < 0) {
		pr_err("%s: Failed to read AVERAGE CURRENT\n", __func__);
		return -1;
	}

	temp = ((data2[1]<<8) | data2[0]) & 0xFFFF;
	if (temp & (0x1 << 15)) {
		sign = NEGATIVE;
		temp = (~temp & 0xFFFF) + 1;
	} else
		sign = POSITIVE;

	avg_current = temp * MAX17042_CURRENT_UNIT;

	if (sign)
		avg_current *= -1;

	pr_info("%s : AVG_CURRENT(%dmA)\n", __func__, avg_current);
	return avg_current;
}

int fg_reset_soc(void)
{
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data[2];
	u32 fg_vfsoc, new_soc, new_remcap, fullcap;
	u16 temp = 0;

	pr_info("%s : Before quick-start - VfOCV(%d), VfSOC(%d), RepSOC(%d)\n",
					__func__, fg_read_vfocv(), fg_read_vfsoc(), fg_read_soc());

	if (!check_jig_on()) {
		pr_info("%s : Return by No JIG_ON signal\n", __func__);
		return 0;
	}

	fg_write_register(CYCLES_REG, 0);

	if (fg_i2c_read(client, MISCCFG_REG, data, 2) < 0) {
		pr_err("%s: Failed to read MiscCFG\n", __func__);
		return -1;
	}

	data[1] |= (0x1 << 2);
	if (fg_i2c_write(client, MISCCFG_REG, data, 2) < 0) {
		pr_err("%s: Failed to write MiscCFG\n", __func__);
		return -1;
	}

	msleep(250);
	fg_write_register(FULLCAP_REG, chip->info.capacity);
	msleep(500);

	pr_info("%s : After quick-start - VfOCV(%d), VfSOC(%d), RepSOC(%d)\n",
					__func__, fg_read_vfocv(), fg_read_vfsoc(), fg_read_soc());
	fg_write_register(CYCLES_REG, 0x00a0);

	// Additional Step : Adjust SOC
	fullcap = fg_read_register(FULLCAP_REG);
	temp = fg_read_register(VFSOC_REG);
	fg_vfsoc = (temp>>8) * 100 + (temp&0xff) * 39 / 100;
	new_soc = fg_vfsoc * 1333 / 1000 - 80;
	new_remcap = new_soc * fullcap / 10000;
	if(new_remcap >= fullcap)
		new_remcap = fullcap;
	fg_write_register(REMCAP_REP_REG, (u16)(new_remcap));

	pr_info("%s : Additional Step - VfSOC(%d.%d), New SOC(%d.%d), New RemCAP(%d)\n",
					__func__, (fg_vfsoc/100), (fg_vfsoc%100), (new_soc/100),
					(new_soc%100), fg_read_register(REMCAP_REP_REG)/2);

	return 0;
}


int fg_reset_capacity(void)
{
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);

	return fg_write_register(DESIGNCAP_REG, chip->info.vfcapacity-1);
}

int fg_adjust_capacity(void)
{
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 data[2];

	data[0] = 0;
	data[1] = 0;

	/* 1. Write RemCapREP(05h)=0; */
	if (fg_i2c_write(client, REMCAP_REP_REG, data, 2) < 0) {
		pr_err("%s: Failed to write RemCap_REP\n", __func__);
		return -1;
	}
	msleep(200);

	pr_info("%s : After adjust - RepSOC(%d)\n", __func__, fg_read_soc());

	chip->info.soc_restart_flag = 1;

	return 0;
}

#ifdef KEEP_SOC_LEVEL1
void fg_prevent_early_poweroff(int vcell, int fg_soc)
{
	struct i2c_client *client = fg_i2c_client;
	int repsoc, repsoc_data = 0;
	u8 data[2];
	static u16 RemCapMix =0;
	static u16 RemCapRep =0;
	static bool is_first_time = 1;
		
	pr_debug("%s : vcell=%d, fg_soc=%d\r\n", __func__, vcell, fg_soc);

	if (fg_i2c_read(client, SOCREP_REG, data, 2) < 0) {
		pr_err("%s: Failed to read SOCREP\n", __func__);
		return;
	}
	repsoc = data[1];
	repsoc_data = ((data[1] << 8) | data[0]);

	/* 1% keep condition : (RepSoc<1.5%) && (3.5v<vcell) */
	if (repsoc_data > POWER_OFF_SOC_HIGH_MARGIN) {
		/* clear flag, RemCap and RemCapRep */
		is_first_time = 1;
		RemCapMix = 0;
		RemCapRep = 0;
		return;
	}
	
	pr_info("%s : fg_soc=%d, soc=%d (0x%04x), vcell=%d\n", __func__, fg_soc, repsoc, repsoc_data, vcell);

	if(vcell > POWER_OFF_VOLTAGE_HIGH_MARGIN) {
		pr_debug("%s : Keep RemCapMix & RemCapRep to 1.5%!!\n", __func__);
		if(is_first_time) {
			/* save RemCapMix and RemCapRep it as RemCapMix_1%, RemCapRep_1% */
			is_first_time = 0;
			RemCapMix = fg_read_register(REMCAP_MIX_REG);
			RemCapRep = fg_read_register(REMCAP_REP_REG);
			pr_info("%s : 1. save RemCapMix=0x%x, RemCapRep=0x%x\n", __func__, RemCapMix, RemCapRep);			
		} else {
			/* keep 1.5% by writing RemCapMix_1, RemCapRep_1 */
			fg_write_register(REMCAP_MIX_REG, (u16)RemCapMix);
			fg_write_register(REMCAP_REP_REG, (u16)RemCapRep);
			pr_info("%s : 2. writing RemCapMix =0x%x, RemCapRep=0x%x\n", __func__, RemCapMix, RemCapRep);			
			msleep(200);	
		}
	}
}
#endif

void fg_low_batt_compensation(u32 level)
{
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);
	int read_val;
	u32 temp;
	u16 tempVal;

	pr_info("%s : Adjust SOCrep to %d!!\n", __func__, level);

	/* 1) RemCapREP (05h) = FullCap(10h) x 0.034 (or 0.014) */
	read_val = fg_read_register(FULLCAP_REG);
	if (read_val < 0)
		return;

	temp = read_val * (level*100 + 1) / 10000;
	fg_write_register(REMCAP_REP_REG, (u16)temp);

	/* 2) RepSOC (06h) = 3.01% or 1.01% */
	tempVal = (u16)((level << 8) | 0x3);
	fg_write_register(SOCREP_REG, tempVal);

	/* 3) MixSOC (0Dh) = RepSOC */
	fg_write_register(SOCMIX_REG, tempVal);

	/* 4) AVSOC (0Eh) = RepSOC; */
	fg_write_register(SOCAV_REG, tempVal);

	chip->info.low_batt_comp_flag = 1;
}

void fg_periodic_read(void)
{
	u8 reg;
	int i;
	int data[0x10];
	struct timespec ts;
	struct rtc_time tm;

	getnstimeofday(&ts);
	rtc_time_to_tm(ts.tv_sec, &tm);

	pr_info("[MAX17042] %d/%d/%d %02d:%02d,",
		tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900, tm.tm_hour, tm.tm_min);

#if 0
	for (i = 0; i < 16; i++) {
		for (reg = 0; reg < 0x10; reg++) {
			data[reg] = fg_read_register(reg + i * 0x10);
		}
		printk("%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,%04xh,",
			data[0x00], data[0x01], data[0x02], data[0x03], data[0x04], data[0x05], data[0x06], data[0x07],
			data[0x08], data[0x09], data[0x0a], data[0x0b], data[0x0c], data[0x0d], data[0x0e], data[0x0f]);
		if (i == 4)
			i = 13;
	}
#endif
}

static void fg_read_model_data(void)
{
	u16 data0[16], data1[16], data2[16];
	int i;
	int relock_check;

	pr_info("[FG_Model] ");

	/* Unlock model access */
	fg_write_register(0x62, 0x0059);
	fg_write_register(0x63, 0x00C4);

	/* Read model data */
	fg_read_16register(0x80, data0);
	fg_read_16register(0x90, data1);
	fg_read_16register(0xa0, data2);

	/* Print model data */
	for (i = 0; i < 16; i++)
		pr_info("0x%04x, ", data0[i]);

	for (i = 0; i < 16; i++)
		pr_info("0x%04x, ", data1[i]);

	for (i = 0; i < 16; i++) {
		if (i == 15)
			pr_info("0x%04x", data2[i]);
		else
			pr_info("0x%04x, ", data2[i]);
	}

	do {
		relock_check = 0;
		/* Lock model access */
		fg_write_register(0x62, 0x0000);
		fg_write_register(0x63, 0x0000);

		/* Read model data again */
		fg_read_16register(0x80, data0);
		fg_read_16register(0x90, data1);
		fg_read_16register(0xa0, data2);

		for (i = 0; i < 16; i++) {
			if (data0[i] || data1[i] || data2[i]) {
				pr_info("%s : data is non-zero, lock again!!\n", __func__);
				relock_check = 1;
			}
		}
	} while (relock_check);

}

int fg_alert_init(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 misccgf_data[2];
	u8 salrt_data[2];
	u8 config_data[2];
	u8 valrt_data[2];
	u8 talrt_data[2];
	u16 read_data = 0;

	/* Using RepSOC */
	if (fg_i2c_read(client, MISCCFG_REG, misccgf_data, 2) < 0) {
		pr_err("%s: Failed to read MISCCFG_REG\n", __func__);
		return -1;
	}
	misccgf_data[0] = misccgf_data[0] & ~(0x03);

	if (fg_i2c_write(client, MISCCFG_REG, misccgf_data, 2) < 0) {
		pr_info("%s: Failed to write MISCCFG_REG\n", __func__);
		return -1;
	}

	/* SALRT Threshold setting */
	salrt_data[1] = 0xff;
	salrt_data[0] = 0x01;
	if (fg_i2c_write(client, SALRT_THRESHOLD_REG, salrt_data, 2) < 0) {
		pr_info("%s: Failed to write SALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	/* Reset VALRT Threshold setting (disable) */
	valrt_data[1] = 0xFF;
	valrt_data[0] = 0x00;
	if (fg_i2c_write(client, VALRT_THRESHOLD_REG, valrt_data, 2) < 0) {
		pr_info("%s: Failed to write VALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	read_data = fg_read_register((u8)VALRT_THRESHOLD_REG);
	if (read_data != 0xff00)
		pr_err("%s : VALRT_THRESHOLD_REG is not valid (0x%x)\n", __func__, read_data);

	/* Reset TALRT Threshold setting (disable) */
	talrt_data[1] = 0x7F;
	talrt_data[0] = 0x80;
	if (fg_i2c_write(client, TALRT_THRESHOLD_REG, talrt_data, 2) < 0) {
		pr_info("%s: Failed to write TALRT_THRESHOLD_REG\n", __func__);
		return -1;
	}

	read_data = fg_read_register((u8)TALRT_THRESHOLD_REG);
	if (read_data != 0x7f80)
		pr_err("%s : TALRT_THRESHOLD_REG is not valid (0x%x)\n", __func__, read_data);

	mdelay(100);

	/* Enable SOC alerts */
	if (fg_i2c_read(client, CONFIG_REG, config_data, 2) < 0) {
		pr_err("%s: Failed to read CONFIG_REG\n", __func__);
		return -1;
	}
	config_data[0] = config_data[0] | (0x1 << 2);

	if (fg_i2c_write(client, CONFIG_REG, config_data, 2) < 0) {
		pr_info("%s: Failed to write CONFIG_REG\n", __func__);
		return -1;
	}

	return 1;
}

static int fg_check_status_reg(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 status_data[2];
	int ret = 0;

	/* 1. Check Smn was generatedread */
	if (fg_i2c_read(client, STATUS_REG, status_data, 2) < 0) {
		pr_err("%s: Failed to read STATUS_REG\n", __func__);
		return -1;
	}
	pr_info("%s - addr(0x00), data(0x%04x)\n", __func__, (status_data[1]<<8) | status_data[0]);

	if (status_data[1] & (0x1 << 2))
		ret = 1;

	/* 2. clear Status reg */
	status_data[1] = 0;
	if (fg_i2c_write(client, STATUS_REG, status_data, 2) < 0) {
		pr_info("%s: Failed to write STATUS_REG\n", __func__);
		return -1;
	}

	return ret;
}

static int fg_get_battery_type(void)
{
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);

	return chip->info.battery_type;
}

void fg_fullcharged_compensation(u32 is_recharging, u32 pre_update)
{
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);
	static int new_fullcap_data;

	pr_info("%s : is_recharging(%d), pre_update(%d)\n", __func__, is_recharging, pre_update);

	new_fullcap_data = fg_read_register(FULLCAP_REG);
	if (new_fullcap_data < 0)
		new_fullcap_data = chip->info.capacity;

	if (new_fullcap_data > (chip->info.capacity * 110 / 100)) {
		pr_info("%s : [Case 1] previous_fullcap = 0x%04x, NewFullCap = 0x%04x\n",
			__func__, chip->info.previous_fullcap, new_fullcap_data);

		new_fullcap_data = (chip->info.capacity * 110) / 100;
		fg_write_register(REMCAP_REP_REG, (u16)(new_fullcap_data));
		fg_write_register(FULLCAP_REG, (u16)(new_fullcap_data));
	} else if (new_fullcap_data < (chip->info.capacity * 70 / 100)) {
		pr_info("%s : [Case 5] previous_fullcap = 0x%04x, NewFullCap = 0x%04x\n",
			__func__, chip->info.previous_fullcap, new_fullcap_data);

		new_fullcap_data = (chip->info.capacity * 70) / 100;
		fg_write_register(REMCAP_REP_REG, (u16)(new_fullcap_data));
		fg_write_register(FULLCAP_REG, (u16)(new_fullcap_data));
	} else {
		if (new_fullcap_data > (chip->info.previous_fullcap * 105 / 100)) {
			pr_info("%s : [Case 2] previous_fullcap = 0x%04x, NewFullCap = 0x%04x\n",
				__func__, chip->info.previous_fullcap, new_fullcap_data);

			new_fullcap_data = (chip->info.previous_fullcap * 105) / 100;
			fg_write_register(REMCAP_REP_REG, (u16)(new_fullcap_data));
			fg_write_register(FULLCAP_REG, (u16)(new_fullcap_data));
		} else if (new_fullcap_data < (chip->info.previous_fullcap * 90 / 100)) {
			pr_info("%s : [Case 3] previous_fullcap = 0x%04x, NewFullCap = 0x%04x\n",
				__func__, chip->info.previous_fullcap, new_fullcap_data);

			new_fullcap_data = (chip->info.previous_fullcap * 90) / 100;
			fg_write_register(REMCAP_REP_REG, (u16)(new_fullcap_data));
			fg_write_register(FULLCAP_REG, (u16)(new_fullcap_data));
		} else {
			pr_info("%s : [Case 4] previous_fullcap = 0x%04x, NewFullCap = 0x%04x\n",
				__func__, chip->info.previous_fullcap, new_fullcap_data);
		}
	}

	/* In case of recharging, re-write full_charged_cap to FullCAP, RemCAP_REP. */
	if (!is_recharging)
		chip->info.full_charged_cap = new_fullcap_data;
	else {
		pr_info("%s : [Case 6] FirstFullCap = 0x%04x, NewFullCap = 0x%04x\n",
			__func__, chip->info.full_charged_cap, new_fullcap_data);

		//fg_write_register(REMCAP_REP_REG, (u16)(chip->info.full_charged_cap));
		//fg_write_register(FULLCAP_REG, (u16)(chip->info.full_charged_cap));
	}

	/* 4. Write RepSOC(06h)=100%; */
	fg_write_register(SOCREP_REG, (u16)(0x64 << 8));

	/* 5. Write MixSOC(0Dh)=100%; */
	fg_write_register(SOCMIX_REG, (u16)(0x64 << 8));

	/* 6. Write AVSOC(0Eh)=100%; */
	fg_write_register(SOCAV_REG, (u16)(0x64 << 8));

	/* if pre_update case, skip updating PrevFullCAP value. */
	if (!pre_update)
		chip->info.previous_fullcap = fg_read_register(FULLCAP_REG);

	pr_info("%s : (A) FullCap = 0x%04x, RemCap = 0x%04x\n", __func__,
		fg_read_register(FULLCAP_REG), fg_read_register(REMCAP_REP_REG));

	fg_periodic_read();

}

void fg_check_vf_fullcap_range(void)
{
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);
	static int new_vffullcap;
	u16 print_flag = 1;

	new_vffullcap = fg_read_register(FULLCAP_NOM_REG);
	if (new_vffullcap < 0)
		new_vffullcap = chip->info.vfcapacity;

	if (new_vffullcap > (chip->info.vfcapacity * 110 / 100)) {
		pr_info("%s : [Case 1] previous_vffullcap = 0x%04x, NewVfFullCap = 0x%04x\n",
			__func__, chip->info.previous_vffullcap, new_vffullcap);

		new_vffullcap = (chip->info.vfcapacity * 110) / 100;

		fg_write_register(DQACC_REG, (u16)(new_vffullcap / 4));
		fg_write_register(DPACC_REG, (u16)0x3200);
	} else if (new_vffullcap < (chip->info.vfcapacity * 70 / 100)) {
		pr_info("%s : [Case 5] previous_vffullcap = 0x%04x, NewVfFullCap = 0x%04x\n",
			__func__, chip->info.previous_vffullcap, new_vffullcap);

		new_vffullcap = (chip->info.vfcapacity * 70) / 100;

		fg_write_register(DQACC_REG, (u16)(new_vffullcap / 4));
		fg_write_register(DPACC_REG, (u16)0x3200);
	} else {
		if (new_vffullcap > (chip->info.previous_vffullcap * 105 / 100)) {
			pr_info("%s : [Case 2] previous_vffullcap = 0x%04x, NewVfFullCap = 0x%04x\n",
				__func__, chip->info.previous_vffullcap, new_vffullcap);

			new_vffullcap = (chip->info.previous_vffullcap * 105) / 100;

			fg_write_register(DQACC_REG, (u16)(new_vffullcap / 4));
			fg_write_register(DPACC_REG, (u16)0x3200);
		} else if (new_vffullcap < (chip->info.previous_vffullcap * 90 / 100)) {
			pr_info("%s : [Case 3] previous_vffullcap = 0x%04x, NewVfFullCap = 0x%04x\n",
				__func__, chip->info.previous_vffullcap, new_vffullcap);

			new_vffullcap = (chip->info.previous_vffullcap * 90) / 100;

			fg_write_register(DQACC_REG, (u16)(new_vffullcap / 4));
			fg_write_register(DPACC_REG, (u16)0x3200);
		} else {
			pr_info("%s : [Case 4] previous_vffullcap = 0x%04x, NewVfFullCap = 0x%04x\n",
				__func__, chip->info.previous_vffullcap, new_vffullcap);
			print_flag = 0;
		}
	}

	chip->info.previous_vffullcap = fg_read_register(FULLCAP_NOM_REG);

	if (print_flag)
		pr_info("%s : VfFullCap(0x%04x), dQacc(0x%04x), dPacc(0x%04x)\n", __func__,
			fg_read_register(FULLCAP_NOM_REG), fg_read_register(DQACC_REG), fg_read_register(DPACC_REG));

}

int fg_check_cap_corruption(void)
{
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);
	int vfsoc = fg_read_vfsoc();
	int repsoc = fg_read_soc();
	int mixcap = fg_read_register(REMCAP_MIX_REG);
	int vfocv = fg_read_register(VFOCV_REG);
	int remcap = fg_read_register(REMCAP_REP_REG);
	int fullcapacity = fg_read_register(FULLCAP_REG);
	int vffullcapacity = fg_read_register(FULLCAP_NOM_REG);
	u32 temp, temp2, new_vfocv, pr_vfocv;
	int ret = 0;

	/* If usgin Jig or low batt compensation flag is set, then skip checking. */
	if (check_jig_on()) {
		pr_info("%s : Return by Using Jig(%d)\n", __func__, check_jig_on());
		return 0;
	}

	if (vfsoc < 0 || repsoc < 0 || mixcap < 0 || vfocv < 0 ||
		remcap < 0 || fullcapacity < 0 || vffullcapacity < 0)
		return 0;

	/* Check full charge learning case. */
	if (((vfsoc >= 70) && ((remcap >= (fullcapacity * 995 / 1000)) && (remcap <= (fullcapacity * 1005 / 1000))))
		|| chip->info.low_batt_comp_flag || chip->info.soc_restart_flag) {
		/* pr_info("%s : RemCap(%d), FullCap(%d), SOC(%d), low_batt_comp_flag(%d), soc_restart_flag(%d)\n",
					__func__, (RemCap/2), (FullCapacity/2), RepSOC, low_batt_comp_flag, soc_restart_flag); */
		chip->info.previous_repsoc = repsoc;
		chip->info.previous_remcap = remcap;
		chip->info.previous_fullcapacity = fullcapacity;
		if (chip->info.soc_restart_flag)
			chip->info.soc_restart_flag = 0;

		ret = 1;
	}

	/* ocv calculation for print */
	temp = (vfocv & 0xFFF) * 78125;
	pr_vfocv = temp / 1000000;

	temp = ((vfocv & 0xF000) >> 4) * 78125;
	temp2 = temp / 1000000;
	pr_vfocv += (temp2 << 4);

	/* MixCap differ is greater than 265mAh */
	if ((((vfsoc+5) < chip->info.previous_vfsoc) || (vfsoc > (chip->info.previous_vfsoc+5)))
		|| (((repsoc+5) < chip->info.previous_repsoc) || (repsoc > (chip->info.previous_repsoc+5)))
		|| (((mixcap+530) < chip->info.previous_mixcap) || (mixcap > (chip->info.previous_mixcap+530)))) {
		fg_periodic_read();

		pr_info("[FG_Recovery] (B) VfSOC(%d), prevVfSOC(%d), RepSOC(%d), prevRepSOC(%d), MixCap(%d), prevMixCap(%d),VfOCV(0x%04x, %d)\n",
			vfsoc, chip->info.previous_vfsoc, repsoc, chip->info.previous_repsoc, (mixcap/2), (chip->info.previous_mixcap/2),  vfocv, pr_vfocv);

		mutex_lock(&chip->fg_lock);

		fg_write_and_verify_register(REMCAP_MIX_REG , chip->info.previous_mixcap);
		fg_write_register(VFOCV_REG, chip->info.previous_vfocv);
		mdelay(200);

		fg_write_and_verify_register(REMCAP_REP_REG, chip->info.previous_remcap);
		vfsoc = fg_read_register(VFSOC_REG);
		fg_write_register(0x60, 0x0080);
		fg_write_and_verify_register(0x48, vfsoc);
		fg_write_register(0x60, 0x0000);

		fg_write_and_verify_register(0x45, (chip->info.previous_vfcapacity / 4));
		fg_write_and_verify_register(0x46, 0x3200);
		fg_write_and_verify_register(FULLCAP_REG, chip->info.previous_fullcapacity);
		fg_write_and_verify_register(FULLCAP_NOM_REG, chip->info.previous_vfcapacity);

		mutex_unlock(&chip->fg_lock);

		msleep(200);

		/* ocv calculation for print */
		new_vfocv = fg_read_register(VFOCV_REG);
		temp = (new_vfocv & 0xFFF) * 78125;
		pr_vfocv = temp / 1000000;

		temp = ((new_vfocv & 0xF000) >> 4) * 78125;
		temp2 = temp / 1000000;
		pr_vfocv += (temp2 << 4);

		pr_info("[FG_Recovery] (A) newVfSOC(%d), newRepSOC(%d), newMixCap(%d), newVfOCV(0x%04x, %d)\n",
			fg_read_vfsoc(), fg_read_soc(), (fg_read_register(REMCAP_MIX_REG)/2), new_vfocv, pr_vfocv);

		fg_periodic_read();

		ret = 1;
	} else {
		chip->info.previous_vfsoc = vfsoc;
		chip->info.previous_repsoc = repsoc;
		chip->info.previous_remcap = remcap;
		chip->info.previous_mixcap = mixcap;
		chip->info.previous_fullcapacity = fullcapacity;
		chip->info.previous_vfcapacity = vffullcapacity;
		chip->info.previous_vfocv = vfocv;
	}

	return ret;
}

void fg_set_full_charged(void)
{
	pr_info("[FG_Set_Full] (B) FullCAP(%d), RemCAP(%d)\n",
		(fg_read_register(FULLCAP_REG)/2), (fg_read_register(REMCAP_REP_REG)/2));

	fg_write_register(FULLCAP_REG, (u16)fg_read_register(REMCAP_REP_REG));

	pr_info("[FG_Set_Full] (A) FullCAP(%d), RemCAP(%d)\n",
		(fg_read_register(FULLCAP_REG)/2), (fg_read_register(REMCAP_REP_REG)/2));
}

static void display_low_batt_comp_cnt(void)
{
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);
	u8 type_str[10];

	if (fg_get_battery_type() == SDI_BATTERY_TYPE)
		sprintf(type_str, "SDI");
	else if (fg_get_battery_type() == ATL_BATTERY_TYPE)
		sprintf(type_str, "ATL");
	else
		sprintf(type_str, "Unknown");

	pr_info("Check Array(%s) : [%d, %d], [%d, %d], [%d, %d], [%d, %d], [%d, %d]\n", type_str,
			chip->info.low_batt_comp_cnt[0][0], chip->info.low_batt_comp_cnt[0][1],
			chip->info.low_batt_comp_cnt[1][0], chip->info.low_batt_comp_cnt[1][1],
			chip->info.low_batt_comp_cnt[2][0], chip->info.low_batt_comp_cnt[2][1],
			chip->info.low_batt_comp_cnt[3][0], chip->info.low_batt_comp_cnt[3][1],
			chip->info.low_batt_comp_cnt[4][0], chip->info.low_batt_comp_cnt[4][1]);
}

static void add_low_batt_comp_cnt(int range, int level)
{
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);
	int i;
	int j;

	/* Increase the requested count value, and reset others. */
	chip->info.low_batt_comp_cnt[range-1][level/2]++;

	for (i = 0; i < LOW_BATT_COMP_RANGE_NUM; i++) {
		for (j = 0; j < LOW_BATT_COMP_LEVEL_NUM; j++) {
			if (i == range-1 && j == level/2)
				continue;
			else
				chip->info.low_batt_comp_cnt[i][j] = 0;
		}
	}
#ifdef INTENSIVE_LOW_COMPENSATION
	if (chip->info.low_batt_comp_cnt[range-1][level/2] == 2)
		chip->pre_cond_ok = 1;
#endif
}

void reset_low_batt_comp_cnt(void)
{
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);

	memset(chip->info.low_batt_comp_cnt, 0, sizeof(chip->info.low_batt_comp_cnt));
#ifdef INTENSIVE_LOW_COMPENSATION
	chip->pre_cond_ok = 0;
	chip->low_comp_pre_cond = 0;
#endif	
}

static int check_low_batt_comp_condtion(int *nLevel)
{
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);
	int i;
	int j;
	int ret = 0;

	for (i = 0; i < LOW_BATT_COMP_RANGE_NUM; i++) {
		for (j = 0; j < LOW_BATT_COMP_LEVEL_NUM; j++) {
#ifdef INTENSIVE_LOW_COMPENSATION
			if(chip->info.low_batt_comp_cnt[i][j]!=0)
				pr_info("BAT %s: chip->info.low_batt_comp_cnt[%d][%d] = %d \n", __func__, i, j, chip->info.low_batt_comp_cnt[i][j]);
#endif		
			if (chip->info.low_batt_comp_cnt[i][j] >= MAX_LOW_BATT_CHECK_CNT) {
				display_low_batt_comp_cnt();
				ret = 1;
				*nLevel = j*2 + 1;
				break;
			}
		}
	}

	return ret;
}

static int get_low_batt_threshold(int range, int level, int nCurrent)
{
	int ret = 0;

	if (fg_get_battery_type() == SDI_BATTERY_TYPE) {
		switch (range) {
		case 5:
			if (level == 1)
				ret = SDI_Range5_1_Offset + ((nCurrent * SDI_Range5_1_Slope) / 1000);
			else if (level == 3)
				ret = SDI_Range5_3_Offset + ((nCurrent * SDI_Range5_3_Slope) / 1000);
			break;
			
		case 4:
			if (level == 1)
				ret = SDI_Range4_1_Offset + ((nCurrent * SDI_Range4_1_Slope) / 1000);
			else if (level == 3)
				ret = SDI_Range4_3_Offset + ((nCurrent * SDI_Range4_3_Slope) / 1000);
			break;

		case 3:
			if (level == 1)
				ret = SDI_Range3_1_Offset + ((nCurrent * SDI_Range3_1_Slope) / 1000);
			else if (level == 3)
				ret = SDI_Range3_3_Offset + ((nCurrent * SDI_Range3_3_Slope) / 1000);
			break;

		case 2:
			if (level == 1)
				ret = SDI_Range2_1_Offset + ((nCurrent * SDI_Range2_1_Slope) / 1000);
			else if (level == 3)
				ret = SDI_Range2_3_Offset + ((nCurrent * SDI_Range2_3_Slope) / 1000);
			break;

		case 1:
			if (level == 1)
				ret = SDI_Range1_1_Offset + ((nCurrent * SDI_Range1_1_Slope) / 1000);
			else if (level == 3)
				ret = SDI_Range1_3_Offset + ((nCurrent * SDI_Range1_3_Slope) / 1000);
			break;

		default:
			break;
		}
	}  else if (fg_get_battery_type() == ATL_BATTERY_TYPE) {
		switch (range) {
		case 5:
			if (level == 1)
				ret = ATL_Range5_1_Offset + ((nCurrent * ATL_Range5_1_Slope) / 1000);
			else if (level == 3)
				ret = ATL_Range5_3_Offset + ((nCurrent * ATL_Range5_3_Slope) / 1000);
			break;
			
		case 4:
			if (level == 1)
				ret = ATL_Range4_1_Offset + ((nCurrent * ATL_Range4_1_Slope) / 1000);
			else if (level == 3)
				ret = ATL_Range4_3_Offset + ((nCurrent * ATL_Range4_3_Slope) / 1000);
			break;

		case 3:
			if (level == 1)
				ret = ATL_Range3_1_Offset + ((nCurrent * ATL_Range3_1_Slope) / 1000);
			else if (level == 3)
				ret = ATL_Range3_3_Offset + ((nCurrent * ATL_Range3_3_Slope) / 1000);
			break;

		case 2:
			if (level == 1)
				ret = ATL_Range2_1_Offset + ((nCurrent * ATL_Range2_1_Slope) / 1000);
			else if (level == 3)
				ret = ATL_Range2_3_Offset + ((nCurrent * ATL_Range2_3_Slope) / 1000);
			break;

		case 1:
			if (level == 1)
				ret = ATL_Range1_1_Offset + ((nCurrent * ATL_Range1_1_Slope) / 1000);
			else if (level == 3)
				ret = ATL_Range1_3_Offset + ((nCurrent * ATL_Range1_3_Slope) / 1000);
			break;

		default:
			break;
		}
	}

	return ret;
}

int p5_low_batt_compensation(int fg_soc, int fg_vcell, int fg_current)
{
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);
	int nRet = 0;
	int fg_avg_current = 0;
	int fg_min_current = 0;
	int new_level = 0;
	int bCntReset = 0;

	/* Not charging, flag is none, Under 3.60V or 3.45V */
	if (fg_vcell <= chip->info.check_start_vol) {
		fg_avg_current = fg_read_avg_current();
		fg_min_current = min(fg_avg_current, fg_current);

		if (fg_min_current < -2500) {
			if (fg_soc >= 2 && fg_vcell < get_low_batt_threshold(5, 1, fg_min_current))
				add_low_batt_comp_cnt(5, 1);
			else if (fg_soc >= 4 && fg_vcell < get_low_batt_threshold(5, 3, fg_min_current))
				add_low_batt_comp_cnt(5, 3);
			else
				bCntReset = 1;
		} else if (fg_min_current >= -2500 && fg_min_current < -1500) {
			if (fg_soc >= 2 && fg_vcell < get_low_batt_threshold(4, 1, fg_min_current))
				add_low_batt_comp_cnt(4, 1);
			else if (fg_soc >= 4 && fg_vcell < get_low_batt_threshold(4, 3, fg_min_current))
				add_low_batt_comp_cnt(4, 3);
			else
				bCntReset = 1;
		} else if (fg_min_current >= -1500 && fg_min_current < -600) {
			if (fg_soc >= 2 && fg_vcell < get_low_batt_threshold(3, 1, fg_min_current))
				add_low_batt_comp_cnt(3, 1);
			else if (fg_soc >= 4 && fg_vcell < get_low_batt_threshold(3, 3, fg_min_current))
				add_low_batt_comp_cnt(3, 3);
			else
				bCntReset = 1;
		} else if (fg_min_current >= -600 && fg_min_current < -200) {
			if (fg_soc >= 2 && fg_vcell < get_low_batt_threshold(2, 1, fg_min_current))
				add_low_batt_comp_cnt(2, 1);
			else if (fg_soc >= 4 && fg_vcell < get_low_batt_threshold(2, 3, fg_min_current))
				add_low_batt_comp_cnt(2, 3);
			else
				bCntReset = 1;
		} else if (fg_min_current >= -200 && fg_min_current < 0) {
			if (fg_soc >= 2 && fg_vcell < get_low_batt_threshold(1, 1, fg_min_current))
				add_low_batt_comp_cnt(1, 1);
			else if (fg_soc >= 4 && fg_vcell < get_low_batt_threshold(1, 3, fg_min_current))
				add_low_batt_comp_cnt(1, 3);
			else
				bCntReset = 1;
		}

		if (check_low_batt_comp_condtion(&new_level)) {
			fg_low_batt_compensation(new_level);
			reset_low_batt_comp_cnt();
#ifdef INTENSIVE_LOW_COMPENSATION
			nRet = 1;
#endif
		}

		if (bCntReset) {
			reset_low_batt_comp_cnt();
#ifdef INTENSIVE_LOW_COMPENSATION
			nRet = 2;
#endif
		}

		/* if compensation finished, then read SOC again!!*/
		if (chip->info.low_batt_comp_flag) {
			pr_info("%s : MIN_CURRENT(%d), AVG_CURRENT(%d), CURRENT(%d), SOC(%d), VCELL(%d)\n",
				__func__, fg_min_current, fg_avg_current, fg_current, fg_soc, fg_vcell);
			fg_soc = fg_read_soc();
			pr_info("%s : SOC is set to %d\n", __func__, fg_soc);
		}
	}

#ifdef KEEP_SOC_LEVEL1
	fg_prevent_early_poweroff(fg_vcell, fg_soc);
#endif
#ifndef INTENSIVE_LOW_COMPENSATION
	nRet = fg_soc;
#endif

	return nRet;
}

static void fg_set_battery_type(void)
{
	struct i2c_client *client = fg_i2c_client;
	struct max17042_chip *chip = i2c_get_clientdata(client);

	u16 data;
	u8 type_str[10];

	data = fg_read_register(DESIGNCAP_REG);

	if ((data == chip->pdata->sdi_vfcapacity) || (data == chip->pdata->sdi_vfcapacity-1)) {
		chip->info.battery_type = SDI_BATTERY_TYPE;
		sprintf(type_str, "SDI");
	} else if ((data == chip->pdata->atl_vfcapacity) || (data == chip->pdata->atl_vfcapacity-1)) {
		chip->info.battery_type = ATL_BATTERY_TYPE;
		sprintf(type_str, "ATL");
	} else {
		/*[11.05.23] prevent skip initialization*/
		chip->info.battery_type = SDI_BATTERY_TYPE;
		sprintf(type_str, "Unknown=>SDI");
	}

	pr_info("%s : DesignCAP(0x%04x), Battery type(%s)\n", __func__, data, type_str);

	switch (chip->info.battery_type) {
	case ATL_BATTERY_TYPE:
		chip->info.capacity = chip->pdata->atl_capacity;
		chip->info.vfcapacity = chip->pdata->atl_vfcapacity;
		break;

	case SDI_BATTERY_TYPE:
	default:
		chip->info.capacity = chip->pdata->sdi_capacity;
		chip->info.vfcapacity = chip->pdata->sdi_vfcapacity;
		break;
	}

	/* If not initialized yet, then init threshold values. */
	if (!chip->info.check_start_vol) {
		if (chip->info.battery_type == SDI_BATTERY_TYPE)
			chip->info.check_start_vol = 3600;
		else if (chip->info.battery_type == ATL_BATTERY_TYPE)
			chip->info.check_start_vol = 3450;
	}

}

int get_fuelgauge_value(int data)
{
	int ret;

	switch (data) {
	case FG_LEVEL:
		ret = fg_read_soc();
		break;

	case FG_TEMPERATURE:
		ret = fg_read_temp();
		break;

	case FG_VOLTAGE:
		ret = fg_read_vcell();
		break;

	case FG_CURRENT:
		ret = fg_read_current();
		break;

	case FG_CURRENT_AVG:
		ret = fg_read_avg_current();
		break;

	case FG_BATTERY_TYPE:
		ret = fg_get_battery_type();
		break;

	case FG_CHECK_STATUS:
		ret = fg_check_status_reg();
		break;

	case FG_VF_SOC:
		ret = fg_read_vfsoc();
		break;

#if defined(CONFIG_TARGET_LOCALE_KOR_SKT) || defined(CONFIG_TARGET_LOCALE_KOR_KT) || defined(CONFIG_TARGET_LOCALE_KOR_LGU)
	case FG_FULLCAP:
		ret = fg_read_register(FULLCAP_REG);
		break;

	case FG_FULLCAP_NOM:
		ret = fg_read_register(FULLCAP_NOM_REG);
		break;

	case FG_REMCAP_REP:
		ret = fg_read_register(REMCAP_REP_REG);
		break;

	case FG_REMCAP_MIX:
		ret = fg_read_register(REMCAP_MIX_REG);
		break;

	case FG_REMCAP_AV:
		ret = fg_read_register(REMCAP_AV_REG);
		break;

	case FG_VFOCV:
		ret = fg_read_register(VFOCV_REG);
		break;

	case FG_FILTERCFG:
		ret = fg_read_register(FILTERCFG_REG);
		break;
#endif

	default:
		ret = -1;
		break;
	}

	return ret;
}

#if defined(CONFIG_TARGET_LOCALE_KOR_SKT) || defined(CONFIG_TARGET_LOCALE_KOR_KT) || defined(CONFIG_TARGET_LOCALE_KOR_LGU)
int set_fuelgauge_value(int data, u16 value)
{
	int ret = 0;
	
	switch (data) {
	case FG_FILTERCFG:
		ret = fg_write_register(FILTERCFG_REG, value);
		break;

	default:
		ret = -2;
		break;
	}

	return ret;
}
#endif

static int max17042_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = get_fuelgauge_value(FG_VOLTAGE);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = get_fuelgauge_value(FG_LEVEL);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property max17042_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
};

#if defined(CONFIG_TARGET_LOCALE_KOR_SKT) || defined(CONFIG_TARGET_LOCALE_KOR_KT) || defined(CONFIG_TARGET_LOCALE_KOR_LGU)
static ssize_t max17042_show_property(struct device *dev,
				    struct device_attribute *attr, char *buf);

static ssize_t max17042_store_property(struct device *dev,
			     	struct device_attribute *attr,
			     	const char *buf, size_t count);

#define SEC_MAX17042_ATTR(_name) \
{ \
	.attr = { .name = #_name, .mode = S_IRUGO | (S_IWUSR | S_IWGRP) }, \
	.show = max17042_show_property, \
	.store = max17042_store_property, \
}

static struct device_attribute sec_max17042_attrs[] = {
	SEC_MAX17042_ATTR(fg_vfsoc),
	SEC_MAX17042_ATTR(fg_fullcap),
	SEC_MAX17042_ATTR(fg_fullcap_nom),
	SEC_MAX17042_ATTR(fg_remcap_rep),
	SEC_MAX17042_ATTR(fg_remcap_mix),
	SEC_MAX17042_ATTR(fg_remcap_av),
	SEC_MAX17042_ATTR(fg_vfocv),
	SEC_MAX17042_ATTR(fg_filtercfg),
	SEC_MAX17042_ATTR(fg_cgain),
};

enum {
	MAX17042_VFSOC = 0,
	MAX17042_FULLCAP,
	MAX17042_FULLCAP_NOM,
	MAX17042_REMCAP_REP,
	MAX17042_REMCAP_MIX,
	MAX17042_REMCAP_AV,
	MAX17042_VFOCV,
	MAX17042_FILTERCFG,
	MAX17042_CGAIN,
};

static ssize_t max17042_show_property(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	int i = 0;
	const ptrdiff_t off = attr - sec_max17042_attrs;
	int val;

	switch (off) {
	case MAX17042_VFSOC:
		val = fg_read_register(VFSOC_REG);
		val = val >> 8; /* % */
		if (val >= 0) 
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		else
			i = -EINVAL;
		break;
	case MAX17042_FULLCAP:
		val = fg_read_register(FULLCAP_REG);
		if (val >= 0) 
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		else
			i = -EINVAL;
		break;
	case MAX17042_FULLCAP_NOM:
		val = fg_read_register(FULLCAP_NOM_REG);
		if (val >= 0) 
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		else
			i = -EINVAL;
		break;
	case MAX17042_REMCAP_REP:
		val = fg_read_register(REMCAP_REP_REG);
		if (val >= 0) 
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		else
			i = -EINVAL;
		break;
	case MAX17042_REMCAP_MIX:
		val = fg_read_register(REMCAP_MIX_REG);
		if (val >= 0) 
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		else
			i = -EINVAL;
		break;
	case MAX17042_REMCAP_AV:
		val = fg_read_register(REMCAP_AV_REG);
		if (val >= 0) 
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		else
			i = -EINVAL;
		break;
	case MAX17042_VFOCV:
		val = fg_read_register(VFOCV_REG);
		val = ((val >> 3) * 625) / 1000; /* mV */
		if (val >= 0) 
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		else
			i = -EINVAL;
		break;
	case MAX17042_FILTERCFG:
		val = fg_read_register(FILTERCFG_REG);
		if (val >= 0) 
			i += scnprintf(buf + i, PAGE_SIZE - i, "0x%x\n", val);
		else
			i = -EINVAL;
		break;
	case MAX17042_CGAIN:
		val = fg_read_register(CGAIN_REG);
		if (val >= 0) 
			i += scnprintf(buf + i, PAGE_SIZE - i, "0x%x\n", val);
		else
			i = -EINVAL;
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

static ssize_t max17042_store_property(struct device *dev,
			     	struct device_attribute *attr,
			     	const char *buf, size_t count)
{
	int ret = 0;
	const ptrdiff_t off = attr - sec_max17042_attrs;

	switch (off) {
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int max17042_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(sec_max17042_attrs); i++) {
		rc = device_create_file(dev, &sec_max17042_attrs[i]);
		if (rc)
			goto max17042_attrs_failed;
	}
	goto succeed;

max17042_attrs_failed:
	while (i--)
		device_remove_file(dev, &sec_max17042_attrs[i]);
succeed:
	return rc;
}
#endif

static int max17042_remove(struct i2c_client *client)
{
	struct max17042_chip *chip = i2c_get_clientdata(client);

	kfree(chip);
	return 0;
}

static int max17042_probe(struct i2c_client *client,  const struct i2c_device_id *id)
{
	struct max17042_chip *chip;
	int ret = 0;

	if (!client->dev.platform_data) {
		pr_err("%s : No platform data\n", __func__);
		return -EINVAL;
	}

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		pr_info("failed to allocate memory.\n");
		return -ENOMEM;
	}

	chip->client = client;
	chip->pdata = client->dev.platform_data;
	i2c_set_clientdata(client, chip);

	chip->pdata->hw_init(); /* important */
	max17042_chip_data = chip;
	/* rest of the initialisation goes here. */
	pr_info("Fuel gauge attach success!!!\n");

	fg_i2c_client = client;
	fg_set_battery_type();

	/* Init parameters to prevent wrong compensation. */
	chip->info.previous_fullcap = fg_read_register(FULLCAP_REG);
	chip->info.previous_vffullcap = fg_read_register(FULLCAP_NOM_REG);
	/* Init FullCAP of first full charging. */
	chip->info.full_charged_cap = chip->info.previous_fullcap;

	chip->info.previous_vfsoc = fg_read_vfsoc();
	chip->info.previous_repsoc = fg_read_soc();
	chip->info.previous_remcap = fg_read_register(REMCAP_REP_REG);
	chip->info.previous_mixcap = fg_read_register(REMCAP_MIX_REG);
	chip->info.previous_vfocv = fg_read_register(VFOCV_REG);
	chip->info.previous_fullcapacity = chip->info.previous_fullcap;
	chip->info.previous_vfcapacity = chip->info.previous_vffullcap;

	mutex_init(&chip->fg_lock);

	fg_read_model_data();
	fg_periodic_read();

	chip->battery.name = "fuelgauge";
	chip->battery.type	= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= max17042_get_property;
	chip->battery.properties	= max17042_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17042_battery_props);
	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		kfree(chip);
		return ret;
	}

#if defined(CONFIG_TARGET_LOCALE_KOR_SKT) || defined(CONFIG_TARGET_LOCALE_KOR_KT) || defined(CONFIG_TARGET_LOCALE_KOR_LGU)
	/* create max17042 attributes */
	max17042_create_attrs(chip->battery.dev);
#endif

	return 0;
}

static const struct i2c_device_id max17042_id[] = {
	{ "max17042", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17042_id);

static struct i2c_driver max17042_i2c_driver = {
	.driver = {
		.name = "max17042",
	},
	.probe	= max17042_probe,
	.remove		= __devexit_p(max17042_remove),
	.id_table	= max17042_id,
};

static int __init max17042_init(void)
{
	return i2c_add_driver(&max17042_i2c_driver);
}
subsys_initcall(max17042_init);

static void __exit max17042_exit(void)
{
	i2c_del_driver(&max17042_i2c_driver);
}
module_exit(max17042_exit);

MODULE_DESCRIPTION("MAX17042 Fuel Gauge");
MODULE_LICENSE("GPL");
