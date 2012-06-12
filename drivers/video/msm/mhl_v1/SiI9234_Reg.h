/*===========================================================================

                      EDIT HISTORY FOR FILE

when              who                         what, where, why
--------        ---                        ----------------------------------------------------------
2010/11/06    Daniel Lee(Philju)      Initial version of file, SIMG Korea 
===========================================================================*/

/*===========================================================================
                     INCLUDE FILES FOR MODULE
===========================================================================*/


/*===========================================================================
                   FUNCTION DEFINITIONS
===========================================================================*/
#include "Common_Def.h"


#define RGND_OPEN 0x00
#define RGND_2K 0x01
#define RGND_1K 0x02
#define RGND_SHOT 0x03



//page: 0 Offset: 0x78 Start
#define PVT_HTBT_FAIL_EN BIT_7
#define RGND_RDY_EN BIT_6
#define VBUS_LOW_EN BIT_5
#define CBUS_LKOUT_EN BIT_4
#define USB_EST_EN BIT_3
#define MHL_EST_EN BIT_2
#define RPWR5V_CHANGE_EN BIT_1
#define SCDT_CHANGE_EN BIT_0
//page: 0 Offset: 0x78 End


// TPI Video Mode Data
//====================

#define TPI_PIX_CLK_LSB						(0x00)
#define TPI_PIX_CLK_MSB						(0x01)

#define TPI_VERT_FREQ_LSB					(0x02)
#define TPI_VERT_FREQ_MSB					(0x03)

#define TPI_TOTAL_PIX_LSB					(0x04)
#define TPI_TOTAL_PIX_MSB					(0x05)

#define TPI_TOTAL_LINES_LSB					(0x06)
#define TPI_TOTAL_LINES_MSB					(0x07)

// Pixel Repetition Data
//======================

#define TPI_PIX_REPETITION					(0x08)

// TPI AVI Input and Output Format Data
//=====================================

/// AVI Input Format Data ================================================= ///

#define TPI_INPUT_FORMAT_REG				(0x09)

//Finish this...

#define INPUT_COLOR_SPACE_MASK				(BIT_1 | BIT_0)
#define INPUT_COLOR_SPACE_RGB				(_ZERO)
#define INPUT_COLOR_SPACE_YCBCR444			(BIT_0)
#define INPUT_COLOR_SPACE_YCBCR422			(BIT_1)
#define INPUT_COLOR_SPACE_BLACK_MODE		(BIT_1 | BIT_0)

/// AVI Output Format Data ================================================ ///

#define TPI_OUTPUT_FORMAT_REG				(0x0A)

#define TPI_YC_Input_Mode					(0x0B)

// TPI AVI InfoFrame Data
//======================= 

#define TPI_AVI_BYTE_0						(0x0C)
#define TPI_AVI_BYTE_1						(0x0D)
#define TPI_AVI_BYTE_2						(0x0E)
#define TPI_AVI_BYTE_3						(0x0F)
#define TPI_AVI_BYTE_4						(0x10)
#define TPI_AVI_BYTE_5						(0x11)

#define TPI_AUDIO_BYTE_0					(0xBF)

#define TPI_END_TOP_BAR_LSB					(0x12)
#define TPI_END_TOP_BAR_MSB					(0x13)

#define TPI_START_BTM_BAR_LSB				(0x14)
#define TPI_START_BTM_BAR_MSB				(0x15)

#define TPI_END_LEFT_BAR_LSB				(0x16)
#define TPI_END_LEFT_BAR_MSB				(0x17)

#define TPI_END_RIGHT_BAR_LSB				(0x18)
#define TPI_END_RIGHT_BAR_MSB				(0x19)

// Colorimetry
//============
#define SET_EX_COLORIMETRY	0x0C	// Set TPI_AVI_BYTE_2 to extended colorimetry and use 
									//TPI_AVI_BYTE_3

// ===================================================== //

#define TPI_SYSTEM_CONTROL_DATA_REG			(0x1A)

#define LINK_INTEGRITY_MODE_MASK			(BIT_6)
#define LINK_INTEGRITY_STATIC				(_ZERO)
#define LINK_INTEGRITY_DYNAMIC				(BIT_6)

#define TMDS_OUTPUT_CONTROL_MASK			(BIT_4)
#define TMDS_OUTPUT_CONTROL_ACTIVE			(_ZERO)
#define TMDS_OUTPUT_CONTROL_POWER_DOWN		(BIT_4)

#define AV_MUTE_MASK						(BIT_3)
#define AV_MUTE_NORMAL						(_ZERO)
#define AV_MUTE_MUTED						(BIT_3)

#define DDC_BUS_REQUEST_MASK				(BIT_2)
#define DDC_BUS_REQUEST_NOT_USING			(_ZERO)
#define DDC_BUS_REQUEST_REQUESTED			(BIT_2)

#define DDC_BUS_GRANT_MASK					(BIT_1)
#define DDC_BUS_GRANT_NOT_AVAILABLE			(_ZERO)
#define DDC_BUS_GRANT_GRANTED				(BIT_1)

#define OUTPUT_MODE_MASK					(BIT_0)
#define OUTPUT_MODE_DVI						(_ZERO)
#define OUTPUT_MODE_HDMI					(BIT_0)


// TPI Identification Registers
//=============================

#define TPI_DEVICE_ID						(0x1B)
#define TPI_DEVICE_REV_ID					(0x1C)

#define TPI_RESERVED2						(0x1D)

// ===================================================== //

#define TPI_DEVICE_POWER_STATE_CTRL_REG		(0x1E)

#define CTRL_PIN_CONTROL_MASK				(BIT_4)
#define CTRL_PIN_TRISTATE					(_ZERO)
#define CTRL_PIN_DRIVEN_TX_BRIDGE			(0x10)

#define TX_POWER_STATE_MASK					(BIT_1 | BIT_0)
#define TX_POWER_STATE_D0					(_ZERO)
#define TX_POWER_STATE_D1					(BIT_0)
#define TX_POWER_STATE_D2					(BIT_1)
#define TX_POWER_STATE_D3					(BIT_1 | BIT_0)

// Configuration of I2S Interface
//===============================

#define TPI_I2S_EN							(0x1F)
#define TPI_I2S_IN_CFG						(0x20)

// Available only when TPI 0x26[7:6]=10 to select I2S input
//=========================================================
#define TPI_I2S_CHST_0						(0x21)
#define TPI_I2S_CHST_1						(0x22)
#define TPI_I2S_CHST_2						(0x23)
#define TPI_I2S_CHST_3						(0x24)
#define TPI_I2S_CHST_4						(0x25)


// Available only when 0x26[7:6]=01
//=================================
#define TPI_SPDIF_HEADER					(0x24)
#define TPI_AUDIO_HANDLING					(0x25)


// Audio Configuration Regiaters
//==============================
#define TPI_AUDIO_INTERFACE_REG				(0x26)

// Finish this...

#define AUDIO_MUTE_MASK						(BIT_4)
#define AUDIO_MUTE_NORMAL					(_ZERO)
#define AUDIO_MUTE_MUTED					(BIT_4)






#define TPI_AUDIO_SAMPLE_CTRL				(0x27)

#define TPI_SPEAKER_CFG						(0xC7)
#define TPI_CHANNEL_COUNT					(0xC4)

/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ///

/*\
| | HDCP Implementation
| |
| | HDCP link security logic is implemented in certain transmitters; unique
| |   keys are embedded in each chip as part of the solution. The security 
| |   scheme is fully automatic and handled completely by the hardware.
\*/

/// HDCP Query Data Register ============================================== ///

#define TPI_HDCP_QUERY_DATA_REG				(0x29)

#define EXTENDED_LINK_PROTECTION_MASK		(BIT_7)
#define EXTENDED_LINK_PROTECTION_NONE		(_ZERO)
#define EXTENDED_LINK_PROTECTION_SECURE		(BIT_7)

#define LOCAL_LINK_PROTECTION_MASK			(BIT_6)
#define LOCAL_LINK_PROTECTION_NONE			(_ZERO)
#define LOCAL_LINK_PROTECTION_SECURE		(BIT_6)

#define LINK_STATUS_MASK					(BIT_5 | BIT_4)
#define LINK_STATUS_NORMAL					(_ZERO)
#define LINK_STATUS_LINK_LOST				(BIT_4)
#define LINK_STATUS_RENEGOTIATION_REQ		(BIT_5)
#define LINK_STATUS_LINK_SUSPENDED			(BIT_5 | BIT_4)

#define HDCP_REPEATER_MASK					(BIT_3)
#define HDCP_REPEATER_NO					(_ZERO)
#define HDCP_REPEATER_YES					(BIT_3)

#define CONNECTOR_TYPE_MASK					(BIT_2 | BIT_0)
#define CONNECTOR_TYPE_DVI					(_ZERO)
#define CONNECTOR_TYPE_RSVD					(BIT_0)
#define CONNECTOR_TYPE_HDMI					(BIT_2)
#define CONNECTOR_TYPE_FUTURE				(BIT_2 | BIT_0)

#define PROTECTION_TYPE_MASK				(BIT_1)
#define PROTECTION_TYPE_NONE				(_ZERO)
#define PROTECTION_TYPE_HDCP				(BIT_1)

/// HDCP Control Data Register ============================================ ///

#define TPI_HDCP_CONTROL_DATA_REG			(0x2A)

#define PROTECTION_LEVEL_MASK				(BIT_0)
#define PROTECTION_LEVEL_MIN				(0x00)
#define PROTECTION_LEVEL_MAX				(0x01)

/// HDCP BKSV Registers =================================================== ///

#define TPI_BKSV_1_REG						(0x2B)
#define TPI_BKSV_2_REG						(0x2C)
#define TPI_BKSV_3_REG						(0x2D)
#define TPI_BKSV_4_REG						(0x2E)
#define TPI_BKSV_5_REG						(0x2F)

/// HDCP Revision Data Register =========================================== ///

#define TPI_HDCP_REVISION_DATA_REG			(0x30)

#define HDCP_MAJOR_REVISION_MASK			(BIT_7 | BIT_6 | BIT_5 | BIT_4)
#define HDCP_MAJOR_REVISION_VALUE			(0x10)

#define HDCP_MINOR_REVISION_MASK			(BIT_3 | BIT_2 | BIT_1 | BIT_0)
#define HDCP_MINOR_REVISION_VALUE			(0x02)

/// HDCP KSV and V' Value Data Register =================================== ///

#define TPI_V_PRIME_SELECTOR_REG			(0x31)

/// V' Value Readback Registers =========================================== ///

#define TPI_V_PRIME_7_0_REG					(0x32)
#define TPI_V_PRIME_15_9_REG				(0x33)
#define TPI_V_PRIME_23_16_REG				(0x34)
#define TPI_V_PRIME_31_24_REG				(0x35)

/// HDCP AKSV Registers =================================================== ///

#define TPI_AKSV_1_REG						(0x36)
#define TPI_AKSV_2_REG						(0x37)
#define TPI_AKSV_3_REG						(0x38)
#define TPI_AKSV_4_REG						(0x39)
#define TPI_AKSV_5_REG						(0x3A)

/*\
| | Interrupt Service
| |
| | TPI can be configured to generate an interrupt to the host to notify it of
| |   various events. The host can either poll for activity or use an interrupt
| |   handler routine. TPI generates on a single interrupt (INT) to the host.
\*/

/// Interrupt Enable Register ============================================= ///

#define TPI_INTERRUPT_ENABLE_REG			(0x3C)

#define HDCP_AUTH_STATUS_CHANGE_EN_MASK		(BIT_7)
#define HDCP_AUTH_STATUS_CHANGE_DISABLE		(0x00)
#define HDCP_AUTH_STATUS_CHANGE_ENABLE		(0x80)

#define HDCP_VPRIME_VALUE_READY_EN_MASK		(BIT_6)
#define HDCP_VPRIME_VALUE_READY_DISABLE		(0x00)
#define HDCP_VPRIME_VALUE_READY_ENABLE		(0x40)

#define HDCP_SECURITY_CHANGE_EN_MASK		(BIT_5)
#define HDCP_SECURITY_CHANGE_DISABLE		(0x00)
#define HDCP_SECURITY_CHANGE_ENABLE			(0x20)

#define AUDIO_ERROR_EVENT_EN_MASK			(BIT_4)
#define AUDIO_ERROR_EVENT_DISABLE			(0x00)
#define AUDIO_ERROR_EVENT_ENABLE			(0x10)

#define CPI_EVENT_NO_RX_SENSE_MASK			(BIT_3)
#define CPI_EVENT_NO_RX_SENSE_DISABLE		(0x00)
#define CPI_EVENT_NO_RX_SENSE_ENABLE		(0x08)

#define RECEIVER_SENSE_EVENT_EN_MASK		(BIT_1)
#define RECEIVER_SENSE_EVENT_DISABLE		(0x00)
#define RECEIVER_SENSE_EVENT_ENABLE			(0x02)

#define HOT_PLUG_EVENT_EN_MASK				(BIT_0)
#define HOT_PLUG_EVENT_DISABLE				(0x00)
#define HOT_PLUG_EVENT_ENABLE				(0x01)

/// Interrupt Status Register ============================================= ///

#define TPI_INTERRUPT_STATUS_REG			(0x3D)

#define HDCP_AUTH_STATUS_CHANGE_EVENT_MASK	(BIT_7)
#define HDCP_AUTH_STATUS_CHANGE_EVENT_NO	(0x00)
#define HDCP_AUTH_STATUS_CHANGE_EVENT_YES	(0x80)

#define HDCP_VPRIME_VALUE_READY_EVENT_MASK	(BIT_6)
#define HDCP_VPRIME_VALUE_READY_EVENT_NO	(0x00)
#define HDCP_VPRIME_VALUE_READY_EVENT_YES	(0x40)

#define HDCP_SECURITY_CHANGE_EVENT_MASK		(BIT_5)
#define HDCP_SECURITY_CHANGE_EVENT_NO		(0x00)
#define HDCP_SECURITY_CHANGE_EVENT_YES		(0x20)

#define AUDIO_ERROR_EVENT_MASK				(BIT_4)
#define AUDIO_ERROR_EVENT_NO				(0x00)
#define AUDIO_ERROR_EVENT_YES				(0x10)

#define CPI_EVENT_MASK						(BIT_3)
#define CPI_EVENT_NO						(0x00)
#define CPI_EVENT_YES						(0x08)
#define RX_SENSE_MASK						(BIT_3)		// This bit is dual purpose depending on the value of 0x3C[3]
#define RX_SENSE_NOT_ATTACHED				(0x00)
#define RX_SENSE_ATTACHED					(0x08)

#define HOT_PLUG_PIN_STATE_MASK				(BIT_2)
#define HOT_PLUG_PIN_STATE_LOW				(0x00)
#define HOT_PLUG_PIN_STATE_HIGH				(0x04)

#define RECEIVER_SENSE_EVENT_MASK			(BIT_1)
#define RECEIVER_SENSE_EVENT_NO				(0x00)
#define RECEIVER_SENSE_EVENT_YES			(0x02)

#define HOT_PLUG_EVENT_MASK					(BIT_0)
#define HOT_PLUG_EVENT_NO					(0x00)
#define HOT_PLUG_EVENT_YES					(0x01)

/// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ ///

// Sync Register Configuration and Sync Monitoring Registers
//==========================================================

#define TPI_SYNC_GEN_CTRL					(0x60)
#define TPI_SYNC_POLAR_DETECT				(0x61)

// Explicit Sync DE Generator Registers (TPI 0x60[7]=0)
//=====================================================

#define TPI_DE_DLY							(0x62)
#define TPI_DE_CTRL							(0x63)
#define TPI_DE_TOP							(0x64)

#define TPI_RESERVED4						(0x65)

#define TPI_DE_CNT_7_0						(0x66)
#define TPI_DE_CNT_11_8						(0x67)

#define TPI_DE_LIN_7_0						(0x68)
#define TPI_DE_LIN_10_8						(0x69)

#define TPI_DE_H_RES_7_0					(0x6A)
#define TPI_DE_H_RES_10_8					(0x6B)

#define TPI_DE_V_RES_7_0					(0x6C)
#define TPI_DE_V_RES_10_8					(0x6D)

// Embedded Sync Register Set (TPI 0x60[7]=1)
//===========================================

#define TPI_HBIT_TO_HSYNC_7_0				(0x62)
#define TPI_HBIT_TO_HSYNC_9_8				(0x63)
#define TPI_FIELD_2_OFFSET_7_0				(0x64)
#define TPI_FIELD_2_OFFSET_11_8				(0x65)
#define TPI_HWIDTH_7_0						(0x66)
#define TPI_HWIDTH_8_9						(0x67)
#define TPI_VBIT_TO_VSYNC					(0x68)
#define TPI_VWIDTH							(0x69)

// TPI Enable Register
//====================

#define TPI_ENABLE							(0xC7)

// Misc InfoFrames
//================
#define MISC_INFO_FRAMES_CTRL				(0xBF)
#define MISC_INFO_FRAMES_TYPE				(0xC0)
#define MISC_INFO_FRAMES_VER				(0xC1)
#define MISC_INFO_FRAMES_LEN				(0xC2)
#define MISC_INFO_FRAMES_CHKSUM				(0xC3)

//NAGSM_Android_SEL_Kernel_Aakash_20101206

typedef struct 
{
    //bool_t		interruptDriven;	// Remember what app told us about interrupt availability.
    //uint8_t		pollIntervalMs;		// Remember what app set the polling frequency as.

	byte		mscState;			// Internal MSC engine's states.

	byte		status_0;			// Received status from peer is stored here
	byte		status_1;			// Received status from peer is stored here

	bool		mhlConnectionEvent;
	byte		mhlConnected;

	// mscMsgArrived == true when a MSC MSG arrives, false when it has been picked up
	bool		mscMsgArrived;
	byte		mscMsgSubCommand;
	byte		mscMsgData;

	// Remember FEATURE FLAG of the peer to reject app commands if unsupported
	byte		mscFeatureFlag;

	// Remember last command, offset that was sent. Mostly for READ_DEVCAP command
	byte		mscLastCommand;
	byte		mscLastOffset;

	// Remember last MSC_MSG command (RCPE particularly)
	byte		mscMsgLastCommand;
	byte		mscMsgLastData;
	byte		mscSaveRcpKeyCode;
  

//  uint8_t 	mscData[ 16 ]; 		// What we got back as message data

} mhlTx_config_t;

enum
{
	MSC_STATE_IDLE		= 0x00,		// Until MHL is connected, do nothing
	MSC_STATE_BEGIN		= 0x01,		// Start of MSC engine to read devcap of the peer
	MSC_STATE_POW_DONE	= 0x02,		// Just read devcap[2]. Next do the feature flags
	MSC_STATE_RCP_READY = 0x03		// Feature flags done. Ready to take in RCP.
};
//NAGSM_Android_SEL_Kernel_Aakash_20101206
