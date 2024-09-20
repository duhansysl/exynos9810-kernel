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

#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <tzdev/tee_client_api.h>

#include "iw_messages.h"
#include "misc.h"
#include "types.h"
#include "core/iwsock.h"
#include "core/log.h"

#define CONTEXT_NAME_SAMSUNG	"TEE Samsung"

static uint32_t tzdev_teec_context_init(TEEC_Context *context)
{
	struct tzdev_teec_context *ctx;
	uint32_t result;

	log_debug(tzdev_teec, "Enter, context = %pK\n", context);

	ctx = kmalloc(sizeof(struct tzdev_teec_context), GFP_KERNEL);
	if (!ctx) {
		log_error(tzdev_teec, "Failed to allocate context struct\n");
		result = TEEC_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	mutex_init(&ctx->mutex);
	ctx->socket = NULL;
	ctx->serial = 0;
	ctx->id = 0;

	context->imp = ctx;

	result = TEEC_SUCCESS;

out:
	log_debug(tzdev_teec, "Exit, context = %pK result = %x\n", context, result);

	return result;
}

static void tzdev_teec_context_fini(TEEC_Context *context)
{
	struct tzdev_teec_context *ctx = context->imp;

	log_debug(tzdev_teec, "Enter, context = %pK\n", context);

	context->imp = NULL;

	mutex_destroy(&ctx->mutex);

	kfree(ctx);

	log_debug(tzdev_teec, "Exit, context = %pK\n", context);
}

static uint32_t tzdev_teec_initialize_context(TEEC_Context *context, uint32_t *origin)
{
	struct tzdev_teec_context *ctx = context->imp;
	struct cmd_initialize_context cmd;
	struct cmd_reply_initialize_context ack;
	uint32_t result;
	int ret;

	log_debug(tzdev_teec, "Enter, context = %pK\n", context);

	*origin = TEEC_ORIGIN_API;

	cmd.base.cmd = CMD_INITIALIZE_CONTEXT;
	cmd.base.serial = ctx->serial;

	ret = tzdev_teec_send_then_recv(ctx->socket,
			&cmd, sizeof(cmd), 0x0,
			&ack, sizeof(ack), 0x0,
			&result, origin);
	if (ret < 0) {
		log_error(tzdev_teec, "Failed to xmit initialize context, ret = %d\n", ret);
		goto out;
	}

	ret = tzdev_teec_check_reply(&ack.base, CMD_REPLY_INITIALIZE_CONTEXT,
			ctx->serial, &result, origin);
	if (ret) {
		log_error(tzdev_teec, "Failed to check initialize context reply, ret = %d\n", ret);
		goto out;
	}

	result = ack.base.result;
	*origin = ack.base.origin;

	ctx->id = ack.ctx_id;

out:
	ctx->serial++;

	tzdev_teec_fixup_origin(result, origin);

	log_debug(tzdev_teec, "Exit, context = %pK\n", context);

	return result;
}

static uint32_t tzdev_teec_finalize_context(TEEC_Context *context, uint32_t *origin)
{
	struct tzdev_teec_context *ctx = context->imp;
	struct cmd_finalize_context cmd;
	struct cmd_reply_finalize_context ack;
	uint32_t result;
	int ret;

	log_debug(tzdev_teec, "Enter, context = %pK\n", context);

	*origin = TEEC_ORIGIN_API;

	cmd.base.cmd = CMD_FINALIZE_CONTEXT;
	cmd.base.serial = ctx->serial;

	ret = tzdev_teec_send_then_recv(ctx->socket,
			&cmd, sizeof(cmd), 0x0,
			&ack, sizeof(ack), 0x0,
			&result, origin);
	if (ret < 0) {
		log_error(tzdev_teec, "Failed to xmit finalize context, ret = %d\n", ret);
		goto out;
	}

	ret = tzdev_teec_check_reply(&ack.base, CMD_REPLY_FINALIZE_CONTEXT,
			ctx->serial, &result, origin);
	if (ret) {
		log_error(tzdev_teec, "Failed to check finalize context reply, ret = %d\n", ret);
		goto out;
	}

	result = ack.base.result;
	*origin = ack.base.origin;

out:
	ctx->serial++;

	tzdev_teec_fixup_origin(result, origin);

	log_debug(tzdev_teec, "Exit, context = %pK\n", context);

	return result;
}

static uint32_t tzdev_teec_context_check_args(const char *name, TEEC_Context *context)
{
	if (!context) {
		log_error(tzdev_teec, "Null context passed\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (name && strncmp(name, CONTEXT_NAME_SAMSUNG, sizeof(CONTEXT_NAME_SAMSUNG))) {
		log_error(tzdev_teec, "Unsupported TEE name = \"%s\"\n", name);
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	return TEEC_SUCCESS;
}

TEEC_Result TEEC_InitializeContext(const char *name, TEEC_Context *context)
{
	struct tzdev_teec_context *ctx;
	struct sock_desc *sd;
	uint32_t result;
	uint32_t origin;
	int ret;

	log_debug(tzdev_teec, "Enter, context = %pK\n", context);

	origin = TEEC_ORIGIN_API;

	result = tzdev_teec_context_check_args(name, context);
	if (result != TEEC_SUCCESS)
		goto out;

	result = tzdev_teec_context_init(context);
	if (result != TEEC_SUCCESS)
		goto out;

	ctx = context->imp;

	sd = tz_iwsock_socket(1, TZ_NON_INTERRUPTIBLE);
	if (IS_ERR(sd))
		goto out_fini_context;

	ret = tzdev_teec_connect(sd, ROOT_TASK_SOCK, &result, &origin);
	if (ret < 0) {
		log_error(tzdev_teec, "Failed to connect to root task, context = %pK ret = %d\n",
				context, ret);
		goto out_disconnect;
	}

	ctx->socket = sd;

	result = tzdev_teec_initialize_context(context, &origin);
	if (result != TEEC_SUCCESS) {
		log_error(tzdev_teec, "Failed to initialize context, context = %pK\n", context);
		goto out_disconnect;
	}

	log_debug(tzdev_teec, "Success, context = %pK socket = %u id = %u\n",
			context, ctx->socket->id, ctx->id);
	goto out;

out_disconnect:
	tzdev_teec_disconnect(sd);
out_fini_context:
	tzdev_teec_context_fini(context);
out:
	log_debug(tzdev_teec, "Exit, context = %pK result = %x origin = %u\n", context, result, origin);

	return result;
}

void TEEC_FinalizeContext(TEEC_Context *context)
{
	struct tzdev_teec_context *ctx;
	uint32_t result;
	uint32_t origin;

	log_debug(tzdev_teec, "Enter, context = %pK\n", context);

	origin = TEEC_ORIGIN_API;

	if (!context || !context->imp) {
		log_error(tzdev_teec, "Null context passed\n");
		result = TEEC_ERROR_BAD_PARAMETERS;
		goto out;
	}

	ctx = context->imp;

	result = tzdev_teec_finalize_context(context, &origin);
	if (result != TEEC_SUCCESS) {
		log_error(tzdev_teec, "Failed to finalize context, context = %pK socket = %u id = %u\n",
				context, ctx->socket->id, ctx->id);
		goto out_fini;
	}

	log_debug(tzdev_teec, "Success, context = %pK socket = %u id = %u\n",
			context, ctx->socket->id, ctx->id);

out_fini:
	tzdev_teec_disconnect(ctx->socket);
	tzdev_teec_context_fini(context);
out:
	log_debug(tzdev_teec, "Exit, context = %pK result = %x origin = %u\n", context, result, origin);
}
