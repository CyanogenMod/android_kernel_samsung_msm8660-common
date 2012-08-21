

/***************************************************************
CAMERA Power control
****************************************************************/


#include "sec_apex_pmic.h"


#include <asm/gpio.h>

#include <linux/clk.h>
#include <linux/io.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>

#include <linux/regulator/consumer.h>

static struct regulator *i_core12, *s_core12, *s_io18, *i_host18, *af28, *vt_core15;

extern unsigned int get_hw_rev(void);


void cam_mclk_onoff(int onoff)
{
	unsigned int mclk_cfg;
	if(onoff) {
#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
		mclk_cfg = GPIO_CFG(32, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_4MA);
#else
		mclk_cfg = GPIO_CFG(32, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA);
#endif
		gpio_tlmm_config(mclk_cfg, GPIO_CFG_ENABLE);

	}
	else {
#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
		mclk_cfg = GPIO_CFG(32, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_4MA);
#else
		mclk_cfg = GPIO_CFG(32, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA);
#endif
		gpio_tlmm_config(mclk_cfg, GPIO_CFG_ENABLE);
	}
}

int cam_ldo_power_on(void)
{
	int ret;
	int temp=0;

	printk("%s:cam_ldo_power_on\n", __func__);

	cam_mclk_onoff(OFF);
	mdelay(5);

	//preempt_disable();

//ISP CORE 1.2V	5M Core 1.2v DVDD
	i_core12 = regulator_get(NULL, "8901_s2");
	if (IS_ERR(i_core12))
		goto main_cam_power_fail;

	ret = regulator_set_voltage(i_core12, 1200000, 1200000);

	if (ret) {
		printk("%s:i_core12 error setting voltage\n", __func__);
	}
	ret = regulator_enable(i_core12);
	if (ret) {
		printk("%s:i_core12 error enabling regulator\n", __func__);
	}
	mdelay(1);


//SENSOR A2.8V Sensor AVDD 2.8v AVDD
	gpio_set_value_cansleep(CAM_IO_EN, HIGH);
	mdelay(1); //min 20us


// VT core 1.5v //DVDD 1.5V (sub)
	if (get_hw_rev() >= 0x09)
		vt_core15 = regulator_get(NULL, "8058_l24");
	else
	{
		printk("DVDD1.5V : 8058_l10\n");
		vt_core15 = regulator_get(NULL, "8058_l10");
	}
	if (IS_ERR(vt_core15))
		goto main_cam_power_fail;

	ret = regulator_set_voltage(vt_core15, 1500000, 1500000);
	if (ret) {
		printk("%s:vt_core15 error setting voltage\n", __func__);
	}
	ret = regulator_enable(vt_core15);
	if (ret) {
		printk("%s:vt_core15 error enabling regulator\n", __func__);
	}
	mdelay(1);  //min 15us


//HOST 1.8V  SENSOR I/O 1.8V VDDIO

	if (get_hw_rev()>=0x0D) //Hercules_rev06
 		i_host18 = regulator_get(NULL, "8901_usb_otg");
	else
	{
		printk("Host1.8V : 8058_l8\n");
		i_host18 = regulator_get(NULL, "8058_l8");
		if (IS_ERR(i_host18))
			goto main_cam_power_fail;

		ret = regulator_set_voltage(i_host18, 1800000, 1800000);
		if (ret) {
			printk("%s:i_host18 error setting voltage\n", __func__);
		}

	}

	if (IS_ERR(i_host18))
		goto main_cam_power_fail;

	ret = regulator_enable(i_host18);
	if (ret) {
		printk("%s:i_host18 error enabling regulator\n", __func__);
	}
	mdelay(1);

//AF 2.8V
	af28 = regulator_get(NULL, "8058_l15"); //AF 2.8V

	if (IS_ERR(af28))
		goto main_cam_power_fail;

	ret = regulator_set_voltage(af28, 2850000, 2850000);
	if (ret) {
		printk("%s:af28 error setting voltage\n", __func__);
	}
	ret = regulator_enable(af28);
	if (ret) {
		printk("%s:af28 error enabling regulator\n", __func__);
	}
	mdelay(1);	// min 5ms~max 10ms,


// VT STBY
    gpio_set_value_cansleep(CAM_FRONT_EN, HIGH);
	temp = gpio_get_value(CAM_FRONT_EN);
	printk("[isx012] VT STBY : %d\n", temp);

    mdelay(1);


//MCLK
	cam_mclk_onoff(ON);
	mdelay(1);

	return ret;


main_cam_power_fail:
	return -1;

}

int sub_cam_ldo_power(int onoff)
{
	int ret = 0;
	printk("%s: %d\n", __func__, onoff);

	if(onoff) { // power on
		cam_mclk_onoff(OFF);
		mdelay(5);

//ISP CORE 1.2V	5M Core 1.2V
	i_core12 = regulator_get(NULL, "8901_s2"); //CORE 1.2V

	if (IS_ERR(i_core12))
		goto sub_cam_power_fail;

	ret = regulator_set_voltage(i_core12, 1200000, 1200000);
	if (ret) {
		printk("%s: i_core12 error setting voltage\n", __func__);
	}
	ret = regulator_enable(i_core12);
	if (ret) {
		printk("%s: i_core12 error enabling regulator\n", __func__);
	}
	mdelay(2);


//SENSOR A2.8V SENSOR AVDD 2.8V
	gpio_set_value_cansleep(CAM_IO_EN, HIGH);
	mdelay(1); //min 20us


//DVDD 1.5V (sub) VT CORE 1.5V
	if (get_hw_rev() >= 0x09)
		vt_core15 = regulator_get(NULL, "8058_l24");
	else
	{
		printk("DVDD1.5V : 8058_l10\n");
		vt_core15 = regulator_get(NULL, "8058_l10");
	}
	if (IS_ERR(vt_core15))
		goto sub_cam_power_fail;

	ret = regulator_set_voltage(vt_core15, 1500000, 1500000);
	if (ret) {
		printk("%s:vt_core15 error setting voltage\n", __func__);
	}
	ret = regulator_enable(vt_core15);
	if (ret) {
		printk("%s:vt_core15 error enabling regulator\n", __func__);
	}
	udelay(50);  //min 15us


//HOST 1.8V SENSOR IO 1.8V
	if (get_hw_rev()>=0x0D) //Hercules_rev06
		i_host18 = regulator_get(NULL, "8901_usb_otg");
	else
	{
		printk("Host1.8V : 8058_l8\n");
		i_host18 = regulator_get(NULL, "8058_l8");
		if (IS_ERR(i_host18))
			goto sub_cam_power_fail;

		ret = regulator_set_voltage(i_host18, 1800000, 1800000);
		if (ret) {
			printk("%s:i_host18 error setting voltage\n", __func__);
		}
	}
	if (IS_ERR(i_host18))
		goto sub_cam_power_fail;

	ret = regulator_enable(i_host18);
	if (ret) {
		printk("%s: i_host18 error enabling regulator\n", __func__);
	}
	mdelay(1);

//VT STBY
	gpio_set_value_cansleep(CAM_FRONT_EN, HIGH); // STBY
	mdelay(2);	//udelay(50);

	cam_mclk_onoff(ON);
	mdelay(1); // min50us
	}
	else{ //off
		cam_mclk_onoff(OFF);// Disable MCLK
		mdelay(2);	//udelay(50);



//HOST 1.8V  SENSOR I/O 1.8V VDDIO
	if (regulator_is_enabled(i_host18)) {
		ret=regulator_disable(i_host18);
		if (ret) {
			printk("%s:i_host18 error disabling regulator\n", __func__);
		}
		//regulator_put(l8);
	}
	mdelay(1);


//DVDD 1.5V (sub)	 VT CORE 1.5V
	if (regulator_is_enabled(vt_core15)) {
		ret=regulator_disable(vt_core15);
		if (ret) {
			printk("%s:vt_core15 error disabling regulator\n", __func__);
		}
		//regulator_put(l24);
	}
	mdelay(1);

//SENSOR A2.8V ADD 2.8V
	gpio_set_value_cansleep(CAM_IO_EN, LOW);
	mdelay(1);


//ISP CORE 1.2V 5M CORE 1.2V
   	if (regulator_is_enabled(i_core12)) {
   		ret=regulator_disable(i_core12);
   		if (ret) {
   			printk("%s:i_core12 error disabling regulator\n", __func__);
   		}
   		//regulator_put(s2);
   	}
   	mdelay(5);

	}

	return ret;

sub_cam_power_fail:
	return -1;

}


int cam_ldo_power_off(void)
{
	int ret = 0;
	printk("cam_ldo_power_off\n");


// Disable MCLK
	cam_mclk_onoff(OFF);
	mdelay(1);
   printk("cam_mclk_onoff\n");


//VT RESET
   gpio_set_value_cansleep(CAM_FRONT_RST, 0);//RST
   mdelay(1);

//AF 2.8V
	if (regulator_is_enabled(af28)) {
		ret=regulator_disable(af28);
		if (ret) {
			printk("%s:af28 error disabling regulator\n", __func__);
		}
		//regulator_put(l15);
	}
	mdelay(1);


//HOST 1.8V
	if (regulator_is_enabled(i_host18)) {
		ret=regulator_disable(i_host18);
		if (ret) {
			printk("%s:i_host18 error disabling regulator\n", __func__);
		}
		//regulator_put(l8);
	}
	mdelay(1);


// VT core 1.5v
	if (regulator_is_enabled(vt_core15)) {
		ret=regulator_disable(vt_core15);
		if (ret) {
			printk("%s:vt_core15 error disabling regulator\n", __func__);
		}
		//regulator_put(l24);
	}
	mdelay(1);


//SENSOR A2.8V Sensor AVDD 2.8v AVDD
	gpio_set_value_cansleep(CAM_IO_EN, LOW);  //HOST 1.8V
	mdelay(1);



//ISP CORE 1.2V	5M Core 1.2v DVDD
	if (regulator_is_enabled(i_core12)) {
		ret=regulator_disable(i_core12);
		if (ret) {
			printk("%s:i_core12 error disabling regulator\n", __func__);
		}
		//regulator_put(s2);
	}
	mdelay(5);


	printk("cam_ldo_power_off done\n");
	return ret;
}


