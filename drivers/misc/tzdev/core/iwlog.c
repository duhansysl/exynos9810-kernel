/*
 * Copyright (C) 2013-2019 Samsung Electronics, Inc.
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

#include <linux/atomic.h>
#include <linux/kthread.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include "lib/circ_buf.h"

#include "tzdev_internal.h"
#include "core/iwio.h"
#include "core/iwlog.h"
#include "core/log.h"
#include "core/notifier.h"

#define TZ_IWLOG_BUF_SIZE		(CONFIG_TZLOG_PG_CNT * PAGE_SIZE - sizeof(struct circ_buf))

#define TZ_IWLOG_LINE_MAX_LEN		256
#define TZ_IWLOG_PREFIX			KERN_DEFAULT "SW> "

enum {
	TZ_IWLOG_WAIT,
	TZ_IWLOG_BUSY
};

struct tz_iwlog_print_state {
	char line[TZ_IWLOG_LINE_MAX_LEN + 1];	/* one byte for \0 */
	unsigned int line_len;
};

static atomic_t tz_iwlog_init_done = ATOMIC_INIT(0);

static DEFINE_PER_CPU(struct tz_iwlog_print_state, tz_iwlog_print_state);
static DEFINE_PER_CPU(struct circ_buf *, tz_iwlog_buffer);

static DECLARE_WAIT_QUEUE_HEAD(tz_iwlog_wq);
static atomic_t tz_iwlog_request = ATOMIC_INIT(0);

static struct task_struct *tz_iwlog_kthread;

static void tz_iwlog_buffer_print(const char *buf, unsigned int count)
{
	struct tz_iwlog_print_state *ps;
	unsigned int avail, bytes_in, bytes_out, bytes_printed, tmp, wait_dta;
	char *p;

	ps = &get_cpu_var(tz_iwlog_print_state);

	wait_dta = 0;
	while (count) {
		avail = TZ_IWLOG_LINE_MAX_LEN - ps->line_len;

		p = memchr(buf, '\n', count);

		if (p) {
			if (p - buf > avail) {
				bytes_in = avail;
				bytes_out = avail;
			} else {
				bytes_in = p - buf + 1;
				bytes_out = p - buf;
			}
		} else {
			if (count >= avail) {
				bytes_in = avail;
				bytes_out = avail;
			} else {
				bytes_in = count;
				bytes_out = count;

				wait_dta = 1;
			}
		}

		memcpy(&ps->line[ps->line_len], buf, bytes_out);
		ps->line_len += bytes_out;

		if (wait_dta)
			break;

		ps->line[ps->line_len] = 0;

		bytes_printed = 0;
		while (bytes_printed < ps->line_len) {
			tmp = printk(TZ_IWLOG_PREFIX "%s\n", &ps->line[bytes_printed]);
			if (!tmp)
				break;

			bytes_printed += tmp;
		}

		ps->line_len = 0;

		count -= bytes_in;
		buf += bytes_in;
	}

	put_cpu_var(tz_iwlog_print_state);
}

static void tz_iwlog_read_buffers(void)
{
	struct circ_buf *buf;
	unsigned int i, count, write_count;

	for_each_possible_cpu(i) {
		buf = per_cpu(tz_iwlog_buffer, i);
		if (!buf)
			continue;

		write_count = buf->write_count;
		if (write_count < buf->read_count) {
			count = TZ_IWLOG_BUF_SIZE - buf->read_count;
			if (count) {
				log_debug(tzdev_iwlog, "Read %d bytes from cpu=%d buffer.\n", count, i);
				tz_iwlog_buffer_print(buf->buffer + buf->read_count, count);
				buf->read_count = 0;
			}
		}

		count = write_count - buf->read_count;
		if (count) {
			log_debug(tzdev_iwlog, "Read %d bytes from cpu=%d buffer.\n", count, i);
			tz_iwlog_buffer_print(buf->buffer + buf->read_count, count);
			buf->read_count += count;
		}
	}
}

static void tz_iwlog_read_deferred(void)
{
	if (atomic_cmpxchg(&tz_iwlog_request, TZ_IWLOG_WAIT, TZ_IWLOG_BUSY) == TZ_IWLOG_WAIT)
		wake_up(&tz_iwlog_wq);
}

#if defined(CONFIG_TZLOG_POLLING)
static atomic_t nr_swd_cores = ATOMIC_INIT(0);

static int tz_iwlog_pre_smc_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	(void)cb;
	(void)code;
	(void)unused;

	atomic_inc(&nr_swd_cores);
	tz_iwlog_read_deferred();

	return NOTIFY_DONE;
}

static struct notifier_block tz_iwlog_pre_smc_notifier = {
	.notifier_call = tz_iwlog_pre_smc_call,
};
#endif

static int tz_iwlog_post_smc_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	(void)cb;
	(void)code;
	(void)unused;

#if defined(CONFIG_TZLOG_POLLING)
	atomic_dec(&nr_swd_cores);
#endif
	tz_iwlog_read_deferred();

	return NOTIFY_DONE;
}

static int tz_iwlog_kthread_handler(void *data)
{
	(void)data;

	log_debug(tzdev_iwlog, "Log kernel thread ready.\n");

	while (!kthread_should_stop()) {
#if defined(CONFIG_TZLOG_POLLING)
		if (atomic_read(&nr_swd_cores)) {
			wait_event_timeout(tz_iwlog_wq,
					atomic_xchg(&tz_iwlog_request, TZ_IWLOG_WAIT) == TZ_IWLOG_BUSY ||
					kthread_should_stop(),
					msecs_to_jiffies(CONFIG_TZLOG_POLLING_PERIOD));
			tz_iwlog_read_buffers();
			continue;
		}
#endif
		wait_event(tz_iwlog_wq,
				atomic_xchg(&tz_iwlog_request, TZ_IWLOG_WAIT) == TZ_IWLOG_BUSY ||
				kthread_should_stop());
		tz_iwlog_read_buffers();
	}

	log_debug(tzdev_iwlog, "Log kernel thread exited.\n");

	return 0;
}

static struct notifier_block tz_iwlog_post_smc_notifier = {
	.notifier_call = tz_iwlog_post_smc_call,
};

int tz_iwlog_init(void)
{
	int ret;
	unsigned int i;

	for (i = 0; i < NR_SW_CPU_IDS; ++i) {
		struct circ_buf *buffer = tz_iwio_alloc_iw_channel(
				TZ_IWIO_CONNECT_LOG, CONFIG_TZLOG_PG_CNT,
				NULL, NULL, NULL);
		if (IS_ERR(buffer)) {
			log_error(tzdev_iwlog, "Failed to allocate iw channel, cpu=%u, error=%ld\n",
					i, PTR_ERR(buffer));
			return PTR_ERR(buffer);
		}

		if (cpu_possible(i))
			per_cpu(tz_iwlog_buffer, i) = buffer;
	}

	tz_iwlog_kthread = kthread_run(tz_iwlog_kthread_handler, NULL, "tz_iwlog_thread");
	if (IS_ERR(tz_iwlog_kthread)) {
		log_error(tzdev_iwlog, "Failed to create kernel thread, error=%ld\n",
				PTR_ERR(tz_iwlog_kthread));
		return PTR_ERR(tz_iwlog_kthread);
	}

	ret = tzdev_atomic_notifier_register(TZDEV_POST_SMC_NOTIFIER, &tz_iwlog_post_smc_notifier);
	if (ret) {
		log_error(tzdev_iwlog, "Failed to register post smc notifier, error=%d\n", ret);
		goto post_smc_notifier_fail;
	}
#if defined(CONFIG_TZLOG_POLLING)
	ret = tzdev_atomic_notifier_register(TZDEV_PRE_SMC_NOTIFIER, &tz_iwlog_pre_smc_notifier);
	if (ret) {
		log_error(tzdev_iwlog, "Failed to register pre smc notifier, error=%d\n", ret);
		goto pre_smc_notifier_fail;
	}
#endif
	atomic_set(&tz_iwlog_init_done, 1);

	log_info(tzdev_iwlog, "IW log initialization done.\n");

	return 0;

#if defined(CONFIG_TZLOG_POLLING)
pre_smc_notifier_fail:
	tzdev_atomic_notifier_unregister(TZDEV_POST_SMC_NOTIFIER, &tz_iwlog_post_smc_notifier);
#endif
post_smc_notifier_fail:
	kthread_stop(tz_iwlog_kthread);

	return ret;
}

void tz_iwlog_fini(void)
{
	if (atomic_cmpxchg(&tz_iwlog_init_done, 1, 0))
		return;

	kthread_stop(tz_iwlog_kthread);
	tz_iwlog_read_buffers();
	tzdev_atomic_notifier_unregister(TZDEV_POST_SMC_NOTIFIER, &tz_iwlog_post_smc_notifier);
#if defined(CONFIG_TZLOG_POLLING)
	tzdev_atomic_notifier_unregister(TZDEV_PRE_SMC_NOTIFIER, &tz_iwlog_pre_smc_notifier);
#endif
	log_info(tzdev_iwlog, "IW log finalization done.\n");
}
