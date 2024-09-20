/*
 * s2mu106_charger.h - Header of S2MU106 Charger Driver
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

#ifndef S2MU106_CHARGER_H
#define S2MU106_CHARGER_H
#include <linux/mfd/samsung/s2mu106.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#include "../sec_charging_common.h"

extern bool mfc_fw_update;

/* define function if need */
#define ENABLE_MIVR 0

/* define IRQ function if need */
#define EN_BAT_DET_IRQ 1
#define EN_CHG1_IRQ_CHGIN 0

/* Test debug log enable */
#define EN_TEST_READ 1


#define HEALTH_DEBOUNCE_CNT 1


#define MINVAL(a, b) ((a <= b) ? a : b)
#define MASK(width, shift)	(((0x1 << (width)) - 1) << shift)

#define ENABLE 1
#define DISABLE 0

#define S2MU106_CHG_INT1		0x01
#define S2MU106_CHG_INT2		0x02
#define S2MU106_CHG_INT3		0x03

#define S2MU106_CHG_INT1M		0x08
#define S2MU106_CHG_INT2M		0x09
#define S2MU106_CHG_INT3M		0x0A

#define S2MU106_CHG_STATUS0		0x0E
#define S2MU106_CHG_STATUS1		0x0F
#define S2MU106_CHG_STATUS2		0x10
#define S2MU106_CHG_STATUS3		0x11
#define S2MU106_CHG_STATUS4		0x12
#define S2MU106_CHG_STATUS5		0x13

#define S2MU106_CHG_CTRL0		0x18
#define S2MU106_CHG_CTRL1		0x19
#define S2MU106_CHG_CTRL2		0x1A
#define S2MU106_CHG_CTRL3		0x1B
#define S2MU106_CHG_CTRL4		0x1C
#define S2MU106_CHG_CTRL5		0x1D
#define S2MU106_CHG_CTRL6		0x1E
#define S2MU106_CHG_CTRL7		0x1F
#define S2MU106_CHG_CTRL8		0x20
#define S2MU106_CHG_CTRL9		0x21
#define S2MU106_CHG_CTRL10		0x22

#define S2MU106_CHG_CTRL11		0x23
#define S2MU106_CHG_CTRL12		0x24
#define S2MU106_CHG_CTRL13		0x25
#define S2MU106_CHG_CTRL14		0x26
#define S2MU106_CHG_CTRL15		0x27
#define S2MU106_CHG_CTRL16		0x28
#define S2MU106_CHG_CTRL17		0x29
#define S2MU106_CHG_CTRL18		0x2A
#define S2MU106_CHG_CTRL19		0x2B
#define S2MU106_CHG_CTRL20		0x2C

#define S2MU106_CHG_CTRL21		0x2D
#define S2MU106_CHG_CTRL22		0x2E
#define S2MU106_CHG_CTRL23		0x2F
#define S2MU106_CHG_CTRL24		0x30

/* S2MU106_CHG_STATUS0 */
#define WCIN_STATUS_SHIFT	4
#define WCIN_STATUS_WIDTH	3
#define WCIN_STATUS_MASK	MASK(WCIN_STATUS_WIDTH, WCIN_STATUS_SHIFT)

#define CHGIN_STATUS_SHIFT	0
#define CHGIN_STATUS_WIDTH	3
#define CHGIN_STATUS_MASK	MASK(CHGIN_STATUS_WIDTH, CHGIN_STATUS_SHIFT)


/* S2MU106_CHG_STATUS1 */
#define CHG_FAULT_STATUS_SHIFT		4
#define CHG_FAULT_STATUS_WIDTH		4
#define CHG_FAULT_STATUS_MASK		MASK(CHG_FAULT_STATUS_WIDTH,\
					 CHG_FAULT_STATUS_SHIFT)

#define CHG_STATUS_NORMAL	0
#define CHG_STATUS_WD_SUSPEND	1
#define CHG_STATUS_WD_RST	2
#define CHG_STATUS_TSD		3
#define CHG_STATUS_TFB		4
#define CHG_STATUS_TO_PRE_CHARGE	6
#define CHG_STATUS_TO_FAST_CHARGE	7

#define CHG_CV_STATUS_SHIFT	3
#define CHG_CV_STATUS_MASK		BIT(CHG_Restart_STATUS_SHIFT)

#define CHG_Restart_STATUS_SHIFT	2
#define CHG_Restart_STATUS_MASK		BIT(CHG_Restart_STATUS_SHIFT)

#define TOP_OFF_STATUS_SHIFT		1
#define TOP_OFF_STATUS_MASK		BIT(TOP_OFF_STATUS_SHIFT)

#define DONE_STATUS_SHIFT	0
#define DONE_STATUS_MASK	BIT(DONE_STATUS_SHIFT)

/* S2MU106_CHG_STATUS2 */
#define OTG_STATUS_SHIFT	6
#define OTG_STATUS_WIDTH	2
#define OTG_STATUS_MASK		MASK(OTG_STATUS_WIDTH, OTG_STATUS_SHIFT)

#define TX_STATUS_SHIFT		4
#define TX_STATUS_WIDTH		2
#define TX_STATUS_MASK		MASK(TX_STATUS_WIDTH, TX_STATUS_SHIFT)

/* S2MU106_CHG_STATUS3 */
#define BAT_STATUS_SHIFT	1
#define BAT_STATUS_WIDTH	3
#define BAT_STATUS_MASK		MASK(BAT_STATUS_WIDTH, BAT_STATUS_SHIFT)

#define DET_BAT_STATUS_SHIFT	0
#define DET_BAT_STATUS_MASK	BIT(DET_BAT_STATUS_SHIFT)

/* S2MU106_CHG_STATUS5 */
#define ICR_STATUS_SHIFT	2
#define ICR_STATUS_MASK		BIT(ICR_STATUS_SHIFT)

#define IVR_STATUS_SHIFT	3
#define IVR_STATUS_MASK		BIT(IVR_STATUS_SHIFT)

#define IVR_M_SHIFT	1
#define IVR_M_MASK	BIT(IVR_M_SHIFT)
#define IVR_STATUS	0x08

/* S2MU106_CHG_CTRL0 */
#define REG_MODE_SHIFT		0
#define REG_MODE_WIDTH		4
#define REG_MODE_MASK		MASK(REG_MODE_WIDTH, REG_MODE_SHIFT)

#define CHARGER_OFF_MODE	0
#define BUCK_MODE		1
#define BST_MODE		2
#define CHG_MODE		3
#define OTG_BST_MODE		6

/* S2MU106_CHG_CTRL1, S2MU106_CHG_CTRL2 */
#define INPUT_CURRENT_LIMIT_SHIFT	0
#define INPUT_CURRENT_LIMIT_WIDTH	7
#define INPUT_CURRENT_LIMIT_MASK	MASK(INPUT_CURRENT_LIMIT_WIDTH,\
					INPUT_CURRENT_LIMIT_SHIFT)

/* S2MU106_CHG_CTRL3 */
#define SEL_PRIO_WCIN_SHIFT		6
#define SEL_PRIO_WCIN_SHIFT_MASK	BIT(SEL_PRIO_WCIN_SHIFT)

#define OTG_OCP_SW_ON_SHIFT		5
#define OTG_OCP_SW_ON_MASK		BIT(OTG_OCP_SW_ON_SHIFT)

#define OTG_OCP_SW_OFF_SHIFT	4
#define OTG_OCP_SW_OFF_MASK		BIT(OTG_OCP_SW_OFF_SHIFT)

#define SET_OTG_OCP_SHIFT	2
#define SET_OTG_OCP_WIDTH	2
#define SET_OTG_OCP_MASK	MASK(SET_OTG_OCP_WIDTH, SET_OTG_OCP_SHIFT)

#define SET_TX_OCP_SHIFT	0
#define SET_TX_OCP_WIDTH	2
#define SET_TX_OCP_MASK	MASK(SET_TX_OCP_WIDTH, SET_TX_OCP_SHIFT)

/* S2MU106_CHG_CTRL5 */
#define SET_VF_VBAT_SHIFT	0
#define SET_VF_VBAT_WIDTH	7
#define SET_VF_VBAT_MASK	MASK(SET_VF_VBAT_WIDTH, SET_VF_VBAT_SHIFT)

#define SET_CHGIN_IVR_SHIFT	2
#define SET_CHGIN_IVR_WIDTH	2
#define SET_CHGIN_IVR_MASK	MASK(SET_CHGIN_IVR_WIDTH,\
				SET_CHGIN_IVR_SHIFT)

#define SET_WCIN_IVR_SHIFT	2
#define SET_WCIN_IVR_WIDTH	2
#define SET_WCIN_IVR_MASK	MASK(SET_WCIN_IVR_WIDTH,\
				SET_WCIN_IVR_SHIFT)

/* S2MU106_CHG_CTRL6 */
#define COOL_CHARGING_CURRENT_SHIFT	0
#define COOL_CHARGING_CURRENT_WIDTH	6
#define COOL_CHARGING_CURRENT_MASK	MASK(COOL_CHARGING_CURRENT_WIDTH,\
					COOL_CHARGING_CURRENT_SHIFT)

/* S2MU106_CHG_CTRL7 */
#define FAST_CHARGING_CURRENT_SHIFT	0
#define FAST_CHARGING_CURRENT_WIDTH	6
#define FAST_CHARGING_CURRENT_MASK	MASK(FAST_CHARGING_CURRENT_WIDTH,\
					FAST_CHARGING_CURRENT_SHIFT)

/* S2MU106_CHG_CTRL8 */
#define SET_VSYS_SHIFT	0
#define SET_VSYS_WIDTH	3
#define SET_VSYS_MASK	MASK(SET_VSYS_WIDTH, SET_VSYS_SHIFT)

#define EN_JIG_REG_AP_SHIFT		7
#define EN_JIG_REG_AP_WIDTH		1
#define EN_JIG_REG_AP_MASK	MASK(EN_JIG_REG_AP_WIDTH, EN_JIG_REG_AP_SHIFT)

/* S2MU106_CHG_CTRL9 */
#define SET_BAT_OCP_SHIFT	0
#define SET_BAT_OCP_WIDTH	3
#define SET_BAT_OCP_MASK	MASK(SET_BAT_OCP_WIDTH, SET_BAT_OCP_SHIFT)

/* S2MU106_CHG_CTRL10 */
#define FIRST_TOPOFF_CURRENT_SHIFT	4
#define FIRST_TOPOFF_CURRENT_WIDTH	4
#define FIRST_TOPOFF_CURRENT_MASK	MASK(FIRST_TOPOFF_CURRENT_WIDTH,\
					FIRST_TOPOFF_CURRENT_SHIFT)

#define SECOND_TOPOFF_CURRENT_SHIFT	0
#define SECOND_TOPOFF_CURRENT_WIDTH	4
#define SECOND_TOPOFF_CURRENT_MASK	MASK(SECOND_TOPOFF_CURRENT_WIDTH,\
					SECOND_TOPOFF_CURRENT_SHIFT)

/* S2MU106_CHG_CTRL11 */
#define SET_VF_BOOST_SHIFT	0
#define SET_VF_BOOST_WIDTH	8
#define SET_VF_BOOST_MASK	MASK(SET_VF_BOOST_WIDTH, SET_VF_BOOST_SHIFT)

/* S2MU106_CHG_CTRL12 */
#define WDT_TIME_SHIFT        1
#define WDT_TIME_WIDTH        3

#define WDT_TIME_MASK        MASK(WDT_TIME_WIDTH, WDT_TIME_SHIFT)
#define SET_EN_WDT_SHIFT 4
#define SET_EN_WDT_MASK BIT(SET_EN_WDT_SHIFT)

#define SET_EN_WDT_AP_RESET_SHIFT 5
#define SET_EN_WDT_AP_RESET_MASK BIT(SET_EN_WDT_AP_RESET_SHIFT)

#define WDT_CLR_SHIFT 0
#define WDT_CLR_MASK BIT(WDT_CLR_SHIFT)

/* S2MU106_CHG_CTRL13 */
#define SET_TIME_FC_CHG_SHIFT	3
#define SET_TIME_FC_CHG_WIDTH	3
#define SET_TIME_FC_CHG_MASK	MASK(SET_TIME_FC_CHG_WIDTH, SET_TIME_FC_CHG_SHIFT)

/* S2MU106_CHG_CTRL14 */
#define TOP_OFF_TIME_SHIFT    0
#define TOP_OFF_TIME_WIDTH    3
#define TOP_OFF_TIME_MASK    MASK(TOP_OFF_TIME_WIDTH, TOP_OFF_TIME_SHIFT)

/* S2MU106_CHG_CTRL21 */
#define SET_BAT_OCP_SHIFT    0
#define SET_BAT_OCP_WIDTH    3
#define SET_BAT_OCP_MASK    MASK(SET_BAT_OCP_WIDTH, SET_BAT_OCP_SHIFT)


#define REDUCE_CURRENT_STEP         25
#define MINIMUM_INPUT_CURRENT           300

#define FAKE_BAT_LEVEL          50

ssize_t s2mu106_chg_show_attrs(struct device *dev,
		struct device_attribute *attr, char *buf);

ssize_t s2mu106_chg_store_attrs(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

#define S2MU106_CHARGER_ATTR(_name)				\
{							                    \
	.attr = {.name = #_name, .mode = 0664},	    \
	.show = s2mu106_chg_show_attrs,			    \
	.store = s2mu106_chg_store_attrs,			\
}

enum {
	CHG_REG = 0,
	CHG_DATA,
	CHG_REGS,
};

enum {
	S2MU106_TOPOFF_TIMER_5m		= 0x1,
	S2MU106_TOPOFF_TIMER_10m	= 0x2,
	S2MU106_TOPOFF_TIMER_30m	= 0x3,
	S2MU106_TOPOFF_TIMER_50m	= 0x4,
	S2MU106_TOPOFF_TIMER_70m	= 0x5,
	S2MU106_TOPOFF_TIMER_90m	= 0x6,
	S2MU106_TOPOFF_TIMER_DIS	= 0x7,
};

enum {
	S2MU106_WDT_TIMER_40s	= 0x1,
	S2MU106_WDT_TIMER_50s	= 0x2,
	S2MU106_WDT_TIMER_60s	= 0x3,
	S2MU106_WDT_TIMER_70s	= 0x4,
	S2MU106_WDT_TIMER_80s	= 0x5,
	S2MU106_WDT_TIMER_90s	= 0x6,
	S2MU106_WDT_TIMER_100s	= 0x7,
};

enum {
	S2MU106_FC_CHG_TIMER_4hr	= 0x1,
	S2MU106_FC_CHG_TIMER_6hr	= 0x2,
	S2MU106_FC_CHG_TIMER_8hr	= 0x3,
	S2MU106_FC_CHG_TIMER_10hr	= 0x4,
	S2MU106_FC_CHG_TIMER_12hr	= 0x5,
	S2MU106_FC_CHG_TIMER_14hr	= 0x6,
	S2MU106_FC_CHG_TIMER_16hr	= 0x7,
};

enum {
	S2MU106_SET_OTG_TX_OCP_500mA   = 0x0,
	S2MU106_SET_OTG_TX_OCP_900mA   = 0x1,
	S2MU106_SET_OTG_TX_OCP_1200mA  = 0x2,
	S2MU106_SET_OTG_TX_OCP_1500mA  = 0x3,
};

enum {
	S2MU106_SET_BAT_OCP_3500mA	= 0x0,
	S2MU106_SET_BAT_OCP_4000mA	= 0x1,
	S2MU106_SET_BAT_OCP_4500mA	= 0x2,
	S2MU106_SET_BAT_OCP_5000mA	= 0x3,
	S2MU106_SET_BAT_OCP_5500mA	= 0x4,
	S2MU106_SET_BAT_OCP_6000mA	= 0x5,
	S2MU106_SET_BAT_OCP_6500mA	= 0x6,
	S2MU106_SET_BAT_OCP_7000mA	= 0x7,
};

typedef struct s2mu106_charger_platform_data {
	int chg_float_voltage;
	char *charger_name;
	char *fuelgauge_name;
	char *wireless_charger_name;
	bool chg_eoc_dualpath;
	int recharge_vcell;
	uint32_t is_1MHz_switching:1;
	int chg_switching_freq;
	int wireless_cc_cv;
} s2mu106_charger_platform_data_t;


struct s2mu106_charger_data {
	struct i2c_client       *i2c;
	struct device *dev;
	struct s2mu106_platform_data *s2mu106_pdata;
	struct delayed_work otg_vbus_work;
	struct delayed_work ivr_work;
	struct delayed_work pmeter_3lv_work;
	struct delayed_work pmeter_2lv_work;
	struct wake_lock ivr_wake_lock;
	struct delayed_work wc_current_work;
	struct wake_lock wc_current_wake_lock;

	struct workqueue_struct *charger_wqueue;
	struct power_supply *psy_chg;
	struct power_supply_desc psy_chg_desc;
	struct power_supply *psy_otg;
	struct power_supply_desc psy_otg_desc;
	struct power_supply *psy_bat;
	struct power_supply *psy_fg;

	s2mu106_charger_platform_data_t *pdata;
	int dev_id;
	int input_current;
	int charging_current;
	int topoff_current;
	int cable_type;
	bool is_charging;
	unsigned int charge_mode;
	struct mutex charger_mutex;

	bool ovp;
	bool otg_on;

	int unhealth_cnt;
	int status;
	int health;

	/* s2mu106 */
	int irq_det_bat;
	int irq_chg;
	int irq_chgin;
	int irq_chg_fault;
	int irq_tx;
	int irq_otg;
	int irq_vbus;
	int irq_rst;
	int irq_done;
	int irq_sys;
	int irq_event;
	int irq_bat;
	int irq_ivr;

	int irq_ivr_enabled;
	int ivr_on;
	int wc_current;
	int wc_pre_current;

	/* efficiency 9V charging */
	unsigned char reg_0x9E;

#if defined(CONFIG_MUIC_NOTIFIER)
	struct notifier_block cable_check;
#endif
	bool uno_on;
	struct mutex regmode_mutex;
	u8 read_reg;
};

#endif /*S2MU106_CHARGER_H*/
