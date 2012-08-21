/* drivers/i2c/chips/a2220.c - a2220 voice processor driver
 *
 * Copyright (C) 2009 HTC Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/a2220.h>
#include <linux/kthread.h>
#include "a2220_firmware.h"

#define MODULE_NAME "audience_a2220 :"
#define DEBUG			(0)
#define ENABLE_DIAG_IOCTLS	(0)
#define WAKEUP_GPIO_NUM_HERCULES_REV01 33
#define WAKEUP_GPIO_NUM_CELOX_ATT_REV05 33

#ifdef CONFIG_EUR_MODEL_GT_I9210
#define phonecall_Bandtype_Init		0x804E0C80
#define phonecall_Bandtype_NB		0x804C0000
#define phonecall_Bandtype_WB		0x804C0001
#define GetRouteChangeStatus 		0x804F0000

static int a2220_WB;
#endif

static struct i2c_client *this_client;
static struct a2220_platform_data *pdata;
static struct task_struct *task;
static int execute_cmdmsg(unsigned int);

static struct mutex a2220_lock;
static int a2220_opened;
static int a2220_suspended;
// lsj +
static int control_a2220_clk = 0;
int a2220_ioctl2(unsigned int cmd , unsigned long arg);
// lsj -
static unsigned int a2220_NS_state = A2220_NS_STATE_AUTO;
static int a2220_current_config = A2220_PATH_SUSPEND;
static int a2220_param_ID;

extern unsigned int get_hw_rev(void);

struct vp_ctxt {
	unsigned char *data;
	unsigned int img_size;
};

struct vp_ctxt the_vp;

static int a2220_i2c_read(char *rxData, int length)
{
	int rc;
	struct i2c_msg msgs[] = {
		{
			.addr = this_client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = rxData,
		},
	};

	rc = i2c_transfer(this_client->adapter, msgs, 1);
	if (rc < 0) {
		printk("%s: transfer error %d\n", __func__, rc);
		return rc;
	}

#if DEBUG
	{
		int i = 0;
		for (i = 0; i < length; i++)
			pr_info("%s: rx[%d] = %2x\n", __func__, i, rxData[i]);
	}
#endif

	return 0;
}

static int a2220_i2c_write(char *txData, int length)
{
	int rc;
	struct i2c_msg msg[] = {
		{
			.addr = this_client->addr,
			.flags = 0,
			.len = length,
			.buf = txData,
		},
	};

	rc = i2c_transfer(this_client->adapter, msg, 1);
	if (rc < 0) {
		printk("%s: transfer error %d\n", __func__, rc);
		return rc;
	}

#if DEBUG
	{
		int i = 0;
		for (i = 0; i < length; i++)
			pr_info("%s: tx[%d] = %2x\n", __func__, i, txData[i]);
	}
#endif

	return 0;
}

static int a2220_open(struct inode *inode, struct file *file)
{
	int rc = 0;
	struct vp_ctxt *vp = &the_vp;

	mutex_lock(&a2220_lock);

	if (a2220_opened) {
		printk("%s: busy\n", __func__);
		rc = -EBUSY;
		goto done;
	}

	file->private_data = vp;
	vp->img_size = 0;
	a2220_opened = 1;
done:
	mutex_unlock(&a2220_lock);
	return rc;
}

static int a2220_release(struct inode *inode, struct file *file)
{
	mutex_lock(&a2220_lock);
	a2220_opened = 0;
	mutex_unlock(&a2220_lock);

	return 0;
}

#ifdef AUDIENCE_BYPASS //(+)dragonball Multimedia mode
#define A100_msg_mutimedia1		0x801C0000 //VoiceProcessingOn, 0x0000:off
#define A100_msg_mutimedia2		0x8026001F //SelectRouting, 0x001A:(26)
#define A100_msg_mutimedia3   0x800C0B03 // ; PCM B Din delay 1bit
#define A100_msg_mutimedia4   0x800D0001
#define A100_msg_mutimedia5   0x800C0A03 // ; PCM A Din delay 1bit
#define A100_msg_mutimedia6   0x800D0001
#endif 

static void a2220_i2c_sw_reset(unsigned int reset_cmd)
{
	int rc = 0;
	unsigned char msgbuf[4];

	msgbuf[0] = (reset_cmd >> 24) & 0xFF;
	msgbuf[1] = (reset_cmd >> 16) & 0xFF;
	msgbuf[2] = (reset_cmd >> 8) & 0xFF;
	msgbuf[3] = reset_cmd & 0xFF;

	pr_info("%s: %08x\n", __func__, reset_cmd);

	rc = a2220_i2c_write(msgbuf, 4);
	if (!rc)
		msleep(20);
}

static ssize_t  a2220_hw_reset(struct a2220img *img)
{
	struct a2220img *vp = img;
	int rc,i, pass = 0;
	int remaining;
	int retry = RETRY_CNT;
	unsigned char *index;
	char buf[2];

	printk("%s \n",__func__);
	while (retry--) {
		/* Reset A2220 chip */
		gpio_set_value(pdata->gpio_a2220_reset, 0);

		/* Enable A2220 clock */
		if (control_a2220_clk)
			gpio_set_value(pdata->gpio_a2220_clk, 1);
		mdelay(1);

		/* Take out of reset */
		gpio_set_value(pdata->gpio_a2220_reset, 1);

		msleep(50); /* Delay before send I2C command */

		/* Boot Cmd to A2220 */
		buf[0] = A2220_msg_BOOT >> 8;
		buf[1] = A2220_msg_BOOT & 0xff;

		rc = a2220_i2c_write(buf, 2);
		if (rc < 0) {
			printk("%s: set boot mode error (%d retries left)\n",
					__func__, retry);
			continue;
		}
		
		mdelay(1); 
		rc = a2220_i2c_read(buf, 1);
		if (rc < 0) {
			printk("%s: boot mode ack error (%d retries left)\n",
					__func__, retry);
			continue;
		}		
		
		remaining = vp->img_size / 32;
		index = vp->buf;
				
		for (; remaining; remaining--, index += 32) {
			rc = a2220_i2c_write(index, 32);
			if (rc < 0)
				break;
		}

		if (rc >= 0 && vp->img_size % 32)
			rc = a2220_i2c_write(index, vp->img_size % 32);
		
		if (rc < 0) {
			printk("%s: fw load error %d (%d retries left)\n",
					__func__, rc, retry);
			continue;
		}
		
		msleep(20); /* Delay time before issue a Sync Cmd */

		for (i=0; i<10 ; i++)		
			msleep(20);		

		rc = execute_cmdmsg(A100_msg_Sync);
		if (rc < 0) {
			printk("%s: sync command error %d (%d retries left)\n",
					__func__, rc, retry);
			continue;
		}
		
		pass = 1;
		break;
	}
	return rc;
}
///////////////////////////////////////////////////////////////
// eS305B HPT 
#ifdef CONFIG_USA_MODEL_SGH_I717
unsigned char hpt_init_macro [] = {
/*
	0x80, 0x0C, 0x0A, 0x07, 0x80, 0x0D, 0x00, 0x01, // 0x800C:SetDeviceParmID, 0x0A:PCM0, 0x07:PCM Audio Port Mode, 0x800D:SetDeviceParm, 0x0001:I2S
	0x80, 0x0C, 0x0B, 0x07, 0x80, 0x0D, 0x00, 0x01, // 0x800C:SetDeviceParmID, 0x0B:PCM1, 0x07:PCM Audio Port Mode, 0x800D:SetDeviceParm, 0x0001:I2S
	0x80, 0x0C, 0x0C, 0x07, 0x80, 0x0D, 0x00, 0x01, // 0x800C:SetDeviceParmID, 0x0C:PCM2, 0x07:PCM Audio Port Mode, 0x800D:SetDeviceParm, 0x0001:I2S
	0x80, 0x0C, 0x0D, 0x07, 0x80, 0x0D, 0x00, 0x01, // 0x800C:SetDeviceParmID, 0x0D:PCM3, 0x07:PCM Audio Port Mode, 0x800D:SetDeviceParm, 0x0001:I2S
	0x80, 0x0C, 0x0A, 0x03, 0x80, 0x0D, 0x00, 0x01, // 0x800C:SetDeviceParmID, 0x0A:PCM0, 0x03:PCM DelFromFsRx, 0x800D:SetDeviceParm, 0x0001:(1 clock)
	0x80, 0x0C, 0x0B, 0x03, 0x80, 0x0D, 0x00, 0x01, // 0x800C:SetDeviceParmID, 0x0B:PCM1, 0x03:PCM DelFromFsRx, 0x800D:SetDeviceParm, 0x0001:(1 clock)
	0x80, 0x0C, 0x0C, 0x03, 0x80, 0x0D, 0x00, 0x01, // 0x800C:SetDeviceParmID, 0x0C:PCM2, 0x03:PCM DelFromFsRx, 0x800D:SetDeviceParm, 0x0001:(1 clock)
	0x80, 0x0C, 0x0D, 0x03, 0x80, 0x0D, 0x00, 0x01, // 0x800C:SetDeviceParmID, 0x0D:PCM3, 0x03:PCM DelFromFsRx, 0x800D:SetDeviceParm, 0x0001:(1 clock)
*/
0x80, 0x1C, 0x00, 0x01, // 0x80, 0x1C:VoiceProcessingOn, , 0x00, 0x01:On
0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x00, // Microphone Configuration, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x00:2-mic Close Talk (CT)
//0x80, 0x26, 0x00, 0x26, // 0x80, 0x26:SelectRouting, , 0x00, 0x26: 26
0x80, 0x1B, 0x00, 0x04, // 0x80, 0x1B:SetDigitalInputGain, , 0x00, 0x:PCM-A left, 0x04:(4 dB)
0x80, 0x1B, 0x01, 0x04, // 0x80, 0x1B:SetDigitalInputGain, 0x01:PCM-A right, 0x04:(4 dB)
0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x06, // Tx Noise Suppression Level, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x06:Level 6
0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x00, // AEC Mode, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x00:AEC off
0x80, 0x17, 0x00, 0x12, 0x80, 0x18, 0x00, 0x00, // Downlink Speaker Volume, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x00:(0 dB)
0x80, 0x17, 0x00, 0x23, 0x80, 0x18, 0x00, 0x00, // Use AEC Comfort Noise Fill, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x00:No
0x80, 0x17, 0x00, 0x34, 0x80, 0x18, 0x00, 0x00, // Echo Suppression Enhancement, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x00:(0 dB)
0x80, 0x17, 0x00, 0x2E, 0x80, 0x18, 0x00, 0x00, // AEC Comfort Noise Gain, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x00:(0 dB)
0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00, // Use AGC, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x00:No
0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00, // Use Rx AGC, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x00:No
0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x00, // Far End Noise Suppression, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x00:Off
0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x03, // Rx Noise Suppression Level, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x03:Level 3
0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, // Tx ComfortNoise, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x00:No
//0x80, 0x17, 0x00, 0x4F, 0x80, 0x18, 0x00, 0x00, // 0x80, 0x17:SetAlgorithmParmID, , 0x00, 0x4F:Use Bandwidth Expansion, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x00:no
0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x00, // 0x80, 0x17:SetAlgorithmParmID, , 0x00, 0x20:Tx PostEq Mode, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x00:off
0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, // 0x80, 0x17:SetAlgorithmParmID, , 0x00, 0x1F:Rx PostEq Mode, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x00:Off
0x80, 0x17, 0x00, 0x30, 0x80, 0x18, 0x00, 0x00, // 0x80, 0x17:SetAlgorithmParmID, , 0x00, 0x30:Tx MBC Mode, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x00:Off
0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x00, // 0x80, 0x17:SetAlgorithmParmID, , 0x00, 0x31:Rx MBC Mode, 0x80, 0x18:SetAlgorithmParm, , 0x00, 0x00:Off


};

unsigned char hpt_sleep [] = {
	0x80, 0x52, 0x00, 0x48, // 0x8052:
	0x80, 0x52, 0x00, 0x5C, // 0x8052:
	0x80, 0x10, 0x00, 0x01, // sleep
};

static int hpt_longCmd_execute(unsigned char *i2c_cmds, int size){

	int i = 0, rc = 0;
	int retry = 4;
	unsigned int sw_reset = 0;
	unsigned int msg;
	unsigned char *pMsg;

	pMsg = (unsigned char*)&msg;

	for(i=0 ; i < size ; i+=4)
	{
		pMsg[3] = i2c_cmds[i];
		pMsg[2] = i2c_cmds[i+1];
		pMsg[1] = i2c_cmds[i+2];
		pMsg[0] = i2c_cmds[i+3];
		
		do {
			rc = execute_cmdmsg(msg);
		} while ((rc < 0) && --retry);

		// Audience not responding to execute_cmdmsg , doing HW reset of the chipset
		/*
		if((retry == 0)&&(rc < 0))
		{
		        struct a2220img img;
        		img.buf = a2220_firmware_buf;
        		img.img_size = sizeof(a2220_firmware_buf);
			rc=a2220_hw_reset(&img);		// Call if the Audience chipset is not responding after retrying 12 times
			if(rc < 0)
			{
				printk(MODULE_NAME "%s ::  Audience HW Reset Failed\n");
				return rc;
			}
		}
		*/
		
	}

}
#endif
///////////////////////////////////////////////////////////////

static ssize_t a2220_bootup_init(struct a2220img *pImg)
	// lsj -
	//static ssize_t a2220_bootup_init(struct file *file, struct a2220img *img)
{
	// lsj
	//struct vp_ctxt *vp = file->private_data;
	struct a2220img *vp = pImg;
	//lsj
	int rc, pass = 0;
	int remaining;
	int retry = RETRY_CNT;
	unsigned char *index;
	char buf[2];

	// lsj + 
#if 0
	if (img->img_size > A2220_MAX_FW_SIZE) {
		printk("%s: invalid a2220 image size %d\n", __func__,
				img->img_size);
		return -EINVAL;
	}

	vp->data = kmalloc(img->img_size, GFP_KERNEL);
	if (!vp->data) {
		printk("%s: out of memory\n", __func__);
		return -ENOMEM;
	}
	vp->img_size = img->img_size;
	if (copy_from_user(vp->data, img->buf, img->img_size)) {
		printk("%s: copy from user failed\n", __func__);
		kfree(vp->data);
		return -EFAULT;
	}
#endif
	// lsj -
	mdelay(100);
	printk(MODULE_NAME "%s : lsj::a2220_bootup_init +\n", __func__);
	while (retry--) {
		/* Reset A2220 chip */
		gpio_set_value(pdata->gpio_a2220_reset, 0);

		/* Enable A2220 clock */
		if (control_a2220_clk)
			gpio_set_value(pdata->gpio_a2220_clk, 1);
		mdelay(1);

		/* Take out of reset */
		gpio_set_value(pdata->gpio_a2220_reset, 1);

		msleep(50); /* Delay before send I2C command */

		/* Boot Cmd to A2220 */
		buf[0] = A2220_msg_BOOT >> 8;
		buf[1] = A2220_msg_BOOT & 0xff;
		printk(MODULE_NAME "%s : lsj::a2220_bootup_init 1\n", __func__);

		rc = a2220_i2c_write(buf, 2);
		if (rc < 0) {
			printk("%s: set boot mode error (%d retries left)\n",
					__func__, retry);
			continue;
		}
		printk(MODULE_NAME "%s : lsj::a2220_bootup_init 2\n", __func__);

		mdelay(1); 
		rc = a2220_i2c_read(buf, 1);
		if (rc < 0) {
			printk("%s: boot mode ack error (%d retries left)\n",
					__func__, retry);
			continue;
		}
		printk(MODULE_NAME "%s : lsj::a2220_bootup_init 3\n", __func__);


		printk(MODULE_NAME "%s : lsj::a2220_bootup_init 4\n", __func__);
		//lsj +
		remaining = vp->img_size / 32;
		index = vp->buf;
		//lsj -
		pr_info("%s: starting to load image (%d passes)...\n",
				__func__,
				remaining + !!(vp->img_size % 32));

		for (; remaining; remaining--, index += 32) {
			rc = a2220_i2c_write(index, 32);
			if (rc < 0)
				break;
		}

		printk(MODULE_NAME "%s : lsj::a2220_bootup_init 5\n", __func__);
		if (rc >= 0 && vp->img_size % 32)
			rc = a2220_i2c_write(index, vp->img_size % 32);

		printk(MODULE_NAME "%s : lsj::a2220_bootup_init 6\n", __func__);
		if (rc < 0) {
			printk("%s: fw load error %d (%d retries left)\n",
					__func__, rc, retry);
			continue;
		}
		printk(MODULE_NAME "%s : lsj::a2220_bootup_init 7\n", __func__);

		msleep(20); /* Delay time before issue a Sync Cmd */

		pr_info("%s: firmware loaded successfully\n", __func__);

		msleep(200); // lsj

		rc = execute_cmdmsg(A100_msg_Sync);
		if (rc < 0) {
			printk("%s: sync command error %d (%d retries left)\n",
					__func__, rc, retry);
			continue;
		}
		printk(MODULE_NAME "%s : lsj::a2220_bootup_init 8\n", __func__);

		pass = 1;
		break;
	}
	printk(MODULE_NAME "%s : get_hw_rev of Target = %d\n", __func__, get_hw_rev());
#ifdef AUDIENCE_BYPASS //(+)dragonball Multimedia mode
#ifdef CONFIG_USA_MODEL_SGH_I717
		/*
		// HPT INIT
		rc = hpt_longCmd_execute(&hpt_init_macro, sizeof(hpt_init_macro));
		if (rc < 0) {
			printk(MODULE_NAME "%s: htp init error\n", __func__);
		}
		*/
		// HPT SLEEP 
		rc = hpt_longCmd_execute(&hpt_sleep, sizeof(hpt_sleep));
		if (rc < 0) {
			printk(MODULE_NAME "%s: htp sleep error\n", __func__);
		}else{
			a2220_suspended = 1;
		}
#endif
	//printk("get_hw_rev of TMO = %d\n", get_hw_rev());
	if (get_hw_rev() < 0x05){	
		//A100_msg_mutimedia1
		rc = execute_cmdmsg(A100_msg_mutimedia1);
		if (rc < 0) {
			printk(MODULE_NAME "%s: A100_msg_mutimedia1 error\n", __func__);
			//goto set_suspend_err;
		}
		rc = execute_cmdmsg(A100_msg_mutimedia2);
		if (rc < 0) {
			printk(MODULE_NAME "%s: A100_msg_mutimedia2 error\n", __func__);
			//goto set_suspend_err;
		}

		rc = execute_cmdmsg(A100_msg_mutimedia3);
		if (rc < 0) {
			printk(MODULE_NAME "%s: A100_msg_mutimedia3 error\n", __func__);
			//goto set_suspend_err;
		}
		rc = execute_cmdmsg(A100_msg_mutimedia4);
		if (rc < 0) {
			printk(MODULE_NAME "%s: A100_msg_mutimedia4 error\n", __func__);
			//goto set_suspend_err;
		}

		rc = execute_cmdmsg(A100_msg_mutimedia5);
		if (rc < 0) {
			printk(MODULE_NAME "%s: A100_msg_mutimedia5 error\n", __func__);
			//goto set_suspend_err;
		}
		rc = execute_cmdmsg(A100_msg_mutimedia6);
		if (rc < 0) {
			printk(MODULE_NAME "%s: A100_msg_mutimedia6 error\n", __func__);
			//goto set_suspend_err;
		}

		printk(MODULE_NAME "%s : a2220_bootup_init:: setting Multimedia mode\n", __func__);
	}
	else
	{
#ifdef CONFIG_USA_MODEL_SGH_I577
		a2220_ioctl2(A2220_SET_CONFIG , A2220_PATH_BYPASS_MULTIMEDIA);
#else
		rc = execute_cmdmsg(A100_msg_Sleep);
		if (rc < 0) {
			printk("%s: suspend error\n", __func__);
			goto set_suspend_err;
		}
		a2220_suspended = 1;
		a2220_current_config = A2220_PATH_SUSPEND;	
		msleep(120);
		/* Disable A2220 clock */
		if (control_a2220_clk)
			gpio_set_value(pdata->gpio_a2220_clk, 0);

set_suspend_err:
		if (pass && !rc)
			pr_info("%s: initialized!\n", __func__);
		else
			printk("%s: initialization failed\n", __func__);
#endif
	}

#else //(+)dragonball Multimedia mode

	printk(MODULE_NAME "%s : lsj::a2220_bootup_init 9\n", __func__);
	/* Put A2220 into sleep mode */
	rc = execute_cmdmsg(A100_msg_Sleep);
	if (rc < 0) {
		printk("%s: suspend error\n", __func__);
		goto set_suspend_err;
	}

	printk(MODULE_NAME "%s : lsj::a2220_bootup_init 10\n", __func__);
	a2220_suspended = 1;
	a2220_current_config = A2220_PATH_SUSPEND;	
#if defined(CONFIG_EUR_MODEL_GT_I9210)
	a2220_WB = A2220_PATH_NARROWBAND;
#endif
	//#endif
	msleep(120);
	/* Disable A2220 clock */
	if (control_a2220_clk)
		gpio_set_value(pdata->gpio_a2220_clk, 0);

	printk(MODULE_NAME "%s : lsj::a2220_bootup_init 11\n", __func__);
set_suspend_err:
	if (pass && !rc)
		pr_info("%s: initialized!\n", __func__);
	else
		printk("%s: initialization failed\n", __func__);

#endif // AUDIENCE_BYPASS //(+)dragonball Multimedia mode

	printk(MODULE_NAME "%s : a2220_bootup_init - finish\n", __func__);

	return rc;
}

unsigned char phonecall_receiver_nson[] = {
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
	0x80, 0x1C, 0x00, 0x01, 													//; VoiceProcessing On
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x00, 							//; 2-mic Close Talk (CT)
	0x80, 0x26, 0x00, 0x1A, 													//; SelectRouting, 0x001A:Pri,Sec,Fei,Zro,Zro,Zro,Zro,Zro - Snk,Snk,Snk,Snk,Csp,Snk,Feo,Snk
	0x80, 0x1B, 0x00, 0x09, 													//; SetDigitalInputGain, 0x00:PCM-A left, 0x09:(9 dB)
	0x80, 0x1B, 0x01, 0x03, 													//; SetDigitalInputGain, 0x01:PCM-A right, 0x06:(3 dB)
	0x80, 0x15, 0x04, 0xF8, 													//; SetDigitalOutputGain, 0x04:PCM-C left, 0xF8:(-8 dB)
	0x80, 0x15, 0x05, 0xF8, 													//; SetDigitalOutputGain, 0x05:PCM-C right, 0xF8:(-8 dB)
	0x80, 0x1B, 0x02, 0x00, 													//; SetDigitalInputGain, 0x02:PCM-B left, 0x00:(0 dB)
	0x80, 0x1B, 0x03, 0x00, 													//; SetDigitalInputGain, 0x03:PCM-B right, 0x00:(0 dB)
	0x80, 0x15, 0x06, 0x00, 													//; SetDigitalOutputGain, 0x06:PCM-D left, 0x00:(0 dB)
	0x80, 0x15, 0x07, 0x00, 													//; SetDigitalOutputGain, 0x07:PCM-D right, 0x00:(0 dB)
	0x80, 0x17, 0x00, 0x42, 0x80, 0x18, 0xFF, 0xFD, 							//;Tx-in  Limiter Max Level (dB), 0x8018:SetAlgorithmParm, 0xFFFD:(-3 dB)
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x06, 							//; Tx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0006:Level 6
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x00, 							//;Far End Noise Suppression, 0x8018:SetAlgorithmParm, 0x0000:OFF (auto select mode)
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x03, 							//;Rx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0003:Level 3
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00, 							//;Use AGC, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00, 							//; Use Rx AGC, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x00, 							//;Rx MBC Mode, 0x8018:SetAlgorithmParm, 0x0000:OFF
	0x80, 0x17, 0x00, 0x09, 0x80, 0x18, 0x00, 0x00, 							//;Speaker Enhancement Mode, 0x8018:SetAlgorithmParm, 0x0000:SE Off (HPF only)
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x01, 							//;Tx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0001:On
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, 							//;Rx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, 							//;Use Tx ComfortNoise, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x01, 							//; AEC Mode, 0x8018:SetAlgorithmParm, 0x0001:AEC On (auto select mode)
	0x80, 0x17, 0x00, 0x12, 0x80, 0x18, 0xFF, 0xFF, 							//; Downlink Speaker Volume, 0x8018:SetAlgorithmParm, 0xFFFF:(-1 dB)
	0x80, 0x17, 0x00, 0x34, 0x80, 0x18, 0x00, 0x01, 							//; Echo Suppression Enhancement, 0x8018:SetAlgorithmParm, 0x0001:(1 dB)
	0x80, 0x17, 0x00, 0x23, 0x80, 0x18, 0x00, 0x00, 							//; aec comfortnoise off
	0x80, 0x17, 0x00, 0x1E, 0x80, 0x18, 0x00, 0x02, 							//; PST2
#elif defined(CONFIG_EUR_MODEL_GT_I9210)
	// IJ 110905_HW tuned values - 3_Celox Euro_CT_0904.txt
	0x80, 0x00, 0x00, 0x00, // ; 0x8000:Sync, 0x0000:None
	0x80, 0x1C, 0x00, 0x01, // ; 0x801C:VoiceProcessingOn, 0x0001:On
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0002:Microphone Configuration, 0x8018:SetAlgorithmParm, 0x0000:2-mic Close Talk (CT)
	0x80, 0x26, 0x00, 0x1A, // ; 0x8026:SelectRouting, 0x001A:Pri,Sec,Fei,Zro,Zro,Zro,Zro,Zro - Snk,Snk,Snk,Snk,Csp,Snk,Feo,Snk
	0x80, 0x1B, 0x00, 0x0c, // ; 0x801B:SetDigitalInputGain, 0x00:PCM-A left, 0x0c:(12 dB)
	0x80, 0x1B, 0x01, 0x08, // ; 0x801B:SetDigitalInputGain, 0x01:PCM-A right, 0x08:(8 dB)
	0x80, 0x15, 0x04, 0xF5, // ; 0x8015:SetDigital outGain, 0x04:PCM-C left, 0xF5:(-11 dB)
	0x80, 0x15, 0x05, 0xF5, // ; 0x8015:SetDigital outtGain, 0x05:PCM-C right, 0xF5:(-11 dB)
	0x80, 0x1B, 0x02, 0x00, // ; 0x801B:SetDigitalInputGain, 0x02:PCM-B left, 0x00:(0 dB)
	0x80, 0x1B, 0x03, 0x00, // ; 0x801B:SetDigitalInputGain, 0x03:PCM-B right, 0x00:(0 dB)
	0x80, 0x15, 0x06, 0x00, // ; 0x801B:SetDigitaloutputGain, 0x06:PCM-D left, 0x00:(0 dB)
	0x80, 0x15, 0x07, 0x00, // ; 0x801B:SetDigitaloutputGain, 0x07:PCM-D right, 0x00:(0 dB)
	0x80, 0x17, 0x00, 0x42, 0x80, 0x18, 0xFF, 0xFD, // ; 0x8017:SetAlgorithmParmID, 0x0042:Tx-in  Limiter Max Level (dB), 0x8018:SetAlgorithmParm, 0xFFFD:(-3 dB)
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x08, // ; 0x8017:SetAlgorithmParmID, 0x004B:Tx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0008:Level 8
	0x80, 0x17, 0x00, 0x15, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0015:Side Tone Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0003:AEC Mode, 0x8018:SetAlgorithmParm, 0x0001:AEC Off
	0x80, 0x17, 0x00, 0x12, 0x80, 0x18, 0xFF, 0xF0, // ; DSV -16
	0x80, 0x17, 0x00, 0x34, 0x80, 0x18, 0x00, 0x06, // ; ESE 6
	0x80, 0x17, 0x00, 0x23, 0x80, 0x18, 0x00, 0x00, // ; AEC comfort noise OFF
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0004:Use AGC, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x05, 0x80, 0x18, 0xFF, 0xE3, // ; Taregt -29
	0x80, 0x17, 0x00, 0x06, 0x80, 0x18, 0xFF, 0xC4, // ; NS floor -60
	0x80, 0x17, 0x00, 0x07, 0x80, 0x18, 0x00, 0x00, // ; SNR 0
	0x80, 0x17, 0x00, 0x26, 0x80, 0x18, 0x00, 0x01, // ; up rate 1
	0x80, 0x17, 0x00, 0x27, 0x80, 0x18, 0x00, 0x01, // ;down rate 1
	0x80, 0x17, 0x01, 0x00, 0x80, 0x18, 0x00, 0x05, // ; max gain 5
	0x80, 0x17, 0x00, 0x3E, 0x80, 0x18, 0x00, 0x0A, // ; guard band 10
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0028:Use Rx AGC, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x09, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0009:Speaker Enhancement Mode, 0x8018:SetAlgorithmParm, 0x0000:SE Off (HPF only)
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x01, // ; 0x8017:SetAlgorithmParmID, 0x000E:Far End Noise Suppression, 0x8018:SetAlgorithmParm, 0x0001:On (auto select mode)
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x03, // ; 0x8017:SetAlgorithmParmID, 0x004C:Rx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0003:Level 3
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0020:Tx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x001F:Rx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x30, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0030:Tx MBC Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0031:Rx MBC Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x001A:Use Tx ComfortNoise, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x1E, 0x80, 0x18, 0x00, 0x02, // ; PST 2

#elif defined(CONFIG_USA_MODEL_SGH_I717)
	0x80, 0x1C, 0x00, 0x01, //, 0x80, 0x1C:VoiceProcessingOn,, 0x00, 0x01:On
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x00, // Microphone Configuration,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:2-mic Close Talk (CT)
	0x80, 0x26, 0x00, 0x1A, //, 0x80, 0x26:SelectRouting,, 0x00, 0x1A: 26
	0x80, 0x1B, 0x00, 0x06, //, 0x80, 0x1B:SetDigitalInputGain,, 0x00, 0x:PCM-A left, 0x06:(6 dB)
	0x80, 0x1B, 0x01, 0x03, //, 0x80, 0x1B:SetDigitalInputGain, 0x01:PCM-A right, 0x03:(3 dB)
	0x80, 0x15, 0x04, 0xF9, // SetDigitalOutputGain, 0x04:PCM-C left, 0xF9:(-7 dB)
	0x80, 0x15, 0x05, 0xF9, // setgitalOutputGain, 0x05:PCM-C right, 0xF9:(-7 dB)
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x08, // Tx Noise Suppression Level,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x08:Level 8
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x01, // AEC Mode,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x01:AEC ON
	0x80, 0x17, 0x00, 0x12, 0x80, 0x18, 0x00, 0x00, // Downlink Speaker Volume,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:(0 dB)
	0x80, 0x17, 0x00, 0x23, 0x80, 0x18, 0x00, 0x00, // Use AEC Comfort Noise Fill,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:No
	0x80, 0x17, 0x00, 0x34, 0x80, 0x18, 0x00, 0x00, // Echo Suppression Enhancement,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:(0 dB)
	0x80, 0x17, 0x00, 0x2E, 0x80, 0x18, 0x00, 0x00, // AEC Comfort Noise Gain,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:(0 dB)
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00, // Use AGC,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:No
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00, // Use Rx AGC,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:No
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x00, // Far End Noise Suppression,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:Off
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x03, // Rx Noise Suppression Level,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x03:Level 3
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, // Tx ComfortNoise,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:No
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x01, //, 0x80, 0x17:SetAlgorithmParmID,, 0x00, 0x20:Tx PostEq Mode,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x01:on
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, //, 0x80, 0x17:SetAlgorithmParmID,, 0x00, 0x1F:Rx PostEq Mode,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:off
	0x80, 0x17, 0x00, 0x30, 0x80, 0x18, 0x00, 0x00, //, 0x80, 0x17:SetAlgorithmParmID,, 0x00, 0x30:Tx MBC Mode,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:Off
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x00, //, 0x80, 0x17:SetAlgorithmParmID,, 0x00, 0x31:Rx MBC Mode,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:Off
	0x80, 0x1B, 0x02, 0xFD, // 0x801B:SetDigitalInputGain, 0x02:PCM-B left, 0xF7:(-3 dB)
	0x80, 0x1B, 0x03, 0xFD, // 0x801B:SetDigitalInputGain, 0x03:PCM-B right, 0xF7:(-3 dB)

#else
	0x80, 0x00, 0x00, 0x00, // 0x8000:Sync, 0x0000:None
	0x80, 0x1C, 0x00, 0x01, // 0x801C:VoiceProcessingOn, 0x0001:On
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0002:Microphone Configuration, 0x8018:SetAlgorithmParm, 0x0000:2-mic Close Talk (CT)
	0x80, 0x26, 0x00, 0x1A, // 0x8026:SelectRouting, 0x001A:Pri,Sec,Fei,Zro,Zro,Zro,Zro,Zro - Snk,Snk,Snk,Snk,Csp,Snk,Feo,Snk
	0x80, 0x1B, 0x00, 0x0c, // 0x801B:SetDigitalInputGain, 0x00:PCM-A left, 0x12:(18 dB)
	0x80, 0x1B, 0x01, 0x08, // 0x801B:SetDigitalInputGain, 0x01:PCM-A right, 0x0F:(15 dB)
	0x80, 0x15, 0x04, 0xF5, // 0x8015:SetDigitaloutGain, 0x04:PCM-C left, 0xEF:(-17 dB)
	0x80, 0x15, 0x05, 0xF5, // 0x8015:SetDigitaloutGain, 0x05:PCM-C right, 0xEF:(-17 dB)
	0x80, 0x1B, 0x02, 0x00, // 0x801B:SetDigitalInputGain, 0x02:PCM-B left, 0x00:(0 dB)
	0x80, 0x1B, 0x03, 0x00, // 0x801B:SetDigitalInputGain, 0x03:PCM-B right, 0x00:(0 dB)
	0x80, 0x15, 0x06, 0x00, // 0x801B:SetDigitaloutGain, 0x06:PCM-D left, 0x00:(0 dB)
	0x80, 0x15, 0x07, 0x00, // 0x801B:SetDigitaloutGain, 0x07:PCM-D right, 0x00:(0 dB)
	0x80, 0x17, 0x00, 0x42, 0x80, 0x18, 0xFF, 0xFD, // 0x8017:SetAlgorithmParmID, 0x0042:Tx-in  Limiter Max Level (dB), 0x8018:SetAlgorithmParm, 0xFFFD:(-3 dB)
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x06, // 0x8017:SetAlgorithmParmID, 0x004B:Tx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0006:Level 6
	0x80, 0x17, 0x00, 0x15, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0015:Side Tone Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0003:AEC Mode, 0x8018:SetAlgorithmParm, 0x0000:AEC Off
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0004:Use AGC, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0028:Use Rx AGC, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x09, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0009:Speaker Enhancement Mode, 0x8018:SetAlgorithmParm, 0x0000:SE Off (HPF only)
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x01, // 0x8017:SetAlgorithmParmID, 0x000E:Far End Noise Suppression, 0x8018:SetAlgorithmParm, 0x0001:On (auto select mode)
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x03, // 0x8017:SetAlgorithmParmID, 0x004C:   Rx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0003:Level 3
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0020:Tx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x001F:Rx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x30, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0030:Tx MBC Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x01, // 0x8017:SetAlgorithmParmID, 0x0031:Rx MBC Mode, 0x8018:SetAlgorithmParm, 0x0001:On
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x001A:Use Tx ComfortNoise, 0x8018:SetAlgorithmParm, 0x0000:No
//	0x80, 0x17, 0x00, 0x4F, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x004F:Use Bandwidth Expansion, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x1E, 0x80, 0x18, 0x00, 0x01, //; 0x8017:SetAlgorithmParmID, 0x001E:Position/Suppression Tradeoff, 0x8018:SetAlgorithmParm,0x0001:(1)
#endif
};

unsigned char phonecall_receiver_nsoff[] = {
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
	0x80, 0x1C, 0x00, 0x01, 													//; VoiceProcessing On
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x00, 							//; 2-mic Close Talk (CT)
	0x80, 0x26, 0x00, 0x1A, 													//; SelectRouting, 0x001A:Pri,Sec,Fei,Zro,Zro,Zro,Zro,Zro - Snk,Snk,Snk,Snk,Csp,Snk,Feo,Snk
	0x80, 0x1B, 0x00, 0x09, 													//; SetDigitalInputGain, 0x00:PCM-A left, 0x09:(9 dB)
	0x80, 0x1B, 0x01, 0x03, 													//; SetDigitalInputGain, 0x01:PCM-A right, 0x06:(3 dB)
	0x80, 0x15, 0x04, 0xF8, 													//; SetDigitalOutputGain, 0x04:PCM-C left, 0xF8:(-8 dB)
	0x80, 0x15, 0x05, 0xF8, 													//; SetDigitalOutputGain, 0x05:PCM-C right, 0xF8:(-8 dB)
	0x80, 0x1B, 0x02, 0x00, 													//; SetDigitalInputGain, 0x02:PCM-B left, 0x00:(0 dB)
	0x80, 0x1B, 0x03, 0x00, 													//; SetDigitalInputGain, 0x03:PCM-B right, 0x00:(0 dB)
	0x80, 0x15, 0x06, 0x00, 													//; SetDigitalOutputGain, 0x06:PCM-D left, 0x00:(0 dB)
	0x80, 0x15, 0x07, 0x00, 													//; SetDigitalOutputGain, 0x07:PCM-D right, 0x00:(0 dB)
	0x80, 0x17, 0x00, 0x42, 0x80, 0x18, 0xFF, 0xFD, 							//;Tx-in  Limiter Max Level (dB), 0x8018:SetAlgorithmParm, 0xFFFD:(-3 dB)
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x03, 							//; Tx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0003:Level 3
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x00, 							//;Far End Noise Suppression, 0x8018:SetAlgorithmParm, 0x0000:OFF (auto select mode)
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x03, 							//;Rx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0003:Level 3
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00, 							//;Use AGC, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00, 							//; Use Rx AGC, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x00, 							//;Rx MBC Mode, 0x8018:SetAlgorithmParm, 0x0000:OFF
	0x80, 0x17, 0x00, 0x09, 0x80, 0x18, 0x00, 0x00, 							//;Speaker Enhancement Mode, 0x8018:SetAlgorithmParm, 0x0000:SE Off (HPF only)
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x01, 							//;Tx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0001:On
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, 							//;Rx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, 							//;Use Tx ComfortNoise, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x01, 							//; AEC Mode, 0x8018:SetAlgorithmParm, 0x0001:AEC On (auto select mode)
	0x80, 0x17, 0x00, 0x12, 0x80, 0x18, 0xFF, 0xFF, 							//; Downlink Speaker Volume, 0x8018:SetAlgorithmParm, 0xFFFF:(-1 dB)
	0x80, 0x17, 0x00, 0x34, 0x80, 0x18, 0x00, 0x01, 							//; Echo Suppression Enhancement, 0x8018:SetAlgorithmParm, 0x0001:(1 dB)
	0x80, 0x17, 0x00, 0x23, 0x80, 0x18, 0x00, 0x00, 							//; aec comfortnoise off
	0x80, 0x17, 0x00, 0x1E, 0x80, 0x18, 0x00, 0x02, 							//; PST2
#elif defined(CONFIG_EUR_MODEL_GT_I9210)
	// IJ 110905_HW tuned values - 3_Celox Euro_CT_NS OFF_0904.txt
	0x80, 0x00, 0x00, 0x00, // ; 0x8000:Sync, 0x0000:None
	0x80, 0x1C, 0x00, 0x01, // ; 0x801C:VoiceProcessingOn, 0x0001:On
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0002:Microphone Configuration, 0x8018:SetAlgorithmParm, 0x0000:2-mic Close Talk (CT)
	0x80, 0x26, 0x00, 0x1A, // ; 0x8026:SelectRouting, 0x001A:Pri,Sec,Fei,Zro,Zro,Zro,Zro,Zro - Snk,Snk,Snk,Snk,Csp,Snk,Feo,Snk
	0x80, 0x1B, 0x00, 0x0c, // ; 0x801B:SetDigitalInputGain, 0x00:PCM-A left, 0x0c:(12 dB)
	0x80, 0x1B, 0x01, 0x08, // ; 0x801B:SetDigitalInputGain, 0x01:PCM-A right, 0x08:(8 dB)
	0x80, 0x15, 0x04, 0xF5, // ; 0x8015:SetDigital outGain, 0x04:PCM-C left, 0xF5:(-11 dB)
	0x80, 0x15, 0x05, 0xF5, // ; 0x8015:SetDigital outtGain, 0x05:PCM-C right, 0xF5:(-11 dB)
	0x80, 0x1B, 0x02, 0x00, // ; 0x801B:SetDigitalInputGain, 0x02:PCM-B left, 0x00:(0 dB)
	0x80, 0x1B, 0x03, 0x00, // ; 0x801B:SetDigitalInputGain, 0x03:PCM-B right, 0x00:(0 dB)
	0x80, 0x15, 0x06, 0x00, // ; 0x801B:SetDigitaloutputGain, 0x06:PCM-D left, 0x00:(0 dB)
	0x80, 0x15, 0x07, 0x00, // ; 0x801B:SetDigitaloutputGain, 0x07:PCM-D right, 0x00:(0 dB)
	0x80, 0x17, 0x00, 0x42, 0x80, 0x18, 0xFF, 0xFD, // ; 0x8017:SetAlgorithmParmID, 0x0042:Tx-in  Limiter Max Level (dB), 0x8018:SetAlgorithmParm, 0xFFFD:(-3 dB)
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x03, // ; 0x8017:SetAlgorithmParmID, 0x004B:Tx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0008:Level 3
	0x80, 0x17, 0x00, 0x15, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0015:Side Tone Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x01, // ; 0x8017:SetAlgorithmParmID, 0x0003:AEC Mode, 0x8018:SetAlgorithmParm, 0x0001:AEC On
	0x80, 0x17, 0x00, 0x12, 0x80, 0x18, 0xFF, 0xF0, // ; DSV -16
	0x80, 0x17, 0x00, 0x34, 0x80, 0x18, 0x00, 0x06, // ; ESE 6
	0x80, 0x17, 0x00, 0x23, 0x80, 0x18, 0x00, 0x00, // ; AEC comfort noise OFF
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x01, // ; 0x8017:SetAlgorithmParmID, 0x0004:Use AGC, 0x8018:SetAlgorithmParm, 0x0001:Yes
	0x80, 0x17, 0x00, 0x05, 0x80, 0x18, 0xFF, 0xE3, // ; Taregt -29
	0x80, 0x17, 0x00, 0x06, 0x80, 0x18, 0xFF, 0xC4, // ; NS floor -60
	0x80, 0x17, 0x00, 0x07, 0x80, 0x18, 0x00, 0x00, // ; SNR 0
	0x80, 0x17, 0x00, 0x26, 0x80, 0x18, 0x00, 0x01, // ; up rate 1
	0x80, 0x17, 0x00, 0x27, 0x80, 0x18, 0x00, 0x01, // ;down rate 1
	0x80, 0x17, 0x01, 0x00, 0x80, 0x18, 0x00, 0x05, // ; max gain 5
	0x80, 0x17, 0x00, 0x3E, 0x80, 0x18, 0x00, 0x0A, // ; guard band 10
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0028:Use Rx AGC, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x09, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0009:Speaker Enhancement Mode, 0x8018:SetAlgorithmParm, 0x0000:SE Off (HPF only)
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x01, // ; 0x8017:SetAlgorithmParmID, 0x000E:Far End Noise Suppression, 0x8018:SetAlgorithmParm, 0x0001:On (auto select mode)
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x03, // ; 0x8017:SetAlgorithmParmID, 0x004C:Rx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0003:Level 3
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0020:Tx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x001F:Rx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x30, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0030:Tx MBC Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x0031:Rx MBC Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, // ; 0x8017:SetAlgorithmParmID, 0x001A:Use Tx ComfortNoise, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x1E, 0x80, 0x18, 0x00, 0x02, // ; PST 2
#elif defined(CONFIG_USA_MODEL_SGH_I717)
	0x80, 0x1C, 0x00, 0x00, //, 0x80, 0x1C:VoiceProcessingOn,, 0x00, 0x01:Off
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x00, // Microphone Configuration,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:2-mic Close Talk (CT)
	0x80, 0x26, 0x00, 0x1A, //, 0x80, 0x26:SelectRouting,, 0x00, 0x1A: 26
	0x80, 0x1B, 0x00, 0x04, //, 0x80, 0x1B:SetDigitalInputGain,, 0x00, 0x:PCM-A left, 0x04:(4 dB)
	0x80, 0x1B, 0x01, 0x04, //, 0x80, 0x1B:SetDigitalInputGain, 0x01:PCM-A right, 0x04:(4 dB)
	0x80, 0x15, 0x04, 0x03, // SetDigitalOutputGain, 0x04:PCM-C left, 0x03:(3 dB)
	0x80, 0x15, 0x05, 0x03, // setgitalOutputGain, 0x05:PCM-C right, 0x03:(3 dB)
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x06, // Tx Noise Suppression Level,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x06:Level 6
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x00, // AEC Mode,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:AEC off
	0x80, 0x17, 0x00, 0x12, 0x80, 0x18, 0x00, 0x00, // Downlink Speaker Volume,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:(0 dB)
	0x80, 0x17, 0x00, 0x23, 0x80, 0x18, 0x00, 0x00, // Use AEC Comfort Noise Fill,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:No
	0x80, 0x17, 0x00, 0x34, 0x80, 0x18, 0x00, 0x00, // Echo Suppression Enhancement,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:(0 dB)
	0x80, 0x17, 0x00, 0x2E, 0x80, 0x18, 0x00, 0x00, // AEC Comfort Noise Gain,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:(0 dB)
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00, // Use AGC,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:No
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00, // Use Rx AGC,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:No
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x00, // Far End Noise Suppression,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:Off
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x03, // Rx Noise Suppression Level,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x03:Level 3
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, // Tx ComfortNoise,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:No
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x01, //, 0x80, 0x17:SetAlgorithmParmID,, 0x00, 0x20:Tx PostEq Mode,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x01:on
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, //, 0x80, 0x17:SetAlgorithmParmID,, 0x00, 0x1F:Rx PostEq Mode,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:off
	0x80, 0x17, 0x00, 0x30, 0x80, 0x18, 0x00, 0x00, //, 0x80, 0x17:SetAlgorithmParmID,, 0x00, 0x30:Tx MBC Mode,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:Off
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x00, //, 0x80, 0x17:SetAlgorithmParmID,, 0x00, 0x31:Rx MBC Mode,, 0x80, 0x18:SetAlgorithmParm,, 0x00, 0x00:Off
#else
	0x80, 0x00, 0x00, 0x00, // 0x8000:Sync, 0x0000:None
	0x80, 0x1C, 0x00, 0x01, // 0x801C:VoiceProcessingOn, 0x0001:On
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0002:Microphone Configuration, 0x8018:SetAlgorithmParm, 0x0000:2-mic Close Talk (CT)
	0x80, 0x26, 0x00, 0x1A, // 0x8026:SelectRouting, 0x001A:Pri,Sec,Fei,Zro,Zro,Zro,Zro,Zro - Snk,Snk,Snk,Snk,Csp,Snk,Feo,Snk
	0x80, 0x1B, 0x00, 0x0c, // 0x801B:SetDigitalInputGain, 0x00:PCM-A left, 0x12:(18 dB)
	0x80, 0x1B, 0x01, 0x08, // 0x801B:SetDigitalInputGain, 0x01:PCM-A right, 0x0F:(15 dB)
	0x80, 0x15, 0x04, 0xF5, // 0x8015:SetDigitalInputGain, 0x04:PCM-C left, 0xEF:(-17 dB)
	0x80, 0x15, 0x05, 0xF5, // 0x8015:SetDigitalInputGain, 0x05:PCM-C right, 0xEF:(-17 dB)
	0x80, 0x1B, 0x02, 0x00, // 0x801B:SetDigitalInputGain, 0x02:PCM-B left, 0x00:(0 dB)
	0x80, 0x1B, 0x03, 0x00, // 0x801B:SetDigitalInputGain, 0x03:PCM-B right, 0x00:(0 dB)
	0x80, 0x15, 0x06, 0x00, // 0x801B:SetDigitalInputGain, 0x06:PCM-D left, 0x00:(0 dB)
	0x80, 0x15, 0x07, 0x00, // 0x801B:SetDigitalInputGain, 0x07:PCM-D right, 0x00:(0 dB)
	0x80, 0x17, 0x00, 0x42, 0x80, 0x18, 0xFF, 0xFD, // 0x8017:SetAlgorithmParmID, 0x0042:Tx-in  Limiter Max Level (dB), 0x8018:SetAlgorithmParm, 0xFFFD:(-3 dB)
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x03, // 0x8017:SetAlgorithmParmID, 0x004B:Tx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0006:Level 6
	0x80, 0x17, 0x00, 0x15, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0015:Side Tone Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0003:AEC Mode, 0x8018:SetAlgorithmParm, 0x0000:AEC Off
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0004:Use AGC, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0028:Use Rx AGC, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x09, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0009:Speaker Enhancement Mode, 0x8018:SetAlgorithmParm, 0x0000:SE Off (HPF only)
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x01, // 0x8017:SetAlgorithmParmID, 0x000E:Far End Noise Suppression, 0x8018:SetAlgorithmParm, 0x0001:On (auto select mode)
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x03, // 0x8017:SetAlgorithmParmID, 0x004C:   Rx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0003:Level 3
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0020:Tx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x001F:Rx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x30, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x0030:Tx MBC Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x01, // 0x8017:SetAlgorithmParmID, 0x0031:Rx MBC Mode, 0x8018:SetAlgorithmParm, 0x0001:On
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x001A:Use Tx ComfortNoise, 0x8018:SetAlgorithmParm, 0x0000:No
//	0x80, 0x17, 0x00, 0x4F, 0x80, 0x18, 0x00, 0x00, // 0x8017:SetAlgorithmParmID, 0x004F:Use Bandwidth Expansion, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x1E, 0x80, 0x18, 0x00, 0x01, //; 0x8017:SetAlgorithmParmID, 0x001E:Position/Suppression Tradeoff, 0x8018:SetAlgorithmParm,0x0001:(1)
#endif
};

#ifdef CONFIG_VP_A2220
unsigned char bypass_multimedia[] = {
#ifdef CONFIG_USA_MODEL_SGH_I717
	0x80, 0x52, 0x00, 0x48, // 0x8052:
	0x80, 0x52, 0x00, 0x5C, // 0x8052:
	0x80, 0x10, 0x00, 0x01, // sleep
#elif defined (CONFIG_USA_MODEL_SGH_I577)
	0x80, 0x52, 0x00, 0xE2, // 0x8052:
	0x80, 0x52, 0x00, 0xDD, // 0x8052:
	0x80, 0x10, 0x00, 0x01, // sleep
#else
	//hercules_eS305_SPK[1].txt
	0x80, 0x1C, 0x00, 0x00, 												//; 0x801C:VoiceProcessingOn, 0x0000:off
	0x80, 0x26, 0x00, 0x1F, 												// ; 0x8015:SetDigitalOutputGain, 0x05:PCM-B right, 0xA6:(30 dB)
	0x80, 0x0C, 0x0B, 0x03, 0x80, 0x0D, 0x00, 0x01, // ; PCM B Din delay 1bit
	0x80, 0x0C, 0x0A, 0x03, 0x80, 0x0D, 0x00, 0x01, // ; PCM A Din delay 1bit
#endif
};
#endif



unsigned char phonecall_headset[] = {
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
	0x80, 0x1C, 0x00, 0x01, 										  //; 0x801C:VoiceProcessingOn, 0x0001:On
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x03, 				  //; 0x8017:SetAlgorithmParmID, 0x0002:Microphone Configuration, 0x8018:SetAlgorithmParm, 0x0003:1-mic MD
	0x80, 0x26, 0x00, 0x1A, 										  //; 0x8026:SelectRouting, 0x001A:(26)
	0x80, 0x1B, 0x00, 0x00, 										  //;port a left in gain 0db
	0x80, 0x1B, 0x01, 0xA6, 										  //;port a right -90db
	0x80, 0x15, 0x04, 0x00, 										  //; port c left out odb
	0x80, 0x15, 0x05, 0x00, 										  //; port c Rout 0db
	0x80, 0x1B, 0x02, 0x00, 										  //; Port B Lin 0db
	0x80, 0x1B, 0x03, 0x00, 										  //; Port B Rin 0db
	0x80, 0x15, 0x06, 0x00, 										  //; Port D Lout 0db
	0x80, 0x15, 0x07, 0x00, 										  //; Port D Rout 0db
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x00, 				  //; Ns level 0
	0x80, 0x17, 0x00, 0x15, 0x80, 0x18, 0x00, 0x00, 				  //;side tone OFF
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x00, 				  //; AEC OFF
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00, 				  //; Tx AGC Off
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00, 				  //; Rx AGC Off
	0x80, 0x17, 0x00, 0x29, 0x80, 0x18, 0xFF, 0xE6, 				  //; Target level -26db
	0x80, 0x17, 0x00, 0x2A, 0x80, 0x18, 0xFF, 0xC5, 				  //; noise floor ; -59db
	0x80, 0x17, 0x00, 0x2B, 0x80, 0x18, 0x00, 0x00, 				  //; SNR 0
	0x80, 0x17, 0x00, 0x2C, 0x80, 0x18, 0x00, 0x00, 				  //; up 0
	0x80, 0x17, 0x00, 0x2C, 0x80, 0x18, 0x00, 0x00, 				  //; down 0
	0x80, 0x17, 0x01, 0x02, 0x80, 0x18, 0x00, 0x05, 				  //; max gain 5
	0x80, 0x17, 0x00, 0x3F, 0x80, 0x18, 0x00, 0x0A, 				  //; guar 10dB
	
	0x80, 0x17, 0x00, 0x09, 0x80, 0x18, 0x00, 0x00, 				  //; VEQ OFF
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x00, 				  //;Rx NS Off
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x03, 				  //;Rx NS levle 3
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x00, 				  //; Tx post EQ off
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, 				  //; Rx post EQ off
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, 				  //; Tx comport noise off
//	0x80, 0x17, 0x00, 0x4F, 0x80, 0x18, 0x00, 0x00, 				  //;BWE OFF
	0x80, 0x17, 0x00, 0x30, 0x80, 0x18, 0x00, 0x00, 				  //; Tx MBC Off
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x00, 				  //; Rx MBC Off
#elif defined(CONFIG_EUR_MODEL_GT_I9210)
	0x80, 0x26, 0x00, 0x1A, /* SelectRouting, 0x0015:Snk,Pri,Snk,Snk - Csp,Zro,Zro (none) */
	0x80, 0x1C, 0x00, 0x00, /* VoiceProcessingOn, 0x0000:No */
	0x80, 0x1B, 0x00, 0x12, /* SetDigitalInputGain, 0x00:Primay Mic (Tx), 0x12:(18 dB) */
	0x80, 0x15, 0x00, 0xF8, /* SetDigitalOutputGain, 0x00:Tx, 0xF8:(-8 dB) */
	0x80, 0x0C, 0x0A, 0x04, 0x80, 0x0D, 0x00, 0x00, // ; PCMA latch 0
	0x80, 0x0C, 0x0A, 0x02, 0x80, 0x0D, 0x00, 0x10, // ; PCMA tx delay 16
	0x80, 0x0C, 0x0A, 0x03, 0x80, 0x0D, 0x00, 0x10, // ; PCMA Rx Delay 16
	0x80, 0x0C, 0x0B, 0x04, 0x80, 0x0D, 0x00, 0x00, // ; PCMB latch 0
	0x80, 0x0C, 0x0B, 0x02, 0x80, 0x0D, 0x00, 0x10, // ; PCMB tx delay 16
	0x80, 0x0C, 0x0B, 0x03, 0x80, 0x0D, 0x00, 0x10, // ; PCMB Rx delay 16
	0x80, 0x0C, 0x0C, 0x04, 0x80, 0x0D, 0x00, 0x00, // ; PCMC Latch 0
	0x80, 0x0C, 0x0C, 0x02, 0x80, 0x0D, 0x00, 0x10, // ; PCMC tx delay 16
	0x80, 0x0C, 0x0C, 0x03, 0x80, 0x0D, 0x00, 0x10, // ; PCMC rx delay 16
	0x80, 0x0C, 0x0D, 0x04, 0x80, 0x0D, 0x00, 0x00, // ; PCM latch 0
	0x80, 0x0C, 0x0D, 0x02, 0x80, 0x0D, 0x00, 0x10, // ; PCM Tx delay 16
	0x80, 0x0C, 0x0D, 0x03, 0x80, 0x0D, 0x00, 0x10, // ; PCM Rx delay 16
#elif defined(CONFIG_USA_MODEL_SGH_I717)
	0x80, 0x1C, 0x00, 0x00, 							//; 0x801C:VoiceProcessingOn, 0x0000:OFF
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x03, 	//; Microphone Configuration,0x0003:1-mic External (MD)
	0x80, 0x26, 0x00, 0x1A, 							//;0x8026:SelectRouting, 0x001A:(26)
	0x80, 0x1B, 0x00, 0x00, 							//;port a left in gain 0db
	0x80, 0x1B, 0x01, 0x00, 							//;port a right 0db
	0x80, 0x15, 0x04, 0x00, 							//; port c left out odb
	0x80, 0x15, 0x05, 0x00, 							//; port c Rout 0db
	0x80, 0x1B, 0x02, 0x00, 							//; Port B Lin 0db
	0x80, 0x1B, 0x03, 0x00, 							//; Port B Rin 0db
	0x80, 0x15, 0x06, 0x00, 							//; Port D Lout 0db
	0x80, 0x15, 0x07, 0x00, 							//; Port D Rout 0db
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x00, 	//; Tx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0006:Level 6
	0x80, 0x17, 0x00, 0x15, 0x80, 0x18, 0x00, 0x00, 	//; Side Tone Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x00, 	//; AEC Mode, 0x8018:SetAlgorithmParm, 0x0001:AEC On (auto select mode)
	0x80, 0x17, 0x00, 0x12, 0x80, 0x18, 0x00, 0x00, 	//; Downlink Speaker Volume, 0x8018:SetAlgorithmParm, 0x0000:(0 dB)
	0x80, 0x17, 0x00, 0x34, 0x80, 0x18, 0x00, 0x00, 	//; Echo Suppression Enhancement, 0x8018:SetAlgorithmParm, 0x0000:(0 dB)
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00, 	//; Use AGC, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00, 	//; Use Rx AGC, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x09, 0x80, 0x18, 0x00, 0x00, 	//; Speaker Enhancement Mode, 0x8018:SetAlgorithmParm, 0x0000:SE Off (HPF only)
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x00, 	//; Far End Noise Suppression, 0x8018:SetAlgorithmParm, 0x0002:On
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x00, 	//; Rx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0003:Level 3
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x00, 	//; Tx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, 	//; Rx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x30, 0x80, 0x18, 0x00, 0x00, 	//; Tx MBC Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x01, 	//; Rx MBC Mode, 0x8018:SetAlgorithmParm, 0x0001:On
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, 	//; Use Tx ComfortNoise, 0x8018:SetAlgorithmParm, 0x0000:No
//	0x80, 0x17, 0x00, 0x4F, 0x80, 0x18, 0x00, 0x00, 	//; Use Bandwidth Expansion, 0x8018:SetAlgorithmParm, 0x0000:No
//	0x80, 0x17, 0x00, 0x50, 0x80, 0x18, 0xFF, 0xF6, 	//; Gain High Band (dB), 0x8018:SetAlgorithmParm, 0xFFF6:(-10 dB)
//	0x80, 0x17, 0x00, 0x52, 0x80, 0x18, 0x00, 0x00, 	//; Use BWE Post EQ, 0x8018:SetAlgorithmParm, 0x0000:No
#else
	0x80, 0x1C, 0x00, 0x00, 							//; 0x801C:VoiceProcessingOn, 0x0000:OFF
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x03, 	//; Microphone Configuration,0x0003:1-mic External (MD)
	0x80, 0x26, 0x00, 0x1A, 							//;0x8026:SelectRouting, 0x001A:(26)
	0x80, 0x1B, 0x00, 0x00, 							//;port a left in gain 0db
	0x80, 0x1B, 0x01, 0x00, 							//;port a right 0db
	0x80, 0x15, 0x04, 0x00, 							//; port c left out odb
	0x80, 0x15, 0x05, 0x00, 							//; port c Rout 0db
	0x80, 0x1B, 0x02, 0x00, 							//; Port B Lin 0db
	0x80, 0x1B, 0x03, 0x00, 							//; Port B Rin 0db
	0x80, 0x15, 0x06, 0x00, 							//; Port D Lout 0db
	0x80, 0x15, 0x07, 0x00, 							//; Port D Rout 0db
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x00, 	//; Tx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0006:Level 6
	0x80, 0x17, 0x00, 0x15, 0x80, 0x18, 0x00, 0x00, 	//; Side Tone Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x00, 	//; AEC Mode, 0x8018:SetAlgorithmParm, 0x0001:AEC On (auto select mode)
	0x80, 0x17, 0x00, 0x12, 0x80, 0x18, 0x00, 0x00, 	//; Downlink Speaker Volume, 0x8018:SetAlgorithmParm, 0x0000:(0 dB)
	0x80, 0x17, 0x00, 0x34, 0x80, 0x18, 0x00, 0x00, 	//; Echo Suppression Enhancement, 0x8018:SetAlgorithmParm, 0x0000:(0 dB)
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00, 	//; Use AGC, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00, 	//; Use Rx AGC, 0x8018:SetAlgorithmParm, 0x0000:No
	0x80, 0x17, 0x00, 0x09, 0x80, 0x18, 0x00, 0x00, 	//; Speaker Enhancement Mode, 0x8018:SetAlgorithmParm, 0x0000:SE Off (HPF only)
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x00, 	//; Far End Noise Suppression, 0x8018:SetAlgorithmParm, 0x0002:On
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x00, 	//; Rx Noise Suppression Level, 0x8018:SetAlgorithmParm, 0x0003:Level 3
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x00, 	//; Tx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, 	//; Rx PostEq Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x30, 0x80, 0x18, 0x00, 0x00, 	//; Tx MBC Mode, 0x8018:SetAlgorithmParm, 0x0000:Off
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x01, 	//; Rx MBC Mode, 0x8018:SetAlgorithmParm, 0x0001:On
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, 	//; Use Tx ComfortNoise, 0x8018:SetAlgorithmParm, 0x0000:No
//	0x80, 0x17, 0x00, 0x4F, 0x80, 0x18, 0x00, 0x00, 	//; Use Bandwidth Expansion, 0x8018:SetAlgorithmParm, 0x0000:No
//	0x80, 0x17, 0x00, 0x50, 0x80, 0x18, 0xFF, 0xF6, 	//; Gain High Band (dB), 0x8018:SetAlgorithmParm, 0xFFF6:(-10 dB)
//	0x80, 0x17, 0x00, 0x52, 0x80, 0x18, 0x00, 0x00, 	//; Use BWE Post EQ, 0x8018:SetAlgorithmParm, 0x0000:No
#endif
};

unsigned char phonecall_speaker[] = {

#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
	0x80, 0x1C, 0x00, 0x00, 												//; 0x801C:VoiceProcessingOn, 0x0000:OFF
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x02, //; 0x8017:SetAlgorithmParmID, 0x0002:Microphone Configuration, 0x8018:SetAlgorithmParm, 0x0002:1-mic SPK
	0x80, 0x26, 0x00, 0x1B, 												//; 0x8026:SelectRouting, 0x001B:(27)
	0x80, 0x1B, 0x00, 0x00, 												//;port a left in gain -0db
	0x80, 0x1B, 0x01, 0x00, 												//;port a right -0db
	0x80, 0x15, 0x04, 0x00, 												//; port c left out -0db
	0x80, 0x15, 0x05, 0x00, 												//; port c Rout -0db
	0x80, 0x1B, 0x02, 0x00, 												//; Port B Lin 0db
	0x80, 0x1B, 0x03, 0x00, 												//; Port B Rin 0db
	0x80, 0x15, 0x06, 0x00, 												//; Port D Lout 0
	0x80, 0x15, 0x07, 0x00, 												//; Port D Rout 0db
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x06, //;NS level 6
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x02, //; Rx Ns On
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x07, //; Rx NS levle 7
	0x80, 0x17, 0x00, 0x15, 0x80, 0x18, 0x00, 0x00, //;side tone OFF
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x00, //;AEC OFF
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00, //;Tx AGC Off
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00, //;Rx AGC Off
	0x80, 0x17, 0x00, 0x09, 0x80, 0x18, 0x00, 0x00, //;VEQ OFF
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x00, //;Tx post EQ off
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, //;Rx post EQ off
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, //;Tx comport noise off
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x00, //; AEC OFF
	0x80, 0x17, 0x00, 0x12, 0x80, 0x18, 0xFF, 0xFF, //; DSV -1
	0x80, 0x17, 0x00, 0x34, 0x80, 0x18, 0xFF, 0xFF, //; ESE -1
#elif defined(CONFIG_EUR_MODEL_GT_I9210)
	// IJ 110905_HW tuned values - 3_Celox Euro_SPK_0904.txt
	0x80, 0x1C, 0x00, 0x01, // ; 0x801C:VoiceProcessingOn, 0x0001:On
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x02, // ; 0x8017:SetAlgorithmParmID, 0x0002:Microphone Configuration, 0x8018:SetAlgorithmParm, 0x0002:1-mic FT
	0x80, 0x26, 0x00, 0x1B, // ; 0x8026:SelectRouting, 0x001B:(27)
	0x80, 0x1B, 0x00, 0x00, // ;port a left in gain -90db
	0x80, 0x1B, 0x01, 0x00, // ;port a right 0db
	0x80, 0x15, 0x04, 0x00, // ; port c left out 0db
	0x80, 0x15, 0x05, 0x00, // ; port c Rout 0db
	0x80, 0x1B, 0x02, 0x00, // ; Port B Lin 0db
	0x80, 0x1B, 0x03, 0x00, // ; Port B Rin 0db
	0x80, 0x15, 0x06, 0x00, // ; Port D Lout 0db
	0x80, 0x15, 0x07, 0x00, // ; Port D Rout 0db
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x03, // ; NS level 3
	0x80, 0x17, 0x00, 0x15, 0x80, 0x18, 0x00, 0x00, // ;side tone OFF
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x01, // ; AEC On
	0x80, 0x17, 0x00, 0x12, 0x80, 0x18, 0xFF, 0xF5, // ; DSV -11db
	0x80, 0x17, 0x00, 0x34, 0x80, 0x18, 0x00, 0x15, // ;ese 21
	0x80, 0x17, 0x00, 0x23, 0x80, 0x18, 0x00, 0x00, // ;  aec comfort noise off
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x01, // ; Tx AGC On
	0x80, 0x17, 0x00, 0x05, 0x80, 0x18, 0xFF, 0xE6, // ; target -26
	0x80, 0x17, 0x00, 0x06, 0x80, 0x18, 0xFF, 0xBA, // ; NS floor -70db
	0x80, 0x17, 0x00, 0x07, 0x80, 0x18, 0x00, 0x00, // ; snr imp 0db
	0x80, 0x17, 0x00, 0x26, 0x80, 0x18, 0x00, 0x00, // ; up rate 0
	0x80, 0x17, 0x00, 0x27, 0x80, 0x18, 0x00, 0x00, // ; down rate 0
	0x80, 0x17, 0x01, 0x00, 0x80, 0x18, 0x00, 0x05, // ; max gain 5
	0x80, 0x17, 0x00, 0x3E, 0x80, 0x18, 0x00, 0x0A, // ; guard 10
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00, // ; Rx AGC Off
	0x80, 0x17, 0x00, 0x29, 0x80, 0x18, 0xFF, 0xE0, // ; AGc target levle -32dB
	0x80, 0x17, 0x00, 0x2A, 0x80, 0x18, 0xFF, 0xC4, // ; Noise floor -60dB
	0x80, 0x17, 0x00, 0x2B, 0x80, 0x18, 0x00, 0x00, // ; SNR improve 0
	0x80, 0x17, 0x00, 0x2C, 0x80, 0x18, 0x00, 0x00, // ; up rate 0
	0x80, 0x17, 0x00, 0x2D, 0x80, 0x18, 0x00, 0x00, // ; down rate 0
	0x80, 0x17, 0x01, 0x02, 0x80, 0x18, 0x00, 0x05, // ; max gain 5dB
	0x80, 0x17, 0x00, 0x3F, 0x80, 0x18, 0x00, 0x0A, // ;guard band 10dB
	0x80, 0x17, 0x00, 0x09, 0x80, 0x18, 0x00, 0x00, // ; VEQ OFF
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x01, // ;Rx NS on
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x03, // ; Rx NS 3
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x00, // ; Tx post EQ off
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, // ; Rx post EQ off
	0x80, 0x17, 0x00, 0x30, 0x80, 0x18, 0x00, 0x00, // ; Tx MBC OFF
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x00, // ; Rx MBC OFF
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, // ; Tx comport noise off
	0x80, 0x26, 0x00, 0x1B, // ; 0x8026:SelectRouting, 0x001B:(27)
#elif defined(CONFIG_USA_MODEL_SGH_I717)
	0x80, 0x1C, 0x00, 0x01, 															//; 0x801C:VoiceProcessingOn, 0x0001:On
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x02, 			//; 0x8017:SetAlgorithmParmID, 0x0002:Microphone Configuration, 0x8018:SetAlgorithmParm, 0x0002:1-mic FT
	0x80, 0x26, 0x00, 0x1B, 							  //; 0x8026:SelectRouting, 0x001B:(27)
	0x80, 0x1B, 0x00, 0xa6, 							  //;port a left in gain -90db
	0x80, 0x1B, 0x01, 0x00, 							  //;port a right 0db
	0x80, 0x15, 0x04, 0x00, 							  //; port c left out 0db
	0x80, 0x15, 0x05, 0x00, 							  //; port c Rout 0db
	0x80, 0x1B, 0x02, 0x00, 							  //; Port B Lin 0db
	0x80, 0x1B, 0x03, 0x00, 							  //; Port B Rin 0db
	0x80, 0x15, 0x06, 0x03, 							  //; Port D Lout 3db
	0x80, 0x15, 0x07, 0x03, 							  //; Port D Rout 3db
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x03, 	  //; NS level 6
	0x80, 0x17, 0x00, 0x15, 0x80, 0x18, 0x00, 0x00, 	  //;side tone OFF
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x01, 	  //; AEC On
	0x80, 0x17, 0x00, 0x12, 0x80, 0x18, 0xFF, 0xF5, 	  //; DSV -11
	0x80, 0x17, 0x00, 0x23, 0x80, 0x18, 0x00, 0x00, 	  //;EC comfort noise off
	0x80, 0x17, 0x00, 0x34, 0x80, 0x18, 0x00, 0x10, 	  //; ese 16
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00, 	  //; Tx AGC Off
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00,       //; Rx AGC Off
	0x80, 0x17, 0x00, 0x09, 0x80, 0x18, 0x00, 0x00, 	  //; VEQ OFF
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x02, 	  //;Rx NS on
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x00, 	  //; Rx NS 3
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x00, 	  //; Tx post EQ off
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, 	  //; Rx post EQ off
	0x80, 0x17, 0x00, 0x30, 0x80, 0x18, 0x00, 0x00, 	  //; Tx MBC Mode, Off
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x00, 	  //; Rx MBC Mode, On
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, 	  //; Tx comport noise off
//	0x80, 0x17, 0x00, 0x4F, 0x80, 0x18, 0x00, 0x00, 	  //; Use Bandwidth Expansion, 0x8018:SetAlgorithmParm, 0x0000:No
//	0x80, 0x17, 0x00, 0x50, 0x80, 0x18, 0xFF, 0xF6, 	  //; Gain High Band (dB), 0x8018:SetAlgorithmParm, 0xFFF6:(-10 dB)
//	0x80, 0x17, 0x00, 0x52, 0x80, 0x18, 0x00, 0x00, 	  //; Use BWE Post EQ, 0x8018:SetAlgorithmParm, 0x0000:no
#elif defined(CONFIG_USA_MODEL_SGH_I757)
	0x80, 0x1C, 0x00, 0x01, //; 0x801C:VoiceProcessingOn, 0x0001:On
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x02, //; 0x8017:SetAlgorithmParmID, 0x0002:Microphone Configuration, 0x8018:SetAlgorithmParm, 0x0002:1-mic FT
	0x80, 0x26, 0x00, 0x1B, //; 0x8026:SelectRouting, 0x001B:(27)
	0x80, 0x1B, 0x00, 0xa6, //;port a left in gain -90db
	0x80, 0x1B, 0x01, 0x00, //;port A in 0dB
	0x80, 0x15, 0x04, 0xFD, //; Port C out -3dB
	0x80, 0x15, 0x05, 0x00, //; port c Rout 0db
	0x80, 0x1B, 0x02, 0x00, //; Port B Lin 0db
	0x80, 0x1B, 0x03, 0x00, //; Port B Rin 0db
	0x80, 0x15, 0x06, 0x03, //; Port D Lout 3db
	0x80, 0x15, 0x07, 0x03, //; Port D Rout 3db
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x03, //; NS level 3
	0x80, 0x17, 0x00, 0x15, 0x80, 0x18, 0x00, 0x00, //;side tone OFF
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x01, //; AEC On
	0x80, 0x17, 0x00, 0x12, 0x80, 0x18, 0xFF, 0xFA, //; DSV -6
	0x80, 0x17, 0x00, 0x23, 0x80, 0x18, 0x00, 0x00, //; EC comfort noise off
	0x80, 0x17, 0x00, 0x34, 0x80, 0x18, 0x00, 0x13, //; ese 19
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00, //; Tx AGC Off
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00, //; Rx AGC Off
	0x80, 0x17, 0x00, 0x09, 0x80, 0x18, 0x00, 0x00, //; VEQ OFF
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x02, //; Rx NS on
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x00, //; Rx NS 0 (off)
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x00, //; Tx post EQ off
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, //; Rx post EQ off
	0x80, 0x17, 0x00, 0x30, 0x80, 0x18, 0x00, 0x00, //; Tx MBC Mode, :Off
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x00, //; Rx MBC Mode, :Off
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, //; Tx comport noise off
//	0x80, 0x17, 0x00, 0x4F, 0x80, 0x18, 0x00, 0x00, 	  //; Use Bandwidth Expansion, 0x8018:SetAlgorithmParm, 0x0000:No
//	0x80, 0x17, 0x00, 0x50, 0x80, 0x18, 0xFF, 0xF6, 	  //; Gain High Band (dB), 0x8018:SetAlgorithmParm, 0xFFF6:(-10 dB)
//	0x80, 0x17, 0x00, 0x52, 0x80, 0x18, 0x00, 0x00, 	  //; Use BWE Post EQ, 0x8018:SetAlgorithmParm, 0x0000:no
#else
	0x80, 0x1C, 0x00, 0x01, 															//; 0x801C:VoiceProcessingOn, 0x0001:On
	0x80, 0x17, 0x00, 0x02, 0x80, 0x18, 0x00, 0x02, 			//; 0x8017:SetAlgorithmParmID, 0x0002:Microphone Configuration, 0x8018:SetAlgorithmParm, 0x0002:1-mic FT
	0x80, 0x26, 0x00, 0x1B, 							  //; 0x8026:SelectRouting, 0x001B:(27)
	0x80, 0x1B, 0x00, 0xa6, 							  //;port a left in gain -90db
	0x80, 0x1B, 0x01, 0x00, 							  //;port a right 0db
	0x80, 0x15, 0x04, 0x00, 							  //; port c left out 0db
	0x80, 0x15, 0x05, 0x00, 							  //; port c Rout 0db
	0x80, 0x1B, 0x02, 0x00, 							  //; Port B Lin 0db
	0x80, 0x1B, 0x03, 0x00, 							  //; Port B Rin 0db
	0x80, 0x15, 0x06, 0x03, 							  //; Port D Lout 3db
	0x80, 0x15, 0x07, 0x03, 							  //; Port D Rout 3db
	0x80, 0x17, 0x00, 0x4B, 0x80, 0x18, 0x00, 0x03, 	  //; NS level 6
	0x80, 0x17, 0x00, 0x15, 0x80, 0x18, 0x00, 0x00, 	  //;side tone OFF
	0x80, 0x17, 0x00, 0x03, 0x80, 0x18, 0x00, 0x01, 	  //; AEC On
	0x80, 0x17, 0x00, 0x12, 0x80, 0x18, 0xFF, 0xF5, 	  //; DSV -11
	0x80, 0x17, 0x00, 0x23, 0x80, 0x18, 0x00, 0x00, 	  //;EC comfort noise off
	0x80, 0x17, 0x00, 0x34, 0x80, 0x18, 0x00, 0x10, 	  //; ese 16
	0x80, 0x17, 0x00, 0x04, 0x80, 0x18, 0x00, 0x00, 	  //; Tx AGC Off
	0x80, 0x17, 0x00, 0x28, 0x80, 0x18, 0x00, 0x00,       //; Rx AGC Off
	0x80, 0x17, 0x00, 0x09, 0x80, 0x18, 0x00, 0x00, 	  //; VEQ OFF
	0x80, 0x17, 0x00, 0x0E, 0x80, 0x18, 0x00, 0x02, 	  //;Rx NS on
	0x80, 0x17, 0x00, 0x4C, 0x80, 0x18, 0x00, 0x00, 	  //; Rx NS 3
	0x80, 0x17, 0x00, 0x20, 0x80, 0x18, 0x00, 0x00, 	  //; Tx post EQ off
	0x80, 0x17, 0x00, 0x1F, 0x80, 0x18, 0x00, 0x00, 	  //; Rx post EQ off
	0x80, 0x17, 0x00, 0x30, 0x80, 0x18, 0x00, 0x00, 	  //; Tx MBC Mode, Off
	0x80, 0x17, 0x00, 0x31, 0x80, 0x18, 0x00, 0x00, 	  //; Rx MBC Mode, On
	0x80, 0x17, 0x00, 0x1A, 0x80, 0x18, 0x00, 0x00, 	  //; Tx comport noise off
//	0x80, 0x17, 0x00, 0x4F, 0x80, 0x18, 0x00, 0x00, 	  //; Use Bandwidth Expansion, 0x8018:SetAlgorithmParm, 0x0000:No
//	0x80, 0x17, 0x00, 0x50, 0x80, 0x18, 0xFF, 0xF6, 	  //; Gain High Band (dB), 0x8018:SetAlgorithmParm, 0xFFF6:(-10 dB)
//	0x80, 0x17, 0x00, 0x52, 0x80, 0x18, 0x00, 0x00, 	  //; Use BWE Post EQ, 0x8018:SetAlgorithmParm, 0x0000:no
#endif
};

unsigned char phonecall_bt[] = {
	0x80,0x17,0x00,0x02, /* SetAlgorithmParmID, 0x0002:Microphone Configuration */
	0x80,0x18,0x00,0x03, /* SetAlgorithmParm, 0x0003:1-mic External (MD) */
	0x80,0x26,0x00,0x06, /* SelectRouting, 0x0006:Snk,Snk,Fei,Pri - Zro,Csp,Feo (PCM0->PCM1+ADCs) */
	0x80,0x1C,0x00,0x00, /* VoiceProcessingOn, 0x0000:No */
	0x80,0x1B,0x00,0x00, /* SetDigitalInputGain, 0x00:Primay Mic (Tx), 0x00:(0 dB) */
	0x80,0x15,0x00,0x00, /* SetDigitalOutputGain, 0x00:Tx, 0x00:(0 dB) */
};

unsigned char phonecall_tty[] = {
	0x80,0x26,0x00,0x15, /* SelectRouting, 0x0015:Snk,Pri,Snk,Snk - Csp,Zro,Zro (none) */
	0x80,0x1C,0x00,0x00, /* VoiceProcessingOn, 0x0000:No */
	0x80,0x1B,0x00,0x00, /* SetDigitalInputGain, 0x00:Primay Mic (Tx), 0x00:(0 dB) */
	0x80,0x15,0x00,0xFB, /* SetDigitalOutputGain, 0x00:Tx, 0xFB:(-5 dB) */
};

unsigned char INT_MIC_recording_receiver[] = {
	0x80,0x26,0x00,0x07, /* SelectRouting, 0x0007:Pri,Snk,Snk,Snk - Csp,Zro,Zro (none) */
	0x80,0x1C,0x00,0x00, /* VoiceProcessingOn, 0x0000:No */
	0x80,0x1B,0x00,0x12, /* SetDigitalInputGain, 0x00:Primay Mic (Tx), 0x12:(18 dB) */
	0x80,0x15,0x00,0x00, /* SetDigitalOutputGain, 0x00:Tx, 0x00:(0 dB) */
};

unsigned char EXT_MIC_recording[] = {
	0x80,0x26,0x00,0x15, /* SelectRouting, 0x0015:Snk,Pri,Snk,Snk - Csp,Zro,Zro (none) */
	0x80,0x1C,0x00,0x00, /* VoiceProcessingOn, 0x0000:No */
	0x80,0x1B,0x00,0x12, /* SetDigitalInputGain, 0x00:Primay Mic (Tx), 0x12:(18 dB) */
	0x80,0x15,0x00,0x00, /* SetDigitalOutputGain, 0x00:Tx, 0x00:(0 dB) */
};

unsigned char INT_MIC_recording_speaker[] = {
	0x80,0x17,0x00,0x02, /* SetAlgorithmParmID, 0x0002:Microphone Configuration */
	0x80,0x18,0x00,0x02, /* SetAlgorithmParm, 0x0002:1-mic Desktop/Vehicle (DV) */
	0x80,0x1C,0x00,0x00, /* VoiceProcessingOn, 0x0000:No */
	0x80,0x1B,0x00,0x12, /* SetDigitalInputGain, 0x00:Primay Mic (Tx), 0x12:(18 dB) */
	0x80,0x15,0x00,0x00, /* SetDigitalOutputGain, 0x00:Tx, 0x00:(0 dB) */
};

unsigned char BACK_MIC_recording[] = {
	0x80,0x17,0x00,0x02, /* SetAlgorithmParmID, 0x0002:Microphone Configuration */
	0x80,0x18,0x00,0x02, /* SetAlgorithmParm, 0x0002:1-mic Desktop/Vehicle (DV) */
	0x80,0x26,0x00,0x15, /* SelectRouting, 0x0015:Snk,Pri,Snk,Snk - Csp,Zro,Zro (none) */
	0x80,0x1C,0x00,0x01, /* VoiceProcessingOn, 0x0001:Yes */
	0x80,0x17,0x00,0x04, /* SetAlgorithmParmID, 0x0004:Use AGC */
	0x80,0x18,0x00,0x01, /* SetAlgorithmParm, 0x0001:Yes */
	0x80,0x17,0x00,0x1A, /* SetAlgorithmParmID, 0x001A:Use ComfortNoise */
	0x80,0x18,0x00,0x00, /* SetAlgorithmParm, 0x0000:No */
	0x80,0x17,0x00,0x00, /* SetAlgorithmParmID, 0x0000:Suppression Strength */
	0x80,0x18,0x00,0x00, /* SetAlgorithmParm, 0x0000:No Suppression */
	0x80,0x1B,0x00,0x12, /* SetDigitalInputGain, 0x00:Primay Mic (Tx), 0x12:(18 dB) */
	0x80,0x15,0x00,0x06, /* SetDigitalOutputGain, 0x00:Tx, 0x06:(6 dB) */
};

unsigned char vr_no_ns_receiver[] = {
	0x80,0x17,0x00,0x02, /* SetAlgorithmParmID, 0x0002:Microphone Configuration */
	0x80,0x18,0x00,0x00, /* SetAlgorithmParm, 0x0000:2-mic Close Talk (CT) */
	0x80,0x1C,0x00,0x00, /* VoiceProcessingOn, 0x0000:No */
	0x80,0x1B,0x00,0x0C, /* SetDigitalInputGain, 0x00:Primay Mic (Tx), 0x0C:(12 dB) */
	0x80,0x1B,0x01,0x0C, /* SetDigitalInputGain, 0x01:Secondary Mic (Tx), 0x09:(12 dB) */
	0x80,0x15,0x00,0x00, /* SetDigitalOutputGain, 0x00:Tx, 0x00:(0 dB) */
};

unsigned char vr_no_ns_headset[] = {
	0x80,0x17,0x00,0x02, /* SetAlgorithmParmID, 0x0002:Microphone Configuration */
	0x80,0x18,0x00,0x03, /* SetAlgorithmParm, 0x0003:1M-DG (1-mic digital input) */
	0x80,0x26,0x00,0x15, /* SelectRouting, 0x0015:Snk,Pri,Snk,Snk - Csp,Zro,Zro (none) */
	0x80,0x1C,0x00,0x00, /* VoiceProcessingOn, 0x0000:No */
	0x80,0x1B,0x00,0x12, /* SetDigitalInputGain, 0x00:Primay Mic (Tx), 0x12:(18 dB) */
	0x80,0x15,0x00,0x00, /* SetDigitalOutputGain, 0x00:Tx, 0x00:(0 dB) */
};

unsigned char vr_no_ns_speaker[] = {
	0x80,0x17,0x00,0x02, /* SetAlgorithmParmID, 0x0002:Microphone Configuration */
	0x80,0x18,0x00,0x02, /* SetAlgorithmParm, 0x0002:1-mic Desktop/Vehicle (DV) */
	0x80,0x1C,0x00,0x00, /* VoiceProcessingOn, 0x0000:No */
	0x80,0x1B,0x00,0x0C, /* SetDigitalInputGain, 0x00:Primay Mic (Tx), 0x0C:(12 dB) */
	0x80,0x15,0x00,0x00, /* SetDigitalOutputGain, 0x00:Tx, 0x00:(0 dB) */
};

unsigned char vr_no_ns_bt[] = {
	0x80,0x26,0x00,0x06, /* SelectRouting, 0x0006:Snk,Snk,Fei,Pri - Zro,Csp,Feo (PCM0->PCM1+ADCs) */
	0x80,0x1C,0x00,0x00, /* VoiceProcessingOn, 0x0000:No */
	0x80,0x1B,0x00,0x00, /* SetDigitalInputGain, 0x00:Primay Mic (Tx), 0x00:(0 dB) */
	0x80,0x15,0x00,0x00, /* SetDigitalOutputGain, 0x00:Tx, 0x00:(0 dB) */
};

unsigned char vr_ns_receiver[] = {
	0x80,0x17,0x00,0x02, /* SetAlgorithmParmID, 0x0002:Microphone Configuration */
	0x80,0x18,0x00,0x00, /* SetAlgorithmParm, 0x0000:2-mic Close Talk (CT) */
	0x80,0x1C,0x00,0x01, /* VoiceProcessingOn, 0x0001:Yes */
	0x80,0x17,0x00,0x1A, /* SetAlgorithmParmID, 0x001A:Use ComfortNoise */
	0x80,0x18,0x00,0x00, /* SetAlgorithmParm, 0x0000:No */
	0x80,0x17,0x00,0x04, /* SetAlgorithmParmID, 0x0004:Use AGC */
	0x80,0x18,0x00,0x00, /* SetAlgorithmParm, 0x0000:No */
	0x80,0x17,0x00,0x00, /* SetAlgorithmParmID, 0x0000:Suppression Strength */
	0x80,0x18,0x00,0x04, /* SetAlgorithmParm, 0x0004:20dB Max Suppression */
	0x80,0x1B,0x00,0x0C, /* SetDigitalInputGain, 0x00:Primay Mic (Tx), 0x0C:(12 dB) */
	0x80,0x1B,0x01,0x0C, /* SetDigitalInputGain, 0x01:Secondary Mic (Tx), 0x0C:(12 dB) */
	0x80,0x15,0x00,0x00, /* SetDigitalOutputGain, 0x00:Tx, 0x00:(0 dB) */
};

unsigned char vr_ns_headset[] = {
	0x80,0x17,0x00,0x02, /* SetAlgorithmParmID, 0x0002:Microphone Configuration */
	0x80,0x18,0x00,0x03, /* SetAlgorithmParm, 0x0003:1-mic External (MD) */
	0x80,0x26,0x00,0x15, /* SelectRouting, 0x0015:Snk,Pri,Snk,Snk - Csp,Zro,Zro (none) */
	0x80,0x1C,0x00,0x01, /* VoiceProcessingOn, 0x0001:Yes */
	0x80,0x17,0x00,0x00, /* SetAlgorithmParmID, 0x0000:Suppression Strength */
	0x80,0x18,0x00,0x02, /* SetAlgorithmParm, 0x0002:20dB Max Suppression */
	0x80,0x17,0x00,0x1A, /* SetAlgorithmParmID, 0x001A:Use ComfortNoise */
	0x80,0x18,0x00,0x00, /* SetAlgorithmParm, 0x0000:No */
	0x80,0x17,0x00,0x04, /* SetAlgorithmParmID, 0x0004:Use AGC */
	0x80,0x18,0x00,0x00, /* SetAlgorithmParm, 0x0000:No */
	0x80,0x1B,0x00,0x12, /* SetDigitalInputGain, 0x00:Primay Mic (Tx), 0x12:(18 dB) */
	0x80,0x15,0x00,0x00, /* SetDigitalOutputGain, 0x00:Tx, 0x00:(0 dB) */
};

unsigned char vr_ns_speaker[] = {
	0x80,0x17,0x00,0x02, /* SetAlgorithmParmID, 0x0002:Microphone Configuration */
	0x80,0x18,0x00,0x02, /* SetAlgorithmParm, 0x0002:1-mic Desktop/Vehicle (DV) */
	0x80,0x1C,0x00,0x01, /* VoiceProcessingOn, 0x0001:Yes */
	0x80,0x17,0x00,0x00, /* SetAlgorithmParmID, 0x0000:Suppression Strength */
	0x80,0x18,0x00,0x04, /* SetAlgorithmParm, 0x0004:20dB Max Suppression */
	0x80,0x17,0x00,0x04, /* SetAlgorithmParmID, 0x0004:Use AGC */
	0x80,0x18,0x00,0x00, /* SetAlgorithmParm, 0x0000:No */
	0x80,0x17,0x00,0x1A, /* SetAlgorithmParmID, 0x001A:Use ComfortNoise */
	0x80,0x18,0x00,0x00, /* SetAlgorithmParm, 0x0000:No */
	0x80,0x1B,0x00,0x0C, /* SetDigitalInputGain, 0x00:Primay Mic (Tx), 0x0C:(12 dB) */
	0x80,0x15,0x00,0x00, /* SetDigitalOutputGain, 0x00:Tx, 0x00:(0 dB) */
};

unsigned char vr_ns_bt[] = {
	0x80,0x26,0x00,0x06, /* SelectRouting, 0x0006:Snk,Snk,Fei,Pri - Zro,Csp,Feo (PCM0->PCM1+ADCs) */
	0x80,0x1C,0x00,0x01, /* VoiceProcessingOn, 0x0001:Yes */
	0x80,0x17,0x00,0x00, /* SetAlgorithmParmID, 0x0000:Suppression Strength */
	0x80,0x18,0x00,0x02, /* SetAlgorithmParm, 0x0002:20dB Max Suppression */
	0x80,0x17,0x00,0x04, /* SetAlgorithmParmID, 0x0004:Use AGC */
	0x80,0x18,0x00,0x00, /* SetAlgorithmParm, 0x0000:No */
	0x80,0x17,0x00,0x1A, /* SetAlgorithmParmID, 0x001A:Use ComfortNoise */
	0x80,0x18,0x00,0x00, /* SetAlgorithmParm, 0x0000:No */
	0x80,0x1B,0x00,0x00, /* SetDigitalInputGain, 0x00:Primay Mic (Tx), 0x00:(0 dB) */
	0x80,0x15,0x00,0x00, /* SetDigitalOutputGain, 0x00:Tx, 0x00:(0 dB) */
};

unsigned char suspend_mode[] = {
	0x80,0x10,0x00,0x01
};

#if defined(CONFIG_EUR_MODEL_GT_I9210)
int a2220_change_network_type(int network_type)
{
	int rc = 0, i = 0; 
	
	if(a2220_WB == network_type)
	{
		printk("%s : already set as same network type\n", __func__);
		return 0;
	}
	a2220_WB = network_type;

	if(a2220_current_config == A2220_PATH_INCALL_RECEIVER_NSON \
		|| a2220_current_config == A2220_PATH_INCALL_RECEIVER_NSOFF)
	{
		rc = execute_cmdmsg(phonecall_Bandtype_Init);
		
		if(a2220_WB == A2220_PATH_WIDEBAND){
			rc = execute_cmdmsg(phonecall_Bandtype_WB);
			printk("[%s] Wideband setting\n", __func__);
		}
		else{
			rc = execute_cmdmsg(phonecall_Bandtype_NB);
			printk("[%s] Narrowband setting\n", __func__);
		}

		for(i=0;i<10;i++)
			msleep(10);

		rc = execute_cmdmsg(GetRouteChangeStatus);
	}
	return 0;
}
#endif

static ssize_t chk_wakeup_a2220(void)
{
	int i,rc = 0, retry = 4;

#ifdef CONFIG_USA_MODEL_SGH_I577 /* Do not enable audience on I577 HW rev0.0 !!! */
	printk("[AUDIENCE] %s : SKIP\n", __func__);
	return 0;
#endif

	if (a2220_suspended == 1) {
		/* Enable A2220 clock */
		if (control_a2220_clk) {
			gpio_set_value(pdata->gpio_a2220_clk, 1);
			mdelay(1);
		}

		if (pdata->gpio_a2220_wakeup)
		{
			printk(MODULE_NAME "%s : chk_wakeup_a2220  --> get_hw_rev of Target = %d\n", __func__, get_hw_rev());
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
			if (get_hw_rev() >= 0x05 )	
				gpio_set_value(WAKEUP_GPIO_NUM_HERCULES_REV01, 0);
			else
				gpio_set_value(pdata->gpio_a2220_wakeup, 0);
#elif defined(CONFIG_USA_MODEL_SGH_I727)
			if (get_hw_rev() >= 0x05)	
				gpio_set_value(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, 0);
			else
				gpio_set_value(pdata->gpio_a2220_wakeup, 0);
#elif defined(CONFIG_USA_MODEL_SGH_I717)
			gpio_set_value(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, 0);
#else	
			gpio_set_value(pdata->gpio_a2220_wakeup, 0);
#endif
		}
		//gpio_set_value(pdata->gpio_a2220_wakeup, 0);
#ifdef CONFIG_USA_MODEL_SGH_I717
		for (i=0; i<5 ; i++)		
			msleep(20);	
#else
		msleep(30);
#endif

		do {
			rc = execute_cmdmsg(A100_msg_Sync);
		} while ((rc < 0) && --retry);

		// Audience not responding to execute_cmdmsg , doing HW reset of the chipset
		if((retry == 0)&&(rc < 0))
		{
			struct a2220img img;
        		img.buf = a2220_firmware_buf;
        		img.img_size = sizeof(a2220_firmware_buf);
			rc=a2220_hw_reset(&img);		// Call if the Audience chipset is not responding after retrying 12 times
		}
		if(rc < 0)
			printk(MODULE_NAME "%s ::  Audience HW Reset Failed\n");
/*	
#ifdef CONFIG_USA_MODEL_SGH_I717
		rc = hpt_longCmd_execute(&hpt_init_macro, sizeof(hpt_init_macro));
		if (rc < 0) {
			printk(MODULE_NAME "%s: htp init error\n", __func__);
		}
#endif
*/
		if (pdata->gpio_a2220_wakeup)
		{
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
			if (get_hw_rev() >= 0x05 )	
				gpio_set_value(WAKEUP_GPIO_NUM_HERCULES_REV01, 1);
			else
				gpio_set_value(pdata->gpio_a2220_wakeup, 1);
#elif defined(CONFIG_USA_MODEL_SGH_I727)
			if (get_hw_rev() >= 0x05)	
				gpio_set_value(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, 1);
			else
				gpio_set_value(pdata->gpio_a2220_wakeup, 1);		
#elif defined(CONFIG_USA_MODEL_SGH_I717)
			gpio_set_value(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, 1);
#else
			gpio_set_value(pdata->gpio_a2220_wakeup, 1);
#endif
		}
		//gpio_set_value(pdata->gpio_a2220_wakeup, 1);

		if (rc < 0) {
			printk("%s: failed (%d)\n", __func__, rc);
			goto wakeup_sync_err;
		}

		a2220_suspended = 0;
	}
wakeup_sync_err:
	return rc;
}

/* Filter commands according to noise suppression state forced by
 * A2220_SET_NS_STATE ioctl.
 *
 * For this function to operate properly, all configurations must include
 * both A100_msg_Bypass and Mic_Config commands even if default values
 * are selected or if Mic_Config is useless because VP is off
 */
int a2220_filter_vp_cmd(int cmd, int mode)
{
	int msg = (cmd >> 16) & 0xFFFF;
	int filtered_cmd = cmd;

	if (a2220_NS_state == A2220_NS_STATE_AUTO)
		return cmd;

	switch (msg) {
		case A100_msg_Bypass:
			if (a2220_NS_state == A2220_NS_STATE_OFF)
				filtered_cmd = A2220_msg_VP_OFF;
			else
				filtered_cmd = A2220_msg_VP_ON;
			break;
		case A100_msg_SetAlgorithmParmID:
			a2220_param_ID = cmd & 0xFFFF;
			break;
		case A100_msg_SetAlgorithmParm:
			if (a2220_param_ID == Mic_Config) {
				if (a2220_NS_state == A2220_NS_STATE_CT)
					filtered_cmd = (msg << 16);
				else if (a2220_NS_state == A2220_NS_STATE_FT)
					filtered_cmd = (msg << 16) | 0x0002;
			}
			break;
		default:
			if (mode == A2220_CONFIG_VP)
				filtered_cmd = -1;
			break;
	}

	pr_info("%s: %x filtered = %x, a2220_NS_state %d, mode %d\n", __func__,
			cmd, filtered_cmd, a2220_NS_state, mode);

	return filtered_cmd;
}

int a2220_set_config(char newid, int mode)
{
	int i = 0, rc = 0, size = 0;
	int retry = 4;
	unsigned int sw_reset = 0;
	unsigned char *i2c_cmds;
	unsigned int msg;
	unsigned char *pMsg;
	
#ifdef CONFIG_USA_MODEL_SGH_I577 /* Do not enable audience on I577 HW rev0.0 !!! */
	if (newid != A2220_PATH_BYPASS_MULTIMEDIA) {
		printk("[AUDIENCE] %s : SKIP\n, __func__");
		return 0;
	}
#endif

	printk("[AUD] new mode =%d\n", newid);	
	if ((a2220_suspended) && (newid == A2220_PATH_SUSPEND))
		return rc;

#if defined(CONFIG_USA_MODEL_SGH_T989 ) || defined(CONFIG_USA_MODEL_SGH_I727) || defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_USA_MODEL_SGH_I757) || defined (CONFIG_USA_MODEL_SGH_T769)
	if(a2220_current_config == newid)
	{
		printk("already configured this path!!! \n");
		return rc;
	}
#endif 

	rc = chk_wakeup_a2220();
	if (rc < 0)
		return rc;

	sw_reset = ((A100_msg_Reset << 16) | RESET_IMMEDIATE);

	switch (newid) {
		case A2220_PATH_INCALL_RECEIVER_NSON:
			i2c_cmds = phonecall_receiver_nson;
			size = sizeof(phonecall_receiver_nson);
			break;

		case A2220_PATH_INCALL_RECEIVER_NSOFF:
			i2c_cmds = phonecall_receiver_nsoff;
			size = sizeof(phonecall_receiver_nsoff);
			break;			

			// (+) ysseo 20110420 : to use a2220 bypass mode		
#ifdef AUDIENCE_BYPASS //(+)dragonball Multimedia bypass mode
		case A2220_PATH_BYPASS_MULTIMEDIA:
			pr_debug(MODULE_NAME "%s : setting A2220_PATH_BYPASS_MULTIMEDIA\n", __func__);
			i2c_cmds = bypass_multimedia;
			size = sizeof(bypass_multimedia);
			break;
#endif		
		case A2220_PATH_INCALL_HEADSET:
			//gpio_set_value(pdata->gpio_a2220_micsel, 1);
			i2c_cmds = phonecall_headset;
			size = sizeof(phonecall_headset);
			break;
		case A2220_PATH_INCALL_SPEAKER:
			//gpio_set_value(pdata->gpio_a2220_micsel, 0);
			i2c_cmds = phonecall_speaker;
			size = sizeof(phonecall_speaker);
			break;
		case A2220_PATH_INCALL_BT:
			//gpio_set_value(pdata->gpio_a2220_micsel, 0);
			i2c_cmds = phonecall_bt;
			size = sizeof(phonecall_bt);
			break;
		case A2220_PATH_INCALL_TTY:
			//gpio_set_value(pdata->gpio_a2220_micsel, 1);
			i2c_cmds = phonecall_tty;
			size = sizeof(phonecall_tty);
			break;
		case A2220_PATH_VR_NO_NS_RECEIVER:
			//gpio_set_value(pdata->gpio_a2220_micsel, 0);
			i2c_cmds = vr_no_ns_receiver;
			size = sizeof(vr_no_ns_receiver);
			break;
		case A2220_PATH_VR_NO_NS_HEADSET:
			//gpio_set_value(pdata->gpio_a2220_micsel, 1);
			i2c_cmds = vr_no_ns_headset;
			size = sizeof(vr_no_ns_headset);
			break;
		case A2220_PATH_VR_NO_NS_SPEAKER:
			//gpio_set_value(pdata->gpio_a2220_micsel, 0);
			i2c_cmds = vr_no_ns_speaker;
			size = sizeof(vr_no_ns_speaker);
			break;
		case A2220_PATH_VR_NO_NS_BT:
			//gpio_set_value(pdata->gpio_a2220_micsel, 0);
			i2c_cmds = vr_no_ns_bt;
			size = sizeof(vr_no_ns_bt);
			break;
		case A2220_PATH_VR_NS_RECEIVER:
			//gpio_set_value(pdata->gpio_a2220_micsel, 0);
			i2c_cmds = vr_ns_receiver;
			size = sizeof(vr_ns_receiver);
			break;
		case A2220_PATH_VR_NS_HEADSET:
			//gpio_set_value(pdata->gpio_a2220_micsel, 1);
			i2c_cmds = vr_ns_headset;
			size = sizeof(vr_ns_headset);
			break;
		case A2220_PATH_VR_NS_SPEAKER:
			//gpio_set_value(pdata->gpio_a2220_micsel, 0);
			i2c_cmds = vr_ns_speaker;
			size = sizeof(vr_ns_speaker);
			break;
		case A2220_PATH_VR_NS_BT:
			//gpio_set_value(pdata->gpio_a2220_micsel, 0);
			i2c_cmds = vr_ns_bt;
			size = sizeof(vr_ns_bt);
			break;
		case A2220_PATH_RECORD_RECEIVER:
			//gpio_set_value(pdata->gpio_a2220_micsel, 0);
			i2c_cmds = INT_MIC_recording_receiver;
			size = sizeof(INT_MIC_recording_receiver);
			break;
		case A2220_PATH_RECORD_HEADSET:
			//gpio_set_value(pdata->gpio_a2220_micsel, 1);
			i2c_cmds = EXT_MIC_recording;
			size = sizeof(EXT_MIC_recording);
			break;
		case A2220_PATH_RECORD_SPEAKER:
			//gpio_set_value(pdata->gpio_a2220_micsel, 0);
			i2c_cmds = INT_MIC_recording_speaker;
			size = sizeof(INT_MIC_recording_speaker);
			break;
		case A2220_PATH_RECORD_BT:
			//gpio_set_value(pdata->gpio_a2220_micsel, 0);
			i2c_cmds = phonecall_bt;
			size = sizeof(phonecall_bt);
			break;
		case A2220_PATH_SUSPEND:
			//gpio_set_value(pdata->gpio_a2220_micsel, 0);
			i2c_cmds = (unsigned char *)suspend_mode;
			size = sizeof(suspend_mode);
			break;
		case A2220_PATH_CAMCORDER:
			//gpio_set_value(pdata->gpio_a2220_micsel, 0);
			i2c_cmds = BACK_MIC_recording;
			size = sizeof(BACK_MIC_recording);
			break;
		default:
			printk("%s: invalid cmd %d\n", __func__, newid);
			rc = -1;
			goto input_err;
			break;
	}

	a2220_current_config = newid;

#if DEBUG	
	pr_info("%s: change to mode %d\n", __func__, newid);
	pr_info("%s: block write start (size = %d)\n", __func__, size);
	for (i = 1; i <= size; i++) {
		pr_info("%x ", *(i2c_cmds + i - 1));
		if ( !(i % 4))
			pr_info("\n");
	}
#endif
	
#if 1

	pMsg = (unsigned char*)&msg;

	//pr_info("START!!!\n");
	for(i=0 ; i < size ; i+=4)
	{
		pMsg[3] = i2c_cmds[i];
		pMsg[2] = i2c_cmds[i+1];
		pMsg[1] = i2c_cmds[i+2];
		pMsg[0] = i2c_cmds[i+3];
		
		do {
			rc = execute_cmdmsg(msg);
		} while ((rc < 0) && --retry);

		// Audience not responding to execute_cmdmsg , doing HW reset of the chipset
		if((retry == 0)&&(rc < 0))
		{
			struct a2220img img;
        		img.buf = a2220_firmware_buf;
        		img.img_size = sizeof(a2220_firmware_buf);
			rc=a2220_hw_reset(&img);		// Call if the Audience chipset is not responding after retrying 12 times
			if(rc < 0)
			{
				printk(MODULE_NAME "%s ::  Audience HW Reset Failed\n");
				return rc;
			}
		}
		
	}
	//pr_info("END!!!\n");

#if defined(CONFIG_EUR_MODEL_GT_I9210)
	if(a2220_current_config == A2220_PATH_INCALL_RECEIVER_NSON \
		|| a2220_current_config == A2220_PATH_INCALL_RECEIVER_NSOFF)
	{
		rc = execute_cmdmsg(phonecall_Bandtype_Init);

		if(a2220_WB == A2220_PATH_WIDEBAND)
		{
			rc = execute_cmdmsg(phonecall_Bandtype_WB);
			if(rc == 0)
				printk("%s : Wideband Setting!!\n", __func__);
		}
		else
		{
			rc = execute_cmdmsg(phonecall_Bandtype_NB);
			if(rc == 0)
				printk("%s : Narrowband Setting!!\n", __func__);
		}
	
		for(i=0;i<10;i++)
			msleep(10);

		rc = execute_cmdmsg(GetRouteChangeStatus);
	}
#endif

#else
	rc = a2220_i2c_write(i2c_cmds, size);
	if (rc < 0) {
		printk("A2220 CMD block write error!\n");
		a2220_i2c_sw_reset(sw_reset);
		return rc;
	}
	pr_info("%s: block write end\n", __func__);

	/* Don't need to get Ack after sending out a suspend command */
	if (*i2c_cmds == 0x80 && *(i2c_cmds + 1) == 0x10
			&& *(i2c_cmds + 2) == 0x00 && *(i2c_cmds + 3) == 0x01) {
		a2220_suspended = 1;
		/* Disable A2220 clock */
		msleep(120);
		if (control_a2220_clk)
			gpio_set_value(pdata->gpio_a2220_clk, 0);
		return rc;
	}

	memset(ack_buf, 0, sizeof(ack_buf));
	msleep(20);
	pr_info("%s: CMD ACK block read start\n", __func__);
	rc = a2220_i2c_read(ack_buf, size);
	if (rc < 0) {
		printk("%s: CMD ACK block read error\n", __func__);
		a2220_i2c_sw_reset(sw_reset);
		return rc;
	} else {
		pr_info("%s: CMD ACK block read end\n", __func__);
#if DEBUG
		for (i = 1; i <= size; i++) {
			pr_info("%x ", ack_buf[i-1]);
			if ( !(i % 4))
				pr_info("\n");
		}
#endif
		index = ack_buf;
		number_of_cmd_sets = size / 4;
		do {
			if (*index == 0x00) {
				rd_retry_cnt = POLLING_RETRY_CNT;
rd_retry:
				if (rd_retry_cnt--) {
					memset(rdbuf, 0, sizeof(rdbuf));
					rc = a2220_i2c_read(rdbuf, 4);
					if (rc < 0)
						return rc;
#if DEBUG
					for (i = 0; i < sizeof(rdbuf); i++) {
						pr_info("0x%x\n", rdbuf[i]);
					}
					pr_info("-----------------\n");
#endif
					if (rdbuf[0] == 0x00) {
						msleep(20);
						goto rd_retry;
					}
				} else {
					printk("%s: CMD ACK Not Ready\n",
							__func__);
					return -EBUSY;
				}
			} else if (*index == 0xff) { /* illegal cmd */
				return -ENOEXEC;
			} else if (*index == 0x80) {
				index += 4;
			}
		} while (--number_of_cmd_sets);
	}
#endif
	//lsj -
input_err:
	return rc;
}

int execute_cmdmsg(unsigned int msg)
{
	int rc = 0;
	int retries, pass = 0;
	unsigned char msgbuf[4];
	unsigned char chkbuf[4];
	unsigned int sw_reset = 0;

#if 0
#ifdef AUDIENCE_BYPASS//(+)dragonball Multimedia mode	
	if (msg == A100_msg_Sleep) {
		printk("Multimedia mode skip sleep	\n");
		return rc;
	}	
#endif	//(+)dragonball Multimedia mode	
#endif

	sw_reset = ((A100_msg_Reset << 16) | RESET_IMMEDIATE);

	msgbuf[0] = (msg >> 24) & 0xFF;
	msgbuf[1] = (msg >> 16) & 0xFF;
	msgbuf[2] = (msg >> 8) & 0xFF;
	msgbuf[3] = msg & 0xFF;

	//printk("execute_cmdmsg +\n");
 #if DEBUG
	pr_debug(MODULE_NAME "%s : execute_cmdmsg :: %x %x %x %x\n" , __func__, msgbuf[0] , msgbuf[1] , msgbuf[2] , msgbuf[3]);
#endif
	memcpy(chkbuf, msgbuf, 4);

	rc = a2220_i2c_write(msgbuf, 4);
	if (rc < 0) {
		printk("%s: error %d\n", __func__, rc);
		a2220_i2c_sw_reset(sw_reset);

		if(msg == A100_msg_Sleep) {
			printk(MODULE_NAME "%s : execute_cmdmsg ...go to suspend first\n", __func__);
			a2220_suspended = 1; //(+)dragonball test for audience
			msleep(120);
			
		}
		return rc;
	}

	//printk("execute_cmdmsg + 1\n");
	/* We don't need to get Ack after sending out a suspend command */
	if (msg == A100_msg_Sleep) {
		printk(MODULE_NAME "%s : ...go to suspend first\n", __func__);
		a2220_suspended = 1; //(+)dragonball test for audience
		
		return rc;
	}
	//printk("execute_cmdmsg + 2\n");

	retries = POLLING_RETRY_CNT;
	while (retries--) {
		rc = 0;		
		memset(msgbuf, 0, sizeof(msgbuf));
		rc = a2220_i2c_read(msgbuf, 4);
		if (rc < 0) {
			printk("%s: ack-read error %d (%d retries)\n", __func__,
					rc, retries);
			continue;
		}

		//printk("execute_cmdmsg + 3\n");

		if (msgbuf[0] == 0x80  && msgbuf[1] == chkbuf[1]) {
			pass = 1;
			//printk("execute_cmdmsg + 4\n");
			break;
		} else if (msgbuf[0] == 0xff && msgbuf[1] == 0xff) {
			printk("%s: illegal cmd %08x\n", __func__, msg);
			rc = -EINVAL;
			//printk("execute_cmdmsg + 5\n");
			//break;
		} else if ( msgbuf[0] == 0x00 && msgbuf[1] == 0x00 ) {
			pr_info("%s: not ready (%d retries)\n", __func__,
					retries);
			//printk("execute_cmdmsg + 6\n");
			rc = -EBUSY;
		} else {
			pr_info("%s: cmd/ack mismatch: (%d retries left)\n",
					__func__,
					retries);
#if DEBUG
			pr_info("%s: msgbuf[0] = %x\n", __func__, msgbuf[0]);
			pr_info("%s: msgbuf[1] = %x\n", __func__, msgbuf[1]);
			pr_info("%s: msgbuf[2] = %x\n", __func__, msgbuf[2]);
			pr_info("%s: msgbuf[3] = %x\n", __func__, msgbuf[3]);
#endif
			printk("execute_cmdmsg + 7\n");
			rc = -EBUSY;
		}
		msleep(20); /* use polling */
	}

	if (!pass) {
		//printk("execute_cmdmsg + 8\n");
		printk("%s: failed execute cmd %08x (%d)\n", __func__,
				msg, rc);
		a2220_i2c_sw_reset(sw_reset);
	}

	//printk("execute_cmdmsg - finish\n");

	return rc;
}

#if ENABLE_DIAG_IOCTLS
static int a2220_set_mic_state(char miccase)
{
	int rc = 0;
	unsigned int cmd_msg = 0;

	switch (miccase) {
		case 1: /* Mic-1 ON / Mic-2 OFF */
			cmd_msg = 0x80260007;
			break;
		case 2: /* Mic-1 OFF / Mic-2 ON */
			cmd_msg = 0x80260015;
			break;
		case 3: /* both ON */
			cmd_msg = 0x80260001;
			break;
		case 4: /* both OFF */
			cmd_msg = 0x80260006;
			break;
		default:
			pr_info("%s: invalid input %d\n", __func__, miccase);
			rc = -EINVAL;
			break;
	}
	rc = execute_cmdmsg(cmd_msg);
	return rc;
}

static int exe_cmd_in_file(unsigned char *incmd)
{
	int rc = 0;
	int i = 0;
	unsigned int cmd_msg = 0;
	unsigned char tmp = 0;

	for (i = 0; i < 4; i++) {
		tmp = *(incmd + i);
		cmd_msg |= (unsigned int)tmp;
		if (i != 3)
			cmd_msg = cmd_msg << 8;
	}
	rc = execute_cmdmsg(cmd_msg);
	if (rc < 0)
		printk("%s: cmd %08x error %d\n", __func__, cmd_msg, rc);
	return rc;
}
#endif /* ENABLE_DIAG_IOCTLS */
/*
	Thread does the init process of Audience Chip
*/
static int a2220_init_thread(void *data)
{
	int rc = 0;
	struct a2220img img;
	printk(MODULE_NAME "%s\n", __func__);
	img.buf = a2220_firmware_buf;
	img.img_size = sizeof(a2220_firmware_buf);
	rc=a2220_bootup_init(&img);
	return rc;
}
	static int
#if defined(CONFIG_EUR_MODEL_GT_I9210)
a2220_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
#else
a2220_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
		unsigned long arg)
#endif
{
	void __user *argp = (void __user *)arg;
	struct a2220img img;
	int rc = 0;
#if ENABLE_DIAG_IOCTLS
	char msg[4];
	int mic_cases = 0;
	int mic_sel = 0;
#endif
	//int pathid = 0;
	unsigned int ns_state;

#ifdef CONFIG_EUR_MODEL_GT_I9210
	int amr_state;
#endif

	switch (cmd) {
		case A2220_BOOTUP_INIT:
			//lsj +
			//img.buf = 0;
			//img.img_size = 0;
			img.buf = a2220_firmware_buf;
			img.img_size = sizeof(a2220_firmware_buf);
			// lsj -
			printk(MODULE_NAME "%s : lsj:: a2220_firmware_buf = %d\n" , __func__, sizeof(a2220_firmware_buf));
			//if (copy_from_user(&img, argp, sizeof(img)))
			//	return -EFAULT;
			//rc = a2220_bootup_init(file, &img);
			//rc=a2220_bootup_init(&img);
			printk(MODULE_NAME "%s: creating thread..\n", __func__);
			task = kthread_run(a2220_init_thread, NULL, "a2220_init_thread");
			if (IS_ERR(task)) {
				rc = PTR_ERR(task);
				task = NULL;
			}
			break;
		case A2220_SET_CONFIG:
			//if (copy_from_user(&pathid, argp, sizeof(pathid)))
			//	return -EFAULT;
			rc = a2220_set_config(arg, A2220_CONFIG_FULL);
			if (rc < 0)
				printk("%s: A2220_SET_CONFIG (%lu) error %d!\n",
						__func__, arg, rc);
			break;
		case A2220_SET_NS_STATE:
			if (copy_from_user(&ns_state, argp, sizeof(ns_state)))
				return -EFAULT;
			pr_info("%s: set noise suppression %d\n", __func__, ns_state);
			if (ns_state < 0 || ns_state >= A2220_NS_NUM_STATES)
				return -EINVAL;
			a2220_NS_state = ns_state;
			if (!a2220_suspended)
				a2220_set_config(a2220_current_config,
						A2220_CONFIG_VP);
			break;
#ifdef CONFIG_EUR_MODEL_GT_I9210
		case A2220_SET_NETWORK_TYPE:
			if(copy_from_user(&amr_state, argp, sizeof(amr_state)))
				return -EFAULT;
			if(amr_state<0 || amr_state>1)
				return -EFAULT;
			a2220_change_network_type(amr_state);
			break;
#endif
#if ENABLE_DIAG_IOCTLS
		case A2220_SET_MIC_ONOFF:
			rc = chk_wakeup_a2220();
			if (rc < 0)
				return rc;
			if (copy_from_user(&mic_cases, argp, sizeof(mic_cases)))
				return -EFAULT;
			rc = a2220_set_mic_state(mic_cases);
			if (rc < 0)
				printk("%s: A2220_SET_MIC_ONOFF %d error %d!\n",
						__func__, mic_cases, rc);
			break;
		case A2220_SET_MICSEL_ONOFF:
			rc = chk_wakeup_a2220();
			if (rc < 0)
				return rc;
			if (copy_from_user(&mic_sel, argp, sizeof(mic_sel)))
				return -EFAULT;
			//gpio_set_value(pdata->gpio_a2220_micsel, !!mic_sel);
			rc = 0;
			break;
		case A2220_READ_DATA:
			rc = chk_wakeup_a2220();
			if (rc < 0)
				return rc;
			rc = a2220_i2c_read(msg, 4);
			if (copy_to_user(argp, &msg, 4))
				return -EFAULT;
			break;
		case A2220_WRITE_MSG:
			rc = chk_wakeup_a2220();
			if (rc < 0)
				return rc;
			if (copy_from_user(msg, argp, sizeof(msg)))
				return -EFAULT;
			rc = a2220_i2c_write(msg, 4);
			break;
		case A2220_SYNC_CMD:
			rc = chk_wakeup_a2220();
			if (rc < 0)
				return rc;
			msg[0] = 0x80;
			msg[1] = 0x00;
			msg[2] = 0x00;
			msg[3] = 0x00;
			rc = a2220_i2c_write(msg, 4);
			break;
		case A2220_SET_CMD_FILE:
			rc = chk_wakeup_a2220();
			if (rc < 0)
				return rc;
			if (copy_from_user(msg, argp, sizeof(msg)))
				return -EFAULT;
			rc = exe_cmd_in_file(msg);
			break;
#endif /* ENABLE_DIAG_IOCTLS */
		default:
			printk("%s: invalid command %d\n", __func__, _IOC_NR(cmd));
			rc = -EINVAL;
			break;
	}

	return rc;
}

//lsj +
int a2220_ioctl2(unsigned int cmd , unsigned long arg)
{
#if defined(CONFIG_EUR_MODEL_GT_I9210)
	a2220_ioctl(NULL, cmd, arg);
#else
	a2220_ioctl(NULL, NULL, cmd, arg);
#endif
	return 0;
}

EXPORT_SYMBOL(a2220_ioctl2);
//lsj -

static const struct file_operations a2220_fops = {
	.owner = THIS_MODULE,
	.open = a2220_open,
	.release = a2220_release,
	.unlocked_ioctl = a2220_ioctl,  /* The removal of the ioctl field happend in 2.6.36 so i changed to unlocked_ioctl for ICS */
};

static struct miscdevice a2220_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "audience_a2220",
	.fops = &a2220_fops,
};

static int a2220_probe(
		struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc = 0;

	pdata = client->dev.platform_data;

	if (pdata == NULL) {
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (pdata == NULL) {
			rc = -ENOMEM;
			printk("%s: platform data is NULL\n", __func__);
			goto err_alloc_data_failed;
		}
	}


	gpio_tlmm_config(GPIO_CFG(126,0,GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA),GPIO_CFG_ENABLE); // 2MIC_RST

#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
	if (get_hw_rev() >= 0x05)	
		gpio_tlmm_config(GPIO_CFG(33,0,GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA),GPIO_CFG_ENABLE); // 2MIC_PWDN
	else
		gpio_tlmm_config(GPIO_CFG(34,0,GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA),GPIO_CFG_ENABLE); // 2MIC_PWDN
#elif defined(CONFIG_USA_MODEL_SGH_I727)
	if (get_hw_rev() >= 0x05)	
	{
		pr_debug(MODULE_NAME "%s : GPIO 33\n", __func__);
		gpio_tlmm_config(GPIO_CFG(33,0,GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA),GPIO_CFG_ENABLE); // 2MIC_PWDN
	}
	else
	{
		pr_debug(MODULE_NAME "%s : get_hw_rev() == %d\n", __func__, get_hw_rev());
		pr_debug(MODULE_NAME "%s : GPIO 34\n", __func__);

		gpio_tlmm_config(GPIO_CFG(34,0,GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA),GPIO_CFG_ENABLE); // 2MIC_PWDN
	}
#elif defined(CONFIG_USA_MODEL_SGH_I717)
		pr_debug(MODULE_NAME "%s : GPIO 33\n", __func__);
		gpio_tlmm_config(GPIO_CFG(33,0,GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA),GPIO_CFG_ENABLE); // 2MIC_PWDN
		//gpio_tlmm_config(GPIO_CFG(34,0,GPIO_CFG_OUTPUT,GPIO_CFG_PULL_DOWN,GPIO_CFG_2MA),GPIO_CFG_ENABLE); // 2MIC_PWDN
#else
	if (pdata->gpio_a2220_wakeup) {
		pr_debug(MODULE_NAME "%s : Wakeup GPIO %d\n", __func__, pdata->gpio_a2220_wakeup);
		gpio_tlmm_config(GPIO_CFG(pdata->gpio_a2220_wakeup,0,GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA),GPIO_CFG_ENABLE); // 2MIC_PWDN
	} else {
		pr_debug(MODULE_NAME "%s : No Wakeup GPIO %d\n", __func__);
	}
#endif	

#if !defined(CONFIG_USA_MODEL_SGH_I727) && !defined(CONFIG_USA_MODEL_SGH_T989) && !defined(CONFIG_USA_MODEL_SGH_T769) &&  !defined(CONFIG_USA_MODEL_SGH_I717) && !defined(CONFIG_USA_MODEL_SGH_I757)//qup_a2220
	gpio_tlmm_config(GPIO_CFG(35,0,GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA),GPIO_CFG_ENABLE); // 2MIC_SDA_1.8V
	gpio_tlmm_config(GPIO_CFG(36,0,GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA),GPIO_CFG_ENABLE); // 2MIC_SCL_1.8V
#endif 

	//Handling GPIO Audience CIP Sel
#ifdef CONFIG_USA_MODEL_SGH_I717
	// No switch
#elif defined(CONFIG_USA_MODEL_SGH_I577)
	gpio_tlmm_config(GPIO_CFG(GPIO_SELECT_I2S_AUDIENCE_QTR,0,GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA),GPIO_CFG_ENABLE); //Audeince Chip Sel
	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 0);
#else
	gpio_tlmm_config(GPIO_CFG(GPIO_SELECT_I2S_AUDIENCE_QTR,0,GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA),GPIO_CFG_ENABLE); //Audeince Chip Sel
	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 1);
#endif

	this_client = client;

	if ( pdata->gpio_a2220_clk)
	{
		rc = gpio_request(pdata->gpio_a2220_clk, "a2220");
		if (rc < 0) {
			control_a2220_clk = 0;
			goto chk_gpio_micsel;
		}
		control_a2220_clk = 1;

		rc = gpio_direction_output(pdata->gpio_a2220_clk, 1);
		if (rc < 0) {
			printk("%s: request clk gpio direction failed\n", __func__);
			goto err_free_gpio_clk;
		}
	}	
chk_gpio_micsel:
	if (pdata->gpio_a2220_micsel)
	{
		rc = gpio_request(pdata->gpio_a2220_micsel, "a2220");
		if (rc < 0) {
			printk("%s: gpio request mic_sel pin failed\n", __func__);
			goto err_free_gpio_micsel;
		}

		rc = gpio_direction_output(pdata->gpio_a2220_micsel, 1);
		if (rc < 0) {
			printk("%s: request mic_sel gpio direction failed\n", __func__);
			goto err_free_gpio_micsel;
		}
	}


	if (pdata->gpio_a2220_wakeup)
	{
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
		if (get_hw_rev() >= 0x05 )	
			rc = gpio_request(WAKEUP_GPIO_NUM_HERCULES_REV01, "a2220");
		else
			rc = gpio_request(pdata->gpio_a2220_wakeup, "a2220");
#elif defined(CONFIG_USA_MODEL_SGH_I727)
		if (get_hw_rev() >= 0x05 )	
			rc = gpio_request(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, "a2220");
		else
			rc = gpio_request(pdata->gpio_a2220_wakeup, "a2220");			
#elif defined(CONFIG_USA_MODEL_SGH_I717)
		rc = gpio_request(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, "a2220");
#else	
		rc = gpio_request(pdata->gpio_a2220_wakeup, "a2220");
#endif
		if (rc < 0) {
			printk("%s: gpio request wakeup pin failed\n", __func__);
			goto err_free_gpio;
		}

#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
		if (get_hw_rev() >= 0x05 )	
			rc = gpio_direction_output(WAKEUP_GPIO_NUM_HERCULES_REV01, 1);
		else
			rc = gpio_direction_output(pdata->gpio_a2220_wakeup, 1);
#elif defined(CONFIG_USA_MODEL_SGH_I727)
		if (get_hw_rev() >= 0x05 )	
			rc = gpio_direction_output(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, 1);
		else
			rc = gpio_direction_output(pdata->gpio_a2220_wakeup, 1);
#elif defined(CONFIG_USA_MODEL_SGH_I717)
		rc = gpio_direction_output(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, 1);
#else	
		rc = gpio_direction_output(pdata->gpio_a2220_wakeup, 1);
#endif

		//		rc = gpio_direction_output(pdata->gpio_a2220_wakeup, 1);

		if (rc < 0) {
			printk("%s: request wakeup gpio direction failed\n", __func__);
			goto err_free_gpio;
		}
	}

	rc = gpio_request(pdata->gpio_a2220_reset, "a2220");
	if (rc < 0) {
		printk("%s: gpio request reset pin failed\n", __func__);
		goto err_free_gpio;
	}

	rc = gpio_direction_output(pdata->gpio_a2220_reset, 1);
	if (rc < 0) {
		printk("%s: request reset gpio direction failed\n", __func__);
		goto err_free_gpio_all;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk("%s: i2c check functionality error\n", __func__);
		rc = -ENODEV;
		goto err_free_gpio_all;
	}

	if (control_a2220_clk)
		gpio_set_value(pdata->gpio_a2220_clk, 1);
	if (pdata->gpio_a2220_micsel)
		gpio_set_value(pdata->gpio_a2220_micsel, 0);

	if (pdata->gpio_a2220_wakeup)
	{
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
		if (get_hw_rev() >= 0x05 )	
			gpio_set_value(WAKEUP_GPIO_NUM_HERCULES_REV01, 1);
		else
			gpio_set_value(pdata->gpio_a2220_wakeup, 1);
#elif defined(CONFIG_USA_MODEL_SGH_I727)
		if (get_hw_rev() >= 0x05)	
			gpio_set_value(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, 1);
		else
			gpio_set_value(pdata->gpio_a2220_wakeup, 1);
#elif defined(CONFIG_USA_MODEL_SGH_I717)	
		gpio_set_value(WAKEUP_GPIO_NUM_CELOX_ATT_REV05, 1);
#else	
		gpio_set_value(pdata->gpio_a2220_wakeup, 1);
#endif
	}		
	//gpio_set_value(pdata->gpio_a2220_wakeup, 1);

	gpio_set_value(pdata->gpio_a2220_reset, 1);
	
	if (pdata->gpio_a2220_audience_chip_sel)
		gpio_set_value(pdata->gpio_a2220_audience_chip_sel, 1);
	

	rc = misc_register(&a2220_device);
	if (rc) {
		printk("%s: a2220_device register failed\n", __func__);
		goto err_free_gpio_all;
	}

#ifdef CONFIG_VP_A2220 //lsj
	//printk("lsj::A2220 firmware download start ..\n");
	//a2220_ioctl2(A2220_BOOTUP_INIT , 0);	
	//printk("lsj::A2220 firmware download end \n");
#endif

	pr_debug(MODULE_NAME "%s : a2220_probe - finish\n", __func__);
	return 0;

err_free_gpio_all:
	gpio_free(pdata->gpio_a2220_reset);
err_free_gpio:
	if (pdata->gpio_a2220_wakeup)
	{
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
		if (get_hw_rev() >= 0x05 )	
			gpio_free(WAKEUP_GPIO_NUM_HERCULES_REV01);
		else
			gpio_free(pdata->gpio_a2220_wakeup);
#elif defined(CONFIG_USA_MODEL_SGH_I727)
		if (get_hw_rev() >= 0x05)	
			gpio_free(WAKEUP_GPIO_NUM_CELOX_ATT_REV05);
		else
			gpio_free(pdata->gpio_a2220_wakeup);
#elif defined(CONFIG_USA_MODEL_SGH_I717)	
		gpio_free(WAKEUP_GPIO_NUM_CELOX_ATT_REV05);
#else	
		gpio_free(pdata->gpio_a2220_wakeup);
#endif
	}
	//gpio_free(pdata->gpio_a2220_wakeup);
err_free_gpio_micsel:
	if (pdata->gpio_a2220_micsel)
		gpio_free(pdata->gpio_a2220_micsel);
err_free_gpio_clk:
	if (control_a2220_clk)
		gpio_free(pdata->gpio_a2220_clk);
err_alloc_data_failed:

	printk(MODULE_NAME "%s : a2220_probe - failed\n", __func__);
	return rc;
}

static int a2220_remove(struct i2c_client *client)
{
	struct a2220_platform_data *p1026data = i2c_get_clientdata(client);
	kfree(p1026data);

	return 0;
}

static int a2220_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int a2220_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id a2220_id[] = {
	{ "audience_a2220", 0 },
	{ }
};

static struct i2c_driver a2220_driver = {
	.probe = a2220_probe,
	.remove = a2220_remove,
	.suspend = a2220_suspend,
	.resume	= a2220_resume,
	.id_table = a2220_id,
	.driver = {
		.name = "audience_a2220",
	},
};

#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

static int __init a2220_init(void)
{
#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return 0\n", __func__);
		return 0;
	}
#endif
	pr_debug( MODULE_NAME "%s : lsj::a2220_init\n", __func__);
	pr_info("%s\n", __func__);
	mutex_init(&a2220_lock);

	return i2c_add_driver(&a2220_driver);
}

static void __exit a2220_exit(void)
{
	i2c_del_driver(&a2220_driver);
}

module_init(a2220_init);
module_exit(a2220_exit);

MODULE_DESCRIPTION("A2220 voice processor driver");
MODULE_LICENSE("GPL");
