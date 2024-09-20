/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_mem.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/iommu.h>

#include "s5p_mfc_mem.h"

struct vb2_mem_ops *s5p_mfc_mem_ops(void)
{
	return (struct vb2_mem_ops *)&vb2_ion_memops;
}

void s5p_mfc_mem_clean(struct s5p_mfc_dev *dev,
			struct s5p_mfc_special_buf *special_buf,
			off_t offset, size_t size)
{
	exynos_ion_sync_vaddr_for_device(dev->device, special_buf->dma_buf,
			special_buf->vaddr, size, offset, DMA_TO_DEVICE);
	return;
}

void s5p_mfc_mem_invalidate(struct s5p_mfc_dev *dev,
			struct s5p_mfc_special_buf *special_buf,
			off_t offset, size_t size)
{
	exynos_ion_sync_vaddr_for_device(dev->device, special_buf->dma_buf,
			special_buf->vaddr, size, offset, DMA_FROM_DEVICE);
	return;
}

int s5p_mfc_mem_clean_vb(struct vb2_buffer *vb, u32 num_planes)
{
	struct vb2_ion_cookie *cookie;
	int i;
	size_t size;

	for (i = 0; i < num_planes; i++) {
		cookie = vb2_plane_cookie(vb, i);
		if (!cookie)
			continue;

		size = vb->planes[i].length;
		vb2_ion_sync_for_device(cookie, 0, size, DMA_TO_DEVICE);
	}

	return 0;
}

int s5p_mfc_mem_inv_vb(struct vb2_buffer *vb, u32 num_planes)
{
	struct vb2_ion_cookie *cookie;
	int i;
	size_t size;

	for (i = 0; i < num_planes; i++) {
		cookie = vb2_plane_cookie(vb, i);
		if (!cookie)
			continue;

		size = vb->planes[i].length;
		vb2_ion_sync_for_device(cookie, 0, size, DMA_FROM_DEVICE);
	}

	return 0;
}

int s5p_mfc_mem_get_user_shared_handle(struct s5p_mfc_ctx *ctx,
	struct mfc_user_shared_handle *handle)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct dma_buf *dma_buf;
	int ret = 0;
	int fd = handle->fd;

	handle->ion_handle = ion_import_dma_buf_fd(dev->mfc_ion_client, fd);
	if (IS_ERR(handle->ion_handle)) {
		mfc_err_ctx("Failed to import fd\n");
		ret = PTR_ERR(handle->ion_handle);
		goto import_dma_fail;
	}

	dma_buf = dma_buf_get(fd);
	if (IS_ERR(dma_buf)) {
		mfc_err_ctx("Faiiled to dma_buf_get (err %ld)\n", PTR_ERR(dma_buf));
		ret = -EINVAL;
		goto dma_buf_get_fail;
	}

	if (dma_buf->size < handle->data_size) {
		mfc_err_ctx("User-provided dma_buf size(%ld) is smaller than required size(%ld)\n",
				dma_buf->size, handle->data_size);
		ret = -EINVAL;
		goto dma_buf_size_fail;
	}

	handle->vaddr =
		ion_map_kernel(dev->mfc_ion_client, handle->ion_handle);
	if (handle->vaddr == NULL) {
		mfc_err_ctx("Failed to get kernel virtual address\n");
		ret = -EINVAL;
		goto map_kernel_fail;
	}

	dma_buf_put(dma_buf);

	mfc_debug(2, "User Handle: fd = %d, virtual addr = 0x%p\n",
				handle->fd, handle->vaddr);

	return 0;

map_kernel_fail:
dma_buf_size_fail:
	dma_buf_put(dma_buf);
dma_buf_get_fail:
	ion_free(dev->mfc_ion_client, handle->ion_handle);
import_dma_fail:
	return ret;
}

int s5p_mfc_mem_cleanup_user_shared_handle(struct s5p_mfc_ctx *ctx,
		struct mfc_user_shared_handle *handle)
{
	struct s5p_mfc_dev *dev = ctx->dev;

	if (handle->fd == -1)
		return 0;

	if (handle->vaddr)
		ion_unmap_kernel(dev->mfc_ion_client,
					handle->ion_handle);

	ion_free(dev->mfc_ion_client, handle->ion_handle);

	return 0;
}

void *s5p_mfc_mem_get_vaddr(struct s5p_mfc_dev *dev,
		struct s5p_mfc_special_buf *special_buf)
{
	return ion_map_kernel(dev->mfc_ion_client, special_buf->handle);
}

void s5p_mfc_mem_ion_free(struct s5p_mfc_dev *dev,
		struct s5p_mfc_special_buf *special_buf)
{
	if (!special_buf->handle)
		return;

	if (special_buf->daddr)
		ion_iovmm_unmap(special_buf->attachment, special_buf->daddr);

	if (special_buf->handle) {
		dma_buf_unmap_attachment(special_buf->attachment,
				special_buf->sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach(special_buf->dma_buf, special_buf->attachment);
		dma_buf_put(special_buf->dma_buf);
		ion_free(dev->mfc_ion_client, special_buf->handle);
	}

	memset(&special_buf->handle, 0, sizeof(struct ion_handle *));
	memset(&special_buf->daddr, 0, sizeof(special_buf->daddr));

	special_buf->handle = NULL;
	special_buf->dma_buf = NULL;
	special_buf->attachment = NULL;
	special_buf->sgt = NULL;
	special_buf->daddr = 0;
	special_buf->vaddr = NULL;
}

int s5p_mfc_mem_ion_alloc(struct s5p_mfc_dev *dev,
		struct s5p_mfc_special_buf *special_buf)
{
	struct s5p_mfc_ctx *ctx = dev->ctx[dev->curr_ctx];
	int ion_mask, flag;

	switch (special_buf->buftype) {
	case MFCBUF_NORMAL:
		ion_mask = EXYNOS_ION_HEAP_SYSTEM_MASK;
		flag = 0;
		break;
	case MFCBUF_NORMAL_FW:
		ion_mask = EXYNOS_ION_HEAP_VIDEO_NFW_MASK;
		flag = 0;
		break;
	case MFCBUF_DRM:
		ion_mask = EXYNOS_ION_HEAP_VIDEO_FRAME_MASK;
		flag = ION_FLAG_PROTECTED;
		break;
	case MFCBUF_DRM_FW:
		ion_mask = EXYNOS_ION_HEAP_VIDEO_FW_MASK;
		flag = ION_FLAG_PROTECTED;
		break;
	default:
		mfc_err_ctx("not supported mfc mem type: %d\n",
				special_buf->buftype);
		return -EINVAL;
	}
	special_buf->handle = ion_alloc(dev->mfc_ion_client,
			special_buf->size, 0, ion_mask, flag);
	if (IS_ERR(special_buf->handle)) {
		mfc_err_ctx("Failed to allocate buffer (err %ld)",
				PTR_ERR(special_buf->handle));
		special_buf->handle = NULL;
		goto err_ion_alloc;
	}

	special_buf->dma_buf = ion_share_dma_buf(dev->mfc_ion_client,
			special_buf->handle);
	if (IS_ERR(special_buf->dma_buf)) {
		mfc_err_ctx("Failed to get dma_buf (err %ld)",
				PTR_ERR(special_buf->dma_buf));
		special_buf->dma_buf = NULL;
		goto err_ion_alloc;
	}

	special_buf->attachment = dma_buf_attach(special_buf->dma_buf, dev->device);
	if (IS_ERR(special_buf->attachment)) {
		mfc_err_ctx("Failed to get dma_buf_attach (err %ld)",
				PTR_ERR(special_buf->attachment));
		special_buf->attachment = NULL;
		goto err_ion_alloc;
	}

	special_buf->sgt = dma_buf_map_attachment(special_buf->attachment,
			DMA_BIDIRECTIONAL);
	if (IS_ERR(special_buf->sgt)) {
		mfc_err_ctx("Failed to get sgt (err %ld)",
				PTR_ERR(special_buf->sgt));
		special_buf->sgt = NULL;
		goto err_ion_alloc;
	}

	special_buf->daddr = ion_iovmm_map(special_buf->attachment, 0,
			special_buf->size, DMA_BIDIRECTIONAL, 0);
	if (IS_ERR_VALUE(special_buf->daddr)) {
		mfc_err_ctx("Failed to allocate iova (err %pa)",
				&special_buf->daddr);
		special_buf->daddr = 0;
		goto err_ion_alloc;
	}

	return 0;

err_ion_alloc:
	s5p_mfc_mem_ion_free(dev, special_buf);
	return -ENOMEM;
}

void s5p_mfc_put_iovmm(struct s5p_mfc_ctx *ctx, int num_planes, int index, int spare)
{
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	int i;

	for (i = 0; i < num_planes; i++) {
		if (dec->assigned_addr[index][spare][i]) {
			mfc_debug(2, "[IOVMM] index %d buf[%d] addr: %#llx\n",
					index, i, dec->assigned_addr[index][spare][i]);
			ion_iovmm_unmap(dec->assigned_attach[index][spare][i], dec->assigned_addr[index][spare][i]);
		}
		if (dec->assigned_attach[index][spare][i])
			dma_buf_detach(dec->assigned_dmabufs[index][spare][i], dec->assigned_attach[index][spare][i]);
		if (dec->assigned_dmabufs[index][spare][i])
			dma_buf_put(dec->assigned_dmabufs[index][spare][i]);

		dec->assigned_addr[index][spare][i] = 0;
		dec->assigned_attach[index][spare][i] = NULL;
		dec->assigned_dmabufs[index][spare][i] = NULL;
	}

	dec->assigned_refcnt[index]--;
	mfc_debug(2, "[IOVMM] index %d ref %d\n", index, dec->assigned_refcnt[index]);
}

void s5p_mfc_get_iovmm(struct s5p_mfc_ctx *ctx, struct vb2_buffer *vb)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	int i, mem_get_count = 0;
	int index = vb->index;
	int ioprot = IOMMU_WRITE;

	for (i = 0; i < ctx->dst_fmt->mem_planes; i++) {
		mem_get_count++;

		dec->assigned_dmabufs[index][0][i] = dma_buf_get(vb->planes[i].m.fd);
		if (IS_ERR(dec->assigned_dmabufs[index][0][i])) {
			mfc_err_ctx("[IOVMM] Failed to dma_buf_get (err %ld)\n",
					PTR_ERR(dec->assigned_dmabufs[index][0][i]));
			dec->assigned_dmabufs[index][0][i] = NULL;
			goto err_iovmm;
		}

		dec->assigned_attach[index][0][i] = dma_buf_attach(dec->assigned_dmabufs[index][0][i], dev->device);
		if (IS_ERR(dec->assigned_attach[index][0][i])) {
			mfc_err_ctx("[IOVMM] Failed to get dma_buf_attach (err %ld)\n",
					PTR_ERR(dec->assigned_attach[index][0][i]));
			dec->assigned_attach[index][0][i] = NULL;
			goto err_iovmm;
		}

		if (device_get_dma_attr(dev->device) == DEV_DMA_COHERENT)
			ioprot |= IOMMU_CACHE;

		dec->assigned_addr[index][0][i] = ion_iovmm_map(dec->assigned_attach[index][0][i],
				0, ctx->raw_buf.plane_size[i], DMA_FROM_DEVICE, ioprot);
		if (IS_ERR_VALUE(dec->assigned_addr[index][0][i])) {
			mfc_err_ctx("[IOVMM] Failed to allocate iova (err 0x%p)\n",
					&dec->assigned_addr[index][0][i]);
			dec->assigned_addr[index][0][i] = 0;
			goto err_iovmm;
		}
		mfc_debug(2, "[IOVMM] index %d buf[%d] addr: %#llx\n",
				index, i, dec->assigned_addr[index][0][i]);
	}

	dec->assigned_refcnt[index]++;
	mfc_debug(2, "[IOVMM] index %d ref %d\n", index, dec->assigned_refcnt[index]);

	return;

err_iovmm:
	dec->assigned_refcnt[index]++;
	s5p_mfc_put_iovmm(ctx, mem_get_count, index, 0);
}

void s5p_mfc_move_iovmm_to_spare(struct s5p_mfc_ctx *ctx, int num_planes, int index)
{
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	int i;

	for (i = 0; i < num_planes; i++) {
		dec->assigned_addr[index][1][i] = dec->assigned_addr[index][0][i];
		dec->assigned_attach[index][1][i] = dec->assigned_attach[index][0][i];
		dec->assigned_dmabufs[index][1][i] = dec->assigned_dmabufs[index][0][i];
		dec->assigned_addr[index][0][i] = 0;
		dec->assigned_attach[index][0][i] = NULL;
		dec->assigned_dmabufs[index][0][i] = NULL;
	}

	mfc_debug(2, "[IOVMM] moved DPB[%d] to spare\n", index);
}

void s5p_mfc_cleanup_assigned_iovmm(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	int i;

	mutex_lock(&dec->dpb_mutex);

	for (i = 0; i < MFC_MAX_DPBS; i++) {
		if (dec->assigned_refcnt[i] == 0) {
			continue;
		} else if (dec->assigned_refcnt[i] == 1) {
			s5p_mfc_put_iovmm(ctx, ctx->dst_fmt->mem_planes, i, 0);
		} else if (dec->assigned_refcnt[i] == 2) {
			s5p_mfc_put_iovmm(ctx, ctx->dst_fmt->mem_planes, i, 0);
			s5p_mfc_put_iovmm(ctx, ctx->dst_fmt->mem_planes, i, 1);
		} else {
			mfc_err_ctx("[IOVMM] index %d invalid refcnt %d\n", i, dec->assigned_refcnt[i]);
			MFC_TRACE_CTX("DPB[%d] invalid refcnt %d\n",
					i, dec->assigned_refcnt[i]);
		}
	}

	mutex_unlock(&dec->dpb_mutex);
}
