/*
 * sec_charging_common.h
 * Samsung Mobile Charging Common Header
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
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

#ifndef __SEC_CHARGING_COMMON_H
#define __SEC_CHARGING_COMMON_H __FILE__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/wakelock.h>
#include <dt-bindings/battery/sec_battery_param_define.h>

/* definitions */
#define SEC_BATTERY_CABLE_HV_WIRELESS_ETX	100

#define WC_AUTH_MSG		"@WC_AUTH "
#define WC_TX_MSG		"@Tx_Mode "

#define MFC_LDO_ON		1
#define MFC_LDO_OFF		0

enum power_supply_ext_property {
	POWER_SUPPLY_EXT_PROP_CHECK_SLAVE_I2C = POWER_SUPPLY_PROP_MAX,
	POWER_SUPPLY_EXT_PROP_MULTI_CHARGER_MODE,
	POWER_SUPPLY_EXT_PROP_WIRELESS_OP_FREQ,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TRX_CMD,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TRX_VAL,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ID,
	POWER_SUPPLY_EXT_PROP_WIRELESS_ERR,
	POWER_SUPPLY_EXT_PROP_WIRELESS_SWITCH,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ENABLE,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_VOUT,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_IOUT,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_UNO_VIN,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_UNO_IIN,
	POWER_SUPPLY_EXT_PROP_WIRELESS_RX_CONNECTED,
	POWER_SUPPLY_EXT_PROP_WIRELESS_DUO_RX_POWER,
	POWER_SUPPLY_EXT_PROP_WIRELESS_AUTH_ADT_STATUS,	
	POWER_SUPPLY_EXT_PROP_WIRELESS_AUTH_ADT_DATA,
	POWER_SUPPLY_EXT_PROP_WIRELESS_AUTH_ADT_SIZE,
	POWER_SUPPLY_EXT_PROP_WIRELESS_RX_TYPE,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_ERR,
	POWER_SUPPLY_EXT_PROP_WIRELESS_MIN_DUTY,
	POWER_SUPPLY_EXT_PROP_WIRELESS_SEND_FSK,
	POWER_SUPPLY_EXT_PROP_WIRELESS_RX_VOUT,
	POWER_SUPPLY_EXT_PROP_AICL_CURRENT,
	POWER_SUPPLY_EXT_PROP_CHECK_MULTI_CHARGE,
	POWER_SUPPLY_EXT_PROP_CHIP_ID,
	POWER_SUPPLY_EXT_PROP_ERROR_CAUSE,
	POWER_SUPPLY_EXT_PROP_SYSOVLO,
	POWER_SUPPLY_EXT_PROP_VBAT_OVP,
	POWER_SUPPLY_EXT_PROP_INBAT_VOLTAGE_FGSRC_SWITCHING,
	POWER_SUPPLY_EXT_PROP_USB_CONFIGURE,
	POWER_SUPPLY_EXT_PROP_WDT_STATUS,
	POWER_SUPPLY_EXT_PROP_HV_DISABLE,
	POWER_SUPPLY_EXT_PROP_FUELGAUGE_RESET,
	POWER_SUPPLY_EXT_PROP_FACTORY_VOLTAGE_REGULATION,
	POWER_SUPPLY_EXT_PROP_ANDIG_IVR_SWITCH,
	POWER_SUPPLY_EXT_PROP_FUELGAUGE_FACTORY,
	POWER_SUPPLY_EXT_PROP_DISABLE_FACTORY_MODE,
	POWER_SUPPLY_EXT_PROP_SUB_PBA_TEMP_REC,
	POWER_SUPPLY_EXT_PROP_CHARGE_POWER,
	POWER_SUPPLY_EXT_PROP_MEASURE_SYS,
	POWER_SUPPLY_EXT_PROP_MEASURE_INPUT,
	POWER_SUPPLY_EXT_PROP_WC_CONTROL,
	POWER_SUPPLY_EXT_PROP_CHGINSEL,
	POWER_SUPPLY_EXT_PROP_MONITOR_WORK,
	POWER_SUPPLY_EXT_PROP_JIG_GPIO,
	POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST,
	POWER_SUPPLY_EXT_PROP_AUTO_SHIPMODE_CONTROL,
	POWER_SUPPLY_EXT_PROP_WIRELESS_TIMER_ON,
	POWER_SUPPLY_EXT_PROP_CALL_EVENT,
	POWER_SUPPLY_EXT_PROP_WATER_DETECT,
	POWER_SUPPLY_EXT_PROP_SURGE,
	POWER_SUPPLY_EXT_PROP_OVERHEAT_NOTIFY,
	POWER_SUPPLY_EXT_PROP_OVERHEAT_HICCUP,
#if defined(CONFIG_DUAL_BATTERY)
	POWER_SUPPLY_EXT_PROP_CHGIN_OK,
	POWER_SUPPLY_EXT_PROP_SUPLLEMENT_MODE,
	POWER_SUPPLY_EXT_PROP_RECHG_ON,
	POWER_SUPPLY_EXT_PROP_EOC_ON,
	POWER_SUPPLY_EXT_PROP_DISCHG_MODE,
	POWER_SUPPLY_EXT_PROP_CHG_MODE,
	POWER_SUPPLY_EXT_PROP_CHG_VOLTAGE,
	POWER_SUPPLY_EXT_PROP_BAT_VOLTAGE,
	POWER_SUPPLY_EXT_PROP_CHG_CURRENT,
	POWER_SUPPLY_EXT_PROP_DISCHG_CURRENT,
	POWER_SUPPLY_EXT_PROP_FASTCHG_LIMIT_CURRENT,
	POWER_SUPPLY_EXT_PROP_TRICKLECHG_LIMIT_CURRENT,
	POWER_SUPPLY_EXT_PROP_DISCHG_LIMIT_CURRENT,
	POWER_SUPPLY_EXT_PROP_RECHG_VOLTAGE,
	POWER_SUPPLY_EXT_PROP_EOC_VOLTAGE,
	POWER_SUPPLY_EXT_PROP_EOC_CURRENT,
	POWER_SUPPLY_EXT_PROP_POWERMETER_ENABLE,
	POWER_SUPPLY_EXT_PROP_TSD_ENABLE,
	POWER_SUPPLY_EXT_PROP_DUAL_BAT_DET,
#endif
	POWER_SUPPLY_EXT_PROP_CURRENT_EVENT,
	POWER_SUPPLY_EXT_PROP_MAX_DUTY_EVENT,
	POWER_SUPPLY_EXT_PROP_VCHGIN_CHANGE,
#if defined(CONFIG_WIRELESS_TX_MODE)
	POWER_SUPPLY_EXT_PROP_WIRELESS_TX_AVG_CURR,
#endif
	POWER_SUPPLY_EXT_PROP_CURRENT_MEASURE,
#if defined(CONFIG_DIRECT_CHARGING)
	POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE,
	POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED_DC,
	POWER_SUPPLY_EXT_PROP_DIRECT_DONE,
	POWER_SUPPLY_EXT_PROP_DIRECT_FIXED_PDO,
	POWER_SUPPLY_EXT_PROP_DIRECT_WDT_CONTROL,
	POWER_SUPPLY_EXT_PROP_DIRECT_VOLTAGE_MAX,
	POWER_SUPPLY_EXT_PROP_DIRECT_CURRENT_MAX,
	POWER_SUPPLY_EXT_PROP_DIRECT_FLOAT_MAX,
	POWER_SUPPLY_EXT_PROP_DIRECT_ADC_CTRL,
	POWER_SUPPLY_EXT_PROP_DIRECT_HV_PDO,
	POWER_SUPPLY_EXT_PROP_DIRECT_PPS,
	POWER_SUPPLY_EXT_PROP_DIRECT_PPS_FAILED,
	POWER_SUPPLY_EXT_PROP_DIRECT_PPS_READY,
	POWER_SUPPLY_EXT_PROP_DIRECT_DETACHED,
	POWER_SUPPLY_EXT_PROP_DIRECT_BUCK_OFF,
	POWER_SUPPLY_EXT_PROP_DIRECT_HARD_RESET,
	POWER_SUPPLY_EXT_PROP_DIRECT_PPS_DISABLE,
	POWER_SUPPLY_EXT_PRO_CHANGE_CHARGING_SOURCE,
#endif
#if defined(CONFIG_FUELGAUGE_S2MU004) || defined(CONFIG_FUELGAUGE_S2MU005) || defined(CONFIG_FUELGAUGE_S2MU106) || defined(CONFIG_FUELGAUGE_S2MU205) || defined(CONFIG_FUELGAUGE_S2MU107)
	POWER_SUPPLY_EXT_PROP_UPDATE_BATTERY_DATA,
#endif
	POWER_SUPPLY_EXT_PROP_WD_QBATTOFF,
	POWER_SUPPLY_EXT_PROP_WPC_DET_STATUS,
	POWER_SUPPLY_EXT_PROP_CHARGE_BOOST,
	POWER_SUPPLY_EXT_PROP_CHARGE_MODE,
	POWER_SUPPLY_EXT_PROP_FLED_BOOST_ON,
	POWER_SUPPLY_EXT_PROP_FLED_BOOST_OFF,
	POWER_SUPPLY_EXT_PROP_BYPASS_MODE_DISABLE,
};

enum rx_device_type {
	NO_DEV = 0,
	OTHER_DEV,
	SS_GEAR,
	SS_PHONE,
	SS_BUDS,
};

enum sec_battery_usb_conf {
	USB_CURRENT_UNCONFIGURED = 100,
	USB_CURRENT_HIGH_SPEED = 500,
	USB_CURRENT_SUPER_SPEED = 900,
};

enum power_supply_ext_health {
	POWER_SUPPLY_HEALTH_VSYS_OVP = POWER_SUPPLY_HEALTH_MAX,
	POWER_SUPPLY_HEALTH_VBAT_OVP,
	POWER_SUPPLY_HEALTH_DC_ERR,
};

enum sec_battery_voltage_mode {
	/* average voltage */
	SEC_BATTERY_VOLTAGE_AVERAGE = 0,
	/* open circuit voltage */
	SEC_BATTERY_VOLTAGE_OCV,
};

enum sec_battery_current_type {
	/* uA */
	SEC_BATTERY_CURRENT_UA = 0,
	/* mA */
	SEC_BATTERY_CURRENT_MA,
};

enum sec_battery_voltage_type {
	/* uA */
	SEC_BATTERY_VOLTAGE_UV = 0,
	/* mA */
	SEC_BATTERY_VOLTAGE_MV,
};

#if defined(CONFIG_DUAL_BATTERY)
enum sec_battery_dual_mode { 
	SEC_DUAL_BATTERY_MAIN = 0,
	SEC_DUAL_BATTERY_SUB,
};
#endif

enum sec_battery_capacity_mode {
	/* designed capacity */
	SEC_BATTERY_CAPACITY_DESIGNED = 0,
	/* absolute capacity by fuel gauge */
	SEC_BATTERY_CAPACITY_ABSOLUTE,
	/* temperary capacity in the time */
	SEC_BATTERY_CAPACITY_TEMPERARY,
	/* current capacity now */
	SEC_BATTERY_CAPACITY_CURRENT,
	/* cell aging information */
	SEC_BATTERY_CAPACITY_AGEDCELL,
	/* charge count */
	SEC_BATTERY_CAPACITY_CYCLE,
	/* full capacity rep */
	SEC_BATTERY_CAPACITY_FULL,
	/* QH capacity */
	SEC_BATTERY_CAPACITY_QH,
	/* vfsoc */
	SEC_BATTERY_CAPACITY_VFSOC,
};

enum sec_wireless_info_mode {
	SEC_WIRELESS_OTP_FIRM_RESULT = 0,
	SEC_WIRELESS_IC_REVISION,
	SEC_WIRELESS_IC_GRADE,
	SEC_WIRELESS_OTP_FIRM_VER_BIN,
	SEC_WIRELESS_OTP_FIRM_VER,
	SEC_WIRELESS_TX_FIRM_RESULT,
	SEC_WIRELESS_TX_FIRM_VER,
	SEC_TX_FIRMWARE,
	SEC_WIRELESS_OTP_FIRM_VERIFY,
	SEC_WIRELESS_MST_SWITCH_VERIFY,
};

enum sec_wireless_firm_update_mode {
	SEC_WIRELESS_RX_SDCARD_MODE = 0,
	SEC_WIRELESS_RX_BUILT_IN_MODE,
	SEC_WIRELESS_TX_ON_MODE,
	SEC_WIRELESS_TX_OFF_MODE,
	SEC_WIRELESS_RX_INIT,
	SEC_WIRELESS_RX_SPU_MODE,
};

enum sec_tx_sharging_mode {
	SEC_TX_OFF = 0,
	SEC_TX_STANDBY,
	SEC_TX_POWER_TRANSFER,
	SEC_TX_ERROR,
};

enum sec_wireless_auth_mode {
	WIRELESS_AUTH_WAIT = 0,
	WIRELESS_AUTH_START,
	WIRELESS_AUTH_SENT,
	WIRELESS_AUTH_RECEIVED,
	WIRELESS_AUTH_FAIL,
	WIRELESS_AUTH_PASS,
};

enum sec_wireless_control_mode {
	WIRELESS_VOUT_OFF = 0,
	WIRELESS_VOUT_NORMAL_VOLTAGE,	/* 5V , reserved by factory */
	WIRELESS_VOUT_RESERVED,			/* 6V */
	WIRELESS_VOUT_HIGH_VOLTAGE,		/* 9V , reserved by factory */
	WIRELESS_VOUT_CC_CV_VOUT,
	WIRELESS_VOUT_CALL,
	WIRELESS_VOUT_5V,
	WIRELESS_VOUT_9V,
	WIRELESS_VOUT_10V,
	WIRELESS_VOUT_5V_STEP,
	WIRELESS_VOUT_5_5V_STEP,
	WIRELESS_VOUT_9V_STEP,
	WIRELESS_VOUT_10V_STEP,
	WIRELESS_PAD_FAN_OFF,
	WIRELESS_PAD_FAN_ON,
	WIRELESS_PAD_LED_OFF,
	WIRELESS_PAD_LED_ON,
	WIRELESS_VRECT_ADJ_ON,
	WIRELESS_VRECT_ADJ_OFF,
	WIRELESS_VRECT_ADJ_ROOM_0,
	WIRELESS_VRECT_ADJ_ROOM_1,
	WIRELESS_VRECT_ADJ_ROOM_2,
	WIRELESS_VRECT_ADJ_ROOM_3,
	WIRELESS_VRECT_ADJ_ROOM_4,
	WIRELESS_VRECT_ADJ_ROOM_5,
	WIRELESS_CLAMP_ENABLE,
};

enum sec_wireless_tx_vout {
	WC_TX_VOUT_5_0V = 0,
	WC_TX_VOUT_5_5V,
	WC_TX_VOUT_6_0V,
	WC_TX_VOUT_6_5V,
	WC_TX_VOUT_7_0V,
	WC_TX_VOUT_7_5V,
	WC_TX_VOUT_8_0V,
	WC_TX_VOUT_8_5V,
	WC_TX_VOUT_9_0V,
	WC_TX_VOUT_OFF=100,
};

enum sec_wireless_pad_mode {
	SEC_WIRELESS_PAD_NONE = 0,
	SEC_WIRELESS_PAD_WPC,
	SEC_WIRELESS_PAD_WPC_HV,
	SEC_WIRELESS_PAD_WPC_PACK,
	SEC_WIRELESS_PAD_WPC_PACK_HV,
	SEC_WIRELESS_PAD_WPC_STAND,
	SEC_WIRELESS_PAD_WPC_STAND_HV,
	SEC_WIRELESS_PAD_PMA,
	SEC_WIRELESS_PAD_VEHICLE,
	SEC_WIRELESS_PAD_VEHICLE_HV,
	SEC_WIRELESS_PAD_PREPARE_HV,
	SEC_WIRELESS_PAD_TX,
	SEC_WIRELESS_PAD_WPC_PREPARE_DUO_HV_20,
	SEC_WIRELESS_PAD_WPC_DUO_HV_20,
	SEC_WIRELESS_PAD_WPC_DUO_HV_20_LIMIT,
	SEC_WIRELESS_PAD_FAKE,
};

enum sec_wireless_pad_id {
	WC_PAD_ID_UNKNOWN	= 0x00,
	/* 0x01~1F : Single Port */
	WC_PAD_ID_SNGL_NOBLE = 0x10,
	WC_PAD_ID_SNGL_VEHICLE,
	WC_PAD_ID_SNGL_MINI,
	WC_PAD_ID_SNGL_ZERO,
	WC_PAD_ID_SNGL_DREAM,
	/* 0x20~2F : Multi Port */
	/* 0x30~3F : Stand Type */
	WC_PAD_ID_STAND_HERO = 0x30,
	WC_PAD_ID_STAND_DREAM,
	/* 0x40~4F : External Battery Pack */
	WC_PAD_ID_EXT_BATT_PACK = 0x40,
	WC_PAD_ID_EXT_BATT_PACK_TA,
	/* 0x50~6F : Reserved */
	WC_PAD_ID_MAX = 0x6F,
};

enum sec_battery_adc_channel {
	SEC_BAT_ADC_CHANNEL_CABLE_CHECK = 0,
	SEC_BAT_ADC_CHANNEL_BAT_CHECK,
	SEC_BAT_ADC_CHANNEL_TEMP,
	SEC_BAT_ADC_CHANNEL_TEMP_AMBIENT,
	SEC_BAT_ADC_CHANNEL_FULL_CHECK,
	SEC_BAT_ADC_CHANNEL_VOLTAGE_NOW,
	SEC_BAT_ADC_CHANNEL_CHG_TEMP,
	SEC_BAT_ADC_CHANNEL_INBAT_VOLTAGE,
	SEC_BAT_ADC_CHANNEL_DISCHARGING_CHECK,
	SEC_BAT_ADC_CHANNEL_DISCHARGING_NTC,
	SEC_BAT_ADC_CHANNEL_WPC_TEMP,
	SEC_BAT_ADC_CHANNEL_SLAVE_CHG_TEMP,
	SEC_BAT_ADC_CHANNEL_USB_TEMP,
	SEC_BAT_ADC_CHANNEL_SUB_BAT_TEMP,
	SEC_BAT_ADC_CHANNEL_BLKT_TEMP,
	SEC_BAT_ADC_CHANNEL_NUM,
};

enum sec_battery_charge_mode {
	SEC_BAT_CHG_MODE_BUCK_OFF = 0, /* buck, chg off */
	SEC_BAT_CHG_MODE_CHARGING_OFF,
	SEC_BAT_CHG_MODE_CHARGING, /* buck, chg on */
//	SEC_BAT_CHG_MODE_BUCK_ON,
	SEC_BAT_CHG_MODE_OTG_ON,
	SEC_BAT_CHG_MODE_OTG_OFF,
	SEC_BAT_CHG_MODE_UNO_ON,
	SEC_BAT_CHG_MODE_UNO_OFF,
	SEC_BAT_CHG_MODE_UNO_ONLY,
	SEC_BAT_CHG_MODE_MAX,
};

/* charging mode */
enum sec_battery_charging_mode {
	/* no charging */
	SEC_BATTERY_CHARGING_NONE = 0,
	/* 1st charging */
	SEC_BATTERY_CHARGING_1ST,
	/* 2nd charging */
	SEC_BATTERY_CHARGING_2ND,
	/* recharging */
	SEC_BATTERY_CHARGING_RECHARGING,
};

/* POWER_SUPPLY_EXT_PROP_MEASURE_SYS */
enum sec_battery_measure_sys {
	SEC_BATTERY_ISYS_MA = 0,
	SEC_BATTERY_ISYS_UA,
	SEC_BATTERY_ISYS_AVG_MA,
	SEC_BATTERY_ISYS_AVG_UA,
	SEC_BATTERY_VSYS,
};

/* POWER_SUPPLY_EXT_PROP_MEASURE_INPUT */
enum sec_battery_measure_input {
	SEC_BATTERY_IIN_MA = 0,
	SEC_BATTERY_IIN_UA,
	SEC_BATTERY_VBYP,
	SEC_BATTERY_VIN_MA,
	SEC_BATTERY_VIN_UA,
};

/* tx_event */
#define BATT_TX_EVENT_WIRELESS_TX_STATUS		0x00000001
#define BATT_TX_EVENT_WIRELESS_RX_CONNECT		0x00000002
#define BATT_TX_EVENT_WIRELESS_TX_FOD			0x00000004
#define BATT_TX_EVENT_WIRELESS_TX_HIGH_TEMP		0x00000008
#define BATT_TX_EVENT_WIRELESS_RX_UNSAFE_TEMP	0x00000010
#define BATT_TX_EVENT_WIRELESS_RX_CHG_SWITCH	0x00000020
#define BATT_TX_EVENT_WIRELESS_RX_CS100			0x00000040
#define BATT_TX_EVENT_WIRELESS_TX_OTG_ON		0x00000080
#define BATT_TX_EVENT_WIRELESS_TX_LOW_TEMP		0x00000100
#define BATT_TX_EVENT_WIRELESS_TX_SOC_DRAIN		0x00000200
#define BATT_TX_EVENT_WIRELESS_TX_CRITICAL_EOC	0x00000400
#define BATT_TX_EVENT_WIRELESS_TX_CAMERA_ON		0x00000800
#define BATT_TX_EVENT_WIRELESS_TX_OCP			0x00001000
#define BATT_TX_EVENT_WIRELESS_TX_MISALIGN      0x00002000
#define BATT_TX_EVENT_WIRELESS_TX_ETC			0x00004000
#define BATT_TX_EVENT_WIRELESS_TX_RETRY			0x00008000
#define BATT_TX_EVENT_WIRELESS_ALL_MASK			0x0000ffff

#define SEC_BAT_TX_RETRY_NONE			0x0000
#define SEC_BAT_TX_RETRY_MISALIGN		0x0001
#define SEC_BAT_TX_RETRY_CAMERA			0x0002
#define SEC_BAT_TX_RETRY_CALL			0x0004
#define SEC_BAT_TX_RETRY_MIX_TEMP		0x0008
#define SEC_BAT_TX_RETRY_HIGH_TEMP		0x0010
#define SEC_BAT_TX_RETRY_LOW_TEMP		0x0020

/* ext_event */
#define BATT_EXT_EVENT_NONE			0x00000000
#define BATT_EXT_EVENT_CAMERA		0x00000001
#define BATT_EXT_EVENT_DEX			0x00000002
#define BATT_EXT_EVENT_CALL			0x00000004

#define SEC_BAT_ERROR_CAUSE_NONE		0x0000
#define SEC_BAT_ERROR_CAUSE_FG_INIT_FAIL	0x0001
#define SEC_BAT_ERROR_CAUSE_I2C_FAIL		0xFFFFFFFF

/* monitor activation */
enum sec_battery_polling_time_type {
	/* same order with power supply status */
	SEC_BATTERY_POLLING_TIME_BASIC = 0,
	SEC_BATTERY_POLLING_TIME_CHARGING,
	SEC_BATTERY_POLLING_TIME_DISCHARGING,
	SEC_BATTERY_POLLING_TIME_NOT_CHARGING,
	SEC_BATTERY_POLLING_TIME_SLEEP,
};

/* BATT_INBAT_VOLTAGE */
enum sec_battery_inbat_fgsrc_switching {
	SEC_BAT_INBAT_FGSRC_SWITCHING_ON = 0,
	SEC_BAT_INBAT_FGSRC_SWITCHING_OFF,
	SEC_BAT_FGSRC_SWITCHING_ON,
	SEC_BAT_FGSRC_SWITCHING_OFF,
};

/* full check condition type (can be used overlapped) */
#define sec_battery_full_condition_t unsigned int

/* battery check : POWER_SUPPLY_PROP_PRESENT */
enum sec_battery_check {
	/* No Check for internal battery */
	SEC_BATTERY_CHECK_NONE,
	/* by ADC */
	SEC_BATTERY_CHECK_ADC,
	/* by callback function (battery certification by 1 wired)*/
	SEC_BATTERY_CHECK_CALLBACK,
	/* by PMIC */
	SEC_BATTERY_CHECK_PMIC,
	/* by fuel gauge */
	SEC_BATTERY_CHECK_FUELGAUGE,
	/* by charger */
	SEC_BATTERY_CHECK_CHARGER,
	/* by interrupt (use check_battery_callback() to check battery) */
	SEC_BATTERY_CHECK_INT,
#if defined(CONFIG_DUAL_BATTERY)
	/* by dual battery */
	SEC_BATTERY_CHECK_DUAL_BAT_GPIO,
#endif	
};
#define sec_battery_check_t \
	enum sec_battery_check

/* cable check (can be used overlapped) */
#define sec_battery_cable_check_t unsigned int

/* check cable source (can be used overlapped) */
#define sec_battery_cable_source_t unsigned int

/* capacity calculation type (can be used overlapped) */
#define sec_fuelgauge_capacity_type_t int

/* SEC_FUELGAUGE_CAPACITY_TYPE_RESET
  * use capacity information to reset fuel gauge
  * (only for driver algorithm, can NOT be set by user)
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_RESET	(-1)
/* SEC_FUELGAUGE_CAPACITY_TYPE_RAW
  * use capacity information from fuel gauge directly
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_RAW		1
/* SEC_FUELGAUGE_CAPACITY_TYPE_SCALE
  * rescale capacity by scaling, need min and max value for scaling
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_SCALE	2
/* SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE
  * change only maximum capacity dynamically
  * to keep time for every SOC unit
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE	4
/* SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC
  * change capacity value by only -1 or +1
  * no sudden change of capacity
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC	8
/* SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL
  * skip current capacity value
  * if it is abnormal value
  */
#define SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL	16

#define SEC_FUELGAUGE_CAPACITY_TYPE_CAPACITY_POINT	32

/* charger function settings (can be used overlapped) */
#define sec_charger_functions_t unsigned int
/* SEC_CHARGER_NO_GRADUAL_CHARGING_CURRENT
 * disable gradual charging current setting
 * SUMMIT:AICL, MAXIM:regulation loop
 */
#define SEC_CHARGER_NO_GRADUAL_CHARGING_CURRENT		1

/* SEC_CHARGER_MINIMUM_SIOP_CHARGING_CURRENT
 * charging current should be over than USB charging current
 */
#define SEC_CHARGER_MINIMUM_SIOP_CHARGING_CURRENT	2

#if defined(CONFIG_BATTERY_AGE_FORECAST)
typedef struct sec_age_data {
	unsigned int cycle;
	unsigned int float_voltage;
	unsigned int recharge_condition_vcell;
	unsigned int full_condition_vcell;
	unsigned int full_condition_soc;
#if defined(CONFIG_STEP_CHARGING)
	unsigned int step_charging_condition;
#endif
} sec_age_data_t;
#endif

typedef struct {
	unsigned int cycle;
	unsigned int asoc;
} battery_health_condition;

static inline struct power_supply *get_power_supply_by_name(char *name)
{
	if (!name)
		return (struct power_supply *)NULL;
	else
		return power_supply_get_by_name(name);
}

#define psy_do_property(name, function, property, value) \
({	\
	struct power_supply *psy;	\
	int ret = 0;	\
	psy = get_power_supply_by_name((name));	\
	if (!psy) {	\
		pr_err("%s: Fail to "#function" psy (%s)\n",	\
			__func__, (name));	\
		value.intval = 0;	\
		ret = -ENOENT;	\
	} else {	\
		if (psy->desc->function##_property != NULL) { \
			ret = psy->desc->function##_property(psy, \
				(enum power_supply_property) (property), &(value)); \
			if (ret < 0) {	\
				pr_err("%s: Fail to %s "#function" (%d=>%d)\n", \
						__func__, name, (property), ret);	\
				value.intval = 0;	\
			}	\
		} else {	\
			ret = -ENOSYS;	\
		}	\
		power_supply_put(psy);		\
	}					\
	ret;	\
})

#define is_hv_wireless_pad_type(cable_type) ( \
	cable_type == SEC_WIRELESS_PAD_WPC_HV || \
	cable_type == SEC_WIRELESS_PAD_WPC_PACK_HV || \
	cable_type == SEC_WIRELESS_PAD_WPC_STAND_HV || \
	cable_type == SEC_WIRELESS_PAD_VEHICLE_HV || \
	cable_type == SEC_WIRELESS_PAD_WPC_DUO_HV_20 || \
	cable_type == SEC_WIRELESS_PAD_WPC_DUO_HV_20_LIMIT)

#define is_nv_wireless_pad_type(cable_type) ( \
	cable_type == SEC_WIRELESS_PAD_WPC || \
	cable_type == SEC_WIRELESS_PAD_WPC_PACK || \
	cable_type == SEC_WIRELESS_PAD_WPC_STAND || \
	cable_type == SEC_WIRELESS_PAD_PMA || \
	cable_type == SEC_WIRELESS_PAD_VEHICLE || \
	cable_type == SEC_WIRELESS_PAD_WPC_PREPARE_DUO_HV_20 || \
	cable_type == SEC_WIRELESS_PAD_PREPARE_HV)

#define is_wireless_pad_type(cable_type) \
	(is_hv_wireless_pad_type(cable_type) || is_nv_wireless_pad_type(cable_type))

#define is_hv_wireless_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_HV_WIRELESS || \
	cable_type == SEC_BATTERY_CABLE_HV_WIRELESS_ETX || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_STAND || \
	cable_type == SEC_BATTERY_CABLE_HV_WIRELESS_20 || \
	cable_type == SEC_BATTERY_CABLE_HV_WIRELESS_20_LIMIT || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_HV_PACK)

#define is_nv_wireless_type(cable_type)	( \
	cable_type == SEC_BATTERY_CABLE_WIRELESS || \
	cable_type == SEC_BATTERY_CABLE_PMA_WIRELESS || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_PACK || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_STAND || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_VEHICLE || \
	cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV || \
	cable_type == SEC_BATTERY_CABLE_PREPARE_WIRELESS_20 || \
	cable_type == SEC_BATTERY_CABLE_WIRELESS_TX)

#define is_wireless_type(cable_type) \
	(is_hv_wireless_type(cable_type) || is_nv_wireless_type(cable_type))

#define is_not_wireless_type(cable_type) ( \
	cable_type != SEC_BATTERY_CABLE_WIRELESS && \
	cable_type != SEC_BATTERY_CABLE_PMA_WIRELESS && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_PACK && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_STAND && \
	cable_type != SEC_BATTERY_CABLE_HV_WIRELESS && \
	cable_type != SEC_BATTERY_CABLE_HV_WIRELESS_ETX && \
	cable_type != SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_HV_STAND && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_VEHICLE && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_TX && \
	cable_type != SEC_BATTERY_CABLE_PREPARE_WIRELESS_20 && \
	cable_type != SEC_BATTERY_CABLE_HV_WIRELESS_20 && \
	cable_type != SEC_BATTERY_CABLE_HV_WIRELESS_20_LIMIT && \
	cable_type != SEC_BATTERY_CABLE_WIRELESS_HV_PACK)

#define is_wired_type(cable_type) \
	(is_not_wireless_type(cable_type) && (cable_type != SEC_BATTERY_CABLE_NONE) && \
	(cable_type != SEC_BATTERY_CABLE_OTG))

#define is_hv_qc_wire_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_QC20 || \
	cable_type == SEC_BATTERY_CABLE_QC30)

#define is_hv_afc_wire_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_9V_ERR || \
	cable_type == SEC_BATTERY_CABLE_9V_TA || \
	cable_type == SEC_BATTERY_CABLE_9V_UNKNOWN || \
	cable_type == SEC_BATTERY_CABLE_12V_TA)

#define is_hv_wire_9v_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_9V_ERR || \
	cable_type == SEC_BATTERY_CABLE_9V_TA || \
	cable_type == SEC_BATTERY_CABLE_9V_UNKNOWN || \
	cable_type == SEC_BATTERY_CABLE_QC20)

#define is_hv_wire_12v_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_12V_TA || \
	cable_type == SEC_BATTERY_CABLE_QC30)

#define is_hv_wire_type(cable_type) ( \
	is_hv_afc_wire_type(cable_type) || is_hv_qc_wire_type(cable_type))

#define is_nocharge_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_NONE || \
	cable_type == SEC_BATTERY_CABLE_OTG || \
	cable_type == SEC_BATTERY_CABLE_POWER_SHARING)

#define is_slate_mode(battery) ((battery->current_event & SEC_BAT_CURRENT_EVENT_SLATE) \
		== SEC_BAT_CURRENT_EVENT_SLATE)

#if defined(CONFIG_PDIC_PD30)
#define is_pd_wire_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_PDIC || \
	cable_type == SEC_BATTERY_CABLE_PDIC_APDO)

#define is_pd_apdo_wire_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_PDIC_APDO)
#else
#define is_pd_wire_type(cable_type) ( \
	cable_type == SEC_BATTERY_CABLE_PDIC)
#endif
#endif /* __SEC_CHARGING_COMMON_H */
