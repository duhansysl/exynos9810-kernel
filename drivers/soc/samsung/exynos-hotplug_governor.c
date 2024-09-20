/*
 * linux/drivers/exynos/soc/samsung/exynos-hotplug_governor.c
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/irq_work.h>
#include <linux/kobject.h>
#include <linux/cpufreq.h>
#include <linux/exynos-cpufreq.h>
#include <linux/suspend.h>
#include <linux/cpuidle.h>

#include <soc/samsung/exynos-cpu_hotplug.h>
#include <soc/samsung/cal-if.h>

#include <trace/events/power.h>

#include "../../../kernel/sched/sched.h"

#define DEFAULT_BOOT_ENABLE_MS (40000)		/* 40 s */
#define NUM_OF_GROUP	2
#define ATTR_COUNT	14

#define LIT	0
#define BIG	1

enum hpgov_event {
	HPGOV_SLACK_TIMER_EXPIRED = 1,	/* slack timer expired */
};

struct hpgov_attrib {
	struct kobj_attribute	enabled;
	struct kobj_attribute	skip_lit_enabled;
	struct kobj_attribute	ldsum_enabled;
	struct kobj_attribute	single_change_ms;
	struct kobj_attribute	dual_change_ms;
	struct kobj_attribute	quad_change_ms;
	struct kobj_attribute	big_heavy_thr;
	struct kobj_attribute	lit_heavy_thr;
	struct kobj_attribute	big_idle_thr;
	struct kobj_attribute	lit_idle_thr;
	struct kobj_attribute	ldsum_heavy_thr;
	struct kobj_attribute	cl_busy_ratio;
	struct kobj_attribute	user_mode;

	struct attribute_group	attrib_group;
};

struct hpgov_data {
	enum hpgov_event event;
};

struct hpgov_load {
	unsigned long load_sum;
	unsigned long max_load_sum;
};

struct {
	int				enabled;
	int				suspend;
	unsigned int			cur_cpu_min;
	unsigned int			req_cpu_min;
	unsigned int			mode;
	unsigned int			user_mode;
	unsigned int			boostable;

	bool				big_busy;
	int				skip_lit_enabled;

	int				single_change_ms;
	int				dual_change_ms;
	int				quad_change_ms;
	int				big_heavy_thr;
	int				lit_heavy_thr;
	int				big_idle_thr;
	int				lit_idle_thr;
	int				ldsum_heavy_thr;
	int				ldsum_enabled;
	int				change_ms;
	int				cl_busy_ratio;

	unsigned long			pol_max;
	unsigned long			qos_max;

	struct hpgov_attrib		attrib;
	struct mutex			attrib_lock;
	struct task_struct		*task;
	struct irq_work			update_irq_work;

	struct hrtimer			slack_timer;

	struct hpgov_data		data;
	wait_queue_head_t		wait_q;

	int cal_id;
	int maxfreq_table[5];
	struct cpumask big_cpu_mask;

	struct hpgov_load load_info[NUM_OF_GROUP];

} exynos_hpgov;

static struct pm_qos_request hpgov_min_pm_qos;
static struct pm_qos_request hpgov_suspend_pm_qos;

static DEFINE_SPINLOCK(hpgov_lock);
DEFINE_RAW_SPINLOCK(hpgov_load_lock);

enum {
	DISABLE = 0,
	SINGLE = 1,
	DUAL = 2,
	TRIPLE = 3,
	QUAD = 4,
};

void exynos_hpgov_validate_scale(unsigned int cpu, unsigned int target_freq)
{
	struct cpumask mask;
	unsigned int max_freq;
	int cur_mode;

	if (cpumask_test_cpu(cpu, cpu_coregroup_mask(0)) || !exynos_hpgov.enabled)
		return;

	cpumask_and(&mask, cpu_online_mask, cpu_coregroup_mask(4));
	cur_mode = cpumask_weight(&mask);
	max_freq = exynos_hpgov.maxfreq_table[cur_mode];

	if (max_freq < target_freq) {
		pr_info("%s: max_freq %u, target_freq %u, cur_mode %d\n",
			__func__, max_freq, target_freq, cur_mode);
		BUG_ON(true);
	}

	return;
}

void exynos_hpgov_validate_hpin(unsigned int cpu)
{
	struct cpumask mask;
	unsigned int cur_freq, max_freq;
	int next_mode;

	if (cpumask_test_cpu(cpu, cpu_coregroup_mask(0)) || !exynos_hpgov.enabled)
		return;


	cpumask_and(&mask, cpu_online_mask, cpu_coregroup_mask(4));
	next_mode = cpumask_weight(&mask) + 1;
	max_freq = exynos_hpgov.maxfreq_table[next_mode];
	cur_freq = (unsigned int)cal_dfs_get_rate(exynos_hpgov.cal_id);
	pr_info("%s: max_freq %d, cur_freq %d, next_mode %d \n",
			__func__, max_freq, cur_freq, next_mode);

	BUG_ON(max_freq < cur_freq);

	return;
}

/**********************************************************************************/
/*			CPUFREQ for max frequency control			  */
/**********************************************************************************/
static unsigned long get_hpgov_maxfreq(void)
{
	unsigned long max_freq;

	if (!exynos_hpgov.enabled || exynos_hpgov.suspend)
		max_freq = exynos_hpgov.maxfreq_table[QUAD];
	else if (cpumask_weight(&exynos_hpgov.big_cpu_mask) == SINGLE)
		max_freq = exynos_hpgov.maxfreq_table[SINGLE];
	else if (cpumask_weight(&exynos_hpgov.big_cpu_mask) == DUAL)
		max_freq = exynos_hpgov.maxfreq_table[DUAL];
	else
		max_freq = exynos_hpgov.maxfreq_table[QUAD];

	return max_freq;
}

static void exynos_hpgov_set_boostable(void)
{
	unsigned long quad_freq = exynos_hpgov.maxfreq_table[QUAD];

	if ((exynos_hpgov.qos_max <= quad_freq) ||
		exynos_hpgov.pol_max < quad_freq)
		exynos_hpgov.boostable = false;
	else
		exynos_hpgov.boostable = true;
}

static int exynos_hpgov_qos_callback(struct notifier_block *nb,
					unsigned long val, void *v)
{
	exynos_hpgov.qos_max = val;
	exynos_hpgov_set_boostable();

	return NOTIFY_OK;
}

static struct notifier_block hpgov_pm_qos_max_notifier = {
	.notifier_call = exynos_hpgov_qos_callback,
};

static int cpufreq_policy_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned long max_freq;

	if (!policy->cpu)
		return 0;

	switch (event) {
	case CPUFREQ_ADJUST:
		max_freq = get_hpgov_maxfreq();

		if (policy->max != max_freq)
			cpufreq_verify_within_limits(policy, 0, max_freq);
		break;

	case CPUFREQ_NOTIFY:
		exynos_hpgov.pol_max = policy->max;
		exynos_hpgov_set_boostable();
		break;
	}

	return 0;
}
/* Notifier for cpufreq policy change */
static struct notifier_block exynos_hpgov_policy_nb = {
	.notifier_call = cpufreq_policy_notifier,
};

static int exynos_hpgov_max_freq_control(unsigned int cpu)
{
	unsigned long timeout;

	if (exynos_hpgov.suspend)
		return 0;

	/* if cpufreq_update_policy is failed after 50ms, cancel cpu_up */
	timeout = jiffies + msecs_to_jiffies(50);
	while (cpufreq_update_policy(cpu)) {
		if (time_after(jiffies, timeout)) {
			pr_info("%s: failed to control max_freq\n", __func__);
			return -1;
		}
	}

	if (!exynos_cpufreq_allow_change_max(cpu, get_hpgov_maxfreq())) {
		pr_warn("%s: current frequency is higher than max freq\n", __func__);
		return -EBUSY;
	}

	/* update cpu capacity */
	exynos_hpgov_update_cpu_capacity(cpu);

	return 0;
}

static int exynos_hpgov_hp_callback(struct notifier_block *nfb,
					 unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	int big_cpu_cnt;
	struct cpumask mask;

	/* skip little cpu */
	if (cpumask_test_cpu(cpu, cpu_coregroup_mask(0)))
		return NOTIFY_OK;

	switch (action & ~CPU_TASKS_FROZEN) {
	case CPU_UP_PREPARE:
		if (cpumask_test_cpu(cpu, &exynos_hpgov.big_cpu_mask))
			pr_warn("cpuhp: CPU%d was already alive",cpu);

		cpumask_set_cpu(cpu, &exynos_hpgov.big_cpu_mask);
		big_cpu_cnt = cpumask_weight(&exynos_hpgov.big_cpu_mask);
		/*
		 * if policy_update_mask is 1, it means first hotplug in
		 * of fast_cpu domain
		 */
		if (big_cpu_cnt == 1)
			break;

		cpumask_and(&mask, &exynos_hpgov.big_cpu_mask, cpu_online_mask);
		if (exynos_hpgov_max_freq_control(cpumask_first(&mask)))
			return NOTIFY_BAD;
		break;
	case CPU_DEAD:
	case CPU_UP_CANCELED:
		if (!cpumask_test_cpu(cpu, &exynos_hpgov.big_cpu_mask))
			pr_warn("cpuhp: CPU%d was already dead",cpu);

		cpumask_clear_cpu(cpu, &exynos_hpgov.big_cpu_mask);
		big_cpu_cnt = cpumask_weight(&exynos_hpgov.big_cpu_mask);
		/*
		 * if policy_update_mask is 0, it means last hotplug out
		 * of fast_cpu domain
		 */
		if (big_cpu_cnt == 0)
			break;

		if (exynos_hpgov_max_freq_control(cpumask_first(&exynos_hpgov.big_cpu_mask)))
			return NOTIFY_BAD;
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

/**********************************************************************************/
/*			  CPUIDLE control for boosting mode			  */
/**********************************************************************************/
extern void block_cpd(bool enable);
static void exynos_hpgov_control_cpuidle(int req_cpu_min)
{
	block_cpd(req_cpu_min != NR_CPUS);
}

/**********************************************************************************/
/*			HP_GOVERNOR for requestng mode change			  */
/**********************************************************************************/
static void start_slack_timer(void)
{
	if (hrtimer_active(&exynos_hpgov.slack_timer))
		hrtimer_cancel(&exynos_hpgov.slack_timer);

	if (exynos_hpgov.req_cpu_min == exynos_hpgov.cur_cpu_min)
		return;

	hrtimer_start(&exynos_hpgov.slack_timer,
			ktime_add(ktime_get(),
			ktime_set(0, exynos_hpgov.change_ms * NSEC_PER_MSEC)),
			HRTIMER_MODE_PINNED);
}

static enum hrtimer_restart exynos_hpgov_slack_timer(struct hrtimer *timer)
{
	unsigned long flags;

	spin_lock_irqsave(&hpgov_lock, flags);
	exynos_hpgov.data.event = HPGOV_SLACK_TIMER_EXPIRED;
	spin_unlock_irqrestore(&hpgov_lock, flags);

	wake_up(&exynos_hpgov.wait_q);

	return HRTIMER_NORESTART;
}

static void exynos_hpgov_request_mode_change(unsigned int target_mode)
{
	unsigned long flags;

	spin_lock_irqsave(&hpgov_lock, flags);

	if (target_mode == SINGLE)
		exynos_hpgov.change_ms = exynos_hpgov.single_change_ms;
	else if (target_mode == DUAL)
		exynos_hpgov.change_ms = exynos_hpgov.dual_change_ms;
	else
		exynos_hpgov.change_ms = exynos_hpgov.quad_change_ms;

	exynos_hpgov.mode = target_mode;
	exynos_hpgov.req_cpu_min = target_mode + 4;

	spin_unlock_irqrestore(&hpgov_lock, flags);

	irq_work_queue(&exynos_hpgov.update_irq_work);
}

static void exynos_hpgov_irq_work(struct irq_work *irq_work)
{
	start_slack_timer();
}

static int exynos_hpgov_do_update_governor(void *data)
{
	struct hpgov_data *pdata = (struct hpgov_data *)data;
	unsigned long flags;
	long use_fast_hp = FAST_HP;

	while (1) {
		wait_event(exynos_hpgov.wait_q, pdata->event || kthread_should_stop());
		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&hpgov_lock, flags);
		exynos_hpgov.cur_cpu_min = exynos_hpgov.req_cpu_min;
		pdata->event = 0;
		spin_unlock_irqrestore(&hpgov_lock, flags);

		exynos_hpgov_control_cpuidle(exynos_hpgov.cur_cpu_min);
		trace_hpgov_req_hotplug(FAST_HP, exynos_hpgov.cur_cpu_min);
		pm_qos_update_request_param(&hpgov_min_pm_qos,
				exynos_hpgov.cur_cpu_min, (void *)&use_fast_hp);
	}

	return 0;
}

static void __exynos_hpgov_set_disable(void)
{
	if (exynos_hpgov.task)
		kthread_stop(exynos_hpgov.task);

	pm_qos_update_request(&hpgov_min_pm_qos, PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE);

	pr_info("HP_GOV: Stop hotplug governor\n");
}

static void __exynos_hpgov_set_enable(void)
{
	exynos_hpgov.mode = QUAD;
	exynos_hpgov.user_mode = DISABLE;
	exynos_hpgov.req_cpu_min = PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE;
	exynos_hpgov.cur_cpu_min = PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE;

	/* create hp task */
	exynos_hpgov.task = kthread_create(exynos_hpgov_do_update_governor,
			&exynos_hpgov.data, "exynos_hpgov");
	if (IS_ERR(exynos_hpgov.task)) {
		spin_lock(&hpgov_lock);
		exynos_hpgov.enabled--;
		spin_unlock(&hpgov_lock);
		pr_err("HP_GOV: Failed to start hotplug governor\n");
		return;
	}

	set_user_nice(exynos_hpgov.task, MIN_NICE);
	set_cpus_allowed_ptr(exynos_hpgov.task, cpu_coregroup_mask(0));

	if (exynos_hpgov.task)
		wake_up_process(exynos_hpgov.task);

	pr_info("HP_GOV: Start hotplug governor\n");
}

static int exynos_hpgov_set_enable(bool enable)
{
	int enable_cnt;

	spin_lock(&hpgov_lock);
	if (enable)
		exynos_hpgov.enabled++;
	else
		exynos_hpgov.enabled--;
	enable_cnt = exynos_hpgov.enabled;
	smp_wmb();
	spin_unlock(&hpgov_lock);

	if (enable_cnt == 1)
		__exynos_hpgov_set_enable();
	else if (enable_cnt == 0)
		__exynos_hpgov_set_disable();

	return 0;
}

/**********************************************************************************/
/*			Helper function for scheduler				  */
/**********************************************************************************/
int exynos_hpgov_update_cpu_capacity(int cpu)
{
	struct sched_domain *sd;
	struct cpumask mask;

	if (!exynos_hpgov.enabled || exynos_hpgov.suspend)
		return 0;

	if (cpumask_test_cpu(cpu, cpu_coregroup_mask(0)))
		return 0;

	rcu_read_lock();

	cpumask_xor(&mask, cpu_possible_mask, cpu_coregroup_mask(0));
	cpumask_and(&mask, &mask, cpu_active_mask);
	if (cpumask_empty(&mask))
		goto exit;

	cpu = (int) cpumask_first(&mask);
	sd = rcu_dereference(per_cpu(sd_ea, cpu));
	if (!sd)
		goto exit;

	sd = sd->child;
	update_group_capacity(sd, cpu);

exit:
	rcu_read_unlock();

	return 0;
}

static void exynos_hpgov_update_avg_cl_load(struct sched_group *sg,
					struct hpgov_load *hpgov_load)
{
	struct cpumask mask;
	int cpu, num;
	hpgov_load->max_load_sum = 0;;
	hpgov_load->load_sum = 0;

	cpumask_and(&mask, cpu_online_mask, sched_group_cpus(sg));
	num = cpumask_weight(&mask);
	if (!num)
		return;

	for_each_cpu_and(cpu, sched_group_cpus(sg), cpu_online_mask) {
		struct rq *rq = cpu_rq(cpu);
		struct sched_avg *sa = &rq->cfs.avg;
		unsigned long load_sum;

		hpgov_load->max_load_sum += (rq->cpu_capacity_orig / num);
		if (sa->util_avg > rq->cpu_capacity_orig)
			load_sum = rq->cpu_capacity_orig;
		else
			load_sum = sa->util_avg;

		hpgov_load->load_sum += (load_sum / num);
		trace_hpgov_cpu_load(cpu, sa->util_avg, rq->cpu_capacity_orig);
	}

	trace_hpgov_cluster_load(hpgov_load->load_sum, hpgov_load->max_load_sum);
}

static int exynos_hpgov_update_sysload_info(void)
{
	struct sched_group *sg;
	struct sched_domain *sd;
	int num = 0;

	rcu_read_lock();

	sd = rcu_dereference(per_cpu(sd_ea, 0));
	if (!sd) {
		rcu_read_unlock();
		return 1; /* Error */
	}

	sg = sd->groups;
	do {
		exynos_hpgov_update_avg_cl_load(sg, &exynos_hpgov.load_info[num]);
		num++;
	} while (sg = sg->next, sg != sd->groups);

	rcu_read_unlock();

	return 0;
}

static bool is_cluster_busy(int cluster, int ratio)
{
	bool busy = false;

	if (exynos_hpgov.load_info[cluster].load_sum
		> ((exynos_hpgov.load_info[cluster].max_load_sum * ratio) / 100))
		busy = true;

	if (cluster == BIG)
		exynos_hpgov.big_busy = busy;

	trace_hpgov_cluster_busy(cluster, ratio,
		exynos_hpgov.load_info[cluster].load_sum,
		exynos_hpgov.load_info[cluster].max_load_sum, busy);

	return busy;
}

/* get the cluster sched group */
static struct sched_group * get_cl_group(unsigned int cl)
{
	int i;
	struct sched_domain *sd;
	struct sched_group *sg;

	sd = rcu_dereference(per_cpu(sd_ea, 0));
	if (!sd)
		return NULL;

	sg = sd->groups;
	for (i = 0; i < cl; i++)
		sg = sd->groups->next;

	return sg;
}

static int clamp_thr(int thr, bool heavy)
{
	int margin = heavy ? 9 : 11;

	return exynos_hpgov.mode != QUAD ? (thr * margin / 10) : thr;
}

/* return number of imbalance heavy cpu */
static int exynos_hpgov_get_imbal_cpus(unsigned int cluster)
{
	int cpu, imbal_heavy_cnt = 0;
	int heavy_cnt = 0, active_cnt = 0, idle_cnt = 0, ldsum_heavy_cnt = 0;
	struct sched_group *sg = get_cl_group(cluster);
	int heavy_thr, idle_thr, ldsum_heavy_thr = 1;
	unsigned int active_util = 0;

	if (exynos_hpgov.skip_lit_enabled &&
			cluster == LIT && !exynos_hpgov.big_busy)
		return 0;

	if (!sg || !cpumask_weight(sched_group_cpus(sg)))
		return 0;

	/* get the min capaicity of cpu */
	if (cluster == LIT) {
		heavy_thr = clamp_thr(exynos_hpgov.lit_heavy_thr, 1);
		idle_thr = clamp_thr(exynos_hpgov.lit_idle_thr, 0);
	} else {
		heavy_thr = clamp_thr(exynos_hpgov.big_heavy_thr, 1);
		idle_thr = clamp_thr(exynos_hpgov.big_idle_thr, 0);
		ldsum_heavy_thr = clamp_thr(exynos_hpgov.ldsum_heavy_thr, 0);
	}

	for_each_cpu(cpu, cpu_coregroup_mask(cpumask_first(sched_group_cpus(sg)))) {
		struct rq *rq = cpu_rq(cpu);
		struct sched_avg *sa = &rq->cfs.avg;

		if (!cpumask_test_cpu(cpu, cpu_online_mask)) {
			idle_cnt++;
			continue;
		}

		if (sa->util_avg >= heavy_thr)
			heavy_cnt++;
		else if (cluster != LIT && sa->util_avg >= idle_thr) {
			active_cnt++;
			active_util += sa->util_avg;
		} else
			idle_cnt++;
	}

	if (!heavy_cnt)
		goto exit;

	if (exynos_hpgov.ldsum_enabled && cluster == BIG && active_util) {
		ldsum_heavy_cnt = (active_util / ldsum_heavy_thr) + 1;

		trace_hpgov_load_sum(active_util,
				ldsum_heavy_thr, active_cnt, ldsum_heavy_cnt);

		active_cnt = ldsum_heavy_cnt;
	}

	imbal_heavy_cnt = heavy_cnt + active_cnt;

exit:
	trace_hpgov_load_imbalance(cluster, idle_thr, heavy_thr,
				idle_cnt, active_cnt, imbal_heavy_cnt);

	return imbal_heavy_cnt;
}

static bool exynos_hpgov_system_busy(void)
{
	if (is_cluster_busy(BIG, exynos_hpgov.cl_busy_ratio)
		&& is_cluster_busy(LIT, exynos_hpgov.cl_busy_ratio))
			return true;
	return false;
}

static bool exynos_hpgov_change_quad(void)
{
	int heavy_cnt;

	/* If system is busy, change quad mode */
	if (exynos_hpgov_system_busy())
		return true;

	heavy_cnt = exynos_hpgov_get_imbal_cpus(LIT) +
			exynos_hpgov_get_imbal_cpus(BIG);

	if ((heavy_cnt > DUAL) || !heavy_cnt)
		return true;

	return false;
}

static bool exynos_hpgov_change_dual(void)
{
	int big_heavy_cnt, lit_heavy_cnt;

	/* If system is busy, doesn't change boost mode */
	if (exynos_hpgov_system_busy())
		return false;

	lit_heavy_cnt = exynos_hpgov_get_imbal_cpus(LIT);
	big_heavy_cnt = exynos_hpgov_get_imbal_cpus(BIG);
	if ((big_heavy_cnt == DUAL && !lit_heavy_cnt) ||
		(big_heavy_cnt == SINGLE && lit_heavy_cnt == SINGLE))
		return true;

	return false;
}

static bool exynos_hpgov_change_single(void)
{
	/* If system is busy, doesn't change boost mode */
	if (exynos_hpgov_system_busy())
		return false;

	if ((exynos_hpgov_get_imbal_cpus(BIG) == SINGLE)
			&& !exynos_hpgov_get_imbal_cpus(LIT))
		return true;

	return false;
}

static unsigned int exynos_hpgov_get_mode(unsigned int cur_mode)
{
	unsigned int target_mode = 0;

	if (exynos_hpgov.user_mode)
		return exynos_hpgov.user_mode;

	if (!exynos_hpgov.boostable)
		return QUAD;

	switch(cur_mode) {
	case SINGLE:
		if (exynos_hpgov_change_quad())
			target_mode = QUAD;
		else if (exynos_hpgov_change_dual())
			target_mode = DUAL;
		break;
	case DUAL:
		if (exynos_hpgov_change_quad())
			target_mode = QUAD;
		else if (exynos_hpgov_change_single())
			target_mode = SINGLE;
		break;
	case QUAD:
		if (exynos_hpgov_change_dual())
			target_mode = DUAL;
		else if (exynos_hpgov_change_single())
			target_mode = SINGLE;
		break;
	}

	if (!target_mode)
		target_mode = cur_mode;

	return target_mode;
}


static int exynos_hpgov_update_mode(void)
{
	unsigned int target_mode, cur_mode;

	rcu_read_lock();

	cur_mode = exynos_hpgov.mode;
	target_mode = exynos_hpgov_get_mode(cur_mode);
	trace_hpgov_update_mode(cur_mode, target_mode);

	if (target_mode == cur_mode)
		goto exit;

	exynos_hpgov_request_mode_change(target_mode);
exit:
	rcu_read_unlock();
	return 0;
}

void exynos_hpgov_update_rq_load(int cpu)
{
	unsigned long flags;

	if (!exynos_hpgov.enabled || exynos_hpgov.suspend)
		return;

	/* little cpus sikp updating bt mode */
	if (cpumask_test_cpu(cpu, cpu_coregroup_mask(0)))
		return;

	if (!raw_spin_trylock_irqsave(&hpgov_load_lock, flags))
		return;

	/* update sysload */
	if (exynos_hpgov_update_sysload_info())
		goto exit;

	/* update hpgov mode */
	exynos_hpgov_update_mode();

exit:
	raw_spin_unlock_irqrestore(&hpgov_load_lock, flags);
}

/**********************************************************************************/
/*					SYSFS 					  */
/**********************************************************************************/
static int exynos_hpgov_set_enabled(int val)
{
	if (val > 0)
		return exynos_hpgov_set_enable(true);
	else
		return exynos_hpgov_set_enable(false);

	return 0;
}

static int exynos_hpgov_set_user_mode(int val)
{
	if (val == DISABLE)
		exynos_hpgov.user_mode = DISABLE;
	else if (val == QUAD)
		exynos_hpgov.user_mode = QUAD;
	else if (val == DUAL)
		exynos_hpgov.user_mode = DUAL;
	else if (val == SINGLE)
		exynos_hpgov.user_mode = SINGLE;
	else
		pr_info("%s: input invalied value \n", __func__);

	return 0;
}

static int exynos_hpgov_set_cl_busy_ratio(int val)
{
	if (!(val >= 0))
		return -EINVAL;

	exynos_hpgov.cl_busy_ratio = val;

	return 0;
}

static int exynos_hpgov_set_big_heavy_thr(int val)
{
	if (!(val > 0))
		return -EINVAL;

	exynos_hpgov.big_heavy_thr = val;

	return 0;
}

static int exynos_hpgov_set_lit_heavy_thr(int val)
{
	if (!(val > 0))
		return -EINVAL;

	exynos_hpgov.lit_heavy_thr = val;

	return 0;
}

static int exynos_hpgov_set_big_idle_thr(int val)
{
	if (!(val > 0))
		return -EINVAL;

	exynos_hpgov.big_idle_thr = val;

	return 0;
}

static int exynos_hpgov_set_lit_idle_thr(int val)
{
	if (!(val > 0))
		return -EINVAL;

	exynos_hpgov.lit_idle_thr = val;

	return 0;
}

static int exynos_hpgov_set_ldsum_heavy_thr(int val)
{
	if (!(val > 0))
		return -EINVAL;

	exynos_hpgov.ldsum_heavy_thr = val;

	return 0;
}

static int exynos_hpgov_set_ldsum_enabled(int val)
{
	exynos_hpgov.ldsum_enabled = val;
	return 0;
}

static int exynos_hpgov_set_skip_lit_enabled(int val)
{
	exynos_hpgov.skip_lit_enabled = val;
	return 0;
}

static int exynos_hpgov_set_quad_change_ms(int val)
{
	if (!(val >= 0))
		return -EINVAL;

	exynos_hpgov.quad_change_ms = val;

	return 0;
}

static int exynos_hpgov_set_dual_change_ms(int val)
{
	if (!(val >= 0))
		return -EINVAL;

	exynos_hpgov.dual_change_ms = val;

	return 0;
}

static int exynos_hpgov_set_single_change_ms(int val)
{
	if (!(val >= 0))
		return -EINVAL;

	exynos_hpgov.single_change_ms = val;

	return 0;
}


#define HPGOV_PARAM(_name, _param) \
static ssize_t exynos_hpgov_attr_##_name##_show(struct kobject *kobj, \
			struct kobj_attribute *attr, char *buf) \
{ \
	return snprintf(buf, PAGE_SIZE, "%d\n", _param); \
} \
static ssize_t exynos_hpgov_attr_##_name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, const char *buf, size_t count) \
{ \
	int ret = 0; \
	int val; \
	int old_val; \
	mutex_lock(&exynos_hpgov.attrib_lock); \
	ret = kstrtoint(buf, 10, &val); \
	if (ret) { \
		pr_err("Invalid input %s for %s %d\n", \
				buf, __stringify(_name), ret);\
		return 0; \
	} \
	old_val = _param; \
	ret = exynos_hpgov_set_##_name(val); \
	if (ret) { \
		pr_err("Error %d returned when setting param %s to %d\n",\
				ret, __stringify(_name), val); \
		_param = old_val; \
	} \
	mutex_unlock(&exynos_hpgov.attrib_lock); \
	return count; \
}

#define HPGOV_RW_ATTRIB(i, _name) \
	exynos_hpgov.attrib._name.attr.name = __stringify(_name); \
	exynos_hpgov.attrib._name.attr.mode = S_IRUGO | S_IWUSR; \
	exynos_hpgov.attrib._name.show = exynos_hpgov_attr_##_name##_show; \
	exynos_hpgov.attrib._name.store = exynos_hpgov_attr_##_name##_store; \
	exynos_hpgov.attrib.attrib_group.attrs[i] = &exynos_hpgov.attrib._name.attr;

HPGOV_PARAM(enabled, exynos_hpgov.enabled);
HPGOV_PARAM(user_mode, exynos_hpgov.user_mode);
HPGOV_PARAM(big_heavy_thr, exynos_hpgov.big_heavy_thr);
HPGOV_PARAM(lit_heavy_thr, exynos_hpgov.lit_heavy_thr);
HPGOV_PARAM(big_idle_thr, exynos_hpgov.big_idle_thr);
HPGOV_PARAM(lit_idle_thr, exynos_hpgov.lit_idle_thr);
HPGOV_PARAM(ldsum_heavy_thr, exynos_hpgov.ldsum_heavy_thr);
HPGOV_PARAM(ldsum_enabled, exynos_hpgov.ldsum_enabled);
HPGOV_PARAM(skip_lit_enabled, exynos_hpgov.skip_lit_enabled);
HPGOV_PARAM(cl_busy_ratio, exynos_hpgov.cl_busy_ratio);
HPGOV_PARAM(single_change_ms, exynos_hpgov.single_change_ms);
HPGOV_PARAM(dual_change_ms, exynos_hpgov.dual_change_ms);
HPGOV_PARAM(quad_change_ms, exynos_hpgov.quad_change_ms);

static void hpgov_boot_enable(struct work_struct *work);
static DECLARE_DELAYED_WORK(hpgov_boot_work, hpgov_boot_enable);
static void hpgov_boot_enable(struct work_struct *work)
{
	exynos_hpgov_set_enable(true);
}

static int exynos_hp_gov_pm_suspend_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	int online_cnt;
	unsigned long timeout;
	long use_fast_hp = FAST_HP;

	if (pm_event != PM_SUSPEND_PREPARE)
		return NOTIFY_OK;

	pm_qos_update_request_param(&hpgov_suspend_pm_qos,
			PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE, (void *)&use_fast_hp);

	online_cnt = min(pm_qos_request(PM_QOS_CPU_ONLINE_MIN),
				pm_qos_request(PM_QOS_CPU_ONLINE_MAX));

	timeout = jiffies + msecs_to_jiffies(10000);
	/* waiting for qoad mode */
	if (online_cnt > cpumask_weight(cpu_coregroup_mask(0)))
		while (cpumask_weight(cpu_online_mask) < online_cnt) {
			msleep(3);
			if (time_after(jiffies, timeout))
				BUG_ON(1);
		}

	exynos_hpgov.suspend = true;

	return NOTIFY_OK;
}

static int exynos_hp_gov_pm_resume_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	if (pm_event != PM_POST_SUSPEND)
		return NOTIFY_OK;

	exynos_hpgov.suspend = false;
	pm_qos_update_request(&hpgov_suspend_pm_qos,
			PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE);

	return NOTIFY_OK;
}

static struct notifier_block exynos_hp_gov_suspend_nb = {
	.notifier_call = exynos_hp_gov_pm_suspend_notifier,
	.priority = INT_MAX,
};

static struct notifier_block exynos_hp_gov_resume_nb = {
	.notifier_call = exynos_hp_gov_pm_resume_notifier,
	.priority = INT_MIN,
};

static int __init exynos_hpgov_parse_dt(void)
{
	int i, freq, max_freq;
	struct device_node *np = of_find_node_by_name(NULL, "hotplug_governor");

	if (of_property_read_u32(np, "single_change_ms", &exynos_hpgov.single_change_ms))
		goto exit;

	if (of_property_read_u32(np, "dual_change_ms", &exynos_hpgov.dual_change_ms))
		goto exit;

	if (of_property_read_u32(np, "quad_change_ms", &exynos_hpgov.quad_change_ms))
		goto exit;

	if (of_property_read_u32(np, "big_heavy_thr", &exynos_hpgov.big_heavy_thr))
		goto exit;

	if (of_property_read_u32(np, "lit_heavy_thr", &exynos_hpgov.lit_heavy_thr))
		goto exit;

	if (of_property_read_u32(np, "big_idle_thr", &exynos_hpgov.big_idle_thr))
		goto exit;

	if (of_property_read_u32(np, "lit_idle_thr", &exynos_hpgov.lit_idle_thr))
		goto exit;

	if (of_property_read_u32(np, "ldsum_heavy_thr", &exynos_hpgov.ldsum_heavy_thr))
		goto exit;

	if (of_property_read_u32(np, "ldsum_enabled", &exynos_hpgov.ldsum_enabled))
		goto exit;

	if (of_property_read_u32(np, "skip_lit_enabled", &exynos_hpgov.skip_lit_enabled))
		goto exit;

	if (of_property_read_u32(np, "cl_busy_ratio", &exynos_hpgov.cl_busy_ratio))
		goto exit;

	if (of_property_read_u32(np, "cal-id", &exynos_hpgov.cal_id))
		goto exit;
	max_freq = (int)cal_dfs_get_max_freq(exynos_hpgov.cal_id);
	if (!max_freq)
		goto exit;
	exynos_hpgov.maxfreq_table[SINGLE] = max_freq;

	if (of_property_read_u32(np, "dual_freq", &freq))
		goto exit;
	exynos_hpgov.maxfreq_table[DUAL] = min(freq, max_freq);

	if (of_property_read_u32(np, "triple_freq", &freq))
		goto exit;
	exynos_hpgov.maxfreq_table[TRIPLE] = min(freq, max_freq);

	if (of_property_read_u32(np, "quad_freq", &freq))
		goto exit;
	exynos_hpgov.maxfreq_table[QUAD] = min(freq, max_freq);

	exynos_hpgov.maxfreq_table[DISABLE] = exynos_hpgov.maxfreq_table[QUAD];

	for (i = 0; i <= QUAD; i++)
		pr_info("HP_GOV: mode %d: max_freq = %d\n",
				i, exynos_hpgov.maxfreq_table[i]);

	return 0;

exit:
	pr_warn("HP_GOV: faield to parse dt!\n");
	return -EINVAL;
}

extern unsigned long sec_cpumask;
static int __init exynos_hpgov_boostable(void)
{
	return (exynos_hpgov.maxfreq_table[SINGLE]
			> exynos_hpgov.maxfreq_table[QUAD]);
}

extern struct cpumask early_cpu_mask;
static int __init exynos_hpgov_init(void)
{
	int ret = 0;
	int attr_count = ATTR_COUNT;
	int i_attr = attr_count;

	/* if user controls core number by boot arg, disable hpgov */
	if (cpumask_weight(&early_cpu_mask) != NR_CPUS) {
		pr_info("HP_GOV initialization is canceled by core ccontrol\n");
		goto failed;
	}

	/* parse DT */
	if (exynos_hpgov_parse_dt())
		goto failed;

	/* start hp_governor after BOOT_ENAMBE_MS */
	if (!exynos_hpgov_boostable()) {
		pr_info("HP_GOV initialization is canceled by boost frequency\n");
		goto failed;
	}

	/* init timer */
	hrtimer_init(&exynos_hpgov.slack_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_PINNED);
	exynos_hpgov.slack_timer.function = exynos_hpgov_slack_timer;

	/* init mutex */
	mutex_init(&exynos_hpgov.attrib_lock);

	/* init workqueue */
	init_waitqueue_head(&exynos_hpgov.wait_q);
	init_irq_work(&exynos_hpgov.update_irq_work, exynos_hpgov_irq_work);

	/* register sysfs */
	exynos_hpgov.attrib.attrib_group.attrs =
		kzalloc(attr_count * sizeof(struct attribute *), GFP_KERNEL);
	if (!exynos_hpgov.attrib.attrib_group.attrs) {
		ret = -ENOMEM;
		goto failed;
	}

	HPGOV_RW_ATTRIB(attr_count - (i_attr--), enabled);
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), big_heavy_thr);
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), lit_heavy_thr);
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), big_idle_thr);
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), lit_idle_thr);
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), ldsum_heavy_thr);
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), ldsum_enabled);
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), skip_lit_enabled);
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), cl_busy_ratio);
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), user_mode);
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), single_change_ms);
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), dual_change_ms);
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), quad_change_ms);

	exynos_hpgov.attrib.attrib_group.name = "governor";
	ret = sysfs_create_group(exynos_cpu_hotplug_kobj(),
				&exynos_hpgov.attrib.attrib_group);
	if (ret) {
		pr_err("Unable to create sysfs objects :%d\n", ret);
		goto failed;
	}

	/* set default valuse */
	exynos_hpgov.pol_max = ULONG_MAX;
	exynos_hpgov.qos_max = ULONG_MAX;
	exynos_hpgov.enabled = 0;
	exynos_hpgov.mode = QUAD;
	exynos_hpgov.user_mode = DISABLE;
	exynos_hpgov.boostable = true;
	exynos_hpgov.req_cpu_min = PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE;
	exynos_hpgov.cur_cpu_min = PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE;

	/* Add pm qos handler for hotplug */
	pm_qos_add_request(&hpgov_min_pm_qos, PM_QOS_CPU_ONLINE_MIN,
					PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE);
	pm_qos_update_request_param(&hpgov_min_pm_qos,
			PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE, NULL);

	pm_qos_add_request(&hpgov_suspend_pm_qos, PM_QOS_CPU_ONLINE_MIN,
					PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE);

	/* register cpu frequency control noti */
	cpufreq_register_notifier(&exynos_hpgov_policy_nb,
					CPUFREQ_POLICY_NOTIFIER);

	__hotcpu_notifier(exynos_hpgov_hp_callback, INT_MAX-1);

	cpumask_clear(&exynos_hpgov.big_cpu_mask);
	cpumask_xor(&exynos_hpgov.big_cpu_mask,
			cpu_possible_mask, cpu_coregroup_mask(0));
	cpufreq_update_policy(cpumask_first(&exynos_hpgov.big_cpu_mask));

	cpumask_and(&exynos_hpgov.big_cpu_mask,
			&exynos_hpgov.big_cpu_mask, cpu_online_mask);

	/* register pm notifier */
	register_pm_notifier(&exynos_hp_gov_suspend_nb);
	register_pm_notifier(&exynos_hp_gov_resume_nb);

	/* register pm qos notifier */
	pm_qos_add_notifier(PM_QOS_CLUSTER1_FREQ_MAX,
				&hpgov_pm_qos_max_notifier);

	schedule_delayed_work_on(0, &hpgov_boot_work,
			msecs_to_jiffies(DEFAULT_BOOT_ENABLE_MS));

	exynos_cpu_hotplug_gov_activated();

	return ret;

failed:
	exynos_hpgov.enabled = 0;
	exynos_hpgov.user_mode = DISABLE;
	pr_info("HP_GOV: failed to initialized HOTPLUG_GOVERNOR (ret: %d)!\n", ret);

	return ret;
}
late_initcall(exynos_hpgov_init);
