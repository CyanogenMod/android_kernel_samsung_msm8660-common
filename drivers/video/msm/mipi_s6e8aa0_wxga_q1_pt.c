/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_s6e8aa0_wxga_q1.h"

static struct msm_panel_info pinfo;
#if 1
static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = { 
/* DSI_BIT_CLK at 480MHz, RGB888 */ 
{0x13, 0x01, 0x01, 0x00},	/* regulator */ 
/* timing */ 
{0xB2, 0x8C, 0x1C, 0x00, 0x1A, 0x93, 0x1F, 0x8E,             // THS_EXIT spec
0x1A, 0x03, 0x04}, 
{0x7f, 0x00, 0x00, 0x00},	/* phy ctrl */ 
{0xee, 0x03, 0x86, 0x03},	/* strength */ 
{0x41, 0xB7, 0x31, 0xDA, 0x00, 0x50, 0x48, 0x63,             // clock 
0x31, 0x0F, 0x03,/* 4 lane */ 
0x05, 0x14, 0x03, 0x03, 0x03, 0x54, 0x06, 0x10, 0x04, 0x03 }, 
//{0xAE, 0x8B, 0x1A, 0x00, 0x18, 0x92, 0x1D, 0x8D, 
//0x18, 0x03, 0x04}, 
//{0x7f, 0x00, 0x00, 0x00},	/* phy ctrl */ 
//{0xee, 0x03, 0x86, 0x03},	/* strength */ 
//{0x41, 0x89, 0x01, 0x19, 0x00, 0x50, 0x48, 0x63, 
//0x31, 0x0F, 0x03,/* 4 lane */ 
//0x05, 0x14, 0x03, 0x03, 0x03, 0x54, 0x06, 0x10, 0x04, 0x03 }, 

}; 
#else
static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = { 
/* DSI_BIT_CLK at 480MHz, RGB888 */ 
{0x13, 0x01, 0x01, 0x00},	/* regulator */ 
/* timing */ 
{0xAC, 0x8B, 0x19, 0x00, 0x1B, 0x97, 0x1C, 0x8D, 
0x1B, 0x03, 0x04}, 
{0x7f, 0x00, 0x00, 0x00},	/* phy ctrl */ 
{0xee, 0x03, 0x86, 0x03},	/* strength */ 
{0x41, 0x85, 0x01, 0x1a, 0x00, 0x50, 0x48, 0x63, 
0x31, 0x0F, 0x03,/* 4 lane */ 
0x05, 0x14, 0x03, 0x03, 0x03, 0x54, 0x06, 0x10, 0x04, 0x03 }, 
}; 

#endif
static int __init mipi_video_s6e8aa0_wxga_q1_pt_init(void)
{
	int ret;
    printk("Mipi_Lcd : mipi_video_s6e8aa0_wxga_q1_pt_init \n");
#ifdef CONFIG_FB_MSM_MIPI_PANEL_DETECT
	if (msm_fb_detect_client("mipi_video_s6e8aa0_wxga_q1"))
		return 0;
#endif

	pinfo.xres = 800; 	// quincy
	pinfo.yres = 1280; 
	pinfo.type = MIPI_VIDEO_PANEL; 
	pinfo.pdest = DISPLAY_1; 
	pinfo.wait_cycle = 0; 
	pinfo.bpp = 24; 

#if defined(S6E8AA0_WXGA_Q1_58HZ_500MBPS)
	pinfo.lcdc.h_back_porch = 153;		// quincy
	pinfo.lcdc.h_front_porch = 153;		// quincy 
#else
	pinfo.lcdc.h_back_porch = 134;		// quincy
	pinfo.lcdc.h_front_porch = 134;		// quincy 
#endif
	pinfo.lcdc.h_pulse_width = 2;	   

	pinfo.lcdc.v_back_porch = 1; 
	pinfo.lcdc.v_front_porch = 13; 
	pinfo.lcdc.v_pulse_width = 2; 

	pinfo.lcdc.border_clr = 0; /* blk */ 
	pinfo.lcdc.underflow_clr = 0; // black
	pinfo.lcdc.hsync_skew = 0; 

	pinfo.bl_max = 255; 
	pinfo.bl_min = 1; 
#ifdef CONFIG_FB_MSM_TRIPLE_BUFFER
	pinfo.fb_num = 3; 
#else
	pinfo.fb_num = 2; 
#endif

//	pinfo.clk_rate = 400000000; 

	pinfo.mipi.mode = DSI_VIDEO_MODE; 
	pinfo.mipi.pulse_mode_hsa_he = FALSE; 
	pinfo.mipi.hfp_power_stop = FALSE; 
	pinfo.mipi.hbp_power_stop = FALSE; 
	pinfo.mipi.hsa_power_stop = FALSE; 
	pinfo.mipi.eof_bllp_power_stop = TRUE; 
	pinfo.mipi.bllp_power_stop = TRUE; 

	pinfo.mipi.traffic_mode = DSI_BURST_MODE;
	//pinfo.mipi.traffic_mode = DSI_NON_BURST_SYNCH_EVENT;
	pinfo.mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888; 
	pinfo.mipi.vc = 0; 
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_BGR; 
	pinfo.mipi.data_lane0 = TRUE; 
	pinfo.mipi.data_lane1 = TRUE; 
	pinfo.mipi.data_lane2 = TRUE; 
	pinfo.mipi.data_lane3 = TRUE; 
	pinfo.mipi.t_clk_post = 35;//0x01; 
	pinfo.mipi.t_clk_pre = 62;//0x19; 

	pinfo.mipi.stream = 0; /* dma_p */ 
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_SW; 
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW; 
#if defined(S6E8AA0_WXGA_Q1_57p2HZ_480MBPS)
	pinfo.mipi.frame_rate = 58;		// Quincy
#elif defined(S6E8AA0_WXGA_Q1_58HZ_500MBPS)
	pinfo.mipi.frame_rate = 58;		// Quincy
#elif defined(S6E8AA0_WXGA_Q1_60HZ_500MBPS)
	pinfo.mipi.frame_rate = 60;		// Quincy
#endif 	

	pinfo.mipi.dsi_phy_db = &dsi_video_mode_phy_db; 

	// add info for CTS
	pinfo.lcd.refx100 = pinfo.mipi.frame_rate *100;
	pinfo.lcd.v_back_porch = pinfo.lcdc.v_back_porch;
	pinfo.lcd.v_front_porch = pinfo.lcdc.v_front_porch;
	pinfo.lcd.v_pulse_width = pinfo.lcdc.v_pulse_width;

	ret = mipi_s6e8aa0_wxga_q1_device_register(&pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_WVGA_PT);
	if (ret)
		printk(KERN_ERR "%s: failed to register device!\n", __func__);

	return ret;
}

module_init(mipi_video_s6e8aa0_wxga_q1_pt_init);
