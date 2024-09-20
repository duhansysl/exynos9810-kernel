/*
 * s2mu107_charger.h - Header of S2MU107 Charger Driver
 *
 * Copyright (C) 2016 Samsung Electronics Co.Ltd
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

#ifndef S2MU107_CHARGER_H
#define S2MU107_CHARGER_H
#include <linux/mfd/samsung/s2mu107.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#include "../common/include/sec_charging_common.h"

#define EVT0	0
#define EVT1	1
#define EVT2	2

/* define function if need */
#define ENABLE_MIVR 0

/* Test debug log enable */
#define EN_TEST_READ 1

#define HEALTH_DEBOUNCE_CNT 1

#define MINVAL(a, b) ((a <= b) ? a : b)
#define MASK(width, shift)	(((0x1 << (width)) - 1) << shift)

#define ENABLE 1
#define DISABLE 0

/* FLASH */
#define S2MU107_FLED_STATUS	0x10
#define S2MU107_FLED_TORCH_ON	0x40

#define S2MU107_SC_INT1		0x00
#define S2MU107_SC_INT2		0x01
#define S2MU107_SC_INT3		0x02

#define S2MU107_SC_INT1_MASK	0x08
#define S2MU107_SC_INT2_MASK	0x09
#define S2MU107_SC_INT3_MASK	0x0A

#define S2MU107_SC_STATUS0		0x10
#define S2MU107_SC_STATUS1		0x11
#define S2MU107_SC_STATUS2		0x12
#define S2MU107_SC_STATUS3		0x13
#define S2MU107_SC_STATUS4		0x14
#define S2MU107_SC_STATUS5		0x15

#define S2MU107_SC_CTRL0		0x18
#define S2MU107_SC_CTRL1		0x19
#define S2MU107_SC_CTRL2		0x1A
#define S2MU107_SC_CTRL3		0x1B
#define S2MU107_SC_CTRL4		0x1C
#define S2MU107_SC_CTRL5		0x1D
#define S2MU107_SC_CTRL6		0x1E
#define S2MU107_SC_CTRL7		0x1F
#define S2MU107_SC_CTRL8		0x20
#define S2MU107_SC_CTRL9		0x21
#define S2MU107_SC_CTRL10		0x22
#define S2MU107_SC_CTRL11		0x23
#define S2MU107_SC_CTRL12		0x24
#define S2MU107_SC_CTRL13		0x25
#define S2MU107_SC_CTRL14		0x26
#define S2MU107_SC_CTRL15		0x27
#define S2MU107_SC_CTRL16		0x28
#define S2MU107_SC_CTRL17		0x29
#define S2MU107_SC_CTRL18		0x2A
#define S2MU107_SC_CTRL19		0x2B
#define S2MU107_SC_CTRL20		0x2C
#define S2MU107_SC_TEST3		0x34
#define S2MU107_SC_TEST5		0x36
#define S2MU107_SC_TEST6		0x37
#define S2MU107_SC_TEST7		0x38
#define S2MU107_SC_TEST8		0x39
#define S2MU107_SC_TEST9		0x3A
#define S2MU107_DC_CTRL0		0x41
#define S2MU107_DC_TEST4		0x5F
#define S2MU107_SC_OTP_02		0x6E

/* S2MU107_SC_CTRL0 */
#define REG_MODE_SHIFT		0
#define REG_MODE_WIDTH		4
#define REG_MODE_MASK		MASK(REG_MODE_WIDTH, REG_MODE_SHIFT)

#define CHARGER_OFF_MODE	0
#define BUCK_MODE			1
#define BST_MODE			2
#define CHG_MODE			3
#define OTG_BST_MODE		6
#define TX_BST_MODE			10
#define DUAL_BUCK_MODE		13
#define OTG_TX_BST_MODE		14

/* S2MU107_SC_STATUS0 */
#define WCIN_STATUS_SHIFT	0
#define WCIN_STATUS_WIDTH	3
#define WCIN_STATUS_MASK	MASK(WCIN_STATUS_WIDTH, WCIN_STATUS_SHIFT)

#define CHGIN_STATUS_SHIFT	4
#define CHGIN_STATUS_WIDTH	3
#define CHGIN_STATUS_MASK	MASK(CHGIN_STATUS_WIDTH, CHGIN_STATUS_SHIFT)

/* S2MU107_SC_STATUS1 */
#define CHG_FAULT_STATUS_SHIFT		4
#define CHG_FAULT_STATUS_WIDTH		4
#define CHG_FAULT_STATUS_MASK		MASK(CHG_FAULT_STATUS_WIDTH,\
					 CHG_FAULT_STATUS_SHIFT)

#define CHG_STATUS_NORMAL				0
#define CHG_STATUS_WD_SUSPEND			1
#define CHG_STATUS_WD_RST				2
#define CHG_STATUS_PRECHG_TMR_FAULT		6
#define CHG_STATUS_FASTCHG_TMR_FAULT	7
#define CHG_STATUS_TOPOFF_TMR_FAULT		8

#define CHG_CV_STATUS_SHIFT	3
#define CHG_CV_STATUS_MASK		BIT(CHG_Restart_STATUS_SHIFT)

#define CHG_Restart_STATUS_SHIFT	2
#define CHG_Restart_STATUS_MASK		BIT(CHG_Restart_STATUS_SHIFT)

#define TOP_OFF_STATUS_SHIFT		1
#define TOP_OFF_STATUS_MASK		BIT(TOP_OFF_STATUS_SHIFT)

#define DONE_STATUS_SHIFT	0
#define DONE_STATUS_MASK	BIT(DONE_STATUS_SHIFT)

/* S2MU107_SC_STATUS3 */
#define ICR_STATUS_SHIFT	0
#define ICR_STATUS_MASK		BIT(ICR_STATUS_SHIFT)

#define IVR_STATUS_SHIFT	1
#define IVR_STATUS_MASK		BIT(IVR_STATUS_SHIFT)

/* S2MU107_SC_STATUS4 */
#define OTG_STATUS_SHIFT	6
#define OTG_STATUS_WIDTH	2
#define OTG_STATUS_MASK		MASK(OTG_STATUS_WIDTH, OTG_STATUS_SHIFT)

#define TX_STATUS_SHIFT		4
#define TX_STATUS_WIDTH		2
#define TX_STATUS_MASK		MASK(TX_STATUS_WIDTH, TX_STATUS_SHIFT)

/* S2MU107_SC_STATUS5 */
#define BAT_STATUS_SHIFT	0
#define BAT_STATUS_WIDTH	3
#define BAT_STATUS_MASK		MASK(BAT_STATUS_WIDTH, BAT_STATUS_SHIFT)

#define BATID_STATUS_SHIFT	4
#define BATID_STATUS_WIDTH	2
#define BATID_STATUS_MASK	MASK(BATID_STATUS_WIDTH, BATID_STATUS_SHIFT)

/* S2MU107_SC_CTRL1 */
#define INPUT_CURRENT_LIMIT_SHIFT	0
#define INPUT_CURRENT_LIMIT_WIDTH	7
#define INPUT_CURRENT_LIMIT_MASK	MASK(INPUT_CURRENT_LIMIT_WIDTH,\
					INPUT_CURRENT_LIMIT_SHIFT)

/* S2MU107_SC_CTRL3 */
#define OTG_OCP_SW_ON_SHIFT		5
#define OTG_OCP_SW_ON_MASK		BIT(OTG_OCP_SW_ON_SHIFT)

#define OTG_OCP_SW_OFF_SHIFT	4
#define OTG_OCP_SW_OFF_MASK		BIT(OTG_OCP_SW_OFF_SHIFT)

#define SET_OTG_OCP_SHIFT	2
#define SET_OTG_OCP_WIDTH	2
#define SET_OTG_OCP_MASK	MASK(SET_OTG_OCP_WIDTH, SET_OTG_OCP_SHIFT)

/* S2MU107_SC_CTRL4 */
#define SET_CHGIN_IVR_SHIFT	3
#define SET_CHGIN_IVR_WIDTH	4
#define SET_CHGIN_IVR_MASK	MASK(SET_CHGIN_IVR_WIDTH,\
				SET_CHGIN_IVR_SHIFT)

#define SET_WCIN_IVR_SHIFT	0
#define SET_WCIN_IVR_WIDTH	3
#define SET_WCIN_IVR_MASK	MASK(SET_WCIN_IVR_WIDTH,\
				SET_WCIN_IVR_SHIFT)

/* S2MU107_SC_CTRL5 */
#define SET_VF_VBAT_SHIFT	0
#define SET_VF_VBAT_WIDTH	7
#define SET_VF_VBAT_MASK	MASK(SET_VF_VBAT_WIDTH, SET_VF_VBAT_SHIFT)

/* S2MU107_SC_CTRL6 */
#define COOL_CHARGING_CURRENT_SHIFT	0
#define COOL_CHARGING_CURRENT_WIDTH	6
#define COOL_CHARGING_CURRENT_MASK	MASK(COOL_CHARGING_CURRENT_WIDTH,\
					COOL_CHARGING_CURRENT_SHIFT)

/* S2MU107_SC_CTRL7 */
#define FAST_CHARGING_CURRENT_SHIFT	0
#define FAST_CHARGING_CURRENT_WIDTH	6
#define FAST_CHARGING_CURRENT_MASK	MASK(FAST_CHARGING_CURRENT_WIDTH,\
					FAST_CHARGING_CURRENT_SHIFT)

/* S2MU107_SC_CTRL8 */
#define SET_VSYS_SHIFT	0
#define SET_VSYS_WIDTH	4
#define SET_VSYS_MASK	MASK(SET_VSYS_WIDTH, SET_VSYS_SHIFT)

#define EN_JIG_REG_AP_SHIFT		7
#define EN_JIG_REG_AP_WIDTH		1
#define EN_JIG_REG_AP_MASK	MASK(EN_JIG_REG_AP_WIDTH, EN_JIG_REG_AP_SHIFT)

/* S2MU107_SC_CTRL12 */
#define WDT_TIME_SHIFT        1
#define WDT_TIME_WIDTH        3
#define WDT_TIME_MASK        MASK(WDT_TIME_WIDTH, WDT_TIME_SHIFT)

#define SET_EN_WDT_SHIFT 4
#define SET_EN_WDT_MASK BIT(SET_EN_WDT_SHIFT)

#define SET_EN_WDT_AP_RESET_SHIFT 5
#define SET_EN_WDT_AP_RESET_MASK BIT(SET_EN_WDT_AP_RESET_SHIFT)

#define WDT_CLR_SHIFT 0
#define WDT_CLR_MASK BIT(WDT_CLR_SHIFT)

/* S2MU107_SC_CTRL13 */
#define SET_TIME_FC_CHG_SHIFT	3
#define SET_TIME_FC_CHG_WIDTH	3
#define SET_TIME_FC_CHG_MASK	MASK(SET_TIME_FC_CHG_WIDTH, SET_TIME_FC_CHG_SHIFT)

/* S2MU107_SC_CTRL14 */
#define TOP_OFF_TIME_SHIFT    0
#define TOP_OFF_TIME_WIDTH    3
#define TOP_OFF_TIME_MASK    MASK(TOP_OFF_TIME_WIDTH, TOP_OFF_TIME_SHIFT)

/* S2MU107_SC_CTRL15 */
#define FIRST_TOPOFF_CURRENT_SHIFT	0
#define FIRST_TOPOFF_CURRENT_WIDTH	5
#define FIRST_TOPOFF_CURRENT_MASK	MASK(FIRST_TOPOFF_CURRENT_WIDTH,\
					FIRST_TOPOFF_CURRENT_SHIFT)

/* S2MU107_SC_CTRL16 */
#define SECOND_TOPOFF_CURRENT_SHIFT	0
#define SECOND_TOPOFF_CURRENT_WIDTH	5
#define SECOND_TOPOFF_CURRENT_MASK	MASK(SECOND_TOPOFF_CURRENT_WIDTH,\
					SECOND_TOPOFF_CURRENT_SHIFT)
#define IVR_M_SHIFT	1
#define IVR_M_MASK	BIT(IVR_M_SHIFT)
#define IVR_STATUS	0x02

#define REDUCE_CURRENT_STEP         25
#define MINIMUM_INPUT_CURRENT           300
#define SLOW_CHARGING_CURRENT_STANDARD      400

#define FAKE_BAT_LEVEL          50

ssize_t s2mu107_chg_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf);

ssize_t s2mu107_chg_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

#define S2MU107_CHARGER_ATTR(_name)				\
{							                    \
	.attr = {.name = #_name, .mode = 0664},	    \
	.show = s2mu107_chg_show_attrs,			    \
	.store = s2mu107_chg_store_attrs,			\
}

enum {
	CHG_REG = 0,
	CHG_DATA,
	CHG_REGS,
};

/* TODO */
enum {
	S2MU107_TOPOFF_TIMER_5m		= 0x1,
	S2MU107_TOPOFF_TIMER_10m	= 0x2,
	S2MU107_TOPOFF_TIMER_30m	= 0x3,
	S2MU107_TOPOFF_TIMER_50m	= 0x4,
	S2MU107_TOPOFF_TIMER_70m	= 0x5,
	S2MU107_TOPOFF_TIMER_90m	= 0x6,
	S2MU107_TOPOFF_TIMER_DIS	= 0x7,
};

enum {
	S2MU107_WDT_TIMER_40s	= 0x1,
	S2MU107_WDT_TIMER_50s	= 0x2,
	S2MU107_WDT_TIMER_60s	= 0x3,
	S2MU107_WDT_TIMER_70s	= 0x4,
	S2MU107_WDT_TIMER_80s	= 0x5,
	S2MU107_WDT_TIMER_90s	= 0x6,
	S2MU107_WDT_TIMER_100s	= 0x7,
};

enum {
	S2MU107_FC_CHG_TIMER_4hr	= 0x1,
	S2MU107_FC_CHG_TIMER_6hr	= 0x2,
	S2MU107_FC_CHG_TIMER_8hr	= 0x3,
	S2MU107_FC_CHG_TIMER_10hr	= 0x4,
	S2MU107_FC_CHG_TIMER_12hr	= 0x5,
	S2MU107_FC_CHG_TIMER_14hr	= 0x6,
	S2MU107_FC_CHG_TIMER_16hr	= 0x7,
};

enum {
	S2MU107_SET_OTG_OCP_500mA   = 0x0,
	S2MU107_SET_OTG_OCP_900mA   = 0x1,
	S2MU107_SET_OTG_OCP_1200mA  = 0x2,
	S2MU107_SET_OTG_OCP_1500mA  = 0x3,
};

typedef struct s2mu107_sc_platform_data {
	int chg_float_voltage;
	char *sc_name;
	char *dc_name; /* name of direct charger */
	char *fuelgauge_name;
	bool chg_eoc_dualpath;
	int recharge_vcell;
	uint32_t is_1MHz_switching:1;
	int chg_switching_freq;
	int slow_charging_current;
} s2mu107_sc_platform_data_t;


struct s2mu107_sc_data {
	struct i2c_client       *i2c;
	struct i2c_client       *i2c_common;
	struct i2c_client       *i2c_muic;
	struct device *dev;
	struct s2mu107_platform_data *s2mu107_pdata;
	struct delayed_work otg_vbus_work;
	struct delayed_work ivr_work;
	struct delayed_work rev_bst_work;
	struct wake_lock ivr_wake_lock;

	struct workqueue_struct *charger_wqueue;
	struct power_supply *psy_sc;
	struct power_supply_desc psy_sc_desc;
	struct power_supply *psy_otg;
	struct power_supply_desc psy_otg_desc;

	s2mu107_sc_platform_data_t *pdata;
	int rev_id;
	int es_id;
	int input_current;
	int charging_current;
	int topoff_current;
	int cable_type;
	bool is_charging;
	bool is_vbus;
	struct mutex charger_mutex;
	struct mutex ivr_mutex;
	struct mutex ivr_work_mutex;
	struct mutex chgin_mutex;
	struct mutex wa_chk_mutex;

	bool ovp;
	bool otg_on;

	int unhealth_cnt;
	int status;
	int health;

	int irq_det_bat;
	int irq_chg;
	int irq_chgin;
	int irq_chg_fault;
	int irq_otg;
	int irq_vbus;
	int irq_rst;
	int irq_done;
	int irq_sys;
	int irq_event;
	int irq_ivr;

	int charge_mode;
	bool bypass_mode;

	int irq_ivr_enabled;
	int ivr_on;
	bool slow_charging;

	bool boost_wa;

	/* efficiency 9V charging */
	unsigned char reg_0x9E;
	unsigned char reg_0x7B;

#if defined(CONFIG_MUIC_NOTIFIER)
	struct notifier_block cable_check;
#endif
};

#endif /*S2MU107_SW_CHARGER_H*/
