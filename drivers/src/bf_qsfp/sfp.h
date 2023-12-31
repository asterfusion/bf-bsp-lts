/*!
 * @file sfp.h
 * @date 2023/05/04
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

#ifndef __SFP_PRIV_H__
#define __SFP_PRIV_H__

#define XBIT(nr)			((1) << (nr))

/* SFP EEPROM registers */
enum {
	SFP_PHYS_ID			= 0,

	SFP_PHYS_EXT_ID			= 1,
	SFP_PHYS_EXT_ID_SFP		= 0x04,

	SFP_CONNECTOR			= 2,
	SFP_COMPLIANCE			= 3,
	SFP_ENCODING			= 11,
	SFP_BR_NOMINAL			= 12,
	SFP_RATE_ID			= 13,
	SFP_LINK_LEN_SM_KM		= 14,
	SFP_LINK_LEN_SM_100M		= 15,
	SFP_LINK_LEN_50UM_OM2_10M	= 16,
	SFP_LINK_LEN_62_5UM_OM1_10M	= 17,
	SFP_LINK_LEN_COPPER_1M		= 18,
	SFP_LINK_LEN_50UM_OM4_10M	= 18,
	SFP_LINK_LEN_50UM_OM3_10M	= 19,
	SFP_VENDOR_NAME			= 20,
	SFP_VENDOR_OUI			= 37,
	SFP_VENDOR_PN			= 40,
	SFP_VENDOR_REV			= 56,
	SFP_OPTICAL_WAVELENGTH_MSB	= 60,
	SFP_OPTICAL_WAVELENGTH_LSB	= 61,
	SFP_CABLE_SPEC			= 60,
	SFP_CC_BASE			= 63,

	SFP_OPTIONS			= 64,	/* 2 bytes, MSB, LSB */
	SFP_OPTIONS_HIGH_POWER_LEVEL	= XBIT(13),
	SFP_OPTIONS_PAGING_A2		= XBIT(12),
	SFP_OPTIONS_RETIMER		= XBIT(11),
	SFP_OPTIONS_COOLED_XCVR		= XBIT(10),
	SFP_OPTIONS_POWER_DECL		= XBIT(9),
	SFP_OPTIONS_RX_LINEAR_OUT	= XBIT(8),
	SFP_OPTIONS_RX_DECISION_THRESH	= XBIT(7),
	SFP_OPTIONS_TUNABLE_TX		= XBIT(6),
	SFP_OPTIONS_RATE_SELECT		= XBIT(5),
	SFP_OPTIONS_TX_DISABLE		= XBIT(4),
	SFP_OPTIONS_TX_FAULT		= XBIT(3),
	SFP_OPTIONS_LOS_INVERTED	= XBIT(2),
	SFP_OPTIONS_LOS_NORMAL		= XBIT(1),

	SFP_BR_MAX			= 66,
	SFP_BR_MIN			= 67,
	SFP_VENDOR_SN			= 68,
	SFP_DATECODE			= 84,

	SFP_DIAGMON			= 92,
	SFP_DIAGMON_DDM			= XBIT(6),
	SFP_DIAGMON_INT_CAL		= XBIT(5),
	SFP_DIAGMON_EXT_CAL		= XBIT(4),
	SFP_DIAGMON_RXPWR_AVG		= XBIT(3),
	SFP_DIAGMON_ADDRMODE		= XBIT(2),

	SFP_ENHOPTS			= 93,
	SFP_ENHOPTS_ALARMWARN		= XBIT(7),
	SFP_ENHOPTS_SOFT_TX_DISABLE	= XBIT(6),
	SFP_ENHOPTS_SOFT_TX_FAULT	= XBIT(5),
	SFP_ENHOPTS_SOFT_RX_LOS		= XBIT(4),
	SFP_ENHOPTS_SOFT_RATE_SELECT	= XBIT(3),
	SFP_ENHOPTS_APP_SELECT_SFF8079	= XBIT(2),
	SFP_ENHOPTS_SOFT_RATE_SFF8431	= XBIT(1),

	SFP_SFF8472_COMPLIANCE		= 94,
	SFP_SFF8472_COMPLIANCE_NONE	= 0x00,
	SFP_SFF8472_COMPLIANCE_REV9_3	= 0x01,
	SFP_SFF8472_COMPLIANCE_REV9_5	= 0x02,
	SFP_SFF8472_COMPLIANCE_REV10_2	= 0x03,
	SFP_SFF8472_COMPLIANCE_REV10_4	= 0x04,
	SFP_SFF8472_COMPLIANCE_REV11_0	= 0x05,
	SFP_SFF8472_COMPLIANCE_REV11_3	= 0x06,
	SFP_SFF8472_COMPLIANCE_REV11_4	= 0x07,
	SFP_SFF8472_COMPLIANCE_REV12_0	= 0x08,

	SFP_CC_EXT			= 95,
};

/* SFP Diagnostics */
enum {
	/* Alarm and warnings stored MSB at lower address then LSB */
	SFP_TEMP_HIGH_ALARM		= 0,
	SFP_TEMP_LOW_ALARM		= 2,
	SFP_TEMP_HIGH_WARN		= 4,
	SFP_TEMP_LOW_WARN		= 6,
	SFP_VOLT_HIGH_ALARM		= 8,
	SFP_VOLT_LOW_ALARM		= 10,
	SFP_VOLT_HIGH_WARN		= 12,
	SFP_VOLT_LOW_WARN		= 14,
	SFP_BIAS_HIGH_ALARM		= 16,
	SFP_BIAS_LOW_ALARM		= 18,
	SFP_BIAS_HIGH_WARN		= 20,
	SFP_BIAS_LOW_WARN		= 22,
	SFP_TXPWR_HIGH_ALARM		= 24,
	SFP_TXPWR_LOW_ALARM		= 26,
	SFP_TXPWR_HIGH_WARN		= 28,
	SFP_TXPWR_LOW_WARN		= 30,
	SFP_RXPWR_HIGH_ALARM		= 32,
	SFP_RXPWR_LOW_ALARM		= 34,
	SFP_RXPWR_HIGH_WARN		= 36,
	SFP_RXPWR_LOW_WARN		= 38,
	SFP_LASER_TEMP_HIGH_ALARM	= 40,
	SFP_LASER_TEMP_LOW_ALARM	= 42,
	SFP_LASER_TEMP_HIGH_WARN	= 44,
	SFP_LASER_TEMP_LOW_WARN		= 46,
	SFP_TEC_CUR_HIGH_ALARM		= 48,
	SFP_TEC_CUR_LOW_ALARM		= 50,
	SFP_TEC_CUR_HIGH_WARN		= 52,
	SFP_TEC_CUR_LOW_WARN		= 54,
	SFP_CAL_RXPWR4			= 56,
	SFP_CAL_RXPWR3			= 60,
	SFP_CAL_RXPWR2			= 64,
	SFP_CAL_RXPWR1			= 68,
	SFP_CAL_RXPWR0			= 72,
	SFP_CAL_TXI_SLOPE		= 76,
	SFP_CAL_TXI_OFFSET		= 78,
	SFP_CAL_TXPWR_SLOPE		= 80,
	SFP_CAL_TXPWR_OFFSET		= 82,
	SFP_CAL_T_SLOPE			= 84,
	SFP_CAL_T_OFFSET		= 86,
	SFP_CAL_V_SLOPE			= 88,
	SFP_CAL_V_OFFSET		= 90,
	SFP_CHKSUM			= 95,

	SFP_TEMP			= 96,
	SFP_VCC				= 98,
	SFP_TX_BIAS			= 100,
	SFP_TX_POWER			= 102,
	SFP_RX_POWER			= 104,
	SFP_LASER_TEMP			= 106,
	SFP_TEC_CUR			= 108,

	SFP_STATUS			= 110,
	SFP_STATUS_TX_DISABLE		= XBIT(7),
	SFP_STATUS_TX_DISABLE_FORCE	= XBIT(6),
	SFP_STATUS_TX_FAULT		= XBIT(2),
	SFP_STATUS_RX_LOS		= XBIT(1),
	SFP_ALARM0			= 112,
	SFP_ALARM0_TEMP_HIGH		= XBIT(7),
	SFP_ALARM0_TEMP_LOW		= XBIT(6),
	SFP_ALARM0_VCC_HIGH		= XBIT(5),
	SFP_ALARM0_VCC_LOW		= XBIT(4),
	SFP_ALARM0_TX_BIAS_HIGH		= XBIT(3),
	SFP_ALARM0_TX_BIAS_LOW		= XBIT(2),
	SFP_ALARM0_TXPWR_HIGH		= XBIT(1),
	SFP_ALARM0_TXPWR_LOW		= XBIT(0),

	SFP_ALARM1			= 113,
	SFP_ALARM1_RXPWR_HIGH		= XBIT(7),
	SFP_ALARM1_RXPWR_LOW		= XBIT(6),

	SFP_WARN0			= 116,
	SFP_WARN0_TEMP_HIGH		= XBIT(7),
	SFP_WARN0_TEMP_LOW		= XBIT(6),
	SFP_WARN0_VCC_HIGH		= XBIT(5),
	SFP_WARN0_VCC_LOW		= XBIT(4),
	SFP_WARN0_TX_BIAS_HIGH		= XBIT(3),
	SFP_WARN0_TX_BIAS_LOW		= XBIT(2),
	SFP_WARN0_TXPWR_HIGH		= XBIT(1),
	SFP_WARN0_TXPWR_LOW		= XBIT(0),

	SFP_WARN1			= 117,
	SFP_WARN1_RXPWR_HIGH		= XBIT(7),
	SFP_WARN1_RXPWR_LOW		= XBIT(6),

	SFP_EXT_STATUS			= 118,
	SFP_EXT_STATUS_PWRLVL_SELECT	= XBIT(0),

	SFP_VSL				= 120,
	SFP_PAGE			= 127,
};

#endif
