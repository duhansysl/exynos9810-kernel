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

#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/hrtimer.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#if defined(CONFIG_ARCH_MSM) && defined(CONFIG_MSM_SCM)
#if defined(CONFIG_ARCH_MSM8939) || defined(CONFIG_ARCH_MSM8996)
#include <asm/cacheflush.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/qseecomi.h>
#else
#error "Unsupported target! The only MSM8996 and MSM8939 are supported for Qualcomm chipset"
#endif
#endif

#include "tzdev_internal.h"
#include "core/cdev.h"
#include "core/kthread_pool.h"
#include "core/log.h"
#include "core/platform.h"
#include "core/qc_clocks.h"

#define BF_SMC_SUCCESS		0
#define BF_SMC_INTERRUPTED	1
#define BF_SMC_KERNEL_PANIC	2

/* svc_id and cmd_id to call QSEE 3-rd party smc handler */
#define TZ_SVC_EXECUTIVE_EXT		250
#define TZ_CMD_ID_EXEC_SMC_EXT		1

#define SCM_V2_EBUSY			-12

#define SCM_TZM_FNID(s, c) (((((s) & 0xFF) << 8) | ((c) & 0xFF)) | 0x33000000)

#define TZ_EXECUTIVE_EXT_ID_PARAM_ID \
		TZ_SYSCALL_CREATE_PARAM_ID_4( \
		TZ_SYSCALL_PARAM_TYPE_BUF_RW, TZ_SYSCALL_PARAM_TYPE_VAL,\
		TZ_SYSCALL_PARAM_TYPE_BUF_RW, TZ_SYSCALL_PARAM_TYPE_VAL)

struct tzdev_qc_work {
	struct work_struct work;
	struct completion done;
	int ret;
	union {
		unsigned long val;
		void *ptr;
	} data;
};

struct tzdev_qc_msg {
	uint32_t p0;
	uint32_t p1;
	uint32_t p2;
	uint32_t p3;
	uint32_t p4;
	uint32_t p5;
	uint32_t p6;
	__s64 tv_sec;
	__s32 tv_nsec;
	uint32_t crypto_clk;
} __packed;

struct tzdev_qc_ret_msg {
	uint32_t p0;
	uint32_t p1;
	uint32_t p2;
	uint32_t p3;
	uint32_t timer_remains_ms;
} __packed;

static struct workqueue_struct *tzdev_qc_workq;
static struct hrtimer tzdev_get_event_timer;
static DEFINE_MUTEX(tzdev_init_seq_mutex);
static DEFINE_MUTEX(tzdev_smc_lock);
static atomic_t tz_qc_platform_init_done = ATOMIC_INIT(0);

static struct device_driver tzdev_clk_driver = {
	.name = "tzdev"
};

static struct device device = {
	.driver = &tzdev_clk_driver
};

static struct device *tzdev_clk_device = &device;

static enum hrtimer_restart tzdev_get_event_timer_handler(struct hrtimer *timer)
{
	tz_kthread_pool_cmd_send();

	return HRTIMER_NORESTART;
}

static int tzdev_run_platform_init_sequence(void)
{
	int ret;
	struct workqueue_struct *workq;
	struct workqueue_attrs *attrs;

	workq = create_singlethread_workqueue("tzdev_qc_workq");
	if (!workq) {
		log_error(tzdev_platform, "failed to create worq queue\n");
		return -ENOMEM;
	}

	attrs = alloc_workqueue_attrs(GFP_KERNEL);
	if (!attrs) {
		log_error(tzdev_platform, "failed to allocate work queue attrs\n");
		ret = -ENOMEM;
		goto out_free_workq;
	}

	attrs->nice = 0;
	attrs->no_numa = true;

	/* Bind workqueue to cpu 0 */
	cpumask_clear(attrs->cpumask);
	cpumask_set_cpu(0, attrs->cpumask);

	ret = apply_workqueue_attrs(workq, attrs);
	if (ret) {
		log_error(tzdev_platform, "failed to apply work queue attrs, error=%d\n", ret);
		goto out_free_attrs;
	}

	free_workqueue_attrs(attrs);

	tzdev_qc_workq = workq;

	hrtimer_init(&tzdev_get_event_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	tzdev_get_event_timer.function = tzdev_get_event_timer_handler;

	return 0;

out_free_attrs:
	free_workqueue_attrs(attrs);
out_free_workq:
	destroy_workqueue(workq);

	return ret;
}

static void tzdev_run_platform_fini_sequence(void)
{
	hrtimer_cancel(&tzdev_get_event_timer);
	flush_workqueue(tzdev_qc_workq);
	destroy_workqueue(tzdev_qc_workq);
}

static ssize_t tzdev_run_init_sequence_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	ssize_t ret;

	(void) dev;
	(void) attr;
	(void) buf;

	mutex_lock(&tzdev_init_seq_mutex);

	if (atomic_read(&tz_qc_platform_init_done)) {
		log_error(tzdev_platform, "initialization already done\n");
		ret = -EBUSY;
		goto unlock;
	}

	ret = tzdev_run_platform_init_sequence();
	if (ret) {
		log_error(tzdev_platform, "failed to initialize qc platform, error=%d\n", ret);
		goto unlock;
	}

	ret = tzdev_qc_clock_init(tzdev_clk_device);
	if (ret) {
		log_error(tzdev_platform, "failed to initialize clocks, error=%d\n", ret);
		goto platform_fini;
	}

	ret = tzdev_run_init_sequence();
	if (ret) {
		log_error(tzdev_platform, "failed to initialize tzdev, error=%d\n", ret);
		goto clocks_fini;
	}

	atomic_set(&tz_qc_platform_init_done, 1);

	mutex_unlock(&tzdev_init_seq_mutex);

	return count;

clocks_fini:
	tzdev_qc_clock_fini();
platform_fini:
	tzdev_run_platform_fini_sequence();
unlock:
	mutex_unlock(&tzdev_init_seq_mutex);

	return ret;
}

static DEVICE_ATTR(init_seq, S_IWUSR | S_IWGRP, NULL, tzdev_run_init_sequence_store);

static int tzdev_scm_call(struct tzdev_smc_data *data)
{
	struct tzdev_qc_msg qc_msg = {
		data->args[0], data->args[1], data->args[2], data->args[3], data->args[4],
		data->args[5], data->args[6], 0, 0, tzdev_is_qc_clock_enabled()
	};
	struct tzdev_qc_ret_msg ret_msg = {0, 0, 0, 0, 0};
	struct scm_desc desc = {0};
	struct timespec ts;
	void *scm_buf;
	int ret;

	BUG_ON(raw_smp_processor_id() != 0);

	scm_buf = kzalloc(PAGE_ALIGN(sizeof(qc_msg)), GFP_KERNEL);
	if (!scm_buf)
		return -ENOMEM;

	getnstimeofday(&ts);
	qc_msg.tv_sec = ts.tv_sec;
	qc_msg.tv_nsec = ts.tv_nsec;
	memcpy(scm_buf, &qc_msg, sizeof(qc_msg));
	dmac_flush_range(scm_buf, (unsigned char *)scm_buf + sizeof(qc_msg));

	desc.arginfo = TZ_EXECUTIVE_EXT_ID_PARAM_ID;
	desc.args[0] = virt_to_phys(scm_buf);
	desc.args[1] = sizeof(qc_msg);
	desc.args[2] = virt_to_phys(scm_buf);
	desc.args[3] = sizeof(ret_msg);

	mutex_lock(&tzdev_smc_lock);
	hrtimer_cancel(&tzdev_get_event_timer);

#if !defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_USR_MNG)
	ret = tzdev_qc_clock_enable();
	if (ret)
		goto out;
#endif

	do {
		ret = scm_call2(SCM_TZM_FNID(TZ_SVC_EXECUTIVE_EXT,
				TZ_CMD_ID_EXEC_SMC_EXT), &desc);
	} while (!ret && desc.ret[0] == BF_SMC_INTERRUPTED);

#if !defined(CONFIG_TZDEV_QC_CRYPTO_CLOCKS_USR_MNG)
	tzdev_qc_clock_disable();
#endif

	if (ret) {
		log_error(tzdev_platform, "scm_call() failed: %d\n", ret);
		if (ret == SCM_V2_EBUSY)
			ret = -EBUSY;
		goto out;
	}

	if (desc.ret[0] == BF_SMC_KERNEL_PANIC) {
		static unsigned int panic_printed;

		if (!panic_printed) {
			log_error(tzdev_platform, "Secure kernel panicked\n");
			panic_printed = 1;
		}

#if defined(CONFIG_TZDEV_SWD_PANIC_IS_CRITICAL)
		panic("Secure kernel panicked\n");
#else
		ret = -EIO;
		goto out;
#endif
	}

	dmac_flush_range(scm_buf, (unsigned char *)scm_buf + sizeof(ret_msg));
	memcpy(&ret_msg, scm_buf, sizeof(ret_msg));

	if (ret_msg.timer_remains_ms) {
		unsigned long secs;
		unsigned long nsecs;
		secs = ret_msg.timer_remains_ms / MSEC_PER_SEC;
		nsecs = (ret_msg.timer_remains_ms % MSEC_PER_SEC) * NSEC_PER_MSEC;

		hrtimer_start(&tzdev_get_event_timer,
				ktime_set(secs, nsecs), HRTIMER_MODE_REL);
	}

out:
	mutex_unlock(&tzdev_smc_lock);

	kfree(scm_buf);

	data->args[0] = ret_msg.p0;
	data->args[1] = ret_msg.p1;
	data->args[2] = ret_msg.p2;
	data->args[3] = ret_msg.p3;

	return ret;
}

static void tzdev_smc_call_routed(struct work_struct *work)
{
	struct tzdev_qc_work *qc_work = container_of(work, struct tzdev_qc_work, work);

	qc_work->ret = tzdev_scm_call(qc_work->data.ptr);

	complete(&qc_work->done);
}

int tzdev_platform_smc_call(struct tzdev_smc_data *data)
{
	struct tzdev_qc_work qc_work;

	/* If current smc request is schedulable one then it's guaranteed
	 * that request done via kernel worker thread pinned to cpu 0
	 * and such request could be invoked directly.
	 * Otherwise it must be routed to cpu 0.
	 */
	if (likely(data->args[0] == TZDEV_SMC_SCHEDULE))
		return tzdev_scm_call(data);

	INIT_WORK_ONSTACK(&qc_work.work, tzdev_smc_call_routed);
	init_completion(&qc_work.done);
	qc_work.data.ptr = data;

	BUG_ON(!queue_work_on(0, tzdev_qc_workq, &qc_work.work));

	wait_for_completion(&qc_work.done);

	return qc_work.ret;
}

static struct tz_cdev tzdev_qc_cdev = {
	.name = "tzdev_qc_init",
	.owner = THIS_MODULE,
};

static int tzdev_probe(struct platform_device *pdev)
{
	int ret;

	ret = tz_cdev_register(&tzdev_qc_cdev);
	if (ret) {
		log_error(tzdev_platform, "failed to register qc device, error=%d\n", ret);
		return ret;
	}

	tzdev_clk_device->of_node = pdev->dev.of_node;

	ret = device_create_file(tzdev_qc_cdev.device, &dev_attr_init_seq);
	if (ret) {
		log_error(tzdev_platform, "failed to create device file, error=%d\n", ret);
		goto out_free_init_cdev;
	}
	log_info(tzdev_platform, "tzdev initialization done.\n");

	return 0;

out_free_init_cdev:
	tz_cdev_unregister(&tzdev_qc_cdev);

	return ret;
}

static void tzdev_shutdown(struct platform_device *pdev)
{
	(void) pdev;

	if (!atomic_cmpxchg(&tz_qc_platform_init_done, 1, 0)) {
		log_info(tzdev_platform, "tzdev not initialized.\n");
		return;
	}

	tzdev_run_fini_sequence();
	tzdev_run_platform_fini_sequence();
	tzdev_qc_clock_fini();
	device_remove_file(tzdev_qc_cdev.device, &dev_attr_init_seq);
	tz_cdev_unregister(&tzdev_qc_cdev);

	log_info(tzdev_platform, "tzdev finalization done.\n");
}

static struct of_device_id tzdev_match[] = {
	{
		.compatible = "qcom,tzd",
	},
	{}
};

struct platform_driver tzdev_pdrv = {
	.probe = tzdev_probe,
	.shutdown = tzdev_shutdown,

	.driver = {
		.name = "tzdev",
		.owner = THIS_MODULE,
		.of_match_table = tzdev_match,
	},
};

int tzdev_platform_register(void)
{
	return platform_driver_register(&tzdev_pdrv);
}

void tzdev_platform_unregister(void)
{
	platform_driver_unregister(&tzdev_pdrv);
}

void *tzdev_platform_open(void)
{
	struct tzdev_qc_clock_data *clock_data;

	clock_data = kzalloc(sizeof(struct tzdev_qc_clock_data), GFP_KERNEL);
	if (!clock_data) {
		log_error(tzdev_platform, "Failed to allocate private data\n");
		return ERR_PTR(-ENOMEM);
	}

	mutex_init(&clock_data->mutex);

	return clock_data;
}

void tzdev_platform_release(void *data)
{
	struct tzdev_qc_clock_data *clock_data = data;

	tzdev_qc_set_clk(clock_data, TZIO_CRYPTO_CLOCK_OFF);
	mutex_destroy(&clock_data->mutex);
	kfree(clock_data);
}

long tzdev_platform_ioctl(void *data, unsigned int cmd, unsigned long arg)
{
	unsigned int state = arg;
	struct tzdev_qc_clock_data *clock_data = data;

	switch (cmd) {
	case TZIO_CRYPTO_CLOCK_CONTROL:
		return tzdev_qc_set_clk(clock_data, state);
	default:
		return -ENOTTY;
	}
}
