
#ifndef _SEC_CAM_PMIC_H
#define _SEC_CAM_PMIC_H


#if 0
#define	CAM_8M_RST		50
#define	CAM_VGA_RST		41

#define	CAM_MEGA_EN		37	

#define	CAM_VGA_EN		42

#define	CAM_PMIC_STBY		37	

#define	CAM_IO_EN		37	
#endif

#define	CAM_3M_RST	50
#define	CAM_3M_STBY	49
#define	CAM_VGA_RST	41
#define	CAM_VGA_STBY	42
#define CAM_IO_EN	57

#define	ON		1
#define	OFF		0
#define LOW		0
#define HIGH		1

void cam_ldo_power_on(const char *sensor_name);
void cam_ldo_power_off(void);

void s5k6aafx_ldo_power_on(void);


#endif
