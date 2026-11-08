// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026, DuhanROM. All rights reserved.
 */

#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/string.h>

#include <linux/duhanrom/bootmode.h>

static enum duhanrom_bootmode duhanrom_current_bootmode = DUHANROM_BOOTMODE_UNKNOWN;

enum duhanrom_bootmode get_duhanrom_bootmode(void)
{
	return duhanrom_current_bootmode;
}
EXPORT_SYMBOL_GPL(get_duhanrom_bootmode);

static int __init duhanrom_bootmode_init(void)
{
	pr_info("DuhanROM: checking bootmode\n");

	if (!strlen(boot_command_line)) {
		pr_err("DuhanROM: boot_command_line is empty\n");
		return -1;
	}

	if (strstr(boot_command_line, "androidboot.mode=charger")) {
		pr_info("DuhanROM: bootmode=charger\n");
		duhanrom_current_bootmode = DUHANROM_BOOTMODE_CHARGER;
		return 0;
	}

	if (strstr(boot_command_line, "bootmode=2")) {
		pr_info("DuhanROM: bootmode=recovery\n");
		duhanrom_current_bootmode = DUHANROM_BOOTMODE_RECOVERY;
		return 0;
	}

	pr_info("DuhanROM: bootmode=normal\n");
	duhanrom_current_bootmode = DUHANROM_BOOTMODE_NORMAL;

	return 0;
}

subsys_initcall(duhanrom_bootmode_init);
