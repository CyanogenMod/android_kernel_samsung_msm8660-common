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


#ifndef ISX012_H
#define ISX012_H


#define	ISX012_DEBUG
#ifdef ISX012_DEBUG
#define CAM_DEBUG(fmt, arg...)	\
		do {\
			printk(KERN_DEBUG "[ISX012] %s : " fmt "\n", __FUNCTION__, ##arg);}\
		while(0)

#define cam_info(fmt, arg...)	\
		do {\
		printk(KERN_INFO "[ISX012]" fmt "\n",##arg);}\
		while(0)

#define cam_err(fmt, arg...)	\
		do {\
		printk(KERN_ERR "[ISX012] %s:%d:" fmt "\n",__FUNCTION__, __LINE__, ##arg);}\
		while(0)

#else
#define CAM_DEBUG(fmt, arg...)
#define cam_info(fmt, arg...)
#define cam_err(fmt, arg...)
#endif


#define ISX012_DELAY		0xFFFF0000

struct isx012_setting_struct {

    enum msm_sensor_mode camera_mode; // camera or camcorder

    int8_t snapshot_size;
    int8_t preview_size;

    int8_t effect;
    int8_t brightness;
    int8_t whiteBalance;
    int8_t iso;
    int8_t scene;
    int8_t metering;
    int8_t contrast;
    int8_t saturation;
    int8_t sharpness;
    int8_t fps;
    int8_t zoom;

    int8_t afmode;
    int8_t afcanceled;
    int8_t flash_mode;
    int8_t flash_exifinfo;
    int8_t lowcap_on;
    int8_t nightcap_on;

    int8_t ae_lock;
    int8_t awb_lock;
    int8_t flash_status;

    int8_t auto_contrast;

    int8_t current_lux;

    int8_t check_dataline;

};


struct isx012_status_struct {
//	int8_t power_on;
	int8_t started;
	int8_t initialized;//  1 is not init a sensor
	int8_t config_csi1;

	int8_t need_configuration;
	int8_t camera_status;
	int8_t start_af;
	int8_t cancel_af;
	int8_t cancel_af_running;
	int8_t AE_AWB_hunt;
	int pos_x;	//touch-af
	int pos_y;	//touch-af
	int touchaf;	//touch-af
	int apps;
};



struct isx012_ctrl_t {
	const struct  msm_camera_sensor_info 	*sensordata;

	struct isx012_status_struct status;

	struct isx012_setting_struct setting;
};


struct isx012_exif_data {
	u32 exptime;            /* us */
	u16 flash;
	u16 iso;
	unsigned int shutterspeed;
	int tv;                 /* shutter speed */
	int bv;                 /* brightness */
	int ebv;                /* exposure bias */
};

#endif /* S5K4ECGX_H */

