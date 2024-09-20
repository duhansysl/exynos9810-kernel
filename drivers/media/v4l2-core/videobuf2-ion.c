/* linux/drivers/media/video/videobuf2-ion.c *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Implementation of Android ION memory allocator for videobuf2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/dma-buf.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-memops.h>
#include <media/videobuf2-ion.h>

#include <linux/exynos_iovmm.h>
#include <linux/dmabuf_container.h>

#include <asm/cacheflush.h>

#define vb2ion_err(dev, fmt, ...) \
	dev_err(dev, "VB2ION: " pr_fmt(fmt), ##__VA_ARGS__)

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

#define CACHE_FLUSH_ALL_SIZE	(SZ_8M + SZ_2M)
#define DMA_SYNC_SIZE		SZ_512K
#define OUTER_FLUSH_ALL_SIZE	SZ_1M

static bool vb2_ion_dma_is_coherent(struct device *dev,
				    enum dma_data_direction dir)
{
	if (!(device_get_dma_attr(dev) == DEV_DMA_COHERENT))
		return false;
	/* TODO: condition for VB2ION_CTX_UNCACHED_READ_DMA */
	return (dir != DMA_TO_DEVICE);
}

static int get_iommu_prot(struct device *dev,
			  enum dma_data_direction dir)
{
	int prot;

	if (dir == DMA_TO_DEVICE)
		prot = IOMMU_READ;
	else if (dir == DMA_FROM_DEVICE)
		prot = IOMMU_WRITE;
	else
		prot = IOMMU_READ | IOMMU_WRITE;

	if (vb2_ion_dma_is_coherent(dev, dir))
		prot |= IOMMU_CACHE;

	return prot;
}

/**
 * vb2_get_vma() - acquire and lock the virtual memory area
 * @vma:        given virtual memory area
 *
 * This function attempts to acquire an area mapped in the userspace for
 * the duration of a hardware operation. The area is "locked" by performing
 * the same set of operation that are done when process calls fork() and
 * memory areas are duplicated.
 *
 * Returns a copy of a virtual memory region on success or NULL.
 */
static struct vm_area_struct *vb2_get_vma(struct vm_area_struct *vma)
{
	struct vm_area_struct *vma_copy;

	vma_copy = kmalloc(sizeof(*vma_copy), GFP_KERNEL);
	if (vma_copy == NULL)
		return NULL;

	if (vma->vm_ops && vma->vm_ops->open)
		vma->vm_ops->open(vma);

	if (vma->vm_file)
		get_file(vma->vm_file);

	memcpy(vma_copy, vma, sizeof(*vma));

	vma_copy->vm_mm = NULL;
	vma_copy->vm_next = NULL;
	vma_copy->vm_prev = NULL;

	return vma_copy;
}

/**
 * vb2_put_userptr() - release a userspace virtual memory area
 * @vma:        virtual memory region associated with the area to be released
 *
 * This function releases the previously acquired memory area after a hardware
 * operation.
 */
static void vb2_put_vma(struct vm_area_struct *vma)
{
	if (!vma)
		return;

	if (vma->vm_ops && vma->vm_ops->close)
		vma->vm_ops->close(vma);

	if (vma->vm_file)
		fput(vma->vm_file);

	kfree(vma);
}

void vb2_ion_set_noncoherent_dma_read(struct device *dev, bool noncoherent)
{
	/* TODO: store information into dev */
}
EXPORT_SYMBOL(vb2_ion_set_noncoherent_dma_read);

static void vb2_ion_put(void *buf_priv)
{
	/* TODO */
}

static void *vb2_ion_alloc(struct device *dev, unsigned long attrs,
		unsigned long size,
		enum dma_data_direction dma_dir, gfp_t gfp_flags)
{
	/* TODO */
	return NULL;
}

static void *vb2_ion_cookie(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;

	if (WARN_ON(!buf))
		return NULL;

	return (void *)&buf->cookie;
}

static void *vb2_ion_vaddr(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;

	if (WARN_ON(!buf))
		return NULL;

	if (buf->kva != NULL)
		return buf->kva;

	if (dma_buf_begin_cpu_access(buf->dma_buf, buf->direction))
		return NULL;

	buf->kva = dma_buf_kmap(buf->dma_buf, buf->cookie.offset / PAGE_SIZE);

	if (buf->kva == NULL)
		dma_buf_end_cpu_access(buf->dma_buf, buf->direction);
	else
		buf->kva += buf->cookie.offset & ~PAGE_MASK;

	return buf->kva;
}

static unsigned int vb2_ion_num_users(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;

	if (WARN_ON(!buf))
		return 0;

	return atomic_read(&buf->ref);
}

static int vb2_ion_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct vb2_ion_buf *buf = buf_priv;
	unsigned long vm_start = vma->vm_start;
	unsigned long vm_end = vma->vm_end;
	struct scatterlist *sg = buf->cookie.sgt->sgl;
	unsigned long size;
	int ret = -EINVAL;

	if (buf->size  < (vm_end - vm_start))
		return ret;

	if (!buf->cached)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	size = min_t(size_t, vm_end - vm_start, sg_dma_len(sg));

	ret = remap_pfn_range(vma, vm_start, page_to_pfn(sg_page(sg)),
				size, vma->vm_page_prot);

	for (sg = sg_next(sg), vm_start += size;
			!ret && sg && (vm_start < vm_end);
			vm_start += size, sg = sg_next(sg)) {
		size = min_t(size_t, vm_end - vm_start, sg_dma_len(sg));
		ret = remap_pfn_range(vma, vm_start, page_to_pfn(sg_page(sg)),
						size, vma->vm_page_prot);
	}

	if (ret)
		return ret;

	if (vm_start < vm_end)
		return -EINVAL;

	vma->vm_flags		|= VM_DONTEXPAND;
	vma->vm_private_data	= &buf->handler;
	vma->vm_ops		= &vb2_common_vm_ops;

	vma->vm_ops->open(vma);

	return ret;
}

static int vb2_ion_map_dmabuf(void *mem_priv)
{
	struct vb2_ion_buf *buf = mem_priv;

	if (WARN_ON(!buf->attachment)) {
		pr_err("trying to pin a non attached buffer\n");
		return -EINVAL;
	}

	if (WARN_ON(buf->cookie.sgt)) {
		pr_err("dmabuf buffer is already pinned\n");
		return 0;
	}

	/* get the associated scatterlist for this buffer */
	buf->cookie.sgt = dma_buf_map_attachment(buf->attachment,
						buf->direction);
	if (IS_ERR_OR_NULL(buf->cookie.sgt)) {
		pr_err("Error getting dmabuf scatterlist\n");
		return -EINVAL;
	}

	/*
	 * A dmabuf container itself is also a dmabuf. It is capable of attach
	 * and map but the buffer in it is not accessible because it just has
	 * metadata of dma-buf array.
	 */
	if (dmabuf_container_get_count(buf->dma_buf) > 0)
		return 0;

	buf->cookie.offset = 0;
	buf->cookie.paddr = sg_phys(buf->cookie.sgt->sgl) + buf->cookie.offset;

	if (buf->cookie.ioaddr == 0) {
		buf->cookie.ioaddr = ion_iovmm_map(buf->attachment, 0,
					buf->size, buf->direction,
					get_iommu_prot(buf->dev, buf->direction));
		if (IS_ERR_VALUE(buf->cookie.ioaddr)) {
			pr_err("buf->cookie.ioaddr is error: %pa\n",
					&buf->cookie.ioaddr);
			dma_buf_unmap_attachment(buf->attachment,
					buf->cookie.sgt, buf->direction);
			return (int)buf->cookie.ioaddr;
		}
	}

	return 0;
}

static void vb2_ion_unmap_dmabuf(void *mem_priv)
{
	struct vb2_ion_buf *buf = mem_priv;

	if (WARN_ON(!buf->attachment)) {
		pr_err("trying to unpin a not attached buffer\n");
		return;
	}

	if (WARN_ON(!buf->cookie.sgt)) {
		pr_err("dmabuf buffer is already unpinned\n");
		return;
	}

	dma_buf_unmap_attachment(buf->attachment,
			buf->cookie.sgt, buf->direction);

	buf->cookie.sgt = NULL;
}

static void vb2_ion_detach_dmabuf(void *mem_priv)
{
	struct vb2_ion_buf *buf = mem_priv;
	if (buf->cookie.ioaddr) {
		ion_iovmm_unmap(buf->attachment, buf->cookie.ioaddr);
		buf->cookie.ioaddr = 0;
	}

	if (buf->kva != NULL) {
		dma_buf_kunmap(buf->dma_buf, 0, buf->kva);
		dma_buf_end_cpu_access(buf->dma_buf, 0);
	}

	/* detach this attachment */
	dma_buf_detach(buf->dma_buf, buf->attachment);
	kfree(buf);
}

static void *vb2_ion_attach_dmabuf(struct device *dev, struct dma_buf *dbuf,
		unsigned long size, enum dma_data_direction dma_dir)
{
	struct vb2_ion_buf *buf;
	struct dma_buf_attachment *attachment;

	if (dbuf->size < size) {
		WARN(1, "dbuf->size(%zd) is smaller than size(%ld)\n",
				dbuf->size, size);
		return ERR_PTR(-EFAULT);
	}

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf) {
		pr_err("out of memory\n");
		return ERR_PTR(-ENOMEM);
	}

	buf->dev = dev;
	/* create attachment for the dmabuf with the user device */
	attachment = dma_buf_attach(dbuf, buf->dev);
	if (IS_ERR(attachment)) {
		pr_err("failed to attach dmabuf\n");
		kfree(buf);
		return attachment;
	}

	buf->direction = dma_dir;
	buf->size = size;
	buf->dma_buf = dbuf;
	buf->ion = true;
	buf->attachment = attachment;

	return buf;
}

static void vb2_ion_put_vma(struct vm_area_struct *vma)
{
	while (vma) {
		struct vm_area_struct *tmp;

		tmp = vma;
		vma = vma->vm_prev;
		vb2_put_vma(tmp);
	}
}

/*
 * Verify the user's input and
 * obtain the reference to the vma against the user's area
 * Returns the last node of doubly linked list of replicated vma.
 */
static struct vm_area_struct *vb2_ion_get_vma(struct device *dev,
				unsigned long vaddr, unsigned long size)
{
	struct vm_area_struct *new_vma = NULL;
	struct vm_area_struct *vma;
	unsigned long addr = vaddr;
	unsigned long len = size;

	if ((vaddr + size) <= vaddr) {
		vb2ion_err(dev, "size overflow in user area, [%#lx, %#lx)\n",
				vaddr, size);
		return NULL;
	}

	down_read(&current->mm->mmap_sem);

	for (vma = find_vma(current->mm, addr);
		vma && len && (addr >= vma->vm_start); vma = vma->vm_next) {
		struct vm_area_struct *cur_vma;

		cur_vma = vb2_get_vma(vma);

		if (new_vma) {
			if ((cur_vma->vm_file != new_vma->vm_file) ||
				(cur_vma->vm_ops != new_vma->vm_ops)) {
				vb2ion_err(dev,
					"[%#lx, %#lx) crosses disparate vmas\n",
					vaddr, size);
				vb2_put_vma(cur_vma);
				break;
			}
			new_vma->vm_next = cur_vma;
		}
		cur_vma->vm_prev = new_vma;
		new_vma = cur_vma;

		if ((addr + len) <= vma->vm_end) {
			addr = addr + len;
			len = 0;
		} else {
			len -= vma->vm_end - addr;
			addr = vma->vm_end;
		}
	}

	up_read(&current->mm->mmap_sem);

	if (len) { /* error detected */
		vb2ion_err(dev, "Invalid user area [%#lx, %#lx)\n",
				vaddr, size);
		vb2_ion_put_vma(new_vma);
		return NULL;
	}

	while (new_vma && new_vma->vm_prev)
		new_vma = new_vma->vm_prev;

	return new_vma;
}

static void vb2_ion_put_userptr_dmabuf(struct vb2_ion_buf *buf)
{
	ion_iovmm_unmap(buf->attachment,
			buf->cookie.ioaddr - buf->cookie.offset);

	dma_buf_unmap_attachment(buf->attachment,
				 buf->cookie.sgt, buf->direction);
	dma_buf_detach(buf->dma_buf, buf->attachment);
}

static void *vb2_ion_get_userptr_dmabuf(struct vb2_ion_buf *buf,
						unsigned long vaddr)
{
	void *ret;

	buf->attachment = dma_buf_attach(buf->dma_buf, buf->dev);
	if (IS_ERR(buf->attachment)) {
		dev_err(buf->dev,
			"%s: Failed to pin user buffer @ %#lx/%#lx\n",
			__func__, vaddr, buf->size);
		return ERR_CAST(buf->attachment);
	}

	buf->cookie.sgt = dma_buf_map_attachment(buf->attachment,
						buf->direction);
	if (IS_ERR(buf->cookie.sgt)) {
		dev_err(buf->dev,
			"%s: Failed to get sgt of user buffer @ %#lx/%#lx\n",
			__func__, vaddr, buf->size);
		ret = ERR_CAST(buf->cookie.sgt);
		goto err_map;
	}

	buf->cookie.ioaddr = ion_iovmm_map(buf->attachment, 0,
			buf->size, buf->direction,
			get_iommu_prot(buf->dev, buf->direction));
	if (IS_ERR_VALUE(buf->cookie.ioaddr)) {
		ret = ERR_PTR(buf->cookie.ioaddr);
		goto err_iovmm;
	}

	buf->cookie.ioaddr += buf->cookie.offset;

	return NULL;
err_iovmm:
	dma_buf_unmap_attachment(buf->attachment,
				 buf->cookie.sgt, buf->direction);
err_map:
	dma_buf_detach(buf->dma_buf, buf->attachment);
	return ret;
}

static void *vb2_ion_get_userptr(struct device *dev, unsigned long vaddr,
		unsigned long size, enum dma_data_direction dma_dir)
{
	struct vb2_ion_buf *buf = NULL;
	struct vm_area_struct *vma;
	void *p_ret = ERR_PTR(-ENOMEM);;

	vma = vb2_ion_get_vma(dev, vaddr, size);
	if (!vma) {
		dev_err(dev,
			"%s: Failed to holding user buffer @ %#lx/%#lx\n",
			__func__, vaddr, size);
		return ERR_PTR(-EINVAL);
	}

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		dev_err(dev, "%s: Not enough memory\n", __func__);
		p_ret = ERR_PTR(-ENOMEM);
		goto err_alloc;
	}

	if (vma->vm_file) {
		get_file(vma->vm_file);
		buf->dma_buf = get_dma_buf_file(vma->vm_file);
	}

	buf->dev = dev;
	buf->direction = dma_dir;
	buf->size = size;
	buf->vma = vma;

	if (buf->dma_buf) {
		buf->ion = true;
		buf->cookie.offset = vaddr - vma->vm_start;

		p_ret = vb2_ion_get_userptr_dmabuf(buf, vaddr);
		if (IS_ERR(p_ret))
			goto err_map;
	} else {
		buf->cookie.ioaddr = exynos_iovmm_map_userptr(buf->dev, vaddr,
				 size, get_iommu_prot(buf->dev, buf->direction));
		if (IS_ERR_VALUE(buf->cookie.ioaddr)) {
			p_ret = ERR_PTR(buf->cookie.ioaddr);
			goto err_map;
		}
	}

	if ((pgprot_val(pgprot_noncached(buf->vma->vm_page_prot))
				== pgprot_val(buf->vma->vm_page_prot))
		|| (pgprot_val(pgprot_writecombine(buf->vma->vm_page_prot))
				== pgprot_val(buf->vma->vm_page_prot)))
		buf->cached = false;
	else
		buf->cached = true;

	return buf;
err_map:
	if (buf->dma_buf)
		dma_buf_put(buf->dma_buf);
	if (vma->vm_file)
		fput(vma->vm_file);
	kfree(buf);
err_alloc:
	vb2_ion_put_vma(vma);

	return p_ret;
}

static void vb2_ion_put_userptr(void *mem_priv)
{
	struct vb2_ion_buf *buf = mem_priv;

	if (buf->kva) {
		dma_buf_kunmap(buf->dma_buf, buf->cookie.offset / PAGE_SIZE,
				buf->kva - (buf->cookie.offset & ~PAGE_SIZE));
		dma_buf_end_cpu_access(buf->dma_buf, DMA_FROM_DEVICE);
	}

	if (buf->dma_buf)
		vb2_ion_put_userptr_dmabuf(buf);
	else
		exynos_iovmm_unmap_userptr(buf->dev, buf->cookie.ioaddr);

	if (buf->dma_buf)
		dma_buf_put(buf->dma_buf);
	if (buf->vma->vm_file)
		fput(buf->vma->vm_file);

	vb2_ion_put_vma(buf->vma);

	kfree(buf);
}

const struct vb2_mem_ops vb2_ion_memops = {
	.alloc		= vb2_ion_alloc,
	.put		= vb2_ion_put,
	.cookie		= vb2_ion_cookie,
	.vaddr		= vb2_ion_vaddr,
	.mmap		= vb2_ion_mmap,
	.map_dmabuf	= vb2_ion_map_dmabuf,
	.unmap_dmabuf	= vb2_ion_unmap_dmabuf,
	.attach_dmabuf	= vb2_ion_attach_dmabuf,
	.detach_dmabuf	= vb2_ion_detach_dmabuf,
	.get_userptr	= vb2_ion_get_userptr,
	.put_userptr	= vb2_ion_put_userptr,
	.num_users	= vb2_ion_num_users,
};
EXPORT_SYMBOL_GPL(vb2_ion_memops);

void vb2_ion_sync_for_device(void *cookie, off_t offset, size_t size,
						enum dma_data_direction dir)
{
	struct vb2_ion_buf *buf = container_of(cookie,
					struct vb2_ion_buf, cookie);

	if (vb2_ion_dma_is_coherent(buf->dev, dir))
		return;

	dev_dbg(buf->dev, "syncing for device, dmabuf: %p, kva: %p, "
		"size: %zd, dir: %d\n", buf->dma_buf, buf->kva, size, dir);

	if (buf->kva) {
		BUG_ON((offset < 0) || (offset > buf->size));
		BUG_ON((offset + size) < size);
		BUG_ON((size > buf->size) || ((offset + size) > buf->size));

		exynos_ion_sync_vaddr_for_device(buf->dev,
				buf->dma_buf, buf->kva, size, offset, dir);
	} else if (buf->dma_buf) {
		exynos_ion_sync_dmabuf_for_device(buf->dev,
						buf->dma_buf, size, dir);
	} else if (buf->vma && buf->cached) {
		exynos_iommu_sync_for_device(buf->dev,
					buf->cookie.ioaddr, size, dir);
	}
}
EXPORT_SYMBOL_GPL(vb2_ion_sync_for_device);

void vb2_ion_sync_for_cpu(void *cookie, off_t offset, size_t size,
						enum dma_data_direction dir)
{
	struct vb2_ion_buf *buf = container_of(cookie,
					struct vb2_ion_buf, cookie);

	if (vb2_ion_dma_is_coherent(buf->dev, dir))
		return;

	dev_dbg(buf->dev, "syncing for cpu, dmabuf: %p, kva: %p, "
		"size: %zd, dir: %d\n", buf->dma_buf, buf->kva, size, dir);

	if (buf->kva) {
		BUG_ON((offset < 0) || (offset > buf->size));
		BUG_ON((offset + size) < size);
		BUG_ON((size > buf->size) || ((offset + size) > buf->size));

		exynos_ion_sync_vaddr_for_cpu(buf->dev,
				buf->dma_buf, buf->kva, size, offset, dir);
	} else if (buf->dma_buf) {
		exynos_ion_sync_dmabuf_for_cpu(buf->dev,
						buf->dma_buf, size, dir);
	} else if (buf->vma && buf->cached) {
		exynos_iommu_sync_for_cpu(buf->dev,
					buf->cookie.ioaddr, size, dir);
	}
}
EXPORT_SYMBOL_GPL(vb2_ion_sync_for_cpu);

int vb2_ion_buf_prepare(struct vb2_buffer *vb)
{
	int i;
	enum dma_data_direction dir;

	dir = V4L2_TYPE_IS_OUTPUT(vb->type) ?
					DMA_TO_DEVICE : DMA_FROM_DEVICE;

	for (i = 0; i < vb->num_planes; i++) {
		struct vb2_ion_buf *buf = vb->planes[i].mem_priv;

		vb2_ion_sync_for_device((void *) &buf->cookie, 0,
							buf->size, dir);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(vb2_ion_buf_prepare);

void vb2_ion_buf_finish(struct vb2_buffer *vb)
{
	int i;
	enum dma_data_direction dir;

	dir = V4L2_TYPE_IS_OUTPUT(vb->type) ?
					DMA_TO_DEVICE : DMA_FROM_DEVICE;

	for (i = 0; i < vb->num_planes; i++) {
		struct vb2_ion_buf *buf = vb->planes[i].mem_priv;

		vb2_ion_sync_for_cpu((void *) &buf->cookie, 0,
						buf->size, dir);
	}
}
EXPORT_SYMBOL_GPL(vb2_ion_buf_finish);

int vb2_ion_buf_prepare_exact(struct vb2_buffer *vb)
{
	int i;
	enum dma_data_direction dir;

	dir = V4L2_TYPE_IS_OUTPUT(vb->type) ?
					DMA_TO_DEVICE : DMA_FROM_DEVICE;

	for (i = 0; i < vb->num_planes; i++) {
		struct vb2_ion_buf *buf = vb->planes[i].mem_priv;

		vb2_ion_sync_for_device((void *) &buf->cookie, 0,
					vb2_get_plane_payload(vb, i), dir);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(vb2_ion_buf_prepare_exact);

int vb2_ion_buf_finish_exact(struct vb2_buffer *vb)
{
	int i;
	enum dma_data_direction dir;

	dir = V4L2_TYPE_IS_OUTPUT(vb->type) ?
					DMA_TO_DEVICE : DMA_FROM_DEVICE;

	for (i = 0; i < vb->num_planes; i++) {
		struct vb2_ion_buf *buf = vb->planes[i].mem_priv;

		vb2_ion_sync_for_cpu((void *) &buf->cookie, 0,
					vb2_get_plane_payload(vb, i), dir);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(vb2_ion_buf_finish_exact);

struct dma_buf *vb2_ion_get_dmabuf(void *cookie)
{
	struct vb2_ion_buf *buf = container_of(cookie,
					struct vb2_ion_buf, cookie);

	if (buf->dma_buf)
		get_dma_buf(buf->dma_buf);

	return buf->dma_buf;
}

MODULE_AUTHOR("Cho KyongHo <pullip.cho@samsung.com>");
MODULE_AUTHOR("Jinsung Yang <jsgood.yang@samsung.com>");
MODULE_DESCRIPTION("Android ION allocator handling routines for videobuf2");
MODULE_LICENSE("GPL");

