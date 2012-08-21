/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <linux/io.h>

#include <asm/system.h>
#include <asm/mach-types.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>

#include <linux/fb.h>

#include "mdp.h"
#include "msm_fb.h"
#include "mdp4.h"

static struct mdp4_overlay_pipe *mddi_pipe;
static struct msm_fb_data_type *mddi_mfd;
static int busy_wait_cnt;

static int vsync_start_y_adjust = 4;

static int dmap_vsync_enable;

void mdp_dmap_vsync_set(int enable)
{
	dmap_vsync_enable = enable;
}

int mdp_dmap_vsync_get(void)
{
	return dmap_vsync_enable;
}

void mdp4_mddi_vsync_enable(struct msm_fb_data_type *mfd,
		struct mdp4_overlay_pipe *pipe, int which)
{
	uint32 start_y, data, tear_en;

	tear_en = (1 << which);

	if ((mfd->use_mdp_vsync) && (mfd->ibuf.vsync_enable) &&
		(mfd->panel_info.lcd.vsync_enable)) {

		if (mdp_hw_revision < MDP4_REVISION_V2_1) {
			/* need dmas dmap switch */
			if (which == 0 && dmap_vsync_enable == 0 &&
				mfd->panel_info.lcd.rev < 2) /* dma_p */
				return;
		}

		if (vsync_start_y_adjust <= pipe->dst_y)
			start_y = pipe->dst_y - vsync_start_y_adjust;
		else
			start_y = (mfd->total_lcd_lines - 1) -
				(vsync_start_y_adjust - pipe->dst_y);
		if (which == 0)
			MDP_OUTP(MDP_BASE + 0x210, start_y);	/* primary */
		else
			MDP_OUTP(MDP_BASE + 0x214, start_y);	/* secondary */

		data = inpdw(MDP_BASE + 0x20c);
		data |= tear_en;
		MDP_OUTP(MDP_BASE + 0x20c, data);
	} else {
		data = inpdw(MDP_BASE + 0x20c);
		data &= ~tear_en;
		MDP_OUTP(MDP_BASE + 0x20c, data);
	}
}

#define WHOLESCREEN

void mdp4_overlay_update_lcd(struct msm_fb_data_type *mfd)
{
	MDPIBUF *iBuf = &mfd->ibuf;
	uint8 *src;
	int ptype;
	uint32 mddi_ld_param;
	uint16 mddi_vdo_packet_reg;
	struct mdp4_overlay_pipe *pipe;
	int ret;

	if (mfd->key != MFD_KEY)
		return;

	mddi_mfd = mfd;		/* keep it */

	/* MDP cmd block enable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	if (mddi_pipe == NULL) {
		ptype = mdp4_overlay_format2type(mfd->fb_imgType);
		if (ptype < 0)
			printk(KERN_INFO "%s: format2type failed\n", __func__);
		pipe = mdp4_overlay_pipe_alloc(ptype, MDP4_MIXER0);
		if (pipe == NULL)
			printk(KERN_INFO "%s: pipe_alloc failed\n", __func__);
		pipe->pipe_used++;
		pipe->mixer_num  = MDP4_MIXER0;
		pipe->src_format = mfd->fb_imgType;
		mdp4_overlay_panel_mode(pipe->mixer_num, MDP4_PANEL_MDDI);
		ret = mdp4_overlay_format2pipe(pipe);
		if (ret < 0)
			printk(KERN_INFO "%s: format2type failed\n", __func__);

		mddi_pipe = pipe; /* keep it */
		mddi_pipe->blt_end = 1;	/* mark as end */
		mddi_ld_param = 0;
		mddi_vdo_packet_reg = mfd->panel_info.mddi.vdopkt;

		if (mdp_hw_revision == MDP4_REVISION_V2_1) {
			uint32	data;

			data = inpdw(MDP_BASE + 0x0028);
			data &= ~0x0300;	/* bit 8, 9, MASTER4 */
			if (mfd->fbi->var.xres == 540) /* qHD, 540x960 */
				data |= 0x0200;
			else
				data |= 0x0100;

			MDP_OUTP(MDP_BASE + 0x00028, data);
		}

		if (mfd->panel_info.type == MDDI_PANEL) {
			if (mfd->panel_info.pdest == DISPLAY_1)
				mddi_ld_param = 0;
			else
				mddi_ld_param = 1;
		} else {
			mddi_ld_param = 2;
		}

		MDP_OUTP(MDP_BASE + 0x00090, mddi_ld_param);

		if (mfd->panel_info.bpp == 24)
			MDP_OUTP(MDP_BASE + 0x00094,
			 (MDDI_VDO_PACKET_DESC_24 << 16) | mddi_vdo_packet_reg);
		else if (mfd->panel_info.bpp == 16)
			MDP_OUTP(MDP_BASE + 0x00094,
			 (MDDI_VDO_PACKET_DESC_16 << 16) | mddi_vdo_packet_reg);
		else
			MDP_OUTP(MDP_BASE + 0x00094,
			 (MDDI_VDO_PACKET_DESC << 16) | mddi_vdo_packet_reg);

		MDP_OUTP(MDP_BASE + 0x00098, 0x01);
	} else {
		pipe = mddi_pipe;
	}

	/* 0 for dma_p, client_id = 0 */
	MDP_OUTP(MDP_BASE + 0x00090, 0);


	src = (uint8 *) iBuf->buf;

#ifdef WHOLESCREEN

	{
		struct fb_info *fbi;

		fbi = mfd->fbi;
		pipe->src_height = fbi->var.yres;
		pipe->src_width = fbi->var.xres;
		pipe->src_h = fbi->var.yres;
		pipe->src_w = fbi->var.xres;
		pipe->src_y = 0;
		pipe->src_x = 0;
		pipe->dst_h = fbi->var.yres;
		pipe->dst_w = fbi->var.xres;
		pipe->dst_y = 0;
		pipe->dst_x = 0;
		pipe->srcp0_addr = (uint32)src;
		pipe->srcp0_ystride = fbi->fix.line_length;
	}

#else
	if (mdp4_overlay_active(MDP4_MIXER0)) {
		struct fb_info *fbi;

		fbi = mfd->fbi;
		pipe->src_height = fbi->var.yres;
		pipe->src_width = fbi->var.xres;
		pipe->src_h = fbi->var.yres;
		pipe->src_w = fbi->var.xres;
		pipe->src_y = 0;
		pipe->src_x = 0;
		pipe->dst_h = fbi->var.yres;
		pipe->dst_w = fbi->var.xres;
		pipe->dst_y = 0;
		pipe->dst_x = 0;
		pipe->srcp0_addr = (uint32) src;
		pipe->srcp0_ystride = fbi->fix.line_length;
	} else {
		/* starting input address */
		src += (iBuf->dma_x + iBuf->dma_y * iBuf->ibuf_width)
					* iBuf->bpp;

		pipe->src_height = iBuf->dma_h;
		pipe->src_width = iBuf->dma_w;
		pipe->src_h = iBuf->dma_h;
		pipe->src_w = iBuf->dma_w;
		pipe->src_y = 0;
		pipe->src_x = 0;
		pipe->dst_h = iBuf->dma_h;
		pipe->dst_w = iBuf->dma_w;
		pipe->dst_y = iBuf->dma_y;
		pipe->dst_x = iBuf->dma_x;
		pipe->srcp0_addr = (uint32) src;
		pipe->srcp0_ystride = iBuf->ibuf_width * iBuf->bpp;
	}
#endif

	pipe->mixer_stage  = MDP4_MIXER_STAGE_BASE;

	mdp4_overlay_rgb_setup(pipe);

	mdp4_mixer_stage_up(pipe);

	mdp4_overlayproc_cfg(pipe);

	mdp4_overlay_dmap_xy(pipe);

	mdp4_overlay_dmap_cfg(mfd, 0);

	mdp4_mddi_vsync_enable(mfd, pipe, 0);

	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

}

int mdp4_mddi_overlay_blt_offset(int *off)
{
	if (mdp_hw_revision < MDP4_REVISION_V2_1) { /* need dmas dmap switch */
		if (mddi_pipe->blt_end ||
			(mdp4_overlay_mixer_play(mddi_pipe->mixer_num) == 0)) {
			*off = -1;
			return -EINVAL;
		}
	} else {
		/* no dmas dmap switch */
		if (mddi_pipe->blt_end) {
			*off = -1;
			return -EINVAL;
		}
	}

	if (mddi_pipe->blt_cnt & 0x01)
		*off = mddi_pipe->src_height * mddi_pipe->src_width * 3;
	else
		*off = 0;

	return 0;
}

void mdp4_mddi_overlay_blt(ulong addr)
{
	unsigned long flag;

	spin_lock_irqsave(&mdp_spin_lock, flag);
	if (addr) {
		mdp_pipe_ctrl(MDP_DMA2_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
		mdp_intr_mask |= INTR_DMA_P_DONE;
		outp32(MDP_INTR_ENABLE, mdp_intr_mask);
		mdp_pipe_ctrl(MDP_DMA2_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
		mddi_pipe->blt_cnt = 0;
		mddi_pipe->blt_end = 0;
		mddi_pipe->blt_addr = addr;
	} else {
		mddi_pipe->blt_end = 1;	/* mark as end */
	}
	spin_unlock_irqrestore(&mdp_spin_lock, flag);
}

void mdp4_blt_xy_update(struct mdp4_overlay_pipe *pipe)
{
	uint32 off, addr;
	int bpp;
	char *overlay_base;


	if (pipe->blt_addr == 0)
		return;


#ifdef BLT_RGB565
	bpp = 2; /* overlay ouput is RGB565 */
#else
	bpp = 3; /* overlay ouput is RGB888 */
#endif
	off = 0;
	if (pipe->dmap_cnt & 0x01)
		off = pipe->src_height * pipe->src_width * bpp;

	addr = pipe->blt_addr + off;

	/* dmap */
	MDP_OUTP(MDP_BASE + 0x90008, addr);

	/* overlay 0 */
	overlay_base = MDP_BASE + MDP4_OVERLAYPROC0_BASE;/* 0x10000 */
	outpdw(overlay_base + 0x000c, addr);
	outpdw(overlay_base + 0x001c, addr);
}

/*
 * mdp4_dmap_done_mddi: called from isr
 */
void mdp4_dma_p_done_mddi(void)
{
	if (mddi_pipe->blt_end) {
		mddi_pipe->blt_addr = 0;
		mdp_intr_mask &= ~INTR_DMA_P_DONE;
		outp32(MDP_INTR_ENABLE, mdp_intr_mask);
		mdp4_overlayproc_cfg(mddi_pipe);
		mdp4_overlay_dmap_xy(mddi_pipe);
	}

	/*
	 * single buffer, no need to increase
	 * mdd_pipe->dmap_cnt here
	 */
}

/*
 * mdp4_overlay0_done_mddi: called from isr
 */
void mdp4_overlay0_done_mddi(struct mdp_dma_data *dma)
{
	mdp_disable_irq_nosync(MDP_OVERLAY0_TERM);

	dma->busy = FALSE;
	complete(&dma->comp);
	mdp_pipe_ctrl(MDP_OVERLAY0_BLOCK,
			MDP_BLOCK_POWER_OFF, TRUE);

	if (busy_wait_cnt)
		busy_wait_cnt--;

	pr_debug("%s: ISR-done\n", __func__);

	if (mddi_pipe->blt_addr) {
		if (mddi_pipe->blt_cnt == 0) {
			mdp4_overlayproc_cfg(mddi_pipe);
			mdp4_overlay_dmap_xy(mddi_pipe);
			mddi_pipe->ov_cnt = 0;
			mddi_pipe->dmap_cnt = 0;
			/* BLT start from next frame */
		} else {
			mdp_pipe_ctrl(MDP_DMA2_BLOCK, MDP_BLOCK_POWER_ON,
						FALSE);
			mdp4_blt_xy_update(mddi_pipe);
			outpdw(MDP_BASE + 0x000c, 0x0); /* start DMAP */
		}
		mddi_pipe->blt_cnt++;
		mddi_pipe->ov_cnt++;
	}



}

void mdp4_mddi_overlay_restore(void)
{
	if (mddi_mfd == NULL)
		return;

	pr_debug("%s: resotre, pid=%d\n", __func__, current->pid);

	if (mddi_mfd->panel_power_on == 0)
		return;
	if (mddi_mfd && mddi_pipe) {
		mdp4_mddi_dma_busy_wait(mddi_mfd);
		mdp4_overlay_update_lcd(mddi_mfd);
		mdp4_mddi_overlay_kickoff(mddi_mfd, mddi_pipe);
		mddi_mfd->dma_update_flag = 1;
	}
	if (mdp_hw_revision < MDP4_REVISION_V2_1) /* need dmas dmap switch */
		mdp4_mddi_overlay_dmas_restore();
}

/*
 * mdp4_mddi_cmd_dma_busy_wait: check mddi link activity
 * dsi link is a shared resource and it can only be used
 * while it is in idle state.
 * ov_mutex need to be acquired before call this function.
 */
void mdp4_mddi_dma_busy_wait(struct msm_fb_data_type *mfd)
{
	unsigned long flag;
	int need_wait = 0;

	pr_debug("%s: START, pid=%d\n", __func__, current->pid);
	spin_lock_irqsave(&mdp_spin_lock, flag);
	if (mfd->dma->busy == TRUE) {
		if (busy_wait_cnt == 0)
			INIT_COMPLETION(mfd->dma->comp);
		busy_wait_cnt++;
		need_wait++;
	}
	spin_unlock_irqrestore(&mdp_spin_lock, flag);


	if (need_wait) {
		/* wait until DMA finishes the current job */
		pr_debug("%s: PENDING, pid=%d\n", __func__, current->pid);
		wait_for_completion(&mfd->dma->comp);
	}
	pr_debug("%s: DONE, pid=%d\n", __func__, current->pid);
}

void mdp4_mddi_kickoff_video(struct msm_fb_data_type *mfd,
				struct mdp4_overlay_pipe *pipe)
{
	pr_debug("%s: pid=%d\n", __func__, current->pid);
	mdp4_mddi_overlay_kickoff(mfd, pipe);
}

void mdp4_mddi_kickoff_ui(struct msm_fb_data_type *mfd,
				struct mdp4_overlay_pipe *pipe)
{
	pr_debug("%s: pid=%d\n", __func__, current->pid);
	mdp4_mddi_overlay_kickoff(mfd, pipe);
}


void mdp4_mddi_overlay_kickoff(struct msm_fb_data_type *mfd,
				struct mdp4_overlay_pipe *pipe)
{
	/* change mdp clk while mdp is idle` */
	mdp4_set_perf_level();

	if (mdp_hw_revision == MDP4_REVISION_V2_1) {
		if (mdp4_overlay_status_read(MDP4_OVERLAY_TYPE_UNSET)) {
			uint32  data;
			data = inpdw(MDP_BASE + 0x0028);
			data &= ~0x0300;        /* bit 8, 9, MASTER4 */
			if (mfd->fbi->var.xres == 540) /* qHD, 540x960 */
				data |= 0x0200;
			else
				data |= 0x0100;
			MDP_OUTP(MDP_BASE + 0x00028, data);
			mdp4_overlay_status_write(MDP4_OVERLAY_TYPE_UNSET,
				false);
		}
		if (mdp4_overlay_status_read(MDP4_OVERLAY_TYPE_SET)) {
			uint32  data;
			data = inpdw(MDP_BASE + 0x0028);
			data &= ~0x0300;        /* bit 8, 9, MASTER4 */
			MDP_OUTP(MDP_BASE + 0x00028, data);
			mdp4_overlay_status_write(MDP4_OVERLAY_TYPE_SET, false);
		}
	}
	mdp_enable_irq(MDP_OVERLAY0_TERM);
	mfd->dma->busy = TRUE;
	/* start OVERLAY pipe */
	mdp_pipe_kickoff(MDP_OVERLAY0_TERM, mfd);
	mdp4_stat.kickoff_ov0++;
}

void mdp4_dma_s_update_lcd(struct msm_fb_data_type *mfd,
				struct mdp4_overlay_pipe *pipe)
{
	MDPIBUF *iBuf = &mfd->ibuf;
	uint32 outBpp = iBuf->bpp;
	uint16 mddi_vdo_packet_reg;
	uint32 dma_s_cfg_reg;

	dma_s_cfg_reg = 0;

	if (mfd->fb_imgType == MDP_RGBA_8888)
		dma_s_cfg_reg |= DMA_PACK_PATTERN_BGR; /* on purpose */
	else if (mfd->fb_imgType == MDP_BGR_565)
		dma_s_cfg_reg |= DMA_PACK_PATTERN_BGR;
	else
		dma_s_cfg_reg |= DMA_PACK_PATTERN_RGB;

	if (outBpp == 4)
		dma_s_cfg_reg |= (1 << 26); /* xRGB8888 */
	else if (outBpp == 2)
		dma_s_cfg_reg |= DMA_IBUF_FORMAT_RGB565;

	dma_s_cfg_reg |= DMA_DITHER_EN;

	/* MDP cmd block enable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	/* PIXELSIZE */
	MDP_OUTP(MDP_BASE + 0xa0004, (pipe->dst_h << 16 | pipe->dst_w));
	MDP_OUTP(MDP_BASE + 0xa0008, pipe->srcp0_addr);	/* ibuf address */
	MDP_OUTP(MDP_BASE + 0xa000c, pipe->srcp0_ystride);/* ystride */

	if (mfd->panel_info.bpp == 24) {
		dma_s_cfg_reg |= DMA_DSTC0G_8BITS |	/* 666 18BPP */
		    DMA_DSTC1B_8BITS | DMA_DSTC2R_8BITS;
	} else if (mfd->panel_info.bpp == 18) {
		dma_s_cfg_reg |= DMA_DSTC0G_6BITS |	/* 666 18BPP */
		    DMA_DSTC1B_6BITS | DMA_DSTC2R_6BITS;
	} else {
		dma_s_cfg_reg |= DMA_DSTC0G_6BITS |	/* 565 16BPP */
		    DMA_DSTC1B_5BITS | DMA_DSTC2R_5BITS;
	}

	MDP_OUTP(MDP_BASE + 0xa0010, (pipe->dst_y << 16) | pipe->dst_x);

	/* 1 for dma_s, client_id = 0 */
	MDP_OUTP(MDP_BASE + 0x00090, 1);

	mddi_vdo_packet_reg = mfd->panel_info.mddi.vdopkt;

	if (mfd->panel_info.bpp == 24)
		MDP_OUTP(MDP_BASE + 0x00094,
			(MDDI_VDO_PACKET_DESC_24 << 16) | mddi_vdo_packet_reg);
	else if (mfd->panel_info.bpp == 16)
		MDP_OUTP(MDP_BASE + 0x00094,
			 (MDDI_VDO_PACKET_DESC_16 << 16) | mddi_vdo_packet_reg);
	else
		MDP_OUTP(MDP_BASE + 0x00094,
			 (MDDI_VDO_PACKET_DESC << 16) | mddi_vdo_packet_reg);

	MDP_OUTP(MDP_BASE + 0x00098, 0x01);

	MDP_OUTP(MDP_BASE + 0xa0000, dma_s_cfg_reg);

	mdp4_mddi_vsync_enable(mfd, pipe, 1);

	/* MDP cmd block disable */
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
}

void mdp4_mddi_dma_s_kickoff(struct msm_fb_data_type *mfd,
				struct mdp4_overlay_pipe *pipe)
{
	/* change mdp clk while mdp is idle` */
	mdp4_set_perf_level();

	mdp_enable_irq(MDP_DMA_S_TERM);
	mfd->dma->busy = TRUE;
	mfd->ibuf_flushed = TRUE;
	/* start dma_s pipe */
	mdp_pipe_kickoff(MDP_DMA_S_TERM, mfd);
	mdp4_stat.kickoff_dmas++;

	/* wait until DMA finishes the current job */
	wait_for_completion(&mfd->dma->comp);
	mdp_disable_irq(MDP_DMA_S_TERM);
}

void mdp4_mddi_overlay_dmas_restore(void)
{
	/* mutex held by caller */
	if (mddi_mfd && mddi_pipe) {
		mdp4_mddi_dma_busy_wait(mddi_mfd);
		mdp4_dma_s_update_lcd(mddi_mfd, mddi_pipe);
		mdp4_mddi_dma_s_kickoff(mddi_mfd, mddi_pipe);
		mddi_mfd->dma_update_flag = 1;
	}
}

void mdp4_mddi_overlay(struct msm_fb_data_type *mfd)
{
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mutex_lock(&mfd->dma->ov_mutex);

	if (mfd && mfd->panel_power_on) {
		mdp4_mddi_dma_busy_wait(mfd);
		mdp4_overlay_update_lcd(mfd);

		if (mdp_hw_revision < MDP4_REVISION_V2_1) {
			/* dmas dmap switch */
			if (mdp4_overlay_mixer_play(mddi_pipe->mixer_num)
						== 0) {
				mdp4_dma_s_update_lcd(mfd, mddi_pipe);
				mdp4_mddi_dma_s_kickoff(mfd, mddi_pipe);
			} else
				mdp4_mddi_kickoff_ui(mfd, mddi_pipe);
		} else	/* no dams dmap switch  */
			mdp4_mddi_kickoff_ui(mfd, mddi_pipe);

	/* signal if pan function is waiting for the update completion */
		if (mfd->pan_waiting) {
			mfd->pan_waiting = FALSE;
			complete(&mfd->pan_comp);
		}
	}
	mutex_unlock(&mfd->dma->ov_mutex);
}

int mdp4_mddi_overlay_cursor(struct fb_info *info, struct fb_cursor *cursor)
{
	struct msm_fb_data_type *mfd = info->par;
	
	if (!mfd)
		return -ENODEV;	
	if (mfd->key != MFD_KEY)
		return -EINVAL;
	mutex_lock(&mfd->dma->ov_mutex);
	if (mfd && mfd->panel_power_on) {
		mdp4_mddi_dma_busy_wait(mfd);
		mdp_hw_cursor_update(info, cursor);
	}
	mutex_unlock(&mfd->dma->ov_mutex);
	return 0;
}
