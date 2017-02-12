/*
 * Exynos scheduler for Heterogeneous Multi-Processing (HMP)
 *
 * Copyright (C) 2017 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/sched.h>
#include <linux/cpuidle.h>
#include <linux/pm_qos.h>
#include <linux/ehmp.h>
#include <linux/sched_energy.h>

#define CREATE_TRACE_POINTS
#include <trace/events/ehmp.h>

#include "sched.h"
#include "tune.h"

unsigned long task_util(struct task_struct *p)
{
	if (rt_task(p))
		return p->rt.avg.util_avg;
	else
		return p->se.avg.util_avg;
}

static inline struct task_struct *task_of(struct sched_entity *se)
{
	return container_of(se, struct task_struct, se);
}

static inline struct sched_entity *se_of(struct sched_avg *sa)
{
	return container_of(sa, struct sched_entity, avg);
}

#define entity_is_cfs_rq(se)	(se->my_q)
#define entity_is_task(se)	(!se->my_q)
#define LOAD_AVG_MAX		47742

static unsigned long maxcap_val = 1024;
static int maxcap_cpu = 0;

void ehmp_update_max_cpu_capacity(int cpu, unsigned long val)
{
	maxcap_cpu = cpu;
	maxcap_val = val;
}

static inline struct device_node *get_ehmp_node(void)
{
	return of_find_node_by_path("/cpus/ehmp");
}

/**********************************************************************
 * task initialization                                                *
 **********************************************************************/
void attach_entity_cfs_rq(struct sched_entity *se);

void exynos_init_entity_util_avg(struct sched_entity *se)
{
	struct cfs_rq *cfs_rq = se->cfs_rq;
	struct sched_avg *sa = &se->avg;
	int cpu = cpu_of(cfs_rq->rq);
	unsigned long cap_org = capacity_orig_of(cpu);
	long cap = (long)(cap_org - cfs_rq->avg.util_avg);

	if (cap > 0) {
		if (cfs_rq->avg.util_avg != 0) {
			sa->util_avg  = cfs_rq->avg.util_avg * se->load.weight;
			sa->util_avg /= (cfs_rq->avg.load_avg + 1);
			sa->util_avg = sa->util_avg << 1;

			if (sa->util_avg > cap)
				sa->util_avg = cap;
		} else {
			sa->util_avg = cap_org >> 2;
		}
		/*
		 * If we wish to restore tuning via setting initial util,
		 * this is where we should do it.
		 */
		sa->util_sum = sa->util_avg * LOAD_AVG_MAX;
	}

	if (entity_is_task(se)) {
		struct task_struct *p = task_of(se);
		if (p->sched_class != &fair_sched_class) {
			/*
			 * For !fair tasks do:
			 *
			update_cfs_rq_load_avg(now, cfs_rq, false);
			attach_entity_load_avg(cfs_rq, se);
			switched_from_fair(rq, p);
			 *
			 * such that the next switched_to_fair() has the
			 * expected state.
			 */
			se->avg.last_update_time = rq_clock_task(cfs_rq->rq);
			return;
		}
	}

	attach_entity_cfs_rq(se);
}

/**********************************************************************
 * load balance                                                       *
 **********************************************************************/
bool cpu_overutilized(int cpu);

#define lb_sd_parent(sd) \
	(sd->parent && sd->parent->groups != sd->parent->groups->next)

static inline int
check_cpu_capacity(struct rq *rq, struct sched_domain *sd)
{
	return ((rq->cpu_capacity * sd->imbalance_pct) <
				(rq->cpu_capacity_orig * 100));
}

unsigned long global_boost(void);
int exynos_need_active_balance(enum cpu_idle_type idle, struct sched_domain *sd,
					int src_cpu, int dst_cpu)
{
	unsigned int src_imb_pct = lb_sd_parent(sd) ? sd->imbalance_pct : 1;
	unsigned int dst_imb_pct = lb_sd_parent(sd) ? 100 : 1;
	unsigned long src_cap = capacity_of(src_cpu);
	unsigned long dst_cap = capacity_of(dst_cpu);

	if ((idle != CPU_NOT_IDLE) &&
	    (cpu_rq(src_cpu)->cfs.h_nr_running == 1)) {
		if ((check_cpu_capacity(cpu_rq(src_cpu), sd)) &&
		    (src_cap * sd->imbalance_pct < dst_cap * 100)) {
			return 1;
		}

		if (!lb_sd_parent(sd) && src_cap < dst_cap)
			if (cpu_overutilized(src_cpu) || global_boost())
				return 1;
	}

	if ((src_cap * src_imb_pct < dst_cap * dst_imb_pct) &&
			cpu_rq(src_cpu)->cfs.h_nr_running == 1 &&
			cpu_overutilized(src_cpu) &&
			!cpu_overutilized(dst_cpu)) {
		return 1;
	}

	return unlikely(sd->nr_balance_failed > sd->cache_nice_tries + 2);
}

/**********************************************************************
 * load balance_trigger			                              *
 **********************************************************************/
struct lbt_overutil {
	/*
	 * overutil_ratio means
	 * N < 0  : disable user_overutilized
	 * N == 0 : Always overutilized
	 * N > 0  : overutil_cap = org_capacity * overutil_ratio / 100
	 */
	unsigned long overutil_cap;
	int overutil_ratio;
};

DEFINE_PER_CPU(struct lbt_overutil, ehmp_bot_overutil);
DEFINE_PER_CPU(struct lbt_overutil, ehmp_top_overutil);
#define DISABLE_OU	-1

bool cpu_overutilized(int cpu)
{
	struct lbt_overutil *ou = &per_cpu(ehmp_top_overutil, cpu);

	/*
	 * If top overutil is disabled, use main stream condition
	 * in the fair.c
	 */
	if (ou->overutil_ratio == DISABLE_OU)
		return (capacity_of(cpu) * capacity_orig_of(cpu)) <
			(cpu_util(cpu) * capacity_margin_of(cpu));

	return cpu_util(cpu) > ou->overutil_cap;
}

static bool inline lbt_top_overutilized(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	return sched_feat(ENERGY_AWARE) && rq->rd->overutilized;
}

static bool inline lbt_bot_overutilized(int cpu)
{
	struct lbt_overutil *ou = &per_cpu(ehmp_bot_overutil, cpu);

	/* if bot overutil is disabled, return false */
	if (ou->overutil_ratio == DISABLE_OU)
		return false;

	return cpu_util(cpu) > ou->overutil_cap;
}

static void inline lbt_update_overutilized(int cpu,
			unsigned long capacity, bool top)
{
	struct lbt_overutil *ou;
	ou = top ? &per_cpu(ehmp_top_overutil, cpu) :
			&per_cpu(ehmp_bot_overutil, cpu);

	if (ou->overutil_ratio == DISABLE_OU)
		ou->overutil_cap = 0;
	else
		ou->overutil_cap = (capacity * ou->overutil_ratio) / 100;
}

void ehmp_update_overutilized(int cpu, unsigned long capacity)
{
	lbt_update_overutilized(cpu, capacity, true);
	lbt_update_overutilized(cpu, capacity, false);
}

static bool lbt_is_same_group(int src_cpu, int dst_cpu)
{
	struct sched_domain *sd  = rcu_dereference(per_cpu(sd_ea, src_cpu));
	struct sched_group *sg;

	if (!sd)
		return false;

	sg = sd->groups;
	return cpumask_test_cpu(dst_cpu, sched_group_cpus(sg));
}

static bool lbt_overutilized(int src_cpu, int dst_cpu)
{
	bool top_overutilized, bot_overutilized;

	/* src and dst are in the same domain, check top_overutilized */
	top_overutilized = lbt_top_overutilized(src_cpu);
	if (!lbt_is_same_group(src_cpu, dst_cpu))
		return top_overutilized;

	/* check bot overutilized */
	bot_overutilized = lbt_bot_overutilized(src_cpu);
	return bot_overutilized || top_overutilized;
}

static ssize_t _show_overutil(char *buf, bool top)
{
	struct sched_domain *sd;
	struct sched_group *sg;
	struct lbt_overutil *ou;
	int cpu, ret = 0;

	rcu_read_lock();

	sd = rcu_dereference(per_cpu(sd_ea, 0));
	if (!sd) {
		rcu_read_unlock();
		return ret;
	}

	sg = sd->groups;
	do {
		for_each_cpu_and(cpu, sched_group_cpus(sg), cpu_active_mask) {
			ou = top ? &per_cpu(ehmp_top_overutil, cpu) :
						&per_cpu(ehmp_bot_overutil, cpu);
			ret += sprintf(buf + ret, "cpu%d ratio:%3d cap:%4lu\n",
					cpu, ou->overutil_ratio, ou->overutil_cap);

		}
	} while (sg = sg->next, sg != sd->groups);

	rcu_read_unlock();
	return ret;
}

static ssize_t _store_overutil(const char *buf,
				size_t count, bool top)
{
	struct sched_domain *sd;
	struct sched_group *sg;
	struct lbt_overutil *ou;
	unsigned long capacity;
	int cpu;
	const char *cp = buf;
	int tokenized_data;

	rcu_read_lock();

	sd = rcu_dereference(per_cpu(sd_ea, 0));
	if (!sd) {
		rcu_read_unlock();
		return count;
	}

	sg = sd->groups;
	do {
		if (sscanf(cp, "%d", &tokenized_data) != 1)
			tokenized_data = -1;

		for_each_cpu_and(cpu, sched_group_cpus(sg), cpu_active_mask) {
			ou = top ? &per_cpu(ehmp_top_overutil, cpu) :
					&per_cpu(ehmp_bot_overutil, cpu);
			ou->overutil_ratio = tokenized_data;

			capacity = arch_scale_cpu_capacity(sd, cpu);
			ehmp_update_overutilized(cpu, capacity);
		}

		cp = strpbrk(cp, " :");
		if (!cp)
			break;
		cp++;
	} while (sg = sg->next, sg != sd->groups);

	rcu_read_unlock();
	return count;
}

static ssize_t show_top_overutil(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return _show_overutil(buf, true);
}
static ssize_t store_top_overutil(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	return _store_overutil(buf, count, true);
}
static ssize_t show_bot_overutil(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return _show_overutil(buf, false);
}
static ssize_t store_bot_overutil(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	return _store_overutil(buf, count, false);
}

static struct kobj_attribute top_overutil_attr =
__ATTR(top_overutil, 0644, show_top_overutil, store_top_overutil);
static struct kobj_attribute bot_overutil_attr =
__ATTR(bot_overutil, 0644, show_bot_overutil, store_bot_overutil);

static int __init init_lbt(void)
{
	struct device_node *dn;
	int top_ou[NR_CPUS] = {-1, }, bot_ou[NR_CPUS] = {-1, };
	int cpu;

	dn = get_ehmp_node();
	if (!dn)
		return 0;

	if (of_property_read_u32_array(dn, "top-overutil", top_ou, NR_CPUS) < 0)
		return 0;

	if (of_property_read_u32_array(dn, "bot-overutil", bot_ou, NR_CPUS) < 0)
		return 0;

	for_each_possible_cpu(cpu) {
		per_cpu(ehmp_top_overutil, cpu).overutil_ratio = top_ou[cpu];
		per_cpu(ehmp_bot_overutil, cpu).overutil_ratio = bot_ou[cpu];
	}

	return 0;
}
pure_initcall(init_lbt);

bool ehmp_trigger_lb(int src_cpu, int dst_cpu)
{
	/* check overutilized condition */
	return lbt_overutilized(src_cpu, dst_cpu);
}

/**********************************************************************
 * Global boost                                                       *
 **********************************************************************/
static unsigned long gb_value = 0;
static unsigned long gb_max_value = 0;
static struct gb_qos_request gb_req_user =
{
	.name = "ehmp_gb_req_user",
};

static struct plist_head gb_list = PLIST_HEAD_INIT(gb_list);

static DEFINE_SPINLOCK(gb_lock);

static int gb_qos_max_value(void)
{
	return plist_last(&gb_list)->prio;
}

static int gb_qos_req_value(struct gb_qos_request *req)
{
	return req->node.prio;
}

void gb_qos_update_request(struct gb_qos_request *req, u32 new_value)
{
	unsigned long flags;

	if (req->node.prio == new_value)
		return;

	spin_lock_irqsave(&gb_lock, flags);

	if (req->active)
		plist_del(&req->node, &gb_list);
	else
		req->active = 1;

	plist_node_init(&req->node, new_value);
	plist_add(&req->node, &gb_list);

	gb_value = gb_max_value * gb_qos_max_value() / 100;
	trace_ehmp_global_boost(req->name, new_value);

	spin_unlock_irqrestore(&gb_lock, flags);
}

static ssize_t show_global_boost(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct gb_qos_request *req;
	int ret = 0;

	plist_for_each_entry(req, &gb_list, node)
		ret += snprintf(buf + ret, 30, "%s : %d\n",
				req->name, gb_qos_req_value(req));

	return ret;
}

static ssize_t store_global_boost(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	unsigned int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	gb_qos_update_request(&gb_req_user, input);

	return count;
}

static struct kobj_attribute global_boost_attr =
__ATTR(global_boost, 0644, show_global_boost, store_global_boost);

#define BOOT_BOOST_DURATION 40000000	/* microseconds */
unsigned long global_boost(void)
{
	u64 now = ktime_to_us(ktime_get());

	if (now < BOOT_BOOST_DURATION)
		return gb_max_value;

	return gb_value;
}

int find_second_max_cap(void)
{
	struct sched_domain *sd;
	struct sched_group *sg;
	int max_cap = 0, second_max_cap = 0;

	rcu_read_lock();

	sd = rcu_dereference(per_cpu(sd_ea, 0));
	if (!sd)
		goto unlock;

	sg = sd->groups;
	do {
		int i;

		for_each_cpu(i, sched_group_cpus(sg)) {
			if (max_cap < cpu_rq(i)->cpu_capacity_orig) {
				second_max_cap = max_cap;
				max_cap = cpu_rq(i)->cpu_capacity_orig;
			}
		}
	} while (sg = sg->next, sg != sd->groups);

unlock:
	rcu_read_unlock();

	return second_max_cap;
}

static int __init init_global_boost(void)
{
	gb_max_value = find_second_max_cap() + 1;

	return 0;
}
pure_initcall(init_global_boost);

/**********************************************************************
 * Boost cpu selection (global boost, schedtune.prefer_perf)          *
 **********************************************************************/
#define cpu_selected(cpu)	(cpu >= 0)

int kernel_prefer_perf(int grp_idx);
static ssize_t show_prefer_perf(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i, ret = 0;

	for (i = 0; i < STUNE_GROUP_COUNT; i++)
		ret += snprintf(buf + ret, 10, "%d ", kernel_prefer_perf(i));

	ret += snprintf(buf + ret, 10, "\n");

	return ret;
}

static struct kobj_attribute prefer_perf_attr =
__ATTR(kernel_prefer_perf, 0444, show_prefer_perf, NULL);

enum {
	BT_PREFER_PERF = 0,
	BT_GROUP_BALANCE,
	BT_GLOBAL_BOOST,
};

struct boost_trigger {
	int trigger;
	int boost_val;
};

static int check_boost_trigger(struct task_struct *p, struct boost_trigger *bt)
{
	int gb;

#ifdef CONFIG_CGROUP_SCHEDTUNE
	if (schedtune_prefer_perf(p) > 0) {
		bt->trigger = BT_PREFER_PERF;
		bt->boost_val = schedtune_perf_threshold();
		return 1;
	}

	if (schedtune_need_group_balance(p) > 0) {
		bt->trigger = BT_GROUP_BALANCE;
		bt->boost_val = schedtune_perf_threshold();
		return 1;
	}
#endif

	gb = global_boost();
	if (gb) {
		bt->trigger = BT_GLOBAL_BOOST;
		bt->boost_val = gb;
		return 1;
	}

	/* not boost state */
	return 0;
}

static int boost_select_cpu(struct task_struct *p, struct cpumask *target_cpus)
{
	int i, cpu = 0;

	if (cpumask_empty(target_cpus))
		return -1;

	if (cpumask_test_cpu(task_cpu(p), target_cpus))
		return task_cpu(p);

	/* Return last cpu in target_cpus */
	for_each_cpu(i, target_cpus)
		cpu = i;

	return cpu;
}

static void mark_shallowest_cpu(int cpu, unsigned int *min_exit_latency,
						struct cpumask *shallowest_cpus)
{
	struct rq *rq = cpu_rq(cpu);
	struct cpuidle_state *idle = idle_get_state(rq);

	/* Before enabling cpuidle, all idle cpus are marked */
	if (!idle) {
		cpumask_set_cpu(cpu, shallowest_cpus);
		return;
	}

	/* Deeper idle cpu is ignored */
	if (idle->exit_latency > *min_exit_latency)
		return;

	/* if shallower idle cpu is found, previsouly founded cpu is ignored */
	if (idle->exit_latency < *min_exit_latency) {
		cpumask_clear(shallowest_cpus);
		*min_exit_latency = idle->exit_latency;
	}

	cpumask_set_cpu(cpu, shallowest_cpus);
}
static int check_migration_task(struct task_struct *p)
{
	if (rt_task(p))
		return !p->rt.avg.last_update_time;
	else
		return !p->se.avg.last_update_time;
}

unsigned long cpu_util_wake(int cpu, struct task_struct *p)
{
	unsigned long util, capacity;

	/* Task has no contribution or is new */
	if (cpu != task_cpu(p) || check_migration_task(p))
		return cpu_util(cpu);

	capacity = capacity_orig_of(cpu);
	util = max_t(long, cpu_util(cpu) - task_util(p), 0);

	return (util >= capacity) ? capacity : util;
}

static int find_group_boost_target(struct task_struct *p)
{
	struct sched_domain *sd;
	int shallowest_cpu = -1;
	int lowest_cpu = -1;
	unsigned int min_exit_latency = UINT_MAX;
	unsigned long lowest_util = ULONG_MAX;
	int target_cpu = -1;
	int cpu;
	char state[30] = "fail";

	sd = rcu_dereference(per_cpu(sd_ea, maxcap_cpu));
	if (!sd)
		return target_cpu;

	if (cpumask_test_cpu(task_cpu(p), sched_group_cpus(sd->groups))) {
		if (idle_cpu(task_cpu(p))) {
			target_cpu = task_cpu(p);
			strcpy(state, "current idle");
			goto find_target;
		}
	}

	for_each_cpu_and(cpu, tsk_cpus_allowed(p), sched_group_cpus(sd->groups)) {
		unsigned long util = cpu_util_wake(cpu, p);

		if (idle_cpu(cpu)) {
			struct cpuidle_state *idle;

			idle = idle_get_state(cpu_rq(cpu));
			if (!idle) {
				target_cpu = cpu;
				strcpy(state, "idle wakeup");
				goto find_target;
			}

			if (idle->exit_latency < min_exit_latency) {
				min_exit_latency = idle->exit_latency;
				shallowest_cpu = cpu;
				continue;
			}
		}

		if (cpu_selected(shallowest_cpu))
			continue;

		if (util < lowest_util) {
			lowest_cpu = cpu;
			lowest_util = util;
		}
	}

	if (cpu_selected(shallowest_cpu)) {
		target_cpu = shallowest_cpu;
		strcpy(state, "shallowest idle");
		goto find_target;
	}

	if (cpu_selected(lowest_cpu)) {
		target_cpu = lowest_cpu;
		strcpy(state, "lowest util");
	}

find_target:
	trace_ehmp_select_group_boost(p, target_cpu, state);

	return target_cpu;
}

static int
find_boost_target(struct sched_domain *sd, struct task_struct *p,
			unsigned long min_util, struct boost_trigger *bt)
{
	struct sched_group *sg;
	int boost = bt->boost_val;
	unsigned long max_capacity;
	struct cpumask boost_candidates;
	struct cpumask backup_boost_candidates;
	unsigned int min_exit_latency = UINT_MAX;
	unsigned int backup_min_exit_latency = UINT_MAX;
	int target_cpu;
	bool go_up = false;
	unsigned long lowest_util = ULONG_MAX;
	int lowest_cpu = -1;
	char state[30] = "fail";

	if (bt->trigger == BT_GROUP_BALANCE)
		return find_group_boost_target(p);

	cpumask_setall(&boost_candidates);
	cpumask_clear(&backup_boost_candidates);

	max_capacity = maxcap_val;

	sg = sd->groups;

	do {
		int i;

		for_each_cpu_and(i, tsk_cpus_allowed(p), sched_group_cpus(sg)) {
			unsigned long new_util, wake_util;

			if (!cpu_online(i))
				continue;

			wake_util = cpu_util_wake(i, p);
			new_util = wake_util + task_util(p);
			new_util = max(min_util, new_util);

			if (min(new_util + boost, max_capacity) > capacity_orig_of(i)) {
				if (!cpu_rq(i)->nr_running)
					mark_shallowest_cpu(i, &backup_min_exit_latency,
							&backup_boost_candidates);
				else if (cpumask_test_cpu(task_cpu(p), sched_group_cpus(sg)))
					go_up = true;

				continue;
			}

			if (cpumask_weight(&boost_candidates) >= nr_cpu_ids)
				cpumask_clear(&boost_candidates);

			if (!cpu_rq(i)->nr_running) {
				mark_shallowest_cpu(i, &min_exit_latency, &boost_candidates);
				continue;
			}

			if (wake_util < lowest_util) {
				lowest_util = wake_util;
				lowest_cpu = i;
			}
		}

		if (cpumask_weight(&boost_candidates) >= nr_cpu_ids)
			continue;

		target_cpu = boost_select_cpu(p, &boost_candidates);
		if (cpu_selected(target_cpu)) {
			strcpy(state, "big idle");
			goto out;
		}

		target_cpu = boost_select_cpu(p, &backup_boost_candidates);
		if (cpu_selected(target_cpu)) {
			strcpy(state, "little idle");
			goto out;
		}
	} while (sg = sg->next, sg != sd->groups);

	if (go_up) {
		strcpy(state, "lowest big cpu");
		target_cpu = lowest_cpu;
		goto out;
	}

	strcpy(state, "current cpu");
	target_cpu = task_cpu(p);

out:
	trace_ehmp_select_boost_cpu(p, target_cpu, bt->trigger, state);
	return target_cpu;
}

/**********************************************************************
 * schedtune.prefer_idle                                              *
 **********************************************************************/
static void mark_lowest_cpu(int cpu, unsigned long new_util,
			int *lowest_cpu, unsigned long *lowest_util)
{
	if (new_util >= *lowest_util)
		return;

	*lowest_util = new_util;
	*lowest_cpu = cpu;
}

static int find_prefer_idle_target(struct sched_domain *sd,
			struct task_struct *p, unsigned long min_util)
{
	struct sched_group *sg;
	int target_cpu = -1;
	int lowest_cpu = -1;
	int lowest_idle_cpu = -1;
	int overcap_cpu = -1;
	unsigned long lowest_util = ULONG_MAX;
	unsigned long lowest_idle_util = ULONG_MAX;
	unsigned long overcap_util = ULONG_MAX;
	struct cpumask idle_candidates;
	struct cpumask overcap_idle_candidates;

	cpumask_clear(&idle_candidates);
	cpumask_clear(&overcap_idle_candidates);

	sg = sd->groups;

	do {
		int i;

		for_each_cpu_and(i, tsk_cpus_allowed(p), sched_group_cpus(sg)) {
			unsigned long new_util, wake_util;

			if (!cpu_online(i))
				continue;

			wake_util = cpu_util_wake(i, p);
			new_util = wake_util + task_util(p);
			new_util = max(min_util, new_util);

			trace_ehmp_prefer_idle(p, task_cpu(p), i, task_util(p),
							new_util, idle_cpu(i));

			if (new_util > capacity_orig_of(i)) {
				if (idle_cpu(i)) {
					cpumask_set_cpu(i, &overcap_idle_candidates);
					mark_lowest_cpu(i, new_util,
						&overcap_cpu, &overcap_util);
				}

				continue;
			}

			if (idle_cpu(i)) {
				if (task_cpu(p) == i) {
					target_cpu = i;
					break;
				}

				cpumask_set_cpu(i, &idle_candidates);
				mark_lowest_cpu(i, new_util,
					&lowest_idle_cpu, &lowest_idle_util);

				continue;
			}

			mark_lowest_cpu(i, new_util, &lowest_cpu, &lowest_util);
		}

		if (cpu_selected(target_cpu))
			break;

		if (cpumask_weight(&idle_candidates)) {
			target_cpu = lowest_idle_cpu;
			break;
		}

		if (cpu_selected(lowest_cpu)) {
			target_cpu = lowest_cpu;
			break;
		}

	} while (sg = sg->next, sg != sd->groups);

	if (cpu_selected(target_cpu))
		goto out;

	if (cpumask_weight(&overcap_idle_candidates)) {
		if (cpumask_test_cpu(task_cpu(p), &overcap_idle_candidates))
			target_cpu = task_cpu(p);
		else
			target_cpu = overcap_cpu;

		goto out;
	}

out:
	trace_ehmp_prefer_idle_cpu_select(p, target_cpu);

	return target_cpu;
}

/**********************************************************************
 * On-time migration                                                  *
 **********************************************************************/
static unsigned long up_threshold;
static unsigned long down_threshold;
static unsigned int min_residency_us;

static ssize_t show_min_residency(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%d\n", min_residency_us);
}

static ssize_t store_min_residency(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	input = input < 0 ? 0 : input;

	min_residency_us = input;

	return count;
}

static struct kobj_attribute min_residency_attr =
__ATTR(min_residency, 0644, show_min_residency, store_min_residency);

static ssize_t show_up_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 10, "%ld\n", up_threshold);
}

static ssize_t store_up_threshold(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	long input;

	if (!sscanf(buf, "%ld", &input))
		return -EINVAL;

	input = input < 0 ? 0 : input;
	input = input > 1024 ? 1024 : input;

	up_threshold = input;

	return count;
}

static struct kobj_attribute up_threshold_attr =
__ATTR(up_threshold, 0644, show_up_threshold, store_up_threshold);

static ssize_t show_down_threshold(struct kobject *kobj,
                struct kobj_attribute *attr, char *buf)
{
        return snprintf(buf, 10, "%ld\n", down_threshold);
}

static ssize_t store_down_threshold(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf,
                size_t count)
{
        long input;

        if (!sscanf(buf, "%ld", &input))
                return -EINVAL;

        input = input < 0 ? 0 : input;
        input = input > 1024 ? 1024 : input;

        down_threshold = input;

        return count;
}

static struct kobj_attribute down_threshold_attr =
__ATTR(down_threshold, 0644, show_down_threshold, store_down_threshold);

#define ontime_flag(p)			(ontime_of(p)->flags)
#define ontime_migration_time(p)	(ontime_of(p)->avg.ontime_migration_time)
#define ontime_load_avg(p)		(ontime_of(p)->avg.load_avg)

static inline struct ontime_entity *ontime_of(struct task_struct *p)
{
	return &p->se.ontime;
}

static inline void include_ontime_task(struct task_struct *p)
{
	ontime_flag(p) = ONTIME;

	/* Manage time based on clock task of boot cpu(cpu0) */
	ontime_migration_time(p) = cpu_rq(0)->clock_task;
}

static inline void exclude_ontime_task(struct task_struct *p)
{
	ontime_migration_time(p) = 0;
	ontime_flag(p) = NOT_ONTIME;
}

static int
ontime_select_target_cpu(struct sched_group *sg, const struct cpumask *mask)
{
	int cpu;
	int dest_cpu = -1;
	unsigned int min_exit_latency = UINT_MAX;
	struct cpuidle_state *idle;

	for_each_cpu_and(cpu, sched_group_cpus(sg), mask) {
		if (!idle_cpu(cpu))
			continue;

		if (cpu_rq(cpu)->ontime_migrating)
			continue;

		idle = idle_get_state(cpu_rq(cpu));
		if (idle && idle->exit_latency < min_exit_latency) {
			min_exit_latency = idle->exit_latency;
			dest_cpu = cpu;
		}

		/* Before enableing cpuidle, select last idle cpu in sg */
		if (!idle)
			dest_cpu = cpu;
	}

	return dest_cpu;
}

#define TASK_TRACK_COUNT	5

struct sched_entity *__pick_next_entity(struct sched_entity *se);
static struct task_struct *
ontime_pick_heavy_task(struct sched_entity *se, struct cpumask *dst_cpus,
						int *boost_migration)
{
	struct task_struct *heaviest_task = NULL;
	struct task_struct *p;
	unsigned int max_util_avg = 0;
	int task_count = 0;
	int boosted = !!global_boost() || !!schedtune_prefer_perf(task_of(se));

	/*
	 * Since current task does not exist in entity list of cfs_rq,
	 * check first that current task is heavy.
	 */
	if (boosted) {
		*boost_migration = 1;
		return task_of(se);
	}
	if (ontime_load_avg(task_of(se)) >= up_threshold) {
		heaviest_task = task_of(se);
		max_util_avg = ontime_load_avg(task_of(se));
		*boost_migration = 0;
	}

	se = __pick_first_entity(se->cfs_rq);
	while (se && task_count < TASK_TRACK_COUNT) {
		/* Skip non-task entity */
		if (entity_is_cfs_rq(se))
			goto next_entity;

		p = task_of(se);
		if (schedtune_prefer_perf(p)) {
			heaviest_task = p;
			*boost_migration = 1;
			break;
		}

		if (ontime_load_avg(p) < up_threshold)
			goto next_entity;

		if (ontime_load_avg(p) > max_util_avg &&
		    cpumask_intersects(dst_cpus, tsk_cpus_allowed(p))) {
			heaviest_task = p;
			max_util_avg = ontime_load_avg(p);
			*boost_migration = 0;
		}

next_entity:
		se = __pick_next_entity(se);
		task_count++;
	}

	return heaviest_task;
}
static struct task_struct *
ontime_pick_vip_task(struct sched_entity *se, int src_cpu, int dst_cpu)
{
	struct task_struct *vip_task = NULL;
	struct task_struct *p;
	unsigned int max_util_avg = 0;
	int task_count = 0;

	if (!lbt_overutilized(src_cpu, dst_cpu) || cpu_rq(src_cpu)->nr_running <= 1)
		return NULL;

	if (src_cpu == dst_cpu)
		return NULL;

	p = task_of(se);
	if (cpumask_test_cpu(dst_cpu, tsk_cpus_allowed(p))) {
		vip_task = p;
		max_util_avg = ontime_load_avg(p);
	}

	se = __pick_first_entity(se->cfs_rq);
	while (se && task_count < TASK_TRACK_COUNT) {
		/* Skip non-task entity */
		if (entity_is_cfs_rq(se))
			goto next_entity;

		p = task_of(se);

		if (ontime_load_avg(p) > max_util_avg &&
		    cpumask_test_cpu(dst_cpu, tsk_cpus_allowed(p))) {
			vip_task = p;
			max_util_avg = ontime_load_avg(p);
		}

next_entity:
		se = __pick_next_entity(se);
		task_count++;
	}

	return vip_task;
}

void ontime_new_entity_load(struct task_struct *parent, struct sched_entity *se)
{
	struct ontime_entity *ontime;

	if (entity_is_cfs_rq(se))
		return;

	ontime = &se->ontime;

	ontime->avg.load_sum = ontime_of(parent)->avg.load_sum;
	ontime->avg.load_avg = ontime_of(parent)->avg.load_avg;
	ontime->avg.ontime_migration_time = 0;
	ontime->flags = NOT_ONTIME;

	trace_ehmp_ontime_new_entity_load(task_of(se), &ontime->avg);
}

/* Structure of ontime migration environment */
struct ontime_env {
	struct rq		*dst_rq;
	int			dst_cpu;
	struct rq		*src_rq;
	int			src_cpu;
	struct task_struct	*target_task;
	int			boost_migration;
};
DEFINE_PER_CPU(struct ontime_env, ontime_env);

static int can_migrate(struct task_struct *p, struct ontime_env *env)
{
	if (!cpumask_test_cpu(env->dst_cpu, tsk_cpus_allowed(p)))
		return 0;

	if (task_running(env->src_rq, p))
		return 0;

	return 1;
}

static void move_task(struct task_struct *p, struct ontime_env *env)
{
	p->on_rq = TASK_ON_RQ_MIGRATING;
	deactivate_task(env->src_rq, p, 0);
	set_task_cpu(p, env->dst_cpu);

	activate_task(env->dst_rq, p, 0);
	p->on_rq = TASK_ON_RQ_QUEUED;
	check_preempt_curr(env->dst_rq, p, 0);
}

static int move_specific_task(struct task_struct *target, struct ontime_env *env)
{
	struct task_struct *p, *n;

	list_for_each_entry_safe(p, n, &env->src_rq->cfs_tasks, se.group_node) {
		if (!can_migrate(p, env))
			continue;

		if (p != target) 
			continue;

		move_task(p, env);
		return 1;
	}

	return 0;
}

static int ontime_migration_cpu_stop(void *data)
{
	struct ontime_env *env = data;
	struct rq *src_rq, *dst_rq;
	int src_cpu, dst_cpu;
	struct task_struct *p;
	struct sched_domain *sd;
	int boost_migration;

	/* Initialize environment data */
	src_rq = env->src_rq;
	dst_rq = env->dst_rq = cpu_rq(env->dst_cpu);
	src_cpu = env->src_cpu = env->src_rq->cpu;
	dst_cpu = env->dst_cpu;
	p = env->target_task;
	boost_migration = env->boost_migration;

	raw_spin_lock_irq(&src_rq->lock);

	if (!(ontime_flag(p) & ONTIME_MIGRATING))
		goto out_unlock;

	if (p->exit_state)
		goto out_unlock;

	if (unlikely(src_cpu != smp_processor_id()))
		goto out_unlock;

	if (src_rq->nr_running <= 1)
		goto out_unlock;

	if (src_rq != task_rq(p))
		goto out_unlock;

	BUG_ON(src_rq == dst_rq);

	double_lock_balance(src_rq, dst_rq);

	rcu_read_lock();
	for_each_domain(dst_cpu, sd)
		if (cpumask_test_cpu(src_cpu, sched_domain_span(sd)))
			break;

	if (likely(sd) && move_specific_task(p, env)) {
		if (boost_migration) {
			/* boost task is not classified as ontime task */
			exclude_ontime_task(p);
		} else
			include_ontime_task(p);

		rcu_read_unlock();
		double_unlock_balance(src_rq, dst_rq);

		trace_ehmp_ontime_migration(p, ontime_of(p)->avg.load_avg,
					src_cpu, dst_cpu, boost_migration);
		goto success_unlock;
	}

	rcu_read_unlock();
	double_unlock_balance(src_rq, dst_rq);

out_unlock:
	exclude_ontime_task(p);

success_unlock:
	src_rq->active_balance = 0;
	dst_rq->ontime_migrating = 0;

	raw_spin_unlock_irq(&src_rq->lock);
	put_task_struct(p);

	return 0;
}

DEFINE_PER_CPU(struct cpu_stop_work, ontime_migration_work);

static DEFINE_SPINLOCK(om_lock);

void ontime_migration(void)
{
	struct sched_domain *sd;
	struct sched_group *src_sg, *dst_sg;
	int cpu;

	if (!spin_trylock(&om_lock))
		return;

	rcu_read_lock();

	sd = rcu_dereference(per_cpu(sd_ea, 0));
	if (!sd)
		goto ontime_migration_exit;

	src_sg = sd->groups;

	do {
		dst_sg = src_sg->next;
		for_each_cpu(cpu, cpu_active_mask) {
			unsigned long flags;
			struct rq *rq;
			struct sched_entity *se;
			struct task_struct *p;
			int dst_cpu;
			struct ontime_env *env = &per_cpu(ontime_env, cpu);
			int boost_migration = 0;

			rq = cpu_rq(cpu);
			raw_spin_lock_irqsave(&rq->lock, flags);

			/*
			 * Ontime migration is not performed when active balance
			 * is in progress.
			 */
			if (rq->active_balance) {
				raw_spin_unlock_irqrestore(&rq->lock, flags);
				continue;
			}

			/*
			 * No need to migration if source cpu does not have cfs
			 * tasks.
			 */
			if (!rq->cfs.curr) {
				raw_spin_unlock_irqrestore(&rq->lock, flags);
				continue;
			}

			se = rq->cfs.curr;

			/* Find task entity if entity is cfs_rq. */
			if (entity_is_cfs_rq(se)) {
				struct cfs_rq *cfs_rq;

				cfs_rq = se->my_q;
				while (cfs_rq) {
					se = cfs_rq->curr;
					cfs_rq = se->my_q;
				}
			}

			/*
			 * Select cpu to migrate the task to. Return negative number
			 * if there is no idle cpu in sg.
			 */
			dst_cpu = ontime_select_target_cpu(dst_sg, cpu_active_mask);
			if (dst_cpu < 0) {
				raw_spin_unlock_irqrestore(&rq->lock, flags);
				continue;
			}

			/*
			 * Pick task to be migrated. Return NULL if there is no
			 * heavy task in rq.
			 */
			if (!cpumask_test_cpu(cpu, cpu_coregroup_mask(maxcap_cpu))) {
				p = ontime_pick_heavy_task(se, sched_group_cpus(dst_sg),
						&boost_migration);
				if (!p) {
					raw_spin_unlock_irqrestore(&rq->lock, flags);
					continue;
				}
			} else {
				p = ontime_pick_vip_task(se, cpu, dst_cpu);
				if (!p) {
					raw_spin_unlock_irqrestore(&rq->lock, flags);
					continue;
				}
			}

			ontime_flag(p) = ONTIME_MIGRATING;
			get_task_struct(p);

			/* Set environment data */
			env->dst_cpu = dst_cpu;
			env->src_rq = rq;
			env->target_task = p;
			env->boost_migration = boost_migration;

			/* Prevent active balance to use stopper for migration */
			rq->active_balance = 1;

			cpu_rq(dst_cpu)->ontime_migrating = 1;

			raw_spin_unlock_irqrestore(&rq->lock, flags);

			/* Migrate task through stopper */
			stop_one_cpu_nowait(cpu,
				ontime_migration_cpu_stop, env,
				&per_cpu(ontime_migration_work, cpu));
		}
	} while (src_sg = src_sg->next, src_sg->next != sd->groups);

ontime_migration_exit:
	rcu_read_unlock();
	spin_unlock(&om_lock);
}

int ontime_can_migration(struct task_struct *p, int cpu)
{
	int target_cpu = cpu < 0 ? task_cpu(p) : cpu;
	u64 delta;

	if (ontime_flag(p) & NOT_ONTIME) {
		trace_ehmp_ontime_check_migrate(target_cpu, true, "not_ontime");
		return true;
	}

	/* task is not migrated to big yet */
	if (ontime_flag(p) & ONTIME_MIGRATING) {
		trace_ehmp_ontime_check_migrate(target_cpu, false, "new_ontime");
		return false;
	}

	if (cpumask_test_cpu(target_cpu, cpu_coregroup_mask(maxcap_cpu))) {
		trace_ehmp_ontime_check_migrate(target_cpu, true, "ontime on big");
		return true;
	}

	/*
	 * At this point, task is "ontime task" and running on big
	 * and load balancer is trying to migrate task to LITTLE.
	 */
	delta = cpu_rq(0)->clock_task - ontime_migration_time(p);
	delta = delta >> 10;
	if (delta <= min_residency_us) {
		trace_ehmp_ontime_check_migrate(target_cpu, false, "min residency");
		return false;
	}

	if (cpu_rq(task_cpu(p))->nr_running > 1) {
		trace_ehmp_ontime_check_migrate(target_cpu, true, "big is busy");
		goto release;
	}

	if (ontime_load_avg(p) >= down_threshold) {
		trace_ehmp_ontime_check_migrate(target_cpu, false, "heavy task");
		return false;
	}

	trace_ehmp_ontime_check_migrate(target_cpu, true, "ontime_release");
release:
	exclude_ontime_task(p);

	return true;

}

static int ontime_wakeup_migration(struct task_struct *p, int target_cpu)
{
	struct sched_domain *sd;
	int dest_cpu;

	sd = rcu_dereference(per_cpu(sd_ea, maxcap_cpu));
	if (!sd)
		return target_cpu;

	dest_cpu = ontime_select_target_cpu(sd->groups, tsk_cpus_allowed(p));
	if (cpu_selected(dest_cpu)) {
		include_ontime_task(p);
		target_cpu = dest_cpu;
	}

	return target_cpu;
}

static int ontime_wakeup(struct task_struct *p, int target_cpu)
{
	if (ontime_flag(p) & NOT_ONTIME) {
		if (ontime_load_avg(p) > up_threshold)
			return ontime_wakeup_migration(p, target_cpu);

		return target_cpu;
	}

	if (ontime_can_migration(p, target_cpu))
		return target_cpu;

	/* cancel waking up to target cpu */
	return task_cpu(p);
}

static void ontime_update_next_balance(int cpu, struct ontime_avg *oa)
{
	if (cpumask_test_cpu(cpu, cpu_coregroup_mask(maxcap_cpu)))
		return;

	if (oa->load_avg < up_threshold)
		return;

	/*
	 * Update the next_balance of this cpu because tick is most likely
	 * to occur first in currently running cpu.
	 */
	cpu_rq(smp_processor_id())->next_balance = jiffies;
}

#define cap_scale(v, s) ((v)*(s) >> SCHED_CAPACITY_SHIFT)

u64 decay_load(u64 val, u64 n);
u32 __accumulate_sum(u64 periods, u32 period_contrib, u32 remainder);

/*
 * ontime_update_load_avg : load tracking for ontime-migration
 *
 * @sa : sched_avg to be updated
 * @delta : elapsed time since last update
 * @period_contrib : amount already accumulated against our next period
 * @scale_freq : scale vector of cpu frequency
 * @scale_cpu : scale vector of cpu capacity
 */
void ontime_update_load_avg(int cpu, struct sched_avg *sa,
			u64 delta, unsigned int period_contrib, int weight)
{
	struct ontime_avg *oa = &se_of(sa)->ontime.avg;
	unsigned long scale_freq = arch_scale_freq_capacity(NULL, cpu);
	unsigned long scale_cpu = arch_scale_cpu_capacity(NULL, cpu);
	unsigned int decayed = 0;
	unsigned int delta_w, scaled_delta_w;
	u64 scaled_delta, periods;
	u32 contrib;

	if (entity_is_cfs_rq(se_of(sa)))
		return;

	/* Period elapsed */
	if (delta + period_contrib >= 1024) {
		decayed = 1;

		/* Accumulate the remainder in the last period */
		delta_w = 1024 - period_contrib;
		if (weight) {
			scaled_delta_w = cap_scale(delta_w, scale_freq);
			oa->load_sum += scaled_delta_w * scale_cpu;
		}

		/* Decay the current accumulated load by the elapsed period */
		delta -= delta_w;
		periods = delta / 1024;
		delta %= 1024;
		oa->load_sum = decay_load(oa->load_sum, periods + 1);

		/* New load is decayed by the elapsed period and accumulated */
		if (weight) {
			contrib = __accumulate_sum(periods, period_contrib, delta);
			contrib = cap_scale(contrib, scale_freq);
			oa->load_sum += contrib * scale_cpu;
		}
	}

	/* Accumulate the remaining load in the current period */
	if (weight) {
		scaled_delta = cap_scale(delta, scale_freq);
		oa->load_sum += scaled_delta * scale_cpu;
	}

	if (decayed) {
		oa->load_avg = div_u64(oa->load_sum, LOAD_AVG_MAX);
		ontime_update_next_balance(cpu, oa);
	}
}

void ontime_trace_task_info(struct task_struct *p)
{
	trace_ehmp_ontime_load_avg_task(p, &ontime_of(p)->avg);
}

static inline unsigned long mincap_of(int cpu)
{
	return sge_array[cpu][SD_LEVEL0]->cap_states[0].cap;
}

static int __init init_ontime(void)
{
	struct device_node *dn;
	u32 prop;

	dn = get_ehmp_node();
	if (!dn)
		return 0;

	/*
	 * Initilize default values:
	 *   up_threshold	= 40% of LITTLE maximum capacity
	 *   down_threshold	= 50% of big minimum capacity
	 *   min_residency	= 8ms
	 */
	up_threshold = capacity_orig_of(0) * 40 / 100;
	down_threshold = mincap_of(maxcap_cpu) * 50 / 100;
	min_residency_us = 8192;

	of_property_read_u32(dn, "up-threshold", &prop);
	up_threshold = prop;

	of_property_read_u32(dn, "down-threshold", &prop);
	down_threshold = prop;

	of_property_read_u32(dn, "min-residency-us", &prop);
	min_residency_us = prop;

	return 0;
}
pure_initcall(init_ontime);

/**********************************************************************
 * cpu selection                                                      *
 **********************************************************************/
unsigned long boosted_task_util(struct task_struct *p);
int energy_diff(struct energy_env *eenv);

static inline int find_best_target(struct sched_domain *sd, struct task_struct *p)
{
	int target_cpu = -1;
	int backup_cpu = -1;
	unsigned long target_util = 0;
	unsigned long backup_util = ULONG_MAX;
	unsigned long lowest_util = ULONG_MAX;
	unsigned long backup_capacity = ULONG_MAX;
	unsigned long min_util = boosted_task_util(p);
	struct sched_group *sg;
	int cpu = 0;
	struct cpumask energy_candidates;

	sg = sd->groups;
	cpumask_clear(&energy_candidates);

	do {
		int i;

		for_each_cpu_and(i, tsk_cpus_allowed(p), sched_group_cpus(sg)) {
			unsigned long cur_capacity, new_util, wake_util;
			unsigned long min_wake_util = ULONG_MAX;

			if (!cpu_online(i))
				continue;

			/*
			 * p's blocked utilization is still accounted for on prev_cpu
			 * so prev_cpu will receive a negative bias due to the double
			 * accounting. However, the blocked utilization may be zero.
			 */
			wake_util = cpu_util_wake(i, p);
			new_util = wake_util + task_util(p);

			/*
			 * Ensure minimum capacity to grant the required boost.
			 * The target CPU can be already at a capacity level higher
			 * than the one required to boost the task.
			 */
			new_util = max(min_util, new_util);

			if (new_util > capacity_orig_of(i)) {
				if (wake_util >= capacity_orig_of(i))
					continue;

				if (task_util(p) > (capacity_orig_of(i) >> 1))
					continue;
			}

			cur_capacity = capacity_curr_of(i);

			trace_ehmp_find_best_target_stat(i, cur_capacity, new_util, target_util);

			if (new_util < cur_capacity) {
				if (cpumask_test_cpu(i, cpu_coregroup_mask(cpu))) {
					if (new_util > lowest_util ||
					    wake_util > min_wake_util)
						continue;
					min_wake_util = wake_util;
					lowest_util = new_util;
					target_cpu = i;
				} else {
					if (cpu_rq(i)->nr_running) {
						if (new_util < target_util)
							continue;
						target_util = new_util;
						target_cpu = i;
					} else {
						cpumask_set_cpu(i, &energy_candidates);
						trace_ehmp_find_best_target_candi(i);
					}
				}
			} else if (backup_capacity >= cur_capacity) {
				/* Find a backup cpu with least capacity. */
				backup_capacity = cur_capacity;
				if (new_util < backup_util) {
					backup_util = new_util;
					backup_cpu = i;
					cpumask_set_cpu(backup_cpu, &energy_candidates);
					trace_ehmp_find_best_target_candi(backup_cpu);
				}
			}
		}

		if (target_cpu >= 0)
			break;
	} while (sg = sg->next, sg != sd->groups);

	trace_ehmp_find_best_target_cpu(target_cpu, target_util);

	if (target_cpu < 0) {
		int min_nrg_diff = 0;
		int nrg_diff, i;

		for_each_cpu(i, &energy_candidates) {
			struct energy_env eenv = {
				.util_delta     = task_util(p),
				.src_cpu        = task_cpu(p),
				.dst_cpu        = i,
				.p              = p,
			};

			if (eenv.src_cpu == eenv.dst_cpu)
				continue;

			if (capacity_orig_of(eenv.src_cpu) > capacity_orig_of(eenv.dst_cpu)
				&& task_util(p) <= capacity_orig_of(eenv.dst_cpu)
				&& cpu_util(eenv.dst_cpu) < capacity_orig_of(eenv.dst_cpu))
				return eenv.dst_cpu;

			nrg_diff = energy_diff(&eenv);
			if (nrg_diff < min_nrg_diff) {
				target_cpu = i;
				min_nrg_diff = nrg_diff;
			}
		}

		if (target_cpu < 0)
			target_cpu = backup_cpu;
	}

	return target_cpu;
}

static int select_energy_cpu(struct sched_domain *sd, struct task_struct *p,
				int prev_cpu, int sd_flag, bool boosted)
{
	int target_cpu = prev_cpu, tmp_target;

	/* Find a cpu with sufficient capacity */
	tmp_target = find_best_target(sd, p);
	if (tmp_target >= 0) {
		target_cpu = tmp_target;
		if (boosted && idle_cpu(target_cpu))
			goto out;

		if (sd_flag & SD_BALANCE_FORK)
			goto out;

		if (capacity_orig_of(task_cpu(p)) > capacity_orig_of(target_cpu)
			&& task_util(p) <= capacity_orig_of(target_cpu)
			&& cpu_util(target_cpu) < capacity_orig_of(target_cpu))
			goto out;
	}

	if (target_cpu != prev_cpu) {
		struct energy_env eenv = {
			.util_delta     = task_util(p),
			.src_cpu        = prev_cpu,
			.dst_cpu        = target_cpu,
			.p              = p,
		};

		/* Not enough spare capacity on previous cpu */
		if (cpu_overutilized(prev_cpu))
			goto out;

		if (energy_diff(&eenv) >= 0) {
			target_cpu = prev_cpu;
			goto out;
		}

		goto out;
	}

out:
	return target_cpu;
}

int exynos_select_cpu_rt(struct sched_domain *sd, struct task_struct *p, bool boost)
{
	struct sched_group *sg;
	int target_cpu = -1;
	int lowest_idle_cpu = -1;
	int overcap_cpu = -1;
	unsigned long lowest_idle_util = ULONG_MAX;
	unsigned long overcap_util = ULONG_MAX;
	struct cpumask idle_candidates;
	struct cpumask overcap_idle_candidates;
	unsigned long min_util = task_util(p);

	cpumask_clear(&idle_candidates);
	cpumask_clear(&overcap_idle_candidates);

	sg = sd->groups;

	do {
		int i;

		for_each_cpu_and(i, tsk_cpus_allowed(p), sched_group_cpus(sg)) {
			unsigned long new_util, wake_util;

			if (!cpu_online(i))
				continue;

			wake_util = cpu_util_wake(i, p);
			new_util = wake_util + task_util(p);
			new_util = max(min_util, new_util);

			trace_ehmp_prefer_idle(p, task_cpu(p), i, task_util(p),
							new_util, idle_cpu(i));

			if (new_util > capacity_orig_of(i)) {
				if (idle_cpu(i)) {
					cpumask_set_cpu(i, &overcap_idle_candidates);
					mark_lowest_cpu(i, new_util,
						&overcap_cpu, &overcap_util);
				}

				continue;
			}

			if (idle_cpu(i)) {
				if (task_cpu(p) == i) {
					target_cpu = i;
					break;
				}

				cpumask_set_cpu(i, &idle_candidates);
				mark_lowest_cpu(i, new_util,
					&lowest_idle_cpu, &lowest_idle_util);

				continue;
			}
		}

		if (cpu_selected(target_cpu))
			break;

		if (cpumask_weight(&idle_candidates)) {
			target_cpu = lowest_idle_cpu;
			break;
		}

		if (!boost)
			break;
	} while (sg = sg->next, sg != sd->groups);

	if (cpu_selected(target_cpu))
		goto out;

	if (cpumask_weight(&overcap_idle_candidates)) {
		if (cpumask_test_cpu(task_cpu(p), &overcap_idle_candidates))
			target_cpu = task_cpu(p);
		else
			target_cpu = overcap_cpu;

		goto out;
	}
out:
	trace_ehmp_prefer_idle_cpu_select(p, target_cpu);

	return target_cpu;
}

int exynos_select_cpu(struct task_struct *p, int prev_cpu, int sync, int sd_flag)
{
	struct sched_domain *sd;
	int target_cpu = -1;
	bool boosted, prefer_idle;
	unsigned long min_util;
	struct boost_trigger trigger = {
		.trigger = 0,
		.boost_val = 0
	};

	rcu_read_lock();

	/* Find target cpu from lowest capacity domain(cpu0) */
	sd = rcu_dereference(per_cpu(sd_ea, 0));
	if (!sd)
		goto unlock;

#ifdef CONFIG_CGROUP_SCHEDTUNE
	boosted = schedtune_task_boost(p) > 0;
	prefer_idle = schedtune_prefer_idle(p) > 0;
#else
	boosted = get_sysctl_sched_cfs_boost() > 0;
	prefer_idle = 0;
#endif

	min_util = boosted_task_util(p);

	if (check_boost_trigger(p, &trigger)) {
		target_cpu = find_boost_target(sd, p, min_util, &trigger);
		if (cpu_selected(target_cpu))
			goto check_ontime;
	}

	if (sysctl_sched_sync_hint_enable && sync) {
		int cpu = smp_processor_id();

		if (cpumask_test_cpu(cpu, tsk_cpus_allowed(p))) {
			target_cpu = cpu;
			goto unlock;
		}
	}

	if (prefer_idle) {
		target_cpu = find_prefer_idle_target(sd, p, min_util);
		if (cpu_selected(target_cpu))
			goto check_ontime;
	}

	if (sched_feat(ENERGY_AWARE) && !(cpu_rq(prev_cpu)->rd->overutilized)) {
		target_cpu = select_energy_cpu(sd, p, prev_cpu, sd_flag, boosted);
		if (cpu_selected(target_cpu))
			goto check_ontime;
	}

check_ontime:
	target_cpu = ontime_wakeup(p, target_cpu);
unlock:
	rcu_read_unlock();

	return target_cpu;
}

/**********************************************************************
 * Ensure performance of cpufreq                                      *
 **********************************************************************/
static int cpufreq_domain_count;

#define CPU_QOS_CLASS_OFFSET		2
#define CPU_QOS_CLASS(id)		(PM_QOS_CLUSTER0_FREQ_MIN \
					 + (id * CPU_QOS_CLASS_OFFSET))

static struct pm_qos_request *ensure_perf_qos;

static ssize_t show_ensure_perf(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i, ret = 0;

	for (i = 0; i < cpufreq_domain_count; i++)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"[domain%d] requested value : %d, class value : %d\n",
			i, pm_qos_read_req_value(CPU_QOS_CLASS(i), &ensure_perf_qos[i]),
			pm_qos_request(CPU_QOS_CLASS(i)));

	return ret;
}

static ssize_t store_ensure_perf(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t count)
{
	int domain, value;

	if (!sscanf(buf, "%d %d", &domain, &value))
		return -EINVAL;

	if (value < 0 || domain < 0 || domain >= cpufreq_domain_count)
		return -EINVAL;

	pm_qos_update_request(&ensure_perf_qos[domain], value);

	return count;
}

static struct kobj_attribute ensure_perf_attr =
__ATTR(ensure_perf_cpufreq, 0644, show_ensure_perf, store_ensure_perf);

extern int exynos_cpufreq_domain_count(void);
static int init_ensure_perf(void)
{
	int i;

	cpufreq_domain_count = exynos_cpufreq_domain_count();
	if (!cpufreq_domain_count)
		return -ENODEV;

	ensure_perf_qos = kzalloc(sizeof(struct pm_qos_request)
					* cpufreq_domain_count, GFP_KERNEL);
	if (!ensure_perf_qos)
		return -ENOMEM;

	for (i = 0; i < cpufreq_domain_count; i++)
		pm_qos_add_request(&ensure_perf_qos[i], CPU_QOS_CLASS(i), 0);

	return 0;
}
late_initcall(init_ensure_perf);

/**********************************************************************
 * Sysfs                                                              *
 **********************************************************************/
static struct attribute *ehmp_attrs[] = {
	&global_boost_attr.attr,
	&min_residency_attr.attr,
	&up_threshold_attr.attr,
	&down_threshold_attr.attr,
	&top_overutil_attr.attr,
	&bot_overutil_attr.attr,
	&ensure_perf_attr.attr,
	&prefer_perf_attr.attr,
	NULL,
};

static const struct attribute_group ehmp_group = {
	.attrs = ehmp_attrs,
};

static struct kobject *ehmp_kobj;

static int init_sysfs(void)
{
	int ret;

	ehmp_kobj = kobject_create_and_add("ehmp", kernel_kobj);
	ret = sysfs_create_group(ehmp_kobj, &ehmp_group);

	return 0;
}
late_initcall(init_sysfs);
