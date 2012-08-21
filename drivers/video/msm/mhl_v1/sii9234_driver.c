/******************************************************************************
*
*                        SiI9234 Driver Processor
*              
*
* DESCRIPTION
*  This file explains the SiI9234 initialization and call the virtual main function.
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



/*===========================================================================


                      EDIT HISTORY FOR FILE


when              who                         what, where, why
--------        ---                        ----------------------------------------------------------
2010/10/25    Daniel Lee(Philju)      Initial version of file, SIMG Korea 
2011/04/06    Rajkumar c m            added support for qualcomm msm8060
===========================================================================*/


/*===========================================================================
                     INCLUDE FILES FOR MODULE
===========================================================================*/

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
#include <linux/miscdevice.h>

#include <linux/syscalls.h> 
#include <linux/fcntl.h> 
#include <asm/uaccess.h> 

#include "SiI9234_Reg.h"
#include "Common_Def.h"
#include "SiI9234_I2C_master.h"
#include "SiI9234_I2C_slave_add.h"
#include "si_cbusDefs.h"
#include "si_cbus_regs.h"
#include "si_cbus.h"
#include "si_apiCbus.h"

/*===========================================================================

  Definition                   
===========================================================================*/
#define TX_HW_RESET_PERIOD      200


#define SiI_DEVICE_ID           0xB0
#define SiI_DEVICE_ID_9234		0xB4


#define DDC_XLTN_TIMEOUT_MAX_VAL		0x30


#define INDEXED_PAGE_0		0x01
#define INDEXED_PAGE_1		0x02
#define INDEXED_PAGE_2		0x03

#define ASR_VALUE 0x04

#define	TX_POWER_STATE_D0_NO_MHL		TX_POWER_STATE_D2
#define	TX_POWER_STATE_D0_MHL			TX_POWER_STATE_D0
#define	TX_POWER_STATE_FIRST_INIT		0xFF
#define MHL_INIT_POWER_OFF        0x00
#define MHL_POWER_ON              0x01
#define MHL_1K_IMPEDANCE_VERIFIED 0x02
#define MHL_RSEN_VERIFIED         0x04
#define MHL_TV_OFF_CABLE_CONNECT 0x08

#define TX_DEBUG_PRINT(x) printk x



#define	I2C_READ_MODIFY_WRITE(saddr,offset,mask)	I2C_WriteByte(saddr, offset, I2C_ReadByte(saddr, offset) | (mask));

#define	SET_BIT(saddr,offset,bitnumber)		I2C_READ_MODIFY_WRITE(saddr,offset, (1<<bitnumber))
#define	CLR_BIT(saddr,offset,bitnumber)		I2C_WriteByte(saddr, offset, I2C_ReadByte(saddr, offset) & ~(1<<bitnumber))
//
// 90[0] = Enable / Disable MHL Discovery on MHL link
//
#define	DISABLE_DISCOVERY				CLR_BIT(SA_TX_Page0_Primary, 0x90, 0);
#define	ENABLE_DISCOVERY				SET_BIT(SA_TX_Page0_Primary, 0x90, 0);
//
//	Look for interrupts on INTR_4 (Register 0x74)
//		7 = PVT_HTBT(reserved)
//		6 = RGND RDY		(interested)
//		5 = VBUS low(interested)	
//		4 = CBUS LKOUT		(reserved)
//		3 = USB EST		(reserved)
//		2 = MHL EST		(reserved)
//		1 = RPWR5V CHANGE		(reserved)
//		0 = SCDT CHANGE		(reserved)
#define	INTR_4_DESIRED_MASK				( BIT_2 | BIT_3 | BIT_4 | BIT_6) 
#define	UNMASK_INTR_4_INTERRUPTS		I2C_WriteByte(SA_TX_Page0_Primary, 0x78, 0x00) 
#define	MASK_INTR_4_INTERRUPTS	I2C_WriteByte(SA_TX_Page0_Primary, 0x78, INTR_4_DESIRED_MASK)
#define	MASK_INTR_4_RGND		I2C_WriteByte(SA_TX_Page0_Primary, 0x78, BIT_6)

//	Look for interrupts on INTR_1 (Register 0x71)
//		7 = RSVD		(reserved)
//		6 = MDI_HPD		(interested)
//		5 = RSEN CHANGED(interested)	
//		4 = RSVD		(reserved)
//		3 = RSVD		(reserved)
//		2 = RSVD		(reserved)
//		1 = RSVD		(reserved)
//		0 = RSVD		(reserved)
//3355
#define	INTR_1_DESIRED_MASK				(BIT_5|BIT_6) 
#define	UNMASK_INTR_1_INTERRUPTS		I2C_WriteByte(SA_TX_Page0_Primary, 0x75, 0x00)
#define	MASK_INTR_1_INTERRUPTS			I2C_WriteByte(SA_TX_Page0_Primary, 0x75, INTR_1_DESIRED_MASK)
//3355
//	Look for interrupts on CBUS:CBUS_INTR_STATUS [0xC8:0x08]
//		7 = RSVD			(reserved)
//		6 = MSC_RESP_ABORT	(interested)
//		5 = MSC_REQ_ABORT	(interested)	
//		4 = MSC_REQ_DONE	(interested)
//		3 = MSC_MSG_RCVD	(interested)
//		2 = DDC_ABORT		(interested)
//		1 = RSVD			(reserved)
//		0 = rsvd			(reserved)
#define	INTR_CBUS1_DESIRED_MASK			(BIT_2 | BIT_3 | BIT_4 | BIT_5 | BIT_6)
#define	UNMASK_CBUS1_INTERRUPTS		I2C_WriteByte(SA_TX_CBUS_Primary, 0x09, 0x00)	
#define	MASK_CBUS1_INTERRUPTS			I2C_WriteByte(SA_TX_CBUS_Primary, 0x09, INTR_CBUS1_DESIRED_MASK)

#define	INTR_CBUS2_DESIRED_MASK			(BIT_2 | BIT_3)
#define	UNMASK_CBUS2_INTERRUPTS		 I2C_WriteByte(SA_TX_CBUS_Primary, 0x1F, 0x00)	
#define	MASK_CBUS2_INTERRUPTS			 I2C_WriteByte(SA_TX_CBUS_Primary, 0x1F, INTR_CBUS2_DESIRED_MASK)

#define		MHL_TX_EVENT_NONE			0x00	/* No event worth reporting.  */
#define		MHL_TX_EVENT_DISCONNECTION	0x01	/* MHL connection has been lost */
#define		MHL_TX_EVENT_CONNECTION		0x02	/* MHL connection has been established */
#define		MHL_TX_EVENT_RCP_READY		0x03	/* MHL connection is ready for RCP */
//
#define		MHL_TX_EVENT_RCP_RECEIVED	0x04	/* Received an RCP. Key Code in "eventParameter" */
#define		MHL_TX_EVENT_RCPK_RECEIVED	0x05	/* Received an RCPK message */
#define		MHL_TX_EVENT_RCPE_RECEIVED	0x06	/* Received an RCPE message .*/

/* To use hrtimer*/
#define	MS_TO_NS(x)	(x * 1000000)

DECLARE_WAIT_QUEUE_HEAD(wake_wq);

static struct hrtimer hr_wake_timer;

static bool wakeup_time_expired;

static bool hrtimer_initialized;
static bool first_timer;

enum hrtimer_restart hrtimer_wakeup_callback(struct hrtimer *timer)
{
	wake_up(&wake_wq);
	wakeup_time_expired = true;
//	hrtimer_cancel(&hr_wake_timer);
	return HRTIMER_NORESTART;
}


void start_hrtimer_ms(unsigned long delay_in_ms)
{
	ktime_t ktime;
	ktime = ktime_set(0, MS_TO_NS(delay_in_ms));

	wakeup_time_expired = false;
//	hrtimer_init(&hr_wake_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	if (first_timer)
		first_timer = false;
	else
		hrtimer_cancel(&hr_wake_timer);

//	hr_wake_timer.function = &hrtimer_wakeup_callback;
	hrtimer_start(&hr_wake_timer, ktime, HRTIMER_MODE_REL);
}


/*===========================================================================
                
===========================================================================*/
//
// To remember the current power state.
//
byte	fwPowerState = TX_POWER_STATE_FIRST_INIT;

#if 0//not used
//
// When MHL Fifo underrun or overrun happens, we set this flag
// to avoid calling a function in recursive manner. The monitoring loop
// would look at this flag and call appropriate function and clear this flag.
//
static	bool	gotFifoUnderRunOverRun = FALSE;

//
// This flag is set to TRUE as soon as a INT1 RSEN CHANGE interrupt arrives and
// a deglitch timer is started.
//
// We will not get any further interrupt so the RSEN LOW status needs to be polled
// until this timer expires.
//
static	bool	deglitchingRsenNow = FALSE;
#endif

//
// To serialize the RCP commands posted to the CBUS engine, this flag
// is maintained by the function SiiMhlTxDrvSendCbusCommand()
//
static	bool		mscCmdInProgress;	// FALSE when it is okay to send a new command
//
// Preserve Downstream HPD status
//
static	byte	dsHpdStatus = 0;

byte mhl_cable_status =MHL_INIT_POWER_OFF;			//NAGSM_Android_SEL_Kernel_Aakash_20101214

#define	MHL_MAX_RCP_KEY_CODE	(0x7F + 1)	// inclusive
byte		rcpSupportTable [MHL_MAX_RCP_KEY_CODE] = {
	(MHL_DEV_LD_GUI),		// 0x00 = Select
	(MHL_DEV_LD_GUI),		// 0x01 = Up
	(MHL_DEV_LD_GUI),		// 0x02 = Down
	(MHL_DEV_LD_GUI),		// 0x03 = Left
	(MHL_DEV_LD_GUI),		// 0x04 = Right
	0, 0, 0, 0,				// 05-08 Reserved
	(MHL_DEV_LD_GUI),		// 0x09 = Root Menu
	0, 0, 0,				// 0A-0C Reserved
	(MHL_DEV_LD_GUI),		// 0x0D = Select
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 0E-1F Reserved
	0,//(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),	// Numeric keys 0x20-0x29
	0,//	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	0,//	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	0,//	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	0,//	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	0,//	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	0,//	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	0,//	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	0,//	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	0,//	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),
	0,						// 0x2A = Dot
	0,//	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),	// Enter key = 0x2B
	0,//	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_TUNER),	// Clear key = 0x2C
	0, 0, 0,				// 2D-2F Reserved
	0,//	(MHL_DEV_LD_TUNER),		// 0x30 = Channel Up
	0,//	(MHL_DEV_LD_TUNER),		// 0x31 = Channel Dn
	0,//	(MHL_DEV_LD_TUNER),		// 0x32 = Previous Channel
	0,//	(MHL_DEV_LD_AUDIO),		// 0x33 = Sound Select
	0,						// 0x34 = Input Select
	0,						// 0x35 = Show Information
	0,						// 0x36 = Help
	0,						// 0x37 = Page Up
	0,						// 0x38 = Page Down
	0, 0, 0, 0, 0, 0, 0,	// 0x39-0x3F Reserved
	0,						// 0x40 = Undefined

	0,//	(MHL_DEV_LD_SPEAKER),	// 0x41 = Volume Up
	0,//	(MHL_DEV_LD_SPEAKER),	// 0x42 = Volume Down
	0,//	(MHL_DEV_LD_SPEAKER),	// 0x43 = Mute
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x44 = Play
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),	// 0x45 = Stop
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),	// 0x46 = Pause
	0,//	(MHL_DEV_LD_RECORD),	// 0x47 = Record
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x48 = Rewind
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x49 = Fast Forward
	0,//	(MHL_DEV_LD_MEDIA),		// 0x4A = Eject
	0,//	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA),	// 0x4B = Forward
	0,//	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_MEDIA),	// 0x4C = Backward
	0, 0, 0,				// 4D-4F Reserved
	0,						// 0x50 = Angle
	0,						// 0x51 = Subpicture
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 52-5F Reserved
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x60 = Play Function
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO),	// 0x61 = Pause the Play Function
	0,//	(MHL_DEV_LD_RECORD),	// 0x62 = Record Function
	0,//	(MHL_DEV_LD_RECORD),	// 0x63 = Pause the Record Function
	(MHL_DEV_LD_VIDEO | MHL_DEV_LD_AUDIO | MHL_DEV_LD_RECORD),	// 0x64 = Stop Function

	0,//	(MHL_DEV_LD_SPEAKER),	// 0x65 = Mute Function
	0,//	(MHL_DEV_LD_SPEAKER),	// 0x66 = Restore Mute Function
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 	// Undefined or reserved
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 		// Undefined or reserved
};
bool vbus_mhl_est_fail = FALSE;//3355
bool mhl_vbus = FALSE;//3355
/*===========================================================================
                   FUNCTION DEFINITIONS
===========================================================================*/
static	void	Int4Isr( void );
static	void	Int1RsenIsr( void );
static	void	MhlCbusIsr( void );
#if 0//not used
static	void 	DeglitchRsenLow( void );
#endif

void	CbusReset( void );
void	SwitchToD0( void );
void	SwitchToD3( void );
void	WriteInitialRegisterValues ( void );
static	void	InitCBusRegs( void );
static	void	ForceUsbIdSwitchOpen ( void );
static	void	ReleaseUsbIdSwitchOpen ( void );
void	MhlTxDrvProcessConnection ( void );
	void	MhlTxDrvProcessDisconnection ( void );
static	void	ApplyDdcAbortSafety( void );
#ifdef CONFIG_USB_SWITCH_FSA9480
extern void FSA9480_CheckAndHookAudioDock(int value, int onoff);//Rajucm
extern void FSA9480_MhlSwitchSel(bool);//Rajucm
extern void FSA9480_MhlTvOff(void);//Rajucm
extern void EnableFSA9480Interrupts(void);	//NAGSM_Android_SEL_Kernel_Aakash_20101214
extern void DisableFSA9480Interrupts(void);	//NAGSM_Android_SEL_Kernel_Aakash_20101214
#endif
extern void mhl_hpd_handler(bool);
//
// Store global config info here. This is shared by the driver.
//
//
//
// structure to hold operating information of MhlTx component
//
static	mhlTx_config_t	mhlTxConfig;
//
// Functions used internally.
//
static	bool 		SiiMhlTxRapkSend( void );
static	void		MhlTxDriveStates( void );
static	void		MhlTxResetStates( void );
static	bool		MhlTxSendMscMsg ( byte command, byte cmdData );
void	SiiMhlTxNotifyConnection( bool mhlConnected );
void	AppVbusControl( bool powerOn );
void	AppRcpDemo( byte event, byte eventParameter);
void	SiiMhlTxNotifyDsHpdChange( byte dsHpdStatus );
bool SiiMhlTxReadDevcap( byte offset );
bool SiiMhlTxRcpkSend( byte );
void	SiiMhlTxGotMhlStatus( byte status_0, byte status_1 );
void	SiiMhlTxGotMhlIntr( byte intr_0, byte intr_1 );
void	SiiMhlTxGotMhlMscMsg( byte subCommand, byte cmdData );
void	SiiMhlTxMscCommandDone( byte data1 );
void SiiMhlTxGetEvents( byte *event, byte *eventParameter );
void SiiMhlTxInitialize( void );
bool SiiMhlTxRcpeSend( byte rcpeErrorCode );


void DelayMS(word msec);
void SiI9234_HW_Reset(void);
Bool SiI9234_startTPI(void);
Bool SiI9234_init(void);
void SiI9234_interrupt_event(void);
void	ProcessRgnd( void );

extern void rcp_cbus_uevent(u8);

/*======================================================================*/



/*===========================================================================
  FUNCTION DelayMS

  DESCRIPTION
 

  DEPENDENCIES
  None

  RETURN VALUE
  None

  SIDE EFFECTS
  None
===========================================================================*/
void DelayMS(word msec)
{
	msleep(msec);
}


/*===========================================================================
  FUNCTION SiI9234_HW_Reset

  DESCRIPTION
  SiI9024A HW reset

  DEPENDENCIES
  None

  RETURN VALUE
  None

  SIDE EFFECTS
  None
===========================================================================*/
void SiI9234_HW_Reset(void)
{
	printk((">>TxHW_Reset()\n"));
	gpio_set_value_cansleep(GPIO_MHL_RST, 0);
	DelayMS(TX_HW_RESET_PERIOD);
	gpio_set_value_cansleep(GPIO_MHL_RST, 1);
}

/*===========================================================================
  FUNCTION SiI9234_startTPI

  DESCRIPTION
  Change the TPI mode (SW->H/W TPI mode)

  DEPENDENCIES
  None

  RETURN VALUE
  None

  SIDE EFFECTS
  None
===========================================================================*/
Bool SiI9234_startTPI(void)
{
	byte devID = 0x00;
	printk(">>StartTPI()\n");
	WriteByteTPI(TPI_ENABLE, 0x00);            // Write "0" to 72:C7 to start HW TPI mode
	DelayMS(100);

	devID = ReadByteTPI(TPI_DEVICE_ID);
	if (devID == SiI_DEVICE_ID || devID == SiI_DEVICE_ID_9234) 
	{
		printk("######## Silicon Device Id: %x\n", devID);
		return TRUE;
	}

	printk("Unsupported TX\n");
	return FALSE;
}


/*===========================================================================
  FUNCTION SiI9234_init

  DESCRIPTION
  SiI9234 initialization function.

  DEPENDENCIES
  None

  RETURN VALUE
  None

  SIDE EFFECTS
  None
===========================================================================*/
Bool SiI9234_init(void)
{
//	Bool status;
	printk("# SiI9234 initialization start~ \n");  
	//SiI9234_HW_Reset();	
	
	SiiMhlTxInitialize();  
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxChipInitialize
//
// Chip specific initialization.
// This function is for SiI 9244 Initialization: HW Reset, Interrupt enable.
//
//
//////////////////////////////////////////////////////////////////////////////

bool SiiMhlTxChipInitialize ( void )
{
	TX_DEBUG_PRINT( ("Drv: SiiMhlTxChipInitialize: 0x%X\n", (int)I2C_ReadByte(SA_TX_Page0_Primary, 0x03)) );

	// setup device registers. Ensure RGND interrupt would happen.
	WriteInitialRegisterValues();
	// Setup interrupt masks for all those we are interested.
	//MASK_INTR_4_INTERRUPTS;
	//MASK_INTR_1_INTERRUPTS;
	UNMASK_INTR_4_INTERRUPTS;
	UNMASK_INTR_1_INTERRUPTS;
	MASK_INTR_4_RGND;	
	SwitchToD3();
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////
// SiiMhlTxDeviceIsr
//
// This function must be called from a master interrupt handler or any polling
// loop in the host software if during initialization call the parameter
// interruptDriven was set to TRUE. SiiMhlTxGetEvents will not look at these
// events assuming firmware is operating in interrupt driven mode. MhlTx component
// performs a check of all its internal status registers to see if a hardware event
// such as connection or disconnection has happened or an RCP message has been
// received from the connected device. Due to the interruptDriven being TRUE,
// MhlTx code will ensure concurrency by asking the host software and hardware to
// disable interrupts and restore when completed. Device interrupts are cleared by
// the MhlTx component before returning back to the caller. Any handling of
// programmable interrupt controller logic if present in the host will have to
// be done by the caller after this function returns back.

// This function has no parameters and returns nothing.
//
// This is the master interrupt handler for 9244. It calls sub handlers
// of interest. Still couple of status would be required to be picked up
// in the monitoring routine (Sii9244TimerIsr)
//
// To react in least amount of time hook up this ISR to processor's
// interrupt mechanism.
//
// Just in case environment does not provide this, set a flag so we
// call this from our monitor (Sii9244TimerIsr) in periodic fashion.
//
// Device Interrupts we would look at
//		RGND		= to wake up from D3
//		MHL_EST 	= connection establishment
//		CBUS_LOCKOUT= Service USB switch
//		RSEN_LOW	= Disconnection deglitcher
//		CBUS 		= responder to peer messages
//					  Especially for DCAP etc time based events
//
void 	SiiMhlTxDeviceIsr( void )
{
	byte tmp;		//NAGSM_Android_SEL_Kernel_Aakash_20101214
	if( TX_POWER_STATE_D0_MHL != fwPowerState )
	{  
		//
		// Check important RGND, MHL_EST, CBUS_LOCKOUT and SCDT interrupts
		// During D3 we only get RGND but same ISR can work for both states
		//
		Int4Isr();
		if(mhl_cable_status == MHL_TV_OFF_CABLE_CONNECT)
		{
			return;
		}
                //NAGSM_Android_SEL_Kernel_Aakash_20101214
		tmp = ReadByteCBUS(0x08);
		WriteByteCBUS(0x08, tmp);
		tmp = ReadByteCBUS(0x1E);
		WriteByteCBUS(0x1E, tmp);  
                //NAGSM_Android_SEL_Kernel_Aakash_20101214
	}
	else if( TX_POWER_STATE_D0_MHL == fwPowerState ) //NAGSM_Android_SEL_Kernel_Aakash_20101214
	{
		//Int1RsenIsr(); //NAGSM_Android_SEL_Kernel_Aakash_20101214
		//
		// Check for any peer messages for DCAP_CHG etc
		// Dispatch to have the CBUS module working only once connected.
		//
		MhlCbusIsr();
	}
	Int1RsenIsr();		//NAGSM_Android_SEL_Kernel_Aakash_20101214

}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDriverTmdsControl
//
// Control the TMDS output. MhlTx uses this to support RAP content on and off.
//
void	SiiMhlTxDrvTmdsControl( bool enable )
{
	if( enable )
	{
		SET_BIT(SA_TX_Page0_Primary, 0x80, 4);
		TX_DEBUG_PRINT(("Drv: TMDS Output Enabled\n"));
		{//3355
			byte		rsen  = I2C_ReadByte(SA_TX_Page0_Primary, 0x09) & BIT_2;
			if(vbus_mhl_est_fail == TRUE)
			{
				if(rsen == 0x00)
				{
					TX_DEBUG_PRINT(("TMDS RSEN \n"));
					CLR_BIT(SA_TX_Page0_Primary, 0x80, 4);
					MhlTxDrvProcessDisconnection();
					return;
				}
				else
				{
					vbus_mhl_est_fail = FALSE;
				}
			}
		}//3355
	}
	else
	{
		CLR_BIT(SA_TX_Page0_Primary, 0x80, 4);
		TX_DEBUG_PRINT(("Drv: TMDS Ouput Disabled\n"));
	}
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxDrvNotifyEdidChange
//
// MhlTx may need to inform upstream device of a EDID change. This can be
// achieved by toggling the HDMI HPD signal or by simply calling EDID read
// function.
//
void	SiiMhlTxDrvNotifyEdidChange ( void )
{
    TX_DEBUG_PRINT(("Drv: SiiMhlTxDrvNotifyEdidChange\n"));
	//
	// Prepare to toggle HPD to upstream
	//
	// set reg_hpd_out_ovr_en to first control the hpd
	SET_BIT(SA_TX_Page0_Primary, 0x79, 4);

	// reg_hpd_out_ovr_val = LOW to force the HPD low
	CLR_BIT(SA_TX_Page0_Primary, 0x79, 5);

	// wait a bit
	DelayMS(110);

	// release HPD back to high by reg_hpd_out_ovr_val = HIGH
	SET_BIT(SA_TX_Page0_Primary, 0x79, 5);

}
//------------------------------------------------------------------------------
// Function:    SiiMhlTxDrvSendCbusCommand
//
// Write the specified Sideband Channel command to the CBUS.
// Command can be a MSC_MSG command (RCP/RAP/RCPK/RCPE/RAPK), or another command 
// such as READ_DEVCAP, SET_INT, WRITE_STAT, etc.
//
// Parameters:  
//              pReq    - Pointer to a cbus_req_t structure containing the 
//                        command to write
// Returns:     TRUE    - successful write
//              FALSE   - write failed
//------------------------------------------------------------------------------

bool SiiMhlTxDrvSendCbusCommand ( cbus_req_t *pReq  )
{
    bool  success = TRUE;

    byte i, startbit;

	//
	// If not connected, return with error
	//
	//if( (TX_POWER_STATE_D0_MHL != fwPowerState ) || (0 == ReadByteCBUS(0x0a) || mscCmdInProgress))
  if( (TX_POWER_STATE_D0_MHL != fwPowerState ) || (mscCmdInProgress))
	{
	    TX_DEBUG_PRINT(("Error: Drv: fwPowerState: %02X, CBUS[0x0A]: %02X or mscCmdInProgress = %d\n",
				(int) fwPowerState,
				(int) ReadByteCBUS(0x0a),
				(int) mscCmdInProgress));

   		return FALSE;
	}
	// Now we are getting busy
	mscCmdInProgress	= TRUE;

    TX_DEBUG_PRINT(("Drv: Sending MSC command %02X, %02X, %02X, %02X\n", 
			(int)pReq->command, 
			(int)(pReq->offsetData),
		 	(int)pReq->msgData[0],
		 	(int)pReq->msgData[1]));

    /****************************************************************************************/
    /* Setup for the command - write appropriate registers and determine the correct        */
    /*                         start bit.                                                   */
    /****************************************************************************************/

	// Set the offset and outgoing data byte right away
	WriteByteCBUS( 0x13, pReq->offsetData); 	// set offset
	WriteByteCBUS( 0x14, pReq->msgData[0] );
	
    startbit = 0x00;
    switch ( pReq->command )
    {
		case MHL_SET_INT:	// Set one interrupt register = 0x60
			WriteByteCBUS( (REG_CBUS_PRI_ADDR_CMD & 0xFF), pReq->offsetData + 0x20 ); 	// set offset
			startbit = MSC_START_BIT_WRITE_REG;
			break;

        case MHL_WRITE_STAT:	// Write one status register = 0x60 | 0x80
            WriteByteCBUS( (REG_CBUS_PRI_ADDR_CMD & 0xFF), pReq->offsetData + 0x30 ); 	// set offset
            startbit = MSC_START_BIT_WRITE_REG;
            break;

        case MHL_READ_DEVCAP:	// Read one device capability register = 0x61
            startbit = MSC_START_BIT_READ_REG;
            break;

 		case MHL_GET_STATE:			// 0x62 -
		case MHL_GET_VENDOR_ID:		// 0x63 - for vendor id	
		case MHL_SET_HPD:			// 0x64	- Set Hot Plug Detect in follower
		case MHL_CLR_HPD:			// 0x65	- Clear Hot Plug Detect in follower
		case MHL_GET_SC1_ERRORCODE:		// 0x69	- Get channel 1 command error code
		//case MHL_GET_DDC_ERRORCODE:		// 0x6A	- Get DDC channel command error code.
		case MHL_GET_MSC_ERRORCODE:		// 0x6B	- Get MSC command error code.
		case MHL_GET_SC3_ERRORCODE:		// 0x6D	- Get channel 3 command error code.
			WriteByteCBUS( (REG_CBUS_PRI_ADDR_CMD & 0xFF), pReq->command );
            startbit = MSC_START_BIT_MSC_CMD;
            break;
    	case MHL_GET_DDC_ERRORCODE:		// 0x6A	- Get DDC channel command error code.
			WriteByteCBUS( (REG_CBUS_PRI_ADDR_CMD & 0xFF), 0x00 );
            startbit = MSC_START_BIT_MSC_CMD;
            break;
        case MHL_MSC_MSG:
			WriteByteCBUS( (REG_CBUS_PRI_WR_DATA_2ND & 0xFF), pReq->msgData[1] );
			WriteByteCBUS( (REG_CBUS_PRI_ADDR_CMD & 0xFF), pReq->command );
            startbit = MSC_START_BIT_VS_CMD;
            break;

        case MHL_WRITE_BURST:
            WriteByteCBUS( (REG_CBUS_PRI_ADDR_CMD & 0xFF), pReq->offsetData + 0x40 );
            WriteByteCBUS( (REG_MSC_WRITE_BURST_LEN & 0xFF), pReq->length -1 );

            // Now copy all bytes from array to local scratchpad
            for ( i = 0; i < pReq->length; i++ )
            {
                WriteByteCBUS( (REG_CBUS_SCRATCHPAD_0 & 0xFF) + i, pReq->msgData[i] );
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

    if ( success )
    {
        WriteByteCBUS( REG_CBUS_PRI_START & 0xFF, startbit );
    }

    return( success );
}
////////////////////////////////////////////////////////////////////
//
// L O C A L    F U N C T I O N S
//
////////////////////////////////////////////////////////////////////
// Int1RsenIsr
//
// This interrupt is used only to decide if the MHL is disconnected
// The disconnection is determined by looking at RSEN LOW and applying
// all MHL compliant disconnect timings and deglitch logic.
//
//	Look for interrupts on INTR_1 (Register 0x71)
//		7 = RSVD		(reserved)
//		6 = MDI_HPD		(interested)
//		5 = RSEN CHANGED(interested)	
//		4 = RSVD		(reserved)
//		3 = RSVD		(reserved)
//		2 = RSVD		(reserved)
//		1 = RSVD		(reserved)
//		0 = RSVD		(reserved)
////////////////////////////////////////////////////////////////////

void	Int1RsenIsr( void )
{
	byte		reg71 = I2C_ReadByte(SA_TX_Page0_Primary, 0x71);
	byte		rsen  = I2C_ReadByte(SA_TX_Page0_Primary, 0x09) & BIT_2;

	// Look at RSEN interrupt
	if(reg71 & BIT_5)
	{
		TX_DEBUG_PRINT (("Drv: Got2 INTR_1: reg71 = %02X, rsen = %02X\n", (int) reg71, (int) rsen));
		//pinCBusToggle = 1;	// for debug measurements. RSEN intr
		//
		// RSEN becomes LOW in SYS_STAT register 0x72:0x09[2]
		// SYS_STAT	==> bit 7 = VLOW, 6:4 = MSEL, 3 = TSEL, 2 = RSEN, 1 = HPD, 0 = TCLK STABLE
		//
		// Start countdown timer for deglitch
		// Allow RSEN to stay low this much before reacting
		//
		if(rsen == 0x00)
		{
			if(TX_POWER_STATE_D0_MHL != fwPowerState)
			{
				TX_DEBUG_PRINT (("Drv: Got1 INTR_1: reg71 = %02X, rsen = %02X\n", (int) reg71, (int) rsen));
				I2C_WriteByte(SA_TX_Page0_Primary, 0x71, reg71);
				return;
			}

			if(mhl_cable_status & MHL_1K_IMPEDANCE_VERIFIED)
			{
				TX_DEBUG_PRINT((KERN_ERR "RSEN Low and 1K impedance\n"));
				DelayMS(100);
				if((I2C_ReadByte(SA_TX_Page0_Primary, 0x09) & BIT_2) == 0x00) 
				{
					TX_DEBUG_PRINT((KERN_ERR "Really RSEN Low\n"));
					//mhl_cable_status =MHL_INIT_POWER_OFF;
					mhl_cable_status = MHL_TV_OFF_CABLE_CONNECT;
#ifdef CONFIG_USB_SWITCH_FSA9480
					FSA9480_MhlSwitchSel(0);  
#endif
				}
				else
				{
					TX_DEBUG_PRINT((KERN_ERR "RSEN Stable\n"));
				}
			}
			else
			{
				printk(KERN_ERR "%s: MHL Cable disconnect### 2\n", __func__);
				mhl_cable_status =MHL_INIT_POWER_OFF;
#ifdef CONFIG_USB_SWITCH_FSA9480
				FSA9480_MhlSwitchSel(0); 
#endif
				printk(KERN_ERR "MHL_SEL to 0\n");
			}
			mhl_hpd_handler(false); // TO DO :- Rajesh Fix for USB crash issue.
			return;
			//NAGSM_Android_SEL_Kernel_Aakash_20101214
		}
		else if(rsen == 0x04)
		{
			mhl_cable_status |= MHL_RSEN_VERIFIED;
			printk("MHL Cable Connection ###\n");
		}
		// Clear MDI_RSEN interrupt
	}

	if(reg71 & BIT_6)	
	{
		byte cbusInt;
		//HPD
		printk("HPD \n");
		//
		// Check if a SET_HPD came from the downstream device.
		//
		cbusInt = ReadByteCBUS(0x0D);

		// CBUS_HPD status bit
		if((BIT_6 & cbusInt) != dsHpdStatus)	
		{
			// Inform upper layer of change in Downstream HPD
			SiiMhlTxNotifyDsHpdChange( cbusInt );
			TX_DEBUG_PRINT(("Drv: Downstream HPD changed to: %02X\n", (int) cbusInt));
			// Remember
			dsHpdStatus = (BIT_6 & cbusInt);
			if (!dsHpdStatus) //Rajucm: Check HPD Status and desable Cbus interrupts
			{	
				UNMASK_CBUS1_INTERRUPTS;
				UNMASK_CBUS2_INTERRUPTS;
				mhl_cable_status = MHL_INIT_POWER_OFF;//Rajucm: fix for RCP HDMI cable disconnect and connect
				//#ifdef CONFIG_SAMSUNG_MHL_HPD_SOLUTION //Rajucm
				mhl_hpd_handler(false);
				dsHpdStatus=0;
				//#endif
			}
			else	
			{
				MASK_CBUS1_INTERRUPTS;
				MASK_CBUS2_INTERRUPTS;
				//#ifdef CONFIG_SAMSUNG_MHL_HPD_SOLUTION //Rajucm
				mhl_hpd_handler(true);
				dsHpdStatus=1;
				//#endif
			}

			
		}

	}

  I2C_WriteByte(SA_TX_Page0_Primary, 0x71, reg71);
  
}

#if 0//not used
//////////////////////////////////////////////////////////////////////////////
//
// DeglitchRsenLow
//
// This function looks at the RSEN signal if it is low.
//
// The disconnection will be performed only if we were in fully MHL connected
// state for more than 400ms AND a 150ms deglitch from last interrupt on RSEN
// has expired.
//
// If MHL connection was never established but RSEN was low, we unconditionally
// and instantly process disconnection.
//
static void DeglitchRsenLow( void )
{
	TX_DEBUG_PRINT(("Drv: DeglitchRsenLow RSEN <72:09[2]> = %02X\n", (int) (I2C_ReadByte(SA_TX_Page0_Primary, 0x09)) ));

	if((I2C_ReadByte(SA_TX_Page0_Primary, 0x09) & BIT_2) == 0x00)
	{
		TX_DEBUG_PRINT(("Drv: RSEN is Low.\n"));
		//
		// If no MHL cable is connected or RSEN deglitch timer has started,
		// we may not receive interrupts for RSEN.
		// Monitor the status of RSEN here.
		//
		// 
		// First check means we have not received any interrupts and just started
		// but RSEN is low. Case of "nothing" connected on MHL receptacle
		//
		if(TX_POWER_STATE_D0_MHL == fwPowerState)
		//if((TX_POWER_STATE_D0_MHL == fwPowerState)    && HalTimerExpired(TIMER_TO_DO_RSEN_DEGLITCH) )      
		{
			// Second condition means we were fully operational, then a RSEN LOW interrupt
			// occured and a DEGLITCH_TIMER per MHL specs started and completed.
			// We can disconnect now.
			//
			TX_DEBUG_PRINT(("Drv: Disconnection due to RSEN Low\n"));

			deglitchingRsenNow = FALSE;

			//pinCBusToggle = 0;	// for debug measurements - disconnected due to RSEN

			// FP1226: Toggle MHL discovery to level the voltage to deterministic vale.
			DISABLE_DISCOVERY;
			ENABLE_DISCOVERY;
			//
			// We got here coz cable was never connected
			//
			MhlTxDrvProcessDisconnection();
		}
	}
	else
	{
		//
		// Deglitch here:
		// RSEN is not low anymore. Reset the flag.
		// This flag will be now set on next interrupt.
		//
		// Stay connected
		//
		deglitchingRsenNow = FALSE;
	}
}
#endif

///////////////////////////////////////////////////////////////////////////
// WriteInitialRegisterValues
//
//
///////////////////////////////////////////////////////////////////////////
 void WriteInitialRegisterValues ( void )
{
	TX_DEBUG_PRINT(("Drv: WriteInitialRegisterValues\n"));
	// Power Up
	I2C_WriteByte(SA_TX_Page1_Primary, 0x3D, 0x3F);	// Power up CVCC 1.2V core
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x11, 0x01);	// Enable TxPLL Clock
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x12, 0x15);	// Enable Tx Clock Path & Equalizer
	I2C_WriteByte(SA_TX_Page0_Primary, 0x08, 0x35);	// Power Up TMDS Tx Core

	// Analog PLL Control
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x10, 0xC1);	// bits 5:4 = 2b00 as per characterization team.
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x17, 0x03);	// PLL Calrefsel
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x1A, 0x20);	// VCO Cal
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x22, 0x8A);	// Auto EQ
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x23, 0x6A);	// Auto EQ
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x24, 0xAA);	// Auto EQ
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x25, 0xCA);	// Auto EQ
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x26, 0xEA);	// Auto EQ
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x4C, 0xA0);	// Manual zone control
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x4D, 0x00);	// PLL Mode Value

	I2C_WriteByte(SA_TX_Page0_Primary, 0x80, 0x34);	// Enable Rx PLL Clock Value
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x45, 0x44);	// Rx PLL BW value from I2C
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x31, 0x0A);	// Rx PLL BW ~ 4MHz
	I2C_WriteByte(SA_TX_Page0_Primary, 0xA0, 0xD0);
	I2C_WriteByte(SA_TX_Page0_Primary, 0xA1, 0xFC);	// Disable internal MHL driver
	I2C_WriteByte(SA_TX_Page0_Primary, 0xA3, 0xEA);
	I2C_WriteByte(SA_TX_Page0_Primary, 0xA6, 0x0C);
	

	I2C_WriteByte(SA_TX_Page0_Primary, 0x2B, 0x01);	// Enable HDCP Compliance safety

	//
	// CBUS & Discovery
	// CBUS discovery cycle time for each drive and float = 150us
	//
	ReadModifyWriteTPI(0x90, BIT_3 | BIT_2, BIT_3);

	I2C_WriteByte(SA_TX_Page0_Primary, 0x91, 0xA5);		// Clear bit 6 (reg_skip_rgnd)


	// Changed from 66 to 65 for 94[1:0] = 01 = 5k reg_cbusmhl_pup_sel
	//I2C_WriteByte(SA_TX_Page0_Primary, 0x94, 0x65);			// 1.8V CBUS VTH & GND threshold
    I2C_WriteByte(SA_TX_Page0_Primary, 0x94, 0x75);			// 1.8V CBUS VTH & GND threshold

	//set bit 2 and 3, which is Initiator Timeout
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x31, I2C_ReadByte(SA_TX_CBUS_Primary, 0x31) | 0x0c);

	//I2C_WriteByte(SA_TX_Page0_Primary, 0xA5, 0xAC);			// RGND Hysterisis.
    I2C_WriteByte(SA_TX_Page0_Primary, 0xA5, 0xA0);			
	TX_DEBUG_PRINT(("Drv: MHL 1.0 Compliant Clock\n"));
	
	// RGND & single discovery attempt (RGND blocking)
	I2C_WriteByte(SA_TX_Page0_Primary, 0x95, 0x31);

	// use 1K and 2K setting
	//I2C_WriteByte(SA_TX_Page0_Primary, 0x96, 0x22);

	// Use VBUS path of discovery state machine
	I2C_WriteByte(SA_TX_Page0_Primary, 0x97, 0x00);
	ReadModifyWriteTPI(0x95, BIT_6, BIT_6);		// Force USB ID switch to open

	//
	// For MHL compliance we need the following settings for register 93 and 94
	// Bug 20686
	//
	// To allow RGND engine to operate correctly.
	//
	// When moving the chip from D2 to D0 (power up, init regs) the values should be
	// 94[1:0] = 01  reg_cbusmhl_pup_sel[1:0] should be set for 5k
	// 93[7:6] = 10  reg_cbusdisc_pup_sel[1:0] should be set for 10k (default)
	// 93[5:4] = 00  reg_cbusidle_pup_sel[1:0] = open (default)
	//

	WriteByteTPI(0x92, 0x86);				//
	// change from CC to 8C to match 5K
	WriteByteTPI(0x93, 0x8C);				// Disable CBUS pull-up during RGND measurement
	//NAGSM_Android_SEL_Kernel_Aakash_20101214
 	//ReadModifyWriteTPI(0x79, BIT_5 | BIT_4, BIT_4);	// Force upstream HPD to 0 when not in MHL mode.
	ReadModifyWriteTPI(0x79, BIT_1 | BIT_2, 0); //Set interrupt active high

	DelayMS(25);
	ReadModifyWriteTPI(0x95, BIT_6, 0x00);		// Release USB ID switch

	I2C_WriteByte(SA_TX_Page0_Primary, 0x90, 0x27);			// Enable CBUS discovery

	CbusReset();

	InitCBusRegs();

	// Enable Auto soft reset on SCDT = 0
	I2C_WriteByte(SA_TX_Page0_Primary, 0x05, 0x04);

	// HDMI Transcode mode enable
	I2C_WriteByte(SA_TX_Page0_Primary, 0x0D, 0x1C);

 	//I2C_WriteByte(SA_TX_Page0_Primary, 0x78, RGND_RDY_EN);
}
///////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////
#define MHL_DEVICE_CATEGORY             0x02 //(MHL_DEV_CAT_SOURCE)
#define	MHL_LOGICAL_DEVICE_MAP			(MHL_DEV_LD_AUDIO | MHL_DEV_LD_VIDEO | MHL_DEV_LD_MEDIA | MHL_DEV_LD_GUI )

static void InitCBusRegs( void )
{
	byte		regval;

	TX_DEBUG_PRINT(("Drv: InitCBusRegs\n"));
	// Increase DDC translation layer timer
	//I2C_WriteByte(SA_TX_CBUS_Primary, 0x07, 0x36);
       I2C_WriteByte(SA_TX_CBUS_Primary, 0x07, 0x32); //for default DDC byte mode
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x40, 0x03); 			// CBUS Drive Strength
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x42, 0x06); 			// CBUS DDC interface ignore segment pointer
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x36, 0x0C);

	I2C_WriteByte(SA_TX_CBUS_Primary, 0x3D, 0xFD);	
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x1C, 0x00);

	I2C_WriteByte(SA_TX_CBUS_Primary, 0x44, 0x02);

	// Setup our devcap
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x80, MHL_DEV_ACTIVE);
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x81, MHL_VERSION);
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x82, MHL_DEVICE_CATEGORY);
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x83, 0);  						
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x84, 0);						
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x85, (MHL_DEV_VID_LINK_SUPPRGB444));
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x86, MHL_DEV_AUD_LINK_2CH);
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x87, 0);										// not for source
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x88, MHL_LOGICAL_DEVICE_MAP);
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x89, 0x0F);										// not for source
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x8A, MHL_FEATURE_RCP_SUPPORT | MHL_FEATURE_RAP_SUPPORT|MHL_FEATURE_SP_SUPPORT);
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x8B, 0);
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x8C, 0);										// reserved
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x8D, MHL_SCRATCHPAD_SIZE);
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x8E, 0x44 ); //MHL_INT_AND_STATUS_SIZE);
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x8F, 0);										//reserved

	// Make bits 2,3 (initiator timeout) to 1,1 for register CBUS_LINK_CONTROL_2
	regval = I2C_ReadByte(SA_TX_CBUS_Primary, (byte)REG_CBUS_LINK_CONTROL_2 );
	regval = (regval | 0x0C);
	I2C_WriteByte(SA_TX_CBUS_Primary, (byte)REG_CBUS_LINK_CONTROL_2, regval);

	 // Clear legacy bit on Wolverine TX.
    regval = I2C_ReadByte(SA_TX_CBUS_Primary, (byte)REG_MSC_TIMEOUT_LIMIT);
    I2C_WriteByte(SA_TX_CBUS_Primary, (byte)REG_MSC_TIMEOUT_LIMIT, (regval & MSC_TIMEOUT_LIMIT_MSB_MASK));

	// Set NMax to 1
	I2C_WriteByte(SA_TX_CBUS_Primary, (byte)REG_CBUS_LINK_CONTROL_1, 0x01);

}

///////////////////////////////////////////////////////////////////////////
//
// ForceUsbIdSwitchOpen
//
///////////////////////////////////////////////////////////////////////////
static void ForceUsbIdSwitchOpen ( void )
{
	I2C_WriteByte(SA_TX_Page0_Primary, 0x90, 0x26);		// Disable CBUS discovery
	ReadModifyWriteTPI(0x95, BIT_6, BIT_6);	// Force USB ID switch to open
	WriteByteTPI(0x92, 0x86);
	// Force HPD to 0 when not in MHL mode.
	ReadModifyWriteTPI(0x79, BIT_5 | BIT_4, BIT_4);

}
///////////////////////////////////////////////////////////////////////////
//
// ReleaseUsbIdSwitchOpen
//
///////////////////////////////////////////////////////////////////////////
static void ReleaseUsbIdSwitchOpen ( void )
{
	DelayMS(50);
	// Release USB ID switch
	ReadModifyWriteTPI(0x95, BIT_6, 0x00);
	ENABLE_DISCOVERY;
}

/////////////////////////////////////////////////////////////////////////////
//
// FUNCTION     :   CbusWakeUpPulseGenerator ()
//
// PURPOSE      :   Generate Cbus Wake up pulse sequence using GPIO or I2C method.
//
// INPUT PARAMS :   None
//
// OUTPUT PARAMS:   None
//
// GLOBALS USED :   None
//
// RETURNS      :   None
//
//////////////////////////////////////////////////////////////////////////////

void CbusWakeUpPulseGenerator( void )
{	
	TX_DEBUG_PRINT(("Drv: CbusWakeUpPulseGenerator\n"));

	if (!hrtimer_initialized) {
		hrtimer_init(&hr_wake_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		hr_wake_timer.function = &hrtimer_wakeup_callback;
		hrtimer_initialized = true;
		first_timer = true;
	}
		
	//
	// I2C method
	//
	//I2C_WriteByte(SA_TX_Page0_Primary, 0x92, (I2C_ReadByte(SA_TX_Page0_Primary, 0x92) | 0x10));

	// Start the pulse
	I2C_WriteByte(SA_TX_Page0_Primary, 0x96, (I2C_ReadByte(SA_TX_Page0_Primary, 0x96) | 0xC0));
//	mdelay(T_SRC_WAKE_PULSE_WIDTH_1-1);	// adjust for code path
	start_hrtimer_ms(19);
	wait_event_interruptible(wake_wq, wakeup_time_expired);

	I2C_WriteByte(SA_TX_Page0_Primary, 0x96, (I2C_ReadByte(SA_TX_Page0_Primary, 0x96) & 0x3F));
//	mdelay(T_SRC_WAKE_PULSE_WIDTH_1);	// adjust for code path
	start_hrtimer_ms(19);
	wait_event_interruptible(wake_wq, wakeup_time_expired);

	I2C_WriteByte(SA_TX_Page0_Primary, 0x96, (I2C_ReadByte(SA_TX_Page0_Primary, 0x96) | 0xC0));
//	mdelay(T_SRC_WAKE_PULSE_WIDTH_1-1);	// adjust for code path
	start_hrtimer_ms(19);
	wait_event_interruptible(wake_wq, wakeup_time_expired);

	I2C_WriteByte(SA_TX_Page0_Primary, 0x96, (I2C_ReadByte(SA_TX_Page0_Primary, 0x96) & 0x3F));
//	mdelay(T_SRC_WAKE_PULSE_WIDTH_2-1);	// adjust for code path
	start_hrtimer_ms(60);
	wait_event_interruptible(wake_wq, wakeup_time_expired);

	I2C_WriteByte(SA_TX_Page0_Primary, 0x96, (I2C_ReadByte(SA_TX_Page0_Primary, 0x96) | 0xC0));
//	mdelay(T_SRC_WAKE_PULSE_WIDTH_1);	// adjust for code path
	start_hrtimer_ms(19);
	wait_event_interruptible(wake_wq, wakeup_time_expired);

	I2C_WriteByte(SA_TX_Page0_Primary, 0x96, (I2C_ReadByte(SA_TX_Page0_Primary, 0x96) & 0x3F));
//	mdelay(T_SRC_WAKE_PULSE_WIDTH_1);	// adjust for code path
	start_hrtimer_ms(19);
	wait_event_interruptible(wake_wq, wakeup_time_expired);

	I2C_WriteByte(SA_TX_Page0_Primary, 0x96, (I2C_ReadByte(SA_TX_Page0_Primary, 0x96) | 0xC0));
//	mdelay(20);
	start_hrtimer_ms(19);
	wait_event_interruptible(wake_wq, wakeup_time_expired);

	I2C_WriteByte(SA_TX_Page0_Primary, 0x96, (I2C_ReadByte(SA_TX_Page0_Primary, 0x96) & 0x3F));
//	mdelay(T_SRC_WAKE_TO_DISCOVER);
	start_hrtimer_ms(T_SRC_WAKE_TO_DISCOVER);
	wait_event_interruptible(wake_wq, wakeup_time_expired);
}

///////////////////////////////////////////////////////////////////////////
//
// ApplyDdcAbortSafety
//
///////////////////////////////////////////////////////////////////////////
static	void	ApplyDdcAbortSafety( void )
{
	byte		bTemp, bPost;

	/*TX_DEBUG_PRINT(("[%d] Drv: Do we need DDC Abort Safety\n",
	(int) (HalTimerElapsed( ELAPSED_TIMER ) * MONITORING_PERIOD)));*/

	WriteByteCBUS(0x29, 0xFF);
	bTemp = ReadByteCBUS(0x29);
	DelayMS(3);
	bPost = ReadByteCBUS(0x29);

	if ((bPost > (bTemp + 50)))
	{
		TX_DEBUG_PRINT(("Drv: Applying DDC Abort Safety(SWWA 18958)\n"));

		SET_BIT(SA_TX_Page0_Primary, 0x05, 3);
		CLR_BIT(SA_TX_Page0_Primary, 0x05, 3);

		InitCBusRegs();

		// Why do we do these?
		ForceUsbIdSwitchOpen();
		ReleaseUsbIdSwitchOpen();

		MhlTxDrvProcessDisconnection();
	}
}
///////////////////////////////////////////////////////////////////////////
// ProcessRgnd
//
// H/W has detected impedance change and interrupted.
// We look for appropriate impedance range to call it MHL and enable the
// hardware MHL discovery logic. If not, disable MHL discovery to allow
// USB to work appropriately.
//
// In current chip a firmware driven slow wake up pulses are sent to the
// sink to wake that and setup ourselves for full D0 operation.
///////////////////////////////////////////////////////////////////////////
void	ProcessRgnd( void )
{
	byte		reg99RGNDRange;
	//
	// Impedance detection has completed - process interrupt
	//
	reg99RGNDRange = I2C_ReadByte(SA_TX_Page0_Primary, 0x99) & 0x03;
	TX_DEBUG_PRINT(("Drv: RGND Reg 99 = %02X : \n", (int)reg99RGNDRange));

	/* Keeping DisableFSAinterrupt affects fast connect/disconnect */
	/* But disabling fsa interrupts ... and 
	removing the power adapter cable from the mhl active while cable is connected
	gives multiple FSA interrupts .. Need to find a proper solution. */
	
//	DisableFSA9480Interrupts(); //Test //SEL_Subhransu_20110219		add this code to fsa9485
	//
	// Reg 0x99
	// 00 or 11 means USB.
	// 10 means 1K impedance (MHL)
	// 01 means 2K impedance (MHL)
	//
	if (reg99RGNDRange == 0x00 || reg99RGNDRange == 0x03)	
	{
		TX_DEBUG_PRINT(("%02X : USB impedance. Disable MHL discovery.\n", (int)reg99RGNDRange));
		//3355
		if(vbus_mhl_est_fail == TRUE)	
		{
			MhlTxDrvProcessDisconnection();
			return;
		}
		//3355
		CLR_BIT(SA_TX_Page0_Primary, 0x95, 5);
		mhl_cable_status =MHL_INIT_POWER_OFF;
		TX_DEBUG_PRINT((KERN_ERR "MHL_SEL to 0\n")); 
#ifdef CONFIG_USB_SWITCH_FSA9480
		FSA9480_CheckAndHookAudioDock(0,1); //Rajucm: Audio Dock Detection Algorithm	R1
#endif
	}
	else 	
	{
		mhl_cable_status |= MHL_1K_IMPEDANCE_VERIFIED;
		if(0x01==reg99RGNDRange)
		{
			TX_DEBUG_PRINT(("MHL 2K\n"));
			mhl_cable_status =MHL_TV_OFF_CABLE_CONNECT;
			printk(KERN_ERR "MHL Connection Fail Power off ###1\n");
#ifdef CONFIG_USB_SWITCH_FSA9480
			FSA9480_MhlTvOff();//Rajucm: Mhl cable handling when TV Off 	
#endif
			return ;
		}
		else if(0x02==reg99RGNDRange)
		{
#ifdef CONFIG_USB_SWITCH_FSA9480
			FSA9480_CheckAndHookAudioDock(1,1);  //Rajucm: Audio Dock Detection Algorithm	R1
#endif
			TX_DEBUG_PRINT(("MHL 1K\n"));
			DelayMS(T_SRC_VBUS_CBUS_TO_STABLE);
			// Discovery enabled
			I2C_WriteByte(SA_TX_Page0_Primary, 0x90, 0x25);
			//
			// Send slow wake up pulse using GPIO or I2C
			//
			CbusWakeUpPulseGenerator();
			DelayMS(T_SRC_WAKE_PULSE_WIDTH_2);
#ifdef CONFIG_USB_SWITCH_FSA9480
			EnableFSA9480Interrupts(); 
#endif
		}
		
	}
}
////////////////////////////////////////////////////////////////////
// SwitchToD0
// This function performs s/w as well as h/w state transitions.
//
// Chip comes up in D2. Firmware must first bring it to full operation
// mode in D0.
////////////////////////////////////////////////////////////////////
void	SwitchToD0( void )
{
	//
	// WriteInitialRegisterValues switches the chip to full power mode.
	//
	WriteInitialRegisterValues();
	// Setup interrupt masks for all those we are interested.
	MASK_INTR_4_INTERRUPTS;
	MASK_INTR_1_INTERRUPTS;
	// Force Power State to ON
	I2C_WriteByte(SA_TX_Page0_Primary, 0x90, 0x25);

	fwPowerState = TX_POWER_STATE_D0_NO_MHL;
  	mhl_cable_status =MHL_POWER_ON;
}
////////////////////////////////////////////////////////////////////
// SwitchToD3
//
// This function performs s/w as well as h/w state transitions.
//
////////////////////////////////////////////////////////////////////
void	SwitchToD3( void )
{
	//
	// To allow RGND engine to operate correctly.
	// So when moving the chip from D0 MHL connected to D3 the values should be
	// 94[1:0] = 00  reg_cbusmhl_pup_sel[1:0] should be set for open
	// 93[7:6] = 00  reg_cbusdisc_pup_sel[1:0] should be set for open
	// 93[5:4] = 00  reg_cbusidle_pup_sel[1:0] = open (default)
	//
	// Disable CBUS pull-up during RGND measurement
	//I2C_WriteByte(SA_TX_Page0_Primary, 0x93, 0x04);
    ReadModifyWriteTPI(0x93, BIT_7 | BIT_6 | BIT_5 | BIT_4, 0);
    ReadModifyWriteTPI(0x93, BIT_7 | BIT_6 | BIT_5 | BIT_4, 0);//3355

		ReadModifyWriteTPI(0x94, BIT_1 | BIT_0, 0);

	// 1.8V CBUS VTH & GND threshold
	//I2C_WriteByte(SA_TX_Page0_Primary, 0x94, 0x64);

   	ReleaseUsbIdSwitchOpen();
  	printk("TX_POWER_STATE_D3\n");


	// Change TMDS termination to high impedance on disconnection
	// Bits 1:0 set to 11
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x01, 0x03);
	//
	// Change state to D3 by clearing bit 0 of 3D (SW_TPI, Page 1) register
	// ReadModifyWriteIndexedRegister(INDEXED_PAGE_1, 0x3D, BIT_0, 0x00);
	//
	CLR_BIT(SA_TX_Page1_Primary, 0x3D, 0);

	fwPowerState = TX_POWER_STATE_D3;

}

#if 0//not used
/*===========================================================================
  FUNCTION For_check_resen_int

  DESCRIPTION
  For_check_resen_int

  DEPENDENCIES
  None

  RETURN VALUE
  None

  SIDE EFFECTS
  None
===========================================================================*/
static void For_check_resen_int (void) 
{
	// Power Up
	I2C_WriteByte(SA_TX_Page1_Primary, 0x3D, 0x3F);			// Power up CVCC 1.2V core
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x11, 0x01);			// Enable TxPLL Clock
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x12, 0x15);			// Enable Tx Clock Path & Equalizer
	I2C_WriteByte(SA_TX_Page0_Primary, 0x08, 0x35);			// Power Up TMDS Tx Core

	// Analog PLL Control
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x17, 0x03);			// PLL Calrefsel
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x1A, 0x20);			// VCO Cal
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x22, 0x8A);			// Auto EQ
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x23, 0x6A);			// Auto EQ
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x24, 0xAA);			// Auto EQ
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x25, 0xCA);			// Auto EQ
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x26, 0xEA);			// Auto EQ
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x4C, 0xA0);			// Manual zone control
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x4D, 0x00);			// PLL Mode Value

	//I2C_WriteByte(SA_TX_Page0_Primary, 0x80, 0x34);			// Enable Rx PLL Clock Value
	I2C_WriteByte(SA_TX_Page0_Primary, 0x80, 0x24);			// Enable Rx PLL Clock Value	

	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x45, 0x44);			// Rx PLL BW value from I2C
	I2C_WriteByte(SA_TX_HDMI_RX_Primary, 0x31, 0x0A);			// Rx PLL BW ~ 4MHz
	I2C_WriteByte(SA_TX_Page0_Primary, 0xA0, 0xD0);
	I2C_WriteByte(SA_TX_Page0_Primary, 0xA1, 0xFC);			// Disable internal Mobile HD driver

	I2C_WriteByte(SA_TX_Page0_Primary, 0xA3, 0xEA);
	I2C_WriteByte(SA_TX_Page0_Primary, 0xA6, 0x0C);

	I2C_WriteByte(SA_TX_Page0_Primary, 0x2B, 0x01);			// Enable HDCP Compliance workaround

	// CBUS & Discovery
	ReadModifyWriteTPI(0x90, BIT_3 | BIT_2, BIT_3);	// CBUS discovery cycle time for each drive and float = 150us

	I2C_WriteByte(SA_TX_Page0_Primary, 0x91, 0xA5);

	I2C_WriteByte(SA_TX_Page0_Primary, 0x94, 0x66);			// 1.8V CBUS VTH & GND threshold

	//set bit 2 and 3, which is Initiator Timeout
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x31, I2C_ReadByte(SA_TX_CBUS_Primary, 0x31) | 0x0c);

	I2C_WriteByte(SA_TX_Page0_Primary, 0xA5, 0xAC);			// RGND Hysterisis.

	I2C_WriteByte(SA_TX_Page0_Primary, 0x95, 0x31);			// RGND & single discovery attempt (RGND blocking)

	I2C_WriteByte(SA_TX_Page0_Primary, 0x96, 0x22);			// use 1K and 2K setting
	//	I2C_WriteByte(SA_TX_Page0_Primary, 0x97, 0x03);			// Auto Heartbeat failure enable

	ReadModifyWriteTPI(0x95, BIT_6, BIT_6);		// Force USB ID switch to open

	WriteByteTPI(0x92, 0x86);				//
	WriteByteTPI(0x93, 0xCC);				// Disable CBUS pull-up during RGND measurement

	DelayMS(25);
	ReadModifyWriteTPI(0x95, BIT_6, 0x00);		// Release USB ID switch

	ReadModifyWriteTPI(0x79, BIT_1 | BIT_2, 0); //Set interrupt active high

	I2C_WriteByte(SA_TX_Page0_Primary, 0x90, 0x27);			// Enable CBUS discovery

	// Reset CBus to clear HPD
	I2C_WriteByte(SA_TX_Page0_Primary, 0x05, 0x08);
	DelayMS(2);
	I2C_WriteByte(SA_TX_Page0_Primary, 0x05, 0x00);

	I2C_WriteByte(SA_TX_CBUS_Primary, 0x1F, 0x02); 			// Heartbeat Max Fail Enable
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x07, DDC_XLTN_TIMEOUT_MAX_VAL | 0x06); 			// Increase DDC translation layer timer
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x40, 0x03); 			// CBUS Drive Strength
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x42, 0x06); 			// CBUS DDC interface ignore segment pointer
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x36, 0x0C);

	I2C_WriteByte(SA_TX_CBUS_Primary, 0x3D, 0xFD);	
	I2C_WriteByte(SA_TX_CBUS_Primary, 0x1C, 0x00);

	I2C_WriteByte(SA_TX_CBUS_Primary, 0x44, 0x02);

	I2C_WriteByte(SA_TX_Page0_Primary, 0x05, 0x04); 		// Enable Auto soft reset on SCDT = 0

	I2C_WriteByte(SA_TX_Page0_Primary, 0x0D, 0x1C); 			// HDMI Transcode mode enable

	UNMASK_INTR_4_INTERRUPTS;

	//I2C_WriteByte(SA_TX_Page0_Primary, 0x78, BIT_6) 
	SiI9234_startTPI();
	WriteByteTPI(TPI_INTERRUPT_ENABLE_REG, 0x02);
	//ReadModifyWriteTPI(TPI_INTERRUPT_ENABLE_REG, 0x03, 	0x03);	 //enable HPD and RSEN interrupt

}
#endif

////////////////////////////////////////////////////////////////////
// Int4Isr
//
//
//	Look for interrupts on INTR4 (Register 0x74)
//		7 = RSVD		(reserved)
//		6 = RGND Rdy	(interested)
//		5 = VBUS Low	(ignore)	
//		4 = CBUS LKOUT	(interested)
//		3 = USB EST		(interested)
//		2 = MHL EST		(interested)
//		1 = RPWR5V Change	(ignore)
//		0 = SCDT Change	(interested during D0)
////////////////////////////////////////////////////////////////////
static	void	Int4Isr( void )
{
	byte		reg74;

	reg74 = I2C_ReadByte(SA_TX_Page0_Primary, (0x74));	// read status

	printk("REG74 : %x\n",(int)reg74);

	// When I2C is inoperational (say in D3) and a previous interrupt brought us here, do nothing.
	if(0xFF == reg74)
	{
		printk("RETURN (0xFF == reg74)\n");
		return;
	}

	if(reg74 & BIT_2) // MHL_EST_INT
	{
		MASK_CBUS1_INTERRUPTS; 
		MASK_CBUS2_INTERRUPTS;
		MhlTxDrvProcessConnection(); 
	}

	// process USB_EST interrupt
	else if(reg74 & BIT_3) // MHL_DISC_FAIL_INT
	{
		if(mhl_cable_status == (MHL_1K_IMPEDANCE_VERIFIED|MHL_POWER_ON))//|MHL_RSEN_VERIFIED))
		{
			mhl_cable_status =MHL_TV_OFF_CABLE_CONNECT;
			printk(KERN_ERR "MHL Connection Fail Power off ###2\n");

			//3355
			if(mhl_vbus == TRUE)
			{          
				vbus_mhl_est_fail = TRUE;
				MhlTxDrvProcessDisconnection();
#ifdef CONFIG_USB_SWITCH_FSA9480
				FSA9480_MhlSwitchSel(0); 
#endif
				return;
			}
			else
			{
#ifdef CONFIG_USB_SWITCH_FSA9480
				FSA9480_MhlTvOff();//Rajucm: Mhl cable handling when TV Off 		
#endif
			}//3355
		}
		else
		{
		MhlTxDrvProcessDisconnection();
		}
		return;
	}

	if((TX_POWER_STATE_D3 == fwPowerState) && (reg74 & BIT_6))
	{
		// process RGND interrupt
		// Switch to full power mode.
		SwitchToD0();

		//
		// If a sink is connected but not powered on, this interrupt can keep coming
		// Determine when to go back to sleep. Say after 1 second of this state.
		//
		// Check RGND register and send wake up pulse to the peer
		//
		msleep(1);
		ProcessRgnd();
	}

	// CBUS Lockout interrupt?
	if (reg74 & BIT_4)
	{
		TX_DEBUG_PRINT(("Drv: CBus Lockout\n"));

		ForceUsbIdSwitchOpen();
		ReleaseUsbIdSwitchOpen();
	}
	I2C_WriteByte(SA_TX_Page0_Primary, (0x74), reg74);	// clear all interrupts


}
///////////////////////////////////////////////////////////////////////////
//
// MhlTxDrvProcessConnection
//
///////////////////////////////////////////////////////////////////////////
void MhlTxDrvProcessConnection ( void )
{
	bool	mhlConnected = TRUE;

	TX_DEBUG_PRINT (("Drv: MHL Cable Connected. CBUS:0x0A = %02X\n", (int) ReadByteCBUS(0x0a)));
    
	if( TX_POWER_STATE_D0_MHL == fwPowerState )
	{
		TX_DEBUG_PRINT(("POWER_STATE_D0_MHL == fwPowerState\n"));
		return;
	}

	TX_DEBUG_PRINT(("$$$$$$$\n"));
	
	//
	// Discovery over-ride: reg_disc_ovride	
	//
	I2C_WriteByte(SA_TX_Page0_Primary, 0xA0, 0x10);

	fwPowerState = TX_POWER_STATE_D0_MHL;


	// Increase DDC translation layer timer (byte mode)
	// Setting DDC Byte Mode
	//
	WriteByteCBUS(0x07, 0x32);

	// Enable segment pointer safety
	SET_BIT(SA_TX_CBUS_Primary, 0x44, 1);
	

	// Un-force HPD (it was kept low, now propagate to source
	CLR_BIT(SA_TX_Page0_Primary, 0x79, 4);

	// Enable TMDS
	SiiMhlTxDrvTmdsControl( TRUE );

	// Keep the discovery enabled. Need RGND interrupt
	// SET_BIT(SA_TX_Page0_Primary, 0x90, 0);
	ENABLE_DISCOVERY;

	// Notify upper layer of cable connection
	SiiMhlTxNotifyConnection(mhlConnected = TRUE);
}
///////////////////////////////////////////////////////////////////////////
//
// MhlTxDrvProcessDisconnection
//
///////////////////////////////////////////////////////////////////////////
void MhlTxDrvProcessDisconnection ( void )
{
	bool	mhlConnected = FALSE;

	TX_DEBUG_PRINT (("Drv: MhlTxDrvProcessDisconnection\n"));

	// clear all interrupts
	I2C_WriteByte(SA_TX_Page0_Primary, (0x74), I2C_ReadByte(SA_TX_Page0_Primary, (0x74)));

	I2C_WriteByte(SA_TX_Page0_Primary, 0xA0, 0xD0);

	//
	// Reset CBus to clear register contents
	// This may need some key reinitializations
	//
	CbusReset();

	// Disable TMDS
	SiiMhlTxDrvTmdsControl( FALSE );

	if( TX_POWER_STATE_D0_MHL == fwPowerState )
	{
		// Notify upper layer of cable connection
		SiiMhlTxNotifyConnection(mhlConnected = FALSE);
	}

	// Now put chip in sleep mode
	SwitchToD3();
}
///////////////////////////////////////////////////////////////////////////
//
// CbusReset
//
///////////////////////////////////////////////////////////////////////////
void	CbusReset( void )
{
	SET_BIT(SA_TX_Page0_Primary, 0x05, 3);
	DelayMS(2);
	CLR_BIT(SA_TX_Page0_Primary, 0x05, 3);

	mscCmdInProgress = FALSE;

	// Adjust interrupt mask everytime reset is performed.
	UNMASK_CBUS1_INTERRUPTS;
	UNMASK_CBUS2_INTERRUPTS;
}
///////////////////////////////////////////////////////////////////////////
//
// CBusProcessErrors
//
//
///////////////////////////////////////////////////////////////////////////
static byte CBusProcessErrors( byte intStatus )
{
    byte result          = 0;
    byte mscAbortReason  = 0;
	byte ddcAbortReason  = 0;

    /* At this point, we only need to look at the abort interrupts. */

    intStatus &=  (BIT_MSC_ABORT | BIT_MSC_XFR_ABORT);

    if ( intStatus )
    {
//      result = ERROR_CBUS_ABORT;		// No Retry will help

        /* If transfer abort or MSC abort, clear the abort reason register. */
		if( intStatus & BIT_DDC_ABORT )
		{
			result = ddcAbortReason = ReadByteCBUS( (byte)REG_DDC_ABORT_REASON );
			TX_DEBUG_PRINT( ("CBUS DDC ABORT happened, reason:: %02X\n", (int)(ddcAbortReason)));
		}

        if ( intStatus & BIT_MSC_XFR_ABORT )
        {
            result = mscAbortReason = ReadByteCBUS( (byte)REG_PRI_XFR_ABORT_REASON );

            TX_DEBUG_PRINT( ("CBUS:: MSC Transfer ABORTED. Clearing 0x0D\n"));
            WriteByteCBUS( (byte)REG_PRI_XFR_ABORT_REASON, 0xFF );
        }
        if ( intStatus & BIT_MSC_ABORT )
        {
            TX_DEBUG_PRINT( ("CBUS:: MSC Peer sent an ABORT. Clearing 0x0E\n"));
            WriteByteCBUS( (byte)REG_CBUS_PRI_FWR_ABORT_REASON, 0xFF );
        }

        // Now display the abort reason.

        if ( mscAbortReason != 0 )
        {
            TX_DEBUG_PRINT( ("CBUS:: Reason for ABORT is ....0x%02X = ", (int)mscAbortReason ));

            if ( mscAbortReason & CBUSABORT_BIT_REQ_MAXFAIL)
            {
                TX_DEBUG_PRINT( ("Requestor MAXFAIL - retry threshold exceeded\n"));
            }
            if ( mscAbortReason & CBUSABORT_BIT_PROTOCOL_ERROR)
            {
                TX_DEBUG_PRINT( ("Protocol Error\n"));
            }
            if ( mscAbortReason & CBUSABORT_BIT_REQ_TIMEOUT)
            {
                TX_DEBUG_PRINT( ("Requestor translation layer timeout\n"));
            }
            if ( mscAbortReason & CBUSABORT_BIT_PEER_ABORTED)
            {
                TX_DEBUG_PRINT( ("Peer sent an abort\n"));
            }
            if ( mscAbortReason & CBUSABORT_BIT_UNDEFINED_OPCODE)
            {
                TX_DEBUG_PRINT( ("Undefined opcode\n"));
            }
        }
    }
    return( result );
}

///////////////////////////////////////////////////////////////////////////
//
// MhlCbusIsr
//
// Only when MHL connection has been established. This is where we have the
// first looks on the CBUS incoming commands or returned data bytes for the
// previous outgoing command.
//
// It simply stores the event and allows application to pick up the event
// and respond at leisure.
//
// Look for interrupts on CBUS:CBUS_INTR_STATUS [0xC8:0x08]
//		7 = RSVD			(reserved)
//		6 = MSC_RESP_ABORT	(interested)
//		5 = MSC_REQ_ABORT	(interested)	
//		4 = MSC_REQ_DONE	(interested)
//		3 = MSC_MSG_RCVD	(interested)
//		2 = DDC_ABORT		(interested)
//		1 = RSVD			(reserved)
//		0 = rsvd			(reserved)
///////////////////////////////////////////////////////////////////////////
static void MhlCbusIsr( void )
{
	byte		cbusInt;
	byte     gotData[4];	// Max four status and int registers.
	byte		i;

	//
	// Main CBUS interrupts on CBUS_INTR_STATUS
	//
	cbusInt = ReadByteCBUS(0x08);

	// When I2C is inoperational (say in D3) and a previous interrupt brought us here, do nothing.
	if(cbusInt == 0xFF)
	{
		return;
	}
	if( cbusInt )
	{
	    TX_DEBUG_PRINT(("Drv: CBUS INTR_1: %d\n", (int) cbusInt));
	}

	// Look for DDC_ABORT
	if (cbusInt & BIT_2)
	{
		ApplyDdcAbortSafety();
	}
	// MSC_MSG (RCP/RAP)
	if((cbusInt & BIT_3))
	{
	    TX_DEBUG_PRINT(("Drv: MSC_MSG Received: %02X\n", (int) cbusInt));
		//
		// Two bytes arrive at registers 0x18 and 0x19
		//
		SiiMhlTxGotMhlMscMsg( ReadByteCBUS( 0x18 ), ReadByteCBUS( 0x19 ) );
	}
  #if 0
	// MSC_REQ_DONE received.
	if(cbusInt & BIT_4)
	{
	    TX_DEBUG_PRINT(("Drv: MSC_REQ_DONE: %02X\n", (int) cbusInt));

		mscCmdInProgress = FALSE;

		SiiMhlTxMscCommandDone( ReadByteCBUS( 0x16 ) );
	}
  #endif
	if((cbusInt & BIT_5) || (cbusInt & BIT_6))	// MSC_REQ_ABORT or MSC_RESP_ABORT
	{
		gotData[0] = CBusProcessErrors(cbusInt);
	}
	if(cbusInt)
	{
		//
		// Clear all interrupts that were raised even if we did not process
		//
		WriteByteCBUS(0x08, cbusInt);

	    TX_DEBUG_PRINT(("Drv: Clear CBUS INTR_1: %02X\n", (int) cbusInt));
	}
  // MSC_REQ_DONE received.
	if(cbusInt & BIT_4)
	{
		TX_DEBUG_PRINT(("Drv: MSC_REQ_DONE: %02X\n", (int) cbusInt));

		mscCmdInProgress = FALSE;

		SiiMhlTxMscCommandDone( ReadByteCBUS( 0x16 ) );
	}
	//
	// Clear all interrupts that were raised even if we did not process
	//

	//
	// Now look for interrupts on register 0x1E. CBUS_MSC_INT2
	// 7:4 = Reserved
	//   3 = msc_mr_write_state = We got a WRITE_STAT
	//   2 = msc_mr_set_int. We got a SET_INT
	//   1 = reserved
	//   0 = msc_mr_write_burst. We received WRITE_BURST
	//
	cbusInt = ReadByteCBUS(0x1E);
	if( cbusInt )
	{
	    TX_DEBUG_PRINT(("Drv: CBUS INTR_2: %x\n", (int) cbusInt));
	}
	if(cbusInt & BIT_2)
	{
	    TX_DEBUG_PRINT(("Drv: INT Received: %x\n", (int) cbusInt));

		// We are interested only in first two bytes.
		SiiMhlTxGotMhlIntr( ReadByteCBUS( 0xA0 ), ReadByteCBUS( 0xA1) );

   		for(i = 0; i < 4; i++)
		{
			// Clear all
			WriteByteCBUS( (0xA0 + i), ReadByteCBUS( 0xA0 + i ));
		}
	}
	if(cbusInt & BIT_3)
	{
	    TX_DEBUG_PRINT(("Drv: STATUS Received: %x\n", (int) cbusInt));

		// We are interested only in first two bytes.
		SiiMhlTxGotMhlStatus( ReadByteCBUS( 0xB0 ), ReadByteCBUS( 0xB1) );

		for(i = 0; i < 4; i++)
		{
			// Clear all
			WriteByteCBUS( (0xB0 + i), ReadByteCBUS( 0xB0 + i ));
		}
	}
	if(cbusInt)
	{
		//
		// Clear all interrupts that were raised even if we did not process
		//
		WriteByteCBUS(0x1E, cbusInt);

	    TX_DEBUG_PRINT(("Drv: Clear CBUS INTR_2: %02X\n", (int) cbusInt));
	}

  #if 0
	//
	// Check if a SET_HPD came from the downstream device.
	//
	cbusInt = ReadByteCBUS(0x0D);

	// CBUS_HPD status bit
	if((BIT_6 & cbusInt) != dsHpdStatus)
	{
			// Inform upper layer of change in Downstream HPD
			SiiMhlTxNotifyDsHpdChange( cbusInt );
		    TX_DEBUG_PRINT(("Drv: Downstream HPD changed to: %02X\n", (int) cbusInt));

			// Remember
			dsHpdStatus = (BIT_6 & cbusInt);
	}
  #endif
}




///////////////////////////////////////////////////////////////////////////////
// SiiMhlTxInitialize
//
// Sets the transmitter component firmware up for operation, brings up chip
// into power on state first and then back to reduced-power mode D3 to conserve
// power until an MHL cable connection has been established. If the MHL port is
// used for USB operation, the chip and firmware continue to stay in D3 mode.
// Only a small circuit in the chip observes the impedance variations to see if
// processor should be interrupted to continue MHL discovery process or not.
//
// interruptDriven		If TRUE, MhlTx component will not look at its status
//						registers in a polled manner from timer handler 
//						(SiiMhlTxGetEvents). It will expect that all device 
//						events will result in call to the function 
//						SiiMhlTxDeviceIsr() by host's hardware or software 
//						(a master interrupt handler in host software can call
//						it directly). interruptDriven == TRUE also implies that
//						the MhlTx component shall make use of AppDisableInterrupts()
//						and AppRestoreInterrupts() for any critical section work to
//						prevent concurrency issues.
//
//						When interruptDriven == FALSE, MhlTx component will do
//						all chip status analysis via looking at its register
//						when called periodically into the function
//						SiiMhlTxGetEvents() described below.
//
// pollIntervalMs		This number should be higher than 0 and lower than 
//						51 milliseconds for effective operation of the firmware.
//						A higher number will only imply a slower response to an 
//						event on MHL side which can lead to violation of a 
//						connection disconnection related timing or a slower 
//						response to RCP messages.
//
//
//
//
//void SiiMhlTxInitialize( bool interruptDriven, byte pollIntervalMs )
void SiiMhlTxInitialize( void )
{
	TX_DEBUG_PRINT( ("MhlTx: SiiMhlTxInitialize\n") );

	MhlTxResetStates( );
	
	mhl_cable_status = MHL_POWER_ON;			//NAGSM_Android_SEL_Kernel_Aakash_20101214
	
	SiiMhlTxChipInitialize ();
	
}


///////////////////////////////////////////////////////////////////////////////
// 
// SiiMhlTxGetEvents
//
// This is a function in MhlTx that must be called by application in a periodic
// fashion. The accuracy of frequency (adherence to the parameter pollIntervalMs)
// will determine adherence to some timings in the MHL specifications, however,
// MhlTx component keeps a tolerance of up to 50 milliseconds for most of the
// timings and deploys interrupt disabled mode of operation (applicable only to
// Sii 9244) for creating precise pulse of smaller duration such as 20 ms.
//
// This function does not return anything but it does modify the contents of the
// two pointers passed as parameter.
//
// It is advantageous for application to call this function in task context so
// that interrupt nesting or concurrency issues do not arise. In addition, by
// collecting the events in the same periodic polling mechanism prevents a call
// back from the MhlTx which can result in sending yet another MHL message.
//
// An example of this is responding back to an RCP message by another message
// such as RCPK or RCPE.
//
//
// *event		MhlTx returns a value in this field when function completes execution.
// 				If this field is 0, the next parameter is undefined.
//				The following values may be returned.
//
//
void SiiMhlTxGetEvents( byte *event, byte *eventParameter )
{

  SiiMhlTxDeviceIsr();
	if(mhl_cable_status == MHL_TV_OFF_CABLE_CONNECT)
	{
		return ;
	}

	MhlTxDriveStates( );

	*event = MHL_TX_EVENT_NONE;
	*eventParameter = 0;

	if( mhlTxConfig.mhlConnectionEvent )
	{
		TX_DEBUG_PRINT( ("MhlTx: SiiMhlTxGetEvents mhlConnectionEvent\n") );

		// Consume the message
		mhlTxConfig.mhlConnectionEvent = FALSE;

		//
		// Let app know the connection went away.
		//
		*event          = mhlTxConfig.mhlConnected;
		*eventParameter	= mhlTxConfig.mscFeatureFlag;

		// If connection has been lost, reset all state flags.
		if(MHL_TX_EVENT_DISCONNECTION == mhlTxConfig.mhlConnected)
		{
			MhlTxResetStates( );
		}
	}
	else if( mhlTxConfig.mscMsgArrived )
	{
		TX_DEBUG_PRINT( ("MhlTx: SiiMhlTxGetEvents MSC MSG Arrived\n") );

		// Consume the message
		mhlTxConfig.mscMsgArrived = FALSE;

		//
		// Map sub-command to an event id
		//
		switch(mhlTxConfig.mscMsgSubCommand)
		{
			case	MHL_MSC_MSG_RAP:
				// RAP is fully handled here.
				//
				// Handle RAP sub-commands here itself
				//
				if( MHL_RAP_CONTENT_ON == mhlTxConfig.mscMsgData)
				{
					SiiMhlTxDrvTmdsControl( TRUE );
				}
				else if( MHL_RAP_CONTENT_OFF == mhlTxConfig.mscMsgData)
				{
					SiiMhlTxDrvTmdsControl( FALSE );

				}
				// Always RAPK to the peer
				SiiMhlTxRapkSend( );
				break;

			case	MHL_MSC_MSG_RCP:

				// If we get a RCP key that we do NOT support, send back RCPE
				// Do not notify app layer.
				if(MHL_LOGICAL_DEVICE_MAP & rcpSupportTable [mhlTxConfig.mscMsgData] )
				{
					*event          = MHL_TX_EVENT_RCP_RECEIVED;
					*eventParameter = mhlTxConfig.mscMsgData; // key code
			              rcp_cbus_uevent(*eventParameter);	
			              printk("Key Code:%x \n",(int)mhlTxConfig.mscMsgData);
				}
				else
				{
  				       printk("Key Code Error:%x \n",(int)mhlTxConfig.mscMsgData);
                                   mhlTxConfig.mscSaveRcpKeyCode = mhlTxConfig.mscMsgData;
					SiiMhlTxRcpeSend( 0x01 );
				}
				break;

			case	MHL_MSC_MSG_RCPK:
				*event = MHL_TX_EVENT_RCPK_RECEIVED;
        		*eventParameter = mhlTxConfig.mscMsgData; // key code
				break;

			case	MHL_MSC_MSG_RCPE:
				*event = MHL_TX_EVENT_RCPE_RECEIVED;
        		*eventParameter = mhlTxConfig.mscMsgData; // status code
				break;

			case	MHL_MSC_MSG_RAPK:
				// Do nothing if RAPK comes
				break;

			default:
				// Any freak value here would continue with no event to app
				break;
		}

	}
}
///////////////////////////////////////////////////////////////////////////////
//
// MhlTxDriveStates
//
// This is an internal function to move the MSC engine to do the next thing
// before allowing the application to run RCP APIs.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printks.
//
static	void	MhlTxDriveStates( void )
{

	switch( mhlTxConfig.mscState )
	{
		case MSC_STATE_BEGIN:
      printk("MSC_STATE_BEGIN \n");
			SiiMhlTxReadDevcap( MHL_DEV_CATEGORY_OFFSET );
			break;
		case MSC_STATE_POW_DONE:
			//
			// Send out Read Devcap for MHL_DEV_FEATURE_FLAG_OFFSET
			// to check if it supports RCP/RAP etc
			//
			printk("MSC_STATE_POW_DONE \n");
			SiiMhlTxReadDevcap( MHL_DEV_FEATURE_FLAG_OFFSET );
			break;
		case MSC_STATE_IDLE:
		case MSC_STATE_RCP_READY:
			break;
		default:
			break;

	}
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxMscCommandDone
//
// This function is called by the driver to inform of completion of last command.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printks.
//
void	SiiMhlTxMscCommandDone( byte data1 )
{
	TX_DEBUG_PRINT( ("MhlTx: SiiMhlTxMscCommandDone. data1 = %02X\n", (int) data1) );

	if(( MHL_READ_DEVCAP == mhlTxConfig.mscLastCommand ) && 
			(MHL_DEV_CATEGORY_OFFSET == mhlTxConfig.mscLastOffset))
	{
		// We are done reading POW. Next we read Feature Flag
		mhlTxConfig.mscState	= MSC_STATE_POW_DONE;

		AppVbusControl( (bool) ( data1 & MHL_DEV_CATEGORY_POW_BIT) );

		//
		// Send out Read Devcap for MHL_DEV_FEATURE_FLAG_OFFSET
		// to check if it supports RCP/RAP etc
		//
	}
	else if((MHL_READ_DEVCAP == mhlTxConfig.mscLastCommand) &&
				(MHL_DEV_FEATURE_FLAG_OFFSET == mhlTxConfig.mscLastOffset))
	{
		// We are done reading Feature Flag. Let app know we are done.
		mhlTxConfig.mscState	= MSC_STATE_RCP_READY;

		// Remember features of the peer
		mhlTxConfig.mscFeatureFlag	= data1;

		// Now we can entertain App commands for RCP
		// Let app know this state
		mhlTxConfig.mhlConnectionEvent = TRUE;
		mhlTxConfig.mhlConnected = MHL_TX_EVENT_RCP_READY;

		// These variables are used to remember if we issued a READ_DEVCAP
		// Since we are done, reset them.
		mhlTxConfig.mscLastCommand = 0;
		mhlTxConfig.mscLastOffset  = 0;

		TX_DEBUG_PRINT( ("MhlTx: Peer's Feature Flag = %02X\n\n", (int) data1) );
	}
	else if(MHL_MSC_MSG_RCPE == mhlTxConfig.mscMsgLastCommand)
	{
		//
		// RCPE is always followed by an RCPK with original key code that came.
		//
		if( SiiMhlTxRcpkSend( mhlTxConfig.mscSaveRcpKeyCode ) )
		{
			// Once the command has been sent out successfully, forget this case.
			mhlTxConfig.mscMsgLastCommand = 0;
			mhlTxConfig.mscMsgLastData    = 0;
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxGotMhlMscMsg
//
// This function is called by the driver to inform of arrival of a MHL MSC_MSG
// such as RCP, RCPK, RCPE. To quickly return back to interrupt, this function
// remembers the event (to be picked up by app later in task context).
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing of its own,
//
// No printks.
//
// Application shall not call this function.
//
void	SiiMhlTxGotMhlMscMsg( byte subCommand, byte cmdData )
{
	// Remeber the event.
	mhlTxConfig.mscMsgArrived		= TRUE;
	mhlTxConfig.mscMsgSubCommand	= subCommand;
	mhlTxConfig.mscMsgData			= cmdData;
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxGotMhlIntr
//
// This function is called by the driver to inform of arrival of a MHL INTERRUPT.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printks.
//
void	SiiMhlTxGotMhlIntr( byte intr_0, byte intr_1 )
{
	TX_DEBUG_PRINT( ("MhlTx: INTERRUPT Arrived. %02X, %02X\n", (int) intr_0, (int) intr_1) );

	//
	// Handle DCAP_CHG INTR here
	//
	if(MHL_INT_DCAP_CHG & intr_0)
	{
		SiiMhlTxReadDevcap( MHL_DEV_CATEGORY_OFFSET );
	}
	else if(MHL_INT_EDID_CHG & intr_1)
	{
		// force upstream source to read the EDID again.
		// Most likely by appropriate togggling of HDMI HPD
		SiiMhlTxDrvNotifyEdidChange ( );
	}
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxGotMhlStatus
//
// This function is called by the driver to inform of arrival of a MHL STATUS.
//
// It is called in interrupt context to meet some MHL specified timings, therefore,
// it should not have to call app layer and do negligible processing, no printks.
//
void	SiiMhlTxGotMhlStatus( byte status_0, byte status_1 )
{
	TX_DEBUG_PRINT( ("MhlTx: STATUS Arrived. %02X, %02X\n", (int) status_0, (int) status_1) );
	//
	// Handle DCAP_RDY STATUS here itself
	//
	if(MHL_STATUS_DCAP_RDY & status_0)
	{
	//		MhlTxDriveStates( );
	//		SiiMhlTxReadDevcap( MHL_DEV_CATEGORY_OFFSET );
    mhlTxConfig.mscState	 = MSC_STATE_BEGIN;
	}
	// status_1 has the PATH_EN etc. Not yet implemented
	// Remeber the event.
	mhlTxConfig.status_0 = status_0;
	mhlTxConfig.status_1 = status_1;
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxRcpSend
//
// This function checks if the peer device supports RCP and sends rcpKeyCode. The
// function will return a value of TRUE if it could successfully send the RCP
// subcommand and the key code. Otherwise FALSE.
//
// The followings are not yet utilized.
// 
// (MHL_FEATURE_RAP_SUPPORT & mhlTxConfig.mscFeatureFlag))
// (MHL_FEATURE_SP_SUPPORT & mhlTxConfig.mscFeatureFlag))
//
//
bool SiiMhlTxRcpSend( byte rcpKeyCode )
{
	//
	// If peer does not support do not send RCP or RCPK/RCPE commands
	//
	if((0 == (MHL_FEATURE_RCP_SUPPORT & mhlTxConfig.mscFeatureFlag)) ||
		(MSC_STATE_RCP_READY != mhlTxConfig.mscState))
	{
		return	FALSE;
	}
	return	( MhlTxSendMscMsg ( MHL_MSC_MSG_RCP, rcpKeyCode ) );
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxRcpkSend
//
// This function sends RCPK to the peer device. 
//
bool SiiMhlTxRcpkSend( byte rcpKeyCode )
{
	return	( MhlTxSendMscMsg ( MHL_MSC_MSG_RCPK, rcpKeyCode ) );
}

///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxRapkSend
//
// This function sends RAPK to the peer device. 
//
static	bool SiiMhlTxRapkSend( void )
{
	return	( MhlTxSendMscMsg ( MHL_MSC_MSG_RAPK, 0 ) );
}

  ///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxRcpeSend
//
// The function will return a value of true if it could successfully send the RCPE
// subcommand. Otherwise false.
//
// When successful, MhlTx internally sends RCPK with original (last known)
// keycode.
//
bool SiiMhlTxRcpeSend( byte rcpeErrorCode )
{
	return( MhlTxSendMscMsg ( MHL_MSC_MSG_RCPE, rcpeErrorCode ) );
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxReadDevcap
//
// This function sends a READ DEVCAP MHL command to the peer.
// It  returns TRUE if successful in doing so.
//
// The value of devcap should be obtained by making a call to SiiMhlTxGetEvents()
//
// offset		Which byte in devcap register is required to be read. 0..0x0E
//
bool SiiMhlTxReadDevcap( byte offset )
{
	cbus_req_t	req;
	//
	// Send MHL_READ_DEVCAP command
	//
	req.command     = mhlTxConfig.mscLastCommand = MHL_READ_DEVCAP;
	req.offsetData  = mhlTxConfig.mscLastOffset  = offset;
	return(SiiMhlTxDrvSendCbusCommand( &req  ));
}

///////////////////////////////////////////////////////////////////////////////
//
// MhlTxSendMscMsg
//
// This function sends a MSC_MSG command to the peer.
// It  returns TRUE if successful in doing so.
//
// The value of devcap should be obtained by making a call to SiiMhlTxGetEvents()
//
// offset		Which byte in devcap register is required to be read. 0..0x0E
//
static bool MhlTxSendMscMsg ( byte command, byte cmdData )
{
	cbus_req_t	req;
	byte		ccode;

	//
	// Send MSC_MSG command
	//
	req.command     = MHL_MSC_MSG;

	req.msgData[0]  = mhlTxConfig.mscMsgLastCommand = command;
	req.msgData[1]  = mhlTxConfig.mscMsgLastData    = cmdData;

	ccode = SiiMhlTxDrvSendCbusCommand( &req  );
	return( (bool) ccode );

}
///////////////////////////////////////////////////////////////////////////////
// 
// SiiMhlTxNotifyConnection
//
//
void	SiiMhlTxNotifyConnection( bool mhlConnected )
{
	//printk("SiiMhlTxNotifyConnection %01X\n", (int) mhlConnected );
	mhlTxConfig.mhlConnectionEvent = TRUE;

  	mhlTxConfig.mscState	 = MSC_STATE_IDLE;
	if(mhlConnected)
	{
		mhlTxConfig.mhlConnected = MHL_TX_EVENT_CONNECTION;
	}
	else
	{
		mhlTxConfig.mhlConnected = MHL_TX_EVENT_DISCONNECTION;
	}
}
///////////////////////////////////////////////////////////////////////////////
//
// SiiMhlTxNotifyDsHpdChange
// Driver tells about arrival of SET_HPD or CLEAR_HPD by calling this function.
//
// Turn the content off or on based on what we got.
//
void	SiiMhlTxNotifyDsHpdChange( byte dsHpdStatus )
{
	if( 0 == dsHpdStatus )
	{
	    TX_DEBUG_PRINT(("MhlTx: Disable TMDS\n"));
		SiiMhlTxDrvTmdsControl( FALSE );
	}
	else
	{
	    TX_DEBUG_PRINT(("MhlTx: Enable TMDS\n"));
		SiiMhlTxDrvTmdsControl( TRUE );
	}
}
///////////////////////////////////////////////////////////////////////////////
//
// MhlTxResetStates
//
// Application picks up mhl connection and rcp events at periodic intervals.
// Interrupt handler feeds these variables. Reset them on disconnection. 
//
static void	MhlTxResetStates( void )
{
	mhlTxConfig.mhlConnectionEvent	= FALSE;
	mhlTxConfig.mhlConnected		= MHL_TX_EVENT_DISCONNECTION;
	mhlTxConfig.mscMsgArrived		= FALSE;
	mhlTxConfig.mscState			= MSC_STATE_IDLE;
}

#define	APP_DEMO_RCP_SEND_KEY_CODE 0x44

///////////////////////////////////////////////////////////////////////////////
//
// AppRcpDemo
//
// This function is supposed to provide a demo code to elicit how to call RCP
// API function.
//
void	AppRcpDemo( byte event, byte eventParameter)
{
	byte		rcpKeyCode;

//	printk("App: Got event = %02X, eventParameter = %02X\n", (int)event, (int)eventParameter);

	switch( event )
	{
		case	MHL_TX_EVENT_DISCONNECTION:
			printk("App: Got event = MHL_TX_EVENT_DISCONNECTION\n");
			break;

		case	MHL_TX_EVENT_CONNECTION:
			printk("App: Got event = MHL_TX_EVENT_CONNECTION\n");
			break;

		case	MHL_TX_EVENT_RCP_READY:
			// Demo RCP key code PLAY
			rcpKeyCode = APP_DEMO_RCP_SEND_KEY_CODE;
			printk("App: Got event = MHL_TX_EVENT_RCP_READY...Sending RCP (%02X)\n", (int) rcpKeyCode);
			break;

		case	MHL_TX_EVENT_RCP_RECEIVED:
			//
			// Check if we got an RCP. Application can perform the operation here
			// and send RCPK or RCPE. For now, we send the RCPK
			//
			printk("App: Received an RCP key code = %02X\n", eventParameter );

      switch(eventParameter)
      {

        case MHD_RCP_CMD_SELECT:
          TX_DEBUG_PRINT(( "\nSelect received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_UP:
          TX_DEBUG_PRINT(( "\nUp received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_DOWN:
          TX_DEBUG_PRINT(( "\nDown received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_LEFT:
          TX_DEBUG_PRINT(( "\nLeft received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_RIGHT:
          TX_DEBUG_PRINT(( "\nRight received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_RIGHT_UP:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_RIGHT_UP\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_RIGHT_DOWN:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_RIGHT_DOWN \n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_LEFT_UP:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_LEFT_UP\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_LEFT_DOWN:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_LEFT_DOWN\n\n"/*, (int)eventParameter*/ ));
        break;      

        case MHD_RCP_CMD_ROOT_MENU:
          TX_DEBUG_PRINT(( "\nRoot Menu received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_SETUP_MENU:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_SETUP_MENU\n\n"/*, (int)eventParameter*/ ));
        break;      

        case MHD_RCP_CMD_CONTENTS_MENU:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_CONTENTS_MENU\n\n"/*, (int)eventParameter*/ ));
        break;      

        case MHD_RCP_CMD_FAVORITE_MENU:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_FAVORITE_MENU\n\n"/*, (int)eventParameter*/ ));
        break;            

        case MHD_RCP_CMD_EXIT:
          TX_DEBUG_PRINT(( "\nExit received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_NUM_0:
          TX_DEBUG_PRINT(( "\nNumber 0 received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_NUM_1:
          TX_DEBUG_PRINT(( "\nNumber 1 received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_NUM_2:
          TX_DEBUG_PRINT(( "\nNumber 2 received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_NUM_3:
          TX_DEBUG_PRINT(( "\nNumber 3 received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_NUM_4:
          TX_DEBUG_PRINT(( "\nNumber 4 received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_NUM_5:
          TX_DEBUG_PRINT(( "\nNumber 5 received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_NUM_6:
          TX_DEBUG_PRINT(( "\nNumber 6 received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_NUM_7:
          TX_DEBUG_PRINT(( "\nNumber 7 received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_NUM_8:
          TX_DEBUG_PRINT(( "\nNumber 8 received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_NUM_9:
          TX_DEBUG_PRINT(( "\nNumber 9 received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_DOT:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_DOT\n\n"/*, (int)eventParameter*/ ));
        break;          

        case MHD_RCP_CMD_ENTER:
          TX_DEBUG_PRINT(( "\nEnter received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_CLEAR:
          TX_DEBUG_PRINT(( "\nClear received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_CH_UP:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_CH_UP\n\n"/*, (int)eventParameter*/ ));
        break; 

        case MHD_RCP_CMD_CH_DOWN:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_CH_DOWN\n\n"/*, (int)eventParameter*/ ));
        break;       

        case MHD_RCP_CMD_PRE_CH:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_PRE_CH\n\n"/*, (int)eventParameter*/ ));
        break;           

        case MHD_RCP_CMD_SOUND_SELECT:
          TX_DEBUG_PRINT(( "\nSound Select received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_INPUT_SELECT:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_INPUT_SELECT\n\n"/*, (int)eventParameter*/ ));
        break;    

        case MHD_RCP_CMD_SHOW_INFO:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_SHOW_INFO\n\n"/*, (int)eventParameter*/ ));
        break;     

        case MHD_RCP_CMD_HELP:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_HELP\n\n"/*, (int)eventParameter*/ ));
        break;   

        case MHD_RCP_CMD_PAGE_UP:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_PAGE_UP\n\n"/*, (int)eventParameter*/ ));
        break;  

        case MHD_RCP_CMD_PAGE_DOWN:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_PAGE_DOWN\n\n"/*, (int)eventParameter*/ ));
        break;             

        case MHD_RCP_CMD_VOL_UP:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_VOL_UP\n\n"/*, (int)eventParameter*/ ));
        break;             

        case MHD_RCP_CMD_VOL_DOWN:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_VOL_DOWN\n\n"/*, (int)eventParameter*/ ));
        break;             

        case MHD_RCP_CMD_MUTE:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_MUTE\n\n"/*, (int)eventParameter*/ ));
        break;             
                
        case MHD_RCP_CMD_PLAY:
          TX_DEBUG_PRINT(( "\nPlay received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_STOP:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_STOP\n\n"/*, (int)eventParameter*/ ));
        break;   

        case MHD_RCP_CMD_PAUSE:
          TX_DEBUG_PRINT(( "\nPause received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_RECORD:
          TX_DEBUG_PRINT(( "\n MHD_RCP_CMD_RECORD\n\n"/*, (int)eventParameter*/ ));
        break;   

        case MHD_RCP_CMD_FAST_FWD:
          TX_DEBUG_PRINT(( "\nFastfwd received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_REWIND:
          TX_DEBUG_PRINT(( "\nRewind received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_EJECT:
          TX_DEBUG_PRINT(( "\nEject received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_FWD:
          TX_DEBUG_PRINT(( "\nForward received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_BKWD:
          TX_DEBUG_PRINT(( "\nBackward received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_PLAY_FUNC:
          TX_DEBUG_PRINT(( "\nPlay Function received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_PAUSE_PLAY_FUNC:
          TX_DEBUG_PRINT(( "\nPause_Play Function received\n\n"/*, (int)eventParameter*/ ));
        break;

        case MHD_RCP_CMD_STOP_FUNC:
          TX_DEBUG_PRINT(( "\nStop Function received\n\n"/*, (int)eventParameter*/ ));
        break;

        default:

        break;
      }
      
			rcpKeyCode = eventParameter;
			SiiMhlTxRcpkSend(rcpKeyCode);
			break;

		case	MHL_TX_EVENT_RCPK_RECEIVED:
			printk("App: Received an RCPK\n");
			break;

		case	MHL_TX_EVENT_RCPE_RECEIVED:
			printk("App: Received an RCPE\n");
			break;

		default:
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////
//
// AppVbusControl
//
// This function or macro is invoked from MhlTx driver to ask application to
// control the VBUS power. If powerOn is sent as non-zero, one should assume
// peer does not need power so quickly remove VBUS power.
//
// if value of "powerOn" is 0, then application must turn the VBUS power on
// within 50ms of this call to meet MHL specs timing.
//
// Application module must provide this function.
//
void	AppVbusControl( bool powerOn )
{
	if( powerOn )
	{
		printk("App: Peer's POW bit is set. Turn the VBUS power OFF here.\n");
	}
	else
	{
		printk("App: Peer's POW bit is cleared. Turn the VBUS power ON here.\n");
	}
}


/*===========================================================================
  FUNCTION SiI9234_interrupt_event

  DESCRIPTION
When SiI9234 H/W interrupt happen, call this event function

  DEPENDENCIES
  None

  RETURN VALUE
  None

  SIDE EFFECTS
  None
===========================================================================*/
void SiI9234_interrupt_event(void)
{
  byte    event;
  byte                       eventParameter;
  byte    flag;
  byte rsen;
  byte reg99RGNDRange;

    // TX_DEBUG_PRINT(("Start PinTxInt Pin Init : %d \n", (int) PinTxInt));
	do 
	{
		//
		// Look for any events that might have occurred.
		//
		flag = 0;
		SiiMhlTxGetEvents( &event, &eventParameter );
		if( MHL_TX_EVENT_NONE != event )
		{
			AppRcpDemo( event, eventParameter);
		}
		if(mhl_cable_status == MHL_TV_OFF_CABLE_CONNECT)
		{
			byte tmp;
			tmp = I2C_ReadByte(SA_TX_Page0_Primary, (0x74));   // read status
			I2C_WriteByte(SA_TX_Page0_Primary, (0x74), tmp);   // clear all interrupts
			tmp = I2C_ReadByte(SA_TX_Page0_Primary, 0x71);
			I2C_WriteByte(SA_TX_Page0_Primary, 0x71, tmp);

			tmp = ReadByteCBUS(0x08);
			WriteByteCBUS(0x08, tmp);  

			tmp = ReadByteCBUS(0x1E);
			WriteByteCBUS(0x1E, tmp);    
			TX_DEBUG_PRINT(("mhl_cable_status == MHL_TV_OFF_CABLE_CONNECT\n"));
			return ;			
		}
		else if(((fwPowerState == TX_POWER_STATE_D0_MHL)||(fwPowerState == TX_POWER_STATE_D0_NO_MHL))&& 
		mhl_cable_status) //NAGSM_Android_SEL_Kernel_Aakash_20101214
		{
			byte tmp;
			tmp = I2C_ReadByte(SA_TX_Page0_Primary, (0x74));   // read status
			flag |= (tmp&INTR_4_DESIRED_MASK);	  
			printk("#1 (0x74) flag: %x\n",(int) flag );

			//I2C_WriteByte(SA_TX_Page0_Primary, (0x74), tmp);   // clear all interrupts
			tmp = I2C_ReadByte(SA_TX_Page0_Primary, 0x71);
			//I2C_WriteByte(SA_TX_Page0_Primary, 0x71, tmp);
			flag |= (tmp&INTR_1_DESIRED_MASK);
			printk("#1 (0x71) flag: %x\n",(int) flag );

			if(mhlTxConfig.mhlConnected == MHL_TX_EVENT_DISCONNECTION)//(mhlTxConfig_status()== MHL_TX_EVENT_DISCONNECTION)//
			{
				tmp = ReadByteCBUS(0x08);
				printk("#2 (ReadByteCBUS(0x08)) Temp: %x\n",(int) tmp );
				WriteByteCBUS(0x08, tmp);  
				tmp = ReadByteCBUS(0x1E);
				printk("#2 (ReadByteCBUS(0x1E)) Temp: %x\n",(int) tmp );
				WriteByteCBUS(0x1E, tmp);    
			}
			else
			{
				tmp = ReadByteCBUS(0x08);
				flag |= (tmp&INTR_CBUS1_DESIRED_MASK);
				printk("#1 (ReadByteCBUS(0x08)) Temp: %x\n",(int) flag );
				tmp = ReadByteCBUS(0x1E);
				flag |= (tmp&INTR_CBUS2_DESIRED_MASK);
				printk("#1 (ReadByteCBUS(0x1E)) Temp: %x\n",(int) flag );              
			}
			if((flag == 0xFA)||(flag == 0xFF))
			flag = 0;
		}
	}while(flag);
    printk("#$$$$$$$$$$$$$$$ flag: %x\n",(int) flag );

	//Fix to resolve MHL Detection after mhl is disconnected within one second of connection
	rsen  = I2C_ReadByte(SA_TX_Page0_Primary, 0x09) & BIT_2;
	reg99RGNDRange = I2C_ReadByte(SA_TX_Page0_Primary, 0x99) & 0x03;
	if(rsen == 0x00 && (reg99RGNDRange == 0x01 || reg99RGNDRange == 0x02)){
#ifdef CONFIG_USB_SWITCH_FSA9480
		FSA9480_MhlSwitchSel(0);
#endif
	}
}

EXPORT_SYMBOL(SiI9234_interrupt_event);

