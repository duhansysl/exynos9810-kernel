/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * Park Bumgyu <bumgyu.park@samsung.com>
 *
 * CPU Hotplug driver for Exynos
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpu.h>
#include <linux/fb.h>
#include <linux/kthread.h>
#include <linux/pm_qos.h>
#include <linux/suspend.h>

#include <soc/samsung/exynos-cpu_hotplug.h>

static int cpu_hotplug_in(const struct cpumask *mask)
{
	int cpu, ret = 0;

	for_each_cpu(cpu, mask) {
		ret = cpu_up(cpu);
		if (ret) {
			/*
			 * -EIO means core fail to come online by itself
			 * it is critical error
			 */
			if (ret == -EIO)
				panic("I/O error(-EIO) occurs while CPU%d comes online\n", cpu);

			/*
			 * If it fails to enable cpu,
			 * it cancels cpu hotplug request and retries later.
			 */
			pr_err("%s: Failed to hotplug in CPU%d with error %d\n",
								__func__, cpu, ret);
			break;
		}
	}

	return ret;
}

static int cpu_hotplug_out(const struct cpumask *mask)
{
	int cpu, ret = 0;

	/*
	 * Reverse order of cpu,
	 * explore cpu7, cpu6, cpu5, ... cpu1
	 */
	for (cpu = nr_cpu_ids - 1; cpu > 0; cpu--) {
		if (!cpumask_test_cpu(cpu, mask))
			continue;

		ret = cpu_down(cpu);
		if (ret) {
			pr_err("%s: Failed to hotplug out CPU%d with error %d\n",
								__func__, cpu, ret);
			break;
		}
	}

	return ret;
}

static struct {
	/* Control cpu hotplug operation */
	bool			enabled;

	/* flag for suspend */
	bool			suspended;

	/* Synchronizes accesses to refcount and cpumask */
	struct mutex		lock;

	/* fast hotplug cpus */
	struct cpumask		fast_hp_cpus;

	/* user input minimum and maximum online cpu */
	int			user_min;
	int			user_max;

	/*
	 * In blocking notifier call chain, it is not supposed to call
	 * cpu_up() or cpu_down(). In this case, use workqueue.
	 */
	struct workqueue_struct	*workqueue;

	/*
	 * During reuesting cpu hotplug by other drivers, cpu hotplug framework
	 * rejects cpu hotplug request. To guarantee the request, re-request cpu
	 * hotplug using delayed work.
	 */
	struct delayed_work	delayed_work;

	/* cpu_hotplug kobject */
	struct kobject		*kobj;
} cpu_hotplug = {
	.lock = __MUTEX_INITIALIZER(cpu_hotplug.lock),
};

static inline void cpu_hotplug_suspend(bool enable)
{
	/* This lock guarantees completion of do_cpu_hotplug() */
	mutex_lock(&cpu_hotplug.lock);
	cpu_hotplug.suspended = enable;
	mutex_unlock(&cpu_hotplug.lock);
}

static inline void update_enable_flag(bool enable)
{
	mutex_lock(&cpu_hotplug.lock);
	cpu_hotplug.enabled = enable;
	mutex_unlock(&cpu_hotplug.lock);
}

struct kobject *exynos_cpu_hotplug_kobj(void)
{
	return cpu_hotplug.kobj;
}

bool exynos_cpu_hotplug_enabled(void)
{
	return cpu_hotplug.enabled;
}

/*
 * If somebody requests CPU hotplug, hotplug driver creates cpumask with minimum
 * and maxinum online CPU in PM QoS. The number of online CPU will be same as
 * minimum online CPU on the assumption that minimum online CPU is not greater
 * than maximum online CPU. If mininum is greater than maximum, online CPU will
 * be maximum.
 */
extern struct cpumask early_cpu_mask;
static struct cpumask create_cpumask(void)
{
	int online_cpu_min, online_cpu_max;
	int cpu;
	struct cpumask mask;

	online_cpu_min = min(pm_qos_request(PM_QOS_CPU_ONLINE_MIN), nr_cpu_ids);
	online_cpu_max = min(pm_qos_request(PM_QOS_CPU_ONLINE_MAX), nr_cpu_ids);

	cpumask_clear(&mask);

	for_each_possible_cpu (cpu) {
		if (!cpumask_test_cpu(cpu, &early_cpu_mask))
			continue;

		if (cpumask_weight(&mask) < online_cpu_min)
			cpumask_set_cpu(cpu, &mask);
		else
			break;
	}

	for (cpu = nr_cpu_ids - 1; cpu >= 0; cpu--) {
		if (!cpumask_test_cpu(cpu, &early_cpu_mask))
			continue;

		if (cpumask_weight(&mask) > online_cpu_max)
			cpumask_clear_cpu(cpu, &mask);
		else
			break;
	}

	return mask;
}

/*
 * do_cpu_hotplug() is the main function for cpu hotplug. Only this function
 * enables or disables cpus, so all APIs in this driver call do_cpu_hotplug()
 * eventually.
 */
static int do_cpu_up(struct cpumask enable_cpus, bool fast_hp)
{
	struct cpumask fast_cpus;
	int ret = 0;
	int (*func_cpu_up)(const struct cpumask *) = cpu_hotplug_in;

	cpumask_clear(&fast_cpus);
	if (fast_hp)
		cpumask_copy(&fast_cpus, &cpu_hotplug.fast_hp_cpus);

	cpumask_and(&fast_cpus, &enable_cpus, &fast_cpus);
	cpumask_andnot(&enable_cpus, &enable_cpus, &fast_cpus);
	if (!cpumask_empty(&enable_cpus))
		ret = func_cpu_up(&enable_cpus);
	if (ret)
		goto exit;

	if (fast_hp)
		ret = cpus_up(&fast_cpus);

exit:
	return ret;
}

static int do_cpu_down(struct cpumask disable_cpus, bool fast_hp)
{
	struct cpumask fast_cpus;
	int ret = 0;
	int (*func_cpu_down)(const struct cpumask *) = cpu_hotplug_out;

	cpumask_clear(&fast_cpus);
	if (fast_hp)
		cpumask_copy(&fast_cpus, &cpu_hotplug.fast_hp_cpus);

	cpumask_and(&fast_cpus, &disable_cpus, &fast_cpus);
	cpumask_andnot(&disable_cpus, &disable_cpus, &fast_cpus);
	if (fast_hp)
		ret = cpus_down(&fast_cpus);
	if (ret)
		goto exit;

	if (!cpumask_empty(&disable_cpus))
		ret = func_cpu_down(&disable_cpus);

exit:
	return ret;
}

int exynos_cpuhotplug_register_notifier(struct notifier_block *nb,
							unsigned int list)
{
	int ret;
	if (!cpu_hotplug.enabled)
		return -EINVAL;

	ret = blocking_notifier_chain_register(
			&exynos_cpuhotplug_notifier_list, nb);

	return ret;
}
EXPORT_SYMBOL(exynos_cpuhotplug_register_notifier);

int exynos_cpuhotplug_unregister_notifier(struct notifier_block *nb,
							unsigned int list)
{
	int ret;
	if (!cpu_hotplug.enabled)
		return -EINVAL;

	ret = blocking_notifier_chain_unregister(
			&exynos_cpuhotplug_notifier_list, nb);

	return ret;
}
EXPORT_SYMBOL(exynos_cpuhotplug_unregister_notifier);

static void update_cpu_hotplug_profiler(ktime_t now);
static int do_cpu_hotplug(bool fast_hp)
{
	int ret = 0;
	struct cpumask disable_cpus, enable_cpus;
	char cpus_buf[10];

	mutex_lock(&cpu_hotplug.lock);
	/*
	 * If cpu hotplug is disabled or suspended,
	 * do_cpu_hotplug() do nothing.
	 */
	if (!cpu_hotplug.enabled || cpu_hotplug.suspended) {
		mutex_unlock(&cpu_hotplug.lock);
		return 0;
	}

	/* Create online cpumask */
	enable_cpus = create_cpumask();
	/*
	 * disable_cpus is the opposite of enable_cpus:
	 * disable_cpus = ~enable_cpus
	 */
	cpumask_xor(&disable_cpus, cpu_possible_mask, &enable_cpus);

	/*
	 * Remove unnecessary cpumask bit:
	 * enable_cpus = enable_cpus & ~online mask
	 * disable_cpus = disable_cpus & online mask
	 */
	cpumask_andnot(&enable_cpus, &enable_cpus, cpu_online_mask);
	cpumask_and(&disable_cpus, &disable_cpus, cpu_online_mask);

	scnprintf(cpus_buf, sizeof(cpus_buf), "%*pbl", cpumask_pr_args(&enable_cpus));
	pr_info("%s: enable_cpus=%s fast_hp=%d\n", __func__, cpus_buf, fast_hp);
	scnprintf(cpus_buf, sizeof(cpus_buf), "%*pbl", cpumask_pr_args(&disable_cpus));
	pr_info("%s: disable_cpus=%s fast_hp=%d\n", __func__, cpus_buf, fast_hp);

	if (!cpumask_empty(&enable_cpus))
		ret = do_cpu_up(enable_cpus, fast_hp);
	if (ret)
		goto out;

	if (!cpumask_empty(&disable_cpus))
		ret = do_cpu_down(disable_cpus, fast_hp);

out:
	/* If it fails to complete cpu hotplug request, retries after 100ms */
	if (ret)
		queue_delayed_work(cpu_hotplug.workqueue, &cpu_hotplug.delayed_work,
							msecs_to_jiffies(100));

	blocking_notifier_call_chain(&exynos_cpuhotplug_notifier_list,
			UPDATE_ONLINE_CPU, NULL);

	update_cpu_hotplug_profiler(ktime_get());
	mutex_unlock(&cpu_hotplug.lock);

	return ret;
}

static void cpu_hotplug_work(struct work_struct *work)
{
	do_cpu_hotplug(false);
}

static int control_cpu_hotplug(bool enable)
{
	struct cpumask mask;
	int ret = 0;

	if (enable) {
		update_enable_flag(true);
		do_cpu_hotplug(false);
	} else {
		mutex_lock(&cpu_hotplug.lock);

		cpumask_setall(&mask);
		cpumask_andnot(&mask, &mask, cpu_online_mask);
		cpumask_and(&mask, &mask, &early_cpu_mask);

		/*
		 * If it success to enable all CPUs, clear cpu_hotplug.enabled flag.
		 * Since then all hotplug requests are ignored.
		 */
		ret = cpu_hotplug_in(&mask);
		if (!ret) {
			/*
			 * In this position, can't use update_enable_flag()
			 * because already taken cpu_hotplug.lock
			 */
			cpu_hotplug.enabled = false;
		} else {
			pr_err("Fail to disable cpu hotplug, please try again\n");
		}

		mutex_unlock(&cpu_hotplug.lock);
	}

	return ret;
}

/*
 * If PM_QOS_CPU_ONLINE_MIN and PM_QOS_CPU_ONLINE_MAX request is updated,
 * cpu_hotplug_qos_handler is called.
 */
static int cpu_hotplug_qos_handler(struct notifier_block *b,
					 unsigned long val, void *v)
{
	long *p = (long *)v;

	/* use fast cpu hotplug sequence */
	if (p && *p == FAST_HP)
		return do_cpu_hotplug(true);

	return do_cpu_hotplug(false);
}

static struct notifier_block cpu_hotplug_qos_notifier = {
	.notifier_call = cpu_hotplug_qos_handler,
};

/*
 * Requests to control minimum/maximum online cpu
 */
struct pm_qos_request user_min_cpu_hotplug_request;
struct pm_qos_request user_max_cpu_hotplug_request;

/*
 * User can change the number of online cpu by using min_online_cpu and
 * max_online_cpu sysfs node. User input minimum and maxinum online cpu
 * to this node as below:
 *
 * #echo min > /sys/power/cpuhotplug/min_online_cpu
 * #echo max > /sys/power/cpuhotplug/max_online_cpus
 */
#define attr_online_cpu(type)						\
static ssize_t show_##type##_online_cpu(struct kobject *kobj,		\
	struct kobj_attribute *attr, char *buf)				\
{									\
	return snprintf(buf, 30, #type " online cpu : %d\n",		\
			cpu_hotplug.user_##type);			\
}									\
									\
static ssize_t store_##type##_online_cpu(struct kobject *kobj,		\
	struct kobj_attribute *attr, const char *buf,			\
	size_t count)							\
{									\
	int input;							\
									\
	if (!sscanf(buf, "%d", &input))					\
		return -EINVAL;						\
									\
	if (input <= 0 || input > NR_CPUS)							\
		return -EINVAL;						\
									\
	pm_qos_update_request(&user_##type##_cpu_hotplug_request,	\
				input);					\
	cpu_hotplug.user_##type = input;				\
									\
	return count;							\
}									\
									\
static struct kobj_attribute type##_online_cpu =			\
__ATTR(type##_online_cpu, 0644,					\
	show_##type##_online_cpu, store_##type##_online_cpu)

attr_online_cpu(min);
attr_online_cpu(max);

/* Exynos9810: CPUs 0..3 are LITTLE; CPUs 4..7 are BIG. */
static struct pm_qos_request hyper_big_min_request;
static DEFINE_MUTEX(hyper_big_min_lock);
static int hyper_big_min;

static ssize_t show_big_min_online_cpu(struct kobject *kobj,
                struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", READ_ONCE(hyper_big_min));
}

static ssize_t store_big_min_online_cpu(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t count)
{
	int value, total;

	if (kstrtoint(buf, 10, &value) || value < 0 || value > 4)
		return -EINVAL;
	if (nr_cpu_ids != 8)
		return -ENODEV;

	/* A zero request releases only this client, preserving other QoS limits. */
	total = value ? 4 + value : PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE;
	mutex_lock(&hyper_big_min_lock);
	pm_qos_update_request(&hyper_big_min_request, total);
	WRITE_ONCE(hyper_big_min, value);
	mutex_unlock(&hyper_big_min_lock);

	return count;
}

static struct kobj_attribute big_min_online_cpu =
__ATTR(big_min_online_cpu, 0644,
       show_big_min_online_cpu, store_big_min_online_cpu);

/* Independent BIG cap; global maximum/thermal requests still take precedence. */
static struct pm_qos_request hyper_big_max_request;
static DEFINE_MUTEX(hyper_big_max_lock);
static int hyper_big_max = 4;

static ssize_t show_big_max_online_cpu(struct kobject *kobj,
                struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", READ_ONCE(hyper_big_max));
}

static ssize_t store_big_max_online_cpu(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t count)
{
	int value;

	if (kstrtoint(buf, 10, &value) || value < 0 || value > 4)
		return -EINVAL;
	if (nr_cpu_ids != 8)
		return -ENODEV;

	/* 0 permits only LITTLE CPUs; 4 restores this client's full BIG budget. */
	mutex_lock(&hyper_big_max_lock);
	pm_qos_update_request(&hyper_big_max_request, 4 + value);
	WRITE_ONCE(hyper_big_max, value);
	mutex_unlock(&hyper_big_max_lock);

	return count;
}

static struct kobj_attribute big_max_online_cpu =
__ATTR(big_max_online_cpu, 0644,
       show_big_max_online_cpu, store_big_max_online_cpu);

/*
 * User can control the cpu hotplug operation as below:
 *
 * #echo 1 > /sys/power/cpuhotplug/enabled => enable
 * #echo 0 > /sys/power/cpuhotplug/enabled => disable
 *
 * If enabled become 0, hotplug driver enable the all cpus and no hotplug
 * operation happen from hotplug driver.
 */
static ssize_t show_cpu_hotplug_enable(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", cpu_hotplug.enabled);
}

static ssize_t store_cpu_hotplug_enable(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	control_cpu_hotplug(!!input);

	return count;
}

/*
 * User can profile the hotplug state as below:
 *
 * #echo 1 > /sys/power/cpuhotplug/profiler => start profiler
 * #echo 0 > /sys/power/cpuhotplug/profiler => end profiler
 * #cat /sys/power/cpuhotplug/profiler	    => show profile result
 */
/* hotplug profiler */
struct cpu_profile {
	s64 online_time;
	unsigned int online_count;
};

struct {
	struct cpu_profile cpu_profiler[NR_CPUS];	/* information of hp state */
	s64 last_update_time;			/* last hp time */
	s64 start_time;			/* time of profile start */
	s64 finish_time;			/* time of profile finish */
	s64 profile_time;
	struct cpumask last_online_mask;		/* last online state */
	bool started;					/* start profiler */
} profiler;

static unsigned int online_ratio(int cpu)
{
	s64 cpu_online_time = profiler.cpu_profiler[cpu].online_time * 100;
	return cpu_online_time / profiler.profile_time;
}

static void start_cpu_hotplug_profiler(void)
{
	int cpu;
	s64 now;

	if (!cpu_hotplug.enabled) {
		pr_err("CPU_HOTPLUG is disabled\n");
		return;
	}

	if (profiler.started) {
		pr_err("CPU_HOTPLUG profile is ongoing\n");
		return;
	}

	mutex_lock(&cpu_hotplug.lock);

	now = ktime_to_ms(ktime_get());
	for_each_possible_cpu(cpu) {
		profiler.cpu_profiler[cpu].online_time = 0;
		profiler.cpu_profiler[cpu].online_count = 0;
	}
	cpumask_copy(&profiler.last_online_mask, cpu_online_mask);
	profiler.last_update_time = now;
	profiler.start_time = now;
	profiler.started = true;

	mutex_unlock(&cpu_hotplug.lock);
}

static void update_cpu_hotplug_profiler(ktime_t k_now)
{
	int cpu;
	struct cpumask new_online_mask;
	s64 now;
	s64 online_time;

	if (!profiler.started)
		return;

	/* add online time */
	now = ktime_to_ms(k_now);
	online_time = now - profiler.last_update_time;
	for_each_cpu(cpu, &profiler.last_online_mask)
		profiler.cpu_profiler[cpu].online_time += online_time;

	/* add count for newly online cpu */
	cpumask_andnot(&new_online_mask, cpu_online_mask, &profiler.last_online_mask);
	for_each_cpu(cpu, &new_online_mask)
		profiler.cpu_profiler[cpu].online_count++;

	/* store last online cpu */
	cpumask_copy(&profiler.last_online_mask, cpu_online_mask);

	/* store last update time */
	profiler.last_update_time = now;

	return;
}

static void finish_cpu_hotplug_profiler(void)
{
	ktime_t now;

	if (!profiler.started) {
		pr_err("CPU_HOTPLUG profile does not start yet\n");
		return;
	}

	pr_info("CPU_HOTPLUG profile finish\n");

	mutex_lock(&cpu_hotplug.lock);

	now = ktime_get();
	profiler.finish_time = ktime_to_ms(now);
	profiler.profile_time = profiler.finish_time - profiler.start_time;
	update_cpu_hotplug_profiler(now);
	profiler.started = false;

	mutex_unlock(&cpu_hotplug.lock);

	return;
}

static ssize_t show_sysfs_result(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     char *buf)
{
	int cpu, ret = 0;

	if (profiler.started) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"CPU_HOTPLUG profile is ongoing\n");
		return ret;
	}

	if (profiler.profile_time == 0) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"CPU_HOTPLUPG profiler has not started yet\n");
		return ret;
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#############################################################\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"Profiling Start Time: %llums, Profiling Time : %llums\n",
					profiler.start_time, profiler.profile_time);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"[total cpu online ratio]\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#cpu   #online_cnt   #online_time    #ratio\n");

	for_each_possible_cpu(cpu)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"cpu%d %13u %12llums   %6u%%\n", cpu,
				profiler.cpu_profiler[cpu].online_count,
				profiler.cpu_profiler[cpu].online_time, online_ratio(cpu));

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "\n");

	return ret;
}

static ssize_t show_cpu_hotplug_profiler(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ret =0;

	if (profiler.started)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"CPU_HOTPLUG profile is ongoing\n");
	else
		ret = show_sysfs_result(kobj, attr, buf);


	return ret;
}

static ssize_t store_cpu_hotplug_profiler(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	if (!!input)
		start_cpu_hotplug_profiler();
	else
		finish_cpu_hotplug_profiler();

	return count;
}

static struct kobj_attribute cpu_hotplug_enabled =
__ATTR(enabled, 0644, show_cpu_hotplug_enable, store_cpu_hotplug_enable);
static struct kobj_attribute cpu_hotplug_profiler =
__ATTR(profiler, 0644, show_cpu_hotplug_profiler, store_cpu_hotplug_profiler);

static struct attribute *cpu_hotplug_attrs[] = {
	&min_online_cpu.attr,
	&big_min_online_cpu.attr,
	&big_max_online_cpu.attr,
	&max_online_cpu.attr,
	&cpu_hotplug_enabled.attr,
	&cpu_hotplug_profiler.attr,
	NULL,
};

static const struct attribute_group cpu_hotplug_group = {
	.attrs = cpu_hotplug_attrs,
};

static void __init cpu_hotplug_dt_init(void)
{
	struct device_node *np = of_find_node_by_name(NULL, "cpu_hotplug");
	const char *buf;

	if (of_property_read_string(np, "fast_hp_cpus", &buf)) {
		pr_warn("fast_hp_cpus property is omitted!\n");
		return;
	}
	cpulist_parse(buf, &cpu_hotplug.fast_hp_cpus);

	return;
}

static int exynos_cpu_hotplug_pm_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		cpu_hotplug_suspend(true);
		break;

	case PM_POST_SUSPEND:
		cpu_hotplug_suspend(false);
		do_cpu_hotplug(false);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpu_hotplug_nb = {
	.notifier_call = exynos_cpu_hotplug_pm_notifier,
};


static struct pm_qos_request default_min_cpu_hotplug_request;
void exynos_cpu_hotplug_gov_activated(void)
{
	pm_qos_update_request(&default_min_cpu_hotplug_request,
				PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE);
}

static void __init cpu_hotplug_pm_qos_init(void)
{
	unsigned int default_min = cpumask_weight(&early_cpu_mask);

	/* Register PM QoS notifier handler */
	pm_qos_add_notifier(PM_QOS_CPU_ONLINE_MIN, &cpu_hotplug_qos_notifier);
	pm_qos_add_notifier(PM_QOS_CPU_ONLINE_MAX, &cpu_hotplug_qos_notifier);

	pm_qos_add_request(&default_min_cpu_hotplug_request,
				PM_QOS_CPU_ONLINE_MIN, default_min);

	/* Add PM QoS for sysfs node */
	pm_qos_add_request(&hyper_big_max_request,
		PM_QOS_CPU_ONLINE_MAX, PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE);
	pm_qos_add_request(&hyper_big_min_request,
		PM_QOS_CPU_ONLINE_MIN, PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE);
	pm_qos_add_request(&user_min_cpu_hotplug_request,
		PM_QOS_CPU_ONLINE_MIN, PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE);
	pm_qos_add_request(&user_max_cpu_hotplug_request,
		PM_QOS_CPU_ONLINE_MAX, PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE);
}

static void __init cpu_hotplug_sysfs_init(void)
{
	cpu_hotplug.kobj = kobject_create_and_add("cpuhotplug", power_kobj);
	if (!cpu_hotplug.kobj) {
		pr_err("Fail to create cpu_hotplug kboject\n");
		return;
	}

	/* Create /sys/power/cpuhotplug */
	if (sysfs_create_group(cpu_hotplug.kobj, &cpu_hotplug_group)) {
		pr_err("Fail to create cpu_hotplug group\n");
		return;
	}

	/* link cpuhotplug directory to /sys/devices/system/cpu/cpuhotplug */
	if (sysfs_create_link(&cpu_subsys.dev_root->kobj, cpu_hotplug.kobj, "cpuhotplug"))
		pr_err("Fail to link cpuhotplug directory");
}

static int __init cpu_hotplug_init(void)
{
	/* Initialize delayed work */
	INIT_DELAYED_WORK(&cpu_hotplug.delayed_work, cpu_hotplug_work);

	/* Initialize workqueue */
	cpu_hotplug.workqueue = alloc_workqueue("%s", WQ_HIGHPRI | WQ_UNBOUND |
					WQ_MEM_RECLAIM | WQ_FREEZABLE,
					1, "exynos_cpu_hotplug");
	if (!cpu_hotplug.workqueue)
		return -ENOMEM;

	/* Parse data from device tree */
	cpu_hotplug_dt_init();

	/* Initialize pm_qos request and handler */
	cpu_hotplug_pm_qos_init();

	/* Create sysfs */
	cpu_hotplug_sysfs_init();

	/* register pm notifier */
	register_pm_notifier(&exynos_cpu_hotplug_nb);

	/* Enable cpu_hotplug */
	update_enable_flag(true);

	return 0;
}
arch_initcall(cpu_hotplug_init);
