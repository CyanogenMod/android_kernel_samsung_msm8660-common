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


#ifndef SEC_M5MO_H
#define SEC_M5MO_H

//#include <mach/board.h> //PGH
//#include <linux/mfd/pmic8058.h>


#define	CAMERA_DEBUG	

#ifdef CAMERA_DEBUG
#define CAM_DEBUG(fmt, arg...)	\
		do {\
		printk(KERN_DEBUG "[M5MO] %s : " fmt "\n", __FUNCTION__, ##arg);}\
		while(0)
			
#define cam_info(fmt, arg...)	\
		do {\
		printk(KERN_INFO "[M5MO]" fmt "\n",##arg);}\
		while(0)
			
#define cam_err(fmt, arg...)	\
		do {\
		printk(KERN_ERR "[M5MO] %s:%d:" fmt "\n",__FUNCTION__, __LINE__, ##arg);}\
		while(0)
			
#else
#define CAM_DEBUG(fmt, arg...)	
#define cam_info(fmt, arg...)
#define cam_err(fmt, arg...)
#endif


#if defined (CONFIG_TARGET_SERIES_Q1)//160S/K/L, I727
#define CAMERA_WXGA_PREVIEW 
#endif

#define M5MO_IMG_I2C_ADDR	0x3F

#define	PREVIEW_NORMAL		0
#define	PREVIEW_WIDE		1
#define	PREVIEW_HD		2
#define	PREVIEW_MAX		3

#define PREVIEW_SIZE HVGA

#define FLASH_CAMERA       0
#define FLASH_MOVIE        1  

#define FLASH_CMD_ON      1
#define FLASH_CMD_OFF     0

#define FLASH_CMD       200
#define RESET_FOR_FW    202
#define MODE_CMD		203

#define MODE_CAMERA			0
#define MODE_CAMCORDER		1
#define MODE_FW				2

#define	SCREEN_SIZE_WIDTH		800
#define	SCREEN_SIZE_HEIGHT		480

/* FPS Capabilities */
#define M5MO_5_FPS			5
#define M5MO_15_FPS			15
#define M5MO_30_FPS			30
#define M5MO_60_FPS			60
/*
#define M5MO_5_FPS				0x1
#define M5MO_7_FPS				0x2
#define M5MO_10_FPS			0x3
#define M5MO_12_FPS			0x4
#define M5MO_15_FPS			0x5
#define M5MO_24_FPS			0x6
#define M5MO_30_FPS			0x7
#define M5MO_60_FPS			0x8
#define M5MO_120_FPS			0x9
#define M5MO_AUTO_FPS			0xA
*/
#if defined (CAMERA_WXGA_PREVIEW)
enum m5mo_prev_frmsize {
	M5MO_PREVIEW_QCIF = 0,
	M5MO_PREVIEW_QCIF2,
	M5MO_PREVIEW_QVGA,
	M5MO_PREVIEW_VGA,
	M5MO_PREVIEW_D1,
	M5MO_PREVIEW_WVGA,
	M5MO_PREVIEW_qHD,
	M5MO_PREVIEW_HD,
	M5MO_PREVIEW_FHD,
	M5MO_PREVIEW_HDR,
	M5MO_PREVIEW_1280x800,
	M5MO_PREVIEW_1072x800
};
#else
enum m5mo_prev_frmsize {
	M5MO_PREVIEW_QCIF = 0,
	M5MO_PREVIEW_QCIF2,
	M5MO_PREVIEW_QVGA,
	M5MO_PREVIEW_VGA,
	M5MO_PREVIEW_D1,
	M5MO_PREVIEW_WVGA,
	M5MO_PREVIEW_qHD,
	M5MO_PREVIEW_HD,
	M5MO_PREVIEW_FHD,
	M5MO_PREVIEW_HDR
};

#endif

enum m5mo_cap_frmsize {
	M5MO_CAPTURE_8MP = 0,	/* 3264 x 2448 */
	M5MO_CAPTURE_W7MP,	/* WQXGA - 2560 x 1536 */
	M5MO_CAPTURE_HD6MP,	/* 3264 x 1836*/
	M5MO_CAPTURE_HD4MP,	/* 2560 x 1440*/
	M5MO_CAPTURE_3MP,	/* QXGA - 2048 x 1536 */
	M5MO_CAPTURE_W2MP,	/* 2048 x 1232 */
	M5MO_CAPTURE_SXGA,	/* 1280 x 960 */
	M5MO_CAPTURE_HD1MP,	/* 1280 x 720*/
	M5MO_CAPTURE_WVGA,	/* 800 x 480 */
	M5MO_CAPTURE_VGA,	/* 640 x 480 */
};




struct m5mo_frmsizeenum {
	unsigned int index;
	unsigned int width;
	unsigned int height;
	u8 reg_val;		/* a value for category parameter */
};



/* M5MO Preview Size */
#define M5MO_SUB_QCIF_SIZE	0x1	
#define M5MO_QQVGA_SIZE		0x3		
#define M5MO_144_176_SIZE		0x4
#define M5MO_QCIF_SIZE			0x5
#define M5MO_176_176_SIZE		0x6
#define M5MO_LQVGA_SIZE		0x8		
#define M5MO_QVGA_SIZE			0x9		
#define M5MO_LWQVGA_SIZE		0xC		
#define M5MO_WQVGA_SIZE		0xD		
#define M5MO_CIF_SIZE			0xE		
#define M5MO_480_360_SIZE		0x13		
#define M5MO_QHD_SIZE			0x15		
#define M5MO_VGA_SIZE			0x17		
#define M5MO_NTSC_SIZE			0x18		
#define M5MO_WVGA_SIZE		0x1A		
#define M5MO_SVGA_SIZE			0x1F		// 1600x1200
#define M5MO_HD_SIZE			0x21		
#define M5MO_FULL_HD_SIZE		0x25
#define M5MO_8M_SIZE			0x29
#define M5MO_QVGA_SL60_SIZE	0x30		
#define M5MO_QVGA_SL120_SIZE	0x31		
#if defined (CAMERA_WXGA_PREVIEW) 
#define M5MO_1072x800_SIZE		0x36//for Quincy
#define M5MO_1280x800_SIZE		0x35//for Quincy
#endif
/* M5MO Thumbnail Size */
#define M5MO_THUMB_QVGA_SIZE		0x1
#define M5MO_THUMB_400_225_SIZE	0x2
#define M5MO_THUMB_WQVGA_SIZE	0x3
#define M5MO_THUMB_VGA_SIZE		0x4
#define M5MO_THUMB_WVGA_SIZE		0x5

#define AF_FAILED	0
#define AF_NONE		1
#define AF_SUCCEED	2
#define AF_IDLE		4
#define AF_BUSY		5



/* Flash Setting */
#define M5MO_FLASH_CAPTURE_OFF		0
#define M5MO_FLASH_CAPTURE_AUTO		1
#define M5MO_FLASH_CAPTURE_ON		2
#define M5MO_FLASH_MOVIE_ON		3
#define M5MO_FLASH_MOVIE_ALWAYS		4
#define M5MO_FLASH_LOW_ON		5
#define M5MO_FLASH_LOW_OFF		6


/* JPEG Quality */ 
#define M5MO_JPEG_SUPERFINE		1
#define M5MO_JPEG_FINE			2
#define M5MO_JPEG_NORMAL		3
#define M5MO_JPEG_ECONOMY		4

#define M5MO_I2C_VERIFY_RETRY		200

#define SDCARD_FW

#ifdef SDCARD_FW
#define M5MO_FW_PATH_SDCARD			"/sdcard/RS_M5LS.bin" 
//#define M5MO_FW_PATH_SDCARD			"/sdcard/external_sd/RS_M5LS.bin"
#endif

#if defined(CONFIG_USA_MODEL_SGH_T989)
#define M5MOS_FW_REQUEST_PATH	"/system/cameradata/RS_M5LS_S_TMO.bin"	/* SEMKO */
#define M5MOO_FW_REQUEST_PATH	"/system/cameradata/RS_M5LS_O_TMO.bin"	/* Optical communication */
#elif defined(CONFIG_JPN_MODEL_SC_05D)
#define M5MOS_FW_REQUEST_PATH	"/system/etc/firmware/RS_M5LS_S.bin"
#define M5MOO_FW_REQUEST_PATH	"/system/etc/firmware/RS_M5LS_OO.bin"
#elif defined (CONFIG_TARGET_SERIES_Q1)
#define M5MOS_FW_REQUEST_PATH	"/system/cameradata/RS_M5LS_S.bin"	/* Samsung Electro-Mechanics */
#define M5MOO_FW_REQUEST_PATH	"/system/cameradata/RS_M5LS_OO.bin"	/* Optical communication */
#else
#define M5MOS_FW_REQUEST_PATH	"/system/cameradata/RS_M5LS_S.bin"	/* Samsung Electro-Mechanics */
#define M5MOO_FW_REQUEST_PATH	"/system/cameradata/RS_M5LS_O.bin"	/* Optical communication */
#endif


#define M5MO_FW_DUMP_PATH	"/data/RS_M5LS_dump.bin"
#define M5MO_FLASH_BASE_ADDR	0x10000000
#define M5MO_INT_RAM_BASE_ADDR	0x68000000
#define M5MO_VERSION_INFO_SIZE	21
#define M5MO_VERSION_INFO_ADDR	0x16FF00 //0x16fee0 // offset in binary file 


struct m5mo_isp {
	wait_queue_head_t wait;
	unsigned int irq;	/* irq issued by ISP */
	unsigned int issued;
	unsigned int int_factor;
	unsigned int bad_fw:1;
};

struct m5mo_size
{
	int width;
	int height;
};

struct m5mo_userset {
	unsigned int	metering;
	unsigned int	exposure;
	unsigned int	wb;
	unsigned int	iso;
	int	contrast;
	int	saturation;
	int	sharpness;
	int	ev;

	unsigned int zoom;
	unsigned int effect;	/* Color FX (AKA Color tone) */
	enum camera_scene_mode scenemode;
	enum msm_sensor_mode sensor_mode;
	unsigned int detectmode;
	unsigned int antishake;
	unsigned int face_beauty:1;
	
	unsigned int stabilize;	/* IS */

	unsigned int strobe;
	unsigned int jpeg_quality;
	//unsigned int preview_size;
	struct m5mo_size preview_size;
	struct m5mo_size picture_size;
	unsigned int thumbnail_size;
	unsigned int fps;

	unsigned int started;

	unsigned int check_dataline;
};

struct m5mo_focus {
	unsigned int start:1;
	unsigned int lock:1;
	unsigned int touch:1;
	unsigned int center;

	unsigned int mode;
	unsigned int status;

	unsigned int touchaf;

	unsigned int pos_x;
	unsigned int pos_y;
};


struct m5mo_jpeg {
	unsigned int main_size;		/* Main JPEG file size */
	unsigned int thumb_size;	/* Thumbnail file size */
	unsigned int jpeg_done;
};

struct m5mo_ctrl_t {
	int8_t  opened;
	const struct  msm_camera_sensor_info 	*sensordata;

	struct m5mo_userset settings;
	struct m5mo_isp isp;
	struct m5mo_focus focus;
	
	struct m5mo_jpeg jpeg;
	
	char *fw_version;
	
};

struct m5mo_exif_data {
	char unique_id[7];
	u32 exptime;		/* us */
	u16 flash;
	u16 iso;
	int tv;			/* shutter speed */
	int bv;			/* brightness */
	int ebv;		/* exposure bias */
	
};


/* Category */
#define M5MO_CATEGORY_SYS	0x00
#define M5MO_CATEGORY_PARM	0x01
#define M5MO_CATEGORY_MON	0x02
#define M5MO_CATEGORY_AE	0x03
#define M5MO_CATEGORY_WB	0x06
#define M5MO_CATEGORY_EXIF	0x07
#define M5MO_CATEGORY_FD	0x09
#define M5MO_CATEGORY_LENS	0x0A
#define M5MO_CATEGORY_CAPPARM	0x0B
#define M5MO_CATEGORY_CAPCTRL	0x0C
#define M5MO_CATEGORY_TEST	0x0D
#define M5MO_CATEGORY_ADJST	0x0E
#define M5MO_CATEGORY_FLASH	0x0F    /* F/W update */

/* M5MO_CATEGORY_SYS: 0x00 */
#define M5MO_SYS_PJT_CODE	0x01
#define M5MO_SYS_VER_FW		0x02
#define M5MO_SYS_VER_HW		0x04
#define M5MO_SYS_VER_PARAM	0x06
#define M5MO_SYS_VER_AWB	0x08
#define M5MO_SYS_USER_VER	0x0A
#define M5MO_SYS_MODE		0x0B
#define M5MO_SYS_ESD_INT	0x0E
#define M5MO_SYS_INT_FACTOR	0x10
#define M5MO_SYS_INT_EN		0x11
#define M5MO_SYS_ROOT_EN	0x12

/* M5MO_CATEGORY_PARAM: 0x01 */
#define M5MO_PARM_OUT_SEL	0x00
#define M5MO_PARM_MON_SIZE	0x01
#define M5MO_PARM_EFFECT	0x0B
#define M5MO_PARM_FLEX_FPS	0x31
#define M5MO_PARM_HDMOVIE	0x32
#define M5MO_PARM_HDR_MON	0x39
#define M5MO_PARM_HDR_MON_OFFSET_EV	0x3A
#define M5MO_PARM_MIPI_DATA_TYPE	0x3C

/* M5MO_CATEGORY_MON: 0x02 */
#define M5MO_MON_ZOOM		0x01
#define M5MO_MON_MON_REVERSE	0x05
#define M5MO_MON_MON_MIRROR	0x06
#define M5MO_MON_SHOT_REVERSE	0x07
#define M5MO_MON_SHOT_MIRROR	0x08
#define M5MO_MON_CFIXB		0x09
#define M5MO_MON_CFIXR		0x0A
#define M5MO_MON_COLOR_EFFECT	0x0B
#define M5MO_MON_CHROMA_LVL	0x0F
#define M5MO_MON_EDGE_LVL	0x11
#define M5MO_MON_TONE_CTRL	0x25

/* M5MO_CATEGORY_AE: 0x03 */
#define M5MO_AE_LOCK		0x00
#define M5MO_AE_MODE		0x01
#define M5MO_AE_ISOSEL		0x05
#define M5MO_AE_FLICKER		0x06
#define M5MO_AE_EP_MODE_MON	0x0A
#define M5MO_AE_EP_MODE_CAP	0x0B
#define M5MO_AE_ONESHOT_MAX_EXP	0x36
#define M5MO_AE_INDEX		0x38

/* M5MO_CATEGORY_WB: 0x06 */
#define M5MO_AWB_LOCK		0x00
#define M5MO_WB_AWB_MODE	0x02
#define M5MO_WB_AWB_MANUAL	0x03

/* M5MO_CATEGORY_EXIF: 0x07 */
#define M5MO_EXIF_EXPTIME_NUM	0x00
#define M5MO_EXIF_EXPTIME_DEN	0x04
#define M5MO_EXIF_TV_NUM	0x08
#define M5MO_EXIF_TV_DEN	0x0C
#define M5MO_EXIF_BV_NUM	0x18
#define M5MO_EXIF_BV_DEN	0x1C
#define M5MO_EXIF_EBV_NUM	0x20
#define M5MO_EXIF_EBV_DEN	0x24
#define M5MO_EXIF_ISO		0x28
#define M5MO_EXIF_FLASH		0x2A

/* M5MO_CATEGORY_FD: 0x09 */
#define M5MO_FD_CTL		0x00
#define M5MO_FD_SIZE		0x01
#define M5MO_FD_MAX		0x02

/* M5MO_CATEGORY_LENS: 0x0A */
#define M5MO_LENS_AF_MODE	0x01
#define M5MO_LENS_AF_START	0x02
#define M5MO_LENS_AF_STATUS	0x03

#define M5MO_LENS_AF_MODE_SELECT	0x05
#define M5MO_LENS_AF_UPBYTE_STEP	0x06
#define M5MO_LENS_AF_LOWBYTE_STEP	0x07
#define M5MO_LENS_AF_CAL_DATA_READ	0x0C
#define M5MO_LENS_AF_CAL_DATA		0x13
#define M5MO_LENS_AF_CAL	0x1D
#define M5MO_LENS_AF_TOUCH_POSX	0x30
#define M5MO_LENS_AF_TOUCH_POSY	0x32

/* M5MO_CATEGORY_CAPPARM: 0x0B */
#define M5MO_CAPPARM_YUVOUT_MAIN	0x00
#define M5MO_CAPPARM_MAIN_IMG_SIZE	0x01
#define M5MO_CAPPARM_YUVOUT_PREVIEW	0x05
#define M5MO_CAPPARM_PREVIEW_IMG_SIZE	0x06
#define M5MO_CAPPARM_YUVOUT_THUMB	0x0A
#define M5MO_CAPPARM_THUMB_IMG_SIZE	0x0B
#define M5MO_CAPPARM_JPEG_SIZE_MAX	0x0F
#define M5MO_CAPPARM_JPEG_RATIO		0x17
#define M5MO_CAPPARM_MCC_MODE		0x1D
#define M5MO_CAPPARM_WDR_EN		0x2C
#define M5MO_CAPPARM_LIGHT_CTRL		0x40
#define M5MO_CAPPARM_FLASH_CTRL		0x41
#define M5MO_CAPPARM_JPEG_RATIO_OFS	0x34
#define M5MO_CAPPARM_THUMB_JPEG_MAX	0x3C
#define M5MO_CAPPARM_AFB_CAP_EN		0x53
#define M5MO_CAPPARM_FLASH_LOWTEMP	0x21

/* M5MO_CATEGORY_CAPCTRL: 0x0C */
#define M5MO_CAPCTRL_CAP_MODE	0x00
#define M5MO_CAPCTRL_CAP_FRM_COUNT 0x02
#define M5MO_CAPCTRL_FRM_SEL	0x06
#define M5MO_CAPCTRL_TRANSFER	0x09
#define M5MO_CAPCTRL_IMG_SIZE	0x0D
#define M5MO_CAPCTRL_THUMB_SIZE	0x11

/* M5MO_CATEGORY_ADJST: 0x0E */
#define M5MO_ADJST_AWB_RG_H	0x3C
#define M5MO_ADJST_AWB_RG_L	0x3D
#define M5MO_ADJST_AWB_BG_H	0x3E
#define M5MO_ADJST_AWB_BG_L	0x3F

/* M5MO_CATEGORY_FLASH: 0x0F */
#define M5MO_FLASH_ADDR		0x00
#define M5MO_FLASH_BYTE		0x04
#define M5MO_FLASH_ERASE	0x06
#define M5MO_FLASH_WR		0x07
#define M5MO_FLASH_RAM_CLEAR	0x08
#define M5MO_FLASH_CAM_START	0x12
#define M5MO_FLASH_SEL		0x13

/* M5MO_CATEGORY_TEST:	0x0D */
#define M5MO_TEST_OUTPUT_YCO_TEST_DATA		0x1B
#define M5MO_TEST_ISP_PROCESS			0x59

/* M5MO Sensor Mode */
#define M5MO_SYSINIT_MODE	0x0
#define M5MO_PARMSET_MODE	0x1
#define M5MO_MONITOR_MODE	0x2
#define M5MO_STILLCAP_MODE	0x3

/* Interrupt Factor */
#define M5MO_INT_SOUND		(1 << 7)
#define M5MO_INT_LENS_INIT	(1 << 6)
#define M5MO_INT_FD		(1 << 5)
#define M5MO_INT_FRAME_SYNC	(1 << 4)
#define M5MO_INT_CAPTURE	(1 << 3)
#define M5MO_INT_ZOOM		(1 << 2)
#define M5MO_INT_AF		(1 << 1)
#define M5MO_INT_MODE		(1 << 0)

/* ESD Interrupt */
#define M5MO_INT_ESD		(1 << 0)

#endif /* SEC_M5MO_H */

