/*
** =============================================================================
** Copyright (c) 2016  Texas Instruments Inc.
**
** This program is free software; you can redistribute it and/or modify it under
** the terms of the GNU General Public License as published by the Free Software
** Foundation; version 2.
**
** This program is distributed in the hope that it will be useful, but WITHOUT
** ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
**
** File:
**     tas25xx-algo.h
**
** Description:
**     header file for tas25xx-algo.c
**
** =============================================================================
*/

#ifndef _TAS25XX_ALGO_H
#define _TAS25XX_ALGO_H

#include <linux/debugfs.h>

#define TRANSF_USER_TO_IMPED(X, Y) \
		((X << 19) + ((Y << 19) / 100))
#define QFORMAT19				19
#define QFORMAT31				31

#define MAX_STRING		200
#define TAS25XX_SYSFS_CLASS_NAME	"tas25xx"
#define TAS25XX_CALIB_DIR_NAME		"calib"
#define TAS25XX_VALID_DIR_NAME		"valid"
#define TAS25XX_BD_DIR_NAME			"bigdata"

#define TAS25XX_EFS_CALIB_DATA_L	"/efs/tas25xx/calib_re"
#define TAS25XX_EFS_TEMP_DATA_L		"/efs/tas25xx/amb_temp"
#define TAS25XX_EFS_CALIB_DATA_R	"/efs/tas25xx/calib_re_r"
#define TAS25XX_EFS_TEMP_DATA_R		"/efs/tas25xx/amb_temp_r"

#define CALIB_RETRY_COUNT		5
#define RDC_L					0
#define TEMP_L					1
#define RDC_R					2
#define TEMP_R					3
#define DEFAULT_AMBIENT_TEMP	25
#define CALIB_TIME				2
#define VALIDATION_TIME			3
#define STATUS_NONE				0x00
#define STATUS_SUCCESS			0x01
#define STATUS_FAIL				0xcc

struct big_data
{
	uint32_t exc_max;
	uint32_t exc_max_persist;
	uint32_t exc_over_count;
	uint32_t temp_max;
	uint32_t temp_max_persist;
	uint32_t temp_over_count;
};

struct tas25xx_algo
{
	struct class *algo_class;
	struct device *calib_dev;
	struct device *valid_dev;
	struct device *bd_dev;
	struct big_data b_data[MAX_CHANNELS];
	struct delayed_work calib_work;
	struct delayed_work valid_work;
	uint8_t spk_count;
	uint32_t port;
	uint32_t calib_re[MAX_CHANNELS];
	uint32_t amb_temp[MAX_CHANNELS];
	bool calib_update[MAX_CHANNELS];
};


static ssize_t tas25xx_calib_calibration_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size);
					
static ssize_t tas25xx_calib_calibration_show(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t tas25xx_calib_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t tas25xx_calib_rdc_show(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t tas25xx_amb_temp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t tas25xx_valid_validation_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size);

static ssize_t tas25xx_valid_validation_show(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t tas25xx_valid_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t tas25xx_bd_exc_max_show(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t tas25xx_bd_exc_max_persist_show(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t tas25xx_bd_exc_over_count_show(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t tas25xx_bd_temp_max_show(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t tas25xx_bd_temp_max_persist_show(struct device *dev,
					struct device_attribute *attr,
					char *buf);

static ssize_t tas25xx_bd_temp_over_count_show(struct device *dev,
					struct device_attribute *attr,
					char *buf);

#endif /* _TAS25XX_ALGO_H */