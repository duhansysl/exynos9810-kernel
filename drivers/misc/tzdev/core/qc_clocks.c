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

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "tzdev_internal.h"
#include "core/log.h"
#include "core/qc_clocks.h"

#define QSEE_CE_CLK_100MHZ	100000000

static struct clk *tzdev_qc_core_src = NULL;
static struct clk *tzdev_qc_core_clk = NULL;
static struct clk *tzdev_qc_iface_clk = NULL;
static struct clk *tzdev_qc_bus_clk = NULL;
static DEFINE_MUTEX(tzdev_qc_clk_mutex);
static int tzdev_qc_clk_users = 0;

int tzdev_qc_clock_init(struct device *dev)
{
	int ret = 0;
	int freq_val = 0;
#if defined(CONFIG_ARCH_MSM8939)
	const char *prop_name = "qcom,freq-val";
#else /* CONFIG_ARCH_MSM8939 */
	const char *prop_name = "qcom,ce-opp-freq";
#endif /* CONFIG_ARCH_MSM8939 */

	tzdev_qc_core_src = clk_get(dev, "core_clk_src");
	if (IS_ERR(tzdev_qc_core_src)) {
		ret = PTR_ERR(tzdev_qc_core_src);
		log_error(tzdev_platform, "no tzdev_qc_core_src, ret = %d\n", ret);
		goto error;
	}
	if (of_property_read_u32(dev->of_node, prop_name, &freq_val)) {
		freq_val = QSEE_CE_CLK_100MHZ;
		log_error(tzdev_platform, "unable to get frequency value from \"%s\" property. " \
				"set default: %d\n", prop_name, freq_val);
	}
	ret = clk_set_rate(tzdev_qc_core_src, freq_val);
	if (ret) {
		log_error(tzdev_platform, "clk_set_rate failed, ret = %d\n", ret);
		ret = -EIO;
		goto put_core_src_clk;
	}

	tzdev_qc_core_clk = clk_get(dev, "core_clk");
	if (IS_ERR(tzdev_qc_core_clk)) {
		log_error(tzdev_platform, "no tzdev_qc_core_clk\n");
		ret = PTR_ERR(tzdev_qc_core_clk);
		goto clear_core_clk;
	}
	tzdev_qc_iface_clk = clk_get(dev, "iface_clk");
	if (IS_ERR(tzdev_qc_iface_clk)) {
		log_error(tzdev_platform, "no tzdev_qc_iface_clk\n");
		ret = PTR_ERR(tzdev_qc_iface_clk);
		goto put_core_clk;
	}
	tzdev_qc_bus_clk = clk_get(dev, "bus_clk");
	if (IS_ERR(tzdev_qc_bus_clk)) {
		log_error(tzdev_platform, "no tzdev_qc_bus_clk\n");
		ret = PTR_ERR(tzdev_qc_bus_clk);
		goto put_iface_clk;
	}
	tzdev_set_nwd_sysconf_flag(SYSCONF_NWD_CRYPTO_CLOCK_MANAGEMENT);

	log_info(tzdev_platform, "Got QC HW crypto clks\n");
	return ret;

put_iface_clk:
	clk_put(tzdev_qc_iface_clk);
	tzdev_qc_bus_clk = NULL;

put_core_clk:
	clk_put(tzdev_qc_core_clk);
	tzdev_qc_iface_clk = NULL;

clear_core_clk:
	tzdev_qc_core_clk = NULL;

put_core_src_clk:
	clk_put(tzdev_qc_core_src);

error:
	tzdev_qc_core_src = NULL;

	return ret;
}

void tzdev_qc_clock_fini(void)
{
	clk_put(tzdev_qc_bus_clk);
	clk_put(tzdev_qc_iface_clk);
	clk_put(tzdev_qc_core_clk);
	clk_put(tzdev_qc_core_src);
}

int tzdev_qc_clock_enable(void)
{
	int ret;

	mutex_lock(&tzdev_qc_clk_mutex);

	ret = clk_prepare_enable(tzdev_qc_core_clk);
	if (ret) {
		log_error(tzdev_platform, "failed to enable core clk, ret=%d\n", ret);
		goto out;
	}

	ret = clk_prepare_enable(tzdev_qc_iface_clk);
	if (ret) {
		log_error(tzdev_platform, "failed to enable iface clk, ret=%d\n", ret);
		goto unprepare_core_clk;
	}

	ret = clk_prepare_enable(tzdev_qc_bus_clk);
	if (ret) {
		log_error(tzdev_platform, "failed to enable bus clk, ret=%d\n\n", ret);
		goto unprepare_iface_clk;
	}

	tzdev_qc_clk_users++;

	goto out;

unprepare_iface_clk:
	clk_disable_unprepare(tzdev_qc_iface_clk);
unprepare_core_clk:
	clk_disable_unprepare(tzdev_qc_core_clk);
out:
	mutex_unlock(&tzdev_qc_clk_mutex);

	return ret;
}

void tzdev_qc_clock_disable(void)
{
	mutex_lock(&tzdev_qc_clk_mutex);

	tzdev_qc_clk_users--;
	BUG_ON(tzdev_qc_clk_users < 0);

	clk_disable_unprepare(tzdev_qc_iface_clk);
	clk_disable_unprepare(tzdev_qc_core_clk);
	clk_disable_unprepare(tzdev_qc_bus_clk);

	mutex_unlock(&tzdev_qc_clk_mutex);
}

unsigned int tzdev_is_qc_clock_enabled(void)
{
	return tzdev_qc_clk_users ? 1 : 0;
}

#if defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_USR_MNG)
int tzdev_qc_set_clk(struct tzdev_qc_clock_data *data, unsigned int state)
{
	int ret = 0;

	mutex_lock(&data->mutex);

	switch (state) {
	case TZIO_CRYPTO_CLOCK_ON:
		if (!data->clk_state) {
			ret = tzdev_qc_clock_enable();
			if (!ret)
				data->clk_state = 1;
		} else {
			log_error(tzdev_platform, "Trying to enable crypto clocks twice\n");
			ret = -EBUSY;
		}
		break;
	case TZIO_CRYPTO_CLOCK_OFF:
		if (data->clk_state) {
			tzdev_qc_clock_disable();
			data->clk_state = 0;
		} else {
			log_error(tzdev_platform, "Trying to disable crypto clocks twice\n");
			ret = -EBUSY;
		}
		break;
	default:
		log_error(tzdev_platform, "Unknown crypto clocks request, state=%u\n", state);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&data->mutex);

	return ret;
}
#endif /* CONFIG_TZDEV_QC_CRYPTO_CLOCKS_USR_MNG */
