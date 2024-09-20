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

#include <linux/anon_inodes.h>
#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/file.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include <tzdev/tee_client_api.h>

#include "misc.h"
#include "core/iwsock.h"
#include "core/kthread_pool.h"
#include "core/log.h"
#include "core/sysdep.h"

#define MAX_CONNECTION_ATTEMPTS 20
#define CONNECTION_ATTEMPT_TIMEOUT 100

int tzdev_teec_connect(struct sock_desc *socket, char *name,
				uint32_t *result, uint32_t *origin)
{
	int ret;
	unsigned int attempt;

	*origin = TEEC_ORIGIN_COMMS;
	*result = TEEC_SUCCESS;

	for (attempt = 0; attempt < MAX_CONNECTION_ATTEMPTS; attempt++) {
		ret = tz_iwsock_connect(socket, name, 0);
		if (ret == 0)
			return ret;

		log_info(tzdev_teec, "Failed to connect to %s socket, error = %d, retrying...\n",
				name, ret);

		msleep(CONNECTION_ATTEMPT_TIMEOUT);
	}

	log_error(tzdev_teec, "Failed to connect to %s socket after %d retries\n",
			name, MAX_CONNECTION_ATTEMPTS);

	*result = tzdev_teec_error_to_tee_error(ret);

	return ret;
}

void tzdev_teec_disconnect(struct sock_desc *socket)
{
	tz_iwsock_release(socket);
}

int tzdev_teec_send(struct sock_desc *socket, void *data, uint32_t size, int flags,
		uint32_t *result, uint32_t *origin)
{
	int ret;

	*origin = TEEC_ORIGIN_COMMS;
	*result = TEEC_SUCCESS;

	ret = tz_iwsock_write(socket, data, size, flags);
	if (ret >= 0 && ret != size)
		ret = -EMSGSIZE;
	if (ret < 0)
		*result = tzdev_teec_error_to_tee_error(ret);

	return ret;
}

int tzdev_teec_recv(struct sock_desc *socket, void *data, uint32_t size, int flags,
		uint32_t *result, uint32_t *origin)
{
	int ret;

	*origin = TEEC_ORIGIN_COMMS;
	*result = TEEC_SUCCESS;

	ret = tz_iwsock_read(socket, data, size, flags);
	if (ret > 0 && ret != size)
		ret = -EMSGSIZE;
	if (!ret)
		ret = -ECONNRESET;
	if (ret < 0)
		*result = tzdev_teec_error_to_tee_error(ret);

	return ret;
}

int tzdev_teec_send_then_recv(struct sock_desc *socket,
		void *send_data, uint32_t send_size, int send_flags,
		void *recv_data, uint32_t recv_size, int recv_flags,
		uint32_t *result, uint32_t *origin)
{
	int ret;

	ret = tzdev_teec_send(socket, send_data, send_size, send_flags,
			result, origin);
	if (ret < 0)
		return ret;

	ret = tzdev_teec_recv(socket, recv_data, recv_size, recv_flags,
			result, origin);
	if (ret < 0)
		return ret;

	return 0;
}

int tzdev_teec_check_reply(struct cmd_reply *reply, uint32_t cmd, uint32_t serial,
		uint32_t *result, uint32_t *origin)
{
	int ret;

	*origin = TEEC_ORIGIN_COMMS;
	*result = TEEC_SUCCESS;

	if (reply->base.cmd != cmd) {
		log_error(tzdev_teec, "Received wrong reply = %u, expected = %u\n",
				reply->base.cmd, cmd);
		ret = -ECOMM;
		goto out;
	}

	if (reply->base.serial != serial) {
		log_error(tzdev_teec, "Received wrong reply serial = %u, expected = %u\n",
				reply->base.serial, serial);
		ret = -ECOMM;
		goto out;
	}

	return 0;

out:
	*result = tzdev_teec_error_to_tee_error(ret);

	return ret;
}

uint32_t tzdev_teec_error_to_tee_error(int error)
{
	switch (error) {
	case -EINVAL:
		return TEEC_ERROR_BAD_PARAMETERS;
	case -ENOMEM:
		return TEEC_ERROR_OUT_OF_MEMORY;
	case -EPIPE:
	case -ENOTCONN:
	case -ECONNREFUSED:
	case -EMSGSIZE:
	case -ECOMM:
		return TEEC_ERROR_COMMUNICATION;
	case -ECONNRESET:
		return TEEC_ERROR_TARGET_DEAD;
	case -ECANCELED:
		return TEEC_ERROR_CANCEL;
	default:
		log_error(tzdev_teec, "Unknown error code = %d\n", error);
		return TEEC_ERROR_GENERIC;
	}
}

void tzdev_teec_fixup_origin(uint32_t result, uint32_t *origin)
{
	if (result == TEEC_ERROR_TARGET_DEAD)
		*origin = TEEC_ORIGIN_TEE;
}
