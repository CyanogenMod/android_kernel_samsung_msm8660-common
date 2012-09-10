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
#include <linux/pmic8058-othc.h>
#include <linux/mfd/pmic8901.h>
#include <linux/mfd/msm-adie-codec.h>
#include <linux/regulator/pmic8058-regulator.h>
#include <linux/regulator/pmic8901-regulator.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/machine.h>

#include <mach/qdsp6v2/audio_dev_ctl.h>
#include <sound/apr_audio.h>
#include <mach/mpp.h>
#include <asm/mach-types.h>
#include <asm/uaccess.h>

#include "snddev_icodec.h"
#include "snddev_ecodec.h"
#include "snddev_hdmi.h"
#include "snddev_mi2s.h"
#include "snddev_virtual.h"
//#if defined(CONFIG_MACH_P5_LTE) && defined(CONFIG_TARGET_LOCALE_USA_ATT)
#if defined (CONFIG_USA_MODEL_SGH_I957)
#include "timpani_profile_p5lte_usa_att.h"
#elif defined(CONFIG_MACH_P5_LTE) && defined(CONFIG_TARGET_LOCALE_EUR_OPEN)
#include "timpani_profile_p5lte_eur_open.h"
#elif defined(CONFIG_MACH_P8_LTE) && !defined(CONFIG_TARGET_LOCALE_KOR_SKT)
#include "timpani_profile_p8lte.h"  // not SKT
#elif defined(CONFIG_MACH_P4_LTE)
#include "timpani_profile_p4lte.h"
#elif defined(CONFIG_MACH_P5_LTE) && defined(CONFIG_TARGET_LOCALE_KOR_SKT)
#include "timpani_profile_p5lte_kor_skt.h"
#elif defined(CONFIG_MACH_P8_LTE) && defined(CONFIG_TARGET_LOCALE_KOR_SKT)
#include "timpani_profile_p8lte_kor_skt.h" // SKT
#elif defined(CONFIG_MACH_P5_LTE) && defined(CONFIG_TARGET_LOCALE_KOR_LGU)
#include "timpani_profile_p5lte_kor_lgu.h"
#else
#include "timpani_profile_8x60.h"
#endif
#include "snddev_hdmi.h"
#include "snddev_mi2s.h"


#ifdef CONFIG_SENSORS_YDA160
#include <linux/i2c/yda160.h>
#endif
#ifdef CONFIG_SENSORS_MAX9879
#include <linux/i2c/max9879.h>
#endif
#ifdef CONFIG_WM8994_AMP
#include <linux/i2c/wm8994_amp.h>
#endif

#ifdef CONFIG_DEBUG_FS
static struct dentry *debugfs_hsed_config;
static void snddev_hsed_config_modify_setting(int type);
static void snddev_hsed_config_restore_setting(void);
#endif


/* GPIO_CLASS_D0_EN */
#define SNDDEV_GPIO_CLASS_D0_EN 227

/* GPIO_CLASS_D1_EN */
#define SNDDEV_GPIO_CLASS_D1_EN 229

#define PMIC_GPIO_MUTE_CON		PM8058_GPIO(26)  	/* PMIC GPIO Number 26 */
#define PMIC_GPIO_AMP_EN		PM8058_GPIO(20)	/* PMIC GPIO Number 20 */


#define PMIC_GPIO_MAIN_MICBIAS_EN PM8058_GPIO(28)
#if defined(CONFIG_MACH_P8_LTE) //kth_110922
#define PMIC_GPIO_SUB_MICBIAS_EN PM8058_GPIO(37)
#endif
#define SNDDEV_GPIO_MIC2_ANCR_SEL 294
#define SNDDEV_GPIO_MIC1_ANCL_SEL 295

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

static int msm_qt_icodec_gpio_defaults[] = {
	0,
};

static struct resource msm_qt_icodec_gpio_resources[] = {
	{
		.name   = "msm_icodec_speaker_gpio",
		.start  = SNDDEV_GPIO_CLASS_D0_EN,
		.end    = SNDDEV_GPIO_CLASS_D0_EN,
		.flags  = IORESOURCE_IO,
	},
};

static struct platform_device msm_qt_icodec_gpio_device = {
	.name   = "msm_icodec_gpio",
	.num_resources  = ARRAY_SIZE(msm_qt_icodec_gpio_resources),
	.resource       = msm_qt_icodec_gpio_resources,
	.dev = { .platform_data = &msm_qt_icodec_gpio_defaults },
};

static struct regulator *s3;
static struct regulator *mvs;

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

static struct regulator *l11;

static int msm_snddev_enable_qt_dmic_power(void)
{
	int ret;

	l11 = regulator_get(NULL, "8058_l11");
	if (IS_ERR(l11))
		return -EBUSY;

	ret = regulator_set_voltage(l11, 1500000, 1500000);
	if (ret) {
		pr_err("%s: error setting regulator\n", __func__);
		goto fail_l11;
	}
	ret = regulator_enable(l11);
	if (ret) {
		pr_err("%s: error enabling regulator\n", __func__);
		goto fail_l11;
	}
	return 0;

fail_l11:
	regulator_put(l11);
	l11 = NULL;
	return ret;
}


static void msm_snddev_disable_qt_dmic_power(void)
{
	int ret;

	if (l11) {
		ret = regulator_disable(l11);
		if (ret < 0)
			pr_err("%s: error disabling regulator l11\n", __func__);
		regulator_put(l11);
		l11 = NULL;
	}
}

#define PM8901_MPP_3 (2) /* PM8901 MPP starts from 0 */
#if !defined(CONFIG_MACH_P5_LTE) && !defined(CONFIG_MACH_P8_LTE) && !defined (CONFIG_MACH_P4_LTE)
static int config_class_d1_gpio(int enable)
{
	int rc;

	if (enable) {
		rc = gpio_request(SNDDEV_GPIO_CLASS_D1_EN, "CLASSD1_EN");
		if (rc) {
			pr_err("%s: spkr pamp gpio %d request"
			"failed\n", __func__, SNDDEV_GPIO_CLASS_D1_EN);
			return rc;
		}
		gpio_direction_output(SNDDEV_GPIO_CLASS_D1_EN, 1);
	} else {
		gpio_set_value_cansleep(SNDDEV_GPIO_CLASS_D1_EN, 0);
		gpio_free(SNDDEV_GPIO_CLASS_D1_EN);
	}
	return 0;
}

static int config_class_d0_gpio(int enable)
{
	int rc;

	if (enable) {
		rc = pm8901_mpp_config_digital_out(PM8901_MPP_3,
			PM8901_MPP_DIG_LEVEL_MSMIO, 1);

		if (rc) {
			pr_err("%s: CLASS_D0_EN failed\n", __func__);
			return rc;
		}

		rc = gpio_request(SNDDEV_GPIO_CLASS_D0_EN, "CLASSD0_EN");

		if (rc) {
			pr_err("%s: spkr pamp gpio pm8901 mpp3 request"
			"failed\n", __func__);
			pm8901_mpp_config_digital_out(PM8901_MPP_3,
			PM8901_MPP_DIG_LEVEL_MSMIO, 0);
			return rc;
		}

		gpio_direction_output(SNDDEV_GPIO_CLASS_D0_EN, 1);
		gpio_set_value(SNDDEV_GPIO_CLASS_D0_EN, 1);

	} else {
		pm8901_mpp_config_digital_out(PM8901_MPP_3,
		PM8901_MPP_DIG_LEVEL_MSMIO, 0);
		gpio_set_value(SNDDEV_GPIO_CLASS_D0_EN, 0);
		gpio_free(SNDDEV_GPIO_CLASS_D0_EN);
	}
	return 0;
}
#endif

#if defined(CONFIG_MACH_P5_LTE) || defined(CONFIG_MACH_P8_LTE) || defined(CONFIG_MACH_P4_LTE)
int ear_amp_en = -1;
static void config_mute_con_gpio(int enable)
{
	printk("system rev : %d\n",system_rev);
#ifdef CONFIG_MACH_P5_LTE
	if(system_rev>=04)
		enable = !enable;
#endif
#ifdef CONFIG_MACH_P4_LTE
		if(system_rev>=03)
			enable = !enable;
#endif

	if(ear_amp_en == enable)
		return;

	if (enable) {
		printk("%s : MUTE_CON enable : %d\n",__func__,enable);
#ifdef CONFIG_MACH_P8_LTE
		gpio_set_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MUTE_CON), 0);
#else
		gpio_set_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MUTE_CON), 1);
#endif
		ear_amp_en = 1;
	} else {
		printk("%s : MUTE_CON disable : %d\n",__func__,enable);
#ifdef CONFIG_MACH_P8_LTE
		gpio_set_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MUTE_CON), 1);
#else
		gpio_set_value_cansleep(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MUTE_CON), 0);
#endif
		ear_amp_en = 0;
	}
	return;
}

static int msm_snddev_amp_on_speaker(void)
{
	pr_err("%s\n", __func__);
#ifdef CONFIG_SENSORS_YDA160
	yda160_speaker_on();
#endif

#ifdef CONFIG_SENSORS_MAX9879
		msleep(50);
		max9879_i2c_speaker_onoff(1);
#endif
#ifdef CONFIG_WM8994_AMP
		msleep(50);
		wm8994_set_speaker(1);
#endif

	 return 0;
}

#if defined(CONFIG_MACH_P8_LTE) //kks_110915_1
static int msm_snddev_amp_on_normal_speaker(void)
{
	pr_err("%s\n", __func__);
#ifdef CONFIG_SENSORS_YDA160
	yda160_speaker_on();
#endif

#ifdef CONFIG_SENSORS_MAX9879
		msleep(50);
		max9879_i2c_speaker_onoff(1);
#endif
#ifdef CONFIG_WM8994_AMP
		msleep(50);
		wm8994_set_normal_speaker(1);
#endif

	 return 0;
}
#endif

static int msm_snddev_amp_on_cradle(void)
{
	pr_err("%s\n", __func__);
#ifdef CONFIG_SENSORS_YDA160
	yda160_speaker_on();
#endif


#ifdef CONFIG_SENSORS_MAX9879
		msleep(50);
		max9879_i2c_speaker_onoff(1);
#endif
#ifdef CONFIG_WM8994_AMP
		msleep(50);
		wm8994_set_cradle(1);
#endif	 

	 return 0;
}

static int msm_snddev_amp_on_headset(void)
{
	pr_err("%s\n", __func__);
#ifdef CONFIG_SENSORS_YDA160
	yda160_headset_on();
#endif


#ifdef CONFIG_WM8994_AMP
	 msleep(50);
	 wm8994_set_headset(1);
#endif	
	msleep(30); // mute con
#if defined(CONFIG_MACH_P5_LTE) || defined(CONFIG_MACH_P8_LTE) || defined(CONFIG_MACH_P4_LTE)
	config_mute_con_gpio(1); // P4,P5,P8 amp & mute con (rev)
#endif
	return 0;
}

#if defined(CONFIG_MACH_P8_LTE) //kks_110916_1
static int msm_snddev_amp_on_normal_headset(void)
{
	pr_err("%s\n", __func__);
#ifdef CONFIG_SENSORS_YDA160
	yda160_headset_on();
#endif


#ifdef CONFIG_WM8994_AMP
	 msleep(50);
	 wm8994_set_normal_headset(1);
#endif	
	msleep(30); // mute con
#if defined(CONFIG_MACH_P5_LTE) || defined(CONFIG_MACH_P8_LTE) || defined(CONFIG_MACH_P4_LTE)
	config_mute_con_gpio(1); // P4,P5,P8 amp & mute con (rev)
#endif
	return 0;
}
#endif

static int msm_snddev_amp_on_speaker_headset(void)
{
	pr_err("%s\n", __func__);
#ifdef CONFIG_SENSORS_YDA160
	yda160_speaker_headset_on();
#endif


#ifdef CONFIG_SENSORS_MAX9879
		max9879_i2c_speaker_onoff(1);
#endif

#ifdef CONFIG_WM8994_AMP
		msleep(50); 
		wm8994_set_speaker_headset(1);
#endif	
		msleep(30); // for mute con
#if defined(CONFIG_MACH_P5_LTE) || defined(CONFIG_MACH_P8_LTE) || defined(CONFIG_MACH_P4_LTE)
		config_mute_con_gpio(1); // P4,P5,P8 amp & mute con (rev)
#endif		
	return 0;
}

static int msm_snddev_amp_on_spk_cradle(void)
{
		pr_err("%s\n", __func__);
#ifdef CONFIG_SENSORS_YDA160
		yda160_speaker_headset_on();
#endif
	
#ifdef CONFIG_SENSORS_MAX9879
			max9879_i2c_speaker_onoff(1);
#endif
#ifdef CONFIG_WM8994_AMP
			msleep(50);
			wm8994_set_spk_cradle(1);
#endif	
	
		return 0;

}


static void msm_snddev_amp_off_headset(void)
{
	pr_err("%s\n", __func__);
#ifdef CONFIG_SENSORS_YDA160
	yda160_off();
#endif
#if defined(CONFIG_MACH_P5_LTE) || defined(CONFIG_MACH_P8_LTE) || defined(CONFIG_MACH_P4_LTE)
 //        config_mute_con_gpio(0); // P4,P5,P8 amp & mute con (rev)
#endif         
		msleep(30);
#ifdef CONFIG_WM8994_AMP
		 wm8994_set_headset(0);
#endif	

	return;
}

#if defined(CONFIG_MACH_P8_LTE) //kks_110916_1
static void msm_snddev_amp_off_normal_headset(void)
{
	pr_err("%s\n", __func__);
#ifdef CONFIG_SENSORS_YDA160
	yda160_off();
#endif
#if defined(CONFIG_MACH_P5_LTE) || defined(CONFIG_MACH_P8_LTE) || defined(CONFIG_MACH_P4_LTE)
 //        config_mute_con_gpio(0); // P4,P5,P8 amp & mute con (rev)
#endif         
		msleep(30);
#ifdef CONFIG_WM8994_AMP
		 wm8994_set_normal_headset(0);
#endif	

	return;
}
#endif

static void msm_snddev_amp_off_speaker(void)
{
	pr_err("%s\n", __func__);
#ifdef CONFIG_SENSORS_YDA160
	yda160_off();
#endif

#ifdef CONFIG_SENSORS_MAX9879
		max9879_i2c_speaker_onoff(0);
#endif
#ifdef CONFIG_WM8994_AMP
		wm8994_set_speaker(0);
#endif	

	return;
}

#if defined(CONFIG_MACH_P8_LTE) //kks_110915_1
static void msm_snddev_amp_off_normal_speaker(void)
{
	pr_err("%s\n", __func__);
#ifdef CONFIG_SENSORS_YDA160
	yda160_off();
#endif

#ifdef CONFIG_SENSORS_MAX9879
		max9879_i2c_speaker_onoff(0);
#endif
#ifdef CONFIG_WM8994_AMP
		wm8994_set_normal_speaker(0);
#endif	

	return;
}
#endif

static void msm_snddev_amp_off_cradle(void)
{
	pr_err("%s\n", __func__);
#ifdef CONFIG_SENSORS_YDA160
	yda160_off();
#endif

#ifdef CONFIG_SENSORS_MAX9879
		max9879_i2c_speaker_onoff(0);
#endif
#ifdef CONFIG_WM8994_AMP
		wm8994_set_cradle(0);
#endif	

	return;
}


static void msm_snddev_amp_off_speaker_headset(void)
{
	pr_err("%s\n", __func__);
#ifdef CONFIG_SENSORS_YDA160
	yda160_off();
#endif

#ifdef CONFIG_SENSORS_MAX9879
		max9879_i2c_speaker_onoff(0);
#endif
#if defined(CONFIG_MACH_P5_LTE) || defined(CONFIG_MACH_P8_LTE) || defined(CONFIG_MACH_P4_LTE)
//		config_mute_con_gpio(0); // P4,P5,P8 amp & mute con (rev)
#endif		
		msleep(30); // for mute con

#ifdef CONFIG_WM8994_AMP
		wm8994_set_speaker_headset(0);
#endif	

	return;
}

static int msm_snddev_amp_off_spk_cradle(void)
{
		pr_err("%s\n", __func__);
#ifdef CONFIG_SENSORS_YDA160
		yda160_speaker_headset_on();
#endif

#ifdef CONFIG_SENSORS_MAX9879
			max9879_i2c_speaker_onoff(0);
#endif
#ifdef CONFIG_WM8994_AMP
			wm8994_set_spk_cradle(0);
#endif	
		return 0;

}



#else
static atomic_t pamp_ref_cnt;

static int msm_snddev_poweramp_on(void)
{
	int rc;

	if (atomic_inc_return(&pamp_ref_cnt) > 1)
		return 0;

	pr_debug("%s: enable stereo spkr amp\n", __func__);
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
}

static void msm_snddev_poweramp_off(void)
{
	if (atomic_dec_return(&pamp_ref_cnt) == 0) {
		pr_debug("%s: disable stereo spkr amp\n", __func__);
		config_class_d0_gpio(0);
		if (!machine_is_msm8x60_qt())
			config_class_d1_gpio(0);
		msleep(30);
	}
}
#endif

/* Regulator 8058_l10 supplies regulator 8058_ncp. */
static struct regulator *snddev_reg_ncp;
static struct regulator *snddev_reg_l10;

static atomic_t preg_ref_cnt;

// P5 LTE, capless headset
#if defined(CONFIG_MACH_P5_LTE) || defined(CONFIG_MACH_P8_LTE)
static int msm_snddev_voltage_on(void)
{
	int rc;
	pr_debug("%s\n", __func__);

#if defined(CONFIG_TARGET_LOCALE_KOR_LGU)
	if ( system_rev == 0x05 ) {
		return 0;
	}
#endif

	if (atomic_inc_return(&preg_ref_cnt) > 1)
		return 0;

	snddev_reg_l10 = regulator_get(NULL, "8058_l10");
	if (IS_ERR(snddev_reg_l10)) {
		pr_err("%s: regulator_get(%s) failed (%ld)\n", __func__,
			"l10", PTR_ERR(snddev_reg_l10));
		return -EBUSY;
	}

	rc = regulator_set_voltage(snddev_reg_l10, 2600000, 2600000);
	if (rc < 0)
		pr_err("%s: regulator_set_voltage(l10) failed (%d)\n",
			__func__, rc);

	rc = regulator_enable(snddev_reg_l10);
	if (rc < 0)
		pr_err("%s: regulator_enable(l10) failed (%d)\n", __func__, rc);

	snddev_reg_ncp = regulator_get(NULL, "8058_ncp");
	if (IS_ERR(snddev_reg_ncp)) {
		pr_err("%s: regulator_get(%s) failed (%ld)\n", __func__,
			"ncp", PTR_ERR(snddev_reg_ncp));
		return -EBUSY;
	}

	rc = regulator_set_voltage(snddev_reg_ncp, 1800000, 1800000);
	if (rc < 0) {
		pr_err("%s: regulator_set_voltage(ncp) failed (%d)\n",
			__func__, rc);
		goto regulator_fail;
	}

	rc = regulator_enable(snddev_reg_ncp);
	if (rc < 0) {
		pr_err("%s: regulator_enable(ncp) failed (%d)\n", __func__, rc);
		goto regulator_fail;
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

#if defined(CONFIG_TARGET_LOCALE_KOR_LGU)
	if ( system_rev == 0x05) {
		return;
	}
#endif

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
#else // dummy functions
static int msm_snddev_voltage_on(void)
{
	return 0;
}
static void msm_snddev_voltage_off(void)
{
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

		ret = gpio_request(SNDDEV_GPIO_MIC1_ANCL_SEL, "MIC1_ANCL_SEL");
		if (ret) {
			pr_err("%s: mic1 ancl gpio %d request failed\n",
				__func__, SNDDEV_GPIO_MIC1_ANCL_SEL);
			gpio_free(SNDDEV_GPIO_MIC2_ANCR_SEL);
			return ret;
		}
		gpio_direction_output(SNDDEV_GPIO_MIC1_ANCL_SEL, 0);

	} 
	else {
#if defined(CONFIG_MACH_P5_LTE) || defined(CONFIG_MACH_P8_LTE)
#if defined(CONFIG_MACH_P5_LTE) && defined(CONFIG_TARGET_LOCALE_KOR_LGU)
		if(system_rev >= 0x06)	{
#else
		if(system_rev >= 0x07)	{
#endif
			gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MAIN_MICBIAS_EN), 1);
		}
		else
#endif			
			ret = pm8058_micbias_enable(OTHC_MICBIAS_0,OTHC_SIGNAL_ALWAYS_ON);
	}
	if (ret)
		pr_err("%s: Enabling amic power failed\n", __func__);
	else 
		pr_info("%s\n",__func__);

#endif
	return ret;
}

static void msm_snddev_disable_amic_power(void)
{
#ifdef CONFIG_PMIC8058_OTHC
	int ret = 0;
	if (machine_is_msm8x60_fluid()) {
		ret = pm8058_micbias_enable(OTHC_MICBIAS_0,
				OTHC_SIGNAL_OFF);
		gpio_free(SNDDEV_GPIO_MIC1_ANCL_SEL);
		gpio_free(SNDDEV_GPIO_MIC2_ANCR_SEL);
	} 
	else	{
#if defined(CONFIG_MACH_P5_LTE) || defined(CONFIG_MACH_P8_LTE)
#if defined(CONFIG_MACH_P5_LTE) && defined(CONFIG_TARGET_LOCALE_KOR_LGU)
		if(system_rev >= 0x06)	{
#else
		if(system_rev >= 0x07)	{
#endif
			gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_MAIN_MICBIAS_EN), 0);
		}
		else
#endif			
			ret = pm8058_micbias_enable(OTHC_MICBIAS_0, OTHC_SIGNAL_OFF);
	}
	if (ret)
		pr_err("%s: Disabling amic power failed\n", __func__);
	else 
		pr_info("%s\n",__func__);
#endif
}

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

static int msm_snddev_enable_dmic_sec_power(void)
{
	int ret;

	ret = msm_snddev_enable_dmic_power();
	if (ret) {
		pr_err("%s: Error: Enabling dmic power failed\n", __func__);
		return ret;
	}
#ifdef CONFIG_PMIC8058_OTHC
#if defined(CONFIG_MACH_P8_LTE) //kth_110922
       if(system_rev >= 07)	{
            gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SUB_MICBIAS_EN), 1);
       }
       else
#endif
       {
            ret = pm8058_micbias_enable(OTHC_MICBIAS_2, OTHC_SIGNAL_ALWAYS_ON);
            if (ret) {
                pr_err("%s: Error: Enabling micbias failed\n", __func__);
                msm_snddev_disable_dmic_power();
                return ret;
            }
       }
#endif
	return 0;
}

static void msm_snddev_disable_dmic_sec_power(void)
{
	msm_snddev_disable_dmic_power();

#ifdef CONFIG_PMIC8058_OTHC
#if defined(CONFIG_MACH_P8_LTE) //kth_110922
        if(system_rev >= 07) {
            gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SUB_MICBIAS_EN), 0);
            gpio_free(PMIC_GPIO_SUB_MICBIAS_EN);
       }
       else
#endif
       {
            pm8058_micbias_enable(OTHC_MICBIAS_2, OTHC_SIGNAL_OFF);
       }
#endif
}

#if defined(CONFIG_MACH_P8_LTE) && defined (CONFIG_TARGET_LOCALE_KOR_SKT)	//kkuram 2011.09.26 add submic [[
static int msm_snddev_enable_submic_power(void)
{
	int ret;
	ret = gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SUB_MICBIAS_EN), 1);

	if (ret)
		pr_err("%s: Enabling submic power failed\n", __func__);
	else 
		pr_info("%s\n",__func__);
	return 0;
}

static void msm_snddev_disable_submic_power(void)
{
	gpio_direction_output(PM8058_GPIO_PM_TO_SYS(PMIC_GPIO_SUB_MICBIAS_EN), 0);
	gpio_free(PMIC_GPIO_SUB_MICBIAS_EN);

}
#endif	//--]]

// ------- DEFINITION OF NORMAL PAIRED DEVICES ------ 
static struct adie_codec_action_unit handset_rx_48KHz_osr256_actions[] =
	ADIE_HANDSET_RX_48000_256;
static struct adie_codec_action_unit handset_tx_48KHz_osr256_actions[] =
	ADIE_HANDSET_TX_48000_256;
static struct adie_codec_action_unit speaker_rx_48KHz_osr256_actions[] =
	ADIE_SPEAKER_RX_48000_256;
static struct adie_codec_action_unit speaker_tx_48KHz_osr256_actions[] =
	ADIE_SPEAKER_TX_48000_256;
static struct adie_codec_action_unit speaker_lb_tx_48KHz_osr256_actions[] =
	ADIE_SPEAKER_LB_TX_48000_256;
#if defined(CONFIG_MACH_P8_LTE) && defined (CONFIG_TARGET_LOCALE_KOR_SKT)
static struct adie_codec_action_unit speaker_lb_rx_48KHz_osr256_actions[] =
	ADIE_SPEAKER_LB_RX_48000_256;	
static struct adie_codec_action_unit handset_lb_rx_48KHz_osr256_actions[] =
	ADIE_HANDSET_LB_RX_48000_256;
static struct adie_codec_action_unit handset_lb_tx_48KHz_osr256_actions[] =
	ADIE_HANDSET_LB_TX_48000_256;
#endif
static struct adie_codec_action_unit headset_rx_48KHz_osr256_actions[] =
	ADIE_HEADSET_RX_48000_256;
static struct adie_codec_action_unit headset_tx_48KHz_osr256_actions[] =
	ADIE_HEADSET_TX_48000_256;
#if defined (CONFIG_TARGET_LOCALE_KOR_SKT) ||defined (CONFIG_TARGET_LOCALE_KOR_LGU)
static struct adie_codec_action_unit headset_lb_rx_48KHz_osr256_actions[] =
	ADIE_HEADSET_LB_RX_48000_256;
static struct adie_codec_action_unit headset_lb_tx_48KHz_osr256_actions[] =
	ADIE_HEADSET_LB_TX_48000_256;
#endif
// ------- DEFINITION OF VT CALL PAIRED DEVICES ------ 
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

// ------- DEFINITION OF VOIP CALL PAIRED DEVICES ------ 
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


// ------- DEFINITION OF VOIP2 CALL PAIRED DEVICES ------ 
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


// ------- DEFINITION OF CALL PAIRED DEVICES ------ 
static struct adie_codec_action_unit handset_call_rx_48KHz_osr256_actions[] =
	ADIE_HANDSET_CALL_RX_48000_256;
static struct adie_codec_action_unit handset_call_tx_48KHz_osr256_actions[] =
	ADIE_HANDSET_CALL_TX_48000_256;
static struct adie_codec_action_unit speaker_call_rx_48KHz_osr256_actions[] =
	ADIE_SPEAKER_CALL_RX_48000_256;
static struct adie_codec_action_unit speaker_call_tx_48KHz_osr256_actions[] =
	ADIE_SPEAKER_CALL_TX_48000_256;
static struct adie_codec_action_unit headset_call_rx_48KHz_osr256_actions[] =
	ADIE_HEADSET_CALL_RX_48000_256;
static struct adie_codec_action_unit headset_call_tx_48KHz_osr256_actions[] =
	ADIE_HEADSET_CALL_TX_48000_256;

// ------- DEFINITION OF SPECIAL DEVICES ------ 
static struct adie_codec_action_unit dualmic_handset_tx_48KHz_osr256_actions[] =
	ADIE_HANDSET_TX_48000_256;
static struct adie_codec_action_unit dualmic_speaker_tx_48KHz_osr256_actions[] =
	ADIE_SPEAKER_TX_48000_256;
static struct adie_codec_action_unit speaker_vr_tx_48KHz_osr256_actions[] =
	ADIE_SPEAKER_VOICE_SEARCH_TX_48000_256;
static struct adie_codec_action_unit headset_vr_tx_48KHz_osr256_actions[] =
	ADIE_HEADSET_VOICE_SEARCH_TX_48000_256;
static struct adie_codec_action_unit fm_radio_headset_rx_48KHz_osr256_actions[] =
	ADIE_HEADSET_RX_48000_256;
static struct adie_codec_action_unit fm_radio_speaker_rx_48KHz_osr256_actions[] =
	ADIE_SPEAKER_RX_48000_256;

// ------- DEFINITION OF EXTERNAL DEVICES ------ 
static struct adie_codec_action_unit lineout_rx_48KHz_osr256_actions[] =
	ADIE_SPEAKER_RX_48000_256;
static struct adie_codec_action_unit tty_headset_rx_48KHz_osr256_actions[] =
	ADIE_HEADSET_RX_48000_256;
static struct adie_codec_action_unit tty_headset_tx_48KHz_osr256_actions[] =
	ADIE_HEADSET_TX_48000_256;
static struct adie_codec_action_unit speaker_headset_rx_48KHz_osr256_actions[] =
	ADIE_SPEAKER_HEADSET_RX_48000_256;
static struct adie_codec_action_unit speaker_lineout_rx_48KHz_osr256_actions[] =
	ADIE_SPEAKER_RX_48000_256;

#if defined(CONFIG_MACH_P5_LTE) && defined(CONFIG_TARGET_LOCALE_KOR_LGU)
struct adie_codec_action_unit headset_rx_legacy_48KHz_osr256_actions[] =
	ADIE_HEADSET_RX_48000_256_LEGACY;
struct adie_codec_action_unit headset_vt_rx_legacy_48KHz_osr256_actions[] =
	ADIE_HEADSET_RX_48000_256_LEGACY;
struct adie_codec_action_unit headset_voip_rx_legacy_48KHz_osr256_actions[] =
	ADIE_HEADSET_RX_48000_256_LEGACY;
struct adie_codec_action_unit headset_voip2_rx_legacy_48KHz_osr256_actions[] =
	ADIE_HEADSET_RX_48000_256_LEGACY;
struct adie_codec_action_unit fm_radio_headset_rx_legacy_48KHz_osr256_actions[] =
	ADIE_HEADSET_RX_48000_256_LEGACY;
struct adie_codec_action_unit tty_headset_rx_legacy_48KHz_osr256_actions[] =
	ADIE_HEADSET_RX_48000_256_LEGACY;
struct adie_codec_action_unit headset_call_rx_legacy_48KHz_osr256_actions[] =
	ADIE_HEADSET_CALL_RX_48000_256_LEGACY;
struct adie_codec_action_unit speaker_headset_rx_legacy_48KHz_osr256_actions[] =
	ADIE_SPEAKER_HEADSET_RX_48000_256_LEGACY;
#endif

#if 1
static struct adie_codec_action_unit dualmic_handset_call_tx_48KHz_osr256_actions[] =
	ADIE_DUALMIC_HANDSET_CALL_TX_48000_256;

static struct adie_codec_hwsetting_entry dualmic_handset_call_tx_settings[] = {
	{
#ifdef CONFIG_VP_A2220
		.freq_plan = 16000,
#else
		.freq_plan = 48000,
#endif
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

#endif


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
static struct adie_codec_hwsetting_entry speaker_lb_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = speaker_lb_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_lb_tx_48KHz_osr256_actions),
	}
};
#if defined(CONFIG_MACH_P8_LTE) && defined (CONFIG_TARGET_LOCALE_KOR_SKT)
static struct adie_codec_hwsetting_entry speaker_lb_rx_settings[] = {
	{
#ifdef CONFIG_VP_A2220
		.freq_plan = 16000,
#else
		.freq_plan = 48000,
#endif
		.osr = 256,
		.actions = speaker_lb_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_lb_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry handset_lb_rx_settings[] = {
	{
#ifdef CONFIG_VP_A2220
        .freq_plan = 16000,
#else
		.freq_plan = 48000,
#endif
		.osr = 256,
		.actions = handset_lb_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_lb_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry handset_lb_tx_settings[] = {
	{
#ifdef CONFIG_VP_A2220
		.freq_plan = 16000,
#else
		.freq_plan = 48000,
#endif
		.osr = 256,
		.actions = handset_lb_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_lb_tx_48KHz_osr256_actions),
	}
};
#endif
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

#if defined (CONFIG_TARGET_LOCALE_KOR_SKT) ||defined (CONFIG_TARGET_LOCALE_KOR_LGU)
static struct adie_codec_hwsetting_entry headset_lb_rx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = headset_lb_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_lb_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry headset_lb_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = headset_lb_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_lb_tx_48KHz_osr256_actions),
	}
};
#endif
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


// ------- DEFINITION OF VOIP CALL PAIRED DEVICES ------ 
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


// ------- DEFINITION OF CALL PAIRED DEVICES ------ 
static struct adie_codec_hwsetting_entry handset_call_rx_settings[] = {
	{
#ifdef CONFIG_VP_A2220
        .freq_plan = 16000,
#else
		.freq_plan = 48000,
#endif
		.osr = 256,
		.actions = handset_call_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_call_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry handset_call_tx_settings[] = {
	{
#ifdef CONFIG_VP_A2220
		.freq_plan = 16000,
#else
		.freq_plan = 48000,
#endif
		.osr = 256,
		.actions = handset_call_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(handset_call_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_call_rx_settings[] = {
	{
#ifdef CONFIG_VP_A2220
		.freq_plan = 16000,
#else
		.freq_plan = 48000,
#endif
		.osr = 256,
		.actions = speaker_call_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_call_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry speaker_call_tx_settings[] = {
	{
#ifdef CONFIG_VP_A2220
		.freq_plan = 16000,
#else
		.freq_plan = 48000,
#endif
		.osr = 256,
		.actions = speaker_call_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(speaker_call_tx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry headset_call_rx_settings[] = {
	{
#ifdef CONFIG_VP_A2220
		.freq_plan = 16000,
#else
		.freq_plan = 48000,
#endif
		.osr = 256,
		.actions = headset_call_rx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_call_rx_48KHz_osr256_actions),
	}
};
static struct adie_codec_hwsetting_entry headset_call_tx_settings[] = {
	{
#ifdef CONFIG_VP_A2220
		.freq_plan = 16000,
#else
		.freq_plan = 48000,
#endif
		.osr = 256,
		.actions = headset_call_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(headset_call_tx_48KHz_osr256_actions),
	}
};


// ------- DEFINITION OF SPECIAL DEVICES ------ 
static struct adie_codec_hwsetting_entry dualmic_handset_tx_settings[] = {
	{
		.freq_plan = 48000,
		.osr = 256,
		.actions = dualmic_handset_tx_48KHz_osr256_actions,
		.action_sz = ARRAY_SIZE(dualmic_handset_tx_48KHz_osr256_actions),
	}
};
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
static struct adie_codec_dev_profile speaker_lb_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = speaker_lb_tx_settings,
	.setting_sz = ARRAY_SIZE(speaker_lb_tx_settings),
};
#if defined(CONFIG_MACH_P8_LTE) && defined (CONFIG_TARGET_LOCALE_KOR_SKT)
static struct adie_codec_dev_profile speaker_lb_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = speaker_lb_rx_settings,
	.setting_sz = ARRAY_SIZE(speaker_lb_rx_settings),
};	
static struct adie_codec_dev_profile handset_lb_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = handset_lb_rx_settings,
	.setting_sz = ARRAY_SIZE(handset_lb_rx_settings),
};
static struct adie_codec_dev_profile handset_lb_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = handset_lb_tx_settings,
	.setting_sz = ARRAY_SIZE(handset_lb_tx_settings),
};
#endif
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
#if defined (CONFIG_TARGET_LOCALE_KOR_SKT) ||defined (CONFIG_TARGET_LOCALE_KOR_LGU)
static struct adie_codec_dev_profile headset_lb_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = headset_lb_rx_settings,
	.setting_sz = ARRAY_SIZE(headset_lb_rx_settings),
};
static struct adie_codec_dev_profile headset_lb_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = headset_lb_tx_settings,
	.setting_sz = ARRAY_SIZE(headset_lb_tx_settings),
};
#endif

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

// ------- DEFINITION OF VOIP CALL PAIRED DEVICES ------ 
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


// ------- DEFINITION OF CALL PAIRED DEVICES ------ 
static struct adie_codec_dev_profile handset_call_rx_profile = {
	.path_type = ADIE_CODEC_RX,
	.settings = handset_call_rx_settings,
	.setting_sz = ARRAY_SIZE(handset_call_rx_settings),
};
static struct adie_codec_dev_profile handset_call_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = handset_call_tx_settings,
	.setting_sz = ARRAY_SIZE(handset_call_tx_settings),
};
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

// ------- DEFINITION OF SPECIAL DEVICES ------ 
static struct adie_codec_dev_profile dualmic_handset_tx_profile = {
	.path_type = ADIE_CODEC_TX,
	.settings = dualmic_handset_tx_settings,
	.setting_sz = ARRAY_SIZE(dualmic_handset_tx_settings),
};
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
};

static struct snddev_icodec_data handset_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_tx",
	.copp_id = 1,
	.profile = &handset_tx_profile,
//	.profile = &imic_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
};

#if defined(CONFIG_MACH_P8_LTE) //kks_110915_1
static struct snddev_icodec_data speaker_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_rx",
	.copp_id = 0,
	.profile = &speaker_rx_profile,
//	.profile = &ispkr_stereo_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_amp_on_normal_speaker,
	.pamp_off = msm_snddev_amp_off_normal_speaker,
};
#else
static struct snddev_icodec_data speaker_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_rx",
	.copp_id = 0,
	.profile = &speaker_rx_profile,
//	.profile = &ispkr_stereo_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,
};
#endif

static struct snddev_icodec_data speaker_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_tx_profile,
//	.profile = &idmic_mono_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,//msm_snddev_enable_dmic_power,
	.pamp_off = msm_snddev_disable_amic_power,//msm_snddev_disable_dmic_power,
};

static struct snddev_icodec_data speaker_lb_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_lb_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_lb_tx_profile,
//	.profile = &idmic_mono_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
#if defined(CONFIG_MACH_P8_LTE) && defined (CONFIG_TARGET_LOCALE_KOR_SKT)	//kkuram 2011.09.26 add submic 
	.pamp_on = msm_snddev_enable_submic_power,
	.pamp_off = msm_snddev_disable_submic_power,
#else	
	.pamp_on = msm_snddev_enable_amic_power,//msm_snddev_enable_dmic_power,
	.pamp_off = msm_snddev_disable_amic_power,//msm_snddev_disable_dmic_power,
#endif	
};

#if defined(CONFIG_MACH_P8_LTE) && defined (CONFIG_TARGET_LOCALE_KOR_SKT)
static struct snddev_icodec_data speaker_lb_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_lb_rx",
	.copp_id = 0,
	.profile = &speaker_lb_rx_profile,
	.channel_mode = 2,
#ifdef CONFIG_VP_A2220
	.default_sample_rate = 16000,
	.pamp_on = msm_snddev_audience_speaker_on,
	.pamp_off = msm_snddev_audience_speaker_off,	
#else		
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,
#endif
};

static struct snddev_icodec_data handset_lb_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "handset_lb_rx",
	.copp_id = 0,
	.profile = &handset_lb_rx_profile,
	.channel_mode = 1,
#ifdef CONFIG_VP_A2220
	.default_sample_rate = 16000,
	.pamp_on = msm_snddev_setting_audience_call_connect,
	.pamp_off = msm_snddev_setting_audience_call_disconnect,
#else	
	.default_sample_rate = 48000,
#endif
};

static struct snddev_icodec_data handset_lb_tx_data = {
#ifdef CONFIG_VP_A2220
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_lb_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &dualmic_handset_call_tx_profile,
	.channel_mode = 2,
	.default_sample_rate = 16000,	
	.pamp_on = msm_snddev_enable_amic_power, 
	.pamp_off = msm_snddev_disable_amic_power, 
#else	
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "handset_lb_tx",
	.copp_id = 1,
	.profile = &handset_lb_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#endif
};
#endif

#if defined(CONFIG_MACH_P8_LTE) //kks_110916_1
static struct snddev_icodec_data headset_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_rx",
	.copp_id = 0,
	.profile = &headset_rx_profile,
//	.profile = &headset_ab_cpls_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_amp_on_normal_headset,
	.pamp_off = msm_snddev_amp_off_normal_headset,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};
#else
static struct snddev_icodec_data headset_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_rx",
	.copp_id = 0,
	.profile = &headset_rx_profile,
//	.profile = &headset_ab_cpls_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_amp_on_headset,
	.pamp_off = msm_snddev_amp_off_headset,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};
#endif

static struct snddev_icodec_data headset_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "headset_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &headset_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
};

#if defined (CONFIG_TARGET_LOCALE_KOR_SKT) ||defined (CONFIG_TARGET_LOCALE_KOR_LGU)
static struct snddev_icodec_data headset_lb_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_lb_rx",
	.copp_id = 0,
	.profile = &headset_lb_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_amp_on_headset,
	.pamp_off = msm_snddev_amp_off_headset,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};


static struct snddev_icodec_data headset_lb_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "headset_lb_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &headset_lb_tx_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
};
#endif


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
//	.profile = &imic_profile,
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
//	.profile = &ispkr_stereo_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,
};

static struct snddev_icodec_data speaker_vt_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_vt_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_vt_tx_profile,
//	.profile = &idmic_mono_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
};

static struct snddev_icodec_data headset_vt_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_vt_rx",
	.copp_id = 0,
	.profile = &headset_vt_rx_profile,
//	.profile = &headset_ab_cpls_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_amp_on_headset,
	.pamp_off = msm_snddev_amp_off_headset,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_icodec_data headset_vt_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "headset_vt_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &headset_vt_tx_profile,
//	.profile = &iheadset_mic_profile,
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
//	.profile = &imic_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
};

static struct snddev_icodec_data speaker_voip_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_voip_rx",
	.copp_id = 0,
	.profile = &speaker_voip_rx_profile,
//	.profile = &ispkr_stereo_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,
};

static struct snddev_icodec_data speaker_voip_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_voip_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_voip_tx_profile,
//	.profile = &idmic_mono_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
#if defined(CONFIG_MACH_P8_LTE) && defined (CONFIG_TARGET_LOCALE_KOR_SKT) //kks_111004
	.pamp_on = msm_snddev_enable_submic_power,
	.pamp_off = msm_snddev_disable_submic_power,
#else 
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#endif
};

static struct snddev_icodec_data headset_voip_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_voip_rx",
	.copp_id = 0,
	.profile = &headset_voip_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_amp_on_headset,
	.pamp_off = msm_snddev_amp_off_headset,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_icodec_data headset_voip_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "headset_voip_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &headset_voip_tx_profile,
//	.profile = &iheadset_mic_profile,
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


// ------- DEFINITION OF VOIP CALL PAIRED DEVICES ------ 
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
//	.profile = &imic_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
};

static struct snddev_icodec_data speaker_voip2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_voip2_rx",
	.copp_id = 0,
	.profile = &speaker_voip2_rx_profile,
//	.profile = &ispkr_stereo_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,
};

static struct snddev_icodec_data speaker_voip2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_voip2_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_voip2_tx_profile,
//	.profile = &idmic_mono_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
#if defined(CONFIG_MACH_P8_LTE) && defined (CONFIG_TARGET_LOCALE_KOR_SKT) //kks_111007_1
	.pamp_on = msm_snddev_enable_submic_power,
	.pamp_off = msm_snddev_disable_submic_power,
#else 
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
#endif
};

static struct snddev_icodec_data headset_voip2_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "headset_voip2_rx",
	.copp_id = 0,
	.profile = &headset_voip2_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_amp_on_headset,
	.pamp_off = msm_snddev_amp_off_headset,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,
};

static struct snddev_icodec_data headset_voip2_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "headset_voip2_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &headset_voip2_tx_profile,
//	.profile = &iheadset_mic_profile,
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


// Add
// ------- DEFINITION OF CALL PAIRED DEVICES ------ 
static struct snddev_icodec_data handset_call_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "handset_call_rx",
	.copp_id = 0,
	.profile = &handset_call_rx_profile,
	.channel_mode = 1,
#ifdef CONFIG_VP_A2220
	.default_sample_rate = 16000,
	.pamp_on = msm_snddev_setting_audience_call_connect,
	.pamp_off = msm_snddev_setting_audience_call_disconnect,
#else	
	.default_sample_rate = 48000,
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
	.pamp_on = msm_snddev_enable_amic_power, //msm_snddev_enable_audience_amic_power,
	.pamp_off = msm_snddev_disable_amic_power, //msm_snddev_disable_audience_amic_power,
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
#ifdef CONFIG_VP_A2220
	.default_sample_rate = 16000,
	.pamp_on = msm_snddev_audience_speaker_on,
	.pamp_off = msm_snddev_audience_speaker_off,	
#else		
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,
#endif
};

static struct snddev_icodec_data speaker_call_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_call_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &speaker_call_tx_profile,
	.channel_mode = 1,
#ifdef CONFIG_VP_A2220
	.default_sample_rate = 16000,
#else		
	.default_sample_rate = 48000,
#endif
#if defined(CONFIG_MACH_P8_LTE) && defined (CONFIG_TARGET_LOCALE_KOR_SKT)	//kkuram 2011.09.26 add submic 
	.pamp_on = msm_snddev_enable_submic_power,
	.pamp_off = msm_snddev_disable_submic_power,
#else
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
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
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_amp_on_headset,
	.pamp_off = msm_snddev_amp_off_headset,
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
	.default_sample_rate = 48000,
#endif
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
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "dualmic_handset_ef_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &dualmic_handset_tx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
};

static struct snddev_icodec_data dualmic_speaker_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "dualmic_speaker_ef_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &dualmic_speaker_tx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_amic_power,
	.pamp_off = msm_snddev_disable_amic_power,
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
	.profile = &fm_radio_headset_rx_profile, //  headset_fmradio_rx_profile,
	.channel_mode = 1,
	.default_sample_rate = 8000,
	.pamp_on = msm_snddev_amp_on_headset,
	.pamp_off = msm_snddev_amp_off_headset,
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
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,

};

static struct snddev_mi2s_data fm_radio_tx_data = {
	.capability = SNDDEV_CAP_TX ,
	.name = "fm_radio_tx",
	.copp_id = PRIMARY_I2S_TX,
	.channel_mode = 2,   //check
	.sd_lines = MI2S_SD3,  //check
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
	.pamp_on = msm_snddev_amp_on_cradle,
	.pamp_off = msm_snddev_amp_off_cradle,

};

static struct snddev_icodec_data tty_headset_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "tty_headset_rx",
	.copp_id = 0,
	.profile = &tty_headset_rx_profile,
	.channel_mode = 2,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_amp_on_headset,
	.pamp_off = msm_snddev_amp_off_headset,
	.voltage_on = msm_snddev_voltage_on,
	.voltage_off = msm_snddev_voltage_off,

};

static struct snddev_icodec_data tty_headset_tx_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
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
	.pamp_on = msm_snddev_amp_on_speaker_headset,
	.pamp_off = msm_snddev_amp_off_speaker_headset,
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
	.pamp_on = msm_snddev_amp_on_spk_cradle,
	.pamp_off = msm_snddev_amp_off_spk_cradle,

};

static struct snddev_hdmi_data speaker_hdmi_rx_data = {
	.capability = (SNDDEV_CAP_RX | SNDDEV_CAP_VOICE),
	.name = "speaker_hdmi_rx",
	.copp_id = HDMI_RX,
	.channel_mode = 0,
	.default_sample_rate = 48000,
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
static struct platform_device device_speaker_lb_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_lb_tx_data },
};
#if defined(CONFIG_MACH_P8_LTE) && defined (CONFIG_TARGET_LOCALE_KOR_SKT)
static struct platform_device device_speaker_lb_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &speaker_lb_rx_data },
};	
static struct platform_device device_handset_lb_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_lb_rx_data },
};
static struct platform_device device_handset_lb_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &handset_lb_tx_data },
};
#endif
static struct platform_device device_headset_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_rx_data },
};
static struct platform_device device_headset_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_tx_data },
};

#if defined (CONFIG_TARGET_LOCALE_KOR_SKT) ||defined (CONFIG_TARGET_LOCALE_KOR_LGU)
static struct platform_device device_headset_lb_rx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_lb_rx_data },
};
static struct platform_device device_headset_lb_tx = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &headset_lb_tx_data },
};
#endif

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


// ------- DEFINITION OF VOIP CALL PAIRED DEVICES ------ 
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
	.name = "snddev_hdmi",//"snddev_mi2s",
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
	.name = "snddev_hdmi",//"snddev_icodec",
	.dev = { .platform_data = &speaker_hdmi_rx_data },
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

	// ------- DEFINITION OF VOIP CALL PAIRED DEVICES ------ 
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
};

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
	.pamp_on = msm_snddev_amp_on_headset,
	.pamp_off = msm_snddev_amp_off_headset,
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
	.pamp_on = msm_snddev_enable_anc_power,
	.pamp_off = msm_snddev_disable_anc_power,
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
	//.pamp_on = msm_snddev_poweramp_on,
	//.pamp_off = msm_snddev_poweramp_off,
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,
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

static struct snddev_icodec_data snddev_qt_dual_dmic_d0_data = {
	.capability = (SNDDEV_CAP_TX | SNDDEV_CAP_VOICE),
	.name = "speaker_mono_tx",
	.copp_id = PRIMARY_I2S_TX,
	.profile = &idmic_mono_profile,
	.channel_mode = 1,
	.default_sample_rate = 48000,
	.pamp_on = msm_snddev_enable_qt_dmic_power,
	.pamp_off = msm_snddev_disable_qt_dmic_power,
};

static struct platform_device msm_qt_dual_dmic_d0_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &snddev_qt_dual_dmic_d0_data },
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
	//.pamp_on = msm_snddev_poweramp_on,
	//.pamp_off = msm_snddev_poweramp_off,
	.pamp_on = msm_snddev_amp_on_speaker_headset,
	.pamp_off = msm_snddev_amp_off_speaker_headset,
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
	//.pamp_on = msm_snddev_poweramp_on,
	//.pamp_off = msm_snddev_poweramp_off,
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,
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

static struct adie_codec_action_unit ftm_headset_mono_rx_actions[] =
	HEADSET_AB_CPLS_48000_OSR_256;

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
	//.pamp_on = msm_snddev_poweramp_on,
	//.pamp_off = msm_snddev_poweramp_off,
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,
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
	//.pamp_on = msm_snddev_poweramp_on,
	//.pamp_off = msm_snddev_poweramp_off,
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,
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
	//.pamp_on = msm_snddev_poweramp_on,
	//.pamp_off = msm_snddev_poweramp_off,
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,
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
	//.pamp_on = msm_snddev_poweramp_on,
	//.pamp_off = msm_snddev_poweramp_off,
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,
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
	//.pamp_on = msm_snddev_poweramp_on,
	//.pamp_off = msm_snddev_poweramp_off,
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,
};

static struct platform_device ftm_spk_l_adie_lp_rx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_spkr_l_rx_lp_data},
};

static struct adie_codec_action_unit ftm_spkr_r_adie_lp_rx_actions[] =
	FTM_SPKR_RX_LB;

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
	//.pamp_on = msm_snddev_poweramp_on,
	//.pamp_off = msm_snddev_poweramp_off,
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,
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
	//.pamp_on = msm_snddev_poweramp_on,
	//.pamp_off = msm_snddev_poweramp_off,
	.pamp_on = msm_snddev_amp_on_speaker,
	.pamp_off = msm_snddev_amp_off_speaker,
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
};

static struct platform_device ftm_headset_mic_adie_lp_tx_device = {
	.name = "snddev_icodec",
	.dev = { .platform_data = &ftm_headset_mic_adie_lp_tx_data },
};

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

static struct platform_device *snd_devices_ffa[] __initdata = {
	&msm_iearpiece_ffa_device,
	&msm_imic_ffa_device,
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
	&msm_anc_headset_device,
	&msm_auxpga_lp_hs_device,
	&msm_auxpga_lp_lo_device,
	&msm_icodec_gpio_device,
	&msm_snddev_hdmi_non_linear_pcm_rx_device,
};

static struct platform_device *snd_devices_qt[] __initdata = {
	&msm_headset_stereo_device,
	&msm_headset_mic_device,
	&msm_ispkr_stereo_device,
	&msm_qt_dual_dmic_d0_device,
	&msm_snddev_hdmi_stereo_rx_device,
	&msm_qt_icodec_gpio_device,
};


static struct platform_device *snd_devices_p5_lte[] __initdata = {
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

// ------- DEFINITION OF VOIP CALL PAIRED DEVICES ------ 
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

// ------- DEFINITION OF CALL PAIRED DEVICES ------ 
#if defined (CONFIG_TARGET_LOCALE_KOR_SKT) ||defined (CONFIG_TARGET_LOCALE_KOR_LGU)
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
#endif

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
	&device_speaker_lb_tx,
#if defined (CONFIG_TARGET_LOCALE_KOR_SKT) ||defined (CONFIG_TARGET_LOCALE_KOR_LGU)
	&device_headset_lb_rx,
	&device_headset_lb_tx,
#endif
// ------- DEFINITION OF EXTERNAL DEVICES ------ 
	&device_hdmi_stereo_rx,
	&device_lineout_rx,
	&device_tty_headset_rx,
	&device_tty_headset_tx,
	&device_speaker_headset_rx,
	&device_speaker_lineout_rx,
	&device_speaker_hdmi_rx,

};

static struct platform_device *snd_devices_p8_lte[] __initdata = {
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
// ------- DEFINITION OF VOIP CALL PAIRED DEVICES ------ 
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
	&device_speaker_lb_tx,
#if defined(CONFIG_MACH_P8_LTE) && defined (CONFIG_TARGET_LOCALE_KOR_SKT)
	&device_headset_lb_rx,
	&device_headset_lb_tx,
	&device_speaker_lb_rx,
	&device_handset_lb_rx,
	&device_handset_lb_tx,	
#endif	

// ------- DEFINITION OF EXTERNAL DEVICES ------ 
	&device_hdmi_stereo_rx,
	&device_lineout_rx,
	&device_tty_headset_rx,
	&device_tty_headset_tx,
	&device_speaker_headset_rx,
	&device_speaker_lineout_rx,
	&device_speaker_hdmi_rx,

};

static struct platform_device *snd_devices_p4_lte[] __initdata = {
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

	// ------- DEFINITION OF VOIP CALL PAIRED DEVICES ------ 
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

// ------- DEFINITION OF CALL PAIRED DEVICES ------ 
/*
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
*/

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
	&device_speaker_lb_tx,

// ------- DEFINITION OF EXTERNAL DEVICES ------ 
	&device_hdmi_stereo_rx,
	&device_lineout_rx,
	&device_tty_headset_rx,
	&device_tty_headset_tx,
	&device_speaker_headset_rx,
	&device_speaker_lineout_rx,
	&device_speaker_hdmi_rx,

};

static struct platform_device *snd_devices_common[] __initdata = {
	&msm_aux_pcm_device,
	&msm_cdcclk_ctl_device,
	&msm_mi2s_device,
	&msm_uplink_rx_device,
};

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


void __init msm_snddev_init(void)
{
	int i;
	int dev_id;
#if !defined(CONFIG_MACH_P5_LTE) && !defined(CONFIG_MACH_P8_LTE) && !defined(CONFIG_MACH_P4_LTE)
	int rc;

	atomic_set(&pamp_ref_cnt, 0);
#endif
	atomic_set(&preg_ref_cnt, 0);

	for (i = 0, dev_id = 0; i < ARRAY_SIZE(snd_devices_common); i++)
		snd_devices_common[i]->id = dev_id++;

	platform_add_devices(snd_devices_common,
		ARRAY_SIZE(snd_devices_common));

	/* Auto detect device base on machine info */
/*	if (machine_is_msm8x60_surf() || machine_is_msm8x60_fusion()) {
		pr_info("snd_devices_surf\n");
		for (i = 0; i < ARRAY_SIZE(snd_devices_surf); i++)
			snd_devices_surf[i]->id = dev_id++;

		platform_add_devices(snd_devices_surf,
		ARRAY_SIZE(snd_devices_surf));
	} else if (machine_is_msm8x60_ffa() ||
			machine_is_msm8x60_fusn_ffa()) {
		pr_info("snd_devices_ffa\n");
		for (i = 0; i < ARRAY_SIZE(snd_devices_ffa); i++)
			snd_devices_ffa[i]->id = dev_id++;

		platform_add_devices(snd_devices_ffa,
		ARRAY_SIZE(snd_devices_ffa));
	} else if (machine_is_msm8x60_fluid()) {
		pr_info("snd_devices_fluid\n");
		for (i = 0; i < ARRAY_SIZE(snd_devices_fluid); i++)
			snd_devices_fluid[i]->id = dev_id++;

		platform_add_devices(snd_devices_fluid,
		ARRAY_SIZE(snd_devices_fluid));
	} else if (machine_is_msm8x60_qt()) {
		pr_info("snd_devices_qt\n");
		for (i = 0; i < ARRAY_SIZE(snd_devices_qt); i++)
			snd_devices_qt[i]->id = dev_id++;

		platform_add_devices(snd_devices_qt,
		ARRAY_SIZE(snd_devices_qt));
	} else if (machine_is_p5_lte()){
*/		pr_info("snd_devices_p5_lte\n");
		for (i = 0; i < ARRAY_SIZE(snd_devices_p5_lte); i++)
			snd_devices_p5_lte[i]->id = dev_id++;

		platform_add_devices(snd_devices_p5_lte,
		ARRAY_SIZE(snd_devices_p5_lte));
/*	} else if (machine_is_p8_lte()){
		pr_info("snd_devices_p8_lte\n");
		for (i = 0; i < ARRAY_SIZE(snd_devices_p8_lte); i++)
			snd_devices_p8_lte[i]->id = dev_id++; //kks_110908 snd_devices_p5_lte->snd_devices_p8_lte

		platform_add_devices(snd_devices_p8_lte,
		ARRAY_SIZE(snd_devices_p8_lte));
	} else if (machine_is_p4_lte()){
		pr_info("snd_devices_p4_lte\n");
		for (i = 0; i < ARRAY_SIZE(snd_devices_p4_lte); i++)
			snd_devices_p4_lte[i]->id = dev_id++;

		platform_add_devices(snd_devices_p4_lte,
		ARRAY_SIZE(snd_devices_p4_lte));
	}
*/
	if (machine_is_msm8x60_surf()) {
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
}

