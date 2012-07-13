/*
 *  max17042_battery.h
 *  fuel-gauge systems for lithium-ion (Li+) batteries
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
 */

#ifndef _LINUX_MAX_17042_BATTERY_H
#define _LINUX_MAX_17042_BATTERY_H
#define INTENSIVE_LOW_COMPENSATION
#if (defined(CONFIG_TARGET_SERIES_P5LTE) || defined(CONFIG_TARGET_SERIES_P8LTE))
#define KEEP_SOC_LEVEL1
#define POWER_OFF_SOC_HIGH_MARGIN	0x181	//1.5%
#define POWER_OFF_VOLTAGE_HIGH_MARGIN	3500
#endif

/* Register address */
#define STATUS_REG				0x00
#define VALRT_THRESHOLD_REG		0x01
#define TALRT_THRESHOLD_REG		0x02
#define SALRT_THRESHOLD_REG		0x03
#define REMCAP_REP_REG			0x05
#define SOCREP_REG				0x06
#define TEMPERATURE_REG			0x08
#define VCELL_REG				0x09
#define CURRENT_REG				0x0A
#define AVG_CURRENT_REG			0x0B
#define SOCMIX_REG				0x0D
#define SOCAV_REG				0x0E
#define REMCAP_MIX_REG			0x0F
#define FULLCAP_REG				0x10
#define RFAST_REG				0x15
#define AVR_TEMPERATURE_REG		0x16
#define CYCLES_REG				0x17
#define DESIGNCAP_REG			0x18
#define AVR_VCELL_REG			0x19
#define CONFIG_REG				0x1D
#define REMCAP_AV_REG			0x1F
#define FULLCAP_NOM_REG			0x23
#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_KT) || defined(CONFIG_KOR_OPERATOR_LGU) || defined(CONFIG_JPN_OPERATOR_NTT)
#define FILTERCFG_REG			0x29
#define CGAIN_REG				0x2E
#endif
#define MISCCFG_REG				0x2B
#define RCOMP_REG				0x38
#define FSTAT_REG				0x3D
#define DQACC_REG				0x45
#define DPACC_REG				0x46
#define OCV_REG					0xEE
#define VFOCV_REG				0xFB
#define VFSOC_REG				0xFF

#define FG_LEVEL 0
#define FG_TEMPERATURE 1
#define FG_VOLTAGE 2
#define FG_CURRENT 3
#define FG_CURRENT_AVG 4
#define FG_BATTERY_TYPE 5
#define FG_CHECK_STATUS 6
#define FG_VF_SOC 7
#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_KT) || defined(CONFIG_KOR_OPERATOR_LGU)  || defined(CONFIG_JPN_OPERATOR_NTT)
#define FG_FULLCAP 8
#define FG_FULLCAP_NOM 9
#define FG_REMCAP_REP 10
#define FG_REMCAP_MIX 11
#define FG_REMCAP_AV 12
#define FG_VFOCV 13
#define FG_FILTERCFG 14
#endif
#if defined (CONFIG_TARGET_SERIES_P8LTE) && defined (CONFIG_KOR_OPERATOR_SKT)
#define FG_AVSOC 15
#endif

#define LOW_BATT_COMP_RANGE_NUM	5
#define LOW_BATT_COMP_LEVEL_NUM	2
#ifdef INTENSIVE_LOW_COMPENSATION
#define MAX_LOW_BATT_CHECK_CNT	12
#else
#define MAX_LOW_BATT_CHECK_CNT	2
#endif
#define MAX17042_CURRENT_UNIT	15625 / 100000

struct max17042_platform_data {
	int sdi_capacity;
	int sdi_vfcapacity;
	int atl_capacity;
	int atl_vfcapacity;
	int fuel_alert_line;
	void (*hw_init)(void);
};

struct fuelgauge_info {
	/* test print count */
	int pr_cnt;
	/* battery type */
	int battery_type;
	/* full charge comp */
	u32 previous_fullcap;
	u32 previous_vffullcap;
	u32 full_charged_cap;
	/* capacity and vfcapacity */
	u16 capacity;
	u16 vfcapacity;
	int soc_restart_flag;
	/* cap corruption check */
	u32 previous_repsoc;
	u32 previous_vfsoc;
	u32 previous_remcap;
	u32 previous_mixcap;
	u32 previous_fullcapacity;
	u32 previous_vfcapacity;
	u32 previous_vfocv;
	/* low battery comp */
	int low_batt_comp_cnt[LOW_BATT_COMP_RANGE_NUM][LOW_BATT_COMP_LEVEL_NUM];
	int check_start_vol;
	int low_batt_comp_flag;
#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_KT) || defined(CONFIG_KOR_OPERATOR_LGU)  || defined(CONFIG_JPN_OPERATOR_NTT)
	int psoc;
#endif
};

struct max17042_chip {
	struct i2c_client		*client;
	struct max17042_platform_data	*pdata;
	struct power_supply		battery;
	struct fuelgauge_info	info;
	struct mutex			fg_lock;
#ifdef INTENSIVE_LOW_COMPENSATION
	int pre_cond_ok;
	int low_comp_pre_cond;
#endif	
};

/* SDI type low battery compensation offset */
#ifdef CONFIG_TARGET_SERIES_P4LTE
#if 0
#define SDI_Range5_1_Offset		3318
#define SDI_Range5_3_Offset		3383
#define SDI_Range4_1_Offset		3451 //3371
#define SDI_Range4_3_Offset		3618 //3478
#define SDI_Range3_1_Offset		3453 //3453
#define SDI_Range3_3_Offset		3615 //3614
#define SDI_Range2_1_Offset		3447 //3447
#define SDI_Range2_3_Offset		3606 //3606
#define SDI_Range1_1_Offset		3438 //3438
#define SDI_Range1_3_Offset		3591 //3591

#define SDI_Range5_1_Slope		0
#define SDI_Range5_3_Slope		0
#define SDI_Range4_1_Slope		53 //0
#define SDI_Range4_3_Slope		94 //0
#define SDI_Range3_1_Slope		54 //50
#define SDI_Range3_3_Slope		92 //90
#define SDI_Range2_1_Slope		45 //50
#define SDI_Range2_3_Slope		78 //78
#define SDI_Range1_1_Slope		0 //0
#define SDI_Range1_3_Slope		0 //0
#else 
//09.01 new table
#define SDI_Range5_1_Offset		3283
#define SDI_Range5_3_Offset		3348
#define SDI_Range4_1_Offset		3450
#define SDI_Range4_3_Offset		3618
#define SDI_Range3_1_Offset		3445
#define SDI_Range3_3_Offset		3605
#define SDI_Range2_1_Offset		3442
#define SDI_Range2_3_Offset		3598
#define SDI_Range1_1_Offset		3401
#define SDI_Range1_3_Offset		3580

#define SDI_Range5_1_Slope		0
#define SDI_Range5_3_Slope		0
#define SDI_Range4_1_Slope		67
#define SDI_Range4_3_Slope		108
#define SDI_Range3_1_Slope		63
#define SDI_Range3_3_Slope		99
#define SDI_Range2_1_Slope		59
#define SDI_Range2_3_Slope		87
#define SDI_Range1_1_Slope		0
#define SDI_Range1_3_Slope		0
#endif

#elif (defined(CONFIG_TARGET_SERIES_P5LTE) || defined(CONFIG_TARGET_SERIES_P8LTE))
/* SDI type low battery compensation offset */
//Range4 : current consumption is more than 1.5A
//Range3 : current consumption is between 0.6A ~ 1.5A
//Range2 : current consumption is between 0.2A ~ 0.6A
//Range1 : current consumption is less than 0.2A
//11.09. 01 update 
#define SDI_Range5_1_Offset		3283	//3308
#define SDI_Range5_3_Offset		3330	//3365
#define SDI_Range4_1_Offset		3451	//3361
#define SDI_Range4_3_Offset		3596	//3448
#define SDI_Range3_1_Offset		3445	//3438
#define SDI_Range3_3_Offset		3587	//3634
#define SDI_Range2_1_Offset		3442	//3459
#define SDI_Range2_3_Offset		3589	//3606
#define SDI_Range1_1_Offset		3431	//3442
#define SDI_Range1_3_Offset		3568	//3591

#define SDI_Range5_1_Slope		0
#define SDI_Range5_3_Slope		0
#define SDI_Range4_1_Slope		67	//52	//0
#define SDI_Range4_3_Slope		106	//92	//0
#define SDI_Range3_1_Slope		63	//51	//50
#define SDI_Range3_3_Slope		101	//114	//124
#define SDI_Range2_1_Slope		59	//85	//90
#define SDI_Range2_3_Slope		104	//78	//78
#define SDI_Range1_1_Slope		0	//0	//0
#define SDI_Range1_3_Slope		0	//0	//0

#elif defined(CONFIG_TARGET_SERIES_P8LTE)
#if 0
#define SDI_Range5_1_Offset		3308
#define SDI_Range5_3_Offset		3365
#define SDI_Range4_1_Offset		3440	//3361
#define SDI_Range4_3_Offset		3600	//3448
#define SDI_Range3_1_Offset		3439	//3438
#define SDI_Range3_3_Offset		3628	//3634
#define SDI_Range2_1_Offset		3459	//3459
#define SDI_Range2_3_Offset		3606	//3606
#define SDI_Range1_1_Offset		3441	//3442
#define SDI_Range1_3_Offset		3591	//3591

#define SDI_Range5_1_Slope		0
#define SDI_Range5_3_Slope		0
#define SDI_Range4_1_Slope		52	//0
#define SDI_Range4_3_Slope		92	//0
#define SDI_Range3_1_Slope		51	//50
#define SDI_Range3_3_Slope		114	//124
#define SDI_Range2_1_Slope		85	//90
#define SDI_Range2_3_Slope		78	//78
#define SDI_Range1_1_Slope		0	//0
#define SDI_Range1_3_Slope		0	//0
#else
/* Current range for P8(not dependent on battery type */
#define CURRENT_RANGE1	0
#define CURRENT_RANGE2	-200
#define CURRENT_RANGE3	-600
#define CURRENT_RANGE4	-1500
#define CURRENT_RANGE5	-2500
#define CURRENT_RANGE_MAX	CURRENT_RANGE5
#define CURRENT_RANGE_MAX_NUM	5
/* SDI type low battery compensation Slope & Offset for 1% SOC range*/
#define SDI_Range1_1_Slope		0
#define SDI_Range2_1_Slope		54
#define SDI_Range3_1_Slope		66
#define SDI_Range4_1_Slope		69
#define SDI_Range5_1_Slope		0

#define SDI_Range1_1_Offset		3391
#define SDI_Range2_1_Offset		3402
#define SDI_Range3_1_Offset		3409
#define SDI_Range4_1_Offset		3414
#define SDI_Range5_1_Offset		3240

/* SDI type low battery compensation Slope & Offset for 3% SOC range*/
#define SDI_Range1_3_Slope		0
#define SDI_Range2_3_Slope		92
#define SDI_Range3_3_Slope		125
#define SDI_Range4_3_Slope		110
#define SDI_Range5_3_Slope		0

#define SDI_Range1_3_Offset		3524
#define SDI_Range2_3_Offset		3542
#define SDI_Range3_3_Offset		3562
#define SDI_Range4_3_Offset		3539
#define SDI_Range5_3_Offset		3265

/* ATL type low battery compensation offset */
#define ATL_Range4_1_Offset		3298
#define ATL_Range4_3_Offset		3330
#define ATL_Range3_1_Offset		3375
#define ATL_Range3_3_Offset		3445
#define ATL_Range2_1_Offset		3371
#define ATL_Range2_3_Offset		3466
#define ATL_Range1_1_Offset		3362
#define ATL_Range1_3_Offset		3443

#define ATL_Range4_1_Slope		0
#define ATL_Range4_3_Slope		0
#define ATL_Range3_1_Slope		50
#define ATL_Range3_3_Slope		77
#define ATL_Range2_1_Slope		40
#define ATL_Range2_3_Slope		111
#define ATL_Range1_1_Slope		0
#define ATL_Range1_3_Slope		0
#endif

#else //P8LTE
/* SDI type low battery compensation offset */
#define SDI_Range4_1_Offset		3361 //3369
#define SDI_Range4_3_Offset		3448 //3469
#define SDI_Range3_1_Offset		3438 //3453
#define SDI_Range3_3_Offset		3634 //3619
#define SDI_Range2_1_Offset		3459 //3447
#define SDI_Range2_3_Offset		3606 //3606
#define SDI_Range1_1_Offset		3442 //3438
#define SDI_Range1_3_Offset		3590 //3590

#define SDI_Range4_1_Slope		0
#define SDI_Range4_3_Slope		0
#define SDI_Range3_1_Slope		50 //60
#define SDI_Range3_3_Slope		124 //100
#define SDI_Range2_1_Slope		90 //50
#define SDI_Range2_3_Slope		78 //77
#define SDI_Range1_1_Slope		0
#define SDI_Range1_3_Slope		0
#endif

/* ATL type low battery compensation offset */
#define ATL_Range5_1_Offset		3298
#define ATL_Range5_3_Offset		3330
#define ATL_Range4_1_Offset		3298
#define ATL_Range4_3_Offset		3330
#define ATL_Range3_1_Offset		3375
#define ATL_Range3_3_Offset		3445
#define ATL_Range2_1_Offset		3371
#define ATL_Range2_3_Offset		3466
#define ATL_Range1_1_Offset		3362
#define ATL_Range1_3_Offset		3443

#define ATL_Range5_1_Slope		0
#define ATL_Range5_3_Slope		0
#define ATL_Range4_1_Slope		0
#define ATL_Range4_3_Slope		0
#define ATL_Range3_1_Slope		50
#define ATL_Range3_3_Slope		77
#define ATL_Range2_1_Slope		40
#define ATL_Range2_3_Slope		111
#define ATL_Range1_1_Slope		0
#define ATL_Range1_3_Slope		0

enum {
	POSITIVE = 0,
	NEGATIVE,
};

enum {
	UNKNOWN_TYPE = 0,
	SDI_BATTERY_TYPE,
	ATL_BATTERY_TYPE,
};

void fg_periodic_read(void);

extern int fg_reset_soc(void);
extern int fg_reset_capacity(void);
extern int fg_adjust_capacity(void);
extern void fg_low_batt_compensation(u32 level);
extern int fg_alert_init(void);
extern void fg_fullcharged_compensation(u32 is_recharging, u32 pre_update);
extern void fg_check_vf_fullcap_range(void);
extern int fg_check_cap_corruption(void);
extern void fg_set_full_charged(void);
extern int get_fuelgauge_value(int data);
#if defined(CONFIG_KOR_OPERATOR_SKT) || defined(CONFIG_KOR_OPERATOR_KT) || defined(CONFIG_KOR_OPERATOR_LGU)  || defined(CONFIG_JPN_OPERATOR_NTT)
extern int set_fuelgauge_value(int data, u16 value);
#endif
#if defined (CONFIG_TARGET_SERIES_P8LTE) && defined (CONFIG_KOR_OPERATOR_SKT)
extern void fg_recovery_adjust_repsoc(u32 level);
#endif
#endif
