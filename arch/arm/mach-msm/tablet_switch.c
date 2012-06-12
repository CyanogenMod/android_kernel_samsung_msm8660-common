/*
 *
 * Copyright (C) 2011 Samsung Electronics.
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
#include <linux/moduleparam.h>
#include <linux/usb/android_composite.h>
#include <asm/mach/arch.h>
#include <mach/gpio.h>
#include <mach/sec_switch.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/mfd/pmic8058.h>
#include <linux/sec_param.h>
#include <mach/devices-lte.h>

#define USB_SEL_SW           37
#define UART_SEL_SW          58

#define USB_SEL_MASK         (1<<2)
#define UART_SEL_MASK        (1<<1)

struct sec_switch_struct {
	struct sec_switch_platform_data *pdata;
	int switch_sel;
	int is_manualset;
};

struct sec_switch_wq {
	struct delayed_work work_q;
	struct sec_switch_struct *sdata;
	struct list_head entry;
};

#ifdef _SEC_DM_
struct device *usb_lock;

extern struct class *sec_class;
#endif

/* for sysfs control (/sys/class/sec/switch/) */
extern struct device *switch_dev;
// TEMP extern void samsung_enable_function(int mode);

static void usb_switch_mode(struct sec_switch_struct *secsw, int mode)
{
	if (mode == SWITCH_PDA)	{
		printk("[*#7284#_USB] do nothing\n");
	} else {
// TEMP		printk("[*#7284#_USB] enable ACM1(MDM modem port) + RMNET\n");
// TEMP		samsung_enable_function(USBSTATUS_MDM);
	}
}

static void uart_switch_mode(struct sec_switch_struct *secsw, int mode)
{
	if (mode == SWITCH_PDA)	{	
		gpio_set_value(UART_SEL_SW,0);
		mdelay(10);		
	} else {	
		gpio_set_value(UART_SEL_SW,1);
		mdelay(10);		
	}
}

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

	sec_get_param(param_index_uartsel, &secsw->switch_sel);

	if (strncmp(buf, "PDA", 3) == 0 || strncmp(buf, "pda", 3) == 0) {
		uart_switch_mode(secsw, SWITCH_PDA);
		secsw->switch_sel |= UART_SEL_MASK;
		pr_debug("[UART Switch] Path : PDA\n");
	}

	if (strncmp(buf, "MODEM", 5) == 0 || strncmp(buf, "modem", 5) == 0) {
		uart_switch_mode(secsw, SWITCH_MODEM);
		secsw->switch_sel &= ~UART_SEL_MASK;
		pr_debug("[UART Switch] Path : MODEM\n");
	}

	sec_set_param(param_index_uartsel, &secsw->switch_sel);

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

	sec_get_param(param_index_uartsel, &secsw->switch_sel);

	secsw->is_manualset=1;

	if (strncmp(buf, "PDA", 3) == 0 || strncmp(buf, "pda", 3) == 0) {
		if (usb_sel!=SWITCH_PDA) {
			usb_switch_mode(secsw, SWITCH_PDA);
			secsw->switch_sel |= USB_SEL_MASK;
		}
	}

	if (strncmp(buf, "MODEM", 5) == 0 || strncmp(buf, "modem", 5) == 0) {
		if (usb_sel!=SWITCH_MODEM) {		
			usb_switch_mode(secsw, SWITCH_MODEM);
			secsw->switch_sel &= ~USB_SEL_MASK;
		}
	}

	sec_set_param(param_index_uartsel, &secsw->switch_sel);

	return size;
}

static ssize_t usb_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);
	int cable_state = CABLE_TYPE_NONE;

	if (secsw->pdata && secsw->pdata->get_cable_status) {
		cable_state = secsw->pdata->get_cable_status();
	}

	return sprintf(buf, "%s\n", (cable_state == CABLE_TYPE_USB) ?
			"USB_STATE_CONFIGURED" : "USB_STATE_NOTCONFIGURED");
}

static ssize_t usb_state_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t size)
{
	return 0;
}

#if 0 /* NOT USED */
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
	int dev_type;

	if (IS_ERR_OR_NULL(secsw->pdata) ||
		IS_ERR_OR_NULL(secsw->pdata->get_attached_device))
		return 0;

	dev_type= secsw->pdata->get_attached_device();

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
#endif

#ifdef _SEC_DM_
/* for sysfs control (/sys/class/sec/switch/.usb_lock/enable) */
static ssize_t usb_lock_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_switch_struct *secsw = dev_get_drvdata(dev);

	if (secsw->pdata->usb_access_lock)
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
	
	if(value != secsw->pdata->usb_access_lock) {
		secsw->pdata->usb_access_lock = value;
		
		if(value == 1) {
			pr_info("%s : Set USB Block!!\n", __func__);
			secsw->pdata->set_usb_gadget_vbus(false);
		} else {
			pr_info("%s : Release USB Block!!\n", __func__);
			if (cable_state)
				secsw->pdata->set_usb_gadget_vbus(true);
		}
	}

	return size;
}
#endif

static DEVICE_ATTR(uart_sel, 0660, uart_sel_show, uart_sel_store);
static DEVICE_ATTR(usb_state, 0660, usb_state_show, usb_state_store);
static DEVICE_ATTR(usb_sel, 0660, usb_sel_show, usb_sel_store);
#if 0 /* NOT USED */
static DEVICE_ATTR(disable_vbus, S_IRUGO |S_IWUGO, disable_vbus_show, disable_vbus_store);
static DEVICE_ATTR(device_type, S_IRUGO |S_IWUGO | S_IRUSR | S_IWUSR, device_type_show, NULL);
#endif
#ifdef _SEC_DM_
static DEVICE_ATTR(enable, 0660, usb_lock_enable_show, usb_lock_enable_store);
#endif

static void sec_switch_init_work(struct work_struct *work)
{
	struct delayed_work *dw = container_of(work, struct delayed_work, work);
	struct sec_switch_wq *wq = container_of(dw, struct sec_switch_wq, work_q);
	struct sec_switch_struct *secsw = wq->sdata;
	int usb_sel = 0;
	int uart_sel = 0;

	cancel_delayed_work(&wq->work_q);

	sec_get_param(param_index_uartsel, &secsw->switch_sel);
	if ( secsw->switch_sel == 0 ) {
		secsw->switch_sel = 0x4;		// UART_SEL default to MDM9K & USB_SEL default to APQ8K : 0100
		sec_set_param(param_index_uartsel, &secsw->switch_sel);
	}

	pr_err("sec_switch_probe enter %s value = 0x%X \n", __func__, secsw->switch_sel);

	usb_sel = secsw->switch_sel & USB_SEL_MASK;
	uart_sel = secsw->switch_sel & UART_SEL_MASK;

	if (uart_sel) {
		uart_switch_mode(secsw, SWITCH_PDA);
	} else {
		uart_switch_mode(secsw, SWITCH_MODEM);
	}
	
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
	secsw->switch_sel = 1;	/* UART_SEL default to MDM9K */
	secsw->is_manualset= 0;

	dev_set_drvdata(switch_dev, secsw);

	/* Initializing GPIO for control */
	gpio_request(UART_SEL_SW, "uart_sel");
	gpio_tlmm_config(GPIO_CFG(UART_SEL_SW, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 1);

	/* create sysfs files */
	if (device_create_file(switch_dev, &dev_attr_uart_sel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_uart_sel.attr.name);

	if (device_create_file(switch_dev, &dev_attr_usb_state) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_usb_state.attr.name);

	if (device_create_file(switch_dev, &dev_attr_usb_sel) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_usb_sel.attr.name);

#if 0 	/* NOT USED */
	if (device_create_file(switch_dev, &dev_attr_disable_vbus) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_usb_state.attr.name);

	if (device_create_file(switch_dev, &dev_attr_device_type) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_device_type.attr.name);
#endif

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
		schedule_delayed_work(&wq->work_q, msecs_to_jiffies(7000)); // proper delay until param block works fine
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

MODULE_DESCRIPTION("Samsung Electronics Corp Switch driver");
MODULE_LICENSE("GPL");
