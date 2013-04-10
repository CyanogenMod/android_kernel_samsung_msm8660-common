/*
  SEC ISX012
 */
/***************************************************************
CAMERA DRIVER FOR 5M CAM (SONY)
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


#include "sec_isx012.h"

#include "sec_apex_pmic.h"
#include "sec_cam_dev.h"

#include "sec_isx012_reg.h"

#include <linux/clk.h>
#include <linux/io.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>


#include <asm/mach-types.h>
#include <mach/vreg.h>
#include <linux/io.h>
#include "msm.h"

//#define CONFIG_LOAD_FILE

#ifdef CONFIG_LOAD_FILE

#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

static char *isx012_regs_table = NULL;
static int isx012_regs_table_size;
static int isx012_write_regs_from_sd(char *name);
#define TABLE_MAX_NUM 500
int gtable_buf[TABLE_MAX_NUM];
#endif

#define SONY_ISX012_BURST_DATA_LENGTH	1200

#define ISX012_WRITE_LIST(A)			isx012_i2c_write_list(A,(sizeof(A) / sizeof(A[0])),#A);
#define ISX012_BURST_WRITE_LIST(A)			isx012_i2c_burst_write_list(A,(sizeof(A) / sizeof(A[0])),#A);


/*native cmd code*/
#define CAM_AF		1
#define CAM_FLASH	2

#define FACTORY_TEST 1

struct isx012_work_t {
	struct work_struct work;
};

static struct isx012_work_t *isx012_sensorw;
static struct i2c_client *isx012_client;
static struct isx012_exif_data *isx012_exif;

static struct isx012_ctrl_t *isx012_ctrl;
int iscapture = 0;
int gLowLight_check = 0;
int DtpTest = 0;
uint16_t g_ae_auto = 0, g_ae_now = 0;
int16_t g_ersc_auto = 0, g_ersc_now = 0;

/* for tuning */
int gERRSCL_AUTO = 0, gUSER_AESCL_AUTO = 0, gERRSCL_NOW = 0, gUSER_AESCL_NOW = 0, gAE_OFSETVAL = 0, gAE_MAXDIFF = 0, gGLOWLIGHT_DEFAULT = 0;
int gGLOWLIGHT_ISO50 = 0, gGLOWLIGHT_ISO100 = 0, gGLOWLIGHT_ISO200 = 0, gGLOWLIGHT_ISO400 = 0;
/* for tuning */

static int accessibility_torch = 0;
static DECLARE_WAIT_QUEUE_HEAD(isx012_wait_queue);
//DECLARE_MUTEX(isx012_sem);

/*
#define ISX012_WRITE_LIST(A) \
    isx012_i2c_write_list(A,(sizeof(A) / sizeof(A[0])),#A);
*/

 static int isx012_i2c_read_multi(unsigned short subaddr, unsigned short *data, unsigned short len)
 {
	 unsigned char buf[4];
	 struct i2c_msg msg = {isx012_client->addr, 0, 2, buf};

	 int err = 0;

	 if (!isx012_client->adapter) {
		 dev_err(&isx012_client->dev, "%s: %d can't search i2c client adapter\n", __func__, __LINE__);
		 return -EIO;
	 }

	 buf[0] = subaddr>> 8;
	 buf[1] = subaddr & 0xff;

	 err = i2c_transfer(isx012_client->adapter, &msg, 1);
	 if (unlikely(err < 0)) {
		 dev_err(&isx012_client->dev, "%s: %d register read fail\n", __func__, __LINE__);
		 return -EIO;
	 }

	msg.flags = I2C_M_RD;
	msg.len = 2;

	 err = i2c_transfer(isx012_client->adapter, &msg, 1);
	 if (unlikely(err < 0)) {
		 dev_err(&isx012_client->dev, "%s: %d register read fail\n", __func__, __LINE__);
		 return -EIO;
	 }

	 /*
	  * Data comes in Little Endian in parallel mode; So there
	  * is no need for byte swapping here
	  */
	 *data = *(unsigned long *)(&buf);

	 return err;
 }

static int isx012_i2c_read(unsigned short subaddr, unsigned short *data)
{
	unsigned char buf[2];
	struct i2c_msg msg = {isx012_client->addr, 0, 2, buf};

	int err = 0;

	if (!isx012_client->adapter) {
		//dev_err(&isx012_client->dev, "%s: %d can't search i2c client adapter\n", __func__, __LINE__);
		return -EIO;
	}

	buf[0] = subaddr>> 8;
	buf[1] = subaddr & 0xff;

	err = i2c_transfer(isx012_client->adapter, &msg, 1);
	if (unlikely(err < 0)) {
		//dev_err(&isx012_client->dev, "%s: %d register read fail\n", __func__, __LINE__);
		return -EIO;
	}

	msg.flags = I2C_M_RD;

	err = i2c_transfer(isx012_client->adapter, &msg, 1);
	if (unlikely(err < 0)) {
		//dev_err(&isx012_client->dev, "%s: %d register read fail\n", __func__, __LINE__);
		return -EIO;
	}

	/*
	 * Data comes in Little Endian in parallel mode; So there
	 * is no need for byte swapping here
	 */
	*data = *(unsigned short *)(&buf);

	return err;
}
static int isx012_i2c_write_multi_temp(unsigned short addr, unsigned int w_data, unsigned int w_len)
{
	unsigned char buf[w_len+2];
	struct i2c_msg msg = {0x3C, 0, w_len+2, buf};

	int retry_count = 1;	//for Factory test
	int err = 0;

	if (!isx012_client->adapter) {
		//dev_err(&isx012_client->dev, "%s: %d can't search i2c client adapter\n", __func__, __LINE__);
		return -EIO;
	}

	buf[0] = addr >> 8;
	buf[1] = addr & 0xff;

	/*
	 * Data should be written in Little Endian in parallel mode; So there
	 * is no need for byte swapping here
	 */
	if(w_len == 1) {
		buf[2] = (unsigned char)w_data;
	} else if(w_len == 2)	{
		*((unsigned short *)&buf[2]) = (unsigned short)w_data;
	} else {
		*((unsigned int *)&buf[2]) = w_data;
	}

#if 0 //def ISX012_DEBUG
	{
		int j;
		printk("isx012 i2c write W: ");
		for(j = 0; j <= w_len+1; j++)
		{
			printk("0x%02x ", buf[j]);
		}
		printk("\n");
	}
#endif

	while(retry_count--) {
		err  = i2c_transfer(isx012_client->adapter, &msg, 1);
		if (likely(err == 1))
			break;
//		msleep(POLL_TIME_MS);
	}

	return (err == 1) ? 0 : -EIO;
}

static int isx012_i2c_write_multi(unsigned short addr, unsigned int w_data, unsigned int w_len)
{
	unsigned char buf[w_len+2];
	struct i2c_msg msg = {isx012_client->addr, 0, w_len+2, buf};

	int retry_count = 3;
	int err = 0;

	if (!isx012_client->adapter) {
		//dev_err(&isx012_client->dev, "%s: %d can't search i2c client adapter\n", __func__, __LINE__);
		return -EIO;
	}

	buf[0] = addr >> 8;
	buf[1] = addr & 0xff;

	/*
	 * Data should be written in Little Endian in parallel mode; So there
	 * is no need for byte swapping here
	 */
	if(w_len == 1) {
		buf[2] = (unsigned char)w_data;
	} else if(w_len == 2)	{
		*((unsigned short *)&buf[2]) = (unsigned short)w_data;
	} else {
		*((unsigned int *)&buf[2]) = w_data;
	}

#if 0 //def ISX012_DEBUG
	{
		int j;
		printk("isx012 i2c write W: ");
		for(j = 0; j <= w_len+1; j++)
		{
			printk("0x%02x ", buf[j]);
		}
		printk("\n");
	}
#endif

	while(retry_count--) {
		err  = i2c_transfer(isx012_client->adapter, &msg, 1);
		if (likely(err == 1))
			break;
//		msleep(POLL_TIME_MS);
	}

	return (err == 1) ? 0 : -EIO;
}


static int isx012_i2c_burst_write_list(isx012_short_t regs[], int size, char *name)
{
	int i = 0;
	int iTxDataIndex = 0;
	int retry_count = 3;
	int err = 0;


	unsigned char buf[SONY_ISX012_BURST_DATA_LENGTH];
	struct i2c_msg msg = {isx012_client->addr, 0, 4, buf};

	if (!isx012_client->adapter) {
		printk(KERN_ERR "%s: %d can't search i2c client adapter\n", __func__, __LINE__);
		return -EIO;
	}

	while ( i < size )//0<1
	{
		if ( 0 == iTxDataIndex )
		{
	        //printk("11111111111 delay 0x%04x, value 0x%04x\n", regs[i].subaddr, regs[i].value);
			buf[iTxDataIndex++] = ( regs[i].subaddr & 0xFF00 ) >> 8;
			buf[iTxDataIndex++] = ( regs[i].subaddr & 0xFF ); 
		}	

		if ( ( i < size - 1 ) && ( ( iTxDataIndex + regs[i].len ) <= ( SONY_ISX012_BURST_DATA_LENGTH - regs[i+1].len ) ) && ( regs[i].subaddr + regs[i].len == regs[i+1].subaddr ) )
		{
			if ( 1 == regs[i].len )
			{
				//printk("2222222 delay 0x%04x, value 0x%04x\n", regs[i].subaddr, regs[i].value);
				buf[iTxDataIndex++] = ( regs[i].value & 0xFF );
			}
			else
			{
				// Little Endian
				buf[iTxDataIndex++] = ( regs[i].value & 0x00FF );
				buf[iTxDataIndex++] = ( regs[i].value & 0xFF00 ) >> 8;
				//printk("3333333 delay 0x%04x, value 0x%04x\n", regs[i].subaddr, regs[i].value);
			}
		}
		else
		{
				if ( 1 == regs[i].len )
				{
					//printk("4444444 delay 0x%04x, value 0x%04x\n", regs[i].subaddr, regs[i].value);
					buf[iTxDataIndex++] = ( regs[i].value & 0xFF ); 
					//printk("burst_index:%d\n", iTxDataIndex);
					msg.len = iTxDataIndex;

				}
				else
				{
					//printk("555555 delay 0x%04x, value 0x%04x\n", regs[i].subaddr, regs[i].value);
					// Little Endian
					buf[iTxDataIndex++] = (regs[i].value & 0x00FF );
					buf[iTxDataIndex++] = (regs[i].value & 0xFF00 ) >> 8; 
					//printk("burst_index:%d\n", iTxDataIndex);
					msg.len = iTxDataIndex;

				}

				while(retry_count--) {
				err  = i2c_transfer(isx012_client->adapter, &msg, 1);
					if (likely(err == 1))
						break;
					
        		}
			iTxDataIndex = 0;
		}
		i++;
	}

	return 0;
}

static int isx012_i2c_write_list(isx012_short_t regs[], int size, char *name)
{

#ifdef CONFIG_LOAD_FILE
	isx012_write_regs_from_sd(name);
#else
	int err = 0;
	int i = 0;

	if (!isx012_client->adapter) {
		printk(KERN_ERR "%s: %d can't search i2c client adapter\n", __func__, __LINE__);
		return -EIO;
	}

	for (i = 0; i < size; i++) {
		if(regs[i].subaddr == 0xFFFF)
		{
		    msleep(regs[i].value);
                    printk("delay 0x%04x, value 0x%04x\n", regs[i].subaddr, regs[i].value);
		} else {
        			err = isx012_i2c_write_multi(regs[i].subaddr, regs[i].value, regs[i].len);

        		if (unlikely(err < 0)) {
        			printk(KERN_ERR "%s: register set failed\n",  __func__);
        			return -EIO;
        		}
                }
	}
#endif

	return 0;
}

#ifdef CONFIG_LOAD_FILE
void isx012_regs_table_init(void)
{
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int ret;
	mm_segment_t fs = get_fs();

	printk(KERN_DEBUG "%s %d\n", __func__, __LINE__);

	set_fs(get_ds());

	filp = filp_open("/mnt/sdcard/sec_isx012_reg.h", O_RDONLY, 0);

	if (IS_ERR_OR_NULL(filp)) {
		printk(KERN_DEBUG "file open error\n");
		return PTR_ERR(filp);
	}

	l = filp->f_path.dentry->d_inode->i_size;
	printk(KERN_DEBUG "l = %ld\n", l);
	//dp = kmalloc(l, GFP_KERNEL);
	dp = vmalloc(l);
	if (dp == NULL) {
		printk(KERN_DEBUG "Out of Memory\n");
		filp_close(filp, current->files);
	}

	pos = 0;
	memset(dp, 0, l);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);

	if (ret != l) {
		printk(KERN_DEBUG "Failed to read file ret = %d\n", ret);
		/*kfree(dp);*/
		vfree(dp);
		filp_close(filp, current->files);
		return -EINVAL;
	}

	filp_close(filp, current->files);

	set_fs(fs);

	isx012_regs_table = dp;

	isx012_regs_table_size = l;

	*((isx012_regs_table + isx012_regs_table_size) - 1) = '\0';

	printk("isx012_reg_table_init\n");
	return 0;
}

void isx012_regs_table_exit(void)
{
	printk(KERN_DEBUG "%s %d\n", __func__, __LINE__);

	if (isx012_regs_table) {
		vfree(isx012_regs_table);
		isx012_regs_table = NULL;
	}
}

static int isx012_define_table()
{
	char *start, *end, *reg;
	char *start_token, *reg_token, *temp;
	char reg_buf[61], temp2[61];
	char token_buf[5];
	int token_value = 0;
	int index_1 = 0, index_2 = 0, total_index;
	int len = 0, total_len = 0;

	*(reg_buf + 60) = '\0';
	*(temp2 + 60) = '\0';
	*(token_buf + 4) = '\0';
	memset(gtable_buf, 9999, TABLE_MAX_NUM);

	printk(KERN_DEBUG "isx012_define_table start!\n");

	start = strstr(isx012_regs_table, "aeoffset_table");
	end = strstr(start, "};");

	/* Find table */
	index_2 = 0;
	while (1) {
		reg = strstr(start,"	");
		if ((reg == NULL) || (reg > end)) {
			printk(KERN_DEBUG "isx012_define_table read end!\n");
			break;
		}

		/* length cal */
		index_1 = 0;
		total_len = 0;
		temp = reg;
		if (temp != NULL) {
			memcpy(temp2, (temp + 1), 60);
			//printk(KERN_DEBUG "temp2 : %s\n", temp2);
		}
		start_token = strstr(temp,",");
		while (index_1 < 10) {
			start_token = strstr(temp, ",");
			len = strcspn(temp, ",");
			//printk(KERN_DEBUG "len[%d]\n", len);	//Only debug
			total_len = total_len + len;
			temp = (temp + (len+2));
			index_1 ++;
		}
		total_len = total_len + 19;
		//printk(KERN_DEBUG "%d\n", total_len);	//Only debug

		/* read table */
		if (reg != NULL) {
			memcpy(reg_buf, (reg + 1), total_len);
			//printk(KERN_DEBUG "reg_buf : %s\n", reg_buf);	//Only debug
			start = (reg + total_len+1);
		}

		reg_token = reg_buf;

		index_1 = 0;
		start_token=strstr(reg_token,",");
		while (index_1 < 10) {
			start_token = strstr(reg_token, ",");
			len = strcspn(reg_token, ",");
			//printk(KERN_DEBUG "len[%d]\n", len);	//Only debug
			memcpy(token_buf, reg_token, len);
			//printk(KERN_DEBUG "[%d]%s ", index_1, token_buf);	//Only debug
			token_value = (unsigned short)simple_strtoul(token_buf, NULL, 10);
			total_index = index_2 * 10 + index_1;
			//printk(KERN_DEBUG "[%d]%d ", total_index, token_value);	//Only debug
			gtable_buf[total_index] = token_value;
			index_1 ++;
			reg_token = (reg_token + (len + 2));
		}
		index_2 ++;
	}

#if 0	//Only debug
	index_2 = 0;
	while ( index_2 < TABLE_MAX_NUM) {
		printk(KERN_DEBUG "[%d]%d ",index_2, gtable_buf[index_2]);
		index_2++;
	}
#endif
	printk(KERN_DEBUG "isx012_define_table end!\n");

	return 0;
}

static int isx012_define_read(char *name, int len_size)
{
	char *start, *end, *reg;
	char reg_7[7], reg_5[5];
	int define_value = 0;

	*(reg_7 + 6) = '\0';
	*(reg_5 + 4) = '\0';

	//printk(KERN_DEBUG "isx012_define_read start!\n");

	start = strstr(isx012_regs_table, name);
	end = strstr(start, "tuning");

	reg = strstr(start," ");

	if ((reg == NULL) || (reg > end)) {
		printk(KERN_DEBUG "isx012_define_read error %s : ",name);
		return -1;
	}

	/* Write Value to Address */
	if (reg != NULL) {
		if (len_size == 6) {
			memcpy(reg_7, (reg + 1), len_size);
			define_value = (unsigned short)simple_strtoul(reg_7, NULL, 16);
		} else {
			memcpy(reg_5, (reg + 1), len_size);
			define_value = (unsigned short)simple_strtoul(reg_5, NULL, 10);
		}
	}
	//printk(KERN_DEBUG "isx012_define_read end (0x%x)!\n", define_value);

	return define_value;
}

static int isx012_write_regs_from_sd(char *name)
{
	char *start, *end, *reg, *size;
	unsigned short addr;
	unsigned int len, value;
	char reg_buf[7], data_buf1[5], data_buf2[7], len_buf[5];

	*(reg_buf + 6) = '\0';
	*(data_buf1 + 4) = '\0';
	*(data_buf2 + 6) = '\0';
	*(len_buf + 4) = '\0';

	printk(KERN_DEBUG "isx012_regs_table_write start!\n");
	printk(KERN_DEBUG "E string = %s\n", name);

	start = strstr(isx012_regs_table, name);
	end = strstr(start, "};");

	while (1) {
		/* Find Address */
		reg = strstr(start,"{0x");

		if ((reg == NULL) || (reg > end))
			break;

		/* Write Value to Address */
		if (reg != NULL) {
			memcpy(reg_buf, (reg + 1), 6);
			memcpy(data_buf2, (reg + 8), 6);
			size = strstr(data_buf2,",");
			if (size) { /* 1 byte write */
				memcpy(data_buf1, (reg + 8), 4);
				memcpy(len_buf, (reg + 13), 4);
				addr = (unsigned short)simple_strtoul(reg_buf, NULL, 16);
				value = (unsigned int)simple_strtoul(data_buf1, NULL, 16);
				len = (unsigned int)simple_strtoul(len_buf, NULL, 16);
				if (reg)
					start = (reg + 20);  //{0x000b,0x04,0x01},
			} else {/* 2 byte write */
				memcpy(len_buf, (reg + 15), 4);
				addr = (unsigned short)simple_strtoul(reg_buf, NULL, 16);
				value = (unsigned int)simple_strtoul(data_buf2, NULL, 16);
				len = (unsigned int)simple_strtoul(len_buf, NULL, 16);
				if (reg)
					start = (reg + 22);  //{0x000b,0x0004,0x01},
			}
			size = NULL;

			if (addr == 0xFFFF) {
				msleep(value);
			} else {
				isx012_i2c_write_multi(addr, value, len);
			}
		}
	}

	printk(KERN_DEBUG "isx005_regs_table_write end!\n");

	return 0;
}
#endif

static int isx012_get_LowLightCondition( void ) 
{
	int err = -1;
	unsigned char r_data2[2] = {0, 0};
	unsigned char l_data[2] = {0, 0}, h_data[2] = {0, 0};
	unsigned int LowLight_value = 0;
	unsigned int ldata_temp = 0, hdata_temp = 0;

	if(isx012_ctrl->setting.iso == 0) {	//auto iso
		pr_err("%s: auto iso %d\n", __func__, isx012_ctrl->setting.iso);
		err = isx012_i2c_read_multi(0x01A5, (unsigned short*)r_data2, 2);
		
		if (err < 0)
			pr_err("%s: isx012_get_LowLightCondition() returned error, %d\n", __func__, err);

		LowLight_value = r_data2[0];
		if (LowLight_value >= gGLOWLIGHT_DEFAULT) {
			gLowLight_check = 1;
		} else {
			gLowLight_check = 0;
		}
	} else {	//manual iso
		pr_err("%s: manual iso %d\n", __func__, isx012_ctrl->setting.iso);
		err = isx012_i2c_read_multi(0x019C,(unsigned short*) l_data, 2);	//SHT_TIME_OUT_L
		ldata_temp = (l_data[1] << 8 | l_data[0]);

		err = isx012_i2c_read_multi(0x019E,(unsigned short*) h_data, 2);	//SHT_TIME_OUT_H
		hdata_temp = (h_data[1] << 8 | h_data[0]);
		LowLight_value = (h_data[1] << 24 | h_data[0] << 16 | l_data[1] << 8 | l_data[0]);
		//printk(KERN_ERR "func(%s):line(%d) LowLight_value : 0x%x / hdata_temp : 0x%x ldata_temp : 0x%x\n",__func__, __LINE__, LowLight_value, hdata_temp, ldata_temp);

		switch (isx012_ctrl->setting.iso) {
			case ISO_50 :
				if (LowLight_value >= gGLOWLIGHT_ISO50) {
					gLowLight_check = 1;
				} else {
					gLowLight_check = 0;
				}
				break;

			case ISO_100 :
				if (LowLight_value >= gGLOWLIGHT_ISO100) {
					gLowLight_check = 1;
				} else {
					gLowLight_check = 0;
				}
				break;

			case ISO_200 :
				if (LowLight_value >= gGLOWLIGHT_ISO200) {
					gLowLight_check = 1;
				} else {
					gLowLight_check = 0;
				}
				break;

			case ISO_400 :
				if (LowLight_value >= gGLOWLIGHT_ISO400) {
					gLowLight_check = 1;
				} else {
					gLowLight_check = 0;
				}
				break;

			default :
				printk(KERN_DEBUG "[ISX012][%s:%d] invalid iso[%d]\n", __func__, __LINE__, isx012_ctrl->setting.iso);
				break;

		}
	}
	printk(KERN_ERR "func(%s):line(%d) LowLight_value : 0x%x gLowLight_check : %d\n",__func__, __LINE__, LowLight_value, gLowLight_check);

	return err;
}

int isx012_mode_transition_OM(void)
{
	int timeout_cnt = 0;
	int factory_timeout_cnt = 0;
	int om_status = 0;
	int ret = 0;
	short unsigned int r_data[2] = {0,0};

	printk("[isx012] %s/%d\n", __func__, __LINE__);

	timeout_cnt = 0;
	do {
		if (timeout_cnt > 0){
			mdelay(1);
		}
		timeout_cnt++;
		ret = isx012_i2c_read_multi(0x000E, r_data, 1);
		om_status = r_data[0];
		//printk("%s [isx012] 0x000E (1) read : 0x%x / om_status & 0x1 : 0x%x(origin:0x1)\n", __func__, om_status, om_status & 0x1);
		if (ret != 1) {
			factory_timeout_cnt++;
			if (factory_timeout_cnt > 5) {
				pr_info("factory test1 error(%d)\n", ret);
				return -EIO; /*factory test*/
			}
			
		}
		if (timeout_cnt > ISX012_DELAY_RETRIES_OM) {
			pr_err("%s: %d :Entering OM_1 delay timed out \n", __func__, __LINE__);
			break;
		}
	} while (((om_status & 0x01) != 0x01));

	timeout_cnt = 0;

	if (ret != 1) {
		pr_info("factory test2 error(%d)\n", ret);
		return -EIO; /*factory test*/
	}

	do {
		if (timeout_cnt > 0){
			mdelay(1);
		}
		timeout_cnt++;
		isx012_i2c_write_multi(0x0012, 0x01, 0x01);
		isx012_i2c_read_multi(0x000E, r_data, 1);
		om_status = r_data[0];
		//printk("%s [isx012] 0x000E (2) read : 0x%x / om_status & 0x1 : 0x%x(origin:0x0)\n", __func__, om_status, om_status & 0x1);
		if (timeout_cnt > ISX012_DELAY_RETRIES_OM) {
			pr_err("%s: %d :Entering OM_2 delay timed out \n", __func__, __LINE__);
			break;
		}
	} while (((om_status & 0x01) != 0x00));

	return 0; /*factory test*/
}

void isx012_mode_transition_CM(void)
{
	int timeout_cnt = 0;
	int cm_status = 0;
	short unsigned int r_data[2] = {0,0};

	CAM_DEBUG("E");

	timeout_cnt = 0;
	do {
		if (timeout_cnt > 0){
			mdelay(10);
		}
		timeout_cnt++;
		isx012_i2c_read_multi(0x000E, r_data, 1);
		cm_status = r_data[0];
		//printk("%s [isx012] 0x000E (1) read : 0x%x / cm_status & 0x2 : 0x%x(origin:0x2)\n", __func__, cm_status, cm_status & 0x2);
		if (timeout_cnt > ISX012_DELAY_RETRIES_CM) {
			pr_err("%s: %d :Entering CM_1 delay timed out \n", __func__, __LINE__);
			break;
		}
	} while (((cm_status & 0x2) != 0x2));
	//printk("%s [isx012] 0x000E (1) read : 0x%x / cm_status & 0x2 : 0x%x(origin:0x2) count(%d)\n", __func__, cm_status, cm_status & 0x2, timeout_cnt);

	timeout_cnt = 0;
	do {
		if (timeout_cnt > 0){
			mdelay(10);
		}
		timeout_cnt++;
		isx012_i2c_write_multi(0x0012, 0x02, 0x01);
		isx012_i2c_read_multi(0x000E, r_data, 1);
		cm_status = r_data[0];
		//printk("%s [isx012] 0x000E (2) read : 0x%x / cm_status & 0x2 : 0x%x(origin:0x0)\n", __func__, cm_status, cm_status & 0x2);
		if (timeout_cnt > ISX012_DELAY_RETRIES_CM) {
			pr_err("%s: %d :Entering CM_2 delay timed out \n", __func__, __LINE__);
			break;
		}
	} while (((cm_status & 0x2) != 0x0));
	//printk("%s [isx012] 0x000E (2) read : 0x%x / cm_status & 0x2 : 0x%x(origin:0x0) count(%d)\n", __func__, cm_status, cm_status & 0x2, timeout_cnt);
}

void isx012_Sensor_Calibration(void)
{
	//int count = 0;
	int status = 0;
	int temp = 0;

	printk(KERN_DEBUG "[isx012] %s/%d\n", __func__, __LINE__);

	/* Read OTP1 */
	isx012_i2c_read(0x004F, (unsigned short *)&status);
	//printk(KERN_DEBUG "[isx012] 0x004F read : %x\n", status);

	if ((status & 0x10) == 0x10) {
		/* Read ShadingTable */
		isx012_i2c_read(0x005C, (unsigned short *)&status);
		temp = (status&0x03C0)>>6;
		//printk(KERN_DEBUG "[isx012] Read ShadingTable read : %x\n", temp);

		/* Write Shading Table */
		if (temp == 0x0) {
			ISX012_BURST_WRITE_LIST(ISX012_Shading_0);
		} else if (temp == 0x1) {
			ISX012_BURST_WRITE_LIST(ISX012_Shading_1);
		} else if (temp == 0x2) {
			ISX012_BURST_WRITE_LIST(ISX012_Shading_2);
		}

		/* Write NorR */
		isx012_i2c_read(0x0054, (unsigned short *)&status);
		temp = status&0x3FFF;
		//printk(KERN_DEBUG "[isx012] NorR read : %x\n", temp);
		isx012_i2c_write_multi(0x6804, temp, 0x02);

		/* Write NorB */
		isx012_i2c_read(0x0056, (unsigned short *)&status);
		temp = status&0x3FFF;
		//printk(KERN_DEBUG "[isx012] NorB read : %x\n", temp);
		isx012_i2c_write_multi(0x6806, temp, 0x02);

		/* Write PreR */
		isx012_i2c_read(0x005A, (unsigned short *)&status);
		temp = (status&0x0FFC)>>2;
		//printk(KERN_DEBUG "[isx012] PreR read : %x\n", temp);
		isx012_i2c_write_multi(0x6808, temp, 0x02);

		/* Write PreB */
		isx012_i2c_read(0x005B, (unsigned short *)&status);
		temp = (status&0x3FF0)>>4;
		//printk(KERN_DEBUG "[isx012] PreB read : %x\n", temp);
		isx012_i2c_write_multi(0x680A, temp, 0x02);
	} else {
		/* Read OTP0 */
		isx012_i2c_read(0x0040, (unsigned short *)&status);
		//printk(KERN_DEBUG "[isx012] 0x0040 read : %x\n", status);

		if ((status & 0x10) == 0x10) {
			/* Read ShadingTable */
			isx012_i2c_read(0x004D, (unsigned short *)&status);
			temp = (status&0x03C0)>>6;
			//printk(KERN_DEBUG "[isx012] Read ShadingTable read : %x\n", temp);

			/* Write Shading Table */
			if (temp == 0x0) {
				ISX012_BURST_WRITE_LIST(ISX012_Shading_0);
			} else if (temp == 0x1) {
				ISX012_BURST_WRITE_LIST(ISX012_Shading_1);
			} else if (temp == 0x2) {
				ISX012_BURST_WRITE_LIST(ISX012_Shading_2);
			}

			/* Write NorR */
			isx012_i2c_read(0x0045, (unsigned short *)&status);
			temp = status&0x3FFF;
			//printk(KERN_DEBUG "[isx012] NorR read : %x\n", temp);
			isx012_i2c_write_multi(0x6804, temp, 0x02);

			/* Write NorB */
			isx012_i2c_read(0x0047, (unsigned short *)&status);
			temp = status&0x3FFF;
			//printk(KERN_DEBUG "[isx012] NorB read : %x\n", temp);
			isx012_i2c_write_multi(0x6806, temp, 0x02);

			/* Write PreR */
			isx012_i2c_read(0x004B, (unsigned short *)&status);
			temp = (status&0x0FFC)>>2;
			//printk(KERN_DEBUG "[isx012] PreR read : %x\n", temp);
			isx012_i2c_write_multi(0x6808, temp, 0x02);

			/* Write PreB */
			isx012_i2c_read(0x004C, (unsigned short *)&status);
			temp = (status&0x3FF0)>>4;
			//printk(KERN_DEBUG "[isx012] PreB read : %x\n", temp);
			isx012_i2c_write_multi(0x680A, temp, 0x02);
		} else
			ISX012_BURST_WRITE_LIST(ISX012_Shading_Nocal);
	}
}

#if 0
static int32_t isx012_i2c_write_32bit(unsigned long packet)
{
	int32_t err = -EFAULT;
	//int retry_count = 1;
	//int i;

	unsigned char buf[4];
	struct i2c_msg msg = {
		.addr = isx012_client->addr,
		.flags = 0,
		.len = 4,
		.buf = buf,
	};
	*(unsigned long *)buf = cpu_to_be32(packet);

	//for(i=0; i< retry_count; i++) {
	err = i2c_transfer(isx012_client->adapter, &msg, 1);
  	//}

	return err;
}
#endif



#if 0


static int32_t isx012_i2c_write(unsigned short subaddr, unsigned short val)
{
	unsigned long packet;
	packet = (subaddr << 16) | (val&0xFFFF);

	return isx012_i2c_write_32bit(packet);
}


static int32_t isx012_i2c_read(unsigned short subaddr, unsigned short *data)
{

	int ret;
	unsigned char buf[2];

	struct i2c_msg msg = {
		.addr = isx012_client->addr,
		.flags = 0,
		.len = 2,
		.buf = buf,
	};

	buf[0] = (subaddr >> 8);
	buf[1] = (subaddr & 0xFF);

	ret = i2c_transfer(isx012_client->adapter, &msg, 1) == 1 ? 0 : -EIO;

	if (ret == -EIO)
	    goto error;

	msg.flags = I2C_M_RD;

	ret = i2c_transfer(isx012_client->adapter, &msg, 1) == 1 ? 0 : -EIO;
	if (ret == -EIO)
	    goto error;

	*data = ((buf[0] << 8) | buf[1]);


error:
	return ret;

}

#endif

#if 0
static int isx012_set_ae_lock(char value)
{
	int err = -EINVAL;
	CAM_DEBUG("%d",value);
	printk("isx012_set_ae_lock ");
	switch (value) {
		case EXT_CFG_AE_LOCK:
			isx012_ctrl->setting.ae_lock = EXT_CFG_AE_LOCK;
			 ISX012_WRITE_LIST(isx012_ae_lock);
			break;
		case EXT_CFG_AE_UNLOCK:
			isx012_ctrl->setting.ae_lock = EXT_CFG_AE_UNLOCK;
			ISX012_WRITE_LIST(isx012_ae_unlock);
			break;
		case EXT_CFG_AWB_LOCK:
			isx012_ctrl->setting.awb_lock = EXT_CFG_AWB_LOCK;
		       ISX012_WRITE_LIST(isx012_awb_lock);
			break;
		case EXT_CFG_AWB_UNLOCK:
			isx012_ctrl->setting.awb_lock = EXT_CFG_AWB_UNLOCK;
			ISX012_WRITE_LIST(isx012_awb_unlock);
			break;
		default:
			cam_err("Invalid(%d)", value);
			break;
	}
	return err;
}
#endif

static long isx012_set_effect(int8_t value)
{
	int err = -EINVAL;
	CAM_DEBUG("%d",value);

	switch (value) {
		case CAMERA_EFFECT_OFF:
			ISX012_WRITE_LIST(isx012_Effect_Normal);
			isx012_ctrl->setting.effect = value;
			err =0;
			break;

		case CAMERA_EFFECT_SEPIA:
			ISX012_WRITE_LIST(isx012_Effect_Sepia);
			isx012_ctrl->setting.effect = value;
			err =0;
			break;

		case CAMERA_EFFECT_MONO:
			ISX012_WRITE_LIST(isx012_Effect_Black_White);
			isx012_ctrl->setting.effect = value;
			err =0;
			break;

		case CAMERA_EFFECT_NEGATIVE:
			ISX012_WRITE_LIST(ISX012_Effect_Negative);
			isx012_ctrl->setting.effect = value;
			err =0;
			break;

		default:
			printk(KERN_DEBUG "[ISX012][%s:%d] invalid[%d]\n", __func__, __LINE__, value);
			err = 0;
			break;
	}

	return err;
}

static int isx012_set_whitebalance(int8_t value)
{
	int err = 0;

	switch (value) {
		case WHITE_BALANCE_AUTO :
			ISX012_WRITE_LIST (isx012_WB_Auto);
			break;

		case WHITE_BALANCE_SUNNY:
			ISX012_WRITE_LIST(isx012_WB_Sunny);
			break;

		case WHITE_BALANCE_CLOUDY :
			ISX012_WRITE_LIST(isx012_WB_Cloudy);
			break;

		case WHITE_BALANCE_FLUORESCENT:
			ISX012_WRITE_LIST(isx012_WB_Fluorescent);
			break;

		case WHITE_BALANCE_INCANDESCENT:
			ISX012_WRITE_LIST(isx012_WB_Tungsten);
			break;

		default :
			printk(KERN_DEBUG "[ISX012][%s:%d] invalid[%d]\n", __func__, __LINE__, value);
			break;
	}
	isx012_ctrl->setting.whiteBalance = value;
	return err;
}

static int isx012_exif_shutter_speed(void)
{
	uint32_t shutter_speed = 0;
	int err = 0;

	unsigned char l_data[2] = {0,0}, h_data[2] = {0,0};

	err = isx012_i2c_read_multi(0x019C,(unsigned short*) l_data,2);	//SHT_TIME_OUT_L
	err = isx012_i2c_read_multi(0x019E,(unsigned short*) h_data,2);	//SHT_TIME_OUT_H
	shutter_speed = (h_data[1] << 24 | h_data[0] << 16 | l_data[1] << 8 | l_data[0]);
	//printk(KERN_DEBUG "[exif_shutter][%s:%d] shutter_speed(%x/%d)\n", __func__, __LINE__, shutter_speed, shutter_speed);

	isx012_exif->shutterspeed = shutter_speed;

	return 0;
}

static int isx012_set_flash(int8_t value1, int8_t value2)
{
	int err = -EINVAL;
	int i = 0;
	int torch = 0, torch2 = 0, torch3 =0;

	/* Accessary concept */
	if (accessibility_torch) {
		isx012_ctrl->setting.flash_mode = 0;
		cam_err("Accessibility torch(%d)", isx012_ctrl->setting.flash_mode);
		return 0;
	}

	if ((value1 > -1) && (value2 == 50)) {
		switch (value1) {
			case 0:	//off
				gpio_set_value_cansleep(CAM_FLASH_EN, 0);
				gpio_set_value_cansleep(CAM_FLASH_SET, 0);
				err = 0;
				break;

			case 1: //Auto
				if (gLowLight_check) {
					gpio_set_value_cansleep(CAM_FLASH_EN, 1);
					gpio_set_value_cansleep(CAM_FLASH_SET, 0);
					isx012_exif->flash = 1;
				} else {
					gpio_set_value_cansleep(CAM_FLASH_EN, 0);
					gpio_set_value_cansleep(CAM_FLASH_SET, 0);
					isx012_exif->flash = 0;
				}
				err = 0;
				break;

			case 2:	//on
				gpio_set_value_cansleep(CAM_FLASH_EN, 1);
				gpio_set_value_cansleep(CAM_FLASH_SET, 0);
				err = 0;
				break;

			case 3:	//torch
				gpio_set_value_cansleep(CAM_FLASH_EN, 0);
				torch = gpio_get_value(CAM_FLASH_EN);

				for (i = 5; i > 1; i--) {
					gpio_set_value_cansleep(CAM_FLASH_SET, 1);
					torch2 = gpio_get_value(CAM_FLASH_SET);
					udelay(1);
					gpio_set_value_cansleep(CAM_FLASH_SET, 0);
					torch3 = gpio_get_value(CAM_FLASH_SET);
					udelay(1);
				}
				gpio_set_value_cansleep(CAM_FLASH_SET, 1);
				usleep(2*1000);
				err = 0;
				break;

			default :
				printk(KERN_DEBUG "[ISX012][%s:%d] invalid[%d/%d]\n", __func__, __LINE__, value1, value2);
				err = 0;
				break;
		}
	} else if ((value1 == 50 && value2 > -1)) {
		isx012_ctrl->setting.flash_mode = value2;
		cam_err("flash value1(%d) value2(%d) isx012_ctrl->setting.flash_mode(%d)", value1, value2, isx012_ctrl->setting.flash_mode);
		err = 0;
	}

	cam_err("FINAL flash value1(%d) value2(%d) isx012_ctrl->setting.flash_mode(%d)", value1, value2, isx012_ctrl->setting.flash_mode);

	return err;
}

static int  isx012_set_brightness(int8_t value)
{
	int err = -EINVAL;
	CAM_DEBUG("%d", value);

	switch (value) {
		case EV_MINUS_4 :
			ISX012_WRITE_LIST(ISX012_ExpSetting_M4Step);
			isx012_ctrl->setting.brightness = value;
			err = 0;
			break;

		case EV_MINUS_3 :
			ISX012_WRITE_LIST(ISX012_ExpSetting_M3Step);
			isx012_ctrl->setting.brightness = value;
			err = 0;
			break;

		case EV_MINUS_2 :
			ISX012_WRITE_LIST(ISX012_ExpSetting_M2Step);
			isx012_ctrl->setting.brightness = value;
			err = 0;
			break;

		case EV_MINUS_1 :
			ISX012_WRITE_LIST(ISX012_ExpSetting_M1Step);
			isx012_ctrl->setting.brightness = value;
			err = 0;
			break;

		case EV_DEFAULT :
			ISX012_WRITE_LIST(ISX012_ExpSetting_Default);
			isx012_ctrl->setting.brightness = value;
			err = 0;
			break;

		case EV_PLUS_1 :
			ISX012_WRITE_LIST(ISX012_ExpSetting_P1Step);
			isx012_ctrl->setting.brightness = value;
			err = 0;
			break;

		case EV_PLUS_2 :
			ISX012_WRITE_LIST(ISX012_ExpSetting_P2Step);
			isx012_ctrl->setting.brightness = value;
			err = 0;
			break;

		case EV_PLUS_3 :
			ISX012_WRITE_LIST(ISX012_ExpSetting_P3Step);
			isx012_ctrl->setting.brightness = value;
			err = 0;
			break;

		case EV_PLUS_4 :
			ISX012_WRITE_LIST(ISX012_ExpSetting_P4Step);
			isx012_ctrl->setting.brightness = value;
			err = 0;
			break;

		default :
			printk(KERN_DEBUG "[ISX012][%s:%d] invalid[%d]\n", __func__, __LINE__, value);
			break;
	}

	return err;
}


static int  isx012_set_iso(int8_t value)
{
	int err = -EINVAL;
	CAM_DEBUG("%d", value);

	switch (value) {
		case ISO_AUTO :
			ISX012_WRITE_LIST(isx012_ISO_Auto);
			err = 0;
			break;

		case ISO_50 :
			ISX012_WRITE_LIST(isx012_ISO_50);
			err = 0;
			break;

		case ISO_100 :
			ISX012_WRITE_LIST(isx012_ISO_100);
			err = 0;
			break;

		case ISO_200 :
			ISX012_WRITE_LIST(isx012_ISO_200);
			err = 0;
			break;

		case ISO_400 :
			ISX012_WRITE_LIST(isx012_ISO_400);
			err = 0;
			break;

		default :
			printk(KERN_DEBUG "[ISX012][%s:%d] invalid[%d]\n", __func__, __LINE__, value);
			break;
	}

	isx012_ctrl->setting.iso = value;
	return err;
}


static int isx012_set_metering(int8_t value)
{
	int err = -EINVAL;
	CAM_DEBUG("%d", value);

	switch (value) {
		case METERING_MATRIX:
			ISX012_WRITE_LIST(isx012_Metering_Matrix);
			isx012_ctrl->setting.metering = value;
			err = 0;
			break;

		case METERING_CENTER:
			ISX012_WRITE_LIST(isx012_Metering_Center);
			isx012_ctrl->setting.metering = value;
			err = 0;
			break;

		case METERING_SPOT:
			ISX012_WRITE_LIST(isx012_Metering_Spot);
			isx012_ctrl->setting.metering = value;
			err = 0;
			break;

		default:
			printk(KERN_DEBUG "[ISX012][%s:%d] invalid[%d]\n", __func__, __LINE__, value);
			err = 0;
			break;
	}

	return err;
}



static int isx012_set_contrast(int8_t value)
{
	int err = -EINVAL;
	CAM_DEBUG("%d",value);

	switch (value) {
		case CONTRAST_MINUS_2 :
			ISX012_WRITE_LIST(isx012_Contrast_Minus_2);
			isx012_ctrl->setting.contrast = value;
			err = 0;
			break;

		case CONTRAST_MINUS_1 :
			ISX012_WRITE_LIST(isx012_Contrast_Minus_1);
			isx012_ctrl->setting.contrast = value;
			err = 0;
			break;

		case CONTRAST_DEFAULT :
			ISX012_WRITE_LIST(isx012_Contrast_Default);
			isx012_ctrl->setting.contrast = value;
			err = 0;
			break;

		case CONTRAST_PLUS_1 :
			ISX012_WRITE_LIST(isx012_Contrast_Plus_1);
			isx012_ctrl->setting.contrast = value;
			err = 0;
			break;

		case CONTRAST_PLUS_2 :
			ISX012_WRITE_LIST(isx012_Contrast_Plus_2);
			isx012_ctrl->setting.contrast = value;
			err = 0;
			break;

		default :
			printk(KERN_DEBUG "[ISX012][%s:%d] invalid[%d]\n", __func__, __LINE__, value);
			err = 0;
			break;
   	}

	return err;
}


static int isx012_set_saturation(int8_t value)
{
	int err = -EINVAL;
	CAM_DEBUG("%d",value);

	switch (value) {
		case SATURATION_MINUS_2 :
			ISX012_WRITE_LIST (isx012_Saturation_Minus_2);
			isx012_ctrl->setting.saturation = value;
			err = 0;
			break;

		case SATURATION_MINUS_1 :
			ISX012_WRITE_LIST(isx012_Saturation_Minus_1);
			isx012_ctrl->setting.saturation = value;
			err = 0;
			break;

		case SATURATION_DEFAULT :
			ISX012_WRITE_LIST(isx012_Saturation_Default);
			isx012_ctrl->setting.saturation = value;
			err = 0;
			break;

		case SATURATION_PLUS_1 :
			ISX012_WRITE_LIST(isx012_Saturation_Plus_1);
			isx012_ctrl->setting.saturation = value;
			err = 0;
			break;

		case SATURATION_PLUS_2 :
			ISX012_WRITE_LIST(isx012_Saturation_Plus_2);
			isx012_ctrl->setting.saturation = value;
			err = 0;
			break;

		default :
			printk(KERN_DEBUG "[ISX012][%s:%d] invalid[%d]\n", __func__, __LINE__, value);
			err = 0;
			break;
   	}

	return err;
}


static int isx012_set_sharpness(int8_t value)
{
	int err = -EINVAL;
	CAM_DEBUG("%d",value);

	switch (value) {
		case SHARPNESS_MINUS_2 :
			ISX012_WRITE_LIST(isx012_Sharpness_Minus_2);
			isx012_ctrl->setting.sharpness = value;
			err = 0;
			break;

		case SHARPNESS_MINUS_1 :
			ISX012_WRITE_LIST(isx012_Sharpness_Minus_1);
			isx012_ctrl->setting.sharpness = value;
			err = 0;
			break;

		case SHARPNESS_DEFAULT :
			ISX012_WRITE_LIST(isx012_Sharpness_Default);
			isx012_ctrl->setting.sharpness = value;
			err = 0;
			break;

		case SHARPNESS_PLUS_1 :
			ISX012_WRITE_LIST(isx012_Sharpness_Plus_1);
			isx012_ctrl->setting.sharpness = value;
			err = 0;
			break;

		case SHARPNESS_PLUS_2 :
			ISX012_WRITE_LIST(isx012_Sharpness_Plus_2);
			isx012_ctrl->setting.sharpness = value;
			err = 0;
			break;

		default :
			printk(KERN_DEBUG "[ISX012][%s:%d] invalid[%d]\n", __func__, __LINE__, value);
			err = 0;
			break;
   	}

	return err;
}



static int  isx012_set_scene(int8_t value)
{
	int err = -EINVAL;
	CAM_DEBUG("%d",value);

	if (value != SCENE_MODE_NONE) {
	     ISX012_WRITE_LIST(isx012_Scene_Default);
	}

	switch (value) {
		case SCENE_MODE_NONE:
			ISX012_WRITE_LIST(isx012_Scene_Default);
			err = 0;
			break;

		case SCENE_MODE_PORTRAIT:
			ISX012_WRITE_LIST(isx012_Scene_Portrait);
			err = 0;
			break;

		case SCENE_MODE_LANDSCAPE:
			ISX012_WRITE_LIST(isx012_Scene_Landscape);
			err = 0;
			break;

		case SCENE_MODE_SPORTS:
			ISX012_WRITE_LIST(isx012_Scene_Sports);
			err = 0;
			break;

		case SCENE_MODE_PARTY_INDOOR:
			ISX012_WRITE_LIST(isx012_Scene_Party_Indoor);
			err = 0;
			break;

		case SCENE_MODE_BEACH_SNOW:
			ISX012_WRITE_LIST(isx012_Scene_Beach_Snow);
			err = 0;
			break;

		case SCENE_MODE_SUNSET:
			ISX012_WRITE_LIST(isx012_Scene_Sunset);
			err = 0;
			break;

		case SCENE_MODE_DUSK_DAWN:
			ISX012_WRITE_LIST(isx012_Scene_Duskdawn);
			err = 0;
			break;

		case SCENE_MODE_FALL_COLOR:
			ISX012_WRITE_LIST(isx012_Scene_Fall_Color);
			err = 0;
			break;

		case SCENE_MODE_NIGHTSHOT:
			ISX012_WRITE_LIST(isx012_Scene_Nightshot);
			err = 0;
			break;

		case SCENE_MODE_BACK_LIGHT:
			ISX012_WRITE_LIST(isx012_Scene_Backlight);
			err = 0;
			break;

		case SCENE_MODE_FIREWORKS:
			ISX012_WRITE_LIST(isx012_Scene_Fireworks);
			err = 0;
			break;

		case SCENE_MODE_TEXT:
			ISX012_WRITE_LIST(isx012_Scene_Text);
			err = 0;
			break;

		case SCENE_MODE_CANDLE_LIGHT:
			ISX012_WRITE_LIST(isx012_Scene_Candle_Light);
			err = 0;
			break;

		default:
			printk(KERN_DEBUG "[ISX012][%s:%d] invalid[%d]\n", __func__, __LINE__, value);
			break;
	}


	isx012_ctrl->setting.scene = value;

	return err;
}

static int isx012_set_preview_size( int32_t value1, int32_t value2)
{
	CAM_DEBUG("value1 %d value2 %d ",value1,value2);

	printk("isx012_set_preview_size ");

	if (value1 == 1280 && value2 == 720) {
		printk("isx012_set_preview_size isx012_1280_Preview_E\n");
		ISX012_WRITE_LIST(isx012_1280_Preview_E);
	} else if (value1 == 800 && value2 == 480) {
		printk("isx012_set_preview_size isx012_800_Preview_E\n");
		ISX012_WRITE_LIST(isx012_800_Preview);
	} else if (value1 == 720 && value2 == 480) {
		printk("isx012_set_preview_size isx012_720_Preview_E\n");
		ISX012_WRITE_LIST(isx012_720_Preview);
	} else {
		printk("isx012_set_preview_size isx012_640_Preview_E\n");
		ISX012_WRITE_LIST(isx012_640_Preview);
	}

	ISX012_WRITE_LIST(ISX012_Preview_Mode);
	isx012_mode_transition_CM();

#if 0
	if(HD_mode) {
		    HD_mode = 0;
		    isx012_WRITE_LIST(isx012_1280_Preview_D)
	}

	switch (value1) {
	case 1280:
		//HD_mode = 1;
		printk("isx012_set_preview_size isx012_1280_Preview_E");
		ISX012_WRITE_LIST(isx012_1280_Preview_E);
		ISX012_WRITE_LIST(ISX012_Preview_Mode);
		break;
	case 720:
		printk("isx012_set_preview_size isx012_720_Preview_E");
		ISX012_WRITE_LIST(isx012_720_Preview);
		ISX012_WRITE_LIST(ISX012_Preview_Mode);
		break;
	case 640:
		printk("isx012_set_preview_size isx012_640_Preview_E");
		ISX012_WRITE_LIST(isx012_640_Preview);
		ISX012_WRITE_LIST(ISX012_Preview_Mode);
		break;
	case 320:
		printk("isx012_set_preview_size isx012_320_Preview_E");
		ISX012_WRITE_LIST(isx012_320_Preview);
		ISX012_WRITE_LIST(ISX012_Preview_Mode);
		break;
	case 176:
		printk("isx012_set_preview_size isx012_176_Preview_E");
		ISX012_WRITE_LIST(isx012_176_Preview);
		ISX012_WRITE_LIST(ISX012_Preview_Mode);
		break;
	default:
		cam_err("Invalid");
		break;

}

#endif

	isx012_ctrl->setting.preview_size= value1;


	return 0;
}




static int isx012_set_picture_size(int value)
{
	CAM_DEBUG("%d",value);
	printk("isx012_set_picture_size ");
#if 0
	switch (value) {
	case EXT_CFG_SNAPSHOT_SIZE_2560x1920_5M:
		ISX012_WRITE_LIST(isx012_5M_Capture);
		//isx012_set_zoom(EXT_CFG_ZOOM_STEP_0);
		break;
	case EXT_CFG_SNAPSHOT_SIZE_2560x1536_4M_WIDE:
		ISX012_WRITE_LIST(isx012_4M_WIDE_Capture);
		break;
	case EXT_CFG_SNAPSHOT_SIZE_2048x1536_3M:
		ISX012_WRITE_LIST(isx012_5M_Capture);
		break;
	case EXT_CFG_SNAPSHOT_SIZE_2048x1232_2_4M_WIDE:
		ISX012_WRITE_LIST(isx012_2_4M_WIDE_Capture);
		break;
	case EXT_CFG_SNAPSHOT_SIZE_1600x1200_2M:
		ISX012_WRITE_LIST(isx012_5M_Capture);
		break;
	case EXT_CFG_SNAPSHOT_SIZE_1600x960_1_5M_WIDE:
		ISX012_WRITE_LIST(isx012_1_5M_WIDE_Capture);
		break;
	case EXT_CFG_SNAPSHOT_SIZE_1280x960_1M:
		ISX012_WRITE_LIST(isx012_1M_Capture);
		break;
	case EXT_CFG_SNAPSHOT_SIZE_800x480_4K_WIDE:
		ISX012_WRITE_LIST(isx012_4K_WIDE_Capture);
		break;
	case EXT_CFG_SNAPSHOT_SIZE_640x480_VGA:
		ISX012_WRITE_LIST(isx012_VGA_Capture);
		break;
	case EXT_CFG_SNAPSHOT_SIZE_320x240_QVGA:
		ISX012_WRITE_LIST(isx012_QVGA_Capture);
		break;
	default:
		cam_err("Invalid");
		return -EINVAL;
	}


//	if(size != EXT_CFG_SNAPSHOT_SIZE_2560x1920_5M && isx012_status.zoom != EXT_CFG_ZOOM_STEP_0)
//		isx012_set_zoom(isx012_status.zoom);


	isx012_ctrl->setting.snapshot_size = value;
#endif
	return 0;
}



static int isx012_exif_iso(void)
{
	int exif_iso = 0;
	int err = 0;
	short unsigned int r_data[1] = {0};
	unsigned int iso_table[19] = {25, 32, 40, 50, 64, 80, 100, 125, 160, 200, 250, 320, 400, 500, 640, 800, 1000, 1250, 1600};

	err = isx012_i2c_read(0x019A, r_data);	//ISOSENE_OUT
	exif_iso = r_data[0];
	//printk(KERN_DEBUG "[exif_iso][%s:%d] r_data(%d) exif_iso(%d)\n", __func__, __LINE__, r_data[0], exif_iso);

	isx012_exif->iso = iso_table[exif_iso-1];

	return exif_iso;
}

static int isx012_get_exif(int *exif_cmd, int *val)
{
	switch(*exif_cmd){
                case EXIF_TV:
			//isx012_exif_shutter_speed();
			(*val) = isx012_exif->shutterspeed;
			break;

                case EXIF_ISO:
			isx012_exif_iso();
                        (*val) = isx012_exif->iso;
                        break;

                case EXIF_FLASH:
                        (*val) = isx012_exif->flash;
                        break;
#if 0
                case EXIF_EXPOSURE_TIME:
                        (*val) = isx012_exif->exptime;
                        break;

                case EXIF_TV:
                        (*val) = isx012_exif->tv;
                        break;

                case EXIF_BV:
                        (*val) = isx012_exif->bv;
                        break;

                case EXIF_EBV:
                        (*val) = isx012_exif->ebv;
                        break;
#endif
                default:
			printk(KERN_DEBUG "[%s:%d] invalid(%d)\n", __func__, __LINE__, *exif_cmd);
                        break;
	}

	return 0;
}

static int isx012_cancel_autofocus(void)
{
	CAM_DEBUG("E");

	switch (isx012_ctrl->setting.afmode) {
		case FOCUS_MODE_AUTO :
			ISX012_WRITE_LIST(ISX012_AF_Cancel_Macro_OFF);
			ISX012_WRITE_LIST(ISX012_AF_Macro_OFF);
			ISX012_WRITE_LIST(ISX012_AF_ReStart);
			break;

		case FOCUS_MODE_MACRO :
			ISX012_WRITE_LIST(ISX012_AF_Cancel_Macro_ON);
			ISX012_WRITE_LIST(ISX012_AF_Macro_ON);
			ISX012_WRITE_LIST(ISX012_AF_ReStart);
			break;

		default :
			printk(KERN_DEBUG "[ISX012][%s:%d] invalid[%d]\n", __func__, __LINE__, isx012_ctrl->setting.afmode);
   	}

	CAM_DEBUG("X");

	return 0;
}

static int isx012_set_focus_mode(int value1, int value2)
{
	switch (value1) {
		case FOCUS_MODE_AUTO :
			ISX012_WRITE_LIST(ISX012_AF_Macro_OFF);
			if(value2 == 1) {
				ISX012_WRITE_LIST(ISX012_AF_ReStart);
				isx012_ctrl->setting.afmode = FOCUS_MODE_AUTO;
			}
			break;

		case FOCUS_MODE_MACRO :
			ISX012_WRITE_LIST(ISX012_AF_Macro_ON);
			if(value2 == 1) {
				ISX012_WRITE_LIST(ISX012_AF_ReStart);
				isx012_ctrl->setting.afmode = FOCUS_MODE_MACRO;
			}
			break;

		default :
			printk(KERN_DEBUG "[ISX012][%s:%d] invalid[%d/%d]\n", __func__, __LINE__, value1, value2);
			break;
   	}

	return 0;
}

static int isx012_set_movie_mode(int mode)
{
	CAM_DEBUG("E");

	if(mode == SENSOR_MOVIE) {
		printk("isx012_set_movie_mode Camcorder_Mode_ON\n");
		ISX012_WRITE_LIST(ISX012_Camcorder_Mode_ON);
		isx012_ctrl->status.camera_status = 1;
	} else {
		isx012_ctrl->status.camera_status = 0;
	}

	if ((mode != SENSOR_CAMERA) && (mode != SENSOR_MOVIE)) {
		return -EINVAL;
	}

	return 0;
}


static int isx012_check_dataline(s32 val)
{
	int err = -EINVAL;

	CAM_DEBUG("%s", val ? "ON" : "OFF");
	if (val) {
		printk(KERN_DEBUG "[FACTORY-TEST] DTP ON\n");
		ISX012_WRITE_LIST(isx012_DTP_init);
		DtpTest = 1;
		err = 0;

	} else {
		printk(KERN_DEBUG "[FACTORY-TEST] DTP OFF\n");
		ISX012_WRITE_LIST(isx012_DTP_stop);
		DtpTest = 0;
		err = 0;
	}

	return err;
}

static int isx012_mipi_mode(int mode)
{
	int err = 0;
	struct msm_camera_csi_params isx012_csi_params;

	CAM_DEBUG("E");

	if (!isx012_ctrl->status.config_csi1) {
		isx012_csi_params.lane_cnt = 2;
		isx012_csi_params.data_format = 0x1E;
		isx012_csi_params.lane_assign = 0xe4;
		isx012_csi_params.dpcm_scheme = 0;
		isx012_csi_params.settle_cnt = 24;// 0x14; //0x7; //0x14;
		err = msm_camio_csi_config(&isx012_csi_params);

		if (err < 0)
			printk(KERN_ERR "config csi controller failed \n");

		isx012_ctrl->status.config_csi1 = 1;
	} else {
		CAM_DEBUG("X : already started");
	}

	CAM_DEBUG("X");
	return err;
}

static int isx012_start(void)
{
	int err = 0;

	CAM_DEBUG("E");

	printk("isx012_start ");

	if (isx012_ctrl->status.started) {
		CAM_DEBUG("X : already started");
		return -EINVAL;
	}
	isx012_mipi_mode(1);
	msleep(30); //=> Please add some delay


	//ISX012_WRITE_LIST(isx012_init_reg1);
	msleep(10);

        //ISX012_WRITE_LIST(isx012_init_reg2);

	isx012_ctrl->status.initialized = 1;
	isx012_ctrl->status.started = 1;
	isx012_ctrl->status.AE_AWB_hunt = 1;
	isx012_ctrl->status.start_af = 0;

	CAM_DEBUG("X");

	return err;
}

#if 0
static long isx012_video_config(int mode)
{
	int err = 0; //-EINVAL;
	CAM_DEBUG("E");

	return err;
}

static long isx012_snapshot_config(int mode)
{
	CAM_DEBUG("E");

	//ISX012_WRITE_LIST(ISX012_Capture_SizeSetting);
	//ISX012_WRITE_LIST(isx012_Capture_Start);

	//isx012_mode_transition_CM();


	return 0;
}
#endif

static long isx012_set_sensor_mode(int mode)
{
	int err = -EINVAL;
	short unsigned int r_data[1] = {0};
//	short unsigned int r_data2[2] = {0, 0};
	char awbsts[1] = {0};
	int timeout_cnt = 0;
#if 0
	unsigned int hunt1 = 0;
	char hunt2[1] = {0};
#endif
	printk("isx012_set_sensor_mode\n");

	switch (mode) {
		case SENSOR_PREVIEW_MODE:
			CAM_DEBUG("SENSOR_PREVIEW_MODE START");

			if(iscapture == 1)
			{
				if (((isx012_ctrl->setting.flash_mode == 1) && (gLowLight_check)) || (isx012_ctrl->setting.flash_mode == 2)) {
					isx012_set_flash(0, 50);
					ISX012_WRITE_LIST(ISX012_Flash_OFF);
					if (isx012_ctrl->setting.whiteBalance == WHITE_BALANCE_AUTO) {
						isx012_i2c_write_multi(0x0282, 0x20, 0x01);
					}
					isx012_i2c_write_multi(0x8800, 0x01, 0x01);
					err = isx012_set_focus_mode(isx012_ctrl->setting.afmode, 0);
				}else{
					err = isx012_set_focus_mode(isx012_ctrl->setting.afmode, 0);
					if (isx012_ctrl->setting.scene == SCENE_MODE_NIGHTSHOT)
						ISX012_WRITE_LIST(ISX012_Lowlux_Night_Reset);
				}
				

				ISX012_WRITE_LIST(ISX012_Preview_Mode);
				isx012_mode_transition_CM();

				iscapture = 0;
				isx012_ctrl->status.touchaf = 0;
				isx012_ctrl->status.start_af = 0;
			} else {
				isx012_start();
			}

			if (isx012_ctrl->status.AE_AWB_hunt == 1) {
#ifdef CONFIG_LOAD_FILE
				gERRSCL_AUTO = isx012_define_read("ERRSCL_AUTO",6);
				gUSER_AESCL_AUTO = isx012_define_read("USER_AESCL_AUTO",6);
				gERRSCL_NOW = isx012_define_read("ERRSCL_NOW",6);
				gUSER_AESCL_NOW = isx012_define_read("USER_AESCL_NOW",6);
				gAE_OFSETVAL = isx012_define_read("AE_OFSETVAL",4);
				gAE_MAXDIFF = isx012_define_read("AE_MAXDIFF",4);
				gGLOWLIGHT_DEFAULT = isx012_define_read("GLOWLIGHT_DEFAULT", 6);
				gGLOWLIGHT_ISO50 = isx012_define_read("GLOWLIGHT_ISO50", 6);
				gGLOWLIGHT_ISO100 = isx012_define_read("GLOWLIGHT_ISO100", 6);
				gGLOWLIGHT_ISO200 = isx012_define_read("GLOWLIGHT_ISO200", 6);
				gGLOWLIGHT_ISO400 = isx012_define_read("GLOWLIGHT_ISO400", 6);
				isx012_define_table();
#else
				gERRSCL_AUTO = ERRSCL_AUTO;
				gUSER_AESCL_AUTO = USER_AESCL_AUTO;
				gERRSCL_NOW = ERRSCL_NOW;
				gUSER_AESCL_NOW = USER_AESCL_NOW;
				gAE_OFSETVAL = AE_OFSETVAL;
				gAE_MAXDIFF = AE_MAXDIFF;
				gGLOWLIGHT_DEFAULT = GLOWLIGHT_DEFAULT;
				gGLOWLIGHT_ISO50 = GLOWLIGHT_ISO50;
				gGLOWLIGHT_ISO100 = GLOWLIGHT_ISO100;
				gGLOWLIGHT_ISO200 = GLOWLIGHT_ISO200;
				gGLOWLIGHT_ISO400 = GLOWLIGHT_ISO400;
#endif
#if 0
				printk(KERN_DEBUG "gERRSCL_AUTO = 0x%x", gERRSCL_AUTO);
				printk(KERN_DEBUG "gUSER_AESCL_AUTO = 0x%x", gUSER_AESCL_AUTO);
				printk(KERN_DEBUG "gERRSCL_NOW = 0x%x", gERRSCL_NOW);
				printk(KERN_DEBUG "gUSER_AESCL_NOW = 0x%x", gUSER_AESCL_NOW);
				printk(KERN_DEBUG "gAE_OFSETVAL = %d", gAE_OFSETVAL);
				printk(KERN_DEBUG "gAE_MAXDIFF = %d", gAE_MAXDIFF);
				printk(KERN_DEBUG "gGLOWLIGHT_DEFAULT = 0x%x\n", gGLOWLIGHT_DEFAULT);
				printk(KERN_DEBUG "gGLOWLIGHT_ISO50 = 0x%x\n", gGLOWLIGHT_ISO50);
				printk(KERN_DEBUG "gGLOWLIGHT_ISO100 = 0x%x\n", gGLOWLIGHT_ISO100);
				printk(KERN_DEBUG "gGLOWLIGHT_ISO200 = 0x%x\n", gGLOWLIGHT_ISO200);
				printk(KERN_DEBUG "gGLOWLIGHT_ISO400 = 0x%x\n", gGLOWLIGHT_ISO400);
#endif
#if 0
				timeout_cnt = 0;
				do {
					if (timeout_cnt > 0){
						mdelay(10);
					}
					timeout_cnt++;
					err = isx012_i2c_read_multi(0x8A00, r_data2, 2);	//SHT_TIME_OUT_L
					hunt1 = r_data2[1] << 8 | r_data2[0];
					if (timeout_cnt > ISX012_DELAY_RETRIES2) {
						pr_err("%s: %d :Entering hunt1 delay timed out \n", __func__, __LINE__);
						break;
					}
				} while ((hunt1 != 0x00));

				timeout_cnt = 0;
				do {
					if (timeout_cnt > 0){
						mdelay(10);
					}
					timeout_cnt++;
					err = isx012_i2c_read(0x8A24, r_data);
					hunt2[0] = r_data[0];
					if (timeout_cnt > ISX012_DELAY_RETRIES2) {
						pr_err("%s: %d :Entering hunt2 delay timed out \n", __func__, __LINE__);
						break;
					}
				} while ((hunt2[0] != 0x2));
#endif
				mdelay(30);
				isx012_ctrl->status.AE_AWB_hunt = 0;
			} else {
			}
			//if (isx012_ctrl->sensor_mode != SENSOR_MOVIE)
			//err= isx012_video_config(SENSOR_PREVIEW_MODE);

			break;

		case SENSOR_SNAPSHOT_MODE:
		case SENSOR_RAW_SNAPSHOT_MODE:
			CAM_DEBUG("SENSOR_SNAPSHOT_MODE START");

			if (((isx012_ctrl->setting.flash_mode == 1) && (gLowLight_check)) || (isx012_ctrl->setting.flash_mode == 2)) {
				if (isx012_ctrl->status.touchaf) {
					//pr_err("%s: %d :[for tuning] touchaf :%d \n", __func__, __LINE__, isx012_ctrl->status.touchaf);
					isx012_i2c_write_multi(0x0294, 0x02, 0x01);
					isx012_i2c_write_multi(0x0297, 0x02, 0x01);
					isx012_i2c_write_multi(0x029A, 0x02, 0x01);
					isx012_i2c_write_multi(0x029E, 0x02, 0x01);

					//wait 1V time (66ms)
					mdelay(66);
				}
			}

			isx012_set_flash(isx012_ctrl->setting.flash_mode, 50);

			CAM_DEBUG("ISX012_Capture_Mode start");
			if (isx012_ctrl->status.start_af == 0) {
				if (isx012_ctrl->status.touchaf) {
					//touch-af window
					ISX012_WRITE_LIST(ISX012_AF_TouchSAF_OFF);
				} else {
					ISX012_WRITE_LIST(ISX012_AF_SAF_OFF);
				}

				//wait 1V time (66ms)
				mdelay(120);
				isx012_ctrl->status.touchaf = 0;
			}

			if ((isx012_ctrl->setting.scene == SCENE_MODE_NIGHTSHOT) && (gLowLight_check)) {
				ISX012_WRITE_LIST(ISX012_Lowlux_Night_Capture_Mode);
			} else {
				ISX012_WRITE_LIST(ISX012_Capture_Mode);
			}
			//isx012_i2c_write_multi(0x8800, 0x01, 0x01);
			isx012_mode_transition_CM();

			isx012_exif_shutter_speed();
			CAM_DEBUG("ISX012_Capture_Mode end");

	        	if (((isx012_ctrl->setting.flash_mode == 1) && (gLowLight_check)) || (isx012_ctrl->setting.flash_mode == 2)) {
				//wait 1V time (150ms)
				mdelay(210);

		    		timeout_cnt = 0;
				do {
					if (timeout_cnt > 0){
						mdelay(1);
					}
					timeout_cnt++;
					err = isx012_i2c_read(0x8A24, r_data);
					awbsts[0] = r_data[0];			
					if (timeout_cnt > ISX012_DELAY_RETRIES) {
						pr_err("%s: %d :Entering delay awbsts timed out \n", __func__, __LINE__);
						break;
					}
					if ( awbsts[0] == 0x6 ) {	/* Flash Saturation*/
						pr_err("%s: %d :Entering delay awbsts awbsts[0x%x] \n", __func__, __LINE__, awbsts[0]);
						break;
				    }
					if ( awbsts[0] == 0x4 ) {	/* Flash Saturation*/
						pr_err("%s: %d :Entering delay awbsts awbsts[0x%x] \n", __func__, __LINE__, awbsts[0]);
						break;
				    }
				} while (awbsts[0] != 0x2);
			}
			iscapture = 1;
			//err = isx012_snapshot_config(SENSOR_SNAPSHOT_MODE);
			break;


		case SENSOR_SNAPSHOT_TRANSFER:
			CAM_DEBUG("SENSOR_SNAPSHOT_TRANSFER START");
			break;

		default:
			return 0;//-EFAULT;
	}

	return 0;
}

#ifdef FACTORY_TEST
struct class *camera_class;
struct device *isx012_dev;

static ssize_t cameraflash_file_cmd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	/* Reserved */
	return 0;
}

static ssize_t cameraflash_file_cmd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value = 0, i = 0;
	int torch = 0, torch2 = 0, torch3 = 0;

	sscanf(buf, "%d", &value);

	if (value == 0) {
		accessibility_torch = 0;
		printk(KERN_INFO "[Accessary] flash OFF\n");
		gpio_set_value_cansleep(CAM_FLASH_EN, 0);
		gpio_set_value_cansleep(CAM_FLASH_SET, 0);
	} else {
		accessibility_torch = 1;
		printk(KERN_INFO "[Accessary] flash ON\n");
		printk(KERN_DEBUG "FLASH MOVIE MODE LED_MODE_TORCH\n");
		gpio_set_value_cansleep(CAM_FLASH_EN, 0);
		torch = gpio_get_value(CAM_FLASH_EN);

		for (i = 5; i > 1; i--) {
			gpio_set_value_cansleep(CAM_FLASH_SET, 1);
			torch2 = gpio_get_value(CAM_FLASH_SET);
			udelay(1);
			gpio_set_value_cansleep(CAM_FLASH_SET, 0);
			torch3 = gpio_get_value(CAM_FLASH_SET);
			udelay(1);
		}
		gpio_set_value_cansleep(CAM_FLASH_SET, 1);
		usleep(2*1000);
	}

	return size;
}

static DEVICE_ATTR(rear_flash, 0660, cameraflash_file_cmd_show, cameraflash_file_cmd_store);

static ssize_t camtype_file_cmd_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char camType[] = "SONY_ISX012_NONE";

	return sprintf(buf, "%s", camType);
}

static ssize_t camtype_file_cmd_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}

static DEVICE_ATTR(rear_camtype, 0660, camtype_file_cmd_show, camtype_file_cmd_store);

#endif

static int isx012_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
	int err = 0;
	int temp = 0;

	printk("POWER ON START\n");
	printk("isx012_sensor_init_probe\n");

	gpio_tlmm_config(GPIO_CFG(CAM_5M_EN, OFF, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE); //stanby

	gpio_set_value_cansleep(CAM_5M_RST, OFF);
	gpio_set_value_cansleep(CAM_5M_EN, OFF);

	cam_ldo_power_on();

	msm_camio_clk_rate_set(24000000); //MCLK on
	udelay(50); //min 50us

	gpio_set_value_cansleep(CAM_FRONT_RST, ON); //VT Rest on
	temp = gpio_get_value(CAM_FRONT_RST);
	//printk("[isx012] VT Rest ON : %d\n", temp);
	mdelay(100);

#if 1   
	//Arm Go
	printk("[isx012] I2C sleep mode \n");
	isx012_i2c_write_multi_temp(0xFCFC, 0xD0, 0x00);	
	isx012_i2c_write_multi_temp(0x0010, 0x00, 0x01);
	isx012_i2c_write_multi_temp(0x1030, 0x00, 0x00);
	isx012_i2c_write_multi_temp(0x0014, 0x00, 0x01);	
	mdelay(10);
   
	//sleep mode
	isx012_i2c_write_multi_temp(0x0028, 0xD0, 0x00);	
	isx012_i2c_write_multi_temp(0x002A, 0xB0, 0xB0);	
	isx012_i2c_write_multi_temp(0x0F12, 0x00, 0x01);	

	isx012_i2c_write_multi_temp(0x0028, 0x70, 0x00);	
	isx012_i2c_write_multi_temp(0x002A, 0x02, 0x2E);	
	isx012_i2c_write_multi_temp(0x0F12, 0x00, 0x01);	
	isx012_i2c_write_multi_temp(0x0F12, 0x00, 0x01);	//#REG_TC_GP_SleepModeChanged 
	printk("[isx012] I2C sleep mode end\n");
#endif

	gpio_set_value_cansleep(CAM_FRONT_EN, OFF); //VT STBY off
	temp = gpio_get_value(CAM_FRONT_EN);
	//printk("[isx012] VT STBY off : %d\n", temp);
	udelay(10);

	gpio_set_value_cansleep(CAM_5M_RST, ON); // 5M REST
	temp = gpio_get_value(CAM_5M_RST);
	//printk("[isx012] CAM_5M_RST : %d\n", temp);
	mdelay(7);

	//printk("[isx012] Mode Trandition 1\n");

	err = isx012_mode_transition_OM();
	if (err == -EIO) {
		printk("[isx012] start1 fail!\n");
		return -EIO;
	}

	mdelay(10);

	ISX012_WRITE_LIST(ISX012_Pll_Setting_4);
	//printk("[isx012] Mode Trandition 2\n");

	mdelay(10);
	err = isx012_mode_transition_OM();
	if (err == -EIO) {
		printk("[isx012] start2 fail!\n");
		return -EIO;
	}


	//printk("[isx012] MIPI write\n");

	//isx012_i2c_write_multi_temp(0x5008, 0x00, 0x01);
	isx012_i2c_write_multi(0x5008, 0x00, 0x01);
	isx012_Sensor_Calibration();
	mdelay(1);

	gpio_set_value_cansleep(CAM_5M_EN, ON); //STBY 0 -> 1
	temp = gpio_get_value(CAM_5M_EN);
	//printk("[isx012] CAM_5M_ISP_STNBY : %d\n", temp);

	mdelay(20);
	err = isx012_mode_transition_OM();
	if (err == -EIO) {
		printk("[isx012] start3 fail!\n");
		return -EIO;
	}

	isx012_mode_transition_CM();

	mdelay(50);

#if defined(CONFIG_USA_MODEL_SGH_I577) || defined(CONFIG_CAN_MODEL_SGH_I577R)
	isx012_i2c_write_multi(0x008C, 0x03, 0x01);	//READVECT_CAP : 180
	isx012_i2c_write_multi(0x008D, 0x03, 0x01);	//READVECT_CAP : 180
	isx012_i2c_write_multi(0x008E, 0x03, 0x01);	//READVECT_CAP : 180
#endif
	ISX012_WRITE_LIST(ISX012_Init_Reg);
	ISX012_WRITE_LIST(ISX012_Preview_SizeSetting);
	ISX012_WRITE_LIST(ISX012_Preview_Mode);

	CAM_DEBUG("POWER ON END ");

	return err;
}

int isx012_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int err = 0;
	CAM_DEBUG("E");
	printk("isx012_sensor_open_init ");

	isx012_ctrl = kzalloc(sizeof(struct isx012_ctrl_t), GFP_KERNEL);
	if (!isx012_ctrl) {
		cam_err("failed!");
		err = -ENOMEM;
		goto init_done;
	}

	isx012_exif = kzalloc(sizeof(struct isx012_exif_data), GFP_KERNEL);
	if (!isx012_exif) {
		printk("allocate isx012_exif_data failed!\n");
		err = -ENOMEM;
		goto init_done;
	}

	if (data)
		isx012_ctrl->sensordata = data;

#ifdef CONFIG_LOAD_FILE
		isx012_regs_table_init();
#endif

	err = isx012_sensor_init_probe(data);
	if (err < 0) {
		cam_err("isx012_sensor_open_init failed!");
		goto init_fail;
	}

	isx012_ctrl->status.started = 0;
	isx012_ctrl->status.initialized = 0;
	isx012_ctrl->status.config_csi1 = 0;
	isx012_ctrl->status.cancel_af = 0;
	isx012_ctrl->status.camera_status = 0;
	isx012_ctrl->status.initialized = 0;
	isx012_ctrl->status.apps = 0;

	isx012_ctrl->setting.check_dataline = 0;
	isx012_ctrl->setting.camera_mode = SENSOR_CAMERA;



	CAM_DEBUG("X");
init_done:
	return err;

init_fail:
	kfree(isx012_ctrl);
	return err;
}

static int isx012_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&isx012_wait_queue);
	return 0;
}

static int calculate_AEgain_offset(uint16_t ae_auto, uint16_t ae_now, int16_t ersc_auto, int16_t ersc_now)
{
	int err = -EINVAL;
	int16_t aediff, aeoffset;

	//AE_Gain_Offset = Target - ERRSCL_NOW
	aediff = (ae_now + ersc_now) - (ae_auto + ersc_auto);

	if(aediff < 0) {
		aediff = 0;
	}

	if(ersc_now < 0) {
		if(aediff >= AE_MAXDIFF) {
			aeoffset = -AE_OFSETVAL - ersc_now;
		} else {
#ifdef CONFIG_LOAD_FILE
			aeoffset = -gtable_buf[aediff/10] - ersc_now;
#else
			aeoffset = -aeoffset_table[aediff/10] - ersc_now;
#endif
		}
	} else {
		if(aediff >= AE_MAXDIFF) {
			aeoffset = -AE_OFSETVAL;
		} else {
#ifdef CONFIG_LOAD_FILE
			aeoffset = -gtable_buf[aediff/10];
#else
			aeoffset = -aeoffset_table[aediff/10];
#endif
		}
	}
	//printk(KERN_DEBUG "[For tuning] aeoffset(%d) | aediff(%d) = (ae_now(%d) + ersc_now(%d)) - (ae_auto(%d) + ersc_auto(%d))\n", aeoffset, aediff, ae_now, ersc_now, ae_auto, ersc_auto);


	// SetAE Gain offset
	err = isx012_i2c_write_multi(CAP_GAINOFFSET, aeoffset, 2);

	return err;
}

static int isx012_sensor_af_status(void)
{
	int err = 0, status = 0;
	unsigned char r_data[2] = {0,0};

	//printk(KERN_DEBUG "[isx012] %s/%d\n", __func__, __LINE__);

	err = isx012_i2c_read_multi(0x8B8A, (unsigned short*)r_data, 1);
	//printk(KERN_DEBUG "%s:%d status(0x%x) r_data(0x%x %x)\n", __func__, __LINE__, status, r_data[1], r_data[0]);
	status = r_data[0];
	//printk(KERN_DEBUG "[isx012] %s/%d status(0x%x)\n", __func__, __LINE__, status);
	if (status == 0x8) {
		printk(KERN_DEBUG "[isx012] success\n");
		return 1;
	}

	return 0;
}

static int isx012_set_af_stop(int af_check)
{
	int err = -EINVAL;

	//printk(KERN_DEBUG "[isx012] %s/%d isx012_ctrl->status.cancel_af_running(%d)\n", __func__, __LINE__, isx012_ctrl->status.cancel_af_running);

	isx012_ctrl->status.start_af = 0;

	if (af_check == 1) {	//After AF check
		isx012_ctrl->status.cancel_af_running ++;
		if (isx012_ctrl->status.cancel_af_running == 1) {
			if (isx012_ctrl->status.touchaf) {
				//touch-af window
				ISX012_WRITE_LIST(ISX012_AF_TouchSAF_OFF);
			} else {
				ISX012_WRITE_LIST(ISX012_AF_SAF_OFF);
			}

			//wait 1V time (66ms)
			mdelay(66);
			isx012_ctrl->status.touchaf = 0;

			if (((isx012_ctrl->setting.flash_mode == 1) && (gLowLight_check)) || (isx012_ctrl->setting.flash_mode == 2)) {
				isx012_set_flash(0, 50);
				ISX012_WRITE_LIST(ISX012_Flash_OFF);
				if (isx012_ctrl->setting.whiteBalance == WHITE_BALANCE_AUTO) {
					isx012_i2c_write_multi(0x0282, 0x20, 0x01);
				}
				isx012_i2c_write_multi(0x8800, 0x01, 0x01);
			}
			err = isx012_cancel_autofocus();
		}
	} else {
		isx012_ctrl->status.cancel_af = 1;
		err = 0;
	}

	return 0;
}

static int isx012_set_af_start(void)
{
	int err = -EINVAL;
	int timeout_cnt = 0;
	short unsigned int r_data[1] = {0};
	uint16_t ae_data[1] = {0};
	int16_t ersc_data[1] = {0};
	char modesel_fix[1] = {0}, half_move_sts[1] = {0};

	CAM_DEBUG("E");

	isx012_ctrl->status.start_af++;

	if(isx012_ctrl->status.cancel_af == 1) {
		cam_info("AF START : af cancel...");
		return 10;
	}

	if (isx012_ctrl->status.touchaf) {
		//touch-af window
	} else {
		ISX012_WRITE_LIST(ISX012_AF_Window_Reset);
	}

	if (((isx012_ctrl->setting.flash_mode == 1) && (gLowLight_check)) || (isx012_ctrl->setting.flash_mode == 2)) {
		//AE line change - AE line change value write
		ISX012_WRITE_LIST(ISX012_Flash_AELINE);

		//wait 1V time (60ms)
		mdelay(60);

		//Read AE scale - ae_auto, ersc_auto
		err = isx012_i2c_read(0x01CE, ae_data);
		g_ae_auto = ae_data[0];
		err = isx012_i2c_read(0x01CA, ersc_data);
		g_ersc_auto = ersc_data[0];

		//Flash On set
		if (isx012_ctrl->setting.whiteBalance == WHITE_BALANCE_AUTO) {
			isx012_i2c_write_multi(0x0282, 0x00, 0x01);
		}
		ISX012_WRITE_LIST(ISX012_Flash_ON);

		//Fast AE, AWB, AF start
		isx012_i2c_write_multi(0x8800, 0x01, 0x01);

		//wait 1V time (40ms)
		mdelay(40);

		isx012_set_flash(3 , 50);

		timeout_cnt = 0;
		do {
			if(isx012_ctrl->status.cancel_af == 1) {
				cam_info("AF STOP : af canncel..modesel_fix");
				break;
			}
			if (timeout_cnt > 0){
				mdelay(10);
			}
			timeout_cnt++;
			err = isx012_i2c_read(0x0080, r_data);
			modesel_fix[0] = r_data[0];
			if (timeout_cnt > ISX012_DELAY_RETRIES) {
				pr_err("%s: %d :Entering delay modesel_fix timed out \n", __func__, __LINE__);
				break;
			}
		} while ((modesel_fix[0] != 0x1));

		timeout_cnt = 0;
		do {
			if(isx012_ctrl->status.cancel_af == 1) {
				cam_info("AF STOP : af canncel..half_move_sts");
				break;
			}
			if (timeout_cnt > 0){
				mdelay(10);
			}
			timeout_cnt++;
			err = isx012_i2c_read(0x01B0, r_data);
			half_move_sts[0] = r_data[0];
			if (timeout_cnt > ISX012_DELAY_RETRIES) {
				pr_err("%s: %d :Entering half_move_sts delay timed out \n", __func__, __LINE__);
				break;
			}
		}while ((half_move_sts[0] != 0x0));
	} else {
		if ((isx012_ctrl->setting.scene == SCENE_MODE_NIGHTSHOT) && (gLowLight_check)) {
			ISX012_WRITE_LIST(ISX012_Lowlux_night_Halfrelease_Mode);
		} else if ((!isx012_ctrl->status.apps) && (isx012_ctrl->status.start_af > 1)) {
			if(isx012_ctrl->status.start_af == 2) {
				isx012_set_af_stop(1);
				isx012_ctrl->status.start_af = 2;
			}
			ISX012_WRITE_LIST(ISX012_Barcode_SAF);
		} else {
			ISX012_WRITE_LIST(ISX012_Halfrelease_Mode);
		}

		//wait 1V time (40ms)
		mdelay(40);
	}

	return 0;
}

static int isx012_sensor_af_result(void)
{
	int ret = 0;
	int status = 0;

	CAM_DEBUG("E");

	isx012_i2c_read(0x8B8B, (unsigned short *)&status);
	if ((status & 0x1) == 0x1) {
		printk(KERN_DEBUG "[isx012] AF success\n");
		ret = 1;
	} else if ((status & 0x1) == 0x0) {
		printk(KERN_DEBUG "[isx012] AF fail\n");
		ret = 2;
	}

	return ret;
}

static int isx012_get_af_status(void)
{
	int err = -EINVAL;
	int af_result = -EINVAL, af_status = -EINVAL;
	uint16_t ae_data[1] = {0};
	int16_t ersc_data[1] = {0};
	int16_t aescl_data[1] = {0};
	int16_t ae_scl = 0;

	//printk(KERN_DEBUG "[isx012] %s/%d\n", __func__, __LINE__);
	if(isx012_ctrl->status.cancel_af == 1) {
		cam_info("AF STATUS : af cancel...");
		return 10;
	}

	af_status = isx012_sensor_af_status();
	if (af_status == 1) {
		isx012_i2c_write_multi(0x0012, 0x10, 0x01);
		af_result = isx012_sensor_af_result();

		if (((isx012_ctrl->setting.flash_mode == 1) && (gLowLight_check)) || (isx012_ctrl->setting.flash_mode == 2)) {
			//Read AE scale - ae_now, ersc_now
			err = isx012_i2c_read(0x01CC, ersc_data);
			g_ersc_now = ersc_data[0];
			err = isx012_i2c_read(0x01D0, ae_data);
			g_ae_now = ae_data[0];
			err = isx012_i2c_read(0x8BC0, aescl_data);
			ae_scl = aescl_data[0];
		}

		if (isx012_ctrl->status.touchaf) {
			//touch-af window
			ISX012_WRITE_LIST(ISX012_AF_TouchSAF_OFF);
		} else {
			ISX012_WRITE_LIST(ISX012_AF_SAF_OFF);
		}

		//wait 1V time (66ms)
		mdelay(66);

		// AE SCL
		if (((isx012_ctrl->setting.flash_mode == 1) && (gLowLight_check)) || (isx012_ctrl->setting.flash_mode == 2)) {
			ae_scl = ae_scl - 4129;
			//printk(KERN_DEBUG "[For tuning] original ae_scl(%d) - 4129 => ae_scl(%d) \n", ae_scl + 4129, ae_scl);
			isx012_i2c_write_multi(0x5E02, ae_scl, 0x02);

			err = calculate_AEgain_offset(g_ae_auto, g_ae_now, g_ersc_auto, g_ersc_now);
			////isx012_i2c_write_multi(0x5000, 0x0A, 0x01);
			isx012_set_flash(0 , 50);
		}
	} else {
		af_result = af_status;
	}

	return af_result;
}

#if 1
static int isx012_set_touch_auto_focus(int value1)
{
	int x,y;
	unsigned int H_ratio, V_ratio;
	unsigned int AF_OPD4_HDELAY, AF_OPD4_VDELAY, AF_OPD4_HVALID, AF_OPD4_VVALID;
	
	x = isx012_ctrl->status.pos_x;
	y = isx012_ctrl->status.pos_y;

	H_ratio = 324;	//H_RATIO : 3.24 = 2592 / 800
	V_ratio = 405;	//V_RATIO : 4.05 = 1944 / 480

	//AE value
	//OPD4 Not touchAF value
	//H : 2048 = 896 + 256 + 896
	//V : 1536 = 512 + 512 +512
	AF_OPD4_HDELAY = 486;
	AF_OPD4_VDELAY = 259;
	AF_OPD4_HVALID = 259;
	AF_OPD4_VVALID = 324;


        CAM_DEBUG("%d", value1);

	if (value1 == 0) // Stop touch AF
	{
		if (iscapture != 1) {
			ISX012_WRITE_LIST(ISX012_AF_TouchSAF_OFF);
			isx012_ctrl->status.touchaf=0;	  /* Have to set Zero, otherwise in Touch AF followed by Shutter AF(long press) , AE and AWB locking will not work */

			//wait 1V time (66ms)
			mdelay(66);

			if (((isx012_ctrl->setting.flash_mode == 1) && (gLowLight_check)) || (isx012_ctrl->setting.flash_mode == 2)) {
				isx012_set_flash(0, 50);
				ISX012_WRITE_LIST(ISX012_Flash_OFF);
				if (isx012_ctrl->setting.whiteBalance == WHITE_BALANCE_AUTO) {
					isx012_i2c_write_multi(0x0282, 0x20, 0x01);
				}
				isx012_i2c_write_multi(0x8800, 0x01, 0x01);
			}

			isx012_cancel_autofocus();
		}
	} else {	// Start touch AF
		// Calcurate AF window size & position
		AF_OPD4_HVALID = 259;
		AF_OPD4_VVALID = 324;

		AF_OPD4_HDELAY = ((x * H_ratio) / 100) - (AF_OPD4_HVALID / 2) + 8 +41;	//AF_OPD_HDELAY = X position + 8(color processing margin)+41(isx012 resttiction)
		AF_OPD4_VDELAY = ((y * V_ratio) / 100) - (AF_OPD4_VVALID / 2)+ 5;		//AF_OPD_VDELAY = Y position +5 (isx012 resttiction)

		// out of boundary... min.
		if(AF_OPD4_HDELAY < 8 + 41) AF_OPD4_HDELAY = 8 +41 ;	//WIDSOWN H size min : X position + 8 is really min.
		if(AF_OPD4_VDELAY < 5) AF_OPD4_VDELAY = 5;	//WIDSOWN V size min : Y position is really min.

		// out of boundary... max.
		if(AF_OPD4_HDELAY > 2608 - AF_OPD4_HVALID - 8 -41) AF_OPD4_HDELAY = 2608 - AF_OPD4_HVALID - 8 -41;
		if(AF_OPD4_VDELAY > 1944 - AF_OPD4_VVALID -5) AF_OPD4_VDELAY = 1944 - AF_OPD4_VVALID -5;


		// AF window setting
		isx012_i2c_write_multi(0x6A50, AF_OPD4_HDELAY, 2);
		isx012_i2c_write_multi(0x6A52, AF_OPD4_VDELAY, 2);
		isx012_i2c_write_multi(0x6A54, AF_OPD4_HVALID, 2);
		isx012_i2c_write_multi(0x6A56, AF_OPD4_VVALID, 2);

		isx012_i2c_write_multi(0x6A80, 0x0000, 1);
		isx012_i2c_write_multi(0x6A81, 0x0000, 1);
		isx012_i2c_write_multi(0x6A82, 0x0000, 1);
		isx012_i2c_write_multi(0x6A83, 0x0000, 1);
		isx012_i2c_write_multi(0x6A84, 0x0008, 1);
		isx012_i2c_write_multi(0x6A85, 0x0000, 1);
		isx012_i2c_write_multi(0x6A86, 0x0000, 1);
		isx012_i2c_write_multi(0x6A87, 0x0000, 1);
		isx012_i2c_write_multi(0x6A88, 0x0000, 1);
		isx012_i2c_write_multi(0x6A89, 0x0000, 1);
		isx012_i2c_write_multi(0x6646, 0x0000, 1);
	}

	return 0;
}
#endif

int isx012_sensor_ext_config(void __user *argp)
{
	sensor_ext_cfg_data cfg_data;
	int err = 0;

	//printk("isx012_sensor_ext_config ");

	if (copy_from_user((void *)&cfg_data, (const void *)argp, sizeof(cfg_data))){
		cam_err("fail copy_from_user!");
	}

#if 0
	if(cfg_data.cmd != EXT_CFG_GET_AF_STATUS) {
		printk("[%s][line:%d] cfg_data.cmd(%d) cfg_data.value_1(%d),cfg_data.value_2(%d)\n", __func__, __LINE__, cfg_data.cmd, cfg_data.value_1, cfg_data.value_2);
	}
#endif

	if(DtpTest == 1) {
		if ((cfg_data.cmd != EXT_CFG_SET_VT_MODE)
		&& (cfg_data.cmd != EXT_CFG_SET_MOVIE_MODE)
		&& (cfg_data.cmd != EXT_CFG_SET_DTP)
		&& (!isx012_ctrl->status.initialized)) {
			printk("[%s][line:%d] camera isn't initialized(status.initialized:%d)\n", __func__, __LINE__, isx012_ctrl->status.initialized);
			printk("[%s][line:%d] cfg_data.cmd(%d) cfg_data.value_1(%d),cfg_data.value_2(%d)\n", __func__, __LINE__, cfg_data.cmd, cfg_data.value_1, cfg_data.value_2);
			return 0;
		}
	}

	switch (cfg_data.cmd) {
		case EXT_CFG_SET_FLASH:
			err = isx012_set_flash(cfg_data.value_1, cfg_data.value_2);
			break;

		case EXT_CFG_SET_BRIGHTNESS:
			err = isx012_set_brightness(cfg_data.value_1);
			break;

		case EXT_CFG_SET_EFFECT:
			err = isx012_set_effect(cfg_data.value_1);
			break;

		case EXT_CFG_SET_ISO:
			err = isx012_set_iso(cfg_data.value_1);
			break;

		case EXT_CFG_SET_WB:
			err = isx012_set_whitebalance(cfg_data.value_1);
			break;

		case EXT_CFG_SET_SCENE:
			err = isx012_set_scene(cfg_data.value_1);
			break;

		case EXT_CFG_SET_METERING:	// auto exposure mode
			err = isx012_set_metering(cfg_data.value_1);
			break;

		case EXT_CFG_SET_CONTRAST:
			err = isx012_set_contrast(cfg_data.value_1);
			break;

		case EXT_CFG_SET_SHARPNESS:
			err = isx012_set_sharpness(cfg_data.value_1);
			break;

		case EXT_CFG_SET_SATURATION:
			err = isx012_set_saturation(cfg_data.value_1);
			break;

		case EXT_CFG_SET_PREVIEW_SIZE:
			err = isx012_set_preview_size(cfg_data.value_1,cfg_data.value_2);
			break;

		case EXT_CFG_SET_PICTURE_SIZE:
			err = isx012_set_picture_size(cfg_data.value_1);
			break;

		case EXT_CFG_SET_JPEG_QUALITY:
			//err = isx012_set_jpeg_quality(cfg_data.value_1);
			break;

		case EXT_CFG_SET_FPS:
			//err = isx012_set_frame_rate(cfg_data.value_1,cfg_data.value_2);
			break;

		case EXT_CFG_SET_DTP:
			cam_info("DTP mode : %d", cfg_data.value_1);
			err = isx012_check_dataline(cfg_data.value_1);
			break;

		case EXT_CFG_SET_VT_MODE:
			cam_info("VTCall mode : %d", cfg_data.value_1);
			break;

		case EXT_CFG_SET_MOVIE_MODE:
			cam_info("MOVIE mode : %d", cfg_data.value_1);
			isx012_set_movie_mode(cfg_data.value_1);
			break;

		case EXT_CFG_SET_AF_OPERATION:
			cam_info("AF mode : %d", cfg_data.value_1);
			break;

		case EXT_CFG_SET_AF_START:
			//cam_info("AF START mode : (1)%d (2)%d isx012_ctrl->status.cancel_af(%d)",cfg_data.value_1, cfg_data.value_2, isx012_ctrl->status.cancel_af);
			cfg_data.value_2 = isx012_set_af_start();
			break;

		case EXT_CFG_GET_AF_STATUS:
			cfg_data.value_2 = isx012_get_af_status();
			break;

		case EXT_CFG_SET_AF_STOP :
			cam_info("AF STOP mode : %d", cfg_data.value_1);
			isx012_set_af_stop(cfg_data.value_1);
			break;

		case EXT_CFG_SET_AF_MODE :
			cam_info("Focus mode : %d", cfg_data.value_1);
			err = isx012_set_focus_mode(cfg_data.value_1, 1);
			break;

		case EXT_CFG_SET_TOUCHAF_MODE: /* set touch AF mode on/off */
			err = isx012_set_touch_auto_focus(cfg_data.value_1);
			break;

		case EXT_CFG_SET_TOUCHAF_POS:	/* set touch AF position */
			isx012_ctrl->status.touchaf = 1;
			isx012_ctrl->status.pos_x = cfg_data.value_1;
			isx012_ctrl->status.pos_y = cfg_data.value_2;
			break;

		case EXT_CFG_SET_LOW_LEVEL :
			isx012_ctrl->status.cancel_af = 0;
			isx012_ctrl->status.cancel_af_running = 0;
		   if (isx012_ctrl->setting.flash_mode > 0) { 
		      err = isx012_get_LowLightCondition();
			} else if (isx012_ctrl->setting.scene == SCENE_MODE_NIGHTSHOT){
				err = isx012_get_LowLightCondition();
			} else {
				cam_info("Not Low light check");
			}  
			break;

		case EXT_CFG_GET_EXIF :
			isx012_get_exif(&cfg_data.value_1, &cfg_data.value_2);
			break;

		case EXT_CFG_SET_APPS :
			isx012_ctrl->status.apps = cfg_data.value_1;
			cam_info("[APPS] isx012_ctrl->status.apps(%d)", isx012_ctrl->status.apps);
			break;

		default:
			break;
	}

	if (copy_to_user((void *)argp, (const void *)&cfg_data, sizeof(cfg_data))){
		cam_err("fail copy_from_user!");
	}

	return err;
}

int isx012_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	int err = 0;

	if (copy_from_user(&cfg_data,(void *)argp, sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	CAM_DEBUG("cfgtype = %d, mode = %d", cfg_data.cfgtype, cfg_data.mode);

	switch (cfg_data.cfgtype) {
		case CFG_SET_MODE:
			err = isx012_set_sensor_mode(cfg_data.mode);
			break;

		default:
			err = 0;//-EFAULT;
			break;
	}

	return err;
}

int isx012_sensor_release(void)
{
	int err = 0;

	CAM_DEBUG("POWER OFF START");

	/*Soft landing*/
	ISX012_WRITE_LIST(ISX012_Sensor_Off_VCM);

	gpio_tlmm_config(GPIO_CFG(CAM_5M_EN, OFF, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE); //stanby

	isx012_set_flash(0, 50);
	isx012_ctrl->setting.flash_mode = 0;

	gpio_set_value_cansleep(CAM_5M_EN, OFF);
	mdelay(100);

	gpio_set_value_cansleep(CAM_5M_RST, OFF);
	udelay(50);

	cam_ldo_power_off();	//have to turn off MCLK before PMIC

	isx012_ctrl->status.initialized = 0;
	isx012_ctrl->status.cancel_af = 0;
	isx012_ctrl->status.pos_x = 0;
	isx012_ctrl->status.pos_y = 0;
	isx012_ctrl->status.touchaf = 0;
	isx012_ctrl->status.apps = 0;
	isx012_ctrl->status.start_af = 0;
	iscapture = 0;

	DtpTest = 0;
	kfree(isx012_ctrl);
	kfree(isx012_exif);
        isx012_exif = NULL;

#ifdef CONFIG_LOAD_FILE
	isx012_regs_table_exit();
#endif
	CAM_DEBUG("POWER OFF END");

	return err;
}


 static int isx012_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;

	CAM_DEBUG("E");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENOTSUPP;
		goto probe_failure;
	}

	isx012_sensorw = kzalloc(sizeof(struct isx012_work_t), GFP_KERNEL);

	if (!isx012_sensorw) {
		err = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, isx012_sensorw);
	isx012_init_client(client);
	isx012_client = client;

	gpio_tlmm_config(GPIO_CFG(CAM_FLASH_EN, OFF, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);// flash EN
	gpio_tlmm_config(GPIO_CFG(CAM_FLASH_SET, OFF, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);// flash set

	gpio_set_value_cansleep(CAM_FLASH_EN, OFF);
	gpio_set_value_cansleep(CAM_FLASH_SET, OFF);
#ifdef FACTORY_TEST
	camera_class = class_create(THIS_MODULE, "camera");

        if (IS_ERR(camera_class))
                pr_err("Failed to create class(camera)!\n");

	isx012_dev = device_create(camera_class, NULL, 0, NULL, "rear");
	if (IS_ERR(isx012_dev)) {
		pr_err("Failed to create device!");
		goto probe_failure;
	}

       if (device_create_file(isx012_dev, &dev_attr_rear_camtype) < 0) {
                CDBG("failed to create device file, %s\n",
                        dev_attr_rear_camtype.attr.name);
        }
       if (device_create_file(isx012_dev, &dev_attr_rear_flash) < 0) {
                CDBG("failed to create device file, %s\n",
                        dev_attr_rear_flash.attr.name);
        }
#endif

	CAM_DEBUG("X");

	return err;

probe_failure:
	kfree(isx012_sensorw);
	isx012_sensorw = NULL;
	cam_err("isx012_i2c_probe failed!");
	return err;
}


static int __exit isx012_i2c_remove(struct i2c_client *client)
{
	struct isx012_work_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);

#ifdef FACTORY_TEST
	device_remove_file(isx012_dev, &dev_attr_rear_camtype);
	device_remove_file(isx012_dev, &dev_attr_rear_flash);
#endif

//	i2c_detach_client(client);
	isx012_client = NULL;
	isx012_sensorw = NULL;
	kfree(sensorw);
	return 0;

}


static const struct i2c_device_id isx012_id[] = {
    { "isx012_i2c", 0 },
    { }
};

//PGH MODULE_DEVICE_TABLE(i2c, isx012);

static struct i2c_driver isx012_i2c_driver = {
	.id_table	= isx012_id,
	.probe  	= isx012_i2c_probe,
	.remove 	= __exit_p(isx012_i2c_remove),
	.driver 	= {
		.name = "isx012",
	},
};


int32_t isx012_i2c_init(void)
{
	int32_t err = 0;

	CAM_DEBUG("E");

	err = i2c_add_driver(&isx012_i2c_driver);

	if (IS_ERR_VALUE(err))
		goto init_failure;

	return err;



init_failure:
	cam_err("failed to isx012_i2c_init, err = %d", err);
	return err;
}


void isx012_exit(void)
{
	i2c_del_driver(&isx012_i2c_driver);
}


int isx012_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{
	int err = 0;

	printk("############# isx012_sensor_probe ##############\n");
/*	struct msm_camera_sensor_info *info =
		(struct msm_camera_sensor_info *)dev;

	struct msm_sensor_ctrl *s =
		(struct msm_sensor_ctrl *)ctrl;
*/


	err = isx012_i2c_init();
	if (err < 0)
		goto probe_done;

 	s->s_init	= isx012_sensor_open_init;
	s->s_release	= isx012_sensor_release;
	s->s_config	= isx012_sensor_config;
	s->s_ext_config	= isx012_sensor_ext_config;
	s->s_camera_type = BACK_CAMERA_2D;
	s->s_mount_angle = 90;

probe_done:
	cam_err("Sensor probe done(%d)", err);
	return err;

}


static int __sec_isx012_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, isx012_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __sec_isx012_probe,
	.driver = {
		.name = "msm_camera_isx012",
		.owner = THIS_MODULE,
	},
};

static int __init sec_isx012_camera_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

static void __exit sec_isx012_camera_exit(void)
{
	platform_driver_unregister(&msm_camera_driver);
}

module_init(sec_isx012_camera_init);
module_exit(sec_isx012_camera_exit);

