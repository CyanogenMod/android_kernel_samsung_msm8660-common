/*
 * Bluetooth Broadcom GPIO and Low Power Mode control
 *
 *  Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *  Copyright (C) 2011 Google, Inc.
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>
#include <linux/wakelock.h>
#include <asm/mach-types.h>

#include "devices.h"

#define BT_UART_CFG
#define BT_LPM_ENABLE

#define GPIO_BT_WAKE        86
#define GPIO_BT_HOST_WAKE   127
#define GPIO_BT_REG_ON      158	
#define GPIO_BT_RESET       46

#define GPIO_BT_UART_RTS    56
#define GPIO_BT_UART_CTS    55
#define GPIO_BT_UART_RXD    54
#define GPIO_BT_UART_TXD    53

#define GPIO_BT_PCM_DOUT    111
#define GPIO_BT_PCM_DIN     112
#define GPIO_BT_PCM_SYNC    113
#define GPIO_BT_PCM_CLK     114


static struct rfkill *bt_rfkill;

#ifdef BT_UART_CFG
static unsigned bt_uart_on_table[] = {
#if defined (CONFIG_USA_MODEL_SGH_I757)
         GPIO_CFG(GPIO_BT_RESET,     0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_REG_ON,    0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_WAKE,      0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_UART_RTS,  1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_UART_CTS,  1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_UART_RXD,  1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_UART_TXD,  1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_PCM_DOUT,  1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_PCM_DIN,   1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 
         GPIO_CFG(GPIO_BT_PCM_SYNC,  1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_PCM_CLK,   1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 
         GPIO_CFG(GPIO_BT_HOST_WAKE, 0, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),  
#else
	 GPIO_CFG(GPIO_BT_RESET,     0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_REG_ON,    0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_WAKE,      0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_UART_RTS,  1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_UART_CTS,  1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_UART_RXD,  1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_UART_TXD,  1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_PCM_DOUT,  1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_PCM_DIN,   1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 
         GPIO_CFG(GPIO_BT_PCM_SYNC,  1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
         GPIO_CFG(GPIO_BT_PCM_CLK,   1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 
         GPIO_CFG(GPIO_BT_HOST_WAKE, 0, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
#endif
};

static unsigned bt_uart_off_table[] = {
	#if defined (CONFIG_USA_MODEL_SGH_I757)
		GPIO_CFG(GPIO_BT_UART_RTS, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
	 	GPIO_CFG(GPIO_BT_UART_CTS, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
	 	GPIO_CFG(GPIO_BT_UART_RXD, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
	 	GPIO_CFG(GPIO_BT_UART_TXD, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
		GPIO_CFG(GPIO_BT_RESET,    0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG(GPIO_BT_REG_ON,   0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG(GPIO_BT_WAKE,     0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), 
		GPIO_CFG(GPIO_BT_PCM_DOUT, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG(GPIO_BT_PCM_DIN,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 
		GPIO_CFG(GPIO_BT_PCM_SYNC, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG(GPIO_BT_PCM_CLK,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG(GPIO_BT_HOST_WAKE,0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	 #else
	 
		GPIO_CFG(GPIO_BT_UART_RTS, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG(GPIO_BT_UART_CTS, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG(GPIO_BT_UART_RXD, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG(GPIO_BT_UART_TXD, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG(GPIO_BT_RESET,    0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG(GPIO_BT_REG_ON,   0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG(GPIO_BT_WAKE,     0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 
		GPIO_CFG(GPIO_BT_PCM_DOUT, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG(GPIO_BT_PCM_DIN,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 
		GPIO_CFG(GPIO_BT_PCM_SYNC, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG(GPIO_BT_PCM_CLK,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		GPIO_CFG(GPIO_BT_HOST_WAKE,0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 
	#endif


};
#endif

static void bt_config_gpio_table_generic(uint32_t *table, int len)
{
	int n, rc;
	for (n = 0; n < len; n++) {
		rc = gpio_tlmm_config(table[n], GPIO_CFG_ENABLE);
		if (rc) {
			printk(KERN_NOTICE "[BT] %s: gpio_tlmm_config(%#x)=%d\n",__func__, table[n], rc);
			break;
		}
	}
}


static int bcm4330_bt_rfkill_set_power(void *data, bool blocked)
{
	/* rfkill_ops callback. Turn transmitter on when blocked is false */
	int pin, rc = 0;

	if (!blocked) {
		printk(KERN_ERR "[BT] Bluetooth Power On.\n");
#ifdef BT_UART_CFG
    	for (pin = 0; pin < ARRAY_SIZE(bt_uart_on_table); pin++) {
            	rc = gpio_tlmm_config(bt_uart_on_table[pin], GPIO_CFG_ENABLE);
    
    		if(rc < 0)
            	printk(KERN_ERR "[BT] %s: gpio_tlmm_config(%#x)=%d\n", __func__, bt_uart_on_table[pin], rc);
    	}
#endif

		rc=gpio_direction_output(GPIO_BT_WAKE, 1);
		if (rc) 
			printk(KERN_NOTICE "[BT] %s: failed to gpio %d: %d\n", __func__, GPIO_BT_WAKE, rc);
		else
			printk(KERN_NOTICE "[BT] %s: success to gpio %d: %d\n", __func__, GPIO_BT_WAKE, rc);

		rc = gpio_request(GPIO_BT_REG_ON, NULL);
		if (!rc) {
			rc=gpio_direction_output(GPIO_BT_REG_ON, 1);
			if (rc) 
				printk(KERN_NOTICE "[BT] %s: failed to gpio %d: %d\n", __func__, GPIO_BT_REG_ON, rc);
			else
				printk(KERN_NOTICE "[BT] %s: success to gpio %d: %d\n", __func__, GPIO_BT_REG_ON, rc);
			gpio_free(GPIO_BT_REG_ON);
		}
		else {
			printk(KERN_NOTICE "[BT] %s: failed to gpio %d: %d\n", __func__, GPIO_BT_REG_ON, rc);
		}

		msleep(50);
		
		rc = gpio_request(GPIO_BT_RESET, NULL);
		if (!rc) {
			rc=gpio_direction_output(GPIO_BT_RESET, 1);
			if (rc) 
				printk(KERN_NOTICE "[BT] %s: failed to gpio %d: %d\n", __func__, GPIO_BT_RESET, rc);
			else
				printk(KERN_NOTICE "[BT] %s: success to gpio %d: %d\n", __func__, GPIO_BT_RESET, rc);				
			gpio_free(GPIO_BT_RESET);
		}else{
          	printk(KERN_ERR "[BT] %s: failed to gpio %d: %d\n", __func__, GPIO_BT_RESET, rc);
		}

		printk(KERN_ERR "[BT] %s: check potint 1\n ", __func__);				
			
	} else {
#ifdef BT_UART_CFG
		for (pin = 0; pin < ARRAY_SIZE(bt_uart_off_table); pin++) {
			rc = gpio_tlmm_config(bt_uart_off_table[pin], GPIO_CFG_ENABLE);
			
            if(rc < 0)
     		printk(KERN_ERR "[BT] %s: gpio_tlmm_config(%#x)=%d\n",
				       __func__, bt_uart_off_table[pin], rc);
		}
#endif
		printk(KERN_ERR "[BT] Bluetooth Power Off.\n");
		gpio_set_value(GPIO_BT_RESET,  0);
		gpio_set_value(GPIO_BT_REG_ON, 0);
	}

    printk(KERN_ERR "[BT] %s: check potint 2 \n ", __func__);	
	return 0;
}

static const struct rfkill_ops bcm4330_bt_rfkill_ops = {
	.set_block = bcm4330_bt_rfkill_set_power,
};

static int bcm4330_bluetooth_probe(struct platform_device *pdev)
{
	int rc = 0;

	printk(KERN_ERR "[BT] bcm4330_bluetooth_probe \n");

#if 0
	rc = gpio_request(GPIO_BT_REG_ON, "bcm4330_bt_regon_gpio");
	
	if (unlikely(rc)) {
		printk(KERN_ERR "[BT] GPIO_BT_EN request failed.\n");
		return rc;
	}

	gpio_tlmm_config(GPIO_CFG(GPIO_BT_UART_RTS, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(GPIO_BT_UART_CTS, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(GPIO_BT_UART_RXD, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(GPIO_BT_UART_TXD, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	gpio_tlmm_config(GPIO_CFG(GPIO_BT_REG_ON, 0, GPIO_CFG_INPUT,
		GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	rc=gpio_direction_output(GPIO_BT_WAKE, 1);
	rc=gpio_direction_output(GPIO_BT_REG_ON, 0);	

	if(rc < 0){
		printk(KERN_ERR "[BT] bcm4330_bluetooth_probe error ");
		gpio_free(GPIO_BT_WAKE);
		//gpio_free(GPIO_BT_REG_ON);
		return -1;
	}else
#endif
	{
    	bt_rfkill = rfkill_alloc("bcm4330 Bluetooth", &pdev->dev,
    				RFKILL_TYPE_BLUETOOTH, &bcm4330_bt_rfkill_ops,
    				NULL);

	
    	if (unlikely(!bt_rfkill)) {
    		printk(KERN_ERR "[BT] bt_rfkill alloc failed.\n");
    		gpio_free(GPIO_BT_REG_ON);
    		return -ENOMEM;
    	}

    	rfkill_init_sw_state(bt_rfkill, 0);
    
    	rc = rfkill_register(bt_rfkill);
    
    	if (unlikely(rc)) {
    		printk(KERN_ERR "[BT] bt_rfkill register failed.\n");
    		rfkill_destroy(bt_rfkill);
    		gpio_free(GPIO_BT_REG_ON);
    		return -1;
    	}
    	rfkill_set_sw_state(bt_rfkill, true);
	}
	return rc;
}

static int bcm4330_bluetooth_remove(struct platform_device *pdev)
{
    printk(KERN_ERR "[BT] bcm4330_bluetooth_remove \n");
	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);

	gpio_free(GPIO_BT_REG_ON);

	return 0;
}

static struct platform_driver bcm4330_bluetooth_platform_driver = {
	.probe = bcm4330_bluetooth_probe,
	.remove = bcm4330_bluetooth_remove,
	.driver = {
		   .name = "bcm4330_bluetooth",
		   .owner = THIS_MODULE,
		   },
};

#ifdef BT_LPM_ENABLE
static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
        .start  = GPIO_BT_HOST_WAKE,
        .end    = GPIO_BT_HOST_WAKE,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
        .start  = GPIO_BT_WAKE,
        .end    = GPIO_BT_WAKE,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake",
        .start  = MSM_GPIO_TO_INT(GPIO_BT_HOST_WAKE),
        .end    = MSM_GPIO_TO_INT(GPIO_BT_HOST_WAKE),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device msm_bluesleep_device = {
	.name   = "bluesleep_bcm",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};

static void gpio_rev_init(void)
{
    bt_config_gpio_table_generic(bt_uart_off_table, ARRAY_SIZE(bt_uart_off_table));
}
#endif

#ifdef BT_LPM_ENABLE
extern void bluesleep_setup_uart_port(struct platform_device *uart_dev);
#endif

static int __init bcm4330_bluetooth_init(void)
{
#ifdef BT_LPM_ENABLE
	gpio_rev_init();
    printk(KERN_ERR "[BT] bcm4330_bluetooth_init \n");
	platform_device_register(&msm_bluesleep_device);
	bluesleep_setup_uart_port(&msm_device_uart_dm1);
#endif
	return platform_driver_register(&bcm4330_bluetooth_platform_driver);
}

static void __exit bcm4330_bluetooth_exit(void)
{
    printk(KERN_ERR "[BT] bcm4330_bluetooth_exit \n");
#ifdef BT_LPM_ENABLE
	platform_device_unregister(&msm_bluesleep_device);
#endif
	platform_driver_unregister(&bcm4330_bluetooth_platform_driver);
}

module_init(bcm4330_bluetooth_init);
module_exit(bcm4330_bluetooth_exit);

MODULE_ALIAS("platform:bcm4330");
MODULE_DESCRIPTION("bcm4330_bluetooth");
MODULE_LICENSE("GPL");

