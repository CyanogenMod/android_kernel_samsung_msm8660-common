/***************************************************************************
*
* file     si_regioCbus.c
* brief    CBUS register I/O function wrappers.
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

#include <linux/types.h>

#include "SiI9234_I2C_master.h"
#include "SiI9234_I2C_slave_add.h"


static byte l_cbusPortOffsets [ MHD_MAX_CHANNELS ] = { 0x00 };

//------------------------------------------------------------------------------
// Function:    SiIRegioCbusRead
// Description: Read a one byte CBUS register with port offset.
//              The register address parameter is translated into an I2C slave
//              address and offset. The I2C slave address and offset are used
//              to perform an I2C read operation.
//------------------------------------------------------------------------------


byte SiIRegioCbusRead ( word regAddr, byte channel )
{
    return(I2C_ReadByte(SA_TX_CBUS_Primary + l_cbusPortOffsets[channel], regAddr));
}

//------------------------------------------------------------------------------
// Function:    SiIRegioCbusWrite
// Description: Write a one byte CBUS register with port offset.
//              The register address parameter is translated into an I2C
//              slave address and offset. The I2C slave address and offset
//              are used to perform an I2C write operation.
//------------------------------------------------------------------------------

void SiIRegioCbusWrite ( word regAddr, byte channel, byte value )
{

    I2C_WriteByte(SA_TX_CBUS_Primary + l_cbusPortOffsets[channel], regAddr, value);
}

