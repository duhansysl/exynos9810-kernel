/* drivers/misc/lowmemorykiller.c
 *
 * The lowmemorykiller driver lets user-space specify a set of memory thresholds
 * where processes with a range of oom_score_adj values will get killed. Specify
 * the minimum oom_score_adj values in
 * /sys/module/lowmemorykiller/parameters/adj and the number of free pages in
 * /sys/module/lowmemorykiller/parameters/minfree. Both files take a comma
 * separated list of numbers in ascending order.
 *
 * For example, write "0,8" to /sys/module/lowmemorykiller/parameters/adj and
 * "1024,4096" to /sys/module/lowmemorykiller/parameters/minfree to kill
 * processes with a oom_score_adj value of 8 or higher when the free memory
 * drops below 4096 pages and kill processes with a oom_score_adj value of 0 or
 * higher when the free memory drops below 1024 pages.
 *
 * The driver considers memory used for caches to be free, but if a large
 * percentage of the cached memory is locked this can be very inaccurate
 * and processes may not get killed until the normal oom killer is triggered.
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/cma.h>
#include <linux/oom.h>
#include <linux/sched.h>
#include <linux/swap.h>
#include <linux/rcupdate.h>
#include <linux/profile.h>
#include <linux/notifier.h>
#include <linux/ratelimit.h>
#include <linux/circ_buf.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/poll.h>

#define CREATE_TRACE_POINTS
#include "trace/lowmemorykiller.h"

static int lmkd_count;
static int lmkd_cricount;

static u32 lowmem_debug_level = 1;
static short lowmem_adj[6] = {
	0,
	1,
	6,
	12,
};

static int lowmem_adj_size = 4;
static int lowmem_minfree[6] = {
	3 * 512,	/* 6MB */
	2 * 1024,	/* 8MB */
	4 * 1024,	/* 16MB */
	16 * 1024,	/* 64MB */
};

static int lowmem_minfree_size = 4;
static u32 lowmem_lmkcount;

static unsigned long lowmem_deathpending_timeout;

#define lowmem_print(level, x...)			\
	do {						\
		if (lowmem_debug_level >= (level))	\
			pr_info(x);			\
	} while (0)

static int test_task_flag(struct task_struct *p, int flag)
{
	struct task_struct *t = p;

	do {
		if (test_tsk_thread_flag(t, flag))
			return 1;
	} while_each_thread(p, t);

	return 0;
}

static void show_memory(void)
{
	unsigned long nr_rbin_free, nr_rbin_pool, nr_rbin_alloc, nr_rbin_file;

	nr_rbin_free = global_page_state(NR_FREE_RBIN_PAGES);
	nr_rbin_pool = atomic_read(&rbin_pool_pages);
	nr_rbin_alloc = atomic_read(&rbin_allocated_pages);
	nr_rbin_file = totalrbin_pages - nr_rbin_free - nr_rbin_pool
					- nr_rbin_alloc;

#define K(x) ((x) << (PAGE_SHIFT - 10))
	printk("Mem-Info:"
		" totalram_pages:%lukB"
		" free:%lukB"
		" active_anon:%lukB"
		" inactive_anon:%lukB"
		" active_file:%lukB"
		" inactive_file:%lukB"
		" unevictable:%lukB"
		" isolated(anon):%lukB"
		" isolated(file):%lukB"
		" dirty:%lukB"
		" writeback:%lukB"
		" mapped:%lukB"
		" shmem:%lukB"
		" slab_reclaimable:%lukB"
		" slab_unreclaimable:%lukB"
		" kernel_stack:%lukB"
		" pagetables:%lukB"
		" free_cma:%lukB"
		" rbin_free:%lukB"
		" rbin_pool:%lukB"
		" rbin_alloc:%lukB"
		" rbin_file:%lukB"
		"\n",
		K(totalram_pages),
		K(global_page_state(NR_FREE_PAGES)),
		K(global_node_page_state(NR_ACTIVE_ANON)),
		K(global_node_page_state(NR_INACTIVE_ANON)),
		K(global_node_page_state(NR_ACTIVE_FILE)),
		K(global_node_page_state(NR_INACTIVE_FILE)),
		K(global_node_page_state(NR_UNEVICTABLE)),
		K(global_node_page_state(NR_ISOLATED_ANON)),
		K(global_node_page_state(NR_ISOLATED_FILE)),
		K(global_node_page_state(NR_FILE_DIRTY)),
		K(global_node_page_state(NR_WRITEBACK)),
		K(global_node_page_state(NR_FILE_MAPPED)),
		K(global_node_page_state(NR_SHMEM)),
		K(global_page_state(NR_SLAB_RECLAIMABLE)),
		K(global_page_state(NR_SLAB_UNRECLAIMABLE)),
		global_page_state(NR_KERNEL_STACK_KB),
		K(global_page_state(NR_PAGETABLE)),
		K(global_page_state(NR_FREE_CMA_PAGES)),
		K(nr_rbin_free),
		K(nr_rbin_pool),
		K(nr_rbin_alloc),
		K(nr_rbin_file)
		);
#undef K
}

static DECLARE_WAIT_QUEUE_HEAD(event_wait);
static DEFINE_SPINLOCK(lmk_event_lock);
static struct circ_buf event_buffer;
#define MAX_BUFFERED_EVENTS 8
#define MAX_TASKNAME 128

struct lmk_event {
	char taskname[MAX_TASKNAME];
	pid_t pid;
	uid_t uid;
	pid_t group_leader_pid;
	unsigned long min_flt;
	unsigned long maj_flt;
	unsigned long rss_in_pages;
	short oom_score_adj;
	short min_score_adj;
	unsigned long long start_time;
	struct list_head list;
};

void handle_lmk_event(struct task_struct *selected, int selected_tasksize,
		      short min_score_adj)
{
	int head;
	int tail;
	struct lmk_event *events;
	struct lmk_event *event;

	spin_lock(&lmk_event_lock);

	head = event_buffer.head;
	tail = READ_ONCE(event_buffer.tail);

	/* Do not continue to log if no space remains in the buffer. */
	if (CIRC_SPACE(head, tail, MAX_BUFFERED_EVENTS) < 1) {
		spin_unlock(&lmk_event_lock);
		return;
	}

	events = (struct lmk_event *) event_buffer.buf;
	event = &events[head];

	strncpy(event->taskname, selected->comm, MAX_TASKNAME);

	event->pid = selected->pid;
	event->uid = from_kuid_munged(current_user_ns(), task_uid(selected));
	if (selected->group_leader)
		event->group_leader_pid = selected->group_leader->pid;
	else
		event->group_leader_pid = -1;
	event->min_flt = selected->min_flt;
	event->maj_flt = selected->maj_flt;
	event->oom_score_adj = selected->signal->oom_score_adj;
	event->start_time = nsec_to_clock_t(selected->real_start_time);
	event->rss_in_pages = selected_tasksize;
	event->min_score_adj = min_score_adj;

	event_buffer.head = (head + 1) & (MAX_BUFFERED_EVENTS - 1);

	spin_unlock(&lmk_event_lock);

	wake_up_interruptible(&event_wait);
}

static int lmk_event_show(struct seq_file *s, void *unused)
{
	struct lmk_event *events = (struct lmk_event *) event_buffer.buf;
	int head;
	int tail;
	struct lmk_event *event;

	spin_lock(&lmk_event_lock);

	head = event_buffer.head;
	tail = event_buffer.tail;

	if (head == tail) {
		spin_unlock(&lmk_event_lock);
		return -EAGAIN;
	}

	event = &events[tail];

	seq_printf(s, "%lu %lu %lu %lu %lu %lu %hd %hd %llu\n%s\n",
		(unsigned long) event->pid, (unsigned long) event->uid,
		(unsigned long) event->group_leader_pid, event->min_flt,
		event->maj_flt, event->rss_in_pages, event->oom_score_adj,
		event->min_score_adj, event->start_time, event->taskname);

	event_buffer.tail = (tail + 1) & (MAX_BUFFERED_EVENTS - 1);

	spin_unlock(&lmk_event_lock);
	return 0;
}

static unsigned int lmk_event_poll(struct file *file, poll_table *wait)
{
	int ret = 0;

	poll_wait(file, &event_wait, wait);
	spin_lock(&lmk_event_lock);
	if (event_buffer.head != event_buffer.tail)
		ret = POLLIN;
	spin_unlock(&lmk_event_lock);
	return ret;
}

static int lmk_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, lmk_event_show, inode->i_private);
}

static const struct file_operations event_file_ops = {
	.open = lmk_event_open,
	.poll = lmk_event_poll,
	.read = seq_read
};

static void lmk_event_init(void)
{
	struct proc_dir_entry *entry;

	event_buffer.head = 0;
	event_buffer.tail = 0;
	event_buffer.buf = kmalloc(
		sizeof(struct lmk_event) * MAX_BUFFERED_EVENTS, GFP_KERNEL);
	if (!event_buffer.buf)
		return;
	entry = proc_create("lowmemorykiller", 0, NULL, &event_file_ops);
	if (!entry)
		pr_err("error creating kernel lmk event file\n");
}

static unsigned long lowmem_count(struct shrinker *s,
				  struct shrink_control *sc)
{
	return global_node_page_state(NR_ACTIVE_ANON) +
		global_node_page_state(NR_ACTIVE_FILE) +
		global_node_page_state(NR_INACTIVE_ANON) +
		global_node_page_state(NR_INACTIVE_FILE);
}

static unsigned long lowmem_scan(struct shrinker *s, struct shrink_control *sc)
{
	struct task_struct *tsk;
	struct task_struct *selected = NULL;
	unsigned long rem = 0;
	int tasksize;
	int i;
	short min_score_adj = OOM_SCORE_ADJ_MAX + 1;
	int minfree = 0;
	int selected_tasksize = 0;
	short selected_oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);
	int other_free = global_page_state(NR_FREE_PAGES) - totalreserve_pages;
	int other_file = global_node_page_state(NR_FILE_PAGES) -
				global_node_page_state(NR_SHMEM) -
				global_node_page_state(NR_UNEVICTABLE) -
				total_swapcache_pages();
	static DEFINE_RATELIMIT_STATE(lmk_rs, DEFAULT_RATELIMIT_INTERVAL, 1);
	unsigned long nr_cma_free;
	unsigned long nr_rbin_free, nr_rbin_pool, nr_rbin_alloc, nr_rbin_file;
	int migratetype;
#if defined(CONFIG_SWAP)
	unsigned long swap_orig_nrpages;
	unsigned long swap_comp_nrpages;
	int swap_rss;
	int selected_swap_rss;

	swap_orig_nrpages = get_swap_orig_data_nrpages();
	swap_comp_nrpages = get_swap_comp_pool_nrpages();
#endif

	nr_cma_free = global_page_state(NR_FREE_CMA_PAGES);
	migratetype = gfpflags_to_migratetype(sc->gfp_mask);
	if (!((migratetype == MIGRATE_MOVABLE) &&
		((sc->gfp_mask & GFP_HIGHUSER_MOVABLE) == GFP_HIGHUSER_MOVABLE)))
		other_free -= nr_cma_free;
	if ((sc->gfp_mask & __GFP_RBIN) != __GFP_RBIN) {
		nr_rbin_free = global_page_state(NR_FREE_RBIN_PAGES);
		nr_rbin_pool = atomic_read(&rbin_pool_pages);
		nr_rbin_alloc = atomic_read(&rbin_allocated_pages);
		nr_rbin_file = totalrbin_pages - nr_rbin_free - nr_rbin_pool
						- nr_rbin_alloc;
		other_free -= nr_rbin_free;
		other_file -= nr_rbin_file;
	}

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;
	if (lowmem_minfree_size < array_size)
		array_size = lowmem_minfree_size;
	for (i = 0; i < array_size; i++) {
		minfree = lowmem_minfree[i];
		if (other_free < minfree && other_file < minfree) {
			min_score_adj = lowmem_adj[i];
			break;
		}
	}

	lowmem_print(3, "lowmem_scan %lu, %x, ofree %d %d, ma %hd\n",
		     sc->nr_to_scan, sc->gfp_mask, other_free,
		     other_file, min_score_adj);

	if (min_score_adj == OOM_SCORE_ADJ_MAX + 1) {
		lowmem_print(5, "lowmem_scan %lu, %x, return 0\n",
			     sc->nr_to_scan, sc->gfp_mask);
		return SHRINK_STOP;
	}

	selected_oom_score_adj = min_score_adj;

	rcu_read_lock();
	for_each_process(tsk) {
		struct task_struct *p;
		short oom_score_adj;

		if (tsk->flags & PF_KTHREAD)
			continue;

		if (same_thread_group(tsk, current))
			continue;

		if (test_task_flag(tsk, TIF_MEMALLOC))
			continue;

		p = find_lock_task_mm(tsk);
		if (!p)
			continue;

		if (task_lmk_waiting(p)) {
			task_unlock(p);

			if (time_before_eq(jiffies,
					   lowmem_deathpending_timeout)) {
				rcu_read_unlock();
				return SHRINK_STOP;
			}

			continue;
		}
		if (p->state & TASK_UNINTERRUPTIBLE) {
			task_unlock(p);
			continue;
		}
		oom_score_adj = p->signal->oom_score_adj;
		if (oom_score_adj < min_score_adj) {
			task_unlock(p);
			continue;
		}
		tasksize = get_mm_rss(p->mm);
#if defined(CONFIG_SWAP)
		swap_rss = get_mm_counter(p->mm, MM_SWAPENTS) *
				swap_comp_nrpages / swap_orig_nrpages;
		lowmem_print(3, "%s tasksize rss: %d swap_rss: %d swap: %lu/%lu\n",
			     __func__, tasksize, swap_rss, swap_comp_nrpages,
			     swap_orig_nrpages);
		tasksize += swap_rss;
#endif
		task_unlock(p);
		if (tasksize <= 0)
			continue;
		if (selected) {
			if (oom_score_adj < selected_oom_score_adj)
				continue;
			if (oom_score_adj == selected_oom_score_adj &&
			    tasksize <= selected_tasksize)
				continue;
		}
		selected = p;
		selected_tasksize = tasksize;
#if defined(CONFIG_SWAP)
		selected_swap_rss = swap_rss;
#endif
		selected_oom_score_adj = oom_score_adj;
		lowmem_print(2, "select '%s' (%d), adj %hd, size %d, to kill\n",
			     p->comm, p->pid, oom_score_adj, tasksize);
	}
	if (selected) {
		long cache_size = other_file * (long)(PAGE_SIZE / 1024);
		long cache_limit = minfree * (long)(PAGE_SIZE / 1024);
		long free = other_free * (long)(PAGE_SIZE / 1024);
#if defined(CONFIG_SWAP)
		int orig_tasksize = selected_tasksize - selected_swap_rss;
#endif

		task_lock(selected);
		send_sig(SIGKILL, selected, 0);
		if (selected->mm)
			task_set_lmk_waiting(selected);
		task_unlock(selected);
		trace_lowmemory_kill(selected, cache_size, cache_limit, free);
		lowmem_print(1, "Killing '%s' (%d) (tgid %d), adj %hd,\n"
#if defined(CONFIG_SWAP)
				 "   to free %ldkB (%ldKB %ldKB) on behalf of '%s' (%d) because\n"
#else
				 "   to free %ldkB on behalf of '%s' (%d) because\n"
#endif
				 "   cache %ldkB is below limit %ldkB for oom_score_adj %hd\n"
				 "   Free memory is %ldkB above reserved\n"
				 "   Free CMA is %ldkB\n"
				 "   GFP mask is %#x(%pGg)\n",
			     selected->comm, selected->pid, selected->tgid,
			     selected_oom_score_adj,
#if defined(CONFIG_SWAP)
			     selected_tasksize * (long)(PAGE_SIZE / 1024),
			     orig_tasksize * (long)(PAGE_SIZE / 1024),
			     selected_swap_rss * (long)(PAGE_SIZE / 1024),
#else
			     selected_tasksize * (long)(PAGE_SIZE / 1024),
#endif
			     current->comm, current->pid,
			     cache_size, cache_limit,
			     min_score_adj,
			     free,
			     nr_cma_free * (long)(PAGE_SIZE / 1024),
			     sc->gfp_mask, &sc->gfp_mask);
		show_mem_extra_call_notifiers();
		show_memory();
		if ((selected_oom_score_adj <= 100) && (__ratelimit(&lmk_rs)))
				dump_tasks(NULL, NULL);

		lowmem_deathpending_timeout = jiffies + HZ;
		rem += selected_tasksize;
		get_task_struct(selected);
		lowmem_lmkcount++;
	}

	lowmem_print(4, "lowmem_scan %lu, %x, return %lu\n",
		     sc->nr_to_scan, sc->gfp_mask, rem);
	rcu_read_unlock();

	if (selected) {
		handle_lmk_event(selected, selected_tasksize, min_score_adj);
		put_task_struct(selected);
	}
	if (!rem)
		rem = SHRINK_STOP;

	return rem;
}

static struct shrinker lowmem_shrinker = {
	.scan_objects = lowmem_scan,
	.count_objects = lowmem_count,
	.seeks = DEFAULT_SEEKS * 16
};

static int __init lowmem_init(void)
{
	register_shrinker(&lowmem_shrinker);
	lmk_event_init();
	return 0;
}

static void __exit lowmem_exit(void)
{
		unregister_shrinker(&lowmem_shrinker);
}

#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
static short lowmem_oom_adj_to_oom_score_adj(short oom_adj)
{
	if (oom_adj == OOM_ADJUST_MAX)
		return OOM_SCORE_ADJ_MAX;
	else
		return (oom_adj * OOM_SCORE_ADJ_MAX) / -OOM_DISABLE;
}

static void lowmem_autodetect_oom_adj_values(void)
{
	int i;
	short oom_adj;
	short oom_score_adj;
	int array_size = ARRAY_SIZE(lowmem_adj);

	if (lowmem_adj_size < array_size)
		array_size = lowmem_adj_size;

	if (array_size <= 0)
		return;

	oom_adj = lowmem_adj[array_size - 1];
	if (oom_adj > OOM_ADJUST_MAX)
		return;

	oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
	if (oom_score_adj <= OOM_ADJUST_MAX)
		return;

	lowmem_print(1, "lowmem_shrink: convert oom_adj to oom_score_adj:\n");
	for (i = 0; i < array_size; i++) {
		oom_adj = lowmem_adj[i];
		oom_score_adj = lowmem_oom_adj_to_oom_score_adj(oom_adj);
		lowmem_adj[i] = oom_score_adj;
		lowmem_print(1, "oom_adj %d => oom_score_adj %d\n",
			     oom_adj, oom_score_adj);
	}
}

static int lowmem_adj_array_set(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_array_ops.set(val, kp);

	/* HACK: Autodetect oom_adj values in lowmem_adj array */
	lowmem_autodetect_oom_adj_values();

	return ret;
}

static int lowmem_adj_array_get(char *buffer, const struct kernel_param *kp)
{
	return param_array_ops.get(buffer, kp);
}

static void lowmem_adj_array_free(void *arg)
{
	param_array_ops.free(arg);
}

static struct kernel_param_ops lowmem_adj_array_ops = {
	.set = lowmem_adj_array_set,
	.get = lowmem_adj_array_get,
	.free = lowmem_adj_array_free,
};

static const struct kparam_array __param_arr_adj = {
	.max = ARRAY_SIZE(lowmem_adj),
	.num = &lowmem_adj_size,
	.ops = &param_ops_short,
	.elemsize = sizeof(lowmem_adj[0]),
	.elem = lowmem_adj,
};
#endif

/*
 * not really modular, but the easiest way to keep compat with existing
 * bootargs behaviour is to continue using module_param here.
 */
module_param_named(cost, lowmem_shrinker.seeks, int, 0644);
#ifdef CONFIG_ANDROID_LOW_MEMORY_KILLER_AUTODETECT_OOM_ADJ_VALUES
module_param_cb(adj, &lowmem_adj_array_ops,
		.arr = &__param_arr_adj,
		0644);
__MODULE_PARM_TYPE(adj, "array of short");
#else
module_param_array_named(adj, lowmem_adj, short, &lowmem_adj_size, 0644);
#endif
module_param_array_named(minfree, lowmem_minfree, uint, &lowmem_minfree_size,
			 0644);
module_param_named(debug_level, lowmem_debug_level, uint, 0644);
module_param_named(lmkcount, lowmem_lmkcount, uint, 0444);
module_param_named(lmkd_count, lmkd_count, int, 0644);
module_param_named(lmkd_cricount, lmkd_cricount, int, 0644);

module_init(lowmem_init);
module_exit(lowmem_exit);

MODULE_LICENSE("GPL");
