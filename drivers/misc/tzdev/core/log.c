/*
 * Copyright (C) 2012-2019, Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>

#include "core/log.h"

#define TZDEV_DEBUG_PARAM_DECLARE(param, value)					\
	unsigned int param##_verbosity = value;					\
	module_param(param##_verbosity, uint, 0644);				\
	MODULE_PARM_DESC(param_##verbosity, "0: error, 1: info, 2: debug")

TZDEV_DEBUG_PARAM_DECLARE(tzdev, 0);
TZDEV_DEBUG_PARAM_DECLARE(tzdev_platform, 0);
TZDEV_DEBUG_PARAM_DECLARE(tzdev_iwio, 0);
TZDEV_DEBUG_PARAM_DECLARE(tzdev_iwservice, 0);
TZDEV_DEBUG_PARAM_DECLARE(tzdev_teec, 0);
TZDEV_DEBUG_PARAM_DECLARE(tzdev_kthread, 0);
TZDEV_DEBUG_PARAM_DECLARE(tzdev_iwsock, 0);
TZDEV_DEBUG_PARAM_DECLARE(tzdev_uiwsock, 0);

#if defined(CONFIG_TZLOG)
TZDEV_DEBUG_PARAM_DECLARE(tzdev_iwlog, 0);
#endif
#if defined(CONFIG_TZDEV_HOTPLUG)
TZDEV_DEBUG_PARAM_DECLARE(tzdev_hotplug, 0);
#endif
#if defined(CONFIG_TZDEV_BOOST)
TZDEV_DEBUG_PARAM_DECLARE(tzdev_boost, 0);
#endif
#if defined(CONFIG_TZ_PANIC_DUMP)
TZDEV_DEBUG_PARAM_DECLARE(tzdev_panic_dump, 0);
#endif
#if defined(CONFIG_TZ_SCMA)
TZDEV_DEBUG_PARAM_DECLARE(tzdev_scma, 0);
#endif
#if defined(CONFIG_TZDEV_DEPLOY_TZAR)
TZDEV_DEBUG_PARAM_DECLARE(tzdev_deploy_tzar, 0);
#endif
TZDEV_DEBUG_PARAM_DECLARE(tzdev_mem, 0);
#if defined(CONFIG_TZPROFILER)
TZDEV_DEBUG_PARAM_DECLARE(tzdev_profiler, 0);
#endif
#if defined(CONFIG_TZ_NWFS)
TZDEV_DEBUG_PARAM_DECLARE(tzdev_nwfs, 0);
#endif
#if defined(CONFIG_TZ_PMF)
TZDEV_DEBUG_PARAM_DECLARE(tzdev_pmf, 0);
#endif
