/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/mfd/pmic8058.h>
#include <linux/mfd/pmic8901.h>
#include <linux/mfd/msm-adie-codec.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>
#include <linux/i2c/fsa9480.h>
#ifdef CONFIG_SENSORS_YDA165
#include <linux/i2c/yda165.h>
#endif

#include <mach/qdsp6v2/audio_dev_ctl.h>
#include <sound/apr_audio.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>
#include <mach/board-msm8660.h>

#if defined(CONFIG_JPN_MODEL_SC_03D)
#include <linux/kthread.h>
#endif
#include "snddev_icodec.h"
#include "snddev_ecodec.h"

#ifdef CONFIG_SEC_AUDIO_DEVICE
#define SEC_AUDIO_DEVICE /* #add_device */
#endif

#ifdef SEC_AUDIO_DEVICE /* don't remove this feature */
#if defined(CONFIG_USA_MODEL_SGH_T989)
#include "timpani_profile_celox_tmo.h"
#elif defined(CONFIG_EUR_MODEL_GT_I9210)
#include "timpani_profile_celox_eur.h"
#elif defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717)
#include "timpani_profile_celox_att.h"
#elif defined(CONFIG_USA_MODEL_SGH_I757)
#include "timpani_profile_celoxhd_att.h"
#elif defined(CONFIG_JPN_MODEL_SC_03D)
#include "timpani_profile_celox_jpn_ntt.h"
#elif defined(CONFIG_KOR_MODEL_SHV_E120S) /* DALI-SKT */
#include "timpani_profile_dali_skt.h"
#elif defined(CONFIG_KOR_MODEL_SHV_E120K) /* DALI-KT */
#include "timpani_profile_dali_kt.h"
#elif defined(CONFIG_KOR_MODEL_SHV_E120L) /* DALI-LGT */
#include "timpani_profile_dali_lgt.h"
#elif defined(CONFIG_KOR_MODEL_SHV_E160S) /* QUINCY-SKT */
#include "timpani_profile_quincy_skt.h"
#elif defined(CONFIG_KOR_MODEL_SHV_E160K) /* QUINCY-KT */
#include "timpani_profile_quincy_kt.h"
#elif defined(CONFIG_KOR_MODEL_SHV_E160L) /* QUINCY-LGT */
#include "timpani_profile_quincy_lgt.h"
#elif defined(CONFIG_USA_MODEL_SGH_I957) /* P5LTE-ATT */
#include "timpani_profile_p5lte_att.h"
#else
#include "timpani_profile_celox_kor.h"
#endif
#else
#include "timpani_profile_8x60.h"
#endif

#include "snddev_hdmi.h"
#include "snddev_mi2s.h"
#include "snddev_virtual.h"

#ifdef CONFIG_VP_A2220
#include <linux/a2220.h>
#endif

#ifdef CONFIG_VP_A2220
#define AUDIO_FREQUENCY 16000
#else
#define AUDIO_FREQUENCY 48000
#define GPIO_SELECT_I2S_AUDIENCE_QTR 124
#endif

#ifdef CONFIG_DEBUG_FS
static struct dentry *debugfs_hsed_config;
static void snddev_hsed_config_modify_setting(int type);
static void snddev_hsed_config_restore_setting(void);
#endif

extern unsigned int get_hw_rev(void);

#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
#define SNDDEV_GPIO_VPS_AMP_EN 142
#endif

/* GPIO_CLASS_D0_EN */
#define SNDDEV_GPIO_CLASS_D0_EN 227

/* GPIO_CLASS_D1_EN */
#define SNDDEV_GPIO_CLASS_D1_EN 229

#define SNDDEV_GPIO_MIC2_ANCR_SEL 294
#define SNDDEV_GPIO_MIC1_ANCL_SEL 295
#define SNDDEV_GPIO_HS_MIC4_SEL 296

#ifdef CONFIG_USA_MODEL_SGH_I717
#define PMIC_GPIO_MAIN_MICBIAS_EN      PM8058_GPIO(25)
#define PMIC_GPIO_SUB_MICBIAS_EN       PM8058_GPIO(26)
#endif

#define DSP_RAM_BASE_8x60 0x46700000
#define DSP_RAM_SIZE_8x60 0x2000000
static int dspcrashd_pdata_8x60 = 0xDEADDEAD;

static struct resource resources_dspcrashd_8x60[] = {
	{
		.name   = "msm_dspcrashd",
		.start  = DSP_RAM_BASE_8x60,
		.end    = DSP_RAM_BASE_8x60 + DSP_RAM_SIZE_8x60,
		.flags  = IORESOURCE_DMA,
	},
};

struct platform_device msm_device_dspcrashd_8x60 = {
	.name           = "msm_dspcrashd",
	.num_resources  = ARRAY_SIZE(resources_dspcrashd_8x60),
	.resource       = resources_dspcrashd_8x60,
	.dev = { .platform_data = &dspcrashd_pdata_8x60 },
};

static struct resource msm_cdcclk_ctl_resources[] = {
	{
		.name   = "msm_snddev_tx_mclk",
		.start  = 108,
		.end    = 108,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "msm_snddev_rx_mclk",
		.start  = 109,
		.end    = 109,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device msm_cdcclk_ctl_device = {
	.name   = "msm_cdcclk_ctl",
	.num_resources  = ARRAY_SIZE(msm_cdcclk_ctl_resources),
	.resource       = msm_cdcclk_ctl_resources,
};

static struct resource msm_aux_pcm_resources[] = {

	{
		.name   = "aux_pcm_dout",
		.start  = 111,
		.end    = 111,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_din",
		.start  = 112,
		.end    = 112,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_syncout",
		.start  = 113,
		.end    = 113,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_clkin_a",
		.start  = 114,
		.end    = 114,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device msm_aux_pcm_device = {
	.name   = "msm_aux_pcm",
	.num_resources  = ARRAY_SIZE(msm_aux_pcm_resources),
	.resource       = msm_aux_pcm_resources,
};

#if 0 /* (-) ysseo 20110414 */
static struct resource msm_mi2s_gpio_resources[] = {

	{
		.name   = "mi2s_ws",
		.start  = 101,
		.end    = 101,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "mi2s_sclk",
		.start  = 102,
		.end    = 102,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "mi2s_mclk",
		.start  = 103,
		.end    = 103,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "fm_mi2s_sd",
		.start  = 107,
		.end    = 107,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device msm_mi2s_device = {
	.name		= "msm_mi2s",
	.num_resources	= ARRAY_SIZE(msm_mi2s_gpio_resources),
	.resource	= msm_mi2s_gpio_resources,
};
#endif

/* Must be same size as msm_icodec_gpio_resources */
static int msm_icodec_gpio_defaults[] = {
	0,
	0,
};

static struct resource msm_icodec_gpio_resources[] = {
	{
		.name   = "msm_icodec_speaker_left",
		.start  = SNDDEV_GPIO_CLASS_D0_EN,
		.end    = SNDDEV_GPIO_CLASS_D0_EN,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "msm_icodec_speaker_right",
		.start  = SNDDEV_GPIO_CLASS_D1_EN,
		.end    = SNDDEV_GPIO_CLASS_D1_EN,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device msm_icodec_gpio_device = {
	.name   = "msm_icodec_gpio",
	.num_resources  = ARRAY_SIZE(msm_icodec_gpio_resources),
	.resource       = msm_icodec_gpio_resources,
	.dev = { .platform_data = &msm_icodec_gpio_defaults },
};

static struct regulator *s3;
static struct regulator *mvs;

#ifdef CONFIG_VP_A2220
void msm_snddev_audience_call_route_config(void)
{
	pr_debug("%s()\n", __func__);

	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 0);
	if (!dualmic_enabled) {
		pr_debug("%s: NS off\n", __func__);
		a2220_ioctl2(A2220_SET_CONFIG,
				A2220_PATH_INCALL_RECEIVER_NSOFF);
	} else {
		pr_debug("%s: NS on\n", __func__);
		a2220_ioctl2(A2220_SET_CONFIG,
				A2220_PATH_INCALL_RECEIVER_NSON);
	}
	pr_debug("[AUD] AUD Path\n");

#if 0
	/*
	 * (+)dragonball test Audience emulator.
	 * You should delete this line normal release. Don't forget this one
	 */
	msleep(2000);
	pr_info("##########################################################\n");
	pr_info("# WARNING msm_snddev_PCM_call_route_config WARNING\n");
	pr_info("# Audience emulator. DO NOT FORGET BELOW.\n");
	pr_info("# You should delete this line normal release.\n");
	pr_info("# WARNING WARNING WARNING WARNING WARNING WARNING\n");
	pr_info("##########################################################\n");
	gpio_direction_input(35); /* sda_pin */
	gpio_direction_input(36); /* scl_pin */
	/* gpio_set_value(123, 0); */
	/* gpio_set_value(122, 0); */
#endif

	return;
}

void msm_snddev_audience_call_route_deconfig(void)
{
	pr_debug("%s()\n", __func__);

#if 0
	/*
	 * (+)dragonball test Audience emulator.
	 * You should delete this line normal release. Don't forget this one
	 */
	msleep(2000);
	pr_info("##########################################################\n");
	pr_info("# WARNING msm_snddev_PCM_call_route_config WARNING\n");
	pr_info("# Audience emulator. DO NOT FORGET BELOW.\n");
	pr_info("# You should delete this line normal release.\n");
	pr_info("# WARNING WARNING WARNING WARNING WARNING WARNING\n");
	pr_info("##########################################################\n");
	gpio_direction_output(35, 1); /* sda_pin */
	gpio_direction_output(36, 1); /* scl_pin */
	/* gpio_set_value(123, 0); */
	/* gpio_set_value(122, 0); */
#endif

	a2220_ioctl2(A2220_SET_CONFIG , A2220_PATH_SUSPEND);
	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 1);
	pr_debug("[AUD] QTR Path\n");

#ifdef AUDIENCE_BYPASS
	/* (+)dragonball Multimedia bypass */
	if (get_hw_rev() < 0x05) {
		mdelay(5);
		a2220_ioctl2(A2220_SET_CONFIG , A2220_PATH_BYPASS_MULTIMEDIA);
	}
#endif

	return;
}

void msm_snddev_audience_call_route_speaker_config(void)
{
	pr_info("%s()\n", __func__);

#if defined(CONFIG_USA_MODEL_SGH_T989)
	/* defined(AUDIENCE_SUSPEND) enabling the H/W bypass */
	pr_debug("%s: dualmic disabled\n", __func__);
	return ;
#endif
#ifdef CONFIG_EUR_MODEL_GT_I9210
	a2220_ioctl2(A2220_SET_CONFIG , A2220_PATH_SUSPEND);	
	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 1); //switch  to I2S QTR
	pr_debug("[AUD] Audience bypass \n");
#else

	/* switch to I2S audience */
	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 0);
	a2220_ioctl2(A2220_SET_CONFIG , A2220_PATH_INCALL_SPEAKER);
	pr_debug("[AUD] AUD Path\n");
#endif
	return;
}

void msm_snddev_audience_call_route_speaker_deconfig(void)
{
	pr_debug("%s()\n", __func__);

	a2220_ioctl2(A2220_SET_CONFIG , A2220_PATH_SUSPEND);
	/* switch to I2S QTR */
	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 1);
	pr_debug("[AUD] QTR Path\n");

#ifdef AUDIENCE_BYPASS
	if (get_hw_rev() < 0x05) {
		mdelay(5);
		a2220_ioctl2(A2220_SET_CONFIG , A2220_PATH_BYPASS_MULTIMEDIA);
	}
#endif

	return;
}
#endif

static int msm_snddev_enable_dmic_power(void)
{
	int ret;

	s3 = regulator_get(NULL, "8058_s3");
	if (IS_ERR(s3)) {
		ret = -EBUSY;
		goto fail_get_s3;
	}

	ret = regulator_set_voltage(s3, 1800000, 1800000);
	if (ret) {
		pr_err("%s: error setting voltage\n", __func__);
		goto fail_s3;
	}

	ret = regulator_enable(s3);
	if (ret) {
		pr_err("%s: error enabling regulator\n", __func__);
		goto fail_s3;
	}

	mvs = regulator_get(NULL, "8901_mvs0");
	if (IS_ERR(mvs))
		goto fail_mvs0_get;

	ret = regulator_enable(mvs);
	if (ret) {
		pr_err("%s: error setting regulator\n", __func__);
		goto fail_mvs0_enable;
	}
	return ret;

fail_mvs0_enable:
	regulator_put(mvs);
	mvs = NULL;
fail_mvs0_get:
	regulator_disable(s3);
fail_s3:
	regulator_put(s3);
	s3 = NULL;
fail_get_s3:
	return ret;
}

static void msm_snddev_disable_dmic_power(void)
{
	int ret;

	if (mvs) {
		ret = regulator_disable(mvs);
		if (ret < 0)
			pr_err("%s: error disabling vreg mvs\n", __func__);
		regulator_put(mvs);
		mvs = NULL;
	}

	if (s3) {
		ret = regulator_disable(s3);
		if (ret < 0)
			pr_err("%s: error disabling regulator s3\n", __func__);
		regulator_put(s3);
		s3 = NULL;
	}
}

#define PM8901_MPP_3 (2) /* PM8901 MPP starts from 0 */

#ifndef CONFIG_SENSORS_YDA165
static int config_class_d0_gpio(int enable)
{
	int rc;

	struct pm8xxx_mpp_config_data class_d0_mpp = {
		.type		= PM8XXX_MPP_TYPE_D_OUTPUT,
		.level		= PM8901_MPP_DIG_LEVEL_MSMIO,
	};

	if (enable) {
		class_d0_mpp.control = PM8XXX_MPP_DOUT_CTRL_HIGH;
		rc = pm8xxx_mpp_config(PM8901_MPP_PM_TO_SYS(PM8901_MPP_3),
							&class_d0_mpp);
		if (rc) {
			pr_err("%s: CLASS_D0_EN failed\n", __func__);
			return rc;
		}

		rc = gpio_request(SNDDEV_GPIO_CLASS_D0_EN, "CLASSD0_EN");

		if (rc) {
			pr_err("%s: spkr pamp gpio pm8901 mpp3 request"
				"failed\n", __func__);
			class_d0_mpp.control = PM8XXX_MPP_DOUT_CTRL_LOW;
			pm8xxx_mpp_config(PM8901_MPP_PM_TO_SYS(PM8901_MPP_3),
						&class_d0_mpp);
			return rc;
		}

		gpio_direction_output(SNDDEV_GPIO_CLASS_D0_EN, 1);
		gpio_set_value_cansleep(SNDDEV_GPIO_CLASS_D0_EN, 1);

	} else {
		class_d0_mpp.control = PM8XXX_MPP_DOUT_CTRL_LOW;
		pm8xxx_mpp_config(PM8901_MPP_PM_TO_SYS(PM8901_MPP_3),
						&class_d0_mpp);
		gpio_set_value_cansleep(SNDDEV_GPIO_CLASS_D0_EN, 0);
		gpio_free(SNDDEV_GPIO_CLASS_D0_EN);
	}
	return 0;
}

static int config_class_d1_gpio(int enable)
{
	int rc;

	if (enable) {
		rc = gpio_request(SNDDEV_GPIO_CLASS_D1_EN, "CLASSD1_EN");
		if (rc) {
			pr_err("%s: Right Channel spkr gpio request"
				" failed\n", __func__);
			return rc;
		}

		gpio_direction_output(SNDDEV_GPIO_CLASS_D1_EN, 1);
		gpio_set_value_cansleep(SNDDEV_GPIO_CLASS_D1_EN, 1);
	} else {
		gpio_set_value_cansleep(SNDDEV_GPIO_CLASS_D1_EN, 0);
		gpio_free(SNDDEV_GPIO_CLASS_D1_EN);
	}
	return 0;
}
#endif

static atomic_t pamp_ref_cnt;

#ifdef CONFIG_VP_A2220
static int msm_snddev_audience_speaker_on(void)
{
#ifndef CONFIG_SENSORS_YDA165
	int rc;
#endif

	msm_snddev_audience_call_route_speaker_config();

	pr_debug("%s: enable msm_snddev_audience_speaker_on\n", __func__);

	if (atomic_inc_return(&pamp_ref_cnt) > 1)
		return 0;

	pr_debug("%s: enable stereo spkr amp\n", __func__);

#ifdef CONFIG_SENSORS_YDA165
#if defined(CONFIG_USA_MODEL_SGH_T989)\
|| defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_USA_MODEL_SGH_I757)
	yda165_speaker_call_onoff(1);
#else
	yda165_speaker_onoff(1);
#endif
	return 0;
#else
	rc = config_class_d0_gpio(1);
	if (rc) {
		pr_err("%s: d0 gpio configuration failed\n", __func__);
		goto config_gpio_fail;
	}
	if (!machine_is_msm8x60_qt()) {
		rc = config_class_d1_gpio(1);
		if (rc) {
			pr_err("%s: d1 gpio configuration failed\n", __func__);
			config_class_d0_gpio(0);
		}
	}

config_gpio_fail:
	return rc;
#endif
}

static void msm_snddev_audience_speaker_off(void)
{
	pr_debug("%s: disable msm_snddev_audience_speaker_off\n", __func__);

	msm_snddev_audience_call_route_speaker_deconfig();

	if (atomic_dec_return(&pamp_ref_cnt) == 0) {
		pr_debug("%s: disable stereo spkr amp\n", __func__);
#ifdef CONFIG_SENSORS_YDA165
#if defined(CONFIG_USA_MODEL_SGH_T989)\
|| defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717)
		yda165_speaker_call_onoff(0);
#else
		yda165_speaker_onoff(0);
#endif
#else
		config_class_d0_gpio(0);
		if (!machine_is_msm8x60_qt())
			config_class_d1_gpio(0);
		msleep(30);
#endif
	}
}
#endif

/* temp block for build error || defined(CONFIG_USA_MODEL_SGH_I717) */
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)
static int msm_snddev_poweramp_handset_on(void)
{
	pr_info("%s: enable handset amp\n", __func__);
#ifdef CONFIG_VP_A2220
	a2220_ioctl2(A2220_SET_CONFIG , A2220_PATH_SUSPEND);
	/* switch  to I2S QTR */
	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 1);
	pr_debug("[AUD] QTR Path\n");
#endif

	return 0;
}

static void msm_snddev_poweramp_handset_off(void)
{
	pr_info("%s: disable handset amp\n", __func__);
}
#endif

static int msm_snddev_poweramp_on(void)
{
#ifndef CONFIG_SENSORS_YDA165
	int rc;
#endif

	pr_info("%s: enable stereo spkr amp\n", __func__);

	if (atomic_inc_return(&pamp_ref_cnt) > 1)
		return 0;

	pr_debug("%s: enable stereo spkr amp\n", __func__);
#ifdef CONFIG_SENSORS_YDA165
	yda165_speaker_onoff(1);
	return 0;
#else
	rc = config_class_d0_gpio(1);
	if (rc) {
		pr_err("%s: d0 gpio configuration failed\n", __func__);
		goto config_gpio_fail;
	}
	if (!machine_is_msm8x60_qt()) {
		rc = config_class_d1_gpio(1);
		if (rc) {
			pr_err("%s: d1 gpio configuration failed\n", __func__);
			config_class_d0_gpio(0);
		}
	}
config_gpio_fail:
	return rc;
#endif
}

static void msm_snddev_poweramp_off(void)
{
	pr_info("%s: disable stereo spkr amp\n", __func__);

	if (atomic_dec_return(&pamp_ref_cnt) == 0) {
		pr_debug("%s: disable stereo spkr amp\n", __func__);
#ifdef CONFIG_SENSORS_YDA165
		yda165_speaker_onoff(0);
#else
		config_class_d0_gpio(0);
		if (!machine_is_msm8x60_qt())
			config_class_d1_gpio(0);
		msleep(30);
#endif
	}
}

#if defined(CONFIG_NOISE_REDUCE_FOR_WIFI_ON)
static int msm_snddev_differential_poweramp_on(void)
{
#ifndef CONFIG_SENSORS_YDA165
	int rc;
#endif

	pr_info("%s: enable mono spkr amp\n", __func__);

	if (atomic_inc_return(&pamp_ref_cnt) > 1)
		return 0;

	pr_debug("%s: enable mono spkr amp\n", __func__);
#ifdef CONFIG_SENSORS_YDA165
	yda165_differential_speaker_onoff(1); // Input stereo -> mono
	return 0;
#else
	rc = config_class_d0_gpio(1);
	if (rc) {
		pr_err("%s: d0 gpio configuration failed\n", __func__);
		goto config_gpio_fail;
	}
	if (!machine_is_msm8x60_qt()) {
		rc = config_class_d1_gpio(1);
		if (rc) {
			pr_err("%s: d1 gpio configuration failed\n", __func__);
			config_class_d0_gpio(0);
		}
	}
config_gpio_fail:
	return rc;
#endif
}

static void msm_snddev_differential_poweramp_off(void)
{
	pr_info("%s: disable mono spkr amp\n", __func__);

	if (atomic_dec_return(&pamp_ref_cnt) == 0) {
		pr_debug("%s: disable mono spkr amp\n", __func__);
#ifdef CONFIG_SENSORS_YDA165
		yda165_differential_speaker_onoff(0); // Input stereo -> mono
#else
		config_class_d0_gpio(0);
		if (!machine_is_msm8x60_qt())
			config_class_d1_gpio(0);
		msleep(30);
#endif
	}
}
#endif

#if defined(CONFIG_JPN_MODEL_SC_03D)
static struct task_struct *task;
static int yda_on_thread(void *data)
{
	msleep(100);
	/* For SC_03D, There is no separate yda gain table for SPK_CALL,
	 * so use normal speaker on_off
	 */
	yda165_speaker_onoff(1);
	return 0;
}
#endif

#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_USA_MODEL_SGH_I757)\
|| defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)\
|| defined(CONFIG_KOR_MODEL_SHV_E120L) || defined(CONFIG_USA_MODEL_SGH_T769)\
|| defined(CONFIG_JPN_MODEL_SC_03D)
static int msm_snddev_poweramp_on_call(void)
{
#ifndef CONFIG_SENSORS_YDA165
	int rc;
#endif

	pr_info("%s: enable stereo spkr call amp\n", __func__);

	if (atomic_inc_return(&pamp_ref_cnt) > 1)
		return 0;

	pr_debug("%s: enable stereo spkr amp\n", __func__);

#ifdef CONFIG_SENSORS_YDA165
#if defined(CONFIG_JPN_MODEL_SC_03D)
	pr_err("%s: separate thread for yda165_speaker_call_onoff\n", __func__);
	task = kthread_run(yda_on_thread, NULL, "yda_on_delay_thread");
	if (IS_ERR(task))
		task = NULL;
#else
	yda165_speaker_call_onoff(1);
#endif
	return 0;
#else
	rc = config_class_d0_gpio(1);
	if (rc) {
		pr_err("%s: d0 gpio configuration failed\n", __func__);
		goto config_gpio_fail;
	}
	if (!machine_is_msm8x60_qt()) {
		rc = config_class_d1_gpio(1);
		if (rc) {
			pr_err("%s: d1 gpio configuration failed\n", __func__);
			config_class_d0_gpio(0);
		}
	}
config_gpio_fail:
	return rc;
#endif
}

static void msm_snddev_poweramp_off_call(void)
{
	pr_info("%s: disable stereo spkr call amp\n", __func__);

	if (atomic_dec_return(&pamp_ref_cnt) == 0) {
		pr_debug("%s: disable stereo spkr amp\n", __func__);
#ifdef CONFIG_SENSORS_YDA165
#if defined(CONFIG_JPN_MODEL_SC_03D)
		/* For SC_03D, There is no separate yda gain table for SPK_CALL,
		 * so use normal speaker on_off
		 */
		yda165_speaker_onoff(0);
#else
		yda165_speaker_call_onoff(0);
#endif
#else
		config_class_d0_gpio(0);
		if (!machine_is_msm8x60_qt())
			config_class_d1_gpio(0);
		msleep(30);
#endif
	}
}
#endif

#ifdef CONFIG_VP_A2220
void msm_snddev_audience_call_route_headset_config(void)
{
	pr_debug("%s()\n", __func__);

#if defined(CONFIG_USA_MODEL_SGH_T989) && defined(AUDIENCE_SUSPEND)
	pr_debug("%s: dualmic disabled\n", __func__);
	return 0;
#endif
#if defined(CONFIG_EUR_MODEL_GT_I9210)
	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 1); //switch  to I2S audience
	a2220_ioctl2(A2220_SET_CONFIG , A2220_PATH_SUSPEND);	
	pr_debug("[AUD] QTR Path \n");
#else

	/* switch to I2S audience */
	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 0);
	a2220_ioctl2(A2220_SET_CONFIG , A2220_PATH_INCALL_HEADSET);
	pr_debug("[AUD] AUD Path\n");
#endif
	return;
}

void msm_snddev_audience_call_route_headset_deconfig(void)
{
	pr_debug("%s()\n", __func__);

	/* switch to I2S QTR */
	a2220_ioctl2(A2220_SET_CONFIG , A2220_PATH_SUSPEND);
	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 1);
	pr_debug("[AUD] QTR Path\n");

#ifdef AUDIENCE_BYPASS
	if (get_hw_rev() < 0x05) {
		mdelay(5);
		a2220_ioctl2(A2220_SET_CONFIG ,
				A2220_PATH_BYPASS_MULTIMEDIA);
	}
#endif

	return;
}

int msm_snddev_audience_poweramp_on_headset(void)
{
	msm_snddev_audience_call_route_headset_config();
#ifdef CONFIG_SENSORS_YDA165
#if defined(CONFIG_USA_MODEL_SGH_T989)\
|| defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_USA_MODEL_SGH_I757)
	yda165_headset_call_onoff(1);
#else
	yda165_headset_onoff(1);
#endif
#endif
	pr_debug("%s: power on headset\n", __func__);

	return 0;
}

void msm_snddev_audience_poweramp_off_headset(void)
{
	msm_snddev_audience_call_route_headset_deconfig();
#ifdef CONFIG_SENSORS_YDA165
#if defined(CONFIG_USA_MODEL_SGH_T989)\
|| defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_USA_MODEL_SGH_I757)
	yda165_headset_call_onoff(0);
#else
	yda165_headset_onoff(0);
#endif
#endif
	pr_debug("%s: power off headset\n", __func__);
}
#endif

#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_USA_MODEL_SGH_I757)
int msm_snddev_poweramp_on_headset_call(void)
{
	fsa9480_audiopath_control(0);
#ifdef CONFIG_SENSORS_YDA165
	yda165_headset_call_onoff(1);
#endif
	return 0;
}

void msm_snddev_poweramp_off_headset_call(void)
{
#ifdef CONFIG_SENSORS_YDA165
	yda165_headset_call_onoff(0);
#endif
	pr_info("%s: power on headset call\n", __func__);
}
#endif

int msm_snddev_poweramp_on_headset(void)
{
	//fsa9480_audiopath_control(0); /* prevent lineout sound out */
#ifdef CONFIG_SENSORS_YDA165
	yda165_headset_onoff(1);
#endif
	return 0;
}

void msm_snddev_poweramp_off_headset(void)
{
#ifdef CONFIG_SENSORS_YDA165
	yda165_headset_onoff(0);
#endif
	pr_info("%s: power on headset\n", __func__);
}

#if defined(CONFIG_KOR_MODEL_SHV_E120S)\
|| defined(CONFIG_KOR_MODEL_SHV_E120K)\
|| defined(CONFIG_KOR_MODEL_SHV_E120L)
int msm_snddev_poweramp_on_call_headset(void)
{
#ifdef CONFIG_SENSORS_YDA165
	yda165_headset_call_onoff(1);
#endif
	return 0;
}

void msm_snddev_poweramp_off_call_headset(void)
{
#ifdef CONFIG_SENSORS_YDA165
	yda165_headset_call_onoff(0);
#endif
	pr_info("%s: power on headset\n", __func__);
}
#endif

#if defined(CONFIG_USA_MODEL_SGH_I757)
int msm_snddev_vpsamp_on_headset(void)
{
#ifdef CONFIG_SENSORS_YDA165
	yda165_headset_onoff(1);
#endif
	fsa9480_audiopath_control(1);
	return 0;
}

void msm_snddev_vpsramp_off_headset(void)
{
#ifdef CONFIG_SENSORS_YDA165
	yda165_headset_onoff(0);
#endif
	fsa9480_audiopath_control(0);
	pr_info("%s: power on headset\n", __func__);
}
#endif

#if defined(CONFIG_USA_MODEL_SGH_T989)
int msm_snddev_vpsamp_on_headset(void)
{

	if (get_hw_rev() >= 0x0D) {
		/* remove the pop noise (device -> external speaker of dock) */
		msleep(50);
		gpio_tlmm_config(GPIO_CFG(SNDDEV_GPIO_VPS_AMP_EN, 0,
				GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
				GPIO_CFG_8MA), GPIO_CFG_ENABLE);
		gpio_set_value(SNDDEV_GPIO_VPS_AMP_EN, 1);
		pr_info("%s: power on vps\n", __func__);
	} else {
#ifdef CONFIG_SENSORS_YDA165
		yda165_headset_onoff(1);
		pr_info("%s: power on amp headset\n", __func__);
#endif
	}
	//fsa9480_audiopath_control(1);
	return 0;
}

void msm_snddev_vpsramp_off_headset(void)
{
	if (get_hw_rev() >= 0x0D) {
		gpio_set_value(SNDDEV_GPIO_VPS_AMP_EN, 0);
		pr_info("%s: power off vps\n", __func__);
	} else {
#ifdef CONFIG_SENSORS_YDA165
		yda165_headset_onoff(0);
		pr_info("%s: power off amp headset\n", __func__);
#endif
	}
	//fsa9480_audiopath_control(0);
	// return 0;
}
#endif

int msm_snddev_poweramp_on_together(void)
{
#ifdef CONFIG_SENSORS_YDA165
	yda165_speaker_headset_onoff(1);
#endif
	pr_info("%s: power on amplifier\n", __func__);
	return 0;
}

void msm_snddev_poweramp_off_together(void)
{
#ifdef CONFIG_SENSORS_YDA165
	yda165_speaker_headset_onoff(0);
#endif
	pr_info("%s: power off amplifier\n", __func__);
}
#ifdef CONFIG_EUR_MODEL_GT_I9210
int msm_snddev_poweramp_on_lineout_I9210(void)
{
#if defined(CONFIG_VP_A2220)
	a2220_ioctl2(A2220_SET_CONFIG , A2220_PATH_SUSPEND);	
	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 1); //switch  to I2S QTR
	pr_debug("[AUD] QTR Path \n");
#endif

#ifdef CONFIG_SENSORS_YDA165
	yda165_lineout_onoff(1);
#endif
	return 0;
}

void msm_snddev_poweramp_off_lineout_I9210(void)
{
#ifdef CONFIG_SENSORS_YDA165
	yda165_lineout_onoff(0);
#endif
	return ;
}
#endif

#if defined(CONFIG_TARGET_LOCALE_KOR)
static struct regulator *snddev_reg_l1;

int msm_snddev_poweramp_on_lineout(void)
{
	int rc;
	pr_debug("%s\n", __func__);

	/*
	 * PMIC8058 L1 Setting
	 * (L1 must be set the default voltage 1.0V
	 * because L1 is internally used for NCP level shifter supply)
	 */
#if defined(CONFIG_KOR_MODEL_SHV_E110S)
	if (get_hw_rev() >= 0x8)
#elif defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)
	if (get_hw_rev() >= 0x6)
#elif defined(CONFIG_KOR_MODEL_SHV_E120L) || defined(CONFIG_KOR_MODEL_SHV_E160L)
	if (get_hw_rev() >= 0x3)
#endif
	{
		snddev_reg_l1 = regulator_get(NULL, "8058_l1");
		if (IS_ERR(snddev_reg_l1)) {
			pr_err("%s: regulator_get(%s) failed (%ld)\n",
				__func__, "l2", PTR_ERR(snddev_reg_l1));
			return -EBUSY;
		}

		rc = regulator_set_voltage(snddev_reg_l1, 1000000, 1000000);
		if (rc < 0)
			pr_err("%s: regulator_set_voltage(l1) failed (%d)\n",
				__func__, rc);

		rc = regulator_enable(snddev_reg_l1);
		if (rc < 0)
			pr_err("%s: regulator_enable(l1) failed (%d)\n",
				__func__, rc);
	}

#ifdef CONFIG_SENSORS_YDA165
#if defined(CONFIG_KOR_MODEL_SHV_E120S)\
|| defined(CONFIG_KOR_MODEL_SHV_E120K)\
|| defined(CONFIG_KOR_MODEL_SHV_E120L)
	yda165_lineout_onoff(1);
#else
	yda165_headset_onoff(1);
#endif
#endif

	return 0;
}
void msm_snddev_poweramp_off_lineout(void)
{
	int rc;

#if defined(CONFIG_KOR_MODEL_SHV_E110S)
	if (get_hw_rev() >= 0x8)
#elif defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)
	if (get_hw_rev() >= 0x6)
#elif defined(CONFIG_KOR_MODEL_SHV_E120L) || defined(CONFIG_KOR_MODEL_SHV_E160L)
	if (get_hw_rev() >= 0x3)
#endif
	{
		rc = regulator_disable(snddev_reg_l1);
		if (rc < 0)
			pr_err("%s: regulator_disable(l1) failed (%d)\n",
				__func__, rc);

		regulator_put(snddev_reg_l1);

		snddev_reg_l1 = NULL;
	}

#ifdef CONFIG_SENSORS_YDA165
#if defined(CONFIG_KOR_MODEL_SHV_E120S)\
|| defined(CONFIG_KOR_MODEL_SHV_E120K)\
|| defined(CONFIG_KOR_MODEL_SHV_E120L)
	yda165_lineout_onoff(0);
#else
	yda165_headset_onoff(0);
#endif
#endif

	pr_info("%s: power on headset\n", __func__);
}
#endif /* #if defined(CONFIG_TARGET_LOCALE_KOR) */

/* Regulator 8058_l10 supplies regulator 8058_ncp. */
static struct regulator *snddev_reg_ncp;
static struct regulator *snddev_reg_l10;

static atomic_t preg_ref_cnt;

static int msm_snddev_voltage_on(void)
{
	int rc = 0;
	pr_debug("%s\n", __func__);

	if (atomic_inc_return(&preg_ref_cnt) > 1)
		return 0;

	/*
	 * Always at (CONFIG_KOR_MODEL_SHV_E160S, CONFIG_KOR_MODEL_SHV_E160K,
	 * CONFIG_USA_MODEL_SGH_I727, CONFIG_USA_MODEL_SGH_I717)
	 */
#if defined(CONFIG_USA_MODEL_SGH_T989)
	if (get_hw_rev() >= 0x9) /* Rev0.3, Rev0.3A */
#elif defined(CONFIG_KOR_MODEL_SHV_E110S)
	if (get_hw_rev() >= 0x5) /* CELOX_REV05 */
#elif defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)
	if (get_hw_rev() >= 0x6) /* DALI_SKT_REV03 */
#elif defined(CONFIG_KOR_MODEL_SHV_E120L) || defined(CONFIG_KOR_MODEL_SHV_E160L)
	if (get_hw_rev() >= 0x2) /* DALI_LGT_REV02 */
#elif defined(CONFIG_USA_MODEL_SGH_I727) || defined(CONFIG_USA_MODEL_SGH_I717)
	if (get_hw_rev() >= 0x0) /* CELOX_ATT */
#elif defined(CONFIG_JPN_MODEL_SC_03D)
	if (get_hw_rev() >= 0x3) /* not yet */
#endif
	{
		/*
		 * PMIC8058 L10 Setting
		 * (L10 must be set the default voltage 2.6V
		 * because L10 is internally used for NCP level shifter supply)
		 */
		snddev_reg_l10 = regulator_get(NULL, "8058_l10");
		if (IS_ERR(snddev_reg_l10)) {
			pr_err("%s: regulator_get(%s) failed (%ld)\n",
				__func__, "l2", PTR_ERR(snddev_reg_l10));
			return -EBUSY;
		}

		rc = regulator_set_voltage(snddev_reg_l10, 2600000, 2600000);
		if (rc < 0)
			pr_err("%s: regulator_set_voltage(l10) failed (%d)\n",
				__func__, rc);

		rc = regulator_enable(snddev_reg_l10);
		if (rc < 0)
			pr_err("%s: regulator_enable(l10) failed (%d)\n",
				__func__, rc);

		/* NCP Setting */
		snddev_reg_ncp = regulator_get(NULL, "8058_ncp");
		if (IS_ERR(snddev_reg_ncp)) {
			pr_err("%s: regulator_get(%s) failed (%ld)\n",
				__func__, "ncp", PTR_ERR(snddev_reg_ncp));
			return -EBUSY;
		}

		rc = regulator_set_voltage(snddev_reg_ncp, 1800000, 1800000);
		if (rc < 0) {
			pr_err("%s: regulator_set_voltage(ncp) failed (%d)\n",
				__func__, rc);
			goto regulator_fail;
		}

		rc = regulator_enable(snddev_reg_ncp);
		pr_info("%s: NCP block On\n", __func__);

		if (rc < 0) {
			pr_err("%s: regulator_enable(ncp) failed (%d)\n",
				__func__, rc);
			goto regulator_fail;
		}
	}

	return rc;

regulator_fail:
	regulator_put(snddev_reg_ncp);
	snddev_reg_ncp = NULL;
	return rc;
}

static void msm_snddev_voltage_off(void)
{
	int rc;
	pr_debug("%s\n", __func__);

	if (!snddev_reg_ncp)
		goto done;

	if (atomic_dec_return(&preg_ref_cnt) == 0) {
		rc = regulator_disable(snddev_reg_ncp);
		if (rc < 0)
			pr_err("%s: regulator_disable(ncp) failed (%d)\n",
				__func__, rc);
		regulator_put(snddev_reg_ncp);

		snddev_reg_ncp = NULL;
	}

done:
	if (!snddev_reg_l10)
		return;

	rc = regulator_disable(snddev_reg_l10);
	if (rc < 0)
		pr_err("%s: regulator_disable(l10) failed (%d)\n",
			__func__, rc);

	regulator_put(snddev_reg_l10);

	snddev_reg_l10 = NULL;
}

#ifdef CONFIG_VP_A2220
#if defined(CONFIG_EUR_MODEL_GT_I9210)
static int msm_snddev_setting_audience_loopback_on(void)
{
	pr_info("%s()\n", __func__);
	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 1);
	a2220_ioctl2(A2220_SET_CONFIG, A2220_PATH_SUSPEND);

	return 0;
}

static void msm_snddev_setting_audience_loopback_off(void)
{
	pr_info("%s()\n", __func__);
	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 1);
	a2220_ioctl2(A2220_SET_CONFIG, A2220_PATH_SUSPEND);

	return ;
}
#endif

static int msm_snddev_setting_audience_call_connect(void)
{
#if defined(CONFIG_USA_MODEL_SGH_T989) && defined(AUDIENCE_SUSPEND)
	if (!dualmic_enabled) {
		pr_debug("%s: dualmic disabled\n", __func__);
		return 0;
	}
#endif

	msm_snddev_audience_call_route_config();
	return 0;
}

static void msm_snddev_setting_audience_call_disconnect(void)
{
#if defined(CONFIG_USA_MODEL_SGH_T989) && defined(AUDIENCE_SUSPEND)
	if (!dualmic_enabled) {
		pr_debug("%s: dualmic disabled\n", __func__);
	}
#endif

	msm_snddev_audience_call_route_deconfig();
}

static int msm_snddev_enable_audience_amic_power(void)
{
	int ret = 0;

#ifdef CONFIG_PMIC8058_OTHC
	ret = pm8058_micbias_enable(OTHC_MICBIAS_0,
			OTHC_SIGNAL_ALWAYS_ON);
	if (ret)
		pr_err("%s: Enabling amic power failed\n", __func__);

#ifdef CONFIG_VP_A2220
	pr_debug("%s: A2220::enable sub_mic\n", __func__);
	ret = pm8058_micbias_enable(OTHC_MICBIAS_2,
			OTHC_SIGNAL_ALWAYS_ON);
	if (ret)
		pr_err("%s: Enabling sub_mic power failed\n", __func__);

	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 0);
	if (!dualmic_enabled) {
		pr_debug("%s: NS off\n", __func__);
		a2220_ioctl2(A2220_SET_CONFIG,
			A2220_PATH_INCALL_RECEIVER_NSOFF);
	} else {
		pr_debug("%s: NS on\n", __func__);
		a2220_ioctl2(A2220_SET_CONFIG,
			A2220_PATH_INCALL_RECEIVER_NSON);
	}
	pr_debug("[AUD] AUD Path\n");
#endif
#endif
	return ret;
}

static void msm_snddev_disable_audience_amic_power(void)
{
	int ret;

#ifdef CONFIG_PMIC8058_OTHC
	ret = pm8058_micbias_enable(OTHC_MICBIAS_0, OTHC_SIGNAL_OFF);

	if (ret)
		pr_err("%s: Disabling amic power failed\n", __func__);

#ifdef CONFIG_VP_A2220
	pr_info("%s: A2220::disable sub_mic\n", __func__);
	ret = pm8058_micbias_enable(OTHC_MICBIAS_2, OTHC_SIGNAL_OFF);
	if (ret)
		pr_err("%s: Disabling sub_mic power failed\n", __func__);

#if defined(CONFIG_USA_MODEL_SGH_T989) && defined(AUDIENCE_SUSPEND)
	a2220_ioctl2(A2220_SET_CONFIG, A2220_PATH_SUSPEND);
	/* switch to I2S QTR */
	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 1);
	pr_debug("[AUD] QTR Path\n");
#elif defined(CONFIG_USA_MODEL_SGH_I717)
	a2220_ioctl2(A2220_SET_CONFIG, A2220_PATH_INCALL_RECEIVER_NSOFF);
	pr_debug("[AUD] AUD Path\n");
#endif

#ifdef AUDIENCE_BYPASS
	if (get_hw_rev() < 0x05) {
		mdelay(5);
		a2220_ioctl2(A2220_SET_CONFIG , A2220_PATH_BYPASS_MULTIMEDIA);
	}
#endif

#endif /* CONFIG_VP_A2220 */
#endif /* CONFIG_PMIC8058_OTHC */

	return;
}
#endif

static int msm_snddev_enable_amic_power(void)
{
	int ret = 0;
#ifdef CONFIG_PMIC8058_OTHC

	if (machine_is_msm8x60_fluid()) {
		ret = pm8058_micbias_enable(OTHC_MICBIAS_0,
				OTHC_SIGNAL_ALWAYS_ON);
		if (ret)
			pr_err("%s: Enabling amic power failed\n", __func__);

		ret = gpio_request(SNDDEV_GPIO_MIC2_ANCR_SEL, "MIC2_ANCR_SEL");
		if (ret) {
			pr_err("%s: spkr pamp gpio %d request failed\n",
				__func__, SNDDEV_GPIO_MIC2_ANCR_SEL);
			return ret;
		}
		gpio_direction_output(SNDDEV_GPIO_MIC2_ANCR_SEL, 0);
	} else {
		printk("main_mic bias on\n");
		ret = pm8058_micbias_enable(OTHC_MICBIAS_0,
				OTHC_SIGNAL_ALWAYS_ON);
		if (ret)
			pr_err("%s: Enabling amic power failed\n", __func__);
	}
#ifdef CONFIG_VP_A2220
	if (machine_is_msm8x60_fluid()) {
		printk("msm8x60_fluid enable sub_mic on\n");
		ret = pm8058_micbias_enable(OTHC_MICBIAS_2,
				OTHC_SIGNAL_ALWAYS_ON);
		if (ret)
			pr_err("%s: Enabling sub_mic power failed\n", __func__);
		
			ret = gpio_request(SNDDEV_GPIO_MIC2_ANCR_SEL, "MIC2_ANCR_SEL");
			if (ret) {
				pr_err("%s: spkr pamp gpio %d request failed\n",
					__func__, SNDDEV_GPIO_MIC2_ANCR_SEL);
			return ret;
		}
		gpio_direction_output(SNDDEV_GPIO_MIC2_ANCR_SEL, 0);
		
	} else {
		printk("sub_mic bias on\n");		
		ret = pm8058_micbias_enable(OTHC_MICBIAS_2,
				OTHC_SIGNAL_ALWAYS_ON);
		if (ret)
			pr_err("%s: Enabling sub_mic power failed\n", __func__);
	}

#endif	
#endif
	return ret;
}

static int msm_snddev_enable_voip_amic_power(void)
{
	int ret = 0;
#ifdef CONFIG_PMIC8058_OTHC
	if (machine_is_msm8x60_fluid()) {
		ret = pm8058_micbias_enable(OTHC_MICBIAS_0,
				OTHC_SIGNAL_ALWAYS_ON);
		if (ret)
			pr_err("%s: Enabling amic power failed\n", __func__);

		ret = gpio_request(SNDDEV_GPIO_MIC2_ANCR_SEL, "MIC2_ANCR_SEL");
		if (ret) {
			pr_err("%s: spkr pamp gpio %d request failed\n",
					__func__, SNDDEV_GPIO_MIC2_ANCR_SEL);
			return ret;
		}
		gpio_direction_output(SNDDEV_GPIO_MIC2_ANCR_SEL, 0);
	} else {
#if defined(CONFIG_USA_MODEL_SGH_I717)
		if (get_hw_rev() >= 0x3) {
			gpio_direction_output(PM8058_GPIO_PM_TO_SYS(
				PMIC_GPIO_MAIN_MICBIAS_EN), 1);
		} else {
			ret = pm8058_micbias_enable(OTHC_MICBIAS_0,
					OTHC_SIGNAL_ALWAYS_ON);
			if (ret)
				pr_err("%s: Enabling amic power failed\n",
					__func__);
		}
#else
		ret = pm8058_micbias_enable(OTHC_MICBIAS_0,
				OTHC_SIGNAL_ALWAYS_ON);
		if (ret)
			pr_err("%s: Enabling amic power failed\n", __func__);
#endif
	}

#endif
	return ret;
}

static void msm_snddev_disable_amic_power(void)
{
#ifdef CONFIG_PMIC8058_OTHC
	int ret;
	if (machine_is_msm8x60_fluid()) {
		ret = pm8058_micbias_enable(OTHC_MICBIAS_0,
				OTHC_SIGNAL_OFF);
		gpio_free(SNDDEV_GPIO_MIC1_ANCL_SEL);
		gpio_free(SNDDEV_GPIO_MIC2_ANCR_SEL);
	} else {
#if defined(CONFIG_USA_MODEL_SGH_I717)
		if (get_hw_rev() >= 0x3) {
			gpio_direction_output(PM8058_GPIO_PM_TO_SYS(
				PMIC_GPIO_MAIN_MICBIAS_EN), 0);
		} else {
			ret = pm8058_micbias_enable(OTHC_MICBIAS_0,
				OTHC_SIGNAL_OFF);
			if (ret)
				pr_err("%s: Disabling amic power failed\n",
					__func__);
		}
#else
		printk("main_mic bias off\n");
		ret = pm8058_micbias_enable(OTHC_MICBIAS_0, OTHC_SIGNAL_OFF);
		if (ret)
			pr_err("%s: Disabling amic power failed\n", __func__);
#endif
	}
#endif

#ifdef CONFIG_VP_A2220
	if (machine_is_msm8x60_fluid()) {
		pr_err("1.A2220::disable sub_mic off\n");
		ret = pm8058_micbias_enable(OTHC_MICBIAS_2,
				OTHC_SIGNAL_OFF);
		/* gpio_free(SNDDEV_GPIO_MIC2_ANCR_SEL); */
	} else {
#if defined(CONFIG_USA_MODEL_SGH_I717)
		if (get_hw_rev() >= 0x03) {
			pr_debug("2.A2220::disable sub_mic off\n");
			gpio_direction_output(PM8058_GPIO_PM_TO_SYS(
				PMIC_GPIO_SUB_MICBIAS_EN), 0);
		} else {
			pr_debug("2.A2220::disable sub_mic off\n");
			ret = pm8058_micbias_enable(OTHC_MICBIAS_2,
				OTHC_SIGNAL_OFF);
		}
#else
		printk("sub_mic bias off\n");
		ret = pm8058_micbias_enable(OTHC_MICBIAS_2, OTHC_SIGNAL_OFF);
#endif
	}
	if (ret)
		pr_err("%s: Disabling amic power failed\n", __func__);
#endif
}

#if 0 /* warning, compilation error - not used */
static int msm_snddev_enable_anc_power(void)
{
	int ret = 0;
#ifdef CONFIG_PMIC8058_OTHC
	ret = pm8058_micbias_enable(OTHC_MICBIAS_2,
		OTHC_SIGNAL_ALWAYS_ON);
	if (ret)
		pr_err("%s: Enabling anc micbias 2 failed\n", __func__);

	if (machine_is_msm8x60_fluid()) {
		ret = pm8058_micbias_enable(OTHC_MICBIAS_0,
				OTHC_SIGNAL_ALWAYS_ON);
		if (ret)
			pr_err("%s: Enabling anc micbias 0 failed\n", __func__);

		ret = gpio_request(SNDDEV_GPIO_MIC2_ANCR_SEL, "MIC2_ANCR_SEL");
		if (ret) {
			pr_err("%s: mic2 ancr gpio %d request failed\n",
				__func__, SNDDEV_GPIO_MIC2_ANCR_SEL);
			return ret;
		}
		gpio_direction_output(SNDDEV_GPIO_MIC2_ANCR_SEL, 1);

		ret = gpio_request(SNDDEV_GPIO_MIC1_ANCL_SEL, "MIC1_ANCL_SEL");
		if (ret) {
			pr_err("%s: mic1 ancl gpio %d request failed\n",
				__func__, SNDDEV_GPIO_MIC1_ANCL_SEL);
			gpio_free(SNDDEV_GPIO_MIC2_ANCR_SEL);
			return ret;
		}
		gpio_direction_output(SNDDEV_GPIO_MIC1_ANCL_SEL, 1);
	}
#endif
	return ret;
}

static void msm_snddev_disable_anc_power(void)
{
#ifdef CONFIG_PMIC8058_OTHC
	int ret;

	ret = pm8058_micbias_enable(OTHC_MICBIAS_2, OTHC_SIGNAL_OFF);

	if (machine_is_msm8x60_fluid()) {
		ret |= pm8058_micbias_enable(OTHC_MICBIAS_0,
				OTHC_SIGNAL_OFF);
		gpio_free(SNDDEV_GPIO_MIC2_ANCR_SEL);
		gpio_free(SNDDEV_GPIO_MIC1_ANCL_SEL);
	}

	if (ret)
		pr_err("%s: Disabling anc power failed\n", __func__);
#endif
}
#endif

static int msm_snddev_enable_amic_sec_power(void)
{
#ifdef CONFIG_PMIC8058_OTHC
	int ret;

	if (machine_is_msm8x60_fluid()) {

		ret = pm8058_micbias_enable(OTHC_MICBIAS_2,
				OTHC_SIGNAL_ALWAYS_ON);
		if (ret)
			pr_err("%s: Enabling amic2 power failed\n", __func__);

		ret = gpio_request(SNDDEV_GPIO_HS_MIC4_SEL,
						"HS_MIC4_SEL");
		if (ret) {
			pr_err("%s: spkr pamp gpio %d request failed\n",
					__func__, SNDDEV_GPIO_HS_MIC4_SEL);
			return ret;
		}
		gpio_direction_output(SNDDEV_GPIO_HS_MIC4_SEL, 1);
	}
#endif

	msm_snddev_enable_amic_power();
	return 0;
}

static void msm_snddev_disable_amic_sec_power(void)
{
#ifdef CONFIG_PMIC8058_OTHC
	int ret;
	if (machine_is_msm8x60_fluid()) {

		ret = pm8058_micbias_enable(OTHC_MICBIAS_2,
					OTHC_SIGNAL_OFF);
		if (ret)
			pr_err("%s: Disabling amic2 power failed\n", __func__);

		gpio_free(SNDDEV_GPIO_HS_MIC4_SEL);
	}
#endif

	msm_snddev_disable_amic_power();
}

#ifndef SEC_AUDIO_DEVICE
static int msm_snddev_enable_dmic_sec_power(void)
{
	int ret;

	ret = msm_snddev_enable_dmic_power();
	if (ret) {
		pr_err("%s: Error: Enabling dmic power failed\n", __func__);
		return ret;
	}
#ifdef CONFIG_PMIC8058_OTHC
#if defined(CONFIG_USA_MODEL_SGH_I717)
	if (get_hw_rev() >= 0x03) {
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(
			PMIC_GPIO_SUB_MICBIAS_EN), 1);
	} else {
		ret = pm8058_micbias_enable(OTHC_MICBIAS_2,
					OTHC_SIGNAL_ALWAYS_ON);
		if (ret) {
			pr_err("%s: Error: Enabling micbias failed\n",
				__func__);
			msm_snddev_disable_dmic_power();
			return ret;
		}
	}
#else
	ret = pm8058_micbias_enable(OTHC_MICBIAS_2, OTHC_SIGNAL_ALWAYS_ON);
	if (ret) {
		pr_err("%s: Error: Enabling micbias failed\n", __func__);
		msm_snddev_disable_dmic_power();
		return ret;
	}
#endif
#endif
	return 0;
}

static void msm_snddev_disable_dmic_sec_power(void)
{
	msm_snddev_disable_dmic_power();

#ifdef CONFIG_PMIC8058_OTHC
#if defined(CONFIG_USA_MODEL_SGH_I717)
	if (get_hw_rev() >= 0x03) {
		gpio_direction_output(PM8058_GPIO_PM_TO_SYS(
			PMIC_GPIO_SUB_MICBIAS_EN), 0);
	} else
		pm8058_micbias_enable(OTHC_MICBIAS_2, OTHC_SIGNAL_OFF);
#else
		pm8058_micbias_enable(OTHC_MICBIAS_2, OTHC_SIGNAL_OFF);
#endif
#endif
}
#endif //#ifndef SEC_AUDIO_DEVICE

#ifdef SEC_AUDIO_DEVICE
static int msm_snddev_enable_submic_power(void)
{
	int ret = 0;
#ifdef CONFIG_PMIC8058_OTHC
	ret = pm8058_micbias_enable(OTHC_MICBIAS_2, OTHC_SIGNAL_ALWAYS_ON);
	if (ret)
		pr_err("%s: Enabling submic power failed\n", __func__);
	else
		pr_info("%s: Enabling submic power success\n", __func__);
#endif
	return ret;
}

static void msm_snddev_disable_submic_power(void)
{
#ifdef CONFIG_PMIC8058_OTHC
	int ret;
	ret = pm8058_micbias_enable(OTHC_MICBIAS_2, OTHC_SIGNAL_OFF);

	if (ret)
		pr_err("%s: Disabling submic power failed\n", __func__);
	else
		pr_info("%s: Disabling submic power success\n", __func__);
#endif
}

// ------- DEFINITION OF NORMAL PAIRED DEVICES ------
static struct adie_codec_action_unit handset_rx_48KHz_osr256_actions[] =
ADIE_HANDSET_RX_48000_256;
static struct adie_codec_action_unit handset_tx_48KHz_osr256_actions[] =
ADIE_HANDSET_TX_48000_256;
#if defined(CONFIG_NOISE_REDUCE_FOR_WIFI_ON)
static struct adie_codec_action_unit speaker_rx_48KHz_osr256_actions[] =
ADIE_DIFFERENTIAL_SPEAKER_RX_48000_256; // ADIE_SPEAKER_RX_48000_256
#else
static struct adie_codec_action_unit speaker_rx_48KHz_osr256_actions[] =
ADIE_SPEAKER_RX_48000_256;
#endif
static struct adie_codec_action_unit speaker_tx_48KHz_osr256_actions[] =
ADIE_SPEAKER_TX_48000_256;
static struct adie_codec_action_unit headset_rx_48KHz_osr256_actions[] =
ADIE_HEADSET_RX_48000_256;
static struct adie_codec_action_unit headset_tx_48KHz_osr256_actions[] =
ADIE_HEADSET_TX_48000_256;

// ------- DEFINITION OF VT CALL PAIRED DEVICES ------
#if defined(CONFIG_TARGET_LOCALE_KOR)
static struct adie_codec_action_unit handset_vt_rx_48KHz_osr256_actions[] =
ADIE_HANDSET_VT_RX_48000_256;
static struct adie_codec_action_unit handset_vt_tx_48KHz_osr256_actions[] =
ADIE_HANDSET_VT_TX_48000_256;
static struct adie_codec_action_unit speaker_vt_rx_48KHz_osr256_actions[] =
ADIE_SPEAKER_VT_RX_48000_256;
static struct adie_codec_action_unit speaker_vt_tx_48KHz_osr256_actions[] =
ADIE_SPEAKER_VT_TX_48000_256;
static struct adie_codec_action_unit headset_vt_rx_48KHz_osr256_actions[] =
ADIE_HEADSET_VT_RX_48000_256;
static struct adie_codec_action_unit headset_vt_tx_48KHz_osr256_actions[] =
ADIE_HEADSET_VT_TX_48000_256;
#else
static struct adie_codec_action_unit handset_vt_rx_48KHz_osr256_actions[] =
ADIE_HANDSET_RX_48000_256;
static struct adie_codec_action_unit handset_vt_tx_48KHz_osr256_actions[] =
ADIE_HANDSET_TX_48000_256;
static struct adie_codec_action_unit speaker_vt_rx_48KHz_osr256_actions[] =
ADIE_SPEAKER_RX_48000_256;
static struct adie_codec_action_unit speaker_vt_tx_48KHz_osr256_actions[] =
ADIE_SPEAKER_TX_48000_256;
static struct adie_codec_action_unit headset_vt_rx_48KHz_osr256_actions[] =
ADIE_HEADSET_RX_48000_256;
static struct adie_codec_action_unit headset_vt_tx_48KHz_osr256_actions[] =
ADIE_HEADSET_TX_48000_256;
#endif

// ------- DEFINITION OF VOIP CALL PAIRED DEVICES ------
#if 1
/* defined(CONFIG_USA_MODEL_SGH_I727) || defined(CONFIG_TARGET_LOCALE_KOR)
|| defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
|| defined(CONFIG_JPN_MODEL_SC_03D) || defined(CONFIG_USA_MODEL_SGH_I717) */
static struct adie_codec_action_unit handset_voip_rx_48KHz_osr256_actions[] =
ADIE_HANDSET_VOIP_RX_48000_256;
static struct adie_codec_action_unit handset_voip_tx_48KHz_osr256_actions[] =
ADIE_HANDSET_VOIP_TX_48000_256;
static struct adie_codec_action_unit speaker_voip_rx_48KHz_osr256_actions[] =
ADIE_SPEAKER_VOIP_RX_48000_256;
static struct adie_codec_action_unit speaker_voip_tx_48KHz_osr256_actions[] =
ADIE_SPEAKER_VOIP_TX_48000_256;
static struct adie_codec_action_unit headset_voip_rx_48KHz_osr256_actions[] =
ADIE_HEADSET_VOIP_RX_48000_256;
static struct adie_codec_action_unit headset_voip_tx_48KHz_osr256_actions[] =
ADIE_HEADSET_VOIP_TX_48000_256;
#else
static struct adie_codec_action_unit handset_voip_rx_48KHz_osr256_actions[] =
ADIE_HANDSET_RX_48000_256;
static struct adie_codec_action_unit handset_voip_tx_48KHz_osr256_actions[] =
ADIE_HANDSET_TX_48000_256;
static struct adie_codec_action_unit speaker_voip_rx_48KHz_osr256_actions[] =
ADIE_SPEAKER_RX_48000_256;
static struct adie_codec_action_unit speaker_voip_tx_48KHz_osr256_actions[] =
ADIE_SPEAKER_TX_48000_256;
static struct adie_codec_action_unit headset_voip_rx_48KHz_osr256_actions[] =
ADIE_HEADSET_RX_48000_256;
static struct adie_codec_action_unit headset_voip_tx_48KHz_osr256_actions[] =
ADIE_HEADSET_TX_48000_256;
#endif

// ------- DEFINITION OF VOIP CALL2 PAIRED DEVICES ------
#if 1
/* defined(CONFIG_USA_MODEL_SGH_I727) || defined(CONFIG_TARGET_LOCALE_KOR)
|| defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_T769)
|| defined(CONFIG_JPN_MODEL_SC_03D) || defined(CONFIG_USA_MODEL_SGH_I717) */
static struct adie_codec_action_unit handset_voip2_rx_48KHz_osr256_actions[] =
ADIE_HANDSET_VOIP2_RX_48000_256;
static struct adie_codec_action_unit handset_voip2_tx_48KHz_osr256_actions[] =
ADIE_HANDSET_VOIP2_TX_48000_256;
static struct adie_codec_action_unit speaker_voip2_rx_48KHz_osr256_actions[] =
ADIE_SPEAKER_VOIP2_RX_48000_256;
static struct adie_codec_action_unit speaker_voip2_tx_48KHz_osr256_actions[] =
ADIE_SPEAKER_VOIP2_TX_48000_256;
static struct adie_codec_action_unit headset_voip2_rx_48KHz_osr256_actions[] =
ADIE_HEADSET_VOIP2_RX_48000_256;
static struct adie_codec_action_unit headset_voip2_tx_48KHz_osr256_actions[] =
ADIE_HEADSET_VOIP2_TX_48000_256;
#else
static struct adie_codec_action_unit handset_voip2_rx_48KHz_osr256_actions[] =
ADIE_HANDSET_RX_48000_256;
static struct adie_codec_action_unit handset_voip2_tx_48KHz_osr256_actions[] =
ADIE_HANDSET_TX_48000_256;
static struct adie_codec_action_unit speaker_voip2_rx_48KHz_osr256_actions[] =
ADIE_SPEAKER_RX_48000_256;
static struct adie_codec_action_unit speaker_voip2_tx_48KHz_osr256_actions[] =
ADIE_SPEAKER_TX_48000_256;
static struct adie_codec_action_unit headset_voip2_rx_48KHz_osr256_actions[] =
ADIE_HEADSET_RX_48000_256;
static struct adie_codec_action_unit headset_voip2_tx_48KHz_osr256_actions[] =
ADIE_HEADSET_TX_48000_256;
#endif

// ------- DEFINITION OF VOIP CALL3 PAIRED DEVICES ------
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)\
|| defined(CONFIG_KOR_MODEL_SHV_E120L) || defined(CONFIG_Q1_KOR_AUDIO)
static struct adie_codec_action_unit handset_voip3_rx_48KHz_osr256_actions[] =
ADIE_HANDSET_VOIP3_RX_48000_256;
static struct adie_codec_action_unit handset_voip3_tx_48KHz_osr256_actions[] =
ADIE_HANDSET_VOIP3_TX_48000_256;
static struct adie_codec_action_unit speaker_voip3_rx_48KHz_osr256_actions[] =
ADIE_SPEAKER_VOIP3_RX_48000_256;
static struct adie_codec_action_unit speaker_voip3_tx_48KHz_osr256_actions[] =
ADIE_SPEAKER_VOIP3_TX_48000_256;
static struct adie_codec_action_unit headset_voip3_rx_48KHz_osr256_actions[] =
ADIE_HEADSET_VOIP3_RX_48000_256;
static struct adie_codec_action_unit headset_voip3_tx_48KHz_osr256_actions[] =
ADIE_HEADSET_VOIP3_TX_48000_256;
#endif

// ------- DEFINITION OF CALL PAIRED DEVICES ------
static struct adie_codec_action_unit handset_call_rx_48KHz_osr256_actions[] =
ADIE_HANDSET_CALL_RX_48000_256;
#ifndef CONFIG_VP_A2220
static struct adie_codec_action_unit handset_call_tx_48KHz_osr256_actions[] =
ADIE_HANDSET_CALL_TX_48000_256;
#endif
static struct adie_codec_action_unit speaker_call_rx_48KHz_osr256_actions[] =
ADIE_SPEAKER_CALL_RX_48000_256;
static struct adie_codec_action_unit speaker_call_tx_48KHz_osr256_actions[] =
ADIE_SPEAKER_CALL_TX_48000_256;

#if defined(CONFIG_KOR_MODEL_SHV_E110S) || defined(CONFIG_KOR_MODEL_SHV_E120S)\
|| defined(CONFIG_KOR_MODEL_SHV_E120K) || defined(CONFIG_Q1_KOR_AUDIO)
static struct adie_codec_action_unit speaker_loopback_tx_48KHz_osr256_actions[] =
ADIE_SPEAKER_LOOPBACK_TX_48000_256;
#endif

static struct adie_codec_action_unit headset_call_rx_48KHz_osr256_actions[] =
ADIE_HEADSET_CALL_RX_48000_256;
static struct adie_codec_action_unit headset_call_tx_48KHz_osr256_actions[] =
ADIE_HEADSET_CALL_TX_48000_256;

#if defined(CONFIG_KOR_MODEL_SHV_E110S) || defined(CONFIG_KOR_MODEL_SHV_E120S)\
|| defined(CONFIG_KOR_MODEL_SHV_E120K) || defined(CONFIG_Q1_KOR_AUDIO)\
|| defined(CONFIG_USA_MODEL_SGH_I727) || defined(CONFIG_USA_MODEL_SGH_I757)
static struct adie_codec_action_unit headset_loopback_tx_48KHz_osr256_actions[] =
ADIE_HEADSET_LOOPBACK_TX_48000_256;
#endif

#if 0
/* clean up source */
/* defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_KOR_MODEL_SHV_E110S)
|| defined(CONFIG_KOR_MODEL_SHV_E120L) || defined(CONFIG_JPN_MODEL_SC_03D) */
struct adie_codec_action_unit headset_rx_legacy_48KHz_osr256_actions[] =
ADIE_HEADSET_RX_48000_256_LEGACY;
struct adie_codec_action_unit headset_vt_rx_legacy_48KHz_osr256_actions[] =
ADIE_HEADSET_RX_48000_256_LEGACY;
struct adie_codec_action_unit headset_voip_rx_legacy_48KHz_osr256_actions[] =
ADIE_HEADSET_RX_48000_256_LEGACY;
struct adie_codec_action_unit fm_radio_headset_rx_legacy_48KHz_osr256_actions[] =
ADIE_HEADSET_RX_48000_256_LEGACY;
struct adie_codec_action_unit tty_headset_rx_legacy_48KHz_osr256_actions[] =
ADIE_HEADSET_RX_48000_256_LEGACY;
struct adie_codec_action_unit headset_call_rx_legacy_48KHz_osr256_actions[] =
ADIE_HEADSET_CALL_RX_48000_256_LEGACY;
struct adie_codec_action_unit speaker_headset_rx_legacy_48KHz_osr256_actions[] =
ADIE_SPEAKER_HEADSET_RX_48000_256_LEGACY;
#if defined(CONFIG_KOR_MODEL_SHV_E120L)
struct adie_codec_action_unit lineout_rx_legacy_48KHz_osr256_actions[] =
ADIE_LINEOUT_RX_48000_256_LEGACY;
#endif
#endif

// ------- DEFINITION OF SPECIAL DEVICES ------
#ifndef CONFIG_VP_A2220
static struct adie_codec_action_unit dualmic_handset_tx_48KHz_osr256_actions[] =
ADIE_HANDSET_TX_48000_256;
#endif
static struct adie_codec_action_unit dualmic_speaker_tx_48KHz_osr256_actions[] =
ADIE_SPEAKER_TX_48000_256;
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717)
static struct adie_codec_action_unit speaker_vr_tx_48KHz_osr256_actions[] =
ADIE_SPEAKER_VR_TX_48000_256;
static struct adie_codec_action_unit headset_vr_tx_48KHz_osr256_actions[] =
ADIE_HEADSET_VR_TX_48000_256;
#else
static struct adie_codec_action_unit speaker_vr_tx_48KHz_osr256_actions[] =
ADIE_SPEAKER_VOICE_SEARCH_TX_48000_256;
static struct adie_codec_action_unit headset_vr_tx_48KHz_osr256_actions[] =
ADIE_HEADSET_VOICE_SEARCH_TX_48000_256;
#endif
static struct adie_codec_action_unit fm_radio_headset_rx_48KHz_osr256_actions[] =
ADIE_HEADSET_RX_48000_256;
static struct adie_codec_action_unit fm_radio_speaker_rx_48KHz_osr256_actions[] =
ADIE_SPEAKER_RX_48000_256;

// ------- DEFINITION OF EXTERNAL DEVICES ------
#if defined(CONFIG_TARGET_LOCALE_KOR) || defined(CONFIG_EUR_MODEL_GT_I9210)
static struct adie_codec_action_unit lineout_rx_48KHz_osr256_actions[] =
ADIE_LINEOUT_RX_48000_256;
#else
#if defined(CONFIG_USA_MODEL_SGH_I727) || defined(CONFIG_USA_MODEL_SGH_T989)\
|| defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_USA_MODEL_SGH_I757)
static struct adie_codec_action_unit lineout_rx_48KHz_osr256_actions[] =
ADIE_DOCK_SPEAKER_HEADSET_RX_48000_256;
#else
static struct adie_codec_action_unit lineout_rx_48KHz_osr256_actions[] =
ADIE_HEADSET_RX_48000_256;
#endif
#endif
static struct adie_codec_action_unit tty_headset_rx_48KHz_osr256_actions[] =
ADIE_HEADSET_RX_48000_256;
static struct adie_codec_action_unit tty_headset_tx_48KHz_osr256_actions[] =
ADIE_HEADSET_TX_48000_256;
static struct adie_codec_action_unit speaker_headset_rx_48KHz_osr256_actions[] =
ADIE_SPEAKER_HEADSET_RX_48000_256;
static struct adie_codec_action_unit speaker_lineout_rx_48KHz_osr256_actions[] =
ADIE_SPEAKER_HEADSET_RX_48000_256; /* ADIE_SPEAKER_RX_48000_256; */

#if defined(CONFIG_USA_MODEL_SGH_T989)
static struct adie_codec_action_unit hac_handset_call_rx_48KHz_osr256_actions[] =
ADIE_HANDSET_CALL_RX_48000_256;
#endif

#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)\
|| defined(CONFIG_KOR_MODEL_SHV_E120L) || defined(CONFIG_Q1_KOR_AUDIO)
static struct adie_codec_action_unit camcoder_tx_48KHz_osr256_actions[] =
ADIE_CAMCODER_TX_48000_256;
#else
static struct adie_codec_action_unit camcoder_tx_48KHz_osr256_actions[] =
ADIE_SPEAKER_TX_48000_256;
#endif

#ifdef CONFIG_VP_A2220
static struct adie_codec_action_unit dualmic_handset_call_tx_48KHz_osr256_actions[] =
ADIE_DUALMIC_HANDSET_CALL_TX_48000_256;

static struct adie_codec_hwsetting_entry dualmic_handset_call_tx_settings[] = {
	{
		.freq_plan = AUDIO_FREQUENCY,
		.osr = 256,
		.actions = dualmic_handset_call_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(dualmic_handset_call_tx_48KHz_osr256_actions),
	}
};

static struct adie_codec_dev_profile dualmic_handset_call_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = dualmic_handset_call_tx_settings,
	.setting_sz = ARRAY_SIZE(dualmic_handset_call_tx_settings),
};
#endif	// #ifdef CONFIG_VP_A2220

// ------- DEFINITION OF NORMAL PAIRED DEVICES ------
static struct adie_codec_hwsetting_entry handset_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = handset_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry handset_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = handset_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = speaker_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = speaker_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry headset_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = headset_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry headset_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = headset_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_tx_48KHz_osr256_actions),
	}
};

// ------- DEFINITION OF VT CALL PAIRED DEVICES ------
static struct adie_codec_hwsetting_entry handset_vt_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = handset_vt_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_vt_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry handset_vt_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = handset_vt_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_vt_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_vt_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = speaker_vt_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_vt_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_vt_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = speaker_vt_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_vt_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry headset_vt_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = headset_vt_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_vt_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry headset_vt_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = headset_vt_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_vt_tx_48KHz_osr256_actions),
	}
};

// ------- DEFINITION OF VOIP CALL PAIRED DEVICES ------
static struct adie_codec_hwsetting_entry handset_voip_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = handset_voip_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_voip_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry handset_voip_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = handset_voip_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_voip_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_voip_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = speaker_voip_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_voip_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_voip_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = speaker_voip_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_voip_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry headset_voip_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = headset_voip_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_voip_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry headset_voip_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = headset_voip_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_voip_tx_48KHz_osr256_actions),
	}
};

// ------- DEFINITION OF VOIP CALL2 PAIRED DEVICES ------
static struct adie_codec_hwsetting_entry handset_voip2_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = handset_voip2_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_voip2_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry handset_voip2_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = handset_voip2_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_voip2_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_voip2_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = speaker_voip2_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_voip2_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_voip2_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = speaker_voip2_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_voip2_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry headset_voip2_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = headset_voip2_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_voip2_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry headset_voip2_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = headset_voip2_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_voip2_tx_48KHz_osr256_actions),
	}
};

// ------- DEFINITION OF VOIP CALL3 PAIRED DEVICES ------
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)\
|| defined(CONFIG_KOR_MODEL_SHV_E120L) || defined(CONFIG_Q1_KOR_AUDIO)
static struct adie_codec_hwsetting_entry handset_voip3_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = handset_voip3_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_voip3_rx_48KHz_osr256_actions),
	}
};

static struct adie_codec_hwsetting_entry handset_voip3_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = handset_voip3_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_voip3_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_voip3_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = speaker_voip3_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_voip3_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_voip3_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = speaker_voip3_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_voip3_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry headset_voip3_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = headset_voip3_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_voip3_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry headset_voip3_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = headset_voip3_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_voip3_tx_48KHz_osr256_actions),
	}
};
#endif

// ------- DEFINITION OF CALL PAIRED DEVICES ------
static struct adie_codec_hwsetting_entry handset_call_rx_settings[] = {
	{
		.freq_plan = AUDIO_FREQUENCY,
		.osr = 256,
		.actions = handset_call_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_call_rx_48KHz_osr256_actions),
	}
};
#ifndef CONFIG_VP_A2220
static struct adie_codec_hwsetting_entry handset_call_tx_settings[] = {
	{
		.freq_plan = AUDIO_FREQUENCY,
		.osr = 256,
		.actions = handset_call_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_call_tx_48KHz_osr256_actions),
	}
};
#endif
static struct adie_codec_hwsetting_entry speaker_call_rx_settings[] = {
	{
		.freq_plan = AUDIO_FREQUENCY,
		.osr = 256,
		.actions = speaker_call_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_call_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_call_tx_settings[] = {
	{
		.freq_plan = AUDIO_FREQUENCY,
		.osr = 256,
		.actions = speaker_call_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_call_tx_48KHz_osr256_actions),
	}
};

#if defined(CONFIG_KOR_MODEL_SHV_E110S)||defined(CONFIG_KOR_MODEL_SHV_E120S)\
|| defined(CONFIG_KOR_MODEL_SHV_E120K) || defined(CONFIG_Q1_KOR_AUDIO)
static struct adie_codec_hwsetting_entry speaker_loopback_tx_settings[] = {
	{
		.freq_plan = AUDIO_FREQUENCY,
		.osr = 256,
		.actions = speaker_loopback_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_loopback_tx_48KHz_osr256_actions),
	}
};
#endif


static struct adie_codec_hwsetting_entry headset_call_rx_settings[] = {
	{
		.freq_plan = AUDIO_FREQUENCY,
		.osr = 256,
		.actions = headset_call_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_call_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry headset_call_tx_settings[] = {
	{
		.freq_plan = AUDIO_FREQUENCY,
		.osr = 256,
		.actions = headset_call_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_call_tx_48KHz_osr256_actions),
	}
};

#if defined(CONFIG_KOR_MODEL_SHV_E110S) || defined(CONFIG_KOR_MODEL_SHV_E120S)\
|| defined(CONFIG_KOR_MODEL_SHV_E120K)|| defined(CONFIG_Q1_KOR_AUDIO)\
|| defined(CONFIG_USA_MODEL_SGH_I727)
static struct adie_codec_hwsetting_entry headset_loopback_tx_settings[] = {
	{
		.freq_plan = AUDIO_FREQUENCY,
		.osr = 256,
		.actions = headset_loopback_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_loopback_tx_48KHz_osr256_actions),
	}
};
#endif

// ------- DEFINITION OF SPECIAL DEVICES ------
#ifndef CONFIG_VP_A2220
static struct adie_codec_hwsetting_entry dualmic_handset_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = dualmic_handset_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(dualmic_handset_tx_48KHz_osr256_actions),
	}
};
#endif
static struct adie_codec_hwsetting_entry dualmic_speaker_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = dualmic_speaker_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(dualmic_speaker_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_vr_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = speaker_vr_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_vr_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry headset_vr_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = headset_vr_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_vr_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry fm_radio_headset_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = fm_radio_headset_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(fm_radio_headset_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry fm_radio_speaker_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = fm_radio_speaker_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(fm_radio_speaker_rx_48KHz_osr256_actions),
	}
};

// ------- DEFINITION OF EXTERNAL DEVICES ------
static struct adie_codec_hwsetting_entry lineout_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = lineout_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(lineout_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry tty_headset_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = tty_headset_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(tty_headset_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry tty_headset_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = tty_headset_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(tty_headset_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_headset_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = speaker_headset_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_headset_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_lineout_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = speaker_lineout_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_lineout_rx_48KHz_osr256_actions),
	}
};

#if defined(CONFIG_USA_MODEL_SGH_T989)
static struct adie_codec_hwsetting_entry hac_handset_call_rx_settings[] = {
	{
		.freq_plan = AUDIO_FREQUENCY,
		.osr = 256,
		.actions = hac_handset_call_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(hac_handset_call_rx_48KHz_osr256_actions),
	}
};
#endif

static struct adie_codec_hwsetting_entry camcoder_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = camcoder_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(camcoder_tx_48KHz_osr256_actions),
	}
};
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// ------- DEFINITION OF NORMAL PAIRED DEVICES ------
static struct adie_codec_dev_profile handset_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = handset_rx_settings,
	.setting_sz = ARRAY_SIZE(handset_rx_settings),
};
static struct adie_codec_dev_profile handset_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = handset_tx_settings,
	.setting_sz = ARRAY_SIZE(handset_tx_settings),
};
static struct adie_codec_dev_profile speaker_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = speaker_rx_settings,
	.setting_sz = ARRAY_SIZE(speaker_rx_settings),
};
static struct adie_codec_dev_profile speaker_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = speaker_tx_settings,
	.setting_sz = ARRAY_SIZE(speaker_tx_settings),
};
static struct adie_codec_dev_profile headset_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = headset_rx_settings,
	.setting_sz = ARRAY_SIZE(headset_rx_settings),
};
static struct adie_codec_dev_profile headset_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = headset_tx_settings,
	.setting_sz = ARRAY_SIZE(headset_tx_settings),
};

// ------- DEFINITION OF VT CALL PAIRED DEVICES ------
static struct adie_codec_dev_profile handset_vt_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = handset_vt_rx_settings,
	.setting_sz = ARRAY_SIZE(handset_vt_rx_settings),
};
static struct adie_codec_dev_profile handset_vt_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = handset_vt_tx_settings,
	.setting_sz = ARRAY_SIZE(handset_vt_tx_settings),
};
static struct adie_codec_dev_profile speaker_vt_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = speaker_vt_rx_settings,
	.setting_sz = ARRAY_SIZE(speaker_vt_rx_settings),
};
static struct adie_codec_dev_profile speaker_vt_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = speaker_vt_tx_settings,
	.setting_sz = ARRAY_SIZE(speaker_vt_tx_settings),
};
static struct adie_codec_dev_profile headset_vt_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = headset_vt_rx_settings,
	.setting_sz = ARRAY_SIZE(headset_vt_rx_settings),
};
static struct adie_codec_dev_profile headset_vt_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = headset_vt_tx_settings,
	.setting_sz = ARRAY_SIZE(headset_vt_tx_settings),
};

// ------- DEFINITION OF VOIP CALL PAIRED DEVICES ------
static struct adie_codec_dev_profile handset_voip_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = handset_voip_rx_settings,
	.setting_sz = ARRAY_SIZE(handset_voip_rx_settings),
};
static struct adie_codec_dev_profile handset_voip_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = handset_voip_tx_settings,
	.setting_sz = ARRAY_SIZE(handset_voip_tx_settings),
};
static struct adie_codec_dev_profile speaker_voip_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = speaker_voip_rx_settings,
	.setting_sz = ARRAY_SIZE(speaker_voip_rx_settings),
};
static struct adie_codec_dev_profile speaker_voip_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = speaker_voip_tx_settings,
	.setting_sz = ARRAY_SIZE(speaker_voip_tx_settings),
};
static struct adie_codec_dev_profile headset_voip_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = headset_voip_rx_settings,
	.setting_sz = ARRAY_SIZE(headset_voip_rx_settings),
};
static struct adie_codec_dev_profile headset_voip_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = headset_voip_tx_settings,
	.setting_sz = ARRAY_SIZE(headset_voip_tx_settings),
};

// ------- DEFINITION OF VOIP CALL2 PAIRED DEVICES ------
static struct adie_codec_dev_profile handset_voip2_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = handset_voip2_rx_settings,
	.setting_sz = ARRAY_SIZE(handset_voip2_rx_settings),
};
static struct adie_codec_dev_profile handset_voip2_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = handset_voip2_tx_settings,
	.setting_sz = ARRAY_SIZE(handset_voip2_tx_settings),
};
static struct adie_codec_dev_profile speaker_voip2_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = speaker_voip2_rx_settings,
	.setting_sz = ARRAY_SIZE(speaker_voip2_rx_settings),
};
static struct adie_codec_dev_profile speaker_voip2_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = speaker_voip2_tx_settings,
	.setting_sz = ARRAY_SIZE(speaker_voip2_tx_settings),
};
static struct adie_codec_dev_profile headset_voip2_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = headset_voip2_rx_settings,
	.setting_sz = ARRAY_SIZE(headset_voip2_rx_settings),
};
static struct adie_codec_dev_profile headset_voip2_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = headset_voip2_tx_settings,
	.setting_sz = ARRAY_SIZE(headset_voip2_tx_settings),
};

// ------- DEFINITION OF VOIP CALL3 PAIRED DEVICES ------
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)\
|| defined(CONFIG_KOR_MODEL_SHV_E120L) || defined(CONFIG_Q1_KOR_AUDIO)
static struct adie_codec_dev_profile handset_voip3_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = handset_voip3_rx_settings,
	.setting_sz = ARRAY_SIZE(handset_voip3_rx_settings),
};

static struct adie_codec_dev_profile handset_voip3_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = handset_voip3_tx_settings,
	.setting_sz = ARRAY_SIZE(handset_voip3_tx_settings),
};
static struct adie_codec_dev_profile speaker_voip3_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = speaker_voip3_rx_settings,
	.setting_sz = ARRAY_SIZE(speaker_voip3_rx_settings),
};
static struct adie_codec_dev_profile speaker_voip3_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = speaker_voip3_tx_settings,
	.setting_sz = ARRAY_SIZE(speaker_voip3_tx_settings),
};
static struct adie_codec_dev_profile headset_voip3_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = headset_voip3_rx_settings,
	.setting_sz = ARRAY_SIZE(headset_voip3_rx_settings),
};
static struct adie_codec_dev_profile headset_voip3_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = headset_voip3_tx_settings,
	.setting_sz = ARRAY_SIZE(headset_voip3_tx_settings),
};
#endif

// ------- DEFINITION OF CALL PAIRED DEVICES ------
static struct adie_codec_dev_profile handset_call_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = handset_call_rx_settings,
	.setting_sz = ARRAY_SIZE(handset_call_rx_settings),
};
#ifndef CONFIG_VP_A2220
static struct adie_codec_dev_profile handset_call_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = handset_call_tx_settings,
	.setting_sz = ARRAY_SIZE(handset_call_tx_settings),
};
#endif
static struct adie_codec_dev_profile speaker_call_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = speaker_call_rx_settings,
	.setting_sz = ARRAY_SIZE(speaker_call_rx_settings),
};
static struct adie_codec_dev_profile speaker_call_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = speaker_call_tx_settings,
	.setting_sz = ARRAY_SIZE(speaker_call_tx_settings),
};

#if defined(CONFIG_KOR_MODEL_SHV_E110S)|| defined(CONFIG_KOR_MODEL_SHV_E120S)\
|| defined(CONFIG_KOR_MODEL_SHV_E120K) || defined(CONFIG_Q1_KOR_AUDIO)
static struct adie_codec_dev_profile speaker_loopback_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = speaker_loopback_tx_settings,
	.setting_sz = ARRAY_SIZE(speaker_loopback_tx_settings),
};
#endif

static struct adie_codec_dev_profile headset_call_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = headset_call_rx_settings,
	.setting_sz = ARRAY_SIZE(headset_call_rx_settings),
};
static struct adie_codec_dev_profile headset_call_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = headset_call_tx_settings,
	.setting_sz = ARRAY_SIZE(headset_call_tx_settings),
};

#if defined(CONFIG_KOR_MODEL_SHV_E110S) || defined(CONFIG_KOR_MODEL_SHV_E120S)\
|| defined(CONFIG_KOR_MODEL_SHV_E120K) || defined(CONFIG_Q1_KOR_AUDIO)\
|| defined(CONFIG_USA_MODEL_SGH_I727)
static struct adie_codec_dev_profile headset_loopback_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = headset_loopback_tx_settings,
	.setting_sz = ARRAY_SIZE(headset_loopback_tx_settings),
};
#endif

// ------- DEFINITION OF SPECIAL DEVICES ------
#ifndef CONFIG_VP_A2220
static struct adie_codec_dev_profile dualmic_handset_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = dualmic_handset_tx_settings,
	.setting_sz = ARRAY_SIZE(dualmic_handset_tx_settings),
};
#endif
static struct adie_codec_dev_profile dualmic_speaker_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = dualmic_speaker_tx_settings,
	.setting_sz = ARRAY_SIZE(dualmic_speaker_tx_settings),
};
static struct adie_codec_dev_profile speaker_vr_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = speaker_vr_tx_settings,
	.setting_sz = ARRAY_SIZE(speaker_vr_tx_settings),
};
static struct adie_codec_dev_profile headset_vr_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = headset_vr_tx_settings,
	.setting_sz = ARRAY_SIZE(headset_vr_tx_settings),
};
static struct adie_codec_dev_profile fm_radio_headset_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = fm_radio_headset_rx_settings,
	.setting_sz = ARRAY_SIZE(fm_radio_headset_rx_settings),
};
static struct adie_codec_dev_profile fm_radio_speaker_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = fm_radio_speaker_rx_settings,
	.setting_sz = ARRAY_SIZE(fm_radio_speaker_rx_settings),
};

// ------- DEFINITION OF EXTERNAL DEVICES ------
static struct adie_codec_dev_profile lineout_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = lineout_rx_settings,
	.setting_sz = ARRAY_SIZE(lineout_rx_settings),
};
static struct adie_codec_dev_profile tty_headset_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = tty_headset_rx_settings,
	.setting_sz = ARRAY_SIZE(tty_headset_rx_settings),
};
static struct adie_codec_dev_profile tty_headset_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = tty_headset_tx_settings,
	.setting_sz = ARRAY_SIZE(tty_headset_tx_settings),
};
static struct adie_codec_dev_profile speaker_headset_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = speaker_headset_rx_settings,
	.setting_sz = ARRAY_SIZE(speaker_headset_rx_settings),
};

static struct adie_codec_dev_profile speaker_lineout_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = speaker_lineout_rx_settings,
	.setting_sz = ARRAY_SIZE(speaker_lineout_rx_settings),
};

#if defined(CONFIG_USA_MODEL_SGH_T989)
static struct adie_codec_dev_profile hac_handset_call_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = hac_handset_call_rx_settings,
	.setting_sz = ARRAY_SIZE(hac_handset_call_rx_settings),
};
#endif

static struct adie_codec_dev_profile camcoder_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = camcoder_tx_settings,
	.setting_sz = ARRAY_SIZE(camcoder_tx_settings),
};
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// ------- DEFINITION OF NORMAL PAIRED DEVICES ------
static struct snddev_icodec_data handset_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "handset_rx",
	.copp_id = 0,
	.profile = &handset_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)
/* temp block for build error || defined(CONFIG_USA_MODEL_SGH_I717) */
	.pamp_on = msm_snddev_poweramp_handset_on,
	.pamp_off = msm_snddev_poweramp_handset_off,
#endif
};

static struct snddev_icodec_data handset_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_tx",
	.copp_id = 1,
	.profile = &handset_tx_profile,
	/* .profile = &imic_profile, */
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
};

static struct snddev_icodec_data speaker_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_rx",
	.copp_id = 0,
	.profile = &speaker_rx_profile,
	/* .profile = &ispkr_stereo_profile, */
	.channel_mode = 2,
	/*
#ifdef AUDIENCE_BYPASS
	.default_sample_rate = 16000,
#else
	.default_sample_rate = 48000,
#endif
	*/
	.default_sample_rate = 48000,
#if defined (CONFIG_NOISE_REDUCE_FOR_WIFI_ON)
	.pamp_on = msm_snddev_differential_poweramp_on,
	.pamp_off = msm_snddev_differential_poweramp_off,
#else
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
#endif
};

static struct snddev_icodec_data speaker_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_tx_profile,
	/* .profile = &idmic_mono_profile, */
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
		/* msm_snddev_enable_dmic_power, */
	.pamp_off = msm_snddev_disable_amic_power,
		/* msm_snddev_disable_dmic_power, */
};

static struct snddev_icodec_data headset_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_rx",
	.copp_id = 0,
	.profile = &headset_rx_profile,
	/* .profile = &headset_ab_cpls_profile, */
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_poweramp_on_headset,
	.pamp_off = msm_snddev_poweramp_off_headset,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_icodec_data headset_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "headset_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &headset_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

static struct snddev_ecodec_data bt_sco_mono_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_nrec_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_nrec_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};
static struct snddev_ecodec_data bt_sco_mono_nrec_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_nrec_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_nrec_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_nrec_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};
static struct snddev_ecodec_data bt_sco_stereo_nrec_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_nrec_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

// ------- DEFINITION OF VT CALL PAIRED DEVICES ------
static struct snddev_icodec_data handset_vt_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "handset_vt_rx",
	.copp_id = 0,
	.profile = &handset_vt_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

static struct snddev_icodec_data handset_vt_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_vt_tx",
	.copp_id = 1,
	.profile = &handset_vt_tx_profile,
	/* .profile = &imic_profile, */
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
};

static struct snddev_icodec_data speaker_vt_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_vt_rx",
	.copp_id = 0,
	.profile = &speaker_vt_rx_profile,
	/* .profile = &ispkr_stereo_profile, */
	.channel_mode = 2,
	.default_sample_rate = 48000,
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_KOR_MODEL_SHV_E120S)\
|| defined(CONFIG_KOR_MODEL_SHV_E120K) || defined(CONFIG_KOR_MODEL_SHV_E120L)\
|| defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_USA_MODEL_SGH_T769)
	.pamp_on = msm_snddev_poweramp_on_call,
	.pamp_off = msm_snddev_poweramp_off_call,
#else
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
#endif
};

static struct snddev_icodec_data speaker_vt_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_vt_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_vt_tx_profile,
	/* .profile = &idmic_mono_profile, */
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_submic_power,
	.pamp_off = msm_snddev_disable_submic_power,
};

static struct snddev_icodec_data headset_vt_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_vt_rx",
	.copp_id = 0,
	.profile = &headset_vt_rx_profile,
	/* .profile = &headset_ab_cpls_profile, */
	.channel_mode = 2,
	.default_sample_rate = 48000,
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717)
	.pamp_on = msm_snddev_poweramp_on_headset_call,
	.pamp_off = msm_snddev_poweramp_off_headset_call,
#else
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)
	.pamp_on = msm_snddev_poweramp_on_call_headset,
	.pamp_off = msm_snddev_poweramp_off_call_headset,
#else
	.pamp_on = msm_snddev_poweramp_on_headset,
	.pamp_off = msm_snddev_poweramp_off_headset,
#endif
#endif
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_icodec_data headset_vt_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "headset_vt_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &headset_vt_tx_profile,
	/* .profile = &iheadset_mic_profile, */
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

static struct snddev_ecodec_data bt_sco_mono_vt_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_vt_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_vt_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_vt_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_nrec_vt_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_nrec_vt_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_nrec_vt_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_nrec_vt_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_vt_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_vt_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_vt_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_vt_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_nrec_vt_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_nrec_vt_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_nrec_vt_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_nrec_vt_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_icodec_data deskdock_vt_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "deskdock_vt_rx",
	.copp_id = 0,
	.profile = &speaker_vt_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)
	.pamp_on = msm_snddev_poweramp_on_call,
	.pamp_off = msm_snddev_poweramp_off_call,
#else
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
#endif
};

static struct snddev_icodec_data deskdock_vt_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "deskdock_vt_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_vt_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_submic_power,
	.pamp_off = msm_snddev_disable_submic_power,
};

// ------- DEFINITION OF VOIP CALL PAIRED DEVICES ------
static struct snddev_icodec_data handset_voip_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "handset_voip_rx",
	.copp_id = 0,
	.profile = &handset_voip_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

static struct snddev_icodec_data handset_voip_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_voip_tx",
	.copp_id = 1,
	.profile = &handset_voip_tx_profile,
	/* .profile = &imic_profile, */
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_voip_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
};

static struct snddev_icodec_data speaker_voip_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_voip_rx",
	.copp_id = 0,
	.profile = &speaker_voip_rx_profile,
	/* .profile = &ispkr_stereo_profile, */
	.channel_mode = 2,
	.default_sample_rate = 48000,
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_KOR_MODEL_SHV_E120S)\
|| defined(CONFIG_KOR_MODEL_SHV_E120K) || defined(CONFIG_KOR_MODEL_SHV_E120L)\
|| defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_USA_MODEL_SGH_T769)
	.pamp_on = msm_snddev_poweramp_on_call,
	.pamp_off = msm_snddev_poweramp_off_call,
#else
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
#endif
};

static struct snddev_icodec_data speaker_voip_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_voip_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_voip_tx_profile,
	/* .profile = &idmic_mono_profile, */
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_submic_power,
	.pamp_off = msm_snddev_disable_submic_power,
};

static struct snddev_icodec_data headset_voip_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_voip_rx",
	.copp_id = 0,
	.profile = &headset_voip_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717)
	.pamp_on = msm_snddev_poweramp_on_headset_call,
	.pamp_off = msm_snddev_poweramp_off_headset_call,
#else
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)
	.pamp_on = msm_snddev_poweramp_on_call_headset,
	.pamp_off = msm_snddev_poweramp_off_call_headset,
#else
	.pamp_on = msm_snddev_poweramp_on_headset,
	.pamp_off = msm_snddev_poweramp_off_headset,
#endif
#endif
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_icodec_data headset_voip_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "headset_voip_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &headset_voip_tx_profile,
	/* .profile = &iheadset_mic_profile, */
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

static struct snddev_ecodec_data bt_sco_mono_voip_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_voip_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_voip_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_voip_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_nrec_voip_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_nrec_voip_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_nrec_voip_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_nrec_voip_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_voip_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_voip_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_voip_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_voip_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_nrec_voip_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_nrec_voip_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_nrec_voip_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_nrec_voip_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_icodec_data deskdock_voip_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "deskdock_voip_rx",
	.copp_id = 0,
	.profile = &speaker_voip_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)
	.pamp_on = msm_snddev_poweramp_on_call,
	.pamp_off = msm_snddev_poweramp_off_call,
#else
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
#endif
};

static struct snddev_icodec_data deskdock_voip_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "deskdock_voip_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_voip_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_submic_power,
	.pamp_off = msm_snddev_disable_submic_power,
};

// ------- DEFINITION OF CALL PAIRED DEVICES ------
static struct snddev_icodec_data handset_call_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "handset_call_rx",
	.copp_id = 0,
	.profile = &handset_call_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_VP_A2220
	.pamp_on = msm_snddev_setting_audience_call_connect,
	.pamp_off = msm_snddev_setting_audience_call_disconnect,
#endif
};

static struct snddev_icodec_data handset_call_tx_data = {
#ifdef CONFIG_VP_A2220
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_call_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &dualmic_handset_call_tx_profile,
	.channel_mode = 2,
	.default_sample_rate = 16000,
	.pamp_on = msm_snddev_enable_amic_power,
		/* msm_snddev_enable_audience_amic_power, */
	.pamp_off = msm_snddev_disable_amic_power,
		/* msm_snddev_disable_audience_amic_power, */
#else
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_call_tx",
	.copp_id = 1,
	.profile = &handset_call_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#endif
};

static struct snddev_icodec_data speaker_call_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_call_rx",
	.copp_id = 0,
	.profile = &speaker_call_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_VP_A2220
	.pamp_on = msm_snddev_audience_speaker_on,
	.pamp_off = msm_snddev_audience_speaker_off,
#else
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)\
|| defined(CONFIG_KOR_MODEL_SHV_E120L) || defined(CONFIG_JPN_MODEL_SC_03D)
	.pamp_on = msm_snddev_poweramp_on_call,
	.pamp_off = msm_snddev_poweramp_off_call,
#else
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
#endif
#endif
};

static struct snddev_icodec_data speaker_call_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_call_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_call_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_USA_MODEL_SGH_T769
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#else
	.pamp_on = msm_snddev_enable_submic_power,
		/* msm_snddev_enable_dmic_power, */
	.pamp_off =  msm_snddev_disable_submic_power,
		/* msm_snddev_disable_dmic_power, */
#endif
};


static struct snddev_icodec_data deskdock_call_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "deskdock_call_rx",
	.copp_id = 0,
	.profile = &speaker_call_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_VP_A2220
	.pamp_on = msm_snddev_audience_speaker_on,
	.pamp_off = msm_snddev_audience_speaker_off,
#else
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
#endif
};

static struct snddev_icodec_data deskdock_call_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "deskdock_call_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_call_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_USA_MODEL_SGH_T769
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#else
	.pamp_on = msm_snddev_enable_submic_power,
		/* msm_snddev_enable_dmic_power, */
	.pamp_off =  msm_snddev_disable_submic_power,
		/* msm_snddev_disable_dmic_power, */
#endif
};

static struct snddev_icodec_data headset_call_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_call_rx",
	.copp_id = 0,
	.profile = &headset_call_rx_profile,
	.channel_mode = 2,
#ifdef CONFIG_VP_A2220
	.default_sample_rate = 16000,
	.pamp_on = msm_snddev_audience_poweramp_on_headset,
	.pamp_off = msm_snddev_audience_poweramp_off_headset,
#else
	/* To modify noise 48000 -> 8000 */
	.default_sample_rate = 8000,
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)\
|| defined(CONFIG_KOR_MODEL_SHV_E120L)
	.pamp_on = msm_snddev_poweramp_on_call_headset,
	.pamp_off = msm_snddev_poweramp_off_call_headset,
#else
	.pamp_on = msm_snddev_poweramp_on_headset,
	.pamp_off = msm_snddev_poweramp_off_headset,
#endif
#endif
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_icodec_data headset_call_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "headset_call_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &headset_call_tx_profile,
	.channel_mode = 1,
#ifdef CONFIG_VP_A2220
	.default_sample_rate = 16000,
#else
	/* To modify noise 48000 -> 8000 */
	.default_sample_rate = 8000,
#endif
};

static struct snddev_icodec_data headset_loopback_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_loopback_rx",
	.copp_id = 0,
	.profile = &headset_call_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_VP_A2220
	.pamp_on = msm_snddev_audience_poweramp_on_headset,
	.pamp_off = msm_snddev_audience_poweramp_off_headset,
#else
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K) || defined(CONFIG_KOR_MODEL_SHV_E120L)
	.pamp_on = msm_snddev_poweramp_on_call_headset,
	.pamp_off = msm_snddev_poweramp_off_call_headset,
#else
	.pamp_on = msm_snddev_poweramp_on_headset,
	.pamp_off = msm_snddev_poweramp_off_headset,
#endif
#endif
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_icodec_data headset_loopback_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "headset_loopback_tx",
	.copp_id = PRIMARY_I2S_TX,
#if defined(CONFIG_KOR_MODEL_SHV_E110S) || defined(CONFIG_KOR_MODEL_SHV_E120S)\
|| defined(CONFIG_KOR_MODEL_SHV_E120K) || defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_Q1_KOR_AUDIO)// && !defined(CONFIG_KOR_MODEL_SHV_E160L))
	.profile = &headset_loopback_tx_profile,
#else
	.profile = &headset_call_tx_profile,
#endif
	.channel_mode = 1,
	.default_sample_rate = AUDIO_FREQUENCY,
};

static struct snddev_ecodec_data bt_sco_mono_call_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_call_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_call_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_call_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_nrec_call_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_nrec_call_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};
static struct snddev_ecodec_data bt_sco_mono_nrec_call_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_nrec_call_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_call_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_call_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_call_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_call_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_nrec_call_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_nrec_call_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_nrec_call_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_nrec_call_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

// ------- DEFINITION OF SPECIAL DEVICES ------
static struct snddev_icodec_data dualmic_handset_tx_data = {
#ifdef CONFIG_VP_A2220
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "dualmic_handset_ef_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &dualmic_handset_call_tx_profile,
	.channel_mode = 2,
	.default_sample_rate = 16000,
	.pamp_on = msm_snddev_enable_audience_amic_power,
	.pamp_off = msm_snddev_disable_audience_amic_power,
#else
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "dualmic_handset_ef_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &dualmic_handset_tx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#endif
};

static struct snddev_icodec_data dualmic_speaker_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "dualmic_speaker_ef_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &dualmic_speaker_tx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
#ifdef CONFIG_USA_MODEL_SGH_T769
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#else
	.pamp_on = msm_snddev_enable_submic_power,
	.pamp_off = msm_snddev_disable_submic_power,
#endif
};

static struct snddev_icodec_data speaker_vr_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_vr_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_vr_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
};

static struct snddev_icodec_data headset_vr_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "headset_vr_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &headset_vr_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

static struct snddev_ecodec_data bt_sco_vr_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_vr_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_nrec_vr_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_nrec_vr_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_icodec_data fm_radio_headset_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_FM),
	.name = "fm_radio_headset_rx",
	.copp_id = 0,
	.profile = &fm_radio_headset_rx_profile,
		/* headset_fmradio_rx_profile, */
	.channel_mode = 1,
	.default_sample_rate = 8000,
	.pamp_on = msm_snddev_poweramp_on_headset,
	.pamp_off = msm_snddev_poweramp_off_headset,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_icodec_data fm_radio_speaker_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_FM),
	.name = "fm_radio_speaker_rx",
	.copp_id = 0,
	.profile = &fm_radio_speaker_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 8000,
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
};

static struct snddev_mi2s_data fm_radio_tx_data = {
	.capability = SNDDEV_CAP_TX ,
	.name = "fm_radio_tx",
	.copp_id = PRIMARY_I2S_TX,
	.channel_mode = 2,
	.sd_lines = MI2S_SD3,
	.sample_rate = 48000,
};

// ------- DEFINITION OF EXTERNAL DEVICES ------
static struct snddev_hdmi_data hdmi_stereo_rx_data = {
	.capability = SNDDEV_CAP_RX ,
	.name = "hdmi_rx",
	.copp_id = HDMI_RX,
	.channel_mode = 0,
	.default_sample_rate = 48000,
};

static struct snddev_icodec_data lineout_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "lineout_rx",
	.copp_id = 0,
	.profile = &lineout_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I757)
	.pamp_on = msm_snddev_vpsamp_on_headset,
	.pamp_off = msm_snddev_vpsramp_off_headset,
#elif defined(CONFIG_TARGET_LOCALE_KOR)
	.pamp_on = msm_snddev_poweramp_on_lineout,
	.pamp_off = msm_snddev_poweramp_off_lineout,
#elif defined(CONFIG_EUR_MODEL_GT_I9210)
	.pamp_on = msm_snddev_poweramp_on_lineout_I9210,
	.pamp_off = msm_snddev_poweramp_off_lineout_I9210,
#else
	.pamp_on = msm_snddev_poweramp_on_headset,
	.pamp_off = msm_snddev_poweramp_off_headset,
#endif
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_icodec_data tty_headset_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE | SNDDEV_CAP_TTY ),
	.name = "tty_headset_rx",
	.copp_id = 0,
	.profile = &tty_headset_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717)
	.pamp_on = msm_snddev_poweramp_on_headset_call,
	.pamp_off = msm_snddev_poweramp_off_headset_call,
#else
	.pamp_on = msm_snddev_poweramp_on_headset,
	.pamp_off = msm_snddev_poweramp_off_headset,
#endif
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_icodec_data tty_headset_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE | SNDDEV_CAP_TTY),
	.name = "tty_headset_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &tty_headset_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

static struct snddev_icodec_data speaker_headset_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_headset_rx",
	.copp_id = 0,
	.profile = &speaker_headset_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_poweramp_on_together,
	.pamp_off = msm_snddev_poweramp_off_together,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_icodec_data speaker_lineout_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_lineout_rx",
	.copp_id = 0,
	.profile = &speaker_lineout_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_poweramp_on_together,
	.pamp_off = msm_snddev_poweramp_off_together,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_hdmi_data speaker_hdmi_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_hdmi_rx",
	.copp_id = HDMI_RX,
	.channel_mode = 0,
	.default_sample_rate = 48000,
};

#if defined(CONFIG_USA_MODEL_SGH_T989)
static struct snddev_icodec_data hac_handset_call_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "hac_handset_call_rx",
	.copp_id = 0,
	.profile = &hac_handset_call_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_VP_A2220
	.pamp_on = msm_snddev_setting_audience_call_connect,
	.pamp_off = msm_snddev_setting_audience_call_disconnect,
#endif
};
#endif

static struct snddev_icodec_data camcoder_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "camcoder_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &camcoder_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
#ifdef CONFIG_USA_MODEL_SGH_T769
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#else
	.pamp_on = msm_snddev_enable_submic_power,
	.pamp_off = msm_snddev_disable_submic_power,
#endif
};

// ------- DEFINITION OF CALL2 PAIRED DEVICES ------
static struct snddev_icodec_data handset_call2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "handset_call2_rx",
	.copp_id = 0,
	.profile = &handset_call_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_VP_A2220
	.pamp_on = msm_snddev_setting_audience_call_connect,
	.pamp_off = msm_snddev_setting_audience_call_disconnect,
#endif
};

static struct snddev_icodec_data handset_call2_tx_data = {
#ifdef CONFIG_VP_A2220
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_call2_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &dualmic_handset_call_tx_profile,
	.channel_mode = 2,
	.default_sample_rate = 16000,
	.pamp_on = msm_snddev_enable_amic_power,
		/* msm_snddev_enable_audience_amic_power, */
	.pamp_off = msm_snddev_disable_amic_power,
		/* msm_snddev_disable_audience_amic_power, */
#else
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_call2_tx",
	.copp_id = 1,
	.profile = &handset_call_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#endif
};

static struct snddev_icodec_data speaker_call2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_call2_rx",
	.copp_id = 0,
	.profile = &speaker_call_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_VP_A2220
	.pamp_on = msm_snddev_audience_speaker_on,
	.pamp_off = msm_snddev_audience_speaker_off,
#else
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)\
|| defined(CONFIG_KOR_MODEL_SHV_E120L)
	.pamp_on = msm_snddev_poweramp_on_call,
	.pamp_off = msm_snddev_poweramp_off_call,
#else
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
#endif

#endif
};

static struct snddev_icodec_data speaker_call2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_call2_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_call_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_USA_MODEL_SGH_T769
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#else
	.pamp_on = msm_snddev_enable_submic_power,
		/* msm_snddev_enable_dmic_power, */
	.pamp_off =  msm_snddev_disable_submic_power,
		/* msm_snddev_disable_dmic_power, */
#endif
};

static struct snddev_icodec_data headset_call2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_call2_rx",
	.copp_id = 0,
	.profile = &headset_call_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_VP_A2220
	.pamp_on = msm_snddev_audience_poweramp_on_headset,
	.pamp_off = msm_snddev_audience_poweramp_off_headset,
#else
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)\
|| defined(CONFIG_KOR_MODEL_SHV_E120L)
	.pamp_on = msm_snddev_poweramp_on_call_headset,
	.pamp_off = msm_snddev_poweramp_off_call_headset,
#else
	.pamp_on = msm_snddev_poweramp_on_headset,
	.pamp_off = msm_snddev_poweramp_off_headset,
#endif
#endif
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_icodec_data headset_call2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "headset_call2_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &headset_call_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = AUDIO_FREQUENCY,
};

static struct snddev_ecodec_data bt_sco_mono_call2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_call2_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_call2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_call2_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_nrec_call2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_nrec_call2_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_nrec_call2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_nrec_call2_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_call2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_call2_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_call2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_call2_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_nrec_call2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_nrec_call2_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_nrec_call2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_nrec_call2_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};


static struct snddev_icodec_data deskdock_call2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "deskdock_call2_rx",
	.copp_id = 0,
	.profile = &speaker_call_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_VP_A2220
	.pamp_on = msm_snddev_audience_speaker_on,
	.pamp_off = msm_snddev_audience_speaker_off,
#else
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
#endif
};

static struct snddev_icodec_data deskdock_call2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "deskdock_call2_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_call_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_USA_MODEL_SGH_T769
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#else
	.pamp_on = msm_snddev_enable_submic_power,
		/* msm_snddev_enable_dmic_power, */
	.pamp_off =  msm_snddev_disable_submic_power,
		/* msm_snddev_disable_dmic_power, */
#endif
};

// ------- DEFINITION OF VOIP CALL2 PAIRED DEVICES ------
static struct snddev_icodec_data handset_voip2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "handset_voip2_rx",
	.copp_id = 0,
	.profile = &handset_voip2_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

static struct snddev_icodec_data handset_voip2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_voip2_tx",
	.copp_id = 1,
	.profile = &handset_voip2_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_voip_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
};

static struct snddev_icodec_data speaker_voip2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_voip2_rx",
	.copp_id = 0,
	.profile = &speaker_voip2_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_KOR_MODEL_SHV_E120S)\
|| defined(CONFIG_KOR_MODEL_SHV_E120K) || defined(CONFIG_KOR_MODEL_SHV_E120L)
	.pamp_on = msm_snddev_poweramp_on_call,
	.pamp_off = msm_snddev_poweramp_off_call,
#else
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
#endif
};

static struct snddev_icodec_data speaker_voip2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_voip2_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_voip2_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
#ifdef CONFIG_USA_MODEL_SGH_T769
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#else
	.pamp_on = msm_snddev_enable_submic_power,
	.pamp_off = msm_snddev_disable_submic_power,
#endif
};

static struct snddev_icodec_data headset_voip2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_voip2_rx",
	.copp_id = 0,
	.profile = &headset_voip2_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717)
	.pamp_on = msm_snddev_poweramp_on_headset_call,
	.pamp_off = msm_snddev_poweramp_off_headset_call,
#else
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)
	.pamp_on = msm_snddev_poweramp_on_call_headset,
	.pamp_off = msm_snddev_poweramp_off_call_headset,
#else
	.pamp_on = msm_snddev_poweramp_on_headset,
	.pamp_off = msm_snddev_poweramp_off_headset,
#endif
#endif
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_icodec_data headset_voip2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "headset_voip2_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &headset_voip2_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

static struct snddev_ecodec_data bt_sco_mono_voip2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_voip2_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_voip2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_voip2_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_nrec_voip2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_nrec_voip2_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_nrec_voip2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_nrec_voip2_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_voip2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_voip2_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_voip2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_voip2_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_nrec_voip2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_nrec_voip2_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_nrec_voip2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_nrec_voip2_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_icodec_data deskdock_voip2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "deskdock_voip2_rx",
	.copp_id = 0,
	.profile = &speaker_voip2_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)
	.pamp_on = msm_snddev_poweramp_on_call,
	.pamp_off = msm_snddev_poweramp_off_call,
#else
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
#endif
};

static struct snddev_icodec_data deskdock_voip2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "deskdock_voip2_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_voip2_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
#ifdef CONFIG_USA_MODEL_SGH_T769
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#else
	.pamp_on = msm_snddev_enable_submic_power,
	.pamp_off = msm_snddev_disable_submic_power,
#endif
};


// ------- DEFINITION OF VOIP CALL3 PAIRED DEVICES ------
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)\
|| defined(CONFIG_KOR_MODEL_SHV_E120L) || defined(CONFIG_Q1_KOR_AUDIO)
static struct snddev_icodec_data handset_voip3_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "handset_voip3_rx",
	.copp_id = 0,
	.profile = &handset_voip3_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

static struct snddev_icodec_data handset_voip3_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_voip3_tx",
	.copp_id = 1,
	.profile = &handset_voip3_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_voip_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
};

static struct snddev_icodec_data speaker_voip3_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_voip3_rx",
	.copp_id = 0,
	.profile = &speaker_voip3_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_KOR_MODEL_SHV_E120S)\
|| defined(CONFIG_KOR_MODEL_SHV_E120K) || defined(CONFIG_KOR_MODEL_SHV_E120L)\
|| defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_USA_MODEL_SGH_T769)
	.pamp_on = msm_snddev_poweramp_on_call,
	.pamp_off = msm_snddev_poweramp_off_call,
#else
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
#endif
};

static struct snddev_icodec_data speaker_voip3_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_voip3_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_voip3_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
#ifdef CONFIG_USA_MODEL_SGH_T769
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#else
	.pamp_on = msm_snddev_enable_submic_power,
	.pamp_off = msm_snddev_disable_submic_power,
#endif
};

static struct snddev_icodec_data headset_voip3_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_voip3_rx",
	.copp_id = 0,
	.profile = &headset_voip3_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_USA_MODEL_SGH_I757)\
|| defined(CONFIG_USA_MODEL_SGH_T769)
	.pamp_on = msm_snddev_poweramp_on_headset_call,
	.pamp_off = msm_snddev_poweramp_off_headset_call,
#else
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)
	.pamp_on = msm_snddev_poweramp_on_call_headset,
	.pamp_off = msm_snddev_poweramp_off_call_headset,
#else
	.pamp_on = msm_snddev_poweramp_on_headset,
	.pamp_off = msm_snddev_poweramp_off_headset,
#endif
#endif
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_icodec_data headset_voip3_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "headset_voip3_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &headset_voip3_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

static struct snddev_ecodec_data bt_sco_mono_voip3_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_voip3_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_voip3_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_voip3_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_nrec_voip3_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_nrec_voip3_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_mono_nrec_voip3_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_mono_nrec_voip3_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_voip3_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_voip3_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_voip3_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_voip3_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_nrec_voip3_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_nrec_voip3_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data bt_sco_stereo_nrec_voip3_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_stereo_nrec_voip3_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

static struct snddev_icodec_data deskdock_voip3_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "deskdock_voip3_rx",
	.copp_id = 0,
	.profile = &speaker_voip3_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
#if defined(CONFIG_USA_MODEL_SGH_T989) || defined(CONFIG_USA_MODEL_SGH_I727)\
|| defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_USA_MODEL_SGH_T769)
	.pamp_on = msm_snddev_poweramp_on_call,
	.pamp_off = msm_snddev_poweramp_off_call,
#else
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
#endif
};

static struct snddev_icodec_data deskdock_voip3_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "deskdock_voip3_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_voip3_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
#ifdef CONFIG_USA_MODEL_SGH_T769
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#else
	.pamp_on = msm_snddev_enable_submic_power,
	.pamp_off = msm_snddev_disable_submic_power,
#endif
};
#endif

// ------- DEFINITION OF LOOPBACK PAIRED DEVICES ------
static struct snddev_icodec_data handset_loopback_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "handset_loopback_rx",
	.copp_id = 0,
	.profile = &handset_call_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_VP_A2220
#if defined(CONFIG_EUR_MODEL_GT_I9210)
	.pamp_on = msm_snddev_setting_audience_loopback_on,
	.pamp_off = msm_snddev_setting_audience_loopback_off,
#else
	.pamp_on = msm_snddev_setting_audience_call_connect,
	.pamp_off = msm_snddev_setting_audience_call_disconnect,
#endif
#endif
};

static struct snddev_icodec_data handset_loopback_tx_data = {
#ifdef CONFIG_VP_A2220
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_loopback_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &dualmic_handset_call_tx_profile,
	.channel_mode = 2,
	.default_sample_rate = 16000,
	.pamp_on = msm_snddev_enable_amic_power,
		/* msm_snddev_enable_audience_amic_power, */
	.pamp_off = msm_snddev_disable_amic_power,
		/* msm_snddev_disable_audience_amic_power, */
#else
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_loopback_tx",
	.copp_id = 1,
	.profile = &handset_call_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#endif
};

static struct snddev_icodec_data speaker_loopback_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_loopback_rx",
	.copp_id = 0,
	.profile = &speaker_call_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_VP_A2220
	.pamp_on = msm_snddev_audience_speaker_on,
	.pamp_off = msm_snddev_audience_speaker_off,
#else
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
#endif
};

static struct snddev_icodec_data speaker_loopback_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_loopback_tx",
	.copp_id = PRIMARY_I2S_TX,
#if defined(CONFIG_KOR_MODEL_SHV_E110S)|| defined(CONFIG_KOR_MODEL_SHV_E120S)\
|| defined(CONFIG_KOR_MODEL_SHV_E120K) || defined(CONFIG_Q1_KOR_AUDIO)
	.profile = &speaker_loopback_tx_profile,
#else
	.profile = &speaker_call_tx_profile,
#endif
	.channel_mode = 1,
	.default_sample_rate = AUDIO_FREQUENCY,
#ifdef CONFIG_USA_MODEL_SGH_T769
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#else
	.pamp_on = msm_snddev_enable_submic_power,
		/* msm_snddev_enable_dmic_power, */
	.pamp_off =  msm_snddev_disable_submic_power,
		/* msm_snddev_disable_dmic_power, */
#endif
};
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// ------- DEFINITION OF NORMAL PAIRED DEVICES ------
static struct platform_device device_handset_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_rx_data },
};
static struct platform_device device_handset_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_tx_data },
};
static struct platform_device device_speaker_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_rx_data },
};
static struct platform_device device_speaker_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_tx_data },
};
static struct platform_device device_headset_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_rx_data },
};
static struct platform_device device_headset_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_tx_data },
};

static struct platform_device device_bt_sco_mono_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_rx_data },
};
static struct platform_device device_bt_sco_mono_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_tx_data },
};
static struct platform_device device_bt_sco_mono_nrec_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_nrec_rx_data },
};
static struct platform_device device_bt_sco_mono_nrec_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_nrec_tx_data },
};
static struct platform_device device_bt_sco_stereo_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_rx_data },
};
static struct platform_device device_bt_sco_stereo_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_tx_data },
};
static struct platform_device device_bt_sco_stereo_nrec_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_nrec_rx_data },
};
static struct platform_device device_bt_sco_stereo_nrec_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_nrec_tx_data },
};


// ------- DEFINITION OF VT CALL PAIRED DEVICES ------
static struct platform_device device_handset_vt_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_vt_rx_data },
};
static struct platform_device device_handset_vt_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_vt_tx_data },
};
static struct platform_device device_speaker_vt_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_vt_rx_data },
};
static struct platform_device device_speaker_vt_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_vt_tx_data },
};
static struct platform_device device_headset_vt_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_vt_rx_data },
};
static struct platform_device device_headset_vt_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_vt_tx_data },
};

static struct platform_device device_bt_sco_mono_vt_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_vt_rx_data },
};
static struct platform_device device_bt_sco_mono_vt_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_vt_tx_data },
};
static struct platform_device device_bt_sco_mono_nrec_vt_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_nrec_vt_rx_data },
};
static struct platform_device device_bt_sco_mono_nrec_vt_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_nrec_vt_tx_data },
};
static struct platform_device device_bt_sco_stereo_vt_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_vt_rx_data },
};
static struct platform_device device_bt_sco_stereo_vt_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_vt_tx_data },
};
static struct platform_device device_bt_sco_stereo_nrec_vt_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_nrec_vt_rx_data },
};
static struct platform_device device_bt_sco_stereo_nrec_vt_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_nrec_vt_tx_data },
};

static struct platform_device device_deskdock_vt_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &deskdock_vt_rx_data },
};
static struct platform_device device_deskdock_vt_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &deskdock_vt_tx_data },
};

// ------- DEFINITION OF VOIP CALL PAIRED DEVICES ------
static struct platform_device device_handset_voip_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_voip_rx_data },
};
static struct platform_device device_handset_voip_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_voip_tx_data },
};
static struct platform_device device_speaker_voip_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_voip_rx_data },
};
static struct platform_device device_speaker_voip_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_voip_tx_data },
};
static struct platform_device device_headset_voip_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_voip_rx_data },
};
static struct platform_device device_headset_voip_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_voip_tx_data },
};

static struct platform_device device_bt_sco_mono_voip_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_voip_rx_data },
};
static struct platform_device device_bt_sco_mono_voip_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_voip_tx_data },
};
static struct platform_device device_bt_sco_mono_nrec_voip_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_nrec_voip_rx_data },
};
static struct platform_device device_bt_sco_mono_nrec_voip_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_nrec_voip_tx_data },
};
static struct platform_device device_bt_sco_stereo_voip_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_voip_rx_data },
};
static struct platform_device device_bt_sco_stereo_voip_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_voip_tx_data },
};
static struct platform_device device_bt_sco_stereo_nrec_voip_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_nrec_voip_rx_data },
};
static struct platform_device device_bt_sco_stereo_nrec_voip_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_nrec_voip_tx_data },
};

static struct platform_device device_deskdock_voip_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &deskdock_voip_rx_data },
};
static struct platform_device device_deskdock_voip_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &deskdock_voip_tx_data },
};

// ------- DEFINITION OF CALL PAIRED DEVICES ------
static struct platform_device device_handset_call_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_call_rx_data },
};
static struct platform_device device_handset_call_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_call_tx_data },
};
static struct platform_device device_speaker_call_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_call_rx_data },
};
static struct platform_device device_speaker_call_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_call_tx_data },
};

static struct platform_device device_deskdock_call_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &deskdock_call_rx_data },
};
static struct platform_device device_deskdock_call_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &deskdock_call_tx_data },
};

static struct platform_device device_headset_call_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_call_rx_data },
};
static struct platform_device device_headset_call_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_call_tx_data },
};
static struct platform_device device_bt_sco_mono_call_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_call_rx_data },
};
static struct platform_device device_bt_sco_mono_call_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_call_tx_data },
};
static struct platform_device device_bt_sco_mono_nrec_call_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_nrec_call_rx_data },
};
static struct platform_device device_bt_sco_mono_nrec_call_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_nrec_call_tx_data },
};
static struct platform_device device_bt_sco_stereo_call_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_call_rx_data },
};
static struct platform_device device_bt_sco_stereo_call_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_call_tx_data },
};
static struct platform_device device_bt_sco_stereo_nrec_call_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_nrec_call_rx_data },
};
static struct platform_device device_bt_sco_stereo_nrec_call_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_nrec_call_tx_data },
};

static struct platform_device device_headset_loopback_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_loopback_rx_data },
};
static struct platform_device device_headset_loopback_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_loopback_tx_data },
};

// ------- DEFINITION OF SPECIAL DEVICES ------
static struct platform_device device_dualmic_handset_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &dualmic_handset_tx_data },
};
static struct platform_device device_dualmic_speaker_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &dualmic_speaker_tx_data },
};
static struct platform_device device_speaker_vr_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_vr_tx_data },
};
static struct platform_device device_headset_vr_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_vr_tx_data },
};
static struct platform_device device_bt_sco_vr_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_vr_tx_data },
};
static struct platform_device device_bt_sco_nrec_vr_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_nrec_vr_tx_data },
};
static struct platform_device device_fm_radio_headset_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &fm_radio_headset_rx_data },
};
static struct platform_device device_fm_radio_speaker_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &fm_radio_speaker_rx_data },
};
static struct platform_device device_fm_radio_tx = {
	.name = "snddev_mi2s",
	.dev = { .platform_data = &fm_radio_tx_data},
};

// ------- DEFINITION OF EXTERNAL DEVICES ------
static struct platform_device device_hdmi_stereo_rx = {
	.name = "snddev_hdmi", /* "snddev_mi2s", */
	.dev = { .platform_data = &hdmi_stereo_rx_data },
};
static struct platform_device device_lineout_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &lineout_rx_data },
};
static struct platform_device device_tty_headset_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &tty_headset_rx_data },
};
static struct platform_device device_tty_headset_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &tty_headset_tx_data },
};
static struct platform_device device_speaker_headset_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_headset_rx_data },
};
static struct platform_device device_speaker_lineout_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_lineout_rx_data },
};
static struct platform_device device_speaker_hdmi_rx = {
	.name = "snddev_hdmi", /* "snddev_icodec", */
	.dev = { .platform_data = &speaker_hdmi_rx_data },
};

#if defined(CONFIG_USA_MODEL_SGH_T989)
static struct platform_device device_hac_handset_call_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &hac_handset_call_rx_data },
};
#endif

static struct platform_device device_camcoder_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &camcoder_tx_data },
};

// ------- DEFINITION OF CALL2 PAIRED DEVICES ------
static struct platform_device device_handset_call2_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_call2_rx_data },
};
static struct platform_device device_handset_call2_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_call2_tx_data },
};
static struct platform_device device_speaker_call2_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_call2_rx_data },
};
static struct platform_device device_speaker_call2_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_call2_tx_data },
};
static struct platform_device device_headset_call2_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_call2_rx_data },
};
static struct platform_device device_headset_call2_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_call2_tx_data },
};
static struct platform_device device_bt_sco_mono_call2_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_call2_rx_data },
};
static struct platform_device device_bt_sco_mono_call2_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_call2_tx_data },
};
static struct platform_device device_bt_sco_mono_nrec_call2_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_nrec_call2_rx_data },
};
static struct platform_device device_bt_sco_mono_nrec_call2_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_nrec_call2_tx_data },
};
static struct platform_device device_bt_sco_stereo_call2_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_call2_rx_data },
};
static struct platform_device device_bt_sco_stereo_call2_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_call2_tx_data },
};
static struct platform_device device_bt_sco_stereo_nrec_call2_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_nrec_call2_rx_data },
};
static struct platform_device device_bt_sco_stereo_nrec_call2_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_nrec_call2_tx_data },
};

static struct platform_device device_deskdock_call2_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &deskdock_call2_rx_data },
};
static struct platform_device device_deskdock_call2_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &deskdock_call2_tx_data },
};

// ------- DEFINITION OF VOIP CALL2 PAIRED DEVICES ------
static struct platform_device device_handset_voip2_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_voip2_rx_data },
};
static struct platform_device device_handset_voip2_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_voip2_tx_data },
};
static struct platform_device device_speaker_voip2_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_voip2_rx_data },
};
static struct platform_device device_speaker_voip2_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_voip2_tx_data },
};
static struct platform_device device_headset_voip2_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_voip2_rx_data },
};
static struct platform_device device_headset_voip2_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_voip2_tx_data },
};

static struct platform_device device_bt_sco_mono_voip2_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_voip2_rx_data },
};
static struct platform_device device_bt_sco_mono_voip2_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_voip2_tx_data },
};
static struct platform_device device_bt_sco_mono_nrec_voip2_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_nrec_voip2_rx_data },
};
static struct platform_device device_bt_sco_mono_nrec_voip2_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_nrec_voip2_tx_data },
};
static struct platform_device device_bt_sco_stereo_voip2_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_voip2_rx_data },
};
static struct platform_device device_bt_sco_stereo_voip2_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_voip2_tx_data },
};
static struct platform_device device_bt_sco_stereo_nrec_voip2_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_nrec_voip2_rx_data },
};
static struct platform_device device_bt_sco_stereo_nrec_voip2_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_nrec_voip2_tx_data },
};

static struct platform_device device_deskdock_voip2_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &deskdock_voip2_rx_data },
};
static struct platform_device device_deskdock_voip2_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &deskdock_voip2_tx_data },
};

// ------- DEFINITION OF VOIP CALL3 PAIRED DEVICES ------
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)\
|| defined(CONFIG_KOR_MODEL_SHV_E120L) || defined(CONFIG_Q1_KOR_AUDIO)
static struct platform_device device_handset_voip3_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_voip3_rx_data },
};

static struct platform_device device_handset_voip3_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_voip3_tx_data },
};
static struct platform_device device_speaker_voip3_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_voip3_rx_data },
};
static struct platform_device device_speaker_voip3_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_voip3_tx_data },
};
static struct platform_device device_headset_voip3_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_voip3_rx_data },
};
static struct platform_device device_headset_voip3_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_voip3_tx_data },
};

static struct platform_device device_bt_sco_mono_voip3_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_voip3_rx_data },
};
static struct platform_device device_bt_sco_mono_voip3_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_voip3_tx_data },
};
static struct platform_device device_bt_sco_mono_nrec_voip3_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_nrec_voip3_rx_data },
};
static struct platform_device device_bt_sco_mono_nrec_voip3_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_mono_nrec_voip3_tx_data },
};
static struct platform_device device_bt_sco_stereo_voip3_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_voip3_rx_data },
};
static struct platform_device device_bt_sco_stereo_voip3_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_voip3_tx_data },
};
static struct platform_device device_bt_sco_stereo_nrec_voip3_rx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_nrec_voip3_rx_data },
};
static struct platform_device device_bt_sco_stereo_nrec_voip3_tx = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &bt_sco_stereo_nrec_voip3_tx_data },
};

static struct platform_device device_deskdock_voip3_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &deskdock_voip3_rx_data },
};
static struct platform_device device_deskdock_voip3_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &deskdock_voip3_tx_data },
};
#endif

// ------- DEFINITION OF LOOPBACK PAIRED DEVICES ------
static struct platform_device device_handset_loopback_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_loopback_rx_data },
};
static struct platform_device device_handset_loopback_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_loopback_tx_data },
};
static struct platform_device device_speaker_loopback_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_loopback_rx_data },
};
static struct platform_device device_speaker_loopback_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_loopback_tx_data },
};

static struct platform_device *snd_devices_celox[] __initdata = {
	// ------- DEFINITION OF NORMAL PAIRED DEVICES ------
	&device_handset_rx,
	&device_handset_tx,
	&device_speaker_rx,
	&device_speaker_tx,
	&device_headset_rx,
	&device_headset_tx,

	&device_bt_sco_mono_rx,
	&device_bt_sco_mono_tx,
	&device_bt_sco_mono_nrec_rx,
	&device_bt_sco_mono_nrec_tx,
	&device_bt_sco_stereo_rx,
	&device_bt_sco_stereo_tx,
	&device_bt_sco_stereo_nrec_rx,
	&device_bt_sco_stereo_nrec_tx,

	// ------- DEFINITION OF VT CALL PAIRED DEVICES ------
	&device_handset_vt_rx,
	&device_handset_vt_tx,
	&device_speaker_vt_rx,
	&device_speaker_vt_tx,
	&device_headset_vt_rx,
	&device_headset_vt_tx,

	&device_bt_sco_mono_vt_rx,
	&device_bt_sco_mono_vt_tx,
	&device_bt_sco_mono_nrec_vt_rx,
	&device_bt_sco_mono_nrec_vt_tx,
	&device_bt_sco_stereo_vt_rx,
	&device_bt_sco_stereo_vt_tx,
	&device_bt_sco_stereo_nrec_vt_rx,
	&device_bt_sco_stereo_nrec_vt_tx,

	&device_deskdock_vt_rx,
	&device_deskdock_vt_tx,

	// ------- DEFINITION OF VOIP CALL PAIRED DEVICES ------
	&device_handset_voip_rx,
	&device_handset_voip_tx,
	&device_speaker_voip_rx,
	&device_speaker_voip_tx,
	&device_headset_voip_rx,
	&device_headset_voip_tx,

	&device_bt_sco_mono_voip_rx,
	&device_bt_sco_mono_voip_tx,
	&device_bt_sco_mono_nrec_voip_rx,
	&device_bt_sco_mono_nrec_voip_tx,
	&device_bt_sco_stereo_voip_rx,
	&device_bt_sco_stereo_voip_tx,
	&device_bt_sco_stereo_nrec_voip_rx,
	&device_bt_sco_stereo_nrec_voip_tx,

	&device_deskdock_voip_rx,
	&device_deskdock_voip_tx,

	// ------- DEFINITION OF CALL PAIRED DEVICES ------
	&device_handset_call_rx,
	&device_handset_call_tx,
	&device_speaker_call_rx,
	&device_speaker_call_tx,
	&device_headset_call_rx,
	&device_headset_call_tx,

	&device_bt_sco_mono_call_rx,
	&device_bt_sco_mono_call_tx,
	&device_bt_sco_mono_nrec_call_rx,
	&device_bt_sco_mono_nrec_call_tx,
	&device_bt_sco_stereo_call_rx,
	&device_bt_sco_stereo_call_tx,
	&device_bt_sco_stereo_nrec_call_rx,
	&device_bt_sco_stereo_nrec_call_tx,

	&device_deskdock_call_rx,
	&device_deskdock_call_tx,

	&device_headset_loopback_rx,
	&device_headset_loopback_tx,

	// ------- DEFINITION OF SPECIAL DEVICES ------
	&device_dualmic_handset_tx,
	&device_dualmic_speaker_tx,
	&device_speaker_vr_tx,
	&device_headset_vr_tx,
	&device_bt_sco_vr_tx,
	&device_bt_sco_nrec_vr_tx,
	&device_fm_radio_headset_rx,
	&device_fm_radio_speaker_rx,
	&device_fm_radio_tx,

	// ------- DEFINITION OF EXTERNAL DEVICES ------
	&device_hdmi_stereo_rx,
	&device_lineout_rx,
	&device_tty_headset_rx,
	&device_tty_headset_tx,
	&device_speaker_headset_rx,
	&device_speaker_lineout_rx,
	&device_speaker_hdmi_rx,
#if defined(CONFIG_USA_MODEL_SGH_T989)
	&device_hac_handset_call_rx,
#endif
	&device_camcoder_tx,

	// ------- DEFINITION OF CALL2 PAIRED DEVICES ------
	&device_handset_call2_rx,
	&device_handset_call2_tx,
	&device_speaker_call2_rx,
	&device_speaker_call2_tx,
	&device_headset_call2_rx,
	&device_headset_call2_tx,

	&device_bt_sco_mono_call2_rx,
	&device_bt_sco_mono_call2_tx,
	&device_bt_sco_mono_nrec_call2_rx,
	&device_bt_sco_mono_nrec_call2_tx,
	&device_bt_sco_stereo_call2_rx,
	&device_bt_sco_stereo_call2_tx,
	&device_bt_sco_stereo_nrec_call2_rx,
	&device_bt_sco_stereo_nrec_call2_tx,

	&device_deskdock_call2_rx,
	&device_deskdock_call2_tx,

	// ------- DEFINITION OF VOIP CALL2 PAIRED DEVICES ------
	&device_handset_voip2_rx,
	&device_handset_voip2_tx,
	&device_speaker_voip2_rx,
	&device_speaker_voip2_tx,
	&device_headset_voip2_rx,
	&device_headset_voip2_tx,

	&device_bt_sco_mono_voip2_rx,
	&device_bt_sco_mono_voip2_tx,
	&device_bt_sco_mono_nrec_voip2_rx,
	&device_bt_sco_mono_nrec_voip2_tx,
	&device_bt_sco_stereo_voip2_rx,
	&device_bt_sco_stereo_voip2_tx,
	&device_bt_sco_stereo_nrec_voip2_rx,
	&device_bt_sco_stereo_nrec_voip2_tx,

	&device_deskdock_voip2_rx,
	&device_deskdock_voip2_tx,

	// ------- DEFINITION OF VOIP CALL3 PAIRED DEVICES ------
#if defined(CONFIG_KOR_MODEL_SHV_E120S) || defined(CONFIG_KOR_MODEL_SHV_E120K)\
|| defined(CONFIG_KOR_MODEL_SHV_E120L) || defined(CONFIG_Q1_KOR_AUDIO)
	&device_handset_voip3_rx,
	&device_handset_voip3_tx,
	&device_speaker_voip3_rx,
	&device_speaker_voip3_tx,
	&device_headset_voip3_rx,
	&device_headset_voip3_tx,

	&device_bt_sco_mono_voip3_rx,
	&device_bt_sco_mono_voip3_tx,
	&device_bt_sco_mono_nrec_voip3_rx,
	&device_bt_sco_mono_nrec_voip3_tx,
	&device_bt_sco_stereo_voip3_rx,
	&device_bt_sco_stereo_voip3_tx,
	&device_bt_sco_stereo_nrec_voip3_rx,
	&device_bt_sco_stereo_nrec_voip3_tx,

	&device_deskdock_voip3_rx,
	&device_deskdock_voip3_tx,
#endif
	&device_handset_loopback_rx,
	&device_handset_loopback_tx,
	&device_speaker_loopback_rx,
	&device_speaker_loopback_tx
};
#endif /* SEC_AUDIO_DEVICE */

static struct adie_codec_action_unit iearpiece_48KHz_osr256_actions[] =
	EAR_PRI_MONO_8000_OSR_256;

static struct adie_codec_hwsetting_entry iearpiece_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = iearpiece_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(iearpiece_48KHz_osr256_actions),
	}
};

static struct adie_codec_dev_profile iearpiece_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = iearpiece_settings,
	.setting_sz = ARRAY_SIZE(iearpiece_settings),
};

static struct snddev_icodec_data snddev_iearpiece_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "handset_rx",
	.copp_id = 0,
	.profile = &iearpiece_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

static struct platform_device msm_iearpiece_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_iearpiece_data },
};

static struct adie_codec_action_unit imic_48KHz_osr256_actions[] =
	AMIC_PRI_MONO_OSR_256;

static struct adie_codec_hwsetting_entry imic_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = imic_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(imic_48KHz_osr256_actions),
	}
};

static struct adie_codec_dev_profile imic_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = imic_settings,
	.setting_sz = ARRAY_SIZE(imic_settings),
};

static struct snddev_icodec_data snddev_imic_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_tx",
	.copp_id = 1,
	.profile = &imic_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
};

static struct platform_device msm_imic_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_imic_data },
};

static struct snddev_icodec_data snddev_fluid_ispkr_mic_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_mono_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &imic_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
};

static struct platform_device msm_fluid_ispkr_mic_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_fluid_ispkr_mic_data },
};


static struct adie_codec_action_unit headset_ab_cpls_48KHz_osr256_actions[] =
	HEADSET_AB_CPLS_48000_OSR_256;

static struct adie_codec_hwsetting_entry headset_ab_cpls_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = headset_ab_cpls_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_ab_cpls_48KHz_osr256_actions),
	}
};

static struct adie_codec_dev_profile headset_ab_cpls_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = headset_ab_cpls_settings,
	.setting_sz = ARRAY_SIZE(headset_ab_cpls_settings),
};

static struct snddev_icodec_data snddev_ihs_stereo_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_stereo_rx",
	.copp_id = 0,
	.profile = &headset_ab_cpls_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_poweramp_on_headset,
	.pamp_off = msm_snddev_poweramp_off_headset,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct platform_device msm_headset_stereo_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_ihs_stereo_rx_data },
};

static struct adie_codec_action_unit headset_anc_48KHz_osr256_actions[] =
	ANC_HEADSET_CPLS_AMIC1_AUXL_RX1_48000_OSR_256;

static struct adie_codec_hwsetting_entry headset_anc_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = headset_anc_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_anc_48KHz_osr256_actions),
	}
};

static struct adie_codec_dev_profile headset_anc_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = headset_anc_settings,
	.setting_sz = ARRAY_SIZE(headset_anc_settings),
};

static struct snddev_icodec_data snddev_anc_headset_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE | SNDDEV_CAP_ANC),
	.name = "anc_headset_stereo_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &headset_anc_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_poweramp_on_together,
	.pamp_off = msm_snddev_poweramp_off_together,
	/* .pamp_on = msm_snddev_enable_anc_power, */
	/* .pamp_off = msm_snddev_disable_anc_power, */
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct platform_device msm_anc_headset_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_anc_headset_data },
};

static struct adie_codec_action_unit ispkr_stereo_48KHz_osr256_actions[] =
	SPEAKER_PRI_STEREO_48000_OSR_256;

static struct adie_codec_hwsetting_entry ispkr_stereo_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ispkr_stereo_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(ispkr_stereo_48KHz_osr256_actions),
	}
};

static struct adie_codec_dev_profile ispkr_stereo_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ispkr_stereo_settings,
	.setting_sz = ARRAY_SIZE(ispkr_stereo_settings),
};

static struct snddev_icodec_data snddev_ispkr_stereo_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_stereo_rx",
	.copp_id = 0,
	.profile = &ispkr_stereo_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
};

static struct platform_device msm_ispkr_stereo_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_ispkr_stereo_data },
};

static struct adie_codec_action_unit idmic_mono_48KHz_osr256_actions[] =
	DMIC1_PRI_MONO_OSR_256;

static struct adie_codec_hwsetting_entry idmic_mono_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = idmic_mono_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(idmic_mono_48KHz_osr256_actions),
	}
};

static struct adie_codec_dev_profile idmic_mono_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = idmic_mono_settings,
	.setting_sz = ARRAY_SIZE(idmic_mono_settings),
};

static struct snddev_icodec_data snddev_ispkr_mic_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_mono_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &idmic_mono_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_dmic_power,
	.pamp_off = msm_snddev_disable_dmic_power,
};

static struct platform_device msm_ispkr_mic_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_ispkr_mic_data },
};

#ifndef SEC_AUDIO_DEVICE
static struct adie_codec_action_unit iearpiece_ffa_48KHz_osr256_actions[] =
	EAR_PRI_MONO_8000_OSR_256;

static struct adie_codec_hwsetting_entry iearpiece_ffa_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = iearpiece_ffa_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(iearpiece_ffa_48KHz_osr256_actions),
	}
};

static struct adie_codec_dev_profile iearpiece_ffa_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = iearpiece_ffa_settings,
	.setting_sz = ARRAY_SIZE(iearpiece_ffa_settings),
};

static struct snddev_icodec_data snddev_iearpiece_ffa_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "handset_rx",
	.copp_id = 0,
	.profile = &iearpiece_ffa_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

static struct platform_device msm_iearpiece_ffa_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_iearpiece_ffa_data },
};

static struct snddev_icodec_data snddev_imic_ffa_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &idmic_mono_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_dmic_power,
	.pamp_off = msm_snddev_disable_dmic_power,
};

static struct platform_device msm_imic_ffa_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_imic_ffa_data },
};

static struct adie_codec_action_unit dual_mic_endfire_8KHz_osr256_actions[] =
	DMIC1_PRI_STEREO_OSR_256;

static struct adie_codec_hwsetting_entry dual_mic_endfire_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = dual_mic_endfire_8KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(dual_mic_endfire_8KHz_osr256_actions),
	}
};

static struct adie_codec_dev_profile dual_mic_endfire_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = dual_mic_endfire_settings,
	.setting_sz = ARRAY_SIZE(dual_mic_endfire_settings),
};

static struct snddev_icodec_data snddev_dual_mic_endfire_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_dual_mic_endfire_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &dual_mic_endfire_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_dmic_power,
	.pamp_off = msm_snddev_disable_dmic_power,
};

static struct platform_device msm_hs_dual_mic_endfire_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_dual_mic_endfire_data },
};

static struct snddev_icodec_data snddev_dual_mic_spkr_endfire_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_dual_mic_endfire_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &dual_mic_endfire_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_dmic_power,
	.pamp_off = msm_snddev_disable_dmic_power,
};

static struct platform_device msm_spkr_dual_mic_endfire_device = {
	.name = "snddev_icodec",
	.id = 15,
	.dev = { .platform_data = &snddev_dual_mic_spkr_endfire_data },
};

static struct adie_codec_action_unit dual_mic_broadside_8osr256_actions[] =
	HS_DMIC2_STEREO_OSR_256;

static struct adie_codec_hwsetting_entry dual_mic_broadside_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = dual_mic_broadside_8osr256_actions,
		.action_sz = ARRAY_SIZE(dual_mic_broadside_8osr256_actions),
	}
};

static struct adie_codec_dev_profile dual_mic_broadside_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = dual_mic_broadside_settings,
	.setting_sz = ARRAY_SIZE(dual_mic_broadside_settings),
};

static struct snddev_icodec_data snddev_hs_dual_mic_broadside_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_dual_mic_broadside_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &dual_mic_broadside_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_dmic_sec_power,
	.pamp_off = msm_snddev_disable_dmic_sec_power,
};

static struct platform_device msm_hs_dual_mic_broadside_device = {
	.name = "snddev_icodec",
	.id = 21,
	.dev = { .platform_data = &snddev_hs_dual_mic_broadside_data },
};

static struct snddev_icodec_data snddev_spkr_dual_mic_broadside_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_dual_mic_broadside_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &dual_mic_broadside_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_dmic_sec_power,
	.pamp_off = msm_snddev_disable_dmic_sec_power,
};

static struct platform_device msm_spkr_dual_mic_broadside_device = {
	.name = "snddev_icodec",
	.id = 18,
	.dev = { .platform_data = &snddev_spkr_dual_mic_broadside_data },
};
#endif // #ifndef SEC_AUDIO_DEVICE

static struct adie_codec_action_unit
		fluid_dual_mic_endfire_8KHz_osr256_actions[] =
	FLUID_AMIC_DUAL_8000_OSR_256;

static struct adie_codec_hwsetting_entry fluid_dual_mic_endfire_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = fluid_dual_mic_endfire_8KHz_osr256_actions,
		.action_sz =
			ARRAY_SIZE(fluid_dual_mic_endfire_8KHz_osr256_actions),
	}
};

static struct adie_codec_dev_profile fluid_dual_mic_endfire_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = fluid_dual_mic_endfire_settings,
	.setting_sz = ARRAY_SIZE(fluid_dual_mic_endfire_settings),
};

static struct snddev_icodec_data snddev_fluid_dual_mic_endfire_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_dual_mic_endfire_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &fluid_dual_mic_endfire_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_sec_power,
	.pamp_off = msm_snddev_disable_amic_sec_power,
};

static struct platform_device msm_fluid_hs_dual_mic_endfire_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_fluid_dual_mic_endfire_data },
};

static struct snddev_icodec_data snddev_fluid_dual_mic_spkr_endfire_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_dual_mic_endfire_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &fluid_dual_mic_endfire_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_sec_power,
	.pamp_off = msm_snddev_disable_amic_sec_power,
};

static struct platform_device msm_fluid_spkr_dual_mic_endfire_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_fluid_dual_mic_spkr_endfire_data },
};

static struct adie_codec_action_unit
		fluid_dual_mic_broadside_8KHz_osr256_actions[] =
	FLUID_AMIC_DUAL_BROADSIDE_8000_OSR_256;

static struct adie_codec_hwsetting_entry fluid_dual_mic_broadside_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = fluid_dual_mic_broadside_8KHz_osr256_actions,
		.action_sz =
		ARRAY_SIZE(fluid_dual_mic_broadside_8KHz_osr256_actions),
	}
};

static struct adie_codec_dev_profile fluid_dual_mic_broadside_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = fluid_dual_mic_broadside_settings,
	.setting_sz = ARRAY_SIZE(fluid_dual_mic_broadside_settings),
};

static struct snddev_icodec_data snddev_fluid_dual_mic_broadside_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_dual_mic_broadside_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &fluid_dual_mic_broadside_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
};

static struct platform_device msm_fluid_hs_dual_mic_broadside_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_fluid_dual_mic_broadside_data },
};

static struct snddev_icodec_data snddev_fluid_dual_mic_spkr_broadside_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_dual_mic_broadside_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &fluid_dual_mic_broadside_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
};

static struct platform_device msm_fluid_spkr_dual_mic_broadside_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_fluid_dual_mic_spkr_broadside_data },
};

static struct snddev_hdmi_data snddev_hdmi_stereo_rx_data = {
	.capability = SNDDEV_CAP_RX ,
	.name = "hdmi_stereo_rx",
	.copp_id = HDMI_RX,
	.channel_mode = 0,
	.default_sample_rate = 48000,
};

static struct platform_device msm_snddev_hdmi_stereo_rx_device = {
	.name = "snddev_hdmi",
	.dev = { .platform_data = &snddev_hdmi_stereo_rx_data },
};

static struct snddev_mi2s_data snddev_mi2s_fm_tx_data = {
	.capability = SNDDEV_CAP_TX ,
	.name = "fmradio_stereo_tx",
	.copp_id = MI2S_TX,
	.channel_mode = 2, /* stereo */
	.sd_lines = MI2S_SD3, /* sd3 */
	.sample_rate = 48000,
};

static struct platform_device msm_mi2s_fm_tx_device = {
	.name = "snddev_mi2s",
	.dev = { .platform_data = &snddev_mi2s_fm_tx_data },
};

static struct snddev_mi2s_data snddev_mi2s_fm_rx_data = {
	.capability = SNDDEV_CAP_RX ,
	.name = "fmradio_stereo_rx",
	.copp_id = MI2S_RX,
	.channel_mode = 2, /* stereo */
	.sd_lines = MI2S_SD3, /* sd3 */
	.sample_rate = 48000,
};

static struct platform_device msm_mi2s_fm_rx_device = {
	.name = "snddev_mi2s",
	.id = 1,
	.dev = { .platform_data = &snddev_mi2s_fm_rx_data },
};

static struct adie_codec_action_unit iheadset_mic_tx_osr256_actions[] =
	HEADSET_AMIC2_TX_MONO_PRI_OSR_256;

static struct adie_codec_hwsetting_entry iheadset_mic_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = iheadset_mic_tx_osr256_actions,
		.action_sz = ARRAY_SIZE(iheadset_mic_tx_osr256_actions),
	}
};

static struct adie_codec_dev_profile iheadset_mic_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = iheadset_mic_tx_settings,
	.setting_sz = ARRAY_SIZE(iheadset_mic_tx_settings),
};

static struct snddev_icodec_data snddev_headset_mic_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "headset_mono_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &iheadset_mic_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

static struct platform_device msm_headset_mic_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_headset_mic_data },
};

static struct adie_codec_action_unit
	ihs_stereo_speaker_stereo_rx_48KHz_osr256_actions[] =
	SPEAKER_HPH_AB_CPL_PRI_STEREO_48000_OSR_256;

static struct adie_codec_hwsetting_entry
	ihs_stereo_speaker_stereo_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ihs_stereo_speaker_stereo_rx_48KHz_osr256_actions,
		.action_sz =
		ARRAY_SIZE(ihs_stereo_speaker_stereo_rx_48KHz_osr256_actions),
	}
};

static struct adie_codec_dev_profile ihs_stereo_speaker_stereo_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ihs_stereo_speaker_stereo_rx_settings,
	.setting_sz = ARRAY_SIZE(ihs_stereo_speaker_stereo_rx_settings),
};

static struct snddev_icodec_data snddev_ihs_stereo_speaker_stereo_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_stereo_speaker_stereo_rx",
	.copp_id = 0,
	.profile = &ihs_stereo_speaker_stereo_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct platform_device msm_ihs_stereo_speaker_stereo_rx_device = {
	.name = "snddev_icodec",
	.id = 22,
	.dev = { .platform_data = &snddev_ihs_stereo_speaker_stereo_rx_data },
};

/* define the value for BT_SCO */

static struct snddev_ecodec_data snddev_bt_sco_earpiece_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_rx",
	.copp_id = PCM_RX,
	.channel_mode = 1,
};

static struct snddev_ecodec_data snddev_bt_sco_mic_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "bt_sco_tx",
	.copp_id = PCM_TX,
	.channel_mode = 1,
};

struct platform_device msm_bt_sco_earpiece_device = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &snddev_bt_sco_earpiece_data },
};

struct platform_device msm_bt_sco_mic_device = {
	.name = "msm_snddev_ecodec",
	.dev = { .platform_data = &snddev_bt_sco_mic_data },
};

static struct adie_codec_action_unit itty_mono_tx_actions[] =
	TTY_HEADSET_MONO_TX_OSR_256;

static struct adie_codec_hwsetting_entry itty_mono_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = itty_mono_tx_actions,
		.action_sz = ARRAY_SIZE(itty_mono_tx_actions),
	},
};

static struct adie_codec_dev_profile itty_mono_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = itty_mono_tx_settings,
	.setting_sz = ARRAY_SIZE(itty_mono_tx_settings),
};

static struct snddev_icodec_data snddev_itty_mono_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE | SNDDEV_CAP_TTY),
	.name = "tty_headset_mono_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &itty_mono_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

static struct platform_device msm_itty_mono_tx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_itty_mono_tx_data },
};

static struct adie_codec_action_unit itty_mono_rx_actions[] =
	TTY_HEADSET_MONO_RX_8000_OSR_256;

static struct adie_codec_hwsetting_entry itty_mono_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = itty_mono_rx_actions,
		.action_sz = ARRAY_SIZE(itty_mono_rx_actions),
	},
};

static struct adie_codec_dev_profile itty_mono_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = itty_mono_rx_settings,
	.setting_sz = ARRAY_SIZE(itty_mono_rx_settings),
};

static struct snddev_icodec_data snddev_itty_mono_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE | SNDDEV_CAP_TTY),
	.name = "tty_headset_mono_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &itty_mono_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct platform_device msm_itty_mono_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_itty_mono_rx_data },
};

static struct adie_codec_action_unit linein_pri_actions[] =
	LINEIN_PRI_STEREO_OSR_256;

static struct adie_codec_hwsetting_entry linein_pri_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = linein_pri_actions,
		.action_sz = ARRAY_SIZE(linein_pri_actions),
	},
};

static struct adie_codec_dev_profile linein_pri_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = linein_pri_settings,
	.setting_sz = ARRAY_SIZE(linein_pri_settings),
};

static struct snddev_icodec_data snddev_linein_pri_data = {
	.capability = SNDDEV_CAP_TX,
	.name = "linein_pri_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &linein_pri_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct platform_device msm_linein_pri_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_linein_pri_data },
};

static struct adie_codec_action_unit auxpga_lp_lo_actions[] =
	LB_AUXPGA_LO_STEREO;

static struct adie_codec_hwsetting_entry auxpga_lp_lo_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = auxpga_lp_lo_actions,
		.action_sz = ARRAY_SIZE(auxpga_lp_lo_actions),
	},
};

static struct adie_codec_dev_profile auxpga_lp_lo_profile = {
	.path_type = ADIE_CODEC_LB,
	.settings = auxpga_lp_lo_settings,
	.setting_sz = ARRAY_SIZE(auxpga_lp_lo_settings),
};

static struct snddev_icodec_data snddev_auxpga_lp_lo_data = {
	.capability = SNDDEV_CAP_LB,
	.name = "speaker_stereo_lb",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &auxpga_lp_lo_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
	.dev_vol_type = SNDDEV_DEV_VOL_ANALOG,
};

static struct platform_device msm_auxpga_lp_lo_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_auxpga_lp_lo_data },
};

static struct adie_codec_action_unit auxpga_lp_hs_actions[] =
	LB_AUXPGA_HPH_AB_CPLS_STEREO;

static struct adie_codec_hwsetting_entry auxpga_lp_hs_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = auxpga_lp_hs_actions,
		.action_sz = ARRAY_SIZE(auxpga_lp_hs_actions),
	},
};

static struct adie_codec_dev_profile auxpga_lp_hs_profile = {
	.path_type = ADIE_CODEC_LB,
	.settings = auxpga_lp_hs_settings,
	.setting_sz = ARRAY_SIZE(auxpga_lp_hs_settings),
};

static struct snddev_icodec_data snddev_auxpga_lp_hs_data = {
	.capability = SNDDEV_CAP_LB,
	.name = "hs_stereo_lb",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &auxpga_lp_hs_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
	.dev_vol_type = SNDDEV_DEV_VOL_ANALOG,
};

static struct platform_device msm_auxpga_lp_hs_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_auxpga_lp_hs_data },
};

#ifdef CONFIG_MSM8X60_FTM_AUDIO_DEVICES
static struct adie_codec_action_unit ftm_headset_mono_rx_actions[] =
	HPH_PRI_AB_CPLS_MONO;

static struct adie_codec_hwsetting_entry ftm_headset_mono_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_headset_mono_rx_actions,
		.action_sz = ARRAY_SIZE(ftm_headset_mono_rx_actions),
	},
};

static struct adie_codec_dev_profile ftm_headset_mono_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ftm_headset_mono_rx_settings,
	.setting_sz = ARRAY_SIZE(ftm_headset_mono_rx_settings),
};

static struct snddev_icodec_data ftm_headset_mono_rx_data = {
	.capability = SNDDEV_CAP_RX,
	.name = "ftm_headset_mono_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &ftm_headset_mono_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_headset_mono_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_headset_mono_rx_data},
};

static struct adie_codec_action_unit ftm_headset_mono_diff_rx_actions[] =
	HEADSET_MONO_DIFF_RX;

static struct adie_codec_hwsetting_entry ftm_headset_mono_diff_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_headset_mono_diff_rx_actions,
		.action_sz = ARRAY_SIZE(ftm_headset_mono_diff_rx_actions),
	},
};

static struct adie_codec_dev_profile ftm_headset_mono_diff_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ftm_headset_mono_diff_rx_settings,
	.setting_sz = ARRAY_SIZE(ftm_headset_mono_diff_rx_settings),
};

static struct snddev_icodec_data ftm_headset_mono_diff_rx_data = {
	.capability = SNDDEV_CAP_RX,
	.name = "ftm_headset_mono_diff_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &ftm_headset_mono_diff_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_headset_mono_diff_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_headset_mono_diff_rx_data},
};

static struct adie_codec_action_unit ftm_spkr_mono_rx_actions[] =
	SPEAKER_PRI_STEREO_48000_OSR_256;

static struct adie_codec_hwsetting_entry ftm_spkr_mono_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_spkr_mono_rx_actions,
		.action_sz = ARRAY_SIZE(ftm_spkr_mono_rx_actions),
	},
};

static struct adie_codec_dev_profile ftm_spkr_mono_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ftm_spkr_mono_rx_settings,
	.setting_sz = ARRAY_SIZE(ftm_spkr_mono_rx_settings),
};

static struct snddev_icodec_data ftm_spkr_mono_rx_data = {
	.capability = SNDDEV_CAP_RX,
	.name = "ftm_spkr_mono_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &ftm_spkr_mono_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_spkr_mono_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_spkr_mono_rx_data},
};

static struct adie_codec_action_unit ftm_spkr_l_rx_actions[] =
	FTM_SPKR_L_RX;

static struct adie_codec_hwsetting_entry ftm_spkr_l_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_spkr_l_rx_actions,
		.action_sz = ARRAY_SIZE(ftm_spkr_l_rx_actions),
	},
};

static struct adie_codec_dev_profile ftm_spkr_l_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ftm_spkr_l_rx_settings,
	.setting_sz = ARRAY_SIZE(ftm_spkr_l_rx_settings),
};

static struct snddev_icodec_data ftm_spkr_l_rx_data = {
	.capability = SNDDEV_CAP_RX,
	.name = "ftm_spkr_l_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &ftm_spkr_l_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_spkr_l_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_spkr_l_rx_data},
};

static struct adie_codec_action_unit ftm_spkr_r_rx_actions[] =
	SPKR_R_RX;

static struct adie_codec_hwsetting_entry ftm_spkr_r_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_spkr_r_rx_actions,
		.action_sz = ARRAY_SIZE(ftm_spkr_r_rx_actions),
	},
};

static struct adie_codec_dev_profile ftm_spkr_r_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ftm_spkr_r_rx_settings,
	.setting_sz = ARRAY_SIZE(ftm_spkr_r_rx_settings),
};

static struct snddev_icodec_data ftm_spkr_r_rx_data = {
	.capability = SNDDEV_CAP_RX,
	.name = "ftm_spkr_r_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &ftm_spkr_r_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_spkr_r_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_spkr_r_rx_data},
};

static struct adie_codec_action_unit ftm_spkr_mono_diff_rx_actions[] =
	SPKR_MONO_DIFF_RX;

static struct adie_codec_hwsetting_entry ftm_spkr_mono_diff_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_spkr_mono_diff_rx_actions,
		.action_sz = ARRAY_SIZE(ftm_spkr_mono_diff_rx_actions),
	},
};

static struct adie_codec_dev_profile ftm_spkr_mono_diff_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ftm_spkr_mono_diff_rx_settings,
	.setting_sz = ARRAY_SIZE(ftm_spkr_mono_diff_rx_settings),
};

static struct snddev_icodec_data ftm_spkr_mono_diff_rx_data = {
	.capability = SNDDEV_CAP_RX,
	.name = "ftm_spkr_mono_diff_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &ftm_spkr_mono_diff_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_spkr_mono_diff_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_spkr_mono_diff_rx_data},
};

static struct adie_codec_action_unit ftm_headset_mono_l_rx_actions[] =
	HPH_PRI_AB_CPLS_MONO_LEFT;

static struct adie_codec_hwsetting_entry ftm_headset_mono_l_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_headset_mono_l_rx_actions,
		.action_sz = ARRAY_SIZE(ftm_headset_mono_l_rx_actions),
	},
};

static struct adie_codec_dev_profile ftm_headset_mono_l_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ftm_headset_mono_l_rx_settings,
	.setting_sz = ARRAY_SIZE(ftm_headset_mono_l_rx_settings),
};

static struct snddev_icodec_data ftm_headset_mono_l_rx_data = {
	.capability = SNDDEV_CAP_RX,
	.name = "ftm_headset_mono_l_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &ftm_headset_mono_l_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_headset_mono_l_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_headset_mono_l_rx_data},
};

static struct adie_codec_action_unit ftm_headset_mono_r_rx_actions[] =
	HPH_PRI_AB_CPLS_MONO_RIGHT;

static struct adie_codec_hwsetting_entry ftm_headset_mono_r_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_headset_mono_r_rx_actions,
		.action_sz = ARRAY_SIZE(ftm_headset_mono_r_rx_actions),
	},
};

static struct adie_codec_dev_profile ftm_headset_mono_r_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ftm_headset_mono_r_rx_settings,
	.setting_sz = ARRAY_SIZE(ftm_headset_mono_r_rx_settings),
};

static struct snddev_icodec_data ftm_headset_mono_r_rx_data = {
	.capability = SNDDEV_CAP_RX,
	.name = "ftm_headset_mono_r_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &ftm_headset_mono_r_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_headset_mono_r_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_headset_mono_r_rx_data},
};

static struct adie_codec_action_unit ftm_linein_l_tx_actions[] =
	LINEIN_MONO_L_TX;

static struct adie_codec_hwsetting_entry ftm_linein_l_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_linein_l_tx_actions,
		.action_sz = ARRAY_SIZE(ftm_linein_l_tx_actions),
	},
};

static struct adie_codec_dev_profile ftm_linein_l_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = ftm_linein_l_tx_settings,
	.setting_sz = ARRAY_SIZE(ftm_linein_l_tx_settings),
};

static struct snddev_icodec_data ftm_linein_l_tx_data = {
	.capability = SNDDEV_CAP_TX,
	.name = "ftm_linein_l_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &ftm_linein_l_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_linein_l_tx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_linein_l_tx_data },
};

static struct adie_codec_action_unit ftm_linein_r_tx_actions[] =
	LINEIN_MONO_R_TX;

static struct adie_codec_hwsetting_entry ftm_linein_r_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_linein_r_tx_actions,
		.action_sz = ARRAY_SIZE(ftm_linein_r_tx_actions),
	},
};

static struct adie_codec_dev_profile ftm_linein_r_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = ftm_linein_r_tx_settings,
	.setting_sz = ARRAY_SIZE(ftm_linein_r_tx_settings),
};

static struct snddev_icodec_data ftm_linein_r_tx_data = {
	.capability = SNDDEV_CAP_TX,
	.name = "ftm_linein_r_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &ftm_linein_r_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_linein_r_tx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_linein_r_tx_data },
};

static struct adie_codec_action_unit ftm_aux_out_rx_actions[] =
	AUX_OUT_RX;

static struct adie_codec_hwsetting_entry ftm_aux_out_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_aux_out_rx_actions,
		.action_sz = ARRAY_SIZE(ftm_aux_out_rx_actions),
	},
};

static struct adie_codec_dev_profile ftm_aux_out_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ftm_aux_out_rx_settings,
	.setting_sz = ARRAY_SIZE(ftm_aux_out_rx_settings),
};

static struct snddev_icodec_data ftm_aux_out_rx_data = {
	.capability = SNDDEV_CAP_RX,
	.name = "ftm_aux_out_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &ftm_aux_out_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_aux_out_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_aux_out_rx_data},
};

static struct adie_codec_action_unit ftm_dmic1_left_tx_actions[] =
	DMIC1_LEFT_TX;

static struct adie_codec_hwsetting_entry ftm_dmic1_left_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_dmic1_left_tx_actions,
		.action_sz = ARRAY_SIZE(ftm_dmic1_left_tx_actions),
	},
};

static struct adie_codec_dev_profile ftm_dmic1_left_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = ftm_dmic1_left_tx_settings,
	.setting_sz = ARRAY_SIZE(ftm_dmic1_left_tx_settings),
};

static struct snddev_icodec_data ftm_dmic1_left_tx_data = {
	.capability = SNDDEV_CAP_TX,
	.name = "ftm_dmic1_left_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &ftm_dmic1_left_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_dmic_power,
	.pamp_off = msm_snddev_disable_dmic_power,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_dmic1_left_tx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_dmic1_left_tx_data},
};

static struct adie_codec_action_unit ftm_dmic1_right_tx_actions[] =
	DMIC1_RIGHT_TX;

static struct adie_codec_hwsetting_entry ftm_dmic1_right_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_dmic1_right_tx_actions,
		.action_sz = ARRAY_SIZE(ftm_dmic1_right_tx_actions),
	},
};

static struct adie_codec_dev_profile ftm_dmic1_right_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = ftm_dmic1_right_tx_settings,
	.setting_sz = ARRAY_SIZE(ftm_dmic1_right_tx_settings),
};

static struct snddev_icodec_data ftm_dmic1_right_tx_data = {
	.capability = SNDDEV_CAP_TX,
	.name = "ftm_dmic1_right_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &ftm_dmic1_right_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_dmic_power,
	.pamp_off = msm_snddev_disable_dmic_power,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_dmic1_right_tx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_dmic1_right_tx_data},
};

static struct adie_codec_action_unit ftm_dmic1_l_and_r_tx_actions[] =
	DMIC1_LEFT_AND_RIGHT_TX;

static struct adie_codec_hwsetting_entry ftm_dmic1_l_and_r_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_dmic1_l_and_r_tx_actions,
		.action_sz = ARRAY_SIZE(ftm_dmic1_l_and_r_tx_actions),
	},
};

static struct adie_codec_dev_profile ftm_dmic1_l_and_r_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = ftm_dmic1_l_and_r_tx_settings,
	.setting_sz = ARRAY_SIZE(ftm_dmic1_l_and_r_tx_settings),
};

static struct snddev_icodec_data ftm_dmic1_l_and_r_tx_data = {
	.capability = SNDDEV_CAP_TX,
	.name = "ftm_dmic1_l_and_r_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &ftm_dmic1_l_and_r_tx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_dmic_power,
	.pamp_off = msm_snddev_disable_dmic_power,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_dmic1_l_and_r_tx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_dmic1_l_and_r_tx_data},
};

static struct adie_codec_action_unit ftm_dmic2_left_tx_actions[] =
	DMIC2_LEFT_TX;

static struct adie_codec_hwsetting_entry ftm_dmic2_left_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_dmic2_left_tx_actions,
		.action_sz = ARRAY_SIZE(ftm_dmic2_left_tx_actions),
	},
};

static struct adie_codec_dev_profile ftm_dmic2_left_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = ftm_dmic2_left_tx_settings,
	.setting_sz = ARRAY_SIZE(ftm_dmic2_left_tx_settings),
};

static struct snddev_icodec_data ftm_dmic2_left_tx_data = {
	.capability = SNDDEV_CAP_TX,
	.name = "ftm_dmic2_left_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &ftm_dmic2_left_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_dmic_power,
	.pamp_off = msm_snddev_disable_dmic_power,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_dmic2_left_tx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_dmic2_left_tx_data },
};

static struct adie_codec_action_unit ftm_dmic2_right_tx_actions[] =
	DMIC2_RIGHT_TX;

static struct adie_codec_hwsetting_entry ftm_dmic2_right_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_dmic2_right_tx_actions,
		.action_sz = ARRAY_SIZE(ftm_dmic2_right_tx_actions),
	},
};

static struct adie_codec_dev_profile ftm_dmic2_right_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = ftm_dmic2_right_tx_settings,
	.setting_sz = ARRAY_SIZE(ftm_dmic2_right_tx_settings),
};

static struct snddev_icodec_data ftm_dmic2_right_tx_data = {
	.capability = SNDDEV_CAP_TX,
	.name = "ftm_dmic2_right_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &ftm_dmic2_right_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_dmic_power,
	.pamp_off = msm_snddev_disable_dmic_power,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_dmic2_right_tx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_dmic2_right_tx_data },
};

static struct adie_codec_action_unit ftm_dmic2_l_and_r_tx_actions[] =
	DMIC2_LEFT_AND_RIGHT_TX;

static struct adie_codec_hwsetting_entry ftm_dmic2_l_and_r_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_dmic2_l_and_r_tx_actions,
		.action_sz = ARRAY_SIZE(ftm_dmic2_l_and_r_tx_actions),
	},
};

static struct adie_codec_dev_profile ftm_dmic2_l_and_r_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = ftm_dmic2_l_and_r_tx_settings,
	.setting_sz = ARRAY_SIZE(ftm_dmic2_l_and_r_tx_settings),
};

static struct snddev_icodec_data ftm_dmic2_l_and_r_tx_data = {
	.capability = SNDDEV_CAP_TX,
	.name = "ftm_dmic2_l_and_r_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &ftm_dmic2_l_and_r_tx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_dmic_power,
	.pamp_off = msm_snddev_disable_dmic_power,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_dmic2_l_and_r_tx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_dmic2_l_and_r_tx_data},
};

static struct adie_codec_action_unit ftm_handset_mic1_aux_in_actions[] =
	HANDSET_MIC1_AUX_IN;

static struct adie_codec_hwsetting_entry ftm_handset_mic1_aux_in_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_handset_mic1_aux_in_actions,
		.action_sz = ARRAY_SIZE(ftm_handset_mic1_aux_in_actions),
	},
};

static struct adie_codec_dev_profile ftm_handset_mic1_aux_in_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = ftm_handset_mic1_aux_in_settings,
	.setting_sz = ARRAY_SIZE(ftm_handset_mic1_aux_in_settings),
};

static struct snddev_icodec_data ftm_handset_mic1_aux_in_data = {
	.capability = SNDDEV_CAP_TX,
	.name = "ftm_handset_mic1_aux_in",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &ftm_handset_mic1_aux_in_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	/* Assumption is that inputs are not tied to analog mic, so
	 * no need to enable mic bias.
	 */
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_handset_mic1_aux_in_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_handset_mic1_aux_in_data},
};

static struct snddev_mi2s_data snddev_mi2s_sd0_rx_data = {
	.capability = SNDDEV_CAP_RX ,
	.name = "mi2s_sd0_rx",
	.copp_id = MI2S_RX,
	.channel_mode = 2, /* stereo */
	.sd_lines = MI2S_SD0, /* sd0 */
	.sample_rate = 48000,
};

static struct platform_device ftm_mi2s_sd0_rx_device = {
	.name = "snddev_mi2s",
	.dev = { .platform_data = &snddev_mi2s_sd0_rx_data },
};

static struct snddev_mi2s_data snddev_mi2s_sd1_rx_data = {
	.capability = SNDDEV_CAP_RX ,
	.name = "mi2s_sd1_rx",
	.copp_id = MI2S_RX,
	.channel_mode = 2, /* stereo */
	.sd_lines = MI2S_SD1, /* sd1 */
	.sample_rate = 48000,
};

static struct platform_device ftm_mi2s_sd1_rx_device = {
	.name = "snddev_mi2s",
	.dev = { .platform_data = &snddev_mi2s_sd1_rx_data },
};

static struct snddev_mi2s_data snddev_mi2s_sd2_rx_data = {
	.capability = SNDDEV_CAP_RX ,
	.name = "mi2s_sd2_rx",
	.copp_id = MI2S_RX,
	.channel_mode = 2, /* stereo */
	.sd_lines = MI2S_SD2, /* sd2 */
	.sample_rate = 48000,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_mi2s_sd2_rx_device = {
	.name = "snddev_mi2s",
	.dev = { .platform_data = &snddev_mi2s_sd2_rx_data },
};

/* earpiece */
static struct adie_codec_action_unit ftm_handset_adie_lp_rx_actions[] =
	EAR_PRI_MONO_LB;

static struct adie_codec_hwsetting_entry ftm_handset_adie_lp_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_handset_adie_lp_rx_actions,
		.action_sz = ARRAY_SIZE(ftm_handset_adie_lp_rx_actions),
	}
};

static struct adie_codec_dev_profile ftm_handset_adie_lp_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ftm_handset_adie_lp_rx_settings,
	.setting_sz = ARRAY_SIZE(ftm_handset_adie_lp_rx_settings),
};

static struct snddev_icodec_data ftm_handset_adie_lp_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "ftm_handset_adie_lp_rx",
	.copp_id = 0,
	.profile = &ftm_handset_adie_lp_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_handset_adie_lp_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_handset_adie_lp_rx_data },
};

static struct adie_codec_action_unit ftm_headset_l_adie_lp_rx_actions[] =
	FTM_HPH_PRI_AB_CPLS_MONO_LB_LEFT;

static struct adie_codec_hwsetting_entry ftm_headset_l_adie_lp_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_headset_l_adie_lp_rx_actions,
		.action_sz = ARRAY_SIZE(ftm_headset_l_adie_lp_rx_actions),
	},
};

static struct adie_codec_dev_profile ftm_headset_l_adie_lp_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ftm_headset_l_adie_lp_rx_settings,
	.setting_sz = ARRAY_SIZE(ftm_headset_l_adie_lp_rx_settings),
};

static struct snddev_icodec_data ftm_headset_l_adie_lp_rx_data = {
	.capability = SNDDEV_CAP_RX,
	.name = "ftm_headset_l_adie_lp_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &ftm_headset_l_adie_lp_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_headset_l_adie_lp_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_headset_l_adie_lp_rx_data },
};

static struct adie_codec_action_unit ftm_headset_r_adie_lp_rx_actions[] =
	FTM_HPH_PRI_AB_CPLS_MONO_LB_RIGHT;

static struct adie_codec_hwsetting_entry ftm_headset_r_adie_lp_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_headset_r_adie_lp_rx_actions,
		.action_sz = ARRAY_SIZE(ftm_headset_r_adie_lp_rx_actions),
	},
};

static struct adie_codec_dev_profile ftm_headset_r_adie_lp_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ftm_headset_r_adie_lp_rx_settings,
	.setting_sz = ARRAY_SIZE(ftm_headset_r_adie_lp_rx_settings),
};

static struct snddev_icodec_data ftm_headset_r_adie_lp_rx_data = {
	.capability = SNDDEV_CAP_RX,
	.name = "ftm_headset_r_adie_lp_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &ftm_headset_r_adie_lp_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_headset_r_adie_lp_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_headset_r_adie_lp_rx_data },
};

static struct adie_codec_action_unit ftm_spkr_l_rx_lp_actions[] =
	FTM_SPKR_L_RX;

static struct adie_codec_hwsetting_entry ftm_spkr_l_rx_lp_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_spkr_l_rx_lp_actions,
		.action_sz = ARRAY_SIZE(ftm_spkr_l_rx_lp_actions),
	},
};

static struct adie_codec_dev_profile ftm_spkr_l_rx_lp_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ftm_spkr_l_rx_lp_settings,
	.setting_sz = ARRAY_SIZE(ftm_spkr_l_rx_lp_settings),
};

static struct snddev_icodec_data ftm_spkr_l_rx_lp_data = {
	.capability = SNDDEV_CAP_RX,
	.name = "ftm_spk_l_adie_lp_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &ftm_spkr_l_rx_lp_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_spk_l_adie_lp_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_spkr_l_rx_lp_data},
};

static struct adie_codec_action_unit ftm_spkr_r_adie_lp_rx_actions[] =
	SPKR_R_RX;

static struct adie_codec_hwsetting_entry ftm_spkr_r_adie_lp_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_spkr_r_adie_lp_rx_actions,
		.action_sz = ARRAY_SIZE(ftm_spkr_r_adie_lp_rx_actions),
	},
};

static struct adie_codec_dev_profile ftm_spkr_r_adie_lp_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ftm_spkr_r_adie_lp_rx_settings,
	.setting_sz = ARRAY_SIZE(ftm_spkr_r_adie_lp_rx_settings),
};

static struct snddev_icodec_data ftm_spkr_r_adie_lp_rx_data = {
	.capability = SNDDEV_CAP_RX,
	.name = "ftm_spk_r_adie_lp_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &ftm_spkr_r_adie_lp_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_spk_r_adie_lp_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_spkr_r_adie_lp_rx_data},
};

static struct adie_codec_action_unit ftm_spkr_adie_lp_rx_actions[] =
	FTM_SPKR_RX_LB;

static struct adie_codec_hwsetting_entry ftm_spkr_adie_lp_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_spkr_adie_lp_rx_actions,
		.action_sz = ARRAY_SIZE(ftm_spkr_adie_lp_rx_actions),
	},
};

static struct adie_codec_dev_profile ftm_spkr_adie_lp_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = ftm_spkr_adie_lp_rx_settings,
	.setting_sz = ARRAY_SIZE(ftm_spkr_adie_lp_rx_settings),
};

static struct snddev_icodec_data ftm_spkr_adie_lp_rx_data = {
	.capability = SNDDEV_CAP_RX,
	.name = "ftm_spk_adie_lp_rx",
	.copp_id = PRIMARY_I2S_RX,
	.profile = &ftm_spkr_adie_lp_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_poweramp_on,
	.pamp_off = msm_snddev_poweramp_off,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_spk_adie_lp_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_spkr_adie_lp_rx_data},
};

static struct adie_codec_action_unit ftm_handset_dual_tx_lp_actions[] =
	FTM_AMIC_DUAL_HANDSET_TX_LB;

static struct adie_codec_hwsetting_entry ftm_handset_dual_tx_lp_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_handset_dual_tx_lp_actions,
		.action_sz = ARRAY_SIZE(ftm_handset_dual_tx_lp_actions),
	}
};

static struct adie_codec_dev_profile ftm_handset_dual_tx_lp_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = ftm_handset_dual_tx_lp_settings,
	.setting_sz = ARRAY_SIZE(ftm_handset_dual_tx_lp_settings),
};

static struct snddev_icodec_data ftm_handset_dual_tx_lp_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_mic1_handset_mic2",
	.copp_id = 1,
	.profile = &ftm_handset_dual_tx_lp_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_handset_dual_tx_lp_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_handset_dual_tx_lp_data },
};

static struct adie_codec_action_unit ftm_handset_mic_adie_lp_tx_actions[] =
	FTM_HANDSET_LB_TX;

static struct adie_codec_hwsetting_entry
	ftm_handset_mic_adie_lp_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_handset_mic_adie_lp_tx_actions,
		.action_sz = ARRAY_SIZE(ftm_handset_mic_adie_lp_tx_actions),
	}
};

static struct adie_codec_dev_profile ftm_handset_mic_adie_lp_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = ftm_handset_mic_adie_lp_tx_settings,
	.setting_sz = ARRAY_SIZE(ftm_handset_mic_adie_lp_tx_settings),
};

static struct snddev_icodec_data ftm_handset_mic_adie_lp_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "ftm_handset_mic_adie_lp_tx",
	.copp_id = 1,
	.profile = &ftm_handset_mic_adie_lp_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_handset_mic_adie_lp_tx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_handset_mic_adie_lp_tx_data },
};

static struct adie_codec_action_unit ftm_headset_mic_adie_lp_tx_actions[] =
	FTM_HEADSET_LB_TX;

static struct adie_codec_hwsetting_entry
			ftm_headset_mic_adie_lp_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = ftm_headset_mic_adie_lp_tx_actions,
		.action_sz = ARRAY_SIZE(ftm_headset_mic_adie_lp_tx_actions),
	}
};

static struct adie_codec_dev_profile ftm_headset_mic_adie_lp_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = ftm_headset_mic_adie_lp_tx_settings,
	.setting_sz = ARRAY_SIZE(ftm_headset_mic_adie_lp_tx_settings),
};

static struct snddev_icodec_data ftm_headset_mic_adie_lp_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "ftm_headset_mic_adie_lp_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &ftm_headset_mic_adie_lp_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.dev_vol_type = SNDDEV_DEV_VOL_DIGITAL,
};

static struct platform_device ftm_headset_mic_adie_lp_tx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_headset_mic_adie_lp_tx_data },
};
#endif /* CONFIG_MSM8X60_FTM_AUDIO_DEVICES */

static struct snddev_virtual_data snddev_uplink_rx_data = {
	.capability = SNDDEV_CAP_RX,
	.name = "uplink_rx",
	.copp_id = VOICE_PLAYBACK_TX,
};

static struct platform_device msm_uplink_rx_device = {
	.name = "snddev_virtual",
	.dev = { .platform_data = &snddev_uplink_rx_data },
};

static struct snddev_hdmi_data snddev_hdmi_non_linear_pcm_rx_data = {
	.capability = SNDDEV_CAP_RX ,
	.name = "hdmi_pass_through",
	.default_sample_rate = 48000,
	.on_apps = 1,
};

static struct platform_device msm_snddev_hdmi_non_linear_pcm_rx_device = {
	.name = "snddev_hdmi",
	.dev = { .platform_data = &snddev_hdmi_non_linear_pcm_rx_data },
};


#ifdef CONFIG_DEBUG_FS
static struct adie_codec_action_unit
	ihs_stereo_rx_class_d_legacy_48KHz_osr256_actions[] =
	HPH_PRI_D_LEG_STEREO;

static struct adie_codec_hwsetting_entry
	ihs_stereo_rx_class_d_legacy_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions =
		ihs_stereo_rx_class_d_legacy_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE
		(ihs_stereo_rx_class_d_legacy_48KHz_osr256_actions),
	}
};

static struct adie_codec_action_unit
	ihs_stereo_rx_class_ab_legacy_48KHz_osr256_actions[] =
	HPH_PRI_AB_LEG_STEREO;

static struct adie_codec_hwsetting_entry
	ihs_stereo_rx_class_ab_legacy_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions =
		ihs_stereo_rx_class_ab_legacy_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE
		(ihs_stereo_rx_class_ab_legacy_48KHz_osr256_actions),
	}
};

static void snddev_hsed_config_modify_setting(int type)
{
	struct platform_device *device;
	struct snddev_icodec_data *icodec_data;

	device = &msm_headset_stereo_device;
	icodec_data = (struct snddev_icodec_data *)device->dev.platform_data;

	if (icodec_data) {
		if (type == 1) {
			icodec_data->voltage_on = NULL;
			icodec_data->voltage_off = NULL;
			icodec_data->profile->settings =
				ihs_stereo_rx_class_d_legacy_settings;
			icodec_data->profile->setting_sz =
			ARRAY_SIZE(ihs_stereo_rx_class_d_legacy_settings);
		} else if (type == 2) {
			icodec_data->voltage_on = NULL;
			icodec_data->voltage_off = NULL;
			icodec_data->profile->settings =
				ihs_stereo_rx_class_ab_legacy_settings;
			icodec_data->profile->setting_sz =
			ARRAY_SIZE(ihs_stereo_rx_class_ab_legacy_settings);
		}
	}
}

static void snddev_hsed_config_restore_setting(void)
{
	struct platform_device *device;
	struct snddev_icodec_data *icodec_data;

	device = &msm_headset_stereo_device;
	icodec_data = (struct snddev_icodec_data *)device->dev.platform_data;

	if (icodec_data) {
		icodec_data->voltage_on = msm_snddev_voltage_on;
		icodec_data->voltage_off = msm_snddev_voltage_off;
		icodec_data->profile->settings = headset_ab_cpls_settings;
		icodec_data->profile->setting_sz =
			ARRAY_SIZE(headset_ab_cpls_settings);
	}
}

static ssize_t snddev_hsed_config_debug_write(struct file *filp,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char *lb_str = filp->private_data;
	char cmd;

	if (get_user(cmd, ubuf))
		return -EFAULT;

	if (!strcmp(lb_str, "msm_hsed_config")) {
		switch (cmd) {
		case '0':
			snddev_hsed_config_restore_setting();
			break;

		case '1':
			snddev_hsed_config_modify_setting(1);
			break;

		case '2':
			snddev_hsed_config_modify_setting(2);
			break;

		default:
			break;
		}
	}
	return cnt;
}

static int snddev_hsed_config_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations snddev_hsed_config_debug_fops = {
	.open = snddev_hsed_config_debug_open,
	.write = snddev_hsed_config_debug_write
};
#endif

#ifndef SEC_AUDIO_DEVICE
static struct platform_device *snd_devices_ffa[] __initdata = {
	&msm_iearpiece_ffa_device,
	&msm_imic_device,
	&msm_ispkr_stereo_device,
	&msm_snddev_hdmi_stereo_rx_device,
	&msm_headset_mic_device,
	&msm_ispkr_mic_device,
	&msm_bt_sco_earpiece_device,
	&msm_bt_sco_mic_device,
	&msm_headset_stereo_device,
	&msm_itty_mono_tx_device,
	&msm_itty_mono_rx_device,
	&msm_mi2s_fm_tx_device,
	&msm_mi2s_fm_rx_device,
	&msm_hs_dual_mic_endfire_device,
	&msm_spkr_dual_mic_endfire_device,
	&msm_hs_dual_mic_broadside_device,
	&msm_spkr_dual_mic_broadside_device,
	&msm_ihs_stereo_speaker_stereo_rx_device,
	&msm_anc_headset_device,
	&msm_auxpga_lp_hs_device,
	&msm_auxpga_lp_lo_device,
	&msm_linein_pri_device,
	&msm_icodec_gpio_device,
	&msm_snddev_hdmi_non_linear_pcm_rx_device,
};
#endif

static struct platform_device *snd_devices_surf[] __initdata = {
	&msm_iearpiece_device,
	&msm_imic_device,
	&msm_ispkr_stereo_device,
	&msm_snddev_hdmi_stereo_rx_device,
	&msm_headset_mic_device,
	&msm_ispkr_mic_device,
	&msm_bt_sco_earpiece_device,
	&msm_bt_sco_mic_device,
	&msm_headset_stereo_device,
	&msm_itty_mono_tx_device,
	&msm_itty_mono_rx_device,
	&msm_mi2s_fm_tx_device,
	&msm_mi2s_fm_rx_device,
	&msm_ihs_stereo_speaker_stereo_rx_device,
	&msm_auxpga_lp_hs_device,
	&msm_auxpga_lp_lo_device,
	&msm_linein_pri_device,
	&msm_icodec_gpio_device,
	&msm_snddev_hdmi_non_linear_pcm_rx_device,
};

static struct platform_device *snd_devices_fluid[] __initdata = {
	&msm_iearpiece_device,
	&msm_imic_device,
	&msm_ispkr_stereo_device,
	&msm_snddev_hdmi_stereo_rx_device,
	&msm_headset_stereo_device,
	&msm_headset_mic_device,
	&msm_fluid_ispkr_mic_device,
	&msm_bt_sco_earpiece_device,
	&msm_bt_sco_mic_device,
	&msm_mi2s_fm_tx_device,
	&msm_mi2s_fm_rx_device,
	&msm_fluid_hs_dual_mic_endfire_device,
	&msm_fluid_spkr_dual_mic_endfire_device,
	&msm_fluid_hs_dual_mic_broadside_device,
	&msm_fluid_spkr_dual_mic_broadside_device,
	&msm_anc_headset_device,
	&msm_auxpga_lp_hs_device,
	&msm_auxpga_lp_lo_device,
	&msm_icodec_gpio_device,
	&msm_snddev_hdmi_non_linear_pcm_rx_device,
};

static struct platform_device *snd_devices_common[] __initdata = {
	&msm_aux_pcm_device,
	&msm_cdcclk_ctl_device,
#if 0 /* (-) ysseo 20110414 */
	&msm_mi2s_device,
#endif
	&msm_uplink_rx_device,
	&msm_device_dspcrashd_8x60,
};

#ifdef CONFIG_VP_A2220
extern int a2220_ioctl2(unsigned int cmd , unsigned long arg);
#endif

#ifdef CONFIG_MSM8X60_FTM_AUDIO_DEVICES
static struct platform_device *snd_devices_ftm[] __initdata = {
	&ftm_headset_mono_rx_device,
	&ftm_headset_mono_l_rx_device,
	&ftm_headset_mono_r_rx_device,
	&ftm_headset_mono_diff_rx_device,
	&ftm_spkr_mono_rx_device,
	&ftm_spkr_l_rx_device,
	&ftm_spkr_r_rx_device,
	&ftm_spkr_mono_diff_rx_device,
	&ftm_linein_l_tx_device,
	&ftm_linein_r_tx_device,
	&ftm_aux_out_rx_device,
	&ftm_dmic1_left_tx_device,
	&ftm_dmic1_right_tx_device,
	&ftm_dmic1_l_and_r_tx_device,
	&ftm_dmic2_left_tx_device,
	&ftm_dmic2_right_tx_device,
	&ftm_dmic2_l_and_r_tx_device,
	&ftm_handset_mic1_aux_in_device,
	&ftm_mi2s_sd0_rx_device,
	&ftm_mi2s_sd1_rx_device,
	&ftm_mi2s_sd2_rx_device,
	&ftm_handset_mic_adie_lp_tx_device,
	&ftm_headset_mic_adie_lp_tx_device,
	&ftm_handset_adie_lp_rx_device,
	&ftm_headset_l_adie_lp_rx_device,
	&ftm_headset_r_adie_lp_rx_device,
	&ftm_spk_l_adie_lp_rx_device,
	&ftm_spk_r_adie_lp_rx_device,
	&ftm_spk_adie_lp_rx_device,
	&ftm_handset_dual_tx_lp_device,
};
#else
static struct platform_device *snd_devices_ftm[] __initdata = {};
#endif

void __init msm_snddev_init(void)
{
	int i;
	int dev_id;

	atomic_set(&pamp_ref_cnt, 0);
	atomic_set(&preg_ref_cnt, 0);

	pr_err("%s \n",	__func__);

	for (i = 0, dev_id = 0; i < ARRAY_SIZE(snd_devices_common); i++)
		snd_devices_common[i]->id = dev_id++;

	platform_add_devices(snd_devices_common,
			ARRAY_SIZE(snd_devices_common));

	/* Auto detect device base on machine info */
	if (machine_is_msm8x60_surf() || machine_is_msm8x60_fusion()) {
		for (i = 0; i < ARRAY_SIZE(snd_devices_surf); i++)
			snd_devices_surf[i]->id = dev_id++;

		platform_add_devices(snd_devices_surf,
				ARRAY_SIZE(snd_devices_surf));
	} else if (machine_is_msm8x60_ffa() ||
			machine_is_msm8x60_fusn_ffa()) {
#ifdef SEC_AUDIO_DEVICE
		pr_err("%s snd_devices_celox - config \n", __func__);

		for (i = 0; i < ARRAY_SIZE(snd_devices_celox); i++)
			snd_devices_celox[i]->id = dev_id++;

		platform_add_devices(snd_devices_celox,
				ARRAY_SIZE(snd_devices_celox));
#else
		for (i = 0; i < ARRAY_SIZE(snd_devices_ffa); i++)
			snd_devices_ffa[i]->id = dev_id++;

		platform_add_devices(snd_devices_ffa,
				ARRAY_SIZE(snd_devices_ffa));
#endif
	} else if (machine_is_msm8x60_fluid()) {
		for (i = 0; i < ARRAY_SIZE(snd_devices_fluid); i++)
			snd_devices_fluid[i]->id = dev_id++;

		platform_add_devices(snd_devices_fluid,
				ARRAY_SIZE(snd_devices_fluid));
	}
	if (machine_is_msm8x60_surf() || machine_is_msm8x60_ffa()
		|| machine_is_msm8x60_fusion()
		|| machine_is_msm8x60_fusn_ffa()) {
		for (i = 0; i < ARRAY_SIZE(snd_devices_ftm); i++)
			snd_devices_ftm[i]->id = dev_id++;

		platform_add_devices(snd_devices_ftm,
				ARRAY_SIZE(snd_devices_ftm));
	}

#ifdef CONFIG_DEBUG_FS
	debugfs_hsed_config = debugfs_create_file("msm_hsed_config",
				S_IFREG | S_IRUGO, NULL,
		(void *) "msm_hsed_config", &snddev_hsed_config_debug_fops);
#endif

	/*
	 * Configuring SWITCH to QTR, since Audience is disabled
	 * and PCM data is re-directed to QTR directly
	 */
#ifndef CONFIG_VP_A2220
#if defined(CONFIG_TARGET_LOCALE_USA)
	/* 2MIC Reset */
	gpio_tlmm_config(GPIO_CFG(GPIO_SELECT_I2S_AUDIENCE_QTR, 0,
		GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		GPIO_CFG_ENABLE);
	gpio_set_value(GPIO_SELECT_I2S_AUDIENCE_QTR, 1);
#endif
#endif
}

