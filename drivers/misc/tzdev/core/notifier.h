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

#ifndef __TZ_NOTIFIER_H__
#define __TZ_NOTIFIER_H__

#include <linux/notifier.h>

enum tzdev_blocking_notifier_type {
	TZDEV_INIT_NOTIFIER,
	TZDEV_FINI_NOTIFIER,
	TZDEV_MAX_BLOCKING_NOTIFIERS
};

enum tzdev_atomic_notifier_type {
	TZDEV_PRE_SMC_NOTIFIER,
	TZDEV_POST_SMC_NOTIFIER,
	TZDEV_MAX_ATOMIC_NOTIFIERS
};

int tzdev_blocking_notifier_register(enum tzdev_blocking_notifier_type type, struct notifier_block *nb);
int tzdev_blocking_notifier_unregister(enum tzdev_blocking_notifier_type type, struct notifier_block *nb);
int tzdev_blocking_notifier_run(enum tzdev_blocking_notifier_type type);

int tzdev_atomic_notifier_register(enum tzdev_atomic_notifier_type type, struct notifier_block *nb);
int tzdev_atomic_notifier_unregister(enum tzdev_atomic_notifier_type type, struct notifier_block *nb);
int tzdev_atomic_notifier_run(enum tzdev_atomic_notifier_type type);

#endif /* __TZ_NOTIFIER_H__ */
