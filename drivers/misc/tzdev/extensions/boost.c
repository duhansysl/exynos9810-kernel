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

#include <linux/atomic.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pm_qos.h>

#include "tzdev_internal.h"
#include "core/cdev.h"
#include "core/log.h"
#include "core/notifier.h"
#include "extensions/hotplug.h"

#if defined(TZDEV_BOOST_CLUSTER_1)

/* cluster 1 is a high performance cluster */
#ifndef PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE
#define TZ_BOOST_CPU_FREQ_MAX_DEFAULT_VALUE	INT_MAX
#else
#define TZ_BOOST_CPU_FREQ_MAX_DEFAULT_VALUE	PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE
#endif

#define TZ_BOOST_CPU_FREQ_MIN			PM_QOS_CLUSTER1_FREQ_MIN

#elif defined(TZDEV_BOOST_CLUSTER_2)

/* cluster 2 is a high performance cluster */
#ifndef PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE
#define TZ_BOOST_CPU_FREQ_MAX_DEFAULT_VALUE	INT_MAX
#else
#define TZ_BOOST_CPU_FREQ_MAX_DEFAULT_VALUE	PM_QOS_CLUSTER2_FREQ_MAX_DEFAULT_VALUE
#endif

#define TZ_BOOST_CPU_FREQ_MIN			PM_QOS_CLUSTER2_FREQ_MIN

#endif /* TZDEV_BOOST_CLUSTER */

static int tz_boost_users = 0;
static struct pm_qos_request tz_boost_qos;
static unsigned int cpu_boost_mask;
static DEFINE_MUTEX(tz_boost_lock);

static int tz_boost_init_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	(void)cb;
	(void)code;
	(void)unused;

	cpu_boost_mask = tzdev_sysconf()->swd_sysconf.big_cpus_mask;

	log_info(tzdev_boost, "Booster initialization done.\n");

	return NOTIFY_DONE;
}

static struct notifier_block tz_boost_init_notifier = {
	.notifier_call = tz_boost_init_call,
};

static int __init tz_boost_init(void)
{
	int rc;

	rc = tzdev_blocking_notifier_register(TZDEV_INIT_NOTIFIER, &tz_boost_init_notifier);
	if (rc) {
		log_error(tzdev_boost, "Failed to register init notifier, error=%d\n", rc);
		return rc;
	}
	log_info(tzdev_boost, "Boost callbacks registration done\n");

	return 0;
}

early_initcall(tz_boost_init);

void tz_boost_enable(void)
{
	mutex_lock(&tz_boost_lock);

	if (!tz_boost_users) {
		tz_hotplug_update_nwd_cpu_mask(cpu_boost_mask);
		pm_qos_add_request(&tz_boost_qos, TZ_BOOST_CPU_FREQ_MIN,
				TZ_BOOST_CPU_FREQ_MAX_DEFAULT_VALUE);
	}

	tz_boost_users++;

	mutex_unlock(&tz_boost_lock);
}

void tz_boost_disable(void)
{
	mutex_lock(&tz_boost_lock);

	tz_boost_users--;
	BUG_ON(tz_boost_users < 0);

	if (!tz_boost_users) {
		pm_qos_remove_request(&tz_boost_qos);
		tz_hotplug_update_nwd_cpu_mask(0x0);
	}

	mutex_unlock(&tz_boost_lock);
}
