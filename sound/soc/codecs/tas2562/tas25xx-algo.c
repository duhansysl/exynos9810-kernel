/* Copyright (c) 2015-2016, 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * Author		: Vijeth (vijeth.po@pathpartnertech.com)
 * Revised Date	: 30-07-19
 * Description	: TI Smartamp algorithm control interface
 *
 */
#ifdef CONFIG_TAS25XX_ALGO

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <sound/smart_amp.h>
#include <linux/power_supply.h>
#include "tas25xx-algo.h"

static uint32_t port = 0;

static uint8_t calibration_result[MAX_CHANNELS] = {STATUS_NONE};
static uint8_t validation_result[MAX_CHANNELS] = {STATUS_NONE};
static bool calibration_status = 0;
static bool validation_running_status = 0;
static uint32_t calib_re_hold[MAX_CHANNELS] = {0};
static uint32_t amb_temp_hold[MAX_CHANNELS] = {0};
static int32_t re_low[MAX_CHANNELS] = {0};
static int32_t re_high[MAX_CHANNELS] = {0};

/*Mutex to serialize DSP read/write commands*/
static struct mutex routing_lock;
static struct tas25xx_algo* p_tas25xx_algo = NULL;

/*Max value supported is 2^8*/
static uint8_t trans_val_to_user_m(uint32_t val, uint8_t qformat)
{
	uint32_t ret = (uint32_t)(((long long)val * 1000) >> qformat) % 1000;
	return (uint8_t)(ret / 10);
}

/*Max value supported is 2^8*/
static uint8_t trans_val_to_user_i(uint32_t val, uint8_t qformat)
{
	return ((val * 100) >> qformat) / 100;
}

static int afe_smartamp_get_set(u8 *user_data, uint32_t param_id,
		uint8_t get_set, uint32_t length)
{
	int ret = 0;
	struct ti_smartpa_data resp_data;

	if(!p_tas25xx_algo)
	{
		pr_err("[TI-SmartPA:%s] memory not allocated yet for p_tas25xx_algo",
			__func__);
	}

	switch (get_set) {
		case TAS_SET_PARAM:
		    memcpy(resp_data.payload,user_data,length);
			ret = ti_smartpa_write((void*)&resp_data, param_id, length);
			break;
		case TAS_GET_PARAM:
			memset(&resp_data, 0, sizeof(resp_data));

			ret = ti_smartpa_read((void*)&resp_data, param_id, length);

			if (ret == 0)
				memcpy(user_data, resp_data.payload, length);

			break;
		default:
			goto fail_cmd;
	}
fail_cmd:
	return ret;
}

/*Wrapper around set/get parameter, all set/get commands pass through this wrapper*/
int afe_smartamp_algo_ctrl(u8 *user_data, uint32_t param_id,
		uint8_t get_set, uint32_t length)
{
	int ret = 0;
	mutex_lock(&routing_lock);
	ret = afe_smartamp_get_set(user_data, param_id, get_set, length);
	mutex_unlock(&routing_lock);
	return ret;
}

static uint8_t tas25xx_get_amb_temp(void)
{
	struct power_supply *psy;
	union power_supply_propval value = {0};

	psy = power_supply_get_by_name("battery");
	if (!psy || !psy->desc || !psy->desc->get_property) {
		pr_err("[TI-SmartPA:%s] getting ambient temp failed, using default value %d",
			__func__, DEFAULT_AMBIENT_TEMP);
		return DEFAULT_AMBIENT_TEMP;
	}
	psy->desc->get_property(psy, POWER_SUPPLY_PROP_TEMP, &value);

	return DIV_ROUND_CLOSEST(value.intval, 10);
}

static int tas25xx_get_efs_data(uint32_t *data, uint8_t rdc_temp_l_r)
{
	struct file *pf = NULL;
	char fname[MAX_STRING] = {0};
	loff_t pos = 0;
	char calib_data[MAX_STRING] = {0};
	uint32_t data_flt[2];
	mm_segment_t fs;
	int ret = 0;

	if(!p_tas25xx_algo)
	{
		pr_err("[TI-SmartPA:%s] memory not allocated yet for p_tas25xx_algo",
			__func__);
	}

	fs = get_fs();
	set_fs(get_ds());

	if(rdc_temp_l_r == TEMP_L)
		memcpy(fname, TAS25XX_EFS_TEMP_DATA_L, sizeof(TAS25XX_EFS_TEMP_DATA_L));
	else if(rdc_temp_l_r == RDC_L)
		memcpy(fname, TAS25XX_EFS_CALIB_DATA_L, sizeof(TAS25XX_EFS_CALIB_DATA_L));
	else if(rdc_temp_l_r == TEMP_R)
		memcpy(fname, TAS25XX_EFS_TEMP_DATA_R, sizeof(TAS25XX_EFS_TEMP_DATA_R));
	else if(rdc_temp_l_r == RDC_R)
		memcpy(fname, TAS25XX_EFS_CALIB_DATA_R, sizeof(TAS25XX_EFS_CALIB_DATA_R));

	pf = filp_open(fname, O_RDONLY, 0666);

	if(!IS_ERR(pf))
	{
		vfs_read(pf, calib_data, MAX_STRING-1, &pos);
		if((rdc_temp_l_r == TEMP_L) || (rdc_temp_l_r == TEMP_R))
		{
			ret = sscanf(calib_data, "%d", data);
			if(ret != 1)
			{
				pr_err("[TI-SmartPA:%s] file %s read error\n", __func__, fname);
				ret = -1;
			}
		}
		else
		{
			ret = sscanf(calib_data, "%d.%d", &(data_flt[0]), &(data_flt[1]));
			if(ret != 2)
			{
				pr_err("[TI-SmartPA:%s] file %s read error\n", __func__, fname);
				ret = -1;
			}
			*data = TRANSF_USER_TO_IMPED(data_flt[0], data_flt[1]);
		}
		filp_close(pf, NULL);
	}
	else
	{
		pr_err("[TI-SmartPA:%s] file %s open failed\n", __func__, fname);
		ret = -1;
	}
	set_fs(fs);
	return ret;
}

void tas25xx_update_big_data(void)
{
	uint8_t iter = 0;
	uint32_t data[8] = {0};
	uint32_t param_id = 0;
	int32_t ret = 0;

	if(!p_tas25xx_algo)
	{
		pr_err("[TI-SmartPA:%s] memory not allocated yet for p_tas25xx_algo",
			__func__);
	}

	for(iter = 0; iter < p_tas25xx_algo->spk_count; iter++)
	{
		/*Reset data*/
		memset(data, 0, 8*sizeof(uint32_t));
		param_id = (TAS_SA_EXC_TEMP_STAT)|((iter+1)<<24)|((sizeof(data)/sizeof(uint32_t))<<16);
		ret = afe_smartamp_algo_ctrl((u8*)(&(data[0])), param_id
			, TAS_GET_PARAM, sizeof(data));
		if (ret < 0)
		{
			pr_err("[TI-SmartPA:%s] Failed to get Excursion and Temperature Stats", __func__);
		}
		else
		{
			pr_err("[TI-SmartPA:%s] Emax[%d] %d(%02d.%02d), Tmax[%d] %d, EOcount[%d] %d, TOcount[%d] %d \n",
						__func__, iter, data[0], (int32_t)trans_val_to_user_i(data[0], QFORMAT31),
						(int32_t)trans_val_to_user_m(data[0], QFORMAT31),
						iter, data[1], iter, data[2], iter, data[3]);

			/*Update Excursion Data*/
			p_tas25xx_algo->b_data[iter].exc_max =
				(data[0] > p_tas25xx_algo->b_data[iter].exc_max) ? data[0]:p_tas25xx_algo->b_data[iter].exc_max;
			p_tas25xx_algo->b_data[iter].exc_max_persist =
				(data[0] > p_tas25xx_algo->b_data[iter].exc_max_persist) ? data[0]:p_tas25xx_algo->b_data[iter].exc_max_persist;
			p_tas25xx_algo->b_data[iter].exc_over_count += data[2];

			/*Update Temperature Data*/
			p_tas25xx_algo->b_data[iter].temp_max =
				(data[1] > p_tas25xx_algo->b_data[iter].temp_max) ? data[1]:p_tas25xx_algo->b_data[iter].temp_max;
			p_tas25xx_algo->b_data[iter].temp_max_persist =
				(data[1] > p_tas25xx_algo->b_data[iter].temp_max_persist) ? data[1]:p_tas25xx_algo->b_data[iter].temp_max_persist;
			p_tas25xx_algo->b_data[iter].temp_over_count += data[3];
		}
	}
}

int tas25xx_update_calibration_limits(void)
{
	uint8_t iter = 0;
	uint32_t param_id = 0;
	int32_t ret = 0;

	/*Reset Calibration Result*/	
	memset(calibration_result, STATUS_NONE, sizeof(uint8_t)*MAX_CHANNELS);

	for(iter = 0; iter < p_tas25xx_algo->spk_count; iter++)
	{
		/*Update Lower limit for calibration*/
		re_low[iter] = 0;//Reset data to 0
		param_id = ((TAS_SA_GET_RE_LOW)|((iter+1)<<24)|(1<<16));
		ret = afe_smartamp_algo_ctrl((u8*)&re_low[iter], param_id,
			TAS_GET_PARAM, sizeof(uint32_t));
		if (ret < 0) {
			pr_err("[TI-SmartPA:%s]get re low fail channel no %d Exiting ..\n", __func__, iter);
			return -1;
		}
		re_low[iter] = re_low[iter] >> 8; /* Qformat 27 -> 19*/
		/*Update Upper limit for calibration*/
		re_high[iter] = 0;//Reset data to 0
		param_id = ((TAS_SA_GET_RE_HIGH)|((iter+1)<<24)|(1<<16));
		ret = afe_smartamp_algo_ctrl((u8*)&re_high[iter], param_id,
			TAS_GET_PARAM, sizeof(uint32_t));
		if (ret < 0) {
			pr_err("[TI-SmartPA:%s]get re high fail channel no %d Exiting ..\n", __func__, iter);
			return -1;
		}
		re_high[iter] = re_high[iter] >> 8; /* Qformat 27 -> 19*/
		pr_err("[TI-SmartPA:%s] Channel No:%d, Rdc Limits(%02d.%02d ~ %02d.%02d) \n", __func__, iter,
			(int32_t)trans_val_to_user_i(re_low[iter], QFORMAT19), (int32_t)trans_val_to_user_m(re_low[iter], QFORMAT19),
			(int32_t)trans_val_to_user_i(re_high[iter], QFORMAT19), (int32_t)trans_val_to_user_m(re_high[iter], QFORMAT19));
	}
	return 0;
} 

int tas25xx_check_limits(uint8_t channel, int32_t rdc)
{
	if((rdc >= re_low[channel]) && (rdc <= re_high[channel]))
	{
		calibration_result[channel] = STATUS_SUCCESS;
		return 1;
	}
	calibration_result[channel] = STATUS_FAIL;
	return 0;
}

void tas25xx_send_algo_calibration(void)
{
	uint32_t calib_re = 0;
	uint32_t amb_temp = 0;
	uint32_t data = 0;
	uint32_t param_id = 0;
	uint8_t iter;
	int32_t ret = 0;

	if(!p_tas25xx_algo)
	{
		pr_err("[TI-SmartPA:%s] memory not allocated yet for p_tas25xx_algo",
			__func__);
	}

	for(iter = 0; iter < p_tas25xx_algo->spk_count; iter++)
	{
		if(!(p_tas25xx_algo->calib_update[iter]))
		{
			p_tas25xx_algo->calib_update[iter] = true;

			ret = tas25xx_get_efs_data(&calib_re, RDC_L + iter*2);
			if(ret < 0)
			{
				p_tas25xx_algo->calib_update[iter] = false;
			}

			ret = tas25xx_get_efs_data(&amb_temp, TEMP_L + iter*2);
			if(ret < 0)
			{
				p_tas25xx_algo->calib_update[iter] = false;
			}

			if(p_tas25xx_algo->calib_update[iter])
			{
				p_tas25xx_algo->calib_re[iter] = calib_re;
				p_tas25xx_algo->amb_temp[iter] = amb_temp;
			}
		}

		if(p_tas25xx_algo->calib_update[iter])
		{
			/*Set ambient temperature*/
			data = p_tas25xx_algo->amb_temp[iter];
			param_id = (TAS_SA_SET_TCAL)|((iter+1)<<24)|(1<<16);
			ret = afe_smartamp_algo_ctrl((u8*)&data, param_id
				,TAS_SET_PARAM, sizeof(uint32_t));
			/*Set Re*/
			data = p_tas25xx_algo->calib_re[iter];
			param_id = (TAS_SA_SET_RE)|((iter+1)<<24)|(1<<16);
			ret = afe_smartamp_algo_ctrl((u8*)&data, param_id
				,TAS_SET_PARAM, sizeof(uint32_t));
		}
	}
}

static int tas25xx_save_calib_data(uint32_t *calib_rdc)
{
	uint8_t iter = 0;

	if(!calib_rdc)
	{
		pr_err("[TI-SmartPA:%s] argument is Null", __func__);
		return -1;
	}

	for(iter = 0; iter < p_tas25xx_algo->spk_count; iter++)
	{
		calib_re_hold[iter] = calib_rdc[iter];
		amb_temp_hold[iter] = tas25xx_get_amb_temp();
	}
	return 0;
}

/*******************************Calibration Related Codes Start*************************************/
static void calib_work_routine(struct work_struct *work)
{
	uint8_t iter = 0, iter2 = 0;
	uint32_t data = 0;
	uint32_t param_id = 0;
	uint32_t calib_re[MAX_CHANNELS] = {0};
	int32_t ret = 0;
	
	if(tas25xx_update_calibration_limits())
		return;
	
	/*Get Re*/
	for(iter2 = 0; iter2 < CALIB_RETRY_COUNT; iter2++)
	{
		for(iter = 0; iter < p_tas25xx_algo->spk_count; iter++)
		{
			if(calibration_result[iter] == STATUS_SUCCESS)
				continue;
			
			/*Calinration Init*/
			data = 1;/*Value is ignored*/
			param_id = (TAS_SA_CALIB_INIT)|((iter+1)<<24)|(1<<16);
			ret = afe_smartamp_algo_ctrl((u8*)&data, param_id
				, TAS_SET_PARAM, sizeof(uint32_t));
			if (ret < 0)
			{
				pr_err("[TI-SmartPA:%s] Init error. Exiting ..", __func__);
				return;
			}

			msleep(CALIB_TIME*1000);

			data = 0;//Reset data to 0
			param_id = ((TAS_SA_GET_RE)|((iter+1)<<24)|(1<<16));
			ret = afe_smartamp_algo_ctrl((u8*)&data, param_id,
				TAS_GET_PARAM, sizeof(uint32_t));
			if (ret < 0) {
				calibration_result[iter] = STATUS_FAIL;
				pr_info("[TI-SmartPA:%s]get re fail. Exiting ..\n", __func__);
			} else {
				calib_re[iter] = data;
				pr_info("[TI-SmartPA:%s]calib_re is %02d.%02d (%d) \n", __func__,
					(int32_t)trans_val_to_user_i(calib_re[iter], QFORMAT19), (int32_t)trans_val_to_user_m(calib_re[iter], QFORMAT19),
					(int32_t)calib_re[iter]);
				if(tas25xx_check_limits(iter, calib_re[iter]))
					pr_info("[TI-SmartPA:%s] Calibration Pass Channel No:%d", __func__, iter);
			}
			
			/*Calibration De-Init*/
			data = 1;//Value is ignored
			param_id = (TAS_SA_CALIB_DEINIT)|((iter+1)<<24)|(1<<16);
			ret = afe_smartamp_algo_ctrl((u8*)&data, param_id
				, TAS_SET_PARAM, sizeof(uint32_t));
			/*Wait some time*/
			msleep(200);
		}
	}
	tas25xx_save_calib_data(calib_re);

	calibration_status = 0;
	return;
}

static DEVICE_ATTR(calibration, 0664, tas25xx_calib_calibration_show,
				tas25xx_calib_calibration_store);

static DEVICE_ATTR(cstatus, 0664, tas25xx_calib_status_show,
				NULL);

static DEVICE_ATTR(rdc, 0664, tas25xx_calib_rdc_show,
				NULL);

static DEVICE_ATTR(temp, 0664, tas25xx_amb_temp_show,
				NULL);

static DEVICE_ATTR(cstatus_r, 0664, tas25xx_calib_status_show,
				NULL);

static DEVICE_ATTR(rdc_r, 0664, tas25xx_calib_rdc_show,
				NULL);

static DEVICE_ATTR(temp_r, 0664, tas25xx_amb_temp_show,
				NULL);

static ssize_t tas25xx_calib_calibration_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int32_t ret = 0;
	int32_t start;

	ret = kstrtos32(buf, 10, &start);
	if(ret)
	{
		pr_err("[TI-SmartPA:%s] Invalid input", __func__);
		goto end;
	}

	if(start)
	{	
		calibration_status = 1;
		/*Give time for algorithm to converge rdc*/
		schedule_delayed_work(&p_tas25xx_algo->calib_work,
				msecs_to_jiffies(200));
	}
end:
	return size;
}

static ssize_t tas25xx_calib_calibration_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	/*Enough to check for only one channel*/
	return sprintf(buf, "%s\n",
			(calibration_status) ? "Enabled" : "Disabled");
}

static ssize_t tas25xx_calib_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret = 0;

	if(attr == &dev_attr_cstatus_r)
		ret = sprintf(buf, "%d", calibration_result[1]);
	else
		ret = sprintf(buf, "%d", calibration_result[0]);

	return ret;
}

static ssize_t tas25xx_calib_rdc_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret;
	uint32_t calib_re = 0;

	if(attr == &dev_attr_rdc_r)
	{
		if(calibration_result[1] == STATUS_NONE)
		{
			if(p_tas25xx_algo->calib_update[1])
				calib_re = p_tas25xx_algo->calib_re[1];
			else
				tas25xx_get_efs_data(&calib_re, RDC_R);
		}
		else
			calib_re = calib_re_hold[1];

		ret = sprintf(buf, "%02d.%02d",
				(int32_t)trans_val_to_user_i(calib_re, QFORMAT19), (int32_t)trans_val_to_user_m(calib_re, QFORMAT19));
	}
	else
	{
		if(calibration_result[0] == STATUS_NONE)
		{
			if(p_tas25xx_algo->calib_update[0])
				calib_re = p_tas25xx_algo->calib_re[0];
			else
				tas25xx_get_efs_data(&calib_re, RDC_L);
		}
		else
			calib_re = calib_re_hold[0];

		ret = sprintf(buf, "%02d.%02d",
				(int32_t)trans_val_to_user_i(calib_re, QFORMAT19), (int32_t)trans_val_to_user_m(calib_re, QFORMAT19));
	}

	return ret;
}

static ssize_t tas25xx_amb_temp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret;
	uint32_t amb_temp = 0;

	if(attr == &dev_attr_temp_r)
	{
		if(calibration_result[1] == STATUS_NONE)
		{
			if(p_tas25xx_algo->calib_update[1])
				amb_temp = p_tas25xx_algo->amb_temp[1];
			else
				tas25xx_get_efs_data(&amb_temp, TEMP_R);
		}
		else
			amb_temp = amb_temp_hold[1];

		ret = sprintf(buf, "%d", amb_temp);
	}
	else
	{
		if(calibration_result[0] == STATUS_NONE)
		{
			if(p_tas25xx_algo->calib_update[0])
				amb_temp = p_tas25xx_algo->amb_temp[0];
			else
				tas25xx_get_efs_data(&amb_temp, TEMP_L);
		}
		else
			amb_temp = amb_temp_hold[0];

		ret = sprintf(buf, "%d", amb_temp);
	}

	return ret;
}

static struct attribute *tas25xx_calib_attr[] = {
	&dev_attr_calibration.attr,
	&dev_attr_cstatus.attr,
	&dev_attr_rdc.attr,
	&dev_attr_temp.attr,
};

static struct attribute *tas25xx_calib_attr_r[] = {
	&dev_attr_cstatus_r.attr,
	&dev_attr_rdc_r.attr,
	&dev_attr_temp_r.attr,
};

static struct attribute *tas25xx_calib_attr_m[
	ARRAY_SIZE(tas25xx_calib_attr) +
	ARRAY_SIZE(tas25xx_calib_attr_r) + 1] = {NULL};

static struct attribute_group tas25xx_calib_attr_grp = {
		.attrs = tas25xx_calib_attr_m,
};

/*******************************Calibration Related Codes End*************************************/
/*******************************Validation Related Codes Start*************************************/
static void valid_work_routine(struct work_struct *work)
{
	uint8_t iter = 0;
	uint32_t data = 0;
	uint32_t param_id = 0;
	int32_t ret = 0;

	//Get F0,Q
	for(iter = 0; iter < p_tas25xx_algo->spk_count; iter++)
	{
		data = 0;//Reset data to 0
		param_id = ((TAS_SA_GET_VALID_STATUS)|((iter+1)<<24)|(1<<16));
		ret = afe_smartamp_algo_ctrl((u8*)&data, param_id,
			TAS_GET_PARAM, sizeof(uint32_t));
		if (ret < 0) {
			validation_result[iter] = STATUS_FAIL;
			pr_info("[TI-SmartPA:%s]status read failed \n", __func__);
		} else {
			if (data == VALIDATION_SUCCESS){
				validation_result[iter] = STATUS_SUCCESS;
			}
			else{
				validation_result[iter] = STATUS_FAIL;
			}
		}
		pr_info("[TI-SmartPA:%s] Channel-%d", __func__, iter);
		pr_info("[TI-SmartPA:%s] validation_result %s(0x%x)\n",__func__,
			validation_result[iter] == STATUS_SUCCESS ? "Success":"Fail", (int32_t)data);
		
		/*De-Init the validation process*/
		data = 0;//Value is ignored
		param_id = (TAS_SA_VALID_DEINIT)|((iter+1)<<24)|(1<<16);
		ret = afe_smartamp_algo_ctrl((u8*)&data, param_id,
			TAS_SET_PARAM, sizeof(uint32_t));
	}
	validation_running_status = 0;
	return;
}

static DEVICE_ATTR(validation, 0644, tas25xx_valid_validation_show,
				tas25xx_valid_validation_store);

static DEVICE_ATTR(status, 0644, tas25xx_valid_status_show,
				NULL);

static DEVICE_ATTR(status_r, 0644, tas25xx_valid_status_show,
				NULL);

static ssize_t tas25xx_valid_validation_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{	
	uint8_t iter = 0;
	uint32_t data = 0;
	uint32_t param_id = 0;
	int32_t start = 0;
	int32_t ret = 0;

	ret = kstrtos32(buf, 10, &start);
	if(ret)
	{
		pr_err("[TI-SmartPA:%s] Invalid input", __func__);
		goto end;
	}

	if(start)
	{
		//Init
		for(iter = 0; iter < p_tas25xx_algo->spk_count; iter++)
		{
			data = 1;/*Value is ignored*/
			param_id = (TAS_SA_VALID_INIT)|((iter+1)<<24)|(1<<16);
			ret = afe_smartamp_algo_ctrl((u8*)&data, param_id
				, TAS_SET_PARAM, sizeof(uint32_t));
			if (ret < 0)
			{
				pr_err("[TI-SmartPA:%s] Init error. Exiting ..", __func__);
				goto end;
			}
		}

		//Give time for algorithm to converge V-sns level
		validation_running_status = 1;
		schedule_delayed_work(&p_tas25xx_algo->valid_work,
				msecs_to_jiffies(VALIDATION_TIME * 1000));
	}
end:
	return size;
}

static ssize_t tas25xx_valid_validation_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%s\n",
			(validation_running_status) ? "Enabled" : "Disabled");
}

static ssize_t tas25xx_valid_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret;

	if(attr == &dev_attr_status_r)
	{
		ret = sprintf(buf, "%d", validation_result[1]);
	}
	else
	{
		ret = sprintf(buf, "%d", validation_result[0]);
	}
	return ret;
}

static struct attribute *tas25xx_valid_attr[] = {
	&dev_attr_validation.attr,
	&dev_attr_status.attr,
};

static struct attribute *tas25xx_valid_attr_r[] = {
	&dev_attr_status_r.attr,
};

static struct attribute *tas25xx_valid_attr_m[
	ARRAY_SIZE(tas25xx_valid_attr) +
	ARRAY_SIZE(tas25xx_valid_attr_r) + 1] = {NULL};

static struct attribute_group tas25xx_valid_attr_grp = {
		.attrs = tas25xx_valid_attr_m,
};
/*******************************Validation Related Codes End*************************************/
/*******************************BigData Related Codes Start*************************************/

static DEVICE_ATTR(exc_max, 0664, tas25xx_bd_exc_max_show,
				NULL);

static DEVICE_ATTR(exc_max_persist, 0664, tas25xx_bd_exc_max_persist_show,
				NULL);

static DEVICE_ATTR(exc_over_count, 0664, tas25xx_bd_exc_over_count_show,
				NULL);

static DEVICE_ATTR(temp_max, 0664, tas25xx_bd_temp_max_show,
				NULL);

static DEVICE_ATTR(temp_max_persist, 0664, tas25xx_bd_temp_max_persist_show,
				NULL);

static DEVICE_ATTR(temp_over_count, 0664, tas25xx_bd_temp_over_count_show,
				NULL);

static DEVICE_ATTR(exc_max_r, 0664, tas25xx_bd_exc_max_show,
				NULL);

static DEVICE_ATTR(exc_max_persist_r, 0664, tas25xx_bd_exc_max_persist_show,
				NULL);

static DEVICE_ATTR(exc_over_count_r, 0664, tas25xx_bd_exc_over_count_show,
				NULL);

static DEVICE_ATTR(temp_max_r, 0664, tas25xx_bd_temp_max_show,
				NULL);

static DEVICE_ATTR(temp_max_persist_r, 0664, tas25xx_bd_temp_max_persist_show,
				NULL);

static DEVICE_ATTR(temp_over_count_r, 0664, tas25xx_bd_temp_over_count_show,
				NULL);

static ssize_t tas25xx_bd_exc_max_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret;

	if(attr == &dev_attr_exc_max_r)
	{
		ret = sprintf(buf, "%02d.%02d",
			(int32_t)trans_val_to_user_i(p_tas25xx_algo->b_data[1].exc_max, QFORMAT31),
			(int32_t)trans_val_to_user_m(p_tas25xx_algo->b_data[1].exc_max, QFORMAT31));
		p_tas25xx_algo->b_data[1].exc_max = 0;
	}
	else
	{
		ret = sprintf(buf, "%02d.%02d",
			(int32_t)trans_val_to_user_i(p_tas25xx_algo->b_data[0].exc_max, QFORMAT31),
			(int32_t)trans_val_to_user_m(p_tas25xx_algo->b_data[0].exc_max, QFORMAT31));
		p_tas25xx_algo->b_data[0].exc_max = 0;
	}

	return ret;
}

static ssize_t tas25xx_bd_exc_max_persist_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret;

	if(attr == &dev_attr_exc_max_persist_r)
	{
		ret = sprintf(buf, "%02d.%02d",
			(int32_t)trans_val_to_user_i(p_tas25xx_algo->b_data[1].exc_max_persist, QFORMAT31),
			(int32_t)trans_val_to_user_m(p_tas25xx_algo->b_data[1].exc_max_persist, QFORMAT31));
	}
	else
	{
		ret = sprintf(buf, "%02d.%02d",
			(int32_t)trans_val_to_user_i(p_tas25xx_algo->b_data[0].exc_max_persist, QFORMAT31),
			(int32_t)trans_val_to_user_m(p_tas25xx_algo->b_data[0].exc_max_persist, QFORMAT31));
	}

	return ret;
}

static ssize_t tas25xx_bd_exc_over_count_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret;

	if(attr == &dev_attr_exc_over_count_r)
	{
		ret = sprintf(buf, "%d",
			p_tas25xx_algo->b_data[1].exc_over_count);
	}
	else
	{
		ret = sprintf(buf, "%d",
			p_tas25xx_algo->b_data[0].exc_over_count);
	}

	return ret;
}

static ssize_t tas25xx_bd_temp_max_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret;

	if(attr == &dev_attr_temp_max_r)
	{
		ret = sprintf(buf, "%d",p_tas25xx_algo->b_data[1].temp_max);
		p_tas25xx_algo->b_data[1].temp_max = 0;
	}
	else
	{
		ret = sprintf(buf, "%d",p_tas25xx_algo->b_data[0].temp_max);
		p_tas25xx_algo->b_data[0].temp_max = 0;
	}

	return ret;
}

static ssize_t tas25xx_bd_temp_max_persist_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret;

	if(attr == &dev_attr_temp_max_persist_r)
	{
		ret = sprintf(buf, "%d",p_tas25xx_algo->b_data[1].temp_max_persist);
	}
	else
	{
		ret = sprintf(buf, "%d",p_tas25xx_algo->b_data[0].temp_max_persist);
	}

	return ret;
}

static ssize_t tas25xx_bd_temp_over_count_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t ret;

	if(attr == &dev_attr_temp_over_count_r)
	{
		ret = sprintf(buf, "%d", p_tas25xx_algo->b_data[1].temp_over_count);
	}
	else
	{
		ret = sprintf(buf, "%d", p_tas25xx_algo->b_data[0].temp_over_count);
	}

	return ret;
}

static struct attribute *tas25xx_bd_attr[] = {
	&dev_attr_exc_max.attr,
	&dev_attr_exc_max_persist.attr,
	&dev_attr_exc_over_count.attr,
	&dev_attr_temp_max.attr,
	&dev_attr_temp_max_persist.attr,
	&dev_attr_temp_over_count.attr,
};

static struct attribute *tas25xx_bd_attr_r[] = {
	&dev_attr_exc_max_r.attr,
	&dev_attr_exc_max_persist_r.attr,
	&dev_attr_exc_over_count_r.attr,
	&dev_attr_temp_max_r.attr,
	&dev_attr_temp_max_persist_r.attr,
	&dev_attr_temp_over_count_r.attr,
};

static struct attribute *tas25xx_bd_attr_m[
	ARRAY_SIZE(tas25xx_bd_attr) +
	ARRAY_SIZE(tas25xx_bd_attr_r) + 1] = {NULL};

static struct attribute_group tas25xx_bd_attr_grp = {
	.attrs = tas25xx_bd_attr_m,
};

/*******************************BigData Related Codes End*************************************/

static void clean_up_tas_sysfs (void)
{
	if (p_tas25xx_algo) {

		if (p_tas25xx_algo->calib_dev) {
			sysfs_remove_group (&p_tas25xx_algo->calib_dev->kobj, &tas25xx_calib_attr_grp);
			device_destroy (p_tas25xx_algo->algo_class, 1);
		}

		if (p_tas25xx_algo->valid_dev) {
			sysfs_remove_group (&p_tas25xx_algo->valid_dev->kobj, &tas25xx_valid_attr_grp);
			device_destroy (p_tas25xx_algo->algo_class, 1);
		}

		if (p_tas25xx_algo->bd_dev) {
			sysfs_remove_group (&p_tas25xx_algo->bd_dev->kobj, &tas25xx_bd_attr_grp);
			device_destroy (p_tas25xx_algo->algo_class, 1);
		}

		if (p_tas25xx_algo->algo_class) {
			class_destroy (p_tas25xx_algo->algo_class);
		}

		kfree (p_tas25xx_algo);
		p_tas25xx_algo = NULL;
	}
}

static void update_dts_info(void)
{
	if(!p_tas25xx_algo)
	{
		pr_err("[TI-SmartPA:%s] memory not allocated yet for p_tas25xx_algo",
			__func__);
	}

	p_tas25xx_algo->port = port;
}

void tas25xx_parse_algo_dt(struct device_node *np)
{
	uint32_t data = 0;
	int32_t ret = 0;

	ret = of_property_read_u32(np, "ti,port_id", &data);
	if (ret) {
		pr_err("[TI-SmartPA:%s] Looking up %s property in node %s failed %d\n",
			__func__, "ti,port_id", np->full_name, ret);
	} else {
		port = data;
		pr_err("[TI-SmartPA:%s] ti,port_id=0x%x",
			__func__, data);
	}

	return;
}


void smartamp_add_algo(uint8_t channels)
{
	int32_t ret = 0;
	pr_err("[TI-SmartPA:%s] Adding Smartamp algo functions, spk_count=%d",
			__func__, channels);

	mutex_init(&routing_lock);

	p_tas25xx_algo = kzalloc(sizeof(struct tas25xx_algo), GFP_KERNEL);
	if (p_tas25xx_algo == NULL)
	{
		pr_err("[TI-SmartPA:%s] No Memory", __func__);
		return;
	}

	p_tas25xx_algo->spk_count = channels;

	memcpy(tas25xx_calib_attr_m, tas25xx_calib_attr, sizeof(tas25xx_calib_attr));
	memcpy(tas25xx_valid_attr_m, tas25xx_valid_attr, sizeof(tas25xx_valid_attr));
	memcpy(tas25xx_bd_attr_m, tas25xx_bd_attr, sizeof(tas25xx_bd_attr));
	if(channels == 2)
	{
		memcpy(tas25xx_calib_attr_m + ARRAY_SIZE(tas25xx_calib_attr),
			tas25xx_calib_attr_r, sizeof(tas25xx_calib_attr_r));
		memcpy(tas25xx_valid_attr_m + ARRAY_SIZE(tas25xx_valid_attr),
			tas25xx_valid_attr_r, sizeof(tas25xx_valid_attr_r));
		memcpy(tas25xx_bd_attr_m + ARRAY_SIZE(tas25xx_bd_attr),
			tas25xx_bd_attr_r, sizeof(tas25xx_bd_attr_r));
	}

	update_dts_info();

	p_tas25xx_algo->algo_class = class_create(THIS_MODULE, TAS25XX_SYSFS_CLASS_NAME);
	if (IS_ERR(p_tas25xx_algo->algo_class)) {
		ret = PTR_ERR(p_tas25xx_algo->algo_class);
		pr_err("[TI-SmartPA:%s] err class create\n", __func__);
		p_tas25xx_algo->algo_class = NULL;
		goto err_dev;
	}

	p_tas25xx_algo->calib_dev = device_create(p_tas25xx_algo->algo_class, NULL, 1, NULL, TAS25XX_CALIB_DIR_NAME);
	if (IS_ERR(p_tas25xx_algo->calib_dev)) {
		pr_err("[TI-SmartPA:%s]Failed to create calib_dev\n", __func__);
		ret = PTR_ERR(p_tas25xx_algo->calib_dev);
		p_tas25xx_algo->calib_dev = NULL;
		goto err_dev;
	}

	ret = sysfs_create_group(&p_tas25xx_algo->calib_dev->kobj, &tas25xx_calib_attr_grp);
	if (ret) {
		pr_err("[TI-SmartPA:%s]Failed to create sysfs group\n", __func__);
		goto err_dev;
	}

	INIT_DELAYED_WORK(&p_tas25xx_algo->calib_work, calib_work_routine);

	p_tas25xx_algo->valid_dev = device_create(p_tas25xx_algo->algo_class, NULL, 1, NULL,
			TAS25XX_VALID_DIR_NAME);
	if (IS_ERR(p_tas25xx_algo->valid_dev)) {
		pr_err("[TI-SmartPA:%s]Failed to create valid_dev\n", __func__);
		ret = PTR_ERR(p_tas25xx_algo->valid_dev);
		p_tas25xx_algo->valid_dev = NULL;
		goto err_dev;
	}

	ret = sysfs_create_group(&p_tas25xx_algo->valid_dev->kobj, &tas25xx_valid_attr_grp);
	if (ret) {
		pr_err("[TI-SmartPA:%s]Failed to create sysfs group\n", __func__);
		p_tas25xx_algo->valid_dev = NULL;
		goto err_dev;
	}

	INIT_DELAYED_WORK(&p_tas25xx_algo->valid_work, valid_work_routine);

	p_tas25xx_algo->bd_dev = device_create(p_tas25xx_algo->algo_class, NULL, 1, NULL,
			TAS25XX_BD_DIR_NAME);
	if (IS_ERR(p_tas25xx_algo->bd_dev)) {
		pr_err("[TI-SmartPA:%s]Failed to create bd_dev\n", __func__);
		ret = PTR_ERR(p_tas25xx_algo->bd_dev);
		p_tas25xx_algo->bd_dev = NULL;
		goto err_dev;
	}

	ret = sysfs_create_group(&p_tas25xx_algo->bd_dev->kobj, &tas25xx_bd_attr_grp);
	if (ret) {
		pr_err("[TI-SmartPA:%s]Failed to create sysfs group\n", __func__);
		p_tas25xx_algo->bd_dev = NULL;
		goto err_dev;
	}

	pr_info ("[TI-SmartPA:%s] ret=%d", __func__, ret);
	return;

err_dev:
	clean_up_tas_sysfs();
	pr_err("[TI-SmartPA:%s] Error %d", __func__, ret);
}

void smartamp_remove_algo(void)
{
	pr_info("[TI-SmartPA:%s] Removing Smartamp Algorithm functions", __func__);
	clean_up_tas_sysfs();
	mutex_destroy(&routing_lock);
}

MODULE_AUTHOR("Texas Instruments Inc.");
MODULE_DESCRIPTION("TAS25XX Algorithm");
MODULE_LICENSE("GPL v2");

#endif /*CONFIG_TAS25XX_ALGO*/
