/*
 *  drivers/misc/sec_param.c
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sec_param.h>

#define PARAM_RD	0
#define PARAM_WR	1

#define SEC_PARAM_FILE_NAME	"/dev/block/mmcblk0p12"	/* parameter block */
#define SEC_PARAM_FILE_SIZE	0xA00000		/* 10MB */
#define SEC_PARAM_FILE_OFFSET	(SEC_PARAM_FILE_SIZE - 0x100000)	

/* single global instance */
sec_param_data *param_data = NULL;

static bool param_sec_operation(void *value, int offset, int size, int direction)
{
	/* Read from PARAM(parameter) partition  */
	struct file *filp;
	mm_segment_t fs;
	int ret = true;
	int flag = (direction == PARAM_WR) ? (O_RDWR | O_SYNC) : O_RDONLY;

	pr_debug("%s %x %x %d %d\n", __func__, value, offset, size, direction);

	filp = filp_open(SEC_PARAM_FILE_NAME, flag, 0);

	if (IS_ERR(filp)) {
		pr_err("%s: filp_open failed. (%ld)\n", __func__, PTR_ERR(filp));
		return false;
	}

	fs = get_fs();
	set_fs(get_ds());

	ret = filp->f_op->llseek(filp, offset, SEEK_SET);
	if (ret < 0) {
		pr_err("%s FAIL LLSEEK \n", __func__);
		ret = false;
		goto param_sec_debug_out;
	}

	if (direction == PARAM_RD) {
		ret = filp->f_op->read(filp, (char __user *)value, size, &filp->f_pos);
	} else if (direction == PARAM_WR) {
		ret = filp->f_op->write(filp, (char __user *)value, size, &filp->f_pos);
	}

param_sec_debug_out:
	set_fs(fs);
	filp_close(filp, NULL);
	return ret;
}

bool sec_open_param(void)
{
	int ret = true;
	int offset = SEC_PARAM_FILE_OFFSET;

	if (param_data != NULL)
		return true;

	param_data = kmalloc(sizeof(sec_param_data), GFP_KERNEL);
	
	ret = param_sec_operation(param_data, offset, sizeof(sec_param_data), PARAM_RD);

	if (!ret) {
		kfree(param_data);
		param_data = NULL;
		pr_err("%s PARAM OPEN FAIL\n", __func__);
		return false;
	}

#if 0
	printk("************* PARAM ***************\n");
	printk("0x%x\n", param_data->debuglevel);
	printk("0x%x\n", param_data->uartsel);
	printk("0x%x\n", param_data->rory_control);
	printk("************* PARAM ***************\n");
#endif

	return ret;

}
EXPORT_SYMBOL(sec_open_param);

bool sec_get_param(sec_param_index index, void *value)
{
	int ret = true;
	ret = sec_open_param();
	if (!ret)
		return ret;
	
	switch (index) {
	case param_index_debuglevel:
		memcpy(value, &(param_data->debuglevel), sizeof(unsigned int));
		break;
	case param_index_uartsel:
		memcpy(value, &(param_data->uartsel), sizeof(unsigned int));
		break;
	case param_rory_control:
		memcpy(value, &(param_data->rory_control), sizeof(unsigned int));
		break;
	default:
		return false;
	}

	return true;
}
EXPORT_SYMBOL(sec_get_param);

bool sec_set_param(sec_param_index index, void *value)
{
	int ret = true;
	int offset = SEC_PARAM_FILE_OFFSET;

	ret = sec_open_param();
	if (!ret)
		return ret;
	
	switch (index) {
	case param_index_debuglevel:
		memcpy(&(param_data->debuglevel), value, sizeof(unsigned int));
		break;
	case param_index_uartsel:
		memcpy(&(param_data->uartsel), value, sizeof(unsigned int));
		break;
	case param_rory_control:
		memcpy(&(param_data->rory_control), value, sizeof(unsigned int));
		break;
	default:
		return false;
	}

	return param_sec_operation(param_data, offset, sizeof(sec_param_data), PARAM_WR);
}
EXPORT_SYMBOL(sec_set_param);

