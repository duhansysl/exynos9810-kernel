/*
 * Samsung Exynos SoC series Sensor driver
 *
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_CIS_2L2_H
#define FIMC_IS_CIS_2L2_H

#include "fimc-is-cis.h"

#define EXT_CLK_Mhz (26)

#define SENSOR_2L2_MAX_WIDTH		(4032 + 0)
#define SENSOR_2L2_MAX_HEIGHT		(3024 + 0)

#define SENSOR_2L2_FINE_INTEGRATION_TIME_MIN                0x100
#define SENSOR_2L2_FINE_INTEGRATION_TIME_MAX                0x100
#define SENSOR_2L2_COARSE_INTEGRATION_TIME_MIN              0x04
#define SENSOR_2L2_COARSE_INTEGRATION_TIME_MAX_MARGIN       0x08

#define USE_GROUP_PARAM_HOLD	(0)

enum sensor_2l2_mode_enum {
	/* MODE 3 */
	SENSOR_2L2_4032X3024_30FPS = 0,		/* 16 */
	SENSOR_2L2_4032X2268_60FPS,			/* 14 */
	SENSOR_2L2_4032X2268_30FPS,			/* 17 */
	SENSOR_2L2_2016X1512_30FPS,			/* 18 */
	SENSOR_2L2_2016X1136_30FPS,			/* 19 */
	/* MODE 3 - 24fps LIVE FOCUS */
	SENSOR_2L2_4032X3024_24FPS,
	SENSOR_2L2_4032X2268_24FPS,
	/* MODE 2 */
	SENSOR_2L2_2016X1134_240FPS_MODE2, 	/* 07 */
	SENSOR_2L2_1008X756_120FPS_MODE2,	/* 08 */
	SENSOR_2L2_MODE_MAX,
};

static bool sensor_2l2_support_wdr[] = {
	/* MODE 3 */
	true, //SENSOR_2L2_4032X3024_30FPS
	true, //SENSOR_2L2_4032X2268_60FPS
	true, //SENSOR_2L2_4032X2268_30FPS
	false, //SENSOR_2L2_2016X1512_30FPS
	false, //SENSOR_2L2_2016X1136_30FPS
	/* MODE 3 - 24fps LIVE FOCUS */
	true, //SENSOR_2L2_4032X3024_24FPS = 4,
	true, //SENSOR_2L2_4032X2268_24FPS = 5,
	/* MODE 2 */
	false, //SENSOR_2L2_2016X1134_240FPS_MODE2 = 6,
	false, //SENSOR_2L2_1008X756_120FPS_MODE2 = 7,
};

#endif

