
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

#if defined(CONFIG_TARGET_LOCALE_KOR_SKT) || defined(CONFIG_TARGET_LOCALE_KOR_KT) || defined(CONFIG_TARGET_LOCALE_KOR_LGU) || defined (CONFIG_TARGET_LOCALE_USA)
#define	S5K5CCAF_DEBUG	0
#else
#define	S5K5CCAF_DEBUG	1
#endif

#if S5K5CCAF_DEBUG
#define CAM_DEBUG(fmt, arg...)	\
		do {\
		printk(KERN_DEBUG "[S5K5CCAF] %s : " fmt "\n", __FUNCTION__, ##arg);}\
		while(0)
			
#define cam_info(fmt, arg...)	\
		do {\
		printk(KERN_INFO "[S5K5CCAF] " fmt "\n",##arg);}\
		while(0)
			
#define cam_err(fmt, arg...)	\
		do {\
		printk(KERN_ERR "[S5K5CCAF] %s:%d:" fmt "\n",__FUNCTION__, __LINE__, ##arg);}\
		while(0)
			
#else
#define CAM_DEBUG(fmt, arg...)	
#define cam_info(fmt, arg...)
#define cam_err(fmt, arg...)
#endif
/*
#define	CAM_3M_RST	50
#define	CAM_3M_STBY	49
#define	CAM_VGA_RST	41
#define	CAM_VGA_STBY	42
*/
#define	CAM_FLASH_EN	137
#define	CAM_MOVIE_EN	142


#define	S5K5CCGX_PREVIEW_MODE		0
#define	S5K5CCGX_CAPTURE_MODE		1


#define	S5K5CCGX_CAMERA_MODE		0
#define	S5K5CCGX_MOVIE_MODE		1
#define	S5K5CCGX_MMS_MODE		2

#define	S5K5CCGX_3RD_PARTY_APP		0
#define S5K5CCGX_SAMSUNG_APP		1

#define	S5K5CCGX_FLASH_OFF		0
#define	S5K5CCGX_FLASH_AUTO		1
#define	S5K5CCGX_FLASH_ON		2
#define	S5K5CCGX_FLASH_TORCH		3


#define	S5K5CCGX_PREVIEW_VGA		0	/* 640x480 */
#define	S5K5CCGX_PREVIEW_D1		1	/* 720x480 */
#define	S5K5CCGX_PREVIEW_SVGA		2	/* 800x600 */
#define	S5K5CCGX_PREVIEW_XGA		3	/* 1024x768*/
#define	S5K5CCGX_PREVIEW_PVGA		4	/* 1280*720*/
#define	S5K5CCGX_PREVIEW_528x432	5	/* 528*432 */

#define S5K5CCGX_5_FPS			5
#define S5K5CCGX_8_FPS			8
#define S5K5CCGX_15_FPS			15
#define S5K5CCGX_30_FPS			30
#define S5K5CCGX_60_FPS			60



#if 0
#define PCAM_AUTO_TUNNING               0
#define PCAM_SDCARD_DETECT              1
#define PCAM_GET_INFO		        2
#define PCAM_FRAME_CONTROL              3
#define PCAM_AF_CONTROL                 4
#define PCAM_EFFECT_CONTROL             5
#define PCAM_WB_CONTROL                 6
#define PCAM_BR_CONTROL                 7
#define PCAM_ISO_CONTROL                8
#define PCAM_METERING_CONTROL           9
#define PCAM_SCENE_CONTROL		10
#define PCAM_AWB_AE_CONTROL		11
#define PCAM_CR_CONTROL                 12
#define PCAM_SA_CONTROL                 13
#define PCAM_SP_CONTROL                 14
#define PCAM_CPU_CONTROL                15
#define PCAM_DTP_CONTROL                16
#define PCAM_SET_PREVIEW_SIZE           17
#define PCAM_GET_MODULE_STATUS		18


#define PCAM_FRAME_AUTO			0
#define PCAM_FRAME_FIX_15		1
#define PCAM_FRAME_FIX_30		2
#define PCAM_BR_STEP_P_4                4
#define PCAM_BR_STEP_P_3                3
#define PCAM_BR_STEP_P_2                2
#define PCAM_BR_STEP_P_1                1
#define PCAM_BR_STEP_0                  0
#define PCAM_BR_STEP_M_1                255//-1
#define PCAM_BR_STEP_M_2                254//-2
#define PCAM_BR_STEP_M_3                253//-3
#define PCAM_BR_STEP_M_4                252//-4
#define PCAM_CR_STEP_P_2                4
#define PCAM_CR_STEP_P_1                3
#define PCAM_CR_STEP_0                  2
#define PCAM_CR_STEP_M_1                1
#define PCAM_CR_STEP_M_2                0

#define PCAM_SA_STEP_P_2                4
#define PCAM_SA_STEP_P_1                3
#define PCAM_SA_STEP_0                  2
#define PCAM_SA_STEP_M_1                1
#define PCAM_SA_STEP_M_2                0

#define PCAM_SP_STEP_P_2                4
#define PCAM_SP_STEP_P_1                3
#define PCAM_SP_STEP_0                  2
#define PCAM_SP_STEP_M_1                1
#define PCAM_SP_STEP_M_2                0

#define PCAM_ISO_AUTO			0
#define PCAM_ISO_50			1
#define PCAM_ISO_100			2
#define PCAM_ISO_200			3
#define PCAM_ISO_400			4
#endif

/* Saturation*/
#define S5K5CCGX_SATURATION_MINUS_2		0
#define S5K5CCGX_SATURATION_MINUS_1		1
#define S5K5CCGX_SATURATION_DEFAULT		2
#define S5K5CCGX_SATURATION_PLUS_1		3
#define S5K5CCGX_SATURATION_PLUS_2		4
/* Sharpness */
#define S5K5CCGX_SHARPNESS_MINUS_2		0
#define S5K5CCGX_SHARPNESS_MINUS_1		1
#define S5K5CCGX_SHARPNESS_DEFAULT		2
#define S5K5CCGX_SHARPNESS_PLUS_1		3
#define S5K5CCGX_SHARPNESS_PLUS_2		4

/* Contrast */
#define S5K5CCGX_CONTRAST_MINUS_2		0
#define S5K5CCGX_CONTRAST_MINUS_1		1
#define S5K5CCGX_CONTRAST_DEFAULT		2
#define S5K5CCGX_CONTRAST_PLUS_1		3
#define S5K5CCGX_CONTRAST_PLUS_2		4




#if 0
#define PCAM_AF_CHECK_STATUS		0
#define PCAM_AF_OFF			1
#define PCAM_AF_SET_NORMAL		2
#define PCAM_AF_SET_MACRO		3
#define PCAM_AF_DO			4
#define PCAM_AF_SET_MANUAL		5
#define PCAM_AF_ABORT			6
#define PCAM_AF_2ND_CHECK_STATUS	7
#endif

#define S5K5CCGX_AF_MODE_INFINITY		0
#define S5K5CCGX_AF_MODE_MACRO		1
#define S5K5CCGX_AF_MODE_AUTO		2


#define S5K5CCGX_AF_STOP		0
#define S5K5CCGX_AF_START		1
#define S5K5CCGX_AF_ABORT		2

#define S5K5CCGX_AF_CHECK_STATUS	0
#define S5K5CCGX_AF_OFF			1
#define S5K5CCGX_AF_SET_NORMAL		2
#define S5K5CCGX_AF_SET_MACRO		3
#define S5K5CCGX_AF_DO			4
#define S5K5CCGX_AF_SET_MANUAL		5
//#define S5K5CCGX_AF_ABORT		6
#define S5K5CCGX_AF_2ND_CHECK_STATUS	7

#define INNER_WINDOW_WIDTH_1024_768	230         //INNER_WINDOW_WIDTH_ON_PREVIEW       
#define INNER_WINDOW_HEIGHT_1024_768	230
#define OUTER_WINDOW_WIDTH_1024_768	512
#define OUTER_WINDOW_HEIGHT_1024_768	426
#define INNER_WINDOW_WIDTH_720P		287
#define INNER_WINDOW_HEIGHT_720P	215
#define OUTER_WINDOW_WIDTH_720P		640
#define OUTER_WINDOW_HEIGHT_720P	399         //OUTER_WINDOW_HEIGHT_ON_CAMREC

#if 0
#define PCAM_AF_LOWCONF			0 //Fail
#define PCAM_AF_PROGRESS		1
#define PCAM_AF_SUCCESS			2
#define PCAM_AF_LOWCONF_2		3 //Fail
#define PCAM_AF_LOWCONF_3               4 //Fail
#define PCAM_AF_LOWCONF_4               6 //Fail
#define PCAM_AF_LOWCONF_5               8 //Fail
#define PCAM_AF_CANCELED		9
#define PCAM_AF_TIMEOUT			10

#define PCAM_AF_2ND_PROGRESS		256 //(0x0100)
#define PCAM_AF_2ND_DONE		0 //(0x0000)



#define PCAM_AWB_AE_LOCK		0
#define PCAM_AWB_AE_UNLOCK		1


#define PCAM_CPU_CONSERVATIVE		0
#define PCAM_CPU_ONDEMAND		1
#define PCAM_CPU_PERFORMANCE		2		

//for PCAM_SET_PREVIEW_SIZE
#define PCAM_CAMERA_MODE		0
#define PCAM_CAMCORDER_MODE		1
#endif

#define S5K5CCGX_DTP_OFF		0
#define S5K5CCGX_DTP_ON			1


#define S5K5CCGX_MODE_PREVIEW		1
#define S5K5CCGX_MODE_CAPTURE		2	
#define S5K5CCGX_MODE_DTP		3

#define CAPTURE_FLASH			1
#define MOVIE_FLASH			0

/* level at or below which we need to enable flash when in auto mode */
#define LOW_LIGHT_LEVEL		0x3B	//0x4B is about 85lux // org 0x20 is about 40lux  


/* DTP */
#define DTP_OFF		0
#define DTP_ON		1
#define DTP_OFF_ACK	2
#define DTP_ON_ACK	3

struct s5k5ccaf_userset {
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
	int	scene;
	unsigned int zoom;
	unsigned int effect;	/* Color FX (AKA Color tone) */
	unsigned int scenemode;
	unsigned int detectmode;
	unsigned int antishake;
	unsigned int fps;
	unsigned int flash_mode;
	unsigned int flash_state;
	unsigned int exif_shutterspeed;
	
	unsigned int stabilize;	/* IS */

	unsigned int strobe;
	unsigned int jpeg_quality;
	unsigned int preview_size_idx;
	unsigned int preview_size_width;
	unsigned int preview_size_height;
	unsigned int capture_size;
	unsigned int thumbnail_size;
	unsigned int fps_mode;
	unsigned int ae_awb_lock;
};

struct s5k5ccaf_ctrl {
	const struct msm_camera_sensor_info *sensordata;
	struct s5k5ccaf_userset settings;
	
	int op_mode;
	int dtp_mode;
	int hd_enabled;
	int touchaf_enable;
	int started;
	int cam_mode;	// camera or camcorder
	int vtcall_mode;	
	int sensor_mode;
	int app_mode;
#if defined(CONFIG_TARGET_SERIES_P8LTE)
	int first_af_running;
#endif
};                                                               

#endif
