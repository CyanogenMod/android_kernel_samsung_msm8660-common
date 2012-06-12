/***************************************************************************
*
* file     si_apiCbus.h
* brief    CBUS API
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

#ifndef __SI_APICBUS_H__
#define __SI_APICBUS_H__


#include "Common_Def.h"
#include "si_RegioCbus.h"
#include "si_cbus_regs.h"
#include "si_cbusDefs.h"

//------------------------------------------------------------------------------
// API Manifest Constants
//------------------------------------------------------------------------------

#define CBUS_NOCHANNEL      0xFF
#define CBUS_NOPORT         0xFF


enum
{
    MHD_MSC_MSG_RCP             = 0x10,     // RCP sub-command
    MHD_MSC_MSG_RCPK            = 0x11,     // RCP Acknowledge sub-command
    MHD_MSC_MSG_RCPE            = 0x12,     // RCP Error sub-command
    MHD_MSC_MSG_RAP             = 0x20,     // Mode Change Warning sub-command
    MHD_MSC_MSG_RAPK            = 0x21     // MCW Acknowledge sub-command
};

enum
{
	MHD_MSC_MSG_NEW_TESTER	             	= 0x80,
	MHD_MSC_MSG_NEW_TESTER_REPLY			= 0x81,

  MHD_MSC_MSG_STATE_CHANGE             	= 0x90,
  MHD_MSC_MSG_STATE_CHANGE_REPLY			= 0x91,
 
	MHD_MSC_MSG_DEVCAP0_CHANGE            	= 0xA0,   
	MHD_MSC_MSG_DEVCAP1_CHANGE            	= 0xA1,    
	MHD_MSC_MSG_DEVCAP2_CHANGE            	= 0xA2,    
	MHD_MSC_MSG_DEVCAP3_CHANGE            	= 0xA3,    
	MHD_MSC_MSG_DEVCAP4_CHANGE            	= 0xA4,    
	MHD_MSC_MSG_DEVCAP5_CHANGE            	= 0xA5,    
	MHD_MSC_MSG_DEVCAP6_CHANGE            	= 0xA6,   
	MHD_MSC_MSG_DEVCAP7_CHANGE            	= 0xA7,   
	MHD_MSC_MSG_DEVCAP8_CHANGE            	= 0xA8,   
	MHD_MSC_MSG_DEVCAP9_CHANGE            	= 0xA9,   
	MHD_MSC_MSG_DEVCAP10_CHANGE            	= 0xAA,    
	MHD_MSC_MSG_DEVCAP11_CHANGE            	= 0xAB,    
	MHD_MSC_MSG_DEVCAP12_CHANGE            	= 0xAC,  
	MHD_MSC_MSG_DEVCAP13_CHANGE            	= 0xAD,   
	MHD_MSC_MSG_DEVCAP14_CHANGE            	= 0xAE,    
	MHD_MSC_MSG_DEVCAP15_CHANGE            	= 0xAF,
    MHD_MSC_MSG_DEVCAP_CHANGE_REPLY			= 0xB0,

	MHD_MSC_MSG_SET_INT0_CHECK				= 0xC0,
	MHD_MSC_MSG_SET_INT1_CHECK				= 0xC1,
	MHD_MSC_MSG_SET_INT2_CHECK				= 0xC2,
	MHD_MSC_MSG_SET_INT3_CHECK				= 0xC3,
	MHD_MSC_MSG_SET_INT_REPLY				= 0xC4,

	MHD_MSC_MSG_WRITE_STAT0_CHECK			= 0xD0,
	MHD_MSC_MSG_WRITE_STAT1_CHECK			= 0xD1,
	MHD_MSC_MSG_WRITE_STAT2_CHECK			= 0xD2,
	MHD_MSC_MSG_WRITE_STAT3_CHECK			= 0xD3,
	MHD_MSC_MSG_WRITE_STAT_REPLY			= 0xD4
};

// RCPK/RCPE sub commands
enum
{
    MHD_MSC_MSG_NO_ERROR        		= 0x00,     // RCP No Error
    MHD_MSC_MSG_INEFFECTIVE_KEY_CODE  	= 0x01,     // The key code in the RCP sub-command is not recognized
    MHD_MSC_MSG_RESPONDER_BUSY          = 0x02     // RCP Response busy
};

//RAPK sub commands
enum
{
    MHD_MSC_MSG_RAP_NO_ERROR        		= 0x00,     // RAP No Error
    MHD_MSC_MSG_RAP_UNRECOGNIZED_ACT_CODE  	= 0x01,     
    MHD_MSC_MSG_RAP_UNSUPPORTED_ACT_CODE  	= 0x02,      			
    MHD_MSC_MSG_RAP_RESPONDER_BUSY   		= 0x03     
};

enum
{
  MHD_RCP_CMD_SELECT          = 0x00,
  MHD_RCP_CMD_UP              = 0x01,
  MHD_RCP_CMD_DOWN            = 0x02,
  MHD_RCP_CMD_LEFT            = 0x03,
  MHD_RCP_CMD_RIGHT           = 0x04,
  MHD_RCP_CMD_RIGHT_UP        = 0x05,
  MHD_RCP_CMD_RIGHT_DOWN      = 0x06,
  MHD_RCP_CMD_LEFT_UP         = 0x07,
  MHD_RCP_CMD_LEFT_DOWN       = 0x08,
  MHD_RCP_CMD_ROOT_MENU       = 0x09,
  MHD_RCP_CMD_SETUP_MENU      = 0x0A,
  MHD_RCP_CMD_CONTENTS_MENU   = 0x0B,
  MHD_RCP_CMD_FAVORITE_MENU   = 0x0C,
  MHD_RCP_CMD_EXIT            = 0x0D,
	
	//0x0E - 0x1F are reserved

	MHD_RCP_CMD_NUM_0           = 0x20,
  MHD_RCP_CMD_NUM_1           = 0x21,
  MHD_RCP_CMD_NUM_2           = 0x22,
  MHD_RCP_CMD_NUM_3           = 0x23,
  MHD_RCP_CMD_NUM_4           = 0x24,
  MHD_RCP_CMD_NUM_5           = 0x25,
  MHD_RCP_CMD_NUM_6           = 0x26,
  MHD_RCP_CMD_NUM_7           = 0x27,
  MHD_RCP_CMD_NUM_8           = 0x28,
  MHD_RCP_CMD_NUM_9           = 0x29,

	MHD_RCP_CMD_DOT             = 0x2A,
	MHD_RCP_CMD_ENTER           = 0x2B,
	MHD_RCP_CMD_CLEAR           = 0x2C,

	//0x2D - 0x2F are reserved

  MHD_RCP_CMD_CH_UP           = 0x30,
  MHD_RCP_CMD_CH_DOWN         = 0x31,
  MHD_RCP_CMD_PRE_CH          = 0x32,
  MHD_RCP_CMD_SOUND_SELECT    = 0x33,
  MHD_RCP_CMD_INPUT_SELECT    = 0x34,
  MHD_RCP_CMD_SHOW_INFO       = 0x35,
  MHD_RCP_CMD_HELP            = 0x36,
  MHD_RCP_CMD_PAGE_UP         = 0x37,
  MHD_RCP_CMD_PAGE_DOWN       = 0x38,

	//0x39 - 0x40 are reserved

  MHD_RCP_CMD_VOL_UP	        = 0x41,
  MHD_RCP_CMD_VOL_DOWN        = 0x42,
  MHD_RCP_CMD_MUTE            = 0x43,
  MHD_RCP_CMD_PLAY            = 0x44,
  MHD_RCP_CMD_STOP            = 0x45,
  MHD_RCP_CMD_PAUSE           = 0x46,
  MHD_RCP_CMD_RECORD          = 0x47,
  MHD_RCP_CMD_REWIND          = 0x48,
  MHD_RCP_CMD_FAST_FWD        = 0x49,
  MHD_RCP_CMD_EJECT           = 0x4A,
  MHD_RCP_CMD_FWD             = 0x4B,
  MHD_RCP_CMD_BKWD            = 0x4C,

	//0x4D - 0x4F are reserved

  MHD_RCP_CMD_ANGLE            = 0x50,
  MHD_RCP_CMD_SUBPICTURE       = 0x51,

//0x52 - 0x5F are reserved

  MHD_RCP_CMD_PLAY_FUNC       = 0x60,
  MHD_RCP_CMD_PAUSE_PLAY_FUNC = 0x61,
  MHD_RCP_CMD_RECORD_FUNC     = 0x62,
  MHD_RCP_CMD_PAUSE_REC_FUNC  = 0x63,
  MHD_RCP_CMD_STOP_FUNC       = 0x64,
MHD_RCP_CMD_MUTE_FUNC       = 0x65,
  MHD_RCP_CMD_UN_MUTE_FUNC    = 0x66,
  MHD_RCP_CMD_TUNE_FUNC       = 0x67,
  MHD_RCP_CMD_MEDIA_FUNC      = 0x68,

//0x69 - 0x70 are reserved

  MHD_RCP_CMD_F1              = 0x71,
  MHD_RCP_CMD_F2              = 0x72,
  MHD_RCP_CMD_F3              = 0x73,
  MHD_RCP_CMD_F4              = 0x74,
  MHD_RCP_CMD_F5              = 0x75,

//0x76 - 0x7D are reserved

  MHD_RCP_CMD_VS              = 0x7E,
  MHD_RCP_CMD_RSVD            = 0x7F

};

enum
{
  MHD_RAP_CMD_POLL         	= 0x00,
  MHD_RAP_CMD_CHG_ACTIVE_PWR  = 0x10,
  MHD_RAP_CMD_CHG_QUIET       = 0x11,
  MHD_RAP_CMD_END             = 0x12
};

//
// MHD spec related defines
//
enum
{
	MHD_ACK						= 0x33,	// Command or Data byte acknowledge
	MHD_NACK					= 0x34,	// Command or Data byte not acknowledge
	MHD_ABORT					= 0x35,	// Transaction abort
	MHD_WRITE_STAT				= 0x60 | 0x80,	// Write one status register strip top bit
	MHD_SET_INT					= 0x60,	// Write one interrupt register
	MHD_READ_DEVCAP				= 0x61,	// Read one register
	MHD_GET_STATE				= 0x62,	// Read CBUS revision level from follower
	MHD_GET_VENDOR_ID			= 0x63,	// Read vendor ID value from follower.
	MHD_SET_HPD					= 0x64,	// Set Hot Plug Detect in follower
	MHD_CLR_HPD					= 0x65,	// Clear Hot Plug Detect in follower
	MHD_SET_CAP_ID				= 0x66,	// Set Capture ID for downstream device.
	MHD_GET_CAP_ID				= 0x67,	// Get Capture ID from downstream device.
	MHD_MSC_MSG					= 0x68,	// VS command to send RCP sub-commands
	MHD_GET_SC1_ERRORCODE		= 0x69,	// Get Vendor-Specific command error code.
	MHD_GET_DDC_ERRORCODE		= 0x6A,	// Get DDC channel command error code.
	MHD_GET_MSC_ERRORCODE		= 0x6B,	// Get MSC command error code.
	MHD_WRITE_BURST				= 0x6C,	// Write 1-16 bytes to responder’s scratchpad.
	MHD_GET_SC3_ERRORCODE		= 0x6D	// Get channel 3 command error code.
};

enum
{
  CBUS_TASK_IDLE,
  CBUS_TASK_TRANSLATION_LAYER_DONE,
  CBUS_TASK_WAIT_FOR_RESPONSE
};

//
// CBUS module reports these error types
//
typedef enum
{
  STATUS_SUCCESS = 0,
  ERROR_CBUS_CAN_RETRY,
  ERROR_CBUS_ABORT,
  ERROR_CBUS_TIMEOUT,
  ERROR_CBUS_LINK_DOWN,
  ERROR_INVALID,
  ERROR_INIT,
  ERROR_WRITE_FAILED,
  ERROR_NACK_FROM_PEER,
  ERROR_NO_HEARTBEAT

} CBUS_SOFTWARE_ERRORS_t;

typedef enum
{
  CBUS_IDLE           = 0,    // BUS idle
  CBUS_SENT,                  // Command sent
  CBUS_XFR_DONE,              // Translation layer complete
  CBUS_WAIT_RESPONSE,         // Waiting for response
  CBUS_RECEIVED              // Message received, verify and send ACK
} CBUS_STATE_t;

typedef enum
{
  CBUS_REQ_IDLE       = 0,
  CBUS_REQ_PENDING,           // Request is waiting to be sent
  CBUS_REQ_SENT,              // Request has been sent
  CBUS_REQ_RECEIVED          // Request data has been received
} CBUS_REQ_t;

//------------------------------------------------------------------------------
// API typedefs
//------------------------------------------------------------------------------

//
// structure to hold command details from upper layer to CBUS module
//
typedef struct 
{
  byte reqStatus;                      // CBUS_IDLE, CBUS_PENDING
  byte command;                        // VS_CMD or RCP opcode
  byte offsetData;                     // Offset of register on CBUS or RCP data
  byte length;                         // Only applicable to write burst. ignored otherwise.
  byte msgData[MHD_MAX_BUFFER_SIZE];   // Pointer to message data area.
  unsigned char	*pdatabytes;			// pointer for write burst or read many bytes
} cbus_req_t;

#define CBUS_MAX_COMMAND_QUEUE      4

typedef struct
{
  Bool  connected;      // True if a connected MHD port
  byte state;          // State of command execution for this channel
                          // IDLE, WAIT_RESPONSE
  byte timeout;        // Timer for message response (must be able to indicate at least 3000ms).
  byte activeIndex;    // Active queue entry.
  cbus_req_t  request[ CBUS_MAX_COMMAND_QUEUE ];
} cbusChannelState_t;

//------------------------------------------------------------------------------
// API Function Templates
//------------------------------------------------------------------------------
Bool SI_CbusInitialize( void );
byte SI_CbusHandler( byte channel );

#if MSC_TESTER
void SI_CbusHeartBeat( byte chn, byte enable);
#endif // MSC_TESTER

    /* CBUS Request functions.      */
byte SI_CbusRequestStatus( byte channel );
void SI_CbusRequestSetIdle( byte channel, byte newState );
cbus_req_t* SI_CbusRequestData( byte channel );

    /* CBUS Channel functions.      */
Bool SI_CbusChannelConnected( byte channel );

    /* RCP Message send functions.  */

Bool SI_CbusMscMsgSubCmdSend( byte channel, byte vsCommand, byte cmdData );
Bool SI_CbusRcpMessageAck( byte channel, byte cmdStatus, byte keyCode );
Bool SI_CbusRapMessageAck( byte channel, byte cmdStatus );
Bool SI_CbusWriteCommand( byte channel, cbus_req_t *pReq );
Bool SI_CbusUpdateBusStatus( byte channel );


#if MSC_TESTER

byte SI_CbusMscReturnCmd();
byte SI_CbusMscReturnValue();
void SI_CbusMscResetReturnCmd();
void SI_CbusMscResetReturnValue();

#endif //MSC_TESTER

	/* looking at the registers */
void cbus_display_registers(int startfrom, int howmany);
Bool SI_CbusSendDcapRdyMsg ( byte channel );


#endif  // __SI_APICBUS_H__

