/*
 * s2mu107_fuelgauge.c - S2MU107 Fuel Gauge Driver
 *
 * Copyright (C) 2019 Samsung Electronics, Inc.
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

#define DEBUG	1

#define SINGLE_BYTE	1
#define TABLE_SIZE	22

#include "include/fuelgauge/s2mu107_fuelgauge.h"
#include <linux/of_gpio.h>

static enum power_supply_property s2mu107_fuelgauge_props[] = {
};

static void s2mu107_init_regs(struct s2mu107_fuelgauge_data *fuelgauge);
#if defined(CONFIG_FUELGAUGE_S2MU107_TEMP_COMPEN)
static void s2mu107_init_temp_compen(struct s2mu107_fuelgauge_data *fuelgauge);
#endif
static void s2mu107_init_batcap_learn(struct s2mu107_fuelgauge_data *fuelgauge);

#if defined(CONFIG_CHARGER_S2MU107_DIRECT)
static void s2mu107_set_tperiod(struct s2mu107_fuelgauge_data *fuelgauge,
		bool is_dc_charging);
static void s2mu107_init_for_direct_charge(struct s2mu107_fuelgauge_data *fuelgauge);
#endif

#if defined(CONFIG_FUELGAUGE_S2MU107_USE_10MILLIOHM)
static void s2mu107_set_trim_10mohm(struct s2mu107_fuelgauge_data *fuelgauge);
#endif

static int s2mu107_get_vbat(struct s2mu107_fuelgauge_data *fuelgauge);
static int s2mu107_get_ocv(struct s2mu107_fuelgauge_data *fuelgauge);
static int s2mu107_get_current(struct s2mu107_fuelgauge_data *fuelgauge);
static int s2mu107_get_avgcurrent(struct s2mu107_fuelgauge_data *fuelgauge);
static int s2mu107_get_avgvbat(struct s2mu107_fuelgauge_data *fuelgauge);

static int s2mu107_read_reg_byte(struct i2c_client *client, int reg, void *data)
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

/* I2C write enable for bulk write */
static int s2mu107_write_enable(struct i2c_client *client)
{
	u8 data = 0;
	int ret = 0;
	int i;

	ret = s2mu107_read_reg_byte(client, 0x03, &data);

	data = data | IF_EN_MASK;

	ret = i2c_smbus_write_byte_data(client, 0x03, data);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = i2c_smbus_write_byte_data(client, 0x03, data);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}

	return ret;
}

/* I2C write disable for bulk write */
static int s2mu107_write_disable(struct i2c_client *client)
{
	u8 data = 0;
	int ret = 0;
	int i;

	ret = s2mu107_read_reg_byte(client, 0x03, &data);

	data = data & ~IF_EN_MASK;

	ret = i2c_smbus_write_byte_data(client, 0x03, data);
	if (ret < 0) {
		for (i = 0; i < 3; i++) {
			ret = i2c_smbus_write_byte_data(client, 0x03, data);
			if (ret >= 0)
				break;
		}

		if (i >= 3)
			dev_err(&client->dev, "%s: Error(%d)\n", __func__, ret);
	}

	return ret;
}

static int s2mu107_write_and_verify_reg_byte(struct i2c_client *client, int reg, u8 data)
{
	int ret, i = 0;
	int i2c_corrupted_cnt = 0;
	u8 temp = 0;

	ret = s2mu107_write_enable(client);

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

	/* TODO: Update non-writable registers */
	if ((reg == 0xee) || (reg == 0xef) || (reg == 0xf2) || (reg == 0xf3) ||
		(reg == 0x0C) || (reg == 0x1e) || (reg == 0x1f) || (reg == 0x27) ||
		(reg == 0x8E) || (reg == 0x90)) {
		ret = s2mu107_write_disable(client);
		return ret;
	}

	s2mu107_read_reg_byte(client, reg, &temp);
	while ((temp != data) && (i2c_corrupted_cnt < 5)) {
		dev_err(&client->dev,
			"%s: I2C write Incorrect! REG: 0x%x Expected: 0x%x Real-Value: 0x%x\n",
			__func__, reg, data, temp);
		ret = i2c_smbus_write_byte_data(client, reg, data);
		s2mu107_read_reg_byte(client, reg, &temp);
		i2c_corrupted_cnt++;
	}

	if (i2c_corrupted_cnt == 5)
		dev_err(&client->dev,
			"%s: I2C write failed REG: 0x%x Expected: 0x%x\n",
			__func__, reg, data);

	ret = s2mu107_write_disable(client);
	return ret;
}

/* I2C Write & Verify for bulk write */
static int s2mu107_write_and_verify_reg_byte_no_en(struct i2c_client *client, int reg, u8 data)
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

	/* TODO: Update non-writable registers */
	if ((reg == 0xee) || (reg == 0xef) || (reg == 0xf2) || (reg == 0xf3) ||
		(reg == 0x0C) || (reg == 0x1e) || (reg == 0x1f) || (reg == 0x27)) {
		return ret;
	}

	s2mu107_read_reg_byte(client, reg, &temp);
	while ((temp != data) && (i2c_corrupted_cnt < 5)) {
		dev_err(&client->dev,
			"%s: I2C write Incorrect! REG: 0x%x Expected: 0x%x Real-Value: 0x%x\n",
			__func__, reg, data, temp);
		ret = i2c_smbus_write_byte_data(client, reg, data);
		s2mu107_read_reg_byte(client, reg, &temp);
		i2c_corrupted_cnt++;
	}

	if (i2c_corrupted_cnt == 5)
		dev_err(&client->dev,
			"%s: I2C write failed REG: 0x%x Expected: 0x%x\n",
			__func__, reg, data);

	return ret;
}

static int s2mu107_update_reg_byte(struct i2c_client *client, int reg, u8 val, u8 mask)
{
	int ret;
	u8 old_val = 0, new_val = 0;

	ret = s2mu107_read_reg_byte(client, reg, &old_val);
	if (ret >= 0) {
		new_val = (val & mask) | (old_val & (~mask));
		ret = s2mu107_write_and_verify_reg_byte(client, reg, new_val);
	}

	return ret;
}

static int s2mu107_update_reg_byte_no_en(struct i2c_client *client, int reg, u8 val, u8 mask)
{
	int ret;
	u8 old_val = 0, new_val = 0;

	ret = s2mu107_read_reg_byte(client, reg, &old_val);
	if (ret >= 0) {
		new_val = (val & mask) | (old_val & (~mask));
		ret = s2mu107_write_and_verify_reg_byte_no_en(client, reg, new_val);
	}

	return ret;
}
#if 0
static int s2mu107_write_reg(struct i2c_client *client, int reg, u8 *buf)
{
#if SINGLE_BYTE
	int ret = 0;
	s2mu107_write_enable(client);
	s2mu107_write_and_verify_reg_byte_no_en(client, reg, buf[0]);
	s2mu107_write_and_verify_reg_byte_no_en(client, reg+1, buf[1]);
	s2mu107_write_disable(client);
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
#endif

static int s2mu107_read_reg(struct i2c_client *client, int reg, u8 *buf)
{

#if SINGLE_BYTE
	int ret = 0;
	u8 data1 = 0, data2 = 0;
	s2mu107_read_reg_byte(client, reg, &data1);
	s2mu107_read_reg_byte(client, reg+1, &data2);
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

static int calc_ttf(struct s2mu107_fuelgauge_data *fuelgauge,
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

static void s2mu107_fg_test_read(struct i2c_client *client)
{
	static int reg_list[] = {
		0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0D,
		0x0E, 0x0F, 0x10, 0x11, 0x14, 0x1A, 0x1B, 0x1E, 0x1F, 0x24,
		0x25, 0x26, 0x27, 0x28, 0x29, 0x40, 0x41, 0x43, 0x44, 0x45,
		0x46, 0x48, 0x4A, 0x4B, 0x50, 0x51, 0x52, 0x53, 0x58, 0x59,
		0x5A, 0x5B, 0x5C, 0x67, 0x6B, 0x6D, 0x70, 0x71, 0x72, 0x73,
		0x7A, 0x7B, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
		0x88, 0x89, 0x8E, 0x8F, 0x90, 0x91
	};
	u8 data = 0;
	char str[1016] = {0,};
	int i = 0, reg_list_size = 0;

	reg_list_size = ARRAY_SIZE(reg_list);
	for (i = 0; i < reg_list_size; i++) {
		s2mu107_read_reg_byte(client, reg_list[i], &data);
		sprintf(str+strlen(str), "0x%02x:0x%02x, ", reg_list[i], data);
	}

	/* print buffer */
	pr_info("%s: %s\n", __func__, str);
}

int s2mu107_fg_check_current_level(struct s2mu107_fuelgauge_data *fuelgauge)
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

static void s2mu107_restart_gauging(struct s2mu107_fuelgauge_data *fuelgauge)
{
	/* TODO: Update reset sequence */
	pr_info("%s: Re-calculate SOC and voltage\n", __func__);

	mutex_lock(&fuelgauge->fg_lock);

	s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, S2MU107_REG_START, 0x0F);

	msleep(300);

	mutex_unlock(&fuelgauge->fg_lock);
}

/* Need to lock/unlock start&end of reset */
static void s2mu107_reset_fg(struct s2mu107_fuelgauge_data *fuelgauge)
{
	int i;

	/* Enable I2C write for battery parameter write */
	s2mu107_write_enable(fuelgauge->i2c);

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, S2MU107_REG_RBATCAP_OCV,
			fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[0]);
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, S2MU107_REG_RBATCAP_OCV + 1,
			fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[1]);

	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		S2MU107_REG_RBATCAP_OCV_NEW_IN,
		(fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[0] | 0x01));
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		S2MU107_REG_RBATCAP_OCV_NEW_IN + 1,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[1]);

	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, S2MU107_REG_RDESIGN_CAP,
			fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[0]);
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, S2MU107_REG_RDESIGN_CAP + 1,
			fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[1]);

	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, S2MU107_REG_RBATCAP,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[2]);
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, S2MU107_REG_RBATCAP + 1,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].batcap[3]);

	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, 0x13,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].volt_mode_tunning);
#else
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		S2MU107_REG_RBATCAP_OCV, fuelgauge->info.batcap[0]);
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		S2MU107_REG_RBATCAP_OCV + 1, fuelgauge->info.batcap[1]);

	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		S2MU107_REG_RBATCAP_OCV_NEW_IN, (fuelgauge->info.batcap[0] | 0x01));
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		S2MU107_REG_RBATCAP_OCV_NEW_IN + 1, fuelgauge->info.batcap[1]);

	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		S2MU107_REG_RDESIGN_CAP, fuelgauge->info.batcap[0]);
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		S2MU107_REG_RDESIGN_CAP + 1, fuelgauge->info.batcap[1]);

	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		S2MU107_REG_RBATCAP, fuelgauge->info.batcap[2]);
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		S2MU107_REG_RBATCAP + 1, fuelgauge->info.batcap[3]);
#endif

	/* After battery capacity update, set BATCAP_OCV_EN(0x0C[6]=1) */
	s2mu107_update_reg_byte_no_en(fuelgauge->i2c, 0x0C, 0x40, 0x40);

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	for(i = 0x92; i <= 0xe9; i++) {
		s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, i,
			fuelgauge->age_data_info[fuelgauge->fg_age_step].battery_table3[i - 0x92]);
	}
	for(i = 0xea; i <= 0xff; i++) {
		s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, i,
			fuelgauge->age_data_info[fuelgauge->fg_age_step].battery_table4[i - 0xea]);
	}
#else
	for (i = 0x92; i <= 0xe9; i++)
		s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, i, fuelgauge->info.battery_table3[i - 0x92]);
	for (i = 0xea; i <= 0xff; i++)
		s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, i, fuelgauge->info.battery_table4[i - 0xea]);
#endif

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, 0x44,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].accum[0]);
	s2mu107_update_reg_byte_no_en(fuelgauge->i2c, 0x45,
		fuelgauge->age_data_info[fuelgauge->fg_age_step].accum[1], 0x0F);
#else
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, 0x44, fuelgauge->info.accum[0]);
	s2mu107_update_reg_byte_no_en(fuelgauge->i2c, 0x45, fuelgauge->info.accum[1], 0x0F);
#endif

	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, 0x14, 0x67);

	s2mu107_update_reg_byte_no_en(fuelgauge->i2c, 0x4B, 0x00, 0x70);
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, 0x4A, 0x10);

	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, 0x40, 0x08);
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, 0x41, 0x04);

	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, 0x5C, 0x1A);

	/* Dumpdone. Re-calculate SOC */
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, S2MU107_REG_START, 0x0F);
	msleep(300);

	/* If it was voltage mode, recover it */
	if (fuelgauge->mode == HIGH_SOC_VOLTAGE_MODE) {
		s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, 0x4A, 0xFF);
		s2mu107_update_reg_byte_no_en(fuelgauge->i2c, 0x4B, 0x70, 0x70);
	}

	s2mu107_write_disable(fuelgauge->i2c);

	pr_info("%s: Reset FG completed\n", __func__);
}

static int s2mu107_fix_rawsoc_reset_fg(struct s2mu107_fuelgauge_data *fuelgauge)
{
	int ret = 0, ui_soc = 0, f_soc = 0;
	u8 data;
	struct power_supply *psy;
	union power_supply_propval value;

	psy = power_supply_get_by_name("battery");
	if (!psy)
		return -EINVAL;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &value);
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
	s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, 0x29, data);

	/* Parameter write */
	s2mu107_reset_fg(fuelgauge);

	/* Disable rawsoc fix */
	s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, 0x29, 0x00);

	dev_info(&fuelgauge->i2c->dev, "%s: Finish\n", __func__);

	return ret;
}

/* Set model data version for next boot up initializing fuelgauge */
static void s2mu107_fg_reset_capacity_by_jig_connection(struct s2mu107_fuelgauge_data *fuelgauge)
{
	/* TODO : model data version check */
	u8 data = 0;

	s2mu107_read_reg_byte(fuelgauge->i2c, S2MU107_REG_FG_ID, &data);
	data &= 0xF0;
	data |= 0x0F; //set model data version 0xF for next boot up initializing fuelgague
	s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, S2MU107_REG_FG_ID, data);

	pr_info("%s: set Model data version (0x%x)\n", __func__, data & 0x0F);
}

static int s2mu107_set_temperature(struct s2mu107_fuelgauge_data *fuelgauge,
			int temperature)
{
	/* TODO: Add temperature setting code */
	return temperature;
}

static int s2mu107_get_temperature(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int temperature = 0;

	mutex_lock(&fuelgauge->fg_lock);

	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		S2MU107_REG_MONOUT_SEL, S2MU107_MONOUT_SEL_AVGTEMP);
	if (s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_MONOUT, data) < 0)
		goto err;

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

static int s2mu107_fg_check_surge(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 por_state = 0;
	u8 reg_1E = 0;
	u8 reg_OTP_52 = 0, reg_OTP_53 = 0;
#if defined(CONFIG_CHARGER_S2MU107)
	bool charging_enabled = false;
	struct power_supply *psy;
	union power_supply_propval value;
	int ret;
#endif

	s2mu107_read_reg_byte(fuelgauge->i2c, S2MU107_REG_START, &reg_1E);
	s2mu107_read_reg_byte(fuelgauge->i2c, S2MU107_REG_START + 1, &por_state);
	s2mu107_read_reg_byte(fuelgauge->i2c, 0x52, &reg_OTP_52);
	s2mu107_read_reg_byte(fuelgauge->i2c, 0x53, &reg_OTP_53);

	dev_err(&fuelgauge->i2c->dev, "%s: OTP 52(%02x) 53(%02x), current 52(%02x) 53(%02x), "
			"0x1F(%02x), 0x1E(%02x)\n", __func__, fuelgauge->reg_OTP_52, fuelgauge->reg_OTP_53,
			reg_OTP_52, reg_OTP_53, por_state, reg_1E);

#if defined(CONFIG_BATTERY_AGE_FORECAST)
	if((((por_state != 0x00) || (reg_1E != 0x03)) && (fuelgauge->age_reset_status == 0)) ||
#else
	if(((por_state != 0x00) || (reg_1E != 0x03)) ||
#endif
		(fuelgauge->probe_done == true &&
		(fuelgauge->reg_OTP_52 != reg_OTP_52 || fuelgauge->reg_OTP_53 != reg_OTP_53))) {

#if defined(CONFIG_CHARGER_S2MU107)
		/* check charging enable */
		psy = power_supply_get_by_name(fuelgauge->pdata->charger_name);
		if (!psy)
			return -EINVAL;
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);
		charging_enabled = value.intval;

		value.intval = SEC_BAT_CHG_MODE_CHARGING_OFF;
		psy = power_supply_get_by_name(fuelgauge->pdata->charger_name);
		if (!psy)
			return -EINVAL;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &value);
		if (ret < 0)
			pr_err("%s: Fail to execute property\n", __func__);
#endif

		mutex_lock(&fuelgauge->fg_lock);

		if (fuelgauge->reg_OTP_52 != reg_OTP_52 || fuelgauge->reg_OTP_53 != reg_OTP_53) {
			s2mu107_write_enable(fuelgauge->i2c);
			s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
				S2MU107_REG_START + 1, 0x40);
			usleep_range(10000, 11000);

			s2mu107_write_enable(fuelgauge->i2c);
			s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
				S2MU107_REG_START + 1, 0x01);
			msleep(50);
			s2mu107_write_disable(fuelgauge->i2c);

			s2mu107_read_reg_byte(fuelgauge->i2c, 0x53, &reg_OTP_53);
			s2mu107_read_reg_byte(fuelgauge->i2c, 0x52, &reg_OTP_52);

			dev_err(&fuelgauge->i2c->dev, "1st reset after %s: OTP 52(%02x) 53(%02x) "
					"current 52(%02x) 53(%02x)\n", __func__,
					fuelgauge->reg_OTP_52, fuelgauge->reg_OTP_53, reg_OTP_52, reg_OTP_53);

			if (fuelgauge->reg_OTP_52 != reg_OTP_52 || fuelgauge->reg_OTP_53 != reg_OTP_53) {
				s2mu107_write_enable(fuelgauge->i2c);
				s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
					S2MU107_REG_START + 1, 0x40);
				usleep_range(10000, 11000);

				s2mu107_write_enable(fuelgauge->i2c);
				s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
					S2MU107_REG_START + 1, 0x01);
				msleep(50);
				s2mu107_write_disable(fuelgauge->i2c);

				dev_err(&fuelgauge->i2c->dev, "%s : 2nd reset\n", __func__);
			}
		}

		dev_info(&fuelgauge->i2c->dev, "%s: FG reset\n", __func__);
		/* If UI SOC is 0%, do not use raw SOC fix reset */
		if(fuelgauge->ui_soc == 0)
			s2mu107_reset_fg(fuelgauge);
		else
			s2mu107_fix_rawsoc_reset_fg(fuelgauge);
		por_state = 0x00;
		s2mu107_write_and_verify_reg_byte(fuelgauge->i2c,
			S2MU107_REG_START + 1, por_state);

		/* Need to do initial setting again, after IC reset */
		s2mu107_init_regs(fuelgauge);
#if defined(CONFIG_FUELGAUGE_S2MU107_TEMP_COMPEN)
		s2mu107_init_temp_compen(fuelgauge);
#endif
		s2mu107_init_batcap_learn(fuelgauge);

#if defined(CONFIG_FUELGAUGE_S2MU107_USE_10MILLIOHM)
		s2mu107_set_trim_10mohm(fuelgauge);
#endif
#if defined(CONFIG_CHARGER_S2MU107_DIRECT)
		s2mu107_init_for_direct_charge(fuelgauge);
#endif
		mutex_unlock(&fuelgauge->fg_lock);

#if defined(CONFIG_CHARGER_S2MU107)
		/* Recover charger status after f.g reset */
		if (charging_enabled) {
			value.intval = SEC_BAT_CHG_MODE_CHARGING;
			psy = power_supply_get_by_name(fuelgauge->pdata->charger_name);
			if (!psy)
				return -EINVAL;
			ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &value);
			if (ret < 0)
				pr_err("%s: Fail to execute property\n", __func__);
		}
#endif
	}

	return 0;
}

static void s2mu107_fg_update_mode(struct s2mu107_fuelgauge_data *fuelgauge)
{
	int float_voltage = 0;
#if defined(CONFIG_CHARGER_S2MU107)
	struct power_supply *psy;
	union power_supply_propval value;
	int ret;
#endif
	u8 reg_0x67;

#if defined(CONFIG_CHARGER_S2MU107)
	psy = power_supply_get_by_name(fuelgauge->pdata->charger_name);
	if (!psy)
		float_voltage = 4350;
	else {
		ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_MAX, &value);
		if (ret < 0) {
			pr_err("%s: Fail to execute property\n", __func__);
			float_voltage = 4350;
		} else
			float_voltage = value.intval;
	}
#else
	float_voltage = 4350;
#endif

	float_voltage = (float_voltage * 996) / 1000;

	mutex_lock(&fuelgauge->fg_lock);

	if ((fuelgauge->is_charging == true) &&
			((fuelgauge->ui_soc >= 98) ||
			 ((fuelgauge->avg_vbat > float_voltage) &&
			  (fuelgauge->avg_curr < s2mu107_fg_check_current_level(fuelgauge))))) {
		if (fuelgauge->mode == CURRENT_MODE) { /* switch to VOLTAGE_MODE */
			fuelgauge->mode = HIGH_SOC_VOLTAGE_MODE;

			s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0xFF);
			s2mu107_update_reg_byte(fuelgauge->i2c, 0x4B, 0x70, 0x70);

			dev_info(&fuelgauge->i2c->dev, "%s: FG is in high soc voltage mode\n", __func__);
		}
	} else if (fuelgauge->avg_curr < -50 || fuelgauge->avg_curr >= s2mu107_fg_check_current_level(fuelgauge)) {
		if (fuelgauge->mode == HIGH_SOC_VOLTAGE_MODE) {
			fuelgauge->mode = CURRENT_MODE;

			s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0x10);
			s2mu107_update_reg_byte(fuelgauge->i2c, 0x4B, 0x00, 0x70);

			dev_info(&fuelgauge->i2c->dev, "%s: FG is in current mode\n", __func__);
		}
	}

	s2mu107_read_reg_byte(fuelgauge->i2c, 0x67, &reg_0x67);

	if ((fuelgauge->avg_vbat > 3400) && (fuelgauge->is_charging == true) &&
			(fuelgauge->soc_m < 400) && ((reg_0x67 & 0x02) == 0x02)) {
		s2mu107_update_reg_byte(fuelgauge->i2c, 0x67, 0x00, 0x02);
		pr_info("%s: 0x67[1] = 0", __func__);
	} else if ((fuelgauge->soc_m > 450) && ((reg_0x67 & 0x02) == 0x00)) {
		s2mu107_update_reg_byte(fuelgauge->i2c, 0x67, 0x02, 0x02);
		pr_info("%s: 0x67[1] = 1", __func__);
	}

	mutex_unlock(&fuelgauge->fg_lock);
}

static void s2mu107_fg_low_vbat_WA(struct s2mu107_fuelgauge_data *fuelgauge)
{
	/* Low voltage W/A, make 0% */
	if (fuelgauge->temperature > fuelgauge->low_temp_limit) {
		if ((fuelgauge->avg_vbat < fuelgauge->low_vbat_threshold) &&
				(fuelgauge->avg_curr < -50) && (fuelgauge->soc_m > 300)) {
			dev_info(&fuelgauge->i2c->dev, "%s: Low voltage WA.\n", __func__);

			mutex_lock(&fuelgauge->fg_lock);

			s2mu107_write_enable(fuelgauge->i2c);
			s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, 0x29, 0x07);
			/* Dumpdone. Re-calculate SOC */
			s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, S2MU107_REG_START, 0x0F);
			msleep(300);
			s2mu107_update_reg_byte_no_en(fuelgauge->i2c, 0x29, 0x00, 0x01);

			s2mu107_write_disable(fuelgauge->i2c);

			mutex_unlock(&fuelgauge->fg_lock);
		}
	} else {
		if ((fuelgauge->avg_vbat < fuelgauge->low_vbat_threshold_lowtemp) &&
				(fuelgauge->avg_curr < -50) && (fuelgauge->info.soc > 100)) {
			dev_info(&fuelgauge->i2c->dev, "%s: Low voltage WA. Make UI SOC 0\n", __func__);

			/* Make report SOC 0% */
			fuelgauge->info.soc = 0;
		}
	}
}

static int s2mu107_get_raw_soc(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 data[2], check_data[2];
	u16 compliment;
	int rsoc = 0;
	int i;

	mutex_lock(&fuelgauge->fg_lock);

	for (i = 0; i < 50; i++) {
		if (s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_RSOC, data) < 0)
			goto err;

		if (s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_RSOC, check_data) < 0)
			goto err;

		if ((data[0] == check_data[0]) && (data[1] == check_data[1])) {
			dev_dbg(&fuelgauge->i2c->dev,
					"%s: data0 (%d) data1 (%d)\n", __func__, data[0], data[1]);
			break;
		}
	}

	mutex_unlock(&fuelgauge->fg_lock);

	compliment = (data[1] << 8) | (data[0]);
	if (compliment & (0x1 << 15)) {
		/* Negative */
		rsoc = ((~compliment) & 0xFFFF) + 1;
		rsoc = (rsoc * (-10000)) / (0x1 << 14);
	} else {
		rsoc = compliment & 0x7FFF;
		rsoc = ((rsoc * 10000) / (0x1 << 14));
	}

	return rsoc;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

static int s2mu107_get_compen_soc(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 data[2], check_data[2], temp = 0;
	u16 compliment;
	int soc_r = 0;
	int i, ui_soc = 0;
	int update_soc;

	mutex_lock(&fuelgauge->fg_lock);

	if (fuelgauge->init_start) {
		s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_RSOC_R_SAVE, data);
		if (data[1] == 0) {
			ui_soc = (data[1] << 8) | (data[0]);
			if ((fuelgauge->temperature < fuelgauge->low_temp_limit) || ui_soc == 100) {
				pr_info("%s: temperature is low or UI soc 100! use saved UI SOC(%d)"
						" for mapping, data[1] = 0x%02x, data[0] = 0x%02x\n",
						__func__, ui_soc, data[1], data[0]);

				fuelgauge->ui_soc = ui_soc;
				fuelgauge->capacity_old = ui_soc;
				if (fuelgauge->temperature < fuelgauge->low_temp_limit)
					fuelgauge->initial_update_of_soc = false;

				s2mu107_read_reg_byte(fuelgauge->i2c, 0x67, &temp);
				temp = temp | TEMP_COMPEN_INC_OK_EN;
				s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, 0x67, temp);

				if (ui_soc == 100)
					update_soc = 0xFFFF;
				else
					update_soc = (ui_soc * (0x1 << 16)) / 100;

				/* WRITE_EN */
				data[0] = (update_soc & 0x00FF) | 0x0001;
				data[1] = (update_soc & 0xFF00) >> 8;

				s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, S2MU107_REG_RSOC_R_I2C + 1, data[1]);
				s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, S2MU107_REG_RSOC_R_I2C, data[0]);

				msleep(300);

				s2mu107_read_reg_byte(fuelgauge->i2c, 0x67, &temp);
				temp = temp & ~TEMP_COMPEN_INC_OK_EN;
				s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, 0x67, temp);

				s2mu107_fg_test_read(fuelgauge->i2c);
			}
		}
	}

	for (i = 0; i < 50; i++) {
		if (s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_RSOC_R, data) < 0)
			goto err;

		if (s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_RSOC_R, check_data) < 0)
			goto err;

		if ((data[0] == check_data[0]) && (data[1] == check_data[1])) {
			dev_dbg(&fuelgauge->i2c->dev,
					"%s: data0 (%d) data1 (%d)\n", __func__, data[0], data[1]);
			break;
		}
	}

	mutex_unlock(&fuelgauge->fg_lock);

	compliment = (data[1] << 8) | (data[0]);
	if (compliment & (0x1 << 15)) {
		/* Negative */
		soc_r = ((~compliment) & 0xFFFF) + 1;
		soc_r = (soc_r * (-10000)) / (0x1 << 14);
	} else {
		soc_r = compliment & 0x7FFF;
		soc_r = ((soc_r * 10000) / (0x1 << 14));
	}

	if (fuelgauge->init_start) {
		if (fuelgauge->temperature < fuelgauge->low_temp_limit) {
			s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_RSOC_R_SAVE, data);
			if (data[1] != 0) {
				fuelgauge->ui_soc = soc_r / 100;
				fuelgauge->capacity_old = soc_r / 100;
				fuelgauge->initial_update_of_soc = false;
			}
		}
	}

	fuelgauge->init_start = 0;

	/* Save UI SOC for maintain SOC, after low temperature reset */
	data[0] = fuelgauge->ui_soc;
	data[1] = 0;
	s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, S2MU107_REG_RSOC_R_SAVE, data[0]);
	s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, S2MU107_REG_RSOC_R_SAVE + 1, data[1]);

	/* Print UI SOC & saved value for debugging */
	s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_RSOC_R_SAVE, data);
	ui_soc = (data[1] << 8) | (data[0]);
	pr_info("%s: saved UI SOC = %d, data[1] = 0x%02x, data[0] = 0x%02x\n",
			__func__, ui_soc, data[1], data[0]);

	return soc_r;
err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

#if DEBUG
#define S2MU107_REG_RSOCR	0x82
#define S2MU107_REG_RSOH	0x84
#define S2MU107_REG_RRM		0x8A
#define S2MU107_REG_RFCC	0x8C

static int s2mu107_get_socr(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int socr;

	mutex_lock(&fuelgauge->fg_lock);

	if (s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_RSOCR, data) < 0)
		goto err;

	mutex_unlock(&fuelgauge->fg_lock);

	compliment = (data[1] << 8) | (data[0]);
	if (compliment & (0x1 << 15)) {
		/* Negative */
		socr = ((~compliment) & 0xFFFF) + 1;
		socr = (socr * (-10000)) / (0x1 << 14);
	} else {
		socr = compliment & 0x7FFF;
		socr = ((socr * 10000) / (0x1 << 14));
	}

	return socr;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

static int s2mu107_get_soh(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int soh;

	mutex_lock(&fuelgauge->fg_lock);

	if (s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_RSOH, data) < 0)
		goto err;

	mutex_unlock(&fuelgauge->fg_lock);

	compliment = (data[1] << 8) | (data[0]);
	if (compliment & (0x1 << 15)) {
		/* Negative */
		soh = ((~compliment) & 0xFFFF) + 1;
		soh = (soh * (-10000)) / (0x1 << 14);
	} else {
		soh = compliment & 0x7FFF;
		soh = ((soh * 10000) / (0x1 << 14));
	}

	return soh;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

static int s2mu107_get_rm(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int rm;

	mutex_lock(&fuelgauge->fg_lock);

	if (s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_RRM, data) < 0)
		goto err;

	mutex_unlock(&fuelgauge->fg_lock);

	compliment = (data[1] << 8) | (data[0]);
	rm = (compliment * 100) / (0x1 << 2);

	return rm;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

static int s2mu107_get_fcc(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int fcc;

	mutex_lock(&fuelgauge->fg_lock);

	if (s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_RFCC, data) < 0)
		goto err;

	mutex_unlock(&fuelgauge->fg_lock);

	compliment = (data[1] << 8) | (data[0]);
	fcc = (compliment * 100) / (0x1 << 2);

	return fcc;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

static int s2mu107_get_cycle(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment, cycle;

	mutex_lock(&fuelgauge->fg_lock);

	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		S2MU107_REG_MONOUT_SEL, S2MU107_MONOUT_SEL_CYCLE);

	if (s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_MONOUT, data) < 0)
		goto err;
	compliment = (data[1] << 8) | (data[0]);
	cycle = compliment;

	mutex_unlock(&fuelgauge->fg_lock);

	return cycle;
err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}
#endif

static int s2mu107_get_soc(struct s2mu107_fuelgauge_data *fuelgauge)
{
	int ret = 0;
	struct power_supply *psy;
	union power_supply_propval value;

	s2mu107_fg_check_surge(fuelgauge);

	/* Get UI SOC from battery driver */
	psy = power_supply_get_by_name("battery");
	if (!psy)
		return -EINVAL;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	fuelgauge->ui_soc = value.intval;

	/* TODO: Need to add avoiding first 0 degree */
	/* Get temperature from battery driver */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &value);
	if (ret < 0)
		pr_err("%s: Fail to execute property\n", __func__);
	fuelgauge->temperature = value.intval;

	/* Get raw SOC */
	fuelgauge->soc_m = s2mu107_get_raw_soc(fuelgauge);

	/* Get compensated SOC */
	fuelgauge->soc_r = s2mu107_get_compen_soc(fuelgauge);

	dev_info(&fuelgauge->i2c->dev, "%s: current_raw_soc (%d), current_compen_soc (%d), "
			"previous_soc (%d), FG_mode(%s)\n",
			__func__, fuelgauge->soc_m, fuelgauge->soc_r,
			fuelgauge->info.soc, mode_to_str[fuelgauge->mode]);

#if defined(CONFIG_FUELGAUGE_S2MU107_TEMP_COMPEN)
	fuelgauge->info.soc = fuelgauge->soc_r;
#else
	fuelgauge->info.soc = fuelgauge->soc_m;
#endif

	/* Collect other informations */
	fuelgauge->avg_curr = s2mu107_get_avgcurrent(fuelgauge);
	fuelgauge->avg_vbat = s2mu107_get_avgvbat(fuelgauge);
	fuelgauge->vbat = s2mu107_get_vbat(fuelgauge);
	fuelgauge->curr = s2mu107_get_current(fuelgauge);

	/* Fuel guage mode control */
	s2mu107_fg_update_mode(fuelgauge);

#if DEBUG
	/* TODO: Print information */
	fuelgauge->socr = s2mu107_get_socr(fuelgauge);
	fuelgauge->soh = s2mu107_get_soh(fuelgauge);
	fuelgauge->rm = s2mu107_get_rm(fuelgauge);
	fuelgauge->fcc = s2mu107_get_fcc(fuelgauge);
	fuelgauge->cycle = s2mu107_get_cycle(fuelgauge);
	pr_info("%s: avg_curr = %d, curr = %d, avg_vbat = %d, vbat = %d, "
		"socr = %d, soh = %d, rm = %d, fcc = %d, cycle = %d, IC temp = %d, batt temp = %d\n",
		__func__, fuelgauge->avg_curr, fuelgauge->curr, fuelgauge->avg_vbat, fuelgauge->vbat,
		fuelgauge->socr, fuelgauge->soh, fuelgauge->rm, fuelgauge->fcc, fuelgauge->cycle,
		s2mu107_get_temperature(fuelgauge), fuelgauge->temperature);
#endif

	s2mu107_fg_low_vbat_WA(fuelgauge);

	s2mu107_fg_test_read(fuelgauge->i2c);

	return min(fuelgauge->info.soc, 10000);
}

static int s2mu107_get_current(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int curr = 0;

	if (s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_RCUR_CC, data) < 0)
		return -EINVAL;

	compliment = (data[1] << 8) | (data[0]);
	dev_dbg(&fuelgauge->i2c->dev, "%s: rCUR_CC(0x%4x)\n", __func__, compliment);

	if (compliment & (0x1 << 15)) { /* Charging */
		curr = ((~compliment) & 0xFFFF) + 1;
		curr = (curr * 1000) >> 11;
	} else { /* dischaging */
		curr = compliment & 0x7FFF;
		curr = (curr * (-1000)) >> 11;
	}

	dev_info(&fuelgauge->i2c->dev, "%s: current (%d)mA\n", __func__, curr);

	return curr;
}

static int s2mu107_get_ocv(struct s2mu107_fuelgauge_data *fuelgauge)
{
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
	if (soc <= soc_arr[TABLE_SIZE - 1]) {
		ocv = ocv_arr[TABLE_SIZE - 1];
		goto ocv_soc_mapping;
	} else if (soc >= soc_arr[0]) {
		ocv = ocv_arr[0];
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
	if ((high_index >= 0 && high_index < TABLE_SIZE) &&
	    (low_index >= 0 && low_index < TABLE_SIZE)) {
			ocv = ocv_arr[high_index];
			ocv += ((ocv_arr[low_index] - ocv_arr[high_index]) *
				(soc - soc_arr[high_index])) /
				(soc_arr[low_index] - soc_arr[high_index]);
	}

ocv_soc_mapping:
	dev_info(&fuelgauge->i2c->dev, "%s: soc (%d), ocv (%d)\n", __func__, soc, ocv);
	return ocv;
}

static int s2mu107_get_avgcurrent(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment;
	int curr = 0;
	mutex_lock(&fuelgauge->fg_lock);

	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		S2MU107_REG_MONOUT_SEL, S2MU107_MONOUT_SEL_AVGCURR);

	if (s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_MONOUT, data) < 0)
		goto err;

	mutex_unlock(&fuelgauge->fg_lock);

	compliment = (data[1] << 8) | (data[0]);
	dev_dbg(&fuelgauge->i2c->dev, "%s: MONOUT(0x%4x)\n", __func__, compliment);

	if (compliment & (0x1 << 15)) { /* Charging */
		curr = ((~compliment) & 0xFFFF) + 1;
		curr = (curr * 1000) >> 11;
	} else { /* dischaging */
		curr = compliment & 0x7FFF;
		curr = (curr * (-1000)) >> 11;
	}

	dev_info(&fuelgauge->i2c->dev, "%s: avg current (%d)mA\n", __func__, curr);

	return curr;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

static int s2mu107_maintain_avgcurrent(
	struct s2mu107_fuelgauge_data *fuelgauge)
{
	static int cnt;
	int vcell = 0;
	int curr = 0;

	curr = s2mu107_get_avgcurrent(fuelgauge);

	vcell = s2mu107_get_vbat(fuelgauge);
	if ((cnt < 10) && (curr < 0) && (fuelgauge->is_charging) &&
		(vcell < 3500)) {
		curr = 1;
		cnt++;
		dev_info(&fuelgauge->i2c->dev, "%s: vcell (%d)mV, modified avg current (%d)mA\n",
				 __func__, vcell, curr);
	}

	return curr;
}

static int s2mu107_get_vbat(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u32 vbat = 0;

	if (s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_RVBAT, data) < 0)
		return -EINVAL;

	dev_dbg(&fuelgauge->i2c->dev, "%s: data0 (%d) data1 (%d)\n",
		__func__, data[0], data[1]);
	vbat = ((data[0] + (data[1] << 8)) * 1000) >> 13;

	dev_info(&fuelgauge->i2c->dev, "%s: vbat (%d)\n", __func__, vbat);

	return vbat;
}

static int s2mu107_get_avgvbat(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 data[2];
	u16 compliment, avg_vbat;

	mutex_lock(&fuelgauge->fg_lock);

	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		S2MU107_REG_MONOUT_SEL, S2MU107_MONOUT_SEL_AVGVBAT);

	if (s2mu107_read_reg(fuelgauge->i2c, S2MU107_REG_MONOUT, data) < 0)
		goto err;

	mutex_unlock(&fuelgauge->fg_lock);

	compliment = (data[1] << 8) | (data[0]);
	avg_vbat = (compliment * 1000) >> 12;

	dev_info(&fuelgauge->i2c->dev, "%s: avgvbat (%d)\n", __func__, avg_vbat);

	return avg_vbat;

err:
	mutex_unlock(&fuelgauge->fg_lock);
	return -EINVAL;
}

static void s2mu107_fuelalert_init(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 data;
	fuelgauge->is_fuel_alerted = false;

	mutex_lock(&fuelgauge->fg_lock);

	/* VBAT Threshold setting: 3.55V */
	data = (fuelgauge->pdata->fuel_alert_soc << 4) | 0x0F;
	s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, S2MU107_REG_ALERT_LVL, data);

	/* VBAT, SOC IRQ enable */
	s2mu107_update_reg_byte(fuelgauge->i2c, S2MU107_REG_IRQ_M, 0x00, 0x03);

	mutex_unlock(&fuelgauge->fg_lock);

	pr_info("%s: irq_reg(%02x) irq(%d)\n",
			__func__, data, fuelgauge->pdata->fg_irq);
}

static void s2mu107_fg_isr_work(struct work_struct *work)
{
	struct s2mu107_fuelgauge_data *fuelgauge =
		container_of(work, struct s2mu107_fuelgauge_data, isr_work.work);
	u8 fg_alert_status = 0;

	s2mu107_read_reg_byte(fuelgauge->i2c, S2MU107_REG_STATUS, &fg_alert_status);
	dev_info(&fuelgauge->i2c->dev, "%s : fg_alert_status(0x%x)\n",
		__func__, fg_alert_status);

	fg_alert_status &= 0x03;
	if (fg_alert_status & 0x01)
		pr_info("%s : Battery Level(SOC) is very Low!\n", __func__);

	if (fg_alert_status & 0x02) {
		int voltage = s2mu107_get_vbat(fuelgauge);

		pr_info("%s : Battery Votage is very Low! (%dmV)\n",
				__func__, voltage);
	}

	if (!fg_alert_status) {
		fuelgauge->is_fuel_alerted = false;
		pr_info("%s : SOC or Voltage is Good!\n", __func__);
		wake_unlock(&fuelgauge->fuel_alert_wake_lock);
	}
}

static irqreturn_t s2mu107_fg_irq_thread(int irq, void *irq_data)
{
	struct s2mu107_fuelgauge_data *fuelgauge = irq_data;
	u8 fg_irq = 0;

	s2mu107_read_reg_byte(fuelgauge->i2c, S2MU107_REG_IRQ, &fg_irq);
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

#if defined(CONFIG_BATTERY_AGE_FORECAST)
static int s2mu107_fg_aging_check(
		struct s2mu107_fuelgauge_data *fuelgauge, int step)
{
	u8 batcap0 = 0, batcap1 = 0, batcap2 = 0, batcap3 = 0;
	u8 por_state = 0;
	union power_supply_propval value;
	int charging_enabled = false;

	fuelgauge->fg_age_step = step;

	s2mu107_read_reg_byte(fuelgauge->i2c, S2MU107_REG_RBATCAP_OCV, &batcap0);
	s2mu107_read_reg_byte(fuelgauge->i2c, S2MU107_REG_RBATCAP_OCV + 1, &batcap1);
	s2mu107_read_reg_byte(fuelgauge->i2c, S2MU107_REG_RBATCAP, &batcap2);
	s2mu107_read_reg_byte(fuelgauge->i2c, S2MU107_REG_RBATCAP + 1, &batcap3);

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
		mutex_lock(&fuelgauge->fg_lock);

		fuelgauge->age_reset_status = 1;
		por_state |= 0x10;
		s2mu107_write_and_verify_reg_byte(fuelgauge->i2c,
			S2MU107_REG_START + 1, por_state);

		/* check charging enable */
		psy_do_property(fuelgauge->pdata->charger_name, get,
			POWER_SUPPLY_PROP_CHARGING_ENABLED, value);
		charging_enabled = value.intval;

		if (charging_enabled == true) {
			pr_info("%s: [Long life] disable charger for reset gauge age forecast \n",
				__func__);
			value.intval = SEC_BAT_CHG_MODE_CHARGING_OFF;
			psy_do_property(fuelgauge->pdata->charger_name, set,
				POWER_SUPPLY_PROP_CHARGING_ENABLED, value);
		}

		s2mu107_reset_fg(fuelgauge);

		if (charging_enabled == true) {
			psy_do_property("battery", get, POWER_SUPPLY_PROP_STATUS, value);
			charging_enabled = value.intval;

			if (charging_enabled == 1) { /* POWER_SUPPLY_STATUS_CHARGING 1 */
				pr_info("%s: [Long life] enable charger for reset gauge age forecast \n",
					__func__);
				value.intval = SEC_BAT_CHG_MODE_CHARGING;
				psy_do_property(fuelgauge->pdata->charger_name,
					set, POWER_SUPPLY_PROP_CHARGING_ENABLED, value);
			}
		}

		por_state &= ~0x10;
		s2mu107_write_and_verify_reg_byte(fuelgauge->i2c,
			S2MU107_REG_START + 1, por_state);
		fuelgauge->age_reset_status = 0;

		mutex_unlock(&fuelgauge->fg_lock);
		return 1;
	}
	return 0;
}
#endif

/* capacity is 0.1% unit */
static void s2mu107_fg_get_scaled_capacity(
		struct s2mu107_fuelgauge_data *fuelgauge,
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
static void s2mu107_fg_get_atomic_capacity(
        struct s2mu107_fuelgauge_data *fuelgauge,
        union power_supply_propval *val)
{
    if (fuelgauge->pdata->capacity_calculation_type &
            SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC) {
        if (fuelgauge->capacity_old < val->intval)
            val->intval = fuelgauge->ui_soc + 1;
        else if (fuelgauge->capacity_old > val->intval)
            val->intval = fuelgauge->ui_soc - 1;
    }

    /* keep SOC stable in abnormal status */
    if (fuelgauge->pdata->capacity_calculation_type &
            SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL) {
        if (!fuelgauge->is_charging &&
                fuelgauge->capacity_old < val->intval) {
            dev_err(&fuelgauge->i2c->dev,
                    "%s: capacity (old %d : new %d, ui_soc %d)\n",
                    __func__, fuelgauge->capacity_old, val->intval, fuelgauge->ui_soc);
            val->intval = fuelgauge->ui_soc;
        }
    }

    /* updated old capacity */
    fuelgauge->capacity_old = val->intval;
}

static int s2mu107_fg_check_capacity_max(
		struct s2mu107_fuelgauge_data *fuelgauge, int capacity_max)
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

static int s2mu107_fg_calculate_dynamic_scale(
		struct s2mu107_fuelgauge_data *fuelgauge, int capacity, bool scale_by_full)
{
	union power_supply_propval raw_soc_val;
	raw_soc_val.intval = s2mu107_get_soc(fuelgauge) / 10;

	if (raw_soc_val.intval <
		fuelgauge->pdata->capacity_max -
		fuelgauge->pdata->capacity_max_margin) {
		pr_info("%s: raw soc(%d) is very low, skip routine\n",
			__func__, raw_soc_val.intval);
	} else if (fuelgauge->capacity_max_conv) {
		pr_info("%s: skip dynamic scale routine\n", __func__);
	} else {
		fuelgauge->capacity_max =
			(raw_soc_val.intval * 100 / (capacity + 1));
		fuelgauge->capacity_old = capacity;

		fuelgauge->capacity_max =
			s2mu107_fg_check_capacity_max(fuelgauge,
			fuelgauge->capacity_max);

		pr_info("%s: %d is used for capacity_max, capacity(%d)\n",
				__func__, fuelgauge->capacity_max, capacity);
		if ((capacity == 100) && !fuelgauge->capacity_max_conv && scale_by_full) {
			fuelgauge->capacity_max_conv = true;
			fuelgauge->g_capacity_max = raw_soc_val.intval;
			pr_info("%s: Goal capacity max %d\n", __func__, fuelgauge->g_capacity_max);
		}
	}

	return fuelgauge->capacity_max;
}

static int s2mu107_fg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2mu107_fuelgauge_data *fuelgauge =
					power_supply_get_drvdata(psy);
	static struct timespec old_ts = {0, };
	struct timespec c_ts = {0, };

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		return -ENODATA;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = fuelgauge->pdata->capacity_full * fuelgauge->raw_capacity;
		break;
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
		val->intval = s2mu107_get_vbat(fuelgauge);
		break;
		/* Additional Voltage Information (mV) */
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		switch (val->intval) {
		case SEC_BATTERY_VOLTAGE_AVERAGE:
			val->intval = s2mu107_get_avgvbat(fuelgauge);
			break;
		case SEC_BATTERY_VOLTAGE_OCV:
			val->intval = s2mu107_get_ocv(fuelgauge);
			break;
		}
		break;
		/* Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = s2mu107_get_current(fuelgauge) * 1000;
		else
			val->intval = s2mu107_get_current(fuelgauge);
		break;
		/* Average Current (mA) */
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		if (val->intval == SEC_BATTERY_CURRENT_UA)
			val->intval = s2mu107_maintain_avgcurrent(fuelgauge) * 1000;
		else
			fuelgauge->current_avg = val->intval = s2mu107_maintain_avgcurrent(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW) {
			val->intval = s2mu107_get_soc(fuelgauge);
		} else if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_CAPACITY_POINT) {
			val->intval = fuelgauge->raw_capacity % 10;
		} else {
			val->intval = s2mu107_get_soc(fuelgauge) / 10;

			if (fuelgauge->pdata->capacity_calculation_type &
					(SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
					 SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)) {
				if (fuelgauge->capacity_max_conv) {
					c_ts = ktime_to_timespec(ktime_get_boottime());
					pr_info("%s : capacit max conv time(%ld)\n", __func__, c_ts.tv_sec - old_ts.tv_sec);

					if ((fuelgauge->capacity_max < fuelgauge->g_capacity_max) &&
							((unsigned long)(c_ts.tv_sec - old_ts.tv_sec) >= 60)) {
						fuelgauge->capacity_max++;
						old_ts = c_ts;
					} else if (fuelgauge->capacity_max >= fuelgauge->g_capacity_max) {
						fuelgauge->g_capacity_max = 0;
						fuelgauge->capacity_max_conv = false;
					}
					pr_info("%s : capacity_max_conv(%d) Capacity Max(%d | %d)\n",
							__func__, fuelgauge->capacity_max_conv,
							fuelgauge->capacity_max, fuelgauge->g_capacity_max);
				}
				s2mu107_fg_get_scaled_capacity(fuelgauge, val);

				if (val->intval > 1010) {	
					pr_info("%s : scaled capacity (%d)\n", __func__, val->intval);
					s2mu107_fg_calculate_dynamic_scale(fuelgauge, 100, false);
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
				s2mu107_fuelalert_init(fuelgauge);
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
				s2mu107_fg_get_atomic_capacity(fuelgauge, val);
		}
		break;
	/* Battery Temperature */
	case POWER_SUPPLY_PROP_TEMP:
	/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		val->intval = s2mu107_get_temperature(fuelgauge);
		break;
	case POWER_SUPPLY_PROP_ENERGY_FULL:
		fuelgauge->soh = s2mu107_get_soh(fuelgauge);
		val->intval = fuelgauge->soh / 100;
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
		fuelgauge->soh = s2mu107_get_soh(fuelgauge);
		val->intval = fuelgauge->soh;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mu107_fg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mu107_fuelgauge_data *fuelgauge =
				power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property)psp;
	u8 temp = 0;

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
#if defined(CONFIG_BATTERY_AGE_FORECAST)
			if (val->intval == POWER_SUPPLY_STATUS_FULL)
				s2mu107_fg_aging_check(fuelgauge, fuelgauge->change_step);
#endif
			break;
		case POWER_SUPPLY_PROP_CHARGE_FULL:
			if (fuelgauge->pdata->capacity_calculation_type &
					SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE) {
				s2mu107_fg_calculate_dynamic_scale(fuelgauge, val->intval, true);
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
				s2mu107_restart_gauging(fuelgauge);
				fuelgauge->initial_update_of_soc = true;
			}
			break;
		case POWER_SUPPLY_PROP_TEMP:
		case POWER_SUPPLY_PROP_TEMP_AMBIENT:
			s2mu107_set_temperature(fuelgauge, val->intval);
			fuelgauge->init_battery_temp = true;
			break;
		case POWER_SUPPLY_PROP_ENERGY_NOW:
			s2mu107_fg_reset_capacity_by_jig_connection(fuelgauge);
			break;
		case POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN:
			dev_info(&fuelgauge->i2c->dev,
				"%s: capacity_max changed, %d -> %d\n",
				__func__, fuelgauge->capacity_max, val->intval);
			fuelgauge->capacity_max = s2mu107_fg_check_capacity_max(fuelgauge, val->intval);
			fuelgauge->initial_update_of_soc = true;
			break;
		case POWER_SUPPLY_PROP_CHARGE_EMPTY:
			break;
		case POWER_SUPPLY_PROP_ENERGY_AVG:
			break;
		case POWER_SUPPLY_PROP_CURRENT_FULL:
			fuelgauge->topoff_current = val->intval;
			break;
#if defined(CONFIG_CHARGER_S2MU107_DIRECT)
		case POWER_SUPPLY_PROP_FAST_IAVG:
			fuelgauge->is_dc_charging = val->intval;
			s2mu107_set_tperiod(fuelgauge, fuelgauge->is_dc_charging);
			break;
#endif
		case POWER_SUPPLY_PROP_MAX ... POWER_SUPPLY_EXT_PROP_MAX:
			switch (ext_psp) {
			case POWER_SUPPLY_EXT_PROP_INBAT_VOLTAGE_FGSRC_SWITCHING:
				if ((val->intval == SEC_BAT_INBAT_FGSRC_SWITCHING_ON) ||
						(val->intval == SEC_BAT_FGSRC_SWITCHING_ON)) {
					/* Get Battery voltage (by I2C control) */
					s2mu107_update_reg_byte(fuelgauge->i2c, 0x25, 0x10, 0x30);
					msleep(1000);
					s2mu107_read_reg_byte(fuelgauge->i2c, 0x25, &temp);
					pr_info("%s: SW Vbat: fgsrc_switching_on: REG25:0x%02x, 0x25[5:4]=0x%x\n",
							__func__, temp, (temp & 0x30) >> 4);

					if (val->intval == SEC_BAT_INBAT_FGSRC_SWITCHING_ON)
						s2mu107_restart_gauging(fuelgauge);
					s2mu107_fg_reset_capacity_by_jig_connection(fuelgauge);
					s2mu107_fg_test_read(fuelgauge->i2c);
				} else if ((val->intval == SEC_BAT_INBAT_FGSRC_SWITCHING_OFF) ||
						(val->intval == SEC_BAT_FGSRC_SWITCHING_OFF)) {
					s2mu107_update_reg_byte(fuelgauge->i2c, 0x25, 0x30, 0x30);
					msleep(1000);
					s2mu107_read_reg_byte(fuelgauge->i2c, 0x25, &temp);
					pr_info("%s: SW Vsys: fgsrc_switching_off: REG25:0x%02x, 0x25[5:4]=0x%x\n",
							__func__, temp, (temp & 0x30) >> 4);

					if (val->intval == SEC_BAT_INBAT_FGSRC_SWITCHING_OFF)
						s2mu107_restart_gauging(fuelgauge);
					s2mu107_fg_reset_capacity_by_jig_connection(fuelgauge);
					s2mu107_fg_test_read(fuelgauge->i2c);
				}
				break;
			case POWER_SUPPLY_EXT_PROP_FUELGAUGE_FACTORY:
					pr_info("%s:[DEBUG_FAC] fuelgauge\n", __func__);
					s2mu107_update_reg_byte(fuelgauge->i2c, 0x25, 0x30, 0x30);
					s2mu107_fg_reset_capacity_by_jig_connection(fuelgauge);
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

static void s2mu107_init_regs(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 temp = 0;

	pr_info("%s: s2mu107 fuelgauge initialize\n", __func__);

	/* Save register values for surge check */
	s2mu107_read_reg_byte(fuelgauge->i2c, 0x53, &temp);
	fuelgauge->reg_OTP_53 = temp;
	s2mu107_read_reg_byte(fuelgauge->i2c, 0x52, &temp);
	fuelgauge->reg_OTP_52 = temp;

	/* Disable VM3_flag_EN */
	s2mu107_update_reg_byte(fuelgauge->i2c, S2MU107_REG_VM, 0x00, 0x04);

	s2mu107_update_reg_byte(fuelgauge->i2c, 0x0D, 0x00, ADC_AVCC_EN_MASK);

	s2mu107_update_reg_byte(fuelgauge->i2c, 0x29, 0x00, 0x01);
}

#if defined(CONFIG_FUELGAUGE_S2MU107_TEMP_COMPEN)
static void s2mu107_init_temp_compen(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 temp = 0;

	pr_info("%s: s2mu107 temperature compensation init\n", __func__);

	s2mu107_read_reg_byte(fuelgauge->i2c, 0x67, &temp);
	if (fuelgauge->pdata->inc_ok_en)
		temp = temp | TEMP_COMPEN_INC_OK_EN;
	else
		temp = temp & ~TEMP_COMPEN_INC_OK_EN;
	s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, 0x67, temp);

	s2mu107_write_enable(fuelgauge->i2c);
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, S2MU107_REG_RCOMPI,
		(u8)fuelgauge->pdata->comp_i);
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, S2MU107_REG_RA0,
		(u8)fuelgauge->pdata->a0);
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, S2MU107_REG_RB0,
		(u8)fuelgauge->pdata->b0);
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, S2MU107_REG_RA1,
		(u8)fuelgauge->pdata->a1);
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c, S2MU107_REG_RB1,
		(u8)fuelgauge->pdata->b1);
	s2mu107_write_disable(fuelgauge->i2c);
}
#endif

static void s2mu107_init_batcap_learn(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 temp = 0;

#if defined(CONFIG_FUELGAUGE_S2MU107_BATCAP_LRN)
	pr_info("%s: s2mu107 battery capacity learning init\n",
		__func__);

	s2mu107_read_reg_byte(fuelgauge->i2c, 0x67, &temp);
	if (fuelgauge->pdata->fast_lrn_en)
		temp = temp | BATCAP_LEARN_FAST_LRN_EN;
	else
		temp = temp & ~BATCAP_LEARN_FAST_LRN_EN;
	s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, 0x67, temp);

	s2mu107_read_reg_byte(fuelgauge->i2c, 0x6B, &temp);
	temp = temp |
		((fuelgauge->pdata->no4learn << 4) & BATCAP_LEARN_NO4LEARN_MASK);
	s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, 0x6B, temp);

	s2mu107_read_reg_byte(fuelgauge->i2c, 0x6D, &temp);
	if (fuelgauge->pdata->auto_lrn_en)
		temp = temp | BATCAP_LEARN_AUTO_LRN_EN;
	else
		temp = temp & ~BATCAP_LEARN_AUTO_LRN_EN;
	if (fuelgauge->pdata->wide_lrn_en)
		temp = temp | BATCAP_LEARN_WIDE_LRN_EN;
	else
		temp = temp & ~BATCAP_LEARN_WIDE_LRN_EN;
	if (fuelgauge->pdata->low_en)
		temp = temp | BATCAP_LEARN_LOW_EN;
	else
		temp = temp & ~BATCAP_LEARN_LOW_EN;
	s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, 0x6D, temp);

	temp = (fuelgauge->pdata->c1_num & BATCAP_LEARN_C1_NUM_MASK) |
		((fuelgauge->pdata->c2_num << 2) & BATCAP_LEARN_C2_NUM_MASK) |
		((fuelgauge->pdata->c1_curr << 4) & BATCAP_LEARN_C1_CURR_MASK);
	s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, 0x46, temp);
#else
	pr_info("%s: battery capacity learning is not enabled. Disable.\n",
		__func__);

	s2mu107_read_reg_byte(fuelgauge->i2c, 0x6D, &temp);
	temp = temp & ~BATCAP_LEARN_AUTO_LRN_EN;
	s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, 0x6D, temp);
#endif
}

#if defined(CONFIG_FUELGAUGE_S2MU107_USE_10MILLIOHM)
static void s2mu107_set_trim_10mohm(struct s2mu107_fuelgauge_data *fuelgauge)
{
	u8 temp_58, temp_59, temp_5a, temp_5b;
	u32 cslope = 0, coffset = 0;

	s2mu107_read_reg_byte(fuelgauge->i2c, 0x58, &temp_58);
	s2mu107_read_reg_byte(fuelgauge->i2c, 0x59, &temp_59);
	s2mu107_read_reg_byte(fuelgauge->i2c, 0x5A, &temp_5a);
	s2mu107_read_reg_byte(fuelgauge->i2c, 0x5B, &temp_5b);

	cslope = ((temp_5b & 0xF0) << 12) | (temp_59 << 8) | temp_58;
	coffset = ((temp_5b & 0x0F) << 8) | temp_5a;

	pr_info("%s: before cslope = 0x%x, coffset = 0x%x", __func__,
		cslope, coffset);

	cslope = (cslope ^ 0xFFFFF) + 1;
	cslope = cslope / 2;
	cslope = (cslope ^ 0xFFFFF) + 1;

	if (coffset & (1 << 11)) {
		coffset = (coffset ^ 0xFFF) + 1;
		coffset = coffset / 2;
		coffset = (coffset ^ 0xFFF) + 1;
	} else {
		coffset = coffset / 2;
	}

	s2mu107_write_enable(fuelgauge->i2c);

	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		0x58, (cslope & 0xFF));
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		0x59, (cslope & 0xFF00) >> 8);
	s2mu107_update_reg_byte_no_en(fuelgauge->i2c,
		0x5B, (cslope & 0xF0000) >> 12, 0xF0);

	s2mu107_update_reg_byte_no_en(fuelgauge->i2c,
		0x5B, (coffset & 0xF00) >> 8, 0x0F);
	s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
		0x5A, (coffset & 0xFF));

	s2mu107_write_disable(fuelgauge->i2c);

	/* Check written value */
	s2mu107_read_reg_byte(fuelgauge->i2c, 0x58, &temp_58);
	s2mu107_read_reg_byte(fuelgauge->i2c, 0x59, &temp_59);
	s2mu107_read_reg_byte(fuelgauge->i2c, 0x5A, &temp_5a);
	s2mu107_read_reg_byte(fuelgauge->i2c, 0x5B, &temp_5b);

	cslope = ((temp_5b & 0xF0) << 12) | (temp_59 << 8) | temp_58;
	coffset = ((temp_5b & 0x0F) << 8) | temp_5a;

	pr_info("%s: after cslope = 0x%x, coffset = 0x%x", __func__,
		cslope, coffset);
}
#endif

#if defined(CONFIG_CHARGER_S2MU107_DIRECT)
static void s2mu107_set_tperiod(struct s2mu107_fuelgauge_data *fuelgauge, bool is_dc_charging)
{
	if (is_dc_charging == true) {
		/* Set refresh period, 250ms -> 63ms */
		pr_info("%s: dc charging. Decrease Tperiod\n", __func__);
		s2mu107_update_reg_byte(fuelgauge->i2c, 0x45,
			T_PERIOD_63MS << T_PEROID_SHIFT, T_PERIOD_MASK);
	} else {
		pr_info("%s: Recover Tperiod\n", __func__);
		s2mu107_update_reg_byte(fuelgauge->i2c, 0x45,
			T_PERIOD_250MS << T_PEROID_SHIFT, T_PERIOD_MASK);
	}
}
static void s2mu107_init_for_direct_charge(struct s2mu107_fuelgauge_data *fuelgauge)
{
	pr_info("%s: s2mu107 fuelgauge init for direct charge\n",
			__func__);
	s2mu107_write_and_verify_reg_byte(fuelgauge->i2c,
		0x7A, 0xB3);
}
#endif

#ifdef CONFIG_OF
static int s2mu107_fuelgauge_parse_dt(struct s2mu107_fuelgauge_data *fuelgauge)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mu107-fuelgauge");
	int ret;
	const u32 *p;
	int len;
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	int i;
#endif
	/* reset, irq gpio info */
	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_string(np,
				"fuelgauge,charger_name",
				(char const **)&fuelgauge->pdata->charger_name);
		if (ret < 0) {
			fuelgauge->pdata->charger_name = "s2mu107-switching-charger";
			pr_info("%s: Charger name is Empty. Use default charger name %s\n",
					__func__, fuelgauge->pdata->charger_name);
		}

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

		ret = of_property_read_u32(np, "fuelgauge,low_vbat_threshold",
				&fuelgauge->low_vbat_threshold);
		if (ret < 0) {
			pr_err("%s There is no low vbat threshold. Use default(3450)\n",
					__func__);
			fuelgauge->low_vbat_threshold = 3450;
		}

		ret = of_property_read_u32(np, "fuelgauge,low_vbat_threshold_lowtemp",
				&fuelgauge->low_vbat_threshold_lowtemp);
		if (ret < 0) {
			pr_err("%s There is no low vbat threshold low temp. Use default(3450)\n",
					__func__);
			fuelgauge->low_vbat_threshold_lowtemp = 3450;
		}

		/* Get values for temperature compensation */
		ret = of_property_read_u32(np, "fuelgauge,inc_ok_en",
				&fuelgauge->pdata->inc_ok_en);
		if (ret < 0) {
			fuelgauge->pdata->inc_ok_en = 0;
			pr_err("%s There is no inc_ok_en. Use default value, %d\n",
					__func__, fuelgauge->pdata->inc_ok_en);
		}

		ret = of_property_read_u32(np, "fuelgauge,comp_i",
				&fuelgauge->pdata->comp_i);
		if (ret < 0) {
			fuelgauge->pdata->comp_i = 0x11;
			pr_err("%s There is no comp_i. Use default value, %d\n",
					__func__, fuelgauge->pdata->comp_i);
		}
		ret = of_property_read_u32(np, "fuelgauge,a0",
				&fuelgauge->pdata->a0);
		if (ret < 0) {
			fuelgauge->pdata->a0 = 0x40;
			pr_err("%s There is no a0. Use default value, %d\n",
					__func__, fuelgauge->pdata->a0);
		}
		ret = of_property_read_u32(np, "fuelgauge,b0",
				&fuelgauge->pdata->b0);
		if (ret < 0) {
			fuelgauge->pdata->b0 = 0x00;
			pr_err("%s There is no b0. Use default value, %d\n",
					__func__, fuelgauge->pdata->b0);
		}

		ret = of_property_read_u32(np, "fuelgauge,a1",
				&fuelgauge->pdata->a1);
		if (ret < 0) {
			fuelgauge->pdata->a1 = 0x7F;
			pr_err("%s There is no a1. Use default value, %d\n",
					__func__, fuelgauge->pdata->a1);
		}
		ret = of_property_read_u32(np, "fuelgauge,b1",
				&fuelgauge->pdata->b1);
		if (ret < 0) {
			fuelgauge->pdata->b1 = 0x00;
			pr_err("%s There is no b1. Use default value, %d\n",
					__func__, fuelgauge->pdata->b1);
		}

		/* Get values for battery capacity learning */
		ret = of_property_read_u32(np, "fuelgauge,fast_lrn_en",
				&fuelgauge->pdata->fast_lrn_en);
		if (ret < 0) {
			fuelgauge->pdata->fast_lrn_en = 0;
			pr_err("%s There is no fast_lrn_en. Use default value, %d\n",
					__func__, fuelgauge->pdata->fast_lrn_en);
		}

		ret = of_property_read_u32(np, "fuelgauge,no4learn",
				&fuelgauge->pdata->no4learn);
		if (ret < 0) {
			fuelgauge->pdata->no4learn = 0x01;
			pr_err("%s There is no no4learn. Use default value, %d\n",
					__func__, fuelgauge->pdata->no4learn);
		}

		ret = of_property_read_u32(np, "fuelgauge,auto_lrn_en",
				&fuelgauge->pdata->auto_lrn_en);
		if (ret < 0) {
			fuelgauge->pdata->auto_lrn_en = 1;
			pr_err("%s There is no auto_lrn_en. Use default value, %d\n",
					__func__, fuelgauge->pdata->auto_lrn_en);
		}

		ret = of_property_read_u32(np, "fuelgauge,wide_lrn_en",
				&fuelgauge->pdata->wide_lrn_en);
		if (ret < 0) {
			fuelgauge->pdata->wide_lrn_en = 0;
			pr_err("%s There is no wide_lrn_en. Use default value, %d\n",
					__func__, fuelgauge->pdata->wide_lrn_en);
		}

		ret = of_property_read_u32(np, "fuelgauge,low_en",
				&fuelgauge->pdata->low_en);
		if (ret < 0) {
			fuelgauge->pdata->low_en = 0;
			pr_err("%s There is no low_en. Use default value, %d\n",
					__func__, fuelgauge->pdata->low_en);
		}

		ret = of_property_read_u32(np, "fuelgauge,c1_num",
				&fuelgauge->pdata->c1_num);
		if (ret < 0) {
			fuelgauge->pdata->c1_num = 2;
			pr_err("%s There is no c1_num. Use default value, %d\n",
					__func__, fuelgauge->pdata->c1_num);
		}

		ret = of_property_read_u32(np, "fuelgauge,c2_num",
				&fuelgauge->pdata->c2_num);
		if (ret < 0) {
			fuelgauge->pdata->c2_num = 2;
			pr_err("%s There is no c2_num. Use default value, %d\n",
					__func__, fuelgauge->pdata->c2_num);
		}

		ret = of_property_read_u32(np, "fuelgauge,c1_curr",
				&fuelgauge->pdata->c1_curr);
		if (ret < 0) {
			fuelgauge->pdata->c1_curr = 0x0b;
			pr_err("%s There is no c1_curr. Use default value, %d\n",
					__func__, fuelgauge->pdata->c1_curr);
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

static struct of_device_id s2mu107_fuelgauge_match_table[] = {
	{ .compatible = "samsung,s2mu107-fuelgauge",},
	{},
};
#else
static int s2mu107_fuelgauge_parse_dt(struct s2mu107_fuelgauge_data *fuelgauge)
{
	return -ENOSYS;
}

#define s2mu107_fuelgauge_match_table NULL
#endif /* CONFIG_OF */

static const struct power_supply_desc s2mu107_fuelgauge_power_supply_desc = {
	.name = "s2mu107-fuelgauge",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = s2mu107_fuelgauge_props,
	.num_properties = ARRAY_SIZE(s2mu107_fuelgauge_props),
	.get_property = s2mu107_fg_get_property,
	.set_property = s2mu107_fg_set_property,
};

static int s2mu107_fuelgauge_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct s2mu107_fuelgauge_data *fuelgauge;
	union power_supply_propval raw_soc_val;
	struct power_supply_config fuelgauge_cfg = {};
	int ret = 0;
	u8 temp = 0;
#if 1
	u8 por_state = 0;
	u8 reg_1E = 0;
#endif
#if defined(CONFIG_FUELGAUGE_S2MU107_USE_10MILLIOHM)
	u8 temp_59, temp_5b;
#endif

	pr_info("%s: S2MU107 Fuelgauge Driver Loading\n", __func__);

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
		ret = s2mu107_fuelgauge_parse_dt(fuelgauge);
		if (ret < 0)
			goto err_parse_dt;
	} else {
		fuelgauge->pdata = client->dev.platform_data;
	}

	i2c_set_clientdata(client, fuelgauge);

	if (fuelgauge->pdata->fuelgauge_name == NULL)
		fuelgauge->pdata->fuelgauge_name = "s2mu107-fuelgauge";

	fuelgauge_cfg.drv_data = fuelgauge;

	/* FG Revision Value Setting */
	fuelgauge->revision = 0;
	s2mu107_read_reg_byte(fuelgauge->i2c, 0x48, &temp);
	fuelgauge->revision = (temp & 0xF0) >> 4;

	pr_info("%s: S2MU107 Fuelgauge revision: 0x%x, reg 0x48 = 0x%x\n",
			__func__, fuelgauge->revision, temp);

	fuelgauge->info.soc = 0;

	/* default CURRENT_MODE setting */
	fuelgauge->mode = CURRENT_MODE;
	s2mu107_write_and_verify_reg_byte(fuelgauge->i2c, 0x4A, 0x10);
	s2mu107_update_reg_byte(fuelgauge->i2c, 0x4B, 0x00, 0x70);

	/* capacity Max */
	fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;

#if 1
	/* TODO: Before bootloader bring-up,
	 * battery driver gets default SOC 56.12%.
	 * To avoid that, check por_state for init.
	 * This code need to be deleted after BL bring-up.
	 */
	s2mu107_read_reg_byte(fuelgauge->i2c, S2MU107_REG_START, &reg_1E);
	s2mu107_read_reg_byte(fuelgauge->i2c, S2MU107_REG_START + 1, &por_state);

#if defined(CONFIG_FUELGAUGE_S2MU107_USE_10MILLIOHM)
	s2mu107_read_reg_byte(fuelgauge->i2c, 0x59, &temp_59);
	s2mu107_read_reg_byte(fuelgauge->i2c, 0x5B, &temp_5b);
	if((por_state != 0x00) || (reg_1E != 0x03) ||
		((temp_59 == 0x20) && (temp_5b == 0xE0))) {
#else
	if((por_state != 0x00) || (reg_1E != 0x03)) {
#endif
		mutex_lock(&fuelgauge->fg_lock);

		s2mu107_write_enable(fuelgauge->i2c);
		s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
				S2MU107_REG_START + 1, 0x40);
		usleep_range(10000, 11000);

		s2mu107_write_enable(fuelgauge->i2c);
		s2mu107_write_and_verify_reg_byte_no_en(fuelgauge->i2c,
				S2MU107_REG_START + 1, 0x01);
		msleep(50);
		s2mu107_write_disable(fuelgauge->i2c);

		dev_info(&fuelgauge->i2c->dev, "%s: FG reset\n", __func__);
		/* If UI SOC is 0%, do not use raw SOC fix reset */
		if(fuelgauge->ui_soc == 0)
			s2mu107_reset_fg(fuelgauge);
		else
			s2mu107_fix_rawsoc_reset_fg(fuelgauge);
		por_state = 0x00;
		s2mu107_write_and_verify_reg_byte(fuelgauge->i2c,
				S2MU107_REG_START + 1, por_state);

#if defined(CONFIG_FUELGAUGE_S2MU107_USE_10MILLIOHM)
		/* TODO: s2mu107 uses 5mohm sensing resistor.
		 But 10mohm is used, need to adjust trim value */
		s2mu107_set_trim_10mohm(fuelgauge);
#endif

		mutex_unlock(&fuelgauge->fg_lock);
	}
#endif
	fuelgauge->g_capacity_max = 0;
	fuelgauge->capacity_max_conv = false;

	raw_soc_val.intval = s2mu107_get_soc(fuelgauge);
	raw_soc_val.intval = raw_soc_val.intval / 10;

	if (raw_soc_val.intval > fuelgauge->capacity_max)
		s2mu107_fg_calculate_dynamic_scale(fuelgauge, 100, true);

	s2mu107_init_regs(fuelgauge);
#if defined(CONFIG_FUELGAUGE_S2MU107_TEMP_COMPEN)
	s2mu107_init_temp_compen(fuelgauge);
#endif
	s2mu107_init_batcap_learn(fuelgauge);
#if defined(CONFIG_CHARGER_S2MU107_DIRECT)
	s2mu107_init_for_direct_charge(fuelgauge);
#endif

	fuelgauge->psy_fg = power_supply_register(
		&client->dev, &s2mu107_fuelgauge_power_supply_desc, &fuelgauge_cfg);
	if (!fuelgauge->psy_fg) {
		pr_err("%s: Failed to Register psy_fg\n", __func__);
		ret = PTR_ERR(fuelgauge->psy_fg);
		goto err_data_free;
	}

	fuelgauge->is_fuel_alerted = false;
	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		s2mu107_fuelalert_init(fuelgauge);
		wake_lock_init(&fuelgauge->fuel_alert_wake_lock,
					WAKE_LOCK_SUSPEND, "fuel_alerted");

		if (fuelgauge->pdata->fg_irq > 0) {
			INIT_DELAYED_WORK(&fuelgauge->isr_work, s2mu107_fg_isr_work);

			fuelgauge->fg_irq = gpio_to_irq(fuelgauge->pdata->fg_irq);
			dev_info(&client->dev, "%s : fg_irq = %d\n", __func__, fuelgauge->fg_irq);
			if (fuelgauge->fg_irq > 0) {
				ret = request_threaded_irq(fuelgauge->fg_irq,
						NULL, s2mu107_fg_irq_thread,
						IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT,
						"fuelgauge-irq", fuelgauge);
				if (ret) {
					dev_err(&client->dev, "%s: Failed to Request IRQ\n", __func__);
					goto err_supply_unreg;
				}

				ret = enable_irq_wake(fuelgauge->fg_irq);
				if (ret < 0)
					dev_err(&client->dev, "%s: Failed to Enable Wakeup Source(%d)\n",
							__func__, ret);
			} else {
				dev_err(&client->dev, "%s: Failed gpio_to_irq(%d)\n", __func__, fuelgauge->fg_irq);
				goto err_supply_unreg;
			}
		}
	}

	fuelgauge->cable_type = SEC_BATTERY_CABLE_NONE;
	fuelgauge->sleep_initial_update_of_soc = false;
	fuelgauge->initial_update_of_soc = true;
	fuelgauge->probe_done = true;
	fuelgauge->init_start = 1;

	pr_info("%s: S2MU107 Fuelgauge Driver Loaded\n", __func__);
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

static const struct i2c_device_id s2mu107_fuelgauge_id[] = {
	{"s2mu107-fuelgauge", 0},
	{}
};

static void s2mu107_fuelgauge_shutdown(struct i2c_client *client)
{
	struct s2mu107_fuelgauge_data *fuelgauge = i2c_get_clientdata(client);

	if (!fuelgauge->i2c) {
		pr_err("%s: no i2c client\n", __func__);
		return;
	}
}

static int s2mu107_fuelgauge_remove(struct i2c_client *client)
{
	struct s2mu107_fuelgauge_data *fuelgauge = i2c_get_clientdata(client);

	if (fuelgauge->pdata->fuel_alert_soc >= 0)
		wake_lock_destroy(&fuelgauge->fuel_alert_wake_lock);

	return 0;
}

#if defined CONFIG_PM
static int s2mu107_fuelgauge_suspend(struct device *dev)
{
	return 0;
}

static int s2mu107_fuelgauge_resume(struct device *dev)
{
	struct s2mu107_fuelgauge_data *fuelgauge = dev_get_drvdata(dev);

	fuelgauge->sleep_initial_update_of_soc = true;

	return 0;
}
#else
#define s2mu107_fuelgauge_suspend NULL
#define s2mu107_fuelgauge_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(s2mu107_fuelgauge_pm_ops, s2mu107_fuelgauge_suspend,
		s2mu107_fuelgauge_resume);

static struct i2c_driver s2mu107_fuelgauge_driver = {
	.driver = {
		.name = "s2mu107-fuelgauge",
		.owner = THIS_MODULE,
		.pm = &s2mu107_fuelgauge_pm_ops,
		.of_match_table = s2mu107_fuelgauge_match_table,
	},
	.probe = s2mu107_fuelgauge_probe,
	.remove = s2mu107_fuelgauge_remove,
	.shutdown = s2mu107_fuelgauge_shutdown,
	.id_table = s2mu107_fuelgauge_id,
};

static int __init s2mu107_fuelgauge_init(void)
{
	pr_info("%s: S2MU107 Fuelgauge Init\n", __func__);
	return i2c_add_driver(&s2mu107_fuelgauge_driver);
}

static void __exit s2mu107_fuelgauge_exit(void)
{
	i2c_del_driver(&s2mu107_fuelgauge_driver);
}
module_init(s2mu107_fuelgauge_init);
module_exit(s2mu107_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung S2MU107 Fuel Gauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
