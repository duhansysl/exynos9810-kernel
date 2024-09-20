/*
 * Copyright (C) 2012-2019 Samsung Electronics, Inc.
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

#ifndef __TZ_BOOST_H__
#define __TZ_BOOST_H__

#ifdef CONFIG_TZDEV_BOOST
void tz_boost_enable(void);
void tz_boost_disable(void);
#else
static inline void tz_boost_enable(void)
{
}

static inline void tz_boost_disable(void)
{
}
#endif /* CONFIG_TZDEV_BOOST */

#endif /* __TZ_BOOST_H__ */
