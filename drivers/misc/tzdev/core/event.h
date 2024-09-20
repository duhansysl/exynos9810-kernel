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

#ifndef __TZ_EVENT_H__
#define __TZ_EVENT_H__

#define BITS_PER_INT			(BITS_PER_BYTE * sizeof(int))

struct iwd_events_buf {
	unsigned int size;
	atomic_t num_events;
	uint32_t events[];
};

void tz_event_init(struct iwd_events_buf *buf, unsigned int size);
int tz_event_get_num_pending(struct iwd_events_buf *buf);
int tz_event_add(struct iwd_events_buf *buf, unsigned int id);
void tz_event_process(struct iwd_events_buf *buf, void (*callback)(unsigned int id));

#endif /* __TZ_EVENT_H__ */
