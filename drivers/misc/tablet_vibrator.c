/* drivers/misc/p5-vibrator.c
 *
 * Copyright (C) 2011 Samsung Electronics Co. Ltd. All Rights Reserved.
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

#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/pwm.h>
#include <linux/wakelock.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/workqueue.h>

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include <asm/mach-types.h>
//#include <mach/gpio-sec.h>
//#include <mach/pinmux.h>
#include <linux/timed_output.h>
#include <linux/isa1200_vibrator.h>

#include <linux/fs.h>
#include <asm/uaccess.h>


#if 0
#define MOTOR_DEBUG
#endif

struct isa1200_vibrator_drvdata {
	struct timed_output_dev dev;
	struct hrtimer timer;
	struct work_struct work;
	struct clk *vib_clk;
	struct i2c_client *client;
	spinlock_t lock;
	bool running;
	int gpio_en;
	int max_timeout;
	u8 ctrl0;
	u8 ctrl1;
	u8 ctrl2;
	u8 ctrl4;
	u8 pll;
	u8 duty;
	u8 period;
};

struct clk *android_vib_clk; /* sfpb_clk */

#ifdef CONFIG_IMAGIS_AUTOHAPTIC
#undef DEBUG_HPTDRV

#define IOCTL_I2C_READ						0x5295
#define IOCTL_I2C_WRITE						0x5296

#define IOCTL_HPT_ENABLE						0x5293
#define IOCTL_HPT_DISABLE						0x5294

#define IOCTL_PWMVIB_ON				0x5997
#define IOCTL_PWMVIB_OFF			0x5998

static struct class *haptic_class;

#define AUDIO_HAPTIC


#ifdef AUDIO_HAPTIC
/* Audio Haptic */
#define IOCTL_SNDHPT_STATUS_SET				0x5297
#define IOCTL_SNDHPT_STATUS_GET				0x5298

typedef unsigned short   sndhpt_status_t;

/* Union of Audio Haptic Status */
typedef union {
    sndhpt_status_t mem;
    struct {
        unsigned char enable	: 1;
		unsigned char strength  : 3;
		unsigned char sndvol    : 4;
		unsigned char mode	    : 1;
		unsigned char rsrvd     : 7;
	} _SNDHPT_STATUS;
} SNDHPT_STATUS;

SNDHPT_STATUS SndHptStatus;
#endif

typedef unsigned char   reg_data_t;
typedef struct {
    reg_data_t addr;
    reg_data_t val[2];
} _HPTREG_INFO;

bool flag_chip_en =0;
#endif


#ifdef CONFIG_VIBETONZ
static struct isa1200_vibrator_drvdata	*g_drvdata;
static int isa1200_vibrator_i2c_write(struct i2c_client *client, u8 addr, u8 val);
int vibtonz_i2c_write(u8 addr, int length, u8 *data)
{
	if (NULL == g_drvdata) {
		pr_err("[VIB] driver is not ready\n");
		return -EFAULT;
	}

	if (2 != length)
		pr_err("[VIB] length should be 2(len:%d)\n", length);

#ifdef MOTOR_DEBUG
		pr_info("[VIB] data : %x, %x\n", data[0], data[1]);
#endif

	return isa1200_vibrator_i2c_write(g_drvdata->client, data[0], data[1]);
}

int vibtonz_clk_enable(bool en)
{
	if (NULL == g_drvdata) {
		pr_err("[VIB] driver is not ready\n");
		return -EFAULT;
	}

	if (g_drvdata->vib_clk) {
		if (en) {
			clk_enable(g_drvdata->vib_clk);
		} else {
			clk_disable(g_drvdata->vib_clk);
		}
	}

#ifdef CONFIG_IMAGIS_AUTOHAPTIC
	flag_chip_en = en ? true : false;
#endif
	return 0;
}

int vibtonz_chip_enable(bool en)
{
	if (NULL == g_drvdata) {
		pr_err("[VIB] driver is not ready\n");
		return -EFAULT;
	}

	gpio_direction_output(g_drvdata->gpio_en, en ? true : false);
#ifdef CONFIG_IMAGIS_AUTOHAPTIC
	flag_chip_en = en ? true : false;
#endif
	return 0;
}
#endif

#ifdef CONFIG_IMAGIS_AUTOHAPTIC
static int vibrator_write_register(u8 addr, u8 w_data)
{
	int ret;

	ret = i2c_smbus_write_byte_data(g_drvdata->client, addr, w_data);

	if (ret < 0) {
		pr_err("%s: Failed to write addr=[0x%x], data=[0x%x] (ret:%d)\n",
		   __func__, addr, w_data, ret);
		return -1;
	}

	return 0;
}

static int vibrator_read_register(u8 addr)
{
	int ret;

	ret = i2c_smbus_read_byte_data(g_drvdata->client, addr);

	if (ret < 0) {
		pr_err("%s: Failed to write addr=[0x%x], (ret:%d)\n",
		   __func__, addr, ret);
		return -1;
	}

	return ret;
}
#endif

static int isa1200_vibrator_i2c_write(struct i2c_client *client, u8 addr, u8 val)
{
	int error = 0;
	error = i2c_smbus_write_byte_data(client, addr, val);
	if (error)
		pr_err("[VIB] Failed to write addr=[0x%x], val=[0x%x]\n",
				addr, val);

	return error;
}

static void isa1200_vibrator_hw_init(struct isa1200_vibrator_drvdata *data)
{
	gpio_direction_output(data->gpio_en, 1);
	msleep(20);
	isa1200_vibrator_i2c_write(data->client, HAPTIC_CONTROL_REG0, data->ctrl0);
	isa1200_vibrator_i2c_write(data->client, HAPTIC_CONTROL_REG1, data->ctrl1);
	isa1200_vibrator_i2c_write(data->client, HAPTIC_CONTROL_REG2, data->ctrl2);
	isa1200_vibrator_i2c_write(data->client, HAPTIC_PLL_REG, data->pll);
	isa1200_vibrator_i2c_write(data->client, HAPTIC_CONTROL_REG4, data->ctrl4);
	isa1200_vibrator_i2c_write(data->client, HAPTIC_PWM_DUTY_REG, data->period/2);
	isa1200_vibrator_i2c_write(data->client, HAPTIC_PWM_PERIOD_REG, data->period);

#ifdef MOTOR_DEBUG
	pr_info("[VIB] ctrl0 = 0x%x\n", data->ctrl0);
	pr_info("[VIB] ctrl1 = 0x%x\n", data->ctrl1);
	pr_info("[VIB] ctrl2 = 0x%x\n", data->ctrl2);
	pr_info("[VIB] pll = 0x%x\n", data->pll);
	pr_info("[VIB] ctrl4 = 0x%x\n", data->ctrl4);
	pr_info("[VIB] duty = 0x%x\n", data->period/2);
	pr_info("[VIB] period = 0x%x\n", data->period);
	pr_info("[VIB] gpio_en = 0x%x\n", data->gpio_en);
#endif

}

static void isa1200_vibrator_on(struct isa1200_vibrator_drvdata *data)
{
	isa1200_vibrator_i2c_write(data->client,
		HAPTIC_CONTROL_REG0, data->ctrl0 | CTL0_NORMAL_OP);
	isa1200_vibrator_i2c_write(data->client,
		HAPTIC_PWM_DUTY_REG, data->duty - 3);
#ifdef MOTOR_DEBUG
	pr_info("[VIB] ctrl0 = 0x%x\n", data->ctrl0 | CTL0_NORMAL_OP);
	pr_info("[VIB] duty = 0x%x\n", data->duty);
#endif
}

static void isa1200_vibrator_off(struct isa1200_vibrator_drvdata *data)
{
	isa1200_vibrator_i2c_write(data->client,
		HAPTIC_PWM_DUTY_REG, data->period/2);
	isa1200_vibrator_i2c_write(data->client,
		HAPTIC_CONTROL_REG0, data->ctrl0);
#ifdef CONFIG_IMAGIS_AUTOHAPTIC
	flag_chip_en = false;
#endif
}

static void isa1200_vibrator_set_val(struct isa1200_vibrator_drvdata *data, int val)
{
	if (0 == val) {
		if (!data->running)
			return ;

		data->running = false;
		isa1200_vibrator_off(data);
		clk_disable(data->vib_clk);
//		gpio_set_value(ddata->gpio_en, 1);		//HASH

	} else {
		if (data->running)
			return ;

		data->running = true;

		clk_enable(data->vib_clk);
		mdelay(1);
		isa1200_vibrator_on(data);
	}

}

static enum hrtimer_restart isa1200_vibrator_timer_func(struct hrtimer *_timer)
{
	struct isa1200_vibrator_drvdata *data =
		container_of(_timer, struct isa1200_vibrator_drvdata, timer);

	schedule_work(&data->work);
	return HRTIMER_NORESTART;
}

static void isa1200_vibrator_work(struct work_struct *_work)
{
	struct isa1200_vibrator_drvdata *data =
		container_of(_work, struct isa1200_vibrator_drvdata, work);

	isa1200_vibrator_set_val(data, 0);
}

static int isa1200_vibrator_get_time(struct timed_output_dev *_dev)
{
	struct isa1200_vibrator_drvdata	*data =
		container_of(_dev, struct isa1200_vibrator_drvdata, dev);

	if (hrtimer_active(&data->timer)) {
		ktime_t r = hrtimer_get_remaining(&data->timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	} else
		return 0;
}

static void isa1200_vibrator_enable(struct timed_output_dev *_dev, int value)
{
	struct isa1200_vibrator_drvdata	*data =
		container_of(_dev, struct isa1200_vibrator_drvdata, dev);
	unsigned long	flags;

#ifdef CONFIG_KERNEL_DEBUG_SEC
	pr_info("[VIB] time = %dms\n", value);
#endif
	cancel_work_sync(&data->work);
	spin_lock_irqsave(&data->lock, flags);
	hrtimer_cancel(&data->timer);
	isa1200_vibrator_set_val(data, value);
	if (value > 0) {
		if (value > data->max_timeout)
			value = data->max_timeout;

		hrtimer_start(&data->timer,
			ns_to_ktime((u64)value * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&data->lock, flags);
}

#ifdef CONFIG_IMAGIS_AUTOHAPTIC
static int haptic_ioctl(struct file *file, unsigned int cmd, unsigned long arg)

{
	_HPTREG_INFO reg_info;

//	printk("Imagis_haptic.c [%s] [%x]\n",__FUNCTION__, cmd);

    if(copy_from_user(&reg_info, (void *)arg, sizeof(reg_info))){
		printk("copy from user fail in Imagis_haptic.c\n");
        return -EFAULT;
	}

	switch (cmd)
	{
        case IOCTL_I2C_READ:
            reg_info.val[0] = vibrator_read_register(reg_info.addr);

			#ifdef DEBUG_HPTDRV
            printk("HptDrv - r[0x%02x]: 0x%02x\n", reg_info.addr, reg_info.val[0]);
			#endif
            if (copy_to_user((struct reg_data_t *)arg, &reg_info, sizeof(reg_info)))
                return -EFAULT;
		break;

        case IOCTL_I2C_WRITE:
            vibrator_write_register(reg_info.addr, reg_info.val[0]);

			#ifdef DEBUG_HPTDRV
			printk("HptDrv - w[0x%02x]: 0x%02x\n", reg_info.addr, reg_info.val[0]);
			#endif
            break;


        case IOCTL_HPT_ENABLE:
            vibrator_write_register(0x30, 0x91);
            printk("IOCTL_HPT_ENABLE  \n");
            break;

		case IOCTL_HPT_DISABLE:
			vibrator_write_register(0x30, 0x11);
			printk("IOCTL_HPT_DISABLE  \n");
			break;


		case IOCTL_PWMVIB_ON:
//			imm_vibrator_clk_enable();
			vibtonz_clk_enable(1);
			printk("IOCTL_PWMVIB_ON  \n");
			break;

		case IOCTL_PWMVIB_OFF:
//			imm_vibrator_clk_disable();
			vibtonz_clk_enable(0);
			printk("IOCTL_PWMVIB_OFF  \n");
			break;

#ifdef AUDIO_HAPTIC
		case IOCTL_SNDHPT_STATUS_SET:
			#ifdef DEBUG_HPTDRV
	            		printk("HptDrv - SndHpt Status set: 0x%04x\n", reg_info.val[0] | reg_info.val[1]);
			#endif

			SndHptStatus.mem = (sndhpt_status_t) ((reg_info.val[0]<<8) | reg_info.val[1]);
			#ifdef DEBUG_HPTDRV
			printk("         enable: %d, strength: %d, sndvol: %d, mode: %d\n",
			        SndHptStatus._SNDHPT_STATUS.enable, SndHptStatus._SNDHPT_STATUS.strength,
			        SndHptStatus._SNDHPT_STATUS.sndvol, SndHptStatus._SNDHPT_STATUS.mode);
			#endif
            break;

        case IOCTL_SNDHPT_STATUS_GET:
            reg_info.val[0] = (SndHptStatus.mem>>8) & 0xff;
            reg_info.val[1] = SndHptStatus.mem & 0xff;

			#ifdef DEBUG_HPTDRV
			printk("HptDrv - SndHpt Status get: 0x%04x\n", reg_info.val[0] | reg_info.val[1]);
			#endif

            if (copy_to_user((struct reg_data_t *)arg, &reg_info, sizeof(reg_info)))
                return -EFAULT;
			if(flag_chip_en==false){
				vibtonz_clk_enable(1);
				vibtonz_chip_enable(1);
				vibrator_write_register(0x31, 0xc0);
				vibrator_write_register(0x33, 0x23);
				vibrator_write_register(0x36, 0x74);
				vibrator_write_register(0x30, 0x91);
				flag_chip_en = true;
			}
            break;
#endif

        default:
            return -ENOIOCTLCMD;
	}

	return 0;
}



#define MAJOR_HAPTIC 223
static const struct file_operations haptic_fileops = {
    	.owner   	= THIS_MODULE,
    	//.ioctl   	= haptic_ioctl,
		.unlocked_ioctl	= haptic_ioctl,
};
#endif

static int __devinit isa1200_vibrator_i2c_probe(struct i2c_client *client,	const struct i2c_device_id *id)
{
	struct isa1200_vibrator_platform_data *pdata = client->dev.platform_data;
	struct isa1200_vibrator_drvdata *ddata;
	int ret = 0;

	printk("[VIB] %s, ln:%d \n", __func__, __LINE__);
	ddata = kzalloc(sizeof(struct isa1200_vibrator_drvdata), GFP_KERNEL);
	if (NULL == ddata) {
		pr_err("[VIB] Failed to alloc memory\n");
		ret = -ENOMEM;
		goto err_free_mem;
	}

	ddata->client = client;
	ddata->gpio_en = pdata->gpio_en;
#if defined(CONFIG_TARGET_SERIES_P8LTE)	  
	android_vib_clk = clk_get_sys("vibrator","core_clk");
	if(IS_ERR(android_vib_clk)) {
		printk("android vib clk failed!!!\n");
		ddata->vib_clk = NULL;
	} else {
		clk_set_rate(android_vib_clk, 27000000);
		ddata->vib_clk = android_vib_clk;
	}
#else
	android_vib_clk = clk_get_sys("msm_sys_fpb","bus_clk");
	if(IS_ERR(android_vib_clk)) {
		printk("android vib clk failed!!!\n");
		ddata->vib_clk = NULL;
	} else {
		ddata->vib_clk = android_vib_clk;
	}
#endif
	ddata->max_timeout = pdata->max_timeout;
	ddata->ctrl0 = pdata->ctrl0;
	ddata->ctrl1 = pdata->ctrl1;
	ddata->ctrl2 = pdata->ctrl2;
	ddata->ctrl4 = pdata->ctrl4;
	ddata->pll = pdata->pll;
	ddata->duty = pdata->duty;
	ddata->period = pdata->period;

	ddata->dev.name = "vibrator";
	ddata->dev.get_time = isa1200_vibrator_get_time;
	ddata->dev.enable = isa1200_vibrator_enable;

	hrtimer_init(&ddata->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ddata->timer.function = isa1200_vibrator_timer_func;
	INIT_WORK(&ddata->work, isa1200_vibrator_work);
	spin_lock_init(&ddata->lock);

	gpio_set_value(ddata->gpio_en, 1);		//HASH

//	ret = gpio_request(ddata->gpio_en, "vib_en");
	if (ret < 0) {
		pr_err("[VIB] Failed to request gpio %d\n", ddata->gpio_en);
		goto err_gpio_req2;
	}

	//platform_set_drvdata(client, ddata);
	i2c_set_clientdata(client, ddata);
	isa1200_vibrator_hw_init(ddata);

	#ifdef CONFIG_IMAGIS_AUTOHAPTIC
	if (register_chrdev(MAJOR_HAPTIC, "haptic",&haptic_fileops)){
		printk("unable to get major %d for haptic devs\n", MAJOR_HAPTIC);
		return -ENOMEM;
	}
	printk("registered chardev : haptic!\n");


	haptic_class = class_create(THIS_MODULE, "haptic");
	device_create(haptic_class, NULL, MKDEV(MAJOR_HAPTIC, 0), NULL, "haptic" );
	#endif

	ret = timed_output_dev_register(&ddata->dev);
	if (ret < 0) {
		pr_err("[VIB] Failed to register timed_output : -%d\n", ret);
		goto err_to_dev_reg;
	}

#ifdef CONFIG_VIBETONZ
	g_drvdata = ddata;
#endif
	return 0;

err_to_dev_reg:
	gpio_free(ddata->gpio_en);
err_gpio_req2:

err_free_mem:
	kfree(ddata);
	return ret;

}

static int isa1200_vibrator_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct isa1200_vibrator_drvdata *ddata  = i2c_get_clientdata(client);
	gpio_direction_output(ddata->gpio_en, 0);

	return 0;
}

static int isa1200_vibrator_resume(struct i2c_client *client)
{
	struct isa1200_vibrator_drvdata *ddata  = i2c_get_clientdata(client);
	isa1200_vibrator_hw_init(ddata);

	return 0;
}

static const struct i2c_device_id isa1200_vibrator_device_id[] = {
	{"isa1200", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, isa1200_vibrator_device_id);

static struct i2c_driver isa1200_vibrator_i2c_driver = {
	.driver = {
		.name = "isa1200",
		.owner = THIS_MODULE,
	},
	.probe     = isa1200_vibrator_i2c_probe,
	.id_table  = isa1200_vibrator_device_id,
	.suspend	= isa1200_vibrator_suspend,
	.resume	= isa1200_vibrator_resume,
};

static int __init isa1200_vibrator_init(void)
{
	printk("[VIB] %s, ln:%d \n", __func__, __LINE__);
	return i2c_add_driver(&isa1200_vibrator_i2c_driver);
}

static void __exit isa1200_vibrator_exit(void)
{
	i2c_del_driver(&isa1200_vibrator_i2c_driver);
}

module_init(isa1200_vibrator_init);
module_exit(isa1200_vibrator_exit);
