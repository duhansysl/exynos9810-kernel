#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/memblock.h>
#include <linux/dma-contiguous.h>
#include <linux/cma.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/kref.h>
#include <linux/genalloc.h>
#include <linux/exynos_ion.h>
#include <linux/oom.h>
#include <linux/uaccess.h>
#include "dmabuf_container_priv.h"
#include "../../uapi/exynos_ion_uapi.h"

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
#include <linux/smc.h>
#endif

#include <asm/dma-contiguous.h>

#include <linux/exynos_ion.h>

#include "../ion.h"
#include "../ion_priv.h"
#include "ion_hpa_heap.h"

static struct ion_device *ion_exynos;

#define ION_SECURE_DMA_BASE	0x80000000
#define ION_SECURE_DMA_END	0xE0000000

/* starting from index=1 regarding default index=0 for system heap */
static int nr_heaps = 1;

struct exynos_ion_platform_heap {
	struct ion_platform_heap heap_data;
	struct reserved_mem *rmem;
	unsigned int id;
	unsigned int compat_ids;
	bool secure;
	bool reusable;
	bool recyclable;
	bool protected;
	atomic_t secure_ref;
	struct ion_heap *heap;
};

static struct ion_platform_heap ion_noncontig_heap __initdata = {
	.name = "ion_noncontig_heap",
	.type = ION_HEAP_TYPE_SYSTEM,
	.id = EXYNOS_ION_HEAP_SYSTEM_ID,
};

struct exynos_ion_platform_heap ion_hpa_heaps[] __initdata = {
#ifdef CONFIG_HPA
	{
		.heap_data = {
			.name = "crypto_heap",
			.type = ION_HEAP_TYPE_HPA,
			.id = ION_EXYNOS_HEAP_ID_CRYPTO,
		},
		.id = ION_EXYNOS_HEAP_ID_CRYPTO,
		.secure = true,
	}, {
		.heap_data = {
			.name = "vfw_heap",
			.type = ION_HEAP_TYPE_HPA,
			.id = ION_EXYNOS_HEAP_ID_VIDEO_FW,
		},
		.id = ION_EXYNOS_HEAP_ID_VIDEO_FW,
		.compat_ids = (1 << 24),
		.secure = true,
	}, {
		.heap_data = {
			.name = "vnfw_heap",
			.type = ION_HEAP_TYPE_HPA,
			.id = ION_EXYNOS_HEAP_ID_VIDEO_NFW,
		},
		.id = ION_EXYNOS_HEAP_ID_VIDEO_NFW,
		.compat_ids = (1 << 20),
	}, {
		.heap_data = {
			.name = "vstream_heap",
			.type = ION_HEAP_TYPE_HPA,
			.id = ION_EXYNOS_HEAP_ID_VIDEO_STREAM,
		},
		.id = ION_EXYNOS_HEAP_ID_VIDEO_STREAM,
		.compat_ids = (1 << 25) | (1 << 31),
		.secure = true,
	}, {
		.heap_data = {
			.name = "vframe_heap",
			.type = ION_HEAP_TYPE_HPA,
			.id = ION_EXYNOS_HEAP_ID_VIDEO_FRAME,
		},
		.id = ION_EXYNOS_HEAP_ID_VIDEO_FRAME,
		.compat_ids = (1 << 26) | (1 << 29),
		.secure = true,
	}, {
		.heap_data = {
			.name = "vscaler_heap",
			.type = ION_HEAP_TYPE_HPA,
			.id = ION_EXYNOS_HEAP_ID_VIDEO_SCALER,
		},
		.id = ION_EXYNOS_HEAP_ID_VIDEO_SCALER,
		.compat_ids = (1 << 28),
		.secure = true,
	}, {
		.heap_data = {
			.name = "gpu_heap",
			.type = ION_HEAP_TYPE_HPA,
			.id = 9, /* 9 but it is not defined in exynos_ion.h */
		},
		.id = 9, /* 9 but it is not defined in exynos_ion.h */
		.secure = true,
	},
#endif /* CONFIG_HPA */
};

static struct exynos_ion_platform_heap plat_heaps[ION_NUM_HEAPS];

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION

static struct gen_pool *secure_iova_pool;
static DEFINE_SPINLOCK(siova_pool_lock);

/*
 * Parse the heap ids of camera and camera_contig heaps during boot up
 * - SEC MOBILE, kernel core memory
 */
unsigned int camera_heap_id;
unsigned int secure_camera_heap_id;
unsigned int camera_contig_heap_id;

static void exynos_ion_init_camera_heaps(void)
{
       int i;
       for (i = 0; i < nr_heaps; i++) {
               if (plat_heaps[i].heap) {
                       struct ion_heap *heap = plat_heaps[i].heap;
                       if (!strncmp(heap->name, "camera_contig", 13)) {
                               WARN_ON(camera_contig_heap_id);
                               camera_contig_heap_id = heap->id;
                       } else if (!strncmp(heap->name, "secure_camera", 13)) {
                               WARN_ON(camera_heap_id);
                               secure_camera_heap_id = heap->id;
                       } else if (!strncmp(heap->name, "camera", 6)) {
                               WARN_ON(camera_heap_id);
                               camera_heap_id = heap->id;
                       }
               }
       }
       pr_info("%s: [heap id] camera %d, secure_camera %d, camera_contig %d\n",
                       __func__, camera_heap_id,
                       secure_camera_heap_id, camera_contig_heap_id);
}

#define MAX_IOVA_ALIGNMENT	12
static unsigned long find_first_fit_with_align(unsigned long *map,
				unsigned long size, unsigned long start,
				unsigned int nr, void *data, struct gen_pool *pool)
{
	unsigned long align = ((*(unsigned long *)data) >> PAGE_SHIFT);

	if (align > (1 << MAX_IOVA_ALIGNMENT))
		align = (1 << MAX_IOVA_ALIGNMENT);

	return bitmap_find_next_zero_area(map, size, start, nr, (align - 1));
}

int ion_secure_iova_alloc(unsigned long *addr, unsigned long size,
				unsigned int align)
{
	spin_lock(&siova_pool_lock);
	if (align > PAGE_SIZE) {
		gen_pool_set_algo(secure_iova_pool,
				find_first_fit_with_align, &align);
		*addr = gen_pool_alloc(secure_iova_pool, size);
		gen_pool_set_algo(secure_iova_pool, NULL, NULL);
	} else {
		*addr = gen_pool_alloc(secure_iova_pool, size);
	}
	spin_unlock(&siova_pool_lock);

	if (*addr == 0) {
		pr_err("%s: failed to allocate secure iova.\
				Used %zu B / Available %zu B\n",
				__func__, gen_pool_size(secure_iova_pool),
				gen_pool_avail(secure_iova_pool));
		return -ENOMEM;
	}
	return 0;
}

void ion_secure_iova_free(unsigned long addr, unsigned long size)
{
	spin_lock(&siova_pool_lock);
	gen_pool_free(secure_iova_pool, addr, size);
	spin_unlock(&siova_pool_lock);
}

int __init ion_secure_iova_pool_create(void)
{
	secure_iova_pool = gen_pool_create(PAGE_SHIFT, -1);
	if (!secure_iova_pool) {
		pr_err("%s: failed to create Secure IOVA pool\n", __func__);
		return -ENOMEM;
	}

	if (gen_pool_add(secure_iova_pool, ION_SECURE_DMA_BASE,
				ION_SECURE_DMA_END - ION_SECURE_DMA_BASE, -1)) {
		pr_err("%s: failed to set address range of Secure IOVA pool\n",
			__func__);
		return -ENOMEM;
	}

	return 0;
}

static int __find_platform_heap_id(unsigned int heap_id)
{
	int i;

	for (i = 0; i < nr_heaps; i++) {
		if (heap_id == plat_heaps[i].id)
			break;
	}

	if (i == nr_heaps)
		return -EINVAL;

	return i;
}

static int __ion_secure_protect_buffer(struct exynos_ion_platform_heap *pdata,
					struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_buffer_info *info = buffer->priv_virt;
	struct ion_buffer_prot_info *prot = &info->prot_desc;
	unsigned long dma_addr = 0;
	int ret = 0;

	BUG_ON(heap->type != ION_HEAP_TYPE_HPA && prot->chunk_count != 1);

	ret = ion_secure_iova_alloc(&dma_addr,
			(prot->chunk_count * prot->chunk_size), PAGE_SIZE);
	if (ret)
		return ret;

	prot->dma_addr = (unsigned int)dma_addr;

	__flush_dcache_area(prot, sizeof(struct ion_buffer_prot_info));
	if (prot->chunk_count > 1)
		__flush_dcache_area(phys_to_virt(prot->bus_address),
			sizeof(unsigned long) * prot->chunk_count);

	ret = exynos_smc(SMC_DRM_PPMP_PROT, virt_to_phys(prot), 0, 0);
	if (ret != DRMDRV_OK) {
		pr_crit("%s: smc call failed, err=%d\n", __func__, ret);
		pr_info("prot desc dump: nr_chk %u, sz_chk %u, flags %#x\n",
			prot->chunk_count, prot->chunk_size, prot->flags);
		pr_info("prot desc dump: dma_addr %#x, paddr %#lx\n",
			prot->dma_addr,
			prot->chunk_count == 1 ? prot->bus_address : -1L);
		ret = -EINVAL;
		ion_secure_iova_free(info->prot_desc.dma_addr,
				     prot->chunk_count * prot->chunk_size);
	}

	return ret;
}

int ion_secure_protect(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct exynos_ion_platform_heap *pdata;
	struct ion_buffer_info *info = (struct ion_buffer_info *)buffer->priv_virt;
	int id;

	id = __find_platform_heap_id(heap->id);
	if (id < 0) {
		pr_err("%s: invalid heap id(%d) for %s\n", __func__,
						heap->id, heap->name);
		return -EINVAL;
	}

	pdata = &plat_heaps[id];
	if (!pdata->secure) {
		pr_err("%s: heap %s is not secure heap\n", __func__,
						heap->name);
		return -EPERM;
	}

       /* use same protection ID for both of camera & camera_contig heap */
       if (info->prot_desc.flags == camera_contig_heap_id)
               info->prot_desc.flags = camera_heap_id;

	return	__ion_secure_protect_buffer(pdata, buffer);
}

static int __ion_secure_unprotect_buffer(struct exynos_ion_platform_heap *pdata,
					struct ion_buffer *buffer)
{
	struct ion_buffer_info *info = buffer->priv_virt;
	struct ion_buffer_prot_info *prot = &info->prot_desc;
	unsigned int size = 0;
	int ret;

	ret = exynos_smc(SMC_DRM_PPMP_UNPROT,
			 virt_to_phys(prot), 0, 0);
	if (ret != DRMDRV_OK) {
		pr_crit("%s: smc call failed, err=%d\n", __func__, ret);
		pr_info("prot desc dump: nr_chk %u, sz_chk %u, flags %#x\n",
			prot->chunk_count, prot->chunk_size, prot->flags);
		pr_info("prot desc dump: dma_addr %#x, paddr %#lx\n",
			prot->dma_addr,
			prot->chunk_count == 1 ? prot->bus_address : -1L);
		BUG();
		/*
		 * retain the secure iova to mark the area unusable because the
		 * next protection request on that area might fail.
		 */
		return -EINVAL;
	}

	size = info->prot_desc.chunk_count * info->prot_desc.chunk_size;
	ion_secure_iova_free(info->prot_desc.dma_addr, size);

	return 0;
}

int ion_secure_unprotect(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct exynos_ion_platform_heap *pdata;
	int id;

	id = __find_platform_heap_id(heap->id);
	if (id < 0) {
		pr_err("%s: invalid heap id(%d) for %s\n", __func__,
						heap->id, heap->name);
		return -EINVAL;
	}

	pdata = &plat_heaps[id];
	if (!pdata->secure) {
		pr_err("%s: heap %s is not secure heap\n", __func__, heap->name);
		return -EPERM;
	}

	return __ion_secure_unprotect_buffer(pdata, buffer);
}
#else
int ion_secure_protect(struct ion_buffer *buffer)
{
	return 0;
}

int ion_secure_unprotect(struct ion_buffer *buffer)
{
	return 0;
}

#define ion_secure_iova_alloc(addr, size, align) do { } while (0)
#define ion_secure_iova_free(addr, size) do { } while (0)
#define ion_secure_iova_pool_create() (0)
#endif

struct ion_client *exynos_ion_client_create(const char *name)
{
	return ion_client_create(ion_exynos, name);
}

bool ion_is_heap_available(struct ion_heap *heap,
				unsigned long flags, void *data)
{
	return true;
}

unsigned int ion_parse_heap_id(unsigned int heap_id_mask, unsigned int flags)
{
	unsigned int heap_id = 1;
	int i;

	pr_debug("%s: heap_id_mask=%#x, flags=%#x\n",
			__func__, heap_id_mask, flags);

	/*
	 * Buffer alloc request on "camera heap" id with ION_FLAG_PROTECTED
	 * should go to camera_contig heap.
	 * This is the exynos9810-specific requirement.
	 * - SEC MOBILE, kernel core memory
	 */
	if (camera_heap_id && (heap_id_mask == (1 << camera_heap_id))
			       && (flags & ION_FLAG_PROTECTED))
		return camera_contig_heap_id ? (1 << camera_contig_heap_id) :
						heap_id_mask;

	/*
	 * User space cannot request camera_contig heap directly
	 * - SEC MOBILE, kernel core memory
	 */
	if (camera_contig_heap_id &&
	    (heap_id_mask == (1 << camera_contig_heap_id)))
		return 0;

	if (heap_id_mask != EXYNOS_ION_HEAP_EXYNOS_CONTIG_MASK)
		return heap_id_mask;

	if (flags & EXYNOS_ION_CONTIG_ID_MASK)
		heap_id =
			(unsigned long)__fls(flags & EXYNOS_ION_CONTIG_ID_MASK);

	for (i = 1; i < nr_heaps; i++) {
		if ((plat_heaps[i].id == heap_id) ||
			(plat_heaps[i].compat_ids & (1 << heap_id)))
			break;
	}

	if (i == nr_heaps) {
		pr_err("%s: bad heap flags %#x\n", __func__, flags);
		return 0;
	}

	pr_debug("%s: found new heap id %d for %s\n", __func__,
			plat_heaps[i].id, plat_heaps[i].heap_data.name);

	return (1 << plat_heaps[i].id);
}

static int exynos_ion_rmem_device_init(struct reserved_mem *rmem,
						struct device *dev)
{
	WARN(1, "%s() should never be called!", __func__);
	return 0;
}

static void exynos_ion_rmem_device_release(struct reserved_mem *rmem,
						struct device *dev)
{
}

/*
 * The below rmem device ops are never called because the the device is not
 * associated with the rmem in the flattened device tree
 */
static const struct reserved_mem_ops exynos_ion_rmem_ops = {
	.device_init	= exynos_ion_rmem_device_init,
	.device_release	= exynos_ion_rmem_device_release,
};

static int __init exynos_ion_reserved_mem_setup(struct reserved_mem *rmem)
{
	struct exynos_ion_platform_heap *pdata;
	struct ion_platform_heap *heap_data;
	int len = 0;
	const __be32 *prop;

	BUG_ON(nr_heaps >= ION_NUM_HEAPS);

	pdata = &plat_heaps[nr_heaps];
	pdata->secure = !!of_get_flat_dt_prop(rmem->fdt_node, "ion,secure", NULL);
	pdata->reusable = !!of_get_flat_dt_prop(rmem->fdt_node, "ion,reusable", NULL);
#ifdef CONFIG_ION_RBIN_HEAP
	pdata->recyclable = !!of_get_flat_dt_prop(rmem->fdt_node, "ion,recyclable", NULL);
#else
	pdata->recyclable = false;
#endif

	prop = of_get_flat_dt_prop(rmem->fdt_node, "id", &len);
	if (!prop) {
		pr_err("%s: no <id> found\n", __func__);
		return -EINVAL;
	}

	len /= sizeof(int);
	if (len != 1) {
		pr_err("%s: wrong <id> field format\n", __func__);
		return -EINVAL;
	}

	/*
	 * id=0: system heap
	 * id=1 ~: contig heaps
	 */
	pdata->id = be32_to_cpu(prop[0]);
	if (pdata->id >= ION_NUM_HEAPS) {
		pr_err("%s: bad <id> number\n", __func__);
		return -EINVAL;
	}

	prop = of_get_flat_dt_prop(rmem->fdt_node, "compat-id", &len);
	if (prop) {
		len /= sizeof(int);
		while (len > 0)
			pdata->compat_ids |= (1 << be32_to_cpu(prop[--len]));
	}

	rmem->ops = &exynos_ion_rmem_ops;
	pdata->rmem = rmem;
	rmem->reusable = pdata->reusable | pdata->recyclable;

	heap_data = &pdata->heap_data;
	heap_data->id = pdata->id;
	heap_data->name = rmem->name;
	heap_data->base = rmem->base;
	heap_data->size = rmem->size;
	heap_data->untouchable =
		!!of_get_flat_dt_prop(rmem->fdt_node, "ion,untouchable", NULL);

	if (heap_data->untouchable) {
		/* untouchable heap is considered as secure heap */
		pdata->secure = true;

		if (pdata->reusable) {
			pr_err("%s: invalid cma region with untouchable\n", __func__);
			return -EINVAL;
		}
	}

	prop = of_get_flat_dt_prop(rmem->fdt_node, "alignment", &len);
	if (!prop)
		heap_data->align = PAGE_SIZE;
	else
		heap_data->align = be32_to_cpu(prop[0]);

	if (pdata->reusable || pdata->recyclable) {
		int ret;
		struct cma *cma;

		if (pdata->reusable)
			heap_data->type = ION_HEAP_TYPE_DMA;
		else
			heap_data->type = ION_HEAP_TYPE_RBIN;

		ret = cma_init_reserved_mem_with_name(
				heap_data->base, heap_data->size, 0, &cma,
				heap_data->name);
		if (ret) {
			pr_err("%s: failed to declare cma region %s (%d)\n",
			       __func__, heap_data->name, ret);
			return ret;
		}

		if (pdata->recyclable) {
			cma_set_rbin(cma);
			totalrbin_pages += (heap_data->size / PAGE_SIZE);
			/*
			 * # of cma pages was increased by this RBIN memory in
			 * cma_init_reserved_mem_with_name(). Need to deduct.
			 */
			totalcma_pages -= (heap_data->size / PAGE_SIZE);
		}

		heap_data->priv = cma;

		if (pdata->reusable)
			pr_info("CMA memory[%d]: %s:%#lx\n", heap_data->id,
				heap_data->name, (unsigned long)rmem->size);
		else
			pr_info("rbin CMA memory[%d]: %s:%#lx\n", heap_data->id,
				heap_data->name, (unsigned long)rmem->size);
	} else {
		heap_data->type = ION_HEAP_TYPE_CARVEOUT;
		heap_data->priv = rmem;
		pr_info("Reserved memory[%d]: %s:%#lx\n", heap_data->id,
				heap_data->name, (unsigned long)rmem->size);
	}

	atomic_set(&pdata->secure_ref, 0);
	nr_heaps++;

	return 0;
}

#define DECLARE_EXYNOS_ION_RESERVED_REGION(compat, name) \
RESERVEDMEM_OF_DECLARE(name, compat#name, exynos_ion_reserved_mem_setup)

DECLARE_EXYNOS_ION_RESERVED_REGION("exynos8890-ion,", crypto);
DECLARE_EXYNOS_ION_RESERVED_REGION("exynos8890-ion,", vfw);
DECLARE_EXYNOS_ION_RESERVED_REGION("exynos8890-ion,", vnfw);
DECLARE_EXYNOS_ION_RESERVED_REGION("exynos8890-ion,", vstream);
DECLARE_EXYNOS_ION_RESERVED_REGION("exynos8890-ion,", vframe);
DECLARE_EXYNOS_ION_RESERVED_REGION("exynos8890-ion,", vscaler);
DECLARE_EXYNOS_ION_RESERVED_REGION("exynos8890-ion,", camera);
DECLARE_EXYNOS_ION_RESERVED_REGION("exynos8890-ion,", secure_camera);
DECLARE_EXYNOS_ION_RESERVED_REGION("exynos8890-ion,", camera_contig);

static int __init exynos_ion_populate_heaps(struct ion_device *ion_dev)
{
	int i, ret, num;

	plat_heaps[0].reusable = false;
	memcpy(&plat_heaps[0].heap_data, &ion_noncontig_heap,
				sizeof(struct ion_platform_heap));

	ret = ion_secure_iova_pool_create();
	if (ret)
		return ret;

	num = nr_heaps;
	for (i = 0; i < (int)ARRAY_SIZE(ion_hpa_heaps); i++) {
		int j;
		for (j = 1; j < num; j++)
			if (ion_hpa_heaps[i].heap_data.id == plat_heaps[j].id)
				break;

		if (j == num)
			memcpy(&plat_heaps[nr_heaps++], &ion_hpa_heaps[i],
				sizeof(plat_heaps[0]));
	}

	for (i = 0; i < nr_heaps; i++) {
		plat_heaps[i].heap = ion_heap_create(&plat_heaps[i].heap_data);
		if (IS_ERR(plat_heaps[i].heap)) {
			pr_err("%s: failed to create heap %s[%d]\n", __func__,
					plat_heaps[i].heap_data.name,
					plat_heaps[i].id);
			ret = PTR_ERR(plat_heaps[i].heap);
			goto err;
		}

		ion_device_add_heap(ion_exynos, plat_heaps[i].heap);
	}

	exynos_ion_init_camera_heaps();
	return 0;

err:
	while (i-- > 0)
		ion_heap_destroy(plat_heaps[i].heap);

	return ret;
}

static int ion_oomdebug_notify(struct notifier_block *self,
			       unsigned long dummy, void *parm)
{
	struct rb_node *n;

	pr_info("-------------------- ion buffers -------------------------\n");
	pr_info("%10.s %2.s %4.s %6.s %16.s %10.s %8.s\n",
		"heap", "id", "type", "pid", "task", "size", "flag");
	pr_info("----------------------------------------------------------\n");

	mutex_lock(&ion_exynos->buffer_lock);
	for (n = rb_first(&ion_exynos->buffers); n; n = rb_next(n)) {
		struct ion_buffer *buf = rb_entry(n, struct ion_buffer, node);

		pr_info("%10.s %2u %4u %6u %16.s %10zu %#8lx\n",
			buf->heap->name, buf->heap->id, buf->heap->type,
			buf->pid, buf->task_comm, buf->size, buf->flags);
	}
	mutex_unlock(&ion_exynos->buffer_lock);

	return NOTIFY_OK;
}

static struct notifier_block ion_oomdebug_nb = {
	.notifier_call = ion_oomdebug_notify,
};

static long exynos_custom_ioctl(struct ion_client *client, unsigned int cmd,
				unsigned long arg)
{
	int ret = -ENOTTY;

	if (cmd == ION_IOC_CUSTOM_CONTAINER_CREATE)
		ret = dmabuf_container_create((void __user *)arg);
	else
		pr_err("%s: Unknown IOCTL cmd %#x\n", __func__, cmd);

	return ret;
}

static int __init exynos_ion_init(void)
{
	ion_exynos = ion_device_create(exynos_custom_ioctl);
	if (IS_ERR(ion_exynos)) {
		pr_err("%s: failed to create ion device\n", __func__);
		return PTR_ERR(ion_exynos);
	}

	if (register_oomdebug_notifier(&ion_oomdebug_nb) < 0)
		pr_err("%s: failed to register oom debug notifier\n", __func__);

	return exynos_ion_populate_heaps(ion_exynos);
}

subsys_initcall(exynos_ion_init);
