/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * EXYNOS - EL3 monitor support
 * Author: Jang Hyunsung <hs79.jang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/slab.h>
#include <linux/smc.h>
#include <linux/kallsyms.h>
#include <asm/cacheflush.h>

#include <soc/samsung/exynos-el3_mon.h>

static char *smc_lockup;
static char *smc_debug_mem;

#define EXYNOS_GET_IN_PD_DOWN			0
#define EXYNOS_WAKEUP_PD_DOWN			1

#ifdef CONFIG_EXYNOS_KERNEL_PROTECTION
static int __init exynos_protect_kernel_text(void)
{
	int ret = 0;
	unsigned long ktext_start_va = 0;
	unsigned long ktext_start_pa = 0;
	unsigned long ktext_end_va = 0;
	unsigned long ktext_end_pa = 0;

	/* Get virtual addresses of kernel text */
	ktext_start_va = kallsyms_lookup_name("_text");
	if (!ktext_start_va) {
		pr_err("%s: [ERROR] Kernel text start address is invalid\n",
								__func__);
		return -1;
	}

#ifdef CONFIG_UH_RKP
	ktext_end_va = kallsyms_lookup_name("rkp_pgt_bitmap");
#else
	ktext_end_va = kallsyms_lookup_name("_etext");
#endif
	if (!ktext_end_va) {
		pr_err("%s: [ERROR] Kernel text end address is invalid\n",
								__func__);
		return -1;
	}

	/* Translate VA to PA */
	ktext_start_pa = virt_to_phys((void *)ktext_start_va);
	ktext_end_pa = virt_to_phys((void *)ktext_end_va);

	pr_info("%s: Kernel text start VA(%#lx), PA(%#lx)\n",
			__func__, ktext_start_va, ktext_start_pa);
	pr_info("%s: Kernel text end VA(%#lx), PA(%#lx)\n",
			__func__, ktext_end_va, ktext_end_pa);

	/* Request to protect kernel text area */
	ret = exynos_smc(SMC_CMD_PROTECT_KERNEL_TEXT,
			ktext_start_pa,
			ktext_end_pa,
			0);
	if (ret) {
		switch (ret) {
		case EXYNOS_ERROR_NOT_VALID_ADDRESS:
			pr_err("%s: [ERROR] Invalid address\n", __func__);
			break;
		case EXYNOS_ERROR_TZASC_WRONG_REGION:
			pr_err("%s: [ERROR] Wrong TZASC region\n", __func__);
			break;
		case EXYNOS_ERROR_ALREADY_INITIALIZED:
			pr_err("%s: [ERROR] Already initialized\n", __func__);
			break;
		default:
			pr_err("%s: [ERROR] Unknown error [ret = %#x]\n",
					__func__, ret);
			break;
		}

		return ret;
	}

	pr_info("%s: Success to set Kernel code as read-only\n", __func__);

	return 0;
}
core_initcall(exynos_protect_kernel_text);
#endif

static int  __init exynos_set_debug_mem(void)
{
	int ret;
	char *phys;
	unsigned int size = PAGE_SIZE * 2;

	smc_debug_mem = kmalloc(size, GFP_KERNEL);

	if (!smc_debug_mem) {
		pr_err("%s: kmalloc for smc_debug failed.\n", __func__);
		return 0;
	}

	/* to map & flush memory */
	memset(smc_debug_mem, 0x00, size);
	__flush_dcache_area(smc_debug_mem, size);

	phys = (char *)virt_to_phys(smc_debug_mem);
	pr_err("%s: alloc kmem for smc_dbg virt: 0x%p phys: 0x%p size: %d.\n",
			__func__, smc_debug_mem, phys, size);
	ret = exynos_smc(SMC_CMD_SET_DEBUG_MEM, (u64)phys, (u64)size, 0);

	/* correct return value is input size */
	if (ret != size) {
		pr_err("%s: Can not set the address to el3 monitor. "
				"ret = 0x%x. free the kmem\n", __func__, ret);
		kfree(smc_debug_mem);
	}

	return 0;
}
late_initcall(exynos_set_debug_mem);

static int  __init exynos_get_reason_mem(void)
{
	smc_lockup = kmalloc(PAGE_SIZE, GFP_KERNEL);

	if (!smc_lockup) {
		pr_err("%s: kmalloc for smc_lockup failed.\n", __func__);
		smc_lockup = NULL;
	}

	return 0;
}
arch_initcall(exynos_get_reason_mem);

struct __exception_info {
	unsigned long exception_type;
	unsigned long sp_el1;
	unsigned long sp_el3;
	unsigned long elr_el3;
	unsigned long esr_el3;
};

struct __lockup_info {
	struct __exception_info exception_info[NR_CPUS];
};

static const char *ename[] = {
	"info38961",
	"sync",
	"irq",
	"fiq",
	"async",
	"stack corruption",
	"unknown"
};

static const char *el_mode[] = {
	"el1 mode",
	"el3 mode"
};

#define EXYNOS_EXCEPTION_FROM_SHIFT			(63)

#define EXYNOS_EXCEPTION_FROM_EL3			(1)
#define EXYNOS_EXCEPTION_FROM_EL1			(0)


static int exynos_parse_reason(struct __lockup_info *ptr)
{
	int i, count, ekind, efrom;
	struct __lockup_info *lockup_info = ptr;
	unsigned long etype, elr_el3, sp_el1, sp_el3, esr_el3;

	for(i = 0, count = 0; i < NR_CPUS; i++) {
		etype = lockup_info->exception_info[i].exception_type;

		if (!etype) {
			/* this core has not got stuck in EL3 monitor */
			continue;
		}

		/* add 1 to count for the core got stuck in EL3 monitor */
		count++;

		/* parsing the information */
		ekind = (etype & 0xf) > 6 ? 6 : (etype & 0xf) - 1;
		efrom = (etype >> EXYNOS_EXCEPTION_FROM_SHIFT) & 0x1;
		elr_el3 = lockup_info->exception_info[i].elr_el3;
		sp_el1 = lockup_info->exception_info[i].sp_el1;
		sp_el3 = lockup_info->exception_info[i].sp_el3;
		esr_el3 = lockup_info->exception_info[i].esr_el3;

		/* it got stuck due to unexpected exception */
		pr_emerg("%s: %dth core gets stuck in EL3 monitor due to " \
			"%s exception from %s.\n", \
			 __func__, i, ename[ekind], el_mode[efrom]);
		pr_emerg("%s: elr 0x%lx sp_el1 0x%lx sp_el3 0x%lx " \
			"esr_el3 0x%lx\n", __func__, elr_el3, sp_el1, \
			sp_el3, esr_el3);
	}

	/* count should be more than '1' */
	return !count;
}

int exynos_check_hardlockup_reason(void)
{
	int ret;
	char *phys;

	if (!smc_lockup) {
		pr_err("%s: fail to alloc memory for storing lockup info.\n",
			__func__);
		return 0;
	}

	/* to map & flush memory */
	memset(smc_lockup, 0x00, PAGE_SIZE);
	__flush_dcache_area(smc_lockup, PAGE_SIZE);

	phys = (char *)virt_to_phys(smc_lockup);
	pr_err("%s: smc_lockup virt: 0x%p phys: 0x%p size: %ld.\n",
			__func__, smc_lockup, phys, PAGE_SIZE);

	ret = exynos_smc(SMC_CMD_GET_LOCKUP_REASON, (u64)phys, (u64)PAGE_SIZE, 0);

	if (ret) {
		pr_emerg("%s: SMC_CMD_GET_LOCKUP_REASON returns 0x%x. fail " \
			"to get the information.\n",  __func__, ret);
		goto check_exit;
	}

	ret = exynos_parse_reason((struct __lockup_info *)smc_lockup);

check_exit:
	return ret;
}

static void exynos_smart_exception_handler(unsigned int id,
				unsigned long elr, unsigned long esr,
				unsigned long sctlr, unsigned long ttbr,
				unsigned long tcr, unsigned long x6,
				unsigned int offset)
{
	int i;
	unsigned long *ptr;
	unsigned long tmp;

	pr_err("========================================="
		"=========================================\n");

	if (id)
		pr_err("%s: There has been an unexpected exception from "
			"a LDFW which has smc id 0x%x\n\n", __func__, id);
	else
		pr_err("%s: There has been an unexpected exception from "
			"the EL3 monitor.\n\n", __func__);

	if (id) {
		pr_err("elr_el1   : 0x%016lx, \tesr_el1  : 0x%016lx\n",
								elr, esr);
		pr_err("sctlr_el1 : 0x%016lx, \tttbr_el1 : 0x%016lx\n",
								sctlr, ttbr);
		pr_err("tcr_el1   : 0x%016lx, \tlr (EL1) : 0x%016lx\n\n",
								tcr, x6);
	} else {
		pr_err("elr_el3   : 0x%016lx, \tesr_el3  : 0x%016lx\n",
								elr, esr);
		pr_err("sctlr_el3 : 0x%016lx, \tttbr_el3 : 0x%016lx\n",
								sctlr, ttbr);
		pr_err("tcr_el3   : 0x%016lx, \tscr_el3  : 0x%016lx\n\n",
								tcr, x6);

		if ((offset > 0x0 && offset < (PAGE_SIZE * 2))
				&& !(offset % 0x8) && (smc_debug_mem)) {
			tmp = (unsigned long)smc_debug_mem;
			tmp += (unsigned long)offset;
			ptr = (unsigned long *)tmp;

			for (i = 0; i < 15; i++) {
				pr_err("x%02d : 0x%016lx, \tx%02d : 0x%016lx\n",
					i * 2, ptr[i * 2],
					i * 2 + 1, ptr[i * 2 + 1]);
			}
			pr_err("x%02d : 0x%016lx\n", i * 2,  ptr[i * 2]);
		}
	}

	pr_err("\n[WARNING] IT'S GOING TO CAUSE KERNEL PANIC FOR DEBUGGING.\n\n");

	pr_err("========================================="
		"=========================================\n");
	/* make kernel panic */
	BUG();

	/* SHOULD NOT be here */
	while(1);
}

static int  __init exynos_set_seh_address(void)
{
	int ret;
	unsigned long addr = (unsigned long)exynos_smart_exception_handler;

	pr_info("%s: send smc call with SMC_CMD_SET_SEH_ADDRESS.\n", __func__);

	ret = exynos_smc(SMC_CMD_SET_SEH_ADDRESS, addr, 0, 0);

	/* return value not zero means failure */
	if (ret)
		pr_err("%s: did not set the seh address to el3 monitor. "
				"ret = 0x%x.\n", __func__, ret);
	else
		pr_err("%s: set the seh address to el3 monitor well.\n",
							__func__);

	return 0;
}
arch_initcall(exynos_set_seh_address);

int exynos_pd_tz_save(unsigned int addr)
{
	return exynos_smc(SMC_CMD_PREAPRE_PD_ONOFF,
			EXYNOS_GET_IN_PD_DOWN, addr, 0);
}

int exynos_pd_tz_restore(unsigned int addr)
{
	return exynos_smc(SMC_CMD_PREAPRE_PD_ONOFF,
			EXYNOS_WAKEUP_PD_DOWN, addr, 0);
}
