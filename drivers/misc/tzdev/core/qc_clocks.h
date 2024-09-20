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

#ifndef __TZ_QC_CLOCKS_H__
#define __TZ_QC_CLOCKS_H__

#include <linux/device.h>
#include <linux/mutex.h>

struct tzdev_qc_clock_data {
	unsigned int clk_state;
	struct mutex mutex;
};

#ifdef CONFIG_TZDEV_QC_CRYPTO_CLOCKS_MANAGEMENT

int tzdev_qc_clock_init(struct device *dev);
void tzdev_qc_clock_fini(void);
int tzdev_qc_clock_enable(void);
void tzdev_qc_clock_disable(void);
unsigned int tzdev_is_qc_clock_enabled(void);

#ifdef CONFIG_TZDEV_QC_CRYPTO_CLOCKS_USR_MNG

int tzdev_qc_set_clk(struct tzdev_qc_clock_data *data, unsigned int state);

#else /* CONFIG_TZDEV_QC_CRYPTO_CLOCKS_USR_MNG */

static inline int tzdev_qc_set_clk(struct tzdev_qc_clock_data *data, unsigned int state)
{
	(void) data;
	(void) state;

	return -ENOSYS;
}

#endif /* CONFIG_TZDEV_QC_CRYPTO_CLOCKS_USR_MNG */

#else /* CONFIG_TZDEV_QC_CRYPTO_CLOCKS_MANAGEMENT */

static inline int tzdev_qc_clock_initialize(void)
{
	return 0;
}

static inline void tzdev_qc_clock_finalize(void)
{
	return;
}

static inline int tzdev_qc_clock_enable(void)
{
	return 0;
}

static inline void tzdev_qc_clock_disable(void)
{
	return;
}

static inline int tzdev_qc_set_clk(struct tzdev_qc_clock_data *data, unsigned int state)
{
	(void) data;
	(void) state;

	return -ENOSYS;
}

static inline unsigned int tzdev_is_qc_clock_enabled(void)
{
	return 0;
}

#endif /* CONFIG_TZDEV_QC_CRYPTO_CLOCKS_MANAGEMENT */

#endif /* __TZ_QC_CLOCKS_H__ */
