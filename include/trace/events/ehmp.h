/*
 *  Copyright (C) 2017 Park Bumgyu <bumgyu.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ehmp

#if !defined(_TRACE_EHMP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EHMP_H

#include <linux/sched.h>
#include <linux/tracepoint.h>

/*
 * Tracepoint for selection of boost cpu
 */
TRACE_EVENT(ehmp_select_boost_cpu,

	TP_PROTO(struct task_struct *p, int cpu, int trigger, char *state),

	TP_ARGS(p, cpu, trigger, state),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cpu			)
		__field(	int,		trigger			)
		__array(	char,		state,		64	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->cpu		= cpu;
		__entry->trigger	= trigger;
		memcpy(__entry->state, state, 64);
	),

	TP_printk("comm=%s pid=%d target_cpu=%d trigger=%d state=%s",
		  __entry->comm, __entry->pid, __entry->cpu,
		  __entry->trigger, __entry->state)
);

/*
 * Tracepoint for selection of group balancer
 */
TRACE_EVENT(ehmp_select_group_boost,

	TP_PROTO(struct task_struct *p, int cpu, char *state),

	TP_ARGS(p, cpu, state),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cpu			)
		__array(	char,		state,		64	)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->cpu		= cpu;
		memcpy(__entry->state, state, 64);
	),

	TP_printk("comm=%s pid=%d target_cpu=%d state=%s",
		  __entry->comm, __entry->pid, __entry->cpu, __entry->state)
);

TRACE_EVENT(ehmp_global_boost,

	TP_PROTO(char *name, unsigned long boost),

	TP_ARGS(name, boost),

	TP_STRUCT__entry(
		__array(	char,		name,		64	)
		__field(	unsigned long,	boost			)
	),

	TP_fast_assign(
		memcpy(__entry->name, name, 64);
		__entry->boost		= boost;
	),

	TP_printk("name=%s global_boost_value=%ld", __entry->name, __entry->boost)
);

/*
 * Tracepoint for prefer idle
 */
TRACE_EVENT(ehmp_prefer_idle,

	TP_PROTO(struct task_struct *p, int orig_cpu, int target_cpu,
		unsigned long task_util, unsigned long new_util, int idle),

	TP_ARGS(p, orig_cpu, target_cpu, task_util, new_util, idle),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		orig_cpu		)
		__field(	int,		target_cpu		)
		__field(	unsigned long,	task_util		)
		__field(	unsigned long,	new_util		)
		__field(	int,		idle			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->orig_cpu	= orig_cpu;
		__entry->target_cpu	= target_cpu;
		__entry->task_util	= task_util;
		__entry->new_util	= new_util;
		__entry->idle		= idle;
	),

	TP_printk("comm=%s pid=%d orig_cpu=%d target_cpu=%d task_util=%lu new_util=%lu idle=%d",
		__entry->comm, __entry->pid, __entry->orig_cpu, __entry->target_cpu,
		__entry->task_util, __entry->new_util, __entry->idle)
);

TRACE_EVENT(ehmp_prefer_idle_cpu_select,

	TP_PROTO(struct task_struct *p, int cpu),

	TP_ARGS(p, cpu),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	int,		cpu			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->cpu		= cpu;
	),

	TP_printk("comm=%s pid=%d target_cpu=%d",
		  __entry->comm, __entry->pid, __entry->cpu)
);

/*
 * Tracepoint for cpu selection
 */
TRACE_EVENT(ehmp_find_best_target_stat,

	TP_PROTO(int cpu, unsigned long cap, unsigned long util, unsigned long target_util),

	TP_ARGS(cpu, cap, util, target_util),

	TP_STRUCT__entry(
		__field( int,		cpu	)
		__field( unsigned long, cap	)
		__field( unsigned long, util	)
		__field( unsigned long, target_util	)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->cap = cap;
		__entry->util = util;
		__entry->target_util = target_util;
	),

	TP_printk("find_best : [cpu%d] capacity %lu, util %lu, target_util %lu\n",
		__entry->cpu, __entry->cap, __entry->util, __entry->target_util)
);

TRACE_EVENT(ehmp_find_best_target_candi,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field( unsigned int, cpu	)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
	),

	TP_printk("find_best: energy candidate cpu %d\n", __entry->cpu)
);

TRACE_EVENT(ehmp_find_best_target_cpu,

	TP_PROTO(unsigned int cpu, unsigned long target_util),

	TP_ARGS(cpu, target_util),

	TP_STRUCT__entry(
		__field( unsigned int, cpu	)
		__field( unsigned long, target_util	)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->target_util = target_util;
	),

	TP_printk("find_best: target_cpu %d, target_util %lu\n", __entry->cpu, __entry->target_util)
);

/*
 * Tracepoint for ontime migration
 */
TRACE_EVENT(ehmp_ontime_migration,

	TP_PROTO(struct task_struct *p, unsigned long load,
		int src_cpu, int dst_cpu, int boost_migration),

	TP_ARGS(p, load, src_cpu, dst_cpu, boost_migration),

	TP_STRUCT__entry(
		__array(	char,		comm,	TASK_COMM_LEN	)
		__field(	pid_t,		pid			)
		__field(	unsigned long,	load			)
		__field(	int,		src_cpu			)
		__field(	int,		dst_cpu			)
		__field(	int,		bm			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, p->comm, TASK_COMM_LEN);
		__entry->pid		= p->pid;
		__entry->load		= load;
		__entry->src_cpu	= src_cpu;
		__entry->dst_cpu	= dst_cpu;
		__entry->bm		= boost_migration;
	),

	TP_printk("comm=%s pid=%d ontime_load_avg=%lu src_cpu=%d dst_cpu=%d boost_migration=%d",
		__entry->comm, __entry->pid, __entry->load,
		__entry->src_cpu, __entry->dst_cpu, __entry->bm)
);

/*
 * Tracepoint for accounting ontime load averages for tasks.
 */
TRACE_EVENT(ehmp_ontime_new_entity_load,

	TP_PROTO(struct task_struct *tsk, struct ontime_avg *avg),

	TP_ARGS(tsk, avg),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN		)
		__field( pid_t,		pid				)
		__field( int,		cpu				)
		__field( unsigned long,	load_avg			)
		__field( u64,		load_sum			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->cpu			= task_cpu(tsk);
		__entry->load_avg		= avg->load_avg;
		__entry->load_sum		= avg->load_sum;
	),
	TP_printk("comm=%s pid=%d cpu=%d load_avg=%lu load_sum=%llu",
		  __entry->comm,
		  __entry->pid,
		  __entry->cpu,
		  __entry->load_avg,
		  (u64)__entry->load_sum)
);

/*
 * Tracepoint for accounting ontime load averages for tasks.
 */
TRACE_EVENT(ehmp_ontime_load_avg_task,

	TP_PROTO(struct task_struct *tsk, struct ontime_avg *avg),

	TP_ARGS(tsk, avg),

	TP_STRUCT__entry(
		__array( char,		comm,	TASK_COMM_LEN		)
		__field( pid_t,		pid				)
		__field( int,		cpu				)
		__field( unsigned long,	load_avg			)
		__field( u64,		load_sum			)
	),

	TP_fast_assign(
		memcpy(__entry->comm, tsk->comm, TASK_COMM_LEN);
		__entry->pid			= tsk->pid;
		__entry->cpu			= task_cpu(tsk);
		__entry->load_avg		= avg->load_avg;
		__entry->load_sum		= avg->load_sum;
	),
	TP_printk("comm=%s pid=%d cpu=%d load_avg=%lu load_sum=%llu",
		  __entry->comm,
		  __entry->pid,
		  __entry->cpu,
		  __entry->load_avg,
		  (u64)__entry->load_sum)
);

TRACE_EVENT(ehmp_ontime_check_migrate,

       TP_PROTO(int cpu, int migrate, char *label),

       TP_ARGS(cpu, migrate, label),

       TP_STRUCT__entry(
               __array(char, label, 64)
               __field(int, cpu)
               __field(int, migrate)
       ),

       TP_fast_assign(
               strncpy(__entry->label, label, 64);
               __entry->cpu   = cpu;
               __entry->migrate = migrate;
       ),

       TP_printk("target_cpu=%d migrate?=%d reason=%s",
               __entry->cpu, __entry->migrate,
               __entry->label)
);

#endif /* _TRACE_EHMP_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
