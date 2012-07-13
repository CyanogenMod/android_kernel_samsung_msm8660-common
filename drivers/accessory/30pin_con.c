

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/30pin_con.h>
#include <linux/switch.h>
#include <linux/wakelock.h>

#include <asm/irq.h>

#ifdef CONFIG_MHL_SII9234
#include "sii9234.h"
#endif

#ifdef CONFIG_SAMSUNG_8X60_TABLET
#define FEATURE_PM8058_ADC
#endif

#define SUBJECT "ACCESSORY"

#define ACC_CONDEV_DBG(format, ...) \
	pr_info("[ "SUBJECT " (%s,%d) ] " format "\n", \
		__func__, __LINE__, ## __VA_ARGS__);

#define DETECTION_INTR_DELAY	(get_jiffies_64() + (HZ*15)) /* 20s */

enum accessory_type {
	ACCESSORY_NONE = 0,
	ACCESSORY_OTG,
	ACCESSORY_LINEOUT,
	ACCESSORY_CARMOUNT,
	ACCESSORY_UNKNOWN,
};

enum dock_type {
	DOCK_NONE = 0,
	DOCK_DESK,
	DOCK_KEYBOARD,
};

enum uevent_dock_type {
	UEVENT_DOCK_NONE = 0,
	UEVENT_DOCK_DESK,
	UEVENT_DOCK_CAR,
	UEVENT_DOCK_KEYBOARD = 9,
};

struct acc_con_info {
	struct device *acc_dev;
	struct acc_con_platform_data *pdata;
	struct delayed_work acc_dwork;
	struct delayed_work acc_id_dwork;
	struct switch_dev dock_switch;
	struct switch_dev ear_jack_switch;
	struct wake_lock wake_lock;
	struct mutex lock;
	enum accessory_type current_accessory;
	enum dock_type current_dock;
	int accessory_irq;
	int dock_irq;
	int mhl_irq;
};


extern int hdmi_msm_hpd_switch(bool detect_flag);

struct acc_con_info *g_acc;
extern int64_t acc_get_adc_value(void);

static int check_using_stmpe811_adc(void)
{
	int ret =0 ;

#if defined(CONFIG_TARGET_LOCALE_KOR) || \
    defined(CONFIG_TARGET_LOCALE_JPN) || \
    defined(CONFIG_EUR_OPERATOR_OPEN) || \
    defined(CONFIG_JPN_OPERATOR_NTT)  || \
    defined(CONFIG_USA_OPERATOR_ATT)
	ret = (system_rev>=0x0003) ? 1 : 0;
#else
	ret = 0;
#endif

	return ret;
}

static int acc_get_accessory_id(void)
{
	int i;
	u32 adc = 0, adc_sum = 0;
	u32 adc_buff[5] = {0};
	u32 adc_val;
	u32 adc_min = 0;
	u32 adc_max = 0;

	for (i = 0; i < 5; i++) {
		/*change this reading ADC function  */
////		tps6586x_adc_read(&mili_volt, 0);

		adc_val=acc_get_adc_value();

		adc_buff[i] = adc_val;
		adc_sum += adc_buff[i];
		if (i == 0) {
			adc_min = adc_buff[0];
			adc_max = adc_buff[0];
		} else {
			if (adc_max < adc_buff[i])
				adc_max = adc_buff[i];
			else if (adc_min > adc_buff[i])
				adc_min = adc_buff[i];
		}
		msleep(20);
	}
	adc = (adc_sum - adc_max - adc_min)/3;
	ACC_CONDEV_DBG("ACCESSORY_ID : ADC value = %d\n", adc);
	return (int)adc;
}


extern int64_t acc_get_adc_value(void);

void acc_accessory_uevent(struct acc_con_info *acc, int acc_adc)
{
	enum accessory_type current_accessory = ACCESSORY_NONE;
	char *env_ptr;
	char *stat_ptr;
	char *envp[3];

// value is changed for PxLTE
// 3 pole earjack  1.00 V ( 0.90~1.10V)   adc: 797~1002
// Car mount        1.38 V (1.24~1.45V)   adc: 1134~1352
// 4 pole earjack   just bundles is supported . adc :1360~1449 : No Warranty
// OTG                 2.2 V  (2.00~2.35V)    adc: 1903~2248

	if (acc_adc != false) {
		if(check_using_stmpe811_adc()) {
			if ((1100 < acc_adc) && (1400 > acc_adc)) { 	// 3 pole earjack 1247
				env_ptr = "ACCESSORY=lineout";
				current_accessory = ACCESSORY_LINEOUT;
//				} else if ((1134 < acc_adc) && (1352 > acc_adc)) {	// car mount
//					env_ptr = "ACCESSORY=carmount";
//					acc->current_accessory = ACCESSORY_CARMOUNT;
			} else if ((1800 < acc_adc) && (2350 > acc_adc)) {	// 4 pole earjack, No warranty 1948
				env_ptr = "ACCESSORY=lineout";
				current_accessory = ACCESSORY_LINEOUT;
			} else if ((2400 < acc_adc) && (2800 > acc_adc)) {	// otg 2586
				env_ptr = "ACCESSORY=OTG";
				current_accessory = ACCESSORY_OTG;
			} else {
				env_ptr = "ACCESSORY=unknown";
				current_accessory = ACCESSORY_UNKNOWN;
			}
		} else { // not stmpe adc
			if ((797 < acc_adc) && (1002 > acc_adc)) {		// 3 pole earjack 911
				env_ptr = "ACCESSORY=lineout";
				current_accessory = ACCESSORY_LINEOUT;
			} else if ((1134 < acc_adc) && (1352 > acc_adc)) {	// car mount
				env_ptr = "ACCESSORY=carmount";
				current_accessory = ACCESSORY_CARMOUNT;
			} else if ((1400 < acc_adc) && (1690 > acc_adc)) {	// 4 pole earjack, No warranty 1534
				env_ptr = "ACCESSORY=lineout";
				current_accessory = ACCESSORY_LINEOUT;
			} else if ((1900 < acc_adc) && (2250 > acc_adc)) {	// otg 2125
				env_ptr = "ACCESSORY=OTG";
				current_accessory = ACCESSORY_OTG;
			} else {
				env_ptr = "ACCESSORY=unknown";
				current_accessory = ACCESSORY_UNKNOWN;
			}
		}

		if (current_accessory == acc->current_accessory) {
			ACC_CONDEV_DBG("same accessory type is connected %d", \
				current_accessory);
			return;
		}

		if (acc->current_accessory != ACCESSORY_NONE) {
			ACC_CONDEV_DBG("assuming prev accessory disconnected %d", \
				acc->current_accessory);

			if (acc->current_accessory == ACCESSORY_OTG)
				envp[0] = "ACCESSORY=OTG";
			else if (acc->current_accessory == ACCESSORY_LINEOUT)
				envp[0] = "ACCESSORY=lineout";
			else if (acc->current_accessory == ACCESSORY_CARMOUNT)
				envp[0] = "ACCESSORY=carmount";
			else
				envp[0] = "ACCESSORY=unknown";

			envp[1] = "STATE=offline";
			envp[2] = NULL;
			kobject_uevent_env(&acc->acc_dev->kobj, KOBJ_CHANGE, envp);
			if ((acc->current_accessory == ACCESSORY_OTG) &&
				acc->pdata->otg_en)
				acc->pdata->otg_en(0);
#if defined(CONFIG_TARGET_LOCALE_KOR) || defined(CONFIG_TARGET_LOCALE_JPN)
#else
			if (acc->current_accessory == ACCESSORY_LINEOUT)
				switch_set_state(&acc->ear_jack_switch, UEVENT_DOCK_NONE);
#endif

			ACC_CONDEV_DBG("%s : %s", envp[0], envp[1]);
		}

		acc->current_accessory = current_accessory;

		stat_ptr = "STATE=online";
		envp[0] = env_ptr;
		envp[1] = stat_ptr;
		envp[2] = NULL;

		if (acc->current_accessory == ACCESSORY_OTG) {
			/* force acc power off to ensure otg detection */
			if (acc->pdata->acc_power)
				acc->pdata->acc_power(0, false);
			msleep(20);

			if (acc->pdata->otg_en)
				acc->pdata->otg_en(1);
			msleep(30);
		} else if (acc->current_accessory == ACCESSORY_LINEOUT) {
#if defined(CONFIG_TARGET_LOCALE_KOR)  || defined(CONFIG_TARGET_LOCALE_JPN)
#else
			switch_set_state(&acc->ear_jack_switch, 1);
#endif
		}

		kobject_uevent_env(&acc->acc_dev->kobj, KOBJ_CHANGE, envp);
		ACC_CONDEV_DBG("%s : %s", env_ptr, stat_ptr);
	} else {
		if (acc->current_accessory == ACCESSORY_OTG)
			env_ptr = "ACCESSORY=OTG";
		else if (acc->current_accessory == ACCESSORY_LINEOUT) {
			env_ptr = "ACCESSORY=lineout";
#if defined(CONFIG_TARGET_LOCALE_KOR) || defined(CONFIG_TARGET_LOCALE_JPN)
#else
			switch_set_state(&acc->ear_jack_switch, UEVENT_DOCK_NONE);
#endif
		}
		else if (acc->current_accessory == ACCESSORY_CARMOUNT)
			env_ptr = "ACCESSORY=carmount";
		else
			env_ptr = "ACCESSORY=unknown";

		stat_ptr = "STATE=offline";
		envp[0] = env_ptr;
		envp[1] = stat_ptr;
		envp[2] = NULL;
		kobject_uevent_env(&acc->acc_dev->kobj, KOBJ_CHANGE, envp);
		if ((acc->current_accessory == ACCESSORY_OTG) &&
			acc->pdata->otg_en)
			acc->pdata->otg_en(0);

		acc->current_accessory = ACCESSORY_NONE;
		ACC_CONDEV_DBG("%s : %s", env_ptr, stat_ptr);
	}
}

static void acc_dock_uevent(struct acc_con_info *acc, bool connected)
{
	char *env_ptr;
	char *stat_ptr;
	char *envp[3];

	if (acc->current_dock == DOCK_KEYBOARD)
		env_ptr = "DOCK=keyboard";
	else if (acc->current_dock == DOCK_DESK)
		env_ptr = "DOCK=desk";
	else
		env_ptr = "DOCK=unknown";

	if (!connected) {
		stat_ptr = "STATE=offline";
		acc->current_dock = DOCK_NONE;
	} else {
		stat_ptr = "STATE=online";
	}

	envp[0] = env_ptr;
	envp[1] = stat_ptr;
	envp[2] = NULL;
	kobject_uevent_env(&acc->acc_dev->kobj, KOBJ_CHANGE, envp);
	ACC_CONDEV_DBG("%s : %s", env_ptr, stat_ptr);
}

static void acc_check_dock_detection(struct acc_con_info *acc)
{
	if (!acc->pdata->get_dock_state()) {
//		ACC_CONDEV_DBG("[30PIN] failed to get acc state!!!");

		wake_lock(&acc->wake_lock);

#ifdef CONFIG_SEC_KEYBOARD_DOCK
		if (acc->pdata->check_keyboard &&
			acc->pdata->check_keyboard(true)) {
			acc->current_dock = DOCK_KEYBOARD;
			ACC_CONDEV_DBG
			("[30PIN] keyboard dock station attached!!!");
			switch_set_state(&acc->dock_switch, 
				UEVENT_DOCK_KEYBOARD);
		} else
#endif
		{
			ACC_CONDEV_DBG
				("[30PIN] desktop dock station attached!!!");
			switch_set_state(&acc->dock_switch, UEVENT_DOCK_DESK);
			acc->current_dock = DOCK_DESK;
		}

#ifdef CONFIG_MHL_SII9234
		mutex_lock(&acc->lock);
		sii9234_tpi_init();
		hdmi_msm_hpd_switch(true);
		mutex_unlock(&acc->lock);
#endif
		acc_dock_uevent(acc, true);

		wake_unlock(&acc->wake_lock);

	} else {
		if (acc->current_dock == DOCK_NONE)
			return;
		ACC_CONDEV_DBG("docking station detached!!!");
		switch_set_state(&acc->dock_switch, UEVENT_DOCK_NONE);
#ifdef CONFIG_SEC_KEYBOARD_DOCK
		if (acc->pdata->check_keyboard)
			acc->pdata->check_keyboard(false);
#endif
#ifdef CONFIG_MHL_SII9234
		/*call MHL deinit */
		MHD_HW_Off();
		hdmi_msm_hpd_switch(false);
		/*TVout_LDO_ctrl(false); */
#endif
		acc_dock_uevent(acc, false);
	}


	
}

static irqreturn_t acc_dock_isr(int irq, void *ptr)
{
	struct acc_con_info *acc = ptr;

	ACC_CONDEV_DBG("");

	acc_check_dock_detection(acc);

	return IRQ_HANDLED;
}

#define DET_CHECK_TIME_MS 100
#define DET_SLEEP_TIME_MS 10
#define DETECTION_DELAY_MS	200

static irqreturn_t acc_accessory_isr(int irq, void *dev_id)
{
	struct acc_con_info *acc = (struct acc_con_info *)dev_id;
//	int acc_ID_val=0, pre_acc_ID_val=0;
//	int time_left_ms = DET_CHECK_TIME_MS;

	ACC_CONDEV_DBG("");
/*
	while (time_left_ms > 0){
		acc_ID_val = acc->pdata->get_acc_state();

		if (acc_ID_val == pre_acc_ID_val) {
			time_left_ms -= DET_SLEEP_TIME_MS;
		} else {
			time_left_ms = DET_CHECK_TIME_MS;
		}
		pre_acc_ID_val = acc_ID_val;
		msleep (DET_SLEEP_TIME_MS) ;
	}

	ACC_CONDEV_DBG("IRQ_DOCK_GPIO is %d", acc_ID_val);
	if (acc_ID_val == 1) {
		ACC_CONDEV_DBG("Accessory detached");
		acc_accessory_uevent(acc, false);
	} else
*/
	cancel_delayed_work_sync(&acc->acc_id_dwork);
		schedule_delayed_work(&acc->acc_id_dwork,
			msecs_to_jiffies(DETECTION_DELAY_MS));
	return IRQ_HANDLED;
}

static int acc_init_dock_int(struct acc_con_info *acc)
{
	int ret = 0;
	ACC_CONDEV_DBG("");

	gpio_request(acc->pdata->accessory_irq_gpio, "accessory");
	gpio_direction_input(acc->pdata->accessory_irq_gpio);
	acc->accessory_irq = gpio_to_irq(acc->pdata->accessory_irq_gpio);
	ret = request_threaded_irq(acc->accessory_irq, NULL, acc_dock_isr,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"accessory_detect", acc);
	if (ret)
		ACC_CONDEV_DBG("request_irq(accessory_irq) return : %d\n", ret);

	ret = enable_irq_wake(acc->accessory_irq);
	if (ret)
		ACC_CONDEV_DBG("enable_irq_wake(accessory_irq) return : %d\n", ret);
	return ret;
}

static int acc_init_accessory_int(struct acc_con_info *acc)
{
	int ret = 0;

	ACC_CONDEV_DBG("");

	ret = request_threaded_irq(acc->pdata->dock_irq, NULL, acc_accessory_isr,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			"dock_detect", acc);
	if (ret)
		ACC_CONDEV_DBG("request_irq(dock_irq) return : %d\n", ret);

	ret = enable_irq_wake(acc->pdata->dock_irq);
	if (ret)
		ACC_CONDEV_DBG("enable_irq_wake(dock_irq) return : %d\n", ret);

	return ret;
}

static void acc_dwork_int_init(struct work_struct *work)
{
	struct acc_con_info *acc = container_of(work,
		struct acc_con_info, acc_dwork.work);
	int retval;

	ACC_CONDEV_DBG("");

	retval = acc_init_dock_int(acc);
	if (retval) {
		ACC_CONDEV_DBG("failed to initialize dock_int irq");
		goto err_irq_dock;
	}

	retval = acc_init_accessory_int(acc);
	if (retval) {
		ACC_CONDEV_DBG(" failed to initialize accessory_int irq");
		goto err_irq_acc;
	}

	if (acc->pdata->get_dock_state) {
		if (!acc->pdata->get_dock_state())
			acc_check_dock_detection(acc);
	}

	if (acc->pdata->get_acc_state) {
		if (!acc->pdata->get_acc_state())
			schedule_delayed_work(&acc->acc_id_dwork,
				msecs_to_jiffies(DETECTION_DELAY_MS));
	}

	return ;

err_irq_acc:
	free_irq(acc->accessory_irq, acc);
err_irq_dock:
	switch_dev_unregister(&acc->ear_jack_switch);
	return ;
}

static void acc_dwork_accessory_detect(struct work_struct *work)
{
	struct acc_con_info *acc = container_of(work,
		struct acc_con_info, acc_id_dwork.work);

	int  adc_val=0;
	int acc_state = 0;

	acc_state = acc->pdata->get_acc_state();

	if(acc_state) {
		ACC_CONDEV_DBG("Accessory detached");
		acc_accessory_uevent(acc, false);
	} else {
		ACC_CONDEV_DBG("Accessory attached");
		adc_val = acc_get_accessory_id();
		ACC_CONDEV_DBG("adc_val : %d", adc_val);
		acc_accessory_uevent(acc, adc_val);
	}
}

#ifdef CONFIG_MHL_SII9234
static ssize_t MHD_check_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;
	int res;
//	TVout_LDO_ctrl(true);
	if(!MHD_HW_IsOn())
	{
		sii9234_tpi_init();
		res = MHD_Read_deviceID();
		MHD_HW_Off();
	}
	else
	{
		sii9234_tpi_init();
		res = MHD_Read_deviceID();
	}

	count = sprintf(buf,"%d\n", res );
//	TVout_LDO_ctrl(false);
	return count;
}

static ssize_t MHD_check_write(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	printk("input data --> %s\n", buf);

	return size;
}

static DEVICE_ATTR(MHD_file, S_IRUGO|S_IWUSR|S_IWGRP, MHD_check_read, MHD_check_write);
#endif

static int acc_con_probe(struct platform_device *pdev)
{
	struct acc_con_info *acc;
	struct acc_con_platform_data *pdata = pdev->dev.platform_data;
	int	retval;

	ACC_CONDEV_DBG("");

	if (pdata == NULL) {
		pr_err("%s: no pdata\n", __func__);
		return -ENODEV;
	}

	acc = kzalloc(sizeof(*acc), GFP_KERNEL);
	if (!acc)
		return -ENOMEM;

	acc->pdata = pdata;
	acc->current_dock = DOCK_NONE;
	acc->current_accessory = ACCESSORY_NONE;
	acc->mhl_irq = gpio_to_irq(pdata->mhl_irq_gpio);

#if 0 /* defined(CONFIG_HAS_EARLYSUSPEND) */
	acc->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	acc->early_suspend.suspend = acc_con_early_suspend;
	acc->early_suspend.resume = acc_con_late_resume;
	register_early_suspend(&acc->early_suspend);
	mutex_init(&acc->lock);
	INIT_DELAYED_WORK(&acc->acc_con_work, acc_con_late_resume_work);
#endif
	mutex_init(&acc->lock);

	dev_set_drvdata(&pdev->dev, acc);

	acc->acc_dev = &pdev->dev;
#ifdef CONFIG_MHL_SII9234
	retval = i2c_add_driver(&SII9234A_i2c_driver);
	if (retval) {
		pr_err("[MHL SII9234A] can't add i2c driver\n");
		goto err_i2c_a;
	} else {
		pr_info("[MHL SII9234A] add i2c driver\n");
	}

	retval = i2c_add_driver(&SII9234B_i2c_driver);
	if (retval) {
		pr_err("[MHL SII9234B] can't add i2c driver\n");
		goto err_i2c_b;
	} else {
		pr_info("[MHL SII9234B] add i2c driver\n");
	}

	retval = i2c_add_driver(&SII9234C_i2c_driver);
	if (retval) {
		pr_err("[MHL SII9234C] can't add i2c driver\n");
		goto err_i2c_c;
	} else {
		pr_info("[MHL SII9234C] add i2c driver\n");
	}

	retval = i2c_add_driver(&SII9234_i2c_driver);
	if (retval) {
		pr_err("[MHL SII9234] can't add i2c driver\n");
		goto err_i2c;
	} else {
		pr_info("[MHL SII9234] add i2c driver\n");
	}
#endif
	acc->dock_switch.name = "dock";
	retval = switch_dev_register(&acc->dock_switch);
	if (retval < 0)
		goto err_sw_dock;

	acc->ear_jack_switch.name = "usb_audio";
	retval = switch_dev_register(&acc->ear_jack_switch);
	if (retval < 0)
		goto err_sw_jack;

	wake_lock_init(&acc->wake_lock, WAKE_LOCK_SUSPEND, "30pin_con");
	INIT_DELAYED_WORK(&acc->acc_dwork, acc_dwork_int_init);
	schedule_delayed_work(&acc->acc_dwork, msecs_to_jiffies(10000));
	INIT_DELAYED_WORK(&acc->acc_id_dwork, acc_dwork_accessory_detect);

	g_acc = acc;

	if (device_create_file(acc->acc_dev, &dev_attr_MHD_file) < 0)		//hm0412 _build_error_debug
		printk("Failed to create device file(%s)!\n", dev_attr_MHD_file.attr.name);

	return 0;

err_sw_jack:
	switch_dev_unregister(&acc->dock_switch);
err_sw_dock:
	i2c_del_driver(&SII9234_i2c_driver);
err_i2c:
	i2c_del_driver(&SII9234C_i2c_driver);
err_i2c_c:
	i2c_del_driver(&SII9234B_i2c_driver);
err_i2c_b:
	i2c_del_driver(&SII9234A_i2c_driver);
err_i2c_a:

	kfree(acc);

	return retval;
}

static int acc_con_remove(struct platform_device *pdev)
{
	struct acc_con_info *acc = platform_get_drvdata(pdev);
	ACC_CONDEV_DBG("");
#ifdef CONFIG_MHL_SII9234
	i2c_del_driver(&SII9234A_i2c_driver);
	i2c_del_driver(&SII9234B_i2c_driver);
	i2c_del_driver(&SII9234C_i2c_driver);
	i2c_del_driver(&SII9234_i2c_driver);
#endif
	disable_irq_wake(acc->accessory_irq);
	disable_irq_wake(acc->dock_irq);
	free_irq(acc->accessory_irq, acc);
	free_irq(acc->dock_irq, acc);
	switch_dev_unregister(&acc->dock_switch);
	switch_dev_unregister(&acc->ear_jack_switch);
	kfree(acc);
	return 0;
}

static int acc_con_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct acc_con_info *acc = platform_get_drvdata(pdev);
	ACC_CONDEV_DBG("");
#ifdef CONFIG_MHL_SII9234
	if ((acc->current_dock == DOCK_DESK) || (acc->current_dock == DOCK_KEYBOARD))
		MHD_HW_Off();   /*call MHL deinit */
#endif
	return 0;
}

static int acc_con_resume(struct platform_device *pdev)
{
	struct acc_con_info *acc = platform_get_drvdata(pdev);
	ACC_CONDEV_DBG("");
#ifdef CONFIG_MHL_SII9234
	if ((acc->current_dock == DOCK_DESK) || (acc->current_dock == DOCK_KEYBOARD))
		sii9234_tpi_init();  /* call MHL init */
#endif
	return 0;
}

static struct platform_driver acc_con_driver = {
	.probe		= acc_con_probe,
	.remove		= acc_con_remove,
	.suspend	= acc_con_suspend,
	.resume		= acc_con_resume,
	.driver		= {
		.name		= "acc_con",
		.owner		= THIS_MODULE,
	},
};

static int __init acc_con_init(void)
{
	ACC_CONDEV_DBG("");

	return platform_driver_register(&acc_con_driver);
}

static void __exit acc_con_exit(void)
{
	platform_driver_unregister(&acc_con_driver);
}

late_initcall(acc_con_init);
module_exit(acc_con_exit);

MODULE_AUTHOR("Kyungrok Min <gyoungrok.min@samsung.com>");
MODULE_DESCRIPTION("acc connector driver");
MODULE_LICENSE("GPL");
