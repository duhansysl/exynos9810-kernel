/*
 * Copyright (C) 2012-2019, Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/atomic.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/kthread.h>
#include <linux/seqlock.h>
#include <linux/wait.h>

#include "tzdev_internal.h"
#include "core/iwservice.h"
#include "core/log.h"
#include "core/notifier.h"
#include "core/sysdep.h"

static DECLARE_WAIT_QUEUE_HEAD(tz_hotplug_wq);
static atomic_t tz_hotplug_request = ATOMIC_INIT(0);

static cpumask_t nwd_cpu_mask;
static atomic_t tz_hotplug_init_done = ATOMIC_INIT(0);
static DEFINE_SEQLOCK(tzdev_hotplug_lock);
static struct task_struct *tz_hotplug_thread;

static int tz_hotplug_callback(struct notifier_block *nfb,
			unsigned long action, void *hcpu);

static struct notifier_block tz_hotplug_notifier = {
	.notifier_call = tz_hotplug_callback,
};

static void tz_hotplug_cpus_up(cpumask_t mask)
{
	int cpu;

	for_each_cpu_mask(cpu, mask)
		if (!cpu_online(cpu)) {
			log_debug(tzdev_hotplug, "bringup cpu[%d]\n", cpu);
			cpu_up(cpu);
		}
}

void tz_hotplug_update_nwd_cpu_mask(unsigned long new_mask)
{
	cpumask_t new_cpus_mask;
	unsigned long new_mask_tmp = new_mask;

	write_seqlock(&tzdev_hotplug_lock);

	cpumask_copy(&nwd_cpu_mask, to_cpumask(&new_mask_tmp));

	write_sequnlock(&tzdev_hotplug_lock);

	cpumask_andnot(&new_cpus_mask, to_cpumask(&new_mask_tmp), cpu_online_mask);

	tz_hotplug_cpus_up(new_cpus_mask);
}

static int tz_hotplug_cpu(void *data)
{
	int seq;
	cpumask_t new_cpu_mask;
	cpumask_t nwd_cpu_mask_local;
	unsigned long sk_cpu_mask;

	while (!kthread_should_stop()) {
		wait_event(tz_hotplug_wq, atomic_xchg(&tz_hotplug_request, 0) ||
			kthread_should_stop());
		do {
			seq = read_seqbegin(&tzdev_hotplug_lock);
			cpumask_copy(&nwd_cpu_mask_local, &nwd_cpu_mask);
		} while (read_seqretry(&tzdev_hotplug_lock, seq));

		sk_cpu_mask = tz_iwservice_get_cpu_mask();
		cpumask_or(&new_cpu_mask, to_cpumask(&sk_cpu_mask), &nwd_cpu_mask_local);

		tz_hotplug_cpus_up(new_cpu_mask);
	}

	return 0;
}

static int tz_hotplug_callback(struct notifier_block *nfb,
			unsigned long action, void *hcpu)
{
	int seq, set;
	unsigned long sk_cpu_mask;
	cpumask_t swd_cpu_mask;

	if (action == CPU_DOWN_PREPARE) {
		do {
			sk_cpu_mask = tz_iwservice_get_cpu_mask();
			cpumask_copy(&swd_cpu_mask, to_cpumask(&sk_cpu_mask));
			seq = read_seqbegin(&tzdev_hotplug_lock);
			set = cpu_isset((unsigned long)hcpu, swd_cpu_mask);
			set |= cpu_isset((unsigned long)hcpu, nwd_cpu_mask);
		} while (read_seqretry(&tzdev_hotplug_lock, seq));

		if (set) {
			log_debug(tzdev_hotplug, "deny cpu[%ld] shutdown\n", (unsigned long) hcpu);
			return NOTIFY_BAD;
		}
	}

	return NOTIFY_OK;
}

static int tz_hotplug_cpu_down_callback(unsigned int cpu)
{
	void *hcpu = (void *)(long)cpu;

	return tz_hotplug_callback(&tz_hotplug_notifier, CPU_DOWN_PREPARE, hcpu);
}

static int tz_hotplug_init_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	(void)cb;
	(void)code;
	(void)unused;

	sysdep_register_cpu_notifier(&tz_hotplug_notifier,
		NULL,
		tz_hotplug_cpu_down_callback);

	tz_hotplug_thread = kthread_run(tz_hotplug_cpu, NULL, "tzdev_hotplug");
	if (IS_ERR(tz_hotplug_thread)) {
		log_error(tzdev_hotplug, "Failed to create hotplug kernel thread, error=%ld\n",
				PTR_ERR(tz_hotplug_thread));
		sysdep_unregister_cpu_notifier(&tz_hotplug_notifier);
		return NOTIFY_DONE;
	}
	atomic_set(&tz_hotplug_init_done, 1);

	log_info(tzdev_hotplug, "Hotplug initialization done.\n");

	return NOTIFY_DONE;
}

static int tz_hotplug_fini_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	(void)cb;
	(void)code;
	(void)unused;

	if (!atomic_cmpxchg(&tz_hotplug_init_done, 1, 0)) {
		log_info(tzdev_hotplug, "Hotplug not initialized.\n");
		return NOTIFY_DONE;
	}

	kthread_stop(tz_hotplug_thread);
	sysdep_unregister_cpu_notifier(&tz_hotplug_notifier);

	log_info(tzdev_hotplug, "Hotplug finalization done.\n");

	return NOTIFY_DONE;
}

static int tz_hotplug_post_smc_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	cpumask_t new_cpus_mask;
	unsigned long sk_cpu_mask;

	(void)cb;
	(void)code;
	(void)unused;

	/* Check cores activation */
	sk_cpu_mask = tz_iwservice_get_cpu_mask();
	cpumask_andnot(&new_cpus_mask, to_cpumask(&sk_cpu_mask), cpu_online_mask);

	if (!cpumask_empty(&new_cpus_mask)) {
		atomic_set(&tz_hotplug_request, 1);
		wake_up(&tz_hotplug_wq);
	}

	return NOTIFY_DONE;
}

static struct notifier_block tz_hotplug_init_notifier = {
	.notifier_call = tz_hotplug_init_call,
};

static struct notifier_block tz_hotplug_fini_notifier = {
	.notifier_call = tz_hotplug_fini_call,
};

static struct notifier_block tz_hotplug_post_smc_notifier = {
	.notifier_call = tz_hotplug_post_smc_call,
};

static int __init tz_hotplug_init(void)
{
	int rc;

	rc = tzdev_blocking_notifier_register(TZDEV_INIT_NOTIFIER, &tz_hotplug_init_notifier);
	if (rc) {
		log_error(tzdev_hotplug, "Failed to register init notifier, error=%d\n", rc);
		return rc;
	}

	rc = tzdev_blocking_notifier_register(TZDEV_FINI_NOTIFIER, &tz_hotplug_fini_notifier);
	if (rc) {
		log_error(tzdev_hotplug, "Failed to register fini notifier, error=%d\n", rc);
		goto fini_notifier_registration_failed;
	}

	rc = tzdev_atomic_notifier_register(TZDEV_POST_SMC_NOTIFIER, &tz_hotplug_post_smc_notifier);
	if (rc) {
		log_error(tzdev_hotplug, "Failed to register post smc notifier, error=%d\n", rc);
		goto post_smc_notifier_registration_failed;
	}

	tzdev_set_nwd_sysconf_flag(SYSCONF_NWD_CPU_HOTPLUG);
	log_info(tzdev_hotplug, "Hotplug callbacks registration done\n");

	return rc;

post_smc_notifier_registration_failed:
	tzdev_blocking_notifier_unregister(TZDEV_FINI_NOTIFIER, &tz_hotplug_fini_notifier);
fini_notifier_registration_failed:
	tzdev_blocking_notifier_unregister(TZDEV_INIT_NOTIFIER, &tz_hotplug_init_notifier);

	return rc;
}

early_initcall(tz_hotplug_init);
