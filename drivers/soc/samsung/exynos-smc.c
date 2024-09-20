/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS SMC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/smc.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/device.h>

#ifdef CONFIG_EXYNOS_SNAPSHOT_LOGGING_SMC_CALL

#define EXYNOS_SMC_LOG_SIZE	1024

static DEFINE_SPINLOCK(smc_log_lock);

static struct bus_type esmc_subsys = {
	.name = "exynos-smc",
	.dev_name = "exynos-smc",
};

struct esmc_log {
	unsigned long cpu_clk;			// cpu clock
	unsigned long long time;		// start time
	unsigned long sp;			// call stack
	unsigned long long end_time;		// end time
	unsigned long long latency;		// latency
	unsigned long cmd;
	unsigned long arg1;
	unsigned long arg2;
	unsigned long arg3;
};

struct esmc_log smc_log[NR_CPUS][EXYNOS_SMC_LOG_SIZE];
static uint32_t smc_log_idx[NR_CPUS];

static unsigned int esmc_log_threshold =
		CONFIG_EXYNOS_SMC_LOG_THRESHOLD;

static ssize_t esmc_log_threshold_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t n;

	n = scnprintf(buf, 46, "threshold : %12u us\n", esmc_log_threshold);
	return n;
}

static ssize_t esmc_log_threshold_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long val;
	int err;

	err = kstrtoul(buf, 0, &val);

	if (err != 0)
		pr_err("can't read threshold value with err 0x%x\n", err);
	else {
		esmc_log_threshold = val;
		pr_info("threshold value : %lu\n", val);
	}

	return count;
}

static struct kobj_attribute esmc_log_threshold_attr =
	__ATTR(threshold_esmc_log, 0644, esmc_log_threshold_show,
					esmc_log_threshold_store);

static struct attribute *esmc_sysfs_attrs[] = {
	&esmc_log_threshold_attr.attr,
	NULL,
};

static struct attribute_group esmc_sysfs_group = {
	.attrs = esmc_sysfs_attrs,
};

static const struct attribute_group *esmc_sysfs_groups[] = {
	&esmc_sysfs_group,
	NULL,
};

static int __init esmc_sysfs_init(void)
{
	int ret = 0;

	ret = subsys_system_register(&esmc_subsys, esmc_sysfs_groups);
	if (ret)
		pr_err("fail to register exynos-smc subsys\n");

	return ret;
}
late_initcall(esmc_sysfs_init);
#endif

int exynos_smc(unsigned long cmd, unsigned long arg1, unsigned long arg2, unsigned long arg3)
{
	int32_t ret;
#ifdef CONFIG_EXYNOS_SNAPSHOT_LOGGING_SMC_CALL
	unsigned long flags;
	uint32_t cpu;
	uint64_t time, end_time, latency;

	cpu = get_current_cpunum();
	time = cpu_clock(cpu);
#endif
	ret = __exynos_smc(cmd, arg1, arg2, arg3);
#ifdef CONFIG_EXYNOS_SNAPSHOT_LOGGING_SMC_CALL
	end_time = cpu_clock(cpu);
	latency = end_time - time;

	spin_lock_irqsave(&smc_log_lock, flags);
	if (latency > (esmc_log_threshold * 1000)) {
		smc_log[cpu][smc_log_idx[cpu]].cpu_clk = local_clock();
		smc_log[cpu][smc_log_idx[cpu]].latency = latency;
		smc_log[cpu][smc_log_idx[cpu]].time = time;
		smc_log[cpu][smc_log_idx[cpu]].end_time = end_time;
		smc_log[cpu][smc_log_idx[cpu]].sp =
			(uint64_t)current_stack_pointer;
		smc_log[cpu][smc_log_idx[cpu]].cmd = cmd;
		smc_log[cpu][smc_log_idx[cpu]].arg1 = arg1;
		smc_log[cpu][smc_log_idx[cpu]].arg2 = arg2;
		smc_log[cpu][smc_log_idx[cpu]].arg3 = arg3;
		smc_log_idx[cpu]++;
		if (smc_log_idx[cpu] == EXYNOS_SMC_LOG_SIZE)
			smc_log_idx[cpu] = 0;
	}
	spin_unlock_irqrestore(&smc_log_lock, flags);
#endif
	return ret;
}
