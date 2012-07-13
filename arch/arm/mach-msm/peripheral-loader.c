/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/elf.h>
#include <linux/mutex.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/rwsem.h>

#include <mach/socinfo.h>

#include <asm/uaccess.h>
#include <asm/setup.h>

#include "peripheral-loader.h"

struct pil_device {
	struct pil_desc *desc;
	int count;
	struct mutex lock;
	struct list_head list;
};

static DEFINE_MUTEX(pil_list_lock);
static LIST_HEAD(pil_list);

static struct pil_device *__find_peripheral(const char *str)
{
	struct pil_device *dev;

	list_for_each_entry(dev, &pil_list, list)
		if (!strcmp(dev->desc->name, str))
			return dev;
	return NULL;
}

static struct pil_device *find_peripheral(const char *str)
{
	struct pil_device *dev;

	if (!str)
		return NULL;

	mutex_lock(&pil_list_lock);
	dev = __find_peripheral(str);
	mutex_unlock(&pil_list_lock);

	return dev;
}

#define IOMAP_SIZE SZ_4M

static int load_segment(const struct elf32_phdr *phdr, unsigned num,
		struct pil_device *pil)
{
	int ret, count, paddr;
	char fw_name[30];
	const struct firmware *fw = NULL;
	const u8 *data;

	if (memblock_is_region_memory(phdr->p_paddr, phdr->p_memsz)) {
		dev_err(pil->desc->dev, "Kernel memory would be overwritten");
		return -EPERM;
	}

	if (phdr->p_filesz) {
		snprintf(fw_name, ARRAY_SIZE(fw_name), "%s.b%02d",
				pil->desc->name, num);
		ret = request_firmware(&fw, fw_name, pil->desc->dev);
		if (ret) {
			dev_err(pil->desc->dev, "Failed to locate blob %s\n",
					fw_name);
			return ret;
		}

		if (fw->size != phdr->p_filesz) {
			dev_err(pil->desc->dev,
				"Blob size %u doesn't match %u\n", fw->size,
				phdr->p_filesz);
			ret = -EPERM;
			goto release_fw;
		}
	}

	/* Load the segment into memory */
	count = phdr->p_filesz;
	paddr = phdr->p_paddr;
	data = fw ? fw->data : NULL;
	while (count > 0) {
		int size;
		u8 __iomem *buf;

		size = min_t(size_t, IOMAP_SIZE, count);
		buf = ioremap(paddr, size);
		if (!buf) {
			dev_err(pil->desc->dev, "Failed to map memory\n");
			ret = -ENOMEM;
			goto release_fw;
		}
		memcpy(buf, data, size);
		iounmap(buf);

		count -= size;
		paddr += size;
		data += size;
	}

	/* Zero out trailing memory */
	count = phdr->p_memsz - phdr->p_filesz;
	while (count > 0) {
		int size;
		u8 __iomem *buf;

		size = min_t(size_t, IOMAP_SIZE, count);
		buf = ioremap(paddr, size);
		if (!buf) {
			dev_err(pil->desc->dev, "Failed to map memory\n");
			ret = -ENOMEM;
			goto release_fw;
		}
		memset(buf, 0, size);
		iounmap(buf);

		count -= size;
		paddr += size;
	}

	ret = pil->desc->ops->verify_blob(pil->desc, phdr->p_paddr,
					  phdr->p_memsz);
	if (ret)
		dev_err(pil->desc->dev, "Blob %u failed verification\n", num);

release_fw:
	release_firmware(fw);
	return ret;
}

#define segment_is_hash(flag) (((flag) & (0x7 << 24)) == (0x2 << 24))

static int segment_is_loadable(const struct elf32_phdr *p)
{
	return (p->p_type & PT_LOAD) && !segment_is_hash(p->p_flags);
}

/* Sychronize request_firmware() with suspend */
static DECLARE_RWSEM(pil_pm_rwsem);

static int load_image(struct pil_device *pil)
{
	int i, ret;
	char fw_name[30];
	struct elf32_hdr *ehdr;
	const struct elf32_phdr *phdr;
	const struct firmware *fw;

	down_read(&pil_pm_rwsem);
	snprintf(fw_name, sizeof(fw_name), "%s.mdt", pil->desc->name);
	ret = request_firmware(&fw, fw_name, pil->desc->dev);
	if (ret) {
		dev_err(pil->desc->dev, "Failed to locate %s\n", fw_name);
		goto out;
	}

	if (fw->size < sizeof(*ehdr)) {
		dev_err(pil->desc->dev, "Not big enough to be an elf header\n");
		ret = -EIO;
		goto release_fw;
	}

	ehdr = (struct elf32_hdr *)fw->data;
	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG)) {
		dev_err(pil->desc->dev, "Not an elf header\n");
		ret = -EIO;
		goto release_fw;
	}

	if (ehdr->e_phnum == 0) {
		dev_err(pil->desc->dev, "No loadable segments\n");
		ret = -EIO;
		goto release_fw;
	}
	if (sizeof(struct elf32_phdr) * ehdr->e_phnum +
	    sizeof(struct elf32_hdr) > fw->size) {
		dev_err(pil->desc->dev, "Program headers not within mdt\n");
		ret = -EIO;
		goto release_fw;
	}

	ret = pil->desc->ops->init_image(pil->desc, fw->data, fw->size);
	if (ret) {
		dev_err(pil->desc->dev, "Invalid firmware metadata\n");
		goto release_fw;
	}

	phdr = (const struct elf32_phdr *)(fw->data + sizeof(struct elf32_hdr));
	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		if (!segment_is_loadable(phdr))
			continue;

		ret = load_segment(phdr, i, pil);
		if (ret) {
			dev_err(pil->desc->dev, "Failed to load segment %d\n",
					i);
			goto release_fw;
		}
	}

	ret = pil->desc->ops->auth_and_reset(pil->desc);
	if (ret) {
		dev_err(pil->desc->dev, "Failed to bring out of reset\n");
		goto release_fw;
	}
	dev_info(pil->desc->dev, "brought out of reset\n");

release_fw:
	release_firmware(fw);
out:
	up_read(&pil_pm_rwsem);
	return ret;
}

/**
 * pil_get() - Load a peripheral into memory and take it out of reset
 * @name: pointer to a string containing the name of the peripheral to load
 *
 * This function returns a pointer if it succeeds. If an error occurs an
 * ERR_PTR is returned.
 *
 * If PIL is not enabled in the kernel, the value %NULL will be returned.
 */
void *pil_get(const char *name)
{
	int ret;
	struct pil_device *pil;
	struct pil_device *pil_d;
	void *retval;

	/* PIL is not yet supported on 8064. */
	if (cpu_is_apq8064())
		return NULL;

	pil = retval = find_peripheral(name);
	if (!pil)
		return ERR_PTR(-ENODEV);

	pil_d = find_peripheral(pil->desc->depends_on);
	if (pil_d) {
		void *p = pil_get(pil_d->desc->name);
		if (IS_ERR(p))
			return p;
	}

	mutex_lock(&pil->lock);
	if (pil->count) {
		pil->count++;
		goto unlock;
	}

	ret = load_image(pil);
	if (ret) {
		retval = ERR_PTR(ret);
		goto unlock;
	}

	pil->count++;
unlock:
	mutex_unlock(&pil->lock);
	return retval;
}
EXPORT_SYMBOL(pil_get);

/**
 * pil_put() - Inform PIL the peripheral no longer needs to be active
 * @peripheral_handle: pointer from a previous call to pil_get()
 *
 * This doesn't imply that a peripheral is shutdown or in reset since another
 * driver could be using the peripheral.
 */
void pil_put(void *peripheral_handle)
{
	struct pil_device *pil_d;
	struct pil_device *pil = peripheral_handle;
	if (!pil || IS_ERR(pil))
		return;

	mutex_lock(&pil->lock);
	WARN(!pil->count, "%s: Reference count mismatch\n", __func__);
	/* TODO: Peripheral shutdown support */
	if (pil->count == 1)
		goto unlock;
	if (pil->count)
		pil->count--;
	if (pil->count == 0)
		pil->desc->ops->shutdown(pil->desc);
unlock:
	mutex_unlock(&pil->lock);

	pil_d = find_peripheral(pil->desc->depends_on);
	if (pil_d)
		pil_put(pil_d);
}
EXPORT_SYMBOL(pil_put);

void pil_force_shutdown(const char *name)
{
	struct pil_device *pil;

	pil = find_peripheral(name);
	if (!pil)
		return;

	mutex_lock(&pil->lock);
	if (!WARN(!pil->count, "%s: Reference count mismatch\n", __func__))
		pil->desc->ops->shutdown(pil->desc);
	mutex_unlock(&pil->lock);
}
EXPORT_SYMBOL(pil_force_shutdown);

int pil_force_boot(const char *name)
{
	int ret = -EINVAL;
	struct pil_device *pil;

	pil = find_peripheral(name);
	if (!pil)
		return -EINVAL;

	mutex_lock(&pil->lock);
	if (!WARN(!pil->count, "%s: Reference count mismatch\n", __func__))
		ret = load_image(pil);
	mutex_unlock(&pil->lock);

	return ret;
}
EXPORT_SYMBOL(pil_force_boot);

#ifdef CONFIG_DEBUG_FS
int msm_pil_debugfs_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static ssize_t msm_pil_debugfs_read(struct file *filp, char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	int r;
	char buf[40];
	struct pil_device *pil = filp->private_data;

	mutex_lock(&pil->lock);
	r = snprintf(buf, sizeof(buf), "%d\n", pil->count);
	mutex_unlock(&pil->lock);
	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static ssize_t msm_pil_debugfs_write(struct file *filp,
		const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	struct pil_device *pil = filp->private_data;
	char buf[4];

	if (cnt > sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	if (!strncmp(buf, "get", 3)) {
		if (IS_ERR(pil_get(pil->desc->name)))
			return -EIO;
	} else if (!strncmp(buf, "put", 3))
		pil_put(pil);
	else
		return -EINVAL;

	return cnt;
}

static const struct file_operations msm_pil_debugfs_fops = {
	.open	= msm_pil_debugfs_open,
	.read	= msm_pil_debugfs_read,
	.write	= msm_pil_debugfs_write,
};

static struct dentry *pil_base_dir;

static int msm_pil_debugfs_init(void)
{
	pil_base_dir = debugfs_create_dir("pil", NULL);
	if (!pil_base_dir) {
		pil_base_dir = NULL;
		return -ENOMEM;
	}

	return 0;
}

static int msm_pil_debugfs_add(struct pil_device *pil)
{
	if (!pil_base_dir)
		return -ENOMEM;

	if (!debugfs_create_file(pil->desc->name, S_IRUGO | S_IWUSR,
				pil_base_dir, pil, &msm_pil_debugfs_fops))
		return -ENOMEM;
	return 0;
}
#else
static int msm_pil_debugfs_init(void) { return 0; }
static int msm_pil_debugfs_add(struct pil_device *pil) { return 0; }
#endif

static int msm_pil_shutdown_at_boot(void)
{
	struct pil_device *pil;

	mutex_lock(&pil_list_lock);
	list_for_each_entry(pil, &pil_list, list)
		pil->desc->ops->shutdown(pil->desc);
	mutex_unlock(&pil_list_lock);

	return 0;
}
late_initcall(msm_pil_shutdown_at_boot);

int msm_pil_register(struct pil_desc *desc)
{
	struct pil_device *pil = kzalloc(sizeof(*pil), GFP_KERNEL);
	if (!pil)
		return -ENOMEM;

	mutex_init(&pil->lock);
	INIT_LIST_HEAD(&pil->list);
	pil->desc = desc;

	mutex_lock(&pil_list_lock);
	list_add(&pil->list, &pil_list);
	mutex_unlock(&pil_list_lock);

	return msm_pil_debugfs_add(pil);
}
EXPORT_SYMBOL(msm_pil_register);

static int pil_pm_notify(struct notifier_block *b, unsigned long event, void *p)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		down_write(&pil_pm_rwsem);
		break;
	case PM_POST_SUSPEND:
		up_write(&pil_pm_rwsem);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block pil_pm_notifier = {
	.notifier_call = pil_pm_notify,
};

static int __init msm_pil_init(void)
{
	int ret = msm_pil_debugfs_init();
	if (ret)
		return ret;
	register_pm_notifier(&pil_pm_notifier);
	return ret;
}
arch_initcall(msm_pil_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Load peripheral images and bring peripherals out of reset");
