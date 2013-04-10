
#ifndef _SEC_APEX_PMIC_H
#define _SEC_APEX_PMIC_H


#define	CAM_5M_RST		50
#define	CAM_5M_EN		49
#define	CAM_FRONT_RST		41
#define	CAM_FRONT_EN		42

#define	CAM_IO_EN		37
#define FLASH_SEL		106

#define	CAM_FLASH_EN		62
#define	CAM_FLASH_SET		63
#define	VT_CAM_ID		64

#define GPIO_ISP_INT   49

#define	ON		1
#define	OFF		0
#define LOW		0
#define HIGH		1

int cam_ldo_power_on(void);
int cam_ldo_power_off(void);


int sub_cam_ldo_power(int onoff);


#endif

