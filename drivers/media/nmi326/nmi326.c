/*****************************************************************************
 Copyright(c) 2010 NMI Inc. All Rights Reserved
 
 File name : nmi_hw.c
 
 Description : NM326 host interface
 
 History : 
 ----------------------------------------------------------------------
 2011/11/10 	ssw		initial
*******************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/fcntl.h>
//#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/irq.h>
//#include <asm/irq.h>
#include <asm/mach/irq.h>

#include <linux/wait.h>
#include <linux/stat.h>
#include <linux/ioctl.h>
#include <linux/delay.h>
#include <linux/slab.h>
//#include <plat/gpio.h>
//#include <plat/mux.h>

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/vmalloc.h>

#include <linux/io.h>
#include <mach/board.h>
#include <mach/gpio.h>

#include "nmi326.h"
#include "nmi326_spi_drv.h"


static struct class *isdbt_class;
static struct isdbt_platform_data gpio_cfg;
//static struct tdmb_platform_data gpio_cfg;
static char g_bCatchIrq = 0;

#define SPI_RW_BUF		(188*50*2)

static irqreturn_t isdbt_irq_handler(int irq, void *dev_id)
{
	unsigned long flags;

	ISDBT_OPEN_INFO_T *pdev = (ISDBT_OPEN_INFO_T *)dev_id;

	spin_lock_irqsave(&pdev->isr_lock, flags);
	
	disable_irq_nosync(gpio_cfg.irq);
	pdev->irq_status = DTV_IRQ_INIT;	
	g_bCatchIrq = 1;
	wake_up(&(pdev->isdbt_waitqueue));
	NM_KMSG("<isdbt> isr: wake up\n");
	
	spin_unlock_irqrestore(&pdev->isr_lock, flags);

	return IRQ_HANDLED;
}

static unsigned int	isdbt_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	ISDBT_OPEN_INFO_T* pdev = (ISDBT_OPEN_INFO_T*)(filp->private_data);

	NM_KMSG("<isdbt> poll\n");
	poll_wait(filp, &(pdev->isdbt_waitqueue), wait);

	if ( g_bCatchIrq == 1)
	{
		mask |= (POLLIN | POLLRDNORM);
		NM_KMSG("<isdbt> poll: release (%d)\n", g_bCatchIrq);
		g_bCatchIrq = 0;
	} else {
		NM_KMSG("<isdbt> poll: release (%d)\n", g_bCatchIrq);
	}

	return mask;
}

static int isdbt_open(struct inode *inode, struct file *filp)
{
	ISDBT_OPEN_INFO_T *pdev = NULL;

	NM_KMSG("<isdbt> isdbt open\n");
	
	pdev = (ISDBT_OPEN_INFO_T *)kmalloc(sizeof(ISDBT_OPEN_INFO_T), GFP_KERNEL);
	if(pdev == NULL)
	{
		NM_KMSG("<isdbt> pdev kmalloc FAIL\n");
		return -1;
	}

	NM_KMSG("<isdbt> pdev kmalloc SUCCESS\n");
	
	memset(pdev, 0x00, sizeof(ISDBT_OPEN_INFO_T));
	pdev->irq_status = DTV_IRQ_DEINIT;

	filp->private_data = pdev;

	pdev->rwBuf = kmalloc(SPI_RW_BUF, GFP_KERNEL);

	if(pdev->rwBuf == NULL)
	{
		NM_KMSG("<isdbt> pdev->rwBuf kmalloc FAIL\n");
		return -1;
	}

	spin_lock_init(&pdev->isr_lock);
	init_waitqueue_head(&(pdev->isdbt_waitqueue));
	
	return 0;
}

static int isdbt_release(struct inode *inode, struct file *filp)
{
	ISDBT_OPEN_INFO_T *pdev = (ISDBT_OPEN_INFO_T*)(filp->private_data);

	NM_KMSG("<isdbt> isdbt release \n");

	kfree(pdev->rwBuf);
	kfree((void*)pdev);	

	return 0;
}

static int isdbt_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	int rv = 0;
	ISDBT_OPEN_INFO_T *pdev = (ISDBT_OPEN_INFO_T *)(filp->private_data);

	rv = nmi326_spi_read(pdev->rwBuf, count);
	if (rv < 0) {
		NM_KMSG("isdbt_read() : nmi326_spi_read failed(rv:%d)\n", rv); 
		return rv;
	}

	//NM_KMSG("[R] count (%d), rv (%d)\n", count, rv);
	
	/* move data from kernel area to user area */
	if( copy_to_user (buf, pdev->rwBuf, count) < 0)
	{
		NM_KMSG("isdbt_read() : put_user() error(rv:%d)!\n"); 
		return -1;
	}
	return rv;
}

static int isdbt_write(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	int rv = 0;
	ISDBT_OPEN_INFO_T* pdev = (ISDBT_OPEN_INFO_T*)(filp->private_data);

	/* move data from user area to kernel  area */
	if(copy_from_user(pdev->rwBuf, buf, count) < 0)
	{
		NM_KMSG("isdbt_write() : get_user() error(rv:%d)!\n"); 
		return -1;
	}

	/* write data to SPI Controller */
	rv = nmi326_spi_write(pdev->rwBuf, count);
	if (rv < 0) {
		NM_KMSG("isdbt_write() : nmi326_spi_write failed(rv:%d)\n", rv); 
		return rv;
	}

	return rv;
}

static int isdbt_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	long res = 1;
	ISDBT_OPEN_INFO_T* pdev = (ISDBT_OPEN_INFO_T*)(filp->private_data);

	int	err, size;

	if(_IOC_TYPE(cmd) != IOCTL_MAGIC) return -EINVAL;
	if(_IOC_NR(cmd) >= IOCTL_MAXNR) return -EINVAL;

	size = _IOC_SIZE(cmd);

	if(size) {
		if(_IOC_DIR(cmd) & _IOC_READ)
			err = access_ok(VERIFY_WRITE, (void *) arg, size);
		else if(_IOC_DIR(cmd) & _IOC_WRITE)
			err = access_ok(VERIFY_READ, (void *) arg, size);
		if(!err) {
			NM_KMSG("%s : Wrong argument! cmd(0x%08x) _IOC_NR(%d) _IOC_TYPE(0x%x) _IOC_SIZE(%d) _IOC_DIR(0x%x)\n", 
			__FUNCTION__, cmd, _IOC_NR(cmd), _IOC_TYPE(cmd), _IOC_SIZE(cmd), _IOC_DIR(cmd));
			return err;
		}
	}

	switch(cmd)
	{
		case IOCTL_ISDBT_POWER_ON:
			NM_KMSG("ISDBT_POWER_ON enter..\n");
			gpio_cfg.gpio_on();
			break;
			
		case IOCTL_ISDBT_POWER_OFF:
			NM_KMSG("ISDBT_POWER_OFF enter..\n");
			gpio_cfg.gpio_off();
			break;

		case IOCTL_ISDBT_INTERRUPT_REGISTER:
		{
			unsigned long flags, retval;
			spin_lock_irqsave(&pdev->isr_lock, flags);

			NM_KMSG("<isdbt> ioctl: interrupt register, (stat : %d)\n", pdev->irq_status);

			if(pdev->irq_status == DTV_IRQ_DEINIT)
			{
				set_irq_type(gpio_cfg.irq, IRQ_TYPE_LEVEL_LOW);
				retval = request_irq(gpio_cfg.irq, isdbt_irq_handler, /*IRQF_DISABLED|*/IRQF_TRIGGER_LOW, ISDBT_DEV_NAME, (void*)pdev);
				if(retval < 0) {
					NM_KMSG("<isdbt> INT reg, fail\n");
					return -1;
				}
				disable_irq_nosync(gpio_cfg.irq);
				pdev->irq_status = DTV_IRQ_INIT;
			}
			spin_unlock_irqrestore(&pdev->isr_lock, flags);

			break;
		}
		
		case IOCTL_ISDBT_INTERRUPT_UNREGISTER:
		{
			unsigned long flags, retval;
			spin_lock_irqsave(&pdev->isr_lock, flags);

			NM_KMSG("<isdbt> ioctl: interrupt unregister, (stat : %d)\n", pdev->irq_status);

			if(pdev->irq_status == DTV_IRQ_INIT)
			{
				free_irq(gpio_cfg.irq, pdev);
				pdev->irq_status = DTV_IRQ_DEINIT;
			}

			spin_unlock_irqrestore(&pdev->isr_lock, flags);
			
			break;
		}
		
		case IOCTL_ISDBT_INTERRUPT_ENABLE:
		{
			unsigned long flags;
			spin_lock_irqsave(&pdev->isr_lock, flags);

			NM_KMSG("<isdbt> ioctl: interrupt enable, (stat : %d)\n", pdev->irq_status);

			if(pdev->irq_status == DTV_IRQ_INIT)
			{
				enable_irq(gpio_cfg.irq);
				pdev->irq_status = DTV_IRQ_SET;
			}
			
			spin_unlock_irqrestore(&pdev->isr_lock, flags);
			break;
		}

		case IOCTL_ISDBT_INTERRUPT_DISABLE:
		{
			unsigned long flags;
			spin_lock_irqsave(&pdev->isr_lock, flags);

			NM_KMSG("<isdbt> ioctl: interrupt disable, (stat : %d)\n", pdev->irq_status);

			if(pdev->irq_status == DTV_IRQ_SET)
			{
				disable_irq_nosync(gpio_cfg.irq);
				pdev->irq_status = DTV_IRQ_INIT;
			}

			spin_unlock_irqrestore(&pdev->isr_lock, flags);
			break;
		}

		case IOCTL_ISDBT_INTERRUPT_DONE:
		{
			unsigned long flags;

			spin_lock_irqsave(&pdev->isr_lock,flags);

			NM_KMSG("<isdbt> ioctl: interrupt done, (stat : %d)\n", pdev->irq_status);
			if ( pdev->irq_status == DTV_IRQ_INIT )
			{
				enable_irq(gpio_cfg.irq);
				pdev->irq_status = DTV_IRQ_SET;
			}

			spin_unlock_irqrestore(&pdev->isr_lock,flags);
			break;
		}

		case IOCTL_ISDBT_INTERRUPT_HANDLER_START:
			NM_KMSG("<isdbt> IOCTL_ISDBT_INTERRUPT_HANDLER_START\n");
			break;

		default:
			res = 1;
			break;
			
	}

	return res;
}


static const struct file_operations isdbt_fops = {
	.owner		= THIS_MODULE,
	.open		= isdbt_open,
	.release		= isdbt_release,
	.read		= isdbt_read,
	.write		= isdbt_write,
	.ioctl			= isdbt_ioctl,
	.poll			= isdbt_poll,
};

void isdbt_set_hw_func(struct isdbt_platform_data *gpio)
{
	NM_KMSG("<isdbt> isdbt_set_hw_func\n");
	memcpy(&gpio_cfg, gpio, sizeof(struct isdbt_platform_data));
}

static int isdbt_probe(struct platform_device *pdev)
{
	int ret;
	struct device *isdbt_dev;
	struct isdbt_platform_data *p = pdev->dev.platform_data;

	NM_KMSG("<isdbt> isdbt_probe, MAJOR = %d\n", ISDBT_DEV_MAJOR);

	// 1. register character device
	ret = register_chrdev(ISDBT_DEV_MAJOR, ISDBT_DEV_NAME, &isdbt_fops);
	if(ret < 0)
		NM_KMSG("<isdbt> register_chrdev(ISDBT_DEV) failed\n");

	// 2. class create
	isdbt_class = class_create(THIS_MODULE, ISDBT_DEV_NAME);
	if(IS_ERR(isdbt_class)) {
		unregister_chrdev(ISDBT_DEV_MAJOR, ISDBT_DEV_NAME);
		class_destroy(isdbt_class);
		NM_KMSG("<isdbt> class create failed\n");

		return -EFAULT;
	}

	// 3. device create
	isdbt_dev = device_create(isdbt_class, NULL, MKDEV(ISDBT_DEV_MAJOR, ISDBT_DEV_MINOR), NULL, ISDBT_DEV_NAME);
	if(IS_ERR(isdbt_dev)) {
		unregister_chrdev(ISDBT_DEV_MAJOR, ISDBT_DEV_NAME);
		class_destroy(isdbt_class);
		NM_KMSG("<isdbt> device create failed\n");

		return -EFAULT;
	}
		
	nmi326_spi_init();
	isdbt_set_hw_func(p);

	return 0;
	
}

static int isdbt_remove(struct platform_device *pdev)
{
	return 0;
}

static int isdbt_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int isdbt_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver isdbt_driver = {
	.probe	= isdbt_probe,
	.remove	= isdbt_remove,
	.suspend	= isdbt_suspend,
	.resume	= isdbt_resume,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "isdbt"
	},
};

static int __init isdbt_init(void)
{
	int result;

	NM_KMSG("<isdbt> isdbt_init \n");

	result = platform_driver_register(&isdbt_driver);
	if(result)
		return result;
	return 0;
}

static void __exit isdbt_exit(void)
{
	NM_KMSG("<isdbt> isdbt_exit \n");

	unregister_chrdev(ISDBT_DEV_MAJOR, "isdbt");
	device_destroy(isdbt_class, MKDEV(ISDBT_DEV_MAJOR, ISDBT_DEV_MINOR));
	class_destroy(isdbt_class);

	platform_driver_unregister(&isdbt_driver);

	nmi326_spi_exit();
}

module_init(isdbt_init);
module_exit(isdbt_exit);

MODULE_LICENSE("Dual BSD/GPL");

