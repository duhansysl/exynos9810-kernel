/*
 * s2mu107_charger.h - Header of S2MU107 Charger Driver
 *
 * Copyright (C) 2019 Samsung Electronics Co.Ltd
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef S2MU107_DIRECT_CHARGER_H
#define S2MU107_DIRECT_CHARGER_H
#include <linux/mfd/samsung/s2mu107.h>
#include <linux/wakelock.h>

#include "../sec_direct_charger.h"

/* Test debug log enable */
#define EN_TEST_READ 1

#define MINVAL(a, b) ((a <= b) ? a : b)
#define MASK(width, shift)	(((0x1 << (width)) - 1) << shift)

#define ENABLE 1
#define DISABLE 0

#define S2MU107_DC_INT0		0x03
#define S2MU107_DC_INT1		0x04
#define S2MU107_DC_INT2		0x05
#define S2MU107_DC_INT3		0x06

#define S2MU107_DC_INT0_MASK		0x0B
#define S2MU107_DC_INT1_MASK		0x0C
#define S2MU107_DC_INT2_MASK		0x0D
#define S2MU107_DC_INT3_MASK		0x0E

#define S2MU107_SC_STATUS1_DC		0x11
#define S2MU107_DC_STATUS0		0x17
#define S2MU107_SC_CTRL0		0x18
#define S2MU107_SC_CTRL4_DC		0x1C
#define S2MU107_SC_CTRL8_DC		0x20

#define S2MU107_SC_CTRL12_DC		0x24
#define S2MU107_SC_CTRL17_DC		0x29
#define S2MU107_SC_TEST2_DC		0x33
#define S2MU107_SC_TEST9_DC		0x3A

#define S2MU107_DC_CTRL0		0x41
#define S2MU107_DC_CTRL1		0x42
#define S2MU107_DC_CTRL2		0x43
#define S2MU107_DC_CTRL3		0x44
#define S2MU107_DC_CTRL4		0x45
#define S2MU107_DC_CTRL5		0x46
#define S2MU107_DC_CTRL6		0x47
#define S2MU107_DC_CTRL7		0x48
#define S2MU107_DC_CTRL8		0x49
#define S2MU107_DC_CTRL9		0x4A
#define S2MU107_DC_CTRL10		0x4B

#define S2MU107_DC_CTRL11		0x4C
#define S2MU107_DC_CTRL12		0x4D
#define S2MU107_DC_CTRL13		0x4E
#define S2MU107_DC_CTRL14		0x4F
#define S2MU107_DC_CTRL15		0x50
#define S2MU107_DC_CTRL16		0x51
#define S2MU107_DC_CTRL17		0x52
#define S2MU107_DC_CTRL18		0x53
#define S2MU107_DC_CTRL19		0x54
#define S2MU107_DC_CTRL20		0x55

#define S2MU107_DC_CTRL21		0x56
#define S2MU107_DC_CTRL22		0x57
#define S2MU107_DC_CTRL23		0x58
#define S2MU107_DC_CTRL24		0x59
#define S2MU107_DC_CTRL25		0x5A

#define S2MU107_DC_TEST0		0x5B
#define S2MU107_DC_TEST1		0x5C
#define S2MU107_DC_TEST2		0x5D
#define S2MU107_DC_TEST3		0x5E
#define S2MU107_DC_TEST4		0x5F
#define S2MU107_DC_TEST5		0x60
#define S2MU107_DC_TEST6		0x61
#define S2MU107_DC_TEST7		0x62

#define S2MU107_SC_OTP_96		0xCC

#define S2MU107_SC_OTP103		0xD3
#define S2MU107_DC_CD_OTP_01		0xD8
#define S2MU107_DC_CD_OTP_02		0xD9
#define S2MU107_DC_INPUT_OTP_04		0xDF

/* S2MU107_DC_INT1_MASK */
#define DC_WCIN_RCP_INT_MASK_SHIFT	5
#define DC_WCIN_RCP_INT_MASK_MASK	BIT(DC_WCIN_RCP_INT_MASK_SHIFT)

/* S2MU107_SC_STATUS1_DC */
#define SC_FAULT_STATUS_SHIFT		4
#define SC_FAULT_STATUS_WIDTH		4
#define SC_FAULT_STATUS_MASK		MASK(SC_FAULT_STATUS_WIDTH,\
					 SC_FAULT_STATUS_SHIFT)
#define SC_STATUS_WD_SUSPEND		1
#define SC_STATUS_WD_RST		2

/* S2MU107_DC_STATUS0 */
#define CV_REGULATION_STATUS		0x10
#define LONG_CC_STATUS			0x08
#define THERMAL_STATUS			0x04
#define ICL_STATUS			0x02
#define NORMAL_CHARGING_STATUS		0x01

/* S2MU107_SC_CTRL0 */
#define SC_REG_MODE_SHIFT		0
#define SC_REG_MODE_WIDTH		4
#define SC_REG_MODE_MASK		MASK(SC_REG_MODE_WIDTH, SC_REG_MODE_SHIFT)
#define SC_REG_MODE_DUAL_BUCK		(0x0d)

/* S2MU107_SC_CTRL4_DC */
#define SET_IVR_CHGIN_SHIFT		3
#define SET_IVR_CHGIN_WIDTH		3
#define SET_IVR_CHGIN_MASK		MASK(SET_IVR_CHGIN_WIDTH, SET_IVR_CHGIN_SHIFT)
#define SET_IVR_WCIN_SHIFT		0
#define SET_IVR_WCIN_WIDTH		3
#define SET_IVR_WCIN_MASK		MASK(SET_IVR_WCIN_WIDTH, SET_IVR_WCIN_SHIFT)

/* S2MU107_SC_CTRL8_DC */
#define SET_VF_VSYS_SHIFT		0
#define SET_VF_VSYS_WIDTH		4
#define SET_VF_VSYS_MASK		MASK(SET_VF_VSYS_WIDTH, SET_VF_VSYS_SHIFT)

/* S2MU107_SC_TEST2_DC */
#define SEL_VSYS_RS_DC_MASK		(0x1)

/* S2MU107_SC_TEST9_DC */
#define T_GD_ASYNC_SHIFT		0
#define T_GD_ASYNC_WIDTH		2
#define T_GD_ASYNC_MASK			MASK(T_GD_ASYNC_WIDTH, T_GD_ASYNC_SHIFT)
#define SET_ASYNC_MODE			0x3
/* S2MU107_SC_CTRL17_DC */
#define T_EN_VSYS_DISCHARGE_SHIFT	6
#define T_EN_VSYS_DISCHARGE_WIDTH	2
#define T_EN_VSYS_DISCHARGE_MASK	MASK(T_EN_VSYS_DISCHARGE_WIDTH, T_EN_VSYS_DISCHARGE_SHIFT)

/* S2MU107_DC_CTRL0 */
#define FREQ_SEL_SHIFT			5
#define FREQ_SEL_WIDTH			3
#define FREQ_SEL_MASK			MASK(FREQ_SEL_WIDTH, FREQ_SEL_SHIFT)

#define DC_EN_MASK			0x01
#define DC_FORCED_EN_SHIFT		1
#define DC_FORCED_EN_MASK		BIT(DC_FORCED_EN_SHIFT)

/* S2MU107_DC_CTRL1 */
#define SET_CV_SHIFT			0
#define SET_CV_WIDTH			7
#define SET_CV_MASK			MASK(SET_CV_WIDTH, SET_CV_SHIFT)
#define SET_CV_STEP_UV			(10000)
#define SET_CV_MIN_UV			(3600000)
#define SET_CV_MAX_UV			(4870000)
#define SET_CV_4400_MV			(0x50)
#define SET_CV_4550_MV			(0x5F)

/* S2MU107_DC_CTRL2 */
#define DC_SET_CHGIN_ICHG_SHIFT		0
#define	DC_SET_CHGIN_ICHG_WIDTH		7
#define DC_SET_CHGIN_ICHG_MASK		MASK(DC_SET_CHGIN_ICHG_WIDTH,\
					DC_SET_CHGIN_ICHG_SHIFT)
/* S2MU107_DC_CTRL3 */
#define DC_SET_CHGIN_IDISCHG_SHIFT	0
#define	DC_SET_CHGIN_IDISCHG_WIDTH	7
#define DC_SET_CHGIN_IDISCHG_MASK	MASK(DC_SET_CHGIN_IDISCHG_WIDTH,\
					DC_SET_CHGIN_IDISCHG_SHIFT)
/* S2MU107_DC_CTRL4 */
#define DC_SET_WCIN_ICHG_SHIFT		0
#define	DC_SET_WCIN_ICHG_WIDTH		7
#define DC_SET_WCIN_ICHG_MASK		MASK(DC_SET_WCIN_ICHG_WIDTH,\
					DC_SET_WCIN_ICHG_SHIFT)
/* S2MU107_DC_CTRL5 */
#define DC_SET_WCIN_IDISCHG_SHIFT	0
#define	DC_SET_WCIN_IDISCHG_WIDTH	7
#define DC_SET_WCIN_IDISCHG_MASK	MASK(DC_SET_WCIN_IDISCHG_WIDTH,\
					DC_SET_WCIN_IDISCHG_SHIFT)

/* S2MU107_DC_CTRL6 */
#define CHGIN_OVP_RISE_SHIFT		4
#define CHGIN_OVP_RISE_WIDTH		4
#define CHGIN_OVP_RISE_MASK		MASK(CHGIN_OVP_RISE_WIDTH, CHGIN_OVP_RISE_SHIFT)

#define CHGIN_OVP_FALL_SHIFT		0
#define CHGIN_OVP_FALL_WIDTH		4
#define CHGIN_OVP_FALL_MASK		MASK(CHGIN_OVP_FALL_WIDTH, CHGIN_OVP_FALL_SHIFT)

/* S2MU107_DC_CTRL7 */
#define DC_SET_VBYP_OVP_SHIFT		0
#define DC_SET_VBYP_OVP_WIDTH		7
#define DC_SET_VBYP_OVP_MASK		MASK(DC_SET_VBYP_OVP_WIDTH, DC_SET_VBYP_OVP_SHIFT)

/* S2MU107_DC_CTRL8 */
#define DC_SET_VBAT_OVP_SHIFT		0
#define DC_SET_VBAT_OVP_WIDTH		8
#define DC_SET_VBAT_OVP_MASK		MASK(DC_SET_VBYP_OVP_WIDTH, DC_SET_VBYP_OVP_SHIFT)

/* S2MU107_DC_CTRL13 */
#define DC_SET_CC_SHIFT			0
#define DC_SET_CC_WIDTH			7
#define DC_SET_CC_MASK			MASK(DC_SET_CC_WIDTH, DC_SET_CC_SHIFT)

/* S2MU107_DC_CTRL15 */
#define DC_TOPOFF_SHIFT			4
#define DC_TOPOFF_WIDTH			4
#define DC_TOPOFF_MASK			MASK(DC_TOPOFF_WIDTH, DC_TOPOFF_SHIFT)

#define DC_CC_BAND_WIDTH_SHIFT		0
#define DC_CC_BAND_WIDTH_WIDTH		3
#define DC_CC_BAND_WIDTH_MASK		MASK(DC_CC_BAND_WIDTH_WIDTH, DC_CC_BAND_WIDTH_SHIFT)
//#define DC_CC_BAND_WIDTH_MIN_MA		(150)
//#define DC_CC_BAND_WIDTH_STEP_MA	(50)
#define DC_CC_BAND_WIDTH_MIN_MA		(300)
#define DC_CC_BAND_WIDTH_400_MA		(400)
#define DC_CC_BAND_WIDTH_STEP_MA	(100)

/* S2MU107_DC_CTRL16 */
#define DC_EN_LONG_CC_SHIFT		7
#define	DC_EN_LONG_CC_MASK		BIT(DC_EN_LONG_CC_SHIFT)
#define	CV_OFF_SHIFT			6
#define	CV_OFF_MASK			BIT(CV_OFF_SHIFT)
#define LONGCC_SEL_CELL_PACKB_SHIFT	5
#define LONGCC_SEL_CELL_PACKB_MASK	BIT(LONGCC_SEL_CELL_PACKB_SHIFT)

#define	DC_LONG_CC_STEP_SHIFT		0
#define DC_LONG_CC_STEP_WIDTH		5
#define DC_LONG_CC_STEP_MASK		MASK(DC_LONG_CC_STEP_WIDTH, DC_LONG_CC_STEP_SHIFT)

/* S2MU107_DC_CTRL17 */
#define EN_SCP_CHECK_SHIFT		5
#define EN_SCP_CHECK_MASK		BIT(EN_SCP_CHECK_SHIFT)

/* S2MU107_DC_CTRL20 */
#define	DC_EN_THERMAL_LOOP_SHIFT	7
#define DC_EN_THERMAL_LOOP_MASK		BIT(DC_EN_THERMAL_LOOP_SHIFT)

#define TEMPERATURE_SOURCE_MASK		0x40
#define CHIP_JUNCTION			0
#define	EXTERNAL_NTC			1

#define THERMAL_RISING_SHIFT		0
#define	THERMAL_RISING_WIDTH		6
#define THERMAL_RISING_MASK		MASK(THERMAL_RISING_WIDTH, THERMAL_RISING_SHIFT)

/* S2MU107_DC_CTRL21 */
#define THERMAL_FALLING_SHIFT		0
#define	THERMAL_FALLING_WIDTH		6
#define THERMAL_FALLING_MASK		MASK(THERMAL_FALLING_WIDTH, THERMAL_FALLING_SHIFT)

/* S2MU107_DC_CTRL22 */
#define	TA_COMMUNICATION_FAIL_MASK	BIT(1)
#define TA_TRANSIENT_DONE_MASK		BIT(0)

/* S2MU107_DC_CTRL23 */
#define THERMAL_WAIT_SHIFT		4
#define THERMAL_WAIT_WIDTH		3
#define THERMAL_WAIT_MASK		MASK(THERMAL_WAIT_WIDTH, THERMAL_WAIT_SHIFT)

#define DC_SEL_BAT_OK_SHIFT		0
#define DC_SEL_BAT_OK_WIDTH		4
#define DC_SEL_BAT_OK_MASK		MASK(DC_SEL_BAT_OK_WIDTH, DC_SEL_BAT_OK_SHIFT)

/* S2MU107_DC_TEST2 */
#define DC_EN_OCP_FLAG_SHIFT		6
#define DC_EN_OCP_FLAG_WIDTH		2
#define DC_EN_OCP_FLAG_MASK		MASK(DC_EN_OCP_FLAG_WIDTH, DC_EN_OCP_FLAG_SHIFT)

/* S2MU107_DC_TEST4 */
#define T_DC_EN_WCIN_PWRTR_MODE_SHIFT	5
#define T_DC_EN_WCIN_PWRTR_MODE_MASK	BIT(T_DC_EN_WCIN_PWRTR_MODE_SHIFT)

/* S2MU107_DC_TEST6 */
#define T_DC_EN_DC_CV_SHIFT		0
#define T_DC_EN_DC_CV_WIDTH		2
#define T_DC_EN_DC_CV_MASK		MASK(T_DC_EN_DC_CV_WIDTH, T_DC_EN_DC_CV_SHIFT)

/* S2MU107_DC_CD_OTP_01 */
#define SET_IIN_OFF_SHIFT		4
#define SET_IIN_OFF_WIDTH		4
#define SET_IIN_OFF_MASK		MASK(SET_IIN_OFF_WIDTH, SET_IIN_OFF_SHIFT)
#define SET_IIN_OFF_MIN_MA		(100)
#define SET_IIN_OFF_MAX_MA		(850)
#define SET_IIN_OFF_STEP_MA		(50)
#define SET_IIN_OFF_TARGET_MA		(250)

/* S2MU107_SC_OTP_96 */
#define SET_VSYS_OVP_RISING_SHIFT	0
#define SET_VSYS_OVP_RISING_WIDTH	2
#define SET_VSYS_OVP_RISING_MASK	MASK(SET_VSYS_OVP_RISING_WIDTH, SET_VSYS_OVP_RISING_SHIFT)
#define SET_VSYS_OVP_FALLING_SHIFT	2
#define SET_VSYS_OVP_FALLING_WIDTH	2
#define SET_VSYS_OVP_FALLING_MASK	MASK(SET_VSYS_OVP_FALLING_SHIFT, SET_VSYS_OVP_FALLING_SHIFT)

#define DIG_CV_SHIFT		0
#define DIG_CV_WIDTH		4
#define DIG_CV_MASK		MASK(DIG_CV_WIDTH, DIG_CV_SHIFT)
#define DIG_CV_MAX_UV		(250000)
#define DIG_CV_STEP_UV		(15625)

/* S2MU107_SC_OTP103 */
#define T_SET_VF_VBAT_MASK		0x07

/* S2MU107_SC_CTRL12 */
#define WDT_CLR_SHIFT 0
#define WDT_CLR_MASK BIT(WDT_CLR_SHIFT)

/* S2MU107_DC_CD_OTP_02 */
#define RCP_ACTION_SHIFT	4
#define CD_OCP_ACTION_SHIFT	1
#define RCP_ACTION_MASK		BIT(RCP_ACTION_SHIFT)
#define CD_OCP_ACTION_MASK	BIT(CD_OCP_ACTION_SHIFT)

/* S2MU107_DC_INPUT_OTP_04 */
#define RAMP_OFF_ACTION_SHIFT	0
#define PLUG_OUT_ACTION_SHIFT	4
#define RAMP_OFF_ACTION_MASK	BIT(RAMP_OFF_ACTION_SHIFT)
#define PLUG_OUT_ACTION_MASK	BIT(PLUG_OUT_ACTION_SHIFT)

#define FAKE_BAT_LEVEL          50

typedef enum {
	DC_STATE_OFF = 0,
	DC_STATE_CHECK_VBAT,
	DC_STATE_PRESET,
	DC_STATE_START_CC,
	DC_STATE_CC,
	DC_STATE_SLEEP_CC,
	DC_STATE_ADJUSTED_CC,
	DC_STATE_CV,
	DC_STATE_WAKEUP_CC,
	DC_STATE_DONE,
	DC_STATE_MAX_NUM,
} s2mu107_dc_state_t;

typedef enum {
	DC_TRANS_INVALID = 0,
	DC_TRANS_CHG_ENABLED,
	DC_TRANS_NO_COND,
	DC_TRANS_VBATT_RETRY,
	DC_TRANS_WAKEUP_DONE,
	DC_TRANS_RAMPUP,
	DC_TRANS_RAMPUP_FINISHED,
	DC_TRANS_BATTERY_OK,
	DC_TRANS_BATTERY_NG,
	DC_TRANS_CHGIN_OKB_INT,
	DC_TRANS_BAT_OKB_INT,
	DC_TRANS_NORMAL_CHG_INT,
	DC_TRANS_DC_DONE_INT,
	DC_TRANS_CC_STABLED,
	DC_TRANS_CC_WAKEUP,
	DC_TRANS_DETACH,
	DC_TRANS_FAIL_INT,
	DC_TRANS_FLOAT_VBATT,
	DC_TRANS_RAMPUP_OVER_VBATT,
	DC_TRANS_RAMPUP_OVER_CURRENT,
	DC_TRANS_SET_CURRENT_MAX,
	DC_TRANS_TOP_OFF_CURRENT,
	DC_TRANS_DC_OFF_INT,
	DC_TRANS_MAX_NUM,
} s2mu107_dc_trans_t;

typedef enum {
	PMETER_ID_VCHGIN = 0,
	PMETER_ID_ICHGIN,
	PMETER_ID_IWCIN,
	PMETER_ID_VBATT,
	PMETER_ID_VSYS,
	PMETER_ID_TDIE,
	PMETER_ID_MAX_NUM,
} s2mu107_pmeter_id_t;

typedef enum {
	S2MU107_DC_DISABLE = 0,
	S2MU107_DC_ENABLE
} s2mu107_dc_ctrl_t;

typedef enum {
	S2MU107_DC_PPS_SIGNAL_DEC = 0,
	S2MU107_DC_PPS_SIGNAL_INC
} s2mu107_dc_pps_signal_t;

enum {
	TA_PWR_TYPE_25W = 0,
	TA_PWR_TYPE_45W,
};

typedef enum {
	S2MU107_DC_MODE_TA_CC = 0,
	S2MU107_DC_MODE_DC_CC
} s2mu107_dc_cc_mode;

typedef enum {
	S2MU107_DC_SC_STATUS_OFF,
	S2MU107_DC_SC_STATUS_CHARGE,
	S2MU107_DC_SC_STATUS_DUAL_BUCK
} s2mu107_dc_sc_status_t;

#define SC_CHARGING_ENABLE_CHECK_VBAT	5
#define DC_TA_MIN_PPS_CURRENT		1000
#define DC_TA_START_PPS_CURR_45W	2000
#define DC_TA_START_PPS_CURR_25W	1000
#define DC_MIN_VBAT		3700
#define DC_TA_CURR_STEP_MA	50
#define DC_TA_VOL_STEP_MV	20
#define DC_MAX_SOC		95
#define DC_MAX_SHIFTING_CNT	60
#define DC_TOPOFF_CURRENT_MA	1000
#define DC_TA_VOL_END_MV	9000
#define DC_MAX_INPUT_CURRENT_MA		12000

#define CONV_STR(x, r) { case x: r = #x; break; }

ssize_t s2mu107_dc_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf);

ssize_t s2mu107_dc_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

#define S2MU107_DIRECT_CHARGER_ATTR(_name)				\
{							                    \
	.attr = {.name = #_name, .mode = 0664},	    \
	.show = s2mu107_dc_show_attrs,			    \
	.store = s2mu107_dc_store_attrs,			\
}

typedef struct s2mu107_dc_platform_data {
	char *dc_name;
	char *pm_name;
	char *fg_name;
	char *sc_name;
	char *sec_dc_name;
	int input_current_limit;
	int charging_current;
	int topoff_current;
	int sense_resistance;
	int recharge_vcell;
	int chg_switching_freq;
	int temperature_source; /* 0 : chip junction, 1 : external ntc for FG */
} s2mu107_dc_platform_data_t;

typedef struct s2mu107_dc_pd_data {
	unsigned int pdo_pos;
	unsigned int taMaxVol;
	unsigned int taMaxCur;
	unsigned int taMaxPwr;
} s2mu107_dc_pd_data_t;

typedef struct s2mu107_dc_prop {
	int prop;
	union power_supply_propval value;
} s2mu107_dc_prop_t;

struct s2mu107_dc_data {
	struct i2c_client       *i2c;
	struct i2c_client       *i2c_common;
	struct device *dev;
	struct s2mu107_platform_data *s2mu107_pdata;
	struct power_supply *psy_fg;
	struct power_supply *psy_sc;
	struct power_supply *psy_sec_dc;

	struct delayed_work timer_wqueue;
	struct delayed_work check_vbat_wqueue;
	struct delayed_work start_cc_wqueue;
	struct delayed_work state_manager_wqueue;
	struct delayed_work set_prop_wqueue;
	struct delayed_work dc_monitor_work;
	struct delayed_work chk_valid_wqueue;
	struct delayed_work update_wqueue;

	/* step check monitor */
	struct delayed_work step_monitor_work;
	struct workqueue_struct *step_monitor_wqueue;
	struct wake_lock dc_mon_wake_lock;
	struct alarm dc_monitor_alarm;
	unsigned int alarm_interval;
	int step_now;

	struct power_supply *psy_dc;
	struct power_supply_desc psy_dc_desc;

	s2mu107_dc_platform_data_t *pdata;
	s2mu107_dc_prop_t prop_data;

	int rev_id;
	int cable_type;
	bool is_charging;
	bool is_plugout_mask;
	int ta_pwr_type;
	struct mutex dc_mutex;
	struct mutex timer_mutex;
	struct mutex dc_state_mutex;
	struct mutex pps_isr_mutex;
	struct mutex trans_mutex;
	struct mutex dc_mon_mutex;
	struct mutex auto_pps_mutex;
	struct mutex mode_mutex;
	struct wake_lock wake_lock;
	struct wake_lock state_manager_wake;
	struct wake_lock mode_irq;
	struct wake_lock fail_irq;
	struct wake_lock thermal_irq;

	s2mu107_dc_state_t dc_state;
	s2mu107_dc_trans_t dc_trans;
	void (*dc_state_fp[DC_STATE_MAX_NUM])(void *);
	s2mu107_dc_pd_data_t *pd_data;
	s2mu107_dc_sc_status_t dc_sc_status;
	struct power_supply *psy_pmeter;
	int pmeter[PMETER_ID_MAX_NUM];
	unsigned int targetIIN;
	unsigned int adjustIIN;
	unsigned int minIIN;
	unsigned int chgIIN;
	unsigned int ppsVol;
	unsigned int floatVol;
	unsigned int step_vbatt_mv;
	unsigned int step_iin_ma;
	int vchgin_okb_retry;
	unsigned int cc_count;
	unsigned int step_cv_cnt;
	unsigned int chk_vbat_margin;
	unsigned int chk_vbat_charged;
	unsigned int chk_vbat_prev;
	unsigned int chk_iin_prev;

	wait_queue_head_t wait;
	bool is_pps_ready;
	bool is_rampup_adjusted;
	bool is_autopps_disabled;
	int rise_speed_dly_ms;

	int dc_chg_status;
	int ps_health;

	int irq_base;

	/* DC_INT_0 */
	int irq_ramp_fail;
	int irq_normal_charging;
	int irq_wcin_okb;
	int irq_vchgin_okb;

	int irq_byp2out_ovp;
	int irq_byp_ovp;
	int irq_vbat_okb;
	int irq_out_ovp;

	/* DC_INT_1 */
	/* [7] rsvd */
	int irq_ocp;
	int irq_wcin_rcp;
	int irq_chgin_rcp;
	int irq_off;
	int irq_plug_out;
	int irq_wcin_diod_prot;
	int irq_chgin_diod_prot;

	/* DC_INT_2 */
	int irq_done;
	int irq_pps_fail;
	int irq_long_cc;
	int irq_thermal;
	int irq_scp;
	int irq_cv;
	int irq_wcin_icl;
	int irq_chgin_icl;

	/* DC_INT_3 */
	/* [7] rsvd */
	int irq_sc_off;
	int irq_pm_off;
	int irq_wcin_up;
	int irq_wcin_down;	
	int irq_tsd;

	/* [1] rsvd */
	int irq_tfb;

	long mon_chk_time;
};

extern int sec_pd_get_apdo_max_power(unsigned int *pdo_pos, unsigned int *taMaxVol, unsigned int *taMaxCur, unsigned int *taMaxPwr);
extern int sec_pd_select_pps(int num, int ppsVol, int ppsCur);
extern int sec_pps_enable(int num, int ppsVol, int ppsCur, int enable);
extern int sec_get_pps_voltage(void);
#endif /*S2MU107_CHARGER_H*/
