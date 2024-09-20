/*
 * s2mu107_direct_charger.c - S2MU107 Charger Driver
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
#include <linux/mfd/samsung/s2mu107.h>
#include <linux/delay.h>
#include "s2mu107_direct_charger.h"
#include "s2mu107_pmeter.h"
#include <linux/time.h>

#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/usb_notify.h>
#endif

#define DEFAULT_ALARM_INTERVAL	10
#define HV_ALARM_INTERVAL	5
#define RUSH_ALARM_INTERVAL	1
#define SLEEP_ALARM_INTERVAL	30

static char *s2mu107_supplied_to[] = {
	"battery",
};

static enum power_supply_property s2mu107_dc_props[] = {
};

static int s2mu107_dc_is_enable(struct s2mu107_dc_data *charger);
static void s2mu107_dc_enable(struct s2mu107_dc_data *charger, int onoff);
static void s2mu107_dc_pm_enable(struct s2mu107_dc_data *charger);
static int s2mu107_dc_get_topoff_current(struct s2mu107_dc_data *charger);
static void s2mu107_dc_set_topoff_current(struct s2mu107_dc_data *charger, int topoff_current);
static int s2mu107_dc_get_float_voltage(struct s2mu107_dc_data *charger);
static void s2mu107_dc_set_float_voltage(struct s2mu107_dc_data *charger, int float_voltage);
static int s2mu107_dc_get_input_current(struct s2mu107_dc_data *charger);
static void s2mu107_dc_set_input_current(struct s2mu107_dc_data *charger, int input_current);
static int s2mu107_dc_get_charging_current(struct s2mu107_dc_data *charger);
static void s2mu107_dc_set_charging_current(struct s2mu107_dc_data *charger,
				int charging_current, s2mu107_dc_cc_mode mode);
static void s2mu107_dc_state_manager(struct s2mu107_dc_data *charger, s2mu107_dc_trans_t trans);
static int s2mu107_dc_get_pmeter(struct s2mu107_dc_data *charger, s2mu107_pmeter_id_t id);
static int s2mu107_dc_pps_isr(struct s2mu107_dc_data *charger, s2mu107_dc_pps_signal_t sig);
static int s2mu107_dc_set_wcin_pwrtr(struct s2mu107_dc_data *charger);
static void s2mu107_dc_forced_enable(struct s2mu107_dc_data *charger, int onoff);
static void s2mu107_dc_set_target_curr(struct s2mu107_dc_data *charger);
static int s2mu107_dc_send_fenced_pps(struct s2mu107_dc_data *charger);
static int s2mu107_dc_get_fg_value(struct s2mu107_dc_data *charger, enum power_supply_property prop);
static int s2mu107_dc_delay(struct s2mu107_dc_data *charger, int delay_ms);
static bool s2mu107_dc_is_wdt_reset(struct s2mu107_dc_data *charger);
static void s2mu107_dc_set_irq_unmask(struct s2mu107_dc_data *charger);
static void s2mu107_dc_state_trans_enqueue(struct s2mu107_dc_data *charger, s2mu107_dc_trans_t trans);
static void s2mu107_dc_set_sc_prop(struct s2mu107_dc_data *charger, enum power_supply_property psp, int val);
static int s2mu107_dc_send_verify(struct s2mu107_dc_data *charger);
static void s2mu107_dc_refresh_auto_pps(struct s2mu107_dc_data *charger, unsigned int vol, unsigned int iin);
static void s2mu107_dc_approach_target_curr_manual(struct s2mu107_dc_data *charger);
static int s2mu107_dc_check_vbat_validity(struct s2mu107_dc_data *charger);
static void s2mu107_dc_vchgin_compensation(struct s2mu107_dc_data *charger);

static const char *pmeter_to_str(s2mu107_pmeter_id_t n)
{
	char *ret;
	switch (n) {
	CONV_STR(PMETER_ID_VCHGIN, ret);
	CONV_STR(PMETER_ID_ICHGIN, ret);
	CONV_STR(PMETER_ID_IWCIN, ret);
	CONV_STR(PMETER_ID_VBATT, ret);
	CONV_STR(PMETER_ID_VSYS, ret);
	CONV_STR(PMETER_ID_TDIE, ret);
	default:
		return "invalid";
	}
	return ret;
}

static const char *state_to_str(s2mu107_dc_state_t n)
{
	char *ret;
	switch (n) {
	CONV_STR(DC_STATE_OFF, ret);
	CONV_STR(DC_STATE_CHECK_VBAT, ret);
	CONV_STR(DC_STATE_PRESET, ret);
	CONV_STR(DC_STATE_START_CC, ret);
	CONV_STR(DC_STATE_CC, ret);
	CONV_STR(DC_STATE_SLEEP_CC, ret);
	CONV_STR(DC_STATE_ADJUSTED_CC, ret);
	CONV_STR(DC_STATE_CV, ret);
	CONV_STR(DC_STATE_WAKEUP_CC, ret);
	CONV_STR(DC_STATE_DONE, ret);
	default:
		return "invalid";
	}
	return ret;
}

static const char *trans_to_str(s2mu107_dc_trans_t n)
{
	char *ret;
	switch (n) {
	CONV_STR(DC_TRANS_INVALID, ret);
	CONV_STR(DC_TRANS_NO_COND, ret);
	CONV_STR(DC_TRANS_VBATT_RETRY, ret);
	CONV_STR(DC_TRANS_WAKEUP_DONE, ret);
	CONV_STR(DC_TRANS_RAMPUP, ret);
	CONV_STR(DC_TRANS_RAMPUP_FINISHED, ret);
	CONV_STR(DC_TRANS_CHG_ENABLED, ret);
	CONV_STR(DC_TRANS_BATTERY_OK, ret);
	CONV_STR(DC_TRANS_BATTERY_NG, ret);
	CONV_STR(DC_TRANS_CHGIN_OKB_INT, ret);
	CONV_STR(DC_TRANS_CC_WAKEUP, ret);
	CONV_STR(DC_TRANS_BAT_OKB_INT, ret);
	CONV_STR(DC_TRANS_NORMAL_CHG_INT, ret);
	CONV_STR(DC_TRANS_DC_DONE_INT, ret);
	CONV_STR(DC_TRANS_CC_STABLED, ret);
	CONV_STR(DC_TRANS_DETACH, ret);
	CONV_STR(DC_TRANS_FAIL_INT, ret);
	CONV_STR(DC_TRANS_FLOAT_VBATT, ret);
	CONV_STR(DC_TRANS_RAMPUP_OVER_VBATT, ret);
	CONV_STR(DC_TRANS_RAMPUP_OVER_CURRENT, ret);
	CONV_STR(DC_TRANS_SET_CURRENT_MAX, ret);
	CONV_STR(DC_TRANS_TOP_OFF_CURRENT, ret);
	CONV_STR(DC_TRANS_DC_OFF_INT, ret);
	default:
		return "invalid";
	}
	return ret;
}

static const char *prop_to_str(enum power_supply_property n)
{
	char *ret;
	switch (n) {
	CONV_STR(POWER_SUPPLY_PROP_VOLTAGE_AVG, ret);
	CONV_STR(POWER_SUPPLY_PROP_CURRENT_AVG, ret);
	CONV_STR(POWER_SUPPLY_PROP_CURRENT_NOW, ret);
	CONV_STR(POWER_SUPPLY_PROP_CAPACITY, ret);
	default:
		return "invalid";
	}
	return ret;
}

static void s2mu107_dc_test_read(struct i2c_client *i2c)
{
	static int reg_list[] = {
                0x0B, 0x0C, 0x0D, 0x0E, 0x17, 0x41, 0x42,
		0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D,
		0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
		0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60, 0x61, 0x62
	};

	u8 data = 0;
	char str[1016] = {0,};
	int i = 0, reg_list_size = 0;

	reg_list_size = ARRAY_SIZE(reg_list);
	for (i = 0; i < reg_list_size; i++) {
                s2mu107_read_reg(i2c, reg_list[i], &data);
                sprintf(str+strlen(str), "0x%02x:0x%02x, ", reg_list[i], data);
        }

        for (i = 0xd7; i < 0xf8; i++) {
		s2mu107_read_reg(i2c, i, &data);
                sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	/* print buffer */
	pr_info("[DC]%s: %s\n", __func__, str);

}

static long s2mu107_dc_update_time_chk(struct s2mu107_dc_data *charger)
{
	struct timeval time;
	long duration;

	do_gettimeofday(&time);
	duration = (long)time.tv_sec - charger->mon_chk_time;
	charger->mon_chk_time = (long)time.tv_sec;
	return duration;
}

static int s2mu107_dc_is_enable(struct s2mu107_dc_data *charger)
{
	/* TODO : DC status? check */
	u8 data;
	int ret;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL0, &data);
	if (ret < 0)
		return ret;

	return (data & DC_EN_MASK) ? 1 : 0;
}

static int s2mu107_dc_delay(struct s2mu107_dc_data *charger, int delay_ms)
{
	int cnt = delay_ms / 50, i = 0;

	for (i = 0; i < cnt; i++) {
		msleep(50);
		if (!charger->is_charging)
			return 1;
	}
	return 0;
}

static void s2mu107_dc_enable(struct s2mu107_dc_data *charger, int onoff)
{
	if (onoff > 0) {
		pr_info("%s, direct charger enable\n", __func__);
		wake_lock(&charger->wake_lock);
		charger->is_charging = true;
		s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL0, S2MU107_DC_ENABLE, DC_EN_MASK);
	} else {
		s2mu107_dc_forced_enable(charger, S2MU107_DC_DISABLE);
		pr_info("%s, direct charger disable\n", __func__);
		s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL0, S2MU107_DC_DISABLE, DC_EN_MASK);
	}
}

static void s2mu107_dc_forced_enable(struct s2mu107_dc_data *charger, int onoff)
{
	if (onoff) {
		pr_info("%s, forced en enable\n", __func__);
		wake_lock(&charger->wake_lock);
		s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL0, DC_FORCED_EN_MASK, DC_FORCED_EN_MASK);
	} else {
		pr_info("%s, forced en disable\n", __func__);
		s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL0, S2MU107_DC_DISABLE, DC_FORCED_EN_MASK);
	}
}

static void s2mu107_dc_platform_state_update(struct s2mu107_dc_data *charger)
{
	union power_supply_propval value;
	struct power_supply *psy;
	int state = -1, ret;

	psy = power_supply_get_by_name(charger->pdata->sec_dc_name);
	if (!psy) {
		pr_err("%s, can't access power_supply", __func__);
		return;
	}
	switch (charger->dc_state) {
	case DC_STATE_OFF:
		state = SEC_DIRECT_CHG_MODE_DIRECT_OFF;
		break;
	case DC_STATE_CHECK_VBAT:
		state = SEC_DIRECT_CHG_MODE_DIRECT_CHECK_VBAT;
		break;
	case DC_STATE_PRESET:
		state = SEC_DIRECT_CHG_MODE_DIRECT_PRESET;
		break;
	case DC_STATE_START_CC:
		state = SEC_DIRECT_CHG_MODE_DIRECT_ON_ADJUST;
		break;
	case DC_STATE_CC:
	case DC_STATE_SLEEP_CC:
	case DC_STATE_CV:
	case DC_STATE_WAKEUP_CC:
	case DC_STATE_ADJUSTED_CC:
		state = SEC_DIRECT_CHG_MODE_DIRECT_ON;
		break;
	case DC_STATE_DONE:
		state = SEC_DIRECT_CHG_MODE_DIRECT_DONE;
		break;
	default:
		break;
	}

	if (state < 0) {
		pr_err("%s: Invalid state report.\n", __func__);
		return;
	}

	pr_info("%s, updated state : %d\n", __func__, state);
	ret = power_supply_get_property(psy, (enum power_supply_property)POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	if (value.intval != state) {
		value.intval = state;
		ret = power_supply_set_property(psy,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE, &value);
	}
}

static void s2mu107_dc_pm_enable(struct s2mu107_dc_data *charger)
{
	union power_supply_propval value;

        value.intval = PM_TYPE_VCHGIN | PM_TYPE_VBAT | PM_TYPE_ICHGIN | PM_TYPE_TDIE |
                PM_TYPE_VWCIN | PM_TYPE_IWCIN | PM_TYPE_VBYP | PM_TYPE_VSYS |
                PM_TYPE_VCC1 | PM_TYPE_VCC2 | PM_TYPE_ICHGIN | PM_TYPE_ITX;
	psy_do_property(charger->pdata->pm_name, set,
				POWER_SUPPLY_PROP_CO_ENABLE, value);
}

static int s2mu107_dc_set_comm_fail(struct s2mu107_dc_data *charger)
{
	u8 data;
	int ret = 0;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL22, &data);
	if (ret < 0)
		return ret;

	data |= TA_COMMUNICATION_FAIL_MASK;
	pr_info("%s, \n", __func__);

	s2mu107_write_reg(charger->i2c, S2MU107_DC_CTRL22, data);

	return 0;
}

static int s2mu107_dc_get_topoff_current(struct s2mu107_dc_data *charger)
{
	u8 data = 0x00;
	int ret, topoff_current = 0;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL15, &data);
	if (ret < 0)
		return ret;
	data = (data & DC_TOPOFF_MASK) >> DC_TOPOFF_SHIFT;

	if (data > 0x0F)
		data = 0x0F;

	topoff_current = data * 100 + 500;
	pr_info("%s, topoff_current : %d(0x%2x)\n", __func__, topoff_current, data);

	return topoff_current;
}

static void s2mu107_dc_set_topoff_current(struct s2mu107_dc_data *charger, int topoff_current)
{
	u8 data = 0x00;

	if (topoff_current <= 500)
		data = 0x00;
	else if (topoff_current > 500 && topoff_current <= 2000)
		data = (topoff_current - 500) / 100;
	else
		data = 0x0F;

	pr_info("%s, topoff_current : %d(0x%2x)\n", __func__, topoff_current, data);

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL15,
				data << DC_TOPOFF_SHIFT, DC_TOPOFF_MASK);
}

static int s2mu107_dc_set_cc_band_width(struct s2mu107_dc_data *charger, int band_current)
{
	u8 target;

	if (band_current < DC_CC_BAND_WIDTH_MIN_MA) {
		pr_err("%s, band_current : %d\n", __func__, band_current);
		return -1;
	}

	target = (band_current - DC_CC_BAND_WIDTH_MIN_MA) / DC_CC_BAND_WIDTH_STEP_MA;

	pr_info("%s, band_current : %d(0x%2x)\n", __func__, band_current, target);

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL15,
				target, DC_CC_BAND_WIDTH_MASK);
	return 0;
}

static int s2mu107_dc_get_float_voltage(struct s2mu107_dc_data *charger)
{
	u8 data = 0x00;
	int ret, float_voltage = 0;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL1, &data);
	if (ret < 0)
		return data;

	data = data & SET_CV_MASK;

	if (data >= 0x7F)
		float_voltage = 4870;
	else if (data <= 0x00)
		float_voltage = 3600;
	else
		float_voltage = data * 10 + 3600;

	pr_info("%s, float_voltage : %d(0x%2x)\n", __func__, float_voltage, data);

	return float_voltage;
}

static void s2mu107_dc_set_float_voltage(struct s2mu107_dc_data *charger, int float_voltage)
{
	u8 data = 0x00;

	if (float_voltage <= 3600)
		data = 0x00;
	else if (float_voltage > 3600 && float_voltage <= 4870)
		data = (float_voltage - 3600) / 10;
	else
		data = 0x7F;

	pr_info("%s, float_voltage : %d(0x%2x)\n", __func__, float_voltage, data);
	charger->floatVol = float_voltage;

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL1,
				data << SET_CV_SHIFT, SET_CV_MASK);
}

static int s2mu107_dc_get_chgin_input_current(struct s2mu107_dc_data *charger)
{
	u8 data = 0x00;
	int ret, input_current = 0;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL2, &data);
	if (ret < 0)
		return data;
	data = data & DC_SET_CHGIN_ICHG_MASK;

	input_current = data * 50 + 50;

	pr_info("%s,chgin input_current : %d(0x%2x)\n", __func__, input_current, data);
	return input_current;
}

static int s2mu107_dc_get_wcin_input_current(struct s2mu107_dc_data *charger)
{
	u8 data = 0x00;
	int ret, input_current = 0;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL4, &data);
	if (ret < 0)
		return data;
	data = data & DC_SET_WCIN_ICHG_MASK;

	input_current = data * 50 + 50;

	pr_info("%s,wcin input_current : %d(0x%2x)\n", __func__, input_current, data);
	return input_current;
}

static int s2mu107_dc_get_input_current(struct s2mu107_dc_data *charger)
{
	int chgin_current = 0, wcin_current = 0, input_current = 0;

	chgin_current = s2mu107_dc_get_chgin_input_current(charger);
	wcin_current = s2mu107_dc_get_wcin_input_current(charger);

	input_current = chgin_current + wcin_current;

	pr_info("%s, input_current : %d\n", __func__, input_current);
	return input_current;
}

static void s2mu107_dc_set_chgin_input_current(struct s2mu107_dc_data *charger, int input_current)
{
	u8 data = 0x00;
	data = (input_current - 50) / 50;
	if (data >= 0x7F)
		data = 0x7E;
	pr_info("%s, chgin input_current : %d(0x%2x)\n", __func__, input_current, data);

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL2,
				data << DC_SET_CHGIN_ICHG_SHIFT, DC_SET_CHGIN_ICHG_MASK);
}

static void s2mu107_dc_set_wcin_input_current(struct s2mu107_dc_data *charger, int input_current)
{
	u8 data = 0x00;
	data = (input_current - 50) / 50;
	if (data == 0x7F)
		data = 0x7E;

	pr_info("%s, wcin input_current : %d(0x%2x)\n", __func__, input_current, data);

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL4,
				data << DC_SET_WCIN_ICHG_SHIFT, DC_SET_WCIN_ICHG_MASK);
}

static void s2mu107_dc_set_input_current(struct s2mu107_dc_data *charger, int input_current)
{
	int chgin_current = 0, wcin_current = 0;

	if (input_current < 500)
		input_current = 500;
//	else if (input_current > 6000)
//		input_current = 6000;

	chgin_current = input_current / 2;
	wcin_current = input_current - chgin_current;

	pr_info("%s, chgin : %d, wcin : %d\n", __func__, chgin_current, wcin_current);

	s2mu107_dc_set_chgin_input_current(charger, chgin_current);
	s2mu107_dc_set_wcin_input_current(charger, wcin_current);
}

static int s2mu107_dc_get_charging_current(struct s2mu107_dc_data *charger)
{
	u8 data = 0x00;
	int ret, charging_current = 0;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL13, &data);
	if (ret < 0)
		return data;
	data = data & DC_SET_CC_MASK;

	if (data >= 0x63)
		charging_current = 10000;
	else if (data <= 0x00)
		charging_current = 100;
	else
		charging_current = data * 100 + 100;

	pr_info("%s, charging_current : %d(0x%2x)\n", __func__, charging_current, data);
	return charging_current;
}

static void s2mu107_dc_set_charging_current(struct s2mu107_dc_data *charger, int charging_current, s2mu107_dc_cc_mode mode)
{
	u8 data = 0x00;

	if (mode == S2MU107_DC_MODE_TA_CC)
		charging_current += 500;

	if (charging_current <= 100)
		data = 0x00;
	else if (charging_current > 100 && charging_current < 10000)
		data = (charging_current - 100) / 100;
	else
		data = 0x63;

	pr_info("%s, charging_current : %d(0x%2x)\n", __func__, charging_current, data);

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL13,
				data << DC_SET_CC_SHIFT, DC_SET_CC_MASK);
}

static void s2mu107_dc_set_vsys_dcmode(struct s2mu107_dc_data *charger, s2mu107_dc_ctrl_t en)
{
	pr_info("%s, en : %d\n", __func__, (int)en);

	switch (en) {
	case S2MU107_DC_ENABLE:
		if (charger->rev_id <= 1) {
			/* 0x1C[5:0] = 11_1111 (CHGIN IVR, WCIN IVR level to 4.7V) */
			s2mu107_update_reg(charger->i2c, S2MU107_SC_CTRL4_DC,
					0x7 << SET_IVR_CHGIN_SHIFT, SET_IVR_CHGIN_MASK);
			s2mu107_update_reg(charger->i2c, S2MU107_SC_CTRL4_DC,
					0x7 << SET_IVR_WCIN_SHIFT, SET_IVR_WCIN_MASK);
		}

		if (charger->rev_id == 0) {
			s2mu107_write_reg(charger->i2c, 0xCC, 0x7F);
			usleep_range(1000, 1100);
			s2mu107_write_reg(charger->i2c, 0xC6, 0x3F);
			usleep_range(1000, 1100);
			s2mu107_update_reg(charger->i2c, 0x20, 0x9, 0xF);
		} else {
			/*
			 * 0x7A  CC[3:0] = 4'b1001
			 * (SET_SYS_OVP_F[1:0] / SET_SYS_OVP_R[1:0] = 4.6V / 4.7V)
			 */
			s2mu107_update_reg(charger->i2c, S2MU107_SC_OTP_96,
					0x2 << SET_VSYS_OVP_FALLING_SHIFT, SET_VSYS_OVP_FALLING_MASK);
			s2mu107_update_reg(charger->i2c, S2MU107_SC_OTP_96,
					0x1 << SET_VSYS_OVP_RISING_SHIFT, SET_VSYS_OVP_RISING_MASK);

			usleep_range(1000, 1100);

			/*
			 * 0x7A20[3:0] = 4'b0110
			 * (SET_VF_VSYS[3:0] = 4.5V)
			 */
			s2mu107_update_reg(charger->i2c, S2MU107_SC_CTRL8_DC,
				0x9 << SET_VF_VSYS_SHIFT, SET_VF_VSYS_MASK);
		}
		break;
	case S2MU107_DC_DISABLE:
	default:
		if (charger->rev_id == 0) {
			s2mu107_update_reg(charger->i2c, 0x20, 0x8, 0xF);
			usleep_range(1000, 1100);
			s2mu107_write_reg(charger->i2c, 0xC6, 0x20);
			usleep_range(1000, 1100);
			s2mu107_write_reg(charger->i2c, 0xCC, 0x20);
		} else {
			/*
			 * SET_VF_VSYS[3:0] = 4.4V
			 */
			s2mu107_update_reg(charger->i2c, S2MU107_SC_CTRL8_DC,
					0x8 << SET_VF_VSYS_SHIFT, SET_VF_VSYS_MASK);
			usleep_range(1000, 1100);
			/*
			 * SET_SYS_OVP_F[1:0] / SET_SYS_OVP_R[1:0] = 4.5V / 4.6V
			 */
			s2mu107_update_reg(charger->i2c, S2MU107_SC_OTP_96,
					0x1 << SET_VSYS_OVP_FALLING_SHIFT, SET_VSYS_OVP_FALLING_MASK);
			s2mu107_update_reg(charger->i2c, S2MU107_SC_OTP_96,
					0x0 << SET_VSYS_OVP_RISING_SHIFT, SET_VSYS_OVP_RISING_MASK);
		}

		if (charger->rev_id <= 1) {
			/* 0x1C[5:0] = 10_1101 (CHGIN IVR, WCIN IVR level to 4.5V) */
			s2mu107_update_reg(charger->i2c, S2MU107_SC_CTRL4_DC,
					0x5 << SET_IVR_CHGIN_SHIFT, SET_IVR_CHGIN_MASK);
			s2mu107_update_reg(charger->i2c, S2MU107_SC_CTRL4_DC,
					0x5 << SET_IVR_WCIN_SHIFT, SET_IVR_WCIN_MASK);
		}
		break;
	}
}

static void s2mu107_dc_set_dual_buck(struct s2mu107_dc_data *charger, s2mu107_dc_ctrl_t en)
{
	union power_supply_propval value;

	pr_info("%s, en : %d\n", __func__, (int)en);
	switch (en) {
	case S2MU107_DC_ENABLE:
		value.intval = 1;
		charger->dc_sc_status = S2MU107_DC_SC_STATUS_DUAL_BUCK;
		psy_do_property(charger->pdata->sc_name, set,
			POWER_SUPPLY_EXT_PROP_CHARGE_MODE, value);
		break;
	default:
		break;
	}
}

static void s2mu107_dc_set_iin_off(struct s2mu107_dc_data *charger, int iin_off)
{
	u8 data = 0x00;

	if (iin_off <= SET_IIN_OFF_MIN_MA)
		data = 0x00;
	else if (iin_off > SET_IIN_OFF_MIN_MA && iin_off <= SET_IIN_OFF_MAX_MA)
		data = (iin_off - SET_IIN_OFF_MIN_MA) / SET_IIN_OFF_STEP_MA;
	else
		data = SET_IIN_OFF_MASK;

	pr_info("%s, iin_off : %d(0x%2x)\n", __func__, iin_off, data);

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CD_OTP_01,
				data << SET_IIN_OFF_SHIFT, SET_IIN_OFF_MASK);
}

static void s2mu107_dc_set_irq_unmask(struct s2mu107_dc_data *charger)
{
	s2mu107_write_reg(charger->i2c, 0xB, 0x0);
	s2mu107_write_reg(charger->i2c, 0xC, 0x20);
	s2mu107_write_reg(charger->i2c, 0xD, 0x0);
	s2mu107_write_reg(charger->i2c, 0xE, 0x0);
}

static int s2mu107_dc_fg_prop_param(enum power_supply_property prop)
{
	if (prop == POWER_SUPPLY_PROP_VOLTAGE_AVG)
		return SEC_BATTERY_VOLTAGE_AVERAGE;
	else if (prop == POWER_SUPPLY_PROP_CURRENT_AVG
		|| prop == POWER_SUPPLY_PROP_CURRENT_NOW)
		return SEC_BATTERY_CURRENT_MA;
	else
		return 0;
}

static int s2mu107_dc_get_fg_value(struct s2mu107_dc_data *charger, enum power_supply_property prop)
{
	union power_supply_propval value;
	u32 get = 0;
	int ret;

	if (!charger->psy_fg)
		return -EINVAL;
	value.intval = s2mu107_dc_fg_prop_param(prop);
	ret = power_supply_get_property(charger->psy_fg, prop, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	get = value.intval;
	pr_info("%s, %s : (%d)\n", __func__, prop_to_str(prop), get);
	return get;
}

static void s2mu107_dc_wdt_clear(struct s2mu107_dc_data *charger)
{
	/* wdt clear */
	s2mu107_update_reg(charger->i2c, S2MU107_SC_CTRL12_DC,
		0x1 << WDT_CLR_SHIFT, WDT_CLR_MASK);
}

static bool s2mu107_dc_is_wdt_reset(struct s2mu107_dc_data *charger)
{
	u8 data, chg_fault_status;
	int ret;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_SC_STATUS1_DC, &data);
	if (ret < 0)
		pr_err("%s: Fail to readback\n", __func__);

	chg_fault_status = (data & SC_FAULT_STATUS_MASK) >> SC_FAULT_STATUS_SHIFT;

	pr_info("%s, fault status : 0x%X\n", __func__, chg_fault_status);
	if (chg_fault_status & (SC_STATUS_WD_SUSPEND | SC_STATUS_WD_RST))
		return true;

	return false;
}

static int s2mu107_dc_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2mu107_dc_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	union power_supply_propval value;
	int ret = 0;
	int data = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->is_charging;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = charger->dc_chg_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		/* TODO : health check */
		pr_info("%s: health check & wdt clear\n", __func__);
		s2mu107_dc_wdt_clear(charger);
		if (charger->ps_health == POWER_SUPPLY_HEALTH_UNKNOWN)
			charger->ps_health = POWER_SUPPLY_HEALTH_GOOD;
		val->intval = charger->ps_health;
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = s2mu107_dc_is_enable(charger);
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		val->intval = s2mu107_dc_get_input_current(charger);
		break;
	case POWER_SUPPLY_PROP_TEMP:
		/* FG temp? PM temp? */
		if (!charger->psy_fg)
			return -EINVAL;
		ret = power_supply_get_property(charger->psy_fg, POWER_SUPPLY_PROP_TEMP, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);

		val->intval = value.intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = charger->step_iin_ma;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = charger->step_iin_ma * 2;
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:

		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			pr_info("%s: mon context triggered from the platform drv\n", __func__);
			//cancel_delayed_work(&charger->dc_monitor_work);
			//schedule_delayed_work(&charger->dc_monitor_work, msecs_to_jiffies(0));
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE:
			switch (charger->dc_state) {
				case DC_STATE_OFF:
					val->intval = SEC_DIRECT_CHG_MODE_DIRECT_OFF;
					break;
				case DC_STATE_CHECK_VBAT:
					val->intval = SEC_DIRECT_CHG_MODE_DIRECT_CHECK_VBAT;
					break;
				case DC_STATE_PRESET:
					val->intval = SEC_DIRECT_CHG_MODE_DIRECT_PRESET;
					break;
				case DC_STATE_START_CC:
					val->intval = SEC_DIRECT_CHG_MODE_DIRECT_ON_ADJUST;
					break;
				case DC_STATE_CC:
				case DC_STATE_SLEEP_CC:
				case DC_STATE_CV:
				case DC_STATE_WAKEUP_CC:
				case DC_STATE_ADJUSTED_CC:
					val->intval = SEC_DIRECT_CHG_MODE_DIRECT_ON;
					break;
				case DC_STATE_DONE:
					val->intval = SEC_DIRECT_CHG_MODE_DIRECT_DONE;
					break;
				default:
					val->intval = SEC_DIRECT_CHG_MODE_DIRECT_OFF;
					break;
			}
			break;
		case POWER_SUPPLY_EXT_PROP_MEASURE_INPUT:
			switch (val->intval) {
			case SEC_BATTERY_IIN_MA:
				data = s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN) +
					s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);
				pr_info("%s, IIN_MA : %d\n", __func__, data);
				break;
			case SEC_BATTERY_IIN_UA:
				data = s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN) +
					s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);
				data = data * 1000;
				pr_info("%s, IIN_UA : %d\n", __func__, data);
				break;
			case SEC_BATTERY_VIN_MA:
				data = s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);
				pr_info("%s, VIN_MV : %d\n", __func__, data);
				break;
			case SEC_BATTERY_VIN_UA:
				data = s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN) * 1000;
				pr_info("%s, VIN_UV : %d\n", __func__, data);
				break;
			default:
				data = 0;
				break;
			}
			val->intval = data;
			break;

		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mu107_dc_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mu107_dc_data *charger = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
//	union power_supply_propval value;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		/*
		 * todo :
		 * need to change top off current
		 * 1000 -> 500 mA by IIN.
		 */
		if (val->intval <= 1000)
			charger->step_iin_ma = 1050;
		else
			charger->step_iin_ma = val->intval;
		pr_info("%s, set iin : %d", __func__, charger->step_iin_ma);
		s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_SET_CURRENT_MAX);
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		/* Not Taking charging current orders from platform */
		//charger->charging_current = val->intval;
		//s2mu107_dc_set_charging_current(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		if (val->intval == SEC_BAT_CHG_MODE_BUCK_OFF || val->intval == SEC_BAT_CHG_MODE_CHARGING_OFF) {
			charger->is_charging = false;
			if (charger->dc_state != DC_STATE_OFF)
				s2mu107_dc_state_manager(charger, DC_TRANS_DETACH);
		} else if (val->intval == SEC_BAT_CHG_MODE_CHARGING) {
			if (charger->cable_type || !s2mu107_dc_is_wdt_reset(charger)) {
				charger->is_charging = true;
				charger->ps_health = POWER_SUPPLY_HEALTH_GOOD;
				s2mu107_dc_state_manager(charger, DC_TRANS_CHG_ENABLED);
			}
		} else {
			pr_info("%s, chg en w/ unexpected parameter : %d", __func__, val->intval);
		}
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_DIRECT_PPS:
			/* PPS INT from the pdic */
			s2mu107_dc_pps_isr(charger, ((s2mu107_dc_pps_signal_t)val->intval));
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_PPS_FAILED:
			/* PS RDY Timeout case */
			pr_info("%s, PS RDY Timeout case", __func__);
			s2mu107_dc_set_comm_fail(charger);
			s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_FAIL_INT);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_WDT_CONTROL:
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_VOLTAGE_MAX:
			charger->step_vbatt_mv = val->intval;
			pr_info("%s, set voltage max : %d", __func__, charger->step_vbatt_mv);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_CURRENT_MAX:
			/*
			 * todo :
			 * need to change top off current
			 * 1000 -> 500 mA by IIN.
			 */
			if (val->intval <= 1000)
				charger->step_iin_ma = 1050;
			else
				charger->step_iin_ma = val->intval;
			pr_info("%s, set iin : %d", __func__, charger->step_iin_ma);
			s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_SET_CURRENT_MAX);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_PPS_READY:
			pr_info("%s, pps ready comes", __func__);
			charger->is_pps_ready = true;
			wake_up_interruptible(&charger->wait);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_DETACHED:
			//s2mu107_dc_state_manager(charger, DC_TRANS_DETACH);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_HARD_RESET:
			pr_info("%s, hard reset comes", __func__);
			s2mu107_dc_enable(charger, S2MU107_DC_DISABLE);
			charger->ps_health = POWER_SUPPLY_HEALTH_DC_ERR;
			s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_FAIL_INT);
			break;
		case POWER_SUPPLY_EXT_PROP_DIRECT_PPS_DISABLE:
			charger->is_autopps_disabled = true;
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void s2mu107_dc_cal_target_value(struct s2mu107_dc_data *charger)
{
	unsigned int vBatt;
	unsigned int temp;
	int i, sum = 0;

	if ((charger->vchgin_okb_retry * DC_TA_VOL_STEP_MV) > 200) {
		charger->vchgin_okb_retry = 0;
	}

	for (i = 0; i < 3; i++) {
		sum += s2mu107_dc_get_pmeter(charger, PMETER_ID_VBATT);
		usleep_range(20000, 21000);
	}
	vBatt = sum / 3;
	temp = vBatt * 2 * (1031);
	charger->ppsVol = (((temp / 1000) / DC_TA_VOL_STEP_MV) * DC_TA_VOL_STEP_MV) + 200 + (charger->vchgin_okb_retry * DC_TA_VOL_STEP_MV);

	if (charger->ppsVol > charger->pd_data->taMaxVol) {
		charger->ppsVol = charger->pd_data->taMaxVol;
	}

	pr_info("%s ppsVol : %d, vchgin_okb_retry : %d\n",
		__func__, charger->ppsVol, charger->vchgin_okb_retry);
}

static int s2mu107_dc_get_pmeter(struct s2mu107_dc_data *charger, s2mu107_pmeter_id_t id)
{
	struct power_supply *psy_pm;
	union power_supply_propval value;
	int ret = 0;

	psy_pm = power_supply_get_by_name(charger->pdata->pm_name);
	if (!psy_pm)
		return -EINVAL;

	switch (id) {
		case PMETER_ID_VCHGIN:
			ret = power_supply_get_property(psy_pm, POWER_SUPPLY_PROP_VCHGIN, &value);
			break;
		case PMETER_ID_ICHGIN:
			ret = power_supply_get_property(psy_pm, POWER_SUPPLY_PROP_ICHGIN, &value);
			break;
		case PMETER_ID_IWCIN:
			ret = power_supply_get_property(psy_pm, POWER_SUPPLY_PROP_IWCIN, &value);
			break;
		case PMETER_ID_VBATT:
			ret = power_supply_get_property(psy_pm, POWER_SUPPLY_PROP_VBAT, &value);
			break;
		case PMETER_ID_VSYS:
			ret = power_supply_get_property(psy_pm, POWER_SUPPLY_PROP_VSYS, &value);
			break;
		case PMETER_ID_TDIE:
			ret = power_supply_get_property(psy_pm, POWER_SUPPLY_PROP_TDIE, &value);
			break;
		default:
			break;
	}

	if (ret < 0) {
		pr_err("%s: fail to set power_suppy pmeter property(%d)\n",
				__func__, ret);
	} else {
		charger->pmeter[id] = value.intval;
		pr_info("%s pm[%s] : %d\n", __func__, pmeter_to_str(id), value.intval);
		return value.intval;
	}
	return -1;
}

static void s2mu107_dc_set_fg_iavg(struct s2mu107_dc_data *charger, int onoff)
{
	union power_supply_propval value;
	int ret;

	if (!charger->psy_fg)
		return;

	if (onoff > 0)
		value.intval = S2MU107_DC_ENABLE;
	else
		value.intval = S2MU107_DC_DISABLE;

	ret = power_supply_set_property(charger->psy_fg, POWER_SUPPLY_PROP_FAST_IAVG, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
}

static void s2mu107_dc_set_sc_prop(struct s2mu107_dc_data *charger, enum power_supply_property psp, int val)
{
	union power_supply_propval value;
	int ret;
	if (!charger->psy_sc)
		return;
	value.intval = val;
	pr_info("%s psp : %d\n", __func__, (int)psp);
	ret = power_supply_set_property(charger->psy_sc, psp, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
}

static void s2mu107_dc_vchgin_compensation(struct s2mu107_dc_data *charger)
{
	int vdiff;
	int vchgin = s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);

	pr_info("%s enter, pps vol : %d, vchgin : %d\n", __func__, charger->ppsVol, vchgin);
	if (vchgin > charger->ppsVol + 150) {
		vdiff = ((vchgin - charger->ppsVol) / DC_TA_VOL_STEP_MV) * DC_TA_VOL_STEP_MV;
		charger->ppsVol -= vdiff;
		s2mu107_dc_send_verify(charger);
	} else if (vchgin < charger->ppsVol - 150) {
		vdiff = ((charger->ppsVol - vchgin) / DC_TA_VOL_STEP_MV) * DC_TA_VOL_STEP_MV;
		charger->ppsVol += vdiff;
		s2mu107_dc_send_verify(charger);
	} else {
		pr_info("%s ok, pps vol : %d, vchgin : %d\n", __func__, charger->ppsVol, vchgin);
	}
	s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);
}

static int s2mu107_dc_check_vbat_validity(struct s2mu107_dc_data *charger)
{
	int vBatt = s2mu107_dc_get_pmeter(charger, PMETER_ID_VBATT);
	if (vBatt >= 3450) {
		charger->chk_vbat_margin = 0;
		return 0;
	} else {
		charger->chk_vbat_margin = 100;
		return 1;
	}
}

static void s2mu107_dc_set_target_curr(struct s2mu107_dc_data *charger)
{
	if (charger->step_iin_ma > charger->pd_data->taMaxCur)
		charger->step_iin_ma = charger->pd_data->taMaxCur;

	pr_info("%s step_iin_ma : %d\n", __func__, charger->step_iin_ma);
	charger->targetIIN = ((charger->step_iin_ma / DC_TA_CURR_STEP_MA) * DC_TA_CURR_STEP_MA) + DC_TA_CURR_STEP_MA;
	s2mu107_dc_set_charging_current(charger, (charger->targetIIN * 2), S2MU107_DC_MODE_TA_CC);
	return;
}

static void s2mu107_dc_refresh_auto_pps(struct s2mu107_dc_data *charger, unsigned int vol, unsigned int iin)
{
	mutex_lock(&charger->auto_pps_mutex);
	charger->ppsVol = vol;
	charger->targetIIN = iin;

	pr_info("%s vol : %d, iin : %d\n", __func__, vol, iin);
	sec_pps_enable(charger->pd_data->pdo_pos,
		charger->ppsVol, charger->targetIIN, S2MU107_DC_DISABLE);

	if (s2mu107_dc_delay(charger, 500))
		goto CANCEL_AUTO_PPS;

	charger->ppsVol = vol;
	charger->targetIIN = iin;
	s2mu107_dc_send_fenced_pps(charger);

	if (s2mu107_dc_delay(charger, 500))
		goto CANCEL_AUTO_PPS;

	charger->is_autopps_disabled = false;
	sec_pps_enable(charger->pd_data->pdo_pos,
		charger->ppsVol, charger->targetIIN, S2MU107_DC_ENABLE);
CANCEL_AUTO_PPS:
	mutex_unlock(&charger->auto_pps_mutex);
}

static void s2mu107_dc_approach_target_curr_manual(struct s2mu107_dc_data *charger)
{
	int wait_ret, try_cnt = 0, iin;

	if (charger->step_iin_ma <= 1050)
		return;

	charger->is_rampup_adjusted = true;
	charger->ppsVol = charger->pd_data->taMaxVol;
	wait_ret = s2mu107_dc_send_verify(charger);
	msleep(50);
	iin = s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN) +
		s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);

	if (iin > charger->step_iin_ma) {
		while((try_cnt++ < 20) && (charger->targetIIN > DC_TOPOFF_CURRENT_MA)) {
			msleep(100);
			iin = s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN) +
				s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);
			pr_info("%s Reduce iin : %d\n", __func__, iin);		

			if (iin > charger->step_iin_ma + 50) {
				charger->targetIIN -= DC_TA_CURR_STEP_MA;
				wait_ret = s2mu107_dc_send_verify(charger);
			} else
				break;

			if (!charger->is_charging)
				return;
		}
	} else {
		while((try_cnt++ < 20) && (charger->targetIIN < charger->pd_data->taMaxCur)) {
			msleep(100);
			iin = s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN) +
				s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);
			pr_info("%s Raise iin : %d\n", __func__, iin);		

			if (iin < charger->step_iin_ma) {
				charger->targetIIN += DC_TA_CURR_STEP_MA;
				wait_ret = s2mu107_dc_send_verify(charger);
			} else
				break;

			if (!charger->is_charging)
				return;
		}
	}
	try_cnt = 0;
	iin = s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN) +
		s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);
	if ((charger->ta_pwr_type == TA_PWR_TYPE_25W)
		&& (charger->step_iin_ma >= charger->pd_data->taMaxCur)
		&& (iin < charger->step_iin_ma - 50)) {
		while(try_cnt++ < 40) {
			iin = s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN) +
				s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);
			pr_info("%s Raise iin by Reduce Vol : %d\n", __func__, iin);

			if (iin < charger->step_iin_ma - 50) {
				charger->ppsVol -= DC_TA_VOL_STEP_MV;
				wait_ret = s2mu107_dc_send_verify(charger);
			} else if (iin < charger->step_iin_ma) {
				charger->ppsVol -= DC_TA_VOL_STEP_MV * 2;
				wait_ret = s2mu107_dc_send_verify(charger);
			} else
				break;

			if (!charger->is_charging)
				return;
		}
	}
}

static void s2mu107_dc_state_manager(struct s2mu107_dc_data *charger, s2mu107_dc_trans_t trans)
{
	s2mu107_dc_state_t next_state = DC_STATE_OFF;
	mutex_lock(&charger->dc_state_mutex);
	wake_lock(&charger->state_manager_wake);

	pr_info("%s: %s --> %s", __func__,
		state_to_str(charger->dc_state), trans_to_str(trans));

	if (trans == DC_TRANS_DETACH || trans == DC_TRANS_FAIL_INT || trans == DC_TRANS_DC_OFF_INT) {
		if (charger->dc_state != DC_STATE_OFF) {
			next_state = DC_STATE_OFF;
			goto HNDL_DC_OFF;
		} else {
			goto SKIP_HNDL_DC_OFF;
		}
	}

	if (trans == DC_TRANS_DC_DONE_INT || trans == DC_TRANS_TOP_OFF_CURRENT) {
		if (charger->dc_state != DC_STATE_DONE) {
			next_state = DC_STATE_DONE;
			goto HNDL_DC_OFF;
		} else {
			goto SKIP_HNDL_DC_OFF;
		}
	}

	switch (charger->dc_state) {
	case DC_STATE_OFF:
		if (trans == DC_TRANS_CHG_ENABLED)
			next_state = DC_STATE_CHECK_VBAT;
		else
			goto ERR;
		break;
	case DC_STATE_CHECK_VBAT:
		if (trans == DC_TRANS_BATTERY_OK)
			next_state = DC_STATE_PRESET;
		else if (trans == DC_TRANS_BATTERY_NG)
			next_state = DC_STATE_CHECK_VBAT;
		else
			goto ERR;
		break;
	case DC_STATE_PRESET:
		if (trans == DC_TRANS_NORMAL_CHG_INT)
			next_state = DC_STATE_START_CC;
		else if (trans == DC_TRANS_BAT_OKB_INT)
			next_state = DC_STATE_CHECK_VBAT;
		else if (trans == DC_TRANS_CHGIN_OKB_INT)
			next_state = DC_STATE_CHECK_VBAT;
		else
			goto ERR;
		break;
	case DC_STATE_START_CC:
		if (trans == DC_TRANS_RAMPUP)
			next_state = DC_STATE_START_CC;
		else if (trans == DC_TRANS_RAMPUP_FINISHED)
#ifdef CONFIG_SEC_FACTORY
			next_state = DC_STATE_SLEEP_CC;
#else
			next_state = DC_STATE_CC;
#endif
		else
			goto ERR;
		break;
	case DC_STATE_CC:
		if (trans == DC_TRANS_CC_STABLED)
			next_state = DC_STATE_SLEEP_CC;
		else if (trans == DC_TRANS_RAMPUP_OVER_VBATT
			|| trans == DC_TRANS_RAMPUP_OVER_CURRENT)
			next_state = DC_STATE_SLEEP_CC;
		else
			goto ERR;
		break;
	case DC_STATE_ADJUSTED_CC:
		if (trans == DC_TRANS_FLOAT_VBATT)
			next_state = DC_STATE_CV;
		else if (trans == DC_TRANS_SET_CURRENT_MAX)
			next_state = DC_STATE_SLEEP_CC;
		else
			goto ERR;
		break;
	case DC_STATE_SLEEP_CC:
		if (trans == DC_TRANS_FLOAT_VBATT)
			next_state = DC_STATE_CV;
		else if (trans == DC_TRANS_SET_CURRENT_MAX)
			next_state = DC_STATE_SLEEP_CC;
		else
			goto ERR;
		break;
	case DC_STATE_CV:
		if (trans == DC_TRANS_SET_CURRENT_MAX)
			next_state = DC_STATE_SLEEP_CC;
		else
			goto ERR;
		break;
	case DC_STATE_DONE:
		break;
	default:
		break;
	}

HNDL_DC_OFF:
	pr_info("%s: %s --> %s --> %s", __func__,
		state_to_str(charger->dc_state), trans_to_str(trans),
		state_to_str(next_state));

	/* Trigger state function */
	charger->dc_state = next_state;

	/* State update */
	schedule_delayed_work(&charger->update_wqueue, 0);

	charger->dc_state_fp[next_state]((void *)charger);
	wake_unlock(&charger->state_manager_wake);
	mutex_unlock(&charger->dc_state_mutex);

	return;	

SKIP_HNDL_DC_OFF:
	pr_info("%s: %s --> %s --> %s", __func__,
		state_to_str(charger->dc_state), trans_to_str(trans),
		state_to_str(next_state));

	/* Trigger state function */
	charger->dc_state = next_state;

	/* State update */
	schedule_delayed_work(&charger->update_wqueue, 0);

	msleep(50);
	wake_unlock(&charger->state_manager_wake);
	mutex_unlock(&charger->dc_state_mutex);

	return;

ERR:
	pr_err("%s err occured, state now : %s\n", __func__,
		state_to_str(charger->dc_state));
	wake_unlock(&charger->state_manager_wake);
	mutex_unlock(&charger->dc_state_mutex);

	return;
}

static void s2mu107_dc_state_trans_enqueue(struct s2mu107_dc_data *charger, s2mu107_dc_trans_t trans)
{
	mutex_lock(&charger->trans_mutex);
	charger->dc_trans = trans;
	schedule_delayed_work(&charger->state_manager_wqueue, msecs_to_jiffies(0));
	msleep(50);
	mutex_unlock(&charger->trans_mutex);
}

static void s2mu107_dc_state_check_vbat(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;
	unsigned int vBatt, soc;

	soc = s2mu107_dc_get_fg_value(charger, POWER_SUPPLY_PROP_CAPACITY);

	if (charger->dc_sc_status == S2MU107_DC_SC_STATUS_CHARGE)
		vBatt = s2mu107_dc_get_fg_value(charger, POWER_SUPPLY_PROP_VOLTAGE_AVG);
	else
 		vBatt = s2mu107_dc_get_pmeter(charger, PMETER_ID_VBATT);

	msleep(100);
	select_pdo(1);
	msleep(100);

	pr_info("%s, soc : %d, vBatt : %d\n", __func__, soc, vBatt);
	s2mu107_dc_set_vsys_dcmode(charger, S2MU107_DC_DISABLE);

	if (vBatt > (DC_MIN_VBAT + charger->chk_vbat_margin) && soc < DC_MAX_SOC) {
		if (charger->dc_sc_status == S2MU107_DC_SC_STATUS_CHARGE) {
			s2mu107_dc_set_sc_prop(charger,
					POWER_SUPPLY_PROP_CHARGING_ENABLED,
					SEC_BAT_CHG_MODE_CHARGING_OFF);
			charger->dc_sc_status = S2MU107_DC_SC_STATUS_OFF;
		}
		s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_BATTERY_OK);
	} else {
		s2mu107_dc_set_sc_prop(charger,
				POWER_SUPPLY_PROP_CHARGING_ENABLED,
				SEC_BAT_CHG_MODE_CHARGING);
		msleep(100);
		s2mu107_dc_set_sc_prop(charger,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DIRECT_BUCK_OFF,
			S2MU107_DC_DISABLE);

		charger->dc_sc_status = S2MU107_DC_SC_STATUS_CHARGE;
		schedule_delayed_work(&charger->check_vbat_wqueue, msecs_to_jiffies(1000));
	}

	return;
}

static void s2mu107_dc_state_preset(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;

	unsigned int pdo_pos = 0;
	unsigned int taMaxVol = 4400 * 2;
	unsigned int taMaxCur;
	unsigned int taMaxPwr;

	union power_supply_propval value;
	unsigned int float_voltage;
	u8 val;

	mutex_lock(&charger->dc_mutex);

	/* EVT1 Reg Control */
	if (charger->rev_id == 1) {
		s2mu107_read_reg(charger->i2c, 0xE3, &val);
		if (val == 0x00)
			s2mu107_write_reg(charger->i2c, 0xE3, 0x20);
		s2mu107_read_reg(charger->i2c, 0xEA, &val);
		if (val == 0x00)
			s2mu107_write_reg(charger->i2c, 0xEA, 0x20);
		s2mu107_read_reg(charger->i2c, 0xEB, &val);
		if (val == 0x00)
			s2mu107_write_reg(charger->i2c, 0xEB, 0x20);
		s2mu107_read_reg(charger->i2c, 0xEC, &val);
		if (val == 0x00)
			s2mu107_write_reg(charger->i2c, 0xEC, 0x40);
		s2mu107_read_reg(charger->i2c, 0xF0, &val);
		if (val == 0x00)
			s2mu107_write_reg(charger->i2c, 0xF0, 0x20);
	}

	charger->rise_speed_dly_ms = 150;
	charger->is_rampup_adjusted = false;

	sec_pd_get_apdo_max_power(&pdo_pos, &taMaxVol, &taMaxCur, &taMaxPwr);
	charger->pd_data->pdo_pos	= pdo_pos;
	charger->pd_data->taMaxVol	= taMaxVol - 700;
	charger->pd_data->taMaxCur	= taMaxCur;
	charger->pd_data->taMaxPwr 	= taMaxPwr;

	if (pdo_pos == 0) {
		pr_info("%s invalid pdo_pos.\n", __func__);
		s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_DETACH);
		mutex_unlock(&charger->dc_mutex);
		return;
	}

	/* get step charging current */
	charger->ta_pwr_type = (taMaxCur >= 4500) ? TA_PWR_TYPE_45W : TA_PWR_TYPE_25W;

	if (charger->step_iin_ma > taMaxCur)
		charger->step_iin_ma = taMaxCur;

	pr_info("%s pdo_pos : %d, taMaxVol : %d, taMaxCur : %d, taMaxPwr : %d\n",
		__func__, pdo_pos, taMaxVol, taMaxCur, taMaxPwr);

	/* Buck Off */
	s2mu107_dc_set_sc_prop(charger,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DIRECT_BUCK_OFF,
		S2MU107_DC_ENABLE);
	msleep(50);

	/* Set Dual Buck */
	s2mu107_dc_set_dual_buck(charger, S2MU107_DC_ENABLE);
	msleep(100);

	/* Determine the Initial Voltage Here */
	s2mu107_dc_cal_target_value(charger);
	charger->minIIN		= DC_TA_MIN_PPS_CURRENT;
	charger->adjustIIN 	= ((charger->step_iin_ma / DC_TA_CURR_STEP_MA) * DC_TA_CURR_STEP_MA) + DC_TA_CURR_STEP_MA;
	sec_pd_select_pps(charger->pd_data->pdo_pos, charger->ppsVol, charger->adjustIIN);

	s2mu107_dc_set_fg_iavg(charger, S2MU107_DC_ENABLE);
	if (s2mu107_dc_delay(charger, 900)) {
		pr_info("%s DC stopped is_charging : %d, VCHGIN : %d\n", __func__,
			(int)charger->is_charging, charger->pmeter[PMETER_ID_VCHGIN]);
		s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_DETACH);
		mutex_unlock(&charger->dc_mutex);
		return;
	}

	/* Determine the Initial Voltage Here */
	s2mu107_dc_cal_target_value(charger);

	s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);
	if (!charger->is_charging || (charger->pmeter[PMETER_ID_VCHGIN] < 4100)) {
		pr_info("%s DC stopped is_charging : %d, VCHGIN : %d\n", __func__,
			(int)charger->is_charging, charger->pmeter[PMETER_ID_VCHGIN]);
		s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_DETACH);
		mutex_unlock(&charger->dc_mutex);
		return;
	}

	pr_info("%s: sec_pd_select_pps, pdo_pos : %d, ppsVol : %d, curr : %d\n",
		__func__, charger->pd_data->pdo_pos, charger->ppsVol, charger->adjustIIN);
	sec_pd_select_pps(charger->pd_data->pdo_pos, charger->ppsVol, charger->adjustIIN);

	charger->chgIIN = charger->step_iin_ma * 2;
	charger->targetIIN = ((charger->step_iin_ma / DC_TA_CURR_STEP_MA) * DC_TA_CURR_STEP_MA) + DC_TA_CURR_STEP_MA;

	s2mu107_dc_set_input_current(charger, DC_MAX_INPUT_CURRENT_MA);
	s2mu107_dc_set_charging_current(charger, charger->chgIIN, S2MU107_DC_MODE_TA_CC);
	s2mu107_dc_set_topoff_current(charger, DC_TOPOFF_CURRENT_MA);

	s2mu107_dc_test_read(charger->i2c);

	/* Set WCIN Pwr TR */
	if (charger->rev_id == 0x00)
		s2mu107_dc_set_wcin_pwrtr(charger);

	/* Set TFB value */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL11, 0x38, 0x38);

	/* Set cc band to minimum */
	s2mu107_dc_set_cc_band_width(charger, DC_CC_BAND_WIDTH_MIN_MA);

	/* VBAT_OK_LEVEL 3.4V */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL23, 0x00, DC_SEL_BAT_OK_MASK);

	/* Long CC Step 100mA */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL16, 0x00, DC_LONG_CC_STEP_MASK);

	/* OCP Off */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_TEST2,
				0x0 << DC_EN_OCP_FLAG_SHIFT,
				DC_EN_OCP_FLAG_MASK);

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CD_OTP_02,
				0x1 << CD_OCP_ACTION_SHIFT,
				CD_OCP_ACTION_MASK);

	/* step CC difference setting */
	s2mu107_write_reg(charger->i2c, S2MU107_DC_CD_OTP_01, 0x02);

	/* DC_EN_CV off */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_TEST6, 0x00, T_DC_EN_DC_CV_MASK);

	/* Expand VBUS Target range */
	s2mu107_write_reg(charger->i2c, S2MU107_DC_CTRL19, 0x1F);

	/* tSET_VF_VBAT */
	s2mu107_update_reg(charger->i2c, S2MU107_SC_OTP103, 0x04, T_SET_VF_VBAT_MASK);

	/* STEP CC disable */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL16,
				0x0, DC_EN_LONG_CC_MASK);

	/* LONG_CC_SEL_CELL_PACKB */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL16,
				LONGCC_SEL_CELL_PACKB_MASK, LONGCC_SEL_CELL_PACKB_MASK);

	/* Disable Thermal Control */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL20,
				0, DC_EN_THERMAL_LOOP_MASK);

	/* 50 degree */
	//s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL20, 0x0F, THERMAL_RISING_MASK);

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL23,
				0x04 << THERMAL_WAIT_SHIFT, THERMAL_WAIT_MASK);

	/* BYP OVP Disable */
	s2mu107_write_reg(charger->i2c, S2MU107_DC_INPUT_OTP_04, 0xCA);

	/* RAMPUP FAIL OFF */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_INPUT_OTP_04,
				RAMP_OFF_ACTION_MASK, RAMP_OFF_ACTION_MASK);

	charger->is_plugout_mask = true;
	/* PLUG OUT OFF */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_INPUT_OTP_04,
				PLUG_OUT_ACTION_MASK, PLUG_OUT_ACTION_MASK);

	/* DC_WCIN_RCP_INT_MASK */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_INT1_MASK,
				DC_WCIN_RCP_INT_MASK_MASK, DC_WCIN_RCP_INT_MASK_MASK);

	/* RCP_ACTION */
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CD_OTP_02,
				RCP_ACTION_MASK, RCP_ACTION_MASK);

	if (charger->rev_id == 1) {
		/* enable EN_SCP_CHECK */
		s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL17,
				EN_SCP_CHECK_MASK, EN_SCP_CHECK_MASK);
		/* DC Frequency SET */
		s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL0,
				4 << FREQ_SEL_SHIFT,
				FREQ_SEL_MASK);
	}

	/* RCP offset trim : Stabling DC off sequence */
	s2mu107_write_reg(charger->i2c_common, 0x62, 0x90);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);


	s2mu107_dc_test_read(charger->i2c);

	/* VSYS UP */
	s2mu107_dc_set_vsys_dcmode(charger, S2MU107_DC_ENABLE);

	charger->alarm_interval = DEFAULT_ALARM_INTERVAL;

	/* Set Float Voltage */
	if (!charger->psy_sc)
		s2mu107_dc_set_float_voltage(charger, 4380);
	else {
		power_supply_get_property(charger->psy_sc,
				POWER_SUPPLY_PROP_VOLTAGE_MAX, &value);
		if (value.intval > 0) {
			float_voltage = value.intval + 30;
			s2mu107_dc_set_float_voltage(charger, float_voltage);
		} else
			s2mu107_dc_set_float_voltage(charger, 4380);
	}

	/* Diff to 0 at 4.38V FV */
	s2mu107_update_reg(charger->i2c,
		S2MU107_DC_CD_OTP_01, 0, DIG_CV_MASK);

	s2mu107_dc_set_iin_off(charger, SET_IIN_OFF_TARGET_MA);

	msleep(100);

	/* Set Dual Buck */
	s2mu107_dc_set_dual_buck(charger, S2MU107_DC_ENABLE);

	s2mu107_dc_test_read(charger->i2c);

	//s2mu107_dc_ir_drop_compensation(charger);
	if (!charger->is_charging || (charger->pmeter[PMETER_ID_VCHGIN] < 4100)) {
		pr_info("%s DC stopped is_charging : %d, VCHGIN : %d\n", __func__,
			(int)charger->is_charging, charger->pmeter[PMETER_ID_VCHGIN]);
		s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_DETACH);
		mutex_unlock(&charger->dc_mutex);
		return;
	}

	s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);
	s2mu107_dc_forced_enable(charger, S2MU107_DC_DISABLE);
	s2mu107_dc_check_vbat_validity(charger);

	/* set pm enable */
	s2mu107_dc_pm_enable(charger);
	msleep(20);

	/* Determine the Initial Voltage Here */
	s2mu107_dc_cal_target_value(charger);

	/* Request and Verify */
	s2mu107_dc_send_verify(charger);

	/* Check if the VCHGIN is ok */
	s2mu107_dc_vchgin_compensation(charger);

	/* dc en */
	s2mu107_dc_enable(charger, S2MU107_DC_ENABLE);

	//schedule_delayed_work(&charger->chk_valid_wqueue,  msecs_to_jiffies(10000));
	mutex_unlock(&charger->dc_mutex);

	return;
}

static void s2mu107_dc_state_start_cc(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;
#ifndef CONFIG_SEC_FACTORY
	unsigned int iChgin;

	charger->dc_chg_status = POWER_SUPPLY_STATUS_CHARGING;

	charger->vchgin_okb_retry = 0;
	iChgin = s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN);

	if (charger->adjustIIN >= charger->targetIIN) {
		pr_info("%s Rampup Finished, iChgin : %d\n", __func__, iChgin);
		cancel_delayed_work(&charger->start_cc_wqueue);
		s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_RAMPUP_FINISHED);
	} else {
		schedule_delayed_work(&charger->start_cc_wqueue, msecs_to_jiffies(100));
	}
#else
	int vbatt, wait_ret = 0, iin;

	usleep_range(20000, 21000);
	s2mu107_dc_set_target_curr(charger);
	s2mu107_dc_send_fenced_pps(charger);
	usleep_range(20000, 21000);
	if (!charger->is_charging)
		return;

	while (charger->ppsVol < charger->pd_data->taMaxVol) {
		vbatt = s2mu107_dc_get_fg_value(charger, POWER_SUPPLY_PROP_VOLTAGE_NOW);
		if (vbatt >= 4380)
			break;

		charger->ppsVol += DC_TA_VOL_STEP_MV * 4;
		wait_ret = s2mu107_dc_send_verify(charger);
		usleep_range(10000, 11000);

		if (!charger->is_charging)
			return;

		iin = s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN) +
			s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);

		pr_info("%s iin : %d\n", __func__, iin);
		if ((charger->ta_pwr_type == TA_PWR_TYPE_25W)
			&& (charger->step_iin_ma == charger->pd_data->taMaxCur)) {
			if (iin > charger->step_iin_ma) {
				msleep(50);
				iin = s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN) +
					s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);
				pr_info("%s chk point iin : %d\n", __func__, iin);
			}

			if (iin > charger->step_iin_ma + 50) {
				break;
			}
		}

		if (charger->is_plugout_mask) {
			if (iin > 300) {
				charger->is_plugout_mask = false;
				/* PLUG OUT OFF */
				s2mu107_update_reg(charger->i2c, S2MU107_DC_INPUT_OTP_04,
							0, PLUG_OUT_ACTION_MASK);
			}
		}
	}

	s2mu107_dc_approach_target_curr_manual(charger);
	if (s2mu107_dc_delay(charger, 5000))
		return;
	s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_RAMPUP_FINISHED);
#endif
	return;
}

static void s2mu107_dc_state_cc(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;

	cancel_delayed_work(&charger->timer_wqueue);
	schedule_delayed_work(&charger->timer_wqueue, msecs_to_jiffies(2000));
	pr_info("%s Enter", __func__);

	s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);

	s2mu107_dc_set_irq_unmask(charger);

	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL22,
 		TA_TRANSIENT_DONE_MASK, TA_TRANSIENT_DONE_MASK);

	return;
}

static void s2mu107_dc_state_sleep_cc(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;

	pr_info("%s Enter", __func__);

	cancel_delayed_work(&charger->timer_wqueue);
	cancel_delayed_work(&charger->check_vbat_wqueue);
	cancel_delayed_work(&charger->start_cc_wqueue);
	alarm_cancel(&charger->dc_monitor_alarm);

	s2mu107_dc_set_topoff_current(charger, 500);

	s2mu107_dc_forced_enable(charger, S2MU107_DC_ENABLE);

	if (charger->is_rampup_adjusted) {
		charger->is_rampup_adjusted = false;
		s2mu107_dc_set_charging_current(charger, (charger->targetIIN * 2), S2MU107_DC_MODE_TA_CC);
	} else {
		s2mu107_dc_set_target_curr(charger);
		s2mu107_dc_approach_target_curr_manual(charger);
		charger->is_rampup_adjusted = false;
	}
	s2mu107_dc_refresh_auto_pps(charger, charger->ppsVol, charger->targetIIN);

	/* Set cc band to 400mA */
	s2mu107_dc_set_cc_band_width(charger, DC_CC_BAND_WIDTH_MIN_MA);

	if (s2mu107_dc_delay(charger, 3000))
		return;

	charger->alarm_interval = HV_ALARM_INTERVAL;
	alarm_start_relative(&charger->dc_monitor_alarm, ktime_set(charger->alarm_interval, 0));
	wake_unlock(&charger->wake_lock);
}

static void s2mu107_dc_state_adjusted_cc(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;
	int iin, iin_cal, iin_set, vol_set;

	s2mu107_dc_forced_enable(charger, S2MU107_DC_ENABLE);
	pr_info("%s Enter", __func__);

	cancel_delayed_work(&charger->timer_wqueue);
	cancel_delayed_work(&charger->check_vbat_wqueue);
	cancel_delayed_work(&charger->start_cc_wqueue);
	alarm_cancel(&charger->dc_monitor_alarm);

	charger->is_rampup_adjusted = true;
	iin = s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN) +
		s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);
	iin_cal = (iin / DC_TA_CURR_STEP_MA) * DC_TA_CURR_STEP_MA;

	iin_set = (iin_cal < charger->targetIIN) ? iin_cal + DC_TA_CURR_STEP_MA : charger->targetIIN;
	vol_set = (charger->ta_pwr_type == TA_PWR_TYPE_45W) ? charger->ppsVol + 500 : charger->ppsVol;
	
	s2mu107_dc_set_topoff_current(charger, 500);
	s2mu107_dc_refresh_auto_pps(charger, vol_set, iin_set);

	if (s2mu107_dc_delay(charger, 3000))
		return;

	alarm_start_relative(&charger->dc_monitor_alarm, ktime_set(charger->alarm_interval, 0));
	wake_unlock(&charger->wake_lock);
}

static void s2mu107_dc_state_cv(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;

	wake_lock(&charger->wake_lock);
	pr_info("%s Enter", __func__);

	mutex_lock(&charger->auto_pps_mutex);
	if (charger->pd_data->pdo_pos) {
		charger->targetIIN -= DC_TA_CURR_STEP_MA;
		sec_pps_enable(charger->pd_data->pdo_pos,
			charger->ppsVol, charger->targetIIN, S2MU107_DC_DISABLE);
	}
	mutex_unlock(&charger->auto_pps_mutex);
	wake_unlock(&charger->wake_lock);
}

static void s2mu107_dc_state_wakeup_cc(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;

	pr_info("%s Enter", __func__);

	wake_lock(&charger->wake_lock);

	charger->rise_speed_dly_ms = 10;
	charger->ppsVol = (unsigned int)sec_get_pps_voltage();

	mutex_lock(&charger->auto_pps_mutex);
	if (charger->pd_data->pdo_pos) {
		sec_pps_enable(charger->pd_data->pdo_pos,
			charger->ppsVol, charger->targetIIN, S2MU107_DC_DISABLE);
	}
	mutex_unlock(&charger->auto_pps_mutex);

	s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_WAKEUP_DONE);
}

static void s2mu107_dc_state_done(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;

	cancel_delayed_work(&charger->timer_wqueue);
	cancel_delayed_work(&charger->chk_valid_wqueue);

	charger->is_charging = false;
	charger->ps_health = POWER_SUPPLY_HEALTH_GOOD;
	charger->dc_chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
	charger->dc_sc_status = S2MU107_DC_SC_STATUS_OFF;

	alarm_cancel(&charger->dc_monitor_alarm);
	cancel_delayed_work_sync(&charger->dc_monitor_work);

	mutex_lock(&charger->auto_pps_mutex);
	if (charger->pd_data->pdo_pos) {
		sec_pps_enable(charger->pd_data->pdo_pos,
			DC_TA_VOL_END_MV, charger->targetIIN, S2MU107_DC_DISABLE);
			charger->pd_data->pdo_pos = 0;

		msleep(100);
	}
	mutex_unlock(&charger->auto_pps_mutex);

	s2mu107_dc_enable(charger, S2MU107_DC_DISABLE);
	usleep_range(10000, 11000);

	s2mu107_dc_set_vsys_dcmode(charger, S2MU107_DC_DISABLE);

	/* Buck Off mode disable */
	s2mu107_dc_set_sc_prop(charger,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DIRECT_BUCK_OFF,
		S2MU107_DC_DISABLE);

	s2mu107_dc_set_fg_iavg(charger, S2MU107_DC_DISABLE);

	/*
	 * Ctrl SC Charger Here.
	 */
	charger->prop_data.value.intval = S2MU107_DC_ENABLE;
	charger->prop_data.prop = POWER_SUPPLY_EXT_PROP_DIRECT_DONE;
	schedule_delayed_work(&charger->set_prop_wqueue, 0);
	return;
}

static void s2mu107_dc_state_off(void *data)
{
	struct s2mu107_dc_data *charger = (struct s2mu107_dc_data *)data;
	pr_info("%s\n", __func__);
	wake_unlock(&charger->wake_lock);

	charger->is_charging = false;
	charger->dc_chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
	charger->is_rampup_adjusted = false;
	charger->dc_sc_status = S2MU107_DC_SC_STATUS_OFF;

	alarm_cancel(&charger->dc_monitor_alarm);
	cancel_delayed_work_sync(&charger->dc_monitor_work);

	mutex_lock(&charger->auto_pps_mutex);
	if (charger->pd_data->pdo_pos) {
		sec_pps_enable(charger->pd_data->pdo_pos,
			DC_TA_VOL_END_MV, charger->targetIIN, S2MU107_DC_DISABLE);
			charger->pd_data->pdo_pos = 0;
	}
	mutex_unlock(&charger->auto_pps_mutex);

	s2mu107_dc_enable(charger, S2MU107_DC_DISABLE);
	usleep_range(10000, 11000);

	/* Set VSYS recover */
	s2mu107_dc_set_vsys_dcmode(charger, S2MU107_DC_DISABLE);

	/* Buck Off mode disable */
	s2mu107_dc_set_sc_prop(charger,
		(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DIRECT_BUCK_OFF,
		S2MU107_DC_DISABLE);

	s2mu107_dc_set_fg_iavg(charger, S2MU107_DC_DISABLE);

	cancel_delayed_work(&charger->timer_wqueue);
	cancel_delayed_work(&charger->check_vbat_wqueue);
	cancel_delayed_work(&charger->start_cc_wqueue);
	cancel_delayed_work(&charger->chk_valid_wqueue);
	return;
}

static int s2mu107_dc_send_fenced_pps(struct s2mu107_dc_data *charger)
{
	if (charger->ppsVol > charger->pd_data->taMaxVol)
		charger->ppsVol = charger->pd_data->taMaxVol;

	if (charger->targetIIN > charger->pd_data->taMaxCur)
		charger->targetIIN = charger->pd_data->taMaxCur;

	if (charger->targetIIN < charger->minIIN)
		charger->targetIIN = charger->minIIN;

	pr_info("%s vol : %d, curr : %d, duration : %ld\n",
		__func__, charger->ppsVol, charger->targetIIN,
		s2mu107_dc_update_time_chk(charger));
	if (charger->is_charging)
		sec_pd_select_pps(charger->pd_data->pdo_pos,
			charger->ppsVol, charger->targetIIN);
	else
		return 0;

	s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);

	return 1;
}

static int s2mu107_dc_send_verify(struct s2mu107_dc_data *charger)
{
	int wait_ret = 0;
	s2mu107_dc_send_fenced_pps(charger);

	charger->is_pps_ready = false;
	wait_ret = wait_event_interruptible_timeout(charger->wait,
			charger->is_pps_ready,
			msecs_to_jiffies(500));
	pr_info("%s, wait %d ms.\n", __func__, wait_ret);
	return wait_ret;
}

static void s2mu107_dc_timer_work(struct work_struct *work)
{
	struct s2mu107_dc_data *charger = container_of(work, struct s2mu107_dc_data,
						 timer_wqueue.work);
	int ret;
	u8 val;

	if ((!charger->is_charging) || (charger->dc_state != DC_STATE_CC)) {
		pr_info("%s, skip work\n", __func__);
		return;
	}

	mutex_lock(&charger->timer_mutex);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_VBATT);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_TDIE);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL14, &val);
	pr_info("%s READ Charging current : 0x%X: %d mA\n", __func__, val & 0x7F, ((val & 0x7F) * 100));
	s2mu107_read_reg(charger->i2c, S2MU107_DC_CD_OTP_02, &val);
	pr_info("%s D9. CC_IINB_CTRL : 0x%X\n", __func__, val & 0x8);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_TEST6, &val);
	pr_info("%s CV_EN(0x61) : 0x%X\n", __func__, val & 0x3);
	//s2mu107_dc_cal_input_current(charger);
	/* Set Dual Buck */
	s2mu107_dc_set_dual_buck(charger, S2MU107_DC_ENABLE);

	ret = s2mu107_dc_send_fenced_pps(charger);
	pr_info("%s send pps : %d\n", __func__, ret);

	if (charger->ppsVol == charger->pd_data->taMaxVol) {
		s2mu107_dc_forced_enable(charger, S2MU107_DC_ENABLE);
		s2mu107_dc_set_input_current(charger, DC_MAX_INPUT_CURRENT_MA);
		msleep(50);
		s2mu107_dc_approach_target_curr_manual(charger);
		s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_CC_STABLED);
		goto SKIP_ENQUEUE;
	}

	schedule_delayed_work(&charger->timer_wqueue, msecs_to_jiffies(5000));

SKIP_ENQUEUE
:
	mutex_unlock(&charger->timer_mutex);
}

static void s2mu107_dc_start_cc_work(struct work_struct *work)
{
	struct s2mu107_dc_data *charger = container_of(work, struct s2mu107_dc_data,
						 start_cc_wqueue.work);
	unsigned int ppsVol;
	mutex_lock(&charger->dc_mutex);

	ppsVol = charger->ppsVol;
	charger->adjustIIN += DC_TA_CURR_STEP_MA;
	pr_info("%s, charger->adjustIIN : %d\n", __func__, charger->adjustIIN);
	sec_pd_select_pps(charger->pd_data->pdo_pos, charger->ppsVol, charger->adjustIIN);
	s2mu107_dc_state_manager(charger, DC_TRANS_RAMPUP);

	mutex_unlock(&charger->dc_mutex);
}

static void s2mu107_dc_check_vbat_work(struct work_struct *work)
{
	struct s2mu107_dc_data *charger = container_of(work, struct s2mu107_dc_data,
						 check_vbat_wqueue.work);

	mutex_lock(&charger->dc_mutex);
	pr_info("%s\n", __func__);
	s2mu107_dc_state_manager(charger, DC_TRANS_BATTERY_NG);
	mutex_unlock(&charger->dc_mutex);
}

static void s2mu107_dc_state_manager_work(struct work_struct *work)
{
	struct s2mu107_dc_data *charger = container_of(work, struct s2mu107_dc_data,
						 state_manager_wqueue.work);

	pr_info("%s\n", __func__);
	s2mu107_dc_state_manager(charger, charger->dc_trans);
}

static void s2mu107_dc_set_prop_work(struct work_struct *work)
{
	struct s2mu107_dc_data *charger = container_of(work, struct s2mu107_dc_data,
						 set_prop_wqueue.work);
	struct power_supply *psy;
	int ret;

	pr_info("%s\n", __func__);
	psy = power_supply_get_by_name(charger->pdata->sec_dc_name);
	if (!psy) {
		pr_err("%s, can't get power supply", __func__);
		return;
	}
	ret = power_supply_set_property(psy, charger->prop_data.prop,
		&(charger->prop_data.value));
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);

	return;
}

static int s2mu107_dc_pps_isr(struct s2mu107_dc_data *charger, s2mu107_dc_pps_signal_t sig)
{
	int vbatt, vsys, inow, target_cur, iin;
	int wait_ret = 0;
	u8 val;

	mutex_lock(&charger->pps_isr_mutex);

	pr_info("%s sig : %d, ppsVol : %d\n",
	__func__, sig, charger->ppsVol);

	if (charger->dc_state == DC_STATE_CC) {
		if (sig == S2MU107_DC_PPS_SIGNAL_DEC) {
			charger->ppsVol -= DC_TA_VOL_STEP_MV;
		} else if (sig == S2MU107_DC_PPS_SIGNAL_INC) {
			vbatt = s2mu107_dc_get_fg_value(charger, POWER_SUPPLY_PROP_VOLTAGE_NOW);
			if (vbatt >= charger->step_vbatt_mv) {
				s2mu107_dc_approach_target_curr_manual(charger);
				s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_RAMPUP_OVER_VBATT);
				goto SKIP_PPS_ISR_SEND_MSG;
			}

			vsys = s2mu107_dc_get_pmeter(charger, PMETER_ID_VSYS);
			inow = s2mu107_dc_get_fg_value(charger, POWER_SUPPLY_PROP_CURRENT_NOW);
			s2mu107_read_reg(charger->i2c, S2MU107_DC_CTRL14, &val);
			target_cur = ((val & 0x7F) * 100);
			pr_info("%s READ Chging current : %d mA\n", __func__, target_cur);

			if (inow > target_cur) {
				pr_info("%s skipped increasing, inow : %d\n", __func__, inow);
				goto SKIP_PPS_ISR_SEND_MSG;
			}

			iin = s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN) +
				s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);
			if (iin > charger->step_iin_ma + 50) {
				s2mu107_dc_set_input_current(charger, charger->step_iin_ma + 100);
				charger->ppsVol = charger->pd_data->taMaxVol;
			} else {
				charger->ppsVol += DC_TA_VOL_STEP_MV * 2;
			}

			pr_info("%s UP!, vsys : %d, vbatt : %d\n", __func__, vsys, vbatt);
		} else {
			pr_err("%s err\n", __func__);
			goto SKIP_PPS_ISR_DELAY;
		}

		charger->is_pps_ready = false;
		cancel_delayed_work(&charger->timer_wqueue);
		schedule_delayed_work(&charger->timer_wqueue, msecs_to_jiffies(0));
		wait_ret = wait_event_interruptible_timeout(charger->wait,
				charger->is_pps_ready,
				msecs_to_jiffies(2000));
		if (charger->is_plugout_mask) {
			iin = s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN) +
				s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);
			if (iin > 300) {
				charger->is_plugout_mask = false;
				/* PLUG OUT OFF */
				s2mu107_update_reg(charger->i2c, S2MU107_DC_INPUT_OTP_04,
							0, PLUG_OUT_ACTION_MASK);
			}
		}
	} else {
		pr_info("%s skipped by state check, curr state : %s", __func__,
			state_to_str(charger->dc_state));
		goto SKIP_PPS_ISR_TR_DONE;
	}

SKIP_PPS_ISR_SEND_MSG:
	if (charger->rise_speed_dly_ms <= 10)
		usleep_range(10000, 11000);
	else
		msleep(charger->rise_speed_dly_ms);
SKIP_PPS_ISR_DELAY:
	pr_info("%s TA_TRANSIENT_DONE", __func__);
	s2mu107_update_reg(charger->i2c, S2MU107_DC_CTRL22,
		TA_TRANSIENT_DONE_MASK, TA_TRANSIENT_DONE_MASK);
SKIP_PPS_ISR_TR_DONE:
	mutex_unlock(&charger->pps_isr_mutex);
	return 1;
}

static irqreturn_t s2mu107_dc_mode_isr(int irq, void *data)
{
	struct s2mu107_dc_data *charger = data;

	wake_lock(&charger->mode_irq);
	pr_info("%s", __func__);

	if (irq == charger->irq_normal_charging) {
		/* status DC_START_CC */
		pr_info("%s Normal Charger detected\n", __func__);
		usleep_range(10000,11000);
		s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_NORMAL_CHG_INT);
	} else if (irq == charger->irq_done){
		pr_info("%s Finish DC detected\n", __func__);
		/* Done detect alarm */
		/* status DC_DONE */
		s2mu107_dc_state_manager(charger, DC_TRANS_DC_DONE_INT);
	} else if (irq == charger->irq_long_cc) {
		pr_info("%s lcc is activated\n", __func__);
	} else if (irq == charger->irq_off) {
		s2mu107_dc_state_manager(charger, DC_TRANS_DC_OFF_INT);
	} else if (irq == charger->irq_plug_out) {
		if (!charger->is_plugout_mask)
			s2mu107_dc_state_manager(charger, DC_TRANS_DC_OFF_INT);
	}
	wake_unlock(&charger->mode_irq);

	return IRQ_HANDLED;
}

static irqreturn_t s2mu107_dc_fail_isr(int irq, void *data)
{
	struct s2mu107_dc_data *charger = data;

	if (!charger->is_charging) {
		pr_info("%s, DC is not charging. skip isr\n", __func__);
		return IRQ_HANDLED;
	}

	wake_lock(&charger->fail_irq);
	pr_info("%s, duration : %ld",
			__func__, s2mu107_dc_update_time_chk(charger));

	s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_ICHGIN);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_IWCIN);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_VBATT);
	s2mu107_dc_get_pmeter(charger, PMETER_ID_TDIE);

	if (charger->rev_id == 0x00)
		s2mu107_dc_set_wcin_pwrtr(charger);

	if (irq == charger->irq_ramp_fail
		|| irq == charger->irq_byp2out_ovp
		|| irq == charger->irq_byp_ovp
		|| irq == charger->irq_out_ovp
		|| irq == charger->irq_wcin_diod_prot
		|| irq == charger->irq_chgin_diod_prot
		|| irq == charger->irq_pps_fail
		|| irq == charger->irq_scp
		|| irq == charger->irq_sc_off
		|| irq == charger->irq_pm_off
		|| irq == charger->irq_tsd) {
		pr_info("%s : dc was stopped\n", __func__);
		charger->ps_health = POWER_SUPPLY_HEALTH_DC_ERR;
		charger->is_charging = false;
		s2mu107_dc_state_manager(charger, DC_TRANS_FAIL_INT);
		charger->vchgin_okb_retry = 0;
	} else if (irq == charger->irq_vchgin_okb) {
		pr_info("%s : VBUS is not ready\n", __func__);
		s2mu107_dc_get_pmeter(charger, PMETER_ID_VCHGIN);
		charger->vchgin_okb_retry++;
		msleep(100);
		if (charger->dc_state != DC_STATE_PRESET) {
			charger->ps_health = POWER_SUPPLY_HEALTH_DC_ERR;
			s2mu107_dc_state_manager(charger, DC_TRANS_FAIL_INT);
		} else {
			s2mu107_dc_state_manager(charger, DC_TRANS_CHGIN_OKB_INT);
		}
	} else if (irq == charger->irq_vbat_okb) {
		pr_info("%s : VBAT is not ready\n", __func__);
		if (charger->dc_state != DC_STATE_PRESET) {
			charger->ps_health = POWER_SUPPLY_HEALTH_DC_ERR;
			pr_info("%s : vbat to fail.\n", __func__);
			s2mu107_dc_state_manager(charger, DC_TRANS_FAIL_INT);
		} else {
			pr_info("%s : vbat to okb.\n", __func__);
			s2mu107_dc_state_manager(charger, DC_TRANS_BAT_OKB_INT);
		}
	} else if (irq == charger->irq_chgin_rcp) {
		if (charger->pmeter[PMETER_ID_ICHGIN] <= 125
			|| charger->pmeter[PMETER_ID_IWCIN] <= 125) {
			pr_info("%s : RCP occurs\n", __func__);
			charger->ps_health = POWER_SUPPLY_HEALTH_DC_ERR;
			charger->is_charging = false;
			s2mu107_dc_state_manager(charger, DC_TRANS_FAIL_INT);
			charger->vchgin_okb_retry = 0;
		} else {
			pr_info("%s : seems to be a false rcp alarm.\n", __func__);
		}
	} else {
		pr_info("%s :Not handled IRQ\n", __func__);
	}

	wake_unlock(&charger->fail_irq);
	return IRQ_HANDLED;
}

#ifdef CONFIG_S2MU107_DC_SUPPORT_THERMAL_CTRL
static irqreturn_t s2mu107_dc_thermal_isr(int irq, void *data)
{
	struct s2mu107_dc_data *charger = data;
	u8 int0, int1, int2, int3;

	wake_lock(&charger->thermal_irq);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT0, &int0);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT1, &int1);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT2, &int2);
	s2mu107_read_reg(charger->i2c, S2MU107_DC_INT3, &int3);

	pr_info("%s Thermal Status! (0x%2x, 0x%2x, 0x%2x, 0x%2x) :",
			__func__, int0, int1, int2, int3);

	wake_unlock(&charger->thermal_irq);

	return IRQ_HANDLED;
}
#endif

static int s2mu107_dc_irq_init(struct s2mu107_dc_data *charger)
{
	int ret = 0;
	int irq_base;

	if (charger->irq_base == 0)
		return -1;

	irq_base = charger->irq_base;

	/* DC_INT_0 */
	charger->irq_ramp_fail = irq_base + S2MU107_DC_IRQ0_DC_RAMP_FAIL;
	ret = request_threaded_irq(charger->irq_ramp_fail, NULL,
			s2mu107_dc_fail_isr, 0, "dc-ramp-fail", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_normal_charging = irq_base + S2MU107_DC_IRQ0_DC_NORMAL_CHARGING;
	ret = request_threaded_irq(charger->irq_normal_charging, NULL,
			s2mu107_dc_mode_isr, 0, "dc-normal-charging", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_vchgin_okb = irq_base + S2MU107_DC_IRQ0_DC_CHGIN_OKB;
	ret = request_threaded_irq(charger->irq_vchgin_okb, NULL,
			s2mu107_dc_fail_isr, 0, "dc-chgin-okb", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_byp2out_ovp = irq_base + S2MU107_DC_IRQ0_DC_BYP2OUT_OVP;
	ret = request_threaded_irq(charger->irq_byp2out_ovp, NULL,
			s2mu107_dc_fail_isr, 0, "dc-byp2out-ovp", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_byp_ovp = irq_base + S2MU107_DC_IRQ0_DC_BYP_OVP;
	ret = request_threaded_irq(charger->irq_byp_ovp, NULL,
			s2mu107_dc_fail_isr, 0, "dc-byp-ovp", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_vbat_okb = irq_base + S2MU107_DC_IRQ0_DC_BAT_OKB;
	ret = request_threaded_irq(charger->irq_vbat_okb, NULL,
			s2mu107_dc_fail_isr, 0, "dc-bat-fail", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_out_ovp = irq_base + S2MU107_DC_IRQ0_DC_OUT_OVP;
	ret = request_threaded_irq(charger->irq_out_ovp, NULL,
			s2mu107_dc_fail_isr, 0, "dc-outovp", charger);
	if (ret < 0)
		goto err_irq;

	/* DC_INT_1 */
	charger->irq_chgin_rcp = irq_base + S2MU107_DC_IRQ1_DC_CHGIN_RCP;
	ret = request_threaded_irq(charger->irq_chgin_rcp, NULL,
			s2mu107_dc_fail_isr, 0, "dc-chgin-rcp", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_off = irq_base + S2MU107_DC_IRQ1_DC_OFF;
	ret = request_threaded_irq(charger->irq_off, NULL,
			s2mu107_dc_mode_isr, 0, "dc-off", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_plug_out = irq_base + S2MU107_DC_IRQ1_DC_CHGIN_PLUG_OUT;
	ret = request_threaded_irq(charger->irq_plug_out, NULL,
			s2mu107_dc_mode_isr, 0, "dc-plugout", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_wcin_diod_prot = irq_base + S2MU107_DC_IRQ1_DC_WCIN_DIODE_PROT;
	ret = request_threaded_irq(charger->irq_wcin_diod_prot, NULL,
			s2mu107_dc_fail_isr, 0, "dc-wcin-diod-prot", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_chgin_diod_prot = irq_base + S2MU107_DC_IRQ1_DC_CHGIN_DIODE_PROT;
	ret = request_threaded_irq(charger->irq_chgin_diod_prot, NULL,
			s2mu107_dc_fail_isr, 0, "dc-chgin-diod-prot", charger);
	if (ret < 0)
		goto err_irq;

	/* DC_INT_2 */
	charger->irq_done = irq_base + S2MU107_DC_IRQ2_DC_DONE;
	ret = request_threaded_irq(charger->irq_done, NULL,
			s2mu107_dc_mode_isr, 0, "dc-done", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_pps_fail = irq_base + S2MU107_DC_IRQ2_DC_PPS_FAIL;
	ret = request_threaded_irq(charger->irq_pps_fail, NULL,
			s2mu107_dc_fail_isr, 0, "dc-pps-fail", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_long_cc = irq_base + S2MU107_DC_IRQ2_DC_LONG_CC;
	ret = request_threaded_irq(charger->irq_long_cc, NULL,
			s2mu107_dc_mode_isr, 0, "dc-long-cc", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_scp = irq_base + S2MU107_DC_IRQ2_DC_SCP;
	ret = request_threaded_irq(charger->irq_scp, NULL,
			s2mu107_dc_fail_isr, 0, "dc-scp", charger);
	if (ret < 0)
		goto err_irq;

	/* DC_INT_3 */
	charger->irq_sc_off = irq_base + S2MU107_DC_IRQ3_DC_SC_OFF;
	ret = request_threaded_irq(charger->irq_sc_off, NULL,
			s2mu107_dc_fail_isr, 0, "dc-sc-off", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_pm_off = irq_base + S2MU107_DC_IRQ3_DC_PM_OFF;
	ret = request_threaded_irq(charger->irq_pm_off, NULL,
			s2mu107_dc_fail_isr, 0, "dc-pm-off", charger);
	if (ret < 0)
		goto err_irq;

	charger->irq_tsd = irq_base + S2MU107_DC_IRQ3_DC_TSD;
	ret = request_threaded_irq(charger->irq_tsd, NULL,
			s2mu107_dc_fail_isr, 0, "dc-tsd", charger);
	if (ret < 0)
		goto err_irq;

	s2mu107_dc_set_irq_unmask(charger);

err_irq:
	return ret;
}

static int s2mu107_dc_set_wcin_pwrtr(struct s2mu107_dc_data *charger)
{
	u8 data;
	int ret;

	ret = s2mu107_read_reg(charger->i2c, S2MU107_DC_TEST4, &data);

	if (ret) {
		pr_err("%s i2c read failed.\n", __func__);
		return ret;
	}

	data |= T_DC_EN_WCIN_PWRTR_MODE_MASK;
	s2mu107_write_reg(charger->i2c, S2MU107_DC_TEST4, data);

	return 0;
}

static bool s2mu107_dc_init(struct s2mu107_dc_data *charger)
{
	int topoff_current = s2mu107_dc_get_topoff_current(charger);
	int float_voltage = s2mu107_dc_get_float_voltage(charger);
	int input_current = s2mu107_dc_get_input_current(charger);
	int charging_current = s2mu107_dc_get_charging_current(charger);

	s2mu107_dc_set_topoff_current(charger, topoff_current);
	s2mu107_dc_set_float_voltage(charger, float_voltage);
	s2mu107_dc_set_input_current(charger, input_current);
	s2mu107_dc_set_charging_current(charger, charging_current, S2MU107_DC_MODE_DC_CC);
	s2mu107_dc_is_enable(charger);
	s2mu107_dc_pm_enable(charger);
	s2mu107_dc_enable(charger, 0);
	s2mu107_dc_set_fg_iavg(charger, S2MU107_DC_DISABLE);

	if (charger->rev_id == 0x00)
		s2mu107_dc_set_wcin_pwrtr(charger);

	charger->dc_state = DC_STATE_OFF;
	charger->dc_state_fp[DC_STATE_OFF]	 	= s2mu107_dc_state_off;
	charger->dc_state_fp[DC_STATE_CHECK_VBAT] 	= s2mu107_dc_state_check_vbat;
	charger->dc_state_fp[DC_STATE_PRESET] 		= s2mu107_dc_state_preset;
	charger->dc_state_fp[DC_STATE_START_CC] 	= s2mu107_dc_state_start_cc;
	charger->dc_state_fp[DC_STATE_CC] 		= s2mu107_dc_state_cc;
	charger->dc_state_fp[DC_STATE_SLEEP_CC]		= s2mu107_dc_state_sleep_cc;
	charger->dc_state_fp[DC_STATE_ADJUSTED_CC]	= s2mu107_dc_state_adjusted_cc;
	charger->dc_state_fp[DC_STATE_CV]		= s2mu107_dc_state_cv;
	charger->dc_state_fp[DC_STATE_WAKEUP_CC]	= s2mu107_dc_state_wakeup_cc;
	charger->dc_state_fp[DC_STATE_DONE] 		= s2mu107_dc_state_done;
	charger->vchgin_okb_retry = 0;

	charger->is_pps_ready = false;
	charger->rise_speed_dly_ms = 10;
	charger->step_vbatt_mv = 4150;
	charger->step_iin_ma = 2000;
	charger->step_cv_cnt = 0;
	charger->mon_chk_time = 0;
	charger->ps_health = POWER_SUPPLY_HEALTH_GOOD;
	charger->dc_chg_status = POWER_SUPPLY_STATUS_DISCHARGING;
	charger->dc_sc_status = S2MU107_DC_SC_STATUS_OFF;
	charger->chk_vbat_margin = 0;
	charger->is_rampup_adjusted = false;
	charger->is_plugout_mask = false;
	charger->is_autopps_disabled = false;
	charger->chk_vbat_charged = 0;
	charger->chk_vbat_prev = 0;
	charger->chk_iin_prev = 0;

	/* Default Thermal Setting */
	s2mu107_write_reg(charger->i2c, 0x55, 0x7F);
	s2mu107_write_reg(charger->i2c, 0x56, 0x3F);

	return true;
}

static void s2mu107_dc_step_cv(struct s2mu107_dc_data *charger)
{
	int iavg = 0;
	int vavg = 0, ret;
	struct power_supply *psy;
	union power_supply_propval value;

	iavg = s2mu107_dc_get_fg_value(charger, POWER_SUPPLY_PROP_CURRENT_AVG);
	vavg = s2mu107_dc_get_fg_value(charger, POWER_SUPPLY_PROP_VOLTAGE_AVG);
	pr_info("%s, iavg : %d, vavg : %d, step_vbatt_mv : %d, step_cv_cnt : %d\n",
		__func__, iavg, vavg, charger->step_vbatt_mv, charger->step_cv_cnt++);

	if (charger->targetIIN <= DC_TOPOFF_CURRENT_MA) {
		/* Chk for Done */
		s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_TOP_OFF_CURRENT);
		return;
	}

	if (iavg < DC_TOPOFF_CURRENT_MA) {
		psy = power_supply_get_by_name(charger->pdata->sec_dc_name);
		if (!psy) {
			pr_err("%s, can't access power_supply", __func__);
			return;
		}

		value.intval = SEC_DIRECT_CHG_MODE_DIRECT_DONE;
		ret = power_supply_set_property(psy,
			(enum power_supply_property)POWER_SUPPLY_EXT_PROP_DIRECT_CHARGER_MODE, &value);

		/* Chk for Done */
		s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_TOP_OFF_CURRENT);
		return;
	}

	/* Step CV Op */
	if (vavg >= charger->step_vbatt_mv) {
		charger->targetIIN -= DC_TA_CURR_STEP_MA;
	}
	s2mu107_dc_send_fenced_pps(charger);

	if (vavg >= 4300)
		charger->alarm_interval = HV_ALARM_INTERVAL;
}

static void s2mu107_dc_step_cc(struct s2mu107_dc_data *charger)
{
	int vavg = 0;

	vavg = s2mu107_dc_get_fg_value(charger, POWER_SUPPLY_PROP_VOLTAGE_AVG);
	pr_info("%s, vavg : %d, float voltage : %d\n", __func__, vavg, charger->step_vbatt_mv);

	if (vavg >= charger->step_vbatt_mv) {
		s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_FLOAT_VBATT);
	}
}

static void s2mu107_dc_monitor_work(struct work_struct *work)
{
	struct s2mu107_dc_data *charger =
		container_of(work, struct s2mu107_dc_data, dc_monitor_work.work);

	if (!charger->is_charging) {
		pr_info("%s, skip work\n", __func__);
		wake_unlock(&charger->dc_mon_wake_lock);
		alarm_cancel(&charger->dc_monitor_alarm);
		return;
	}

	mutex_lock(&charger->dc_mon_mutex);
	pr_info("%s: %s, duration : %ld s", __func__,
		state_to_str(charger->dc_state), s2mu107_dc_update_time_chk(charger));

	switch (charger->dc_state) {
	case DC_STATE_ADJUSTED_CC:
	case DC_STATE_SLEEP_CC:
		s2mu107_dc_step_cc(charger);
		break;
	case DC_STATE_CV:
		s2mu107_dc_step_cv(charger);
		break;
	default:
		pr_info("%s, stop work, state : %s\n", __func__, state_to_str(charger->dc_state));
		alarm_cancel(&charger->dc_monitor_alarm);
		goto SKIP_WORK;
	}
	s2mu107_dc_test_read(charger->i2c);

	alarm_cancel(&charger->dc_monitor_alarm);
	alarm_start_relative(&charger->dc_monitor_alarm, ktime_set(charger->alarm_interval, 0));

	msleep(50);
SKIP_WORK:
	wake_unlock(&charger->dc_mon_wake_lock);
	mutex_unlock(&charger->dc_mon_mutex);
}

static void s2mu107_dc_chk_valid_work(struct work_struct *work)
{
	struct s2mu107_dc_data *charger =
		container_of(work, struct s2mu107_dc_data, chk_valid_wqueue.work);

	if (charger->dc_state == DC_STATE_PRESET) {
		charger->ps_health = POWER_SUPPLY_HEALTH_DC_ERR;
		pr_err("%s err status\n", __func__);
		s2mu107_dc_state_trans_enqueue(charger, DC_TRANS_FAIL_INT);
	}
}

static void s2mu107_dc_update_work(struct work_struct *work)
{
	struct s2mu107_dc_data *charger =
		container_of(work, struct s2mu107_dc_data, update_wqueue.work);
	s2mu107_dc_platform_state_update(charger);
}

static enum alarmtimer_restart dc_monitor_alarm(
	struct alarm *alarm, ktime_t now)
{
	struct s2mu107_dc_data *charger = container_of(alarm,
				struct s2mu107_dc_data, dc_monitor_alarm);

	wake_lock(&charger->dc_mon_wake_lock);
	schedule_delayed_work(&charger->dc_monitor_work, msecs_to_jiffies(500));

	return ALARMTIMER_NORESTART;
}

static int s2mu107_dc_parse_dt(struct device *dev,
		struct s2mu107_dc_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mu107-direct-charger");
	int ret = 0;

	if (!np) {
		pr_err("%s np NULL(s2mu107-direct-charger)\n", __func__);
		ret = -1;
		goto err;
	}

	ret = of_property_read_string(np, "dc,direct_charger_name",
				(char const **)&pdata->dc_name);
	if (ret < 0)
		pr_info("%s: Direct Charger name is Empty\n", __func__);

	ret = of_property_read_u32(np, "dc,input_current_limit",
			&pdata->input_current_limit);
	if (ret) {
		pr_info("%s: dc,input_current_limit Empty default 4000\n", __func__);
		pdata->input_current_limit = DC_MAX_INPUT_CURRENT_MA;
	}

	ret = of_property_read_u32(np, "dc,topoff_current",
			&pdata->topoff_current);
	if (ret) {
		pr_info("%s: dc,topoff_current Empty default 2000\n", __func__);
		pdata->topoff_current = DC_TOPOFF_CURRENT_MA;
	}

	ret = of_property_read_u32(np, "dc,temperature_source",
			&pdata->temperature_source);
	if (ret) {
		pr_info("%s: dc,temperature_source empty, default NTC\n", __func__);
		pdata->temperature_source = 1;
	}

	pr_info("%s DT file parsed succesfully\n", __func__);
	return 0;

err:
	pr_info("%s, direct charger parsing failed\n", __func__);
	return -1;
}

ssize_t s2mu107_dc_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t s2mu107_dc_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);
#define S2MU107_DC_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = s2mu107_dc_show_attrs,			\
	.store = s2mu107_dc_store_attrs,			\
}
enum {
	CHIP_ID = 0,
	DATA
};
static struct device_attribute s2mu107_dc_attrs[] = {
	S2MU107_DC_ATTR(chip_id),
	S2MU107_DC_ATTR(data),
};
static int s2mu107_dc_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(s2mu107_dc_attrs); i++) {
		rc = device_create_file(dev, &s2mu107_dc_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &s2mu107_dc_attrs[i]);
	return rc;
}

ssize_t s2mu107_dc_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mu107_dc_data *charger = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - s2mu107_dc_attrs;
	int i = 0;
	u8 addr, data;

	switch (offset) {
	case CHIP_ID:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n", charger->rev_id);
		break;
	case DATA:
		for (addr = 0x0B; addr <= 0x0E; addr++) {
			s2mu107_read_reg(charger->i2c, addr, &data);
			i += scnprintf(buf + i, PAGE_SIZE - i,
				       "0x%02x : 0x%02x\n", addr, data);
		}
		for (addr = 0x41; addr <= 0x5A; addr++) {
			s2mu107_read_reg(charger->i2c, addr, &data);
			i += scnprintf(buf + i, PAGE_SIZE - i,
				       "0x%02x : 0x%02x\n", addr, data);
		}
		break;
	default:
		return -EINVAL;
	}
	return i;
}

ssize_t s2mu107_dc_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mu107_dc_data *charger = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - s2mu107_dc_attrs;
	int ret = 0;
	int x, y;

	switch (offset) {
	case CHIP_ID:
		ret = count;
		break;
	case DATA:
		if (sscanf(buf, "0x%8x 0x%8x", &x, &y) == 2) {
			if (x >= 0x00 && x <= 0x2F) {
				u8 addr = x;
				u8 data = y;

				if (s2mu107_write_reg(charger->i2c, addr, data) < 0) {
					dev_info(charger->dev,
						"%s: addr: 0x%x write fail\n", __func__, addr);
				}
			} else {
				dev_info(charger->dev,
					"%s: addr: 0x%x is wrong\n", __func__, x);
			}
		}
		ret = count;
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

/* if need to set s2mu107 pdata */
static const struct of_device_id s2mu107_direct_charger_match_table[] = {
	{ .compatible = "samsung,s2mu107-direct-charger",},
	{},
};

static int s2mu107_direct_charger_probe(struct platform_device *pdev)
{
	struct s2mu107_dev *s2mu107 = dev_get_drvdata(pdev->dev.parent);
	struct s2mu107_platform_data *pdata = dev_get_platdata(s2mu107->dev);
	struct s2mu107_dc_data *charger;
	struct power_supply_config psy_cfg = {};
	int ret = 0;
	s2mu107_dc_pd_data_t *pd_data;

	pr_info("%s: S2MU107 Direct Charger driver probe\n", __func__);
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	pd_data = kzalloc(sizeof(*pd_data), GFP_KERNEL);
	if (unlikely(!pd_data)) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_pd_data_alloc;
	}
	charger->pd_data = pd_data;

	mutex_init(&charger->dc_mutex);
	mutex_init(&charger->pps_isr_mutex);
	mutex_init(&charger->dc_state_mutex);
	mutex_init(&charger->timer_mutex);
	mutex_init(&charger->trans_mutex);
	mutex_init(&charger->dc_mon_mutex);
	mutex_init(&charger->auto_pps_mutex);
	wake_lock_init(&charger->wake_lock, WAKE_LOCK_SUSPEND, "dc_wake");
	wake_lock_init(&charger->mode_irq, WAKE_LOCK_SUSPEND, "dc_mode_irq");
	wake_lock_init(&charger->fail_irq, WAKE_LOCK_SUSPEND, "dc_fail_irq");
	wake_lock_init(&charger->thermal_irq, WAKE_LOCK_SUSPEND, "dc_thermal_irq");
	wake_lock_init(&charger->dc_mon_wake_lock, WAKE_LOCK_SUSPEND, "dc_mon_wake");
	wake_lock_init(&charger->state_manager_wake, WAKE_LOCK_SUSPEND, "state_man_wake");

	charger->dev = &pdev->dev;
	charger->i2c = s2mu107->chg;
	charger->i2c_common = s2mu107->i2c;
	charger->rev_id = s2mu107->pmic_rev;

	charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)),
			GFP_KERNEL);
	if (!charger->pdata) {
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = s2mu107_dc_parse_dt(&pdev->dev, charger->pdata);
	if (ret < 0)
		goto err_parse_dt;

	platform_set_drvdata(pdev, charger);

	if (charger->pdata->dc_name == NULL)
		charger->pdata->dc_name = "s2mu107-direct-charger";

	if (charger->pdata->pm_name == NULL)
		charger->pdata->pm_name = "s2mu107-pmeter";

	if (charger->pdata->fg_name == NULL)
		charger->pdata->fg_name = "s2mu107-fuelgauge";

	if (charger->pdata->sc_name == NULL)
		charger->pdata->sc_name = "s2mu107-switching-charger";

	if (charger->pdata->sec_dc_name == NULL)
		charger->pdata->sec_dc_name = "sec-direct-charger";

	charger->psy_dc_desc.name           = charger->pdata->dc_name;
	charger->psy_dc_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_dc_desc.get_property   = s2mu107_dc_get_property;
	charger->psy_dc_desc.set_property   = s2mu107_dc_set_property;
	charger->psy_dc_desc.properties     = s2mu107_dc_props;
	charger->psy_dc_desc.num_properties = ARRAY_SIZE(s2mu107_dc_props);

	charger->psy_pmeter = get_power_supply_by_name("s2mu107-pmeter");
	if (!charger->psy_pmeter) {
		pr_err("%s: Fail to get pmeter\n", __func__);
		goto err_get_pmeter;
	}

	s2mu107_dc_init(charger);

	psy_cfg.drv_data = charger;
	psy_cfg.supplied_to = s2mu107_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(s2mu107_supplied_to);

	charger->psy_dc = power_supply_register(&pdev->dev, &charger->psy_dc_desc, &psy_cfg);
	if (IS_ERR(charger->psy_dc)) {
		pr_err("%s: Failed to Register psy_dc\n", __func__);
		ret = PTR_ERR(charger->psy_dc);
		goto err_power_supply_register;
	}

	charger->psy_fg = power_supply_get_by_name(charger->pdata->fg_name);
	if (!charger->psy_fg)
		goto err_get_psy;

	charger->psy_sc = power_supply_get_by_name(charger->pdata->sc_name);
	if (!charger->psy_sc)
		goto err_get_psy;

	INIT_DELAYED_WORK(&charger->timer_wqueue,
		s2mu107_dc_timer_work);
	INIT_DELAYED_WORK(&charger->start_cc_wqueue,
		s2mu107_dc_start_cc_work);
	INIT_DELAYED_WORK(&charger->check_vbat_wqueue,
		s2mu107_dc_check_vbat_work);
	INIT_DELAYED_WORK(&charger->state_manager_wqueue,
		s2mu107_dc_state_manager_work);
	INIT_DELAYED_WORK(&charger->set_prop_wqueue,
		s2mu107_dc_set_prop_work);
	INIT_DELAYED_WORK(&charger->dc_monitor_work,
		s2mu107_dc_monitor_work);
	INIT_DELAYED_WORK(&charger->chk_valid_wqueue,
		s2mu107_dc_chk_valid_work);
	INIT_DELAYED_WORK(&charger->update_wqueue,
		s2mu107_dc_update_work);

	alarm_init(&charger->dc_monitor_alarm, ALARM_BOOTTIME, dc_monitor_alarm);
	charger->alarm_interval = DEFAULT_ALARM_INTERVAL;

	/* Init work & alarm for monitoring */
	init_waitqueue_head(&charger->wait);
#if defined(BRINGUP)
	charger->timer_wqueue = create_singlethread_workqueue("dc-wq");
	if (!charger->timer_wqueue) {
		pr_info("%s: failed to create wq.\n", __func__);
		ret = -ESRCH;
		goto err_create_wq;
	}
#endif
	charger->irq_base = pdata->irq_base;
	s2mu107_dc_irq_init(charger);

#if EN_TEST_READ
	s2mu107_dc_test_read(charger->i2c);
#endif
	ret = s2mu107_dc_create_attrs(&charger->psy_dc->dev);
	if (ret) {
		dev_err(charger->dev,
			"%s : Failed to create_attrs\n", __func__);
	}
	pr_info("%s: S2MU107 Direct Charger driver loaded OK\n", __func__);

	return 0;

#if defined(BRINGUP)
err_create_wq:
#endif
err_get_psy:
	power_supply_unregister(charger->psy_dc);
err_power_supply_register:
err_get_pmeter:
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&charger->dc_mutex);
	mutex_destroy(&charger->pps_isr_mutex);
	mutex_destroy(&charger->dc_state_mutex);
	mutex_destroy(&charger->timer_mutex);
	mutex_destroy(&charger->trans_mutex);
	mutex_destroy(&charger->dc_mon_mutex);
	mutex_destroy(&charger->auto_pps_mutex);
	wake_lock_destroy(&charger->wake_lock);
	wake_lock_destroy(&charger->mode_irq);
	wake_lock_destroy(&charger->fail_irq);
	wake_lock_destroy(&charger->thermal_irq);
	wake_lock_destroy(&charger->dc_mon_wake_lock);
	wake_lock_destroy(&charger->state_manager_wake);
	kfree(charger->pd_data);
err_pd_data_alloc:
	kfree(charger);
	return ret;
}

static int s2mu107_direct_charger_remove(struct platform_device *pdev)
{
	struct s2mu107_dc_data *charger = platform_get_drvdata(pdev);

	power_supply_unregister(charger->psy_dc);
	mutex_destroy(&charger->dc_mutex);
	mutex_destroy(&charger->pps_isr_mutex);
	mutex_destroy(&charger->dc_state_mutex);
	mutex_destroy(&charger->timer_mutex);
	mutex_destroy(&charger->trans_mutex);
	mutex_destroy(&charger->dc_mon_mutex);
	mutex_destroy(&charger->auto_pps_mutex);
	wake_lock_destroy(&charger->wake_lock);
	wake_lock_destroy(&charger->mode_irq);
	wake_lock_destroy(&charger->fail_irq);
	wake_lock_destroy(&charger->thermal_irq);
	wake_lock_destroy(&charger->dc_mon_wake_lock);
	wake_lock_destroy(&charger->state_manager_wake);
	kfree(charger->pd_data);
	kfree(charger);
	return 0;
}

#if defined CONFIG_PM
static int s2mu107_direct_charger_prepare(struct device *dev)
{
	return 0;
}

static int s2mu107_direct_charger_suspend(struct device *dev)
{
	return 0;
}

static int s2mu107_direct_charger_resume(struct device *dev)
{
	return 0;
}

static void s2mu107_direct_charger_complete(struct device *dev)
{
	return;
}

#else
#define s2mu107_direct_charger_suspend NULL
#define s2mu107_direct_charger_resume NULL
#endif

static void s2mu107_direct_charger_shutdown(struct device *dev)
{
	pr_info("%s: S2MU107 Direct Charger driver shutdown\n", __func__);
}

static const struct dev_pm_ops s2mu107_direct_charger_pm_ops = {
	.prepare = s2mu107_direct_charger_prepare,
	.suspend = s2mu107_direct_charger_suspend,
	.resume = s2mu107_direct_charger_resume,
	.complete = s2mu107_direct_charger_complete,
};

static struct platform_driver s2mu107_direct_charger_driver = {
	.driver         = {
		.name   = "s2mu107-direct-charger",
		.owner  = THIS_MODULE,
		.of_match_table = s2mu107_direct_charger_match_table,
		.pm     = &s2mu107_direct_charger_pm_ops,
		.shutdown   =   s2mu107_direct_charger_shutdown,
	},
	.probe          = s2mu107_direct_charger_probe,
	.remove     = s2mu107_direct_charger_remove,
};

static int __init s2mu107_direct_charger_init(void)
{
	int ret = 0;
	pr_info("%s start\n", __func__);
	ret = platform_driver_register(&s2mu107_direct_charger_driver);

	return ret;
}
module_init(s2mu107_direct_charger_init);

static void __exit s2mu107_direct_charger_exit(void)
{
	platform_driver_unregister(&s2mu107_direct_charger_driver);
}
module_exit(s2mu107_direct_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Suji Lee <suji0908.lee@samsung.com>, Sejong Park <sejong123.park@samsung.com>");
MODULE_DESCRIPTION("Charger driver for S2MU107");
