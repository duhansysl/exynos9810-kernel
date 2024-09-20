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

#include <linux/atomic.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include <tzdev/tee_client_api.h>

#include "teec/iw_messages.h"
#include "teec/misc.h"
#include "teec/pmf_defs.h"

#include "core/cdev.h"
#include "core/iwsock.h"
#include "core/log.h"
#include "core/notifier.h"
#include "debug/pmf.h"

static atomic_t tz_pmf_init_done = ATOMIC_INIT(0);
static struct sock_desc *sd;
static uint32_t serial;
static pid_t tz_pmf_pid;
static struct pmf_data *pmf_sample = NULL;

static uint32_t tzdev_teec_pmf_init(struct tzio_pmf_init *s, uint32_t *origin)
{
	struct cmd_init_pmf cmd;
	struct cmd_reply_init_pmf ack;
	uint32_t result;
	int ret;

	*origin = TEEC_ORIGIN_API;

	serial = current->pid;
	cmd.base.cmd = CMD_INIT_PMF;
	cmd.base.serial = serial;
	cmd.pmf_shmem_id = s->id;

	ret = tzdev_teec_send_then_recv(sd,
			&cmd, sizeof(cmd), 0x0,
			&ack, sizeof(ack), 0x0,
			&result, origin);
	if (ret < 0) {
		log_error(tzdev_pmf, "Failed to pass pmf init command, ret = %d\n", ret);
		goto out;
	}

	ret = tzdev_teec_check_reply(&ack.base, CMD_REPLY_INIT_PMF,
			serial, &result, origin);
	if (ret) {
		log_error(tzdev_pmf, "Failed to check pmf init command, ret = %d\n", ret);
		goto out;
	}

	result = ack.base.result;
	*origin = ack.base.origin;

out:
	tzdev_teec_fixup_origin(result, origin);

	return result;
}

static uint32_t tzdev_teec_pmf_fini(uint32_t *origin)
{
	struct cmd_init_pmf cmd;
	struct cmd_reply_init_pmf ack;
	uint32_t result;
	int ret;

	*origin = TEEC_ORIGIN_API;

	cmd.base.cmd = CMD_FINI_PMF;
	cmd.base.serial = current->pid;

	ret = tzdev_teec_send_then_recv(sd,
			&cmd, sizeof(cmd), 0x0,
			&ack, sizeof(ack), 0x0,
			&result, origin);
	if (ret < 0) {
		log_error(tzdev_pmf, "Failed to pass pmf fini command, ret = %d\n", ret);
		goto out;
	}

	ret = tzdev_teec_check_reply(&ack.base, CMD_REPLY_FINI_PMF,
			serial, &result, origin);
	if (ret) {
		log_error(tzdev_pmf, "Failed to check pmf fini command, ret = %d\n", ret);
		goto out;
	}

	result = ack.base.result;
	*origin = ack.base.origin;

out:
	serial = 0;
	tzdev_teec_fixup_origin(result, origin);

	return result;
}

static int tzdev_pmf_get_ticks(struct file *filp, unsigned long arg)
{
	enum pmf_id __user id = (enum pmf_id)arg;
	(void) filp;

	tzdev_pmf_stamp(id);

	return 0;
}

static int tzdev_pmf_init(struct file *filp, unsigned long arg)
{
	int ret;
	uint32_t result;
	uint32_t origin;
	struct tzio_pmf_init __user *argp = (struct tzio_pmf_init __user *)arg;
	struct tzio_pmf_init s;

	(void) filp;

	if (copy_from_user(&s, argp, sizeof(struct tzio_pmf_init)))
		return -EFAULT;

	sd = tz_iwsock_socket(1, TZ_NON_INTERRUPTIBLE);
	if (IS_ERR(sd))
		goto out;

	ret = tzdev_teec_connect(sd, ROOT_TASK_SOCK, &result, &origin);
	if (ret < 0) {
		log_error(tzdev_pmf, "Failed to connect to root_task socket, ret = %d\n", ret);
		goto out_close_conn;
	}

	result = tzdev_teec_pmf_init(&s, &origin);
	if (result != TEEC_SUCCESS) {
		log_error(tzdev_pmf, "Failed to initialize pmf, result = %#x\n", result);
		goto out_close_conn;
	}
	tz_pmf_pid = current->pid;
	pmf_sample = (struct pmf_data *)s.ptr;

	return result;

out_close_conn:
	tzdev_teec_disconnect(sd);
	sd = NULL;

out:
	return result;
}

static int tzdev_pmf_fini(struct file *filp, unsigned long arg)
{
	uint32_t result;
	uint32_t origin;

	(void) filp;
	(void) arg;

	if (sd == NULL) {
		result = TEEC_ERROR_BAD_STATE;
		goto out;
	}
	result = tzdev_teec_pmf_fini(&origin);
	if (result != TEEC_SUCCESS)
		log_error(tzdev_pmf, "Failed to fini pmf, result = %#x", result);

	tzdev_teec_disconnect(sd);

out:
	tz_pmf_pid = 0;
	pmf_sample = NULL;
	return result;
}

static long tzdev_pmf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {
	case TZIO_PMF_INIT:
		return tzdev_pmf_init(filp, arg);
	case TZIO_PMF_FINI:
		return tzdev_pmf_fini(filp, arg);
	case TZIO_PMF_GET_TICKS:
		return tzdev_pmf_get_ticks(filp, arg);
	default:
		return -ENOTTY;
	}
}

static const struct file_operations tzdev_pmf_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tzdev_pmf_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tzdev_pmf_ioctl,
#endif /* CONFIG_COMPAT */
};

static struct tz_cdev tz_pmf_cdev = {
	.name = "tz_pmf",
	.fops = &tzdev_pmf_fops,
	.owner = THIS_MODULE,
};

void tzdev_pmf_stamp(enum pmf_id id)
{
	uint64_t ticks = read_ticks();

	if (current->pid != tz_pmf_pid)
		return;

	if (copy_to_user(&pmf_sample->samples[id], &ticks, sizeof(uint64_t)))
		log_error(tzdev_pmf, "Unable to copy the ticks.\n");
}

static int tz_pmf_init_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	int rc;

	(void)cb;
	(void)code;
	(void)unused;

	rc = tz_cdev_register(&tz_pmf_cdev);
	if (rc) {
		log_info(tzdev_pmf, "Failed to create pmf device, error=%d\n", rc);
		return NOTIFY_DONE;
	}

	atomic_set(&tz_pmf_init_done, 1);

	log_info(tzdev_pmf, "PMF initialization done.");

	return NOTIFY_DONE;
}

static int tz_pmf_fini_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	(void)cb;
	(void)code;
	(void)unused;

	if (!atomic_cmpxchg(&tz_pmf_init_done, 1, 0)) {
		log_info(tzdev_pmf, "PMF not initialized.");
		return NOTIFY_DONE;
	}

	tz_cdev_unregister(&tz_pmf_cdev);

	log_info(tzdev_pmf, "PMF finalization done.");

	return NOTIFY_DONE;
}

static struct notifier_block tz_pmf_init_notifier = {
	.notifier_call = tz_pmf_init_call,
};

static struct notifier_block tz_pmf_fini_notifier = {
	.notifier_call = tz_pmf_fini_call,
};

static int __init tz_pmf_init(void)
{
	int rc;

	rc = tzdev_blocking_notifier_register(TZDEV_INIT_NOTIFIER, &tz_pmf_init_notifier);
	if (rc) {
		log_error(tzdev_pmf, "Failed to register init notifier, error=%d\n", rc);
		return rc;
	}
	rc = tzdev_blocking_notifier_register(TZDEV_FINI_NOTIFIER, &tz_pmf_fini_notifier);
	if (rc) {
		tzdev_blocking_notifier_unregister(TZDEV_INIT_NOTIFIER, &tz_pmf_init_notifier);
		log_error(tzdev_pmf, "Failed to register fini notifier, error=%d\n", rc);
		return rc;
	}
	log_info(tzdev_pmf, "PMF callbacks registration done\n");

	return 0;
}

early_initcall(tz_pmf_init);
