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

#ifndef __TZ_PANIC_DUMP_H__
#define __TZ_PANIC_DUMP_H__

#if defined(CONFIG_TZ_PANIC_DUMP)
int tz_panic_dump_init(void);
#else
static inline int tz_panic_dump_init(void)
{
	return 0;
}
#endif

#endif /* __TZ_PANIC_DUMP_H__ */
