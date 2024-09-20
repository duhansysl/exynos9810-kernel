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
#include <linux/io.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/kallsyms.h>
#include <linux/input.h>
#include <linux/smc.h>
#include <linux/bitops.h>

#ifdef CONFIG_EXYNOS_SNAPSHOT_CRASH_KEY
#include <linux/gpio_keys.h>
#endif

#include <asm/cputype.h>
#include <asm/cacheflush.h>
#include <asm/smp_plat.h>
#include <asm/core_regs.h>
#include <soc/samsung/exynos-sdm.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/acpm_ipc_ctrl.h>
#if defined(CONFIG_SEC_SIPC_MODEM_IF)
#include <soc/samsung/exynos-modem-ctrl.h>
#endif
#ifdef CONFIG_SEC_DEBUG
#include <linux/sec_debug.h>
#endif /* CONFIG_SEC_DEBUG */
#include "exynos-ss-local.h"

#include <linux/nmi.h>

DEFINE_PER_CPU(struct pt_regs *, ess_core_reg);
DEFINE_PER_CPU(struct exynos_ss_mmu_reg *, ess_mmu_reg);

struct exynos_ss_allcorelockup_param {
	unsigned long last_pc_addr;
	unsigned long spin_pc_addr;
} ess_allcorelockup_param;

void __iomem *exynos_ss_get_base_vaddr(void)
{
	return (void __iomem *)(ess_base.vaddr);
}

void __iomem *exynos_ss_get_base_paddr(void)
{
	return (void __iomem *)(ess_base.paddr);
}

static void exynos_ss_set_core_power_stat(unsigned int val, unsigned int cpu)
{
	if (exynos_ss_get_enable("header"))
		__raw_writel(val, (exynos_ss_get_base_vaddr() +
					ESS_OFFSET_CORE_POWER_STAT + cpu * 4));
}

static unsigned int exynos_ss_get_core_panic_stat(unsigned cpu)
{
	if (exynos_ss_get_enable("header"))
		return __raw_readl(exynos_ss_get_base_vaddr() +
					ESS_OFFSET_PANIC_STAT + cpu * 4);
	else
		return 0;
}

static void exynos_ss_set_core_panic_stat(unsigned int val, unsigned cpu)
{
	if (exynos_ss_get_enable("header"))
		__raw_writel(val, (exynos_ss_get_base_vaddr() +
					ESS_OFFSET_PANIC_STAT + cpu * 4));
}

static void exynos_ss_report_reason(unsigned int val)
{
	if (exynos_ss_get_enable("header"))
		__raw_writel(val, exynos_ss_get_base_vaddr() + ESS_OFFSET_EMERGENCY_REASON);
}

int exynos_ss_get_debug_level_reg(void)
{
	int ret = ESS_DEBUG_LEVEL_MID;

	if (exynos_ss_get_enable("header")) {
		int val = __raw_readl(exynos_ss_get_base_vaddr() + ESS_OFFSET_DEBUG_LEVEL);

		if ((val & GENMASK(31, 16)) == ESS_DEBUG_LEVEL_PREFIX)
			ret = val & GENMASK(15, 0);
	}

	return ret;
}

void exynos_ss_scratch_reg(unsigned int val)
{
	if (exynos_ss_get_enable("header"))
		__raw_writel(val, exynos_ss_get_base_vaddr() + ESS_OFFSET_SCRATCH);
}

unsigned long exynos_ss_get_last_pc_paddr(void)
{
	/*
	 * Basically we want to save the pc value to non-cacheable region
	 * if ESS is enabled. But we should also consider cases that are not so.
	 */

	if (exynos_ss_get_enable("header"))
		return ((unsigned long)exynos_ss_get_base_paddr() + ESS_OFFSET_CORE_LAST_PC);
	else
		return virt_to_phys((void *)ess_desc.hardlockup_core_pc);
}

unsigned long exynos_ss_get_last_pc(unsigned int cpu)
{
	if (exynos_ss_get_enable("header"))
		return __raw_readq(exynos_ss_get_base_vaddr() +
				ESS_OFFSET_CORE_LAST_PC + cpu * 8);
	else
		return ess_desc.hardlockup_core_pc[cpu];
}

unsigned long exynos_ss_get_spare_vaddr(unsigned int offset)
{
	return (unsigned long)(exynos_ss_get_base_vaddr() +
				ESS_OFFSET_SPARE_BASE + offset);
}

unsigned long exynos_ss_get_spare_paddr(unsigned int offset)
{
	unsigned long base_vaddr = 0;
	unsigned long base_paddr = (unsigned long)exynos_ss_get_base_paddr();

	if (base_paddr)
		base_vaddr = (unsigned long)(base_paddr +
				ESS_OFFSET_SPARE_BASE + offset);

	return base_vaddr;
}

unsigned int exynos_ss_get_item_size(char* name)
{
	unsigned long i;

	for (i = 0; i < ess_desc.log_enable_cnt; i++) {
		if (!strncmp(ess_items[i].name, name, strlen(name)))
			return ess_items[i].entry.size;
	}
	return 0;
}
EXPORT_SYMBOL(exynos_ss_get_item_size);

unsigned long exynos_ss_get_item_vaddr(char *name)
{
	unsigned long i;

	for (i = 0; i < ess_desc.log_enable_cnt; i++) {
		if (!strncmp(ess_items[i].name, name, strlen(name)))
			return ess_items[i].entry.vaddr;
	}
	return 0;
}
EXPORT_SYMBOL(exynos_ss_get_item_vaddr);

unsigned int exynos_ss_get_item_paddr(char* name)
{
	unsigned long i;

	for (i = 0; i < ess_desc.log_enable_cnt; i++) {
		if (!strncmp(ess_items[i].name, name, strlen(name)))
			return ess_items[i].entry.paddr;
	}
	return 0;
}
EXPORT_SYMBOL(exynos_ss_get_item_paddr);

int exynos_ss_get_hardlockup(void)
{
	return ess_desc.hardlockup_detected;
}
EXPORT_SYMBOL(exynos_ss_get_hardlockup);

int exynos_ss_set_hardlockup(int val)
{
	unsigned long flags;

	if (unlikely(!ess_base.enabled))
		return 0;

	raw_spin_lock_irqsave(&ess_desc.ctrl_lock, flags);
	ess_desc.hardlockup_detected = val;
	raw_spin_unlock_irqrestore(&ess_desc.ctrl_lock, flags);
	return 0;
}
EXPORT_SYMBOL(exynos_ss_set_hardlockup);

void exynos_ss_hook_hardlockup_entry(void *v_regs)
{
	int cpu = get_current_cpunum();
	unsigned int val;

	if (!ess_base.enabled)
		return;

	if (!ess_desc.hardlockup_core_mask) {
		if (ess_desc.multistage_wdt_irq &&
			!ess_desc.allcorelockup_detected) {
			/* 1st FIQ trigger */
			val = readl(exynos_ss_get_base_vaddr() +
				ESS_OFFSET_CORE_LAST_PC + (ESS_NR_CPUS * sizeof(unsigned long)));
			if (val == ESS_SIGN_LOCKUP || val == (ESS_SIGN_LOCKUP + 1)) {
				ess_desc.allcorelockup_detected = true;
				ess_desc.hardlockup_core_mask = GENMASK(ESS_NR_CPUS - 1, 0);
			} else {
				return;
			}
		}
	}

	/* re-check the cpu number which is lockup */
	if (ess_desc.hardlockup_core_mask & BIT(cpu)) {
		int ret;
		unsigned long last_pc;
		struct pt_regs *regs;
		unsigned long timeout = USEC_PER_SEC * 3;

		do {
			/*
			 * If one cpu is occurred to lockup,
			 * others are going to output its own information
			 * without side-effect.
			 */
			ret = do_raw_spin_trylock(&ess_desc.nmi_lock);
			if (!ret)
				udelay(1);
		} while (!ret && timeout--);

		last_pc = exynos_ss_get_last_pc(cpu);

		regs = (struct pt_regs *)v_regs;

		/* Replace real pc value even if it is invalid */
		regs->pc = last_pc;

		/* Then, we expect bug() function works well */
		pr_emerg("\n--------------------------------------------------------------------------\n"
			"      Debugging Information for Hardlockup core - CPU %d"
			"\n--------------------------------------------------------------------------\n\n", cpu);

#if defined(CONFIG_HARDLOCKUP_DETECTOR_OTHER_CPU)			\
	&& defined(CONFIG_SEC_DEBUG)
		update_hardlockup_type(cpu);
#endif

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
		sec_debug_set_extra_info_backtrace_cpu(v_regs, cpu);
#endif
	}
}

void exynos_ss_hook_hardlockup_exit(void)
{
	int cpu = get_current_cpunum();

	if (!ess_base.enabled ||
		!ess_desc.hardlockup_core_mask ||
		!ess_desc.allcorelockup_detected) {
		return;
	}

	/* re-check the cpu number which is lockup */
	if (ess_desc.hardlockup_core_mask & BIT(cpu)) {
		/* clear bit to complete replace */
		ess_desc.hardlockup_core_mask &= ~(BIT(cpu));
		/*
		 * If this unlock function does not make a side-effect
		 * even it's not lock
		 */
		do_raw_spin_unlock(&ess_desc.nmi_lock);
	}
}

static void exynos_ss_recall_hardlockup_core(void)
{
	int i, ret;
	unsigned long cpu_mask = 0, tmp_bit = 0;
	unsigned long last_pc_addr = 0, timeout;

	if (ess_desc.allcorelockup_detected) {
		pr_emerg("exynos-snapshot: skip recall hardlockup for dump of each core\n");
		goto out;
	}

	for (i = 0; i < ESS_NR_CPUS; i++) {
		if (i == get_current_cpunum())
			continue;
		tmp_bit = cpu_online_mask->bits[ESS_NR_CPUS/SZ_64] & (1 << i);
		if (tmp_bit)
			cpu_mask |= tmp_bit;
	}

	if (!cpu_mask)
		goto out;

	last_pc_addr = exynos_ss_get_last_pc_paddr();

	pr_emerg("exynos-snapshot: core hardlockup mask information: 0x%lx\n", cpu_mask);
	ess_desc.hardlockup_core_mask = cpu_mask;

	/* Setup for generating NMI interrupt to unstopped CPUs */
	ret = exynos_smc(SMC_CMD_KERNEL_PANIC_NOTICE,
			 cpu_mask,
			 (unsigned long)exynos_ss_bug_func,
			 last_pc_addr);
	if (ret) {
		pr_emerg("exynos-snapshot: failed to generate NMI, "
			 "not support to dump information of core\n");
		ess_desc.hardlockup_core_mask = 0;
		goto out;
	}

	/* Wait up to 3 seconds for NMI interrupt */
	timeout = USEC_PER_SEC * 3;
	while (ess_desc.hardlockup_core_mask != 0 && timeout--)
		udelay(1);
out:
	return;
}

int exynos_ss_prepare_panic(void)
{
	unsigned cpu;

	if (unlikely(!ess_base.enabled))
		return 0;
	/*
	 * kick watchdog to prevent unexpected reset during panic sequence
	 * and it prevents the hang during panic sequence by watchedog
	 */
	s3c2410wdt_keepalive_emergency(true, 0);

	/* Again disable log_kevents */
	exynos_ss_set_enable("log_kevents", false);

	for_each_possible_cpu(cpu) {
#ifdef CONFIG_EXYNOS_PMU
		if (exynos_cpu.power_state(cpu))
			exynos_ss_set_core_power_stat(ESS_SIGN_ALIVE, cpu);
		else
			exynos_ss_set_core_power_stat(ESS_SIGN_DEAD, cpu);
#else
		exynos_ss_set_core_power_stat(ESS_SIGN_DEAD, cpu);
#endif
	}
#if defined(CONFIG_SEC_SIPC_MODEM_IF)
	ss310ap_send_panic_noti_ext();
#endif
	acpm_stop_log();

	return 0;
}
EXPORT_SYMBOL(exynos_ss_prepare_panic);

int exynos_ss_post_panic(void)
{
	if (ess_base.enabled) {
		exynos_ss_recall_hardlockup_core();
		exynos_ss_dump_sfr();
		exynos_ss_save_context(NULL);
		flush_cache_all();
		exynos_ss_print_panic_report();
#ifdef CONFIG_EXYNOS_SDM
		if ((__raw_readl(exynos_ss_get_base_vaddr() +
				ESS_OFFSET_SCRATCH) == ESS_SIGN_SCRATCH) &&
			sec_debug_enter_upload())
			exynos_sdm_dump_secure_region();
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_PANIC_REBOOT
		if (!ess_desc.no_wdt_dev) {
#ifdef CONFIG_EXYNOS_SNAPSHOT_WATCHDOG_RESET
			if (ess_desc.hardlockup_detected || num_online_cpus() > 1) {
			/* for stall cpu */
				while(1)
					wfi();
			}
#endif
		}
#endif
	}
#ifdef CONFIG_SEC_DEBUG
	sec_debug_post_panic_handler();
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_PANIC_REBOOT
	arm_pm_restart(0, "panic");
#endif
	goto loop;
	/* for stall cpu when not enabling panic reboot */
loop:
	while(1)
		wfi();

	/* Never run this function */
	pr_emerg("exynos-snapshot: %s DO NOT RUN this function (CPU:%d)\n",
					__func__, raw_smp_processor_id());
	return 0;
}
EXPORT_SYMBOL(exynos_ss_post_panic);

int exynos_ss_dump_panic(char *str, size_t len)
{
	if (unlikely(!ess_base.enabled) ||
		!exynos_ss_get_enable("header"))
		return 0;

	/*  This function is only one which runs in panic funcion */
	if (str && len && len < 1024)
		memcpy(exynos_ss_get_base_vaddr() + ESS_OFFSET_PANIC_STRING, str, len);

	return 0;
}
EXPORT_SYMBOL(exynos_ss_dump_panic);

int exynos_ss_post_reboot(char *cmd)
{
	int cpu;

	if (unlikely(!ess_base.enabled))
		return 0;

	/* clear ESS_SIGN_PANIC when normal reboot */
	for_each_possible_cpu(cpu) {
		exynos_ss_set_core_panic_stat(ESS_SIGN_RESET, cpu);
	}
	exynos_ss_report_reason(ESS_SIGN_NORMAL_REBOOT);
	exynos_ss_scratch_reg(ESS_SIGN_RESET);

	pr_emerg("exynos-snapshot: normal reboot done\n");

	exynos_ss_save_context(NULL);
	flush_cache_all();

	return 0;
}
EXPORT_SYMBOL(exynos_ss_post_reboot);

void exynos_ss_save_system(void *unused)
{
	struct exynos_ss_mmu_reg *mmu_reg;

	if (!exynos_ss_get_enable("header"))
		return;

	mmu_reg = raw_cpu_read(ess_mmu_reg);
#ifdef CONFIG_ARM64
	asm("mrs x1, SCTLR_EL1\n\t"		/* SCTLR_EL1 */
		"str x1, [%0]\n\t"
		"mrs x1, TTBR0_EL1\n\t"		/* TTBR0_EL1 */
		"str x1, [%0,#8]\n\t"
		"mrs x1, TTBR1_EL1\n\t"		/* TTBR1_EL1 */
		"str x1, [%0,#16]\n\t"
		"mrs x1, TCR_EL1\n\t"		/* TCR_EL1 */
		"str x1, [%0,#24]\n\t"
		"mrs x1, ESR_EL1\n\t"		/* ESR_EL1 */
		"str x1, [%0,#32]\n\t"
		"mrs x1, FAR_EL1\n\t"		/* FAR_EL1 */
		"str x1, [%0,#40]\n\t"
		/* Don't populate AFSR0_EL1 and AFSR1_EL1 */
		"mrs x1, CONTEXTIDR_EL1\n\t"	/* CONTEXTIDR_EL1 */
		"str x1, [%0,#48]\n\t"
		"mrs x1, TPIDR_EL0\n\t"		/* TPIDR_EL0 */
		"str x1, [%0,#56]\n\t"
		"mrs x1, TPIDRRO_EL0\n\t"		/* TPIDRRO_EL0 */
		"str x1, [%0,#64]\n\t"
		"mrs x1, TPIDR_EL1\n\t"		/* TPIDR_EL1 */
		"str x1, [%0,#72]\n\t"
		"mrs x1, MAIR_EL1\n\t"		/* MAIR_EL1 */
		"str x1, [%0,#80]\n\t"
		"mrs x1, ELR_EL1\n\t"		/* ELR_EL1 */
		"str x1, [%0, #88]\n\t"
		"mrs x1, SP_EL0\n\t"		/* SP_EL0 */
		"str x1, [%0, #96]\n\t" :	/* output */
		: "r"(mmu_reg)			/* input */
		: "%x1", "memory"			/* clobbered register */
	);
#else
	asm("mrc    p15, 0, r1, c1, c0, 0\n\t"	/* SCTLR */
	    "str r1, [%0]\n\t"
	    "mrc    p15, 0, r1, c2, c0, 0\n\t"	/* TTBR0 */
	    "str r1, [%0,#4]\n\t"
	    "mrc    p15, 0, r1, c2, c0,1\n\t"	/* TTBR1 */
	    "str r1, [%0,#8]\n\t"
	    "mrc    p15, 0, r1, c2, c0,2\n\t"	/* TTBCR */
	    "str r1, [%0,#12]\n\t"
	    "mrc    p15, 0, r1, c3, c0,0\n\t"	/* DACR */
	    "str r1, [%0,#16]\n\t"
	    "mrc    p15, 0, r1, c5, c0,0\n\t"	/* DFSR */
	    "str r1, [%0,#20]\n\t"
	    "mrc    p15, 0, r1, c6, c0,0\n\t"	/* DFAR */
	    "str r1, [%0,#24]\n\t"
	    "mrc    p15, 0, r1, c5, c0,1\n\t"	/* IFSR */
	    "str r1, [%0,#28]\n\t"
	    "mrc    p15, 0, r1, c6, c0,2\n\t"	/* IFAR */
	    "str r1, [%0,#32]\n\t"
	    /* Don't populate DAFSR and RAFSR */
	    "mrc    p15, 0, r1, c10, c2,0\n\t"	/* PMRRR */
	    "str r1, [%0,#44]\n\t"
	    "mrc    p15, 0, r1, c10, c2,1\n\t"	/* NMRRR */
	    "str r1, [%0,#48]\n\t"
	    "mrc    p15, 0, r1, c13, c0,0\n\t"	/* FCSEPID */
	    "str r1, [%0,#52]\n\t"
	    "mrc    p15, 0, r1, c13, c0,1\n\t"	/* CONTEXT */
	    "str r1, [%0,#56]\n\t"
	    "mrc    p15, 0, r1, c13, c0,2\n\t"	/* URWTPID */
	    "str r1, [%0,#60]\n\t"
	    "mrc    p15, 0, r1, c13, c0,3\n\t"	/* UROTPID */
	    "str r1, [%0,#64]\n\t"
	    "mrc    p15, 0, r1, c13, c0,4\n\t"	/* POTPIDR */
	    "str r1, [%0,#68]\n\t" :		/* output */
	    : "r"(mmu_reg)			/* input */
	    : "%r1", "memory"			/* clobbered register */
	);
#endif
}

int exynos_ss_dump(void)
{
	/*
	 *  Output CPU Memory Error syndrome Register
	 *  CPUMERRSR, L2MERRSR
	 */
#ifdef CONFIG_ARM64
	unsigned long reg1, reg2, reg3;

	if (read_cpuid_implementor() == ARM_CPU_IMP_SEC) {
		switch (read_cpuid_part_number()) {
		case ARM_CPU_PART_MONGOOSE:
		case ARM_CPU_PART_MEERKAT:
			asm ("mrs %0, S3_1_c15_c2_0\n\t"
					"mrs %1, S3_1_c15_c2_4\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("FEMERR0SR: %016lx, FEMERR1SR: %016lx\n", reg1, reg2);
			asm ("mrs %0, S3_1_c15_c2_1\n\t"
					"mrs %1, S3_1_c15_c2_5\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("LSMERR0SR: %016lx, LSMERR1SR: %016lx\n", reg1, reg2);
			asm ("mrs %0, S3_1_c15_c2_2\n\t"
					"mrs %1, S3_1_c15_c2_6\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("TBWMERR0SR: %016lx, TBWMERR1SR: %016lx\n", reg1, reg2);
			asm ("mrs %0, S3_1_c15_c2_3\n\t"
					"mrs %1, S3_1_c15_c2_7\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("L2MERR0SR: %016lx, L2MERR1SR: %016lx\n", reg1, reg2);

			/* L3 MERR */
			asm ("msr S3_1_c15_c7_1, %0\n\t"
					"isb\n"
					:: "r" (0));
			asm ("mrs %0, S3_1_c15_c3_0\n\t"
					"mrs %1, S3_1_c15_c3_4\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("BANK0 L3MERR0SR: %016lx, L3MERR1SR: %016lx\n", reg1, reg2);
			asm ("msr S3_1_c15_c7_1, %0\n\t"
					"isb\n"
					:: "r" (1));
			asm ("mrs %0, S3_1_c15_c3_0\n\t"
					"mrs %1, S3_1_c15_c3_4\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("BANK1 L3MERR0SR: %016lx, L3MERR1SR: %016lx\n", reg1, reg2);
			asm ("msr S3_1_c15_c7_1, %0\n\t"
					"isb\n"
					:: "r" (2));
			asm ("mrs %0, S3_1_c15_c3_0\n\t"
					"mrs %1, S3_1_c15_c3_4\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("BANK2 L3MERR0SR: %016lx, L3MERR1SR: %016lx\n", reg1, reg2);
			asm ("msr S3_1_c15_c7_1, %0\n\t"
					"isb\n"
					:: "r" (3));
			asm ("mrs %0, S3_1_c15_c3_0\n\t"
					"mrs %1, S3_1_c15_c3_4\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("BANK3 L3MERR0SR: %016lx, L3MERR1SR: %016lx\n", reg1, reg2);

			break;
		default:
			break;
		}
	} else {
		switch (read_cpuid_part_number()) {
		case ARM_CPU_PART_CORTEX_A57:
		case ARM_CPU_PART_CORTEX_A53:
			asm ("mrs %0, S3_1_c15_c2_2\n\t"
					"mrs %1, S3_1_c15_c2_3\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("CPUMERRSR: %016lx, L2MERRSR: %016lx\n", reg1, reg2);
			break;
		case ARM_CPU_PART_ANANKE:
			asm ("HINT #16");
			asm ("mrs %0, S3_0_c12_c1_1\n" : "=r" (reg1)); /* read DISR_EL1 */
			pr_emerg("DISR_EL1: %016lx\n", reg1);

			asm ("msr S3_0_c5_c3_1, %0\n"  :: "r" (0)); /* set 1st ERRSELR_EL1 */

			asm ("mrs %0, S3_0_c5_c4_2\n"
					"mrs %1, S3_0_c5_c4_3\n"
					"mrs %2, S3_0_c5_c5_0\n"
					: "=r" (reg1), "=r" (reg2), "=r" (reg3));
			pr_emerg("1st : ERXSTATUS_EL1: %016lx, ERXADDR_EL1: %016lx, "
					"ERXMISC0_EL1: %016lx\n", reg1, reg2, reg3);

			asm ("msr S3_0_c5_c3_1, %0\n"  :: "r" (1)); /* set 2nd ERRSELR_EL1 */

			asm ("mrs %0, S3_0_c5_c4_2\n"
					"mrs %1, S3_0_c5_c4_3\n"
					"mrs %2, S3_0_c5_c5_0\n"
					: "=r" (reg1), "=r" (reg2), "=r" (reg3));
			pr_emerg("2nd : ERXSTATUS_EL1: %016lx, ERXADDR_EL1: %016lx, "
					"ERXMISC0_EL1: %016lx\n", reg1, reg2, reg3);

			break;
		default:
			break;
		}
	}
#else
	unsigned long reg0;
	asm ("mrc p15, 0, %0, c0, c0, 0\n": "=r" (reg0));
	if (((reg0 >> 4) & 0xFFF) == 0xC0F) {
		/*  Only Cortex-A15 */
		unsigned long reg1, reg2, reg3;
		asm ("mrrc p15, 0, %0, %1, c15\n\t"
			"mrrc p15, 1, %2, %3, c15\n"
			: "=r" (reg0), "=r" (reg1),
			"=r" (reg2), "=r" (reg3));
		pr_emerg("CPUMERRSR: %08lx_%08lx, L2MERRSR: %08lx_%08lx\n",
				reg1, reg0, reg3, reg2);
	}
#endif
	return 0;
}
EXPORT_SYMBOL(exynos_ss_dump);

static int exynos_ss_save_core_safe(struct pt_regs *regs, int cpu)
{
	struct pt_regs *core_reg = per_cpu(ess_core_reg, cpu);

	if (!exynos_ss_get_enable("header"))
		return 0;

	memcpy(core_reg, regs, sizeof(struct pt_regs));

	return 0;
}

int exynos_ss_save_core(void *v_regs)
{
	struct pt_regs *regs = (struct pt_regs *)v_regs;
	struct pt_regs *core_reg =
			per_cpu(ess_core_reg, smp_processor_id());

	if (!exynos_ss_get_enable("header"))
		return 0;

	if (!regs) {
		asm("str x0, [%0, #0]\n\t"
		    "mov x0, %0\n\t"
		    "str x1, [x0, #8]\n\t"
		    "str x2, [x0, #16]\n\t"
		    "str x3, [x0, #24]\n\t"
		    "str x4, [x0, #32]\n\t"
		    "str x5, [x0, #40]\n\t"
		    "str x6, [x0, #48]\n\t"
		    "str x7, [x0, #56]\n\t"
		    "str x8, [x0, #64]\n\t"
		    "str x9, [x0, #72]\n\t"
		    "str x10, [x0, #80]\n\t"
		    "str x11, [x0, #88]\n\t"
		    "str x12, [x0, #96]\n\t"
		    "str x13, [x0, #104]\n\t"
		    "str x14, [x0, #112]\n\t"
		    "str x15, [x0, #120]\n\t"
		    "str x16, [x0, #128]\n\t"
		    "str x17, [x0, #136]\n\t"
		    "str x18, [x0, #144]\n\t"
		    "str x19, [x0, #152]\n\t"
		    "str x20, [x0, #160]\n\t"
		    "str x21, [x0, #168]\n\t"
		    "str x22, [x0, #176]\n\t"
		    "str x23, [x0, #184]\n\t"
		    "str x24, [x0, #192]\n\t"
		    "str x25, [x0, #200]\n\t"
		    "str x26, [x0, #208]\n\t"
		    "str x27, [x0, #216]\n\t"
		    "str x28, [x0, #224]\n\t"
		    "str x29, [x0, #232]\n\t"
		    "str x30, [x0, #240]\n\t" :
		    : "r"(core_reg));
		core_reg->sp = (unsigned long)__builtin_frame_address(1);
		core_reg->regs[29] = core_reg->sp;
		core_reg->pc =
			(unsigned long)(core_reg->regs[30] - sizeof(unsigned int));
	} else {
		memcpy(core_reg, regs, sizeof(struct pt_regs));
	}

	pr_emerg("exynos-snapshot: core register saved(CPU:%d)\n",
						smp_processor_id());
	return 0;
}
EXPORT_SYMBOL(exynos_ss_save_core);

int exynos_ss_save_context(void *v_regs)
{
	int cpu;
	unsigned long flags;
	struct pt_regs *regs = (struct pt_regs *)v_regs;

	if (unlikely(!ess_base.enabled))
		return 0;
#ifdef CONFIG_EXYNOS_CORESIGHT_ETR
	exynos_trace_stop();
#endif
	cpu = smp_processor_id();
	raw_spin_lock_irqsave(&ess_desc.ctrl_lock, flags);

	/* If it was already saved the context information, it should be skipped */
	if (exynos_ss_get_core_panic_stat(cpu) !=  ESS_SIGN_PANIC) {
		exynos_ss_save_system(NULL);
		exynos_ss_save_core(regs);
		exynos_ss_dump();
		exynos_ss_set_core_panic_stat(ESS_SIGN_PANIC, cpu);
		pr_emerg("exynos-snapshot: context saved(CPU:%d)\n", cpu);
	} else
		pr_emerg("exynos-snapshot: skip context saved(CPU:%d)\n", cpu);

	flush_cache_all();
	raw_spin_unlock_irqrestore(&ess_desc.ctrl_lock, flags);
	return 0;
}
EXPORT_SYMBOL(exynos_ss_save_context);

static void exynos_ss_dump_one_task_info(struct task_struct *tsk, bool is_main)
{
	char state_array[] = {'R', 'S', 'D', 'T', 't', 'Z', 'X', 'x', 'K', 'W'};
	unsigned char idx = 0;
	unsigned long state = (tsk->state & TASK_REPORT) | tsk->exit_state;
	unsigned long wchan;
	unsigned long pc = 0;
	char symname[KSYM_NAME_LEN];

	if (!try_get_task_stack(tsk))
		return;
	pc = KSTK_EIP(tsk);
	wchan = get_wchan(tsk);
	if (lookup_symbol_name(wchan, symname) < 0)
		snprintf(symname, KSYM_NAME_LEN, "%lu", wchan);

	while (state) {
		idx++;
		state >>= 1;
	}

	/*
	 * kick watchdog to prevent unexpected reset during panic sequence
	 * and it prevents the hang during panic sequence by watchedog
	 */
	touch_softlockup_watchdog();
	s3c2410wdt_keepalive_emergency(false, 0);

	pr_info("%8d %8d %8d %16lld %c(%d) %3d  %16zx %16zx  %16zx %c %16s [%s]\n",
			tsk->pid, (int)(tsk->utime), (int)(tsk->stime),
			tsk->se.exec_start, state_array[idx], (int)(tsk->state),
			task_cpu(tsk), wchan, pc, (unsigned long)tsk,
			is_main ? '*' : ' ', tsk->comm, symname);

	if (tsk->state == TASK_RUNNING
			|| tsk->state == TASK_UNINTERRUPTIBLE
			|| tsk->mm == NULL) {
		print_worker_info(KERN_INFO, tsk);
		show_stack(tsk, NULL);
		pr_info("\n");
	}
}

static inline struct task_struct *get_next_thread(struct task_struct *tsk)
{
	return container_of(tsk->thread_group.next,
				struct task_struct,
				thread_group);
}

static void exynos_ss_dump_task_info(void)
{
	struct task_struct *frst_tsk;
	struct task_struct *curr_tsk;
	struct task_struct *frst_thr;
	struct task_struct *curr_thr;

	pr_info("\n");
	pr_info(" current proc : %d %s\n", current->pid, current->comm);
	pr_info(" ----------------------------------------------------------------------------------------------------------------------------\n");
	pr_info("     pid      uTime    sTime      exec(ns)  stat  cpu       wchan           user_pc        task_struct       comm   sym_wchan\n");
	pr_info(" ----------------------------------------------------------------------------------------------------------------------------\n");

	/* processes */
	frst_tsk = &init_task;
	curr_tsk = frst_tsk;
	while (curr_tsk != NULL) {
		exynos_ss_dump_one_task_info(curr_tsk,  true);
		/* threads */
		if (curr_tsk->thread_group.next != NULL) {
			frst_thr = get_next_thread(curr_tsk);
			curr_thr = frst_thr;
			if (frst_thr != curr_tsk) {
				while (curr_thr != NULL) {
					exynos_ss_dump_one_task_info(curr_thr, false);
					curr_thr = get_next_thread(curr_thr);
					if (curr_thr == curr_tsk)
						break;
				}
			}
		}
		curr_tsk = container_of(curr_tsk->tasks.next,
					struct task_struct, tasks);
		if (curr_tsk == frst_tsk)
			break;
	}
	pr_info(" ----------------------------------------------------------------------------------------------------------------------------\n");
}

#ifdef CONFIG_EXYNOS_SNAPSHOT_CRASH_KEY
void exynos_ss_check_crash_key(unsigned int code, int value)
{
	static bool volup_p;
	static bool voldown_p;
	static int loopcount;

	static const unsigned int VOLUME_UP = KEY_VOLUMEUP;
	static const unsigned int VOLUME_DOWN = KEY_VOLUMEDOWN;


	if (code == KEY_POWER)
		pr_info("exynos-snapshot: POWER-KEY %s\n", value ? "pressed" : "released");

	/* Enter Forced Upload
	 *  Hold volume down key first
	 *  and then press power key twice
	 *  and volume up key should not be pressed
	 */
	if (value) {
		if (code == VOLUME_UP)
			volup_p = true;
		if (code == VOLUME_DOWN)
			voldown_p = true;
		if (!volup_p && voldown_p) {
			if (code == KEY_POWER) {
				pr_info
				    ("exynos-snapshot: count for entering forced upload [%d]\n",
				     ++loopcount);
				if (loopcount == 2) {
					panic("Crash Key");
				}
			}
		}
	} else {
		if (code == VOLUME_UP) {
			volup_p = false;
		}
		if (code == VOLUME_DOWN) {
			loopcount = 0;
			voldown_p = false;
		}
	}
}
EXPORT_SYMBOL(exynos_ss_check_crash_key);
#endif

static int exynos_ss_reboot_handler(struct notifier_block *nb,
				    unsigned long l, void *p)
{
	if (unlikely(!ess_base.enabled))
		return 0;

	pr_emerg("exynos-snapshot: normal reboot starting\n");

#ifdef CONFIG_SEC_DEBUG
	sec_debug_reboot_handler(p);
#endif
	return 0;
}

static int exynos_ss_panic_handler(struct notifier_block *nb,
				   unsigned long l, void *buf)
{
	exynos_ss_report_reason(ESS_SIGN_PANIC);
	if (unlikely(!ess_base.enabled))
		return 0;

#ifdef CONFIG_EXYNOS_SNAPSHOT_PANIC_REBOOT
	local_irq_disable();
	pr_emerg("exynos-snapshot: panic - reboot[%s]\n", __func__);
#else
	pr_emerg("exynos-snapshot: panic - normal[%s]\n", __func__);
#endif
	exynos_ss_dump_task_info();
	pr_emerg("linux_banner: %s\n", linux_banner);
	flush_cache_all();
#ifdef CONFIG_SEC_DEBUG
	sec_debug_panic_handler(buf, true);
#endif
	return 0;
}

static struct notifier_block nb_reboot_block = {
	.notifier_call = exynos_ss_reboot_handler
};

static struct notifier_block nb_panic_block = {
	.notifier_call = exynos_ss_panic_handler,
};

void exynos_ss_panic_handler_safe(struct pt_regs *regs)
{
	char *cpu_num[SZ_16] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
	char text[SZ_32] = "safe panic handler at cpu ";
	int cpu = get_current_cpunum();
	size_t len;

	if (unlikely(!ess_base.enabled))
		return;

	strncat(text, cpu_num[cpu], 1);
	len = strnlen(text, SZ_32);

	exynos_ss_report_reason(ESS_SIGN_SAFE_FAULT);
	exynos_ss_save_system(NULL);
	exynos_ss_save_core_safe(regs, cpu);
	exynos_ss_set_core_panic_stat(ESS_SIGN_PANIC, cpu);
	flush_cache_all();
	exynos_ss_dump_panic(text, len);
	s3c2410wdt_set_emergency_reset(100, 0);
}

void __init exynos_ss_allcorelockup_detector_init(void)
{
	int ret;

	if (!ess_desc.multistage_wdt_irq)
		return;

	ess_allcorelockup_param.last_pc_addr = exynos_ss_get_last_pc_paddr();
	ess_allcorelockup_param.spin_pc_addr = virt_to_phys(exynos_ss_spin_func);

	__flush_dcache_area((void *)&ess_allcorelockup_param,
			sizeof(struct exynos_ss_allcorelockup_param));

	/* Setup for generating NMI interrupt to unstopped CPUs */
	ret = exynos_smc(SMC_CMD_LOCKUP_NOTICE,
			 (unsigned long)exynos_ss_bug_func,
			 ess_desc.multistage_wdt_irq,
			 (unsigned long)(virt_to_phys)(&ess_allcorelockup_param));

	pr_emerg("exynos-snapshot: %s to register all-core lockup detector - ret: %d\n",
			ret == 0 ? "success" : "failed", ret);
}

void __init exynos_ss_utils_init(void)
{
	size_t vaddr;
	int i;

	vaddr = ess_items[ess_desc.header_num].entry.vaddr;

	for (i = 0; i < ESS_NR_CPUS; i++) {
		per_cpu(ess_mmu_reg, i) = (struct exynos_ss_mmu_reg *)
					  (vaddr + ESS_HEADER_SZ +
					   i * ESS_MMU_REG_OFFSET);
		per_cpu(ess_core_reg, i) = (struct pt_regs *)
					   (vaddr + ESS_HEADER_SZ + ESS_MMU_REG_SZ +
					    i * ESS_CORE_REG_OFFSET);
	}

	register_reboot_notifier(&nb_reboot_block);
	atomic_notifier_chain_register(&panic_notifier_list, &nb_panic_block);

	/* hardlockup_detector function should be called before secondary booting */
	exynos_ss_allcorelockup_detector_init();
}

static int __init exynos_ss_utils_save_systems_all(void)
{
	smp_call_function(exynos_ss_save_system, NULL, 1);
	exynos_ss_save_system(NULL);

	return 0;
}
postcore_initcall(exynos_ss_utils_save_systems_all);
