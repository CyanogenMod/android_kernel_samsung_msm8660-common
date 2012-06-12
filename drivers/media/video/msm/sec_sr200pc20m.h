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


#ifndef SR200PC20M_H
#define SR200PC20M_H


#define	SR200PC20M_DEBUG	
#ifdef SR200PC20M_DEBUG
#define CAM_DEBUG(fmt, arg...)	\
		do {\
			printk(KERN_DEBUG "[SR200PC20M] %s : " fmt "\n", __FUNCTION__, ##arg);}\
		while(0)
			
#define cam_info(fmt, arg...)	\
		do {\
		printk(KERN_INFO "[SR200PC20M]" fmt "\n",##arg);}\
		while(0)
			
#define cam_err(fmt, arg...)	\
		do {\
		printk(KERN_ERR "[SR200PC20M] %s:%d:" fmt "\n",__FUNCTION__, __LINE__, ##arg);}\
		while(0)
			
//#define CAM_I2C_DEBUG
			
#else
#define CAM_DEBUG(fmt, arg...)	
#define cam_info(fmt, arg...)
#define cam_err(fmt, arg...)
#endif

#define SR200PC20M_DELAY		0xFF00

struct sr200pc20m_ctrl_t {
	int8_t  opened;
	const struct  msm_camera_sensor_info 	*sensordata;
	int dtp_mode;
	enum msm_sensor_mode  sensor_mode;	// camera or camcorder
	int vtcall_mode;
	int started;
	int initialized;

	unsigned int check_dataline;
	int	brightness;
	int	blur;
	int 	exif_exptime;
	int	exif_iso;
};



#endif /* SR200PC20M_H */

