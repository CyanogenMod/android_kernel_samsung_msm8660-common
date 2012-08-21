/* linux/drivers/video/samsung/smartdimming.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com

 * Samsung Smart Dimming for OCTA
 *
 * Minwoo Kim, <minwoo7945.kim@samsung.com>
 *
*/


#include "smart_dimming_ea8868.h"
#include "s6ea8868_volt_tbl.h"

const u8 v1_offset_table[14] = {
	49,44,39,34,29,24,20,16,12,8,6,4,2,0
};

const u8 v15_offset_table[20] = {
	132,124,116,108,100,92,84,76,69,62,55,48,42,36,30,24,18,12,6,0
};

const u8 range_table_count[IV_TABLE_MAX] = {
    1, 14, 20, 24, 28, 84, 84, 1
};


const u32 table_radio[IV_TABLE_MAX] = {
	0, 607, 234, 1365, 1170, 390, 390, 0
};

const u32 dv_value[IV_MAX] = {
    0, 15, 35, 59, 87, 171, 255    
};

const char color_name[3] = {'R','G','B'}; 

const u8 *offset_table[IV_TABLE_MAX] = 
{
    NULL,
    v1_offset_table,
    v15_offset_table,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static s16 s9_to_s16(s16 v)
{
	return (s16)(v << 7) >> 7;
}
u32 calc_v1_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
    u32 ret = 0;

    if(gamma >= V1_VOLTAGE_COUNT) {
        printk("[SMART:ERROR] : %s Exceed Max Gamma Value (%d)",__func__,gamma);
        gamma = V1_VOLTAGE_COUNT-1;
    }
    ret = volt_table_v1[gamma] >> 10;
    return ret;
}


u32 calc_v15_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
    //for CV : 20, DV :320 
    int ret = 0;
    u32 v1, v35;
    u32 ratio = 0;

    v1 = adjust_volt[rgb_index][AD_IV1];
    v35 = adjust_volt[rgb_index][AD_IV35];
    if(gamma >= V15_VOLTAGE_COUNT) {
        printk("[SMART:ERROR] : %s Exceed Max Gamma Value (%d)",__func__,gamma);
        gamma = V15_VOLTAGE_COUNT-1;
    }

    ratio = volt_table_cv_20_dv_320[gamma];
    
    if (v1 == 0 || v35 == 0) 
    {
        // 
    }
    
    ret = (v1 << 10) - ((v1-v35)*ratio);
    ret = ret >> 10;
    return ret;
}



u32 calc_v35_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
    //for CV : 64, DV :320 
    int ret = 0;
    u32 v1, v59;
    u32 ratio = 0;

    v1 = adjust_volt[rgb_index][AD_IV1];
    v59 = adjust_volt[rgb_index][AD_IV59];
    if(gamma >= V35_VOLTAGE_COUNT) {
        printk("[SMART:ERROR] : %s Exceed Max Gamma Value (%d)",__func__,gamma);
        gamma = V35_VOLTAGE_COUNT-1;
    }
    ratio = volt_table_cv_64_dv_320[gamma];
    
    if (v1 == 0 || v59 == 0) 
    {
        // 
    }
    
    ret = (v1 << 10) - ((v1-v59)*ratio);
    ret = ret >> 10;
    return ret;
}


u32 calc_v59_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
    //for CV : 64, DV :320 
    int ret = 0;
    u32 v1, v87;
    u32 ratio = 0;

    v1 = adjust_volt[rgb_index][AD_IV1];
    v87 = adjust_volt[rgb_index][AD_IV87];
    if(gamma >= V59_VOLTAGE_COUNT) {
        printk("[SMART:ERROR] : %s Exceed Max Gamma Value (%d)",__func__,gamma);
        gamma = V59_VOLTAGE_COUNT-1;

    }
    ratio = volt_table_cv_64_dv_320[gamma];
    
    if (v1 == 0 || v87 == 0) 
    {
        // 
    }
    
    ret = (v1 << 10) - ((v1-v87)*ratio);
    ret = ret >> 10;
    return ret;
}


u32 calc_v87_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
    //for CV : 64, DV :320 
    int ret = 0;
    u32 v1, v171;
    u32 ratio = 0;

    v1 = adjust_volt[rgb_index][AD_IV1];
    v171 = adjust_volt[rgb_index][AD_IV171];
    if(gamma >= V87_VOLTAGE_COUNT) {
        printk("[SMART:ERROR] : %s Exceed Max Gamma Value (%d)",__func__,gamma);
        gamma = V87_VOLTAGE_COUNT-1;

    }
    ratio = volt_table_cv_64_dv_320[gamma];
    
    if (v1 == 0 || v171 == 0) 
    {
        // 
    }
    
    ret = (v1 << 10) - ((v1-v171)*ratio);
    ret = ret >> 10;
    return ret;
}


u32 calc_v171_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
    //for CV : 64, DV :320 
    int ret = 0;
    u32 v1, v255;
    u32 ratio = 0;

    v1 = adjust_volt[rgb_index][AD_IV1];
    v255 = adjust_volt[rgb_index][AD_IV255];
    if(gamma >= V171_VOLTAGE_COUNT) {
        printk("[SMART:ERROR] : %s Exceed Max Gamma Value (%d)",__func__,gamma);
        gamma = V171_VOLTAGE_COUNT-1;

    }
    ratio = volt_table_cv_64_dv_320[gamma];

    if (v1 == 0 || v255 == 0) 
    {
        // 
    }
    
    ret = (v1 << 10) - ((v1-v255)*ratio);
    ret = ret >> 10;
    return ret;
}



u32 calc_v255_volt(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX])
{
    u32 ret = 0;
    if(gamma >= V255_VOLTAGE_COUNT) {
        printk("[SMART:ERROR] : %s Exceed Max Gamma Value (%d)",__func__,gamma);
        gamma = V255_VOLTAGE_COUNT-1;
    }

    ret = volt_table_v255[gamma] >> 10;
    return ret;
}

u8 calc_voltage_table(struct str_smart_dim *smart, const u8 *mtp)
{
    int c,i,j;
    int offset = 0;
    int offset1 = 0;	
    s16 t1,t2; 
    s16 adjust_mtp[CI_MAX][IV_MAX] = {0, };
    //u32 adjust_volt[CI_MAX][AD_IVMAX] = {0, };
    u8 range_index; 
    u8 table_index=0;

    u32 v1, v2;
    u32 ratio;
    
    const u32(*calc_volt[IV_MAX])(s16 gamma, int rgb_index, u32 adjust_volt[CI_MAX][AD_IVMAX]) = 
    {
        calc_v1_volt,
        calc_v15_volt,
        calc_v35_volt,
        calc_v59_volt,
        calc_v87_volt,
        calc_v171_volt,
        calc_v255_volt,
    };

    u8 calc_seq[6] = {IV_1, IV_171, IV_87, IV_59, IV_35, IV_15};
    u8 ad_seq[6] = {AD_IV1, AD_IV171, AD_IV87, AD_IV59, AD_IV35, AD_IV15};


	// Gamma MTP Offset Setting, Output 1, V0, V255
    for(c=CI_RED;c<CI_MAX;c++){
		//V255R -> V171R -> V87 -> V59 -> V35 -> V15 -> V1R ->  V255G -> .. -> V1G -> .. -> V1B
		offset = IV_MAX * c + (IV_MAX - IV_255); // MTP 21 Bytes, 
		offset1 = IV_TABLE_MAX * c + (IV_MAX - IV_255); // default gamma(Cmd) = 24 bytes
        t1 = s9_to_s16(mtp[offset-1]<<8|mtp[offset]);
		t2 = s9_to_s16( (smart->default_gamma[offset1-1] << 8) | (smart->default_gamma[offset1 ]) ) + t1; 

        smart->mtp[c][IV_255] = t1;
        adjust_mtp[c][IV_255] = t2;
  //for V0 All RGB Voltage Value is Reference Voltage     		
        smart->adjust_volt[c][AD_IV0] = 4608; // 4.5 * 1024 VREG in mV
        smart->adjust_volt[c][AD_IV255] = calc_volt[IV_255](t2, c, smart->adjust_volt);
    }

	// Gamma MTP Offset Setting, Output 1, V1 to V171
    for(c=CI_RED;c<CI_MAX;c++){
        for(i=IV_1;i<IV_255;i++){
			offset = IV_MAX * c + (IV_MAX - calc_seq[i]);
			offset1 = IV_TABLE_MAX * c + (IV_MAX - calc_seq[i]);

			if( i == IV_1 ) /* MTP Offset V15 ~ V255. not use for V1 */
				t1 = 0;
			else
	            t1 = (s8)mtp[offset];
			
            t2 = smart->default_gamma[offset1] + t1;
	
            smart->mtp[c][calc_seq[i]] = t1;
            adjust_mtp[c][calc_seq[i]] = t2;
            smart->adjust_volt[c][ad_seq[i]] = calc_volt[calc_seq[i]](t2, c, smart->adjust_volt);
        }
    }

	// Gamma MTP Offset Setting, Output Gray Table (smart->ve)
	for (i = 0; i < AD_IVMAX; i++) {
        for(c=CI_RED;c<CI_MAX;c++){
			smart->ve[table_index].v[c] = smart->adjust_volt[c][i];
        }
        
        range_index = 0;
		for (j = table_index+1; j < table_index+range_table_count[i]; j++) {
			for (c = CI_RED; c < CI_MAX; c++) {
				if (smart->t_info[i].offset_table != NULL)
					ratio = smart->t_info[i].offset_table[range_index] * smart->t_info[i].rv;
				else
					ratio = (range_table_count[i]-(range_index+1)) * smart->t_info[i].rv;

				v1 = smart->adjust_volt[c][i+1] << 15;
				v2 = (smart->adjust_volt[c][i] - smart->adjust_volt[c][i+1])*ratio;
				smart->ve[j].v[c] = ((v1+v2) >> 15);
			}
			range_index++;
		}
		table_index = j;
	}


#if 1 

		printk("++++++++++++++++++++++++++++++ MTP VALUE ++++++++++++++++++++++++++++++\n");
		for(i=IV_1;i<IV_MAX;i++){
			printk("V Level : %d - ",i);
		for (c = CI_RED; c < CI_MAX; c++)
				printk("  %c : 0x%08x(%04d)",color_name[c],smart->mtp[c][i],smart->mtp[c][i]);
			printk("\n");
		}
		
		printk("\n\n++++++++++++++++++++++++++++++ ADJUST VALUE ++++++++++++++++++++++++++++++\n");
		for(i=IV_1;i<IV_MAX;i++){
			printk("V Level : %d - ",i);
		for (c = CI_RED; c < CI_MAX; c++)
				printk("  %c : 0x%08x(%04d)",color_name[c],
					adjust_mtp[c][i],adjust_mtp[c][i]);
			printk("\n");
		}
	
		printk("\n\n++++++++++++++++++++++++++++++ ADJUST VOLTAGE ++++++++++++++++++++++++++++++\n");
		for(i=AD_IV0;i<AD_IVMAX;i++){
			printk("V Level : %d - ",i);
		for (c = CI_RED; c < CI_MAX; c++)
			printk("  %c : %04dV", color_name[c], smart->adjust_volt[c][i]);
			printk("\n");
		}

		printk("\n\n++++++++++++++++++++++++++++++++++++++	VOLTAGE TABLE ++++++++++++++++++++++++++++++++++++++\n");
		for(i=0;i<256;i++){
			printk("Gray Level : %03d - ",i);
		for (c = CI_RED; c < CI_MAX; c++)
			printk("  %c : %04dV", color_name[c], smart->ve[i].v[c]);
			printk("\n");
		}
#endif    

    return 0;
}

int init_table_info(struct str_smart_dim *smart, unsigned char* srcGammaTable)
{
    int i;
    int offset = 0;
    for(i=0;i<IV_TABLE_MAX;i++){
        smart->t_info[i].count = range_table_count[i];
        smart->t_info[i].offset_table = offset_table[i];
        smart->t_info[i].rv = table_radio[i];
        offset += range_table_count[i];        
    }  
    smart->flooktbl = flookup_table;
    smart->g300_gra_tbl = gamma_300_gra_table;
    smart->g22_tbl = gamma_22_table;
    smart->default_gamma = srcGammaTable;

    return 0;
}

#define VALUE_DIM_1000 1024

u32 lookup_vtbl_idx(struct str_smart_dim *smart, u32 gamma)
{
    u32 lookup_index;
    u16 table_count, table_index;
    u32 gap,i;
    u32 minimum = smart->g300_gra_tbl[255];
    u32 candidate = 0;
    u32 offset = 0;
 
 
    //printk("Input Gamma Value : %d\n",gamma);

    lookup_index = (gamma/VALUE_DIM_1000)+1;
    if(lookup_index > MAX_GRADATION){ /* gamma = 300 */
		lookup_index = MAX_GRADATION;
		printk("Set To MAX Gradation");
	}

    //printk("lookup index : %d [%d] : %d \n",lookup_index, smart->flooktbl[lookup_index].entry, smart->flooktbl[lookup_index].count);
    
    if(smart->flooktbl[lookup_index].count) {
        if(smart->flooktbl[lookup_index-1].count){
            table_index = smart->flooktbl[lookup_index-1].entry;
            table_count = smart->flooktbl[lookup_index].count + smart->flooktbl[lookup_index-1].count;
        }
        else{
            table_index = smart->flooktbl[lookup_index].entry;
            table_count = smart->flooktbl[lookup_index].count;
        }
    }
    else{
        offset += 1;
        while(!(smart->flooktbl[lookup_index+offset].count || smart->flooktbl[lookup_index-offset].count)) {
            offset++;
        }
        
        if(smart->flooktbl[lookup_index-offset].count)
            table_index = smart->flooktbl[lookup_index-offset].entry;
        else 
            table_index = smart->flooktbl[lookup_index+offset].entry;
        table_count = smart->flooktbl[lookup_index+offset].count + smart->flooktbl[lookup_index-offset].count;
    }


    for(i=0;i<table_count;i++){
        if(gamma >= smart->g300_gra_tbl[table_index]) {
            gap = gamma - smart->g300_gra_tbl[table_index];
        }
        else {
            gap = smart->g300_gra_tbl[table_index]- gamma;
        }
	//printk("table index : %d [%d] , Gap :%d \n",table_index, smart->g300_gra_tbl[table_index],gap );

        if(gap == 0) {
            candidate = table_index; 
            break;
        }

        if (gap < minimum) {
            minimum = gap;
            candidate = table_index; 
        }
        table_index++;
    }
#if 0     
    printk("cal : found index  : %d\n",candidate);
    printk("gamma : %d, found index : %d found gamma : %d\n",
        gamma, candidate, smart->g300_gra_tbl[candidate]);
#endif
	//printk("candidate : %d\n",candidate);

    return candidate;
 
}

/*
V1 = 4.5 - 4.5(5+i)/600
600V1 = 2700 - 22.5 - 4.5i
i = (2700 - 22.5)/4.5 - 600V1/4.5
  = 595 - 133V1 
*/
u32 calc_v1_reg(int ci, u32 dv[CI_MAX][IV_MAX])
{
    u32 ret;
    u32 v1;

    v1 = dv[ci][IV_1];
    ret = (596 << 10) - (133 * v1);
    ret = ret >> 10;
    
    return ret;
    
    
}
u32 calc_v15_reg(int ci, u32 dv[CI_MAX][IV_MAX])
{
    u32 t1,t2;
    u32 v1, v15, v35;
    u32 ret;

    v1 = dv[ci][IV_1];
    v15 = dv[ci][IV_15];
    v35 = dv[ci][IV_35];

#if 0 
    t1 = (v1 - v15) * 1000;
    t2 = v1 - v35;
    
    ret = 320*(t1/t2)-(20*1000);

    ret = ret/1000;
#else
    t1 = (v1 - v15) << 10;
	t2 = (v1 - v35) ? (v1 - v35) : (v1) ? v1 : 1;
    ret = (320 * (t1/t2)) - (20 << 10);
    ret >>= 10;
    
#endif
    return ret;
    
}
u32 calc_v35_reg(int ci, u32 dv[CI_MAX][IV_MAX])
{
    u32 t1,t2;
    u32 v1, v35, v57;
    u32 ret;

    v1 = dv[ci][IV_1];
    v35 = dv[ci][IV_35];
    v57 = dv[ci][IV_59];

#if 0 
    t1 = (v1 - v35) * 1000;
    t2 = v1 - v57;
    ret = 320*(t1/t2) - (65 * 1000);

    ret = ret/1000;
#else
    t1 = (v1 - v35) << 10;
	t2 = (v1 - v57) ? (v1 - v57) : (v1) ? v1 : 1;
    ret = (320 * (t1/t2)) - (64 << 10);

    ret >>= 10;
#endif
    
    return ret;
}

u32 calc_v59_reg(int ci, u32 dv[CI_MAX][IV_MAX])
{
    u32 t1,t2;
    u32 v1, v57, v87;
    u32 ret;

    v1 = dv[ci][IV_1];
    v57 = dv[ci][IV_59];
    v87 = dv[ci][IV_87];
    
#if 0 
    t1 = (v1 - v57) * 1000;
    t2 = v1 - v87;
    ret = 320*(t1/t2) - (65 * 1000);
    ret = ret/1000;
#else
    t1 = (v1 - v57) << 10;
	t2 = (v1 - v87) ? (v1 - v87) : (v1) ? v1 : 1;
    ret = (320 * (t1/t2)) - (64 << 10);
    ret >>= 10;
#endif
    return ret;
}
u32 calc_v87_reg(int ci, u32 dv[CI_MAX][IV_MAX])
{
    u32 t1,t2;
    u32 v1, v87, v171;
    u32 ret;

    v1 = dv[ci][IV_1];
    v87 = dv[ci][IV_87];
    v171 = dv[ci][IV_171];
    
#if 0 
    t1 = (v1 - v87) * 1000;
    t2 = v1 - v171;
    ret = 320*(t1/t2) - (65 * 1000);
    ret = ret/1000;
#else
    t1 = (v1 - v87) << 10;
	t2 = (v1 - v171) ? (v1 - v171) : (v1) ? v1 : 1;
    ret = (320 * (t1/t2)) - (64 << 10);
    ret >>= 10;
#endif

    return ret;
}


u32 calc_v171_reg(int ci, u32 dv[CI_MAX][IV_MAX])
{
    u32 t1,t2;
    u32 v1, v171, v255;
    u32 ret;

    v1 = dv[ci][IV_1];
    v171 = dv[ci][IV_171];
    v255 = dv[ci][IV_255];
    
#if 0 
    t1 = (v1 - v171) * 1000;
    t2 = v1 - v255;
    ret = 320*(t1/t2) - (65 * 1000);
    ret = ret/1000;
#else
    t1 = (v1 - v171) << 10;
	t2 = (v1 - v255) ? (v1 - v255) : (v1) ? v1 : 1;
    ret = (320 * (t1/t2)) - (64 << 10);
    ret >>= 10;
#endif

    return ret;
}



/*
V255 = 4.5 - 4.5(100+i)/600
600V255 = 2700 - 450 - 4.8i
i = (2880-450)/4.5 - 600V255/4.5
  = 500 - 133*V255
*/


u32 calc_v255_reg(int ci, u32 dv[CI_MAX][IV_MAX])
{
    u32 ret;
    u32 v255;

    v255 = dv[ci][IV_255];
    
    ret = (500 << 10 ) - (133 * v255);
    ret = ret >> 10;

    return ret;
}


u32 calc_gamma_table(struct str_smart_dim *smart, u32 gv, u8 result[])
{
    u32 i,c;
    u32 temp;
    u32 lidx;
    u32 dv[CI_MAX][IV_MAX];
    s16 gamma[CI_MAX][IV_MAX] = {0, };
    u16 offset;
    const u32(*calc_reg[IV_MAX])(int ci, u32 dv[CI_MAX][IV_MAX]) = 
    {
        calc_v1_reg,
        calc_v15_reg,
        calc_v35_reg,
        calc_v59_reg,
        calc_v87_reg,
        calc_v171_reg,
        calc_v255_reg,
    };

	memset(gamma, 0, sizeof(gamma));
    for(c=CI_RED;c<CI_MAX;c++){
            dv[c][0] = smart->adjust_volt[c][AD_IV1];
    }
    
    
    for(i=IV_15;i<IV_MAX;i++){
        temp = smart->g22_tbl[dv_value[i]] * gv;
        lidx = lookup_vtbl_idx(smart, temp);
        for(c=CI_RED;c<CI_MAX;c++){
            dv[c][i] = smart->ve[lidx].v[c];
        }
    }

    // for IV1 does not calculate value
    // just use default gamma value (IV1)
    for(c=CI_RED;c<CI_MAX;c++){
        gamma[c][IV_1] = smart->default_gamma[IV_TABLE_MAX * c + IV_MAX -IV_1];
    }
        
    for(i=IV_15;i<IV_MAX;i++){
        for(c=CI_RED;c<CI_MAX;c++){
            gamma[c][i] = (s16)calc_reg[i](c,dv) - smart->mtp[c][i];
    }
    }

    for(c=CI_RED;c<CI_MAX;c++){
        offset = (IV_TABLE_MAX * c);
        result[offset] = (gamma[c][IV_255] & 0xff00) >> 8;
        result[offset+1] = gamma[c][IV_255] & 0xff;
    }

    for(c=CI_RED;c<CI_MAX;c++){
        for(i=IV_1;i<IV_255;i++){
            result[(IV_MAX-i)+(IV_TABLE_MAX * c)] = gamma[c][i];
        }
    }
#if 0

    printk("++++++++++++++++++++++++++++++ MTP VALUE ++++++++++++++++++++++++++++++\n");
    for(i=IV_1;i < IV_MAX;i++){
        printk("V Level : %d - ",i);
        for(c=CI_RED;c<CI_MAX;c++){
            printk("  %c : 0x%08x(%04d)",color_name[c],smart->mtp[c][i],smart->mtp[c][i]);
        }
        printk("\n");
    }

	printk("++++++++++++++++++++++++++++++ MTP READ VALUE ++++++++++++++++++++++++++++++\n");
	for(i=0; i<7; i++)
		{
		extern unsigned char lcd_mtp_data[];
		printk( "[R]: %02x    [G]: %02x    [B] : %02x \n",lcd_mtp_data[i], lcd_mtp_data[i+7], lcd_mtp_data[i+14]);
		}
	printk("\n");

    printk("\n\n++++++++++++++++++++++++++++++ FOUND VOLTAGE ++++++++++++++++++++++++++++++\n");
    for(i=IV_1;i<IV_MAX;i++){
        printk("V Level : %d - ",i);
        for(c=CI_RED;c<CI_MAX;c++){
            printk("  %c : %04dV",color_name[c],
                dv[c][i]);
        }
        printk("\n");
    }

    
    printk("\n\n++++++++++++++++++++++++++++++ FOUND REG ++++++++++++++++++++++++++++++\n");
    for(i=IV_1;i<IV_MAX;i++){
        printk("V Level : %d - ",i);
        for(c=CI_RED;c<CI_MAX;c++){
            printk("  %c : %3d, 0x%2x",color_name[c],
                gamma[c][i],gamma[c][i]);
        }
        printk("\n");
    }

 printk("\n\n++++++++++++++++++++++++++++++ ADJUST VOLTAGE ++++++++++++++++++++++++++++++\n");
 for(i=AD_IV0;i<AD_IVMAX;i++){
	 printk("V Level : %d - ",i);
	 for(c=CI_RED;c<CI_MAX;c++){
		 printk("  %c : %04dV",color_name[c],
			 smart->adjust_volt[c][i]);
	 }
	 printk("\n");
 }

     printk("\n\n++++++++++++++++++++++++++++++++++++++  VOLTAGE TABLE ++++++++++++++++++++++++++++++++++++++\n");
    for(i=0;i<256;i++){
        printk("Gray Level : %03d - ",i);
        for(c=CI_RED;c<CI_MAX;c++){
            printk("  %c : %04d mV",color_name[c],
                smart->ve[i].v[c]);   
        }
        printk("\n");
    }
#endif
     return 0;    
}
