/*
  SEC SR200PC20M
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

#include "sec_sr200pc20m.h"
#include "sec_sr200pc20m_reg.h"	

//#include "sec_cam_pmic.h"
#include "sec_cam_dev.h"

#include <linux/clk.h>
#include <linux/io.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>


/*	Read setting file from SDcard
	- There must be no "0x" string in comment. If there is, it cause some problem.
*/
//#define CONFIG_LOAD_FILE

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

struct sr200pc20m_work_t {
	struct work_struct work;
};

static struct  sr200pc20m_work_t *sr200pc20m_sensorw;
static struct  i2c_client *sr200pc20m_client;


static unsigned int config_csi2;

static struct sr200pc20m_ctrl_t *sr200pc20m_ctrl;

static DECLARE_WAIT_QUEUE_HEAD(sr200pc20m_wait_queue);
DECLARE_MUTEX(sr200pc20m_sem);

#ifdef CONFIG_LOAD_FILE
static int sr200pc20m_write_regs_from_sd(char *name);
static int sr200pc20m_regs_table_write(char *name);
#endif

static int sr200pc20m_start(void);


static int sr200pc20m_i2c_read(unsigned char subaddr, unsigned char *data)
{
	int ret;
	unsigned char buf[1];
	struct i2c_msg msg = { sr200pc20m_client->addr, 0, 1, buf };
	
	buf[0] = subaddr;

	ret = i2c_transfer(sr200pc20m_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	if (ret == -EIO) 
		goto error;

	msg.flags = I2C_M_RD;
	
	ret = i2c_transfer(sr200pc20m_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	if (ret == -EIO) 
		goto error;

	*data = buf[0];

error:
	return ret;
}

static int32_t sr200pc20m_i2c_write_16bit(u16 packet)
{
	int32_t rc = -EFAULT;
	int retry_count = 0;

	unsigned char buf[2];

	struct i2c_msg msg;

	buf[0] = (u8) (packet >> 8);
	buf[1] = (u8) (packet & 0xff);

	msg.addr = sr200pc20m_client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

#if defined(CAM_I2C_DEBUG)
	cam_err("I2C CHIP ID=0x%x, DATA 0x%x 0x%x", sr200pc20m_client->addr, buf[0], buf[1]);
#endif

	do {
		rc = i2c_transfer(sr200pc20m_client->adapter, &msg, 1);
		if (rc == 1)
			return 0;
		retry_count++;
		cam_err("i2c transfer failed, retrying %x err:%d",
		       packet, rc);
		mdelay(3);

	} while (retry_count <= 5);

	return 0;
}

static int sr200pc20m_i2c_write_list(const u16 *list, int size, char *name)
{
	int ret = 0;

#ifdef CONFIG_LOAD_FILE
	ret = sr200pc20m_write_regs_from_sd(name);
#else
	int i;
	u8 m_delay = 0;

	u16 temp_packet;
	

	CAM_DEBUG("%s, size=%d",name, size);
	for (i = 0; i < size; i++)
	{
		temp_packet = list[i];

		if ((temp_packet & SR200PC20M_DELAY) == SR200PC20M_DELAY)
		{
			m_delay = temp_packet & 0xFF;
			cam_info(" delay = %d",m_delay * 10);
			msleep(m_delay * 10);//step is 10msec
			continue;
		}

		if(sr200pc20m_i2c_write_16bit(temp_packet) < 0)
		{
			cam_err("fail(0x%x, 0x%x:%d)", sr200pc20m_client->addr, temp_packet, i);
			return -EIO;
		}
		//udelay(10);
	}
#endif	
	return ret;
}

#ifdef CONFIG_LOAD_FILE
static inline int sr200pc20m_write(struct i2c_client *client,
		u16 packet)
{
	u8 buf[2];
	int err = 0, retry_count = 5;

	struct i2c_msg msg;

	if (!client->adapter) {
		cam_err("ERR - can't search i2c client adapter");
		return -EIO;
	}

	buf[0] = (u8) (packet >> 8);
	buf[1] = (u8) (packet & 0xff);	

#if defined(CAM_I2C_DEBUG)
	cam_err(" I2C 0x%x 0x%x", buf[0], buf[1]);
#endif

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;
	
	do {
		err = i2c_transfer(sr200pc20m_client->adapter, &msg, 1);
		if (err == 1)
			return 0;
		retry_count++;
		cam_err("i2c transfer failed, retrying %x err:%d",
		       packet, err);
		mdelay(3);

	} while (retry_count <= 5);

	return (err != 1) ? -1 : 0;
}

int sr200pc20m_regs_table_init(void)
//void sr200pc20m_regs_table_init(void) //for panic
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

	CAM_DEBUG("CONFIG_LOAD_FILE is enable!!\n");

	mm_segment_t fs = get_fs();
	set_fs(get_ds());

	BUG_ON(testBuf);

	//fp = filp_open("/mnt/sdcard/external_sd/sec_sr200pc20m_reg.h", O_RDONLY, 0);
	fp = filp_open("/mnt/sdcard/sec_sr200pc20m_reg.h", O_RDONLY, 0);
	if (IS_ERR(fp)) {
		cam_err("failed to open /mnt/sdcard/sec_sr200pc20m_reg.h");
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


void sr200pc20m_regs_table_exit(void)
{
	if (testBuf) {
		large_file ? vfree(testBuf) : kfree(testBuf);
		large_file = 0;
		testBuf = NULL;
	}
}

static int sr200pc20m_write_regs_from_sd(char *name)
{
	struct test *tempData = NULL;

	int ret = -EAGAIN;
	u16 temp;
	u16 delay = 0;
	u8 data[7];
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
				for (i = 1; i < 7; i++) {
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
		if ((temp & SR200PC20M_DELAY) == SR200PC20M_DELAY) {
			delay = temp & 0xFF;
			cam_info("delay(%d)",delay*10); //step is 10msec
			msleep(delay*10);
			continue;
		}
		ret = sr200pc20m_write(sr200pc20m_client,temp);

		/* In error circumstances */
		/* Give second shot */
		if (unlikely(ret)) {
			ret = sr200pc20m_write(sr200pc20m_client,temp);

			/* Give it one more shot */
			if (unlikely(ret)) {
				ret = sr200pc20m_write(sr200pc20m_client, temp);
			}
		}
	}
	return 0;
}
#endif

static void  sr200pc20m_get_exif(void)
{
	u8 read_value1, read_value2, read_value3;	
	u16 iso_gain_table[] = {114, 214, 264, 752};
	u16 iso_table[] = {50, 100, 200, 400, 800};
	u16 gain = 0, index =0;
	u32 temp;

	/* Get ExposureTime */
	sr200pc20m_i2c_write_16bit(0x0320); // page 20
	sr200pc20m_i2c_read(0x82, &read_value3); 
	sr200pc20m_i2c_read(0x81, &read_value2); 
	sr200pc20m_i2c_read(0x80, &read_value1); 
	
	temp = (read_value3 << 3) | (read_value2 << 11) | (read_value1 << 19);
	temp = 24000000 / temp;
	
	sr200pc20m_ctrl->exif_exptime = temp; 
	CAM_DEBUG("[ExposureTime] read1 = 0x%x, read2 =0x%x, read3 = 0x%x, ExposureTime = %d",read_value1, read_value2, read_value3, sr200pc20m_ctrl->exif_exptime); 

	
	/* Get ISO */
	sr200pc20m_i2c_write_16bit(0x0320); // page 20
	sr200pc20m_i2c_read(0xB0, &read_value1); 	

	gain =((read_value1 * 100 ) / 32) + 50;
	for (index = 0; index < 4; index++) {
		if (gain < iso_gain_table[index])
			break;
	}
	sr200pc20m_ctrl->exif_iso = iso_table[index];
	CAM_DEBUG("[ISO] read1 = 0x%x, gain = %d, ISO = %d",read_value1, gain, sr200pc20m_ctrl->exif_iso);
}

static int sr200pc20m_check_dataline(s32 val)
{
	int err = 0;

	CAM_DEBUG("%s", val ? "ON" : "OFF");
	if (val) {
		err = sr200pc20m_i2c_write_list(sr200pc20m_pattern_on,
			sizeof(sr200pc20m_pattern_on)/sizeof(sr200pc20m_pattern_on[0]),"sr200pc20m_pattern_on");
		
	} else {
		err = sr200pc20m_i2c_write_list(sr200pc20m_pattern_off,
			sizeof(sr200pc20m_pattern_off)/sizeof(sr200pc20m_pattern_off[0]),"sr200pc20m_pattern_off");
	}

	return err;
}



static long sr200pc20m_video_config(int mode)
{
	int err = 0;

	err = sr200pc20m_i2c_write_list(sr200pc20m_preview, 
		sizeof(sr200pc20m_preview) / sizeof(sr200pc20m_preview[0]), "sr200pc20m_preview");

	if (sr200pc20m_ctrl->check_dataline)
		err = sr200pc20m_check_dataline(1);

	return err;

}

static long sr200pc20m_snapshot_config(int mode)
{
	int err = 0;

	err = sr200pc20m_i2c_write_list(sr200pc20m_capture, 
		sizeof(sr200pc20m_capture) / sizeof(sr200pc20m_capture[0]), "sr200pc20m_capture");
	
	sr200pc20m_get_exif();

	return err;				
}



static long sr200pc20m_set_sensor_mode(int mode)
{
	int err = 0;

	switch (mode) 
	{
	case SENSOR_PREVIEW_MODE:
		CAM_DEBUG("SENSOR_PREVIEW_MODE START");
		
		err = sr200pc20m_start();
		if(err < 0) {
			printk("sr200pc20m_start failed!\n"); 
			return err;
		}
		
		if (sr200pc20m_ctrl->sensor_mode != SENSOR_MOVIE)
			err= sr200pc20m_video_config(SENSOR_PREVIEW_MODE);

		break;

	case SENSOR_SNAPSHOT_MODE:
		CAM_DEBUG("SENSOR_SNAPSHOT_MODE START");		
		err= sr200pc20m_snapshot_config(SENSOR_SNAPSHOT_MODE);

		break;

	
	case SENSOR_SNAPSHOT_TRANSFER:
		CAM_DEBUG("SENSOR_SNAPSHOT_TRANSFER START");

		break;
		
	default:
		return -EFAULT;
	}

	return 0;
}


static int sr200pc20m_set_brightness(int8_t ev)
{
	int err = 0;
	
	CAM_DEBUG("%d",ev);
	
	if (sr200pc20m_ctrl->check_dataline || (sr200pc20m_ctrl->brightness == ev))
		return 0;
	
	switch(ev)
	{
		case EV_MINUS_4:
			err = sr200pc20m_i2c_write_list(sr200pc20m_bright_m4,
				sizeof(sr200pc20m_bright_m4)/sizeof(sr200pc20m_bright_m4[0]),"sr200pc20m_bright_m4");
			break;
		case EV_MINUS_3:
			err = sr200pc20m_i2c_write_list(sr200pc20m_bright_m3,
				sizeof(sr200pc20m_bright_m3)/sizeof(sr200pc20m_bright_m3[0]),"sr200pc20m_bright_m3");
			break;
		case EV_MINUS_2:
			err = sr200pc20m_i2c_write_list(sr200pc20m_bright_m2,
				sizeof(sr200pc20m_bright_m2)/sizeof(sr200pc20m_bright_m2[0]),"sr200pc20m_bright_m2");
			break;
		case EV_MINUS_1:
			err = sr200pc20m_i2c_write_list(sr200pc20m_bright_m1,
				sizeof(sr200pc20m_bright_m1)/sizeof(sr200pc20m_bright_m1[0]),"sr200pc20m_bright_m1");
			break;
		case EV_DEFAULT:
			err = sr200pc20m_i2c_write_list(sr200pc20m_bright_default,
				sizeof(sr200pc20m_bright_default)/sizeof(sr200pc20m_bright_default[0]),"sr200pc20m_bright_default");
			break;
		case EV_PLUS_1:
			err = sr200pc20m_i2c_write_list(sr200pc20m_bright_p1,
				sizeof(sr200pc20m_bright_p1)/sizeof(sr200pc20m_bright_p1[0]),"sr200pc20m_bright_p1");
			break;
		case EV_PLUS_2:
			err = sr200pc20m_i2c_write_list(sr200pc20m_bright_p2,
				sizeof(sr200pc20m_bright_p2)/sizeof(sr200pc20m_bright_p2[0]),"sr200pc20m_bright_p2");
			break;	
		case EV_PLUS_3:
			err = sr200pc20m_i2c_write_list(sr200pc20m_bright_p3,
				sizeof(sr200pc20m_bright_p3)/sizeof(sr200pc20m_bright_p3[0]),"sr200pc20m_bright_p3");
			break;
		case EV_PLUS_4:
			err = sr200pc20m_i2c_write_list(sr200pc20m_bright_p4,
				sizeof(sr200pc20m_bright_p4)/sizeof(sr200pc20m_bright_p4[0]),"sr200pc20m_bright_p4");
			break;
		default:
			cam_err("invalid");
			break;
	}
	
	sr200pc20m_ctrl->brightness = ev;
	return err;
}

static int sr200pc20m_set_blur(int8_t blur)
{
	int err = 0;
	
	CAM_DEBUG("%d",blur);
	
	if (sr200pc20m_ctrl->check_dataline || (sr200pc20m_ctrl->blur == blur))
		return 0;
	
	switch(blur)
	{
		case BLUR_LEVEL_0:
			err = sr200pc20m_i2c_write_list(sr200pc20m_vt_pretty_default,
				sizeof(sr200pc20m_vt_pretty_default)/sizeof(sr200pc20m_vt_pretty_default[0]),"sr200pc20m_vt_pretty_default");
			break;
		case BLUR_LEVEL_1:
			err = sr200pc20m_i2c_write_list(sr200pc20m_vt_pretty_1,
				sizeof(sr200pc20m_vt_pretty_1)/sizeof(sr200pc20m_vt_pretty_1[0]),"sr200pc20m_vt_pretty_1");
			break;
		case BLUR_LEVEL_2:
			err = sr200pc20m_i2c_write_list(sr200pc20m_vt_pretty_2,
				sizeof(sr200pc20m_vt_pretty_2)/sizeof(sr200pc20m_vt_pretty_2[0]),"sr200pc20m_vt_pretty_2");
			break;
		case BLUR_LEVEL_3:
			err = sr200pc20m_i2c_write_list(sr200pc20m_vt_pretty_3,
				sizeof(sr200pc20m_vt_pretty_3)/sizeof(sr200pc20m_vt_pretty_3[0]),"sr200pc20m_vt_pretty_3");
			break;
		default:
			cam_err("invalid");
			break;
	}

	sr200pc20m_ctrl->blur = blur;
	
	return err;
}


static int sr200pc20m_set_flip(uint32_t flip)
{
	int err = 0;
	
	CAM_DEBUG("%d",flip);
	
#if defined (CONFIG_TARGET_LOCALE_KOR) || defined (CONFIG_JPN_MODEL_SC_03D) || defined (CONFIG_TARGET_LOCALE_JPN)
	if(sr200pc20m_ctrl->check_dataline)
		return 0;

	switch(flip)
	{
		case 180:
			err = sr200pc20m_i2c_write_list(sr200pc20m_vflip,
				sizeof(sr200pc20m_vflip)/sizeof(sr200pc20m_vflip[0]),"sr200pc20m_vflip");
			break;
		default:
			cam_err("invalid");
			break;
	}
#endif
return err;

}


static int sr200pc20m_set_frame_rate( unsigned int fps)
{
	int err = 0;

	CAM_DEBUG("fps =%d", fps);
	
	if (sr200pc20m_ctrl->vtcall_mode == 0) { //normal
		return 0;
	}
	
	switch (fps) {
	case 7:
		err = sr200pc20m_i2c_write_list(sr200pc20m_vt_7fps,
				sizeof(sr200pc20m_vt_7fps)/sizeof(sr200pc20m_vt_7fps[0]), "sr200pc20m_vt_7fps");
		break;
	case 10:
		err = sr200pc20m_i2c_write_list(sr200pc20m_vt_10fps,
				sizeof(sr200pc20m_vt_10fps)/sizeof(sr200pc20m_vt_10fps[0]), "sr200pc20m_vt_10fps");

		break;
	case 12:
		err = sr200pc20m_i2c_write_list(sr200pc20m_vt_12fps,
				sizeof(sr200pc20m_vt_12fps)/sizeof(sr200pc20m_vt_12fps[0]), "sr200pc20m_vt_12fps");

		break;
	case 15:
		err = sr200pc20m_i2c_write_list(sr200pc20m_vt_15fps,
				sizeof(sr200pc20m_vt_15fps)/sizeof(sr200pc20m_vt_15fps[0]), "sr200pc20m_vt_15fps");
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

static int sr200pc20m_mipi_mode(int mode)
{
	int rc = 0;
	struct msm_camera_csi_params sr200pc20m_csi_params;
	
	CAM_DEBUG("E");

	if (!config_csi2) {
		sr200pc20m_csi_params.lane_cnt = 1;
		sr200pc20m_csi_params.data_format = CSI_8BIT;
		sr200pc20m_csi_params.lane_assign = 0xe4;
		sr200pc20m_csi_params.dpcm_scheme = 0;
		sr200pc20m_csi_params.settle_cnt =  0x11; // 
		rc = msm_camio_csi_config(&sr200pc20m_csi_params);
		if (rc < 0)
			printk(KERN_ERR "config csi controller failed \n");
		config_csi2 = 1;
	}
	CAM_DEBUG("X");
	return rc;
}


static int sr200pc20m_start(void)
{
	int err = 0;

	CAM_DEBUG("E");
	
	if(sr200pc20m_ctrl->started)
	{
		CAM_DEBUG("X : already started");
		return 0;
	}
	sr200pc20m_mipi_mode(1);
	msleep(30); //=> Please add some delay 
	

	CAM_DEBUG("mode %d, %d", sr200pc20m_ctrl->sensor_mode,sr200pc20m_ctrl->vtcall_mode);
	
	if (sr200pc20m_ctrl->sensor_mode == SENSOR_CAMERA) {
		if(sr200pc20m_ctrl->vtcall_mode == 0)  //normal
		{
			CAM_DEBUG("SELF CAMERA");
#ifdef I2C_BURST_MODE
			err = sr200pc20m_i2c_write_burst_list(sr200pc20m_common,
				sizeof(sr200pc20m_common)/sizeof(sr200pc20m_common[0]),"sr200pc20m_common");				
#else
			err = sr200pc20m_i2c_write_list(sr200pc20m_common,
				sizeof(sr200pc20m_common)/sizeof(sr200pc20m_common[0]),"sr200pc20m_common");
#endif
		}
		else if(sr200pc20m_ctrl->vtcall_mode == 1) //vt call
		{
			CAM_DEBUG("VT CALL");
			err = sr200pc20m_i2c_write_list(sr200pc20m_vt_common,
				sizeof(sr200pc20m_vt_common)/sizeof(sr200pc20m_vt_common[0]),"sr200pc20m_vt_common");
		}
		else //wifi vt call
		{
			CAM_DEBUG("WIFI VT CALL");
			err = sr200pc20m_i2c_write_list(sr200pc20m_vt_wifi_common,
				sizeof(sr200pc20m_vt_wifi_common)/sizeof(sr200pc20m_vt_wifi_common[0]),"sr200pc20m_vt_wifi_common");
		
		}
	}
	else {
		CAM_DEBUG("SELF RECORD");

#if defined (CONFIG_JPN_MODEL_SC_03D) //50Hz
#ifdef I2C_BURST_MODE
		err = sr200pc20m_i2c_write_burst_list(sr200pc20m_recording_50Hz_common,
			sizeof(sr200pc20m_recording_50Hz_common)/sizeof(sr200pc20m_recording_50Hz_common[0]),"sr200pc20m_recording_50Hz_common");
#else
		err = sr200pc20m_i2c_write_list(sr200pc20m_recording_50Hz_common,
			sizeof(sr200pc20m_recording_50Hz_common)/sizeof(sr200pc20m_recording_50Hz_common[0]),"sr200pc20m_recording_50Hz_common");
#endif


#else //60Hz
#ifdef I2C_BURST_MODE
		err = sr200pc20m_i2c_write_burst_list(sr200pc20m_recording_60Hz_common,
			sizeof(sr200pc20m_recording_60Hz_common)/sizeof(sr200pc20m_recording_60Hz_common[0]),"sr200pc20m_recording_60Hz_common");
#else
		err = sr200pc20m_i2c_write_list(sr200pc20m_recording_60Hz_common,
			sizeof(sr200pc20m_recording_60Hz_common)/sizeof(sr200pc20m_recording_60Hz_common[0]),"sr200pc20m_recording_60Hz_common");
#endif


#endif
	}

	sr200pc20m_ctrl->initialized = 1;
	sr200pc20m_ctrl->started = 1;
	CAM_DEBUG("X");
	return err;
}

static int sr200pc20m_set_movie_mode(int mode)
{
	if ((mode != SENSOR_CAMERA) && (mode != SENSOR_MOVIE)) {
		return -EINVAL;
	}
	/* We does not support movie mode when in VT. */
	if ((mode == SENSOR_MOVIE) && sr200pc20m_ctrl->vtcall_mode) {
		sr200pc20m_ctrl->sensor_mode = SENSOR_CAMERA;
	} else
		sr200pc20m_ctrl->sensor_mode = mode;

	CAM_DEBUG("mode %d, %d", sr200pc20m_ctrl->sensor_mode,sr200pc20m_ctrl->vtcall_mode);
	return 0;
}

static int sr200pc20m_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
	int rc = 0;
#ifdef CONFIG_LOAD_FILE	
	int err = 0;
#endif
	CAM_DEBUG("E");
#if 0	// -> msm_io_8x60.c, board-msm8x60_XXX.c
	CAM_DEBUG("POWER ON START ");
	gpio_set_value_cansleep(CAM_VGA_EN, LOW);
	gpio_set_value_cansleep(CAM_8M_RST, LOW);
	gpio_set_value_cansleep(CAM_VGA_RST, LOW);
	gpio_set_value_cansleep(CAM_IO_EN, LOW);  //HOST 1.8V

	rc = sub_cam_ldo_power(ON);
	
	//msm_camio_clk_rate_set(info->mclk);
	msm_camio_clk_rate_set(24000000);
	mdelay(30);//min 30ms
#endif

#ifdef CONFIG_LOAD_FILE
	err = sr200pc20m_regs_table_init();
//	sr200pc20m_regs_table_init(); //for panic

	if (err < 0 ) {
		cam_err("sr200pc20m_regs_table_init fail!!");
		return err;
	}
#endif
#if 0	// -> msm_io_8x60.c, board-msm8x60_XXX.c
	gpio_set_value_cansleep(CAM_VGA_RST, HIGH);
	mdelay(2);//min 50us
	CAM_DEBUG("POWER ON END ");
#endif
	return rc;
}


int sr200pc20m_get_exif_data(int32_t *exif_cmd, int32_t *val)
{
	//cfg_data.cmd == EXT_CFG_SET_EXIF
	switch(*exif_cmd){	
		case EXIF_EXPOSURE_TIME:
			(*val) = sr200pc20m_ctrl->exif_exptime;
			break;
		case EXIF_ISO:
			(*val) = sr200pc20m_ctrl->exif_iso;
			break;
		default:
			cam_err("Invalid");
			break;
		}

	return 0;	
}

int sr200pc20m_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int rc = 0;

	CAM_DEBUG("E");

	config_csi2 = 0;
	
	sr200pc20m_ctrl = kzalloc(sizeof(struct sr200pc20m_ctrl_t), GFP_KERNEL);
	if (!sr200pc20m_ctrl) {
		cam_err("sr200pc20m_ctrl failed!");
		rc = -ENOMEM;
		goto init_done;
	}	
	
	if (!data) {
		cam_err("data failed!");
		rc = -ENOMEM;
		goto init_fail;
	}

	sr200pc20m_ctrl->sensordata = data;
	
	rc = sr200pc20m_sensor_init_probe(data);
	if (rc < 0) {
		cam_err("sr200pc20m_sensor_open_init failed!");
		goto init_fail;
	}

	sr200pc20m_ctrl->initialized = 0;	
	sr200pc20m_ctrl->started = 0;
	sr200pc20m_ctrl->check_dataline = 0;
	sr200pc20m_ctrl->sensor_mode = SENSOR_CAMERA;	
	sr200pc20m_ctrl->brightness = EV_DEFAULT; 
	sr200pc20m_ctrl->blur = BLUR_LEVEL_0;
	sr200pc20m_ctrl->exif_exptime = 0; 
	sr200pc20m_ctrl->exif_iso = 0;

	CAM_DEBUG("X");
init_done:
	return rc;

init_fail:
	kfree(sr200pc20m_ctrl);
	sr200pc20m_ctrl = NULL;
	return rc;
}

static int sr200pc20m_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&sr200pc20m_wait_queue);
	return 0;
}



int sr200pc20m_sensor_ext_config(void __user *argp)
{
	sensor_ext_cfg_data cfg_data;
	int rc = 0;

	if (!sr200pc20m_ctrl)
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
		&& (!sr200pc20m_ctrl->initialized)){
		cam_err("camera isn't initialized");
		return 0;
	}
	
	switch(cfg_data.cmd) {
		case EXT_CFG_SET_BRIGHTNESS:
			rc = sr200pc20m_set_brightness(cfg_data.value_1);
			break;

		case EXT_CFG_SET_BLUR:
			rc = sr200pc20m_set_blur(cfg_data.value_1);
			break;		

		case EXT_CFG_SET_DTP:
			sr200pc20m_ctrl->check_dataline = cfg_data.value_1;
			
			if (sr200pc20m_ctrl->check_dataline == 0)
				rc = sr200pc20m_check_dataline(0);
			break;

		case EXT_CFG_SET_FPS:
			//rc = sr200pc20m_set_fps(cfg_data.value_1,cfg_data.value_2);
			rc = sr200pc20m_set_frame_rate(cfg_data.value_1);
			break;

#if 0
		case EXT_CFG_SET_FRONT_CAMERA_MODE:
			CAM_DEBUG("VTCall mode : %d",cfg_data.value_1);
			sr200pc20m_ctrl->vtcall_mode = cfg_data.value_1;
			break;
#endif

		case EXT_CFG_SET_VT_MODE:
			cam_info("VTCall mode : %d",cfg_data.value_1);
			sr200pc20m_ctrl->vtcall_mode = cfg_data.value_1;
			break;
			
		case EXT_CFG_SET_MOVIE_MODE:
			cam_info("MOVIE mode : %d",cfg_data.value_1);
			rc = sr200pc20m_set_movie_mode(cfg_data.value_1);
			break;

		case EXT_CFG_SET_FLIP:
			rc = sr200pc20m_set_flip(cfg_data.value_1);
			break;


		case EXT_CFG_GET_EXIF:
			rc = sr200pc20m_get_exif_data(&cfg_data.value_1, &cfg_data.value_2);	
			break;

		default:
			break;
	}

	if (copy_to_user((void *)argp, (const void *)&cfg_data, sizeof(cfg_data))){
		cam_err("fail copy_from_user!");
	}
	
	return rc;	
}

int sr200pc20m_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	int rc = 0;

	if (copy_from_user(&cfg_data,(void *)argp, sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	if (!sr200pc20m_ctrl)
		return -ENOSYS;

	CAM_DEBUG("cfgtype = %d, mode = %d", cfg_data.cfgtype, cfg_data.mode);

	switch (cfg_data.cfgtype) {
	case CFG_SET_MODE:
		rc = sr200pc20m_set_sensor_mode(cfg_data.mode);
		break;
	default:
		break;
	}

	return rc;
}

int sr200pc20m_sensor_release(void)
{
	CAM_DEBUG("E");
	
#if 0	// -> msm_io_8x60.c, board-msm8x60_XXX.c
	CAM_DEBUG("POWER OFF START");

	gpio_set_value_cansleep(CAM_VGA_RST, LOW);
	mdelay(1);

	sub_cam_ldo_power(OFF);	// have to turn off MCLK before PMIC
#endif
#ifdef CONFIG_LOAD_FILE
	sr200pc20m_regs_table_exit();
#endif
#if 0	// -> msm_io_8x60.c, board-msm8x60_XXX.c
	CAM_DEBUG("POWER OFF END");
#endif

	if (sr200pc20m_ctrl != NULL){
		kfree(sr200pc20m_ctrl);
		sr200pc20m_ctrl = NULL;
	}	
	return 0;
}

static int sr200pc20m_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc = 0;

	CAM_DEBUG("E");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	sr200pc20m_sensorw =
		kzalloc(sizeof(struct sr200pc20m_work_t), GFP_KERNEL);

	if (!sr200pc20m_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, sr200pc20m_sensorw);
	sr200pc20m_init_client(client);
	sr200pc20m_client = client;

	CAM_DEBUG("X");
	return 0;

probe_failure:
	kfree(sr200pc20m_sensorw);
	sr200pc20m_sensorw = NULL;
	cam_err("sr200pc20m_i2c_probe failed!");
	return rc;
}


static int __exit sr200pc20m_i2c_remove(struct i2c_client *client)
{

	struct sr200pc20m_work_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
//	i2c_detach_client(client);
	sr200pc20m_client = NULL;
	sr200pc20m_sensorw = NULL;
	kfree(sensorw);
	return 0;
}


static const struct i2c_device_id sr200pc20m_id[] = {
    { "sr200pc20m_i2c", 0 },
    { }
};

static struct i2c_driver sr200pc20m_i2c_driver = {
	.id_table	= sr200pc20m_id,
	.probe  	= sr200pc20m_i2c_probe,
	.remove 	= __exit_p(sr200pc20m_i2c_remove),
	.driver 	= {
		.name = "sr200pc20m",
	},
};


int32_t sr200pc20m_i2c_init(void)
{
	int32_t rc = 0;

	CAM_DEBUG("E");

	rc = i2c_add_driver(&sr200pc20m_i2c_driver);

	if (IS_ERR_VALUE(rc))
		goto init_failure;

	return rc;

init_failure:
	cam_err("failed to sr200pc20m_i2c_init, rc = %d", rc);
	return rc;
}


void sr200pc20m_exit(void)
{
	i2c_del_driver(&sr200pc20m_i2c_driver);
}


int sr200pc20m_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{
	int rc = 0;

	CAM_DEBUG("E");

	rc = sr200pc20m_i2c_init();

	if (rc < 0)
		goto probe_done;

	s->s_init	= sr200pc20m_sensor_open_init;
	s->s_release	= sr200pc20m_sensor_release;
	s->s_config	= sr200pc20m_sensor_config;
	s->s_ext_config	= sr200pc20m_sensor_ext_config;
	s->s_camera_type = FRONT_CAMERA_2D;
	s->s_mount_angle = 180;

probe_done:
	cam_err("rc = %d", rc);
	return rc;
	
}

#if 0
static int msm_vga_camera_remove(struct platform_device *pdev)
{
	return msm_camera_drv_remove(pdev);
}
#endif

static int __sec_sr200pc20m_probe(struct platform_device *pdev)
{
	printk("############# SR200PC20M probe ##############\n");

	return msm_camera_drv_start(pdev, sr200pc20m_sensor_probe);
}

static struct platform_driver msm_vga_camera_driver = {
	.probe = __sec_sr200pc20m_probe,
//	.remove	 = msm_vga_camera_remove,
	.driver = {
		.name = "msm_camera_sr200pc20m",
		.owner = THIS_MODULE,
	},
};

#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

static int __init sec_sr200pc20m_camera_init(void)
{
#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return 0\n", __func__);
		return 0;
	}
#endif
	
	return platform_driver_register(&msm_vga_camera_driver);
}

static void __exit sec_sr200pc20m_camera_exit(void)
{
	platform_driver_unregister(&msm_vga_camera_driver);
}

module_init(sec_sr200pc20m_camera_init);
module_exit(sec_sr200pc20m_camera_exit);

