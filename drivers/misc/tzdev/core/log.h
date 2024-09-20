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

#ifndef __TZLOG_H__
#define __TZLOG_H__

#include "tzdev_internal.h"

extern unsigned int tzdev_verbosity;
extern unsigned int tzdev_platform_verbosity;
extern unsigned int tzdev_iwlog_verbosity;
extern unsigned int tzdev_iwio_verbosity;
extern unsigned int tzdev_iwservice_verbosity;
extern unsigned int tzdev_hotplug_verbosity;
extern unsigned int tzdev_boost_verbosity;
extern unsigned int tzdev_panic_dump_verbosity;
extern unsigned int tzdev_scma_verbosity;
extern unsigned int tzdev_teec_verbosity;
extern unsigned int tzdev_kthread_verbosity;
extern unsigned int tzdev_iwsock_verbosity;
extern unsigned int tzdev_uiwsock_verbosity;
extern unsigned int tzdev_deploy_tzar_verbosity;
extern unsigned int tzdev_mem_verbosity;
extern unsigned int tzdev_profiler_verbosity;
extern unsigned int tzdev_nwfs_verbosity;
extern unsigned int tzdev_pmf_verbosity;

enum {
	TZDEV_LOG_LEVEL_ERROR,
	TZDEV_LOG_LEVEL_INFO,
	TZDEV_LOG_LEVEL_DEBUG,
};

static inline const char *tz_print_prefix(unsigned int lvl)
{
	switch (lvl) {
	case TZDEV_LOG_LEVEL_ERROR:
		return "ERR";
	case TZDEV_LOG_LEVEL_INFO:
		return "INF";
	case TZDEV_LOG_LEVEL_DEBUG:
		return "DBG";
	default:
		return "???";
	}
}

#define tz_print(lvl, param, fmt, ...)							\
	do {										\
		if (lvl <= param)							\
			printk("TZDEV %s %s(%d): "fmt,					\
					tz_print_prefix(lvl),				\
					__func__, __LINE__, ##__VA_ARGS__);		\
	} while (0)

#define log_debug(module, fmt, ...)	tz_print(TZDEV_LOG_LEVEL_DEBUG, module##_verbosity, fmt, ##__VA_ARGS__)
#define log_info(module, fmt, ...)	tz_print(TZDEV_LOG_LEVEL_INFO, module##_verbosity, fmt, ##__VA_ARGS__)
#define log_error(module, fmt, ...)	tz_print(TZDEV_LOG_LEVEL_ERROR, module##_verbosity, fmt, ##__VA_ARGS__)

#endif /* __TZLOG_H__ */
