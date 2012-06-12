/*
 * =====================================================================================
 *
 *       Filename:  p5lte_cmc623.h
 *
 *    Description:  LCDC P5LTE define header file.
 *                  LVDS define
 *                  CMC623 Imaging converter driver define
 *
 *        Version:  1.0
 *        Created:  2011년 04월 08일 16시 41분 42초
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Park Gyu Tae (), 
 *        Company:  Samsung Electronics
 *
 * =====================================================================================
 */
#include "cmc623.h"
#if defined (CONFIG_MACH_P4_LTE) && (defined (CONFIG_TARGET_LOCALE_JPN_NTT) || defined(CONFIG_TARGET_LOCALE_KOR_SKT))
#include "p4lte_cmc623_tune.h"
#else
#include "p5lte_cmc623_tune.h"
#endif

#define CMC623_INITSEQ cmc623_init2
//#define CMC623_INITSEQ cmc623_bypass
#define CMC623_MAX_SETTINGS	 100
#define TUNING_FILE_PATH "/sdcard/tuning/"

#define PMIC_GPIO_LVDS_nSHDN 18 /*  LVDS_nSHDN GPIO on PM8058 is 19 */
#define PMIC_GPIO_BACKLIGHT_RST 36 /*  LVDS_nSHDN GPIO on PM8058 is 37 */
#define PMIC_GPIO_LCD_LEVEL_HIGH	1
#define PMIC_GPIO_LCD_LEVEL_LOW		0

#define CMC623_nRST			28
#define CMC623_SLEEP		103
#define CMC623_BYPASS		104
#define CMC623_FAILSAFE		106

#define MLCD_ON				128
#define CMC623_SDA			156
#define CMC623_SCL			157

#define P5LTE_LCDON			1
#define P5LTE_LCDOFF		0

/*  BYPASS Mode OFF */
#define BYPASS_MODE_ON 		0


struct cmc623_state_type{
	enum eCabc_Mode cabc_mode;
	unsigned int brightness;
	unsigned int suspended;
    enum eLcd_mDNIe_UI scenario;
    enum eBackground_Mode background;
//This value must reset to 0 (standard value) when change scenario
    enum eCurrent_Temp temperature;
    enum eOutdoor_Mode ove;
    const struct str_sub_unit *sub_tune;
    const struct str_main_unit *main_tune;
};


/*  CMC623 function */
int cmc623_sysfs_init(void);
void bypass_onoff_ctrl(int value);
void cabc_onoff_ctrl(int value);
void set_backlight_pwm(int level);
int load_tuning_data(char *filename);
int apply_main_tune_value(enum eLcd_mDNIe_UI ui, enum eBackground_Mode bg, enum eCabc_Mode cabc, int force);
int apply_sub_tune_value(enum eCurrent_Temp temp, enum eOutdoor_Mode ove, enum eCabc_Mode cabc, int force);
void dump_cmc623_register(void);

int p5lte_cmc623_init(void);
int p5lte_cmc623_on(int enable);
int p5lte_cmc623_bypass_mode(void);
int p5lte_cmc623_normal_mode(void);
int p5lte_lvds_on(int enable);
int p5lte_backlight_en(int enable);
void cmc623_manual_brightness(int bl_lvl);

#if defined(CONFIG_TARGET_LOCALE_KOR_SKT) || defined(CONFIG_TARGET_LOCALE_KOR_LGU)
void cmc623_Set_Region_Ext(int enable, int hStart, int hEnd, int vStart, int vEnd);
#endif
