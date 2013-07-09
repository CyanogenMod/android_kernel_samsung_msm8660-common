/*
 * UART/USB path switching driver for Samsung Electronics devices.
 *
 * Copyright (C) 2010 Samsung Electronics.
 *
 * Authors: Ikkeun Kim <iks.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/i2c/fsa9480.h>
//#include <linux/mfd/max8998.h>
#include <linux/regulator/consumer.h>
#include <linux/moduleparam.h>
#include <linux/usb/android_composite.h>
#include <asm/mach/arch.h>
//#include <mach/param.h>
#include <mach/gpio.h>
//#include <mach/gpio-p1.h>
#include <mach/sec_switch.h>
#include <linux/kdev_t.h>
//#include <mach/regs-clock.h>
//#include <mach/regs-gpio.h>
//#include <plat/gpio-cfg.h>
#ifdef CONFIG_SEC_MISC
#include <linux/sec_param.h>
#endif

#define _SEC_DM_

#define USB_SEL_MASK (1<<0)
#define UART_SEL_MASK (1<<1)

struct sec_switch_struct {
	struct sec_switch_platform_data *pdata;
	int switch_sel;
};

struct sec_switch_wq {
	struct delayed_work work_q;
	struct sec_switch_struct *sdata;
	struct list_head entry;
};

#ifdef _SEC_DM_
struct device *usb_lock;

extern struct class *sec_class;
extern int usb_access_lock;
#endif

/* for sysfs control (/sys/class/sec/switch/) */
extern struct device *switch_dev;
extern int cp_boot_ok;

extern bool charging_mode_get(void);
static void usb_switch_mode(struct sec_switch_struct *secsw, int mode)
{
	if (mode == SWITCH_PDA)	{
		if (secsw->pdata && secsw->pdata->set_vbus_status)
			secsw->pdata->set_vbus_status((u8)USB_VBUS_AP_ON);
		mdelay(10);
#ifdef CONFIG_TARGET_LOCALE_US_ATT_REV01
		gpio_set_value(USB_SEL_SW,0);
		gpio_set_value(USB_MDM_EN,0);
#else
		fsa9480_manual_switching(SWITCH_PORT_AUTO);
#endif		
	} else {
		if (secsw->pdata && secsw->pdata->set_vbus_status)
			secsw->pdata->set_vbus_status((u8)USB_VBUS_CP_ON);
		mdelay(10);		
#ifdef CONFIG_TARGET_LOCALE_US_ATT_REV01
		gpio_set_value(USB_SEL_SW,1);
		gpio_set_value(USB_MDM_EN,1);
#else
		fsa9480_manual_switching(SWITCH_PORT_AUTO);
#endif
	}
}

#if 1
static void uart_switch_mode(struct sec_switch_struct *secsw, int mode)
{
	if (mode == SWITCH_PDA)	{	
#ifdef CONFIG_TARGET_LOCALE_US_ATT_REV01
		fsa9480_manual_switching(SWITCH_PORT_AUTO);
#else
		gpio_set_value(UART_SEL_SW,0);
#endif
		mdelay(10);		
	} else {	
#ifdef CONFIG_TARGET_LOCALE_US_ATT_REV01
		fsa9480_manual_switching(SWITCH_PORT_VAUDIO);
#else
		gpio_set_value(UART_SEL_SW,1);
#endif
		mdelay(10);		
	}
}
#endif


static ssize_t uart_sel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);
	int uart_sel = secsw->switch_sel & UART_SEL_MASK;

	return sprintf(buf, "%s UART\n", uart_sel ? "PDA" : "MODEM");
}

static ssize_t uart_sel_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);
#ifdef CONFIG_SEC_MISC
	sec_get_param(param_index_uartsel, &secsw->switch_sel);
#endif

	if (strncmp(buf, "PDA", 3) == 0 || strncmp(buf, "pda", 3) == 0) {
		uart_switch_mode(secsw, SWITCH_PDA);
		secsw->switch_sel |= UART_SEL_MASK;
		pr_err("[UART Switch] Path : PDA\n");
	}

	if (strncmp(buf, "MODEM", 5) == 0 || strncmp(buf, "modem", 5) == 0) {
		uart_switch_mode(secsw, SWITCH_MODEM);
		secsw->switch_sel &= ~UART_SEL_MASK;
		pr_err("[UART Switch] Path : MODEM\n");
	}

	pr_err("end %s value = 0x%X \n", __func__, secsw->switch_sel);
#ifdef CONFIG_SEC_MISC
	sec_set_param(param_index_uartsel, &secsw->switch_sel);
#endif
	return size;
}

static ssize_t usb_sel_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);
	int usb_path = secsw->switch_sel & USB_SEL_MASK;
	return sprintf(buf, "%s USB\n", usb_path ? "PDA" : "MODEM");
}

static ssize_t usb_sel_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t size)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);
	int usb_sel = 0;
	usb_sel = secsw->switch_sel & USB_SEL_MASK;
#ifdef CONFIG_SEC_MISC
	sec_get_param(param_index_uartsel, &secsw->switch_sel);
#endif
	if (strncmp(buf, "PDA", 3) == 0 || strncmp(buf, "pda", 3) == 0) {
		if (usb_sel!=SWITCH_PDA)
		{
			usb_switch_mode(secsw, SWITCH_PDA);
			secsw->switch_sel |= USB_SEL_MASK;
		}
	}
	if (strncmp(buf, "MODEM", 5) == 0 || strncmp(buf, "modem", 5) == 0) {
		if (usb_sel!=SWITCH_MODEM)
		{		
			usb_switch_mode(secsw, SWITCH_MODEM);
			secsw->switch_sel &= ~USB_SEL_MASK;
		}
	}
#ifdef CONFIG_SEC_MISC
	sec_set_param(param_index_uartsel, &secsw->switch_sel);
#endif
	return size;
}

static ssize_t usb_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);
	int cable_state = CABLE_TYPE_NONE;

	if (secsw->pdata && secsw->pdata->get_cable_status)
		cable_state = secsw->pdata->get_cable_status();

	return sprintf(buf, "%s\n", (cable_state == CABLE_TYPE_USB) ?
			"USB_STATE_CONFIGURED" : "USB_STATE_NOTCONFIGURED");
}

static ssize_t usb_state_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	return 0;
}

static ssize_t disable_vbus_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t disable_vbus_store(struct device *dev, struct device_attribute *attr,
				  const char *buf, size_t size)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(secsw->pdata) ||
		IS_ERR_OR_NULL(secsw->pdata->set_vbus_status) ||
		IS_ERR_OR_NULL(secsw->pdata->set_usb_gadget_vbus))
		return size;

	secsw->pdata->set_usb_gadget_vbus(false);
	secsw->pdata->set_vbus_status((u8)USB_VBUS_ALL_OFF);
	msleep(10);
	secsw->pdata->set_usb_gadget_vbus(true);

	return size;
}

static ssize_t device_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{	
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);
	int dev_type = secsw->pdata->get_attached_device();

	if (dev_type == DEV_TYPE_NONE)
		return sprintf(buf, "NONE\n");
	else if (dev_type == DEV_TYPE_USB)
		return sprintf(buf, "USB\n");
	else if (dev_type == DEV_TYPE_CHARGER)
		return sprintf(buf, "CHARGER\n");
	else if (dev_type == DEV_TYPE_JIG)
		return sprintf(buf, "JIG\n");
	else if (dev_type == DEV_TYPE_DESKDOCK)
		return sprintf(buf, "DESKDOCK\n");
	else if (dev_type == DEV_TYPE_CARDOCK)
		return sprintf(buf, "CARDOCK\n");
	else
		return sprintf(buf, "UNKNOWN\n");
}

static DEVICE_ATTR(device_type, 0664, device_type_show, NULL);

#ifdef _SEC_DM_
/* for sysfs control (/sys/class/sec/switch/.usb_lock/enable) */
static ssize_t usb_lock_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (usb_access_lock)
		return snprintf(buf, PAGE_SIZE, "USB_LOCK");
	else
		return snprintf(buf, PAGE_SIZE, "USB_UNLOCK");
}

static ssize_t usb_lock_enable_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);
	int cable_state;
	int value;
	
	if (sscanf(buf, "%d", &value) != 1) {
		pr_err("%s : Invalid value\n", __func__);
		return -EINVAL;
	}

	if((value < 0) || (value > 1)) {
		pr_err("%s : Invalid value\n", __func__);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(secsw->pdata) ||
		IS_ERR_OR_NULL(secsw->pdata->set_usb_gadget_vbus) ||
	 	IS_ERR_OR_NULL(secsw->pdata->get_cable_status))
		return size;

		cable_state = secsw->pdata->get_cable_status();
	
	if(value != usb_access_lock) {
		usb_access_lock = value;
		
		if(value == 1) {
			pr_err("%s : Set USB Block!!\n", __func__);
			secsw->pdata->set_usb_gadget_vbus(false);
		} else {
			pr_err("%s : Release USB Block!!\n", __func__);
			if (cable_state)
				secsw->pdata->set_usb_gadget_vbus(true);
		}
	}

	return size;
}
#endif

static DEVICE_ATTR(uart_sel, 0664, uart_sel_show, uart_sel_store);
static DEVICE_ATTR(usb_sel, 0664, usb_sel_show, usb_sel_store);
static DEVICE_ATTR(usb_state, 0664, usb_state_show, usb_state_store);
static DEVICE_ATTR(disable_vbus, 0664, disable_vbus_show, disable_vbus_store);
#ifdef _SEC_DM_
static DEVICE_ATTR(enable, 0664, usb_lock_enable_show, usb_lock_enable_store);
#endif

#if 0
static int check_for_cp_boot(void)
{
#define MAX_CHECK_COUNT 10
	static unsigned int check_count = 0;

	if (charging_mode_get())
		return 1;

	if (check_count >= MAX_CHECK_COUNT || cp_boot_ok)
		return 1;

	check_count++;
	
	return 0;
}
#endif

//To prevent lock up during getting parameter about switch_sel.
#ifdef CONFIG_SEC_MISC
static int gRetryCount=0;
#endif

static void sec_switch_init_work(struct work_struct *work)
{
	struct delayed_work *dw = container_of(work, struct delayed_work, work);
	struct sec_switch_wq *wq = container_of(dw, struct sec_switch_wq, work_q);
	struct sec_switch_struct *secsw = wq->sdata;
	int usb_sel = 0;
	int uart_sel = 0;
#if 0	
	if (sec_get_param_value &&
	    secsw->pdata &&
	    secsw->pdata->set_vbus_status &&
	    secsw->pdata->get_phy_init_status &&
	    secsw->pdata->get_phy_init_status() &&
	    check_for_cp_boot()) {
		sec_get_param_value(__SWITCH_SEL, &secsw->switch_sel);
#endif
	if (secsw->pdata &&
	    secsw->pdata->set_vbus_status &&
	    secsw->pdata->get_phy_init_status &&
	    secsw->pdata->get_phy_init_status()) {
#ifdef CONFIG_SEC_MISC
		if ( (!sec_get_param(param_index_uartsel, &secsw->switch_sel)) && (gRetryCount++ < 9) )
		{	
			pr_err("%s error value = %d \n", __func__, secsw->switch_sel);
			schedule_delayed_work(&wq->work_q, msecs_to_jiffies(1000));
			return;
		}

		//To prevent lock up during getting parameter about switch_sel.
		if(gRetryCount > 9)
		{
			secsw->switch_sel = 1; // default modem
			pr_err("%s failed 10 times... So switch_sel is set by [1]\n", __func__);
		}
		gRetryCount = 0;
#endif
		pr_err("%s value = %d \n", __func__, secsw->switch_sel);
		cancel_delayed_work(&wq->work_q);
	} else {
		schedule_delayed_work(&wq->work_q, msecs_to_jiffies(1000));
		return;
	}
	usb_sel = secsw->switch_sel & USB_SEL_MASK;
	uart_sel = secsw->switch_sel & UART_SEL_MASK;


// USB switch not use LTE

	if (usb_sel)
		usb_switch_mode(secsw, SWITCH_PDA);
	else
		usb_switch_mode(secsw, SWITCH_MODEM);


// UART switch mode
	if (uart_sel)
		uart_switch_mode(secsw, SWITCH_PDA);
	else
		uart_switch_mode(secsw, SWITCH_MODEM);
	
}

static int sec_switch_probe(struct platform_device *pdev)
{
	struct sec_switch_struct *secsw;
	struct sec_switch_platform_data *pdata = pdev->dev.platform_data;
	struct sec_switch_wq *wq;

	pr_err("sec_switch_probe enter %s\n", __func__);

	if (!pdata) {
		pr_err("%s : pdata is NULL.\n", __func__);
		return -ENODEV;
	}

	secsw = kzalloc(sizeof(struct sec_switch_struct), GFP_KERNEL);
	if (!secsw) {
		pr_err("%s : failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	secsw->pdata = pdata;
	
#if defined (CONFIG_TARGET_LOCALE_US_ATT_REV01) || defined(CONFIG_KOR_MODEL_SHV_E160S)|| defined (CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	secsw->switch_sel = 3;
#else
	secsw->switch_sel = 3;
#endif

	dev_set_drvdata(switch_dev, secsw);

	/* create sysfs files */
	if (device_create_file(switch_dev, &dev_attr_uart_sel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_uart_sel.attr.name);

	if (device_create_file(switch_dev, &dev_attr_usb_sel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_usb_sel.attr.name);

	if (device_create_file(switch_dev, &dev_attr_usb_state) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_usb_state.attr.name);

	if (device_create_file(switch_dev, &dev_attr_disable_vbus) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_usb_state.attr.name);

	if (device_create_file(switch_dev, &dev_attr_device_type) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_device_type.attr.name);

#ifdef _SEC_DM_
	usb_lock = device_create(sec_class, switch_dev, MKDEV(0, 0), NULL, ".usb_lock");

	if (IS_ERR(usb_lock)) {
		pr_err("Failed to create device (usb_lock)!\n");
		return PTR_ERR(usb_lock);
	}

	dev_set_drvdata(usb_lock, secsw);

	if (device_create_file(usb_lock, &dev_attr_enable) < 0)	{
		pr_err("Failed to create device file(%s)!\n", dev_attr_enable.attr.name);
		device_destroy((struct class *)usb_lock, MKDEV(0, 0));
	}
#endif

	/* run work queue */
	wq = kmalloc(sizeof(struct sec_switch_wq), GFP_ATOMIC);
	if (wq) {
		wq->sdata = secsw;
		INIT_DELAYED_WORK(&wq->work_q, sec_switch_init_work);
		schedule_delayed_work(&wq->work_q, msecs_to_jiffies(100));
	} else
		return -ENOMEM;
	return 0;
}

static int sec_switch_remove(struct platform_device *pdev)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(&pdev->dev);
	kfree(secsw);
	return 0;
}

static struct platform_driver sec_switch_driver = {
	.probe = sec_switch_probe,
	.remove = sec_switch_remove,
	.driver = {
			.name = "sec_switch",
			.owner = THIS_MODULE,
	},
};

static int __init sec_switch_init(void)
{
	return platform_driver_register(&sec_switch_driver);
}

static void __exit sec_switch_exit(void)
{
	platform_driver_unregister(&sec_switch_driver);
}

module_init(sec_switch_init);
module_exit(sec_switch_exit);

MODULE_AUTHOR("Ikkeun Kim <iks.kim@samsung.com>");
MODULE_DESCRIPTION("Samsung Electronics Corp Switch driver");
MODULE_LICENSE("GPL");
