/*
  SEC M5MO
 */
/***************************************************************
CAMERA DRIVER FOR 5M CAM(FUJITSU M4Mo) by PGH
ver 0.1 : only preview (base on universal)
****************************************************************/

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include <mach/vreg.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/kernel.h>

#include "sec_m5mo.h"
#include "sec_cam_dev.h"

#include <asm/gpio.h> 

#include <mach/board.h>
#include <mach/msm_iomap.h>

#include <linux/interrupt.h>

//#define FORCE_FIRMWARE_UPDATE

#define M5MO_I2C_RETRY		5
#define M5MO_I2C_VERIFY		100
#define M5MO_ISP_TIMEOUT	3000
#define M5MO_ISP_AFB_TIMEOUT	15000 /* FIXME */

#define M5MO_THUMB_MAXSIZE	0xFC00   //0x10000 -> 0xFC00 

static int m5mo_reset_for_update(void);
static int m5mo_load_fw(void);
static int m5mo_check_fw(void);
static int m5mo_dump_fw(void);

static int firmware_update_mode = 0;

static int m5mo_set_touch_auto_focus(int val);

#define CHECK_ERR(x)   \
		do {\
		if ((x) < 0) { \
			printk("i2c falied, err %d\n", x); \
			x = -1; \
			return x; \
				}	\
		} while(0)


struct m5mo_work_t {
	struct work_struct work;
};

static struct  m5mo_work_t *m5mo_sensorw;
static struct  i2c_client *m5mo_client;
static unsigned int config_csi;

#if defined (CAMERA_WXGA_PREVIEW)
static const struct m5mo_frmsizeenum m5mo_preview_sizes[] = {
	{ M5MO_PREVIEW_QCIF,	176,	144,	0x05 },	/* 176 x 144 */
	{ M5MO_PREVIEW_QCIF2,	528,	432,	0x2C },	/* 176 x 144 */
	{ M5MO_PREVIEW_QVGA,	320,	240,	0x09 },
	{ M5MO_PREVIEW_VGA,	640,	480,	0x17 },
	{ M5MO_PREVIEW_D1,	720,	480,	0x18 },
	{ M5MO_PREVIEW_WVGA,	800,	480,	0x1A },
	{ M5MO_PREVIEW_qHD,	960,	720,	0x2D }, /* 960x720 */
	{ M5MO_PREVIEW_HD,	1280,	720,	0x21 },
	{ M5MO_PREVIEW_1280x800, 1280,    800,   0x35 },
	{ M5MO_PREVIEW_1072x800, 1072,  800,  0x36 },
	{ M5MO_PREVIEW_FHD,	1920,	1080,	0x28 },
	{ M5MO_PREVIEW_HDR,	3264,	2448,	0x27 },	
};
#else
static const struct m5mo_frmsizeenum m5mo_preview_sizes[] = {
	{ M5MO_PREVIEW_QCIF,	176,	144,	0x05 },	/* 176 x 144 */
	{ M5MO_PREVIEW_QCIF2,	528,	432,	0x2C },	/* 176 x 144 */
	{ M5MO_PREVIEW_QVGA,	320,	240,	0x09 },
	{ M5MO_PREVIEW_VGA,	640,	480,	0x17 },
	{ M5MO_PREVIEW_D1,	720,	480,	0x18 },
	{ M5MO_PREVIEW_WVGA,	800,	480,	0x1A },
	{ M5MO_PREVIEW_qHD,	960,	720,	0x2D }, /* 960x720 */
	{ M5MO_PREVIEW_HD,	1280,	720,	0x21 },
	{ M5MO_PREVIEW_FHD,	1920,	1080,	0x28 },
	{ M5MO_PREVIEW_HDR,	3264,	2448,	0x27 },	
};
#endif
static const struct m5mo_frmsizeenum m5mo_picture_sizes[] = {
	{ M5MO_CAPTURE_8MP,	3264,	2448,	0x25 }, //4:3
	{ M5MO_CAPTURE_W7MP,	3264,	1968,	0x2D }, // 5:3
	{ M5MO_CAPTURE_HD6MP,	3264,	1836,	0x21 }, // 16:9
	{ M5MO_CAPTURE_HD4MP,	2560,	1440,	0x1C }, // 16:9
	{ M5MO_CAPTURE_3MP,	2048,	1536,	0x1B }, // 4:3
	{ M5MO_CAPTURE_W2MP,	2048,	1232,	0x2C }, // 5:3
	{ M5MO_CAPTURE_SXGA,	1280,	960,	0x14 }, // 5:3	
	{ M5MO_CAPTURE_HD1MP,	1280,	720,	0x10 }, // 16:9
	{ M5MO_CAPTURE_WVGA,	800,	480,	0x0A }, //5:3
	{ M5MO_CAPTURE_VGA,	640,	480,	0x09 }, //4:3
};


#define M5MO_DEF_APEX_DEN	100

static struct m5mo_ctrl_t *m5mo_ctrl;
static struct m5mo_exif_data * m5mo_exif;


#define m5mo_readb(g, b, v)	m5mo_read(1, g, b, v)
#define m5mo_readw(g, b, v)	m5mo_read(2, g, b, v)
#define m5mo_readl(g, b, v)	m5mo_read(4, g, b, v)


#define m5mo_writeb(g, b, v)	m5mo_write(1, g, b, v) // 8-bit write
#define m5mo_writew(g, b, v)	m5mo_write(2, g, b, v) // 16-bit write
#define m5mo_writel(g, b, v)	m5mo_write(4, g, b, v) // 32-bit write

#ifdef CONFIG_FB_MSM_MDP_ADDITIONAL_BUS_SCALING
int require_exceptional_MDP_clock=0;
#endif

static DECLARE_WAIT_QUEUE_HEAD(m5mo_wait_queue);

#if 0

static int32_t m5mo_i2c_txdata(unsigned short saddr, char *txdata, int length)
{
	//unsigned long	flags;

	struct i2c_msg msg[] = {
	{
		.addr = saddr,
		.flags = 0,
		.len = length,
		.buf = txdata,
	},
	};

	//local_irq_save(flags);
#if 0
#if 0
	writel(3 << 6 | 1 << 2 | 3, MSM_TLMM_BASE+0x1470); 
	writel(3 << 6 | 1 << 2 | 3, MSM_TLMM_BASE+0x1480); 
	printk (KERN_ERR "47 : 0x%x 48 : 0x%x \n", readl(MSM_TLMM_BASE+0x1470) & 0x3C, readl(MSM_TLMM_BASE+0x1480) & 0x3C); 

#else
	//ADD
	writel(1 << 9 | 3 << 6 | 0 << 2 | 0, MSM_TLMM_BASE+0x1470);
	writel(1 << 9 | 3 << 6 | 0 << 2 | 0, MSM_TLMM_BASE+0x1480); 
	gpio_direction_output(47, 1);
	gpio_direction_output(48, 1);
	gpio_set_value_cansleep(47, 1);
	gpio_set_value_cansleep(48, 1);
	msleep(30);
	gpio_set_value_cansleep(47, 0);
	gpio_set_value_cansleep(48, 0);
	msleep(30);
	gpio_set_value_cansleep(47, 1);
	gpio_set_value_cansleep(48, 1);
	msleep(30);
	gpio_set_value_cansleep(47, 0);
	gpio_set_value_cansleep(48, 0);
	msleep(30);
	//ADDend
#endif	
#endif
	if (i2c_transfer(m5mo_client->adapter, msg, 1) < 0) {
		printk("m5mo_i2c_txdata failed\n");
		//local_irq_restore(flags);
		return -EIO;
	}
	//local_irq_restore(flags);
	return 0;
}
#endif

static int m5mo_write(unsigned char len, unsigned char category, unsigned char byte, int val)
{
	struct i2c_msg msg;
	unsigned char data[len + 4];
	int i, err;

	if (len != 0x01 && len != 0x02 && len != 0x04)
		return -EINVAL;

	msg.addr = 0x1F;//m5mo_client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	data[0] = msg.len;
	data[1] = 0x02;			/* Write category parameters */
	data[2] = category;
	data[3] = byte;
	
	if (len == 0x01) {
		data[4] = val & 0xFF;
	} else if (len == 0x02) {
		data[4] = (val >> 8) & 0xFF;
		data[5] = val & 0xFF;
	} else {
		data[4] = (val >> 24) & 0xFF;
		data[5] = (val >> 16) & 0xFF;
		data[6] = (val >> 8) & 0xFF;
		data[7] = val & 0xFF;
	}

	//cam_info("category %#x, byte %#x, value %#x", category, byte, val);

	for (i = M5MO_I2C_RETRY; i; i--) {
		err = i2c_transfer(m5mo_client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	return err;
}



static int m5mo_read(unsigned char len, unsigned char category, unsigned char byte, unsigned int *val)
{
	int rc = 0;

	unsigned char tx_buf[5];	
	unsigned char rx_buf[len + 1];

	struct i2c_msg msg = {
		.addr = m5mo_client->addr,
		.flags = 0,		
		.len = 5,		
		.buf = tx_buf,	
	};

	*val = 0;

	tx_buf[0] = 0x05;	//just protocol
	tx_buf[1] = 0x01;	//just protocol
	tx_buf[2] = category;
	tx_buf[3] = byte;
	tx_buf[4] = len;


	rc = i2c_transfer(m5mo_client->adapter, &msg, 1);
	if (likely(rc == 1)) 	{
		msg.flags = I2C_M_RD;		
		msg.len = len + 1;		
		msg.buf = rx_buf;		
		rc = i2c_transfer(m5mo_client->adapter, &msg, 1);	
	} else {		
		printk(KERN_ERR "M5MO: %s: failed at category=%x,  byte=%x  \n", __func__,category,  byte);		
		return -EIO;	
	}


	if (likely(rc == 1)) {		
		if (len == 1)			
			*val = rx_buf[1];		
		else if (len == 2)			
			*(unsigned short *)val = be16_to_cpu(*(unsigned short *)(&rx_buf[1]));		
		else			
			*val = be32_to_cpu(*(unsigned int *)(&rx_buf[1]));		

		return 0;	
	}	


		printk(KERN_ERR "M5MO: %s: 2  failed at category=%x,  byte=%x  \n", __func__,category,  byte);		

	return -EIO;

}


#if 0 // for_8M_cam
static int32_t m5mo_i2c_write_memory(int address, short size, char *value)
{
	int32_t rc = -EFAULT;
 	
	char *buf;
	int i;

	CAM_DEBUG("START");

	buf = kmalloc(8 + size, GFP_KERNEL);
	if(buf <0) {
		printk("[PGH] m5mo_i2c_write_memory  memory alloc fail!\n");
		return -1;
	}

	buf[0] = 0x00;
	buf[1] = 0x04;

	buf[2] = (address & 0xFF000000) >> 24;
	buf[3] = (address & 0x00FF0000) >> 16;
	buf[4] = (address & 0x0000FF00) >> 8;
	buf[5] = (address & 0x000000FF) ;

	buf[6] = (size & 0xFF00) >> 8;
	buf[7] = (size & 0x00FF);

	memcpy(buf+8 , value, size);


	for(i=0; i<10; i++) {
		rc = m5mo_i2c_txdata(0x1F, buf,  8 + size);

		if(rc == 0) {
			msleep(5);
			kfree(buf);
			return rc;
		} else {
			msleep(i*2);
			printk("m5mo_i2c_write_memory fail!\n");
		}
	}


	msleep(20);

	kfree(buf);
	
	return rc;
}
#endif



static int m5mo_i2c_verify(unsigned char category, unsigned char byte, unsigned char value)
{
	unsigned int val = 0;
	unsigned char 	i;

	for (i= 0; i < M5MO_I2C_VERIFY_RETRY; i++) {
		m5mo_readb(category, byte, &val);
		if ((unsigned char)val == value) {
			CAM_DEBUG("[c,b,v] [%02x, %02x, %02x] (try = %d)", category, byte, value, i);
			return 0;
		}
		msleep(10);
	}

	cam_err("Failed !!");
	return -EBUSY;
}

static irqreturn_t m5mo_isp_isr(int irq, void *dev_id)
{
	cam_info("**************** interrupt ****************");
	m5mo_ctrl->isp.issued = 1;
	wake_up_interruptible(&m5mo_ctrl->isp.wait);

	return IRQ_HANDLED;
}

static u32 m5mo_wait_interrupt(unsigned int timeout)
{
	cam_info(" m5mo_wait_interrupt :E");

	if (wait_event_interruptible_timeout(m5mo_ctrl->isp.wait, m5mo_ctrl->isp.issued == 1,
		msecs_to_jiffies(timeout)) == 0) {
		cam_err("timeout");
		return 0;
	}

	m5mo_ctrl->isp.issued = 0;

	m5mo_readb(M5MO_CATEGORY_SYS, M5MO_SYS_INT_FACTOR, &m5mo_ctrl->isp.int_factor);

	cam_info(" m5mo_wait_interrupt:	X: 0x%x", m5mo_ctrl->isp.int_factor);
	return m5mo_ctrl->isp.int_factor;
}


#if 0
static int wait_interrupt_mode(int mode)
{
	int wait_count = 200;
	unsigned int value;
	do {
		m5mo_readb(M5MO_CATEGORY_SYS, M5MO_SYS_INT_FACTOR, &value);
		msleep(10);
	} while((--wait_count) > 0 && !(value & mode));

	
	if (wait_count == 0)
		cam_err("INT ERROR!!!!!!!!!!!!!!!!!!!!!!!! : mode: 0x%x = 0x%x",mode, value);
	else
		cam_info("mode: 0x%x = 0x%x,  count: %d",mode, value, wait_count);
		
	return wait_count;
}

#endif

static int m5mo_set_mode(u32 mode)
{
	int i, err;
	u32 old_mode, val;

	err = m5mo_readb(M5MO_CATEGORY_SYS, M5MO_SYS_MODE, &old_mode);
	CAM_DEBUG("E %#x -> %#x", old_mode, mode);
	if (err < 0)
		return err;

	if (old_mode == mode)
		return old_mode;


	switch (old_mode) {
	case M5MO_SYSINIT_MODE:
		cam_info("sensor is initializing\n");
		err = -EBUSY;
		break;

	case M5MO_PARMSET_MODE:
		if (mode == M5MO_STILLCAP_MODE) {
			err = m5mo_writeb(M5MO_CATEGORY_SYS,M5MO_SYS_MODE, M5MO_MONITOR_MODE);
			if (err < 0)
				break;
			for (i = M5MO_I2C_VERIFY; i; i--) {
				err = m5mo_readb(M5MO_CATEGORY_SYS, M5MO_SYS_MODE, &val);
				if (val == M5MO_MONITOR_MODE)
					break;
				msleep(10);
			}
		}
	case M5MO_MONITOR_MODE:
	case M5MO_STILLCAP_MODE:
		err = m5mo_writeb(M5MO_CATEGORY_SYS, M5MO_SYS_MODE, mode);
		break;

	default:
		cam_err("current mode is unknown, %d\n", old_mode);
		err = -EINVAL;
	}
	
	if (err < 0)
		return err;
	
	m5mo_i2c_verify(M5MO_CATEGORY_SYS, M5MO_SYS_MODE, mode);

	CAM_DEBUG("X");
	return old_mode;
}


static int m5mo_set_lock(int val)
{
	int err;

	CAM_DEBUG("%s", val ? "on" : "off");

	err = m5mo_writeb(M5MO_CATEGORY_AE, M5MO_AE_LOCK, val);
	CHECK_ERR(err);
	err = m5mo_writeb(M5MO_CATEGORY_WB, M5MO_AWB_LOCK, val);
	CHECK_ERR(err);
	
	m5mo_ctrl->focus.lock = val;
	return 0;
}


static int m5mo_set_face_beauty(int val)
{
	int err = 0;
	
	CAM_DEBUG("%s", val ? "on" : "off");
	err = m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_AFB_CAP_EN, val ? 0x01 : 0x00);
	CHECK_ERR(err);

	m5mo_ctrl->settings.face_beauty = val;
	return 0;
}



static int m5mo_check_dataline(int val)
{
	int err = 0;

	CAM_DEBUG("%s", val ? "on" : "off");

	if(!val) {
		m5mo_set_mode(M5MO_MONITOR_MODE);
		m5mo_set_lock(0);
	}
	err = m5mo_writeb(M5MO_CATEGORY_TEST, M5MO_TEST_OUTPUT_YCO_TEST_DATA, val ? 0x01 : 0x00);
	CHECK_ERR(err);

	return 0;
}


static int m5mo_set_face_detection(int mode)
{
	int err = 0;
	
	CAM_DEBUG("%s", mode ? "on" : "off");
	
	err = m5mo_writeb(M5MO_CATEGORY_FD, M5MO_FD_CTL, mode ? 0x11 : 0x00);
	CHECK_ERR(err);
	
	return 0;
}

static int m5mo_set_wdr(int val)
{
	int contrast, wdr, err = 0;

	CAM_DEBUG("%s", val ? "on" : "off");
			
	contrast = (val == 1 ? 0x09 : 0x05);
	wdr = (val == 1 ? 0x01 : 0x00);

	err = m5mo_writeb(M5MO_CATEGORY_MON, M5MO_MON_TONE_CTRL, contrast);
	CHECK_ERR(err);
	err = m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_WDR_EN, wdr);
	CHECK_ERR(err);

	return 0;
}

static long m5mo_set_zoom(	int8_t level)
{
	int err = 0;

	int zoom[] = { 1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15, 17, 18, 19,
		20, 21, 22, 24, 25, 26, 28, 29, 30, 31, 32, 34, 35, 36, 38, 39};

	CAM_DEBUG("level:%d ",level);

	if(level < 0 || level > 30 )	{
		cam_err("Invalid Zoom Level !!!");
		level = 0;
	}
	
	/* Start Digital Zoom */
	err = m5mo_writeb(M5MO_CATEGORY_MON, M5MO_MON_ZOOM, zoom[level]); 
	CHECK_ERR(err);
	//msleep(30);

	m5mo_ctrl->settings.zoom = level;
	return 0;
}

int find_preview_size_value(int w, int h)
{
	int i = 0;
	int num_entries = ARRAY_SIZE(m5mo_preview_sizes);
	
	for (i = 0; i < num_entries; i++) {
		if (m5mo_preview_sizes[i].width ==  w && h == m5mo_preview_sizes[i].height) {
			return i;
		}
	}
	
	cam_err("Invalid preview size");
	return M5MO_PREVIEW_VGA;
}
			
int find_picture_size_value(int w, int h)
{
	int i = 0;
	int num_entries = ARRAY_SIZE(m5mo_picture_sizes);

	for (i = 0; i < num_entries; i++) {
		if((m5mo_picture_sizes[i].width == w) && (m5mo_picture_sizes[i].height== h)) 
			return i;		
	}
	
	cam_err("Invalid picture size");
	return M5MO_CAPTURE_3MP;
}

static int m5mo_set_picture_size(u32 width, u32 height)
{
	int frame_size = -1;
	
	CAM_DEBUG("%d*%d", width, height);
	
	frame_size = find_picture_size_value(width, height);

	cam_err("size = 0x%x", m5mo_picture_sizes[frame_size].reg_val);
	m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_MAIN_IMG_SIZE, m5mo_picture_sizes[frame_size].reg_val);
	
	m5mo_ctrl->settings.picture_size.width = m5mo_picture_sizes[frame_size].width;
	m5mo_ctrl->settings.picture_size.height =  m5mo_picture_sizes[frame_size].height;
	
	return 0;
}

static int m5mo_set_preview_size(int width, int height)
{
//	u32 int_factor;
	int frame_size = -1;
	
	CAM_DEBUG("%d*%d", width, height);

	if (m5mo_ctrl->settings.preview_size.width == width && m5mo_ctrl->settings.preview_size.height == height) {
		cam_err("same preview size");
		return 0;
	}
	
	frame_size = find_preview_size_value(width, height);
	
	if (frame_size < M5MO_PREVIEW_VGA ) {
		cam_err("Resizing");
		frame_size = M5MO_PREVIEW_VGA;
	}
	
	/* change to paramset mode */
	m5mo_set_mode(M5MO_PARMSET_MODE);

	cam_err("size = 0x%x", m5mo_preview_sizes[frame_size].reg_val);
	m5mo_writeb(M5MO_CATEGORY_PARM, M5MO_PARM_MON_SIZE, m5mo_preview_sizes[frame_size].reg_val);
	m5mo_ctrl->settings.preview_size.width = m5mo_preview_sizes[frame_size].width;
	m5mo_ctrl->settings.preview_size.height =  m5mo_preview_sizes[frame_size].height;

	if (m5mo_ctrl->settings.zoom) {
		m5mo_set_zoom(m5mo_ctrl->settings.zoom);
	}

// param_mode -> preview size -> [LP11 state] ( mipi mode )-> monitor_mode
#if 0 // for LP11 state
	/* change to monitor mode */
	m5mo_set_mode(M5MO_MONITOR_MODE);

	int_factor = m5mo_wait_interrupt(M5MO_ISP_TIMEOUT);
	if (!(int_factor & M5MO_INT_MODE)) {
		cam_err("M5MO_INT_MODE isn't issued, %#x",int_factor);
		return -ETIMEDOUT;
	}
#endif
	return 0;
}


static int m5mo_set_jpeg_quality(unsigned int jpeg_quality)
{
	int err = 0, ratio = 0;

	CAM_DEBUG("%d",jpeg_quality);
	
//	if(m5mo_ctrl->settings.jpeg_quality == jpeg_quality)
//		return 0;
	
	switch(jpeg_quality) {
	case M5MO_JPEG_ECONOMY:
		ratio = 0x0F;
		break;
	case M5MO_JPEG_NORMAL:
		ratio = 0x0A;
		break;
	case M5MO_JPEG_FINE:
		ratio = 0x05;
		break;
	case M5MO_JPEG_SUPERFINE:
	default:
		ratio = 0x00;
		break;
	}

	err = m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_JPEG_RATIO, 0x62);
	CHECK_ERR(err);
	err = m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_JPEG_RATIO_OFS, ratio);
	CHECK_ERR(err);

	m5mo_ctrl->settings.jpeg_quality = jpeg_quality;

	return 0;
}


static int  m5mo_get_exif(void)
{
	/* standard values */
	u16 iso_std_values[] = { 10, 12, 16, 20, 25, 32, 40, 50, 64, 80,
		100, 125, 160, 200, 250, 320, 400, 500, 640, 800,
		1000, 1250, 1600, 2000, 2500, 3200, 4000, 5000, 6400, 8000};
	/* quantization table */
	u16 iso_qtable[] = { 11, 14, 17, 22, 28, 35, 44, 56, 71, 89,
		112, 141, 178, 224, 282, 356, 449, 565, 712, 890,
		1122, 1414, 1782, 2245, 2828, 3564, 4490, 5657, 7127, 8909};
	
	int iso_index = 30;
	
	int num, den, i, err = 0;

	CAM_DEBUG("E");
	/* exposure time */
	err = m5mo_readl(M5MO_CATEGORY_EXIF, M5MO_EXIF_EXPTIME_NUM, &num);
	CHECK_ERR(err);
	err = m5mo_readl(M5MO_CATEGORY_EXIF, M5MO_EXIF_EXPTIME_DEN, &den);
	CHECK_ERR(err);
	m5mo_exif->exptime = (u32)num*1000/den;

	/* flash */
	err = m5mo_readw(M5MO_CATEGORY_EXIF, M5MO_EXIF_FLASH, &num);
	CHECK_ERR(err);
	m5mo_exif->flash = (u16)num;

	/* iso */
	err = m5mo_readw(M5MO_CATEGORY_EXIF, M5MO_EXIF_ISO, &num);
	CHECK_ERR(err);
	
	for (i = 0; i < iso_index ; i++) {
		if (num <= iso_qtable[i]) {
			m5mo_exif->iso = iso_std_values[i];
			break;
		}
	}

	/* shutter speed */
	err = m5mo_readl(M5MO_CATEGORY_EXIF, M5MO_EXIF_TV_NUM, &num);
	CHECK_ERR(err);
	err = m5mo_readl(M5MO_CATEGORY_EXIF, M5MO_EXIF_TV_DEN, &den);
	CHECK_ERR(err);
	m5mo_exif->tv = num*M5MO_DEF_APEX_DEN/den;


	/* brightness */
	err = m5mo_readl(M5MO_CATEGORY_EXIF, M5MO_EXIF_BV_NUM, &num);
	CHECK_ERR(err);
	err = m5mo_readl(M5MO_CATEGORY_EXIF, M5MO_EXIF_BV_DEN, &den);
	CHECK_ERR(err);
	if (den == 0) {
		cam_err("check calibraion version. (M5MO_EXIF_BV_DEN is ZERO)");
		den = 1;
	}
	m5mo_exif->bv = num*M5MO_DEF_APEX_DEN/den;

	/* exposure */
	err = m5mo_readl(M5MO_CATEGORY_EXIF, M5MO_EXIF_EBV_NUM, &num);
	CHECK_ERR(err);
	err = m5mo_readl(M5MO_CATEGORY_EXIF, M5MO_EXIF_EBV_DEN, &den);
	CHECK_ERR(err);
	if (den == 0) {
		cam_err("check calibraion version. (M5MO_EXIF_EBV_DEN is ZERO)");
		den = 1;
	}
	m5mo_exif->ebv = num*M5MO_DEF_APEX_DEN/den;

	
	CAM_DEBUG("X");

	return 0;
}


static int m5mo_set_movie_mode(int mode)
{
	int err = 0;
	
	if ((mode != SENSOR_CAMERA) && (mode != SENSOR_MOVIE)) {
		return -EINVAL;
	}

	CAM_DEBUG("%d",mode);

	err = m5mo_set_mode(M5MO_PARMSET_MODE);
	CHECK_ERR(err);

	err = m5mo_writeb(M5MO_CATEGORY_PARM, M5MO_PARM_HDMOVIE, mode == SENSOR_MOVIE ? 0x01 : 0x00);
	CHECK_ERR(err);

	m5mo_ctrl->settings.sensor_mode = mode;
	return 0;
}


static int m5mo_set_fps(unsigned int fps)
{
	int err = 0;
	u32 int_factor;
	CAM_DEBUG("fps : %d ",fps);

	if (m5mo_ctrl->settings.fps == fps){
		m5mo_ctrl->settings.fps = fps;
		return 0;		
	}
	
	/* change to paramset mode */
	m5mo_set_mode(M5MO_PARMSET_MODE);
	
	err = m5mo_writeb(M5MO_CATEGORY_PARM, M5MO_PARM_FLEX_FPS,fps != 30 ? fps : 0);

	m5mo_set_mode(M5MO_MONITOR_MODE);
	
	int_factor = m5mo_wait_interrupt(M5MO_ISP_TIMEOUT);
	if (!(int_factor & M5MO_INT_MODE)) {
		cam_err("M5MO_INT_MODE isn't issued, %#x",int_factor);
		return -ETIMEDOUT;
	}
	
	m5mo_ctrl->settings.fps = fps;
	
	return 0;
	
}
static int m5mo_set_ev(int8_t val)
{
	int err = 0;
	unsigned char ev_table[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
	
	CAM_DEBUG("%d",val);
	if (val < EV_MINUS_4 || val > EV_PLUS_4) {
		cam_err("Invalid");
		val = EV_DEFAULT;
	}

	m5mo_ctrl->settings.ev = val;
	err = m5mo_writeb(M5MO_CATEGORY_AE, M5MO_AE_INDEX, ev_table[val]);
	CHECK_ERR(err);
	
	return 0;
}

static int m5mo_set_whitebalance(int8_t wb)
{
	int err = 0;
	CAM_DEBUG("%d",wb);
	
retry:
	switch(wb) {
	case WHITE_BALANCE_AUTO:
		err = m5mo_writeb( M5MO_CATEGORY_WB, M5MO_WB_AWB_MODE, 0x01);
		CHECK_ERR(err);
		err = m5mo_writeb( M5MO_CATEGORY_WB, M5MO_WB_AWB_MANUAL, 0x01);
		CHECK_ERR(err);
		break;
	case WHITE_BALANCE_INCANDESCENT:
		err = m5mo_writeb( M5MO_CATEGORY_WB, M5MO_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m5mo_writeb( M5MO_CATEGORY_WB, M5MO_WB_AWB_MANUAL, 0x01);
		CHECK_ERR(err);
		break;
	case WHITE_BALANCE_FLUORESCENT:
		err = m5mo_writeb( M5MO_CATEGORY_WB, M5MO_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m5mo_writeb( M5MO_CATEGORY_WB, M5MO_WB_AWB_MANUAL, 0x02);
		CHECK_ERR(err);
		break;
	case WHITE_BALANCE_SUNNY:
		err = m5mo_writeb( M5MO_CATEGORY_WB, M5MO_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m5mo_writeb( M5MO_CATEGORY_WB, M5MO_WB_AWB_MANUAL, 0x04);
		CHECK_ERR(err);
		break;
	case WHITE_BALANCE_CLOUDY:
		err = m5mo_writeb( M5MO_CATEGORY_WB, M5MO_WB_AWB_MODE, 0x02);
		CHECK_ERR(err);
		err = m5mo_writeb( M5MO_CATEGORY_WB, M5MO_WB_AWB_MANUAL, 0x05);
		CHECK_ERR(err);
		break;
	default:
		cam_err("Invalid(%d)", wb);
		wb = WHITE_BALANCE_AUTO;
		goto retry;
	}

	m5mo_ctrl->settings.wb = wb;
	return 0;
}

static int m5mo_set_metering(int8_t metering)
{
	int err = 0;
	CAM_DEBUG("%d", metering);

retry:
	switch(metering) {
	case METERING_MATRIX:
		err = m5mo_writeb(M5MO_CATEGORY_AE, M5MO_AE_MODE, 0x01);
		CHECK_ERR(err);
		break;
	case METERING_CENTER:
		err = m5mo_writeb(M5MO_CATEGORY_AE, M5MO_AE_MODE, 0x03);
		CHECK_ERR(err);
		break;
	case METERING_SPOT:
		err = m5mo_writeb(M5MO_CATEGORY_AE, M5MO_AE_MODE, 0x06);
		CHECK_ERR(err);
		break;
	default:
		cam_err("Invalid(%d)", metering);
		metering = METERING_CENTER;
		goto retry;
	}

	m5mo_ctrl->settings.metering = metering;
	return 0;
}

static int m5mo_set_saturation(int8_t val)
{
	int err = 0;
	unsigned char saturation_table[] = {0x01, 0x02, 0x03, 0x04, 0x05};
	
	CAM_DEBUG("%d",val);
	
	if (val < SATURATION_MINUS_2 || val > SATURATION_PLUS_2) {
		cam_err("Invalid");
		val = CONTRAST_DEFAULT;
	}
	
	err = m5mo_writeb(M5MO_CATEGORY_MON, M5MO_MON_CHROMA_LVL, saturation_table[val]);
	CHECK_ERR(err);
	
	m5mo_ctrl->settings.saturation = val;
	return 0;
}

static int m5mo_set_sharpness(int val)
{
	int err = 0;
	unsigned char sharpness_table[] = {0x03, 0x04, 0x05, 0x06, 0x07};
	
	CAM_DEBUG("%d",val);

	if (val < SHARPNESS_MINUS_2 || val > SHARPNESS_PLUS_2) {
		cam_err("Invalid");
		val = SHARPNESS_DEFAULT;
	}
	
	err = m5mo_writeb(M5MO_CATEGORY_MON, M5MO_MON_EDGE_LVL, sharpness_table[val]);
	CHECK_ERR(err);

	m5mo_ctrl->settings.sharpness = val;
	return 0;
}


#if 0
static int m5mo_set_contrast(int8_t val)
{
	int err = 0;
	unsigned char contrast_table[] = {0x03, 0x04, 0x05, 0x06, 0x07};
	CAM_DEBUG("%d",val);

	if (val < CONTRAST_MINUS_2 || val > CONTRAST_PLUS_2) {
		cam_err("Invalid");
		val = CONTRAST_DEFAULT;
	}
	
	err = m5mo_writeb(M5MO_CATEGORY_MON, M5MO_MON_TONE_CTRL, contrast_table[val]);
	CHECK_ERR(err);
	
	m5mo_ctrl->settings.contrast = val;
	return 0;
}
#endif

static int m5mo_set_iso(int8_t val)
{
	int err = 0;
	unsigned char iso_table[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
	
	if (m5mo_ctrl->settings.scenemode != SCENE_MODE_NONE){
		cam_info("should not be set with scene mode");
		return 0;
	}
	
	CAM_DEBUG("%d",val);

	if (val < ISO_AUTO || val > ISO_1600) {
		cam_err("Invalid");
		val = ISO_AUTO;
	}
	
	err = m5mo_writeb(M5MO_CATEGORY_AE, M5MO_AE_ISOSEL, iso_table[val]);
	CHECK_ERR(err);
	
	m5mo_ctrl->settings.iso = val;
	return 0;
}


static int m5mo_set_flash(int value)
{
	CAM_DEBUG("%d",value);

	switch(value) {
	case M5MO_FLASH_CAPTURE_OFF:
		cam_info("Flash Off is set");
		m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_LIGHT_CTRL, 0x00);
		m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_FLASH_CTRL, 0x00);
		//if(m5mo_ctrl->settings.scenemode == M5MO_SCENE_BACKLIGHT)
		//m5mo_set_metering(M5MO_METERING_SPOT);
		break;
	case M5MO_FLASH_CAPTURE_ON:
		cam_info("Flash On is set");
		m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_LIGHT_CTRL, 0x01);
		m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_FLASH_CTRL, 0x01);
		//if(m5mo_ctrl->settings.scenemode == M5MO_SCENE_BACKLIGHT)
		//m5mo_set_metering(M5MO_METERING_CENTER);
		break;
	case M5MO_FLASH_CAPTURE_AUTO:
		cam_info("Flash Auto is set");
		m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_LIGHT_CTRL, 0x02);
		m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_FLASH_CTRL, 0x02);
		break;
	case M5MO_FLASH_MOVIE_ON:
	case M5MO_FLASH_MOVIE_ALWAYS:
		cam_info("Flash Movie On is set");
		m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_LIGHT_CTRL, 0x03);
		break;
	case M5MO_FLASH_LOW_ON:
		cam_info("M5MO_FLASH_LOW_ON is set");		
		m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_FLASH_LOWTEMP, 0x01);
		break;		
	case M5MO_FLASH_LOW_OFF:
		cam_info("M5MO_FLASH_LOW_OFF is set");		
		m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_FLASH_LOWTEMP, 0x00);
		break;		
	default:
		cam_err("Invalid");
		break;
	}

	return 0;
}


static int m5mo_set_effect_color(int8_t effect)
{
	unsigned int effect_on = 0;
	u32 int_factor;
	
	int err, old_mode, cb, cr;
	
	CAM_DEBUG("%d",effect);
	
	/* read gamma effect flag */
	m5mo_readb(M5MO_CATEGORY_PARM, M5MO_PARM_EFFECT, &effect_on);
	
	if(effect_on) {
		/* change to paramset mode */
		old_mode = m5mo_set_mode(M5MO_PARMSET_MODE);
		CHECK_ERR(old_mode);

		/* gamma effect off */
		cam_info("gamma effect off ");
		err = m5mo_writeb(M5MO_CATEGORY_PARM, M5MO_PARM_EFFECT, 0x00); 
		CHECK_ERR(err);

		if (old_mode == M5MO_MONITOR_MODE) {
			err = m5mo_set_mode(M5MO_MONITOR_MODE);
			CHECK_ERR(err);

			int_factor = m5mo_wait_interrupt(M5MO_ISP_TIMEOUT);
 			if (!(int_factor & M5MO_INT_MODE)) {
				cam_err("M5MO_INT_MODE isn't issued, %#x",int_factor);
				return -ETIMEDOUT;
			}
		}
		
	}

	cb = 0x00;
	cr = 0x00;

	switch(effect) {
	case CAMERA_EFFECT_OFF:
		break;
	case CAMERA_EFFECT_SEPIA:
		cb = 0xD8;
		cr = 0x18;
		break;
	case CAMERA_EFFECT_AQUA:
		cb = 0x40;
		cr = 0x00;
		break;
	case CAMERA_EFFECT_MONO:
		cb = 0x00;
		cr = 0x00;
		break;
	default:
		cam_err("Invalid");
		break;
	}


	err = m5mo_writeb( M5MO_CATEGORY_MON, M5MO_MON_COLOR_EFFECT, effect == CAMERA_EFFECT_OFF ? 0x00 : 0x01);
	CHECK_ERR(err);

	if (effect != CAMERA_EFFECT_OFF) {
		err = m5mo_writeb(M5MO_CATEGORY_MON, M5MO_MON_CFIXB, cb);
		CHECK_ERR(err);
		err = m5mo_writeb(M5MO_CATEGORY_MON, M5MO_MON_CFIXR, cr);
		CHECK_ERR(err);
	}

	return 0;
}


static int m5mo_set_effect_gamma(int8_t val)
{
	u32 int_factor;
	int on, effect, old_mode;
	int err = 0;

	CAM_DEBUG("%d",val);
	
	m5mo_readb(M5MO_CATEGORY_MON, M5MO_MON_COLOR_EFFECT, &on);
	if(on) {
		/* color effect off */
		cam_info("color effect off ");
		err = m5mo_writeb(M5MO_CATEGORY_MON, M5MO_MON_COLOR_EFFECT, 0x00); 	
		CHECK_ERR(err);
	}	

	switch(val) {
	case CAMERA_EFFECT_NEGATIVE:
		effect = 0x01;
		break;
	default:
		cam_err("Invalid");
		break;
	}

	/* change to paramset mode */
	old_mode = m5mo_set_mode(M5MO_PARMSET_MODE);
	CHECK_ERR(old_mode);
	
	err = m5mo_writeb(M5MO_CATEGORY_PARM, M5MO_PARM_EFFECT, effect);
	CHECK_ERR(err);
	
	if (old_mode == M5MO_MONITOR_MODE)
	{
		err = m5mo_set_mode(M5MO_MONITOR_MODE);
		CHECK_ERR(err);

		int_factor = m5mo_wait_interrupt(M5MO_ISP_TIMEOUT);
		if (!(int_factor & M5MO_INT_MODE)) {
			cam_err("M5MO_INT_MODE isn't issued, %#x",int_factor);
			return -ETIMEDOUT;
		}
	
	}
	
	return 0;
}



static long m5mo_set_effect(	int8_t effect)
{
	int err = 0;

retry:
	switch(effect) {
	case CAMERA_EFFECT_OFF:
	case CAMERA_EFFECT_SEPIA:
	case CAMERA_EFFECT_MONO:
	case CAMERA_EFFECT_AQUA:
		err = m5mo_set_effect_color(effect);
		CHECK_ERR(err);
		break;
	case CAMERA_EFFECT_NEGATIVE:
		err = m5mo_set_effect_gamma(effect);
		CHECK_ERR(err);
		break;
	default:
		cam_err("Invalid(%d)", effect);
		effect = CAMERA_EFFECT_OFF;
		goto retry;
	}

	m5mo_ctrl->settings.effect = effect;
	return err;
}

#ifdef FEATURE_CAMERA_HDR
static int m5mo_set_hdr(int val)
{
	u32 int_factor;
	int err = 0;
	CAM_DEBUG("E");

	err = m5mo_writeb(M5MO_CATEGORY_CAPCTRL, M5MO_CAPCTRL_FRM_SEL, val + 1);
	CHECK_ERR(err);

	int_factor = m5mo_wait_interrupt( M5MO_ISP_TIMEOUT);
	if (!(int_factor & M5MO_INT_CAPTURE)) {
		cam_err("M5MO_INT_CAPTURE isn't issued on transfer, %#x\n", int_factor);
		return -ETIMEDOUT;
	}

	err = m5mo_writeb(M5MO_CATEGORY_CAPCTRL, M5MO_CAPCTRL_TRANSFER, 0x01);
	int_factor = m5mo_wait_interrupt(M5MO_ISP_TIMEOUT);
	if (!(int_factor & M5MO_INT_CAPTURE)) {
		cam_err("M5MO_INT_CAPTURE isn't issued on transfer, %#x\n", int_factor);
		return -ETIMEDOUT;
	}

	CAM_DEBUG("X");
	return 0;
}
#endif

static long m5mo_set_antishake_mode(u32 onoff)
{
	int ahs, err = 0;
	
	CAM_DEBUG("%s", onoff ? "on" : "off");

	if (m5mo_ctrl->settings.scenemode != SCENE_MODE_NONE){
		cam_info("should not be set with scene mode");
		return 0;
	}
		
	ahs = (onoff == 1 ? 0x0E : 0x00);

	err = m5mo_writeb(M5MO_CATEGORY_AE, M5MO_AE_EP_MODE_MON, ahs);
	CHECK_ERR(err);
	err = m5mo_writeb(M5MO_CATEGORY_AE, M5MO_AE_EP_MODE_CAP, ahs);
	CHECK_ERR(err);

	m5mo_ctrl->settings.antishake = onoff;
	return 0;
}


static int m5mo_set_af_softlanding(void)
{
	u32 status = 0;
	int i, err = 0;

	CAM_DEBUG("E");

	if (m5mo_ctrl->isp.bad_fw){
		cam_err("\"Unknown\" state, please update F/W");
		return -ENOSYS;
	}

	err = m5mo_set_mode(M5MO_MONITOR_MODE);
	if (err <= 0) {
		cam_err("failed to set mode");
		return err;
	}

	//err = m5mo_writeb( M5MO_CATEGORY_LENS, M5MO_LENS_AF_MODE, 0x07);
	err = m5mo_writeb( M5MO_CATEGORY_LENS, M5MO_LENS_AF_MODE, 0x10);
	CHECK_ERR(err);

	for (i = M5MO_I2C_VERIFY; i; i--) {
		msleep(10);
		err = m5mo_readb(M5MO_CATEGORY_LENS, M5MO_LENS_AF_STATUS, &status);
		CHECK_ERR(err);

		cam_info("status = %d", status);
		if (status  == AF_IDLE)
			break;
	}
	
	if (status == AF_BUSY) {
		cam_err("failed");
		return -ETIMEDOUT;
	}
	CAM_DEBUG("X");
	return err;
}



static int m5mo_get_af_status(int* status)
{
	*status = m5mo_ctrl->focus.status;
	CAM_DEBUG(" : %d ",m5mo_ctrl->focus.status);
	return 0;
}


static int m5mo_set_af_mode_select(int focus_mode) {
/*
0x00 : AF normal
0x01 : AF macro
0x02 : Movie
0x03 : AF Touch Normal
0x04 : AF Touch Macro
*/
	int mode, err = 0;

	switch(focus_mode) {
	case FOCUS_MODE_MACRO:
		mode = 0x01;
		break;
					
	case FOCUS_MODE_TOUCH:
		mode = 0x03;
		break;
			
	case FOCUS_MODE_TOUCH_MACRO:	
		mode = 0x04;
		break;
		
	case FOCUS_MODE_AUTO:
		mode = 0x00;
		break;
		
	default:
		cam_info("No need to set mode_select value(focus_mode:%d)", focus_mode);
		return 0;
		
	}

	err = m5mo_writeb(M5MO_CATEGORY_LENS, M5MO_LENS_AF_MODE_SELECT, mode);
	CHECK_ERR(err);
	CAM_DEBUG("write(0x05 : 0x%x)",mode);

	return err;

}


static int m5mo_set_af(int val)
{
	int i, status, err;

// touch
//	CAM_DEBUG("focus.touch = %d   focus.touchaf = %d", m5mo_ctrl->focus.touch,  m5mo_ctrl->focus.touchaf);
	
	if (m5mo_ctrl->focus.touchaf == 1) {
		m5mo_set_touch_auto_focus(val);
	} else {
		cam_info("touch af : stop");
		m5mo_ctrl->focus.touch  = 0;//m5mo_set_touch_auto_focus(0);
	}

	CAM_DEBUG("%s, focus mode %d", val ? "start" : "stop", m5mo_ctrl->focus.mode);

	m5mo_ctrl->focus.start = val;

	if (m5mo_ctrl->focus.mode != FOCUS_MODE_CONTINOUS) {
		
		// AF Mode Select << Add
		if (val == 1) {
			err = m5mo_set_af_mode_select(m5mo_ctrl->focus.mode);
			CHECK_ERR(err);
		}
		// AF Start/Stop
		err = m5mo_writeb(M5MO_CATEGORY_LENS, M5MO_LENS_AF_START, val);
		CHECK_ERR(err);

		if (!(m5mo_ctrl->focus.touch &&
			(m5mo_ctrl->focus.mode == FOCUS_MODE_TOUCH || m5mo_ctrl->focus.mode == FOCUS_MODE_TOUCH_MACRO ) )) {
			if (val && m5mo_ctrl->focus.lock) {
				CAM_DEBUG(" ######## m5mo_set_lock : unlock+100ms sleep");
				m5mo_set_lock(0);
				msleep(100);
			}
		//	m5mo_set_lock(val);
		}

		m5mo_set_lock(val);

		/* check AF status for 6 sec */
		for (i = 600; i > 0; i--) {
			msleep(10);
			err = m5mo_readb(M5MO_CATEGORY_LENS, M5MO_LENS_AF_STATUS, &status);
			CHECK_ERR(err);
#if defined (CONFIG_TARGET_SERIES_CELOX)
			if (!(status & 0x01) && i < 599) // it will be Valid status after 10ms for celox 
#else
			if (!(status & 0x01))  
#endif
				break;
		}

		m5mo_ctrl->focus.status = status;
		cam_info("%d, %d", m5mo_ctrl->focus.status, status );
		if ((m5mo_ctrl->focus.touch &&
			(m5mo_ctrl->focus.mode == FOCUS_MODE_TOUCH || m5mo_ctrl->focus.mode == FOCUS_MODE_TOUCH_MACRO ) )) {
			if (val && m5mo_ctrl->focus.lock) {
				m5mo_set_lock(0);
				CAM_DEBUG(" ######## m5mo_set_lock : unlock");
			}
		}

	}
	else {
		if (m5mo_ctrl->settings.sensor_mode == SENSOR_MOVIE){
			if (m5mo_ctrl->settings.preview_size.height < 720) {
				return 0;
			}
		}

		CAM_DEBUG("movie mode:CAF START/STOP");
				
		err = m5mo_writeb(M5MO_CATEGORY_LENS, M5MO_LENS_AF_START, val ? 0x02 : 0x00);
		CHECK_ERR(err);

		err = -EBUSY;
		for (i = 10; i >0; i--) {
			msleep(10);
			err = m5mo_readb(M5MO_CATEGORY_LENS, M5MO_LENS_AF_STATUS, &status);
			CHECK_ERR(err);

			if ((val && status == 0x05) || (!val && status != 0x05))
				break;
		}
	}
	m5mo_ctrl->focus.touchaf = 0;

	CAM_DEBUG("X");
	return err;

}



static int m5mo_set_touchaf_pos(u32 screen_x, u32 screen_y)
{
	//cam_info(" screenX : %d, screenY : %d",screen_x,screen_y);
	m5mo_ctrl->focus.pos_x = screen_x;
	m5mo_ctrl->focus.pos_y = screen_y;
	
	m5mo_ctrl->focus.touchaf = 1;   // JB, 
	
	return 0;
}


static int m5mo_set_af_mode(int val)
{
/*
0x00: Normal Mode(Init)
0x01: Macro Mode(Init)
0x02: CAF Mode
0x03: Face Detection Mode (Normal Mode)
0x04: Touch Normal Mode(Init)
0x05: Touch Macro Mode(Init)

0x07: Soft-landing Mode

*/
	u32 cancel, mode, status = 0;
	int i, err = 0;

	cancel = val & FOCUS_MODE_DEFAULT;
	CAM_DEBUG("%d",val);

	val &= 0xFF;
retry:	
	switch(val) {
	case FOCUS_MODE_AUTO:
	case FOCUS_MODE_FIXED:
		mode = 0x00;
		break;

	case FOCUS_MODE_MACRO:
		mode = 0x01;
		break;

	case FOCUS_MODE_CONTINOUS:
		mode = 0x02;
		cancel = 0;
		break;
		
	case FOCUS_MODE_FACEDETECT:
		mode = 0x03;
		break;
		
	case FOCUS_MODE_TOUCH:	
		mode = 0x04;
		cancel = 0;
		break;

	case FOCUS_MODE_TOUCH_MACRO:
		mode = 0x01;
		cancel = 0;
		break;

	case FOCUS_MODE_INFINITY:
		mode = 0x06;
		cancel = 0;
		break;
	
	default:
		cam_err("Invalid");
		val = FOCUS_MODE_AUTO;
		goto retry;			
	}

	if (cancel) {
		cam_info("AF cancel");
		m5mo_set_af(0);
		m5mo_set_lock(0);
	} else {
		if (m5mo_ctrl->focus.mode == val) {
			cam_err("focus mode: return");
			return 0;
		}
	}

	if (val == FOCUS_MODE_FACEDETECT) {
		/* enable face detection */
		err = m5mo_set_face_detection(1);
		CHECK_ERR(err);
		msleep(10);
	} else if (m5mo_ctrl->focus.mode == FOCUS_MODE_FACEDETECT) {
		/* disable face detection */
		err = m5mo_set_face_detection(0);
		CHECK_ERR(err);
	}

	m5mo_ctrl->focus.mode = val;

	err = m5mo_writeb(M5MO_CATEGORY_LENS, M5MO_LENS_AF_MODE, mode);
	CHECK_ERR(err);

	for (i = 100; i; i--) {
		msleep(10);
		err = m5mo_readb(M5MO_CATEGORY_LENS,M5MO_LENS_AF_STATUS, &status);
		CHECK_ERR(err);

		if (!(status & 0x01))
			break;
	}

	if ((status & 0x01) != 0x00) {
		cam_err("failed : TimeOut");
		return -ETIMEDOUT;
	}

	CAM_DEBUG("X");
	return 0;
}

static int m5mo_set_touch_auto_focus(int val)
{
	int err = 0;
	CAM_DEBUG("%s", val ? "start" : "stop");

	m5mo_ctrl->focus.touch = val;

	if (val) {
		m5mo_ctrl->focus.center = 0;
		CAM_DEBUG("focus_mode = %d", m5mo_ctrl->focus.mode);
		// AF Mode
		if (m5mo_ctrl->focus.mode == FOCUS_MODE_MACRO || m5mo_ctrl->focus.mode == FOCUS_MODE_TOUCH_MACRO ){
			CAM_DEBUG("MACRO TOUCH");
			err = m5mo_set_af_mode(FOCUS_MODE_TOUCH_MACRO); 
		}
		else {
			CAM_DEBUG("NORMAL TOUCH");
			err = m5mo_set_af_mode(FOCUS_MODE_TOUCH);
		}
		
		if (err < 0) {
			cam_err("m5mo_set_af_mode failed\n");
			return err;
		}
		// Set Position
		err = m5mo_writew(M5MO_CATEGORY_LENS,
				M5MO_LENS_AF_TOUCH_POSX, m5mo_ctrl->focus.pos_x);
		CHECK_ERR(err);
		err = m5mo_writew(M5MO_CATEGORY_LENS,
				M5MO_LENS_AF_TOUCH_POSY, m5mo_ctrl->focus.pos_y);
		CHECK_ERR(err);
	}
	else {
	
		if (m5mo_ctrl->focus.center) {
			CAM_DEBUG("center: focus_mode = %d", m5mo_ctrl->focus.mode);
			if (m5mo_ctrl->focus.mode == FOCUS_MODE_TOUCH_MACRO) {
				CAM_DEBUG("MACRO");
				err = m5mo_set_af_mode(FOCUS_MODE_MACRO); 
			}
			else if (m5mo_ctrl->focus.mode == FOCUS_MODE_TOUCH) {
				CAM_DEBUG("NORMAL");
				err = m5mo_set_af_mode(FOCUS_MODE_AUTO);
			}
		}
		m5mo_ctrl->focus.touchaf = 0;
	}

	CAM_DEBUG("X");
	return err;
}


static int m5mo_set_scene(int mode)
{
	int evp, metering, brightness, whitebalance, sharpness, saturation;
	int err = 0;

	CAM_DEBUG("%d",mode);

//	metering = METERING_CENTER;
	brightness = EV_DEFAULT;
//	whitebalance = WHITE_BALANCE_AUTO;
	sharpness = SHARPNESS_DEFAULT;
	saturation = CONTRAST_DEFAULT;

retry:
	switch (mode) {
	case SCENE_MODE_NONE:
		evp = 0x00;
		break;

	case SCENE_MODE_PORTRAIT:
		evp = 0x01;
		sharpness = SHARPNESS_MINUS_1;
		break;

	case SCENE_MODE_LANDSCAPE:
		evp = 0x02;
		metering = METERING_MATRIX;
		sharpness = SHARPNESS_PLUS_1;
		saturation = SATURATION_PLUS_1;
		break;

	case SCENE_MODE_SPORTS:
		evp = 0x03;
		break;

	case SCENE_MODE_PARTY_INDOOR:
		evp = 0x04;
		/*iso = ISO_200; sensor will set internally */
		saturation = SATURATION_PLUS_1;
		break;

	case SCENE_MODE_BEACH_SNOW:
		evp = 0x05;
		/*iso = ISO_50; sensor will set internally */
		brightness = EV_PLUS_2;
		saturation = SATURATION_PLUS_1;
		break;

	case SCENE_MODE_SUNSET:
		evp = 0x06;
		whitebalance = WHITE_BALANCE_SUNNY;
		break;

	case SCENE_MODE_DUSK_DAWN:
		evp = 0x07;
		whitebalance = WHITE_BALANCE_FLUORESCENT; 
		break;

	case SCENE_MODE_FALL_COLOR:
		evp = 0x08;
		saturation = SATURATION_PLUS_2;
		break;

	case SCENE_MODE_NIGHTSHOT:
		evp = 0x09;
		break;

	case SCENE_MODE_BACK_LIGHT:
		evp = 0x0A;
		break;

	case SCENE_MODE_FIREWORKS:
		evp = 0x0B;
		/*iso = ISO_50; sensor will set internally */
		break;

	case SCENE_MODE_TEXT:
		evp = 0x0C;
		sharpness = SHARPNESS_PLUS_2;
		break;

	case SCENE_MODE_CANDLE_LIGHT:
		evp = 0x0D;
		whitebalance = WHITE_BALANCE_SUNNY;
		break;

	default:
		cam_err("Invalid(%d)", mode);
		mode = SCENE_MODE_NONE;
		goto retry;
	}


	/* EV-P */
	err = m5mo_writeb(M5MO_CATEGORY_AE, M5MO_AE_EP_MODE_MON, evp);
	CHECK_ERR(err);
	err = m5mo_writeb(M5MO_CATEGORY_AE, M5MO_AE_EP_MODE_CAP, evp);
	CHECK_ERR(err);


	/* AE light meterig  */
//	m5mo_set_metering(metering);

	/* EV Bias */
	m5mo_set_ev(brightness);

	/* AWB */
//	m5mo_set_whitebalance(whitebalance);

	/* Chroma Saturation */
	m5mo_set_saturation(saturation);

	/* Sharpness */
	m5mo_set_sharpness(sharpness);

	/* Emotional Color */
	err = m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_MCC_MODE, 
		mode == SCENE_MODE_NONE ? 0x01 : 0x00);
	CHECK_ERR(err);

	m5mo_ctrl->settings.scenemode = mode;
	return err;
}

int get_camera_fw_id(int * fw_version)
{
	int rc = 0;
	unsigned int fw_ver = 0;
	unsigned int param_ver = 0;
	
	if( m5mo_readw(0x00, 0x02, &fw_ver) < 0 )
		return -1;

	(*fw_version) = fw_ver;

	/* parameter version */
	m5mo_readw(0x00, 0x06, &param_ver);

	printk("#################################\n");
	printk("#### Firmware version : %x \t####\n", fw_ver);
	printk("#### Parameter version : %x \t####\n", param_ver);
	printk("#################################\n");
	
	return rc;
}

static int m5mo_check_version(void)
{
	int i, val;
	unsigned int fw_ver = 0, param_ver = 0,  awb_ver = 0;

	for (i = 0; i < 6; i++) {
		m5mo_readb(M5MO_CATEGORY_SYS, M5MO_SYS_USER_VER, &val);
		m5mo_exif->unique_id[i] = (char)val;
	}
	m5mo_exif->unique_id[i] = '\0';
	
	m5mo_readw(M5MO_CATEGORY_SYS, M5MO_SYS_VER_FW, &fw_ver);
	m5mo_readw(M5MO_CATEGORY_SYS, M5MO_SYS_VER_PARAM, &param_ver);
	m5mo_readw(M5MO_CATEGORY_SYS, M5MO_SYS_VER_AWB, &awb_ver);

	
	printk("*************************************\n");
	printk("[M5MO] F/W Version: %s\n", m5mo_exif->unique_id);
	printk("[M5MO] Firmware Version: %x\n", fw_ver);
	printk("[M5MO] Parameter Version: %x\n", param_ver);
	printk("[M5MO] AWB Version: %x\n", awb_ver);
	printk("*************************************\n");

	return 0;
}


static int m5mo_mipi_mode(int mode)
{
	int rc = 0;
	struct msm_camera_csi_params m5mo_csi_params;
	CAM_DEBUG("%d",config_csi);

	if (!config_csi) {
		m5mo_csi_params.lane_cnt = 2;
		m5mo_csi_params.data_format = CSI_8BIT;
		m5mo_csi_params.lane_assign = 0xe4;
		m5mo_csi_params.dpcm_scheme = 0;
		m5mo_csi_params.settle_cnt = 0x2F; //0x14 ->0x2F// 24 -> 20
		rc = msm_camio_csi_config(&m5mo_csi_params);
		if (rc < 0)
			printk(KERN_ERR "config csi controller failed rc=%d\n",rc);
		config_csi = 1;
		
		msleep(100); //=> Please add some delay 
	}

	return rc;
}

static void m5mo_init_param(void)
{
	m5mo_ctrl->settings.scenemode = SCENE_MODE_NONE;
	m5mo_ctrl->settings.sensor_mode = SENSOR_CAMERA;
	m5mo_ctrl->settings.face_beauty = 0;
	
	m5mo_ctrl->settings.preview_size.width = 640;
	m5mo_ctrl->settings.preview_size.height = 480;
	m5mo_ctrl->settings.check_dataline = 0;
	m5mo_ctrl->settings.fps = 30;
	m5mo_ctrl->settings.started = 0;

	m5mo_ctrl->isp.bad_fw = 0;
	m5mo_ctrl->isp.issued = 0;
	m5mo_ctrl->focus.mode = FOCUS_MODE_MAX;
	m5mo_ctrl->focus.center = 0;
	
	m5mo_ctrl->focus.touchaf = 0;
}

static int m5mo_start(void) 
{
	int err = 0;
	int count = 0;
	u32 int_factor;

	CAM_DEBUG("E");

	m5mo_init_param();

	/* start camera */
	retry :
	err = m5mo_writeb(M5MO_CATEGORY_FLASH, M5MO_FLASH_CAM_START, 0x01);
	CHECK_ERR(err);

	int_factor = m5mo_wait_interrupt(M5MO_ISP_TIMEOUT);
	
	if (!(int_factor & M5MO_INT_MODE)) {
		cam_err("firmware was erased?\n");
		if (count == 0){
			count ++;
			cam_err (" retry again ");
			goto retry;
		}
		m5mo_ctrl->isp.bad_fw = 1;
		return -ENOSYS;
	}

	/* read firmware version */
	m5mo_check_version();	

	/* MIPI mode  */
	err=m5mo_writeb(M5MO_CATEGORY_PARM, M5MO_PARM_OUT_SEL, 0x02);   //MIPI
	CHECK_ERR(err);
	err = m5mo_writeb(M5MO_CATEGORY_PARM, M5MO_PARM_MIPI_DATA_TYPE, 0x01);	// data_type RAW8 Setting
	CHECK_ERR(err); 

	/* set monitor(preview) size - VGA*/
	cam_info("set Preview size as VGA");
	err = m5mo_writeb(M5MO_CATEGORY_PARM, M5MO_PARM_MON_SIZE, 0x17);	// 640x480
	CHECK_ERR(err);

	cam_info("set antibanding");

#if defined (CONFIG_JPN_MODEL_SC_03D)
	/* set auto flicker - 50Hz */
	err = m5mo_writeb(M5MO_CATEGORY_AE, M5MO_AE_FLICKER, 0x01);
#else
	/* set auto flicker - 60Hz */
	err = m5mo_writeb(M5MO_CATEGORY_AE, M5MO_AE_FLICKER, 0x02);
#endif
	CHECK_ERR(err);

	/* set capture mode */
	//err = m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_YUVOUT_MAIN, 0x00);
	err = m5mo_writeb(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_YUVOUT_MAIN, 0x20);	// main + Thumbnail JPEG + Thumbnail YUV
	CHECK_ERR(err);

	// set Thumbnail JPEG size
	err = m5mo_writel(M5MO_CATEGORY_CAPPARM, M5MO_CAPPARM_THUMB_JPEG_MAX, M5MO_THUMB_MAXSIZE);
	CHECK_ERR(err);
/*	m5mo_i2c_write_8bit(0x0B, 0x3F, 0x00);	
	m5mo_i2c_write_8bit(0x0B, 0x3E, 0xF0);	
	m5mo_i2c_write_8bit(0x0B, 0x3D, 0x00);	
	m5mo_i2c_write_8bit(0x0B, 0x3C, 0x00);	*/

	/* HDR */
	err = m5mo_writeb(M5MO_CATEGORY_CAPCTRL, M5MO_CAPCTRL_CAP_FRM_COUNT, 0x03);
	CHECK_ERR(err);
	
	/* FD */
#if 0
	err = m5mo_writeb(M5MO_CATEGORY_FD, M5MO_FD_SIZE, 0x01);
	CHECK_ERR(err);
	err = m5mo_writeb(M5MO_CATEGORY_FD, M5MO_FD_MAX, 0x0B);
	CHECK_ERR(err);
#endif
	
	m5mo_writeb(M5MO_CATEGORY_SYS, M5MO_SYS_INT_EN, M5MO_INT_MODE | M5MO_INT_CAPTURE | M5MO_INT_SOUND );

//	m5mo_set_mode(M5MO_MONITOR_MODE);	
	
	CAM_DEBUG("X");

	return err;
}


void m5mo_dump_registers(int category, int start_addr,int end_addr)
{
	int i=start_addr;
	unsigned int value=0;
	
	for(; i< end_addr; i++) {
		m5mo_readb(category, i, &value);
		printk("0x%x : 0x%x : 0x%x \n", category, i, value);
	}
}


static long m5mo_preview_mode(void)
{
	u32 old_mode, int_factor;
	CAM_DEBUG("E");
	if (m5mo_ctrl->focus.touchaf) {
		m5mo_set_touch_auto_focus(0);
	}
	
	old_mode = m5mo_set_mode(M5MO_MONITOR_MODE);

	if (old_mode <= 0) {
		cam_err("failed to set mode - old_mode %d\n", old_mode);
		return old_mode;
	}

	if (old_mode != M5MO_MONITOR_MODE) {
		int_factor = m5mo_wait_interrupt(M5MO_ISP_TIMEOUT);
		if (!(int_factor & M5MO_INT_MODE)) {
			cam_err("M5MO_INT_MODE isn't issued, %#x",int_factor);
			return -ETIMEDOUT;
		}
	}
	/* AE/AWB Unlock */
	m5mo_set_lock(0);
	
	if (!m5mo_ctrl->settings.started) { //ae/awb delay
		m5mo_ctrl->settings.started = 1;
		msleep(130); // 80 -> 130 
	}

	if (m5mo_ctrl->settings.check_dataline) {
		m5mo_check_dataline(m5mo_ctrl->settings.check_dataline);
	}
	CAM_DEBUG("X");
	return 0;
}

static long m5mo_snapshot_mode(void)
{
	u32 int_factor;
	
	m5mo_ctrl->focus.center = 1;
		
	m5mo_set_mode(M5MO_STILLCAP_MODE);
	
	int_factor = m5mo_wait_interrupt(M5MO_ISP_TIMEOUT);
	if (!(int_factor & M5MO_INT_SOUND)) {
		cam_err("M5MO_INT_SOUND isn't issued, %#x",int_factor);
		return -ETIMEDOUT;
	}
	return 0;
}


#ifdef FEATURE_CAMERA_HDR
static long m5mo_snapshot_mode_hdr(void)
{
	u32 int_factor;
	int int_en, i, err, enable=1;

	/* lock AE/AWB */
	m5mo_set_lock(1);
	
	// for HDR snapshot
	err = m5mo_writeb(M5MO_CATEGORY_CAPCTRL, M5MO_CAPCTRL_CAP_MODE, enable ? 0x06 : 0x00);
	CHECK_ERR(err);
	
	err = m5mo_writeb(M5MO_CATEGORY_CAPPARM,M5MO_CAPPARM_YUVOUT_MAIN, enable ? 0x00 : 0x21);
	CHECK_ERR(err);
	
	err = m5mo_readb(M5MO_CATEGORY_SYS, M5MO_SYS_INT_EN, &int_en);
	CHECK_ERR(err);
	
	if (enable)
		int_en |= M5MO_INT_FRAME_SYNC;
	else
		int_en &= ~M5MO_INT_FRAME_SYNC;
	
	err = m5mo_writeb(M5MO_CATEGORY_SYS, M5MO_SYS_INT_EN, int_en);
	CHECK_ERR(err);
	
	err = m5mo_set_mode(M5MO_STILLCAP_MODE);
	if (err <= 0) {
		cam_err("failed to set mode\n");
		return err;
	}
	
	/* convert raw to jpeg by the image data processing and store memory on ISP
			and receive preview jpeg image from ISP */
	for (i = 0; i < 3; i++) {
		int_factor = m5mo_wait_interrupt(M5MO_ISP_TIMEOUT);
		if (!(int_factor & M5MO_INT_FRAME_SYNC)) {
			cam_err("M5MO_INT_FRAME_SYNC isn't issued, %#x\n",int_factor);
			return -ETIMEDOUT;
		}
	}
	
	/* stop ring-buffer */
	if (!(m5mo_ctrl->isp.int_factor & M5MO_INT_CAPTURE)) {
		/* FIXME - M5MO_INT_FRAME_SYNC interrupt should be issued just three times */
		for (i = 0; i < 9; i++) {
			int_factor = m5mo_wait_interrupt(M5MO_ISP_TIMEOUT);
			if (int_factor & M5MO_INT_CAPTURE)
				break;
			cam_err("M5MO_INT_CAPTURE isn't issued, %#x\n", int_factor);
		}
	}

	
	return 0;
}
#endif

static long m5mo_snapshot_transfer_start(void)
{
	u32  int_factor;
	unsigned int main_size =0, thumb_size = 0;
	int err = 0;
	
	m5mo_ctrl->focus.center = 1;

	m5mo_ctrl->jpeg.main_size = 0;
	m5mo_ctrl->jpeg.thumb_size = 0;
	m5mo_ctrl->jpeg.jpeg_done = 0;

	if (!(m5mo_ctrl->isp.int_factor & M5MO_INT_CAPTURE)) {
		int_factor = m5mo_wait_interrupt(m5mo_ctrl->settings.face_beauty ? M5MO_ISP_AFB_TIMEOUT : M5MO_ISP_TIMEOUT);
		if (!(int_factor & M5MO_INT_CAPTURE)) {
			cam_err("M5MO_INT_CAPTURE isn't issued, %#x", int_factor);
			return -ETIMEDOUT;
		}
	}

	/* select main image */
	err = m5mo_writeb(M5MO_CATEGORY_CAPCTRL, M5MO_CAPCTRL_FRM_SEL, 0x01);		

	/* start trasnfer */
	err = m5mo_writeb(M5MO_CATEGORY_CAPCTRL, M5MO_CAPCTRL_TRANSFER, 0x01);

	int_factor = m5mo_wait_interrupt(M5MO_ISP_TIMEOUT);
	if (!(int_factor & M5MO_INT_CAPTURE)) {
		cam_err("M5MO_INT_CAPTURE isn't issued on transfer, %#x", int_factor);
		return -ETIMEDOUT;
	}

	err = m5mo_readl(M5MO_CATEGORY_CAPCTRL, M5MO_CAPCTRL_IMG_SIZE, &main_size);
	err = m5mo_readl(M5MO_CATEGORY_CAPCTRL, M5MO_CAPCTRL_THUMB_SIZE, &thumb_size);
	cam_info("##############size1: 0x%x, 0x%x", main_size, thumb_size);
	m5mo_ctrl->jpeg.main_size = main_size;
	m5mo_ctrl->jpeg.thumb_size = thumb_size;
	m5mo_ctrl->jpeg.jpeg_done = 1;

		/* get exif data */
	m5mo_get_exif();


#ifdef EARLY_PREVIEW_AFTER_CAPTURE
	CAM_DEBUG("Early preivew after capture ");
	msleep(50);
			
			/* start YUV output */
	m5mo_writeb( 0x00, 0x11, M5MO_INT_MODE);

	m5mo_writeb(0x00, 0x0B, M5MO_MONITOR_MODE);
	m5mo_i2c_verify(0x00, 0x0B, M5MO_MONITOR_MODE);
		
	/* AE/AWB Unlock */
	m5mo_set_lock(0);
		
	wait_interrupt_mode(M5MO_INT_MODE);
#endif

	return 0;

}


int m5mo_sensor_get_jpeg_size(int32_t size_cmd, unsigned int *size)
{
	int wait_count = 5;
	
	while ((--wait_count) > 0 && (m5mo_ctrl->jpeg.jpeg_done == 0)){
		mdelay(5);
	}

	CAM_DEBUG("jpeg done : %d", m5mo_ctrl->jpeg.jpeg_done);
	
	if (size_cmd == SIZE_MAIN) {
		(*size) =  m5mo_ctrl->jpeg.main_size;
	} else {
		(*size) = m5mo_ctrl->jpeg.thumb_size;
	}

	return 0;
}



static long m5mo_set_sensor_mode(int mode)
{
	if (m5mo_ctrl->isp.bad_fw){
		cam_err("\"Unknown\" state, please update F/W");
		return -ENOSYS;
	}

	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		CAM_DEBUG("SENSOR_PREVIEW_MODE START");
		m5mo_mipi_mode(mode);
		m5mo_preview_mode();
		break;

	case SENSOR_SNAPSHOT_MODE:
		CAM_DEBUG("SENSOR_SNAPSHOT_MODE START");
		m5mo_snapshot_mode();
		break;

	
	case SENSOR_SNAPSHOT_TRANSFER:
		CAM_DEBUG("SENSOR_SNAPSHOT_TRANSFER START");
		m5mo_snapshot_transfer_start();
		break;

	default:
		return -EFAULT;
	}

	return 0;
}


static int m5mo_mem_read(u16 len, u32 addr, u8 *val)
{

	struct i2c_msg msg;
	unsigned char data[8];
	unsigned char recv_data[len + 3];
	int i, err=0;

	if (!m5mo_client->adapter)
		return -ENODEV;
	
	if (len < 1) {
		printk("Data Length to read is out of range !\n");
		return -EINVAL;
	}
	
	msg.addr = m5mo_client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	/* high byte goes out first */
	data[0] = 0x00;
	data[1] = 0x03;
	data[2] = (addr >> 24) & 0xFF;
	data[3] = (addr >> 16) & 0xFF;
	data[4] = (addr >> 8) & 0xFF;
	data[5] = addr & 0xFF;
	data[6] = (len >> 8) & 0xFF;
	data[7] = len & 0xFF;

	for (i = M5MO_I2C_RETRY; i; i--) {
		err = i2c_transfer(m5mo_client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1)
		return err;

	msg.flags = I2C_M_RD;
	msg.len = sizeof(recv_data);
	msg.buf = recv_data;
	for (i = M5MO_I2C_RETRY; i; i--) {
		err = i2c_transfer(m5mo_client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	if (err != 1)
		return err;

	if (len != (recv_data[1] << 8 | recv_data[2]))
		CAM_DEBUG("expected length %d, but return length %d\n",
			len, recv_data[1] << 8 | recv_data[2]);

	memcpy(val, recv_data + 3, len);


	printk( "read from offset 0x%x \n", addr);
	return err;
	
}

static int m5mo_mem_write(u8 cmd, u16 len, u32 addr, u8 *val)
{
	struct i2c_msg msg;
	unsigned char data[len + 8];
	int i;
	int err = 0;

	if (!m5mo_client->adapter)
		return -ENODEV;

	msg.addr = m5mo_client->addr;
	msg.flags = 0;
	msg.len = sizeof(data);
	msg.buf = data;

	/* high byte goes out first */
	data[0] = 0x00;
	data[1] = cmd;//0x04;
	data[2] = (addr >> 24) & 0xFF;
	data[3] = (addr >> 16) & 0xFF;
	data[4] = (addr >> 8) & 0xFF;
	data[5] = addr & 0xFF;
	data[6] = (len >> 8) & 0xFF;
	data[7] = len & 0xFF;
	memcpy(data + 2 + sizeof(addr) + sizeof(len), val, len);

	for(i = M5MO_I2C_VERIFY_RETRY; i; i--) {
		err = i2c_transfer(m5mo_client->adapter, &msg, 1);
		if (err == 1)
			break;
		msleep(20);
	}

	return err;
}

static int m5mo_get_sensor_fw_version(char *buf)
{
	int8_t val;
	int err;

	CAM_DEBUG("E");
	/* set pin */
	val = 0x7E;
	err = m5mo_mem_write(0x04, sizeof(val), 0x50000308, &val);
	CHECK_ERR(err);

	err = m5mo_mem_read(M5MO_VERSION_INFO_SIZE+1,
		M5MO_FLASH_BASE_ADDR + 0x16FF00, buf);

	CAM_DEBUG("X  %s", buf);
	return 0;
}

static int m5mo_get_phone_fw_version(char *buf)
{
	char sensor_ver[M5MO_VERSION_INFO_SIZE+1] = {0, };
	int err;

	struct file *fp;
	mm_segment_t old_fs;
	long nread;
	int fw_requested = 1;

	old_fs = get_fs();
	set_fs(KERNEL_DS);


// Check -1: SD Card
#ifdef SDCARD_FW
	fp = filp_open(M5MO_FW_PATH_SDCARD, O_RDONLY, S_IRUSR);

	if (IS_ERR(fp)) {	// if there is no firmware in SDCARD
		cam_err("failed to open %s", M5MO_FW_PATH_SDCARD);
	}
	else {
		cam_err("update from SD Card");
		goto request_fw;
	}
#endif

// Check -2: Phone
	m5mo_get_sensor_fw_version(sensor_ver);
	CAM_DEBUG("sensor_ver %s \n", sensor_ver);

	if (sensor_ver[0] == 'S')
		fp = filp_open(M5MOS_FW_REQUEST_PATH, O_RDONLY, 0);
	else
		fp = filp_open(M5MOO_FW_REQUEST_PATH, O_RDONLY, 0);

	if (IS_ERR(fp)) {
		cam_err("failed to open %s", M5MOS_FW_REQUEST_PATH);
		err = -ENOENT;
		goto out;
	}
	cam_err("update from Phone Image");


request_fw:

	fw_requested = 0;
	err = vfs_llseek(fp, M5MO_VERSION_INFO_ADDR, SEEK_SET);
	if (err < 0) {
		CAM_DEBUG("failed to fseek, %d\n", err);
		goto out;
	}

	nread = vfs_read(fp, (char __user *)buf, M5MO_VERSION_INFO_SIZE+1, &fp->f_pos);
	if (nread != M5MO_VERSION_INFO_SIZE+1) {
		cam_err("failed to read firmware file, %ld Bytes\n", nread);
		err = -EIO;
		goto out;
	}

out:
#ifdef SDCARD_FW
	if (!fw_requested) {
		filp_close(fp, current->files);
		set_fs(old_fs);
	}
#endif 

	CAM_DEBUG("%s", buf);
	return 0;
}


static ssize_t m5mo_camera_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char type[] = "SONY_M5MO_NONE";

	return sprintf(buf, "%s\n", type);
}

static ssize_t m5mo_camera_fw_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	CAM_DEBUG("fw_version %s", m5mo_ctrl->fw_version);
	return sprintf(buf, "%s\n", m5mo_ctrl->fw_version);
}

static DEVICE_ATTR(camera_type, S_IRUGO, m5mo_camera_type_show, NULL);
static DEVICE_ATTR(camera_fw, S_IRUGO, m5mo_camera_fw_show, NULL);


static irqreturn_t m5mo_register_isp_irq(const struct msm_camera_sensor_info *data)
{
	int err = 0;

	m5mo_ctrl->isp.irq = 0;
	if(data->irq) {
		CAM_DEBUG("E");
		init_waitqueue_head(&m5mo_ctrl->isp.wait);
		err = request_irq(data->irq, m5mo_isp_isr, IRQF_TRIGGER_RISING, "m5mo_isp", NULL);
		m5mo_ctrl->isp.irq = 1;
	}
	
	return IRQ_HANDLED;
}


static int m5mo_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

#ifdef	FORCE_FIRMWARE_UPDATE
	struct file* fp;
#endif
#if 0 // -> msm_io_8x60.c, board-msm8x60_XXX.c
	CAM_DEBUG("POWER ON START ");
	
	CAM_DEBUG("POWER ON END ");
#endif	

#ifdef FORCE_FIRMWARE_UPDATE
	if (firmware_update_mode == 0 ) {
		if ( (fp = filp_open(M5MO_FW_PATH_SDCARD, O_RDONLY, S_IRUSR)) > 0) {
		printk(" #### FIRMWARE UPDATE MODE ####\n");
		firmware_update_mode = 1;
		filp_close(fp, current->files);

		m5mo_load_fw();
		}else {
			printk(" #### FIRMWARE UPDATE MODE - FAIL ####\n");
			return false;
		}
	}
	else
#endif
	{	
		m5mo_register_isp_irq(data);  
		rc =(int)m5mo_start();
		
		CAM_DEBUG("m5mo_start END %d", rc);

#ifdef CONFIG_FB_MSM_MDP_ADDITIONAL_BUS_SCALING
		CAM_DEBUG("require_exceptional_MDP_clock");
		require_exceptional_MDP_clock = 1;
#endif
	}

	return rc;
}


int m5mo_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	cam_err("E");

	/* Reset MIPI configuration flag */
	config_csi = 0;
	
	m5mo_ctrl = kzalloc(sizeof(struct m5mo_ctrl_t), GFP_KERNEL);
	if (!m5mo_ctrl) {
		printk("m5mo_sensor_open_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}	
	
	if (data) {
		m5mo_ctrl->sensordata = data;
	} else {
		printk("m5mo_sensor_open_init failed! msm_camera_sensor_info :NULL\n");
		rc = -ENOMEM;
		goto init_done;
	}

	m5mo_exif = kzalloc(sizeof(struct m5mo_exif_data), GFP_KERNEL);
	if (!m5mo_exif) {
		printk("allocate m5mo_exif_data failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

  	rc = m5mo_sensor_init_probe(data);
	
	if (rc < 0) {
		if (rc == -ENOSYS) { 
			printk("m5mo_sensor_open_init failed: please update the F/W\n");
			return 0;
		}
		else {
		printk("m5mo_sensor_open_init failed!\n");
		goto init_fail;
	}
	}

	cam_err("X");
init_done:
	return rc;

init_fail:
	if (m5mo_ctrl->isp.irq > 0){
		cam_err("init_fail : release irq");
		free_irq(m5mo_ctrl->sensordata->irq, NULL);
	}
	kfree(m5mo_ctrl);
	m5mo_ctrl = NULL;
	kfree(m5mo_exif);
	m5mo_exif = NULL;
	return rc;
}

static int m5mo_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&m5mo_wait_queue);
	return 0;
}


int m5mo_sensor_get_exif_data(int32_t *exif_cmd, int32_t *val)
{
	//cfg_data.cmd == EXT_CFG_SET_EXIF

	switch(*exif_cmd){	
		case EXIF_EXPOSURE_TIME:
			(*val) = m5mo_exif->exptime;
			break;
		case EXIF_TV:
			(*val) = m5mo_exif->tv;
			break;
		case EXIF_BV:
			(*val) = m5mo_exif->bv;
			break;
		case EXIF_EBV:
			(*val) = m5mo_exif->ebv;
			break;
		case EXIF_ISO:
			(*val) = m5mo_exif->iso;
			break;
		case EXIF_FLASH:
			(*val) = m5mo_exif->flash;
			break;
		default:
			cam_err("Invalid");
			break;
		}

	return 0;	
}

/* Define default F/W version.
 * - be cafeful of string size.
 */
#if defined(CONFIG_TARGET_SERIES_Q1)
#define DEFAULT_PHONE_FW_VER    "OOEI08 Fujitsu M5MOLS"
#elif defined (CONFIG_TARGET_SERIES_DALI)
#define DEFAULT_PHONE_FW_VER    "OLEI02 Fujitsu M5MOLS"
#elif defined (CONFIG_KOR_MODEL_SHV_E110S) 
#define DEFAULT_PHONE_FW_VER    "SDEH0D Fujitsu M5MOLS"
#else
#define DEFAULT_PHONE_FW_VER    "FAILED Fujitsu M5MOLS"
#endif

int m5mo_check_fw(void)
{
	char sensor_ver[M5MO_VERSION_INFO_SIZE+1] = "FAILED Fujitsu M5MOLS";
	char phone_ver[M5MO_VERSION_INFO_SIZE+1] = DEFAULT_PHONE_FW_VER;
	int af_cal_h = 0, af_cal_l = 0;
	int rg_cal_h = 0, rg_cal_l = 0;
	int bg_cal_h = 0, bg_cal_l = 0;
	int update_count = 0;
//	int32_t int_factor;
	int err;

	CAM_DEBUG("E");

	/* F/W version */
	m5mo_get_phone_fw_version(phone_ver);

	if (m5mo_ctrl->isp.bad_fw )
		goto out;
	
	m5mo_get_sensor_fw_version(sensor_ver);

	err = m5mo_writeb(M5MO_CATEGORY_FLASH, M5MO_FLASH_CAM_START, 0x01);
	CHECK_ERR(err);

	if (sensor_ver[0] == 'O'){
		m5mo_writeb(M5MO_CATEGORY_LENS, M5MO_LENS_AF_CAL_DATA_READ, 0x04);
		m5mo_writeb(M5MO_CATEGORY_LENS, M5MO_LENS_AF_CAL_DATA_READ, 0x05);
	}
	msleep(10);		// minimum 6ms 

	if (sensor_ver[0] == 'O'){
	CAM_DEBUG("sensor_ver");
		err = m5mo_readw(M5MO_CATEGORY_LENS, M5MO_LENS_AF_CAL_DATA, &af_cal_l);
	CAM_DEBUG("af_cal_l : %x", af_cal_l);
	}
	else{
	        err = m5mo_readb(M5MO_CATEGORY_LENS, M5MO_LENS_AF_CAL, &af_cal_l);
	}
	
	CHECK_ERR(err);

	err = m5mo_readb(M5MO_CATEGORY_ADJST, M5MO_ADJST_AWB_RG_H, &rg_cal_h);
	CHECK_ERR(err);
	err = m5mo_readb(M5MO_CATEGORY_ADJST, M5MO_ADJST_AWB_RG_L, &rg_cal_l);
	CHECK_ERR(err);

	err = m5mo_readb(M5MO_CATEGORY_ADJST, M5MO_ADJST_AWB_RG_H, &bg_cal_h);
	CHECK_ERR(err);
	err = m5mo_readb(M5MO_CATEGORY_ADJST, M5MO_ADJST_AWB_RG_L, &bg_cal_l);
	CHECK_ERR(err);

out:
	if (!m5mo_ctrl->fw_version) {
		m5mo_ctrl->fw_version = kzalloc(50, GFP_KERNEL);
		if (!m5mo_ctrl->fw_version) {
			cam_err("no memory for F/W version");
			return -ENOMEM;
		}
	}

	sprintf(m5mo_ctrl->fw_version, "%s %s %d %x %x %x %x %x %x",
		sensor_ver, phone_ver, update_count,
		af_cal_h, af_cal_l, rg_cal_h, rg_cal_l, bg_cal_h, bg_cal_l);
	CAM_DEBUG("fw_version %s", m5mo_ctrl->fw_version);
	CAM_DEBUG("X");
	return 0;
}


static int m5mo_firmware_update_mode(int mode)
{
	int   rc = 0;
	CAM_DEBUG(": %d", mode);

	firmware_update_mode = mode; //cfg_data.cmd;
		
	switch(firmware_update_mode) 
	{
	case READ_FW_VERSION:
		m5mo_check_fw();
		break;
		
	case UPDATE_M5MO_FW:
		cam_info("#### FIRMWARE UPDATE START ! ####");
		m5mo_reset_for_update();
			
		rc = m5mo_load_fw();
		if (rc == 0) {
			cam_info("#### FIRMWARE UPDATE SUCCEEDED ! ####");
			firmware_update_mode = 0;
		} else {
			cam_info("#### FIRMWARE UPDATE FAILED ! ####");
		}
		break;
				
	case READ_UPDATE_STATE:
		cam_info("#### FIRMWARE DUMP START ! ####");
		m5mo_reset_for_update();
				
		rc = m5mo_dump_fw();
		if (rc == 0) {
			cam_info("#### FIRMWARE DUMP SUCCEEDED ! ####");
			//firmware_update_mode = 0;
		} else {
			cam_info("#### FIRMWARE DUMP FAILED ! ####");
		}
		break;		
			
	}

	return 0;
}


int m5mo_sensor_ext_config(void __user *argp)
{
	sensor_ext_cfg_data cfg_data;
	int rc = 0;

	if (!m5mo_ctrl)
		return -EFAULT;
	
	if(copy_from_user((void *)&cfg_data, (const void *)argp, sizeof(cfg_data)))	{
		cam_err("copy_to_user Failed");
		return -EFAULT;
	}

	if (m5mo_ctrl->isp.bad_fw && cfg_data.cmd != EXT_CFG_SET_FIRMWARE_UPDATE){
		cam_err("\"Unknown\" state, please update F/W ");
		return -ENOSYS;
	}

	switch(cfg_data.cmd) {
	case EXT_CFG_SET_FIRMWARE_UPDATE:
		rc = m5mo_firmware_update_mode(cfg_data.value_1);			
		break;
		
	case EXT_CFG_SET_MOVIE_MODE:
		rc = m5mo_set_movie_mode(cfg_data.value_1);	
		break;
		
	case EXT_CFG_SET_FLASH:
		rc = m5mo_set_flash(cfg_data.value_1);
		break;
		
	case EXT_CFG_SET_SCENE:
		rc = m5mo_set_scene(cfg_data.value_1);
		break;
		
	case EXT_CFG_SET_EFFECT:
		rc = m5mo_set_effect(cfg_data.value_1);
		break;
		
	case EXT_CFG_SET_ISO:
		rc = m5mo_set_iso(cfg_data.value_1);
		break;

	case EXT_CFG_SET_WB:
		rc = m5mo_set_whitebalance(cfg_data.value_1);
		break;

	case EXT_CFG_SET_BRIGHTNESS:
		rc = m5mo_set_ev(cfg_data.value_1);
		break;

	case EXT_CFG_SET_ZOOM:
		rc = m5mo_set_zoom(cfg_data.value_1);
		break;

	case EXT_CFG_SET_FPS:
		rc = m5mo_set_fps(cfg_data.value_1);
		break;

	case EXT_CFG_SET_AF_MODE:
		rc = m5mo_set_af_mode(cfg_data.value_1);
		break;

	case EXT_CFG_SET_AF_START:
		rc = m5mo_set_af(1);
		break;
		
	case EXT_CFG_SET_AF_STOP:
		rc = m5mo_set_af(0);
		break;
		
	case EXT_CFG_GET_AF_STATUS:
		rc = m5mo_get_af_status(&cfg_data.value_1);
		break;		

	case EXT_CFG_SET_TOUCHAF_POS:	/* set touch AF position */
		rc = m5mo_set_touchaf_pos(cfg_data.value_1,cfg_data.value_2);
		break;
		
	case EXT_CFG_SET_TOUCHAF_MODE: /* set touch AF mode on/off */
		rc = m5mo_set_touch_auto_focus(cfg_data.value_1);
		break;

	case EXT_CFG_SET_METERING:	// auto exposure mode
		rc = m5mo_set_metering(cfg_data.value_1);
		break;

	case EXT_CFG_SET_PREVIEW_SIZE:	
		rc = m5mo_set_preview_size(cfg_data.value_1, cfg_data.value_2);
		break;

	case EXT_CFG_SET_PICTURE_SIZE:	
		rc = m5mo_set_picture_size(cfg_data.value_1,cfg_data.value_2);
		break;

	case EXT_CFG_SET_JPEG_QUALITY:	
		rc = m5mo_set_jpeg_quality(cfg_data.value_1);
		break;
		
	case EXT_CFG_SET_ANTISHAKE:
		rc = m5mo_set_antishake_mode(cfg_data.value_1);
		break;
		
	case EXT_CFG_SET_WDR:
		rc = m5mo_set_wdr(cfg_data.value_1);
		break;
		
	case EXT_CFG_SET_BEAUTY:
		rc = m5mo_set_face_beauty(cfg_data.value_1);
		break;
		
	case EXT_CFG_SET_DTP:
		m5mo_ctrl->settings.check_dataline = cfg_data.value_1;
		if(!m5mo_ctrl->settings.check_dataline)
			m5mo_check_dataline(cfg_data.value_1);
		break;
		
	case EXT_CFG_GET_JPEG_SIZE:
		rc = m5mo_sensor_get_jpeg_size(cfg_data.value_1, &cfg_data.value_2);	
		cam_info("size_%d : 0x%x", cfg_data.value_1, cfg_data.value_2);
		break;

	case EXT_CFG_GET_EXIF:
		rc = m5mo_sensor_get_exif_data(&cfg_data.value_1, &cfg_data.value_2);	
		break;

		
	case EXT_CFG_GET_SENSOR_FW_VER:
		strcpy(cfg_data.value_string, m5mo_exif->unique_id);
		break;
				
	default:
		CAM_DEBUG("cmd = %d, param1 = %d",cfg_data.cmd,cfg_data.value_1);
		break;
	}

	if(copy_to_user((void *)argp, (const void *)&cfg_data, sizeof(cfg_data))) {
		cam_err("copy_to_user Failed");
	}
	
	return rc;	
}

int m5mo_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	int rc = 0;

	if (copy_from_user(&cfg_data, (void *)argp, sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	if (!m5mo_ctrl)
		return -EFAULT;
	
	if (m5mo_ctrl->isp.bad_fw){
		cam_err("\"Unknown\" state, please update F/W");
		return -ENOSYS;
	}

	CAM_DEBUG("cfgtype = %d, mode = %d",cfg_data.cfgtype, cfg_data.mode);

	switch (cfg_data.cfgtype) {
	case CFG_SET_MODE:
		rc = m5mo_set_sensor_mode(cfg_data.mode);
		break;
		
	default:
		break;
	}

	return rc;
}

int m5mo_sensor_release(void)
{
	int rc = 0;

	if (m5mo_set_af_softlanding() < 0)
		cam_err("failed to set soft landing");
	
	mdelay(3);
#if 0	
	CAM_DEBUG("POWER OFF START");
	
	gpio_set_value_cansleep(CAM_VGA_RST, LOW);
	gpio_set_value_cansleep(CAM_8M_RST, LOW);
	mdelay(2);
	
	main_cam_ldo_power(OFF);
	
	CAM_DEBUG("POWER OFF END");
#endif

#ifdef CONFIG_FB_MSM_MDP_ADDITIONAL_BUS_SCALING
	require_exceptional_MDP_clock = 0;
#endif

	if (!m5mo_ctrl)
		goto release_done;
	
	if (m5mo_ctrl->isp.irq > 0) {
		//gpio_free(GPIO_ISP_INT);
		free_irq(m5mo_ctrl->sensordata->irq, NULL);
	}
	kfree(m5mo_ctrl);
        m5mo_ctrl = NULL;
	kfree(m5mo_exif);
        m5mo_exif = NULL;

release_done:
	return rc;
}

static int m5mo_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc = 0;

	CAM_DEBUG("E");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	m5mo_sensorw =
		kzalloc(sizeof(struct m5mo_work_t), GFP_KERNEL);

	if (!m5mo_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, m5mo_sensorw);
	m5mo_init_client(client);
	m5mo_client = client;

	CDBG("m5mo_i2c_probe successed!\n");
	CAM_DEBUG("%s X\n",__FUNCTION__);

	if (device_create_file(&client->dev, &dev_attr_camera_type) < 0) {
		CDBG("failed to create device file, %s\n",
			dev_attr_camera_type.attr.name);
	}

	if (device_create_file(&client->dev, &dev_attr_camera_fw) < 0) {
		CDBG("failed to create device file, %s\n",
			dev_attr_camera_fw.attr.name);
	}
#if defined (CONFIG_USA_MODEL_SGH_I757)
#define TORCH_EN		62
#define TORCH_SET		63
	gpio_tlmm_config(GPIO_CFG(TORCH_EN, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(TORCH_SET, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	gpio_set_value_cansleep(TORCH_EN, 0);
	gpio_set_value_cansleep(TORCH_SET, 0);
#endif
	return 0;

probe_failure:
	kfree(m5mo_sensorw);
	m5mo_sensorw = NULL;
	CDBG("m5mo_i2c_probe failed!\n");
	CAM_DEBUG("m5mo_i2c_probe failed!");
	return rc;
}


static int __exit m5mo_i2c_remove(struct i2c_client *client)
{
	struct m5mo_work_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
//	i2c_detach_client(client);

	device_remove_file(&client->dev, &dev_attr_camera_type);
	device_remove_file(&client->dev, &dev_attr_camera_fw);

	m5mo_client = NULL;
	m5mo_sensorw = NULL;
	kfree(m5mo_ctrl->fw_version);
	kfree(sensorw);
	return 0;
}


static const struct i2c_device_id m5mo_id[] = {
    { "m5mo_i2c", 0 },
    { }
};

//PGH MODULE_DEVICE_TABLE(i2c, m5mo);

static struct i2c_driver m5mo_i2c_driver = {
	.id_table	= m5mo_id,
	.probe  	= m5mo_i2c_probe,
	.remove 	= __exit_p(m5mo_i2c_remove),
	.driver 	= {
		.name = "m5mo",
	},
};


int32_t m5mo_i2c_init(void)
{
	int32_t rc = 0;

	CAM_DEBUG("E");

	rc = i2c_add_driver(&m5mo_i2c_driver);

	if (IS_ERR_VALUE(rc))
		goto init_failure;

	return rc;

init_failure:
	CDBG("failed to m5mo_i2c_init, rc = %d\n", rc);
	return rc;
}


void m5mo_exit(void)
{
	i2c_del_driver(&m5mo_i2c_driver);
}

//int m5mo_sensor_probe(void *dev, void *ctrl)
int m5mo_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{
	int rc = 0;

	CAM_DEBUG("E");

	rc = m5mo_i2c_init();
	if (rc < 0)
		goto probe_done;

	s->s_init	= m5mo_sensor_open_init;
	s->s_release	= m5mo_sensor_release;
	s->s_config	= m5mo_sensor_config;
	s->s_ext_config	= m5mo_sensor_ext_config;
	s->s_camera_type = BACK_CAMERA_2D;
	s->s_mount_angle = 0;
probe_done:
	CDBG("%s %s:%d\n", __FILE__, __func__, __LINE__);
	return rc;
	
}

static int m5mo_check_manufacturer_id(void)
{
	int i, err;
	u8 id = 0;
	u32 addr[] = {0x1000AAAA, 0x10005554, 0x1000AAAA};
	u8 val[3][2] = {
		[0] = {0x00, 0xAA},
		[1] = {0x00, 0x55},
		[2] = {0x00, 0x90},
	};
	u8 reset[] = {0x00, 0xF0};

	/* set manufacturer's ID read-mode */
	for (i = 0; i < 3; i++) {
		err = m5mo_mem_write(0x06, 2, addr[i], val[i]);
		CHECK_ERR(err);
	}

	/* read manufacturer's ID */
	err = m5mo_mem_read(sizeof(id), 0x10000001, &id);
	CHECK_ERR(err);

	/* reset manufacturer's ID read-mode */
	err = m5mo_mem_write(0x06, sizeof(reset), 0x10000000, reset);
	CHECK_ERR(err);

	CAM_DEBUG(": %d", id);

	return id;
}


static int m5mo_program_fw(u8* buf, u32 addr, u32 unit, u32 count, u8 id)
{
	u32 val;
	u32 intram_unit = 0x1000;//SZ_4K
	int i, j,retries, err = 0;

	int erase = 0x01;
	if (unit == SZ_64K && id != 0x01)
		erase = 0x04;


	CAM_DEBUG("-- start -- (id:%d)", id);
	for (i = 0; i < count; i++) {
		/* Set Flash ROM memory address */
	//	CAM_DEBUG("set addr : 0x%x (index = %d)", addr,i);	
		err = m5mo_writel(M5MO_CATEGORY_FLASH, M5MO_FLASH_ADDR, addr);
		CHECK_ERR(err);		

		/* Erase FLASH ROM entire memory */
		err = m5mo_writeb(M5MO_CATEGORY_FLASH, M5MO_FLASH_ERASE, erase);
		CHECK_ERR(err);
		
	//	CAM_DEBUG("erase Flash ROM");
		/* Response while sector-erase is operating */
		//m5mo_i2c_verify(M5MO_CATEGORY_FLASH, M5MO_FLASH_ERASE, 0x00);
		retries = 0;
		do {
			mdelay(50);
			err = m5mo_readb(M5MO_CATEGORY_FLASH, M5MO_FLASH_ERASE, &val);
			CHECK_ERR(err);
		} while (val == erase && retries++ < M5MO_I2C_VERIFY);


		/* Set FLASH ROM programming size */
		err = m5mo_writew(M5MO_CATEGORY_FLASH, M5MO_FLASH_BYTE, unit == SZ_64K ? 0 : unit);
		CHECK_ERR(err);
		msleep(10);
		/* Clear M-5MoLS internal RAM */
		err = m5mo_writeb(M5MO_CATEGORY_FLASH, M5MO_FLASH_RAM_CLEAR, 0x01);
		CHECK_ERR(err);
		msleep(10);
		/* Set Flash ROM programming address */
		err = m5mo_writel(M5MO_CATEGORY_FLASH, M5MO_FLASH_ADDR, addr);
		CHECK_ERR(err);
	
		/* Send programmed firmware */
		for (j = 0; j < unit; j += intram_unit) {
			err = m5mo_mem_write(0x04, intram_unit, M5MO_INT_RAM_BASE_ADDR + j, buf + (i * unit) + j);
			CHECK_ERR(err);
			msleep(10);
		}

		/* Start Programming */
		err = m5mo_writeb(M5MO_CATEGORY_FLASH, M5MO_FLASH_WR, 0x01);
		CHECK_ERR(err);
	//	CAM_DEBUG("start programming");
		msleep(50);
		
		/* Confirm programming has been completed */
		retries = 0;
		do {
			mdelay(50);
			err = m5mo_readb( M5MO_CATEGORY_FLASH, M5MO_FLASH_WR, &val);
			CHECK_ERR(err);
		} while (val && retries++ < M5MO_I2C_VERIFY);
				
		/* Increase Flash ROM memory address */
		addr += unit;
	}

	CAM_DEBUG("-- end --");
	return 0;
}


static int m5mo_reset_for_update(void)
{
	if (!m5mo_ctrl)
		return 0;
	
	CAM_DEBUG(" E ");
	msm_camio_sensor_reset((struct msm_camera_sensor_info *)m5mo_ctrl->sensordata);
	
	return 0;
}

static int m5mo_load_fw(void)
{
	//struct i2c_client *client = v4l2_get_subdevdata(sd);
	char sensor_ver[M5MO_VERSION_INFO_SIZE+1] = {0, };
	struct file *fp;
	mm_segment_t old_fs;
	u8 *buf = NULL, val;
	long fsize, nread;
	loff_t fpos = 0;
	int err, id;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

// Check -1: SD Card
#ifdef SDCARD_FW
	fp = filp_open(M5MO_FW_PATH_SDCARD, O_RDONLY, S_IRUSR);

	if (IS_ERR(fp)) {	// if there is no firmware in SDCARD
		cam_err("failed to open %s", M5MO_FW_PATH_SDCARD);
	}
	else {
		cam_err("update from SD Card");
		goto request_fw;
	}
	
#endif

// Check -2: Phone
	m5mo_get_sensor_fw_version(sensor_ver);
	CAM_DEBUG("sensor_ver %s", sensor_ver);

	if (sensor_ver[0] == 'S')
		fp = filp_open(M5MOS_FW_REQUEST_PATH, O_RDONLY, 0);
	else
		fp = filp_open(M5MOO_FW_REQUEST_PATH, O_RDONLY, 0);	


	if (IS_ERR(fp)) {
		cam_err("failed to open %s", M5MOS_FW_REQUEST_PATH);
		err = -ENOENT;
		goto out;
	}
	cam_err("update from Phone Image");


request_fw:
		
	fsize = fp->f_path.dentry->d_inode->i_size;
	
	buf = vmalloc(fsize);
	if (!buf) {
		printk("failed to allocate memory\n");
		goto out;
	}

	nread = vfs_read(fp, (char __user *)buf, fsize, &fpos);
	if (nread != fsize) {
		printk("failed to read firmware file, nread %ld Bytes\n", nread);
		goto out;
	}	

		
	/* set pin */
	val = 0x7E;
	err = m5mo_mem_write(0x04, sizeof(val), 0x50000308, &val);
	CHECK_ERR(err);
	

	id = m5mo_check_manufacturer_id();

	if (id < 0) {
		cam_err("manufacturer_id, err %d", id);
		goto out;
	}
	
	/* select flash memory */
	err = m5mo_writeb(M5MO_CATEGORY_FLASH, M5MO_FLASH_SEL, id == 0x01 ? 0x00 : 0x01);
	CHECK_ERR(err);
	
	/* program FLSH ROM */
	err = m5mo_program_fw(buf, M5MO_FLASH_BASE_ADDR, SZ_64K, 31, id);
	CHECK_ERR(err);

	if (id == 0x01) {
		err = m5mo_program_fw(buf, M5MO_FLASH_BASE_ADDR + SZ_64K * 31, SZ_8K, 4, id);
	} else {
		err = m5mo_program_fw(buf, M5MO_FLASH_BASE_ADDR + SZ_64K * 31, SZ_4K, 8, id);
	}
	CHECK_ERR(err);
	
out:	
	CAM_DEBUG("end");

	if (buf != NULL)	
	vfree(buf);

	filp_close(fp, current->files);
	set_fs(old_fs);

	return 0;
}

static int m5mo_dump_fw(void)
{
	struct file *fp;
	mm_segment_t old_fs;
	u8 *buf = NULL, val;
	u32 addr, unit, count, intram_unit = 0x1000;
	int i, j, err;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(M5MO_FW_DUMP_PATH,
		O_WRONLY|O_CREAT|O_TRUNC, S_IRUGO|S_IWUGO|S_IXUSR);
	if (IS_ERR(fp)) {
		cam_err("failed to open %s, err %ld\n",
			M5MO_FW_DUMP_PATH, PTR_ERR(fp));
		err = -ENOENT;
		goto out;
	}

	buf = kmalloc(intram_unit, GFP_KERNEL);
	if (!buf) {
		cam_err("failed to allocate memory\n");
		err = -ENOMEM;
		goto out;
	}

	CAM_DEBUG("start, file path %s\n", M5MO_FW_DUMP_PATH);

	/* set pin */
	val = 0x7E;
	err = m5mo_mem_write(0x04, sizeof(val), 0x50000308, &val);
	
	if (err < 0) {
		cam_err("set_pin : i2c falied, err %d\n", err);
		goto out;
        }
	addr = M5MO_FLASH_BASE_ADDR;
	unit = SZ_64K;
	count = 31;
	for (i = 0; i < count; i++) {
		for (j = 0; j < unit; j += intram_unit) {
			err = m5mo_mem_read(
				intram_unit, addr + (i * unit) + j, buf);
			if (err < 0) {
				cam_err("i2c falied, err %d\n", err);
				goto out;
			}
			vfs_write(fp, buf, intram_unit, &fp->f_pos);
		}
	}

	addr = M5MO_FLASH_BASE_ADDR + SZ_64K * count;
	unit = SZ_8K;
	count = 4;
	for (i = 0; i < count; i++) {
		for (j = 0; j < unit; j += intram_unit) {
			err = m5mo_mem_read(
				intram_unit, addr + (i * unit) + j, buf);
			if (err < 0) {
				cam_err("i2c falied, err %d\n", err);
				goto out;
			}
			vfs_write(fp, buf, intram_unit, &fp->f_pos);
		}
	}

	CAM_DEBUG("end\n");

out:
	if (buf != NULL)	
   	        kfree(buf);
	
	if (!IS_ERR(fp))
		filp_close(fp, current->files);
	set_fs(old_fs);

	return 0;
}

static int __sec_m5mo_probe(struct platform_device *pdev)
{
	printk("############# M5MO probe ##############\n");

	return msm_camera_drv_start(pdev, m5mo_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __sec_m5mo_probe,
//	.remove	 = msm_camera_remove,
	.driver = {
		.name = "msm_camera_m5mo",
		.owner = THIS_MODULE,
	},
};

#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

static int __init sec_m5mo_init(void)
{
#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return 0\n", __func__);
		return 0;
	}
#endif

	return platform_driver_register(&msm_camera_driver);
}


module_init(sec_m5mo_init);


