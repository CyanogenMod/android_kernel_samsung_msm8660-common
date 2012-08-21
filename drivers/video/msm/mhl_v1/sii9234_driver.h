/******************************************************************************
*
*                        SiI9234_DRIVER.H
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
2010/11/06    Daniel Lee(Philju)      Initial version of file, SIMG Korea 
===========================================================================*/

/*===========================================================================
                     INCLUDE FILES FOR MODULE
===========================================================================*/


/*===========================================================================
                   FUNCTION DEFINITIONS
===========================================================================*/


void SiI9234_interrupt_event(void);
bool SiI9234_init(void);
//Disabling //NAGSM_Android_SEL_Kernel_Aakash_20101206
/*extern byte GetCbusRcpData(void);
extern void ResetCbusRcpData(void);*/


//MHL IOCTL INTERFACE
#define MHL_READ_RCP_DATA 0x1


