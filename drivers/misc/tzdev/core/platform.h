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

#ifndef __TZ_PLATFORM_H__
#define __TZ_PLATFORM_H__

#include "tzdev_internal.h"

int tzdev_platform_register(void);
void tzdev_platform_unregister(void);
void *tzdev_platform_open(void);
void tzdev_platform_release(void *data);
long tzdev_platform_ioctl(void *data, unsigned int cmd, unsigned long arg);
int tzdev_platform_smc_call(struct tzdev_smc_data *data);

#endif /* __TZ_PLATFORM_H__ */
