/***************************************************************************
*
* file     si_cbusDefs.c
* brief    CBUS API Definitions
*
* Copyright (C) (2011, Silicon Image Inc)
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation version 2.
*
* This program is distributed "as is" WITHOUT ANY WARRANTY of any
* kind, whether express or implied; without even the implied warranty
* of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*****************************************************************************/

#ifndef __SI_CBUS_DEFS_H__
#define	__SI_CBUS_DEFS_H__

// Device Power State
#define MHD_DEV_UNPOWERED		0x00
#define MHD_DEV_INACTIVE		0x01
#define MHD_DEV_QUIET			0x03
#define MHD_DEV_ACTIVE			0x04

// Version that this chip supports
#define	MHD_VER_MAJOR		(0x01 << 4)	// bits 4..7
#define	MHD_VER_MINOR		0x00		// bits 0..3
#define MHD_VERSION						(MHD_VER_MAJOR | MHD_VER_MINOR)

/*
#define	CBUS_VER_MAJOR		(0x01 << 4)	// bits 4..7
#define	CBUS_VER_MINOR		0x00		// bits 0..3
#define		MHD_CBUS_VERSION				(CBUS_VER_MAJOR | CBUS_VER_MINOR)
*/

//Device Category
#define	MHD_DEV_CAT_SOURCE					0x00
#define	MHD_DEV_CAT_SINGLE_INPUT_SINK		0x01
#define	MHD_DEV_CAT_MULTIPLE_INPUT_SINK		0x02
#define	MHD_DEV_CAT_UNPOWERED_DONGLE		0x03
#define	MHD_DEV_CAT_SELF_POWERED_DONGLE		0x04
#define	MHD_DEV_CAT_HDCP_REPEATER			0x05
#define	MHD_DEV_CAT_NON_DISPLAY_SINK		0x06
#define	MHD_DEV_CAT_POWER_NEUTRAL_SINK		0x07
#define	MHD_DEV_CAT_OTHER					0x80

//Video Link Mode
#define	MHD_DEV_VID_LINK_SUPPRGB444			0x01
#define	MHD_DEV_VID_LINK_SUPPYCBCR444		0x02
#define	MHD_DEV_VID_LINK_SUPPYCBCR422		0x04
#define	MHD_DEV_VID_LINK_PPIXEL				0x08
#define	MHD_DEV_VID_LINK_SUPP_ISLANDS		0x10

//Audio Link Mode Support
#define	MHD_DEV_AUD_LINK_2CH				0x01
#define	MHD_DEV_AUD_LINK_8CH				0x02

/*
#define		MHD_POWER_SUPPLY_CAPACITY		16		// 160 mA current
#define		MHD_POWER_SUPPLY_PROVIDED		16		// 160mA 0r 0 for Wolverine.
#define		MHD_HDCP_STATUS					0		// Bits set dynamically
*/

// VIDEO TYPES
#define		MHD_VT_GRAPHICS					0x00		
#define		MHD_VT_PHOTO					0x02		
#define		MHD_VT_CINEMA					0x04		
#define		MHD_VT_GAMES					0x08		
#define		MHD_SUPP_VT						0x80		

//Logical Dev Map
#define	MHD_DEV_LD_DISPLAY					(0x01 << 0)
#define	MHD_DEV_LD_VIDEO					(0x01 << 1)
#define	MHD_DEV_LD_AUDIO					(0x01 << 2)
#define	MHD_DEV_LD_MEDIA					(0x01 << 3)
#define	MHD_DEV_LD_TUNER					(0x01 << 4)
#define	MHD_DEV_LD_RECORD					(0x01 << 5)
#define	MHD_DEV_LD_SPEAKER					(0x01 << 6)
#define	MHD_DEV_LD_GUI						(0x01 << 7)

//Bandwidth
#define	MHD_BANDWIDTH_LIMIT					22		// 225 MHz

//Feature Support
#define	MHD_RCP_SUPPORT						0x01
#define	MHD_RAP_SUPPORT						0x02


//#define		MHD_DEVCAP_SIZE				16
#define		MHD_INTERRUPT_SIZE				4
//#define		MHD_STATUS_SIZE				4
#define		MHD_SCRATCHPAD_SIZE				16
#define		MHD_MAX_BUFFER_SIZE				MHD_SCRATCHPAD_SIZE	// manually define highest number
//------------------------------------------------------------------------------
//
// MHD Specs defined registers in device capability set
//
//
typedef struct {
	unsigned char	mhd_devcap_version;					// 0x00
	unsigned char	mhd_devcap_cbus_version;			// 0x01
	unsigned char	mhd_devcap_device_category;			// 0x02
	unsigned char	mhd_devcap_power_supply_capacity;	// 0x03
   	unsigned char	mhd_devcap_power_supply_provided;	// 0x04
   	unsigned char	mhd_devcap_video_link_mode_support;	// 0x05
   	unsigned char	mhd_devcap_audio_link_mode_support;	// 0x06
   	unsigned char	mhd_devcap_hdcp_status;				// 0x07
   	unsigned char	mhd_devcap_logical_device_map;		// 0x08
   	unsigned char	mhd_devcap_link_bandwidth_limit;	// 0x09
   	unsigned char	mhd_devcap_reserved_1;				// 0x0a
   	unsigned char	mhd_devcap_reserved_2;				// 0x0b
   	unsigned char	mhd_devcap_reserved_3;				// 0x0c
   	unsigned char	mhd_devcap_scratchpad_size;			// 0x0d
   	unsigned char	mhd_devcap_interrupt_size;			// 0x0e
   	unsigned char	mhd_devcap_devcap_size;				// 0x0f

} mhd_devcap_t;
//------------------------------------------------------------------------------
//
// MHD Specs defined registers for interrupts
//
//
typedef struct {

	unsigned char	mhd_intr_0;		// 0x00
	unsigned char	mhd_intr_1;		// 0x01
	unsigned char	mhd_intr_2;		// 0x02
	unsigned char	mhd_intr_3;		// 0x03

} mhd_interrupt_t;
//------------------------------------------------------------------------------
//
// MHD Specs defined registers for status
//
//
typedef struct {

	unsigned char	mhd_status_0;	// 0x00
	unsigned char	mhd_status_1;	// 0x01
	unsigned char	mhd_status_2;	// 0x02
	unsigned char	mhd_status_3;	// 0x03

} mhd_status_t;
//------------------------------------------------------------------------------
//
// MHD Specs defined registers for local scratchpad registers
//
//
typedef struct {

	unsigned char	mhd_scratchpad[16];

} mhd_scratchpad_t;
//------------------------------------------------------------------------------

/*Daniel added */
#define SINK_DEV_TYPE 0x01
#define SOURCE_DEV_TYPE 0x02
#define Dongle_DEV_TYPE 0x03
#define POW_EN_DEV_TYPE 0x10

//------- END OF DEFINES -------------
#endif	// __SI_CBUS_DEFS_H__
