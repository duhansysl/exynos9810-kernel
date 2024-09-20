/*
 * Copyright (C) 2012-2020, Samsung Electronics Co., Ltd.
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

#include <linux/delay.h>
#include <linux/list.h>
#include <linux/migrate.h>
#include <linux/mmzone.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>

#include "tzdev_internal.h"
#include "core/cred.h"
#include "core/iwio.h"
#include "core/log.h"
#include "core/mem.h"
#include "core/sysdep.h"

#define TZDEV_PFNS_PER_PAGE		(PAGE_SIZE / sizeof(sk_pfn_t))
#define TZDEV_IWSHMEM_IDS_PER_PAGE	(PAGE_SIZE / sizeof(uint32_t))

#define TZDEV_IWSHMEM_REG_FLAG_WRITE	(1 << 0)
#define TZDEV_IWSHMEM_REG_FLAG_KERNEL	(1 << 1)

static void *tzdev_mem_release_buf;
static DEFINE_IDR(tzdev_mem_map);
static DEFINE_MUTEX(tzdev_mem_mutex);

static void tzdev_mem_free(int id, struct tzdev_mem_reg *mem, unsigned int is_user)
{
	unsigned long i;

	if (!mem->is_user) {
		if (!is_user) {
			if (mem->free_func)
				mem->free_func(mem->free_data);
			idr_remove(&tzdev_mem_map, id);
			kfree(mem);
		}

		/* Nothing to do for kernel memory */
		return;
	}

	idr_remove(&tzdev_mem_map, id);

	for (i = 0; i < mem->nr_pages; i++)
		__free_page(mem->pages[i]);

	kfree(mem->pages);
	kfree(mem);
}

static void tzdev_mem_list_release(unsigned char *buf, unsigned int cnt)
{
	uint32_t *ids;
	unsigned int i;
	struct tzdev_mem_reg *mem;

	ids = (uint32_t *)buf;
	for (i = 0; i < cnt; i++) {
		mem = idr_find(&tzdev_mem_map, ids[i]);
		BUG_ON(!mem);
		tzdev_mem_free(ids[i], mem, 0);
	}
}

static int _tzdev_mem_release(int id, unsigned int is_user)
{
	struct tzdev_mem_reg *mem;
	struct tz_iwio_aux_channel *ch;
	long cnt;
	int ret = 0;

	mutex_lock(&tzdev_mem_mutex);

	mem = idr_find(&tzdev_mem_map, id);
	if (!mem) {
		ret = -ENOENT;
		goto out;
	}

	if (is_user != mem->is_user) {
		log_error(tzdev_mem, "Trying to release %s memory but memory belongs %s.\n",
				is_user ? "user space":"kernel space",
				mem->is_user ? "user space":"kernel space");
		ret = -EPERM;
		goto out;
	}

	mem->in_release = 1;

	ch = tz_iwio_get_aux_channel();
	cnt = tzdev_smc_shmem_list_rls(id);
	if (cnt > 0) {
		BUG_ON(cnt > TZDEV_IWSHMEM_IDS_PER_PAGE);

		memcpy(tzdev_mem_release_buf, ch->buffer, cnt * sizeof(uint32_t));
		tz_iwio_put_aux_channel();

		tzdev_mem_list_release(tzdev_mem_release_buf, cnt);
	} else {
		ret = cnt;
		tz_iwio_put_aux_channel();
	}

	if (ret == -ESHUTDOWN)
		tzdev_mem_free(id, mem, 0);

out:
	mutex_unlock(&tzdev_mem_mutex);

	return ret;
}

static int _tzdev_mem_register(struct tzdev_mem_reg *mem, sk_pfn_t *pfns,
		unsigned long nr_pages, unsigned int is_writable)
{
	int ret, id;
	struct tz_iwio_aux_channel *ch;
	unsigned int pfns_used, pfns_transferred, off, pfns_current;
	ssize_t size;

	ret = tz_format_cred(&mem->cred);
	if (ret) {
		log_error(tzdev_mem, "Failed to calculate shmem credentials, error=%d\n", ret);
		return ret;
	}

	mutex_lock(&tzdev_mem_mutex);
	ret = sysdep_idr_alloc(&tzdev_mem_map, mem);
	if (ret < 0) {
		log_error(tzdev_mem, "Failed to allocate shmem id, error=%d\n", ret);
		goto unlock;
	}

	id = ret;
	ch = tz_iwio_get_aux_channel();

	memcpy(ch->buffer, &mem->cred, sizeof(struct tz_cred));

	pfns_used = DIV_ROUND_UP(sizeof(struct tz_cred), sizeof(sk_pfn_t));
	off = pfns_used * sizeof(sk_pfn_t);
	pfns_transferred = 0;

	while (pfns_transferred < nr_pages) {
		pfns_current = min(nr_pages - pfns_transferred,
				TZDEV_PFNS_PER_PAGE - pfns_used);
		size = pfns_current * sizeof(sk_pfn_t);

		memcpy(&ch->buffer[off], &pfns[pfns_transferred], size);
		ret = tzdev_smc_shmem_list_reg(id, nr_pages, is_writable);
		if (ret) {
			log_error(tzdev_mem, "Failed register pfns, error=%d\n", ret);
			goto put_aux_channel;
		}

		pfns_transferred += pfns_current;

		/* First write to aux channel is done with additional offset,
		 * because first call requires passing credentianls.
		 * For the subsequent calls offset is 0. */
		off = 0;
		pfns_used = 0;
	}

	tz_iwio_put_aux_channel();
	mutex_unlock(&tzdev_mem_mutex);

	return id;

put_aux_channel:
	tz_iwio_put_aux_channel();
	idr_remove(&tzdev_mem_map, id);
unlock:
	mutex_unlock(&tzdev_mem_mutex);

	return ret;
}

int tzdev_mem_init(void)
{
	struct page *page;

	page = alloc_page(GFP_KERNEL);
	if (!page) {
		log_error(tzdev_mem, "Failed to allocate mem release buffer.\n");
		return -ENOMEM;
	}

	tzdev_mem_release_buf = page_address(page);

	log_info(tzdev_mem, "IW mem initialization done.\n");

	return 0;
}

void tzdev_mem_fini(void)
{
	struct tzdev_mem_reg *mem;
	unsigned int id;

	mutex_lock(&tzdev_mem_mutex);
	idr_for_each_entry(&tzdev_mem_map, mem, id)
		tzdev_mem_free(id, mem, 0);
	mutex_unlock(&tzdev_mem_mutex);

	log_info(tzdev_mem, "IW mem finalization done.\n");
}

int tzdev_mem_register_user(unsigned long size, unsigned int write)
{
	struct page **pages;
	struct tzdev_mem_reg *mem;
	sk_pfn_t *pfns;
	unsigned long nr_pages = 0;
	unsigned long i, j;
	int ret, id;
	unsigned int flags = 0;

	if (!size) {
		log_error(tzdev_mem, "Size is invalid, size=%lu\n", size);
		return -EINVAL;
	}

	nr_pages = NUM_PAGES(size);

	if (write)
		flags |= TZDEV_IWSHMEM_REG_FLAG_WRITE;

	pages = kcalloc(nr_pages, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		log_error(tzdev_mem, "Failed to allocate pages buffer.\n");
		return -ENOMEM;
	}

	pfns = kmalloc(nr_pages * sizeof(sk_pfn_t), GFP_KERNEL);
	if (!pfns) {
		log_error(tzdev_mem, "Failed to allocate pfns buffer.\n");
		ret = -ENOMEM;
		goto out_pages;
	}

	mem = kmalloc(sizeof(struct tzdev_mem_reg), GFP_KERNEL);
	if (!mem) {
		log_error(tzdev_mem, "Failed to allocate mem descriptor.\n");
		ret = -ENOMEM;
		goto out_pfns;
	}

	for (i = 0; i < nr_pages; i++) {
		pages[i] = alloc_page(GFP_KERNEL | __GFP_ZERO);
		if (!pages[i]) {
			log_error(tzdev_mem, "Failed to allocate iwshmem page.\n");
			ret = -ENOMEM;
			goto out_mem;
		}

		pfns[i] = page_to_pfn(pages[i]);
	}

	mem->is_user = 1;
	mem->nr_pages = nr_pages;
	mem->pages = pages;
	mem->free_func = NULL;
	mem->free_data = NULL;
	mem->in_release = 0;

	id = _tzdev_mem_register(mem, pfns, nr_pages, flags);
	if (id < 0) {
		ret = id;
		goto out_mem;
	}

	kfree(pfns);

	return id;

out_mem:
	kfree(mem);

	for (j = 0; j < i; j++)
		__free_page(pages[j]);
out_pfns:
	kfree(pfns);
out_pages:
	kfree(pages);

	return ret;
}

int tzdev_mem_register(void *ptr, unsigned long size, unsigned int write,
		tzdev_mem_free_func_t free_func, void *free_data)
{
	struct tzdev_mem_reg *mem;
	struct page *page;
	sk_pfn_t *pfns;
	unsigned long addr, start, end;
	unsigned long nr_pages = 0;
	int ret, i, id;
	unsigned int flags = TZDEV_IWSHMEM_REG_FLAG_KERNEL;

	if (!size) {
		log_error(tzdev_mem, "Size is invalid, size=%lu\n", size);
		return -EINVAL;
	}

	addr = (unsigned long)ptr;

	BUG_ON(addr + size <= addr || !IS_ALIGNED(addr | size, PAGE_SIZE));

	start = addr >> PAGE_SHIFT;
	end = (addr + size) >> PAGE_SHIFT;
	nr_pages = end - start;

	if (write)
		flags |= TZDEV_IWSHMEM_REG_FLAG_WRITE;

	pfns = kmalloc(nr_pages * sizeof(sk_pfn_t), GFP_KERNEL);
	if (!pfns) {
		log_error(tzdev_mem, "Failed to allocate pfns buffer.\n");
		return -ENOMEM;
	}

	mem = kmalloc(sizeof(struct tzdev_mem_reg), GFP_KERNEL);
	if (!mem) {
		log_error(tzdev_mem, "Failed to allocate mem descriptor.\n");
		ret = -ENOMEM;
		goto out_pfns;
	}

	mem->is_user = 0;
	mem->free_func = free_func;
	mem->free_data = free_data;
	mem->in_release = 0;

	for (i = 0; i < nr_pages; i++) {
		page = is_vmalloc_addr(ptr + PAGE_SIZE * i)
				? vmalloc_to_page(ptr + PAGE_SIZE * i)
				: virt_to_page(addr + PAGE_SIZE * i);

		pfns[i] = page_to_pfn(page);
	}

	id = _tzdev_mem_register(mem, pfns, nr_pages, flags);
	if (id < 0) {
		ret = id;
		goto out_mem;
	}

	kfree(pfns);

	return id;

out_mem:
	kfree(mem);
out_pfns:
	kfree(pfns);

	return ret;
}

int tzdev_mem_release_user(unsigned int id)
{
	return _tzdev_mem_release(id, 1);
}

int tzdev_mem_release(unsigned int id)
{
	return _tzdev_mem_release(id, 0);
}

int tzdev_mem_find(unsigned int id, struct tzdev_mem_reg **mem)
{
	mutex_lock(&tzdev_mem_mutex);
	*mem = idr_find(&tzdev_mem_map, id);
	mutex_unlock(&tzdev_mem_mutex);

	if (*mem == NULL)
		return -ENOENT;

	return 0;
}
