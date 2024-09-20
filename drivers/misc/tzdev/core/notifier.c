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

#include <linux/mm.h>
#include <linux/notifier.h>

#include "core/notifier.h"

BLOCKING_NOTIFIER_HEAD(tzdev_init_notifier);
BLOCKING_NOTIFIER_HEAD(tzdev_fini_notifier);
ATOMIC_NOTIFIER_HEAD(tzdev_pre_smc_notifier);
ATOMIC_NOTIFIER_HEAD(tzdev_post_smc_notifier);

struct blocking_notifier_head *blocking_head[TZDEV_MAX_BLOCKING_NOTIFIERS] =
{
	&tzdev_init_notifier,
	&tzdev_fini_notifier,
};

struct atomic_notifier_head *atomic_head[TZDEV_MAX_ATOMIC_NOTIFIERS] =
{
	&tzdev_pre_smc_notifier,
	&tzdev_post_smc_notifier,
};

int tzdev_blocking_notifier_register(enum tzdev_blocking_notifier_type type,
		struct notifier_block *nb)
{
	BUG_ON(type >= TZDEV_MAX_BLOCKING_NOTIFIERS);

	return blocking_notifier_chain_register(blocking_head[type], nb);
}

int tzdev_blocking_notifier_unregister(enum tzdev_blocking_notifier_type type,
		struct notifier_block *nb)
{
	BUG_ON(type >= TZDEV_MAX_BLOCKING_NOTIFIERS);

	return blocking_notifier_chain_unregister(blocking_head[type], nb);
}

int tzdev_blocking_notifier_run(enum tzdev_blocking_notifier_type type)
{
	int ret;

	BUG_ON(type >= TZDEV_MAX_BLOCKING_NOTIFIERS);

	ret = blocking_notifier_call_chain(blocking_head[type], 0, NULL);
	if (ret == NOTIFY_DONE)
		return 0;

	return ret;
}

int tzdev_atomic_notifier_register(enum tzdev_atomic_notifier_type type,
		struct notifier_block *nb)
{
	BUG_ON(type >= TZDEV_MAX_ATOMIC_NOTIFIERS);

	return atomic_notifier_chain_register(atomic_head[type], nb);
}

int tzdev_atomic_notifier_unregister(enum tzdev_atomic_notifier_type type,
		struct notifier_block *nb)
{
	BUG_ON(type >= TZDEV_MAX_ATOMIC_NOTIFIERS);

	return atomic_notifier_chain_unregister(atomic_head[type], nb);
}

int tzdev_atomic_notifier_run(enum tzdev_atomic_notifier_type type)
{
	int ret;

	BUG_ON(type >= TZDEV_MAX_ATOMIC_NOTIFIERS);

	ret = atomic_notifier_call_chain(atomic_head[type], 0, NULL);
	if (ret == NOTIFY_DONE)
		return 0;

	return ret;
}
