/*
  SEC S5K6AAFX
 */
/***************************************************************
CAMERA DRIVER FOR 2M CAM (SYS.LSI)
****************************************************************/

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>

#include "sec_s5k6aafx.h"
#include "sec_s5k6aafx_reg.h"

#include "sec_apex_pmic.h"
#include "sec_cam_dev.h"

#include <linux/clk.h>
#include <linux/io.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>

/*	Read setting file from SDcard
	- There must be no "0x" string in comment. If there is, it cause some problem.
*/
//#define CONFIG_LOAD_FILE

#define FACTORY_TEST 1

#ifndef CONFIG_LOAD_FILE
#define I2C_BURST_MODE
#endif

#ifdef CONFIG_LOAD_FILE
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

struct test {
	u8 data;
	struct test *nextBuf;
};
static struct test *testBuf;
static s32 large_file;


#define TEST_INIT	\
{			\
	.data = 0;	\
	.nextBuf = NULL;	\
}

#endif

struct s5k6aafx_work_t {
	struct work_struct work;
};

static struct  s5k6aafx_work_t *s5k6aafx_sensorw;
static struct  i2c_client *s5k6aafx_client;


static unsigned int config_csi2;

static struct s5k6aafx_ctrl_t *s5k6aafx_ctrl;

static DECLARE_WAIT_QUEUE_HEAD(s5k6aafx_wait_queue);
DECLARE_MUTEX(s5k6aafx_sem);

#ifdef CONFIG_LOAD_FILE
static int s5k6aafx_write_regs_from_sd(char *name);
static int s5k6aafx_regs_table_write(char *name);
#endif

static int s5k6aafx_start(void);


static int s5k6aafx_i2c_read(unsigned short subaddr, unsigned short *data)
{
	int ret;
	unsigned char buf[2];
	struct i2c_msg msg = { s5k6aafx_client->addr, 0, 2, buf };

	buf[0] = (subaddr >> 8);
	buf[1] = (subaddr & 0xFF);

	ret = i2c_transfer(s5k6aafx_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	if (ret == -EIO)
		goto error;

	msg.flags = I2C_M_RD;

	ret = i2c_transfer(s5k6aafx_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	if (ret == -EIO)
		goto error;

	*data = ((buf[0] << 8) | buf[1]);

error:
	return ret;
}


static int32_t s5k6aafx_i2c_write_32bit(unsigned short saddr, unsigned long packet)
		{
	int32_t rc = -EFAULT;
	int retry_count = 1;

	unsigned char buf[4];

	struct i2c_msg msg = {
		.addr = saddr,
		.flags = 0,
		.len = 4,
		.buf = buf,
	};

	*(unsigned long *)buf = cpu_to_be32(packet);

	while (retry_count--) {

		rc = i2c_transfer(s5k6aafx_client->adapter, &msg, 1);

		if(rc ==1)
			break;
			mdelay(10);
		}
	if (rc < 0) {
		cam_err("fail to write registers!!");
		return -EIO;
	}

	return 0;
}



static int s5k6aafx_i2c_write_list(const u32 *list, int size, char *name)
{
	int ret = 0;

#ifdef CONFIG_LOAD_FILE

	ret = s5k6aafx_write_regs_from_sd(name);
#else
	int i;
	u16 m_delay = 0;

	u32 temp_packet;


	CAM_DEBUG("%s, size=%d",name, size);
	for (i = 0; i < size; i++)
	{
		temp_packet = list[i];

		if ((temp_packet & 0xFFFF0000) == S5K6AAFX_DELAY)
		{
			m_delay = temp_packet & 0xFFFF;
			cam_info("delay = %d",m_delay );
			msleep(m_delay);
			continue;
		}

		if(s5k6aafx_i2c_write_32bit(s5k6aafx_client->addr, temp_packet) < 0)
		{
			printk("fail(0x%x, 0x%x:%d)", s5k6aafx_client->addr, temp_packet, i);
			return -EIO;
		}
		//udelay(10);
	}
#endif
	return ret;
}

#ifdef I2C_BURST_MODE
#define BURST_MODE_BUFFER_MAX_SIZE 1600

static int s5k6aafx_i2c_write_burst_list(const u32 *list, int size, char *name)
{
	int ret = -EAGAIN;
	u32 temp = 0;
	u16 delay = 0;
	int retry_count = 5;

	u16 addr, value;
	int len = 0;
	u8 buf[BURST_MODE_BUFFER_MAX_SIZE] = {0,};

	CAM_DEBUG("%s, size=%d",name, size);


	struct i2c_msg msg = {
		.addr = s5k6aafx_client->addr,
		.flags = 0,
		.len = 4,
		.buf = buf,
	};

	while (size--) {
		temp = *list++;

		if ((temp & S5K6AAFX_DELAY) == S5K6AAFX_DELAY) {
			delay = temp & 0xFFFF;
			cam_info("delay = %d",delay );
			msleep(delay);
			continue;
		}

		addr = temp >> 16;
		value = temp & 0xFFFF;

		switch(addr) {
			case 0x0F12:
				if (len == 0) {
					buf[len++] = addr >> 8;
					buf[len++] = addr & 0xFF;
				}
				buf[len++] = value >> 8;
				buf[len++] = value & 0xFF;

				if ((*list >> 16) != addr) {
					msg.len = len;
					//cam_info("msg.len = %d",msg.len );
					goto s5k6aafx_burst_write;
				}
				break;

			case 0xFFFF:
				break;

			default:
				msg.len = 4;
				*(u32 *)buf = cpu_to_be32(temp);
				goto s5k6aafx_burst_write;
		}

		continue;


s5k6aafx_burst_write:
		len = 0;

		retry_count = 5;

		while (retry_count--) {
			ret = i2c_transfer(s5k6aafx_client->adapter, &msg, 1);
			if (ret == 1)
				break;
			mdelay(10);
		}

		if (ret < 0) {
			cam_err("ERR - 0x%08x write failed err=%d\n", (u32)list, ret);
			break;
		}
	}

	if (ret < 0) {
		cam_err("fail to write registers!!\n");
		return -EIO;
	}

	return 0;
}
#endif


#ifdef CONFIG_LOAD_FILE
static inline int s5k6aafx_write(struct i2c_client *client,
		u32 packet)
{
	u8 buf[4];
	int err = 0, retry_count = 5;

	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.buf	= buf,
		.len	= 4,
	};

	if (!client->adapter) {
		cam_err("ERR - can't search i2c client adapter");
		return -EIO;
	}

	while (retry_count--) {
		*(u32 *)buf = cpu_to_be32(packet);
		err = i2c_transfer(client->adapter, &msg, 1);
		if (likely(err == 1))
			break;
		mdelay(10);
	}

	if (unlikely(err < 0)) {
		cam_err("ERR - 0x%08x write failed err=%d",(u32)packet, err);
		return err;
	}

	return (err != 1) ? -1 : 0;
}


void s5k6aafx_regs_table_init(void)
{
	struct file *fp = NULL;
	struct test *nextBuf = NULL;

	u8 *nBuf = NULL;
	size_t file_size = 0, max_size = 0, testBuf_size = 0;
	ssize_t nread = 0;
	s32 check = 0, starCheck = 0;
	s32 tmp_large_file = 0;
	s32 i = 0;
	int ret = 0;
	loff_t pos;

	mm_segment_t fs = get_fs();
	set_fs(get_ds());

	BUG_ON(testBuf);

	//fp = filp_open("/mnt/sdcard/external_sd/sec_s5k6aafx_reg.h", O_RDONLY, 0);
	fp = filp_open("/mnt/sdcard/sec_s5k6aafx_reg.h", O_RDONLY, 0);
	if (IS_ERR(fp)) {
		cam_err("failed to open /mnt/sdcard/sec_s5k6aafx_reg.h");
		return PTR_ERR(fp);
	}

	file_size = (size_t) fp->f_path.dentry->d_inode->i_size;
	max_size = file_size;

	cam_info("file_size = %d", file_size);

	nBuf = kmalloc(file_size, GFP_ATOMIC);
	if (nBuf == NULL) {
		cam_err("Fail to 1st get memory");
		nBuf = vmalloc(file_size);
		if (nBuf == NULL) {
			cam_err("ERR: nBuf Out of Memory");
			ret = -ENOMEM;
			goto error_out;
		}
		tmp_large_file = 1;
	}

	testBuf_size = sizeof(struct test) * file_size;
	if (tmp_large_file) {
		testBuf = (struct test *)vmalloc(testBuf_size);
		large_file = 1;
	} else {
		testBuf = kmalloc(testBuf_size, GFP_ATOMIC);
		if (testBuf == NULL) {
			cam_err("Fail to get mem(%d bytes)", testBuf_size);
			testBuf = (struct test *)vmalloc(testBuf_size);
			large_file = 1;
		}
	}
	if (testBuf == NULL) {
		cam_err("ERR: Out of Memory");
		ret = -ENOMEM;
		goto error_out;
	}

	pos = 0;
	memset(nBuf, 0, file_size);
	memset(testBuf, 0, file_size * sizeof(struct test));

	nread = vfs_read(fp, (char __user *)nBuf, file_size, &pos);
	if (nread != file_size) {
		cam_err("failed to read file ret = %d", nread);
		ret = -1;
		goto error_out;
	}

	set_fs(fs);

	i = max_size;

	cam_info("i = %d", i);

	while (i) {
		testBuf[max_size - i].data = *nBuf;
		if (i != 1) {
			testBuf[max_size - i].nextBuf = &testBuf[max_size - i + 1];
		} else {
			testBuf[max_size - i].nextBuf = NULL;
			break;
		}
		i--;
		nBuf++;
	}

	i = max_size;
	nextBuf = &testBuf[0];

#if 1
	while (i - 1) {
		if (!check && !starCheck) {
			if (testBuf[max_size - i].data == '/') {
				if (testBuf[max_size-i].nextBuf != NULL) {
					if (testBuf[max_size-i].nextBuf->data
								== '/') {
						check = 1;/* when find '//' */
						i--;
					} else if (testBuf[max_size-i].nextBuf->data == '*') {
						starCheck = 1;/* when find '/ *' */
						i--;
					}
				} else
					break;
			}
			if (!check && !starCheck) {
				/* ignore '\t' */
				if (testBuf[max_size - i].data != '\t') {
					nextBuf->nextBuf = &testBuf[max_size-i];
					nextBuf = &testBuf[max_size - i];
				}
			}
		} else if (check && !starCheck) {
			if (testBuf[max_size - i].data == '/') {
				if(testBuf[max_size-i].nextBuf != NULL) {
					if (testBuf[max_size-i].nextBuf->data == '*') {
						starCheck = 1; /* when find '/ *' */
						check = 0;
						i--;
					}
				} else
					break;
			}

			 /* when find '\n' */
			if (testBuf[max_size - i].data == '\n' && check) {
				check = 0;
				nextBuf->nextBuf = &testBuf[max_size - i];
				nextBuf = &testBuf[max_size - i];
			}

		} else if (!check && starCheck) {
			if (testBuf[max_size - i].data == '*') {
				if (testBuf[max_size-i].nextBuf != NULL) {
					if (testBuf[max_size-i].nextBuf->data == '/') {
						starCheck = 0; /* when find '* /' */
						i--;
					}
				} else
					break;
			}
		}

		i--;

		if (i < 2) {
			nextBuf = NULL;
			break;
		}

		if (testBuf[max_size - i].nextBuf == NULL) {
			nextBuf = NULL;
			break;
		}
	}
#endif

#if 0 // for print
	printk("i = %d\n", i);
	nextBuf = &testBuf[0];
	while (1) {
		//printk("sdfdsf\n");
		if (nextBuf->nextBuf == NULL)
			break;
		printk("%c", nextBuf->data);
		nextBuf = nextBuf->nextBuf;
	}
#endif

error_out:

	if (nBuf)
		tmp_large_file ? vfree(nBuf) : kfree(nBuf);
	if (fp)
		filp_close(fp, current->files);
	return ret;
}


void s5k6aafx_regs_table_exit(void)
{
	if (testBuf) {
		large_file ? vfree(testBuf) : kfree(testBuf);
		large_file = 0;
		testBuf = NULL;
	}
}

static int s5k6aafx_write_regs_from_sd(char *name)
{
	struct test *tempData = NULL;

	int ret = -EAGAIN;
	u32 temp;
	u32 delay = 0;
	u8 data[11];
	s32 searched = 0;
	size_t size = strlen(name);
	s32 i;

	CAM_DEBUG("E size = %d, string = %s", size, name);
	tempData = &testBuf[0];

	while (!searched) {
		searched = 1;
		for (i = 0; i < size; i++) {
			if (tempData->data != name[i]) {
				searched = 0;
				break;
			}
			tempData = tempData->nextBuf;
		}
		tempData = tempData->nextBuf;
	}
	/* structure is get..*/

	while (1) {
		if (tempData->data == '{')
			break;
		else
			tempData = tempData->nextBuf;
	}

	while (1) {
		searched = 0;
		while (1) {
			if (tempData->data == 'x') {
				/* get 10 strings.*/
				data[0] = '0';
				for (i = 1; i < 11; i++) {
					data[i] = tempData->data;
					tempData = tempData->nextBuf;
				}
				/*CAM_DEBUG("%s\n", data);*/
				temp = simple_strtoul(data, NULL, 16);
				break;
			} else if (tempData->data == '}') {
				searched = 1;
				break;
			} else
				tempData = tempData->nextBuf;

			if (tempData->nextBuf == NULL)
				return -1;
		}

		if (searched)
			break;
		if ((temp & S5K6AAFX_DELAY) == S5K6AAFX_DELAY) {
			delay = temp & 0xFFFF;
			cam_info("delay(%d)",delay);
			msleep(delay);
			continue;
		}
		ret = s5k6aafx_write(s5k6aafx_client,temp);

		/* In error circumstances */
		/* Give second shot */
		if (unlikely(ret)) {
			ret = s5k6aafx_write(s5k6aafx_client,temp);

			/* Give it one more shot */
			if (unlikely(ret)) {
				ret = s5k6aafx_write(s5k6aafx_client, temp);
			}
		}
	}
	return 0;
}
#endif

static void s5k6aafx_get_exif(void)
{
	unsigned char shutter_data0[2] = {0, 0};
	unsigned char shutter_data1[2] = {0, 0};
	unsigned int shutter_for_exif = 0;
	static char str_shutter_value[8];

	unsigned char iso_data[2] = {0, 0};
	int iso_value = 0;
	int iso_for_exif = 0, iso_exif = 0;
	static char str_iso_value[8];

	/* get shutter speed(exposure time) for exif */
	s5k6aafx_i2c_write_32bit(s5k6aafx_client->addr, 0x002C7000);
	s5k6aafx_i2c_write_32bit(s5k6aafx_client->addr, 0x002E1508);
	s5k6aafx_i2c_read(0x0F12, shutter_data0);
	s5k6aafx_i2c_write_32bit(s5k6aafx_client->addr, 0x002C7000);
	s5k6aafx_i2c_write_32bit(s5k6aafx_client->addr, 0x002E150A);
	s5k6aafx_i2c_read(0x0F12, shutter_data1);

	shutter_for_exif = ((shutter_data1[1] << 24)|(shutter_data1[0] << 16)|(shutter_data0[1] << 8)|(shutter_data0[0]));

	sprintf(str_shutter_value, "%d", shutter_for_exif);
	s5k6aafx_ctrl->exif_exptime = (simple_strtol( str_shutter_value, NULL, 10) / 500);

	/* get ISO for exif */
	s5k6aafx_i2c_write_32bit(s5k6aafx_client->addr, 0x002C7000);
	s5k6aafx_i2c_write_32bit(s5k6aafx_client->addr, 0x002E1508);
	s5k6aafx_i2c_read(0x0F12, iso_data);
	iso_exif = ((iso_data[1] << 8)|(iso_data[0]));

	sprintf(str_iso_value, "%d", iso_exif);
	iso_value = (simple_strtol( str_iso_value, NULL, 10) / 256);

	if(iso_value <= 50) {
		iso_for_exif = 50;
	} else if(iso_value > 50 && iso_value <= 100) {
		iso_for_exif = 100;
	} else if(iso_value > 100 && iso_value <= 200) {
		iso_for_exif = 200;
	} else if(iso_value > 200) {
		 iso_for_exif = 400;
	}

	s5k6aafx_ctrl->exif_iso = iso_for_exif;
}

static int s5k6aafx_check_dataline(s32 val)
{
	int err = 0;

	CAM_DEBUG("%s", val ? "ON" : "OFF");
	if (val) {
		err = s5k6aafx_i2c_write_list(s5k6aafx_pattern_on,
			sizeof(s5k6aafx_pattern_on)/sizeof(s5k6aafx_pattern_on[0]),"s5k6aafx_pattern_on");

	} else {
		err = s5k6aafx_i2c_write_list(s5k6aafx_pattern_off,
			sizeof(s5k6aafx_pattern_off)/sizeof(s5k6aafx_pattern_off[0]),"s5k6aafx_pattern_off");
	}

	return err;
}



static long s5k6aafx_video_config(int mode)
{
	int err = 0;

	err = s5k6aafx_i2c_write_list(s5k6aafx_preview,
		sizeof(s5k6aafx_preview) / sizeof(s5k6aafx_preview[0]), "s5k6aafx_preview");

	if (s5k6aafx_ctrl->check_dataline) {
		err = s5k6aafx_check_dataline(1);
	} else {
		mdelay(150);
	}

	return err;

}

static long s5k6aafx_snapshot_config(int mode)
{
	int err = 0;

	err = s5k6aafx_i2c_write_list(s5k6aafx_capture,
		sizeof(s5k6aafx_capture) / sizeof(s5k6aafx_capture[0]), "s5k6aafx_capture");

	s5k6aafx_get_exif();

	return err;
}



static long s5k6aafx_set_sensor_mode(int mode)
{
	int err = 0;
	printk("s5k6aafx_set_sensor_mode!\n");


	switch(mode) {
		case SENSOR_PREVIEW_MODE:
			CAM_DEBUG("SENSOR_PREVIEW_MODE START");

			err = s5k6aafx_start();
			if(err < 0) {
				printk("s5k6aafx_start failed!\n");
				return err;
			}

			if (s5k6aafx_ctrl->sensor_mode != SENSOR_MOVIE)
				err= s5k6aafx_video_config(SENSOR_PREVIEW_MODE);

			break;

		case SENSOR_SNAPSHOT_MODE:
			CAM_DEBUG("SENSOR_SNAPSHOT_MODE START");
			err = s5k6aafx_snapshot_config(SENSOR_SNAPSHOT_MODE);

			break;


		case SENSOR_SNAPSHOT_TRANSFER:
			CAM_DEBUG("SENSOR_SNAPSHOT_TRANSFER START");

			break;

		default:
			return 0;//-EFAULT;
	}

	return 0;
}


static int s5k6aafx_set_brightness(int8_t ev)
{
	int err = 0;

	CAM_DEBUG("%d",ev);

	if (s5k6aafx_ctrl->check_dataline || (s5k6aafx_ctrl->brightness == ev))
		return 0;

	switch(ev) {
		case EV_MINUS_4:
			err = s5k6aafx_i2c_write_list(s5k6aafx_bright_m4,
				sizeof(s5k6aafx_bright_m4)/sizeof(s5k6aafx_bright_m4[0]),"s5k6aafx_bright_m4");
			break;
		case EV_MINUS_3:
			err = s5k6aafx_i2c_write_list(s5k6aafx_bright_m3,
				sizeof(s5k6aafx_bright_m3)/sizeof(s5k6aafx_bright_m3[0]),"s5k6aafx_bright_m3");
			break;
		case EV_MINUS_2:
			err = s5k6aafx_i2c_write_list(s5k6aafx_bright_m2,
				sizeof(s5k6aafx_bright_m2)/sizeof(s5k6aafx_bright_m2[0]),"s5k6aafx_bright_m2");
			break;
		case EV_MINUS_1:
			err = s5k6aafx_i2c_write_list(s5k6aafx_bright_m1,
				sizeof(s5k6aafx_bright_m1)/sizeof(s5k6aafx_bright_m1[0]),"s5k6aafx_bright_m1");
			break;
		case EV_DEFAULT:
			err = s5k6aafx_i2c_write_list(s5k6aafx_bright_default,
				sizeof(s5k6aafx_bright_default)/sizeof(s5k6aafx_bright_default[0]),"s5k6aafx_bright_default");
			break;
		case EV_PLUS_1:
			err = s5k6aafx_i2c_write_list(s5k6aafx_bright_p1,
				sizeof(s5k6aafx_bright_p1)/sizeof(s5k6aafx_bright_p1[0]),"s5k6aafx_bright_p1");
			break;
		case EV_PLUS_2:
			err = s5k6aafx_i2c_write_list(s5k6aafx_bright_p2,
				sizeof(s5k6aafx_bright_p2)/sizeof(s5k6aafx_bright_p2[0]),"s5k6aafx_bright_p2");
			break;
		case EV_PLUS_3:
			err = s5k6aafx_i2c_write_list(s5k6aafx_bright_p3,
				sizeof(s5k6aafx_bright_p3)/sizeof(s5k6aafx_bright_p3[0]),"s5k6aafx_bright_p3");
			break;
		case EV_PLUS_4:
			err = s5k6aafx_i2c_write_list(s5k6aafx_bright_p4,
				sizeof(s5k6aafx_bright_p4)/sizeof(s5k6aafx_bright_p4[0]),"s5k6aafx_bright_p4");
			break;
		default:
			cam_err("invalid");
			break;
	}

	s5k6aafx_ctrl->brightness = ev;
	return err;
}

static int s5k6aafx_set_blur(int8_t blur)
{
	int err = 0;

	CAM_DEBUG("%d",blur);

	if (s5k6aafx_ctrl->check_dataline || (s5k6aafx_ctrl->blur == blur))
		return 0;

	switch(blur) {
		case BLUR_LEVEL_0:
			err = s5k6aafx_i2c_write_list(s5k6aafx_vt_pretty_default,
				sizeof(s5k6aafx_vt_pretty_default)/sizeof(s5k6aafx_vt_pretty_default[0]),"s5k6aafx_vt_pretty_default");
			break;
		case BLUR_LEVEL_1:
			err = s5k6aafx_i2c_write_list(s5k6aafx_vt_pretty_1,
				sizeof(s5k6aafx_vt_pretty_1)/sizeof(s5k6aafx_vt_pretty_1[0]),"s5k6aafx_vt_pretty_1");
			break;
		case BLUR_LEVEL_2:
			err = s5k6aafx_i2c_write_list(s5k6aafx_vt_pretty_2,
				sizeof(s5k6aafx_vt_pretty_2)/sizeof(s5k6aafx_vt_pretty_2[0]),"s5k6aafx_vt_pretty_2");
			break;
		case BLUR_LEVEL_3:
			err = s5k6aafx_i2c_write_list(s5k6aafx_vt_pretty_3,
				sizeof(s5k6aafx_vt_pretty_3)/sizeof(s5k6aafx_vt_pretty_3[0]),"s5k6aafx_vt_pretty_3");
			break;
		default:
			cam_err("invalid");
			break;
	}

	s5k6aafx_ctrl->blur = blur;

	return err;
}


static int s5k6aafx_set_flip(uint32_t flip)
{
	int err = 0;

	CAM_DEBUG("%d",flip);

#if defined (CONFIG_TARGET_LOCALE_KOR) || defined (CONFIG_JPN_MODEL_SC_03D) || defined (CONFIG_TARGET_LOCALE_JPN)
	if(s5k6aafx_ctrl->check_dataline)
		return 0;

	switch(flip) {
		case 180:
			err = s5k6aafx_i2c_write_list(s5k6aafx_vflip,
				sizeof(s5k6aafx_vflip)/sizeof(s5k6aafx_vflip[0]),"s5k6aafx_vflip");
			break;
		default:
			cam_err("invalid");
			break;
	}
#endif
return err;

}


static int s5k6aafx_set_frame_rate( unsigned int fps)
{
	int err = 0;

	CAM_DEBUG("fps =%d", fps);

	if (s5k6aafx_ctrl->vtcall_mode == 0) { //normal
		return 0;
	}

	switch (fps) {
		case 7:
			err = s5k6aafx_i2c_write_list(s5k6aafx_vt_7fps,
					sizeof(s5k6aafx_vt_7fps)/sizeof(s5k6aafx_vt_7fps[0]), "s5k6aafx_vt_7fps");
			break;
		case 10:
			err = s5k6aafx_i2c_write_list(s5k6aafx_vt_10fps,
					sizeof(s5k6aafx_vt_10fps)/sizeof(s5k6aafx_vt_10fps[0]), "s5k6aafx_vt_10fps");

			break;
		case 12:
			err = s5k6aafx_i2c_write_list(s5k6aafx_vt_12fps,
					sizeof(s5k6aafx_vt_12fps)/sizeof(s5k6aafx_vt_12fps[0]), "s5k6aafx_vt_12fps");

			break;
		case 15:
			err = s5k6aafx_i2c_write_list(s5k6aafx_vt_15fps,
					sizeof(s5k6aafx_vt_15fps)/sizeof(s5k6aafx_vt_15fps[0]), "s5k6aafx_vt_15fps");
			break;
		case 30:
			//cam_warn("frame rate is 30\n");
			break;
		default:
			//cam_err("ERR: Invalid framerate\n");
			break;
	}

	return err;

}

static int s5k6aafx_mipi_mode(int mode)
{
	int rc = 0;
	struct msm_camera_csi_params s5k6aafx_csi_params;

	CAM_DEBUG("E");

	if (!config_csi2) {
#if 0
		s5k6aafx_csi_params.lane_cnt = 1;
		s5k6aafx_csi_params.data_format = CSI_8BIT;
		s5k6aafx_csi_params.lane_assign = 0xe4;
		s5k6aafx_csi_params.dpcm_scheme = 0;
		s5k6aafx_csi_params.settle_cnt =  0x14; // 24 -> 20
		rc = msm_camio_csi_config(&s5k6aafx_csi_params);
#endif
		s5k6aafx_csi_params.lane_cnt = 1;
		s5k6aafx_csi_params.data_format = 0x1E; //CSI_8BIT;
		s5k6aafx_csi_params.lane_assign = 0xe4;
		s5k6aafx_csi_params.dpcm_scheme = 0;
		s5k6aafx_csi_params.settle_cnt = 0x07; //24;// 0x14; //0x7; //0x14;
		rc = msm_camio_csi_config(&s5k6aafx_csi_params);
		if (rc < 0)
			printk(KERN_ERR "config csi controller failed \n");
		config_csi2 = 1;
	}
	CAM_DEBUG("X");
	return rc;
}


static int s5k6aafx_start(void)
{
	int err = 0;

	CAM_DEBUG("E");

	if(s5k6aafx_ctrl->started)
	{
		CAM_DEBUG("X : already started");
		return 0;
	}
	s5k6aafx_mipi_mode(1);
	msleep(30); //=> Please add some delay
	printk("mipi done");


	printk("mode %d, %d", s5k6aafx_ctrl->sensor_mode,s5k6aafx_ctrl->vtcall_mode);

	if (s5k6aafx_ctrl->sensor_mode == SENSOR_CAMERA) {
		if(s5k6aafx_ctrl->vtcall_mode == 0)  //normal
		{
			CAM_DEBUG("SELF CAMERA");
#ifdef I2C_BURST_MODE
			err = s5k6aafx_i2c_write_burst_list(s5k6aafx_common,
				sizeof(s5k6aafx_common)/sizeof(s5k6aafx_common[0]),"s5k6aafx_common");
#else
			err = s5k6aafx_i2c_write_list(s5k6aafx_common,
				sizeof(s5k6aafx_common)/sizeof(s5k6aafx_common[0]),"s5k6aafx_common");
#endif
		}
		else if(s5k6aafx_ctrl->vtcall_mode == 1) //vt call
		{
			CAM_DEBUG("VT CALL");
			err = s5k6aafx_i2c_write_list(s5k6aafx_vt_common,
				sizeof(s5k6aafx_vt_common)/sizeof(s5k6aafx_vt_common[0]),"s5k6aafx_vt_common");
		}
		else //wifi vt call
		{
			CAM_DEBUG("WIFI VT CALL");
			err = s5k6aafx_i2c_write_list(s5k6aafx_vt_wifi_common,
				sizeof(s5k6aafx_vt_wifi_common)/sizeof(s5k6aafx_vt_wifi_common[0]),"s5k6aafx_vt_wifi_common");

		}
	}
	else {
		CAM_DEBUG("SELF RECORD");

#if defined (CONFIG_JPN_MODEL_SC_03D) //50Hz
#ifdef I2C_BURST_MODE
		err = s5k6aafx_i2c_write_burst_list(s5k6aafx_recording_50Hz_common,
			sizeof(s5k6aafx_recording_50Hz_common)/sizeof(s5k6aafx_recording_50Hz_common[0]),"s5k6aafx_recording_50Hz_common");
#else
		err = s5k6aafx_i2c_write_list(s5k6aafx_recording_50Hz_common,
			sizeof(s5k6aafx_recording_50Hz_common)/sizeof(s5k6aafx_recording_50Hz_common[0]),"s5k6aafx_recording_50Hz_common");
#endif


#else //60Hz
#ifdef I2C_BURST_MODE
		err = s5k6aafx_i2c_write_burst_list(s5k6aafx_recording_60Hz_common,
			sizeof(s5k6aafx_recording_60Hz_common)/sizeof(s5k6aafx_recording_60Hz_common[0]),"s5k6aafx_recording_60Hz_common");
#else
		err = s5k6aafx_i2c_write_list(s5k6aafx_recording_60Hz_common,
			sizeof(s5k6aafx_recording_60Hz_common)/sizeof(s5k6aafx_recording_60Hz_common[0]),"s5k6aafx_recording_60Hz_common");
#endif


#endif
	}

	s5k6aafx_ctrl->initialized = 1;
	s5k6aafx_ctrl->started = 1;
	CAM_DEBUG("X");
	return err;
}

static int s5k6aafx_set_movie_mode(int mode)
{
	if ((mode != SENSOR_CAMERA) && (mode != SENSOR_MOVIE)) {
		return -EINVAL;
	}
	/* We does not support movie mode when in VT. */
	if ((mode == SENSOR_MOVIE) && s5k6aafx_ctrl->vtcall_mode) {
		s5k6aafx_ctrl->sensor_mode = SENSOR_CAMERA;
	} else
		s5k6aafx_ctrl->sensor_mode = mode;

	CAM_DEBUG("mode %d, %d", s5k6aafx_ctrl->sensor_mode,s5k6aafx_ctrl->vtcall_mode);
	return 0;
}

#ifdef FACTORY_TEST
extern struct class *camera_class;
struct device *s5k6aafx_dev;

static ssize_t camtype_file_cmd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char camType[] = "LSI_S5K6AAFX_NONE";

	return sprintf(buf, "%s", camType);
}

static ssize_t camtype_file_cmd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(front_camtype, 0660, camtype_file_cmd_show, camtype_file_cmd_store);

#endif

static int s5k6aafx_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
	int rc = 0;
	printk("s5k6aafx_sensor_init_probe start");


	sub_cam_ldo_power(ON);
	mdelay(1);

	msm_camio_clk_rate_set(24000000);
	udelay(50);
	

	printk("3. CAM_VGA_RST HIGH\n");
	gpio_set_value_cansleep(CAM_FRONT_RST, 1);// VT RST
	mdelay(1);

	printk("4. CAM_5M_RST LOW\n ");
	gpio_set_value_cansleep(CAM_5M_RST, 0);// 5M RST
	mdelay(6);

#ifdef CONFIG_LOAD_FILE
	s5k6aafx_regs_table_init();
#endif

	CAM_DEBUG("POWER ON END ");

	return rc;
}


int s5k6aafx_get_exif_data(int32_t *exif_cmd, int32_t *val)
{
	//cfg_data.cmd == EXT_CFG_SET_EXIF
	switch(*exif_cmd) {
		case EXIF_TV:
			(*val) = s5k6aafx_ctrl->exif_exptime;
			break;

		case EXIF_ISO:
			(*val) = s5k6aafx_ctrl->exif_iso;
			break;

		default:
			cam_err("Invalid");
			break;
	}

	return 0;
}

int s5k6aafx_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	CAM_DEBUG("E");

	config_csi2 = 0;

	s5k6aafx_ctrl = kzalloc(sizeof(struct s5k6aafx_ctrl_t), GFP_KERNEL);
	if (!s5k6aafx_ctrl) {
		cam_err("s5k6aafx_ctrl failed!");
		rc = -ENOMEM;
		goto init_done;
	}

	if (!data) {
		cam_err("data failed!");
		rc = -ENOMEM;
		goto init_fail;
	}

	s5k6aafx_ctrl->sensordata = data;

	rc = s5k6aafx_sensor_init_probe(data);
	if (rc < 0) {
		cam_err("s5k6aafx_sensor_open_init failed!");
		goto init_fail;
	}

	s5k6aafx_ctrl->started = 0;
	s5k6aafx_ctrl->check_dataline = 0;
	s5k6aafx_ctrl->sensor_mode = SENSOR_CAMERA;
	s5k6aafx_ctrl->brightness = EV_DEFAULT;
	s5k6aafx_ctrl->blur = BLUR_LEVEL_0;
	s5k6aafx_ctrl->exif_exptime = 0;
	s5k6aafx_ctrl->exif_iso = 0;

	CAM_DEBUG("X");
init_done:
	return rc;

init_fail:
	kfree(s5k6aafx_ctrl);
	s5k6aafx_ctrl = NULL;
	return rc;
}

static int s5k6aafx_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&s5k6aafx_wait_queue);
	return 0;
}



int s5k6aafx_sensor_ext_config(void __user *argp)
{
	sensor_ext_cfg_data cfg_data;
	int rc = 0;
    printk("s5k6aafx_sensor_ext_config");

	if (!s5k6aafx_ctrl)
		return -ENOSYS;

	if(copy_from_user((void *)&cfg_data, (const void *)argp, sizeof(cfg_data)))
	{
		cam_err("fail copy_from_user!");
		return -EFAULT;
	}

	CAM_DEBUG("cmd = %d , param1 = %d",cfg_data.cmd,cfg_data.value_1);
	if( (cfg_data.cmd != EXT_CFG_SET_DTP)
		&& (cfg_data.cmd != EXT_CFG_SET_VT_MODE)
		&& (cfg_data.cmd != EXT_CFG_SET_MOVIE_MODE)
		&& (!s5k6aafx_ctrl->initialized)){
		cam_err("camera isn't initialized\n");
		return 0;
	}

	switch(cfg_data.cmd) {
		case EXT_CFG_SET_BRIGHTNESS:
			rc = s5k6aafx_set_brightness(cfg_data.value_1);
			break;

		case EXT_CFG_SET_BLUR:
			rc = s5k6aafx_set_blur(cfg_data.value_1);
			break;

		case EXT_CFG_SET_DTP:
			s5k6aafx_ctrl->check_dataline = cfg_data.value_1;

			if (s5k6aafx_ctrl->check_dataline == 0)
				rc = s5k6aafx_check_dataline(0);
			break;

		case EXT_CFG_SET_FPS:
			//rc = s5k6aafx_set_fps(cfg_data.value_1,cfg_data.value_2);
			rc = s5k6aafx_set_frame_rate(cfg_data.value_1);
			break;

#if 0
		case EXT_CFG_SET_FRONT_CAMERA_MODE:
			CAM_DEBUG("VTCall mode : %d",cfg_data.value_1);
			s5k6aafx_ctrl->vtcall_mode = cfg_data.value_1;
			break;
#endif

		case EXT_CFG_SET_VT_MODE:
			cam_info("VTCall mode : %d",cfg_data.value_1);
			s5k6aafx_ctrl->vtcall_mode = cfg_data.value_1;
			break;

		case EXT_CFG_SET_MOVIE_MODE:
			cam_info("MOVIE mode : %d",cfg_data.value_1);
			rc = s5k6aafx_set_movie_mode(cfg_data.value_1);
			break;

		case EXT_CFG_SET_FLIP:
			rc = s5k6aafx_set_flip(cfg_data.value_1);
			break;


		case EXT_CFG_GET_EXIF:
			rc = s5k6aafx_get_exif_data(&cfg_data.value_1, &cfg_data.value_2);
			break;

		default:
			break;
	}

	if (copy_to_user((void *)argp, (const void *)&cfg_data, sizeof(cfg_data))){
		cam_err("fail copy_from_user!");
	}

	return rc;
}

int s5k6aafx_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	int rc = 0;

	if (copy_from_user(&cfg_data,(void *)argp, sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	if (!s5k6aafx_ctrl)
		return -ENOSYS;

	CAM_DEBUG("cfgtype = %d, mode = %d", cfg_data.cfgtype, cfg_data.mode);

	switch (cfg_data.cfgtype) {
		case CFG_SET_MODE:
			rc = s5k6aafx_set_sensor_mode(cfg_data.mode);
			break;
		default:
			break;
	}

	return rc;
}

int s5k6aafx_sensor_release(void)
{
	int rc = 0;

	/* down(&m5mo_sem); */

	printk("POWER OFF");
	printk("camera turn off\n");

//VT RESET
	printk(" 1. CAM_VT_RST OFF");
	gpio_set_value_cansleep(CAM_FRONT_RST, 0);// VT RST
	udelay(50);


//VT STBY
	printk(" 2. CAM_VT_STBY OFF");
	gpio_set_value_cansleep(CAM_FRONT_EN, 0);// VT  RST
	mdelay(1);

//5M RST
	printk(" 3. CAM_5M_RST OFF");// 5M RST
	gpio_set_value_cansleep(CAM_5M_RST, 0);// VT  RST
	mdelay(1);

//power off
	sub_cam_ldo_power(OFF); // power off
	mdelay(1);

	kfree(s5k6aafx_ctrl);

#ifdef CONFIG_LOAD_FILE
	s5k6aafx_regs_table_exit();
#endif
	/* up(&m5mo_sem); */

	return rc;
}

static int s5k6aafx_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc = 0;

	CAM_DEBUG("E");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	s5k6aafx_sensorw =
		kzalloc(sizeof(struct s5k6aafx_work_t), GFP_KERNEL);

	if (!s5k6aafx_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, s5k6aafx_sensorw);
	s5k6aafx_init_client(client);
	s5k6aafx_client = client;

#ifdef FACTORY_TEST
	s5k6aafx_dev = device_create(camera_class, NULL, 0, NULL, "front");
	if (IS_ERR(s5k6aafx_dev)) {
		pr_err("Failed to create device!");
		goto probe_failure;
	}

       if (device_create_file(s5k6aafx_dev, &dev_attr_front_camtype) < 0) {
                CDBG("failed to create device file, %s\n",
                        dev_attr_front_camtype.attr.name);
        }
#endif

	CAM_DEBUG("X");
	return 0;

probe_failure:
	kfree(s5k6aafx_sensorw);
	s5k6aafx_sensorw = NULL;
	cam_err("s5k6aafx_i2c_probe failed!");
	return rc;
}


static int __exit s5k6aafx_i2c_remove(struct i2c_client *client)
{

	struct s5k6aafx_work_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
//	i2c_detach_client(client);
#ifdef FACTORY_TEST
	device_remove_file(s5k6aafx_dev, &dev_attr_front_camtype);
#endif
	s5k6aafx_client = NULL;
	s5k6aafx_sensorw = NULL;
	kfree(sensorw);
	return 0;
}


static const struct i2c_device_id s5k6aafx_id[] = {
    { "s5k6aafx_i2c", 0 },
    { }
};

//PGH MODULE_DEVICE_TABLE(i2c, s5k6aafx);

static struct i2c_driver s5k6aafx_i2c_driver = {
	.id_table	= s5k6aafx_id,
	.probe  	= s5k6aafx_i2c_probe,
	.remove 	= __exit_p(s5k6aafx_i2c_remove),
	.driver 	= {
		.name = "s5k6aafx",
	},
};


int32_t s5k6aafx_i2c_init(void)
{
	int32_t rc = 0;

	CAM_DEBUG("E");

	rc = i2c_add_driver(&s5k6aafx_i2c_driver);

	if (IS_ERR_VALUE(rc))
		goto init_failure;

	return rc;

init_failure:
	cam_err("failed to s5k6aafx_i2c_init, rc = %d", rc);
	return rc;
}


void s5k6aafx_exit(void)
{
	i2c_del_driver(&s5k6aafx_i2c_driver);
}


int s5k6aafx_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{
	int rc = 0;

	rc = s5k6aafx_i2c_init();

	if (rc < 0)
		goto probe_done;

	s->s_init	= s5k6aafx_sensor_open_init;
	s->s_release	= s5k6aafx_sensor_release;
	s->s_config	= s5k6aafx_sensor_config;
	s->s_ext_config	= s5k6aafx_sensor_ext_config;
	s->s_camera_type = FRONT_CAMERA_2D;
	s->s_mount_angle = 270;

probe_done:
	cam_err("Sensor probe done(%d)", rc);
	return rc;

}

#if 0
static int msm_vga_camera_remove(struct platform_device *pdev)
{
	return msm_camera_drv_remove(pdev);
}
#endif

static int __sec_s5k6aafx_probe(struct platform_device *pdev)
{
	printk("############# s5k6aafx probe ##############\n");

	return msm_camera_drv_start(pdev, s5k6aafx_sensor_probe);
}

static struct platform_driver msm_vga_camera_driver = {
	.probe = __sec_s5k6aafx_probe,
//	.remove	 = msm_vga_camera_remove,
	.driver = {
		.name = "msm_camera_s5k6aafx",
		.owner = THIS_MODULE,
	},
};

#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

static int __init sec_s5k6aafx_camera_init(void)
{
#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return 0\n", __func__);
		return 0;
	}
#endif

	return platform_driver_register(&msm_vga_camera_driver);
}

static void __exit sec_s5k6aafx_camera_exit(void)
{
	platform_driver_unregister(&msm_vga_camera_driver);
}

module_init(sec_s5k6aafx_camera_init);
module_exit(sec_s5k6aafx_camera_exit);



