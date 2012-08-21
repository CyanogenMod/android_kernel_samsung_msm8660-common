/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */
 
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/unistd.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/major.h>
#include <linux/input.h>
#include <mach/gpio.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/miscdevice.h>

MODULE_LICENSE("GPL");

#define CAM_SW_EN	130		// New Flash Analog Switch - LOW: ISP / HIGH: AP
#define CAM_IO_EN	37		// Old Flash Analog Switch - LOW: AP / HIGH: ISP
#define TORCH_EN	62
#define TORCH_SET	63

#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) ||defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
#define TORCH_MINOR 216
#endif


extern unsigned int get_hw_rev(void);

struct class *ledflash_class;

static int ledflash_open(struct inode *inode, struct file *file)
{
	/* Do nothing */
	return 0;
}

static int ledflash_close(struct inode *inode, struct file *file)
{
	/* Do nothing */
	return 0;
}

static ssize_t ledflash_torch_onoff_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	char value;
	
	sscanf(buf, "%c", &value);
	
	printk("[Torch] %s : torch = %c\n", __func__,*buf);
	
	if (value == '1') {	/* Torch ON */	
	// Flash Analog Switch - AP
#if defined (CONFIG_USA_MODEL_SGH_I717)
 		if (get_hw_rev() >= 0x05)
 			gpio_set_value_cansleep(CAM_SW_EN, 1);
#endif
		gpio_set_value_cansleep(TORCH_EN, 1);
		gpio_set_value_cansleep(TORCH_SET, 1);
		mdelay(1); 
	} else {			/* Torch OFF */
	// Flash Analog Switch - ISP
#if defined (CONFIG_USA_MODEL_SGH_I717)
	 	if (get_hw_rev() >= 0x05)
	 		gpio_set_value_cansleep(CAM_SW_EN, 0); 
#endif
		gpio_set_value_cansleep(TORCH_EN, 0);
		gpio_set_value_cansleep(TORCH_SET, 0);
		mdelay(1);
	}
	
	return size;
}

static const struct file_operations ledflash_fops = {
	.owner = THIS_MODULE,
	.open = ledflash_open,
	.release = ledflash_close,
};

static struct miscdevice ledflash_device = {
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) ||defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	.minor = TORCH_MINOR,
#else  
	.minor = MISC_DYNAMIC_MINOR,
#endif	
	.name = "ledflash",
	.fops = &ledflash_fops,
};

static DEVICE_ATTR(torch, 0660, NULL, ledflash_torch_onoff_store);

static int ledflash_init(void)
{
	int err = 0;
	struct device *dev_t;
	
	printk("[Torch] ledflash_init\n");
	
	if((err = misc_register(&ledflash_device)) < 0) {
		printk(KERN_ERR "%s: misc_register failed! ret=%d\n", __func__, err);
		return err;
	}
	
	ledflash_class = class_create(THIS_MODULE, "ledflash");
	
	if (IS_ERR(ledflash_class)) 
		return PTR_ERR( ledflash_class );
	
	dev_t = device_create( ledflash_class, NULL, 0, NULL, "sec_ledflash");
	
	if (device_create_file(dev_t, &dev_attr_torch) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_torch.attr.name);

	if (IS_ERR(dev_t)) {
		return PTR_ERR(dev_t);
	}

#if defined (CONFIG_USA_MODEL_SGH_I717)
	 if (get_hw_rev() >= 0x05) {
		gpio_tlmm_config(GPIO_CFG(CAM_SW_EN, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
 	}
#endif 

	return 0;
}

static void ledflash_exit(void)
{
	printk("[Torch] ledflash exit\n");
	misc_deregister(&ledflash_device);
	class_destroy(ledflash_class);	
}

module_init(ledflash_init);
module_exit(ledflash_exit);


