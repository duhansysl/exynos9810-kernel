/*
 * sec_battery.h
 * Samsung Mobile Battery Header
 *
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

#ifndef __SEC_BATTERY_H
#define __SEC_BATTERY_H __FILE__

#include "sec_charging_common.h"
#include <linux/of_gpio.h>
#include <linux/alarmtimer.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>

#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/manager/usb_typec_manager_notifier.h>
#else
#if defined(CONFIG_CCIC_NOTIFIER)
#include <linux/ccic/ccic_notifier.h>
#endif /* CONFIG_CCIC_NOTIFIER */
#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif
#endif

#if defined(CONFIG_BATTERY_NOTIFIER)
#include <linux/battery/battery_notifier.h>
#endif
#if defined(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif

#if defined(CONFIG_BATTERY_CISD)
#include "sec_cisd.h"
#endif
#if defined(CONFIG_DIRECT_CHARGING)
#include "sec_direct_charger.h"
#endif

#if defined(CONFIG_WIRELESS_AUTH)
#include "sec_battery_misc.h"
#endif

#include "sec_battery_vote.h"
#include "sec_adc.h"
#include <linux/sec_batt.h>

extern char *sec_cable_type[];

/* current event */
#define SEC_BAT_CURRENT_EVENT_NONE					0x000000
#define SEC_BAT_CURRENT_EVENT_AFC					0x000001
#define SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE		0x000002
#define SEC_BAT_CURRENT_EVENT_SKIP_HEATING_CONTROL	0x000004
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING		0x000008
#define SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING	0x000020
#if defined(CONFIG_ENABLE_100MA_CHARGING_BEFORE_USB_CONFIGURED)
#define SEC_BAT_CURRENT_EVENT_USB_100MA			0x000040
#else
#define SEC_BAT_CURRENT_EVENT_USB_100MA			0x000000
#endif
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_2ND			0x000080
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_3RD			0x000010
#define SEC_BAT_CURRENT_EVENT_SWELLING_MODE		(SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING | SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_2ND | SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING | SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_3RD)
#define SEC_BAT_CURRENT_EVENT_LOW_TEMP_MODE		(SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING | SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_2ND | SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_3RD)
#define SEC_BAT_CURRENT_EVENT_USB_SUPER			0x000100
#define SEC_BAT_CURRENT_EVENT_CHG_LIMIT			0x000200
#define SEC_BAT_CURRENT_EVENT_CALL				0x000400
#define SEC_BAT_CURRENT_EVENT_SLATE				0x000800
#define SEC_BAT_CURRENT_EVENT_VBAT_OVP			0x001000
#define SEC_BAT_CURRENT_EVENT_VSYS_OVP			0x002000
#define SEC_BAT_CURRENT_EVENT_WPC_VOUT_LOCK		0x004000
#define SEC_BAT_CURRENT_EVENT_AICL				0x008000
#define SEC_BAT_CURRENT_EVENT_HV_DISABLE		0x010000
#define SEC_BAT_CURRENT_EVENT_SELECT_PDO		0x020000
#define SEC_BAT_CURRENT_EVENT_FG_RESET			0x040000
#define SEC_BAT_CURRENT_EVENT_WDT_EXPIRED		0x080000
#define SEC_BAT_CURRENT_EVENT_SAFETY_TMR		0x100000
#define SEC_BAT_CURRENT_EVENT_ISDB				0x200000
#define SEC_BAT_CURRENT_EVENT_DC_ERR			0x400000
#define SEC_BAT_CURRENT_EVENT_TEMP_CTRL_TEST		0x1000000

/* misc_event */
#define BATT_MISC_EVENT_UNDEFINED_RANGE_TYPE	0x00000001
#define BATT_MISC_EVENT_WIRELESS_BACKPACK_TYPE	0x00000002
#define BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE		0x00000004
#define BATT_MISC_EVENT_BATT_RESET_SOC			0x00000008
#define BATT_MISC_EVENT_HICCUP_TYPE				0x00000020
#define BATT_MISC_EVENT_WIRELESS_DET_LEVEL		0x00000040
#define BATT_MISC_EVENT_WIRELESS_FOD			0x00000100
#define BATT_MISC_EVENT_WIRELESS_AUTH_START     0x00000200
#define BATT_MISC_EVENT_WIRELESS_AUTH_RECVED    0x00000400
#define BATT_MISC_EVENT_WIRELESS_AUTH_FAIL      0x00000800
#define BATT_MISC_EVENT_WIRELESS_AUTH_PASS      0x00001000
#define BATT_MISC_EVENT_TEMP_HICCUP_TYPE	0x00002000
#define BATT_MISC_EVENT_BATTERY_HEALTH			0x000F0000

#define BATTERY_HEALTH_SHIFT                16
enum misc_battery_health {
	BATTERY_HEALTH_UNKNOWN = 0,
	BATTERY_HEALTH_GOOD,
	BATTERY_HEALTH_NORMAL,
	BATTERY_HEALTH_AGED,
	BATTERY_HEALTH_MAX = BATTERY_HEALTH_AGED,

	/* For event */
	BATTERY_HEALTH_BAD = 0xF,
};

#if defined(CONFIG_SEC_FACTORY)             // SEC_FACTORY
#define STORE_MODE_CHARGING_MAX 80
#define STORE_MODE_CHARGING_MIN 70
#else                                       // !SEC_FACTORY, STORE MODE
#define STORE_MODE_CHARGING_MAX 70
#define STORE_MODE_CHARGING_MIN 60
#define STORE_MODE_CHARGING_MAX_VZW 35
#define STORE_MODE_CHARGING_MIN_VZW 30
#endif //(CONFIG_SEC_FACTORY)

#define ADC_CH_COUNT		10
#define ADC_SAMPLE_COUNT	10

#define DEFAULT_HEALTH_CHECK_COUNT	5
#define TEMP_HIGHLIMIT_DEFAULT	2000

#define SIOP_INPUT_LIMIT_CURRENT                1200
#define SIOP_CHARGING_LIMIT_CURRENT             1000
#define SIOP_WIRELESS_INPUT_LIMIT_CURRENT       530
#define SIOP_WIRELESS_CHARGING_LIMIT_CURRENT    780
#define SIOP_HV_WIRELESS_INPUT_LIMIT_CURRENT	700
#define SIOP_HV_WIRELESS_CHARGING_LIMIT_CURRENT	600
#define SIOP_STORE_HV_WIRELESS_CHARGING_LIMIT_CURRENT	450
#define SIOP_HV_INPUT_LIMIT_CURRENT				1200
#define SIOP_HV_CHARGING_LIMIT_CURRENT			1000
#define SIOP_HV_12V_INPUT_LIMIT_CURRENT			535
#define SIOP_HV_12V_CHARGING_LIMIT_CURRENT		1000
#if defined(CONFIG_DIRECT_CHARGING)
#define SIOP_APDO_INPUT_LIMIT_CURRENT				1000
#define SIOP_APDO_CHARGING_LIMIT_CURRENT			2000
#endif

#define FOREACH_VOTER(GENERATE) \
	GENERATE(VOTER_VBUS_CHANGE)   \
	GENERATE(VOTER_USB_100MA)	\
	GENERATE(VOTER_CHG_LIMIT)	\
	GENERATE(VOTER_AICL)	\
	GENERATE(VOTER_SELECT_PDO)	\
	GENERATE(VOTER_CABLE)	\
	GENERATE(VOTER_MIX_LIMIT)	\
	GENERATE(VOTER_WPC_TEMP)	\
	GENERATE(VOTER_PDIC_TEMP)	\
	GENERATE(VOTER_AFC_TEMP)	\
	GENERATE(VOTER_CHG_TEMP)	\
	GENERATE(VOTER_STORE_MODE)	\
	GENERATE(VOTER_SIOP)	\
	GENERATE(VOTER_WPC_CUR)	\
	GENERATE(VOTER_SWELLING)	\
	GENERATE(VOTER_OTG)		\
	GENERATE(VOTER_SLEEP_MODE)	\
	GENERATE(VOTER_USER)	\
	GENERATE(VOTER_STEP)	\
	GENERATE(VOTER_AGING_STEP)	\
	GENERATE(VOTER_VBUS_OVP)	\
	GENERATE(VOTER_FULL_CHARGE)	\
	GENERATE(VOTER_TEST_MODE)	\
	GENERATE(VOTER_TIME_EXPIRED)	\
	GENERATE(VOTER_WATER)	\
	GENERATE(VOTER_WC_TX)	\
	GENERATE(VOTER_SLATE)	\
	GENERATE(VOTER_SYSOVLO)	\
	GENERATE(VOTER_VBAT_OVP)	\
	GENERATE(VOTER_STEP_CHARGE)	\
	GENERATE(VOTER_TOPOFF_CHANGE)	\
	GENERATE(VOTER_HMT)	\
	GENERATE(VOTER_DC_ERR)	\
	GENERATE(VOTER_MAX)

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

enum VOTER_ENUM {
		FOREACH_VOTER(GENERATE_ENUM)
};
#define WIRELESS_OTG_INPUT_CURRENT 900

#define SEC_INPUT_VOLTAGE_0V	0
#define SEC_INPUT_VOLTAGE_5V	5
#define SEC_INPUT_VOLTAGE_9V	9
#define SEC_INPUT_VOLTAGE_10V	10
#define SEC_INPUT_VOLTAGE_12V	12
#define SEC_INPUT_VOLTAGE_NONE	100

#define HV_CHARGER_STATUS_STANDARD1	12000 /* mW */
#define HV_CHARGER_STATUS_STANDARD2	20000 /* mW */
#define HV_CHARGER_STATUS_STANDARD3 24500 /* mW */
#define HV_CHARGER_STATUS_STANDARD4 40000 /* mW */

enum {
	NORMAL_TA,
	AFC_9V_OR_15W,
	AFC_12V_OR_20W,
	SFC_25W,
	SFC_45W,
};

#if defined(CONFIG_CCIC_NOTIFIER)
struct sec_bat_pdic_info {
	unsigned int pdo_index;
#if defined(CONFIG_PDIC_PD30) && defined(CONFIG_DIRECT_CHARGING)
	bool apdo;
	unsigned int max_voltage;
	unsigned int min_voltage;
	unsigned int max_current;
#else
	unsigned int input_voltage;
	unsigned int input_current;
#endif
};

struct sec_bat_pdic_list {
	struct sec_bat_pdic_info pd_info[8]; /* 5V ~ 12V */
	unsigned int now_pd_index;
	unsigned int max_pd_count;
#if defined(CONFIG_PDIC_PD30)
	bool now_isApdo;
	unsigned int num_fpdo;
	unsigned int num_apdo;
#endif
};
#endif

enum {
	BAT_THERMAL_COLD = 0,
	BAT_THERMAL_COOL3,
	BAT_THERMAL_COOL2,
	BAT_THERMAL_COOL1,
	BAT_THERMAL_NORMAL,
	BAT_THERMAL_WARM,
	BAT_THERMAL_OVERHEAT,
	BAT_THERMAL_OVERHEATLIMIT,
};

enum {
	USB_THM_NORMAL = 0,
	USB_THM_OVERHEATLIMIT,
	USB_THM_GAP_OVER,
};

enum tx_switch_mode_state {
	TX_SWITCH_MODE_OFF = 0,
	TX_SWITCH_CHG_ONLY,
	TX_SWITCH_UNO_ONLY,
};

struct adc_sample_info {
	unsigned int cnt;
	int total_adc;
	int average_adc;
	int adc_arr[ADC_SAMPLE_COUNT];
	int index;
};

typedef struct sec_charging_current {
	unsigned int input_current_limit;
	unsigned int fast_charging_current;
#if defined(CONFIG_DUAL_BATTERY)
	unsigned int fast_main_charging_current;
	unsigned int fast_sub_charging_current;
#endif
} sec_charging_current_t;

/**
 * struct sec_bat_adc_table_data - adc to temperature table for sec battery
 * driver
 * @adc: adc value
 * @temperature: temperature(C) * 10
 */
typedef struct sec_bat_adc_table_data {
	int adc;
	int data;
} sec_bat_adc_table_data_t;

typedef struct sec_bat_adc_region {
	int min;
	int max;
} sec_bat_adc_region_t;

typedef struct sec_battery_platform_data {
	/* NO NEED TO BE CHANGED */
	/* callback functions */
	void (*monitor_additional_check)(void);
	bool (*bat_gpio_init)(void);
	bool (*fg_gpio_init)(void);
	bool (*check_jig_status)(void);
	int (*check_cable_callback)(void);
	bool (*cable_switch_check)(void);
	bool (*cable_switch_normal)(void);
	bool (*check_cable_result_callback)(int);
	bool (*check_battery_callback)(void);
	bool (*check_battery_result_callback)(void);
	int (*ovp_uvlo_callback)(void);
	bool (*ovp_uvlo_result_callback)(int);
	bool (*get_temperature_callback)(
			enum power_supply_property,
			union power_supply_propval*);

	/* ADC region by power supply type
	 * ADC region should be exclusive
	 */
	sec_bat_adc_region_t *cable_adc_value;
	/* charging current for type (0: not use) */
	sec_charging_current_t *charging_current;
	unsigned int *polling_time;
	char *chip_vendor;
	unsigned int temp_adc_type;
	/* NO NEED TO BE CHANGED */
	unsigned int pre_afc_input_current;
	unsigned int pre_wc_afc_input_current;
	unsigned int store_mode_afc_input_current;
	unsigned int store_mode_hv_wireless_input_current;
	unsigned int prepare_ta_delay;

	char *pmic_name;

	/* battery */
	char *vendor;
	int technology;
	int battery_type;
	void *battery_data;

	int bat_gpio_ta_nconnected;
	/* 1 : active high, 0 : active low */
	int bat_polarity_ta_nconnected;
	int bat_irq;
	int bat_irq_gpio; /* BATT_INT(BAT_ID detecting) */
	unsigned long bat_irq_attr;
	int jig_irq;
	unsigned long jig_irq_attr;
	sec_battery_cable_check_t cable_check_type;
	sec_battery_cable_source_t cable_source_type;

#if defined(CONFIG_DUAL_BATTERY)
	unsigned int swelling_main_low_temp_current;
	unsigned int swelling_sub_low_temp_current;
	unsigned int swelling_main_low_temp_current_2nd;
	unsigned int swelling_sub_low_temp_current_2nd;
	unsigned int swelling_main_high_temp_current;
	unsigned int swelling_sub_high_temp_current;
#endif
	unsigned int swelling_high_rechg_voltage;
	unsigned int swelling_low_rechg_voltage;

#if defined(CONFIG_CALC_TIME_TO_FULL)
	unsigned int ttf_hv_12v_charge_current;
	unsigned int ttf_hv_charge_current;
	unsigned int ttf_hv_12v_wireless_charge_current;
	unsigned int ttf_hv_wireless_charge_current;
	unsigned int ttf_wireless_charge_current;
	unsigned int ttf_dc25_charge_current;
	unsigned int ttf_dc45_charge_current;
#endif

#if defined(CONFIG_STEP_CHARGING)
	/* step charging */
	unsigned int *step_charging_condition;
	unsigned int *step_charging_condition_curr;
	unsigned int *step_charging_current;
	unsigned int *step_charging_float_voltage;
#if defined(CONFIG_DIRECT_CHARGING)
	unsigned int *dc_step_chg_cond_vol;
	unsigned int *dc_step_chg_cond_soc;
	unsigned int *dc_step_chg_cond_iin;
	int dc_step_chg_iin_check_cnt;

	unsigned int *dc_step_chg_val_iout;
	unsigned int *dc_step_chg_val_vfloat;
#endif
#endif

	/* Monitor setting */
	int polling_type;
	/* for initial check */
	unsigned int monitor_initial_count;

	/* Battery check */
	sec_battery_check_t battery_check_type;
	/* how many times do we need to check battery */
	unsigned int check_count;
	/* ADC */
	/* battery check ADC maximum value */
	unsigned int check_adc_max;
	/* battery check ADC minimum value */
	unsigned int check_adc_min;

	/* OVP/UVLO check */
	int ovp_uvlo_check_type;

	int thermal_source;
	int usb_thermal_source; /* To confirm the usb temperature */
	int chg_thermal_source; /* To confirm the charger temperature */
	int wpc_thermal_source; /* To confirm the wpc temperature */
	int slave_thermal_source; /* To confirm the slave charger temperature */
	int sub_bat_thermal_source; /* To confirm the sub battery temperature */
#if defined(CONFIG_DIRECT_CHARGING)
	int dchg_thermal_source; /* To confirm the charger temperature */
#endif
	int blkt_thermal_source; /* To confirm the blanket temperature */

	/*
	 * inbat_adc_table
	 * in-battery voltage check for table models:
	 * To read real battery voltage with Jig cable attached,
	 * dedicated hw pin & conversion table of adc-voltage are required
	 */
	sec_bat_adc_table_data_t *temp_adc_table;
	sec_bat_adc_table_data_t *temp_amb_adc_table;
	sec_bat_adc_table_data_t *usb_temp_adc_table;
	sec_bat_adc_table_data_t *chg_temp_adc_table;
	sec_bat_adc_table_data_t *wpc_temp_adc_table;
	sec_bat_adc_table_data_t *slave_chg_temp_adc_table;
	sec_bat_adc_table_data_t *inbat_adc_table;
	sec_bat_adc_table_data_t *sub_bat_temp_adc_table;
#if defined(CONFIG_DIRECT_CHARGING)
	sec_bat_adc_table_data_t *dchg_temp_adc_table;
#endif
	sec_bat_adc_table_data_t *blkt_temp_adc_table;

	unsigned int temp_adc_table_size;
	unsigned int temp_amb_adc_table_size;
	unsigned int usb_temp_adc_table_size;
	unsigned int chg_temp_adc_table_size;
	unsigned int wpc_temp_adc_table_size;
	unsigned int slave_chg_temp_adc_table_size;
	unsigned int inbat_adc_table_size;
	unsigned int sub_bat_temp_adc_table_size;
#if defined(CONFIG_DIRECT_CHARGING)
	unsigned int dchg_temp_adc_table_size;
#endif
	unsigned int blkt_temp_adc_table_size;

	int temp_check_type;
	unsigned int temp_check_count;
	int usb_temp_check_type;
	int usb_temp_check_type_backup;
	int chg_temp_check_type;
	int wpc_temp_check_type;
	int slave_chg_temp_check_type;
	unsigned int sub_bat_temp_check_type;
#if defined(CONFIG_DIRECT_CHARGING)
	int dchg_temp_check_type;
#endif
	int blkt_temp_check_type;

	unsigned int inbat_voltage;

	/*
	 * limit can be ADC value or Temperature
	 * depending on temp_check_type
	 * temperature should be temp x 10 (0.1 degree)
	 */
	int wireless_cold_cool3_thresh;
	int wireless_cool3_cool2_thresh;
	int wireless_cool2_cool1_thresh;
	int wireless_cool1_normal_thresh;
	int wireless_normal_warm_thresh;
	int wireless_warm_overheat_thresh;

	int wire_cold_cool3_thresh;
	int wire_cool3_cool2_thresh;
	int wire_cool2_cool1_thresh;
	int wire_cool1_normal_thresh;
	int wire_normal_warm_thresh;
	int wire_warm_overheat_thresh;

	int wire_warm_current;
	int wire_cool1_current;
	int wire_cool2_current;
	int wire_cool3_current;
	int wireless_warm_current;
	int wireless_cool1_current;
	int wireless_cool2_current;
	int wireless_cool3_current;
	int high_temp_topoff;
	int low_temp_topoff;
	int high_temp_float;
	int low_temp_float;

	int tx_high_threshold;
	int tx_high_recovery;
	int tx_low_threshold;
	int tx_low_recovery;
	int chg_12v_high_temp;
	int chg_high_temp;
	int chg_high_temp_recovery;
	unsigned int chg_charging_limit_current;
#if defined(CONFIG_DUAL_BATTERY)
	unsigned int chg_main_charging_limit_current;
	unsigned int chg_sub_charging_limit_current;
#endif	
	unsigned int chg_input_limit_current;
#if defined(CONFIG_DIRECT_CHARGING)
	unsigned int dchg_charging_limit_current;
	unsigned int dchg_input_limit_current;
#endif
	unsigned int wpc_temp_control_source;
	unsigned int wpc_temp_lcd_on_control_source;
	int wpc_high_temp;
	int wpc_high_temp_recovery;
	unsigned int wpc_input_limit_current;	
	unsigned int wpc_charging_limit_current;
	int wpc_lcd_on_high_temp;
	int wpc_lcd_on_high_temp_rec;
	unsigned int wpc_lcd_on_input_limit_current;
	unsigned int sleep_mode_limit_current;
	unsigned int wc_full_input_limit_current;
	unsigned int max_charging_current;
	unsigned int max_charging_charge_power;
	int mix_high_temp;
	int mix_high_chg_temp;
	int mix_high_temp_recovery;
	unsigned int charging_limit_by_tx_check; /* check limited charging current during wireless power sharing with cable charging */
	unsigned int charging_limit_current_by_tx;
	unsigned int wpc_input_limit_by_tx_check; /* check limited wpc input current with tx device */
	unsigned int wpc_input_limit_current_by_tx;

	/* If these is NOT full check type or NONE full check type,
	 * it is skipped
	 */
	/* 1st full check */
	int full_check_type;
	/* 2nd full check */
	int full_check_type_2nd;
	unsigned int full_check_count;
	int chg_gpio_full_check;
	/* 1 : active high, 0 : active low */
	int chg_polarity_full_check;
	sec_battery_full_condition_t full_condition_type;
	unsigned int full_condition_soc;
	unsigned int full_condition_vcell;
	unsigned int full_condition_avgvcell;
	unsigned int full_condition_ocv;

	unsigned int recharge_check_count;
	sec_battery_recharge_condition_t recharge_condition_type;
	unsigned int recharge_condition_soc;
	unsigned int recharge_condition_avgvcell;
	unsigned int recharge_condition_vcell;

	/* for absolute timer (second) */
	unsigned long charging_total_time;
	/* for recharging timer (second) */
	unsigned long recharging_total_time;
	/* reset charging for abnormal malfunction (0: not use) */
	unsigned long charging_reset_time;
	unsigned int hv_charging_total_time;
	unsigned int normal_charging_total_time;
	unsigned int usb_charging_total_time;

	/* fuel gauge */
	char *fuelgauge_name;
	int fg_irq;
	unsigned long fg_irq_attr;
	/* fuel alert SOC (-1: not use) */
	int fuel_alert_soc;
	/* fuel alert can be repeated */
	bool repeated_fuelalert;
	sec_fuelgauge_capacity_type_t capacity_calculation_type;
	/* soc should be soc x 10 (0.1% degree)
	 * only for scaling
	 */
	int capacity_max;
	int capacity_max_margin;
	int capacity_min;

	unsigned int store_mode_charging_max;
	unsigned int store_mode_charging_min;
	/* charger */
	char *charger_name;
	char *fgsrc_switch_name;
	bool support_fgsrc_change;

	/* wireless charger */
	char *wireless_charger_name;
	int wireless_cc_cv;
	int wpc_en;

	int chg_gpio_en;
	int chg_irq;
	unsigned long chg_irq_attr;
	/* float voltage (mV) */
	unsigned int chg_float_voltage;
	unsigned int chg_float_voltage_conv;

#if defined(CONFIG_DUAL_BATTERY)
	/* current limiter */
	char *dual_battery_name;
	char *main_limiter_name;
	char *sub_limiter_name;
	bool support_dual_battery;
	int main_bat_enb_gpio;
	int sub_bat_enb_gpio;	
#endif

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	int num_age_step;
	int age_step;
	int age_data_length;
	sec_age_data_t* age_data;
#endif
	battery_health_condition* health_condition;

	int siop_input_limit_current;
	int siop_charging_limit_current;
#if defined(CONFIG_DUAL_BATTERY)
	int siop_main_charging_limit_current;
	int siop_sub_charging_limit_current;
#endif	
	int siop_hv_input_limit_current;
	int siop_hv_input_limit_current_2nd;
	int siop_hv_charging_limit_current;
#if defined(CONFIG_DUAL_BATTERY)
	int siop_main_hv_charging_limit_current;
	int siop_sub_hv_charging_limit_current;
#endif
	int siop_hv_12v_input_limit_current;
	int siop_hv_12v_charging_limit_current;
#if defined(CONFIG_DIRECT_CHARGING)
	int siop_apdo_input_limit_current;
	int siop_apdo_charging_limit_current;
#endif

	int siop_wireless_input_limit_current;
	int siop_wireless_charging_limit_current;
	int siop_hv_wireless_input_limit_current;
	int siop_hv_wireless_charging_limit_current;
	int wireless_otg_input_current;
	int wc_hero_stand_cc_cv;
	int wc_hero_stand_cv_current;
	int wc_hero_stand_hv_cv_current;

	int default_input_current;
	int default_charging_current;
#if defined(CONFIG_DUAL_BATTERY)	
	int default_main_charging_current;
	int default_sub_charging_current;
#endif
	int default_usb_input_current;
	int default_usb_charging_current;
	int max_input_voltage;
	int max_input_current;
	int pre_afc_work_delay;
	int pre_wc_afc_work_delay;

	unsigned int rp_current_rp1;
	unsigned int rp_current_rp2;
	unsigned int rp_current_rp3;
	unsigned int rp_current_rdu_rp3;
	unsigned int rp_current_abnormal_rp3;

	sec_charger_functions_t chg_functions_setting;

	bool fake_capacity;

	/* tx power sharging */
	unsigned int tx_stop_capacity;

	unsigned int battery_full_capacity;
#if defined(CONFIG_BATTERY_CISD)	
	unsigned int cisd_cap_high_thr;
	unsigned int cisd_cap_low_thr;
	unsigned int cisd_cap_limit;
	unsigned int max_voltage_thr;
	unsigned int cisd_alg_index;
	unsigned int *ignore_cisd_index;
	unsigned int *ignore_cisd_index_d;
#endif

#if defined(CONFIG_DUAL_BATTERY)
	/* main + sub value should be over 110% */
	unsigned int main_charging_rate;
	unsigned int sub_charging_rate;
	unsigned int force_recharge_margin;
	unsigned int max_main_charging_current;
	unsigned int min_main_charging_current;
	unsigned int max_sub_charging_current;
	unsigned int min_sub_charging_current;
#endif

	/* ADC setting */
	unsigned int adc_check_count;

	unsigned int full_check_current_1st;
	unsigned int full_check_current_2nd;

	unsigned int pd_charging_charge_power;
	unsigned int nv_charge_power;

	unsigned int expired_time;
	unsigned int recharging_expired_time;
	int standard_curr;

	unsigned int tx_minduty_5V;
	unsigned int tx_minduty_default;

	unsigned int tx_uno_iout;
	unsigned int tx_mfc_iout_gear;
	unsigned int tx_mfc_iout_phone;
	unsigned int tx_mfc_iout_phone_5v;
	unsigned int tx_mfc_iout_lcd_on;

	/* ADC type for each channel */
	unsigned int adc_type[];
} sec_battery_platform_data_t;

struct sec_battery_info {
	struct device *dev;
	sec_battery_platform_data_t *pdata;

	/* power supply used in Android */
	struct power_supply *psy_bat;
	struct power_supply *psy_usb;
	struct power_supply *psy_ac;
	struct power_supply *psy_wireless;
	struct power_supply *psy_pogo;
	unsigned int irq;

	int pd_usb_attached;
#if defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	struct notifier_block usb_typec_nb;
#else
#if defined(CONFIG_CCIC_NOTIFIER)
	struct notifier_block pdic_nb;
#endif
#if defined(CONFIG_MUIC_NOTIFIER)
	struct notifier_block batt_nb;
#endif
#endif

#if defined(CONFIG_CCIC_NOTIFIER)
	bool pdic_attach;
	bool pdic_ps_rdy;
	bool hv_pdo;
	struct pdic_notifier_struct pdic_info;
	struct sec_bat_pdic_list pd_list;
#endif
#if defined(CONFIG_VBUS_NOTIFIER)
	struct notifier_block vbus_nb;
	int muic_vbus_status;
#endif

	bool is_sysovlo;
	bool is_vbatovlo;
	bool is_abnormal_temp;

	bool safety_timer_set;
	bool lcd_status;
	bool skip_swelling;

	int status;
	int health;
	bool present;
	unsigned int charger_mode;

	int voltage_now;		/* cell voltage (mV) */
	int voltage_avg;		/* average voltage (mV) */
	int voltage_ocv;		/* open circuit voltage (mV) */
	int current_now;		/* current (mA) */
	int inbat_adc;			/* inbat adc */
	int current_avg;		/* average current (mA) */
	int current_max;		/* input current limit (mA) */
	int current_sys;		/* system current (mA) */
	int current_sys_avg;		/* average system current (mA) */
	int charge_counter;		/* remaining capacity (uAh) */
	int current_adc;

#if defined(CONFIG_DUAL_BATTERY)
	int voltage_avg_main;		/* average voltage (mV) */
	int voltage_avg_sub;		/* average voltage (mV) */
	int current_now_main;		/* current (mA) */
	int current_now_sub;		/* current (mA) */
#endif

	unsigned int capacity;			/* SOC (%) */
	unsigned int input_voltage;		/* CHGIN/WCIN input voltage (V) */
	unsigned int charge_power;		/* charge power (mW) */
	unsigned int max_charge_power;		/* max charge power (mW) */
	unsigned int pd_max_charge_power;		/* max charge power for pd (mW) */

	struct mutex adclock;
	struct adc_sample_info	adc_sample[ADC_CH_COUNT];

	/* keep awake until monitor is done */
	struct wake_lock monitor_wake_lock;
	struct workqueue_struct *monitor_wqueue;
	struct delayed_work monitor_work;
#ifdef CONFIG_SAMSUNG_BATTERY_FACTORY
	struct wake_lock lpm_wake_lock;
#endif
	unsigned int polling_count;
	unsigned int polling_time;
	bool polling_in_sleep;
	bool polling_short;

	struct delayed_work polling_work;
	struct alarm polling_alarm;
	ktime_t last_poll_time;

#if defined(CONFIG_BATTERY_CISD)
	struct cisd cisd;
	bool skip_cisd;
	bool usb_overheat_check;
	int prev_volt;
	int prev_temp;
	int prev_jig_on;
	int enable_update_data;
	int prev_chg_on;
#endif

#if defined(CONFIG_WIRELESS_AUTH)
	sec_bat_misc_dev_t *misc_dev;
#endif

	/* battery check */
	unsigned int check_count;
	/* ADC check */
	unsigned int check_adc_count;
	unsigned int check_adc_value;

	/* health change check*/
	bool health_change;
	/* ovp-uvlo health check */
	int health_check_count;

	/* time check */
	unsigned long charging_start_time;
	unsigned long charging_passed_time;
	unsigned long charging_next_time;
	unsigned long charging_fullcharged_time;

	unsigned long wc_heating_start_time;
	unsigned long wc_heating_passed_time;
	unsigned int wc_heat_limit;

	/* chg temperature check */
	unsigned int chg_limit;
	unsigned int chg_limit_recovery_cable;
	unsigned int vbus_chg_by_siop;
	unsigned int vbus_chg_by_full;
	unsigned int mix_limit;
	unsigned int vbus_limit;

	/* temperature check */
	int temperature;	/* battery temperature */
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	int temperature_test_battery;
	int temperature_test_usb;
	int temperature_test_wpc;
	int temperature_test_chg;
	int temperature_test_dchg;
	int temperature_test_blkt;
	bool test_max_current;
	bool test_charge_current;
#if defined(CONFIG_STEP_CHARGING)
	int test_step_condition;
#endif
#if defined(CONFIG_DUAL_BATTERY)
	int temperature_test_sub;
#endif
#endif
	int temper_amb;		/* target temperature */
	int usb_temp;
	int chg_temp;		/* charger temperature */
	int wpc_temp;
	int coil_temp;
	int slave_chg_temp;
	int sub_bat_temp;
	int usb_thm_status;
#if defined(CONFIG_PREVENT_USB_CONN_OVERHEAT)
	int usb_protection_temp;
	int temp_gap_bat_usb;
#endif
#if defined(CONFIG_DIRECT_CHARGING)
	int dchg_temp;
#endif
	int blkt_temp;		/* blanket temperature(instead of batt temp in mix_temp func for tablet model) */

	int temp_adc;
	int temp_ambient_adc;
	int usb_temp_adc;
	int chg_temp_adc;
	int wpc_temp_adc;
	int coil_temp_adc;
	int slave_chg_temp_adc;
	int sub_bat_temp_adc;	
#if defined(CONFIG_DIRECT_CHARGING)
	int dchg_temp_adc;
#endif
	int blkt_temp_adc;

	int overheatlimit_threshold_backup;
	int overheatlimit_threshold;
	int overheatlimit_recovery;
	int cold_cool3_thresh;
	int cool3_cool2_thresh;
	int cool2_cool1_thresh;
	int cool1_normal_thresh;
	int normal_warm_thresh;
	int warm_overheat_thresh;
	int thermal_zone;
	bool swelling_mode;
	int bat_thm_count;

	/* charging */
	unsigned int charging_mode;
	bool is_recharging;
	int wdt_kick_disable;

	bool is_jig_on;
	int cable_type;
	int muic_cable_type;
	int extended_cable_type;

	struct wake_lock cable_wake_lock;
	struct delayed_work cable_work;
	struct wake_lock vbus_wake_lock;
	struct delayed_work siop_work;
	struct wake_lock siop_wake_lock;
	struct wake_lock afc_wake_lock;
	struct delayed_work afc_work;
#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
	struct delayed_work update_work;
	struct delayed_work fw_init_work;
#endif
	struct delayed_work siop_level_work;
	struct wake_lock siop_level_wake_lock;
	struct delayed_work wc_headroom_work;
	struct wake_lock wc_headroom_wake_lock;
	struct wake_lock wpc_tx_wake_lock;
	struct delayed_work wpc_tx_work;
#if defined(CONFIG_UPDATE_BATTERY_DATA)
	struct delayed_work batt_data_work;
	struct wake_lock batt_data_wake_lock;
	char *data_path;
#endif
#ifdef CONFIG_OF
	struct delayed_work parse_mode_dt_work;
	struct wake_lock parse_mode_dt_wake_lock;
#endif
	struct delayed_work init_chg_work;
	struct delayed_work otg_work;

	char batt_type[48];
	unsigned int full_check_cnt;
	unsigned int recharge_check_cnt;

	struct mutex iolock;
	int input_current;
	int charging_current;
#if defined(CONFIG_DUAL_BATTERY)
	unsigned int main_charging_current;
	unsigned int sub_charging_current;
#endif	
	int topoff_current;
	int wpc_vout_level;
	unsigned int current_event;

	/* wireless charging enable */
	struct mutex wclock;
	int wc_enable;
	int wc_enable_cnt;
	int wc_enable_cnt_value;
	int led_cover;
	int wc_status;
	bool wc_cv_mode;
	bool wc_pack_max_curr;
	bool wc_rx_phm_mode;
	bool wc_need_ldo_on;

	int wire_status;

	/* wireless tx */
	bool wc_tx_enable;
	bool wc_rx_connected;
	bool wc_tx_chg_limit;
	bool afc_disable;
	bool buck_cntl_by_tx;
	bool tx_switch_mode_change;
	int wc_tx_vout;
	bool uno_en;
	unsigned int wc_rx_type;
	unsigned int tx_minduty;
	unsigned int tx_switch_mode;
	unsigned int tx_switch_start_soc;

	unsigned int tx_mfc_iout;
	unsigned int tx_uno_iout;

	int pogo_status;

	/* test mode */
	int test_mode;
	bool factory_mode;
	bool store_mode;

	/* MTBF test for CMCC */
	bool is_hc_usb;

	int siop_level;
	int siop_prev_event;
	int stability_test;
	int eng_not_full_status;

	bool skip_chg_temp_check;
	bool skip_wpc_temp_check;
	bool wpc_temp_mode;
	bool charging_block;
#if defined(CONFIG_AFC_CHARGER_MODE)
	char *hv_chg_name;
#endif
#if defined(CONFIG_CALC_TIME_TO_FULL)
	int timetofull;
	struct delayed_work timetofull_work;
#endif
#if defined(CONFIG_WIRELESS_TX_MODE)
	int tx_avg_curr;
	int tx_time_cnt;
	int tx_total_power;
	int tx_total_power_cisd;
	bool tx_clear;
	bool tx_clear_cisd;
	struct delayed_work wpc_txpower_calc_work;
#endif
	struct delayed_work slowcharging_work;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	int batt_cycle;
#endif
	int batt_asoc;
#if defined(CONFIG_STEP_CHARGING)
	unsigned int step_charging_type;
	unsigned int step_charging_charge_power;
	int step_charging_status;
	int step_charging_step;
#if defined(CONFIG_DIRECT_CHARGING)
	int dc_step_chg_step;
	unsigned int dc_step_chg_type;
	unsigned int dc_step_chg_charge_power;

	bool dc_float_voltage_set;
	unsigned int dc_step_chg_iin_cnt;
#endif
#endif
	struct mutex misclock;
	struct mutex txeventlock;
	unsigned int misc_event;
	unsigned int tx_event;
	unsigned int ext_event;
	unsigned int prev_misc_event;
	unsigned int tx_retry_case;
	unsigned int tx_misalign_cnt;
	struct delayed_work ext_event_work;
	struct delayed_work misc_event_work;
	struct wake_lock ext_event_wake_lock;
	struct wake_lock misc_event_wake_lock;
	struct wake_lock tx_event_wake_lock;
	struct mutex batt_handlelock;
	struct mutex current_eventlock;
	struct mutex typec_notylock;
	struct mutex voutlock;
	unsigned long tx_misalign_start_time;
	unsigned long tx_misalign_passed_time;

	unsigned int hiccup_status;

	bool stop_timer;
	unsigned long prev_safety_time;
	unsigned long expired_time;
	unsigned long cal_safety_time;
	int fg_reset;

	struct sec_vote * fcc_vote;
	struct sec_vote * input_vote;
	struct sec_vote * fv_vote;
	struct sec_vote * chgen_vote;
	struct sec_vote * topoff_vote;
};

/* event check */
#define EVENT_NONE				(0)
#define EVENT_2G_CALL			(0x1 << 0)
#define EVENT_3G_CALL			(0x1 << 1)
#define EVENT_MUSIC				(0x1 << 2)
#define EVENT_VIDEO				(0x1 << 3)
#define EVENT_BROWSER			(0x1 << 4)
#define EVENT_HOTSPOT			(0x1 << 5)
#define EVENT_CAMERA			(0x1 << 6)
#define EVENT_CAMCORDER			(0x1 << 7)
#define EVENT_DATA_CALL			(0x1 << 8)
#define EVENT_WIFI				(0x1 << 9)
#define EVENT_WIBRO				(0x1 << 10)
#define EVENT_LTE				(0x1 << 11)
#define EVENT_LCD			(0x1 << 12)
#define EVENT_GPS			(0x1 << 13)

enum {
	EXT_DEV_NONE = 0,
	EXT_DEV_GAMEPAD_CHG,
	EXT_DEV_GAMEPAD_OTG,
};

extern bool sleep_mode;
//extern unsigned int lpcharge;
extern bool mfc_fw_update;
//extern int fg_reset;

extern void select_pdo(int num);
#if defined(CONFIG_PDIC_PD30)
extern int sec_pd_select_pps(int num, int ppsVol, int ppsCur);
extern int sec_pd_get_apdo_max_power(unsigned int *pdo_pos, unsigned int *taMaxVol, unsigned int *taMaxCur, unsigned int *taMaxPwr);
#endif

extern int adc_read(struct sec_battery_info *battery, int channel);
extern void adc_init(struct platform_device *pdev, struct sec_battery_info *battery);
extern void adc_exit(struct sec_battery_info *battery);
extern void sec_cable_init(struct platform_device *pdev, struct sec_battery_info *battery);
extern int sec_bat_get_adc_data(struct sec_battery_info *battery, int adc_ch, int count);
extern int sec_bat_get_charger_type_adc(struct sec_battery_info *battery);
extern bool sec_bat_get_value_by_adc(struct sec_battery_info *battery, enum sec_battery_adc_channel channel, union power_supply_propval *value, int check_type);
extern int sec_bat_get_adc_value(struct sec_battery_info *battery, int channel);
extern bool sec_bat_check_vf_adc(struct sec_battery_info *battery);
#if defined(CONFIG_DIRECT_CHARGING)
extern int sec_bat_get_direct_chg_temp_adc(struct sec_battery_info *battery, int adc_data, int count);
#endif
extern void sec_bat_set_misc_event(struct sec_battery_info *battery, unsigned int misc_event_val, unsigned int misc_event_mask);
extern void sec_bat_set_tx_event(struct sec_battery_info *battery, unsigned int tx_event_val, unsigned int tx_event_mask);
extern void sec_bat_set_current_event(struct sec_battery_info *battery, unsigned int current_event_val, unsigned int current_event_mask);
extern void sec_bat_set_temp_control_test(struct sec_battery_info *battery, bool temp_enable);
extern void sec_bat_get_battery_info(struct sec_battery_info *battery);
extern int sec_bat_set_charging_current(struct sec_battery_info *battery);
extern void sec_bat_aging_check(struct sec_battery_info *battery);
extern void sec_wireless_set_tx_enable(struct sec_battery_info *battery, bool wc_tx_enable);

extern void sec_bat_set_threshold(struct sec_battery_info *battery);
extern void sec_bat_thermal_check(struct sec_battery_info *battery);
extern void sec_bat_set_charging_status(struct sec_battery_info *battery, int status);
extern bool sec_bat_check_full(struct sec_battery_info *battery, int full_check_type);
extern void sec_bat_check_wpc_temp(struct sec_battery_info *battery);
extern void sec_bat_check_mix_temp(struct sec_battery_info *battery);
extern void sec_bat_check_afc_temp(struct sec_battery_info *battery);
extern void sec_bat_check_pdic_temp(struct sec_battery_info *battery);
extern void sec_bat_check_direct_chg_temp(struct sec_battery_info *battery);
extern void sec_bat_check_tx_temperature(struct sec_battery_info *battery);


#if defined(CONFIG_WIRELESS_FIRMWARE_UPDATE)
extern void sec_bat_fw_update_work(struct sec_battery_info *battery, int mode);
extern bool sec_bat_check_boost_mfc_condition(struct sec_battery_info *battery);
#endif

#if defined(CONFIG_STEP_CHARGING)
extern void sec_bat_reset_step_charging(struct sec_battery_info *battery);
extern void sec_step_charging_init(struct sec_battery_info *battery, struct device *dev);
extern bool sec_bat_check_step_charging(struct sec_battery_info *battery);
#if defined(CONFIG_DIRECT_CHARGING)
extern bool sec_bat_check_dc_step_charging(struct sec_battery_info *battery);
#endif
#if defined(CONFIG_BATTERY_AGE_FORECAST)
void sec_bat_set_aging_info_step_charging(struct sec_battery_info *battery);
#endif
#endif

#if defined(CONFIG_DIRECT_CHARGING)
extern void sec_direct_chg_init(struct sec_battery_info *battery, struct device *dev);
#endif

#if defined(CONFIG_UPDATE_BATTERY_DATA)
extern int sec_battery_update_data(const char* file_path);
#endif
#if defined(CONFIG_BATTERY_CISD)
extern bool sec_bat_cisd_check(struct sec_battery_info *battery);
extern void sec_battery_cisd_init(struct sec_battery_info *battery);
extern void set_cisd_pad_data(struct sec_battery_info *battery, const char* buf);
#endif

#if defined(CONFIG_WIRELESS_AUTH)
extern int sec_bat_misc_init(struct sec_battery_info *battery);
#endif

int sec_bat_parse_dt(struct device *dev, struct sec_battery_info *battery);
void sec_bat_parse_mode_dt(struct sec_battery_info *battery);
void sec_bat_parse_mode_dt_work(struct work_struct *work);
#if defined(CONFIG_BATTERY_AGE_FORECAST)
void sec_bat_check_battery_health(struct sec_battery_info *battery);
#endif

#endif /* __SEC_BATTERY_H */
