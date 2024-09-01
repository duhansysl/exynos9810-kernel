/* iva_mem.c
 *
 * Copyright (C) 2016 Samsung Electronics Co.Ltd
 * Authors:
 *	Ilkwan Kim <ilkwan.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/stat.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/time.h>
#include <linux/mm.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/delay.h>

#include "iva_mem.h"
#include "iva_mem_sync.h"

#define CALL_SHOW_PROC_MAP_LIST

//#define ENABLE_MAP_ATTACHMENT
#undef ENABLE_CACHE_FLUSH_ALL

/* should be same as iva_ext_ram.h in user space */
#define IVA_MEM_ALLOC_TYPE_FD_SHIFT	(24)
#define IVA_MEM_GET_SHARED_FD(fd) \
	(fd & ~(1 << IVA_MEM_ALLOC_TYPE_FD_SHIFT))

static void __iva_mem_map_destroy(struct kref *kref)
{
	struct iva_mem_map	*iva_map_node =
			container_of(kref, struct iva_mem_map, map_ref);
	struct dma_buf		*dmabuf;
	struct device		*dev = iva_map_node->dev;

	dmabuf = iva_map_node->dmabuf;
	if (!dmabuf) {
		dev_err(dev, "%s() dmabuf is null, buf_fd(%d)\n",
		       __func__, iva_map_node->shared_fd);
	}

	dev_dbg(dev, "%s() buf_fd(%d) iova(0x%lx), size(0x%x, 0x%x), attachment(%p)\n",
		__func__,
		iva_map_node->shared_fd,
		iva_map_node->io_va, iva_map_node->req_size,
		iva_map_node->act_size,
		iva_map_node->attachment);
#ifdef CONFIG_ION_EXYNOS
	if (iva_map_node->attachment)
		ion_iovmm_unmap(iva_map_node->attachment, iva_map_node->io_va);
#endif
	if (iva_map_node->sg_table) {
		dma_buf_unmap_attachment(iva_map_node->attachment,
				iva_map_node->sg_table, DMA_BIDIRECTIONAL);
		iva_map_node->sg_table = NULL;
	}

	if (iva_map_node->attachment) {
		dma_buf_detach(dmabuf, iva_map_node->attachment);
		iva_map_node->attachment = NULL;
	}
}

static inline void iva_mem_map_get_refcnt(struct iva_mem_map *iva_map)
{
	kref_get(&iva_map->map_ref);
}

static inline int iva_mem_map_put_refcnt(struct iva_mem_map *iva_map)
{
	return kref_put(&iva_map->map_ref, __iva_mem_map_destroy);
}

int iva_mem_map_read_refcnt(struct iva_mem_map *iva_map)
{
	return atomic_read(&iva_map->map_ref.refcount);
}

static int iva_mem_show_proc_mapped_list_nolock(struct iva_proc *proc)
{
	struct iva_mem_map	*iva_map_node;
	struct iva_dev_data	*iva = proc->iva_data;
	struct device		*dev = iva->dev;
	int			i, j = 0;

	if (hash_empty(proc->h_mem_map))
		return 0;

	dev_info(dev, "proc mapped list: %s mem_map_nr(%u)------------------------\n",
			proc->tsk->comm, proc->mem_map_nr);
	dev_info(dev, "  [  i,  j] buf_fd(cnt)  flags   io_va   req_size  act_size  ref_cnt\n");

	hash_for_each(proc->h_mem_map, i, iva_map_node, h_node) {
		dev_info(dev, "  [%03d,%3d]     %04d(%ld)    %03x  %08lx %8x  %8x %8d\n",
				i, j,
				iva_map_node->shared_fd,
				file_count(iva_map_node->dmabuf->file),
				iva_map_node->flags,
				iva_map_node->io_va,
				iva_map_node->req_size,
				iva_map_node->act_size,
				iva_mem_map_read_refcnt(iva_map_node));
		j++;
	}
	dev_info(dev, "--------------------------------------------------------------------\n");

	return j;
}

void iva_mem_show_global_mapped_list_lock(struct iva_dev_data *iva, bool lock)
{
	struct list_head	*work_node;
	struct iva_proc		*proc;
	struct device		*dev = iva->dev;
	int			i = 0;

	dev_info(dev, "====================== show global mapped list =====================\n");
	list_for_each(work_node, &iva->proc_head) {
		proc = list_entry(work_node, struct iva_proc, proc_node);
		if (lock)
			mutex_lock(&proc->mem_map_lock);
		iva_mem_show_proc_mapped_list_nolock(proc);
		if (lock)
			mutex_unlock(&proc->mem_map_lock);
		i++;
	}
	dev_info(dev, "====================================================================\n");
}

static struct iva_mem_map *iva_mem_alloc_map_node(struct iva_dev_data *iva)
{
	if (likely(iva->map_node_cache))
		return (struct iva_mem_map *) kmem_cache_alloc(
				iva->map_node_cache, GFP_KERNEL);
	else
		return (struct iva_mem_map *) devm_kmalloc(iva->dev,
				sizeof(struct iva_mem_map), GFP_KERNEL);
}

static void iva_mem_free_map_node(struct iva_dev_data *iva,
			struct iva_mem_map *map_node)
{
	if (likely(iva->map_node_cache))
		kmem_cache_free(iva->map_node_cache, (void *) map_node);
	else
		devm_kfree(iva->dev, map_node);
}

struct iva_mem_map *iva_mem_find_proc_map_with_fd_nolock(struct iva_proc *proc,
			int shared_fd)
{
	struct iva_mem_map      *iva_map_node;

	hash_for_each_possible(proc->h_mem_map, iva_map_node,
			h_node, shared_fd){
		if (iva_map_node->shared_fd == shared_fd)
			return iva_map_node;
	}
	return NULL;
}

struct iva_mem_map *iva_mem_find_proc_map_with_fd(struct iva_proc *proc,
			int shared_fd)
{
	struct iva_mem_map      *iva_map_node;

	mutex_lock(&proc->mem_map_lock);
	iva_map_node = iva_mem_find_proc_map_with_fd_nolock(proc, shared_fd);
	mutex_unlock(&proc->mem_map_lock);

	return iva_map_node;
}

static struct iva_mem_map *iva_mem_ion_alloc(struct iva_proc *proc,
			uint32_t size, uint32_t align, uint32_t cacheflag)
{
#define ZONE_BIT(zone)		(1 << zone)
	int			ion_shared_fd;
	struct iva_dev_data	*iva = proc->iva_data;
	struct ion_client	*client = iva->ion_client;
	struct device		*dev = iva->dev;
	struct ion_handle	*handle;
	struct iva_mem_map	*iva_map_node;
	struct dma_buf		*dmabuf;

	iva_map_node = iva_mem_alloc_map_node(iva);
	if (!iva_map_node) {
		dev_err(dev, "%s() fail to alloc mem for map.\n", __func__);
		return NULL;
	}

	handle = ion_alloc(client, size, align, REQ_ION_HEAP_ID, cacheflag);
	if (IS_ERR(handle)) {
		dev_err(dev, "%s() fail to alloc ion with id(0x%x), ",
			__func__, REQ_ION_HEAP_ID);
		dev_err(dev, "size(0x%x), align(0x%x), ret(%ld)\n",
			size, align, PTR_ERR(handle));
		goto err_alloc_ion;
	}

	dmabuf = ion_share_dma_buf(client, handle);
	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_err(dev, "%s() ion with dma_buf sharing failed.\n",
				__func__);
		goto err_dmabuf;
	}

	/* increase file cnt. iva_mem_ion_free() required */
	get_dma_buf(dmabuf);

	/* close() should be called for full release operation*/
	ion_shared_fd = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (ion_shared_fd < 0) {
		usleep_range(10000, 100000);
		dev_dbg(dev, "%s() retry to get dma_buf_fd.\n", __func__);

		ion_shared_fd = dma_buf_fd(dmabuf, O_CLOEXEC);
		if (ion_shared_fd < 0) {
			dev_err(dev, "%s() ion with getting dma_buf_fd failed.\n",
					__func__);
			goto err_dmabuf_fd;
		}
	}

	iva_map_node->shared_fd	= ion_shared_fd;
	iva_map_node->handle	= handle;
	iva_map_node->dmabuf	= dmabuf;
	iva_map_node->attachment = NULL;
	iva_map_node->sg_table	= NULL;
	iva_map_node->io_va	= 0x0;
	iva_map_node->req_size	= size;
	iva_map_node->act_size	= (uint32_t) dmabuf->size;

	/* data for managing */
	iva_map_node->flags	= 0x0;
	SET_IVA_ALLOC_TYPE(iva_map_node->flags, IVA_ALLOC_TYPE_ALLOCATED);
	SET_IVA_CACHE_TYPE(iva_map_node->flags, cacheflag);
	atomic_set(&iva_map_node->map_ref.refcount, 0);
	iva_map_node->dev	= dev;

	dev_dbg(dev, "%s() succeed : size(0x%x, 0x%x) align(0x%x) handle(%p) ",
		__func__, iva_map_node->req_size,
		iva_map_node->act_size,	align, handle);
	dev_dbg(dev, "dma_buf(%p) with buf fd(%d, %ld)\n",
		dmabuf,	ion_shared_fd, file_count(dmabuf->file));

	hash_add(proc->h_mem_map, &iva_map_node->h_node, ion_shared_fd);
	proc->mem_map_nr++;

	return iva_map_node;

	/*
	 * handle.ref = 1, buffer.ref = 2(dma_buf, ion_buffer),
	 * buffer.handle_count = 1, dma_buf = exported, dma_buf.ref = 1
	 * dmabuf, ion_buffer_fd, iva_map_node are filled
	 */

err_dmabuf_fd:
	dma_buf_put(dmabuf);
err_dmabuf:
	ion_free(client, handle);
err_alloc_ion:
	iva_mem_free_map_node(iva, iva_map_node);
	return NULL;
}


static int iva_mem_ion_free(struct iva_proc *proc, struct iva_mem_map *iva_map_node)
{
	int			buf_fd = iva_map_node->shared_fd;
	struct iva_dev_data	*iva = proc->iva_data;
	struct ion_client	*client = iva->ion_client;
	struct device		*dev = iva->dev;
	int			ref_cnt;

	/* in case that ion mem was allocated outside */
	if (!ALLOC_INSIDE(iva_map_node->flags)) {
		dev_warn(dev, "%s() fd(%d) alloced outside. flags(0x%x)\n",
				__func__, buf_fd, iva_map_node->flags);
		return -EINVAL;
	}

	/* still iova mapped */
	ref_cnt = iva_mem_map_read_refcnt(iva_map_node);
	if (ref_cnt) {
		iva_map_node->flags |= IVA_FREE_REQUESTED;
		dev_dbg(dev,
			"%s() fd(%d) still mapped, ref_cnt(0x%x), flags(0x%x)\n",
			__func__, buf_fd, ref_cnt, iva_map_node->flags);
		return 0;
	}

	dma_buf_put(iva_map_node->dmabuf);	/* ion_share_dma_buf */

	/* close() should be done in user space */
	ion_free(client, iva_map_node->handle); /* handle-1, ion_buffer-1 */

	hash_del(&iva_map_node->h_node);
	proc->mem_map_nr--;

	iva_mem_free_map_node(iva, iva_map_node);

	return 0;
}

static int iva_mem_get_ion_iova(struct iva_proc *proc,
			struct iva_mem_map *iva_map_node)
{
	int			ret = 0;
	struct iva_dev_data	*iva = proc->iva_data;
	struct dma_buf		*dmabuf;
	struct sg_table		*sg_table = NULL;
	struct device		*dev = iva->dev;
	struct ion_handle	*handle = NULL;
	struct dma_buf_attachment *attachment;
	int			map_ref_cnt;
#ifdef CONFIG_ION_EXYNOS
	dma_addr_t		io_va;
#else
	ion_phys_addr_t		io_va;
	size_t			io_va_len;
#endif
	map_ref_cnt = iva_mem_map_read_refcnt(iva_map_node);
	if (map_ref_cnt) {	/* already mapped: ref_cnt > 0 */
		dev_dbg(dev,
			"%s() already mapped: fd(%d), iova(%lx), size(0x%x, 0x%x), ref_cnt(%d)",
			__func__,
			iva_map_node->shared_fd,
			iva_map_node->io_va,
			iva_map_node->req_size,
			iva_map_node->act_size,
			map_ref_cnt);
		iva_mem_map_get_refcnt(iva_map_node);
		return 0;
	}

	dmabuf = iva_map_node->dmabuf;
	if (!dmabuf) {
		dev_err(dev, "%s() fail to get dmabuf from list.\n",
				__func__);
		return -EINVAL;
	}

	handle = iva_map_node->handle;
	if (IS_ERR(handle)) {
		dev_err(dev, "%s() node has no handle, ret(%ld)\n",
				__func__, PTR_ERR(handle));
		return PTR_ERR(handle);
	}

	attachment = dma_buf_attach(dmabuf, dev);
	if (IS_ERR_OR_NULL(attachment)) {
		dev_err(dev, "%s() fail to attach dma buf, buf_fd(%d).\n",
				__func__, iva_map_node->shared_fd);
		goto err_attach;
	}

#ifdef ENABLE_MAP_ATTACHMENT	/* for showing */
	sg_table = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR_OR_NULL(sg_table)) {
		dev_err(dev, "%s() fail to map attachment(%p)\n",
				__func__, attachment);
		goto err_map_attach;
	}
#endif

	/* try to map io */
#ifdef CONFIG_ION_EXYNOS
	io_va = ion_iovmm_map(attachment, 0,
			iva_map_node->act_size, DMA_BIDIRECTIONAL, 0);
	if (IS_ERR_VALUE(io_va)) {
		ret = (int) io_va;
		dev_err(dev, "%s() fail to map iovm (%d)\n", __func__, ret);
		goto err_get_iova;
	}
#else
	/* CAUTION: io_va is filled with a physical addr */
	ret = ion_phys(iva->ion_client, handle, &io_va, &io_va_len);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "%s() ion_phys() returns failure\n",
		       __func__);
		ret = -EFAULT;
		goto err_get_iova;
	}
#endif
	/* success */
	iva_map_node->io_va		= (unsigned long) io_va;
	iva_map_node->attachment	= attachment;
	iva_map_node->sg_table		= sg_table;
	kref_init(&iva_map_node->map_ref);

	dev_dbg(dev,
		"%s() buf_fd(%d, %ld) attachment(%p) iova(0x%lx), size(0x%x, 0x%x), ref_cnt(%d)\n",
		__func__, iva_map_node->shared_fd,
		file_count(dmabuf->file), iva_map_node->attachment,
		iva_map_node->io_va,
		iva_map_node->req_size, iva_map_node->act_size,
		iva_mem_map_read_refcnt(iva_map_node));
	return 0;

err_get_iova:
#ifdef ENABLE_MAP_ATTACHMENT
	dma_buf_unmap_attachment(attachment,
				sg_table, DMA_BIDIRECTIONAL);
err_map_attach:
#endif
	dma_buf_detach(dmabuf, attachment);
err_attach:
	return ret;
}

static int iva_mem_put_ion_iova(struct iva_proc *proc,
		struct iva_mem_map *iva_map_node, bool forced_put)
{
	struct iva_dev_data	*iva = proc->iva_data;
	struct device		*dev = iva->dev;

	dev_dbg(dev, "%s() ref_cnt: %d, fd(%d, %ld)\n", __func__,
			iva_mem_map_read_refcnt(iva_map_node),
			iva_map_node->shared_fd,
			file_count(iva_map_node->dmabuf->file));

	if (forced_put) {
		/* forced to unmap iova */
		__iva_mem_map_destroy(&iva_map_node->map_ref);
		atomic_set(&iva_map_node->map_ref.refcount, 0);
		return 0;
	}

	/* decrease ref_cnt of iva_map_node and release */
	if (iva_mem_map_put_refcnt(iva_map_node)) {
		/* removed iova map successfully */
		dev_dbg(dev, "%s() fd(%d, %ld) iova unmaped, flags(0x%x)\n",
			__func__,
			iva_map_node->shared_fd,
			file_count(iva_map_node->dmabuf->file),
			iva_map_node->flags);
		return 0;
	}

	/* return ref count */
	return iva_mem_map_read_refcnt(iva_map_node);
}

static int iva_mem_ion_sync_for_cpu(struct iva_proc *proc,
		struct iva_mem_map *iva_map_node)
{
	struct iva_dev_data	*iva = proc->iva_data;
	struct device		*dev = iva->dev;
	struct dma_buf		*dmabuf;

	if (iva_map_node) {
		dmabuf = iva_map_node->dmabuf;
		if (!dmabuf) {
			dev_err(dev, "%s() dmabuf is null, buf_fd(%d)\n",
			       __func__, iva_map_node->shared_fd);
			return -EINVAL;
		}
	#ifdef CONFIG_ION_EXYNOS
		exynos_ion_sync_dmabuf_for_cpu(dev, dmabuf,
				iva_map_node->act_size,	DMA_BIDIRECTIONAL);

	#else
		iva_ion_sync_sg_for_cpu(iva, iva_map_node);

	#endif
	} else
#ifdef ENABLE_CACHE_FLUSH_ALL
		flush_cache_all();
#else
		dev_warn(dev, "%s() not supported flush_cache_all.\n", __func__);
#endif

	return 0;
}

static int iva_mem_ion_sync_for_device(struct iva_proc *proc,
		struct iva_mem_map *iva_map_node, bool clean_only)
{
	struct iva_dev_data	*iva = proc->iva_data;
	struct device		*dev = iva->dev;
	struct dma_buf		*dmabuf;

	if (iva_map_node) {
		dmabuf = iva_map_node->dmabuf;
		if (!dmabuf) {
			dev_err(dev, "%s() dmabuf is null, buf_fd(%d)\n",
				__func__, iva_map_node->shared_fd);
			return -EINVAL;
		}
	#ifdef CONFIG_ION_EXYNOS
		if (clean_only)
			exynos_ion_sync_dmabuf_for_device(dev, dmabuf,
				iva_map_node->act_size, DMA_BIDIRECTIONAL);
		else
			exynos_ion_flush_dmabuf_for_device(dev,
					dmabuf, iva_map_node->act_size);
	#else
		iva_ion_sync_sg_for_device(iva, iva_map_node);
	#endif

	} else
#ifdef ENABLE_CACHE_FLUSH_ALL
		flush_cache_all();
#else
		dev_warn(dev, "%s() not supported flush_cache_all.\n", __func__);
#endif

	return 0;
}

/* import the buffer created outside */
static struct iva_mem_map *iva_mem_import_ion_buf(struct iva_proc *proc,
		int import_fd)
{
	struct iva_dev_data	*iva = proc->iva_data;
	struct device		*dev = iva->dev;
	struct iva_mem_map	*iva_map_node;
	struct dma_buf		*dmabuf;
	struct ion_handle	*handle;

	iva_map_node = iva_mem_alloc_map_node(iva);
	if (!iva_map_node) {
		dev_err(dev, "%s() fail to alloc mem for map.\n",
				__func__);
		return NULL;
	}

	dmabuf = dma_buf_get(IVA_MEM_GET_SHARED_FD(import_fd));
	if (IS_ERR_OR_NULL(dmabuf)) {
		dev_err(dev, "%s() fail to get dmabuf from fd(%d)\n",
			__func__, import_fd);
		goto err_buf_get;
	}

	/* create handle in space of our client */
#if defined(CONFIG_SOC_EXYNOS8895)
	handle = ion_import_dma_buf(iva->ion_client, IVA_MEM_GET_SHARED_FD(import_fd));
#elif defined(CONFIG_SOC_EXYNOS9810)
	handle = ion_import_dma_buf(iva->ion_client, dmabuf);
#endif
	if (IS_ERR(handle)) {
		dev_err(dev,
			"%s() fail to import handle with fd(%d), ret(%ld)\n",
			__func__, import_fd, PTR_ERR(handle));
		goto err_buf_import;
	}

	iva_map_node->shared_fd		= import_fd;
	iva_map_node->handle		= handle;
	iva_map_node->dmabuf		= dmabuf;
	iva_map_node->attachment	= NULL;
	iva_map_node->sg_table		= NULL;
	iva_map_node->io_va		= 0x0;
	iva_map_node->req_size		= 0;	/* not alloced inside */
	iva_map_node->act_size		= (uint32_t) dmabuf->size;

	iva_map_node->flags	= 0x0;
	SET_IVA_ALLOC_TYPE(iva_map_node->flags, IVA_ALLOC_TYPE_IMPORTED);
	atomic_set(&iva_map_node->map_ref.refcount, 0);
	iva_map_node->dev	= dev;

	/* lock is held */
	hash_add(proc->h_mem_map, &iva_map_node->h_node, import_fd);
	proc->mem_map_nr++;

	return iva_map_node;

err_buf_import:
	dma_buf_put(dmabuf);
err_buf_get:
	iva_mem_free_map_node(iva, iva_map_node);
	return NULL;
}

static void iva_mem_cancel_imported_ion_buf(struct iva_proc *proc,
		struct iva_mem_map *iva_map_node)
{
	struct iva_dev_data	*iva = proc->iva_data;
	struct device		*dev = iva->dev;
	int			ref_cnt;

	ref_cnt = iva_mem_map_read_refcnt(iva_map_node);
	if (ref_cnt) {
		dev_dbg(dev,
			"%s() fd(%d) still mapped, ref_cnt(0x%x), flags(0x%x)\n",
			__func__, iva_map_node->shared_fd,
			ref_cnt, iva_map_node->flags);
		return;
	}

	dev_dbg(dev, "%s() will release mem with outside buf_fd(%d)\n",
			__func__, iva_map_node->shared_fd);
	if (iva_map_node->dmabuf)	/* ion_import_dma_buf */
		dma_buf_put(iva_map_node->dmabuf);

	ion_free(iva->ion_client, iva_map_node->handle);/* destroy handle */

	hash_del(&iva_map_node->h_node);
	proc->mem_map_nr--;

	iva_mem_free_map_node(iva, iva_map_node);
}

static int iva_mem_ion_alloc_param(struct iva_proc *proc,
			struct iva_ion_param *ion_param)
{
#define ZONE_BIT(zone)		(1 << zone)
	struct iva_dev_data	*iva = proc->iva_data;
	struct device		*dev = iva->dev;
	struct iva_mem_map	*iva_map_node;

	mutex_lock(&proc->mem_map_lock);
	iva_map_node = iva_mem_ion_alloc(proc,
			ion_param->size, ion_param->align, ion_param->cacheflag);
	if (!iva_map_node) {
		dev_err(dev, "%s() fail to alloc ion/ion fd\n", __func__);
		mutex_unlock(&proc->mem_map_lock);
		return -ENOMEM;
	}

	ion_param->shared_fd	= iva_map_node->shared_fd;
	ion_param->iova		= (unsigned int) iva_map_node->io_va;
	mutex_unlock(&proc->mem_map_lock);
	return 0;

}

static int iva_mem_ion_free_param(struct iva_proc *proc,
			struct iva_ion_param *ion_param)
{
	int			ret = 0;
	struct iva_mem_map	*iva_map_node;
	struct iva_dev_data	*iva = proc->iva_data;
	struct device		*dev = iva->dev;

	mutex_lock(&proc->mem_map_lock);
	iva_map_node = iva_mem_find_proc_map_with_fd_nolock(proc, ion_param->shared_fd);
	if (!iva_map_node) {
		dev_err(dev, "%s() shared_fd(%d) is not found in list !!!\n",
				__func__, ion_param->shared_fd);
		mutex_unlock(&proc->mem_map_lock);
		return -EINVAL;
	}

	ret = iva_mem_ion_free(proc, iva_map_node);
	if (ret) {
		dev_warn(dev, "%s() shared_fd(%d) alloced outside.\n",
				__func__, ion_param->shared_fd);
		mutex_unlock(&proc->mem_map_lock);
		return -EINVAL;
	}
	mutex_unlock(&proc->mem_map_lock);
	return 0;
}

static void iva_mem_force_to_free_proc_mapped_list(struct iva_proc *proc)
{
	struct hlist_node	*tmp;
	struct iva_mem_map	*iva_map_node;
	struct iva_dev_data	*iva = proc->iva_data;
	int			i;
#ifdef CALL_SHOW_PROC_MAP_LIST
	struct device		*dev = iva->dev;
	int			j = 0;

	mutex_lock(&proc->mem_map_lock);
#else
	int			ret;

	mutex_lock(&proc->mem_map_lock);
	ret = iva_mem_show_proc_mapped_list_nolock(proc);
	if (ret == 0) {	/* nothing to do */
		mutex_unlock(&proc->mem_map_lock);
		return;
	}
#endif

	hash_for_each_safe(proc->h_mem_map, i, tmp, iva_map_node, h_node) {
	#ifdef CALL_SHOW_PROC_MAP_LIST
		dev_info(dev,
			"FORCE FREE [%3d, %3d] fd:%04d(%ld) flags(0x%x) ",
			i, j,
			iva_map_node->shared_fd,
			file_count(iva_map_node->dmabuf->file),
			iva_map_node->flags);
		dev_info(dev,
			"io_va(0x%08lx) size(0x%08x, 0x%08x) ref_cnt(%d)\n",
			iva_map_node->io_va,
			iva_map_node->req_size,
			iva_map_node->act_size,
			iva_mem_map_read_refcnt(iva_map_node));
	#endif
		/* mapping : get_iova? */
		if (iva_mem_map_read_refcnt(iva_map_node) > 0)
			iva_mem_put_ion_iova(proc, iva_map_node, true);

		/* release memory also iva_mem_map */
		if (ALLOC_INSIDE(iva_map_node->flags))
			iva_mem_ion_free(proc, iva_map_node);
		else
			iva_mem_cancel_imported_ion_buf(proc, iva_map_node);
	#ifdef CALL_SHOW_PROC_MAP_LIST
		j++;
	#endif
	}

	mutex_unlock(&proc->mem_map_lock);
}

static int iva_mem_get_ion_iova_param(struct iva_proc *proc,
			struct iva_ion_param *ion_param)
{
	int	ret;
	struct iva_mem_map	*iva_map_node;
	bool			new_map = false;	/* from outside */

	mutex_lock(&proc->mem_map_lock);
	iva_map_node = iva_mem_find_proc_map_with_fd_nolock(proc, ion_param->shared_fd);
	if (!iva_map_node) {	/* buf fd from outside */
		iva_map_node = iva_mem_import_ion_buf(proc, ion_param->shared_fd);
		if (!iva_map_node) {
			mutex_unlock(&proc->mem_map_lock);
			return -EINVAL;
		}
		new_map = true;
	}

	ret = iva_mem_get_ion_iova(proc, iva_map_node);
	if (ret < 0) {
		if (new_map)
			iva_mem_cancel_imported_ion_buf(proc, iva_map_node);

		mutex_unlock(&proc->mem_map_lock);
		return ret;
	}
	mutex_unlock(&proc->mem_map_lock);

	ion_param->iova = (unsigned int) iva_map_node->io_va;
	ion_param->size = iva_map_node->act_size;

	return 0;
}

static int iva_mem_put_ion_iova_param(struct iva_proc *proc,
			struct iva_ion_param *ion_param)
{
	int	ret;
	struct iva_mem_map	*iva_map_node;
	struct iva_dev_data	*iva = proc->iva_data;
	struct device		*dev = iva->dev;

	mutex_lock(&proc->mem_map_lock);
	iva_map_node = iva_mem_find_proc_map_with_fd_nolock(proc, ion_param->shared_fd);
	if (!iva_map_node) {
		dev_warn(dev, "%s() fail to get map node with buf fd(%d)\n",
				__func__, ion_param->shared_fd);
		mutex_unlock(&proc->mem_map_lock);
		return -EINVAL;
	}

	ret = iva_mem_put_ion_iova(proc, iva_map_node, false);
	if (!ret) {	/* success to iova unmap */
		if (ALLOC_OUTSIDE(iva_map_node->flags))	{
			/* if exported fd */
			iva_mem_cancel_imported_ion_buf(proc, iva_map_node);
		} else if (FREE_REQUESTED(iva_map_node->flags)) {
			/* already free is called and inside allocation */
			iva_mem_ion_free(proc, iva_map_node);
		}
	}
	mutex_unlock(&proc->mem_map_lock);

	return ret;
}

static int iva_mem_ion_sync_for_cpu_param(struct iva_proc *proc,
		struct iva_ion_param *ion_param)
{
	struct iva_mem_map	*iva_map_node;
	struct iva_dev_data	*iva = proc->iva_data;
	struct device		*dev = iva->dev;
	int			ret;

	mutex_lock(&proc->mem_map_lock);
	if (ion_param->shared_fd < 0) {
		dev_warn(dev, "%s() unexpected shared fd(%d)\n",
					__func__, ion_param->shared_fd);
		mutex_unlock(&proc->mem_map_lock);
		return -EINVAL;
	} else if (ion_param->shared_fd == 0) {	/* special purpose */
		iva_map_node = NULL;
	} else {
		iva_map_node = iva_mem_find_proc_map_with_fd_nolock(proc,
				ion_param->shared_fd);
		if (!iva_map_node) {
			dev_warn(dev, "%s() fail to get map node with buf fd(%d)\n",
					__func__, ion_param->shared_fd);
			mutex_unlock(&proc->mem_map_lock);
			return -EINVAL;
		}
	}

	ret = iva_mem_ion_sync_for_cpu(proc, iva_map_node);
	mutex_unlock(&proc->mem_map_lock);

	return ret;
}

static int iva_mem_ion_sync_for_device_param(struct iva_proc *proc,
		struct iva_ion_param *ion_param)
{
#define SYNC_ALL_FLUSH			(0)
#define SYNC_ALL_CLEAN			(1)
	struct iva_mem_map	*iva_map_node;
	struct iva_dev_data	*iva = proc->iva_data;
	struct device		*dev = iva->dev;
	bool			clean_only;
	int			ret;

	mutex_lock(&proc->mem_map_lock);
	if (ion_param->shared_fd < 0) {
		dev_warn(dev, "%s() unexpected shared fd(%d)\n",
					__func__, ion_param->shared_fd);
		mutex_unlock(&proc->mem_map_lock);
		return -EINVAL;
	} else if (ion_param->shared_fd == 0)	/* special purpose */
		iva_map_node = NULL;
	else {
		iva_map_node = iva_mem_find_proc_map_with_fd_nolock(proc,
				ion_param->shared_fd);
		if (!iva_map_node) {
			dev_warn(dev,
				"%s() fail to get map node with buf fd(%d)\n",
				__func__, ion_param->shared_fd);
			mutex_unlock(&proc->mem_map_lock);
			return -EINVAL;
		}
	}

	clean_only = !!(ion_param->cacheflag >> SYNC_ALL_TYPE_LOC);
	ret = iva_mem_ion_sync_for_device(proc, iva_map_node, clean_only);
	mutex_unlock(&proc->mem_map_lock);

	return ret;
}

long iva_mem_ioctl(struct iva_proc *proc, unsigned int cmd, void __user *p)
{
	struct iva_ion_param	iva_param;
	struct device		*dev = proc->iva_data->dev;
	long			ret;

	/* copy from user routine */
	if (_IOC_DIR(cmd) & _IOC_WRITE) {
		ret = copy_from_user(&iva_param, p, sizeof(iva_param));
		if (ret) {
			dev_err(dev, "%s() fail to copy_from_user, %d\n",
					__func__, (int) ret);
			return ret;
		}
	}

	switch (cmd) {
	case IVA_ION_GET_IOVA:
		ret = iva_mem_get_ion_iova_param(proc, &iva_param);
		break;

	case IVA_ION_PUT_IOVA:
		ret = iva_mem_put_ion_iova_param(proc, &iva_param);
		break;

	case IVA_ION_ALLOC:
		ret = iva_mem_ion_alloc_param(proc, &iva_param);
		break;

	case IVA_ION_FREE: /* This may not be used */
		ret = iva_mem_ion_free_param(proc, &iva_param);
		break;

	case IVA_ION_SYNC_FOR_CPU:
		ret = iva_mem_ion_sync_for_cpu_param(proc, &iva_param);
		break;

	case IVA_ION_SYNC_FOR_DEVICE:
		ret = iva_mem_ion_sync_for_device_param(proc, &iva_param);
		break;

	default:
		return -ENOTTY;
	}

	/* copy to user routine */
	if (ret >= 0 && _IOC_DIR(cmd) & _IOC_READ) {
		ret = copy_to_user((void __user *)p,
				&iva_param, sizeof(iva_param));
		if (ret) {
			dev_err(dev, "%s() fail to copy to user, %d\n",
				__func__, (int) ret);
		}
	}

	return ret;
}

void iva_mem_init_proc_mem(struct iva_proc *proc)
{
	proc->mem_map_nr = 0;
	mutex_init(&proc->mem_map_lock);
	hash_init(proc->h_mem_map);
}

void iva_mem_deinit_proc_mem(struct iva_proc *proc)
{
	iva_mem_force_to_free_proc_mapped_list(proc);
	mutex_destroy(&proc->mem_map_lock);
}


int iva_mem_create_ion_client(struct iva_dev_data *iva)
{
#define ION_IVA_CLIENT_NAME	"iva_ion"
	struct device		*dev = iva->dev;
	struct ion_client	*client;

#ifdef CONFIG_ION_EXYNOS
	client = exynos_ion_client_create(ION_IVA_CLIENT_NAME);
#else
	iva->ion_dev = idev;
	client = ion_client_create(iva->ion_dev, ION_IVA_CLIENT_NAME);
#endif
	if (IS_ERR_OR_NULL(client)) {
		dev_err(dev, "%s(%d) ion client creation is failed..\n",
				__func__, __LINE__);
		return -EINVAL;
	}

	dev_dbg(dev, "%s(%d) ion device is created with client %p\n",
				__func__, __LINE__, client);
	iva->ion_client = client;

	/* panic if kmem cache is not created */
	iva->map_node_cache = kmem_cache_create("iva_mem",
				sizeof(struct iva_mem_map),
				0, SLAB_PANIC, NULL);
	return 0;
}

int iva_mem_destroy_ion_client(struct iva_dev_data *iva)
{
	iva_mem_show_global_mapped_list(iva);

	kmem_cache_destroy(iva->map_node_cache);

	if (iva->ion_client) {
		ion_client_destroy(iva->ion_client);
		iva->ion_client = NULL;
	}
#ifndef CONFIG_ION_EXYNOS
	iva->ion_dev = NULL;
#endif
	return 0;
}
