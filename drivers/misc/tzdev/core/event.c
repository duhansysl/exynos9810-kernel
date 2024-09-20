/*
 * Copyright (C) 2012-2018, Samsung Electronics Co., Ltd.
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

#include <linux/kernel.h>
#include <asm/atomic.h>

#include "core/event.h"
#include "core/sysdep.h"

void tz_event_init(struct iwd_events_buf *buf, unsigned int size)
{
	buf->size = size;
	atomic_set(&buf->num_events, 0);
	memset(buf->events, 0, round_up(size, BITS_PER_INT) / BITS_PER_BYTE);
}

int tz_event_get_num_pending(struct iwd_events_buf *buf)
{
	return atomic_read(&buf->num_events);
}

int tz_event_add(struct iwd_events_buf *buf, unsigned int id)
{
	int prev;

	BUG_ON(id >= buf->size);

	prev = atomic_fetch_or(1 << id % BITS_PER_INT,
			(atomic_t *)&buf->events[id / BITS_PER_INT]);

	if (!(prev & (1 << id % BITS_PER_INT))) {
		smp_wmb();

		atomic_inc(&buf->num_events);

		return 1;
	}

	return 0;
}

static int popcount(int v)
{
	int i, num = 0;

	for (i = 0; i < BITS_PER_INT; i++)
		if (v & (1 << i))
			num++;

	return num;
}

void tz_event_process(struct iwd_events_buf *buf, void (*callback)(unsigned int id))
{
	unsigned int i, j;
	uint32_t tmp;

	for (i = 0; i < round_up(buf->size, BITS_PER_INT) / BITS_PER_INT; i++) {
		if (!atomic_read(&buf->num_events))
			return;

		smp_rmb();

		tmp = xchg(&buf->events[i], 0);

		if (!tmp)
			continue;

		smp_wmb();

		atomic_sub(popcount(tmp), &buf->num_events);

		for (j = 0; j < BITS_PER_INT; j++) {
			if (tmp & (1 << j))
				callback(i * BITS_PER_INT + j);
		}
	}
}
