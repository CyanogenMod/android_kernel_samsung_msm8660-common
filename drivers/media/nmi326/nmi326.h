/* linux/drivers/media/nmi326/nmi326.h
*
* Header file for NMI326 oneseg device
*
*  Copyright (c) 2012 Samsung Electronics
 
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
 */
 

#ifndef __NMI_HW_H__
#define __NMI_HW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/poll.h>  
#include <linux/sched.h>
#include <mach/isdbt_pdata.h>

#define NM_DEBUG_ON

#ifdef NM_DEBUG_ON
#define NM_KMSG(x...) printk(KERN_DEBUG ">ISDBT< " x)
#else
#define NM_KMSG(x...) /* null */
#endif

#define ISDBT_DEV_NAME		"isdbt"
#define ISDBT_DEV_MAJOR	225
#define ISDBT_DEV_MINOR	0

#define DTV_IRQ_DEINIT			0
#define DTV_IRQ_INIT			1
#define DTV_IRQ_SET				2

typedef struct {
	long		index;
	void		**hInit;
	void		*hI2C;
	int			irq_status;
	spinlock_t	isr_lock;

	struct fasync_struct *async_queue;

	wait_queue_head_t isdbt_waitqueue;

	unsigned char* rwBuf;
	
} ISDBT_OPEN_INFO_T;

#define MAX_OPEN_NUM		8

#define IOCTL_MAGIC	't'
//#define NMI326_IOCTL_BUF_SIZE	256

#define IOCTL_MAXNR				8// !!!!!!!!!!!!!! TO DO

#define IOCTL_ISDBT_POWER_ON		_IO( IOCTL_MAGIC, 0 )
#define IOCTL_ISDBT_POWER_OFF		_IO( IOCTL_MAGIC, 1 )

#define IOCTL_ISDBT_INTERRUPT_REGISTER		_IO(IOCTL_MAGIC, 2)
#define IOCTL_ISDBT_INTERRUPT_UNREGISTER	_IO(IOCTL_MAGIC, 3)
#define IOCTL_ISDBT_INTERRUPT_ENABLE		_IO(IOCTL_MAGIC, 4)
#define IOCTL_ISDBT_INTERRUPT_DISABLE		_IO(IOCTL_MAGIC, 5)
#define IOCTL_ISDBT_INTERRUPT_DONE		_IO(IOCTL_MAGIC, 6)
#define IOCTL_ISDBT_INTERRUPT_HANDLER_START	_IO(IOCTL_MAGIC, 7)

#ifdef __cplusplus
}
#endif

#endif // __NMI_HW_H__

