/* drivers/misc/a2220b.c - a2220 voice processor driver
 *
 * Copyright (C) 2011 Samsung Corporation.
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

#ifdef CONFIG_USA_MODEL_SGH_I717
#include "a2220b_cmd_Q1.h"
#elif defined (CONFIG_USA_MODEL_SGH_I577)
#include "a2220b_cmd_A2.h"
#else
#include "a2220b_cmd.h"
#endif

#define MODULE_NAME "audience_a2220B:"
#define DEBUG			(0)

static struct i2c_client *this_client;
static struct a2220_platform_data *pdata;
static struct task_struct *task;
static int execute_cmdmsg(unsigned int);

static struct mutex a2220_lock;
static int a2220_opened;
static int a2220_suspended;
static int control_a2220_clk = 0;

static unsigned int a2220_NS_state = A2220_NS_STATE_AUTO;
static int a2220_current_config = A2220_PATH_SUSPEND;
static int a2220_param_ID;

extern unsigned int get_hw_rev(void);
int a2220_ctrl(unsigned int cmd , unsigned long arg);


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


static ssize_t a2220_bootup_init(struct a2220img *pImg)
{

	struct a2220img *vp = pImg;

	int rc, pass = 0;
	int remaining;
	int retry = RETRY_CNT;
	unsigned char *index;
	char buf[2];

	mdelay(100);
	printk(MODULE_NAME "%s : lsj::a2220_bootup_init AAAAAAAAAAAAAAAAAAAAAA +\n", __func__);
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

		remaining = vp->img_size / 32;
		index = vp->buf;
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

		msleep(200); 

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
	if (pdata->bypass == A2220_BYPASS_DATA_ONLY) {
		a2220_ctrl(A2220_SET_CONFIG , A2220_PATH_BYPASS_MULTIMEDIA);
	} else if (pdata->bypass == A2220_BYPASS_ALL) {
		a2220_ctrl(A2220_SET_CONFIG , A2220_PATH_BYPASS_MULTIMEDIA_ALL);	
	} else {
		/* Put A2220 into sleep mode */
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

	}

	printk(MODULE_NAME "%s : a2220_bootup_init - finish\n", __func__);

	return rc;
}

static ssize_t chk_wakeup_a2220(void)
{
	int rc = 0, retry = 4;

	if (a2220_suspended == 1) {
		/* Enable A2220 clock */
		if (control_a2220_clk) {
			gpio_set_value(pdata->gpio_a2220_clk, 1);
			mdelay(1);
		}

		if (pdata->gpio_a2220_wakeup)
		{
			printk(MODULE_NAME "%s : chk_wakeup_a2220  --> get_hw_rev of Target = %d\n", __func__, get_hw_rev());
			gpio_set_value(pdata->gpio_a2220_wakeup, 0);
		}
		msleep(100);

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

		if (pdata->gpio_a2220_wakeup)
		{	
			gpio_set_value(pdata->gpio_a2220_wakeup, 1);
		}

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
	

	printk("[AUD] new mode =%d\n", newid);	
	if ((a2220_suspended) && (newid == A2220_PATH_SUSPEND))
		return rc;

	if(a2220_current_config == newid)
	{
		printk("already configured this path!!! \n");
		return rc;
	}

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

		case A2220_PATH_BYPASS_MULTIMEDIA:
			pr_debug(MODULE_NAME "%s : setting A2220_PATH_BYPASS_MULTIMEDIA\n", __func__);
			i2c_cmds = bypass_multimedia;
			size = sizeof(bypass_multimedia);
			break;
		case A2220_PATH_INCALL_HEADSET:
			i2c_cmds = phonecall_headset;
			size = sizeof(phonecall_headset);
			break;
		case A2220_PATH_INCALL_SPEAKER:
			i2c_cmds = phonecall_speaker;
			size = sizeof(phonecall_speaker);
			break;
		case A2220_PATH_INCALL_BT:
			i2c_cmds = phonecall_bt;
			size = sizeof(phonecall_bt);
			break;
		case A2220_PATH_INCALL_TTY:
			i2c_cmds = phonecall_tty;
			size = sizeof(phonecall_tty);
			break;
		case A2220_PATH_VR_NO_NS_RECEIVER:
			i2c_cmds = vr_no_ns_receiver;
			size = sizeof(vr_no_ns_receiver);
			break;
		case A2220_PATH_VR_NO_NS_HEADSET:
			i2c_cmds = vr_no_ns_headset;
			size = sizeof(vr_no_ns_headset);
			break;
		case A2220_PATH_VR_NO_NS_SPEAKER:
			i2c_cmds = vr_no_ns_speaker;
			size = sizeof(vr_no_ns_speaker);
			break;
		case A2220_PATH_VR_NO_NS_BT:
			i2c_cmds = vr_no_ns_bt;
			size = sizeof(vr_no_ns_bt);
			break;
		case A2220_PATH_VR_NS_RECEIVER:
			i2c_cmds = vr_ns_receiver;
			size = sizeof(vr_ns_receiver);
			break;
		case A2220_PATH_VR_NS_HEADSET:
			i2c_cmds = vr_ns_headset;
			size = sizeof(vr_ns_headset);
			break;
		case A2220_PATH_VR_NS_SPEAKER:
			i2c_cmds = vr_ns_speaker;
			size = sizeof(vr_ns_speaker);
			break;
		case A2220_PATH_VR_NS_BT:
			i2c_cmds = vr_ns_bt;
			size = sizeof(vr_ns_bt);
			break;
		case A2220_PATH_RECORD_RECEIVER:
			i2c_cmds = INT_MIC_recording_receiver;
			size = sizeof(INT_MIC_recording_receiver);
			break;
		case A2220_PATH_RECORD_HEADSET:
			i2c_cmds = EXT_MIC_recording;
			size = sizeof(EXT_MIC_recording);
			break;
		case A2220_PATH_RECORD_SPEAKER:
			i2c_cmds = INT_MIC_recording_speaker;
			size = sizeof(INT_MIC_recording_speaker);
			break;
		case A2220_PATH_RECORD_BT:
			i2c_cmds = phonecall_bt;
			size = sizeof(phonecall_bt);
			break;
		case A2220_PATH_SUSPEND:
			i2c_cmds = (unsigned char *)suspend_mode;
			size = sizeof(suspend_mode);
			break;
		case A2220_PATH_CAMCORDER:
			i2c_cmds = BACK_MIC_recording;
			size = sizeof(BACK_MIC_recording);
			break;
		case A2220_PATH_BYPASS_MULTIMEDIA_ALL:
			pr_debug(MODULE_NAME "%s : setting A2220_PATH_BYPASS_MULTIMEDIA_ALL\n", __func__);
			i2c_cmds = bypass_multimedia_all;
			size = sizeof(bypass_multimedia_all);
			break;
		default:
			printk("%s: invalid cmd %d\n", __func__, newid);
			rc = -1;
			goto input_err;
			break;
	}

	a2220_current_config = newid;

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
		if((retry == 0)&&(rc < 0)) {
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

	sw_reset = ((A100_msg_Reset << 16) | RESET_IMMEDIATE);

	msgbuf[0] = (msg >> 24) & 0xFF;
	msgbuf[1] = (msg >> 16) & 0xFF;
	msgbuf[2] = (msg >> 8) & 0xFF;
	msgbuf[3] = msg & 0xFF;

	memcpy(chkbuf, msgbuf, 4);

	rc = a2220_i2c_write(msgbuf, 4);
	if (rc < 0) {
		printk("%s: error %d\n", __func__, rc);
		a2220_i2c_sw_reset(sw_reset);

		if(msg == A100_msg_Sleep) {
			printk(MODULE_NAME "%s : execute_cmdmsg ...go to suspend first\n", __func__);
			a2220_suspended = 1; 
			msleep(120);
			
		}
		return rc;
	}

	/* We don't need to get Ack after sending out a suspend command */
	if (msg == A100_msg_Sleep) {
		printk(MODULE_NAME "%s : ...go to suspend first\n", __func__);
		a2220_suspended = 1; 
		
		return rc;
	}

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
#if 1
		printk(MODULE_NAME "%s : execute_cmdmsg CHK :: %x %x %x %x\n" , __func__, chkbuf[0] , chkbuf[1] , chkbuf[2] , chkbuf[3]);
		printk(MODULE_NAME "%s : execute_cmdmsg :: %x %x %x %x\n" , __func__, msgbuf[0] , msgbuf[1] , msgbuf[2] , msgbuf[3]);
#endif
#ifdef CONFIG_USA_MODEL_SGH_I577
		static int temp_pass = 0;
		if (msgbuf[0] == 0x80 && msgbuf[1] == 0x17 && msgbuf[2] == 0x00 && msgbuf[3] == 0x02) {
			pr_info(MODULE_NAME "%s : 0x%08x set the temp_pass\n", __func__, msg);
			temp_pass = 1;
		}
		if ((msgbuf[0] == 0xff && msgbuf[1] == 0xff) && temp_pass) {
			temp_pass = 0;
			pr_err(MODULE_NAME "%s :  illegal cmd %08x but skip the reset because it is not fail on SGH-I577\n", __func__, msg);
			pass = 1;
			break;
		}
#endif
		if (msgbuf[0] == 0x80  && msgbuf[1] == chkbuf[1]) {
			pass = 1;
			break;
		} else if (msgbuf[0] == 0xff && msgbuf[1] == 0xff) {
			printk("%s: illegal cmd %08x\n", __func__, msg);
			rc = -EINVAL;
			break;
		} else if ( msgbuf[0] == 0x00 && msgbuf[1] == 0x00 ) {
			pr_info("%s: not ready (%d retries)\n", __func__,
					retries);
			rc = -EBUSY;
		} else {
			pr_info("%s: cmd/ack mismatch: (%d retries left)\n",
					__func__,
					retries);
			rc = -EBUSY;
		}
		msleep(20); /* use polling */
	}

	if (!pass) {
		printk("%s: failed execute cmd %08x (%d)\n", __func__,
				msg, rc);
		a2220_i2c_sw_reset(sw_reset);
	}

	return rc;
}

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

static int a2220_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
		unsigned long arg)
{
	return a2220_ctrl(cmd ,arg);
}

int a2220_ctrl(unsigned int cmd , unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct a2220img img;
	int rc = 0;
	unsigned int ns_state;

	switch (cmd) {
		case A2220_BOOTUP_INIT:
			img.buf = a2220_firmware_buf;
			img.img_size = sizeof(a2220_firmware_buf);
			printk(MODULE_NAME "%s : lsj:: a2220_firmware_buf = %d\n" , __func__, sizeof(a2220_firmware_buf));
			printk(MODULE_NAME "%s: creating thread..\n", __func__);
			task = kthread_run(a2220_init_thread, NULL, "a2220_init_thread");
			if (IS_ERR(task)) {
				rc = PTR_ERR(task);
				task = NULL;
			}
			break;
		case A2220_SET_CONFIG:
			if (pdata->use_cases[arg] == 0)
				return true;
			
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
		default:
			printk("%s: invalid command %d\n", __func__, _IOC_NR(cmd));
			rc = -EINVAL;
			break;
	}

	return rc;
}

EXPORT_SYMBOL(a2220_ctrl);

static const struct file_operations a2220_fops = {
	.owner = THIS_MODULE,
	.open = a2220_open,
	.release = a2220_release,
//temp	.ioctl = a2220_ioctl,
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

	this_client = client;

	if ( pdata->gpio_a2220_clk) {
		rc = gpio_request(pdata->gpio_a2220_clk, "a2220_clk");
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
	if (pdata->gpio_a2220_micsel) {
		rc = gpio_request(pdata->gpio_a2220_micsel, "a2220_micsel");
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


	if (pdata->gpio_a2220_wakeup) {
		gpio_tlmm_config(GPIO_CFG(pdata->gpio_a2220_wakeup,0,GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA),GPIO_CFG_ENABLE); // 2MIC_PWDN

		rc = gpio_request(pdata->gpio_a2220_wakeup, "a2220_wakeup");

		if (rc < 0) {
			printk("%s: gpio request wakeup pin failed\n", __func__);
			goto err_free_gpio;
		}

		rc = gpio_direction_output(pdata->gpio_a2220_wakeup, 1);

		if (rc < 0) {
			printk("%s: request wakeup gpio direction failed\n", __func__);
			goto err_free_gpio;
		}
	}
	if (pdata->gpio_a2220_reset) {
		gpio_tlmm_config(GPIO_CFG(pdata->gpio_a2220_reset,0,GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA),GPIO_CFG_ENABLE); // 2MIC_RST
		rc = gpio_request(pdata->gpio_a2220_reset, "a2220_reset");
		if (rc < 0) {
			printk("%s: gpio request reset pin failed\n", __func__);
			goto err_free_gpio;
		}

		rc = gpio_direction_output(pdata->gpio_a2220_reset, 1);
		if (rc < 0) {
			printk("%s: request reset gpio direction failed\n", __func__);
			goto err_free_gpio_all;
		}
	}
	if (pdata->gpio_a2220_uart_out) {
		gpio_tlmm_config(GPIO_CFG(pdata->gpio_a2220_uart_out,0,GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_2MA),GPIO_CFG_ENABLE); // 2MIC_UART_OUT
		rc = gpio_request(pdata->gpio_a2220_uart_out, "a2220_uart_out");

		if (rc < 0) {
			printk("%s: gpio request uart_out pin failed\n", __func__);
			goto err_free_gpio;
		}

		rc = gpio_direction_output(pdata->gpio_a2220_uart_out, 0);

		if (rc < 0) {
			printk("%s: request uart_out gpio direction failed\n", __func__);
			goto err_free_gpio;
		}
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
		gpio_set_value(pdata->gpio_a2220_wakeup, 1);
	}		
	gpio_set_value(pdata->gpio_a2220_reset, 1);
	
	if (pdata->gpio_a2220_audience_chip_sel)
		gpio_set_value(pdata->gpio_a2220_audience_chip_sel, 1);
	

	rc = misc_register(&a2220_device);
	if (rc) {
		printk("%s: a2220_device register failed\n", __func__);
		goto err_free_gpio_all;
	}

	pr_debug(MODULE_NAME "%s : a2220_probe - finish\n", __func__);
	return 0;

err_free_gpio_all:
	gpio_free(pdata->gpio_a2220_reset);
err_free_gpio:
	if (pdata->gpio_a2220_wakeup)
	{
		gpio_free(pdata->gpio_a2220_wakeup);
	}
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
