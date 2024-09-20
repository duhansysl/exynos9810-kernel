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

#ifndef __TZ_WAIT_H__
#define __TZ_WAIT_H__

#include <linux/freezer.h>
#include <linux/wait.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 10, 0)
#include <linux/sched.h>
#else
#include <linux/sched/signal.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
#define __wait_event_uninterruptible_freezable(wq_head, condition)		\
do {										\
	DEFINE_WAIT(__wait);							\
										\
	for (;;) {								\
		prepare_to_wait(&wq_head, &__wait, TASK_UNINTERRUPTIBLE);	\
		if (condition)							\
			break;							\
		freezable_schedule();						\
	}									\
	finish_wait(&wq_head, &__wait);						\
} while (0)

#define wait_event_uninterruptible_freezable_nested(wq_head, condition)		\
do {										\
	if (condition)								\
		break;								\
	__wait_event_uninterruptible_freezable(wq_head, condition);		\
} while (0)

#define wait_event_interruptible_nested(wq, condition)				\
	wait_event_interruptible(wq, condition)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
#define wait_event_uninterruptible_freezable_nested(wq_head, condition)		\
do {										\
	might_sleep();								\
	if (condition)								\
		break;								\
	__wait_event(wq_head, condition, TASK_UNINTERRUPTIBLE, 0, 0,		\
			freezable_schedule);					\
} while (0)

#define wait_event_interruptible_nested(wq, condition)				\
	wait_event_interruptible(wq, condition)
#else
#define wait_event_uninterruptible_freezable_nested(wq, condition)		\
({										\
	DEFINE_WAIT_FUNC(wait, woken_wake_function);				\
	add_wait_queue(&wq, &wait);						\
	freezer_do_not_count();							\
	while (!(condition))							\
		wait_woken(&wait, TASK_UNINTERRUPTIBLE,				\
			MAX_SCHEDULE_TIMEOUT);					\
	freezer_count();							\
	remove_wait_queue(&wq, &wait);						\
})

#define wait_event_interruptible_nested(wq, condition)				\
({										\
	DEFINE_WAIT_FUNC(wait, woken_wake_function);				\
	long __ret = 0;								\
	add_wait_queue(&wq, &wait);						\
	while (!(condition)) {							\
		if (signal_pending(current)) {					\
			__ret = -ERESTARTSYS;					\
			break;							\
		}								\
		wait_woken(&wait, TASK_INTERRUPTIBLE,				\
			MAX_SCHEDULE_TIMEOUT);					\
	}									\
	remove_wait_queue(&wq, &wait);						\
	__ret;									\
})
#endif

#endif /* __TZ_WAIT_H__ */
