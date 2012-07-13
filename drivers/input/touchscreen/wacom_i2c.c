/*
 *  wacom_i2c.c - Wacom G5 Digitizer Controller (I2C bus)
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/wacom_i2c.h>
#include <linux/earlysuspend.h>
#include <linux/uaccess.h>
#ifdef EPEN_CPU_LOCK
#include <mach/cpufreq.h>
#endif

#include "wacom_i2c_func.h"
#include "wacom_i2c_flash.h"
#include "wacom_i2c_coord_tables.h"

#define WACOM_FW_PATH "/sdcard/firmware/wacom_firm.bin"


#if defined(CONFIG_USA_MODEL_SGH_I717)
#define HWREV_PEN_PITCH4P4   0x02
#elif defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
#define HWREV_PEN_PITCH4P4   0x05
#endif

extern unsigned int get_hw_rev(void);

/*sec_class sysfs*/
extern struct class *sec_class;
struct device *sec_epen;

struct i2c_client *g_client;

unsigned char screen_rotate=0;
unsigned char user_hand=1;

int wacom_is_pressed;			// to skip touch-key event while wacom is pressed.  Xtopher
EXPORT_SYMBOL(wacom_is_pressed);


#ifdef EPEN_CPU_LOCK
extern bool epen_cpu_lock_status;
#endif
static bool epen_reset_result = false;
static bool epen_checksum_result = false;
char Firmware_checksum_backup[]={0x1F, 0xFF, 0xFF, 0xFF, 0xF5,};   

bool IsWacomEnabled = true;

int wacom_i2c_load_fw(struct i2c_client *client)
{
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret;
	struct wacom_i2c *wac_i2c = i2c_get_clientdata(client);
	unsigned char *Binary_UMS = NULL;    

	Binary_UMS = kmalloc(WACOM_FW_SIZE, GFP_KERNEL);
	if (Binary_UMS == NULL) {
		printk(KERN_DEBUG "[E-PEN] %s, kmalloc failed\n", __func__);
		return -ENOENT;
		}
	Binary = Binary_UMS;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(WACOM_FW_PATH, O_RDONLY, S_IRUSR);

	if (IS_ERR(fp)) {
		printk(KERN_ERR "[E-PEN]: failed to open %s.\n", WACOM_FW_PATH);
		goto open_err;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	printk(KERN_NOTICE "[E-PEN]: start, file path %s, size %ld Bytes\n", WACOM_FW_PATH, fsize);

	if(fsize > WACOM_FW_SIZE) {
	    kfree(Binary_UMS);
		    printk(KERN_ERR "[E-PEN]: UMS file size (%ld) > WACOM_FW_SIZE (%ld)\n", fsize, WACOM_FW_SIZE);	  
		    return -ENOENT;
	}

	nread = vfs_read(fp, (char __user *)Binary, fsize, &fp->f_pos);
	printk(KERN_ERR "[E-PEN]: nread %ld Bytes\n", nread);
	if (nread != fsize) {
		printk(KERN_ERR "[E-PEN]: failed to read firmware file, nread %ld Bytes\n", nread);
		goto read_err;
	}

	ret = wacom_i2c_flash(wac_i2c);
	if (ret < 0) {
		printk(KERN_ERR "[E-PEN]: failed to write firmware(%d)\n", ret);
		goto fw_write_err;
	}

	Binary = Binary_44;
	kfree(Binary_UMS);

	filp_close(fp, current->files);
	set_fs(old_fs);

	return 0;

open_err :
	kfree(Binary_UMS);
	set_fs(old_fs);
	return -ENOENT;

read_err :
	kfree(Binary_UMS);
	filp_close(fp, current->files);
	set_fs(old_fs);
	return -EIO;

fw_write_err :
	kfree(Binary_UMS);
	filp_close(fp, current->files);
	set_fs(old_fs);
	return -1;
}


static irqreturn_t wacom_interrupt(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;
	wacom_i2c_coord(wac_i2c);
	return IRQ_HANDLED;
}

#if defined(WACOM_PDCT_WORK_AROUND)
static irqreturn_t wacom_interrupt_pdct(int irq, void *dev_id)
{
	struct wacom_i2c *wac_i2c = dev_id;

	wac_i2c->pen_pdct = gpio_get_value(wac_i2c->wac_pdata->gpio_pendct);

	printk(KERN_DEBUG "[E-PEN] pdct %d(%d)\n",
		wac_i2c->pen_pdct, wac_i2c->pen_prox);

	if (wac_i2c->pen_pdct == PDCT_NOSIGNAL) {
		/* If rdy is 1, pen is still working*/
		if (wac_i2c->pen_prox == 0)
			forced_release(wac_i2c);
	} else if (wac_i2c->pen_prox == 0)
		forced_hover(wac_i2c);

	return IRQ_HANDLED;
}
#endif

static void wacom_i2c_set_input_values(struct i2c_client *client,
				struct wacom_i2c *wac_i2c,
				struct input_dev *input_dev)
{
	/*Set input values before registering input device*/

	input_dev->name = "sec_e-pen";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->evbit[0] |= BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

	__set_bit(ABS_X, input_dev->absbit);
	__set_bit(ABS_Y, input_dev->absbit);
	__set_bit(ABS_PRESSURE, input_dev->absbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(BTN_TOOL_PEN, input_dev->keybit);
	__set_bit(BTN_TOOL_RUBBER, input_dev->keybit);
	__set_bit(BTN_STYLUS, input_dev->keybit);
	__set_bit(KEY_UNKNOWN, input_dev->keybit);
	/*  __set_bit(BTN_STYLUS2, input_dev->keybit); */
	/*  __set_bit(ABS_MISC, input_dev->absbit); */
}

static int wacom_check_emr_prox(struct wacom_g5_callbacks *cb)
{
	struct wacom_i2c *wac = container_of(cb, struct wacom_i2c, callbacks);
	printk(KERN_DEBUG "[E-PEN]:%s:\n", __func__);

	return wac->pen_prox;
}



static int wacom_i2c_remove(struct i2c_client *client)
{
	struct wacom_i2c *wac_i2c = i2c_get_clientdata(client);
	free_irq(client->irq, wac_i2c);
	input_unregister_device(wac_i2c->input_dev);
	kfree(wac_i2c);

	return 0;
}

static void wacom_i2c_early_suspend(struct early_suspend *h)
{
	struct wacom_i2c *wac_i2c = container_of(h, struct wacom_i2c, early_suspend);

	if(IsWacomEnabled == false){
		printk(KERN_ERR "[E-PEN]: E-PEN have been disabled by menu \n");
		return;
	}


	disable_irq(wac_i2c->client->irq);

#ifdef EPEN_CPU_LOCK

	if (epen_cpu_lock_status) {
		s5pv310_cpufreq_lock_free(DVFS_LOCK_ID_PEN);
		epen_cpu_lock_status = 0;
	}

#endif

#ifdef WACOM_PDCT_WORK_AROUND
	disable_irq(wac_i2c->irq_pdct);
#endif

	/* release pen, if it is pressed*/
#ifdef WACOM_PDCT_WORK_AROUND
	if (wac_i2c->pen_pdct == PDCT_DETECT_PEN)
		forced_release(wac_i2c);
#else
	if(wac_i2c->pen_pressed || wac_i2c->side_pressed)
	{
		input_report_abs(wac_i2c->input_dev, ABS_PRESSURE, 0);
		input_report_key(wac_i2c->input_dev, BTN_STYLUS, 0);
		input_report_key(wac_i2c->input_dev, BTN_TOUCH, 0);
		input_report_key(wac_i2c->input_dev, wac_i2c->tool, 0);
		input_sync(wac_i2c->input_dev);
		pr_info("[E-PEN] is released\n");
	}
#endif
	wac_i2c->wac_pdata->early_suspend_platform_hw();
	printk(KERN_DEBUG "[E-PEN]:%s.\n", __func__);
}

static void wacom_i2c_resume_work(struct work_struct *work)
{
	struct wacom_i2c *wac_i2c =
	    container_of(work, struct wacom_i2c, resume_work);

	enable_irq(wac_i2c->client->irq);
#ifdef WACOM_PDCT_WORK_AROUND
	enable_irq(wac_i2c->irq_pdct);
#endif
	printk(KERN_DEBUG"[E-PEN] %s\n", __func__);
}

static void wacom_i2c_late_resume(struct early_suspend *h)
{
	struct wacom_i2c *wac_i2c = container_of(h, struct wacom_i2c, early_suspend);

	if(IsWacomEnabled == false){
		printk(KERN_ERR "[E-PEN]: E-PEN have been disabled by menu \n");
		return;
	}

	wac_i2c->wac_pdata->late_resume_platform_hw();
	schedule_delayed_work(&wac_i2c->resume_work, HZ / 5); /* 200ms */
	printk(KERN_DEBUG "[E-PEN]:%s.\n", __func__);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
#define wacom_i2c_suspend	NULL
#define wacom_i2c_resume	NULL

#else
static int wacom_i2c_suspend(struct i2c_client *client, pm_message_t mesg)
{
	wacom_i2c_early_suspend();
	printk(KERN_DEBUG "[E-PEN]:%s.\n", __func__);
	return 0;
}

static int wacom_i2c_resume(struct i2c_client *client)
{
	wacom_i2c_late_resume();
	printk(KERN_DEBUG "[E-PEN]:%s.\n", __func__);
	return 0;
}
#endif


static ssize_t epen_firm_update_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);

	printk(KERN_DEBUG "[E-PEN]:%s:(%d)\n", __func__, wac_i2c->wac_feature->firm_update_status);

	if (wac_i2c->wac_feature->firm_update_status == 2) {
		return sprintf(buf, "PASS\n");
	} else if (wac_i2c->wac_feature->firm_update_status == 1) {
		return sprintf(buf, "DOWNLOADING\n");
	} else if (wac_i2c->wac_feature->firm_update_status == -1) {
		return sprintf(buf, "FAIL\n");
	} else {
		return 0;
	}

}

static ssize_t epen_firm_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	printk(KERN_DEBUG "[E-PEN]:%s: 0x%x|0x%X\n", __func__, wac_i2c->wac_feature->fw_version, Firmware_version_of_file);

	return sprintf(buf, "0x%X\t0x%X\n", wac_i2c->wac_feature->fw_version, Firmware_version_of_file);
}

static ssize_t epen_firmware_update_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	u8 *data;
	u8 buf_if;
	int ret;

#if defined(CONFIG_USA_MODEL_SGH_I717)	
	disable_irq(wac_i2c->irq);
#ifdef WACOM_PDCT_WORK_AROUND
	disable_irq(wac_i2c->irq_pdct);
#endif
#endif

	buf_if = COM_QUERY;
	data = wac_i2c->wac_feature->data;
	
	printk(KERN_DEBUG "[E-PEN]:%s:\n", __func__);

#if !defined(CONFIG_USA_MODEL_SGH_I717)
	disable_irq(wac_i2c->irq);
#endif
	wac_i2c->wac_feature->firm_update_status = 1;

	if (*buf == 'F') {
		printk(KERN_ERR "[E-PEN]: Start firmware flashing (UMS).\n");
		ret = wacom_i2c_load_fw(wac_i2c->client);
		if (ret < 0) {
			printk(KERN_ERR "[E-PEN]: failed to flash firmware.\n");
			goto failure;
		}
	} else if (*buf == 'B') {
		printk(KERN_ERR "[E-PEN]: Start firmware flashing (kernel image).\n");
		Binary = Binary_44;
		ret = wacom_i2c_flash(wac_i2c);
		if (ret < 0) {
			printk(KERN_ERR "[E-PEN]: failed to flash firmware.\n");
			goto failure;
		}
	} else {
		printk(KERN_ERR "[E-PEN]: wrong parameter.\n");
		goto param_err;
	}
	printk(KERN_ERR "[E-PEN]: Finish firmware flashing.\n");

	msleep(800);

	wacom_i2c_query(wac_i2c);

	wac_i2c->wac_feature->firm_update_status = 2;

	enable_irq(wac_i2c->irq);
#ifdef WACOM_PDCT_WORK_AROUND
	enable_irq(wac_i2c->irq_pdct);
#endif

	return count;

param_err:

failure:
	wac_i2c->wac_feature->firm_update_status = -1;
	enable_irq(wac_i2c->irq);
#ifdef WACOM_PDCT_WORK_AROUND
	enable_irq(wac_i2c->irq_pdct);
#endif

	return -1;

}

static ssize_t epen_rotation_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	static bool factory_test = false;
	static unsigned char last_rotation = 0;
	unsigned int val;

	sscanf(buf, "%u", &val);

	/* Fix the rotation value to 0(Portrait) when factory test(15 mode) */
	if( val == 100 && !factory_test ) {
		factory_test = true;
		screen_rotate = 0;
		printk(KERN_DEBUG "[E-PEN] %s, enter factory test mode\n", __func__);
	}
	else if( val == 200 && factory_test ) {
		factory_test = false;
		screen_rotate = last_rotation;
		printk(KERN_DEBUG "[E-PEN] %s, exit factory test mode\n", __func__);
	}

	/* converting rotation ::InputManager[ 0:0, 90:1, 180:2, 270:3] start */
	if(val == 2) 
		val = 3;
	else if (val == 3)
		val = 2;
	/*converting rotation :: end*/
	
	if( val >= 0 && val <= 2) {
		if( factory_test )
			last_rotation = val;
		else
			screen_rotate = val;
	}
	
	/* 0: Portrait 0, 1: Landscape 90, 2: Landscape 270 */
	printk(KERN_DEBUG "[E-PEN]:%s: rotate=%d\n", __func__, screen_rotate);

	return count;
}

static ssize_t epen_hand_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int val;

	sscanf(buf, "%u", &val);

	if( val == 0 || val == 1 )
		user_hand = (unsigned char)val;

	/* 0:Left hand, 1:Right Hand */
	printk(KERN_DEBUG "[E-PEN]:%s: hand=%u\n", __func__, user_hand);

	return count;
}


static ssize_t epen_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int ret;
	int val;

	sscanf(buf, "%d", &val);

	if (val == 1) {
		disable_irq(wac_i2c->client->irq);
#ifdef WACOM_PDCT_WORK_AROUND
		disable_irq(wac_i2c->irq_pdct);
#endif
		
		/* reset IC */	
		gpio_set_value(GPIO_PEN_RESET, 0);
		msleep(200);
		gpio_set_value(GPIO_PEN_RESET, 1);
		msleep(200);

		/* I2C Test */
		ret = wacom_i2c_query(wac_i2c);

		enable_irq(wac_i2c->client->irq);
#ifdef WACOM_PDCT_WORK_AROUND
		enable_irq(wac_i2c->irq_pdct);
#endif

		if( ret < 0 )
			epen_reset_result = false;
		else
			epen_reset_result = true;

		printk(KERN_DEBUG "[E-PEN] %s, result %d\n", __func__, epen_reset_result);
	}

	return count;
}

static ssize_t epen_reset_result_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if( epen_reset_result ) {
		printk(KERN_DEBUG "[E-PEN] %s, PASS\n", __func__);
		return sprintf(buf, "PASS\n");
	}
	else {
		printk(KERN_DEBUG "[E-PEN] %s, FAIL\n", __func__);
		return sprintf(buf, "FAIL\n");
	}
}

static ssize_t epen_checksum_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int ret;
	int val;
	int i;
	int retry = 3;
	unsigned char data[6] = {0,};

	sscanf(buf, "%d", &val);
	if( val == 1 && wac_i2c->wac_feature->fw_version >= 0x31E )
	{
 
	 data[0] = COM_CHECKSUM;

	 printk(KERN_DEBUG "[E-PEN] received checksum %x, %x, %x, %x, %x\n", 
					 Firmware_checksum_backup[0], Firmware_checksum_backup[1], Firmware_checksum_backup[2], Firmware_checksum_backup[3], Firmware_checksum_backup[4]);
 
	 for( i = 0 ; i < 5; ++i ) {
		 if( Firmware_checksum_backup[i] != Firmware_checksum[i] ){
			 printk(KERN_DEBUG "[E-PEN] checksum fail %dth %d %d\n", i, Firmware_checksum_backup[i], Firmware_checksum[i]);
			 break;
		 }
	 }
	 if( i == 5 )
		 epen_checksum_result = true;
	 else
		 epen_checksum_result = false;

 
	 printk(KERN_DEBUG "[E-PEN] %s, result %d\n", __func__, epen_checksum_result);
	}

	return count;
}

static ssize_t epen_checksum_result_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if( epen_checksum_result ) {
		printk(KERN_DEBUG "[E-PEN] checksum, PASS\n", __func__);
		return sprintf(buf, "PASS\n");
	}
	else {
		printk(KERN_DEBUG "[E-PEN] checksum, FAIL\n", __func__);
		return sprintf(buf, "FAIL\n");
	}
}

static void epen_checksum_read_atBoot(struct wacom_i2c *wac_i2c)
{
	int ret;
	int val;
	int i,j;
	int retry = 3;
	unsigned char data[6] = {0,};

	{
		disable_irq(wac_i2c->client->irq);
	#ifdef WACOM_PDCT_WORK_AROUND
		disable_irq(wac_i2c->irq_pdct);
	#endif

		data[0] = COM_CHECKSUM;

		while( retry-- )
		{
			ret = i2c_master_send(wac_i2c->client, &data[0], 1);
			if( ret < 0 ){
				printk(KERN_DEBUG "[E-PEN] i2c fail, retry, %d\n", __LINE__);
				continue;
			}

			msleep(200);

			ret = i2c_master_recv(wac_i2c->client, data, 5);
			if( ret < 0 ){
				printk(KERN_DEBUG "[E-PEN] i2c fail, retry, %d\n", __LINE__);
				continue;
			}
			else if( data[0] == 0x1f )
				break;

			printk(KERN_DEBUG "[E-PEN] checksum retry\n");
		}

		if (ret >= 0) {
			printk(KERN_DEBUG "[E-PEN] received checksum %x, %x, %x, %x, %x\n", 
								data[0], data[1], data[2], data[3], data[4]);
			
			for( j = 0 ; j < 5; j++ ){
				Firmware_checksum_backup[j] = data[j];
			}
			
		}

		for( i = 0 ; i < 5; ++i ) {
			if( data[i] != Firmware_checksum[i] ){
				printk(KERN_DEBUG "[E-PEN] checksum fail %dth %d %d\n", i, data[i], Firmware_checksum[i]);
				break;
			}
		}
		if( i == 5 )
			epen_checksum_result = true;
		else
			epen_checksum_result = false;

		enable_irq(wac_i2c->client->irq);
	#ifdef WACOM_PDCT_WORK_AROUND
		enable_irq(wac_i2c->irq_pdct);
	#endif

		printk(KERN_DEBUG "[E-PEN] %s, result %d\n", __func__, epen_checksum_result);
	}

}


/*--------------------------------------------------------*/
static ssize_t epen_disable_show(struct device *dev,
		struct device_attribute *attr, const char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int ret = 0;

	printk(KERN_ERR "[E-PEN]:%s.   S\n", __func__);
	if(IsWacomEnabled == false){
		printk(KERN_ERR "[E-PEN]: E-PEN have been off \n");
		return ret;
	}

	IsWacomEnabled = false;

	disable_irq(wac_i2c->client->irq);
	
	/* release pen, if it is pressed*/
	if(wac_i2c->pen_pressed || wac_i2c->side_pressed)
	{
		input_report_abs(wac_i2c->input_dev, ABS_PRESSURE, 0);
		input_report_key(wac_i2c->input_dev, BTN_STYLUS, 0);
		input_report_key(wac_i2c->input_dev, BTN_TOUCH, 0);
		input_report_key(wac_i2c->input_dev, wac_i2c->tool, 0);
		input_sync(wac_i2c->input_dev);
		pr_info("[E-PEN] is released\n");
	}

	wac_i2c->wac_pdata->early_suspend_platform_hw();

	
	printk(KERN_ERR "[E-PEN]:%s.   E\n", __func__);
	ret = 1;

	return ret;
}


static ssize_t epen_enable_show(struct device *dev,
		struct device_attribute *attr, const char *buf)
{
	struct wacom_i2c *wac_i2c = dev_get_drvdata(dev);
	int ret = 0;

	printk(KERN_ERR "[E-PEN]:%s.   S\n", __func__);
	if(IsWacomEnabled == true){
		printk(KERN_ERR "[E-PEN]: E-PEN have been on \n");
		return ret;		
	}

	wac_i2c->wac_pdata->late_resume_platform_hw();
	schedule_delayed_work(&wac_i2c->resume_work, HZ / 5); /* 200ms */

	IsWacomEnabled = true;

	printk(KERN_ERR "[E-PEN]:%s.   E\n", __func__);

	ret = 1;
	return ret;
}

/*--------------------------------------------------------*/



static DEVICE_ATTR(set_epen_module_off, S_IRUGO | S_IWUSR | S_IWGRP, epen_disable_show, NULL);
static DEVICE_ATTR(set_epen_module_on, S_IRUGO | S_IWUSR | S_IWGRP, epen_enable_show, NULL);


static DEVICE_ATTR(epen_firm_update, S_IWUSR|S_IWGRP, NULL, epen_firmware_update_store);		/* firmware update */
static DEVICE_ATTR(epen_firm_update_status, S_IRUGO, epen_firm_update_status_show, NULL);		/* return firmware update status*/
static DEVICE_ATTR(epen_firm_version, S_IRUGO, epen_firm_version_show, NULL);					/* return firmware version */

static DEVICE_ATTR(epen_rotation, S_IWUSR|S_IWGRP, NULL, epen_rotation_store);					/* screen rotation */
static DEVICE_ATTR(epen_hand, S_IWUSR|S_IWGRP, NULL, epen_hand_store);							/* hand type */


static DEVICE_ATTR(epen_reset, S_IWUSR|S_IWGRP, NULL, epen_reset_store);
static DEVICE_ATTR(epen_reset_result, S_IRUSR|S_IRGRP, epen_reset_result_show, NULL);

static DEVICE_ATTR(epen_checksum, S_IWUSR|S_IWGRP, NULL, epen_checksum_store);					/* For SMD Test. Check checksum */
static DEVICE_ATTR(epen_checksum_result, S_IRUSR|S_IRGRP, epen_checksum_result_show, NULL);

/*
static struct attribute *w8501_attributes[] = {
	&dev_attr_w8501_firmware_in_binary,
	NULL
};
*/

static void init_offset_tables(void)
{
	short hand, rotate;
	if( get_hw_rev() >= HWREV_PEN_PITCH4P4 ) {
		printk("[E-PEN] Use 4.4mm pitch Offset tables\n");
	}else {
		printk("[E-PEN] Use 4.8mm pitch Offset tables\n");
		origin_offset[0] = origin_offset_48[0];
		origin_offset[1] = origin_offset_48[1];
		for( hand = 0 ; hand < 2 ; ++hand) {
			for( rotate = 0 ; rotate < 3 ; ++rotate) {
				tableX[hand][rotate] = tableX_48[hand][rotate];
				tableY[hand][rotate] = tableY_48[hand][rotate];
				tilt_offsetX[hand][rotate] = tilt_offsetX_48[hand][rotate];
				tilt_offsetY[hand][rotate] = tilt_offsetY_48[hand][rotate];
			}
		}
	}
}

static void update_work_func(struct work_struct *work)
{
	int ret;
	int retry = 2;
	struct wacom_i2c *wac_i2c =
	    container_of(work, struct wacom_i2c, update_work);

	printk("[E-PEN] %s\n", __func__);

	disable_irq(wac_i2c->irq);

	while( retry-- ) {
		printk(KERN_DEBUG "[E-PEN]: INIT_FIRMWARE_FLASH is enabled.\n");
	ret = wacom_i2c_flash(wac_i2c);

		if( ret == 0 )
			break;

		printk(KERN_DEBUG "[E-PEN] update failed. retry %d\n", retry);

		/* Reset IC */
	        gpio_set_value(GPIO_PEN_RESET, 0);
	        msleep(200);
	        gpio_set_value(GPIO_PEN_RESET, 1);
		msleep(200);
	}
	msleep(800);
	printk(KERN_DEBUG "[E-PEN]: flashed.(%d)\n", ret);

	wacom_i2c_query(wac_i2c);

	enable_irq(wac_i2c->irq);
}

static int wacom_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct wacom_i2c *wac_i2c;
	struct wacom_g5_platform_data *pdata = client->dev.platform_data;
	int i, ret;
	i = ret = 0;

	printk(KERN_DEBUG "[E-PEN]:%s:\n", __func__);

	/*Check I2C functionality*/
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		goto err3;

	/*Obtain kernel memory space for wacom i2c*/
	wac_i2c = kzalloc(sizeof(struct wacom_i2c), GFP_KERNEL);
	if (wac_i2c == NULL) goto fail;
	wac_i2c->wac_feature = &wacom_feature_EMR;

	pdata->init_platform_hw();


	/*Initializing for semaphor*/
	mutex_init(&wac_i2c->lock);

	/*Register platform data*/
	wac_i2c->wac_pdata = client->dev.platform_data;

	/*Register callbacks*/
	wac_i2c->callbacks.check_prox = wacom_check_emr_prox;
	if (wac_i2c->wac_pdata->register_cb)
		wac_i2c->wac_pdata->register_cb(&wac_i2c->callbacks);

	/*Register wacom i2c to input device*/
	wac_i2c->input_dev = input_allocate_device();
	if (wac_i2c == NULL || wac_i2c->input_dev == NULL)
		goto fail;
	wacom_i2c_set_input_values(client, wac_i2c, wac_i2c->input_dev);

	wac_i2c->client = client;
	wac_i2c->irq = client->irq;
#ifdef WACOM_PDCT_WORK_AROUND
	wac_i2c->irq_pdct = gpio_to_irq(pdata->gpio_pendct);
#endif

	/*Change below if irq is needed*/
	wac_i2c->irq_flag = 1;

#ifdef CONFIG_HAS_EARLYSUSPEND
		wac_i2c->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
		wac_i2c->early_suspend.suspend = wacom_i2c_early_suspend;
		wac_i2c->early_suspend.resume = wacom_i2c_late_resume;
		register_early_suspend(&wac_i2c->early_suspend);
#endif

	/*Init Featreus by hw rev*/
#if defined(CONFIG_USA_MODEL_SGH_I717)
		if( get_hw_rev() == 0x01 ) {
			printk("[E-PEN] Wacom driver is working for 4.4mm pitch pad.\n");
			/* Firmware Feature */
			Firmware_version_of_file = 0x340;
			Binary = Binary_44;		
		}		
		else if( get_hw_rev() >= HWREV_PEN_PITCH4P4 ) {
			printk("[E-PEN] Wacom driver is working for 4.4mm pitch pad.\n");
			/* Firmware Feature */
			Firmware_version_of_file = Firmware_version_of_file_44;
			Binary = Binary_44;
		}
		
#else

	if( get_hw_rev() >= HWREV_PEN_PITCH4P4 ) {
		printk("[E-PEN] Wacom driver is working for 4.4mm pitch pad.\n");

		/* Firmware Feature */
		Firmware_version_of_file = Firmware_version_of_file_44;
		Binary = Binary_44;
	}
	else {
		printk("[E-PEN] Wacom driver is working for 4.8mm pitch pad.\n");

		/* Firmware Feature */
		Firmware_version_of_file = Firmware_version_of_file_48;
		Binary = Binary_48;
	}
#endif


	init_offset_tables();
	INIT_WORK(&wac_i2c->update_work, update_work_func);
	INIT_DELAYED_WORK(&wac_i2c->resume_work, wacom_i2c_resume_work);

	/* Reset IC */
#if defined(CONFIG_USA_MODEL_SGH_I717)	
	gpio_set_value(GPIO_PEN_RESET, 0);
	msleep(200);
	gpio_set_value(GPIO_PEN_RESET, 1);
	msleep(200);
#else
	gpio_set_value(GPIO_PEN_RESET, 0);
	msleep(120);
	gpio_set_value(GPIO_PEN_RESET, 1);
	msleep(15);
#endif
	ret = wacom_i2c_query(wac_i2c);

	if( ret < 0 )
		epen_reset_result = false;
	else
		epen_reset_result = true;

	input_set_abs_params(wac_i2c->input_dev, ABS_X, pdata->min_x,
		pdata->max_x, 4, 0);
	input_set_abs_params(wac_i2c->input_dev, ABS_Y, pdata->min_y,
		pdata->max_y, 4, 0);
	input_set_abs_params(wac_i2c->input_dev, ABS_PRESSURE, pdata->min_pressure,
		pdata->max_pressure, 0, 0);
	input_set_drvdata(wac_i2c->input_dev, wac_i2c);

	/*Set client data*/
	i2c_set_clientdata(client, wac_i2c);

	/*Before registering input device, data in each input_dev must be set*/
	if (input_register_device(wac_i2c->input_dev))
		goto err2;

	g_client = client;

	/*  if(wac_i2c->irq_flag) */
	/*   disable_irq(wac_i2c->irq); */

	sec_epen= device_create(sec_class, NULL, 0, NULL, "sec_epen");
	dev_set_drvdata(sec_epen, wac_i2c);

	if (IS_ERR(sec_epen))
			printk(KERN_ERR "Failed to create device(sec_epen)!\n");

	if (device_create_file(sec_epen, &dev_attr_epen_firm_update)< 0)
			printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_epen_firm_update.attr.name);

	if (device_create_file(sec_epen, &dev_attr_epen_firm_update_status)< 0)
			printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_epen_firm_update_status.attr.name);

	if (device_create_file(sec_epen, &dev_attr_epen_firm_version)< 0)
			printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_epen_firm_version.attr.name);

	if (device_create_file(sec_epen, &dev_attr_epen_rotation)< 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_epen_rotation.attr.name);

	if (device_create_file(sec_epen, &dev_attr_epen_hand)< 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_epen_hand.attr.name);

	if (device_create_file(sec_epen, &dev_attr_epen_reset)< 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_epen_reset.attr.name);

	if (device_create_file(sec_epen, &dev_attr_epen_reset_result)< 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_epen_reset_result.attr.name);

	if (device_create_file(sec_epen, &dev_attr_epen_checksum)< 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_epen_checksum.attr.name);

	if (device_create_file(sec_epen, &dev_attr_epen_checksum_result)< 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_epen_checksum_result.attr.name);
	

	if (device_create_file(sec_epen, &dev_attr_set_epen_module_off) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_epen_module_off.attr.name);

	if (device_create_file(sec_epen, &dev_attr_set_epen_module_on) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n", dev_attr_set_epen_module_on.attr.name);


	
	/*Request IRQ*/
	if (wac_i2c->irq_flag) {
		ret = request_threaded_irq(wac_i2c->irq, NULL, wacom_interrupt, IRQF_DISABLED|IRQF_TRIGGER_RISING|IRQF_ONESHOT, wac_i2c->name, wac_i2c);
		if (ret < 0)
			goto err1;
	}
#if defined(WACOM_PDCT_WORK_AROUND)
		ret =
			request_threaded_irq(wac_i2c->irq_pdct, NULL,
					wacom_interrupt_pdct,
					IRQF_DISABLED | IRQF_TRIGGER_RISING |
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					wac_i2c->name, wac_i2c);
		if (ret < 0) {
			printk(KERN_ERR
				"[E-PEN]: failed to request irq(%d) - %d\n",
				wac_i2c->irq_pdct, ret);
			goto err1;
		}
#endif

	/* firmware update */
	printk(KERN_NOTICE "[E-PEN] wacom fw ver : 0x%x, new fw ver : 0x%x\n",
		wac_i2c->wac_feature->fw_version, Firmware_version_of_file);
	if( wac_i2c->wac_feature->fw_version < Firmware_version_of_file ) 
	{
		#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
		printk("[E-PEN] %s\n", __func__);
		
		disable_irq(wac_i2c->irq);
		
		printk(KERN_NOTICE "[E-PEN]: INIT_FIRMWARE_FLASH is enabled.\n");
		ret = wacom_i2c_flash(wac_i2c);
		msleep(800);
		printk(KERN_ERR "[E-PEN]: flashed.(%d)\n", ret);
		
		wacom_i2c_query(wac_i2c);
		
		enable_irq(wac_i2c->irq);
		#else
		schedule_work(&wac_i2c->update_work);
#endif
	}

	/* To send exact checksum data at sleep state ... Xtopher */
	printk(KERN_ERR"[E-PEN]: Verify CHECKSUM.\n");
	epen_checksum_read_atBoot(wac_i2c);
	msleep(20);

	return 0;

err3:
	printk(KERN_ERR "[E-PEN]: No I2C functionality found\n");
	return -ENODEV;

err2:
	printk(KERN_ERR "[E-PEN]: err2 occured\n");
	input_free_device(wac_i2c->input_dev);
	return -EIO;

err1:
	printk(KERN_ERR "[E-PEN]: err1 occured(num:%d)\n", ret);
	input_free_device(wac_i2c->input_dev);
	wac_i2c->input_dev = NULL;
	return -EIO;

fail:
	printk(KERN_ERR "[E-PEN]: fail occured\n");
	return -ENOMEM;
}



static const struct i2c_device_id wacom_i2c_id[] = {
	{"wacom_g5sp_i2c", 0},
	{},
};

/*Create handler for wacom_i2c_driver*/
static struct i2c_driver wacom_i2c_driver = {
	.driver = {
		.name = "wacom_g5sp_i2c",
	},
	.probe = wacom_i2c_probe,
	.remove = wacom_i2c_remove,
	.suspend = wacom_i2c_suspend,
	.resume = wacom_i2c_resume,
	.id_table = wacom_i2c_id,
};


#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

static int __init wacom_i2c_init(void)
{
	int ret = 0;

#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return ENODEV!\n", __func__);
		return 0;
	}
#endif

#if defined(WACOM_SLEEP_WITH_PEN_SLP)
	printk(KERN_ERR "[E-PEN]: %s: Sleep type-PEN_SLP pin\n", __func__);
#elif defined(WACOM_SLEEP_WITH_PEN_LDO_EN)
	printk(KERN_ERR "[E-PEN]: %s: Sleep type-PEN_LDO_EN pin\n", __func__);
#endif

	ret = i2c_add_driver(&wacom_i2c_driver);
	if (ret)
		printk(KERN_ERR "[E-PEN]: fail to i2c_add_driver\n");
	return ret;

}

static void __exit wacom_i2c_exit(void)
{
	i2c_del_driver(&wacom_i2c_driver);
}


late_initcall(wacom_i2c_init);
module_exit(wacom_i2c_exit);

MODULE_AUTHOR("Samsung");
MODULE_DESCRIPTION("Driver for Wacom G5SP Digitizer Controller");

MODULE_LICENSE("GPL");
