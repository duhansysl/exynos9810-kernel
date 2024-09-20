// SPDX-License-Identifier: GPL-2.0
/*
 * linux/mm/mm_debug.c
 *
 * Copyright (C) 2019 Samsung Electronics
 *
 */

#include <linux/module.h>
#include <linux/mm.h>
#include <linux/oom.h>
#include <linux/ratelimit.h>

#define MB_TO_PAGES(x) ((x) << (20 - PAGE_SHIFT))
#define MIN_FILE_SIZE	200

static DEFINE_RATELIMIT_STATE(mm_debug_rs, 30 * HZ, 1);

static unsigned long mm_debug_count(struct shrinker *s,
				  struct shrink_control *sc)
{
	unsigned long inactive_file, active_file;

	if (!__ratelimit(&mm_debug_rs))
		goto out;

	inactive_file = global_node_page_state(NR_INACTIVE_FILE);
	active_file = global_node_page_state(NR_ACTIVE_FILE);
	if ((inactive_file + active_file) < MB_TO_PAGES(MIN_FILE_SIZE)) {
		show_mem(0);
		dump_tasks(NULL, NULL);
	}

out:
	return 0; /* return 0 not to call to scan_objects */
}

static struct shrinker mm_debug_shrinker = {
	.count_objects = mm_debug_count,
};

static int __init mm_debug_init(void)
{
	register_shrinker(&mm_debug_shrinker);
	return 0;
}

static void __exit mm_debug_exit(void)
{
	unregister_shrinker(&mm_debug_shrinker);
}

module_init(mm_debug_init);
module_exit(mm_debug_exit);
