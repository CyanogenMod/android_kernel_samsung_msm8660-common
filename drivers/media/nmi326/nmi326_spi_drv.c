/* linux/drivers/media/nmi326/nmi326_spi_drv.c
 *
 * Driver file for NMI326 SPI Interface
 *
 *  Copyright (c) 2010 Samsung Electronics
 * 	http://www.samsungsemi.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
 
#include <linux/version.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/page.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <mach/gpio.h>
#include <asm/mach/map.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>  
//#include <plat/gpio.h>
//#include <plat/mux.h>

#include "nmi326_spi_drv.h"
#include "nmi326.h"



static struct spi_device *nmi326_spi;

/*This should be done in platform*/

int nmi326_spi_read(u8 *buf, size_t len)
{

	struct spi_message msg;
	struct spi_transfer	transfer[2];
	unsigned char status = 0;
	int r_len;

	memset(&msg, 0, sizeof(msg));
	memset(transfer, 0, sizeof(transfer));

	spi_message_init(&msg);
	msg.spi = nmi326_spi;

	transfer[0].tx_buf = (unsigned char *)NULL;
	transfer[0].rx_buf = (unsigned char *)buf;
	transfer[0].len = len;
	transfer[0].bits_per_word = 8;
	transfer[0].delay_usecs = 0;

	spi_message_add_tail(&(transfer[0]), &msg);
	status = spi_sync(nmi326_spi, &msg);

    if (status==0)
	{
		//r_len = msg.actual_length;
		r_len = len;
	}
	else
	{
		r_len =status;
	}
		
	
	return r_len; 
}

int nmi326_spi_write(u8 *buf, size_t len)
{

	struct spi_message msg;
	struct spi_transfer	transfer[2];
	unsigned char status = 0;
	int w_len;

	memset(&msg, 0, sizeof(msg));
	memset(transfer, 0, sizeof(transfer));

	spi_message_init(&msg);
	msg.spi = nmi326_spi;

	transfer[0].tx_buf = (unsigned char *)buf;
	transfer[0].rx_buf = (unsigned char *)NULL;
	transfer[0].len = len;
	transfer[0].bits_per_word = 8;
	transfer[0].delay_usecs = 0;

	spi_message_add_tail(&(transfer[0]), &msg);
	status = spi_sync(nmi326_spi, &msg);

    if (status==0)
		w_len = len;
	else
		w_len =status;
	
	return w_len; 
}

#if defined(NMI326_HW_CHIP_ID_CHECK)
void nmi326_spi_read_chip_id(void)
{
	unsigned char b[20];
	unsigned long adr = 0x6400;
	int retry;
	size_t len;
	unsigned char sta = 0;
	unsigned long val = 0;
	int ret_size;

	b[0] = 0x70;								/* word access */
	b[1] = 0x00;
	b[2] = 0x04;
	b[3] = (uint8_t)(adr >> 16);
	b[4] = (uint8_t)(adr >> 8);
	b[5] = (uint8_t)(adr);
	len = 6;

	//chip.inp.hlp.write(0x61, b, len);
	ret_size = nmi326_spi_write(b,len);
	spi_dbg("chip id ret_size 1= %d\n",ret_size);
	/**
		Wait for complete
	**/
	retry = 1000;
	do {
		//chip.inp.hlp.read(0x61, &sta, 1);
		spi_dbg("chip id retry = %d ",retry);

		ret_size = nmi326_spi_read(&sta,1);
		if (sta == 0xff)
			break;
		//nmi_delay(1);
	}while (retry--);

	spi_dbg("\nchip id ret_size 2= %d\n",ret_size);
	spi_dbg("chip id sta = %d\n",sta);

	if (sta== 0xff) {
		/**
			Read the Count
		**/
		//chip.inp.hlp.read(0x61, b, 3);
		nmi326_spi_read(b,3);
		len = b[2] | (b[1] << 8) | (b[0] << 16);
		if (len == 4) {
			//chip.inp.hlp.read(0x61, (uint8_t *)&val, 4);
			nmi326_spi_read((unsigned char*)&val,4);
			b[0] = 0xff;
			//chip.inp.hlp.write(0x61, b, 1);
			nmi326_spi_write(b,1);

			spi_dbg("===============================\n",val);
			spi_dbg("NMI326 Chip ID = [0x%x] on SPI\n",val);
			spi_dbg("===============================\n",val);
		} else {
			spi_dbg("Error, SPI bus, bad count (%d)\n", len);
		}
	} else {
		spi_dbg("Error, SPI bus, not complete\n");
	}
}
#endif


static int  nmi326_spi_probe(struct spi_device *spi)
{
	int retval = 0;

	spi_dbg(" **** nmi326_spi_probe.\n");
	nmi326_spi = spi;
	nmi326_spi->mode = (SPI_MODE_0) ;
	nmi326_spi->bits_per_word = 8 ;

	retval = spi_setup( nmi326_spi );
	if( retval != 0 ) {
		spi_dbg( "ERROR : %d\n", retval );
	}
	else {
		spi_dbg( "Done : %d\n", retval );
	}
	
	return 0;
}

static int nmi326_spi_remove(struct spi_device *spi)
{
	return 0;
}

static struct spi_driver nmi326_spi_driver = {
	.driver = {
		.name	= "isdbtspi",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
	.probe		= nmi326_spi_probe,
	.remove		= __devexit_p(nmi326_spi_remove),
};


static const char banner[] __initdata =  "NMI326 SPI Driver Version 1.0\n";

int nmi326_spi_init( void )
{
	int retval = 0;
	
	spi_dbg("%s",banner);

	retval = spi_register_driver( &nmi326_spi_driver );
	if( retval < 0 ) {
		spi_dbg( "spi_register_driver ERROR : %d\n", retval );

		goto exit;
	}

	spi_dbg( "(%d) init Done.\n",  __LINE__ );

	return 0;

exit :
	return retval;
}

void nmi326_spi_exit( void )
{
	printk( "[%s]\n", __func__ );
	
	spi_unregister_driver( &nmi326_spi_driver );
}


