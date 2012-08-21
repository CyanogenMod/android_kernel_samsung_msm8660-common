/* linux/drivers/video/samsung/smartdimming.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com

 * Samsung Smart Dimming for OCTA
 *
 * Minwoo Kim, <minwoo7945.kim@samsung.com>
 *
*/


#ifndef __SMART_DIMMING_H__
#define __SMART_DIMMING_H__


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>


#if 0 
typedef unsigned long long  u64;
typedef signed long long    s64;
typedef unsigned int	    u32;
typedef signed int	        s32;
typedef unsigned short	    u16;
typedef signed short	    s16;
typedef unsigned char	    u8;
typedef signed char	        s8;
#endif

enum {
    CI_RED      = 0,
    CI_GREEN    = 1,
    CI_BLUE     = 2,
    CI_MAX      = 3,
};

enum {
    IV_1        = 0,
    IV_15       = 1,
    IV_35       = 2,
    IV_59       = 3,
    IV_87       = 4,
    IV_171      = 5,
    IV_255      = 6,
    IV_MAX      = 7,
    IV_TABLE_MAX = 8,
};

enum {
    AD_IV0      =   0,
    AD_IV1      =   1,
    AD_IV15     =   2,
    AD_IV35     =   3,
    AD_IV59     =   4,
    AD_IV87     =   5,
    AD_IV171    =   6,
    AD_IV255    =   7,
    AD_IVMAX    =   8,
};


#define MAX_GRADATION   300


struct str_voltage_entry{
    //u32 g22_value;
    u32 v[CI_MAX];
};

struct str_table_info {
    //et : start gray value
    u8 st;
    // end gray value, st + count
    u8 et;
    u8 count;
    u8 *offset_table;
    // rv : ratio value 
    u32 rv;
};


struct str_flookup_table{
    u16 entry;
    u16 count;
};


struct str_smart_dim{
    s16 mtp[CI_MAX][IV_MAX];
    //s16 adjust_mtp[CI_MAX][IV_MAX];
    struct str_voltage_entry ve[256];
    u8 *default_gamma; 
    struct str_table_info t_info[IV_TABLE_MAX];
    //u8 v_table_array[256];
    struct str_flookup_table *flooktbl;
    u32 *g22_tbl;
    u32 *g300_gra_tbl;
    u32 adjust_volt[CI_MAX][AD_IVMAX];
    //u32 *ad_cv;
    //u32 *ad_dv;
    //u32 refvolt;
};



int init_table_info(struct str_smart_dim *smart, unsigned char* srcGammaTable);
u8 calc_voltage_table(struct str_smart_dim *smart, const u8 *mtp);
u32 calc_gamma_table(struct str_smart_dim *smart, u32 gv, u8 result[]);


#endif
