/*
 * Copyright (C) 2011 Samsung Electronics
 *
 * Authors: Adam Hampson <ahampson@sta.samsung.com>
 *          Erik Gilling <konkers@android.com>
 *
 * Additional contributions by : Shankar Bandal <shankar.b@samsung.com>
 *                               Dharam Kumar <dharam.kr@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/sii9234.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/power_supply.h>


#include "sii9234_driver.h"

#ifdef CONFIG_MHL_SWING_LEVEL
#include <linux/ctype.h>
#endif
/* enable debug msg */
#ifdef pr_debug
#undef pr_debug
#define pr_debug pr_info
#endif

int fsa9480_mhl_chg_status_cb(void);
static u8 sii9234_tmds_control(struct sii9234_data *sii9234, bool enable);
static bool cbus_command_request(struct sii9234_data *sii9234,
				 enum cbus_command command,
				 u8 offset, u8 data);
static void cbus_command_response(struct sii9234_data *sii9234);
static irqreturn_t sii9234_irq_thread(int irq, void *data);
static struct cbus_packet cbus_pkt_buf[CBUS_PKT_BUF_COUNT];

#ifdef CONFIG_MHL_SWING_LEVEL
static ssize_t sii9234_swing_test_show(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct sii9234_data *sii9234 = dev_get_drvdata(sii9244_mhldev);
	return sprintf(buf, "mhl_show_value : 0x%x\n",
					sii9234->pdata->swing_level);

}
static ssize_t sii9234_swing_test_store(struct device *dev,
				struct device_attribute *attr,
					const char *buf, size_t size)
{
	struct sii9234_data *sii9234 = dev_get_drvdata(sii9244_mhldev);
	char temp[4] = { 0, };
	const char *p = buf;
	int data, clk;
	unsigned int value;

	while (*p != '\0') {
		if (!isspace(*p))
			strncat(temp, p, 1);
		p++;
	}

	if (strlen(temp) != 2)
		return -EINVAL;

	kstrtoul(temp, 10, &value);
	data = value / 10;
	clk = value % 10;
	sii9234->pdata->swing_level = 0xc0;
	sii9234->pdata->swing_level = sii9234->pdata->swing_level
							| (data << 3) | clk;
	sprintf(buf, "mhl_store_value : 0x%x\n", sii9234->pdata->swing_level);
	return size;
}

static CLASS_ATTR(swing, 0664,
		sii9234_swing_test_show, sii9234_swing_test_store);
#endif

int fsa9480_mhl_chg_status_cb(void)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;
	int ret;
	if (!psy) {
		pr_err("%s: psy is null!!\n", __func__);
		return 0;
	}

	ret = psy->get_property(psy, POWER_SUPPLY_PROP_STATUS, &value);
	if (ret < 0) {
		pr_err("%s: failed to get voltage now\n", __func__);
		return 0;
	}

	switch (value.intval) {
	case POWER_SUPPLY_STATUS_CHARGING:
		pr_err("%s: POWER_SUPPLY_STATUS_CHARGING %d\n", __func__, value.intval);
		break;
	case POWER_SUPPLY_STATUS_DISCHARGING:
		pr_err("%s: POWER_SUPPLY_STATUS_DISCHARGING %d\n", __func__, value.intval);
		break;
	case POWER_SUPPLY_STATUS_NOT_CHARGING:
		pr_err("%s: POWER_SUPPLY_STATUS_NOT_CHARGING %d\n", __func__, value.intval);
		break;
	case POWER_SUPPLY_STATUS_FULL:
		pr_err("%s: POWER_SUPPLY_STATUS_FULL %d\n", __func__, value.intval);
		break;
	default:
		pr_err("%s: unknown value %d\n", __func__, value.intval);
		break;
	}
	return value.intval;
}

u8 mhl_onoff_ex(bool onoff)
{
	struct sii9234_data *sii9234 = dev_get_drvdata(sii9244_mhldev);

	pr_debug("sii9234: mhl_onoff_ex(%d)\n", onoff);

	if (!sii9234 || !sii9234->pdata) {
		pr_info("mhl_onoff_ex: getting resource is failed\n");
		return 2;
	}

	if (sii9234->pdata->power_state == onoff) {
		pr_info("mhl_onoff_ex: mhl already %s\n", onoff ? "on" : "off");
		return 2;
	}

	sii9234->pdata->power_state = onoff; /*save power state*/

	if (sii9234->pdata->mhl_sel)
		sii9234->pdata->mhl_sel(onoff);

	if (onoff) {
		if (sii9234->pdata->hw_onoff)
			sii9234->pdata->hw_onoff(1);

		if (sii9234->pdata->hw_reset)
			sii9234->pdata->hw_reset();
#if defined(CONFIG_MHL_D3_SUPPORT)
		sii9234->power_mode = MHL_POWER_MODE_OFF;
#endif
		sii9234_detection_callback();
	} else {
		sii9234_cancel_callback();

		if (sii9234->pdata->hw_onoff)
			sii9234->pdata->hw_onoff(0);
#if defined(CONFIG_MHL_D3_SUPPORT)
		sii9234->power_mode = MHL_POWER_MODE_OFF;
#endif
	}
	return sii9234->rgnd;
}

EXPORT_SYMBOL(mhl_onoff_ex);

static int mhl_tx_write_reg(struct sii9234_data *sii9234, unsigned int offset,
		u8 value)
{
	return i2c_smbus_write_byte_data(sii9234->pdata->mhl_tx_client, offset,
			value);
}

static int mhl_tx_read_reg(struct sii9234_data *sii9234, unsigned int offset,
		u8 *value)
{
	int ret;

	if (!value)
		return -EINVAL;

	ret = i2c_smbus_write_byte(sii9234->pdata->mhl_tx_client, offset);
	if (ret < 0)
		return ret;

	ret = i2c_smbus_read_byte(sii9234->pdata->mhl_tx_client);
	if (ret < 0)
		return ret;

	*value = ret & 0x000000FF;

	return 0;
}

static int mhl_tx_set_reg(struct sii9234_data *sii9234, unsigned int offset,
		u8 mask)
{
	int ret;
	u8 value;

	ret = mhl_tx_read_reg(sii9234, offset, &value);
	if (ret < 0)
		return ret;

	value |= mask;

	return mhl_tx_write_reg(sii9234, offset, value);
}

static int mhl_tx_clear_reg(struct sii9234_data *sii9234, unsigned int offset,
		u8 mask)
{
	int ret;
	u8 value;

	ret = mhl_tx_read_reg(sii9234, offset, &value);
	if (ret < 0)
		return ret;

	value &= ~mask;

	return mhl_tx_write_reg(sii9234, offset, value);
}

static int tpi_write_reg(struct sii9234_data *sii9234, unsigned int offset,
		u8 value)
{
	return i2c_smbus_write_byte_data(sii9234->pdata->tpi_client, offset,
			value);
}

static int tpi_read_reg(struct sii9234_data *sii9234, unsigned int offset,
		u8 *value)
{
	int ret;

	if (!value)
		return -EINVAL;

	ret = i2c_smbus_write_byte(sii9234->pdata->tpi_client, offset);
	if (ret < 0)
		return ret;

	ret = i2c_smbus_read_byte(sii9234->pdata->tpi_client);
	if (ret < 0)
		return ret;

	*value = ret & 0x000000FF;

	return 0;
}

static int hdmi_rx_write_reg(struct sii9234_data *sii9234, unsigned int offset,
		u8 value)
{
	return i2c_smbus_write_byte_data(sii9234->pdata->hdmi_rx_client, offset,
			value);
}

static int cbus_write_reg(struct sii9234_data *sii9234, unsigned int offset,
		u8 value)
{
	return i2c_smbus_write_byte_data(sii9234->pdata->cbus_client, offset,
			value);
}

static int cbus_read_reg(struct sii9234_data *sii9234, unsigned int offset,
		u8 *value)
{
	int ret;

	if (!value)
		return -EINVAL;

	ret = i2c_smbus_write_byte(sii9234->pdata->cbus_client, offset);
	if (ret < 0)
		return ret;

	ret = i2c_smbus_read_byte(sii9234->pdata->cbus_client);
	if (ret < 0)
		return ret;

	*value = ret & 0x000000FF;

	return 0;
}

static int cbus_set_reg(struct sii9234_data *sii9234, unsigned int offset,
		u8 mask)
{
	int ret;
	u8 value;

	ret = cbus_read_reg(sii9234, offset, &value);
	if (ret < 0)
		return ret;

	value |= mask;

	return cbus_write_reg(sii9234, offset, value);
}

static int mhl_wake_toggle(struct sii9234_data *sii9234,
		unsigned long high_period,
		unsigned long low_period)
{
	int ret;

	/* These bits are not documented. */
	ret = mhl_tx_set_reg(sii9234, MHL_TX_DISC_CTRL7_REG, (1<<7) | (1<<6));
	if (ret < 0)
		return ret;

	usleep_range(high_period * USEC_PER_MSEC, high_period * USEC_PER_MSEC);

	ret = mhl_tx_clear_reg(sii9234, MHL_TX_DISC_CTRL7_REG, (1<<7) | (1<<6));
	if (ret < 0)
		return ret;

	usleep_range(low_period * USEC_PER_MSEC, low_period * USEC_PER_MSEC);

	return 0;
}

static int mhl_send_wake_pulses(struct sii9234_data *sii9234)
{
	int ret;

	ret = mhl_wake_toggle(sii9234, T_SRC_WAKE_PULSE_WIDTH_1,
			T_SRC_WAKE_PULSE_WIDTH_1);
	if (ret < 0)
		return ret;

	ret = mhl_wake_toggle(sii9234, T_SRC_WAKE_PULSE_WIDTH_1,
			T_SRC_WAKE_PULSE_WIDTH_2);
	if (ret < 0)
		return ret;

	ret = mhl_wake_toggle(sii9234, T_SRC_WAKE_PULSE_WIDTH_1,
			T_SRC_WAKE_PULSE_WIDTH_1);
	if (ret < 0)
		return ret;

	ret = mhl_wake_toggle(sii9234, T_SRC_WAKE_PULSE_WIDTH_1,
			T_SRC_WAKE_TO_DISCOVER);
	if (ret < 0)
		return ret;

	return 0;
}

static int sii9234_cbus_reset(struct sii9234_data *sii9234)
{
	int ret;
	u8 idx;
	/* Reset CBUS */
	ret = mhl_tx_set_reg(sii9234, MHL_TX_SRST, 0x03);
	if (ret < 0)
		return ret;

	msleep(T_SRC_CBUS_DEGLITCH);

	ret = mhl_tx_clear_reg(sii9234, MHL_TX_SRST, 0x03);
	if (ret < 0)
		return ret;

	for (idx = 0; idx < 4; idx++) {
		/* Enable WRITE_STAT interrupt for writes to all
				 4 MSC Status registers.*/
		cbus_write_reg(sii9234, 0xE0 + idx, 0xF2);

		/*Enable SET_INT interrupt for writes to all
					4 MSC Interrupt registers.*/
		cbus_write_reg(sii9234, 0xF0 + idx, 0xF2);
	}

	return 0;

}

/* require to chek mhl imformation of samsung in cbus_init_register*/
static int sii9234_cbus_init(struct sii9234_data *sii9234)
{
	u8 value;

	cbus_write_reg(sii9234, 0x07, 0xF2);

	cbus_write_reg(sii9234, 0x40, 0x03);
	cbus_write_reg(sii9234, 0x42, 0x06);
	cbus_write_reg(sii9234, 0x36, 0x0C);
	cbus_write_reg(sii9234, 0x3D, 0xFD);

	cbus_write_reg(sii9234, 0x1C, 0x01);
	cbus_write_reg(sii9234, 0x1D, 0x0F);

	cbus_write_reg(sii9234, 0x44, 0x02);

	/* Setup our devcap*/
	cbus_write_reg(sii9234, 0x80, 0x00);/*To meet cts 6.3.10.1 spec*/
	cbus_write_reg(sii9234, 0x81, 0x11);/*mhl version 1.1*/
	cbus_write_reg(sii9234, 0x82, 0x02);
	cbus_write_reg(sii9234, 0x83, 0x1);	/* adopter id SAMSUNG 321 -> 0x141 */
	cbus_write_reg(sii9234, 0x84, 0x41);/* adopter id SAMSUNG 321 -> 0x141 */
	cbus_write_reg(sii9234, 0x85, 0x01);/*ONLY SUPP_RGB444*/
	cbus_write_reg(sii9234, 0x86, 0x01);
	cbus_write_reg(sii9234, 0x87, 0);
	cbus_write_reg(sii9234, 0x88, (1<<7));
	cbus_write_reg(sii9234, 0x89, 0x0F);
	cbus_write_reg(sii9234, 0x8A, (1<<0) | (1<<1) | (1<<2));
	cbus_write_reg(sii9234, 0x8B, 0);
	cbus_write_reg(sii9234, 0x8C, 0);
	cbus_write_reg(sii9234, 0x8D, 16);
	cbus_write_reg(sii9234, 0x8E, 0x33);/*6.Interrupt Register size and Status Register size is 0x00 for 1 byte, up to 0x0F for 16 bytes.*/
	cbus_write_reg(sii9234, 0x8F, 0);

	cbus_read_reg(sii9234, 0x31, &value);
	value |= 0x0C;
	cbus_write_reg(sii9234, 0x31, value);
	cbus_write_reg(sii9234, 0x30, 0x01);
	cbus_read_reg(sii9234, 0x3C, &value);
	value &= ~0x38;
	value |= 0x30;
	cbus_write_reg(sii9234, 0x3C, value);

	cbus_read_reg(sii9234, 0x22, &value);
	value &= ~0x0F;
	value |= 0x0D;
	cbus_write_reg(sii9234, 0x22, value);

	cbus_read_reg(sii9234, 0x2E, &value);
	value |= 0x15;
	cbus_write_reg(sii9234, 0x2E, value);

	cbus_write_reg(sii9234, CBUS_INTR1_ENABLE_REG, 0);
	cbus_write_reg(sii9234, CBUS_INTR2_ENABLE_REG, 0);

	return 0;
}

static void cbus_req_abort_error(struct sii9234_data *sii9234)
{
	u8 abort_reason = 0;

	pr_debug("sii9234: MSC Request Aborted:");

	cbus_read_reg(sii9234, MSC_REQ_ABORT_REASON_REG, &abort_reason);

	if (abort_reason) {
		if (abort_reason & BIT_MSC_XFR_ABORT) {
			cbus_read_reg(sii9234, MSC_REQ_ABORT_REASON_REG,
							&abort_reason);
			pr_cont("ABORT_REASON_REG = %d\n", abort_reason);
			cbus_write_reg(sii9234, MSC_REQ_ABORT_REASON_REG, 0xff);
		}
		if (abort_reason & BIT_MSC_ABORT) {
			cbus_read_reg(sii9234, BIT_MSC_ABORT, &abort_reason);
			pr_cont("BIT_MSC_ABORT = %d\n", abort_reason);
			cbus_write_reg(sii9234, BIT_MSC_ABORT, 0xff);
		}
		if (abort_reason & ABORT_BY_PEER)
			pr_cont(" Peer Sent an ABORT");
		if (abort_reason & UNDEF_CMD)
			pr_cont(" Undefined Opcode");
		if (abort_reason & TIMEOUT)
			pr_cont(" Requestor Translation layer Timeout");
		if (abort_reason & PROTO_ERROR)
			pr_cont(" Protocol Error");
		if (abort_reason & MAX_FAIL) {
			u8 msc_retry_thr_val = 0;
			pr_cont(" Retry Threshold exceeded");
			cbus_read_reg(sii9234,
					MSC_RETRY_FAIL_LIM_REG,
					&msc_retry_thr_val);
			pr_cont("Retry Threshold value is:%d",
					msc_retry_thr_val);
		}
	}
	pr_cont("\n");
}

static void cbus_resp_abort_error(struct sii9234_data *sii9234)
{
	u8 abort_reason = 0;

	pr_debug("sii9234: MSC Response Aborted:");
	cbus_read_reg(sii9234,
			MSC_RESP_ABORT_REASON_REG,
			&abort_reason);

	if (abort_reason) {
		if (abort_reason & ABORT_BY_PEER)
			pr_cont(" Peer Sent an ABORT");
		if (abort_reason & UNDEF_CMD)
			pr_cont(" Undefined Opcode");
		if (abort_reason & TIMEOUT)
			pr_cont(" Requestor Translation layer Timeout");
	}
	pr_cont("\n");
}

static void force_usb_id_switch_open(struct sii9234_data *sii9234)
{
	/*Disable CBUS discovery*/
	mhl_tx_clear_reg(sii9234, MHL_TX_DISC_CTRL1_REG, (1<<0));
	/*Force USB ID switch to open*/
	mhl_tx_set_reg(sii9234, MHL_TX_DISC_CTRL6_REG, USB_ID_OVR);

	mhl_tx_set_reg(sii9234, MHL_TX_DISC_CTRL3_REG, 0x86);
	/*Force upstream HPD to 0 when not in MHL mode.*/
	mhl_tx_clear_reg(sii9234, MHL_TX_INT_CTRL_REG, (1<<4) | (1<<5));
}

static void release_usb_id_switch_open(struct sii9234_data *sii9234)
{
	msleep(T_SRC_CBUS_FLOAT);
	/* clear USB ID switch to open*/
	mhl_tx_clear_reg(sii9234, MHL_TX_DISC_CTRL6_REG, USB_ID_OVR);
	/* Enable CBUS discovery*/
	mhl_tx_set_reg(sii9234, MHL_TX_DISC_CTRL1_REG, (1<<0));
}

static bool cbus_ddc_abort_error(struct sii9234_data *sii9234)
{
	u8 val1, val2;

	/* clear the ddc abort counter */
	cbus_write_reg(sii9234, 0x29, 0xFF);
	cbus_read_reg(sii9234, 0x29, &val1);
	usleep_range(3000, 4000);
	cbus_read_reg(sii9234, 0x29, &val2);
	if (val2 > (val1 + 50)) {
		pr_debug("Applying DDC Abort Safety(SWA 18958)\n)");
		mhl_tx_set_reg(sii9234, MHL_TX_SRST, (1<<3));
		mhl_tx_clear_reg(sii9234, MHL_TX_SRST, (1<<3));
		force_usb_id_switch_open(sii9234);
		release_usb_id_switch_open(sii9234);
		mhl_tx_write_reg(sii9234, MHL_TX_MHLTX_CTL1_REG, 0xD0);
		sii9234_tmds_control(sii9234, false);
		/* Disconnect and notify to OTG */
		return true;
	}
	pr_debug("sii9234: DDC abort interrupt\n");

	return false;
}

#ifdef CONFIG_SII9234_RCP
static void rcp_uevent_report(struct sii9234_data *sii9234, u8 key)
{
	pr_info("rcp_uevent_report key: %d\n", key);
	input_report_key(sii9234->input_dev, (unsigned int)key+1, 1);
	input_report_key(sii9234->input_dev, (unsigned int)key+1, 0);
	input_sync(sii9234->input_dev);
}

/*
 * is_rcp_code_valid: Validdates the recevied RCP key,
 * valid key is 1 to 1  map to fwk keylayer file sii9234_rcp.kl
 * located at (/system/usr/keylayout/sii9234_rcp.kl).
 *
 * New key support needs to be update is_rcp_key_code_valid at
 * driver side and /system/usr/keylayout/sii9234_rcp.kl at fwk side.
 */

static int is_rcp_key_code_valid(u8 key)
{
	switch (key+1)	{
	/*should resemble /system/usr/keylayout/sii9234_rcp.kl*/
	case 1:		/* ENTER		WAKE_DROPPED*/
	case 2:		/* DPAD_UP		WAKE_DROPPED*/
	case 3:		/* DPAD_DOWN		WAKE_DROPPED*/
	case 4:		/* DPAD_LEFT		WAKE_DROPPED*/
	case 5:		/* DPAD_RIGHT		WAKE_DROPPED*/
	case 10:	/* MENU			WAKE_DROPPED*/
	case 14:	/* BACK			WAKE_DROPPED*/
	case 33:	/* 0	*/
	case 34:	/* 1	*/
	case 35:	/* 2	*/
	case 36:	/* 3	*/
	case 37:	/* 4	*/
	case 38:	/* 5	*/
	case 39:	/* 6	*/
	case 40:	/* 7	*/
	case 41:	/* 8	*/
	case 42:	/* 9	*/
	case 43:    /* ENTER*/
	case 45:	/* DEL	*/
	case 69:    /* MEDIA_PLAY_PAUSE		WAKE*/
	case 70:    /* MEDIA_STOP		WAKE*/
	case 71:    /* MEDIA_PAUSE		WAKE*/
	case 73:    /* MEDIA_REWIND		WAKE*/
	case 74:    /* MEDIA_FAST_FORWARD	WAKE*/
	case 76:    /* MEDIA_NEXT		WAKE*/
	case 77:    /* MEDIA_PREVIOUS		WAKE*/
		return 1;
	default:
		return 0;
	}

}

static void cbus_process_rcp_key(struct sii9234_data *sii9234, u8 key)
{

	if (is_rcp_key_code_valid(key)) {
		/* Report the key */
		rcp_uevent_report(sii9234, key);
		/* Send the RCP ack */
		cbus_command_request(sii9234, CBUS_MSC_MSG,
						MSG_RCPK, key);
	} else {
		sii9234->error_key = key;
		/*
		* Send a RCPE(RCP Error Message) to Peer followed by
		* RCPK with old key-code so that initiator(TV) can
		* recognize failed key code.error code = 0x01 means
		* Ineffective key code was received.
		* See Table 21.(PRM)for details.
		*/
		cbus_command_request(sii9234, CBUS_MSC_MSG, MSG_RCPE, 0x01);
	}
}
#endif

static void cbus_process_rap_key(struct sii9234_data *sii9234, u8 key)
{
	if (CBUS_MSC_RAP_CONTENT_ON == key)
		sii9234_tmds_control(sii9234, true);
	else if (CBUS_MSC_RAP_CONTENT_OFF == key)
		sii9234_tmds_control(sii9234, false);

	cbus_command_request(sii9234, CBUS_MSC_MSG, MSG_RAPK, 0x00);
}

/*
 * Incoming MSC_MSG : RCP/RAP/RCPK/RCPE/RAPK commands
 *
 * Process RCP key codes and the send supported keys to userspace.
 * If a key is not supported then an error ack is sent to the peer.  Note
 * that by default all key codes are supported.
 *
 * An alternate method might be to decide the validity of the key in the
 * driver itself.  However, the driver does not have any criteria to which
 * to make this decision.
 */
static void cbus_handle_msc_msg(struct sii9234_data *sii9234)
{
	u8 cmd_code, key;

	mutex_lock(&sii9234->cbus_lock);
	if (sii9234->state != STATE_ESTABLISHED) {
		pr_debug("sii9234: invalid MHL state\n");
		mutex_unlock(&sii9234->cbus_lock);
		return ;
	}

	cbus_read_reg(sii9234, CBUS_MSC_MSG_CMD_IN_REG, &cmd_code);
	cbus_read_reg(sii9234, CBUS_MSC_MSG_DATA_IN_REG, &key);

	pr_info("sii9234: cmd_code:%d, key:%d\n", cmd_code, key);

	switch (cmd_code) {
	case MSG_RCP:
		pr_debug("sii9234: RCP Arrived. KEY CODE:%d\n", key);
		mutex_unlock(&sii9234->cbus_lock);
		cbus_process_rcp_key(sii9234, key);
		return;
	case MSG_RAP:
		pr_debug("sii9234: RAP Arrived\n");
		mutex_unlock(&sii9234->cbus_lock);
		cbus_process_rap_key(sii9234, key);
		return;
	case MSG_RCPK:
		pr_debug("sii9234: RCPK Arrived\n");
		break;
	case MSG_RCPE:
		pr_debug("sii9234: RCPE Arrived\n");
		break;
	case MSG_RAPK:
		pr_debug("sii9234: RAPK Arrived\n");
		break;
	default:
		pr_debug("sii9234: MAC error\n");
		mutex_unlock(&sii9234->cbus_lock);
		cbus_command_request(sii9234, CBUS_GET_MSC_ERR_CODE, 0, 0);
		return;
	}
	mutex_unlock(&sii9234->cbus_lock);
}

void mhl_path_enable(struct sii9234_data *sii9234, bool path_en)
{
	pr_debug("sii9234: mhl_path_enable MHL_STATUS_PATH_ENABLED,"
					" path_en=%d !!!\n", path_en);

	if (path_en)
		sii9234->mhl_status_value.linkmode |= MHL_STATUS_PATH_ENABLED;
		else
		sii9234->mhl_status_value.linkmode &= ~MHL_STATUS_PATH_ENABLED;

	cbus_command_request(sii9234, CBUS_WRITE_STAT, CBUS_LINK_CONTROL_2_REG,
					sii9234->mhl_status_value.linkmode);
}

#if 0/*not used*/
static void cbus_handle_wrt_burst_recd(struct sii9234_data *sii9234)
{
	pr_debug("sii9234: CBUS WRT_BURST_RECD\n");
}
#endif

static void cbus_handle_wrt_stat_recd(struct sii9234_data *sii9234)
{
	u8 status_reg0, status_reg1;

	pr_debug("sii9234: CBUS WRT_STAT_RECD\n");

	/*
	* The two MHL status registers need to read to ensure that the MSC is
	* ready to receive the READ_DEVCAP command.
	* The READ_DEVCAP command is need to determine the dongle power state
	* and whether RCP, RCPE, RCPK, RAP, and RAPE are supported.
	*
	* Note that this is not documented properly in the PRM.
	 */

	cbus_read_reg(sii9234, CBUS_MHL_STATUS_REG_0, &status_reg0);
	cbus_write_reg(sii9234, CBUS_MHL_STATUS_REG_0, 0xFF);
	cbus_read_reg(sii9234, CBUS_MHL_STATUS_REG_1, &status_reg1);
	cbus_write_reg(sii9234, CBUS_MHL_STATUS_REG_1, 0xFF);

	pr_debug("sii9234: STATUS_REG0 : [%d];STATUS_REG1 : [%d]\n",
			status_reg0, status_reg1);

	if (!(sii9234->mhl_status_value.linkmode & MHL_STATUS_PATH_ENABLED) &&
				(MHL_STATUS_PATH_ENABLED & status_reg1)) {
		mhl_path_enable(sii9234, true);
	} else if ((sii9234->mhl_status_value.linkmode
			 & MHL_STATUS_PATH_ENABLED) &&
				!(MHL_STATUS_PATH_ENABLED & status_reg1)) {
		mhl_path_enable(sii9234, false);
	}

	if (status_reg0 & MHL_STATUS_DCAP_READY) {
		pr_debug("sii9234: DEV CAP READY\n");
		cbus_command_request(sii9234, CBUS_READ_DEVCAP,
						DEVCAP_DEV_CAT, 0x00);
		cbus_command_request(sii9234, CBUS_READ_DEVCAP,
						DEVCAP_DEV_FEATURE_FLAG, 0x00);
	}
}

static void cbus_handle_set_int_recd(struct sii9234_data *sii9234)
{
	u8 intr_reg0, intr_reg1, value;

	/* read and clear interrupt*/
	cbus_read_reg(sii9234, CBUS_MHL_INTR_REG_0, &intr_reg0);
	cbus_write_reg(sii9234, CBUS_MHL_INTR_REG_0, intr_reg0);

	cbus_read_reg(sii9234, CBUS_MHL_INTR_REG_1, &intr_reg1);
	cbus_write_reg(sii9234, CBUS_MHL_INTR_REG_1, intr_reg1);

	pr_debug("sii9234: INTR_REG0 : [%d]; INTR_REG1 : [%d]\n",
			intr_reg0, intr_reg1);

	if (intr_reg0 & MHL_INT_DCAP_CHG) {
		pr_debug("sii9234: MHL_INT_DCAP_CHG\n");
		cbus_command_request(sii9234, CBUS_READ_DEVCAP,
				DEVCAP_DEV_CAT, 0x00);
		cbus_command_request(sii9234, CBUS_READ_DEVCAP,
				DEVCAP_DEV_FEATURE_FLAG, 0x00);
	}

	if (intr_reg0 & MHL_INT_DSCR_CHG)
		pr_debug("sii9234:  MHL_INT_DSCR_CHG\n");

	if (intr_reg0 & MHL_INT_REQ_WRT) {
		pr_debug("sii9234:  MHL_INT_REQ_WRT\n");
		cbus_command_request(sii9234, CBUS_SET_INT,
					 MHL_RCHANGE_INT, MHL_INT_GRT_WRT);
	}

	if (intr_reg0 & MHL_INT_GRT_WRT)
		pr_debug("sii9234:  MHL_INT_GRT_WRT\n");

	if (intr_reg1 & MHL_INT_EDID_CHG) {
		/* Enable Overriding HPD OUT */
		mhl_tx_set_reg(sii9234, MHL_TX_INT_CTRL_REG, (1<<4));

		/*
		 * As per HDMI specification to indicate EDID change
		 * in TV (or sink), we need to toggle HPD line.
		 */

		/* HPD OUT = Low */
		mhl_tx_clear_reg(sii9234, MHL_TX_INT_CTRL_REG, (1<<5));

		/* A SET_HPD command shall not follow a CLR_HPD command
		 * within less than THPD_WIDTH(50ms).
		 */
		msleep(T_HPD_WIDTH);

		/* HPD OUT = High */
		mhl_tx_set_reg(sii9234, MHL_TX_INT_CTRL_REG, (1<<5));

		/* Disable Overriding of HPD OUT */
		mhl_tx_clear_reg(sii9234, MHL_TX_INT_CTRL_REG, (1<<4));
	}

	/* clear SET_INT_RECD interrupt */
	cbus_read_reg(sii9234, CBUS_MHL_INTR_REG_2, &value);
	cbus_write_reg(sii9234, CBUS_MHL_INTR_REG_2, value);

	cbus_read_reg(sii9234, CBUS_MHL_INTR_REG_3, &value);
	cbus_write_reg(sii9234, CBUS_MHL_INTR_REG_3, value);
}

static int sii9234_power_init(struct sii9234_data *sii9234)
{
	int ret;

	/* Force the SiI9234 into the D0 state. */
	ret = tpi_write_reg(sii9234, TPI_DPD_REG, 0x3F);
	if (ret < 0) {
		pr_info("sii9234: %s: tpi i2c operation failure @ %d \n", __func__, __LINE__);
		return ret;
		}

	/* Enable TxPLL Clock */
	ret = hdmi_rx_write_reg(sii9234, HDMI_RX_TMDS_CLK_EN_REG, 0x01);
	if (ret < 0) {
		pr_info("sii9234: %s: hdmi i2c operation failure @ %d \n", __func__, __LINE__);
		return ret;
		}

	/* Enable Tx Clock Path & Equalizer*/
	ret = hdmi_rx_write_reg(sii9234, HDMI_RX_TMDS_CH_EN_REG, 0x15);
	if (ret < 0) {
		pr_info("sii9234: %s: hdmi i2c operaion failure @ %d \n", __func__, __LINE__);
		return ret;
		}

	/* Power Up TMDS*/
	ret = mhl_tx_write_reg(sii9234, 0x08, 0x35);
	if (ret < 0) {
		pr_info("sii9234: %s: mhl i2c operation failure @ %d \n", __func__, __LINE__);
		return ret;
		}
	return 0;
}

static void sii9234_hdmi_init(struct sii9234_data *sii9234)
{
	/* Analog PLL Control
	 * bits 5:4 = 2b00 as per characterization team.
	 */
	hdmi_rx_write_reg(sii9234, HDMI_RX_TMDS0_CCTRL1_REG, 0xC1);

	/* PLL Calrefsel */
	hdmi_rx_write_reg(sii9234, HDMI_RX_PLL_CALREFSEL_REG, 0x03);

	/* VCO Cal */
	hdmi_rx_write_reg(sii9234, HDMI_RX_PLL_VCOCAL_REG, 0x20);

	/* Auto EQ */
	hdmi_rx_write_reg(sii9234, HDMI_RX_EQ_DATA0_REG, 0x8A);

	/* Auto EQ */
	hdmi_rx_write_reg(sii9234, HDMI_RX_EQ_DATA1_REG, 0x6A);

	/* Auto EQ */
	hdmi_rx_write_reg(sii9234, HDMI_RX_EQ_DATA2_REG, 0xAA);

	/* Auto EQ */
	hdmi_rx_write_reg(sii9234, HDMI_RX_EQ_DATA3_REG, 0xCA);

	/* Auto EQ */
	hdmi_rx_write_reg(sii9234, HDMI_RX_EQ_DATA4_REG, 0xEA);

	/* Manual zone */
	hdmi_rx_write_reg(sii9234, HDMI_RX_TMDS_ZONE_CTRL_REG, 0xA0);

	/* PLL Mode Value */
	hdmi_rx_write_reg(sii9234, HDMI_RX_TMDS_MODE_CTRL_REG, 0x00);

	mhl_tx_write_reg(sii9234, MHL_TX_TMDS_CCTRL, 0x34);

	hdmi_rx_write_reg(sii9234, 0x45, 0x44);

	/* Rx PLL BW ~ 4MHz */
	hdmi_rx_write_reg(sii9234, 0x31, 0x0A);

	/* Analog PLL Control
	 * bits 5:4 = 2b00 as per characterization team.
	 */
	hdmi_rx_write_reg(sii9234, HDMI_RX_TMDS0_CCTRL1_REG, 0xC1);
}

static void sii9234_mhl_tx_ctl_int(struct sii9234_data *sii9234)
{
	mhl_tx_write_reg(sii9234, MHL_TX_MHLTX_CTL1_REG, 0xD0);
	mhl_tx_write_reg(sii9234, MHL_TX_MHLTX_CTL2_REG, 0xFC);
	mhl_tx_write_reg(sii9234, MHL_TX_MHLTX_CTL4_REG,
					sii9234->pdata->swing_level);
	mhl_tx_write_reg(sii9234, MHL_TX_MHLTX_CTL7_REG, 0x0C);
}

static void sii9234_power_down(struct sii9234_data *sii9234)
{
	disable_irq_nosync(sii9234->pdata->mhl_tx_client->irq);

	if (sii9234->claimed) {
		if (sii9234->pdata->vbus_present)
			sii9234->pdata->vbus_present(false);
	}

	sii9234->state = STATE_DISCONNECTED;
	sii9234->claimed = false;

	tpi_write_reg(sii9234, TPI_DPD_REG, 0);
	/*turn on&off hpd festure for only QCT HDMI*/
	mhl_hpd_handler(false);
}
#if defined(CONFIG_MHL_D3_SUPPORT)
static void vbus_watchdog_state_work(struct work_struct *work)
{
	struct sii9234_data *sii9234 = dev_get_drvdata(sii9244_mhldev);
	if (!get_vbus_valid())
				mhl_onoff_ex(0);
		else
		queue_delayed_work(sii9234->vbus_workqueue, &sii9234->d3_timer_work,
						msecs_to_jiffies(2000));
}
#endif
int rsen_state_timer_out(struct sii9234_data *sii9234)
{
	int ret = 0;
	u8 value;

	ret = mhl_tx_read_reg(sii9234, MHL_TX_SYSSTAT_REG, &value);
	if (ret < 0)
		goto err_exit;
	sii9234->rsen = value & RSEN_STATUS;

#if 0//defined(CONFIG_KOR_MODEL_SHV_E160S) || defined(CONFIG_KOR_MODEL_SHV_E160K) || defined (CONFIG_KOR_MODEL_SHV_E160L) || defined (CONFIG_JPN_MODEL_SC_05D)
	pr_info("sii9234: jgk:%s() - ignore MHL_TX_SYSSTAT_REG\n", __func__);
#else
	if (value & RSEN_STATUS) {
		pr_info("sii9234: MHL cable connected.. RESN High\n");
	} else {
		pr_info("sii9234: RSEN lost\n");
		msleep(T_SRC_RXSENSE_DEGLITCH);
		ret = mhl_tx_read_reg(sii9234, MHL_TX_SYSSTAT_REG, &value);
		if (ret < 0)
			goto err_exit;

		pr_info("sys_stat: %x ~\n", value);
		if ((value & RSEN_STATUS) == 0)	{
			pr_info("RSEN Really LOW ~\n");
			/*To meet CTS 3.3.22.2 spec*/
			sii9234_tmds_control(sii9234, false);
			force_usb_id_switch_open(sii9234);
			release_usb_id_switch_open(sii9234);
			ret = -1;
			goto err_exit;
		}
	}
#endif
	return ret;

err_exit:
	/*turn off mhl and change usb_sel to usb*/
#if defined(CONFIG_MHL_D3_SUPPORT)
/*Go to D3 Mode Transition from D3->D0 When Rground Measures 1K*/
	sii9234->power_mode = MHL_POWER_MODE_OFF;
	INIT_WORK(&sii9234->redetect_work, (void *)sii9234_detection_callback);
	disable_irq_nosync(sii9234->pdata->mhl_tx_client->irq);
	if (sii9234->pdata->hw_reset)
		sii9234->pdata->hw_reset();
	schedule_work(&sii9234->redetect_work);
#else
	mhl_onoff_ex(0);
#endif
	return ret;
}

void tx_goto_d3(struct sii9234_data *sii9234)
{
	u8 value = 0;
	pr_debug("tx_goto_d3\n");

	force_usb_id_switch_open(sii9234);

	mhl_tx_clear_reg(sii9234, MHL_TX_DISC_CTRL4_REG,
					BIT7 | BIT6 | BIT5 | BIT4);
	mhl_tx_clear_reg(sii9234, MHL_TX_DISC_CTRL5_REG, BIT1 | BIT0);
	release_usb_id_switch_open(sii9234);

	/*Force HPD to LOW*/
	mhl_tx_clear_reg(sii9234, MHL_TX_INT_CTRL_REG, BIT5);
	mhl_tx_set_reg(sii9234, MHL_TX_INT_CTRL_REG, BIT4);

	hdmi_rx_write_reg(sii9234, 0x01, 0x03);
	tpi_read_reg(sii9234, 0x3D, &value);
	value &= ~BIT0;
	tpi_write_reg(sii9234, 0x3D, value);
}

static int sii9234_detection_callback(void)
{
	struct sii9234_data *sii9234 = dev_get_drvdata(sii9244_mhldev);
	int ret;
	u8 value;
	int handled = 0;

	pr_debug("sii9234: detection started\n");

	mutex_lock(&sii9234->lock);
	sii9234->mhl_status_value.linkmode = MHL_STATUS_CLK_MODE_NORMAL;
	sii9234->rgnd = RGND_UNKNOWN;
	sii9234->state = STATE_DISCONNECTED;
	sii9234->rsen = false;
	memset(cbus_pkt_buf, 0x00, sizeof(cbus_pkt_buf));
	ret = sii9234_power_init(sii9234);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}
	ret = sii9234_cbus_reset(sii9234);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}
	sii9234_hdmi_init(sii9234);

	sii9234_mhl_tx_ctl_int(sii9234);

	/* Enable HDCP Compliance safety*/
	ret = mhl_tx_write_reg(sii9234, 0x2B, 0x01);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}
	/* CBUS discovery cycle time for each drive and float = 150us*/
	ret = mhl_tx_read_reg(sii9234, MHL_TX_DISC_CTRL1_REG, &value);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}
	value &= ~(1<<3);
	value |= (1<<2);
	ret = mhl_tx_write_reg(sii9234, MHL_TX_DISC_CTRL1_REG, value);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}
	/* Clear bit 6 (reg_skip_rgnd) */
	ret = mhl_tx_write_reg(sii9234, MHL_TX_DISC_CTRL2_REG,
			(1<<7) /* Reserved Bit */ |
			2 << ATT_THRESH_SHIFT |
			DEGLITCH_TIME_50MS);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	/* Changed from 66 to 65 for 94[1:0] = 01 = 5k reg_cbusmhl_pup_sel */
	/* 1.8V CBUS VTH & GND threshold */
	/*To meet CTS 3.3.7.2 spec*/
	ret = mhl_tx_write_reg(sii9234, MHL_TX_DISC_CTRL5_REG, 0x77);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	/* set bit 2 and 3, which is Initiator Timeout */
	ret = cbus_read_reg(sii9234, CBUS_LINK_CONTROL_2_REG, &value);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	value |= 0x0C;

	ret = cbus_write_reg(sii9234, CBUS_LINK_CONTROL_2_REG, value);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	ret = mhl_tx_write_reg(sii9234, MHL_TX_MHLTX_CTL6_REG, 0xA0);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	/* RGND & single discovery attempt (RGND blocking) */
	ret = mhl_tx_write_reg(sii9234, MHL_TX_DISC_CTRL6_REG, BLOCK_RGND_INT |
			DVRFLT_SEL | SINGLE_ATT);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	/* Use VBUS path of discovery state machine*/
	ret = mhl_tx_write_reg(sii9234, MHL_TX_DISC_CTRL8_REG, 0);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	ret = mhl_tx_set_reg(sii9234, MHL_TX_DISC_CTRL6_REG, USB_ID_OVR);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	/* To allow RGND engine to operate correctly.
	 * When moving the chip from D2 to D0 (power up, init regs)
	 * the values should be
	 * 94[1:0] = 01  reg_cbusmhl_pup_sel[1:0] should be set for 5k
	 * 93[7:6] = 10  reg_cbusdisc_pup_sel[1:0] should be
	 * set for 10k (default)
	 * 93[5:4] = 00  reg_cbusidle_pup_sel[1:0] = open (default)
	 */
	ret = mhl_tx_set_reg(sii9234, MHL_TX_DISC_CTRL3_REG, 0x86);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	/* change from CC to 8C to match 5K*/
	/*To meet CTS 3.3.72 spec*/
	ret = mhl_tx_set_reg(sii9234, MHL_TX_DISC_CTRL4_REG, 0x8C);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	/* Force upstream HPD to 0 when not in MHL mode */
	mhl_tx_clear_reg(sii9234, MHL_TX_INT_CTRL_REG, (1<<5));
	mhl_tx_set_reg(sii9234, MHL_TX_INT_CTRL_REG, (1<<4));

	/* Configure the interrupt as active high */
	ret = mhl_tx_clear_reg(sii9234, MHL_TX_INT_CTRL_REG, (1<<2) | (1<<1));
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	msleep(25);

	/* release usb_id switch */
	ret = mhl_tx_clear_reg(sii9234, MHL_TX_DISC_CTRL6_REG, USB_ID_OVR);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}
	ret = mhl_tx_write_reg(sii9234, MHL_TX_DISC_CTRL1_REG, 0x27);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	sii9234_cbus_init(sii9234);

	/* Enable Auto soft reset on SCDT = 0*/
	ret = mhl_tx_write_reg(sii9234, 0x05, 0x04);

	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	/* HDMI Transcode mode enable*/
	ret = mhl_tx_write_reg(sii9234, 0x0D, 0x1C);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	ret = mhl_tx_write_reg(sii9234, MHL_TX_INTR4_ENABLE_REG,
			RGND_READY_MASK | CBUS_LKOUT_MASK |
			MHL_DISC_FAIL_MASK | MHL_EST_MASK);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	ret = mhl_tx_write_reg(sii9234, MHL_TX_INTR1_ENABLE_REG,
			       (1<<5) | (1<<6));
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

/* this point is very importand before megsure RGND impedance*/
	force_usb_id_switch_open(sii9234);
	mhl_tx_clear_reg(sii9234, MHL_TX_DISC_CTRL4_REG,
					(1<<7) | (1<<6) | (1<<5) | (1<<4));
	mhl_tx_clear_reg(sii9234, MHL_TX_DISC_CTRL5_REG, (1<<1) | (1<<0));
	release_usb_id_switch_open(sii9234);
/*end of this*/
#if defined(CONFIG_MHL_D3_SUPPORT)
	/*Go to D3 Mode Transition from D3->D0 When Rground Measures 1K*/
	if (sii9234->power_mode == MHL_POWER_MODE_OFF) {
		pr_debug("sii9234: Go to D3 and waiting for RGND measurement\n");
		/* Force upstream HPD to 0 when not in MHL mode */
		mhl_tx_clear_reg(sii9234, MHL_TX_INT_CTRL_REG, (1<<5));
		mhl_tx_set_reg(sii9234, MHL_TX_INT_CTRL_REG, (1<<4));
		hdmi_rx_write_reg(sii9234, 0x01, 0x03);
		ret = tpi_read_reg(sii9234, 0x3D, &value);
		if (ret<0)	{
			pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
			goto unhandled;
			}
		value &= ~BIT0;
		msleep(20);
		ret = tpi_write_reg(sii9234, 0x3D, value);
		if (ret<0)	{
			pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
			goto unhandled;
			}
		sii9234->power_mode = MHL_POWER_MODE_D3;
		enable_irq(sii9234->pdata->mhl_tx_client->irq);
		queue_delayed_work(sii9234->vbus_workqueue, &sii9234->d3_timer_work,
						msecs_to_jiffies(2000));
		mutex_unlock(&sii9234->lock);
		return handled;
	}
#endif

	pr_debug("sii9234: waiting for RGND measurement\n");
	enable_irq(sii9234->pdata->mhl_tx_client->irq);

	/* SiI9244 Programmer's Reference Section 2.4.3
	 * State : RGND Ready
	 */
	mutex_unlock(&sii9234->lock);
	ret = wait_event_timeout(sii9234->wq,
				 ((sii9234->rgnd != RGND_UNKNOWN) ||
				  mhl_state_is_error(sii9234->state)),
				 msecs_to_jiffies(T_WAIT_TIMEOUT_RGND_INT*1.5));

	mutex_lock(&sii9234->lock);
	if (ret == 0 || mhl_state_is_error(sii9234->state)) {
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	if (sii9234->rgnd != RGND_1K) {
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	mutex_unlock(&sii9234->lock);

	pr_debug("sii9234: waiting for detection\n");
	ret = wait_event_timeout(sii9234->wq,
				 sii9234->state != STATE_DISCONNECTED,
				 msecs_to_jiffies(T_WAIT_TIMEOUT_DISC_INT*2));
	mutex_lock(&sii9234->lock);
	if (ret == 0) {
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	if (sii9234->state == STATE_DISCOVERY_FAILED) {
		handled = -1;
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
	}

	if (mhl_state_is_error(sii9234->state)) {
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	mutex_unlock(&sii9234->lock);
	pr_info("sii9234: Established & start to moniter RSEN\n");
	/*CTS 3.3.14.3 Discovery;Sink Never Drives MHL+/- HIGH*/
	/*MHL SPEC 8.2.1.1.1;Transition SRC4-SRC7*/
	if (rsen_state_timer_out(sii9234) < 0)
		return handled;

	sii9234->claimed = true;

	mutex_lock(&sii9234->lock);
	ret = cbus_write_reg(sii9234,
			CBUS_INTR1_ENABLE_REG,
			MSC_RESP_ABORT_MASK |
			MSC_REQ_ABORT_MASK |
			MSC_REQ_DONE_MASK |
			MSC_MSG_RECD_MASK |
			CBUS_DDC_ABORT_MASK);
	if (ret < 0) {
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	ret = cbus_write_reg(sii9234,
			CBUS_INTR2_ENABLE_REG,
			WRT_STAT_RECD_MASK |
			SET_INT_RECD_MASK);
	if (ret < 0){
	pr_debug("sii9234: %s : unhandled dbg @ %d \n", __func__, __LINE__);
		goto unhandled;
}

	mutex_unlock(&sii9234->lock);

	if (sii9234->pdata->vbus_present)
		sii9234->pdata->vbus_present(true);

	return handled;

unhandled:
	pr_info("sii9234: Detection failed");
	if (sii9234->state == STATE_DISCONNECTED) {
		pr_cont(" (timeout)");
		mutex_unlock(&sii9234->lock);
#ifndef CONFIG_MHL_D3_SUPPORT
		mhl_onoff_ex(0);
#endif
		return handled;
	} else if (sii9234->state == STATE_DISCOVERY_FAILED)
		pr_cont(" (discovery failed)");
	else if (sii9234->state == STATE_CBUS_LOCKOUT)
		pr_cont(" (cbus_lockout)");
	pr_cont("\n");

	/*mhl spec: 8.3.3, if discovery failed, must retry discovering*/
	if ((sii9234->state == STATE_DISCOVERY_FAILED) &&
						 (sii9234->rgnd == RGND_1K)) {
		pr_cont("Discovery failed but RGND_1K impedence"
					" restart detection_callback");
		INIT_WORK(&sii9234->redetect_work, (void *)sii9234_detection_callback);
		disable_irq_nosync(sii9234->pdata->mhl_tx_client->irq);

		if (sii9234->pdata->hw_reset)
			sii9234->pdata->hw_reset();

		schedule_work(&sii9234->redetect_work);
	}

	mutex_unlock(&sii9234->lock);

	return handled;
}

static void sii9234_cancel_callback(void)
{
	struct sii9234_data *sii9234 = dev_get_drvdata(sii9244_mhldev);

	mutex_lock(&sii9234->lock);
	sii9234_power_down(sii9234);
	mutex_unlock(&sii9234->lock);
}

static void save_cbus_pkt_to_buffer(struct sii9234_data *sii9234)
{
	int index;

	for (index = 0; index < CBUS_PKT_BUF_COUNT; index++)
		if (sii9234->cbus_pkt_buf[index].status == false)
			break;

	if (index == CBUS_PKT_BUF_COUNT) {
		pr_debug("sii9234: Error save_cbus_pkt Buffer Full\n");
		index -= 1; /*adjust index*/
	}

	pr_debug("sii9234: save_cbus_pkt_to_buffer index = %d\n", index);
	memcpy(&sii9234->cbus_pkt_buf[index], &sii9234->cbus_pkt,
					sizeof(struct cbus_packet));
	sii9234->cbus_pkt_buf[index].status = true;
}

static void cbus_command_response(struct sii9234_data *sii9234)
{
	u8 value, offset = 0;

	mutex_lock(&sii9234->cbus_lock);
	pr_debug("sii9234: cbus_command_response\n");

	switch (sii9234->cbus_pkt.command) {
	case CBUS_MSC_MSG:
		pr_debug("sii9234: cbus_command_response Received"
					" ACK for CBUS_MSC_MSG\n");
#ifdef CONFIG_SII9234_RCP
		if (sii9234->cbus_pkt.data[0] == MSG_RCPE &&
					sii9234->cbus_pkt.data[1] == 0x01) {
			sii9234->cbus_pkt.command = CBUS_IDLE;
			mutex_unlock(&sii9234->cbus_lock);
			cbus_command_request(sii9234, CBUS_MSC_MSG, MSG_RCPK,
							sii9234->error_key);
			return;
		}
#endif
		break;
	case CBUS_WRITE_STAT:
		pr_debug("sii9234: cbus_command_response"
					"CBUS_WRITE_STAT\n");
		cbus_read_reg(sii9234, CBUS_MSC_FIRST_DATA_IN_REG,
					&sii9234->cbus_pkt.data[0]);
		break;
	case CBUS_SET_INT:
		pr_debug("sii9234: cbus_command_response CBUS_SET_INT\n");
		if (sii9234->cbus_pkt.offset == MHL_RCHANGE_INT &&
				sii9234->cbus_pkt.data[0] == MHL_INT_DSCR_CHG) {
			/*Write burst final step... Req->GRT->Write->DSCR*/
			pr_debug("sii9234: MHL_RCHANGE_INT &"
						"MHL_INT_DSCR_CHG\n");
		} else if (sii9234->cbus_pkt.offset == MHL_RCHANGE_INT &&
				sii9234->cbus_pkt.data[0] == MHL_INT_DCAP_CHG) {
			pr_debug("sii9234: MHL_RCHANGE_INT &"
						"MHL_INT_DCAP_CHG\n");
			sii9234->cbus_pkt.command = CBUS_IDLE;
			mutex_unlock(&sii9234->cbus_lock);
			cbus_command_request(sii9234, CBUS_WRITE_STAT,
						MHL_STATUS_REG_CONNECTED_RDY,
						MHL_STATUS_DCAP_READY);
			return;
		}
		break;
	case CBUS_WRITE_BURST:
		pr_debug("sii9234: cbus_command_response"
						"MHL_WRITE_BURST\n");
		sii9234->cbus_pkt.command = CBUS_IDLE;
		mutex_unlock(&sii9234->cbus_lock);
		cbus_command_request(sii9234, CBUS_SET_INT,
					 MHL_RCHANGE_INT, MHL_INT_DSCR_CHG);
		return;
	case CBUS_READ_DEVCAP:
		pr_debug("sii9234: cbus_command_response"
					" CBUS_READ_DEVCAP\n");
		cbus_read_reg(sii9234, CBUS_MSC_FIRST_DATA_IN_REG,
								 &value);
		switch (sii9234->cbus_pkt.offset) {
		case DEVCAP_MHL_VERSION:
			sii9234->devcap.mhl_ver = value;
			pr_debug("sii9234: MHL_VERSION: %X\n", value);
			break;
		case DEVCAP_DEV_CAT:
			if (value & MHL_DEV_CATEGORY_POW_BIT)
				pr_debug("sii9234: CAT=POWERED");
			else
				pr_debug("sii9234: CAT=UNPOWERED");
				break;
		case DEVCAP_ADOPTER_ID_H:
			sii9234->devcap.adopter_id = (value & 0xFF) << 0x8;
			pr_debug("sii9234: DEVCAP_ADOPTER_ID_H = %X\n", value);
			break;
		case DEVCAP_ADOPTER_ID_L:
			sii9234->devcap.adopter_id |= value & 0xFF;
			pr_debug("sii9234: DEVCAP_ADOPTER_ID_L = %X\n", value);
				break;
		case DEVCAP_VID_LINK_MODE:
			sii9234->devcap.vid_link_mode = 0x3F & value;
			pr_debug("sii9234: MHL_CAP_VID_LINK_MODE = %d\n",
					 sii9234->devcap.vid_link_mode);
			break;
		case DEVCAP_AUD_LINK_MODE:
			sii9234->devcap.aud_link_mode = 0x03 & value;
			pr_debug("sii9234: DEVCAP_AUD_LINK_MODE =%d\n",
					sii9234->devcap.aud_link_mode);
			break;
		case DEVCAP_VIDEO_TYPE:
			sii9234->devcap.video_type = 0x8F & value;
			pr_debug("sii9234: DEVCAP_VIDEO_TYPE =%d\n",
					sii9234->devcap.video_type);
			break;
		case DEVCAP_LOG_DEV_MAP:
			sii9234->devcap.log_dev_map = value;
			pr_debug("sii9234: DEVCAP_LOG_DEV_MAP =%d\n",
					sii9234->devcap.log_dev_map);
			break;
		case DEVCAP_BANDWIDTH:
			sii9234->devcap.bandwidth = value;
			pr_debug("sii9234: DEVCAP_BANDWIDTH =%d\n",
					sii9234->devcap.bandwidth);
			break;
		case DEVCAP_DEV_FEATURE_FLAG:
			if ((value & MHL_FEATURE_RCP_SUPPORT) == 0)
				pr_debug("sii9234: FEATURE_FLAG=RCP");

			if ((value & MHL_FEATURE_RAP_SUPPORT) == 0)
				pr_debug("sii9234: FEATURE_FLAG=RAP\n");

			if ((value & MHL_FEATURE_SP_SUPPORT) == 0)
				pr_debug("sii9234: FEATURE_FLAG=SP\n");
			break;
		case DEVCAP_DEVICE_ID_H:
			sii9234->devcap.device_id = (value & 0xFF) << 0x8;
			pr_info("sii9234: DEVICE_ID_H=0x%x\n", value);
			offset = DEVCAP_DEVICE_ID_L;
			break;
		case DEVCAP_DEVICE_ID_L:
			sii9234->devcap.device_id |= value & 0xFF;
			pr_info("sii9234: DEVICE_ID_L=0x%x\n", value);
			break;
		case DEVCAP_SCRATCHPAD_SIZE:
			  sii9234->devcap.scratchpad_size = value;
			  pr_debug("sii9234: DEVCAP_SCRATCHPAD_SIZE =%d\n",
					sii9234->devcap.scratchpad_size);
			  break;
		case DEVCAP_INT_STAT_SIZE:
			  sii9234->devcap.int_stat_size = value;
			  pr_debug("sii9234: DEVCAP_INT_STAT_SIZE =%d\n",
					sii9234->devcap.int_stat_size);
			  break;
		case DEVCAP_RESERVED:
			pr_info("sii9234: DEVCAP_RESERVED : %d\n", value);
			break;
		case DEVCAP_DEV_STATE:
			pr_debug("sii9234: DEVCAP_DEV_STATE\n");
			break;
		default:
			pr_debug("sii9234: DEVCAP DEFAULT\n");
			 break;
		}

		break;
	default:
		pr_debug("sii9234: error: cbus_command_response"
					"cannot handle...\n");
	}

	sii9234->cbus_pkt.command = CBUS_IDLE;
	mutex_unlock(&sii9234->cbus_lock);

	if (offset)
		cbus_command_request(sii9234, CBUS_READ_DEVCAP,
						offset, 0x00);
}

#ifdef DEBUG_MHL
static void cbus_command_response_dbg_msg(struct sii9234_data *sii9234,
							 u8 index)
{
	/*Added to debugcbus_pkt_buf*/
	pr_info("sii9234: cbus_pkt_buf[index].command = %d,"
			"sii9234->cbus_pkt.command = %d\n",
					sii9234->cbus_pkt_buf[index].command,
					sii9234->cbus_pkt.command);
	pr_info("sii9234: cbus_pkt_buf[index].data[0] = %d,"
			"sii9234->cbus_pkt.data[0] =  %d\n",
					sii9234->cbus_pkt_buf[index].data[0],
					sii9234->cbus_pkt.data[0]);

	pr_info("sii9234: cbus_pkt_buf[index].data[1] = %d,"
			 "sii9234->cbus_pkt.data[1] = %d\n",
					sii9234->cbus_pkt_buf[index].data[1],
					sii9234->cbus_pkt.data[1]);
	pr_info("sii9234: cbus_pkt_buf[index].offset = %d,"
			"sii9234->cbus_pkt.offset = %d\n",
					sii9234->cbus_pkt_buf[index].offset,
					sii9234->cbus_pkt.offset);
}
#endif

static void cbus_command_response_all(struct sii9234_data *sii9234)
{
	u8 index;
	struct cbus_packet cbus_pkt_process_buf[CBUS_PKT_BUF_COUNT];
	pr_debug("sii9234: cbus_command_response_all for All requests\n");

	/*take bkp of cbus_pkt_buf*/
	memcpy(cbus_pkt_process_buf, sii9234->cbus_pkt_buf,
			sizeof(cbus_pkt_process_buf));

	/*clear cbus_pkt_buf  to hold next request*/
	memset(sii9234->cbus_pkt_buf, 0x00, sizeof(sii9234->cbus_pkt_buf));

	/*process all previous requests*/
	for (index = 0; index < CBUS_PKT_BUF_COUNT; index++) {
		if (cbus_pkt_process_buf[index].status == true) {
			memcpy(&sii9234->cbus_pkt, &cbus_pkt_process_buf[index],
						sizeof(struct cbus_packet));
			cbus_command_response(sii9234);
#ifdef DEBUG_MHL
			/*print cbus_cmd messg*/
			cbus_command_response_dbg_msg(sii9234, index);
#endif
		}
	}
}

static bool cbus_command_request(struct sii9234_data *sii9234,
				 enum cbus_command command, u8 offset, u8 data)
{
	u8 start_bit = 0;

	mutex_lock(&sii9234->cbus_lock);
	if (sii9234->state != STATE_ESTABLISHED) {
		pr_debug("sii9234: cbus_command_request without establish\n");
		pr_debug("sii9234: ==> command:0x%X, offset:0x%X",
							command, offset);
			mutex_unlock(&sii9234->cbus_lock);
			return -EINVAL;
	}

	sii9234->cbus_pkt.command = command;
	sii9234->cbus_pkt.offset = offset;

	if (command == CBUS_MSC_MSG)
		sii9234->cbus_pkt.data[0] = offset;
	else
			sii9234->cbus_pkt.data[0] = data;

	pr_debug("sii9234: cbus_command_request Sending MSC_MSG SubCommand=%d"
				",key-code=%d\n", sii9234->cbus_pkt.offset,
						sii9234->cbus_pkt.data[0]);

	cbus_write_reg(sii9234, CBUS_MSC_OFFSET_REG,
						sii9234->cbus_pkt.offset);
	cbus_write_reg(sii9234, CBUS_MSC_FIRST_DATA_OUT_REG,
					sii9234->cbus_pkt.data[0]);

	switch (sii9234->cbus_pkt.command) {
	case CBUS_SET_INT:
		pr_debug("sii9234: cbus_command_request"
						"CBUS_SET_INT\n");
		start_bit = START_BIT_WRITE_STAT_INT;
		break;
	case CBUS_WRITE_STAT:
		pr_debug("sii9234: cbus_command_request"
						"CBUS_WRITE_STAT\n");
		start_bit = START_BIT_WRITE_STAT_INT;
		break;
	case CBUS_MSC_MSG:
		/*treat offset as data[0] in case of CBUS_MSC_MSG*/
		sii9234->cbus_pkt.data[0] = offset;
		sii9234->cbus_pkt.data[1] = data;
		pr_debug("sii9234: cbus_command_request CBUS_MSC_MSG"
					"SubCommand=%d,key-code=%d\n",
					sii9234->cbus_pkt.data[0],
					sii9234->cbus_pkt.data[1]);

		cbus_write_reg(sii9234, CBUS_MSC_SECOND_DATA_OUT_REG,
					sii9234->cbus_pkt.data[1]);
		cbus_write_reg(sii9234, CBUS_MSC_OFFSET_REG,
					sii9234->cbus_pkt.command);
		start_bit = START_BIT_MSC_MSG;
		break;
	case CBUS_READ_DEVCAP:
		pr_debug("sii9234: cbus_command_request CBUS_READ_DEVCAP\n");
		start_bit = START_BIT_READ_DEVCAP;
		break;
	case CBUS_WRITE_BURST:
		pr_debug("sii9234: cbus_command_request CBUS_WRITE_BURST\n");
		start_bit = START_BIT_WRITE_BURST;
		break;
	case CBUS_GET_STATE:
	case CBUS_GET_VENDOR_ID:
	case CBUS_SET_HPD:
	case CBUS_CLR_HPD:
	case CBUS_GET_MSC_ERR_CODE:
	case CBUS_GET_SC3_ERR_CODE:
	case CBUS_GET_SC1_ERR_CODE:
	case CBUS_GET_DDC_ERR_CODE:
		cbus_write_reg(sii9234, CBUS_MSC_OFFSET_REG,
					sii9234->cbus_pkt.command);
		start_bit = START_BIT_MSC_RESERVED;
		break;
	default:
		pr_debug("sii9234: error send cbus command fail\n");
		mutex_unlock(&sii9234->cbus_lock);
		return false;
	}

	pr_debug("sii9234: startbit = %d\n", start_bit);
	cbus_write_reg(sii9234, CBUS_MSC_COMMAND_START_REG, start_bit);
	save_cbus_pkt_to_buffer(sii9234);
	mutex_unlock(&sii9234->cbus_lock);

	return true;
}

static u8 sii9234_tmds_control(struct sii9234_data *sii9234, bool enable)
{
	u8 ret;

	if (enable) {
		ret = mhl_tx_set_reg(sii9234, MHL_TX_TMDS_CCTRL, (1<<4));
		pr_debug("sii9234: MHL HPD High, enabled TMDS\n");
		ret = mhl_tx_set_reg(sii9234, MHL_TX_INT_CTRL_REG,
							(1<<4) | (1<<5));
	} else {
		ret = mhl_tx_clear_reg(sii9234, MHL_TX_TMDS_CCTRL, (1<<4));
		pr_debug("sii9234 MHL HPD low, disabled TMDS\n");
		ret = mhl_tx_clear_reg(sii9234, MHL_TX_INT_CTRL_REG,
							(1<<4) | (1<<5));
	}

	return ret;
}

static irqreturn_t sii9234_irq_thread(int irq, void *data)
{
	struct sii9234_data *sii9234 = data;
	int ret;
	u8 intr1, intr4, value = 0;
	u8 intr1_en, intr4_en;
	bool release_otg = false;
	u8 cbus_intr1, cbus_intr2;
	u8 mhl_poweroff = 0;
	void (*cbus_resp_callback)(struct sii9234_data *) = NULL;

	if (!sii9234) {
		pr_err("%s: sii9234 is NULL & skipp this rutine\n", __func__);
		return IRQ_HANDLED;
	}
	msleep(30);
	mutex_lock(&sii9234->lock);

	ret = mhl_tx_read_reg(sii9234, MHL_TX_INTR1_REG, &intr1);
	if (ret < 0) {
		printk(KERN_ERR
		       "[ERROR] %s():%d read MHL_TX_INTR1_REG failed !\n",
		       __func__, __LINE__);
		goto i2c_error_exit;
	}
	ret = mhl_tx_read_reg(sii9234, MHL_TX_INTR4_REG, &intr4);
	if (ret < 0) {
		printk(KERN_ERR
		       "[ERROR] %s():%d read MHL_TX_INTR4_REG failed !\n",
		       __func__, __LINE__);
		goto i2c_error_exit;
	}

	ret = mhl_tx_read_reg(sii9234, MHL_TX_INTR1_ENABLE_REG, &intr1_en);
	if (ret < 0) {
		printk(KERN_ERR
		       "[ERROR] %s():%d read MHL_TX_INTR1_ENABLE_REG failed !\n",
		       __func__, __LINE__);
		goto i2c_error_exit;
	}
	ret = mhl_tx_read_reg(sii9234, MHL_TX_INTR4_ENABLE_REG, &intr4_en);
	if (ret < 0) {
		printk(KERN_ERR
		       "[ERROR] %s():%d read MHL_TX_INTR4_ENABLE_REG failed !\n",
		       __func__, __LINE__);
		goto i2c_error_exit;
	}

	ret = cbus_read_reg(sii9234, CBUS_INT_STATUS_1_REG, &cbus_intr1);
	if (ret < 0) {
		printk(KERN_ERR
		       "[ERROR] %s():%d read CBUS_INT_STATUS_1_REG failed !\n",
		       __func__, __LINE__);
		goto i2c_error_exit;
	}
	ret = cbus_read_reg(sii9234, CBUS_INT_STATUS_2_REG, &cbus_intr2);
	if (ret < 0) {
		printk(KERN_ERR
		       "[ERROR] %s():%d read CBUS_INT_STATUS_2_REG failed !\n",
		       __func__, __LINE__);
		goto i2c_error_exit;
	}

	pr_debug("sii9234: irq %02x/%02x %02x/%02x %02x/%02x\n",
			intr1, intr1_en,
			intr4, intr4_en,
			cbus_intr1, cbus_intr2);

	if (intr4 & RGND_READY_INT) {
#if defined(CONFIG_MHL_D3_SUPPORT)
		if (sii9234->power_mode == MHL_POWER_MODE_D3) {
			pr_debug("sii9234_irq: RGND READY\n");
			cancel_delayed_work(&sii9234->d3_timer_work);
			INIT_WORK(&sii9234->redetect_work, (void *)sii9234_detection_callback);
			disable_irq_nosync(sii9234->pdata->mhl_tx_client->irq);

			if (sii9234->pdata->hw_reset)
				sii9234->pdata->hw_reset();
			sii9234->power_mode = MHL_POWER_MODE_D0;
			schedule_work(&sii9234->redetect_work);
			goto err_exit;
		}
#endif

		ret = mhl_tx_read_reg(sii9234, MHL_TX_STAT2_REG, &value);
		if (ret < 0) {
			dev_err(&sii9234->pdata->mhl_tx_client->dev,
					"STAT2 reg, err %d\n", ret);
			goto err_exit;
		}

		switch (value & RGND_INTP_MASK) {
		case RGND_INTP_OPEN:
			pr_debug("sii9234: RGND Open\n");
			sii9234->rgnd = RGND_OPEN;
			break;
		case RGND_INTP_1K:
			pr_debug("sii9234: RGND 1K\n");
			/* After applying RGND patch, there is some issue
			about discovry failure
			This point is add to fix that problem*/
			mhl_tx_set_reg(sii9234, MHL_TX_DISC_CTRL4_REG, 0x8C);
			if (ret < 0) {
				printk(KERN_ERR
				       "[ERROR] %s():%d write MHL_TX_DISC_CTRL4_REG failed !\n",
				       __func__, __LINE__);
				goto i2c_error_exit;
			}
			mhl_tx_write_reg(sii9234, MHL_TX_DISC_CTRL5_REG, 0x77);
			if (ret < 0) {
				printk(KERN_ERR
				       "[ERROR] %s():%d write MHL_TX_DISC_CTRL5_REG failed !\n",
				       __func__, __LINE__);
				goto i2c_error_exit;
			}
			mhl_tx_set_reg(sii9234, MHL_TX_DISC_CTRL6_REG, 0x05);
			if (ret < 0) {
				printk(KERN_ERR
				       "[ERROR] %s():%d write MHL_TX_DISC_CTRL6_REG failed !\n",
				       __func__, __LINE__);
				goto i2c_error_exit;
			}
			usleep_range(T_SRC_VBUS_CBUS_TO_STABLE * USEC_PER_MSEC,
				T_SRC_VBUS_CBUS_TO_STABLE  * USEC_PER_MSEC);
			/* end of this*/
			ret = mhl_send_wake_pulses(sii9234);
			if (ret < 0) {
				pr_err("sii9234: sending wake pulses error\n");
				goto err_exit;
			}
			sii9234->rgnd = RGND_1K;
			break;

		case RGND_INTP_2K:
			pr_debug("sii9234: RGND 2K\n");
			sii9234->rgnd = RGND_2K;
			break;
		case RGND_INTP_SHORT:
			pr_debug("sii9234: RGND Short\n");
			sii9234->rgnd = RGND_SHORT;
		};

		if (sii9234->rgnd != RGND_1K) {
#if defined(CONFIG_MHL_D3_SUPPORT)
			sii9234->power_mode = MHL_POWER_MODE_OFF;
			INIT_WORK(&sii9234->redetect_work, (void *)sii9234_detection_callback);
			disable_irq_nosync(sii9234->pdata->mhl_tx_client->irq);
				if (sii9234->pdata->hw_reset)
					sii9234->pdata->hw_reset();
			schedule_work(&sii9234->redetect_work);
#else
			mhl_poweroff = 1; /*Power down mhl chip */
#endif
			goto err_exit;
		}
	}

	if (intr4 & CBUS_LKOUT_INT) {
		pr_debug("sii9234: CBUS Lockout Interrupt\n");
		sii9234->state = STATE_CBUS_LOCKOUT;
	}

	if (intr4 & MHL_DISC_FAIL_INT) {
		pr_debug("sii9234: MHL_DISC_FAIL_INT\n");
		sii9234->state = STATE_DISCOVERY_FAILED;
		goto err_exit;
	}

	if (intr4 & MHL_EST_INT) {
		pr_debug("sii9234: mhl est interrupt %d\n", value);

		/* discovery override */
		ret = mhl_tx_write_reg(sii9234, MHL_TX_MHLTX_CTL1_REG, 0x10);
		if (ret < 0) {
			printk(KERN_ERR
			       "[ERROR] %s():%d write MHL_TX_MHLTX_CTRL1_REG failed !\n",
			       __func__, __LINE__);
			goto i2c_error_exit;
		}

		/* increase DDC translation layer timer (byte mode) */
		ret = cbus_write_reg(sii9234, 0x07, 0x32);
		if (ret < 0) {
			printk(KERN_ERR
			       "[ERROR] %s():%d cbus_write_reg failed !\n",
			       __func__, __LINE__);
			goto i2c_error_exit;
		}
		ret = cbus_set_reg(sii9234, 0x44, 1 << 1);
		if (ret < 0) {
			printk(KERN_ERR
			       "[ERROR] %s():%d cbus_set_reg failed !\n",
			       __func__, __LINE__);
			goto i2c_error_exit;
		}

		/* Keep the discovery enabled. Need RGND interrupt */
		ret = mhl_tx_set_reg(sii9234, MHL_TX_DISC_CTRL1_REG, (1<<0));
		if (ret < 0) {
			printk(KERN_ERR
			       "[ERROR] %s():%d mhl_tx_set_reg failed !\n",
			       __func__, __LINE__);
			goto i2c_error_exit;
		}

		sii9234->state = STATE_ESTABLISHED;

		cbus_command_request(sii9234, CBUS_SET_INT,
					 MHL_RCHANGE_INT, MHL_INT_DCAP_CHG);

		mhl_tx_write_reg(sii9234, MHL_TX_INTR1_ENABLE_REG,
				RSEN_CHANGE_INT_MASK | HPD_CHANGE_INT_MASK);
	}

	if (intr1 & HPD_CHANGE_INT) {
		ret = cbus_read_reg(sii9234, MSC_REQ_ABORT_REASON_REG, &value);
		if (ret < 0) {
			printk(KERN_ERR
			       "[ERROR] %s():%d cbus_read_reg failed !\n",
			       __func__, __LINE__);
			goto i2c_error_exit;
		}

		if (value & SET_HPD_DOWNSTREAM) {
			pr_info("sii9234: hpd high\n");
			/* Downstream HPD Highi */

			/* Do we need to send HPD upstream using
			 * Register 0x79(page0)? Is HPD need to be overriden??
			 *      TODO: See if we need code for overriding HPD OUT
			 *      as per Page 0,0x79 Register
			 */
			sii9234->mhl_status_value.sink_hpd = true;
			cbus_command_request(sii9234, CBUS_WRITE_STAT,
					CBUS_LINK_CONTROL_2_REG,
					sii9234->mhl_status_value.linkmode);
			/* Enable TMDS */
			sii9234_tmds_control(sii9234, true);
			/*turn on&off hpd festure for only QCT HDMI*/
			mhl_hpd_handler(true);

		} else {
			pr_info("sii9234: hpd low\n");
			/*Downstream HPD Low*/

			/* Similar to above comments.
			 * TODO:Do we need to override HPD OUT value
			 * and do we need to disable TMDS here?
			 */
			sii9234->mhl_status_value.sink_hpd = false;
			/* Disable TMDS */
			sii9234_tmds_control(sii9234, false);
#if 0
			/* in order to handle HDMI cable disconnection while MHL dongle is connected.*/
			mhl_hpd_handler(false);
#endif
		}
	}

	if (intr1 & RSEN_CHANGE_INT) {
		/* work_around code to handle worng interrupt*/
		if (sii9234->rgnd != RGND_1K) {
			pr_info("sii9234: Err RSEN_HIGH without RGND_1K\n");
			goto err_exit2;
		}
		ret = mhl_tx_read_reg(sii9234, MHL_TX_SYSSTAT_REG, &value);
		if (ret < 0) {
			pr_info("sii9234: MHL_TX_SYSSTAT_REG read error\n");
			goto err_exit2;
		}
		sii9234->rsen = value & RSEN_STATUS;

		if (value & RSEN_STATUS) {
			pr_info("sii9234: MHL cable connected.. RESN High\n");
		} else {
			pr_info("sii9234: RSEN lost -");
			/* Once RSEN loss is confirmed,we need to check
			 * based on cable status and chip power status,whether
			 * it is SINK Loss(HDMI cable not connected, TV Off)
			 * or MHL cable disconnection
			 * TODO: Define the below mhl_disconnection()
			 */
			msleep(T_SRC_RXSENSE_DEGLITCH);
			ret = mhl_tx_read_reg(sii9234, MHL_TX_SYSSTAT_REG,
								 &value);
			if (ret < 0) {
				printk(KERN_ERR
					"[ERROR] %s() read MHL_TX_SYSSTAT_REG\n",
					__func__);
				goto i2c_error_exit;
			}
			pr_cont(" sys_stat: %x\n", value);

			if ((value & RSEN_STATUS) == 0)	{
				pr_info("RSEN Really LOW ~\n");
				/*To meet CTS 3.3.22.2 spec*/
				sii9234_tmds_control(sii9234, false);
				force_usb_id_switch_open(sii9234);
				release_usb_id_switch_open(sii9234);
#if defined(CONFIG_MHL_D3_SUPPORT)
				mhl_hpd_handler(false);
				if (!get_vbus_valid()) {
					mhl_poweroff = 1;
					goto err_exit;
				} else
					sii9234->power_mode = MHL_POWER_MODE_OFF;

					INIT_WORK(&sii9234->redetect_work, (void *)sii9234_detection_callback);
					disable_irq_nosync(sii9234->pdata->mhl_tx_client->irq);

					if (sii9234->pdata->hw_reset)
						sii9234->pdata->hw_reset();
					schedule_work(&sii9234->redetect_work);
#else
				mhl_poweroff = 1; /*Power down mhl chip */
#endif
				goto err_exit;
			}
		}
	}

	/*
	 * Process CBUS interrupts only when MHL connection has been
	 * established
	 */
	if (sii9234->state == STATE_ESTABLISHED) {

		if (cbus_intr1 & MSC_RESP_ABORT)
			cbus_resp_abort_error(sii9234);

		if (cbus_intr1 & MSC_REQ_ABORT)
			cbus_req_abort_error(sii9234);

		if ((cbus_intr1 & CBUS_DDC_ABORT) ||
					 (cbus_intr1 & MSC_RESP_ABORT)) {
			pr_debug("sii9234: CBUS DDC abort\n");
			if (cbus_ddc_abort_error(sii9234)) {
				if (sii9234->claimed == true)
					release_otg = true;
				sii9234_power_down(sii9234);
			}
		}

		if (cbus_intr1 & MSC_REQ_DONE) {
			pr_debug("sii9234: CBUS cmd ACK Received\n");
			cbus_resp_callback = cbus_command_response_all;
		}
#ifdef CONFIG_SII9234_RCP
		if (cbus_intr1 & MSC_MSG_RECD) {
			pr_debug("sii9234: MSC MSG Received\n");
			cbus_handle_msc_msg(sii9234);
		}
#endif

		/* ignore WRT_STAT_RECD interrupt when we get HPD CHANGE */
		if (cbus_intr2 & WRT_STAT_RECD && intr1 == 0)
			cbus_handle_wrt_stat_recd(sii9234);

		if (cbus_intr2 & SET_INT_RECD)
			cbus_handle_set_int_recd(sii9234);
	}

err_exit:
	pr_debug("sii9234: wake_up\n");
	wake_up(&sii9234->wq);
err_exit2:
	ret = mhl_tx_write_reg(sii9234, MHL_TX_INTR1_REG, intr1);
	if (ret < 0)
		goto i2c_error_exit;
	ret = mhl_tx_write_reg(sii9234, MHL_TX_INTR4_REG, intr4);
	if (ret < 0)
		goto i2c_error_exit;

	ret = cbus_write_reg(sii9234, CBUS_INT_STATUS_1_REG, cbus_intr1);
	if (ret < 0)
		goto i2c_error_exit;
	ret = cbus_write_reg(sii9234, CBUS_INT_STATUS_2_REG, cbus_intr2);
	if (ret < 0)
		goto i2c_error_exit;

	mutex_unlock(&sii9234->lock);

	if (cbus_resp_callback)
		cbus_resp_callback(sii9234);

	if (mhl_poweroff)
		mhl_onoff_ex(0);

	return IRQ_HANDLED;
i2c_error_exit:
	mutex_unlock(&sii9234->lock);
	pr_info("%s(): i2c error exit\n", __func__);
	pr_debug("sii9234: wake_up\n");
	wake_up(&sii9234->wq);
	return IRQ_HANDLED;
}

#ifdef MHL_SS_FACTORY
#define SII_ID 0x92
static ssize_t sysfs_check_mhl_command(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int size;
	u8 sii_id = 0;
	struct sii9234_data *sii9234 = dev_get_drvdata(sii9244_mhldev);

	if (sii9234->pdata->hw_onoff)
		sii9234->pdata->hw_onoff(1);

	if (sii9234->pdata->hw_reset)
		sii9234->pdata->hw_reset();

	mhl_tx_read_reg(sii9234, MHL_TX_IDH_REG, &sii_id);
	pr_info("sel_show sii_id: %X\n", sii_id);

	if (sii9234->pdata->hw_onoff)
		sii9234->pdata->hw_onoff(0);

	size = snprintf(buf, 10, "%d\n", sii_id == SII_ID ? 1 : 0);

	return size;

}

static CLASS_ATTR(test_result, 0664 , (void *)sysfs_check_mhl_command, NULL);
#endif /*MHL_SS_FACTORY*/

#ifdef CONFIG_PM
static int sii9234_mhl_tx_suspend(struct device *dev)
{
	struct sii9234_data *sii9234 = dev_get_drvdata(dev);

	/*set config_gpio for mhl*/
	if (sii9234->pdata->gpio_cfg)
		sii9234->pdata->gpio_cfg();

	return 0;
}

static int sii9234_mhl_tx_resume(struct device *dev)
{
	struct sii9234_data *sii9234 = dev_get_drvdata(dev);

	/*set config_gpio for mhl*/
	if (sii9234->pdata->gpio_cfg)
		sii9234->pdata->gpio_cfg();

	return 0;
}

static const struct dev_pm_ops sii9234_pm_ops = {
	.suspend        = sii9234_mhl_tx_suspend,
	.resume         = sii9234_mhl_tx_resume,
};
#endif

static int __devinit sii9234_mhl_tx_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct sii9234_data *sii9234;
#ifdef CONFIG_SII9234_RCP
	struct input_dev *input;
#endif
	int ret;
#ifdef MHL_SS_FACTORY
	struct class *sec_mhl;
#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	sii9234 = kzalloc(sizeof(struct sii9234_data), GFP_KERNEL);
	if (!sii9234) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}
#ifdef CONFIG_SII9234_RCP
	input = input_allocate_device();
	if (!input) {
		dev_err(&client->dev, "failed to allocate input device.\n");
		ret = -ENOMEM;
		goto err_exit0;
	}
#endif

	sii9234->pdata = client->dev.platform_data;
	if (!sii9234->pdata) {
		ret = -EINVAL;
		goto err_exit1;
	}
	sii9234->pdata->mhl_tx_client = client;

	init_waitqueue_head(&sii9234->wq);
	mutex_init(&sii9234->lock);
	mutex_init(&sii9234->cbus_lock);

#if defined(CONFIG_MHL_D3_SUPPORT)
	sii9234->vbus_workqueue = create_singlethread_workqueue("vbus_watchdog");
	INIT_DELAYED_WORK(&sii9234->d3_timer_work, vbus_watchdog_state_work);
#endif
	i2c_set_clientdata(client, sii9234);
	client->irq = sii9234->pdata->get_irq();
	sii9244_mhldev = &client->dev;
	ret = request_threaded_irq(client->irq, NULL, sii9234_irq_thread,
				   IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				   "sii9234", sii9234);
	if (ret < 0)
		goto err_exit1;

	disable_irq(client->irq);
	if (sii9234->pdata->swing_level == 0)
		sii9234->pdata->swing_level = 0xEB;

#ifdef MHL_SS_FACTORY
	pr_info("create mhl sysfile\n");

	sec_mhl = class_create(THIS_MODULE, "mhl");
	if (IS_ERR(sec_mhl)) {
		pr_err("Failed to create class(sec_mhl)!\n");
		goto err_exit1;
	}

	ret = class_create_file(sec_mhl, &class_attr_test_result);
	if (ret) {
		pr_err("Failed to create device file in sysfs entries!\n");
		goto err_exit2;
	}
#endif
#ifdef CONFIG_MHL_SWING_LEVEL
	pr_info("create mhl sysfile\n");

	ret = class_create_file(sec_mhl, &class_attr_swing);
	if (ret) {
		pr_err("failed to create swing sysfs file\n");
		goto err_exit2;
	}
#endif

	sii9234->cbus_pkt.command = CBUS_IDLE;
	sii9234->cbus_pkt.offset = DEVCAP_DEV_STATE;

#ifdef CONFIG_SII9234_RCP
	/* indicate that we generate key events */
	set_bit(EV_KEY, input->evbit);
	bitmap_fill(input->keybit, KEY_MAX);

	sii9234->input_dev = input;
	input_set_drvdata(input, sii9234);
	input->name = "sii9234_rcp";
	input->id.bustype = BUS_I2C;

	ret = input_register_device(input);
	if (ret < 0) {
		dev_err(&client->dev, "fail to register input device\n");
		goto err_exit2;
	}
#endif

	return 0;

err_exit2:
#ifdef MHL_SS_FACTORY
	class_destroy(sec_mhl);
#endif
err_exit1:
#ifdef CONFIG_SII9234_RCP
	input_free_device(input);
#endif
err_exit0:
	kfree(sii9234);
	return ret;
}

static int __devinit sii9234_tpi_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct sii9234_platform_data *pdata = client->dev.platform_data;
	if (!pdata)
		return -EINVAL;
	pdata->tpi_client = client;
	return 0;
}

static int __devinit sii9234_hdmi_rx_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct sii9234_platform_data *pdata = client->dev.platform_data;
	if (!pdata)
		return -EINVAL;

	pdata->hdmi_rx_client = client;
	return 0;
}

static int __devinit sii9234_cbus_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct sii9234_platform_data *pdata = client->dev.platform_data;
	if (!pdata)
		return -EINVAL;

	pdata->cbus_client = client;
	return 0;
}

static int __devexit sii9234_mhl_tx_remove(struct i2c_client *client)
{
	return 0;
}

static int __devexit sii9234_tpi_remove(struct i2c_client *client)
{
	return 0;
}

static int __devexit sii9234_hdmi_rx_remove(struct i2c_client *client)
{
	return 0;
}

static int __devexit sii9234_cbus_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id sii9234_mhl_tx_id[] = {
	{"sii9234_mhl_tx", 0},
	{}
};

static const struct i2c_device_id sii9234_tpi_id[] = {
	{"sii9234_tpi", 0},
	{}
};

static const struct i2c_device_id sii9234_hdmi_rx_id[] = {
	{"sii9234_hdmi_rx", 0},
	{}
};

static const struct i2c_device_id sii9234_cbus_id[] = {
	{"sii9234_cbus", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sii9234_mhl_tx_id);
MODULE_DEVICE_TABLE(i2c, sii9234_tpi_id);
MODULE_DEVICE_TABLE(i2c, sii9234_hdmi_rx_id);
MODULE_DEVICE_TABLE(i2c, sii9234_cbus_id);

static struct i2c_driver sii9234_mhl_tx_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "sii9234_mhl_tx",
#ifdef CONFIG_PM
		.pm	= &sii9234_pm_ops,
#endif
	},
	.id_table = sii9234_mhl_tx_id,
	.probe = sii9234_mhl_tx_i2c_probe,
	.remove = __devexit_p(sii9234_mhl_tx_remove),
	.command = NULL,
};

static struct i2c_driver sii9234_tpi_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "sii9234_tpi",
	},
	.id_table = sii9234_tpi_id,
	.probe = sii9234_tpi_i2c_probe,
	.remove = __devexit_p(sii9234_tpi_remove),
};

static struct i2c_driver sii9234_hdmi_rx_i2c_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sii9234_hdmi_rx",
	},
	.id_table	= sii9234_hdmi_rx_id,
	.probe	= sii9234_hdmi_rx_i2c_probe,
	.remove	= __devexit_p(sii9234_hdmi_rx_remove),
};

static struct i2c_driver sii9234_cbus_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "sii9234_cbus",
	},
	.id_table = sii9234_cbus_id,
	.probe = sii9234_cbus_i2c_probe,
	.remove = __devexit_p(sii9234_cbus_remove),
};

#ifdef CONFIG_BATTERY_SEC
extern unsigned int is_lpcharging_state(void);
#endif

static int __init sii9234_init(void)
{
	int ret;

#ifdef CONFIG_BATTERY_SEC
	if (is_lpcharging_state()) {
		pr_info("%s : LPM Charging Mode! return 0\n", __func__);
		return 0;
	}
#endif

	ret = i2c_add_driver(&sii9234_mhl_tx_i2c_driver);
	if (ret < 0)
		return ret;

	ret = i2c_add_driver(&sii9234_tpi_i2c_driver);
	if (ret < 0)
		goto err_exit1;

	ret = i2c_add_driver(&sii9234_hdmi_rx_i2c_driver);
	if (ret < 0)
		goto err_exit2;

	ret = i2c_add_driver(&sii9234_cbus_i2c_driver);
	if (ret < 0)
		goto err_exit3;

	return 0;

err_exit3:
	i2c_del_driver(&sii9234_hdmi_rx_i2c_driver);
err_exit2:
	i2c_del_driver(&sii9234_tpi_i2c_driver);
err_exit1:
	i2c_del_driver(&sii9234_mhl_tx_i2c_driver);
	pr_err("i2c_add_driver fail\n");
	return ret;
}

static void __exit sii9234_exit(void)
{
	i2c_del_driver(&sii9234_cbus_i2c_driver);
	i2c_del_driver(&sii9234_hdmi_rx_i2c_driver);
	i2c_del_driver(&sii9234_tpi_i2c_driver);
	i2c_del_driver(&sii9234_mhl_tx_i2c_driver);
}

module_init(sii9234_init);
module_exit(sii9234_exit);
