/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef MIPI_S6E8AA0_HD720_SEQ_H
#define MIPI_S6E8AA0_HD720_SEQ_H


struct dsi_cmd_desc_LCD {
	int lux;
	char strID[8];
	struct dsi_cmd_desc *cmd;
};


#ifdef MAPPING_TBL_AUTO_BRIGHTNESS
#define CANDELA_TABLE_SIZE 24
static const unsigned int candela_table[CANDELA_TABLE_SIZE] = {
	 30,  40,  50,  60,  70,  80,  90, 100, 110, 120,
	130, 140, 150, 160, 170, 180, 190, 200, 210, 220,
	230, 240, 250, 300
};
#endif


#if defined (CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
#define MAX_GAMMA_VALUE 25
#else
#define MAX_GAMMA_VALUE 24
#endif

#ifdef MAPPING_TBL_AUTO_BRIGHTNESS
#undef MAX_GAMMA_VALUE
#define MAX_GAMMA_VALUE CANDELA_TABLE_SIZE
#endif




// updated 11.06.28
//static char GAMMA_8500K_COND_SET_0[]={ 0xFA, 0x01, 	0x0, 	0x0, 	0x0, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00,	0x00, 	0x00,	0x00, 	0x00,	0x00, 	};
static char GAMMA_8500K_COND_SET_30[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0x0, 	0x1, 	0x7F, 	0xBD, 	0x77, 	0xCD, 	0xD2, 	0xBF, 	0xD5, 	0xB2, 	0xAA, 	0xB3, 	0xCF, 	0xCD, 	0xCC, 	0x0, 	0x33, 	0x0, 	0x2B, 	0x0, 	0x50};
static char GAMMA_8500K_COND_SET_40[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0x0, 	0x0, 	0xA1, 	0xC0, 	0x91, 	0xCB, 	0xD1, 	0xC3, 	0xD4, 	0xB2, 	0xAC, 	0xB3, 	0xCD, 	0xCC, 	0xCC, 	0x0, 	0x3B, 	0x0, 	0x33, 	0x0, 	0x58};
//static char GAMMA_8500K_COND_SET_50[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0x0, 	0x0, 	0xC0, 	0xBE, 	0x9A, 	0xC9, 	0xD1, 	0xC7, 	0xD3, 	0xB3, 	0xAE, 	0xB1, 	0xCC, 	0xCA, 	0xCA, 	0x0, 	0x42, 	0x0, 	0x3A, 	0x0, 	0x62};
//static char GAMMA_8500K_COND_SET_60[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0x2B, 	0x0, 	0xD3, 	0xC1, 	0xA3, 	0xC8, 	0xD1, 	0xC7, 	0xD3, 	0xB3, 	0xAE, 	0xB2, 	0xCB, 	0xCA, 	0xCA, 	0x0, 	0x47, 	0x0, 	0x3F, 	0x0, 	0x67};
static char GAMMA_8500K_COND_SET_70[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0x7B, 	0x0, 	0xD7, 	0xC2, 	0xA8, 	0xC8, 	0xD0, 	0xC8, 	0xD2, 	0xB2, 	0xAF, 	0xB1, 	0xCA, 	0xCA, 	0xCA, 	0x0, 	0x4C, 	0x0, 	0x43, 	0x0, 	0x6D};
//static char GAMMA_8500K_COND_SET_80[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0x9B, 	0x0, 	0xD9, 	0xC1, 	0xAB, 	0xC7, 	0xD2, 	0xCA, 	0xD3, 	0xB2, 	0xAE, 	0xB1, 	0xC7, 	0xC9, 	0xC7, 	0x0, 	0x52, 	0x0, 	0x48, 	0x0, 	0x74};
static char GAMMA_8500K_COND_SET_90[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xD2, 	0x0, 	0xE9, 	0xC2, 	0xAD, 	0xC7, 	0xD1, 	0xCA, 	0xD1, 	0xB2, 	0xAF, 	0xB2, 	0xC6, 	0xC8, 	0xC6, 	0x0, 	0x56, 	0x0, 	0x4C, 	0x0, 	0x79};
static char GAMMA_8500K_COND_SET_100[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xBD, 	0x0, 	0xE0, 	0xC1, 	0xB0, 	0xC7, 	0xD0, 	0xCB, 	0xD1, 	0xB1, 	0xAF, 	0xAF, 	0xC9, 	0xC9, 	0xC7, 	0x0, 	0x5A, 	0x0, 	0x4F, 	0x0, 	0x7E};
static char GAMMA_8500K_COND_SET_110[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xC5, 	0x0, 	0xE1, 	0xC2, 	0xB2, 	0xC5, 	0xD1, 	0xCC, 	0xD2, 	0xB0, 	0xAE, 	0xAF, 	0xC7, 	0xC8, 	0xC6, 	0x0, 	0x5E, 	0x0, 	0x53, 	0x0, 	0x83};
static char GAMMA_8500K_COND_SET_120[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xC7, 	0x27, 	0xE0, 	0xC1, 	0xB2, 	0xC5, 	0xD0, 	0xCC, 	0xD2, 	0xB0, 	0xAE, 	0xAE, 	0xC7, 	0xC7, 	0xC5, 	0x0, 	0x61, 	0x0, 	0x56, 	0x0, 	0x87};
static char GAMMA_8500K_COND_SET_130[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xC9, 	0x3A, 	0xE1, 	0xC1, 	0xB3, 	0xC5, 	0xD2, 	0xCD, 	0xD1, 	0xB0, 	0xAD, 	0xAE, 	0xC6, 	0xC7, 	0xC5, 	0x0, 	0x64, 	0x0, 	0x59, 	0x0, 	0x8B};
static char GAMMA_8500K_COND_SET_140[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xC7, 	0x56, 	0xDF, 	0xC2, 	0xB5, 	0xC5, 	0xD0, 	0xCC, 	0xCF, 	0xB1, 	0xAE, 	0xAF, 	0xC5, 	0xC6, 	0xC4, 	0x0, 	0x67, 	0x0, 	0x5C, 	0x0, 	0x8F};
static char GAMMA_8500K_COND_SET_150[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xC8, 	0x65, 	0xDE, 	0xC4, 	0xB6, 	0xC5, 	0xD1, 	0xCC, 	0xD0, 	0xAE, 	0xAD, 	0xAE, 	0xC4, 	0xC6, 	0xC2, 	0x0, 	0x6B, 	0x0, 	0x5F, 	0x0, 	0x94};
static char GAMMA_8500K_COND_SET_160[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xCA, 	0x6E, 	0xDE, 	0xC2, 	0xB6, 	0xC4, 	0xD2, 	0xCD, 	0xD2, 	0xAE, 	0xAD, 	0xAD, 	0xC2, 	0xC5, 	0xC0, 	0x0, 	0x6E, 	0x0, 	0x62, 	0x0, 	0x98};
static char GAMMA_8500K_COND_SET_170[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xC8, 	0x7C, 	0xDD, 	0xC3, 	0xB7, 	0xC4, 	0xD1, 	0xCD, 	0xD1, 	0xAD, 	0xAD, 	0xAC, 	0xC4, 	0xC5, 	0xC1, 	0x0, 	0x70, 	0x0, 	0x64, 	0x0, 	0x9B};
static char GAMMA_8500K_COND_SET_180[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xCC, 	0x7A, 	0xDB, 	0xC4, 	0xB7, 	0xC4, 	0xD2, 	0xCD, 	0xD2, 	0xAC, 	0xAD, 	0xAC, 	0xC2, 	0xC4, 	0xBF, 	0x0, 	0x74, 	0x0, 	0x67, 	0x0, 	0xA0};
static char GAMMA_8500K_COND_SET_190[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xCC, 	0x80, 	0xDC, 	0xC3, 	0xB8, 	0xC4, 	0xD0, 	0xCD, 	0xD1, 	0xAC, 	0xAD, 	0xAB, 	0xC4, 	0xC5, 	0xC0, 	0x0, 	0x76, 	0x0, 	0x69, 	0x0, 	0xA3};
static char GAMMA_8500K_COND_SET_200[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xCC, 	0x85, 	0xDC, 	0xC2, 	0xB8, 	0xC4, 	0xCF, 	0xCD, 	0xCF, 	0xAD, 	0xAD, 	0xAA, 	0xC5, 	0xC4, 	0xC1, 	0x0, 	0x78, 	0x0, 	0x6B, 	0x0, 	0xA6};
static char GAMMA_8500K_COND_SET_210[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xCC, 	0x89, 	0xDC, 	0xC4, 	0xB9, 	0xC2, 	0xD0, 	0xCD, 	0xD0, 	0xAE, 	0xAC, 	0xAB, 	0xC2, 	0xC3, 	0xC0, 	0x0, 	0x7A, 	0x0, 	0x6E, 	0x0, 	0xA9};
static char GAMMA_8500K_COND_SET_220[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xCD, 	0x8C, 	0xDB, 	0xC4, 	0xB9, 	0xC4, 	0xCE, 	0xCD, 	0xCF, 	0xAE, 	0xAD, 	0xAB, 	0xC1, 	0xC2, 	0xBE, 	0x0, 	0x7D, 	0x0, 	0x70, 	0x0, 	0xAC};
static char GAMMA_8500K_COND_SET_230[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xCD, 	0x90, 	0xDA, 	0xC3, 	0xB9, 	0xC4, 	0xD1, 	0xCE, 	0xD0, 	0xAC, 	0xAC, 	0xAA, 	0xC0, 	0xC1, 	0xBD, 	0x0, 	0x80, 	0x0, 	0x73, 	0x0, 	0xB0};
static char GAMMA_8500K_COND_SET_240[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xCE, 	0x94, 	0xDA, 	0xC2, 	0xBA, 	0xC3, 	0xD0, 	0xCE, 	0xD0, 	0xAD, 	0xAB, 	0xAA, 	0xC0, 	0xC2, 	0xBD, 	0x0, 	0x82, 	0x0, 	0x75, 	0x0, 	0xB3};
static char GAMMA_8500K_COND_SET_250[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xCD, 	0x97, 	0xDA, 	0xC3, 	0xBB, 	0xC3, 	0xCF, 	0xCD, 	0xCF, 	0xAC, 	0xAB, 	0xA9, 	0xC1, 	0xC2, 	0xBD, 	0x0, 	0x84, 	0x0, 	0x76, 	0x0, 	0xB6};
static char GAMMA_8500K_COND_SET_260[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xCF, 	0x9A, 	0xDA, 	0xC1, 	0xBA, 	0xC3, 	0xCF, 	0xCE, 	0xCF, 	0xAA, 	0xAB, 	0xA8, 	0xC0, 	0xC1, 	0xBB, 	0x0, 	0x87, 	0x0, 	0x79, 	0x0, 	0xBA};
static char GAMMA_8500K_COND_SET_270[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xCE, 	0x9B, 	0xDA, 	0xC3, 	0xBA, 	0xC3, 	0xCF, 	0xCE, 	0xCF, 	0xAA, 	0xAA, 	0xA7, 	0xBF, 	0xC0, 	0xBC, 	0x0, 	0x89, 	0x0, 	0x7B, 	0x0, 	0xBC};
static char GAMMA_8500K_COND_SET_280[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xCE, 	0x9D, 	0xD8, 	0xC2, 	0xBB, 	0xC3, 	0xCF, 	0xCD, 	0xCF, 	0xAA, 	0xAB, 	0xA8, 	0xBE, 	0xC0, 	0xBA, 	0x0, 	0x8B, 	0x0, 	0x7D, 	0x0, 	0xBF};
static char GAMMA_8500K_COND_SET_290[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xCE, 	0x9E, 	0xD8, 	0xC2, 	0xBB, 	0xC3, 	0xCE, 	0xCD, 	0xCE, 	0xAA, 	0xAB, 	0xA7, 	0xC0, 	0xBF, 	0xBB, 	0x0, 	0x8D, 	0x0, 	0x7F, 	0x0, 	0xC2};
static char GAMMA_8500K_COND_SET_300[]={ 0xFA, 0x01, 	0x0F, 	0x0F, 	0x0F, 	0xCC, 	0x9C, 	0xD7, 	0xC1, 	0xBA, 	0xC1, 	0xCF, 	0xCD, 	0xCF, 	0xAA, 	0xAA, 	0xA7, 	0xBE, 	0xBE, 	0xBA, 	0x0, 	0x90, 	0x0, 	0x81, 	0x0, 	0xC5};

//static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_0 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_0), GAMMA_8500K_COND_SET_0};  
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_30 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_30), GAMMA_8500K_COND_SET_30};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_40 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_40), GAMMA_8500K_COND_SET_40};
//static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_50 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_50), GAMMA_8500K_COND_SET_50};
//static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_60 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_60), GAMMA_8500K_COND_SET_60};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_70 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_70), GAMMA_8500K_COND_SET_70};
//static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_80 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_80), GAMMA_8500K_COND_SET_80};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_90 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_90), GAMMA_8500K_COND_SET_90};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_100 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_100), GAMMA_8500K_COND_SET_100};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_110 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_110), GAMMA_8500K_COND_SET_110};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_120 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_120), GAMMA_8500K_COND_SET_120};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_130 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_130), GAMMA_8500K_COND_SET_130};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_140 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_140), GAMMA_8500K_COND_SET_140};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_150 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_150), GAMMA_8500K_COND_SET_150};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_160 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_160), GAMMA_8500K_COND_SET_160};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_170 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_170), GAMMA_8500K_COND_SET_170};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_180 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_180), GAMMA_8500K_COND_SET_180};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_190 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_190), GAMMA_8500K_COND_SET_190};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_200 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_100), GAMMA_8500K_COND_SET_200};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_210 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_210), GAMMA_8500K_COND_SET_210};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_220 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_220), GAMMA_8500K_COND_SET_220};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_230 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_230), GAMMA_8500K_COND_SET_230};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_240 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_240), GAMMA_8500K_COND_SET_240};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_250 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_250), GAMMA_8500K_COND_SET_250};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_260 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_260), GAMMA_8500K_COND_SET_260};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_270 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_270), GAMMA_8500K_COND_SET_270};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_280 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_280), GAMMA_8500K_COND_SET_280};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_290 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_290), GAMMA_8500K_COND_SET_290};
static struct dsi_cmd_desc DSI_CMD_GAMMA_8500K_300 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_8500K_COND_SET_300), GAMMA_8500K_COND_SET_300};

static struct dsi_cmd_desc_LCD lcd_brightness_8500k_table[MAX_GAMMA_VALUE+1] = {
{ 30, "85K", &DSI_CMD_GAMMA_8500K_30},   // 0 = 30_dimming,
{ 40, "85K", &DSI_CMD_GAMMA_8500K_40},   // 1 = 40
{ 70, "85K", &DSI_CMD_GAMMA_8500K_70},   // 2 = 70
//{ 80, "85K", &DSI_CMD_GAMMA_8500K_80},   //  3 = 90,
{ 90, "85K", &DSI_CMD_GAMMA_8500K_90},   //  3 = 90,
{ 100, "85K",&DSI_CMD_GAMMA_8500K_100},   // 4 = 100,
{ 110, "85K",&DSI_CMD_GAMMA_8500K_110},   // 5 = 110,
{ 120, "85K",&DSI_CMD_GAMMA_8500K_120},   // 6 = 120,
{ 130, "85K",&DSI_CMD_GAMMA_8500K_130},   // 7 = 130,
{ 140, "85K",&DSI_CMD_GAMMA_8500K_140},   // 8 = 140,
{ 150, "85K",&DSI_CMD_GAMMA_8500K_150},   // 9 = 150,
{ 160, "85K",&DSI_CMD_GAMMA_8500K_160},   // 10= 160,
{ 170, "85K",&DSI_CMD_GAMMA_8500K_170},   // 11= 170,
{ 180, "85K",&DSI_CMD_GAMMA_8500K_180},   // 12= 180,
{ 190, "85K",&DSI_CMD_GAMMA_8500K_190},   // 13= 190,
{ 200, "85K",&DSI_CMD_GAMMA_8500K_200},   // 14= 200,
{ 210, "85K",&DSI_CMD_GAMMA_8500K_210},   // 15= 210,
{ 220, "85K",&DSI_CMD_GAMMA_8500K_220},   // 16= 220,
{ 230, "85K",&DSI_CMD_GAMMA_8500K_230},   // 17= 230,
{ 240, "85K",&DSI_CMD_GAMMA_8500K_240},   // 18= 240,
{ 250, "85K",&DSI_CMD_GAMMA_8500K_250},   // 19= 250,
{ 260, "85K",&DSI_CMD_GAMMA_8500K_260},   // 20= 260,
{ 270, "85K",&DSI_CMD_GAMMA_8500K_270},   // 21= 270,
{ 280, "85K",&DSI_CMD_GAMMA_8500K_280},   // 22= 280,
{ 290, "85K",&DSI_CMD_GAMMA_8500K_290},   // 23= 290,
{ 300, "85K",&DSI_CMD_GAMMA_8500K_300}   // 24= 300,
};




// Updated 110802
static char GAMMA_7500K_COND_SET_300[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x87,	0xD0,	0xA4,	0xA1,	0xCE,	0xA7,	0xBC,	0xDA,	0xBD,	0x94,	0xBA,	0x92,	0xB0,	0xCA,	0xAF,	0x00,	0xA2,	0x00,	0x92,	0x00,	0xCF,	};	
static char GAMMA_7500K_COND_SET_290[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x89,	0xBF,	0xA5,	0xA2,	0xCD,	0xA8,	0xBB,	0xDA,	0xBE,	0x93,	0xBB,	0x91,	0xB3,	0xCB,	0xB1,	0x00,	0xA0,	0x00,	0x90,	0x00,	0xCC,	};	
static char GAMMA_7500K_COND_SET_280[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x89,	0xBD,	0xA4,	0xA3,	0xCD,	0xA7,	0xBD,	0xDA,	0xC0,	0x92,	0xBC,	0x92,	0xB4,	0xCB,	0xB1,	0x00,	0x9D,	0x00,	0x8E,	0x00,	0xC9,	};	
static char GAMMA_7500K_COND_SET_270[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x8B,	0xBD,	0xA6,	0xA2,	0xCD,	0xA6,	0xBF,	0xDB,	0xC0,	0x91,	0xBA,	0x92,	0xB5,	0xCD,	0xB1,	0x00,	0x9B,	0x00,	0x8C,	0x00,	0xC7,	};	
static char GAMMA_7500K_COND_SET_260[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x89,	0xBA,	0xA5,	0xA1,	0xCD,	0xA6,	0xBE,	0xDB,	0xBE,	0x94,	0xBC,	0x94,	0xB4,	0xCC,	0xB1,	0x00,	0x99,	0x00,	0x8A,	0x00,	0xC4,	};	
static char GAMMA_7500K_COND_SET_250[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x89,	0xB7,	0xA5,	0xA2,	0xCD,	0xA7,	0xBE,	0xDA,	0xBF,	0x93,	0xBB,	0x93,	0xB6,	0xCE,	0xB3,	0x00,	0x96,	0x00,	0x88,	0x00,	0xC1,	};	
static char GAMMA_7500K_COND_SET_240[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x87,	0xB4,	0xA5,	0xA0,	0xCD,	0xA6,	0xBF,	0xDB,	0xBF,	0x92,	0xBC,	0x92,	0xB9,	0xCF,	0xB5,	0x00,	0x94,	0x00,	0x85,	0x00,	0xBE,	};	
static char GAMMA_7500K_COND_SET_230[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x87,	0xB1,	0xA6,	0x9F,	0xCC,	0xA6,	0xBF,	0xDC,	0xBF,	0x92,	0xBC,	0x93,	0xB7,	0xCE,	0xB4,	0x00,	0x92,	0x00,	0x84,	0x00,	0xBB,	};	
static char GAMMA_7500K_COND_SET_220[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x85,	0xAD,	0xA4,	0xA1,	0xCD,	0xA7,	0xBD,	0xDA,	0xBD,	0x95,	0xBD,	0x95,	0xB7,	0xCE,	0xB5,	0x00,	0x8F,	0x00,	0x82,	0x00,	0xB8,	};	
static char GAMMA_7500K_COND_SET_210[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x85,	0xA7,	0xA6,	0xA0,	0xCB,	0xA7,	0xBC,	0xDB,	0xBF,	0x92,	0xBD,	0x93,	0xB8,	0xCF,	0xB4,	0x00,	0x8E,	0x00,	0x80,	0x00,	0xB5,	};	
static char GAMMA_7500K_COND_SET_200[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x82,	0xA0,	0xA4,	0xA0,	0xCC,	0xA6,	0xBE,	0xDB,	0xBF,	0x93,	0xBD,	0x94,	0xB8,	0xD1,	0xB5,	0x00,	0x8B,	0x00,	0x7D,	0x00,	0xB2,	};	
static char GAMMA_7500K_COND_SET_190[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x81,	0x98,	0xA5,	0x9D,	0xCB,	0xA5,	0xBE,	0xDB,	0xBE,	0x95,	0xBE,	0x95,	0xB8,	0xD0,	0xB6,	0x00,	0x89,	0x00,	0x7B,	0x00,	0xAF,	};	
static char GAMMA_7500K_COND_SET_180[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x80,	0x8C,	0xA5,	0x9E,	0xCB,	0xA7,	0xBC,	0xDB,	0xBE,	0x94,	0xBE,	0x95,	0xB9,	0xD1,	0xB7,	0x00,	0x86,	0x00,	0x79,	0x00,	0xAB,	};	
static char GAMMA_7500K_COND_SET_170[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x7E,	0x7B,	0xA4,	0x9E,	0xCB,	0xA5,	0xBE,	0xDB,	0xBF,	0x94,	0xBE,	0x96,	0xB9,	0xD1,	0xB7,	0x00,	0x83,	0x00,	0x77,	0x00,	0xA8,	};	
static char GAMMA_7500K_COND_SET_160[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x7A,	0x5D,	0xA5,	0x9C,	0xCA,	0xA4,	0xBD,	0xDC,	0xBE,	0x93,	0xBD,	0x95,	0xBA,	0xD2,	0xB7,	0x00,	0x81,	0x00,	0x75,	0x00,	0xA5,	};	
static char GAMMA_7500K_COND_SET_150[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x76,	0x00,	0xA4,	0x9B,	0xCA,	0xA5,	0xBA,	0xDB,	0xBC,	0x93,	0xBF,	0x95,	0xBB,	0xD3,	0xB7,	0x00,	0x7F,	0x00,	0x71,	0x00,	0xA2,	};	
static char GAMMA_7500K_COND_SET_140[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x6C,	0x00,	0x9E,	0x9B,	0xC9,	0xA5,	0xBA,	0xDA,	0xBD,	0x93,	0xC0,	0x96,	0xBA,	0xD3,	0xB7,	0x00,	0x7C,	0x00,	0x6F,	0x00,	0x9E,	};	
static char GAMMA_7500K_COND_SET_130[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x5C,	0x00,	0x97,	0x9C,	0xC9,	0xA5,	0xBB,	0xDA,	0xBD,	0x94,	0xBF,	0x97,	0xBA,	0xD4,	0xB9,	0x00,	0x78,	0x00,	0x6C,	0x00,	0x99,	};	
static char GAMMA_7500K_COND_SET_120[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x4A,	0x00,	0x92,	0x98,	0xC7,	0xA2,	0xBA,	0xDB,	0xBC,	0x96,	0xBF,	0x97,	0xBA,	0xD5,	0xBA,	0x00,	0x75,	0x00,	0x69,	0x00,	0x95,	};	
static char GAMMA_7500K_COND_SET_110[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x33,	0x00,	0x8B,	0x97,	0xC7,	0xA2,	0xB8,	0xDA,	0xBB,	0x96,	0xC0,	0x97,	0xBB,	0xD5,	0xBB,	0x00,	0x72,	0x00,	0x66,	0x00,	0x91,	};	
static char GAMMA_7500K_COND_SET_100[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x12,	0x00,	0x84,	0x95,	0xC6,	0xA2,	0xB7,	0xD9,	0xBA,	0x95,	0xC1,	0x96,	0xBD,	0xD6,	0xBC,	0x00,	0x6E,	0x00,	0x62,	0x00,	0x8D,	};	
static char GAMMA_7500K_COND_SET_90[]={ 0xFA, 0x01,	0x0F,	0x0F,	0x0F,	0x82,	0x1E,	0xED,	0xCB,	0xBC,	0xCF,	0xDB,	0xD4,	0xD9,	0xBB,	0xBB,	0xB9,	0xD1,	0xD3,	0xCF,	0x00,	0x6A,	0x00,	0x5F,	0x00,	0x89,	};	
static char GAMMA_7500K_COND_SET_70[]={ 0xFA, 0x01,	0x0F,	0x0F,	0x0F,	0x53,	0x00,	0xDE,	0xCC,	0xB5,	0xD1,	0xDA,	0xD3,	0xDA,	0xB9,	0xBB,	0xB9,	0xD3,	0xD5,	0xD0,	0x00,	0x63,	0x00,	0x57,	0x00,	0x7F,	};	
static char GAMMA_7500K_COND_SET_40[]={ 0xFA, 0x01,	0x0F,	0x0F,	0x0F,	0x00,	0x00,	0xB4,	0xCE,	0x99,	0xD4,	0xDA,	0xCE,	0xDC,	0xBD,	0xBA,	0xBD,	0xD4,	0xD6,	0xD3,	0x00,	0x52,	0x00,	0x48,	0x00,	0x6A,	};	
static char GAMMA_7500K_COND_SET_30[]={ 0xFA, 0x01,	0x0F,	0x0F,	0x0F,	0x00,	0x01,	0x9F,	0xCC,	0x6C,	0xD5,	0xDD,	0xC9,	0xDE,	0xBC,	0xB8,	0xBD,	0xD5,	0xD7,	0xD5,	0x00,	0x4B,	0x00,	0x41,	0x00,	0x61,	};	
//static char GAMMA_7500K_COND_SET_0[]={ 0xFA, 0x01, 	0x0, 	0x0, 	0x0, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00, 	0x00,	0x00, 	0x00,	0x00, 	0x00,	0x00, 	};

//static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_0 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_0), GAMMA_7500K_COND_SET_0};  
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_30 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_30), GAMMA_7500K_COND_SET_30};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_40 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_40), GAMMA_7500K_COND_SET_40};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_70 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_70), GAMMA_7500K_COND_SET_70};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_90 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_90), GAMMA_7500K_COND_SET_90};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_100 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_100), GAMMA_7500K_COND_SET_100};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_110 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_110), GAMMA_7500K_COND_SET_110};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_120 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_120), GAMMA_7500K_COND_SET_120};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_130 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_130), GAMMA_7500K_COND_SET_130};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_140 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_140), GAMMA_7500K_COND_SET_140};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_150 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_150), GAMMA_7500K_COND_SET_150};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_160 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_160), GAMMA_7500K_COND_SET_160};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_170 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_170), GAMMA_7500K_COND_SET_170};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_180 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_180), GAMMA_7500K_COND_SET_180};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_190 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_190), GAMMA_7500K_COND_SET_190};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_200 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_100), GAMMA_7500K_COND_SET_200};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_210 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_210), GAMMA_7500K_COND_SET_210};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_220 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_220), GAMMA_7500K_COND_SET_220};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_230 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_230), GAMMA_7500K_COND_SET_230};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_240 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_240), GAMMA_7500K_COND_SET_240};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_250 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_250), GAMMA_7500K_COND_SET_250};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_260 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_260), GAMMA_7500K_COND_SET_260};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_270 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_270), GAMMA_7500K_COND_SET_270};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_280 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_280), GAMMA_7500K_COND_SET_280};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_290 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_290), GAMMA_7500K_COND_SET_290};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_300 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_300), GAMMA_7500K_COND_SET_300};

#if defined (CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
static char GAMMA_7500K_COND_SET_80[]={ 0xFA, 0x01,	0x0F,	0x0F,	0x0F,	0x7B,	0x00,	0xE6,	0xCC,	0xB9,	0xD1,	0xD9,	0xD4,	0xD9,	0xBA,	0xBB,	0xB8,	0xD3,	0xD4,	0xD0,	0x00,	0x67,	0x00,	0x5B,	0x00,	0x84,	};	
static char GAMMA_7500K_COND_SET_60[]={ 0xFA, 0x01,	0x0F,	0x0F,	0x0F,	0x00,	0x00,	0xD4,	0xCD,	0xAF,	0xD2,	0xD9,	0xD2,	0xDA,	0xBB,	0xBB,	0xBB,	0xD3,	0xD5,	0xD0,	0x00,	0x5E,	0x00,	0x53,	0x00,	0x79,	};	
static char GAMMA_7500K_COND_SET_50[]={ 0xFA, 0x01,	0x0F,	0x0F,	0x0F,	0x00,	0x00,	0xC3,	0xCE,	0xA8,	0xD3,	0xD9,	0xD1,	0xDA,	0xBB,	0xBA,	0xBB,	0xD3,	0xD5,	0xD1,	0x00,	0x59,	0x00,	0x4E,	0x00,	0x73,	};	

static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_50 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_50), GAMMA_7500K_COND_SET_50};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_60 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_60), GAMMA_7500K_COND_SET_60};
static struct dsi_cmd_desc DSI_CMD_GAMMA_7500K_80 = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_7500K_COND_SET_80), GAMMA_7500K_COND_SET_80};

static struct dsi_cmd_desc_LCD lcd_brightness_7500k_table[MAX_GAMMA_VALUE+1] = {
{ 30, "75K", &DSI_CMD_GAMMA_7500K_30},   // 0 = 30_dimming,
{ 40, "75K", &DSI_CMD_GAMMA_7500K_40},   // 1 =  40
{ 50, "75K", &DSI_CMD_GAMMA_7500K_50},   // 2 =  70
{ 60, "75K", &DSI_CMD_GAMMA_7500K_60},   //  3 = 90,
{ 70, "75K",&DSI_CMD_GAMMA_7500K_70},   // 4 = 100,
{ 80, "75K",&DSI_CMD_GAMMA_7500K_80},   // 5 = 110,
{ 90, "75K",&DSI_CMD_GAMMA_7500K_90},   // 6 = 120,
{ 100, "75K",&DSI_CMD_GAMMA_7500K_100},   // 7 = 130,
{ 110, "75K",&DSI_CMD_GAMMA_7500K_110},   // 8 = 140,
{ 110, "75K",&DSI_CMD_GAMMA_7500K_110},   // 9 = 150,
{ 120, "75K",&DSI_CMD_GAMMA_7500K_120},   // 10= 160,
{ 130, "75K",&DSI_CMD_GAMMA_7500K_130},   // 11= 170,
{ 140, "75K",&DSI_CMD_GAMMA_7500K_140},   // 12= 180,
{ 150, "75K",&DSI_CMD_GAMMA_7500K_150},   // 13= 190,
{ 160, "75K",&DSI_CMD_GAMMA_7500K_160},   // 14= 200,
{ 170, "75K",&DSI_CMD_GAMMA_7500K_170},   // 15= 210,
{ 180, "75K",&DSI_CMD_GAMMA_7500K_180},   // 16= 220,
{ 190, "75K",&DSI_CMD_GAMMA_7500K_190},   // 17= 230,
{ 200, "75K",&DSI_CMD_GAMMA_7500K_200},   // 18= 240,
{ 210, "75K",&DSI_CMD_GAMMA_7500K_210},   // 19= 250,
{ 210, "75K",&DSI_CMD_GAMMA_7500K_210},   // 20= 260,
{ 220, "75K",&DSI_CMD_GAMMA_7500K_220},   // 21= 270,
{ 230, "75K",&DSI_CMD_GAMMA_7500K_230},   // 22= 280,
{ 240, "75K",&DSI_CMD_GAMMA_7500K_240},   // 23= 290,
{ 250, "75K",&DSI_CMD_GAMMA_7500K_250},  // 24= 300,
{ 300, "75K",&DSI_CMD_GAMMA_7500K_300}   // 24= 300,
};
#else
static struct dsi_cmd_desc_LCD lcd_brightness_7500k_table[MAX_GAMMA_VALUE+1] = {
{ 30, "75K", &DSI_CMD_GAMMA_7500K_30},   // 0 = 30_dimming,
{ 40, "75K", &DSI_CMD_GAMMA_7500K_40},   // 1 =  40
{ 70, "75K", &DSI_CMD_GAMMA_7500K_70},   // 2 =  70
//{ 80, "75K", &DSI_CMD_GAMMA_7500K_80},   // 2 =  70
{ 90, "75K", &DSI_CMD_GAMMA_7500K_90},   //  3 = 90,
{ 100, "75K",&DSI_CMD_GAMMA_7500K_100},   // 4 = 100,
{ 110, "75K",&DSI_CMD_GAMMA_7500K_110},   // 5 = 110,
{ 120, "75K",&DSI_CMD_GAMMA_7500K_120},   // 6 = 120,
{ 130, "75K",&DSI_CMD_GAMMA_7500K_130},   // 7 = 130,
{ 140, "75K",&DSI_CMD_GAMMA_7500K_140},   // 8 = 140,
{ 150, "75K",&DSI_CMD_GAMMA_7500K_150},   // 9 = 150,
{ 160, "75K",&DSI_CMD_GAMMA_7500K_160},   // 10= 160,
{ 170, "75K",&DSI_CMD_GAMMA_7500K_170},   // 11= 170,
{ 180, "75K",&DSI_CMD_GAMMA_7500K_180},   // 12= 180,
{ 190, "75K",&DSI_CMD_GAMMA_7500K_190},   // 13= 190,
{ 200, "75K",&DSI_CMD_GAMMA_7500K_200},   // 14= 200,
{ 210, "75K",&DSI_CMD_GAMMA_7500K_210},   // 15= 210,
{ 220, "75K",&DSI_CMD_GAMMA_7500K_220},   // 16= 220,
{ 230, "75K",&DSI_CMD_GAMMA_7500K_230},   // 17= 230,
{ 240, "75K",&DSI_CMD_GAMMA_7500K_240},   // 18= 240,
{ 250, "75K",&DSI_CMD_GAMMA_7500K_250},   // 19= 250,
{ 260, "75K",&DSI_CMD_GAMMA_7500K_260},   // 20= 260,
{ 270, "75K",&DSI_CMD_GAMMA_7500K_270},   // 21= 270,
{ 280, "75K",&DSI_CMD_GAMMA_7500K_280},   // 22= 280,
{ 290, "75K",&DSI_CMD_GAMMA_7500K_290},   // 23= 290,
{ 300, "75K",&DSI_CMD_GAMMA_7500K_300}   // 24= 300,
};
#endif

#define SmartDimming_GammaUpdate_Pos (2)
static unsigned char GAMMA_SmartDimming_COND_SET[]={ 0xFA, 0x01,	0x4A,	0x01,	0x4D,	0x7A,	0x5D,	0xA5,	0x9C,	0xCA,	0xA4,	0xBD,	0xDC,	0xBE,	0x93,	0xBD,	0x95,	0xBA,	0xD2,	0xB7,	0x00,	0x81,	0x00,	0x75,	0x00,	0xA5,	};	
static struct dsi_cmd_desc DSI_CMD_SmartDimming_GAMMA = {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(GAMMA_SmartDimming_COND_SET), GAMMA_SmartDimming_COND_SET};  
const unsigned char GAMMA_SmartDimming_VALUE_SET_A1SM2_300cd[24] = {
0x71,0x31,0x7B,0xa4,0xb6,0x95,0xa9,0xbc,
0xa2,0xbb,0xc9,0xb6,0x91,0xa3,0x8b,0xad,
0xb6,0xa9,0x00,0xd6,0x00,0xbe,0x00,0xfc
};
const unsigned char GAMMA_SmartDimming_VALUE_SET_A1SM2_AID1_300cd[24] = {
0x74,0x38,0x7b,0xa2,0xb0,0x97,0xa7,0xb9,
0xa2,0xbc,0xc8,0xb9,0x8c,0x9a,0x87,0xa7,
0xb0,0xa4,0x00,0xe8,0x00,0xd1,0x01,0x12
};
const unsigned char GAMMA_SmartDimming_VALUE_SET_A1SM2_AID2_300cd[24] = {
0x77,0x3b,0x7d,0x9d,0xaf,0x95,0xa4,0xb6,
0xa0,0xbc,0xc8,0xb9,0x88,0x96,0x85,0xa4,
0xad,0xa1,0x00,0xf3,0x00,0xdc,0x01,0x1f
};
const unsigned char GAMMA_SmartDimming_VALUE_SET_A1SM2_AID3_300cd[24] = {
0x77,0x3e,0x7c,0x9e,0xad,0x99,0xa5,0xb3,
0xa2,0xba,0xc5,0xb8,0x86,0x94,0x83,0xa0,
0xa7,0x9f,0x01,0x05,0x00,0xee,0x01,0x33
};

// Use original For A2 line_1. OPM Rev.K
const unsigned char GAMMA_SmartDimming_VALUE_SET_A2SM2_300cd[24] = {
0x5F,0x2E,0x67,0xAA,0xC6,0xAC,0xB0,0xC8,
0xBB,0xBE,0xCB,0xBD,0x97,0xA5,0x91,0xAF,
0xB8,0xAB,0x00,0xC2,0x00,0xBA,0x00,0xE2
};
// For A2 line_2. CELOXHD OPM Rev.K
const unsigned char GAMMA_SmartDimming_VALUE_SET_A2SM2_300cd_line2[24] = {
0x41,0x0A,0x47,0xAB,0xBE,0xA8,0xAF,0xC5,
0xB7,0xC3,0xCC,0xC3,0x9A,0xA3,0x96,0xB1,
0xB7,0xAF,0x00,0xBD,0x00,0xAC,0x00,0xDE
};


const unsigned char GAMMA_SmartDimming_VALUE_SET_A2SM2_AID1_300cd[24] = {
0x61,0x30,0x67,0xA9,0xCB,0xB8,0xAE,0xC4,
0xB8,0xBC,0xC9,0xBC,0x93,0xA1,0x8E,0xAA,
0xB4,0xA6,0x00,0xD4,0x00,0xCD,0x00,0xF8
};
const unsigned char GAMMA_SmartDimming_VALUE_SET_A2SM2_AID2_300cd[24] = {
0x64,0x36,0x69,0xA3,0xC6,0xB9,0xAC,0xC0,
0xB5,0xBA,0xC6,0xB9,0x8F,0x9C,0x8B,0xA7,
0xAE,0xA3,0x00,0xDF,0x00,0xDA,0x01,0x06
};
const unsigned char GAMMA_SmartDimming_VALUE_SET_A2SM2_AID3_300cd[24] = {
0x64,0x3B,0x6B,0xA7,0xC3,0xBA,0xAB,0xBD,
0xB2,0xBB,0xC3,0xB8,0x8C,0x97,0x87,0xA3,
0xA9,0x9F,0x00,0xF0,0x00,0xEB,0x01,0x19
};



// 2011.07.04 from SMD - Brightness has became to Dark!!! not use
static char ACL_COND_SET_40[]={	0xC1, 0x47, 0x53, 0x13, 0x53, 0x00, 0x00, 0x02, 0xCF, 0x00, 0x00, 0x04, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x08, 0x0F, 0x16, 0x1D, 0x24, 0x2A, 0x31, 0x38, 0x3F, 0x46 };
#if 0 // warning : defined but not used
static struct dsi_cmd_desc DSI_CMD_ACL_40= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_COND_SET_40), ACL_COND_SET_40};

static struct dsi_cmd_desc_LCD lcd_acl_const40_table[MAX_GAMMA_VALUE+1] = {
{ 0, "30", NULL},   // 1 = 30_dimming,
{ 0, "40", NULL},   // 4 =  (Normal Range is From 4~31)
{ 40, "70", &DSI_CMD_ACL_40},   // 8 =
//{ 40, "80", &DSI_CMD_ACL_40},   // 8 =
{ 40, "90", &DSI_CMD_ACL_40},   //  10 = 90,
{ 40, "100", &DSI_CMD_ACL_40},   // 11 = 100,
{ 40, "110", &DSI_CMD_ACL_40},   // 12 = 110,
{ 40, "120", &DSI_CMD_ACL_40},   // 13 = 120,
{ 40, "130", &DSI_CMD_ACL_40},   // 14 = 130,
{ 40, "140", &DSI_CMD_ACL_40},   // 15 = 140,
{ 40, "150", &DSI_CMD_ACL_40},   // 16 = 150,
{ 40, "160", &DSI_CMD_ACL_40},   // 17= 160,
{ 40, "170", &DSI_CMD_ACL_40},   // 18= 170,
{ 40, "180", &DSI_CMD_ACL_40},   // 19= 180,
{ 40, "190", &DSI_CMD_ACL_40},   // 20= 190,
{ 40, "200", &DSI_CMD_ACL_40},   // 21= 200,
{ 40, "210", &DSI_CMD_ACL_40},   // 22= 210,
{ 40, "220", &DSI_CMD_ACL_40},   // 23= 220,
{ 40, "230", &DSI_CMD_ACL_40},   // 24= 230,
{ 40, "240", &DSI_CMD_ACL_40},   // 25= 240,
{ 40, "250", &DSI_CMD_ACL_40},   // 26= 250,
{ 40, "260", &DSI_CMD_ACL_40},   // 27= 260,
{ 40, "270", &DSI_CMD_ACL_40},   // 28= 270,
{ 40, "280", &DSI_CMD_ACL_40},   // 29= 280,
{ 40, "290", &DSI_CMD_ACL_40},   // 30= 290,
{ 40, "300", &DSI_CMD_ACL_40}   // 31= 300,
};


static char ACL_COND_SET_55[]={	0xC1, 0x47, 0x53, 0x13, 0x53, 0x00, 0x00, 0x02, 0xCF, 0x00, 0x00, 0x04, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x09, 0x11, 0x18, 0x20, 0x28, 0x30, 0x38, 0x3F, 0x47, 0x4F };
static struct dsi_cmd_desc DSI_CMD_ACL_55= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_COND_SET_40), ACL_COND_SET_40};

static struct dsi_cmd_desc_LCD lcd_acl_const55_table[MAX_GAMMA_VALUE+1] = {
{ 0, "30", NULL},   // 1 = 30_dimming,
{ 0, "40", NULL},   // 4 =  (Normal Range is From 4~31)
{ 55, "70", &DSI_CMD_ACL_55},   // 8 =
//{ 55, "80", &DSI_CMD_ACL_55},   // 8 =
{ 55, "90", &DSI_CMD_ACL_55},   //  10 = 90,
{ 55, "100", &DSI_CMD_ACL_55},   // 11 = 100,
{ 55, "110", &DSI_CMD_ACL_55},   // 12 = 110,
{ 55, "120", &DSI_CMD_ACL_55},   // 13 = 120,
{ 55, "130", &DSI_CMD_ACL_55},   // 14 = 130,
{ 55, "140", &DSI_CMD_ACL_55},   // 15 = 155,
{ 55, "150", &DSI_CMD_ACL_55},   // 16 = 150,
{ 55, "160", &DSI_CMD_ACL_55},   // 17= 160,
{ 55, "170", &DSI_CMD_ACL_55},   // 18= 170,
{ 55, "180", &DSI_CMD_ACL_55},   // 19= 180,
{ 55, "190", &DSI_CMD_ACL_55},   // 20= 190,
{ 55, "200", &DSI_CMD_ACL_55},   // 21= 200,
{ 55, "210", &DSI_CMD_ACL_55},   // 22= 210,
{ 55, "220", &DSI_CMD_ACL_55},   // 23= 220,
{ 55, "230", &DSI_CMD_ACL_55},   // 24= 230,
{ 55, "240", &DSI_CMD_ACL_55},   // 25= 255,
{ 55, "250", &DSI_CMD_ACL_55},   // 26= 250,
{ 55, "260", &DSI_CMD_ACL_55},   // 27= 260,
{ 55, "270", &DSI_CMD_ACL_55},   // 28= 270,
{ 55, "280", &DSI_CMD_ACL_55},   // 29= 280,
{ 55, "290", &DSI_CMD_ACL_55},   // 30= 290,
{ 55, "300", &DSI_CMD_ACL_55}   // 31= 300,
};
#endif 

#define ACL_xxP_DIFF_POS (22)
#define ACL_xxP_COND_HEADER 0xC1, 0x4D, 0x96, 0x1D, 0x00, 0x00, 0x01, 0xDF, 0x00, 0x00, 0x03, 0x1F

#if 0 // warning : defined but not used
static char ACL_55P_COND_SET_230_300CD[] = { ACL_xxP_COND_HEADER, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x09, 0x11, 0x18, 0x20, 0x28, 0x30, 0x38, 0x3F, 0x47, 0x4F };
static char ACL_55P_COND_SET_220_220CD[] = { ACL_xxP_COND_HEADER, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x09, 0x10, 0x18, 0x1F, 0x27, 0x2E, 0x36, 0x3D, 0x45, 0x4C };
static char ACL_55P_COND_SET_210_210CD[] = { ACL_xxP_COND_HEADER, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x08, 0x10, 0x17, 0x1E, 0x26, 0x2D, 0x34, 0x3B, 0x43, 0x4A };
static char ACL_55P_COND_SET_200_200CD[] = { ACL_xxP_COND_HEADER, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x08, 0x0F, 0x15, 0x1C, 0x23, 0x2A, 0x31, 0x37, 0x3E, 0x45 };
static char ACL_55P_COND_SET_190_190CD[] = { ACL_xxP_COND_HEADER, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x08, 0x0E, 0x15, 0x1B, 0x22, 0x28, 0x2F, 0x35, 0x3C, 0x42 };
static char ACL_55P_COND_SET_70_180CD[] = { ACL_xxP_COND_HEADER, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x0D, 0x13, 0x19, 0x20, 0x26, 0x2C, 0x32, 0x38, 0x3E };

static struct dsi_cmd_desc DSI_CMD_ACL_55P_230_300CD= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_55P_COND_SET_230_300CD), ACL_55P_COND_SET_230_300CD};
static struct dsi_cmd_desc DSI_CMD_ACL_55P_220_220CD= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_55P_COND_SET_220_220CD), ACL_55P_COND_SET_220_220CD};
static struct dsi_cmd_desc DSI_CMD_ACL_55P_210_210CD= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_55P_COND_SET_210_210CD), ACL_55P_COND_SET_210_210CD};
static struct dsi_cmd_desc DSI_CMD_ACL_55P_200_200CD= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_55P_COND_SET_200_200CD), ACL_55P_COND_SET_200_200CD};
static struct dsi_cmd_desc DSI_CMD_ACL_55P_190_190CD= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_55P_COND_SET_190_190CD), ACL_55P_COND_SET_190_190CD};
static struct dsi_cmd_desc DSI_CMD_ACL_55P_70_180CD= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_55P_COND_SET_70_180CD), ACL_55P_COND_SET_70_180CD};

static struct dsi_cmd_desc_LCD lcd_acl_55p_table[MAX_GAMMA_VALUE+1] = {
{ 0, "55p30", NULL},   // 1 = 30_dimming,
{ 0, "55p40", NULL},   // 4 =  (Normal Range is From 4~31)
{ 45, "55p70", &DSI_CMD_ACL_55P_70_180CD},   // 8 =
//{ 45, "55p80", &DSI_CMD_ACL_55P_70_180CD},   //  10 = 90,
{ 45, "55p90", &DSI_CMD_ACL_55P_70_180CD},   //  10 = 90,
{ 45, "55p100", &DSI_CMD_ACL_55P_70_180CD},   // 11 = 100,
{ 45, "55p110", &DSI_CMD_ACL_55P_70_180CD},   // 12 = 110,
{ 45, "55p120", &DSI_CMD_ACL_55P_70_180CD},   // 13 = 120,
{ 45, "55p130", &DSI_CMD_ACL_55P_70_180CD},   // 14 = 130,
{ 45, "55p140", &DSI_CMD_ACL_55P_70_180CD},   // 15 = 140,
{ 45, "55p150", &DSI_CMD_ACL_55P_70_180CD},   // 16 = 150,
{ 45, "55p160", &DSI_CMD_ACL_55P_70_180CD},   // 17= 160,
{ 45, "55p170", &DSI_CMD_ACL_55P_70_180CD},   // 18= 170,
{ 45, "55p180", &DSI_CMD_ACL_55P_70_180CD},   // 19= 180,
{ 47, "55p190", &DSI_CMD_ACL_55P_190_190CD},   // 20= 190,
{ 49, "55p200", &DSI_CMD_ACL_55P_200_200CD},   // 21= 200,
{ 52, "55p210", &DSI_CMD_ACL_55P_210_210CD},   // 22= 210,
{ 53, "55p220", &DSI_CMD_ACL_55P_220_220CD},   // 23= 220,
{ 55, "55p230", &DSI_CMD_ACL_55P_230_300CD},   // 24= 230,
{ 55, "55p240", &DSI_CMD_ACL_55P_230_300CD},   // 25= 240,
{ 55, "55p250", &DSI_CMD_ACL_55P_230_300CD},   // 26= 250,
{ 55, "55p260", &DSI_CMD_ACL_55P_230_300CD},   // 27= 260,
{ 55, "55p270", &DSI_CMD_ACL_55P_230_300CD},   // 28= 270,
{ 55, "55p280", &DSI_CMD_ACL_55P_230_300CD},   // 29= 280,
{ 55, "55p290", &DSI_CMD_ACL_55P_230_300CD},   // 30= 290,
{ 55, "55p300", &DSI_CMD_ACL_55P_230_300CD}   // 31= 300,
};
#endif 

#define ACL_ManualF_DIFF_POS (22)
#define ACL_ManualF_COND_HEADER 0xC1, 0x47, 0x53, 0x13, 0x53, 0x00, 0x00, 0x02, 0xCF, 0x00, 0x00, 0x04, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00

//static char ACL_ManualF_COND_SET_55p[] = { ACL_ManualF_COND_HEADER, 0x01, 0x0A, 0x12, 0x1B, 0x23, 0x2C, 0x35, 0x3D, 0x46, 0x4E, 0x57 };
//static char ACL_ManualF_COND_SET_53p[] = { ACL_ManualF_COND_HEADER, 0x01, 0x09, 0x11, 0x1A, 0x22, 0x2A, 0x32, 0x3A, 0x43, 0x4B, 0x53 };
//static char ACL_ManualF_COND_SET_52p[] = { ACL_ManualF_COND_HEADER, 0x01, 0x09, 0x11, 0x19, 0x21, 0x29, 0x31, 0x39, 0x41, 0x49, 0x51 };
static char ACL_ManualF_COND_SET_20p[] = { ACL_ManualF_COND_HEADER, 0x01, 0x04, 0x07, 0x0A, 0x0D, 0x10, 0x12, 0x15, 0x18, 0x1B, 0x1E };
static char ACL_ManualF_COND_SET_33p[] = { ACL_ManualF_COND_HEADER, 0x01, 0x06, 0x0A, 0x0F, 0x14, 0x19, 0x1D, 0x22, 0x27, 0x2B, 0x30 };
static char ACL_ManualF_COND_SET_50p[] = { ACL_ManualF_COND_HEADER, 0x01, 0x09, 0x10, 0x18, 0x1F, 0x27, 0x2E, 0x36, 0x3D, 0x45, 0x4C };
static char ACL_ManualF_COND_SET_48p[] = { ACL_ManualF_COND_HEADER, 0x01, 0x08, 0x0F, 0x17, 0x1E, 0x25, 0x2C, 0x33, 0x3B, 0x42, 0x49 };
static char ACL_ManualF_COND_SET_47p[] = { ACL_ManualF_COND_HEADER, 0x01, 0x08, 0x0F, 0x15, 0x1C, 0x23, 0x2A, 0x31, 0x37, 0x3E, 0x45 };
static char ACL_ManualF_COND_SET_45p[] = { ACL_ManualF_COND_HEADER, 0x01, 0x07, 0x0E, 0x14, 0x1B, 0x21, 0x27, 0x2E, 0x34, 0x3B, 0x41 };
static char ACL_ManualF_COND_SET_43p[] = { ACL_ManualF_COND_HEADER, 0x01, 0x07, 0x0D, 0x14, 0x1A, 0x20, 0x26, 0x2C, 0x33, 0x39, 0x3F };
static char ACL_ManualF_COND_SET_40p[] = { ACL_ManualF_COND_HEADER, 0x01, 0x07, 0x0D, 0x12, 0x18, 0x1E, 0x24, 0x2A, 0x2F, 0x35, 0x3B };

//static struct dsi_cmd_desc DSI_CMD_ACL_ManualF_55p= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_ManualF_COND_SET_55p), ACL_ManualF_COND_SET_55p};
//static struct dsi_cmd_desc DSI_CMD_ACL_ManualF_53p= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_ManualF_COND_SET_53p), ACL_ManualF_COND_SET_53p};
//static struct dsi_cmd_desc DSI_CMD_ACL_ManualF_52p= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_ManualF_COND_SET_52p), ACL_ManualF_COND_SET_52p};
static struct dsi_cmd_desc DSI_CMD_ACL_ManualF_20p= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_ManualF_COND_SET_20p), ACL_ManualF_COND_SET_20p};
static struct dsi_cmd_desc DSI_CMD_ACL_ManualF_33p= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_ManualF_COND_SET_33p), ACL_ManualF_COND_SET_33p};
static struct dsi_cmd_desc DSI_CMD_ACL_ManualF_50p= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_ManualF_COND_SET_50p), ACL_ManualF_COND_SET_50p};
static struct dsi_cmd_desc DSI_CMD_ACL_ManualF_48p= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_ManualF_COND_SET_48p), ACL_ManualF_COND_SET_48p};
static struct dsi_cmd_desc DSI_CMD_ACL_ManualF_47p= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_ManualF_COND_SET_47p), ACL_ManualF_COND_SET_47p};
static struct dsi_cmd_desc DSI_CMD_ACL_ManualF_45p= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_ManualF_COND_SET_45p), ACL_ManualF_COND_SET_45p};
static struct dsi_cmd_desc DSI_CMD_ACL_ManualF_43p= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_ManualF_COND_SET_45p), ACL_ManualF_COND_SET_43p};
static struct dsi_cmd_desc DSI_CMD_ACL_ManualF_40p= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ACL_ManualF_COND_SET_40p), ACL_ManualF_COND_SET_40p};

#if 0 // Manual-H
static struct dsi_cmd_desc_LCD lcd_acl_ManualF_table[MAX_GAMMA_VALUE+1] = {
	{ 0, "mf30", NULL},   // 1 = 30_dimming,
	{ 0, "mf40", NULL},   // 4 =  (Normal Range is From 4~31)
	{ 43, "mf70", &DSI_CMD_ACL_ManualF_43p},   // 8 =
	//{ 45, "mf80", &DSI_CMD_ACL_ManualF_45p},   // 8 =
	{ 45, "mf90", &DSI_CMD_ACL_ManualF_45p},   //  10 = 90,
	{ 45, "mf100", &DSI_CMD_ACL_ManualF_45p},   // 11 = 100,
	{ 45, "mf110", &DSI_CMD_ACL_ManualF_45p},   // 12 = 110,
	{ 45, "mf120", &DSI_CMD_ACL_ManualF_45p},   // 13 = 120,
	{ 45, "mf130", &DSI_CMD_ACL_ManualF_45p},   // 14 = 130,
	{ 45, "mf140", &DSI_CMD_ACL_ManualF_45p},   // 15 = 140,
	{ 45, "mf150", &DSI_CMD_ACL_ManualF_45p},   // 16 = 150,
	{ 45, "mf160", &DSI_CMD_ACL_ManualF_45p},   // 17= 160,
	{ 45, "mf170", &DSI_CMD_ACL_ManualF_45p},   // 18= 170,
	{ 45, "mf180", &DSI_CMD_ACL_ManualF_45p},   // 19= 180,
	{ 48, "mf190", &DSI_CMD_ACL_ManualF_48p},   // 20= 190,
	{ 50, "mf200", &DSI_CMD_ACL_ManualF_50p},   // 21= 200,
	{ 52, "mf210", &DSI_CMD_ACL_ManualF_52p},   // 22= 210,
	{ 53, "mf220", &DSI_CMD_ACL_ManualF_53p},   // 23= 220,
	{ 55, "mf230", &DSI_CMD_ACL_ManualF_55p},   // 24= 230,
	{ 55, "mf240", &DSI_CMD_ACL_ManualF_55p},   // 25= 240,
	{ 55, "mf250", &DSI_CMD_ACL_ManualF_55p},   // 26= 250,
	{ 55, "mf260", &DSI_CMD_ACL_ManualF_55p},   // 27= 260,
	{ 55, "mf270", &DSI_CMD_ACL_ManualF_55p},   // 28= 270,
	{ 55, "mf280", &DSI_CMD_ACL_ManualF_55p},   // 29= 280,
	{ 55, "mf290", &DSI_CMD_ACL_ManualF_55p},   // 30= 290,
	{ 55, "mf300", &DSI_CMD_ACL_ManualF_55p}   // 31= 300,
};
#else // 11.10.18 HW request

#if defined (CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)
static struct dsi_cmd_desc_LCD lcd_acl_ManualF_table[MAX_GAMMA_VALUE+1] = {
	{ 0, "mf30", NULL},   // 1 = 30_dimming,
	{ 0, "mf40", NULL},   // 4 =  (Normal Range is From 4~31)
	{ 20, "mf70", &DSI_CMD_ACL_ManualF_20p},   // 8 =
	//{ 40, "mf80", &DSI_CMD_ACL_ManualF_40p},   // 8 =
	{ 33, "mf90", &DSI_CMD_ACL_ManualF_33p},   //  10 = 90,
	{ 40, "mf100", &DSI_CMD_ACL_ManualF_40p},   // 11 = 100,
	{ 40, "mf110", &DSI_CMD_ACL_ManualF_40p},   // 12 = 110,
	{ 40, "mf120", &DSI_CMD_ACL_ManualF_40p},   // 13 = 120,
	{ 40, "mf130", &DSI_CMD_ACL_ManualF_40p},   // 14 = 130,
	{ 40, "mf140", &DSI_CMD_ACL_ManualF_40p},   // 15 = 140,
	{ 40, "mf150", &DSI_CMD_ACL_ManualF_40p},   // 16 = 150,
	{ 40, "mf160", &DSI_CMD_ACL_ManualF_40p},   // 17= 160,
	{ 40, "mf170", &DSI_CMD_ACL_ManualF_40p},   // 18= 170,
	{ 40, "mf180", &DSI_CMD_ACL_ManualF_40p},   // 19= 180,
	{ 40, "mf190", &DSI_CMD_ACL_ManualF_40p},   // 20= 190,
	{ 40, "mf200", &DSI_CMD_ACL_ManualF_40p},   // 21= 200,
	{ 40, "mf210", &DSI_CMD_ACL_ManualF_40p},   // 22= 210, // there's not 47%, use 48%
	{ 40, "mf220", &DSI_CMD_ACL_ManualF_40p},   // 23= 220,
	{ 40, "mf230", &DSI_CMD_ACL_ManualF_40p},   // 24= 230,
	{ 40, "mf240", &DSI_CMD_ACL_ManualF_40p},   // 25= 240,
	{ 40, "mf250", &DSI_CMD_ACL_ManualF_40p},   // 26= 250,
	{ 40, "mf260", &DSI_CMD_ACL_ManualF_40p},   // 27= 260,
	{ 40, "mf270", &DSI_CMD_ACL_ManualF_40p},   // 28= 270,
	{ 40, "mf280", &DSI_CMD_ACL_ManualF_40p},   // 29= 280,
	{ 40, "mf290", &DSI_CMD_ACL_ManualF_40p},   // 30= 290,
	{ 40, "mf300", &DSI_CMD_ACL_ManualF_40p},  // 31= 300,
	{ 50, "mf300", &DSI_CMD_ACL_ManualF_50p}   // 31= 300,	
};
#else


#ifdef MAPPING_TBL_AUTO_BRIGHTNESS
static struct dsi_cmd_desc_LCD lcd_acl_ManualF_table[MAX_GAMMA_VALUE+1] = {
	{ 0, "mf30", NULL},   // 1 = 30_dimming,
	{ 0, "mf40", NULL},   // 2
	{ 20, "mf50", &DSI_CMD_ACL_ManualF_20p},   
	{ 20, "mf60", &DSI_CMD_ACL_ManualF_20p},   
	{ 33, "mf70", &DSI_CMD_ACL_ManualF_33p},   
   	{ 40, "mf80", &DSI_CMD_ACL_ManualF_40p},   
	{ 40, "mf90", &DSI_CMD_ACL_ManualF_40p},   
	{ 40, "mf100", &DSI_CMD_ACL_ManualF_40p},   
	{ 40, "mf110", &DSI_CMD_ACL_ManualF_40p},   
	{ 40, "mf120", &DSI_CMD_ACL_ManualF_40p},   
	{ 40, "mf130", &DSI_CMD_ACL_ManualF_40p},   
	{ 40, "mf140", &DSI_CMD_ACL_ManualF_40p},   
	{ 40, "mf150", &DSI_CMD_ACL_ManualF_40p},   
	{ 40, "mf160", &DSI_CMD_ACL_ManualF_40p},   
	{ 40, "mf170", &DSI_CMD_ACL_ManualF_40p},  
	{ 40, "mf180", &DSI_CMD_ACL_ManualF_40p},   
	{ 40, "mf190", &DSI_CMD_ACL_ManualF_40p},   
	{ 40, "mf200", &DSI_CMD_ACL_ManualF_40p},   
	{ 40, "mf210", &DSI_CMD_ACL_ManualF_40p},   
	{ 40, "mf220", &DSI_CMD_ACL_ManualF_40p},  
	{ 40, "mf230", &DSI_CMD_ACL_ManualF_40p},   
	{ 40, "mf240", &DSI_CMD_ACL_ManualF_40p},   
	{ 40, "mf250", &DSI_CMD_ACL_ManualF_40p},   
	{ 50, "mf300", &DSI_CMD_ACL_ManualF_50p}   
};
#else
static struct dsi_cmd_desc_LCD lcd_acl_ManualF_table[MAX_GAMMA_VALUE+1] = {
	{ 0, "mf30", NULL},   // 1 = 30_dimming,
	{ 0, "mf40", NULL},   // 4 =  (Normal Range is From 4~31)
	{ 40, "mf70", &DSI_CMD_ACL_ManualF_40p},   // 8 =
	//{ 40, "mf80", &DSI_CMD_ACL_ManualF_40p},   // 8 =
	{ 40, "mf90", &DSI_CMD_ACL_ManualF_40p},   //  10 = 90,
	{ 40, "mf100", &DSI_CMD_ACL_ManualF_40p},   // 11 = 100,
	{ 40, "mf110", &DSI_CMD_ACL_ManualF_40p},   // 12 = 110,
	{ 40, "mf120", &DSI_CMD_ACL_ManualF_40p},   // 13 = 120,
	{ 40, "mf130", &DSI_CMD_ACL_ManualF_40p},   // 14 = 130,
	{ 40, "mf140", &DSI_CMD_ACL_ManualF_40p},   // 15 = 140,
	{ 40, "mf150", &DSI_CMD_ACL_ManualF_40p},   // 16 = 150,
	{ 40, "mf160", &DSI_CMD_ACL_ManualF_40p},   // 17= 160,
	{ 40, "mf170", &DSI_CMD_ACL_ManualF_40p},   // 18= 170,
	{ 40, "mf180", &DSI_CMD_ACL_ManualF_40p},   // 19= 180,
	{ 43, "mf190", &DSI_CMD_ACL_ManualF_43p},   // 20= 190,
	{ 45, "mf200", &DSI_CMD_ACL_ManualF_45p},   // 21= 200,
	{ 47, "mf210", &DSI_CMD_ACL_ManualF_47p},   // 22= 210, // there's not 47%, use 48%
	{ 48, "mf220", &DSI_CMD_ACL_ManualF_48p},   // 23= 220,
	{ 50, "mf230", &DSI_CMD_ACL_ManualF_50p},   // 24= 230,
	{ 50, "mf240", &DSI_CMD_ACL_ManualF_50p},   // 25= 240,
	{ 50, "mf250", &DSI_CMD_ACL_ManualF_50p},   // 26= 250,
	{ 50, "mf260", &DSI_CMD_ACL_ManualF_50p},   // 27= 260,
	{ 50, "mf270", &DSI_CMD_ACL_ManualF_50p},   // 28= 270,
	{ 50, "mf280", &DSI_CMD_ACL_ManualF_50p},   // 29= 280,
	{ 50, "mf290", &DSI_CMD_ACL_ManualF_50p},   // 30= 290,
	{ 50, "mf300", &DSI_CMD_ACL_ManualF_50p}   // 31= 300,
};
#endif 
#endif 
#endif


#define GET_NORMAL_ELVSS_ID_ADDRESS (2)
static char ELVSS_COND_SET_9Fh[]=	{ 0xB1, 0x04, 0x9F };
static char ELVSS_COND_SET_95h[]=	{ 0xB1, 0x04, 0x95 };
static char ELVSS_COND_SET_8Bh[]=	{ 0xB1, 0x04, 0x8B };
static char ELVSS_COND_SET_160[]=	{ 0xB1, 0x04, 0x95 };

static struct dsi_cmd_desc DSI_CMD_ELVSS_9Fh= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ELVSS_COND_SET_9Fh), ELVSS_COND_SET_9Fh}; 
static struct dsi_cmd_desc DSI_CMD_ELVSS_95h= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ELVSS_COND_SET_95h), ELVSS_COND_SET_95h}; 
static struct dsi_cmd_desc DSI_CMD_ELVSS_8Bh= {DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(ELVSS_COND_SET_8Bh), ELVSS_COND_SET_8Bh}; 
#if defined (CONFIG_USA_MODEL_SGH_I757) || defined(CONFIG_CAN_MODEL_SGH_I757M)

#ifdef MAPPING_TBL_AUTO_BRIGHTNESS
static struct dsi_cmd_desc_LCD lcd_elvss_dcdc13_table[CANDELA_TABLE_SIZE+1] = {
#else
static struct dsi_cmd_desc_LCD lcd_elvss_dcdc13_table[MAX_GAMMA_VALUE+1] = {
#endif
	{ 44, "30", &DSI_CMD_ELVSS_9Fh},   // 1 = 30_dimming,
	{ 44, "40", &DSI_CMD_ELVSS_9Fh},   // 4 =  (Normal Range is From 4~31)
	{ 44, "50", &DSI_CMD_ELVSS_9Fh},   // 8 =
	{ 44, "60", &DSI_CMD_ELVSS_9Fh},   // 8 =
	{ 44, "70", &DSI_CMD_ELVSS_9Fh},   //  10 = 90,
	{ 44, "80", &DSI_CMD_ELVSS_9Fh},   // 11 = 100,
	{ 44, "90", &DSI_CMD_ELVSS_9Fh},   // 12 = 110,
	{ 44, "100", &DSI_CMD_ELVSS_9Fh},   // 13 = 120,
#ifndef MAPPING_TBL_AUTO_BRIGHTNESS
	{ 44, "105", &DSI_CMD_ELVSS_9Fh},   // 14 = 130,
#endif
	{ 44, "110", &DSI_CMD_ELVSS_9Fh},   // 15 = 140,
	{ 44, "120", &DSI_CMD_ELVSS_9Fh},   // 16 = 150,
	{ 44, "130", &DSI_CMD_ELVSS_9Fh},   // 17= 160,
	{ 44, "140", &DSI_CMD_ELVSS_9Fh},   // 18= 170,
	{ 44, "150", &DSI_CMD_ELVSS_9Fh},   // 19= 180,
	{ 44, "160", &DSI_CMD_ELVSS_9Fh},   // 20= 190,
	{ 44, "170", &DSI_CMD_ELVSS_9Fh},   // 21= 200,
	{ 44, "180", &DSI_CMD_ELVSS_9Fh},   // 22= 210,
	{ 44, "190", &DSI_CMD_ELVSS_9Fh},   // 23= 220,
	{ 44, "200", &DSI_CMD_ELVSS_9Fh},   // 24= 230,
#ifndef MAPPING_TBL_AUTO_BRIGHTNESS
	{ 44, "205", &DSI_CMD_ELVSS_9Fh},   // 25= 240,
#endif
	{ 44, "210", &DSI_CMD_ELVSS_95h},   // 26= 250,
	{ 44, "220", &DSI_CMD_ELVSS_95h},   // 27= 260,
	{ 44, "230", &DSI_CMD_ELVSS_95h},   // 28= 270,
	{ 44, "240", &DSI_CMD_ELVSS_95h},   // 29= 280,
	{ 44, "250", &DSI_CMD_ELVSS_95h},   // 30= 290,
	{ 44, "300", &DSI_CMD_ELVSS_95h}   // 31= 300,
};

#ifdef MAPPING_TBL_AUTO_BRIGHTNESS
static struct dsi_cmd_desc_LCD lcd_elvss_dcdc03_table[CANDELA_TABLE_SIZE+1] = {
#else
static struct dsi_cmd_desc_LCD lcd_elvss_dcdc03_table[MAX_GAMMA_VALUE+1] = {
#endif
	{ 44, "30", &DSI_CMD_ELVSS_95h},   
	{ 44, "40", &DSI_CMD_ELVSS_95h},   
	{ 44, "50", &DSI_CMD_ELVSS_95h},   
	{ 44, "60", &DSI_CMD_ELVSS_95h},   
	{ 44, "70", &DSI_CMD_ELVSS_95h},   
	{ 44, "80", &DSI_CMD_ELVSS_95h},   
	{ 44, "90", &DSI_CMD_ELVSS_95h},  
	{ 44, "100", &DSI_CMD_ELVSS_95h},   
#ifndef MAPPING_TBL_AUTO_BRIGHTNESS
	{ 44, "105", &DSI_CMD_ELVSS_95h},   
#endif
	{ 44, "110", &DSI_CMD_ELVSS_95h},   
	{ 44, "120", &DSI_CMD_ELVSS_95h},   
	{ 44, "130", &DSI_CMD_ELVSS_95h},   
	{ 44, "140", &DSI_CMD_ELVSS_95h},   
	{ 44, "150", &DSI_CMD_ELVSS_95h},   
	{ 44, "160", &DSI_CMD_ELVSS_95h},   
	{ 44, "170", &DSI_CMD_ELVSS_95h},   
	{ 44, "180", &DSI_CMD_ELVSS_95h},   
	{ 44, "190", &DSI_CMD_ELVSS_95h},   
	{ 44, "200", &DSI_CMD_ELVSS_95h},   
#ifndef MAPPING_TBL_AUTO_BRIGHTNESS
	{ 44, "205", &DSI_CMD_ELVSS_95h},   
#endif
	{ 44, "210", &DSI_CMD_ELVSS_8Bh},   
	{ 44, "220", &DSI_CMD_ELVSS_8Bh},   
	{ 44, "230", &DSI_CMD_ELVSS_8Bh},   
	{ 44, "240", &DSI_CMD_ELVSS_8Bh},   
	{ 44, "250", &DSI_CMD_ELVSS_8Bh},   
	{ 44, "300", &DSI_CMD_ELVSS_8Bh}   
};

#else
static struct dsi_cmd_desc_LCD lcd_elvss_dcdc13_table[MAX_GAMMA_VALUE+1] = {
	{ 44, "30", &DSI_CMD_ELVSS_9Fh},   // 1 = 30_dimming,
	{ 44, "40", &DSI_CMD_ELVSS_9Fh},   // 4 =  (Normal Range is From 4~31)
	{ 44, "70", &DSI_CMD_ELVSS_9Fh},   // 8 =
//	{ 44, "80", &DSI_CMD_ELVSS_9Fh},   // 8 =
	{ 44, "90", &DSI_CMD_ELVSS_9Fh},   //  10 = 90,
	{ 44, "100", &DSI_CMD_ELVSS_9Fh},   // 11 = 100,
	{ 44, "110", &DSI_CMD_ELVSS_9Fh},   // 12 = 110,
	{ 44, "120", &DSI_CMD_ELVSS_9Fh},   // 13 = 120,
	{ 44, "130", &DSI_CMD_ELVSS_9Fh},   // 14 = 130,
	{ 44, "140", &DSI_CMD_ELVSS_9Fh},   // 15 = 140,
	{ 44, "150", &DSI_CMD_ELVSS_9Fh},   // 16 = 150,
	{ 44, "160", &DSI_CMD_ELVSS_9Fh},   // 17= 160,
	{ 44, "170", &DSI_CMD_ELVSS_9Fh},   // 18= 170,
	{ 44, "180", &DSI_CMD_ELVSS_9Fh},   // 19= 180,
	{ 44, "190", &DSI_CMD_ELVSS_9Fh},   // 20= 190,
	{ 44, "200", &DSI_CMD_ELVSS_9Fh},   // 21= 200,
	{ 44, "210", &DSI_CMD_ELVSS_95h},   // 22= 210,
	{ 44, "220", &DSI_CMD_ELVSS_95h},   // 23= 220,
	{ 44, "230", &DSI_CMD_ELVSS_95h},   // 24= 230,
	{ 44, "240", &DSI_CMD_ELVSS_95h},   // 25= 240,
	{ 44, "250", &DSI_CMD_ELVSS_95h},   // 26= 250,
	{ 44, "260", &DSI_CMD_ELVSS_95h},   // 27= 260,
	{ 44, "270", &DSI_CMD_ELVSS_95h},   // 28= 270,
	{ 44, "280", &DSI_CMD_ELVSS_95h},   // 29= 280,
	{ 44, "290", &DSI_CMD_ELVSS_95h},   // 30= 290,
	{ 44, "300", &DSI_CMD_ELVSS_95h}   // 31= 300,
};

static struct dsi_cmd_desc_LCD lcd_elvss_dcdc03_table[MAX_GAMMA_VALUE+1] = {
	{ 44, "30", &DSI_CMD_ELVSS_95h},   // 1 = 30_dimming,
	{ 44, "40", &DSI_CMD_ELVSS_95h},   // 4 =  (Normal Range is From 4~31)
	{ 44, "70", &DSI_CMD_ELVSS_95h},   // 8 =
//	{ 44, "80", &DSI_CMD_ELVSS_95h},   // 8 =
	{ 44, "90", &DSI_CMD_ELVSS_95h},   //  10 = 90,
	{ 44, "100", &DSI_CMD_ELVSS_95h},   // 11 = 100,
	{ 44, "110", &DSI_CMD_ELVSS_95h},   // 12 = 110,
	{ 44, "120", &DSI_CMD_ELVSS_95h},   // 13 = 120,
	{ 44, "130", &DSI_CMD_ELVSS_95h},   // 14 = 130,
	{ 44, "140", &DSI_CMD_ELVSS_95h},   // 15 = 140,
	{ 44, "150", &DSI_CMD_ELVSS_95h},   // 16 = 150,
	{ 44, "160", &DSI_CMD_ELVSS_95h},   // 17= 160,
	{ 44, "170", &DSI_CMD_ELVSS_95h},   // 18= 170,
	{ 44, "180", &DSI_CMD_ELVSS_95h},   // 19= 180,
	{ 44, "190", &DSI_CMD_ELVSS_95h},   // 20= 190,
	{ 44, "200", &DSI_CMD_ELVSS_95h},   // 21= 200,
	{ 44, "210", &DSI_CMD_ELVSS_8Bh},   // 22= 210,
	{ 44, "220", &DSI_CMD_ELVSS_8Bh},   // 23= 220,
	{ 44, "230", &DSI_CMD_ELVSS_8Bh},   // 24= 230,
	{ 44, "240", &DSI_CMD_ELVSS_8Bh},   // 25= 240,
	{ 44, "250", &DSI_CMD_ELVSS_8Bh},   // 26= 250,
	{ 44, "260", &DSI_CMD_ELVSS_8Bh},   // 27= 260,
	{ 44, "270", &DSI_CMD_ELVSS_8Bh},   // 28= 270,
	{ 44, "280", &DSI_CMD_ELVSS_8Bh},   // 29= 280,
	{ 44, "290", &DSI_CMD_ELVSS_8Bh},   // 30= 290,
	{ 44, "300", &DSI_CMD_ELVSS_8Bh}   // 31= 300,
};

#endif


#define LCD_ELVSS_DELTA_210_300CD (0)
#define LCD_ELVSS_DELTA_170_200CD (0x08)
#define LCD_ELVSS_DELTA_110_160CD (0x0D)
#define LCD_ELVSS_DELTA_000_100CD (0x11)
#define LCD_ELVSS_RESULT_LIMIT	(0x1F)

#define EACH_ELVSS_COND_UPDATE_POS	(2)
static char EACH_ELVSS_COND_SET[] = { 0xB1, 0x04, 0xFF };
static struct dsi_cmd_desc lcd_each_elvss_table[1] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(EACH_ELVSS_COND_SET), EACH_ELVSS_COND_SET}
};


#ifdef MAPPING_TBL_AUTO_BRIGHTNESS
static int lcd_elvss_delta_table[CANDELA_TABLE_SIZE+1] = {
LCD_ELVSS_DELTA_000_100CD, // 0 = 30
LCD_ELVSS_DELTA_000_100CD, // 1 =  40        
LCD_ELVSS_DELTA_000_100CD, // 2 =  50       
LCD_ELVSS_DELTA_000_100CD, //  3 = 60,       
LCD_ELVSS_DELTA_000_100CD, // 4 = 70,       
LCD_ELVSS_DELTA_000_100CD, // 5 = 80,       
LCD_ELVSS_DELTA_000_100CD, // 6 = 90,       
LCD_ELVSS_DELTA_000_100CD, // 7 = 100,       
LCD_ELVSS_DELTA_110_160CD, // 8 = 110,		 
LCD_ELVSS_DELTA_110_160CD, // 9 = 120,       
LCD_ELVSS_DELTA_110_160CD, // 10 = 130,   
LCD_ELVSS_DELTA_110_160CD, // 11 = 140,       
LCD_ELVSS_DELTA_110_160CD, // 12 = 150,       
LCD_ELVSS_DELTA_110_160CD, // 13= 160,       
LCD_ELVSS_DELTA_170_200CD, // 14= 170,       
LCD_ELVSS_DELTA_170_200CD, // 15= 180,       
LCD_ELVSS_DELTA_170_200CD, // 16= 190,       
LCD_ELVSS_DELTA_170_200CD, // 17= 200,       
LCD_ELVSS_DELTA_210_300CD, // 18= 210,       
LCD_ELVSS_DELTA_210_300CD, // 19= 220,       
LCD_ELVSS_DELTA_210_300CD, // 20= 230,       
LCD_ELVSS_DELTA_210_300CD, // 21= 240,       
LCD_ELVSS_DELTA_210_300CD, // 22= 250,          
LCD_ELVSS_DELTA_210_300CD // 23= 300,       
};

#else
static int lcd_elvss_delta_table[MAX_GAMMA_VALUE+1] = {
LCD_ELVSS_DELTA_000_100CD, // 0 = 30_dimming,
LCD_ELVSS_DELTA_000_100CD, // 1 =  40        
LCD_ELVSS_DELTA_000_100CD, // 2 =  70        
//LCD_ELVSS_DELTA_000_100CD, //  3 = 80,       
LCD_ELVSS_DELTA_000_100CD, //  3 = 90,       
LCD_ELVSS_DELTA_000_100CD, // 4 = 100,       
LCD_ELVSS_DELTA_110_160CD, // 5 = 110,       
LCD_ELVSS_DELTA_110_160CD, // 6 = 120,       
LCD_ELVSS_DELTA_110_160CD, // 7 = 130,       
LCD_ELVSS_DELTA_110_160CD, // 8 = 140,       
LCD_ELVSS_DELTA_110_160CD, // 9 = 150,       
LCD_ELVSS_DELTA_110_160CD, // 10= 160,       
LCD_ELVSS_DELTA_170_200CD, // 11= 170,       
LCD_ELVSS_DELTA_170_200CD, // 12= 180,       
LCD_ELVSS_DELTA_170_200CD, // 13= 190,       
LCD_ELVSS_DELTA_170_200CD, // 14= 200,       
LCD_ELVSS_DELTA_210_300CD, // 15= 210,       
LCD_ELVSS_DELTA_210_300CD, // 16= 220,       
LCD_ELVSS_DELTA_210_300CD, // 17= 230,       
LCD_ELVSS_DELTA_210_300CD, // 18= 240,       
LCD_ELVSS_DELTA_210_300CD, // 19= 250,       
LCD_ELVSS_DELTA_210_300CD, // 20= 260,       
LCD_ELVSS_DELTA_210_300CD, // 21= 270,       
LCD_ELVSS_DELTA_210_300CD, // 22= 280,       
LCD_ELVSS_DELTA_210_300CD, // 23= 290,       
LCD_ELVSS_DELTA_210_300CD // 24= 300,       
};
#endif


#endif  /* MIPI_S6E8AA0_HD720_SEQ_H */
