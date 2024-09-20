/*
 * MFC_charger.h
 * Samsung MFC Charger Header
 *
 * Copyright (C) 2015 Samsung Electronics, Inc.
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

#ifndef __MFC_CHARGER_H
#define __MFC_CHARGER_H __FILE__

#include <linux/mfd/core.h>
#include <linux/regulator/machine.h>
#include <linux/i2c.h>
#include "../sec_charging_common.h"

#define MFC_FW_BIN_VERSION			0x58
#define MFC_FW_BIN_FULL_VERSION		0x00580001
#define MFC_FW_BIN_VERSION_ADDR		0x14a8 //fw rev58 address
#define FW_ADDRES_MAX 7

/* REGISTER MAPS */
#define MFC_CHIP_ID_L_REG					0x00
#define MFC_CHIP_ID_H_REG					0x01
#define MFC_CHIP_REVISION_REG				0x02
#define MFC_CUSTOMER_ID_REG					0x03
#define MFC_FW_MAJOR_REV_L_REG				0x04
#define MFC_FW_MAJOR_REV_H_REG				0x05
#define MFC_FW_MINOR_REV_L_REG				0x06
#define MFC_FW_MINOR_REV_H_REG				0x07
#define MFC_PRMC_ID_L_REG					0x0A
#define MFC_PRMC_ID_H_REG					0x0B
/* RXID BIT[0:47] */
#define MFC_WPC_RXID_0_REG					0x10
#define MFC_WPC_RXID_1_REG					0x11
#define MFC_WPC_RXID_2_REG					0x12
#define MFC_WPC_RXID_3_REGs					0x13
#define MFC_WPC_RXID_4_REG					0x14
#define MFC_WPC_RXID_5_REG					0x15

#define MFC_STATUS_L_REG					0x20
#define MFC_STATUS_H_REG					0x21
#define MFC_INT_A_L_REG						0x22
#define MFC_INT_A_H_REG						0x23
#define MFC_INT_A_ENABLE_L_REG				0x24
#define MFC_INT_A_ENABLE_H_REG				0x25
#define MFC_INT_A_CLEAR_L_REG				0x26
#define MFC_INT_A_CLEAR_H_REG				0x27
#define MFC_INT_B_REG						0x28
#define MFC_INT_B_ENABLE_REG				0x29
#define MFC_INT_B_CLEAR_REG					0x2A

#define MFC_SYS_OP_MODE_REG					0x2B
#define MFC_BATTERY_CHG_STATUS_REG			0x3A
/* EPT(End of Power Transfer) cases. PMA has only EOC case */
#define MFC_EPT_REG							0x3B
#define MFC_ADC_VOUT_L_REG					0x3C
#define MFC_ADC_VOUT_H_REG					0x3D
#define MFC_VOUT_SET_REG					0x3E
#define MFC_VRECT_ADJ_REG					0x3F
#define MFC_ADC_VRECT_L_REG					0x40
#define MFC_ADC_VRECT_H_REG					0x41
#define MFC_ADC_TX_ISENSE_L_REG				0x42
#define MFC_ADC_TX_ISENSE_H_REG				0x43
#define MFC_ADC_RX_IOUT_L_REG				0x44
#define MFC_ADC_RX_IOUT_H_REG				0x45
#define MFC_ADC_DIE_TEMP_L_REG				0x46 /* 8 LSB field is used, Celsius */
#define MFC_ADC_DIE_TEMP_H_REG				0x47 /* only 4 MSB[3:0] field is used, Celsius */
#define MFC_RX_OP_FREQ_L_REG				0x48 /* kHZ */
#define MFC_RX_OP_FREQ_H_REG				0x49 /* kHZ */
#define MFC_RX_PING_FREQ_L_REG				0x4A /* kHZ */
#define MFC_RX_PING_FREQ_H_REG				0x4B /* kHZ */
/* ILim =  value * 0.1(A) + 0.1(A) */
#define MFC_ILIM_SET_REG					0x4C
/* Target Vrect is ReadOnly register, and updated by every 10ms
	Its default value is 0x1A90(6800mV).
	Target_Vrect (Iout,Vout) = {Vout + 0.05} + { Vrect(Iout,5V)-Vrect(1A,5V) } * 5/9
	*/
#define MFC_TARGET_VRECT_L_REG				0x015B /* default 0x90 */
#define MFC_TARGET_VRECT_H_REG				0x015C /* default 0x1A */

#define MFC_AP2MFC_CMD_L_REG				0x4E
#define MFC_AP2MFC_CMD_H_REG				0x4F
#define MFC_BT2MFC_CMD_REG					0xA0

#define MFC_WPC_PCKT_HEADER_REG				0x50
#define MFC_WPC_RX_DATA_COM_REG				0x51 /* WPC Rx to Tx COMMAND */
#define MFC_WPC_RX_DATA_VALUE0_REG			0x52
#define MFC_WPC_RX_DATA_VALUE1_REG			0x53
#define MFC_WPC_RX_DATA_VALUE2_REG			0x54
#define MFC_WPC_RX_DATA_VALUE3_REG			0x55
#define MFC_PMA_RX_ADVT_CS_REG				0x56 /* PMA Advertisement Reg, 4MSB[3:0] Checksum */
#define MFC_PMA_RX_ADVT_MSG_REG				0x57 /* PMA Advertisement Reg, Message */

#define MFC_WPC_TX_DATA_COM_REG				0x58
#define MFC_WPC_TX_DATA_VALUE0_REG			0x59
#define MFC_WPC_TX_DATA_VALUE1_REG			0x5A

/* AP2BT DATA VALUE[24:0] */
#define MFC_AP2BT_DATA_COM_REG				0xA1
#define MFC_AP2BT_DATA_VALUE0_REG			0xA2
#define MFC_AP2BT_DATA_VALUE1_REG			0xA3
#define MFC_AP2BT_DATA_VALUE2_REG			0xA4
#define MFC_AP2BT_DATA_VALUE3_REG			0xA5
/* BT2AP DATA VALUE[24:0] */
#define MFC_BT2AP_DATA_COM_REG				0xA6
#define MFC_BT2AP_DATA_VALUE0_REG			0xA7
#define MFC_BT2AP_DATA_VALUE1_REG			0xA8
#define MFC_BT2AP_DATA_VALUE2_REG			0xA9
#define MFC_BT2AP_DATA_VALUE3_REG			0xAA
/* TX Max Operating Frequency = 60 MHz/value, default is 148kHz (60MHz/0x195=148) */
#define MFC_TX_MAX_OP_FREQ_L_REG			0x60 /* default 0x95 */
#define MFC_TX_MAX_OP_FREQ_H_REG			0x61 /* default 0x01 */
/* TX Max Operating Frequency = 60 MHz/value, default is 80kHz (60MHz/0x2EE=80) */
#define MFC_TX_MIN_OP_FREQ_L_REG			0x62 /* default 0xEE */
#define MFC_TX_MIN_OP_FREQ_H_REG			0x63 /* default 0x02 */
/* TX Digital Ping Frequency = 60 MHz/value, default is 90kHz (60MHz/0x29B=90) */
#define MFC_TX_PING_FREQ_L_REG				0x64 /* default 0x9B */
#define MFC_TX_PING_FREQ_H_REG				0x65 /* default 0x02 */
/* TX Digital Ping Duty-Cycle, 5%(0x05) ~ 50%(0x32) */
#define MFC_TX_PING_DUTY_CYCLE_REG			0x66 /* default 0x32 */
#define MFC_TX_INVERTER_CTRL_REG			0x67
/* RX Mode Communication Modulation FET Ctrl */
#define MFC_RX_CMFET_CTRL_REG				0x68
#define MFC_MST_MODE_SEL_REG				0x69
#define MFC_RX_OV_CLAMP_REG					0x6A
#define MFC_RX_COMM_MOD_FET_REG				0x6B

#define MFC_WPC_FOD_0A_REG					0x70
#define MFC_WPC_FOD_0B_REG					0x71
#define MFC_WPC_FOD_1A_REG					0x72
#define MFC_WPC_FOD_1B_REG					0x73
#define MFC_WPC_FOD_2A_REG					0x74
#define MFC_WPC_FOD_2B_REG					0x75
#define MFC_WPC_FOD_3A_REG					0x76
#define MFC_WPC_FOD_3B_REG					0x77
#define MFC_WPC_FOD_4A_REG					0x78
#define MFC_WPC_FOD_4B_REG					0x79
#define MFC_WPC_FOD_5A_REG					0x7A
#define MFC_WPC_FOD_5B_REG					0x7B

#define MFC_PMA_FOD_0A_REG					0x80
#define MFC_PMA_FOD_0B_REG					0x81
#define MFC_PMA_FOD_1A_REG					0x82
#define MFC_PMA_FOD_1B_REG					0x83
#define MFC_PMA_FOD_2A_REG					0x84
#define MFC_PMA_FOD_2B_REG					0x85
#define MFC_PMA_FOD_3A_REG					0x86
#define MFC_PMA_FOD_3B_REG					0x87
#define MFC_PMA_FOD_4A_REG					0x88
#define MFC_PMA_FOD_4B_REG					0x89
#define MFC_PMA_FOD_5A_REG					0x8A
#define MFC_PMA_FOD_5B_REG					0x8B

#define MFC_A4WP_FOD_0A_REG					0x0
#define MFC_A4WP_FOD_0B_REG					0x91
#define MFC_A4WP_FOD_1A_REG					0x92
#define MFC_A4WP_FOD_1B_REG					0x93
#define MFC_A4WP_FOD_2A_REG					0x94
#define MFC_A4WP_FOD_2B_REG					0x95
#define MFC_A4WP_FOD_3A_REG					0x96
#define MFC_A4WP_FOD_3B_REG					0x97
#define MFC_A4WP_FOD_4A_REG					0x98
#define MFC_A4WP_FOD_4B_REG					0x99
#define MFC_A4WP_FOD_5A_REG					0x9A
#define MFC_A4WP_FOD_5B_REG					0x9B

#define MFC_FW_DATA_CODE_0					0xB0
#define MFC_FW_DATA_CODE_1					0xB1
#define MFC_FW_DATA_CODE_2					0xB2
#define MFC_FW_DATA_CODE_3					0xB3
#define MFC_FW_DATA_CODE_4					0xB4
#define MFC_FW_DATA_CODE_5					0xB5
#define MFC_FW_DATA_CODE_6					0xB6
#define MFC_FW_DATA_CODE_7					0xB7
#define MFC_FW_DATA_CODE_8					0xB8
#define MFC_FW_DATA_CODE_9					0xB9
#define MFC_FW_DATA_CODE_A					0xBA
/* Timer code contains ASCII value. (ex. 31 means '1', 3A means ':') */
#define MFC_FW_TIMER_CODE_0					0xC0
#define MFC_FW_TIMER_CODE_1					0xC1
#define MFC_FW_TIMER_CODE_2					0xC2
#define MFC_FW_TIMER_CODE_3					0xC3
#define MFC_FW_TIMER_CODE_4					0xC4
#define MFC_FW_TIMER_CODE_5					0xC5
#define MFC_FW_TIMER_CODE_6					0xC6
#define MFC_FW_TIMER_CODE_7					0xC7
#define MFC_RPP_SCALE_COEF_REG					0xF0
enum {
	MFC_PAD_NONE = 0,
	MFC_PAD_WPC,				/* 1 */
	MFC_PAD_WPC_AFC,			/* 2 */
	MFC_PAD_WPC_PACK,			/* 3 */
	MFC_PAD_WPC_PACK_TA,		/* 4 */
	MFC_PAD_WPC_STAND,			/* 5 */
	MFC_PAD_WPC_STAND_HV,		/* 6 */
	MFC_PAD_PMA,				/* 7 */
	MFC_PAD_WPC_VEHICLE,		/* 8 */
	MFC_PAD_WPC_VEHICLE_HV,		/* 9 */
	MFC_PAD_PREPARE_HV,			/* 10 */
	MFC_PAD_A4WP,				/* 11 */
	MFC_PAD_TX,					/* 12 */
};

#define MFC_VOUT_CFG_STEP			7
static const u8 mfc_idt_vout_val[] = {
 0x0F, 0x19, 0x23, 0x2D, 0x37, 0x41, 0x14,
};
static const u8 mfc_lsi_vout_val[] = {
 0x72, 0x78, 0x8C, 0xA0, 0xC8, 0xC8, 0x6E,
};

enum {
	MFC_VOUT_5V = 0,	/* CC CALL, CV CALL */
	MFC_VOUT_6V, // 1
	MFC_VOUT_7V, // 2
	MFC_VOUT_8V, // 3
	MFC_VOUT_9V, // 4
	MFC_VOUT_10V, // 5
	MFC_VOUT_5_5V,		/* CC-CV */
};

/* System Operating Mode Register, Sys_Op_Mode (0x2B) */
#define PAD_MODE_MISSING			0
#define PAD_MODE_WPC_BASIC			1
#define PAD_MODE_WPC_ADV			2
#define PAD_MODE_PMA_SR1			3
#define PAD_MODE_PMA_SR1E			4
#define PAD_MODE_A4WP				5
#define PAD_MODE_A4WP_LPM			6
#define PAD_MODE_UNKNOWN			7

/* MFC_WPC_DATA_COM_REG (0x51) : RX Command */
#define	WPC_COM_UNKNOWN					0x00
#define	WPC_COM_TX_ID					0x01
#define	WPC_COM_CHG_STATUS				0x05
#define	WPC_COM_AFC_SET					0x06
#define	WPC_COM_AFC_DEBOUNCE			0x07 /* Data Values [ 0~1000mV : 0x0000~0x03E8 ], 2 bytes*/
#define	WPC_COM_SID_TAG					0x08
#define	WPC_COM_SID_TOKEN				0x09
#define	WPC_COM_TX_STANDBY				0x0A
#define	WPC_COM_LED_CONTROL				0x0B /* Data Value LED Enable(0x00), LED Disable(0xFF) */
#define	WPC_COM_REQ_AFC_TX				0x0C /* Data Value (0x00) */
#define	WPC_COM_COOLING_CTRL			0x0D /* Data Value ON(0x00), OFF(0xFF) */
#define	WPC_COM_CHG_LEVEL				0x0F /* Battery level */

/* MFC_TX_DATA_COM_REG (0x58) : TX Command */
#define	WPC_TX_COM_UNKNOWN					0x00
#define	WPC_TX_COM_TX_ID					0x01
#define	WPC_TX_COM_AFC_SET					0x02
#define	WPC_TX_COM_ACK						0x03
#define	WPC_TX_COM_NAK						0x04

#define TX_AFC_SET_5V			0x00
#define TX_AFC_SET_10V			0x01
#define TX_AFC_SET_12V			0x02
#define TX_AFC_SET_18V			0x03
#define TX_AFC_SET_19V			0x04
#define TX_AFC_SET_20V			0x05
#define TX_AFC_SET_24V			0x06

#define TX_ID_UNKNOWN				0x00
#define TX_ID_SNGL_PORT_START		0x01
#define TX_ID_VEHICLE_PAD			0x11
#define TX_ID_SNGL_PORT_END			0x1F
#define TX_ID_MULTI_PORT_START		0x20
#define TX_ID_MULTI_PORT_END		0x2F
#define TX_ID_STAND_TYPE_START		0x30
#define TX_ID_STAND_TYPE_END		0x3F
#define TX_ID_BATT_PACK				0x40
#define TX_ID_BATT_PACK_TA			0x41
#define TX_ID_DREAM_STAND			0x31
#define TX_ID_DREAM_DOWN			0x14

#define	WPC_COM_AFC_DEBOUNCE			0x07 /* Data Values [ 0~1000mV : 0x0000~0x03E8 ], 2 bytes*/

/* MFC_AP2BT_DATA_COM_REG (0xA1) : Command */
#define	AP2BT_COM_UNKNOWN				0x00
#define	AP2BT_COM_TX_ID					0x01
#define	AP2BT_COM_CHG_STATUS			0x05
#define	AP2BT_COM_AFC_MODE				0x06
#define	AP2BT_COM_VRECT_OFFSET			0x07 /* Data Values [ 0~1000mV : 0x0000~0x03E8 ], 2 bytes*/
#define	AP2BT_COM_SID_TAG				0x08
#define	AP2BT_COM_SID_TOKEN				0x09
#define	AP2BT_COM_TX_STANDBY			0x0A
#define	AP2BT_COM_LED_CONTROL			0x0B /* Data Value LED Enable(0x00), LED Disable(0xFF) */
#define	AP2BT_COM_REQ_AFC_TX			0x0C /* Data Value (0x00) */
#define	AP2BT_COM_COOLING_CTRL			0x0D /* Data Value ON(0x00), OFF(0xFF) */

/* MFC_BT2AP_DATA_COM_REG (0xA6) : Command */
#define	BT2AP_COM_UNKNOWN				0x00
#define	BT2AP_COM_TX_ID					0x01
#define	BT2AP_COM_CHG_STATUS			0x05
#define	BT2AP_COM_AFC_MODE				0x06
#define	BT2AP_COM_PWR_STATUS			0x07
#define	BT2AP_COM_SID_TAG				0x08
#define	BT2AP_COM_SID_TOKEN				0x09
#define	BT2AP_COM_TX_STANDBY			0x0A
#define	BT2AP_COM_LED_CONTROL			0x0B /* Data Value LED Enable(0x00), LED Disable(0xFF) */
#define	BT2AP_COM_REQ_AFC_TX			0x0C /* Data Value (0x00) */
#define	BT2AP_COM_COOLING_CTRL			0x0D /* Data Value ON(0x00), OFF(0xFF) */

#define MFC_NUM_FOD_REG					12

/* Command Register, COM_L(0x4E) */
#define MFC_CMD_AP2BT_DATA_SHIFT			7
#define MFC_CMD_INT_LPM_SHIFT				6 /* set this bit to 0, then INT_LPM LOW, set this bit to 1, then INT_LPM HIGH */
#define MFC_CMD_CLEAR_INT_SHIFT				5
#define MFC_CMD_SEND_CHG_STS_SHIFT			4
#define MFC_CMD_SEND_EOP_SHIFT				3
#define MFC_CMD_MCU_RESET_SHIFT				2
#define MFC_CMD_TOGGLE_LDO_SHIFT			1
#define MFC_CMD_SEND_RX_DATA_SHIFT			0
#define MFC_CMD_AP2BT_DATA_MASK				(1 << MFC_CMD_AP2BT_DATA_SHIFT)
#define MFC_CMD_INT_LPM_MASK				(1 << MFC_CMD_INT_LPM_SHIFT)
#define MFC_CMD_CLEAR_INT_MASK				(1 << MFC_CMD_CLEAR_INT_SHIFT)
#define MFC_CMD_SEND_CHG_STS_MASK			(1 << MFC_CMD_SEND_CHG_STS_SHIFT) /* MFC MCU sends ChgStatus packet to TX */
#define MFC_CMD_SEND_EOP_MASK				(1 << MFC_CMD_SEND_EOP_SHIFT)
#define MFC_CMD_MCU_RESET_MASK				(1 << MFC_CMD_MCU_RESET_SHIFT)
#define MFC_CMD_TOGGLE_LDO_MASK				(1 << MFC_CMD_TOGGLE_LDO_SHIFT)
#define MFC_CMD_SEND_RX_DATA_MASK			(1 << MFC_CMD_SEND_RX_DATA_SHIFT)

/* Command Register, COM_H(0x4F) */
#define MFC_CMD2_WP_ON_SHIFT				0
#define MFC_CMD2_WP_ON_MASK					(1 << MFC_CMD2_WP_ON_SHIFT)

/* Chip Revision and Font Register, Chip_Rev (0x02) */
#define MFC_CHIP_REVISION_MASK				0xf0
#define MFC_CHIP_FONT_MASK					0x0f

/* Status Registers, Status_L (0x20), Status_H (0x21) */
#define MFC_STAT_L_STAT_VOUT_SHIFT				7
#define MFC_STAT_L_STAT_VRECT_SHIFT				6
#define MFC_STAT_L_OP_MODE_SHIFT				5
#define MFC_STAT_L_OVER_VOL_SHIFT				4
#define MFC_STAT_L_OVER_CURR_SHIFT				3
#define MFC_STAT_L_OVER_TEMP_SHIFT				2
#define MFC_STAT_L_INT_LPM_SHIFT				1
#define MFC_STAT_L_BT2AP_DATA_SHIFT				0

#define MFC_STAT_L_STAT_VOUT_MASK				(1 << MFC_STAT_L_STAT_VOUT_SHIFT)
#define MFC_STAT_L_STAT_VRECT_MASK				(1 << MFC_STAT_L_STAT_VRECT_SHIFT)
#define MFC_STAT_L_OP_MODE_MASK					(1 << MFC_STAT_L_OP_MODE_SHIFT)
#define MFC_STAT_L_OVER_VOL_MASK				(1 << MFC_STAT_L_OVER_VOL_SHIFT)
#define MFC_STAT_L_OVER_CURR_MASK				(1 << MFC_STAT_L_OVER_CURR_SHIFT)
#define MFC_STAT_L_OVER_TEMP_MASK				(1 << MFC_STAT_L_OVER_TEMP_SHIFT)
#define MFC_STAT_L_INT_LPM_MASK					(1 << MFC_STAT_L_INT_LPM_SHIFT)
#define MFC_STAT_L_BT2AP_DATA_MASK				(1 << MFC_STAT_L_BT2AP_DATA_SHIFT)

#define MFC_STAT_H_TX_DATA_RECEIVED_SHFIT		7
#define MFC_STAT_H_TX_OVER_CURR_SHIFT			6
#define MFC_STAT_H_TX_OVER_TEMP_SHIFT			5
#define MFC_STAT_H_TX_FOD_SHIFT					4
#define MFC_STAT_H_TX_CON_DISCON_SHIFT			3
#define MFC_STAT_H_AC_MISSING_DET_SHIFT			2
#define MFC_STAT_H_RESERVED1_SHIFT				1
#define MFC_STAT_H_RESERVED0_SHIFT				0
#define MFC_STAT_H_TX_DATA_RECEIVED_MASK		(1 << MFC_STAT_H_TX_DATA_RECEIVED_SHFIT)
#define MFC_STAT_H_TX_OVER_CURR_MASK			(1 << MFC_STAT_H_TX_OVER_CURR_SHIFT)
#define MFC_STAT_H_TX_OVER_TEMP_MASK			(1 << MFC_STAT_H_TX_OVER_TEMP_SHIFT)
#define MFC_STAT_H_TX_FOD_MASK					(1 << MFC_STAT_H_TX_FOD_SHIFT)
#define MFC_STAT_H_TX_CON_DISCON_MASK			(1 << MFC_STAT_H_TX_CON_DISCON_SHIFT)
#define MFC_STAT_H_AC_MISSING_DET_MASK			(1 << MFC_STAT_H_AC_MISSING_DET_SHIFT)
#define MFC_STAT_H_RESERVED1_MASK				(1 << MFC_STAT_H_RESERVED1_SHIFT)
#define MFC_STAT_H_RESERVED0_MASK				(1 << MFC_STAT_H_RESERVED0_SHIFT)

#define MFC_STAT_OVER_TEMP_SHIFT			7
#define MFC_STAT_TX_OVER_CURR_SHIFT		6
#define MFC_STAT_TX_OVER_TEMP_SHIFT		5
#define MFC_STAT_TX_FOD_SHIFT				4
#define MFC_STAT_TX_CONNECT_SHIFT			3
#define MFC_STAT_OVER_TEMP_MASK			(1 << MFC_STAT_OVER_TEMP_SHIFT)
#define MFC_STAT_TX_OVER_CURR_MASK		(1 << MFC_STAT_TX_OVER_CURR_SHIFT)
#define MFC_STAT_TX_OVER_TEMP_MASK		(1 << MFC_STAT_TX_OVER_TEMP_SHIFT)
#define MFC_STAT_TX_FOD_MASK				(1 << MFC_STAT_TX_FOD_SHIFT)
#define MFC_STAT_TX_CONNECT_MASK			(1 << MFC_STAT_TX_CONNECT_SHIFT)

/* Interrupt Registers, INT_L (0x36), INT_H (0x37) */
#define MFC_INT_STAT_VOUT					MFC_STAT_VOUT_MASK
#define MFC_INT_STAT_VRECT				MFC_STAT_STAT_VRECT_MASK
#define MFC_INT_MODE_CHANGE				MFC_STAT_MODE_CHANGE_MASK
#define MFC_INT_TX_DATA_RECEIVED			MFC_STAT_TX_DATA_RECEIVED_MASK
#define MFC_INT_OVER_VOLT					MFC_STAT_OVER_VOL_MASK
#define MFC_INT_OVER_CURR					MFC_STAT_OVER_CURR_MASK

#define MFC_INT_OVER_TEMP					MFC_STAT_OVER_TEMP_MASK
#define MFC_INT_TX_OVER_CURR				MFC_STAT_TX_OVER_CURR_MASK
#define MFC_INT_TX_OVER_TEMP				MFC_STAT_TX_OVER_TEMP_MASK
#define MFC_INT_TX_FOD					MFC_STAT_TX_FOD_MASK
#define MFC_INT_TX_CONNECT				MFC_STAT_TX_CONNECT_MASK

/* End of Power Transfer Register, EPT (0x3B) (RX only) */
#define MFC_WPC_EPT_UNKNOWN						0
#define MFC_WPC_EPT_END_OF_CHG					1
#define MFC_WPC_EPT_INT_FAULT					2
#define MFC_WPC_EPT_OVER_TEMP					3
#define MFC_WPC_EPT_OVER_VOL					4
#define MFC_WPC_EPT_OVER_CURR					5
#define MFC_WPC_EPT_BATT_FAIL					6
#define MFC_WPC_EPT_RECONFIG					7
#define MFC_WPC_EPT_NO_RESPONSE					8

/* Proprietary Packet Header Register, PPP_Header(0x50) */
#define MFC_HEADER_END_SIG_STRENGTH		0x01
#define MFC_HEADER_END_POWER_TRANSFER		0x02
#define MFC_HEADER_END_CTR_ERROR			0x03
#define MFC_HEADER_END_RECEIVED_POWER		0x04
#define MFC_HEADER_END_CHARGE_STATUS		0x05
#define MFC_HEADER_POWER_CTR_HOLD_OFF		0x06
#define MFC_HEADER_AFC_CONF				0x28
#define MFC_HEADER_CONFIGURATION			0x51
#define MFC_HEADER_IDENTIFICATION			0x71
#define MFC_HEADER_EXTENDED_IDENT			0x81


/* TX Data Command Register, TX Data_COM (0x58) */
#define MFC_TX_DATA_COM_UNKNOWN			0x00
#define MFC_TX_DATA_COM_TX_ID				0x01
#define MFC_TX_DATA_COM_AFC_TX			0x02
#define MFC_TX_DATA_COM_ACK				0x03
#define MFC_TX_DATA_COM_NAK				0x04

/* END POWER TRANSFER CODES IN WPC */
#define MFC_EPT_CODE_UNKOWN				0x00
#define MFC_EPT_CODE_CHARGE_COMPLETE		0x01
#define MFC_EPT_CODE_INTERNAL_FAULT		0x02
#define MFC_EPT_CODE_OVER_TEMPERATURE		0x03
#define MFC_EPT_CODE_OVER_VOLTAGE			0x04
#define MFC_EPT_CODE_OVER_CURRENT			0x05
#define MFC_EPT_CODE_BATTERY_FAILURE		0x06
#define MFC_EPT_CODE_RECONFIGURE			0x07
#define MFC_EPT_CODE_NO_RESPONSE			0x08

#define MFC_POWER_MODE_MASK				(0x1 << 0)
#define MFC_SEND_USER_PKT_DONE_MASK		(0x1 << 7)
#define MFC_SEND_USER_PKT_ERR_MASK		(0x3 << 5)
#define MFC_SEND_ALIGN_MASK				(0x1 << 3)
#define MFC_SEND_EPT_CC_MASK				(0x1 << 0)
#define MFC_SEND_EOC_MASK					(0x1 << 0)

#define MFC_PTK_ERR_NO_ERR				0x00
#define MFC_PTK_ERR_ERR					0x01
#define MFC_PTK_ERR_ILLEGAL_HD			0x02
#define MFC_PTK_ERR_NO_DEF				0x03

#define MFC_FW_RESULT_DOWNLOADING			2
#define MFC_FW_RESULT_PASS				1
#define MFC_FW_RESULT_FAIL				0

#define MFC_FLASH_FW_HEX_PATH		"mfc/mfc_fw_flash.bin"
#define MFC_FW_SDCARD_BIN_PATH		"/sdcard/mfc_fw_flash.bin"

enum {
	MFC_EVENT_IRQ = 0,
	MFC_IRQS_NR,
};

#define MFC_CHIP_ID_MAJOR_1_REG			0x0070
#define MFC_CHIP_ID_MAJOR_0_REG			0x0074
#define MFC_CHIP_ID_MINOR_REG				0x0078
#define MFC_LDO_EN_REG					0x301c

/* PAD Vout */
enum {
	PAD_VOUT_5V = 0,
	PAD_VOUT_9V,
	PAD_VOUT_10V,
	PAD_VOUT_12V,
	PAD_VOUT_18V,
	PAD_VOUT_19V,
	PAD_VOUT_20V,
	PAD_VOUT_24V,
};

enum {
    MFC_ADC_VOUT = 0,
    MFC_ADC_VRECT,
    MFC_ADC_TX_ISENSE,
    MFC_ADC_RX_IOUT,
    MFC_ADC_DIE_TEMP,
//    MFC_ADC_ALLIGN_X,
//    MFC_ADC_ALLIGN_Y,
    MFC_ADC_OP_FRQ,
    MFC_ADC_PING_FRQ,
};

enum {
	MFC_END_SIG_STRENGTH = 0,
	MFC_END_POWER_TRANSFER,			/* 1 */
	MFC_END_CTR_ERROR,				/* 2 */
	MFC_END_RECEIVED_POWER,			/* 3 */
	MFC_END_CHARGE_STATUS,			/* 4 */
	MFC_POWER_CTR_HOLD_OFF,			/* 5 */
	MFC_AFC_CONF_5V,				/* 6 */
	MFC_AFC_CONF_10V,				/* 7 */
	MFC_CONFIGURATION,				/* 8 */
	MFC_IDENTIFICATION,				/* 9 */
	MFC_EXTENDED_IDENT,				/* 10 */
	MFC_LED_CONTROL_ON,				/* 11 */
	MFC_LED_CONTROL_OFF,			/* 12 */
	MFC_FAN_CONTROL_ON,				/* 13 */
	MFC_FAN_CONTROL_OFF,			/* 14 */
	MFC_REQUEST_AFC_TX,				/* 15 */
	MFC_REQUEST_TX_ID,				/* 16 */
};

enum mfc_irq_source {
	TOP_INT = 0,
};

enum mfc_irq {
	MFC_IRQ_STAT_VOUT = 0,
	MFC_IRQ_STAT_VRECT,
	MFC_IRQ_MODE_CHANGE,
	MFC_IRQ_TX_DATA_RECEIVED,
	MFC_IRQ_OVER_VOLT,
	MFC_IRQ_OVER_CURR,
	MFC_IRQ_OVER_TEMP,
	MFC_IRQ_TX_OVER_CURR,
	MFC_IRQ_TX_OVER_TEMP,
	MFC_IRQ_TX_FOD,
	MFC_IRQ_TX_CONNECT,
	MFC_IRQ_NR,
};

struct mfc_irq_data {
	int mask;
	enum mfc_irq_source group;
};

enum mfc_firmware_mode {
	MFC_RX_FIRMWARE = 0,
	MFC_TX_FIRMWARE,
};

enum mfc_ic_revision {
	MFC_IC_REVISION = 0,
	MFC_IC_FONT,
};

enum mfc_chip_id {
	MFC_CHIP_IDT = 0,
	MFC_CHIP_LSI,
};

enum mfc_headroom {
	MFC_HEADROOM_0 = 0,
	MFC_HEADROOM_1, /* 0.277V */
	MFC_HEADROOM_2, /* 0.497V */
	MFC_HEADROOM_3, /* 0.650V */
	MFC_HEADROOM_4, /* 0.030V */
	MFC_HEADROOM_5, /* 0.082V */
};

struct mfc_ppp_info {
	u8 header;
	u8 rx_data_com;
	u8 data_val[4];
	int data_size;
};

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }
static const struct mfc_irq_data mfc_irqs[] = {
	DECLARE_IRQ(MFC_IRQ_STAT_VOUT,	TOP_INT, 1 << 0),
};

//this file is generated by hex2txtOTP9220.exe. DO NOT MODIFY UNLESS YOU KNOW WHAT YOU ARE DOING!
static const u8 MTPBootloader9320[] = {
0x00, 0x04, 0x00, 0x20, 0xE3, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
0xFE, 0xE7, 0x00, 0x00, 0xF0, 0xB5, 0x45, 0x49, 0x00, 0x20, 0x0A, 0x88, 0x05, 0x46, 0x93, 0x06,
0x00, 0xD5, 0x04, 0x20, 0xD2, 0x06, 0x07, 0xD5, 0x8A, 0x78, 0x0B, 0x79, 0x1A, 0x43, 0x92, 0x07,
0x02, 0xD1, 0x20, 0x22, 0x10, 0x43, 0x01, 0x25, 0x3D, 0x4B, 0x5A, 0x22, 0x1A, 0x74, 0x3C, 0x4B,
0x20, 0x3B, 0x18, 0x72, 0x02, 0x20, 0x40, 0x1C, 0x20, 0x28, 0xFC, 0xD3, 0x39, 0x4C, 0x00, 0x26,
0xA6, 0x81, 0x48, 0x88, 0xE2, 0x13, 0x82, 0x18, 0x00, 0x2D, 0x09, 0xD0, 0x00, 0x20, 0x03, 0xE0,
0x45, 0x18, 0xAD, 0x68, 0x15, 0x50, 0x00, 0x1D, 0x8D, 0x88, 0x85, 0x42, 0xF8, 0xD8, 0x08, 0xE0,
0x00, 0x20, 0x03, 0xE0, 0x45, 0x18, 0x2D, 0x7A, 0x15, 0x54, 0x40, 0x1C, 0x8D, 0x88, 0x85, 0x42,
0xF8, 0xD8, 0x1E, 0x72, 0x2C, 0x48, 0xA0, 0x81, 0x02, 0x20, 0x00, 0x24, 0x23, 0x46, 0x0B, 0xE0,
0x5F, 0x18, 0x3E, 0x7A, 0xD5, 0x5C, 0xAE, 0x42, 0x05, 0xD0, 0x3D, 0x72, 0x00, 0x2C, 0x00, 0xD1,
0x4B, 0x80, 0x04, 0x20, 0x64, 0x1C, 0x5B, 0x1C, 0x8D, 0x88, 0x9D, 0x42, 0xF0, 0xD8, 0x8C, 0x80,
0xF0, 0xBD, 0x23, 0x49, 0x21, 0x48, 0x08, 0x60, 0x1E, 0x48, 0x08, 0x26, 0x40, 0x38, 0x86, 0x83,
0x5A, 0x21, 0x01, 0x70, 0x01, 0x22, 0x02, 0x71, 0x05, 0x21, 0x01, 0x72, 0x1D, 0x49, 0x81, 0x81,
0x16, 0x4D, 0x00, 0x21, 0x29, 0x80, 0xFF, 0x21, 0x49, 0x1E, 0xFD, 0xD1, 0x01, 0x21, 0x1A, 0x4B,
0x49, 0x02, 0x99, 0x80, 0x49, 0x1E, 0x01, 0x82, 0x81, 0x8B, 0x11, 0x43, 0x81, 0x83, 0x02, 0x27,
0x28, 0x78, 0x2C, 0x46, 0xC0, 0x07, 0xFB, 0xD0, 0x60, 0x88, 0xA1, 0x88, 0x08, 0x18, 0x80, 0xB2,
0x00, 0x22, 0x04, 0xE0, 0x13, 0x19, 0x1B, 0x7A, 0x18, 0x18, 0x80, 0xB2, 0x52, 0x1C, 0x91, 0x42,
0xF8, 0xD8, 0xE2, 0x88, 0x82, 0x42, 0x01, 0xD0, 0x2E, 0x80, 0xE9, 0xE7, 0x00, 0x29, 0x03, 0xD0,
0xFF, 0xF7, 0x78, 0xFF, 0x20, 0x80, 0xE3, 0xE7, 0x2F, 0x80, 0xE1, 0xE7, 0x00, 0x04, 0x00, 0x20,
0x40, 0x5C, 0x00, 0x40, 0x40, 0x30, 0x00, 0x40, 0xFF, 0x01, 0x00, 0x00, 0xFF, 0x0F, 0x00, 0x00,
0x80, 0xE1, 0x00, 0xE0, 0x04, 0x0E, 0x00, 0x00, 0x00, 0x34, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00,
};

#define is_mfc_hv_wireless_type(cable_type) ( \
	cable_type == MFC_PAD_WPC_AFC || \
	cable_type == MFC_PAD_WPC_STAND_HV || \
	cable_type == MFC_PAD_PREPARE_HV || \
	cable_type == MFC_PAD_WPC_VEHICLE_HV)

struct mfc_charger_platform_data {
	int pad_mode;
	int wpc_det;
	int irq_wpc_det;
	int wpc_int;
	int mst_pwr_en;
	int wpc_en;
	int irq_wpc_int;
	int cs100_status;
	int vout_status;
	int wireless_cc_cv;
	int siop_level;
	u8 capacity;
	int cable_type;
	bool default_voreg;
	int is_charging;
	u32 *fod_a4wp_data_cv;
	u32 *fod_wpc_data_cv;
	u32 *fod_pma_data_cv;
	u32 *fod_a4wp_data;
	u32 *fod_wpc_data;
	u32 *fod_pma_data;
	u32 *fod_hero_5v_data;
	int fod_data_check;
	bool ic_on_mode;
	int hw_rev_changed; /* this is only for noble/zero2 */
	int otp_firmware_result;
	int tx_firmware_result;
	int wc_ic_grade;
	int wc_ic_rev;
	int otp_firmware_ver;
	int tx_firmware_ver;
	int vout;
	int vrect;
	u8 tx_data_cmd;
	u8 tx_data_val;
	char *wireless_charger_name;
	char *wired_charger_name;
	char *fuelgauge_name;
	int tx_status;
	int wpc_cc_cv_vout;
	int wpc_cv_call_vout;
	int wpc_cc_call_vout;
	int opfq_cnt;
	int hv_vout_wa;
	int mst_switch_delay;
	int wc_cover_rpp;
	int wc_hv_rpp;
};

#define mfc_charger_platform_data_t \
	struct mfc_charger_platform_data

#define MST_MODE_0			0
#define MST_MODE_2			1

struct mfc_charger_data {
	struct i2c_client				*client;
	struct device					*dev;
	mfc_charger_platform_data_t	*pdata;
	struct mutex io_lock;
	const struct firmware *firm_data_bin;

	int wc_w_state;

	struct power_supply *psy_chg;
	struct wake_lock wpc_wake_lock;
	struct wake_lock mst_wake_lock;
	struct wake_lock wpc_update_lock;
	struct wake_lock wpc_opfq_lock;
	struct wake_lock wpc_afc_vout_lock;
	struct wake_lock wpc_vout_mode_lock;
	struct workqueue_struct *wqueue;
	struct work_struct	wcin_work;
	struct delayed_work	wpc_det_work;
	struct delayed_work	wpc_opfq_work;
	struct delayed_work	wpc_isr_work;
	struct delayed_work	wpc_tx_id_work;
	struct delayed_work mst_off_work;
	struct delayed_work	wpc_int_req_work;
	struct delayed_work	wpc_fw_update_work;
	struct delayed_work	wpc_afc_vout_work;
	struct delayed_work	wpc_fw_booting_work;
	struct delayed_work	wpc_vout_mode_work;
	struct delayed_work	wpc_cm_fet_work;

	u16 addr;
	int size;
	int is_afc;
	int pad_vout;
	int is_mst_on; /* mst */
	int chip_id;
	int fw_cmd;
	int vout_mode;
	int is_full_status;
	int mst_off_lock;
	bool is_otg_on;
	int led_cover;
	bool is_probed;
	bool is_afc_tx;
	int tx_id;

	struct mutex fw_lock;
};

#endif /* __MFC_CHARGER_H */
