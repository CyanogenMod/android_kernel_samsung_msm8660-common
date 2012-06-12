/* drivers/video/msm/logo.c
 *
 * Show Logo in RLE 565 format
 *
 * Copyright (C) 2008 Google Incorporated
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/vt_kern.h>
#include <linux/unistd.h>
#include <linux/syscalls.h>

#include <linux/irq.h>
#include <asm/system.h>
#include <asm/cacheflush.h>

#define fb_width(fb)	((fb)->var.xres)
#define fb_height(fb)	((fb)->var.yres)
#define fb_size(fb)	((fb)->var.xres * (fb)->var.yres * 2)

#define DISPLAY_BOOT_PROGRESS

#ifdef DISPLAY_BOOT_PROGRESS
static int progress_flag = 0;
static int progress_pos;
static struct timer_list progress_timer;

#ifdef CONFIG_FB_MSM_MIPI_S6E8AA0_HD720_PANEL
#define PROGRESS_BAR_WIDTH	4
#define PROGRESS_BAR_HEIGHT	8
#define PROGRESS_BAR_LEFT_POS	82
#define PROGRESS_BAR_RIGHT_POS	637
#define PROGRESS_BAR_START_Y	922
#elif defined(CONFIG_FB_MSM_MIPI_S6E8AA0_WXGA_Q1_PANEL)
#define PROGRESS_BAR_WIDTH	4
#define PROGRESS_BAR_HEIGHT	8
#define PROGRESS_BAR_LEFT_POS	82
#define PROGRESS_BAR_RIGHT_POS	717
#define PROGRESS_BAR_START_Y	922
#else
#define PROGRESS_BAR_WIDTH	4
#define PROGRESS_BAR_HEIGHT	8
#define PROGRESS_BAR_LEFT_POS	54
#define PROGRESS_BAR_RIGHT_POS	425
#define PROGRESS_BAR_START_Y	576
#endif

static unsigned char anycall_progress_bar_left[] =
{	
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x2E, 0xB1, 0xDB, 0x00, 0x2E, 0xB1, 0xDB, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x2E, 0xB1, 0xDB, 0x00, 0x2E, 0xB1, 0xDB, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x2E, 0xB1, 0xDB, 0x00, 0x2E, 0xB1, 0xDB, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x2E, 0xB1, 0xDB, 0x00, 0x2E, 0xB1, 0xDB, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00
};

static unsigned char anycall_progress_bar_right[] =
{	
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
	0x2E, 0xB1, 0xDB, 0x00, 0x2E, 0xB1, 0xDB, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 
	0x2E, 0xB1, 0xDB, 0x00, 0x2E, 0xB1, 0xDB, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 
	0x2E, 0xB1, 0xDB, 0x00, 0x2E, 0xB1, 0xDB, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 
	0x2E, 0xB1, 0xDB, 0x00, 0x2E, 0xB1, 0xDB, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00
};

static unsigned char anycall_progress_bar_center[] =
{
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x2E, 0xB1, 0xDB, 0x00, 0x2E, 0xB1, 0xDB, 0x00,
	0x2E, 0xB1, 0xDB, 0x00, 0x2E, 0xB1, 0xDB, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00
};

static unsigned char anycall_progress_bar[] =
{
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00,
	0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00, 0x33, 0x33, 0x33, 0x00
};

static void s3cfb_start_progress(struct fb_info *fb);
static void s3cfb_stop_progress(void);
static void progress_timer_handler(unsigned long data);

static int show_progress = 1;
module_param_named(progress, show_progress, bool, 0);
extern unsigned int is_lpcharging_state(void);
#endif

extern unsigned int sec_debug_is_recovery_mode(void);

#if 1
/* convert RGB565 to RBG8888 */
static void memset16_rgb8888(void *_ptr, unsigned short val, unsigned count)
{
	unsigned short *ptr = _ptr;
	unsigned short red; 
	unsigned short green;
	unsigned short blue;
	
	red = ( val & 0xF800) >> 8;
	green = (val & 0x7E0) >> 3;
	blue = (val & 0x1F) << 3;

	count >>= 1;
	while (count--)
	{
		*ptr++ = (red<<8) | green;
		*ptr++ = blue;
	}
}

int load_565rle_image(char *filename)
{
	int fd, err = 0;
	unsigned count, max;
	unsigned short *data, *bits, *ptr;
	struct fb_info *info;
#ifndef CONFIG_FRAMEBUFFER_CONSOLE 
	struct module *owner; 
#endif 	
	info = registered_fb[0];
	
	if (!info) {
		printk(KERN_WARNING "%s: Can not access framebuffer\n",
			__func__);
		return -ENODEV;
	}
#ifndef CONFIG_FRAMEBUFFER_CONSOLE 
	owner = info->fbops->owner; 
	if (!try_module_get(owner)) 
		return NULL; 
	if (info->fbops->fb_open && info->fbops->fb_open(info, 0)) { 
		module_put(owner); 
		return NULL; 
	}
#endif
	fd = sys_open(filename, O_RDONLY, 0);
	if (fd < 0) {
		printk(KERN_WARNING "%s: Can not open %s\n",
			__func__, filename);
		return -ENOENT;
	}
printk("%s: open OK! %s\n",__func__, filename);
	count = (unsigned)sys_lseek(fd, (off_t)0, 2);
	if (count == 0) {
		sys_close(fd);
		err = -EIO;
		goto err_logo_close_file;
	}
printk("%s: count %d\n",__func__, count);    
	sys_lseek(fd, (off_t)0, 0);
	data = kmalloc(count, GFP_KERNEL);
	if (!data) {
		printk(KERN_WARNING "%s: Can not alloc data\n", __func__);
		err = -ENOMEM;
		goto err_logo_close_file;
	}
	if ((unsigned)sys_read(fd, (char *)data, count) != count) {
		err = -EIO;
		goto err_logo_free_data;
	}

	max = fb_width(info) * fb_height(info);

	ptr = data;
	bits = (unsigned short *)(info->screen_base);
printk("%s: max %d, n %d 0x%x\n",__func__, max, ptr[0], (unsigned int)bits);
	while (count > 3) {
		unsigned n = ptr[0];
		if (n > max)
			break;

		memset16_rgb8888(bits, ptr[1], n << 1);
		bits += n*2; // for rgb8888
		max -= n;
		ptr += 2;
		count -= 4;
	}
#if !defined (CONFIG_USA_OPERATOR_ATT) && !defined (CONFIG_JPN_MODEL_SC_03D) && !defined (CONFIG_CAN_OPERATOR_RWC)
	if (!is_lpcharging_state() && !sec_debug_is_recovery_mode())
		s3cfb_start_progress(info);
#endif

err_logo_free_data:
	kfree(data);
err_logo_close_file:
	sys_close(fd);
	return err;
}
#else

static void memset16(void *_ptr, unsigned short val, unsigned count)
{
	unsigned short *ptr = _ptr;
	count >>= 1;
	while (count--)
		*ptr++ = val;
}

/* 565RLE image format: [count(2 bytes), rle(2 bytes)] */
int load_565rle_image(char *filename)
{
	struct fb_info *info;
	int fd, count, err = 0;
	unsigned max;
	unsigned short *data, *bits, *ptr;

	info = registered_fb[0];
	if (!info) {
		printk(KERN_WARNING "%s: Can not access framebuffer\n",
			__func__);
		return -ENODEV;
	}

	fd = sys_open(filename, O_RDONLY, 0);
	if (fd < 0) {
		printk(KERN_WARNING "%s: Can not open %s\n",
			__func__, filename);
		return -ENOENT;
	}
	count = sys_lseek(fd, (off_t)0, 2);
	if (count <= 0) {
		err = -EIO;
		goto err_logo_close_file;
	}
	sys_lseek(fd, (off_t)0, 0);
	data = kmalloc(count, GFP_KERNEL);
	if (!data) {
		printk(KERN_WARNING "%s: Can not alloc data\n", __func__);
		err = -ENOMEM;
		goto err_logo_close_file;
	}
	if (sys_read(fd, (char *)data, count) != count) {
		err = -EIO;
		goto err_logo_free_data;
	}

	max = fb_width(info) * fb_height(info);
	ptr = data;
	bits = (unsigned short *)(info->screen_base);
	while (count > 3) {
		unsigned n = ptr[0];
		if (n > max)
			break;
		memset16(bits, ptr[1], n << 1);
		bits += n;
		max -= n;
		ptr += 2;
		count -= 4;
	}

err_logo_free_data:
	kfree(data);
err_logo_close_file:
	sys_close(fd);
	return err;
}
#endif

#ifdef DISPLAY_BOOT_PROGRESS
static void s3cfb_update_framebuffer(struct fb_info *fb,
									int x, int y, void *buffer, 
									int src_width, int src_height)
{
//	struct s3cfb_global *fbdev =
//			platform_get_drvdata(to_platform_device(fb->device));
//	struct s3c_platform_fb *pdata = to_fb_plat(fbdev->dev);
	struct fb_fix_screeninfo *fix = &fb->fix;
	struct fb_var_screeninfo *var = &fb->var;
	int row;
	int bytes_per_pixel = (var->bits_per_pixel / 8 );
	
	unsigned char *pSrc = buffer;
	unsigned char *pDst = fb->screen_base;

	if (x+src_width > var->xres || y+src_height > var->yres)
	{
		printk("invalid destination coordinate or source size (%d, %d) (%d %d) \n", x, y, src_width, src_height);
		return;
	}	

	pDst += y * fix->line_length + x * bytes_per_pixel;	

	for (row = 0; row < src_height ; row++)	
	{		
		memcpy(pDst, pSrc, src_width * bytes_per_pixel);
		flush_cache_all();
		pSrc += src_width * bytes_per_pixel;
		pDst += fix->line_length;
	}
 }


/*
if Updated-pixel is overwrited by other color, progressbar-Update Stop.
return value : TRUE(update), FALSE(STOP)
*/
static int s3cfb_check_progress(struct fb_info *fb, const int progress_pos, int width)
{
	unsigned char *pDst = fb->screen_base;
	struct fb_fix_screeninfo *fix = &fb->fix;
	struct fb_var_screeninfo *var = &fb->var;
	int bytes_per_pixel = (var->bits_per_pixel / 8);
	int x = PROGRESS_BAR_LEFT_POS;
	int y = PROGRESS_BAR_START_Y;

	if (progress_pos + width >= PROGRESS_BAR_RIGHT_POS)
		return 0;

	pDst += y * fix->line_length + x * bytes_per_pixel;
	if (*pDst == anycall_progress_bar_left[0])
		return 1;
	else
		return 0;
}

static void s3cfb_start_progress(struct fb_info *fb)
{	
	int x_pos;
	init_timer(&progress_timer);	
	
	progress_timer.expires  = (get_jiffies_64() + (HZ/10));	
	progress_timer.data     = (long)fb;	
	progress_timer.function = progress_timer_handler;	
	progress_pos = PROGRESS_BAR_LEFT_POS;	
	
	// draw progress background.
	for (x_pos = PROGRESS_BAR_LEFT_POS ; x_pos <= PROGRESS_BAR_RIGHT_POS ; x_pos += PROGRESS_BAR_WIDTH)
	{
		s3cfb_update_framebuffer(fb,
			x_pos,
			PROGRESS_BAR_START_Y,
			(void*)anycall_progress_bar,					
			PROGRESS_BAR_WIDTH,
			PROGRESS_BAR_HEIGHT);
	}

	s3cfb_update_framebuffer(fb,
		PROGRESS_BAR_LEFT_POS,
		PROGRESS_BAR_START_Y,
		(void*)anycall_progress_bar_left,					
		PROGRESS_BAR_WIDTH,
		PROGRESS_BAR_HEIGHT);
	
	progress_pos += PROGRESS_BAR_WIDTH;	
	
	s3cfb_update_framebuffer(fb,		
		progress_pos,
		PROGRESS_BAR_START_Y,		
		(void*)anycall_progress_bar_right,				
		PROGRESS_BAR_WIDTH,
		PROGRESS_BAR_HEIGHT);
	
	add_timer(&progress_timer);	
	progress_flag = 1;

}

static void s3cfb_stop_progress(void)
{	
	if (progress_flag == 0)		
		return;	
	del_timer(&progress_timer);	
	progress_flag = 0;
}

#ifdef CONFIG_FB_MSM_MIPI_S6E8AA0_HD720_PANEL
#define CENTERBAR_WIDTH 12
#elif defined(CONFIG_FB_MSM_MIPI_S6E8AA0_WXGA_Q1_PANEL)
#define CENTERBAR_WIDTH 12
#else
#define CENTERBAR_WIDTH 10
#endif
static void progress_timer_handler(unsigned long data)
{	
	int i;	

	if (s3cfb_check_progress((struct fb_info *)data, progress_pos, CENTERBAR_WIDTH)) {	
		for(i = 0; i < CENTERBAR_WIDTH; i++)	
		{		
			s3cfb_update_framebuffer((struct fb_info *)data,
				progress_pos++,
				PROGRESS_BAR_START_Y,
				(void*)anycall_progress_bar_center,					
				1,
				PROGRESS_BAR_HEIGHT);	
		}	
	
		s3cfb_update_framebuffer((struct fb_info *)data,		
			progress_pos,
			PROGRESS_BAR_START_Y,
			(void*)anycall_progress_bar_right,		
			PROGRESS_BAR_WIDTH,
			PROGRESS_BAR_HEIGHT);
		
			progress_timer.expires = (get_jiffies_64() + (HZ/14));         
			progress_timer.function = progress_timer_handler;         
			add_timer(&progress_timer);    
	} else
		s3cfb_stop_progress();

}

EXPORT_SYMBOL(s3cfb_start_progress);
#endif
EXPORT_SYMBOL(load_565rle_image);
