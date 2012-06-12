/*
 * include/linux/sec_param.h
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

typedef struct _sec_param_data {
	unsigned int debuglevel;
	unsigned int uartsel;
	unsigned int rory_control;
} sec_param_data;

typedef enum
{
	param_index_debuglevel,
	param_index_uartsel,
	param_rory_control,
} sec_param_index;

extern bool sec_open_param(void);
extern bool sec_get_param(sec_param_index index, void *value);
extern bool sec_set_param(sec_param_index index, void *value);

