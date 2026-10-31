/*
 * Copyright (c) 2019 Park Bumgyu, Samsung Electronics Co., Ltd <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * UCC(User Cstate Control) driver implementation
 */

#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/exynos-ucc.h>

static int ucc_initialized = false;

bool ib_ucc_initialized;
EXPORT_SYMBOL(ib_ucc_initialized);

/*
 * struct ucc_config
 *
 * - index : index of cstate config
 * - cstate : mask indicating whether each cpu supports cstate
 *    - mask bit set : cstate enable
 *    - mask bit unset : cstate disable
 */
struct ucc_config {
	int		index;
	struct cpumask	cstate;
};

static struct ucc_config *ucc_configs;
static int ucc_config_count;

/*
 * cur_cstate has cpu cstate config corresponding to maximum index among ucc
 * request. Whenever maximum requested index is changed, cur_state is updated.
 */
static struct cpumask cur_cstate;

int filter_cstate(int cpu, int index)
{
	if (!ucc_initialized)
		return index;

	if (!cpumask_test_cpu(cpu, &cur_cstate)) {
		/* index 0 is default idle state */
		return 0;
	}

	return index;
}

static struct plist_head ucc_req_list = PLIST_HEAD_INIT(ucc_req_list);

static DEFINE_SPINLOCK(ucc_lock);

static int ucc_get_value(void)
{
	if (plist_head_empty(&ucc_req_list))
		return -1;

	return plist_last(&ucc_req_list)->prio;
}

static void update_cur_cstate(void)
{
	int value = ucc_get_value();

	if (value < 0) {
		cpumask_copy(&cur_cstate, cpu_possible_mask);
		return;
	}

	cpumask_copy(&cur_cstate, &ucc_configs[value].cstate);
}

enum {
	UCC_REMOVE_REQ,
	UCC_UPDATE_REQ,
	UCC_ADD_REQ,
};

static void ucc_update(struct ucc_req *req, int value, int action)
{
	int prev_value = ucc_get_value();
	switch (action) {
	case UCC_REMOVE_REQ:
		plist_del(&req->node, &ucc_req_list);
		req->active = 0;
		break;
	case UCC_UPDATE_REQ:
		plist_del(&req->node, &ucc_req_list);
	case UCC_ADD_REQ:
		plist_node_init(&req->node, value);
		plist_add(&req->node, &ucc_req_list);
		req->active = 1;
		break;
	}

	if (prev_value != ucc_get_value())
		update_cur_cstate();
}

void ucc_add_request(struct ucc_req *req, int value)
{
	unsigned long flags;

	spin_lock_irqsave(&ucc_lock, flags);

	if (req->active) {
		spin_unlock_irqrestore(&ucc_lock, flags);
		return;
	}

	ucc_update(req, value, UCC_ADD_REQ);

	spin_unlock_irqrestore(&ucc_lock, flags);
}

void ucc_update_request(struct ucc_req *req, int value)
{
	unsigned long flags;

	spin_lock_irqsave(&ucc_lock, flags);
	if (!req->active) {
		spin_unlock_irqrestore(&ucc_lock, flags);
		return;
	}

	ucc_update(req, value, UCC_UPDATE_REQ);

	spin_unlock_irqrestore(&ucc_lock, flags);
}

void ucc_remove_request(struct ucc_req *req)
{
	unsigned long flags;

	spin_lock_irqsave(&ucc_lock, flags);

	if (!req->active) {
		spin_unlock_irqrestore(&ucc_lock, flags);
		return;
	}

	ucc_update(req, 0, UCC_REMOVE_REQ);

	spin_unlock_irqrestore(&ucc_lock, flags);
}

/* Userspace owns one request; releasing it preserves in-kernel clients. */
static struct ucc_req userspace_ucc_req = { .name = "userspace" };
static DEFINE_MUTEX(userspace_ucc_lock);
static bool userspace_ucc_requested;

static ssize_t cstate_control_show(struct kobject *kobj,
                struct kobj_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
			READ_ONCE(userspace_ucc_requested));
}

static ssize_t cstate_control_store(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t count)
{
	int value;

	if (kstrtoint(buf, 10, &value) || value < 0 || value > 1)
		return -EINVAL;
	if (!ucc_initialized || !ucc_config_count)
		return -ENODEV;

	mutex_lock(&userspace_ucc_lock);
	if (value != userspace_ucc_requested) {
		/* Configuration zero is the existing 9810 UCC boost level. */
		if (value)
			ucc_add_request(&userspace_ucc_req, 0);
		else
			ucc_remove_request(&userspace_ucc_req);
		WRITE_ONCE(userspace_ucc_requested, !!value);
	}
	mutex_unlock(&userspace_ucc_lock);

	return count;
}

static struct kobj_attribute cstate_control =
__ATTR(cstate_control, 0644, cstate_control_show, cstate_control_store);

static ssize_t show_ucc_requests(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	struct ucc_req *req;
	int ret = 0;

	plist_for_each_entry(req, &ucc_req_list, node)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			" request : %d (%s)\n", req->node.prio, req->name);

	return ret;
}

static struct kobj_attribute ucc_requests =
__ATTR(ucc_requests, 0444, show_ucc_requests, NULL);


void __init init_exynos_ucc(void)
{
	struct device_node *dn, *child;
	int i = 0;

	dn = of_find_node_by_name(NULL, "cpuidle-ucc");
	ucc_config_count = of_get_child_count(dn);
	if (!ucc_config_count)
		return;

	ucc_configs = kcalloc(ucc_config_count,
				sizeof(struct ucc_config), GFP_KERNEL);
	if (!ucc_configs)
		return;

	for_each_child_of_node(dn, child) {
		const char *mask;

		of_property_read_u32(child, "index", &ucc_configs[i].index);

		if (of_property_read_string(child, "cstate", &mask))
			cpumask_clear(&ucc_configs[i].cstate);
		else
			cpulist_parse(mask, &ucc_configs[i].cstate);

		i++;
	}
	if (sysfs_create_file(power_kobj, &ucc_requests.attr))
		pr_err("failed to create ucc_requests node\n");

	cpumask_setall(&cur_cstate);
	ucc_initialized = true;
	ib_ucc_initialized = true;

	if (sysfs_create_file(power_kobj, &cstate_control.attr))
		pr_err("failed to create cstate_control node\n");
}
