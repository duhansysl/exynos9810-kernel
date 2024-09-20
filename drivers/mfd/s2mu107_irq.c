/*
 * s2mu107-irq.c - Interrupt controller support for s2mu107
 *
 * Copyright (C) 2019 Samsung Electronics Co.Ltd
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
#include <linux/mfd/samsung/s2mu107.h>
#include <linux/err.h>

/* TODO : add IP Header file include*/
#if defined(CONFIG_CHARGER_S2MU107)
#if defined(CONFIG_BATTERY_SAMSUNG_LEGO_STYLE)
#include "../battery/charger/s2mu107_switching_charger.h"
#else
#include "../battery_v2/include/charger/s2mu107_switching_charger.h"
#endif
#endif
#if defined(CONFIG_MUIC_S2MU107)
#include <linux/muic/s2mu107-muic.h>
#endif
#if defined(CONFIG_PM_S2MU107)
#if defined(CONFIG_BATTERY_SAMSUNG_LEGO_STYLE)
#include "../battery/charger/s2mu107_pmeter.h"
#else
#include "../battery_v2/include/s2mu107_pmeter.h"
#endif
#endif
#if defined(CONFIG_LEDS_S2MU107_FLASH)
#include <linux/leds-s2mu107.h>
#endif
#if defined(CONFIG_CHARGER_S2MU107_DIRECT)
#if defined(CONFIG_BATTERY_SAMSUNG_LEGO_STYLE)
#include "../battery/charger/s2mu107_direct_charger.h"
#else
#include "../battery_v2/include/charger/s2mu107_direct_charger.h"
#endif
#endif
static const u8 s2mu107_mask_reg[] = {
#if defined(CONFIG_LEDS_S2MU107_FLASH)
	[FLED_INT1] = S2MU107_FLED_INT1_MASK,
	[FLED_INT2] = S2MU107_FLED_INT2_MASK,
#endif
#if defined(CONFIG_CHARGER_S2MU107)
	[SC_INT1] = S2MU107_SC_INT1_MASK,
	[SC_INT2] = S2MU107_SC_INT2_MASK,
	[SC_INT3] = S2MU107_SC_INT3_MASK,
#endif
#if defined(CONFIG_CHARGER_S2MU107_DIRECT)
	[DC_INT0] = S2MU107_DC_INT0_MASK,
	[DC_INT1] = S2MU107_DC_INT1_MASK,
	[DC_INT2] = S2MU107_DC_INT2_MASK,
	[DC_INT3] = S2MU107_DC_INT3_MASK,
#endif
#if defined(CONFIG_HV_MUIC_S2MU107_AFC)
	[AFC_INT] = S2MU107_REG_AFC_INT,
#endif
#if defined(CONFIG_MUIC_S2MU107)
	[MUIC_INT1] = S2MU107_REG_MUIC_INT1,
	[MUIC_INT2] = S2MU107_REG_MUIC_INT2,
#endif
#if defined(CONFIG_PM_S2MU107)
	[PM_VALUP1] = S2MU107_PM_VALUP1_MASK,
	[PM_VALUP2] = S2MU107_PM_VALUP2_MASK,
	[PM_INT1] = S2MU107_PM_INT1_MASK,
	[PM_INT2] = S2MU107_PM_INT2_MASK,
#endif
};

/* Interrupt register */
#define S2MU107_FLED_INT1		0x02
#define S2MU107_FLED_INT2		0x03

#define S2MU107_FLED_INT1_MASK		0x0A
#define S2MU107_FLED_INT2_MASK		0x0B

#define S2MU107_SC_INT1		0x00
#define S2MU107_SC_INT2		0x01
#define S2MU107_SC_INT3		0x02

#define S2MU107_SC_INT1_MASK	0x08
#define S2MU107_SC_INT2_MASK	0x09
#define S2MU107_SC_INT3_MASK	0x0A

#define S2MU107_PM_VALUP1	0x03
#define S2MU107_PM_VALUP2	0x04
#define S2MU107_PM_INT1		0x05
#define S2MU107_PM_INT2		0x06
#define S2MU107_PM_VALUP1_MASK	0x0B
#define S2MU107_PM_VALUP2_MASK	0x0C
#define S2MU107_PM_INT1_MASK	0x0D
#define S2MU107_PM_INT2_MASK	0x0E

#define S2MU107_DC_INT0		0x03
#define S2MU107_DC_INT1		0x04
#define S2MU107_DC_INT2		0x05
#define S2MU107_DC_INT3		0x06

#define S2MU107_DC_INT0_MASK		0x0B
#define S2MU107_DC_INT1_MASK		0x0C
#define S2MU107_DC_INT2_MASK		0x0D
#define S2MU107_DC_INT3_MASK		0x0E

struct s2mu107_irq_data {
	int mask;
	enum s2mu107_irq_source group;
};

static struct i2c_client *get_i2c(struct s2mu107_dev *s2mu107,
				  enum s2mu107_irq_source src)
{
	switch (src) {
#if defined(CONFIG_LEDS_S2MU107_FLASH)
	case FLED_INT1 ... FLED_INT2:
		return s2mu107->i2c;
#endif
#if defined(CONFIG_CHARGER_S2MU107)
	case SC_INT1 ... SC_INT3:
		return s2mu107->chg;
#endif
#if defined(CONFIG_CHARGER_S2MU107_DIRECT)
	case DC_INT0 ... DC_INT3:
		return s2mu107->chg;
#endif
#if defined(CONFIG_HV_MUIC_S2MU107_AFC)
	case AFC_INT:
		return s2mu107->muic;
#endif
#if defined(CONFIG_MUIC_S2MU107)
	case MUIC_INT1 ... MUIC_INT2:
		return s2mu107->muic;
#endif
#if defined(CONFIG_PM_S2MU107)
	case PM_VALUP1 ... PM_INT2:
		return s2mu107->muic;
#endif
	default:
		return ERR_PTR(-EINVAL);
	}
}

#define DECLARE_IRQ(idx, _group, _mask)		\
	[(idx)] = { .group = (_group), .mask = (_mask) }
static const struct s2mu107_irq_data s2mu107_irqs[] = {
#if defined(CONFIG_LEDS_S2MU107_FLASH)
	DECLARE_IRQ(S2MU107_FLED_IRQ1_C2F_VBYP_OVP_PROT,	FLED_INT1, MASK(1,0)),
	DECLARE_IRQ(S2MU107_FLED_IRQ1_C2F_VBYP_OK_WARNING,	FLED_INT1, MASK(1,1)),
	DECLARE_IRQ(S2MU107_FLED_IRQ1_SW_OFF_TORCH_ON,		FLED_INT1, MASK(1,2)),
	DECLARE_IRQ(S2MU107_FLED_IRQ1_FLED_ON_TA_DISCONNECT,	FLED_INT1, MASK(1,3)),
	DECLARE_IRQ(S2MU107_FLED_IRQ1_CH1_LED_ON_DONE,		FLED_INT1, MASK(1,5)),
	DECLARE_IRQ(S2MU107_FLED_IRQ1_FLED_OPEN_CH1_I,		FLED_INT1, MASK(1,6)),
	DECLARE_IRQ(S2MU107_FLED_IRQ1_FLED_SHORT_CH1_I,		FLED_INT1, MASK(1,7)),

	DECLARE_IRQ(S2MU107_FLED_IRQ2_TORCH_DC_INT,		FLED_INT2, MASK(1,0)),
	DECLARE_IRQ(S2MU107_FLED_IRQ2_FLASH_DC_INT,		FLED_INT2, MASK(1,1)),
#endif
#if defined(CONFIG_CHARGER_S2MU107)
	DECLARE_IRQ(S2MU107_SC_IRQ2_IVR,			SC_INT2, MASK(1,1)),
	DECLARE_IRQ(S2MU107_SC_IRQ1_SYS,			SC_INT1, MASK(1,0)),
	DECLARE_IRQ(S2MU107_SC_IRQ1_CV,				SC_INT1, MASK(1,1)),
	DECLARE_IRQ(S2MU107_SC_IRQ1_CHG_Fault,			SC_INT1, MASK(1,2)),
	DECLARE_IRQ(S2MU107_SC_IRQ1_CHG_Restart,		SC_INT1, MASK(1,3)),
	DECLARE_IRQ(S2MU107_SC_IRQ1_DONE,			SC_INT1, MASK(1,4)),
	DECLARE_IRQ(S2MU107_SC_IRQ1_TOP_OFF,			SC_INT1, MASK(1,5)),
	DECLARE_IRQ(S2MU107_SC_IRQ1_WCIN,			SC_INT1, MASK(1,6)),
	DECLARE_IRQ(S2MU107_SC_IRQ1_CHGIN,			SC_INT1, MASK(1,7)),

	DECLARE_IRQ(S2MU107_SC_IRQ2_ICR,			SC_INT2, MASK(1,0)),
	DECLARE_IRQ(S2MU107_SC_IRQ2_AICL,			SC_INT2, MASK(1,2)),
	DECLARE_IRQ(S2MU107_SC_IRQ2_VBUS_Short,			SC_INT2, MASK(1,3)),
	DECLARE_IRQ(S2MU107_SC_IRQ2_BST,			SC_INT2, MASK(1,4)),
	DECLARE_IRQ(S2MU107_SC_IRQ2_OTG,			SC_INT2, MASK(1,5)),
	DECLARE_IRQ(S2MU107_SC_IRQ2_BAT,			SC_INT2, MASK(1,6)),
	DECLARE_IRQ(S2MU107_SC_IRQ2_DET_VBUS,			SC_INT2, MASK(1,7)),

	DECLARE_IRQ(S2MU107_SC_IRQ3_BATID,			SC_INT3, MASK(1,0)),
	DECLARE_IRQ(S2MU107_SC_IRQ3_BATID_DONE,			SC_INT3, MASK(1,1)),
	DECLARE_IRQ(S2MU107_SC_IRQ3_QBAT_OFF,			SC_INT3, MASK(1,2)),
	DECLARE_IRQ(S2MU107_SC_IRQ3_BATN_OPEN,			SC_INT3, MASK(1,4)),
	DECLARE_IRQ(S2MU107_SC_IRQ3_BATP_OPEN,			SC_INT3, MASK(1,5)),
	DECLARE_IRQ(S2MU107_SC_IRQ3_BAT_CONTACT_CK_DONE,	SC_INT3, MASK(1,6)),
#endif
#if defined(CONFIG_CHARGER_S2MU107_DIRECT)
	DECLARE_IRQ(S2MU107_DC_IRQ2_DC_DONE,			DC_INT2, MASK(1,7)),

	DECLARE_IRQ(S2MU107_DC_IRQ0_DC_OUT_OVP,			DC_INT0, MASK(1,0)),
	DECLARE_IRQ(S2MU107_DC_IRQ0_DC_BAT_OKB,			DC_INT0, MASK(1,1)),
	DECLARE_IRQ(S2MU107_DC_IRQ0_DC_BYP_OVP,			DC_INT0, MASK(1,2)),
	DECLARE_IRQ(S2MU107_DC_IRQ0_DC_BYP2OUT_OVP,		DC_INT0, MASK(1,3)),
	DECLARE_IRQ(S2MU107_DC_IRQ0_DC_CHGIN_OKB,		DC_INT0, MASK(1,4)),
	DECLARE_IRQ(S2MU107_DC_IRQ0_DC_WCIN_OKB,		DC_INT0, MASK(1,5)),
	DECLARE_IRQ(S2MU107_DC_IRQ0_DC_NORMAL_CHARGING,		DC_INT0, MASK(1,6)),
	DECLARE_IRQ(S2MU107_DC_IRQ0_DC_RAMP_FAIL,		DC_INT0, MASK(1,7)),

	DECLARE_IRQ(S2MU107_DC_IRQ1_DC_CHGIN_DIODE_PROT,	DC_INT1, MASK(1,0)),
	DECLARE_IRQ(S2MU107_DC_IRQ1_DC_WCIN_DIODE_PROT,		DC_INT1, MASK(1,1)),
	DECLARE_IRQ(S2MU107_DC_IRQ1_DC_CHGIN_PLUG_OUT,		DC_INT1, MASK(1,2)),
	DECLARE_IRQ(S2MU107_DC_IRQ1_DC_OFF,			DC_INT1, MASK(1,3)),
	DECLARE_IRQ(S2MU107_DC_IRQ1_DC_CHGIN_RCP,		DC_INT1, MASK(1,4)),
	DECLARE_IRQ(S2MU107_DC_IRQ1_DC_WCIN_RCP,		DC_INT1, MASK(1,5)),
	DECLARE_IRQ(S2MU107_DC_IRQ1_DC_OCP,			DC_INT1, MASK(1,6)),

	DECLARE_IRQ(S2MU107_DC_IRQ2_DC_CHGIN_ICL,		DC_INT2, MASK(1,0)),
	DECLARE_IRQ(S2MU107_DC_IRQ2_DC_WCIN_ICL,		DC_INT2, MASK(1,1)),
	DECLARE_IRQ(S2MU107_DC_IRQ2_DC_CV,			DC_INT2, MASK(1,2)),
	DECLARE_IRQ(S2MU107_DC_IRQ2_DC_SCP,			DC_INT2, MASK(1,3)),
	DECLARE_IRQ(S2MU107_DC_IRQ2_DC_THERMAL,			DC_INT2, MASK(1,4)),
	DECLARE_IRQ(S2MU107_DC_IRQ2_DC_LONG_CC,			DC_INT2, MASK(1,5)),
	DECLARE_IRQ(S2MU107_DC_IRQ2_DC_PPS_FAIL,		DC_INT2, MASK(1,6)),

	DECLARE_IRQ(S2MU107_DC_IRQ3_DC_TFB,			DC_INT3, MASK(1,0)),
	DECLARE_IRQ(S2MU107_DC_IRQ3_DC_TSD,			DC_INT3, MASK(1,2)),
	DECLARE_IRQ(S2MU107_DC_IRQ3_DC_WCIN_DOWN,		DC_INT3, MASK(1,3)),
	DECLARE_IRQ(S2MU107_DC_IRQ3_DC_WCIN_UP,			DC_INT3, MASK(1,4)),
	DECLARE_IRQ(S2MU107_DC_IRQ3_DC_PM_OFF,			DC_INT3, MASK(1,5)),
	DECLARE_IRQ(S2MU107_DC_IRQ3_DC_SC_OFF,			DC_INT3, MASK(1,6)),
#endif
#if defined(CONFIG_HV_MUIC_S2MU107_AFC)
	DECLARE_IRQ(S2MU107_AFC_IRQ_VbADC,			AFC_INT, MASK(1,0)),
	DECLARE_IRQ(S2MU107_AFC_IRQ_VDNMon,			AFC_INT, MASK(1,1)),
	DECLARE_IRQ(S2MU107_AFC_IRQ_DNRes,			AFC_INT, MASK(1,2)),
	DECLARE_IRQ(S2MU107_AFC_IRQ_MPNack,			AFC_INT, MASK(1,3)),
	DECLARE_IRQ(S2MU107_AFC_IRQ_MRxBufOw,			AFC_INT, MASK(1,4)),
	DECLARE_IRQ(S2MU107_AFC_IRQ_MRxTrf,			AFC_INT, MASK(1,5)),
	DECLARE_IRQ(S2MU107_AFC_IRQ_MRxPerr,			AFC_INT, MASK(1,6)),
	DECLARE_IRQ(S2MU107_AFC_IRQ_MRxRdy,			AFC_INT, MASK(1,7)),
#endif
#if defined(CONFIG_MUIC_S2MU107)
	DECLARE_IRQ(S2MU107_MUIC_IRQ1_DETACH,			MUIC_INT1, MASK(1,1)),
	DECLARE_IRQ(S2MU107_MUIC_IRQ1_ATTATCH,			MUIC_INT1, MASK(1,0)),
	DECLARE_IRQ(S2MU107_MUIC_IRQ1_KP,			MUIC_INT1, MASK(1,2)),
	DECLARE_IRQ(S2MU107_MUIC_IRQ1_LKP,			MUIC_INT1, MASK(1,3)),
	DECLARE_IRQ(S2MU107_MUIC_IRQ1_LKR,			MUIC_INT1, MASK(1,4)),
	DECLARE_IRQ(S2MU107_MUIC_IRQ1_RID_CHG,			MUIC_INT1, MASK(1,5)),
	DECLARE_IRQ(S2MU107_MUIC_IRQ1_USB_KILLER,		MUIC_INT1, MASK(1,6)),
	DECLARE_IRQ(S2MU107_MUIC_IRQ1_WAKE_UP,			MUIC_INT1, MASK(1,7)),

	DECLARE_IRQ(S2MU107_MUIC_IRQ2_VBUS_ON,			MUIC_INT2, MASK(1,0)),
	DECLARE_IRQ(S2MU107_MUIC_IRQ2_RSVD_ATTACH,		MUIC_INT2, MASK(1,1)),
	DECLARE_IRQ(S2MU107_MUIC_IRQ2_ADC_CHANGE,		MUIC_INT2, MASK(1,2)),
	DECLARE_IRQ(S2MU107_MUIC_IRQ2_STUCK,			MUIC_INT2, MASK(1,3)),
	DECLARE_IRQ(S2MU107_MUIC_IRQ2_STUCKRCV,			MUIC_INT2, MASK(1,4)),
	DECLARE_IRQ(S2MU107_MUIC_IRQ2_MHDL,			MUIC_INT2, MASK(1,5)),
	DECLARE_IRQ(S2MU107_MUIC_IRQ2_AV_CHARGE,		MUIC_INT2, MASK(1,6)),
	DECLARE_IRQ(S2MU107_MUIC_IRQ2_VBUS_OFF,			MUIC_INT2, MASK(1,7)),
#endif
#if defined(CONFIG_PM_S2MU107)
	DECLARE_IRQ(S2MU107_PM_VALUP1_TDIEUP,			PM_VALUP1, MASK(1,0)),
	DECLARE_IRQ(S2MU107_PM_VALUP1_VCC2UP,			PM_VALUP1, MASK(1,1)),
	DECLARE_IRQ(S2MU107_PM_VALUP1_VCC1UP,			PM_VALUP1, MASK(1,2)),
	DECLARE_IRQ(S2MU107_PM_VALUP1_VBATUP,			PM_VALUP1, MASK(1,3)),
	DECLARE_IRQ(S2MU107_PM_VALUP1_VSYSUP,			PM_VALUP1, MASK(1,4)),
	DECLARE_IRQ(S2MU107_PM_VALUP1_VBYPUP,			PM_VALUP1, MASK(1,5)),
	DECLARE_IRQ(S2MU107_PM_VALUP1_VWCINUP,			PM_VALUP1, MASK(1,6)),
	DECLARE_IRQ(S2MU107_PM_VALUP1_VCHGINUP,			PM_VALUP1, MASK(1,7)),

	DECLARE_IRQ(S2MU107_PM_VALUP2_ITXUP,			PM_VALUP2, MASK(1,4)),
	DECLARE_IRQ(S2MU107_PM_VALUP2_IOTGUP,			PM_VALUP2, MASK(1,5)),
	DECLARE_IRQ(S2MU107_PM_VALUP2_IWCINUP,			PM_VALUP2, MASK(1,6)),
	DECLARE_IRQ(S2MU107_PM_VALUP2_ICHGINUP,			PM_VALUP2, MASK(1,7)),

	DECLARE_IRQ(S2MU107_PM_IRQ1_TDIEI,			PM_INT1, MASK(1,0)),
	DECLARE_IRQ(S2MU107_PM_IRQ1_VCC2I,			PM_INT1, MASK(1,1)),
	DECLARE_IRQ(S2MU107_PM_IRQ1_VCC1I,			PM_INT1, MASK(1,2)),
	DECLARE_IRQ(S2MU107_PM_IRQ1_VBATI,			PM_INT1, MASK(1,3)),
	DECLARE_IRQ(S2MU107_PM_IRQ1_VSYSI,			PM_INT1, MASK(1,4)),
	DECLARE_IRQ(S2MU107_PM_IRQ1_VBYPI,			PM_INT1, MASK(1,5)),
	DECLARE_IRQ(S2MU107_PM_IRQ1_VWCINI,			PM_INT1, MASK(1,6)),
	DECLARE_IRQ(S2MU107_PM_IRQ1_VCHGINI,			PM_INT1, MASK(1,7)),

	DECLARE_IRQ(S2MU107_PM_IRQ2_IWCIN_TH,			PM_INT2, MASK(1,0)),
	DECLARE_IRQ(S2MU107_PM_IRQ2_ICHGIN_TH,			PM_INT2, MASK(1,1)),
	DECLARE_IRQ(S2MU107_PM_IRQ2_GPADCI,			PM_INT2, MASK(1,3)),
	DECLARE_IRQ(S2MU107_PM_IRQ2_ITXI,			PM_INT2, MASK(1,4)),
	DECLARE_IRQ(S2MU107_PM_IRQ2_IOTGI,			PM_INT2, MASK(1,5)),
	DECLARE_IRQ(S2MU107_PM_IRQ2_IWCINI,			PM_INT2, MASK(1,6)),
	DECLARE_IRQ(S2MU107_PM_IRQ2_ICHGINI,			PM_INT2, MASK(1,7)),
#endif
};

static void s2mu107_irq_lock(struct irq_data *data)
{
	struct s2mu107_dev *s2mu107 = irq_get_chip_data(data->irq);

	mutex_lock(&s2mu107->irqlock);
}

static void s2mu107_irq_sync_unlock(struct irq_data *data)
{
	struct s2mu107_dev *s2mu107 = irq_get_chip_data(data->irq);
	int i;
	u8 mask_reg;
	struct i2c_client *i2c;

	if (s2mu107->change_irq_mask == false)
 		goto skip;

	for (i = 0; i < S2MU107_IRQ_GROUP_NR; i++) {
		mask_reg = s2mu107_mask_reg[i];
		i2c = get_i2c(s2mu107, i);

		if (mask_reg == S2MU107_REG_INVALID ||
				IS_ERR_OR_NULL(i2c))
			continue;
		s2mu107->irq_masks_cache[i] = s2mu107->irq_masks_cur[i];

		s2mu107_write_reg(i2c, s2mu107_mask_reg[i],
				s2mu107->irq_masks_cur[i]);
	}

	s2mu107->change_irq_mask = false;
skip:
	mutex_unlock(&s2mu107->irqlock);
}

static const inline struct s2mu107_irq_data *
irq_to_s2mu107_irq(struct s2mu107_dev *s2mu107, int irq)
{
	return &s2mu107_irqs[irq - s2mu107->irq_base];
}

static void s2mu107_irq_mask(struct irq_data *data)
{
	struct s2mu107_dev *s2mu107 = irq_get_chip_data(data->irq);
	const struct s2mu107_irq_data *irq_data = irq_to_s2mu107_irq(s2mu107,
								     data->irq);

	if (irq_data->group >= S2MU107_IRQ_GROUP_NR)
		return;

	s2mu107->irq_masks_cur[irq_data->group] |= irq_data->mask;
	s2mu107->change_irq_mask = true;
}

static void s2mu107_irq_unmask(struct irq_data *data)
{
	struct s2mu107_dev *s2mu107 = irq_get_chip_data(data->irq);
	const struct s2mu107_irq_data *irq_data = irq_to_s2mu107_irq(s2mu107,
								     data->irq);

	if (irq_data->group >= S2MU107_IRQ_GROUP_NR)
		return;

	s2mu107->irq_masks_cur[irq_data->group] &= ~irq_data->mask;
	s2mu107->change_irq_mask = true;
}

static struct irq_chip s2mu107_irq_chip = {
	.name			= MFD_DEV_NAME,
	.irq_bus_lock		= s2mu107_irq_lock,
	.irq_bus_sync_unlock	= s2mu107_irq_sync_unlock,
	.irq_mask		= s2mu107_irq_mask,
	.irq_unmask		= s2mu107_irq_unmask,
};

static irqreturn_t s2mu107_irq_thread(int irq, void *data)
{
	struct s2mu107_dev *s2mu107 = data;
	u8 irq_src;
#if defined(CONFIG_LEDS_S2MU107_FLASH) || defined(CONFIG_CHARGER_S2MU107) || \
    defined(CONFIG_CHARGER_S2MU107_DIRECT) || defined(CONFIG_MUIC_S2MU107) || \
    defined(CONFIG_HV_MUIC_S2MU107_AFC) || defined(CONFIG_PM_S2MU107)
	u8 irq_reg[S2MU107_IRQ_GROUP_NR] = {0, };
	int i;
#endif
	int ret;

	ret = s2mu107_read_reg(s2mu107->i2c, S2MU107_REG_IPINT, &irq_src);
	if (ret) {
		pr_err("%s:%s Failed to read interrupt source: %d\n",
			MFD_DEV_NAME, __func__, ret);
		return IRQ_NONE;
	}
	pr_info("%s: Top interrupt(0x%02x)\n", __func__, irq_src);

#if defined(CONFIG_LEDS_S2MU107_FLASH)
	if (irq_src & S2MU107_IRQSRC_FLED) {
		ret = s2mu107_bulk_read(s2mu107->i2c, S2MU107_FLED_INT1,
					S2MU107_NUM_IRQ_LED_REGS,
					&irq_reg[FLED_INT1]);
		if (ret) {
			pr_err("%s:%s Failed to read FLED interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}
		pr_info("%s: FLED interrupt(0x%02x, 0x%02x)\n",
			__func__, irq_reg[FLED_INT1], irq_reg[FLED_INT2]);
	}
#endif
#if defined(CONFIG_CHARGER_S2MU107)
	if (irq_src & S2MU107_IRQSRC_SC) {
		ret = s2mu107_bulk_read(s2mu107->chg, S2MU107_SC_INT1,
					S2MU107_NUM_IRQ_SC_REGS,
					&irq_reg[SC_INT1]);
		if (ret) {
			pr_err("%s:%s Failed to read Charger interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}
		pr_info("%s: CHARGER interrupt(0x%02x, 0x%02x, 0x%02x)\n",
			__func__, irq_reg[SC_INT1], irq_reg[SC_INT2],
			irq_reg[SC_INT3]);
	}
#endif
#if defined(CONFIG_CHARGER_S2MU107_DIRECT)
	if (irq_src & S2MU107_IRQSRC_DC) {
		ret = s2mu107_bulk_read(s2mu107->chg, S2MU107_DC_INT0,
					S2MU107_NUM_IRQ_DC_REGS,
					&irq_reg[DC_INT0]);
		if (ret) {
			pr_err("%s:%s Failed to read Direct Charger interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}
		pr_info("%s: DIRECT CHARGER interrupt(0x%02x, 0x%02x, "
			"0x%02x, 0x%02x)\n", __func__,
			irq_reg[DC_INT0], irq_reg[DC_INT1],
			irq_reg[DC_INT2], irq_reg[DC_INT3]);
	}
#endif
#if defined(CONFIG_HV_MUIC_S2MU107_AFC)
	if (irq_src & S2MU107_IRQSRC_AFC) {
		ret = s2mu107_read_reg(s2mu107->muic,
				       S2MU107_REG_AFC_INT, &irq_reg[AFC_INT]);
		if (ret) {
			pr_err("%s:%s Failed to read AFC interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}
		pr_info("%s: AFC interrupt(0x%02x)\n",
			 __func__, irq_reg[AFC_INT]);
	}
#endif
#if defined(CONFIG_MUIC_S2MU107)
	if (irq_src & S2MU107_IRQSRC_MUIC) {
		ret = s2mu107_bulk_read(s2mu107->muic, S2MU107_REG_MUIC_INT1,
					S2MU107_NUM_IRQ_MUIC_REGS,
					&irq_reg[MUIC_INT1]);
		if (ret) {
			pr_err("%s:%s Failed to read MUIC interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}
		pr_info("%s: MUIC interrupt(0x%02x, 0x%02x)\n", __func__,
			irq_reg[MUIC_INT1], irq_reg[MUIC_INT2]);
	}
#endif
#if defined(CONFIG_PM_S2MU107)
	if (irq_src & S2MU107_IRQSRC_PM) {
		ret = s2mu107_bulk_read(s2mu107->muic, S2MU107_PM_VALUP1,
					S2MU107_NUM_IRQ_PM_REGS,
					&irq_reg[PM_VALUP1]);
		if (ret) {
			pr_err("%s:%s Failed to read PM interrupt: %d\n",
				MFD_DEV_NAME, __func__, ret);
			return IRQ_NONE;
		}
		pr_info("%s: PM interrupt(0x%02x, 0x%02x, "
			"0x%02x, 0x%02x)\n", __func__,
			irq_reg[PM_VALUP1], irq_reg[PM_VALUP2],
			irq_reg[PM_INT1], irq_reg[PM_INT2]);
	}
#endif

#if defined(CONFIG_LEDS_S2MU107_FLASH) || defined(CONFIG_CHARGER_S2MU107) || \
    defined(CONFIG_CHARGER_S2MU107_DIRECT) || defined(CONFIG_MUIC_S2MU107) || \
    defined(CONFIG_HV_MUIC_S2MU107_AFC) || defined(CONFIG_PM_S2MU107)
	/* Apply masking */
	for (i = 0; i < S2MU107_IRQ_GROUP_NR; i++)
		irq_reg[i] &= ~s2mu107->irq_masks_cur[i];

	/* Report */
	for (i = 0; i < S2MU107_IRQ_NR; i++) {
		if (irq_reg[s2mu107_irqs[i].group] & s2mu107_irqs[i].mask)
			handle_nested_irq(s2mu107->irq_base + i);
	}
#endif
	return IRQ_HANDLED;
}
static int irq_is_enable = true;
int s2mu107_irq_init(struct s2mu107_dev *s2mu107)
{
	struct i2c_client *i2c = s2mu107->i2c;
	int i, ret, cur_irq;
	u8 i2c_data;

	if (!s2mu107->irq_gpio) {
		dev_warn(s2mu107->dev, "No interrupt specified.\n");
		s2mu107->irq_base = 0;
		return 0;
	}

	if (!s2mu107->irq_base) {
		dev_err(s2mu107->dev, "No interrupt base specified.\n");
		return 0;
	}

	mutex_init(&s2mu107->irqlock);

	s2mu107->irq = gpio_to_irq(s2mu107->irq_gpio);
	pr_err("%s:%s irq=%d, irq->gpio=%d\n", MFD_DEV_NAME, __func__,
		s2mu107->irq, s2mu107->irq_gpio);

	ret = gpio_request(s2mu107->irq_gpio, "if_pmic_irq");
	if (ret) {
		dev_err(s2mu107->dev, "%s: failed requesting gpio %d\n",
			__func__, s2mu107->irq_gpio);
		return ret;
	}
	gpio_direction_input(s2mu107->irq_gpio);
	gpio_free(s2mu107->irq_gpio);

	/* Mask individual interrupt sources */
	for (i = 0; i < S2MU107_IRQ_GROUP_NR; i++) {
		s2mu107->irq_masks_cur[i] = 0xFF;
		s2mu107->irq_masks_cache[i] = 0xFF;

		i2c = get_i2c(s2mu107, i);

		if (IS_ERR_OR_NULL(i2c))
			continue;
		if (s2mu107_mask_reg[i] == S2MU107_REG_INVALID)
			continue;
		s2mu107_write_reg(i2c, s2mu107_mask_reg[i], 0xFF);
	}

	/* Register with genirq */
	for (i = 0; i < S2MU107_IRQ_NR; i++) {
		cur_irq = 0;
		cur_irq = i + s2mu107->irq_base;
		irq_set_chip_data(cur_irq, s2mu107);
		irq_set_chip_and_handler(cur_irq, &s2mu107_irq_chip,
					 handle_level_irq);
		irq_set_nested_thread(cur_irq, 1);
#ifdef CONFIG_ARM
		set_irq_flags(cur_irq, IRQF_VALID);
#else
		irq_set_noprobe(cur_irq);
#endif
	}
	/* Unmask S2MU107 interrupt */
	i2c_data = 0xFF;
#if defined(CONFIG_CHARGER_S2MU107)
	i2c_data &= ~(S2MU107_IRQSRC_SC);
#endif
#if defined(CONFIG_LEDS_S2MU107_FLASH)
	i2c_data &= ~(S2MU107_IRQSRC_FLED);
#endif
#if defined(CONFIG_HV_MUIC_S2MU107_AFC)
	i2c_data &= ~(S2MU107_IRQSRC_AFC);
#endif
#if defined(CONFIG_MUIC_S2MU107)
	i2c_data &= ~(S2MU107_IRQSRC_MUIC);
#endif
#if defined(CONFIG_PM_S2MU107)
	i2c_data &= ~(S2MU107_IRQSRC_PM);
#endif
#if defined(CONFIG_CHARGER_S2MU107_DIRECT)
	i2c_data &= ~(S2MU107_IRQSRC_DC);
#endif
	/* TODO: check write_reg */
	s2mu107_write_reg(s2mu107->i2c, 0x08, i2c_data);

	pr_info("%s: %s init top-irq mask(0x%02x)\n",
		MFD_DEV_NAME, __func__, i2c_data);
	pr_info("%s: irq gpio pre-state(0x%02x)\n",
		__func__, gpio_get_value(s2mu107->irq_gpio));

	if (irq_is_enable) {
		ret = request_threaded_irq(s2mu107->irq, NULL,
					   s2mu107_irq_thread,
					   IRQF_TRIGGER_LOW | IRQF_ONESHOT,
					   "s2mu107-irq", s2mu107);
	}

	if (ret) {
		dev_err(s2mu107->dev, "Failed to request IRQ %d: %d\n",
			s2mu107->irq, ret);
		return ret;
	}

	return 0;
}

void s2mu107_irq_exit(struct s2mu107_dev *s2mu107)
{
	if (s2mu107->irq)
		free_irq(s2mu107->irq, s2mu107);
}
