/* drivers/gpu/arm/.../platform/gpu_integration_callbacks.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T Series DDK porting layer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_integration_callbacks.c
 * DDK porting layer.
 */

#include <mali_kbase.h>
#include <mali_midg_regmap.h>
#include <mali_kbase_sync.h>

#include <linux/pm_qos.h>
#include <linux/sched.h>

#include <mali_kbase_gpu_memory_debugfs.h>
#include <backend/gpu/mali_kbase_device_internal.h>

/* MALI_SEC_INTEGRATION */
#define KBASE_REG_CUSTOM_TMEM       (1ul << 19)
#define KBASE_REG_CUSTOM_PMEM       (1ul << 20)

#define ENTRY_TYPE_MASK     3ULL
#define ENTRY_IS_ATE        1ULL
#define ENTRY_IS_INVAL      2ULL
#define ENTRY_IS_PTE        3ULL

#define ENTRY_ATTR_BITS (7ULL << 2)	/* bits 4:2 */
#define ENTRY_RD_BIT (1ULL << 6)
#define ENTRY_WR_BIT (1ULL << 7)
#define ENTRY_SHARE_BITS (3ULL << 8)	/* bits 9:8 */
#define ENTRY_ACCESS_BIT (1ULL << 10)
#define ENTRY_NX_BIT (1ULL << 54)

#define ENTRY_FLAGS_MASK (ENTRY_ATTR_BITS | ENTRY_RD_BIT | ENTRY_WR_BIT | \
		ENTRY_SHARE_BITS | ENTRY_ACCESS_BIT | ENTRY_NX_BIT)

/*
* peak_flops: 100/85
* sobel: 100/50
*/
#define COMPUTE_JOB_WEIGHT (10000/50)

#ifdef CONFIG_SENSORS_SEC_THERMISTOR
extern int sec_therm_get_ap_temperature(void);
#endif

#ifdef CONFIG_SCHED_HMP
extern int set_hmp_boost(int enable);
#endif

extern int gpu_register_dump(void);

void gpu_create_context(void *ctx)
{
	struct kbase_context *kctx;
	char current_name[sizeof(current->comm)];

	kctx = (struct kbase_context *)ctx;
	KBASE_DEBUG_ASSERT(kctx != NULL);

	kctx->ctx_status = CTX_UNINITIALIZED;
	kctx->ctx_need_qos = false;

	get_task_comm(current_name, current);
	strncpy((char *)(&kctx->name), current_name, CTX_NAME_SIZE);

	kctx->ctx_status = CTX_INITIALIZED;

	kctx->destroying_context = false;
}

void gpu_destroy_context(void *ctx)
{
	struct kbase_context *kctx;
	struct kbase_device *kbdev;

	kctx = (struct kbase_context *)ctx;
	KBASE_DEBUG_ASSERT(kctx != NULL);

	kbdev = kctx->kbdev;
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	kctx->destroying_context = true;

	kctx->ctx_status = CTX_DESTROYED;

	if (kctx->ctx_need_qos)
	{
#ifdef CONFIG_MALI_DVFS
		gpu_dvfs_boost_lock(GPU_DVFS_BOOST_UNSET);
#endif
#ifdef CONFIG_SCHED_HMP
		set_hmp_boost(0);
		set_hmp_aggressive_up_migration(false);
		set_hmp_aggressive_yield(false);
#endif
	}
#ifdef CONFIG_MALI_DVFS_USER
	gpu_dvfs_check_destroy_context(kctx);
#endif
}

int gpu_vendor_dispatch(struct kbase_context *kctx, void * const args, u32 args_size)
{
	struct kbase_device *kbdev;
	union uk_header *ukh = args;
	u32 id;

	KBASE_DEBUG_ASSERT(ukh != NULL);

	kbdev = kctx->kbdev;
	id = ukh->id;
	ukh->ret = 0;	/* Be optimistic */

	switch(id)
	{

	case KBASE_FUNC_STEP_UP_MAX_GPU_LIMIT :
		{
#ifdef CONFIG_MALI_DVFS
			struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;

			if (!platform->using_max_limit_clock) {
				platform->using_max_limit_clock = true;
			}
#endif
			break;
		}
	case KBASE_FUNC_RESTORE_MAX_GPU_LIMIT :
		{
#ifdef CONFIG_MALI_DVFS
			struct exynos_context *platform = (struct exynos_context *)kbdev->platform_context;

			if (platform->using_max_limit_clock) {
				platform->using_max_limit_clock = false;
			}
#endif
			break;
		}
	case KBASE_FUNC_SET_MIN_LOCK :
		{
#ifdef CONFIG_MALI_PM_QOS
			struct exynos_context *platform;
#endif
			if (!kctx->ctx_need_qos) {
				kctx->ctx_need_qos = true;
#ifdef CONFIG_SCHED_HMP
				/* set hmp boost */
				set_hmp_boost(1);
				set_hmp_aggressive_up_migration(true);
				set_hmp_aggressive_yield(true);
#endif /* CONFIG_SCHED_HMP */
			}
#ifdef CONFIG_MALI_PM_QOS
			platform = (struct exynos_context *) kbdev->platform_context;
			gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_EGL_SET);
#endif /* CONFIG_MALI_PM_QOS */
			break;
		}

	case KBASE_FUNC_UNSET_MIN_LOCK :
		{
#ifdef CONFIG_MALI_PM_QOS
			struct exynos_context *platform;
#endif
			if (kctx->ctx_need_qos) {
				kctx->ctx_need_qos = false;
#ifdef CONFIG_SCHED_HMP
				/* unset hmp boost */
				set_hmp_boost(0);
				set_hmp_aggressive_up_migration(false);
				set_hmp_aggressive_yield(false);
#endif /* CONFIG_SCHED_HMP */
#ifdef CONFIG_MALI_PM_QOS
				platform = (struct exynos_context *) kbdev->platform_context;
				gpu_pm_qos_command(platform, GPU_CONTROL_PM_QOS_EGL_RESET);
#endif /* CONFIG_MALI_PM_QOS */
			}
			break;
		}
	default:
		break;
	}

	return 0;

}

#include <mali_kbase_gpu_memory_debugfs.h>
int gpu_memory_seq_show(struct seq_file *sfile, void *data)
{
	ssize_t ret = 0;
	struct list_head *entry;
	const struct list_head *kbdev_list;
	size_t free_size = 0;
	size_t each_free_size = 0;

	kbdev_list = kbase_dev_list_get();
	list_for_each(entry, kbdev_list) {
		struct kbase_device *kbdev = NULL;
		struct kbasep_kctx_list_element *element;

		kbdev = list_entry(entry, struct kbase_device, entry);
		/* output the total memory usage and cap for this device */
		mutex_lock(&kbdev->kctx_list_lock);
		list_for_each_entry(element, &kbdev->kctx_list, link) {
			spin_lock(&(element->kctx->mem_pool.pool_lock));
			free_size += element->kctx->mem_pool.cur_size;
			spin_unlock(&(element->kctx->mem_pool.pool_lock));
		}
		mutex_unlock(&kbdev->kctx_list_lock);
		seq_printf(sfile, "===========================================================\n");
		seq_printf(sfile, " %16s  %18s  %20s\n", \
				"dev name", \
				"total used pages", \
				"total shrink pages");
		seq_printf(sfile, "-----------------------------------------------------------\n");
		seq_printf(sfile, " %16s  %18u  %20zu\n", \
				kbdev->devname, \
				atomic_read(&(kbdev->memdev.used_pages)), \
				free_size);
		seq_printf(sfile, "===========================================================\n\n");
		seq_printf(sfile, "%28s     %20s  %16s  %12s\n", \
				"context name", \
				"context addr", \
				"used pages", \
				"shrink pages");
		seq_printf(sfile, "====================================================");
		seq_printf(sfile, "========================================\n");
		mutex_lock(&kbdev->kctx_list_lock);
		list_for_each_entry(element, &kbdev->kctx_list, link) {
			/* output the memory usage and cap for each kctx
			* opened on this device */

			spin_lock(&(element->kctx->mem_pool.pool_lock));
			each_free_size = element->kctx->mem_pool.cur_size;
			spin_unlock(&(element->kctx->mem_pool.pool_lock));
			seq_printf(sfile, "  (%24s), %s-0x%p    %12u  %10zu\n", \
					element->kctx->name, \
					"kctx", \
					element->kctx, \
					atomic_read(&(element->kctx->used_pages)),
					each_free_size );
		}
		mutex_unlock(&kbdev->kctx_list_lock);
	}
	kbase_dev_list_put(kbdev_list);
	return ret;
}

void gpu_update_status(void *dev, char *str, u32 val)
{
	struct kbase_device *kbdev;

	kbdev = (struct kbase_device *)dev;
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	if(strcmp(str, "completion_code") == 0)
	{
		if(val == 0x58) /* DATA_INVALID_FAULT */
			((struct exynos_context *)kbdev->platform_context)->data_invalid_fault_count ++;
		else if((val & 0xf0) == 0xc0) /* MMU_FAULT */
			((struct exynos_context *)kbdev->platform_context)->mmu_fault_count ++;

	}
	else if(strcmp(str, "reset_count") == 0)
		((struct exynos_context *)kbdev->platform_context)->reset_count++;
}

#define KBASE_MMU_PAGE_ENTRIES	512

static phys_addr_t mmu_pte_to_phy_addr(u64 entry)
{
	if (!(entry & 1))
		return 0;

	return entry & ~0xFFF;
}

/* MALI_SEC_INTEGRATION */
static void gpu_page_table_info_dp_level(struct kbase_context *kctx, u64 vaddr, phys_addr_t pgd, int level)
{
	u64 *pgd_page;
	int i;
	int index = (vaddr >> (12 + ((3 - level) * 9))) & 0x1FF;
	int min_index = index - 3;
	int max_index = index + 3;

	if (min_index < 0)
		min_index = 0;
	if (max_index >= KBASE_MMU_PAGE_ENTRIES)
		max_index = KBASE_MMU_PAGE_ENTRIES - 1;

	/* Map and dump entire page */

	pgd_page = kmap(pfn_to_page(PFN_DOWN(pgd)));

	dev_err(kctx->kbdev->dev, "Dumping level %d @ physical address 0x%016llX (matching index %d):\n", level, pgd, index);

	if (!pgd_page) {
		dev_err(kctx->kbdev->dev, "kmap failure\n");
		return;
	}

	for (i = min_index; i <= max_index; i++) {
		if (i == index) {
			dev_err(kctx->kbdev->dev, "[%03d]: 0x%016llX *\n", i, pgd_page[i]);
		} else {
			dev_err(kctx->kbdev->dev, "[%03d]: 0x%016llX\n", i, pgd_page[i]);
		}
	}

	/* parse next level (if any) */

	if ((pgd_page[index] & 3) == ENTRY_IS_PTE) {
		phys_addr_t target_pgd = mmu_pte_to_phy_addr(pgd_page[index]);
		gpu_page_table_info_dp_level(kctx, vaddr, target_pgd, level + 1);
	} else if ((pgd_page[index] & 3) == ENTRY_IS_ATE) {
		dev_err(kctx->kbdev->dev, "Final physical address: 0x%016llX\n", pgd_page[index] & ~(0xFFF | ENTRY_FLAGS_MASK));
	} else {
		dev_err(kctx->kbdev->dev, "Final physical address: INVALID!\n");
	}

	kunmap(pfn_to_page(PFN_DOWN(pgd)));
}

void gpu_debug_pagetable_info(void *ctx, u64 vaddr)
{
	struct kbase_context *kctx;

	kctx = (struct kbase_context *)ctx;
	KBASE_DEBUG_ASSERT(kctx != NULL);

	dev_err(kctx->kbdev->dev, "Looking up virtual GPU address: 0x%016llX\n", vaddr);
	gpu_page_table_info_dp_level(kctx, vaddr, kctx->pgd, 0);
}

#ifdef CONFIG_MALI_SEC_CL_BOOST
void gpu_cl_boost_init(void *dev)
{
	struct kbase_device *kbdev;

	kbdev = (struct kbase_device *)dev;
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	atomic_set(&kbdev->pm.backend.metrics.time_compute_jobs, 0);
	atomic_set(&kbdev->pm.backend.metrics.time_vertex_jobs, 0);
	atomic_set(&kbdev->pm.backend.metrics.time_fragment_jobs, 0);
}

void gpu_cl_boost_update_utilization(void *dev, void *atom, u64 microseconds_spent)
{
	struct kbase_jd_atom *katom;
	struct kbase_device *kbdev;

	kbdev = (struct kbase_device *)dev;
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	katom = (struct kbase_jd_atom *)atom;
	KBASE_DEBUG_ASSERT(katom != NULL);

	if (katom->core_req & BASE_JD_REQ_ONLY_COMPUTE)
		atomic_add((microseconds_spent >> KBASE_PM_TIME_SHIFT), &kbdev->pm.backend.metrics.time_compute_jobs);
	else if (katom->core_req & BASE_JD_REQ_FS)
		atomic_add((microseconds_spent >> KBASE_PM_TIME_SHIFT), &kbdev->pm.backend.metrics.time_fragment_jobs);
	else if (katom->core_req & BASE_JD_REQ_CS)
		atomic_add((microseconds_spent >> KBASE_PM_TIME_SHIFT), &kbdev->pm.backend.metrics.time_vertex_jobs);
}
#endif

#ifdef CONFIG_MALI_DVFS
static void dvfs_callback(struct work_struct *data)
{
	unsigned long flags;
	struct kbasep_pm_metrics_data *metrics;
	struct kbase_device *kbdev;
	struct exynos_context *platform;

	KBASE_DEBUG_ASSERT(data != NULL);

	metrics = container_of(data, struct kbasep_pm_metrics_data, work.work);

	kbdev = metrics->kbdev;
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	platform = (struct exynos_context *)kbdev->platform_context;
	KBASE_DEBUG_ASSERT(platform != NULL);

	kbase_platform_dvfs_event(metrics->kbdev, 0);

	spin_lock_irqsave(&metrics->lock, flags);

	if (metrics->timer_active)
		queue_delayed_work_on(0, platform->dvfs_wq,
				platform->delayed_work, msecs_to_jiffies(platform->polling_speed));

	spin_unlock_irqrestore(&metrics->lock, flags);
}

void gpu_pm_metrics_init(void *dev)
{
	struct kbase_device *kbdev;
	struct exynos_context *platform;

	kbdev = (struct kbase_device *)dev;
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	platform = (struct exynos_context *)kbdev->platform_context;
	KBASE_DEBUG_ASSERT(platform != NULL);

	INIT_DELAYED_WORK(&kbdev->pm.backend.metrics.work, dvfs_callback);
	platform->dvfs_wq = create_workqueue("g3d_dvfs");
	platform->delayed_work = &kbdev->pm.backend.metrics.work;

	queue_delayed_work_on(0, platform->dvfs_wq,
		platform->delayed_work, msecs_to_jiffies(platform->polling_speed));
}

void gpu_pm_metrics_term(void *dev)
{
	struct kbase_device *kbdev;
	struct exynos_context *platform;

	kbdev = (struct kbase_device *)dev;
	KBASE_DEBUG_ASSERT(kbdev != NULL);

	platform = (struct exynos_context *)kbdev->platform_context;
	KBASE_DEBUG_ASSERT(platform != NULL);

	cancel_delayed_work(platform->delayed_work);
	flush_workqueue(platform->dvfs_wq);
	destroy_workqueue(platform->dvfs_wq);
}
#endif

/* caller needs to hold kbdev->pm.backend.metrics.lock before calling this function */
#ifdef CONFIG_MALI_DVFS
int gpu_pm_get_dvfs_utilisation(struct kbase_device *kbdev, int *util_gl_share, int util_cl_share[2])
{
	unsigned long flags;
	int utilisation = 0;
#if !defined(CONFIG_MALI_SEC_CL_BOOST)
	int busy;
#else
	int compute_time = 0, vertex_time = 0, fragment_time = 0, total_time = 0, compute_time_rate = 0;
#endif

	ktime_t now = ktime_get();
	ktime_t diff;

	KBASE_DEBUG_ASSERT(kbdev != NULL);
	spin_lock_irqsave(&kbdev->pm.backend.metrics.lock, flags);
	diff = ktime_sub(now, kbdev->pm.backend.metrics.time_period_start);

	if (kbdev->pm.backend.metrics.gpu_active) {
		u32 ns_time = (u32) (ktime_to_ns(diff) >> KBASE_PM_TIME_SHIFT);
		kbdev->pm.backend.metrics.time_busy += ns_time;
		kbdev->pm.backend.metrics.busy_cl[0] += ns_time * kbdev->pm.backend.metrics.active_cl_ctx[0];
		kbdev->pm.backend.metrics.busy_cl[1] += ns_time * kbdev->pm.backend.metrics.active_cl_ctx[1];
		kbdev->pm.backend.metrics.time_period_start = now;
	} else {
		kbdev->pm.backend.metrics.time_idle += (u32) (ktime_to_ns(diff) >> KBASE_PM_TIME_SHIFT);
		kbdev->pm.backend.metrics.time_period_start = now;
	}
	spin_unlock_irqrestore(&kbdev->pm.backend.metrics.lock, flags);
	if (kbdev->pm.backend.metrics.time_idle + kbdev->pm.backend.metrics.time_busy == 0) {
		/* No data - so we return NOP */
		utilisation = -1;
#if !defined(CONFIG_MALI_SEC_CL_BOOST)
		if (util_gl_share)
			*util_gl_share = -1;
		if (util_cl_share) {
			util_cl_share[0] = -1;
			util_cl_share[1] = -1;
		}
#endif
		goto out;
	}

	utilisation = (100 * kbdev->pm.backend.metrics.time_busy) /
			(kbdev->pm.backend.metrics.time_idle +
			 kbdev->pm.backend.metrics.time_busy);

#if !defined(CONFIG_MALI_SEC_CL_BOOST)
	busy = kbdev->pm.backend.metrics.busy_gl +
		kbdev->pm.backend.metrics.busy_cl[0] +
		kbdev->pm.backend.metrics.busy_cl[1];

	if (busy != 0) {
		if (util_gl_share)
			*util_gl_share =
				(100 * kbdev->pm.backend.metrics.busy_gl) / busy;
		if (util_cl_share) {
			util_cl_share[0] =
				(100 * kbdev->pm.backend.metrics.busy_cl[0]) / busy;
			util_cl_share[1] =
				(100 * kbdev->pm.backend.metrics.busy_cl[1]) / busy;
		}
	} else {
		if (util_gl_share)
			*util_gl_share = -1;
		if (util_cl_share) {
			util_cl_share[0] = -1;
			util_cl_share[1] = -1;
		}
	}
#endif

#ifdef CONFIG_MALI_SEC_CL_BOOST
	compute_time = atomic_read(&kbdev->pm.backend.metrics.time_compute_jobs);
	vertex_time = atomic_read(&kbdev->pm.backend.metrics.time_vertex_jobs);
	fragment_time = atomic_read(&kbdev->pm.backend.metrics.time_fragment_jobs);
	total_time = compute_time + vertex_time + fragment_time;

	if (compute_time > 0 && total_time > 0) {
		compute_time_rate = (100 * compute_time) / total_time;
		if (compute_time_rate == 100)
			kbdev->pm.backend.metrics.is_full_compute_util = true;
		else
			kbdev->pm.backend.metrics.is_full_compute_util = false;
	} else
		kbdev->pm.backend.metrics.is_full_compute_util = false;
#endif
 out:

	spin_lock_irqsave(&kbdev->pm.backend.metrics.lock, flags);
	kbdev->pm.backend.metrics.time_idle = 0;
	kbdev->pm.backend.metrics.time_busy = 0;
#ifdef CONFIG_MALI_SEC_CL_BOOST
	atomic_set(&kbdev->pm.backend.metrics.time_compute_jobs, 0);
	atomic_set(&kbdev->pm.backend.metrics.time_vertex_jobs, 0);
	atomic_set(&kbdev->pm.backend.metrics.time_fragment_jobs, 0);
#else
	kbdev->pm.backend.metrics.busy_cl[0] = 0;
	kbdev->pm.backend.metrics.busy_cl[1] = 0;
	kbdev->pm.backend.metrics.busy_gl = 0;
#endif
	spin_unlock_irqrestore(&kbdev->pm.backend.metrics.lock, flags);
	return utilisation;
}
#endif /* CONFIG_MALI_DVFS */

/* MALI_SEC_INTEGRATION */
static bool gpu_mem_profile_check_kctx(void *ctx)
{
	struct kbase_context *kctx;
	struct kbase_device *kbdev;
	struct kbasep_kctx_list_element *element, *tmp;
	bool found_element = false;

	kctx = (struct kbase_context *)ctx;
	kbdev = gpu_get_device_structure();

	list_for_each_entry_safe(element, tmp, &kbdev->kctx_list, link) {
		if (element->kctx == kctx) {
			if (kctx->destroying_context == false) {
				found_element = true;
				break;
			}
		}
	}

	return found_element;
}

struct kbase_vendor_callbacks exynos_callbacks = {
	.create_context = gpu_create_context,
	.destroy_context = gpu_destroy_context,
#ifdef CONFIG_MALI_SEC_CL_BOOST
	.cl_boost_init = gpu_cl_boost_init,
	.cl_boost_update_utilization = gpu_cl_boost_update_utilization,
#else
	.cl_boost_init = NULL,
	.cl_boost_update_utilization = NULL,
#endif
#if defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS7890)
	.init_hw = exynos_gpu_init_hw,
#else
	.init_hw = NULL,
#endif
#ifdef CONFIG_MALI_DVFS
	.pm_metrics_init = gpu_pm_metrics_init,
	.pm_metrics_term = gpu_pm_metrics_term,
#else
	.pm_metrics_init = NULL,
	.pm_metrics_term = NULL,
#endif
	.debug_pagetable_info = gpu_debug_pagetable_info,
	.mem_profile_check_kctx = gpu_mem_profile_check_kctx,
	.register_dump = gpu_register_dump,
};

uintptr_t gpu_get_callbacks(void)
{
	return ((uintptr_t)&exynos_callbacks);
}

