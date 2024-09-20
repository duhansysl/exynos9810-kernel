/*
 * s2mu107-private.h - Voltage regulator driver for the s2mu107
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

#ifndef __LINUX_MFD_S2MU107_PRIV_H
#define __LINUX_MFD_S2MU107_PRIV_H

#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define MFD_DEV_NAME "s2mu107"
#define MASK(width, shift)              (((0x1 << (width)) - 1) << shift)

/*
 * Slave Address for the MFD
 * includes :
 * FLED.
 * 1 bit right-shifted.
 */

#define I2C_ADDR_74_SLAVE		((0x74) >> 1)

/*
 * Slave Address for the MFD
 * includes :
 * AFC, MUIC, PM.
 * 1 bit right-shifted.
 */
#define I2C_ADDR_7C_SLAVE		((0x7C) >> 1)

/*
 * Slave Address for
 * Charger, Direct Charger.
 */
#define I2C_ADDR_7A_SLAVE		((0x7A) >> 1)

/* Master Register Addr */
#define S2MU107_REG_IPINT		0x00
#define S2MU107_REG_IPINT_MASK		MASK(6,0)
#define S2MU107_REG_TOPINT		0x01
#define S2MU107_REG_TOPINT_MASK		MASK(1,0)

#define S2MU107_REG_ESREV_NUM		0xF5
#define S2MU107_REG_REV_MASK		MASK(4,0)
#define S2MU107_REG_ES_MASK		MASK(4,4)
#define S2MU107_REG_INVALID 		0xFF

/* IRQ */
enum s2mu107_irq_source {
	FLED_INT1,
	FLED_INT2,

	SC_INT1,
	SC_INT2,
	SC_INT3,

	DC_INT0,
	DC_INT1,
	DC_INT2,
	DC_INT3,

	AFC_INT,

	MUIC_INT1,
	MUIC_INT2,

	PM_VALUP1,
	PM_VALUP2,
	PM_INT1,
	PM_INT2,

	S2MU107_IRQ_GROUP_NR,
};

#define S2MU107_NUM_IRQ_LED_REGS	2
#define S2MU107_NUM_IRQ_SC_REGS		3
#define S2MU107_NUM_IRQ_DC_REGS		4
#define S2MU107_NUM_IRQ_AFC_REGS	1
#define S2MU107_NUM_IRQ_MUIC_REGS	2
#define S2MU107_NUM_IRQ_PM_REGS		4

#define S2MU107_IRQSRC_MUIC		(1 << 0)
#define S2MU107_IRQSRC_FLED		(1 << 1)
#define S2MU107_IRQSRC_SC		(1 << 2)
#define S2MU107_IRQSRC_AFC		(1 << 3)
#define S2MU107_IRQSRC_PM		(1 << 4)
#define S2MU107_IRQSRC_DC		(1 << 5)

enum s2mu107_irq {
#if defined(CONFIG_LEDS_S2MU107_FLASH)
	S2MU107_FLED_IRQ1_C2F_VBYP_OVP_PROT,
	S2MU107_FLED_IRQ1_C2F_VBYP_OK_WARNING,
	S2MU107_FLED_IRQ1_SW_OFF_TORCH_ON,
	S2MU107_FLED_IRQ1_FLED_ON_TA_DISCONNECT,
	S2MU107_FLED_IRQ1_CH1_LED_ON_DONE,
	S2MU107_FLED_IRQ1_FLED_OPEN_CH1_I,
	S2MU107_FLED_IRQ1_FLED_SHORT_CH1_I,

	S2MU107_FLED_IRQ2_TORCH_DC_INT,
	S2MU107_FLED_IRQ2_FLASH_DC_INT,
#endif
#if defined(CONFIG_CHARGER_S2MU107)
	S2MU107_SC_IRQ1_SYS,
	S2MU107_SC_IRQ1_CV,
	S2MU107_SC_IRQ1_CHG_Fault,
	S2MU107_SC_IRQ1_CHG_Restart,
	S2MU107_SC_IRQ1_DONE,
	S2MU107_SC_IRQ1_TOP_OFF,
	S2MU107_SC_IRQ1_WCIN,
	S2MU107_SC_IRQ1_CHGIN,

	S2MU107_SC_IRQ2_ICR,
	S2MU107_SC_IRQ2_IVR,
	S2MU107_SC_IRQ2_AICL,
	S2MU107_SC_IRQ2_VBUS_Short,
	S2MU107_SC_IRQ2_BST,
	S2MU107_SC_IRQ2_OTG,
	S2MU107_SC_IRQ2_BAT,
	S2MU107_SC_IRQ2_DET_VBUS,

	S2MU107_SC_IRQ3_BATID,
	S2MU107_SC_IRQ3_BATID_DONE,
	S2MU107_SC_IRQ3_QBAT_OFF,
	S2MU107_SC_IRQ3_BATN_OPEN,
	S2MU107_SC_IRQ3_BATP_OPEN,
	S2MU107_SC_IRQ3_BAT_CONTACT_CK_DONE,
#endif
#if defined(CONFIG_CHARGER_S2MU107_DIRECT)
	S2MU107_DC_IRQ0_DC_OUT_OVP,
	S2MU107_DC_IRQ0_DC_BAT_OKB,
	S2MU107_DC_IRQ0_DC_BYP_OVP,
	S2MU107_DC_IRQ0_DC_BYP2OUT_OVP,
	S2MU107_DC_IRQ0_DC_CHGIN_OKB,
	S2MU107_DC_IRQ0_DC_WCIN_OKB,
	S2MU107_DC_IRQ0_DC_NORMAL_CHARGING,
	S2MU107_DC_IRQ0_DC_RAMP_FAIL,

	S2MU107_DC_IRQ1_DC_CHGIN_DIODE_PROT,
	S2MU107_DC_IRQ1_DC_WCIN_DIODE_PROT,
	S2MU107_DC_IRQ1_DC_CHGIN_PLUG_OUT,
	S2MU107_DC_IRQ1_DC_OFF,
	S2MU107_DC_IRQ1_DC_CHGIN_RCP,
	S2MU107_DC_IRQ1_DC_WCIN_RCP,
	S2MU107_DC_IRQ1_DC_OCP,

	S2MU107_DC_IRQ2_DC_CHGIN_ICL,
	S2MU107_DC_IRQ2_DC_WCIN_ICL,
	S2MU107_DC_IRQ2_DC_CV,
	S2MU107_DC_IRQ2_DC_SCP,
	S2MU107_DC_IRQ2_DC_THERMAL,
	S2MU107_DC_IRQ2_DC_LONG_CC,
	S2MU107_DC_IRQ2_DC_PPS_FAIL,
	S2MU107_DC_IRQ2_DC_DONE,

	S2MU107_DC_IRQ3_DC_TFB,
	S2MU107_DC_IRQ3_DC_TSD,
	S2MU107_DC_IRQ3_DC_WCIN_DOWN,
	S2MU107_DC_IRQ3_DC_WCIN_UP,
	S2MU107_DC_IRQ3_DC_PM_OFF,
	S2MU107_DC_IRQ3_DC_SC_OFF,
#endif
#if defined(CONFIG_HV_MUIC_S2MU107_AFC)
	S2MU107_AFC_IRQ_VbADC,
	S2MU107_AFC_IRQ_VDNMon,
	S2MU107_AFC_IRQ_DNRes,
	S2MU107_AFC_IRQ_MPNack,
	S2MU107_AFC_IRQ_MRxBufOw,
	S2MU107_AFC_IRQ_MRxTrf,
	S2MU107_AFC_IRQ_MRxPerr,
	S2MU107_AFC_IRQ_MRxRdy,
#endif
#if defined(CONFIG_MUIC_S2MU107)
	S2MU107_MUIC_IRQ1_DETACH, /* TODO: check */
	S2MU107_MUIC_IRQ1_ATTATCH,
	S2MU107_MUIC_IRQ1_KP,
	S2MU107_MUIC_IRQ1_LKP,
	S2MU107_MUIC_IRQ1_LKR,
	S2MU107_MUIC_IRQ1_RID_CHG,
	S2MU107_MUIC_IRQ1_USB_KILLER,
	S2MU107_MUIC_IRQ1_WAKE_UP,

	S2MU107_MUIC_IRQ2_VBUS_ON,
	S2MU107_MUIC_IRQ2_RSVD_ATTACH,
	S2MU107_MUIC_IRQ2_ADC_CHANGE,
	S2MU107_MUIC_IRQ2_STUCK,
	S2MU107_MUIC_IRQ2_STUCKRCV,
	S2MU107_MUIC_IRQ2_MHDL,
	S2MU107_MUIC_IRQ2_AV_CHARGE,
	S2MU107_MUIC_IRQ2_VBUS_OFF,
#endif
#if defined(CONFIG_PM_S2MU107)
	S2MU107_PM_VALUP1_TDIEUP,
	S2MU107_PM_VALUP1_VCC2UP,
	S2MU107_PM_VALUP1_VCC1UP,
	S2MU107_PM_VALUP1_VBATUP,
	S2MU107_PM_VALUP1_VSYSUP,
	S2MU107_PM_VALUP1_VBYPUP,
	S2MU107_PM_VALUP1_VWCINUP,
	S2MU107_PM_VALUP1_VCHGINUP,

	S2MU107_PM_VALUP2_ITXUP,
	S2MU107_PM_VALUP2_IOTGUP,
	S2MU107_PM_VALUP2_IWCINUP,
	S2MU107_PM_VALUP2_ICHGINUP,

	S2MU107_PM_IRQ1_TDIEI,
	S2MU107_PM_IRQ1_VCC2I,
	S2MU107_PM_IRQ1_VCC1I,
	S2MU107_PM_IRQ1_VBATI,
	S2MU107_PM_IRQ1_VSYSI,
	S2MU107_PM_IRQ1_VBYPI,
	S2MU107_PM_IRQ1_VWCINI,
	S2MU107_PM_IRQ1_VCHGINI,

	S2MU107_PM_IRQ2_IWCIN_TH,
	S2MU107_PM_IRQ2_ICHGIN_TH,
	S2MU107_PM_IRQ2_GPADCI,
	S2MU107_PM_IRQ2_ITXI,
	S2MU107_PM_IRQ2_IOTGI,
	S2MU107_PM_IRQ2_IWCINI,
	S2MU107_PM_IRQ2_ICHGINI,
#endif
	S2MU107_IRQ_NR,
};

struct s2mu107_platform_data {
	/* IRQ */
	int irq_base;
	int irq_gpio;
	bool wakeup;
};

struct s2mu107_dev {
	struct device *dev;
	struct i2c_client *i2c;		/* Slave addr = 0x3A */
	struct i2c_client *muic;	/* Slave addr = 0x3E */
	struct i2c_client *chg;		/* Slave addr = 0x3D */
	struct mutex i2c_lock;

	int type;

	int irq;
	int irq_base;
	int irq_gpio;
	bool wakeup;
	bool change_irq_mask;
	struct mutex irqlock;
	int irq_masks_cur[S2MU107_IRQ_GROUP_NR];
	int irq_masks_cache[S2MU107_IRQ_GROUP_NR];

	/* pmic REV/ES register */
	u8 pmic_rev;	/* pmic Rev */
	u8 pmic_es;	/* pmic ES */

	struct s2mu107_platform_data *pdata;
};

enum s2mu107_types {
	TYPE_S2MU107,
};

extern int s2mu107_irq_init(struct s2mu107_dev *s2mu107);
extern void s2mu107_irq_exit(struct s2mu107_dev *s2mu107);

/* s2mu107 shared i2c API function */
extern int s2mu107_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int s2mu107_bulk_read(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int s2mu107_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int s2mu107_bulk_write(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int s2mu107_write_word(struct i2c_client *i2c, u8 reg, u16 value);
extern int s2mu107_read_word(struct i2c_client *i2c, u8 reg);

extern int s2mu107_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

#endif /* __LINUX_MFD_S2MU107_PRIV_H */
