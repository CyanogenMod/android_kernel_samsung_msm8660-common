
#include <linux/i2c.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <mach/vreg.h>
#include <asm/io.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/i2c/yda165.h>

#define MODULE_NAME "yda165"
#define AMPREG_DEBUG 0
#define MODE_NUM_MAX 30

static struct yda165_i2c_data g_data;
static struct i2c_client *pclient;
static struct snd_set_ampgain g_ampgain[MODE_NUM_MAX];
static struct snd_set_ampgain temp;
static int set_mode = 0;
static int cur_mode = 0;

extern unsigned int get_hw_rev(void);


static ssize_t mode_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d\n", set_mode);
}
static ssize_t mode_store(struct device *dev,struct device_attribute *attr,
		const char *buf,size_t count)
{
    int reg = simple_strtoul(buf, NULL, 10);
	if( ( reg >= 0 ) && ( reg < MODE_NUM_MAX ) )
	{
		set_mode = reg;	
		
#if defined (CONFIG_TARGET_LOCALE_KOR) || defined (CONFIG_TARGET_LOCALE_JPN)
		temp.in1_gain = g_ampgain[set_mode].in1_gain;
		temp.in2_gain = g_ampgain[set_mode].in2_gain;
		temp.hp_att = g_ampgain[set_mode].hp_att;
		temp.hp_gainup = g_ampgain[set_mode].hp_gainup;
		temp.sp_att = g_ampgain[set_mode].sp_att;
		temp.sp_gainup = g_ampgain[set_mode].sp_gainup;
#endif

	}
	return count;
}

static ssize_t in1_gain_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d\n", temp.in1_gain);
}
static ssize_t in1_gain_store(struct device *dev,struct device_attribute *attr,
		const char *buf,size_t count)
{
    int reg = simple_strtoul(buf, NULL, 10);
	if( ( reg >= 0 ) && ( reg <= 7 ) )
	{
		temp.in1_gain = reg;
	}
	return count;
}
static ssize_t in2_gain_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d\n", temp.in2_gain);
}
static ssize_t in2_gain_store(struct device *dev,struct device_attribute *attr,
		const char *buf,size_t count)
{
    int reg = simple_strtoul(buf, NULL, 10);
	if( ( reg >= 0 ) && ( reg <= 7 ) )
	{
		temp.in2_gain = reg;
	}
	return count;
}
static ssize_t hp_att_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d\n", temp.hp_att);
}
static ssize_t hp_att_store(struct device *dev,struct device_attribute *attr,
		const char *buf,size_t count)
{
    int reg = simple_strtoul(buf, NULL, 10);
	if( ( reg >= 0 ) && ( reg <= 31 ) )
	{
		temp.hp_att = reg;
	}
	return count;
}
static ssize_t hp_gainup_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d\n", temp.hp_gainup);
}
static ssize_t hp_gainup_store(struct device *dev,struct device_attribute *attr,
		const char *buf,size_t count)
{
    int reg = simple_strtoul(buf, NULL, 10);
	if( ( reg >= 0 ) && ( reg <= 3 ) )
	{
		temp.hp_gainup = reg;
	}
	return count;
}
static ssize_t sp_att_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d\n", temp.sp_att);
}
static ssize_t sp_att_store(struct device *dev,struct device_attribute *attr,
		const char *buf,size_t count)
{
    int reg = simple_strtoul(buf, NULL, 10);
	if( ( reg >= 0 ) && ( reg <= 31 ) )
	{
		temp.sp_att = reg;
	}
	return count;
}
static ssize_t sp_gainup_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d\n", temp.sp_gainup);
}
static ssize_t sp_gainup_store(struct device *dev,struct device_attribute *attr,
		const char *buf,size_t count)
{
    int reg = simple_strtoul(buf, NULL, 10);
	if( ( reg >= 0 ) && ( reg <= 2 ) )
	{
		temp.sp_gainup = reg;
	}
	return count;
}

#if defined (CONFIG_USA_MODEL_SGH_T989)  || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_I717) \
    || defined(CONFIG_USA_MODEL_SGH_I757)|| defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577)
static ssize_t sp_Ng_DetectionLv_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d\n", temp.bSpNg_DetectionLv);
}
static ssize_t sp_Ng_DetectionLv_store(struct device *dev,struct device_attribute *attr,
		const char *buf,size_t count)
{
    int reg = simple_strtoul(buf, NULL, 10);
	if( ( reg >= 0 ) && ( reg <= 7 ) )
	{
		temp.bSpNg_DetectionLv = reg;
	}
	return count;
}

static ssize_t sp_SpNg_AttackTime_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d\n", temp.bSpNg_AttackTime);
}
static ssize_t sp_SpNg_AttackTime_store(struct device *dev,struct device_attribute *attr,
		const char *buf,size_t count)
{
    int reg = simple_strtoul(buf, NULL, 10);
	if( ( reg >= 0 ) && ( reg <= 3 ) )
	{
		temp.bSpNg_AttackTime = reg;
	}
	return count;
}

static ssize_t sp_SpNcpl_NonClipRatio_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d\n", temp.bSpNcpl_NonClipRatio);
}
static ssize_t sp_SpNcpl_NonClipRatio_store(struct device *dev,struct device_attribute *attr,
		const char *buf,size_t count)
{
    int reg = simple_strtoul(buf, NULL, 10);
	if( ( reg >= 0 ) && ( reg <= 7 ) )
	{
		temp.bSpNcpl_NonClipRatio = reg;
	}
	return count;
}

static ssize_t sp_SpNcpl_PowerLimit_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d\n", temp.bSpNcpl_PowerLimit);
}
static ssize_t sp_SpNcpl_PowerLimit_store(struct device *dev,struct device_attribute *attr,
		const char *buf,size_t count)
{
    int reg = simple_strtoul(buf, NULL, 10);
	if( ( reg >= 0 ) && ( reg <= 15 ) )
	{
		temp.bSpNcpl_PowerLimit = reg;
	}
	return count;
}

static ssize_t sp_SpNcpl_AttackTime_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d\n", temp.bSpNcpl_AttackTime);
}
static ssize_t sp_SpNcpl_AttackTime_store(struct device *dev,struct device_attribute *attr,
		const char *buf,size_t count)
{
    int reg = simple_strtoul(buf, NULL, 10);
	if( ( reg >= 0 ) && ( reg <= 3 ) )
	{
		temp.bSpNcpl_AttackTime = reg;
	}
	return count;
}

static ssize_t sp_SpNcpl_ReleaseTime_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d\n", temp.bSpNcpl_ReleaseTime);
}
static ssize_t sp_SpNcpl_ReleaseTime_store(struct device *dev,struct device_attribute *attr,
		const char *buf,size_t count)
{
    int reg = simple_strtoul(buf, NULL, 10);
	if( ( reg >= 0 ) && ( reg <= 1 ) )
	{
		temp.bSpNcpl_ReleaseTime = reg;
	}
	return count;
}

#endif

#if defined (CONFIG_USA_MODEL_SGH_T989)  || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_I717) \
    || defined(CONFIG_USA_MODEL_SGH_I757)|| defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577)
static ssize_t gain_all_show(struct device *dev,struct device_attribute *attr,char *buf)
	{
		return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d \n",
												  set_mode,
												  g_ampgain[set_mode].in1_gain ,
												  g_ampgain[set_mode].in2_gain ,
												  g_ampgain[set_mode].hp_att   , 
												  g_ampgain[set_mode].hp_gainup,
												  g_ampgain[set_mode].sp_att   , 
												  g_ampgain[set_mode].sp_gainup,
												  g_ampgain[set_mode].bSpNg_DetectionLv,
												  g_ampgain[set_mode].bSpNg_AttackTime,
												  g_ampgain[set_mode].bSpNcpl_NonClipRatio,
												  g_ampgain[set_mode].bSpNcpl_PowerLimit,
												  g_ampgain[set_mode].bSpNcpl_AttackTime,
												  g_ampgain[set_mode].bSpNcpl_ReleaseTime);
}
#else
static ssize_t gain_all_show(struct device *dev,struct device_attribute *attr,char *buf)
{
    return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d \n",
											  set_mode,
											  temp.in1_gain ,
                                              temp.in2_gain ,
                                              temp.hp_att   , 
                                              temp.hp_gainup,
                                              temp.sp_att   , 
                                              temp.sp_gainup);
}

#endif

static ssize_t save_store(struct device *dev,struct device_attribute *attr,
		const char *buf,size_t count)
{
    int reg = simple_strtoul(buf, NULL, 10);
	if(reg == 1)
	{
		memcpy(&g_ampgain[set_mode], &temp, sizeof(struct snd_set_ampgain));
	}
		
	return count;
}

static DEVICE_ATTR(mode		, S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP, mode_show, 		mode_store);
static DEVICE_ATTR(in1_gain	, S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP, in1_gain_show, 	in1_gain_store);
static DEVICE_ATTR(in2_gain	, S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP, in2_gain_show, 	in2_gain_store);
static DEVICE_ATTR(hp_att	, S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP, hp_att_show, 	hp_att_store);
static DEVICE_ATTR(hp_gainup, S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP, hp_gainup_show, 	hp_gainup_store);
static DEVICE_ATTR(sp_att	, S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP, sp_att_show, 	sp_att_store);
static DEVICE_ATTR(sp_gainup, S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP, sp_gainup_show, 	sp_gainup_store);
#if defined (CONFIG_USA_MODEL_SGH_T989)  || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_I717) \
    || defined(CONFIG_USA_MODEL_SGH_I757)|| defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577)
static DEVICE_ATTR(sp_Ng_DetectionLv	, S_IRUGO|(S_IWUSR|S_IWGRP), sp_Ng_DetectionLv_show, 	sp_Ng_DetectionLv_store);
static DEVICE_ATTR(sp_SpNg_AttackTime	, S_IRUGO|(S_IWUSR|S_IWGRP), sp_SpNg_AttackTime_show, 	sp_SpNg_AttackTime_store);
static DEVICE_ATTR(sp_SpNcpl_NonClipRatio	, S_IRUGO|(S_IWUSR|S_IWGRP), sp_SpNcpl_NonClipRatio_show, 	sp_SpNcpl_NonClipRatio_store);
static DEVICE_ATTR(sp_SpNcpl_PowerLimit	, S_IRUGO|(S_IWUSR|S_IWGRP), sp_SpNcpl_PowerLimit_show, 	sp_SpNcpl_PowerLimit_store);
static DEVICE_ATTR(sp_SpNcpl_AttackTime	, S_IRUGO|(S_IWUSR|S_IWGRP), sp_SpNcpl_AttackTime_show, 	sp_SpNcpl_AttackTime_store);
static DEVICE_ATTR(sp_SpNcpl_ReleaseTime	, S_IRUGO|(S_IWUSR|S_IWGRP), sp_SpNcpl_ReleaseTime_show, 	sp_SpNcpl_ReleaseTime_store);
#endif
static DEVICE_ATTR(gain_all	, S_IRUGO, 				   gain_all_show, 	NULL);
static DEVICE_ATTR(save		, S_IWUSR|S_IWGRP, 				   NULL, 			save_store);

static struct attribute *yda165_attributes[] = {
    &dev_attr_mode.attr,
    &dev_attr_in1_gain.attr,
    &dev_attr_in2_gain.attr,
    &dev_attr_hp_att.attr,
    &dev_attr_hp_gainup.attr,
    &dev_attr_sp_att.attr,
    &dev_attr_sp_gainup.attr,
    &dev_attr_gain_all.attr,
    &dev_attr_save.attr,
#if defined (CONFIG_USA_MODEL_SGH_T989)  || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_I717) \
    || defined(CONFIG_USA_MODEL_SGH_I757)|| defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577)
    &dev_attr_sp_Ng_DetectionLv.attr,
    &dev_attr_sp_SpNg_AttackTime.attr,
    &dev_attr_sp_SpNcpl_NonClipRatio.attr,
    &dev_attr_sp_SpNcpl_PowerLimit.attr,
    &dev_attr_sp_SpNcpl_AttackTime.attr,
    &dev_attr_sp_SpNcpl_ReleaseTime.attr,
#endif    
    NULL
};

static struct attribute_group yda165_attribute_group = {
    .attrs = yda165_attributes
};

static int load_ampgain(void)
{
	struct yda165_i2c_data *yd ;
	int numofmodes;
	int index=0;
	yd = &g_data;
	numofmodes = yd->num_modes;
	for(index=0; index<numofmodes; index++)
	{
		memcpy(&g_ampgain[index], &yd->ampgain[index], sizeof(struct snd_set_ampgain));
	}

#if defined(CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)
	pr_info(MODULE_NAME ":Override hp_gainup for Rev0.3A and afterwards (%d) \n", get_hw_rev() );
	if(get_hw_rev()>=0x9) //Rev0.3, Rev0.3A
	{
		g_ampgain[1].hp_gainup = 0; //HEADSET
		g_ampgain[2].hp_gainup = 2; //SPK+HEADSET		
		g_ampgain[1].hp_att = 29; 
		g_ampgain[4].hp_gainup = 2; //headset_call
		g_ampgain[4].hp_att = 25;
	}
	else
	{
		g_ampgain[1].hp_gainup = 0; //HEADSET
		g_ampgain[2].hp_gainup = 0; //SPK+HEADSET		
		g_ampgain[1].hp_att = 29;
		g_ampgain[4].hp_gainup = 0; //headset_call
		g_ampgain[4].hp_att = 25;
	}	
	g_ampgain[3].sp_gainup = 0;
	g_ampgain[3].bSpNg_DetectionLv = 0;

		g_ampgain[0].bSpNg_DetectionLv = 0;
		g_ampgain[0].bSpNg_AttackTime = 1;
		g_ampgain[0].bSpNcpl_NonClipRatio = 0;
		g_ampgain[0].bSpNcpl_PowerLimit = 0;
		g_ampgain[0].bSpNcpl_AttackTime = 1;
		g_ampgain[0].bSpNcpl_ReleaseTime = 1;
	
		g_ampgain[1].bSpNg_DetectionLv = 0; /* SP Noise Gate : detection level */
		g_ampgain[1].bSpNg_AttackTime = 1;		/* SP Noise Gate : attack time */
		g_ampgain[1].bSpNcpl_NonClipRatio = 0;	/* SP Non-Clip power limiter : Non-Clip distortion ratio */
		g_ampgain[1].bSpNcpl_PowerLimit = 0;	/* SP Non-Clip power limiter : Power Limit */
		g_ampgain[1].bSpNcpl_AttackTime = 1;	/* SP Non-Clip power limiter : attack Time */
		g_ampgain[1].bSpNcpl_ReleaseTime = 1;	/* SP Non-Clip power limiter : release Time */
	
		g_ampgain[2].bSpNg_DetectionLv = 0; /* SP Noise Gate : detection level */
		g_ampgain[2].bSpNg_AttackTime = 1;		/* SP Noise Gate : attack time */
		g_ampgain[2].bSpNcpl_NonClipRatio = 0;	/* SP Non-Clip power limiter : Non-Clip distortion ratio */
		g_ampgain[2].bSpNcpl_PowerLimit = 0;	/* SP Non-Clip power limiter : Power Limit */
		g_ampgain[2].bSpNcpl_AttackTime = 1;	/* SP Non-Clip power limiter : attack Time */
		g_ampgain[2].bSpNcpl_ReleaseTime = 1;	/* SP Non-Clip power limiter : release Time */

		g_ampgain[3].bSpNg_AttackTime = 1;		/* SP Noise Gate : attack time */
		g_ampgain[3].bSpNcpl_NonClipRatio = 1;	/* SP Non-Clip power limiter : Non-Clip distortion ratio */
		g_ampgain[3].bSpNcpl_PowerLimit = 0;	/* SP Non-Clip power limiter : Power Limit */
		g_ampgain[3].bSpNcpl_AttackTime = 1;	/* SP Non-Clip power limiter : attack Time */
		g_ampgain[3].bSpNcpl_ReleaseTime = 1;	/* SP Non-Clip power limiter : release Time */

		g_ampgain[4].bSpNg_DetectionLv = 0; /* SP Noise Gate : detection level */
		g_ampgain[4].bSpNg_AttackTime = 1;		/* SP Noise Gate : attack time */
		g_ampgain[4].bSpNcpl_NonClipRatio = 1;	/* SP Non-Clip power limiter : Non-Clip distortion ratio */
		g_ampgain[4].bSpNcpl_PowerLimit = 0;	/* SP Non-Clip power limiter : Power Limit */
		g_ampgain[4].bSpNcpl_AttackTime = 1;	/* SP Non-Clip power limiter : attack Time */
		g_ampgain[4].bSpNcpl_ReleaseTime = 1;	/* SP Non-Clip power limiter : release Time */
	
#elif defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_USA_MODEL_SGH_I727R)


#if defined(CONFIG_USA_MODEL_SGH_I727R)
		g_ampgain[2].sp_gainup = 0; /* speaker_headset */
		g_ampgain[2].sp_att = 22;
#endif


#if defined(CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_EUR_MODEL_GT_I9210)
	g_ampgain[3].bSpNg_DetectionLv = 0;
#else
	g_ampgain[3].bSpNg_DetectionLv = 4;
#endif

#if defined(CONFIG_EUR_MODEL_GT_I9210)
		g_ampgain[0].bSpNg_DetectionLv = 0;
		g_ampgain[0].bSpNg_AttackTime = 0;
		g_ampgain[0].bSpNcpl_NonClipRatio = 0;
		g_ampgain[0].bSpNcpl_PowerLimit = 0;
		g_ampgain[0].bSpNcpl_AttackTime = 1;
		g_ampgain[0].bSpNcpl_ReleaseTime = 1;

		g_ampgain[1].bSpNg_DetectionLv = 2; /* SP Noise Gate : detection level */
		g_ampgain[1].bSpNg_AttackTime = 1;		/* SP Noise Gate : attack time */
		g_ampgain[1].bSpNcpl_NonClipRatio = 1;	/* SP Non-Clip power limiter : Non-Clip distortion ratio */
		g_ampgain[1].bSpNcpl_PowerLimit = 0;	/* SP Non-Clip power limiter : Power Limit */
		g_ampgain[1].bSpNcpl_AttackTime = 1;	/* SP Non-Clip power limiter : attack Time */
		g_ampgain[1].bSpNcpl_ReleaseTime = 1;	/* SP Non-Clip power limiter : release Time */

		g_ampgain[2].bSpNg_DetectionLv = 0; /* SP Noise Gate : detection level */
		g_ampgain[2].bSpNg_AttackTime = 1;		/* SP Noise Gate : attack time */
		g_ampgain[2].bSpNcpl_NonClipRatio = 6;	/* SP Non-Clip power limiter : Non-Clip distortion ratio */
		g_ampgain[2].bSpNcpl_PowerLimit = 0;	/* SP Non-Clip power limiter : Power Limit */
		g_ampgain[2].bSpNcpl_AttackTime = 1;	/* SP Non-Clip power limiter : attack Time */
		g_ampgain[2].bSpNcpl_ReleaseTime = 1;	/* SP Non-Clip power limiter : release Time */

		g_ampgain[3].bSpNg_AttackTime = 1;		/* SP Noise Gate : attack time */
		g_ampgain[3].bSpNcpl_NonClipRatio = 1;	/* SP Non-Clip power limiter : Non-Clip distortion ratio */
		g_ampgain[3].bSpNcpl_PowerLimit = 0;	/* SP Non-Clip power limiter : Power Limit */
		g_ampgain[3].bSpNcpl_AttackTime = 1;	/* SP Non-Clip power limiter : attack Time */
		g_ampgain[3].bSpNcpl_ReleaseTime = 1;	/* SP Non-Clip power limiter : release Time */

		g_ampgain[4].bSpNg_DetectionLv = 0; /* SP Noise Gate : detection level */
		g_ampgain[4].bSpNg_AttackTime = 1;		/* SP Noise Gate : attack time */
		g_ampgain[4].bSpNcpl_NonClipRatio = 0;	/* SP Non-Clip power limiter : Non-Clip distortion ratio */
		g_ampgain[4].bSpNcpl_PowerLimit = 0;	/* SP Non-Clip power limiter : Power Limit */
		g_ampgain[4].bSpNcpl_AttackTime = 1;	/* SP Non-Clip power limiter : attack Time */
		g_ampgain[4].bSpNcpl_ReleaseTime = 1;	/* SP Non-Clip power limiter : release Time */
#else
		g_ampgain[0].bSpNg_DetectionLv = 0;
		g_ampgain[0].bSpNg_AttackTime = 1;
		g_ampgain[0].bSpNcpl_NonClipRatio = 0;
		g_ampgain[0].bSpNcpl_PowerLimit = 0;
		g_ampgain[0].bSpNcpl_AttackTime = 1;
		g_ampgain[0].bSpNcpl_ReleaseTime = 1;
	
		g_ampgain[1].bSpNg_DetectionLv = 0; /* SP Noise Gate : detection level */
		g_ampgain[1].bSpNg_AttackTime = 1;		/* SP Noise Gate : attack time */
		g_ampgain[1].bSpNcpl_NonClipRatio = 1;	/* SP Non-Clip power limiter : Non-Clip distortion ratio */
		g_ampgain[1].bSpNcpl_PowerLimit = 0;	/* SP Non-Clip power limiter : Power Limit */
		g_ampgain[1].bSpNcpl_AttackTime = 1;	/* SP Non-Clip power limiter : attack Time */
		g_ampgain[1].bSpNcpl_ReleaseTime = 1;	/* SP Non-Clip power limiter : release Time */
	
		g_ampgain[2].bSpNg_DetectionLv = 0; /* SP Noise Gate : detection level */
		g_ampgain[2].bSpNg_AttackTime = 1;		/* SP Noise Gate : attack time */
		g_ampgain[2].bSpNcpl_NonClipRatio = 0;	/* SP Non-Clip power limiter : Non-Clip distortion ratio */
		g_ampgain[2].bSpNcpl_PowerLimit = 0;	/* SP Non-Clip power limiter : Power Limit */
		g_ampgain[2].bSpNcpl_AttackTime = 1;	/* SP Non-Clip power limiter : attack Time */
		g_ampgain[2].bSpNcpl_ReleaseTime = 1;	/* SP Non-Clip power limiter : release Time */

		g_ampgain[3].bSpNg_AttackTime = 1;		/* SP Noise Gate : attack time */
		g_ampgain[3].bSpNcpl_NonClipRatio = 0;	/* SP Non-Clip power limiter : Non-Clip distortion ratio */
		g_ampgain[3].bSpNcpl_PowerLimit = 0;	/* SP Non-Clip power limiter : Power Limit */
		g_ampgain[3].bSpNcpl_AttackTime = 1;	/* SP Non-Clip power limiter : attack Time */
		g_ampgain[3].bSpNcpl_ReleaseTime = 1;	/* SP Non-Clip power limiter : release Time */

		g_ampgain[4].bSpNg_DetectionLv = 0; /* SP Noise Gate : detection level */
		g_ampgain[4].bSpNg_AttackTime = 1;		/* SP Noise Gate : attack time */
		g_ampgain[4].bSpNcpl_NonClipRatio = 1;	/* SP Non-Clip power limiter : Non-Clip distortion ratio */
		g_ampgain[4].bSpNcpl_PowerLimit = 0;	/* SP Non-Clip power limiter : Power Limit */
		g_ampgain[4].bSpNcpl_AttackTime = 1;	/* SP Non-Clip power limiter : attack Time */
		g_ampgain[4].bSpNcpl_ReleaseTime = 1;	/* SP Non-Clip power limiter : release Time */
#endif
#endif


	memcpy(&temp, &g_ampgain[0], sizeof(struct snd_set_ampgain));

#if AMPREG_DEBUG
	for(index=0; index<numofmodes; index++)
	{
		pr_info(MODULE_NAME ":[%d].in1_gain = %d \n",index,g_ampgain[index].in1_gain  );
		pr_info(MODULE_NAME ":[%d].in2_gain = %d \n",index,g_ampgain[index].in2_gain  );
		pr_info(MODULE_NAME ":[%d].hp_att   = %d \n",index,g_ampgain[index].hp_att    );
		pr_info(MODULE_NAME ":[%d].hp_gainup= %d \n",index,g_ampgain[index].hp_gainup );
		pr_info(MODULE_NAME ":[%d].sp_att   = %d \n",index,g_ampgain[index].sp_att    );
		pr_info(MODULE_NAME ":[%d].sp_gainup= %d \n",index,g_ampgain[index].sp_gainup );
	}

	pr_info(MODULE_NAME ":temp.in1_gain = %d \n",temp.in1_gain  );
	pr_info(MODULE_NAME ":temp.in2_gain = %d \n",temp.in2_gain  );
	pr_info(MODULE_NAME ":temp.hp_att   = %d \n",temp.hp_att    );
	pr_info(MODULE_NAME ":temp.hp_gainup= %d \n",temp.hp_gainup );
	pr_info(MODULE_NAME ":temp.sp_att   = %d \n",temp.sp_att    );
	pr_info(MODULE_NAME ":temp.sp_gainup= %d \n",temp.sp_gainup );
#endif

	pr_info(MODULE_NAME ":%s completed\n",__func__);
	return 0;
}

/*******************************************************************************
 *	d4Write
 *
 *	Function:
 *			write register parameter function
 *	Argument:
 *			UINT8 bWriteRA  : register address
 *			UINT8 bWritePrm : register parameter
 *
 *	Return:
 *			SINT32	>= 0 success
 *					<  0 error
 *
 ******************************************************************************/
SINT32 d4Write(UINT8 bWriteRA, UINT8 bWritePrm)
{
	/* Write 1 byte data to the register for each system. */

	int rc = 0;
	
	if(pclient == NULL)
	{
		pr_err(MODULE_NAME ": i2c_client error\n");
		return -1;
	}

	rc = i2c_smbus_write_byte_data(pclient, bWriteRA, bWritePrm); 
	if(rc)
	{
		pr_err(MODULE_NAME ": i2c write error %d\n", rc);
		return rc;
	}

	return 0;
}

/*******************************************************************************
 *	d4WriteN
 *
 *	Function:
 *			write register parameter function
 *	Argument:
 *			UINT8 bWriteRA    : register address
 *			UINT8 *pbWritePrm : register parameter
 *			UINT8 bWriteSize  : size of "*pbWritePrm"
 *
 *	Return:
 *			SINT32	>= 0 success
 *					<  0 error
 *
 ******************************************************************************/
SINT32 d4WriteN(UINT8 bWriteRA, UINT8 *pbWritePrm, UINT8 bWriteSize)
{
	/* Write N byte data to the register for each system. */
	
	int rc = 0;
	
	if(pclient == NULL)
	{
		pr_err(MODULE_NAME ": i2c_client error\n");
		return -1;
	}

	rc = i2c_smbus_write_block_data(pclient, bWriteRA, bWriteSize, pbWritePrm); 
	if(rc)
	{
		pr_err(MODULE_NAME ": i2c write error %d\n", rc);
		return rc;
	}

	return 0;
	
}

/*******************************************************************************
 *	d4Read
 *
 *	Function:
 *			read register parameter function
 *	Argument:
 *			UINT8 bReadRA    : register address
 *			UINT8 *pbReadPrm : register parameter
 *
 *	Return:
 *			SINT32	>= 0 success
 *					<  0 error
 *
 ******************************************************************************/
SINT32 d4Read(UINT8 bReadRA, UINT8 *pbReadPrm)
{
	/* Read byte data to the register for each system. */
	
	int buf;
	
	if(pclient == NULL)
	{
		pr_err(MODULE_NAME ": i2c_client error\n");
		return -1;
	}

	buf  = i2c_smbus_read_byte_data(pclient, bReadRA);	
	if(buf < 0)
	{
		pr_err(MODULE_NAME ": i2c read error %d\n", buf);
		return buf;
	}
	
	*pbReadPrm = (UINT8)buf;

	return 0;
}

/*******************************************************************************
 *	d4Wait
 *
 *	Function:
 *			wait function
 *	Argument:
 *			UINT32 dTime : wait time [ micro second ]
 *
 *	Return:
 *			none
 *
 ******************************************************************************/
void d4Wait(UINT32 dTime)
{
	/* Wait procedure for each system */
	
	udelay(dTime);
}

/*******************************************************************************
 *	d4Sleep
 *
 *	Function:
 *			sleep function
 *	Argument:
 *			UINT32 dTime : sleep time [ milli second ]
 *
 *	Return:
 *			none
 *
 ******************************************************************************/
void d4Sleep(UINT32 dTime)
{
	/* Sleep procedure for each system */

	msleep(dTime);
}

/*******************************************************************************
 *	d4ErrHandler
 *
 *	Function:
 *			error handler function
 *	Argument:
 *			SINT32 dError : error code
 *
 *	Return:
 *			none
 *
 ******************************************************************************/
void d4ErrHandler(SINT32 dError)
{
	/* Error procedure for each system */

	pr_err(MODULE_NAME ": %s error %ld\n", __func__, dError);
}

/* D-4HP3 register map */
UINT8 g_bD4Hp3RegisterMap[9] = 
{	
	0x80,										/* 0x80 */
	0x04, 0x06, 0x40, 0x22, 0x40, 0x40, 0x00,	/* 0x81 - 0x87 */
	0x00										/* 0x88 */
};

/*******************************************************************************
 *	D4Hp3_UpdateRegisterMap
 *
 *	Function:
 *			update register map (g_bD4Hp3RegisterMap[]) function
 *	Argument:
 *			SINT32	sdRetVal	: update flag
 *			UINT8	bRN			: register number (0 - 8)
 *			UINT8	bPrm		: register parameter
 *
 *	Return:
 *			none
 *
 ******************************************************************************/
static void D4Hp3_UpdateRegisterMap( SINT32 sdRetVal, UINT8 bRN, UINT8 bPrm )
{
	if(sdRetVal < 0)
	{
		d4ErrHandler( D4HP3_ERROR );
	}
	else
	{
		/* update register map */
		g_bD4Hp3RegisterMap[ bRN ] = bPrm;
	}
}

/*******************************************************************************
 *	D4Hp3_UpdateRegisterMapN
 *
 *	Function:
 *			update register map (g_bD4Hp3RegisterMap[]) function
 *	Argument:
 *			SINT32	sdRetVal	: update flag
 *			UINT8	bRN			: register number(0 - 8)
 *			UINT8	*pbPrm		: register parameter
 *			UINT8	bPrmSize	: size of " *pbPrm"
 *
 *	Return:
 *			none
 *
 ******************************************************************************/
static void D4Hp3_UpdateRegisterMapN( SINT32 sdRetVal, UINT8 bRN, UINT8 *pbPrm, UINT8 bPrmSize )
{
	UINT8 bCnt = 0;

	if(sdRetVal < 0)
	{
		d4ErrHandler( D4HP3_ERROR );
	}
	else
	{
		/* update register map */
		for(bCnt = 0; bCnt < bPrmSize; bCnt++)
		{
			g_bD4Hp3RegisterMap[ bRN + bCnt ] = pbPrm[ bCnt ];
		}
	}
}

/*******************************************************************************
 *	D4Hp3_WriteRegisterBit
 *
 *	Function:
 *			write register "bit" function
 *	Argument:
 *			UINT32	dName	: register name
 *			UINT8	bPrm	: register parameter
 *
 *	Return:
 *			none
 *
 ******************************************************************************/
void D4Hp3_WriteRegisterBit( UINT32 dName, UINT8 bPrm )
{
	UINT8 bWritePrm;			/* I2C sending parameter */
	UINT8 bDummy;				/* setting parameter */
	UINT8 bRA, bRN, bMB, bSB;	/* register address, register number, mask bit, shift bit */

	/* 
	dName
	bit 31 - 16 : register address
	bit 15 -  8	: mask bit
	bit  7 -  0	: shift bit
	*/
	bRA = (UINT8)(( dName & 0xFF0000 ) >> 16);
	bRN = bRA - 0x80;
	bMB = (UINT8)(( dName & 0x00FF00 ) >> 8);
	bSB = (UINT8)( dName & 0x0000FF );

	/* check arguments */
	if((bRA < D4HP3_MIN_REGISTERADDRESS) && (D4HP3_MAX_WRITE_REGISTERADDRESS < bRA))
	{
		d4ErrHandler( D4HP3_ERROR_ARGUMENT );
	}
	/* set register parameter */
	bPrm = (bPrm << bSB) & bMB;
	bDummy = bMB ^ 0xFF;
	bWritePrm = g_bD4Hp3RegisterMap[ bRN ] & bDummy;	/* set bit of writing position to 0 */
	bWritePrm = bWritePrm | bPrm;						/* set parameter of writing bit */
	/* call the user implementation function "d4Write()", and write register */
	D4Hp3_UpdateRegisterMap( d4Write( bRA, bWritePrm ), bRN, bWritePrm );
}

/*******************************************************************************
 *	D4Hp3_WriteRegisterByte
 *
 *	Function:
 *			write register "byte" function
 *	Argument:
 *			UINT8 bAddress  : register address
 *			UINT8 bPrm : register parameter
 *
 *	Return:
 *			none
 *
 ******************************************************************************/
void D4Hp3_WriteRegisterByte(UINT8 bAddress, UINT8 bPrm)
{
	UINT8 bNumber;
	/* check arguments */
	if((bAddress < D4HP3_MIN_REGISTERADDRESS) && (D4HP3_MAX_WRITE_REGISTERADDRESS < bAddress))
	{
		d4ErrHandler( D4HP3_ERROR_ARGUMENT );
	}
	bNumber = bAddress - 0x80;
	D4Hp3_UpdateRegisterMap( d4Write( bAddress, bPrm ), bNumber, bPrm );
}

/*******************************************************************************
 *	D4Hp3_WriteRegisterByteN
 *
 *	Function:
 *			write register "n byte" function
 *	Argument:
 *			UINT8 bAddress	: register address
 *			UINT8 *pbPrm	: register parameter
 *			UINT8 bPrmSize	: size of "*pbPrm"
 *
 *	Return:
 *			none
 *
 ******************************************************************************/
void D4Hp3_WriteRegisterByteN(UINT8 bAddress, UINT8 *pbPrm, UINT8 bPrmSize)
{
	UINT8 bNumber;
	/* check arguments */
	if((bAddress < D4HP3_MIN_REGISTERADDRESS) && (D4HP3_MAX_WRITE_REGISTERADDRESS < bAddress))
	{
		d4ErrHandler( D4HP3_ERROR_ARGUMENT );
	}
	if( bPrmSize > ((D4HP3_MAX_WRITE_REGISTERADDRESS - D4HP3_MIN_REGISTERADDRESS) + 1))
	{
		d4ErrHandler( D4HP3_ERROR_ARGUMENT );
	}
	bNumber = bAddress - 0x80;
	D4Hp3_UpdateRegisterMapN( d4WriteN( bAddress, pbPrm, bPrmSize ), bNumber, pbPrm, bPrmSize);
}

/*******************************************************************************
 *	D4Hp3_ReadRegisterBit
 *
 *	Function:
 *			read register "bit" function
 *	Argument:
 *			UINT32 dName	: register name
 *			UINT8  *pbPrm	: register parameter
 *
 *	Return:
 *			none
 *
 ******************************************************************************/
void D4Hp3_ReadRegisterBit( UINT32 dName, UINT8 *pbPrm)
{
	SINT32 sdRetVal = D4HP3_SUCCESS;
	UINT8 bRA, bRN, bMB, bSB;	/* register address, register number, mask bit, shift bit */

	/* 
	dName
	bit 31 - 16	: register address
	bit 15 -  8	: mask bit
	bit  7 -  0	: shift bit
	*/
	bRA = (UINT8)(( dName & 0xFF0000 ) >> 16);
	bRN = bRA - 0x80;
	bMB = (UINT8)(( dName & 0x00FF00 ) >> 8);
	bSB = (UINT8)( dName & 0x0000FF );

	/* check arguments */
	if((bRA < D4HP3_MIN_REGISTERADDRESS) && (D4HP3_MAX_READ_REGISTERADDRESS < bRA))
	{
		d4ErrHandler( D4HP3_ERROR_ARGUMENT );
	}
	/* call the user implementation function "d4Read()", and read register */
	sdRetVal = d4Read( bRA, pbPrm );
	D4Hp3_UpdateRegisterMap( sdRetVal, bRN, *pbPrm );
	/* extract the parameter of selected register in the read register parameter */
	*pbPrm = ((g_bD4Hp3RegisterMap[ bRN ] & bMB) >> bSB);
}

/*******************************************************************************
 *	D4Hp3_ReadRegisterByte
 *
 *	Function:
 *			read register "byte" function
 *	Argument:
 *			UINT8 bAddress	: register address
 *			UINT8 *pbPrm	: register parameter
 *
 *	Return:
 *			none
 *
 ******************************************************************************/
void D4Hp3_ReadRegisterByte( UINT8 bAddress, UINT8 *pbPrm)
{
	SINT32 sdRetVal = D4HP3_SUCCESS;
	UINT8 bNumber;
	/* check arguments */
	if((bAddress < D4HP3_MIN_REGISTERADDRESS) && (D4HP3_MAX_READ_REGISTERADDRESS < bAddress))
	{
		d4ErrHandler( D4HP3_ERROR_ARGUMENT );
	}
	/* call the user implementation function "d4Read()", and read register */
	bNumber = bAddress - 0x80;
	sdRetVal = d4Read( bAddress, pbPrm );
	D4Hp3_UpdateRegisterMap( sdRetVal, bNumber, *pbPrm );
}


/*******************************************************************************
 *	D4Hp3_CheckArgument_Mixer
 *
 *	Function:
 *			check D-4HP3 setting information for mixer
 *	Argument:
 *			UINT8 bHpFlag : "change HP amp mixer setting" flag(0 : no check, 1 : check)
 *			UINT8 bSpFlag : "change SP amp mixer setting" flag(0 : no check, 1 : check)
 *			D4HP3_SETTING_INFO *pstSettingInfo : D-4HP3 setting information
 *
 *	Return:
 *			none
 *
 ******************************************************************************/
void D4Hp3_CheckArgument_Mixer(UINT8 bHpFlag, UINT8 bSpFlag, D4HP3_SETTING_INFO *pstSettingInfo)
{
	UINT8 bCheckArgument = 0;

	/* HP */
	if(bHpFlag == 1)
	{
		if(pstSettingInfo->bHpCh > 1)
		{
			pstSettingInfo->bHpCh = 0;
			bCheckArgument++;
		}
		if(pstSettingInfo->bHpMixer_Line1 > 1)
		{
			pstSettingInfo->bHpMixer_Line1 = 0;
			bCheckArgument++;
		}
		if(pstSettingInfo->bHpMixer_Line2 > 1)
		{
			pstSettingInfo->bHpMixer_Line2 = 0;
			bCheckArgument++;
		}
	}

	/* SP */
	if(bSpFlag == 1)
	{
		if(pstSettingInfo->bSpMixer_Line1 > 1)
		{
			pstSettingInfo->bSpMixer_Line1 = 0;
			bCheckArgument++;
		}
		if(pstSettingInfo->bSpMixer_Line2 > 1)
		{
			pstSettingInfo->bSpMixer_Line2 = 0;
			bCheckArgument++;
		}
	}
	
	/* check argument */
	if(bCheckArgument > 0)
	{
		d4ErrHandler( D4HP3_ERROR_ARGUMENT );
	}
}

/*******************************************************************************
 *	D4Hp3_CheckArgument
 *
 *	Function:
 *			check D-4HP3 setting information
 *	Argument:
 *			D4HP3_SETTING_INFO *pstSettingInfo : D-4HP3 setting information
 *
 *	Return:
 *			none
 *
 ******************************************************************************/
void D4Hp3_CheckArgument(D4HP3_SETTING_INFO *pstSettingInfo)
{
	UINT8 bCheckArgument = 0;

	/* IN */
	if(pstSettingInfo->bLine1Gain > 7)
	{
		pstSettingInfo->bLine1Gain = 2;
		bCheckArgument++;
	}
	if(pstSettingInfo->bLine2Gain > 7)
	{
		pstSettingInfo->bLine2Gain = 2;
		bCheckArgument++;
	}
	if(pstSettingInfo->bLine1Balance > 1)
	{
		pstSettingInfo->bLine1Balance = 0;
		bCheckArgument++;
	}
	if(pstSettingInfo->bLine2Balance > 1)	
	{
		pstSettingInfo->bLine2Balance = 0;
		bCheckArgument++;
	}
	/* HP */
	if(pstSettingInfo->bHpCpMode > 1)
	{
		pstSettingInfo->bHpCpMode = 0;
		bCheckArgument++;
	}
	if(pstSettingInfo->bHpAvddLev > 1)
	{
		pstSettingInfo->bHpAvddLev = 0;
		bCheckArgument++;
	}
	if(pstSettingInfo->bHpEco > 1)
	{
		pstSettingInfo->bHpEco = 0;
		bCheckArgument++;
	}
	if(pstSettingInfo->bHpAtt > 31)
	{
		pstSettingInfo->bHpAtt = 0;
		bCheckArgument++;
	}
	if(pstSettingInfo->bHpGainUp > 3)
	{
		pstSettingInfo->bHpGainUp = 0;
		bCheckArgument++;
	}
	if(pstSettingInfo->bHpSvol > 1)
	{
		pstSettingInfo->bHpSvol = 0;
		bCheckArgument++;
	}
	if(pstSettingInfo->bHpZcs > 1)
	{
		pstSettingInfo->bHpZcs = 1;
		bCheckArgument++;
	}
	if(pstSettingInfo->bHpCh > 1)
	{
		pstSettingInfo->bHpCh = 0;
		bCheckArgument++;
	}
	if(pstSettingInfo->bHpMixer_Line1 > 1)
	{
		pstSettingInfo->bHpMixer_Line1 = 0;
		bCheckArgument++;
	}
	if(pstSettingInfo->bHpMixer_Line2 > 1)
	{
		pstSettingInfo->bHpMixer_Line2 = 0;
		bCheckArgument++;
	}

	/* SP */
	if(pstSettingInfo->bSpAtt > 31)
	{
		pstSettingInfo->bSpAtt = 0;
		bCheckArgument++;
	}
	if(pstSettingInfo->bSpGainUp > 3)
	{
		pstSettingInfo->bSpGainUp = 0;
		bCheckArgument++;
	}
	if(pstSettingInfo->bSpSvol > 1)
	{
		pstSettingInfo->bSpSvol = 0;
		bCheckArgument++;
	}
	if(pstSettingInfo->bSpZcs > 1)
	{
		pstSettingInfo->bSpZcs = 1;
		bCheckArgument++;
	}
	if(pstSettingInfo->bSpMixer_Line1 > 1)
	{
		pstSettingInfo->bSpMixer_Line1 = 0;
		bCheckArgument++;
	}
	if(pstSettingInfo->bSpMixer_Line2 > 1)
	{
		pstSettingInfo->bSpMixer_Line2 = 0;
		bCheckArgument++;
	}

	if(pstSettingInfo->bSpNg_DetectionLv > 7)
	{
		pstSettingInfo->bSpNg_DetectionLv = 4;
		bCheckArgument++;
	}
	if(pstSettingInfo->bSpNg_AttackTime > 3)
	{
		pstSettingInfo->bSpNg_AttackTime = 3;
		bCheckArgument++;
	}
	if(pstSettingInfo->bSpNcpl_NonClipRatio > 7)
	{
		pstSettingInfo->bSpNcpl_NonClipRatio = 0;
		bCheckArgument++;
	}
	if(pstSettingInfo->bSpNcpl_PowerLimit > 15)
	{
		pstSettingInfo->bSpNcpl_PowerLimit =0;
		bCheckArgument++;
	}
	if((pstSettingInfo->bSpNcpl_AttackTime == 0) || (pstSettingInfo->bSpNcpl_AttackTime > 3))
	{
		pstSettingInfo->bSpNcpl_AttackTime = 1;
		bCheckArgument++;
	}
	if((pstSettingInfo->bSpNcpl_ReleaseTime == 0) || (pstSettingInfo->bSpNcpl_ReleaseTime > 1))
	{
		pstSettingInfo->bSpNcpl_ReleaseTime = 1;
		bCheckArgument++;
	}

	/* check argument */
	if(bCheckArgument > 0)
	{
		d4ErrHandler( D4HP3_ERROR_ARGUMENT );
	}
}


/*******************************************************************************
 *	D4Hp3_ControlMixer
 *
 *	Function:
 *			control HP amp mixer and SP amp mixer in D-4HP3
 *	Argument:
 *			UINT8 bHpFlag : "change HP amp mixer setting" flag(0 : no change, 1 : change)
 *			UINT8 bSpFlag : "change SP amp mixer setting" flag(0 : no change, 1 : change)
 *			D4HP3_SETTING_INFO *pstSetMixer : D-4HP3 setting information
 *
 *	Return:
 *			none
 *
 ******************************************************************************/
void D4Hp3_ControlMixer(UINT8 bHpFlag, UINT8 bSpFlag, D4HP3_SETTING_INFO *pstSetMixer)
{
	UINT8 bWriteRA, bWritePrm;
	UINT8 bTempHpCh, bTempHpMixer_Line1, bTempHpMixer_Line2;
	UINT8 bTempSpMixer_Line1, bTempSpMixer_Line2;

	/* check argument */
	if( bHpFlag > 1 )
	{
		bHpFlag = 0;
	}
	if( bSpFlag > 1 )
	{
		bSpFlag = 0;
	}
	D4Hp3_CheckArgument_Mixer( bHpFlag, bSpFlag, pstSetMixer );

	/* change mixer sequence */
	/* SP */
	if(bSpFlag == 1)
	{
		bTempSpMixer_Line1 = 0;
		bTempSpMixer_Line2 = 0;
	}
	else
	{
		bTempSpMixer_Line1 = (g_bD4Hp3RegisterMap[7] & 0x20) >> (D4HP3_SP_AMIX & 0xFF);
		bTempSpMixer_Line2 = (g_bD4Hp3RegisterMap[7] & 0x10) >> (D4HP3_SP_BMIX & 0xFF);
	}

	/* HP */
	bTempHpCh = (g_bD4Hp3RegisterMap[7] & 0x08) >> (D4HP3_HP_MONO & 0xFF);
	if(bHpFlag == 1)
	{
		bTempHpMixer_Line1 = 0;
		bTempHpMixer_Line2 = 0;
	}
	else
	{
		bTempHpMixer_Line1 = (g_bD4Hp3RegisterMap[7] & 0x02) >> (D4HP3_HP_AMIX & 0xFF);
		bTempHpMixer_Line2 = (g_bD4Hp3RegisterMap[7] & 0x01) >> (D4HP3_HP_BMIX & 0xFF);
	}

	/* write register #0x87 */
	bWriteRA = 0x87;
	bWritePrm = (bTempSpMixer_Line1 << 5) | (bTempSpMixer_Line2 << 4)		/* SP_AMIX, SP_BMIX */
					| (bTempHpCh << 3)										/* HP_MONO */
					| (bTempHpMixer_Line1 << 1) | (bTempHpMixer_Line2);		/* HP_AMIX, HP_BMIX */

	D4Hp3_WriteRegisterByte(bWriteRA, bWritePrm);

	/* set HP amp mixer, SP amp mixer */
	if(bHpFlag == 1)
	{
		bTempHpCh = pstSetMixer->bHpCh;
		bTempHpMixer_Line1 = pstSetMixer->bHpMixer_Line1;
		bTempHpMixer_Line2 = pstSetMixer->bHpMixer_Line2;
	}
	if(bSpFlag == 1)
	{
		bTempSpMixer_Line1 = pstSetMixer->bSpMixer_Line1;
		bTempSpMixer_Line2 = pstSetMixer->bSpMixer_Line2;
	}

	/* write register #0x87 */
	if((bHpFlag == 1) || (bSpFlag == 1))
	{
		bWritePrm = (bTempSpMixer_Line1 << 5) | (bTempSpMixer_Line2 << 4)	/* SP_AMIX, SP_BMIX */
					| (bTempHpCh << 3)										/* HP_MONO */
					| (bTempHpMixer_Line1 << 1) | (bTempHpMixer_Line2);		/* HP_AMIX, HP_BMIX */
		bWriteRA = 0x87;
		D4Hp3_WriteRegisterByte(bWriteRA, bWritePrm);
	}
}

/*******************************************************************************
 *	D4Hp3_PowerOn
 *
 *	Function:
 *			power on D-4HP3
 *	Argument:
 *			D4HP3_SETTING_INFO *pstSettingInfo : D-4HP3 setting information
 *
 *	Return:
 *			none
 *
 ******************************************************************************/
void D4Hp3_PowerOn(D4HP3_SETTING_INFO *pstSettingInfo)
{
	//UINT8 bWriteAddress;
	UINT8 abWritePrm[8];
	UINT8 bTemp;
	/* check argument */
	D4Hp3_CheckArgument( pstSettingInfo );

#if AMPREG_DEBUG
	pr_info(MODULE_NAME ": mixer [LINE1:0x%0x  LINE2:0x%0x]\n", pstSettingInfo->bHpMixer_Line1, pstSettingInfo->bHpMixer_Line2 );
#endif

	/* set parameter */
	//bWriteAddress = 0x80;
	/* 0x80 */
	abWritePrm[0] = (pstSettingInfo->bHpCpMode << (D4HP3_CPMOD & 0xFF))
					| (pstSettingInfo->bHpAvddLev << (D4HP3_VLEVEL & 0xFF));
	/* 0x81 */
	abWritePrm[1] = (pstSettingInfo->bHpEco << (D4HP3_ECO_MODE & 0xFF))
					| (pstSettingInfo->bLine1Balance << (D4HP3_DIFA & 0xFF))
					| (pstSettingInfo->bLine2Balance << (D4HP3_DIFB & 0xFF))

#if defined (CONFIG_KOR_MODEL_SHV_E110S) || defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K) || defined (CONFIG_JPN_MODEL_SC_03D)
					| (0x00 << (D4HP3_HIZ_HP & 0xFF))
#elif defined (HP_HIZ_ON)
					| (0x01 << (D4HP3_HIZ_HP & 0xFF))
#endif
#ifdef SP_HIZ_ON
					| (0x01 << (D4HP3_HIZ_SP & 0xFF))
#endif
					| (pstSettingInfo->bHpGainUp << (D4HP3_HP_GAIN & 0xFF));

	/* 0x82 */
	/* set "DATRT" bit */
	if(pstSettingInfo->bSpNcpl_ReleaseTime == 0)
	{
		bTemp = pstSettingInfo->bSpNcpl_ReleaseTime;
	}
	else
	{
		bTemp = pstSettingInfo->bSpNcpl_AttackTime;
	}
	abWritePrm[2] = (pstSettingInfo->bSpNcpl_PowerLimit << (D4HP3_DPLT & 0xFF))
					| (pstSettingInfo->bSpNg_AttackTime << (D4HP3_NG_ATRT & 0xFF)) 
					| (bTemp << (D4HP3_DATRT & 0xFF));
	/* 0x83 */
	abWritePrm[3] = (pstSettingInfo->bSpNg_DetectionLv << (D4HP3_NG_RATIO & 0xFF))
					| (pstSettingInfo->bSpNcpl_NonClipRatio << (D4HP3_DALC & 0xFF)) 
					| (pstSettingInfo->bSpGainUp << (D4HP3_SP_GAIN & 0xFF));
	/* 0x84 */
	abWritePrm[4] = (pstSettingInfo->bLine1Gain << (D4HP3_VA & 0xFF))
					| (pstSettingInfo->bLine2Gain << (D4HP3_VB & 0xFF));
    /* 0x85 */
	abWritePrm[5] = (pstSettingInfo->bSpSvol << (D4HP3_SPSVOFF & 0xFF))
					| (pstSettingInfo->bSpZcs << (D4HP3_SPZCSOFF & 0xFF))
					| (pstSettingInfo->bSpAtt << (D4HP3_SPATT & 0xFF));
    /* 0x86 */
	abWritePrm[6] = (pstSettingInfo->bHpSvol << (D4HP3_HPSVOFF & 0xFF))
					| (pstSettingInfo->bHpZcs << (D4HP3_HPZCSOFF & 0xFF))
					| (pstSettingInfo->bHpAtt << (D4HP3_HPATT & 0xFF));
	/* 0x87 */
	abWritePrm[7] = (pstSettingInfo->bSpMixer_Line1 << (D4HP3_SP_AMIX & 0xFF))
					| (pstSettingInfo->bSpMixer_Line2 << (D4HP3_SP_BMIX & 0xFF))
					| (pstSettingInfo->bHpCh << (D4HP3_HP_MONO & 0xFF))
					| (pstSettingInfo->bHpMixer_Line1 << (D4HP3_HP_AMIX & 0xFF))
					| (pstSettingInfo->bHpMixer_Line2 << (D4HP3_HP_BMIX & 0xFF));
#if AMPREG_DEBUG
	pr_info(MODULE_NAME ": 0x80 = %02x\n", abWritePrm[0]);
	pr_info(MODULE_NAME ": 0x81 = %02x\n", abWritePrm[1]);
	pr_info(MODULE_NAME ": 0x82 = %02x\n", abWritePrm[2]);
	pr_info(MODULE_NAME ": 0x83 = %02x\n", abWritePrm[3]);
	pr_info(MODULE_NAME ": 0x84 = %02x\n", abWritePrm[4]);
	pr_info(MODULE_NAME ": 0x85 = %02x\n", abWritePrm[5]);
	pr_info(MODULE_NAME ": 0x86 = %02x\n", abWritePrm[6]);
	pr_info(MODULE_NAME ": 0x87 = %02x\n", abWritePrm[7]);
#endif

	/* write 0x80 - 0x87 : power on */
#if 1
	D4Hp3_WriteRegisterByte(0x80, abWritePrm[0]);
	D4Hp3_WriteRegisterByte(0x81, abWritePrm[1]);
	D4Hp3_WriteRegisterByte(0x82, abWritePrm[2]);
	D4Hp3_WriteRegisterByte(0x83, abWritePrm[3]);
	D4Hp3_WriteRegisterByte(0x84, abWritePrm[4]);
	D4Hp3_WriteRegisterByte(0x85, abWritePrm[5]);
	D4Hp3_WriteRegisterByte(0x86, abWritePrm[6]);
	D4Hp3_WriteRegisterByte(0x87, abWritePrm[7]);
#else
	D4Hp3_WriteRegisterByteN(bWriteAddress, abWritePrm, 8);
#endif
}

/*******************************************************************************
 *	D4Hp3_PowerOff
 *
 *	Function:
 *			power off D-4HP3
 *	Argument:
 *			none
 *
 *	Return:
 *			none
 *
 ******************************************************************************/
void D4Hp3_PowerOff(void)
{
	UINT8 bWriteAddress;
	UINT8 bWritePrm;

	/* 0x87 : power off HP amp, SP amp */
	bWriteAddress = 0x87;
	bWritePrm = 0x00;
	D4Hp3_WriteRegisterByte(bWriteAddress, bWritePrm);

	d4Sleep(D4HP3_OFFSEQUENCE_WAITTIME);
}

void yda165_speaker_onoff(int onoff) /* speaker path amp onoff */
{
	D4HP3_SETTING_INFO stInfo;
#if AMPREG_DEBUG	
	unsigned char buf;
#endif
	struct yda165_i2c_data *yd ;
	yd = &g_data;

	if (onoff) //on
	{
		if(yd->power_on)
			yd->power_on();

		cur_mode = 0;
		pr_info(MODULE_NAME ":speaker on[cur_mode:%d]\n", cur_mode);
		
		/* input */
		stInfo.bLine1Gain = g_ampgain[cur_mode].in1_gain;		/* LINE1 Gain Amp */
		stInfo.bLine2Gain = g_ampgain[cur_mode].in2_gain;		/* LINE2 Gain Amp */

		stInfo.bLine1Balance = 0;	/* LINE1 Single-ended(0) or Differential(1) */
		stInfo.bLine2Balance = 0;	/* LINE2 Single-ended(0) or Differential(1) */

		/* HP */
		stInfo.bHpCpMode = 0;			/* HP charge pump mode setting, 3stage mode(0) / 2stage mode(1) */

		
#if defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)
		if(get_hw_rev() < 0x5)
			stInfo.bHpAvddLev = 0;
		else
#elif defined (CONFIG_KOR_MODEL_SHV_E110S) || defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K) || defined (CONFIG_JPN_MODEL_SC_03D)
		if(get_hw_rev() < 0x4) // rev0.3
			stInfo.bHpAvddLev = 0;
		else
#elif defined (CONFIG_USA_MODEL_SGH_I727)
		if(get_hw_rev() < 0x6) // rev0.6
			stInfo.bHpAvddLev = 0;
		else
#endif
		stInfo.bHpAvddLev = 1;			/* HP charge pump AVDD level, 1.65V<=AVDD<2.40V(0) / 2.40V<=AVDD<=2.86V(1) */

		stInfo.bHpEco = 0;				/* HP eco mode, normal(0) / eco mode(1) */
		stInfo.bHpAtt = g_ampgain[cur_mode].hp_att;				/* HP attenuator */
		stInfo.bHpGainUp = g_ampgain[cur_mode].hp_gainup;			/* HP gain up */
		stInfo.bHpSvol = 0;				/* HP soft volume setting, on(0) / off(1) */

#if defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)
        stInfo.bHpZcs = 0;				/* HP zero cross mute setting, on(0) / off(1) */
#elif defined (CONFIG_EUR_MODEL_GT_I9210)
        stInfo.bHpZcs = 0;				/* HP zero cross mute setting, on(0) / off(1) */
#else
        stInfo.bHpZcs = 1;				/* HP zero cross mute setting, on(0) / off(1) */
#endif

		stInfo.bHpCh = 0;				/* HP channel, stereo(0)/mono(1) */
		stInfo.bHpMixer_Line1 = 0;		/* HP mixer LINE1 setting */
		stInfo.bHpMixer_Line2 = 0;		/* HP mixer LINE2 setting */


		/* SP */
		stInfo.bSpAtt = g_ampgain[cur_mode].sp_att;				/* SP attenuator */
		stInfo.bSpGainUp = g_ampgain[cur_mode].sp_gainup;			/* SP gain up */
		stInfo.bSpSvol = 0;				/* SP soft volume setting, on(0) / off(1) */
		stInfo.bSpZcs = 0;				/* SP zero cross mute setting, on(0) / off(1) */

        /* using L/RIN1->EAR_L/R , L/RIN2->SPK_L/R */
        stInfo.bSpMixer_Line1 = 0;		/* SP mixer LINE1 setting */
        stInfo.bSpMixer_Line2 = 1;		/* SP mixer LINE2 setting */

#if defined (CONFIG_USA_MODEL_SGH_T989)  || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_I717) || defined(CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_USA_MODEL_SGH_I577) || defined (CONFIG_USA_MODEL_SGH_T769)
		stInfo.bSpNg_DetectionLv = g_ampgain[cur_mode].bSpNg_DetectionLv;	/* SP Noise Gate : detection level */
		stInfo.bSpNg_AttackTime = g_ampgain[cur_mode].bSpNg_AttackTime;	/* SP Noise Gate : attack time */
		stInfo.bSpNcpl_NonClipRatio = g_ampgain[cur_mode].bSpNcpl_NonClipRatio;  /* SP Non-Clip power limiter : Non-Clip distortion ratio */
		stInfo.bSpNcpl_PowerLimit = g_ampgain[cur_mode].bSpNcpl_PowerLimit;	/* SP Non-Clip power limiter : Power Limit */
		stInfo.bSpNcpl_AttackTime = g_ampgain[cur_mode].bSpNcpl_AttackTime;	/* SP Non-Clip power limiter : attack Time */
		stInfo.bSpNcpl_ReleaseTime = g_ampgain[cur_mode].bSpNcpl_ReleaseTime;	/* SP Non-Clip power limiter : release Time */
#else
		stInfo.bSpNg_DetectionLv = 0;
		stInfo.bSpNg_AttackTime = 1;
#if defined (CONFIG_KOR_MODEL_SHV_E110S) || defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K) || defined (CONFIG_KOR_MODEL_SHV_E120L)
		stInfo.bSpNcpl_NonClipRatio = 0;
#elif defined (CONFIG_Q1_KOR_AUDIO)
		stInfo.bSpNcpl_NonClipRatio = 0;
#else
		stInfo.bSpNcpl_NonClipRatio = 1;
#endif
		stInfo.bSpNcpl_PowerLimit = 0;
		stInfo.bSpNcpl_AttackTime = 1;
		stInfo.bSpNcpl_ReleaseTime = 1;
#endif		
		D4Hp3_PowerOn(&stInfo);

#if AMPREG_DEBUG
		D4Hp3_ReadRegisterByte( 0x80, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x81, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x82, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x83, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x84, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x85, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x86, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x87, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
#endif

	}
	else  //off
	{
		pr_info(MODULE_NAME ":speaker off\n");
		
		D4Hp3_PowerOff();
		
		if(yd->power_off)
			yd->power_off();
	}
	
}
void yda165_headset_onoff(int onoff) /* headset path amp onoff */
{
	D4HP3_SETTING_INFO stInfo;

	struct yda165_i2c_data *yd ;
	yd = &g_data;

	if (onoff)
	{
		if(yd->power_on)
			yd->power_on();

		cur_mode = 1;
		pr_info(MODULE_NAME ":headset on[cur_mode:%d]\n", cur_mode);
		
		/* input */
		stInfo.bLine1Gain = g_ampgain[cur_mode].in1_gain;		/* LINE1 Gain Amp */
		stInfo.bLine2Gain = g_ampgain[cur_mode].in2_gain;		/* LINE2 Gain Amp */

		stInfo.bLine1Balance = 0;	/* LINE1 Single-ended(0) or Differential(1) */
		stInfo.bLine2Balance = 0;	/* LINE2 Single-ended(0) or Differential(1) */

		/* HP */
		stInfo.bHpCpMode = 0;			/* HP charge pump mode setting, 3stage mode(0) / 2stage mode(1) */


#if defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)
		if(get_hw_rev() < 0x5)
			stInfo.bHpAvddLev = 0;
		else
#elif defined (CONFIG_KOR_MODEL_SHV_E110S) || defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K) || defined (CONFIG_JPN_MODEL_SC_03D)
		if(get_hw_rev() < 0x4) // rev0.3
			stInfo.bHpAvddLev = 0;
		else
#elif defined (CONFIG_USA_MODEL_SGH_I727)
		if(get_hw_rev() < 0x6) // rev0.6
			stInfo.bHpAvddLev = 0;
		else
#endif
		stInfo.bHpAvddLev = 1;			/* HP charge pump AVDD level, 1.65V<=AVDD<2.40V(0) / 2.40V<=AVDD<=2.86V(1) */

		stInfo.bHpEco = 0;				/* HP eco mode, normal(0) / eco mode(1) */
		stInfo.bHpAtt = g_ampgain[cur_mode].hp_att;				/* HP attenuator */
		stInfo.bHpGainUp = g_ampgain[cur_mode].hp_gainup;			/* HP gain up */
		stInfo.bHpSvol = 0;				/* HP soft volume setting, on(0) / off(1) */
		stInfo.bHpZcs = 0;;				/* HP zero cross mute setting, on(0) / off(1) */
		stInfo.bHpCh = 0;				/* HP channel, stereo(0)/mono(1) */

        /* using L/RIN1->EAR_L/R , L/RIN2->SPK_L/R */
        stInfo.bHpMixer_Line1 = 1;		/* HP mixer LINE1 setting */
        stInfo.bHpMixer_Line2 = 0;		/* HP mixer LINE2 setting */

		/* SP */
		stInfo.bSpAtt = g_ampgain[cur_mode].sp_att;				/* SP attenuator */
		stInfo.bSpGainUp = g_ampgain[cur_mode].sp_gainup;			/* SP gain up */
		stInfo.bSpSvol = 0;				/* SP soft volume setting, on(0) / off(1) */
		stInfo.bSpZcs = 0;				/* SP zero cross mute setting, on(0) / off(1) */
		stInfo.bSpMixer_Line1 = 0;		/* SP mixer LINE1 setting */
		stInfo.bSpMixer_Line2 = 0;		/* SP mixer LINE2 setting */
		
#if defined (CONFIG_USA_MODEL_SGH_T989)  || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_I717) \
    || defined(CONFIG_USA_MODEL_SGH_I757)|| defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577)
				stInfo.bSpNg_DetectionLv = g_ampgain[cur_mode].bSpNg_DetectionLv;	/* SP Noise Gate : detection level */
				stInfo.bSpNg_AttackTime = g_ampgain[cur_mode].bSpNg_AttackTime; /* SP Noise Gate : attack time */
				stInfo.bSpNcpl_NonClipRatio = g_ampgain[cur_mode].bSpNcpl_NonClipRatio;  /* SP Non-Clip power limiter : Non-Clip distortion ratio */
				stInfo.bSpNcpl_PowerLimit = g_ampgain[cur_mode].bSpNcpl_PowerLimit; /* SP Non-Clip power limiter : Power Limit */
				stInfo.bSpNcpl_AttackTime = g_ampgain[cur_mode].bSpNcpl_AttackTime; /* SP Non-Clip power limiter : attack Time */
				stInfo.bSpNcpl_ReleaseTime = g_ampgain[cur_mode].bSpNcpl_ReleaseTime;	/* SP Non-Clip power limiter : release Time */
#else
				stInfo.bSpNg_DetectionLv = 0;
				stInfo.bSpNg_AttackTime = 1;

#if defined (CONFIG_KOR_MODEL_SHV_E110S) || defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K)  || defined (CONFIG_KOR_MODEL_SHV_E120L)
				stInfo.bSpNcpl_NonClipRatio = 0;
#elif defined (CONFIG_KOR_MODEL_SHV_E160L) // byeongguk.kim_20111025
				stInfo.bSpNcpl_NonClipRatio = 1;
#elif defined (CONFIG_Q1_KOR_AUDIO)
				stInfo.bSpNcpl_NonClipRatio = 1;
#else
				stInfo.bSpNcpl_NonClipRatio = 1;
#endif
				stInfo.bSpNcpl_PowerLimit = 0;
				stInfo.bSpNcpl_AttackTime = 1;
				stInfo.bSpNcpl_ReleaseTime = 1;
#endif	

		D4Hp3_PowerOn(&stInfo);
	}
	else
	{
		pr_info(MODULE_NAME ":headset off\n");
		
		D4Hp3_PowerOff();
		
		if(yd->power_off)
			yd->power_off();
	}
}

#if defined (CONFIG_KOR_MODEL_SHV_E120S)  || defined (CONFIG_KOR_MODEL_SHV_E120K) || defined (CONFIG_KOR_MODEL_SHV_E120L)
void yda165_speaker_call_onoff(int onoff) /* speaker call path amp onoff */
{

	D4HP3_SETTING_INFO stInfo;
#if AMPREG_DEBUG	
	unsigned char buf;
#endif
	struct yda165_i2c_data *yd ;
	yd = &g_data;


	if (onoff) //on
	{
		if(yd->power_on)
			yd->power_on();

		cur_mode = 3;
		pr_info(MODULE_NAME ":speaker call on[cur_mode:%d]\n", cur_mode);
		
		/* input */
		stInfo.bLine1Gain = g_ampgain[cur_mode].in1_gain;		/* LINE1 Gain Amp */
		stInfo.bLine2Gain = g_ampgain[cur_mode].in2_gain;		/* LINE2 Gain Amp */

		stInfo.bLine1Balance = 0;	/* LINE1 Single-ended(0) or Differential(1) */
		stInfo.bLine2Balance = 0;	/* LINE2 Single-ended(0) or Differential(1) */

		/* HP */
		stInfo.bHpCpMode = 0;			/* HP charge pump mode setting, 3stage mode(0) / 2stage mode(1) */

		
#if defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K)
		if(get_hw_rev() < 0x4) // rev0.3
			stInfo.bHpAvddLev = 0;
		else  
#endif
		//defined (CONFIG_KOR_MODEL_SHV_E120L) 
	    stInfo.bHpAvddLev = 1;			/* HP charge pump AVDD level, 1.65V<=AVDD<2.40V(0) / 2.40V<=AVDD<=2.86V(1) */

		stInfo.bHpEco = 0;				/* HP eco mode, normal(0) / eco mode(1) */
		stInfo.bHpAtt = g_ampgain[cur_mode].hp_att;				/* HP attenuator */
		stInfo.bHpGainUp = g_ampgain[cur_mode].hp_gainup;			/* HP gain up */
		stInfo.bHpSvol = 0;				/* HP soft volume setting, on(0) / off(1) */
        stInfo.bHpZcs = 1;				/* HP zero cross mute setting, on(0) / off(1) */
		stInfo.bHpCh = 0;				/* HP channel, stereo(0)/mono(1) */
		stInfo.bHpMixer_Line1 = 0;		/* HP mixer LINE1 setting */
		stInfo.bHpMixer_Line2 = 0;		/* HP mixer LINE2 setting */


		/* SP */
		stInfo.bSpAtt = g_ampgain[cur_mode].sp_att;				/* SP attenuator */
		stInfo.bSpGainUp = g_ampgain[cur_mode].sp_gainup;			/* SP gain up */
		stInfo.bSpSvol = 0;				/* SP soft volume setting, on(0) / off(1) */
		stInfo.bSpZcs = 0;				/* SP zero cross mute setting, on(0) / off(1) */

        /* using L/RIN1->EAR_L/R , L/RIN2->SPK_L/R */
        stInfo.bSpMixer_Line1 = 0;		/* SP mixer LINE1 setting */
        stInfo.bSpMixer_Line2 = 1;		/* SP mixer LINE2 setting */


		stInfo.bSpNg_DetectionLv = 0;
		stInfo.bSpNg_AttackTime = 1;
		
	    // Non-Clip 	
		stInfo.bSpNcpl_NonClipRatio = 1;

		stInfo.bSpNcpl_PowerLimit = 0;
		stInfo.bSpNcpl_AttackTime = 1;
		stInfo.bSpNcpl_ReleaseTime = 1;

		D4Hp3_PowerOn(&stInfo);

        #if AMPREG_DEBUG
		D4Hp3_ReadRegisterByte( 0x80, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x81, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x82, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x83, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x84, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x85, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x86, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x87, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
        #endif

	}
	else  //off
	{
		pr_info(MODULE_NAME ":speaker off\n");
		
		D4Hp3_PowerOff();
		
		if(yd->power_off)
			yd->power_off();
	}
	
}

void yda165_headset_call_onoff(int onoff) /* headset path amp onoff */
{
	D4HP3_SETTING_INFO stInfo;

	struct yda165_i2c_data *yd ;
	yd = &g_data;

	if (onoff)
	{
		if(yd->power_on)
			yd->power_on();

		cur_mode = 4;
		//pr_info(MODULE_NAME ":headset call on[cur_mode:%d]\n", cur_mode);
		
		/* input */
		stInfo.bLine1Gain = g_ampgain[cur_mode].in1_gain;		/* LINE1 Gain Amp */
		stInfo.bLine2Gain = g_ampgain[cur_mode].in2_gain;		/* LINE2 Gain Amp */

		stInfo.bLine1Balance = 0;	/* LINE1 Single-ended(0) or Differential(1) */
		stInfo.bLine2Balance = 0;	/* LINE2 Single-ended(0) or Differential(1) */

		/* HP */
		stInfo.bHpCpMode = 0;			/* HP charge pump mode setting, 3stage mode(0) / 2stage mode(1) */
		stInfo.bHpAvddLev = 1;			/* HP charge pump AVDD level, 1.65V<=AVDD<2.40V(0) / 2.40V<=AVDD<=2.86V(1) */
		stInfo.bHpEco = 0;				/* HP eco mode, normal(0) / eco mode(1) */
		
		stInfo.bHpAtt = g_ampgain[cur_mode].hp_att; 				/* HP attenuator */ 
		pr_info(MODULE_NAME ":headset call on[cur_mode:%d bHpAtt=%d]\n", cur_mode,stInfo.bHpAtt );
		
		stInfo.bHpGainUp = g_ampgain[cur_mode].hp_gainup;			/* HP gain up */		
		stInfo.bHpSvol = 0;				/* HP soft volume setting, on(0) / off(1) */
		stInfo.bHpZcs = 0;				/* HP zero cross mute setting, on(0) / off(1) */
		stInfo.bHpCh = 0;				/* HP channel, stereo(0)/mono(1) */
  
		/* using L/RIN1->EAR_L/R , L/RIN2->SPK_L/R */
		stInfo.bHpMixer_Line1 = 1;		/* HP mixer LINE1 setting */
		stInfo.bHpMixer_Line2 = 0;		/* HP mixer LINE2 setting */  

		/* SP */
		stInfo.bSpAtt = g_ampgain[cur_mode].sp_att;				/* SP attenuator */
		stInfo.bSpGainUp = g_ampgain[cur_mode].sp_gainup;			/* SP gain up */
		stInfo.bSpSvol = 0;				/* SP soft volume setting, on(0) / off(1) */
		stInfo.bSpZcs = 0;				/* SP zero cross mute setting, on(0) / off(1) */
		stInfo.bSpMixer_Line1 = 0;		/* SP mixer LINE1 setting */
		stInfo.bSpMixer_Line2 = 0;		/* SP mixer LINE2 setting */

		stInfo.bSpNg_DetectionLv = 0;
		stInfo.bSpNg_AttackTime = 1;
		stInfo.bSpNcpl_NonClipRatio = 0;
		stInfo.bSpNcpl_PowerLimit = 0;
		stInfo.bSpNcpl_AttackTime = 1;
		stInfo.bSpNcpl_ReleaseTime = 1;

		D4Hp3_PowerOn(&stInfo);
	}
	else
	{
		pr_info(MODULE_NAME ":E120L headset off\n");
		
		D4Hp3_PowerOff();
		
		if(yd->power_off)
			yd->power_off();
	}
}


void yda165_lineout_onoff(int onoff) /* headset path amp onoff */
{
	D4HP3_SETTING_INFO stInfo;

	struct yda165_i2c_data *yd ;
	yd = &g_data;

	if (onoff)
	{
		if(yd->power_on)
			yd->power_on();

		cur_mode = 5;
		pr_info(MODULE_NAME ":headset on[cur_mode:%d]\n", cur_mode);
		
		/* input */
		stInfo.bLine1Gain = g_ampgain[cur_mode].in1_gain;		/* LINE1 Gain Amp */
		stInfo.bLine2Gain = g_ampgain[cur_mode].in2_gain;		/* LINE2 Gain Amp */

		stInfo.bLine1Balance = 0;	/* LINE1 Single-ended(0) or Differential(1) */
		stInfo.bLine2Balance = 0;	/* LINE2 Single-ended(0) or Differential(1) */

		/* HP */
		stInfo.bHpCpMode = 0;			/* HP charge pump mode setting, 3stage mode(0) / 2stage mode(1) */


#if defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)
		if(get_hw_rev() < 0x5)
			stInfo.bHpAvddLev = 0;
		else
#elif defined (CONFIG_KOR_MODEL_SHV_E110S) || defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K) || defined (CONFIG_JPN_MODEL_SC_03D)
		if(get_hw_rev() < 0x4) // rev0.3
			stInfo.bHpAvddLev = 0;
		else
#elif defined (CONFIG_USA_MODEL_SGH_I727)
		if(get_hw_rev() < 0x6) // rev0.6
			stInfo.bHpAvddLev = 0;
		else
#endif
		stInfo.bHpAvddLev = 1;			/* HP charge pump AVDD level, 1.65V<=AVDD<2.40V(0) / 2.40V<=AVDD<=2.86V(1) */

		stInfo.bHpEco = 0;				/* HP eco mode, normal(0) / eco mode(1) */
		stInfo.bHpAtt = g_ampgain[cur_mode].hp_att;				/* HP attenuator */
		stInfo.bHpGainUp = g_ampgain[cur_mode].hp_gainup;			/* HP gain up */
		stInfo.bHpSvol = 0;				/* HP soft volume setting, on(0) / off(1) */
		stInfo.bHpZcs = 0;;				/* HP zero cross mute setting, on(0) / off(1) */
		stInfo.bHpCh = 0;				/* HP channel, stereo(0)/mono(1) */

        /* using L/RIN1->EAR_L/R , L/RIN2->SPK_L/R */
        stInfo.bHpMixer_Line1 = 1;		/* HP mixer LINE1 setting */
        stInfo.bHpMixer_Line2 = 0;		/* HP mixer LINE2 setting */

		/* SP */
		stInfo.bSpAtt = g_ampgain[cur_mode].sp_att;				/* SP attenuator */
		stInfo.bSpGainUp = g_ampgain[cur_mode].sp_gainup;			/* SP gain up */
		stInfo.bSpSvol = 0;				/* SP soft volume setting, on(0) / off(1) */
		stInfo.bSpZcs = 0;				/* SP zero cross mute setting, on(0) / off(1) */
		stInfo.bSpMixer_Line1 = 0;		/* SP mixer LINE1 setting */
		stInfo.bSpMixer_Line2 = 0;		/* SP mixer LINE2 setting */
		
#if defined (CONFIG_USA_MODEL_SGH_T989)  || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_I717) \
    || defined(CONFIG_USA_MODEL_SGH_I757)|| defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577)
				stInfo.bSpNg_DetectionLv = g_ampgain[cur_mode].bSpNg_DetectionLv;	/* SP Noise Gate : detection level */
				stInfo.bSpNg_AttackTime = g_ampgain[cur_mode].bSpNg_AttackTime; /* SP Noise Gate : attack time */
				stInfo.bSpNcpl_NonClipRatio = g_ampgain[cur_mode].bSpNcpl_NonClipRatio;  /* SP Non-Clip power limiter : Non-Clip distortion ratio */
				stInfo.bSpNcpl_PowerLimit = g_ampgain[cur_mode].bSpNcpl_PowerLimit; /* SP Non-Clip power limiter : Power Limit */
				stInfo.bSpNcpl_AttackTime = g_ampgain[cur_mode].bSpNcpl_AttackTime; /* SP Non-Clip power limiter : attack Time */
				stInfo.bSpNcpl_ReleaseTime = g_ampgain[cur_mode].bSpNcpl_ReleaseTime;	/* SP Non-Clip power limiter : release Time */
#else
				stInfo.bSpNg_DetectionLv = 0;
				stInfo.bSpNg_AttackTime = 1;

#if defined (CONFIG_KOR_MODEL_SHV_E110S) || defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K) || defined (CONFIG_KOR_MODEL_SHV_E120L)
				stInfo.bSpNcpl_NonClipRatio = 0;
#elif defined (CONFIG_Q1_KOR_AUDIO)
				stInfo.bSpNcpl_NonClipRatio = 1;
#else
				stInfo.bSpNcpl_NonClipRatio = 1;
#endif
				stInfo.bSpNcpl_PowerLimit = 0;
				stInfo.bSpNcpl_AttackTime = 1;
				stInfo.bSpNcpl_ReleaseTime = 1;
#endif	

		D4Hp3_PowerOn(&stInfo);
	}
	else
	{
		pr_info(MODULE_NAME ":headset off\n");
		
		D4Hp3_PowerOff();
		
		if(yd->power_off)
			yd->power_off();
	}
}

#endif




#if defined (CONFIG_USA_MODEL_SGH_T989)  || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_I717) \
    || defined(CONFIG_USA_MODEL_SGH_I757)|| defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577)
void yda165_speaker_call_onoff(int onoff) /* speaker path amp onoff */
{
	D4HP3_SETTING_INFO stInfo;
#if AMPREG_DEBUG	
	unsigned char buf;
#endif
	struct yda165_i2c_data *yd ;
	yd = &g_data;

	if (onoff) //on
	{
		if(yd->power_on)
			yd->power_on();

		cur_mode = 3;
		pr_info(MODULE_NAME ":speaker call on[cur_mode:%d]\n", cur_mode);
		
		/* input */
		stInfo.bLine1Gain = g_ampgain[cur_mode].in1_gain;		/* LINE1 Gain Amp */
		stInfo.bLine2Gain = g_ampgain[cur_mode].in2_gain;		/* LINE2 Gain Amp */

		stInfo.bLine1Balance = 0;	/* LINE1 Single-ended(0) or Differential(1) */
		stInfo.bLine2Balance = 0;	/* LINE2 Single-ended(0) or Differential(1) */

		/* HP */
		stInfo.bHpCpMode = 0;			/* HP charge pump mode setting, 3stage mode(0) / 2stage mode(1) */

		
#if defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)
		if(get_hw_rev() < 0x5)
			stInfo.bHpAvddLev = 0;
		else
#elif defined (CONFIG_USA_MODEL_SGH_I727)
		if(get_hw_rev() < 0x6) // rev0.6
			stInfo.bHpAvddLev = 0;
		else
#endif
		stInfo.bHpAvddLev = 1;			/* HP charge pump AVDD level, 1.65V<=AVDD<2.40V(0) / 2.40V<=AVDD<=2.86V(1) */


		stInfo.bHpEco = 0;				/* HP eco mode, normal(0) / eco mode(1) */
		stInfo.bHpAtt = g_ampgain[cur_mode].hp_att;				/* HP attenuator */
		stInfo.bHpGainUp = g_ampgain[cur_mode].hp_gainup;			/* HP gain up */
		stInfo.bHpSvol = 0;				/* HP soft volume setting, on(0) / off(1) */

#if defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)
        stInfo.bHpZcs = 0;				/* HP zero cross mute setting, on(0) / off(1) */
#else
        stInfo.bHpZcs = 1;				/* HP zero cross mute setting, on(0) / off(1) */
#endif

		stInfo.bHpCh = 0;				/* HP channel, stereo(0)/mono(1) */
		stInfo.bHpMixer_Line1 = 0;		/* HP mixer LINE1 setting */
		stInfo.bHpMixer_Line2 = 0;		/* HP mixer LINE2 setting */


		/* SP */
		stInfo.bSpAtt = g_ampgain[cur_mode].sp_att;				/* SP attenuator */
		stInfo.bSpGainUp = g_ampgain[cur_mode].sp_gainup;			/* SP gain up */
		stInfo.bSpSvol = 0;				/* SP soft volume setting, on(0) / off(1) */
		stInfo.bSpZcs = 0;				/* SP zero cross mute setting, on(0) / off(1) */

        /* using L/RIN1->EAR_L/R , L/RIN2->SPK_L/R */
        stInfo.bSpMixer_Line1 = 0;		/* SP mixer LINE1 setting */
        stInfo.bSpMixer_Line2 = 1;		/* SP mixer LINE2 setting */


		stInfo.bSpNg_DetectionLv = g_ampgain[cur_mode].bSpNg_DetectionLv;;	/* SP Noise Gate : detection level */
		stInfo.bSpNg_AttackTime = g_ampgain[cur_mode].bSpNg_AttackTime;	/* SP Noise Gate : attack time */
		stInfo.bSpNcpl_NonClipRatio = g_ampgain[cur_mode].bSpNcpl_NonClipRatio;;  /* SP Non-Clip power limiter : Non-Clip distortion ratio */
		stInfo.bSpNcpl_PowerLimit = g_ampgain[cur_mode].bSpNcpl_PowerLimit;	/* SP Non-Clip power limiter : Power Limit */
		stInfo.bSpNcpl_AttackTime = g_ampgain[cur_mode].bSpNcpl_AttackTime;	/* SP Non-Clip power limiter : attack Time */
		stInfo.bSpNcpl_ReleaseTime = g_ampgain[cur_mode].bSpNcpl_ReleaseTime;	/* SP Non-Clip power limiter : release Time */
		
		D4Hp3_PowerOn(&stInfo);

#if AMPREG_DEBUG
		D4Hp3_ReadRegisterByte( 0x80, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x81, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x82, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x83, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x84, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x85, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x86, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
		D4Hp3_ReadRegisterByte( 0x87, &buf);
		pr_info(MODULE_NAME ":%d = %02x\n",__LINE__,buf);
#endif

	}
	else  //off
	{
		pr_info(MODULE_NAME ":speaker call off\n");
		
		D4Hp3_PowerOff();
		
		if(yd->power_off)
			yd->power_off();
	}
	
}

void yda165_headset_call_onoff(int onoff) /* headset path amp onoff */
{
	D4HP3_SETTING_INFO stInfo;

	struct yda165_i2c_data *yd ;
	yd = &g_data;

	if (onoff)
	{
		if(yd->power_on)
			yd->power_on();

		cur_mode = 4;
		pr_info(MODULE_NAME ":headset call on[cur_mode:%d]\n", cur_mode);
		
		/* input */
		stInfo.bLine1Gain = g_ampgain[cur_mode].in1_gain;		/* LINE1 Gain Amp */
		stInfo.bLine2Gain = g_ampgain[cur_mode].in2_gain;		/* LINE2 Gain Amp */

		stInfo.bLine1Balance = 0;	/* LINE1 Single-ended(0) or Differential(1) */
		stInfo.bLine2Balance = 0;	/* LINE2 Single-ended(0) or Differential(1) */

		/* HP */
		stInfo.bHpCpMode = 0;			/* HP charge pump mode setting, 3stage mode(0) / 2stage mode(1) */

		
#if defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)
		if(get_hw_rev() < 0x5)
			stInfo.bHpAvddLev = 0;
		else
#elif defined (CONFIG_USA_MODEL_SGH_I727)
		if(get_hw_rev() < 0x6) // rev0.6
			stInfo.bHpAvddLev = 0;
		else
#endif
		stInfo.bHpAvddLev = 1;			/* HP charge pump AVDD level, 1.65V<=AVDD<2.40V(0) / 2.40V<=AVDD<=2.86V(1) */


		stInfo.bHpEco = 0;				/* HP eco mode, normal(0) / eco mode(1) */
		stInfo.bHpAtt = g_ampgain[cur_mode].hp_att;				/* HP attenuator */
		stInfo.bHpGainUp = g_ampgain[cur_mode].hp_gainup;			/* HP gain up */
		stInfo.bHpSvol = 0;				/* HP soft volume setting, on(0) / off(1) */
		stInfo.bHpZcs = 0;;				/* HP zero cross mute setting, on(0) / off(1) */
		stInfo.bHpCh = 0;				/* HP channel, stereo(0)/mono(1) */

        /* using L/RIN1->EAR_L/R , L/RIN2->SPK_L/R */
        stInfo.bHpMixer_Line1 = 1;		/* HP mixer LINE1 setting */
        stInfo.bHpMixer_Line2 = 0;		/* HP mixer LINE2 setting */

		/* SP */
		stInfo.bSpAtt = g_ampgain[cur_mode].sp_att;				/* SP attenuator */
		stInfo.bSpGainUp = g_ampgain[cur_mode].sp_gainup;			/* SP gain up */
		stInfo.bSpSvol = 0;				/* SP soft volume setting, on(0) / off(1) */
		stInfo.bSpZcs = 0;				/* SP zero cross mute setting, on(0) / off(1) */
		stInfo.bSpMixer_Line1 = 0;		/* SP mixer LINE1 setting */
		stInfo.bSpMixer_Line2 = 0;		/* SP mixer LINE2 setting */
		
		stInfo.bSpNg_DetectionLv = g_ampgain[cur_mode].bSpNg_DetectionLv;	/* SP Noise Gate : detection level */
		stInfo.bSpNg_AttackTime = g_ampgain[cur_mode].bSpNg_AttackTime;		/* SP Noise Gate : attack time */
		stInfo.bSpNcpl_NonClipRatio = g_ampgain[cur_mode].bSpNcpl_NonClipRatio;	/* SP Non-Clip power limiter : Non-Clip distortion ratio */
		stInfo.bSpNcpl_PowerLimit = g_ampgain[cur_mode].bSpNcpl_PowerLimit;	/* SP Non-Clip power limiter : Power Limit */
		stInfo.bSpNcpl_AttackTime = g_ampgain[cur_mode].bSpNcpl_AttackTime;	/* SP Non-Clip power limiter : attack Time */
		stInfo.bSpNcpl_ReleaseTime = g_ampgain[cur_mode].bSpNcpl_ReleaseTime;	/* SP Non-Clip power limiter : release Time */

		D4Hp3_PowerOn(&stInfo);
	}
	else
	{
		pr_info(MODULE_NAME ":headset call off\n");
		
		D4Hp3_PowerOff();
		
		if(yd->power_off)
			yd->power_off();
	}
}
#endif

void yda165_speaker_headset_onoff(int onoff) /* speaker+headset path amp onoff */
{
	D4HP3_SETTING_INFO stInfo;

	struct yda165_i2c_data *yd ;
	yd = &g_data;

	if (onoff)  //on
	{
		if(yd->power_on)
			yd->power_on();

		cur_mode = 2;
		pr_info(MODULE_NAME ":speaker+headset on[cur_mode:%d]\n", cur_mode);
		
		/* input */
		stInfo.bLine1Gain = g_ampgain[cur_mode].in1_gain;		/* LINE1 Gain Amp */
		stInfo.bLine2Gain = g_ampgain[cur_mode].in2_gain;		/* LINE2 Gain Amp */

		stInfo.bLine1Balance = 0;	/* LINE1 Single-ended(0) or Differential(1) */
		stInfo.bLine2Balance = 0;	/* LINE2 Single-ended(0) or Differential(1) */

		/* HP */
		stInfo.bHpCpMode = 0;			/* HP charge pump mode setting, 3stage mode(0) / 2stage mode(1) */

		
#if defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)
		if(get_hw_rev() < 0x5)
			stInfo.bHpAvddLev = 0;
		else
#elif defined (CONFIG_KOR_MODEL_SHV_E110S) || defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K) || defined (CONFIG_JPN_MODEL_SC_03D)
		if(get_hw_rev() < 0x4) // rev0.3
			stInfo.bHpAvddLev = 0;
		else
#elif defined (CONFIG_USA_MODEL_SGH_I727)
		if(get_hw_rev() < 0x6) // rev0.6
			stInfo.bHpAvddLev = 0;
		else
#endif
		stInfo.bHpAvddLev = 1;			/* HP charge pump AVDD level, 1.65V<=AVDD<2.40V(0) / 2.40V<=AVDD<=2.86V(1) */


		stInfo.bHpEco = 0;				/* HP eco mode, normal(0) / eco mode(1) */
		stInfo.bHpAtt = g_ampgain[cur_mode].hp_att;				/* HP attenuator */
		stInfo.bHpGainUp = g_ampgain[cur_mode].hp_gainup;			/* HP gain up */
		stInfo.bHpSvol = 0;				/* HP soft volume setting, on(0) / off(1) */
		stInfo.bHpZcs = 0;;				/* HP zero cross mute setting, on(0) / off(1) */
		stInfo.bHpCh = 0;				/* HP channel, stereo(0)/mono(1) */

        /* using L/RIN1->EAR_L/R , L/RIN2->SPK_L/R */
        stInfo.bHpMixer_Line1 = 1;		/* HP mixer LINE1 setting */
        stInfo.bHpMixer_Line2 = 0;		/* HP mixer LINE2 setting */


		/* SP */
		stInfo.bSpAtt = g_ampgain[cur_mode].sp_att;				/* SP attenuator */
		stInfo.bSpGainUp = g_ampgain[cur_mode].sp_gainup;			/* SP gain up */
		stInfo.bSpSvol = 0;				/* SP soft volume setting, on(0) / off(1) */
		stInfo.bSpZcs = 0;				/* SP zero cross mute setting, on(0) / off(1) */
		
        /* using L/RIN1->EAR_L/R , L/RIN2->SPK_L/R */
        stInfo.bSpMixer_Line1 = 0;		/* SP mixer LINE1 setting */
        stInfo.bSpMixer_Line2 = 1;		/* SP mixer LINE2 setting */

#if defined (CONFIG_USA_MODEL_SGH_T989)  || defined (CONFIG_USA_MODEL_SGH_I727) || defined (CONFIG_USA_MODEL_SGH_I717) \
    || defined(CONFIG_USA_MODEL_SGH_I757)|| defined (CONFIG_USA_MODEL_SGH_T769) || defined(CONFIG_USA_MODEL_SGH_I577)
				stInfo.bSpNg_DetectionLv = g_ampgain[cur_mode].bSpNg_DetectionLv;	/* SP Noise Gate : detection level */
				stInfo.bSpNg_AttackTime = g_ampgain[cur_mode].bSpNg_AttackTime; /* SP Noise Gate : attack time */
				stInfo.bSpNcpl_NonClipRatio = g_ampgain[cur_mode].bSpNcpl_NonClipRatio;  /* SP Non-Clip power limiter : Non-Clip distortion ratio */
				stInfo.bSpNcpl_PowerLimit = g_ampgain[cur_mode].bSpNcpl_PowerLimit; /* SP Non-Clip power limiter : Power Limit */
				stInfo.bSpNcpl_AttackTime = g_ampgain[cur_mode].bSpNcpl_AttackTime; /* SP Non-Clip power limiter : attack Time */
				stInfo.bSpNcpl_ReleaseTime = g_ampgain[cur_mode].bSpNcpl_ReleaseTime;	/* SP Non-Clip power limiter : release Time */
#else
				stInfo.bSpNg_DetectionLv = 0;
				stInfo.bSpNg_AttackTime = 1;

#if defined (CONFIG_KOR_MODEL_SHV_E110S) || defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K) || defined (CONFIG_KOR_MODEL_SHV_E120L)
				stInfo.bSpNcpl_NonClipRatio = 0;
#elif defined (CONFIG_Q1_KOR_AUDIO)
				stInfo.bSpNcpl_NonClipRatio = 0;
#else
				stInfo.bSpNcpl_NonClipRatio = 1;
#endif
				stInfo.bSpNcpl_PowerLimit = 0;
				stInfo.bSpNcpl_AttackTime = 1;
				stInfo.bSpNcpl_ReleaseTime = 1;
#endif			
		D4Hp3_PowerOn(&stInfo);
	}
	else  //off
	{
		pr_info(MODULE_NAME ":speaker+headset off\n");
		
		D4Hp3_PowerOff();
		
		if(yd->power_off)
			yd->power_off();
	}
}

#ifdef CONFIG_EUR_MODEL_GT_I9210
void yda165_lineout_onoff(int onoff) /* lineout path amp onoff */
{
	D4HP3_SETTING_INFO stInfo;

	struct yda165_i2c_data *yd ;
	yd = &g_data;

	if (onoff)
	{
		if(yd->power_on)
			yd->power_on();

		cur_mode = 5;
		pr_info(MODULE_NAME ":lineout on[cur_mode:%d]\n", cur_mode);

		/* input */
		stInfo.bLine1Gain = g_ampgain[cur_mode].in1_gain;		/* LINE1 Gain Amp */
		stInfo.bLine2Gain = g_ampgain[cur_mode].in2_gain;		/* LINE2 Gain Amp */

		stInfo.bLine1Balance = 0;	/* LINE1 Single-ended(0) or Differential(1) */
		stInfo.bLine2Balance = 0;	/* LINE2 Single-ended(0) or Differential(1) */

		/* HP */
		stInfo.bHpCpMode = 0;			/* HP charge pump mode setting, 3stage mode(0) / 2stage mode(1) */
		
		if(get_hw_rev() < 0x6) // rev0.6
			stInfo.bHpAvddLev = 0;
		else
			stInfo.bHpAvddLev = 1;		/* HP charge pump AVDD level, 1.65V<=AVDD<2.40V(0) / 2.40V<=AVDD<=2.86V(1) */
				
		stInfo.bHpEco = 0;				/* HP eco mode, normal(0) / eco mode(1) */
		stInfo.bHpAtt = g_ampgain[cur_mode].hp_att;				/* HP attenuator */
		stInfo.bHpGainUp = g_ampgain[cur_mode].hp_gainup;			/* HP gain up */
		stInfo.bHpSvol = 0;				/* HP soft volume setting, on(0) / off(1) */

	        stInfo.bHpZcs = 0;				/* HP zero cross mute setting, on(0) / off(1) */

		stInfo.bHpCh = 0;				/* HP channel, stereo(0)/mono(1) */
		stInfo.bHpMixer_Line1 = 1;		/* HP mixer LINE1 setting */
		stInfo.bHpMixer_Line2 = 0;		/* HP mixer LINE2 setting */

		/* SP */
		stInfo.bSpAtt = g_ampgain[cur_mode].sp_att;				/* SP attenuator */
		stInfo.bSpGainUp = g_ampgain[cur_mode].sp_gainup;			/* SP gain up */
		stInfo.bSpSvol = 0;				/* SP soft volume setting, on(0) / off(1) */
		stInfo.bSpZcs = 0;				/* SP zero cross mute setting, on(0) / off(1) */

	        /* using L/RIN1->EAR_L/R , L/RIN2->SPK_L/R */
	        stInfo.bSpMixer_Line1 = 0;		/* SP mixer LINE1 setting */
	        stInfo.bSpMixer_Line2 = 0;		/* SP mixer LINE2 setting */

		stInfo.bSpNg_DetectionLv = 2;
		stInfo.bSpNg_AttackTime = 1;
		stInfo.bSpNcpl_NonClipRatio = 1;
		stInfo.bSpNcpl_PowerLimit = 0;
		stInfo.bSpNcpl_AttackTime = 1;
		stInfo.bSpNcpl_ReleaseTime = 1;

		D4Hp3_PowerOn(&stInfo);
	}
	else
	{
		pr_info(MODULE_NAME ":lineout off\n");
		
		D4Hp3_PowerOff();
		
		if(yd->power_off)
			yd->power_off();
	}
}
#endif

void yda165_tty_onoff(int onoff) /* tty path amp onoff */
{
	D4HP3_SETTING_INFO stInfo;

	struct yda165_i2c_data *yd ;
	yd = &g_data;

	if (onoff)
	{
		pr_info(MODULE_NAME ":tty on\n");

		if(yd->power_on)
			yd->power_on();

		/* input */
		stInfo.bLine1Gain = 2;		/* LINE1 Gain Amp */
		stInfo.bLine2Gain = 2;		/* LINE2 Gain Amp */

		stInfo.bLine1Balance = 0;	/* LINE1 Single-ended(0) or Differential(1) */
		stInfo.bLine2Balance = 1;	/* LINE2 Single-ended(0) or Differential(1) */

		/* HP */
		stInfo.bHpCpMode = 0;			/* HP charge pump mode setting, 3stage mode(0) / 2stage mode(1) */
		
#if defined (CONFIG_USA_MODEL_SGH_T989) || defined (CONFIG_USA_MODEL_SGH_T769)
		if(get_hw_rev()>=0x5)
			stInfo.bHpAvddLev = 1;
		else
			stInfo.bHpAvddLev = 0;
#elif defined (CONFIG_USA_MODEL_SGH_I727)
		if(get_hw_rev()>=0x6) // rev0.6
			stInfo.bHpAvddLev = 1;
		else
			stInfo.bHpAvddLev = 0;
#elif defined (CONFIG_USA_MODEL_SGH_I717)
			stInfo.bHpAvddLev = 1;
#else
		stInfo.bHpAvddLev = 0;			/* HP charge pump AVDD level, 1.65V<=AVDD<2.40V(0) / 2.40V<=AVDD<=2.86V(1) */
#endif

		stInfo.bHpEco = 0;				/* HP eco mode, normal(0) / eco mode(1) */
		stInfo.bHpAtt = 31;				/* HP attenuator */
		stInfo.bHpGainUp = 0;			/* HP gain up */
		stInfo.bHpSvol = 0;				/* HP soft volume setting, on(0) / off(1) */
		stInfo.bHpZcs = 0;;				/* HP zero cross mute setting, on(0) / off(1) */
		stInfo.bHpCh = 0;				/* HP channel, stereo(0)/mono(1) */

        /* using L/RIN1->EAR_L/R , L/RIN2->SPK_L/R */
        stInfo.bHpMixer_Line1 = 1;		/* HP mixer LINE1 setting */
        stInfo.bHpMixer_Line2 = 0;		/* HP mixer LINE2 setting */
	

		/* SP */
		stInfo.bSpAtt = 0;				/* SP attenuator */
		stInfo.bSpGainUp = 0;			/* SP gain up */
		stInfo.bSpSvol = 0;				/* SP soft volume setting, on(0) / off(1) */
		stInfo.bSpZcs = 1;				/* SP zero cross mute setting, on(0) / off(1) */
		stInfo.bSpMixer_Line1 = 0;		/* SP mixer LINE1 setting */
		stInfo.bSpMixer_Line2 = 0;		/* SP mixer LINE2 setting */
		stInfo.bSpNg_DetectionLv = 0;	/* SP Noise Gate : detection level */
		stInfo.bSpNg_AttackTime = 1;		/* SP Noise Gate : attack time */
		stInfo.bSpNcpl_NonClipRatio = 1;	/* SP Non-Clip power limiter : Non-Clip distortion ratio */
		stInfo.bSpNcpl_PowerLimit = 1;	/* SP Non-Clip power limiter : Power Limit */
		stInfo.bSpNcpl_AttackTime = 2;	/* SP Non-Clip power limiter : attack Time */
		stInfo.bSpNcpl_ReleaseTime = 1;	/* SP Non-Clip power limiter : release Time */
		
		D4Hp3_PowerOn(&stInfo);
	}
	else
	{
		pr_info(MODULE_NAME ":tty off\n");
		
		D4Hp3_PowerOff();

		if(yd->power_off)
			yd->power_off();
	}
}

static int amp_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int amp_release(struct inode *inode, struct file *file)
{
	return 0;
}

#if 0//rohbt_temp
static int amp_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	/* int mode;
	 * switch (cmd)
	 * {
		 * case SND_SET_AMPGAIN :
			 * if (copy_from_user(&mode, (void __user *) arg, sizeof(mode)))
			 * {
				 * pr_err(MODULE_NAME ": %s fail\n", __func__);
				 * break;
			 * }
			 * if (mode >= 0 && mode < MODE_NUM_MAX)
				 * cur_mode = mode;

			 * break;
		 * default :
			 * break;
	 * } */
	return 0;
}
#endif

static struct file_operations amp_fops = {
	.owner = THIS_MODULE,
	.open = amp_open,
	.release = amp_release,
//rohbt_temp	.ioctl = amp_ioctl,
};

static struct miscdevice amp_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "amp",
	.fops = &amp_fops,
};

static int __devinit yda165_probe(struct i2c_client *client, const struct i2c_device_id * dev_id)
{
	int err = 0;
	
	pr_info(MODULE_NAME ":%s\n",__func__);
	pr_err(MODULE_NAME ":%s\n",__func__);
	pr_notice("%s: OK\n", __func__);

	if(!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		goto exit_check_functionality_failed;

	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		return err;
	}

	pclient = client;

	memcpy(&g_data, client->dev.platform_data, sizeof(struct yda165_i2c_data));

	if(misc_register(&amp_device))
	{
		pr_err(MODULE_NAME ": misc device register failed\n"); 		
	}
	load_ampgain();

	err = sysfs_create_group(&amp_device.this_device->kobj, &yda165_attribute_group);
	if(err)
	{
		pr_err(MODULE_NAME ":sysfs_create_group failed %s\n", amp_device.name);
		goto exit_sysfs_create_group_failed;
	}

	//if(charging_boot) //shim
		//D4Hp3_PowerOff();

	return 0;

exit_check_functionality_failed:
exit_sysfs_create_group_failed:
	return err;
}

static int __devexit yda165_remove(struct i2c_client *client)
{
	pclient = NULL;

	return 0;
}

#if defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K) || defined (CONFIG_KOR_MODEL_SHV_E120L)
static void __devexit yda165_shutdown(struct i2c_client *client)
{
	pr_info(MODULE_NAME ": %s : amp power off\n", __func__);

	D4Hp3_PowerOff();

	pr_info(MODULE_NAME ": %s : done\n", __func__);

	return;
}
#endif

static const struct i2c_device_id yda165_id[] = {
	{ "yda165", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, yda165_id);

static struct i2c_driver yda165_driver = {
	.id_table   	= yda165_id,
	.probe 			= yda165_probe,
	.remove 		= yda165_remove,
/*
#ifndef CONFIG_ANDROID_POWER
	.suspend    	= yda165_suspend,
	.resume 		= yda165_resume,
#endif
	.shutdown 		= yda165_shutdown,
*/
#if defined (CONFIG_KOR_MODEL_SHV_E120S) || defined (CONFIG_KOR_MODEL_SHV_E120K) || defined (CONFIG_KOR_MODEL_SHV_E120L)
	/* If the device power offed with power key while something is playing,
	 * you colud hear amp off pop-up noise at the end of power off sequence.
	 * Then, add shutdown function to remove noise.
	*/
	.shutdown 		= yda165_shutdown,
#endif
	.driver = {
		.name   = "yda165",
		.owner	= THIS_MODULE,
	},
};

#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

static int __init yda165_init(void)
{
#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return 0\n", __func__);
		return 0;
	}
#endif

	pr_info(MODULE_NAME ":%s\n",__func__);
	return i2c_add_driver(&yda165_driver);
}

static void __exit yda165_exit(void)
{
	i2c_del_driver(&yda165_driver);
}

module_init(yda165_init);
module_exit(yda165_exit);

MODULE_AUTHOR("Jongcheol Park");
MODULE_DESCRIPTION("YDA165 Driver");
MODULE_LICENSE("GPL");



