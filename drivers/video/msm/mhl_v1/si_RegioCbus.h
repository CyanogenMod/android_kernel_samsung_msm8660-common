/***************************************************************************
*
* file     si_RegioCbus.h
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

#ifndef __SI_REGIO_H__
#define __SI_REGIO_H__

#include <linux/types.h>

#include "Common_Def.h"



byte SiIRegioCbusRead ( word regAddr, byte channel );
void SiIRegioCbusWrite ( word regAddr, byte channel, byte value );

#endif // __SI_REGIO_H__

