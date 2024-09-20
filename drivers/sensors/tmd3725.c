/*
 * Copyright (c) 2010 SAMSUNG
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/uaccess.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/sensor/sensors_core.h>
#include "tmd3725.h"

#define MODULE_NAME_PROX                "proximity_sensor"
#define MODULE_NAME_LIGHT               "light_sensor"
#define VENDOR_NAME                     "TAOS"
#define CHIP_NAME                       "TMD3725"

#define DEFAULT_COEF_R                  (-220)  /* -0.22 */
#define DEFAULT_COEF_G                  110 /* 0.11 */
#define DEFAULT_COEF_B                  (-1120) /* -1.12 */
#define DEFAULT_COEF_C                  (1000) /* 1 */
#define DEFAULT_DGF                     831 /* 831.54 */
#define DEFAULT_CCT_COEF                4091 /* 4091 */
#define DEFAULT_CCT_OFFSET              227 /* 227 */
#define DEFAULT_LUX_MULTIPLE            10 /* lux x 1 */
#define DEFAULT_THRESHOLD_DET_HI        55 /* high threshold */
#define DEFAULT_THRESHOLD_STILL_DET_LOW 40 /* low threshold */
#define DEFAULT_THRESHOLD_STILL_DET_HI  250 /* 2nd high threshold */
#define DEFAULT_THRESHOLD_REL_LOW       130 /* 2nd low threshold */
#define DEFAULT_ATIME                   0x11 /* 50.4 msec */
#define DEFAULT_PRATE                   0x38 /* 5ms */
#define DEFAULT_WTIME                   0x00 /* 2.8 msec */
#ifdef CONFIG_SENSORS_TMD3725_RF_NOISE_DEFENCE_CODE
#define DEFAULT_WTIME_FOR_PROX_ONLY     0x12 /* 53.2 msec */
#endif
#define DEFAULT_PPULSE                  0x96 /* ppulse_len: 16us, ppluse: 23 */
#define DEFAULT_PGCFG1                  0x08 /* pagin 1x, pgldrive 54mA */
#define DEFAULT_AGAIN                   0x02

#define TMD3725_CHIP_ID                 0xE4
#define LIGHT_LOG_TIME                  30 /* 200m X 30 */
#define PROX_WAKE_LOCK_TIME             (3 * HZ) /* 3 Sec */
#define PROX_AVG_COUNT                  40
#define PROX_MAX_DATA                   0xff
#define PROX_MIN_DATA                   0
#define PROX_DETECT_OFFSET              250
#define PROX_COMPENSATION_OFFSET        40

#define ATIME_INTERVAL                  28 /* 2.81ms */
#define LIMIT_GAIN_1_CLEAR_DATA         25
#define LIMIT_GAIN_16_CLEAR_DATA        15000
#define LIMIT_MAX_CLEAR_DATA            18500
#define LIGHT_MAX_LUX                   150000

#define AZ_CONFIG_SET                   0x7f /* one-shot autozero */
#define PERSIST_TIME_SET                0x30
#define BINARY_SEARCH_SET               (0x03 << 5)
#define CALIBRATION_SET                 0x01
#define DEFAULT_LIGHT_POLL_DELAY        100 /* 100 ms */
#define DEFAULT_PROX_AVG_POLL_DELAY     (2000 * NSEC_PER_MSEC) /* 2 sec */

/* driver data */
struct tmd3725_data {
	struct i2c_client *i2c_client;
	struct input_dev *light_input_dev;
	struct input_dev *prox_input_dev;
	struct device *light_dev;
	struct device *prox_dev;
	struct work_struct work_prox;
	struct work_struct work_prox_avg;
	struct mutex mode_lock;
	struct mutex prox_mutex;
	struct mutex enable_lock;
	struct wake_lock prx_wake_lock;
	struct hrtimer prox_avg_timer;
	struct delayed_work work_light;
	struct workqueue_struct *wq;
	struct workqueue_struct *prox_avg_wq;
	struct regulator *prox_vled;
	int64_t light_poll_delay;
	ktime_t prox_avg_poll_delay;

	u8 power_state;
	u16 op_mode_state;
	s32 clear;
	s32 red;
	s32 green;
	s32 blue;
	int lux;
	int count_log_time;
	int dgf;
	int cct_coef;
	int cct_offset;
	int coef_r;
	int coef_g;
	int coef_b;
	int coef_c;
	int als_time;
	int prate;
	int wtime;
	int ppulse;
	int pgcfg1;
	int als_gain;
	int lux_mul;

	int prox_irq;
	int prox_avg[3];
	int prox_avg_enable;
	int prox_offset;
	int prox_level_state;
	int prox_irq_gpio;
	int prox_thd_det_hi;
	int prox_thd_still_det_low;
	int prox_thd_still_det_hi;
	int prox_thd_rel_low;
	int prox_vled_ldo_pin;
	bool prox_cal_complete;
};

static int tmd3725_prox_vled_onoff(struct tmd3725_data *taos, int onoff)
{
	int err;

	SENSOR_INFO("%s, ldo:%d\n", (onoff) ? "on" : "off",
		taos->prox_vled_ldo_pin);

	/* ldo control */
	if (taos->prox_vled_ldo_pin) {
		gpio_set_value(taos->prox_vled_ldo_pin, onoff);
		if (onoff)
			msleep(20);
		return 0;
	}

	/* regulator(PMIC) control */
	if (!taos->prox_vled) {
		SENSOR_INFO("VLED get regulator\n");
		taos->prox_vled =
			regulator_get(&taos->i2c_client->dev, "taos,vled");
		if (IS_ERR(taos->prox_vled)) {
			SENSOR_ERR("regulator_get fail\n");
			taos->prox_vled = NULL;
			return -ENODEV;
		}
	}

	if (onoff) {
		if (regulator_is_enabled(taos->prox_vled)) {
			SENSOR_INFO("Regulator already enabled\n");
			return 0;
		}

		err = regulator_enable(taos->prox_vled);
		if (err)
			SENSOR_ERR("Failed to enable vled.\n");

		usleep_range(10000, 11000);
	} else {
		err = regulator_disable(taos->prox_vled);
		if (err)
			SENSOR_ERR("Failed to disable vled.\n");
	}

	return 0;
}

static int tmd3725_i2c_read_data(struct tmd3725_data *taos, u8 reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(taos->i2c_client, reg);
	if (ret < 0)
		SENSOR_ERR("failed %d\n", ret);

	return ret;
}

static int tmd3725_i2c_write_data(struct tmd3725_data *taos, u8 reg, u8 val)
{
	int ret;

	ret = i2c_smbus_write_byte_data(taos->i2c_client,
		(CMD_REG | reg), val);
	if (ret < 0)
		SENSOR_ERR("failed %d\n", ret);

	return ret;
}

static int tmd3725_i2c_modify_write(struct tmd3725_data *taos,
	u8 reg, u8 mask, u8 val)
{
	int ret;

	ret = i2c_smbus_read_byte_data(taos->i2c_client, reg);
	if (ret < 0) {
		SENSOR_ERR("read failed %d\n", ret);
		return ret;
	}

	ret = (ret & (~mask)) | val;
	ret = i2c_smbus_write_byte_data(taos->i2c_client,
		(CMD_REG | reg), ret);
	if (ret < 0)
		SENSOR_ERR("write failed %d\n", ret);

	return ret;
}

static void tmd3725_clear_interrupt(struct tmd3725_data *taos)
{
	int ret;

	SENSOR_INFO("called\n");

	ret = tmd3725_i2c_read_data(taos, STATUS);
	if (ret < 0)
		SENSOR_ERR("failed %d\n", ret);
}

static int tmd3725_light_get_cct(struct tmd3725_data *taos)
{
	int cct = taos->cct_coef;

	if (taos->red != 0)
		cct = (cct * taos->blue) / taos->red;

	cct += taos->cct_offset;
	return cct;
}

static int tmd3725_light_get_lux(struct tmd3725_data *taos)
{
	u8 reg_gain;
	s32 rp1, gp1, bp1, cp1;
	s32 calculated_lux;
	int gain;
	int ret;

	ret = i2c_smbus_read_word_data(taos->i2c_client, CFG1);
	if (ret < 0) {
		SENSOR_ERR("failed %d\n", ret);
		return taos->lux;
	}
	reg_gain = (u16)ret & 0xff;

	taos->clear = i2c_smbus_read_word_data(taos->i2c_client, CLR_CHAN0LO);
	taos->red = i2c_smbus_read_word_data(taos->i2c_client, RED_CHAN1LO);
	taos->green = i2c_smbus_read_word_data(taos->i2c_client, GRN_CHAN1LO);
	taos->blue = i2c_smbus_read_word_data(taos->i2c_client, BLU_CHAN1LO);

	if ((taos->clear < 0) || (taos->red < 0) ||
		(taos->green < 0) || (taos->blue < 0)) {
		SENSOR_ERR("rgb read failed\n");

		return taos->lux;
	}

	switch (reg_gain & 0x03) {
	case 0x00:
		gain = 1;
		break;
	case 0x01:
		gain = 4;
		break;
	case 0x02:
		gain = 16;
		break;
/*	case 0x03:
		gain = 64;
		break;
*/
	default:
		gain = 1;
		break;
	}

	if (gain == 1 && taos->clear < LIMIT_GAIN_1_CLEAR_DATA) {
		reg_gain = 0x02; /* Gain 16x */

		ret = tmd3725_i2c_write_data(taos, CFG1, reg_gain);
		if (ret < 0)
			SENSOR_ERR("failed %d\n", ret);

		return -1;
	} else if (gain == 16 && taos->clear > LIMIT_GAIN_16_CLEAR_DATA) {
		reg_gain = 0x00; /* Gain 1x */

		ret = tmd3725_i2c_write_data(taos, CFG1, reg_gain);
		if (ret < 0)
			SENSOR_ERR("failed %d\n", ret);

		return -1;
	}

	if ((taos->clear >= LIMIT_MAX_CLEAR_DATA) && (gain == 1))
		return LIGHT_MAX_LUX;

	/* calculate lux */
	rp1 = taos->red * taos->coef_r;
	gp1 = taos->green * taos->coef_g;
	bp1 = taos->blue * taos->coef_b;
	cp1 = taos->clear * taos->coef_c;

	calculated_lux = (rp1 + gp1 + bp1 + cp1) / 1000;
	if (calculated_lux < 0)
		calculated_lux = 0;
	else {
		/* divide by CPL, CPL = (ATIME_MS * ALS_GAIN / DGF); */
		calculated_lux = calculated_lux * taos->dgf;
		calculated_lux *= taos->lux_mul;
		calculated_lux /= ((taos->als_time + 1) * ATIME_INTERVAL);
		calculated_lux /= gain;
	}

	taos->lux = (int)calculated_lux;
	taos->als_gain = gain;

	return taos->lux;
}

static int tmd3725_initialize_chip(struct tmd3725_data *taos)
{
	int ret = 0;

	ret = tmd3725_i2c_write_data(taos, CFG3, INT_READ_CLEAR);
	if (ret < 0)
		SENSOR_ERR("failed to write cfg3 reg %d\n", ret);

	ret = tmd3725_i2c_write_data(taos, AZ_CONFIG, AZ_CONFIG_SET);
	if (ret < 0)
		SENSOR_ERR("failed to write auto zero cfg reg %d\n", ret);

	ret = tmd3725_i2c_write_data(taos, CFG1, taos->als_gain);
	if (ret < 0)
		SENSOR_ERR("failed to write als gain ctrl reg %d\n", ret);

	ret = tmd3725_i2c_write_data(taos, ALS_TIME, taos->als_time);
	if (ret < 0)
		SENSOR_ERR("failed to write als time reg %d\n", ret);

	ret = tmd3725_i2c_write_data(taos, PPERS, PERSIST_TIME_SET);
	if (ret < 0)
		SENSOR_ERR("failed to write proximity persistence %d\n", ret);

	ret = tmd3725_i2c_write_data(taos, PGCFG0, taos->ppulse);
	if (ret < 0)
		SENSOR_ERR("failed to write proximity pulse %d\n", ret);

	ret = tmd3725_i2c_write_data(taos, PRX_RATE, taos->prate);
	if (ret < 0)
		SENSOR_ERR("failed to write proximity sample rate %d\n", ret);

	ret = tmd3725_i2c_write_data(taos, WAIT_TIME, taos->wtime);
	if (ret < 0)
		SENSOR_ERR("failed to write als wait time reg %d\n", ret);

	ret = tmd3725_i2c_write_data(taos, PGCFG1, taos->pgcfg1);
	if (ret < 0)
		SENSOR_ERR("failed to write prox pulse reg %d\n", ret);

	ret = tmd3725_i2c_modify_write(taos, CALIBCFG,
		AUTO_OFFSET_ADJ, AUTO_OFFSET_ADJ);
	if (ret < 0)
		SENSOR_ERR("failed to write enable state reg %d\n", ret);

	return ret;
}

static int tmd3725_set_op_mode(struct tmd3725_data *taos, u16 op_mode, u8 state)
{
	u8 intenab_val, enable_val;
	int ret = -1;

	mutex_lock(&taos->mode_lock);

	if (state)
		taos->op_mode_state |= (op_mode << 0);
	else
		taos->op_mode_state &= ~(op_mode << 0);

	if (op_mode == MODE_PROX)
		tmd3725_clear_interrupt(taos);

	switch (taos->op_mode_state) {
	case MODE_OFF:
		intenab_val = CNTL_REG_CLEAR;
		enable_val = CNTL_PWRON;
		break;
	case MODE_ALS:
		intenab_val = CNTL_REG_CLEAR;
		enable_val = PON | AEN;
		break;
	case MODE_PROX:
		if (taos->prox_cal_complete == false) {
			SENSOR_INFO("returned in MODE_PROX\n");
			mutex_unlock(&taos->mode_lock);
			return 0;
		}
		intenab_val = PIEN | ZIEN;
		enable_val = PEN | PON | WEN;
		break;
	case MODE_ALS_PROX:
		if (taos->prox_cal_complete == false) {
			SENSOR_INFO("returned in MODE_ALS_PROX\n");
			mutex_unlock(&taos->mode_lock);
			return 0;
		}
		intenab_val = PIEN | ZIEN;
		enable_val = PEN | PON | AEN | WEN;
		break;
	default:
		mutex_unlock(&taos->mode_lock);
		return ret;
	}

	ret = tmd3725_i2c_write_data(taos, INTENAB, intenab_val);
	if (ret < 0) {
		SENSOR_ERR("INTENAB failed %d\n", ret);
		mutex_unlock(&taos->mode_lock);
		return ret;
	}
	ret = tmd3725_i2c_write_data(taos, CMD_REG, enable_val);
	if (ret < 0) {
		SENSOR_ERR("CMD_REG failed %d\n", ret);
		mutex_unlock(&taos->mode_lock);
		return ret;
	}

	mutex_unlock(&taos->mode_lock);
	return 0;
}

static void tmd3725_light_enable(struct tmd3725_data *taos)
{
	SENSOR_INFO("start poll timer\n");
	taos->count_log_time = LIGHT_LOG_TIME;
	schedule_delayed_work(&taos->work_light,
		msecs_to_jiffies(DEFAULT_LIGHT_POLL_DELAY));
}

static void tmd3725_light_disable(struct tmd3725_data *taos)
{
	SENSOR_INFO("cancelling poll timer\n");
	cancel_delayed_work_sync(&taos->work_light);
}

static void tmd3725_work_func_light(struct work_struct *work)
{
	struct tmd3725_data *taos = container_of((struct delayed_work *)work,
			struct tmd3725_data, work_light);

	int lux = tmd3725_light_get_lux(taos);
	int cct = tmd3725_light_get_cct(taos);

	if (lux < 0) {
		schedule_delayed_work(&taos->work_light,
			msecs_to_jiffies(DEFAULT_LIGHT_POLL_DELAY));
		return;
	}

	if (taos->count_log_time >= LIGHT_LOG_TIME) {
		SENSOR_INFO("R:%d G:%d B:%d C:%d lux:%d again:%d\n",
			taos->red, taos->green, taos->blue,
			taos->clear, lux, taos->als_gain);
		taos->count_log_time = 0;
	} else {
		taos->count_log_time++;
	}

	input_report_rel(taos->light_input_dev, REL_MISC, lux + 1);
	input_report_rel(taos->light_input_dev, REL_WHEEL, cct);
	input_sync(taos->light_input_dev);

	schedule_delayed_work(&taos->work_light,
		msecs_to_jiffies(taos->light_poll_delay));
}

static int tmd3725_prox_get_adc(struct tmd3725_data *taos)
{
	int adc;

	adc = tmd3725_i2c_read_data(taos, PRX_DATA_HIGH);
	if (adc < 0)
		return PROX_MIN_DATA;
	else if (adc > PROX_MAX_DATA)
		return PROX_MAX_DATA;

	return adc;
}

static int tmd3725_prox_get_threshold(struct tmd3725_data *taos, u8 buf)
{
	int threshold;

	threshold = tmd3725_i2c_read_data(taos, buf);
	if (threshold < 0)
		SENSOR_ERR("failed %d\n", threshold);

	return threshold;
}

static void tmd3725_prox_set_threshold(struct tmd3725_data *taos)
{
	u8 prox_int_thresh[2];
	int ret;

	switch (taos->prox_level_state) {
	case STATE_INIT:
		prox_int_thresh[0] = PROX_MAX_DATA;
		prox_int_thresh[1] = PROX_MIN_DATA;
		break;
	case STATE_DETECTION:
		prox_int_thresh[0] = PROX_MIN_DATA;
		prox_int_thresh[1] = taos->prox_thd_det_hi;
		break;
	case STATE_STILL_DETECTION:
		prox_int_thresh[0] = taos->prox_thd_still_det_low;
		prox_int_thresh[1] = taos->prox_thd_still_det_hi;
		break;
	case STATE_RELEASE:
		prox_int_thresh[0] = taos->prox_thd_rel_low;
		prox_int_thresh[1] = PROX_MAX_DATA;
		break;
	case STATE_HIGH_OFFSET:
		prox_int_thresh[0] = taos->prox_thd_still_det_low;
		prox_int_thresh[1] = PROX_MAX_DATA;
		break;
	default:
		SENSOR_ERR("Unknown state err = %d\n", taos->prox_level_state);
		return;
	}

	ret = tmd3725_i2c_write_data(taos, PRX_MINTHRESH, prox_int_thresh[0]);
	if (ret < 0)
		SENSOR_ERR("failed %d\n", ret);

	ret = tmd3725_i2c_write_data(taos, PRX_MAXTHRESH, prox_int_thresh[1]);
	if (ret < 0)
		SENSOR_ERR("failed %d\n", ret);
}

static int tmd3725_prox_get_offset(struct tmd3725_data *taos)
{
	int ret;
	u8 offset_l;
	u8 offset_h;

	ret = tmd3725_i2c_read_data(taos, POFFSET_L);
	if (ret < 0) {
		SENSOR_ERR("POFFSET_L failed, err = %d\n", ret);
		return ret;
	}

	offset_l = (u8)ret;
	ret = tmd3725_i2c_read_data(taos, POFFSET_H);
	if (ret < 0) {
		SENSOR_ERR("POFFSET_H failed, err = %d\n", ret);
		return ret;
	}

	offset_h = (u8)ret;
	if (offset_h == 0x01)
		taos->prox_offset = offset_l * (-1);
	else
		taos->prox_offset = offset_l;

	return taos->prox_offset;
}

static void tmd3725_prox_set_offset(struct tmd3725_data *taos, u8 offset)
{
	int ret;

	ret = tmd3725_i2c_write_data(taos, POFFSET_L, offset);
	if (ret < 0) {
		SENSOR_ERR("POFFSET_L failed, err = %d\n", ret);
		return;
	}

	ret  = tmd3725_i2c_write_data(taos, POFFSET_H, PROX_MIN_DATA);
	if (ret < 0) {
		SENSOR_ERR("POFFSET_H failed, err = %d\n", ret);
		return;
	}

	taos->prox_offset = offset;
}

static void tmd3725_prox_initialize_target(struct tmd3725_data *taos)
{
	int ret;

	mutex_lock(&taos->mode_lock);

	SENSOR_INFO("Calibration Start !!!\n");

	ret = tmd3725_i2c_write_data(taos, ENABLE, PON);
	if (ret < 0)
		SENSOR_ERR("ENABLE failed %d\n", ret);

	ret = tmd3725_i2c_write_data(taos, INTENAB, CIEN);
	if (ret < 0)
		SENSOR_ERR("INTENAB failed %d\n", ret);

	/* < BINARY SEARCH TARGET > */
	/*
		value   :   TARGET
		0       :      0
		1       :      1
		2       :      3
		3       :      7
		4       :      15
		5       :      31
		6       :      63
		7       :      127
	*/
	ret = tmd3725_i2c_modify_write(taos, CALIBCFG,
			BINSRCH_TARGET, BINARY_SEARCH_SET);
	if (ret < 0)
		SENSOR_ERR("CALIBCFG failed %d\n", ret);

	ret = tmd3725_i2c_write_data(taos, CALIB , CALIBRATION_SET);
	if (ret < 0)
		SENSOR_ERR("CALIB failed %d\n", ret);

	taos->prox_cal_complete = false;
	taos->op_mode_state |= (MODE_PROX << 0);

	mutex_unlock(&taos->mode_lock);
}

static void tmd3725_prox_send_event(struct tmd3725_data *taos, int val)
{
	input_report_abs(taos->prox_input_dev, ABS_DISTANCE, val);
	input_sync(taos->prox_input_dev);

	SENSOR_INFO("prox value = %d\n", val);
}

#ifdef CONFIG_SENSORS_TMD3725_RF_NOISE_DEFENCE_CODE
static void tmd3725_change_wtime(struct tmd3725_data *taos, int val)
{
	int ret;

	taos->wtime = val;

	ret = tmd3725_i2c_write_data(taos, WAIT_TIME, taos->wtime);
	if (ret < 0)
		SENSOR_ERR("failed to write als wait time reg %d\n", ret);
	else
		SENSOR_INFO("change wait time reg %d\n", taos->wtime);
}
#endif

static void tmd3725_prox_process_state(struct tmd3725_data *taos)
{
	int adc_data;
	int thresh_hi;
	int thresh_low;
	int prox_state;

	mutex_lock(&taos->prox_mutex);
	adc_data = tmd3725_prox_get_adc(taos);
	mutex_unlock(&taos->prox_mutex);

	thresh_hi = tmd3725_prox_get_threshold(taos, PRX_MAXTHRESH);
	thresh_low = tmd3725_prox_get_threshold(taos, PRX_MINTHRESH);
	SENSOR_INFO("hi = %d, low = %d, adc_data = %d\n",
		thresh_hi, thresh_low, adc_data);

	if ((taos->prox_level_state < STATE_INIT) ||
		((taos->prox_level_state > STATE_RELEASE))) {
		SENSOR_ERR("Unavailable STATE\n");
		return;
	}

	switch (taos->prox_level_state) {
	case STATE_INIT:
		tmd3725_prox_get_offset(taos);
		SENSOR_INFO("STATE_INIT - offset: %d\n", taos->prox_offset);

		if (taos->prox_offset > PROX_DETECT_OFFSET) {
			tmd3725_prox_set_offset(taos,
				taos->prox_offset - PROX_COMPENSATION_OFFSET);
			taos->prox_level_state = STATE_RELEASE;
			prox_state = PROX_CLOSE;
		} else {
			if (adc_data >= taos->prox_thd_still_det_hi) {
				taos->prox_level_state = STATE_RELEASE;
				prox_state = PROX_CLOSE;
			} else if (adc_data >= taos->prox_thd_det_hi) {
				taos->prox_level_state = STATE_STILL_DETECTION;
				prox_state = PROX_CLOSE;
			} else {
				taos->prox_level_state = STATE_DETECTION;
				prox_state = PROX_FAR;
			}
		}
		input_report_abs(taos->prox_input_dev,
			ABS_DISTANCE, !prox_state);
		tmd3725_prox_send_event(taos, prox_state);

		break;
	case STATE_DETECTION:
		SENSOR_INFO("STATE_DETECTION\n");
		taos->prox_level_state = STATE_STILL_DETECTION;
		tmd3725_prox_send_event(taos, PROX_CLOSE);
		break;
	case STATE_STILL_DETECTION:
		SENSOR_INFO("STATE_STILL_DETECTION\n");
		if (adc_data >= taos->prox_thd_still_det_hi) {
			taos->prox_level_state = STATE_RELEASE;
		} else if (adc_data <= taos->prox_thd_still_det_low) {
			taos->prox_level_state = STATE_DETECTION;
			tmd3725_prox_send_event(taos, PROX_FAR);
		}
		break;
	case STATE_RELEASE:
		SENSOR_INFO("STATE_RELEASE\n");
		tmd3725_prox_initialize_target(taos);
		taos->prox_level_state = STATE_DETECTION;
		tmd3725_prox_send_event(taos, PROX_FAR);
		break;
	case STATE_HIGH_OFFSET:
		SENSOR_INFO("STATE_HIGH_OFFSET\n");
		break;
	default:
		SENSOR_INFO("NONE State or Unknown state\n");
		break;
	}

	tmd3725_prox_set_threshold(taos);
}

static void tmd3725_work_func_prox_avg(struct work_struct *work)
{
	struct tmd3725_data *taos = container_of(work, struct tmd3725_data,
		work_prox_avg);
	int prox_level_state;
	int min = 0, max = 0, avg = 0;
	int i;

	for (i = 0; i < PROX_AVG_COUNT; i++) {
		mutex_lock(&taos->prox_mutex);
		prox_level_state = tmd3725_prox_get_adc(taos);
		mutex_unlock(&taos->prox_mutex);
		if (prox_level_state > PROX_MIN_DATA) {
			avg += prox_level_state;
			if (i == 0)
				min = prox_level_state;
			if (prox_level_state < min)
				min = prox_level_state;
			if (prox_level_state > max)
				max = prox_level_state;
		} else {
			prox_level_state = PROX_MIN_DATA;
		}
		msleep(40);
	}
	avg /= i;
	taos->prox_avg[0] = min;
	taos->prox_avg[1] = avg;
	taos->prox_avg[2] = max;
}

static void tmd3725_work_func_prox(struct work_struct *work)
{
	u8 status;
	int ret;
	struct tmd3725_data *taos =
		container_of(work, struct tmd3725_data, work_prox);

	/* disable INT */
	disable_irq_nosync(taos->prox_irq);

	/* Calibration Handle Event */
	ret = tmd3725_i2c_read_data(taos, STATUS);
	if (ret < 0)
		SENSOR_ERR("STATUS failed %d\n", ret);

	tmd3725_clear_interrupt(taos);
	enable_irq(taos->prox_irq);

	status = (u8)ret;
	if (status & CINT) {
		taos->prox_cal_complete = true;
		ret = tmd3725_i2c_modify_write(taos,
			CALIBSTAT, CALIB_FINISHED, CALIB_FINISHED);
		if (ret < 0)
			SENSOR_ERR("CALIBSTAT failed %d\n", ret);

		tmd3725_set_op_mode(taos, MODE_PROX, ON);
		tmd3725_prox_get_offset(taos);
		SENSOR_INFO("CINT - status: 0x%x, offset: %d\n",
			status, taos->prox_offset);
		return;
	}

	if (status & ZINT) {
		tmd3725_prox_get_offset(taos);
		SENSOR_INFO("ZINT - status: 0x%x, offset: %d\n",
			status, taos->prox_offset);
		tmd3725_prox_initialize_target(taos);
		return;
	}

	if (status & PINT) {
		if (taos->prox_cal_complete == true)
			tmd3725_prox_process_state(taos);
		else
			SENSOR_INFO("prox cal is not completed!!\n");
	}
}

irqreturn_t tmd3725_prox_irq_handler(int irq, void *data)
{
	struct tmd3725_data *taos = data;

	if (taos->prox_irq != -1) {
		wake_lock_timeout(&taos->prx_wake_lock, PROX_WAKE_LOCK_TIME);
		queue_work(taos->wq, &taos->work_prox);
	}

	SENSOR_INFO("taos interrupt handler is called\n");
	return IRQ_HANDLED;
}

static enum hrtimer_restart tmd3725_prox_avg_timer_func(struct hrtimer *timer)
{
	struct tmd3725_data *taos = container_of(timer, struct tmd3725_data,
					prox_avg_timer);
	queue_work(taos->prox_avg_wq, &taos->work_prox_avg);
	hrtimer_forward_now(&taos->prox_avg_timer, taos->prox_avg_poll_delay);

	return HRTIMER_RESTART;
}

static ssize_t tmd3725_light_poll_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%lld\n", taos->light_poll_delay);
}

static ssize_t tmd3725_light_poll_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);
	int64_t new_delay;
	int err;

	err = kstrtoll(buf, 10, &new_delay);
	if (err < 0) {
		SENSOR_ERR("invalid value %d\n", err);
		return size;
	}

	SENSOR_INFO("new delay = %lldns, old delay = %lldms\n",
		new_delay, taos->light_poll_delay);

	mutex_lock(&taos->enable_lock);
	if ((new_delay / NSEC_PER_MSEC) != (taos->light_poll_delay)) {
		taos->light_poll_delay = new_delay / NSEC_PER_MSEC;
		if (taos->power_state & LIGHT_ENABLED) {
			tmd3725_light_disable(taos);
			tmd3725_light_enable(taos);
		}
	}

	mutex_unlock(&taos->enable_lock);

	return size;
}

static ssize_t tmd3725_light_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		(taos->power_state & LIGHT_ENABLED) ? 1 : 0);
}

static ssize_t tmd3725_light_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);
	bool new_value;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		SENSOR_ERR("invalid value\n");
		return size;
	}

	SENSOR_INFO("new_value = %d, old state = %d\n",
		new_value, (taos->power_state & LIGHT_ENABLED) ? 1 : 0);

	mutex_lock(&taos->enable_lock);
	if (new_value && !(taos->power_state & LIGHT_ENABLED)) {
		taos->power_state |= LIGHT_ENABLED;
#ifdef CONFIG_SENSORS_TMD3725_RF_NOISE_DEFENCE_CODE
		tmd3725_change_wtime(taos, DEFAULT_WTIME);
#endif
		tmd3725_set_op_mode(taos, MODE_ALS, ON);
		tmd3725_light_enable(taos);
	} else if (!new_value && (taos->power_state & LIGHT_ENABLED)) {
		tmd3725_light_disable(taos);
		tmd3725_set_op_mode(taos, MODE_ALS, OFF);
#ifdef CONFIG_SENSORS_TMD3725_RF_NOISE_DEFENCE_CODE
		tmd3725_change_wtime(taos, DEFAULT_WTIME_FOR_PROX_ONLY);
#endif
		taos->power_state &= ~LIGHT_ENABLED;
	}
	mutex_unlock(&taos->enable_lock);
	return size;
}

static ssize_t tmd3725_prox_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		(taos->power_state & PROXIMITY_ENABLED) ? 1 : 0);
}

static ssize_t tmd3725_prox_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);
	bool new_value;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		SENSOR_ERR("invalid value\n");
		return size;
	}

	SENSOR_INFO("new_value = %d, old state = %d\n",
		new_value, (taos->power_state & PROXIMITY_ENABLED) ? 1 : 0);

	mutex_lock(&taos->enable_lock);
	if (new_value && !(taos->power_state & PROXIMITY_ENABLED)) {
		tmd3725_prox_vled_onoff(taos, ON);
		tmd3725_prox_initialize_target(taos);
		taos->prox_level_state = STATE_INIT;
		tmd3725_prox_set_threshold(taos);

		taos->power_state |= PROXIMITY_ENABLED;

		enable_irq(taos->prox_irq);
		enable_irq_wake(taos->prox_irq);
	} else if (!new_value && (taos->power_state & PROXIMITY_ENABLED)) {
		disable_irq_wake(taos->prox_irq);
		disable_irq(taos->prox_irq);

		tmd3725_set_op_mode(taos, MODE_PROX, OFF);
		tmd3725_prox_vled_onoff(taos, OFF);
		taos->power_state &= ~PROXIMITY_ENABLED;
	}
	mutex_unlock(&taos->enable_lock);

	return size;
}

static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   tmd3725_light_poll_delay_show,
		   tmd3725_light_poll_delay_store);

static struct device_attribute dev_attr_light_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		tmd3725_light_enable_show, tmd3725_light_enable_store);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_light_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

static struct device_attribute dev_attr_proximity_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		tmd3725_prox_enable_show, tmd3725_prox_enable_store);

static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_proximity_enable.attr,
	NULL
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};

static ssize_t tmd3725_get_vendor_name(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t tmd3725_get_chip_name(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_NAME);
}

static ssize_t tmd3725_prox_level_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", tmd3725_prox_get_adc(taos));
}

static ssize_t tmd3725_prox_avg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
		taos->prox_avg[0], taos->prox_avg[1], taos->prox_avg[2]);
}

static ssize_t tmd3725_prox_avg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);
	int new_value = 0;

	if (sysfs_streq(buf, "1"))
		new_value = true;
	else if (sysfs_streq(buf, "0"))
		new_value = false;
	else {
		SENSOR_ERR("invalid value\n");
		return size;
	}

	if (taos->prox_avg_enable == new_value)
		SENSOR_INFO("same status\n");
	else if (new_value == 1) {
		SENSOR_INFO("starting poll timer, delay %lldns\n",
		ktime_to_ns(taos->prox_avg_poll_delay));
		hrtimer_start(&taos->prox_avg_timer,
			taos->prox_avg_poll_delay, HRTIMER_MODE_REL);
		taos->prox_avg_enable = 1;
	} else {
		SENSOR_INFO("cancelling prox avg poll timer\n");
		hrtimer_cancel(&taos->prox_avg_timer);
		cancel_work_sync(&taos->work_prox_avg);
		taos->prox_avg_enable = 0;
	}

	return size;
}

static ssize_t tmd3725_prox_thd_det_high_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", taos->prox_thd_det_hi);
}

static ssize_t tmd3725_prox_thd_det_high_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);
	int thresh_value, ret;

	ret = kstrtoint(buf, 10, &thresh_value);
	if (ret < 0) {
		SENSOR_ERR("kstrtoint failed. %d", ret);
		return size;
	}

	SENSOR_INFO("thresh_hi = %d\n", thresh_value);
	taos->prox_thd_det_hi = thresh_value;
	tmd3725_prox_set_threshold(taos);

	return size;
}

static ssize_t tmd3725_prox_thd_still_det_hi_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", taos->prox_thd_still_det_hi);
}

static ssize_t tmd3725_prox_thd_still_det_hi_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);
	int thresh_value, ret;

	ret = kstrtoint(buf, 10, &thresh_value);
	if (ret < 0) {
		SENSOR_ERR("kstrtoint failed. %d", ret);
		return size;
	}

	SENSOR_INFO("thresh_low = %d\n", thresh_value);
	taos->prox_thd_still_det_hi = thresh_value;
	tmd3725_prox_set_threshold(taos);

	return size;
}

static ssize_t tmd3725_prox_thd_still_det_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", taos->prox_thd_still_det_low);
}

static ssize_t tmd3725_prox_thd_still_det_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);
	int thresh_value, ret;

	ret = kstrtoint(buf, 10, &thresh_value);
	if (ret < 0) {
		SENSOR_ERR("kstrtoint failed. %d", ret);
		return size;
	}

	SENSOR_INFO("thresh_low = %d\n", thresh_value);
	taos->prox_thd_still_det_low = thresh_value;
	tmd3725_prox_set_threshold(taos);

	return size;
}

static ssize_t tmd3725_prox_thd_rel_low_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", taos->prox_thd_rel_low);
}

static ssize_t tmd3725_prox_thd_rel_low_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);
	int thresh_value, ret;

	ret = kstrtoint(buf, 10, &thresh_value);
	if (ret < 0) {
		SENSOR_ERR("kstrtoint failed. %d", ret);
		return size;
	}

	SENSOR_INFO("thresh_low = %d\n", thresh_value);
	taos->prox_thd_rel_low = thresh_value;
	tmd3725_prox_set_threshold(taos);

	return size;
}

static ssize_t tmd3725_prox_trim_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", taos->prox_offset);
}

static ssize_t tmd3725_prox_trim_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int trim_value, ret;

	ret = kstrtoint(buf, 10, &trim_value);
	if (ret < 0) {
		SENSOR_ERR("kstrtoint failed. %d", ret);
		return size;
	}

	SENSOR_INFO("prox_trim = %d\n", trim_value);

	return size;
}

static ssize_t tmd3725_write_register_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int reg, val, ret;
	struct tmd3725_data *taos = dev_get_drvdata(dev);

	if (sscanf(buf, "%2x,%2x", &reg, &val) != 2) {
		SENSOR_ERR("invalid value\n");
		return count;
	}

	ret = tmd3725_i2c_write_data(taos, reg, val);
	if (ret < 0)
		SENSOR_ERR("failed %d\n", ret);
	else
		SENSOR_INFO("Register(0x%2x) data(0x%2x)\n", reg, val);

	return count;
}

static ssize_t tmd3725_read_register_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 reg;
	int offset = 0, val = 0;
	struct tmd3725_data *taos = dev_get_drvdata(dev);

	for (reg = ALS_TIME; reg <= CFG1; reg++) {
		val = tmd3725_i2c_read_data(taos, reg);
		SENSOR_INFO("Read Reg: 0x%2x Value: 0x%2x\n", reg, val);
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Reg: 0x%2x Value: 0x%2x\n", reg, val);
	}

	return offset;
}

static ssize_t tmd3725_dhr_sensor_info_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);
	uint8_t pgcfg1 = 0, pgcfg0 = 0;
	uint8_t p_gain = 0, p_drive_cur = 0;
	uint8_t per_time = 0;
	uint8_t p_pulse = 0, p_pulse_len = 0;
	uint8_t p_time = 0, l_atime = 0;

	pgcfg1 = tmd3725_i2c_read_data(taos, PGCFG1);
	p_gain = (pgcfg1 & 0XC0) >> 6;
	p_drive_cur = pgcfg1 & 0x1F;

	per_time = tmd3725_i2c_read_data(taos, PPERS);
	pgcfg0 = tmd3725_i2c_read_data(taos, PGCFG0);
	p_pulse_len = (pgcfg0 & 0XC0) >> 6;
	p_pulse = pgcfg0 & 0X3F;

	p_time = tmd3725_i2c_read_data(taos, PRX_RATE);
	l_atime = tmd3725_i2c_read_data(taos, ALS_TIME);

	return snprintf(buf, PAGE_SIZE, "\"THD\":\"%d %d %d %d\","\
		"\"PDRIVE_CURRENT\":\"%02x\","\
		"\"PERSIST_TIME\":\"%02x\","\
		"\"PPULSE\":\"%02x\","\
		"\"PGAIN\":\"%02x\","\
		"\"PTIME\":\"%02x\","\
		"\"PPLUSE_LEN\":\"%02x\","\
		"\"ATIME\":\"%02x\","\
		"\"POFFSET\":\"%d\"\n",
		taos->prox_thd_det_hi, taos->prox_thd_still_det_low,
		taos->prox_thd_still_det_hi, taos->prox_thd_rel_low,
		p_drive_cur, per_time, p_pulse,
		p_gain, p_time, p_pulse_len, l_atime, taos->prox_offset);
}

static ssize_t tmd3725_prox_cal_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int result = 0;
	return sprintf(buf, "%d\n", result);
}

static ssize_t tmd3725_prox_cal_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int result, ret;

	ret = kstrtoint(buf, 10, &result);
	if (ret < 0) {
		SENSOR_ERR("kstrtoint failed. %d", ret);
		return size;
	}

	SENSOR_INFO("prox_cal = %d\n", result);

	return size;
}


static DEVICE_ATTR(vendor, S_IRUGO, tmd3725_get_vendor_name, NULL);
static DEVICE_ATTR(name, S_IRUGO, tmd3725_get_chip_name, NULL);
static struct device_attribute dev_attr_proximity_raw_data =
	__ATTR(raw_data, S_IRUGO, tmd3725_prox_level_state_show, NULL);
static DEVICE_ATTR(prox_avg, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd3725_prox_avg_show, tmd3725_prox_avg_store);
static DEVICE_ATTR(state, S_IRUGO, tmd3725_prox_level_state_show, NULL);
static DEVICE_ATTR(thresh_high, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd3725_prox_thd_det_high_show,
	tmd3725_prox_thd_det_high_store);
static DEVICE_ATTR(thresh_low, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd3725_prox_thd_still_det_low_show,
	tmd3725_prox_thd_still_det_low_store);
static DEVICE_ATTR(thresh_detect_high, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd3725_prox_thd_still_det_hi_show,
	tmd3725_prox_thd_still_det_hi_store);
static DEVICE_ATTR(thresh_detect_low, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd3725_prox_thd_rel_low_show,
	tmd3725_prox_thd_rel_low_store);
static DEVICE_ATTR(prox_trim, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd3725_prox_trim_show, tmd3725_prox_trim_store);
static DEVICE_ATTR(register, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd3725_read_register_show, tmd3725_write_register_store);
static DEVICE_ATTR(dhr_sensor_info, S_IRUSR | S_IRGRP,
				tmd3725_dhr_sensor_info_show, NULL);
static DEVICE_ATTR(prox_cal, S_IRUGO | S_IWUSR | S_IWGRP,
	tmd3725_prox_cal_show,
	tmd3725_prox_cal_store);


static struct device_attribute *prox_sensor_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_state,
	&dev_attr_prox_avg,
	&dev_attr_proximity_raw_data,
	&dev_attr_thresh_high,
	&dev_attr_thresh_low,
	&dev_attr_thresh_detect_high,
	&dev_attr_thresh_detect_low,
	&dev_attr_prox_trim,
	&dev_attr_register,
	&dev_attr_dhr_sensor_info,
	&dev_attr_prox_cal,
	NULL
};

static ssize_t tmd3725_light_get_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", tmd3725_light_get_lux(taos));
}

static ssize_t tmd3725_light_get_raw_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tmd3725_data *taos = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u,%u,%u,%u,%u,%u\n",
		taos->red, taos->green, taos->blue, taos->clear,
		taos->als_time, taos->als_gain);
}

static DEVICE_ATTR(adc, S_IRUGO, tmd3725_light_get_lux_show, NULL);
static DEVICE_ATTR(lux, S_IRUGO, tmd3725_light_get_lux_show, NULL);
static struct device_attribute dev_attr_light_raw_data =
	__ATTR(raw_data, S_IRUGO, tmd3725_light_get_raw_data_show, NULL);

static struct device_attribute *lightsensor_additional_attributes[] = {
	&dev_attr_adc,
	&dev_attr_lux,
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_light_raw_data,
	NULL
};

static int tmd3725_setup_irq(struct tmd3725_data *taos)
{
	int ret = -EIO;

	ret = gpio_request(taos->prox_irq_gpio, "gpio_proximity_irq");
	if (ret < 0) {
		SENSOR_ERR("gpio %d request failed %d\n",
			taos->prox_irq_gpio, ret);
		goto err_exit;
	}

	ret = gpio_direction_input(taos->prox_irq_gpio);
	if (ret < 0) {
		SENSOR_ERR("failed to set gpio %d as input %d\n",
			taos->prox_irq_gpio, ret);
		goto err_gpio_direction_input;
	}

	taos->prox_irq = gpio_to_irq(taos->prox_irq_gpio);

	ret = request_threaded_irq(taos->prox_irq, NULL,
		tmd3725_prox_irq_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		"proximity_irq", taos);
	if (ret < 0) {
		SENSOR_ERR("request_irq %d failed for gpio %d %d\n",
			taos->prox_irq, taos->prox_irq_gpio, ret);
		goto err_request_irq;
	}

	/* start with interrupts disabled */
	disable_irq(taos->prox_irq);
	return 0;

err_request_irq:
err_gpio_direction_input:
	gpio_free(taos->prox_irq_gpio);
err_exit:
	return ret;
}

static int tmd3725_parse_dt(struct tmd3725_data *taos, struct device *dev)
{
	int ret = 0;
	enum of_gpio_flags flags;
	struct device_node *np = dev->of_node;


	taos->prox_irq_gpio = of_get_named_gpio_flags(np,
				"taos,irq_gpio", 0, &flags);
	if (taos->prox_irq_gpio < 0) {
		SENSOR_ERR("fail to get prox_irq_gpio\n");
		return -ENODEV;
	}

	taos->prox_vled_ldo_pin = of_get_named_gpio_flags(np,
				"taos,vled_ldo_pin", 0, &flags);
	if (taos->prox_vled_ldo_pin < 0) {
		SENSOR_ERR("fail to get vled_ldo_pin\n");
		taos->prox_vled_ldo_pin = 0;
	} else {
		ret = gpio_request(taos->prox_vled_ldo_pin, "prox_vled_en");
		if (ret < 0)
			SENSOR_ERR("gpio %d request failed %d\n",
				taos->prox_vled_ldo_pin, ret);
		else
			gpio_direction_output(taos->prox_vled_ldo_pin, 0);
	}

	if (of_property_read_u32(np, "taos,prox_thd_det_hi",
		&taos->prox_thd_det_hi) < 0)
		taos->prox_thd_det_hi = DEFAULT_THRESHOLD_DET_HI;
	if (of_property_read_u32(np, "taos,prox_thd_still_det_low",
		&taos->prox_thd_still_det_low) < 0)
		taos->prox_thd_still_det_low = DEFAULT_THRESHOLD_STILL_DET_LOW;
	SENSOR_INFO("th_det_hi:%d, th_still_det_low:%d\n",
		taos->prox_thd_det_hi, taos->prox_thd_still_det_low);

	if (of_property_read_u32(np, "taos,prox_thd_still_det_hi",
		&taos->prox_thd_still_det_hi) < 0)
		taos->prox_thd_still_det_hi = DEFAULT_THRESHOLD_STILL_DET_HI;
	if (of_property_read_u32(np, "taos,prox_thd_rel_low",
		&taos->prox_thd_rel_low) < 0)
		taos->prox_thd_rel_low = DEFAULT_THRESHOLD_REL_LOW;
	SENSOR_INFO("th_still_det_hi:%d, th_rel_low:%d\n",
		taos->prox_thd_still_det_hi, taos->prox_thd_rel_low);

	if (of_property_read_u32(np, "taos,coef_r", &taos->coef_r) < 0)
		taos->coef_r = DEFAULT_COEF_R;
	if (of_property_read_u32(np, "taos,coef_g", &taos->coef_g) < 0)
		taos->coef_g = DEFAULT_COEF_G;
	if (of_property_read_u32(np, "taos,coef_b", &taos->coef_b) < 0)
		taos->coef_b = DEFAULT_COEF_B;
	if (of_property_read_u32(np, "taos,coef_c", &taos->coef_c) < 0)
		taos->coef_c = DEFAULT_COEF_C;
	if (of_property_read_u32(np, "taos,dgf", &taos->dgf) < 0)
		taos->dgf = DEFAULT_DGF;
	if (of_property_read_u32(np, "taos,cct_coef", &taos->cct_coef) < 0)
		taos->cct_coef = DEFAULT_CCT_COEF;
	if (of_property_read_u32(np, "taos,cct_offset", &taos->cct_offset) < 0)
		taos->cct_offset = DEFAULT_CCT_OFFSET;
	SENSOR_INFO("c_r:%d c_g:%d c_b:%d dgf:%d cct_coef:%d cct_offset:%d\n",
		taos->coef_r, taos->coef_g, taos->coef_b,
		taos->dgf, taos->cct_coef, taos->cct_offset);

	if (of_property_read_u32(np, "taos,lux_mul", &taos->lux_mul) < 0)
		taos->lux_mul = DEFAULT_LUX_MULTIPLE;

	taos->ppulse = DEFAULT_PPULSE;
	taos->pgcfg1 = DEFAULT_PGCFG1;
	taos->als_time = DEFAULT_ATIME;
	taos->prate = DEFAULT_PRATE;
#ifdef CONFIG_SENSORS_TMD3725_RF_NOISE_DEFENCE_CODE
	taos->wtime = DEFAULT_WTIME_FOR_PROX_ONLY;
#else
	taos->wtime = DEFAULT_WTIME;
#endif
	taos->als_gain = DEFAULT_AGAIN;

	return 0;
}

static int tmd3725_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct tmd3725_data *taos;

	SENSOR_INFO("Start\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		SENSOR_ERR("i2c functionality check failed!\n");
		return ret;
	}

	taos = kzalloc(sizeof(struct tmd3725_data), GFP_KERNEL);
	if (!taos) {
		SENSOR_ERR("failed to alloc memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	if (client->dev.of_node) {
		ret = tmd3725_parse_dt(taos, &client->dev);
		if (ret < 0)
			goto err_tmd3725_data_free;
	}

	taos->i2c_client = client;
	i2c_set_clientdata(client, taos);

	/* ID Check */
	ret = i2c_smbus_read_byte_data(client, CMD_REG | CHIPID);
	if (ret < 0) {
		SENSOR_ERR("i2c read error %d\n", ret);
		goto err_chip_id_or_i2c_error;
	} else if (ret != TMD3725_CHIP_ID) {
		SENSOR_ERR("chip id error 0x[%X]\n", ret);
		ret = -ENXIO;
		goto err_chip_id_or_i2c_error;
	}

	/* wake lock init */
	wake_lock_init(&taos->prx_wake_lock, WAKE_LOCK_SUSPEND,
		"prx_wake_lock");
	mutex_init(&taos->prox_mutex);
	mutex_init(&taos->enable_lock);
	mutex_init(&taos->mode_lock);

	/* allocate proximity input_device */
	taos->prox_input_dev = input_allocate_device();
	if (!taos->prox_input_dev) {
		ret = -ENOMEM;
		SENSOR_ERR("could not allocate input device\n");
		goto err_input_allocate_device_proximity;
	}

	input_set_drvdata(taos->prox_input_dev, taos);
	taos->prox_input_dev->name = MODULE_NAME_PROX;
	input_set_capability(taos->prox_input_dev, EV_ABS, ABS_DISTANCE);
	input_set_abs_params(taos->prox_input_dev, ABS_DISTANCE, 0, 1, 0, 0);

	ret = input_register_device(taos->prox_input_dev);
	if (ret < 0) {
		SENSOR_ERR("could not register input device\n");
		input_free_device(taos->prox_input_dev);
		goto err_input_register_device_proximity;
	}
	ret = sensors_register(&taos->prox_dev, taos, prox_sensor_attrs,
		MODULE_NAME_PROX); /* factory attributs */
	if (ret < 0) {
		SENSOR_ERR("could not registersensors_register\n");
		goto err_sensor_register_device_proximity;
	}
	ret = sensors_create_symlink(&taos->prox_input_dev->dev.kobj,
		taos->prox_input_dev->name);
	if (ret < 0) {
		SENSOR_ERR("fail: sensors_create_symlink\n");
		goto err_symlink_device_proximity;
	}
	ret = sysfs_create_group(&taos->prox_input_dev->dev.kobj,
		&proximity_attribute_group);
	if (ret < 0) {
		SENSOR_ERR("could not create sysfs group\n");
		goto err_create_sensorgoup_proximity;
	}
	/* the timer just fires off a work queue request.  we need a thread
	   to read the i2c (can be slow and blocking). */
	taos->wq = create_singlethread_workqueue("tmd3725_wq");
	if (!taos->wq) {
		ret = -ENOMEM;
		SENSOR_ERR("could not create workqueue\n");
		goto err_create_workqueue;
	}

	taos->prox_avg_wq =
		create_singlethread_workqueue("tmd3725_wq_prox_avg_wq");
	if (!taos->prox_avg_wq) {
		ret = -ENOMEM;
		SENSOR_ERR("could not create workqueue\n");
		goto err_create_avg_workqueue;
	}

	/* this is the thread function we run on the work queue */
	INIT_DELAYED_WORK(&taos->work_light, tmd3725_work_func_light);
	taos->light_poll_delay = DEFAULT_LIGHT_POLL_DELAY;
	INIT_WORK(&taos->work_prox, tmd3725_work_func_prox);
	INIT_WORK(&taos->work_prox_avg, tmd3725_work_func_prox_avg);

	hrtimer_init(&taos->prox_avg_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	taos->prox_avg_poll_delay = ns_to_ktime(DEFAULT_PROX_AVG_POLL_DELAY);
	taos->prox_avg_timer.function = tmd3725_prox_avg_timer_func;

	/* allocate lightsensor-level input_device */
	taos->light_input_dev = input_allocate_device();
	if (!taos->light_input_dev) {
		SENSOR_ERR("could not allocate input device\n");
		ret = -ENOMEM;
		goto err_input_allocate_device_light;
	}
	input_set_drvdata(taos->light_input_dev, taos);
	taos->light_input_dev->name = MODULE_NAME_LIGHT;
	input_set_capability(taos->light_input_dev, EV_REL, REL_MISC);
	input_set_capability(taos->light_input_dev, EV_REL, REL_WHEEL);

	SENSOR_INFO("registering lightsensor-level input device\n");
	ret = input_register_device(taos->light_input_dev);
	if (ret < 0) {
		SENSOR_ERR("could not register input device\n");
		input_free_device(taos->light_input_dev);
		goto err_input_register_device_light;
	}
	ret = sensors_register(&taos->light_dev, taos,
		lightsensor_additional_attributes, MODULE_NAME_LIGHT);
	if (ret < 0) {
		SENSOR_ERR("cound not register light sensor device %d\n",
			ret);
		goto err_sensor_register_device_light;
	}

	ret = sensors_create_symlink(&taos->light_input_dev->dev.kobj,
		taos->light_input_dev->name);
	if (ret < 0) {
		SENSOR_ERR("cound not sensors_create_symlink %d.\n", ret);
		goto err_symlink_device_light;
	}

	ret = sysfs_create_group(&taos->light_input_dev->dev.kobj,
		&light_attribute_group);
	if (ret < 0) {
		SENSOR_ERR("could not create sysfs group\n");
		goto err_create_sensorgoup_light;
	}

	/* setup irq */
	ret = tmd3725_setup_irq(taos);
	if (ret < 0) {
		SENSOR_ERR("could not setup irq\n");
		goto err_setup_irq;
	}

	tmd3725_initialize_chip(taos);

	SENSOR_INFO("success\n");
	return 0;

err_setup_irq:
	sysfs_remove_group(&taos->light_input_dev->dev.kobj,
		&light_attribute_group);
err_create_sensorgoup_light:
	sensors_remove_symlink(&taos->light_input_dev->dev.kobj,
		taos->prox_input_dev->name);
err_symlink_device_light:
err_sensor_register_device_light:
	input_unregister_device(taos->light_input_dev);
err_input_register_device_light:
err_input_allocate_device_light:
	destroy_workqueue(taos->prox_avg_wq);
err_create_avg_workqueue:
	destroy_workqueue(taos->wq);
err_create_workqueue:
	sysfs_remove_group(&taos->prox_input_dev->dev.kobj,
		&proximity_attribute_group);
err_create_sensorgoup_proximity:
	sensors_remove_symlink(&taos->prox_input_dev->dev.kobj,
		taos->prox_input_dev->name);
err_symlink_device_proximity:
err_sensor_register_device_proximity:
	input_unregister_device(taos->prox_input_dev);
err_input_register_device_proximity:
err_input_allocate_device_proximity:
	mutex_destroy(&taos->mode_lock);
	mutex_destroy(&taos->enable_lock);
	mutex_destroy(&taos->prox_mutex);
	wake_lock_destroy(&taos->prx_wake_lock);
err_chip_id_or_i2c_error:
	if (taos->prox_vled_ldo_pin)
		gpio_free(taos->prox_vled_ldo_pin);
err_tmd3725_data_free:
	kfree(taos);
err_exit:
	SENSOR_ERR("failed %d\n", ret);
	return ret;
}

static void tmd3725_i2c_shutdown(struct i2c_client *client)
{
	struct tmd3725_data *taos = i2c_get_clientdata(client);

	if (taos->power_state & LIGHT_ENABLED) {
		tmd3725_light_disable(taos);
		tmd3725_set_op_mode(taos, MODE_ALS, OFF);
		taos->power_state &= ~LIGHT_ENABLED;
	}

	if (taos->power_state & PROXIMITY_ENABLED) {
		disable_irq_wake(taos->prox_irq);
		disable_irq(taos->prox_irq);

		tmd3725_set_op_mode(taos, MODE_PROX, OFF);
		tmd3725_prox_vled_onoff(taos, OFF);
		taos->power_state &= ~PROXIMITY_ENABLED;
	}

	SENSOR_INFO("is called\n");
}

static int tmd3725_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tmd3725_data *taos = i2c_get_clientdata(client);

	if (taos->power_state & LIGHT_ENABLED) {
		SENSOR_INFO("is called\n");
		tmd3725_light_disable(taos);
		tmd3725_set_op_mode(taos, MODE_ALS, OFF);
	}

	if (taos->power_state & PROXIMITY_ENABLED) {
		SENSOR_INFO("is called\n");
		disable_irq(taos->prox_irq);
	}

	return 0;
}

static int tmd3725_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tmd3725_data *taos = i2c_get_clientdata(client);

	if (taos->power_state & LIGHT_ENABLED) {
		SENSOR_INFO("is called\n");
		tmd3725_light_enable(taos);
		tmd3725_set_op_mode(taos, MODE_ALS, ON);
	}

	if (taos->power_state & PROXIMITY_ENABLED) {
		SENSOR_INFO("is called\n");
		enable_irq(taos->prox_irq);
	}
	return 0;
}

static int tmd3725_i2c_remove(struct i2c_client *client)
{
	SENSOR_INFO("\n");
	return 0;
}

static const struct i2c_device_id tmd3725_device_id[] = {
	{ CHIP_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, tmd3725_device_id);

static const struct dev_pm_ops tmd3725_pm_ops = {
	.suspend = tmd3725_suspend,
	.resume = tmd3725_resume
};

#ifdef CONFIG_OF
static struct of_device_id tm3725_match_table[] = {
	{ .compatible = "taos,tmd3725",},
};
#else
#define tm3725_match_table NULL
#endif

static struct i2c_driver tmd3725_i2c_driver = {
	.driver = {
		.name = CHIP_NAME,
		.owner = THIS_MODULE,
		.pm = &tmd3725_pm_ops,
		.of_match_table = tm3725_match_table,
	},
	.probe = tmd3725_i2c_probe,
	.remove = tmd3725_i2c_remove,
	.shutdown = tmd3725_i2c_shutdown,
	.id_table = tmd3725_device_id,
};


static int __init tmd3725_init(void)
{
	return i2c_add_driver(&tmd3725_i2c_driver);
}

static void __exit tmd3725_exit(void)
{
	i2c_del_driver(&tmd3725_i2c_driver);
}

module_init(tmd3725_init);
module_exit(tmd3725_exit);

MODULE_AUTHOR("SAMSUNG");
MODULE_DESCRIPTION("Optical Sensor driver for tmd3725");
MODULE_LICENSE("GPL");
