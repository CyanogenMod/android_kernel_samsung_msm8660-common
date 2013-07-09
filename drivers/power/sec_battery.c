/*
 *  sec_battery.c
 *  Samsung Mobile Battery Driver
 *
 *  Copyright (C) 2011 Samsung Electronics
 *
 *  <jongmyeong.ko@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/android_alarm.h>
#include <linux/msm_adc.h>
#include <linux/earlysuspend.h>
#include <mach/sec_battery.h>
#include <linux/gpio.h>
#include <linux/pmic8058-xoadc.h>

/* MPP_CHECK_FEATURE is in sec_battery.h */
/* ADJUST_RCOMP_WITH_TEMPER  : default, not used, refer to fuelguae source */
#define ADC_QUEUE_FEATURE
/* #define PRE_CHANOPEN_FEATURE */
/* #define USE_COMP_METHOD_OTHER */
#define GET_TOPOFF_WITH_REGISTER
/* #define CHK_TOPOFF_WITH_REGISTER_ONLY */
#ifdef CONFIG_USA_MODEL_SGH_I717
#define ADJUST_RCOMP_WITH_CHARGING_STATUS
#endif
#if defined(ADC_QUEUE_FEATURE) || defined(PRE_CHANOPEN_FEATURE)
#define MAX_BATT_ADC_CHAN 4
#endif

#define POLLING_INTERVAL	(40 * 1000)
#define MEASURE_DSG_INTERVAL	(20 * 1000)
#define MEASURE_CHG_INTERVAL	(5 * 1000)

#if defined(CONFIG_KOR_MODEL_SHV_E120L)
#if defined(CONFIG_PMIC8058_XOADC_CAL)
#define CURRENT_OF_FULL_CHG		2750	/* 210mA */
#else
#define CURRENT_OF_FULL_CHG		2100	/* 210mA */
#endif
#define RCOMP0_TEMP			20	/* 'C */
#elif defined(CONFIG_KOR_MODEL_SHV_E120S) || \
	defined(CONFIG_KOR_MODEL_SHV_E120K)
#if defined(CONFIG_PMIC8058_XOADC_CAL)
#define CURRENT_OF_FULL_CHG		2700	/* 230mA */
#else
#define CURRENT_OF_FULL_CHG		2300	/* 230mA */
#endif /* CONFIG_PMIC8058_XOADC_CAL */
#define RCOMP0_TEMP			20	/* 'C */
#elif defined(CONFIG_KOR_MODEL_SHV_E160S) || \
	defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
#if defined(CONFIG_PMIC8058_XOADC_CAL)
#define CURRENT_OF_FULL_CHG		2300	/* 170mA */
#else
#define CURRENT_OF_FULL_CHG		2700	/* 270mA */
#endif /* CONFIG_PMIC8058_XOADC_CAL */
#define RCOMP0_TEMP			20	/* 'C */
#elif defined(CONFIG_KOR_MODEL_SHV_E160K)
#if defined(CONFIG_PMIC8058_XOADC_CAL)
#define CURRENT_OF_FULL_CHG		2200	/* 170mA */
#else
#define CURRENT_OF_FULL_CHG		2700	/* 270mA */
#endif /* CONFIG_PMIC8058_XOADC_CAL */
#define RCOMP0_TEMP			20	/* 'C */
#elif defined(CONFIG_TARGET_LOCALE_USA)
#if defined(CONFIG_USA_MODEL_SGH_I727) || \
	defined(CONFIG_USA_MODEL_SGH_I757) || \
	defined(CONFIG_USA_MODEL_SGH_I577) || \
	defined(CONFIG_CAN_MODEL_SGH_I577R) || \
	defined(CONFIG_CAN_MODEL_SGH_I757M)
#if defined(CONFIG_PMIC8058_XOADC_CAL)
#define CURRENT_OF_FULL_CHG_UI		2780	/* 278mA */
#define CURRENT_OF_FULL_CHG		2780	/* 278mA */
#define RCOMP0_TEMP			20	/* 'C */
#else
#define CURRENT_OF_FULL_CHG_UI		2300	/* 278mA */
#define CURRENT_OF_FULL_CHG		2300	/* 278mA */
#define RCOMP0_TEMP			20	/* 'C */
#endif /* CONFIG_PMIC8058_XOADC_CAL */

#elif defined(CONFIG_USA_MODEL_SGH_I717)

#if defined(CONFIG_PMIC8058_XOADC_CAL)
#define CURRENT_OF_FULL_CHG_UI		1800	/* 1800mA */
#define CURRENT_OF_FULL_CHG		1800	/* 1800mA */
#define RCOMP0_TEMP			20	/* 'C */
#else
#define CURRENT_OF_FULL_CHG_UI		1500	/* 1800mA */
#define CURRENT_OF_FULL_CHG		1500	/* 1800mA */
#define RCOMP0_TEMP			20	/* 'C */
#endif /* CONFIG_PMIC8058_XOADC_CAL */

#else

#if defined(CONFIG_PMIC8058_XOADC_CAL)
#define CURRENT_OF_FULL_CHG_UI		2800	/* 280mA */
#define CURRENT_OF_FULL_CHG		2400	/* 240mA */
#define RCOMP0_TEMP			20	/* 'C */
#else
#define CURRENT_OF_FULL_CHG_UI		2400	/* 280mA */
#define CURRENT_OF_FULL_CHG		2000	/* 240mA */
#define RCOMP0_TEMP			20	/* 'C */
#endif /* CONFIG_PMIC8058_XOADC_CAL */

#endif /* CONFIG_USA_MODEL_SGH_I727 */
#else /* default : celox-skt */
#if defined(CONFIG_PMIC8058_XOADC_CAL)
#define CURRENT_OF_FULL_CHG		2700	/* 230mA */
#else
#define CURRENT_OF_FULL_CHG		2300	/* 230mA */
#endif /* CONFIG_PMIC8058_XOADC_CAL */
#define RCOMP0_TEMP			20	/* 'C */
#endif /* CONFIG_KOR_MODEL_SHV_E120L */

/* new concept : in case of time-out charging stop,
  Do not update FULL for UI,
  Use same time-out value for first charing and re-charging
*/
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || \
	defined(CONFIG_KOR_MODEL_SHV_E160K) || \
	defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
#define FULL_CHARGING_TIME	(8 * 60 * 60 * HZ)	/* 8hr */
#define RECHARGING_TIME		(2 * 60 * 60 * HZ)	/* 2hr */
#else
#define FULL_CHARGING_TIME	(6 * 60 * 60 * HZ)	/* 6hr */
#define RECHARGING_TIME		(2 * 60 * 60 * HZ)	/* 2hr */
#endif

#if defined(CONFIG_USA_MODEL_SGH_I577) || \
	defined(CONFIG_CAN_MODEL_SGH_I577R)
#define RECHARGING_VOLTAGE	(4140 * 1000)		/* 4.14 V */
#else
#define RECHARGING_VOLTAGE	(4130 * 1000)		/* 4.13 V */
#endif

#if defined(CONFIG_USA_MODEL_SGH_I577) || \
	defined(CONFIG_CAN_MODEL_SGH_I577R)
#define HIGH_BLOCK_TEMP_ADC			706
#define HIGH_RECOVER_TEMP_ADC			700
#define EVT_HIGH_BLOCK_TEMP_ADC			707
#define EVT_HIGH_RECOVER_TEMP_ADC		701
#define LOW_BLOCK_TEMP_ADC			514
#define LOW_RECOVER_TEMP_ADC			524

#define NB_HIGH_TEMP_ADC_DELTA			60
#define NB_HIGH_BLOCK_TEMP_ADC			585
#define NB_HIGH_RECOVER_TEMP_ADC		670
#define NB_EVT_HIGH_BLOCK_TEMP_ADC		380
#define NB_EVT_HIGH_RECOVER_TEMP_ADC		705
#define NB_LOW_BLOCK_TEMP_ADC			1690
#define NB_LOW_RECOVER_TEMP_ADC			1610

#elif defined(CONFIG_USA_MODEL_SGH_I727)
#if defined(CONFIG_PMIC8058_XOADC_CAL)
#define HIGH_BLOCK_TEMP_ADC			706
#define HIGH_RECOVER_TEMP_ADC			700
#define EVT_HIGH_BLOCK_TEMP_ADC			707
#define EVT_HIGH_RECOVER_TEMP_ADC		701
#define LOW_BLOCK_TEMP_ADC			514
#define LOW_RECOVER_TEMP_ADC			524

#if defined(CONFIG_EUR_MODEL_GT_I9210)
#define NB_HIGH_BLOCK_TEMP_ADC			310
#define NB_HIGH_RECOVER_TEMP_ADC		645
#else
#define NB_HIGH_BLOCK_TEMP_ADC			570
#define NB_HIGH_RECOVER_TEMP_ADC		698
#endif
#define NB_EVT_HIGH_BLOCK_TEMP_ADC		335
#define NB_EVT_HIGH_RECOVER_TEMP_ADC		645
#define NB_LOW_BLOCK_TEMP_ADC			1670
#define NB_LOW_RECOVER_TEMP_ADC			1615

#else

#define HIGH_BLOCK_TEMP_ADC			706
#define HIGH_RECOVER_TEMP_ADC			700
#define EVT_HIGH_BLOCK_TEMP_ADC			707
#define EVT_HIGH_RECOVER_TEMP_ADC		701
#define LOW_BLOCK_TEMP_ADC			514
#define LOW_RECOVER_TEMP_ADC			524

#if defined(CONFIG_EUR_MODEL_GT_I9210)
#define NB_HIGH_BLOCK_TEMP_ADC			230
#define NB_HIGH_RECOVER_TEMP_ADC		560
#else
#define NB_HIGH_BLOCK_TEMP_ADC			485
#define NB_HIGH_RECOVER_TEMP_ADC		613
#endif
#define NB_EVT_HIGH_BLOCK_TEMP_ADC		250
#define NB_EVT_HIGH_RECOVER_TEMP_ADC		560
#define NB_LOW_BLOCK_TEMP_ADC			1610
#define NB_LOW_RECOVER_TEMP_ADC			1535
#endif /* CONFIG_PMIC8058_XOADC_CAL */

#elif defined(CONFIG_USA_MODEL_SGH_I757) || \
	defined(CONFIG_CAN_MODEL_SGH_I757M)
#define HIGH_BLOCK_TEMP_ADC			706
#define HIGH_RECOVER_TEMP_ADC			700
#define EVT_HIGH_BLOCK_TEMP_ADC			707
#define EVT_HIGH_RECOVER_TEMP_ADC		701
#define LOW_BLOCK_TEMP_ADC			514
#define LOW_RECOVER_TEMP_ADC			524

#define NB_HIGH_BLOCK_TEMP_ADC			570
#define NB_HIGH_BLOCK_TEMP_ADC_ON		561
#define NB_HIGH_RECOVER_TEMP_ADC		648
#define NB_HIGH_RECOVER_TEMP_ADC_ON		686
#define NB_EVT_HIGH_BLOCK_TEMP_ADC		358
#define NB_EVT_HIGH_RECOVER_TEMP_ADC		648
#define NB_EVT_HIGH_RECOVER_TEMP_ADC_ON		686
#define NB_LOW_BLOCK_TEMP_ADC			1670
#define NB_LOW_RECOVER_TEMP_ADC			1584

#elif defined(CONFIG_USA_MODEL_SGH_I717)
#define HIGH_BLOCK_TEMP_ADC			706
#define HIGH_RECOVER_TEMP_ADC			700
#define EVT_HIGH_BLOCK_TEMP_ADC			707
#define EVT_HIGH_RECOVER_TEMP_ADC		701
#define LOW_BLOCK_TEMP_ADC			514
#define LOW_RECOVER_TEMP_ADC			524

#define NB_HIGH_TEMP_ADC_DELTA			20
#define NB_HIGH_BLOCK_TEMP_ADC			572
#define NB_HIGH_RECOVER_TEMP_ADC		685
#define NB_EVT_HIGH_BLOCK_TEMP_ADC		380
#define NB_EVT_HIGH_RECOVER_TEMP_ADC		630
#define NB_LOW_BLOCK_TEMP_ADC			1670
#define NB_LOW_RECOVER_TEMP_ADC			1615

#elif defined(CONFIG_USA_MODEL_SGH_T989D)
#define HIGH_BLOCK_TEMP_ADC			626
#define HIGH_RECOVER_TEMP_ADC			614
#define EVT_HIGH_BLOCK_TEMP_ADC			707
#define EVT_HIGH_RECOVER_TEMP_ADC		701
#define LOW_BLOCK_TEMP_ADC			513
#define LOW_RECOVER_TEMP_ADC			522

#define NB_HIGH_BLOCK_TEMP_ADC			300
#define NB_HIGH_RECOVER_TEMP_ADC		620
#define NB_EVT_HIGH_BLOCK_TEMP_ADC		323
#define NB_EVT_HIGH_RECOVER_TEMP_ADC		606
#define NB_LOW_BLOCK_TEMP_ADC			1650
#define NB_LOW_RECOVER_TEMP_ADC			1600

#elif defined(CONFIG_USA_MODEL_SGH_T769)
#define HIGH_BLOCK_TEMP_ADC			626
#define HIGH_RECOVER_TEMP_ADC			614
#define EVT_HIGH_BLOCK_TEMP_ADC			707
#define EVT_HIGH_RECOVER_TEMP_ADC		701
#define LOW_BLOCK_TEMP_ADC			513
#define LOW_RECOVER_TEMP_ADC			522

#define NB_HIGH_BLOCK_TEMP_ADC			510
#define NB_HIGH_RECOVER_TEMP_ADC		637
#define NB_EVT_HIGH_BLOCK_TEMP_ADC		300
#define NB_EVT_HIGH_RECOVER_TEMP_ADC		595
#define NB_LOW_BLOCK_TEMP_ADC			1675
#define NB_LOW_RECOVER_TEMP_ADC			1582

#elif defined(CONFIG_USA_MODEL_SGH_T989)
#if defined(CONFIG_PMIC8058_XOADC_CAL)
#define HIGH_BLOCK_TEMP_ADC			626
#define HIGH_RECOVER_TEMP_ADC			614
#define EVT_HIGH_BLOCK_TEMP_ADC			707
#define EVT_HIGH_RECOVER_TEMP_ADC		701
#define LOW_BLOCK_TEMP_ADC			513
#define LOW_RECOVER_TEMP_ADC			522

#define NB_HIGH_BLOCK_TEMP_ADC			545
#define NB_HIGH_RECOVER_TEMP_ADC		651
#define NB_EVT_HIGH_BLOCK_TEMP_ADC		323
#define NB_EVT_HIGH_RECOVER_TEMP_ADC		606
#define NB_LOW_BLOCK_TEMP_ADC			1670
#define NB_LOW_RECOVER_TEMP_ADC			1577

#else

#define HIGH_BLOCK_TEMP_ADC			626
#define HIGH_RECOVER_TEMP_ADC			614
#define EVT_HIGH_BLOCK_TEMP_ADC			707
#define EVT_HIGH_RECOVER_TEMP_ADC		701
#define LOW_BLOCK_TEMP_ADC			513
#define LOW_RECOVER_TEMP_ADC			522

#define NB_HIGH_BLOCK_TEMP_ADC			467
#define NB_HIGH_RECOVER_TEMP_ADC		572
#define NB_EVT_HIGH_BLOCK_TEMP_ADC		245
#define NB_EVT_HIGH_RECOVER_TEMP_ADC		527
#define NB_LOW_BLOCK_TEMP_ADC			1625
#define NB_LOW_RECOVER_TEMP_ADC			1541
#endif /* CONFIG_PMIC8058_XOADC_CAL */
#endif

#if defined(CONFIG_JPN_MODEL_SC_03D)
#define HIGH_BLOCK_TEMP_ADC_PMICTHERM		656
#define HIGH_RECOVER_TEMP_ADC_PMICTHERM		603
#define LOW_BLOCK_TEMP_ADC_PMICTHERM		504
#define LOW_RECOVER_TEMP_ADC_PMICTHERM		514
#define HIGH_BLOCK_TEMP_ADC_SETTHERM		330
#define HIGH_RECOVER_TEMP_ADC_SETTHERM		248
#define LOW_BLOCK_TEMP_ADC_SETTHERM		-14
#define LOW_RECOVER_TEMP_ADC_SETTHERM		7
#define JPN_CHARGE_CURRENT_DOWN_TEMP		240
#define JPN_CHARGE_CURRENT_RECOVERY_TEMP	235
#else
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_JPN_MODEL_SC_05D)
#define HIGH_BLOCK_TEMP_ADC_PMICTHERM		727
#define HIGH_RECOVER_TEMP_ADC_PMICTHERM		667
#define LOW_BLOCK_TEMP_ADC_PMICTHERM		504
#define LOW_RECOVER_TEMP_ADC_PMICTHERM		514
#else
#define HIGH_BLOCK_TEMP_ADC_PMICTHERM		656
#define HIGH_RECOVER_TEMP_ADC_PMICTHERM		603
#define LOW_BLOCK_TEMP_ADC_PMICTHERM		504
#define LOW_RECOVER_TEMP_ADC_PMICTHERM		514
#endif
#if defined(CONFIG_KOR_MODEL_SHV_E120L)
#define HIGH_BLOCK_TEMP_ADC_SETTHERM		388
#define HIGH_RECOVER_TEMP_ADC_SETTHERM		362
#define LOW_BLOCK_TEMP_ADC_SETTHERM		221
#define LOW_RECOVER_TEMP_ADC_SETTHERM		238
#elif defined(CONFIG_KOR_MODEL_SHV_E120S)
#define HIGH_BLOCK_TEMP_ADC_SETTHERM		391
#define HIGH_RECOVER_TEMP_ADC_SETTHERM		363
#define LOW_BLOCK_TEMP_ADC_SETTHERM		226
#define LOW_RECOVER_TEMP_ADC_SETTHERM		240
#elif defined(CONFIG_KOR_MODEL_SHV_E120K)
#define HIGH_BLOCK_TEMP_ADC_SETTHERM		390
#define HIGH_RECOVER_TEMP_ADC_SETTHERM		365
#define LOW_BLOCK_TEMP_ADC_SETTHERM		226
#define LOW_RECOVER_TEMP_ADC_SETTHERM		241
#elif defined(CONFIG_KOR_MODEL_SHV_E160S)
#define HIGH_BLOCK_TEMP_ADC_SETTHERM		390
#define HIGH_RECOVER_TEMP_ADC_SETTHERM		369
#define LOW_BLOCK_TEMP_ADC_SETTHERM		232
#define LOW_RECOVER_TEMP_ADC_SETTHERM		248
#elif defined(CONFIG_KOR_MODEL_SHV_E160L)
#define HIGH_BLOCK_TEMP_ADC_SETTHERM		387
#define HIGH_RECOVER_TEMP_ADC_SETTHERM		363
#define LOW_BLOCK_TEMP_ADC_SETTHERM		226
#define LOW_RECOVER_TEMP_ADC_SETTHERM		244
#elif defined(CONFIG_KOR_MODEL_SHV_E160K)
#define HIGH_BLOCK_TEMP_ADC_SETTHERM		387
#define HIGH_RECOVER_TEMP_ADC_SETTHERM		365
#define LOW_BLOCK_TEMP_ADC_SETTHERM		227
#define LOW_RECOVER_TEMP_ADC_SETTHERM		242
#elif defined (CONFIG_JPN_MODEL_SC_05D)
#define HIGH_BLOCK_TEMP_ADC_SETTHERM		378
#define HIGH_RECOVER_TEMP_ADC_SETTHERM		350
#define LOW_BLOCK_TEMP_ADC_SETTHERM		205
#define LOW_RECOVER_TEMP_ADC_SETTHERM		223

#else
#define HIGH_BLOCK_TEMP_ADC_SETTHERM		389
#define HIGH_RECOVER_TEMP_ADC_SETTHERM		359
#define LOW_BLOCK_TEMP_ADC_SETTHERM		220
#define LOW_RECOVER_TEMP_ADC_SETTHERM		233
#endif
#endif

#define FG_T_SOC		0
#define FG_T_VCELL		1
#define FG_T_PSOC		2
#define FG_T_RCOMP		3
#define FG_T_FSOC		4
/* all count duration = (count - 1) * poll interval */
#define RE_CHG_COND_COUNT		4
#define RE_CHG_MIN_COUNT		2
#define TEMP_BLOCK_COUNT		2
#define BAT_DET_COUNT			2
#define FULL_CHG_COND_COUNT		2
#define USB_FULL_COND_COUNT		3
#define USB_FULL_COND_VOLTAGE		4150000
#define FULL_CHARGE_COND_VOLTAGE	4000000
#define INIT_CHECK_COUNT		4

/* Offset Bit Value */
#define OFFSET_VIBRATOR_ON		(0x1 << 0)
#define OFFSET_CAMERA_ON		(0x1 << 1)
#define OFFSET_MP3_PLAY			(0x1 << 2)
#define OFFSET_VIDEO_PLAY		(0x1 << 3)
#define OFFSET_VOICE_CALL_2G		(0x1 << 4)
#define OFFSET_VOICE_CALL_3G		(0x1 << 5)
#define OFFSET_DATA_CALL		(0x1 << 6)
#define OFFSET_LCD_ON			(0x1 << 7)
#define OFFSET_TA_ATTACHED		(0x1 << 8)
#define OFFSET_CAM_FLASH		(0x1 << 9)
#define OFFSET_BOOTING			(0x1 << 10)
#define OFFSET_WIFI			(0x1 << 11)
#define OFFSET_GPS			(0x1 << 12)

#define COMPENSATE_VIBRATOR		0
#define COMPENSATE_CAMERA		0
#define COMPENSATE_MP3			0
#define COMPENSATE_VIDEO		0
#define COMPENSATE_VOICE_CALL_2G	0
#define COMPENSATE_VOICE_CALL_3G	0
#define COMPENSATE_DATA_CALL		0
#define COMPENSATE_LCD			0
#define COMPENSATE_TA			0
#define COMPENSATE_CAM_FALSH		0
#define COMPENSATE_BOOTING		0
#define COMPENSATE_WIFI			0
#define COMPENSATE_GPS			0

#define TOTAL_EVENT_TIME  (30*60*1000)  /* 30 minites */

#define EVT_CASE	(OFFSET_MP3_PLAY | OFFSET_VOICE_CALL_2G | \
	OFFSET_VOICE_CALL_3G | OFFSET_DATA_CALL | OFFSET_VIDEO_PLAY |\
	OFFSET_CAMERA_ON | OFFSET_WIFI | OFFSET_GPS)

#if defined(CONFIG_TARGET_LOCALE_USA)
static int event_occur;
static unsigned int event_start_time_msec;
static unsigned int event_total_time_msec;
#endif

#define CELOX_BATTERY_CHARGING_CONTROL

#if defined(CELOX_BATTERY_CHARGING_CONTROL)
/* Tells if charging forcefully disabled. */
static int is_charging_disabled;
#endif

enum cable_type_t {
	CABLE_TYPE_NONE = 0,
	CABLE_TYPE_USB,
	CABLE_TYPE_AC,
	CABLE_TYPE_MISC,
	CABLE_TYPE_CARDOCK,
	CABLE_TYPE_UARTOFF,
	CABLE_TYPE_UNKNOWN,
};

enum batt_full_t {
	BATT_NOT_FULL = 0,
	BATT_FULL,
};

enum {
	BAT_NOT_DETECTED,
	BAT_DETECTED
};

static ssize_t sec_bat_show_property(struct device *dev,
				     struct device_attribute *attr, char *buf);

static ssize_t sec_bat_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count);

struct battest_info {
	int rechg_count;
	int full_count;
	int test_value;
	int test_esuspend;
	bool is_rechg_state;
};

/*
struct adc_sample {
	int average_adc;
	int adc_arr[ADC_TOTAL_COUNT];
	int index;
};
*/

struct sec_temperature_spec {
	int high_block;
	int high_recovery;
	int low_block;
	int low_recovery;
};

#if defined(ADC_QUEUE_FEATURE) || defined(PRE_CHANOPEN_FEATURE)
struct sec_batt_adc_chan {
	int id;
#ifdef PRE_CHANOPEN_FEATURE
	void *adc_handle;
#endif
#ifdef ADC_QUEUE_FEATURE
	int adc_data_depot;
	int adc_physical_depot;
#endif
};
#endif

struct sec_bat_info {
	struct device *dev;

	char *fuel_gauge_name;
	char *charger_name;

	/*
	unsigned int adc_channel;
	struct adc_sample temper_adc_sample;
	*/

	struct power_supply psy_bat;
	struct power_supply psy_usb;
	struct power_supply psy_ac;

	struct wake_lock vbus_wake_lock;
	struct wake_lock monitor_wake_lock;
	struct wake_lock cable_wake_lock;
	struct wake_lock test_wake_lock;
	struct wake_lock measure_wake_lock;
#ifdef ADC_QUEUE_FEATURE
	struct wake_lock adc_wake_lock;
#endif

	enum cable_type_t cable_type;
	enum batt_full_t batt_full_status;

	unsigned int batt_temp;	/* Battery Temperature (C) */
	unsigned int batt_temp_sub; /* Battery Temperature (C) */
	int batt_temp_high_cnt;
	int batt_temp_low_cnt;
	unsigned int batt_health;
	unsigned int batt_vcell;
	unsigned int batt_soc;
	unsigned int batt_raw_soc;
	unsigned int batt_full_soc;
	unsigned int batt_presoc;
	unsigned int polling_interval;
	unsigned int measure_interval;
	int charging_status;
	/* int charging_full_count; */
	int otg_state;

	int adc_channel_main;
	int adc_channel_sub;
	unsigned int batt_temp_adc;
	unsigned int batt_temp_radc;
	unsigned int batt_temp_adc_sub;
	unsigned int batt_temp_radc_sub;
	unsigned int batt_current_adc;
	unsigned int present;
	struct battest_info	test_info;
	struct workqueue_struct *monitor_wqueue;
	struct work_struct monitor_work;
#ifdef ADC_QUEUE_FEATURE
	struct workqueue_struct *adc_wqueue;
	struct work_struct adc_work;
	unsigned int wadc_alive;
	unsigned int wadc_alive_prev;
	unsigned int wadc_freezed_count;
	bool is_adc_wq_freezed;
#endif
	struct delayed_work	cable_work;
	struct delayed_work polling_work;
	struct delayed_work measure_work;
	struct delayed_work otg_work;
	struct early_suspend bat_early_suspend;
	struct sec_temperature_spec temper_spec;

	unsigned long charging_start_time;
	unsigned long charging_passed_time;
	unsigned int recharging_status;
	unsigned int batt_lpm_state;
#if defined(GET_TOPOFF_WITH_REGISTER)
	unsigned int is_top_off;
#endif
	unsigned int hw_rev;
	unsigned int voice_call_state;
	unsigned int full_cond_voltage;
	unsigned int full_cond_count;

	void (*chg_shutdown_cb) (void);
	unsigned int (*get_lpcharging_state) (void);
	bool charging_enabled;
	bool is_timeout_chgstop;
	bool is_esus_state;
	bool has_sub_therm;
	bool lpm_chg_mode;
	bool is_rechg_triggered;
	bool dcin_intr_triggered;
	bool is_adc_ok;
	u16 batt_rcomp;

	int initial_check_count;
	struct proc_dir_entry *entry;
#if defined(CONFIG_TARGET_LOCALE_USA)
	bool ui_full_charge_status;
	unsigned int device_state;
	bool cable_uart_off;
#endif
#if defined(ADC_QUEUE_FEATURE) || defined(PRE_CHANOPEN_FEATURE)
	/* 2 temperature, current, battid */
	struct sec_batt_adc_chan batt_adc_chan[MAX_BATT_ADC_CHAN];
#endif
#ifdef MPP_CHECK_FEATURE
	int mpp_get_cblpwr;
#endif
#if defined(CONFIG_JPN_MODEL_SC_03D)
	int jpn_chg_cur_ctrl_status;
	int jpn_chg_cur_ctrl_temp_high_cnt;
#endif
};

static char *supply_list[] = {
	"battery",
};

static enum power_supply_property sec_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static enum power_supply_property sec_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int sec_bat_read_adc(struct sec_bat_info *info, int channel,
		int *adc_data, int *adc_physical);

#ifdef ADC_QUEUE_FEATURE
static int sec_bat_get_adc_depot(struct sec_bat_info *info, int channel,
		int *adc_data, int *adc_physical)
{
	int idx = 0;

	for (idx = 0; idx < MAX_BATT_ADC_CHAN; idx++) {
		if (channel == info->batt_adc_chan[idx].id) {
			/*
			printk("%s : channel = %d, idx = %d\n",
				__func__, channel, idx);
			*/
			break;
		}
	}

	if (idx >= MAX_BATT_ADC_CHAN) {
		pr_err("%s: invalid channel data\n", __func__);
		return -EINVAL;
	}

	*adc_data = info->batt_adc_chan[idx].adc_data_depot;
	*adc_physical = info->batt_adc_chan[idx].adc_physical_depot;
	/*
	pr_info("%s : [idx = %d] data = %d, physical = %d\n",
		__func__, idx,
		info->batt_adc_chan[idx].adc_data_depot,
		info->batt_adc_chan[idx].adc_physical_depot);
	*/

	return 0;
}

static int sec_bat_set_adc_depot(struct sec_bat_info *info, int channel,
		int *adc_data, int *adc_physical)
{
	int idx = 0;

	for (idx = 0; idx < MAX_BATT_ADC_CHAN; idx++) {
		if (channel == info->batt_adc_chan[idx].id) {
			/*
			printk("%s : channel = %d, idx = %d\n",
				__func__, channel, idx);
			*/
			break;
		}
	}

	if (idx >= MAX_BATT_ADC_CHAN) {
		pr_err("%s: invalid channel data\n", __func__);
		return -EINVAL;
	}

	info->batt_adc_chan[idx].adc_data_depot = *adc_data;
	info->batt_adc_chan[idx].adc_physical_depot = *adc_physical;
	/*
	printk("%s : [idx = %d] data = %d, physical = %d\n", __func__, idx,
		info->batt_adc_chan[idx].adc_data_depot,
		info->batt_adc_chan[idx].adc_physical_depot);
	*/

	return 0;
}
#endif

static int sec_bat_check_vf(struct sec_bat_info *info)
{
	int health = info->batt_health;

	if (info->present == 0) {
		if (info->test_info.test_value == 999) {
			pr_info("%s : test case : %d\n", __func__,
					info->test_info.test_value);
			health = POWER_SUPPLY_HEALTH_DEAD;
		} else
			health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
	} else {
		health = POWER_SUPPLY_HEALTH_GOOD;
	}

	/* update health */
	if (health != info->batt_health) {
		if (health == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE ||
			health == POWER_SUPPLY_HEALTH_DEAD){
			info->batt_health = health;
			pr_info("%s : vf error update\n", __func__);
		} else if (info->batt_health != POWER_SUPPLY_HEALTH_OVERHEAT &&
			info->batt_health != POWER_SUPPLY_HEALTH_COLD &&
			health == POWER_SUPPLY_HEALTH_GOOD) {
			info->batt_health = health;
			pr_info("%s : recovery form vf error\n", __func__);
		}
	}

	return 0;
}

static int sec_bat_check_detbat(struct sec_bat_info *info)
{
	struct power_supply *psy =
		power_supply_get_by_name(info->charger_name);
	union power_supply_propval value;
	static int cnt;
	int vf_state = BAT_DETECTED;
#if defined(CONFIG_EUR_MODEL_GT_I9210)
/*	if (get_hw_rev() >= 0x07) { */ /* EUR rev0.3 and after [remove condition to fix build error]*/ 
		int adc_data = 0, adc_physical = 0;
/*	} */
        int ret = 0; /* added to fix build error*/
#elif defined(CONFIG_TARGET_LOCALE_USA)
	int adc_data = 0, adc_physical = 0;
#ifndef ADC_QUEUE_FEATURE
	int ret = 0;
#endif

#else
	int ret = 0;
#endif
	if (!psy) {
		dev_err(info->dev, "%s: fail to get charger ps\n", __func__);
		return -ENODEV;
	}

#if defined(CONFIG_EUR_MODEL_GT_I9210)
	if (get_hw_rev() >= 0x07) { /* EUR rev0.3 and after */

#ifdef ADC_QUEUE_FEATURE
		if (sec_bat_get_adc_depot(info, CHANNEL_ADC_BATT_ID,
					&adc_data, &adc_physical) < 0) {
			pr_err("%s: get adc depot failed (chan - %d), return\n",
					__func__, CHANNEL_ADC_BATT_ID);
			return 0;
	}
#else /* ADC_QUEUE_FEATURE */
		ret = sec_bat_read_adc(info, CHANNEL_ADC_BATT_ID,
						&adc_data, &adc_physical);
#endif /* ADC_QUEUE_FEATURE */

		pr_info("%s : vf adc : %d\n", __func__, adc_physical);

		if (adc_physical > 500 && adc_physical < 900)
			value.intval = BAT_DETECTED;
		else
			value.intval = BAT_NOT_DETECTED;
	} else { /* EUR rev0.2 and before */
		ret = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &value);
		if (ret < 0) {
			dev_err(info->dev, "%s: fail to get status(%d)\n",
						__func__, ret);
			return -ENODEV;
		}
	}
#elif defined(CONFIG_TARGET_LOCALE_USA)

#ifdef ADC_QUEUE_FEATURE
	if (sec_bat_get_adc_depot(info, CHANNEL_ADC_BATT_ID,
		&adc_data, &adc_physical) < 0) {
		pr_info("%s: get adc depot failed (chan - %d), return\n",
			__func__, CHANNEL_ADC_BATT_ID);
		return 0;
	}
#else /* ADC_QUEUE_FEATURE */
	ret = sec_bat_read_adc(info, CHANNEL_ADC_BATT_ID,
		&adc_data, &adc_physical);
#endif /* ADC_QUEUE_FEATURE */
	/*
	printk("%s: channel : %d, raw adc is %d, result is %d (temper.)\n",
		__func__, CHANNEL_ADC_BATT_ID, adc_data, temp_adc);
	*/

#if defined(CONFIG_USA_MODEL_SGH_I757) || \
	defined(CONFIG_CAN_MODEL_SGH_I757M)
	if (adc_physical > 100 && adc_physical < 400)
#elif defined(CONFIG_USA_MODEL_SGH_T989) || \
	defined(CONFIG_USA_MODEL_SGH_I727) || \
	defined(CONFIG_USA_MODEL_SGH_T769) || \
	defined(CONFIG_USA_MODEL_SGH_I577) || \
	defined(CONFIG_CAN_MODEL_SGH_I577R)
	if (adc_physical > 500 && adc_physical < 900)
#elif defined(CONFIG_USA_MODEL_SGH_I717)
	if ((get_hw_rev() == 0x01) &&
		(adc_physical > 1290 && adc_physical < 1800))
		value.intval = BAT_DETECTED;
	else if ((get_hw_rev() != 0x01) &&
		(adc_physical > 300 && adc_physical < 660))
#endif
		value.intval = BAT_DETECTED;
	else
		value.intval = BAT_NOT_DETECTED;

	/*
	if (adc_physical >100 && adc_physical < 800)
		value.intval = BAT_DETECTED;
	else
		value.intval = BAT_NOT_DETECTED;
	*/
#else
	ret = psy->get_property(psy, POWER_SUPPLY_PROP_PRESENT, &value);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to get status(%d)\n",
			__func__, ret);
		return -ENODEV;
	}
#endif

	if ((info->cable_type != CABLE_TYPE_NONE) &&
		(value.intval == BAT_NOT_DETECTED)) {
		if (cnt <= BAT_DET_COUNT)
			cnt++;
		if (cnt >= BAT_DET_COUNT)
			vf_state = BAT_NOT_DETECTED;
		else
			vf_state = BAT_DETECTED;
	} else {
		vf_state = BAT_DETECTED;
		cnt = 0;
	}

	if (info->present == 1 &&
		vf_state == BAT_NOT_DETECTED) {
		pr_info("%s : detbat state(->%d) changed\n",
			__func__, vf_state);
		info->present = 0;
		cancel_work_sync(&info->monitor_work);
		wake_lock(&info->monitor_wake_lock);
		queue_work(info->monitor_wqueue, &info->monitor_work);
	} else if (info->present == 0 &&
		vf_state == BAT_DETECTED) {
		pr_info("%s : detbat state(->%d) changed\n",
			__func__, vf_state);
		info->present = 1;
		cancel_work_sync(&info->monitor_work);
		wake_lock(&info->monitor_wake_lock);
		queue_work(info->monitor_wqueue, &info->monitor_work);
	}

	return value.intval;
}

static int sec_bat_get_fuelgauge_data(struct sec_bat_info *info, int type)
{
	struct power_supply *psy
	    = power_supply_get_by_name(info->fuel_gauge_name);
	union power_supply_propval value;

	if (!psy) {
		dev_err(info->dev, "%s: fail to get fuel gauge ps\n", __func__);
		return -ENODEV;
	}

	switch (type) {
	case FG_T_VCELL:
		value.intval = 0; /*vcell */
		psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
		break;
	case FG_T_SOC:
		value.intval = 0; /*normal soc */
		psy->get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &value);
#if defined(CONFIG_TARGET_LOCALE_USA)
		/* According to the SAMSUMG inner charger charging concept,
		   Full charging process should be divided into 2 phase
		   as 1st UI full charging, 2nd real full charging.
		   This is for the 1st UI Full charging.
		*/
		if (info->ui_full_charge_status &&
		    info->charging_status == POWER_SUPPLY_STATUS_CHARGING)
			value.intval = 100;
#endif
		break;
	case FG_T_PSOC:
		value.intval = 1; /*raw soc */
		psy->get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &value);
		break;
	case FG_T_RCOMP:
		value.intval = 2; /*rcomp */
		psy->get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &value);
		break;
	case FG_T_FSOC:
		value.intval = 3; /*full soc */
		psy->get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &value);
		break;
	default:
		return -ENODEV;
	}

	return value.intval;
}

static int sec_bat_get_property(struct power_supply *ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sec_bat_info *info = container_of(ps, struct sec_bat_info,
						 psy_bat);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (info->test_info.test_value == 999) {
			pr_info("%s : test case : %d\n",
				__func__, info->test_info.test_value);
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;
		} else if (info->is_timeout_chgstop &&
			info->charging_status == POWER_SUPPLY_STATUS_FULL &&
			info->batt_soc != 100) {
			/* new concept : in case of time-out charging stop,
			   Do not update FULL for UI except soc 100%,
			   Use same time-out value for first charing and
			   re-charging
			*/
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		} else {
#if defined(CONFIG_TARGET_LOCALE_USA)
			if (info->ui_full_charge_status &&
			   info->charging_status ==
				POWER_SUPPLY_STATUS_CHARGING) {
				val->intval = POWER_SUPPLY_STATUS_FULL;
				break;
			}
#endif
			val->intval = info->charging_status;
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = info->batt_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = info->present;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = info->batt_temp;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* battery is always online */
		/* val->intval = 1; */
		if (info->charging_status == POWER_SUPPLY_STATUS_DISCHARGING &&
			info->cable_type != CABLE_TYPE_NONE) {
			val->intval = CABLE_TYPE_NONE;
		} else
			val->intval = info->cable_type;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = info->batt_vcell;
		if (val->intval == -1)
			return -EINVAL;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (!info->is_timeout_chgstop &&
			info->charging_status == POWER_SUPPLY_STATUS_FULL) {
			val->intval = 100;
			break;
		}

#if defined(CONFIG_TARGET_LOCALE_USA)
		if (info->ui_full_charge_status &&
		    info->charging_status == POWER_SUPPLY_STATUS_CHARGING) {
			val->intval = 100;
			break;
		}
#endif
		val->intval = info->batt_soc;
		if (val->intval == -1)
			return -EINVAL;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sec_bat_handle_charger_topoff(struct sec_bat_info *info)
{
	struct power_supply *psy =
	    power_supply_get_by_name(info->charger_name);
	struct power_supply *psy_fg =
		power_supply_get_by_name(info->fuel_gauge_name);
	union power_supply_propval value;
	int ret = 0;

	if (!psy || !psy_fg) {
		pr_err("%s: fail to get charger ps\n", __func__);
		return -ENODEV;
	}

	if (info->batt_full_status == BATT_NOT_FULL) {
		info->charging_status = POWER_SUPPLY_STATUS_FULL;
		info->batt_full_status = BATT_FULL;
		info->recharging_status = false;
		info->charging_passed_time = 0;
		info->charging_start_time = 0;
#if defined(CONFIG_TARGET_LOCALE_USA)
		info->ui_full_charge_status = false;
#endif
		/* disable charging */
		value.intval = POWER_SUPPLY_STATUS_DISCHARGING;
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_STATUS,
			&value);
		info->charging_enabled = false;
		/* notify full state to fuel guage */
		value.intval = POWER_SUPPLY_STATUS_FULL;
		ret = psy_fg->set_property(psy_fg, POWER_SUPPLY_PROP_STATUS,
			&value);
	}
	return ret;
}

static int sec_bat_is_charging(struct sec_bat_info *info)
{
	struct power_supply *psy = power_supply_get_by_name(info->charger_name);
	union power_supply_propval value;
	int ret;

	if (!psy) {
		dev_err(info->dev, "%s: fail to get charger ps\n", __func__);
		return -ENODEV;
	}

	ret = psy->get_property(psy, POWER_SUPPLY_PROP_STATUS, &value);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to get status(%d)\n", __func__,
			ret);
		return ret;
	}

	return value.intval;
}

static int sec_bat_adjust_charging_current(struct sec_bat_info *info,
		int chg_current)
{
	struct power_supply *psy = power_supply_get_by_name(info->charger_name);
	union power_supply_propval val_chg_current;
	int ret;

	if (!psy) {
		dev_err(info->dev, "%s: fail to get charger ps\n", __func__);
		return -ENODEV;
	}

	val_chg_current.intval = chg_current;

	ret = psy->set_property(psy,
		POWER_SUPPLY_PROP_CURRENT_ADJ, &val_chg_current);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to adjust charging current (%d)\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

static int sec_bat_get_charging_current(struct sec_bat_info *info)
{
	struct power_supply *psy = power_supply_get_by_name(info->charger_name);
	union power_supply_propval val_chg_current;
	int ret;

	if (!psy) {
		dev_err(info->dev, "%s: fail to get charger ps\n", __func__);
		return -ENODEV;
	}

	ret = psy->get_property(psy,
		POWER_SUPPLY_PROP_CURRENT_ADJ, &val_chg_current);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to charging current (%d)\n",
			__func__, ret);
		return ret;
	}

	/*
	pr_info("%s : retun value = %d\n", __func__, value.intval);
	*/
	return val_chg_current.intval;
}

static int sec_bat_is_invalid_bmd(struct sec_bat_info *info)
{
	struct power_supply *psy = power_supply_get_by_name(info->charger_name);
	union power_supply_propval value;
	int ret;

	if (!psy) {
		dev_err(info->dev, "%s: fail to get charger ps\n", __func__);
		return -ENODEV;
	}

	ret = psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	if (ret < 0) {
		dev_err(info->dev, "%s: fail to get online status(%d)\n",
			__func__, ret);
		return ret;
	}

	/*
	printk("%s : retun value = %d\n", __func__, value.intval);
	*/
	return value.intval;
}

static void sec_otg_work(struct work_struct *work)
{
	struct sec_bat_info *info =
		container_of(work, struct sec_bat_info, otg_work.work);
	struct power_supply *psy =
		power_supply_get_by_name(info->charger_name);
	union power_supply_propval val_type ;

	if (!psy) {
		dev_err(info->dev, "%s: fail to get charger ps\n", __func__);
		return;
	}

	val_type.intval = info->otg_state;
	psy->set_property(psy, POWER_SUPPLY_PROP_OTG, &val_type);
}

static int sec_bat_set_property(struct power_supply *ps,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct sec_bat_info *info = container_of(ps, struct sec_bat_info,
						 psy_bat);
	/*
	struct power_supply *psy = power_supply_get_by_name(info->charger_name);
	union power_supply_propval value;
	*/
	int chg_status = 0;
	unsigned long wdelay = msecs_to_jiffies(500);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		chg_status = sec_bat_is_charging(info);
		pr_info("%s: %d\n", __func__, chg_status);
		if (val->intval == POWER_SUPPLY_STATUS_CHARGING) {
			pr_info("%s: charger inserted!!\n", __func__);
			info->dcin_intr_triggered = true;
			cancel_delayed_work(&info->measure_work);
			wake_lock(&info->measure_wake_lock);
			queue_delayed_work(info->monitor_wqueue,
				&info->measure_work, HZ);
		} else if (val->intval == POWER_SUPPLY_STATUS_DISCHARGING) {
			pr_info("%s: charger removed!!\n", __func__);
			info->dcin_intr_triggered = false;
			cancel_delayed_work(&info->measure_work);
			wake_lock(&info->measure_wake_lock);
			queue_delayed_work(info->monitor_wqueue,
				&info->measure_work, HZ);
		} else {
			pr_err("%s: unknown chg intr state!!\n", __func__);
			return -EINVAL;
		}
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		/* TODO: lowbatt interrupt: called by fuel gauge */
		dev_info(info->dev, "%s: lowbatt intr\n", __func__);
		if (val->intval != POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL)
			return -EINVAL;
		wake_lock(&info->monitor_wake_lock);
		queue_work(info->monitor_wqueue, &info->monitor_work);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		/* cable is attached or detached. called by usb switch ic */
		dev_info(info->dev, "%s: cable was changed(%d)\n", __func__,
			 val->intval);
		switch (val->intval) {
		case POWER_SUPPLY_TYPE_BATTERY:
			info->cable_type = CABLE_TYPE_NONE;
#if defined(CONFIG_TARGET_LOCALE_USA)
			info->cable_uart_off = false;
#endif
			break;
		case POWER_SUPPLY_TYPE_MAINS:
			info->cable_type = CABLE_TYPE_AC;
			break;
		case POWER_SUPPLY_TYPE_USB:
			info->cable_type = CABLE_TYPE_USB;
			break;
		case POWER_SUPPLY_TYPE_MISC:
			info->cable_type = CABLE_TYPE_MISC;
			break;
		case POWER_SUPPLY_TYPE_CARDOCK:
			info->cable_type = CABLE_TYPE_CARDOCK;
			break;
		case POWER_SUPPLY_TYPE_UARTOFF:
			info->cable_type = CABLE_TYPE_UARTOFF;
#if defined(CONFIG_TARGET_LOCALE_USA)
			info->cable_uart_off = true;
#endif
			break;
		default:
			return -EINVAL;
		}
		wake_lock(&info->cable_wake_lock);
		queue_delayed_work(info->monitor_wqueue,
			&info->cable_work, wdelay);
		break;
	case POWER_SUPPLY_PROP_OTG:
		info->otg_state = val->intval;
		queue_delayed_work(info->monitor_wqueue,
			&info->otg_work, 0);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_usb_get_property(struct power_supply *ps,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct sec_bat_info *info = container_of(ps, struct sec_bat_info,
						 psy_usb);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the USB charger is connected */
	val->intval = (info->cable_type == CABLE_TYPE_USB);

	return 0;
}

static int sec_ac_get_property(struct power_supply *ps,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct sec_bat_info *info = container_of(ps, struct sec_bat_info,
						 psy_ac);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the AC charger is connected */
	if (info->charging_status == POWER_SUPPLY_STATUS_DISCHARGING &&
			info->cable_type != CABLE_TYPE_NONE) {
			val->intval = 0;
	} else {
		if (info->cable_type == CABLE_TYPE_MISC ||
			info->cable_type == CABLE_TYPE_UARTOFF ||
			info->cable_type == CABLE_TYPE_CARDOCK) {
			if (!info->dcin_intr_triggered)
				val->intval = 0;
			else
				val->intval = 1;
		} else {
			/* org */
			val->intval = (info->cable_type == CABLE_TYPE_AC) ||
				(info->cable_type == CABLE_TYPE_UNKNOWN);
		}
	}

	/*
	printk("%s : cable type = %d, return val = %d\n",
		__func__, info->cable_type, val->intval);
	*/

	return 0;
}

static int sec_bat_read_adc(struct sec_bat_info *info, int channel,
		int *adc_data, int *adc_physical)
{
	int ret = 0;
	struct adc_chan_result adc_chan_result;
#ifdef USE_COMP_METHOD_OTHER
	DECLARE_COMPLETION_ONSTACK(wait);
#else
	struct completion  wait;
#endif
#if defined(PRE_CHANOPEN_FEATURE) || defined(ADC_QUEUE_FEATURE)
	int idx = 0;
#endif
#ifndef PRE_CHANOPEN_FEATURE
	void *h;
#endif
#ifdef CONFIG_SEC_DEBUG_PM8058_ADC
	static int retry_cnt;
#endif

	pr_debug("%s: called for %d\n", __func__, channel);

#if defined(PRE_CHANOPEN_FEATURE) || defined(ADC_QUEUE_FEATURE)
	/* channel handle */
	for (idx = 0; idx < MAX_BATT_ADC_CHAN; idx++) {
		if (channel == info->batt_adc_chan[idx].id) {
			/*
			printk("%s : channel = %d, idx = %d\n",
				__func__, channel, idx);
			*/
			break;
		}
	}

	if (idx >= MAX_BATT_ADC_CHAN) {
		pr_err("%s: invalid channel data\n", __func__);
		return -ENODEV;
	}
#endif

#ifndef PRE_CHANOPEN_FEATURE
	ret = adc_channel_open(channel, &h);
	if (ret) {
		pr_err("%s: couldnt open channel %d ret=%d\n",
			__func__, channel, ret);
		goto out;
	}
#endif

#ifndef USE_COMP_METHOD_OTHER
	init_completion(&wait);
#endif

#ifdef PRE_CHANOPEN_FEATURE
	ret = adc_channel_request_conv(info->batt_adc_chan[idx].adc_handle,
		&wait);
#else
	ret = adc_channel_request_conv(h, &wait);
#endif
	if (ret) {
		pr_err("%s: couldnt request conv channel %d ret=%d\n",
			__func__, channel, ret);
		goto out;
	}

	ret = wait_for_completion_timeout(&wait, 5*HZ);
	if (!ret) {
		pr_err("%s: wait interrupted channel %d ret=%d\n",
			__func__, channel, ret);
#ifdef CONFIG_SEC_DEBUG_PM8058_ADC_VERBOSE
		adc_dbg_info_timer(0);
#else
		pm8058_xoadc_clear_recentQ(h);
		adc_channel_close(h);
#ifdef CONFIG_SEC_DEBUG_PM8058_ADC
		if (retry_cnt++ >= 10)
			adc_dbg_info_timer(0);
#endif
#endif
		goto out;
	}
#ifdef CONFIG_SEC_DEBUG_PM8058_ADC
	retry_cnt = 0;
#endif

#ifdef PRE_CHANOPEN_FEATURE
	ret = adc_channel_read_result(info->batt_adc_chan[idx].adc_handle,
						&adc_chan_result);
#else
	ret = adc_channel_read_result(h, &adc_chan_result);
#endif
	if (ret) {
		pr_err("%s: couldnt read result channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}

#ifndef PRE_CHANOPEN_FEATURE
	ret = adc_channel_close(h);
	if (ret) {
		pr_err("%s: couldnt close channel %d ret=%d\n",
						__func__, channel, ret);
		goto out;
	}
#endif

	if (adc_data)
		*adc_data = adc_chan_result.measurement;

	pr_debug("%s: done for %d\n", __func__, channel);
	*adc_physical = adc_chan_result.physical;

	info->is_adc_ok = true;
#ifdef ADC_QUEUE_FEATURE
	info->batt_adc_chan[idx].adc_data_depot = *adc_data;
	info->batt_adc_chan[idx].adc_physical_depot = *adc_physical;
	/*
	printk("%s : [idx = %d] data = %d, physical = %d\n", __func__, idx,
		info->batt_adc_chan[idx].adc_data_depot,
		info->batt_adc_chan[idx].adc_physical_depot);
	*/
#endif
	return 0;
out:
	pr_err("%s: channel(%d) - error returned, use previous data\n",
		__func__, channel);
	info->is_adc_ok = false;

	return ret;
}

static int sec_bat_rescale_adcvalue(struct sec_bat_info *info,
		int channel, int adc_data, int adc_physical)
{
	switch (channel)	{
	case CHANNEL_ADC_PMIC_THERM:
		/* adc_physical : 0.001'C */
		if (info->has_sub_therm &&
			(channel == info->adc_channel_sub)) {
			info->batt_temp_radc_sub = adc_data/1000;
			info->batt_temp_sub = adc_physical/100;
		} else {
			info->batt_temp_radc = adc_data/1000;
			info->batt_temp = adc_physical/100;
		}
		break;
	case CHANNEL_ADC_BATT_THERM:
	case CHANNEL_ADC_XOTHERM:
	case CHANNEL_ADC_XOTHERM_4K:
		/* adc_physical : 0.1'C */
		if (info->has_sub_therm &&
			(channel == info->adc_channel_sub)) {
			info->batt_temp_radc_sub = adc_data;
			info->batt_temp_sub = adc_physical;
		} else {
			info->batt_temp_radc = adc_data;
			info->batt_temp = adc_physical;
		}
		break;
	default:
		pr_info("%s : unknown channel type, return!", __func__);
		return -ENODEV;
	}

	return 0;
}

#if defined(CONFIG_TARGET_LOCALE_USA)
static int is_over_event_time(void)
{
	unsigned int total_time = 0;

	/*
	pr_info("[BAT]:%s\n", __func__);
	*/

	if (!event_start_time_msec) {
		/*
		junghyunseok edit for CTIA of behold3 20100413
		*/
		return 1;
	}

	total_time = TOTAL_EVENT_TIME;

	if (jiffies_to_msecs(jiffies) >= event_start_time_msec) {
		event_total_time_msec =
			jiffies_to_msecs(jiffies) - event_start_time_msec;
	} else {
		event_total_time_msec =
			0xFFFFFFFF - event_start_time_msec +
			jiffies_to_msecs(jiffies);
	}

	if (event_total_time_msec > total_time && event_start_time_msec) {
		pr_info("[BAT]:%s:abs time is over.:event_start_time_msec=%u,"
			" event_total_time_msec=%u\n", __func__,
			event_start_time_msec, event_total_time_msec);
		return 1;
	} else {
		return 0;
	}
}

static void sec_set_time_for_event(int mode)
{

	/* pr_info("[BAT]:%s\n", __func__); */

	if (mode) {
		/* record start time for abs timer */
		event_start_time_msec = jiffies_to_msecs(jiffies);
		/*
		pr_info("[BAT]:%s: start_time(%u)\n",
				__func__, event_start_time_msec);
		*/
	} else {
		/* initialize start time for abs timer */
		event_start_time_msec = 0;
		event_total_time_msec = 0;
		/*
		pr_info("[BAT]:%s: start_time_msec(%u)\n",
				__func__, event_start_time_msec);
		*/
	}
}

#if defined(CONFIG_USA_MODEL_SGH_T769)
const int temper_table[][2] =  {
	/* ADC, Temperature (C) */
	{ 1815,		-150	},
	{ 1758,		-100	},
	{ 1684,		-50	},
	{ 1596,		0	},
	{ 1480,		50	},
	{ 1386,		100	},
	{ 1268,		150	},
	{ 1142,		200	},
	{ 1015,		250	},
	{ 898,		300	},
	{ 783,		350	},
	{ 674,		400	},
	{ 580,		450	},
	{ 495,		500	},
	{ 422,		550	},
	{ 359,		600	},
	{ 303,		650	},
};
#elif defined(CONFIG_USA_MODEL_SGH_T989)
#if defined(CONFIG_PMIC8058_XOADC_CAL)
const int temper_table[][2] =  {
	/* ADC, Temperature (C) */
	{ 1858,		-200	},
	{ 1815,		-150	},
	{ 1754,		-100	}, /* 10 */
	{ 1725,		-80	},
	{ 1676,		-50	},
	{ 1664,		-40	},
	{ 1577,		0	},
	{ 1487,		50	},
	{ 1387,		100	},
	{ 1270,		150	},
	{ 1148,		200	},
	{ 1013,		250	},
	{ 896,		300	},
	{ 780,		350	},
	{ 712,		400	},
	{ 610,		450	},
	{ 525,		500	},
	{ 448,		550	},
	{ 382,		600	},
	{ 368,		610	},
	{ 345,		630	},
	{ 324,		650	},
};
#else
const int temper_table[][2] =  {
	/* ADC, Temperature (C) */
	{ 1810,		-200	},
	{ 1761,		-150	},
	{ 1700,		-100	}, /* 10 */
	{ 1673,		-80	},
	{ 1626,		-50	},
	{ 1606,		-40	},
	{ 1540,		0	},
	{ 1443,		50	},
	{ 1334,		100	},
	{ 1215,		150	},
	{ 1093,		200	},
	{ 970,		250	},
	{ 846,		300	},
	{ 753,		340	},
	{ 730,		350	},
	{ 622,		400	},
	{ 526,		450	},
	{ 440,		500	},
	{ 362,		550	},
	{ 300,		600	},
	{ 286,		610	},
	{ 265,		630	},
	{ 243,		650	},
};
#endif /* CONFIG_PMIC8058_XOADC_CAL */
#elif defined(CONFIG_USA_MODEL_SGH_I727) || \
	defined(CONFIG_USA_MODEL_SGH_I717) || \
	defined(CONFIG_USA_MODEL_SGH_I757) || \
	defined(CONFIG_USA_MODEL_SGH_I577) || \
	defined(CONFIG_CAN_MODEL_SGH_I577R) || \
	defined(CONFIG_CAN_MODEL_SGH_I757M)
#if defined(CONFIG_PMIC8058_XOADC_CAL)
const int temper_table[][2] =  {
	/* ADC, Temperature (C) */
	{ 1755,		-100	},
	{ 1685,		-50	},
	{ 1605,		0	},
	{ 1500,		50	},
	{ 1405,		100	},
	{ 1290,		150	},
	{ 1170,		200	},
	{ 1045,		250	},
	{ 925,		300	},
	{ 810,		350	},
	{ 705,		400	},
	{ 610,		450	},
	{ 525,		500	},
	{ 445,		550	},
	{ 380,		600	},
	{ 347,		630	},
	{ 320,		650	},
};
#else
const int temper_table[][2] =  {
	/* ADC, Temperature (C) */
	{ 1630,		-50	},
	{ 1530,		0	},
	{ 1420,		50	},
	{ 1320,		100	},
	{ 1185,		150	},
	{ 1060,		200	},
	{ 950,		250	},
	{ 835,		300	},
	{ 725,		350	},
	{ 618,		400	},
	{ 525,		450	},
	{ 445,		500	},
	{ 368,		550	},
	{ 305,		600	},
	{ 232,		650	},
};
#endif /* CONFIG_PMIC8058_XOADC_CAL */
#endif

static int sec_bat_check_temper_adc_USA(struct sec_bat_info *info)
{
#ifndef ADC_QUEUE_FEATURE
	int ret = 0;
#endif
	int adc_data = 0, adc_physical = 0;
	int rescale_adc = 0;
	int high_block_temp_USA = HIGH_BLOCK_TEMP_ADC;
	int high_recover_temp_USA = HIGH_BLOCK_TEMP_ADC;

	int health = info->batt_health;

#ifdef ADC_QUEUE_FEATURE
	if (sec_bat_get_adc_depot(info, CHANNEL_ADC_PMIC_THERM,
		&adc_data, &adc_physical) < 0) {
		pr_info("%s: get adc depot failed (chan - %d), return\n",
			__func__, CHANNEL_ADC_PMIC_THERM);
		return 0;
	}
	/*
	if (!info->is_adc_ok || info->is_adc_wq_freezed) {
		adc_data = 333;
		adc_physical = RCOMP0_TEMP*10;
	}
	*/
#else /* ADC_QUEUE_FEATURE */
	ret = sec_bat_read_adc(info, CHANNEL_ADC_PMIC_THERM,
		&adc_data, &adc_physical);
	/*
	pr_info("%s: channel : %d, raw adc is %d, result is %d (temper.)\n",
		__func__, CHANNEL_ADC_PMIC_THERM, adc_data, adc_physical);
	*/
	if (ret) {
		pr_err("%s : read error! skip update\n", __func__);
		return 0;
	}
#endif /* ADC_QUEUE_FEATURE */
	info->batt_temp_adc = adc_data;
	if (sec_bat_rescale_adcvalue(info, info->adc_channel_main,
		adc_data, adc_physical) < 0) {
		pr_info("%s: rescale failed, return\n", __func__);
		return 0;
	}

	rescale_adc = info->batt_temp_radc;
	if (info->test_info.test_value == 1) {
		pr_info("%s : test case : %d\n", __func__,
			info->test_info.test_value);
		rescale_adc = HIGH_BLOCK_TEMP_ADC + 1;
		if (info->cable_type == CABLE_TYPE_NONE)
			rescale_adc = HIGH_RECOVER_TEMP_ADC - 1;
		info->batt_temp_radc = rescale_adc;
	}

	if (info->cable_type == CABLE_TYPE_NONE ||
		info->test_info.test_value == 999) {
		info->batt_temp_high_cnt = 0;
		info->batt_temp_low_cnt = 0;
		health = POWER_SUPPLY_HEALTH_GOOD;
		goto skip_hupdate;
	}

	if (info->device_state & EVT_CASE) {
		if (!event_occur) {
			event_occur = 1;
			sec_set_time_for_event(1);
			pr_info("EVT occurred: %x\n", info->device_state);
		}
	} else {
		if (event_occur) {
			if (is_over_event_time()) {
				sec_set_time_for_event(0);
				event_occur = 0;
				pr_info("EVT OVER\n");
			}
		}
	}

	if (event_occur) {
		high_block_temp_USA = EVT_HIGH_BLOCK_TEMP_ADC;
		high_recover_temp_USA = EVT_HIGH_RECOVER_TEMP_ADC;
	} else {
		high_block_temp_USA = HIGH_BLOCK_TEMP_ADC;
		high_recover_temp_USA = HIGH_RECOVER_TEMP_ADC;
	}

	if (rescale_adc >= high_block_temp_USA) {
		if (health != POWER_SUPPLY_HEALTH_OVERHEAT)
			if (info->batt_temp_high_cnt <= TEMP_BLOCK_COUNT)
				info->batt_temp_high_cnt++;
	} else if (rescale_adc <= high_recover_temp_USA &&
		rescale_adc >= LOW_RECOVER_TEMP_ADC) {
		if (health == POWER_SUPPLY_HEALTH_OVERHEAT ||
		    health == POWER_SUPPLY_HEALTH_COLD) {
			info->batt_temp_high_cnt = 0;
			info->batt_temp_low_cnt = 0;
		}
	} else if (rescale_adc <= LOW_BLOCK_TEMP_ADC) {
		if (health != POWER_SUPPLY_HEALTH_COLD)
			if (info->batt_temp_low_cnt <= TEMP_BLOCK_COUNT)
				info->batt_temp_low_cnt++;
	}

	if (info->batt_temp_high_cnt >= TEMP_BLOCK_COUNT)
		health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (info->batt_temp_low_cnt >= TEMP_BLOCK_COUNT)
		health = POWER_SUPPLY_HEALTH_COLD;
	else
		health = POWER_SUPPLY_HEALTH_GOOD;
skip_hupdate:
	if (info->batt_health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE &&
		info->batt_health != POWER_SUPPLY_HEALTH_DEAD &&
		health != info->batt_health) {
		info->batt_health = health;
		cancel_work_sync(&info->monitor_work);
		wake_lock(&info->monitor_wake_lock);
		queue_work(info->monitor_wqueue, &info->monitor_work);
	}

	return 0;
}

static int sec_bat_check_temper_adc_USA_nb(struct sec_bat_info *info)
{
#ifndef ADC_QUEUE_FEATURE
	int ret = 0;
#endif
	int adc_data = 0, adc_physical = 0;
	int temp = 0;
	int array_size, i;
	int rescale_adc = 0;
	int high_block_temp_USA = NB_HIGH_BLOCK_TEMP_ADC;
	int high_recover_temp_USA = NB_HIGH_BLOCK_TEMP_ADC;

	int health = info->batt_health;

#ifdef ADC_QUEUE_FEATURE
	if (sec_bat_get_adc_depot(info, CHANNEL_ADC_BATT_THERM,
		&adc_data, &adc_physical) < 0) {
		pr_info("%s: get adc depot failed (chan - %d), return\n",
			__func__, CHANNEL_ADC_BATT_THERM);
		return 0;
	}
	/*
	if (!info->is_adc_ok || info->is_adc_wq_freezed) {
		adc_data = 333;
		adc_physical = RCOMP0_TEMP*10;
	}
	*/
#else /* ADC_QUEUE_FEATURE */
	ret = sec_bat_read_adc(info, CHANNEL_ADC_BATT_THERM,
						&adc_data, &adc_physical);
	/*
	printk("%s: channel : %d, raw adc is %d, result is %d (temper.)\n",
		__func__, CHANNEL_ADC_BATT_THERM, adc_data, adc_physical);
	*/
	/*
	if (ret)
		adc_data = info->batt_temp_adc;
	*/
#endif /* ADC_QUEUE_FEATURE */
	info->batt_temp_adc = adc_data;
	info->batt_temp_radc = adc_data;

	array_size = ARRAY_SIZE(temper_table);
	for (i = 0; i < (array_size - 1); i++) {
		if (i == 0) {
			if (adc_data >= temper_table[0][0]) {
				temp = temper_table[0][1];
				break;
			} else if (adc_data <= temper_table[array_size-1][0]) {
				temp = temper_table[array_size-1][1];
				break;
			}
		}

		if (temper_table[i][0] > adc_data &&
			temper_table[i+1][0] <= adc_data) {
			/* temp = temper_table[i+1][1]; */
			temp = temper_table[i+1][1] -
				(temper_table[i+1][1] - temper_table[i][1])*
				(temper_table[i+1][0] - adc_data)/
				(temper_table[i+1][0] - temper_table[i][0]);
			break;
		}
	}

	/*
	if (temp != info->batt_temp)
		printk("[temper.] %d, %d\n", info->batt_temp, adc_data);
	*/
	info->batt_temp = temp;

	rescale_adc = info->batt_temp_radc;
	if (info->test_info.test_value == 1) {
		pr_info("%s : test case : %d\n", __func__,
			info->test_info.test_value);
		rescale_adc = NB_HIGH_BLOCK_TEMP_ADC + 1;
		if (info->cable_type == CABLE_TYPE_NONE)
			rescale_adc = NB_HIGH_RECOVER_TEMP_ADC - 1;
		info->batt_temp_radc = rescale_adc;
	}

	if (info->cable_type == CABLE_TYPE_NONE ||
		info->test_info.test_value == 999) {
		info->batt_temp_high_cnt = 0;
		info->batt_temp_low_cnt = 0;
		health = POWER_SUPPLY_HEALTH_GOOD;
		goto skip_hupdate;
	}

#if !defined(CONFIG_USA_MODEL_SGH_T989D)
	if (info->device_state & EVT_CASE) {
		if (!event_occur) {
			event_occur = 1;
			sec_set_time_for_event(1);
			pr_info("EVT occurred: %x\n", info->device_state);
		}
	} else {
		if (event_occur) {
			if (is_over_event_time()) {
				sec_set_time_for_event(0);
				event_occur = 0;
				pr_info("EVT OVER\n");
			}
		}
	}
#endif

	if (event_occur) {
		high_block_temp_USA = NB_EVT_HIGH_BLOCK_TEMP_ADC;
		high_recover_temp_USA = NB_EVT_HIGH_RECOVER_TEMP_ADC;

#if defined(CONFIG_USA_MODEL_SGH_I757) || \
	defined(CONFIG_CAN_MODEL_SGH_I757M)
		if (!info->lpm_chg_mode)
			high_recover_temp_USA = NB_EVT_HIGH_RECOVER_TEMP_ADC_ON;
#endif
	} else {
		high_block_temp_USA = NB_HIGH_BLOCK_TEMP_ADC;
		high_recover_temp_USA = NB_HIGH_RECOVER_TEMP_ADC;

#if defined(CONFIG_USA_MODEL_SGH_I717)
		if (info->lpm_chg_mode)
			high_recover_temp_USA -= NB_HIGH_TEMP_ADC_DELTA;
#endif

#if defined(CONFIG_USA_MODEL_SGH_I577) || \
	defined(CONFIG_CAN_MODEL_SGH_I577R)
		if (info->lpm_chg_mode)
			high_block_temp_USA += NB_HIGH_TEMP_ADC_DELTA;
#endif

#if defined(CONFIG_USA_MODEL_SGH_I757) || \
	defined(CONFIG_CAN_MODEL_SGH_I757M)
		if (!info->lpm_chg_mode) {
			high_block_temp_USA = NB_HIGH_BLOCK_TEMP_ADC_ON;
			high_recover_temp_USA = NB_HIGH_RECOVER_TEMP_ADC_ON;
		}
#endif
	}

	if (rescale_adc <= high_block_temp_USA) {
		if (health != POWER_SUPPLY_HEALTH_OVERHEAT)
			if (info->batt_temp_high_cnt <= TEMP_BLOCK_COUNT)
				info->batt_temp_high_cnt++;
	} else if (rescale_adc >= high_recover_temp_USA &&
		rescale_adc <= NB_LOW_RECOVER_TEMP_ADC) {
		if (health == POWER_SUPPLY_HEALTH_OVERHEAT ||
		    health == POWER_SUPPLY_HEALTH_COLD) {
			info->batt_temp_high_cnt = 0;
			info->batt_temp_low_cnt = 0;
		}
	} else if (rescale_adc >= NB_LOW_BLOCK_TEMP_ADC) {
		if (health != POWER_SUPPLY_HEALTH_COLD)
			if (info->batt_temp_low_cnt <= TEMP_BLOCK_COUNT)
				info->batt_temp_low_cnt++;
	}

	if (info->batt_temp_high_cnt >= TEMP_BLOCK_COUNT)
		health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (info->batt_temp_low_cnt >= TEMP_BLOCK_COUNT)
		health = POWER_SUPPLY_HEALTH_COLD;
	else
		health = POWER_SUPPLY_HEALTH_GOOD;
skip_hupdate:
	if (info->batt_health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE &&
		info->batt_health != POWER_SUPPLY_HEALTH_DEAD &&
		health != info->batt_health) {
		info->batt_health = health;
		cancel_work_sync(&info->monitor_work);
		wake_lock(&info->monitor_wake_lock);
		queue_work(info->monitor_wqueue, &info->monitor_work);
	}

	/* if you need below, consider ADC_QUEUE_FEATURE if it is enabled */
	/*
	adc_data = 0;
	adc_physical = 0;
	ret = sec_bat_read_adc(info, CHANNEL_ADC_BATT_THERM,
						&adc_data, &adc_physical);
	printk("%s: channel : %d, raw adc is %d, result is %d\n",
		__func__, CHANNEL_ADC_BATT_THERM, adc_data, adc_physical);
	*/
	return 0;
}
#else

/*
static int sec_bat_check_temper_adc_sub(struct sec_bat_info *info)
{
	int ret = 0;
	int adc_data = 0, adc_physical = 0;

	if (!info->has_sub_therm)
		return 0;

	ret = sec_bat_read_adc(info, info->adc_channel_sub,
		&adc_data, &adc_physical);

	pr_info("%s: channel : %d, raw adc is %d, result is %d (temper.)\n",
		__func__, info->adc_channel_sub, adc_data, adc_physical);

	if (ret) {
		pr_err("%s : read error! skip update\n", __func__);
	} else {
		info->batt_temp_adc_sub = adc_data;
		if (sec_bat_rescale_adcvalue(info, info->adc_channel_sub,
			adc_data, adc_physical) < 0) {
			pr_info("%s: rescale failed, return\n", __func__);
		}
	}

	return 0;
}
*/

static int sec_bat_check_temper_adc(struct sec_bat_info *info)
{
#ifndef ADC_QUEUE_FEATURE
	int ret = 0;
#endif
	int adc_data = 0, adc_physical = 0;
	int rescale_adc = 0;
	int health = info->batt_health;

#ifdef ADC_QUEUE_FEATURE
	if (sec_bat_get_adc_depot(info, info->adc_channel_main,
		&adc_data, &adc_physical) < 0) {
		pr_info("%s: get adc depot failed (chan - %d), return\n",
			__func__, info->adc_channel_main);
		return 0;
	}
	/*
	if (!info->is_adc_ok || info->is_adc_wq_freezed) {
		adc_data = 333;
		adc_physical = RCOMP0_TEMP*10;
	}
	*/
#else /* ADC_QUEUE_FEATURE */
	ret = sec_bat_read_adc(info, info->adc_channel_main,
		&adc_data, &adc_physical);
	/*
	printk("%s: channel : %d, raw adc is %d, result is %d (temper.)\n",
		__func__, info->adc_channel_main, adc_data, adc_physical);
	*/
	if (ret) {
		pr_err("%s : read error! skip update\n", __func__);
		return 0;
	}
#endif /* ADC_QUEUE_FEATURE */
	info->batt_temp_adc = adc_data;
	if (sec_bat_rescale_adcvalue(info, info->adc_channel_main,
		adc_data, adc_physical) < 0) {
		pr_err("%s: rescale failed, return\n", __func__);
		return 0;
	}

	rescale_adc = info->batt_temp_radc;
	if (info->test_info.test_value == 1) {
		pr_info("%s : test case : %d\n", __func__,
			info->test_info.test_value);
		rescale_adc = info->temper_spec.high_block + 1;
		if (info->cable_type == CABLE_TYPE_NONE)
			rescale_adc = info->temper_spec.high_recovery - 1;
		info->batt_temp_radc = rescale_adc;
	}

	if (info->cable_type == CABLE_TYPE_NONE ||
		info->test_info.test_value == 999) {
		info->batt_temp_high_cnt = 0;
		info->batt_temp_low_cnt = 0;
		health = POWER_SUPPLY_HEALTH_GOOD;
#if defined(CONFIG_JPN_MODEL_SC_03D)
		info->jpn_chg_cur_ctrl_status = POWER_SUPPLY_HEALTH_GOOD;
		info->jpn_chg_cur_ctrl_temp_high_cnt = 0;
#endif
		goto skip_hupdate;
	}

	if (rescale_adc >= info->temper_spec.high_block) {
		if (health != POWER_SUPPLY_HEALTH_OVERHEAT)
			if (info->batt_temp_high_cnt <= TEMP_BLOCK_COUNT)
				info->batt_temp_high_cnt++;
	} else if (rescale_adc <= info->temper_spec.high_recovery &&
		rescale_adc >= info->temper_spec.low_recovery) {
		if (health == POWER_SUPPLY_HEALTH_OVERHEAT ||
		    health == POWER_SUPPLY_HEALTH_COLD) {
			info->batt_temp_high_cnt = 0;
			info->batt_temp_low_cnt = 0;
		}
	} else if (rescale_adc <= info->temper_spec.low_block) {
		if (health != POWER_SUPPLY_HEALTH_COLD)
			if (info->batt_temp_low_cnt <= TEMP_BLOCK_COUNT)
				info->batt_temp_low_cnt++;
	}
#if defined(CONFIG_JPN_MODEL_SC_03D)
	if (rescale_adc >= JPN_CHARGE_CURRENT_DOWN_TEMP) {
		if (info->jpn_chg_cur_ctrl_status !=
			POWER_SUPPLY_HEALTH_OVERHEAT)
			if (info->jpn_chg_cur_ctrl_temp_high_cnt <=
				TEMP_BLOCK_COUNT)
				info->jpn_chg_cur_ctrl_temp_high_cnt++;
	} else if (rescale_adc <= JPN_CHARGE_CURRENT_RECOVERY_TEMP) {
		if (info->jpn_chg_cur_ctrl_status ==
			POWER_SUPPLY_HEALTH_OVERHEAT) {
			info->jpn_chg_cur_ctrl_temp_high_cnt = 0;
		}
	}

	if (info->jpn_chg_cur_ctrl_temp_high_cnt >= TEMP_BLOCK_COUNT)
		info->jpn_chg_cur_ctrl_status = POWER_SUPPLY_HEALTH_OVERHEAT;
	else
		info->jpn_chg_cur_ctrl_status = POWER_SUPPLY_HEALTH_GOOD;

	/*
	pr_info("[SC-03D] %s : adc=%d, jpn_chg_cur_ctrl_status=%d, jpn_temp_h"
			"igh_cnt=%d\n", __func__, rescale_adc,
			info->jpn_chg_cur_ctrl_status,
			info->jpn_chg_cur_ctrl_temp_high_cnt);
	*/
#endif

	if (info->batt_temp_high_cnt >= TEMP_BLOCK_COUNT)
		health = POWER_SUPPLY_HEALTH_OVERHEAT;
	else if (info->batt_temp_low_cnt >= TEMP_BLOCK_COUNT)
		health = POWER_SUPPLY_HEALTH_COLD;
	else
		health = POWER_SUPPLY_HEALTH_GOOD;
skip_hupdate:
	if (info->batt_health != POWER_SUPPLY_HEALTH_UNSPEC_FAILURE &&
		info->batt_health != POWER_SUPPLY_HEALTH_DEAD &&
		health != info->batt_health) {
		info->batt_health = health;
		cancel_work_sync(&info->monitor_work);
		wake_lock(&info->monitor_wake_lock);
		queue_work(info->monitor_wqueue, &info->monitor_work);
	}

	return 0;
}
#endif

static void check_chgcurrent(struct sec_bat_info *info)
{
#ifndef ADC_QUEUE_FEATURE
	int ret = 0;
#endif
	int adc_data = 0, adc_physical = 0;

#if defined(CONFIG_KOR_MODEL_SHV_E160S) || \
	defined(CONFIG_KOR_MODEL_SHV_E160K) || \
	defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	if (info->hw_rev < 0x04)
		return;
#endif

#ifdef ADC_QUEUE_FEATURE
	if (sec_bat_get_adc_depot(info, CHANNEL_ADC_CHG_MONITOR,
		&adc_data, &adc_physical) < 0) {
		pr_err("%s: get adc depot failed (chan - %d), return\n",
			__func__, CHANNEL_ADC_CHG_MONITOR);
		return;
	}
	info->batt_current_adc = adc_physical;
#else /* ADC_QUEUE_FEATURE */
	ret = sec_bat_read_adc(info, CHANNEL_ADC_CHG_MONITOR,
		&adc_data, &adc_physical);
	/*
	printk("%s: channel : %d, raw adc is %d, result is %d\n",
		__func__, CHANNEL_ADC_CHG_MONITOR, adc_data, adc_physical);
	*/
	if (ret)
		adc_physical = info->batt_current_adc;
	info->batt_current_adc = adc_physical;
#endif /* ADC_QUEUE_FEATURE */

	/*
	printk("[chg_cur] %d, %d\n", info->batt_current_adc, chg_current_adc);
	*/
	dev_dbg(info->dev,
		"[battery] charging current = %d\n", info->batt_current_adc);
}

#if defined(CONFIG_KOR_MODEL_SHV_E160S) || \
	defined(CONFIG_KOR_MODEL_SHV_E160K) || \
	defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
static void check_chgstop_from_charger(struct sec_bat_info *info)
{
	struct power_supply *psy = power_supply_get_by_name(info->charger_name);
	union power_supply_propval value;
	static int cnt;
	int ret;

	if (!psy) {
		dev_err(info->dev, "%s: fail to get charger ps\n", __func__);
		return;
	}

	if (info->charging_enabled) {
		if (info->batt_vcell >= info->full_cond_voltage) {
			ret = psy->get_property(psy,
				POWER_SUPPLY_PROP_CHARGE_FULL, &value);
			if (ret < 0) {
				dev_err(info->dev,
					"%s: fail to get charge full(%d)\n",
					__func__, ret);
				return;
			}

			if (info->test_info.test_value == 3)
				value.intval = 0;

			info->is_top_off = value.intval;

			if (info->is_top_off == 1) {
				cnt++; /* accumulated counting */
				pr_info("%s : full state? %d, %d\n",
					__func__, info->batt_current_adc, cnt);
				if (cnt >= info->full_cond_count) {
					pr_info("%s : full state!! %d/%d\n",
						__func__, cnt,
						info->full_cond_count);
					sec_bat_handle_charger_topoff(info);
					cnt = 0;
				}
			}
		} else {
			cnt = 0;
		}
	} else {
		cnt = 0;
		info->batt_current_adc = 0;
	}
	info->test_info.full_count = cnt;
}
#endif

static void sec_check_chgcurrent(struct sec_bat_info *info)
{
	static int cnt;
	static int cnt_ui;
#ifdef ADC_QUEUE_FEATURE
	bool is_full_condition = false;
#endif

#if defined(GET_TOPOFF_WITH_REGISTER)
	struct power_supply *psy = power_supply_get_by_name(info->charger_name);
	union power_supply_propval value;
	int ret;

	if (!psy) {
		dev_err(info->dev, "%s: fail to get charger ps\n", __func__);
		return;
	}
#endif

	if (info->charging_enabled) {
		check_chgcurrent(info);
#ifndef ADC_QUEUE_FEATURE
		/* AGAIN_FEATURE */
		if (info->batt_current_adc <= CURRENT_OF_FULL_CHG)
			check_chgcurrent(info);
#endif

		/* if (info->batt_vcell >= FULL_CHARGE_COND_VOLTAGE) { */
		if (info->batt_vcell >= info->full_cond_voltage) {
#if defined(GET_TOPOFF_WITH_REGISTER)
			/* check full state with smb328a register */
			/* check 36h bit6 */
			ret = psy->get_property(psy,
				POWER_SUPPLY_PROP_CHARGE_FULL, &value);
			if (ret < 0) {
				dev_err(info->dev,
					"%s: fail to get charge full(%d)\n",
					__func__, ret);
				return;
			}

			if (info->test_info.test_value == 3)
				value.intval = 0;

			info->is_top_off = value.intval;
#endif /* GET_TOPOFF_WITH_REGISTER */

#if defined(CHK_TOPOFF_WITH_REGISTER_ONLY)
			if (info->is_top_off == 1)
#else /* mainly, check topoff with vichg adc value */
			if (info->test_info.test_value == 3) {
				info->batt_current_adc =
					CURRENT_OF_FULL_CHG + 1;
			}
#ifdef ADC_QUEUE_FEATURE
			/*
			if ((info->batt_current_adc <= CURRENT_OF_FULL_CHG) ||
				(info->is_adc_wq_freezed &&
					info->is_top_off == 1))
			*/
			if (info->is_adc_wq_freezed || !info->is_adc_ok ||
				(info->batt_current_adc == 0)) {
				if (info->is_top_off == 1) {
					is_full_condition = true;
					pr_info("%s : is_top_off "
						"(%d, %d, %d)\n",
						__func__,
						info->is_adc_wq_freezed,
						info->is_adc_ok,
						info->batt_current_adc);
				} else {
					is_full_condition = false;
				}
			} else { /* adc data is ok */
#if defined(CONFIG_TARGET_LOCALE_USA)
				if (info->charging_status !=
				    POWER_SUPPLY_STATUS_FULL &&
				    info->batt_current_adc <=
				    CURRENT_OF_FULL_CHG_UI &&
				    !info->ui_full_charge_status) {
					cnt_ui++;
					pr_info("%s : UI full state? %d, %d\n",
						__func__,
						info->batt_current_adc,
						cnt_ui);
					if (cnt_ui >= info->full_cond_count) {
						pr_info("%s : UI full state!!"
							" %d/%d\n",
							__func__, cnt_ui,
							info->full_cond_count);

						info->ui_full_charge_status =
							true;
						cnt_ui = 0;
						power_supply_changed(
							&info->psy_bat);
					}
				}
#endif /* CONFIG_TARGET_LOCALE_USA */
				if (info->batt_current_adc <=
					CURRENT_OF_FULL_CHG) {
					is_full_condition = true;
				} else {
					is_full_condition = false;
				}
			}

			if (is_full_condition) {
#else
			if (info->batt_current_adc <= CURRENT_OF_FULL_CHG) {
#endif /* ADC_QUEUE_FEATURE */
#endif /* CHK_TOPOFF_WITH_REGISTER_ONLY */
				cnt++; /* accumulated counting */
				pr_info("%s : full state? %d, %d\n",
					__func__, info->batt_current_adc, cnt);
				/* if (cnt >= FULL_CHG_COND_COUNT) { */
				if (cnt >= info->full_cond_count) {
					/*
					pr_info("%s : full state!! %d/%d\n",
						__func__, cnt,
						FULL_CHG_COND_COUNT);
					*/
					pr_info("%s : full state!! %d/%d\n",
						__func__, cnt,
						info->full_cond_count);
					sec_bat_handle_charger_topoff(info);
					cnt = 0;
				}
			}
		} else {
			cnt = 0;
			cnt_ui = 0;
		}
	} else {
		cnt = 0;
		cnt_ui = 0;
		info->batt_current_adc = 0;
	}
	info->test_info.full_count = cnt;
}

static int sec_check_recharging(struct sec_bat_info *info)
{
	static int cnt;
	int ret;

	if (info->charging_status != POWER_SUPPLY_STATUS_FULL ||
		info->recharging_status != false) {
		cnt = 0;
		return 0;
	}

	info->batt_vcell = sec_bat_get_fuelgauge_data(info, FG_T_VCELL);

	if (info->batt_vcell > RECHARGING_VOLTAGE) {
		cnt = 0;
		return 0;
	} else {
		/* AGAIN_FEATURE
		info->batt_vcell = sec_bat_get_fuelgauge_data(info, FG_T_VCELL);
		 */
		if (info->batt_vcell <= RECHARGING_VOLTAGE) {
			cnt++; /* continuous counting */
			pr_info("%s : rechg condition ? %d\n", __func__, cnt);
			if (cnt >= RE_CHG_COND_COUNT) {
				pr_info("%s : rechg condition(1) OK - %d\n",
					__func__, cnt);
				cnt = 0;
				info->test_info.is_rechg_state = true;
				ret = 1;
			} else if (cnt >= RE_CHG_MIN_COUNT &&
					info->batt_vcell <=
					FULL_CHARGE_COND_VOLTAGE) {
				pr_info("%s : rechg condition(2) OK - %d\n",
					__func__, cnt);
				cnt = 0;
				info->test_info.is_rechg_state = true;
				ret = 1;
			} else
				ret = 0;
		} else {
			cnt = 0;
			ret = 0;
		}
	}
	info->test_info.rechg_count = cnt;

	return ret;
}

static int sec_bat_notify_vcell2charger(struct sec_bat_info *info)
{
	struct power_supply *psy = power_supply_get_by_name(info->charger_name);
	union power_supply_propval val_vcell;
	int ret;

	if (!psy) {
		dev_err(info->dev, "%s: fail to get charger ps\n", __func__);
		return -ENODEV;
	}

	/* Notify Voltage Now */
	val_vcell.intval = info->batt_vcell;
	ret = psy->set_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW,
				&val_vcell);
	if (ret) {
		dev_err(info->dev, "%s: fail to notify vcell now(%d)\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

static void sec_bat_update_info(struct sec_bat_info *info)
{
	info->batt_presoc = info->batt_soc;
	info->batt_raw_soc = sec_bat_get_fuelgauge_data(info, FG_T_PSOC);
	info->batt_soc = sec_bat_get_fuelgauge_data(info, FG_T_SOC);
	info->batt_vcell = sec_bat_get_fuelgauge_data(info, FG_T_VCELL);
	info->batt_rcomp = sec_bat_get_fuelgauge_data(info, FG_T_RCOMP);
	info->batt_full_soc = sec_bat_get_fuelgauge_data(info, FG_T_FSOC);
	sec_bat_notify_vcell2charger(info);
}

static int sec_bat_enable_charging(struct sec_bat_info *info, bool enable)
{
	struct power_supply *psy = power_supply_get_by_name(info->charger_name);
	struct power_supply *psy_fg =
		power_supply_get_by_name(info->fuel_gauge_name);
	union power_supply_propval val_type,
		val_chg_current, val_topoff, val_vcell;
	int ret;

	if (!psy || !psy_fg) {
		dev_err(info->dev, "%s: fail to get charger ps\n", __func__);
		return -ENODEV;
	}

	info->batt_full_status = BATT_NOT_FULL;

	if (enable) { /* Enable charging */
		switch (info->cable_type) {
		case CABLE_TYPE_USB:
			val_type.intval = POWER_SUPPLY_STATUS_CHARGING;
			val_chg_current.intval = 500; /* USB 500 mode */
			info->full_cond_count = USB_FULL_COND_COUNT;
			info->full_cond_voltage = USB_FULL_COND_VOLTAGE;
			break;
		case CABLE_TYPE_AC:
		case CABLE_TYPE_CARDOCK:
		case CABLE_TYPE_UARTOFF:
			val_type.intval = POWER_SUPPLY_STATUS_CHARGING;
			 /* input : 900mA, output : 900mA */
			val_chg_current.intval = 900;
			info->full_cond_count = FULL_CHG_COND_COUNT;
			info->full_cond_voltage = FULL_CHARGE_COND_VOLTAGE;
			break;
		case CABLE_TYPE_MISC:
			val_type.intval = POWER_SUPPLY_STATUS_CHARGING;
			 /* input : 700, output : 700mA */
			val_chg_current.intval = 700;
			info->full_cond_count = FULL_CHG_COND_COUNT;
			info->full_cond_voltage = FULL_CHARGE_COND_VOLTAGE;
			break;
		case CABLE_TYPE_UNKNOWN:
			val_type.intval = POWER_SUPPLY_STATUS_CHARGING;
			 /* input : 450, output : 500mA */
			val_chg_current.intval = 450;
			info->full_cond_count = USB_FULL_COND_COUNT;
			info->full_cond_voltage = USB_FULL_COND_VOLTAGE;
			break;
		default:
			dev_err(info->dev, "%s: Invalid func use\n", __func__);
			return -EINVAL;
		}

		/* Set charging current */
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_CURRENT_NOW,
					&val_chg_current);
		if (ret) {
			dev_err(info->dev, "%s: fail to set charging cur(%d)\n",
				__func__, ret);
			return ret;
		}

		/* Set topoff current */
		/* from 25mA to 200mA, in 25mA step */
		val_topoff.intval = 200;
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_CHARGE_FULL,
					&val_topoff);
		if (ret) {
			dev_err(info->dev, "%s: fail to set topoff val(%d)\n",
				__func__, ret);
			return ret;
		}

		/* Notify Voltage Now */
		info->batt_vcell = sec_bat_get_fuelgauge_data(info, FG_T_VCELL);
		val_vcell.intval = info->batt_vcell;
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW,
					&val_vcell);
		if (ret) {
			dev_err(info->dev, "%s: fail to notify vcell now(%d)\n",
				__func__, ret);
			return ret;
		}

		/* Adjust Charging Current */
		if (info->lpm_chg_mode &&
			(info->cable_type == CABLE_TYPE_AC ||
			 info->cable_type == CABLE_TYPE_UARTOFF ||
			 info->cable_type == CABLE_TYPE_CARDOCK)) {
			pr_info("%s : lpm-mode, adjust charging current"
				" (to 1A)\n",
				__func__);
			sec_bat_adjust_charging_current(info, 1000); /* 1A */
		}

		info->charging_start_time = jiffies;
	} else { /* Disable charging */
		val_type.intval = POWER_SUPPLY_STATUS_DISCHARGING;
		info->charging_passed_time = 0;
		info->charging_start_time = 0;
	}

	ret = psy->set_property(psy, POWER_SUPPLY_PROP_STATUS, &val_type);
	if (ret) {
		dev_err(info->dev, "%s: fail to set charging status(%d)\n",
			__func__, ret);
		return ret;
	}

	info->charging_enabled = enable;

	return 0;
}

#ifdef ADJUST_RCOMP_WITH_CHARGING_STATUS
static int sec_fg_update_rcomp(struct sec_bat_info *info)
{
	struct power_supply *psy
	    = power_supply_get_by_name(info->fuel_gauge_name);
	union power_supply_propval value;
	int ret;

	if (!psy) {
		pr_err("%s: fail to get fuelgauge ps\n", __func__);
		return -ENODEV;
	}

	if (info->fuel_gauge_name) {
		value.intval = info->charging_status;
		ret =
		psy->set_property(psy, POWER_SUPPLY_PROP_STATUS, &value);
		if (ret) {
			dev_err(info->dev, "%s: fail to set status(%d)\n",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}
#endif

static void sec_bat_handle_unknown_disable(struct sec_bat_info *info)
{
	pr_info(" %s : cable_type = %d\n", __func__, info->cable_type);

	info->batt_full_status = BATT_NOT_FULL;
	info->recharging_status = false;
	info->test_info.is_rechg_state = false;
	info->charging_start_time = 0;
#if defined(CONFIG_TARGET_LOCALE_USA)
	info->ui_full_charge_status = false;
#endif
	info->charging_status = POWER_SUPPLY_STATUS_DISCHARGING;
	info->is_timeout_chgstop = false;
#ifdef ADJUST_RCOMP_WITH_CHARGING_STATUS
	sec_fg_update_rcomp(info);
#endif
	sec_bat_enable_charging(info, false);

	/*
	power_supply_changed(&info->psy_ac);
	power_supply_changed(&info->psy_usb);
	*/
	power_supply_changed(&info->psy_bat);
}

static void sec_bat_cable_work(struct work_struct *work)
{
	struct sec_bat_info *info = container_of(work, struct sec_bat_info,
							cable_work.work);

	/*
	if (info->cable_type == CABLE_TYPE_UNKNOWN)
		info->cable_type = CABLE_TYPE_NONE;
	if (info->cable_type != CABLE_TYPE_NONE) {
		sec_bat_enable_charging(info, true);
		info->cable_type = CABLE_TYPE_NONE;
	}
	*/

	#ifdef CONFIG_USA_MODEL_SGH_I717
        if (info->cable_type != CABLE_TYPE_NONE) {
                sec_bat_check_detbat(info);
                sec_bat_check_vf(info);
        }
	#endif
	
	switch (info->cable_type) {
	case CABLE_TYPE_NONE:
		/* TODO : check DCIN state again*/
		/* test */
		/*
		if (gpio_get_value_cansleep(info->mpp_get_cblpwr) == 0) {
			pr_info("cable none : mpp high skip!!!\n");
			return 0;
		}
		*/

		if ((sec_bat_is_charging(info) ==
			POWER_SUPPLY_STATUS_CHARGING) &&
			info->dcin_intr_triggered) {
			pr_info("cable none : vdcin ok, skip!!!\n");
			return;
		}
		wake_lock_timeout(&info->vbus_wake_lock, 5 * HZ);
		cancel_delayed_work(&info->measure_work);
		info->batt_full_status = BATT_NOT_FULL;
		info->recharging_status = false;
		info->test_info.is_rechg_state = false;
		info->charging_start_time = 0;
		info->charging_status = POWER_SUPPLY_STATUS_DISCHARGING;
		info->is_timeout_chgstop = false;
		info->dcin_intr_triggered = false;
#if defined(CONFIG_TARGET_LOCALE_USA)
		info->ui_full_charge_status = false;
#endif
#ifdef ADJUST_RCOMP_WITH_CHARGING_STATUS
		sec_fg_update_rcomp(info);
#endif
		sec_bat_enable_charging(info, false);
		info->measure_interval = MEASURE_DSG_INTERVAL;
		wake_lock(&info->measure_wake_lock);
		queue_delayed_work(info->monitor_wqueue,
			&info->measure_work, HZ/2);
		/*
		schedule_delayed_work(&info->measure_work, 0);
		*/
		break;
	case CABLE_TYPE_MISC:
	case CABLE_TYPE_CARDOCK:
#if !defined(CONFIG_TARGET_LOCALE_USA)
	case CABLE_TYPE_UARTOFF:
#endif

#if defined(CONFIG_TARGET_LOCALE_USA)
		if (!info->dcin_intr_triggered) {
#else
		if (!info->dcin_intr_triggered && !info->lpm_chg_mode) {
#endif
			wake_lock_timeout(&info->vbus_wake_lock, 5 * HZ);
			pr_info("%s : dock inserted, "
				"but dcin nok skip charging!\n", __func__);
			sec_bat_enable_charging(info, true);
			info->charging_enabled = false;
			break;
		}
		sec_bat_enable_charging(info, true);
		info->charging_status = POWER_SUPPLY_STATUS_CHARGING;
		break;
#if defined(CONFIG_TARGET_LOCALE_USA)
	case CABLE_TYPE_UARTOFF:
		if (!info->dcin_intr_triggered) {
			pr_info("%s : jig cable is attached without vbus, "
				"skip charging!\n",
				   __func__);
			break;
		} else
		{
			pr_info("%s : jig cable is attached with vbus\n",
				   __func__);
			break;
		}
#endif
	case CABLE_TYPE_UNKNOWN:
#if defined(CONFIG_TOUCHSCREEN_QT602240) || defined(CONFIG_TOUCHSCREEN_MXT768E)
		tsp_set_unknown_charging_cable(true);
#endif
	case CABLE_TYPE_USB:
	case CABLE_TYPE_AC:
		/* TODO : check DCIN state again*/
		wake_lock(&info->vbus_wake_lock);
		cancel_delayed_work(&info->measure_work);
		info->charging_status = POWER_SUPPLY_STATUS_CHARGING;
#ifdef ADJUST_RCOMP_WITH_CHARGING_STATUS
		sec_fg_update_rcomp(info);
#endif
		sec_bat_enable_charging(info, true);
		info->measure_interval = MEASURE_CHG_INTERVAL;
		wake_lock(&info->measure_wake_lock);
		queue_delayed_work(info->monitor_wqueue,
			&info->measure_work, HZ/2);
		/*
		schedule_delayed_work(&info->measure_work, 0);
		*/
		break;
	default:
		dev_err(info->dev, "%s: Invalid cable type\n", __func__);
		break;
	}

	/*
	power_supply_changed(&info->psy_ac);
	power_supply_changed(&info->psy_usb);
	*/
	power_supply_changed(&info->psy_bat);
	/* TBD */
	/*
	wake_lock(&info->monitor_wake_lock);
	queue_work(info->monitor_wqueue, &info->monitor_work);
	*/

	wake_unlock(&info->cable_wake_lock);
}

static void sec_bat_charging_time_management(struct sec_bat_info *info)
{
	unsigned long charging_time;

	if (info->charging_start_time == 0) {
		dev_dbg(info->dev, "%s: charging_start_time has never "
			 "been used since initializing\n", __func__);
		return;
	}

	if (jiffies >= info->charging_start_time)
		charging_time = jiffies - info->charging_start_time;
	else
		charging_time = 0xFFFFFFFF - info->charging_start_time
		    + jiffies;

	info->charging_passed_time = charging_time;

	switch (info->charging_status) {
	case POWER_SUPPLY_STATUS_FULL:
		if (time_after(charging_time, (unsigned long)RECHARGING_TIME) &&
		    info->recharging_status == true) {
			sec_bat_enable_charging(info, false);
			info->recharging_status = false;
			info->is_timeout_chgstop = true;
			dev_info(info->dev, "%s: Recharging timer expired\n",
				 __func__);
		}
		break;
	case POWER_SUPPLY_STATUS_CHARGING:
		if (time_after(charging_time,
			       (unsigned long)FULL_CHARGING_TIME)) {
			sec_bat_enable_charging(info, false);
			info->charging_status = POWER_SUPPLY_STATUS_FULL;
			info->is_timeout_chgstop = true;

			dev_info(info->dev, "%s: Charging timer expired\n",
				 __func__);
		}
		break;
	default:
		info->is_timeout_chgstop = false;
		return;
	}

	dev_dbg(info->dev, "Time past : %u secs, Spec : %u, %u\n",
		jiffies_to_msecs(charging_time) / 1000,
		FULL_CHARGING_TIME, RECHARGING_TIME);

	return;
}

#ifdef ADJUST_RCOMP_WITH_TEMPER
static int sec_fg_update_temper(struct sec_bat_info *info)
{
	struct power_supply *psy
	    = power_supply_get_by_name(info->fuel_gauge_name);
	union power_supply_propval value;
	int ret;

	/* Notify temperature to fuel gauge */
	if (info->fuel_gauge_name) {
		if (!info->is_adc_ok || info->is_adc_wq_freezed)
			value.intval = RCOMP0_TEMP;
		else
			value.intval = info->batt_temp / 10;
		ret = psy->set_property(psy, POWER_SUPPLY_PROP_TEMP, &value);
		if (ret) {
			dev_err(info->dev, "%s: fail to set temperature(%d)\n",
				__func__, ret);
			return ret;
		}
	}
	return 0;
}
#endif

static void sec_bat_monitor_work(struct work_struct *work)
{
	struct sec_bat_info *info = container_of(work, struct sec_bat_info,
						 monitor_work);
	struct power_supply *psy_fg =
		power_supply_get_by_name(info->fuel_gauge_name);
	union power_supply_propval value;
	int ret = 0;

	if (!psy_fg) {
		pr_err("%s: fail to get charger ps\n", __func__);
		return;
	}

	wake_lock(&info->monitor_wake_lock);

	sec_bat_charging_time_management(info);

	sec_bat_update_info(info);
	sec_bat_check_vf(info);
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || \
	defined(CONFIG_KOR_MODEL_SHV_E160K) || \
	defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	if (info->hw_rev < 0x04)
		check_chgstop_from_charger(info);
	else
		sec_check_chgcurrent(info);
#else
	sec_check_chgcurrent(info);
#endif

#ifdef ADJUST_RCOMP_WITH_TEMPER
	sec_fg_update_temper(info);
#endif

	switch (info->charging_status) {
	case POWER_SUPPLY_STATUS_FULL:
		/* notify full state to fuel guage */
		if (!info->is_timeout_chgstop) {
			value.intval = POWER_SUPPLY_STATUS_FULL;
			ret = psy_fg->set_property(psy_fg,
				POWER_SUPPLY_PROP_STATUS, &value);
		}

		/* if (sec_check_recharging(info) && */
		if (info->is_rechg_triggered &&
		    info->recharging_status == false) {
			info->recharging_status = true;
			sec_bat_enable_charging(info, true);
		    info->is_rechg_triggered = false;

			dev_info(info->dev,
				 "%s: Start Recharging, Vcell = %d\n", __func__,
				 info->batt_vcell);
		}
		/* break; */
	case POWER_SUPPLY_STATUS_CHARGING:
		if (info->batt_health == POWER_SUPPLY_HEALTH_OVERHEAT
		    || info->batt_health == POWER_SUPPLY_HEALTH_COLD) {
			sec_bat_enable_charging(info, false);
			info->charging_status =
			    POWER_SUPPLY_STATUS_NOT_CHARGING;
			info->test_info.is_rechg_state = false;

			dev_info(info->dev, "%s: Not charging\n", __func__);
		} else if (info->batt_health ==
				POWER_SUPPLY_HEALTH_UNSPEC_FAILURE) {
			sec_bat_enable_charging(info, false);
			info->charging_status =
				POWER_SUPPLY_STATUS_NOT_CHARGING;
			info->test_info.is_rechg_state = false;
			dev_info(info->dev,
				"%s: Not charging (VF err!)\n", __func__);
		}
		break;
	case POWER_SUPPLY_STATUS_DISCHARGING:
		dev_dbg(info->dev, "%s: Discharging\n", __func__);
		break;
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		if (info->batt_health == POWER_SUPPLY_HEALTH_GOOD) {
			dev_info(info->dev, "%s: recover health state\n",
				 __func__);
			if (info->cable_type != CABLE_TYPE_NONE) {
				sec_bat_enable_charging(info, true);
				info->charging_status
				    = POWER_SUPPLY_STATUS_CHARGING;
			} else
				info->charging_status
				    = POWER_SUPPLY_STATUS_DISCHARGING;
		}
		break;
	default:
		pr_err("%s: Undefined Battery Status\n", __func__);
		wake_unlock(&info->monitor_wake_lock);
		return;
	}

	/* check default charger state, and set again */
	if (sec_bat_is_charging(info) == POWER_SUPPLY_STATUS_CHARGING &&
		info->charging_enabled) {
		if (sec_bat_is_invalid_bmd(info)) {
			pr_info("%s : default charger state, set again\n",
				__func__);
			wake_lock(&info->cable_wake_lock);
			queue_delayed_work(info->monitor_wqueue,
				&info->cable_work, 0);
		}
	}

	if (info->batt_soc != info->batt_presoc)
		pr_err("[fg] p:%d, s1:%d, s2:%d, v:%d, t:%d\n",
			info->batt_raw_soc,
			info->batt_soc, info->batt_presoc,
			info->batt_vcell, info->batt_temp_radc);

	power_supply_changed(&info->psy_bat);

	wake_unlock(&info->monitor_wake_lock);

	return;
}

static void sec_bat_polling_work(struct work_struct *work)
{
	unsigned long flags;
	struct sec_bat_info *info =
		container_of(work, struct sec_bat_info, polling_work.work);
	int ret = 0;

#ifdef ADC_QUEUE_FEATURE
	local_irq_save(flags);
	if (!info->initial_check_count) {
		if (info->wadc_alive == info->wadc_alive_prev)
			info->wadc_freezed_count++;
		else
			info->wadc_freezed_count = 0;
	}
	if (info->wadc_freezed_count > 10) {
		info->is_adc_wq_freezed = true;
		info->wadc_freezed_count = 11;
	} else {
		info->is_adc_wq_freezed = false;
	}
	info->wadc_alive_prev = info->wadc_alive;
	local_irq_restore(flags);

	wake_lock(&info->adc_wake_lock);
	ret = queue_work(info->adc_wqueue, &info->adc_work);
	if (!ret)
		pr_err("%s: adc work already on a queue\n", __func__);

	if (info->is_adc_wq_freezed) {
		pr_err("%s: error! adc wq freezed(%d, %d, %d, %d, %d)!!!\n",
			__func__, info->wadc_alive,
			info->wadc_alive_prev, info->is_adc_wq_freezed,
			info->batt_temp_adc, info->batt_temp_radc);
		pr_err("%s: work is pending?? (%d)\n", __func__,
			work_pending(&info->adc_work));
	}
#endif

	wake_lock(&info->monitor_wake_lock);
	queue_work(info->monitor_wqueue, &info->monitor_work);

	if (info->initial_check_count) {
		schedule_delayed_work(&info->polling_work, HZ);
		info->initial_check_count--;
	} else
		schedule_delayed_work(&info->polling_work,
			msecs_to_jiffies(info->polling_interval));
}

static void sec_bat_measure_work(struct work_struct *work)
{
	struct sec_bat_info *info =
		container_of(work, struct sec_bat_info, measure_work.work);
	unsigned long flags;
	int set_chg_current;
#ifdef MPP_CHECK_FEATURE
	int val = 0;
#endif
	int ret = 0;
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || \
	defined(CONFIG_KOR_MODEL_SHV_E120K) || \
	defined(CONFIG_KOR_MODEL_SHV_E120L) || \
	defined(CONFIG_KOR_MODEL_SHV_E160S) || \
	defined(CONFIG_KOR_MODEL_SHV_E160K) || \
	defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	bool isFirstCheck = false;
#endif

	wake_lock(&info->measure_wake_lock);

	if (sec_check_recharging(info)) {
		pr_info("%s : rechg triggered!\n", __func__);
		info->is_rechg_triggered = true;
		cancel_work_sync(&info->monitor_work);
		wake_lock(&info->monitor_wake_lock);
		queue_work(info->monitor_wqueue, &info->monitor_work);
	}

#ifdef ADC_QUEUE_FEATURE
	wake_lock(&info->adc_wake_lock);
	ret = queue_work(info->adc_wqueue, &info->adc_work);
	if (!ret)
		pr_err("%s: adc work already on a queue\n", __func__);
#endif /* ADC_QUEUE_FEATURE */

#if defined(CONFIG_TARGET_LOCALE_USA)
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
	if (get_hw_rev() >= 0x06)
#elif defined(CONFIG_USA_MODEL_SGH_I727)
	if (get_hw_rev() >= 0x08)
#elif defined(CONFIG_USA_MODEL_SGH_I717) || \
	defined(CONFIG_USA_MODEL_SGH_I757) || \
	defined(CONFIG_USA_MODEL_SGH_I577) || \
	defined(CONFIG_CAN_MODEL_SGH_I577R) || \
	defined(CONFIG_CAN_MODEL_SGH_I757M)
	if (true)
#endif
		sec_bat_check_temper_adc_USA_nb(info);
	else
		sec_bat_check_temper_adc_USA(info);
#else
	sec_bat_check_temper_adc(info);
	/* sec_bat_check_temper_adc_sub(info); */
#endif

#ifdef ADC_QUEUE_FEATURE
	sec_bat_check_detbat(info);
#else
	if (sec_bat_check_detbat(info) == BAT_NOT_DETECTED &&
		info->present == 1)
		sec_bat_check_detbat(info); /* AGAIN_FEATURE */
#endif /* ADC_QUEUE_FEATURE */

	/* check dcin */
	if ((sec_bat_is_charging(info) == POWER_SUPPLY_STATUS_CHARGING) &&
		(info->charging_status == POWER_SUPPLY_STATUS_DISCHARGING)) {
		pr_info("%s : dcin ok, but not charging,"
			" set cable type again!\n", __func__);
#if defined(CELOX_BATTERY_CHARGING_CONTROL)
		if (0 == is_charging_disabled) {
#else
		if (true) {
#endif
			local_irq_save(flags);
			if (info->cable_type == CABLE_TYPE_NONE) {
#if defined(CONFIG_TARGET_LOCALE_USA)
				if (info->cable_uart_off)
					info->cable_type =
						CABLE_TYPE_UARTOFF;
				else
#endif
				info->cable_type = CABLE_TYPE_UNKNOWN;
			}

			local_irq_restore(flags);
			wake_lock(&info->cable_wake_lock);
			queue_delayed_work(info->monitor_wqueue,
				&info->cable_work, HZ);
		}
	} else if ((sec_bat_is_charging(info) ==
			POWER_SUPPLY_STATUS_DISCHARGING) &&
		(info->charging_status != POWER_SUPPLY_STATUS_DISCHARGING)) {
		pr_info("%s : dcin nok, but still charging,"
			" just disable charging!\n", __func__);
		local_irq_save(flags);
		if (info->cable_type == CABLE_TYPE_UNKNOWN ||
			info->cable_type == CABLE_TYPE_UARTOFF)
			info->cable_type = CABLE_TYPE_NONE;
		local_irq_restore(flags);
#if defined(CONFIG_TOUCHSCREEN_QT602240) || defined(CONFIG_TOUCHSCREEN_MXT768E)
		tsp_set_unknown_charging_cable(false);
#endif
		sec_bat_handle_unknown_disable(info);
	}

	/* TEST : adjust fast charging current */
	/*
	static int count = 0;
	static int chg_current = 500;
	*/
	/*
	if (info->charging_enabled) {
		count++;
		if (count%2 == 0) {
			sec_bat_adjust_charging_current(info, chg_current);
			chg_current+=100;
		}

		if (count > 100000)
			count = 0;
		if (chg_current > 1100)
			chg_current = 500;
	}
	*/

	/* Adjust Charging Current */
	if (!info->lpm_chg_mode &&
		(info->cable_type == CABLE_TYPE_AC ||
		 info->cable_type == CABLE_TYPE_UARTOFF ||
		 info->cable_type == CABLE_TYPE_CARDOCK) &&
		info->charging_enabled) {
		set_chg_current = sec_bat_get_charging_current(info);
#if defined(CONFIG_JPN_MODEL_SC_03D)
		if (set_chg_current >= 0) {
			if (info->jpn_chg_cur_ctrl_status ==
				POWER_SUPPLY_HEALTH_GOOD) {
				if (info->is_esus_state) {
					if (info->voice_call_state == 0 &&
						set_chg_current != 1000) {
						pr_info("[SC-03D] %s : "
							"adjust current to"
							" 1A\n", __func__);
						sec_bat_adjust_charging_current(
							info, 1000);
					}
				} else {
					if (set_chg_current != 900) {
						pr_info("[SC-03D] %s : "
							"adjust current to"
							" 0.9A\n", __func__);
						sec_bat_adjust_charging_current(
							info, 900);
					}
				}
			} else {
				/* charge curren down when temp is above 41  */
				if (set_chg_current != 600) {
					pr_info("[SC-03D] %s : Temp is above"
						" 41celsius, adjust current to"
						" 600mA, temp adc=%d\n",
						__func__, info->batt_temp_radc);
					sec_bat_adjust_charging_current(
						info, 600);
				}
			}
		} else {
			pr_info("%s : invalid charging current"
				" from charger (%d)\n",
				__func__, set_chg_current);
		}
		set_chg_current = sec_bat_get_charging_current(info);
		/*
		pr_info("[SC-03D] %s : adc=%d, present chg_current=%d\n",
			__func__, info->batt_temp_radc, set_chg_current);
		*/
#else
		if (set_chg_current >= 0) {
			if (info->is_esus_state) {
				if (info->voice_call_state == 0 &&
					set_chg_current != 1000) {
					pr_info("%s : adjust curretn to 1A\n",
						__func__);
					sec_bat_adjust_charging_current(
						info, 1000);
				}
			} else {
				if (set_chg_current != 900) {
					pr_info("%s : adjust curretn to 0.9A\n",
						__func__);
					sec_bat_adjust_charging_current(
						info, 900);
				}
			}
		} else {
			pr_info("%s : invalid charging current from charger"
				" (%d)\n", __func__, set_chg_current);
		}
#endif
	}

#ifdef MPP_CHECK_FEATURE
	/* check mpp cable_pwr state */
	val = gpio_get_value_cansleep(info->mpp_get_cblpwr);
	if (val < 0) {
		pr_err("%s gpio_get_value failed for %d ret=%d\n", __func__,
			info->mpp_get_cblpwr, val);
	} else {
		pr_info("%s : cblpwr state = %d\n", __func__, val);
	}
#endif

#if defined(CONFIG_KOR_MODEL_SHV_E120S) || \
	defined(CONFIG_KOR_MODEL_SHV_E120K) || \
	defined(CONFIG_KOR_MODEL_SHV_E120L) || \
	defined(CONFIG_KOR_MODEL_SHV_E160S) || \
	defined(CONFIG_KOR_MODEL_SHV_E160K) || \
	defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
		if (info->charging_enabled &&
			(((0 < info->batt_temp_high_cnt) &&
			(info->batt_temp_high_cnt < TEMP_BLOCK_COUNT))  ||
			((0 < info->batt_temp_low_cnt) &&
			(info->batt_temp_low_cnt < TEMP_BLOCK_COUNT)))) {
			isFirstCheck = true;
		} else {
			isFirstCheck = false;
		}
#endif

	if (info->initial_check_count) {
		queue_delayed_work(info->monitor_wqueue,
			&info->measure_work, HZ);
	}
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || \
	defined(CONFIG_KOR_MODEL_SHV_E120K) || \
	defined(CONFIG_KOR_MODEL_SHV_E120L) || \
	defined(CONFIG_KOR_MODEL_SHV_E160S) || \
	defined(CONFIG_KOR_MODEL_SHV_E160K) || \
	defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	else if (isFirstCheck) {
		queue_delayed_work(info->monitor_wqueue, &info->measure_work,
					  HZ);
	}
#endif
	else {
		queue_delayed_work(info->monitor_wqueue, &info->measure_work,
		      msecs_to_jiffies(info->measure_interval));
	}
	/*
	schedule_delayed_work(&info->measure_work,
		      msecs_to_jiffies(info->measure_interval));
	*/
	wake_unlock(&info->measure_wake_lock);
}

#ifdef ADC_QUEUE_FEATURE
static void sec_bat_adc_work(struct work_struct *work)
{
	int ret = 0;
	int channel = 0;
	int adc_data = 0, adc_physical = 0;
	struct sec_bat_info *info =
		container_of(work, struct sec_bat_info, adc_work);
	unsigned long flags;

	/*
	pr_info("%s :\n", __func__);
	*/
	wake_lock(&info->adc_wake_lock);

#if defined(CONFIG_TARGET_LOCALE_USA)
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
	if (get_hw_rev() >= 0x06)
#elif defined(CONFIG_USA_MODEL_SGH_I727)
	if (get_hw_rev() >= 0x08)
#elif defined(CONFIG_USA_MODEL_SGH_I717) || \
	defined(CONFIG_USA_MODEL_SGH_I757) || \
	defined(CONFIG_USA_MODEL_SGH_I577) || \
	defined(CONFIG_CAN_MODEL_SGH_I577R) || \
	defined(CONFIG_CAN_MODEL_SGH_I757M)
	if (true)
#endif
		channel = CHANNEL_ADC_BATT_THERM;
	else
		channel = CHANNEL_ADC_PMIC_THERM;
#else /* CONFIG_TARGET_LOCALE_USA */
	channel = info->adc_channel_main;
#endif /* CONFIG_TARGET_LOCALE_USA */
	ret = sec_bat_read_adc(info, channel, &adc_data, &adc_physical);
	if (ret)
		pr_err("%s : read error! (chan - %d)\n", __func__, channel);

	if (info->charging_enabled) {
		channel = CHANNEL_ADC_CHG_MONITOR;
		ret = sec_bat_read_adc(info, channel, &adc_data, &adc_physical);
		if (ret) {
			pr_err("%s : read error! (chan - %d)\n",
				__func__, channel);
		}
	} else {
		channel = CHANNEL_ADC_CHG_MONITOR;
		adc_data = 0;
		adc_physical = 0;
		ret = sec_bat_set_adc_depot(info, channel,
			&adc_data, &adc_physical);
		if (ret < 0) {
			pr_err("%s : set depot error! (chan - %d)\n",
				__func__, channel);
		}
	}

#if defined(CONFIG_TARGET_LOCALE_USA)
	channel = CHANNEL_ADC_BATT_ID;
	ret = sec_bat_read_adc(info, channel, &adc_data, &adc_physical);
	if (ret)
		pr_err("%s : read error! (chan - %d)\n", __func__, channel);
#endif

	local_irq_save(flags);
	info->wadc_alive++;
	if (info->wadc_alive > 500000)
		info->wadc_alive = 0;
	local_irq_restore(flags);

	wake_unlock(&info->adc_wake_lock);
}
#endif /* ADC_QUEUE_FEATURE */

#define SEC_BATTERY_ATTR(_name)		\
{					\
	.attr = { .name = #_name,	\
		  .mode = 0664 },	\
	.show = sec_bat_show_property,	\
	.store = sec_bat_store,		\
}

static struct device_attribute sec_battery_attrs[] = {
	SEC_BATTERY_ATTR(batt_vol),
	SEC_BATTERY_ATTR(batt_soc),
	SEC_BATTERY_ATTR(batt_vfocv),
	SEC_BATTERY_ATTR(batt_temp),
#if defined(CONFIG_USA_MODEL_SGH_I717)
	SEC_BATTERY_ATTR(batt_vol_adc_aver),
#endif
	SEC_BATTERY_ATTR(batt_temp_sub),
	SEC_BATTERY_ATTR(batt_temp_adc),
	SEC_BATTERY_ATTR(batt_temp_radc),
	SEC_BATTERY_ATTR(batt_temp_radc_sub),
	SEC_BATTERY_ATTR(batt_charging_source),
	SEC_BATTERY_ATTR(batt_lp_charging),
	SEC_BATTERY_ATTR(batt_type),
	SEC_BATTERY_ATTR(batt_full_check),
	SEC_BATTERY_ATTR(batt_temp_check),
	SEC_BATTERY_ATTR(batt_temp_adc_spec),
	SEC_BATTERY_ATTR(batt_test_value),
	SEC_BATTERY_ATTR(batt_current_adc),
	SEC_BATTERY_ATTR(batt_esus_test),
	SEC_BATTERY_ATTR(system_rev),
	SEC_BATTERY_ATTR(batt_read_raw_soc),
	SEC_BATTERY_ATTR(batt_lpm_state),
	SEC_BATTERY_ATTR(current_avg),
#ifdef ADC_QUEUE_FEATURE
	SEC_BATTERY_ATTR(wadc_alive),
#endif
#if !defined(CONFIG_TARGET_LOCALE_USA)
	SEC_BATTERY_ATTR(talk_wcdma),
	SEC_BATTERY_ATTR(talk_gsm),
	SEC_BATTERY_ATTR(data_call),
	SEC_BATTERY_ATTR(camera),
	SEC_BATTERY_ATTR(browser),
#endif

#if defined(CELOX_BATTERY_CHARGING_CONTROL)
	SEC_BATTERY_ATTR(batt_charging_enable),
#endif

#if defined(CONFIG_TARGET_LOCALE_USA)
	SEC_BATTERY_ATTR(camera),
	SEC_BATTERY_ATTR(music),
	SEC_BATTERY_ATTR(video),
	SEC_BATTERY_ATTR(talk_gsm),
	SEC_BATTERY_ATTR(talk_wcdma),
	SEC_BATTERY_ATTR(data_call),
	SEC_BATTERY_ATTR(batt_wifi),
	SEC_BATTERY_ATTR(gps),
	SEC_BATTERY_ATTR(device_state),
#endif
};

enum {
	BATT_VOL = 0,
	BATT_SOC,
	BATT_VFOCV,
	BATT_TEMP,
#if defined(CONFIG_USA_MODEL_SGH_I717)
	BATT_VOL_ADC_AVER,
#endif
	BATT_TEMP_SUB,
	BATT_TEMP_ADC,
	BATT_TEMP_RADC,
	BATT_TEMP_RADC_SUB,
	BATT_CHARGING_SOURCE,
	BATT_LP_CHARGING,
	BATT_TYPE,
	BATT_FULL_CHECK,
	BATT_TEMP_CHECK,
	BATT_TEMP_ADC_SPEC,
	BATT_TEST_VALUE,
	BATT_CURRENT_ADC,
	BATT_ESUS_TEST,
	BATT_SYSTEM_REV,
	BATT_READ_RAW_SOC,
	BATT_LPM_STATE,
	CURRENT_AVG,
#ifdef ADC_QUEUE_FEATURE
	BATT_WADC_ALIVE,
#endif
#if !defined(CONFIG_TARGET_LOCALE_USA)
	BATT_WCDMA_CALL,
	BATT_GSM_CALL,
	BATT_DATACALL,
	BATT_CAMERA,
	BATT_BROWSER,
#endif
#if defined(CELOX_BATTERY_CHARGING_CONTROL)
	BATT_CHARGING_ENABLE,
#endif
#if defined(CONFIG_TARGET_LOCALE_USA)
	BATT_CAMERA,
	BATT_MP3,
	BATT_VIDEO,
	BATT_VOICE_CALL_2G,
	BATT_VOICE_CALL_3G,
	BATT_DATA_CALL,
	BATT_WIFI,
	BATT_GPS,
	BATT_DEV_STATE,
#endif
};

#if defined(CONFIG_TARGET_LOCALE_USA)
static void sec_bat_set_compesation(struct sec_bat_info *info,
		int mode, int offset, int compensate_value)
{
	/*
	pr_info("[BAT]:%s\n", __func__);
	*/

	if (mode) {
		if (!(info->device_state & offset)) {
			info->device_state |= offset;
			/* batt_compensation += compensate_value; */
		}
	} else {
		if (info->device_state & offset) {
			info->device_state &= ~offset;

			/* later, may be used (if we need to compensate) */
			/* batt_compensation -= compensate_value; */
		}
	}

	/* later, may be used (to prevent ap sleep when calling) */
	/*
	is_calling_or_playing = s3c_bat_info.device_state;
	pr_info("[BAT]:%s: device_state=0x%x, compensation=%d\n",
		__func__, s3c_bat_info.device_state, batt_compensation);
	*/
}
#endif

static ssize_t sec_bat_show_property(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct sec_bat_info *info = dev_get_drvdata(dev->parent);
	int i = 0, val = 0, adc_physical = 0;
	const ptrdiff_t off = attr - sec_battery_attrs;

	switch (off) {
	case BATT_VOL:
		val = sec_bat_get_fuelgauge_data(info, FG_T_VCELL);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		break;
	case BATT_SOC:
		val = sec_bat_get_fuelgauge_data(info, FG_T_SOC);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		break;
	case BATT_VFOCV:
#ifdef ADC_QUEUE_FEATURE
		/*
		wake_lock(&info->adc_wake_lock);
		queue_work(info->adc_wqueue, &info->adc_work);
		*/
#endif
		/*
		val = sec_bat_get_fuelgauge_data(info, FG_T_VFOCV);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		*/
#if defined(CONFIG_TARGET_LOCALE_USA)
#ifdef ADC_QUEUE_FEATURE
		if (sec_bat_get_adc_depot(info, CHANNEL_ADC_BATT_ID,
			&val, &adc_physical) < 0) {
			pr_info("%s: get adc depot failed (chan - %d), "
				"return\n", __func__, CHANNEL_ADC_BATT_ID);
			return 0;
		}
#else /* ADC_QUEUE_FEATURE */
		sec_bat_read_adc(info, CHANNEL_ADC_BATT_ID,
			&val, &adc_physical);
#endif
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
#endif /* ADC_QUEUE_FEATURE */
		break;
	case BATT_TEMP:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", info->batt_temp);
		break;
#if defined(CONFIG_USA_MODEL_SGH_I717)
	case BATT_VOL_ADC_AVER:
		val = sec_bat_get_fuelgauge_data(info, FG_T_PSOC);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		break;
#endif
	case BATT_TEMP_SUB:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			info->batt_temp_sub);
		break;
	case BATT_TEMP_ADC:
#ifdef ADC_QUEUE_FEATURE
		/*
		wake_lock(&info->adc_wake_lock);
		queue_work(info->adc_wqueue, &info->adc_work);
		*/
#endif	/* ADC_QUEUE_FEATURE */
#if defined(CONFIG_TARGET_LOCALE_USA)
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
		if (get_hw_rev() >= 0x06) {
#elif defined(CONFIG_USA_MODEL_SGH_I727)
		if (get_hw_rev() >= 0x08) {
#elif defined(CONFIG_USA_MODEL_SGH_I717) || \
	defined(CONFIG_USA_MODEL_SGH_I757) || \
	defined(CONFIG_USA_MODEL_SGH_I577) || \
	defined(CONFIG_CAN_MODEL_SGH_I577R) || \
	defined(CONFIG_CAN_MODEL_SGH_I757M)
		if (true) {
#else
		if (true) {
#endif
#ifdef ADC_QUEUE_FEATURE
			if (sec_bat_get_adc_depot(info, CHANNEL_ADC_BATT_THERM,
				&val, &adc_physical) < 0) {
				pr_info("%s: get adc depot failed (chan - %d),"
					" return\n", __func__,
					CHANNEL_ADC_BATT_THERM);
				return 0;
			}
#else /* ADC_QUEUE_FEATURE */
			sec_bat_read_adc(info, CHANNEL_ADC_BATT_THERM,
				&val, &adc_physical);
#endif /* ADC_QUEUE_FEATURE */
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
			pr_info("%s: channel : %d, raw adc is %d,"
				" result is (temper.)\n",
				__func__, CHANNEL_ADC_BATT_THERM, val);
		} else {
#ifdef ADC_QUEUE_FEATURE
			if (sec_bat_get_adc_depot(info, CHANNEL_ADC_PMIC_THERM,
				&val, &adc_physical) < 0) {
				pr_info("%s: get adc depot failed (chan - %d),"
					" return\n",
					__func__, CHANNEL_ADC_PMIC_THERM);
				return 0;
			}
#else /* ADC_QUEUE_FEATURE */
			sec_bat_read_adc(info, CHANNEL_ADC_PMIC_THERM,
				&val, &adc_physical);
#endif /* ADC_QUEUE_FEATURE */
			i += scnprintf(buf + i, PAGE_SIZE - i,
				"%d\n", val/1000);
		}
#else /* CONFIG_TARGET_LOCALE_USA */
#ifdef ADC_QUEUE_FEATURE
		if (sec_bat_get_adc_depot(info, info->adc_channel_main,
			&val, &adc_physical) < 0) {
			pr_info("%s: get adc depot failed (chan - %d), "
				"return\n", __func__, info->adc_channel_main);
			return 0;
		}
#else /* ADC_QUEUE_FEATURE */
		sec_bat_read_adc(info, info->adc_channel_main,
			&val, &adc_physical);
#endif /* ADC_QUEUE_FEATURE */
		info->batt_temp_adc = val;
		if (sec_bat_rescale_adcvalue(info, info->adc_channel_main,
			val, adc_physical) < 0) {
			pr_info("%s: main temper. rescale failed,"
				" return\n", __func__);
			val = -1;
		}
		i += scnprintf(buf + i, PAGE_SIZE - i,
			"%d\n", info->batt_temp_adc);
#endif /* CONFIG_TARGET_LOCALE_USA */
		break;
	case BATT_TEMP_RADC:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			info->batt_temp_radc);
		break;
	case BATT_TEMP_RADC_SUB:
		/*
		sec_bat_read_adc(info, info->adc_channel_sub,
		&val, &adc_physical);
		if (sec_bat_rescale_adcvalue(info, info->adc_channel_sub,
			val, adc_physical) < 0) {
			printk("%s: sub temper. rescale failed,"
				" return\n", __func__);
			val = -1;
		}
		*/
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			info->batt_temp_radc_sub);
		break;
	case BATT_CHARGING_SOURCE:
		val = info->cable_type;
		/* for lpm test */
		/* val = 2; */
		if (info->lpm_chg_mode &&
			info->cable_type != CABLE_TYPE_NONE &&
			info->charging_status ==
				POWER_SUPPLY_STATUS_DISCHARGING) {
			val = CABLE_TYPE_NONE;
		}
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		break;
	case BATT_LP_CHARGING:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       info->get_lpcharging_state());
		break;
	case BATT_TYPE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", "SDI_SDI");
		break;
	case BATT_FULL_CHECK:
		/* new concept : in case of time-out charging stop,
		  Do not update FULL for UI,
		  Use same time-out value for first charing and re-charging
		*/
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(info->is_timeout_chgstop == false &&
			 info->charging_status == POWER_SUPPLY_STATUS_FULL)
			 ? 1 : 0);
		break;
	case BATT_TEMP_CHECK:
		i += scnprintf(buf + i, PAGE_SIZE - i,
			"%d\n", info->batt_health);
		break;
	case BATT_TEMP_ADC_SPEC:
#if !defined(CONFIG_TARGET_LOCALE_USA)
		i += scnprintf(buf + i, PAGE_SIZE - i,
			"(HIGH: %d / %d,   LOW: %d / %d)\n",
			info->temper_spec.high_block,
			info->temper_spec.high_recovery,
			info->temper_spec.low_block,
			info->temper_spec.low_recovery);
#else
		i += scnprintf(buf + i, PAGE_SIZE - i,
			"(HIGH: %d / %d,   LOW: %d / %d)\n",
			HIGH_BLOCK_TEMP_ADC, HIGH_RECOVER_TEMP_ADC,
			LOW_BLOCK_TEMP_ADC, LOW_RECOVER_TEMP_ADC);
#endif
		break;
	case BATT_TEST_VALUE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			info->test_info.test_value);
		break;
	case BATT_CURRENT_ADC:
		check_chgcurrent(info);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			info->batt_current_adc);
		break;
	case BATT_ESUS_TEST:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			info->test_info.test_esuspend);
		break;
	case BATT_SYSTEM_REV:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", info->hw_rev);
		break;
	case BATT_READ_RAW_SOC:
		val = sec_bat_get_fuelgauge_data(info, FG_T_PSOC);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", val);
		break;
	case BATT_LPM_STATE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			info->batt_lpm_state);
		break;
	case CURRENT_AVG:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", 0);
		break;
#if defined(CONFIG_TARGET_LOCALE_USA)
	case BATT_DEV_STATE:
		i += scnprintf(buf + i, PAGE_SIZE - i,
			"0x%08x\n", info->device_state);
			break;
#endif
#if defined(CELOX_BATTERY_CHARGING_CONTROL)
	case BATT_CHARGING_ENABLE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			(info->charging_enabled) ? 1 : 0);
		break;
#endif
#ifdef ADC_QUEUE_FEATURE
	case BATT_WADC_ALIVE:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d-%d(%d-%d)\n",
			info->wadc_alive, info->wadc_alive_prev,
			info->wadc_freezed_count, info->is_adc_wq_freezed);
		break;
#endif
	default:
		i = -EINVAL;
	}

	return i;
}

static ssize_t sec_bat_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int ret = 0, x = 0;
	const ptrdiff_t off = attr - sec_battery_attrs;
	struct sec_bat_info *info = dev_get_drvdata(dev->parent);

	switch (off) {
	case BATT_ESUS_TEST: /* early_suspend test */
		if (sscanf(buf, "%d\n", &x) == 1) {
			if (x == 0) {
				info->test_info.test_esuspend = 0;
				wake_lock_timeout(&info->test_wake_lock, 5*HZ);
				cancel_delayed_work(&info->measure_work);
				info->measure_interval = MEASURE_DSG_INTERVAL;
				wake_lock(&info->measure_wake_lock);
				queue_delayed_work(info->monitor_wqueue,
					&info->measure_work, 0);
				/* schedule_delayed_work(
					&info->measure_work, 0); */
			} else {
				info->test_info.test_esuspend = 1;
				wake_lock(&info->test_wake_lock);
				cancel_delayed_work(&info->measure_work);
				info->measure_interval = MEASURE_CHG_INTERVAL;
				wake_lock(&info->measure_wake_lock);
				queue_delayed_work(info->monitor_wqueue,
					&info->measure_work, 0);
				/* schedule_delayed_work(
					&info->measure_work, 0); */
			}
			ret = count;
		}
		break;
	case BATT_TEST_VALUE:
		if (sscanf(buf, "%d\n", &x) == 1) {
			if (x == 0)
				info->test_info.test_value = 0;
			else if (x == 1) /* for temp warning event */
				info->test_info.test_value = 1;
			else if (x == 2) /* for full event */
				/* info->test_info.test_value = 2; */
				/* disable full test interface. */
				info->test_info.test_value = 0;
			else if (x == 3) /* for abs time event */
				info->test_info.test_value = 3;
			else if (x == 999) { /* for pop-up disable */
				info->test_info.test_value = 999;
				if ((info->batt_health ==
					POWER_SUPPLY_HEALTH_OVERHEAT) ||
					(info->batt_health ==
						POWER_SUPPLY_HEALTH_COLD)) {
					info->batt_health =
						POWER_SUPPLY_HEALTH_GOOD;
					wake_lock(&info->monitor_wake_lock);
					queue_work(info->monitor_wqueue,
						&info->monitor_work);
				}
			} else
				info->test_info.test_value = 0;
			pr_info("%s : test case : %d\n", __func__,
				info->test_info.test_value);
			ret = count;
		}
		break;
	case BATT_LPM_STATE:
		if (sscanf(buf, "%d\n", &x) == 1) {
			info->batt_lpm_state = x;
			ret = count;
		}
		break;
#if !defined(CONFIG_TARGET_LOCALE_USA)
	case BATT_WCDMA_CALL:
	case BATT_GSM_CALL:
		if (sscanf(buf, "%d\n", &x) == 1) {
			info->voice_call_state = x;
			pr_info("%s : voice call = %d, %d\n", __func__,
				x, info->voice_call_state);
		}
		break;
	case BATT_DATACALL:
		if (sscanf(buf, "%d\n", &x) == 1)
			dev_dbg(info->dev,
				"%s : data call = %d\n", __func__, x);
		break;
	case BATT_CAMERA:
		if (sscanf(buf, "%d\n", &x) == 1)
			ret = count;
		break;
	case BATT_BROWSER:
		if (sscanf(buf, "%d\n", &x) == 1)
			ret = count;
		break;
#endif
#if defined(CELOX_BATTERY_CHARGING_CONTROL)
	case BATT_CHARGING_ENABLE:
		pr_info("%s : BATT_CHARGING_ENABLE buf=[%s]\n", __func__, buf);
		if (sscanf(buf, "%d\n", &x) == 1) {
			if (x == 0) {
				is_charging_disabled = 1;
				info->cable_type = CABLE_TYPE_NONE;
				wake_lock(&info->cable_wake_lock);
				queue_delayed_work(info->monitor_wqueue,
					&info->cable_work, 0);
			} else if (x == 1) {
				is_charging_disabled = 0;
				info->cable_type = CABLE_TYPE_UNKNOWN;
				wake_lock(&info->cable_wake_lock);
				queue_delayed_work(info->monitor_wqueue,
					&info->cable_work, 0);
			} else {
				pr_info("%s : ****ERR BATT_CHARGING_ENABLE :"
					" Invalid Input\n", __func__);
			}

			ret = 1;
		}
		break;
#endif
#if defined(CONFIG_TARGET_LOCALE_USA)
	case BATT_CAMERA:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_set_compesation(info, x,
				OFFSET_CAMERA_ON, COMPENSATE_CAMERA);
			ret = count;
		}
		/*
		pr_info("[BAT]:%s: camera = %d\n", __func__, x);
		*/
		break;
	case BATT_MP3:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_set_compesation(info, x,
				OFFSET_MP3_PLAY, COMPENSATE_MP3);
			ret = count;
		}
		/*
		pr_info("[BAT]:%s: mp3 = %d\n", __func__, x);
		*/
		break;
	case BATT_VIDEO:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_set_compesation(info, x,
				OFFSET_VIDEO_PLAY, COMPENSATE_VIDEO);
			ret = count;
		}
		/*
		pr_info("[BAT]:%s: video = %d\n", __func__, x);
		*/
		break;
	case BATT_VOICE_CALL_2G:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_set_compesation(info, x,
				OFFSET_VOICE_CALL_2G,
				COMPENSATE_VOICE_CALL_2G);
			ret = count;
		}
		/*
		pr_info("[BAT]:%s: voice call 2G = %d\n", __func__, x);
		*/
		break;
	case BATT_VOICE_CALL_3G:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_set_compesation(info, x,
				OFFSET_VOICE_CALL_3G,
				COMPENSATE_VOICE_CALL_3G);
			ret = count;
		}
		/*
		pr_info("[BAT]:%s: voice call 3G = %d\n", __func__, x);
		*/
		break;
	case BATT_DATA_CALL:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_set_compesation(info, x,
				OFFSET_DATA_CALL, COMPENSATE_DATA_CALL);
			ret = count;
		}
		/*
		pr_info("[BAT]:%s: data call = %d\n", __func__, x);
		*/
		break;
	case BATT_WIFI:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_set_compesation(info, x,
				OFFSET_WIFI, COMPENSATE_WIFI);
			ret = count;
		}
		/*
		pr_info("[BAT]:%s: wifi = %d\n", __func__, x);
		*/
		break;
	case BATT_GPS:
		if (sscanf(buf, "%d\n", &x) == 1) {
			sec_bat_set_compesation(info, x,
				OFFSET_GPS, COMPENSATE_GPS);
			ret = count;
		}
		/*
		pr_info("[BAT]:%s: gps = %d\n", __func__, x);
		*/
		break;
#endif

	default:
		ret = -EINVAL;
	}

	return ret;
}

static int sec_bat_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(sec_battery_attrs); i++) {
		rc = device_create_file(dev, &sec_battery_attrs[i]);
		if (rc)
			goto sec_attrs_failed;
	}
	goto succeed;

sec_attrs_failed:
	while (i--)
		device_remove_file(dev, &sec_battery_attrs[i]);
succeed:
	return rc;
}

static int sec_bat_read_proc(char *buf, char **start,
			off_t offset, int count, int *eof, void *data)
{
	struct sec_bat_info *info = data;
	struct timespec cur_time;
	ktime_t ktime;
	int len = 0;
	/* Guess we need no more than 100 bytes. */
	int size = 100;

	ktime = alarm_get_elapsed_realtime();
	cur_time = ktime_to_timespec(ktime);

	len = snprintf(buf, size,
		"%lu\t%u\t%u\t%u\t%u\t%d\t%u\t%d\t%d\t%d\t%u\t%u\t"
		"%u\t%d\t%u\t%u\t0x%04x\t%u\t%lu\n",
		cur_time.tv_sec,
		info->batt_raw_soc,
		info->batt_soc,
		info->batt_vcell,
		info->batt_current_adc,
		info->charging_enabled,
		info->batt_full_status,
		info->test_info.full_count,
		info->test_info.rechg_count,
		info->test_info.is_rechg_state,
		info->recharging_status,
		info->batt_temp_radc,
		info->batt_health,
		info->charging_status,
		info->present,
		info->cable_type,
		info->batt_rcomp,
		info->batt_full_soc,
		info->charging_passed_time);
	return len;
}

static void sec_bat_early_suspend(struct early_suspend *handle)
{
	struct sec_bat_info *info = container_of(handle, struct sec_bat_info,
						 bat_early_suspend);

	pr_info("%s...\n", __func__);
	info->is_esus_state = true;

	return;
}

static void sec_bat_late_resume(struct early_suspend *handle)
{
	struct sec_bat_info *info = container_of(handle, struct sec_bat_info,
						 bat_early_suspend);

	pr_info("%s...\n", __func__);
	info->is_esus_state = false;

	return;
}

static __devinit int sec_bat_probe(struct platform_device *pdev)
{
	struct sec_bat_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct sec_bat_info *info;
	int ret = 0;

	dev_info(&pdev->dev, "%s: SEC Battery Driver Loading\n", __func__);

#if defined(CELOX_BATTERY_CHARGING_CONTROL)
	is_charging_disabled = 0;
#endif

	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	platform_set_drvdata(pdev, info);

	info->dev = &pdev->dev;
	if (!pdata->fuel_gauge_name || !pdata->charger_name) {
		dev_err(info->dev, "%s: no fuel gauge or charger name\n",
			__func__);
		goto err_kfree;
	}
	info->fuel_gauge_name = pdata->fuel_gauge_name;
	info->charger_name = pdata->charger_name;

/*
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || \
	defined(CONFIG_KOR_MODEL_SHV_E160K) || \
	defined(CONFIG_KOR_MODEL_SHV_E160L)
	if (!pdata->charger_name_old) {
		dev_err(info->dev, "%s: no old charger name\n",
			__func__);
		goto err_kfree;
	}
#endif
*/

	info->hw_rev = get_hw_rev();
/*
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || \
	defined(CONFIG_KOR_MODEL_SHV_E160K) || \
	defined(CONFIG_KOR_MODEL_SHV_E160L)
	if (info->hw_rev < 0x04) {
		info->charger_name = pdata->charger_name_old;
	}
#endif
*/
	info->chg_shutdown_cb = pdata->chg_shutdown_cb;
	info->get_lpcharging_state = pdata->get_lpcharging_state;
	info->present = 1;

	info->psy_bat.name = "battery",
	info->psy_bat.type = POWER_SUPPLY_TYPE_BATTERY,
	info->psy_bat.properties = sec_battery_props,
	info->psy_bat.num_properties = ARRAY_SIZE(sec_battery_props),
	info->psy_bat.get_property = sec_bat_get_property,
	info->psy_bat.set_property = sec_bat_set_property,
	info->psy_usb.name = "usb",
	info->psy_usb.type = POWER_SUPPLY_TYPE_USB,
	info->psy_usb.supplied_to = supply_list,
	info->psy_usb.num_supplicants = ARRAY_SIZE(supply_list),
	info->psy_usb.properties = sec_power_props,
	info->psy_usb.num_properties = ARRAY_SIZE(sec_power_props),
	info->psy_usb.get_property = sec_usb_get_property,
	info->psy_ac.name = "ac",
	info->psy_ac.type = POWER_SUPPLY_TYPE_MAINS,
	info->psy_ac.supplied_to = supply_list,
	info->psy_ac.num_supplicants = ARRAY_SIZE(supply_list),
	info->psy_ac.properties = sec_power_props,
	info->psy_ac.num_properties = ARRAY_SIZE(sec_power_props),
	info->psy_ac.get_property = sec_ac_get_property;

	wake_lock_init(&info->vbus_wake_lock, WAKE_LOCK_SUSPEND,
		       "vbus_present");
	wake_lock_init(&info->monitor_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec-battery-monitor");
	wake_lock_init(&info->cable_wake_lock, WAKE_LOCK_SUSPEND,
		       "sec-battery-cable");
	wake_lock_init(&info->test_wake_lock, WAKE_LOCK_SUSPEND,
				"bat_esus_test");
	wake_lock_init(&info->measure_wake_lock, WAKE_LOCK_SUSPEND,
				"sec-battery-measure");
#ifdef ADC_QUEUE_FEATURE
	wake_lock_init(&info->adc_wake_lock, WAKE_LOCK_SUSPEND,
				"sec-battery-adc");
#endif

	info->batt_health = POWER_SUPPLY_HEALTH_GOOD;
	info->charging_status = sec_bat_is_charging(info);
	if (info->charging_status < 0)
		info->charging_status = POWER_SUPPLY_STATUS_DISCHARGING;
	else if (info->charging_status == POWER_SUPPLY_STATUS_CHARGING)
		info->cable_type = CABLE_TYPE_UNKNOWN; /* default */
	info->batt_soc = 100;
	info->batt_temp = RCOMP0_TEMP*10;
	info->recharging_status = false;
	info->is_timeout_chgstop = false;
	info->is_esus_state = false;
	info->is_rechg_triggered = false;
	info->dcin_intr_triggered = false;
	info->is_adc_ok = false;
#if defined(CONFIG_TARGET_LOCALE_USA)
	info->ui_full_charge_status = false;
#endif
	info->present = 1;
	info->initial_check_count = INIT_CHECK_COUNT;

	if ((pdata->hwrev_has_2nd_therm >= 0) &&
		(info->hw_rev >= pdata->hwrev_has_2nd_therm)) {
		info->has_sub_therm = true;
		info->adc_channel_main = CHANNEL_ADC_BATT_THERM;
		info->adc_channel_sub = CHANNEL_ADC_PMIC_THERM;
		info->temper_spec.high_block = HIGH_BLOCK_TEMP_ADC_SETTHERM;
		info->temper_spec.high_recovery =
			HIGH_RECOVER_TEMP_ADC_SETTHERM;
		info->temper_spec.low_block = LOW_BLOCK_TEMP_ADC_SETTHERM;
		info->temper_spec.low_recovery = LOW_RECOVER_TEMP_ADC_SETTHERM;
	} else {
		info->has_sub_therm = false;
		info->adc_channel_main = info->adc_channel_sub =
			CHANNEL_ADC_PMIC_THERM;
		info->temper_spec.high_block = HIGH_BLOCK_TEMP_ADC_PMICTHERM;
		info->temper_spec.high_recovery =
			HIGH_RECOVER_TEMP_ADC_PMICTHERM;
		info->temper_spec.low_block = LOW_BLOCK_TEMP_ADC_PMICTHERM;
		info->temper_spec.low_recovery = LOW_RECOVER_TEMP_ADC_PMICTHERM;
	}

#if defined(ADC_QUEUE_FEATURE) || defined(PRE_CHANOPEN_FEATURE)
	info->batt_adc_chan[0].id = CHANNEL_ADC_BATT_THERM;
	info->batt_adc_chan[1].id = CHANNEL_ADC_CHG_MONITOR;
	info->batt_adc_chan[2].id = CHANNEL_ADC_BATT_ID;
	info->batt_adc_chan[3].id = CHANNEL_ADC_PMIC_THERM;
#endif

#ifdef PRE_CHANOPEN_FEATURE
	/* adc channel open */
	info->batt_adc_chan[0].adc_handle = NULL;
	ret = adc_channel_open(CHANNEL_ADC_BATT_THERM,
		&(info->batt_adc_chan[0].adc_handle));
	if (ret < 0) {
		pr_err("%s: adc_channel_open() failed. (chan 0)\n", __func__);
		goto err_adc_open_0;
	}
	info->batt_adc_chan[1].adc_handle = NULL;
	ret = adc_channel_open(CHANNEL_ADC_CHG_MONITOR,
		&(info->batt_adc_chan[1].adc_handle));
	if (ret < 0) {
		pr_err("%s: adc_channel_open() failed. (chan 1)\n", __func__);
		goto err_adc_open_1;
	}
	info->batt_adc_chan[2].adc_handle = NULL;
	ret = adc_channel_open(CHANNEL_ADC_BATT_ID,
		&(info->batt_adc_chan[2].adc_handle));
	if (ret < 0) {
		pr_err("%s: adc_channel_open() failed. (chan 2)\n", __func__);
		goto err_adc_open_2;
	}
	info->batt_adc_chan[3].adc_handle = NULL;
	ret = adc_channel_open(CHANNEL_ADC_PMIC_THERM,
		&(info->batt_adc_chan[3].adc_handle));
	if (ret < 0) {
		pr_err("%s: adc_channel_open() failed. (chan 3)\n", __func__);
		goto err_adc_open_3;
	}
#endif

	info->charging_start_time = 0;

	if (info->get_lpcharging_state) {
		if (info->get_lpcharging_state()) {
			info->polling_interval = POLLING_INTERVAL / 4;
			info->lpm_chg_mode = true;
		} else {
			info->polling_interval = POLLING_INTERVAL;
			info->lpm_chg_mode = false;
		}
	}

	if (info->charging_status == POWER_SUPPLY_STATUS_CHARGING)
		info->measure_interval = MEASURE_CHG_INTERVAL;
	else
		info->measure_interval = MEASURE_DSG_INTERVAL;

#ifdef MPP_CHECK_FEATURE
	if (pdata->mpp_cblpwr_config)
		ret = pdata->mpp_cblpwr_config();
	if (ret) {
		dev_err(info->dev, "%s mpp_cblpwr_config failed ret=%d\n",
			__func__, ret);
		goto err_wake_lock;
	}
	info->mpp_get_cblpwr = pdata->mpp_get_cblpwr;

	ret = gpio_request(pdata->mpp_get_cblpwr, "mpp_cblpwr_state");
	if (ret) {
		dev_err(info->dev, "%s gpio_request failed for %d ret=%d\n",
			__func__, pdata->mpp_get_cblpwr, ret);
		goto err_wake_lock;
	}
#endif

	/* init power supplier framework */
	ret = power_supply_register(&pdev->dev, &info->psy_bat);
	if (ret) {
		dev_err(info->dev, "%s: failed to register psy_bat\n",
			__func__);
		goto err_gpio_free;
	}

	ret = power_supply_register(&pdev->dev, &info->psy_usb);
	if (ret) {
		dev_err(info->dev, "%s: failed to register psy_usb\n",
			__func__);
		goto err_supply_unreg_bat;
	}

	ret = power_supply_register(&pdev->dev, &info->psy_ac);
	if (ret) {
		dev_err(info->dev, "%s: failed to register psy_ac\n", __func__);
		goto err_supply_unreg_usb;
	}

	/* create sec detail attributes */
	sec_bat_create_attrs(info->psy_bat.dev);

	info->entry = create_proc_entry("batt_info_proc", S_IRUGO, NULL);
	if (!info->entry)
		dev_err(info->dev, "%s: failed to create proc_entry\n",
			__func__);
	else {
		info->entry->read_proc = sec_bat_read_proc;
		info->entry->data = (struct sec_bat_info *)info;
	}

	info->monitor_wqueue =
	    create_freezable_workqueue(dev_name(&pdev->dev));
	if (!info->monitor_wqueue) {
		dev_err(info->dev, "%s: fail to create workqueue\n", __func__);
		goto err_supply_unreg_ac;
	}
#ifdef ADC_QUEUE_FEATURE
	info->adc_wqueue =
	    create_freezable_workqueue("batt_adc_update");
	if (!info->adc_wqueue) {
		dev_err(info->dev,
			"%s: fail to create workqueue adc\n", __func__);
		goto err_supply_unreg_ac;
	}
#endif

	info->bat_early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	info->bat_early_suspend.suspend = sec_bat_early_suspend;
	info->bat_early_suspend.resume = sec_bat_late_resume;
	register_early_suspend(&info->bat_early_suspend);

	/* for lpm test */
	/*
	wake_lock(&info->test_wake_lock);
	*/

	INIT_WORK(&info->monitor_work, sec_bat_monitor_work);
	INIT_DELAYED_WORK_DEFERRABLE(&info->cable_work, sec_bat_cable_work);

#ifdef ADC_QUEUE_FEATURE
	INIT_WORK(&info->adc_work, sec_bat_adc_work);
	queue_work(info->adc_wqueue, &info->adc_work);
#endif

	INIT_DELAYED_WORK_DEFERRABLE(&info->polling_work, sec_bat_polling_work);
	schedule_delayed_work(&info->polling_work, 0);

	INIT_DELAYED_WORK_DEFERRABLE(&info->measure_work, sec_bat_measure_work);
	queue_delayed_work(info->monitor_wqueue, &info->measure_work, 0);

	INIT_DELAYED_WORK_DEFERRABLE(&info->otg_work, sec_otg_work);

	return 0;

err_supply_unreg_ac:
	power_supply_unregister(&info->psy_ac);
err_supply_unreg_usb:
	power_supply_unregister(&info->psy_usb);
err_supply_unreg_bat:
	power_supply_unregister(&info->psy_bat);
err_gpio_free:
#ifdef MPP_CHECK_FEATURE
	gpio_free(info->mpp_get_cblpwr);
err_wake_lock:
#endif
#ifdef PRE_CHANOPEN_FEATURE
	adc_channel_close(info->batt_adc_chan[3].adc_handle);
err_adc_open_3:
	adc_channel_close(info->batt_adc_chan[2].adc_handle);
err_adc_open_2:
	adc_channel_close(info->batt_adc_chan[1].adc_handle);
err_adc_open_1:
	adc_channel_close(info->batt_adc_chan[0].adc_handle);
err_adc_open_0:
#endif
	wake_lock_destroy(&info->vbus_wake_lock);
	wake_lock_destroy(&info->monitor_wake_lock);
	wake_lock_destroy(&info->cable_wake_lock);
	wake_lock_destroy(&info->test_wake_lock);
	wake_lock_destroy(&info->measure_wake_lock);
#ifdef ADC_QUEUE_FEATURE
	wake_lock_destroy(&info->adc_wake_lock);
#endif
err_kfree:
	kfree(info);

	return ret;
}

static int __devexit sec_bat_remove(struct platform_device *pdev)
{
	struct sec_bat_info *info = platform_get_drvdata(pdev);

	remove_proc_entry("batt_info_proc", NULL);

	flush_workqueue(info->monitor_wqueue);
	destroy_workqueue(info->monitor_wqueue);

#ifdef ADC_QUEUE_FEATURE
	flush_workqueue(info->adc_wqueue);
	destroy_workqueue(info->adc_wqueue);
#endif

	cancel_delayed_work(&info->cable_work);
	cancel_delayed_work(&info->polling_work);
	cancel_delayed_work(&info->measure_work);
	cancel_delayed_work(&info->otg_work);

	power_supply_unregister(&info->psy_bat);
	power_supply_unregister(&info->psy_usb);
	power_supply_unregister(&info->psy_ac);

#ifdef MPP_CHECK_FEATURE
	gpio_free(info->mpp_get_cblpwr);
#endif

#ifdef PRE_CHANOPEN_FEATURE
	adc_channel_close(info->batt_adc_chan[3].adc_handle);
	adc_channel_close(info->batt_adc_chan[2].adc_handle);
	adc_channel_close(info->batt_adc_chan[1].adc_handle);
	adc_channel_close(info->batt_adc_chan[0].adc_handle);
#endif

	wake_lock_destroy(&info->vbus_wake_lock);
	wake_lock_destroy(&info->monitor_wake_lock);
	wake_lock_destroy(&info->cable_wake_lock);
	wake_lock_destroy(&info->test_wake_lock);
	wake_lock_destroy(&info->measure_wake_lock);
#ifdef ADC_QUEUE_FEATURE
	wake_lock_destroy(&info->adc_wake_lock);
#endif

	kfree(info);

	return 0;
}

static int sec_bat_suspend(struct device *dev)
{
	struct sec_bat_info *info = dev_get_drvdata(dev);

#ifdef ADC_QUEUE_FEATURE
	cancel_work_sync(&info->adc_work);
#endif
	cancel_work_sync(&info->monitor_work);
	cancel_delayed_work(&info->cable_work);
	cancel_delayed_work(&info->polling_work);
	cancel_delayed_work(&info->measure_work);
	cancel_delayed_work(&info->otg_work);

	return 0;
}

static int sec_bat_resume(struct device *dev)
{
	struct sec_bat_info *info = dev_get_drvdata(dev);

	queue_delayed_work(info->monitor_wqueue,
		&info->measure_work, 0);

	wake_lock(&info->monitor_wake_lock);
	queue_work(info->monitor_wqueue, &info->monitor_work);

	schedule_delayed_work(&info->polling_work,
			      3*HZ);

	return 0;
}

static void sec_bat_shutdown(struct device *dev)
{
	struct sec_bat_info *info = dev_get_drvdata(dev);

	if (info->chg_shutdown_cb)
		info->chg_shutdown_cb();
}

static const struct dev_pm_ops sec_bat_pm_ops = {
	.suspend = sec_bat_suspend,
	.resume = sec_bat_resume,
};

static struct platform_driver sec_bat_driver = {
	.driver = {
		   .name = "sec-battery",
		   .pm = &sec_bat_pm_ops,
		   .shutdown = sec_bat_shutdown,
		   },
	.probe = sec_bat_probe,
	.remove = __devexit_p(sec_bat_remove),
};

static int __init sec_bat_init(void)
{
	return platform_driver_register(&sec_bat_driver);
}

static void __exit sec_bat_exit(void)
{
	platform_driver_unregister(&sec_bat_driver);
}

late_initcall(sec_bat_init);
module_exit(sec_bat_exit);

MODULE_DESCRIPTION("SEC battery driver");
MODULE_AUTHOR("<jongmyeong.ko@samsung.com>");
MODULE_AUTHOR("<ms925.kim@samsung.com>");
MODULE_AUTHOR("<joshua.chang@samsung.com>");
MODULE_LICENSE("GPL");
