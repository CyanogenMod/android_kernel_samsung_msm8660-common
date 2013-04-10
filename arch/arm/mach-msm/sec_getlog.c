/*
 *  sec_getlog.c
 *
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/memory.h>

static struct {
	u32 special_mark_1;
	u32 special_mark_2;
	u32 special_mark_3;
	u32 special_mark_4;
	void *p_fb;		/* it must be physical address */
	u32 xres;
	u32 yres;
	u32 bpp;		/* color depth : 16 or 24 */
	u32 frames;		/* frame buffer count : 2 */
} frame_buf_mark = {
	.special_mark_1 = (('*' << 24) | ('^' << 16) | ('^' << 8) | ('*' << 0)),
	.special_mark_2 = (('I' << 24) | ('n' << 16) | ('f' << 8) | ('o' << 0)),
	.special_mark_3 = (('H' << 24) | ('e' << 16) | ('r' << 8) | ('e' << 0)),
	.special_mark_4 = (('f' << 24) | ('b' << 16) | ('u' << 8) | ('f' << 0)),
};

void sec_getlog_supply_fbinfo(void *p_fb, u32 xres, u32 yres, u32 bpp,
			      u32 frames)
{
#ifdef CONFIG_FB_MSM_MIPI_S6E8AA0_HD720_PANEL 
// Adreno GPU requires 32 pixel aligned width  for texture so that it makes 720 to 736 which is multiple of 32, though padding is not exposed on LCD.
	volatile int Xres;

	// make xres to multiple of 32
	Xres = xres +(32-1);
	Xres -= (Xres % 32);

	pr_info("%s: xres to be 32's : %d -> %d\n", __func__, xres, Xres );
	xres = Xres;
#endif

	if (p_fb) {
		pr_info("%s: 0x%p %d %d %d %d\n", __func__, p_fb, xres, yres,
			bpp, frames);
		frame_buf_mark.p_fb = phys_to_virt((unsigned long)p_fb);
		frame_buf_mark.xres = xres;
		frame_buf_mark.yres = yres;
		frame_buf_mark.bpp = bpp;
		frame_buf_mark.frames = frames;
	}
}
EXPORT_SYMBOL(sec_getlog_supply_fbinfo);

static struct {
	u32 special_mark_1;
	u32 special_mark_2;
	u32 special_mark_3;
	u32 special_mark_4;
	u32 log_mark_version;
	u32 framebuffer_mark_version;
	void *this;		/* this is used for addressing
				   log buffer in 2 dump files */
	struct {
		u32 size;	/* memory block's size */
		u32 addr;	/* memory block'sPhysical address */
	} mem[2];
} marks_ver_mark = {
	.special_mark_1 = (('*' << 24) | ('^' << 16) | ('^' << 8) | ('*' << 0)),
	.special_mark_2 = (('I' << 24) | ('n' << 16) | ('f' << 8) | ('o' << 0)),
	.special_mark_3 = (('H' << 24) | ('e' << 16) | ('r' << 8) | ('e' << 0)),
	.special_mark_4 = (('v' << 24) | ('e' << 16) | ('r' << 8) | ('s' << 0)),
	.log_mark_version = 1,
	.framebuffer_mark_version = 1,
	.this = &marks_ver_mark,
};

void sec_getlog_supply_meminfo(u32 size0, u32 addr0, u32 size1, u32 addr1)
{
	pr_info("%s: %x %x %x %x\n", __func__, size0, addr0, size1, addr1);
	marks_ver_mark.mem[0].size = size0;
	marks_ver_mark.mem[0].addr = addr0;
	marks_ver_mark.mem[1].size = size1;
	marks_ver_mark.mem[1].addr = addr1;
}
EXPORT_SYMBOL(sec_getlog_supply_meminfo);

/* mark for GetLog extraction */
static struct {
	u32 special_mark_1;
	u32 special_mark_2;
	u32 special_mark_3;
	u32 special_mark_4;
	void *p_main;
	void *p_radio;
	void *p_events;
	void *p_system;
} plat_log_mark = {
	.special_mark_1 = (('*' << 24) | ('^' << 16) | ('^' << 8) | ('*' << 0)),
	.special_mark_2 = (('I' << 24) | ('n' << 16) | ('f' << 8) | ('o' << 0)),
	.special_mark_3 = (('H' << 24) | ('e' << 16) | ('r' << 8) | ('e' << 0)),
	.special_mark_4 = (('p' << 24) | ('l' << 16) | ('o' << 8) | ('g' << 0)),
};

void sec_getlog_supply_loggerinfo(void *p_main,
				  void *p_radio, void *p_events, void *p_system)
{
	pr_info("%s: 0x%p 0x%p 0x%p 0x%p\n", __func__, p_main, p_radio,
		p_events, p_system);
	plat_log_mark.p_main = p_main;
	plat_log_mark.p_radio = p_radio;
	plat_log_mark.p_events = p_events;
	plat_log_mark.p_system = p_system;
}
EXPORT_SYMBOL(sec_getlog_supply_loggerinfo);

static struct {
	u32 special_mark_1;
	u32 special_mark_2;
	u32 special_mark_3;
	u32 special_mark_4;
	void *klog_buf;
} kernel_log_mark = {
	.special_mark_1 = (('*' << 24) | ('^' << 16) | ('^' << 8) | ('*' << 0)),
	.special_mark_2 = (('I' << 24) | ('n' << 16) | ('f' << 8) | ('o' << 0)),
	.special_mark_3 = (('H' << 24) | ('e' << 16) | ('r' << 8) | ('e' << 0)),
	.special_mark_4 = (('k' << 24) | ('l' << 16) | ('o' << 8) | ('g' << 0)),
};

void sec_getlog_supply_kloginfo(void *klog_buf)
{
	pr_info("%s: 0x%p\n", __func__, klog_buf);
	kernel_log_mark.klog_buf = klog_buf;
}
EXPORT_SYMBOL(sec_getlog_supply_kloginfo);
