/*
 * Samsung Exynos5 SoC series Sensor driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_CIS_3L6_H
#define FIMC_IS_CIS_3L6_H

#include "fimc-is-cis.h"

#define EXT_CLK_Mhz (26)

#ifdef USE_3L6B_SETFILE
#define SENSOR_3L6_MAX_WIDTH		(4128)
#define SENSOR_3L6_MAX_HEIGHT		(3096)
#else
#define SENSOR_3L6_MAX_WIDTH		(4000)
#define SENSOR_3L6_MAX_HEIGHT		(3000)
#endif

/* TODO: Check below values are valid */
#define SENSOR_3L6_FINE_INTEGRATION_TIME_MIN                0x1C5
#define SENSOR_3L6_FINE_INTEGRATION_TIME_MAX                0x1C5
#define SENSOR_3L6_COARSE_INTEGRATION_TIME_MIN              0x2
#define SENSOR_3L6_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x4

#define USE_GROUP_PARAM_HOLD	(0)

/****
 **  Register Address
 **  : address name format: SENSOR_3L6_XX...XX_ADDR
 ****/
#define SENSOR_3L6_MODEL_ID_ADDR		(0x0000)
#define SENSOR_3L6_REVISION_NUMBER_ADDR		(0x0002)
#define SENSOR_3L6_FRAME_COUNT_ADDR		(0x0005)
#define SENSOR_3L6_MODE_SELECT_ADDR		(0x0100)
#define SENSOR_3L6_GROUP_PARAM_HOLD_ADDR	(0x0104)
#define SENSOR_3L6_COARSE_INTEGRATION_TIME_ADDR	(0x0202)
#define SENSOR_3L6_ANALOG_GAIN_ADDR		(0x0204)
#define SENSOR_3L6_DIGITAL_GAIN_ADDR		(0x020E)
#define SENSOR_3L6_FRAME_LENGTH_LINE_ADDR	(0x0340)
#define SENSOR_3L6_X_ADDR_START_ADDR		(0x0344)
#define SENSOR_3L6_Y_ADDR_START_ADDR		(0x0346)
#define SENSOR_3L6_X_ADDR_END_ADDR		(0x0348)
#define SENSOR_3L6_Y_ADDR_END_ADDR		(0x034A)
#define SENSOR_3L6_X_OUTPUT_SIZE_ADDR		(0x034C)
#define SENSOR_3L6_Y_OUTPUT_SIZE_ADDR		(0x034E)
#define SENSOR_3L6_X_EVEN_INC_ADDR		(0x0380)
#define SENSOR_3L6_X_ODD_INC_ADDR		(0x0382)
#define SENSOR_3L6_Y_EVEN_INC_ADDR		(0x0384)
#define SENSOR_3L6_Y_ODD_INC_ADDR		(0x0386)
#define SENSOR_3L6_SCALING_MODE_ADDR		(0x0400)
#define SENSOR_3L6_DOWN_SCALE_M_ADDR		(0x0404)
#define SENSOR_3L6_BINNING_MODE_ADDR		(0x0900)
#define SENSOR_3L6_BINNING_TYPE_ADDR		(0x0901)
#define SENSOR_3L6_PLL_POWER_CONTROL_ADDR	(0x3C1E)
#define SENSOR_3L6_PAGE_SELECT_ADDR		(0x6028)

#ifdef USE_3L6B_SETFILE
/*Apply the same order as in fimc-is-cis-3l6-setX.h file*/
enum sensor_3l6_mode_enum {
	SENSOR_3L6_MODE_4128x3096_30FPS,
	SENSOR_3L6_MODE_4128x2324_30FPS,
	SENSOR_3L6_MODE_2064x1548_30FPS,
	SENSOR_3L6_MODE_2064x1160_30FPS,
	SENSOR_3L6_MODE_1024x768_120FPS,
};
#else
/*Apply the same order as in fimc-is-cis-3l6-setX.h file*/
enum sensor_3l6_mode_enum {
	SENSOR_3L6_MODE_4000x3000_30FPS,
	SENSOR_3L6_MODE_4000x2256_30FPS,
	SENSOR_3L6_MODE_2000x1500_30FPS,
	SENSOR_3L6_MODE_2000x1124_30FPS,
	SENSOR_3L6_MODE_992x744_120FPS,
};
#endif

#endif

