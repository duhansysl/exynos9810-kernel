/*
 * s2mu107_fuelgauge.h - Header of S2MU107 Fuel Gauge
 *
 * Copyright (C) 2019 Samsung Electronics, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __S2MU107_FUELGAUGE_H
#define __S2MU107_FUELGAUGE_H __FILE__

#if defined(ANDROID_ALARM_ACTIVATED)
#include <linux/android_alarm.h>
#endif

#include <linux/wakelock.h>
#include "../sec_charging_common.h"

#define MASK(width, shift)	(((0x1 << (width)) - 1) << shift)

/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */

#define S2MU107_REG_STATUS		0x00
#define S2MU107_REG_IRQ			0x02
#define S2MU107_REG_IRQ_M		0x03
#define S2MU107_REG_RVBAT		0x04
#define S2MU107_REG_RCUR_CC		0x06
#define S2MU107_REG_RSOC		0x08
#define S2MU107_REG_MONOUT		0x0A
#define S2MU107_REG_MONOUT_SEL		0x0C
#define S2MU107_REG_RBATCAP_OCV		0x0E
#define S2MU107_REG_RBATCAP		0x10
#define S2MU107_REG_RCAPCC		0x3E
#define S2MU107_REG_RSOC_R_SAVE		0x2A	//	Register for SOC saving

#define S2MU107_REG_RZADJ		0x12
#define S2MU107_REG_RBATZ0		0x16
#define S2MU107_REG_RBATZ1		0x18
#define S2MU107_REG_ALERT_LVL		0x1A
#define S2MU107_REG_START		0x1E
#define S2MU107_REG_FG_ID		0x48

#define S2MU107_REG_VM			0x67

#define S2MU107_REG_RSOC_R		0x80
#define S2MU107_REG_RDESIGN_CAP		0x86
#define S2MU107_REG_RSOC_R_I2C		0x8E

#define S2MU107_REG_RBATCAP_OCV_NEW		0x88
#define S2MU107_REG_RBATCAP_OCV_NEW_IN	0x90

#define IF_EN_MASK		MASK(2,4)

/* For MONOUT */
#define S2MU107_MONOUT_SEL_AVGVBAT	0x16
#define S2MU107_MONOUT_SEL_AVGCURR	0x17
#define S2MU107_MONOUT_SEL_AVGTEMP	0x18
#define S2MU107_MONOUT_SEL_CYCLE	0x27

/* For temperature compensation */
#define TEMP_COMPEN_INC_OK_EN	(1 << 7)
#define S2MU107_REG_RCOMPI		0x6E
#define S2MU107_REG_RA0		0x71
#define S2MU107_REG_RB0		0x70
#define S2MU107_REG_RA1		0x73
#define S2MU107_REG_RB1		0x72

/* For battery capacity learning */
#define BATCAP_LEARN_FAST_LRN_EN	(1 << 5)
#define BATCAP_LEARN_AUTO_LRN_EN	(1 << 7)
#define BATCAP_LEARN_WIDE_LRN_EN	(1 << 6)
#define BATCAP_LEARN_LOW_EN		(1 << 5)

#define BATCAP_LEARN_NO4LEARN_MASK	MASK(4,4)

#define BATCAP_LEARN_C1_NUM_MASK	MASK(2,0)
#define BATCAP_LEARN_C2_NUM_MASK	MASK(2,2)
#define BATCAP_LEARN_C1_CURR_MASK	MASK(4,4)

/* For init regs */
#define ADC_AVCC_EN_SHIFT		4
#define ADC_AVCC_EN_MASK		MASK(2,ADC_AVCC_EN_SHIFT)

/* For Direct charge setting */
#define T_PEROID_SHIFT		6
#define T_PERIOD_MASK		MASK(2,T_PEROID_SHIFT)
enum {
	T_PERIOD_500MS = 0,
	T_PERIOD_250MS,
	T_PERIOD_125MS,
	T_PERIOD_63MS,
};

enum {
	CURRENT_MODE = 0,
	LOW_SOC_VOLTAGE_MODE,
	HIGH_SOC_VOLTAGE_MODE,
	END_MODE,
};

static char* mode_to_str[] = {
	"CC_MODE",
	"VOLTAGE_MODE",	// not used
	"VOLTAGE_MODE",
	"END",
};

struct fg_info {
	/* battery info */
	int soc;
#if !defined(CONFIG_BATTERY_AGE_FORECAST)
	/* copy from platform data /
	 * DTS or update by shell script */
	int battery_table3[88]; // evt2
	int battery_table4[22]; // evt2
	int soc_arr_val[22];
	int ocv_arr_val[22];

	int batcap[4];
	int accum[2];
#endif
};
#if defined(CONFIG_BATTERY_AGE_FORECAST)
struct fg_age_data_info {
	int battery_table3[88]; // evt2
	int battery_table4[22]; // evt2
	int batcap[4];
	int accum[2];
	int soc_arr_val[22];
	int ocv_arr_val[22];
	int volt_mode_tunning;
};

#define	fg_age_data_info_t \
	struct fg_age_data_info
#endif

struct s2mu107_fuelgauge_platform_data {
	int fuel_alert_soc;
	int fg_irq;
	int fuel_alert_vol;

	unsigned int capacity_full;

	char *fuelgauge_name;
	char *charger_name;

	bool repeated_fuelalert;

	struct sec_charging_current *charging_current;

	int capacity_max;
	int capacity_max_margin;
	int capacity_min;
	int capacity_calculation_type;
	int fullsocthr;


	/* For temperature compensation */
	int inc_ok_en;
	int comp_i;
	int a0;
	int b0;
	int a1;
	int b1;

	/* For battery capacity learning */
	int fast_lrn_en;
	int no4learn;
	int auto_lrn_en;
	int wide_lrn_en;
	int low_en;
	int c1_num;
	int c2_num;
	int c1_curr;
};

struct cv_slope {
	int fg_current;
	int soc;
	int time;
};

struct s2mu107_fuelgauge_data {
	struct device           *dev;
	struct i2c_client       *i2c;
	struct i2c_client       *pmic;
	struct mutex            fuelgauge_mutex;
	struct s2mu107_fuelgauge_platform_data *pdata;
	struct power_supply	*psy_fg;
	/* struct delayed_work isr_work; */

	int cable_type;
	bool is_charging; /* charging is enabled */
#if defined(CONFIG_CHARGER_S2MU107_DIRECT)
	bool is_dc_charging;
#endif
	int soc_m;
	int soc_r;
	int avg_curr;
	int avg_vbat;
	int curr;
	int vbat;

	int topoff_current;

	bool vm_status; /* Now voltage mode or not */
	int comp_socr; /* 1% unit */
	int capcc;
	int batcap_ocv;
	int cycle;
	int soh;
	int socr;
	int rm;
	int fcc;

	int mode;
	u8 revision;

	struct fg_info info;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	fg_age_data_info_t *age_data_info;
	int fg_num_age_step;
	int fg_age_step;
	int age_reset_status;
	struct mutex fg_reset_lock;
	int change_step;
#endif
	bool is_fuel_alerted;
	struct wake_lock fuel_alert_wake_lock;

	unsigned int ui_soc;

	unsigned int capacity_old;      /* only for atomic calculation */
	unsigned int capacity_max;      /* only for dynamic calculation */
	unsigned int g_capacity_max;      /* only for dynamic calculation */
	unsigned int standard_capacity;
	int raw_capacity;

	int current_avg;
	unsigned int ttf_capacity;
	struct cv_slope *cv_data;
	int cv_data_length;

	bool capacity_max_conv;
	bool initial_update_of_soc;
	bool init_battery_temp;
	bool sleep_initial_update_of_soc;
	struct mutex fg_lock;
	struct delayed_work isr_work;

	u8 reg_OTP_53;
	u8 reg_OTP_52;

	int low_vbat_threshold;
	int low_vbat_threshold_lowtemp;
	int low_temp_limit;
	int temperature;

	int fg_irq;
	bool probe_done;

	int init_start;
};

#endif /* __S2MU107_FUELGAUGE_H */
