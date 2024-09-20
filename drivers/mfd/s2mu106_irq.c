/*
 * s2mu106-irq.c - Interrupt controller support for s2mu106
 *
 * Copyright (C) 2018 Samsung Electronics Co.Ltd
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/mfd/samsung/s2mu106.h>
#include <linux/err.h>

/* TODO : add IP Header file include*/
#if defined(CONFIG_CHARGER_S2MU106)
#include "../battery_v2/include/charger/s2mu106_charger.h"
#endif
#if defined(CONFIG_MUIC_S2MU106)
#include <linux/muic/s2mu106-muic.h>
#endif
#if defined(CONFIG_PM_S2MU106)
#include "../battery_v2/include/s2mu106_pmeter.h"
#endif
#if defined(CONFIG_LEDS_S2MU106_FLASH)
#include <linux/leds-s2mu106.h>
#endif
#if defined(CONFIG_MOTOR_S2MU106)
#include <linux/s2mu106_haptic.h>
#endif
static const u8 s2mu106_mask_reg[] = {
#if defined(CONFIG_CHARGER_S2MU106)
	[CHG_INT1] = S2MU106_CHG_INT1M,
	[CHG_INT2] = S2MU106_CHG_INT2M,
	[CHG_INT3] = S2MU106_CHG_INT3M,
#endif
#if defined(CONFIG_LEDS_S2MU106_FLASH)
	[FLED_INT1] = S2MU106_FLED_INT1_MASK,
	[FLED_INT2] = S2MU106_FLED_INT2_MASK,
#endif
#if defined(CONFIG_HV_MUIC_S2MU106_AFC)
	[AFC_INT] = S2MU106_REG_AFC_INT,
#endif
#if defined(CONFIG_MUIC_S2MU106)
	[MUIC_INT1] = S2MU106_REG_MUIC_INT1,
	[MUIC_INT2] = S2MU106_REG_MUIC_INT2,
#endif
#if defined(CONFIG_PM_S2MU106)
	[PM_VALUP1] = S2MU106_PM_VALUP1_MASK,
	[PM_VALUP2] = S2MU106_PM_VALUP2_MASK,
	[PM_INT1] = S2MU106_PM_INT1_MASK,
	[PM_INT2] = S2MU106_PM_INT2_MASK,
#endif
#if defined(CONFIG_MOTOR_S2MU106)
	[HAPTIC_INT] = S2MU106_REG_HAPTIC_INT_MASK,
#endif
};

struct s2mu106_irq_data {
	int mask;
	enum s2mu106_irq_source group;
};

static struct i2c_client *get_i2c(struct s2mu106_dev *s2mu106,
					enum s2mu106_irq_source src)
{
	switch (src) {
#if defined(CONFIG_CHARGER_S2MU106)
	case CHG_INT1 ... CHG_INT3:
		return s2mu106->i2c;
#endif
#if defined(CONFIG_LEDS_S2MU106_FLASH)
	case FLED_INT1 ... FLED_INT2:
		return s2mu106->i2c;
#endif
#if defined(CONFIG_HV_MUIC_S2MU106_AFC)
	case AFC_INT:
		return s2mu106->muic;
#endif
#if defined(CONFIG_MUIC_S2MU106)
	case MUIC_INT1 ... MUIC_INT2:
		return s2mu106->muic;
#endif
#if defined(CONFIG_PM_S2MU106)
	case PM_VALUP1 ... PM_INT2:
		return s2mu106->muic;
#endif
#if defined(CONFIG_MST_S2MU106)
	case MST_INT:
		return s2mu106->muic;
#endif
#if defined(CONFIG_MOTOR_S2MU106)
	case HAPTIC_INT:
		return s2mu106->haptic;
#endif
#if defined(CONFIG_REGULATOR_S2MU106)
	case HBST_INT:
		return s2mu106->haptic;
#endif
	default:
		return ERR_PTR(-EINVAL);
	}
}

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }
static const struct s2mu106_irq_data s2mu106_irqs[] = {
#if defined(CONFIG_CHARGER_S2MU106)
	DECLARE_IRQ(S2MU106_CHG1_IRQ_SYS,	CHG_INT1,	1 << 0),
	DECLARE_IRQ(S2MU106_CHG1_IRQ_CV,	CHG_INT1,	1 << 1),
	DECLARE_IRQ(S2MU106_CHG1_IRQ_CHG_Fault,	CHG_INT1,	1 << 2),
	DECLARE_IRQ(S2MU106_CHG1_IRQ_CHG_RSTART, CHG_INT1,	1 << 3),
	DECLARE_IRQ(S2MU106_CHG1_IRQ_DONE,	CHG_INT1,	1 << 4),
	DECLARE_IRQ(S2MU106_CHG1_IRQ_TOP_OFF,	CHG_INT1,	1 << 5),
	DECLARE_IRQ(S2MU106_CHG1_IRQ_WCIN,	CHG_INT1,	1 << 6),
	DECLARE_IRQ(S2MU106_CHG1_IRQ_CHGIN,	CHG_INT1,	1 << 7),

	DECLARE_IRQ(S2MU106_CHG2_IRQ_ICR,	CHG_INT2,	1 << 0),
	DECLARE_IRQ(S2MU106_CHG2_IRQ_IVR,	CHG_INT2,	1 << 1),
	DECLARE_IRQ(S2MU106_CHG2_IRQ_AICL,	CHG_INT2,	1 << 2),
	DECLARE_IRQ(S2MU106_CHG2_IRQ_DET_BAT,	CHG_INT2,	1 << 3),
	DECLARE_IRQ(S2MU106_CHG2_IRQ_BAT,	CHG_INT2,	1 << 4),
	DECLARE_IRQ(S2MU106_CHG2_IRQ_BATN_OPEN,	CHG_INT2,	1 << 5),
	DECLARE_IRQ(S2MU106_CHG2_IRQ_BATP_OPEN,	CHG_INT2,	1 << 6),
	DECLARE_IRQ(S2MU106_CHG2_IRQ_QBAT_OFF,	CHG_INT2,	1 << 7),

	DECLARE_IRQ(S2MU106_CHG3_IRQ_BST,	CHG_INT3,	1 << 0),
	DECLARE_IRQ(S2MU106_CHG3_IRQ_TX,	CHG_INT3,	1 << 1),
	DECLARE_IRQ(S2MU106_CHG3_IRQ_OTG,	CHG_INT3,	1 << 2),
	DECLARE_IRQ(S2MU106_CHG3_IRQ_VBUS_Short, CHG_INT3,	1 << 3),
	DECLARE_IRQ(S2MU106_CHG3_IRQ_MaxDuty,	CHG_INT3,	1 << 4),
	DECLARE_IRQ(S2MU106_CHG3_IRQ_PEdone,	CHG_INT3,	1 << 5),
	DECLARE_IRQ(S2MU106_CHG3_IRQ_BAT_Contact_CK_Done, CHG_INT3, 1 << 6),
#endif
#if defined(CONFIG_LEDS_S2MU106_FLASH)
	DECLARE_IRQ(S2MU106_FLED1_IRQ_C2F_Vbyp_ovp_prot, FLED_INT1, 1 << 0),
	DECLARE_IRQ(S2MU106_FLED1_IRQ_C2F_Vbyp_OK_Warning, FLED_INT1, 1 << 1),
	DECLARE_IRQ(S2MU106_FLED1_IRQ_OPEN_CH3,	FLED_INT1,	1 << 2),
	DECLARE_IRQ(S2MU106_FLED1_IRQ_OPEN_CH2,	FLED_INT1,	1 << 3),
	DECLARE_IRQ(S2MU106_FLED1_IRQ_OPEN_CH1,	FLED_INT1,	1 << 4),
	DECLARE_IRQ(S2MU106_FLED1_IRQ_SHORT_CH3, FLED_INT1,	1 << 5),
	DECLARE_IRQ(S2MU106_FLED1_IRQ_SHORT_CH2, FLED_INT1,	1 << 6),
	DECLARE_IRQ(S2MU106_FLED1_IRQ_SHORT_CH1, FLED_INT1,	1 << 7),

	DECLARE_IRQ(S2MU106_FLED2_IRQ_CH3_ON,	FLED_INT2,	1 << 2),
	DECLARE_IRQ(S2MU106_FLED2_IRQ_CH2_ON,	FLED_INT2,	1 << 3),
	DECLARE_IRQ(S2MU106_FLED2_IRQ_CH1_ON,	FLED_INT2,	1 << 4),
	DECLARE_IRQ(S2MU106_FLED2_IRQ_TORCH_ON,	FLED_INT2,	1 << 6),
	DECLARE_IRQ(S2MU106_FLED2_IRQ_LED_ON_TA_Detach,	FLED_INT2, 1 << 7),
#endif
#if defined(CONFIG_HV_MUIC_S2MU106_AFC)
	DECLARE_IRQ(S2MU106_AFC_IRQ_VbADC,	AFC_INT,	1 << 0),
	DECLARE_IRQ(S2MU106_AFC_IRQ_VDNMon,	AFC_INT,	1 << 1),
	DECLARE_IRQ(S2MU106_AFC_IRQ_DNRes,	AFC_INT,	1 << 2),
	DECLARE_IRQ(S2MU106_AFC_IRQ_MPNack,	AFC_INT,	1 << 3),
	DECLARE_IRQ(S2MU106_AFC_IRQ_MRxTrf,	AFC_INT,	1 << 5),
	DECLARE_IRQ(S2MU106_AFC_IRQ_MRxPerr,	AFC_INT,	1 << 6),
	DECLARE_IRQ(S2MU106_AFC_IRQ_MRxRdy,	AFC_INT,	1 << 7),
#endif
#if defined(CONFIG_MUIC_S2MU106)
	DECLARE_IRQ(S2MU106_MUIC_IRQ1_DETACH,	MUIC_INT1,	1 << 1),
	DECLARE_IRQ(S2MU106_MUIC_IRQ1_ATTATCH,	MUIC_INT1,	1 << 0),
	DECLARE_IRQ(S2MU106_MUIC_IRQ1_KP,	MUIC_INT1,	1 << 2),
	DECLARE_IRQ(S2MU106_MUIC_IRQ1_LKP,	MUIC_INT1,	1 << 3),
	DECLARE_IRQ(S2MU106_MUIC_IRQ1_LKR,	MUIC_INT1,	1 << 4),
	DECLARE_IRQ(S2MU106_MUIC_IRQ1_RID_CHG,	MUIC_INT1,	1 << 5),
	DECLARE_IRQ(S2MU106_MUIC_IRQ1_USB_Killer,	MUIC_INT1,	1 << 6),

	DECLARE_IRQ(S2MU106_MUIC_IRQ2_VBUS_ON,		MUIC_INT2, 1 << 0),
	DECLARE_IRQ(S2MU106_MUIC_IRQ2_RSVD_ATTACH,	MUIC_INT2, 1 << 1),
	DECLARE_IRQ(S2MU106_MUIC_IRQ2_ADC_CHANGE,	MUIC_INT2, 1 << 2),
	DECLARE_IRQ(S2MU106_MUIC_IRQ2_STUCK,		MUIC_INT2, 1 << 3),
	DECLARE_IRQ(S2MU106_MUIC_IRQ2_STUCKRCV,		MUIC_INT2, 1 << 4),
	DECLARE_IRQ(S2MU106_MUIC_IRQ2_MHDL,		MUIC_INT2, 1 << 5),
	DECLARE_IRQ(S2MU106_MUIC_IRQ2_AV_CHARGE,	MUIC_INT2, 1 << 6),
	DECLARE_IRQ(S2MU106_MUIC_IRQ2_VBUS_OFF,		MUIC_INT2, 1 << 7),
#endif
#if defined(CONFIG_PM_S2MU106)
	DECLARE_IRQ(S2MU106_PM_VALUP1_VCC2UP,	PM_VALUP1,	1 << 0),
	DECLARE_IRQ(S2MU106_PM_VALUP1_VGPADCUP,	PM_VALUP1,	1 << 1),
	DECLARE_IRQ(S2MU106_PM_VALUP1_VCC1UP,	PM_VALUP1,	1 << 2),
	DECLARE_IRQ(S2MU106_PM_VALUP1_VBATAUP,	PM_VALUP1,	1 << 3),
	DECLARE_IRQ(S2MU106_PM_VALUP1_VSYSAUP,	PM_VALUP1,	1 << 4),
	DECLARE_IRQ(S2MU106_PM_VALUP1_VBYPUP,	PM_VALUP1,	1 << 5),
	DECLARE_IRQ(S2MU106_PM_VALUP1_VWCINUP,	PM_VALUP1,	1 << 6),
	DECLARE_IRQ(S2MU106_PM_VALUP1_VCHGINUP,	PM_VALUP1,	1 << 7),

	DECLARE_IRQ(S2MU106_PM_VALUP2_ITXUP,	PM_VALUP2,	1 << 3),
	DECLARE_IRQ(S2MU106_PM_VALUP2_IOTGUP,	PM_VALUP2,	1 << 4),
	DECLARE_IRQ(S2MU106_PM_VALUP2_IWCINUP,	PM_VALUP2,	1 << 6),
	DECLARE_IRQ(S2MU106_PM_VALUP2_ICHGINUP,	PM_VALUP2,	1 << 7),

	DECLARE_IRQ(S2MU106_PM_IRQ1_VCC2UP,	PM_INT1,	1 << 0),
	DECLARE_IRQ(S2MU106_PM_IRQ1_VGPADCUP,	PM_INT1,	1 << 1),
	DECLARE_IRQ(S2MU106_PM_IRQ1_VCC1UP,	PM_INT1,	1 << 2),
	DECLARE_IRQ(S2MU106_PM_IRQ1_VBATAUP,	PM_INT1,	1 << 3),
	DECLARE_IRQ(S2MU106_PM_IRQ1_VSYSAUP,	PM_INT1,	1 << 4),
	DECLARE_IRQ(S2MU106_PM_IRQ1_VBYPUP,	PM_INT1,	1 << 5),
	DECLARE_IRQ(S2MU106_PM_IRQ1_VWCINUP,	PM_INT1,	1 << 6),
	DECLARE_IRQ(S2MU106_PM_IRQ1_VCHGINUP,	PM_INT1,	1 << 7),

	DECLARE_IRQ(S2MU106_PM_IRQ2_ITXUP,	PM_INT2,	1 << 3),
	DECLARE_IRQ(S2MU106_PM_IRQ2_IOTGUP,	PM_INT2,	1 << 4),
	DECLARE_IRQ(S2MU106_PM_IRQ2_IWCINUP,	PM_INT2,	1 << 6),
	DECLARE_IRQ(S2MU106_PM_IRQ2_ICHGINUP,	PM_INT2,	1 << 7),
#endif
#if defined(CONFIG_MST_S2MU106)
	DECLARE_IRQ(S2MU106_MST_IRQ_SHORT,	MST_INT,	1 << 3),
	DECLARE_IRQ(S2MU106_MST_IRQ_OCP,	MST_INT,	1 << 4),
	DECLARE_IRQ(S2MU106_MST_IRQ_EN_OVL,	MST_INT,	1 << 5),
	DECLARE_IRQ(S2MU106_MST_IRQ_DONE,	MST_INT,	1 << 6),
	DECLARE_IRQ(S2MU106_MST_IRQ_EN,		MST_INT,	1 << 7),
#endif
#if defined(CONFIG_MOTOR_S2MU106)
	DECLARE_IRQ(S2MU106_HAPTIC_IRQ_OCP,	HAPTIC_INT,	1 << 0),
#endif
#if defined(CONFIG_REGULATOR_S2MU106)
	DECLARE_IRQ(S2MU106_HBST_IRQ_OFF,	HBST_INT	1 << 0),
	DECLARE_IRQ(S2MU106_HBST_IRQ_ON,	HBST_INT	1 << 1),
	DECLARE_IRQ(S2MU106_HBST_IRQ_SCP,	HBST_INT	1 << 2),
#endif
};

static void s2mu106_irq_lock(struct irq_data *data)
{
	struct s2mu106_dev *s2mu106 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mu106->irqlock);
}

static void s2mu106_irq_sync_unlock(struct irq_data *data)
{
	struct s2mu106_dev *s2mu106 = irq_get_chip_data(data->irq);
	int i;
	u8 mask_reg;
	struct i2c_client *i2c;

	if (s2mu106->change_irq_mask == false)
		goto skip;

	for (i = 0; i < S2MU106_IRQ_GROUP_NR; i++) {
		mask_reg = s2mu106_mask_reg[i];
		i2c = get_i2c(s2mu106, i);

		if (mask_reg == S2MU106_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;
		s2mu106->irq_masks_cache[i] = s2mu106->irq_masks_cur[i];

		s2mu106_write_reg(i2c, s2mu106_mask_reg[i],
				s2mu106->irq_masks_cur[i]);
	}

	s2mu106->change_irq_mask = false;
skip:
	mutex_unlock(&s2mu106->irqlock);
}

static const inline struct s2mu106_irq_data *
irq_to_s2mu106_irq(struct s2mu106_dev *s2mu106, int irq)
{
	return &s2mu106_irqs[irq - s2mu106->irq_base];
}

static void s2mu106_irq_mask(struct irq_data *data)
{
	struct s2mu106_dev *s2mu106 = irq_get_chip_data(data->irq);
	const struct s2mu106_irq_data *irq_data =
	    irq_to_s2mu106_irq(s2mu106, data->irq);

	if (irq_data->group >= S2MU106_IRQ_GROUP_NR)
		return;

	s2mu106->irq_masks_cur[irq_data->group] |= irq_data->mask;
	s2mu106->change_irq_mask = true;
}

static void s2mu106_irq_unmask(struct irq_data *data)
{
	struct s2mu106_dev *s2mu106 = irq_get_chip_data(data->irq);
	const struct s2mu106_irq_data *irq_data =
	    irq_to_s2mu106_irq(s2mu106, data->irq);

	if (irq_data->group >= S2MU106_IRQ_GROUP_NR)
		return;

	s2mu106->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
	s2mu106->change_irq_mask = true;
}

static struct irq_chip s2mu106_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock	= s2mu106_irq_lock,
	.irq_bus_sync_unlock	= s2mu106_irq_sync_unlock,
	.irq_mask		= s2mu106_irq_mask,
	.irq_unmask		= s2mu106_irq_unmask,
};

static irqreturn_t s2mu106_irq_thread(int irq, void *data)
{
	struct s2mu106_dev *s2mu106 = data;
	u8 irq_reg[S2MU106_IRQ_GROUP_NR] = {0, };
	int i, ret;
	u8 irq_src;

	ret = s2mu106_read_reg(s2mu106->i2c, S2MU106_REG_IPINT, &irq_src);
	if (ret) {
        pr_err("%s:%s Failed to read interrupt source: %d\n",
            MFD_DEV_NAME, __func__, ret);
        return IRQ_NONE;
	}
	pr_info("%s: Top interrupt(0x%02x)\n", __func__, irq_src);

#if defined(CONFIG_CHARGER_S2MU106)
	if (irq_src & S2MU106_IRQSRC_CHG) {
		ret = s2mu106_bulk_read(s2mu106->i2c, S2MU106_CHG_INT1,
			S2MU106_NUM_IRQ_CHG_REGS, &irq_reg[CHG_INT1]);
		if (ret) {
			pr_err("%s:%s Failed to read charger interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}
		pr_info("%s() CHARGER intrrupt(0x%02x, 0x%02x, 0x%02x)\n",
			__func__, irq_reg[CHG_INT1], irq_reg[CHG_INT2],
			irq_reg[CHG_INT3]);
	}
#endif
#if defined(CONFIG_LEDS_S2MU106_FLASH)
	if (irq_src & S2MU106_IRQSRC_FLED) {
		ret = s2mu106_bulk_read(s2mu106->i2c, S2MU106_FLED_INT1,
			S2MU106_NUM_IRQ_LED_REGS, &irq_reg[FLED_INT1]);
		if (ret) {
			pr_err("%s:%s Failed to read charger interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}
		pr_info("%s() FLED intrrupt(0x%02x, 0x%02x)\n",
			__func__, irq_reg[FLED_INT1], irq_reg[FLED_INT2]);
	}
#endif
#if defined(CONFIG_HV_MUIC_S2MU106_AFC)
	if (irq_src & S2MU106_IRQSRC_AFC) {
        s2mu106_read_reg(s2mu106->muic, S2MU106_REG_AFC_INT, &irq_reg[AFC_INT]);
        pr_info("%s: AFC interrupt(0x%02x)\n", __func__, irq_reg[AFC_INT]);
	}
#endif
#if defined(CONFIG_MUIC_S2MU106)
	if (irq_src & S2MU106_IRQSRC_MUIC) {
        s2mu106_read_reg(s2mu106->muic, S2MU106_REG_MUIC_INT1, &irq_reg[MUIC_INT1]);
        s2mu106_read_reg(s2mu106->muic, S2MU106_REG_MUIC_INT2, &irq_reg[MUIC_INT2]);
        pr_info("%s: muic interrupt(0x%02x, 0x%02x)\n", __func__, irq_reg[MUIC_INT1],
            irq_reg[MUIC_INT2]);
	}
#endif
#if defined(CONFIG_PM_S2MU106)
	if (irq_src & S2MU106_IRQSRC_PM) {
        s2mu106_read_reg(s2mu106->muic, S2MU106_PM_VALUP1, &irq_reg[PM_VALUP1]);
        s2mu106_read_reg(s2mu106->muic, S2MU106_PM_VALUP2, &irq_reg[PM_VALUP2]);
        s2mu106_read_reg(s2mu106->muic, S2MU106_PM_INT1, &irq_reg[PM_INT1]);
        s2mu106_read_reg(s2mu106->muic, S2MU106_PM_INT2, &irq_reg[PM_INT2]);
        pr_info("%s: powermeter interrupt(0x%02x, 0x%02x, 0x%02x, 0x%02x)\n",__func__,
				irq_reg[PM_VALUP1], irq_reg[PM_VALUP2], irq_reg[PM_INT1], irq_reg[PM_INT2]);
	}
#endif
#if defined(CONFIG_MST_S2MU106)
	if (irq_src & S2MU106_IRQSRC_MST) {
	}
#endif
#if defined(CONFIG_MOTOR_S2MU106)
	if (irq_src & S2MU106_IRQSRC_HAPTIC) {
        	s2mu106_read_reg(s2mu106->haptic, S2MU106_REG_HAPTIC_INT, &irq_reg[HAPTIC_INT]);
        	pr_info("%s: haptic interrupt(0x%02x)\n", __func__, irq_reg[HAPTIC_INT]);
	}
#endif
#if defined(CONFIG_REGULATOR_S2MU106)
	if (irq_src & S2MU106_IRQSRC_HBST) {
	}
#endif

	/* Apply masking */
	for (i = 0; i < S2MU106_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~s2mu106->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MU106_IRQ_NR; i++) {
		if (irq_reg[s2mu106_irqs[i].group] & s2mu106_irqs[i].mask)
			handle_nested_irq(s2mu106->irq_base + i);
	}

	return IRQ_HANDLED;
}
static int irq_is_enable = true;
int s2mu106_irq_init(struct s2mu106_dev *s2mu106)
{
	int i;
	int ret;
	struct i2c_client *i2c = s2mu106->i2c;
	int cur_irq;
	u8 i2c_data;

	if (!s2mu106->irq_gpio) {
		dev_warn(s2mu106->dev, "No interrupt specified.\n");
		s2mu106->irq_base = 0;
		return 0;
	}

	if (!s2mu106->irq_base) {
		dev_err(s2mu106->dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&s2mu106->irqlock);

	s2mu106->irq = gpio_to_irq(s2mu106->irq_gpio);
	pr_err("%s:%s irq=%d, irq->gpio=%d\n", MFD_DEV_NAME, __func__,
			s2mu106->irq, s2mu106->irq_gpio);

	ret = gpio_request(s2mu106->irq_gpio, "if_pmic_irq");
	if (ret) {
		dev_err(s2mu106->dev, "%s: failed requesting gpio %d\n",
			__func__, s2mu106->irq_gpio);
		return ret;
	}
	gpio_direction_input(s2mu106->irq_gpio);
	gpio_free(s2mu106->irq_gpio);

	/* Mask individual interrupt sources */
	for (i = 0; i < S2MU106_IRQ_GROUP_NR; i++) {

		s2mu106->irq_masks_cur[i] = 0xff;
		s2mu106->irq_masks_cache[i] = 0xff;
        i2c = get_i2c(s2mu106, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (s2mu106_mask_reg[i] == S2MU106_REG_INVALID)
			continue;
		s2mu106_write_reg(i2c, s2mu106_mask_reg[i], 0xff);
	}

	/* Register with genirq */
	for (i = 0; i < S2MU106_IRQ_NR; i++) {
		cur_irq = 0;
		cur_irq = i + s2mu106->irq_base;
		irq_set_chip_data(cur_irq, s2mu106);
		irq_set_chip_and_handler(cur_irq, &s2mu106_irq_chip,
						handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}

	/* Unmask S2MU106 interrupt */
	i2c_data = 0xff;
#if defined(CONFIG_CHARGER_S2MU106)
	i2c_data &= ~(S2MU106_IRQSRC_CHG);
#endif
#if defined(CONFIG_LEDS_S2MU106_FLASH)
	i2c_data &= ~(S2MU106_IRQSRC_FLED);
#endif
#if defined(CONFIG_HV_MUIC_S2MU106_AFC)
	i2c_data &= ~(S2MU106_IRQSRC_AFC);
#endif
#if defined(CONFIG_MUIC_S2MU106)
	i2c_data &= ~(S2MU106_IRQSRC_MUIC);
#endif
#if defined(CONFIG_PM_S2MU106)
	i2c_data &= ~(S2MU106_IRQSRC_PM);
#endif
#if defined(CONFIG_MST_S2MU106)
	i2c_data &= ~(S2MU106_IRQSRC_MST);
#endif
#if defined(CONFIG_MOTOR_S2MU106)
	i2c_data &= ~(S2MU106_IRQSRC_HAPTIC);
#endif
#if defined(CONFIG_REGULATOR_S2MU106)
	i2c_data &= ~(S2MU106_IRQSRC_HBST);
#endif
	s2mu106_write_reg(s2mu106->i2c, S2MU106_REG_IPINT_MASK, i2c_data);
	pr_info("%s: %s init top-irq mask(0x%02x)\n", MFD_DEV_NAME, __func__, i2c_data);
	pr_info("%s: irq gpio pre-state(0x%02x)\n", __func__,
		gpio_get_value(s2mu106->irq_gpio));

	if (irq_is_enable) {
		ret = request_threaded_irq(s2mu106->irq, NULL,
				s2mu106_irq_thread,
				IRQF_TRIGGER_LOW | IRQF_ONESHOT,
				"s2mu106-irq", s2mu106);
	}

	if (ret) {
		dev_err(s2mu106->dev, "Failed to request IRQ %d: %d\n",
			s2mu106->irq, ret);
		return ret;
	}

	return 0;
}

void s2mu106_irq_exit(struct s2mu106_dev *s2mu106)
{
	if (s2mu106->irq)
		free_irq(s2mu106->irq, s2mu106);
}
