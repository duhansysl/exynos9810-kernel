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

#pragma once

#include <teec/pmf_defs.h>

#ifdef CONFIG_TZ_PMF
void tzdev_pmf_stamp(enum pmf_id id);
#else
void tzdev_pmf_stamp(enum pmf_id id)
{
	(void) id;
}
#endif /* CONFIG_TZ_PMF */
