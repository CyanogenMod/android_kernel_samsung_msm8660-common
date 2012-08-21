struct Cmc624RegisterSet{
	unsigned int RegAddr;
	unsigned int Data;
};

enum eLcd_mDNIe_UI{
	mDNIe_UI_MODE,
	mDNIe_VIDEO_MODE,
	mDNIe_VIDEO_WARM_MODE,
	mDNIe_VIDEO_COLD_MODE,
	mDNIe_CAMERA_MODE,
	mDNIe_NAVI,
	mDNIe_GALLERY_MODE,
	mDNIe_DMB_MODE,
	mDNIe_VT_MODE,
	MAX_mDNIe_MODE,
};

enum SCENARIO_COLOR_TONE {
	COLOR_TONE_1 = 40,
	COLOR_TONE_2,
	COLOR_TONE_3,
	COLOR_TONE_MAX,
};

enum eCurrent_Temp {
    TEMP_STANDARD =0,
    TEMP_WARM,
    TEMP_COLD,
    MAX_TEMP_MODE,
};

enum eBackground_Mode {
    DYNAMIC_MODE = 0,
    STANDARD_MODE,
    NATURAL_MODE,
    MOVIE_MODE,
    MAX_BACKGROUND_MODE,
};

enum eCabc_Mode {
    CABC_OFF_MODE = 0,
    CABC_ON_MODE, 
    MAX_CABC_MODE,        
};

enum eOutdoor_Mode {
    OUTDOOR_OFF_MODE = 0,
    OUTDOOR_ON_MODE,
    MAX_OUTDOOR_MODE,
};

enum mDNIe_mode_CABC_type{
	mode_type_CABC_none,
	mode_type_CABC_on,
	mode_type_CABC_off,
};

struct str_sub_unit {
    char *name;
    const struct Cmc623RegisterSet *value;
	int size;
};

struct str_sub_tuning {
/*
    Array Index 0 : cabc off tuning value
    Array Index 1 : cabc on tunning value 
*/    
    struct str_sub_unit value[MAX_CABC_MODE];
};

#define TUNE_FLAG_CABC_AUTO         0
#define TUNE_FLAG_CABC_ALWAYS_OFF   1
#define TUNE_FLAG_CABC_ALWAYS_ON    2 


struct str_main_unit {
    char *name;
    int flag;
    const struct Cmc623RegisterSet *tune;
    unsigned char *plut;
	int size;
};

struct str_main_tuning {
/*
    Array Index 0 : cabc off tuning value
    Array Index 1 : cabc on tunning value 
*/    
    struct str_main_unit value[MAX_CABC_MODE];
};

#define NUM_ITEM_POWER_LUT	9
#define NUM_POWER_LUT	2


#define NUM_ITEM_POWER_LUT	9

