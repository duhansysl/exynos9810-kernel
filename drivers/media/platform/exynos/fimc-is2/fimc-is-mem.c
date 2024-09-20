/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is core functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <video/videonode.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/dma-buf.h>
#include <media/videobuf2-memops.h>

#include "fimc-is-core.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0))
#include <linux/exynos_iovmm.h>
#else
#include <plat/iovmm.h>
#endif

#if defined(CONFIG_VIDEOBUF2_ION)
struct vb2_ion_buf {
	struct device			*dev;
	struct vb2_vmarea_handler	handler;
	struct vm_area_struct		*vma;
	struct ion_handle		*handle;
	struct dma_buf			*dma_buf;
	struct dma_buf_attachment	*attachment;
	enum dma_data_direction		direction;
	void				*kva;
	unsigned long			size;
	atomic_t			ref;
	bool				cached;
	bool				ion;
	struct vb2_ion_cookie		cookie;
};

/* fimc-is vb2 buffer operations */
static inline ulong fimc_is_vb2_ion_plane_kvaddr(
		struct fimc_is_vb2_buf *vbuf, u32 plane)

{
	return (ulong)vb2_plane_vaddr(&vbuf->vb.vb2_buf, plane);
}

static dma_addr_t fimc_is_vb2_ion_plane_dvaddr(
		struct fimc_is_vb2_buf *vbuf, u32 plane)

{
	dma_addr_t dva = 0;

	WARN_ON(vb2_ion_dma_address(vb2_plane_cookie(&vbuf->vb.vb2_buf, plane), &dva) != 0);

	return (ulong)dva;
}

static void fimc_is_vb2_ion_plane_prepare(struct fimc_is_vb2_buf *vbuf,
		u32 plane, bool exact)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	enum dma_data_direction dir;
	unsigned long size;
	u32 spare;

	/* skip meta plane */
	spare = vb->num_planes - 1;
	if (plane == spare)
		return;

	dir = V4L2_TYPE_IS_OUTPUT(vb->type) ?
		DMA_TO_DEVICE : DMA_FROM_DEVICE;

	size = exact ?
		vb2_get_plane_payload(vb, plane) : vb2_plane_size(vb, plane);

	vb2_ion_sync_for_device((void *)vb2_plane_cookie(vb, plane), 0,
			size, dir);
}

static void fimc_is_vb2_ion_plane_finish(struct fimc_is_vb2_buf *vbuf,
		u32 plane, bool exact)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	enum dma_data_direction dir;
	unsigned long size;
	u32 spare;

	/* skip meta plane */
	spare = vb->num_planes - 1;
	if (plane == spare)
		return;

	dir = V4L2_TYPE_IS_OUTPUT(vb->type) ?
		DMA_TO_DEVICE : DMA_FROM_DEVICE;

	size = exact ?
		vb2_get_plane_payload(vb, plane) : vb2_plane_size(vb, plane);

	vb2_ion_sync_for_cpu((void *)vb2_plane_cookie(vb, plane), 0,
			size, dir);
}

static void fimc_is_vb2_ion_buf_prepare(struct fimc_is_vb2_buf *vbuf, bool exact)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	enum dma_data_direction dir;
	unsigned long size;
	u32 plane;
	u32 spare;

	dir = V4L2_TYPE_IS_OUTPUT(vb->type) ?
		DMA_TO_DEVICE : DMA_FROM_DEVICE;

	/* skip meta plane */
	spare = vb->num_planes - 1;

	for (plane = 0; plane < spare; plane++) {
		size = exact ?
			vb2_get_plane_payload(vb, plane) : vb2_plane_size(vb, plane);

		vb2_ion_sync_for_device((void *)vb2_plane_cookie(vb, plane), 0,
				size, dir);
	}
}

static void fimc_is_vb2_ion_buf_finish(struct fimc_is_vb2_buf *vbuf, bool exact)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	enum dma_data_direction dir;
	unsigned long size;
	u32 plane;
	u32 spare;

	dir = V4L2_TYPE_IS_OUTPUT(vb->type) ?
		DMA_TO_DEVICE : DMA_FROM_DEVICE;

	/* skip meta plane */
	spare = vb->num_planes - 1;

	for (plane = 0; plane < spare; plane++) {
		size = exact ?
			vb2_get_plane_payload(vb, plane) : vb2_plane_size(vb, plane);

		vb2_ion_sync_for_cpu((void *)vb2_plane_cookie(vb, plane), 0,
				size, dir);
	}

	clear_bit(FIMC_IS_VBUF_CACHE_INVALIDATE, &vbuf->cache_state);
}

static long is_vb2_ion_buf_remap_attr(struct fimc_is_vb2_buf *vbuf, int attr)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct vb2_ion_buf *buf;
	unsigned int plane;
	long ret;
	int ioprot = IOMMU_READ	| IOMMU_WRITE;

	for (plane = 0; plane < vb->num_planes; ++plane) {
		buf = vb->planes[plane].mem_priv;
		vbuf->sgt[plane] = dma_buf_map_attachment(buf->attachment,
								buf->direction);

		if (IS_ERR_OR_NULL(vbuf->sgt[plane])) {
			ret = -EINVAL;
			pr_err("Error getting dmabuf scatterlist\n");
			goto err_get_sgt;
		}

		if ((vbuf->dva[plane] == 0) || IS_ERR_VALUE(vbuf->dva[plane])) {
			if (device_get_dma_attr(buf->dev) == DEV_DMA_COHERENT)
				ioprot |= IOMMU_CACHE;

			vbuf->dva[plane] = ion_iovmm_map_attr(buf->attachment,0,
						buf->size, DMA_BIDIRECTIONAL,
						ioprot, attr);
			if (IS_ERR_VALUE(vbuf->dva[plane])) {
				ret = vbuf->dva[plane];
				pr_err("Error from ion_iovmm_map_attr(): %ld\n", ret);
				goto err_map_remap;
			}
		}
	}

	return 0;

err_map_remap:
	dma_buf_unmap_attachment(buf->attachment, vbuf->sgt[plane], DMA_BIDIRECTIONAL);
	vbuf->dva[plane] = 0;

err_get_sgt:
	while (plane-- > 0) {
		buf = vb->planes[plane].mem_priv;

		ion_iovmm_unmap_attr(buf->attachment, vbuf->dva[plane], attr);
		dma_buf_unmap_attachment(buf->attachment, vbuf->sgt[plane],
								DMA_BIDIRECTIONAL);
		vbuf->dva[plane] = 0;
	}

	return ret;
}

void is_vb2_ion_buf_unremap_attr(struct fimc_is_vb2_buf *vbuf, int attr)
{
	struct vb2_buffer *vb = &vbuf->vb.vb2_buf;
	struct vb2_ion_buf *buf;
	unsigned int plane;

	for (plane = 0; plane < vb->num_planes; ++plane) {
		buf = vb->planes[plane].mem_priv;

		ion_iovmm_unmap_attr(buf->attachment, vbuf->dva[plane], attr);
		dma_buf_unmap_attachment(buf->attachment, vbuf->sgt[plane],
								DMA_BIDIRECTIONAL);
		vbuf->dva[plane] = 0;
	}
}

static dma_addr_t fimc_is_bufcon_map(struct fimc_is_vb2_buf *vbuf,
	struct device *dev,
	int idx,
	struct dma_buf *dmabuf)
{
	int ret = 0;

	vbuf->bufs[idx] = dmabuf;

	vbuf->attachment[idx] = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(vbuf->attachment[idx])) {
		ret = PTR_ERR(vbuf->attachment[idx]);
		pr_err("%s: Failed to dma_buf_attach %d\n", __func__, idx);
		goto err_attach;
	}

	vbuf->sgt[idx] = dma_buf_map_attachment(vbuf->attachment[idx], DMA_BIDIRECTIONAL);
	if (IS_ERR(vbuf->sgt[idx])) {
		ret = PTR_ERR(vbuf->sgt[idx]);
		pr_err("%s: Failed to dma_buf_map_attachment %d\n", __func__, idx);
		goto err_map_dmabuf;
	}

	vbuf->dva[idx] = ion_iovmm_map(vbuf->attachment[idx], 0, 0, DMA_BIDIRECTIONAL, 0);
	if (IS_ERR_VALUE(vbuf->dva[idx])) {
		pr_err("%s: Failed to ion_iovmm_map %d\n", __func__, idx);
		ret = -ENOMEM;
		goto err_ion_map_io;
	}

	return vbuf->dva[idx];

err_ion_map_io:
	dma_buf_unmap_attachment(vbuf->attachment[idx], vbuf->sgt[idx], DMA_BIDIRECTIONAL);
err_map_dmabuf:
	dma_buf_detach(dmabuf, vbuf->attachment[idx]);
err_attach:
	vbuf->bufs[idx] = NULL;

	return 0;
}

static void fimc_is_bufcon_unmap(struct fimc_is_vb2_buf *vbuf,
	int idx)
{
	if (!vbuf->bufs[idx])
		return;

	ion_iovmm_unmap(vbuf->attachment[idx], vbuf->dva[idx]);
	vbuf->dva[idx] = 0;

	dma_buf_unmap_attachment(vbuf->attachment[idx], vbuf->sgt[idx], DMA_BIDIRECTIONAL);
	dma_buf_detach(vbuf->bufs[idx], vbuf->attachment[idx]);

	vbuf->bufs[idx] = NULL;
}

const struct fimc_is_vb2_buf_ops fimc_is_vb2_buf_ops_ion = {
	.plane_kvaddr	= fimc_is_vb2_ion_plane_kvaddr,
	.plane_dvaddr	= fimc_is_vb2_ion_plane_dvaddr,
	.plane_prepare	= fimc_is_vb2_ion_plane_prepare,
	.plane_finish	= fimc_is_vb2_ion_plane_finish,
	.buf_prepare	= fimc_is_vb2_ion_buf_prepare,
	.buf_finish	= fimc_is_vb2_ion_buf_finish,
	.remap_attr	= is_vb2_ion_buf_remap_attr,
	.unremap_attr	= is_vb2_ion_buf_unremap_attr,
	.bufcon_map	= fimc_is_bufcon_map,
	.bufcon_unmap	= fimc_is_bufcon_unmap,

};

/* fimc-is private buffer operations */
static void fimc_is_ion_free(struct fimc_is_priv_buf *pbuf)
{
	struct fimc_is_ion_ctx *alloc_ctx;

	alloc_ctx = pbuf->ctx;
	mutex_lock(&alloc_ctx->lock);
	if (pbuf->cookie.ioaddr)
		ion_iovmm_unmap(pbuf->attachment, pbuf->cookie.ioaddr);
	mutex_unlock(&alloc_ctx->lock);

	dma_buf_unmap_attachment(pbuf->attachment, pbuf->cookie.sgt,
				DMA_BIDIRECTIONAL);
	dma_buf_detach(pbuf->dma_buf, pbuf->attachment);
	dma_buf_put(pbuf->dma_buf);

	if (pbuf->kva)
		ion_unmap_kernel(alloc_ctx->client, pbuf->handle);
	ion_free(alloc_ctx->client, pbuf->handle);

	vfree(pbuf);
}

static ulong fimc_is_ion_kvaddr(struct fimc_is_priv_buf *pbuf)
{
	struct fimc_is_ion_ctx *alloc_ctx;

	if (!pbuf)
		return 0;

	alloc_ctx = pbuf->ctx;
	if (!pbuf->kva) {
		pbuf->kva = ion_map_kernel(alloc_ctx->client, pbuf->handle);
		if (IS_ERR_OR_NULL(pbuf->kva))
			pbuf->kva = NULL;

		pbuf->kva += pbuf->cookie.offset;
	}

	return (ulong)pbuf->kva;
}

static dma_addr_t fimc_is_ion_dvaddr(struct fimc_is_priv_buf *pbuf)
{
	dma_addr_t dva = 0;

	if (!pbuf)
		return -EINVAL;

	if (pbuf->cookie.ioaddr == 0)
		return -EINVAL;

	dva = pbuf->cookie.ioaddr;

	return (ulong)dva;
}

static phys_addr_t fimc_is_ion_phaddr(struct fimc_is_priv_buf *pbuf)
{
	/* PHCONTIG option is not supported in ION */
	return 0;
}

static void fimc_is_ion_sync_for_device(struct fimc_is_priv_buf *pbuf,
		off_t offset, size_t size, enum dma_data_direction dir)
{
	struct fimc_is_ion_ctx *alloc_ctx = pbuf->ctx;

	if (pbuf->kva) {
		FIMC_BUG_VOID((offset < 0) || (offset > pbuf->size));
		FIMC_BUG_VOID((offset + size) < size);
		FIMC_BUG_VOID((size > pbuf->size) || ((offset + size) > pbuf->size));

		exynos_ion_sync_vaddr_for_device(alloc_ctx->dev,
			pbuf->dma_buf, pbuf->kva, size, offset, dir);
	}
}

static void fimc_is_ion_sync_for_cpu(struct fimc_is_priv_buf *pbuf,
		off_t offset, size_t size, enum dma_data_direction dir)
{
	struct fimc_is_ion_ctx *alloc_ctx = pbuf->ctx;

	if (pbuf->kva) {
		FIMC_BUG_VOID((offset < 0) || (offset > pbuf->size));
		FIMC_BUG_VOID((offset + size) < size);
		FIMC_BUG_VOID((size > pbuf->size) || ((offset + size) > pbuf->size));

		exynos_ion_sync_vaddr_for_cpu(alloc_ctx->dev,
			pbuf->dma_buf, pbuf->kva, size, offset, dir);
	}
}

const struct fimc_is_priv_buf_ops fimc_is_priv_buf_ops_ion = {
	.free			= fimc_is_ion_free,
	.kvaddr			= fimc_is_ion_kvaddr,
	.dvaddr			= fimc_is_ion_dvaddr,
	.phaddr			= fimc_is_ion_phaddr,
	.sync_for_device	= fimc_is_ion_sync_for_device,
	.sync_for_cpu		= fimc_is_ion_sync_for_cpu,
};

/* fimc-is memory operations */
static void *fimc_is_ion_init(struct platform_device *pdev)
{
	struct fimc_is_ion_ctx *ctx;

	ctx = vzalloc(sizeof(*ctx));
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ctx->dev = &pdev->dev;
	ctx->client = exynos_ion_client_create(dev_name(&pdev->dev));
	if (IS_ERR(ctx->client)) {
		void *retp = ctx->client;
		kfree(ctx);
		return retp;
	}

	ctx->alignment = SZ_4K;
	ctx->flags = ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC;
	mutex_init(&ctx->lock);

	return ctx;
}

static void fimc_is_ion_deinit(void *ctx)
{
	struct fimc_is_ion_ctx *alloc_ctx = ctx;

	mutex_destroy(&alloc_ctx->lock);
	ion_client_destroy(alloc_ctx->client);
	vfree(alloc_ctx);
}

static struct fimc_is_priv_buf *fimc_is_ion_alloc(void *ctx,
		size_t size, unsigned int heap_id_mask, unsigned int flags)
{
	struct fimc_is_ion_ctx *alloc_ctx = ctx;
	struct fimc_is_priv_buf *buf;
	int ret = 0;

	buf = vzalloc(sizeof(*buf));
	if (!buf)
		return ERR_PTR(-ENOMEM);

	if (heap_id_mask)
		info("heap_id_mask: 0x%08x, size: 0x%lx\n", heap_id_mask, size);

	size = PAGE_ALIGN(size);

	buf->handle = ion_alloc(alloc_ctx->client, size, alloc_ctx->alignment,
				heap_id_mask ? heap_id_mask : EXYNOS_ION_HEAP_SYSTEM_MASK,
				flags ? flags : alloc_ctx->flags);
	if (IS_ERR(buf->handle)) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	buf->dma_buf = ion_share_dma_buf(alloc_ctx->client, buf->handle);
	if (IS_ERR(buf->dma_buf)) {
		ret = PTR_ERR(buf->dma_buf);
		goto err_share;
	}

	buf->attachment = dma_buf_attach(buf->dma_buf, alloc_ctx->dev);
	if (IS_ERR(buf->attachment)) {
		ret = PTR_ERR(buf->attachment);
		goto err_attach;
	}

	buf->cookie.sgt = dma_buf_map_attachment(
				buf->attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(buf->cookie.sgt)) {
		ret = PTR_ERR(buf->cookie.sgt);
		goto err_map_dmabuf;
	}

	buf->ctx = alloc_ctx;
	buf->size = size;
	buf->direction = DMA_BIDIRECTIONAL;
	buf->ops = &fimc_is_priv_buf_ops_ion;

	mutex_lock(&alloc_ctx->lock);
	buf->cookie.ioaddr = ion_iovmm_map(buf->attachment, 0,
				   buf->size, buf->direction, 0);
	if (IS_ERR_VALUE(buf->cookie.ioaddr)) {
		ret = (int)buf->cookie.ioaddr;
		mutex_unlock(&alloc_ctx->lock);
		goto err_ion_map_io;
	}
	mutex_unlock(&alloc_ctx->lock);

	return buf;

err_ion_map_io:
	if (buf->kva)
		ion_unmap_kernel(alloc_ctx->client, buf->handle);
	dma_buf_unmap_attachment(buf->attachment, buf->cookie.sgt,
				DMA_BIDIRECTIONAL);
err_map_dmabuf:
	dma_buf_detach(buf->dma_buf, buf->attachment);
err_attach:
	dma_buf_put(buf->dma_buf);
err_share:
	ion_free(alloc_ctx->client, buf->handle);
err_alloc:
	vfree(buf);

	pr_err("%s: Error occured while allocating\n", __func__);
	return ERR_PTR(ret);
}

static int fimc_is_ion_resume(void *ctx)
{
	struct fimc_is_ion_ctx *alloc_ctx = ctx;
	int ret = 0;

	if (!alloc_ctx)
		return -ENOENT;

	mutex_lock(&alloc_ctx->lock);
	if (alloc_ctx->iommu_active_cnt == 0)
		ret = iovmm_activate(alloc_ctx->dev);
	if (!ret)
		alloc_ctx->iommu_active_cnt++;
	mutex_unlock(&alloc_ctx->lock);

	return ret;
}

static void fimc_is_ion_suspend(void *ctx)
{
	struct fimc_is_ion_ctx *alloc_ctx = ctx;

	if (!alloc_ctx)
		return;

	mutex_lock(&alloc_ctx->lock);
	FIMC_BUG_VOID(alloc_ctx->iommu_active_cnt == 0);

	if (--alloc_ctx->iommu_active_cnt == 0)
		iovmm_deactivate(alloc_ctx->dev);
	mutex_unlock(&alloc_ctx->lock);
}

const struct fimc_is_mem_ops fimc_is_mem_ops_ion = {
	.init			= fimc_is_ion_init,
	.cleanup		= fimc_is_ion_deinit,
	.resume			= fimc_is_ion_resume,
	.suspend		= fimc_is_ion_suspend,
	.alloc			= fimc_is_ion_alloc,
};
#endif

/* fimc-is private buffer operations */
static void fimc_is_km_free(struct fimc_is_priv_buf *pbuf)
{
	kfree(pbuf->kvaddr);
	kfree(pbuf);
}

static ulong fimc_is_km_kvaddr(struct fimc_is_priv_buf *pbuf)
{
	if (!pbuf)
		return 0;

	return (ulong)pbuf->kvaddr;
}

static phys_addr_t fimc_is_km_phaddr(struct fimc_is_priv_buf *pbuf)
{
	phys_addr_t pa = 0;

	if (!pbuf)
		return 0;

	pa = virt_to_phys(pbuf->kvaddr);

	return pa;
}
const struct fimc_is_priv_buf_ops fimc_is_priv_buf_ops_km = {
	.free			= fimc_is_km_free,
	.kvaddr			= fimc_is_km_kvaddr,
	.phaddr			= fimc_is_km_phaddr,
};

static struct fimc_is_priv_buf *fimc_is_kmalloc(size_t size, size_t align)
{
	struct fimc_is_priv_buf *buf = NULL;
	int ret = 0;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	buf->kvaddr = kzalloc(DEBUG_REGION_SIZE + 0x10, GFP_KERNEL);
	if (!buf->kvaddr) {
		ret = -ENOMEM;
		goto err_priv_alloc;
	}

	buf->size = size;
	buf->align = align;
	buf->ops = &fimc_is_priv_buf_ops_km;

	return buf;

err_priv_alloc:
	kfree(buf);

	return ERR_PTR(ret);
}

int fimc_is_mem_init(struct fimc_is_mem *mem, struct platform_device *pdev)
{
#if defined(CONFIG_VIDEOBUF2_ION)
	mem->fimc_is_mem_ops = &fimc_is_mem_ops_ion;
	mem->vb2_mem_ops = &vb2_ion_memops;
	mem->fimc_is_vb2_buf_ops = &fimc_is_vb2_buf_ops_ion;
	mem->kmalloc = &fimc_is_kmalloc;
#endif

	mem->default_ctx = CALL_PTR_MEMOP(mem, init, pdev);
	if (IS_ERR_OR_NULL(mem->default_ctx)) {
		if (IS_ERR(mem->default_ctx))
			return PTR_ERR(mem->default_ctx);
		else
			return -EINVAL;
	}

	return 0;
}
