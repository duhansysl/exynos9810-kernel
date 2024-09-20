/*
 * Copyright (C) 2012-2017, Samsung Electronics Co., Ltd.
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

#include <linux/cred.h>
#include <linux/crypto.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <crypto/sha.h>

#include "tzdev_internal.h"
#include "core/cred.h"
#include "core/sysdep.h"

struct cred_cache_entry {
	struct list_head link;
	char hash[CRED_HASH_SIZE];
	struct pid *pid;
};

static LIST_HEAD(cred_cache);
static DEFINE_MUTEX(cred_cache_mutex);

static const uint8_t kernel_client_hash[CRED_HASH_SIZE] = {
	0x49, 0x43, 0x54, 0xd8, 0x88, 0x45, 0x37, 0xaa,
	0x95, 0xff, 0x73, 0x57, 0x22, 0x07, 0xc9, 0x01,
	0x60, 0x09, 0x57, 0x37, 0xe5, 0x42, 0x6c, 0x5f,
	0xfa, 0x94, 0x79, 0x6b, 0xc9, 0x37, 0x39, 0x7e
};

static int tz_crypto_file_sha256(uint8_t *hash, struct file *file)
{
	loff_t i_size, offset = 0;
	struct crypto_shash *tfm;
	char *rbuf;
	int ret = 0;

	rbuf = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!rbuf)
		return -ENOMEM;

	i_size = i_size_read(file_inode(file));

	tfm = crypto_alloc_shash("sha256", 0, 0);
	if (IS_ERR(tfm)) {
		ret = PTR_ERR(tfm);
		goto out;
	}

	{
		SHASH_DESC_ON_STACK(desc, tfm);

		desc->tfm = tfm;
		desc->flags = 0;

		ret = crypto_shash_init(desc);
		if (ret < 0)
			goto out;

		while (offset < i_size) {
			int rbuf_len;

			ret = sysdep_kernel_read(file, rbuf, PAGE_SIZE, offset);
			if (ret <= 0)
				break;

			rbuf_len = ret;

			ret = crypto_shash_update(desc, rbuf, rbuf_len);
			if (ret)
				break;
		}

		if (!ret)
			crypto_shash_final(desc, hash);

		shash_desc_zero(desc);
	}

	crypto_free_shash(tfm);

out:
	kfree(rbuf);

	return ret;
}

static int tz_format_cred_user(struct tz_cred *cred)
{
	uint8_t hash[SHA256_DIGEST_SIZE];
	struct cred_cache_entry *cred_cache_entry;
	struct pid *pid = get_task_pid(current, PIDTYPE_PID);
	struct file *exe_file;
	int ret = 0;

	mutex_lock(&cred_cache_mutex);
	list_for_each_entry(cred_cache_entry, &cred_cache, link) {
		if (cred_cache_entry->pid == pid) {
			memcpy(&cred->hash, &cred_cache_entry->hash,
					sizeof(cred->hash));
			goto unlock;
		}
	}
	
	exe_file = get_mm_exe_file(current->mm);
	if (!exe_file) {
		ret = -EINVAL;
		goto unlock;
	}

	/* Calculate hash of process ELF. */
	ret = tz_crypto_file_sha256(hash, exe_file);

	fput(exe_file);

	if (ret)
		goto unlock;

	cred_cache_entry = kmalloc(sizeof(struct cred_cache_entry), GFP_KERNEL);

	if (cred_cache_entry) {
		get_pid(pid);

		cred_cache_entry->pid = pid;
		memcpy(&cred_cache_entry->hash, hash, SHA256_DIGEST_SIZE);

		list_add(&cred_cache_entry->link, &cred_cache);
	}

	memcpy(&cred->hash, hash, SHA256_DIGEST_SIZE);

unlock:
	mutex_unlock(&cred_cache_mutex);

	put_pid(pid);

	cred->pid = current->tgid;
	cred->uid = __kuid_val(current_uid());
	cred->gid = __kgid_val(current_gid());

	return ret;
}

static int tz_format_cred_kernel(struct tz_cred *cred)
{
	cred->pid = current->real_parent->pid;
	cred->uid = __kuid_val(current_uid());
	cred->gid = __kgid_val(current_gid());

	memcpy(&cred->hash, &kernel_client_hash, SHA256_DIGEST_SIZE);

	return 0;
}

int tz_format_cred(struct tz_cred *cred)
{
	BUILD_BUG_ON(sizeof(pid_t) != sizeof(cred->pid));
	BUILD_BUG_ON(sizeof(uid_t) != sizeof(cred->uid));
	BUILD_BUG_ON(sizeof(gid_t) != sizeof(cred->gid));

	return current->mm ? tz_format_cred_user(cred) : tz_format_cred_kernel(cred);
}

static struct delayed_work tz_cred_gc_work;

static void tz_cred_gc_callback(struct work_struct *work)
{
	struct cred_cache_entry *cred_cache_entry, *tmp;

	mutex_lock(&cred_cache_mutex);
	list_for_each_entry_safe(cred_cache_entry, tmp, &cred_cache, link) {
		if (atomic_read(&cred_cache_entry->pid->count) == 1) {
			list_del(&cred_cache_entry->link);
			put_pid(cred_cache_entry->pid);
			kfree(cred_cache_entry);
		}
	}
	mutex_unlock(&cred_cache_mutex);

	queue_delayed_work(system_wq, to_delayed_work(work), 500);
}

static int __init tz_cred_init_notifier(void)
{
	INIT_DEFERRABLE_WORK(&tz_cred_gc_work, tz_cred_gc_callback);
	queue_delayed_work(system_wq, &tz_cred_gc_work, 500);

	return 0;
}

module_init(tz_cred_init_notifier);
