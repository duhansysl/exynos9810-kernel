/*
 * s2mu106_fuelgauge.h - Header of S2MU106 Fuel Gauge
 *
 * Copyright (C) 2018 Samsung Electronics, Inc.
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

#ifndef __S2MU106_FUELGAUGE_H
#define __S2MU106_FUELGAUGE_H __FILE__

#if defined(ANDROID_ALARM_ACTIVATED)
#include <linux/android_alarm.h>
#endif

#include <linux/wakelock.h>
#include "../sec_charging_common.h" 

/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */

#define TEMP_COMPEN		1
#define BATCAP_LEARN	1

#define S2MU106_REG_STATUS		0x00
#define S2MU106_REG_IRQ			0x02
#define S2MU106_REG_RVBAT		0x04
#define S2MU106_REG_RCUR_CC		0x06
#define S2MU106_REG_RSOC		0x08
#define S2MU106_REG_MONOUT		0x0A
#define S2MU106_REG_MONOUT_SEL		0x0C
#define S2MU106_REG_RBATCAP		0x0E
#define S2MU106_REG_BATCAP		0x10
#define S2MU106_REG_CAPCC		0x3E
#define S2MU106_REG_RSOC_R		0x2A

#define S2MU106_REG_RZADJ		0x12
#define S2MU106_REG_RBATZ0		0x16
#define S2MU106_REG_RBATZ1		0x18
#define S2MU106_REG_IRQ_LVL		0x1A
#define S2MU106_REG_START		0x1E
#define S2MU106_REG_FG_ID		0x48

#define S2MU106_REG_VM			0x67

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

struct s2mu106_fuelgauge_platform_data {
	int fuel_alert_soc;
	int fg_irq;
	int fuel_alert_vol;

	unsigned int capacity_full;

	char *fuelgauge_name;

	bool repeated_fuelalert;

	struct sec_charging_current *charging_current;

	int capacity_max;
	int capacity_max_margin;
	int capacity_min;
	int capacity_calculation_type;
	int fullsocthr;
};
struct cv_slope {
	int fg_current;
	int soc;
	int time;
};

struct s2mu106_fuelgauge_data {
	struct device           *dev;
	struct i2c_client       *i2c;
	struct i2c_client       *pmic;
	struct mutex            fuelgauge_mutex;
	struct s2mu106_fuelgauge_platform_data *pdata;
	struct power_supply	*psy_fg;
	struct power_supply	*psy_chg;
	struct power_supply	*psy_bat;
	/* struct delayed_work isr_work; */

	int cable_type;
	bool is_charging; /* charging is enabled */
	int rsoc;
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
	unsigned int standard_capacity;
	int raw_capacity;
	int current_avg;
	unsigned int ttf_capacity;
	struct cv_slope *cv_data;
	int cv_data_length;

	bool initial_update_of_soc;
	bool init_battery_temp;
	bool sleep_initial_update_of_soc;
	struct mutex fg_lock;
	struct delayed_work isr_work;

	/* register programming */
	int reg_addr;
	u8 reg_data[2];
	u8 reg_OTP_53;
	u8 reg_OTP_52;

	int low_voltage_limit_lowtemp;
	int low_voltage_recover_lowtemp;
	int low_voltage_limit;
	int low_temp_limit;
	int temperature;

	int fg_irq;
	bool probe_done;
#if (TEMP_COMPEN) || (BATCAP_LEARN)
	bool bat_charging; /* battery is charging */
#endif
#if (TEMP_COMPEN) && (BATCAP_LEARN)
	int fcc;
	int rmc;
#endif
#if (TEMP_COMPEN)
	bool vm_status; /* Now voltage mode or not */
	bool pre_vm_status;
	bool pre_is_charging;
	bool pre_bat_charging;
	bool flag_mapping;

	int socni;
	int soc0i;
	int comp_socr; /* 1% unit */
	int pre_comp_socr; /* 1% unit */
	int init_start;
	int soc_r;
	int avg_curr;
	int topoff_current;

	int i_socr_coeff;
	int t_socr_coeff;
	int t_compen_coeff;
	int low_t_compen_coeff;
#endif
#if (BATCAP_LEARN)
	bool learn_start;
	bool cond1_ok;
	int c1_count;
	int c2_count;
	int capcc;
	int batcap_ocv;
	int batcap_ocv_fin;
	int cycle;
	int soh;
	u8 batcap_0x0E;
	u8 batcap_0x0F;
#endif
	int val_0x5C;
	int low_voltage_limit_cnt;
};

#if (BATCAP_LEARN)
/* cycle, rLOW_EN, rC1_num, rC2_num, rC1_CURR, rWide_lrn_EN, Fast_lrn_EN, Auto_lrn_EN */
int BAT_L_CON[8] = {20, 0, 10, 10, 500, 0, 0, 1};
#endif

#endif /* __S2MU106_FUELGAUGE_H */
