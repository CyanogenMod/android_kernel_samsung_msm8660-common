/*
 * wm8994_LTE.c  --  WM8994 ALSA Soc Audio driver related LTE
 *
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include <linux/mfd/pmic8058.h>
#include <linux/gpio.h>

#include <linux/mfd/wm8994/core.h>
#include <linux/mfd/wm8994/registers.h>
#include <linux/mfd/wm8994/pdata.h>
#include <linux/mfd/wm8994/gpio.h>




//------------------------------------------------
//		Debug Feature
//------------------------------------------------
#define SUBJECT "wm8994_LTE.c"

//------------------------------------------------
// Definitions of tunning volumes for wm8994
//------------------------------------------------

//------------------------------------------------
// Common

// DAC
#define TUNING_DAC1L_VOL		0xC0		// 610h
#define TUNING_DAC1R_VOL		0xC0		// 611h
#define TUNING_DAC2L_VOL		0xC0		// 612h
#define TUNING_DAC2R_VOL		0xC0		// 613h

// Speaker
#define TUNING_SPKMIXL_ATTEN		0x0		// 22h
#define TUNING_SPKMIXR_ATTEN		0x0		// 23h

// Headset
#define TUNING_EAR_OUTMIX5_VOL		0x0		// 31h
#define TUNING_EAR_OUTMIX6_VOL		0x0		// 32h

//------------------------------------------------
// Normal
// Speaker
#define TUNING_MP3_SPKL_VOL		    0x3E		// 26h
#define TUNING_MP3_CLASSD_VOL		0x5		// 25h

// Headset
#define TUNING_MP3_OUTPUTL_VOL		0x2F		// 1Ch
#define TUNING_MP3_OUTPUTR_VOL   	0x2F		// 1Dh
#define TUNING_MP3_OPGAL_VOL		0x39		// 20h
#define TUNING_MP3_OPGAR_VOL		0x39		// 21h

// Dual
#define TUNING_MP3_DUAL_OUTPUTL_VOL		0x1E		// 1Ch
#define TUNING_MP3_DUAL_OUTPUTR_VOL		0x1E		// 1Dh

// Extra_Dock_speaker
#define TUNING_MP3_EXTRA_DOCK_SPK_OPGAL_VOL		0x39		// 20h
#define TUNING_MP3_EXTRA_DOCK_SPK_OPGAR_VOL		0x39		// 21h
#define TUNING_EXTRA_DOCK_SPK_OUTMIX5_VOL		0x1		// 31h
#define TUNING_EXTRA_DOCK_SPK_OUTMIX6_VOL		0x1		// 32h
#define TUNING_MP3_EXTRA_DOCK_SPK_VOL	0x0		//1Eh

//------------------------------------------------
// Ringtone
// Speaker
#define TUNING_RING_SPKL_VOL		0x3E		// 26h
#define TUNING_RING_CLASSD_VOL		0x5		// 25h

// Headset
#define TUNING_RING_OUTPUTL_VOL		0x34		// 1Ch
#define TUNING_RING_OUTPUTR_VOL		0x34		// 1Dh
#define TUNING_RING_OPGAL_VOL		0x39		// 20h
#define TUNING_RING_OPGAR_VOL		0x39		// 21h

// Dual
#define TUNING_RING_DUAL_OUTPUTL_VOL		0x1E		// 1Ch
#define TUNING_RING_DUAL_OUTPUTR_VOL		0x1E		// 1Dh

#define WM8994_REG_CACHE_SIZE  0x621

#define PMIC_GPIO_AMP_EN		PM8058_GPIO(20)  	/* PMIC GPIO Number 20 */
#define PMIC_GPIO_SPK_SEL		PM8058_GPIO(16)  	/* PMIC GPIO Number 20 */

struct pm_gpio amp_en = {
	.direction		= PM_GPIO_DIR_OUT,
	.output_value	= 1,
	.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
	.pull			= PM_GPIO_PULL_NO,
	.out_strength	= PM_GPIO_STRENGTH_HIGH,
	.vin_sel		= 2,
	.function		= PM_GPIO_FUNC_NORMAL,
	.inv_int_pol	= 0,
};
struct pm_gpio spk_sel = {
	.direction		= PM_GPIO_DIR_OUT,
	.output_value	= 1,
	.output_buffer	= PM_GPIO_OUT_BUF_CMOS,
	.pull			= PM_GPIO_PULL_NO,
	.out_strength	= PM_GPIO_STRENGTH_HIGH,
	.vin_sel		= 2,
	.function		= PM_GPIO_FUNC_NORMAL,
	.inv_int_pol	= 0,
};

struct wm8994 *wm8994_amp;
int CompensationCAL = 0;
int wm_check_inited =0;

static struct dentry *debugfs_root;
static struct dentry *debugfs_file;
static struct dentry *debugfs_reg;
//------------------------------------------------
static int wm8994_read(struct wm8994 *wm8994, unsigned short reg,
		       int bytes, void *dest)
{
	int ret, i;
	u16 *buf = dest;

	BUG_ON(bytes % 2);
	BUG_ON(bytes <= 0);

	ret = wm8994->read_dev(wm8994, reg, bytes, dest);
	if (ret < 0)
		return ret;

	for (i = 0; i < bytes / 2; i++) {
		buf[i] = be16_to_cpu(buf[i]);

		dev_vdbg(wm8994->dev, "Read %04x from R%d(0x%x)\n",
			 buf[i], reg + i, reg + i);
	}

	return 0;
}

/**
 * wm8994_reg_read: Read a single WM8994 register.
 *
 * @wm8994: Device to read from.
 * @reg: Register to read.
 */
int wm8994_reg_read(struct wm8994 *wm8994, unsigned short reg)
{
	unsigned short val;
	int ret;

	mutex_lock(&wm8994->io_lock);

	ret = wm8994_read(wm8994, reg, 2, &val);

	mutex_unlock(&wm8994->io_lock);

	if (ret < 0)
		return ret;
	else
		return val;
}
EXPORT_SYMBOL_GPL(wm8994_reg_read);

/**
 * wm8994_bulk_read: Read multiple WM8994 registers
 *
 * @wm8994: Device to read from
 * @reg: First register
 * @count: Number of registers
 * @buf: Buffer to fill.
 */
int wm8994_bulk_read(struct wm8994 *wm8994, unsigned short reg,
		     int count, u16 *buf)
{
	int ret;

	mutex_lock(&wm8994->io_lock);

	ret = wm8994_read(wm8994, reg, count * 2, buf);

	mutex_unlock(&wm8994->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(wm8994_bulk_read);

static int wm8994_write(struct wm8994 *wm8994, unsigned short reg,
			int bytes, void *src)
{
	u16 *buf = src;
	int i;

	BUG_ON(bytes % 2);
	BUG_ON(bytes <= 0);

	for (i = 0; i < bytes / 2; i++) {
		dev_vdbg(wm8994->dev, "Write %04x to R%d(0x%x)\n",
			 buf[i], reg + i, reg + i);

		buf[i] = cpu_to_be16(buf[i]);
	}

	return wm8994->write_dev(wm8994, reg, bytes, src);
}

/**
 * wm8994_reg_write: Write a single WM8994 register.
 *
 * @wm8994: Device to write to.
 * @reg: Register to write to.
 * @val: Value to write.
 */
int wm8994_reg_write(struct wm8994 *wm8994, unsigned short reg,
		     unsigned short val)
{
	int ret;

	mutex_lock(&wm8994->io_lock);

	ret = wm8994_write(wm8994, reg, 2, &val);
	mutex_unlock(&wm8994->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(wm8994_reg_write);


//------------------------------------------------
// Definition external function prototype.
//------------------------------------------------
#if defined(CONFIG_WM8994_KOR_E140S) || defined(CONFIG_WM8994_KOR_E140L)
/*
 * ONLY FOR P5_LTE_KOR SKT/KT/LGU
 */
int audio_power(int en)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	if(en == 1) {
		printk("%s: audio_power on for E140SKL\n",__func__);
		CompensationCAL = 0;
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_AMP_EN), 1);
		msleep(50);
		wm8994_reg_write(amp, 0x00, 0x0000);
		msleep(10);
		// wolfson HQ recommended 50ms
		wm8994_reg_write(amp, 0x102, 0x0003);
		wm8994_reg_write(amp,  0x55, 0x03E0);
		wm8994_reg_write(amp,  0x56, 0x0003);
		wm8994_reg_write(amp, 0x102, 0x0000);

		wm8994_reg_write(amp, 0x39, 0x006C);
		wm8994_reg_write(amp, 0x01, 0x0003);
		wm8994_reg_write(amp, 0x15, 0x0040);
		msleep(50);

		wm8994_reg_write(amp, 0x220, 0x0002);
		wm8994_reg_write(amp, 0x221, 0x0700);
		wm8994_reg_write(amp, 0x224, 0x0CC0);
		wm8994_reg_write(amp, 0x200, 0x0011); // End of clocking

		wm8994_reg_write(amp, 0x620, 0x0000);
		wm8994_reg_write(amp, 0x302, 0x7000);
		wm8994_reg_write(amp, 0x301, 0x4001);

		wm8994_reg_write(amp, 0x601, 0x0001);
		wm8994_reg_write(amp, 0x602, 0x0001);

		wm8994_reg_write(amp, 0x610, 0x00C0);
		wm8994_reg_write(amp, 0x611, 0x01C0);
		wm8994_reg_write(amp, 0x606, 0x0002);
		wm8994_reg_write(amp, 0x607, 0x0002);
		wm8994_reg_write(amp, 0x208, 0x000A);
		wm8994_reg_write(amp, 0x200, 0x0011); // End of clocking
		msleep(10);
		wm8994_reg_write(amp, 0x02, 0x63A0);
		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);

		wm8994_reg_write(amp, 0x28, 0x0011);
		wm8994_reg_write(amp, 0x29, 0x0020);
		wm8994_reg_write(amp, 0x2A, 0x0020);

		wm8994_reg_write(amp, 0x36, 0x00C0);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);
		wm8994_reg_write(amp, 0x24, 0x0011);
		wm8994_reg_write(amp, 0x25, 0x016D);

		wm8994_reg_write(amp, 0x410, 0x3800);

		wm8994_reg_write(amp, 0x444, 0x0277);
		wm8994_reg_write(amp, 0x443, 0x0000);
		wm8994_reg_write(amp, 0x442, 0x0400);

		wm8994_reg_write(amp, 0x441, 0x0845);
		wm8994_reg_write(amp, 0x440, 0x01BB);

		wm8994_reg_write(amp, 0x400, 0x1D2);
		wm8994_reg_write(amp, 0x401, 0x1D2);

		wm8994_reg_write(amp, 0x15, 0x0000);
		msleep(100);

		wm8994_reg_write(amp, 0x2D, 0x0000);
		wm8994_reg_write(amp, 0x2E, 0x0000);
	} else {
		printk("%s: audio_power off for E140SKL\n",__func__);

		wm8994_reg_write(amp, 0x1C, 0x0131);
		wm8994_reg_write(amp, 0x1D, 0x0131);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x0, 0x8994);
		msleep(50);
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_AMP_EN), 0);
	}

	return 0;
}


void wm8994_set_headset(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	if(wm_check_inited == 0)
		return;

	if(onoff == 1) {
		printk("%s: headset amp on for E140SKL\n",__func__);
		wm8994_reg_write(amp, 0x15, 0x0040);
		wm8994_reg_write(amp, 0x02, 0x6350);
		msleep(50);
		wm8994_reg_write(amp, 0x19, 0x018B);
		wm8994_reg_write(amp, 0x1B, 0x018B);

		wm8994_reg_write(amp, 0x20, 0x0139);
		wm8994_reg_write(amp, 0x21, 0x0139);

		wm8994_reg_write(amp, 0x25, 0x0140);
		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x28, 0x0011);
		wm8994_reg_write(amp, 0x29, 0x0020);
		wm8994_reg_write(amp, 0x2A, 0x0020);

		wm8994_reg_write(amp, 0x34, 0x0000);
		wm8994_reg_write(amp, 0x35, 0x0000);
		wm8994_reg_write(amp, 0x36, 0x0000);

		wm8994_reg_write(amp, 0x2D, 0x0010);
		wm8994_reg_write(amp, 0x2E, 0x0010);

		wm8994_reg_write(amp, 0x19, 0x018B);
		wm8994_reg_write(amp, 0x1B, 0x018B);

		wm8994_reg_write(amp, 0x1C, 0x0074);
		wm8994_reg_write(amp, 0x1D, 0x0174);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x410, 0x1800);

		wm8994_reg_write(amp, 0x610, 0x01C0);
		wm8994_reg_write(amp, 0x611, 0x01C0);

		wm8994_reg_write(amp, 0x15, 0x0000);
		msleep(50);
		wm8994_reg_write(amp, 0x01, 0x0303);
		if(CompensationCAL == 0){
			wm8994_reg_write(amp, 0x01, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);
			wm8994_reg_write(amp, 0x4C, 0x9f25);
			msleep(5);
			wm8994_reg_write(amp, 0x1C, 0x0174);
			wm8994_reg_write(amp, 0x1D, 0x0174);

			wm8994_reg_write(amp, 0x2D, 0x0010); // DC Offset calibration by DC_Servo calculation
			wm8994_reg_write(amp, 0x2E, 0x0010);
			wm8994_reg_write(amp, 0x03, 0x0FF0);
			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(160);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low-5)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High-5)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;

			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(15);
			CompensationCAL = 1;
		} else {
			wm8994_reg_write(amp, 0x55, 0x00E0);
			wm8994_reg_write(amp, 0x54, 0x0303);
			msleep(50);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low-5)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High-5)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;

			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(15);
		}
		wm8994_reg_write(amp, 0x60, 0x00EE);
	} else {
		printk("%s: headset amp off for E140SKL\n",__func__);
		wm8994_reg_write(amp, 0x60, 0x0022);
		wm8994_reg_write(amp, 0x2D, 0x0000);
		wm8994_reg_write(amp, 0x2E, 0x0000);

		wm8994_reg_write(amp, 0x02, 0x63A0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0011);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x36, 0x00C0);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x3, 0x0FF0);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);
	}
}

void wm8994_set_speaker(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;
	printk("wm8994_set_speaker onoff val= %d debasis ",onoff);
	if(wm_check_inited == 0)
		return;

	if(onoff==1){
		printk("%s: spk amp on for E140S/K/L\n",__func__);
		wm8994_reg_write(amp, 0x1, 0x0003);

		wm8994_reg_write(amp, 0x2D, 0x0000);
		wm8994_reg_write(amp, 0x2E, 0x0000);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x19, 0x018B);
		wm8994_reg_write(amp, 0x1A, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x018B);
		wm8994_reg_write(amp, 0x28, 0x0011);
		wm8994_reg_write(amp, 0x29, 0x0020);
		wm8994_reg_write(amp, 0x2A, 0x0020);

		wm8994_reg_write(amp, 0x36, 0x00C0);

		wm8994_reg_write(amp, 0x26, 0x0179);
		wm8994_reg_write(amp, 0x27, 0x0179);

		wm8994_reg_write(amp, 0x1C, 0x012D);
		wm8994_reg_write(amp, 0x1D, 0x012D);

		wm8994_reg_write(amp, 0x20, 0x0139);
		wm8994_reg_write(amp, 0x21, 0x0139);

		wm8994_reg_write(amp, 0x02, 0x6350);
		msleep(50);

		wm8994_reg_write(amp, 0x610, 0x01c0);
		wm8994_reg_write(amp, 0x611, 0x01c0);

		wm8994_reg_write(amp, 0x410, 0x3800);

		// Analogue out gain control
		wm8994_reg_write(amp, 0x25, 0x0176);
		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x55, 0x03E0);
			wm8994_reg_write(amp, 0x60, 0x0022);
			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);
			wm8994_reg_write(amp, 0x03, 0x0FF0);
			msleep(20);
			wm8994_reg_write(amp, 0x01, 0x3303);
		}
		wm8994_reg_write(amp, 0x01, 0x3303);
	} else {
		printk("%s: spk amp off for E140SKL\n",__func__);
		wm8994_reg_write(amp, 0x60, 0x0022);
		wm8994_reg_write(amp, 0x2D, 0x0000);
		wm8994_reg_write(amp, 0x2E, 0x0000);

		wm8994_reg_write(amp, 0x02, 0x63A0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0011);
		wm8994_reg_write(amp, 0x29, 0x0020);
		wm8994_reg_write(amp, 0x2A, 0x0020);

		wm8994_reg_write(amp, 0x36, 0x00C0);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x03, 0x0FF0);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);
	}
}

void wm8994_set_speaker_headset(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	if(wm_check_inited == 0)
		return;

	if(onoff==1){
		printk("%s: headset_spk amp on for E140SKL\n",__func__);
		wm8994_reg_write(amp, 0x1, 0x0003);

		wm8994_reg_write(amp, 0x20, 0x0139);
		wm8994_reg_write(amp, 0x21, 0x0139);

		wm8994_reg_write(amp, 0x15, 0x0040);
		msleep(50);
		wm8994_reg_write(amp, 0x2D, 0x0010);
		wm8994_reg_write(amp, 0x2E, 0x0010);
		// Analgoue input gain
		wm8994_reg_write(amp, 0x02, 0x6350);
		msleep(50);
		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);
		wm8994_reg_write(amp, 0x19, 0x018B);
		wm8994_reg_write(amp, 0x1B, 0x018B);
		wm8994_reg_write(amp, 0x28, 0x0011);
		wm8994_reg_write(amp, 0x29, 0x0020);
		wm8994_reg_write(amp, 0x2A, 0x0020);
		wm8994_reg_write(amp, 0x36, 0x00C0);
		wm8994_reg_write(amp, 0x28, 0x0011);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);
		wm8994_reg_write(amp, 0x36, 0x00C0);

		wm8994_reg_write(amp, 0x03, 0x0FF0);

		wm8994_reg_write(amp, 0x26, 0x017C);
		wm8994_reg_write(amp, 0x27, 0x017C);

		wm8994_reg_write(amp, 0x610, 0x01C0);
		wm8994_reg_write(amp, 0x611, 0x01C0);

		wm8994_reg_write(amp, 0x25, 0x0176);

		wm8994_reg_write(amp, 0x1C, 0x0160);
		wm8994_reg_write(amp, 0x1D, 0x0160);

		wm8994_reg_write(amp, 0x15, 0x0000);
		msleep(50);
		if(CompensationCAL == 0){
			wm8994_reg_write(amp, 0x01, 0x3303);
			wm8994_reg_write(amp, 0x60, 0x0022);
			wm8994_reg_write(amp, 0x4C, 0x9f25);
			msleep(5);
			wm8994_reg_write(amp, 0x1C, 0x01F4);
			wm8994_reg_write(amp, 0x1D, 0x01F4);

			wm8994_reg_write(amp, 0x2D, 0x0010); // DC Offset calibration by DC_Servo calculation
			wm8994_reg_write(amp, 0x2E, 0x0010);
			wm8994_reg_write(amp, 0x3, 0x0030);
			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(160);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -5)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -5)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;

			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(15);
			CompensationCAL = 1;
		} else {
			wm8994_reg_write(amp, 0x55, 0x00E0);
			wm8994_reg_write(amp, 0x54, 0x0303);
			msleep(50);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -5)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -5)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;

			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(15);
		}
		wm8994_reg_write(amp, 0x01, 0x3303);
		//wm8994_reg_write(amp, 0x1, 0x3303);
		wm8994_reg_write(amp, 0x60, 0x00EE);
	} else {
		printk("%s: headset_spk amp off for E140SKL\n",__func__);
		wm8994_reg_write(amp, 0x60, 0x0022);
		wm8994_reg_write(amp, 0x2D, 0x0000);
		wm8994_reg_write(amp, 0x2E, 0x0000);

		wm8994_reg_write(amp, 0x02, 0x63F0);
		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);
		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0011);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x36, 0x00C0);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x03, 0x0FF0);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);
	}
}

void wm8994_set_cradle(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	if(wm_check_inited == 0)
		return;

	if(onoff==1){
		printk("%s: cradle amp on for E140SKL\n",__func__);

		wm8994_reg_write(amp, 0x18, 0x018B);
		wm8994_reg_write(amp, 0x19, 0x018B);
		wm8994_reg_write(amp, 0x1A, 0x018B);
		wm8994_reg_write(amp, 0x1B, 0x018B);

		wm8994_reg_write(amp, 0x24, 0x0000);

		wm8994_reg_write(amp, 0x28, 0x0011);
		wm8994_reg_write(amp, 0x29, 0x0020);
		wm8994_reg_write(amp, 0x2A, 0x0020);

		//wm8994_reg_write(amp, 0x2B, 0x0000);
		//wm8994_reg_write(amp, 0x2C, 0x0000);
		//wm8994_reg_write(amp, 0x2F, 0x0000);
		//wm8994_reg_write(amp, 0x30, 0x0000);

		wm8994_reg_write(amp, 0x2d, 0x0002);
		wm8994_reg_write(amp, 0x2e, 0x0002);

		wm8994_reg_write(amp, 0x36, 0x0000);

		wm8994_reg_write(amp, 0x37, 0x0000);
		wm8994_reg_write(amp, 0x38, 0x0080);
		wm8994_reg_write(amp, 0x35, 0x0031);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		// HPF setting
		wm8994_reg_write(amp, 0x410, 0x1800);

		// Analogue out gain control
		wm8994_reg_write(amp, 0x20, 0x017B);
		wm8994_reg_write(amp, 0x21, 0x017B);

		// Line AMP Enable
		wm8994_reg_write(amp, 0x1E, 0x60); // Check schematic

		wm8994_reg_write(amp, 0x02, 0x6350); // Check schematic
		wm8994_reg_write(amp, 0x03, 0x0FF0);
		wm8994_reg_write(amp, 0x01, 0x3303);

		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x1, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x3, 0x0FF0);

			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(180);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -2)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -2)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;

			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
			CompensationCAL = 1;
		}
	} else {
		printk("%s: cradle amp off for E140SKL\n",__func__);

		wm8994_reg_write(amp, 0x02, 0x63F0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x1E, 0x0066);
		wm8994_reg_write(amp, 0x28, 0x0011);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x36, 0x00C0);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x24, 0x0011);

		wm8994_reg_write(amp, 0x3, 0x0FF0);

		wm8994_reg_write(amp, 0x2D, 0x0010);
		wm8994_reg_write(amp, 0x2E, 0x0010);

		wm8994_reg_write(amp, 0x4c, 0x9f25);
		wm8994_reg_write(amp, 0x1, 0x3303);
	}
}

void wm8994_set_spk_cradle(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	if(wm_check_inited == 0)
	return;

	if(onoff==1){
		printk("%s: cradle_spk amp on for E140SKL\n",__func__);

		// Analgoue input gain, IN2P L/R
		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x018B);
		wm8994_reg_write(amp, 0x1B, 0x018B);
		wm8994_reg_write(amp, 0x28, 0x0011);
		wm8994_reg_write(amp, 0x29, 0x0020);
		wm8994_reg_write(amp, 0x2A, 0x0020);

		wm8994_reg_write(amp, 0x36, 0x00C0);

		wm8994_reg_write(amp, 0x2D, 0x0002);
		wm8994_reg_write(amp, 0x2E, 0x0002);
		wm8994_reg_write(amp, 0x37, 0x0000);
		wm8994_reg_write(amp, 0x38, 0x0080);
		wm8994_reg_write(amp, 0x35, 0x0031);

		// HPF setting
		wm8994_reg_write(amp, 0x410, 0x1800);

		// Analogue out gain control
		wm8994_reg_write(amp, 0x20, 0x0139);
		wm8994_reg_write(amp, 0x21, 0x0139);

		// Line AMP Enable
		wm8994_reg_write(amp, 0x1E, 0x60); // Check schematic

		wm8994_reg_write(amp, 0x22, 0x0000); // Check schematic
		wm8994_reg_write(amp, 0x23, 0x0000); // Check schematic
		wm8994_reg_write(amp, 0x36, 0x00C0);
		// Analogue out gain control
		wm8994_reg_write(amp, 0x25, 0x0176);
		wm8994_reg_write(amp, 0x26, 0x0179);
		wm8994_reg_write(amp, 0x27, 0x0179);

		wm8994_reg_write(amp, 0x02, 0x6350);
		wm8994_reg_write(amp, 0x03, 0x0FF0);
		wm8994_reg_write(amp, 0x01, 0x3303); //

		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x01, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x01, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x03, 0x0FF0);

			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(180);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -2)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -2)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;

			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
			CompensationCAL = 1;
		}
	} else {
		printk("%s: cradle_spk amp off for E140SKL\n",__func__);

		wm8994_reg_write(amp, 0x02, 0x63F0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0011);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x1E, 0x0066);

		wm8994_reg_write(amp, 0x36, 0x00C0);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x2D, 0x0010);
		wm8994_reg_write(amp, 0x2E, 0x0010);

		wm8994_reg_write(amp, 0x4C, 0x9f25);
		wm8994_reg_write(amp, 0x01, 0x3303);
	}
}

#elif defined(CONFIG_MACH_P8_LTE)
/*
 FOR ONLY P8_LTE_SKT WM8994
*/

int audio_power(int en)
{
	struct wm8994 *amp = wm8994_amp;

	if(en == 1) {
		printk("[P8_LTE] %s: audio_power on\n",__func__);
		CompensationCAL = 0;
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_AMP_EN), 1);
		msleep(50);

		wm8994_reg_write(amp, 0x39, 0x006C);
		wm8994_reg_write(amp, 0x01, 0x0003);
		msleep(50);

		// internal osc enable
		wm8994_reg_write(amp, 0x220, 0x0002);
		msleep(200);

		wm8994_reg_write(amp, 0x224, 0x0EC0);
		wm8994_reg_write(amp, 0x221, 0x0600);

		wm8994_reg_write(amp, 0x200, 0x0010);

		wm8994_reg_write(amp, 0x620, 0x0000);
		wm8994_reg_write(amp, 0x302, 0x7000);
		wm8994_reg_write(amp, 0x301, 0x4001);

		wm8994_reg_write(amp, 0x601, 0x0001);
		wm8994_reg_write(amp, 0x602, 0x0001);

		wm8994_reg_write(amp, 0x610, 0x00C0);
		wm8994_reg_write(amp, 0x611, 0x01C0);
		wm8994_reg_write(amp, 0x606, 0x0002);
		wm8994_reg_write(amp, 0x607, 0x0002);
		wm8994_reg_write(amp, 0x208, 0x000A);
		wm8994_reg_write(amp, 0x200, 0x0011); // End of clocking
		msleep(10);

		//wm8994_reg_write(amp, 0x01, 0x0303);
		//wm8994_reg_write(amp, 0x02, 0x63F0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);

		wm8994_reg_write(amp, 0x28, 0x0055);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x2D, 0x0001);
		wm8994_reg_write(amp, 0x2E, 0x0001);

		wm8994_reg_write(amp, 0x36, 0x0003);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);
		wm8994_reg_write(amp, 0x24, 0x0011);
		wm8994_reg_write(amp, 0x25, 0x016D);

		wm8994_reg_write(amp, 0x410, 0x3800);

		wm8994_reg_write(amp, 0x444, 0x02B9);
		wm8994_reg_write(amp, 0x443, 0x01EF);
		wm8994_reg_write(amp, 0x442, 0x0418);

		wm8994_reg_write(amp, 0x441, 0x0845);
		wm8994_reg_write(amp, 0x440, 0x01BB);

		wm8994_reg_write(amp, 0x400, 0x1D8);
		wm8994_reg_write(amp, 0x401, 0x1D8);
	} else {
		printk("[P8_LTE] %s: audio_power off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);

		wm8994_reg_write(amp, 0x1C, 0x0131);
		wm8994_reg_write(amp, 0x1D, 0x0131);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x0, 0x8994);
		msleep(50);
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_AMP_EN), 0);
	}

	return 0;
}

void wm8994_set_headset(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	if(wm_check_inited == 0)
		return;

	if(onoff == 1) {
		printk("[P8_LTE] %s: headset amp on\n",__func__);

		wm8994_reg_write(amp, 0x02, 0x63F0);
		msleep(50);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);

		wm8994_reg_write(amp, 0x1C, 0x0174);
		wm8994_reg_write(amp, 0x1D, 0x0174);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x410, 0x1800);

		wm8994_reg_write(amp, 0x610, 0x01c0);
		wm8994_reg_write(amp, 0x611, 0x01c0);
		
		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x1, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x5, 0x3303);
			wm8994_reg_write(amp, 0x3, 0x0FF0);
			wm8994_reg_write(amp, 0x4, 0x0303);

			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(160);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -2)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -2)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;
			
			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
			CompensationCAL = 1;
		}	 
		wm8994_reg_write(amp, 0x420, 0x0000);
	} else {
		printk("[P8_LTE] %s: headset amp off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);
		wm8994_reg_write(amp, 0x02, 0x63F0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0055);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x36, 0x0003);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x3, 0x0FF0);


		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x1C, 0x01B4);
		wm8994_reg_write(amp, 0x1D, 0x01B4);
	}
}


void wm8994_set_normal_headset(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	if(wm_check_inited == 0)
		return;

	if(onoff == 1) {
		printk("[P8_LTE] %s: headset amp on\n",__func__);
		wm8994_reg_write(amp, 0x02, 0x63F0);
		msleep(50);

		wm8994_reg_write(amp, 0x2D, 0x0010);  //kks_111006 0x0001->0x0010 kks_110922 remain //kks_110916_2 0x0010->0x0001
		wm8994_reg_write(amp, 0x2E, 0x0010);  //kks_111006 0x0001->0x0010 kks_110922 remain //kks_110916_2 0x0010->0x0001

		wm8994_reg_write(amp, 0x19, 0x0107);  //kks_110922 remain //kks_110916_2 0x010F->0x0107
		wm8994_reg_write(amp, 0x1B, 0x0107);  //kks_110922 remain //kks_110916_2 0x010F->0x0107

		wm8994_reg_write(amp, 0x1C, 0x0177); //kks_110922 remain //kks_110916_2 0x0176->0x0177 //kks_110916_3 0x0074->0x0176
		wm8994_reg_write(amp, 0x1D, 0x0177); //kks_110922 remain //kks_110916_2 0x0176->0x0177 //kks_110916_3 0x0174->0x0176

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x410, 0x1800); //kks_110922 remain 

		wm8994_reg_write(amp, 0x444, 0x0277); //kks_110922 remain 
		wm8994_reg_write(amp, 0x443, 0x0000);
		wm8994_reg_write(amp, 0x442, 0x0800);   //kks_110922 remain //kks_110916_3 0x0000->0x0800
		wm8994_reg_write(amp, 0x441, 0x0845); //kks_110922 remain 
		wm8994_reg_write(amp, 0x440, 0x01BB);

		wm8994_reg_write(amp, 0x400, 0x1D5); //kks_110916_2 0x1D3->0x1D5 //kks_110916_3 0x1C3->0x1D3
		wm8994_reg_write(amp, 0x401, 0x1D5); //kks_110916_2 0x1D3->0x1D5 //kks_110916_3 0x1C3->0x1D3

		wm8994_reg_write(amp, 0x480, 0x6318); //kks_110922 remain //kks_110916_2

		wm8994_reg_write(amp, 0x610, 0x01c0);
		wm8994_reg_write(amp, 0x611, 0x01c0);
		
		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x1, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x5, 0x3303);
			wm8994_reg_write(amp, 0x3, 0x0FF0);
			wm8994_reg_write(amp, 0x4, 0x0303);

			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(160);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -2)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -2)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;
			
			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
			CompensationCAL = 1;
		}	 
		wm8994_reg_write(amp, 0x420, 0x0000);
	} else {
		printk("[P8_LTE] %s: headset amp off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);
		wm8994_reg_write(amp, 0x02, 0x63F0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0055);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x36, 0x0003);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x3, 0x0FF0);


		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x1C, 0x01B4);
		wm8994_reg_write(amp, 0x1D, 0x01B4);
	}
}

void wm8994_set_speaker(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;
	if(wm_check_inited == 0)
		return;

	if(onoff==1){
		printk("[P8_LTE] %s: spk amp on\n",__func__);

		wm8994_reg_write(amp, 0x26, 0x017F);
		wm8994_reg_write(amp, 0x27, 0x017F);

		wm8994_reg_write(amp, 0x1C, 0x0131);
		wm8994_reg_write(amp, 0x1D, 0x0131);

		wm8994_reg_write(amp, 0x20, 0x0131);
		wm8994_reg_write(amp, 0x21, 0x0131);
		
		wm8994_reg_write(amp, 0x02, 0x63F0);
		msleep(50);

		wm8994_reg_write(amp, 0x610, 0x01c0);
		wm8994_reg_write(amp, 0x611, 0x01c0);

		wm8994_reg_write(amp, 0x420, 0x0000);

		wm8994_reg_write(amp, 0x410, 0x3800);

		wm8994_reg_write(amp, 0x444, 0x02DA);
		wm8994_reg_write(amp, 0x443, 0x01EF);
		wm8994_reg_write(amp, 0x442, 0x0418);

		wm8994_reg_write(amp, 0x400, 0x1D3);
		wm8994_reg_write(amp, 0x401, 0x1D3);

		wm8994_reg_write(amp, 0x481, 0x5A80);
		wm8994_reg_write(amp, 0x480, 0x6319);

		// Analogue out gain control
		wm8994_reg_write(amp, 0x25, 0x017F);

		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x1, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x5, 0x3303);
			wm8994_reg_write(amp, 0x3, 0x0FF0);
			wm8994_reg_write(amp, 0x4, 0x0303);

			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
		}	 
	} else {
		printk("[P8_LTE] %s: spk amp off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x1C, 0x0131);
		wm8994_reg_write(amp, 0x1D, 0x0131);
	}
}

void wm8994_set_normal_speaker(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	if(wm_check_inited == 0)
		return;

	if(onoff==1){
		printk("[P8_LTE] %s: spk amp on\n",__func__);


		wm8994_reg_write(amp, 0x2D, 0x0001);
		wm8994_reg_write(amp, 0x2E, 0x0001);


		wm8994_reg_write(amp, 0x26, 0x017D); //kks_110926 kks_110925 0x007F->0x017D
		wm8994_reg_write(amp, 0x27, 0x017D); //kks_110926 kks_110925 0x017F->0x017D

		wm8994_reg_write(amp, 0x1C, 0x0131);
		wm8994_reg_write(amp, 0x1D, 0x0131);

		wm8994_reg_write(amp, 0x20, 0x0131);
		wm8994_reg_write(amp, 0x21, 0x0131);
		
		wm8994_reg_write(amp, 0x02, 0x63F0);
		msleep(50);

		wm8994_reg_write(amp, 0x610, 0x01c0);
		wm8994_reg_write(amp, 0x611, 0x01c0);

		wm8994_reg_write(amp, 0x420, 0x0000);
		wm8994_reg_write(amp, 0x410, 0x3800);

		// DRC Control tuning
		wm8994_reg_write(amp, 0x444, 0x02DA);
		wm8994_reg_write(amp, 0x441, 0x0845); //kks_110922 remain //kks_110916

		wm8994_reg_write(amp, 0x443, 0x0000);
		wm8994_reg_write(amp, 0x442, 0x0400); //kks_110916_2 0x0000->0x0400

		wm8994_reg_write(amp, 0x400, 0x1C8);    //kks_110922 0x0C8->0x1C8 //kks_110917 0x0C6->0x0C8
		wm8994_reg_write(amp, 0x401, 0x1C8);    //kks_110922 0x0C8->0x1C8  //kks_110917 0x1C6->0x0C8

		wm8994_reg_write(amp, 0x481, 0x5280); //kks_110925 0x5A80->0x5280 
		wm8994_reg_write(amp, 0x480, 0x6319); //kks_110926 kks_110925 0x6318->0x6319 //kks_110922 remain //kks_110916 0x6319->0x6318 EQ Off

		// Analogue out gain control
		wm8994_reg_write(amp, 0x25, 0x0164); //kks_110922 remain //kks_110917 0x0176->0x0164

		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x1, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x5, 0x3303);
			wm8994_reg_write(amp, 0x3, 0x0FF0);
			wm8994_reg_write(amp, 0x4, 0x0303);

			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(180);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -2)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -2)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;
			
			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
			CompensationCAL = 1;
		}	 
	} else {
		printk("[P8_LTE] %s: spk amp off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x1C, 0x0131);
		wm8994_reg_write(amp, 0x1D, 0x0131);
	}
}

void wm8994_set_speaker_headset(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	if(wm_check_inited == 0)
		return;

	if(onoff==1){
		printk("[P8_LTE] %s: headset_spk amp on\n",__func__);

		wm8994_reg_write(amp, 0x02, 0x63F0);
		msleep(50);
		// Analgoue input gain
		wm8994_reg_write(amp, 0x18, 0x000b);
		wm8994_reg_write(amp, 0x1A, 0x010b);
		wm8994_reg_write(amp, 0x28, 0x0011);
		wm8994_reg_write(amp, 0x29, 0x0020);
		wm8994_reg_write(amp, 0x2A, 0x0020);
		wm8994_reg_write(amp, 0x03, 0x03F0);
		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);
		wm8994_reg_write(amp, 0x36, 0x0003);

		wm8994_reg_write(amp, 0x26, 0x0179); //kks_110928 0x0175->0x0179
		wm8994_reg_write(amp, 0x27, 0x0179); //kks_110928 0x0175->0x0179		

		wm8994_reg_write(amp, 0x610, 0x01c0);
		wm8994_reg_write(amp, 0x611, 0x01c0);

		wm8994_reg_write(amp, 0x420, 0x0000);

		wm8994_reg_write(amp, 0x443, 0x01EF);
		wm8994_reg_write(amp, 0x442, 0x0418);

		wm8994_reg_write(amp, 0x400, 0x1D8);
		wm8994_reg_write(amp, 0x401, 0x1D8);

		wm8994_reg_write(amp, 0x25, 0x017F);

		wm8994_reg_write(amp, 0x1C, 0x01E0);
		wm8994_reg_write(amp, 0x1D, 0x01E0);
		
		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x1, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x5, 0x3303);
			wm8994_reg_write(amp, 0x3, 0x0FF0);
			wm8994_reg_write(amp, 0x4, 0x0303);

			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(180);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -2)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -2)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;
			
			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
			CompensationCAL = 1;
		}	 
	} else {
		printk("[P8_LTE] %s: headset_spk amp off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);

		wm8994_reg_write(amp, 0x02, 0x63F0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0055);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x36, 0x0003);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x3, 0x0FF0);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x1C, 0x01A0);
		wm8994_reg_write(amp, 0x1D, 0x01A0);
	}
}

void wm8994_set_cradle(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;
	
	if(wm_check_inited == 0)
		return;

	if(onoff==1){
		printk("[P8_LTE] %s: cradle amp on\n",__func__);

		wm8994_reg_write(amp, 0x19, 0x0005);
		wm8994_reg_write(amp, 0x1b, 0x0105);

		wm8994_reg_write(amp, 0x28, 0x00cc);
		wm8994_reg_write(amp, 0x29, 0x0100);
		wm8994_reg_write(amp, 0x2A, 0x0100);

		wm8994_reg_write(amp, 0x2d, 0x0001);
		wm8994_reg_write(amp, 0x2e, 0x0001);
		wm8994_reg_write(amp, 0x37, 0x0000);

		wm8994_reg_write(amp, 0x38, 0x0080);
		wm8994_reg_write(amp, 0x35, 0x0031);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		// HPF setting
		wm8994_reg_write(amp, 0x410, 0x1800);

		// DRC Control tuning
		wm8994_reg_write(amp, 0x444, 0x0217); //kks_110928 0x0256->0x0217

		wm8994_reg_write(amp, 0x443, 0x0000); //kks_110916_4 0x01EF->0x0000
		wm8994_reg_write(amp, 0x442, 0x0800); //kks_110916_4 0x0418->0x0800

		wm8994_reg_write(amp, 0x441, 0x0845);
		wm8994_reg_write(amp, 0x440, 0x01BB);

		wm8994_reg_write(amp, 0x480, 0x6318);

		wm8994_reg_write(amp, 0x400, 0x1E1); //kks_110922 0x1E3->0x1E1 //kks_110916_4 0x0C0->0x1E3
		wm8994_reg_write(amp, 0x401, 0x1E1); //kks_110922 0x1E3->0x1E1 //kks_110916_4 0x1C0->0x1E3
		wm8994_reg_write(amp, 0x480, 0x6318);


		// Analogue out gain control
		wm8994_reg_write(amp, 0x20, 0x0177); //kks_110922 0x0176->0x0177 //kks_110916_4 0x007E->0x0176
		wm8994_reg_write(amp, 0x21, 0x0177); //kks_110922 0x0176->0x0177 //kks_110916_4 0x017E->0x0176

		// Line AMP Enable
		wm8994_reg_write(amp, 0x1E, 0x60); // Check schematic


		wm8994_reg_write(amp, 0x02, 0x63A0); // Check schematic
		wm8994_reg_write(amp, 0x03, 0x0CF0);
		wm8994_reg_write(amp, 0x01, 0x3303); 
		
		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x1, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x5, 0x3303);
			wm8994_reg_write(amp, 0x3, 0x0FF0);
			wm8994_reg_write(amp, 0x4, 0x0303);

			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(180);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -2)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -2)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;
			
			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
			CompensationCAL = 1;
		}	 
		// Unmute
		wm8994_reg_write(amp, 0x420, 0x0000);
	} else {
		printk("[P8_LTE] %s: cradle amp off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);

		wm8994_reg_write(amp, 0x02, 0x63F0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0055);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x36, 0x0003);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x3, 0x0FF0);

		wm8994_reg_write(amp, 0x2D, 0x0010);
		wm8994_reg_write(amp, 0x2E, 0x0010);
		
		wm8994_reg_write(amp, 0x4c, 0x9f25);
		wm8994_reg_write(amp, 0x1, 0x3303);
	}
}

void wm8994_set_spk_cradle(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;
	
	if(wm_check_inited == 0)
	return;

	if(onoff==1){
		printk("[P8_LTE] %s: cradle_spk amp on\n",__func__);

		// Analgoue input gain, IN2P L/R
		wm8994_reg_write(amp, 0x19, 0x000b);
		wm8994_reg_write(amp, 0x1b, 0x010b);

		wm8994_reg_write(amp, 0x28, 0x00cc);
		wm8994_reg_write(amp, 0x29, 0x0100);
		wm8994_reg_write(amp, 0x2A, 0x0100);
		
		wm8994_reg_write(amp, 0x2d, 0x0001);
		wm8994_reg_write(amp, 0x2e, 0x0001);
		wm8994_reg_write(amp, 0x37, 0x0000);
		wm8994_reg_write(amp, 0x38, 0x0080);
		wm8994_reg_write(amp, 0x35, 0x0031);

		// HPF setting
		wm8994_reg_write(amp, 0x410, 0x1800);

		// DRC Control tuning
		wm8994_reg_write(amp, 0x444, 0x02DA);
		wm8994_reg_write(amp, 0x443, 0x0000);
		wm8994_reg_write(amp, 0x442, 0x0418);

		wm8994_reg_write(amp, 0x441, 0x0845);
		wm8994_reg_write(amp, 0x440, 0x01BB);
		wm8994_reg_write(amp, 0x480, 0x6318);

		wm8994_reg_write(amp, 0x400, 0x1D8);
		wm8994_reg_write(amp, 0x401, 0x1D8);

		// Analogue out gain control
		wm8994_reg_write(amp, 0x20, 0x0079);
		wm8994_reg_write(amp, 0x21, 0x0179);

		// Line AMP Enable
		wm8994_reg_write(amp, 0x1E, 0x60); // Check schematic

		wm8994_reg_write(amp, 0x22, 0x0); // Check schematic
		wm8994_reg_write(amp, 0x23, 0x0); // Check schematic
		wm8994_reg_write(amp, 0x36, 0x3);
		// Analogue out gain control
		wm8994_reg_write(amp, 0x25, 0x017F);
		wm8994_reg_write(amp, 0x26, 0x0078);
		wm8994_reg_write(amp, 0x27, 0x0178);

		wm8994_reg_write(amp, 0x02, 0x63F0); // Check schematic
		wm8994_reg_write(amp, 0x03, 0x0FF0);
		wm8994_reg_write(amp, 0x01, 0x3303); //
		
		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x1, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x5, 0x3303);
			wm8994_reg_write(amp, 0x3, 0x0FF0);
			wm8994_reg_write(amp, 0x4, 0x0303);

			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(180);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -2)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -2)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;
			
			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
			CompensationCAL = 1;
		}	 
		// Unmute
		wm8994_reg_write(amp, 0x420, 0x0000);
	} else {
		printk("[P8_LTE] %s: cradle_spk amp off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);

		wm8994_reg_write(amp, 0x02, 0x63F0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0055);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x36, 0x0003);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x2D, 0x0010);
		wm8994_reg_write(amp, 0x2E, 0x0010);

		wm8994_reg_write(amp, 0x4c, 0x9f25);
		wm8994_reg_write(amp, 0x1, 0x3303);
	}
}

#elif defined(CONFIG_MACH_P5_LTE)

int audio_power(int en)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	if(en == 1) {
		printk("%s: audio_power on\n",__func__);
		CompensationCAL = 0;
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_AMP_EN), 1);
		msleep(50);
		wm8994_reg_write(amp,0x0, 0x0);
		msleep(10);
		// wolfson HQ recommended 50ms
		wm8994_reg_write(amp,0x102, 0x0003);
		wm8994_reg_write(amp,0x55, 0x03E0);
		wm8994_reg_write(amp,0x56, 0x0003);
		wm8994_reg_write(amp,0x102, 0x0000);
		
		wm8994_reg_write(amp,0x39, 0x6C);
		wm8994_reg_write(amp,0x1, 0x3);
		wm8994_reg_write(amp,0x15, 0x40);		
		msleep(50);

		wm8994_reg_write(amp, 0x220, 0x0002);
		wm8994_reg_write(amp, 0x221, 0x0700);
		wm8994_reg_write(amp, 0x224, 0x0CC0);
		wm8994_reg_write(amp, 0x200, 0x0011); // End of clocking

		wm8994_reg_write(amp, 0x620, 0x0000);
		wm8994_reg_write(amp, 0x302, 0x7000);
		wm8994_reg_write(amp, 0x301, 0x4001);

		wm8994_reg_write(amp, 0x601, 0x0001);
		wm8994_reg_write(amp, 0x602, 0x0001);

		wm8994_reg_write(amp, 0x610, 0x00C0);
		wm8994_reg_write(amp, 0x611, 0x01C0);
		wm8994_reg_write(amp, 0x606, 0x0002);
		wm8994_reg_write(amp, 0x607, 0x0002);
		wm8994_reg_write(amp, 0x208, 0x000A);
		wm8994_reg_write(amp, 0x200, 0x0011); // End of clocking
		msleep(10);
		wm8994_reg_write(amp, 0x2, 0x63A0);
		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);

		wm8994_reg_write(amp, 0x28, 0x0044);
		wm8994_reg_write(amp, 0x29, 0x0100);
		wm8994_reg_write(amp, 0x2A, 0x0100);

		wm8994_reg_write(amp, 0x36, 0x00C0);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);
		wm8994_reg_write(amp, 0x24, 0x0011);
		wm8994_reg_write(amp, 0x25, 0x016D);

		wm8994_reg_write(amp, 0x410, 0x3800);

		wm8994_reg_write(amp, 0x444, 0x0277);
		wm8994_reg_write(amp, 0x443, 0x0000);
		wm8994_reg_write(amp, 0x442, 0x0400);

		wm8994_reg_write(amp, 0x441, 0x0845);
		wm8994_reg_write(amp, 0x440, 0x01BB);

		wm8994_reg_write(amp, 0x400, 0x1D2);
		wm8994_reg_write(amp, 0x401, 0x1D2);

		wm8994_reg_write(amp, 0x15, 0x0000);
		msleep(100);

		wm8994_reg_write(amp, 0x2D, 0x0000);
		wm8994_reg_write(amp, 0x2E, 0x0000);
	} else {
		printk("%s: audio_power off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);

		wm8994_reg_write(amp, 0x1C, 0x0131);
		wm8994_reg_write(amp, 0x1D, 0x0131);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x0, 0x8994);
		msleep(50);
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_AMP_EN), 0);
	}

	return 0;
}


void wm8994_set_headset(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	if(wm_check_inited == 0)
		return;

	if(onoff == 1) {
		printk("%s: headset amp on\n",__func__);
		wm8994_reg_write(amp, 0x15, 0x0040);
		wm8994_reg_write(amp, 0x02, 0x6000);
		msleep(50);
		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0044);
		wm8994_reg_write(amp, 0x29, 0x0100);
		wm8994_reg_write(amp, 0x2A, 0x0100);

		wm8994_reg_write(amp, 0x36, 0x00C0);
		
		wm8994_reg_write(amp, 0x2D, 0x0010);
		wm8994_reg_write(amp, 0x2E, 0x0010);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);

		wm8994_reg_write(amp, 0x1C, 0x01F4);
		wm8994_reg_write(amp, 0x1D, 0x01F4);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x410, 0x1800);

		wm8994_reg_write(amp, 0x610, 0x01c0);
		wm8994_reg_write(amp, 0x611, 0x01c0);
			 
		wm8994_reg_write(amp, 0x420, 0x0000);
		wm8994_reg_write(amp, 0x15, 0x0000);
		msleep(50);
		if(CompensationCAL == 0){
			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);
			wm8994_reg_write(amp, 0x4C, 0x9f25);
			msleep(5);
			wm8994_reg_write(amp, 0x1C, 0x01F4);
			wm8994_reg_write(amp, 0x1D, 0x01F4);

			wm8994_reg_write(amp, 0x2D, 0x0010); // DC Offset calibration by DC_Servo calculation
			wm8994_reg_write(amp, 0x2E, 0x0010);
			wm8994_reg_write(amp, 0x3, 0x0030);
			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(160);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -5)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -5)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;

			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(15);
			CompensationCAL = 1;
		}
		else{
			wm8994_reg_write(amp, 0x55, 0x00E0);
			wm8994_reg_write(amp, 0x54, 0x0303);
			msleep(50);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -5)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -5)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;

			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(15);
		}
		wm8994_reg_write(amp, 0x60, 0x00EE);
	} else {
		printk("%s: headset amp off\n",__func__);
		wm8994_reg_write(amp, 0x60, 0x0022);
		wm8994_reg_write(amp, 0x2D, 0x0000);
		wm8994_reg_write(amp, 0x2E, 0x0000);
		
		wm8994_reg_write(amp, 0x420, 0x0200);
		wm8994_reg_write(amp, 0x02, 0x63A0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0044);
		wm8994_reg_write(amp, 0x29, 0x0100);
		wm8994_reg_write(amp, 0x2A, 0x0100);

		wm8994_reg_write(amp, 0x36, 0x00C0);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x3, 0x0FF0);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);
	}
}

void wm8994_set_speaker(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;
	if(wm_check_inited == 0)
		return;

	if(onoff==1){
		printk("%s: spk amp on\n",__func__);
		wm8994_reg_write(amp, 0x1, 0x3303);
		wm8994_reg_write(amp, 0x4, 0x0303);
		wm8994_reg_write(amp, 0x5, 0x3303);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0055);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x36, 0x0003);
		
		wm8994_reg_write(amp, 0x26, 0x017F);
		wm8994_reg_write(amp, 0x27, 0x017F);

		wm8994_reg_write(amp, 0x1C, 0x0131);
		wm8994_reg_write(amp, 0x1D, 0x0131);

		wm8994_reg_write(amp, 0x20, 0x0131);
		wm8994_reg_write(amp, 0x21, 0x0131);
		
		wm8994_reg_write(amp, 0x02, 0x63F0);
		msleep(50);

		wm8994_reg_write(amp, 0x610, 0x01c0);
		wm8994_reg_write(amp, 0x611, 0x01c0);

		wm8994_reg_write(amp, 0x420, 0x0000);
		wm8994_reg_write(amp, 0x410, 0x3800);

		// DRC Control tuning
		wm8994_reg_write(amp, 0x444, 0x037F);
		wm8994_reg_write(amp, 0x443, 0x0000);
		wm8994_reg_write(amp, 0x442, 0x2400);

		wm8994_reg_write(amp, 0x400, 0x1C0);
		wm8994_reg_write(amp, 0x401, 0x1C0);

		wm8994_reg_write(amp, 0x481, 0x48C0);
		wm8994_reg_write(amp, 0x480, 0x6319);

		// Analogue out gain control
		wm8994_reg_write(amp, 0x25, 0x016D);
		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x55, 0x03E0);
			wm8994_reg_write(amp, 0x60, 0x0022);
			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);
			wm8994_reg_write(amp, 0x5, 0x3303);
			wm8994_reg_write(amp, 0x3, 0x0FF0);
			wm8994_reg_write(amp, 0x4, 0x0303);
			msleep(20);
			wm8994_reg_write(amp, 0x01, 0x3303);
		}	 
	} else {
		printk("%s: spk amp off\n",__func__);
		wm8994_reg_write(amp, 0x60, 0x0022);
		wm8994_reg_write(amp, 0x2D, 0x0000);
		wm8994_reg_write(amp, 0x2E, 0x0000);
		
		wm8994_reg_write(amp, 0x420, 0x0200);
		wm8994_reg_write(amp, 0x02, 0x63A0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0044);
		wm8994_reg_write(amp, 0x29, 0x0100);
		wm8994_reg_write(amp, 0x2A, 0x0100);

		wm8994_reg_write(amp, 0x36, 0x00C0);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x3, 0x0FF0);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);
	}
}

void wm8994_set_speaker_headset(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	if(wm_check_inited == 0)
		return;

	if(onoff==1){
		printk("%s: headset_spk amp on\n",__func__);
		wm8994_reg_write(amp, 0x1, 0x3303);
		wm8994_reg_write(amp, 0x4, 0x0303);
		wm8994_reg_write(amp, 0x5, 0x3303);

		wm8994_reg_write(amp, 0x20, 0x0131);
		wm8994_reg_write(amp, 0x21, 0x0131);
		
		wm8994_reg_write(amp, 0x15, 0x0040);
		msleep(50);
		wm8994_reg_write(amp, 0x2D, 0x0010);
		wm8994_reg_write(amp, 0x2E, 0x0010);		
		// Analgoue input gain
		wm8994_reg_write(amp, 0x02, 0x6000);
		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);
		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0055);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);
		wm8994_reg_write(amp, 0x36, 0x000C);
		
		wm8994_reg_write(amp, 0x03, 0x0FF0);

		wm8994_reg_write(amp, 0x26, 0x017C);
		wm8994_reg_write(amp, 0x27, 0x017C);

		wm8994_reg_write(amp, 0x610, 0x01c0);
		wm8994_reg_write(amp, 0x611, 0x01c0);

		wm8994_reg_write(amp, 0x420, 0x0000);

		wm8994_reg_write(amp, 0x443, 0x0000);
		wm8994_reg_write(amp, 0x442, 0x0400);
		// Analogue out gain control
		wm8994_reg_write(amp, 0x400, 0x1C0);
		wm8994_reg_write(amp, 0x401, 0x1C0);

		wm8994_reg_write(amp, 0x25, 0x017F);

		wm8994_reg_write(amp, 0x1C, 0x0167);
		wm8994_reg_write(amp, 0x1D, 0x0167);

		wm8994_reg_write(amp, 0x15, 0x0000);
		msleep(50);
		if(CompensationCAL == 0){
			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);
			wm8994_reg_write(amp, 0x4C, 0x9f25);
			msleep(5);
			wm8994_reg_write(amp, 0x1C, 0x01F4);
			wm8994_reg_write(amp, 0x1D, 0x01F4);

			wm8994_reg_write(amp, 0x2D, 0x0010); // DC Offset calibration by DC_Servo calculation
			wm8994_reg_write(amp, 0x2E, 0x0010);
			wm8994_reg_write(amp, 0x3, 0x0030);
			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(160);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -5)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -5)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;

			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(15);
			CompensationCAL = 1;
		}
		else{
			wm8994_reg_write(amp, 0x55, 0x00E0);
			wm8994_reg_write(amp, 0x54, 0x0303);
			msleep(50);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -5)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -5)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;

			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(15);
		}
//		wm8994_reg_write(amp, 0x1, 0x3303);
		wm8994_reg_write(amp, 0x60, 0x00EE);
	} else {
		printk("%s: headset_spk amp off\n",__func__);
		wm8994_reg_write(amp, 0x60, 0x0022);
		wm8994_reg_write(amp, 0x2D, 0x0000);
		wm8994_reg_write(amp, 0x2E, 0x0000);
		
		wm8994_reg_write(amp, 0x420, 0x0200);

		wm8994_reg_write(amp, 0x02, 0x63F0);
		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);
		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0044);
		wm8994_reg_write(amp, 0x29, 0x0100);
		wm8994_reg_write(amp, 0x2A, 0x0100);

		wm8994_reg_write(amp, 0x36, 0x00C0);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x3, 0x0FF0);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);
	}
}



void wm8994_set_cradle(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;
	
	if(wm_check_inited == 0)
		return;

	if(onoff==1){
		printk("%s: cradle amp on\n",__func__);
		wm8994_reg_write(amp, 0x4, 0x0);
		wm8994_reg_write(amp, 0x5, 0x3303);
		
		wm8994_reg_write(amp, 0x19, 0x010b);
		wm8994_reg_write(amp, 0x1b, 0x010b);

		wm8994_reg_write(amp, 0x28, 0x00cc);
		wm8994_reg_write(amp, 0x29, 0x0100);
		wm8994_reg_write(amp, 0x2A, 0x0100);

		wm8994_reg_write(amp, 0x2B, 0x0000);
		wm8994_reg_write(amp, 0x2C, 0x0000);
		wm8994_reg_write(amp, 0x2F, 0x0200);
		wm8994_reg_write(amp, 0x30, 0x0200);

		wm8994_reg_write(amp, 0x2d, 0x0002);
		wm8994_reg_write(amp, 0x2e, 0x0002);

		wm8994_reg_write(amp, 0x37, 0x0000);
		wm8994_reg_write(amp, 0x38, 0x0080);
		wm8994_reg_write(amp, 0x35, 0x0031);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		// HPF setting
		wm8994_reg_write(amp, 0x410, 0x1800);

		// DRC Control tuning
		wm8994_reg_write(amp, 0x444, 0x02DA);
		wm8994_reg_write(amp, 0x443, 0x0000);
		wm8994_reg_write(amp, 0x442, 0x0400);
		wm8994_reg_write(amp, 0x441, 0x0845);
		wm8994_reg_write(amp, 0x440, 0x01BB);
		wm8994_reg_write(amp, 0x480, 0x6318);

		wm8994_reg_write(amp, 0x400, 0x1CA);
		wm8994_reg_write(amp, 0x401, 0x1CA);
		wm8994_reg_write(amp, 0x480, 0x6318);

		// Analogue out gain control
		wm8994_reg_write(amp, 0x20, 0x017F);
		wm8994_reg_write(amp, 0x21, 0x017F);

		// Line AMP Enable
		wm8994_reg_write(amp, 0x1E, 0x60); // Check schematic


		wm8994_reg_write(amp, 0x02, 0x63A0); // Check schematic
		wm8994_reg_write(amp, 0x03, 0x0CF0);
		wm8994_reg_write(amp, 0x01, 0x3303); 
		
		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x1, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x5, 0x3303);
			wm8994_reg_write(amp, 0x3, 0x0FF0);
			wm8994_reg_write(amp, 0x4, 0x0303);

			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(180);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -2)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -2)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;
			
			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
			CompensationCAL = 1;
		}	 
		// Unmute
		wm8994_reg_write(amp, 0x420, 0x0000);
	} else {
		printk("%s: cradle amp off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);

		wm8994_reg_write(amp, 0x02, 0x63A0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0044);
		wm8994_reg_write(amp, 0x29, 0x0100);
		wm8994_reg_write(amp, 0x2A, 0x0100);

		wm8994_reg_write(amp, 0x36, 0x00C0);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x3, 0x0FF0);

		wm8994_reg_write(amp, 0x2D, 0x0010);
		wm8994_reg_write(amp, 0x2E, 0x0010);
		
		wm8994_reg_write(amp, 0x4c, 0x9f25);
		wm8994_reg_write(amp, 0x1, 0x3303);
		wm8994_reg_write(amp, 0x4, 0x0303);
		wm8994_reg_write(amp, 0x5, 0x3303);
	}
}

void wm8994_set_spk_cradle(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;
	
	if(wm_check_inited == 0)
	return;

	if(onoff==1){
		printk("%s: cradle_spk amp on\n",__func__);

		// Analgoue input gain, IN2P L/R
		wm8994_reg_write(amp, 0x02, 0x63A0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x00CC);
		wm8994_reg_write(amp, 0x29, 0x0100);
		wm8994_reg_write(amp, 0x2A, 0x0100);

		wm8994_reg_write(amp, 0x36, 0x00C0);
		
		wm8994_reg_write(amp, 0x2d, 0x0001);
		wm8994_reg_write(amp, 0x2e, 0x0001);
		wm8994_reg_write(amp, 0x37, 0x0000);
		wm8994_reg_write(amp, 0x38, 0x0080);
		wm8994_reg_write(amp, 0x35, 0x0031);

		// HPF setting
		wm8994_reg_write(amp, 0x410, 0x1800);

		// DRC Control tuning
		wm8994_reg_write(amp, 0x444, 0x02DA);
		wm8994_reg_write(amp, 0x443, 0x0000);
		wm8994_reg_write(amp, 0x442, 0x0400);

		wm8994_reg_write(amp, 0x441, 0x0845);
		wm8994_reg_write(amp, 0x440, 0x01BB);
		wm8994_reg_write(amp, 0x480, 0x6318);

		wm8994_reg_write(amp, 0x400, 0x1D4);
		wm8994_reg_write(amp, 0x401, 0x1D4);

		// Analogue out gain control
		wm8994_reg_write(amp, 0x20, 0x0079);
		wm8994_reg_write(amp, 0x21, 0x0179);

		// Line AMP Enable
		wm8994_reg_write(amp, 0x1E, 0x60); // Check schematic

		wm8994_reg_write(amp, 0x22, 0x0); // Check schematic
		wm8994_reg_write(amp, 0x23, 0x0); // Check schematic
		wm8994_reg_write(amp, 0x36, 0x00C0);
		// Analogue out gain control
		wm8994_reg_write(amp, 0x25, 0x0176);
		wm8994_reg_write(amp, 0x26, 0x0078);
		wm8994_reg_write(amp, 0x27, 0x0178);

		wm8994_reg_write(amp, 0x02, 0x63A0); // Check schematic
		wm8994_reg_write(amp, 0x03, 0x0FF0);
		wm8994_reg_write(amp, 0x01, 0x3303); //
		
		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x1, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x5, 0x3303);
			wm8994_reg_write(amp, 0x3, 0x0FF0);
			wm8994_reg_write(amp, 0x4, 0x0303);

			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(180);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -2)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -2)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;
			
			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
			CompensationCAL = 1;
		}	 
		// Unmute
		wm8994_reg_write(amp, 0x420, 0x0000);
	} else {
		printk("%s: cradle_spk amp off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);

		wm8994_reg_write(amp, 0x02, 0x63A0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0044);
		wm8994_reg_write(amp, 0x29, 0x0100);
		wm8994_reg_write(amp, 0x2A, 0x0100);

		wm8994_reg_write(amp, 0x36, 0x00C0);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x2D, 0x0010);
		wm8994_reg_write(amp, 0x2E, 0x0010);

		wm8994_reg_write(amp, 0x4c, 0x9f25);
		wm8994_reg_write(amp, 0x1, 0x3303);
	}
}


#else // P4LTE
int audio_power(int en)
{
	struct wm8994 *amp = wm8994_amp;

	if(en == 1) {
		printk("%s: audio_power on\n",__func__);
		CompensationCAL = 0;
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_AMP_EN), 1);
		msleep(50);

		wm8994_reg_write(amp, 0x39, 0x006C);
		wm8994_reg_write(amp, 0x01, 0x0003);
		msleep(50);

		// internal osc enable
		wm8994_reg_write(amp, 0x220, 0x0002);
		msleep(200);

		wm8994_reg_write(amp, 0x224, 0x0EC0);
		wm8994_reg_write(amp, 0x221, 0x0600);

		wm8994_reg_write(amp, 0x200, 0x0010);

		wm8994_reg_write(amp, 0x620, 0x0000);
		wm8994_reg_write(amp, 0x302, 0x7000);
		wm8994_reg_write(amp, 0x301, 0x4001);

		wm8994_reg_write(amp, 0x601, 0x0001);
		wm8994_reg_write(amp, 0x602, 0x0001);

		wm8994_reg_write(amp, 0x610, 0x00C0);
		wm8994_reg_write(amp, 0x611, 0x01C0);
		wm8994_reg_write(amp, 0x606, 0x0002);
		wm8994_reg_write(amp, 0x607, 0x0002);
		wm8994_reg_write(amp, 0x208, 0x000A);
		wm8994_reg_write(amp, 0x200, 0x0011); // End of clocking
		msleep(10);
		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);

		wm8994_reg_write(amp, 0x28, 0x0055);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x2D, 0x0001);
		wm8994_reg_write(amp, 0x2E, 0x0001);

		wm8994_reg_write(amp, 0x36, 0x0003);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);
		wm8994_reg_write(amp, 0x24, 0x0011);
		wm8994_reg_write(amp, 0x25, 0x016D);

		wm8994_reg_write(amp, 0x410, 0x3800);

		wm8994_reg_write(amp, 0x444, 0x02B9);
		wm8994_reg_write(amp, 0x443, 0x01EF);
		wm8994_reg_write(amp, 0x442, 0x0418);

		wm8994_reg_write(amp, 0x441, 0x0825);
		wm8994_reg_write(amp, 0x440, 0x01BB);

		wm8994_reg_write(amp, 0x400, 0x1D8);
		wm8994_reg_write(amp, 0x401, 0x1D8);

	} else {
		printk("%s: audio_power off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);

		wm8994_reg_write(amp, 0x1C, 0x0131);
		wm8994_reg_write(amp, 0x1D, 0x0131);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x0, 0x8994);
		msleep(50);
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_AMP_EN), 0);
	}

	return 0;
}


void wm8994_set_headset(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	if(wm_check_inited == 0)
		return;

	if(onoff == 1) {
		printk("%s: headset amp on\n",__func__);
		wm8994_reg_write(amp, 0x02, 0x63A0);
		msleep(50);

		wm8994_reg_write(amp, 0x28, 0x0044);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);

		wm8994_reg_write(amp, 0x1C, 0x0174);
		wm8994_reg_write(amp, 0x1D, 0x0174);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x410, 0x1800);

		wm8994_reg_write(amp, 0x440, 0x1BB);
		wm8994_reg_write(amp, 0x444, 0x298);
		wm8994_reg_write(amp, 0x400, 0x1C0);
		wm8994_reg_write(amp, 0x401, 0x1C0);
		wm8994_reg_write(amp, 0x442, 0x0400);
		wm8994_reg_write(amp, 0x441, 0x0805);

		wm8994_reg_write(amp, 0x610, 0x01c0);
		wm8994_reg_write(amp, 0x611, 0x01c0);
		
		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x1, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x5, 0x3303);
			wm8994_reg_write(amp, 0x3, 0x0FF0);
			wm8994_reg_write(amp, 0x4, 0x0303);

			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(160);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -2)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -2)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;
			
			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
			CompensationCAL = 1;
		}	 
		wm8994_reg_write(amp, 0x420, 0x0000);
	} else {
		printk("%s: headset amp off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);
		wm8994_reg_write(amp, 0x02, 0x63F0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0055);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x36, 0x0003);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x3, 0x0FF0);


		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x1C, 0x01B4);
		wm8994_reg_write(amp, 0x1D, 0x01B4);
	}
}

void wm8994_set_speaker(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	if(wm_check_inited == 0)
		return;

	if(onoff==1){
		printk("%s: spk amp on\n",__func__);
		wm8994_reg_write(amp, 0x26, 0x0175);
		wm8994_reg_write(amp, 0x27, 0x0175);

		wm8994_reg_write(amp, 0x1C, 0x0131);
		wm8994_reg_write(amp, 0x1D, 0x0131);

		wm8994_reg_write(amp, 0x20, 0x0131);
		wm8994_reg_write(amp, 0x21, 0x0131);
		
		wm8994_reg_write(amp, 0x02, 0x63F0);
		msleep(50);

		wm8994_reg_write(amp, 0x610, 0x01c0);
		wm8994_reg_write(amp, 0x611, 0x01c0);

		wm8994_reg_write(amp, 0x420, 0x0000);
		wm8994_reg_write(amp, 0x410, 0x1800);

		// DRC Control tuning
		wm8994_reg_write(amp, 0x444, 0x02DA);
		wm8994_reg_write(amp, 0x443, 0x01EF);
		wm8994_reg_write(amp, 0x442, 0x0418);

		wm8994_reg_write(amp, 0x400, 0x1D3);
		wm8994_reg_write(amp, 0x401, 0x1D3);
		wm8994_reg_write(amp, 0x481, 0x5A80);
		wm8994_reg_write(amp, 0x480, 0x6318);

		// Analogue out gain control
		wm8994_reg_write(amp, 0x25, 0x017F);

		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x1, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x5, 0x3303);
			wm8994_reg_write(amp, 0x3, 0x0FF0);
			wm8994_reg_write(amp, 0x4, 0x0303);

			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
		}	 
	} else {
		printk("%s: spk amp off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x1C, 0x0131);
		wm8994_reg_write(amp, 0x1D, 0x0131);
	}
}

void wm8994_set_speaker_headset(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;

	if(wm_check_inited == 0)
		return;

	if(onoff==1){
		printk("%s: headset_spk amp on\n",__func__);
#ifdef CONFIG_MACH_P4_LTE
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SPK_SEL), 1);
#endif
		wm8994_reg_write(amp, 0x02, 0x63F0);
		msleep(50);
		// Analgoue input gain
		wm8994_reg_write(amp, 0x18, 0x000b);
		wm8994_reg_write(amp, 0x1A, 0x010b);
		wm8994_reg_write(amp, 0x28, 0x0011);
		wm8994_reg_write(amp, 0x29, 0x0020);
		wm8994_reg_write(amp, 0x2A, 0x0020);
		wm8994_reg_write(amp, 0x03, 0x03F0);
		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);
		wm8994_reg_write(amp, 0x36, 0x0003);

		wm8994_reg_write(amp, 0x26, 0x0175);
		wm8994_reg_write(amp, 0x27, 0x0175);		

		wm8994_reg_write(amp, 0x610, 0x01c0);
		wm8994_reg_write(amp, 0x611, 0x01c0);

		wm8994_reg_write(amp, 0x420, 0x0000);

		wm8994_reg_write(amp, 0x443, 0x01EF);
		wm8994_reg_write(amp, 0x442, 0x0418);
		// Analogue out gain control
		wm8994_reg_write(amp, 0x400, 0x1D8);
		wm8994_reg_write(amp, 0x401, 0x1D8);

		wm8994_reg_write(amp, 0x25, 0x017F);

		wm8994_reg_write(amp, 0x1C, 0x0167);
		wm8994_reg_write(amp, 0x1D, 0x0167);
		
		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x1, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x5, 0x3303);
			wm8994_reg_write(amp, 0x3, 0x0FF0);
			wm8994_reg_write(amp, 0x4, 0x0303);

			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(180);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -2)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -2)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;
			
			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
			CompensationCAL = 1;
		}	 
	} else {
		printk("%s: headset_spk amp off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);

		wm8994_reg_write(amp, 0x02, 0x63F0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0055);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x36, 0x0003);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x3, 0x0FF0);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		wm8994_reg_write(amp, 0x1C, 0x01A0);
		wm8994_reg_write(amp, 0x1D, 0x01A0);
	}
}



void wm8994_set_cradle(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;
	
	if(wm_check_inited == 0)
		return;

	if(onoff==1){
		printk("%s: cradle amp on\n",__func__);

		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SPK_SEL), 0);

		wm8994_reg_write(amp, 0x19, 0x0005);
		wm8994_reg_write(amp, 0x1b, 0x0105);

		wm8994_reg_write(amp, 0x28, 0x00cc);
		wm8994_reg_write(amp, 0x29, 0x0100);
		wm8994_reg_write(amp, 0x2A, 0x0100);

		wm8994_reg_write(amp, 0x2d, 0x0001);
		wm8994_reg_write(amp, 0x2e, 0x0001);

		wm8994_reg_write(amp, 0x37, 0x0000);

		wm8994_reg_write(amp, 0x38, 0x0080);
		wm8994_reg_write(amp, 0x35, 0x0031);

		wm8994_reg_write(amp, 0x26, 0x0139);
		wm8994_reg_write(amp, 0x27, 0x0139);

		// HPF setting
		wm8994_reg_write(amp, 0x410, 0x1800);

		// DRC Control tuning
		wm8994_reg_write(amp, 0x444, 0x0256);
		wm8994_reg_write(amp, 0x443, 0x01EF);
		wm8994_reg_write(amp, 0x442, 0x0418);

		wm8994_reg_write(amp, 0x441, 0x0845);
		wm8994_reg_write(amp, 0x440, 0x01BB);

		wm8994_reg_write(amp, 0x480, 0x6318);

		wm8994_reg_write(amp, 0x400, 0x1E0);
		wm8994_reg_write(amp, 0x401, 0x1E0);
		wm8994_reg_write(amp, 0x480, 0x6318);

		// Analogue out gain control
		wm8994_reg_write(amp, 0x20, 0x017F);
		wm8994_reg_write(amp, 0x21, 0x007F);

		// Line AMP Enable
		wm8994_reg_write(amp, 0x1E, 0x60); // Check schematic


		wm8994_reg_write(amp, 0x02, 0x63A0); // Check schematic
		wm8994_reg_write(amp, 0x03, 0x0CF0);
		wm8994_reg_write(amp, 0x01, 0x3303); 
		
		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x1, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x5, 0x3303);
			wm8994_reg_write(amp, 0x3, 0x0FF0);
			wm8994_reg_write(amp, 0x4, 0x0303);

			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(180);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -2)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -2)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;
			
			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
			CompensationCAL = 1;
		}	 
		// Unmute
		wm8994_reg_write(amp, 0x420, 0x0000);
	} else {
		printk("%s: cradle amp off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);

		wm8994_reg_write(amp, 0x02, 0x63F0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0055);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x36, 0x0003);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x3, 0x0FF0);

		wm8994_reg_write(amp, 0x2D, 0x0010);
		wm8994_reg_write(amp, 0x2E, 0x0010);
		
		wm8994_reg_write(amp, 0x4c, 0x9f25);
		wm8994_reg_write(amp, 0x1, 0x3303);
	}
}

void wm8994_set_spk_cradle(int onoff)
{
	struct wm8994 *amp = wm8994_amp;
	u16 nReadServo4Val = 0;
	static u16 ncompensationResult = 0;
	u16 nCompensationResultLow=0;
	u16 nCompensationResultHigh=0;
	u8	nServo4Low = 0;
	u8	nServo4High = 0;
	
	if(wm_check_inited == 0)
	return;

	if(onoff==1){
		printk("%s: cradle_spk amp on\n",__func__);

		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SPK_SEL), 0);
		// Analgoue input gain, IN2P L/R
		wm8994_reg_write(amp, 0x19, 0x000b);
		wm8994_reg_write(amp, 0x1b, 0x010b);

		wm8994_reg_write(amp, 0x28, 0x00cc);
		wm8994_reg_write(amp, 0x29, 0x0100);
		wm8994_reg_write(amp, 0x2A, 0x0100);
		
		wm8994_reg_write(amp, 0x2d, 0x0001);
		wm8994_reg_write(amp, 0x2e, 0x0001);
		wm8994_reg_write(amp, 0x37, 0x0000);
		wm8994_reg_write(amp, 0x38, 0x0080);
		wm8994_reg_write(amp, 0x35, 0x0031);

		// HPF setting
		wm8994_reg_write(amp, 0x410, 0x1800);

		// DRC Control tuning
		wm8994_reg_write(amp, 0x444, 0x02DA);
		wm8994_reg_write(amp, 0x443, 0x0000);
		wm8994_reg_write(amp, 0x442, 0x0418);
		wm8994_reg_write(amp, 0x441, 0x0845);
		wm8994_reg_write(amp, 0x440, 0x01BB);
		wm8994_reg_write(amp, 0x480, 0x6318);

		wm8994_reg_write(amp, 0x400, 0x1D8);
		wm8994_reg_write(amp, 0x401, 0x1D8);
		// Analogue out gain control
		wm8994_reg_write(amp, 0x20, 0x0079);
		wm8994_reg_write(amp, 0x21, 0x0179);

		// Line AMP Enable
		wm8994_reg_write(amp, 0x1E, 0x60); // Check schematic

		wm8994_reg_write(amp, 0x22, 0x0); // Check schematic
		wm8994_reg_write(amp, 0x23, 0x0); // Check schematic
		wm8994_reg_write(amp, 0x36, 0x3);
		// Analogue out gain control
		wm8994_reg_write(amp, 0x25, 0x017F);

		wm8994_reg_write(amp, 0x26, 0x0078);
		wm8994_reg_write(amp, 0x27, 0x0178);

		wm8994_reg_write(amp, 0x02, 0x63F0); // Check schematic
		wm8994_reg_write(amp, 0x03, 0x0FF0);
		wm8994_reg_write(amp, 0x01, 0x3303); //
		
		if(CompensationCAL == 0)
		{
			wm8994_reg_write(amp, 0x39, 0x006C);
			wm8994_reg_write(amp, 0x1, 0x0003);
			msleep(20);
			wm8994_reg_write(amp, 0x102, 0x0003);
			wm8994_reg_write(amp, 0x56, 0x0003);
			wm8994_reg_write(amp, 0x102, 0x0000);

			wm8994_reg_write(amp, 0x5D, 0x0002);
			wm8994_reg_write(amp, 0x55, 0x03E0);

			wm8994_reg_write(amp, 0x1, 0x0303);
			wm8994_reg_write(amp, 0x60, 0x0022);

			wm8994_reg_write(amp, 0x4C, 0x9F25);
			msleep(5);

			wm8994_reg_write(amp, 0x5, 0x3303);
			wm8994_reg_write(amp, 0x3, 0x0FF0);
			wm8994_reg_write(amp, 0x4, 0x0303);

			wm8994_reg_write(amp, 0x54, 0x0303);

			msleep(180);
			nReadServo4Val=wm8994_reg_read(amp, 0x57);
			nServo4Low=(signed char)(nReadServo4Val & 0xff);
			nServo4High=(signed char)((nReadServo4Val>>8) & 0xff);
			nCompensationResultLow=((signed short)nServo4Low -2)&0x00ff;
			nCompensationResultHigh=((signed short)(nServo4High -2)<<8)&0xff00;
			ncompensationResult=nCompensationResultLow|nCompensationResultHigh;
			
			wm8994_reg_write(amp, 0x57, ncompensationResult); // popup
			wm8994_reg_write(amp, 0x54, 0x000F);
			msleep(20);
			wm8994_reg_write(amp, 0x60, 0x00EE);
			wm8994_reg_write(amp, 0x01, 0x3303);
			CompensationCAL = 1;
		}	 
		// Unmute
		wm8994_reg_write(amp, 0x420, 0x0000);
	} else {
		printk("%s: cradle_spk amp off\n",__func__);

		wm8994_reg_write(amp, 0x420, 0x0200);

		wm8994_reg_write(amp, 0x02, 0x63F0);

		wm8994_reg_write(amp, 0x18, 0x010B);
		wm8994_reg_write(amp, 0x1A, 0x010B);

		wm8994_reg_write(amp, 0x19, 0x010B);
		wm8994_reg_write(amp, 0x1B, 0x010B);
		wm8994_reg_write(amp, 0x28, 0x0055);
		wm8994_reg_write(amp, 0x29, 0x0120);
		wm8994_reg_write(amp, 0x2A, 0x0120);

		wm8994_reg_write(amp, 0x36, 0x0003);

		wm8994_reg_write(amp, 0x22, 0x0000);
		wm8994_reg_write(amp, 0x23, 0x0000);

		wm8994_reg_write(amp, 0x2D, 0x0010);
		wm8994_reg_write(amp, 0x2E, 0x0010);

		wm8994_reg_write(amp, 0x4c, 0x9f25);
		wm8994_reg_write(amp, 0x1, 0x3303);
	}
}

#endif

static int wm8994_i2c_read_device(struct wm8994 *wm8994, unsigned short reg,
				  int bytes, void *dest)
{
	struct i2c_client *i2c = wm8994->control_data;
	int ret;
	u16 r = cpu_to_be16(reg);

	ret = i2c_master_send(i2c, (unsigned char *)&r, 2);
	if (ret < 0)
		{
		return ret;
		}
	if (ret != 2){
		return -EIO;
		}

	ret = i2c_master_recv(i2c, dest, bytes);

	if (ret < 0){
		return ret;
		}
	if (ret != bytes){
		return -EIO;
		}
	return 0;
}

/* Currently we allocate the write buffer on the stack; this is OK for
 * small writes - if we need to do large writes this will need to be
 * revised.
 */
static int wm8994_i2c_write_device(struct wm8994 *wm8994, unsigned short reg,
				   int bytes, void *src)
{
	struct i2c_client *i2c = wm8994->control_data;
	unsigned char msg[bytes + 2];
	int ret;

	reg = cpu_to_be16(reg);
	memcpy(&msg[0], &reg, 2);
	memcpy(&msg[2], src, bytes);

	ret = i2c_master_send(i2c, msg, bytes + 2);

	if (ret < 0){
		return ret;
		}
	if (ret < bytes + 2){
		return -EIO;
		}

	return 0;
}

#ifdef CONFIG_DEBUG_FS

static struct {
	unsigned short  readable;   /* Mask of readable bits */
	unsigned short  writable;   /* Mask of writable bits */
	unsigned short  vol;        /* Mask of volatile bits */
} access_masks[] = {
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R0     - Software Reset */
	{ 0x3B37, 0x3B37, 0x0000 }, /* R1     - Power Management (1) */
	{ 0x6BF0, 0x6BF0, 0x0000 }, /* R2     - Power Management (2) */
	{ 0x3FF0, 0x3FF0, 0x0000 }, /* R3     - Power Management (3) */
	{ 0x3F3F, 0x3F3F, 0x0000 }, /* R4     - Power Management (4) */
	{ 0x3F0F, 0x3F0F, 0x0000 }, /* R5     - Power Management (5) */
	{ 0x003F, 0x003F, 0x0000 }, /* R6     - Power Management (6) */
	{ 0x0000, 0x0000, 0x0000 }, /* R7 */
	{ 0x0000, 0x0000, 0x0000 }, /* R8 */
	{ 0x0000, 0x0000, 0x0000 }, /* R9 */
	{ 0x0000, 0x0000, 0x0000 }, /* R10 */
	{ 0x0000, 0x0000, 0x0000 }, /* R11 */
	{ 0x0000, 0x0000, 0x0000 }, /* R12 */
	{ 0x0000, 0x0000, 0x0000 }, /* R13 */
	{ 0x0000, 0x0000, 0x0000 }, /* R14 */
	{ 0x0000, 0x0000, 0x0000 }, /* R15 */
	{ 0x0000, 0x0000, 0x0000 }, /* R16 */
	{ 0x0000, 0x0000, 0x0000 }, /* R17 */
	{ 0x0000, 0x0000, 0x0000 }, /* R18 */
	{ 0x0000, 0x0000, 0x0000 }, /* R19 */
	{ 0x0000, 0x0000, 0x0000 }, /* R20 */
	{ 0x01C0, 0x01C0, 0x0000 }, /* R21    - Input Mixer (1) */
	{ 0x0000, 0x0000, 0x0000 }, /* R22 */
	{ 0x0000, 0x0000, 0x0000 }, /* R23 */
	{ 0x00DF, 0x01DF, 0x0000 }, /* R24    - Left Line Input 1&2 Volume */
	{ 0x00DF, 0x01DF, 0x0000 }, /* R25    - Left Line Input 3&4 Volume */
	{ 0x00DF, 0x01DF, 0x0000 }, /* R26    - Right Line Input 1&2 Volume */
	{ 0x00DF, 0x01DF, 0x0000 }, /* R27    - Right Line Input 3&4 Volume */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R28    - Left Output Volume */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R29    - Right Output Volume */
	{ 0x0077, 0x0077, 0x0000 }, /* R30    - Line Outputs Volume */
	{ 0x0030, 0x0030, 0x0000 }, /* R31    - HPOUT2 Volume */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R32    - Left OPGA Volume */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R33    - Right OPGA Volume */
	{ 0x007F, 0x007F, 0x0000 }, /* R34    - SPKMIXL Attenuation */
	{ 0x017F, 0x017F, 0x0000 }, /* R35    - SPKMIXR Attenuation */
	{ 0x003F, 0x003F, 0x0000 }, /* R36    - SPKOUT Mixers */
	{ 0x003F, 0x003F, 0x0000 }, /* R37    - ClassD */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R38    - Speaker Volume Left */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R39    - Speaker Volume Right */
	{ 0x00FF, 0x00FF, 0x0000 }, /* R40    - Input Mixer (2) */
	{ 0x01B7, 0x01B7, 0x0000 }, /* R41    - Input Mixer (3) */
	{ 0x01B7, 0x01B7, 0x0000 }, /* R42    - Input Mixer (4) */
	{ 0x01C7, 0x01C7, 0x0000 }, /* R43    - Input Mixer (5) */
	{ 0x01C7, 0x01C7, 0x0000 }, /* R44    - Input Mixer (6) */
	{ 0x01FF, 0x01FF, 0x0000 }, /* R45    - Output Mixer (1) */
	{ 0x01FF, 0x01FF, 0x0000 }, /* R46    - Output Mixer (2) */
	{ 0x0FFF, 0x0FFF, 0x0000 }, /* R47    - Output Mixer (3) */
	{ 0x0FFF, 0x0FFF, 0x0000 }, /* R48    - Output Mixer (4) */
	{ 0x0FFF, 0x0FFF, 0x0000 }, /* R49    - Output Mixer (5) */
	{ 0x0FFF, 0x0FFF, 0x0000 }, /* R50    - Output Mixer (6) */
	{ 0x0038, 0x0038, 0x0000 }, /* R51    - HPOUT2 Mixer */
	{ 0x0077, 0x0077, 0x0000 }, /* R52    - Line Mixer (1) */
	{ 0x0077, 0x0077, 0x0000 }, /* R53    - Line Mixer (2) */
	{ 0x03FF, 0x03FF, 0x0000 }, /* R54    - Speaker Mixer */
	{ 0x00C1, 0x00C1, 0x0000 }, /* R55    - Additional Control */
	{ 0x00F0, 0x00F0, 0x0000 }, /* R56    - AntiPOP (1) */
	{ 0x01EF, 0x01EF, 0x0000 }, /* R57    - AntiPOP (2) */
	{ 0x00FF, 0x00FF, 0x0000 }, /* R58    - MICBIAS */
	{ 0x000F, 0x000F, 0x0000 }, /* R59    - LDO 1 */
	{ 0x0007, 0x0007, 0x0000 }, /* R60    - LDO 2 */
	{ 0x0000, 0x0000, 0x0000 }, /* R61 */
	{ 0x0000, 0x0000, 0x0000 }, /* R62 */
	{ 0x0000, 0x0000, 0x0000 }, /* R63 */
	{ 0x0000, 0x0000, 0x0000 }, /* R64 */
	{ 0x0000, 0x0000, 0x0000 }, /* R65 */
	{ 0x0000, 0x0000, 0x0000 }, /* R66 */
	{ 0x0000, 0x0000, 0x0000 }, /* R67 */
	{ 0x0000, 0x0000, 0x0000 }, /* R68 */
	{ 0x0000, 0x0000, 0x0000 }, /* R69 */
	{ 0x0000, 0x0000, 0x0000 }, /* R70 */
	{ 0x0000, 0x0000, 0x0000 }, /* R71 */
	{ 0x0000, 0x0000, 0x0000 }, /* R72 */
	{ 0x0000, 0x0000, 0x0000 }, /* R73 */
	{ 0x0000, 0x0000, 0x0000 }, /* R74 */
	{ 0x0000, 0x0000, 0x0000 }, /* R75 */
	{ 0x8000, 0x8000, 0x0000 }, /* R76    - Charge Pump (1) */
	{ 0x0000, 0x0000, 0x0000 }, /* R77 */
	{ 0x0000, 0x0000, 0x0000 }, /* R78 */
	{ 0x0000, 0x0000, 0x0000 }, /* R79 */
	{ 0x0000, 0x0000, 0x0000 }, /* R80 */
	{ 0x0301, 0x0301, 0x0000 }, /* R81    - Class W (1) */
	{ 0x0000, 0x0000, 0x0000 }, /* R82 */
	{ 0x0000, 0x0000, 0x0000 }, /* R83 */
	{ 0x333F, 0x333F, 0x0000 }, /* R84    - DC Servo (1) */
	{ 0x0FEF, 0x0FEF, 0x0000 }, /* R85    - DC Servo (2) */
	{ 0x0000, 0x0000, 0x0000 }, /* R86 */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R87    - DC Servo (4) */
	{ 0x0333, 0x0000, 0x0000 }, /* R88    - DC Servo Readback */
	{ 0x0000, 0x0000, 0x0000 }, /* R89 */
	{ 0x0000, 0x0000, 0x0000 }, /* R90 */
	{ 0x0000, 0x0000, 0x0000 }, /* R91 */
	{ 0x0000, 0x0000, 0x0000 }, /* R92 */
	{ 0x0000, 0x0000, 0x0000 }, /* R93 */
	{ 0x0000, 0x0000, 0x0000 }, /* R94 */
	{ 0x0000, 0x0000, 0x0000 }, /* R95 */
	{ 0x00EE, 0x00EE, 0x0000 }, /* R96    - Analogue HP (1) */
	{ 0x0000, 0x0000, 0x0000 }, /* R97 */
	{ 0x0000, 0x0000, 0x0000 }, /* R98 */
	{ 0x0000, 0x0000, 0x0000 }, /* R99 */
	{ 0x0000, 0x0000, 0x0000 }, /* R100 */
	{ 0x0000, 0x0000, 0x0000 }, /* R101 */
	{ 0x0000, 0x0000, 0x0000 }, /* R102 */
	{ 0x0000, 0x0000, 0x0000 }, /* R103 */
	{ 0x0000, 0x0000, 0x0000 }, /* R104 */
	{ 0x0000, 0x0000, 0x0000 }, /* R105 */
	{ 0x0000, 0x0000, 0x0000 }, /* R106 */
	{ 0x0000, 0x0000, 0x0000 }, /* R107 */
	{ 0x0000, 0x0000, 0x0000 }, /* R108 */
	{ 0x0000, 0x0000, 0x0000 }, /* R109 */
	{ 0x0000, 0x0000, 0x0000 }, /* R110 */
	{ 0x0000, 0x0000, 0x0000 }, /* R111 */
	{ 0x0000, 0x0000, 0x0000 }, /* R112 */
	{ 0x0000, 0x0000, 0x0000 }, /* R113 */
	{ 0x0000, 0x0000, 0x0000 }, /* R114 */
	{ 0x0000, 0x0000, 0x0000 }, /* R115 */
	{ 0x0000, 0x0000, 0x0000 }, /* R116 */
	{ 0x0000, 0x0000, 0x0000 }, /* R117 */
	{ 0x0000, 0x0000, 0x0000 }, /* R118 */
	{ 0x0000, 0x0000, 0x0000 }, /* R119 */
	{ 0x0000, 0x0000, 0x0000 }, /* R120 */
	{ 0x0000, 0x0000, 0x0000 }, /* R121 */
	{ 0x0000, 0x0000, 0x0000 }, /* R122 */
	{ 0x0000, 0x0000, 0x0000 }, /* R123 */
	{ 0x0000, 0x0000, 0x0000 }, /* R124 */
	{ 0x0000, 0x0000, 0x0000 }, /* R125 */
	{ 0x0000, 0x0000, 0x0000 }, /* R126 */
	{ 0x0000, 0x0000, 0x0000 }, /* R127 */
	{ 0x0000, 0x0000, 0x0000 }, /* R128 */
	{ 0x0000, 0x0000, 0x0000 }, /* R129 */
	{ 0x0000, 0x0000, 0x0000 }, /* R130 */
	{ 0x0000, 0x0000, 0x0000 }, /* R131 */
	{ 0x0000, 0x0000, 0x0000 }, /* R132 */
	{ 0x0000, 0x0000, 0x0000 }, /* R133 */
	{ 0x0000, 0x0000, 0x0000 }, /* R134 */
	{ 0x0000, 0x0000, 0x0000 }, /* R135 */
	{ 0x0000, 0x0000, 0x0000 }, /* R136 */
	{ 0x0000, 0x0000, 0x0000 }, /* R137 */
	{ 0x0000, 0x0000, 0x0000 }, /* R138 */
	{ 0x0000, 0x0000, 0x0000 }, /* R139 */
	{ 0x0000, 0x0000, 0x0000 }, /* R140 */
	{ 0x0000, 0x0000, 0x0000 }, /* R141 */
	{ 0x0000, 0x0000, 0x0000 }, /* R142 */
	{ 0x0000, 0x0000, 0x0000 }, /* R143 */
	{ 0x0000, 0x0000, 0x0000 }, /* R144 */
	{ 0x0000, 0x0000, 0x0000 }, /* R145 */
	{ 0x0000, 0x0000, 0x0000 }, /* R146 */
	{ 0x0000, 0x0000, 0x0000 }, /* R147 */
	{ 0x0000, 0x0000, 0x0000 }, /* R148 */
	{ 0x0000, 0x0000, 0x0000 }, /* R149 */
	{ 0x0000, 0x0000, 0x0000 }, /* R150 */
	{ 0x0000, 0x0000, 0x0000 }, /* R151 */
	{ 0x0000, 0x0000, 0x0000 }, /* R152 */
	{ 0x0000, 0x0000, 0x0000 }, /* R153 */
	{ 0x0000, 0x0000, 0x0000 }, /* R154 */
	{ 0x0000, 0x0000, 0x0000 }, /* R155 */
	{ 0x0000, 0x0000, 0x0000 }, /* R156 */
	{ 0x0000, 0x0000, 0x0000 }, /* R157 */
	{ 0x0000, 0x0000, 0x0000 }, /* R158 */
	{ 0x0000, 0x0000, 0x0000 }, /* R159 */
	{ 0x0000, 0x0000, 0x0000 }, /* R160 */
	{ 0x0000, 0x0000, 0x0000 }, /* R161 */
	{ 0x0000, 0x0000, 0x0000 }, /* R162 */
	{ 0x0000, 0x0000, 0x0000 }, /* R163 */
	{ 0x0000, 0x0000, 0x0000 }, /* R164 */
	{ 0x0000, 0x0000, 0x0000 }, /* R165 */
	{ 0x0000, 0x0000, 0x0000 }, /* R166 */
	{ 0x0000, 0x0000, 0x0000 }, /* R167 */
	{ 0x0000, 0x0000, 0x0000 }, /* R168 */
	{ 0x0000, 0x0000, 0x0000 }, /* R169 */
	{ 0x0000, 0x0000, 0x0000 }, /* R170 */
	{ 0x0000, 0x0000, 0x0000 }, /* R171 */
	{ 0x0000, 0x0000, 0x0000 }, /* R172 */
	{ 0x0000, 0x0000, 0x0000 }, /* R173 */
	{ 0x0000, 0x0000, 0x0000 }, /* R174 */
	{ 0x0000, 0x0000, 0x0000 }, /* R175 */
	{ 0x0000, 0x0000, 0x0000 }, /* R176 */
	{ 0x0000, 0x0000, 0x0000 }, /* R177 */
	{ 0x0000, 0x0000, 0x0000 }, /* R178 */
	{ 0x0000, 0x0000, 0x0000 }, /* R179 */
	{ 0x0000, 0x0000, 0x0000 }, /* R180 */
	{ 0x0000, 0x0000, 0x0000 }, /* R181 */
	{ 0x0000, 0x0000, 0x0000 }, /* R182 */
	{ 0x0000, 0x0000, 0x0000 }, /* R183 */
	{ 0x0000, 0x0000, 0x0000 }, /* R184 */
	{ 0x0000, 0x0000, 0x0000 }, /* R185 */
	{ 0x0000, 0x0000, 0x0000 }, /* R186 */
	{ 0x0000, 0x0000, 0x0000 }, /* R187 */
	{ 0x0000, 0x0000, 0x0000 }, /* R188 */
	{ 0x0000, 0x0000, 0x0000 }, /* R189 */
	{ 0x0000, 0x0000, 0x0000 }, /* R190 */
	{ 0x0000, 0x0000, 0x0000 }, /* R191 */
	{ 0x0000, 0x0000, 0x0000 }, /* R192 */
	{ 0x0000, 0x0000, 0x0000 }, /* R193 */
	{ 0x0000, 0x0000, 0x0000 }, /* R194 */
	{ 0x0000, 0x0000, 0x0000 }, /* R195 */
	{ 0x0000, 0x0000, 0x0000 }, /* R196 */
	{ 0x0000, 0x0000, 0x0000 }, /* R197 */
	{ 0x0000, 0x0000, 0x0000 }, /* R198 */
	{ 0x0000, 0x0000, 0x0000 }, /* R199 */
	{ 0x0000, 0x0000, 0x0000 }, /* R200 */
	{ 0x0000, 0x0000, 0x0000 }, /* R201 */
	{ 0x0000, 0x0000, 0x0000 }, /* R202 */
	{ 0x0000, 0x0000, 0x0000 }, /* R203 */
	{ 0x0000, 0x0000, 0x0000 }, /* R204 */
	{ 0x0000, 0x0000, 0x0000 }, /* R205 */
	{ 0x0000, 0x0000, 0x0000 }, /* R206 */
	{ 0x0000, 0x0000, 0x0000 }, /* R207 */
	{ 0x0000, 0x0000, 0x0000 }, /* R208 */
	{ 0x0000, 0x0000, 0x0000 }, /* R209 */
	{ 0x0000, 0x0000, 0x0000 }, /* R210 */
	{ 0x0000, 0x0000, 0x0000 }, /* R211 */
	{ 0x0000, 0x0000, 0x0000 }, /* R212 */
	{ 0x0000, 0x0000, 0x0000 }, /* R213 */
	{ 0x0000, 0x0000, 0x0000 }, /* R214 */
	{ 0x0000, 0x0000, 0x0000 }, /* R215 */
	{ 0x0000, 0x0000, 0x0000 }, /* R216 */
	{ 0x0000, 0x0000, 0x0000 }, /* R217 */
	{ 0x0000, 0x0000, 0x0000 }, /* R218 */
	{ 0x0000, 0x0000, 0x0000 }, /* R219 */
	{ 0x0000, 0x0000, 0x0000 }, /* R220 */
	{ 0x0000, 0x0000, 0x0000 }, /* R221 */
	{ 0x0000, 0x0000, 0x0000 }, /* R222 */
	{ 0x0000, 0x0000, 0x0000 }, /* R223 */
	{ 0x0000, 0x0000, 0x0000 }, /* R224 */
	{ 0x0000, 0x0000, 0x0000 }, /* R225 */
	{ 0x0000, 0x0000, 0x0000 }, /* R226 */
	{ 0x0000, 0x0000, 0x0000 }, /* R227 */
	{ 0x0000, 0x0000, 0x0000 }, /* R228 */
	{ 0x0000, 0x0000, 0x0000 }, /* R229 */
	{ 0x0000, 0x0000, 0x0000 }, /* R230 */
	{ 0x0000, 0x0000, 0x0000 }, /* R231 */
	{ 0x0000, 0x0000, 0x0000 }, /* R232 */
	{ 0x0000, 0x0000, 0x0000 }, /* R233 */
	{ 0x0000, 0x0000, 0x0000 }, /* R234 */
	{ 0x0000, 0x0000, 0x0000 }, /* R235 */
	{ 0x0000, 0x0000, 0x0000 }, /* R236 */
	{ 0x0000, 0x0000, 0x0000 }, /* R237 */
	{ 0x0000, 0x0000, 0x0000 }, /* R238 */
	{ 0x0000, 0x0000, 0x0000 }, /* R239 */
	{ 0x0000, 0x0000, 0x0000 }, /* R240 */
	{ 0x0000, 0x0000, 0x0000 }, /* R241 */
	{ 0x0000, 0x0000, 0x0000 }, /* R242 */
	{ 0x0000, 0x0000, 0x0000 }, /* R243 */
	{ 0x0000, 0x0000, 0x0000 }, /* R244 */
	{ 0x0000, 0x0000, 0x0000 }, /* R245 */
	{ 0x0000, 0x0000, 0x0000 }, /* R246 */
	{ 0x0000, 0x0000, 0x0000 }, /* R247 */
	{ 0x0000, 0x0000, 0x0000 }, /* R248 */
	{ 0x0000, 0x0000, 0x0000 }, /* R249 */
	{ 0x0000, 0x0000, 0x0000 }, /* R250 */
	{ 0x0000, 0x0000, 0x0000 }, /* R251 */
	{ 0x0000, 0x0000, 0x0000 }, /* R252 */
	{ 0x0000, 0x0000, 0x0000 }, /* R253 */
	{ 0x0000, 0x0000, 0x0000 }, /* R254 */
	{ 0x0000, 0x0000, 0x0000 }, /* R255 */
	{ 0x000F, 0x0000, 0x0000 }, /* R256   - Chip Revision */
	{ 0x0074, 0x0074, 0x0000 }, /* R257   - Control Interface */
	{ 0x0000, 0x0000, 0x0000 }, /* R258 */
	{ 0x0000, 0x0000, 0x0000 }, /* R259 */
	{ 0x0000, 0x0000, 0x0000 }, /* R260 */
	{ 0x0000, 0x0000, 0x0000 }, /* R261 */
	{ 0x0000, 0x0000, 0x0000 }, /* R262 */
	{ 0x0000, 0x0000, 0x0000 }, /* R263 */
	{ 0x0000, 0x0000, 0x0000 }, /* R264 */
	{ 0x0000, 0x0000, 0x0000 }, /* R265 */
	{ 0x0000, 0x0000, 0x0000 }, /* R266 */
	{ 0x0000, 0x0000, 0x0000 }, /* R267 */
	{ 0x0000, 0x0000, 0x0000 }, /* R268 */
	{ 0x0000, 0x0000, 0x0000 }, /* R269 */
	{ 0x0000, 0x0000, 0x0000 }, /* R270 */
	{ 0x0000, 0x0000, 0x0000 }, /* R271 */
	{ 0x807F, 0x837F, 0x0000 }, /* R272   - Write Sequencer Ctrl (1) */
	{ 0x017F, 0x0000, 0x0000 }, /* R273   - Write Sequencer Ctrl (2) */
	{ 0x0000, 0x0000, 0x0000 }, /* R274 */
	{ 0x0000, 0x0000, 0x0000 }, /* R275 */
	{ 0x0000, 0x0000, 0x0000 }, /* R276 */
	{ 0x0000, 0x0000, 0x0000 }, /* R277 */
	{ 0x0000, 0x0000, 0x0000 }, /* R278 */
	{ 0x0000, 0x0000, 0x0000 }, /* R279 */
	{ 0x0000, 0x0000, 0x0000 }, /* R280 */
	{ 0x0000, 0x0000, 0x0000 }, /* R281 */
	{ 0x0000, 0x0000, 0x0000 }, /* R282 */
	{ 0x0000, 0x0000, 0x0000 }, /* R283 */
	{ 0x0000, 0x0000, 0x0000 }, /* R284 */
	{ 0x0000, 0x0000, 0x0000 }, /* R285 */
	{ 0x0000, 0x0000, 0x0000 }, /* R286 */
	{ 0x0000, 0x0000, 0x0000 }, /* R287 */
	{ 0x0000, 0x0000, 0x0000 }, /* R288 */
	{ 0x0000, 0x0000, 0x0000 }, /* R289 */
	{ 0x0000, 0x0000, 0x0000 }, /* R290 */
	{ 0x0000, 0x0000, 0x0000 }, /* R291 */
	{ 0x0000, 0x0000, 0x0000 }, /* R292 */
	{ 0x0000, 0x0000, 0x0000 }, /* R293 */
	{ 0x0000, 0x0000, 0x0000 }, /* R294 */
	{ 0x0000, 0x0000, 0x0000 }, /* R295 */
	{ 0x0000, 0x0000, 0x0000 }, /* R296 */
	{ 0x0000, 0x0000, 0x0000 }, /* R297 */
	{ 0x0000, 0x0000, 0x0000 }, /* R298 */
	{ 0x0000, 0x0000, 0x0000 }, /* R299 */
	{ 0x0000, 0x0000, 0x0000 }, /* R300 */
	{ 0x0000, 0x0000, 0x0000 }, /* R301 */
	{ 0x0000, 0x0000, 0x0000 }, /* R302 */
	{ 0x0000, 0x0000, 0x0000 }, /* R303 */
	{ 0x0000, 0x0000, 0x0000 }, /* R304 */
	{ 0x0000, 0x0000, 0x0000 }, /* R305 */
	{ 0x0000, 0x0000, 0x0000 }, /* R306 */
	{ 0x0000, 0x0000, 0x0000 }, /* R307 */
	{ 0x0000, 0x0000, 0x0000 }, /* R308 */
	{ 0x0000, 0x0000, 0x0000 }, /* R309 */
	{ 0x0000, 0x0000, 0x0000 }, /* R310 */
	{ 0x0000, 0x0000, 0x0000 }, /* R311 */
	{ 0x0000, 0x0000, 0x0000 }, /* R312 */
	{ 0x0000, 0x0000, 0x0000 }, /* R313 */
	{ 0x0000, 0x0000, 0x0000 }, /* R314 */
	{ 0x0000, 0x0000, 0x0000 }, /* R315 */
	{ 0x0000, 0x0000, 0x0000 }, /* R316 */
	{ 0x0000, 0x0000, 0x0000 }, /* R317 */
	{ 0x0000, 0x0000, 0x0000 }, /* R318 */
	{ 0x0000, 0x0000, 0x0000 }, /* R319 */
	{ 0x0000, 0x0000, 0x0000 }, /* R320 */
	{ 0x0000, 0x0000, 0x0000 }, /* R321 */
	{ 0x0000, 0x0000, 0x0000 }, /* R322 */
	{ 0x0000, 0x0000, 0x0000 }, /* R323 */
	{ 0x0000, 0x0000, 0x0000 }, /* R324 */
	{ 0x0000, 0x0000, 0x0000 }, /* R325 */
	{ 0x0000, 0x0000, 0x0000 }, /* R326 */
	{ 0x0000, 0x0000, 0x0000 }, /* R327 */
	{ 0x0000, 0x0000, 0x0000 }, /* R328 */
	{ 0x0000, 0x0000, 0x0000 }, /* R329 */
	{ 0x0000, 0x0000, 0x0000 }, /* R330 */
	{ 0x0000, 0x0000, 0x0000 }, /* R331 */
	{ 0x0000, 0x0000, 0x0000 }, /* R332 */
	{ 0x0000, 0x0000, 0x0000 }, /* R333 */
	{ 0x0000, 0x0000, 0x0000 }, /* R334 */
	{ 0x0000, 0x0000, 0x0000 }, /* R335 */
	{ 0x0000, 0x0000, 0x0000 }, /* R336 */
	{ 0x0000, 0x0000, 0x0000 }, /* R337 */
	{ 0x0000, 0x0000, 0x0000 }, /* R338 */
	{ 0x0000, 0x0000, 0x0000 }, /* R339 */
	{ 0x0000, 0x0000, 0x0000 }, /* R340 */
	{ 0x0000, 0x0000, 0x0000 }, /* R341 */
	{ 0x0000, 0x0000, 0x0000 }, /* R342 */
	{ 0x0000, 0x0000, 0x0000 }, /* R343 */
	{ 0x0000, 0x0000, 0x0000 }, /* R344 */
	{ 0x0000, 0x0000, 0x0000 }, /* R345 */
	{ 0x0000, 0x0000, 0x0000 }, /* R346 */
	{ 0x0000, 0x0000, 0x0000 }, /* R347 */
	{ 0x0000, 0x0000, 0x0000 }, /* R348 */
	{ 0x0000, 0x0000, 0x0000 }, /* R349 */
	{ 0x0000, 0x0000, 0x0000 }, /* R350 */
	{ 0x0000, 0x0000, 0x0000 }, /* R351 */
	{ 0x0000, 0x0000, 0x0000 }, /* R352 */
	{ 0x0000, 0x0000, 0x0000 }, /* R353 */
	{ 0x0000, 0x0000, 0x0000 }, /* R354 */
	{ 0x0000, 0x0000, 0x0000 }, /* R355 */
	{ 0x0000, 0x0000, 0x0000 }, /* R356 */
	{ 0x0000, 0x0000, 0x0000 }, /* R357 */
	{ 0x0000, 0x0000, 0x0000 }, /* R358 */
	{ 0x0000, 0x0000, 0x0000 }, /* R359 */
	{ 0x0000, 0x0000, 0x0000 }, /* R360 */
	{ 0x0000, 0x0000, 0x0000 }, /* R361 */
	{ 0x0000, 0x0000, 0x0000 }, /* R362 */
	{ 0x0000, 0x0000, 0x0000 }, /* R363 */
	{ 0x0000, 0x0000, 0x0000 }, /* R364 */
	{ 0x0000, 0x0000, 0x0000 }, /* R365 */
	{ 0x0000, 0x0000, 0x0000 }, /* R366 */
	{ 0x0000, 0x0000, 0x0000 }, /* R367 */
	{ 0x0000, 0x0000, 0x0000 }, /* R368 */
	{ 0x0000, 0x0000, 0x0000 }, /* R369 */
	{ 0x0000, 0x0000, 0x0000 }, /* R370 */
	{ 0x0000, 0x0000, 0x0000 }, /* R371 */
	{ 0x0000, 0x0000, 0x0000 }, /* R372 */
	{ 0x0000, 0x0000, 0x0000 }, /* R373 */
	{ 0x0000, 0x0000, 0x0000 }, /* R374 */
	{ 0x0000, 0x0000, 0x0000 }, /* R375 */
	{ 0x0000, 0x0000, 0x0000 }, /* R376 */
	{ 0x0000, 0x0000, 0x0000 }, /* R377 */
	{ 0x0000, 0x0000, 0x0000 }, /* R378 */
	{ 0x0000, 0x0000, 0x0000 }, /* R379 */
	{ 0x0000, 0x0000, 0x0000 }, /* R380 */
	{ 0x0000, 0x0000, 0x0000 }, /* R381 */
	{ 0x0000, 0x0000, 0x0000 }, /* R382 */
	{ 0x0000, 0x0000, 0x0000 }, /* R383 */
	{ 0x0000, 0x0000, 0x0000 }, /* R384 */
	{ 0x0000, 0x0000, 0x0000 }, /* R385 */
	{ 0x0000, 0x0000, 0x0000 }, /* R386 */
	{ 0x0000, 0x0000, 0x0000 }, /* R387 */
	{ 0x0000, 0x0000, 0x0000 }, /* R388 */
	{ 0x0000, 0x0000, 0x0000 }, /* R389 */
	{ 0x0000, 0x0000, 0x0000 }, /* R390 */
	{ 0x0000, 0x0000, 0x0000 }, /* R391 */
	{ 0x0000, 0x0000, 0x0000 }, /* R392 */
	{ 0x0000, 0x0000, 0x0000 }, /* R393 */
	{ 0x0000, 0x0000, 0x0000 }, /* R394 */
	{ 0x0000, 0x0000, 0x0000 }, /* R395 */
	{ 0x0000, 0x0000, 0x0000 }, /* R396 */
	{ 0x0000, 0x0000, 0x0000 }, /* R397 */
	{ 0x0000, 0x0000, 0x0000 }, /* R398 */
	{ 0x0000, 0x0000, 0x0000 }, /* R399 */
	{ 0x0000, 0x0000, 0x0000 }, /* R400 */
	{ 0x0000, 0x0000, 0x0000 }, /* R401 */
	{ 0x0000, 0x0000, 0x0000 }, /* R402 */
	{ 0x0000, 0x0000, 0x0000 }, /* R403 */
	{ 0x0000, 0x0000, 0x0000 }, /* R404 */
	{ 0x0000, 0x0000, 0x0000 }, /* R405 */
	{ 0x0000, 0x0000, 0x0000 }, /* R406 */
	{ 0x0000, 0x0000, 0x0000 }, /* R407 */
	{ 0x0000, 0x0000, 0x0000 }, /* R408 */
	{ 0x0000, 0x0000, 0x0000 }, /* R409 */
	{ 0x0000, 0x0000, 0x0000 }, /* R410 */
	{ 0x0000, 0x0000, 0x0000 }, /* R411 */
	{ 0x0000, 0x0000, 0x0000 }, /* R412 */
	{ 0x0000, 0x0000, 0x0000 }, /* R413 */
	{ 0x0000, 0x0000, 0x0000 }, /* R414 */
	{ 0x0000, 0x0000, 0x0000 }, /* R415 */
	{ 0x0000, 0x0000, 0x0000 }, /* R416 */
	{ 0x0000, 0x0000, 0x0000 }, /* R417 */
	{ 0x0000, 0x0000, 0x0000 }, /* R418 */
	{ 0x0000, 0x0000, 0x0000 }, /* R419 */
	{ 0x0000, 0x0000, 0x0000 }, /* R420 */
	{ 0x0000, 0x0000, 0x0000 }, /* R421 */
	{ 0x0000, 0x0000, 0x0000 }, /* R422 */
	{ 0x0000, 0x0000, 0x0000 }, /* R423 */
	{ 0x0000, 0x0000, 0x0000 }, /* R424 */
	{ 0x0000, 0x0000, 0x0000 }, /* R425 */
	{ 0x0000, 0x0000, 0x0000 }, /* R426 */
	{ 0x0000, 0x0000, 0x0000 }, /* R427 */
	{ 0x0000, 0x0000, 0x0000 }, /* R428 */
	{ 0x0000, 0x0000, 0x0000 }, /* R429 */
	{ 0x0000, 0x0000, 0x0000 }, /* R430 */
	{ 0x0000, 0x0000, 0x0000 }, /* R431 */
	{ 0x0000, 0x0000, 0x0000 }, /* R432 */
	{ 0x0000, 0x0000, 0x0000 }, /* R433 */
	{ 0x0000, 0x0000, 0x0000 }, /* R434 */
	{ 0x0000, 0x0000, 0x0000 }, /* R435 */
	{ 0x0000, 0x0000, 0x0000 }, /* R436 */
	{ 0x0000, 0x0000, 0x0000 }, /* R437 */
	{ 0x0000, 0x0000, 0x0000 }, /* R438 */
	{ 0x0000, 0x0000, 0x0000 }, /* R439 */
	{ 0x0000, 0x0000, 0x0000 }, /* R440 */
	{ 0x0000, 0x0000, 0x0000 }, /* R441 */
	{ 0x0000, 0x0000, 0x0000 }, /* R442 */
	{ 0x0000, 0x0000, 0x0000 }, /* R443 */
	{ 0x0000, 0x0000, 0x0000 }, /* R444 */
	{ 0x0000, 0x0000, 0x0000 }, /* R445 */
	{ 0x0000, 0x0000, 0x0000 }, /* R446 */
	{ 0x0000, 0x0000, 0x0000 }, /* R447 */
	{ 0x0000, 0x0000, 0x0000 }, /* R448 */
	{ 0x0000, 0x0000, 0x0000 }, /* R449 */
	{ 0x0000, 0x0000, 0x0000 }, /* R450 */
	{ 0x0000, 0x0000, 0x0000 }, /* R451 */
	{ 0x0000, 0x0000, 0x0000 }, /* R452 */
	{ 0x0000, 0x0000, 0x0000 }, /* R453 */
	{ 0x0000, 0x0000, 0x0000 }, /* R454 */
	{ 0x0000, 0x0000, 0x0000 }, /* R455 */
	{ 0x0000, 0x0000, 0x0000 }, /* R456 */
	{ 0x0000, 0x0000, 0x0000 }, /* R457 */
	{ 0x0000, 0x0000, 0x0000 }, /* R458 */
	{ 0x0000, 0x0000, 0x0000 }, /* R459 */
	{ 0x0000, 0x0000, 0x0000 }, /* R460 */
	{ 0x0000, 0x0000, 0x0000 }, /* R461 */
	{ 0x0000, 0x0000, 0x0000 }, /* R462 */
	{ 0x0000, 0x0000, 0x0000 }, /* R463 */
	{ 0x0000, 0x0000, 0x0000 }, /* R464 */
	{ 0x0000, 0x0000, 0x0000 }, /* R465 */
	{ 0x0000, 0x0000, 0x0000 }, /* R466 */
	{ 0x0000, 0x0000, 0x0000 }, /* R467 */
	{ 0x0000, 0x0000, 0x0000 }, /* R468 */
	{ 0x0000, 0x0000, 0x0000 }, /* R469 */
	{ 0x0000, 0x0000, 0x0000 }, /* R470 */
	{ 0x0000, 0x0000, 0x0000 }, /* R471 */
	{ 0x0000, 0x0000, 0x0000 }, /* R472 */
	{ 0x0000, 0x0000, 0x0000 }, /* R473 */
	{ 0x0000, 0x0000, 0x0000 }, /* R474 */
	{ 0x0000, 0x0000, 0x0000 }, /* R475 */
	{ 0x0000, 0x0000, 0x0000 }, /* R476 */
	{ 0x0000, 0x0000, 0x0000 }, /* R477 */
	{ 0x0000, 0x0000, 0x0000 }, /* R478 */
	{ 0x0000, 0x0000, 0x0000 }, /* R479 */
	{ 0x0000, 0x0000, 0x0000 }, /* R480 */
	{ 0x0000, 0x0000, 0x0000 }, /* R481 */
	{ 0x0000, 0x0000, 0x0000 }, /* R482 */
	{ 0x0000, 0x0000, 0x0000 }, /* R483 */
	{ 0x0000, 0x0000, 0x0000 }, /* R484 */
	{ 0x0000, 0x0000, 0x0000 }, /* R485 */
	{ 0x0000, 0x0000, 0x0000 }, /* R486 */
	{ 0x0000, 0x0000, 0x0000 }, /* R487 */
	{ 0x0000, 0x0000, 0x0000 }, /* R488 */
	{ 0x0000, 0x0000, 0x0000 }, /* R489 */
	{ 0x0000, 0x0000, 0x0000 }, /* R490 */
	{ 0x0000, 0x0000, 0x0000 }, /* R491 */
	{ 0x0000, 0x0000, 0x0000 }, /* R492 */
	{ 0x0000, 0x0000, 0x0000 }, /* R493 */
	{ 0x0000, 0x0000, 0x0000 }, /* R494 */
	{ 0x0000, 0x0000, 0x0000 }, /* R495 */
	{ 0x0000, 0x0000, 0x0000 }, /* R496 */
	{ 0x0000, 0x0000, 0x0000 }, /* R497 */
	{ 0x0000, 0x0000, 0x0000 }, /* R498 */
	{ 0x0000, 0x0000, 0x0000 }, /* R499 */
	{ 0x0000, 0x0000, 0x0000 }, /* R500 */
	{ 0x0000, 0x0000, 0x0000 }, /* R501 */
	{ 0x0000, 0x0000, 0x0000 }, /* R502 */
	{ 0x0000, 0x0000, 0x0000 }, /* R503 */
	{ 0x0000, 0x0000, 0x0000 }, /* R504 */
	{ 0x0000, 0x0000, 0x0000 }, /* R505 */
	{ 0x0000, 0x0000, 0x0000 }, /* R506 */
	{ 0x0000, 0x0000, 0x0000 }, /* R507 */
	{ 0x0000, 0x0000, 0x0000 }, /* R508 */
	{ 0x0000, 0x0000, 0x0000 }, /* R509 */
	{ 0x0000, 0x0000, 0x0000 }, /* R510 */
	{ 0x0000, 0x0000, 0x0000 }, /* R511 */
	{ 0x001F, 0x001F, 0x0000 }, /* R512   - AIF1 Clocking (1) */
	{ 0x003F, 0x003F, 0x0000 }, /* R513   - AIF1 Clocking (2) */
	{ 0x0000, 0x0000, 0x0000 }, /* R514 */
	{ 0x0000, 0x0000, 0x0000 }, /* R515 */
	{ 0x001F, 0x001F, 0x0000 }, /* R516   - AIF2 Clocking (1) */
	{ 0x003F, 0x003F, 0x0000 }, /* R517   - AIF2 Clocking (2) */
	{ 0x0000, 0x0000, 0x0000 }, /* R518 */
	{ 0x0000, 0x0000, 0x0000 }, /* R519 */
	{ 0x001F, 0x001F, 0x0000 }, /* R520   - Clocking (1) */
	{ 0x0777, 0x0777, 0x0000 }, /* R521   - Clocking (2) */
	{ 0x0000, 0x0000, 0x0000 }, /* R522 */
	{ 0x0000, 0x0000, 0x0000 }, /* R523 */
	{ 0x0000, 0x0000, 0x0000 }, /* R524 */
	{ 0x0000, 0x0000, 0x0000 }, /* R525 */
	{ 0x0000, 0x0000, 0x0000 }, /* R526 */
	{ 0x0000, 0x0000, 0x0000 }, /* R527 */
	{ 0x00FF, 0x00FF, 0x0000 }, /* R528   - AIF1 Rate */
	{ 0x00FF, 0x00FF, 0x0000 }, /* R529   - AIF2 Rate */
	{ 0x000F, 0x0000, 0x0000 }, /* R530   - Rate Status */
	{ 0x0000, 0x0000, 0x0000 }, /* R531 */
	{ 0x0000, 0x0000, 0x0000 }, /* R532 */
	{ 0x0000, 0x0000, 0x0000 }, /* R533 */
	{ 0x0000, 0x0000, 0x0000 }, /* R534 */
	{ 0x0000, 0x0000, 0x0000 }, /* R535 */
	{ 0x0000, 0x0000, 0x0000 }, /* R536 */
	{ 0x0000, 0x0000, 0x0000 }, /* R537 */
	{ 0x0000, 0x0000, 0x0000 }, /* R538 */
	{ 0x0000, 0x0000, 0x0000 }, /* R539 */
	{ 0x0000, 0x0000, 0x0000 }, /* R540 */
	{ 0x0000, 0x0000, 0x0000 }, /* R541 */
	{ 0x0000, 0x0000, 0x0000 }, /* R542 */
	{ 0x0000, 0x0000, 0x0000 }, /* R543 */
	{ 0x0007, 0x0007, 0x0000 }, /* R544   - FLL1 Control (1) */
	{ 0x3F77, 0x3F77, 0x0000 }, /* R545   - FLL1 Control (2) */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R546   - FLL1 Control (3) */
	{ 0x7FEF, 0x7FEF, 0x0000 }, /* R547   - FLL1 Control (4) */
	{ 0x1FDB, 0x1FDB, 0x0000 }, /* R548   - FLL1 Control (5) */
	{ 0x0000, 0x0000, 0x0000 }, /* R549 */
	{ 0x0000, 0x0000, 0x0000 }, /* R550 */
	{ 0x0000, 0x0000, 0x0000 }, /* R551 */
	{ 0x0000, 0x0000, 0x0000 }, /* R552 */
	{ 0x0000, 0x0000, 0x0000 }, /* R553 */
	{ 0x0000, 0x0000, 0x0000 }, /* R554 */
	{ 0x0000, 0x0000, 0x0000 }, /* R555 */
	{ 0x0000, 0x0000, 0x0000 }, /* R556 */
	{ 0x0000, 0x0000, 0x0000 }, /* R557 */
	{ 0x0000, 0x0000, 0x0000 }, /* R558 */
	{ 0x0000, 0x0000, 0x0000 }, /* R559 */
	{ 0x0000, 0x0000, 0x0000 }, /* R560 */
	{ 0x0000, 0x0000, 0x0000 }, /* R561 */
	{ 0x0000, 0x0000, 0x0000 }, /* R562 */
	{ 0x0000, 0x0000, 0x0000 }, /* R563 */
	{ 0x0000, 0x0000, 0x0000 }, /* R564 */
	{ 0x0000, 0x0000, 0x0000 }, /* R565 */
	{ 0x0000, 0x0000, 0x0000 }, /* R566 */
	{ 0x0000, 0x0000, 0x0000 }, /* R567 */
	{ 0x0000, 0x0000, 0x0000 }, /* R568 */
	{ 0x0000, 0x0000, 0x0000 }, /* R569 */
	{ 0x0000, 0x0000, 0x0000 }, /* R570 */
	{ 0x0000, 0x0000, 0x0000 }, /* R571 */
	{ 0x0000, 0x0000, 0x0000 }, /* R572 */
	{ 0x0000, 0x0000, 0x0000 }, /* R573 */
	{ 0x0000, 0x0000, 0x0000 }, /* R574 */
	{ 0x0000, 0x0000, 0x0000 }, /* R575 */
	{ 0x0007, 0x0007, 0x0000 }, /* R576   - FLL2 Control (1) */
	{ 0x3F77, 0x3F77, 0x0000 }, /* R577   - FLL2 Control (2) */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R578   - FLL2 Control (3) */
	{ 0x7FEF, 0x7FEF, 0x0000 }, /* R579   - FLL2 Control (4) */
	{ 0x1FDB, 0x1FDB, 0x0000 }, /* R580   - FLL2 Control (5) */
	{ 0x0000, 0x0000, 0x0000 }, /* R581 */
	{ 0x0000, 0x0000, 0x0000 }, /* R582 */
	{ 0x0000, 0x0000, 0x0000 }, /* R583 */
	{ 0x0000, 0x0000, 0x0000 }, /* R584 */
	{ 0x0000, 0x0000, 0x0000 }, /* R585 */
	{ 0x0000, 0x0000, 0x0000 }, /* R586 */
	{ 0x0000, 0x0000, 0x0000 }, /* R587 */
	{ 0x0000, 0x0000, 0x0000 }, /* R588 */
	{ 0x0000, 0x0000, 0x0000 }, /* R589 */
	{ 0x0000, 0x0000, 0x0000 }, /* R590 */
	{ 0x0000, 0x0000, 0x0000 }, /* R591 */
	{ 0x0000, 0x0000, 0x0000 }, /* R592 */
	{ 0x0000, 0x0000, 0x0000 }, /* R593 */
	{ 0x0000, 0x0000, 0x0000 }, /* R594 */
	{ 0x0000, 0x0000, 0x0000 }, /* R595 */
	{ 0x0000, 0x0000, 0x0000 }, /* R596 */
	{ 0x0000, 0x0000, 0x0000 }, /* R597 */
	{ 0x0000, 0x0000, 0x0000 }, /* R598 */
	{ 0x0000, 0x0000, 0x0000 }, /* R599 */
	{ 0x0000, 0x0000, 0x0000 }, /* R600 */
	{ 0x0000, 0x0000, 0x0000 }, /* R601 */
	{ 0x0000, 0x0000, 0x0000 }, /* R602 */
	{ 0x0000, 0x0000, 0x0000 }, /* R603 */
	{ 0x0000, 0x0000, 0x0000 }, /* R604 */
	{ 0x0000, 0x0000, 0x0000 }, /* R605 */
	{ 0x0000, 0x0000, 0x0000 }, /* R606 */
	{ 0x0000, 0x0000, 0x0000 }, /* R607 */
	{ 0x0000, 0x0000, 0x0000 }, /* R608 */
	{ 0x0000, 0x0000, 0x0000 }, /* R609 */
	{ 0x0000, 0x0000, 0x0000 }, /* R610 */
	{ 0x0000, 0x0000, 0x0000 }, /* R611 */
	{ 0x0000, 0x0000, 0x0000 }, /* R612 */
	{ 0x0000, 0x0000, 0x0000 }, /* R613 */
	{ 0x0000, 0x0000, 0x0000 }, /* R614 */
	{ 0x0000, 0x0000, 0x0000 }, /* R615 */
	{ 0x0000, 0x0000, 0x0000 }, /* R616 */
	{ 0x0000, 0x0000, 0x0000 }, /* R617 */
	{ 0x0000, 0x0000, 0x0000 }, /* R618 */
	{ 0x0000, 0x0000, 0x0000 }, /* R619 */
	{ 0x0000, 0x0000, 0x0000 }, /* R620 */
	{ 0x0000, 0x0000, 0x0000 }, /* R621 */
	{ 0x0000, 0x0000, 0x0000 }, /* R622 */
	{ 0x0000, 0x0000, 0x0000 }, /* R623 */
	{ 0x0000, 0x0000, 0x0000 }, /* R624 */
	{ 0x0000, 0x0000, 0x0000 }, /* R625 */
	{ 0x0000, 0x0000, 0x0000 }, /* R626 */
	{ 0x0000, 0x0000, 0x0000 }, /* R627 */
	{ 0x0000, 0x0000, 0x0000 }, /* R628 */
	{ 0x0000, 0x0000, 0x0000 }, /* R629 */
	{ 0x0000, 0x0000, 0x0000 }, /* R630 */
	{ 0x0000, 0x0000, 0x0000 }, /* R631 */
	{ 0x0000, 0x0000, 0x0000 }, /* R632 */
	{ 0x0000, 0x0000, 0x0000 }, /* R633 */
	{ 0x0000, 0x0000, 0x0000 }, /* R634 */
	{ 0x0000, 0x0000, 0x0000 }, /* R635 */
	{ 0x0000, 0x0000, 0x0000 }, /* R636 */
	{ 0x0000, 0x0000, 0x0000 }, /* R637 */
	{ 0x0000, 0x0000, 0x0000 }, /* R638 */
	{ 0x0000, 0x0000, 0x0000 }, /* R639 */
	{ 0x0000, 0x0000, 0x0000 }, /* R640 */
	{ 0x0000, 0x0000, 0x0000 }, /* R641 */
	{ 0x0000, 0x0000, 0x0000 }, /* R642 */
	{ 0x0000, 0x0000, 0x0000 }, /* R643 */
	{ 0x0000, 0x0000, 0x0000 }, /* R644 */
	{ 0x0000, 0x0000, 0x0000 }, /* R645 */
	{ 0x0000, 0x0000, 0x0000 }, /* R646 */
	{ 0x0000, 0x0000, 0x0000 }, /* R647 */
	{ 0x0000, 0x0000, 0x0000 }, /* R648 */
	{ 0x0000, 0x0000, 0x0000 }, /* R649 */
	{ 0x0000, 0x0000, 0x0000 }, /* R650 */
	{ 0x0000, 0x0000, 0x0000 }, /* R651 */
	{ 0x0000, 0x0000, 0x0000 }, /* R652 */
	{ 0x0000, 0x0000, 0x0000 }, /* R653 */
	{ 0x0000, 0x0000, 0x0000 }, /* R654 */
	{ 0x0000, 0x0000, 0x0000 }, /* R655 */
	{ 0x0000, 0x0000, 0x0000 }, /* R656 */
	{ 0x0000, 0x0000, 0x0000 }, /* R657 */
	{ 0x0000, 0x0000, 0x0000 }, /* R658 */
	{ 0x0000, 0x0000, 0x0000 }, /* R659 */
	{ 0x0000, 0x0000, 0x0000 }, /* R660 */
	{ 0x0000, 0x0000, 0x0000 }, /* R661 */
	{ 0x0000, 0x0000, 0x0000 }, /* R662 */
	{ 0x0000, 0x0000, 0x0000 }, /* R663 */
	{ 0x0000, 0x0000, 0x0000 }, /* R664 */
	{ 0x0000, 0x0000, 0x0000 }, /* R665 */
	{ 0x0000, 0x0000, 0x0000 }, /* R666 */
	{ 0x0000, 0x0000, 0x0000 }, /* R667 */
	{ 0x0000, 0x0000, 0x0000 }, /* R668 */
	{ 0x0000, 0x0000, 0x0000 }, /* R669 */
	{ 0x0000, 0x0000, 0x0000 }, /* R670 */
	{ 0x0000, 0x0000, 0x0000 }, /* R671 */
	{ 0x0000, 0x0000, 0x0000 }, /* R672 */
	{ 0x0000, 0x0000, 0x0000 }, /* R673 */
	{ 0x0000, 0x0000, 0x0000 }, /* R674 */
	{ 0x0000, 0x0000, 0x0000 }, /* R675 */
	{ 0x0000, 0x0000, 0x0000 }, /* R676 */
	{ 0x0000, 0x0000, 0x0000 }, /* R677 */
	{ 0x0000, 0x0000, 0x0000 }, /* R678 */
	{ 0x0000, 0x0000, 0x0000 }, /* R679 */
	{ 0x0000, 0x0000, 0x0000 }, /* R680 */
	{ 0x0000, 0x0000, 0x0000 }, /* R681 */
	{ 0x0000, 0x0000, 0x0000 }, /* R682 */
	{ 0x0000, 0x0000, 0x0000 }, /* R683 */
	{ 0x0000, 0x0000, 0x0000 }, /* R684 */
	{ 0x0000, 0x0000, 0x0000 }, /* R685 */
	{ 0x0000, 0x0000, 0x0000 }, /* R686 */
	{ 0x0000, 0x0000, 0x0000 }, /* R687 */
	{ 0x0000, 0x0000, 0x0000 }, /* R688 */
	{ 0x0000, 0x0000, 0x0000 }, /* R689 */
	{ 0x0000, 0x0000, 0x0000 }, /* R690 */
	{ 0x0000, 0x0000, 0x0000 }, /* R691 */
	{ 0x0000, 0x0000, 0x0000 }, /* R692 */
	{ 0x0000, 0x0000, 0x0000 }, /* R693 */
	{ 0x0000, 0x0000, 0x0000 }, /* R694 */
	{ 0x0000, 0x0000, 0x0000 }, /* R695 */
	{ 0x0000, 0x0000, 0x0000 }, /* R696 */
	{ 0x0000, 0x0000, 0x0000 }, /* R697 */
	{ 0x0000, 0x0000, 0x0000 }, /* R698 */
	{ 0x0000, 0x0000, 0x0000 }, /* R699 */
	{ 0x0000, 0x0000, 0x0000 }, /* R700 */
	{ 0x0000, 0x0000, 0x0000 }, /* R701 */
	{ 0x0000, 0x0000, 0x0000 }, /* R702 */
	{ 0x0000, 0x0000, 0x0000 }, /* R703 */
	{ 0x0000, 0x0000, 0x0000 }, /* R704 */
	{ 0x0000, 0x0000, 0x0000 }, /* R705 */
	{ 0x0000, 0x0000, 0x0000 }, /* R706 */
	{ 0x0000, 0x0000, 0x0000 }, /* R707 */
	{ 0x0000, 0x0000, 0x0000 }, /* R708 */
	{ 0x0000, 0x0000, 0x0000 }, /* R709 */
	{ 0x0000, 0x0000, 0x0000 }, /* R710 */
	{ 0x0000, 0x0000, 0x0000 }, /* R711 */
	{ 0x0000, 0x0000, 0x0000 }, /* R712 */
	{ 0x0000, 0x0000, 0x0000 }, /* R713 */
	{ 0x0000, 0x0000, 0x0000 }, /* R714 */
	{ 0x0000, 0x0000, 0x0000 }, /* R715 */
	{ 0x0000, 0x0000, 0x0000 }, /* R716 */
	{ 0x0000, 0x0000, 0x0000 }, /* R717 */
	{ 0x0000, 0x0000, 0x0000 }, /* R718 */
	{ 0x0000, 0x0000, 0x0000 }, /* R719 */
	{ 0x0000, 0x0000, 0x0000 }, /* R720 */
	{ 0x0000, 0x0000, 0x0000 }, /* R721 */
	{ 0x0000, 0x0000, 0x0000 }, /* R722 */
	{ 0x0000, 0x0000, 0x0000 }, /* R723 */
	{ 0x0000, 0x0000, 0x0000 }, /* R724 */
	{ 0x0000, 0x0000, 0x0000 }, /* R725 */
	{ 0x0000, 0x0000, 0x0000 }, /* R726 */
	{ 0x0000, 0x0000, 0x0000 }, /* R727 */
	{ 0x0000, 0x0000, 0x0000 }, /* R728 */
	{ 0x0000, 0x0000, 0x0000 }, /* R729 */
	{ 0x0000, 0x0000, 0x0000 }, /* R730 */
	{ 0x0000, 0x0000, 0x0000 }, /* R731 */
	{ 0x0000, 0x0000, 0x0000 }, /* R732 */
	{ 0x0000, 0x0000, 0x0000 }, /* R733 */
	{ 0x0000, 0x0000, 0x0000 }, /* R734 */
	{ 0x0000, 0x0000, 0x0000 }, /* R735 */
	{ 0x0000, 0x0000, 0x0000 }, /* R736 */
	{ 0x0000, 0x0000, 0x0000 }, /* R737 */
	{ 0x0000, 0x0000, 0x0000 }, /* R738 */
	{ 0x0000, 0x0000, 0x0000 }, /* R739 */
	{ 0x0000, 0x0000, 0x0000 }, /* R740 */
	{ 0x0000, 0x0000, 0x0000 }, /* R741 */
	{ 0x0000, 0x0000, 0x0000 }, /* R742 */
	{ 0x0000, 0x0000, 0x0000 }, /* R743 */
	{ 0x0000, 0x0000, 0x0000 }, /* R744 */
	{ 0x0000, 0x0000, 0x0000 }, /* R745 */
	{ 0x0000, 0x0000, 0x0000 }, /* R746 */
	{ 0x0000, 0x0000, 0x0000 }, /* R747 */
	{ 0x0000, 0x0000, 0x0000 }, /* R748 */
	{ 0x0000, 0x0000, 0x0000 }, /* R749 */
	{ 0x0000, 0x0000, 0x0000 }, /* R750 */
	{ 0x0000, 0x0000, 0x0000 }, /* R751 */
	{ 0x0000, 0x0000, 0x0000 }, /* R752 */
	{ 0x0000, 0x0000, 0x0000 }, /* R753 */
	{ 0x0000, 0x0000, 0x0000 }, /* R754 */
	{ 0x0000, 0x0000, 0x0000 }, /* R755 */
	{ 0x0000, 0x0000, 0x0000 }, /* R756 */
	{ 0x0000, 0x0000, 0x0000 }, /* R757 */
	{ 0x0000, 0x0000, 0x0000 }, /* R758 */
	{ 0x0000, 0x0000, 0x0000 }, /* R759 */
	{ 0x0000, 0x0000, 0x0000 }, /* R760 */
	{ 0x0000, 0x0000, 0x0000 }, /* R761 */
	{ 0x0000, 0x0000, 0x0000 }, /* R762 */
	{ 0x0000, 0x0000, 0x0000 }, /* R763 */
	{ 0x0000, 0x0000, 0x0000 }, /* R764 */
	{ 0x0000, 0x0000, 0x0000 }, /* R765 */
	{ 0x0000, 0x0000, 0x0000 }, /* R766 */
	{ 0x0000, 0x0000, 0x0000 }, /* R767 */
	{ 0xE1F8, 0xE1F8, 0x0000 }, /* R768   - AIF1 Control (1) */
	{ 0xCD1F, 0xCD1F, 0x0000 }, /* R769   - AIF1 Control (2) */
	{ 0xF000, 0xF000, 0x0000 }, /* R770   - AIF1 Master/Slave */
	{ 0x01F0, 0x01F0, 0x0000 }, /* R771   - AIF1 BCLK */
	{ 0x0FFF, 0x0FFF, 0x0000 }, /* R772   - AIF1ADC LRCLK */
	{ 0x0FFF, 0x0FFF, 0x0000 }, /* R773   - AIF1DAC LRCLK */
	{ 0x0003, 0x0003, 0x0000 }, /* R774   - AIF1DAC Data */
	{ 0x0003, 0x0003, 0x0000 }, /* R775   - AIF1ADC Data */
	{ 0x0000, 0x0000, 0x0000 }, /* R776 */
	{ 0x0000, 0x0000, 0x0000 }, /* R777 */
	{ 0x0000, 0x0000, 0x0000 }, /* R778 */
	{ 0x0000, 0x0000, 0x0000 }, /* R779 */
	{ 0x0000, 0x0000, 0x0000 }, /* R780 */
	{ 0x0000, 0x0000, 0x0000 }, /* R781 */
	{ 0x0000, 0x0000, 0x0000 }, /* R782 */
	{ 0x0000, 0x0000, 0x0000 }, /* R783 */
	{ 0xF1F8, 0xF1F8, 0x0000 }, /* R784   - AIF2 Control (1) */
	{ 0xFD1F, 0xFD1F, 0x0000 }, /* R785   - AIF2 Control (2) */
	{ 0xF000, 0xF000, 0x0000 }, /* R786   - AIF2 Master/Slave */
	{ 0x01F0, 0x01F0, 0x0000 }, /* R787   - AIF2 BCLK */
	{ 0x0FFF, 0x0FFF, 0x0000 }, /* R788   - AIF2ADC LRCLK */
	{ 0x0FFF, 0x0FFF, 0x0000 }, /* R789   - AIF2DAC LRCLK */
	{ 0x0003, 0x0003, 0x0000 }, /* R790   - AIF2DAC Data */
	{ 0x0003, 0x0003, 0x0000 }, /* R791   - AIF2ADC Data */
	{ 0x0000, 0x0000, 0x0000 }, /* R792 */
	{ 0x0000, 0x0000, 0x0000 }, /* R793 */
	{ 0x0000, 0x0000, 0x0000 }, /* R794 */
	{ 0x0000, 0x0000, 0x0000 }, /* R795 */
	{ 0x0000, 0x0000, 0x0000 }, /* R796 */
	{ 0x0000, 0x0000, 0x0000 }, /* R797 */
	{ 0x0000, 0x0000, 0x0000 }, /* R798 */
	{ 0x0000, 0x0000, 0x0000 }, /* R799 */
	{ 0x0000, 0x0000, 0x0000 }, /* R800 */
	{ 0x0000, 0x0000, 0x0000 }, /* R801 */
	{ 0x0000, 0x0000, 0x0000 }, /* R802 */
	{ 0x0000, 0x0000, 0x0000 }, /* R803 */
	{ 0x0000, 0x0000, 0x0000 }, /* R804 */
	{ 0x0000, 0x0000, 0x0000 }, /* R805 */
	{ 0x0000, 0x0000, 0x0000 }, /* R806 */
	{ 0x0000, 0x0000, 0x0000 }, /* R807 */
	{ 0x0000, 0x0000, 0x0000 }, /* R808 */
	{ 0x0000, 0x0000, 0x0000 }, /* R809 */
	{ 0x0000, 0x0000, 0x0000 }, /* R810 */
	{ 0x0000, 0x0000, 0x0000 }, /* R811 */
	{ 0x0000, 0x0000, 0x0000 }, /* R812 */
	{ 0x0000, 0x0000, 0x0000 }, /* R813 */
	{ 0x0000, 0x0000, 0x0000 }, /* R814 */
	{ 0x0000, 0x0000, 0x0000 }, /* R815 */
	{ 0x0000, 0x0000, 0x0000 }, /* R816 */
	{ 0x0000, 0x0000, 0x0000 }, /* R817 */
	{ 0x0000, 0x0000, 0x0000 }, /* R818 */
	{ 0x0000, 0x0000, 0x0000 }, /* R819 */
	{ 0x0000, 0x0000, 0x0000 }, /* R820 */
	{ 0x0000, 0x0000, 0x0000 }, /* R821 */
	{ 0x0000, 0x0000, 0x0000 }, /* R822 */
	{ 0x0000, 0x0000, 0x0000 }, /* R823 */
	{ 0x0000, 0x0000, 0x0000 }, /* R824 */
	{ 0x0000, 0x0000, 0x0000 }, /* R825 */
	{ 0x0000, 0x0000, 0x0000 }, /* R826 */
	{ 0x0000, 0x0000, 0x0000 }, /* R827 */
	{ 0x0000, 0x0000, 0x0000 }, /* R828 */
	{ 0x0000, 0x0000, 0x0000 }, /* R829 */
	{ 0x0000, 0x0000, 0x0000 }, /* R830 */
	{ 0x0000, 0x0000, 0x0000 }, /* R831 */
	{ 0x0000, 0x0000, 0x0000 }, /* R832 */
	{ 0x0000, 0x0000, 0x0000 }, /* R833 */
	{ 0x0000, 0x0000, 0x0000 }, /* R834 */
	{ 0x0000, 0x0000, 0x0000 }, /* R835 */
	{ 0x0000, 0x0000, 0x0000 }, /* R836 */
	{ 0x0000, 0x0000, 0x0000 }, /* R837 */
	{ 0x0000, 0x0000, 0x0000 }, /* R838 */
	{ 0x0000, 0x0000, 0x0000 }, /* R839 */
	{ 0x0000, 0x0000, 0x0000 }, /* R840 */
	{ 0x0000, 0x0000, 0x0000 }, /* R841 */
	{ 0x0000, 0x0000, 0x0000 }, /* R842 */
	{ 0x0000, 0x0000, 0x0000 }, /* R843 */
	{ 0x0000, 0x0000, 0x0000 }, /* R844 */
	{ 0x0000, 0x0000, 0x0000 }, /* R845 */
	{ 0x0000, 0x0000, 0x0000 }, /* R846 */
	{ 0x0000, 0x0000, 0x0000 }, /* R847 */
	{ 0x0000, 0x0000, 0x0000 }, /* R848 */
	{ 0x0000, 0x0000, 0x0000 }, /* R849 */
	{ 0x0000, 0x0000, 0x0000 }, /* R850 */
	{ 0x0000, 0x0000, 0x0000 }, /* R851 */
	{ 0x0000, 0x0000, 0x0000 }, /* R852 */
	{ 0x0000, 0x0000, 0x0000 }, /* R853 */
	{ 0x0000, 0x0000, 0x0000 }, /* R854 */
	{ 0x0000, 0x0000, 0x0000 }, /* R855 */
	{ 0x0000, 0x0000, 0x0000 }, /* R856 */
	{ 0x0000, 0x0000, 0x0000 }, /* R857 */
	{ 0x0000, 0x0000, 0x0000 }, /* R858 */
	{ 0x0000, 0x0000, 0x0000 }, /* R859 */
	{ 0x0000, 0x0000, 0x0000 }, /* R860 */
	{ 0x0000, 0x0000, 0x0000 }, /* R861 */
	{ 0x0000, 0x0000, 0x0000 }, /* R862 */
	{ 0x0000, 0x0000, 0x0000 }, /* R863 */
	{ 0x0000, 0x0000, 0x0000 }, /* R864 */
	{ 0x0000, 0x0000, 0x0000 }, /* R865 */
	{ 0x0000, 0x0000, 0x0000 }, /* R866 */
	{ 0x0000, 0x0000, 0x0000 }, /* R867 */
	{ 0x0000, 0x0000, 0x0000 }, /* R868 */
	{ 0x0000, 0x0000, 0x0000 }, /* R869 */
	{ 0x0000, 0x0000, 0x0000 }, /* R870 */
	{ 0x0000, 0x0000, 0x0000 }, /* R871 */
	{ 0x0000, 0x0000, 0x0000 }, /* R872 */
	{ 0x0000, 0x0000, 0x0000 }, /* R873 */
	{ 0x0000, 0x0000, 0x0000 }, /* R874 */
	{ 0x0000, 0x0000, 0x0000 }, /* R875 */
	{ 0x0000, 0x0000, 0x0000 }, /* R876 */
	{ 0x0000, 0x0000, 0x0000 }, /* R877 */
	{ 0x0000, 0x0000, 0x0000 }, /* R878 */
	{ 0x0000, 0x0000, 0x0000 }, /* R879 */
	{ 0x0000, 0x0000, 0x0000 }, /* R880 */
	{ 0x0000, 0x0000, 0x0000 }, /* R881 */
	{ 0x0000, 0x0000, 0x0000 }, /* R882 */
	{ 0x0000, 0x0000, 0x0000 }, /* R883 */
	{ 0x0000, 0x0000, 0x0000 }, /* R884 */
	{ 0x0000, 0x0000, 0x0000 }, /* R885 */
	{ 0x0000, 0x0000, 0x0000 }, /* R886 */
	{ 0x0000, 0x0000, 0x0000 }, /* R887 */
	{ 0x0000, 0x0000, 0x0000 }, /* R888 */
	{ 0x0000, 0x0000, 0x0000 }, /* R889 */
	{ 0x0000, 0x0000, 0x0000 }, /* R890 */
	{ 0x0000, 0x0000, 0x0000 }, /* R891 */
	{ 0x0000, 0x0000, 0x0000 }, /* R892 */
	{ 0x0000, 0x0000, 0x0000 }, /* R893 */
	{ 0x0000, 0x0000, 0x0000 }, /* R894 */
	{ 0x0000, 0x0000, 0x0000 }, /* R895 */
	{ 0x0000, 0x0000, 0x0000 }, /* R896 */
	{ 0x0000, 0x0000, 0x0000 }, /* R897 */
	{ 0x0000, 0x0000, 0x0000 }, /* R898 */
	{ 0x0000, 0x0000, 0x0000 }, /* R899 */
	{ 0x0000, 0x0000, 0x0000 }, /* R900 */
	{ 0x0000, 0x0000, 0x0000 }, /* R901 */
	{ 0x0000, 0x0000, 0x0000 }, /* R902 */
	{ 0x0000, 0x0000, 0x0000 }, /* R903 */
	{ 0x0000, 0x0000, 0x0000 }, /* R904 */
	{ 0x0000, 0x0000, 0x0000 }, /* R905 */
	{ 0x0000, 0x0000, 0x0000 }, /* R906 */
	{ 0x0000, 0x0000, 0x0000 }, /* R907 */
	{ 0x0000, 0x0000, 0x0000 }, /* R908 */
	{ 0x0000, 0x0000, 0x0000 }, /* R909 */
	{ 0x0000, 0x0000, 0x0000 }, /* R910 */
	{ 0x0000, 0x0000, 0x0000 }, /* R911 */
	{ 0x0000, 0x0000, 0x0000 }, /* R912 */
	{ 0x0000, 0x0000, 0x0000 }, /* R913 */
	{ 0x0000, 0x0000, 0x0000 }, /* R914 */
	{ 0x0000, 0x0000, 0x0000 }, /* R915 */
	{ 0x0000, 0x0000, 0x0000 }, /* R916 */
	{ 0x0000, 0x0000, 0x0000 }, /* R917 */
	{ 0x0000, 0x0000, 0x0000 }, /* R918 */
	{ 0x0000, 0x0000, 0x0000 }, /* R919 */
	{ 0x0000, 0x0000, 0x0000 }, /* R920 */
	{ 0x0000, 0x0000, 0x0000 }, /* R921 */
	{ 0x0000, 0x0000, 0x0000 }, /* R922 */
	{ 0x0000, 0x0000, 0x0000 }, /* R923 */
	{ 0x0000, 0x0000, 0x0000 }, /* R924 */
	{ 0x0000, 0x0000, 0x0000 }, /* R925 */
	{ 0x0000, 0x0000, 0x0000 }, /* R926 */
	{ 0x0000, 0x0000, 0x0000 }, /* R927 */
	{ 0x0000, 0x0000, 0x0000 }, /* R928 */
	{ 0x0000, 0x0000, 0x0000 }, /* R929 */
	{ 0x0000, 0x0000, 0x0000 }, /* R930 */
	{ 0x0000, 0x0000, 0x0000 }, /* R931 */
	{ 0x0000, 0x0000, 0x0000 }, /* R932 */
	{ 0x0000, 0x0000, 0x0000 }, /* R933 */
	{ 0x0000, 0x0000, 0x0000 }, /* R934 */
	{ 0x0000, 0x0000, 0x0000 }, /* R935 */
	{ 0x0000, 0x0000, 0x0000 }, /* R936 */
	{ 0x0000, 0x0000, 0x0000 }, /* R937 */
	{ 0x0000, 0x0000, 0x0000 }, /* R938 */
	{ 0x0000, 0x0000, 0x0000 }, /* R939 */
	{ 0x0000, 0x0000, 0x0000 }, /* R940 */
	{ 0x0000, 0x0000, 0x0000 }, /* R941 */
	{ 0x0000, 0x0000, 0x0000 }, /* R942 */
	{ 0x0000, 0x0000, 0x0000 }, /* R943 */
	{ 0x0000, 0x0000, 0x0000 }, /* R944 */
	{ 0x0000, 0x0000, 0x0000 }, /* R945 */
	{ 0x0000, 0x0000, 0x0000 }, /* R946 */
	{ 0x0000, 0x0000, 0x0000 }, /* R947 */
	{ 0x0000, 0x0000, 0x0000 }, /* R948 */
	{ 0x0000, 0x0000, 0x0000 }, /* R949 */
	{ 0x0000, 0x0000, 0x0000 }, /* R950 */
	{ 0x0000, 0x0000, 0x0000 }, /* R951 */
	{ 0x0000, 0x0000, 0x0000 }, /* R952 */
	{ 0x0000, 0x0000, 0x0000 }, /* R953 */
	{ 0x0000, 0x0000, 0x0000 }, /* R954 */
	{ 0x0000, 0x0000, 0x0000 }, /* R955 */
	{ 0x0000, 0x0000, 0x0000 }, /* R956 */
	{ 0x0000, 0x0000, 0x0000 }, /* R957 */
	{ 0x0000, 0x0000, 0x0000 }, /* R958 */
	{ 0x0000, 0x0000, 0x0000 }, /* R959 */
	{ 0x0000, 0x0000, 0x0000 }, /* R960 */
	{ 0x0000, 0x0000, 0x0000 }, /* R961 */
	{ 0x0000, 0x0000, 0x0000 }, /* R962 */
	{ 0x0000, 0x0000, 0x0000 }, /* R963 */
	{ 0x0000, 0x0000, 0x0000 }, /* R964 */
	{ 0x0000, 0x0000, 0x0000 }, /* R965 */
	{ 0x0000, 0x0000, 0x0000 }, /* R966 */
	{ 0x0000, 0x0000, 0x0000 }, /* R967 */
	{ 0x0000, 0x0000, 0x0000 }, /* R968 */
	{ 0x0000, 0x0000, 0x0000 }, /* R969 */
	{ 0x0000, 0x0000, 0x0000 }, /* R970 */
	{ 0x0000, 0x0000, 0x0000 }, /* R971 */
	{ 0x0000, 0x0000, 0x0000 }, /* R972 */
	{ 0x0000, 0x0000, 0x0000 }, /* R973 */
	{ 0x0000, 0x0000, 0x0000 }, /* R974 */
	{ 0x0000, 0x0000, 0x0000 }, /* R975 */
	{ 0x0000, 0x0000, 0x0000 }, /* R976 */
	{ 0x0000, 0x0000, 0x0000 }, /* R977 */
	{ 0x0000, 0x0000, 0x0000 }, /* R978 */
	{ 0x0000, 0x0000, 0x0000 }, /* R979 */
	{ 0x0000, 0x0000, 0x0000 }, /* R980 */
	{ 0x0000, 0x0000, 0x0000 }, /* R981 */
	{ 0x0000, 0x0000, 0x0000 }, /* R982 */
	{ 0x0000, 0x0000, 0x0000 }, /* R983 */
	{ 0x0000, 0x0000, 0x0000 }, /* R984 */
	{ 0x0000, 0x0000, 0x0000 }, /* R985 */
	{ 0x0000, 0x0000, 0x0000 }, /* R986 */
	{ 0x0000, 0x0000, 0x0000 }, /* R987 */
	{ 0x0000, 0x0000, 0x0000 }, /* R988 */
	{ 0x0000, 0x0000, 0x0000 }, /* R989 */
	{ 0x0000, 0x0000, 0x0000 }, /* R990 */
	{ 0x0000, 0x0000, 0x0000 }, /* R991 */
	{ 0x0000, 0x0000, 0x0000 }, /* R992 */
	{ 0x0000, 0x0000, 0x0000 }, /* R993 */
	{ 0x0000, 0x0000, 0x0000 }, /* R994 */
	{ 0x0000, 0x0000, 0x0000 }, /* R995 */
	{ 0x0000, 0x0000, 0x0000 }, /* R996 */
	{ 0x0000, 0x0000, 0x0000 }, /* R997 */
	{ 0x0000, 0x0000, 0x0000 }, /* R998 */
	{ 0x0000, 0x0000, 0x0000 }, /* R999 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1000 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1001 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1002 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1003 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1004 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1005 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1006 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1007 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1008 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1009 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1010 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1011 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1012 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1013 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1014 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1015 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1016 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1017 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1018 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1019 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1020 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1021 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1022 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1023 */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R1024  - AIF1 ADC1 Left Volume */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R1025  - AIF1 ADC1 Right Volume */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R1026  - AIF1 DAC1 Left Volume */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R1027  - AIF1 DAC1 Right Volume */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R1028  - AIF1 ADC2 Left Volume */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R1029  - AIF1 ADC2 Right Volume */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R1030  - AIF1 DAC2 Left Volume */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R1031  - AIF1 DAC2 Right Volume */
	{ 0x0000, 0x0000, 0x0000 }, /* R1032 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1033 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1034 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1035 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1036 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1037 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1038 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1039 */
	{ 0xF800, 0xF800, 0x0000 }, /* R1040  - AIF1 ADC1 Filters */
	{ 0x7800, 0x7800, 0x0000 }, /* R1041  - AIF1 ADC2 Filters */
	{ 0x0000, 0x0000, 0x0000 }, /* R1042 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1043 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1044 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1045 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1046 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1047 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1048 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1049 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1050 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1051 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1052 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1053 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1054 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1055 */
	{ 0x02B6, 0x02B6, 0x0000 }, /* R1056  - AIF1 DAC1 Filters (1) */
	{ 0x3F00, 0x3F00, 0x0000 }, /* R1057  - AIF1 DAC1 Filters (2) */
	{ 0x02B6, 0x02B6, 0x0000 }, /* R1058  - AIF1 DAC2 Filters (1) */
	{ 0x3F00, 0x3F00, 0x0000 }, /* R1059  - AIF1 DAC2 Filters (2) */
	{ 0x0000, 0x0000, 0x0000 }, /* R1060 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1061 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1062 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1063 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1064 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1065 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1066 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1067 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1068 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1069 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1070 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1071 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1072 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1073 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1074 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1075 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1076 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1077 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1078 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1079 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1080 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1081 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1082 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1083 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1084 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1085 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1086 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1087 */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1088  - AIF1 DRC1 (1) */
	{ 0x1FFF, 0x1FFF, 0x0000 }, /* R1089  - AIF1 DRC1 (2) */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1090  - AIF1 DRC1 (3) */
	{ 0x07FF, 0x07FF, 0x0000 }, /* R1091  - AIF1 DRC1 (4) */
	{ 0x03FF, 0x03FF, 0x0000 }, /* R1092  - AIF1 DRC1 (5) */
	{ 0x0000, 0x0000, 0x0000 }, /* R1093 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1094 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1095 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1096 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1097 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1098 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1099 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1100 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1101 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1102 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1103 */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1104  - AIF1 DRC2 (1) */
	{ 0x1FFF, 0x1FFF, 0x0000 }, /* R1105  - AIF1 DRC2 (2) */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1106  - AIF1 DRC2 (3) */
	{ 0x07FF, 0x07FF, 0x0000 }, /* R1107  - AIF1 DRC2 (4) */
	{ 0x03FF, 0x03FF, 0x0000 }, /* R1108  - AIF1 DRC2 (5) */
	{ 0x0000, 0x0000, 0x0000 }, /* R1109 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1110 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1111 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1112 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1113 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1114 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1115 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1116 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1117 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1118 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1119 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1120 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1121 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1122 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1123 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1124 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1125 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1126 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1127 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1128 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1129 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1130 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1131 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1132 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1133 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1134 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1135 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1136 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1137 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1138 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1139 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1140 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1141 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1142 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1143 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1144 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1145 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1146 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1147 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1148 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1149 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1150 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1151 */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1152  - AIF1 DAC1 EQ Gains (1) */
	{ 0xFFC0, 0xFFC0, 0x0000 }, /* R1153  - AIF1 DAC1 EQ Gains (2) */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1154  - AIF1 DAC1 EQ Band 1 A */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1155  - AIF1 DAC1 EQ Band 1 B */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1156  - AIF1 DAC1 EQ Band 1 PG */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1157  - AIF1 DAC1 EQ Band 2 A */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1158  - AIF1 DAC1 EQ Band 2 B */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1159  - AIF1 DAC1 EQ Band 2 C */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1160  - AIF1 DAC1 EQ Band 2 PG */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1161  - AIF1 DAC1 EQ Band 3 A */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1162  - AIF1 DAC1 EQ Band 3 B */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1163  - AIF1 DAC1 EQ Band 3 C */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1164  - AIF1 DAC1 EQ Band 3 PG */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1165  - AIF1 DAC1 EQ Band 4 A */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1166  - AIF1 DAC1 EQ Band 4 B */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1167  - AIF1 DAC1 EQ Band 4 C */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1168  - AIF1 DAC1 EQ Band 4 PG */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1169  - AIF1 DAC1 EQ Band 5 A */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1170  - AIF1 DAC1 EQ Band 5 B */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1171  - AIF1 DAC1 EQ Band 5 PG */
	{ 0x0000, 0x0000, 0x0000 }, /* R1172 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1173 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1174 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1175 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1176 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1177 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1178 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1179 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1180 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1181 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1182 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1183 */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1184  - AIF1 DAC2 EQ Gains (1) */
	{ 0xFFC0, 0xFFC0, 0x0000 }, /* R1185  - AIF1 DAC2 EQ Gains (2) */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1186  - AIF1 DAC2 EQ Band 1 A */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1187  - AIF1 DAC2 EQ Band 1 B */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1188  - AIF1 DAC2 EQ Band 1 PG */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1189  - AIF1 DAC2 EQ Band 2 A */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1190  - AIF1 DAC2 EQ Band 2 B */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1191  - AIF1 DAC2 EQ Band 2 C */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1192  - AIF1 DAC2 EQ Band 2 PG */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1193  - AIF1 DAC2 EQ Band 3 A */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1194  - AIF1 DAC2 EQ Band 3 B */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1195  - AIF1 DAC2 EQ Band 3 C */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1196  - AIF1 DAC2 EQ Band 3 PG */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1197  - AIF1 DAC2 EQ Band 4 A */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1198  - AIF1 DAC2 EQ Band 4 B */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1199  - AIF1 DAC2 EQ Band 4 C */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1200  - AIF1 DAC2 EQ Band 4 PG */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1201  - AIF1 DAC2 EQ Band 5 A */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1202  - AIF1 DAC2 EQ Band 5 B */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1203  - AIF1 DAC2 EQ Band 5 PG */
	{ 0x0000, 0x0000, 0x0000 }, /* R1204 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1205 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1206 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1207 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1208 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1209 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1210 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1211 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1212 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1213 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1214 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1215 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1216 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1217 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1218 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1219 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1220 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1221 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1222 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1223 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1224 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1225 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1226 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1227 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1228 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1229 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1230 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1231 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1232 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1233 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1234 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1235 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1236 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1237 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1238 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1239 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1240 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1241 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1242 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1243 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1244 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1245 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1246 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1247 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1248 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1249 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1250 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1251 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1252 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1253 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1254 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1255 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1256 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1257 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1258 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1259 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1260 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1261 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1262 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1263 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1264 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1265 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1266 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1267 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1268 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1269 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1270 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1271 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1272 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1273 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1274 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1275 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1276 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1277 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1278 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1279 */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R1280  - AIF2 ADC Left Volume */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R1281  - AIF2 ADC Right Volume */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R1282  - AIF2 DAC Left Volume */
	{ 0x00FF, 0x01FF, 0x0000 }, /* R1283  - AIF2 DAC Right Volume */
	{ 0x0000, 0x0000, 0x0000 }, /* R1284 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1285 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1286 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1287 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1288 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1289 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1290 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1291 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1292 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1293 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1294 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1295 */
	{ 0xF800, 0xF800, 0x0000 }, /* R1296  - AIF2 ADC Filters */
	{ 0x0000, 0x0000, 0x0000 }, /* R1297 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1298 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1299 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1300 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1301 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1302 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1303 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1304 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1305 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1306 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1307 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1308 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1309 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1310 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1311 */
	{ 0x02B6, 0x02B6, 0x0000 }, /* R1312  - AIF2 DAC Filters (1) */
	{ 0x3F00, 0x3F00, 0x0000 }, /* R1313  - AIF2 DAC Filters (2) */
	{ 0x0000, 0x0000, 0x0000 }, /* R1314 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1315 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1316 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1317 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1318 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1319 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1320 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1321 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1322 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1323 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1324 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1325 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1326 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1327 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1328 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1329 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1330 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1331 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1332 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1333 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1334 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1335 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1336 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1337 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1338 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1339 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1340 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1341 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1342 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1343 */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1344  - AIF2 DRC (1) */
	{ 0x1FFF, 0x1FFF, 0x0000 }, /* R1345  - AIF2 DRC (2) */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1346  - AIF2 DRC (3) */
	{ 0x07FF, 0x07FF, 0x0000 }, /* R1347  - AIF2 DRC (4) */
	{ 0x03FF, 0x03FF, 0x0000 }, /* R1348  - AIF2 DRC (5) */
	{ 0x0000, 0x0000, 0x0000 }, /* R1349 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1350 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1351 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1352 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1353 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1354 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1355 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1356 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1357 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1358 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1359 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1360 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1361 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1362 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1363 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1364 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1365 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1366 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1367 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1368 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1369 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1370 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1371 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1372 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1373 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1374 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1375 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1376 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1377 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1378 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1379 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1380 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1381 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1382 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1383 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1384 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1385 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1386 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1387 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1388 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1389 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1390 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1391 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1392 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1393 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1394 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1395 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1396 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1397 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1398 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1399 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1400 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1401 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1402 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1403 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1404 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1405 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1406 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1407 */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1408  - AIF2 EQ Gains (1) */
	{ 0xFFC0, 0xFFC0, 0x0000 }, /* R1409  - AIF2 EQ Gains (2) */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1410  - AIF2 EQ Band 1 A */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1411  - AIF2 EQ Band 1 B */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1412  - AIF2 EQ Band 1 PG */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1413  - AIF2 EQ Band 2 A */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1414  - AIF2 EQ Band 2 B */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1415  - AIF2 EQ Band 2 C */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1416  - AIF2 EQ Band 2 PG */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1417  - AIF2 EQ Band 3 A */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1418  - AIF2 EQ Band 3 B */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1419  - AIF2 EQ Band 3 C */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1420  - AIF2 EQ Band 3 PG */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1421  - AIF2 EQ Band 4 A */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1422  - AIF2 EQ Band 4 B */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1423  - AIF2 EQ Band 4 C */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1424  - AIF2 EQ Band 4 PG */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1425  - AIF2 EQ Band 5 A */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1426  - AIF2 EQ Band 5 B */
	{ 0xFFFF, 0xFFFF, 0x0000 }, /* R1427  - AIF2 EQ Band 5 PG */
	{ 0x0000, 0x0000, 0x0000 }, /* R1428 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1429 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1430 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1431 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1432 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1433 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1434 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1435 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1436 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1437 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1438 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1439 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1440 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1441 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1442 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1443 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1444 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1445 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1446 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1447 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1448 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1449 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1450 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1451 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1452 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1453 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1454 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1455 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1456 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1457 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1458 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1459 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1460 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1461 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1462 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1463 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1464 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1465 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1466 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1467 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1468 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1469 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1470 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1471 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1472 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1473 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1474 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1475 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1476 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1477 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1478 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1479 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1480 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1481 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1482 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1483 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1484 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1485 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1486 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1487 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1488 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1489 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1490 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1491 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1492 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1493 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1494 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1495 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1496 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1497 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1498 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1499 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1500 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1501 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1502 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1503 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1504 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1505 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1506 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1507 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1508 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1509 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1510 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1511 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1512 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1513 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1514 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1515 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1516 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1517 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1518 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1519 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1520 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1521 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1522 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1523 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1524 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1525 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1526 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1527 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1528 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1529 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1530 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1531 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1532 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1533 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1534 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1535 */
	{ 0x01EF, 0x01EF, 0x0000 }, /* R1536  - DAC1 Mixer Volumes */
	{ 0x0037, 0x0037, 0x0000 }, /* R1537  - DAC1 Left Mixer Routing */
	{ 0x0037, 0x0037, 0x0000 }, /* R1538  - DAC1 Right Mixer Routing */
	{ 0x01EF, 0x01EF, 0x0000 }, /* R1539  - DAC2 Mixer Volumes */
	{ 0x0037, 0x0037, 0x0000 }, /* R1540  - DAC2 Left Mixer Routing */
	{ 0x0037, 0x0037, 0x0000 }, /* R1541  - DAC2 Right Mixer Routing */
	{ 0x0003, 0x0003, 0x0000 }, /* R1542  - AIF1 ADC1 Left Mixer Routing */
	{ 0x0003, 0x0003, 0x0000 }, /* R1543  - AIF1 ADC1 Right Mixer Routing */
	{ 0x0003, 0x0003, 0x0000 }, /* R1544  - AIF1 ADC2 Left Mixer Routing */
	{ 0x0003, 0x0003, 0x0000 }, /* R1545  - AIF1 ADC2 Right mixer Routing */
	{ 0x0000, 0x0000, 0x0000 }, /* R1546 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1547 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1548 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1549 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1550 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1551 */
	{ 0x02FF, 0x03FF, 0x0000 }, /* R1552  - DAC1 Left Volume */
	{ 0x02FF, 0x03FF, 0x0000 }, /* R1553  - DAC1 Right Volume */
	{ 0x02FF, 0x03FF, 0x0000 }, /* R1554  - DAC2 Left Volume */
	{ 0x02FF, 0x03FF, 0x0000 }, /* R1555  - DAC2 Right Volume */
	{ 0x0003, 0x0003, 0x0000 }, /* R1556  - DAC Softmute */
	{ 0x0000, 0x0000, 0x0000 }, /* R1557 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1558 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1559 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1560 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1561 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1562 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1563 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1564 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1565 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1566 */
	{ 0x0000, 0x0000, 0x0000 }, /* R1567 */
	{ 0x0003, 0x0003, 0x0000 }, /* R1568  - Oversampling */
	{ 0x03C3, 0x03C3, 0x0000 }, /* R1569  - Sidetone */
};

static int wm8994_readable(unsigned int reg)
{

        switch (reg) {
        case WM8994_GPIO_1:
        case WM8994_GPIO_2:
        case WM8994_GPIO_3:
        case WM8994_GPIO_4:
        case WM8994_GPIO_5:
        case WM8994_GPIO_6:
        case WM8994_GPIO_7:
        case WM8994_GPIO_8:
        case WM8994_GPIO_9:
        case WM8994_GPIO_10:
        case WM8994_GPIO_11:
        case WM8994_INTERRUPT_STATUS_1:
        case WM8994_INTERRUPT_STATUS_2:
        case WM8994_INTERRUPT_STATUS_1_MASK:
        case WM8994_INTERRUPT_STATUS_2_MASK:
        case WM8994_INTERRUPT_CONTROL:
        case WM8994_INTERRUPT_RAW_STATUS_2:
                return 1;
	}

	if (reg >= ARRAY_SIZE(access_masks))
		return 0;
	return access_masks[reg].readable != 0;
}


/* returns the minimum number of bytes needed to represent
 * a particular given value */
static int min_bytes_needed(unsigned long val)
{
	int c = 0;
	int i;

	for (i = (sizeof val * 8) - 1; i >= 0; --i, ++c)
		if (val & (1UL << i))
			break;
	c = (sizeof val * 8) - c;
	if (!c || (c % 8))
		c = (c + 8) / 8;
	else
		c /= 8;
	return c;
}

/* fill buf which is 'len' bytes with a formatted
 * string of the form 'reg: value\n' */
static int format_register_str(unsigned int reg, char *buf, size_t len)
{
	int wordsize = min_bytes_needed(WM8994_MAX_REGISTER) * 2;
	int regsize = 2 * 2;
	int ret;
	char tmpbuf[len + 1];
	char regbuf[regsize + 1];

	struct wm8994 *amp = wm8994_amp;

	/* since tmpbuf is allocated on the stack, warn the callers if they
	 * try to abuse this function */
	WARN_ON(len > 63);

	/* +2 for ': ' and + 1 for '\n' */
	if (wordsize + regsize + 2 + 1 != len)
		return -EINVAL;

	ret = wm8994_reg_read(amp, reg);
	if (ret < 0) {
		memset(regbuf, 'X', regsize);
		regbuf[regsize] = '\0';
	} else {
		snprintf(regbuf, regsize + 1, "%.*x", regsize, ret);
	}

	/* prepare the buffer */
	snprintf(tmpbuf, len + 1, "%.*x: %s\n", wordsize, reg, regbuf);
	/* copy it back to the caller without the '\0' */
	memcpy(buf, tmpbuf, len);

	return 0;
}

/* codec register dump */
static ssize_t soc_codec_reg_show(char *buf,
				  size_t count, loff_t pos)
{
	int i, step = 1;
	int wordsize, regsize;
	int len;
	size_t total = 0;
	loff_t p = 0;

	wordsize = min_bytes_needed(WM8994_MAX_REGISTER) * 2;
	regsize = 2 * 2;

	len = wordsize + regsize + 2 + 1;

	for (i = 0; i < WM8994_MAX_REGISTER; i++) {
		/* only support larger than PAGE_SIZE bytes debugfs
		 * entries for the default case */
		if(!wm8994_readable(i)) continue;
		if (p >= pos) {
			if (total + len >= count - 1)
				break;
			format_register_str(i, buf + total, len);
			total += len;
		}
		p += len;
	}

	total = min(total, count - 1);

	return total;
}

static int codec_reg_open_file(struct inode *inode, struct file *file)
{
        file->private_data = inode->i_private;
        return 0;
}

static ssize_t codec_reg_read_file(struct file *file, char __user *user_buf,
                               size_t count, loff_t *ppos)
{
	ssize_t ret;
	char *buf;

	if (*ppos < 0 || !count)
		return -EINVAL;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = soc_codec_reg_show(buf, count, *ppos);
	if (ret >= 0) {
		if (copy_to_user(user_buf, buf, ret)) {
			kfree(buf);
			return -EFAULT;
		}
		*ppos += ret;
	}

	kfree(buf);
	return ret;
}

static ssize_t codec_reg_write_file(struct file *file,
                const char __user *user_buf, size_t count, loff_t *ppos)
{
        char buf[32];
        int buf_size;
        char *start = buf;
        unsigned long reg, value;
        int step = 1;
        struct snd_soc_codec *codec = file->private_data;
	struct wm8994 *amp = wm8994_amp;

        buf_size = min(count, (sizeof(buf)-1));
        if (copy_from_user(buf, user_buf, buf_size))
                return -EFAULT;
        buf[buf_size] = 0;

        while (*start == ' ')
                start++;
        reg = simple_strtoul(start, &start, 16);

        while (*start == ' ')
                start++;
        if (strict_strtoul(start, 16, &value))
                return -EINVAL;

        wm8994_reg_write(amp, reg, value);

        return buf_size;
}

static const struct file_operations codec_reg_fops = {
        .open = codec_reg_open_file,
        .read = codec_reg_read_file,
        .write = codec_reg_write_file,
};
#endif

static int wm8994_amp_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	int err = 0;
	int ret, rc;
	struct wm8994 *wm8994;

	wm_check_inited = 1;

	pr_err( ":%s\n",__func__);


	wm8994 = kzalloc(sizeof(struct wm8994), GFP_KERNEL);
	if (wm8994 == NULL) {
		kfree(i2c);
		return -ENOMEM;
	}

	i2c_set_clientdata(i2c, wm8994);
	wm8994->dev = &i2c->dev;
	wm8994->control_data = i2c;
	wm8994->read_dev = wm8994_i2c_read_device;
	wm8994->write_dev = wm8994_i2c_write_device;

	mutex_init(&wm8994->io_lock);

// LDO PIN SETTING
	rc = pm8xxx_gpio_config(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_AMP_EN), &amp_en);
	if (rc) {
		pr_err("%s PMIC_GPIO_AMP_EN config failed\n", __func__);
		return rc;
	}
	rc =  gpio_request(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_AMP_EN), "AMP_EN");
	if (rc) {
		pr_err("%s:Failed to request GPIO %d\n",
					__func__, PMIC_GPIO_AMP_EN);
	}
#ifdef CONFIG_MACH_P4_LTE
// SPK_SEL
/*
	rc = pm8xxx_gpio_config(PMIC_GPIO_SPK_SEL, &spk_sel);
	if (rc) {
		pr_err("%s PMIC_GPIO_AMP_EN config failed\n", __func__);
		return rc;
	}

	rc =  gpio_request(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SPK_SEL), "SPK_SEL");
	if (rc) {
		pr_err("%s:Failed to request GPIO %d\n",
					__func__, PMIC_GPIO_SPK_SEL);
	}
*/
#endif


	wm8994_amp = wm8994;

	//wm8994_set_common(1);

#ifdef CONFIG_DEBUG_FS
        debugfs_root = debugfs_create_dir("asoc", NULL);
        if (IS_ERR(debugfs_root) || !debugfs_root) {
                printk(KERN_WARNING
                       "ASoC: Failed to create debugfs directory\n");
                debugfs_root = NULL;
        }

        debugfs_root = debugfs_create_dir("wm8994_amp", debugfs_root);
        if (IS_ERR(debugfs_root) || !debugfs_root) {
                printk(KERN_WARNING
                       "ASoC: Failed to create debugfs codec directory\n");
                debugfs_root = NULL;
        }

        debugfs_reg = debugfs_create_file("codec_reg", 0644,
                                                 debugfs_root,
                                                 NULL, &codec_reg_fops);
        if (!debugfs_reg)
                printk(KERN_WARNING
                       "ASoC: Failed to create codec register debugfs file\n");
#endif


	audio_power(1);
	return 0;

	err:
		kfree(wm8994);
		return ret;

}


static int __devexit wm8994_amp_remove(struct i2c_client *i2c)
{
	audio_power(0);

	return 0;
}


static int wm8994_suspend(struct i2c_client *i2c)
{
	audio_power(0);
	return 0;
}

static int wm8994_resume(struct i2c_client *i2c)
{
	audio_power(1);
	return 0;
}


static const struct i2c_device_id wm8994_amp_id[] = {
	{ "wm8994-amp", 0 },
};
MODULE_DEVICE_TABLE(i2c, wm8994_amp_id);


static struct i2c_driver wm8994_amp_driver = {
	.id_table   	= wm8994_amp_id,
	.driver = {
		   .name = "wm8994-amp",
		   .owner = THIS_MODULE,
		   },
	.suspend    	= wm8994_suspend,
	.resume 		= wm8994_resume,
	.probe = wm8994_amp_probe,
	.remove = __devexit_p(wm8994_amp_remove),
};

static __init int wm8994_amp_init(void)
{
	#if defined(CONFIG_MACH_P5_LTE)
		if ( system_rev < 3 )
			return;
	#endif

	#if defined(CONFIG_MACH_P4_LTE)
		if ( system_rev < 2 )
			return;
	#endif


	return i2c_add_driver(&wm8994_amp_driver);

}
module_init(wm8994_amp_init);

static __exit void wm8994_amp_exit(void)
{
#if defined(CONFIG_MACH_P5_LTE)
	if ( system_rev < 3 )
		return;
#endif
#if defined(CONFIG_MACH_P4_LTE)
	if ( system_rev < 2 )
		return;
#endif
	i2c_del_driver(&wm8994_amp_driver);
}
module_exit(wm8994_amp_exit);



//EXPORT_SYMBOL(wm8994_set_common);
EXPORT_SYMBOL(wm8994_set_headset);
EXPORT_SYMBOL(wm8994_set_speaker);
#if defined(CONFIG_MACH_P8_LTE) //kks_110915_1
EXPORT_SYMBOL(wm8994_set_normal_headset); //kks_110916_1
EXPORT_SYMBOL(wm8994_set_normal_speaker);
#endif
EXPORT_SYMBOL(wm8994_set_cradle);


MODULE_DESCRIPTION("ASoC WM8994 driver as amp only ");
MODULE_AUTHOR("Joon <anomymous@samsung.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:wm8994-amp");




