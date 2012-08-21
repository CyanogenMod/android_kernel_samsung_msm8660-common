//--------------------------------------------------------
//
//
//	Melfas MCS7000 Series Download base v1.0 2010.04.05
//
//
//--------------------------------------------------------


#ifndef __MELFAS_FIRMWARE_DOWNLOAD_H__
#define __MELFAS_FIRMWARE_DOWNLOAD_H__

#include <asm/io.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#define MELFAS_FW1 "/sdcard/Master.bin"

#define MELFAS_BASE_FW_VER	0x40
#define ILJIN_BASE_FW_VER	0x57

#define MELFAS_FW_VER	0x43
#define ILJIN_FW_VER	0x62
#define CORE_FW_VER	0x43
#define PRIVATE_FW_VER	0x22
#define PUBLIC_FW_VER	0x15

//=====================================================================
//
//   MELFAS Firmware download pharameters
//
//=====================================================================

#define MELFAS_TRANSFER_LENGTH					(32/8)		// Fixed value
#define MELFAS_FIRMWARE_MAX_SIZE				(32*1024)

#define MELFAS_2CHIP_DOWNLOAD_ENABLE                1       // 0 : 1Chip Download, 1: 2Chip Download
// ISC download mode
#define MELFAS_ISC_2CHIP_DOWNLOAD_ENABLE            	1       // 0 : 1Chip Download, 1: 2Chip Download
#define MELFAS_CORE_FIRWMARE_UPDATE_ENABLE          	1      	// 0 : disable, 1: enable
#define MELFAS_PRIVATE_CONFIGURATION_UPDATE_ENABLE   	1       // 0 : disable, 1: enable
#define MELFAS_PUBLIC_CONFIGURATION_UPDATE_ENABLE    	1       // 0 : disable, 1: enable

//----------------------------------------------------
//   ISP Mode
//----------------------------------------------------
#define MELFAS_ISP_DOWNLOAD 					0		// 0: ISC mode   1:ISP mode   

#define ISP_MODE_ERASE_FLASH					0x01
#define ISP_MODE_SERIAL_WRITE					0x02
#define ISP_MODE_SERIAL_READ					0x03
#define ISP_MODE_NEXT_CHIP_BYPASS				0x04


//----------------------------------------------------
//   ISC Mode
//----------------------------------------------------

#define MELFAS_CRC_CHECK_ENABLE 				1

#define ISC_MODE_SLAVE_ADDRESS                  0x48

#define ISC_READ_DOWNLOAD_POSITION              1          //0 : USE ISC_PRIVATE_CONFIG_FLASH_START 1: READ FROM RMI MAP(0x61,0x62)
#define ISC_PRIVATE_CONFIG_FLASH_START          26
#define ISC_PUBLIC_CONFIG_FLASH_START           28

//address for ISC MODE
#define ISC_DOWNLOAD_MODE_ENTER                 0x5F
#define ISC_DOWNLOAD_MODE                       0x60
#define ISC_PRIVATE_CONFIGURATION_START_ADDR    0x61
#define ISC_PUBLIC_CONFIGURATION_START_ADDR     0x62

#define ISC_READ_SLAVE_CRC_OK                   0x63        // return value from slave

// mode
#define ISC_CORE_FIRMWARE_DL_MODE               0x01
#define ISC_PRIVATE_CONFIGURATION_DL_MODE       0x02
#define ISC_PUBLIC_CONFIGURATION_DL_MODE        0x03
#define ISC_SLAVE_DOWNLOAD_START                0x04

//----------------------------------------------------
//   Return values of download function
//----------------------------------------------------
#define MCSDL_RET_SUCCESS						0x00
#define MCSDL_RET_ERASE_FLASH_VERIFY_FAILED		0x01
#define MCSDL_RET_PROGRAM_VERIFY_FAILED			0x02
#define MCSDL_FIRMWARE_UPDATE_MODE_ENTER_FAILED	0x03
#define MCSDL_FIRMWARE_UPDATE_FAILED            0x04
#define MCSDL_LEAVE_FIRMWARE_UPDATE_MODE_FAILED 0x05

#define MCSDL_RET_PROGRAM_SIZE_IS_WRONG			0x10
#define MCSDL_RET_VERIFY_SIZE_IS_WRONG			0x11
#define MCSDL_RET_WRONG_BINARY					0x12

#define MCSDL_RET_READING_HEXFILE_FAILED		0x21
#define MCSDL_RET_FILE_ACCESS_FAILED			0x22
#define MCSDL_RET_MELLOC_FAILED					0x23

#define MCSDL_RET_ISC_SLAVE_CRC_CHECK_FAILED    0x30
#define MCSDL_RET_ISC_SLAVE_DOWNLOAD_TIME_OVER  0x31

#define MCSDL_RET_WRONG_MODULE_REVISION			0x40


//----------------------------------------------------
//	When you can't control VDD nor CE.
//	Set this value 1
//	Then Melfas Chip can prepare chip reset.
//----------------------------------------------------

#define MELFAS_USE_PROTOCOL_COMMAND_FOR_DOWNLOAD 	0							// If 'enable download command' is needed ( Pinmap dependent option ).

//============================================================
//
//	Port setting. ( Melfas preset this value. )
//
//============================================================

// If want to set Enable : Set to 1

#define MCSDL_USE_CE_CONTROL						0
#define MCSDL_USE_VDD_CONTROL						1
#define MCSDL_USE_RESETB_CONTROL                    1

void mcsdl_vdd_on(void);
void mcsdl_vdd_off(void);

#define GPIO_TSP_SCL   44
#define GPIO_TSP_SDA   43
#define GPIO_TOUCH_EN	62
#define GPIO_TOUCH_INT	125
#define GPIO_TOUCH_ID	63

/* Touch Screen Interface Specification Multi Touch (V0.5) */

/* REGISTERS */
#define MCSTS_STATUS_REG        0x00 //Status
#define MCSTS_MODE_CONTROL_REG  0x01 //Mode Control
#define MCSTS_RESOL_HIGH_REG    0x02 //Resolution(High Byte)
#define MCSTS_RESOL_X_LOW_REG   0x08 //Resolution(X Low Byte)
#define MCSTS_RESOL_Y_LOW_REG   0x0a //Resolution(Y Low Byte)
#define MCSTS_INPUT_INFO_REG    0x10 //Input Information
#define MCSTS_POINT_HIGH_REG    0x11 //Point(High Byte)
#define MCSTS_POINT_X_LOW_REG   0x12 //Point(X Low Byte)
#define MCSTS_POINT_Y_LOW_REG   0x13 //Point(Y Low Byte)
#define MCSTS_STRENGTH_REG      0x14 //Strength
#define MCSTS_MODULE_VER_REG    0x30 //H/W Module Revision
#define MCSTS_FIRMWARE_VER_REG_MASTER  	0x31 //F/W Version MASTER
#define MCSTS_FIRMWARE_VER_REG_SLAVE  	0x32 //F/W Version SLAVE
#define MCSTS_FIRMWARE_VER_REG_CORE  			0x33 //CORE F/W Version
#define MCSTS_FIRMWARE_VER_REG_PRIVATE_CUSTOM  	0x34 //PRIVATE_CUSTOM F/W Version
#define MCSTS_FIRMWARE_VER_REG_PUBLIC_CUSTOM	0x35 //PUBLIC CUSTOM F/W version	


//============================================================
//
//	Porting factors for Baseband
//
//============================================================

#include "mcs8000_download_porting_tablet.h"


//----------------------------------------------------
//	Functions
//----------------------------------------------------

int mcsdl_download_binary_data(bool);			// with binary type .c   file.
int mcsdl_download_binary_file(void);			// with binary type .bin file.

int mms100_ISC_download_binary_data(bool, INT8);
int mms100_ISC_download_binary_file(void);

//---------------------------------
//	Delay functions
//---------------------------------
void mcsdl_delay(UINT32 nCount);



#if MELFAS_ENABLE_DELAY_TEST					// For initial porting test.
void mcsdl_delay_test(INT32 nCount);
#endif


#endif		//#ifndef __MELFAS_FIRMWARE_DOWNLOAD_H__

