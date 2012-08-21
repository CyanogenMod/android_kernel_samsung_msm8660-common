/****************************************************************************

                       Common_Def.h
              
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

#ifndef Common_Def_H
#define Common_Def_H

#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/pmic8058.h>


typedef u8 byte;
typedef u32 word;
typedef bool Bool;


#define RCP_ENABLE 	1
#define MSC_TESTER	0

#define IS_CBUS 1  
#define MHD_MAX_CHANNELS  1   // Number of MDHI channels

// Generic Constants
//==================
#define FALSE                   0
#define TRUE                    1

#define OFF                     0
#define ON                      1

#define LOW                     0
#define HIGH                    1

// Generic Masks
//==============
#define LOW_BYTE                0x00FF

#define LOW_NIBBLE              0x0F
#define HI_NIBBLE               0xF0

#define MSBIT                   0x80
#define LSBIT                   0x01

#define _ZERO					0x00

#define BIT_0                   0x01
#define BIT_1                   0x02
#define BIT_2                   0x04
#define BIT_3                   0x08
#define BIT_4                   0x10
#define BIT_5                   0x20
#define BIT_6                   0x40
#define BIT_7                   0x80


//NAGSM_Android_SEL_Kernel_Aakash_20101206

// Device Power State
#define MHL_DEV_UNPOWERED		0x00
#define MHL_DEV_INACTIVE		0x01
#define MHL_DEV_QUIET			0x03
#define MHL_DEV_ACTIVE			0x04

// Version that this chip supports
#define	MHL_VER_MAJOR		(0x01 << 4)	// bits 4..7
#define	MHL_VER_MINOR		0x00		// bits 0..3
#define MHL_VERSION						(MHL_VER_MAJOR | MHL_VER_MINOR)

//Device Category
#define	MHL_DEV_CATEGORY_OFFSET				0x02
#define	MHL_DEV_CATEGORY_POW_BIT			(BIT_4)

#define	MHL_DEV_CAT_SOURCE					0x00
#define	MHL_DEV_CAT_SINGLE_INPUT_SINK		0x01
#define	MHL_DEV_CAT_MULTIPLE_INPUT_SINK		0x02
#define	MHL_DEV_CAT_UNPOWERED_DONGLE		0x03
#define	MHL_DEV_CAT_SELF_POWERED_DONGLE		0x04
#define	MHL_DEV_CAT_HDCP_REPEATER			0x05
#define	MHL_DEV_CAT_NON_DISPLAY_SINK		0x06
#define	MHL_DEV_CAT_POWER_NEUTRAL_SINK		0x07
#define	MHL_DEV_CAT_OTHER					0x80

//Video Link Mode
#define	MHL_DEV_VID_LINK_SUPPRGB444			0x01
#define	MHL_DEV_VID_LINK_SUPPYCBCR444		0x02
#define	MHL_DEV_VID_LINK_SUPPYCBCR422		0x04
#define	MHL_DEV_VID_LINK_PPIXEL				0x08
#define	MHL_DEV_VID_LINK_SUPP_ISLANDS		0x10

//Audio Link Mode Support
#define	MHL_DEV_AUD_LINK_2CH				0x01
#define	MHL_DEV_AUD_LINK_8CH				0x02


//Feature Flag in the devcap
#define	MHL_DEV_FEATURE_FLAG_OFFSET			0x0A
#define	MHL_FEATURE_RCP_SUPPORT				BIT_0	// Dongles have freedom to not support RCP
#define	MHL_FEATURE_RAP_SUPPORT				BIT_1	// Dongles have freedom to not support RAP
#define	MHL_FEATURE_SP_SUPPORT				BIT_2	// Dongles have freedom to not support SCRATCHPAD

/*
#define		MHL_POWER_SUPPLY_CAPACITY		16		// 160 mA current
#define		MHL_POWER_SUPPLY_PROVIDED		16		// 160mA 0r 0 for Wolverine.
#define		MHL_HDCP_STATUS					0		// Bits set dynamically
*/

// VIDEO TYPES
#define		MHL_VT_GRAPHICS					0x00		
#define		MHL_VT_PHOTO					0x02		
#define		MHL_VT_CINEMA					0x04		
#define		MHL_VT_GAMES					0x08		
#define		MHL_SUPP_VT						0x80		

//Logical Dev Map
#define	MHL_DEV_LD_DISPLAY					(0x01 << 0)
#define	MHL_DEV_LD_VIDEO					(0x01 << 1)
#define	MHL_DEV_LD_AUDIO					(0x01 << 2)
#define	MHL_DEV_LD_MEDIA					(0x01 << 3)
#define	MHL_DEV_LD_TUNER					(0x01 << 4)
#define	MHL_DEV_LD_RECORD					(0x01 << 5)
#define	MHL_DEV_LD_SPEAKER					(0x01 << 6)
#define	MHL_DEV_LD_GUI						(0x01 << 7)

//Bandwidth
#define	MHL_BANDWIDTH_LIMIT					22		// 225 MHz


#define	MHL_STATUS_DCAP_RDY					BIT_0
#define	MHL_INT_DCAP_CHG					BIT_0

// On INTR_1 the EDID_CHG is located at BIT 0
#define	MHL_INT_EDID_CHG					BIT_1

#define		MHL_INT_AND_STATUS_SIZE			0x303		// This contains one nibble each - max offset
#define		MHL_SCRATCHPAD_SIZE				16
#define		MHL_MAX_BUFFER_SIZE				MHL_SCRATCHPAD_SIZE	// manually define highest number



enum
{
    MHL_MSC_MSG_RCP             = 0x10,     // RCP sub-command
    MHL_MSC_MSG_RCPK            = 0x11,     // RCP Acknowledge sub-command
    MHL_MSC_MSG_RCPE            = 0x12,     // RCP Error sub-command
    MHL_MSC_MSG_RAP             = 0x20,     // Mode Change Warning sub-command
    MHL_MSC_MSG_RAPK            = 0x21     // MCW Acknowledge sub-command
};

//
// MHL spec related defines
//
enum
{
	MHL_ACK						= 0x33,	// Command or Data byte acknowledge
	MHL_NACK					= 0x34,	// Command or Data byte not acknowledge
	MHL_ABORT					= 0x35,	// Transaction abort
	MHL_WRITE_STAT				= 0x60 | 0x80,	// Write one status register strip top bit
	MHL_SET_INT					= 0x60,	// Write one interrupt register
	MHL_READ_DEVCAP				= 0x61,	// Read one register
	MHL_GET_STATE				= 0x62,	// Read CBUS revision level from follower
	MHL_GET_VENDOR_ID			= 0x63,	// Read vendor ID value from follower.
	MHL_SET_HPD					= 0x64,	// Set Hot Plug Detect in follower
	MHL_CLR_HPD					= 0x65,	// Clear Hot Plug Detect in follower
	MHL_SET_CAP_ID				= 0x66,	// Set Capture ID for downstream device.
	MHL_GET_CAP_ID				= 0x67,	// Get Capture ID from downstream device.
	MHL_MSC_MSG					= 0x68,	// VS command to send RCP sub-commands
	MHL_GET_SC1_ERRORCODE		= 0x69,	// Get Vendor-Specific command error code.
	MHL_GET_DDC_ERRORCODE		= 0x6A,	// Get DDC channel command error code.
	MHL_GET_MSC_ERRORCODE		= 0x6B,	// Get MSC command error code.
	MHL_WRITE_BURST				= 0x6C,	// Write 1-16 bytes to responder?™s scratchpad.
	MHL_GET_SC3_ERRORCODE		= 0x6D	// Get channel 3 command error code.
};

#define	MHL_RAP_CONTENT_ON		0x10	// Turn content streaming ON.
#define	MHL_RAP_CONTENT_OFF		0x11	// Turn content streaming OFF.

///////////////////////////////////////////////////////////////////////////////
//
// MHL Timings applicable to this driver.
//
//
#define	T_SRC_VBUS_CBUS_TO_STABLE	(200)	// 100 - 1000 milliseconds. Per MHL 1.0 Specs
#define	T_SRC_WAKE_PULSE_WIDTH_1	(20)	// 20 milliseconds. Per MHL 1.0 Specs
#define	T_SRC_WAKE_PULSE_WIDTH_2	(60)	// 60 milliseconds. Per MHL 1.0 Specs

#define	T_SRC_WAKE_TO_DISCOVER		(500)	// 100 - 1000 milliseconds. Per MHL 1.0 Specs

#define T_SRC_VBUS_CBUS_T0_STABLE 	(500)

// Allow RSEN to stay low this much before reacting.
// Per specs between 100 to 200 ms
#define	T_SRC_RSEN_DEGLITCH			(150)

// Wait this much after connection before reacting to RSEN (300-500ms)
// Per specs between 300 to 500 ms
#define	T_SRC_RXSENSE_CHK			(400)

//NAGSM_Android_SEL_Kernel_Aakash_20101206


struct i2c_client* get_sii9234_client(u8 device_id);
u8 SII9234_i2c_read(struct i2c_client *client, u8 reg);
int SII9234_i2c_write(struct i2c_client *client, u8 reg, u8 data);
extern void SiI9234_interrupt_event(void);

extern void sii9234_cfg_power(bool on);
extern byte ReadByteTPI (byte Offset);

#define PMIC8058_IRQ_BASE				(NR_MSM_IRQS + NR_GPIO_IRQS)
#define IRQ_MHL_HPD gpio_to_irq(172)

#define PM8058_GPIO_BASE			NR_MSM_GPIOS
#ifndef PM8058_GPIO_PM_TO_SYS
#define PM8058_GPIO_PM_TO_SYS(pm_gpio)		(pm_gpio + PM8058_GPIO_BASE)
#endif

#define IRQ_MHL_INT_9 PM8058_GPIO_IRQ(PMIC8058_IRQ_BASE, (PM8058_GPIO(9)))
#define IRQ_MHL_INT_31 PM8058_GPIO_IRQ(PMIC8058_IRQ_BASE, (PM8058_GPIO(31)))
#define IRQ_MHL_WAKE_UP PM8058_GPIO_IRQ(PMIC8058_IRQ_BASE, (PM8058_GPIO(17)))

#define GPIO_MHL_RST PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(15))
#define GPIO_MHL_SEL PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(16))
#define GPIO_MHL_WAKE_UP PM8058_GPIO_PM_TO_SYS(PM8058_GPIO(17))
#endif