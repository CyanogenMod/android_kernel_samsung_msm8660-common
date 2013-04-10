/*
 * Copyright (c) 2008 QUALCOMM USA, INC.
 * Author: Haibo Jeff Zhong <hzhong@qualcomm.com>
 *
 * All source code in this file is licensed under the following license
 * except where indicated.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 *
 */


#ifndef S5K6AAFX_H
#define S5K6AAFX_H


//#define	S5K6AAFX_DEBUG
#ifdef S5K6AAFX_DEBUG
#define CAM_DEBUG(fmt, arg...)	\
		do {\
			printk(KERN_DEBUG "[6AAFX] %s : " fmt "\n", __FUNCTION__, ##arg);}\
		while(0)

#define cam_info(fmt, arg...)	\
		do {\
		printk(KERN_INFO "[6AAFX]" fmt "\n",##arg);}\
		while(0)

#define cam_err(fmt, arg...)	\
		do {\
		printk(KERN_ERR "[6AAFX] %s:%d:" fmt "\n",__FUNCTION__, __LINE__, ##arg);}\
		while(0)

#else
#define CAM_DEBUG(fmt, arg...)
#define cam_info(fmt, arg...)
#define cam_err(fmt, arg...)
#endif


#define S5K6AAFX_DELAY		0xffff0000

struct s5k6aafx_ctrl_t {
	int8_t opened;
	const struct msm_camera_sensor_info 	*sensordata;
	int dtp_mode;
	enum msm_sensor_mode  sensor_mode;	// camera or camcorder
	int vtcall_mode;
	int started;
	int initialized;

	unsigned int check_dataline;
	int brightness;
	int blur;
	int exif_exptime;
	int exif_iso;
};



#endif /* S5K6AAFX_H */




