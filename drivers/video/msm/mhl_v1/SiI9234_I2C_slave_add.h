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

typedef enum
{

	 SA_ADAC_ALT									= 0x24 //changed from 0x26 to avoid conflict with Simon
	,SA_ADAC										= 0x26

	,SA__PreAuthenticationPage1						= 0x50
	,SA__PreAuthenticationPage2						= 0x52
	,SA__PreAuthenticationPage3						= 0x54
	
	,SA_RX_Page0_Primary							= 0x60
	,SA_RX_Page0_Secondary							= 0x62
	,SA_PP_RX_TMDS_Ports_0_1						= 0x64
	,SA_PP_RX_TMDS_Ports_2_3						= 0x66
	,SA_RX_Page1_Primary							= 0x68 ,SA_PP_RX_TMDS_Port_4	= SA_RX_Page1_Primary
	,SA_RX_Page1_Secondary							= 0x6A

	,SA_TX_Page0_Primary							= 0x72 
	,SA_RX_HDCP_EDDC								= 0x74
	,SA_TX_Page0_Secondary							= 0x76
	,SA_TX_Page1_Primary							= 0x7A
	,SA_TX_Page1_Secondary							= 0x7E

	,SA_TX_HEAC_Primary								= 0x90 ,SA_PP_Transmitter_TMDS	= SA_TX_HEAC_Primary
	,SA_TX_HDMI_RX_Primary							= 0x92
	,SA_TX_HEAC_Secondary							= 0x94 
	,SA_TX_HDMI_RX_Secondary						= 0x96

	,SA_EDID										= 0xA0

	,SA_PP_Page0_Primary							= 0xB0
	,SA_PP_Page0_Secondary							= 0xB2
	,SA_PP_Page1_Primary							= 0xB8
	,SA_PP_Page1_Secondary							= 0xBA

	,SA_TX_CPI_Primary								= 0xC0
	,SA_RX_CEC										= 0xC2
	,SA_TX_CPI_Secondary							= 0xC4
	,SA_TX_CBUS_Primary								= 0xC8
	,SA_TX_CBUS_Secondary							= 0xCC

	,SA_PP_HEAC										= 0xD0

	,SA_RX_SAMABETH_PAGE_5							= 0xD8

	,SA_PP_EDID_NVRAM								= 0xE0
	,SA_RX_CBUS										= 0xE2
	,SA_RX_CBUS_ALT									= 0xE4 //changed to avoid the conflict with the same slave addresses in Tx
	,SA_PP_CTRL_Ports_0_3							= 0xE6
	,SA_PP_CTRL_Port_4								= 0xE8
}SiI_I2C_Default_SlaveAddresses_e;







