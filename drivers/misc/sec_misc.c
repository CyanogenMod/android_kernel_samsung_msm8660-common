/*
 * driver/misc/sec_misc.c
 *
 * driver supporting miscellaneous functions for Samsung P-series device
 *
 * COPYRIGHT(C) Samsung Electronics Co., Ltd. 2006-2011 All Right Reserved.  
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/wakelock.h>
#include <linux/blkdev.h>
#include <mach/gpio.h>
#include <linux/sec_param.h>

#define MOVINAND_CHECKSUM 1
#define RORY_CONTROL

static struct wake_lock sec_misc_wake_lock;

#ifdef MOVINAND_CHECKSUM
unsigned char emmc_checksum_done;
unsigned char emmc_checksum_pass;
#endif

static struct file_operations sec_misc_fops =
{
	.owner = THIS_MODULE,
	//.read = sec_misc_read,
	//.ioctl = sec_misc_ioctl,
	//.open = sec_misc_open,
	//.release = sec_misc_release,
};

static struct miscdevice sec_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sec_misc",
	.fops = &sec_misc_fops,
};

#ifdef MOVINAND_CHECKSUM
static ssize_t emmc_checksum_done_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", emmc_checksum_done);
}

static ssize_t emmc_checksum_done_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int state;

	if (sscanf(buf, "%i", &state) != 1 || (state < 0 || state > 1))
		return -EINVAL;

	emmc_checksum_done = (unsigned char)state;
	return size;
}

static DEVICE_ATTR(emmc_checksum_done, S_IRUGO | (S_IWUSR | S_IWGRP), emmc_checksum_done_show, emmc_checksum_done_store);

static ssize_t emmc_checksum_pass_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", emmc_checksum_pass);
}

static ssize_t emmc_checksum_pass_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int state;

	if (sscanf(buf, "%i", &state) != 1 || (state < 0 || state > 1))
		return -EINVAL;

	emmc_checksum_pass = (unsigned char)state;
	return size;
}

static DEVICE_ATTR(emmc_checksum_pass, S_IRUGO | (S_IWUSR | S_IWGRP), emmc_checksum_pass_show, emmc_checksum_pass_store);
#endif /*MOVINAND_CHECKSUM*/


#ifdef RORY_CONTROL
static ssize_t rory_control_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int rory_control;
	
	sec_get_param(param_rory_control, &rory_control);

	return sprintf(buf, "%d\n", rory_control);
}

static ssize_t rory_control_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int rory_control;

	sscanf(buf, "%i", &rory_control);

	pr_info("rory control store ..... %d \n", rory_control);
	
	/* write to param */
	sec_set_param(param_rory_control, &rory_control);

	return size;
}

static DEVICE_ATTR(rory_control, S_IRUGO | (S_IWUSR | S_IWGRP), rory_control_show, rory_control_store);
#endif /*RORY_CONTROL*/

extern struct class *sec_class;
struct device *sec_misc_dev;

static int __init sec_misc_init(void)
{
	int ret=0;

	ret = misc_register(&sec_misc_device);
	if (ret<0) {
		printk(KERN_ERR "misc_register failed!\n");
		return ret;
	}

	sec_misc_dev = device_create(sec_class, NULL, 0, NULL, "sec_misc");
	if (IS_ERR(sec_misc_dev)) {
		printk(KERN_ERR "failed to create device!\n");
		return -1;
	}

#ifdef MOVINAND_CHECKSUM
	if (device_create_file(sec_misc_dev, &dev_attr_emmc_checksum_done) < 0)
		pr_err("failed to create device file - %s\n", dev_attr_emmc_checksum_done.attr.name);

	if (device_create_file(sec_misc_dev, &dev_attr_emmc_checksum_pass) < 0)
		pr_err("failed to create device file - %s\n", dev_attr_emmc_checksum_pass.attr.name);
#endif

#ifdef RORY_CONTROL
	if (device_create_file(sec_misc_dev, &dev_attr_rory_control) < 0)
		pr_err("failed to create device file - %s\n", dev_attr_rory_control.attr.name);
#endif

	wake_lock_init(&sec_misc_wake_lock, WAKE_LOCK_SUSPEND, "sec_misc");

	return 0;
}

static void __exit sec_misc_exit(void)
{
	wake_lock_destroy(&sec_misc_wake_lock);
}

module_init(sec_misc_init);
module_exit(sec_misc_exit);

/* Module information */
MODULE_AUTHOR("Samsung");
MODULE_DESCRIPTION("Samsung PX misc. driver");
MODULE_LICENSE("GPL");

