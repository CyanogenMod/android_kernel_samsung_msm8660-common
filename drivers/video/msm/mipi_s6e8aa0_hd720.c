/* Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_s6e8aa0_hd720.h"
#include "mipi_s6e8aa0_hd720_seq.h"
#include "mdp4_video_enhance.h"
#include "smart_dimming.h"

#include <linux/gpio.h>

#define LCDC_DEBUG

//#define MIPI_SINGLE_WRITE

#ifdef LCDC_DEBUG
#define DPRINT(x...)	printk("[Mipi_LCD] " x)
#else
#define DPRINT(x...)	
#endif

static char log_buffer[256] = {0,};
#define LOG_START()	do { log_buffer[0] = 0; } while(0)
#define LOG_CLEAR()	do { log_buffer[0] = 0; } while(0)
#define LOG_ADD(x...)	do { sprintf( log_buffer +strlen(log_buffer), x ); } while(0)
#define LOG_EXIST()	(strlen(log_buffer)>0)
#define LOG_FINISH(x...)	do { if( strlen(log_buffer) != 0 ) DPRINT( "%s\n", log_buffer); LOG_START(); } while(0)


#define MUTEX_LOCK(a) do { DPRINT("MUTEX LOCK : Line=%d, %x\n", __LINE__, a); mutex_lock(a);} while(0)
#define MUTEX_UNLOCK(a) do { DPRINT("MUTEX unlock : Line=%d, %x\n", __LINE__, a); mutex_unlock(a);} while(0)

#define GPIO_S6E8AA0_RESET (28)

#define LCD_OFF_GAMMA_VALUE (0)
#define DEFAULT_CANDELA_VALUE (160)   // 신뢰성 spec: celoxhd의 경우 160cd

#define ACL_OFF_GAMMA_VALUE (60)

#define NOT_AID (0)

enum lcd_id_value {
lcd_id_unknown = -1,
lcd_id_a1_m3_line = 1,
lcd_id_a1_sm2_line,
lcd_id_a2_sm2_line_1,
lcd_id_a2_sm2_line_2, // celoxhd
lcd_id_temp_7500k,
lcd_id_temp_8500k,
lcd_id_dcdc_STOD13A,
lcd_id_dcdc_STOD03A,
lcd_id_dcdc_F_G,
lcd_id_dcdc_H,
lcd_id_dcdc_K,
lcd_id_EVT1,
lcd_id_EVT2,
lcd_id_elvss_normal,
lcd_id_elvss_each,
};


struct lcd_state_type{
	boolean initialized;
	boolean display_on;
	boolean powered_up;
};

#define LCD_ID_BUFFER_SIZE (8)


struct lcd_setting {
	struct device			*dev;
	struct spi_device		*spi;
	unsigned int			power;
	unsigned int			gamma_mode;
	unsigned int			lcd_gamma; // lcd has this value
	unsigned int			stored_gamma; // driver has this value
	unsigned int			gamma_table_count;
	unsigned int			lcd_elvss; // lcd has this value
	unsigned int			stored_elvss; // driver has this value
	unsigned int			beforepower;
	unsigned int			ldi_enable;
	unsigned int 			enabled_acl; // lcd has this value
	unsigned int 			stored_acl; // driver has this value
	unsigned int 			lcd_acl; // lcd has this value
	struct mutex	lock;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	struct lcd_platform_data	*lcd_pd;
	struct early_suspend    early_suspend;
	struct lcd_state_type lcd_state;
	char	lcd_id_buffer[LCD_ID_BUFFER_SIZE];
	enum lcd_id_value	factory_id_line;
	enum lcd_id_value	factory_id_colorTemp;
	enum lcd_id_value	factory_id_elvssType;
	enum lcd_id_value factory_id_dcdc;
	enum lcd_id_value factory_id_EVT;
	struct dsi_cmd_desc_LCD *lcd_brightness_table;
	struct dsi_cmd_desc_LCD *lcd_acl_table;
	struct dsi_cmd_desc_LCD *lcd_elvss_table;
	int	eachElvss_value;
	int	eachElvss_vector;
	int	AID;
	int	cellPulse;

	boolean	isSmartDimming;
	boolean	isSmartDimming_loaded;
	struct str_smart_dim smart;
#ifdef MAPPING_TBL_AUTO_BRIGHTNESS
	unsigned int			auto_brightness;
#endif
};

static struct lcd_setting s6e8aa0_lcd;
//


void set_lcd_esd_ignore( boolean src );
static void lcd_set_brightness(struct msm_fb_data_type *mfd, int level);
static void lcd_gamma_apply( struct msm_fb_data_type *mfd, int srcGamma);
static void lcd_gamma_smartDimming_apply( struct msm_fb_data_type *mfd, int srcGamma);
static void lcd_apply_elvss(struct msm_fb_data_type *mfd, int srcElvss, struct lcd_setting *lcd);
static void lcd_apply_acl(struct msm_fb_data_type *mfd, unsigned int srcAcl, boolean nowOffNeedOn);
static void lcd_set_acl(struct msm_fb_data_type *mfd, struct lcd_setting *lcd);
static void lcd_set_elvss(struct msm_fb_data_type *mfd, struct lcd_setting *lcd);
static void lcd_gamma_ctl(struct msm_fb_data_type *mfd, struct lcd_setting *lcd);

static struct msm_panel_common_pdata *pdata;
static struct msm_fb_data_type *pMFD;

#undef  SmartDimming_16bitBugFix
#define SmartDimming_useSampleValue
#define SmartDimming_CANDELA_UPPER_LIMIT (299)
#define MTP_DATA_SIZE (24)
#define MTP_REGISTER	(0xD3)
unsigned char lcd_mtp_data[MTP_DATA_SIZE+16] = {0,};
#ifdef SmartDimming_useSampleValue
boolean	isFAIL_mtp_load = FALSE;
const unsigned char lcd_mtp_data_default [MTP_DATA_SIZE] = {
0x00, 0x00, 0x00 ,0x04, 0x02, 0x05, 
0x06, 0x05, 0x08, 0x03, 0x02, 0x03, 
0x02, 0x01, 0x01, 0xfa, 0xfe, 0xfa, 
0x00, 0x14, 0x00, 0x0e, 0x00, 0x0d,
};
#endif // SmartDimming_useSampleValue

static struct dsi_buf s6e8aa0_tx_buf;
static struct dsi_buf s6e8aa0_rx_buf;

static char ETC_COND_SET_1[3] = {0xF0, 0x5A, 0x5A};
static char ETC_COND_SET_2[3] = {0xF1, 0x5A, 0x5A};

static char ETC_COND_SET_2_1[4] = {0xF6, 0x00, 0x02, 0x00};
static char ETC_COND_SET_2_2[10] = {0xB6, 0x0C, 0x02, 0x03, 0x32, 0xFF, 0x44, 0x44, 0xC0, 0x00};
static char ETC_COND_SET_2_3[15] =         {0xD9, 0x14, 0x40, 0x0C, 0xCB, 0xCE, 0x6E, 0xC4, 0x07, 0x40, 0x41, 0xD0, 0x00, 0x60, 0x19};
static char ETC_COND_SET_2_3_AID[15] = {0xD9, 0x14, 0x40, 0x0C, 0xCB, 0xCE, 0x6E, 0xC4, 0x07, 0xC0, 0x41, 0xD0, 0x00, 0x60, 0x19};
#if 0 // 111005 Manual G. remove this.
static char ETC_COND_SET_2_4[6] = {0xE1, 0x10, 0x1C, 0x17, 0x08, 0x1D};
static char ETC_COND_SET_2_5[7] = {0xE2, 0xED, 0x07, 0xC3, 0x13, 0x0D, 0x03};
static char ETC_COND_SET_2_6[2] = {0xE3, 0x40};
static char ETC_COND_SET_2_7[8] = {0xE4, 0x00, 0x00, 0x14, 0x80, 0x00, 0x00, 0x00};
#endif
static char ETC_COND_SET_2_8[8] = {0xF4, 0xCF, 0x0A, 0x12, 0x10, 0x1E, 0x33, 0x02};

static char PANEL_COND_SET_7500K_500MBPS[39] = {
	0xF8 ,/*0x3C*/0x3d, 0x35, 0x00, 0x00, 0x00, 0x93, 0x00, 0x3C, 0x7D, 
	0x08, 0x27, 0x7D, 0x3F, 0x00, 0x00, 0x00, 0x20, 0x04, 0x08, 
	0x6E, 0x00, 0x00, 0x00, 0x02, 0x08, 0x08, 0x23, 0x23, 0xC0, 
	0xC8, 0x08, 0x48, 0xC1, 0x00, 0xC1, 0xFF, 0xFF, 0xC8
};
	
#if defined (CONFIG_USA_MODEL_SGH_I757)	 || defined(CONFIG_CAN_MODEL_SGH_I757M)
static char PANEL_COND_SET_7500K_480MBPS[39] = {
	0xF8 ,/*0x3C*/0x3d, 0x32, 0x00, 0x00, 0x00, 0x8D, 0x00, 0x39, 0x78, 
	0x08, 0x26, 0x78, 0x3C, 0x00, 0x00, 0x00, 0x20, 0x04, 0x08, 
	0x69, 0x00, 0x00, 0x00, 0x02, 0x07, 0x07, 0x21, 0x21, 0xC0, 
	0xC8, 0x08, 0x48, 0xC1, 0x00, 0xC1, 0xFF, 0xFF, 0xC8
};
#else
static char PANEL_COND_SET_7500K_480MBPS[39] = {
	0xF8 ,/*0x3C*/0x3d, 0x35, 0x00, 0x00, 0x00, 0x8D, 0x00, 0x3A, 0x78, 
	0x08, 0x26, 0x78, 0x3C, 0x00, 0x00, 0x00, 0x20, 0x04, 0x08, 
	0x69, 0x00, 0x00, 0x00, 0x02, 0x07, 0x07, 0x21, 0x21, 0xC0, 
	0xC8, 0x08, 0x48, 0xC1, 0x00, 0xC1, 0xFF, 0xFF, 0xC8
};
#endif

static char PANEL_COND_SET_8500K_500MBPS[39] = {
	0xF8 ,/*0x3C*/0x3d, 0x35, 0x00, 0x00, 0x00, 0x8D, 0x00, 0x4C, 0x6E, 
	0x10, 0x27, 0x7D, 0x3F, 0x10, 0x00, 0x00, 0x20, 0x04, 0x08, 
	0x6E, 0x00, 0x00, 0x00, 0x02, 0x08, 0x08, 0x23, 0x23, 0xC0, 
	0xC8, 0x08, 0x48, 0xC1, 0x00, 0xC1, 0xFF, 0xFF, 0xC8
};

static char PANEL_COND_SET_7500K_500MBPS_AID1[39] = {
	0xf8, 0x3D, 0x35, 0x00, 0x00, 0x00, 0x93, 0x00, 0x3c, 0x7d, 
	0x08, 0x27, 0x7d, 0x3f, 0x00, 0x00, 0x00, 0x20, 0x60, 0x08, 
	0x6e, 0x00, 0x00, 0x00, 0x02, 0x08, 0x08, 0x23, 0x23, 0xc0, 
	0xc8, 0x08, 0x48, 0xc1, 0x00, 0xc1, 0xff, 0xff, 0xc8
};

static char PANEL_COND_SET_7500K_500MBPS_AID2[39] = {
	0xf8, 0x3D, 0x35, 0x00, 0x00, 0x00, 0x93, 0x00, 0x3c, 0x7d, 
	0x08, 0x27, 0x7d, 0x3f, 0x00, 0x00, 0x00, 0x20, 0x80, 0x08, 
	0x6e, 0x00, 0x00, 0x00, 0x02, 0x08, 0x08, 0x23, 0x23, 0xc0, 
	0xc8, 0x08, 0x48, 0xc1, 0x00, 0xc1, 0xff, 0xff, 0xc8
};

static char PANEL_COND_SET_7500K_500MBPS_AID3[39] = {
	0xf8, 0x3D, 0x35, 0x00, 0x00, 0x00, 0x93, 0x00, 0x3c, 0x7d, 
	0x08, 0x27, 0x7d, 0x3f, 0x00, 0x00, 0x00, 0x20, 0xA0, 0x08, 
	0x6e, 0x00, 0x00, 0x00, 0x02, 0x08, 0x08, 0x23, 0x23, 0xc0, 
	0xc8, 0x08, 0x48, 0xc1, 0x00, 0xc1, 0xff, 0xff, 0xc8
};

static char DISPLAY_COND_SET[4] = {0xF2, 0x80, 0x03, 0x0D};

static char GAMMA_COND_SET[] = // 160 cd
{ 0xFA, 0x01, 
0x0F, 	0x0F, 	0x0F, 	
0xCA, 	0x6E, 	0xDE, 	
0xC2, 	0xB6, 	0xC4, 	
0xD2, 	0xCD, 	0xD2, 	
0xAE, 	0xAD, 	0xAD, 	
0xC2, 	0xC5, 	0xC0, 	
0x0, 	0x6E, 	0x0, 	0x62, 	0x0, 	0x98};


static char exit_sleep[2] = {0x11, 0x00};
static char display_on[2] = {0x29, 0x00};
static char display_off[2] = {0x28, 0x00};
static char enter_sleep[2] = {0x10, 0x00};

#if 0 // warning : defined but not used
static char tear_off[2] = {0x34, 0x00};
static char tear_on [2] = {0x35, 0x00};
static char ltps_update_cmd[2] = {0xF7, 0x02};
static char all_pixel_off_cmd[2] = {0x22, 0x00};
static char all_pixel_on_cmd[2] = {0x23, 0x00};
static char reset_cmd[2] = {0x01, 0x00};
#endif 

static char gamma_update_cmd[2] = {0xF7, /*0x01*/0x03};
static char acl_off_cmd[2] = {0xC0, 0x00};
static char acl_on_cmd[2] = {0xC0, 0x01};

static struct dsi_cmd_desc s6e8aa0_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(enter_sleep), enter_sleep}
};

static struct dsi_cmd_desc s6e8aa0_gamma_update_cmd = {
	DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(gamma_update_cmd), gamma_update_cmd};

static struct dsi_cmd_desc s6e8aa0_acl_off_cmd = {
	DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(acl_off_cmd), acl_off_cmd};
static struct dsi_cmd_desc s6e8aa0_acl_on_cmd = {
	DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(acl_on_cmd), acl_on_cmd};


static struct dsi_cmd_desc s6e8aa0_display_on_cmds[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ETC_COND_SET_1), ETC_COND_SET_1},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ETC_COND_SET_2), ETC_COND_SET_2},
    {DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(PANEL_COND_SET_7500K_500MBPS), PANEL_COND_SET_7500K_500MBPS},    
    //{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(ltps_update_cmd), ltps_update_cmd},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DISPLAY_COND_SET), DISPLAY_COND_SET},    
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_COND_SET), GAMMA_COND_SET},    
    {DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(gamma_update_cmd), gamma_update_cmd},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ETC_COND_SET_2_1), ETC_COND_SET_2_1},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ETC_COND_SET_2_2), ETC_COND_SET_2_2},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ETC_COND_SET_2_3), ETC_COND_SET_2_3},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ETC_COND_SET_2_8), ETC_COND_SET_2_8},    
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ELVSS_COND_SET_160), ELVSS_COND_SET_160},    
    {DTYPE_DCS_LWRITE, 1, 0, 0, 120, sizeof(ACL_COND_SET_40), ACL_COND_SET_40},    
    {DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_on), display_on},
};

static struct dsi_cmd_desc s6e8aa0_display_on_before_read_id[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ETC_COND_SET_1), ETC_COND_SET_1},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ETC_COND_SET_2), ETC_COND_SET_2},
    {DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},
};

static struct dsi_cmd_desc s6e8aa0_display_on_before_gamma_cmds[] = {
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(PANEL_COND_SET_7500K_500MBPS), PANEL_COND_SET_7500K_500MBPS},    
    //{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(ltps_update_cmd), ltps_update_cmd},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(DISPLAY_COND_SET), DISPLAY_COND_SET},    
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ETC_COND_SET_2_1), ETC_COND_SET_2_1},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ETC_COND_SET_2_2), ETC_COND_SET_2_2},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ETC_COND_SET_2_3), ETC_COND_SET_2_3},
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ETC_COND_SET_2_8), ETC_COND_SET_2_8},    
    {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ELVSS_COND_SET_160), ELVSS_COND_SET_160},    
    {DTYPE_DCS_LWRITE, 1, 0, 0, 120, sizeof(ACL_COND_SET_40), ACL_COND_SET_40},    
};

static struct dsi_cmd_desc s6e8aa0_display_on_after_gamma_cmds[] = {
	// warning : Don't insert delay time. BrightnessChange in this cmds cannot be apply.
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(gamma_update_cmd), gamma_update_cmd},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_on), display_on},
};

static inline boolean isPANEL_COND_SET( char* src )
{
	if( src == PANEL_COND_SET_7500K_500MBPS 
		|| src == PANEL_COND_SET_7500K_500MBPS_AID1
		|| src == PANEL_COND_SET_7500K_500MBPS_AID2
		|| src == PANEL_COND_SET_7500K_500MBPS_AID3
		|| src == PANEL_COND_SET_7500K_480MBPS
		|| src == PANEL_COND_SET_7500K_500MBPS
		|| src == PANEL_COND_SET_8500K_500MBPS )
	return TRUE;
	else
	return FALSE;
}


static inline boolean is_ETC_COND_0xD9_SET( char* src )
{
	if( src == ETC_COND_SET_2_3 
		|| src == ETC_COND_SET_2_3_AID )
	return TRUE;
	else
	return FALSE;
}

char* get_s6e8aa0_id_buffer( void )
{
	return s6e8aa0_lcd.lcd_id_buffer;
}

static void update_LCD_SEQ_by_id(struct lcd_setting *destLCD)
{
	int	PANEL_COND_SET_dlen_NEW = 0;
	char* PANEL_COND_SET_NEW = NULL;
	int	PANEL_ETC_COND_0xD9_SET_dlen_NEW = 0;
	char* PANEL_ETC_COND_0xD9_SET_NEW = NULL;
	int updateCnt = 0;
	int i;
	unsigned char* pNew_GAMMA_SmartDimming_VALUE_SET_300cd;

	LOG_START();
	LOG_ADD( "%s :", __func__ );
	
	updateCnt = 0;
	switch( destLCD->factory_id_colorTemp )
	{
		case lcd_id_temp_7500k:
		case lcd_id_unknown:
		default:	// if Error-case, Not Change 
		destLCD->lcd_brightness_table = lcd_brightness_7500k_table;


		switch( destLCD->AID )
		{
			case 1:
				PANEL_COND_SET_dlen_NEW = sizeof(PANEL_COND_SET_7500K_500MBPS_AID1);
				PANEL_COND_SET_NEW = PANEL_COND_SET_7500K_500MBPS_AID1;
				LOG_ADD( " 500Mbps AID1" );
			break;
			case 2:
				PANEL_COND_SET_dlen_NEW = sizeof(PANEL_COND_SET_7500K_500MBPS_AID2);
				PANEL_COND_SET_NEW = PANEL_COND_SET_7500K_500MBPS_AID2;
				LOG_ADD( " 500Mbps AID2" );
			break;
			case 3:
				PANEL_COND_SET_dlen_NEW = sizeof(PANEL_COND_SET_7500K_500MBPS_AID3);
				PANEL_COND_SET_NEW = PANEL_COND_SET_7500K_500MBPS_AID3;
				LOG_ADD( " 500Mbps AID3" );
			break;
			default:
				if( MIPI_MBPS == 500 )
				{
					PANEL_COND_SET_dlen_NEW = sizeof(PANEL_COND_SET_7500K_500MBPS);
					PANEL_COND_SET_NEW = PANEL_COND_SET_7500K_500MBPS;
					LOG_ADD( " 500Mbps" );
				} else if( MIPI_MBPS == 480 ) {
					PANEL_COND_SET_dlen_NEW = sizeof(PANEL_COND_SET_7500K_480MBPS);
					PANEL_COND_SET_NEW = PANEL_COND_SET_7500K_480MBPS;
					LOG_ADD( " 480Mbps" );
				} else {
					PANEL_COND_SET_dlen_NEW = sizeof(PANEL_COND_SET_7500K_500MBPS);
					PANEL_COND_SET_NEW = PANEL_COND_SET_7500K_500MBPS;
					LOG_ADD( " 500(%d)Mbps", MIPI_MBPS );
				}			
			break;
		}
		
		LOG_ADD( " 7500K" );
		updateCnt++;
		break;

		case lcd_id_temp_8500k:
		destLCD->lcd_brightness_table = lcd_brightness_8500k_table;
		updateCnt++;
		PANEL_COND_SET_dlen_NEW = sizeof(PANEL_COND_SET_8500K_500MBPS);
		PANEL_COND_SET_NEW = PANEL_COND_SET_8500K_500MBPS;
		LOG_ADD( " 8500K" );
		
		LOG_ADD( " (%dMbps)", MIPI_MBPS );
		break;
	}

	if( destLCD->AID == NOT_AID )
	{
		PANEL_ETC_COND_0xD9_SET_dlen_NEW = sizeof(ETC_COND_SET_2_3);
		PANEL_ETC_COND_0xD9_SET_NEW = ETC_COND_SET_2_3;
		LOG_ADD( " f9NOT" );
	}
	else
	{
		PANEL_ETC_COND_0xD9_SET_dlen_NEW = sizeof(ETC_COND_SET_2_3_AID);
		PANEL_ETC_COND_0xD9_SET_NEW = ETC_COND_SET_2_3_AID;
		LOG_ADD( " f9AID" );
	}

	for( i = 0; i < ARRAY_SIZE( s6e8aa0_display_on_cmds ); i++ )
	{
		if( isPANEL_COND_SET(s6e8aa0_display_on_cmds[i].payload))
		{
			s6e8aa0_display_on_cmds[i].dlen	= PANEL_COND_SET_dlen_NEW;
			s6e8aa0_display_on_cmds[i].payload	= PANEL_COND_SET_NEW;
			updateCnt++;
			LOG_ADD( " pc%d", i );
		}

		if( is_ETC_COND_0xD9_SET(s6e8aa0_display_on_cmds[i].payload))
		{
			s6e8aa0_display_on_cmds[i].dlen	= PANEL_ETC_COND_0xD9_SET_dlen_NEW;
			s6e8aa0_display_on_cmds[i].payload	= PANEL_ETC_COND_0xD9_SET_NEW;
			updateCnt++;
			LOG_ADD( " fc%d", i );
		}

	}

	for( i = 0; i < ARRAY_SIZE( s6e8aa0_display_on_before_gamma_cmds ); i++ )
	{
		if( isPANEL_COND_SET(s6e8aa0_display_on_before_gamma_cmds[i].payload))
		{
			s6e8aa0_display_on_before_gamma_cmds[i].dlen	= PANEL_COND_SET_dlen_NEW;
			s6e8aa0_display_on_before_gamma_cmds[i].payload	= PANEL_COND_SET_NEW;
			updateCnt++;
			LOG_ADD( " pb%d", i );
		}

		if( is_ETC_COND_0xD9_SET(s6e8aa0_display_on_before_gamma_cmds[i].payload))
		{
			s6e8aa0_display_on_before_gamma_cmds[i].dlen	= PANEL_ETC_COND_0xD9_SET_dlen_NEW;
			s6e8aa0_display_on_before_gamma_cmds[i].payload	= PANEL_ETC_COND_0xD9_SET_NEW;
			updateCnt++;
			LOG_ADD( " fb%d", i );
		}
	}

	// smartDimming data initialiazing
	if( destLCD->isSmartDimming && s6e8aa0_lcd.isSmartDimming_loaded )
	{
		pNew_GAMMA_SmartDimming_VALUE_SET_300cd = NULL;
		if( destLCD->factory_id_line == lcd_id_a2_sm2_line_1 || destLCD->factory_id_line == lcd_id_a2_sm2_line_2)
		{
		switch( destLCD->AID )
		{
			case 1:
					pNew_GAMMA_SmartDimming_VALUE_SET_300cd = (unsigned char*) GAMMA_SmartDimming_VALUE_SET_A2SM2_AID1_300cd;
					LOG_ADD( " A2iAID1" );
				break;
				case 2:
					pNew_GAMMA_SmartDimming_VALUE_SET_300cd = (unsigned char*) GAMMA_SmartDimming_VALUE_SET_A2SM2_AID2_300cd;
					LOG_ADD( " A2iAID2" );
				break;
				case 3:
					pNew_GAMMA_SmartDimming_VALUE_SET_300cd = (unsigned char*) GAMMA_SmartDimming_VALUE_SET_A2SM2_AID3_300cd;
					LOG_ADD( " A2iAID3" );
				break;
				default:
                {
                  if(destLCD->factory_id_line == lcd_id_a2_sm2_line_1)
                  {
					pNew_GAMMA_SmartDimming_VALUE_SET_300cd = (unsigned char*) GAMMA_SmartDimming_VALUE_SET_A2SM2_300cd;
                    LOG_ADD( " A2i line_1" );
                  }
                  else
                  {
                    pNew_GAMMA_SmartDimming_VALUE_SET_300cd = (unsigned char*) GAMMA_SmartDimming_VALUE_SET_A2SM2_300cd_line2;
                    LOG_ADD( " A2i line_2" );
                  }
                }
				break;
			}
		}
		else
		{ // destLCD->factory_id_line = (unsigned char*)= (unsigned char*) lcd_id_a1_sm2_line and ETC
		switch( destLCD->AID )
		{
			case 1:
					pNew_GAMMA_SmartDimming_VALUE_SET_300cd = (unsigned char*) GAMMA_SmartDimming_VALUE_SET_A1SM2_AID1_300cd;
				LOG_ADD( " iAID1" );
			break;
			case 2:
					pNew_GAMMA_SmartDimming_VALUE_SET_300cd = (unsigned char*) GAMMA_SmartDimming_VALUE_SET_A1SM2_AID2_300cd;
				LOG_ADD( " iAID2" );
			break;
			case 3:
					pNew_GAMMA_SmartDimming_VALUE_SET_300cd = (unsigned char*) GAMMA_SmartDimming_VALUE_SET_A1SM2_AID3_300cd;
				LOG_ADD( " iAID3" );
			break;
			default:
					pNew_GAMMA_SmartDimming_VALUE_SET_300cd = (unsigned char*) GAMMA_SmartDimming_VALUE_SET_A1SM2_300cd;
				LOG_ADD( " i" );
			break;
		}
		}
			
		init_table_info( &(s6e8aa0_lcd.smart), pNew_GAMMA_SmartDimming_VALUE_SET_300cd );
		calc_voltage_table(&(s6e8aa0_lcd.smart), lcd_mtp_data);
	}


	switch( s6e8aa0_lcd.factory_id_dcdc )
	{
		case lcd_id_dcdc_STOD03A:
			s6e8aa0_lcd.lcd_elvss_table = lcd_elvss_dcdc03_table;
			LOG_ADD( " ed03" );
		break;
		case lcd_id_dcdc_STOD13A:
			s6e8aa0_lcd.lcd_elvss_table = lcd_elvss_dcdc13_table;
			LOG_ADD( " ed13" );
		break;

		case lcd_id_dcdc_F_G:
		case lcd_id_dcdc_H:
		case lcd_id_dcdc_K:
			s6e8aa0_lcd.lcd_elvss_table = lcd_elvss_dcdc03_table;
			LOG_ADD( " ed?03" );
		default:
		break;
	}

	LOG_FINISH();
}


void update_id( char* srcBuffer, struct lcd_setting *destLCD )
{
	unsigned char checkValue;
	boolean elvss_Minus10;
	int	i;

	LOG_START();
	LOG_ADD( "Update LCD ID:" );
	
	// make read-data valid
	if( srcBuffer[0] == 0x00 )
	{
		for( i = 0; i < LCD_ID_BUFFER_SIZE-2; i ++ ) srcBuffer[i] = srcBuffer[i+2];
	}
	destLCD->isSmartDimming = FALSE;
	
	checkValue = srcBuffer[0];
	switch( checkValue )
	{
		case 0xA1:
			destLCD->factory_id_line = lcd_id_a1_m3_line;
#if defined (CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
                        ETC_COND_SET_2_8[5] = 0x19;
#endif
			LOG_ADD( " A1M3-Line" );
		break;
		case 0x12:
			destLCD->factory_id_line = lcd_id_a1_sm2_line;
			destLCD->isSmartDimming = TRUE;
			LOG_ADD( " A1SM2-Line" );
		break;
		case 0xA2:
			destLCD->factory_id_line = lcd_id_a2_sm2_line_1;
			destLCD->isSmartDimming = TRUE;
			LOG_ADD( " A2SM2-Line" );
		break;
		default :
			destLCD->factory_id_line = lcd_id_unknown;
			LOG_ADD( " A?-Line" );
		break;				
	} // end switch
	LOG_ADD( "(%X)", checkValue );

	destLCD->factory_id_colorTemp = lcd_id_temp_7500k;
	LOG_ADD( " 7500K" );
#if 0 	
	checkValue = srcBuffer[1];
	switch( checkValue )
	{
		case 0x25 ... 0x6F:
			destLCD->factory_id_colorTemp = lcd_id_temp_7500k;
			LOG_ADD( " 7500K" );
		break;
		case 0x23:
		case 0x80:
			destLCD->factory_id_colorTemp = lcd_id_temp_8500k;
			LOG_ADD( " 8500K" );
		break;
		default :
			destLCD->factory_id_colorTemp = lcd_id_unknown;
			LOG_ADD( " 7500?K" );
		break;				
	} // end switch
	LOG_ADD( "(%X)", checkValue );
#endif 

	elvss_Minus10 = FALSE;
//	elvss_Minus10 = (srcBuffer[1] == 0x4E);

	checkValue = (srcBuffer[1] & 0xE0) >>5;
	switch( checkValue )
	{
		case 2:
			destLCD->factory_id_dcdc = lcd_id_dcdc_STOD13A;
			LOG_ADD( " STOD13A" );
		break;
		case 3:
			destLCD->factory_id_dcdc = lcd_id_dcdc_STOD03A;
			LOG_ADD( " STOD03A" );
		break;
		case 4:
			destLCD->factory_id_dcdc = lcd_id_dcdc_F_G;
			LOG_ADD( " F_G" );
		break;
		case 5: // A2 line_1 OPM Rev.K
			destLCD->factory_id_dcdc = lcd_id_dcdc_H;
			LOG_ADD( " H~J" );
		break;
		case 6: // A2 line_2. OPM Rev.K
			destLCD->factory_id_line = lcd_id_a2_sm2_line_2; //factory_id_line overwrited  here.
			destLCD->factory_id_dcdc = lcd_id_dcdc_K; 
			LOG_ADD( " K" );
		break;
		default:
			destLCD->factory_id_dcdc = lcd_id_unknown;
			LOG_ADD( " dcdc?" );
		break;
	}
	LOG_ADD( "(%X)", checkValue );

	checkValue = (srcBuffer[1] & 0x3);
	switch( checkValue )
	{
		case 1:
			destLCD->factory_id_EVT = lcd_id_EVT1;
			LOG_ADD( " EVT1" );
		break;
		case 2:
			destLCD->factory_id_EVT = lcd_id_EVT2;
			LOG_ADD( " EVT2" );
		break;
		default:
			destLCD->factory_id_EVT = lcd_id_unknown;
			LOG_ADD( " EVT?" );
		break;
	}
	LOG_ADD( "(%X)", checkValue );

	LOG_ADD( " id3=%X", srcBuffer[2]);
	checkValue = (srcBuffer[2] & 0x80) >> 7;
	if( checkValue )
	{
		checkValue = srcBuffer[2];

		destLCD->AID = (checkValue & 0x60) >> 5;
		LOG_ADD( " AID=%d", destLCD->AID );

		destLCD->cellPulse = (checkValue & 0x1F);
		LOG_ADD( " cp=%d", destLCD->cellPulse );

		destLCD->factory_id_elvssType = lcd_id_elvss_each;
		destLCD->eachElvss_value = (checkValue & 0x1F);

		if( elvss_Minus10 )	destLCD->eachElvss_vector = -10;
		else destLCD->eachElvss_vector = 0;
		LOG_ADD( " Elvss:%x_%d", destLCD->eachElvss_value, destLCD->eachElvss_vector );
	}
	else
	{
		checkValue = srcBuffer[2];
		if( checkValue != 0 )
		{
			destLCD->factory_id_elvssType = lcd_id_elvss_each;
			destLCD->eachElvss_value = (checkValue & 0x1F);

			if( elvss_Minus10 )	destLCD->eachElvss_vector = -10;
			else destLCD->eachElvss_vector = 0;
			LOG_ADD( " Elvss:%x_%d", destLCD->eachElvss_value, destLCD->eachElvss_vector );
		}
		else
		{
			destLCD->factory_id_elvssType = lcd_id_elvss_normal;
			LOG_ADD( " DElvss" );
		} // end switch
	}

	LOG_FINISH();
}



void read_reg( char srcReg, int srcLength, char* destBuffer, const int isUseMutex )
{
	const int	one_read_size = 4;
	const int	loop_limit = 16;

	static char read_reg[2] = {0xFF, 0x00}; // first byte = read-register
	static struct dsi_cmd_desc s6e8aa0_read_reg_cmd = 
	{ DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(read_reg), read_reg};

	static char packet_size[] = { 0x04, 0 }; // first byte is size of Register
	static struct dsi_cmd_desc s6e8aa0_packet_size_cmd = 
	{ DTYPE_MAX_PKTSIZE, 1, 0, 0, 0, sizeof(packet_size), packet_size};

	static char reg_read_pos[] = { 0xB0, 0x00 }; // second byte is Read-position
	static struct dsi_cmd_desc s6e8aa0_read_pos_cmd = 
	{ DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(reg_read_pos), reg_read_pos};

	int read_pos;
	int readed_size;
	int show_cnt;

	int i,j;

	read_reg[0] = srcReg;

	LOG_START();
	LOG_ADD( "read_reg : %X[%d] : ", srcReg, srcLength );

	read_pos = 0;
	readed_size = 0;
	if( isUseMutex ) mutex_lock(&(s6e8aa0_lcd.lock));

//	MIPI_OUTP(MIPI_DSI_BASE + 0x38, 0x14000000); // lp

	packet_size[0] = (char) srcLength;
	mipi_dsi_cmds_tx(pMFD, &s6e8aa0_tx_buf, &(s6e8aa0_packet_size_cmd),	1);

	show_cnt = 0;
	for( j = 0; j < loop_limit; j ++ )
	{
		reg_read_pos[1] = read_pos;
		if( mipi_dsi_cmds_tx(pMFD, &s6e8aa0_tx_buf, &(s6e8aa0_read_pos_cmd), 1) < 1 ) {
			LOG_ADD( "Tx command FAILED" );
			break;
		}

		mipi_dsi_buf_init(&s6e8aa0_rx_buf);
		readed_size = mipi_dsi_cmds_rx(pMFD, &s6e8aa0_tx_buf, &s6e8aa0_rx_buf, &s6e8aa0_read_reg_cmd, one_read_size);
		for( i=0; i < readed_size; i++, show_cnt++) 
		{
			LOG_ADD( "%02x ", s6e8aa0_rx_buf.data[i] );
			if( destBuffer != NULL && show_cnt < srcLength ) {
				destBuffer[show_cnt] = s6e8aa0_rx_buf.data[i];
			} // end if
		} // end for
		LOG_ADD( "." );
		read_pos += readed_size;
		if( read_pos > srcLength ) break;
	} // end for
//	MIPI_OUTP(MIPI_DSI_BASE + 0x38, 0x10000000); // hs
	if( isUseMutex ) mutex_unlock(&(s6e8aa0_lcd.lock));
	if( j == loop_limit ) LOG_ADD( "Overrun" );

	LOG_FINISH();
	
}


static void lcd_read_mtp( char* pDest, const int isUseMutex )
{
	int i, j;
	char pBuffer[256];
	char temp_lcd_mtp_data[MTP_DATA_SIZE+16] = {0,};

	j = 0;
	read_reg( MTP_REGISTER, 8, temp_lcd_mtp_data, FALSE );
	for( i = 2; i<8; i++ )	pDest[j++] = temp_lcd_mtp_data[i];
	read_reg( MTP_REGISTER, 16, temp_lcd_mtp_data, FALSE );
	for( i = 0; i<16; i++ )	pDest[j++] = temp_lcd_mtp_data[i];
	read_reg( MTP_REGISTER, 24, temp_lcd_mtp_data, FALSE );
	for( i = 8; i<10; i++ )	pDest[j++] = temp_lcd_mtp_data[i];
	{
		sprintf( pBuffer, "read_reg MTP :" );
		for( i = 0; i < MTP_DATA_SIZE; i++ )	sprintf( pBuffer+strlen(pBuffer), " %02x", pDest[i] );
		DPRINT( "%s\n", pBuffer );
	}

}


static int lcd_on_seq(struct msm_fb_data_type *mfd)
{
	int	txCnt;
	int	txAmount;
	int	ret = 0;
	volatile int	isFailed_TX = FALSE;
	unsigned int	acl_level;

	int i;
	char pBuffer[256];
	
#if 1
	// in result of test, gamma update can update before gammaTableCommand of LCD-ON-sequence.
	// if update gamma after gammaTableCommand cannot be update to lcd
	// if update gamma after displayOnCommand cannot be update lcd.
	// so gammaTableCommand is moved to ahead of displayOnCommand.

	mutex_lock(&(s6e8aa0_lcd.lock));

	if( s6e8aa0_lcd.lcd_state.initialized )	{
		DPRINT("%s : Already ON -> return 1\n", __func__ );
		ret = 1;
	}
	else {
		DPRINT("%s +\n", __func__);

		txAmount = ARRAY_SIZE(s6e8aa0_display_on_before_read_id);
		txCnt = mipi_dsi_cmds_tx(mfd, &s6e8aa0_tx_buf, s6e8aa0_display_on_before_read_id,
				txAmount );    
		if( txAmount != txCnt )	
		{
			isFailed_TX = TRUE;
			goto failed_tx_lcd_on_seq;
		}

#if 1 
		if( !s6e8aa0_lcd.isSmartDimming_loaded )
		{
			read_reg( MTP_REGISTER, MTP_DATA_SIZE, lcd_mtp_data, FALSE );
#if 0 // kyNam_110915_ do not check MTP-Fail-check			
			for( i=0, j=0; i < MTP_DATA_SIZE; i++ ) if(lcd_mtp_data[i]==0) j++;
			if( j > MTP_DATA_SIZE/2 )
			{
				DPRINT( "MTP Load FAIL\n" );
				s6e8aa0_lcd.isSmartDimming_loaded = FALSE;
				isFAIL_mtp_load = TRUE;

#ifdef SmartDimming_useSampleValue
				for( i = 0; i < MTP_DATA_SIZE; i++ )	 lcd_mtp_data[i] = lcd_mtp_data_default[i];
				s6e8aa0_lcd.isSmartDimming_loaded = TRUE;
#endif 				
			} 
			else 
#endif 			
			{
				sprintf( pBuffer, "mtp :" );
				for( i = 0; i < MTP_DATA_SIZE; i++ )	sprintf( pBuffer+strlen(pBuffer), " %02x", lcd_mtp_data[i] );
				DPRINT( "%s\n", pBuffer );

				s6e8aa0_lcd.isSmartDimming_loaded = TRUE;
			}
		}

		if( s6e8aa0_lcd.factory_id_line == lcd_id_unknown )
		{
			read_reg( 0xD1, LCD_ID_BUFFER_SIZE, s6e8aa0_lcd.lcd_id_buffer, FALSE );
			update_id( s6e8aa0_lcd.lcd_id_buffer, &s6e8aa0_lcd );
			update_LCD_SEQ_by_id( &s6e8aa0_lcd );
		}
#else
		read_reg( 0xD1, LCD_ID_BUFFER_SIZE, s6e8aa0_lcd.lcd_id_buffer, FALSE );

		lcd_read_mtp( lcd_mtp_data, FALSE );
		s6e8aa0_lcd.isSmartDimming_loaded = TRUE;

		update_id( s6e8aa0_lcd.lcd_id_buffer, &s6e8aa0_lcd );

		update_LCD_SEQ_by_id( &s6e8aa0_lcd );
#endif 
		txAmount = ARRAY_SIZE(s6e8aa0_display_on_before_gamma_cmds);
		txCnt = mipi_dsi_cmds_tx(mfd, &s6e8aa0_tx_buf, s6e8aa0_display_on_before_gamma_cmds,
				txAmount );    

		if( txAmount != txCnt )	
		{
			isFailed_TX = TRUE;
			goto failed_tx_lcd_on_seq;
		}

		// update stored-gamma value to gammaTable

		LOG_START();
		if( s6e8aa0_lcd.isSmartDimming )
		{
			lcd_gamma_smartDimming_apply( mfd, s6e8aa0_lcd.stored_gamma );
		} 
		else {
			lcd_gamma_apply( mfd, s6e8aa0_lcd.stored_gamma );
		}
		s6e8aa0_lcd.lcd_gamma = s6e8aa0_lcd.stored_gamma;
		
		// set elvss
		lcd_apply_elvss(mfd, s6e8aa0_lcd.stored_elvss, &s6e8aa0_lcd);
		s6e8aa0_lcd.lcd_elvss = s6e8aa0_lcd.stored_elvss;

		// set acl 
		if( s6e8aa0_lcd.enabled_acl ) acl_level	= s6e8aa0_lcd.stored_acl;
		else acl_level = 0;
		lcd_apply_acl( mfd, acl_level, TRUE );
		s6e8aa0_lcd.lcd_acl = acl_level; 
		LOG_FINISH();

		mipi_dsi_cmds_tx(mfd, &s6e8aa0_tx_buf, s6e8aa0_display_on_after_gamma_cmds,
				ARRAY_SIZE(s6e8aa0_display_on_after_gamma_cmds));    

		s6e8aa0_lcd.lcd_state.initialized = TRUE;
		s6e8aa0_lcd.lcd_state.display_on = TRUE;
		s6e8aa0_lcd.lcd_state.powered_up = TRUE;

		DPRINT("%s - : gamma=%d, ACl=%d, Elvss=%d\n", __func__, s6e8aa0_lcd.lcd_gamma, s6e8aa0_lcd.lcd_acl, s6e8aa0_lcd.lcd_elvss );

failed_tx_lcd_on_seq:
		if( isFailed_TX ) {
			s6e8aa0_lcd.factory_id_line = lcd_id_unknown;
			DPRINT("%s : tx FAILED\n", __func__ );
		}
	}
	mutex_unlock(&(s6e8aa0_lcd.lock));
#else // regal process
	mutex_lock(&(s6e8aa0_lcd.lock));
    // lcd_panel_init
	mipi_dsi_cmds_tx(mfd, &s6e8aa0_tx_buf, s6e8aa0_display_on_cmds,
			ARRAY_SIZE(s6e8aa0_display_on_cmds));    
	s6e8aa0_lcd.lcd_state.initialized = TRUE;
	s6e8aa0_lcd.lcd_state.display_on = TRUE;
	s6e8aa0_lcd.lcd_state.powered_up = TRUE;
	mutex_unlock(&(s6e8aa0_lcd.lock));
#endif 

	return ret;
}

static int lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	DPRINT("%s\n", __func__);

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;
	pMFD = mfd;

	lcd_on_seq( mfd );
//	readCmd(mfd, DTYPE_GEN_READ, 0x05, 0x00, 8, 1 );
//	readCmd(mfd, DTYPE_GEN_READ, 0x0A, 0x00, 8, 1 );
//	readCmd(mfd, DTYPE_GEN_READ, 0x0D, 0x00, 8, 1 );
//	readCmd(mfd, DTYPE_GEN_READ, 0x0E, 0x00, 8, 1 );
	return 0;
}


static int lcd_off_seq(struct msm_fb_data_type *mfd)
{
	if( !s6e8aa0_lcd.lcd_state.initialized )
	{
		DPRINT("%s : Already OFF -> return 1\n", __func__ );
		return 1;
	}

	DPRINT("%s\n", __func__);

	mutex_lock(&(s6e8aa0_lcd.lock));
#ifdef CONFIG_FB_MSM_MIPI_DSI_ESD_REFRESH
	set_lcd_esd_ignore( TRUE );
#endif 	
	mipi_dsi_cmds_tx(mfd, &s6e8aa0_tx_buf, s6e8aa0_display_off_cmds,
			ARRAY_SIZE(s6e8aa0_display_off_cmds));
	s6e8aa0_lcd.lcd_state.display_on = FALSE;
	s6e8aa0_lcd.lcd_state.initialized = FALSE;
	s6e8aa0_lcd.lcd_state.powered_up = FALSE;
#ifdef CONFIG_FB_MSM_MIPI_DSI_ESD_REFRESH
	set_lcd_esd_ignore( FALSE );
#endif 	
	mutex_unlock(&(s6e8aa0_lcd.lock));

	//gpio_set_value(GPIO_S6E8AA0_RESET, 0);
	//msleep(120);

	return 0;
}


static int lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

//#if def CONFIG_HAS_EARLYSUSPEND
#if 0 // for recovery ICS (andre.b.kim)
	DPRINT("%s : to be early_suspend. return.\n", __func__);
	return 0;
#endif 

    DPRINT("%s +\n", __func__);

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;
	pMFD = mfd;

	lcd_off_seq(mfd);
	DPRINT("%s -\n", __func__);
	return 0;
}

static int lcd_shutdown(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	DPRINT("%s +\n", __func__);

	//mfd = platform_get_drvdata(pdev);
	mfd = pMFD;
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;
	//pMFD = mfd;

	lcd_off_seq(mfd);
#if defined (CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
	msleep(20);
	gpio_set_value(GPIO_S6E8AA0_RESET, 0);
#endif
//	mipi_S6E8AA0_panel_power(FALSE);

	DPRINT("%s -\n", __func__);
	return 0;
}

static void lcd_gamma_apply( struct msm_fb_data_type *mfd, int srcGamma)
{
	LOG_ADD( " GAMMA(%d=%dcd)", srcGamma, s6e8aa0_lcd.lcd_brightness_table[srcGamma].lux );
	mipi_dsi_cmds_tx(mfd, &s6e8aa0_tx_buf, s6e8aa0_lcd.lcd_brightness_table[srcGamma].cmd,	1);
	mipi_dsi_cmds_tx(mfd, &s6e8aa0_tx_buf, &s6e8aa0_gamma_update_cmd, 1);
}

static void lcd_gamma_smartDimming_apply( struct msm_fb_data_type *mfd, int srcGamma)
{
	int gamma_lux;
	int i;
	
#ifdef MAPPING_TBL_AUTO_BRIGHTNESS
    gamma_lux = candela_table[srcGamma];
#else
    gamma_lux = s6e8aa0_lcd.lcd_brightness_table[srcGamma].lux;

#endif


	if( gamma_lux > SmartDimming_CANDELA_UPPER_LIMIT ) gamma_lux = SmartDimming_CANDELA_UPPER_LIMIT;

	for( i = SmartDimming_GammaUpdate_Pos; i < sizeof(GAMMA_SmartDimming_COND_SET); i++ ) 	GAMMA_SmartDimming_COND_SET[i] = 0;
	calc_gamma_table(&(s6e8aa0_lcd.smart), gamma_lux, GAMMA_SmartDimming_COND_SET +SmartDimming_GammaUpdate_Pos);

#ifdef SmartDimming_16bitBugFix
	if( gamma_lux > SmartDimming_CANDELA_UPPER_LIMIT /2 )
	{
		if( (int)GAMMA_SmartDimming_COND_SET[21] < 0x70 && GAMMA_SmartDimming_COND_SET[20] == 0 ) GAMMA_SmartDimming_COND_SET[20] =1;
		if( (int)GAMMA_SmartDimming_COND_SET[23] < 0x70 && GAMMA_SmartDimming_COND_SET[22] == 0 ) GAMMA_SmartDimming_COND_SET[22] =1;
		if( (int)GAMMA_SmartDimming_COND_SET[25] < 0x70 && GAMMA_SmartDimming_COND_SET[24] == 0 ) GAMMA_SmartDimming_COND_SET[24] =1;
	}
#endif 	
#if 0 // don't need log 
	char pBuffer[256];
	pBuffer[0] = 0;
	for( i =0; i < sizeof(GAMMA_SmartDimming_COND_SET); i++ )
	{
		sprintf( pBuffer+ strlen(pBuffer), " %02x", GAMMA_SmartDimming_COND_SET[i] );
	}
	DPRINT( "SD: %03d %s\n", gamma_lux, pBuffer );
#endif 
	mipi_dsi_cmds_tx(mfd, &s6e8aa0_tx_buf, &DSI_CMD_SmartDimming_GAMMA,	1);
	mipi_dsi_cmds_tx(mfd, &s6e8aa0_tx_buf, &s6e8aa0_gamma_update_cmd, 1);
	LOG_ADD( " SDIMMING(%d=%dcd)", srcGamma, gamma_lux );
}


static void lcd_gamma_ctl(struct msm_fb_data_type *mfd, struct lcd_setting *lcd)
{
	int gamma_level;

	gamma_level = s6e8aa0_lcd.stored_gamma;
	if( s6e8aa0_lcd.lcd_brightness_table[gamma_level].lux != s6e8aa0_lcd.lcd_brightness_table[s6e8aa0_lcd.lcd_gamma].lux )
	{

		if( s6e8aa0_lcd.isSmartDimming )
		{
			mutex_lock(&(s6e8aa0_lcd.lock));
			lcd_gamma_smartDimming_apply( mfd, gamma_level );
			mutex_unlock(&(s6e8aa0_lcd.lock));
		} 
		else {
			mutex_lock(&(s6e8aa0_lcd.lock));
			lcd_gamma_apply( mfd, gamma_level );
			mutex_unlock(&(s6e8aa0_lcd.lock));
		}

		s6e8aa0_lcd.lcd_gamma = gamma_level;
		
#ifdef MAPPING_TBL_AUTO_BRIGHTNESS
			LOG_ADD(" gamma_ctl(%d->%d(lcd),%dcd)", gamma_level, s6e8aa0_lcd.lcd_gamma, 
				candela_table[s6e8aa0_lcd.lcd_gamma] );
#else
			LOG_ADD(" gamma_ctl(%d->%d(lcd),%s,%dcd)", gamma_level, s6e8aa0_lcd.lcd_gamma, 
				s6e8aa0_lcd.lcd_brightness_table[s6e8aa0_lcd.lcd_gamma].strID,
				s6e8aa0_lcd.lcd_brightness_table[s6e8aa0_lcd.lcd_gamma].lux );
		
#endif
    	}
    	else
    	{
		s6e8aa0_lcd.lcd_gamma = gamma_level;
//		DPRINT("lcd_gamma_ctl %d -> %d(lcd)\n", gamma_level, s6e8aa0_lcd.lcd_gamma );
    	}
	
	return;

}

static void lcd_set_brightness(struct msm_fb_data_type *mfd, int gamma_level)
{
	//TODO: lock
       //spin_lock_irqsave(&bl_ctrl_lock, irqflags);

       s6e8aa0_lcd.stored_gamma = gamma_level;
       s6e8aa0_lcd.stored_elvss= gamma_level;
       s6e8aa0_lcd.stored_acl = gamma_level;

	lcd_gamma_ctl(mfd, &s6e8aa0_lcd);	
	lcd_set_elvss(mfd, &s6e8aa0_lcd);
	lcd_set_acl(mfd, &s6e8aa0_lcd);
	
#if !defined(CONFIG_HAS_EARLYSUSPEND)
	if (s6e8aa0_lcd.lcd_gamma <= LCD_OFF_GAMMA_VALUE) {
		lcd_off_seq( mfd );	// brightness = 0 -> LCD Off 
	}
#endif 

	//TODO: unlock
	//spin_unlock_irqrestore(&bl_ctrl_lock, irqflags);

}


#define DIM_BL 20
#define MIN_BL 30
#define MAX_BL 255

static int get_gamma_value_from_bl(int bl_value )// same as Seine, Celox 
{
	int gamma_value =0;
	int gamma_val_x10 =0;

	if(bl_value >= MIN_BL){
#ifdef MAPPING_TBL_AUTO_BRIGHTNESS	

		if (unlikely(!s6e8aa0_lcd.auto_brightness && bl_value > 250))
			bl_value = 250;
  
        	switch (bl_value) {
        	case 0 ... 29:
        		gamma_value = 0; // 30cd
        		break;
        	case 30 ... 254:
                gamma_value = (bl_value - candela_table[0]) / 10;
       		break;
        	case 255:
                gamma_value = CANDELA_TABLE_SIZE - 1;
    		break;
				
        	default:
              DPRINT(" >>> bl_value:%d , do not gamma_update\n ",bl_value);
              break;
        	}
      
	        DPRINT(" >>> bl_value:%d,gamma_value: %d\n ",bl_value,gamma_value);
#else

		gamma_val_x10 = 10 *(MAX_GAMMA_VALUE-1)*bl_value/(MAX_BL-MIN_BL) + (10 - 10*(MAX_GAMMA_VALUE-1)*(MIN_BL)/(MAX_BL-MIN_BL));
		gamma_value=(gamma_val_x10 +5)/10;
		
#endif			
	}else{
		gamma_value =0;
	}

	return gamma_value;
}

static void set_backlight(struct msm_fb_data_type *mfd)
{	
	int bl_level = mfd->bl_level;
	int gamma_level;
	static int pre_bl_level = 0;

	// brightness tuning
	gamma_level = get_gamma_value_from_bl(bl_level);

	if ((s6e8aa0_lcd.lcd_state.initialized) 
		&& (s6e8aa0_lcd.lcd_state.powered_up) 
		&& (s6e8aa0_lcd.lcd_state.display_on))
	{
		if(s6e8aa0_lcd.lcd_gamma != gamma_level)
		{
			LOG_START();
	       	LOG_ADD(" brightness!!!(%d,g=%d,s=%d)",bl_level,gamma_level,s6e8aa0_lcd.stored_gamma);
			lcd_set_brightness(mfd, gamma_level);
			LOG_FINISH();
		}
		else
		{
			LOG_START();
			lcd_set_brightness(mfd, gamma_level);
			LOG_CLEAR();
		}
	}
	else
	{
	       s6e8aa0_lcd.stored_gamma = gamma_level;
	       s6e8aa0_lcd.stored_elvss = gamma_level;
	       s6e8aa0_lcd.stored_acl = gamma_level;
       	DPRINT("brightness(Ignore_OFF) %d(bl)->gamma=%d stored=%d\n",bl_level,gamma_level,s6e8aa0_lcd.stored_gamma);
	}

	pre_bl_level = bl_level;

}


#define GET_NORMAL_ELVSS_ID(i) (s6e8aa0_lcd.lcd_elvss_table[i].cmd->payload[GET_NORMAL_ELVSS_ID_ADDRESS])
#define GET_EACH_ELVSS_ID(i) (lcd_elvss_delta_table[i])
static void lcd_apply_elvss(struct msm_fb_data_type *mfd, int srcElvss, struct lcd_setting *lcd)
{
	int	calc_elvss;
	char	update_elvss;

	if( lcd->factory_id_elvssType == lcd_id_elvss_each )
	{
			calc_elvss = s6e8aa0_lcd.eachElvss_value+ s6e8aa0_lcd.eachElvss_vector +GET_EACH_ELVSS_ID(srcElvss);
			if( calc_elvss > LCD_ELVSS_RESULT_LIMIT ) calc_elvss = LCD_ELVSS_RESULT_LIMIT;
			update_elvss = (char) calc_elvss | 0x80; //0x80 = indentity of elvss;
			EACH_ELVSS_COND_SET[EACH_ELVSS_COND_UPDATE_POS] = update_elvss;
		
			mipi_dsi_cmds_tx(mfd, &s6e8aa0_tx_buf, &(lcd_each_elvss_table[0]), 1);
			// elvss is always ON, It's not need update-cmd.

			s6e8aa0_lcd.lcd_elvss = srcElvss;
			LOG_ADD(" ELVSS(%d->%d,%x+%x:%x)", srcElvss, s6e8aa0_lcd.lcd_elvss, 
				s6e8aa0_lcd.eachElvss_value, GET_EACH_ELVSS_ID(srcElvss), update_elvss);
	}

	if( lcd->factory_id_elvssType == lcd_id_elvss_normal )
	{
			mipi_dsi_cmds_tx(mfd, &s6e8aa0_tx_buf, s6e8aa0_lcd.lcd_elvss_table[srcElvss].cmd,	1);
			// elvss is always ON, It's not need update-cmd.

			s6e8aa0_lcd.lcd_elvss = srcElvss;
			LOG_ADD(" ELVSS(%d->%d,%x)", srcElvss, s6e8aa0_lcd.lcd_elvss,
				GET_NORMAL_ELVSS_ID(srcElvss));
	}
}

static void lcd_set_elvss(struct msm_fb_data_type *mfd, struct lcd_setting *lcd)
{
	int elvss_level;


	elvss_level = s6e8aa0_lcd.stored_elvss; // gamma level 
	if( lcd->factory_id_elvssType == lcd_id_elvss_each )
	{
		if( GET_EACH_ELVSS_ID(s6e8aa0_lcd.lcd_elvss) == GET_EACH_ELVSS_ID(elvss_level) )
		{
			s6e8aa0_lcd.lcd_elvss = s6e8aa0_lcd.stored_elvss;
		}
		else
		{
			mutex_lock(&(s6e8aa0_lcd.lock));
			lcd_apply_elvss( mfd, elvss_level, &s6e8aa0_lcd );
			mutex_unlock(&(s6e8aa0_lcd.lock));

		}
	}

	if( lcd->factory_id_elvssType == lcd_id_elvss_normal )
	{
		if(GET_NORMAL_ELVSS_ID(s6e8aa0_lcd.lcd_elvss) == GET_NORMAL_ELVSS_ID(elvss_level) ) // if same as LCD
		{
			s6e8aa0_lcd.lcd_elvss = s6e8aa0_lcd.stored_elvss;
		}
		else
		{
			mutex_lock(&(s6e8aa0_lcd.lock));
			lcd_apply_elvss( mfd, elvss_level, &s6e8aa0_lcd );
			// elvss is always ON, It's not need update-cmd.
			mutex_unlock(&(s6e8aa0_lcd.lock));

			s6e8aa0_lcd.lcd_elvss = s6e8aa0_lcd.stored_elvss;
		}
	}
}




static void lcd_apply_acl(struct msm_fb_data_type *mfd, unsigned int srcAcl, boolean nowOffNeedOn)
{
#if 1 // Q1 : Not ACL-OFF by brightness
	if( s6e8aa0_lcd.lcd_acl_table[srcAcl].lux == 0 )
	{
		mipi_dsi_cmds_tx(mfd, &s6e8aa0_tx_buf, &s6e8aa0_acl_off_cmd, 1);
		LOG_ADD( " ACL(OFF:%s,%d)", s6e8aa0_lcd.lcd_acl_table[srcAcl].strID, s6e8aa0_lcd.lcd_acl_table[srcAcl].lux);
	}
	else
#endif 	
	{
		mipi_dsi_cmds_tx(mfd, &s6e8aa0_tx_buf, s6e8aa0_lcd.lcd_acl_table[srcAcl].cmd, 1);
		if( nowOffNeedOn || s6e8aa0_lcd.lcd_acl_table[s6e8aa0_lcd.lcd_acl].lux == 0 )
		{
			mipi_dsi_cmds_tx(mfd, &s6e8aa0_tx_buf, &s6e8aa0_acl_on_cmd, 1);
			LOG_ADD( " ACL(ON:%s,%d)", s6e8aa0_lcd.lcd_acl_table[srcAcl].strID, s6e8aa0_lcd.lcd_acl_table[srcAcl].lux);
		}
		else
		{
			LOG_ADD( " ACL(%s,%d)", s6e8aa0_lcd.lcd_acl_table[srcAcl].strID, s6e8aa0_lcd.lcd_acl_table[srcAcl].lux);
		}
	}
}

static void lcd_set_acl(struct msm_fb_data_type *mfd, struct lcd_setting *lcd)
{
	unsigned int acl_level;

	if( s6e8aa0_lcd.enabled_acl )	acl_level = s6e8aa0_lcd.stored_acl;
	else acl_level = 0;

	if( s6e8aa0_lcd.lcd_acl_table[s6e8aa0_lcd.lcd_acl].lux == s6e8aa0_lcd.lcd_acl_table[acl_level].lux) // if same as LCD
	{
		s6e8aa0_lcd.lcd_acl = acl_level;
	}
	else
	{
		mutex_lock(&(s6e8aa0_lcd.lock));
		lcd_apply_acl(mfd, acl_level, FALSE );
		// acl is always ON, It's not need update-cmd.
		mutex_unlock(&(s6e8aa0_lcd.lock));

		s6e8aa0_lcd.lcd_acl = acl_level;
	}
}


/////////////////[
struct class *sysfs_lcd_class;
struct device *sysfs_panel_dev;

static ssize_t power_reduce_show(struct device *dev, struct 
device_attribute *attr, char *buf)
{
//	struct ld9040 *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", s6e8aa0_lcd.enabled_acl);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t power_reduce_store(struct device *dev, struct 
device_attribute *attr, const char *buf, size_t size)
{
	int value;
	int rc;
	struct msm_fb_data_type *mfd;
	mfd = dev_get_drvdata(dev);

	rc = strict_strtoul(buf, (unsigned int) 0, (unsigned long *)&value);
	if (rc < 0) return rc;
	else {
		if (s6e8aa0_lcd.enabled_acl != value) {
			s6e8aa0_lcd.enabled_acl = value;
			//			if (lcd->ldi_enable)
			{         
				LOG_START();
				LOG_ADD("acl_set_store(changed) : %d -> ", value);	
				lcd_set_acl(pMFD, &s6e8aa0_lcd);    // Arimy to make func
				LOG_FINISH();
			}
		}
		return size;
	}
}

static DEVICE_ATTR(power_reduce, 0664,
		power_reduce_show, power_reduce_store);

static ssize_t lcd_type_show(struct device *dev, struct 
device_attribute *attr, char *buf)
{

	char temp[15];
	sprintf(temp, "SMD_AMS465GS02\n");
	strcat(buf, temp);
	return strlen(buf);
}

static DEVICE_ATTR(lcd_type, 0664,
		lcd_type_show, NULL);

static ssize_t octa_lcdtype_show(struct device *dev, struct 
device_attribute *attr, char *buf)
{
	char pBuffer[64] = {0,};
	char cntBuffer;

	cntBuffer = 0;
	cntBuffer += sprintf(pBuffer +cntBuffer, "HD720");

	switch(s6e8aa0_lcd.factory_id_line )
	{
	case lcd_id_a1_m3_line: 
		cntBuffer += sprintf(pBuffer +cntBuffer, " A1M3");
	break;
	
	case lcd_id_a1_sm2_line: 
		cntBuffer += sprintf(pBuffer +cntBuffer, " A1SM2");
	break;
	
	case lcd_id_a2_sm2_line_1: 
		cntBuffer += sprintf(pBuffer +cntBuffer, " A2SM2 line_1");
	break;

	case lcd_id_a2_sm2_line_2: 
		cntBuffer += sprintf(pBuffer +cntBuffer, " A2SM2 line_2");
	break;

	case lcd_id_unknown: 
	default:
		cntBuffer += sprintf(pBuffer +cntBuffer, " A?");
	break;
	} // end switch

	switch(s6e8aa0_lcd.factory_id_colorTemp )
	{
	case lcd_id_temp_7500k: 
		cntBuffer += sprintf(pBuffer +cntBuffer, " 7500K");
	break;
	
	case lcd_id_temp_8500k: 
		cntBuffer += sprintf(pBuffer +cntBuffer, " 8500K");
	break;

	case lcd_id_unknown: 
	default:
		cntBuffer += sprintf(pBuffer +cntBuffer, " 7500?K");
	break;
	} // end switch

	switch(s6e8aa0_lcd.factory_id_elvssType)
	{
	case lcd_id_elvss_normal:
		cntBuffer += sprintf(pBuffer +cntBuffer, " elvss");
	break;
	
	case lcd_id_elvss_each:
		cntBuffer += sprintf(pBuffer +cntBuffer, " IElvss(%x)", s6e8aa0_lcd.eachElvss_value);
	break;

	case lcd_id_unknown: 
	default:
		cntBuffer += sprintf(pBuffer +cntBuffer, " ?elvss");
	break;
	} // end switch

#ifdef SmartDimming_useSampleValue
	if( isFAIL_mtp_load )
	{
		if(s6e8aa0_lcd.isSmartDimming)
			cntBuffer += sprintf(pBuffer +cntBuffer, " SMARTDIM");
	} else 				
#endif 				
	if(s6e8aa0_lcd.isSmartDimming)
		cntBuffer += sprintf(pBuffer +cntBuffer, " SmartDim");


	strcpy( buf, pBuffer );
	
	return strlen(buf);
}

static DEVICE_ATTR(octa_lcdtype, 0664,
		octa_lcdtype_show, NULL);

static ssize_t octa_mtp_show(struct device *dev, struct 
device_attribute *attr, char *buf)
{
	char pBuffer[160] = {0,};
	char cntBuffer;
	int i;

	cntBuffer = 0;

	cntBuffer += sprintf(pBuffer +cntBuffer, "mtp : ");
	for( i = 0; i < MTP_DATA_SIZE; i++ )	
	{ 
		cntBuffer += sprintf(pBuffer +cntBuffer, " %02x", lcd_mtp_data[i] );
		if( (i+1) % (IV_MAX+1) == 0 ) cntBuffer += sprintf(pBuffer +cntBuffer, "." );  // because, one of mtp is 2byte 
	}

	strcpy( buf, pBuffer );
	
	return strlen(buf);
}

static DEVICE_ATTR(octa_mtp, 0664,
		octa_mtp_show, NULL);

static ssize_t octa_adjust_mtp_show(struct device *dev, struct 
device_attribute *attr, char *buf)
{
	char pBuffer[160] = {0,};
	char cntBuffer;
	int i,j;
	int	is_over255;

	cntBuffer = 0;
	is_over255 = FALSE;

	cntBuffer += sprintf(pBuffer +cntBuffer, "adjust_mtp : ");
	for( i = 0; i < CI_MAX; i ++ )
	{
		for( j = 0; j < IV_MAX; j++ )
		{
			if( s6e8aa0_lcd.smart.adjust_mtp[i][j] > 255 ) 			
			{
				cntBuffer += sprintf(pBuffer +cntBuffer, " %04X", s6e8aa0_lcd.smart.adjust_mtp[i][j] );
				is_over255 = TRUE;
			}
			else cntBuffer += sprintf(pBuffer +cntBuffer, " %02x", s6e8aa0_lcd.smart.adjust_mtp[i][j] );
		}
		cntBuffer += sprintf(pBuffer +cntBuffer, "." );
	}
	if( is_over255 ) cntBuffer += sprintf(pBuffer +cntBuffer, " OVER255" );

	strcpy( buf, pBuffer );
	
	return strlen(buf);
}

static DEVICE_ATTR(octa_adjust_mtp, 0664,
		octa_adjust_mtp_show, NULL);

static ssize_t lcd_sysfs_show_gamma_mode(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
//	struct lcd_setting *plcd = dev_get_drvdata(dev);
	char temp[10];

	switch (s6e8aa0_lcd.gamma_mode) {
	case 0:
		sprintf(temp, "2.2 mode\n");
		strcat(buf, temp);
		break;
	case 1:
		sprintf(temp, "1.9 mode\n");
		strcat(buf, temp);
		break;
	default:
		dev_info(dev, "gamma mode could be 0:2.2, 1:1.9 or 2:1.7)n");
		break;
	}

	return strlen(buf);
}

static ssize_t lcd_sysfs_store_gamma_mode(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	struct lcd_setting *plcd = dev_get_drvdata(dev);
	int rc;

	dev_info(dev, "lcd_sysfs_store_gamma_mode\n");

	rc = strict_strtoul(buf, 0, (unsigned long *)&plcd->gamma_mode);
	if (rc < 0)
		return rc;
	
//	if (lcd->ldi_enable)
//		ld9040_gamma_ctl(lcd);  Arimy to make func

	return len;
}

static DEVICE_ATTR(gamma_mode, 0664,
		lcd_sysfs_show_gamma_mode, lcd_sysfs_store_gamma_mode);

static ssize_t lcd_sysfs_show_gamma_table(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
//	struct lcd_setting *plcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", s6e8aa0_lcd.gamma_table_count);
	strcpy(buf, temp);

	return strlen(buf);
}

static DEVICE_ATTR(gamma_table, 0664,
		lcd_sysfs_show_gamma_table, NULL);


static int lcd_power(struct msm_fb_data_type *mfd, struct lcd_setting *plcd, int power)
{
	int ret = 0;
	
    if(power == FB_BLANK_UNBLANK)
    {
		if(s6e8aa0_lcd.lcd_state.display_on == TRUE)
			return ret;

    		// LCD ON
    		DPRINT("%s : LCD_ON\n", __func__);
    		lcd_on_seq(mfd);
    }
    else if(power == FB_BLANK_POWERDOWN)
    {
        if (s6e8aa0_lcd.lcd_state.powered_up && s6e8aa0_lcd.lcd_state.display_on) {
			
		// LCD OFF
    		DPRINT("%s : LCD_OFF\n", __func__);
		lcd_off_seq(mfd);
	    }
    }

	return ret;
}


static ssize_t lcd_sysfs_store_lcd_power(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	int rc;
	int lcd_enable;
//	struct lcd_setting *plcd = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd;
	mfd = dev_get_drvdata(dev);
	
	dev_info(dev, "lcd_sysfs_store_lcd_power\n");

	rc = strict_strtoul(buf, 0, (unsigned long *)&lcd_enable);
	if (rc < 0)
		return rc;

	if(lcd_enable) {
		lcd_power(mfd, &s6e8aa0_lcd, FB_BLANK_UNBLANK);
	}
	else {
		lcd_power(mfd, &s6e8aa0_lcd, FB_BLANK_POWERDOWN);
	}

	return len;
}

static DEVICE_ATTR(lcd_power, 0664,
		NULL, lcd_sysfs_store_lcd_power);



#ifdef MAPPING_TBL_AUTO_BRIGHTNESS
static ssize_t lcd_sysfs_store_auto_brightness(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t len)
{
	int value;
	int rc;
	
	dev_info(dev, "lcd_sysfs_store_auto_brightness\n");

	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (s6e8aa0_lcd.auto_brightness != value) {
			dev_info(dev, "%s - %d, %d\n", __func__, s6e8aa0_lcd.auto_brightness, value);
			mutex_lock(&(s6e8aa0_lcd.lock));
			s6e8aa0_lcd.auto_brightness = value;
			mutex_unlock(&(s6e8aa0_lcd.lock));

		}
	}
	return len;
}

static DEVICE_ATTR(auto_brightness, 0664,
		NULL, lcd_sysfs_store_auto_brightness);

#endif

/////////////////]



#ifdef CONFIG_HAS_EARLYSUSPEND
static void early_suspend(struct early_suspend *h) 
{
	DPRINT("%s +\n", __func__);
	lcd_off_seq(pMFD);
	DPRINT("%s -\n", __func__);

 return;
}

static void late_resume(struct early_suspend *h) 
{
	DPRINT("%s\n", __func__);
	lcd_on_seq(pMFD);
	return;
}
#endif



static int __devinit lcd_probe(struct platform_device *pdev)
{
	int ret = 0;
	int default_gamma_value;

	if (pdev->id == 0) {
		pdata = pdev->dev.platform_data;
		DPRINT("%s\n", __func__);
		return 0;
	}

	DPRINT("%s+ \n", __func__);

	pMFD = platform_get_drvdata(pdev);

	// get default-gamma address from gamma table
	for( default_gamma_value = 0; default_gamma_value <= MAX_GAMMA_VALUE; default_gamma_value++)
		if( lcd_brightness_7500k_table[default_gamma_value].lux >= DEFAULT_CANDELA_VALUE ) break;
#if defined (CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
    default_gamma_value = MAX_GAMMA_VALUE;  // celox hd plm
#endif
	s6e8aa0_lcd.stored_gamma = default_gamma_value;
	s6e8aa0_lcd.stored_elvss = default_gamma_value;
	s6e8aa0_lcd.lcd_gamma = -1;
	s6e8aa0_lcd.enabled_acl = 1;
	s6e8aa0_lcd.stored_acl= default_gamma_value;
	s6e8aa0_lcd.lcd_acl = -1;
	s6e8aa0_lcd.lcd_state.display_on = false;
	s6e8aa0_lcd.lcd_state.initialized= false;
	s6e8aa0_lcd.lcd_state.powered_up= false;
	s6e8aa0_lcd.factory_id_line	= lcd_id_unknown;
	s6e8aa0_lcd.factory_id_colorTemp	= lcd_id_unknown;
	s6e8aa0_lcd.factory_id_elvssType	= lcd_id_unknown;
	s6e8aa0_lcd.lcd_brightness_table = lcd_brightness_7500k_table;
	s6e8aa0_lcd.lcd_acl_table = lcd_acl_ManualF_table;
	s6e8aa0_lcd.lcd_elvss_table = lcd_elvss_dcdc03_table;
	s6e8aa0_lcd.eachElvss_value = 0;
	s6e8aa0_lcd.eachElvss_vector = 0;
	s6e8aa0_lcd.isSmartDimming = FALSE;
	s6e8aa0_lcd.isSmartDimming_loaded = FALSE;	
	s6e8aa0_lcd.AID = NOT_AID;
	s6e8aa0_lcd.cellPulse = 0;
#ifdef MAPPING_TBL_AUTO_BRIGHTNESS
	s6e8aa0_lcd.auto_brightness = 0;
#endif
	mutex_init( &(s6e8aa0_lcd.lock) );

#ifdef CONFIG_HAS_EARLYSUSPEND
	 s6e8aa0_lcd.early_suspend.suspend = early_suspend;
	 s6e8aa0_lcd.early_suspend.resume = late_resume;
	 s6e8aa0_lcd.early_suspend.level = LCD_OFF_GAMMA_VALUE;
	 register_early_suspend(&s6e8aa0_lcd.early_suspend);
#endif

	DPRINT("msm_fb_add_device +\n");
	msm_fb_add_device(pdev);
	DPRINT("msm_fb_add_device -\n");

/////////////[ sysfs
    sysfs_lcd_class = class_create(THIS_MODULE, "lcd");
	if (IS_ERR(sysfs_lcd_class))
		pr_err("Failed to create class(sysfs_lcd_class)!\n");

	sysfs_panel_dev = device_create(sysfs_lcd_class,
		NULL, 0, NULL, "panel");
	if (IS_ERR(sysfs_panel_dev))
		pr_err("Failed to create device(sysfs_panel_dev)!\n");

	ret = device_create_file(sysfs_panel_dev, &dev_attr_power_reduce);
	if (ret < 0)
		dev_err(&(pdev->dev), "failed to add sysfs entries\n");

	ret = device_create_file(sysfs_panel_dev, &dev_attr_lcd_type);  
	if (ret < 0)
		dev_err(&(pdev->dev), "failed to add sysfs entries\n");

	ret = device_create_file(sysfs_panel_dev, &dev_attr_octa_lcdtype);  
	if (ret < 0)
		DPRINT("octa_lcdtype failed to add sysfs entries\n");
//		dev_err(&(pdev->dev), "failed to add sysfs entries\n");

	ret = device_create_file(sysfs_panel_dev, &dev_attr_octa_mtp);  
	if (ret < 0)
		DPRINT("octa_mtp failed to add sysfs entries\n");

	ret = device_create_file(sysfs_panel_dev, &dev_attr_octa_adjust_mtp);  
	if (ret < 0)
		DPRINT("octa_adjust_mtp failed to add sysfs entries\n");

	ret = device_create_file(sysfs_panel_dev, &dev_attr_lcd_power);  
	if (ret < 0)
		DPRINT("lcd_power failed to add sysfs entries\n");
//		dev_err(&(pdev->dev), "failed to add sysfs entries\n");

	// mdnie sysfs create
	init_mdnie_class();
#ifdef MAPPING_TBL_AUTO_BRIGHTNESS
#ifdef CONFIG_BACKLIGHT_CLASS_DEVICE
    if(1){
      struct backlight_device *pbd = NULL;
      pbd = backlight_device_register("panel", NULL, NULL,NULL,NULL);
      if (IS_ERR(pbd)) {
        DPRINT("Could not register 'panel' backlight device\n");
      }
      else
      {
        ret = device_create_file(&pbd->dev, &dev_attr_auto_brightness);
        if (ret < 0)
          DPRINT("auto_brightness failed to add sysfs entries\n");
      }
    }
#endif
#endif
////////////]	
	DPRINT("%s-\n", __func__ );

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = lcd_probe,
	.driver = {
		.name   = "mipi_lcd_hd720",
	},
	.shutdown = lcd_shutdown
};

static struct msm_fb_panel_data s6e8aa0_panel_data = {
	.on		= lcd_on,
	.off		= lcd_off,
	.set_backlight = set_backlight,
};

static int ch_used[3];

int mipi_s6e8aa0_hd720_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	pdev = platform_device_alloc("mipi_lcd_hd720", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	s6e8aa0_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &s6e8aa0_panel_data,
		sizeof(s6e8aa0_panel_data));
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int __init lcd_init(void)
{
	DPRINT("%s \n", __func__);
	mipi_dsi_buf_alloc(&s6e8aa0_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&s6e8aa0_rx_buf, DSI_BUF_SIZE);

	return platform_driver_register(&this_driver);
}

module_init(lcd_init);
