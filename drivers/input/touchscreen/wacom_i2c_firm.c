/*
 *  wacom_i2c_firm.c - Wacom G5 Digitizer Controller (I2C bus)
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

const unsigned int Binary_nLength = 0x7FFF;
const unsigned char Mpu_type = 0x26;
unsigned int Firmware_version_of_file=0;
const unsigned int Firmware_version_of_file_48 = 0x20A;
unsigned char * Binary;


// 4.8 inch
#include "wacom_i2c_firm_P6_REV02.h"


// 4.4 inch
#if defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined(CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
#include "wacom_i2c_firm_Q1_KOR4P4_0370.h"
const unsigned int Firmware_version_of_file_44 = 0x370;
//const char Firmware_checksum[]={0x1F, 0xE1, 0x57, 0x54, 0x11,};   /* checksum for 0x331 */
//const char Firmware_checksum[]={0x1F, 0x7A, 0xBD, 0x7B, 0x8D,};   /* checksum for 0x332 */
//const char Firmware_checksum[]={0x1F, 0x7A, 0xBD, 0x7B, 0x8D,};   /* checksum for 0x31D */
//const char Firmware_checksum[]={0x1F, 0x1F, 0x1E, 0x87, 0xE2,};   /* checksum for 0x334 */
//const char Firmware_checksum[]={0x1F, 0x0F, 0x4C, 0xA8, 0x63,};   /* checksum for 0x335 */
//const char Firmware_checksum[]={0x1F, 0x7C, 0xB9, 0x9D, 0x94,};   /* checksum for 0x6335 */
//const char Firmware_checksum[]={0x1F, 0x7F, 0x4E, 0xE5, 0x78,};   /* checksum for 0x339 */
//const char Firmware_checksum[]={0x1F, 0x55, 0x75, 0x49, 0x8D,};   /* checksum for 0x361 */
//const char Firmware_checksum[]={0x1F, 0x59, 0x7F, 0x7C, 0xD1,};   /* checksum for 0x362 */
//const char Firmware_checksum[]={0x1F, 0x80, 0x91, 0x6E, 0x2A,};   /* checksum for 0x364 */
//const char Firmware_checksum[]={0x1F, 0xA5, 0x9D, 0x6E, 0x2A,};   /* checksum for 0x369 */
const char Firmware_checksum[]={0x1F, 0x44, 0x75, 0x49, 0x8D,};   /* checksum for 0x370 */

#elif defined(CONFIG_USA_MODEL_SGH_I717)
#include "wacom_i2c_firm_P6_REV05.h"
const unsigned int Firmware_version_of_file_44 = 0x404;
const char Firmware_checksum[]={0x1F, 0xB5, 0x41, 0xB7, 0x7F,};/* checksum for 0x404 */
//const char Firmware_checksum[]={0x1F, 0xC9, 0x13, 0x06, 0x84,};/* checksum for 0x346 */

#else
#include "wacom_i2c_firm_P6_REV03.h"
const unsigned int Firmware_version_of_file_44 = 0x340;
/* checksum for 0x340 */
const char Firmware_checksum[]={0x1F, 0xee, 0x06, 0x4b, 0xdd,};
#endif
