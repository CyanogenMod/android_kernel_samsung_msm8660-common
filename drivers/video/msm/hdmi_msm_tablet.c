/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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

/* #define DEBUG */
#define DEV_DBG_PREFIX "HDMI: "
/* #define PORT_DEBUG */
/* #define REG_DUMP */

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <mach/msm_hdmi_audio.h>
#include <mach/clk.h>
#include <mach/msm_iomap.h>
#include <mach/socinfo.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>

#include "msm_fb.h"
#include "hdmi_msm_tablet.h"
#include "external_common.h"

#ifdef CONFIG_VIDEO_MHL_TABLET_V1	
#include <linux/switch.h>
#endif

//#undef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
/* Supported HDMI Audio channels */
#define MSM_HDMI_AUDIO_CHANNEL_2		0
#define MSM_HDMI_AUDIO_CHANNEL_4		1
#define MSM_HDMI_AUDIO_CHANNEL_6		2
#define MSM_HDMI_AUDIO_CHANNEL_8		3
#define MSM_HDMI_AUDIO_CHANNEL_MAX		4
#define MSM_HDMI_AUDIO_CHANNEL_FORCE_32BIT	0x7FFFFFFF

/* Supported HDMI Audio sample rates */
#define MSM_HDMI_SAMPLE_RATE_32KHZ		0
#define MSM_HDMI_SAMPLE_RATE_44_1KHZ		1
#define MSM_HDMI_SAMPLE_RATE_48KHZ		2
#define MSM_HDMI_SAMPLE_RATE_88_2KHZ		3
#define MSM_HDMI_SAMPLE_RATE_96KHZ		4
#define MSM_HDMI_SAMPLE_RATE_176_4KHZ		5
#define MSM_HDMI_SAMPLE_RATE_192KHZ		6
#define MSM_HDMI_SAMPLE_RATE_MAX		7
#define MSM_HDMI_SAMPLE_RATE_FORCE_32BIT	0x7FFFFFFF

static int hdmi_msm_hpd_on(bool trigger_handler);
struct workqueue_struct *hdmi_work_queue;

struct hdmi_msm_state_type {
	boolean panel_power_on;
	boolean hpd_initialized;
#ifdef CONFIG_SUSPEND
	boolean pm_suspended;
#endif
	int hpd_stable;
	boolean hpd_prev_state;
	boolean hpd_cable_chg_detected;
	boolean full_auth_done;
	boolean hpd_during_auth;
	struct work_struct hpd_state_work, hpd_read_work;
	struct timer_list hpd_state_timer;
	struct completion ddc_sw_done;

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
	boolean hdcp_activating;
	boolean reauth ;
	struct work_struct hdcp_reauth_work, hdcp_work;
	struct completion hdcp_success_done;
	struct timer_list hdcp_timer;
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT */

	int irq;
	struct msm_hdmi_platform_data *pd;
	struct clk *hdmi_app_clk;
	struct clk *hdmi_m_pclk;
	struct clk *hdmi_s_pclk;
	void __iomem *qfprom_io;
	void __iomem *hdmi_io;

	struct external_common_state_type common;

#ifdef CONFIG_SAMSUNG_HDMI_SWITCH_SET_STATE
	struct switch_dev		hpd_switch;
#endif
	struct wake_lock wake_lock;
	boolean dock_state;
	boolean boot_state;
};

static struct hdmi_msm_state_type *hdmi_msm_state;

DEFINE_MUTEX(hdmi_msm_state_mutex);
DEFINE_MUTEX(hdcp_auth_state_mutex);

static void hdmi_msm_hdcp_enable(void);

uint32 hdmi_msm_get_io_base(void)
{
	return (uint32)MSM_HDMI_BASE;
	//return hdmi_msm_state ? HDMI_BASE : (uint32)NULL;
}
EXPORT_SYMBOL(hdmi_msm_get_io_base);

/* Table indicating the video format supported by the HDMI TX Core v1.0 */
/* Valid Pixel-Clock rates: 25.2MHz, 27MHz, 27.03MHz, 74.25MHz, 148.5MHz */
static void hdmi_msm_setup_video_mode_lut(void)
{
	HDMI_SETUP_LUT(640x480p60_4_3);
	HDMI_SETUP_LUT(720x480p60_4_3);
	HDMI_SETUP_LUT(720x480p60_16_9);
	HDMI_SETUP_LUT(1280x720p60_16_9);
	HDMI_SETUP_LUT(1920x1080i60_16_9);
	HDMI_SETUP_LUT(1440x480i60_4_3);
	HDMI_SETUP_LUT(1440x480i60_16_9);
	HDMI_SETUP_LUT(1920x1080p60_16_9);
	HDMI_SETUP_LUT(720x576p50_4_3);
	HDMI_SETUP_LUT(720x576p50_16_9);
	HDMI_SETUP_LUT(1280x720p50_16_9);
	HDMI_SETUP_LUT(1440x576i50_4_3);
	HDMI_SETUP_LUT(1440x576i50_16_9);
	HDMI_SETUP_LUT(1920x1080p50_16_9);
	HDMI_SETUP_LUT(1920x1080p24_16_9);
	HDMI_SETUP_LUT(1920x1080p25_16_9);
	HDMI_SETUP_LUT(1920x1080p30_16_9);
};

#ifdef PORT_DEBUG
const char *hdmi_msm_name(uint32 offset)
{
	switch (offset) {
	case 0x0000: return "CTRL";
	case 0x0020: return "AUDIO_PKT_CTRL1";
	case 0x0024: return "ACR_PKT_CTRL";
	case 0x0028: return "VBI_PKT_CTRL";
	case 0x002C: return "INFOFRAME_CTRL0";
#ifdef CONFIG_FB_MSM_HDMI_3D
	case 0x0034: return "GEN_PKT_CTRL";
#endif
	case 0x003C: return "ACP";
	case 0x0040: return "GC";
	case 0x0044: return "AUDIO_PKT_CTRL2";
	case 0x0048: return "ISRC1_0";
	case 0x004C: return "ISRC1_1";
	case 0x0050: return "ISRC1_2";
	case 0x0054: return "ISRC1_3";
	case 0x0058: return "ISRC1_4";
	case 0x005C: return "ISRC2_0";
	case 0x0060: return "ISRC2_1";
	case 0x0064: return "ISRC2_2";
	case 0x0068: return "ISRC2_3";
	case 0x006C: return "AVI_INFO0";
	case 0x0070: return "AVI_INFO1";
	case 0x0074: return "AVI_INFO2";
	case 0x0078: return "AVI_INFO3";
#ifdef CONFIG_FB_MSM_HDMI_3D
	case 0x0084: return "GENERIC0_HDR";
	case 0x0088: return "GENERIC0_0";
	case 0x008C: return "GENERIC0_1";
#endif
	case 0x00C4: return "ACR_32_0";
	case 0x00C8: return "ACR_32_1";
	case 0x00CC: return "ACR_44_0";
	case 0x00D0: return "ACR_44_1";
	case 0x00D4: return "ACR_48_0";
	case 0x00D8: return "ACR_48_1";
	case 0x00E4: return "AUDIO_INFO0";
	case 0x00E8: return "AUDIO_INFO1";
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
	case 0x0110: return "HDCP_CTRL";
	case 0x0114: return "HDCP_DEBUG_CTRL";
	case 0x0118: return "HDCP_INT_CTRL";
	case 0x011C: return "HDCP_LINK0_STATUS";
	case 0x012C: return "HDCP_ENTROPY_CTRL0";
	case 0x0130: return "HDCP_RESET";
	case 0x0134: return "HDCP_RCVPORT_DATA0";
	case 0x0138: return "HDCP_RCVPORT_DATA1";
	case 0x013C: return "HDCP_RCVPORT_DATA2";
	case 0x0144: return "HDCP_RCVPORT_DATA3";
	case 0x0148: return "HDCP_RCVPORT_DATA4";
	case 0x014C: return "HDCP_RCVPORT_DATA5";
	case 0x0150: return "HDCP_RCVPORT_DATA6";
	case 0x0168: return "HDCP_RCVPORT_DATA12";
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT */
	case 0x01D0: return "AUDIO_CFG";
	case 0x0208: return "USEC_REFTIMER";
	case 0x020C: return "DDC_CTRL";
	case 0x0214: return "DDC_INT_CTRL";
	case 0x0218: return "DDC_SW_STATUS";
	case 0x021C: return "DDC_HW_STATUS";
	case 0x0220: return "DDC_SPEED";
	case 0x0224: return "DDC_SETUP";
	case 0x0228: return "DDC_TRANS0";
	case 0x022C: return "DDC_TRANS1";
	case 0x0238: return "DDC_DATA";
	case 0x0250: return "HPD_INT_STATUS";
	case 0x0254: return "HPD_INT_CTRL";
	case 0x0258: return "HPD_CTRL";
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
	case 0x025C: return "HDCP_ENTROPY_CTRL1";
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT */
	case 0x027C: return "DDC_REF";
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
	case 0x0284: return "HDCP_SW_UPPER_AKSV";
	case 0x0288: return "HDCP_SW_LOWER_AKSV";
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT */
	case 0x02B4: return "ACTIVE_H";
	case 0x02B8: return "ACTIVE_V";
	case 0x02BC: return "ACTIVE_V_F2";
	case 0x02C0: return "TOTAL";
	case 0x02C4: return "V_TOTAL_F2";
	case 0x02C8: return "FRAME_CTRL";
	case 0x02CC: return "AUD_INT";
	case 0x0300: return "PHY_REG0";
	case 0x0304: return "PHY_REG1";
	case 0x0308: return "PHY_REG2";
	case 0x030C: return "PHY_REG3";
	case 0x0310: return "PHY_REG4";
	case 0x0314: return "PHY_REG5";
	case 0x0318: return "PHY_REG6";
	case 0x031C: return "PHY_REG7";
	case 0x0320: return "PHY_REG8";
	case 0x0324: return "PHY_REG9";
	case 0x0328: return "PHY_REG10";
	case 0x032C: return "PHY_REG11";
	case 0x0330: return "PHY_REG12";
	default: return "???";
	}
}

void hdmi_outp(uint32 offset, uint32 value)
{
	uint32 in_val;

	outpdw(MSM_HDMI_BASE+offset, value);
	in_val = inpdw(MSM_HDMI_BASE+offset);
	DEV_DBG("HDMI[%04x] => %08x [%08x] %s\n",
		offset, value, in_val, hdmi_msm_name(offset));
}

uint32 hdmi_inp(uint32 offset)
{
	uint32 value = inpdw(MSM_HDMI_BASE+offset);
	DEV_DBG("HDMI[%04x] <= %08x %s\n",
		offset, value, hdmi_msm_name(offset));
	return value;
}

//#define HDMI_OUTP_ND(offset, value)	outpdw(HDMI_BASE+(offset), (value))
//#define HDMI_OUTP(offset, value)	__hdmi_outp((offset), (value))
//#define HDMI_INP_ND(offset)		inpdw(HDMI_BASE+(offset))
//#define HDMI_INP(offset)		__hdmi_inp((offset))
//#else /* DEBUG */
//#define HDMI_OUTP_ND(offset, value)	outpdw(HDMI_BASE+(offset), (value))
//#define HDMI_OUTP(offset, value)	outpdw(HDMI_BASE+(offset), (value))
//#define HDMI_INP_ND(offset)		inpdw(HDMI_BASE+(offset))
//#define HDMI_INP(offset)		inpdw(HDMI_BASE+(offset))
#endif /* DEBUG */

static void hdmi_msm_turn_on(void);
static int hdmi_msm_audio_off(void);
static int hdmi_msm_read_edid(void);
static void hdmi_msm_hpd_off(void);
//static void hdmi_msm_set_mode(boolean power_on);


static void hdmi_msm_hpd_state_work(struct work_struct *work)
{
	boolean hpd_state;
	char *envp[2];

	if (!hdmi_msm_state || !hdmi_msm_state->hpd_initialized ||
		!MSM_HDMI_BASE) {
		DEV_DBG("%s: ignored, probe failed\n", __func__);
		return;
	}
#ifdef CONFIG_SUSPEND
	mutex_lock(&hdmi_msm_state_mutex);
	if (hdmi_msm_state->pm_suspended) {
		mutex_unlock(&hdmi_msm_state_mutex);
		DEV_WARN("%s: ignored, pm_suspended\n", __func__);
		return;
	}
	mutex_unlock(&hdmi_msm_state_mutex);
#endif

	/* HPD_INT_STATUS[0x0250] */
	hpd_state = (HDMI_INP(0x0250) & 0x2) >> 1;
	mutex_lock(&external_common_state_hpd_mutex);
	mutex_lock(&hdmi_msm_state_mutex);
	if ((external_common_state->hpd_state != hpd_state) || (hdmi_msm_state->
			hpd_prev_state != external_common_state->hpd_state)) {
		external_common_state->hpd_state = hpd_state;
		hdmi_msm_state->hpd_prev_state =
				external_common_state->hpd_state;
		DEV_DBG("%s: state not stable yet, wait again (%d|%d|%d)\n",
			__func__, hdmi_msm_state->hpd_prev_state,
			external_common_state->hpd_state, hpd_state);
		mutex_unlock(&external_common_state_hpd_mutex);
		hdmi_msm_state->hpd_stable = 0;
		mutex_unlock(&hdmi_msm_state_mutex);
		mod_timer(&hdmi_msm_state->hpd_state_timer, jiffies + HZ/2);
		return;
	}
	mutex_unlock(&external_common_state_hpd_mutex);

	if (hdmi_msm_state->hpd_stable++) {
		mutex_unlock(&hdmi_msm_state_mutex);
		DEV_DBG("%s: no more timer, depending for IRQ now\n",
			__func__);
		return;
	}

	hdmi_msm_state->hpd_stable = 1;
	DEV_INFO("HDMI HPD: event detected\n");

	if (!hdmi_msm_state->hpd_cable_chg_detected) {
		mutex_unlock(&hdmi_msm_state_mutex);
		if (hpd_state) {
			if (!external_common_state->
					disp_mode_list.num_of_elements)
				hdmi_msm_read_edid();
			hdmi_msm_turn_on();
		}
	} else {
		hdmi_msm_state->hpd_cable_chg_detected = FALSE;
		mutex_unlock(&hdmi_msm_state_mutex);
		if (hpd_state) {
			hdmi_msm_read_edid();
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
			hdmi_msm_state->reauth = FALSE ;
#endif
			/* Build EDID table */
			envp[0] = "HDCP_STATE=FAIL";
			envp[1] = NULL;
			DEV_INFO("HDMI HPD: QDSP OFF\n");
			kobject_uevent_env(external_common_state->uevent_kobj,
				KOBJ_CHANGE, envp);
			hdmi_msm_turn_on();
			DEV_INFO("HDMI HPD: sense CONNECTED: send ONLINE\n");
			kobject_uevent(external_common_state->uevent_kobj,
			KOBJ_ONLINE);
			hdmi_msm_hdcp_enable();
#ifdef CONFIG_SAMSUNG_HDMI_SWITCH_SET_STATE
			switch_set_state(&hdmi_msm_state->hpd_switch, 1);
#endif
			wake_lock(&hdmi_msm_state->wake_lock);
		} else {
			DEV_INFO("HDMI HPD: sense DISCONNECTED: send OFFLINE\n"
				);
			kobject_uevent(external_common_state->uevent_kobj,
				KOBJ_OFFLINE);
#ifdef CONFIG_SAMSUNG_HDMI_SWITCH_SET_STATE
			switch_set_state(&hdmi_msm_state->hpd_switch, 0);
#endif
			wake_unlock(&hdmi_msm_state->wake_lock);
		}
	}

	/* HPD_INT_CTRL[0x0254]
	 *   31:10 Reserved
	 *   9     RCV_PLUGIN_DET_MASK	receiver plug in interrupt mask.
	 *                              When programmed to 1,
	 *                              RCV_PLUGIN_DET_INT will toggle
	 *                              the interrupt line
	 *   8:6   Reserved
	 *   5     RX_INT_EN		Panel RX interrupt enable
	 *         0: Disable
	 *         1: Enable
	 *   4     RX_INT_ACK		WRITE ONLY. Panel RX interrupt
	 *                              ack
	 *   3     Reserved
	 *   2     INT_EN		Panel interrupt control
	 *         0: Disable
	 *         1: Enable
	 *   1     INT_POLARITY		Panel interrupt polarity
	 *         0: generate interrupt on disconnect
	 *         1: generate interrupt on connect
	 *   0     INT_ACK		WRITE ONLY. Panel interrupt ack */
	/* Set IRQ for HPD */
	HDMI_OUTP(0x0254, 4 | (hpd_state ? 0 : 2));
}

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
static void hdcp_deauthenticate(void);
static void hdmi_msm_hdcp_reauth_work(struct work_struct *work)
{
#ifdef CONFIG_SUSPEND
	mutex_lock(&hdmi_msm_state_mutex);
	if (hdmi_msm_state->pm_suspended) {
		mutex_unlock(&hdmi_msm_state_mutex);
		DEV_WARN("HDCP: deauthenticating skipped, pm_suspended\n");
		return;
	}
	mutex_unlock(&hdmi_msm_state_mutex);
#endif

	/* Don't process recursive actions */
	mutex_lock(&hdmi_msm_state_mutex);
	if (hdmi_msm_state->hdcp_activating) {
		mutex_unlock(&hdmi_msm_state_mutex);
		return;
	}
	mutex_unlock(&hdmi_msm_state_mutex);

	/*
	 * Reauth=>deauth, hdcp_auth
	 * hdcp_auth=>turn_on() which calls
	 * HDMI Core reset without informing the Audio QDSP
	 * this can do bad things to video playback on the HDTV
	 * Therefore, as surprising as it may sound do reauth
	 * only if the device is HDCP-capable
	 */
	if (external_common_state->present_hdcp) {
	hdcp_deauthenticate();
		mod_timer(&hdmi_msm_state->hdcp_timer, jiffies + HZ/2);
	}
}

static void hdmi_msm_hdcp_work(struct work_struct *work)
{
#ifdef CONFIG_SUSPEND
	mutex_lock(&hdmi_msm_state_mutex);
	if (hdmi_msm_state->pm_suspended) {
		mutex_unlock(&hdmi_msm_state_mutex);
		DEV_WARN("HDCP: Re-enable skipped, pm_suspended\n");
		return;
	}
	mutex_unlock(&hdmi_msm_state_mutex);
#endif

	/* Only re-enable if cable still connected */
	mutex_lock(&external_common_state_hpd_mutex);
	if (external_common_state->hpd_state &&
	    !(hdmi_msm_state->full_auth_done)) {
		mutex_unlock(&external_common_state_hpd_mutex);
		hdmi_msm_state->reauth = TRUE;
		hdmi_msm_turn_on();
	} else
		mutex_unlock(&external_common_state_hpd_mutex);
}
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT */

static irqreturn_t hdmi_msm_isr(int irq, void *dev_id)
{
	uint32 hpd_int_status;
	uint32 hpd_int_ctrl;
	uint32 ddc_int_ctrl;
	uint32 audio_int_val;
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
	uint32 hdcp_int_val;
	char *envp[2];
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT */
	static uint32 fifo_urun_int_occurred;
	static uint32 sample_drop_int_occurred;
	const uint32 occurrence_limit = 5;

	if (!hdmi_msm_state || !hdmi_msm_state->hpd_initialized ||
		!MSM_HDMI_BASE) {
		DEV_DBG("ISR ignored, probe failed\n");
		return IRQ_HANDLED;
	}
#ifdef CONFIG_SUSPEND
	mutex_lock(&hdmi_msm_state_mutex);
	if (hdmi_msm_state->pm_suspended) {
		mutex_unlock(&hdmi_msm_state_mutex);
		DEV_WARN("ISR ignored, pm_suspended\n");
		return IRQ_HANDLED;
	}
	mutex_unlock(&hdmi_msm_state_mutex);
#endif

	/* Process HPD Interrupt */
	/* HDMI_HPD_INT_STATUS[0x0250] */
	hpd_int_status = HDMI_INP_ND(0x0250);
	/* HDMI_HPD_INT_CTRL[0x0254] */
	hpd_int_ctrl = HDMI_INP_ND(0x0254);
	if ((hpd_int_ctrl & (1 << 2)) && (hpd_int_status & (1 << 0))) {
		boolean cable_detected = (hpd_int_status & 2) >> 1;

		/* HDMI_HPD_INT_CTRL[0x0254] */
		/* Clear all interrupts, timer will turn IRQ back on */
		HDMI_OUTP(0x0254, 1 << 0);

		DEV_DBG("%s: HPD IRQ, Ctrl=%04x, State=%04x\n", __func__,
			hpd_int_ctrl, hpd_int_status);
		mutex_lock(&hdmi_msm_state_mutex);
		hdmi_msm_state->hpd_cable_chg_detected = TRUE;

		/* ensure 2 readouts */
		hdmi_msm_state->hpd_prev_state = cable_detected ? 0 : 1;
		external_common_state->hpd_state = cable_detected ? 1 : 0;
		hdmi_msm_state->hpd_stable = 0;
		mod_timer(&hdmi_msm_state->hpd_state_timer, jiffies + HZ/2);
		mutex_unlock(&hdmi_msm_state_mutex);
		/*
		 * HDCP Compliance 1A-01:
		 * The Quantum Data Box 882 triggers two consecutive
		 * HPD events very close to each other as a part of this
		 * test which can trigger two parallel HDCP auth threads
		 * if HDCP authentication is going on and we get ISR
		 * then stop the authentication , rather than
		 * reauthenticating it again
		 */
		if (!(hdmi_msm_state->full_auth_done)) {
			DEV_DBG("%s getting hpd while authenticating\n",\
			    __func__);
			mutex_lock(&hdcp_auth_state_mutex);
			hdmi_msm_state->hpd_during_auth = TRUE;
			mutex_unlock(&hdcp_auth_state_mutex);
		}
		return IRQ_HANDLED;
	}
	/* Process DDC Interrupts */
	/* HDMI_DDC_INT_CTRL[0x0214] */
	ddc_int_ctrl = HDMI_INP_ND(0x0214);
	if ((ddc_int_ctrl & (1 << 2)) && (ddc_int_ctrl & (1 << 0))) {
		/* SW_DONE INT occured, clr it */
		HDMI_OUTP_ND(0x0214, ddc_int_ctrl | (1 << 1));
		complete(&hdmi_msm_state->ddc_sw_done);
		return IRQ_HANDLED;
	}
	/* FIFO Underrun Int is enabled */
	/* HDMI_AUD_INT[0x02CC]
	 *   [3] AUD_SAM_DROP_MASK [R/W]
	 *   [2] AUD_SAM_DROP_ACK [W], AUD_SAM_DROP_INT [R]
	 *   [1] AUD_FIFO_URUN_MASK [R/W]
	 *   [0] AUD_FIFO_URUN_ACK [W], AUD_FIFO_URUN_INT [R] */
	audio_int_val = HDMI_INP_ND(0x02CC);
	if ((audio_int_val & (1 << 1)) && (audio_int_val & (1 << 0))) {
		/* FIFO Underrun occured, clr it */
		HDMI_OUTP(0x02CC, audio_int_val | (1 << 0));

		++fifo_urun_int_occurred;
		DEV_INFO("HDMI AUD_FIFO_URUN: %d\n", fifo_urun_int_occurred);

		if (fifo_urun_int_occurred >= occurrence_limit) {
			HDMI_OUTP(0x02CC, HDMI_INP(0x02CC) & ~(1 << 1));
			DEV_INFO("HDMI AUD_FIFO_URUN: INT has been disabled "
				"by the ISR after %d occurences...\n",
				fifo_urun_int_occurred);
		}
		return IRQ_HANDLED;
	}

	/* Audio Sample Drop int is enabled */
	if ((audio_int_val & (1 << 3)) && (audio_int_val & (1 << 2))) {
		/* Audio Sample Drop occured, clr it */
		HDMI_OUTP(0x02CC, audio_int_val | (1 << 2));
		DEV_DBG("%s: AUD_SAM_DROP", __func__);

		++sample_drop_int_occurred;
		if (sample_drop_int_occurred >= occurrence_limit) {
			HDMI_OUTP(0x02CC, HDMI_INP(0x02CC) & ~(1 << 3));
			DEV_INFO("HDMI AUD_SAM_DROP: INT has been disabled "
				"by the ISR after %d occurences...\n",
				sample_drop_int_occurred);
		}
		return IRQ_HANDLED;
	}

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
	/* HDCP_INT_CTRL[0x0118]
	 *    [0] AUTH_SUCCESS_INT	[R]	HDCP Authentication Success
	 *		interrupt status
	 *    [1] AUTH_SUCCESS_ACK	[W]	Acknowledge bit for HDCP
	 *		Authentication Success bit - write 1 to clear
	 *    [2] AUTH_SUCCESS_MASK	[R/W]	Mask bit for HDCP Authentication
	 *		Success interrupt - set to 1 to enable interrupt */
	hdcp_int_val = HDMI_INP_ND(0x0118);
	if ((hdcp_int_val & (1 << 2)) && (hdcp_int_val & (1 << 0))) {
		/* AUTH_SUCCESS_INT */
		HDMI_OUTP(0x0118, (hdcp_int_val | (1 << 1)) & ~(1 << 0));
		DEV_INFO("HDCP: AUTH_SUCCESS_INT received\n");
		complete_all(&hdmi_msm_state->hdcp_success_done);
		return IRQ_HANDLED;
	}
	/*    [4] AUTH_FAIL_INT		[R]	HDCP Authentication Lost
	 *		interrupt Status
	 *    [5] AUTH_FAIL_ACK		[W]	Acknowledge bit for HDCP
	 *		Authentication Lost bit - write 1 to clear
	 *    [6] AUTH_FAIL_MASK	[R/W]	Mask bit fo HDCP Authentication
	 *		Lost interrupt set to 1 to enable interrupt
	 *    [7] AUTH_FAIL_INFO_ACK	[W]	Acknowledge bit for HDCP
	 *		Authentication Failure Info field - write 1 to clear */
	if ((hdcp_int_val & (1 << 6)) && (hdcp_int_val & (1 << 4))) {
		/* AUTH_FAIL_INT */
		/* Clear and Disable */
		HDMI_OUTP(0x0118, (hdcp_int_val | (1 << 5))
			& ~((1 << 6) | (1 << 4)));
		DEV_INFO("HDCP: AUTH_FAIL_INT received, LINK0_STATUS=0x%08x\n",
			HDMI_INP_ND(0x011C));
		if (hdmi_msm_state->full_auth_done) {
			envp[0] = "HDCP_STATE=FAIL";
			envp[1] = NULL;
			DEV_INFO("HDMI HPD:QDSP OFF\n");
			kobject_uevent_env(external_common_state->uevent_kobj,
			KOBJ_CHANGE, envp);
			hdmi_msm_set_mode(true); // to stop the noisy screen display
			mutex_lock(&hdcp_auth_state_mutex);
			hdmi_msm_state->full_auth_done = FALSE;
			mutex_unlock(&hdcp_auth_state_mutex);
			/* Calling reauth only when authentication
			 * is sucessful or else we always go into
			 * the reauth loop
			 */
			queue_work(hdmi_work_queue,
			    &hdmi_msm_state->hdcp_reauth_work);
		}
		mutex_lock(&hdcp_auth_state_mutex);
		/* This flag prevents other threads from re-authenticating
		 * after we've just authenticated (i.e., finished part3)
		 */
		hdmi_msm_state->full_auth_done = FALSE;

		mutex_unlock(&hdcp_auth_state_mutex);
		DEV_DBG("calling reauthenticate from %s HDCP FAIL INT ",
		    __func__);

		return IRQ_HANDLED;
	}
	/*    [8] DDC_XFER_REQ_INT	[R]	HDCP DDC Transfer Request
	 *		interrupt status
	 *    [9] DDC_XFER_REQ_ACK	[W]	Acknowledge bit for HDCP DDC
	 *		Transfer Request bit - write 1 to clear
	 *   [10] DDC_XFER_REQ_MASK	[R/W]	Mask bit for HDCP DDC Transfer
	 *		Request interrupt - set to 1 to enable interrupt */
	if ((hdcp_int_val & (1 << 10)) && (hdcp_int_val & (1 << 8))) {
		/* DDC_XFER_REQ_INT */
		HDMI_OUTP_ND(0x0118, (hdcp_int_val | (1 << 9)) & ~(1 << 8));
		if (!(hdcp_int_val & (1 << 12)))
			return IRQ_HANDLED;
	}
	/*   [12] DDC_XFER_DONE_INT	[R]	HDCP DDC Transfer done interrupt
	 *		status
	 *   [13] DDC_XFER_DONE_ACK	[W]	Acknowledge bit for HDCP DDC
	 *		Transfer done bit - write 1 to clear
	 *   [14] DDC_XFER_DONE_MASK	[R/W]	Mask bit for HDCP DDC Transfer
	 *		done interrupt - set to 1 to enable interrupt */
	if ((hdcp_int_val & (1 << 14)) && (hdcp_int_val & (1 << 12))) {
		/* DDC_XFER_DONE_INT */
		HDMI_OUTP_ND(0x0118, (hdcp_int_val | (1 << 13)) & ~(1 << 12));
		DEV_INFO("HDCP: DDC_XFER_DONE received\n");
		return IRQ_HANDLED;
	}
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT */

	DEV_DBG("%s: HPD<Ctrl=%04x, State=%04x>, ddc_int_ctrl=%04x, "
		"aud_int=%04x, cec_int=%04x\n", __func__, hpd_int_ctrl,
		hpd_int_status, ddc_int_ctrl, audio_int_val,
		HDMI_INP_ND(0x029C));

	return IRQ_HANDLED;
}

static int check_hdmi_features(void)
{
	/* RAW_FEAT_CONFIG_ROW0_LSB */
	uint32 val = inpdw(QFPROM_BASE + 0x0238);
	/* HDMI_DISABLE */
	boolean hdmi_disabled = (val & 0x00200000) >> 21;
	/* HDCP_DISABLE */
	boolean hdcp_disabled = (val & 0x00400000) >> 22;

	DEV_DBG("Features <val:0x%08x, HDMI:%s, HDCP:%s>\n", val,
		hdmi_disabled ? "OFF" : "ON", hdcp_disabled ? "OFF" : "ON");
	if (hdmi_disabled) {
		DEV_ERR("ERROR: HDMI disabled\n");
		return -ENODEV;
	}

	if (hdcp_disabled)
		DEV_WARN("WARNING: HDCP disabled\n");

	return 0;
}

static boolean hdmi_msm_has_hdcp(void)
{
	/* RAW_FEAT_CONFIG_ROW0_LSB, HDCP_DISABLE */
	return (inpdw(QFPROM_BASE + 0x0238) & 0x00400000) ? FALSE : TRUE;
}

static boolean hdmi_msm_is_power_on(void)
{
	/* HDMI_CTRL, ENABLE */
	return (HDMI_INP_ND(0x0000) & 0x00000001) ? TRUE : FALSE;
}

/* 1.2.1.2.1 DVI Operation
 * HDMI compliance requires the HDMI core to support DVI as well. The
 * HDMI core also supports DVI. In DVI operation there are no preambles
 * and guardbands transmitted. THe TMDS encoding of video data remains
 * the same as HDMI. There are no VBI or audio packets transmitted. In
 * order to enable DVI mode in HDMI core, HDMI_DVI_SEL field of
 * HDMI_CTRL register needs to be programmed to 0. */
static boolean hdmi_msm_is_dvi_mode(void)
{
	/* HDMI_CTRL, HDMI_DVI_SEL */
	return (HDMI_INP_ND(0x0000) & 0x00000002) ? FALSE : TRUE;
}

void hdmi_msm_set_mode(boolean power_on)
{
	uint32 reg_val = 0;
	if (power_on) {
		/* ENABLE */
		reg_val |= 0x00000001; /* Enable the block */
		if (external_common_state->hdmi_sink == 0) {
			/* HDMI_DVI_SEL */
			reg_val |= 0x00000002;
			/* HDMI_Encryption_ON */
			reg_val |= 0x00000004;
			/* HDMI_CTRL */
			HDMI_OUTP(0x0000, reg_val);
			/* HDMI_DVI_SEL */
			reg_val &= ~0x00000002;
			reg_val |= 0x00000004;
		} else
			{
			reg_val |= 0x00000002;
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
			/* HDMI_Encryption_ON */
			reg_val |= 0x00000006;
#endif
			}
	} else
		reg_val = 0x00000002;

	/* HDMI_CTRL */
	HDMI_OUTP(0x0000, reg_val);
	DEV_DBG("HDMI Core: %s\n", power_on ? "Enable" : "Disable");
}

static void msm_hdmi_init_ddc(void)
{
	/* 0x0220 HDMI_DDC_SPEED
	   [31:16] PRESCALE prescale = (m * xtal_frequency) /
		(desired_i2c_speed), where m is multiply
		factor, default: m = 1
	   [1:0]   THRESHOLD Select threshold to use to determine whether value
		sampled on SDA is a 1 or 0. Specified in terms of the ratio
		between the number of sampled ones and the total number of times
		SDA is sampled.
		* 0x0: >0
		* 0x1: 1/4 of total samples
		* 0x2: 1/2 of total samples
		* 0x3: 3/4 of total samples */
	/* Configure the Pre-Scale multiplier
	 * Configure the Threshold */
	HDMI_OUTP_ND(0x0220, (10 << 16) | (2 << 0));

	/* 0x0224 HDMI_DDC_SETUP */
	//HDMI_OUTP_ND(0x0224, 0);
	HDMI_OUTP_ND(0x0224, 0xff000000); //CASE00545067 patch


	/* 0x027C HDMI_DDC_REF
	   [6] REFTIMER_ENABLE	Enable the timer
		* 0: Disable
		* 1: Enable
	   [15:0] REFTIMER	Value to set the register in order to generate
		DDC strobe. This register counts on HDCP application clock */
	/* Enable reference timer
	 * 27 micro-seconds */
	HDMI_OUTP_ND(0x027C, (1 << 16) | (27 << 0));
}

static int hdmi_msm_ddc_clear_irq(const char *what)
{
	const uint32 time_out = 0xFFFF;
	uint32 time_out_count, reg_val;

	/* clear pending and enable interrupt */
	time_out_count = time_out;
	do {
		--time_out_count;
		/* HDMI_DDC_INT_CTRL[0x0214]
		   [2] SW_DONE_MK Mask bit for SW_DONE_INT. Set to 1 to enable
		       interrupt.
		   [1] SW_DONE_ACK WRITE ONLY. Acknowledge bit for SW_DONE_INT.
		       Write 1 to clear interrupt.
		   [0] SW_DONE_INT READ ONLY. SW_DONE interrupt status */
		/* Clear and Enable DDC interrupt */
		/* Write */
		HDMI_OUTP_ND(0x0214, (1 << 2) | (1 << 1));
		/* Read back */
		reg_val = HDMI_INP_ND(0x0214);
	} while ((reg_val & 0x1) && time_out_count);
	if (!time_out_count) {
		DEV_ERR("%s[%s]: timedout\n", __func__, what);
		return -ETIMEDOUT;
	}

	return 0;
}

static int hdmi_msm_ddc_write(uint32 dev_addr, uint32 offset,
	const uint8 *data_buf, uint32 data_len, const char *what)
{
	uint32 reg_val, ndx;
	int status = 0, retry = 10;
	uint32 time_out_count;

	if (NULL == data_buf) {
		status = -EINVAL;
		DEV_ERR("%s[%s]: invalid input paramter\n", __func__, what);
		goto error;
	}

again:
	status = hdmi_msm_ddc_clear_irq(what);
	if (status)
		goto error;

	/* Ensure Device Address has LSB set to 0 to indicate Slave addr read */
	dev_addr &= 0xFE;

	/* 0x0238 HDMI_DDC_DATA
	   [31] INDEX_WRITE WRITE ONLY. To write index field, set this bit to
		1 while writing HDMI_DDC_DATA.
	   [23:16] INDEX Use to set index into DDC buffer for next read or
		current write, or to read index of current read or next write.
		Writable only when INDEX_WRITE=1.
	   [15:8] DATA Use to fill or read the DDC buffer
	   [0] DATA_RW Select whether buffer access will be a read or write.
		For writes, address auto-increments on write to HDMI_DDC_DATA.
		For reads, address autoincrements on reads to HDMI_DDC_DATA.
		* 0: Write
		* 1: Read */

	/* 1. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #1
	 *    DATA_RW = 0x1 (write)
	 *    DATA = linkAddress (primary link address and writing)
	 *    INDEX = 0x0 (initial offset into buffer)
	 *    INDEX_WRITE = 0x1 (setting initial offset) */
	HDMI_OUTP_ND(0x0238, (0x1UL << 31) | (dev_addr << 8));

	/* 2. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #2
	 *    DATA_RW = 0x0 (write)
	 *    DATA = offsetAddress
	 *    INDEX = 0x0
	 *    INDEX_WRITE = 0x0 (auto-increment by hardware) */
	HDMI_OUTP_ND(0x0238, offset << 8);

	/* 3. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #3
	 *    DATA_RW = 0x0 (write)
	 *    DATA = data_buf[ndx]
	 *    INDEX = 0x0
	 *    INDEX_WRITE = 0x0 (auto-increment by hardware) */
	for (ndx = 0; ndx < data_len; ++ndx)
		HDMI_OUTP_ND(0x0238, ((uint32)data_buf[ndx]) << 8);

	/* Data setup is complete, now setup the transaction characteristics */

	/* 0x0228 HDMI_DDC_TRANS0
	   [23:16] CNT0 Byte count for first transaction (excluding the first
		byte, which is usually the address).
	   [13] STOP0 Determines whether a stop bit will be sent after the first
		transaction
		* 0: NO STOP
		* 1: STOP
	   [12] START0 Determines whether a start bit will be sent before the
		first transaction
		* 0: NO START
		* 1: START
	   [8] STOP_ON_NACK0 Determines whether the current transfer will stop
		if a NACK is received during the first transaction (current
		transaction always stops).
		* 0: STOP CURRENT TRANSACTION, GO TO NEXT TRANSACTION
		* 1: STOP ALL TRANSACTIONS, SEND STOP BIT
	   [0] RW0 Read/write indicator for first transaction - set to 0 for
		write, 1 for read. This bit only controls HDMI_DDC behaviour -
		the R/W bit in the transaction is programmed into the DDC buffer
		as the LSB of the address byte.
		* 0: WRITE
		* 1: READ */

	/* 4. Write to HDMI_I2C_TRANSACTION0 with the following fields set in
	      order to handle characteristics of portion #1 and portion #2
	 *    RW0 = 0x0 (write)
	 *    START0 = 0x1 (insert START bit)
	 *    STOP0 = 0x0 (do NOT insert STOP bit)
	 *    CNT0 = 0x1 (single byte transaction excluding address) */
	HDMI_OUTP_ND(0x0228, (1 << 12) | (1 << 16));

	/* 0x022C HDMI_DDC_TRANS1
	  [23:16] CNT1 Byte count for second transaction (excluding the first
		byte, which is usually the address).
	  [13] STOP1 Determines whether a stop bit will be sent after the second
		transaction
		* 0: NO STOP
		* 1: STOP
	  [12] START1 Determines whether a start bit will be sent before the
		second transaction
		* 0: NO START
		* 1: START
	  [8] STOP_ON_NACK1 Determines whether the current transfer will stop if
		a NACK is received during the second transaction (current
		transaction always stops).
		* 0: STOP CURRENT TRANSACTION, GO TO NEXT TRANSACTION
		* 1: STOP ALL TRANSACTIONS, SEND STOP BIT
	  [0] RW1 Read/write indicator for second transaction - set to 0 for
		write, 1 for read. This bit only controls HDMI_DDC behaviour -
		the R/W bit in the transaction is programmed into the DDC buffer
		as the LSB of the address byte.
		* 0: WRITE
		* 1: READ */

	/* 5. Write to HDMI_I2C_TRANSACTION1 with the following fields set in
	      order to handle characteristics of portion #3
	 *    RW1 = 0x1 (read)
	 *    START1 = 0x1 (insert START bit)
	 *    STOP1 = 0x1 (insert STOP bit)
	 *    CNT1 = data_len   (0xN (write N bytes of data))
	 *    Byte count for second transition (excluding the first
	 *    Byte which is usually the address) */
	HDMI_OUTP_ND(0x022C, (1 << 13) | ((data_len-1) << 16));

	/* Trigger the I2C transfer */
	/* 0x020C HDMI_DDC_CTRL
	   [21:20] TRANSACTION_CNT
		Number of transactions to be done in current transfer.
		* 0x0: transaction0 only
		* 0x1: transaction0, transaction1
		* 0x2: transaction0, transaction1, transaction2
		* 0x3: transaction0, transaction1, transaction2, transaction3
	   [3] SW_STATUS_RESET
		Write 1 to reset HDMI_DDC_SW_STATUS flags, will reset SW_DONE,
		ABORTED, TIMEOUT, SW_INTERRUPTED, BUFFER_OVERFLOW,
		STOPPED_ON_NACK, NACK0, NACK1, NACK2, NACK3
	   [2] SEND_RESET Set to 1 to send reset sequence (9 clocks with no
		data) at start of transfer.  This sequence is sent after GO is
		written to 1, before the first transaction only.
	   [1] SOFT_RESET Write 1 to reset DDC controller
	   [0] GO WRITE ONLY. Write 1 to start DDC transfer. */

	/* 6. Write to HDMI_I2C_CONTROL to kick off the hardware.
	 *    Note that NOTHING has been transmitted on the DDC lines up to this
	 *    point.
	 *    TRANSACTION_CNT = 0x1 (execute transaction0 followed by
	 *    transaction1)
	 *    GO = 0x1 (kicks off hardware) */
	INIT_COMPLETION(hdmi_msm_state->ddc_sw_done);
	HDMI_OUTP_ND(0x020C, (1 << 0) | (1 << 20));

	time_out_count = wait_for_completion_interruptible_timeout(
		&hdmi_msm_state->ddc_sw_done, HZ/2);
	HDMI_OUTP_ND(0x0214, 0x2);
	if (!time_out_count) {
		if (retry-- > 0) {
			DEV_INFO("%s[%s]: failed timout, retry=%d\n", __func__,
				what, retry);
			goto again;
		}
		status = -ETIMEDOUT;
		DEV_ERR("%s[%s]: timedout, DDC SW Status=%08x, HW "
			"Status=%08x, Int Ctrl=%08x\n", __func__, what,
			HDMI_INP_ND(0x0218), HDMI_INP_ND(0x021C),
			HDMI_INP_ND(0x0214));
		goto error;
	}

	/* Read DDC status */
	reg_val = HDMI_INP_ND(0x0218);
	reg_val &= 0x00001000 | 0x00002000 | 0x00004000 | 0x00008000;

	/* Check if any NACK occurred */
	if (reg_val) {
		if (retry > 1)
			HDMI_OUTP_ND(0x020C, BIT(3)); /* SW_STATUS_RESET */
		else
			HDMI_OUTP_ND(0x020C, BIT(1)); /* SOFT_RESET */
		if (retry-- > 0) {
			DEV_DBG("%s[%s]: failed NACK=%08x, retry=%d\n",
				__func__, what, reg_val, retry);
			msleep(100);
			goto again;
		}
		status = -EIO;
		DEV_ERR("%s[%s]: failed NACK: %08x\n", __func__, what, reg_val);
		goto error;
	}

	DEV_DBG("%s[%s] success\n", __func__, what);

error:
	return status;
}

static int hdmi_msm_ddc_read_retry(uint32 dev_addr, uint32 offset,
	uint8 *data_buf, uint32 data_len, uint32 request_len, int retry,
	const char *what)
{
	uint32 reg_val, ndx;
	int status = 0;
	uint32 time_out_count;
	int log_retry_fail = retry != 1;

	if (NULL == data_buf) {
		status = -EINVAL;
		DEV_ERR("%s: invalid input paramter\n", __func__);
		goto error;
	}

again:
	status = hdmi_msm_ddc_clear_irq(what);
	if (status)
		goto error;

	/* Ensure Device Address has LSB set to 0 to indicate Slave addr read */
	dev_addr &= 0xFE;

	/* 0x0238 HDMI_DDC_DATA
	   [31] INDEX_WRITE WRITE ONLY. To write index field, set this bit to
		1 while writing HDMI_DDC_DATA.
	   [23:16] INDEX Use to set index into DDC buffer for next read or
		current write, or to read index of current read or next write.
		Writable only when INDEX_WRITE=1.
	   [15:8] DATA Use to fill or read the DDC buffer
	   [0] DATA_RW Select whether buffer access will be a read or write.
		For writes, address auto-increments on write to HDMI_DDC_DATA.
		For reads, address autoincrements on reads to HDMI_DDC_DATA.
		* 0: Write
		* 1: Read */

	/* 1. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #1
	 *    DATA_RW = 0x0 (write)
	 *    DATA = linkAddress (primary link address and writing)
	 *    INDEX = 0x0 (initial offset into buffer)
	 *    INDEX_WRITE = 0x1 (setting initial offset) */
	HDMI_OUTP_ND(0x0238, (0x1UL << 31) | (dev_addr << 8));

	/* 2. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #2
	 *    DATA_RW = 0x0 (write)
	 *    DATA = offsetAddress
	 *    INDEX = 0x0
	 *    INDEX_WRITE = 0x0 (auto-increment by hardware) */
	HDMI_OUTP_ND(0x0238, offset << 8);

	/* 3. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #3
	 *    DATA_RW = 0x0 (write)
	 *    DATA = linkAddress + 1 (primary link address 0x74 and reading)
	 *    INDEX = 0x0
	 *    INDEX_WRITE = 0x0 (auto-increment by hardware) */
	HDMI_OUTP_ND(0x0238, (dev_addr | 1) << 8);

	/* Data setup is complete, now setup the transaction characteristics */

	/* 0x0228 HDMI_DDC_TRANS0
	   [23:16] CNT0 Byte count for first transaction (excluding the first
		byte, which is usually the address).
	   [13] STOP0 Determines whether a stop bit will be sent after the first
		transaction
		* 0: NO STOP
		* 1: STOP
	   [12] START0 Determines whether a start bit will be sent before the
		first transaction
		* 0: NO START
		* 1: START
	   [8] STOP_ON_NACK0 Determines whether the current transfer will stop
		if a NACK is received during the first transaction (current
		transaction always stops).
		* 0: STOP CURRENT TRANSACTION, GO TO NEXT TRANSACTION
		* 1: STOP ALL TRANSACTIONS, SEND STOP BIT
	   [0] RW0 Read/write indicator for first transaction - set to 0 for
		write, 1 for read. This bit only controls HDMI_DDC behaviour -
		the R/W bit in the transaction is programmed into the DDC buffer
		as the LSB of the address byte.
		* 0: WRITE
		* 1: READ */

	/* 4. Write to HDMI_I2C_TRANSACTION0 with the following fields set in
	      order to handle characteristics of portion #1 and portion #2
	 *    RW0 = 0x0 (write)
	 *    START0 = 0x1 (insert START bit)
	 *    STOP0 = 0x0 (do NOT insert STOP bit)
	 *    CNT0 = 0x1 (single byte transaction excluding address) */
	HDMI_OUTP_ND(0x0228, (1 << 12) | (1 << 16));

	/* 0x022C HDMI_DDC_TRANS1
	  [23:16] CNT1 Byte count for second transaction (excluding the first
		byte, which is usually the address).
	  [13] STOP1 Determines whether a stop bit will be sent after the second
		transaction
		* 0: NO STOP
		* 1: STOP
	  [12] START1 Determines whether a start bit will be sent before the
		second transaction
		* 0: NO START
		* 1: START
	  [8] STOP_ON_NACK1 Determines whether the current transfer will stop if
		a NACK is received during the second transaction (current
		transaction always stops).
		* 0: STOP CURRENT TRANSACTION, GO TO NEXT TRANSACTION
		* 1: STOP ALL TRANSACTIONS, SEND STOP BIT
	  [0] RW1 Read/write indicator for second transaction - set to 0 for
		write, 1 for read. This bit only controls HDMI_DDC behaviour -
		the R/W bit in the transaction is programmed into the DDC buffer
		as the LSB of the address byte.
		* 0: WRITE
		* 1: READ */

	/* 5. Write to HDMI_I2C_TRANSACTION1 with the following fields set in
	      order to handle characteristics of portion #3
	 *    RW1 = 0x1 (read)
	 *    START1 = 0x1 (insert START bit)
	 *    STOP1 = 0x1 (insert STOP bit)
	 *    CNT1 = data_len   (it's 128 (0x80) for a blk read) */
	HDMI_OUTP_ND(0x022C, 1 | (1 << 12) | (1 << 13) | (request_len << 16));

	/* Trigger the I2C transfer */
	/* 0x020C HDMI_DDC_CTRL
	   [21:20] TRANSACTION_CNT
		Number of transactions to be done in current transfer.
		* 0x0: transaction0 only
		* 0x1: transaction0, transaction1
		* 0x2: transaction0, transaction1, transaction2
		* 0x3: transaction0, transaction1, transaction2, transaction3
	   [3] SW_STATUS_RESET
		Write 1 to reset HDMI_DDC_SW_STATUS flags, will reset SW_DONE,
		ABORTED, TIMEOUT, SW_INTERRUPTED, BUFFER_OVERFLOW,
		STOPPED_ON_NACK, NACK0, NACK1, NACK2, NACK3
	   [2] SEND_RESET Set to 1 to send reset sequence (9 clocks with no
		data) at start of transfer.  This sequence is sent after GO is
		written to 1, before the first transaction only.
	   [1] SOFT_RESET Write 1 to reset DDC controller
	   [0] GO WRITE ONLY. Write 1 to start DDC transfer. */

	/* 6. Write to HDMI_I2C_CONTROL to kick off the hardware.
	 *    Note that NOTHING has been transmitted on the DDC lines up to this
	 *    point.
	 *    TRANSACTION_CNT = 0x1 (execute transaction0 followed by
	 *    transaction1)
	 *    SEND_RESET = Set to 1 to send reset sequence
	 *    GO = 0x1 (kicks off hardware) */
	INIT_COMPLETION(hdmi_msm_state->ddc_sw_done);
	HDMI_OUTP_ND(0x020C, (1 << 0) | (1 << 20));

	time_out_count = wait_for_completion_interruptible_timeout(
		&hdmi_msm_state->ddc_sw_done, HZ/2);
	HDMI_OUTP_ND(0x0214, 0x2);
	if (!time_out_count) {
		if (retry-- > 0) {
			DEV_INFO("%s: failed timout, retry=%d\n", __func__,
				retry);
			goto again;
		}
		status = -ETIMEDOUT;
		DEV_ERR("%s: timedout(7), DDC SW Status=%08x, HW "
			"Status=%08x, Int Ctrl=%08x\n", __func__,
			HDMI_INP(0x0218), HDMI_INP(0x021C), HDMI_INP(0x0214));
		goto error;
	}

	/* Read DDC status */
	reg_val = HDMI_INP_ND(0x0218);
	reg_val &= 0x00001000 | 0x00002000 | 0x00004000 | 0x00008000;

	/* Check if any NACK occurred */
	if (reg_val) {
		HDMI_OUTP_ND(0x020C, BIT(3)); /* SW_STATUS_RESET */
		if (retry == 1)
			HDMI_OUTP_ND(0x020C, BIT(1)); /* SOFT_RESET */
		if (retry-- > 0) {
			DEV_DBG("%s(%s): failed NACK=0x%08x, retry=%d, "
				"dev-addr=0x%02x, offset=0x%02x, "
				"length=%d\n", __func__, what,
				reg_val, retry, dev_addr,
				offset, data_len);
			goto again;
		}
		status = -EIO;
		if (log_retry_fail)
			DEV_ERR("%s(%s): failed NACK=0x%08x, dev-addr=0x%02x, "
				"offset=0x%02x, length=%d\n", __func__, what,
				reg_val, dev_addr, offset, data_len);
		goto error;
	}

	/* 0x0238 HDMI_DDC_DATA
	   [31] INDEX_WRITE WRITE ONLY. To write index field, set this bit to 1
		while writing HDMI_DDC_DATA.
	   [23:16] INDEX Use to set index into DDC buffer for next read or
		current write, or to read index of current read or next write.
		Writable only when INDEX_WRITE=1.
	   [15:8] DATA Use to fill or read the DDC buffer
	   [0] DATA_RW Select whether buffer access will be a read or write.
		For writes, address auto-increments on write to HDMI_DDC_DATA.
		For reads, address autoincrements on reads to HDMI_DDC_DATA.
		* 0: Write
		* 1: Read */

	/* 8. ALL data is now available and waiting in the DDC buffer.
	 *    Read HDMI_I2C_DATA with the following fields set
	 *    RW = 0x1 (read)
	 *    DATA = BCAPS (this is field where data is pulled from)
	 *    INDEX = 0x3 (where the data has been placed in buffer by hardware)
	 *    INDEX_WRITE = 0x1 (explicitly define offset) */
	/* Write this data to DDC buffer */
	HDMI_OUTP_ND(0x0238, 0x1 | (3 << 16) | (1 << 31));

	/* Discard first byte */
	HDMI_INP_ND(0x0238);
	for (ndx = 0; ndx < data_len; ++ndx) {
		reg_val = HDMI_INP_ND(0x0238);
		data_buf[ndx] = (uint8) ((reg_val & 0x0000FF00) >> 8);
	}

	DEV_DBG("%s[%s] success\n", __func__, what);

error:
	return status;
}

static int hdmi_msm_ddc_set_seg( uint32 request_len, int seg_num,
	const char *what)
{
	uint32 reg_val, ndx;
	int status = 0;
	uint32 time_out_count;
	int retry = 1;
	int log_retry_fail = retry != 1;
	int seg_addr = 0x60;//, seg_num = 0x01;


again:
	status = hdmi_msm_ddc_clear_irq(what);
	if (status)
		goto error;

	/* Ensure Device Address has LSB set to 0 to indicate Slave addr read */
	//dev_addr &= 0xFE;

	/* 0x0238 HDMI_DDC_DATA
	   [31] INDEX_WRITE WRITE ONLY. To write index field, set this bit to
		1 while writing HDMI_DDC_DATA.
	   [23:16] INDEX Use to set index into DDC buffer for next read or
		current write, or to read index of current read or next write.
		Writable only when INDEX_WRITE=1.
	   [15:8] DATA Use to fill or read the DDC buffer
	   [0] DATA_RW Select whether buffer access will be a read or write.
		For writes, address auto-increments on write to HDMI_DDC_DATA.
		For reads, address autoincrements on reads to HDMI_DDC_DATA.
		e 0: Write
		* 1: Read */

	/* 1. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #1
	 *    DATA_RW = 0x0 (write)
	 *    DATA = linkAddress (primary link address and writing)
	 *    INDEX = 0x0 (initial offset into buffer)
	 *    INDEX_WRITE = 0x1 (setting initial offset) */
	HDMI_OUTP_ND(0x0238, (0x1UL << 31) | (seg_addr << 8));

	/* 2. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #2
	 *    DATA_RW = 0x0 (write)
	 *    DATA = offsetAddress
	 *    INDEX = 0x0
	 *    INDEX_WRITE = 0x0 (auto-increment by hardware) */
	HDMI_OUTP_ND(0x0238, seg_num << 8);


	/* Data setup is complete, now setup the transaction characteristics */

	/* 0x0228 HDMI_DDC_TRANS0
	   [23:16] CNT0 Byte count for first transaction (excluding the first
		byte, which is usually the address).
	   [13] STOP0 Determines whether a stop bit will be sent after the first
		transaction
		* 0: NO STOP
		* 1: STOP
	   [12] START0 Determines whether a start bit will be sent before the
		first transaction
		* 0: NO START
		* 1: START
	   [8] STOP_ON_NACK0 Determines whether the current transfer will stop
		if a NACK is received during the first transaction (current
		transaction always stops).
		* 0: STOP CURRENT TRANSACTION, GO TO NEXT TRANSACTION
		* 1: STOP ALL TRANSACTIONS, SEND STOP BIT
	   [0] RW0 Read/write indicator for first transaction - set to 0 for
		write, 1 for read. This bit only controls HDMI_DDC behaviour -
		the R/W bit in the transaction is programmed into the DDC buffer
		as the LSB of the address byte.
		* 0: WRITE
		* 1: READ */

	/* 4. Write to HDMI_I2C_TRANSACTION0 with the following fields set in
	      order to handle characteristics of portion #1 and portion #2
	 *    RW0 = 0x0 (write)
	 *    START0 = 0x1 (insert START bit)
	 *    STOP0 = 0x0 (do NOT insert STOP bit)
	 *    CNT0 = 0x1 (single byte transaction excluding address) */
	HDMI_OUTP_ND(0x0228, (1 << 12) | (1 << 16));

	/* 0x022C HDMI_DDC_TRANS1
	  [23:16] CNT1 Byte count for second transaction (excluding the first
		byte, which is usually the address).
	  [13] STOP1 Determines whether a stop bit will be sent after the second
		transaction
		* 0: NO STOP
		* 1: STOP
	  [12] START1 Determines whether a start bit will be sent before the
		second transaction
		* 0: NO START
		* 1: START
	  [8] STOP_ON_NACK1 Determines whether the current transfer will stop if
		a NACK is received during the second transaction (current
		transaction always stops).
		* 0: STOP CURRENT TRANSACTION, GO TO NEXT TRANSACTION
		* 1: STOP ALL TRANSACTIONS, SEND STOP BIT
	  [0] RW1 Read/write indicator for second transaction - set to 0 for
		write, 1 for read. This bit only controls HDMI_DDC behaviour -
		the R/W bit in the transaction is programmed into the DDC buffer
		as the LSB of the address byte.
		* 0: WRITE
		* 1: READ */

	/* 5. Write to HDMI_I2C_TRANSACTION1 with the following fields set in
	      order to handle characteristics of portion #3
	 *    RW1 = 0x1 (read)
	 *    START1 = 0x1 (insert START bit)
	 *    STOP1 = 0x1 (insert STOP bit)
	 *    CNT1 = data_len   (it's 128 (0x80) for a blk read) */
	HDMI_OUTP_ND(0x022C, (1 << 12) | (0 << 13));

	/* Trigger the I2C transfer */
	/* 0x020C HDMI_DDC_CTRL
	   [21:20] TRANSACTION_CNT
		Number of transactions to be done in current transfer.
		* 0x0: transaction0 only
		* 0x1: transaction0, transaction1
		* 0x2: transaction0, transaction1, transaction2
		* 0x3: transaction0, transaction1, transaction2, transaction3
	   [3] SW_STATUS_RESET
		Write 1 to reset HDMI_DDC_SW_STATUS flags, will reset SW_DONE,
		ABORTED, TIMEOUT, SW_INTERRUPTED, BUFFER_OVERFLOW,
		STOPPED_ON_NACK, NACK0, NACK1, NACK2, NACK3
	   [2] SEND_RESET Set to 1 to send reset sequence (9 clocks with no
		data) at start of transfer.  This sequence is sent after GO is
		written to 1, before the first transaction only.
	   [1] SOFT_RESET Write 1 to reset DDC controller
	   [0] GO WRITE ONLY. Write 1 to start DDC transfer. */

	/* 6. Write to HDMI_I2C_CONTROL to kick off the hardware.
	 *    Note that NOTHING has been transmitted on the DDC lines up to this
	 *    point.
	 *    TRANSACTION_CNT = 0x2 (execute transaction0 followed by
	 *    transaction1)
	 *    GO = 0x1 (kicks off hardware) */
	INIT_COMPLETION(hdmi_msm_state->ddc_sw_done);
	HDMI_OUTP_ND(0x020C, (1 << 0) | (1 << 20));

	time_out_count = wait_for_completion_interruptible_timeout(
		&hdmi_msm_state->ddc_sw_done, HZ/2);
	HDMI_OUTP_ND(0x0214, 0x2);
	if (!time_out_count) {
		if (retry-- > 0) {
			DEV_ERR("%s: failed timout, retry=%d\n", __func__,
				retry);
			goto again;
		}
		status = -ETIMEDOUT;
		DEV_ERR("%s: timedout(7), DDC SW Status=%08x, HW "
			"Status=%08x, Int Ctrl=%08x\n", __func__,
			HDMI_INP(0x0218), HDMI_INP(0x021C), HDMI_INP(0x0214));
		goto error;
	}

	/* Read DDC status */
	reg_val = HDMI_INP_ND(0x0218);
	reg_val &= 0x00001000 | 0x00002000 | 0x00004000 | 0x00008000;

	/* Check if any NACK occurred */
	if (reg_val) {
		HDMI_OUTP_ND(0x020C, BIT(3)); /* SW_STATUS_RESET */
		if (retry == 1)
			HDMI_OUTP_ND(0x020C, BIT(1)); /* SOFT_RESET */
		if (retry-- > 0) {
			DEV_DBG("%s(%s): 1_failed NACK=0x%08x, retry=%d, "
				"\n", __func__, what,
				reg_val, retry);
			goto again;
		}
		status = -EIO;
		if (log_retry_fail)
			DEV_DBG("%s(%s): 2_failed NACK=0x%08x,  "
				"\n", __func__, what,
				reg_val);
		goto error;
	}

	DEV_DBG("%s[%s] success\n", __func__, what);

error:
	return status;
}
static int hdmi_msm_ddc_read_edid_seg(uint32 dev_addr, uint32 offset,
	uint8 *data_buf, uint32 data_len, uint32 request_len, int retry,
	const char *what)
{
	uint32 reg_val, ndx;
	int status = 0;
	uint32 time_out_count;
	int log_retry_fail = retry != 1;
	int seg_addr = 0x60, seg_num = 0x01;

	if (NULL == data_buf) {
		status = -EINVAL;
		DEV_ERR("%s: invalid input paramter\n", __func__);
		goto error;
	}

again:
	status = hdmi_msm_ddc_clear_irq(what);
	if (status)
		goto error;

	/* Ensure Device Address has LSB set to 0 to indicate Slave addr read */
	dev_addr &= 0xFE;

	/* 0x0238 HDMI_DDC_DATA
	   [31] INDEX_WRITE WRITE ONLY. To write index field, set this bit to
		1 while writing HDMI_DDC_DATA.
	   [23:16] INDEX Use to set index into DDC buffer for next read or
		current write, or to read index of current read or next write.
		Writable only when INDEX_WRITE=1.
	   [15:8] DATA Use to fill or read the DDC buffer
	   [0] DATA_RW Select whether buffer access will be a read or write.
		For writes, address auto-increments on write to HDMI_DDC_DATA.
		For reads, address autoincrements on reads to HDMI_DDC_DATA.
		* 0: Write
		* 1: Read */

	/* 1. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #1
	 *    DATA_RW = 0x0 (write)
	 *    DATA = linkAddress (primary link address and writing)
	 *    INDEX = 0x0 (initial offset into buffer)
	 *    INDEX_WRITE = 0x1 (setting initial offset) */
	HDMI_OUTP_ND(0x0238, (0x1UL << 31) | (seg_addr << 8));

	/* 2. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #2
	 *    DATA_RW = 0x0 (write)
	 *    DATA = offsetAddress
	 *    INDEX = 0x0
	 *    INDEX_WRITE = 0x0 (auto-increment by hardware) */
	HDMI_OUTP_ND(0x0238, seg_num << 8);

	/* 3. Write to HDMI_I2C_DATA with the following fields set in order to
	 *    handle portion #3
	 *    DATA_RW = 0x0 (write)
	 *    DATA = linkAddress + 1 (primary link address 0x74 and reading)
	 *    INDEX = 0x0
	 *    INDEX_WRITE = 0x0 (auto-increment by hardware) */
	HDMI_OUTP_ND(0x0238, dev_addr << 8);
	HDMI_OUTP_ND(0x0238, offset << 8);
	HDMI_OUTP_ND(0x0238, (dev_addr | 1) << 8);

	/* Data setup is complete, now setup the transaction characteristics */

	/* 0x0228 HDMI_DDC_TRANS0
	   [23:16] CNT0 Byte count for first transaction (excluding the first
		byte, which is usually the address).
	   [13] STOP0 Determines whether a stop bit will be sent after the first
		transaction
		* 0: NO STOP
		* 1: STOP
	   [12] START0 Determines whether a start bit will be sent before the
		first transaction
		* 0: NO START
		* 1: START
	   [8] STOP_ON_NACK0 Determines whether the current transfer will stop
		if a NACK is received during the first transaction (current
		transaction always stops).
		* 0: STOP CURRENT TRANSACTION, GO TO NEXT TRANSACTION
		* 1: STOP ALL TRANSACTIONS, SEND STOP BIT
	   [0] RW0 Read/write indicator for first transaction - set to 0 for
		write, 1 for read. This bit only controls HDMI_DDC behaviour -
		the R/W bit in the transaction is programmed into the DDC buffer
		as the LSB of the address byte.
		* 0: WRITE
		* 1: READ */

	/* 4. Write to HDMI_I2C_TRANSACTION0 with the following fields set in
	      order to handle characteristics of portion #1 and portion #2
	 *    RW0 = 0x0 (write)
	 *    START0 = 0x1 (insert START bit)
	 *    STOP0 = 0x0 (do NOT insert STOP bit)
	 *    CNT0 = 0x1 (single byte transaction excluding address) */
	HDMI_OUTP_ND(0x0228, (1 << 12) | (1 << 16));

	/* 0x022C HDMI_DDC_TRANS1
	  [23:16] CNT1 Byte count for second transaction (excluding the first
		byte, which is usually the address).
	  [13] STOP1 Determines whether a stop bit will be sent after the second
		transaction
		* 0: NO STOP
		* 1: STOP
	  [12] START1 Determines whether a start bit will be sent before the
		second transaction
		* 0: NO START
		* 1: START
	  [8] STOP_ON_NACK1 Determines whether the current transfer will stop if
		a NACK is received during the second transaction (current
		transaction always stops).
		* 0: STOP CURRENT TRANSACTION, GO TO NEXT TRANSACTION
		* 1: STOP ALL TRANSACTIONS, SEND STOP BIT
	  [0] RW1 Read/write indicator for second transaction - set to 0 for
		write, 1 for read. This bit only controls HDMI_DDC behaviour -
		the R/W bit in the transaction is programmed into the DDC buffer
		as the LSB of the address byte.
		* 0: WRITE
		* 1: READ */

	/* 5. Write to HDMI_I2C_TRANSACTION1 with the following fields set in
	      order to handle characteristics of portion #3
	 *    RW1 = 0x1 (read)
	 *    START1 = 0x1 (insert START bit)
	 *    STOP1 = 0x1 (insert STOP bit)
	 *    CNT1 = data_len   (it's 128 (0x80) for a blk read) */
	HDMI_OUTP_ND(0x022C, (1 << 12) | (1 << 16));

	/* 0x022C HDMI_DDC_TRANS2
	  [23:16] CNT1 Byte count for second transaction (excluding the first
		byte, which is usually the address).
	  [13] STOP1 Determines whether a stop bit will be sent after the second
		transaction
		* 0: NO STOP
		* 1: STOP
	  [12] START1 Determines whether a start bit will be sent before the
		second transaction
		* 0: NO START
		* 1: START
	  [8] STOP_ON_NACK1 Determines whether the current transfer will stop if
		a NACK is received during the second transaction (current
		transaction always stops).
		* 0: STOP CURRENT TRANSACTION, GO TO NEXT TRANSACTION
		* 1: STOP ALL TRANSACTIONS, SEND STOP BIT
	  [0] RW1 Read/write indicator for second transaction - set to 0 for
		write, 1 for read. This bit only controls HDMI_DDC behaviour -
		the R/W bit in the transaction is programmed into the DDC buffer
		as the LSB of the address byte.
		* 0: WRITE
		* 1: READ */

	/* 5. Write to HDMI_I2C_TRANSACTION1 with the following fields set in
	      order to handle characteristics of portion #3
	 *    RW1 = 0x1 (read)
	 *    START1 = 0x1 (insert START bit)
	 *    STOP1 = 0x1 (insert STOP bit)
	 *    CNT1 = data_len   (it's 128 (0x80) for a blk read) */
	HDMI_OUTP_ND(0x0230, 1 | (1 << 12) | (1 << 13) | (request_len << 16));

	/* Trigger the I2C transfer */
	/* 0x020C HDMI_DDC_CTRL
	   [21:20] TRANSACTION_CNT
		Number of transactions to be done in current transfer.
		* 0x0: transaction0 only
		* 0x1: transaction0, transaction1
		* 0x2: transaction0, transaction1, transaction2
		* 0x3: transaction0, transaction1, transaction2, transaction3
	   [3] SW_STATUS_RESET
		Write 1 to reset HDMI_DDC_SW_STATUS flags, will reset SW_DONE,
		ABORTED, TIMEOUT, SW_INTERRUPTED, BUFFER_OVERFLOW,
		STOPPED_ON_NACK, NACK0, NACK1, NACK2, NACK3
	   [2] SEND_RESET Set to 1 to send reset sequence (9 clocks with no
		data) at start of transfer.  This sequence is sent after GO is
		written to 1, before the first transaction only.
	   [1] SOFT_RESET Write 1 to reset DDC controller
	   [0] GO WRITE ONLY. Write 1 to start DDC transfer. */

	/* 6. Write to HDMI_I2C_CONTROL to kick off the hardware.
	 *    Note that NOTHING has been transmitted on the DDC lines up to this
	 *    point.
	 *    TRANSACTION_CNT = 0x2 (execute transaction0 followed by
	 *    transaction1)
	 *    GO = 0x1 (kicks off hardware) */
	INIT_COMPLETION(hdmi_msm_state->ddc_sw_done);
	HDMI_OUTP_ND(0x020C, (1 << 0) | (2 << 20));

	time_out_count = wait_for_completion_interruptible_timeout(
		&hdmi_msm_state->ddc_sw_done, HZ/2);
	HDMI_OUTP_ND(0x0214, 0x2);
	if (!time_out_count) {
		if (retry-- > 0) {
			DEV_INFO("%s: failed timout, retry=%d\n", __func__,
				retry);
			goto again;
		}
		status = -ETIMEDOUT;
		DEV_ERR("%s: timedout(7), DDC SW Status=%08x, HW "
			"Status=%08x, Int Ctrl=%08x\n", __func__,
			HDMI_INP(0x0218), HDMI_INP(0x021C), HDMI_INP(0x0214));
		goto error;
	}

	/* Read DDC status */
	reg_val = HDMI_INP_ND(0x0218);
	reg_val &= 0x00001000 | 0x00002000 | 0x00004000 | 0x00008000;

	/* Check if any NACK occurred */
	if (reg_val) {
		HDMI_OUTP_ND(0x020C, BIT(3)); /* SW_STATUS_RESET */
		if (retry == 1)
			HDMI_OUTP_ND(0x020C, BIT(1)); /* SOFT_RESET */
		if (retry-- > 0) {
			DEV_DBG("%s(%s): failed NACK=0x%08x, retry=%d, "
				"dev-addr=0x%02x, offset=0x%02x, "
				"length=%d\n", __func__, what,
				reg_val, retry, dev_addr,
				offset, data_len);
			goto again;
		}
		status = -EIO;
		if (log_retry_fail)
			DEV_ERR("%s(%s): failed NACK=0x%08x, dev-addr=0x%02x, "
				"offset=0x%02x, length=%d\n", __func__, what,
				reg_val, dev_addr, offset, data_len);
		goto error;
	}

	/* 0x0238 HDMI_DDC_DATA
	   [31] INDEX_WRITE WRITE ONLY. To write index field, set this bit to 1
		while writing HDMI_DDC_DATA.
	   [23:16] INDEX Use to set index into DDC buffer for next read or
		current write, or to read index of current read or next write.
		Writable only when INDEX_WRITE=1.
	   [15:8] DATA Use to fill or read the DDC buffer
	   [0] DATA_RW Select whether buffer access will be a read or write.
		For writes, address auto-increments on write to HDMI_DDC_DATA.
		For reads, address autoincrements on reads to HDMI_DDC_DATA.
		* 0: Write
		* 1: Read */

	/* 8. ALL data is now available and waiting in the DDC buffer.
	 *    Read HDMI_I2C_DATA with the following fields set
	 *    RW = 0x1 (read)
	 *    DATA = BCAPS (this is field where data is pulled from)
	 *    INDEX = 0x3 (where the data has been placed in buffer by hardware)
	 *    INDEX_WRITE = 0x1 (explicitly define offset) */
	/* Write this data to DDC buffer */
	HDMI_OUTP_ND(0x0238, 0x1 | (3 << 16) | (1 << 31));

	/* Discard first byte */
	HDMI_INP_ND(0x0238);
	for (ndx = 0; ndx < data_len; ++ndx) {
		reg_val = HDMI_INP_ND(0x0238);
		data_buf[ndx] = (uint8) ((reg_val & 0x0000FF00) >> 8);
	}

	DEV_DBG("%s[%s] success\n", __func__, what);

error:
	return status;
}

static int hdmi_msm_ddc_read(uint32 dev_addr, uint32 offset, uint8 *data_buf,
	uint32 data_len, int retry, const char *what, boolean no_align)
{
	int ret = hdmi_msm_ddc_read_retry(dev_addr, offset, data_buf, data_len,
		data_len, retry, what);
	if (!ret)
		return 0;
	if (no_align) {
		return hdmi_msm_ddc_read_retry(dev_addr, offset, data_buf,
			data_len, data_len, retry, what);
	} else {
		return hdmi_msm_ddc_read_retry(dev_addr, offset, data_buf,
			data_len, 32 * ((data_len + 31) / 32), retry, what);
	}
}

static int hdmi_msm_read_edid_block_seg(int block, uint8 *edid_buf)
{
	int i,ret =0;
	int block_size = 0x80;
	uint32 ndx;
	uint8 seg_num[2]={0x00,0x01};

	for(i=0; i < 128; i++){
		/*Set Segment*/
		ret = hdmi_msm_ddc_set_seg(block_size, seg_num[1], "seg");
		//supprot read offset of 2,3 block
		ret = hdmi_msm_ddc_read(0xA0, i+block_size*(block-2), edid_buf, 1, 1, "EDID", TRUE);
		edid_buf++;
	}

	return 0;
}

static int hdmi_msm_read_edid_block(int block, uint8 *edid_buf)
{
	int i, rc = 0;
	int block_size = 0x80;
	uint32 ndx;

	if(block < 2){
		do {
			DEV_DBG("EDID: reading block(%d) with block-size=%d\n",
				block, block_size);
			for (i = 0; i < 0x80; i += block_size) {
				/*Read EDID twice with 32bit alighnment too */
				if (block < 2) {
					rc = hdmi_msm_ddc_read(0xA0, block*0x80 + i,
						edid_buf+i, block_size, 1,
						"EDID", FALSE);
				} else {
					rc = hdmi_msm_ddc_read_edid_seg(0xA0,
					block*0x80 + i, edid_buf+i, block_size,
					block_size, 1, "EDID");
				}
				if (rc)
					break;
			}

			block_size /= 2;
		} while (rc && (block_size >= 16));
	}else   //EDID_SAMSUNG_FEATURE 
		// becuase of NAK erro when using a Qualcom DDC of segmanet 2.3block access
		rc = hdmi_msm_read_edid_block_seg(block, edid_buf);

#ifdef CONFIG_HDMI_DEBUG_MSG
	const u8 *b = edid_buf;
	printk("****EDID : [%d] block****\n",block);
	for (ndx = 0; ndx < 0x80; ndx += 16)
		printk("EDID[%02x-%02x] %02x %02x %02x %02x  "
			"%02x %02x %02x %02x    %02x %02x %02x %02x  "
			"%02x %02x %02x %02x\n", ndx, ndx+15,
			b[ndx+0], b[ndx+1], b[ndx+2], b[ndx+3],
			b[ndx+4], b[ndx+5], b[ndx+6], b[ndx+7],
			b[ndx+8], b[ndx+9], b[ndx+10], b[ndx+11],
			b[ndx+12], b[ndx+13], b[ndx+14], b[ndx+15]);
#endif

	return rc;
}

static int hdmi_msm_read_edid(void)
{
	int status;

	msm_hdmi_init_ddc();
	/* Looks like we need to turn on HDMI engine before any
	 * DDC transaction */
	if (!hdmi_msm_is_power_on()) {
		DEV_ERR("%s: failed: HDMI power is off", __func__);
		status = -ENXIO;
		goto error;
	}

	external_common_state->read_edid_block = hdmi_msm_read_edid_block;
	status = hdmi_common_read_edid();
	if (!status)
		DEV_DBG("EDID: successfully read\n");

error:
	return status;
}

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
static void hdcp_auth_info(uint32 auth_info)
{
	switch (auth_info) {
	case 0:
		DEV_INFO("%s: None", __func__);
		break;
	case 1:
		DEV_INFO("%s: Software Disabled Authentication", __func__);
		break;
	case 2:
		DEV_INFO("%s: An Written", __func__);
		break;
	case 3:
		DEV_INFO("%s: Invalid Aksv", __func__);
		break;
	case 4:
		DEV_INFO("%s: Invalid Bksv", __func__);
		break;
	case 5:
		DEV_INFO("%s: RI Mismatch (including RO)", __func__);
		break;
	case 6:
		DEV_INFO("%s: consecutive Pj Mismatches", __func__);
		break;
	case 7:
		DEV_INFO("%s: HPD Disconnect", __func__);
		break;
	case 8:
	default:
		DEV_INFO("%s: Reserved", __func__);
		break;
	}
}

static void hdcp_key_state(uint32 key_state)
{
	switch (key_state) {
	case 0:
		DEV_WARN("%s: No HDCP Keys", __func__);
		break;
	case 1:
		DEV_WARN("%s: Not Checked", __func__);
		break;
	case 2:
		DEV_DBG("%s: Checking", __func__);
		break;
	case 3:
		DEV_DBG("%s: HDCP Keys Valid", __func__);
		break;
	case 4:
		DEV_WARN("%s: AKSV not valid", __func__);
		break;
	case 5:
		DEV_WARN("%s: Checksum Mismatch", __func__);
		break;
	case 6:
		DEV_DBG("%s: Production AKSV"
			"with ENABLE_USER_DEFINED_AN=1", __func__);
		break;
	case 7:
	default:
		DEV_INFO("%s: Reserved", __func__);
		break;
	}
}

static int hdmi_msm_count_one(uint8 *array, uint8 len)
{
	int i, j, count = 0;
	for (i = 0; i < len; i++)
		for (j = 0; j < 8; j++)
			count += (((array[i] >> j) & 0x1) ? 1 : 0);
	return count;
}

static void hdcp_deauthenticate(void)
{
	int hdcp_link_status = HDMI_INP(0x011C);

	external_common_state->hdcp_active = FALSE;
	/* 0x0130 HDCP_RESET
	  [0] LINK0_DEAUTHENTICATE */
	HDMI_OUTP(0x0130, 0x1);

	/* 0x0110 HDCP_CTRL
	  [8] ENCRYPTION_ENABLE
	  [0] ENABLE */
	/* encryption_enable = 0 | hdcp block enable = 1 */
	HDMI_OUTP(0x0110, 0x0);

	if (hdcp_link_status & 0x00000004)
		hdcp_auth_info((hdcp_link_status & 0x000000F0) >> 4);
}

static int hdcp_authentication_part1(void)
{
	int ret = 0;
	boolean is_match;
	boolean is_part1_done = FALSE;
	uint32 timeout_count;
	uint8 bcaps;
	uint8 aksv[5];
	uint32 qfprom_aksv_0, qfprom_aksv_1, link0_aksv_0, link0_aksv_1;
	uint8 bksv[5];
	uint32 link0_bksv_0, link0_bksv_1;
	uint8 an[8];
	uint32 link0_an_0, link0_an_1;
	uint32 hpd_int_status, hpd_int_ctrl;


	static uint8 buf[0xFF];
	memset(buf, 0, sizeof(buf));

	if (!is_part1_done) {
		is_part1_done = TRUE;

	/* Fetch aksv from QFprom, this info should be public. */
	qfprom_aksv_0 = inpdw(QFPROM_BASE + 0x000060D8);
	qfprom_aksv_1 = inpdw(QFPROM_BASE + 0x000060DC);

	/* copy an and aksv to byte arrays for transmission */
	aksv[0] =  qfprom_aksv_0        & 0xFF;
	aksv[1] = (qfprom_aksv_0 >> 8)  & 0xFF;
	aksv[2] = (qfprom_aksv_0 >> 16) & 0xFF;
	aksv[3] = (qfprom_aksv_0 >> 24) & 0xFF;
	aksv[4] =  qfprom_aksv_1        & 0xFF;
	/* check there are 20 ones in AKSV */
	if (hdmi_msm_count_one(aksv, 5) != 20) {
			DEV_ERR("HDCP: AKSV read from QFPROM doesn't have\
				20 1's and 20 0's, FAIL (AKSV=%02x%08x)\n",
			qfprom_aksv_1, qfprom_aksv_0);
		ret = -EINVAL;
		goto error;
	}
	DEV_DBG("HDCP: AKSV=%02x%08x\n", qfprom_aksv_1, qfprom_aksv_0);

	/* 0x0288 HDCP_SW_LOWER_AKSV
	     [31:0] LOWER_AKSV */
	/* 0x0284 HDCP_SW_UPPER_AKSV
	     [7:0] UPPER_AKSV */

	/* This is the lower 32 bits of the SW
	 * injected AKSV value(AKSV[31:0]) read
	 * from the EFUSE. It is needed for HDCP
	 * authentication and must be written
	 * before enabling HDCP. */
	HDMI_OUTP(0x0288, qfprom_aksv_0);
	HDMI_OUTP(0x0284, qfprom_aksv_1);

	msm_hdmi_init_ddc();

	/* Read Bksv 5 bytes at 0x00 in HDCP port */
	ret = hdmi_msm_ddc_read(0x74, 0x00, bksv, 5, 5, "Bksv", TRUE);
	if (ret) {
		DEV_ERR("%s(%d): Read BKSV failed", __func__, __LINE__);
		goto error;
	}
	/* check there are 20 ones in BKSV */
	if (hdmi_msm_count_one(bksv, 5) != 20) {
			DEV_ERR("HDCP: BKSV read from Sink doesn't have\
				20 1's and 20 0's, FAIL (BKSV=\
				%02x%02x%02x%02x%02x)\n",
				bksv[4], bksv[3], bksv[2], bksv[1], bksv[0]);
		ret = -EINVAL;
		goto error;
	}

	link0_bksv_0 = bksv[3];
	link0_bksv_0 = (link0_bksv_0 << 8) | bksv[2];
	link0_bksv_0 = (link0_bksv_0 << 8) | bksv[1];
	link0_bksv_0 = (link0_bksv_0 << 8) | bksv[0];
	link0_bksv_1 = bksv[4];
	DEV_DBG("HDCP: BKSV=%02x%08x\n", link0_bksv_1, link0_bksv_0);

	/* read Bcaps at 0x40 in HDCP Port */
	ret = hdmi_msm_ddc_read(0x74, 0x40, &bcaps, 1, 5, "Bcaps",
              TRUE);
	if (ret) {
		DEV_ERR("%s(%d): Read Bcaps failed", __func__,
              __LINE__);
		goto error;
	}
	DEV_DBG("HDCP: Bcaps=%02x\n", bcaps);

	/* HDCP setup prior to HDCP enabled */

	/* 0x0148 HDCP_RCVPORT_DATA4
	     [15:8] LINK0_AINFO
	      [7:0] LINK0_AKSV_1 */
	/* LINK0_AINFO	= 0x2 FEATURE 1.1 on.
	 *		= 0x0 FEATURE 1.1 off*/
	HDMI_OUTP(0x0148, 0x2 << 8);

	/* 0x012C HDCP_ENTROPY_CTRL0
	     [31:0] BITS_OF_INFLUENCE_0 */
	/* 0x025C HDCP_ENTROPY_CTRL1
	     [31:0] BITS_OF_INFLUENCE_1 */
	HDMI_OUTP(0x012C, 0xB1FFB0FF);
	HDMI_OUTP(0x025C, 0xF00DFACE);

	/* 0x0114 HDCP_DEBUG_CTRL
	     [2]	DEBUG_RNG_CIPHER
	   else default 0 */
	HDMI_OUTP(0x0114, HDMI_INP(0x0114) & 0xFFFFFFFB);

	/* 0x0110 HDCP_CTRL
	     [8] ENCRYPTION_ENABLE
	     [0] ENABLE */
	/* encryption_enable | enable  */
	HDMI_OUTP(0x0110, (1 << 8) | (1 << 0));

	/* 0x0118 HDCP_INT_CTRL
		 *    [2] AUTH_SUCCESS_MASK	[R/W]	Mask bit for\
		 *					HDCP Authentication
	 *		Success interrupt - set to 1 to enable interrupt
		 *
		 *    [6] AUTH_FAIL_MASK	[R/W]	Mask bit for HDCP
		 *					Authentication
	 *		Lost interrupt set to 1 to enable interrupt
		 *
		 *    [7] AUTH_FAIL_INFO_ACK	[W]	Acknwledge bit for HDCP
		 *		Auth Failure Info field - write 1 to clear
		 *
		 *   [10] DDC_XFER_REQ_MASK	[R/W]	Mask bit for HDCP\
		 *					DDC Transfer
	 *		Request interrupt - set to 1 to enable interrupt
		 *
		 *   [14] DDC_XFER_DONE_MASK	[R/W]	Mask bit for HDCP\
		 *					DDC Transfer
	 *		done interrupt - set to 1 to enable interrupt */
	/* enable all HDCP ints */
	HDMI_OUTP(0x0118, (1 << 2) | (1 << 6) | (1 << 7));

	/* 0x011C HDCP_LINK0_STATUS
	     [8] AN_0_READY
	     [9] AN_1_READY */
	/* wait for an0 and an1 ready bits to be set in LINK0_STATUS */
	timeout_count = 100;
	while (((HDMI_INP_ND(0x011C) & (0x3 << 8)) != (0x3 << 8))
			&& timeout_count--)
		msleep(20);
	if (!timeout_count) {
		ret = -ETIMEDOUT;
		DEV_ERR("%s(%d): timedout, An0=%d, An1=%d\n",
			__func__, __LINE__,
			(HDMI_INP_ND(0x011C) & BIT(8)) >> 8,
			(HDMI_INP_ND(0x011C) & BIT(9)) >> 9);
		goto error;
	}

	/* 0x0168 HDCP_RCVPORT_DATA12
	     [23:8] BSTATUS
	     [7:0] BCAPS */
	HDMI_OUTP(0x0168, bcaps);

	/* 0x014C HDCP_RCVPORT_DATA5
	     [31:0] LINK0_AN_0 */
	/* read an0 calculation */
	link0_an_0 = HDMI_INP(0x014C);

	/* 0x0150 HDCP_RCVPORT_DATA6
	     [31:0] LINK0_AN_1 */
	/* read an1 calculation */
	link0_an_1 = HDMI_INP(0x0150);

	/* three bits 28..30 */
	hdcp_key_state((HDMI_INP(0x011C) >> 28) & 0x7);

	/* 0x0144 HDCP_RCVPORT_DATA3
	     [31:0] LINK0_AKSV_0 public key
	   0x0148 HDCP_RCVPORT_DATA4
	     [15:8] LINK0_AINFO
	     [7:0]  LINK0_AKSV_1 public key */
	link0_aksv_0 = HDMI_INP(0x0144);
	link0_aksv_1 = HDMI_INP(0x0148);

	/* copy an and aksv to byte arrays for transmission */
	aksv[0] =  link0_aksv_0        & 0xFF;
	aksv[1] = (link0_aksv_0 >> 8)  & 0xFF;
	aksv[2] = (link0_aksv_0 >> 16) & 0xFF;
	aksv[3] = (link0_aksv_0 >> 24) & 0xFF;
	aksv[4] =  link0_aksv_1        & 0xFF;

	an[0] =  link0_an_0        & 0xFF;
	an[1] = (link0_an_0 >> 8)  & 0xFF;
	an[2] = (link0_an_0 >> 16) & 0xFF;
	an[3] = (link0_an_0 >> 24) & 0xFF;
	an[4] =  link0_an_1        & 0xFF;
	an[5] = (link0_an_1 >> 8)  & 0xFF;
	an[6] = (link0_an_1 >> 16) & 0xFF;
	an[7] = (link0_an_1 >> 24) & 0xFF;

	/* Write An 8 bytes to offset 0x18 */
	ret = hdmi_msm_ddc_write(0x74, 0x18, an, 8, "An");
	if (ret) {
		DEV_ERR("%s(%d): Write An failed", __func__, __LINE__);
		goto error;
	}

	/* Write Aksv 5 bytes to offset 0x10 */
	ret = hdmi_msm_ddc_write(0x74, 0x10, aksv, 5, "Aksv");
	if (ret) {
		DEV_ERR("%s(%d): Write Aksv failed", __func__,
                     __LINE__);
		goto error;
	}
	DEV_DBG("HDCP: Link0-AKSV=%02x%08x\n",
		link0_aksv_1 & 0xFF, link0_aksv_0);

	/* 0x0134 HDCP_RCVPORT_DATA0
	     [31:0] LINK0_BKSV_0 */
	HDMI_OUTP(0x0134, link0_bksv_0);
	/* 0x0138 HDCP_RCVPORT_DATA1
	     [31:0] LINK0_BKSV_1 */
	HDMI_OUTP(0x0138, link0_bksv_1);
		DEV_DBG("HDCP: Link0-BKSV=%02x%08x\n", link0_bksv_1,
		    link0_bksv_0);

		/* HDMI_HPD_INT_STATUS[0x0250] */
		hpd_int_status = HDMI_INP_ND(0x0250);
		/* HDMI_HPD_INT_CTRL[0x0254] */
		hpd_int_ctrl = HDMI_INP_ND(0x0254);
		DEV_DBG("[SR-DEUG]: HPD_INTR_CTRL=[%u] HPD_INTR_STATUS=[%u]\
		    before reading R0'\n", hpd_int_ctrl, hpd_int_status);

		/*
		* HDCP Compliace Test case 1B-01:
		* Wait here until all the ksv bytes have been
		* read from the KSV FIFO register.
		*/
		msleep(125);

	/* Reading R0' 2 bytes at offset 0x08 */
	ret = hdmi_msm_ddc_read(0x74, 0x08, buf, 2, 5, "RO'", TRUE);
	if (ret) {
		DEV_ERR("%s(%d): Read RO's failed", __func__,
                 __LINE__);
		goto error;
	}

	/* 0x013C HDCP_RCVPORT_DATA2_0
	     [15:0] LINK0_RI */
	HDMI_OUTP(0x013C, (((uint32)buf[1]) << 8) | buf[0]);
	DEV_DBG("HDCP: R0'=%02x%02x\n", buf[1], buf[0]);

	INIT_COMPLETION(hdmi_msm_state->hdcp_success_done);
	timeout_count = wait_for_completion_interruptible_timeout(
		&hdmi_msm_state->hdcp_success_done, HZ*2);

	if (!timeout_count) {
		ret = -ETIMEDOUT;
		is_match = HDMI_INP(0x011C) & BIT(12);
		DEV_ERR("%s(%d): timedout, Link0=<%s>\n", __func__, 
                      __LINE__,
			  is_match ? "RI_MATCH" : "No RI Match INTR in time");
		if (!is_match)
		goto error;
	}

	/* 0x011C HDCP_LINK0_STATUS
	     [12] RI_MATCHES	[0] MISMATCH, [1] MATCH
	     [0] AUTH_SUCCESS */
	/* Checking for RI, R0 Match */
	/* RI_MATCHES */
	if ((HDMI_INP(0x011C) & BIT(12)) != BIT(12)) {
		ret = -EINVAL;
		DEV_ERR("%s: HDCP_LINK0_STATUS[RI_MATCHES]: MISMATCH\n",
			__func__);
		goto error;
	}

	DEV_INFO("HDCP: authentication part I, successful\n");
		is_part1_done = FALSE;
	return 0;
error:
		DEV_ERR("[%s]: HDCP Reauthentication\n", __func__);
		is_part1_done = FALSE;
	return ret;
	} else {
		return 1;
	}
}

static int hdmi_msm_transfer_v_h(void)
{
	/* Read V'.HO 4 Byte at offset 0x20 */
	char what[20];
	int ret;
	uint8 buf[4];

	snprintf(what, sizeof(what), "V' H0");
	ret = hdmi_msm_ddc_read(0x74, 0x20, buf, 4, 5, what, TRUE);
	if (ret) {
		DEV_ERR("%s: Read %s failed", __func__, what);
		return ret;
	}
	DEV_DBG("buf[0]= %x , buf[1] = %x , buf[2] = %x , buf[3] = %x\n ",
			buf[0] , buf[1] , buf[2] , buf[3]);

	/* 0x0154 HDCP_RCVPORT_DATA7
	   [31:0] V_HO */
	HDMI_OUTP(0x0154 ,
		(buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0]));

	snprintf(what, sizeof(what), "V' H1");
	ret = hdmi_msm_ddc_read(0x74, 0x24, buf, 4, 5, what, TRUE);
	if (ret) {
		DEV_ERR("%s: Read %s failed", __func__, what);
		return ret;
	}
	DEV_DBG("buf[0]= %x , buf[1] = %x , buf[2] = %x , buf[3] = %x\n ",
			buf[0] , buf[1] , buf[2] , buf[3]);

	/* 0x0158 HDCP_RCVPORT_ DATA8
	   [31:0] V_H1 */
	HDMI_OUTP(0x0158,
		(buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0]));


	snprintf(what, sizeof(what), "V' H2");
	ret = hdmi_msm_ddc_read(0x74, 0x28, buf, 4, 5, what, TRUE);
	if (ret) {
		DEV_ERR("%s: Read %s failed", __func__, what);
		return ret;
	}
	DEV_DBG("buf[0]= %x , buf[1] = %x , buf[2] = %x , buf[3] = %x\n ",
			buf[0] , buf[1] , buf[2] , buf[3]);

	/* 0x015c HDCP_RCVPORT_DATA9
	   [31:0] V_H2 */
	HDMI_OUTP(0x015c ,
		(buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0]));

	snprintf(what, sizeof(what), "V' H3");
	ret = hdmi_msm_ddc_read(0x74, 0x2c, buf, 4, 5, what, TRUE);
	if (ret) {
		DEV_ERR("%s: Read %s failed", __func__, what);
		return ret;
	}
	DEV_DBG("buf[0]= %x , buf[1] = %x , buf[2] = %x , buf[3] = %x\n ",
			buf[0] , buf[1] , buf[2] , buf[3]);

	/* 0x0160 HDCP_RCVPORT_DATA10
	   [31:0] V_H3 */
	HDMI_OUTP(0x0160,
		(buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0]));

	snprintf(what, sizeof(what), "V' H4");
	ret = hdmi_msm_ddc_read(0x74, 0x30, buf, 4, 5, what, TRUE);
	if (ret) {
		DEV_ERR("%s: Read %s failed", __func__, what);
		return ret;
	}
	DEV_DBG("buf[0]= %x , buf[1] = %x , buf[2] = %x , buf[3] = %x\n ",
			buf[0] , buf[1] , buf[2] , buf[3]);
	/* 0x0164 HDCP_RCVPORT_DATA11
	   [31:0] V_H4 */
	HDMI_OUTP(0x0164,
		(buf[3] << 24 | buf[2] << 16 | buf[1] << 8 | buf[0]));

	return 0;
}

static int hdcp_authentication_part2(void)
{
	int ret = 0;
	uint32 timeout_count;
	int i = 0;
	int cnt = 0;
	uint bstatus;
	uint8 bcaps;
	uint32 down_stream_devices;
	uint32 ksv_bytes;

	static uint8 buf[0xFF];
	static uint8 kvs_fifo[5 * 127];

	boolean max_devs_exceeded = 0;
	boolean max_cascade_exceeded = 0;

	boolean ksv_done = FALSE;

	memset(buf, 0, sizeof(buf));
	memset(kvs_fifo, 0, sizeof(kvs_fifo));

	/* wait until READY bit is set in bcaps */
	timeout_count = 50;
	do {
		timeout_count--;
		/* read bcaps 1 Byte at offset 0x40 */
		ret = hdmi_msm_ddc_read(0x74, 0x40, &bcaps, 1, 1,
						"Bcaps", FALSE);
		if (ret) {
			DEV_ERR("%s(%d): Read Bcaps failed", __func__,
				__LINE__);
			goto error;
		}
		msleep(100);
	} while ((0 == (bcaps & 0x20)) && timeout_count); /* READY (Bit 5) */
	if (!timeout_count) {
		ret = -ETIMEDOUT;
		DEV_ERR("%s:timedout(1)", __func__);
		goto error;
	}

	/* read bstatus 2 bytes at offset 0x41 */

	ret = hdmi_msm_ddc_read(0x74, 0x41, buf, 2, 5, "Bstatus", FALSE);
	if (ret) {
		DEV_ERR("%s(%d): Read Bstatus failed", __func__, __LINE__);
		goto error;
	}
	bstatus = buf[1];
	bstatus = (bstatus << 8) | buf[0];
	/* 0x0168 DCP_RCVPORT_DATA12
	   [7:0] BCAPS
	   [23:8 BSTATUS */
	HDMI_OUTP(0x0168, bcaps | (bstatus << 8));
	/* BSTATUS [6:0] DEVICE_COUNT Number of HDMI device attached to repeater
	 * - see HDCP spec */
	down_stream_devices = bstatus & 0x7F;

	if (down_stream_devices == 0x0) {
		/* There isn't any devices attaced to the Repeater */
		DEV_ERR("%s: there isn't any devices attached to the "
			"Repeater\n", __func__);
		ret = -EINVAL;
		goto error;
	}

	/*
	 * HDCP Compliance 1B-05:
	 * Check if no. of devices connected to repeater
	 * exceed max_devices_connected from bit 7 of Bstatus.
	 */
	max_devs_exceeded = (bstatus & 0x80) >> 7;
	if (max_devs_exceeded == 0x01) {
		DEV_ERR("%s: Number of devs connected to repeater "
			"exceeds max_devs\n", __func__);
		ret = -EINVAL;
		goto hdcp_error;
	}

	/*
	 * HDCP Compliance 1B-06:
	 * Check if no. of cascade connected to repeater
	 * exceed max_cascade_connected from bit 11 of Bstatus.
	 */
	max_cascade_exceeded = (bstatus & 0x800) >> 11;
	if (max_cascade_exceeded == 0x01) {
		DEV_ERR("%s: Number of cascade connected to repeater "
			"exceeds max_cascade\n", __func__);
		ret = -EINVAL;
		goto hdcp_error;
	}

	/* Read KSV FIFO over DDC
	 * Key Slection vector FIFO
	 * Used to pull downstream KSVs from HDCP Repeaters.
	 * All bytes (DEVICE_COUNT * 5) must be read in a single,
	 *   auto incrementing access.
	 * All bytes read as 0x00 for HDCP Receivers that are not
	 *   HDCP Repeaters (REPEATER == 0). */
	ksv_bytes = 5 * down_stream_devices;
	/* Reading KSV FIFO / KSV FIFO */
	ksv_done = FALSE;

	ret = hdmi_msm_ddc_read(0x74, 0x43, kvs_fifo, ksv_bytes, 5,
							"KSV FIFO", TRUE);
	do {
			if (ret) {
			DEV_ERR("%s(%d): Read KSV FIFO failed",
			    __func__, __LINE__);
			/*
			* HDCP Compliace Test case 1B-01:
			* Wait here until all the ksv bytes have been
			* read from the KSV FIFO register.
			*/
			msleep(25);
		} else {
			ksv_done = TRUE;
	}
		cnt++;
	} while (!ksv_done && cnt != 20);

	if (ksv_done == FALSE)
		goto error;

	ret = hdmi_msm_transfer_v_h();
	if (ret)
		goto error;

	/* Next: Write KSV FIFO to HDCP_SHA_DATA.
	 * This is done 1 byte at time starting with the LSB.
	 * On the very last byte write,
	 * the HDCP_SHA_DATA_DONE bit[0]
	 */

	/* 0x023C HDCP_SHA_CTRL
	   [0] RESET	[0] Enable, [1] Reset
	   [4] SELECT	[0] DIGA_HDCP, [1] DIGB_HDCP */
	/* reset SHA engine */
	HDMI_OUTP(0x023C, 1);
	/* enable SHA engine, SEL=DIGA_HDCP */
	HDMI_OUTP(0x023C, 0);

	for (i = 0; i < ksv_bytes - 1; i++) {
		/* Write KSV byte and do not set DONE bit[0] */
		HDMI_OUTP_ND(0x0244, kvs_fifo[i] << 16);
	}
	/* Write l to DONE bit[0] */
	HDMI_OUTP_ND(0x0244, (kvs_fifo[ksv_bytes - 1] << 16) | 0x1);

	/* 0x0240 HDCP_SHA_STATUS
	   [4] COMP_DONE */
	/* Now wait for HDCP_SHA_COMP_DONE */
	timeout_count = 100;
	while ((0x10 != (HDMI_INP_ND(0x0240) & 0x10)) && timeout_count--)
		msleep(20);
	if (!timeout_count) {
		ret = -ETIMEDOUT;
		DEV_ERR("%s(%d): timedout", __func__, __LINE__);
		goto error;
	}

	/* 0x011C HDCP_LINK0_STATUS
	   [20] V_MATCHES */
	timeout_count = 100;
	while (((HDMI_INP_ND(0x011C) & (1 << 20)) != (1 << 20))
			&& timeout_count--)
		msleep(20);
	if (!timeout_count) {
		ret = -ETIMEDOUT;
		DEV_ERR("%s(%d): timedout", __func__, __LINE__);
		goto error;
	}

	DEV_INFO("HDCP: authentication part II, successful\n");

hdcp_error:
error:
	return ret;
}

static int hdcp_authentication_part3(uint32 found_repeater)
{
	int ret = 0;
	int poll = 3000;
	while (poll) {
		/* 0x011C HDCP_LINK0_STATUS
		    [30:28]  KEYS_STATE = 3 = "Valid"
		    [24] RO_COMPUTATION_DONE	[0] Not Done, [1] Done
		    [20] V_MATCHES		[0] Mismtach, [1] Match
		    [12] RI_MATCHES		[0] Mismatch, [1] Match
		    [0] AUTH_SUCCESS */
		if (HDMI_INP_ND(0x011C) != (0x31001001 |
					(found_repeater << 20))) {
			DEV_ERR("HDCP: autentication part III, FAILED, "
				"Link Status=%08x\n", HDMI_INP(0x011C));
			ret = -EINVAL;
			goto error;
		}
		poll--;
	}

	DEV_INFO("HDCP: authentication part III, successful\n");

error:
	return ret;
}

static void hdmi_msm_hdcp_enable(void)
{
	int ret = 0;
	uint8 bcaps;
	uint32 found_repeater = 0x0;
	char *envp[2];

	if (!hdmi_msm_has_hdcp())
		return;

	mutex_lock(&hdmi_msm_state_mutex);
	hdmi_msm_state->hdcp_activating = TRUE;
	mutex_unlock(&hdmi_msm_state_mutex);

	fill_black_screen();

	mutex_lock(&hdcp_auth_state_mutex);
	/*
	 * Initialize this to zero here to make
	 * sure HPD has not happened yet
	 */
	hdmi_msm_state->hpd_during_auth = FALSE;
	/* This flag prevents other threads from re-authenticating
	* after we've just authenticated (i.e., finished part3)
	* We probably need to protect this in a mutex lock */
	hdmi_msm_state->full_auth_done = FALSE;
	mutex_unlock(&hdcp_auth_state_mutex);

	/* PART I Authentication*/
	ret = hdcp_authentication_part1();
	if (ret)
		goto error;

	/* PART II Authentication*/
	/* read Bcaps at 0x40 in HDCP Port */
	ret = hdmi_msm_ddc_read(0x74, 0x40, &bcaps, 1, 5, "Bcaps", FALSE);
	if (ret) {
		DEV_ERR("%s(%d): Read Bcaps failed\n", __func__, __LINE__);
		goto error;
	}
	DEV_DBG("HDCP: Bcaps=0x%02x (%s)\n", bcaps,
		(bcaps & BIT(6)) ? "repeater" : "no repeater");

	/* if REPEATER (Bit 6), perform Part2 Authentication */
	if (bcaps & BIT(6)) {
		found_repeater = 0x1;
		ret = hdcp_authentication_part2();
		if (ret)
			goto error;
	} else
		DEV_INFO("HDCP: authentication part II skipped, no repeater\n");

	/* PART III Authentication*/
	ret = hdcp_authentication_part3(found_repeater);
	if (ret)
		goto error;

	unfill_black_screen();

	external_common_state->hdcp_active = TRUE;
	mutex_lock(&hdmi_msm_state_mutex);
	hdmi_msm_state->hdcp_activating = FALSE;
	mutex_unlock(&hdmi_msm_state_mutex);

	mutex_lock(&hdcp_auth_state_mutex);
	/*
	 * This flag prevents other threads from re-authenticating
	 * after we've just authenticated (i.e., finished part3)
	 */
	hdmi_msm_state->full_auth_done = TRUE;
	mutex_unlock(&hdcp_auth_state_mutex);

	if (!hdmi_msm_is_dvi_mode()) {
		DEV_INFO("HDMI HPD: sense : send HDCP_PASS\n");
		envp[0] = "HDCP_STATE=PASS";
		envp[1] = NULL;
		kobject_uevent_env(external_common_state->uevent_kobj,
			KOBJ_CHANGE, envp);
	}
	return;

error:
	mutex_lock(&hdmi_msm_state_mutex);
	hdmi_msm_state->hdcp_activating = FALSE;
	mutex_unlock(&hdmi_msm_state_mutex);
	if (hdmi_msm_state->hpd_during_auth) {
		DEV_WARN("Calling Deauthentication: HPD occured during\
		    authentication  from [%s]\n", __func__);
		hdcp_deauthenticate();
		mutex_lock(&hdcp_auth_state_mutex);
		hdmi_msm_state->hpd_during_auth = FALSE;
		mutex_unlock(&hdcp_auth_state_mutex);
	} else {
		DEV_WARN("[DEV_DBG]: Calling reauth from [%s]\n", __func__);
		if (hdmi_msm_state->panel_power_on)
			queue_work(hdmi_work_queue,
			    &hdmi_msm_state->hdcp_reauth_work);
	}
}
#else
	static inline void hdmi_msm_hdcp_enable(void) { return ; }
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT */

void hdmi_msm_init_phy(int video_format)
{
	/* De-serializer delay D/C for non-lbk mode */
	/* PHY REG0 = (DESER_SEL(0) | DESER_DEL_CTRL(3) | AMUX_OUT_SEL(0)) */
	HDMI_OUTP_ND(0x0300, 0x0C); /*0b00001100*/

	if (video_format == HDMI_VFRMT_720x480p60_16_9) {
		/* PHY REG1 = DTEST_MUX_SEL(5) | PLL_GAIN_SEL(0)
			    | OUTVOL_SWING_CTRL(3) */
		HDMI_OUTP_ND(0x0304, 0x53); /*0b01010011*/
	} else {
		/* If the freq. is less than 120MHz, use low gain 0 for board
		   with termination */
		/* PHY REG1 = DTEST_MUX_SEL(5) | PLL_GAIN_SEL(0)
			    | OUTVOL_SWING_CTRL(4) */
		HDMI_OUTP_ND(0x0304, 0x54); /*0b01010100*/
	}

	/* No matter what, start from the power down mode */
	/* PHY REG2 = PD_PWRGEN | PD_PLL | PD_DRIVE_4 | PD_DRIVE_3
		    | PD_DRIVE_2 | PD_DRIVE_1 | PD_DESER */
	HDMI_OUTP_ND(0x0308, 0x7F); /*0b01111111*/

	/* Turn PowerGen on */
	/* PHY REG2 =             PD_PLL | PD_DRIVE_4 | PD_DRIVE_3
		    | PD_DRIVE_2 | PD_DRIVE_1 | PD_DESER */
	HDMI_OUTP_ND(0x0308, 0x3F); /*0b00111111*/

	/* Turn PLL power on */
	/* PHY REG2 =                      PD_DRIVE_4 | PD_DRIVE_3
		    | PD_DRIVE_2 | PD_DRIVE_1 | PD_DESER */
	HDMI_OUTP_ND(0x0308, 0x1F); /*0b00011111*/

	/* Write to HIGH after PLL power down de-assert */
	/* PHY REG3 = PLL_ENABLE */
	HDMI_OUTP_ND(0x030C, 0x01);
	/* ASIC power on */
	/* PHY REG9 = 0 */
	HDMI_OUTP_ND(0x0324, 0x00);
	/* Enable PLL lock detect, PLL lock det will go high after lock
	   Enable the re-time logic */
	/* PHY REG12 = PLL_LOCK_DETECT_EN | RETIMING_ENABLE */
	HDMI_OUTP_ND(0x0330, 0x03); /*0b00000011*/

	/* Drivers are on */
	/* PHY REG2 = PD_DESER */
	HDMI_OUTP_ND(0x0308, 0x01); /*0b00000001*/
	/* If the RX detector is needed */
	/* PHY REG2 = RCV_SENSE_EN | PD_DESER */
	HDMI_OUTP_ND(0x0308, 0x81); /*0b10000001*/

	/* PHY REG4 = 0 */
	HDMI_OUTP_ND(0x0310, 0x00);
	/* PHY REG5 = 0 */
	HDMI_OUTP_ND(0x0314, 0x00);
	/* PHY REG6 = 0 */
	HDMI_OUTP_ND(0x0318, 0x00);
	/* PHY REG7 = 0 */
	HDMI_OUTP_ND(0x031C, 0x00);
	/* PHY REG8 = 0 */
	HDMI_OUTP_ND(0x0320, 0x00);
	/* PHY REG10 = 0 */
	HDMI_OUTP_ND(0x0328, 0x00);
	/* PHY REG11 = 0 */
	HDMI_OUTP_ND(0x032C, 0x00);
	/* If we want to use lock enable based on counting */
	/* PHY REG12 = FORCE_LOCK | PLL_LOCK_DETECT_EN | RETIMING_ENABLE */
	HDMI_OUTP_ND(0x0330, 0x13); /*0b00010011*/
}

void hdmi_msm_powerdown_phy(void)
{
	/* Disable PLL */
	HDMI_OUTP_ND(0x030C, 0x00);
	/* Power down PHY */
	HDMI_OUTP_ND(0x0308, 0xFF); /*0b01111111*/
}

static void hdmi_msm_video_setup(int video_format)
{
	uint32 total_v   = 0;
	uint32 total_h   = 0;
	uint32 start_h   = 0;
	uint32 end_h     = 0;
	uint32 start_v   = 0;
	uint32 end_v     = 0;
	const struct hdmi_disp_mode_timing_type *timing =
		hdmi_common_get_supported_mode(video_format);

	printk("hdmi_msm_video_setup:%d\n",video_format);
	/* timing register setup */
	if (timing == NULL) {
		DEV_ERR("video format not supported: %d\n", video_format);
		return;
	}

	/* Hsync Total and Vsync Total */
	total_h = timing->active_h + timing->front_porch_h
		+ timing->back_porch_h + timing->pulse_width_h - 1;
	total_v = timing->active_v + timing->front_porch_v
		+ timing->back_porch_v + timing->pulse_width_v - 1;
	/* 0x02C0 HDMI_TOTAL
	   [27:16] V_TOTAL Vertical Total
	   [11:0]  H_TOTAL Horizontal Total */
	HDMI_OUTP(0x02C0, ((total_v << 16) & 0x0FFF0000)
		| ((total_h << 0) & 0x00000FFF));

	/* Hsync Start and Hsync End */
	start_h = timing->back_porch_h + timing->pulse_width_h;
	end_h   = (total_h + 1) - timing->front_porch_h;
	/* 0x02B4 HDMI_ACTIVE_H
	   [27:16] END Horizontal end
	   [11:0]  START Horizontal start */
	HDMI_OUTP(0x02B4, ((end_h << 16) & 0x0FFF0000)
		| ((start_h << 0) & 0x00000FFF));

	start_v = timing->back_porch_v + timing->pulse_width_v - 1;
	end_v   = total_v - timing->front_porch_v;
	/* 0x02B8 HDMI_ACTIVE_V
	   [27:16] END Vertical end
	   [11:0]  START Vertical start */
	HDMI_OUTP(0x02B8, ((end_v << 16) & 0x0FFF0000)
		| ((start_v << 0) & 0x00000FFF));

	if (timing->interlaced) {
		/* 0x02C4 HDMI_V_TOTAL_F2
		   [11:0] V_TOTAL_F2 Vertical total for field2 */
		HDMI_OUTP(0x02C4, ((total_v + 1) << 0) & 0x00000FFF);

		/* 0x02BC HDMI_ACTIVE_V_F2
		   [27:16] END_F2 Vertical end for field2
		   [11:0]  START_F2 Vertical start for Field2 */
		HDMI_OUTP(0x02BC,
			  (((start_v + 1) << 0) & 0x00000FFF)
			| (((end_v + 1) << 16) & 0x0FFF0000));
	} else {
		/* HDMI_V_TOTAL_F2 */
		HDMI_OUTP(0x02C4, 0);
		/* HDMI_ACTIVE_V_F2 */
		HDMI_OUTP(0x02BC, 0);
	}

	/* 0x02C8 HDMI_FRAME_CTRL
	   31 INTERLACED_EN   Interlaced or progressive enable bit
	      0: Frame in progressive
	      1: Frame is interlaced
	   29 HSYNC_HDMI_POL  HSYNC polarity fed to HDMI core
	      0: Active Hi Hsync, detect the rising edge of hsync
	      1: Active lo Hsync, Detect the falling edge of Hsync
	   28 VSYNC_HDMI_POL  VSYNC polarity fed to HDMI core
	      0: Active Hi Vsync, detect the rising edge of vsync
	      1: Active Lo Vsync, Detect the falling edge of Vsync
	   12 RGB_MUX_SEL     ALPHA mdp4 input is RGB, mdp4 input is BGR */
	HDMI_OUTP(0x02C8,
		  ((timing->interlaced << 31) & 0x80000000)
		| ((timing->active_low_h << 29) & 0x20000000)
		| ((timing->active_low_v << 28) & 0x10000000)
		| (1 << 12));
}

struct hdmi_msm_audio_acr {
	uint32 n;	/* N parameter for clock regeneration */
	uint32 cts;	/* CTS parameter for clock regeneration */
};

struct hdmi_msm_audio_arcs {
	uint32 pclk;
	struct hdmi_msm_audio_acr lut[MSM_HDMI_SAMPLE_RATE_MAX];
};

#define HDMI_MSM_AUDIO_ARCS(pclk, ...) { pclk, __VA_ARGS__ }

/* Audio constants lookup table for hdmi_msm_audio_acr_setup */
/* Valid Pixel-Clock rates: 25.2MHz, 27MHz, 27.03MHz, 74.25MHz, 148.5MHz */
static const struct hdmi_msm_audio_arcs hdmi_msm_audio_acr_lut[] = {
	/*  25.200MHz  */
	HDMI_MSM_AUDIO_ARCS(25200, {
		{4096, 25200}, {6272, 28000}, {6144, 25200}, {12544, 28000},
		{12288, 25200}, {25088, 28000}, {24576, 25200} }),
	/*  27.000MHz  */
	HDMI_MSM_AUDIO_ARCS(27000, {
		{4096, 27000}, {6272, 30000}, {6144, 27000}, {12544, 30000},
		{12288, 27000}, {25088, 30000}, {24576, 27000} }),
	/*  27.030MHz */
	HDMI_MSM_AUDIO_ARCS(27030, {
		{4096, 27030}, {6272, 30030}, {6144, 27030}, {12544, 30030},
		{12288, 27030}, {25088, 30030}, {24576, 27030} }),
	/*  74.250MHz */
	HDMI_MSM_AUDIO_ARCS(74250, {
		{4096, 74250}, {6272, 82500}, {6144, 74250}, {12544, 82500},
		{12288, 74250}, {25088, 82500}, {24576, 74250} }),
	/* 148.500MHz */
	HDMI_MSM_AUDIO_ARCS(148500, {
		{4096, 148500}, {6272, 165000}, {6144, 148500}, {12544, 165000},
		{12288, 148500}, {25088, 165000}, {24576, 148500} }),
};

static void hdmi_msm_audio_acr_setup(boolean enabled, int video_format,
	int audio_sample_rate, int num_of_channels)
{
	/* Read first before writing */
	/* HDMI_ACR_PKT_CTRL[0x0024] */
	uint32 acr_pck_ctrl_reg = HDMI_INP(0x0024);

	if (enabled) {
		const struct hdmi_disp_mode_timing_type *timing =
			hdmi_common_get_supported_mode(video_format);
		const struct hdmi_msm_audio_arcs *audio_arc =
			&hdmi_msm_audio_acr_lut[0];
		const int lut_size = sizeof(hdmi_msm_audio_acr_lut)
			/sizeof(*hdmi_msm_audio_acr_lut);
		uint32 i, n, cts, layout, multiplier, aud_pck_ctrl_2_reg;

		if (timing == NULL) {
			DEV_WARN("%s: video format %d not supported\n",
				__func__, video_format);
			return;
		}

		for (i = 0; i < lut_size;
			audio_arc = &hdmi_msm_audio_acr_lut[++i]) {
			if (audio_arc->pclk == timing->pixel_freq)
				break;
		}
		if (i >= lut_size) {
			DEV_WARN("%s: pixel clock %d not supported\n", __func__,
				timing->pixel_freq);
			return;
		}

		n = audio_arc->lut[audio_sample_rate].n;
		cts = audio_arc->lut[audio_sample_rate].cts;
		layout = (MSM_HDMI_AUDIO_CHANNEL_2 == num_of_channels) ? 0 : 1;

		if ((MSM_HDMI_SAMPLE_RATE_192KHZ == audio_sample_rate) ||
		    (MSM_HDMI_SAMPLE_RATE_176_4KHZ == audio_sample_rate)) {
			multiplier = 4;
			n >>= 2; /* divide N by 4 and use multiplier */
		} else if ((MSM_HDMI_SAMPLE_RATE_96KHZ == audio_sample_rate) ||
			  (MSM_HDMI_SAMPLE_RATE_88_2KHZ == audio_sample_rate)) {
			multiplier = 2;
			n >>= 1; /* divide N by 2 and use multiplier */
		} else {
			multiplier = 1;
		}
		DEV_DBG("%s: n=%u, cts=%u, layout=%u\n", __func__, n, cts,
			layout);

		/* AUDIO_PRIORITY | SOURCE */
		acr_pck_ctrl_reg |= 0x80000100;
		/* N_MULTIPLE(multiplier) */
		acr_pck_ctrl_reg |= (multiplier & 7) << 16;

		if ((MSM_HDMI_SAMPLE_RATE_48KHZ == audio_sample_rate) ||
		    (MSM_HDMI_SAMPLE_RATE_96KHZ == audio_sample_rate) ||
		    (MSM_HDMI_SAMPLE_RATE_192KHZ == audio_sample_rate)) {
			/* SELECT(3) */
			acr_pck_ctrl_reg |= 3 << 4;
			/* CTS_48 */
			cts <<= 12;

			/* CTS: need to determine how many fractional bits */
			/* HDMI_ACR_48_0 */
			HDMI_OUTP(0x00D4, cts);
			/* N */
			/* HDMI_ACR_48_1 */
			HDMI_OUTP(0x00D8, n);
		} else if ((MSM_HDMI_SAMPLE_RATE_44_1KHZ == audio_sample_rate)
			   || (MSM_HDMI_SAMPLE_RATE_88_2KHZ ==
			       audio_sample_rate)
			   || (MSM_HDMI_SAMPLE_RATE_176_4KHZ ==
			       audio_sample_rate)) {
			/* SELECT(2) */
			acr_pck_ctrl_reg |= 2 << 4;
			/* CTS_44 */
			cts <<= 12;

			/* CTS: need to determine how many fractional bits */
			/* HDMI_ACR_44_0 */
			HDMI_OUTP(0x00CC, cts);
			/* N */
			/* HDMI_ACR_44_1 */
			HDMI_OUTP(0x00D0, n);
		} else {	/* default to 32k */
			/* SELECT(1) */
			acr_pck_ctrl_reg |= 1 << 4;
			/* CTS_32 */
			cts <<= 12;

			/* CTS: need to determine how many fractional bits */
			/* HDMI_ACR_32_0 */
			HDMI_OUTP(0x00C4, cts);
			/* N */
			/* HDMI_ACR_32_1 */
			HDMI_OUTP(0x00C8, n);
		}
		/* Payload layout depends on number of audio channels */
		/* LAYOUT_SEL(layout) */
		aud_pck_ctrl_2_reg = 1 | (layout << 1);
		/* override | layout */
		/* HDMI_AUDIO_PKT_CTRL2[0x00044] */
		HDMI_OUTP(0x00044, aud_pck_ctrl_2_reg);

		/* SEND | CONT */
		acr_pck_ctrl_reg |= 0x00000003;
	} else {
		/* ~(SEND | CONT) */
		acr_pck_ctrl_reg &= ~0x00000003;
	}
	/* HDMI_ACR_PKT_CTRL[0x0024] */
	HDMI_OUTP(0x0024, acr_pck_ctrl_reg);
}

static void hdmi_msm_outpdw_chk(uint32 offset, uint32 data)
{
	uint32 check, i = 0;

#ifdef DEBUG
	HDMI_OUTP(offset, data);
#endif
	do {
		outpdw(MSM_HDMI_BASE+offset, data);
		check = inpdw(MSM_HDMI_BASE+offset);
	} while (check != data && i++ < 10);

	if (check != data)
		DEV_ERR("%s: failed addr=%08x, data=%x, check=%x",
			__func__, offset, data, check);
}

static void hdmi_msm_rmw32or(uint32 offset, uint32 data)
{
	uint32 reg_data;
	reg_data = inpdw(MSM_HDMI_BASE+offset);
	reg_data = inpdw(MSM_HDMI_BASE+offset);
	hdmi_msm_outpdw_chk(offset, reg_data | data);
}


#define HDMI_AUDIO_CFG				0x01D0
#define HDMI_AUDIO_ENGINE_ENABLE		1
#define HDMI_AUDIO_FIFO_MASK			0x000000F0
#define HDMI_AUDIO_FIFO_WATERMARK_SHIFT		4
#define HDMI_AUDIO_FIFO_MAX_WATER_MARK		8


int hdmi_audio_enable(bool on , u32 fifo_water_mark)
{
	u32 hdmi_audio_config;

	hdmi_audio_config = HDMI_INP(HDMI_AUDIO_CFG);

	if (on) {

		if (fifo_water_mark > HDMI_AUDIO_FIFO_MAX_WATER_MARK) {
			pr_err("%s : HDMI audio fifo water mark can not be more"
				" than %u\n", __func__,
				HDMI_AUDIO_FIFO_MAX_WATER_MARK);
			return -EINVAL;
		}

		/*
		*  Enable HDMI Audio engine.
		*  MUST be enabled after Audio DMA is enabled.
		*/
		hdmi_audio_config &= ~(HDMI_AUDIO_FIFO_MASK);

		hdmi_audio_config |= (HDMI_AUDIO_ENGINE_ENABLE |
			 (fifo_water_mark << HDMI_AUDIO_FIFO_WATERMARK_SHIFT));

	} else
		 hdmi_audio_config &= ~(HDMI_AUDIO_ENGINE_ENABLE);

	HDMI_OUTP(HDMI_AUDIO_CFG, hdmi_audio_config);

	return 0;
}
EXPORT_SYMBOL(hdmi_audio_enable);

int hdmi_msm_audio_info_setup(bool enabled, u32 num_of_channels,
	u32 channel_allocation, u32 level_shift, bool down_mix)
{
	uint32 channel_count = 1;	/* Default to 2 channels
					   -> See Table 17 in CEA-D spec */
	uint32 check_sum, audio_info_0_reg, audio_info_1_reg;
	uint32 audio_info_ctrl_reg;
	u32 aud_pck_ctrl_2_reg;
	u32 layout;

	layout = (MSM_HDMI_AUDIO_CHANNEL_2 == num_of_channels) ? 0 : 1;
	aud_pck_ctrl_2_reg = 1 | (layout << 1);
	HDMI_OUTP(0x00044, aud_pck_ctrl_2_reg);

	/* Please see table 20 Audio InfoFrame in HDMI spec
	   FL  = front left
	   FC  = front Center
	   FR  = front right
	   FLC = front left center
	   FRC = front right center
	   RL  = rear left
	   RC  = rear center
	   RR  = rear right
	   RLC = rear left center
	   RRC = rear right center
	   LFE = low frequency effect
	 */

	/* Read first then write because it is bundled with other controls */
	/* HDMI_INFOFRAME_CTRL0[0x002C] */
	audio_info_ctrl_reg = HDMI_INP(0x002C);

	if (enabled) {
		switch (num_of_channels) {
		case MSM_HDMI_AUDIO_CHANNEL_2:
			channel_allocation = 0;	/* Default to FR,FL */
			break;
		case MSM_HDMI_AUDIO_CHANNEL_4:
			channel_count = 3;
			/* FC,LFE,FR,FL */
			channel_allocation = 0x3;
			break;
		case MSM_HDMI_AUDIO_CHANNEL_6:
			channel_count = 5;
			/* RR,RL,FC,LFE,FR,FL */
			channel_allocation = 0xB;
			break;
		case MSM_HDMI_AUDIO_CHANNEL_8:
			channel_count = 7;
			/* FRC,FLC,RR,RL,FC,LFE,FR,FL */
			channel_allocation = 0x1f;
			break;
		default:
			pr_err("%s(): Unsupported num_of_channels = %u\n",
					__func__, num_of_channels);
			return -EINVAL;
			break;
		}

		/* Program the Channel-Speaker allocation */
		audio_info_1_reg = 0;
		/* CA(channel_allocation) */
		audio_info_1_reg |= channel_allocation & 0xff;
		/* Program the Level shifter */
		/* LSV(level_shift) */
		audio_info_1_reg |= (level_shift << 11) & 0x00007800;
		/* Program the Down-mix Inhibit Flag */
		/* DM_INH(down_mix) */
		audio_info_1_reg |= (down_mix << 15) & 0x00008000;

		/* HDMI_AUDIO_INFO1[0x00E8] */
		HDMI_OUTP(0x00E8, audio_info_1_reg);

		/* Calculate CheckSum
		   Sum of all the bytes in the Audio Info Packet bytes
		   (See table 8.4 in HDMI spec) */
		check_sum = 0;
		/* HDMI_AUDIO_INFO_FRAME_PACKET_HEADER_TYPE[0x84] */
		check_sum += 0x84;
		/* HDMI_AUDIO_INFO_FRAME_PACKET_HEADER_VERSION[0x01] */
		check_sum += 1;
		/* HDMI_AUDIO_INFO_FRAME_PACKET_LENGTH[0x0A] */
		check_sum += 0x0A;
		check_sum += channel_count;
		check_sum += channel_allocation;
		/* See Table 8.5 in HDMI spec */
		check_sum += (level_shift & 0xF) << 3 | (down_mix & 0x1) << 7;
		check_sum &= 0xFF;
		check_sum = (uint8) (256 - check_sum);

		audio_info_0_reg = 0;
		/* CHECKSUM(check_sum) */
		audio_info_0_reg |= check_sum & 0xff;
		/* CC(channel_count) */
		audio_info_0_reg |= (channel_count << 8) & 0x00000700;

		/* HDMI_AUDIO_INFO0[0x00E4] */
		HDMI_OUTP(0x00E4, audio_info_0_reg);

		/* Set these flags */
		/* AUDIO_INFO_UPDATE | AUDIO_INFO_SOURCE | AUDIO_INFO_CONT
		 | AUDIO_INFO_SEND */
		audio_info_ctrl_reg |= 0x000000F0;
	} else {
		/* Clear these flags */
		/* ~(AUDIO_INFO_UPDATE | AUDIO_INFO_SOURCE | AUDIO_INFO_CONT
		   | AUDIO_INFO_SEND) */
		audio_info_ctrl_reg &= ~0x000000F0;
	}
	/* HDMI_INFOFRAME_CTRL0[0x002C] */
	HDMI_OUTP(0x002C, audio_info_ctrl_reg);

	//hdmi_msm_dump_regs("HDMI-AUDIO-ON: ");

	return 0;
}
EXPORT_SYMBOL(hdmi_msm_audio_info_setup);

static void hdmi_msm_audio_ctrl_setup(boolean enabled, int delay)
{
	uint32 audio_pkt_ctrl_reg = 0;

	/* Enable Packet Transmission */
	audio_pkt_ctrl_reg |= enabled ? 0x00000001 : 0;
	audio_pkt_ctrl_reg |= (delay << 4);

	/* HDMI_AUDIO_PKT_CTRL1[0x0020] */
	HDMI_OUTP(0x0020, audio_pkt_ctrl_reg);
}

static void hdmi_msm_en_gc_packet(boolean av_mute_is_requested)
{
	/* HDMI_GC[0x0040] */
	HDMI_OUTP(0x0040, av_mute_is_requested ? 1 : 0);

	/* GC packet enable (every frame) */
	/* HDMI_VBI_PKT_CTRL[0x0028] */
	hdmi_msm_rmw32or(0x0028, 3 << 4);
}

static void hdmi_msm_en_isrc_packet(boolean isrc_is_continued)
{
	static const char isrc_psuedo_data[] =
					"ISRC1:0123456789isrc2=ABCDEFGHIJ";
	const uint32 * isrc_data = (const uint32 *) isrc_psuedo_data;

	/* ISRC_STATUS =0b010 | ISRC_CONTINUE | ISRC_VALID */
	/* HDMI_ISRC1_0[0x00048] */
	HDMI_OUTP(0x00048, 2 | (isrc_is_continued ? 1 : 0) << 6 | 0 << 7);

	/* HDMI_ISRC1_1[0x004C] */
	HDMI_OUTP(0x004C, *isrc_data++);
	/* HDMI_ISRC1_2[0x0050] */
	HDMI_OUTP(0x0050, *isrc_data++);
	/* HDMI_ISRC1_3[0x0054] */
	HDMI_OUTP(0x0054, *isrc_data++);
	/* HDMI_ISRC1_4[0x0058] */
	HDMI_OUTP(0x0058, *isrc_data++);

	/* HDMI_ISRC2_0[0x005C] */
	HDMI_OUTP(0x005C, *isrc_data++);
	/* HDMI_ISRC2_1[0x0060] */
	HDMI_OUTP(0x0060, *isrc_data++);
	/* HDMI_ISRC2_2[0x0064] */
	HDMI_OUTP(0x0064, *isrc_data++);
	/* HDMI_ISRC2_3[0x0068] */
	HDMI_OUTP(0x0068, *isrc_data);

	/* HDMI_VBI_PKT_CTRL[0x0028] */
	/* ISRC Send + Continuous */
	hdmi_msm_rmw32or(0x0028, 3 << 8);
}

static void hdmi_msm_en_acp_packet(uint32 byte1)
{
	/* HDMI_ACP[0x003C] */
	HDMI_OUTP(0x003C, 2 | 1 << 8 | byte1 << 16);

	/* HDMI_VBI_PKT_CTRL[0x0028] */
	/* ACP send, s/w source */
	hdmi_msm_rmw32or(0x0028, 3 << 12);
}

static void hdmi_msm_audio_setup(void)
{
	const int channels = MSM_HDMI_AUDIO_CHANNEL_2;

	/* (0) for clr_avmute, (1) for set_avmute */
	hdmi_msm_en_gc_packet(0);
	/* (0) for isrc1 only, (1) for isrc1 and isrc2 */
	hdmi_msm_en_isrc_packet(1);
	/* arbitrary bit pattern for byte1 */
	hdmi_msm_en_acp_packet(0x5a);

	hdmi_msm_audio_acr_setup(TRUE,
		external_common_state->video_resolution,
		MSM_HDMI_SAMPLE_RATE_48KHZ, channels);
	hdmi_msm_audio_info_setup(TRUE, channels,0, 0, FALSE);
	hdmi_msm_audio_ctrl_setup(TRUE, 1);

	/* Turn on Audio FIFO and SAM DROP ISR */
	HDMI_OUTP(0x02CC, HDMI_INP(0x02CC) | BIT(1) | BIT(3));
	DEV_INFO("HDMI Audio: Enabled\n");
}

static int hdmi_msm_audio_off(void)
{
	uint32 audio_pkt_ctrl, audio_cfg;
	 /* Number of wait iterations */
	int i = 10;
	audio_pkt_ctrl = HDMI_INP_ND(0x0020);
	audio_cfg = HDMI_INP_ND(0x01D0);

	/* Checking BIT[0] of AUDIO PACKET CONTROL and */
	/* AUDIO CONFIGURATION register */
	while (((audio_pkt_ctrl & 0x00000001) || (audio_cfg & 0x00000001))
		&& (i--)) {
		audio_pkt_ctrl = HDMI_INP_ND(0x0020);
		audio_cfg = HDMI_INP_ND(0x01D0);
		DEV_DBG("%d times :: HDMI AUDIO PACKET is %08x and "
		"AUDIO CFG is %08x", i, audio_pkt_ctrl, audio_cfg);
		msleep(100);
		if (!i) {
			DEV_ERR("%s:failed to set BIT[0] AUDIO PACKET"
			"CONTROL or AUDIO CONFIGURATION REGISTER\n",
				__func__);
			return -ETIMEDOUT;
		}
	}
	hdmi_msm_audio_info_setup(FALSE, 0, 0, 0, FALSE);
	hdmi_msm_audio_ctrl_setup(FALSE, 0);
	hdmi_msm_audio_acr_setup(FALSE, 0, 0, 0);
	DEV_INFO("HDMI Audio: Disabled\n");
	return 0;
}


static uint8 hdmi_msm_avi_iframe_lut[][16] = {
/*	720x480p60_4_3    | 720x480i60_16_9   | 720x576p50_16_9   | 720x576i50_16_9   |
	1280x720p60_16_9  | 1280x720p50_16_9  | 1920x1080p60_16_9 | 1920x1080i60_16_9 |
	1920x1080p50_16_9 | 1920x1080i50_16_9 | 1920x1080p24_16_9 | 1920x1080p30_16_9 |
	1920x1080p25_16_9 | 640x480p60_4_3    | 720x480p60_16_9   | 720x576p50_4_3    */
	{0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,	0x10,
	 0x10,	0x10,	0x10,	0x10,	0x10, 	0x10,	0x10},	//00

	{0x18,	0x18,	0x28,	0x28,	0x28,	0x28,	0x28,	0x28,	0x28,
	 0x28,	0x28,	0x28,	0x28,	0x18,	0x28,	0x18},	//01

	{0x04,	0x04,	0x04,	0x04,	0x04,	0x04,	0x04,	0x04,	0x04,
	 0x04,	0x04,	0x04,	0x04,	0x88,	0x04,	0x04},	//02

	{0x02,	0x06,	0x12,	0x15,	0x04,	0x13,	0x10,	0x05,	0x1F,
	 0x14,	0x20,	0x22,	0x21,	0x01,	0x03,	0x11},	//03

	{0x00,	0x01,	0x00,	0x01,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00},	//04

	{0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00},	//05

	{0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00},	//06

	{0xE1,	0xE1,	0x41,	0x41,	0xD1,	0xd1,	0x39,	0x39,	0x39,
	 0x39,	0x39,	0x39,	0x39,	0xe1,	0xE1,	0x41},	//07

	{0x01,	0x01,	0x02,	0x02,	0x02,	0x02,	0x04,	0x04,	0x04,
	 0x04,	0x04,	0x04,	0x04,	0x01,	0x01,	0x02},	//08

	{0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00},	//09

	{0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	 0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00},	//10

	{0xD1,	0xD1,	0xD1,	0xD1,	0x01,	0x01,	0x81,	0x81,	0x81,
	 0x81,	0x81,	0x81,	0x81,	0x81,	0xD1,	0xD1},	//11

	{0x02,	0x02,	0x02,	0x02,	0x05,	0x05,	0x07,	0x07,	0x07,
	 0x07,	0x07,	0x07,	0x07,	0x02,	0x02,	0x02}	//12
};

static void hdmi_msm_avi_info_frame(void)
{
	/* two header + length + 13 data */
	uint8 aviInfoFrame[16];
	uint8 checksum;
	uint32 sum;
	uint32 regVal;
	int i;
	int mode = 0;

	switch (external_common_state->video_resolution) {
	case HDMI_VFRMT_720x480p60_4_3:
		mode = 0;
		break;
	case HDMI_VFRMT_720x480i60_16_9:
		mode = 1;
		break;
	case HDMI_VFRMT_720x576p50_16_9:
		mode = 2;
		break;
	case HDMI_VFRMT_720x576i50_16_9:
		mode = 3;
		break;
	case HDMI_VFRMT_1280x720p60_16_9:
		mode = 4;
		break;
	case HDMI_VFRMT_1280x720p50_16_9:
		mode = 5;
		break;
#ifdef CONFIG_VIDEO_MHL_TABLET_V1
	case HDMI_VFRMT_1920x1080p60_16_9:
		mode = 11;
		break;
#else		
	case HDMI_VFRMT_1920x1080p60_16_9:
		mode = 6;
		break;
#endif
	case HDMI_VFRMT_1920x1080i60_16_9:
		mode = 7;
		break;
	case HDMI_VFRMT_1920x1080p50_16_9:
		mode = 8;
		break;
	case HDMI_VFRMT_1920x1080i50_16_9:
		mode = 9;
		break;
	case HDMI_VFRMT_1920x1080p24_16_9:
		mode = 10;
		break;
	case HDMI_VFRMT_1920x1080p30_16_9:
		mode = 11;
		break;
	case HDMI_VFRMT_1920x1080p25_16_9:
		mode = 12;
		break;
	case HDMI_VFRMT_640x480p60_4_3:
		mode = 13;
		break;
	case HDMI_VFRMT_720x480p60_16_9:
		mode = 14;
		break;
	case HDMI_VFRMT_720x576p50_4_3:
		mode = 15;
		break;
	default:
		DEV_INFO("%s: mode %d not supported\n", __func__,
			external_common_state->video_resolution);
		return;
	}

	/* InfoFrame Type = 82 */
	aviInfoFrame[0]  = 0x82;
	/* Version = 2 */
	aviInfoFrame[1]  = 2;
	/* Length of AVI InfoFrame = 13 */
	aviInfoFrame[2]  = 13;

	/* Data Byte 01: 0 Y1 Y0 A0 B1 B0 S1 S0 */
	aviInfoFrame[3]  = hdmi_msm_avi_iframe_lut[0][mode];
	/* Data Byte 02: C1 C0 M1 M0 R3 R2 R1 R0 */
	aviInfoFrame[4]  = hdmi_msm_avi_iframe_lut[1][mode];
	/* Data Byte 03: ITC EC2 EC1 EC0 Q1 Q0 SC1 SC0 */
	aviInfoFrame[5]  = hdmi_msm_avi_iframe_lut[2][mode];
	/* Data Byte 04: 0 VIC6 VIC5 VIC4 VIC3 VIC2 VIC1 VIC0 */
	aviInfoFrame[6]  = hdmi_msm_avi_iframe_lut[3][mode];
	/* Data Byte 05: 0 0 0 0 PR3 PR2 PR1 PR0 */
	aviInfoFrame[7]  = hdmi_msm_avi_iframe_lut[4][mode];
	/* Data Byte 06: LSB Line No of End of Top Bar */
	aviInfoFrame[8]  = hdmi_msm_avi_iframe_lut[5][mode];
	/* Data Byte 07: MSB Line No of End of Top Bar */
	aviInfoFrame[9]  = hdmi_msm_avi_iframe_lut[6][mode];
	/* Data Byte 08: LSB Line No of Start of Bottom Bar */
	aviInfoFrame[10] = hdmi_msm_avi_iframe_lut[7][mode];
	/* Data Byte 09: MSB Line No of Start of Bottom Bar */
	aviInfoFrame[11] = hdmi_msm_avi_iframe_lut[8][mode];
	/* Data Byte 10: LSB Pixel Number of End of Left Bar */
	aviInfoFrame[12] = hdmi_msm_avi_iframe_lut[9][mode];
	/* Data Byte 11: MSB Pixel Number of End of Left Bar */
	aviInfoFrame[13] = hdmi_msm_avi_iframe_lut[10][mode];
	/* Data Byte 12: LSB Pixel Number of Start of Right Bar */
	aviInfoFrame[14] = hdmi_msm_avi_iframe_lut[11][mode];
	/* Data Byte 13: MSB Pixel Number of Start of Right Bar */
	aviInfoFrame[15] = hdmi_msm_avi_iframe_lut[12][mode];

	sum = 0;
	for (i = 0; i < 16; i++)
		sum += aviInfoFrame[i];
	sum &= 0xFF;
	sum = 256 - sum;
	checksum = (uint8) sum;

	regVal = aviInfoFrame[5];
	regVal = regVal << 8 | aviInfoFrame[4];
	regVal = regVal << 8 | aviInfoFrame[3];
	regVal = regVal << 8 | checksum;
	HDMI_OUTP(0x006C, regVal);

	regVal = aviInfoFrame[9];
	regVal = regVal << 8 | aviInfoFrame[8];
	regVal = regVal << 8 | aviInfoFrame[7];
	regVal = regVal << 8 | aviInfoFrame[6];
	HDMI_OUTP(0x0070, regVal);

	regVal = aviInfoFrame[13];
	regVal = regVal << 8 | aviInfoFrame[12];
	regVal = regVal << 8 | aviInfoFrame[11];
	regVal = regVal << 8 | aviInfoFrame[10];
	HDMI_OUTP(0x0074, regVal);

	regVal = aviInfoFrame[1];
	regVal = regVal << 16 | aviInfoFrame[15];
	regVal = regVal << 8 | aviInfoFrame[14];
	HDMI_OUTP(0x0078, regVal);

	/* INFOFRAME_CTRL0[0x002C] */
	/* 0x3 for AVI InfFrame enable (every frame) */
	HDMI_OUTP(0x002C, HDMI_INP(0x002C) | 0x00000003L);
}

#ifdef CONFIG_FB_MSM_HDMI_3D
static void hdmi_msm_vendor_infoframe_packetsetup(void)
{
	uint32 packet_header      = 0;
	uint32 check_sum          = 0;
	uint32 packet_payload     = 0;

	if (!external_common_state->format_3d) {
		HDMI_OUTP(0x0034, 0);
		return;
	}

	/* 0x0084 GENERIC0_HDR
	 *   HB0             7:0  NUM
	 *   HB1            15:8  NUM
	 *   HB2           23:16  NUM */
	/* Setup Packet header and payload */
	/* 0x81 VS_INFO_FRAME_ID
	   0x01 VS_INFO_FRAME_VERSION
	   0x1B VS_INFO_FRAME_PAYLOAD_LENGTH */
	packet_header  = 0x81 | (0x01 << 8) | (0x1B << 16);
	HDMI_OUTP(0x0084, packet_header);

	check_sum  = packet_header & 0xff;
	check_sum += (packet_header >> 8) & 0xff;
	check_sum += (packet_header >> 16) & 0xff;

	/* 0x008C GENERIC0_1
	 *   BYTE4           7:0  NUM
	 *   BYTE5          15:8  NUM
	 *   BYTE6         23:16  NUM
	 *   BYTE7         31:24  NUM */
	/* 0x02 VS_INFO_FRAME_3D_PRESENT */
	packet_payload  = 0x02 << 5;
	switch (external_common_state->format_3d) {
	case 1:
		/* 0b1000 VIDEO_3D_FORMAT_SIDE_BY_SIDE_HALF */
		packet_payload |= (0x08 << 8) << 4;
		break;
	case 2:
		/* 0b0110 VIDEO_3D_FORMAT_TOP_AND_BOTTOM_HALF */
		packet_payload |= (0x06 << 8) << 4;
		break;
	}
	HDMI_OUTP(0x008C, packet_payload);

	check_sum += packet_payload & 0xff;
	check_sum += (packet_payload >> 8) & 0xff;

	#define IEEE_REGISTRATION_ID	0xC03
	/* Next 3 bytes are IEEE Registration Identifcation */
	/* 0x0088 GENERIC0_0
	 *   BYTE0           7:0  NUM (checksum)
	 *   BYTE1          15:8  NUM
	 *   BYTE2         23:16  NUM
	 *   BYTE3         31:24  NUM */
	check_sum += IEEE_REGISTRATION_ID & 0xff;
	check_sum += (IEEE_REGISTRATION_ID >> 8) & 0xff;
	check_sum += (IEEE_REGISTRATION_ID >> 16) & 0xff;

	HDMI_OUTP(0x0088, (0x100 - (0xff & check_sum))
		| ((IEEE_REGISTRATION_ID & 0xff) << 8)
		| (((IEEE_REGISTRATION_ID >> 8) & 0xff) << 16)
		| (((IEEE_REGISTRATION_ID >> 16) & 0xff) << 24));

	/* 0x0034 GEN_PKT_CTRL
	 *   GENERIC0_SEND   0      0 = Disable Generic0 Packet Transmission
	 *                          1 = Enable Generic0 Packet Transmission
	 *   GENERIC0_CONT   1      0 = Send Generic0 Packet on next frame only
	 *                          1 = Send Generic0 Packet on every frame
	 *   GENERIC0_UPDATE 2      NUM
	 *   GENERIC1_SEND   4      0 = Disable Generic1 Packet Transmission
	 *                          1 = Enable Generic1 Packet Transmission
	 *   GENERIC1_CONT   5      0 = Send Generic1 Packet on next frame only
	 *                          1 = Send Generic1 Packet on every frame
	 *   GENERIC0_LINE   21:16  NUM
	 *   GENERIC1_LINE   29:24  NUM
	 */
	/* GENERIC0_LINE | GENERIC0_UPDATE | GENERIC0_CONT | GENERIC0_SEND
	 * Setup HDMI TX generic packet control
	 * Enable this packet to transmit every frame
	 * Enable this packet to transmit every frame
	 * Enable HDMI TX engine to transmit Generic packet 0 */
	HDMI_OUTP(0x0034, (1 << 16) | (1 << 2) | BIT(1) | BIT(0));
}

static void hdmi_msm_switch_3d(boolean on)
{
	mutex_lock(&external_common_state_hpd_mutex);
	if (external_common_state->hpd_state)
		hdmi_msm_vendor_infoframe_packetsetup();
	mutex_unlock(&external_common_state_hpd_mutex);
}
#endif

int hdmi_msm_clk(int on)
{
	int rc;

	DEV_DBG("HDMI Clk: %s\n", on ? "Enable" : "Disable");
	if (on) {
		rc = clk_enable(hdmi_msm_state->hdmi_app_clk);
		if (rc) {
			DEV_ERR("'hdmi_app_clk' clock enable failed, rc=%d\n",
				rc);
			return rc;
		}

		rc = clk_enable(hdmi_msm_state->hdmi_m_pclk);
		if (rc) {
			DEV_ERR("'hdmi_m_pclk' clock enable failed, rc=%d\n",
				rc);
			return rc;
		}

		rc = clk_enable(hdmi_msm_state->hdmi_s_pclk);
		if (rc) {
			DEV_ERR("'hdmi_s_pclk' clock enable failed, rc=%d\n",
				rc);
			return rc;
		}
	} else {
		clk_disable(hdmi_msm_state->hdmi_app_clk);
		clk_disable(hdmi_msm_state->hdmi_m_pclk);
		clk_disable(hdmi_msm_state->hdmi_s_pclk);
	}

	return 0;
}

void hdmi_msm_reset_core(void)
{
	hdmi_msm_set_mode(FALSE);
	hdmi_msm_clk(0);
	udelay(5);
	hdmi_msm_clk(1);

	clk_reset(hdmi_msm_state->hdmi_app_clk, CLK_RESET_ASSERT);
	clk_reset(hdmi_msm_state->hdmi_m_pclk, CLK_RESET_ASSERT);
	clk_reset(hdmi_msm_state->hdmi_s_pclk, CLK_RESET_ASSERT);
	udelay(20);
	clk_reset(hdmi_msm_state->hdmi_app_clk, CLK_RESET_DEASSERT);
	clk_reset(hdmi_msm_state->hdmi_m_pclk, CLK_RESET_DEASSERT);
	clk_reset(hdmi_msm_state->hdmi_s_pclk, CLK_RESET_DEASSERT);
}

static void hdmi_msm_turn_on(void)
{
	uint32 hpd_ctrl;

	hdmi_msm_reset_core();
	hdmi_msm_init_phy(external_common_state->video_resolution);
	/* HDMI_USEC_REFTIMER[0x0208] */
	HDMI_OUTP(0x0208, 0x0001001B);

	hdmi_msm_video_setup(external_common_state->video_resolution);
	if (!hdmi_msm_is_dvi_mode())
		hdmi_msm_audio_setup();

	hdmi_msm_avi_info_frame();
#ifdef CONFIG_FB_MSM_HDMI_3D
	hdmi_msm_vendor_infoframe_packetsetup();
#endif

	/* set timeout to 4.1ms (max) for hardware debounce */
	hpd_ctrl = (HDMI_INP(0x0258) & ~0xFFF) | 0xFFF;

	/* Toggle HPD circuit to trigger HPD sense */
	HDMI_OUTP(0x0258, ~(1 << 28) & hpd_ctrl);
	HDMI_OUTP(0x0258, (1 << 28) | hpd_ctrl);

	hdmi_msm_set_mode(TRUE);

	/* Setup HPD IRQ */
	HDMI_OUTP(0x0254, 4 | (external_common_state->hpd_state ? 0 : 2));

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
	if (hdmi_msm_state->reauth) {
		hdmi_msm_hdcp_enable();
		hdmi_msm_state->reauth = FALSE ;
	}
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT */
	DEV_INFO("HDMI Core: Initialized\n");
}

static void hdmi_msm_hpd_state_timer(unsigned long data)
{
	queue_work(hdmi_work_queue, &hdmi_msm_state->hpd_state_work);
}

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
static void hdmi_msm_hdcp_timer(unsigned long data)
{
	queue_work(hdmi_work_queue, &hdmi_msm_state->hdcp_work);
}
#endif

static void hdmi_msm_hpd_read_work(struct work_struct *work)
{
	uint32 hpd_ctrl;

	clk_enable(hdmi_msm_state->hdmi_app_clk);
	hdmi_msm_state->pd->core_power(1, 1);
	hdmi_msm_state->pd->enable_5v(1);
	hdmi_msm_set_mode(FALSE);
	hdmi_msm_init_phy(external_common_state->video_resolution);
	/* HDMI_USEC_REFTIMER[0x0208] */
	HDMI_OUTP(0x0208, 0x0001001B);
	hpd_ctrl = (HDMI_INP(0x0258) & ~0xFFF) | 0xFFF;

	/* Toggle HPD circuit to trigger HPD sense */
	HDMI_OUTP(0x0258, ~(1 << 28) & hpd_ctrl);
	HDMI_OUTP(0x0258, (1 << 28) | hpd_ctrl);

	hdmi_msm_set_mode(TRUE);
	msleep(1000);
	external_common_state->hpd_state = (HDMI_INP(0x0250) & 0x2) >> 1;
	if (external_common_state->hpd_state) {
		hdmi_msm_read_edid();
		DEV_DBG("%s: sense CONNECTED: send ONLINE\n", __func__);
		kobject_uevent(external_common_state->uevent_kobj,
			KOBJ_ONLINE);
#ifdef CONFIG_SAMSUNG_HDMI_SWITCH_SET_STATE
		switch_set_state(&hdmi_msm_state->hpd_switch, 1);
#endif
	}

	hdmi_msm_hpd_off();
	hdmi_msm_set_mode(FALSE);
	hdmi_msm_state->pd->core_power(0, 1);
	hdmi_msm_state->pd->enable_5v(0);
	clk_disable(hdmi_msm_state->hdmi_app_clk);

	//hdmi_msm_hpd_on(false);
	//hdmi_msm_state->panel_power_on = FALSE;

}

static void hdmi_msm_hpd_off(void)
{
	DEV_DBG("%s: (timer, clk, 5V, core, IRQ off)\n", __func__);
	u32 offset = 0x0308;

	del_timer(&hdmi_msm_state->hpd_state_timer);
	disable_irq(hdmi_msm_state->irq);

	hdmi_msm_set_mode(FALSE);
	//HDMI_OUTP_ND(0x0308, 0x7F); /*0b01111111*/
	HDMI_OUTP_ND(0x0308, 0xFF); /*0b01111111*/
	u32 in_val = HDMI_INP(0x0308);
	DEV_DBG("HDMI[%04x] => [%08x]\n",offset, in_val);

	hdmi_msm_state->hpd_initialized = FALSE;
	hdmi_msm_state->pd->enable_5v(0);
	hdmi_msm_state->pd->core_power(0, 1);
	hdmi_msm_clk(0);
	hdmi_msm_state->hpd_initialized = FALSE;
}

static void hdmi_msm_dump_regs(const char *prefex)
{
#ifdef REG_DUMP
	print_hex_dump(KERN_INFO, prefix, DUMP_PREFIX_OFFSET, 32, 4,
		(void *)MSM_HDMI_BASE, 0x0334, false);
#endif
}

static int hdmi_msm_set_txphy_init(void)
{
#if defined(CONFIG_VIDEO_MHL_V1) || defined(CONFIG_VIDEO_MHL_V2) || defined(CONFIG_VIDEO_MHL_TABLET_V1)
	if (!hdmi_msm_state || !HDMI_BASE) {
		return 0;
	}
#endif

	hdmi_msm_clk(1);
	hdmi_msm_state->pd->core_power(1, 1);
	hdmi_msm_state->pd->enable_5v(1);

	hdmi_msm_set_mode(FALSE);

	HDMI_OUTP_ND(0x0308, 0xFF); /*0b11111111*/
	DEV_DBG("HDMI[0x0308] => [%08x]\n", HDMI_INP(0x0308));

	hdmi_msm_state->pd->enable_5v(0);
	hdmi_msm_state->pd->core_power(0, 1);
	hdmi_msm_clk(0);

	return 0;
}


static int hdmi_msm_hpd_on(bool trigger_handler)
{
	hdmi_msm_clk(1);
	hdmi_msm_state->pd->core_power(1, 1);
	hdmi_msm_state->pd->enable_5v(1);
	hdmi_msm_dump_regs("HDMI-INIT: ");
	hdmi_msm_set_mode(FALSE);
	hdmi_msm_init_phy(external_common_state->video_resolution);
	/* HDMI_USEC_REFTIMER[0x0208] */
	HDMI_OUTP(0x0208, 0x0001001B);

	/* Check HPD State */
	if (!hdmi_msm_state->hpd_initialized) {
		uint32 hpd_ctrl;
		enable_irq(hdmi_msm_state->irq);

		/* set timeout to 4.1ms (max) for hardware debounce */
		hpd_ctrl = (HDMI_INP(0x0258) & ~0xFFF) | 0xFFF;

		/* Toggle HPD circuit to trigger HPD sense */
		HDMI_OUTP(0x0258, ~(1 << 28) & hpd_ctrl);
		HDMI_OUTP(0x0258, (1 << 28) | hpd_ctrl);

		DEV_DBG("%s: (clk, 5V, core, IRQ on) <trigger:%s>\n", __func__,
			trigger_handler ? "true" : "false");

		if (trigger_handler) {
			/* Set HPD state machine: ensure at least 2 readouts */
			mutex_lock(&hdmi_msm_state_mutex);
			hdmi_msm_state->hpd_stable = 0;
			hdmi_msm_state->hpd_prev_state = TRUE;
			mutex_lock(&external_common_state_hpd_mutex);
			external_common_state->hpd_state = FALSE;
			mutex_unlock(&external_common_state_hpd_mutex);
			hdmi_msm_state->hpd_cable_chg_detected = TRUE;
			mutex_unlock(&hdmi_msm_state_mutex);
			mod_timer(&hdmi_msm_state->hpd_state_timer,
				jiffies + HZ/2);
		}

		hdmi_msm_state->hpd_initialized = TRUE;
	}
	hdmi_msm_set_mode(TRUE);

	return 0;
}

int hdmi_msm_hpd_switch(bool detect_flag)
{
	if(detect_flag)
	{
		printk("hdmi_hpd_on !!!!");
		hdmi_msm_state->dock_state = TRUE;
		if(!hdmi_msm_state->boot_state)
			hdmi_msm_hpd_on(true);
	}
	else
	{
		printk("hdmi_hpd_off !!!!");
		hdmi_msm_state->dock_state = FALSE;
	}
}
EXPORT_SYMBOL(hdmi_msm_hpd_switch);

static int hdmi_msm_power_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd = platform_get_drvdata(pdev);
	bool changed;

	if (!hdmi_msm_state || !hdmi_msm_state->hdmi_app_clk || !MSM_HDMI_BASE)
		return -ENODEV;
#ifdef CONFIG_SUSPEND
	mutex_lock(&hdmi_msm_state_mutex);
	if (hdmi_msm_state->pm_suspended) {
		mutex_unlock(&hdmi_msm_state_mutex);
		DEV_WARN("%s: ignored, pm_suspended\n", __func__);
		return -ENODEV;
	}
	mutex_unlock(&hdmi_msm_state_mutex);
#endif

	DEV_INFO("power: ON (%dx%d %d)\n", mfd->var_xres, mfd->var_yres,
		mfd->var_pixclock);

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
	mutex_lock(&hdmi_msm_state_mutex);
	if (hdmi_msm_state->hdcp_activating) {
		hdmi_msm_state->panel_power_on = TRUE;
		DEV_INFO("HDCP: activating, returning\n");
	}
	mutex_unlock(&hdmi_msm_state_mutex);
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT */

	changed = hdmi_common_get_video_format_from_drv_data(mfd);
	if (!external_common_state->hpd_feature_on) {
		int rc = hdmi_msm_hpd_on(true);
		DEV_INFO("HPD: panel power without 'hpd' feature on\n");
		if (rc) {
			DEV_WARN("HPD: activation failed: rc=%d\n", rc);
			return rc;
		}
	}
	hdmi_msm_audio_info_setup(TRUE, 0, 0, 0, FALSE);

	mutex_lock(&external_common_state_hpd_mutex);
	hdmi_msm_state->panel_power_on = TRUE;
	if ((external_common_state->hpd_state && !hdmi_msm_is_power_on())
		|| changed) {
		mutex_unlock(&external_common_state_hpd_mutex);
		hdmi_msm_turn_on();
	} else
		mutex_unlock(&external_common_state_hpd_mutex);

	hdmi_msm_dump_regs("HDMI-ON: ");

	DEV_INFO("power=%s DVI= %s\n",
		hdmi_msm_is_power_on() ? "ON" : "OFF" ,
		hdmi_msm_is_dvi_mode() ? "ON" : "OFF");
	return 0;
}

/* Note that power-off will also be called when the cable-remove event is
 * processed on the user-space and as a result the framebuffer is powered
 * down.  However, we are still required to be able to detect a cable-insert
 * event; so for now leave the HDMI engine running; so that the HPD IRQ is
 * still being processed.
 */
static int hdmi_msm_power_off(struct platform_device *pdev)
{
	if (!hdmi_msm_state->hdmi_app_clk)
		return -ENODEV;
#ifdef CONFIG_SUSPEND
	mutex_lock(&hdmi_msm_state_mutex);
	if (hdmi_msm_state->pm_suspended) {
		mutex_unlock(&hdmi_msm_state_mutex);
		DEV_WARN("%s: ignored, pm_suspended\n", __func__);
		return -ENODEV;
	}
	mutex_unlock(&hdmi_msm_state_mutex);
#endif

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
	mutex_lock(&hdmi_msm_state_mutex);
	if (hdmi_msm_state->hdcp_activating) {
		hdmi_msm_state->panel_power_on = FALSE;
		mutex_unlock(&hdmi_msm_state_mutex);
		DEV_INFO("HDCP: activating, returning\n");
		return 0;
	}
	mutex_unlock(&hdmi_msm_state_mutex);
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT */

	DEV_INFO("power: OFF (audio off, Reset Core)\n");
	hdmi_msm_audio_off();
	//hdmi_msm_hpd_off();

	disable_irq(hdmi_msm_state->irq);
	hdmi_msm_set_mode(FALSE);
	HDMI_OUTP_ND(0x0308, 0x7F); /*0b01111111*/
	hdmi_msm_state->hpd_initialized = FALSE;
	hdmi_msm_state->pd->enable_5v(0);
	hdmi_msm_state->pd->core_power(0, 1);
	hdmi_msm_clk(0);
	hdmi_msm_state->hpd_initialized = FALSE;

	hdmi_msm_powerdown_phy();
	hdmi_msm_dump_regs("HDMI-OFF: ");
	if(hdmi_msm_state->dock_state)
    	        hdmi_msm_hpd_on(false);

	mutex_lock(&external_common_state_hpd_mutex);
	if (!external_common_state->hpd_feature_on)
		hdmi_msm_hpd_off();

	mutex_unlock(&external_common_state_hpd_mutex);

	hdmi_msm_state->panel_power_on = FALSE;
	return 0;
}

static ssize_t sysfs_store_hpd_onoff(struct device *dev,
                                       struct device_attribute *attr,
                                       const char *buf, size_t len)
{
        int rc;
        int onoff;
//        struct hdmi_msm_state_type *hdmi_state = dev_get_drvdata(dev);

	DEV_DBG("sysfs_hpd_onoff, set to make hpd interruptible\n");

        rc = strict_strtoul(buf, 0, (unsigned long *)&onoff);
        if (rc < 0)
                return rc;
	if (onoff)
		rc = hdmi_msm_hpd_on(true);
	else
		hdmi_msm_hpd_off();
        return len;
}

static ssize_t sysfs_show_hpd_onoff(struct device *dev,
                                       struct device_attribute *attr,
                                       const char *buf, size_t len)
{
	DEV_DBG("sysfs_hpd_onoff\n");
#if 1
	if(gpio_get_value_cansleep(hdmi_msm_state->pd->gpio))
		printk("hpd status is high and hdmi_calbe attached\n");
	else
		printk("hpd status is low and hdmi_calbe dettached\n");
#endif	
	return strlen(buf);
}


static DEVICE_ATTR(hpd_onoff, 0664,
                sysfs_show_hpd_onoff, sysfs_store_hpd_onoff);

static int __devinit hdmi_msm_probe(struct platform_device *pdev)
{
	int rc, retval;
	struct platform_device *fb_dev;

	if (!hdmi_msm_state) {
		pr_err("%s: hdmi_msm_state is NULL\n", __func__);
		return -ENOMEM;
	}

	external_common_state->dev = &pdev->dev;
	DEV_DBG("probe\n");
	if (pdev->id == 0) {
		struct resource *res;

		#define GET_RES(name, mode) do {			\
			res = platform_get_resource_byname(pdev, mode, name); \
			if (!res) {					\
				DEV_ERR("'" name "' resource not found\n"); \
				rc = -ENODEV;				\
				goto error;				\
			}						\
		} while (0)

		#define IO_REMAP(var, name) do {			\
			GET_RES(name, IORESOURCE_MEM);			\
			var = ioremap(res->start, resource_size(res));	\
			if (!var) {					\
				DEV_ERR("'" name "' ioremap failed\n");	\
				rc = -ENOMEM;				\
				goto error;				\
			}						\
		} while (0)

		#define GET_IRQ(var, name) do {				\
			GET_RES(name, IORESOURCE_IRQ);			\
			var = res->start;				\
		} while (0)

		IO_REMAP(hdmi_msm_state->qfprom_io, "hdmi_msm_qfprom_addr");
		hdmi_msm_state->hdmi_io = MSM_HDMI_BASE;
		GET_IRQ(hdmi_msm_state->irq, "hdmi_msm_irq");

		hdmi_msm_state->pd = pdev->dev.platform_data;

		#undef GET_RES
		#undef IO_REMAP
		#undef GET_IRQ
		return 0;
	}

	hdmi_msm_state->hdmi_app_clk = clk_get(&pdev->dev, "core_clk");
	if (IS_ERR(hdmi_msm_state->hdmi_app_clk)) {
		DEV_ERR("'core_clk' clk not found\n");
		rc = IS_ERR(hdmi_msm_state->hdmi_app_clk);
		goto error;
	}

	hdmi_msm_state->hdmi_m_pclk = clk_get(&pdev->dev, "master_iface_clk");
	if (IS_ERR(hdmi_msm_state->hdmi_m_pclk)) {
		DEV_ERR("'master_iface_clk' clk not found\n");
		rc = IS_ERR(hdmi_msm_state->hdmi_m_pclk);
		goto error;
	}

	hdmi_msm_state->hdmi_s_pclk = clk_get(&pdev->dev, "slave_iface_clk");
	if (IS_ERR(hdmi_msm_state->hdmi_s_pclk)) {
		DEV_ERR("'slave_iface_clk' clk not found\n");
		rc = IS_ERR(hdmi_msm_state->hdmi_s_pclk);
		goto error;
	}

	rc = check_hdmi_features();
	if (rc) {
		DEV_ERR("Init FAILED: check_hdmi_features rc=%d\n", rc);
		goto error;
	}

	if (!hdmi_msm_state->pd->core_power) {
		DEV_ERR("Init FAILED: core_power function missing\n");
		rc = -ENODEV;
		goto error;
	}
	if (!hdmi_msm_state->pd->enable_5v) {
		DEV_ERR("Init FAILED: enable_5v function missing\n");
		rc = -ENODEV;
		goto error;
	}

#ifdef CONFIG_SAMSUNG_HDMI_SWITCH_SET_STATE
	hdmi_msm_state->hpd_switch.name = "hdmi";
	switch_dev_register(&hdmi_msm_state->hpd_switch);
#endif
	rc = request_threaded_irq(hdmi_msm_state->irq, NULL, &hdmi_msm_isr,
		IRQF_TRIGGER_HIGH | IRQF_ONESHOT, "hdmi_msm_isr", NULL);
	if (rc) {
		DEV_ERR("Init FAILED: IRQ request, rc=%d\n", rc);
		goto error;
	}
	disable_irq(hdmi_msm_state->irq);

	init_timer(&hdmi_msm_state->hpd_state_timer);
	hdmi_msm_state->hpd_state_timer.function =
		hdmi_msm_hpd_state_timer;
	hdmi_msm_state->hpd_state_timer.data = (uint32)NULL;

	hdmi_msm_state->hpd_state_timer.expires = 0xffffffffL;
	add_timer(&hdmi_msm_state->hpd_state_timer);

#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
	init_timer(&hdmi_msm_state->hdcp_timer);
	hdmi_msm_state->hdcp_timer.function =
		hdmi_msm_hdcp_timer;
	hdmi_msm_state->hdcp_timer.data = (uint32)NULL;

	hdmi_msm_state->hdcp_timer.expires = 0xffffffffL;
	add_timer(&hdmi_msm_state->hdcp_timer);
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT */

	fb_dev = msm_fb_add_device(pdev);
	if (fb_dev) {
		rc = external_common_state_create(fb_dev);
		if (rc) {
			DEV_ERR("Init FAILED: hdmi_msm_state_create, rc=%d\n",
				rc);
			goto error;
		}
	} else
		DEV_ERR("Init FAILED: failed to add fb device\n");

	DEV_INFO("HDMI HPD: ON\n");
#ifndef CONFIG_VIDEO_MHL_TABLET_V1
	rc = hdmi_msm_hpd_on(true);
	if (rc)
		goto error;
#endif
	hdmi_msm_set_txphy_init();

	if (hdmi_msm_has_hdcp())
		external_common_state->present_hdcp = TRUE;
	else {
		external_common_state->present_hdcp = FALSE;
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
		/*
		 * If the device is not hdcp capable do
		 * not start hdcp timer.
		 */
		del_timer(&hdmi_msm_state->hdcp_timer);
#endif
	}

//	dev_set_drvdata(&pdev->dev, hdmi_msm_state);

        rc = device_create_file(&pdev->dev, &dev_attr_hpd_onoff);
        if (rc < 0)
                DEV_ERR("failed to add hpd_onoff sysfs entries\n");

#ifndef CONFIG_VIDEO_MHL_TABLET_V1
	queue_work(hdmi_work_queue, &hdmi_msm_state->hpd_read_work);
#endif
	hdmi_msm_state->pd->cec_power(0);

	wake_lock_init(&hdmi_msm_state->wake_lock, WAKE_LOCK_SUSPEND, "hdmi_msm");
	
	return 0;

error:
	if (hdmi_msm_state->qfprom_io)
		iounmap(hdmi_msm_state->qfprom_io);
	hdmi_msm_state->qfprom_io = NULL;

	if (hdmi_msm_state->hdmi_io)
		iounmap(hdmi_msm_state->hdmi_io);
	hdmi_msm_state->hdmi_io = NULL;

	external_common_state_remove();

	if (hdmi_msm_state->hdmi_app_clk)
		clk_put(hdmi_msm_state->hdmi_app_clk);
	if (hdmi_msm_state->hdmi_m_pclk)
		clk_put(hdmi_msm_state->hdmi_m_pclk);
	if (hdmi_msm_state->hdmi_s_pclk)
		clk_put(hdmi_msm_state->hdmi_s_pclk);

	hdmi_msm_state->hdmi_app_clk = NULL;
	hdmi_msm_state->hdmi_m_pclk = NULL;
	hdmi_msm_state->hdmi_s_pclk = NULL;

	return rc;
}

static int __devexit hdmi_msm_remove(struct platform_device *pdev)
{
	DEV_INFO("HDMI device: remove\n");

	DEV_INFO("HDMI HPD: OFF\n");
	hdmi_msm_hpd_off();
	free_irq(hdmi_msm_state->irq, NULL);

	if (hdmi_msm_state->qfprom_io)
		iounmap(hdmi_msm_state->qfprom_io);
	hdmi_msm_state->qfprom_io = NULL;

	if (hdmi_msm_state->hdmi_io)
		iounmap(hdmi_msm_state->hdmi_io);
	hdmi_msm_state->hdmi_io = NULL;

	external_common_state_remove();

	if (hdmi_msm_state->hdmi_app_clk)
		clk_put(hdmi_msm_state->hdmi_app_clk);
	if (hdmi_msm_state->hdmi_m_pclk)
		clk_put(hdmi_msm_state->hdmi_m_pclk);
	if (hdmi_msm_state->hdmi_s_pclk)
		clk_put(hdmi_msm_state->hdmi_s_pclk);

	hdmi_msm_state->hdmi_app_clk = NULL;
	hdmi_msm_state->hdmi_m_pclk = NULL;
	hdmi_msm_state->hdmi_s_pclk = NULL;

	kfree(hdmi_msm_state);
	hdmi_msm_state = NULL;

	return 0;
}

static int hdmi_msm_hpd_feature(int on)
{
	int rc = 0;

	DEV_INFO("%s: %d\n", __func__, on);
	hdmi_msm_state->boot_state = FALSE;
	
	if (on)
		rc = hdmi_msm_hpd_on(true);
	else
		hdmi_msm_hpd_off();
	
	/*Reduce the power consumption in idle*/
	if(!hdmi_msm_state->dock_state)
		hdmi_msm_hpd_off();

	return rc;
}


#ifdef CONFIG_SUSPEND
static int hdmi_msm_device_pm_suspend(struct device *dev)
{
	mutex_lock(&hdmi_msm_state_mutex);
	if (hdmi_msm_state->pm_suspended) {
		mutex_unlock(&hdmi_msm_state_mutex);
		return 0;
	}

	DEV_DBG("pm_suspend\n");

	del_timer(&hdmi_msm_state->hpd_state_timer);
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
	del_timer(&hdmi_msm_state->hdcp_timer);
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT */

	disable_irq(hdmi_msm_state->irq);
	clk_disable(hdmi_msm_state->hdmi_app_clk);
	clk_disable(hdmi_msm_state->hdmi_m_pclk);
	clk_disable(hdmi_msm_state->hdmi_s_pclk);

	hdmi_msm_state->pm_suspended = TRUE;
	mutex_unlock(&hdmi_msm_state_mutex);

	hdmi_msm_powerdown_phy();
	hdmi_msm_state->pd->enable_5v(0);
	hdmi_msm_state->pd->core_power(0, 1);
	return 0;
}

static int hdmi_msm_device_pm_resume(struct device *dev)
{
	mutex_lock(&hdmi_msm_state_mutex);
	if (!hdmi_msm_state->pm_suspended) {
		mutex_unlock(&hdmi_msm_state_mutex);
		return 0;
	}

	DEV_DBG("pm_resume\n");
	
	if(hdmi_msm_state->dock_state)
	{
		hdmi_msm_state->pd->core_power(1, 1);
		hdmi_msm_state->pd->enable_5v(1);
		clk_enable(hdmi_msm_state->hdmi_app_clk);
		clk_enable(hdmi_msm_state->hdmi_m_pclk);
		clk_enable(hdmi_msm_state->hdmi_s_pclk);
	}
	hdmi_msm_state->pm_suspended = FALSE;
	mutex_unlock(&hdmi_msm_state_mutex);
	enable_irq(hdmi_msm_state->irq);
	return 0;
}
#else
#define hdmi_msm_device_pm_suspend	NULL
#define hdmi_msm_device_pm_resume	NULL
#endif

static const struct dev_pm_ops hdmi_msm_device_pm_ops = {
	.suspend = hdmi_msm_device_pm_suspend,
	.resume = hdmi_msm_device_pm_resume,
};

static struct platform_driver this_driver = {
	.probe = hdmi_msm_probe,
	.remove = hdmi_msm_remove,
	.driver.name = "hdmi_msm",
	.driver.pm = &hdmi_msm_device_pm_ops,
};

static struct msm_fb_panel_data hdmi_msm_panel_data = {
	.on = hdmi_msm_power_on,
	.off = hdmi_msm_power_off,
};

static struct platform_device this_device = {
	.name = "hdmi_msm",
	.id = 1,
	.dev.platform_data = &hdmi_msm_panel_data,
};

static int __init hdmi_msm_init(void)
{
	int rc;

	hdmi_msm_setup_video_mode_lut();
	hdmi_msm_state = kzalloc(sizeof(*hdmi_msm_state), GFP_KERNEL);
	if (!hdmi_msm_state) {
		pr_err("hdmi_msm_init FAILED: out of memory\n");
		rc = -ENOMEM;
		goto init_exit;
	}

	external_common_state = &hdmi_msm_state->common;
#ifdef CONFIG_VIDEO_MHL_TABLET_V1
	external_common_state->video_resolution = HDMI_VFRMT_1920x1080p30_16_9;
#else
	external_common_state->video_resolution = HDMI_VFRMT_1920x1080p60_16_9;	
#endif
#ifdef CONFIG_FB_MSM_HDMI_3D
	external_common_state->switch_3d = hdmi_msm_switch_3d;
#endif

	/*
	 * Create your work queue
	 * allocs and returns ptr
	*/
	hdmi_work_queue = create_workqueue("hdmi_hdcp");
	external_common_state->hpd_feature = hdmi_msm_hpd_feature;

	rc = platform_driver_register(&this_driver);
	if (rc) {
		pr_err("hdmi_msm_init FAILED: platform_driver_register rc=%d\n",
		       rc);
		goto init_exit;
	}

	hdmi_common_init_panel_info(&hdmi_msm_panel_data.panel_info);
	init_completion(&hdmi_msm_state->ddc_sw_done);
	INIT_WORK(&hdmi_msm_state->hpd_state_work, hdmi_msm_hpd_state_work);
	INIT_WORK(&hdmi_msm_state->hpd_read_work, hdmi_msm_hpd_read_work);
#ifdef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
	init_completion(&hdmi_msm_state->hdcp_success_done);
	INIT_WORK(&hdmi_msm_state->hdcp_reauth_work, hdmi_msm_hdcp_reauth_work);
	INIT_WORK(&hdmi_msm_state->hdcp_work, hdmi_msm_hdcp_work);
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT */

	rc = platform_device_register(&this_device);
	if (rc) {
		pr_err("hdmi_msm_init FAILED: platform_device_register rc=%d\n",
		       rc);
		platform_driver_unregister(&this_driver);
		goto init_exit;
	}
	hdmi_msm_state->boot_state = TRUE;
	pr_debug("%s: success:"
#ifdef DEBUG
		" DEBUG"
#else
		" RELEASE"
#endif
		" AUDIO EDID HPD HDCP"
#ifndef CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT
		":0"
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_HDCP_SUPPORT */
		" DVI"
#ifndef CONFIG_FB_MSM_HDMI_MSM_PANEL_DVI_SUPPORT
		":0"
#endif /* CONFIG_FB_MSM_HDMI_MSM_PANEL_DVI_SUPPORT */
		"\n", __func__);

	return 0;

init_exit:
	kfree(hdmi_msm_state);
	hdmi_msm_state = NULL;

	return rc;
}

static void __exit hdmi_msm_exit(void)
{
	platform_device_unregister(&this_device);
	platform_driver_unregister(&this_driver);
}

module_init(hdmi_msm_init);
module_exit(hdmi_msm_exit);

MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.3");
MODULE_AUTHOR("Qualcomm Innovation Center, Inc.");
MODULE_DESCRIPTION("HDMI MSM TX driver");
