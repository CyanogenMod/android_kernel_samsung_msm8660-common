/***************************************************************************
*
*  si_apiCbus.c
*
*  CBUS API
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


#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/gpio.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <linux/syscalls.h> 
#include <linux/fcntl.h> 
#include <asm/uaccess.h> 
#include <linux/types.h>




#include "Common_Def.h"

#include "si_apiCbus.h"

#include "si_RegioCbus.h"

#include "si_cbusDefs.h"





#define MHD_DEVICE_CATEGORY             (MHD_DEV_CAT_SOURCE)

#define	MHD_LOGICAL_DEVICE_MAP			(MHD_DEV_LD_AUDIO | MHD_DEV_LD_VIDEO | MHD_DEV_LD_MEDIA | MHD_DEV_LD_GUI )



#define CONFIG_CHECK_DDC_ABORT		0



#define CBUS_FW_INTR_POLL_MILLISECS     50      // wait this to issue next status poll i2c read




//------------------------------------------------------------------------------

//  Global data

//------------------------------------------------------------------------------

static byte msc_return_cmd;
static byte msc_return_value;


static cbusChannelState_t l_cbus[MHD_MAX_CHANNELS];

Bool dev_cap_regs_ready_bit;



//------------------------------------------------------------------------------

// Function:    SI_CbusRequestStatus 

// Description: Return the status of the message currently in process, if any.

// Parameters:  channel - CBUS channel to check

// Returns:     CBUS_REQ_IDLE, CBUS_REQ_PENDING, CBUS_REQ_SENT, or CBUS_REQ_RECEIVED

//------------------------------------------------------------------------------



byte SI_CbusRequestStatus ( byte channel )

{

    return( l_cbus[ channel].request[ l_cbus[ channel].activeIndex ].reqStatus );

}



//------------------------------------------------------------------------------

// Function:    SI_CbusRequestSetIdle

// Description: Set the active request to the specified state 

// Parameters:  channel - CBUS channel to set

//------------------------------------------------------------------------------



void SI_CbusRequestSetIdle ( byte channel, byte newState )

{

    l_cbus[ channel].request[ l_cbus[ channel].activeIndex ].reqStatus = newState;

}



//------------------------------------------------------------------------------

// Function:    SI_CbusRequestData 

// Description: Return a pointer to the currently active request structure

// Parameters:  channel - CBUS channel.

// Returns:     Pointer to a cbus_req_t structure.

//------------------------------------------------------------------------------



cbus_req_t *SI_CbusRequestData ( byte channel )

{

    return( &l_cbus[ channel].request[ l_cbus[ channel].activeIndex ] );

}



//------------------------------------------------------------------------------

// Function:    SI_CbusChannelConnected

// Description: Return the CBUS channel connected status for this channel.

// Returns:     TRUE if connected. 

//              FALSE if disconnected.

//------------------------------------------------------------------------------



Bool SI_CbusChannelConnected (byte channel)

{

    return( l_cbus[ channel].connected );

}



////////////////////////////////////////////////////////////////////////////////

//

// FUNCTION         :   cbus_display_registers

//

// PURPOSE          :   For debugging purposes, show salient register on UART

//

// INPUT PARAMETERS :   startFrom	= First register to show

//						howmany		= number of registers to show.

//									This will be rounded to next multiple of 4.

//

// OUTPUT PARAMETERS:   None.

//

// RETURNED VALUES  :   None

//

// GLOBALS USED     :

//

////////////////////////////////////////////////////////////////////////////////

void cbus_display_registers(int startfrom, int howmany)

{

	int	regnum, regval, i;

	int end_at;



	end_at = startfrom + howmany;



	for(regnum = startfrom; regnum <= end_at;)

	{

		for (i = 0 ; i <= 7; i++, regnum++)

		{

			regval = SiIRegioCbusRead(regnum, 0);

		}

	}

}



//------------------------------------------------------------------------------

// Function:    CBusProcessConnectionChange

// Description: Process a connection change interrupt

// Returns:     

//------------------------------------------------------------------------------



static byte CBusProcessConnectionChange ( int channel )

{

    channel = 0;



    return( ERROR_CBUS_TIMEOUT );

}



//------------------------------------------------------------------------------

// Function:    CBusProcessFailureInterrupts

// Description: Check for and process any failure interrupts.

// Returns:     SUCCESS or ERROR_CBUS_ABORT

//------------------------------------------------------------------------------



static byte CBusProcessFailureInterrupts ( byte channel, byte intStatus, byte inResult )

{



  byte result          = inResult;

  byte mscAbortReason  = STATUS_SUCCESS;

  byte ddcAbortReason  = STATUS_SUCCESS;



    /* At this point, we only need to look at the abort interrupts. */



    intStatus &=  /*BIT_DDC_ABORT |*/ BIT_MSC_ABORT | BIT_MSC_XFR_ABORT;



  if ( intStatus )

  {

      result = ERROR_CBUS_ABORT;		// No Retry will help



      /* If transfer abort or MSC abort, clear the abort reason register. */



	if( intStatus & BIT_CONNECT_CHG )

	{

		printk("CBUS Connection Change Detected\n");

	}

	if( intStatus & BIT_DDC_ABORT )

	{

		ddcAbortReason = SiIRegioCbusRead( REG_DDC_ABORT_REASON, channel );

		printk("CBUS DDC ABORT happened, reason:: %02X\n", (int)(ddcAbortReason));

	}



      if ( intStatus & BIT_MSC_XFR_ABORT )

      {

          mscAbortReason = SiIRegioCbusRead( REG_PRI_XFR_ABORT_REASON, channel );



          printk( "CBUS:: MSC Transfer ABORTED. Clearing 0x0D\n");

          SiIRegioCbusWrite( REG_PRI_XFR_ABORT_REASON, channel, 0xFF );

      }

      if ( intStatus & BIT_MSC_ABORT )

      {

          printk( "CBUS:: MSC Peer sent an ABORT. Clearing 0x0E\n");

          SiIRegioCbusWrite( REG_CBUS_PRI_FWR_ABORT_REASON, channel, 0xFF );

      }



      // Now display the abort reason.



      if ( mscAbortReason != 0 )

      {

          printk( "CBUS:: Reason for ABORT is ....0x%02X = ", (int)mscAbortReason );



          if ( mscAbortReason & CBUSABORT_BIT_REQ_MAXFAIL)

          {

              printk("Requestor MAXFAIL - retry threshold exceeded\n");

          }

          if ( mscAbortReason & CBUSABORT_BIT_PROTOCOL_ERROR)

          {

              printk ("Protocol Error\n");

          }

          if ( mscAbortReason & CBUSABORT_BIT_REQ_TIMEOUT)

          {

              printk ("Requestor translation layer timeout\n");

          }

          if ( mscAbortReason & CBUSABORT_BIT_PEER_ABORTED)

          {

              printk ("Peer sent an abort\n");

          }

          if ( mscAbortReason & CBUSABORT_BIT_UNDEFINED_OPCODE)

          {

              printk ("Undefined opcode\n");

          }

      }

  }

    

	/* Clear any failure interrupt that we received.    */

    SiIRegioCbusWrite( REG_CBUS_INTR_STATUS, channel, intStatus );



    return( result );

}



//------------------------------------------------------------------------------

// Function:    CBusProcessSubCommand

// Description: Process a sub-command (RCP) or sub-command response (RCPK).

//              Modifies channel state as necessary.

// Parameters:  channel - CBUS channel that received the command.

// Returns:     SUCCESS or CBUS_SOFTWARE_ERRORS_t

//              If SUCCESS, command data is returned in l_cbus[channel].msgData[i]

//------------------------------------------------------------------------------



static byte CBusProcessSubCommand ( int channel, byte vs_cmd, byte vs_data )

{

    /* Save RCP message data in the channel request structure to be returned    */

    /* to the upper level.                                                      */



    l_cbus[ channel].request[ l_cbus[ channel].activeIndex ].command    = vs_cmd;

    l_cbus[ channel].request[ l_cbus[ channel].activeIndex ].offsetData = vs_data;



    /* Parse it a little before telling the upper level about it.   */



    switch (vs_cmd)

    {

        case MHD_MSC_MSG_RCP:



            /* Received a Remote Control Protocol message.  Signal that */

            /* it has been received if we are in the right state,       */

            /* otherwise, it is a bogus message.  Don't send RCPK now   */

            /* because the upper layer must validate the command.       */



            printk ("CBUS:: Received <-- MHD_MSC_MSG_RCP:: cbus state = %02X\n", (int)(l_cbus[ channel].state));



            switch ( l_cbus[ channel].state )

            {

                case CBUS_IDLE:

                case CBUS_SENT:

                    l_cbus[ channel].request[ l_cbus[ channel].activeIndex ].reqStatus = CBUS_REQ_RECEIVED;

                    l_cbus[ channel].state = CBUS_RECEIVED;

                    break;

                default:

                    l_cbus[ channel].state = CBUS_IDLE;

                    break;

            }



            break;

        case MHD_MSC_MSG_RCPK:

            printk ("CBUS:: Received <-- MHD_MSC_MSG_RCPK\n");

            break;

        case MHD_MSC_MSG_RCPE:

            printk ("CBUS:: Received <-- MHD_MSC_MSG_RCPE\n");

            break;

        case MHD_MSC_MSG_RAP:

			      printk ("CBUS:: Received <-- MHD_MSC_MSG_RAP:: cbus state = %02X\n", (int)(l_cbus[ channel].state));



            switch ( l_cbus[ channel].state )

            {

                case CBUS_IDLE:

                case CBUS_SENT:

                    l_cbus[ channel].request[ l_cbus[ channel].activeIndex ].reqStatus = CBUS_REQ_RECEIVED;

                    l_cbus[ channel].state = CBUS_RECEIVED;

                    break;

                default:

                    l_cbus[ channel].state = CBUS_IDLE;

                    break;

            }

            break;

        case MHD_MSC_MSG_RAPK:

			      printk ("CBUS:: Received <-- MHD_MSC_MSG_RAPK\n");

            break;



#if MSC_TESTER

		case MHD_MSC_MSG_NEW_TESTER:

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_NEW_TESTER_REPLY, 0x00);

			break;

		case MHD_MSC_MSG_NEW_TESTER_REPLY:

			msc_return_cmd = vs_cmd;

			break;

		case MHD_MSC_MSG_STATE_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_0, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_STATE_CHANGE_REPLY, 0x00);

			break;

		case MHD_MSC_MSG_DEVCAP0_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_0, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_0, channel);

			break;

		case MHD_MSC_MSG_DEVCAP1_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_1, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_1, channel);

			break;

		case MHD_MSC_MSG_DEVCAP2_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_2, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_2, channel);

			break;

		case MHD_MSC_MSG_DEVCAP3_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_3, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_3, channel);

			break;

		case MHD_MSC_MSG_DEVCAP4_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_4, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_4, channel);

			break;

		case MHD_MSC_MSG_DEVCAP5_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_5, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_5, channel);

			break;

		case MHD_MSC_MSG_DEVCAP6_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_6, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_6, channel);

			break;

		case MHD_MSC_MSG_DEVCAP7_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_7, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_7, channel);

			break;

		case MHD_MSC_MSG_DEVCAP8_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_8, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_8, channel);

			break;

		case MHD_MSC_MSG_DEVCAP9_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_9, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_9, channel);

			break;		

		case MHD_MSC_MSG_DEVCAP10_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_A, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_A, channel);

			break;		

		case MHD_MSC_MSG_DEVCAP11_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_B, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_B, channel);

			break;

		case MHD_MSC_MSG_DEVCAP12_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_C, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_C, channel);

			break;

		case MHD_MSC_MSG_DEVCAP13_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_D, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_D, channel);

			break;

		case MHD_MSC_MSG_DEVCAP14_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_E, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_E, channel);

			break;

		case MHD_MSC_MSG_DEVCAP15_CHANGE:

			SiIRegioCbusWrite( REG_CBUS_DEVICE_CAP_F, channel, vs_data );

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_DEVCAP_CHANGE_REPLY, SiIRegioCbusRead( REG_CBUS_DEVICE_CAP_F, channel);

			break;

		case MHD_MSC_MSG_SET_INT0_CHECK:

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_SET_INT_REPLY, SiIRegioCbusRead( REG_CBUS_SET_INT_0, channel );

			break;

 		case MHD_MSC_MSG_SET_INT1_CHECK:

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_SET_INT_REPLY, SiIRegioCbusRead( REG_CBUS_SET_INT_1, channel );

			break;

		case MHD_MSC_MSG_SET_INT2_CHECK:

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_SET_INT_REPLY, SiIRegioCbusRead( REG_CBUS_SET_INT_2, channel );

			break;

		case MHD_MSC_MSG_SET_INT3_CHECK:

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_SET_INT_REPLY, SiIRegioCbusRead( REG_CBUS_SET_INT_3, channel );

			break;

		case MHD_MSC_MSG_WRITE_STAT0_CHECK:

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_WRITE_STAT_REPLY, SiIRegioCbusRead( REG_CBUS_WRITE_STAT_0, channel );

			break;

 		case MHD_MSC_MSG_WRITE_STAT1_CHECK:

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_WRITE_STAT_REPLY, SiIRegioCbusRead( REG_CBUS_WRITE_STAT_1, channel );

			break;

 		case MHD_MSC_MSG_WRITE_STAT2_CHECK:

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_WRITE_STAT_REPLY, SiIRegioCbusRead( REG_CBUS_WRITE_STAT_2, channel );

			break;

 		case MHD_MSC_MSG_WRITE_STAT3_CHECK:

			SI_CbusMscMsgSubCmdSend(channel, MHD_MSC_MSG_WRITE_STAT_REPLY, SiIRegioCbusRead( REG_CBUS_WRITE_STAT_3, channel );

			break;

		case MHD_MSC_MSG_STATE_CHANGE_REPLY:

			msc_return_cmd = vs_cmd;

			break;

		case MHD_MSC_MSG_DEVCAP_CHANGE_REPLY:

			msc_return_cmd = vs_cmd;

			break;	

		case MHD_MSC_MSG_SET_INT_REPLY:

			msc_return_value = vs_data;

			msc_return_cmd = vs_cmd;

			break;

		case MHD_MSC_MSG_WRITE_STAT_REPLY:

			msc_return_value = vs_data;

			msc_return_cmd = vs_cmd;

			break;

#endif



		default:

			break;

    }



    printk ("CBUS:: MSG_MSC CMD:  0x%02X\n", (int)vs_cmd );

    printk ("CBUS:: MSG_MSC Data: 0x%02X\n\n", (int)vs_data );



    return( STATUS_SUCCESS );

}

#if MSC_TESTER



void SI_CbusMscResetReturnCmd()

{

	msc_return_cmd = 0;

}



void SI_CbusMscResetReturnValue()

{

	msc_return_value = 0;

}



byte	SI_CbusMscReturnCmd()

{

	return msc_return_cmd;

}



byte	SI_CbusMscReturnValue()

{

	return msc_return_value;

}

#endif // MSC_TESTER





//------------------------------------------------------------------------------

// Function:    CBusWriteCommand

// Description: Write the specified Sideband Channel command to the CBUS.

//              Command can be a MSC_MSG command (RCP/MCW/RAP), or another command 

//              such as READ_DEVCAP, GET_VENDOR_ID, SET_HPD, CLR_HPD, etc.

//

// Parameters:  channel - CBUS channel to write

//              pReq    - Pointer to a cbus_req_t structure containing the 

//                        command to write

// Returns:     TRUE    - successful write

//              FALSE   - write failed

//------------------------------------------------------------------------------



static Bool CBusWriteCommand ( int channel, cbus_req_t *pReq  )

{

    byte i, startbit;

    Bool  success = TRUE;

	

    printk ("CBUS:: Sending MSC command %02X, %02X, %02X\n", (int)pReq->command, (int)pReq->msgData[0], (int)pReq->msgData[1]);



    /****************************************************************************************/

    /* Setup for the command - write appropriate registers and determine the correct        */

    /*                         start bit.                                                   */

    /****************************************************************************************/



  	// Set the offset and outgoing data byte right away

  	SiIRegioCbusWrite( REG_CBUS_PRI_ADDR_CMD, channel, pReq->offsetData); 	// set offset

  	SiIRegioCbusWrite( REG_CBUS_PRI_WR_DATA_1ST, channel, pReq->msgData[0] );

  	

    startbit = 0x00;

    switch ( pReq->command )

    {

		  case MHD_SET_INT:	// Set one interrupt register = 0x60

			  SiIRegioCbusWrite( REG_CBUS_PRI_ADDR_CMD, channel, pReq->offsetData + 0x20 ); 	// set offset

			  startbit = MSC_START_BIT_WRITE_REG;

			break;



      case MHD_WRITE_STAT:	// Write one status register = 0x60 | 0x80

        SiIRegioCbusWrite( REG_CBUS_PRI_ADDR_CMD, channel, pReq->offsetData + 0x30 ); 	// set offset

        startbit = MSC_START_BIT_WRITE_REG;

        break;



      case MHD_READ_DEVCAP:	// Read one device capability register = 0x61

        startbit = MSC_START_BIT_READ_REG;

        break;



      case MHD_GET_STATE:			// 0x62 - Used for heartbeat

      case MHD_GET_VENDOR_ID:		// 0x63 - for vendor id	

      case MHD_SET_HPD:			// 0x64	- Set Hot Plug Detect in follower

      case MHD_CLR_HPD:			// 0x65	- Clear Hot Plug Detect in follower

      case MHD_GET_SC1_ERRORCODE:		// 0x69	- Get channel 1 command error code

      case MHD_GET_DDC_ERRORCODE:		// 0x6A	- Get DDC channel command error code.

      case MHD_GET_MSC_ERRORCODE:		// 0x6B	- Get MSC command error code.

      case MHD_GET_SC3_ERRORCODE:		// 0x6D	- Get channel 3 command error code.

        SiIRegioCbusWrite( REG_CBUS_PRI_ADDR_CMD, channel, pReq->command );

        startbit = MSC_START_BIT_MSC_CMD;

        break;



      case MHD_MSC_MSG:

        SiIRegioCbusWrite( REG_CBUS_PRI_WR_DATA_2ND, channel, pReq->msgData[1] );

        SiIRegioCbusWrite( REG_CBUS_PRI_ADDR_CMD, channel, pReq->command );

        startbit = MSC_START_BIT_VS_CMD;

        break;



      case MHD_WRITE_BURST:

        SiIRegioCbusWrite( REG_CBUS_PRI_ADDR_CMD, channel, pReq->offsetData + 0x40 );

        SiIRegioCbusWrite( REG_MSC_WRITE_BURST_LEN, channel, pReq->length -1 );

        printk ("CBUS:: pReq->length: 0x%02X\n\n", (int)pReq->length );



        // Now copy all bytes from array to local scratchpad



        for ( i = 0; i < pReq->length; i++ )

        {

            SiIRegioCbusWrite( REG_CBUS_SCRATCHPAD_0 + i, channel, pReq->msgData[i] );

        }

        startbit = MSC_START_BIT_WRITE_BURST;

        break;



      default:

        success = FALSE;

        break;

    }



    /****************************************************************************************/

    /* Trigger the CBUS command transfer using the determined start bit.                    */

    /****************************************************************************************/



    if( success )

    {

      SiIRegioCbusWrite( REG_CBUS_PRI_START, channel, startbit );

    }



    return( success );

}



//------------------------------------------------------------------------------

// Function:    CBusConmmandGetNextInQueue

// Description: find out the next command in the queue to be sent

// Parameters:  channel - CBUS channel that received the command.

// Returns:     the index of the next command

//------------------------------------------------------------------------------



static byte CBusConmmandGetNextInQueue( byte channel )

{

	byte   result = STATUS_SUCCESS;



	byte nextIndex = (l_cbus[ channel].activeIndex == (CBUS_MAX_COMMAND_QUEUE - 1)) ? 

		 0 : (l_cbus[ channel].activeIndex + 1)	;

   



	while ( l_cbus[ channel].request[ nextIndex].reqStatus != CBUS_REQ_PENDING )

	{

		if ( nextIndex == l_cbus[ channel].activeIndex )   //searched whole queue, no pending 

			return 0;	

			

	    nextIndex = ( nextIndex == (CBUS_MAX_COMMAND_QUEUE - 1)) ? 

			 		0 : (nextIndex + 1);

	}

  

  printk("channel:%x nextIndex:%x \n",(int)channel,(int)nextIndex);



  if ( CBusWriteCommand( channel, &l_cbus[ channel].request[ nextIndex] ) )

  {

    l_cbus[ channel].request[ nextIndex].reqStatus = CBUS_REQ_SENT;

    l_cbus[ channel].activeIndex = nextIndex;						

    l_cbus[ channel].state = CBUS_SENT;					

  }

  else

  {

    printk ( "CBUS:: CBusWriteCommand failed\n" );

    result = ERROR_WRITE_FAILED;

  }



	return result;



}


#if 0//not used
//------------------------------------------------------------------------------

// Function:    CBusResetToIdle

// Description: Set the specified channel state to IDLE. Clears any messages that

//              are in progress or queued.  Usually used if a channel connection 

//              changed or the channel heartbeat has been lost.

// Parameters:  channel - CBUS channel to reset

//------------------------------------------------------------------------------



static void CBusResetToIdle ( byte channel )

{

    byte queueIndex;



    l_cbus[ channel].state = CBUS_IDLE;

    

    for ( queueIndex = 0; queueIndex < CBUS_MAX_COMMAND_QUEUE; queueIndex++ )

    {

        l_cbus[ channel].request[ queueIndex].reqStatus = CBUS_REQ_IDLE;

    }

}
#endif



//------------------------------------------------------------------------------

// Function:    CBusCheckInterruptStatus

// Description: If any interrupts on the specified channel are set, process them.

// Parameters:  channel - CBUS channel to check

// Returns:     SUCCESS or CBUS_SOFTWARE_ERRORS_t error code.

//------------------------------------------------------------------------------



static byte CBusCheckInterruptStatus ( byte channel )

{

  byte 	intStatus, result;

  byte     vs_cmd=0, vs_data=0;

  byte 	writeBurstLen 	= 0;

    

	/* Read CBUS interrupt status.  */

    intStatus = SiIRegioCbusRead( REG_CBUS_INTR_STATUS, channel );

	if( intStatus & BIT_MSC_MSG_RCV )

  {

    printk("( intStatus & BIT_MSC_MSG_RCV )\n");

    vs_cmd  = SiIRegioCbusRead( REG_CBUS_PRI_VS_CMD, channel );

    vs_data = SiIRegioCbusRead( REG_CBUS_PRI_VS_DATA, channel );

  }

	SiIRegioCbusWrite( REG_CBUS_INTR_STATUS, channel, intStatus );



    /* Check for interrupts.  */



    result = STATUS_SUCCESS;

	  intStatus &= (~BIT_HEARTBEAT_TIMEOUT);	 //don't check heartbeat

    if( intStatus != 0 )

    {

		    if( intStatus & BIT_CONNECT_CHG )

        {

          printk("( intStatus & BIT_CONNECT_CHG )\n");

            /* The connection change interrupt has been received.   */

            result = CBusProcessConnectionChange( channel );

			      SiIRegioCbusWrite( REG_CBUS_BUS_STATUS, channel, BIT_CONNECT_CHG );

        }



		    if( intStatus & BIT_MSC_XFR_DONE )

        {

          printk("( intStatus & BIT_MSC_XFR_DONE )\n");

            /* A previous MSC sub-command has been acknowledged by the responder.   */

            /* Does not include MSC MSG commands.                                   */



            l_cbus[ channel].state = CBUS_XFR_DONE;



            /* Save any response data in the channel request structure to be returned    */

            /* to the upper level.                                                      */



            msc_return_cmd = l_cbus[ channel].request[ l_cbus[ channel].activeIndex ].msgData[0] = 

            SiIRegioCbusRead( REG_CBUS_PRI_RD_DATA_1ST, channel );

            msc_return_value = l_cbus[ channel].request[ l_cbus[ channel].activeIndex ].msgData[1] = 

            SiIRegioCbusRead( REG_CBUS_PRI_RD_DATA_2ND, channel );



            printk ( "\nCBUS:: Transfer Done \n" );

            printk ("Response data Received:: %02X\n\n", (int)l_cbus[ channel].request[l_cbus[channel].activeIndex].msgData[0]);



            result = STATUS_SUCCESS;



            // Check if we received NACK from Peer

            writeBurstLen = SiIRegioCbusRead( REG_MSC_WRITE_BURST_LEN, channel );

            if( writeBurstLen & MSC_REQUESTOR_DONE_NACK )

            {

              result = ERROR_NACK_FROM_PEER;

              printk( "NACK received!!! :: %02X\n", (int)writeBurstLen) ;

            }

            result = CBusProcessFailureInterrupts( channel, intStatus, result );

        }



		if( intStatus & BIT_MSC_MSG_RCV )

    {

      printk("( intStatus & BIT_MSC_MSG_RCV )\n");

        /* Receiving a sub-command, either an actual command or */

        /* the response to a command we sent.                   */

        result = CBusProcessSubCommand( channel, vs_cmd, vs_data );

    }

  }

    return( result );

}

#if MSC_TESTER



//------------------------------------------------------------------------------

// Function:    SI_CbusHeartBeat

// Description:  Enable/Disable Heartbeat

//------------------------------------------------------------------------------



void SI_CbusHeartBeat ( byte channel, byte enable)

{

	byte  value;

    value = SiIRegioCbusRead( REG_MSC_HEARTBEAT_CONTROL, channel);

    SiIRegioCbusWrite( REG_MSC_HEARTBEAT_CONTROL, channel, enable ? (value | MSC_HEARTBEAT_ENABLE) : ( value & 0x7F) );

}

#endif // MSC_TESTER



//------------------------------------------------------------------------------

// Function:    SI_CbusMscMsgSubCmdSend

// Description: Send MSC_MSG (RCP) message to the specified CBUS channel (port)

//

// Parameters:  channel     - CBUS channel

//              vsCommand   - MSC_MSG cmd (RCP, RCPK or RCPE)

//              cmdData     - MSC_MSG data

// Returns:     TRUE        - successful queue/write

//              FALSE       - write and/or queue failed

//------------------------------------------------------------------------------



Bool SI_CbusMscMsgSubCmdSend ( byte channel, byte vsCommand, byte cmdData )

{

	cbus_req_t	req;



	//

	// Send MSC_MSG command (Vendor Specific command)

	//

	req.command     = MHD_MSC_MSG;

	req.msgData[0]  = vsCommand; 

	req.msgData[1]  = cmdData; 

  return( SI_CbusWriteCommand( channel, &req  ));

}



//------------------------------------------------------------------------------

// Function:    SI_CbusRcpMessageAck

// Description: Send RCP_K (acknowledge) message to the specified CBUS channel 

//              and set the request status to idle.

//

// Parameters:  channel     - CBUS channel

// Returns:     TRUE        - successful queue/write

//              FALSE       - write and/or queue failed

//------------------------------------------------------------------------------



Bool SI_CbusRcpMessageAck ( byte channel, byte cmdStatus, byte keyCode )

{



  SI_CbusRequestSetIdle( channel, CBUS_REQ_IDLE );

	if(cmdStatus != MHD_MSC_MSG_NO_ERROR)

	{

        SI_CbusMscMsgSubCmdSend( channel, MHD_MSC_MSG_RCPE, cmdStatus );

	}

    return( SI_CbusMscMsgSubCmdSend( channel, MHD_MSC_MSG_RCPK, keyCode ));

}





//------------------------------------------------------------------------------

// Function:    SI_CbusRapMessageAck

// Description: Send RAPK (acknowledge) message to the specified CBUS channel 

//              and set the request status to idle.

//

// Parameters:  channel     - CBUS channel

// Returns:     TRUE        - successful queue/write

//              FALSE       - write and/or queue failed

//------------------------------------------------------------------------------



Bool SI_CbusRapMessageAck ( byte channel, byte cmdStatus )

{

    printk("SI_CbusRapMessageAck:%x \n",(int)cmdStatus);  

    SI_CbusRequestSetIdle( channel, CBUS_REQ_IDLE );

    return( SI_CbusMscMsgSubCmdSend( channel, MHD_MSC_MSG_RAPK, cmdStatus ));

}



//------------------------------------------------------------------------------

// Function:    SI_CbusSendDcapRdyMsg

// Description: Send a msg to peer informing the devive capability registers are

//				ready to be read.

// Parameters:  channel to check

// Returns:     TRUE    - success

//              FALSE   - failure

//------------------------------------------------------------------------------

Bool SI_CbusSendDcapRdyMsg ( byte channel )

{

	//cbus_req_t *pReq; // SIMG old code 

	cbus_req_t pReq;  //daniel 20101101

	Bool result = TRUE;



	if( l_cbus[ channel].connected ) 

	{

		printk( "SI_CbusSendDcapRdyMsg Called!!\n");

		//send a msg to peer that the device capability registers are ready to be read.

		//set DCAP_RDY bit 

		pReq.command = MHD_WRITE_STAT;

		pReq.offsetData = 0x00;

		pReq.msgData[0] = BIT_0;

   	//result = SI_CbusWriteCommand(0, pReq); // SIMG old code 

		result = SI_CbusWriteCommand(0, &pReq);

	

		//set DCAP_CHG bit

		pReq.command = MHD_SET_INT;

		pReq.offsetData = 0x00;

		pReq.msgData[0] = BIT_0;

	  //result = SI_CbusWriteCommand(0, pReq); // SIMG old code 

		result = SI_CbusWriteCommand(0, &pReq);



		dev_cap_regs_ready_bit = TRUE;

	}



	return result;

}





//------------------------------------------------------------------------------

// Function:    SI_CbusHandler

// Description: Check the state of any current CBUS message on specified channel.

//              Handle responses or failures and send any pending message if 

//              channel is IDLE.

// Parameters:  channel - CBUS channel to check, must be in range, NOT 0xFF

// Returns:     SUCCESS or one of CBUS_SOFTWARE_ERRORS_t

//------------------------------------------------------------------------------



byte SI_CbusHandler ( byte channel )

{

    byte result = STATUS_SUCCESS;



    /* Check the channel interrupt status to see if anybody is  */

    /* talking to us. If they are, talk back.                   */



    result = CBusCheckInterruptStatus( channel );



    /* Don't bother with the rest if the heart is gone. */



    if ( (result == ERROR_NO_HEARTBEAT) || (result == ERROR_NACK_FROM_PEER) )

    {

        printk("SI_CbusHandler:: CBusCheckInterruptStatus returned -->> %02X\n", (int)result);

        return( result );

    }



    /* Update the channel state machine as necessary.   */



    switch ( l_cbus[ channel].state )

    {

    case CBUS_IDLE:

		result = CBusConmmandGetNextInQueue( channel );				

        break;



    case CBUS_SENT:

        break;



    case CBUS_XFR_DONE:

		l_cbus[ channel].state      = CBUS_IDLE;

        

		/* We may be waiting for a response message, but the    */

        /* request queue is idle.                               */

        l_cbus[ channel].request[ l_cbus[channel].activeIndex].reqStatus = CBUS_REQ_IDLE;

        break;



    case CBUS_WAIT_RESPONSE:

        break;



    case CBUS_RECEIVED:

		// printk(MSG_ALWAYS, ("SI_CbusHandler:: l_cbus[ channel].state -->> %02X\n", (int)(l_cbus[ channel].state));

		// printk(MSG_ALWAYS, ("result -->> %02X\n", (int)(result));



        /* Either command or response data has been received.   */



        break;



    default:

        /* Not a valid state, reset to IDLE and get out with failure. */



        l_cbus[ channel].state = CBUS_IDLE;

        result = ERROR_INVALID;

        break;

    }



    return( result );

}



//------------------------------------------------------------------------------

// Function:    SI_CbusWriteCommand

// Description: Place a command in the CBUS message queue.  If queue was empty,

//              send the new command immediately.

//

// Parameters:  channel - CBUS channel to write

//              pReq    - Pointer to a cbus_req_t structure containing the 

//                        command to write

// Returns:     TRUE    - successful queue/write

//              FALSE   - write and/or queue failed

//------------------------------------------------------------------------------



Bool SI_CbusWriteCommand ( byte channel, cbus_req_t *pReq  )

{

    byte queueIndex;

    Bool  success = FALSE;



	if ( l_cbus[ channel].connected )

	{

    printk("CBUS Write command\n");

    /* Copy the request to the queue.   */



	//printk ( "SI_CbusWriteCommand:: Channel State: %02X\n", (int)l_cbus[ channel].state );

    for ( queueIndex = 0; queueIndex < CBUS_MAX_COMMAND_QUEUE; queueIndex++ )

    {

        if ( l_cbus[ channel].request[ queueIndex].reqStatus == CBUS_REQ_IDLE )

        {

            /* Found an idle queue entry, copy the request and set to pending.  */



            memcpy( &l_cbus[ channel].request[ queueIndex], pReq, sizeof( cbus_req_t ));

            l_cbus[ channel].request[ queueIndex].reqStatus = CBUS_REQ_PENDING;

            success = TRUE;

            break;

        }

    }



    /* If successful at putting the request into the queue, decide  */

    /* whether it can be sent now or later.                         */

    printk("state:%x \n",(int)l_cbus[ channel].state);

    printk("channel:%x queueIndex:%x \n",(int)channel,(int)queueIndex);

    printk ("CBUS:: Sending MSC command %02X, %02X, %02X\n", (int)pReq->command, (int)pReq->msgData[0], (int)pReq->msgData[1]);

    if ( success )

    {

        switch ( l_cbus[ channel].state )

        {

            case CBUS_IDLE:

            case CBUS_RECEIVED:	   

				      success = CBusConmmandGetNextInQueue( channel );				

                break;

            case CBUS_WAIT_RESPONSE:

            case CBUS_SENT:

            case CBUS_XFR_DONE:



                /* Another command is in progress, the Handler loop will    */

                /* send the new command when the bus is free.               */



                //printk ( "CBUS:: Channel State: %02X\n", (int)l_cbus[ channel].state );

                break;



            default:



                /* Illegal values return to IDLE state.     */



                printk ( "CBUS:: Channel State: %02X (illegal)\n", (int)l_cbus[ channel].state );

                l_cbus[ channel].state = CBUS_IDLE;

                l_cbus[ channel].request[ queueIndex].reqStatus = CBUS_REQ_IDLE;

                success = FALSE;

                break;

        }

    }

    else

    {

        printk( "CBUS:: Queue full - Request0: %02X Request1: %02X\n",

            (int)l_cbus[ channel].request[ 0].reqStatus,

            (int)l_cbus[ channel].request[ 1].reqStatus);

    }

	}



    return( success );

}



//------------------------------------------------------------------------------

// Function:    SI_CbusUpdateBusStatus

// Description: Check the BUS status interrupt register for this channel and

//              update the channel data as needed.

//              channel.

// Parameters:  channel to check

// Returns:     TRUE    - connected

//              FALSE   - not connected

//

//  Note: This function should be called periodically to update the bus status

//

//------------------------------------------------------------------------------



Bool SI_CbusUpdateBusStatus ( byte channel )

{

    byte busStatus;



    busStatus = SiIRegioCbusRead( REG_CBUS_BUS_STATUS, channel );



    printk("CBUS status:%x \n",(int)busStatus);

    

    l_cbus[ channel].connected = (busStatus & BIT_BUS_CONNECTED) != 0;



    printk("CBUS connected:%x \n",(int)l_cbus[ channel].connected);

    /* Clear the interrupt register bits.   */



    SiIRegioCbusWrite( REG_CBUS_BUS_STATUS, channel, busStatus );



    return( l_cbus[ channel].connected );

}



//------------------------------------------------------------------------------

// Function:    SI_CbusInitialize

// Description: Attempts to intialize the CBUS. If register reads return 0xFF,

//              it declares error in initialization.

//              Initializes discovery enabling registers and anything needed in

//              config register, interrupt masks.

// Returns:     TRUE if no problem

//------------------------------------------------------------------------------



Bool SI_CbusInitialize ( void )

{

	byte     channel;

	int	result = STATUS_SUCCESS;

//	int	port = 0;

	word	devcap_reg;

	int 		regval;



   	memset( &l_cbus, 0, sizeof( l_cbus ));

	dev_cap_regs_ready_bit = FALSE;



    /* Determine the Port Switch input ports that are selected for MHD  */

    /* operation and initialize the port to channel decode array.       */



    channel = 0;



	//

	// Setup local DEVCAP registers for read by the peer

	//

	devcap_reg = REG_CBUS_DEVICE_CAP_0;

	SiIRegioCbusWrite(devcap_reg++, channel, MHD_DEV_ACTIVE);

	SiIRegioCbusWrite(devcap_reg++, channel, MHD_VERSION);

	SiIRegioCbusWrite(devcap_reg++, channel, MHD_DEVICE_CATEGORY);

	SiIRegioCbusWrite(devcap_reg++, channel, 0);  						

	SiIRegioCbusWrite(devcap_reg++, channel, 0);						

	SiIRegioCbusWrite(devcap_reg++, channel, MHD_DEV_VID_LINK_SUPPRGB444);

	SiIRegioCbusWrite(devcap_reg++, channel, MHD_DEV_AUD_LINK_2CH);

	SiIRegioCbusWrite(devcap_reg++, channel, 0);										// not for source

	SiIRegioCbusWrite(devcap_reg++, channel, MHD_LOGICAL_DEVICE_MAP);

	SiIRegioCbusWrite(devcap_reg++, channel, 0);										// not for source

	SiIRegioCbusWrite(devcap_reg++, channel, MHD_RCP_SUPPORT | MHD_RAP_SUPPORT);		// feature flag

	SiIRegioCbusWrite(devcap_reg++, channel, 0);

	SiIRegioCbusWrite(devcap_reg++, channel, 0);										// reserved

	SiIRegioCbusWrite(devcap_reg++, channel, MHD_SCRATCHPAD_SIZE);

	SiIRegioCbusWrite(devcap_reg++, channel, MHD_INTERRUPT_SIZE);

	SiIRegioCbusWrite(devcap_reg++, channel, 0);										//reserved



	if(SiIRegioCbusRead(REG_CBUS_SUPPORT, channel) == 0xff)

	{

		// Display all registers for debugging. Only at initialization.

		printk( "cbus initialization failed\n");

		cbus_display_registers(0, 0x30);

		return ERROR_INIT;

	}



//NAGSM_Android_SEL_Kernel_Aakash_20101206
	SiIRegioCbusWrite(REG_CBUS_INTR_ENABLE, channel, (BIT_CONNECT_CHG | BIT_MSC_MSG_RCV | BIT_MSC_XFR_DONE	| BIT_MSC_XFR_ABORT | BIT_MSC_ABORT | BIT_HEARTBEAT_TIMEOUT ));
//NAGSM_Android_SEL_Kernel_Aakash_20101206

	regval = SiIRegioCbusRead(REG_CBUS_LINK_CONTROL_2, channel);

	regval = (regval | 0x0C);

	SiIRegioCbusWrite(REG_CBUS_LINK_CONTROL_2, channel, regval);





	 // Clear legacy bit on Wolverine TX.

    regval = SiIRegioCbusRead( REG_MSC_TIMEOUT_LIMIT, channel );

    SiIRegioCbusWrite( REG_MSC_TIMEOUT_LIMIT, channel, (regval & MSC_TIMEOUT_LIMIT_MSB_MASK));



	// Set NMax to 1

	SiIRegioCbusWrite( REG_CBUS_LINK_CONTROL_1, channel, 0x01);



	printk( "cbus_initialize. Poll interval = %d ms. CBUS Connected = %d\n", (int)CBUS_FW_INTR_POLL_MILLISECS, (int)SI_CbusChannelConnected(channel));



	return result;

}





