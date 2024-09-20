/*
 * s2mu106_fuelgauge.c - S2MU106 Fuel Gauge Driver
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define SINGLE_BYTE	1
#define TABLE_SIZE	22

#include "include/fuelgauge/s2mu106_fuelgauge.h"
#include <linux/of_gpio.h>
#include "include/sec_charging_common.h"

static enum power_supply_property s2mu106_fuelgauge_props[] = {
};

#if defined(CONFIG_FUELGAUGE_S2MU106_USE_5MILLIOHM)
static void s2mu106_set_trim_5mohm(struct s2mu106_fuelgauge_data *fuelgauge);
#endif
static int s2mu106_get_vbat(struct s2mu106_fuelgauge_data *fuelgauge);
static int s2mu106_get_ocv(struct s2mu106_fuelgauge_data *fuelgauge);
static int s2mu106_get_current(struct s2mu106_fuelgauge_data *fuelgauge);
static int s2mu106_get_avgcurrent(struct s2mu106_fuelgauge_data *fuelgauge);
static int s2mu106_get_avgvbat(struct s2mu106_fuelgauge_data *fuelgauge);

static int s2mu106_read_reg_byte(struct i2c_client *client, int reg, void *data)
{
	int ret = 0;
	int cnt = 0;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		while (ret < 0 && cnt < 5) {
			ret = i2c_smbus_read_byte_data(client, reg);
			cnt++;
			dev_err(&client->dev,
					"%s: I2C read Incorrect! reg:0x%x, data:0x%x, cnt:%d\n",
					__func__, reg, *(u8 *)data, cnt);
		}
		if (cnt == 5)
			dev_err(&client->dev,
				"%s: I2C read Failed reg:0x%x, data:0x%x\n",
				__func__, reg, *(u8 *)data);
	}
	*(u8 *)data = (u8)ret;

	return ret;
}

static int s2mu106_write_and_verify_reg_byte(struct i2c_client *client, int reg, u8 data)
{
	int ret, i = 0;
	int i2c_corrupted_cnt = 0;
	u8 temp = 0;

	ret = i2c_smbus_write_byte_data(client, reg, data);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = i2c_smbus_write_byte_data(client, reg, data);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}

	/* Skip non-writable registers */
	if ((reg == 0xee) || (reg == 0xef) || (reg == 0xf2) || (reg == 0xf3) ||
		(reg == 0x0C) || (reg == 0x1e) || (reg == 0x1f) || (reg == 0x27)) {
		return ret;
	}

	s2mu106_read_reg_byte(client, reg, &temp);
	while ((temp != data) && (i2c_corrupted_cnt < 5)) {
		dev_err(&client->dev,
			"%s: I2C write Incorrect! REG: 0x%x Expected: 0x%x Real-Value: 0x%x\n",
			__func__, reg, data, temp);
		ret = i2c_smbus_write_byte_data(client, reg, data);
		s2mu106_read_reg_byte(client, reg, &temp);
		i2c_corrupted_cnt++;
	}

	if (i2c_corrupted_cnt == 5)
		dev_err(&client->dev,
			"%s: I2C write failed REG: 0x%x Expected: 0x%x\n",
			__func__, reg, data);

	return ret;
}

static int s2mu106_write_reg(struct i2c_client *client, int reg, u8 *buf)
{
#if SINGLE_BYTE
	int ret = 0;
	s2mu106_write_and_verify_reg_byte(client, reg, buf[0]);
	s2mu106_write_and_verify_reg_byte(client, reg+1, buf[1]);
#else
	int ret, i = 0;

	ret = i2c_smbus_write_i2c_block_data(client, reg, 2, buf);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = i2c_smbus_write_i2c_block_data(client, reg, 2, buf);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}
#endif
	return ret;
}

static int s2mu106_read_reg(struct i2c_client *client, int reg, u8 *buf)
{

#if SINGLE_BYTE
	int ret = 0;
	u8 data1 = 0, data2 = 0;
	s2mu106_read_reg_byte(client, reg, &data1);
	s2mu106_read_reg_byte(client, reg+1, &data2);
	buf[0] = data1;
	buf[1] = data2;
#else
	int ret = 0, i = 0;

	ret = i2c_smbus_read_i2c_block_data(client, reg, 2, buf);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = i2c_smbus_read_i2c_block_data(client, reg, 2, buf);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}
#endif
	return ret;
}

#define FULL_CAPACITY 850
static int calc_ttf_to_full_capacity(struct s2mu106_fuelgauge_data *fuelgauge,
		    union power_supply_propval *val)
{
	int i;
	int cc_time = 0, cv_time = 0;

	int soc = FULL_CAPACITY;
	int charge_current = val->intval;
	struct cv_slope *cv_data = fuelgauge->cv_data;
	int design_cap = fuelgauge->ttf_capacity;

	if (!cv_data || (val->intval <= 0)) {
		pr_info("%s: no cv_data or val: %d\n", __func__, val->intval);
		return -1;
	}
	for (i = 0; i < fuelgauge->cv_data_length; i++) {
		if (charge_current >= cv_data[i].fg_current)
			break;
	}
	i = i >= fuelgauge->cv_data_length ? fuelgauge->cv_data_length - 1 : i;
	if (cv_data[i].soc < soc) {
		for (i = 0; i < fuelgauge->cv_data_length; i++) {
			if (soc <= cv_data[i].soc)
				break;
		}
		cv_time =
		    ((cv_data[i - 1].time - cv_data[i].time) * (cv_data[i].soc - soc)
		     / (cv_data[i].soc - cv_data[i - 1].soc)) + cv_data[i].time;
	} else {		/* CC mode || NONE */
		cv_time = cv_data[i].time;
		cc_time = design_cap * (cv_data[i].soc - soc)
		    / val->intval * 3600 / 1000;
		pr_debug("%s: cc_time: %d\n", __func__, cc_time);
		if (cc_time < 0)
			cc_time = 0;
	}

	pr_debug
	    ("%s: cap: %d, soc: %4d, T: %6d, avg: %4d, cv soc: %4d, i: %4d, val: %d\n",
	     __func__, design_cap, soc, cv_time + cc_time,
	     fuelgauge->current_avg, cv_data[i].soc, i, val->intval);

	if (cv_time + cc_time >= 0)
		return cv_time + cc_time;
	else
		return 0;
}

static int calc_ttf(struct s2mu106_fuelgauge_data *fuelgauge,
		    union power_supply_propval *val)
{
	struct cv_slope *cv_data = fuelgauge->cv_data;
	int i, cc_time = 0, cv_time = 0;
	int soc = fuelgauge->raw_capacity;
	int charge_current = val->intval;
	int design_cap = fuelgauge->ttf_capacity;

	if (!cv_data || (val->intval <= 0)) {
		pr_info("%s: no cv_data or val: %d\n", __func__, val->intval);
		return -1;
	}
	for (i = 0; i < fuelgauge->cv_data_length; i++) {
		if (charge_current >= cv_data[i].fg_current)
			break;
	}
	i = i >= fuelgauge->cv_data_length ? fuelgauge->cv_data_length - 1 : i;
	if (cv_data[i].soc < soc) {
		for (i = 0; i < fuelgauge->cv_data_length; i++) {
			if (soc <= cv_data[i].soc)
				break;
		}
		cv_time =
		    ((cv_data[i - 1].time - cv_data[i].time) * (cv_data[i].soc - soc)
		     / (cv_data[i].soc - cv_data[i - 1].soc)) + cv_data[i].time;
	} else {		/* CC mode || NONE */
		cv_time = cv_data[i].time;
		cc_time =
			design_cap * (cv_data[i].soc - soc) / val->intval * 3600 / 1000;
		pr_debug("%s: cc_time: %d\n", __func__, cc_time);
		if (cc_time < 0)
			cc_time = 0;
	}

	pr_debug("%s: cap: %d, soc: %4d, T: %6d, avg: %4d, cv soc: %4d, i: %4d, val: %d\n",
	     __func__, design_cap, soc, cv_time + cc_time,
	     fuelgauge->current_avg, cv_data[i].soc, i, val->intval);

	if (cv_time + cc_time >= 0)
		return cv_time + cc_time + 60;
	else
		return 60;	/* minimum 1minutes */
}

static void s2mu106_fg_test_read(struct i2c_client *client)
{
	static int reg_list[] = {
		0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0E, 0x0F,
		0x10, 0x11, 0x14, 0x1A, 0x1B, 0x1E, 0x1F, 0x24, 0x25, 0x26,
		0x27, 0x28, 0x29, 0x40, 0x41, 0x43, 0x44, 0x45, 0x48, 0x4A,
		0x4B, 0x50, 0x51, 0x52, 0x53, 0x58, 0x59, 0x5A, 0x5B, 0x5C,
		0x67
	};
	u8 data = 0;
	char str[1016] = {0,};
	int i = 0, reg_list_size = 0;

	reg_list_size = ARRAY_SIZE(reg_list);
	for (i = 0; i < reg_list_size; i++) {
		s2mu106_read_reg_byte(client, reg_list[i], &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", reg_list[i], data);
	}

	/* print buffer */
	pr_info("[FG]%s: %s\n", __func__, str);
}

static void s2mu106_fg_periodic_read_power(struct s2mu106_fuelgauge_data *fuelgauge) {
	union power_supply_propval value;

	psy_do_property("s2mu106_pmeter", get,
			POWER_SUPPLY_PROP_VCHGIN, value);
	psy_do_property("s2mu106_pmeter", get,
			POWER_SUPPLY_PROP_VWCIN, value);
	psy_do_property("s2mu106_pmeter", get,
			POWER_SUPPLY_PROP_VSYS, value);
	psy_do_property("s2mu106_pmeter", get,
			POWER_SUPPLY_PROP_ICHGIN, value);
	psy_do_property("s2mu106_pmeter", get,
			POWER_SUPPLY_PROP_IWCIN, value);
#if defined(CONFIG_WIRELESS_TX_MODE)
	psy_do_property("s2mu106_pmeter", get,
			POWER_SUPPLY_PROP_ITX, value);
#endif
}

int s2mu106_fg_check_current_level(struct s2mu106_fuelgauge_data *fuelgauge)
{
	int ret_val = 500;
	int temp = 0;

	if (fuelgauge->cable_type == SEC_BATTERY_CABLE_USB) {
		return ret_val;
	}

	/* topoff current * 1.6 except USB */
	temp = fuelgauge->topoff_current * 16;
	ret_val = temp / 10;

	return ret_val;
}

static void s2mu106_reset_fg(struct s2mu106_fuelgauge_data *fuelgauge)
{
	int i;
	u8 temp = 0;

	mutex_lock(&fuelgauge->fg_lock);

	/* step 0: [Surge test] initialize register of FG */
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x0E,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[0]);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x0F,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[1]);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x10,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[2]);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x11,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[3]);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x13,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].volt_mode_tunning);
	fuelgauge->batcap_0x0E = fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[0];
	fuelgauge->batcap_0x0F = fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[1];
#else
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x0E, fuelgauge->info.batcap[0]);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x0F, fuelgauge->info.batcap[1]);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x10, fuelgauge->info.batcap[2]);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x11, fuelgauge->info.batcap[3]);
	fuelgauge->batcap_0x0E = fuelgauge->info.batcap[0];
	fuelgauge->batcap_0x0F = fuelgauge->info.batcap[1];
#endif
	/* After battery capacity update, set BATCAP_OCV_EN(0x0C[6]=1) */
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x0C, &temp);
	temp |= 0x40;
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x0C, temp);
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	for(i = 0x92; i <= 0xe9; i++) {
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, i,
			fuelgauge->age_data_info[fuelgauge->fg_age_step].battery_table3[i - 0x92]);
	}
	for(i = 0xea; i <= 0xff; i++) {
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, i,
			fuelgauge->age_data_info[fuelgauge->fg_age_step].battery_table4[i - 0xea]);
	}
#else
	for (i = 0x92; i <= 0xe9; i++)
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, i, fuelgauge->info.battery_table3[i - 0x92]);
	for (i = 0xea; i <= 0xff; i++)
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, i, fuelgauge->info.battery_table4[i - 0xea]);
#endif

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x14, 0x67);
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x44,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].accum[0]);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x45, &temp);
	temp &= 0xF0;
	temp |= fuelgauge->age_data_info[fuelgauge->fg_age_step].accum[1];
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x45, temp);
#else
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x44, fuelgauge->info.accum[0]);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x45, &temp);
	temp &= 0xF0;
	temp |= fuelgauge->info.accum[1];
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x45, temp);
#endif
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x4B, &temp);
	temp &= 0x8F;
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4B, temp);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0x10);

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x40, 0x08);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x41, 0x04);

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x5C, fuelgauge->val_0x5C);

	/* Dumpdone. Re-calculate SOC */
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1E, 0x0F);
	msleep(300);

	/* If it was voltage mode, recover it */
	if (fuelgauge->mode == HIGH_SOC_VOLTAGE_MODE) {
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0xFF);
		s2mu106_read_reg_byte(fuelgauge->i2c, 0x4B, &temp);
		temp |= 0x70;
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4B, temp);
	}

	mutex_unlock(&fuelgauge->fg_lock);

	pr_info("%s: Reset FG completed\n", __func__);
}

static int s2mu106_fix_rawsoc_reset_fg(struct s2mu106_fuelgauge_data *fuelgauge)
{
	int ret = 0, ui_soc = 0, f_soc = 0;
	u8 data;
	union power_supply_propval value;

	if (!fuelgauge->psy_bat)
		fuelgauge->psy_bat = power_supply_get_by_name("battery");
	if (!fuelgauge->psy_bat)
		return -EINVAL;
	ret = power_supply_get_property(fuelgauge->psy_bat, POWER_SUPPLY_PROP_CAPACITY, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	dev_info(&fuelgauge->i2c->dev, "%s: UI SOC = %d\n", __func__, value.intval);

	ui_soc = value.intval;

	f_soc = (ui_soc << 8) / 100;

	if (f_soc > 0xFF)
		f_soc = 0xFF;

	f_soc |= 0x1;

	data = (u8)f_soc;

	/* Set rawsoc fix & enable */
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x29, data);

	s2mu106_reset_fg(fuelgauge);

	/* Disable rawsoc fix */
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x29, 0x00);

	dev_info(&fuelgauge->i2c->dev, "%s: Finish\n", __func__);

	return ret;
}

static void s2mu106_fg_reset_capacity_by_jig_connection(struct s2mu106_fuelgauge_data *fuelgauge)
{
	/* TODO : model data version check */
	u8 data = 0;

	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_FG_ID, &data);
	data &= 0xF0;
	data |= 0x0F; //set model data version 0xF for next boot up initializing fuelgague
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_FG_ID, data);

	pr_info("%s: set Model data version (0x%x)\n", __func__, data & 0x0F);
}

static void s2mu106_restart_gauging(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];

	pr_info("%s: Re-calculate SOC and voltage\n", __func__);

	mutex_lock(&fuelgauge->fg_lock);

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1E, 0x0F);

	msleep(300);

#if (TEMP_COMPEN)
	/* Need to re-init temperature compensation */
	fuelgauge->init_start = 1;
	/* Make saved UI SOC invalid. Set S2MU106_REG_RSOC_R + 1 reg. */
	data[0] = 0;
	data[1] = 1;
	s2mu106_write_reg(fuelgauge->i2c, S2MU106_REG_RSOC_R, data);
#endif

	mutex_unlock(&fuelgauge->fg_lock);
}

static void s2mu106_init_regs(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 temp = 0;
	pr_info("%s: s2mu106 fuelgauge initialize\n", __func__);

	/* Save register values for surge check */
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x53, &temp);
	fuelgauge->reg_OTP_53 = temp;
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x52, &temp);
	fuelgauge->reg_OTP_52 = temp;

	/* Disable VM3_flag_EN */
	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_VM, &temp);
	temp = temp & 0xFB;
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_VM, temp);

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	fuelgauge->batcap_0x0E = fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[0];
	fuelgauge->batcap_0x0F = fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[1];
#else
	fuelgauge->batcap_0x0E = fuelgauge->info.batcap[0];
	fuelgauge->batcap_0x0F = fuelgauge->info.batcap[1];
#endif
}

static void s2mu106_alert_init(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];

	/* VBAT Threshold setting: 3.55V */
	data[0] = 0x00 & 0x0f;

	/* SOC Threshold setting */
	data[0] = data[0] | (fuelgauge->pdata->fuel_alert_soc << 4);

	data[1] = 0x00;
	s2mu106_write_reg(fuelgauge->i2c, S2MU106_REG_IRQ_LVL, data);
}

static int s2mu106_set_temperature(struct s2mu106_fuelgauge_data *fuelgauge,
			int temperature)
{
	/*
	 * s2mu106 include temperature sensor so,
	 * do not need to set temperature value.
	 */
	return temperature;
}

static int s2mu106_get_temperature(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int temperature = 0;

	mutex_lock(&fuelgauge->fg_lock);

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_MONOUT_SEL, 0x18);
	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_MONOUT, data) < 0)
		goto err;
/*	pr_info("%s temp data = 0x%x 0x%x\n", __func__, data[0], data[1]); */

	mutex_unlock(&fuelgauge->fg_lock);
	compliment = (data[1] << 8) | (data[0]);

	/* data[] store 2's compliment format number */
	if (compliment & (0x1 << 15)) {
		/* Negative */
		temperature = -1 * ((~compliment & 0xFFFF) + 1);
	} else {
		temperature = compliment & 0x7FFF;
	}
	temperature = ((temperature * 100) >> 8)/10;

	pr_info("%s: temperature (%d)\n", __func__, temperature);

	return temperature;
err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -ERANGE;
}

#if (TEMP_COMPEN)
static bool s2mu106_get_vm_status(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data = 0;

	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_STATUS, &data);

	return (data & (1 << 6)) ? true : false;
}

static int s2mu106_get_comp_socr(struct s2mu106_fuelgauge_data *fuelgauge)
{
	int comp_socr = 0;
	int t_socr = 0;
	int i_socr = 0;

	if (fuelgauge->temperature <= 0) {
		i_socr = (-1) * fuelgauge->i_socr_coeff * fuelgauge->avg_curr;
		t_socr = (((-1) * fuelgauge->low_t_compen_coeff) * fuelgauge->temperature
				+ fuelgauge->t_socr_coeff) / 1000;
	} else if (fuelgauge->temperature <= 200) {
		i_socr = (-1) * fuelgauge->i_socr_coeff * fuelgauge->avg_curr;
		t_socr = (((-1) * fuelgauge->t_compen_coeff) * fuelgauge->temperature
				+ fuelgauge->t_socr_coeff) / 1000;
	}

	comp_socr = ((t_socr + 1) * i_socr) / 100000;

	comp_socr = comp_socr - (comp_socr % 5);

	if (comp_socr > 80)
		comp_socr = 80;
	else if (comp_socr < 0)
		comp_socr = 0;

	pr_info("%s: SOCr = %d, T_SOCr = %d, I_SOCr = %d\n", __func__,
		comp_socr, t_socr, i_socr / 100000);

	return comp_socr;
}

static int s2mu106_get_soc_map(struct s2mu106_fuelgauge_data *fuelgauge,
		bool bat_charging, int comp_socr)
{
	int soc_map = 0;
	int curr = s2mu106_get_current(fuelgauge);

	if (bat_charging || (fuelgauge->is_charging && curr >= 30)) {
		if (fuelgauge->soc0i >= 9950)
			soc_map = 10000;
		else
			soc_map =
				((10040 - fuelgauge->socni) * (fuelgauge->rsoc - fuelgauge->soc0i)) /
				(10000 - fuelgauge->soc0i) + fuelgauge->socni;
	} else {
		if (fuelgauge->soc0i < ((100 * comp_socr) + 50))
			soc_map = 0;
		else
			soc_map =
				(fuelgauge->socni * (fuelgauge->rsoc - fuelgauge->soc0i)) /
				(fuelgauge->soc0i - (100 * comp_socr)) + fuelgauge->socni;
	}

	if (soc_map > 10000)
		soc_map = 10000;
	else if (soc_map < 0)
		soc_map = 0;

	return soc_map;
}

static void s2mu106_temperature_compensation(struct s2mu106_fuelgauge_data *fuelgauge)
{
	int soc_map = 0;
	int ui_soc = 0;
	u8 data[2];

	fuelgauge->comp_socr = s2mu106_get_comp_socr(fuelgauge);

	if (fuelgauge->init_start) {
		fuelgauge->flag_mapping = true;
		fuelgauge->pre_comp_socr = fuelgauge->comp_socr;
		fuelgauge->pre_vm_status = fuelgauge->vm_status;
	}

	if ((fuelgauge->pre_comp_socr != fuelgauge->comp_socr) ||
			(fuelgauge->pre_bat_charging != fuelgauge->bat_charging) ||
			(fuelgauge->pre_vm_status != fuelgauge->vm_status) ||
			(fuelgauge->pre_is_charging != fuelgauge->is_charging))
		fuelgauge->flag_mapping = true;

	if (fuelgauge->flag_mapping == true) {
		if (fuelgauge->init_start) {
			if (fuelgauge->temperature < fuelgauge->low_temp_limit) {
				s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_RSOC_R, data);

				if (data[1] == 0) {
					ui_soc = (data[1] << 8) | (data[0]);

					pr_info("%s: temperature is low. use saved UI SOC(%d)"
							" for mapping, data[1] = 0x%02x, data[0] = 0x%02x\n",
							__func__, ui_soc, data[1], data[0]);

					fuelgauge->ui_soc = ui_soc;
					fuelgauge->capacity_old = ui_soc;
					if (fuelgauge->temperature < fuelgauge->low_temp_limit)
						fuelgauge->initial_update_of_soc = false;

					/* UI SOC unit is 1% */
					ui_soc = ui_soc * 100;

					fuelgauge->socni = ui_soc;
					fuelgauge->soc0i = fuelgauge->rsoc;
				} else {
					pr_info("%s: temperature is low. but UI SOC is not saved\n",
							__func__);

					fuelgauge->socni = fuelgauge->rsoc;
					fuelgauge->soc0i = fuelgauge->rsoc;
				}
			} else {
				fuelgauge->socni = fuelgauge->rsoc;
				fuelgauge->soc0i = fuelgauge->rsoc;
			}
		} else {
			/* If the difference between SOC_M and SOC_R is 1% or more,
			SOC_R is mapped to follow SOC_M
			*/
			pr_info("%s: socni updated - SOC_M(%d), SOC_R(%d)\n",
						__func__, fuelgauge->rsoc, fuelgauge->soc_r);
			if (fuelgauge->rsoc > fuelgauge->soc_r + 100)
				fuelgauge->soc_r += 10;
			else if (fuelgauge->soc_r > fuelgauge->rsoc + 100)
				fuelgauge->soc_r -= 10;

			fuelgauge->socni = fuelgauge->soc_r;
			fuelgauge->soc0i = fuelgauge->rsoc;
		}
	}

	soc_map =  s2mu106_get_soc_map(fuelgauge,
			fuelgauge->bat_charging, fuelgauge->comp_socr);

	if (fuelgauge->flag_mapping == true) {
		if (fuelgauge->init_start) {
			if (fuelgauge->temperature >= fuelgauge->low_temp_limit ||
				((fuelgauge->temperature < fuelgauge->low_temp_limit) && (data[1] != 0))) {
				fuelgauge->soc_r = soc_map;
				pr_info("%s: When Initial Mapping, UI SOC = soc_r = soc_map\n", __func__, fuelgauge->soc_r);
				fuelgauge->ui_soc = fuelgauge->soc_r / 100;
				fuelgauge->capacity_old = fuelgauge->ui_soc;
			}
		}
	}
#if !defined(INC_OK_EN)
	/* Use is_charging flag for prevent SOC increase when not charging */
	if ((fuelgauge->is_charging == false) && (soc_map > fuelgauge->soc_r)) {
		if(fuelgauge->init_start)
			fuelgauge->soc_r = soc_map;
		else
			pr_info("%s: Not charging, do not reflect SOC increase. soc_map = %d, soc_r = %d\n",
					__func__, soc_map, fuelgauge->soc_r);
	}
	else
		fuelgauge->soc_r = soc_map;
#else
	fuelgauge->soc_r = soc_map;
#endif

	if (fuelgauge->vm_status && (fuelgauge->soc_r > fuelgauge->rsoc) &&
			(fuelgauge->temperature <= fuelgauge->low_temp_limit))
		fuelgauge->soc_r = fuelgauge->rsoc;

#if !(BATCAP_LEARN)
	pr_info("%s: SOC_M = %d, Chg_stat = %d, VM = %d, flag_mapping = %d, avgCURR = %d, avgTEMP = %d, "
			"SOCni = %d, SOC0i = %d, SOCr = %d, SOC_R = %d\n",
			__func__, fuelgauge->rsoc, fuelgauge->bat_charging, fuelgauge->vm_status,
			fuelgauge->flag_mapping, fuelgauge->avg_curr, fuelgauge->temperature,
			fuelgauge->socni, fuelgauge->soc0i, fuelgauge->comp_socr, fuelgauge->soc_r);
#endif
	fuelgauge->init_start = 0;
	fuelgauge->pre_comp_socr = fuelgauge->comp_socr;
	fuelgauge->pre_vm_status = fuelgauge->vm_status;
	fuelgauge->pre_is_charging = fuelgauge->is_charging;
	fuelgauge->pre_bat_charging = fuelgauge->bat_charging;
	fuelgauge->flag_mapping = false;

	/* Save UI SOC for maintain SOC, after low temperature reset */
	data[0] = fuelgauge->ui_soc;
	data[1] = 0;
	s2mu106_write_reg(fuelgauge->i2c, S2MU106_REG_RSOC_R, data);

	/* TODO: Print UI SOC & saved value for debugging */
	s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_RSOC_R, data);
	ui_soc = (data[1] << 8) | (data[0]);
	pr_info("%s: saved UI SOC = %d, data[1] = 0x%02x, data[0] = 0x%02x\n",
			__func__, ui_soc, data[1], data[0]);
}
#endif

#if (BATCAP_LEARN)
static int s2mu106_get_batcap_ocv(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 batcap_ocv = 0;

	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_RBATCAP, data) < 0)
		return -EINVAL;

	dev_dbg(&fuelgauge->i2c->dev, "%s: data0 (%d) data1 (%d) \n", __func__, data[0], data[1]);
	batcap_ocv = (data[0] + (data[1] << 8)) >> 2;

	return batcap_ocv;
}

static int s2mu106_get_cycle(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment, cycle;

	mutex_lock(&fuelgauge->fg_lock);

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_MONOUT_SEL, 0x27);

	msleep(50);

	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_MONOUT, data) < 0)
		goto err;
	compliment = (data[1] << 8) | (data[0]);

	cycle = compliment;

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_MONOUT_SEL, 0x10);

	mutex_unlock(&fuelgauge->fg_lock);

	return cycle;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

void s2mu106_batcap_learning(struct s2mu106_fuelgauge_data *fuelgauge)
{
	int bat_w = 0;
	u8 data[2], temp = 0;
	int range = (BAT_L_CON[5] == 0) ? 900:800;
	int gap_cap = 0;

	gap_cap = (fuelgauge->capcc * 1000) / fuelgauge->batcap_ocv;

	if ((gap_cap > range) && (gap_cap < 1100)) {
		if (BAT_L_CON[6])
			bat_w = ((fuelgauge->batcap_ocv * 75) + (fuelgauge->capcc * 25)) / 100;
		else
			bat_w = ((fuelgauge->batcap_ocv * 90) + (fuelgauge->capcc * 10)) / 100;

		if (BAT_L_CON[7]) {
			fuelgauge->batcap_ocv_fin = bat_w;
			bat_w = bat_w << 2;
			data[1] = (u8)((bat_w >> 8) & 0x00ff);
			data[0] = (u8)(bat_w & 0x00ff);

			mutex_lock(&fuelgauge->fg_lock);

			s2mu106_write_reg(fuelgauge->i2c, S2MU106_REG_RBATCAP, data);
			/* After battery capacity update, set BATCAP_OCV_EN(0x0C[6]=1) */
			s2mu106_read_reg_byte(fuelgauge->i2c, 0x0C, &temp);
			temp |= 0x40;
			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x0C, temp);

			mutex_unlock(&fuelgauge->fg_lock);
		}
	}

	pr_info("%s: gap_cap = %d, capcc = %d, batcap_ocv = %d, bat_w = %d\n",
			__func__, gap_cap, fuelgauge->capcc, fuelgauge->batcap_ocv, bat_w);
}

static int s2mu106_get_cap_cc(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data1 = 0, data0 = 0;
	int cap_cc = 0;

	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_CAPCC + 1, &data1);
	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_CAPCC, &data0);
	cap_cc = (data1 << 8) | data0;
	if (cap_cc & (1 << 15)) {
		cap_cc = (~cap_cc) + 1;
		cap_cc = cap_cc / 2;
		cap_cc = cap_cc * (-1);
	}
	else
		cap_cc /= 2;

	return cap_cc;
}

static int s2mu106_get_soh(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data1 = 0, data0 = 0;
	int original = 0, ret = -1;
	int batcap_ocv = s2mu106_get_batcap_ocv(fuelgauge);

	data0 = fuelgauge->batcap_0x0E;
	data1 = fuelgauge->batcap_0x0F;
	original = (data1 << 8) | data0;
	original = original >> 2;

	if (original != 0) {
		ret = (batcap_ocv * 100) / original;

		if (ret > 100)
			ret = 100;
	} else
		ret = 100;

	pr_info("%s: original batcap = %d, new_batcap = %d, soh = %d\n",
			__func__, original, batcap_ocv, ret);

	return ret;
}
#endif

#if (BATCAP_LEARN) || (TEMP_COMPEN)
static bool s2mu106_get_bat_charging(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data = 0;

	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_STATUS, &data);

	return (data & (1 << 5)) ? true : false;
}
#endif

#if (BATCAP_LEARN) && (TEMP_COMPEN)
static int s2mu106_get_fullcharge_cap(struct s2mu106_fuelgauge_data *fuelgauge)
{
	int ret = -1;
	int batcap_ocv = s2mu106_get_batcap_ocv(fuelgauge);

	ret = ((100 - fuelgauge->comp_socr) * batcap_ocv) / 100;

	return ret;
}

static int s2mu106_get_remaining_cap(struct s2mu106_fuelgauge_data *fuelgauge)
{
	int ret = -1;
	int fcc = s2mu106_get_fullcharge_cap(fuelgauge);

	ret = (fuelgauge->soc_r) * fcc / 10000;

	pr_info("%s: fcc = %d, remaining_cap = %d\n", __func__, fcc, ret);

	return ret;
}
#endif

#if defined(CONFIG_FUELGAUGE_S2MU106_USE_5MILLIOHM)
static void s2mu106_set_trim_5mohm(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 temp_58 = 0, temp_59 = 0, temp_5a = 0, temp_5b = 0, temp = 0;
	u32 cslope = 0, coffset = 0;

	s2mu106_read_reg_byte(fuelgauge->i2c, 0x58, &temp_58);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x59, &temp_59);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x5A, &temp_5a);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x5B, &temp_5b);

	cslope = ((temp_5b & 0xF0) << 12) | (temp_59 << 8) | temp_58;
	coffset = ((temp_5b & 0x0F) << 8) | temp_5a;

	pr_info("%s: before cslope = 0x%x, coffset = 0x%x", __func__,
		cslope, coffset);

	cslope = (cslope ^ 0xFFFFF) + 1;
	cslope = cslope * 2;
	cslope = (cslope ^ 0xFFFFF) + 1;

	if (coffset & (1 << 11)) {
		coffset = (coffset ^ 0xFFF) + 1;
		coffset = coffset * 2;
		coffset = (coffset ^ 0xFFF) + 1;
	} else {
		coffset = coffset * 2;
	}

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c,
		0x58, (cslope & 0xFF));
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c,
		0x59, (cslope & 0xFF00) >> 8);

	temp = ((cslope & 0xF0000) >> 12) | ((coffset & 0xF00) >> 8);
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c,
		0x5B, temp);

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c,
		0x5A, (coffset & 0xFF));

	/* Check written value */
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x58, &temp_58);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x59, &temp_59);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x5A, &temp_5a);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x5B, &temp_5b);

	cslope = ((temp_5b & 0xF0) << 12) | (temp_59 << 8) | temp_58;
	coffset = ((temp_5b & 0x0F) << 8) | temp_5a;

	pr_info("%s: after cslope = 0x%x, coffset = 0x%x", __func__,
		cslope, coffset);
}
#endif

static int s2mu106_get_rawsoc(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2], temp = 0;
	u16 compliment;
	u8 por_state = 0;
	u8 reg_1E = 0;
	u8 reg_OTP_52 = 0, reg_OTP_53 = 0;
	u8 reg_0x67 = 0;
#if defined(CONFIG_CHARGER_S2MU106)
	bool charging_enabled = false;
#endif
	int ret = 0;
	union power_supply_propval value = {0, };
	int float_voltage = 0;
	int avg_current = 0, avg_vbat = 0, vbat = 0, curr = 0;
	u8 fg_mode_reg = 0;
#if (BATCAP_LEARN)
	int BATCAP_L_VBAT;
#endif
	int is_swelling_status = 0;
	static int touch_low_voltage = false;
	static int low_voltage_limit_cnt = 0;

	s2mu106_read_reg_byte(fuelgauge->i2c, 0x1F, &por_state);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x53, &reg_OTP_53);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x52, &reg_OTP_52);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x1E, &reg_1E);

	dev_err(&fuelgauge->i2c->dev, "%s: OTP 52(%02x) 53(%02x), current 52(%02x) 53(%02x), "
			"0x1F(%02x), 0x1E(%02x)\n", __func__, fuelgauge->reg_OTP_52, fuelgauge->reg_OTP_53,
			reg_OTP_52, reg_OTP_53, por_state, reg_1E);

#if defined(CONFIG_CHARGER_S2MU106)
	if (!fuelgauge->psy_chg)
		fuelgauge->psy_chg = power_supply_get_by_name("s2mu106-charger");
	if (!fuelgauge->psy_chg) {
		dev_err(&fuelgauge->i2c->dev, "%s:  s2mu106-charger psy failed\n", __func__);
		return -EINVAL;
	}
#endif
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	if((((por_state != 0x00) || (reg_1E != 0x03)) && (fuelgauge->age_reset_status == 0)) ||
#else
	if(((por_state != 0x00) || (reg_1E != 0x03)) ||
#endif
		(fuelgauge->probe_done == true &&
		(fuelgauge->reg_OTP_52 != reg_OTP_52 || fuelgauge->reg_OTP_53 != reg_OTP_53))) {
		/* check charging enable */
#if defined(CONFIG_CHARGER_S2MU106)

		ret = power_supply_get_property(fuelgauge->psy_chg, POWER_SUPPLY_PROP_CHARGING_ENABLED, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);

		charging_enabled = value.intval;

		value.intval = SEC_BAT_CHG_MODE_CHARGING_OFF;

		ret = power_supply_set_property(fuelgauge->psy_chg, POWER_SUPPLY_PROP_CHARGING_ENABLED, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);
#endif

		if (fuelgauge->reg_OTP_52 != reg_OTP_52 || fuelgauge->reg_OTP_53 != reg_OTP_53) {
#if defined(CONFIG_CHARGER_S2MU106)
			ret = power_supply_set_property(fuelgauge->psy_chg, POWER_SUPPLY_PROP_FUELGAUGE_RESET, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);
#endif

			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, 0x40);
			msleep(50);
			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, 0x01);

			s2mu106_read_reg_byte(fuelgauge->i2c, 0x53, &reg_OTP_53);
			s2mu106_read_reg_byte(fuelgauge->i2c, 0x52, &reg_OTP_52);

			dev_err(&fuelgauge->i2c->dev, "1st reset after %s: OTP 52(%02x) 53(%02x) "
					"current 52(%02x) 53(%02x)\n", __func__,
					fuelgauge->reg_OTP_52, fuelgauge->reg_OTP_53, reg_OTP_52, reg_OTP_53);

			if (fuelgauge->reg_OTP_52 != reg_OTP_52 || fuelgauge->reg_OTP_53 != reg_OTP_53) {
#if defined(CONFIG_CHARGER_S2MU106)
				ret = power_supply_set_property(fuelgauge->psy_chg, POWER_SUPPLY_PROP_FUELGAUGE_RESET, &value);
				if (ret < 0)
					pr_err("%s: Fail to execute property\n", __func__);
#endif
				s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, 0x40);
				msleep(50);
				s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, 0x01);
				dev_err(&fuelgauge->i2c->dev, "%s : 2nd reset\n", __func__);
			}
		}

		dev_info(&fuelgauge->i2c->dev, "%s: FG reset\n", __func__);
		s2mu106_fix_rawsoc_reset_fg(fuelgauge);
		por_state = 0x00;
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, por_state);

#if defined(CONFIG_FUELGAUGE_S2MU106_USE_5MILLIOHM)
		s2mu106_set_trim_5mohm(fuelgauge);
#endif

#if defined(CONFIG_CHARGER_S2MU106)
		/* Recover charger status after f.g reset */
		if (charging_enabled) {
			value.intval = SEC_BAT_CHG_MODE_CHARGING;
			ret = power_supply_set_property(fuelgauge->psy_chg, POWER_SUPPLY_PROP_CHARGING_ENABLED, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);
		}
#endif
	}

	mutex_lock(&fuelgauge->fg_lock);

	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_RSOC, data) < 0)
		goto err;

	mutex_unlock(&fuelgauge->fg_lock);

	compliment = (data[1] << 8) | (data[0]);

	/* data[] store 2's compliment format number */
	if (compliment & (0x1 << 15)) {
		/* Negative */
		fuelgauge->rsoc = ((~compliment) & 0xFFFF) + 1;
		fuelgauge->rsoc = (fuelgauge->rsoc * (-10000)) / (0x1 << 14);
	} else {
		fuelgauge->rsoc = compliment & 0x7FFF;
		fuelgauge->rsoc = ((fuelgauge->rsoc * 10000) / (0x1 << 14));
	}

	avg_current = s2mu106_get_avgcurrent(fuelgauge);
	avg_vbat = s2mu106_get_avgvbat(fuelgauge);
	vbat = s2mu106_get_vbat(fuelgauge);
	curr = s2mu106_get_current(fuelgauge);

	if (!fuelgauge->psy_bat)
		fuelgauge->psy_bat = power_supply_get_by_name("battery");
	if (!fuelgauge->psy_bat) {
		dev_err(&fuelgauge->i2c->dev, "%s:  battery psy failed\n", __func__);
		return -EINVAL;
	}

	if (!fuelgauge->init_battery_temp) {
		ret = s2mu106_read_reg_byte(fuelgauge->i2c, 0x46, &temp);
		if ((temp != 0) && (ret >= 0)) {
			fuelgauge->temperature = (temp & (0x1 << 7)) ? (-1 * ((~temp & 0xFF) + 1)) : (temp & 0x7F);
			fuelgauge->temperature *= 10;
			/* recover default value */
			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x46, 0x0);
		} else {
			fuelgauge->temperature = 300;
		}
 	} else {
		/* Get temperature from battery driver */
		ret = power_supply_get_property(fuelgauge->psy_bat, POWER_SUPPLY_PROP_TEMP, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);
		fuelgauge->temperature = value.intval;
	}

	/* Get UI SOC from battery driver */
	ret = power_supply_get_property(fuelgauge->psy_bat, POWER_SUPPLY_PROP_CAPACITY, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	fuelgauge->ui_soc = value.intval;

	ret = power_supply_get_property(fuelgauge->psy_bat, POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	else
		is_swelling_status = value.intval;

#if (BATCAP_LEARN) || (TEMP_COMPEN)
	fuelgauge->bat_charging = s2mu106_get_bat_charging(fuelgauge);
#endif

#if (TEMP_COMPEN)
	fuelgauge->vm_status = s2mu106_get_vm_status(fuelgauge);
	fuelgauge->avg_curr = avg_current;
	s2mu106_temperature_compensation(fuelgauge);

	dev_info(&fuelgauge->i2c->dev, "%s: current_soc (%d), compen_soc (%d), "
			"previous_soc (%d), FG_mode(%s)\n",
			__func__, fuelgauge->rsoc, fuelgauge->soc_r,
			fuelgauge->info.soc, mode_to_str[fuelgauge->mode]);

	fuelgauge->info.soc = fuelgauge->soc_r;
#else
	dev_info(&fuelgauge->i2c->dev, "%s: current_soc (%d), previous_soc (%d), FG_mode(%s)\n",
		 __func__, fuelgauge->rsoc, fuelgauge->info.soc, mode_to_str[fuelgauge->mode]);

	fuelgauge->info.soc = fuelgauge->rsoc;
#endif

#if defined(CONFIG_CHARGER_S2MU106)
	ret = power_supply_get_property(fuelgauge->psy_chg, POWER_SUPPLY_PROP_VOLTAGE_MAX, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	float_voltage = value.intval;
#else
	float_voltage = 4350;
#endif

	float_voltage = (float_voltage * 996) / 1000;

	s2mu106_read_reg_byte(fuelgauge->i2c, 0x4A, &fg_mode_reg);

	dev_info(&fuelgauge->i2c->dev, "%s: UI SOC = %d, is_charging = %d, avg_vbat = %d, "
		"float_voltage = %d, avg_current = %d, 0x4A = 0x%02x\n", __func__,
		fuelgauge->ui_soc, fuelgauge->is_charging, avg_vbat,
		float_voltage, avg_current, fg_mode_reg);

	if (is_swelling_status) {
		if (fuelgauge->mode == HIGH_SOC_VOLTAGE_MODE) {
			fuelgauge->mode = CURRENT_MODE;

			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0x10);
			s2mu106_read_reg_byte(fuelgauge->i2c, 0x4B, &temp);
			temp &= 0x8F;
			s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4B, temp);

			dev_info(&fuelgauge->i2c->dev, "%s: FG is in current mode\n", __func__);
		}
	} else {
		if (((fuelgauge->is_charging == true) && (fuelgauge->ui_soc >= 98) && (avg_current > 50)) ||
				((fuelgauge->is_charging == true) && (avg_vbat > float_voltage) &&
				 (avg_current < s2mu106_fg_check_current_level(fuelgauge) && (avg_current > 50)))) {
			if (fuelgauge->mode == CURRENT_MODE) { /* switch to VOLTAGE_MODE */
				fuelgauge->mode = HIGH_SOC_VOLTAGE_MODE;

				s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0xFF);
				s2mu106_read_reg_byte(fuelgauge->i2c, 0x4B, &temp);
				temp |= 0x70;
				s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4B, temp);

				dev_info(&fuelgauge->i2c->dev, "%s: FG is in high soc voltage mode\n", __func__);
			}
		} else if (avg_current < -50 || avg_current >= s2mu106_fg_check_current_level(fuelgauge)) {
			if (fuelgauge->mode == HIGH_SOC_VOLTAGE_MODE) {
				fuelgauge->mode = CURRENT_MODE;

				s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0x10);
				s2mu106_read_reg_byte(fuelgauge->i2c, 0x4B, &temp);
				temp &= 0x8F;
				s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4B, temp);

				dev_info(&fuelgauge->i2c->dev, "%s: FG is in current mode\n", __func__);
			}
		}
	}

	s2mu106_read_reg_byte(fuelgauge->i2c, 0x67, &reg_0x67);

	if ((avg_vbat > 3400) && (fuelgauge->is_charging == true) &&
			(fuelgauge->rsoc < 400) && ((reg_0x67 & 0x02) == 0x02)) {
		reg_0x67 &= 0xFD;
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x67, reg_0x67);
		pr_info("%s: 0x67[1] = 0", __func__);
	} else if ((fuelgauge->rsoc > 450) && ((reg_0x67 & 0x02) == 0x00)) {
		reg_0x67 |= 0x02;
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x67, reg_0x67);
		pr_info("%s: 0x67[1] = 1", __func__);
	}

#if (BATCAP_LEARN)
	fuelgauge->capcc = s2mu106_get_cap_cc(fuelgauge);
	fuelgauge->batcap_ocv = s2mu106_get_batcap_ocv(fuelgauge); // CC mode capacity
	fuelgauge->cycle = s2mu106_get_cycle(fuelgauge);
	BATCAP_L_VBAT = (BAT_L_CON[1] == 0) ? 4200:4100;

	if (fuelgauge->temperature >= 200) {
		if (fuelgauge->learn_start == false) {
			if ((fuelgauge->rsoc < 1000) && (fuelgauge->cycle >= BAT_L_CON[0]))
				fuelgauge->learn_start = true;
		} else {
			if ((fuelgauge->cond1_ok == false) && (fuelgauge->bat_charging == false))
				goto batcap_learn_init;

			if (fuelgauge->cond1_ok == false) {
				if (fuelgauge->c1_count >= BAT_L_CON[2]) {
					fuelgauge->cond1_ok = true;
					fuelgauge->c1_count = 0;
				} else {
					if ((vbat >= BATCAP_L_VBAT) && (avg_current < BAT_L_CON[4]) &&
							(fuelgauge->rsoc >= 9700)) {
						fuelgauge->c1_count++;
					} else {
						fuelgauge->c1_count = 0;
					}
				}
			} else {
				if (fuelgauge->c2_count >= BAT_L_CON[3]) {
					s2mu106_batcap_learning(fuelgauge);
					goto batcap_learn_init;
				} else {
					if ((vbat >= (BATCAP_L_VBAT - 100)) && (avg_current > -30) &&
							(avg_current < 30) && (fuelgauge->rsoc >= 9800)) {
						fuelgauge->c2_count++;
					} else {
						fuelgauge->c2_count = 0;

						if (avg_current <= -30)
							goto batcap_learn_init;
					}
				}
			}
		}
	} else {
batcap_learn_init:
		fuelgauge->learn_start = false;
		fuelgauge->cond1_ok  = false;
		fuelgauge->c1_count = 0;
		fuelgauge->c2_count = 0;
	}
#endif

#if (TEMP_COMPEN) && (BATCAP_LEARN)
	fuelgauge->soh = s2mu106_get_soh(fuelgauge);
	fuelgauge->capcc = s2mu106_get_cap_cc(fuelgauge);
	fuelgauge->fcc = s2mu106_get_fullcharge_cap(fuelgauge);
	fuelgauge->rmc = s2mu106_get_remaining_cap(fuelgauge);

	pr_info("%s: SOC_M = %d, Chg_stat = %d, VM = %d, avbVBAT = %d, avgCURR = %d, avgTEMP = %d, "
			"SOCni = %d, SOC0i = %d, SOCr = %d, SOC_R = %d, "
			"Learning_start = %d, C1_count = %d/%d, C2_count = %d/%d, "
			"BATCAP_OCV_new = %d, SOH = %d, CAP_CC = %d, FCC = %d, RM = %d\n",
			__func__,
			fuelgauge->rsoc, fuelgauge->bat_charging, fuelgauge->vm_status, avg_vbat, avg_current, fuelgauge->temperature,
			fuelgauge->socni, fuelgauge->soc0i, fuelgauge->comp_socr, fuelgauge->soc_r,
			fuelgauge->learn_start, fuelgauge->c1_count, BAT_L_CON[2], fuelgauge->c2_count, BAT_L_CON[3],
			fuelgauge->batcap_ocv_fin, fuelgauge->soh, fuelgauge->capcc, fuelgauge->fcc, fuelgauge->rmc);
#endif

	/* Low voltage W/A, make 0% */
	if (fuelgauge->temperature > fuelgauge->low_temp_limit) {
		if ((avg_vbat < fuelgauge->low_voltage_limit) &&
			(avg_current < -50) && (fuelgauge->rsoc > 300)) {
			low_voltage_limit_cnt++;

			if (low_voltage_limit_cnt >= fuelgauge->low_voltage_limit_cnt) {
				dev_info(&fuelgauge->i2c->dev, "%s: Low voltage WA in normal temperature.\n", __func__);

				s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x29, 0x07);

				/* Dumpdone. Re-calculate SOC */
				s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1E, 0x0F);
				msleep(300);

				s2mu106_read_reg_byte(fuelgauge->i2c, 0x29, &temp);
				temp &= 0xFE;
				s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x29, temp);

				low_voltage_limit_cnt = 0;
#if (TEMP_COMPEN)
				fuelgauge->flag_mapping = false;
#endif
			}
		} else { 
			low_voltage_limit_cnt = 0;
		}
	} else {
		if ((avg_vbat < fuelgauge->low_voltage_limit_lowtemp) &&
				(avg_current < -50) && (fuelgauge->info.soc > 100)) {
			dev_info(&fuelgauge->i2c->dev, "%s: Low voltage WA in Low temperature. Make UI SOC 0\n", __func__);

			/* Make report SOC 0% */
			fuelgauge->info.soc = 0;
#if (TEMP_COMPEN)
			fuelgauge->soc_r = 0;
#endif
			touch_low_voltage = true;
		}
	}

	/* Low voltage W/A, Maintain UI SOC if battery is relaxing */
	if (touch_low_voltage &&
				(fuelgauge->soc_r == 0) && (fuelgauge->ui_soc > 5)
				&& (avg_vbat > fuelgauge->low_voltage_recover_lowtemp)) {
		fuelgauge->soc_r = fuelgauge->ui_soc * 100;
		fuelgauge->info.soc = fuelgauge->soc_r;
		fuelgauge->init_start = 1;
		touch_low_voltage = false;

		dev_info(&fuelgauge->i2c->dev,
				"%s:  Maintain UI SOC by Volt if battery is relaxing SOC_R = %d, info.soc = %d\n",
				__func__, fuelgauge->soc_r, fuelgauge->info.soc);
	}
#if (TEMP_COMPEN)
	/* Low temperature W/A, Maintain UI SOC if battery is relaxing */
	if (((fuelgauge->temperature < fuelgauge->low_temp_limit) &&
				(fuelgauge->soc_r == 0) && (fuelgauge->ui_soc > 10)) &&
			(((avg_current > -60) && (avg_current < 50)) || ((curr > -100) && (curr < 50)))) {
		fuelgauge->soc_r = fuelgauge->ui_soc * 100;
		fuelgauge->info.soc = fuelgauge->soc_r;
		fuelgauge->init_start = 1;

		dev_info(&fuelgauge->i2c->dev,
				"%s:  Maintain UI SOC if battery is relaxing SOC_R = %d, info.soc = %d\n",
				__func__, fuelgauge->soc_r, fuelgauge->info.soc);
	}
#endif

	/* S2MU106 FG debug */
	s2mu106_fg_test_read(fuelgauge->i2c);

	return min(fuelgauge->info.soc, 10000);

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

static int s2mu106_get_current(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int curr = 0;

	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_RCUR_CC, data) < 0)
		return -EINVAL;
	compliment = (data[1] << 8) | (data[0]);
	dev_dbg(&fuelgauge->i2c->dev, "%s: rCUR_CC(0x%4x)\n", __func__, compliment);

	if (compliment & (0x1 << 15)) { /* Charging */
		curr = ((~compliment) & 0xFFFF) + 1;
		curr = (curr * 1000) >> 12;
	} else { /* dischaging */
		curr = compliment & 0x7FFF;
		curr = (curr * (-1000)) >> 12;
	}

	dev_info(&fuelgauge->i2c->dev, "%s: current (%d)mA\n", __func__, curr);

	return curr;
}

static int s2mu106_get_ocv(struct s2mu106_fuelgauge_data *fuelgauge)
{
	/* 22 values of mapping table for EVT1*/
	int *soc_arr;
	int *ocv_arr;

	int soc = fuelgauge->info.soc;
	int ocv = 0;

	int high_index = TABLE_SIZE - 1;
	int low_index = 0;
	int mid_index = 0;

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	soc_arr = fuelgauge->age_data_info[fuelgauge->fg_age_step].soc_arr_val;
	ocv_arr = fuelgauge->age_data_info[fuelgauge->fg_age_step].ocv_arr_val;
#else
	soc_arr = fuelgauge->info.soc_arr_val;
	ocv_arr = fuelgauge->info.ocv_arr_val;
#endif

	dev_err(&fuelgauge->i2c->dev,
		"%s: soc (%d) soc_arr[TABLE_SIZE-1] (%d) ocv_arr[TABLE_SIZE-1) (%d)\n",
		__func__, soc, soc_arr[TABLE_SIZE-1], ocv_arr[TABLE_SIZE-1]);
	if (soc <= soc_arr[high_index]) {
		ocv = ocv_arr[high_index];
		goto ocv_soc_mapping;
	} else if (soc >= soc_arr[low_index]) {
		ocv = ocv_arr[low_index];
		goto ocv_soc_mapping;
	}
	while (low_index <= high_index) {
		mid_index = (low_index + high_index) >> 1;
		if (soc_arr[mid_index] > soc)
			low_index = mid_index + 1;
		else if (soc_arr[mid_index] < soc)
			high_index = mid_index - 1;
		else {
			ocv = ocv_arr[mid_index];
			goto ocv_soc_mapping;
		}
	}
	high_index = (high_index < 0) ? 0 : high_index;
	low_index = (low_index > TABLE_SIZE - 1) ? TABLE_SIZE - 1 : low_index;
	ocv = ocv_arr[high_index];
	ocv += ((ocv_arr[low_index] - ocv_arr[high_index]) *
					(soc - soc_arr[high_index])) /
					(soc_arr[low_index] - soc_arr[high_index]);

ocv_soc_mapping:
	s2mu106_fg_periodic_read_power(fuelgauge);

	dev_info(&fuelgauge->i2c->dev, "%s: soc (%d), ocv (%d)\n", __func__, soc, ocv);
	return ocv;
}

static int s2mu106_get_avgcurrent(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int curr = 0;

	mutex_lock(&fuelgauge->fg_lock);

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_MONOUT_SEL, 0x17);

	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_MONOUT, data) < 0)
		goto err;
	compliment = (data[1] << 8) | (data[0]);
	dev_dbg(&fuelgauge->i2c->dev, "%s: MONOUT(0x%4x)\n", __func__, compliment);

	if (compliment & (0x1 << 15)) { /* Charging */
		curr = ((~compliment) & 0xFFFF) + 1;
		curr = (curr * 1000) >> 12;
	} else { /* dischaging */
		curr = compliment & 0x7FFF;
		curr = (curr * (-1000)) >> 12;
	}
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_MONOUT_SEL, 0x10);

	mutex_unlock(&fuelgauge->fg_lock);

	dev_info(&fuelgauge->i2c->dev, "%s: avg current (%d)mA\n", __func__, curr);

	return curr;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

static int s2mu106_maintain_avgcurrent(
	struct s2mu106_fuelgauge_data *fuelgauge)
{
	static int cnt;
	int vcell = 0;
	int curr = 0;

	curr = s2mu106_get_avgcurrent(fuelgauge);

	vcell = s2mu106_get_vbat(fuelgauge);
	if ((cnt < 10) && (curr < 0) && (fuelgauge->is_charging) &&
		(vcell < 3500)) {
		curr = 1;
		cnt++;
		dev_info(&fuelgauge->i2c->dev, "%s: vcell (%d)mV, modified avg current (%d)mA\n",
				 __func__, vcell, curr);
	}

	return curr;
}

static int s2mu106_get_vbat(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vbat = 0;

	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_RVBAT, data) < 0)
		return -EINVAL;

	dev_dbg(&fuelgauge->i2c->dev, "%s: data0 (%d) data1 (%d)\n", __func__, data[0], data[1]);
	vbat = ((data[0] + (data[1] << 8)) * 1000) >> 13;

	dev_info(&fuelgauge->i2c->dev, "%s: vbat (%d)\n", __func__, vbat);

	return vbat;
}

static int s2mu106_get_avgvbat(struct s2mu106_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment, avg_vbat;

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x40, 0x08);
	mutex_lock(&fuelgauge->fg_lock);

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_MONOUT_SEL, 0x16);

	msleep(50);

	if (s2mu106_read_reg(fuelgauge->i2c, S2MU106_REG_MONOUT, data) < 0)
		goto err;
	compliment = (data[1] << 8) | (data[0]);

	avg_vbat = (compliment * 1000) >> 12;

	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, S2MU106_REG_MONOUT_SEL, 0x10);

	mutex_unlock(&fuelgauge->fg_lock);

	dev_info(&fuelgauge->i2c->dev, "%s: avgvbat (%d)\n", __func__, avg_vbat);

	return avg_vbat;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

bool s2mu106_fuelgauge_fuelalert_init(struct i2c_client *client, int soc)
{
	struct s2mu106_fuelgauge_data *fuelgauge = i2c_get_clientdata(client);
	u8 data[2];

	fuelgauge->is_fuel_alerted = false;

	/* 1. Set s2mu106 alert configuration. */
	s2mu106_alert_init(fuelgauge);

	if (s2mu106_read_reg(client, S2MU106_REG_IRQ, data) < 0)
		return -1;

	/*Enable VBAT, SOC */
	data[1] &= 0xfc;

	/*Disable IDLE_ST, INIT)ST */
	data[1] |= 0x0c;

	s2mu106_write_reg(client, S2MU106_REG_IRQ, data);

	dev_dbg(&client->dev, "%s: irq_reg(%02x%02x) irq(%d)\n",
			__func__, data[1], data[0], fuelgauge->pdata->fg_irq);

	return true;
}
#if defined(CONFIG_BATTERY_AGE_FORECAST)
static int s2mu106_fg_aging_check(
		struct s2mu106_fuelgauge_data *fuelgauge, int step)
{
	u8 batcap0 = 0, batcap1 = 0, batcap2 = 0, batcap3 = 0;
	u8 por_state = 0;
	union power_supply_propval value;
	int charging_enabled = false;

	fuelgauge->fg_age_step = step;

	s2mu106_read_reg_byte(fuelgauge->i2c, 0x0E, &batcap0);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x0F, &batcap1);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x10, &batcap2);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x11, &batcap3);

	pr_info("%s: [Long life] orig. batcap : %02x, %02x, %02x, %02x , fg_age_step data : %02x, %02x, %02x, %02x \n",
		__func__, batcap0, batcap1, batcap2, batcap3,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[0],
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[1],
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[2],
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[3]);

	if ((batcap0 != fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[0]) ||
		(batcap1 != fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[1]) ||
		(batcap2 != fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[2]) ||
		(batcap3 != fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[3])) {

		pr_info("%s: [Long life] reset gauge for age forecast , step[%d] \n", __func__, fuelgauge->fg_age_step);

		fuelgauge->age_reset_status = 1;
		por_state |= 0x10;
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, por_state);

		/* check charging enable */
		psy_do_property("s2mu106-charger", get, POWER_SUPPLY_PROP_CHARGING_ENABLED, value);
		charging_enabled = value.intval;

		if (charging_enabled == true) {
			pr_info("%s: [Long life] disable charger for reset gauge age forecast \n",
				__func__);
			value.intval = SEC_BAT_CHG_MODE_CHARGING_OFF;
			psy_do_property("s2mu106-charger", set, POWER_SUPPLY_PROP_CHARGING_ENABLED, value);
		}

		s2mu106_reset_fg(fuelgauge);
#if (TEMP_COMPEN)
		fuelgauge->init_start = 1;
#endif

		if (charging_enabled == true) {
			psy_do_property("battery", get, POWER_SUPPLY_PROP_STATUS, value);
			charging_enabled = value.intval;

			if (charging_enabled == 1) { /* POWER_SUPPLY_STATUS_CHARGING 1 */
				pr_info("%s: [Long life] enable charger for reset gauge age forecast \n",
					__func__);
				value.intval = SEC_BAT_CHG_MODE_CHARGING;
				psy_do_property("s2mu106-charger",
					set, POWER_SUPPLY_PROP_CHARGING_ENABLED, value);
			}
		}

		por_state &= ~0x10;
		s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x1F, por_state);
		fuelgauge->age_reset_status = 0;

		return 1;
	}
	return 0;
}
#endif

/* capacity is 0.1% unit */
static void s2mu106_fg_get_scaled_capacity(
		struct s2mu106_fuelgauge_data *fuelgauge,
		union power_supply_propval *val)
{
	int rawsoc = val->intval;

	val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
		0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
		(fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));

	dev_info(&fuelgauge->i2c->dev,
			"%s: capacity_max(%d) scaled capacity(%d.%d), raw_soc(%d.%d)\n",
			__func__, fuelgauge->capacity_max,
			val->intval/10, val->intval%10, rawsoc/10, rawsoc%10);
}

/* capacity is integer */
static void s2mu106_fg_get_atomic_capacity(
        struct s2mu106_fuelgauge_data *fuelgauge,
        union power_supply_propval *val)
{
    if (fuelgauge->pdata->capacity_calculation_type &
            SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC) {
        if (fuelgauge->capacity_old < val->intval)
            val->intval = fuelgauge->capacity_old + 1;
        else if (fuelgauge->capacity_old > val->intval)
            val->intval = fuelgauge->capacity_old - 1;
    }

    /* keep SOC stable in abnormal status */
    if (fuelgauge->pdata->capacity_calculation_type &
            SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL) {
        if (!fuelgauge->is_charging &&
                fuelgauge->capacity_old < val->intval) {
            dev_err(&fuelgauge->i2c->dev,
                    "%s: capacity (old %d : new %d)\n",
                    __func__, fuelgauge->capacity_old, val->intval);
            val->intval = fuelgauge->capacity_old;
        }
    }

    /* updated old capacity */
    fuelgauge->capacity_old = val->intval;
}

static int s2mu106_fg_check_capacity_max(
		struct s2mu106_fuelgauge_data *fuelgauge, int capacity_max)
{
	int new_capacity_max = capacity_max;

	if (new_capacity_max < (fuelgauge->pdata->capacity_max -
				fuelgauge->pdata->capacity_max_margin - 10)) {
		new_capacity_max =
			(fuelgauge->pdata->capacity_max -
			 fuelgauge->pdata->capacity_max_margin);

		dev_info(&fuelgauge->i2c->dev, "%s: set capacity max(%d --> %d)\n",
				__func__, capacity_max, new_capacity_max);
	} else if (new_capacity_max > (fuelgauge->pdata->capacity_max +
				fuelgauge->pdata->capacity_max_margin)) {
		new_capacity_max =
			(fuelgauge->pdata->capacity_max +
			 fuelgauge->pdata->capacity_max_margin);

		dev_info(&fuelgauge->i2c->dev, "%s: set capacity max(%d --> %d)\n",
				__func__, capacity_max, new_capacity_max);
	}

	return new_capacity_max;
}

static int s2mu106_fg_calculate_dynamic_scale(
		struct s2mu106_fuelgauge_data *fuelgauge, int capacity)
{
	union power_supply_propval raw_soc_val;
	raw_soc_val.intval = s2mu106_get_rawsoc(fuelgauge) / 10;

	if (raw_soc_val.intval <
		fuelgauge->pdata->capacity_max -
		fuelgauge->pdata->capacity_max_margin) {
		pr_info("%s: raw soc(%d) is very low, skip routine\n",
			__func__, raw_soc_val.intval);
	} else {
		fuelgauge->capacity_max =
			(raw_soc_val.intval * 100 / (capacity + 1));
		fuelgauge->capacity_old = capacity;

		fuelgauge->capacity_max =
			s2mu106_fg_check_capacity_max(fuelgauge,
			fuelgauge->capacity_max);

		pr_info("%s: %d is used for capacity_max, capacity(%d)\n",
			__func__, fuelgauge->capacity_max, capacity);
	}

	return fuelgauge->capacity_max;
}

void s2mu106_fg_set_sys_voltage(struct s2mu106_fuelgauge_data *fuelgauge,
			int get_sys_vol)
{
	u8 temp = 0;

	s2mu106_read_reg_byte(fuelgauge->i2c, 0x25, &temp);
	temp &= 0xCF;
	if (get_sys_vol) /* factory mode : 0x25 [5:4] = b11 (Get SYS voltage) */
		temp |= 0x30;
	else /* normal mode : 0x25 [5:4] = b01 (Get Battery voltage by I2C Control) */
		temp |= 0x10;
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x25, temp);

	pr_info("%s: SEC_BAT_INBAT_FGSRC_SWITCHING_OFF : 0x25 = %x\n",
			__func__, temp);
}

static int s2mu106_fg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2mu106_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		return -ENODATA;
	case POWER_SUPPLY_PROP_ENERGY_NOW:
		switch (val->intval) {
		case SEC_BATTERY_CAPACITY_DESIGNED:
			val->intval = fuelgauge->pdata->capacity_full;
			break;
		case SEC_BATTERY_CAPACITY_ABSOLUTE:
			val->intval = 0;
			break;
		case SEC_BATTERY_CAPACITY_TEMPERARY:
			val->intval = 0;
			break;
		case SEC_BATTERY_CAPACITY_CURRENT:
			val->intval = 0;
			break;
		case SEC_BATTERY_CAPACITY_AGEDCELL:
			val->intval = 0;
			break;
		case SEC_BATTERY_CAPACITY_CYCLE:
			val->intval = 0;
			break;
		case SEC_BATTERY_CAPACITY_FULL:
			val->intval = fuelgauge->pdata->capacity_full;
			break;
		}
		break;
		/* Cell voltage (VCELL, mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = s2mu106_get_vbat(fuelgauge);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTERY_VOLTAGE_AVERAGE:
			val->intval = s2mu106_get_avgvbat(fuelgauge);
			break;
		case SEC_BATTERY_VOLTAGE_OCV:
			val->intval = s2mu106_get_ocv(fuelgauge);
			break;
		}
		break;
		/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = s2mu106_get_current(fuelgauge) * 1000;
		else
			val->intval = s2mu106_get_current(fuelgauge);
		break;
		/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = s2mu106_maintain_avgcurrent(fuelgauge) * 1000;
		else
			fuelgauge->current_avg = val->intval = s2mu106_maintain_avgcurrent(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
			val->intval = s2mu106_get_rawsoc(fuelgauge);
		} else if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_CAPACITY_POINT) {
			val->intval = fuelgauge->raw_capacity % 10;
		} else {
			val->intval = s2mu106_get_rawsoc(fuelgauge) / 10;

			if (fuelgauge->pdata->capacity_calculation_type &
				(SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
					SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)) {
				s2mu106_fg_get_scaled_capacity(fuelgauge, val);

				if (val->intval > 1010) {
					pr_info("%s : scaled capacity (%d)\n", __func__, val->intval);
					s2mu106_fg_calculate_dynamic_scale(fuelgauge, 100);
				}
			}

			/* capacity should be between 0% and 100%
			 * (0.1% degree)
			 */
			if (val->intval > 1000)
				val->intval = 1000;
			if (val->intval < 0)
				val->intval = 0;
			fuelgauge->raw_capacity = val->intval;

			/* get only integer part */
			val->intval /= 10;

			/* check whether doing the wake_unlock */
			if ((val->intval > fuelgauge->pdata->fuel_alert_soc) &&
					fuelgauge->is_fuel_alerted) {
				wake_unlock(&fuelgauge->fuel_alert_wake_lock);
				s2mu106_fuelgauge_fuelalert_init(fuelgauge->i2c,
						fuelgauge->pdata->fuel_alert_soc);
			}

			/* (Only for atomic capacity)
			 * In initial time, capacity_old is 0.
			 * and in resume from sleep,
			 * capacity_old is too different from actual soc.
			 * should update capacity_old
			 * by val->intval in booting or resume.
			 */
			if (fuelgauge->initial_update_of_soc &&
				(fuelgauge->temperature > fuelgauge->low_temp_limit)) {
				/* updated old capacity */
				fuelgauge->capacity_old = val->intval;
				fuelgauge->initial_update_of_soc = false;
				break;
			}

			if (fuelgauge->sleep_initial_update_of_soc) {
				/* updated old capacity in case of resume */
				if (fuelgauge->is_charging) {
					fuelgauge->capacity_old = val->intval;
					fuelgauge->sleep_initial_update_of_soc = false;
					break;
				} else if ((!fuelgauge->is_charging) &&
						(fuelgauge->capacity_old >= val->intval)) {
					fuelgauge->capacity_old = val->intval;
					fuelgauge->sleep_initial_update_of_soc = false;
					break;
				}
			}

			if (fuelgauge->pdata->capacity_calculation_type &
					(SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC |
					 SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL))
				s2mu106_fg_get_atomic_capacity(fuelgauge, val);
		}
		break;
	/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
	/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = s2mu106_get_temperature(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL:
#if (BATCAP_LEARN)
		fuelgauge->soh = s2mu106_get_soh(fuelgauge);
		val->intval = fuelgauge->soh;
#endif
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
		val->intval = fuelgauge->capacity_max;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		val->intval = calc_ttf(fuelgauge, val);
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		val->intval = fuelgauge->mode;
		break;
	case POWER_SUPPLY_PROP_SOH:
#if (BATCAP_LEARN)
		fuelgauge->soh = s2mu106_get_soh(fuelgauge);
		val->intval = fuelgauge->soh;
#else
		/* If battery capacity learning is not enabled,
		 * return SOH is 100%
		 */
		val->intval = 100;
#endif
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = fuelgauge->pdata->capacity_full * fuelgauge->raw_capacity;
		break;
	case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_TTF_FULL_CAPACITY:
			val->intval = calc_ttf_to_full_capacity(fuelgauge, val);
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

static int s2mu106_fg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mu106_fuelgauge_data *fuelgauge =
				power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
#if defined(CONFIG_BATTERY_AGE_FORECAST)
			if (val->intval == POWER_SUPPLY_STATUS_FULL)
				s2mu106_fg_aging_check(fuelgauge, fuelgauge->change_step);
#endif
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL:
			if (fuelgauge->pdata->capacity_calculation_type &
					SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
				s2mu106_fg_calculate_dynamic_scale(fuelgauge, val->intval);
			}
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			fuelgauge->cable_type = val->intval;
			break;
		case POWER_SUPPLY_PROP_CHARGING_ENABLED:
			if (val->intval == SEC_BAT_CHG_MODE_CHARGING)
				fuelgauge->is_charging = true;
			else
				fuelgauge->is_charging = false;
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET) {
				s2mu106_restart_gauging(fuelgauge);
				fuelgauge->initial_update_of_soc = true;
			}
			break;
		case POWER_SUPPLY_PROP_TEMP:
		case POWER_SUPPLY_PROP_TEMP_AMBIENT:
			s2mu106_set_temperature(fuelgauge, val->intval);
			fuelgauge->init_battery_temp = true;
			break;
		case POWER_SUPPLY_PROP_ENERGY_NOW:
			s2mu106_fg_reset_capacity_by_jig_connection(fuelgauge);
			break;
		case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
			dev_info(&fuelgauge->i2c->dev,
				"%s: capacity_max changed, %d -> %d\n",
				__func__, fuelgauge->capacity_max, val->intval);
			fuelgauge->capacity_max = s2mu106_fg_check_capacity_max(fuelgauge, val->intval);
			fuelgauge->initial_update_of_soc = true;
			break;
		case POWER_SUPPLY_PROP_CHARGE_EMPTY:
			break;
		case POWER_SUPPLY_PROP_ENERGY_AVG:
			break;
		case POWER_SUPPLY_PROP_CURRENT_FULL:
			fuelgauge->topoff_current = val->intval;
			break;
		case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
			switch (ext_psp) {
			case POWER_SUPPLY_EXT_PROP_INBAT_VOLTAGE_FGSRC_SWITCHING:
				if ((val->intval == SEC_BAT_INBAT_FGSRC_SWITCHING_ON) ||
					(val->intval == SEC_BAT_FGSRC_SWITCHING_ON)) {
					s2mu106_fg_set_sys_voltage(fuelgauge, 0);
					msleep(1000);
					if (val->intval == SEC_BAT_INBAT_FGSRC_SWITCHING_ON)
						s2mu106_restart_gauging(fuelgauge);
					s2mu106_fg_reset_capacity_by_jig_connection(fuelgauge);
					s2mu106_fg_test_read(fuelgauge->i2c);
				} else if ((val->intval == SEC_BAT_INBAT_FGSRC_SWITCHING_OFF) ||
					(val->intval == SEC_BAT_FGSRC_SWITCHING_OFF)) {
					s2mu106_fg_set_sys_voltage(fuelgauge, 1);
					msleep(1000);
					if (val->intval == SEC_BAT_INBAT_FGSRC_SWITCHING_OFF)
						s2mu106_restart_gauging(fuelgauge);
					s2mu106_fg_reset_capacity_by_jig_connection(fuelgauge);
					s2mu106_fg_test_read(fuelgauge->i2c);
				}
				break;
			case POWER_SUPPLY_EXT_PROP_FUELGAUGE_FACTORY:
					pr_info("%s:[DEBUG_FAC] fuelgauge\n", __func__);
					s2mu106_fg_set_sys_voltage(fuelgauge, 1);
					s2mu106_fg_reset_capacity_by_jig_connection(fuelgauge);
					break;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
			case POWER_SUPPLY_EXT_PROP_UPDATE_BATTERY_DATA:
				fuelgauge->change_step = val->intval;
				break;
#endif
			default:
				return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static void s2mu106_fg_isr_work(struct work_struct *work)
{
	struct s2mu106_fuelgauge_data *fuelgauge =
		container_of(work, struct s2mu106_fuelgauge_data, isr_work.work);
	u8 fg_alert_status = 0;

	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_STATUS, &fg_alert_status);
	dev_info(&fuelgauge->i2c->dev, "%s : fg_alert_status(0x%x)\n",
		__func__, fg_alert_status);

	fg_alert_status &= 0x03;
	if (fg_alert_status & 0x01)
		pr_info("%s : Battery Level(SOC) is very Low!\n", __func__);

	if (fg_alert_status & 0x02) {
		int voltage = s2mu106_get_vbat(fuelgauge);

		pr_info("%s : Battery Votage is very Low! (%dmV)\n",
				__func__, voltage);
	}

	if (!fg_alert_status) {
		fuelgauge->is_fuel_alerted = false;
		pr_info("%s : SOC or Voltage is Good!\n", __func__);
		wake_unlock(&fuelgauge->fuel_alert_wake_lock);
	}
}

static irqreturn_t s2mu106_fg_irq_thread(int irq, void *irq_data)
{
	struct s2mu106_fuelgauge_data *fuelgauge = irq_data;
	u8 fg_irq = 0;

	s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_IRQ, &fg_irq);
	dev_info(&fuelgauge->i2c->dev, "%s: fg_irq(0x%x)\n",
		__func__, fg_irq);

	if (fuelgauge->is_fuel_alerted) {
		return IRQ_HANDLED;
	} else {
		wake_lock(&fuelgauge->fuel_alert_wake_lock);
		fuelgauge->is_fuel_alerted = true;
		schedule_delayed_work(&fuelgauge->isr_work, 0);
	}

	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
static int s2mu106_fuelgauge_parse_dt(struct s2mu106_fuelgauge_data *fuelgauge)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mu106-fuelgauge");
	int ret;
	const u32 *p;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	int len, i;
#endif
	/* reset, irq gpio info */
	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		fuelgauge->pdata->fg_irq = of_get_named_gpio(np, "fuelgauge,fuel_int", 0);
		if (fuelgauge->pdata->fg_irq < 0)
			pr_err("%s error reading fg_irq = %d\n",
					__func__, fuelgauge->pdata->fg_irq);

		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_vol",
				&fuelgauge->pdata->fuel_alert_vol);
		if (ret < 0) {
			fuelgauge->pdata->fuel_alert_vol = 3300;
			pr_err("%s Default value of fuel_alert_vol : %d\n",
					__func__, fuelgauge->pdata->fuel_alert_vol);
		}

		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_soc",
				&fuelgauge->pdata->fuel_alert_soc);
		if (ret < 0)
			pr_err("%s error reading pdata->fuel_alert_soc %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max",
				&fuelgauge->pdata->capacity_max);
		if (ret < 0)
			pr_err("%s error reading capacity_max %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
				&fuelgauge->pdata->capacity_max_margin);
		if (ret < 0)
			pr_err("%s error reading capacity_max_margin %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_min",
				&fuelgauge->pdata->capacity_min);
		if (ret < 0)
			pr_err("%s error reading capacity_min %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
				&fuelgauge->pdata->capacity_calculation_type);
		if (ret < 0)
			pr_err("%s error reading capacity_calculation_type %d\n",
					__func__, ret);
		ret = of_property_read_u32(np, "fuelgauge,capacity_full",
				&fuelgauge->pdata->capacity_full);
		if (ret < 0)
			pr_err("%s error reading pdata->capacity_full %d\n",
					__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,low_temp_limit",
				&fuelgauge->low_temp_limit);
		if (ret < 0) {
			pr_err("%s There is no low temperature limit. Use default(100)\n",
					__func__);
			fuelgauge->low_temp_limit = 100;
		}

		ret = of_property_read_u32(np, "fuelgauge,low_voltage_limit",
				&fuelgauge->low_voltage_limit);
		if (ret < 0) {
			pr_err("%s There is no low voltage limit. Use default(3450)\n",
					__func__);
			fuelgauge->low_voltage_limit = 3450;
		}

		ret = of_property_read_u32(np, "fuelgauge,low_voltage_limit_lowtemp",
				&fuelgauge->low_voltage_limit_lowtemp);
		if (ret < 0) {
			pr_err("%s There is no low voltage limit low temp. Use default(3450)\n",
					__func__);
			fuelgauge->low_voltage_limit_lowtemp = 3450;
		}
		ret = of_property_read_u32(np, "fuelgauge,low_voltage_recover_lowtemp",
				&fuelgauge->low_voltage_recover_lowtemp);
		if (ret < 0) {
			pr_err("%s There is no low voltage recover low temp. Use default(3600)\n",
					__func__);
			fuelgauge->low_voltage_recover_lowtemp = 3600;
		}
		pr_err("%s fuelgauge,low_voltage_limit_lowtemp: %d, recover: %d\n",
				__func__, fuelgauge->low_voltage_limit_lowtemp,
				fuelgauge->low_voltage_recover_lowtemp);

		ret = of_property_read_u32(np, "fuelgauge,i_socr_coeff",
				&fuelgauge->i_socr_coeff);
		if (ret < 0) {
			pr_err("%s There is no i_socr_coeff . Use default(333)\n",
					__func__);
			fuelgauge->i_socr_coeff = 333;
		}

		ret = of_property_read_u32(np, "fuelgauge,t_socr_coeff",
				&fuelgauge->t_socr_coeff);
		if (ret < 0) {
			pr_err("%s There is no t_socr_coeff . Use default(15500)\n",
					__func__);
			fuelgauge->t_socr_coeff = 15500;
		}

		ret = of_property_read_u32(np, "fuelgauge,t_compen_coeff",
				&fuelgauge->t_compen_coeff);
		if (ret < 0) {
			pr_err("%s There is no t_compen_coeff . Use default(75)\n",
					__func__);
			fuelgauge->t_compen_coeff = 75;
		}

		ret = of_property_read_u32(np, "fuelgauge,low_t_compen_coeff",
				&fuelgauge->low_t_compen_coeff);
		if (ret < 0) {
			pr_err("%s There is no low_t_compen_coeff . Use default(223)\n",
					__func__);
			fuelgauge->low_t_compen_coeff = 223;
		}

		ret = of_property_read_u32(np, "fuelgauge,val_0x5C",
				&fuelgauge->val_0x5C);
		if (ret < 0) {
			pr_err("%s There is no val_5c . Use default(0x1A)\n",
					__func__);
			fuelgauge->val_0x5C = 0x1A;
		}

		ret = of_property_read_u32(np, "fuelgauge,low_voltage_limit_cnt",
				&fuelgauge->low_voltage_limit_cnt);
		if (ret < 0) {
			pr_err("%s There is no low_voltage_limit_cnt . Use default(1)\n",
					__func__);
			fuelgauge->low_voltage_limit_cnt = 1;
		}

		ret = of_property_read_u32(np, "fuelgauge,ttf_capacity",
					   &fuelgauge->ttf_capacity);
		if (ret < 0) {
			pr_err("%s: error reading capacity_calculation_type %d\n",
				__func__, ret);
			fuelgauge->ttf_capacity = fuelgauge->pdata->capacity_full;
		}

		p = of_get_property(np, "fuelgauge,cv_data", &len);
		if (p) {
			fuelgauge->cv_data = kzalloc(len, GFP_KERNEL);
			fuelgauge->cv_data_length = len / sizeof(struct cv_slope);
			pr_err("%s: len= %ld, length= %d, %d\n", __func__,
			       sizeof(int) * len, len, fuelgauge->cv_data_length);
			ret = of_property_read_u32_array(np, "fuelgauge,cv_data",
					(u32 *)fuelgauge->cv_data, len / sizeof(u32));
			if (ret) {
				pr_err("%s: failed to read fuelgauge->cv_data: %d\n",
					__func__, ret);
				kfree(fuelgauge->cv_data);
				fuelgauge->cv_data = NULL;
			}
		} else {
			pr_err("%s: there is not cv_data\n", __func__);
		}

		/* get topoff info */
		np = of_find_node_by_name(NULL, "cable-info");
		if (!np) {
			pr_err("%s np NULL\n", __func__);
		} else {
			ret = of_property_read_u32(np, "full_check_current_1st",
					&fuelgauge->topoff_current);
			if (ret < 0) {
				pr_err("%s fail to get topoff current %d\n", __func__, ret);
				fuelgauge->topoff_current = 500;
			}
		}

		np = of_find_node_by_name(NULL, "battery");
		if (!np) {
			pr_err("%s np NULL\n", __func__);
		} else {
			ret = of_property_read_string(np,
					"battery,fuelgauge_name",
					(char const **)&fuelgauge->pdata->fuelgauge_name);
		}

		/* get battery node */
		np = of_find_node_by_name(NULL, "battery_params");
		if (!np) {
			pr_err("%s battery_params node NULL\n", __func__);
		} else {
#if !defined(CONFIG_BATTERY_AGE_FORECAST)
			/* get battery_table */
			ret = of_property_read_u32_array(np, "battery,battery_table3", fuelgauge->info.battery_table3, 88);
			if (ret < 0)
				pr_err("%s error reading battery,battery_table3\n", __func__);

			ret = of_property_read_u32_array(np, "battery,battery_table4", fuelgauge->info.battery_table4, 22);
			if (ret < 0)
				pr_err("%s error reading battery,battery_table4\n", __func__);

			ret = of_property_read_u32_array(np, "battery,batcap", fuelgauge->info.batcap, 4);
			if (ret < 0)
				pr_err("%s error reading battery,batcap\n", __func__);

			ret = of_property_read_u32_array(np, "battery,accum", fuelgauge->info.accum, 2);
			if (ret < 0) {
				fuelgauge->info.accum[0]=0x00; // REG 0x44
				fuelgauge->info.accum[1]=0x08; // REG 0x45
				pr_err("%s There is no accumulative rate value in DT. set to the default value(0x800)\n", __func__);
			}

			ret = of_property_read_u32_array(np, "battery,soc_arr_val", fuelgauge->info.soc_arr_val, 22);
			if (ret < 0)
				pr_err("%s error reading battery,soc_arr_val\n", __func__);

			ret = of_property_read_u32_array(np, "battery,ocv_arr_val", fuelgauge->info.ocv_arr_val, 22);
			if (ret < 0)
				pr_err("%s error reading battery,ocv_arr_val\n", __func__);

#else
			of_get_property(np, "battery,battery_data", &len);
			fuelgauge->fg_num_age_step = len / sizeof(fg_age_data_info_t);
			fuelgauge->age_data_info = kzalloc(len, GFP_KERNEL);
			ret = of_property_read_u32_array(np, "battery,battery_data",
					(int *)fuelgauge->age_data_info, len/sizeof(int));

			pr_err("%s: [Long life] fuelgauge->fg_num_age_step %d \n",
				__func__,fuelgauge->fg_num_age_step);

			if ((sizeof(fg_age_data_info_t) * fuelgauge->fg_num_age_step) != len) {
				pr_err("%s: The Long life variables and the data in device tree does not match\n", __func__);
				BUG();
			}

			for(i=0 ; i < fuelgauge->fg_num_age_step ; i++){
				pr_err("%s: [Long life] age_step = %d, table3[0] %d, table4[0] %d, batcap[0] %02x, accum[0] %02x, soc_arr[0] %d, ocv_arr[0] %d, volt_tun : %02x\n",
					__func__, i,
					fuelgauge->age_data_info[i].battery_table3[0],
					fuelgauge->age_data_info[i].battery_table4[0],
					fuelgauge->age_data_info[i].batcap[0],
					fuelgauge->age_data_info[i].accum[0],
					fuelgauge->age_data_info[i].soc_arr_val[0],
					fuelgauge->age_data_info[i].ocv_arr_val[0],
					fuelgauge->age_data_info[i].volt_mode_tunning);
			}
#endif
		}
	}

	return 0;
}

ssize_t s2mu106_fg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf);

ssize_t s2mu106_fg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);
#define S2MU106_ATTR(_name)				\
{							\
	.attr = {.name = #_name, .mode = 0664},	\
	.show = s2mu106_fg_show_attrs,			\
	.store = s2mu106_fg_store_attrs,			\
}
enum {
	CHIP_ID = 0,
	DATA
};
static struct device_attribute s2mu106_fg_attrs[] = {
	S2MU106_ATTR(chip_id),
	S2MU106_ATTR(data),
};
static int s2mu106_fg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(s2mu106_fg_attrs); i++) {
		rc = device_create_file(dev, &s2mu106_fg_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &s2mu106_fg_attrs[i]);
	return rc;
}

ssize_t s2mu106_fg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mu106_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - s2mu106_fg_attrs;
	int i = 0;
	u8 addr = 0, data = 0;

	switch (offset) {
	case CHIP_ID:
		s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_FG_ID, &data);
		i += scnprintf(buf + i, PAGE_SIZE - i,
				"0x%02x : 0x%02x\n", S2MU106_REG_FG_ID, data);
		break;
	case DATA:
		for (addr = 0x00; addr <= 0x1F; addr++) {
			if (addr == 0x02)
				continue;
			s2mu106_read_reg_byte(fuelgauge->i2c, addr, &data);
			i += scnprintf(buf + i, PAGE_SIZE - i,
				       "0x%02x:0x%02x\n", addr, data);
		}
		for (addr = 0x24; addr <= 0x29; addr++) {
			s2mu106_read_reg_byte(fuelgauge->i2c, addr, &data);
			i += scnprintf(buf + i, PAGE_SIZE - i,
				       "0x%02x:0x%02x\n", addr, data);
		}
		for (addr = 0x40; addr <= 0x5C; addr++) {
			s2mu106_read_reg_byte(fuelgauge->i2c, addr, &data);
			i += scnprintf(buf + i, PAGE_SIZE - i,
				       "0x%02x:0x%02x\n", addr, data);
		}
		s2mu106_read_reg_byte(fuelgauge->i2c, S2MU106_REG_FG_ID, &data);
		i += scnprintf(buf + i, PAGE_SIZE - i,
				"0x%02x:0x%02x\n", S2MU106_REG_FG_ID, data);
		break;
	default:
		return -EINVAL;
	}
	return i;
}

ssize_t s2mu106_fg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mu106_fuelgauge_data *fuelgauge = power_supply_get_drvdata(psy);
	const ptrdiff_t offset = attr - s2mu106_fg_attrs;
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
				if (s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, addr, data) < 0) {
					dev_info(fuelgauge->dev,
						"%s: addr: 0x%x write fail\n", __func__, addr);
				}
			} else {
				dev_info(fuelgauge->dev,
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
static struct of_device_id s2mu106_fuelgauge_match_table[] = {
	{ .compatible = "samsung,s2mu106-fuelgauge",},
	{},
};
#else
static int s2mu106_fuelgauge_parse_dt(struct s2mu106_fuelgauge_data *fuelgauge)
{
	return -ENOSYS;
}

#define s2mu106_fuelgauge_match_table NULL
#endif /* CONFIG_OF */

static const struct power_supply_desc s2mu106_fuelgauge_power_supply_desc = {
	.name = "s2mu106-fuelgauge",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = s2mu106_fuelgauge_props,
	.num_properties = ARRAY_SIZE(s2mu106_fuelgauge_props),
	.get_property = s2mu106_fg_get_property,
	.set_property = s2mu106_fg_set_property,
};

static int s2mu106_fuelgauge_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct s2mu106_fuelgauge_data *fuelgauge;
	union power_supply_propval raw_soc_val;
	struct power_supply_config fuelgauge_cfg = {};
	int ret = 0;
	u8 temp = 0;

	pr_info("%s: S2MU106 Fuelgauge Driver Loading\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
	if (!fuelgauge)
		return -ENOMEM;

	mutex_init(&fuelgauge->fg_lock);

	fuelgauge->i2c = client;

	if (client->dev.of_node) {
		fuelgauge->pdata = devm_kzalloc(&client->dev, sizeof(*(fuelgauge->pdata)),
				GFP_KERNEL);
		if (!fuelgauge->pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_parse_dt_nomem;
		}
		ret = s2mu106_fuelgauge_parse_dt(fuelgauge);
		if (ret < 0)
			goto err_parse_dt;
	} else {
		fuelgauge->pdata = client->dev.platform_data;
	}

	i2c_set_clientdata(client, fuelgauge);
	fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;

	if (fuelgauge->pdata->fuelgauge_name == NULL)
		fuelgauge->pdata->fuelgauge_name = "s2mu106-fuelgauge";

	fuelgauge_cfg.drv_data = fuelgauge;

	fuelgauge->revision = 0;
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x48, &temp);
	fuelgauge->revision = (temp & 0xF0) >> 4;

	pr_info("%s: S2MU106 Fuelgauge revision: 0x%x, reg 0x48 = 0x%x\n",
			__func__, fuelgauge->revision, temp);

	fuelgauge->info.soc = 0;

	/* default CURRENT_MODE setting */
	fuelgauge->mode = CURRENT_MODE;
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0x10);
	s2mu106_read_reg_byte(fuelgauge->i2c, 0x4B, &temp);
	temp &= 0x8F;
	s2mu106_write_and_verify_reg_byte(fuelgauge->i2c, 0x4B, temp);

	raw_soc_val.intval = s2mu106_get_rawsoc(fuelgauge);
	raw_soc_val.intval = raw_soc_val.intval / 10;

	if (raw_soc_val.intval > fuelgauge->capacity_max)
		s2mu106_fg_calculate_dynamic_scale(fuelgauge, 100);

#if (TEMP_COMPEN)
	fuelgauge->init_start = 1;
	fuelgauge->flag_mapping = false;
#endif
#if (BATCAP_LEARN)
	fuelgauge->learn_start = false;
	fuelgauge->cond1_ok = false;
	fuelgauge->c1_count = 0;
	fuelgauge->c2_count = 0;
#endif

	s2mu106_init_regs(fuelgauge);

	fuelgauge->psy_fg = power_supply_register(
		&client->dev, &s2mu106_fuelgauge_power_supply_desc, &fuelgauge_cfg);
	if (!fuelgauge->psy_fg) {
		pr_err("%s: Failed to Register psy_fg\n", __func__);
		ret = PTR_ERR(fuelgauge->psy_fg);
		goto err_data_free;
	}

	fuelgauge->is_fuel_alerted = false;
	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		s2mu106_fuelgauge_fuelalert_init(fuelgauge->i2c,
					fuelgauge->pdata->fuel_alert_soc);
		wake_lock_init(&fuelgauge->fuel_alert_wake_lock,
					WAKE_LOCK_SUSPEND, "fuel_alerted");

		if (fuelgauge->pdata->fg_irq > 0) {
			INIT_DELAYED_WORK(
					&fuelgauge->isr_work, s2mu106_fg_isr_work);

			fuelgauge->fg_irq = gpio_to_irq(fuelgauge->pdata->fg_irq);
			dev_info(&client->dev,
					"%s : fg_irq = %d\n", __func__, fuelgauge->fg_irq);
			if (fuelgauge->fg_irq > 0) {
				ret = request_threaded_irq(fuelgauge->fg_irq,
						NULL, s2mu106_fg_irq_thread,
						IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING
						| IRQF_ONESHOT,
						"fuelgauge-irq", fuelgauge);
				if (ret) {
					dev_err(&client->dev,
							"%s: Failed to Request IRQ\n", __func__);
					goto err_supply_unreg;
				}

				ret = enable_irq_wake(fuelgauge->fg_irq);
				if (ret < 0)
					dev_err(&client->dev,
							"%s: Failed to Enable Wakeup Source(%d)\n",
							__func__, ret);
			} else {
				dev_err(&client->dev, "%s: Failed gpio_to_irq(%d)\n",
						__func__, fuelgauge->fg_irq);
				goto err_supply_unreg;
			}
		}
	}

	fuelgauge->cable_type = SEC_BATTERY_CABLE_NONE;
	fuelgauge->sleep_initial_update_of_soc = false;
	fuelgauge->initial_update_of_soc = true;
#if (TEMP_COMPEN) || (BATCAP_LEARN)
	fuelgauge->bat_charging = false;
#endif
	fuelgauge->probe_done = true;
	fuelgauge->psy_chg = power_supply_get_by_name("s2mu106-charger");
	fuelgauge->psy_bat = power_supply_get_by_name("battery");

	ret = s2mu106_fg_create_attrs(&fuelgauge->psy_fg->dev);
	if (ret) {
		dev_err(&client->dev,
			"%s : Failed to create_attrs\n", __func__);
	}
	pr_info("%s: S2MU106 Fuelgauge Driver Loaded\n", __func__);
	return 0;

err_supply_unreg:
	power_supply_unregister(fuelgauge->psy_fg);
err_data_free:
	if (client->dev.of_node)
		kfree(fuelgauge->pdata);

err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&fuelgauge->fg_lock);
	kfree(fuelgauge);

	return ret;
}

static const struct i2c_device_id s2mu106_fuelgauge_id[] = {
	{"s2mu106-fuelgauge", 0},
	{}
};

static void s2mu106_fuelgauge_shutdown(struct i2c_client *client)
{

}

static int s2mu106_fuelgauge_remove(struct i2c_client *client)
{
	struct s2mu106_fuelgauge_data *fuelgauge = i2c_get_clientdata(client);

	if (fuelgauge->pdata->fuel_alert_soc >= 0)
		wake_lock_destroy(&fuelgauge->fuel_alert_wake_lock);

	return 0;
}

#if defined CONFIG_PM
static int s2mu106_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int s2mu106_fuelgauge_resume(struct device *dev)
{
	struct s2mu106_fuelgauge_data *fuelgauge = dev_get_drvdata(dev);

	fuelgauge->sleep_initial_update_of_soc = true;

	return 0;
}
#else
#define s2mu106_fuelgauge_suspend NULL
#define s2mu106_fuelgauge_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(s2mu106_fuelgauge_pm_ops, s2mu106_fuelgauge_suspend,
		s2mu106_fuelgauge_resume);

static struct i2c_driver s2mu106_fuelgauge_driver = {
	.driver = {
		.name = "s2mu106-fuelgauge",
		.owner = THIS_MODULE,
		.pm = &s2mu106_fuelgauge_pm_ops,
		.of_match_table = s2mu106_fuelgauge_match_table,
	},
	.probe = s2mu106_fuelgauge_probe,
	.remove = s2mu106_fuelgauge_remove,
	.shutdown = s2mu106_fuelgauge_shutdown,
	.id_table = s2mu106_fuelgauge_id,
};

static int __init s2mu106_fuelgauge_init(void)
{
	pr_info("%s: S2MU106 Fuelgauge Init\n", __func__);
	return i2c_add_driver(&s2mu106_fuelgauge_driver);
}

static void __exit s2mu106_fuelgauge_exit(void)
{
	i2c_del_driver(&s2mu106_fuelgauge_driver);
}
module_init(s2mu106_fuelgauge_init);
module_exit(s2mu106_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung S2MU106 Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
