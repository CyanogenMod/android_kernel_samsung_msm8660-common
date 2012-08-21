

/***************************************************************
CAMERA Power control
****************************************************************/


#include "sec_cam_pmic.h"


#include <asm/gpio.h> 

#include <linux/clk.h>
#include <linux/io.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>

#include <linux/regulator/consumer.h>
#define CAM_TEST_REV03 //temp, rev03


struct regulator *l8, *l15, *l10, *lvs0, *lvs1, *l9; //temp
struct regulator *s2, *s3;

#if defined(CONFIG_MACH_P8_LTE) && !defined(CONFIG_TARGET_LOCALE_KOR_SKT)
struct regulator *l14;	// before DV version
#endif


/* CAM power
	CAM_SENSOR_A_2.8		:  VREG_L8A		: l8
	CAM_SENSOR_IO_1.8		: VREG_LVS0A	: lvs0
	3M_CORE_1.2			: VREG_S2B		: s2
	3M_AF_2.8			: VREG_L15A		: l15
	VT_CORE_1.8			: VREG_L10A		: 10
	VT_CAM_1.5			: VREG_L10A		: l10
*/

void cam_ldo_power_on(const char *sensor_name)
{
	int ret;
	
	printk("#### cam_ldo_power_on ####\n");
	printk("%s: system_rev %d\n", __func__, system_rev);

// CAM_A_2.8V 
#if defined(CONFIG_MACH_P8_LTE) && !defined(CONFIG_TARGET_LOCALE_KOR_SKT)
	printk("Enable CAM_A_2.8V for P8LTE");
	l14 = regulator_get(NULL, "8058_l14"); //AF 2.8V
	ret = regulator_set_voltage(l14, 2850000, 2850000);
	if (ret) {
	printk("%s: error setting voltage l14\n", __func__);
	}
	ret = regulator_enable(l14);
	if (ret) {
	printk("%s: error enabling regulator l14\n", __func__);
	}
	mdelay(1);	// 10 -> 1
#else
	gpio_set_value_cansleep(CAM_IO_EN, HIGH);  
	mdelay(1);	// 10 -> 1
#endif


//SENSOR_CORE_1.2V
	s2 = regulator_get(NULL, "8901_s2"); //CORE 1.2V
	ret = regulator_set_voltage(s2, 1200000, 1200000);
	if (ret) {
		printk("%s: error setting voltage\n", __func__);
	}
	ret = regulator_enable(s2);
	if (ret) {
		printk("%s: error enabling regulator\n", __func__);
	}
	mdelay(2);	// 5 -> 2

	
//VT_CORE_1.5V(sub)
#if defined(CONFIG_TARGET_LOCALE_USA) || defined(CONFIG_TARGET_LOCALE_KOR) || defined(CONFIG_TARGET_LOCALE_JPN) 

#if defined(CONFIG_MACH_P8_LTE) && defined(CONFIG_TARGET_LOCALE_KOR_SKT)
	if(system_rev>0x00)
#else
	if(system_rev>0x03)
#endif
	{
printk("[1] %s: system_rev %d\n", __func__, system_rev);
		l9 = regulator_get(NULL, "8058_l9"); 
		ret = regulator_set_voltage(l9, 1500000, 1500000);
		if (ret) {
			printk("%s: error setting voltage l9\n", __func__);
		}
		ret = regulator_enable(l9);
		if (ret) {
			printk("%s: error enabling regulator l9\n", __func__);
		}
	} else
#endif	
	{
printk("[2] %s: system_rev %d\n", __func__, system_rev);
		l10 = regulator_get(NULL, "8058_l10"); 
		ret = regulator_set_voltage(l10, 1500000, 1500000);
		if (ret) {
			printk("%s: error setting voltage l10\n", __func__);
		}
		ret = regulator_enable(l10);
		if (ret) {
			printk("%s: error enabling regulator l10\n", __func__);
		}
	}

	mdelay(1);	// 10 -> 1
	
//CAM_IO_1.8V 
	lvs0 = regulator_get(NULL, "8058_lvs0");
	ret = regulator_enable(lvs0);
	if (ret) {
		printk("%s: error enabling regulator lvs0\n", __func__);
	}
	mdelay(1);	// 10-> 1

// 3M_AF 2.8V	
	l15 = regulator_get(NULL, "8058_l15"); //AF 2.8V
	ret = regulator_set_voltage(l15, 2850000, 2850000);
	if (ret) {
		printk("%s: error setting voltage l15\n", __func__);
	}
	ret = regulator_enable(l15);
	if (ret) {
		printk("%s: error enabling regulator l15\n", __func__);
	}
	mdelay(1);	// 10-> 1
	
	if(!strcmp(sensor_name, "s5k5ccaf"))
	{
//		gpio_set_value_cansleep(CAM_3M_STBY, LOW);	
//		gpio_set_value_cansleep(CAM_3M_STBY, HIGH); 
		printk("%s: change PWR SEQUENCE 20110812 \n", __func__);
	}
	else
	{
		gpio_set_value_cansleep(CAM_VGA_STBY, LOW); 
		gpio_set_value_cansleep(CAM_VGA_STBY, HIGH);			
		printk("%s: change PWR SEQUENCE 20110812 \n", __func__);
	}
	mdelay(1);	// 10-> 1
}

void cam_ldo_power_off(void)
{
	int ret;

	printk("#### cam_ldo_power_off ####\n");

	// change power sequence 20110812 Jeonhk
	gpio_set_value_cansleep(CAM_VGA_STBY, LOW);
	gpio_set_value_cansleep(CAM_3M_STBY, LOW);
	mdelay(1);


	// 3M_AF 2.8V
	if (l15) {
		ret = regulator_disable(l15);
		if (ret) {
			printk("%s: error disabling regulator\n", __func__);
		}
		//regulator_put(lvs0);
	}	
	mdelay(2);	// 5-> 2

	//CAM_IO_1.8V
	if (lvs0) {
		ret=regulator_disable(lvs0);
		if (ret) {
			printk("%s: error disabling regulator\n", __func__);
		}
		//regulator_put(lvs1);
	}
	mdelay(2);	// 5-> 2

	//VT_CORE_1.5V(sub)
//HC-original   #if defined(CONFIG_TARGET_LOCALE_USA_ATT) || defined(CONFIG_TARGET_LOCALE_EUR_OPEN)	|| defined(CONFIG_TARGET_LOCALE_KOR_SKT) || defined(CONFIG_TARGET_LOCALE_KOR_LGU)
#if defined(ONFIG_TARGET_LOCALE_USA) || defined(CONFIG_TARGET_LOCALE_KOR) || defined(CONFIG_TARGET_LOCALE_JPN)
#if defined(CONFIG_MACH_P8_LTE) && defined(CONFIG_TARGET_LOCALE_KOR_SKT)
	if(system_rev>0x00)
#else
	if(system_rev>0x03)
#endif
	{
		 printk("%s: L9   \n", __func__);
		if (l9) {
			ret=regulator_disable(l9);
			if (ret) {
				printk("%s: error disabling regulator\n", __func__);
			}
			//regulator_put(l15);
		}
	} else
#endif
	{
		if (l10) {
			ret=regulator_disable(l10);
			if (ret) {
				printk("%s: error disabling regulator\n", __func__);
			}
			//regulator_put(l15);
		}
	}

	mdelay(2);	// 5-> 2
	
	//SENSOR_CORE_1.2V
	if (s2) {
		ret=regulator_disable(s2);
		if (ret) {
			printk("%s: error disabling regulator\n", __func__);
		}
		//regulator_put(l24);
	}	
	mdelay(2);	// 5-> 2
	
	// CAM_A_2.8V 
#if defined(CONFIG_MACH_P8_LTE) && !defined(CONFIG_TARGET_LOCALE_KOR_SKT)
	printk("Disable CAM_A_2.8V for P8LTE");
	if (l14) {
		ret = regulator_disable(l14);
		if (ret) {
			printk("%s: error disabling regulator\n", __func__);
		}
	}	
	mdelay(2);	// 10-> 2
#else
	gpio_set_value_cansleep(CAM_IO_EN, LOW);  
	mdelay(2);	// 10-> 2
#endif	
}

