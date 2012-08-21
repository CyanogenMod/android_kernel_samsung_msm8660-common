/*===========================================================================
*
*                        SiI9024A I2C MASTER.C
*              
*
* DESCRIPTION
*  This file explains the SiI9024A initialization and call the virtual main function.
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
2011/04/06    Rajkumar c m            Added support for qualcomm msm8060
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


#include "Common_Def.h"
#include "SiI9234_I2C_slave_add.h"


/*===========================================================================

===========================================================================*/
#ifdef READ
#undef READ
#define READ   1
#endif
#ifdef WRITE
#undef WRITE
#define WRITE  0
#endif

#define LAST_BYTE      1
#define NOT_LAST_BYTE  0


#define TPI_INDEXED_PAGE_REG		0xBC
#define TPI_INDEXED_OFFSET_REG		0xBD
#define TPI_INDEXED_VALUE_REG		0xBE

//Rajucm: Avoid mhl read/writes on MHL Power off Or cable disconnected state
extern bool get_mhl_power_state(void);
#define EXIT_ON_CABLE_DISCONNECTION \
do{\
	if(!get_mhl_power_state())	{\
		printk("MHL Power off Or cable disconnected \n\r");\
		return false;\
	}\
}while(0)

#define EXIT_ON_CABLE_DISCONNECTION_V \
do{\
	if(!get_mhl_power_state())	{\
		printk("MHL Power off Or cable disconnected \n\r");\
		return;\
	}\
}while(0)

/*===========================================================================

===========================================================================*/
//------------------------------------------------------------------------------
// Function: I2C_WriteByte
// Description:
//------------------------------------------------------------------------------
void I2C_WriteByte(byte deviceID, byte offset, byte value)
{
	int ret = 0;
	struct i2c_client* client_ptr = get_sii9234_client(deviceID);
	if(!client_ptr)
	{
		printk("[MHL]I2C_WriteByte error %x\n",deviceID); 
		return;	
	}
	
	EXIT_ON_CABLE_DISCONNECTION_V;
	if(deviceID == 0x72)
		ret = SII9234_i2c_write(client_ptr,offset,value);
	else if(deviceID == 0x7A)
		ret = SII9234_i2c_write(client_ptr,offset,value);
	else if(deviceID == 0x92)
		ret = SII9234_i2c_write(client_ptr,offset,value);
	else if(deviceID == 0xC8)
		ret = SII9234_i2c_write(client_ptr,offset,value);

	if (ret < 0)
	{
		printk("I2C_WriteByte: Device ID=0x%X, Err ret = %d \n", deviceID, ret);
	}


}


byte I2C_ReadByte(byte deviceID, byte offset)
{
    	byte number = 0;
	struct i2c_client* client_ptr = get_sii9234_client(deviceID);
	if(!client_ptr)
	{
		printk("[MHL]I2C_ReadByte error %x\n",deviceID); 
		return 0;	
	}

	EXIT_ON_CABLE_DISCONNECTION;
  
  	if(deviceID == 0x72)
		number = SII9234_i2c_read(client_ptr,offset);
	else if(deviceID == 0x7A)
		number = SII9234_i2c_read(client_ptr,offset);
	else if(deviceID == 0x92)
		number = SII9234_i2c_read(client_ptr,offset);
	else if(deviceID == 0xC8)
		number = SII9234_i2c_read(client_ptr,offset);

		if (number < 0)
	{
		printk("I2C_ReadByte: Device ID=0x%X, Err ret = %d \n", deviceID, number);
	}

    return (number);

}

byte ReadByteTPI (byte Offset) 
{
	EXIT_ON_CABLE_DISCONNECTION;
	return I2C_ReadByte(SA_TX_Page0_Primary, Offset);
}

void WriteByteTPI (byte Offset, byte Data) 
{
	EXIT_ON_CABLE_DISCONNECTION_V;
	I2C_WriteByte(SA_TX_Page0_Primary, Offset, Data);
}



void ReadModifyWriteTPI(byte Offset, byte Mask, byte Data) 
{

	byte Temp;
	EXIT_ON_CABLE_DISCONNECTION_V;
	Temp = ReadByteTPI(Offset);		// Read the current value of the register.
	Temp &= ~Mask;					// Clear the bits that are set in Mask.
	Temp |= (Data & Mask);			// OR in new value. Apply Mask to Value for safety.
	WriteByteTPI(Offset, Temp);		// Write new value back to register.
}

byte ReadByteCBUS (byte Offset) 
{
	EXIT_ON_CABLE_DISCONNECTION;
	return I2C_ReadByte(SA_TX_CBUS_Primary, Offset);
}

void WriteByteCBUS(byte Offset, byte Data) 
{
	EXIT_ON_CABLE_DISCONNECTION_V;
	I2C_WriteByte(SA_TX_CBUS_Primary, Offset, Data);
}

void ReadModifyWriteCBUS(byte Offset, byte Mask, byte Value) 
{
  byte Temp;
  EXIT_ON_CABLE_DISCONNECTION_V;
  Temp = ReadByteCBUS(Offset);
  Temp &= ~Mask;
  Temp |= (Value & Mask);
  WriteByteCBUS(Offset, Temp);
}


//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	ReadIndexedRegister ()
//
// PURPOSE		:	Read the value from an indexed register.
//
//					Write:
//						1. 0xBC => Indexed page num
//						2. 0xBD => Indexed register offset
//
//					Read:
//						3. 0xBE => Returns the indexed register value
//
// INPUT PARAMS	:	PageNum	-	indexed page number
//					Offset	-	offset of the register within the indexed page.
//
// OUTPUT PARAMS:	None
//
// GLOBALS USED	:	None
//
// RETURNS		:	The value read from the indexed register.
//
//////////////////////////////////////////////////////////////////////////////

byte ReadIndexedRegister (byte PageNum, byte Offset) 
{
	EXIT_ON_CABLE_DISCONNECTION;
	WriteByteTPI(TPI_INDEXED_PAGE_REG, PageNum);		// Indexed page
	WriteByteTPI(TPI_INDEXED_OFFSET_REG, Offset);		// Indexed register
	return ReadByteTPI(TPI_INDEXED_VALUE_REG);			// Return read value
}


//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	WriteIndexedRegister ()
//
// PURPOSE		:	Write a value to an indexed register
//
//					Write:
//						1. 0xBC => Indexed page num
//						2. 0xBD => Indexed register offset
//						3. 0xBE => Set the indexed register value
//
// INPUT PARAMS	:	PageNum	-	indexed page number
//					Offset	-	offset of the register within the indexed page.
//					Data	-	the value to be written.
//
// OUTPUT PARAMS:	None
//
// GLOBALS USED :	None
//
// RETURNS		:	None
//
//////////////////////////////////////////////////////////////////////////////

void WriteIndexedRegister (byte PageNum, byte Offset, byte Data) 
{
	EXIT_ON_CABLE_DISCONNECTION_V;
	WriteByteTPI(TPI_INDEXED_PAGE_REG, PageNum);		// Indexed page
	WriteByteTPI(TPI_INDEXED_OFFSET_REG, Offset);		// Indexed register
	WriteByteTPI(TPI_INDEXED_VALUE_REG, Data);			// Write value
}


//////////////////////////////////////////////////////////////////////////////
//
// FUNCTION		:	ReadModifyWriteIndexedRegister ()
//
// PURPOSE		:	Set or clear individual bits in a TPI register.
//
// INPUT PARAMS	:	PageNum	-	indexed page number
//					Offset	-	the offset of the indexed register to be modified.
//					Mask	-	"1" for each indexed register bit that needs to be
//								modified
//					Data	-	The desired value for the register bits in their
//								proper positions
//
// OUTPUT PARAMS:	None
//
// GLOBALS USED	:	None
//
// RETURNS		:	void
//
//////////////////////////////////////////////////////////////////////////////

void ReadModifyWriteIndexedRegister (byte PageNum, byte Offset, byte Mask, byte Data) 
{

	byte Temp;
	EXIT_ON_CABLE_DISCONNECTION_V;
	Temp = ReadIndexedRegister (PageNum, Offset);	// Read the current value of the register.
	Temp &= ~Mask;									// Clear the bits that are set in Mask.
	Temp |= (Data & Mask);							// OR in new value. Apply Mask to Value for safety.
	WriteByteTPI(TPI_INDEXED_VALUE_REG, Temp);		// Write new value back to register.
}

