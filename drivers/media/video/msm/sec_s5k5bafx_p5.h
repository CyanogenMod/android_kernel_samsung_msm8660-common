
#ifndef _S5K5CCAF_H_
#define _S5K5CCAF_H_

/*******************************************************************************************
 #  Display resolution standards #

	QCIF: 176 x 144
	CIF: 352 x 288
	QVGA: 320 x 240
	VGA: 640 x 480 
	SVGA: 800 x 600 
	XGA: 1024 x 768 
	WXGA: 1280 x 800 
	QVGA: 1280 x 960 
	SXGA: 1280 x 1024 
	SXGA+: 1400 x 1050 
	WSXGA+: 1680 x 1050 
	UXGA: 1600 x 1200 
	WUXGA: 1920 x 1200 
	QXGA: 2048 x 1536
********************************************************************************************/

#if defined(CONFIG_TARGET_LOCALE_KOR_SKT) || defined(CONFIG_TARGET_LOCALE_KOR_KT) || defined(CONFIG_TARGET_LOCALE_KOR_LGU)
#define	S5K5BAFX_DEBUG	0
#else
#define	S5K5BAFX_DEBUG	1
#endif

#if S5K5BAFX_DEBUG
#define CAM_DEBUG(fmt, arg...)	\
		do {\
			printk("\033[[s5k5bafx] %s:%d: " fmt "\033[0m\n", __FUNCTION__, __LINE__, ##arg);}\
		while(0)
			
#define cam_info(fmt, arg...)	\
		do {\
		printk(KERN_INFO "[s5k5bafx]" fmt "\n",##arg);}\
		while(0)
			
#define cam_err(fmt, arg...)	\
		do {\
		printk(KERN_ERR "[s5k5bafx] %s:%d:" fmt "\n",__FUNCTION__, __LINE__, ##arg);}\
		while(0)
			
#else
#define CAM_DEBUG(fmt, arg...)	
#define cam_info(fmt, arg...)
#define cam_err(fmt, arg...)
#endif

#define	CAM_3M_RST	50
#define	CAM_3M_STBY	49
#define	CAM_VGA_RST	41
#define	CAM_VGA_STBY	42


#define	S5K5BAFX_PREVIEW_MODE		0
#define	S5K5BAFX_CAPTURE_MODE		1


#define	S5K5BAFX_CAMERA_MODE		0
#define	S5K5BAFX_MOVIE_MODE		1
#define	S5K5BAFX_MMS_MODE		2

#define	S5K5BAFX_3RD_PARTY_APP		0
#define	S5K5BAFX_SAMSUNG_APP		1


/* Preview Size */
#define	S5K5BAFX_PREVIEW_VGA		0	/* 640x480 */
#define	S5K5BAFX_PREVIEW_D1		1	/* 720x480 */
#define	S5K5BAFX_PREVIEW_SVGA		2	/* 800x600 */
#define	S5K5BAFX_PREVIEW_XGA		3	/* 1024x768*/
#define	S5K5BAFX_PREVIEW_PVGA		4	/* 1280*720*/
#define	S5K5BAFX_PREVIEW_528x432	5	/* 528*432 */


/* Brightness */
#define S5K5BAFX_BR_MINUS_4				0
#define S5K5BAFX_BR_MINUS_3				1
#define S5K5BAFX_BR_MINUS_2				2
#define S5K5BAFX_BR_MINUS_1				3
#define S5K5BAFX_BR_DEFAULT				4
#define S5K5BAFX_BR_PLUS_1					5
#define S5K5BAFX_BR_PLUS_2					6
#define S5K5BAFX_BR_PLUS_3					7
#define S5K5BAFX_BR_PLUS_4					8

#define S5K5BAFX_FRAME_AUTO		0
#define S5K5BAFX_FRAME_FIX_15		1
#define S5K5BAFX_FRAME_FIX_30		2

#define S5K5BAFX_7_FPS				7
#define S5K5BAFX_10_FPS			10
#define S5K5BAFX_12_FPS			12
#define S5K5BAFX_15_FPS			15
#define S5K5BAFX_30_FPS			30

#define S5K5BAFX_DTP_ON		1
#define S5K5BAFX_DTP_OFF		0


/* BLUR  */
#define S5K5BAFX_BLUR_LEVEL_0				0
#define S5K5BAFX_BLUR_LEVEL_1				1
#define S5K5BAFX_BLUR_LEVEL_2				2
#define S5K5BAFX_BLUR_LEVEL_3				3


/* MODE */
#define S5K5BAFX_MODE_PREVIEW		1
#define S5K5BAFX_MODE_CAPTURE		2	
#define S5K5BAFX_MODE_DTP		3


/* DTP */
#define DTP_OFF		0
#define DTP_ON		1
#define DTP_OFF_ACK		2
#define DTP_ON_ACK		3

struct s5k5bafx_userset {
	unsigned int focus_mode;
	unsigned int focus_status;
	unsigned int continuous_af;

	unsigned int	metering;
	unsigned int	exposure;
	unsigned int		wb;
	unsigned int		iso;
	int	contrast;
	int	saturation;
	int	sharpness;
	int	brightness;
	int	ev;

	unsigned int zoom;
	unsigned int effect;	/* Color FX (AKA Color tone) */
	unsigned int scenemode;
	unsigned int detectmode;
	unsigned int antishake;
	unsigned int exif_shutterspeed;
	
	unsigned int stabilize;	/* IS */

	unsigned int strobe;
	unsigned int jpeg_quality;
	unsigned int preview_size_idx;
	unsigned int capture_size;
	unsigned int thumbnail_size;
	unsigned int fps;
	unsigned int fps_mode;
};

struct s5k5bafx_ctrl {
	const struct msm_camera_sensor_info *sensordata;
	struct s5k5bafx_userset settings;
	
	int op_mode;
	int dtp_mode;
	int cam_mode;	// camera or camcorder
	int vtcall_mode;
	int started;
	int sensor_mode;
	int app_mode;
};

#endif
