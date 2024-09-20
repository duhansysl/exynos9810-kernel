/*
 * Copyright (c) 2016 Park Bumgyu, Samsung Electronics Co., Ltd <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Exynos ACME(A Cpufreq that Meets Every chipset) driver implementation
 */

#include "exynos-ufc.h"
#include <soc/samsung/exynos-dm.h>
#include <linux/pm_qos.h>

struct exynos_cpufreq_dm {
	struct list_head		list;
	struct exynos_dm_constraint	c;
};

struct exynos_ufc {
	struct list_head                list;
	struct exynos_ufc_info          info;
};

struct exynos_cpufreq_domain {
	/* list of domain */
	struct list_head		list;

	/* lock */
	struct mutex			lock;

	/* dt node */
	struct device_node		*dn;

	/* domain identity */
	unsigned int			id;
	struct cpumask			cpus;
	unsigned int			cal_id;
	enum exynos_dm_type		dm_type;

	/* frequency scaling */
	bool				enabled;

	unsigned int			table_size;
	struct cpufreq_frequency_table	*freq_table;

	unsigned int			max_freq;
	unsigned int			min_freq;
#ifdef CONFIG_SEC_PM
	unsigned int			max_usable_freq;
#endif
	unsigned int			boot_freq;
	unsigned int			resume_freq;
	unsigned int			old;

	/* PM QoS class */
	unsigned int			pm_qos_min_class;
	unsigned int			pm_qos_max_class;
	struct pm_qos_request		min_qos_req;
	struct pm_qos_request		max_qos_req;
	struct pm_qos_request		user_min_qos_req;
	struct pm_qos_request		user_max_qos_req;
	struct pm_qos_request		user_min_qos_wo_boost_req;
	struct notifier_block		pm_qos_min_notifier;
	struct notifier_block		pm_qos_max_notifier;

	/* for sysfs */
	unsigned int			user_default_qos;

	/* freq boost */
	bool				boost_supported;
	unsigned int			*boost_max_freqs;
	struct cpumask			online_cpus;

	/* list head of DVFS Manager constraints */
	struct list_head		dm_list;

	/* list head of User cpuFreq Ctrl (UFC) */
	struct list_head                ufc_list;
};

/*
 * list head of cpufreq domain
 */
extern struct exynos_cpufreq_domain
		*find_domain_cpumask(const struct cpumask *mask);
extern struct list_head *get_domain_list(void);
extern struct exynos_cpufreq_domain *first_domain(void);
extern struct exynos_cpufreq_domain *last_domain(void);
extern int exynos_cpufreq_domain_count(void);
#ifdef CONFIG_ARM_EXYNOS_UFC
extern int ufc_domain_init(struct exynos_cpufreq_domain *domain);
#else
static inline int ufc_domain_init(struct exynos_cpufreq_domain *domain)
{
	return 0;
}
#endif

/*
 * the time it takes on this CPU to switch between
 * two frequencies in nanoseconds
 */
#define TRANSITION_LATENCY	100000
