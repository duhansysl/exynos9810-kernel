/*
 * Copyright (C) 2012-2019 Samsung Electronics, Inc.
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

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>

#include "tz_common.h"
#include "core/deploy_tzar.h"
#include "core/iwsock.h"
#include "core/log.h"
#include "core/mem.h"
#include "teec/iw_messages.h"

#define MAX_CONNECTION_ATTEMPTS 20
#define CONNECTION_ATTEMPT_TIMEOUT 100

__asm__ (
  ".section .init.data,\"aw\"\n"
  "tzdev_tzar_begin:\n"
  ".incbin \"" KBUILD_SRC "/drivers/misc/tzdev/startup.tzar\"\n"
  "tzdev_tzar_end:\n"
  ".previous\n"
);
extern char tzdev_tzar_begin[], tzdev_tzar_end[];

enum iw_userboot_cmd_type {
	CMD_USERBOOT_INVALID_ID,
	CMD_USERBOOT_UPLOAD_CONTAINER,
	CMD_USERBOOT_REPLY,
	CMD_USERBOOT_REPLY_UPLOAD_CONTAINER,
	CMD_USERBOOT_MAX
};

struct cmd_userboot_upload_container {
	struct cmd_request base;
	struct shared_buffer_description buf_desc;
} IW_STRUCTURE;

struct cmd_userboot_reply_upload_container {
	struct cmd_reply base;
} IW_STRUCTURE;

static __ref int tz_deploy_handler(void *arg)
{
	int ret;
	size_t size;
	struct sock_desc *sd;
	struct cmd_userboot_upload_container command;
	struct cmd_userboot_reply_upload_container reply;
	void *tzar;
	unsigned int attempt;
	(void)arg;

	size = tzdev_tzar_end - tzdev_tzar_begin;
	log_debug(tzdev_deploy_tzar, "tzar size = %zu\n", size);

	tzar = vmalloc(size);
	if (!tzar) {
		log_error(tzdev_deploy_tzar, "Failed to allocate memory for tzar\n");
		return -ENOMEM;
	}

	memcpy(tzar, tzdev_tzar_begin, size);

	size = DIV_ROUND_UP(size, PAGE_SIZE) * PAGE_SIZE;
	ret = tzdev_mem_register(tzar, size, 0, NULL, NULL);
	if (ret < 0) {
		log_error(tzdev_deploy_tzar, "Failed to register shared memory, error=%d\n", ret);
		goto out_tzar;
	}

	command.base.cmd = CMD_USERBOOT_UPLOAD_CONTAINER;
	command.buf_desc.id = ret;
	command.buf_desc.size = size;

	sd = tz_iwsock_socket(1, TZ_NON_INTERRUPTIBLE);
	if (IS_ERR(sd)) {
		ret = PTR_ERR(sd);
		log_error(tzdev_deploy_tzar, "Failed to create IW socket, error=%d\n", ret);
		goto out_mem;
	}

	for (attempt = 0; attempt < MAX_CONNECTION_ATTEMPTS; attempt++) {
		ret = tz_iwsock_connect(sd, USERBOOT_SOCK, 0);
		if (!ret)
			break;

		log_error(tzdev_deploy_tzar, "Failed to connect to userboot socket, "
			"error = %d, retrying...\n",
			ret);

		msleep(CONNECTION_ATTEMPT_TIMEOUT);
	}
	if (ret < 0) {
		log_error(tzdev_deploy_tzar, "Failed to connect SWd, error=%d\n", ret);
		goto out_sock;
	}

	ret = tz_iwsock_write(sd, &command, sizeof(command), 0);
	if (ret != sizeof(command)) {
		log_error(tzdev_deploy_tzar, "Failed to write socket, error=%d\n", ret);
		goto out_sock;
	}

	ret = tz_iwsock_read(sd, &reply, sizeof(reply), 0);
	if (ret < 0) {
		log_error(tzdev_deploy_tzar, "Failed to read socket, error=%d\n", ret);
		goto out_sock;
	}

	ret = 0;
	log_info(tzdev_deploy_tzar, "Startup tzar deployment done.\n");

out_sock:
	tz_iwsock_release(sd);
out_mem:
	tzdev_mem_release(command.buf_desc.id);
out_tzar:
	vfree(tzar);

	return ret;
}

int tzdev_deploy_tzar(void)
{
	struct task_struct *t;

	t = kthread_run(tz_deploy_handler, NULL, "tzdev_deploy_tzar");
	if (IS_ERR(t)) {
		log_error(tzdev_deploy_tzar, "Failed to create kernel thread, error=%ld\n", PTR_ERR(t));
		return PTR_ERR(t);
	}
	return 0;
}

static int __init tz_deploy_tzar_init(void)
{
	tzdev_set_nwd_sysconf_flag(SYSCONF_NWD_TZDEV_DEPLOY_TZAR);

	return 0;
}

early_initcall(tz_deploy_tzar_init);
