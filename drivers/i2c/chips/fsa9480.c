/*
 * driver/misc/fsa9480.c - FSA9480 micro USB switch device driver
 *
 * Copyright (C) 2010 Samsung Electronics
 * Minkyu Kang <mk7.kang@samsung.com>
 * Wonguk Jeong <wonguk.jeong@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/i2c/fsa9480.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
//#include <mach/param.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/pmic8058.h>
#include <linux/input.h>
#if defined(CONFIG_VIDEO_MHL_V1) || defined(CONFIG_VIDEO_MHL_V2)
#include <linux/sii9234.h>
#endif

/* FSA9480 I2C registers */
#define FSA9480_REG_DEVID		0x01
#define FSA9480_REG_CTRL		0x02
#define FSA9480_REG_INT1		0x03
#define FSA9480_REG_INT2		0x04
#define FSA9480_REG_INT1_MASK		0x05
#define FSA9480_REG_INT2_MASK		0x06
#define FSA9480_REG_ADC			0x07
#define FSA9480_REG_TIMING1		0x08
#define FSA9480_REG_TIMING2		0x09
#define FSA9480_REG_DEV_T1		0x0a
#define FSA9480_REG_DEV_T2		0x0b
#define FSA9480_REG_BTN1		0x0c
#define FSA9480_REG_BTN2		0x0d
#define FSA9480_REG_CK			0x0e
#define FSA9480_REG_CK_INT1		0x0f
#define FSA9480_REG_CK_INT2		0x10
#define FSA9480_REG_CK_INTMASK1		0x11
#define FSA9480_REG_CK_INTMASK2		0x12
#define FSA9480_REG_MANSW1		0x13
#define FSA9480_REG_MANSW2		0x14
#define FSA9480_REG_MANUAL_OVERRIDES1	0x1B
#define FSA9480_REG_RESERVED_1D		0x20

/* Control */
#define CON_SWITCH_OPEN		(1 << 4)
#define CON_RAW_DATA		(1 << 3)
#define CON_MANUAL_SW		(1 << 2)
#define CON_WAIT		(1 << 1)
#define CON_INT_MASK		(1 << 0)
#define CON_MASK		(CON_SWITCH_OPEN | CON_RAW_DATA | \
				CON_MANUAL_SW | CON_WAIT)

/* Device Type 1 */
#define DEV_USB_OTG		(1 << 7)
#define DEV_DEDICATED_CHG	(1 << 6)
#define DEV_USB_CHG		(1 << 5)
#define DEV_CAR_KIT		(1 << 4)
#define DEV_UART		(1 << 3)
#define DEV_USB			(1 << 2)
#define DEV_AUDIO_2		(1 << 1)
#define DEV_AUDIO_1		(1 << 0)

#define DEV_T1_USB_MASK		(DEV_USB_OTG | DEV_USB)
#define DEV_T1_UART_MASK	(DEV_UART)
#define DEV_T1_CHARGER_MASK	(DEV_DEDICATED_CHG | DEV_USB_CHG | DEV_CAR_KIT)

/* Device Type 2 */
#define DEV_AV			(1 << 6)
#define DEV_TTY			(1 << 5)
#define DEV_PPD			(1 << 4)
#define DEV_JIG_UART_OFF	(1 << 3)
#define DEV_JIG_UART_ON		(1 << 2)
#define DEV_JIG_USB_OFF		(1 << 1)
#define DEV_JIG_USB_ON		(1 << 0)

#define DEV_T2_USB_MASK		(DEV_JIG_USB_OFF | DEV_JIG_USB_ON)
#define DEV_T2_UART_MASK	DEV_JIG_UART_OFF
#define DEV_T2_JIG_MASK		(DEV_JIG_USB_OFF | DEV_JIG_USB_ON | \
				DEV_JIG_UART_OFF)

/*
 * Manual Switch
 * D- [7:5] / D+ [4:2]
 * 000: Open all / 001: USB / 010: AUDIO / 011: UART / 100: V_AUDIO
 */
#define SW_VAUDIO		((4 << 5) | (4 << 2))
#define SW_UART			((3 << 5) | (3 << 2))
#define SW_AUDIO		((2 << 5) | (2 << 2))
#define SW_DHOST		((1 << 5) | (1 << 2))
#define SW_AUTO			((0 << 5) | (0 << 2))
#define SW_USB_OPEN		(1 << 0)
#define SW_ALL_OPEN		(0)

/* Interrupt 1 */
#define INT_DETACH		(1 << 1)
#define INT_ATTACH		(1 << 0)

/* Interrupt Mask 2 */
#define INT_MASK_ADC_CHANGE     (1 << 2)

/*FSA9485 MANSW1*/
#define VAUDIO_9485            0x93
#define AUDIO_9485             0x4B
#define DHOST_9485             0x27

#define VERSION_FSA9480                0
#define VERSION_FSA9485                1


struct fsa9480_usbsw {
	struct i2c_client		*client;
	struct fsa9480_platform_data	*pdata;
	int				dev1;
	int				dev2;
	int				mansw;

	struct input_dev	*input;
	int			previous_key;

// for fsa9485 is not responding during mhl switching [
	int                     check_watchdog;
	struct delayed_work fsa_watchdog;
// ]

// for dock switching problem
	int                            cardock_attached;

	struct delayed_work	init_work;
	struct mutex		mutex;
};

enum {
	DOCK_KEY_NONE			= 0,
	DOCK_KEY_VOL_UP_PRESSED,
	DOCK_KEY_VOL_UP_RELEASED,
	DOCK_KEY_VOL_DOWN_PRESSED,
	DOCK_KEY_VOL_DOWN_RELEASED,
	DOCK_KEY_PREV_PRESSED,
	DOCK_KEY_PREV_RELEASED,
	DOCK_KEY_PLAY_PAUSE_PRESSED,
	DOCK_KEY_PLAY_PAUSE_RELEASED,
	DOCK_KEY_NEXT_PRESSED,
	DOCK_KEY_NEXT_RELEASED,
};

enum {
	ADC_GND			= 0x00,
	ADC_MHL			= 0x01,
	ADC_DOCK_PREV_KEY	= 0x04,
	ADC_DOCK_NEXT_KEY	= 0x07,
	ADC_DOCK_VOL_DN		= 0x0a,
	ADC_DOCK_VOL_UP		= 0x0b,
	ADC_DOCK_PLAY_PAUSE_KEY = 0x0d,
	ADC_CEA936ATYPE1_CHG	= 0x17,
	ADC_JIG_USB_OFF		= 0x18,
	ADC_JIG_USB_ON		= 0x19,
	ADC_DESKDOCK		= 0x1a,
	ADC_CEA936ATYPE2_CHG	= 0x1b,
	ADC_JIG_UART_OFF	= 0x1c,
	ADC_JIG_UART_ON		= 0x1d,
	ADC_CARDOCK		= 0x1d,
	ADC_OPEN		= 0x1f
};

static struct fsa9480_usbsw *local_usbsw;

#ifdef CONFIG_FTM_SLEEP
extern unsigned char ftm_sleep;
extern unsigned char ftm_sleep_exit;
#endif
static void fsa9480_reg_init(struct fsa9480_usbsw *usbsw);

static bool usedeskdock = false;
extern unsigned int get_hw_rev(void);
static int HWversion=0;
static int gv_intr2=0;
static int isDeskdockconnected=0;
static int Dockconnected = 0;
static int initial_check=0;
#ifdef CONFIG_VIDEO_MHL_V2
#define MHL_DEVICE		2
#endif
#include<linux/power_supply.h>
#define POWER_SUPPLY_TYPE_MHL POWER_SUPPLY_TYPE_USB
#define POWER_SUPPLY_TYPE_NONE 0
#define USE_DESK_DOCK    0
#define USB_HDMI_CABLE  1
#define USE_VEHICLE_DOCK	2

extern bool mhl_vbus; //3355
static bool mhl_onoff = false;
#define GPIO_MHL_SEL PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(16))
extern bool is_hdmi_ready(void);

#ifdef CONFIG_VIDEO_MHL_V1
#define MHL_INIT_COND (SII9234_i2c_status && is_hdmi_ready())
bool gv_mhl_sw_state = 0;
static struct workqueue_struct *fsa_mhl_workqueue=NULL;
static struct work_struct mhl_start_work;
#endif

static DECLARE_WAIT_QUEUE_HEAD(fsa9480_MhlWaitEvent);

extern int SII9234_i2c_status;
extern u8 mhl_cable_status;

extern void sii9234_cfg_power(bool on);	
extern bool SiI9234_init(void);	
extern int get_sec_det_jack_state(void);

void DisableFSA9480Interrupts(void)
{
	struct i2c_client *client = local_usbsw->client;
	int value,ret;

	value = i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL);
	value |= 0x01;

	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

} 

void EnableFSA9480Interrupts(void)
{
	struct i2c_client *client = local_usbsw->client;
	int value,ret;

	i2c_smbus_read_byte_data(client, FSA9480_REG_INT1);
	i2c_smbus_read_byte_data(client, FSA9480_REG_INT1);

	value = i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL);
	value &= 0xFE;

	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	
} 

extern unsigned int sec_get_lpm_mode(void);
void FSA9480_CheckAndHookAudioDock(int value, int onoff)
{
	struct i2c_client *client = local_usbsw->client;
	struct fsa9480_platform_data *pdata = local_usbsw->pdata;
	int ret = 0;
	// unsigned int ctrl = CON_MASK;

	if (onoff)
		local_usbsw->check_watchdog=1;

	if (value == USE_DESK_DOCK)
	{
		if (onoff)
		{
			//skip controlling the sii9234 on lpm mode
			if(!sec_get_lpm_mode())
			{
#if defined(CONFIG_VIDEO_MHL_V1)
				sii9234_cfg_power(0);	//Turn Off power to SiI9234
#elif defined(CONFIG_VIDEO_MHL_V2)
				mhl_onoff_ex(0);		//will call hw_onoff(0)
#endif
				gpio_set_value_cansleep(GPIO_MHL_SEL, 0);
			}

			ret = i2c_smbus_read_byte_data(client,FSA9480_REG_CTRL);

			dev_info(&client->dev, "FSA9480_CheckAndHookAudioDock On ctrl reg: 0x%x\n", ret);

			if (pdata->deskdock_cb)
				pdata->deskdock_cb(FSA9480_ATTACHED);

#if defined(CONFIG_USA_MODEL_SGH_I717)
			if (!get_sec_det_jack_state()) {
#endif
				if (HWversion ==VERSION_FSA9480)
				{
					ret = i2c_smbus_write_byte_data(client,FSA9480_REG_MANSW1,SW_AUDIO);
				}
				else
				{
					ret = i2c_smbus_write_byte_data(client,FSA9480_REG_MANSW1,AUDIO_9485);
				}
			
				if (ret < 0)
					dev_err(&client->dev,"%s: err %d\n", __func__, ret);
#if defined(CONFIG_USA_MODEL_SGH_I717)
			}
			else
				printk("%s: Earjack exist..\n", __func__);
#endif

			ret = i2c_smbus_read_byte_data(client,FSA9480_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,FSA9480_REG_CTRL, ret & ~CON_MANUAL_SW & ~CON_RAW_DATA);
			if (ret < 0)
				dev_err(&client->dev,"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_read_byte_data(client,FSA9480_REG_INT2_MASK);
			if (ret < 0)
				dev_err(&client->dev,"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client, FSA9480_REG_INT2_MASK, ret & ~INT_MASK_ADC_CHANGE);
			if (ret < 0)
				dev_err(&client->dev,"%s: err %d\n", __func__, ret);

			usedeskdock= true;
			isDeskdockconnected=1;
			Dockconnected = 1;
		}
		else
		{
			dev_info(&client->dev, "FSA9480_CheckAndHookAudioDock  Off ctrl reg: 0x%x\n", ret);
			if (pdata->deskdock_cb)
				pdata->deskdock_cb(FSA9480_DETACHED);

			ret = i2c_smbus_read_byte_data(client,FSA9480_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,FSA9480_REG_CTRL, ret | CON_MANUAL_SW | CON_RAW_DATA);
			if (ret < 0)
				dev_err(&client->dev,"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_read_byte_data(client,FSA9480_REG_INT2_MASK);
			if (ret < 0)
				dev_err(&client->dev,"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client, FSA9480_REG_INT2_MASK, ret | INT_MASK_ADC_CHANGE);
			if (ret < 0)
				dev_err(&client->dev,"%s: err %d\n", __func__, ret);

			usedeskdock= false;
			isDeskdockconnected=0;
			Dockconnected = 0;
		}

		EnableFSA9480Interrupts();
	}
	else if (value == USB_HDMI_CABLE)
	{
		if (onoff)
		{	
			if (pdata->mhl_cb)
				pdata->mhl_cb(FSA9480_ATTACHED);
		}
		else
		{
			if (pdata->mhl_cb)
				pdata->mhl_cb(FSA9480_DETACHED);

			if (local_usbsw!=NULL)
				fsa9480_reg_init(local_usbsw);
		}
		usedeskdock= false;	
		isDeskdockconnected=0;		
		Dockconnected = 0;
	} else if (value == USE_VEHICLE_DOCK) {
		if (onoff) {

			ret = i2c_smbus_read_byte_data(client,FSA9480_REG_CTRL);

			dev_info(&client->dev, "FSA9480_CheckAndHookAudioDock On ctrl reg: 0x%x\n", ret);


#if defined(CONFIG_USA_MODEL_SGH_I717)
			if (!get_sec_det_jack_state()) {
#endif
				if (HWversion ==VERSION_FSA9480)
				{
					ret = i2c_smbus_write_byte_data(client,FSA9480_REG_MANSW1,SW_AUDIO);
				}
				else
				{
					ret = i2c_smbus_write_byte_data(client,FSA9480_REG_MANSW1,AUDIO_9485);
				}
			
				if (ret < 0)
					dev_err(&client->dev,"%s: err %d\n", __func__, ret);
#if defined(CONFIG_USA_MODEL_SGH_I717)
			}
			else
				printk("%s: Earjack exist..\n", __func__);
#endif

			ret = i2c_smbus_read_byte_data(client,FSA9480_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,FSA9480_REG_CTRL, ret & ~CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,"%s: err %d\n", __func__, ret);

			Dockconnected = 1;
		}
		else
		{
			dev_info(&client->dev, "FSA9480_CheckAndHookAudioDock  Off ctrl reg: 0x%x\n", ret);

			ret = i2c_smbus_read_byte_data(client,FSA9480_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,FSA9480_REG_CTRL, ret | CON_MANUAL_SW | CON_RAW_DATA);
			if (ret < 0)
				dev_err(&client->dev,"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_read_byte_data(client,FSA9480_REG_INT2_MASK);
			if (ret < 0)
				dev_err(&client->dev,"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client, FSA9480_REG_INT2_MASK, ret | INT_MASK_ADC_CHANGE);
			if (ret < 0)
				dev_err(&client->dev,"%s: err %d\n", __func__, ret);

			Dockconnected = 0;
		}
		
	}
	
}

#ifdef CONFIG_VIDEO_MHL_V1
void mhl_onff_handler(struct work_struct *work) //Rajucm
{	
	// struct i2c_client *client = local_usbsw->client;
	printk("praveen: mhl on off handler\n");

	//Rajucm: phone Power on with MHL Cable,avoid crash
	while((!SII9234_i2c_status))
	{	//4Control enters into while loop only when fsa9480 detects MHL-cable @ phone bootup
		//printk(KERN_ERR"[FSA9480]## mhl cable detected during boot ## \n");
		//i2c_smbus_write_byte_data(client, 0x02, (0x01|i2c_smbus_read_byte_data(client, 0x02)));	//DisableFSA9480Interrupts
		if(!sec_get_lpm_mode())
		{
			printk(KERN_ERR "[FSA9480]## %s ## \n", SII9234_i2c_status? "Ready to start MHL":"Let's Sleep untill MHL condition comes true");
			wait_event_interruptible_timeout(fsa9480_MhlWaitEvent, SII9234_i2c_status, msecs_to_jiffies(5*1000)); //5sec:
		}

		//skip controlling the sii9234 on lpm mode
		if(sec_get_lpm_mode())
		{
			//ignore turn on mhl because of only charging mode in power off state!
			printk(KERN_DEBUG "lpm_mode booting, so there is no to turn on mhl:mhl_onoff[%d]\n",mhl_onoff);
			//only desk_dock without mhl cable!
			DisableFSA9480Interrupts();
			FSA9480_CheckAndHookAudioDock(USE_DESK_DOCK,mhl_onoff);
			return;					
		}
	}
	if(mhl_onoff)
	{
		sii9234_cfg_power(1);	//Turn On power to SiI9234
		printk("Praveen:The value of mhl_cable_status is %d\n", mhl_cable_status);
		if(mhl_cable_status == 0x08 || mhl_cable_status == 0x00 || mhl_cable_status == 0x03)
		{
			SiI9234_init();
			local_usbsw->check_watchdog=0;
			schedule_delayed_work(&local_usbsw->fsa_watchdog, msecs_to_jiffies(3000));
			DisableFSA9480Interrupts();
			gpio_set_value_cansleep(GPIO_MHL_SEL, mhl_onoff);
		}
	}
	else
	{
		sii9234_cfg_power(0);	//Turn Off power to SiI9234
		DisableFSA9480Interrupts();
		gpio_set_value_cansleep(GPIO_MHL_SEL, mhl_onoff);
		mhl_cable_status = 0x08;	//Once mhl powered off cable status cannot be 0x07;
	}

// this code only for deskdock [
	if (!mhl_onoff)
	{
		if (usedeskdock)
		{
			FSA9480_CheckAndHookAudioDock(USE_DESK_DOCK, 0);
		}
		else
		{
			FSA9480_CheckAndHookAudioDock(USB_HDMI_CABLE,0);
			EnableFSA9480Interrupts();
		}
	}
// ]	
}

void FSA9480_MhlSwitchSel(bool sw) //Rajucm
{
	struct power_supply *mhl_power_supply = power_supply_get_by_name("battery");
	union power_supply_propval value;
	if(sw)
	{
		if (HWversion==VERSION_FSA9480)
		{
			if (gv_intr2&0x1) //cts: fix for 3355
			{
				mhl_vbus = true;
				value.intval = POWER_SUPPLY_TYPE_MHL;
				gv_intr2=0;

				//Rajucm: mhl charching implimentation,
				if (mhl_vbus && (mhl_power_supply!=NULL) 
					&& (mhl_power_supply->set_property(mhl_power_supply, POWER_SUPPLY_PROP_ONLINE, &value)))
					printk(KERN_DEBUG "Fail to turn on MHL Charging\n");
			}
			else
				mhl_vbus = false;
		}
		
		mhl_onoff = true;
		queue_work(fsa_mhl_workqueue, &mhl_start_work);
		
	}
	else
	{
		if (HWversion==VERSION_FSA9480)
		{	
			value.intval = POWER_SUPPLY_TYPE_NONE;
			if (mhl_vbus && (mhl_power_supply!=NULL)
				&& (mhl_power_supply->set_property(mhl_power_supply, POWER_SUPPLY_PROP_ONLINE, &value)))
				printk(KERN_DEBUG "Fail to turn off MHL Charging\n");
			mhl_vbus = false;
		}
		else
			mhl_vbus = false;
		
		mhl_onoff = false;
		queue_work(fsa_mhl_workqueue, &mhl_start_work);
	}
}
EXPORT_SYMBOL(FSA9480_MhlSwitchSel);

void FSA9480_MhlTvOff(void)
{
	struct i2c_client *client =  local_usbsw->client;
	int intr1=0;
	int value;

	printk(KERN_DEBUG "%s:  interrupt1= %d\n", __func__, intr1);
	
	value=i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, 0x01 | i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL));
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);
	//printk(KERN_ERR "%s: started######\n", __func__);
	intr1 = i2c_smbus_read_word_data(client, FSA9480_REG_INT1);
	gpio_set_value_cansleep(GPIO_MHL_SEL, 0);

	do {
		msleep(10);
		intr1 = i2c_smbus_read_byte_data(client, FSA9480_REG_INT1);
	}while(!intr1);

	mhl_cable_status =0x08;//MHL_TV_OFF_CABLE_CONNECT;
	value=i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, 0xFE & i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL));
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);
	//printk(KERN_ERR "%s: End######\n",__func__);
	printk(KERN_DEBUG "%s:  interrupt1= %d\n", __func__, intr1);
}
EXPORT_SYMBOL(FSA9480_MhlTvOff);
#endif  //CONFIG_VIDEO_MHL_V1

static ssize_t fsa9480_show_control(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct fsa9480_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int value;

	value = i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	return sprintf(buf, "CONTROL: %02x\n", value);
}

static ssize_t fsa9480_show_device_type(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct fsa9480_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int value;

	value = i2c_smbus_read_byte_data(client, FSA9480_REG_DEV_T1);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	return sprintf(buf, "DEVICE_TYPE: %02x\n", value);
}

static ssize_t fsa9480_show_manualsw(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct fsa9480_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int value;

	value = i2c_smbus_read_byte_data(client, FSA9480_REG_MANSW1);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	if (value == SW_VAUDIO)
		return sprintf(buf, "VAUDIO\n");
	else if (value == SW_UART)
		return sprintf(buf, "UART\n");
	else if (value == SW_AUDIO)
		return sprintf(buf, "AUDIO\n");
	else if (value == SW_DHOST)
		return sprintf(buf, "DHOST\n");
	else if (value == SW_AUTO)
		return sprintf(buf, "AUTO\n");
	else
		return sprintf(buf, "%x", value);
}

static ssize_t fsa9480_set_manualsw(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct fsa9480_usbsw *usbsw = dev_get_drvdata(dev);
	struct i2c_client *client = usbsw->client;
	int value;
	unsigned int path = 0;
	int ret;

	value = i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);

	if ((value & ~CON_MANUAL_SW) !=
			(CON_SWITCH_OPEN | CON_RAW_DATA | CON_WAIT))
		return 0;

	if (!strncmp(buf, "VAUDIO", 6)) {
		path = SW_VAUDIO;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "UART", 4)) {
		path = SW_UART;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "AUDIO", 5)) {
		path = SW_AUDIO;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "DHOST", 5)) {
		path = SW_DHOST;
		value &= ~CON_MANUAL_SW;
	} else if (!strncmp(buf, "AUTO", 4)) {
		path = SW_AUTO;
		value |= CON_MANUAL_SW;
	} else {
		dev_err(dev, "Wrong command\n");
		return 0;
	}

	usbsw->mansw = path;

	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_MANSW1, path);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return count;
}

static DEVICE_ATTR(control, S_IRUGO, fsa9480_show_control, NULL);
static DEVICE_ATTR(device_type, S_IRUGO, fsa9480_show_device_type, NULL);
static DEVICE_ATTR(switch, S_IRUGO | S_IWUSR,
		fsa9480_show_manualsw, fsa9480_set_manualsw);

static struct attribute *fsa9480_attributes[] = {
	&dev_attr_control.attr,
	&dev_attr_device_type.attr,
	&dev_attr_switch.attr,
	NULL
};

static const struct attribute_group fsa9480_group = {
	.attrs = fsa9480_attributes,
};

void fsa9480_otg_set_autosw_pba(void)
{
	unsigned int data = 0;
	int ret;
	struct i2c_client *client = local_usbsw->client;
	dev_info(&client->dev, "%s\n", __func__);
	i2c_smbus_write_byte_data(client,
						FSA9480_REG_CTRL, 0x1E);
}
EXPORT_SYMBOL(fsa9480_otg_set_autosw_pba);

void fsa9480_otg_detach()
{
	// unsigned int value;
	unsigned int data = 0;
	int ret;
	struct i2c_client *client = local_usbsw->client;

	dev_info(&client->dev, "%s\n", __func__);
	data=0x00;
	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_MANSW2, data);
	if (ret < 0)
		dev_info(&client->dev, "%s: err %d\n", __func__, ret);
	data =SW_ALL_OPEN;
	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_MANSW1, data);
	if (ret < 0)
		dev_info(&client->dev, "%s: err %d\n", __func__, ret);

	data=0x1A;
	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, data);
	if (ret < 0)
		dev_info(&client->dev, "%s: err %d\n", __func__, ret);
}

EXPORT_SYMBOL(fsa9480_otg_detach);


void fsa9480_audiopath_control(int enable)
{
	struct i2c_client *client = local_usbsw->client;
	dev_info(&client->dev, "%s(%d)\n", __func__, enable);
#if defined(CONFIG_USA_MODEL_SGH_I717)
	if(enable) {
		if(Dockconnected && !get_sec_det_jack_state())
			fsa9480_manual_switching(SWITCH_PORT_AUDIO); /* dock audio path On */
		else
			dev_info(&client->dev, "%s: does not On dock audio (dock=%d, earjack=%d)\n", __func__, Dockconnected, get_sec_det_jack_state());
	} else {
		if(!Dockconnected || (Dockconnected && get_sec_det_jack_state()))
			fsa9480_manual_switching(SWITCH_PORT_USB); /* dock audio path Off */
		else
			dev_info(&client->dev, "%s: does not Off dock audio (dock=%d, earjack=%d)\n", __func__, Dockconnected, get_sec_det_jack_state());
	}
#else
	if(enable) {
		if(isDeskdockconnected && !get_sec_det_jack_state())
			fsa9480_manual_switching(SWITCH_PORT_AUDIO); /* dock audio path On */
		else
			dev_info(&client->dev, "%s: does not On dock audio (dock=%d, earjack=%d)\n", __func__, isDeskdockconnected, get_sec_det_jack_state());
	} else {
		if(!isDeskdockconnected || (isDeskdockconnected && get_sec_det_jack_state()))
			fsa9480_manual_switching(SWITCH_PORT_USB); /* dock audio path Off */
		else
			dev_info(&client->dev, "%s: does not Off dock audio (dock=%d, earjack=%d)\n", __func__, isDeskdockconnected, get_sec_det_jack_state());
	}
#endif
}


void fsa9480_manual_switching(int path)
{
	struct i2c_client *client = local_usbsw->client;
	int value;
	unsigned int data = 0;
	int ret;

	printk("%s(%d)\n", __func__, path);
	value = i2c_smbus_read_byte_data(client, FSA9480_REG_CTRL);
	if (value < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, value);
#if defined(CONFIG_USA_MODEL_SGH_I717)
	if (Dockconnected) {
		if ((value & ~CON_MANUAL_SW & ~CON_RAW_DATA) !=
				(CON_SWITCH_OPEN |  CON_WAIT)) {
			printk("Dockconnected, value=0x%x, return!!\n", value);
			return;
			}
	} else {
		if ((value & ~CON_MANUAL_SW) !=
			(CON_SWITCH_OPEN | CON_RAW_DATA | CON_WAIT)) {
			printk("Not Dockconnected, value=0x%x, return!!\n", value);
		return;
		}
	}
#else
	if (isDeskdockconnected) {
		if ((value & ~CON_MANUAL_SW) !=
				(CON_SWITCH_OPEN |  CON_WAIT))
			return;
	} else {
		if ((value & ~CON_MANUAL_SW) !=
			(CON_SWITCH_OPEN | CON_RAW_DATA | CON_WAIT))
		return;
	}
#endif
	if (path == SWITCH_PORT_VAUDIO) {
		if (HWversion ==VERSION_FSA9480)
			data = SW_VAUDIO;
		else
			data = VAUDIO_9485;
		
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_UART) {
		data = SW_UART;
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_AUDIO) {
		if (HWversion ==VERSION_FSA9480)
			data = SW_AUDIO;
		else
			data = AUDIO_9485;
		
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_USB) {
		if (HWversion ==VERSION_FSA9480)
			data = SW_DHOST;
		else
			data = DHOST_9485;
		
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_AUTO) {
		data = SW_AUTO;
		value |= CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_USB_OPEN) {
		data = SW_USB_OPEN;		
		value &= ~CON_MANUAL_SW;
	} else if (path ==  SWITCH_PORT_ALL_OPEN) {
		data = SW_ALL_OPEN;
		value &= ~CON_MANUAL_SW;
	} else {
		printk(KERN_DEBUG "%s: wrong path (%d)\n", __func__, path);
		return;
	}

	local_usbsw->mansw = data;

	// path for FTM sleep
	if (path ==  SWITCH_PORT_ALL_OPEN) {
		ret = i2c_smbus_write_byte_data(client, FSA9480_REG_MANUAL_OVERRIDES1, 0x0a);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);

		ret = i2c_smbus_write_byte_data(client, FSA9480_REG_MANSW1, data);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);

		ret = i2c_smbus_write_byte_data(client, FSA9480_REG_MANSW2, data);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);

		ret = i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, value);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);	
	} else {
		ret = i2c_smbus_write_byte_data(client, FSA9480_REG_MANSW1, data);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);

		ret = i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, value);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	}

}
EXPORT_SYMBOL(fsa9480_manual_switching);
#ifdef CONFIG_MHL_D3_SUPPORT
int get_vbus_valid(void)
{
	struct i2c_client *client = local_usbsw->client;
	int ret;
	ret = i2c_smbus_read_byte_data(client, 0x1d);
	if (ret & 0x2) {
		return 1;
	}
	return 0;
}
EXPORT_SYMBOL(get_vbus_valid);
#endif
static void fsa9480_detect_dev(struct fsa9480_usbsw *usbsw)
{
	int device_type, ret, adc;
	unsigned char val1, val2;
	struct fsa9480_platform_data *pdata = usbsw->pdata;
	struct i2c_client *client = usbsw->client;
#if defined(CONFIG_VIDEO_MHL_V2)
	u8 mhl_ret = 0;
#endif

	device_type = i2c_smbus_read_word_data(client, FSA9480_REG_DEV_T1);
	if (device_type < 0)
	{
		dev_err(&client->dev, "%s: err %d\n", __func__, device_type);
		return;
	}
	val1 = device_type & 0xff;
	val2 = device_type >> 8;
        adc = i2c_smbus_read_byte_data(client, FSA9480_REG_ADC);
	dev_info(&client->dev, "dev1: 0x%x, dev2: 0x%x\n", val1, val2);

	/* Attached */
	if (val1 || val2) {
		/* USB */
		if (val1 & DEV_USB || val2 & DEV_T2_USB_MASK) {
			dev_info(&client->dev, "usb connect\n");
			
			if (pdata->usb_cb)
				pdata->usb_cb(FSA9480_ATTACHED);
			if (usbsw->mansw) {
				ret = i2c_smbus_write_byte_data(client,
#ifdef CONFIG_TARGET_LOCALE_US_ATT_REV01
				FSA9480_REG_MANSW1, SW_DHOST);
#else
				FSA9480_REG_MANSW1, usbsw->mansw);
#endif
				
				if (ret < 0)
					dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			}
		/* UART */
		} else if (val1 & DEV_T1_UART_MASK || val2 & DEV_T2_UART_MASK) {
		
			dev_info(&client->dev, "uart connect\n");
			if (pdata->uart_cb)
				pdata->uart_cb(FSA9480_ATTACHED);

			// for uart jig
			if (val2 & DEV_T2_UART_MASK)
				pdata->jig_cb(FSA9480_ATTACHED);
		

			if (usbsw->mansw) {
				ret = i2c_smbus_write_byte_data(client,
#ifdef CONFIG_TARGET_LOCALE_US_ATT_REV01
					FSA9480_REG_MANSW1, usbsw->mansw);
#else
					FSA9480_REG_MANSW1, SW_UART);
#endif
				if (ret < 0)
					dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
			}
		/* CHARGER */
		} else if (val1 & DEV_T1_CHARGER_MASK) {
			dev_info(&client->dev, "charger connect\n");
		
			if (pdata->charger_cb)
				pdata->charger_cb(FSA9480_ATTACHED);
		// for SAMSUNG OTG			
		} else if (val1 & DEV_USB_OTG) {
			dev_info(&client->dev, "otg connect\n");
			i2c_smbus_write_byte_data(client,
						FSA9480_REG_MANSW1, 0x27);
			i2c_smbus_write_byte_data(client,
						FSA9480_REG_MANSW2, 0x02);
			msleep(50);
			i2c_smbus_write_byte_data(client,
						FSA9480_REG_CTRL, 0x1a);
			
		/* JIG */			
		} else if (val2 & DEV_T2_JIG_MASK) {
			dev_info(&client->dev, "jig connect\n");
		
			if (pdata->jig_cb)
				pdata->jig_cb(FSA9480_ATTACHED);
		/* Desk Dock */
		} else if (val2 & DEV_AV) {
#ifdef CONFIG_MHL_D3_SUPPORT
                        if ((adc & 0x1F) == 0x1A) {
                                pr_info("FSA Deskdock Attach\n");
                                FSA9480_CheckAndHookAudioDock(USE_DESK_DOCK, 1);
#if defined(CONFIG_VIDEO_MHL_V1) || defined(CONFIG_VIDEO_MHL_V2)
                                isDeskdockconnected = 1;
				Dockconnected = 1;
#endif
                                i2c_smbus_write_byte_data(client,
                                                FSA9480_REG_RESERVED_1D, 0x08);
                        } else {
#endif
			if (HWversion==VERSION_FSA9485)	{
				if (isDeskdockconnected && usedeskdock) {
					printk(KERN_DEBUG "FSA MHL isDeskdockconnected\n");
					return;
				}
			}
#if defined(CONFIG_VIDEO_MHL_V1)
			printk(KERN_DEBUG "FSA MHL Attach mhl_cable_status = %d \n", mhl_cable_status);
			FSA9480_MhlSwitchSel(1);
#elif defined(CONFIG_VIDEO_MHL_V2)
			DisableFSA9480Interrupts();
			if (!isDeskdockconnected) {
				if(!sec_get_lpm_mode())
					mhl_ret = mhl_onoff_ex(1);
			}
#ifndef CONFIG_MHL_D3_SUPPORT
			if (mhl_ret != MHL_DEVICE) {
				FSA9480_CheckAndHookAudioDock(USE_DESK_DOCK, 1);
				isDeskdockconnected = 1;
				Dockconnected = 1;
			}
#endif
			EnableFSA9480Interrupts();
#else
			FSA9480_CheckAndHookAudioDock(USE_DESK_DOCK, 1);
			if (pdata->deskdock_cb)
				pdata->deskdock_cb(FSA9480_ATTACHED);

			if (HWversion ==VERSION_FSA9480)
				ret = i2c_smbus_write_byte_data(client,
						FSA9480_REG_MANSW1, SW_AUDIO);
			else
				ret = i2c_smbus_write_byte_data(client,
					FSA9480_REG_MANSW1, AUDIO_9485);

			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_read_byte_data(client,
					FSA9480_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,
					FSA9480_REG_CTRL, ret & ~CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
		/* Car Dock */
#endif
#ifdef CONFIG_MHL_D3_SUPPORT
		}
#endif
		} else if (val2 & DEV_JIG_UART_ON) {
			Dockconnected = 1;
			if (pdata->cardock_cb)
				pdata->cardock_cb(FSA9480_ATTACHED);
			FSA9480_CheckAndHookAudioDock(USE_VEHICLE_DOCK, 1);
/*
			if (HWversion ==VERSION_FSA9480)
				ret = i2c_smbus_write_byte_data(client,
						FSA9480_REG_MANSW1, SW_AUDIO);
			else
				ret = i2c_smbus_write_byte_data(client,
					FSA9480_REG_MANSW1, AUDIO_9485);
			
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
		
			ret = i2c_smbus_read_byte_data(client,
					FSA9480_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
		
			ret = i2c_smbus_write_byte_data(client,
				FSA9480_REG_CTRL, ret & ~CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
*/
			usbsw->cardock_attached=1;
		}
	/* Detached */
	} else {
		/* USB */
		if (usbsw->dev1 & DEV_USB ||
				usbsw->dev2 & DEV_T2_USB_MASK) {
			if (pdata->usb_cb)
				pdata->usb_cb(FSA9480_DETACHED);
		/* UART */
		} else if (usbsw->dev1 & DEV_T1_UART_MASK ||
				usbsw->dev2 & DEV_T2_UART_MASK) {
			if (pdata->uart_cb)
				pdata->uart_cb(FSA9480_DETACHED);
			
			// for uart jig
			if (usbsw->dev2 & DEV_T2_UART_MASK)
				pdata->jig_cb(FSA9480_DETACHED);

		/* CHARGER */
		} else if (usbsw->dev1 & DEV_T1_CHARGER_MASK) {
			if (pdata->charger_cb)
				pdata->charger_cb(FSA9480_DETACHED);
		// for SAMSUNG OTG			
		} else if (usbsw->dev1 & DEV_USB_OTG) {
			ret=i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, 0x1E); // switch auto
			if (ret < 0)
				dev_err(&client->dev,"%s: err %d\n", __func__, ret);
		/* JIG */
		} else if (usbsw->dev2 & DEV_T2_JIG_MASK) {
			if (pdata->jig_cb)
				pdata->jig_cb(FSA9480_DETACHED);
		/* Desk Dock */
		} else if (usbsw->dev2 & DEV_AV) {

#if defined(CONFIG_VIDEO_MHL_V1) || defined(CONFIG_VIDEO_MHL_V2)
			if (HWversion==VERSION_FSA9485)
			{
				if (!isDeskdockconnected && usedeskdock) {
					printk(KERN_DEBUG "FSA MHL isDeskdockdisconnected");
					return;
				}	
			}
#endif
#if defined(CONFIG_VIDEO_MHL_V1)
			printk(KERN_DEBUG "FSA MHL Detach\n");
			FSA9480_MhlSwitchSel(0);
#elif defined(CONFIG_VIDEO_MHL_V2)
			if (isDeskdockconnected)
				FSA9480_CheckAndHookAudioDock(USE_DESK_DOCK, 0);
			isDeskdockconnected = 0;
			Dockconnected = 0;
#else 
			FSA9480_CheckAndHookAudioDock(USE_DESK_DOCK, 0);
			if (pdata->deskdock_cb)
				pdata->deskdock_cb(FSA9480_DETACHED);

			ret = i2c_smbus_read_byte_data(client,
					FSA9480_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,
					FSA9480_REG_CTRL, ret | CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
		/* Car Dock */
#endif
		} else if (usbsw->dev2 & DEV_JIG_UART_ON) {
			Dockconnected = 0;
			if (pdata->cardock_cb)
				pdata->cardock_cb(FSA9480_DETACHED);	
			FSA9480_CheckAndHookAudioDock(USE_VEHICLE_DOCK, 0);
/*
				ret = i2c_smbus_read_byte_data(client,
						FSA9480_REG_CTRL);
				if (ret < 0)
					dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);

				ret = i2c_smbus_write_byte_data(client,
						FSA9480_REG_CTRL, ret | CON_MANUAL_SW);
				if (ret < 0)
					dev_err(&client->dev,
						"%s: err %d\n", __func__, ret);
				usbsw->cardock_attached=0;
*/
		}

//We need to check dock detach Because ADC can de changed by water or other reasons.
//We check deskdock, cardock.
		if (isDeskdockconnected) {
			printk(KERN_DEBUG "FSA Deskdock abnomal Detach\n");
#if defined(CONFIG_VIDEO_MHL_V1)
			FSA9480_MhlSwitchSel(0);
#elif defined(CONFIG_VIDEO_MHL_V2)
			FSA9480_CheckAndHookAudioDock(USE_DESK_DOCK, 0);
			isDeskdockconnected = 0;
			Dockconnected = 0;
#endif
		}

		if (usbsw->cardock_attached) {
			printk(KERN_DEBUG "FSA Cardock abnomal Detach\n");
			Dockconnected = 0;
			if (pdata->deskdock_cb)
				pdata->deskdock_cb(FSA9480_DETACHED);

			ret = i2c_smbus_read_byte_data(client,
					FSA9480_REG_CTRL);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);

			ret = i2c_smbus_write_byte_data(client,
					FSA9480_REG_CTRL, ret | CON_MANUAL_SW);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: err %d\n", __func__, ret);
			usbsw->cardock_attached=0;
		}
	}
	usbsw->dev1 = val1;
	usbsw->dev2 = val2;
}

static void fsa9480_reg_init(struct fsa9480_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	unsigned int ctrl = CON_MASK;
	int ret;

	/* mask interrupts (unmask attach/detach only) */
	ret = i2c_smbus_write_word_data(client, FSA9480_REG_INT1_MASK, 0x1afc);  // adc check, AV Charging
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	/* mask all car kit interrupts */
	ret = i2c_smbus_write_word_data(client, FSA9480_REG_CK_INTMASK1, 0x07ff);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	/* ADC Detect Time: 500ms */
	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_TIMING1, 0x0);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	usbsw->mansw = i2c_smbus_read_byte_data(client, FSA9480_REG_MANSW1);
	if (usbsw->mansw < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, usbsw->mansw);

	if (usbsw->mansw)
		ctrl &= ~CON_MANUAL_SW;	/* Manual Switching Mode */

	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, ctrl);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	ret = i2c_smbus_write_byte_data(client, FSA9480_REG_RESERVED_1D, 0x04);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	ret=i2c_smbus_read_byte_data(client, FSA9480_REG_DEVID);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);	
	
	if (ret == 0x00)
	{
		HWversion=VERSION_FSA9485;
	}
	else if (ret == 0x28) 
	{
		HWversion=VERSION_FSA9480;
	}

	dev_info(&client->dev, " fsa9480_reg_init dev ID: 0x%x\n", ret);
	
	
}

// Add for fsa9485 device check (Samsung) [
void fsa9480_check_device(void)
{
	struct i2c_client *client;
	int ret = 0;

#if defined(CONFIG_VIDEO_MHL_V1) || defined(CONFIG_VIDEO_MHL_V2)
	if (mhl_onoff || local_usbsw==NULL || initial_check==0)
		return;
#endif
	client= local_usbsw->client;
	
	ret = i2c_smbus_read_byte_data(client,FSA9480_REG_CTRL);
	if (ret < 0)
		dev_err(&client->dev,"%s: err %d\n", __func__, ret);

	if (ret==0x1F)
	{
		dev_info(&client->dev, " %s  ret : %x \n", __func__,ret);
		ret = i2c_smbus_write_byte_data(client, FSA9480_REG_CTRL, 0x1E);
		if (ret < 0)
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);

		i2c_smbus_read_word_data(client, FSA9480_REG_INT1); // clear interrupt
	}
}
// ]

static int fsa9480_check_dev(struct fsa9480_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	int device_type;
	device_type = i2c_smbus_read_word_data(client, FSA9480_REG_DEV_T1);
	if (device_type < 0)
	{
		dev_err(&client->dev, "%s: err %d\n", __func__, device_type);
		return 0;
	}
	return device_type;
}

// for fsa9485 is not responding during mhl switching [ 
#ifdef CONFIG_VIDEO_MHL_V1
static void fsa9480_mhl_check(struct work_struct *work)
{
	struct i2c_client *client = local_usbsw->client;
	unsigned char val=0;
	int device_type=0;

	dev_info(&client->dev, " %s  %d, Deskdockconnected %d \n", __func__, local_usbsw->check_watchdog,isDeskdockconnected);

// We check again deskdock connection

	if (isDeskdockconnected)
	{
		device_type=fsa9480_check_dev(local_usbsw);
		val = device_type >> 8;
		if (!(val & DEV_AV))
		{
			FSA9480_MhlSwitchSel(0);
			fsa9480_reg_init(local_usbsw);
		}
	}
// there is no respond from MHL driver.
	if (local_usbsw->check_watchdog ==0)
	{
		FSA9480_MhlSwitchSel(0);
		fsa9480_reg_init(local_usbsw);
	}
}
#endif/*CONFIG_VIDEO_MHL_V1*/
// ]
static int fsa9480_handle_dock_vol_key(struct fsa9480_usbsw *info, int adc)
{
	struct input_dev *input = info->input;
	int pre_key = info->previous_key;
	unsigned int code;
	int state;

	if (adc == ADC_OPEN) {
		switch (pre_key) {
			case DOCK_KEY_VOL_UP_PRESSED:
				code = KEY_VOLUMEUP;
				state = 0;
				info->previous_key = DOCK_KEY_VOL_UP_RELEASED;
				break;
			case DOCK_KEY_VOL_DOWN_PRESSED:
				code = KEY_VOLUMEDOWN;
				state = 0;
				info->previous_key = DOCK_KEY_VOL_DOWN_RELEASED;
				break;
			case DOCK_KEY_PREV_PRESSED:
				code = KEY_PREVIOUSSONG;
				state = 0;
				info->previous_key = DOCK_KEY_PREV_RELEASED;
				break;
			case DOCK_KEY_PLAY_PAUSE_PRESSED:
				code = KEY_PLAYPAUSE;
				state = 0;
				info->previous_key = DOCK_KEY_PLAY_PAUSE_RELEASED;
				break;
			case DOCK_KEY_NEXT_PRESSED:
				code = KEY_NEXTSONG;
				state = 0;
				info->previous_key = DOCK_KEY_NEXT_RELEASED;
				break;
			default:
				return 0;
		}
		input_event(input, EV_KEY, code, state);
		input_sync(input);
		return 0;
	}

	if (pre_key == DOCK_KEY_NONE) {
		if (adc != ADC_DOCK_VOL_UP && adc != ADC_DOCK_VOL_DN)
			return 0;
	}

	switch (adc) {
		case ADC_DOCK_VOL_UP:
			code = KEY_VOLUMEUP;
			state = 1;
			info->previous_key = DOCK_KEY_VOL_UP_PRESSED;
			break;
		case ADC_DOCK_VOL_DN:
			code = KEY_VOLUMEDOWN;
			state = 1;
			info->previous_key = DOCK_KEY_VOL_DOWN_PRESSED;
			break;
		case ADC_DOCK_PREV_KEY-1 ... ADC_DOCK_PREV_KEY+1:
			code = KEY_PREVIOUSSONG;
			state = 1;
			info->previous_key = DOCK_KEY_PREV_PRESSED;
			break;
		case ADC_DOCK_PLAY_PAUSE_KEY-1 ... ADC_DOCK_PLAY_PAUSE_KEY+1:
			code = KEY_PLAYPAUSE;
			state = 1;
			info->previous_key = DOCK_KEY_PLAY_PAUSE_PRESSED;
			break;
		case ADC_DOCK_NEXT_KEY-1 ... ADC_DOCK_NEXT_KEY+1:
			code = KEY_NEXTSONG;
			state = 1;
			info->previous_key = DOCK_KEY_NEXT_PRESSED;
			break;
		case ADC_DESKDOCK:
			if (pre_key == DOCK_KEY_VOL_UP_PRESSED) {
				code = KEY_VOLUMEUP;
				state = 0;
				info->previous_key = DOCK_KEY_VOL_UP_RELEASED;
			} else if (pre_key == DOCK_KEY_VOL_DOWN_PRESSED) {
				code = KEY_VOLUMEDOWN;
				state = 0;
				info->previous_key = DOCK_KEY_VOL_DOWN_RELEASED;
			} else if (pre_key == DOCK_KEY_PREV_PRESSED) {
				code = KEY_PREVIOUSSONG;
				state = 0;
				info->previous_key = DOCK_KEY_PREV_RELEASED;
			} else if (pre_key == DOCK_KEY_PLAY_PAUSE_PRESSED) {
				code = KEY_PLAYPAUSE;
				state = 0;
				info->previous_key = DOCK_KEY_PLAY_PAUSE_RELEASED;
			} else if (pre_key == DOCK_KEY_NEXT_PRESSED) {
				code = KEY_NEXTSONG;
				state = 0;
				info->previous_key = DOCK_KEY_NEXT_RELEASED;
			} else {
				return 0;
			}
			break;
		default:
			break;
			return 0;
	}

	input_event(input, EV_KEY, code, state);
	input_sync(input);
	
	return 1;
}

static int fsa9480_Check_AVDock(struct fsa9480_usbsw *usbsw, int device_type)
{
	// struct fsa9480_platform_data *pdata = usbsw->pdata;
	struct i2c_client *client = usbsw->client;
	int adc_val=0;
	int int2_val=0;
#if defined(CONFIG_VIDEO_MHL_V1)
	unsigned char val;
#endif

	dev_info(&client->dev, "%s intr: 0x%x\n",__func__, gv_intr2);

	if(HWversion==VERSION_FSA9480) {
		int2_val = 0x4;
	}
	else if(HWversion==VERSION_FSA9485) {
		int2_val = 0x400;
	}

	if (gv_intr2 & int2_val) // for adc change
	{
		adc_val=i2c_smbus_read_word_data(client, FSA9480_REG_ADC);
		fsa9480_handle_dock_vol_key(usbsw, adc_val);

		dev_info(&client->dev, "intr: 0x%x, adc_val: %d\n", gv_intr2, adc_val);
		return IRQ_NONE;
	}
#if defined(CONFIG_VIDEO_MHL_V1)
	else if (gv_intr2 & 0x1)   // for av change (desk dock, hdmi)
	{
		if (HWversion==VERSION_FSA9485)
			mhl_vbus = true;
		dev_info(&client->dev, "%s enter Av charing \n",__func__);
		return IRQ_HANDLED;
	}
		
	// for check Av charging during boot up
	// when Mhl is recognized, usb interrupt occur
	val = device_type & 0xff;
	if (mhl_onoff && (val &DEV_USB))
	{
		dev_info(&client->dev, "%s enter Av charing \n",__func__);
		return IRQ_NONE;
	}
#endif
	return IRQ_HANDLED;
	
	//
}

static irqreturn_t fsa9480_irq_thread(int irq, void *data)
{
	struct fsa9480_usbsw *usbsw = data;
	struct i2c_client *client = usbsw->client;
	int intr;
	int device_type;
	////////////////////////////////////////
	// dy96.choi                                                         //
	// FSA9480 : Read interrupt -> Read Device         //
	// FSA9485 : Read Device -> Read interrupt         //   
	////////////////////////////////////////

	if (HWversion==VERSION_FSA9485)
		device_type=fsa9480_check_dev(usbsw);

	/* read and clear interrupt status bits */
	intr = i2c_smbus_read_word_data(client, FSA9480_REG_INT1);
	if (intr < 0) {
		mdelay(100);
		dev_err(&client->dev, "%s: err %d\n", __func__, intr);
		intr = i2c_smbus_read_word_data(client, FSA9480_REG_INT1);
		if (intr < 0)
			dev_err(&client->dev, "%s: err at read %d\n", __func__, intr);
		fsa9480_reg_init(usbsw);
		return IRQ_HANDLED;
	} else if (intr == 0) {
		/* interrupt was fired, but no status bits were set,
		so device was reset. In this case, the registers were
		reset to defaults so they need to be reinitialised. */
		fsa9480_reg_init(usbsw);
	}

#if defined(CONFIG_VIDEO_MHL_V1) || defined(CONFIG_VIDEO_MHL_V2)
	if(HWversion==VERSION_FSA9480)
		gv_intr2 = intr >> 8;
	else if(HWversion==VERSION_FSA9485)
		gv_intr2 = intr;
#endif	

	if (HWversion==VERSION_FSA9485)
		if (IRQ_NONE==fsa9480_Check_AVDock(usbsw,device_type)) return 0;

	/* device detection */
	mutex_lock(&usbsw->mutex);
	fsa9480_detect_dev(usbsw);
	mutex_unlock(&usbsw->mutex);

	initial_check=1;
	return IRQ_HANDLED;
	
}

static int fsa9480_irq_init(struct fsa9480_usbsw *usbsw)
{
	struct i2c_client *client = usbsw->client;
	int ret;

	if (client->irq) {
		ret = request_threaded_irq(client->irq, NULL,
			fsa9480_irq_thread, IRQF_TRIGGER_FALLING,
			"fsa9480 micro USB", usbsw);
		if (ret) {
			dev_err(&client->dev, "failed to reqeust IRQ\n");
			return ret;
		}

		ret = enable_irq_wake(client->irq);
		if (ret < 0)
			dev_err(&client->dev,
				"failed to enable wakeup src %d\n", ret);
	}

	return 0;
}

static void fsa9480_init_detect(struct work_struct *work)
{
	struct fsa9480_usbsw *usbsw = container_of(work,
			struct fsa9480_usbsw, init_work.work);
	int ret = 0;

	dev_info(&usbsw->client->dev, "%s \n", __func__);
	
	mutex_lock(&usbsw->mutex);
	fsa9480_detect_dev(usbsw);
	mutex_unlock(&usbsw->mutex);
	
	ret = fsa9480_irq_init(usbsw);
	if (ret)
		dev_info(&usbsw->client->dev, "failed to enable  irq init %s\n", __func__);
}

static int __devinit fsa9480_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct fsa9480_usbsw *usbsw;
	int ret = 0;
	struct input_dev *input;
	
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	input = input_allocate_device();
	usbsw = kzalloc(sizeof(struct fsa9480_usbsw), GFP_KERNEL);
	if (!usbsw ||!input ) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		kfree(usbsw);
		return -ENOMEM;
	}

// for dock key [
	usbsw->input = input;
	input->name = client->name;
	input->phys = "deskdock-key/input0";
	input->dev.parent = &client->dev;
	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0001;

	/* Enable auto repeat feature of Linux input subsystem */
	__set_bit(EV_REP, input->evbit);

	input_set_capability(input, EV_KEY, KEY_VOLUMEUP);
	input_set_capability(input, EV_KEY, KEY_VOLUMEDOWN);
	input_set_capability(input, EV_KEY, KEY_PLAYPAUSE);
	input_set_capability(input, EV_KEY, KEY_PREVIOUSSONG);
	input_set_capability(input, EV_KEY, KEY_NEXTSONG);

	ret = input_register_device(input);
	if (ret) {
		dev_err(&client->dev, "input_register_device %s: err %d\n", __func__, ret);
	}
// ]

	usbsw->client = client;
	usbsw->pdata = client->dev.platform_data;
	if (!usbsw->pdata)
		goto fail1;

	i2c_set_clientdata(client, usbsw);

	mutex_init(&usbsw->mutex);

	local_usbsw = usbsw;  // temp

	if (usbsw->pdata->cfg_gpio)
		usbsw->pdata->cfg_gpio();

	fsa9480_reg_init(usbsw);

	ret = sysfs_create_group(&client->dev.kobj, &fsa9480_group);
	if (ret) {
		dev_err(&client->dev,
				"failed to create fsa9480 attribute group\n");
		goto fail2;
	}

	if (usbsw->pdata->reset_cb)
		usbsw->pdata->reset_cb();

	// set fsa9480 init flag.
	if (usbsw->pdata->set_init_flag)
		usbsw->pdata->set_init_flag();

	#ifdef CONFIG_VIDEO_MHL_V1
	//Rajucm: Isolate MHL work from fsa to unblock normal fsa-operations
	INIT_WORK(&mhl_start_work, mhl_onff_handler);
	fsa_mhl_workqueue = create_singlethread_workqueue("fsa_mhl_workqueue");
	#endif
	
	/* initial cable detection */
	INIT_DELAYED_WORK(&usbsw->init_work, fsa9480_init_detect);
#ifdef CONFIG_VIDEO_MHL_V1
	INIT_DELAYED_WORK(&usbsw->fsa_watchdog, fsa9480_mhl_check);
#endif
	
#if defined (CONFIG_KOR_MODEL_SHV_E160S) || defined (CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	if(get_hw_rev()<=0x03)
		schedule_delayed_work(&usbsw->init_work, msecs_to_jiffies(32000));
	else
		schedule_delayed_work(&usbsw->init_work, msecs_to_jiffies(6000));
#else
	schedule_delayed_work(&usbsw->init_work, msecs_to_jiffies(6000));

#endif

	return 0;

fail2:
	if (client->irq)
		free_irq(client->irq, usbsw);
fail1:
	mutex_destroy(&usbsw->mutex);
	i2c_set_clientdata(client, NULL);
	kfree(usbsw);
	return ret;
}

static int __devexit fsa9480_remove(struct i2c_client *client)
{
	struct fsa9480_usbsw *usbsw = i2c_get_clientdata(client);

	cancel_delayed_work(&usbsw->init_work);
	if (client->irq) {
		disable_irq_wake(client->irq);
		free_irq(client->irq, usbsw);
	}
	mutex_destroy(&usbsw->mutex);
	i2c_set_clientdata(client, NULL);

	sysfs_remove_group(&client->dev.kobj, &fsa9480_group);
	kfree(usbsw);
	return 0;
}

#ifdef CONFIG_PM
static int fsa9480_resume(struct i2c_client *client)
{
	struct fsa9480_usbsw *usbsw = i2c_get_clientdata(client);

#ifdef CONFIG_FTM_SLEEP
	if(ftm_sleep && ftm_sleep_exit)
	{
		ftm_sleep=0;
		ftm_sleep_exit=0;
		fsa9480_manual_switching(SWITCH_PORT_AUTO);
		printk(KERN_DEBUG "wakeup from ftm sleep, port reconnect..\n");
	}
#endif

// add for fsa9480_irq_thread i2c error during wakeup
	if (HWversion==VERSION_FSA9485) 
		fsa9480_check_dev(usbsw);

	i2c_smbus_read_byte_data(client, FSA9480_REG_INT1);
	
	/* device detection */
	mutex_lock(&usbsw->mutex);
	fsa9480_detect_dev(usbsw);
	mutex_unlock(&usbsw->mutex);

	return 0;
}

#else

#define fsa9480_suspend NULL
#define fsa9480_resume NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id fsa9480_id[] = {
	{"fsa9480", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, fsa9480_id);

static struct i2c_driver fsa9480_i2c_driver = {
	.driver = {
		.name = "fsa9480",
	},
	.probe = fsa9480_probe,
	.remove = __devexit_p(fsa9480_remove),
	.resume = fsa9480_resume,
	.id_table = fsa9480_id,
};

static int __init fsa9480_init(void)
{
	return i2c_add_driver(&fsa9480_i2c_driver);
}
module_init(fsa9480_init);

static void __exit fsa9480_exit(void)
{
	i2c_del_driver(&fsa9480_i2c_driver);
}
module_exit(fsa9480_exit);

MODULE_AUTHOR("Minkyu Kang <mk7.kang@samsung.com>");
MODULE_DESCRIPTION("FSA9480 USB Switch driver");
MODULE_LICENSE("GPL");
