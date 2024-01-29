/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2019 Samsung Electronics
 */

#ifndef __SEC_VIB_NOTIFIER_H__
#define __SEC_VIB_NOTIFIER_H__

#include <linux/notifier.h>

enum SEC_VIB_NOTIFIER {
	SEC_VIB_NOTIFIER_OFF = 0,
	SEC_VIB_NOTIFIER_ON,
};

struct vib_notifier_context {
	int index;
	int timeout;
};

extern int sec_vib_notifier_register(struct notifier_block *n);
extern int sec_vib_notifier_unregister(struct notifier_block *nb);
#endif /* __SEC_VIB_NOTIFIER_H__ */
