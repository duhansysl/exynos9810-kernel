/*
 * Copyright (c) 2013-2018 Samsung Electronics Co., Ltd.
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
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/kallsyms.h>
#include <linux/platform_device.h>
#include <linux/clk-provider.h>
#include <linux/pstore_ram.h>
#ifdef CONFIG_SEC_EXT
#include <linux/sec_ext.h>
#endif

#include "exynos-ss-local.h"
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/hardirq.h>
#include <asm/stacktrace.h>
#include <linux/exynos-ss.h>
#include <linux/kernel_stat.h>
#include <linux/irqnr.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>

#include <linux/nmi.h>

#include <linux/sec_debug.h>

struct exynos_ss_lastinfo {
#ifdef CONFIG_EXYNOS_SNAPSHOT_FREQ
	atomic_t freq_last_idx[ESS_FLAG_END];
#endif
	char log[ESS_NR_CPUS][SZ_1K];
	char *last_p[ESS_NR_CPUS];
};

struct ess_dumper {
	bool active;
	u32 items;
	int init_idx;
	int cur_idx;
	u32 cur_cpu;
	u32 step;
};

enum ess_kevent_flag {
	ESS_FLAG_TASK = 1,
	ESS_FLAG_WORK,
	ESS_FLAG_CPUIDLE,
	ESS_FLAG_SUSPEND,
	ESS_FLAG_IRQ,
	ESS_FLAG_IRQ_EXIT,
	ESS_FLAG_SPINLOCK,
	ESS_FLAG_IRQ_DISABLE,
	ESS_FLAG_CLK,
	ESS_FLAG_FREQ,
	ESS_FLAG_REG,
	ESS_FLAG_HRTIMER,
	ESS_FLAG_REGULATOR,
	ESS_FLAG_THERMAL,
	ESS_FLAG_MAILBOX,
	ESS_FLAG_CLOCKEVENT,
	ESS_FLAG_PRINTK,
	ESS_FLAG_PRINTKL,
	ESS_FLAG_KEVENT,
};

struct exynos_ss_log_idx {
	atomic_t task_log_idx[ESS_NR_CPUS];
	atomic_t work_log_idx[ESS_NR_CPUS];
	atomic_t cpuidle_log_idx[ESS_NR_CPUS];
	atomic_t suspend_log_idx;
	atomic_t irq_log_idx[ESS_NR_CPUS];
#ifdef CONFIG_EXYNOS_SNAPSHOT_SPINLOCK
	atomic_t spinlock_log_idx[ESS_NR_CPUS];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
	atomic_t irqs_disabled_log_idx[ESS_NR_CPUS];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
	atomic_t irq_exit_log_idx[ESS_NR_CPUS];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
	atomic_t reg_log_idx[ESS_NR_CPUS];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_HRTIMER
	atomic_t hrtimer_log_idx[ESS_NR_CPUS];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
	atomic_t clk_log_idx;
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_PMU
	atomic_t pmu_log_idx;
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_FREQ
	atomic_t freq_log_idx;
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_DM
	atomic_t dm_log_idx;
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REGULATOR
	atomic_t regulator_log_idx;
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REGULATOR
	atomic_t thermal_log_idx;
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_UART
	atomic_t uart_log_idx;
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_I2C
	atomic_t i2c_log_idx;
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_SPI
	atomic_t spi_log_idx;
#endif
#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
	atomic_t clockevent_log_idx[ESS_NR_CPUS];
	atomic_t printkl_log_idx;
	atomic_t printk_log_idx;
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_ACPM
	atomic_t acpm_log_idx;
#endif
};

int exynos_ss_log_size = sizeof(struct exynos_ss_log);
/*
 *  including or excluding options
 *  if you want to except some interrupt, it should be written in this array
 */
static int ess_irqlog_exlist[] = {
/*  interrupt number ex) 152, 153, 154, */
	-1,
};

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
static int ess_irqexit_exlist[] = {
/*  interrupt number ex) 152, 153, 154, */
	-1,
};

static unsigned ess_irqexit_threshold =
		CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT_THRESHOLD;
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
struct ess_reg_list {
	size_t addr;
	size_t size;
};

static struct ess_reg_list ess_reg_exlist[] = {
/*
 *  if it wants to reduce effect enabled reg feautre to system,
 *  you must add these registers - mct, serial
 *  because they are called very often.
 *  physical address, size ex) {0x10C00000, 0x1000},
 */
	{ESS_REG_MCT_ADDR, ESS_REG_MCT_SIZE},
	{ESS_REG_UART_ADDR, ESS_REG_UART_SIZE},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
	{0, 0},
};
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_FREQ
static char *ess_freq_name[] = {
	"LITTLE", "BIG", "INT", "MIF", "ISP", "DISP", "INTCAM", "AUD", "IVA", "SCORE", "FSYS0",
};
#endif

/*  Internal interface variable */
static struct exynos_ss_log_idx ess_idx;
static struct exynos_ss_lastinfo ess_lastinfo;

void __init exynos_ss_log_idx_init(void)
{
	int i;

#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
	atomic_set(&(ess_idx.printk_log_idx), -1);
	atomic_set(&(ess_idx.printkl_log_idx), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REGULATOR
	atomic_set(&(ess_idx.regulator_log_idx), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_THERMAL
	atomic_set(&(ess_idx.thermal_log_idx), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_FREQ
	atomic_set(&(ess_idx.freq_log_idx), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_DM
	atomic_set(&(ess_idx.dm_log_idx), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
	atomic_set(&(ess_idx.clk_log_idx), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_PMU
	atomic_set(&(ess_idx.pmu_log_idx), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_ACPM
	atomic_set(&(ess_idx.acpm_log_idx), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_UART
	atomic_set(&(ess_idx.uart_log_idx), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_I2C
	atomic_set(&(ess_idx.i2c_log_idx), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_SPI
	atomic_set(&(ess_idx.spi_log_idx), -1);
#endif
	atomic_set(&(ess_idx.suspend_log_idx), -1);

	for (i = 0; i < ESS_NR_CPUS; i++) {
		atomic_set(&(ess_idx.task_log_idx[i]), -1);
		atomic_set(&(ess_idx.work_log_idx[i]), -1);
#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
		atomic_set(&(ess_idx.clockevent_log_idx[i]), -1);
#endif
		atomic_set(&(ess_idx.cpuidle_log_idx[i]), -1);
		atomic_set(&(ess_idx.irq_log_idx[i]), -1);
#ifdef CONFIG_EXYNOS_SNAPSHOT_SPINLOCK
		atomic_set(&(ess_idx.spinlock_log_idx[i]), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
		atomic_set(&(ess_idx.irqs_disabled_log_idx[i]), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
		atomic_set(&(ess_idx.irq_exit_log_idx[i]), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
		atomic_set(&(ess_idx.reg_log_idx[i]), -1);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_HRTIMER
		atomic_set(&(ess_idx.hrtimer_log_idx[i]), -1);
#endif
	}
}

unsigned long sec_debug_get_kevent_index_addr(int type)
{
	switch (type) {
	case DSS_KEVENT_TASK:
		return virt_to_phys(&(ess_idx.task_log_idx[0]));

	case DSS_KEVENT_WORK:
		return virt_to_phys(&(ess_idx.work_log_idx[0]));

	case DSS_KEVENT_IRQ:
		return virt_to_phys(&(ess_idx.irq_log_idx[0]));

	case DSS_KEVENT_FREQ:
		return virt_to_phys(&(ess_idx.freq_log_idx));

	case DSS_KEVENT_IDLE:
		return virt_to_phys(&(ess_idx.cpuidle_log_idx[0]));

	case DSS_KEVENT_THRM:
		return virt_to_phys(&(ess_idx.thermal_log_idx));

	case DSS_KEVENT_ACPM:
		return virt_to_phys(&(ess_idx.acpm_log_idx));

	default:
		return 0;
	}
}

bool exynos_ss_dumper_one(void *v_dumper, char *line, size_t size, size_t *len)
{
	bool ret = false;
	int idx, array_size;
	unsigned int cpu, items;
	unsigned long rem_nsec;
	u64 ts;
	struct ess_dumper *dumper = (struct ess_dumper *)v_dumper;

	if (!line || size < SZ_128 ||
		dumper->cur_cpu >= NR_CPUS)
		goto out;

	if (dumper->active) {
		if (dumper->init_idx == dumper->cur_idx)
			goto out;
	}

	cpu = dumper->cur_cpu;
	idx = dumper->cur_idx;
	items = dumper->items;

	switch(items) {
	case ESS_FLAG_TASK:
	{
		struct task_struct *task;
		array_size = ARRAY_SIZE(ess_log->task[0]) - 1;
		if (!dumper->active) {
			idx = (atomic_read(&ess_idx.task_log_idx[0]) + 1) & array_size;
			dumper->init_idx = idx;
			dumper->active = true;
		}
		ts = ess_log->task[cpu][idx].time;
		rem_nsec = do_div(ts, NSEC_PER_SEC);
		task = ess_log->task[cpu][idx].task;

		*len = snprintf(line, size, "[%8lu.%09lu][%04d:CPU%u] task_name:%16s,  "
					    "task:0x%16p,  stack:0x%16p,  exec_start:%16llu\n",
						(unsigned long)ts, rem_nsec / NSEC_PER_USEC, idx, cpu,
						task->comm, task, task->stack,
						task->se.exec_start);
		break;
	}
	case ESS_FLAG_WORK:
	{
		char work_fn[KSYM_NAME_LEN] = {0,};
		char *task_comm;
		int en;

		array_size = ARRAY_SIZE(ess_log->work[0]) - 1;
		if (!dumper->active) {
			idx = (atomic_read(&ess_idx.work_log_idx[0]) + 1) & array_size;
			dumper->init_idx = idx;
			dumper->active = true;
		}
		ts = ess_log->work[cpu][idx].time;
		rem_nsec = do_div(ts, NSEC_PER_SEC);
		lookup_symbol_name((unsigned long)ess_log->work[cpu][idx].fn, work_fn);
		task_comm = ess_log->work[cpu][idx].task_comm;
		en = ess_log->work[cpu][idx].en;

		dumper->step = 6;
		*len = snprintf(line, size, "[%8lu.%09lu][%04d:CPU%u] task_name:%16s,  work_fn:%32s,  %3s\n",
						(unsigned long)ts, rem_nsec / NSEC_PER_USEC, idx, cpu,
						task_comm, work_fn,
						en == ESS_FLAG_IN ? "IN" : "OUT");
		break;
	}
	case ESS_FLAG_CPUIDLE:
	{
		unsigned int delta;
		int state, num_cpus, en;
		char *index;

		array_size = ARRAY_SIZE(ess_log->cpuidle[0]) - 1;
		if (!dumper->active) {
			idx = (atomic_read(&ess_idx.cpuidle_log_idx[0]) + 1) & array_size;
			dumper->init_idx = idx;
			dumper->active = true;
		}
		ts = ess_log->cpuidle[cpu][idx].time;
		rem_nsec = do_div(ts, NSEC_PER_SEC);

		index = ess_log->cpuidle[cpu][idx].modes;
		en = ess_log->cpuidle[cpu][idx].en;
		state = ess_log->cpuidle[cpu][idx].state;
		num_cpus = ess_log->cpuidle[cpu][idx].num_online_cpus;
		delta = ess_log->cpuidle[cpu][idx].delta;

		*len = snprintf(line, size, "[%8lu.%09lu][%04d:CPU%u] cpuidle: %s,  "
					    "state:%d,  num_online_cpus:%d,  stay_time:%8u,  %3s\n",
						(unsigned long)ts, rem_nsec / NSEC_PER_USEC, idx, cpu,
						index, state, num_cpus, delta,
						en == ESS_FLAG_IN ? "IN" : "OUT");
		break;
	}
	case ESS_FLAG_SUSPEND:
	{
		char suspend_fn[KSYM_NAME_LEN];
		int en;

		array_size = ARRAY_SIZE(ess_log->suspend) - 1;
		if (!dumper->active) {
			idx = (atomic_read(&ess_idx.suspend_log_idx) + 1) & array_size;
			dumper->init_idx = idx;
			dumper->active = true;
		}
		ts = ess_log->suspend[idx].time;
		rem_nsec = do_div(ts, NSEC_PER_SEC);

		lookup_symbol_name((unsigned long)ess_log->suspend[idx].fn, suspend_fn);
		en = ess_log->suspend[idx].en;

		*len = snprintf(line, size, "[%8lu.%09lu][%04d:CPU%u] suspend_fn:%s,  %3s\n",
						(unsigned long)ts, rem_nsec / NSEC_PER_USEC, idx, cpu,
						suspend_fn, en == ESS_FLAG_IN ? "IN" : "OUT");
		break;
	}
	case ESS_FLAG_IRQ:
	{
		char irq_fn[KSYM_NAME_LEN];
		int en, irq, preempt, val;

		array_size = ARRAY_SIZE(ess_log->irq[0]) - 1;
		if (!dumper->active) {
			idx = (atomic_read(&ess_idx.irq_log_idx[0]) + 1) & array_size;
			dumper->init_idx = idx;
			dumper->active = true;
		}
		ts = ess_log->irq[cpu][idx].time;
		rem_nsec = do_div(ts, NSEC_PER_SEC);

		lookup_symbol_name((unsigned long)ess_log->irq[cpu][idx].fn, irq_fn);
		irq = ess_log->irq[cpu][idx].irq;
		preempt = ess_log->irq[cpu][idx].preempt;
		val = ess_log->irq[cpu][idx].val;
		en = ess_log->irq[cpu][idx].en;

		*len = snprintf(line, size, "[%8lu.%09lu][%04d:CPU%u] irq:%6d,  irq_fn:%32s,  "
					    "preempt:%6d,  val:%6d,  %3s\n",
						(unsigned long)ts, rem_nsec / NSEC_PER_USEC, idx, cpu,
						irq, irq_fn, preempt, val,
						en == ESS_FLAG_IN ? "IN" : "OUT");
		break;
	}
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
	case ESS_FLAG_IRQ_EXIT:
	{
		unsigned long end_time, latency;
		int irq;

		array_size = ARRAY_SIZE(ess_log->irq_exit[0]) - 1;
		if (!dumper->active) {
			idx = (atomic_read(&ess_idx.irq_exit_log_idx[0]) + 1) & array_size;
			dumper->init_idx = idx;
			dumper->active = true;
		}
		ts = ess_log->irq_exit[cpu][idx].time;
		rem_nsec = do_div(ts, NSEC_PER_SEC);

		end_time = ess_log->irq_exit[cpu][idx].end_time;
		latency = ess_log->irq_exit[cpu][idx].latency;
		irq = ess_log->irq_exit[cpu][idx].irq;

		*len = snprintf(line, size, "[%8lu.%09lu][%04d:CPU%u] irq:%6d,  "
					    "latency:%16zu,  end_time:%16zu\n",
						(unsigned long)ts, rem_nsec / NSEC_PER_USEC, idx, cpu,
						irq, latency, end_time);
		break;
	}
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_SPINLOCK
	case ESS_FLAG_SPINLOCK:
	{
		unsigned int jiffies_local;
		char callstack[CONFIG_EXYNOS_SNAPSHOT_CALLSTACK][KSYM_NAME_LEN];
		int en, i;
		u16 next, owner;

		array_size = ARRAY_SIZE(ess_log->spinlock[0]) - 1;
		if (!dumper->active) {
			idx = (atomic_read(&ess_idx.spinlock_log_idx[0]) + 1) & array_size;
			dumper->init_idx = idx;
			dumper->active = true;
		}
		ts = ess_log->spinlock[cpu][idx].time;
		rem_nsec = do_div(ts, NSEC_PER_SEC);

		jiffies_local = ess_log->spinlock[cpu][idx].jiffies;
		en = ess_log->spinlock[cpu][idx].en;
		for (i = 0; i < CONFIG_EXYNOS_SNAPSHOT_CALLSTACK; i++)
			lookup_symbol_name((unsigned long)ess_log->spinlock[cpu][idx].caller[i],
						callstack[i]);

		next = ess_log->spinlock[cpu][idx].next;
		owner = ess_log->spinlock[cpu][idx].owner;

		*len = snprintf(line, size, "[%8lu.%09lu][%04d:CPU%u] next:%8x,  owner:%8x  jiffies:%12u,  %3s\n"
					    "callstack: %s\n"
					    "           %s\n"
					    "           %s\n"
					    "           %s\n",
						(unsigned long)ts, rem_nsec / NSEC_PER_USEC, idx, cpu,
						next, owner, jiffies_local,
						en == ESS_FLAG_IN ? "IN" : "OUT",
						callstack[0], callstack[1], callstack[2], callstack[3]);
		break;
	}
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
	case ESS_FLAG_CLK:
	{
		const char *clk_name;
		char clk_fn[KSYM_NAME_LEN];
		struct clk_hw *clk;
		int en;

		array_size = ARRAY_SIZE(ess_log->clk) - 1;
		if (!dumper->active) {
			idx = (atomic_read(&ess_idx.clk_log_idx) + 1) & array_size;
			dumper->init_idx = idx;
			dumper->active = true;
		}
		ts = ess_log->clk[idx].time;
		rem_nsec = do_div(ts, NSEC_PER_SEC);

		clk = (struct clk_hw *)ess_log->clk[idx].clk;
		clk_name = clk_hw_get_name(clk);
		lookup_symbol_name((unsigned long)ess_log->clk[idx].f_name, clk_fn);
		en = ess_log->clk[idx].mode;

		*len = snprintf(line, size, "[%8lu.%09lu][%04d:CPU] clk_name:%30s,  clk_fn:%30s,  "
					    ",  %s\n",
						(unsigned long)ts, rem_nsec / NSEC_PER_USEC, idx,
						clk_name, clk_fn, en == ESS_FLAG_IN ? "IN" : "OUT");
		break;
	}
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_FREQ
	case ESS_FLAG_FREQ:
	{
		char *freq_name;
		unsigned int on_cpu;
		unsigned long old_freq, target_freq;
		int en;

		array_size = ARRAY_SIZE(ess_log->freq) - 1;
		if (!dumper->active) {
			idx = (atomic_read(&ess_idx.freq_log_idx) + 1) & array_size;
			dumper->init_idx = idx;
			dumper->active = true;
		}
		ts = ess_log->freq[idx].time;
		rem_nsec = do_div(ts, NSEC_PER_SEC);

		freq_name = ess_log->freq[idx].freq_name;
		old_freq = ess_log->freq[idx].old_freq;
		target_freq = ess_log->freq[idx].target_freq;
		on_cpu = ess_log->freq[idx].cpu;
		en = ess_log->freq[idx].en;

		*len = snprintf(line, size, "[%8lu.%09lu][%04d:CPU%u] freq_name:%16s,  "
					    "old_freq:%16lu,  target_freq:%16lu,  %3s\n",
						(unsigned long)ts, rem_nsec / NSEC_PER_USEC, idx, on_cpu,
						freq_name, old_freq, target_freq,
						en == ESS_FLAG_IN ? "IN" : "OUT");
		break;
	}
#endif
	case ESS_FLAG_PRINTK:
	{
		char *log;
		char callstack[CONFIG_EXYNOS_SNAPSHOT_CALLSTACK][KSYM_NAME_LEN];
		unsigned int cpu;
		int i;

		array_size = ARRAY_SIZE(ess_log->printk) - 1;
		if (!dumper->active) {
			idx = (atomic_read(&ess_idx.printk_log_idx) + 1) & array_size;
			dumper->init_idx = idx;
			dumper->active = true;
		}
		ts = ess_log->printk[idx].time;
		cpu = ess_log->printk[idx].cpu;
		rem_nsec = do_div(ts, NSEC_PER_SEC);
		log = ess_log->printk[idx].log;
		for (i = 0; i < CONFIG_EXYNOS_SNAPSHOT_CALLSTACK; i++)
			lookup_symbol_name((unsigned long)ess_log->printk[idx].caller[i],
						callstack[i]);

		*len = snprintf(line, size, "[%8lu.%09lu][%04d:CPU%u] log:%s, callstack:%s, %s, %s, %s\n",
						(unsigned long)ts, rem_nsec / NSEC_PER_USEC, idx, cpu,
						log, callstack[0], callstack[1], callstack[2], callstack[3]);
		break;
	}
	case ESS_FLAG_PRINTKL:
	{
		char callstack[CONFIG_EXYNOS_SNAPSHOT_CALLSTACK][KSYM_NAME_LEN];
		size_t msg, val;
		unsigned int cpu;
		int i;

		array_size = ARRAY_SIZE(ess_log->printkl) - 1;
		if (!dumper->active) {
			idx = (atomic_read(&ess_idx.printkl_log_idx) + 1) & array_size;
			dumper->init_idx = idx;
			dumper->active = true;
		}
		ts = ess_log->printkl[idx].time;
		cpu = ess_log->printkl[idx].cpu;
		rem_nsec = do_div(ts, NSEC_PER_SEC);
		msg = ess_log->printkl[idx].msg;
		val = ess_log->printkl[idx].val;
		for (i = 0; i < CONFIG_EXYNOS_SNAPSHOT_CALLSTACK; i++)
			lookup_symbol_name((unsigned long)ess_log->printkl[idx].caller[i],
						callstack[i]);

		*len = snprintf(line, size, "[%8lu.%09lu][%04d:CPU%u] msg:%zx, val:%zx, callstack: %s, %s, %s, %s\n",
						(unsigned long)ts, rem_nsec / NSEC_PER_USEC, idx, cpu,
						msg, val, callstack[0], callstack[1], callstack[2], callstack[3]);
		break;
	}
	default:
		snprintf(line, size, "unsupported inforation to dump\n");
		goto out;
	}
	if (array_size == idx)
		dumper->cur_idx = 0;
	else
		dumper->cur_idx = idx + 1;

	ret = true;
out:
	return ret;
}

#ifdef CONFIG_ARM64
static inline unsigned long pure_arch_local_irq_save(void)
{
	unsigned long flags;

	asm volatile(
		"mrs	%0, daif		// arch_local_irq_save\n"
		"msr	daifset, #2"
		: "=r" (flags)
		:
		: "memory");

	return flags;
}

static inline void pure_arch_local_irq_restore(unsigned long flags)
{
	asm volatile(
		"msr    daif, %0                // arch_local_irq_restore"
		:
		: "r" (flags)
		: "memory");
}
#else
static inline unsigned long arch_local_irq_save(void)
{
	unsigned long flags;

	asm volatile(
		"	mrs	%0, cpsr	@ arch_local_irq_save\n"
		"	cpsid	i"
		: "=r" (flags) : : "memory", "cc");
	return flags;
}

static inline void arch_local_irq_restore(unsigned long flags)
{
	asm volatile(
		"	msr	cpsr_c, %0	@ local_irq_restore"
		:
		: "r" (flags)
		: "memory", "cc");
}
#endif

void exynos_ss_task(int cpu, void *v_task)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		unsigned long i = atomic_inc_return(&ess_idx.task_log_idx[cpu]) &
				    (ARRAY_SIZE(ess_log->task[0]) - 1);

		ess_log->task[cpu][i].time = cpu_clock(cpu);
		ess_log->task[cpu][i].sp = (unsigned long) current_stack_pointer;
		ess_log->task[cpu][i].task = (struct task_struct *)v_task;
		strncpy(ess_log->task[cpu][i].task_comm,
			ess_log->task[cpu][i].task->comm,
			TASK_COMM_LEN - 1);
	}
}

void exynos_ss_work(void *worker, void *v_task, void *fn, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.work_log_idx[cpu]) &
					(ARRAY_SIZE(ess_log->work[0]) - 1);
		struct task_struct *task = (struct task_struct *)v_task;
		ess_log->work[cpu][i].time = cpu_clock(cpu);
		ess_log->work[cpu][i].sp = (unsigned long) current_stack_pointer;
		ess_log->work[cpu][i].worker = (struct worker *)worker;
		strncpy(ess_log->work[cpu][i].task_comm, task->comm, TASK_COMM_LEN - 1);
		ess_log->work[cpu][i].fn = (work_func_t)fn;
		ess_log->work[cpu][i].en = en;
	}
}

void exynos_ss_cpuidle(char *modes, unsigned state, int diff, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.cpuidle_log_idx[cpu]) &
				(ARRAY_SIZE(ess_log->cpuidle[0]) - 1);

		ess_log->cpuidle[cpu][i].time = cpu_clock(cpu);
		ess_log->cpuidle[cpu][i].modes = modes;
		ess_log->cpuidle[cpu][i].state = state;
		ess_log->cpuidle[cpu][i].sp = (unsigned long) current_stack_pointer;
		ess_log->cpuidle[cpu][i].num_online_cpus = num_online_cpus();
		ess_log->cpuidle[cpu][i].delta = diff;
		ess_log->cpuidle[cpu][i].en = en;
	}
}

void exynos_ss_suspend(void *fn, void *dev, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.suspend_log_idx) &
				(ARRAY_SIZE(ess_log->suspend) - 1);

		ess_log->suspend[i].time = cpu_clock(cpu);
		ess_log->suspend[i].sp = (unsigned long) current_stack_pointer;
		ess_log->suspend[i].fn = fn;
		ess_log->suspend[i].dev = (struct device *)dev;
		ess_log->suspend[i].core = cpu;
		ess_log->suspend[i].en = en;
	}
}

static void exynos_ss_print_calltrace(void)
{
	int i;

	pr_info("\n<Call trace>\n");
	for (i = 0; i < ESS_NR_CPUS; i++) {
		pr_info("CPU ID: %d -----------------------------------------------\n", i);
		pr_info("%s", ess_lastinfo.log[i]);
	}
}

void exynos_ss_save_log(int cpu, unsigned long where)
{
	if (ess_lastinfo.last_p[cpu] == NULL)
		ess_lastinfo.last_p[cpu] = &ess_lastinfo.log[cpu][0];

	if (ess_lastinfo.last_p[cpu] > &ess_lastinfo.log[cpu][SZ_1K - SZ_128])
		return;

	*(unsigned long *)&(ess_lastinfo.last_p[cpu]) += sprintf(ess_lastinfo.last_p[cpu],
			"[<%p>] %pS\n", (void *)where, (void *)where);

}

static void exynos_ss_get_sec(unsigned long long ts, unsigned long *sec, unsigned long *msec)
{
	*sec = ts / NSEC_PER_SEC;
	*msec = (ts % NSEC_PER_SEC) / USEC_PER_MSEC;
}

static void exynos_ss_print_last_irq(int cpu)
{
	unsigned long idx, sec, msec;
	char fn_name[KSYM_NAME_LEN];

	idx = atomic_read(&ess_idx.irq_log_idx[cpu]) & (ARRAY_SIZE(ess_log->irq[0]) - 1);
	exynos_ss_get_sec(ess_log->irq[cpu][idx].time, &sec, &msec);
	lookup_symbol_name((unsigned long)ess_log->irq[cpu][idx].fn, fn_name);

	pr_info("%-16s: [%4lu] %10lu.%06lu sec, %10s: %24s, %8s: %8d, %10s: %2d, %s\n",
			">>> last irq", idx, sec, msec,
			"handler", fn_name,
			"irq", ess_log->irq[cpu][idx].irq,
			"en", ess_log->irq[cpu][idx].en,
			(ess_log->irq[cpu][idx].en == 1) ? "[Missmatch]" : "");
}

static void exynos_ss_print_last_task(int cpu)
{
	unsigned long idx, sec, msec;
	struct task_struct *task;

	idx = atomic_read(&ess_idx.task_log_idx[cpu]) & (ARRAY_SIZE(ess_log->task[0]) - 1);
	exynos_ss_get_sec(ess_log->task[cpu][idx].time, &sec, &msec);
	task = ess_log->task[cpu][idx].task;

	pr_info("%-16s: [%4lu] %10lu.%06lu sec, %10s: %24s, %8s: 0x%-16p, %10s: %16llu\n",
			">>> last task", idx, sec, msec,
			"task_comm", (task) ? task->comm : "NULL",
			"task", task,
			"exec_start", (task) ? task->se.exec_start : 0);
}

static void exynos_ss_print_last_work(int cpu)
{
	unsigned long idx, sec, msec;
	char fn_name[KSYM_NAME_LEN];

	idx = atomic_read(&ess_idx.work_log_idx[cpu]) & (ARRAY_SIZE(ess_log->work[0]) - 1);
	exynos_ss_get_sec(ess_log->work[cpu][idx].time, &sec, &msec);
	lookup_symbol_name((unsigned long)ess_log->work[cpu][idx].fn, fn_name);

	pr_info("%-16s: [%4lu] %10lu.%06lu sec, %10s: %24s, %8s: %20s, %3s: %3d %s\n",
			">>> last work", idx, sec, msec,
			"task_name", ess_log->work[cpu][idx].task_comm,
			"work_fn", fn_name,
			"en", ess_log->work[cpu][idx].en,
			(ess_log->work[cpu][idx].en == 1) ? "[Missmatch]" : "");
}

static void exynos_ss_print_last_cpuidle(int cpu)
{
	unsigned long idx, sec, msec;

	idx = atomic_read(&ess_idx.cpuidle_log_idx[cpu]) & (ARRAY_SIZE(ess_log->cpuidle[0]) - 1);
	exynos_ss_get_sec(ess_log->cpuidle[cpu][idx].time, &sec, &msec);

	pr_info("%-16s: [%4lu] %10lu.%06lu sec, %10s: %24d, %8s: %4s, %6s: %3d, %12s: %2d, %3s: %3d %s\n",
			">>> last cpuidle", idx, sec, msec,
			"stay time", ess_log->cpuidle[cpu][idx].delta,
			"modes", ess_log->cpuidle[cpu][idx].modes,
			"state", ess_log->cpuidle[cpu][idx].state,
			"online_cpus", ess_log->cpuidle[cpu][idx].num_online_cpus,
			"en", ess_log->cpuidle[cpu][idx].en,
			(ess_log->cpuidle[cpu][idx].en == 1) ? "[Missmatch]" : "");
}

static void exynos_ss_print_lastinfo(void)
{
	int cpu;

	pr_info("<last info>\n");
	for (cpu = 0; cpu < ESS_NR_CPUS; cpu++) {
		pr_info("CPU ID: %d -----------------------------------------------\n", cpu);
		exynos_ss_print_last_task(cpu);
		exynos_ss_print_last_work(cpu);
		exynos_ss_print_last_irq(cpu);
		exynos_ss_print_last_cpuidle(cpu);
	}
}

#ifdef CONFIG_EXYNOS_SNAPSHOT_REGULATOR
void exynos_ss_regulator(unsigned long long timestamp, char* f_name, unsigned int addr, unsigned int volt, unsigned int rvolt, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.regulator_log_idx) &
				(ARRAY_SIZE(ess_log->regulator) - 1);
		int size = strlen(f_name);
		if (size >= SZ_16)
			size = SZ_16 - 1;
		ess_log->regulator[i].time = cpu_clock(cpu);
		ess_log->regulator[i].cpu = cpu;
		ess_log->regulator[i].acpm_time = timestamp;
		strncpy(ess_log->regulator[i].name, f_name, size);
		ess_log->regulator[i].reg = addr;
		ess_log->regulator[i].en = en;
		ess_log->regulator[i].voltage = volt;
		ess_log->regulator[i].raw_volt = rvolt;
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_THERMAL
void exynos_ss_thermal(void *data, unsigned int temp, char *name, unsigned int max_cooling)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.thermal_log_idx) &
				(ARRAY_SIZE(ess_log->thermal) - 1);

		ess_log->thermal[i].time = cpu_clock(cpu);
		ess_log->thermal[i].cpu = cpu;
		ess_log->thermal[i].data = (struct exynos_tmu_platform_data *)data;
		ess_log->thermal[i].temp = temp;
		ess_log->thermal[i].cooling_device = name;
		ess_log->thermal[i].cooling_state = max_cooling;
	}
}
#endif

void exynos_ss_irq(int irq, void *fn, unsigned int val, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];
	unsigned long flags;

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	flags = pure_arch_local_irq_save();
	{
		int cpu = get_current_cpunum();
		unsigned long i;

		for (i = 0; i < ARRAY_SIZE(ess_irqlog_exlist); i++) {
			if (irq == ess_irqlog_exlist[i]) {
				pure_arch_local_irq_restore(flags);
				return;
			}
		}
		i = atomic_inc_return(&ess_idx.irq_log_idx[cpu]) &
				(ARRAY_SIZE(ess_log->irq[0]) - 1);

		ess_log->irq[cpu][i].time = cpu_clock(cpu);
		ess_log->irq[cpu][i].sp = (unsigned long) current_stack_pointer;
		ess_log->irq[cpu][i].irq = irq;
		ess_log->irq[cpu][i].fn = (void *)fn;
		ess_log->irq[cpu][i].preempt = preempt_count();
		ess_log->irq[cpu][i].val = val;
		ess_log->irq[cpu][i].en = en;
	}
	pure_arch_local_irq_restore(flags);
}

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
void exynos_ss_irq_exit(unsigned int irq, unsigned long long start_time)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];
	unsigned long i;

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	for (i = 0; i < ARRAY_SIZE(ess_irqexit_exlist); i++)
		if (irq == ess_irqexit_exlist[i])
			return;
	{
		int cpu = get_current_cpunum();
		unsigned long long time, latency;

		i = atomic_inc_return(&ess_idx.irq_exit_log_idx[cpu]) &
			(ARRAY_SIZE(ess_log->irq_exit[0]) - 1);

		time = cpu_clock(cpu);
		latency = time - start_time;

		if (unlikely(latency >
			(ess_irqexit_threshold * 1000))) {
			ess_log->irq_exit[cpu][i].latency = latency;
			ess_log->irq_exit[cpu][i].sp = (unsigned long) current_stack_pointer;
			ess_log->irq_exit[cpu][i].end_time = time;
			ess_log->irq_exit[cpu][i].time = start_time;
			ess_log->irq_exit[cpu][i].irq = irq;
		} else
			atomic_dec(&ess_idx.irq_exit_log_idx[cpu]);
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_SPINLOCK
void exynos_ss_spinlock(void *v_lock, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned index = atomic_inc_return(&ess_idx.spinlock_log_idx[cpu]);
		unsigned long j, i = index & (ARRAY_SIZE(ess_log->spinlock[0]) - 1);
		raw_spinlock_t *lock = (raw_spinlock_t *)v_lock;
#ifdef CONFIG_ARM_ARCH_TIMER
		ess_log->spinlock[cpu][i].time = cpu_clock(cpu);
#else
		ess_log->spinlock[cpu][i].time = index;
#endif
		ess_log->spinlock[cpu][i].sp = (unsigned long) current_stack_pointer;
		ess_log->spinlock[cpu][i].jiffies = jiffies_64;
		ess_log->spinlock[cpu][i].lock = lock;
		ess_log->spinlock[cpu][i].next = lock->raw_lock.next;
		ess_log->spinlock[cpu][i].owner = lock->raw_lock.owner;
		ess_log->spinlock[cpu][i].en = en;

		for (j = 0; j < ess_desc.callstack; j++) {
			ess_log->spinlock[cpu][i].caller[j] =
				(void *)((size_t)return_address(j + 1));
		}
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
void exynos_ss_irqs_disabled(unsigned long flags)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];
	int cpu = get_current_cpunum();

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	if (unlikely(flags)) {
		unsigned j, local_flags = pure_arch_local_irq_save();

		/* If flags has one, it shows interrupt enable status */
		atomic_set(&ess_idx.irqs_disabled_log_idx[cpu], -1);
		ess_log->irqs_disabled[cpu][0].time = 0;
		ess_log->irqs_disabled[cpu][0].index = 0;
		ess_log->irqs_disabled[cpu][0].task = NULL;
		ess_log->irqs_disabled[cpu][0].task_comm = NULL;

		for (j = 0; j < ess_desc.callstack; j++) {
			ess_log->irqs_disabled[cpu][0].caller[j] = NULL;
		}

		pure_arch_local_irq_restore(local_flags);
	} else {
		unsigned index = atomic_inc_return(&ess_idx.irqs_disabled_log_idx[cpu]);
		unsigned long j, i = index % ARRAY_SIZE(ess_log->irqs_disabled[0]);

		ess_log->irqs_disabled[cpu][0].time = jiffies_64;
		ess_log->irqs_disabled[cpu][i].index = index;
		ess_log->irqs_disabled[cpu][i].task = get_current();
		ess_log->irqs_disabled[cpu][i].task_comm = get_current()->comm;

		for (j = 0; j < ess_desc.callstack; j++) {
			ess_log->irqs_disabled[cpu][i].caller[j] =
				(void *)((size_t)return_address(j + 1));
		}
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
void exynos_ss_clk(void *clock, const char *func_name, unsigned long arg, int mode)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.clk_log_idx) &
				(ARRAY_SIZE(ess_log->clk) - 1);

		ess_log->clk[i].time = cpu_clock(cpu);
		ess_log->clk[i].mode = mode;
		ess_log->clk[i].arg = arg;
		ess_log->clk[i].clk = (struct clk_hw *)clock;
		ess_log->clk[i].f_name = func_name;
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_PMU
void exynos_ss_pmu(int id, const char *func_name, int mode)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.pmu_log_idx) &
				(ARRAY_SIZE(ess_log->pmu) - 1);

		ess_log->pmu[i].time = cpu_clock(cpu);
		ess_log->pmu[i].mode = mode;
		ess_log->pmu[i].id = id;
		ess_log->pmu[i].f_name = func_name;
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_FREQ
static void exynos_ss_print_freqinfo(void)
{
	unsigned long idx, sec, msec;
	char *freq_name;
	unsigned int i;
	unsigned long old_freq, target_freq;

	pr_info("\n<freq info>\n");

	for (i = 0; i < ESS_FLAG_END; i++) {
		idx = atomic_read(&ess_lastinfo.freq_last_idx[i]) & (ARRAY_SIZE(ess_log->freq) - 1);
		freq_name = ess_log->freq[idx].freq_name;
		if (strncmp(freq_name, ess_freq_name[i], strlen(ess_freq_name[i]))) {
			pr_info("%10s: no infomation\n", ess_freq_name[i]);
			continue;
		}

		exynos_ss_get_sec(ess_log->freq[idx].time, &sec, &msec);
		old_freq = ess_log->freq[idx].old_freq;
		target_freq = ess_log->freq[idx].target_freq;
		pr_info("%10s: [%4lu] %10lu.%06lu sec, %12s: %6luMhz, %12s: %6luMhz, %3s: %3d %s\n",
					freq_name, idx, sec, msec,
					"old_freq", old_freq/1000,
					"target_freq", target_freq/1000,
					"en", ess_log->freq[idx].en,
					(ess_log->freq[idx].en == 1) ? "[Missmatch]" : "");
	}
}

void exynos_ss_freq(int type, unsigned long old_freq, unsigned long target_freq, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.freq_log_idx) &
				(ARRAY_SIZE(ess_log->freq) - 1);

		if (atomic_read(&ess_idx.freq_log_idx) > atomic_read(&ess_lastinfo.freq_last_idx[type]))
			atomic_set(&ess_lastinfo.freq_last_idx[type], atomic_read(&ess_idx.freq_log_idx));

		ess_log->freq[i].time = cpu_clock(cpu);
		ess_log->freq[i].cpu = cpu;
		ess_log->freq[i].freq_name = ess_freq_name[type];
		ess_log->freq[i].old_freq = old_freq;
		ess_log->freq[i].target_freq = target_freq;
		ess_log->freq[i].en = en;
	}
}
#endif

#ifndef arch_irq_stat
#define arch_irq_stat() 0
#endif

static void exynos_ss_print_irq(void)
{
	int i, j;
	u64 sum = 0;

	for_each_possible_cpu(i) {
		sum += kstat_cpu_irqs_sum(i);
		sum += arch_irq_stat_cpu(i);
	}
	sum += arch_irq_stat();

	pr_info("\n<irq info>\n");
	pr_info("------------------------------------------------------------------\n");
	pr_info("\n");
	pr_info("sum irq : %llu", (unsigned long long)sum);
	pr_info("------------------------------------------------------------------\n");

	for_each_irq_nr(j) {
		unsigned int irq_stat = kstat_irqs(j);

		if (irq_stat) {
			struct irq_desc *desc = irq_to_desc(j);
			const char *name;

			name = desc->action ? (desc->action->name ? desc->action->name : "???") : "???";
			pr_info("irq-%-4d : %8u %s\n", j, irq_stat, name);
		}
	}
}

void exynos_ss_print_panic_report(void)
{
	pr_info("============================================================\n");
	pr_info("Panic Report\n");
	pr_info("============================================================\n");
	exynos_ss_print_lastinfo();
	exynos_ss_print_freqinfo();
	exynos_ss_print_calltrace();
	exynos_ss_print_irq();
	pr_info("============================================================\n");
}

#ifdef CONFIG_EXYNOS_SNAPSHOT_DM
void exynos_ss_dm(int type, unsigned long min, unsigned long max, s32 wait_t, s32 t)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = raw_smp_processor_id();
		unsigned long i = atomic_inc_return(&ess_idx.dm_log_idx) &
				(ARRAY_SIZE(ess_log->dm) - 1);

		ess_log->dm[i].time = cpu_clock(cpu);
		ess_log->dm[i].cpu = cpu;
		ess_log->dm[i].dm_num = type;
		ess_log->dm[i].min_freq = min;
		ess_log->dm[i].max_freq = max;
		ess_log->dm[i].wait_dmt = wait_t;
		ess_log->dm[i].do_dmt = t;
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_HRTIMER
void exynos_ss_hrtimer(void *timer, s64 *now, void *fn, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long i = atomic_inc_return(&ess_idx.hrtimer_log_idx[cpu]) &
				(ARRAY_SIZE(ess_log->hrtimers[0]) - 1);

		ess_log->hrtimers[cpu][i].time = cpu_clock(cpu);
		ess_log->hrtimers[cpu][i].now = *now;
		ess_log->hrtimers[cpu][i].timer = (struct hrtimer *)timer;
		ess_log->hrtimers[cpu][i].fn = fn;
		ess_log->hrtimers[cpu][i].en = en;
	}
}
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_UART
void exynos_ss_uart(struct s3c24xx_uart_port *ourport, int flag, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = raw_smp_processor_id();
		unsigned long i = atomic_inc_return(&ess_idx.uart_log_idx) &
			(ARRAY_SIZE(ess_log->uart) - 1);

		ess_log->uart[i].time = cpu_clock(cpu);
		ess_log->uart[i].cpu = cpu;
		ess_log->uart[i].ourport = ourport;
		ess_log->uart[i].flag = flag;
		ess_log->uart[i].en = en;
	}
}
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_I2C
void exynos_ss_i2c(struct i2c_adapter *adap, struct i2c_msg *msgs, int num, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = raw_smp_processor_id();
		unsigned long i = atomic_inc_return(&ess_idx.i2c_log_idx) &
				(ARRAY_SIZE(ess_log->i2c) - 1);

		ess_log->i2c[i].time = cpu_clock(cpu);
		ess_log->i2c[i].cpu = cpu;
		ess_log->i2c[i].adap = adap;
		ess_log->i2c[i].msgs = msgs;
		ess_log->i2c[i].num = num;
		ess_log->i2c[i].en = en;
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_SPI
void exynos_ss_spi(struct spi_master *master, struct spi_message *cur_msg, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = raw_smp_processor_id();
		unsigned long i = atomic_inc_return(&ess_idx.spi_log_idx) &
				(ARRAY_SIZE(ess_log->spi) - 1);

		ess_log->spi[i].time = cpu_clock(cpu);
		ess_log->spi[i].cpu = cpu;
		ess_log->spi[i].master = master;
		ess_log->spi[i].cur_msg = cur_msg;
		ess_log->spi[i].en = en;
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_ACPM
void exynos_ss_acpm(unsigned long long timestamp, const char *log, unsigned int data)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = raw_smp_processor_id();
		unsigned long i = atomic_inc_return(&ess_idx.acpm_log_idx) &
				(ARRAY_SIZE(ess_log->acpm) - 1);
		int len = strlen(log);

		if (len >= 9)
			len = 9;

		ess_log->acpm[i].time = cpu_clock(cpu);
		ess_log->acpm[i].acpm_time = timestamp;
		strncpy(ess_log->acpm[i].log, log, len);
		ess_log->acpm[i].log[8] = '\0';
		ess_log->acpm[i].data = data;
	}
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
static phys_addr_t virt_to_phys_high(size_t vaddr)
{
	phys_addr_t paddr = 0;
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;

	if (virt_addr_valid((void *) vaddr)) {
		paddr = virt_to_phys((void *) vaddr);
		goto out;
	}

	pgd = pgd_offset_k(vaddr);
	if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
		goto out;

	if (pgd_val(*pgd) & 2) {
		paddr = pgd_val(*pgd) & SECTION_MASK;
		goto out;
	}

	pmd = pmd_offset((pud_t *)pgd, vaddr);
	if (pmd_none_or_clear_bad(pmd))
		goto out;

	pte = pte_offset_kernel(pmd, vaddr);
	if (pte_none(*pte))
		goto out;

	paddr = pte_val(*pte) & PAGE_MASK;

out:
	return paddr | (vaddr & UL(SZ_4K - 1));
}

void exynos_ss_reg(unsigned int read, size_t val, size_t reg, int en)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];
	int cpu = get_current_cpunum();
	unsigned long i, j;
	size_t phys_reg, start_addr, end_addr;

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;

	if (ess_reg_exlist[0].addr == 0)
		return;

	phys_reg = virt_to_phys_high(reg);
	if (unlikely(!phys_reg))
		return;

	for (j = 0; j < ARRAY_SIZE(ess_reg_exlist); j++) {
		if (ess_reg_exlist[j].addr == 0)
			break;
		start_addr = ess_reg_exlist[j].addr;
		end_addr = start_addr + ess_reg_exlist[j].size;
		if (start_addr <= phys_reg && phys_reg <= end_addr)
			return;
	}

	i = atomic_inc_return(&ess_idx.reg_log_idx[cpu]) &
		(ARRAY_SIZE(ess_log->reg[0]) - 1);

	ess_log->reg[cpu][i].time = cpu_clock(cpu);
	ess_log->reg[cpu][i].read = read;
	ess_log->reg[cpu][i].val = val;
	ess_log->reg[cpu][i].reg = phys_reg;
	ess_log->reg[cpu][i].en = en;

	for (j = 0; j < ess_desc.callstack; j++) {
		ess_log->reg[cpu][i].caller[j] =
			(void *)((size_t)return_address(j + 1));
	}
}
#endif

#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
void exynos_ss_clockevent(unsigned long long clc, int64_t delta, void *next_event)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long j, i = atomic_inc_return(&ess_idx.clockevent_log_idx[cpu]) &
				(ARRAY_SIZE(ess_log->clockevent[0]) - 1);

		ess_log->clockevent[cpu][i].time = cpu_clock(cpu);
		ess_log->clockevent[cpu][i].mct_cycle = clc;
		ess_log->clockevent[cpu][i].delta_ns = delta;
		ess_log->clockevent[cpu][i].next_event = *((ktime_t *)next_event);

		for (j = 0; j < ess_desc.callstack; j++) {
			ess_log->clockevent[cpu][i].caller[j] =
				(void *)((size_t)return_address(j + 1));
		}
	}
}

void exynos_ss_printk(const char *fmt, ...)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = get_current_cpunum();
		va_list args;
		int ret;
		unsigned long j, i = atomic_inc_return(&ess_idx.printk_log_idx) &
				(ARRAY_SIZE(ess_log->printk) - 1);

		va_start(args, fmt);
		ret = vsnprintf(ess_log->printk[i].log,
				sizeof(ess_log->printk[i].log), fmt, args);
		va_end(args);

		ess_log->printk[i].time = cpu_clock(cpu);
		ess_log->printk[i].cpu = cpu;

		for (j = 0; j < ess_desc.callstack; j++) {
			ess_log->printk[i].caller[j] =
				(void *)((size_t)return_address(j));
		}
	}
}

void exynos_ss_printkl(size_t msg, size_t val)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return;
	{
		int cpu = get_current_cpunum();
		unsigned long j, i = atomic_inc_return(&ess_idx.printkl_log_idx) &
				(ARRAY_SIZE(ess_log->printkl) - 1);

		ess_log->printkl[i].time = cpu_clock(cpu);
		ess_log->printkl[i].cpu = cpu;
		ess_log->printkl[i].msg = msg;
		ess_log->printkl[i].val = val;

		for (j = 0; j < ess_desc.callstack; j++) {
			ess_log->printkl[i].caller[j] =
				(void *)((size_t)return_address(j));
		}
	}
}
#endif

/* This defines are for PSTORE */
#define ESS_LOGGER_LEVEL_HEADER 	(1)
#define ESS_LOGGER_LEVEL_PREFIX 	(2)
#define ESS_LOGGER_LEVEL_TEXT		(3)
#define ESS_LOGGER_LEVEL_MAX		(4)
#define ESS_LOGGER_SKIP_COUNT		(4)
#define ESS_LOGGER_STRING_PAD		(1)
#define ESS_LOGGER_HEADER_SIZE		(68)

#define ESS_LOG_ID_MAIN 		(0)
#define ESS_LOG_ID_RADIO		(1)
#define ESS_LOG_ID_EVENTS		(2)
#define ESS_LOG_ID_SYSTEM		(3)
#define ESS_LOG_ID_CRASH		(4)
#define ESS_LOG_ID_KERNEL		(5)

typedef struct __attribute__((__packed__)) {
	uint8_t magic;
	uint16_t len;
	uint16_t uid;
	uint16_t pid;
} ess_pmsg_log_header_t;

typedef struct __attribute__((__packed__)) {
	unsigned char id;
	uint16_t tid;
	int32_t tv_sec;
	int32_t tv_nsec;
} ess_android_log_header_t;

typedef struct ess_logger {
	uint16_t	len;
	uint16_t	id;
	uint16_t	pid;
	uint16_t	tid;
	uint16_t	uid;
	uint16_t	level;
	int32_t		tv_sec;
	int32_t		tv_nsec;
	char		msg[0];
	char*		buffer;
	void		(*func_hook_logger)(const char*, const char*, size_t);
} __attribute__((__packed__)) ess_logger;

static ess_logger logger;

void register_hook_logger(void (*func)(const char *name, const char *buf, size_t size))
{
	logger.func_hook_logger = func;
	logger.buffer = vmalloc(PAGE_SIZE * 3);

	if (logger.buffer)
		pr_info("exynos-snapshot: logger buffer alloc address: 0x%p\n", logger.buffer);
}
EXPORT_SYMBOL(register_hook_logger);

static int exynos_ss_combine_pmsg(char *buffer, size_t count, unsigned int level)
{
	char *logbuf = logger.buffer;
	if (!logbuf)
		return -ENOMEM;

	switch(level) {
	case ESS_LOGGER_LEVEL_HEADER:
		{
			struct tm tmBuf;
			u64 tv_kernel;
			unsigned int logbuf_len;
			unsigned long rem_nsec;

			if (logger.id == ESS_LOG_ID_EVENTS)
				break;

			tv_kernel = local_clock();
			rem_nsec = do_div(tv_kernel, 1000000000);
			time_to_tm(logger.tv_sec, 0, &tmBuf);

			logbuf_len = snprintf(logbuf, ESS_LOGGER_HEADER_SIZE,
					"\n[%5lu.%06lu][%d:%16s] %02d-%02d %02d:%02d:%02d.%03d %5d %5d  ",
					(unsigned long)tv_kernel, rem_nsec / 1000,
					raw_smp_processor_id(), current->comm,
					tmBuf.tm_mon + 1, tmBuf.tm_mday,
					tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec,
					logger.tv_nsec / 1000000, logger.pid, logger.tid);

			logger.func_hook_logger("log_platform", logbuf, logbuf_len - 1);
		}
		break;
	case ESS_LOGGER_LEVEL_PREFIX:
		{
			static const char* kPrioChars = "!.VDIWEFS";
			unsigned char prio = logger.msg[0];

			if (logger.id == ESS_LOG_ID_EVENTS)
				break;

			logbuf[0] = prio < strlen(kPrioChars) ? kPrioChars[prio] : '?';
			logbuf[1] = ' ';

			logger.func_hook_logger("log_platform", logbuf, ESS_LOGGER_LEVEL_PREFIX);
		}
		break;
	case ESS_LOGGER_LEVEL_TEXT:
		{
			char *eatnl = buffer + count - ESS_LOGGER_STRING_PAD;

			if (logger.id == ESS_LOG_ID_EVENTS)
				break;
			if (count == ESS_LOGGER_SKIP_COUNT && *eatnl != '\0')
				break;

			logger.func_hook_logger("log_platform", buffer, count - 1);
#ifdef CONFIG_SEC_EXT
			if (count > 1 && strncmp(buffer, "!@", 2) == 0) {
				/* To prevent potential buffer overrun
				 * put a null at the end of the buffer if required
				 */
				buffer[count - 1] = '\0';
				pr_info("%s\n", buffer);
#ifdef CONFIG_SEC_BOOTSTAT
				if (count > 5 && strncmp(buffer, "!@Boot", 6) == 0)
					sec_bootstat_add(buffer);
#endif /* CONFIG_SEC_BOOTSTAT */

			}
#endif /* CONFIG_SEC_EXT */
		}
		break;
	default:
		break;
	}
	return 0;
}

int exynos_ss_hook_pmsg(char *buffer, size_t count)
{
	ess_android_log_header_t header;
	ess_pmsg_log_header_t pmsg_header;

	if (!logger.buffer)
		return -ENOMEM;

	switch(count) {
	case sizeof(pmsg_header):
		memcpy((void *)&pmsg_header, buffer, count);
		if (pmsg_header.magic != 'l') {
			exynos_ss_combine_pmsg(buffer, count, ESS_LOGGER_LEVEL_TEXT);
		} else {
			/* save logger data */
			logger.pid = pmsg_header.pid;
			logger.uid = pmsg_header.uid;
			logger.len = pmsg_header.len;
		}
		break;
	case sizeof(header):
		/* save logger data */
		memcpy((void *)&header, buffer, count);
		logger.id = header.id;
		logger.tid = header.tid;
		logger.tv_sec = header.tv_sec;
		logger.tv_nsec  = header.tv_nsec;
		if (logger.id > 7) {
			/* write string */
			exynos_ss_combine_pmsg(buffer, count, ESS_LOGGER_LEVEL_TEXT);
		} else {
			/* write header */
			exynos_ss_combine_pmsg(buffer, count, ESS_LOGGER_LEVEL_HEADER);
		}
		break;
	case sizeof(unsigned char):
		logger.msg[0] = buffer[0];
		/* write char for prefix */
		exynos_ss_combine_pmsg(buffer, count, ESS_LOGGER_LEVEL_PREFIX);
		break;
	default:
		/* write string */
		exynos_ss_combine_pmsg(buffer, count, ESS_LOGGER_LEVEL_TEXT);
		break;
	}

	return 0;
}
EXPORT_SYMBOL(exynos_ss_hook_pmsg);

/*
 *  To support pstore/pmsg/pstore_ram, following is implementation for exynos-snapshot
 *  ess_ramoops platform_device is used by pstore fs.
 */

static struct ramoops_platform_data ess_ramoops_data = {
	.record_size	= SZ_4K,
	.pmsg_size	= SZ_4K,
	.dump_oops	= 1,
};

static struct platform_device ess_ramoops = {
	.name = "ramoops",
	.dev = {
		.platform_data = &ess_ramoops_data,
	},
};

static int __init ess_pstore_init(void)
{
	if (exynos_ss_get_enable("log_pstore")) {
		ess_ramoops_data.mem_size = exynos_ss_get_item_size("log_pstore");
		ess_ramoops_data.mem_address = exynos_ss_get_item_paddr("log_pstore");
		ess_ramoops_data.pmsg_size = ess_ramoops_data.mem_size / 2;
		ess_ramoops_data.record_size = ess_ramoops_data.mem_size / 2;
	}
	return platform_device_register(&ess_ramoops);
}

static void __exit ess_pstore_exit(void)
{
	platform_device_unregister(&ess_ramoops);
}
module_init(ess_pstore_init);
module_exit(ess_pstore_exit);

MODULE_DESCRIPTION("Exynos Snapshot pstore module");
MODULE_LICENSE("GPL");

/*
 *  sysfs implementation for exynos-snapshot
 *  you can access the sysfs of exynos-snapshot to /sys/devices/system/exynos-ss
 *  path.
 */
static struct bus_type ess_subsys = {
	.name = "exynos-ss",
	.dev_name = "exynos-ss",
};

static ssize_t ess_enable_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	struct exynos_ss_item *item;
	unsigned long i;
	ssize_t n = 0;

	/*  item  */
	for (i = 0; i < ess_desc.log_enable_cnt; i++) {
		item = &ess_items[i];
		n += scnprintf(buf + n, 24, "%-12s : %sable\n",
			item->name, item->entry.enabled ? "en" : "dis");
        }

	/*  base  */
	n += scnprintf(buf + n, 24, "%-12s : %sable\n",
			"base", ess_base.enabled ? "en" : "dis");

	return n;
}

static ssize_t ess_enable_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int en;
	char *name;

	name = (char *)kstrndup(buf, count, GFP_KERNEL);
	if (!name)
		return count;

	name[count - 1] = '\0';

	en = exynos_ss_get_enable(name);

	if (en == -1)
		pr_info("echo name > enabled\n");
	else {
		if (en)
			exynos_ss_set_enable(name, false);
		else
			exynos_ss_set_enable(name, true);
	}

	kfree(name);
	return count;
}

static ssize_t ess_callstack_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	ssize_t n = 0;

	n = scnprintf(buf, 24, "callstack depth : %d\n", ess_desc.callstack);

	return n;
}

static ssize_t ess_callstack_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long callstack;

	callstack = simple_strtoul(buf, NULL, 0);
	pr_info("callstack depth(min 1, max 4) : %lu\n", callstack);

	if (callstack < 5 && callstack > 0) {
		ess_desc.callstack = (unsigned int)callstack;
		pr_info("success inserting %lu to callstack value\n", callstack);
	}
	return count;
}

static ssize_t ess_irqlog_exlist_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	unsigned long i;
	ssize_t n = 0;

	n = scnprintf(buf, 24, "excluded irq number\n");

	for (i = 0; i < ARRAY_SIZE(ess_irqlog_exlist); i++) {
		if (ess_irqlog_exlist[i] == 0)
			break;
		n += scnprintf(buf + n, 24, "irq num: %-4d\n", ess_irqlog_exlist[i]);
        }
	return n;
}

static ssize_t ess_irqlog_exlist_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long i;
	unsigned long irq;

	irq = simple_strtoul(buf, NULL, 0);
	pr_info("irq number : %lu\n", irq);

	for (i = 0; i < ARRAY_SIZE(ess_irqlog_exlist); i++) {
		if (ess_irqlog_exlist[i] == 0)
			break;
	}

	if (i == ARRAY_SIZE(ess_irqlog_exlist)) {
		pr_err("list is full\n");
		return count;
	}

	if (irq != 0) {
		ess_irqlog_exlist[i] = irq;
		pr_info("success inserting %lu to list\n", irq);
	}
	return count;
}

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
static ssize_t ess_irqexit_exlist_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	unsigned long i;
	ssize_t n = 0;

	n = scnprintf(buf, 36, "Excluded irq number\n");
	for (i = 0; i < ARRAY_SIZE(ess_irqexit_exlist); i++) {
		if (ess_irqexit_exlist[i] == 0)
			break;
		n += scnprintf(buf + n, 24, "IRQ num: %-4d\n", ess_irqexit_exlist[i]);
        }
	return n;
}

static ssize_t ess_irqexit_exlist_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long i;
	unsigned long irq;

	irq = simple_strtoul(buf, NULL, 0);
	pr_info("irq number : %lu\n", irq);

	for (i = 0; i < ARRAY_SIZE(ess_irqexit_exlist); i++) {
		if (ess_irqexit_exlist[i] == 0)
			break;
	}

	if (i == ARRAY_SIZE(ess_irqexit_exlist)) {
		pr_err("list is full\n");
		return count;
	}

	if (irq != 0) {
		ess_irqexit_exlist[i] = irq;
		pr_info("success inserting %lu to list\n", irq);
	}
	return count;
}

static ssize_t ess_irqexit_threshold_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	ssize_t n;

	n = scnprintf(buf, 46, "threshold : %12u us\n", ess_irqexit_threshold);
	return n;
}

static ssize_t ess_irqexit_threshold_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long val;

	val = simple_strtoul(buf, NULL, 0);
	pr_info("threshold value : %lu\n", val);

	if (val != 0) {
		ess_irqexit_threshold = (unsigned int)val;
		pr_info("success %lu to threshold\n", val);
	}
	return count;
}
#endif

#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
static ssize_t ess_reg_exlist_show(struct kobject *kobj,
			         struct kobj_attribute *attr, char *buf)
{
	unsigned long i;
	ssize_t n = 0;

	n = scnprintf(buf, 36, "excluded register address\n");
	for (i = 0; i < ARRAY_SIZE(ess_reg_exlist); i++) {
		if (ess_reg_exlist[i].addr == 0)
			break;
		n += scnprintf(buf + n, 40, "register addr: %08zx size: %08zx\n",
				ess_reg_exlist[i].addr, ess_reg_exlist[i].size);
        }
	return n;
}

static ssize_t ess_reg_exlist_store(struct kobject *kobj,
				struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	unsigned long i;
	size_t addr;

	addr = simple_strtoul(buf, NULL, 0);
	pr_info("register addr: %zx\n", addr);

	for (i = 0; i < ARRAY_SIZE(ess_reg_exlist); i++) {
		if (ess_reg_exlist[i].addr == 0)
			break;
	}
	if (addr != 0) {
		ess_reg_exlist[i].size = SZ_4K;
		ess_reg_exlist[i].addr = addr;
		pr_info("success %zx to threshold\n", (addr));
	}
	return count;
}
#endif

static struct kobj_attribute ess_enable_attr =
        __ATTR(enabled, 0644, ess_enable_show, ess_enable_store);
static struct kobj_attribute ess_callstack_attr =
        __ATTR(callstack, 0644, ess_callstack_show, ess_callstack_store);
static struct kobj_attribute ess_irqlog_attr =
        __ATTR(exlist_irqdisabled, 0644, ess_irqlog_exlist_show,
					ess_irqlog_exlist_store);
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
static struct kobj_attribute ess_irqexit_attr =
        __ATTR(exlist_irqexit, 0644, ess_irqexit_exlist_show,
					ess_irqexit_exlist_store);
static struct kobj_attribute ess_irqexit_threshold_attr =
        __ATTR(threshold_irqexit, 0644, ess_irqexit_threshold_show,
					ess_irqexit_threshold_store);
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
static struct kobj_attribute ess_reg_attr =
        __ATTR(exlist_reg, 0644, ess_reg_exlist_show, ess_reg_exlist_store);
#endif

static struct attribute *ess_sysfs_attrs[] = {
	&ess_enable_attr.attr,
	&ess_callstack_attr.attr,
	&ess_irqlog_attr.attr,
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
	&ess_irqexit_attr.attr,
	&ess_irqexit_threshold_attr.attr,
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
	&ess_reg_attr.attr,
#endif
	NULL,
};

static struct attribute_group ess_sysfs_group = {
	.attrs = ess_sysfs_attrs,
};

static const struct attribute_group *ess_sysfs_groups[] = {
	&ess_sysfs_group,
	NULL,
};

static int __init exynos_ss_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&ess_subsys, ess_sysfs_groups);
	if (ret)
		pr_err("fail to register exynos-snapshop subsys\n");

	return ret;
}
late_initcall(exynos_ss_sysfs_init);

#if defined(CONFIG_EXYNOS_SNAPSHOT_THERMAL) && defined(CONFIG_SEC_PM_DEBUG)
#include <linux/debugfs.h>

static int exynos_ss_thermal_show(struct seq_file *m, void *unused)
{
	struct exynos_ss_item *item = &ess_items[ess_desc.kevents_num];
	unsigned long idx, size;
	unsigned long rem_nsec;
	u64 ts;
	int i;

	if (unlikely(!ess_base.enabled || !item->entry.enabled))
		return 0;

	seq_puts(m, "time\t\t\ttemperature\tcooling_device\t\tmax_frequency\n");

	size = ARRAY_SIZE(ess_log->thermal);
	idx = atomic_read(&ess_idx.thermal_log_idx);

	for (i = 0; i < 400 && i < size; i++, idx--) {
		idx &= size - 1;

		ts = ess_log->thermal[idx].time;
		if (!ts)
			break;

		rem_nsec = do_div(ts, NSEC_PER_SEC);

		seq_printf(m, "[%8lu.%06lu]\t%u\t\t%-16s\t%u\n",
				(unsigned long)ts, rem_nsec / NSEC_PER_USEC,
				ess_log->thermal[idx].temp,
				ess_log->thermal[idx].cooling_device,
				ess_log->thermal[idx].cooling_state);
	}

	return 0;
}

static int exynos_ss_thermal_open(struct inode *inode, struct file *file)
{
	return single_open(file, exynos_ss_thermal_show, NULL);
}

static const struct file_operations thermal_fops = {
	.owner = THIS_MODULE,
	.open = exynos_ss_thermal_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dentry *debugfs_ess_root;

static int __init exynos_ss_debugfs_init(void)
{
	debugfs_ess_root = debugfs_create_dir("exynos-ss", NULL);
	if (!debugfs_ess_root) {
		pr_err("Failed to create exynos-ss debugfs\n");
		return 0;
	}

	debugfs_create_file("thermal", 0444, debugfs_ess_root, NULL,
			&thermal_fops);

	return 0;
}

late_initcall(exynos_ss_debugfs_init);
#endif /* CONFIG_EXYNOS_SNAPSHOT_THERMAL && CONFIG_SEC_PM_DEBUG */


#if defined(CONFIG_HARDLOCKUP_DETECTOR_OTHER_CPU)			\
	&& defined(CONFIG_SEC_DEBUG)
#define for_each_generated_irq_in_snapshot(idx, i, max, base, cpu)							\
	for (i = 0, idx = base; i < max; ++i, idx = (base - i) & (ARRAY_SIZE(ess_log->irq[0]) - 1))		\
		if (ess_log->irq[cpu][idx].en == ESS_FLAG_IN)

static inline void exynos_ss_get_busiest_irq(struct hardlockup_info *hl_info, unsigned long start_idx, int cpu)
{
	#define MAX_BUF 5
	int i, j, idx, max_count = 20;
	int buf_count = 0;
	int max_irq_idx = 0;

	struct irq_info_buf {
		unsigned int occurrences;
		int irq;
		void *fn;
		unsigned long long total_duration;
		unsigned long long last_time;
	};

	struct irq_info_buf i_buf[MAX_BUF] = {{0,},};

	for_each_generated_irq_in_snapshot(idx, i, max_count, start_idx, cpu) {
		for (j = 0; j < buf_count; j++) {
			if (i_buf[j].irq == ess_log->irq[cpu][idx].irq) {
				i_buf[j].total_duration += (i_buf[j].last_time - ess_log->irq[cpu][idx].time);
				i_buf[j].last_time = ess_log->irq[cpu][idx].time;
				i_buf[j].occurrences++;
				break;
			}
		}

		if (j == buf_count && buf_count < MAX_BUF) {
			i_buf[buf_count].irq = ess_log->irq[cpu][idx].irq;
			i_buf[buf_count].fn = ess_log->irq[cpu][idx].fn;
			i_buf[buf_count].occurrences = 0;
			i_buf[buf_count].total_duration = 0;
			i_buf[buf_count].last_time = ess_log->irq[cpu][idx].time;
			buf_count++;
		} else if (buf_count == MAX_BUF) {
			pr_info("Buffer overflow. Various irqs were generated!!\n");
		}
	}

	for (i = 1; i < buf_count; i++) {
		if (i_buf[max_irq_idx].occurrences < i_buf[i].occurrences)
			max_irq_idx = i;
	}

	hl_info->irq_info.irq = i_buf[max_irq_idx].irq;
	hl_info->irq_info.fn = i_buf[max_irq_idx].fn;
	hl_info->irq_info.avg_period = i_buf[max_irq_idx].total_duration / i_buf[max_irq_idx].occurrences;
}

void exynos_ss_get_hardlockup_info(unsigned int cpu,  void *info)
{
	struct hardlockup_info *hl_info = info;
	unsigned long cpuidle_idx, irq_idx, task_idx;
	unsigned long long cpuidle_delay_time, irq_delay_time, task_delay_time;
	unsigned long long curr, thresh;

	thresh = get_hardlockup_thresh();
	curr = local_clock();

	cpuidle_idx = atomic_read(&ess_idx.cpuidle_log_idx[cpu]) & (ARRAY_SIZE(ess_log->cpuidle[0]) - 1);
	cpuidle_delay_time = curr - ess_log->cpuidle[cpu][cpuidle_idx].time;

	if (ess_log->cpuidle[cpu][cpuidle_idx].en == ESS_FLAG_IN
		&& cpuidle_delay_time > thresh) {
		hl_info->delay_time = cpuidle_delay_time;
		hl_info->cpuidle_info.mode = ess_log->cpuidle[cpu][cpuidle_idx].modes;
		hl_info->hl_type = HL_IDLE_STUCK;
		return;
	}

	irq_idx = atomic_read(&ess_idx.irq_log_idx[cpu]) & (ARRAY_SIZE(ess_log->irq[0]) - 1);
	irq_delay_time = curr - ess_log->irq[cpu][irq_idx].time;

	if (ess_log->irq[cpu][irq_idx].en == ESS_FLAG_IN
		&& irq_delay_time > thresh) {

		hl_info->delay_time = irq_delay_time;

		if (ess_log->irq[cpu][irq_idx].irq < 0) {				// smc calls have negative irq number
			hl_info->smc_info.cmd = ess_log->irq[cpu][irq_idx].irq;
			hl_info->hl_type = HL_SMC_CALL_STUCK;
			return;
		} else {
			hl_info->irq_info.irq = ess_log->irq[cpu][irq_idx].irq;
			hl_info->irq_info.fn = ess_log->irq[cpu][irq_idx].fn;
			hl_info->hl_type = HL_IRQ_STUCK;
			return;
		}
	}

	task_idx = atomic_read(&ess_idx.task_log_idx[cpu]) & (ARRAY_SIZE(ess_log->task[0]) - 1);
	task_delay_time = curr - ess_log->task[cpu][task_idx].time;

	if (task_delay_time > thresh) {
		hl_info->delay_time = task_delay_time;
		if (irq_delay_time > thresh) {
			strncpy(hl_info->task_info.task_comm,
				ess_log->task[cpu][task_idx].task_comm,
				TASK_COMM_LEN - 1);
			hl_info->hl_type = HL_TASK_STUCK;
			return;
		} else {
			exynos_ss_get_busiest_irq(hl_info, irq_idx, cpu);
			hl_info->hl_type = HL_IRQ_STORM;
			return;
		}
	}

	hl_info->hl_type = HL_UNKNOWN_STUCK;
}

void exynos_ss_get_softlockup_info(unsigned int cpu, void *info)
{
	struct softlockup_info *sl_info = info;
	unsigned long task_idx;
	unsigned long long task_delay_time;
	unsigned long long curr, thresh;

	thresh = get_ess_softlockup_thresh();
	curr = local_clock();
	task_idx = atomic_read(&ess_idx.task_log_idx[cpu]) & (ARRAY_SIZE(ess_log->task[0]) - 1);
	task_delay_time = curr - ess_log->task[cpu][task_idx].time;
	sl_info->delay_time = task_delay_time;

	strncpy(sl_info->task_info.task_comm,
		ess_log->task[cpu][task_idx].task_comm,
		TASK_COMM_LEN - 1);
	sl_info->task_info.task_comm[TASK_COMM_LEN - 1] = '\0';

	if (task_delay_time > thresh)
		sl_info->sl_type = SL_TASK_STUCK;
	else
		sl_info->sl_type = SL_UNKNOWN_STUCK;
}
#endif