/*
 * sec_tclm.c - samsung tclm command driver
 *
 * Copyright (C) 2017 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/input/sec_tclm.h>
#include <linux/input.h>

struct sec_cal_position sec_cal_positions[CALPOSITION_MAX] = {
	{CAL_POS_CMD("NONE",	'N'),}, /* 0, NONe */
	{CAL_POS_CMD("INIT",	'I'),},	/* 1, INIT case, calcount is 00 or FF */
	{CAL_POS_CMD("FACT",	'F'),},	/* 2, FACTory line, run_force_calibration without value */
	{CAL_POS_CMD("OUTS",	'O'),},	/* 3, OUTSide of factory */
	{CAL_POS_CMD("LCIA",	'L'),},	/* 4, LCIA in factory line */
	{CAL_POS_CMD("CENT",	'C'),},	/* 5, svc CENTer, cal from service center */
	{CAL_POS_CMD("ABNO",	'A'),},	/* 6, ABNOrmal case */
	{CAL_POS_CMD("BOOT",	'B'),},	/* 7, BOOT firmup, when firmup in booting time */
	{CAL_POS_CMD("SPEC",	'S'),},	/* 8, SPECout case */
	{CAL_POS_CMD("TUNE",	'V'),},	/* 9, TUNE Version up, when afe version is lage than tune version */
	{CAL_POS_CMD("EVER",	'E'),},	/* 10, EVERytime, always cal in Booting */
	{CAL_POS_CMD("TEST",	'T'),},	/* 11, TESTmode, firmup case in *#2663# */
	{CAL_POS_CMD("UNDE",	'U'),},	/* 12, UNDEfine, undefined value */
	{CAL_POS_CMD("UNDE",	'U'),},	/* 13 */
	{CAL_POS_CMD("UNDE",	'U'),},	/* 14 */
	{CAL_POS_CMD("UNDE",	'U'),}	/* 15 */
};

void sec_tclm_case(struct sec_tclm_data *data, int tclm_case)
{
	switch (tclm_case) {
	case 0:
	case 'F':
	case 'f':
		if (data->external_factory == true)
			sec_tclm_root_of_cal(data, CALPOSITION_OUTSIDE);
		else
			sec_tclm_root_of_cal(data, CALPOSITION_FACTORY);
		break;

	case 'L':
	case 'l':
		sec_tclm_root_of_cal(data, CALPOSITION_LCIA);
		break;

	case 'C':
	case 'c':
		sec_tclm_root_of_cal(data, CALPOSITION_SVCCENTER);
		break;

	case 'O':
	case 'o':
		sec_tclm_root_of_cal(data, CALPOSITION_OUTSIDE);
		break;

	default:
		sec_tclm_root_of_cal(data, CALPOSITION_ABNORMAL);
	}
}
bool sec_tclm_get_nvm_all(struct sec_tclm_data *data)
{
	bool ret = true;
	/* just don't read tune_fix_version, because this is write_only_value. */
	data->tclm_read(data->client, SEC_TCLM_NVM_ALL_DATA);

	/* lsi index value */
	data->cal_count = data->tclm_read(data->client, SEC_TCLM_NVM_OFFSET_CAL_COUNT);
	data->cal_position = data->tclm_read(data->client, SEC_TCLM_NVM_OFFSET_CAL_POSITION);
	if (data->cal_count == 0xFF || data->cal_position >= CALPOSITION_MAX) {
		data->cal_count = 0;
		data->cal_position = 0;
		data->tune_fix_ver = 0;
		data->cal_pos_hist_cnt = 0;
		data->cal_pos_hist_lastp = 0;
		ret = false;
		pr_info("%s %s: cal data is abnormal", SECLOG, __func__);
	} else {
		data->cal_pos_hist_cnt = data->tclm_read(data->client, SEC_TCLM_NVM_OFFSET_HISTORY_QUEUE_COUNT);
		data->cal_pos_hist_lastp = data->tclm_read(data->client, SEC_TCLM_NVM_OFFSET_HISTORY_QUEUE_LASTP);
		if ((data->cal_pos_hist_cnt > 0) && (data->cal_pos_hist_cnt <= CAL_HISTORY_QUEUE_MAX)
			&& (data->cal_pos_hist_lastp < CAL_HISTORY_QUEUE_MAX))
			data->tclm_read(data->client, SEC_TCLM_NVM_OFFSET_HISTORY_QUEUE_SIZE);

		else
			data->cal_pos_hist_cnt = 0;	/* error case */

		pr_info("%s %s: cal_count:%d, pos:%d(%4s), hist_count:%d, lastp:%d\n",
			SECLOG, __func__, data->cal_count, data->cal_position,
			data->tclm_string[data->cal_position].f_name,
			data->cal_pos_hist_cnt, data->cal_pos_hist_lastp);
	}

	data->tclm_read(data->client, SEC_TCLM_NVM_ALL_DATA_DONE);

	return ret;
}

void sec_tclm_position_history(struct sec_tclm_data *data)
{
	int i;
	int now_lastp = data->cal_pos_hist_lastp;

	if (data->cal_pos_hist_cnt > CAL_HISTORY_QUEUE_MAX
		|| data->cal_pos_hist_lastp >= CAL_HISTORY_QUEUE_MAX) {
		pr_info("%s %s: not initial case, count:%X, p:%X\n", SECLOG, __func__,
			data->cal_pos_hist_cnt, data->cal_pos_hist_lastp);
		return;
	}

	pr_info("%s %s: [Now] %4s%d\n", SECLOG, __func__,
		data->tclm_string[data->cal_position].f_name, data->cal_count);
	pr_info("%s %s: [Old] ", SECLOG, __func__);

	for (i = 0; i < data->cal_pos_hist_cnt; i++) {
		pr_cont("%c%d", data->tclm_string[data->cal_pos_hist_queue[2 * now_lastp]].s_name, data->cal_pos_hist_queue[2 * now_lastp + 1]);

		if (i < CAL_HISTORY_QUEUE_SHORT_DISPLAY) {
			data->cal_pos_hist_last3[2 * i] = data->tclm_string[data->cal_pos_hist_queue[2 * now_lastp]].s_name;
			data->cal_pos_hist_last3[2 * i + 1] = data->cal_pos_hist_queue[2 * now_lastp + 1];
		}

		if (now_lastp <= 0)
			now_lastp = CAL_HISTORY_QUEUE_MAX - 1;
		else
			now_lastp--;
	}
	pr_cont("\n");

	if (i < CAL_HISTORY_QUEUE_SHORT_DISPLAY)
		data->cal_pos_hist_last3[2 * i] = 0;
	else
		data->cal_pos_hist_last3[6] = 0;

}

void sec_tclm_debug_info(struct sec_tclm_data *data)
{
	sec_tclm_position_history(data);
}

void sec_tclm_root_of_cal(struct sec_tclm_data *data, int pos)
{
	data->root_of_calibration = pos;
	pr_info("%s %s: root - %d(%4s)\n", SECLOG, __func__,
		pos, data->tclm_string[pos].f_name);
}

static bool sec_tclm_check_condition_valid(struct sec_tclm_data *data)
{

	pr_err("%s %s tclm_level:%02X, last pos:%d(%4s), now pos:%d(%4s)\n",
		SECLOG, __func__, data->tclm_level,
		data->cal_position, data->tclm_string[data->cal_position].f_name,
		data->root_of_calibration, data->tclm_string[data->root_of_calibration].f_name);

	/* enter case */
	switch (data->tclm_level) {
	case TCLM_LEVEL_LOCKDOWN:
		if ((data->root_of_calibration == CALPOSITION_TUNEUP)
			|| (data->root_of_calibration == CALPOSITION_INITIAL)) {
			return true;
		} else if ((data->root_of_calibration == CALPOSITION_TESTMODE)
			&& ((data->cal_position == CALPOSITION_TESTMODE)
			|| (data->cal_position == CALPOSITION_TUNEUP))) {
			return true;
		}
		break;

	case TCLM_LEVEL_CLEAR_NV:
		return true;

	case TCLM_LEVEL_EVERYTIME:
		return true;

	case TCLM_LEVEL_NONE:
		if ((data->root_of_calibration == CALPOSITION_TESTMODE)
			|| (data->root_of_calibration == CALPOSITION_INITIAL)) {
			return true;
		} else {
			return false;
		}
	}

	return false;
}

bool sec_execute_tclm_package(struct sec_tclm_data *data, int factory_mode)
{
	int ret, rc;

	pr_err("%s %s: tclm_level:%02X, last pos:%d(%4s), now pos:%d(%4s), factory:%d\n",
		SECLOG, __func__, data->tclm_level,
		data->cal_position, data->tclm_string[data->cal_position].f_name,
		data->root_of_calibration, data->tclm_string[data->root_of_calibration].f_name,
		factory_mode);

	/* first read cal data for compare */
	disable_irq(data->irq);
	sec_tclm_get_nvm_all(data);

	/* if is run_for_calibration, don't check cal condition */
	if (!factory_mode) {

		/*check cal condition */
		rc = sec_tclm_check_condition_valid(data);
		if (rc) {
			pr_err("%s %s: RUN OFFSET CALIBRATION,%d\n", SECLOG, __func__, rc);
		} else {
			pr_err("%s %s: fail tclm condition,%d, root:%d\n",
				SECLOG, __func__, rc, data->root_of_calibration);
			enable_irq(data->irq);
			return 0;
		}

		/* execute force cal */
		ret = data->tclm_execute_force_calibration(data->client, TCLM_OFFSET_CAL_SEC);
		if (ret < 0) {
			pr_err("%s %s: fail to write OFFSET CAL SEC!\n", SECLOG, __func__);
			enable_irq(data->irq);
			return 0;
		}
	}

	if ((data->cal_count < 1) || (data->cal_count >= 0xFF)) {
		/* all nvm clear */
		data->cal_count = 0;
		data->cal_pos_hist_cnt = 0;
		data->cal_pos_hist_lastp = 0;

		/* save history queue */
		data->tclm_write(data->client, SEC_TCLM_NVM_OFFSET_HISTORY_QUEUE_COUNT, data->cal_pos_hist_cnt);
		data->tclm_write(data->client, SEC_TCLM_NVM_OFFSET_HISTORY_QUEUE_LASTP, data->cal_pos_hist_lastp);
	} else if (data->root_of_calibration != data->cal_position) {
		/* current data of cal count,position save cal history queue */

		/* first read history_cnt, history_lastp */
		data->cal_pos_hist_cnt = data->tclm_read(data->client, SEC_TCLM_NVM_OFFSET_HISTORY_QUEUE_COUNT);
		data->cal_pos_hist_lastp = data->tclm_read(data->client, SEC_TCLM_NVM_OFFSET_HISTORY_QUEUE_LASTP);

		if (data->cal_pos_hist_cnt > CAL_HISTORY_QUEUE_MAX || data->cal_pos_hist_lastp >= CAL_HISTORY_QUEUE_MAX) {
			/* queue nvm clear case */
			data->cal_pos_hist_cnt = 0;
			data->cal_pos_hist_lastp = 0;
		}

		/*calculate queue lastpointer */
		if (data->cal_pos_hist_cnt == 0)
			data->cal_pos_hist_lastp = 0;
		else if (data->cal_pos_hist_lastp >= (CAL_HISTORY_QUEUE_MAX - 1))
			data->cal_pos_hist_lastp = 0;
		else
			data->cal_pos_hist_lastp++;

		/*calculate queue count */
		if (data->cal_pos_hist_cnt >= CAL_HISTORY_QUEUE_MAX)
			data->cal_pos_hist_cnt = CAL_HISTORY_QUEUE_MAX;
		else
			data->cal_pos_hist_cnt++;

		/* save history queue */
		data->tclm_write(data->client, SEC_TCLM_NVM_OFFSET_HISTORY_QUEUE_COUNT, data->cal_pos_hist_cnt);
		data->tclm_write(data->client, SEC_TCLM_NVM_OFFSET_HISTORY_QUEUE_LASTP, data->cal_pos_hist_lastp);
		data->tclm_write(data->client, SEC_TCLM_NVM_OFFSET_HISTORY_QUEUE_SAVE, true);

		data->cal_pos_hist_queue[data->cal_pos_hist_lastp * 2] = data->cal_position;
		data->cal_pos_hist_queue[data->cal_pos_hist_lastp * 2 + 1] = data->cal_count;

		data->cal_count = 0;
	}

	if (data->cal_count == 0) {
		/* saving cal position */
		data->cal_position = data->root_of_calibration;
		data->tclm_write(data->client, SEC_TCLM_NVM_OFFSET_CAL_POSITION, data->root_of_calibration);
	}

	data->cal_count++;
	/* saving cal_count */
	data->tclm_write(data->client, SEC_TCLM_NVM_OFFSET_CAL_COUNT, data->cal_count);

	/* saving tune_version */
	rc = data->tclm_read(data->client, SEC_TCLM_NVM_OFFSET_IC_FIRMWARE_VER);
	data->tclm_write(data->client, SEC_TCLM_NVM_OFFSET_TUNE_VERSION, rc);
	data->tune_fix_ver = data->tclm_read(data->client, SEC_TCLM_NVM_OFFSET_TUNE_VERSION);

	sec_tclm_position_history(data);

	enable_irq(data->irq);
	return 1;
}

bool sec_tclm_check_cal_case(struct sec_tclm_data *data)
{
	int restore_cal = 0;

	if (data->cal_count == 0xFF) {
		data->cal_count = data->tclm_read(data->client, SEC_TCLM_NVM_OFFSET_CAL_COUNT);
		pr_info("%s %s: cal_count value [%d]\n", SECLOG, __func__, data->cal_count);

	}

	if ((data->cal_count == 0) || (data->cal_count == 0xFF)) {
		pr_err("%s %s: Calcount is abnormal,%02X\n", SECLOG, __func__, data->cal_count);
		/* nvm uninitialed case */
		sec_tclm_root_of_cal(data, CALPOSITION_INITIAL);
		restore_cal = 1;
	} else if (data->tclm_level == TCLM_LEVEL_EVERYTIME) {
		/* everytime case */
		sec_tclm_root_of_cal(data, CALPOSITION_EVERYTIME);
		restore_cal = 1;
	}

	if (restore_cal) {
		sec_execute_tclm_package(data, 0);
		sec_tclm_root_of_cal(data, CALPOSITION_NONE);
		return true;
	}

	return false;
}

void sec_tclm_initialize(struct sec_tclm_data *data)
{
	data->root_of_calibration = CALPOSITION_NONE;
	data->cal_position = 0;
	data->cal_pos_hist_cnt = 0;
	data->cal_pos_hist_last3[0] = 0;
	data->tclm_string = sec_cal_positions;
	data->cal_count = 0xFF;
}

MODULE_DESCRIPTION("Samsung tclm command");
MODULE_LICENSE("GPL");

