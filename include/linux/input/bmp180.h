/*
 * Copyright (C) 2011 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef _BMP180_H
#define _BMP180_H

/* Register definitions */
#define BMP180_TAKE_MEAS_REG		0xf4
#define BMP180_READ_MEAS_REG_U		0xf6
#define BMP180_READ_MEAS_REG_L		0xf7
#define BMP180_READ_MEAS_REG_XL		0xf8

/*
 * Bytes defined by the spec to take measurements
 * Temperature will take 4.5ms before EOC
 */
#define BMP180_MEAS_TEMP		0x2e
/* 4.5ms wait for measurement */
#define BMP180_MEAS_PRESS_OVERSAMP_0	0x34
/* 7.5ms wait for measurement */
#define BMP180_MEAS_PRESS_OVERSAMP_1	0x74
/* 13.5ms wait for measurement */
#define BMP180_MEAS_PRESS_OVERSAMP_2	0xb4
/* 25.5ms wait for measurement */
#define BMP180_MEAS_PRESS_OVERSAMP_3	0xf4

/*
 * EEPROM registers each is a two byte value so there is
 * an upper byte and a lower byte
 */
#define BMP180_EEPROM_AC1_U	0xaa
#define BMP180_EEPROM_AC1_L	0xab
#define BMP180_EEPROM_AC2_U	0xac
#define BMP180_EEPROM_AC2_L	0xad
#define BMP180_EEPROM_AC3_U	0xae
#define BMP180_EEPROM_AC3_L	0xaf
#define BMP180_EEPROM_AC4_U	0xb0
#define BMP180_EEPROM_AC4_L	0xb1
#define BMP180_EEPROM_AC5_U	0xb2
#define BMP180_EEPROM_AC5_L	0xb3
#define BMP180_EEPROM_AC6_U	0xb4
#define BMP180_EEPROM_AC6_L	0xb5
#define BMP180_EEPROM_B1_U	0xb6
#define BMP180_EEPROM_B1_L	0xb7
#define BMP180_EEPROM_B2_U	0xb8
#define BMP180_EEPROM_B2_L	0xb9
#define BMP180_EEPROM_MB_U	0xba
#define BMP180_EEPROM_MB_L	0xbb
#define BMP180_EEPROM_MC_U	0xbc
#define BMP180_EEPROM_MC_L	0xbd
#define BMP180_EEPROM_MD_U	0xbe
#define BMP180_EEPROM_MD_L	0xbf

#define I2C_TRIES		5
#define AUTO_INCREMENT		0x80

#define DELAY_LOWBOUND		(50 * NSEC_PER_MSEC)
#define DELAY_UPBOUND		(500 * NSEC_PER_MSEC)
#define DELAY_DEFAULT		(200 * NSEC_PER_MSEC)

#define PRESSURE_MAX		125000
#define PRESSURE_MIN		95000
#define PRESSURE_FUZZ		5
#define PRESSURE_FLAT		5

#define FACTORY_TEST
#ifdef FACTORY_TEST
#define TEMP_MAX		3000
#define TEMP_MIN		-3000
#define SEA_LEVEL_MAX		999999
#define SEA_LEVEL_MIN		-1
#endif

struct bmp_i2c_platform_data {
	void (*power_on) (int);
};
extern int sensors_register(struct device *dev, void * drvdata,
	struct device_attribute *attributes[], char *name);

#endif /* _AS5011_H */
