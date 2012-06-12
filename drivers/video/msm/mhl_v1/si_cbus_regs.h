/***************************************************************************
*
* file     si_cbus_regs.h
* brief    MHD/CBUS Registers Manifest Constants.
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
#ifndef __SI_CBUS_REGS_H__
#define __SI_CBUS_REGS_H__

//------------------------------------------------------------------------------
// NOTE: Register addresses are 16 bit values with page and offset combined.
//
// Examples:  0x005 = page 0, offset 0x05
//            0x1B6 = page 1, offset 0xB6
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// CBUS 0 Registers in Page 12     (0xE6)
//------------------------------------------------------------------------------

#define	CBUSREG_MAX_REG					0xFF

#define CBUS_PORT1_OFFSET               0x80

#define	REG_CBUS_INTR_STATUS_2				0x05	//
#define	REG_CBUS_INTR_ENABLE_2				0x06	//

#define REG_CBUS_SUPPORT                0xC07
#define	BIT_VS_ENABLE                       0x01    // Enable MSC_MSG
#define	BIT_PING_ENABLE                     0x02    // Enable PING to test the presence

#define REG_CBUS_INTR_STATUS            0xC08
#define BIT_CONNECT_CHG                     0x01
#define BIT_DDC_ABORT                       0x04    // Responder aborted DDC command at translation layer
#define BIT_MSC_MSG_RCV                     0x08    // Responder sent a VS_MSG packet (response data or command.)
#define BIT_MSC_XFR_DONE                    0x10    // Responder sent ACK packet (not VS_MSG)
#define BIT_MSC_XFR_ABORT                   0x20    // Command send aborted on TX side?????
#define BIT_MSC_ABORT                       0x40    // Responder aborted MSC command at translation layer
#define BIT_HEARTBEAT_TIMEOUT               0x80    // Responder stopped sending 'alive' messages.

#define REG_CBUS_INTR_ENABLE            0xC09

#define REG_DDC_ABORT_REASON        	0xC0C
#define REG_CBUS_BUS_STATUS             0xC0A
#define BIT_BUS_CONNECTED                   0x01
#define BIT_LA_VAL_CHG                      0x02

#define REG_PRI_XFR_ABORT_REASON        0xC0D

#define REG_CBUS_PRI_FWR_ABORT_REASON   0xC0E
#define	CBUSABORT_BIT_REQ_MAXFAIL			(0x01 << 0)
#define	CBUSABORT_BIT_PROTOCOL_ERROR		(0x01 << 1)
#define	CBUSABORT_BIT_REQ_TIMEOUT			(0x01 << 2)
#define	CBUSABORT_BIT_UNDEFINED_OPCODE		(0x01 << 3)
#define	CBUSSTATUS_BIT_CONNECTED			(0x01 << 6)
#define	CBUSABORT_BIT_PEER_ABORTED			(0x01 << 7)

#define REG_CBUS_PRI_START              0xC12
#define BIT_TRANSFER_PVT_CMD                0x01
#define BIT_SEND_MSC_MSG                    0x02
#define	MSC_START_BIT_MSC_CMD		        (0x01 << 0)
#define	MSC_START_BIT_VS_CMD		        (0x01 << 1)
#define	MSC_START_BIT_READ_REG		        (0x01 << 2)
#define	MSC_START_BIT_WRITE_REG		        (0x01 << 3)
#define	MSC_START_BIT_WRITE_BURST	        (0x01 << 4)

#define REG_CBUS_PRI_ADDR_CMD           0xC13
#define REG_CBUS_PRI_WR_DATA_1ST        0xC14
#define REG_CBUS_PRI_WR_DATA_2ND        0xC15
#define REG_CBUS_PRI_RD_DATA_1ST        0xC16
#define REG_CBUS_PRI_RD_DATA_2ND        0xC17


#define REG_CBUS_PRI_VS_CMD             0xC18
#define REG_CBUS_PRI_VS_DATA            0xC19

#define	REG_MSC_WRITE_BURST_LEN         0xC20       // only for WRITE_BURST
#define	MSC_REQUESTOR_DONE_NACK         	(0x01 << 6)      

#define	REG_CBUS_MSC_RETRY_INTERVAL			0x1A		// default is 16
#define	REG_CBUS_DDC_FAIL_LIMIT				0x1C		// default is 5
#define	REG_CBUS_MSC_FAIL_LIMIT				0x1D		// default is 5
#define	REG_CBUS_MSC_INT2_STATUS        	0xC1E
#define REG_CBUS_MSC_INT2_ENABLE             	0xC1F
	#define	MSC_INT2_REQ_WRITE_MSC              (0x01 << 0)	// Write REG data written.
	#define	MSC_INT2_HEARTBEAT_MAXFAIL          (0x01 << 1)	// Retry threshold exceeded for sending the Heartbeat

#define	REG_MSC_WRITE_BURST_LEN         0xC20       // only for WRITE_BURST

#define	REG_MSC_HEARTBEAT_CONTROL       0xC21       // Periodic heart beat. TX sends GET_REV_ID MSC command
#define	MSC_HEARTBEAT_PERIOD_MASK		    0x0F	// bits 3..0
#define	MSC_HEARTBEAT_FAIL_LIMIT_MASK	    0x70	// bits 6..4
#define	MSC_HEARTBEAT_ENABLE			    0x80	// bit 7

#define REG_MSC_TIMEOUT_LIMIT           0xC22
#define	MSC_TIMEOUT_LIMIT_MSB_MASK	        (0x0F)	        // default is 1
#define	MSC_LEGACY_BIT					    (0x01 << 7)	    // This should be cleared.

#define	REG_CBUS_LINK_CONTROL_1				0xC30	// 
#define	REG_CBUS_LINK_CONTROL_2				0xC31	// 
#define	REG_CBUS_LINK_CONTROL_3				0xC32	// 
#define	REG_CBUS_LINK_CONTROL_4				0xC33	// 
#define	REG_CBUS_LINK_CONTROL_5				0xC34	// 
#define	REG_CBUS_LINK_CONTROL_6				0xC35	// 
#define	REG_CBUS_LINK_CONTROL_7				0xC36	// 
#define REG_CBUS_LINK_STATUS_1          	0xC37
#define REG_CBUS_LINK_STATUS_2          	0xC38
#define	REG_CBUS_LINK_CONTROL_8				0xC39	// 
#define	REG_CBUS_LINK_CONTROL_9				0xC3A	// 
#define	REG_CBUS_LINK_CONTROL_10				0xC3B	// 
#define	REG_CBUS_LINK_CONTROL_11				0xC3C	// 
#define	REG_CBUS_LINK_CONTROL_12				0xC3D	// 


#define REG_CBUS_LINK_CTRL9_0           0xC3A
#define REG_CBUS_LINK_CTRL9_1           0xCBA

#define	REG_CBUS_DRV_STRENGTH_0				0xC40	// 
#define	REG_CBUS_DRV_STRENGTH_1				0xC41	// 
#define	REG_CBUS_ACK_CONTROL				0xC42	// 
#define	REG_CBUS_CAL_CONTROL				0xC43	// Calibration

#define REG_CBUS_SCRATCHPAD_0           0xCC0
#define REG_CBUS_DEVICE_CAP_0           0xC80
#define REG_CBUS_DEVICE_CAP_1           0xC81
#define REG_CBUS_DEVICE_CAP_2           0xC82
#define REG_CBUS_DEVICE_CAP_3           0xC83
#define REG_CBUS_DEVICE_CAP_4           0xC84
#define REG_CBUS_DEVICE_CAP_5           0xC85
#define REG_CBUS_DEVICE_CAP_6           0xC86
#define REG_CBUS_DEVICE_CAP_7           0xC87
#define REG_CBUS_DEVICE_CAP_8           0xC88
#define REG_CBUS_DEVICE_CAP_9           0xC89
#define REG_CBUS_DEVICE_CAP_A           0xC8A
#define REG_CBUS_DEVICE_CAP_B           0xC8B
#define REG_CBUS_DEVICE_CAP_C           0xC8C
#define REG_CBUS_DEVICE_CAP_D           0xC8D
#define REG_CBUS_DEVICE_CAP_E           0xC8E
#define REG_CBUS_DEVICE_CAP_F           0xC8F
#define REG_CBUS_SET_INT_0				0xCA0
#define REG_CBUS_SET_INT_1				0xCA1
#define REG_CBUS_SET_INT_2				0xCA2
#define REG_CBUS_SET_INT_3				0xCA3
#define REG_CBUS_WRITE_STAT_0        	0xCB0
#define REG_CBUS_WRITE_STAT_1        	0xCB1
#define REG_CBUS_WRITE_STAT_2        	0xCB2
#define REG_CBUS_WRITE_STAT_3        	0xCB3

//------------------------------------------------------------------------------
// EDID Registers in Page 9     (0xE0)
//------------------------------------------------------------------------------

#define REG_CBUS_PORT_SEL_AUTO          0x9E6
#define REG_CBUS_PORT_OVWR_MODE         0x9E7
#define REG_CBUS_PORT_OVWR_VALUE        0x9E8

#define REG_CBUS_DU_MODE        			0x9E1
#define REG_CBUS_DU_MODE_DELAY				0x00
#define REG_CBUS_DU_CNT_5					0x01
#define REG_CBUS_DU_CNT_7					0x03
#define REG_CBUS_DU_CNT_9					0x05
#define REG_CBUS_DU_CNT_11					0x07
#endif  // __SI_CBUS_REGS_H__

