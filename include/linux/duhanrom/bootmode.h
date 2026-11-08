/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2026, DuhanROM. All rights reserved.
 */

#ifndef _LINUX_DUHANROM_BOOTMODE_H
#define _LINUX_DUHANROM_BOOTMODE_H

enum duhanrom_bootmode {
	DUHANROM_BOOTMODE_NORMAL,
	DUHANROM_BOOTMODE_CHARGER,
	DUHANROM_BOOTMODE_RECOVERY,
	DUHANROM_BOOTMODE_UNKNOWN
};

enum duhanrom_bootmode get_duhanrom_bootmode(void);

#endif /* _LINUX_DUHANROM_BOOTMODE_H */
