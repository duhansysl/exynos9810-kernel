/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Exynos-SnapShot debugging framework for Exynos SoC
 *
 * Author: Hosung Kim <Hosung0.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/bootmem.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/memblock.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>

#include <soc/samsung/cal-if.h>
#include <dt-bindings/soc/samsung/exynos-ss-table.h>

#include <linux/sec_debug.h>
#ifdef CONFIG_SEC_PM_DEBUG
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#endif

#include "exynos-ss-local.h"

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,5,00)
extern void register_hook_logbuf(void (*)(const char));
#else
extern void register_hook_logbuf(void (*)(const char *, size_t));
#endif
extern void register_hook_logger(void (*)(const char *, const char *, size_t));

struct exynos_ss_interface {
	struct exynos_ss_log *info_event;
	struct exynos_ss_item info_log[ESS_ITEM_MAX_NUM];
};

struct exynos_ss_ops ess_ops = {
	.pd_status = cal_pd_status,
};

const char *debug_level_val[] = {
	"low",
	"mid",
};

struct exynos_ss_item ess_items[] = {
	{"header",		{0, 0, 0, false, false}, NULL ,NULL, 0},
	{"log_kernel",		{0, 0, 0, false, false}, NULL ,NULL, 0},
	{"log_platform",	{0, 0, 0, false, false}, NULL ,NULL, 0},
	{"log_sfr",		{0, 0, 0, false, false}, NULL ,NULL, 0},
	{"log_s2d",		{0, 0, 0, true, false}, NULL, NULL, 0},
	{"log_cachedump",	{0, 0, 0, true, false}, NULL, NULL, 0},
	{"log_etm",		{0, 0, 0, true, false}, NULL ,NULL, 0},
	{"log_pstore",		{0, 0, 0, true, false}, NULL ,NULL, 0},
	{"log_kevents",		{0, 0, 0, false, false}, NULL ,NULL, 0},
};

/*  External interface variable for trace debugging */
static struct exynos_ss_interface ess_info __attribute__ ((used));

struct exynos_ss_base ess_base;
struct exynos_ss_log *ess_log = NULL;
struct exynos_ss_desc ess_desc;

void sec_debug_get_kevent_info(struct ess_info_offset *p, int type)
{
	unsigned long kevent_base_va = (unsigned long)(ess_log->task);
	unsigned long kevent_base_pa = ess_items[ess_desc.kevents_num].entry.paddr;

	switch (type) {
	case DSS_KEVENT_TASK:
		p->base = kevent_base_pa + (unsigned long)(ess_log->task) - kevent_base_va;
		p->nr = ESS_LOG_MAX_NUM;
		p->size = sizeof(struct __task_log);
		p->per_core = 1;
		break;

	case DSS_KEVENT_WORK:
		p->base = kevent_base_pa + (unsigned long)(ess_log->work) - kevent_base_va;
		p->nr = ESS_LOG_MAX_NUM;
		p->size = sizeof(struct __work_log);
		p->per_core = 1;
		break;

	case DSS_KEVENT_IRQ:
		p->base = kevent_base_pa + (unsigned long)(ess_log->irq) - kevent_base_va;
		p->nr = ESS_LOG_MAX_NUM * 2;
		p->size = sizeof(struct __irq_log);
		p->per_core = 1;
		break;

	case DSS_KEVENT_FREQ:
		p->base = kevent_base_pa + (unsigned long)(ess_log->freq) - kevent_base_va;
		p->nr = ESS_LOG_MAX_NUM;
		p->size = sizeof(struct __freq_log);
		p->per_core = 0;
		break;

	case DSS_KEVENT_IDLE:
		p->base = kevent_base_pa + (unsigned long)(ess_log->cpuidle) - kevent_base_va;
		p->nr = ESS_LOG_MAX_NUM;
		p->size = sizeof(struct __cpuidle_log);
		p->per_core = 1;
		break;

	case DSS_KEVENT_THRM:
		p->base = kevent_base_pa + (unsigned long)(ess_log->thermal) - kevent_base_va;
		p->nr = ESS_LOG_MAX_NUM;
		p->size = sizeof(struct __thermal_log);
		p->per_core = 0;
		break;

	case DSS_KEVENT_ACPM:
		p->base = kevent_base_pa + (unsigned long)(ess_log->acpm) - kevent_base_va;
		p->nr = ESS_LOG_MAX_NUM;
		p->size = sizeof(struct __acpm_log);
		p->per_core = 0;
		break;

	default:
		p->base = 0;
		p->nr = 0;
		p->size = 0;
		p->per_core = 0;
		break;
	}

	p->last = sec_debug_get_kevent_index_addr(type);
}

int exynos_ss_get_debug_level(void)
{
	return ess_desc.debug_level;
}

int exynos_ss_set_enable(const char *name, int en)
{
	struct exynos_ss_item *item = NULL;
	unsigned long i;

	if (!strncmp(name, "base", strlen(name))) {
		ess_base.enabled = en;
		pr_info("exynos-snapshot: %sabled\n", en ? "en" : "dis");
	} else {
		for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
			if (!strncmp(ess_items[i].name, name, strlen(name))) {
				item = &ess_items[i];
				item->entry.enabled = en;
				item->time = local_clock();
				pr_info("exynos-snapshot: item - %s is %sabled\n",
						name, en ? "en" : "dis");
				break;
			}
		}
	}
	return 0;
}
EXPORT_SYMBOL(exynos_ss_set_enable);

int exynos_ss_try_enable(const char *name, unsigned long long duration)
{
	struct exynos_ss_item *item = NULL;
	unsigned long long time;
	unsigned long i;
	int ret = -1;

	/* If ESS was disabled, just return */
	if (unlikely(!ess_base.enabled) || !exynos_ss_get_enable("header"))
		return ret;

	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		if (!strncmp(ess_items[i].name, name, strlen(name))) {
			item = &ess_items[i];

			/* We only interest in disabled */
			if (!item->entry.enabled) {
				time = local_clock() - item->time;
				if (time > duration) {
					item->entry.enabled = true;
					ret = 1;
				} else
					ret = 0;
			}
			break;
		}
	}
	return ret;
}
EXPORT_SYMBOL(exynos_ss_try_enable);

int exynos_ss_get_enable(const char *name)
{
	struct exynos_ss_item *item = NULL;
	unsigned long i;
	int ret = 0;

	if (!strncmp(name, "base", strlen(name)))
		return ess_base.enabled;

	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		if (!strncmp(ess_items[i].name, name, strlen(name))) {
			item = &ess_items[i];
			ret = item->entry.enabled;
			break;
		}
	}
	return ret;
}
EXPORT_SYMBOL(exynos_ss_get_enable);

static inline int exynos_ss_check_eob(struct exynos_ss_item *item,
						size_t size)
{
	size_t max, cur;

	max = (size_t)(item->head_ptr + item->entry.size);
	cur = (size_t)(item->curr_ptr + size);

	if (unlikely(cur > max))
		return -1;
	else
		return 0;
}

static inline void exynos_ss_hook_logger(const char *name,
					 const char *buf, size_t size)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.log_platform_num];

	if (likely(ess_base.enabled && item->entry.enabled)) {
		if (unlikely((exynos_ss_check_eob(item, size))))
			item->curr_ptr = item->head_ptr;
		memcpy(item->curr_ptr, buf, size);
		item->curr_ptr += size;
	}
}

size_t exynos_ss_get_curr_ptr_for_sysrq(void)
{
#ifdef CONFIG_SEC_DEBUG_SYSRQ_KMSG
	struct exynos_ss_item *item = &ess_items[ess_desc.log_kernel_num];
	return (size_t)item->curr_ptr;
#else
	return 0;
#endif
}

#ifdef CONFIG_SEC_PM_DEBUG
static bool sec_log_full;
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,5,00)
static inline void exynos_ss_hook_logbuf(const char buf)
{
	unsigned int last_buf;
	struct exynos_ss_item *item = &ess_items[ess_desc.log_kernel_num];

	if (likely(ess_base.enabled && item->entry.enabled)) {
		if (exynos_ss_check_eob(item, 1)) {
			item->curr_ptr = item->head_ptr;
#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
			*((unsigned long long *)(item->head_ptr + item->entry.size - (size_t)0x08)) = SEC_LKMSG_MAGICKEY;
#endif
#ifdef CONFIG_SEC_PM_DEBUG
			if (unlikely(!sec_log_full))
				sec_log_full = true;
#endif
		}

		item->curr_ptr[0] = buf;
		item->curr_ptr++;

		/*  save the address of last_buf to physical address */
		last_buf = (unsigned int)item->curr_ptr;
		__raw_writel(item->entry.paddr + (last_buf - item->entry.vaddr),
				exynos_ss_get_base_vaddr() + ESS_OFFSET_LAST_LOGBUF);
	}
}
#else
static inline void exynos_ss_hook_logbuf(const char *buf, size_t size)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.log_kernel_num];

	if (likely(ess_base.enabled && item->entry.enabled)) {
		size_t last_buf;

		if (exynos_ss_check_eob(item, size)) {
			item->curr_ptr = item->head_ptr;
#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
			*((unsigned long long *)(item->head_ptr + item->entry.size - (size_t)0x08)) = SEC_LKMSG_MAGICKEY;
#endif
#ifdef CONFIG_SEC_PM_DEBUG
			if (unlikely(!sec_log_full))
				sec_log_full = true;
#endif
		}

		memcpy(item->curr_ptr, buf, size);
		item->curr_ptr += size;
		/*  save the address of last_buf to physical address */
		last_buf = (size_t)item->curr_ptr;

		__raw_writel(item->entry.paddr + (last_buf - item->entry.vaddr),
				exynos_ss_get_base_vaddr() + ESS_OFFSET_LAST_LOGBUF);
	}
}
#endif

static bool exynos_ss_check_pmu(struct exynos_ss_sfrdump *sfrdump,
						const struct device_node *np)
{
	int ret = 0, count, i;
	unsigned int val;

	if (!sfrdump->pwr_mode)
		return true;

	count = of_property_count_u32_elems(np, "cal-pd-id");
	for (i = 0; i < count; i++) {
		ret = of_property_read_u32_index(np, "cal-pd-id", i, &val);
		if (ret < 0) {
			pr_err("failed to get pd-id - %s\n", sfrdump->name);
			return false;
		}
		ret = ess_ops.pd_status(val);
		if (ret < 0) {
			pr_err("not powered - %s (pd-id: %d)\n", sfrdump->name, i);
			return false;
		}
	}
	return true;
}

void exynos_ss_dump_sfr(void)
{
	struct exynos_ss_sfrdump *sfrdump;
	struct exynos_ss_item *item = &ess_items[ess_desc.log_sfr_num];
	struct list_head *entry;
	struct device_node *np;
	unsigned int reg, offset, val, size;
	int i, ret;
	static char buf[SZ_64];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	if (list_empty(&ess_desc.sfrdump_list)) {
		pr_emerg("exynos-snapshot: %s: No information\n", __func__);
		return;
	}

	list_for_each(entry, &ess_desc.sfrdump_list) {
		sfrdump = list_entry(entry, struct exynos_ss_sfrdump, list);
		np = of_node_get(sfrdump->node);
		ret = exynos_ss_check_pmu(sfrdump, np);
		if (!ret)
			/* may off */
			continue;

		for (i = 0; i < sfrdump->num; i++) {
			ret = of_property_read_u32_index(np, "addr", i, &reg);
			if (ret < 0) {
				pr_err("exynos-snapshot: failed to get address information - %s\n",
					sfrdump->name);
				break;
			}
			if (reg == 0xFFFFFFFF || reg == 0)
				break;
			offset = reg - sfrdump->phy_reg;
			if (reg < offset) {
				pr_err("exynos-snapshot: invalid address information - %s: 0x%08x\n",
				sfrdump->name, reg);
				break;
			}
			val = __raw_readl(sfrdump->reg + offset);
			snprintf(buf, SZ_64, "0x%X = 0x%0X\n",reg, val);
			size = (unsigned int)strlen(buf);
			if (unlikely((exynos_ss_check_eob(item, size))))
				item->curr_ptr = item->head_ptr;
			memcpy(item->curr_ptr, buf, strlen(buf));
			item->curr_ptr += strlen(buf);
		}
		of_node_put(np);
		pr_info("exynos-snapshot: complete to dump %s\n", sfrdump->name);
	}

}

static int exynos_ss_sfr_dump_init(struct device_node *np)
{
	struct device_node *dump_np;
	struct exynos_ss_sfrdump *sfrdump;
	char *dump_str;
	int count, ret, i;
	u32 phy_regs[2];

	ret = of_property_count_strings(np, "sfr-dump-list");
	if (ret < 0) {
		pr_err("failed to get sfr-dump-list\n");
		return ret;
	}
	count = ret;

	INIT_LIST_HEAD(&ess_desc.sfrdump_list);
	for (i = 0; i < count; i++) {
		ret = of_property_read_string_index(np, "sfr-dump-list", i,
						(const char **)&dump_str);
		if (ret < 0) {
			pr_err("failed to get sfr-dump-list\n");
			continue;
		}

		dump_np = of_get_child_by_name(np, dump_str);
		if (!dump_np) {
			pr_err("failed to get %s node, count:%d\n", dump_str, count);
			continue;
		}

		sfrdump = kzalloc(sizeof(struct exynos_ss_sfrdump), GFP_KERNEL);
		if (!sfrdump) {
			pr_err("failed to get memory region of exynos_ss_sfrdump\n");
			of_node_put(dump_np);
			continue;
		}

		ret = of_property_read_u32_array(dump_np, "reg", phy_regs, 2);
		if (ret < 0) {
			pr_err("failed to get register information\n");
			of_node_put(dump_np);
			kfree(sfrdump);
			continue;
		}

		sfrdump->reg = ioremap(phy_regs[0], phy_regs[1]);
		if (!sfrdump->reg) {
			pr_err("failed to get i/o address %s node\n", dump_str);
			of_node_put(dump_np);
			kfree(sfrdump);
			continue;
		}
		sfrdump->name = dump_str;

		ret = of_property_count_u32_elems(dump_np, "addr");
		if (ret < 0) {
			pr_err("failed to get addr count\n");
			of_node_put(dump_np);
			kfree(sfrdump);
			continue;
		}
		sfrdump->phy_reg = phy_regs[0];
		sfrdump->num = ret;

		ret = of_property_count_u32_elems(dump_np, "cal-pd-id");
		if (ret < 0)
			sfrdump->pwr_mode = false;
		else
			sfrdump->pwr_mode = true;

		sfrdump->node = dump_np;
		list_add(&sfrdump->list, &ess_desc.sfrdump_list);

		pr_info("success to regsiter %s\n", sfrdump->name);
		of_node_put(dump_np);
		ret = 0;
	}
	return ret;
}

static size_t __init exynos_ss_remap(void)
{
	unsigned long i;
	unsigned int enabled_count = 0;
	size_t pre_vaddr, item_size;
	pgprot_t prot = __pgprot(PROT_NORMAL_NC);
	int page_size, ret;
	struct page *page;
	struct page **pages;

	page_size = ess_desc.vm.size / PAGE_SIZE;
	pages = kzalloc(sizeof(struct page*) * page_size, GFP_KERNEL);
	page = phys_to_page(ess_desc.vm.phys_addr);

	for (i = 0; i < page_size; i++)
		pages[i] = page++;

	ret = map_vm_area(&ess_desc.vm, prot, pages);
	if (ret) {
		pr_err("exynos-snapshot: failed to mapping between virt and phys");
		kfree(pages);
		return -ENOMEM;
	}
	kfree(pages);

	/* initializing value */
	pre_vaddr = (size_t)ess_base.vaddr;

	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		/* fill rest value of ess_items arrary */
		if (ess_items[i].entry.enabled) {
			item_size = ess_items[i].entry.size;
			ess_items[i].entry.vaddr = pre_vaddr;
			ess_items[i].head_ptr = (unsigned char *)ess_items[i].entry.vaddr;
			ess_items[i].curr_ptr = (unsigned char *)ess_items[i].entry.vaddr;

			/* For Next */
			pre_vaddr = ess_items[i].entry.vaddr + item_size;
			enabled_count++;
		}
	}
	ess_desc.log_enable_cnt = enabled_count;
	return 0;
}

static int __init exynos_ss_init_desc(void)
{
	unsigned int i;
	size_t len;

	/* initialize ess_desc */
	memset((struct exynos_ss_desc *)&ess_desc, 0, sizeof(struct exynos_ss_desc));
	ess_desc.callstack = CONFIG_EXYNOS_SNAPSHOT_CALLSTACK;
	raw_spin_lock_init(&ess_desc.ctrl_lock);
	raw_spin_lock_init(&ess_desc.nmi_lock);

	for (i = 0; i < (unsigned int)ARRAY_SIZE(ess_items); i++) {
		len = strlen(ess_items[i].name);
		if (!strncmp(ess_items[i].name, "header", len))
			ess_desc.header_num = i;
		else if (!strncmp(ess_items[i].name, "log_kevents", len))
			ess_desc.kevents_num = i;
		else if (!strncmp(ess_items[i].name, "log_kernel", len))
			ess_desc.log_kernel_num = i;
		else if (!strncmp(ess_items[i].name, "log_platform", len))
			ess_desc.log_platform_num = i;
		else if (!strncmp(ess_items[i].name, "log_sfr", len))
			ess_desc.log_sfr_num = i;
		else if (!strncmp(ess_items[i].name, "log_pstore", len))
			ess_desc.log_pstore_num = i;
	}

#ifdef CONFIG_S3C2410_WATCHDOG
	ess_desc.no_wdt_dev = false;
#else
	ess_desc.no_wdt_dev = true;
#endif
	return 0;
}

#ifdef CONFIG_OF_RESERVED_MEM
static int __init exynos_ss_reserved_mem_setup(struct reserved_mem *remem)
{
	exynos_ss_init_desc();

	ess_base.paddr = remem->base;
	ess_base.vaddr = (size_t)(ESS_FIXED_VIRT_BASE);
	ess_base.size = remem->size;
	ess_base.enabled = false;

	/* Reserved fixed virtual memory within VMALLOC region */
	ess_desc.vm.phys_addr = remem->base;
	ess_desc.vm.addr = (void *)(ESS_FIXED_VIRT_BASE);
	ess_desc.vm.size = remem->size;
	ess_desc.vm.flags = VM_NO_GUARD;

	vm_area_add_early(&ess_desc.vm);

	pr_info("exynos-snapshot: memory reserved complete : 0x%llx, 0x%zx, 0x%llx\n",
			remem->base, (size_t)(ESS_FIXED_VIRT_BASE), remem->size);
	return 0;
}
RESERVEDMEM_OF_DECLARE(exynos_ss, "exynos,exynos_ss", exynos_ss_reserved_mem_setup);

static int __init exynos_ss_item_reserved_mem_setup(struct reserved_mem *remem)
{
	unsigned int i = 0;

	for (i = 0; i < (unsigned int)ARRAY_SIZE(ess_items); i++) {
		if (strnstr(remem->name, ess_items[i].name, strlen(remem->name)))
			break;
	}

	if (i == ARRAY_SIZE(ess_items))
		return -ENODEV;

	ess_items[i].entry.paddr = remem->base;
	ess_items[i].entry.size = remem->size;
	ess_items[i].entry.enabled = true;

	return 0;
}

#define DECLARE_EXYNOS_SS_RESERVED_REGION(compat, name) \
RESERVEDMEM_OF_DECLARE(name, compat#name, exynos_ss_item_reserved_mem_setup)

DECLARE_EXYNOS_SS_RESERVED_REGION("exynos_ss,", header);
DECLARE_EXYNOS_SS_RESERVED_REGION("exynos_ss,", log_kernel);
DECLARE_EXYNOS_SS_RESERVED_REGION("exynos_ss,", log_platform);
DECLARE_EXYNOS_SS_RESERVED_REGION("exynos_ss,", log_sfr);
DECLARE_EXYNOS_SS_RESERVED_REGION("exynos_ss,", log_s2d);
DECLARE_EXYNOS_SS_RESERVED_REGION("exynos_ss,", log_cachedump);
DECLARE_EXYNOS_SS_RESERVED_REGION("exynos_ss,", log_etm);
DECLARE_EXYNOS_SS_RESERVED_REGION("exynos_ss,", log_pstore);
DECLARE_EXYNOS_SS_RESERVED_REGION("exynos_ss,", log_kevents);
#endif

/*
 *  ---------------------------------------------------------------------
 *  - dummy data:phy_addr, virtual_addr, buffer_size, magic_key(4K)	-
 *  ---------------------------------------------------------------------
 *  -		Cores MMU register(4K)					-
 *  ---------------------------------------------------------------------
 *  -		Cores CPU register(4K)					-
 *  ---------------------------------------------------------------------
 */
static int __init exynos_ss_output(void)
{
	unsigned long i, size = 0;

	pr_info("exynos-snapshot physical / virtual memory layout:\n");
	for (i = 0; i < ARRAY_SIZE(ess_items); i++) {
		if (ess_items[i].entry.enabled)
			pr_info("%-12s: phys:0x%zx / virt:0x%zx / size:0x%zx\n",
				ess_items[i].name,
				ess_items[i].entry.paddr,
				ess_items[i].entry.vaddr,
				ess_items[i].entry.size);
		size += ess_items[i].entry.size;
	}

	pr_info("total_item_size: %ldKB, exynos_ss_log struct size: %dKB\n",
			size / SZ_1K, exynos_ss_log_size / SZ_1K);

	return 0;
}

/*	Header dummy data(4K)
 *	-------------------------------------------------------------------------
 *		0		4		8		C
 *	-------------------------------------------------------------------------
 *	0	vaddr	phy_addr	size		magic_code
 *	4	Scratch_val	logbuf_addr	0		0
 *	-------------------------------------------------------------------------
*/

static void __init exynos_ss_fixmap_header(void)
{
	/*  fill 0 to next to header */
	size_t vaddr, paddr, size;
	size_t *addr;

	vaddr = ess_items[ess_desc.header_num].entry.vaddr;
	paddr = ess_items[ess_desc.header_num].entry.paddr;
	size = ess_items[ess_desc.header_num].entry.size;

	/*  set to confirm exynos-snapshot */
	addr = (size_t *)vaddr;
	memcpy(addr, &ess_base, sizeof(struct exynos_ss_base));

	if (!exynos_ss_get_enable("header"))
		return;

	/*  initialize kernel event to 0 except only header */
	memset((size_t *)(vaddr + ESS_KEEP_HEADER_SZ), 0, size - ESS_KEEP_HEADER_SZ);
}

static void __init exynos_ss_fixmap(void)
{
	size_t last_buf;
	size_t vaddr, paddr, size;
	unsigned long i;

	/*  fixmap to header first */
	exynos_ss_fixmap_header();

	for (i = 1; i < ARRAY_SIZE(ess_items); i++) {
		if (!ess_items[i].entry.enabled)
			continue;

		/*  assign ess_item information */
		paddr = ess_items[i].entry.paddr;
		vaddr = ess_items[i].entry.vaddr;
		size = ess_items[i].entry.size;

		if (i == ess_desc.log_kernel_num) {
			/*  load last_buf address value(phy) by virt address */
			last_buf = (size_t)__raw_readl(exynos_ss_get_base_vaddr() +
							ESS_OFFSET_LAST_LOGBUF);
			/*  check physical address offset of kernel logbuf */
			if (last_buf >= ess_items[i].entry.paddr &&
				(last_buf) <= (ess_items[i].entry.paddr + ess_items[i].entry.size)) {
				/*  assumed valid address, conversion to virt */
				ess_items[i].curr_ptr = (unsigned char *)(ess_items[i].entry.vaddr +
							(last_buf - ess_items[i].entry.paddr));
			} else {
				/*  invalid address, set to first line */
				ess_items[i].curr_ptr = (unsigned char *)vaddr;
				/*  initialize logbuf to 0 */
				memset((size_t *)vaddr, 0, size);
			}
		} else {
			/*  initialized log to 0 if persist == false */
			if (!ess_items[i].entry.persist)
				memset((size_t *)vaddr, 0, size);
		}
		ess_info.info_log[i - 1].name = kstrdup(ess_items[i].name, GFP_KERNEL);
		ess_info.info_log[i - 1].head_ptr = (unsigned char *)ess_items[i].entry.vaddr;
		ess_info.info_log[i - 1].curr_ptr = NULL;
		ess_info.info_log[i - 1].entry.size = size;
	}

	ess_log = (struct exynos_ss_log *)(ess_items[ess_desc.kevents_num].entry.vaddr);

	/*  set fake translation to virtual address to debug trace */
	ess_info.info_event = ess_log;

	/* output the information of exynos-snapshot */
	exynos_ss_output();

#ifdef CONFIG_SEC_DEBUG
	sec_debug_save_last_kmsg(ess_items[ess_desc.log_kernel_num].head_ptr,
			ess_items[ess_desc.log_kernel_num].curr_ptr,
			ess_items[ess_desc.log_kernel_num].entry.size);
#endif
}

static int exynos_ss_init_dt_parse(struct device_node *np)
{
	int ret = 0;
	struct device_node *sfr_dump_np = of_get_child_by_name(np, "dump-info");

	if (!sfr_dump_np) {
		pr_err("exynos-snapshot: failed to get dump-info node\n");
		ret = -ENODEV;
	} else {
		ret = exynos_ss_sfr_dump_init(sfr_dump_np);
		if (ret < 0) {
			pr_err("exynos-snapshot: failed to register sfr dump node\n");
			ret = -ENODEV;
			of_node_put(sfr_dump_np);
		}
	}
	if (ret < 0)
		exynos_ss_set_enable("log_sfr", false);

	if (of_property_read_u32(np, "use_multistage_wdt_irq",
				&ess_desc.multistage_wdt_irq)) {
		ess_desc.multistage_wdt_irq = 0;
		pr_err("exynos-snapshot: no support multistage_wdt\n");
		ret = -EINVAL;
	}

	of_node_put(np);
	return ret;
}

static const struct of_device_id ess_of_match[] __initconst = {
	{ .compatible 	= "samsung,exynos-snapshot",
	  .data		= exynos_ss_init_dt_parse},
	{},
};

static int __init exynos_ss_init_dt(void)
{
	struct device_node *np;
	const struct of_device_id *matched_np;
	ess_initcall_t init_fn;

	np = of_find_matching_node_and_match(NULL, ess_of_match, &matched_np);

	if (!np) {
		pr_info("exynos-snapshot: couldn't find device tree file of exynos-snapshot\n");
		exynos_ss_set_enable("log_sfr", false);
		return -ENODEV;
	}

	init_fn = (ess_initcall_t)matched_np->data;
	return init_fn(np);
}

static int __init exynos_ss_init(void)
{
	if (ess_base.vaddr && ess_base.paddr && ess_base.size) {
	/*
	 *  for debugging when we don't know the virtual address of pointer,
	 *  In just privous the debug buffer, It is added 16byte dummy data.
	 *  start address(dummy 16bytes)
	 *  --> @virtual_addr | @phy_addr | @buffer_size | @magic_key(0xDBDBDBDB)
	 *  And then, the debug buffer is shown.
	 */
		if (exynos_ss_remap())
			goto out;

		exynos_ss_log_idx_init();
		exynos_ss_fixmap();
		exynos_ss_init_dt();
		exynos_ss_utils_init();

	        ess_desc.debug_level = exynos_ss_get_debug_level_reg();
	        pr_info("ess-snapshot: debug_level [%s]\n",
				debug_level_val[ess_desc.debug_level]);

		if (ess_desc.debug_level != ESS_DEBUG_LEVEL_LOW)
			exynos_ss_scratch_reg(ESS_SIGN_SCRATCH);
		exynos_ss_set_enable("base", true);

		register_hook_logbuf(exynos_ss_hook_logbuf);
		register_hook_logger(exynos_ss_hook_logger);
	}

	return 0;
out:
	pr_err("exynos-snapshot: %s failed\n", __func__);
	return 0;
}
early_initcall(exynos_ss_init);

#ifdef CONFIG_SEC_PM_DEBUG
static ssize_t sec_log_read_all(struct file *file, char __user *buf,
				size_t len, loff_t *offset)
{
	loff_t pos = *offset;
	ssize_t count;
	size_t size;
	struct exynos_ss_item *item = &ess_items[ess_desc.log_kernel_num];

	if (sec_log_full)
		size = item->entry.size;
	else
		size = (size_t)(item->curr_ptr - item->head_ptr);

	if (pos >= size)
		return 0;

	count = min(len, size);

	if ((pos + count) > size)
		count = size - pos;

	if (copy_to_user(buf, item->head_ptr + pos, count))
		return -EFAULT;

	*offset += count;
	return count;
}

static const struct file_operations sec_log_file_ops = {
	.owner = THIS_MODULE,
	.read = sec_log_read_all,
};

static int __init sec_log_late_init(void)
{
	struct proc_dir_entry *entry;
	struct exynos_ss_item *item = &ess_items[ess_desc.log_kernel_num];

	if (!item->head_ptr)
		return 0;

	entry = proc_create("sec_log", 0440, NULL, &sec_log_file_ops);
	if (!entry) {
		pr_err("%s: failed to create proc entry\n", __func__);
		return 0;
	}

	proc_set_size(entry, item->entry.size);

	return 0;
}

late_initcall(sec_log_late_init);
#endif /* CONFIG_SEC_PM_DEBUG */
