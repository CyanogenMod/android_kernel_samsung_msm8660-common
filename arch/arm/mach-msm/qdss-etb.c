/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mutex.h>

#include "qdss.h"

#define etb_writel(etb, val, off)	__raw_writel((val), etb.base + off)
#define etb_readl(etb, off)		__raw_readl(etb.base + off)

#define ETB_RAM_DEPTH_REG	(0x004)
#define ETB_STATUS_REG		(0x00C)
#define ETB_RAM_READ_DATA_REG	(0x010)
#define ETB_RAM_READ_POINTER	(0x014)
#define ETB_RAM_WRITE_POINTER	(0x018)
#define ETB_TRG			(0x01C)
#define ETB_CTL_REG		(0x020)
#define ETB_RWD_REG		(0x024)
#define ETB_FFSR		(0x300)
#define ETB_FFCR		(0x304)
#define ETB_ITMISCOP0		(0xEE0)
#define ETB_ITTRFLINACK		(0xEE4)
#define ETB_ITTRFLIN		(0xEE8)
#define ETB_ITATBDATA0		(0xEEC)
#define ETB_ITATBCTR2		(0xEF0)
#define ETB_ITATBCTR1		(0xEF4)
#define ETB_ITATBCTR0		(0xEF8)


#define BYTES_PER_WORD		4
#define ETB_SIZE_WORDS		4096
#define FRAME_SIZE_WORDS	4

#define ETB_LOCK()							\
do {									\
	mb();								\
	etb_writel(etb, 0x0, CS_LAR);					\
} while (0)
#define ETB_UNLOCK()							\
do {									\
	etb_writel(etb, CS_UNLOCK_MAGIC, CS_LAR);			\
	mb();								\
} while (0)

struct etb_ctx {
	uint8_t		*buf;
	void __iomem	*base;
	bool		enabled;
	bool		reading;
	struct mutex	mutex;
	atomic_t	in_use;
	struct device	*dev;
	struct kobject	*kobj;
	uint32_t	trigger_cntr;
};

static struct etb_ctx etb;

static void __etb_enable(void)
{
	int i;

	ETB_UNLOCK();

	etb_writel(etb, 0x0, ETB_RAM_WRITE_POINTER);
	for (i = 0; i < ETB_SIZE_WORDS; i++)
		etb_writel(etb, 0x0, ETB_RWD_REG);

	etb_writel(etb, 0x0, ETB_RAM_WRITE_POINTER);
	etb_writel(etb, 0x0, ETB_RAM_READ_POINTER);

	etb_writel(etb, etb.trigger_cntr, ETB_TRG);
	etb_writel(etb, BIT(13) | BIT(0), ETB_FFCR);
	etb_writel(etb, BIT(0), ETB_CTL_REG);

	ETB_LOCK();
}

void etb_enable(void)
{
	mutex_lock(&etb.mutex);
	__etb_enable();
	etb.enabled = true;
	dev_info(etb.dev, "ETB enabled\n");
	mutex_unlock(&etb.mutex);
}

static void __etb_disable(void)
{
	int count;
	uint32_t ffcr;

	/* Avoid oopsing in panic() if called before the device is probed. */
	if (!etb.base)
		return;

	ETB_UNLOCK();

	ffcr = etb_readl(etb, ETB_FFCR);
	ffcr |= (BIT(12) | BIT(6));
	etb_writel(etb, ffcr, ETB_FFCR);

	for (count = TIMEOUT_US; BVAL(etb_readl(etb, ETB_FFCR), 6) != 0
				&& count > 0; count--)
		udelay(1);
	WARN(count == 0, "timeout while flushing ETB, ETB_FFCR: %#x\n",
	     etb_readl(etb, ETB_FFCR));

	etb_writel(etb, 0x0, ETB_CTL_REG);

	for (count = TIMEOUT_US; BVAL(etb_readl(etb, ETB_FFSR), 1) != 1
				&& count > 0; count--)
		udelay(1);
	WARN(count == 0, "timeout while disabling ETB, ETB_FFSR: %#x\n",
	     etb_readl(etb, ETB_FFSR));

	ETB_LOCK();
}

void etb_disable(void)
{
	mutex_lock(&etb.mutex);
	__etb_disable();
	etb.enabled = false;
	dev_info(etb.dev, "ETB disabled\n");
	mutex_unlock(&etb.mutex);
}

static void __etb_dump(void)
{
	int i;
	uint8_t *buf_ptr;
	uint32_t read_data;
	uint32_t read_ptr;
	uint32_t write_ptr;
	uint32_t frame_off;
	uint32_t frame_endoff;

	ETB_UNLOCK();

	read_ptr = etb_readl(etb, ETB_RAM_READ_POINTER);
	write_ptr = etb_readl(etb, ETB_RAM_WRITE_POINTER);

	frame_off = write_ptr % FRAME_SIZE_WORDS;
	frame_endoff = FRAME_SIZE_WORDS - frame_off;
	if (frame_off) {
		dev_err(etb.dev, "write_ptr: %lu not aligned to formatter "
				"frame size\n", (unsigned long)write_ptr);
		dev_err(etb.dev, "frameoff: %lu, frame_endoff: %lu\n",
			(unsigned long)frame_off, (unsigned long)frame_endoff);
		write_ptr += frame_endoff;
	}

	if ((etb_readl(etb, ETB_STATUS_REG) & BIT(0)) == 0)
		etb_writel(etb, 0x0, ETB_RAM_READ_POINTER);
	else
		etb_writel(etb, write_ptr, ETB_RAM_READ_POINTER);

	buf_ptr = etb.buf;
	for (i = 0; i < ETB_SIZE_WORDS; i++) {
		read_data = etb_readl(etb, ETB_RAM_READ_DATA_REG);
		*buf_ptr++ = read_data >> 0;
		*buf_ptr++ = read_data >> 8;
		*buf_ptr++ = read_data >> 16;
		*buf_ptr++ = read_data >> 24;
	}

	if (frame_off) {
		buf_ptr -= (frame_endoff * BYTES_PER_WORD);
		for (i = 0; i < frame_endoff; i++) {
			*buf_ptr++ = 0x0;
			*buf_ptr++ = 0x0;
			*buf_ptr++ = 0x0;
			*buf_ptr++ = 0x0;
		}
	}

	etb_writel(etb, read_ptr, ETB_RAM_READ_POINTER);

	ETB_LOCK();
}

void etb_dump(void)
{
	mutex_lock(&etb.mutex);
	if (etb.enabled) {
		__etb_disable();
		__etb_dump();
		__etb_enable();

		dev_info(etb.dev, "ETB dumped\n");
	}
	mutex_unlock(&etb.mutex);
}

static int etb_open(struct inode *inode, struct file *file)
{
	if (atomic_cmpxchg(&etb.in_use, 0, 1))
		return -EBUSY;

	dev_dbg(etb.dev, "%s: successfully opened\n", __func__);
	return 0;
}

static ssize_t etb_read(struct file *file, char __user *data,
				size_t len, loff_t *ppos)
{
	if (etb.reading == false) {
		etb_dump();
		etb.reading = true;
	}

	if (*ppos + len > ETB_SIZE_WORDS * BYTES_PER_WORD)
		len = ETB_SIZE_WORDS * BYTES_PER_WORD - *ppos;

	if (copy_to_user(data, etb.buf + *ppos, len)) {
		dev_dbg(etb.dev, "%s: copy_to_user failed\n", __func__);
		return -EFAULT;
	}

	*ppos += len;

	dev_dbg(etb.dev, "%s: %d bytes copied, %d bytes left\n",
		__func__, len, (int) (ETB_SIZE_WORDS * BYTES_PER_WORD - *ppos));

	return len;
}

static int etb_release(struct inode *inode, struct file *file)
{
	etb.reading = false;

	atomic_set(&etb.in_use, 0);

	dev_dbg(etb.dev, "%s: released\n", __func__);

	return 0;
}

static const struct file_operations etb_fops = {
	.owner =	THIS_MODULE,
	.open =		etb_open,
	.read =		etb_read,
	.release =	etb_release,
};

static struct miscdevice etb_misc = {
	.name =		"msm_etb",
	.minor =	MISC_DYNAMIC_MINOR,
	.fops =		&etb_fops,
};

#define ETB_ATTR(name)						\
static struct kobj_attribute name##_attr =				\
		__ATTR(name, S_IRUGO | S_IWUSR, name##_show, name##_store)

static ssize_t trigger_cntr_store(struct kobject *kobj,
			struct kobj_attribute *attr,
			const char *buf, size_t n)
{
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	etb.trigger_cntr = val;
	return n;
}
static ssize_t trigger_cntr_show(struct kobject *kobj,
			struct kobj_attribute *attr,
			char *buf)
{
	unsigned long val = etb.trigger_cntr;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
ETB_ATTR(trigger_cntr);

static int __init etb_sysfs_init(void)
{
	int ret;

	etb.kobj = kobject_create_and_add("etb", qdss_get_modulekobj());
	if (!etb.kobj) {
		dev_err(etb.dev, "failed to create ETB sysfs kobject\n");
		ret = -ENOMEM;
		goto err_create;
	}

	ret = sysfs_create_file(etb.kobj, &trigger_cntr_attr.attr);
	if (ret) {
		dev_err(etb.dev, "failed to create ETB sysfs trigger_cntr"
		" attribute\n");
		goto err_file;
	}

	return 0;
err_file:
	kobject_put(etb.kobj);
err_create:
	return ret;
}

static void etb_sysfs_exit(void)
{
	sysfs_remove_file(etb.kobj, &trigger_cntr_attr.attr);
	kobject_put(etb.kobj);
}

static int __devinit etb_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *res;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -EINVAL;
		goto err_res;
	}

	etb.base = ioremap_nocache(res->start, resource_size(res));
	if (!etb.base) {
		ret = -EINVAL;
		goto err_ioremap;
	}

	etb.dev = &pdev->dev;

	mutex_init(&etb.mutex);

	ret = misc_register(&etb_misc);
	if (ret)
		goto err_misc;

	etb.buf = kzalloc(ETB_SIZE_WORDS * BYTES_PER_WORD, GFP_KERNEL);
	if (!etb.buf) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	etb_sysfs_init();

	dev_info(etb.dev, "ETB initialized\n");
	return 0;

err_alloc:
	misc_deregister(&etb_misc);
err_misc:
	mutex_destroy(&etb.mutex);
	iounmap(etb.base);
err_ioremap:
err_res:
	dev_err(etb.dev, "ETB init failed\n");
	return ret;
}

static int etb_remove(struct platform_device *pdev)
{
	if (etb.enabled)
		etb_disable();
	etb_sysfs_exit();
	kfree(etb.buf);
	misc_deregister(&etb_misc);
	mutex_destroy(&etb.mutex);
	iounmap(etb.base);

	return 0;
}

static struct platform_driver etb_driver = {
	.probe          = etb_probe,
	.remove         = etb_remove,
	.driver         = {
		.name   = "msm_etb",
	},
};

int __init etb_init(void)
{
	return platform_driver_register(&etb_driver);
}

void etb_exit(void)
{
	platform_driver_unregister(&etb_driver);
}
