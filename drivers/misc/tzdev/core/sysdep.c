/*
 * Copyright (C) 2015 Samsung Electronics, Inc.
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

#include <linux/compiler.h>
#include <linux/cpu.h>
#include <linux/cred.h>
#include <linux/crypto.h>
#include <linux/fs.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <crypto/sha.h>

#include "core/sysdep.h"

int sysdep_idr_alloc(struct idr *idr, void *mem)
{
	return sysdep_idr_alloc_in_range(idr, mem, 1, 0);
}

int sysdep_idr_alloc_in_range(struct idr *idr, void *mem,
		unsigned long start, unsigned long end)
{
	int res;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
	res = idr_alloc(idr, mem, start, end, GFP_KERNEL);
#else
	int tmp_id;
	do {
		if (!idr_pre_get(idr, GFP_KERNEL)) {
			res = -ENOMEM;
			break;
		}

		/* Reserving zero value helps with special case handling elsewhere */
		res = idr_get_new_above(idr, mem, start, &tmp_id);
		if (res == 0 && end && tmp_id >= end) {
			res = -ENOSPC;
			idr_remove(idr, tmp_id);
			break;
		}
		if (res != -EAGAIN)
			break;
	} while (1);

	/* Simulate behaviour of new version */
	res = res ? -ENOMEM : tmp_id;
#endif
	return res;
}

static __maybe_unused unsigned int gup_flags(int write, int force)
{
	unsigned int flags = 0;

	if (write)
		flags |= FOLL_WRITE;
	if (force)
		flags |= FOLL_FORCE;

	return flags;
}

long sysdep_get_user_pages(struct task_struct *task,
		struct mm_struct *mm, unsigned long start, unsigned long nr_pages,
		int write, int force, struct page **pages,
		struct vm_area_struct **vmas)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	return get_user_pages_remote(task, mm, start, nr_pages, gup_flags(write, force), pages, vmas, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
	return get_user_pages_remote(task, mm, start, nr_pages, gup_flags(write, force), pages, vmas);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0)
	return get_user_pages_remote(task, mm, start, nr_pages, write, force, pages, vmas);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	return get_user_pages(task, mm, start, nr_pages, write, force, pages, vmas);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 168)
	return get_user_pages(task, mm, start, nr_pages, gup_flags(write, force), pages, vmas);
#else
	return get_user_pages(task, mm, start, nr_pages, write, force, pages, vmas);
#endif
}

void sysdep_register_cpu_notifier(struct notifier_block* notifier,
		int (*startup)(unsigned int cpu),
		int (*teardown)(unsigned int cpu))
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "tee/teegris:online",
			startup, teardown);
#else
	/* Register PM notifier */
	register_cpu_notifier(notifier);
#endif
}

void sysdep_unregister_cpu_notifier(struct notifier_block* notifier)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 10, 0)
	cpuhp_remove_state_nocalls(CPUHP_AP_ONLINE_DYN);
#else
	unregister_cpu_notifier(notifier);
#endif
}

int sysdep_vfs_getattr(struct file *filp, struct kstat *stat)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0))
	struct path p;

	p.mnt = filp->f_path.mnt;
	p.dentry = filp->f_path.dentry;

	return vfs_getattr(&p, stat, STATX_SIZE, KSTAT_QUERY_FLAGS);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
	return vfs_getattr(&filp->f_path, stat);
#else
	return vfs_getattr(filp->f_path.mnt, filp->f_path.dentry, stat);
#endif
}
