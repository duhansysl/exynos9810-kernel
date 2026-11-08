// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2026, DuhanROM. All rights reserved.
 * Dynamic AIDL / New (0-255) and Legacy (0-25500) brightness method
 */

#include <linux/compiler.h>
#include <linux/export.h>
#include <linux/types.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/duhanrom/brightness.h>

/*
 * aidl_brightness:
 *   1 = AIDL / NEW   (Default: OneUI 7, OneUI 8 and above, AIDL Lights HAL: 0..255)
 *   0 = LEGACY / OLD (OneUI 6 and below, AOSP legacy scale: 0..25500)
 */
static int aidl_brightness = 1;
module_param_named(aidl_brightness, aidl_brightness, int, 0644);

/* cmdline support: 'aidl_brightness=1' or 'aidl_brightness=0' */
static int __init parse_aidl_brightness_cmdline(char *str)
{
	get_option(&str, &aidl_brightness);
	pr_info("DuhanROM: aidl_brightness cmdline set to %d\n", aidl_brightness);
	return 0;
}
early_param("aidl_brightness", parse_aidl_brightness_cmdline);

/* alias: 'new_brightness=1' or 'new_brightness=0' */
static int __init parse_new_brightness_cmdline(char *str)
{
	return parse_aidl_brightness_cmdline(str);
}
early_param("new_brightness", parse_new_brightness_cmdline);

int get_fixed_brightness(int brightness)
{
	/*
	 * In AIDL / new mode (OneUI 7, OneUI 8 and above):
	 * Userspace HAL sends values in 0-255 (or up to 420 for HBM).
	 * If value is <= 420, scale it up by 100 to match the 0-25500 panel range.
	 * If value is > 420, it is already in legacy/raw scale (e.g. bootanim 12800),
	 * so leave it untouched.
	 */
	if (aidl_brightness == 1) {
		if (brightness > 0 && brightness <= 420)
			return brightness * 100;
	}

	return brightness;
}
EXPORT_SYMBOL_GPL(get_fixed_brightness);
