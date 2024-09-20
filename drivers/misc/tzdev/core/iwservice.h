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

#ifndef __TZ_IWSERVICE_H__
#define __TZ_IWSERVICE_H__

int tz_iwservice_init(void);
unsigned long tz_iwservice_get_cpu_mask(void);
unsigned long tz_iwservice_get_user_cpu_mask(void);

#endif /* __TZ_IWSERVICE_H__ */
