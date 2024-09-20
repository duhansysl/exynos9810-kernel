/*
 * s2mu106-private.h - Voltage regulator driver for the s2mu106
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

#ifndef __LINUX_MFD_S2MU106_PRIV_H
#define __LINUX_MFD_S2MU106_PRIV_H

#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define MFD_DEV_NAME "s2mu106"

/* Slave Address for
	Charger
	RGBLED
	Flash LED */
#define I2C_ADDR_PM	(0x3D)

/*
 * Slave Address for the MFD
 * includes :
 * MUIC, AFC, PM, MST.
 * 1 bit right-shifted.
 */
#define I2C_ADDR_7C_SLAVE	((0x7C) >> 1)

/* Slave Address for
	Haptic
	Haptic Boost */
#define I2C_ADDR_HAPTIC	(0x3A)

/* Master Register Addr */
#define S2MU106_REG_IPINT		0x00
#define S2MU106_REG_IPINT_MASK		0x07
#define S2MU106_REG_PMICID		0xF5
#define S2MU106_REG_PMICID_MASK		0x0F
#define S2MU106_REG_INVALID 		0xFF

/* IRQ */
enum s2mu106_irq_source {
#if defined(CONFIG_CHARGER_S2MU106)
	CHG_INT1,
	CHG_INT2,
	CHG_INT3,
#endif
#if defined(CONFIG_LEDS_S2MU106_FLASH)
	FLED_INT1,
	FLED_INT2,
#endif
#if defined(CONFIG_HV_MUIC_S2MU106_AFC)
	AFC_INT,
#endif
#if defined(CONFIG_MUIC_S2MU106)
	MUIC_INT1,
	MUIC_INT2,
#endif
#if defined(CONFIG_PM_S2MU106)
	PM_VALUP1,
	PM_VALUP2,
	PM_INT1,
	PM_INT2,
#endif
#if defined(CONFIG_MST_S2MU106)
	MST_INT,
#endif
#if defined(CONFIG_MOTOR_S2MU106)
	HAPTIC_INT,
#endif
#if defined(CONFIG_REGULATOR_S2MU106)
	HBST_INT,
#endif
	S2MU106_IRQ_GROUP_NR,
};

#define S2MU106_NUM_IRQ_CHG_REGS	3
#define S2MU106_NUM_IRQ_LED_REGS	2
#define S2MU106_NUM_IRQ_AFC_REGS	1
#define S2MU106_NUM_IRQ_MUIC_REGS	2
#define S2MU106_NUM_IRQ_PM_REGS		4
#define S2MU106_NUM_IRQ_MST_REGS	1
#define S2MU106_NUM_IRQ_HAPTIC_REGS	1
#define S2MU106_NUM_IRQ_HBST_REGS	1

#define S2MU106_IRQSRC_PM	(1 << 0)
#define S2MU106_IRQSRC_AFC	(1 << 1)
#define S2MU106_IRQSRC_MUIC	(1 << 2)
#define S2MU106_IRQSRC_MST	(1 << 3)
#define S2MU106_IRQSRC_HBST	(1 << 4)
#define S2MU106_IRQSRC_FLED	(1 << 5)
#define S2MU106_IRQSRC_CHG	(1 << 6)
#define S2MU106_IRQSRC_HAPTIC	(1 << 7)

enum s2mu106_irq {
#if defined(CONFIG_CHARGER_S2MU106)
	S2MU106_CHG1_IRQ_SYS,
	S2MU106_CHG1_IRQ_CV,
	S2MU106_CHG1_IRQ_CHG_Fault,
	S2MU106_CHG1_IRQ_CHG_RSTART,
	S2MU106_CHG1_IRQ_DONE,
	S2MU106_CHG1_IRQ_TOP_OFF,
	S2MU106_CHG1_IRQ_WCIN,
	S2MU106_CHG1_IRQ_CHGIN,

	S2MU106_CHG2_IRQ_ICR,
	S2MU106_CHG2_IRQ_IVR,
	S2MU106_CHG2_IRQ_AICL,
	S2MU106_CHG2_IRQ_DET_BAT,
	S2MU106_CHG2_IRQ_BAT,
	S2MU106_CHG2_IRQ_BATN_OPEN,
	S2MU106_CHG2_IRQ_BATP_OPEN,
	S2MU106_CHG2_IRQ_QBAT_OFF,

	S2MU106_CHG3_IRQ_BST,
	S2MU106_CHG3_IRQ_TX,
	S2MU106_CHG3_IRQ_OTG,
	S2MU106_CHG3_IRQ_VBUS_Short,
	S2MU106_CHG3_IRQ_MaxDuty,
	S2MU106_CHG3_IRQ_PEdone,
	S2MU106_CHG3_IRQ_BAT_Contact_CK_Done,
#endif
#if defined(CONFIG_LEDS_S2MU106_FLASH)
	S2MU106_FLED1_IRQ_C2F_Vbyp_ovp_prot,
	S2MU106_FLED1_IRQ_C2F_Vbyp_OK_Warning,
	S2MU106_FLED1_IRQ_OPEN_CH3,
	S2MU106_FLED1_IRQ_OPEN_CH2,
	S2MU106_FLED1_IRQ_OPEN_CH1,
	S2MU106_FLED1_IRQ_SHORT_CH3,
	S2MU106_FLED1_IRQ_SHORT_CH2,
	S2MU106_FLED1_IRQ_SHORT_CH1,

	S2MU106_FLED2_IRQ_CH3_ON,
	S2MU106_FLED2_IRQ_CH2_ON,
	S2MU106_FLED2_IRQ_CH1_ON,
	S2MU106_FLED2_IRQ_TORCH_ON,
	S2MU106_FLED2_IRQ_LED_ON_TA_Detach,
#endif
#if defined(CONFIG_HV_MUIC_S2MU106_AFC)
	S2MU106_AFC_IRQ_VbADC,
	S2MU106_AFC_IRQ_VDNMon,
	S2MU106_AFC_IRQ_DNRes,
	S2MU106_AFC_IRQ_MPNack,
	S2MU106_AFC_IRQ_MRxBufOw,
	S2MU106_AFC_IRQ_MRxTrf,
	S2MU106_AFC_IRQ_MRxPerr,
	S2MU106_AFC_IRQ_MRxRdy,
#endif
#if defined(CONFIG_MUIC_S2MU106)
	S2MU106_MUIC_IRQ1_DETACH,
	S2MU106_MUIC_IRQ1_ATTATCH,
	S2MU106_MUIC_IRQ1_KP,
	S2MU106_MUIC_IRQ1_LKP,
	S2MU106_MUIC_IRQ1_LKR,
	S2MU106_MUIC_IRQ1_RID_CHG,
	S2MU106_MUIC_IRQ1_USB_Killer,
	S2MU106_MUIC_IRQ1_WAKE_UP,

	S2MU106_MUIC_IRQ2_VBUS_ON,
	S2MU106_MUIC_IRQ2_RSVD_ATTACH,
	S2MU106_MUIC_IRQ2_ADC_CHANGE,
	S2MU106_MUIC_IRQ2_STUCK,
	S2MU106_MUIC_IRQ2_STUCKRCV,
	S2MU106_MUIC_IRQ2_MHDL,
	S2MU106_MUIC_IRQ2_AV_CHARGE,
	S2MU106_MUIC_IRQ2_VBUS_OFF,
#endif
#if defined(CONFIG_PM_S2MU106)
	S2MU106_PM_VALUP1_VCC2UP,
	S2MU106_PM_VALUP1_VGPADCUP,
	S2MU106_PM_VALUP1_VCC1UP,
	S2MU106_PM_VALUP1_VBATAUP,
	S2MU106_PM_VALUP1_VSYSAUP,
	S2MU106_PM_VALUP1_VBYPUP,
	S2MU106_PM_VALUP1_VWCINUP,
	S2MU106_PM_VALUP1_VCHGINUP,

	S2MU106_PM_VALUP2_ITXUP,
	S2MU106_PM_VALUP2_IOTGUP,
	S2MU106_PM_VALUP2_IWCINUP,
	S2MU106_PM_VALUP2_ICHGINUP,

	S2MU106_PM_IRQ1_VCC2UP,
	S2MU106_PM_IRQ1_VGPADCUP,
	S2MU106_PM_IRQ1_VCC1UP,
	S2MU106_PM_IRQ1_VBATAUP,
	S2MU106_PM_IRQ1_VSYSAUP,
	S2MU106_PM_IRQ1_VBYPUP,
	S2MU106_PM_IRQ1_VWCINUP,
	S2MU106_PM_IRQ1_VCHGINUP,

	S2MU106_PM_IRQ2_ITXUP,
	S2MU106_PM_IRQ2_IOTGUP,
	S2MU106_PM_IRQ2_IWCINUP,
	S2MU106_PM_IRQ2_ICHGINUP,
#endif
#if defined(CONFIG_MST_S2MU106)
	S2MU106_MST_IRQ_SHORT,
	S2MU106_MST_IRQ_OCP,
	S2MU106_MST_IRQ_EN_OVL,
	S2MU106_MST_IRQ_DONE,
	S2MU106_MST_IRQ_EN,
#endif
#if defined(CONFIG_MOTOR_S2MU106)
	S2MU106_HAPTIC_IRQ_OCP,
#endif
#if defined(CONFIG_REGULATOR_S2MU106)
	S2MU106_HBST_IRQ_OFF,
	S2MU106_HBST_IRQ_ON,
	S2MU106_HBST_IRQ_SCP,
#endif
	S2MU106_IRQ_NR,
};

struct s2mu106_platform_data {
	/* IRQ */
	int irq_base;
	int irq_gpio;
	bool wakeup;
};

struct s2mu106_dev {
	struct device *dev;
	struct i2c_client *i2c;		/* Slave addr = 0x3D */
	struct i2c_client *muic;	/* Slave addr = 0x3E */
	struct i2c_client *haptic;	/* Slave addr = 0x3A */
	struct mutex i2c_lock;

	int type;

	int irq;
	int irq_base;
	int irq_gpio;
	bool wakeup;
	bool change_irq_mask;
	struct mutex irqlock;
	int irq_masks_cur[S2MU106_IRQ_GROUP_NR];
	int irq_masks_cache[S2MU106_IRQ_GROUP_NR];

	/* pmic VER/REV register */
	u8 pmic_rev;	/* pmic Rev */
	u8 pmic_ver;	/* pmic version */

	struct s2mu106_platform_data *pdata;
};

enum s2mu106_types {
	TYPE_S2MU106,
};

extern int s2mu106_irq_init(struct s2mu106_dev *s2mu106);
extern void s2mu106_irq_exit(struct s2mu106_dev *s2mu106);

/* s2mu106 shared i2c API function */
extern int s2mu106_read_reg(struct i2c_client *i2c, u8 reg, u8 *dest);
extern int s2mu106_bulk_read(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int s2mu106_write_reg(struct i2c_client *i2c, u8 reg, u8 value);
extern int s2mu106_bulk_write(struct i2c_client *i2c, u8 reg, int count,
				u8 *buf);
extern int s2mu106_write_word(struct i2c_client *i2c, u8 reg, u16 value);
extern int s2mu106_read_word(struct i2c_client *i2c, u8 reg);

extern int s2mu106_update_reg(struct i2c_client *i2c, u8 reg, u8 val, u8 mask);

#endif /* __LINUX_MFD_S2MU106_PRIV_H */
