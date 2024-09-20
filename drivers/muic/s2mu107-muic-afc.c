/*
 * driver/muic/s2mu107-afc.c - S2MU107 micro USB switch device driver
 *
 * Copyright (C) 2019 Samsung Electronics
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

#define pr_fmt(fmt)	"[MUIC_HV] " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>

#include <linux/mfd/samsung/s2mu107.h>

/* MUIC header file */
#include <linux/muic/muic.h>
#include <linux/muic/s2mu107-muic.h>
#include <linux/muic/s2mu107-muic-hv.h>
#ifdef CONFIG_MUIC_MANAGER
#include <linux/muic/muic_interface.h>
#endif

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */
#include <linux/delay.h>

/* powermeter */
#if defined(CONFIG_PM_S2MU107)
#if defined(CONFIG_BATTERY_SAMSUNG_LEGO_STYLE)
#include "../battery/charger/s2mu107_pmeter.h"
#else
#include "../battery_v2/include/s2mu107_pmeter.h"
#endif
#endif


static struct s2mu107_muic_data *static_data;

/*
 * Functions Prototype
 */
static void s2mu107_hv_muic_handle_attach(struct s2mu107_muic_data *muic_data,
		muic_attached_dev_t new_dev);
static void _s2mu107_hv_muic_reset(struct s2mu107_muic_data *muic_data);

muic_attached_dev_t s2mu107_hv_muic_check_id_err(struct s2mu107_muic_data *muic_data,
		muic_attached_dev_t new_dev)
{
	muic_attached_dev_t after_new_dev = new_dev;
	struct muic_platform_data *muic_pdata = muic_data->pdata;

	if (!muic_core_hv_is_hv_dev(muic_pdata))
		goto out;

	switch (new_dev) {
	case ATTACHED_DEV_TA_MUIC:
		pr_info("%s cannot change HV(%d)->TA(%d)!\n",
			__func__, muic_pdata->attached_dev, new_dev);
		after_new_dev = muic_pdata->attached_dev;
		break;
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
		pr_info("%s Undefined\n", __func__);
		after_new_dev = ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC;
		break;
	case ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC:
		pr_info("%s Unsupported\n", __func__);
		after_new_dev = ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC;
		break;
	default:
		pr_info("%s Supported\n", __func__);
		after_new_dev = ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC;
		break;
	}
out:
	return after_new_dev;
}

/*
 * Unit functions
 */
static int s2mu107_hv_muic_write_reg(struct s2mu107_muic_data *muic_data,
		u8 reg, u8 value)
{
	u8 before_val, after_val;
	int ret;
	struct i2c_client *i2c;

	if (muic_data == NULL) {
		pr_err("%s data NULL\n", __func__);
		return -1;
	}

	i2c = muic_data->i2c;

	s2mu107_read_reg(i2c, reg, &before_val);
	ret = s2mu107_write_reg(i2c, reg, value);
	s2mu107_read_reg(i2c, reg, &after_val);
	pr_info("%s reg[0x%02x] = [0x%02x] + [0x%02x] -> [0x%02x]\n",
			__func__, reg, before_val, value, after_val);

	return ret;
}

static int s2mu107_hv_muic_read_reg(struct s2mu107_muic_data *muic_data,
		u8 reg)
{
	u8 reg_val = 0;
	struct i2c_client *i2c;

	if (muic_data == NULL) {
		pr_err("%s data NULL\n", __func__);
		return -1;
	}

	i2c = muic_data->i2c;

	s2mu107_read_reg(i2c, reg, &reg_val);

	return reg_val;
}

#if defined(CONFIG_MUIC_SUPPORT_POWERMETER)
static int s2mu107_hv_muic_get_vchgin(struct s2mu107_muic_data *muic_data)
{
	struct power_supply *psy_pm;
	union power_supply_propval val;
	int ret = 0;

	if (muic_data == NULL) {
		pr_err("%s data NULL\n", __func__);
		return -1;
	}
	psy_pm = muic_data->psy_pm;

#if defined(CONFIG_PM_S2MU107)//TEMP
	if (psy_pm) {
		ret = psy_pm->desc->get_property(psy_pm, POWER_SUPPLY_PROP_VCHGIN, &val);
	} else {
		pr_err("%s: Fail to get pmeter\n", __func__);
		return -1;
	}
#endif

	if (ret) {
		pr_err("%s: fail to set power_suppy pmeter property(%d)\n", __func__, ret);
	} else {
		return val.intval;
	}
	return -1;
}
#endif

/**
 * @type: 0 = QC, 1 = AFC (protocol_sw_t)
 */
static void s2mu107_hv_muic_set_protocol_sw(struct s2mu107_muic_data* muic_data,
		int type)
{
	u8 r_val = 0, w_val = 0;

	w_val = r_val = s2mu107_hv_muic_read_reg(muic_data, S2MU107_REG_AFC_LOGIC_CTRL2);
	switch (type) {
	case QC_PROTOCOL:
			w_val &= ~(0x4);
		break;
	case AFC_PROTOCOL:
			w_val |= (0x4);
		break;
	default:
		break;
	}

	if (r_val == w_val)
		return;

	s2mu107_hv_muic_write_reg(muic_data, S2MU107_REG_AFC_LOGIC_CTRL2, w_val);
	msleep(30);
}

static inline void s2mu107_hv_muic_set_afc_tx_data(struct s2mu107_muic_data* muic_data,
		int tx_data)
{
	s2mu107_hv_muic_write_reg(muic_data, S2MU107_REG_AFC_TX_BYTE, tx_data);
}

inline int s2mu107_hv_muic_get_vdnmon_status(struct s2mu107_muic_data* muic_data)
{
	return ((s2mu107_hv_muic_read_reg(muic_data, S2MU107_REG_AFC_STATUS) >> S2MU107_VDNMon_SHIFT) & 0x1);
}

#if defined(CONFIG_MUIC_SUPPORT_POWERMETER)
static int s2mu107_hv_muic_get_vbus_voltage(struct s2mu107_muic_data *muic_data)
{
	struct muic_platform_data *muic_pdata = muic_data->pdata;
	int val = 0, vchgin = 0;

	vchgin = (s2mu107_hv_muic_get_vchgin(muic_data));
	pr_info("%s vchgin:(%d)mV, cable:(%d)\n", __func__, vchgin, muic_pdata->attached_dev);

	val = vchgin / 1000;

	return val;
}
#endif

/**
 * @irq: 1 ~ 7 = AFC_INT_MASK bit, 8 = PM_INT_VCHGIN (afc_int_t)
 * @mask: 1 = Mask, 0 = Not Masked (int_mask_t)
 */
static void s2mu107_hv_muic_irq_mask(struct s2mu107_muic_data* muic_data,
		afc_int_t irq, int_mask_t mask)
{
	u8 reg_val = 0;

	if (AFC_IRQ_VDNMON <= irq && irq <= AFC_IRQ_MRXRDY) {
		reg_val = s2mu107_hv_muic_read_reg(muic_data, S2MU107_REG_AFC_INT_MASK);
		if (mask == ((reg_val >> irq) & 0x1))
			return;

		reg_val &= ~(0x1 << irq);
		reg_val |= (mask << irq);
		s2mu107_hv_muic_write_reg(muic_data, S2MU107_REG_AFC_INT_MASK, reg_val);
	}
}

static void s2mu107_hv_muic_set_qc_voltage(struct s2mu107_muic_data *muic_data,
		int qc_type)
{
	u8 r_val = 0, w_val = 0;

	pr_info("%s\n", __func__);

	w_val = r_val = s2mu107_hv_muic_read_reg(muic_data, S2MU107_REG_AFC_CTRL1);
	w_val &= ~(S2MU107_DNVD_MASK | S2MU107_DPVD_MASK);

	switch (qc_type) {
		case QC_5V:
			w_val |= (S2MU107_DP_0p6V_MASK | S2MU107_DN_GND_MASK);
			break;
		case QC_9V:
			w_val |= (S2MU107_DP_3p3V_MASK | S2MU107_DN_0p6V_MASK);
			break;
		case QC_12V:
			w_val |= (S2MU107_DP_0p6V_MASK | S2MU107_DN_0p6V_MASK);
			break;
		default:
			break;
	}

	if (r_val != w_val)
		s2mu107_hv_muic_write_reg(muic_data, S2MU107_REG_AFC_CTRL1, w_val);
}

static void s2mu107_hv_muic_send_mping(struct s2mu107_muic_data* muic_data)
{
	u8 reg_val = 0;

	msleep(30);

	(muic_data->mping_cnt)++;
	pr_info("%s mping_cnt(%d)\n", __func__, muic_data->mping_cnt);

	reg_val = s2mu107_hv_muic_read_reg(muic_data, S2MU107_REG_AFC_CTRL2);
	reg_val |= S2MU107_MTXEN_MASK;
	s2mu107_hv_muic_write_reg(muic_data, S2MU107_REG_AFC_CTRL2, reg_val);

	cancel_delayed_work(&muic_data->mping_retry_work);
	schedule_delayed_work(&muic_data->mping_retry_work, msecs_to_jiffies(90));
}

static void _s2mu107_hv_muic_reset(struct s2mu107_muic_data *muic_data)
{
	struct muic_platform_data *muic_pdata = muic_data->pdata;

	/* set reg default value */
	s2mu107_hv_muic_write_reg(muic_data, S2MU107_REG_AFC_CTRL1, 0);
	s2mu107_hv_muic_write_reg(muic_data, S2MU107_REG_AFC_CTRL2, 0);
	s2mu107_hv_muic_write_reg(muic_data, S2MU107_REG_AFC_TX_BYTE, 0);
	s2mu107_hv_muic_write_reg(muic_data, S2MU107_REG_AFC_LOGIC_CTRL2, 0x1);
	s2mu107_hv_muic_write_reg(muic_data, S2MU107_REG_LDOADC_VSETL, 0x7c);
	s2mu107_hv_muic_write_reg(muic_data, S2MU107_REG_AFC_INT_MASK, 0);

	muic_pdata->hv_state = HV_STATE_IDLE;
	muic_data->is_dp_drive = false;
	muic_data->is_hvcharger_detected = false;
	muic_data->mrxrdy_cnt = 0;
	muic_data->mping_cnt = 0;
	muic_data->qc_retry_cnt = 0;
	muic_data->qc_retry_wait_cnt = 0;

	cancel_delayed_work(&muic_data->reset_work);
	cancel_delayed_work(&muic_data->mping_retry_work);
	cancel_delayed_work(&muic_data->qc_retry_work);
}

static void _s2mu107_hv_muic_dcp_charger_attach(struct s2mu107_muic_data *muic_data)
{
	struct muic_platform_data *muic_pdata;
#if !IS_ENABLED(CONFIG_SEC_FACTORY) && IS_ENABLED(CONFIG_MUIC_MANAGER)
	struct muic_interface_t *muic_if;
#endif

	if (muic_data == NULL) {
		pr_err("%s data NULL\n", __func__);
		return;
	}
	muic_pdata = muic_data->pdata;
#if !IS_ENABLED(CONFIG_SEC_FACTORY) && IS_ENABLED(CONFIG_MUIC_MANAGER)
	muic_if = (struct muic_interface_t *)muic_data->if_data;
#endif

#if IS_ENABLED(CONFIG_SEC_FACTORY)
	muic_core_hv_state_manager(muic_pdata, HV_TRANS_DCP_DETECTED);
	return;
#else
	if (muic_data->afc_check == false || muic_data->pdata->afc_disable == true) {
		pr_info("%s AFC disable: afc_check(%d), afc_disable(%d)\n",
				__func__, muic_data->afc_check, muic_data->pdata->afc_disable);
		return;
#if IS_ENABLED(CONFIG_MUIC_MANAGER)
	} else if (muic_if->is_afc_pdic_ready == false) {
		pr_info("%s AFC disable(%d) by PDIC\n", __func__, muic_if->is_afc_pdic_ready);
		return;
#endif
	}

	muic_core_hv_state_manager(muic_pdata, HV_TRANS_DCP_DETECTED);
#endif
}

static void s2mu107_if_hv_muic_reset(void *mdata)
{
	struct s2mu107_muic_data *muic_data = (struct s2mu107_muic_data *)mdata;

	_s2mu107_hv_muic_reset(muic_data);
}

static void s2mu107_if_hv_muic_dcp_charger(void *mdata)
{
	struct s2mu107_muic_data *muic_data = (struct s2mu107_muic_data *)mdata;

	pr_info("%s\n", __func__);

	msleep(200);

	/* Enable Afc Block */
	s2mu107_hv_muic_write_reg(muic_data, S2MU107_REG_AFC_CTRL1,
			(S2MU107_AFCEN_MASK | S2MU107_DPDNVDEN_MASK));
	s2mu107_hv_muic_write_reg(muic_data, S2MU107_REG_LDOADC_VSETL, 0x63);
	s2mu107_hv_muic_write_reg(muic_data, S2MU107_REG_AFC_CTRL2,
			(S2MU107_DP06EN_MASK | S2MU107_DNRESEN_MASK));
	muic_data->is_dp_drive = true;
}

static void s2mu107_if_hv_muic_fast_charge_adaptor(void *mdata)
{
	struct s2mu107_muic_data *muic_data = (struct s2mu107_muic_data *)mdata;

	pr_info("%s HV Charger is detected\n", __func__);

	muic_data->is_hvcharger_detected = true;

	s2mu107_hv_muic_irq_mask(muic_data, AFC_IRQ_VDNMON, AFC_MASK);
	s2mu107_hv_muic_irq_mask(muic_data, AFC_IRQ_MPNACK, AFC_MASK);
	s2mu107_hv_muic_set_protocol_sw(muic_data, AFC_PROTOCOL);

	muic_data->tx_data = ((S2MU107_TX_BYTE_9V << 4) | S2MU107_TX_BYTE_1p65A);
	s2mu107_hv_muic_set_afc_tx_data(muic_data, muic_data->tx_data);

	/* 1st mping */
	muic_data->mrxrdy_cnt = 0;
	muic_data->mping_cnt = 0;
	s2mu107_hv_muic_send_mping(muic_data);

	s2mu107_hv_muic_handle_attach(muic_data, ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC);
}

static void s2mu107_if_hv_muic_fast_charge_communication(void *mdata)
{
	struct s2mu107_muic_data *muic_data = (struct s2mu107_muic_data *)mdata;
	struct muic_platform_data *muic_pdata = muic_data->pdata;

	cancel_delayed_work(&muic_data->mping_retry_work);
	pr_info("%s mrxrdy_cnt(%d)\n", __func__, muic_data->mrxrdy_cnt);

	if (muic_pdata->hv_state == HV_STATE_IDLE) {
		pr_info("%s emergency exit\n", __func__);
	} else if (muic_data->mping_cnt < AFC_MPING_RETRY_CNT_LIMIT) {
		s2mu107_hv_muic_send_mping(muic_data);
	} else {
		muic_data->mrxrdy_cnt = 0;
		muic_data->mping_cnt = 0;

		muic_core_hv_state_manager(muic_pdata, HV_TRANS_NO_RESPONSE);
	}
}

static void s2mu107_if_hv_muic_afc_5v_charger(void *mdata)
{
	struct s2mu107_muic_data *muic_data = (struct s2mu107_muic_data *)mdata;

	pr_info("%s\n", __func__);

	s2mu107_hv_muic_handle_attach(muic_data, ATTACHED_DEV_AFC_CHARGER_5V_MUIC);
}

static void s2mu107_if_hv_muic_afc_9v_charger(void *mdata)
{
	struct s2mu107_muic_data *muic_data = (struct s2mu107_muic_data *)mdata;

	cancel_delayed_work(&muic_data->mping_retry_work);
	pr_info("%s\n", __func__);

	msleep(30);

	s2mu107_hv_muic_handle_attach(muic_data, ATTACHED_DEV_AFC_CHARGER_9V_MUIC);
}

static void s2mu107_if_hv_muic_qc_charger(void *mdata)
{
	struct s2mu107_muic_data *muic_data = (struct s2mu107_muic_data *)mdata;

	cancel_delayed_work(&muic_data->mping_retry_work);
	pr_info("%s\n", __func__);

	msleep(30);

	s2mu107_hv_muic_handle_attach(muic_data, ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC);

	s2mu107_hv_muic_set_protocol_sw(muic_data, QC_PROTOCOL);
	s2mu107_hv_muic_set_qc_voltage(muic_data, QC_9V);

	schedule_delayed_work(&muic_data->qc_retry_work, msecs_to_jiffies(150));
}

static void s2mu107_if_hv_muic_qc_5v_charger(void *mdata)
{
	struct s2mu107_muic_data *muic_data = (struct s2mu107_muic_data *)mdata;

	pr_info("%s\n", __func__);

	s2mu107_hv_muic_handle_attach(muic_data, ATTACHED_DEV_QC_CHARGER_5V_MUIC);
}

static void s2mu107_if_hv_muic_qc_9v_charger(void *mdata)
{
	struct s2mu107_muic_data *muic_data = (struct s2mu107_muic_data *)mdata;

	cancel_delayed_work(&muic_data->qc_retry_work);
	pr_info("%s\n", __func__);

	s2mu107_hv_muic_handle_attach(muic_data, ATTACHED_DEV_QC_CHARGER_9V_MUIC);
}

static void s2mu107_hv_muic_handle_attach(struct s2mu107_muic_data* muic_data,
		muic_attached_dev_t new_dev)
{
	pr_info("%s new_dev(%d)\n", __func__, new_dev);

	muic_data->pdata->attached_dev = new_dev;

	switch (new_dev) {
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		MUIC_SEND_NOTI_ATTACH(new_dev);
		break;
	default:
		break;
	}
}

static void s2mu107_hv_muic_set_ready(struct s2mu107_muic_data* muic_data)
{
	struct muic_platform_data *muic_pdata = muic_data->pdata;

	pr_info("%s Enter\n", __func__);
	switch (muic_pdata->attached_dev) {
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC:
		if (muic_pdata->hv_state == HV_STATE_IDLE) {
			mutex_lock(&muic_data->afc_mutex);
			_s2mu107_hv_muic_dcp_charger_attach(muic_data);
			mutex_unlock(&muic_data->afc_mutex);
		}
		break;
	default:
		pr_info("%s Not dcp yet.\n", __func__);
		break;
	}
}

/*
 * MUIC Driver Interface functions
 */
static int s2mu107_if_check_afc_ready(void *mdata)
{
	struct s2mu107_muic_data *muic_data = (struct s2mu107_muic_data *)mdata;

	s2mu107_hv_muic_set_ready(muic_data);

	return 0;
}

static int s2mu107_if_reset_hvcontrol_reg(void *mdata)
{
	struct s2mu107_muic_data *muic_data = (struct s2mu107_muic_data *)mdata;
	struct muic_platform_data *muic_pdata = muic_data->pdata;

	mutex_lock(&muic_data->afc_mutex);
	muic_core_hv_init(muic_pdata);
	mutex_unlock(&muic_data->afc_mutex);

	return 0;
}

static muic_attached_dev_t s2mu107_if_check_id_err(void *mdata,
		muic_attached_dev_t new_dev)
{
	struct s2mu107_muic_data *muic_data = (struct s2mu107_muic_data *)mdata;

	return s2mu107_hv_muic_check_id_err(muic_data, new_dev);
}

static void s2mu107_if_set_afc_ready(void *mdata, bool en)
{
	struct s2mu107_muic_data *muic_data = (struct s2mu107_muic_data *)mdata;

	if (en)
		s2mu107_hv_muic_set_ready(muic_data);
}

#if defined(CONFIG_MUIC_SUPPORT_POWERMETER)
static int s2mu107_if_get_vbus_voltage(void *mdata)
{
	struct s2mu107_muic_data *muic_data = (struct s2mu107_muic_data *)mdata;

	return s2mu107_hv_muic_get_vbus_voltage(muic_data);
}

static int s2mu107_if_vchgin_isr(void *mdata, int voltage)
{
	struct s2mu107_muic_data *muic_data = (struct s2mu107_muic_data *)mdata;
	struct muic_platform_data *muic_pdata = muic_data->pdata;
	int vchgin = 0;

	if (muic_data->is_hvcharger_detected == false)
		return 0;

	mutex_lock(&muic_data->afc_mutex);

	usleep_range(10000, 12000);
	vchgin = s2mu107_hv_muic_get_vchgin(muic_data);
	if (IS_VCHGIN_9V(vchgin)) {
		muic_core_hv_state_manager(muic_pdata, HV_TRANS_VBUS_BOOST);
	} else if (IS_VCHGIN_5V(vchgin)) {
		muic_core_hv_state_manager(muic_pdata, HV_TRANS_VBUS_REDUCE);
	}

	mutex_unlock(&muic_data->afc_mutex);

	return 0;
}
#endif

#ifdef CONFIG_HV_MUIC_VOLTAGE_CTRL
static inline int s2mu107_if_set_afc_voltage(void *mdata, int vol)
{
	return muic_afc_set_voltage(vol);
}

void s2mu107_if_hv_muic_afc_ping(struct s2mu107_muic_data *muic_data)
{
	u8 reg_val;
	int i;

	muic_data->mping_cnt = 0;
	s2mu107_hv_muic_set_afc_tx_data(muic_data, muic_data->tx_data);

	for (i=0; i<4; i++) {
		msleep(50);
		(muic_data->mping_cnt)++;
		reg_val = (S2MU107_MTXEN_MASK | S2MU107_DNRESEN_MASK |
				S2MU107_DP06EN_MASK);
		s2mu107_hv_muic_write_reg(muic_data, S2MU107_REG_AFC_CTRL2, reg_val);
	}
}

static void s2mu107_if_change_afc_voltage(void *mdata, int tx_data)
{
	struct muic_platform_data *muic_pdata = (struct muic_platform_data *)mdata;
	struct s2mu107_muic_data *muic_data = muic_pdata->drv_data;

	pr_info("%s %x\n", __func__, tx_data);

	switch (muic_pdata->attached_dev) {
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		if (tx_data == MUIC_HV_5V)
			s2mu107_hv_muic_set_qc_voltage(muic_data, QC_5V);
		break;
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
		if (tx_data == MUIC_HV_9V)
			s2mu107_hv_muic_set_qc_voltage(muic_data, QC_9V);
		break;
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
		if (tx_data == MUIC_HV_5V) {
			muic_data->tx_data = ((S2MU107_TX_BYTE_5V << 4) | S2MU107_TX_BYTE_1p95A);
			s2mu107_if_hv_muic_afc_ping(muic_data);
		}
		break;
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
		if (tx_data == MUIC_HV_9V) {
			muic_data->tx_data = ((S2MU107_TX_BYTE_9V << 4) | S2MU107_TX_BYTE_1p65A);
			s2mu107_if_hv_muic_afc_ping(muic_data);
		}
		break;
	default:
		break;
	}
}

static int s2mu107_if_afc_get_voltage(void *mdata)
{
	struct muic_platform_data *muic_pdata = (struct muic_platform_data *)mdata;
	struct s2mu107_muic_data *muic_data = muic_pdata->drv_data;
	int ret = -1;

	mutex_lock(&muic_data->afc_mutex);
	switch (muic_pdata->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		ret = 9;
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
		ret = 5;
		break;
	default:
		break;
	}
	pr_info("%s attached_dev(%d)\n", __func__, muic_pdata->attached_dev);
	mutex_unlock(&muic_data->afc_mutex);

	return ret;
}

static int s2mu107_if_afc_set_voltage(void *mdata, int vol)
{
	struct muic_platform_data *muic_pdata = (struct muic_platform_data *)mdata;
	struct s2mu107_muic_data *muic_data = muic_pdata->drv_data;
	int ret;

	mutex_lock(&muic_data->afc_mutex);
	if (vol == 5) {
		s2mu107_if_change_afc_voltage(muic_pdata, MUIC_HV_5V);
		ret = 1;
	} else if (vol == 9) {
		s2mu107_if_change_afc_voltage(muic_pdata, MUIC_HV_9V);
		ret = 1;
	} else {
		pr_warn("%s invalid value\n", __func__);
		ret = 0;
	}
	mutex_unlock(&muic_data->afc_mutex);

	return ret;
}
#endif /* CONFIG_HV_MUIC_VOLTAGE_CTRL */

/*
 * Work queue functions
 */
/* No response - timeout */
static void s2mu107_hv_muic_reset_work(struct work_struct *work)
{
	struct s2mu107_muic_data *muic_data =
	    container_of(work, struct s2mu107_muic_data, reset_work.work);
	struct muic_platform_data *muic_pdata = muic_data->pdata;

	mutex_lock(&muic_data->afc_mutex);
	pr_info("%s\n", __func__);

	muic_core_hv_state_manager(muic_pdata, HV_TRANS_NO_RESPONSE);

	mutex_unlock(&muic_data->afc_mutex);
}

static void s2mu107_hv_muic_mping_retry_work(struct work_struct *work)
{
	struct s2mu107_muic_data *muic_data =
	    container_of(work, struct s2mu107_muic_data, mping_retry_work.work);
	struct muic_platform_data *muic_pdata = muic_data->pdata;
	int vbus = 0;

	mutex_lock(&muic_data->afc_mutex);
	cancel_delayed_work(&muic_data->mping_retry_work);
	vbus = _s2mu107_muic_get_vbus_state(muic_data);

	pr_info("%s vbus(%d)\n", __func__, vbus);
	if (vbus == false) {
		goto no_vbus;
	}

	if (muic_data->mping_cnt < AFC_MPING_RETRY_CNT_LIMIT) {
		s2mu107_hv_muic_send_mping(muic_data);
	} else {
		/* No response about mping by 20 times */
		muic_data->mrxrdy_cnt = 0;
		muic_data->mping_cnt = 0;

		muic_core_hv_state_manager(muic_pdata, HV_TRANS_NO_RESPONSE);
	}
no_vbus:
	mutex_unlock(&muic_data->afc_mutex);
}

static void s2mu107_hv_muic_qc_retry_work(struct work_struct *work)
{
	struct s2mu107_muic_data *muic_data =
	    container_of(work, struct s2mu107_muic_data, qc_retry_work.work);
	struct muic_platform_data *muic_pdata = muic_data->pdata;
	int vbus = 0;
	int vchgin = 0;

	mutex_lock(&muic_data->afc_mutex);
	cancel_delayed_work(&muic_data->qc_retry_work);
	vbus = _s2mu107_muic_get_vbus_state(muic_data);
	pr_info("%s vbus(%d)\n", __func__, vbus);
	if (vbus == false) {
		goto exit;
	}

	msleep(50);
	vchgin = s2mu107_hv_muic_get_vchgin(muic_data);
	if (vchgin >= 5500 && vchgin < 8000) {
		pr_info("%s vchgin(%d) wait_cnt(%d)\n", __func__, vchgin,
				muic_data->qc_retry_wait_cnt);
		if (muic_data->qc_retry_wait_cnt < AFC_QC_RETRY_WAIT_CNT_LIMIT) {
			schedule_delayed_work(&muic_data->qc_retry_work, msecs_to_jiffies(150));
		}
		muic_data->qc_retry_wait_cnt++;
		goto exit;
	} else if (IS_VCHGIN_9V(vchgin)) {
		muic_data->qc_retry_wait_cnt = 0;
		muic_core_hv_state_manager(muic_pdata, HV_TRANS_VBUS_BOOST);
		goto exit;
	}

	muic_data->qc_retry_cnt++;
	if (muic_data->qc_retry_cnt < AFC_QC_RETRY_CNT_LIMIT) {
		s2mu107_hv_muic_set_qc_voltage(muic_data, QC_5V);
		msleep(30);
		s2mu107_hv_muic_set_qc_voltage(muic_data, QC_9V);

		schedule_delayed_work(&muic_data->qc_retry_work, msecs_to_jiffies(150));
	} else {
		muic_core_hv_state_manager(muic_pdata, HV_TRANS_NO_RESPONSE);
	}
exit:
	mutex_unlock(&muic_data->afc_mutex);
}

/*
 * ISR functions
 */
static irqreturn_t s2mu107_hv_muic_vdnmon_isr(int irq, void *data)
{
	struct s2mu107_muic_data *muic_data = data;
	u8 vdnmon = 0;
	struct muic_platform_data *muic_pdata = muic_data->pdata;

	mutex_lock(&muic_data->afc_mutex);

	vdnmon = s2mu107_hv_muic_get_vdnmon_status(muic_data);
	pr_info("%s vdnmon(%s)\n", __func__, (vdnmon ? "High" : "Low"));

	if (muic_data->is_dp_drive && !vdnmon) {
		muic_core_hv_state_manager(muic_pdata, HV_TRANS_VDNMON_LOW);
	}

	mutex_unlock(&muic_data->afc_mutex);

	return IRQ_HANDLED;
}

static irqreturn_t s2mu107_hv_muic_mpnack_isr(int irq, void *data)
{
	struct s2mu107_muic_data *muic_data = data;

	mutex_lock(&muic_data->afc_mutex);
	if (muic_data->is_hvcharger_detected == false) {
		mutex_unlock(&muic_data->afc_mutex);
		return IRQ_NONE;
	}

	pr_info("%s \n", __func__);

	mutex_unlock(&muic_data->afc_mutex);

	return IRQ_HANDLED;
}

static irqreturn_t s2mu107_hv_muic_mrxtrf_isr(int irq, void *data)
{
	pr_info("%s\n", __func__);

	return IRQ_HANDLED;
}

static irqreturn_t s2mu107_hv_muic_mrxrdy_isr(int irq, void *data)
{
	struct s2mu107_muic_data *muic_data = data;
	struct muic_platform_data *muic_pdata = muic_data->pdata;

	mutex_lock(&muic_data->afc_mutex);
	if (muic_data->is_hvcharger_detected == false) {
		mutex_unlock(&muic_data->afc_mutex);
		return IRQ_NONE;
	}

	cancel_delayed_work(&muic_data->mping_retry_work);
	pr_info("%s\n", __func__);

	muic_data->mrxrdy_cnt++;
	muic_core_hv_state_manager(muic_pdata, HV_TRANS_FAST_CHARGE_PING_RESPONSE);

	mutex_unlock(&muic_data->afc_mutex);
	return IRQ_HANDLED;
}

/*
 * Init functions
 */
static int s2mu107_hv_muic_irq_init(struct s2mu107_muic_data *muic_data)
{
	int ret = 0;

	if (muic_data->mfd_pdata && (muic_data->mfd_pdata->irq_base > 0)) {
		int irq_base = muic_data->mfd_pdata->irq_base;

		muic_data->irq_vdnmon = irq_base + S2MU107_AFC_IRQ_VDNMon;
		REQUEST_IRQ(muic_data->irq_vdnmon, muic_data,
			"muic-hv-vdnmon", &s2mu107_hv_muic_vdnmon_isr);

		muic_data->irq_mpnack = irq_base + S2MU107_AFC_IRQ_MPNack;
		REQUEST_IRQ(muic_data->irq_mpnack, muic_data,
			"muic-hv-mpnack", &s2mu107_hv_muic_mpnack_isr);

		muic_data->irq_mrxtrf = irq_base + S2MU107_AFC_IRQ_MRxTrf;
		REQUEST_IRQ(muic_data->irq_mrxtrf, muic_data,
			"muic-hv-mrxtrf", &s2mu107_hv_muic_mrxtrf_isr);

		muic_data->irq_mrxrdy = irq_base + S2MU107_AFC_IRQ_MRxRdy;
		REQUEST_IRQ(muic_data->irq_mrxrdy, muic_data,
			"muic-hv-mrxrdy", &s2mu107_hv_muic_mrxrdy_isr);

		pr_info("%s muic-hv-vdnmon(%d), muic-hv-mpnack(%d)\n",
				__func__, muic_data->irq_vdnmon, muic_data->irq_mpnack);
		pr_info("muic-hv-mrxtrf(%d), muic-hv-mrxrdy(%d)\n",
				muic_data->irq_mrxtrf, muic_data->irq_mrxrdy);
	}

	return ret;
}

void s2mu107_hv_muic_free_irqs(struct s2mu107_muic_data *muic_data)
{
	pr_info("%s\n", __func__);

	FREE_IRQ(muic_data->irq_vdnmon, muic_data, "muic-hv-vdnmon");
	FREE_IRQ(muic_data->irq_mpnack, muic_data, "muic-hv-mpanck");
	FREE_IRQ(muic_data->irq_mrxtrf, muic_data, "muic-hv-mrxtrf");
	FREE_IRQ(muic_data->irq_mrxrdy, muic_data, "muic-hv-mrxrdy");
}

static void s2mu107_hv_muic_reg_init(struct s2mu107_muic_data *muic_data)
{
	/* Temporarliy writing */
	int i;
	u8 reg_val[] = {0x00, 0x40, 0x12, 0x24, 0x80, 0x20, 0x84, 0x00};

	for (i = 0; i <= 7; i++) {
		s2mu107_hv_muic_write_reg(muic_data, (0x64 + i), reg_val[i]);
	}
}

int s2mu107_hv_muic_init(struct s2mu107_muic_data *muic_data)
{
	int ret = 0;
	struct muic_interface_t *muic_if;
	struct muic_platform_data *muic_pdata;

	if (muic_data == NULL) {
		pr_err("%s data NULL\n", __func__);
		return -1;
	}

	pr_info("%s start\n", __func__);

	muic_pdata = muic_data->pdata;
	static_data = muic_data;

	mutex_init(&muic_data->afc_mutex);

	muic_if = muic_data->if_data;
	if (muic_if == NULL) {
		pr_err("%s data NULL\n", __func__);
		return -1;
	}

	muic_if->check_afc_ready = s2mu107_if_check_afc_ready;
	muic_if->reset_hvcontrol_reg = s2mu107_if_reset_hvcontrol_reg;
	muic_if->check_id_err = s2mu107_if_check_id_err;
	muic_if->set_afc_ready = s2mu107_if_set_afc_ready;
#ifdef CONFIG_HV_MUIC_VOLTAGE_CTRL
	muic_if->set_afc_voltage = s2mu107_if_set_afc_voltage;
	muic_if->change_afc_voltage = s2mu107_if_change_afc_voltage;
	muic_if->afc_get_voltage = s2mu107_if_afc_get_voltage;
	muic_if->afc_set_voltage = s2mu107_if_afc_set_voltage;
#endif
	muic_if->hv_reset = s2mu107_if_hv_muic_reset;
	muic_if->hv_dcp_charger = s2mu107_if_hv_muic_dcp_charger;
	muic_if->hv_fast_charge_adaptor = s2mu107_if_hv_muic_fast_charge_adaptor;
	muic_if->hv_fast_charge_communication = s2mu107_if_hv_muic_fast_charge_communication;
	muic_if->hv_afc_5v_charger = s2mu107_if_hv_muic_afc_5v_charger;
	muic_if->hv_afc_9v_charger = s2mu107_if_hv_muic_afc_9v_charger;
	muic_if->hv_qc_charger = s2mu107_if_hv_muic_qc_charger;
	muic_if->hv_qc_5v_charger = s2mu107_if_hv_muic_qc_5v_charger;
	muic_if->hv_qc_9v_charger = s2mu107_if_hv_muic_qc_9v_charger;

#if defined(CONFIG_MUIC_SUPPORT_POWERMETER)
	muic_if->get_vbus_voltage = s2mu107_if_get_vbus_voltage;
	muic_if->pm_chgin_irq = s2mu107_if_vchgin_isr;
#if defined(CONFIG_PM_S2MU107)//TEMP
	muic_data->psy_pm = get_power_supply_by_name("s2mu107-pmeter");
#endif
	if (!muic_data->psy_pm) {
		pr_err("%s: Fail to get pmeter\n", __func__);
	}
#endif

	ret = s2mu107_hv_muic_irq_init(muic_data);
	if (ret < 0) {
		pr_err("%s Failed to initialize HV MUIC irq:%d\n", __func__, ret);
		s2mu107_hv_muic_free_irqs(muic_data);
	}

	INIT_DELAYED_WORK(&muic_data->reset_work, s2mu107_hv_muic_reset_work);
	INIT_DELAYED_WORK(&muic_data->mping_retry_work, s2mu107_hv_muic_mping_retry_work);
	INIT_DELAYED_WORK(&muic_data->qc_retry_work, s2mu107_hv_muic_qc_retry_work);

	muic_core_hv_init(muic_pdata);

	s2mu107_hv_muic_reg_init(muic_data);

	return ret;
}

void s2mu107_hv_muic_remove(struct s2mu107_muic_data *muic_data)
{
	pr_info("%s\n", __func__);

	mutex_destroy(&muic_data->afc_mutex);
	_s2mu107_hv_muic_reset(muic_data);
	s2mu107_hv_muic_free_irqs(muic_data);
}
