/*
 * Copyright (C) 2013-2019 Samsung Electronics, Inc.
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
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>

#include "tzdev_internal.h"
#include "core/cdev.h"
#include "core/iwio.h"
#include "core/log.h"
#include "core/notifier.h"
#include "debug/panic_dump.h"

MODULE_AUTHOR("Pavel Bogachev <p.bogachev@partner.samsung.com>");
MODULE_DESCRIPTION("Trustzone panic dump module");
MODULE_LICENSE("GPL");

#define TZ_PANIC_DUMP_IOC_MAGIC	'c'
#define TZ_PANIC_DUMP_GET_SIZE	_IOW(TZ_PANIC_DUMP_IOC_MAGIC, 0, __u32)

static char *tz_panic_dump;
static unsigned long tz_panic_dump_size;
static DEFINE_SPINLOCK(tz_panic_dump_slock);

static ssize_t tz_panic_dump_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	ssize_t ret;

	if (!count)
		return 0;

	spin_lock(&tz_panic_dump_slock);

	if (!tz_panic_dump || !tz_panic_dump_size) {
		ret = -EPERM;
		goto out;
	}

	if (*ppos >= tz_panic_dump_size) {
		ret = 0;
		goto out;
	}

	if (*ppos + count > tz_panic_dump_size)
		count = tz_panic_dump_size - *ppos;

	if (copy_to_user(buf, tz_panic_dump + *ppos, count)) {
		ret = -EFAULT;
		goto out;
	}

	*ppos += count;
	ret = count;

out:
	spin_unlock(&tz_panic_dump_slock);

	return ret;
}

static long tz_panic_dump_unlocked_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	__u32 __user *argp = (__u32 *) arg;
	__u32 size;

	if (!tz_panic_dump)
		return -EPERM;

	size = tz_panic_dump_size;

	return put_user(size, argp);
}

static const struct file_operations tz_panic_dump_fops = {
	.owner = THIS_MODULE,
	.read = tz_panic_dump_read,
	.unlocked_ioctl = tz_panic_dump_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tz_panic_dump_unlocked_ioctl,
#endif /* CONFIG_COMPAT */
};

static struct tz_cdev tz_panic_dump_cdev = {
	.name = "tz_panic_dump",
	.fops = &tz_panic_dump_fops,
	.owner = THIS_MODULE,
};

int tz_panic_dump_init(void)
{
	int ret;
	long size;
	struct tz_iwio_aux_channel *aux_ch;

	aux_ch = tz_iwio_get_aux_channel();

	ret = tzdev_smc_tz_panic_dump_init();
	if (ret) {
		log_error(tzdev_panic_dump, "Failed to init IW panic dump, error=%d\n", ret);
		tz_iwio_put_aux_channel();
		return NOTIFY_DONE;
	}

	memcpy(&size, aux_ch->buffer, sizeof(uint32_t));

	tz_iwio_put_aux_channel();

	tz_panic_dump = tz_iwio_alloc_iw_channel(TZ_IWIO_CONNECT_PANIC_DUMP,
			DIV_ROUND_UP(size, PAGE_SIZE), NULL, NULL, NULL);
	if (IS_ERR(tz_panic_dump)) {
		log_error(tzdev_panic_dump, "Failed to allocate IW buffer for panic dump\n");
		return NOTIFY_DONE;
	}

	ret = tz_cdev_register(&tz_panic_dump_cdev);
	if (ret) {
		log_error(tzdev_panic_dump, "Failed to create IW panic dump device, error=%d\n", ret);
		return NOTIFY_DONE;
	}

	tz_panic_dump_size = size;

	log_info(tzdev_panic_dump, "IW panic dump initialization done.");

	return NOTIFY_DONE;
}
