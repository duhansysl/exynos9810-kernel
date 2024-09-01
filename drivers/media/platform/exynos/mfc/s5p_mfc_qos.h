/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_qos.h
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __S5P_MFC_QOS_H
#define __S5P_MFC_QOS_H __FILE__

#include "s5p_mfc_common.h"

#define MFC_MAX_FPS		(480000)
#define DEC_DEFAULT_FPS		(240000)
#define ENC_DEFAULT_FPS		(240000)

#define MB_COUNT_PER_UHD_FRAME	32400
#define MAX_FPS_PER_UHD_FRAME	120
#define MIN_BW_PER_SEC		1

#define MFC_DRV_TIME		500

#define MFC_QOS_WEIGHT_3PLANE		80
#define MFC_QOS_WEIGHT_OTHER_CODEC	25
#define MFC_QOS_WEIGHT_10BIT		75
#define MFC_QOS_WEIGHT_10BIT_SUPER64_B	60
#define MFC_QOS_WEIGHT_422_10INTRA	70
#define MFC_QOS_WEIGHT_BFRAME		50
#define MFC_QOS_WEIGHT_NUM_OF_REF	50
#define MFC_QOS_WEIGHT_NUM_OF_TILE	75

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
void s5p_mfc_qos_on(struct s5p_mfc_ctx *ctx);
void s5p_mfc_qos_off(struct s5p_mfc_ctx *ctx);
#else
#define s5p_mfc_qos_on(ctx)	do {} while (0)
#define s5p_mfc_qos_off(ctx)	do {} while (0)
#endif

void mfc_qos_idle_worker(struct work_struct *work);
void s5p_mfc_qos_update_framerate(struct s5p_mfc_ctx *ctx, int idle_trigger_only);
void s5p_mfc_qos_update_last_framerate(struct s5p_mfc_ctx *ctx, u64 timestamp);

static inline void s5p_mfc_qos_reset_framerate(struct s5p_mfc_ctx *ctx)
{
	if (ctx->type == MFCINST_DECODER)
		ctx->framerate = DEC_DEFAULT_FPS;
	else if (ctx->type == MFCINST_ENCODER)
		ctx->framerate = ENC_DEFAULT_FPS;
}

static inline void s5p_mfc_qos_reset_last_framerate(struct s5p_mfc_ctx *ctx)
{
	ctx->last_framerate = 0;
}

static inline void s5p_mfc_qos_set_framerate(struct s5p_mfc_ctx *ctx, int rate)
{
	ctx->framerate = rate;
}

static inline int s5p_mfc_qos_get_framerate(struct s5p_mfc_ctx *ctx)
{
	return ctx->framerate;
}

#endif /* __S5P_MFC_QOS_H */
