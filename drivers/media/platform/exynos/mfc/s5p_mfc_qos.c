/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_qos.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/err.h>
#include <soc/samsung/bts.h>

#include "s5p_mfc_qos.h"
#include "s5p_mfc_utils.h"

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
enum {
	MFC_QOS_ADD,
	MFC_QOS_UPDATE,
	MFC_QOS_REMOVE,
	MFC_QOS_BW,
};

static void mfc_qos_operate(struct s5p_mfc_dev *dev, int opr_type, int idx)
{
	struct s5p_mfc_platdata *pdata = dev->pdata;
	struct s5p_mfc_qos *qos_table = pdata->qos_table;

	switch (opr_type) {
	case MFC_QOS_ADD:
		MFC_TRACE_DEV("++ QOS add[%d] (int:%d, mif:%d)\n",
			idx, qos_table[idx].freq_int, qos_table[idx].freq_mif);

		pm_qos_add_request(&dev->qos_req_int,
				PM_QOS_DEVICE_THROUGHPUT,
				qos_table[idx].freq_int);
		pm_qos_add_request(&dev->qos_req_mif,
				PM_QOS_BUS_THROUGHPUT,
				qos_table[idx].freq_mif);

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		pm_qos_add_request(&dev->qos_req_cluster1,
				PM_QOS_CLUSTER1_FREQ_MIN,
				qos_table[idx].freq_cpu);
		pm_qos_add_request(&dev->qos_req_cluster0,
				PM_QOS_CLUSTER0_FREQ_MIN,
				qos_table[idx].freq_kfc);
#endif

		bts_update_scen(BS_MFC_UHD_ENC60, qos_table[idx].mo_uhd_enc60_value);
		bts_update_scen(BS_MFC_UHD_10BIT, qos_table[idx].mo_10bit_value);
		bts_update_scen(BS_MFC_UHD, qos_table[idx].mo_value);

		atomic_set(&dev->qos_req_cur, idx + 1);
		MFC_TRACE_DEV("-- QOS add[%d] (int:%d, mif:%d, mo:%d, mo_10bit:%d, mo_uhd_enc:%d)\n",
				idx, qos_table[idx].freq_int, qos_table[idx].freq_mif,
				qos_table[idx].mo_value, qos_table[idx].mo_10bit_value,
				qos_table[idx].mo_uhd_enc60_value);
		mfc_debug(2, "QOS add[%d] (int:%d, mif:%d, mo:%d, mo_10bit:%d, mo_uhd_enc:%d)\n",
				idx, qos_table[idx].freq_int, qos_table[idx].freq_mif,
				qos_table[idx].mo_value, qos_table[idx].mo_10bit_value,
				qos_table[idx].mo_uhd_enc60_value);
		break;
	case MFC_QOS_UPDATE:
		MFC_TRACE_DEV("++ QOS update[%d] (int:%d, mif:%d)\n",
				idx, qos_table[idx].freq_int, qos_table[idx].freq_mif);

		pm_qos_update_request(&dev->qos_req_int,
				qos_table[idx].freq_int);
		pm_qos_update_request(&dev->qos_req_mif,
				qos_table[idx].freq_mif);

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		pm_qos_update_request(&dev->qos_req_cluster1,
				qos_table[idx].freq_cpu);
		pm_qos_update_request(&dev->qos_req_cluster0,
				qos_table[idx].freq_kfc);
#endif

		bts_update_scen(BS_MFC_UHD_ENC60, qos_table[idx].mo_uhd_enc60_value);
		bts_update_scen(BS_MFC_UHD_10BIT, qos_table[idx].mo_10bit_value);
		bts_update_scen(BS_MFC_UHD, qos_table[idx].mo_value);

		atomic_set(&dev->qos_req_cur, idx + 1);
		MFC_TRACE_DEV("-- QOS update[%d] (int:%d, mif:%d, mo:%d, mo_10bit:%d, mo_uhd_enc:%d)\n",
				idx, qos_table[idx].freq_int, qos_table[idx].freq_mif,
				qos_table[idx].mo_value, qos_table[idx].mo_10bit_value,
				qos_table[idx].mo_uhd_enc60_value);
		mfc_debug(2, "QOS update[%d] (int:%d, mif:%d, mo:%d, mo_10bit:%d, mo_uhd_enc:%d)\n",
				idx, qos_table[idx].freq_int, qos_table[idx].freq_mif,
				qos_table[idx].mo_value, qos_table[idx].mo_10bit_value,
				qos_table[idx].mo_uhd_enc60_value);
		break;
	case MFC_QOS_REMOVE:
		MFC_TRACE_DEV("++ QOS remove\n");

		pm_qos_remove_request(&dev->qos_req_int);
		pm_qos_remove_request(&dev->qos_req_mif);

#ifdef CONFIG_ARM_EXYNOS_MP_CPUFREQ
		pm_qos_remove_request(&dev->qos_req_cluster1);
		pm_qos_remove_request(&dev->qos_req_cluster0);
#endif

		bts_update_scen(BS_MFC_UHD_ENC60, 0);
		bts_update_scen(BS_MFC_UHD_10BIT, 0);
		bts_update_scen(BS_MFC_UHD, 0);

		dev->mfc_bw.peak = 0;
		dev->mfc_bw.read = 0;
		dev->mfc_bw.write = 0;
		bts_update_bw(BTS_BW_MFC, dev->mfc_bw);

		atomic_set(&dev->qos_req_cur, 0);
		MFC_TRACE_DEV("-- QOS remove\n");
		mfc_debug(2, "QoS remove\n");
		break;
	case MFC_QOS_BW:
		MFC_TRACE_DEV("++ QOS BW (peak: %d, read: %d, write: %d)\n",
				dev->mfc_bw.peak, dev->mfc_bw.read, dev->mfc_bw.write);

		bts_update_bw(BTS_BW_MFC, dev->mfc_bw);

		MFC_TRACE_DEV("-- QOS BW (peak: %d, read: %d, write: %d)\n",
				dev->mfc_bw.peak, dev->mfc_bw.read, dev->mfc_bw.write);
		mfc_debug(2, "QoS BW, (peak: %d, read: %d, write: %d)\n",
				dev->mfc_bw.peak, dev->mfc_bw.read, dev->mfc_bw.write);
		break;
	default:
		mfc_err_dev("Unknown request for opr [%d]\n", opr_type);
		break;
	}
}

static void mfc_qos_set(struct s5p_mfc_ctx *ctx, struct bts_bw *mfc_bw, int i)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_platdata *pdata = dev->pdata;
	struct s5p_mfc_qos *qos_table = pdata->qos_table;

	mfc_debug(2, "QoS table[%d] covered mb %d ~ %d (int:%d, mif:%d)\n",
			i, qos_table[i].threshold_mb,
			i == pdata->num_qos_steps - 1 ?
			pdata->max_mb : qos_table[i + 1].threshold_mb,
			qos_table[i].freq_int, qos_table[i].freq_mif);

	if (mfc_bw->peak != dev->mfc_bw.peak) {
		dev->mfc_bw.peak = mfc_bw->peak;
		dev->mfc_bw.read = mfc_bw->read;
		dev->mfc_bw.write = mfc_bw->write;
		mfc_qos_operate(dev, MFC_QOS_BW, i);
	}

	if (atomic_read(&dev->qos_req_cur) == 0)
		mfc_qos_operate(dev, MFC_QOS_ADD, i);
	else if (atomic_read(&dev->qos_req_cur) != (i + 1))
		mfc_qos_operate(dev, MFC_QOS_UPDATE, i);
}

static inline unsigned long mfc_qos_get_weighted_mb(struct s5p_mfc_ctx *ctx,
						unsigned long mb)
{
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	struct s5p_mfc_enc_params *p;
	u32 num_planes = ctx->dst_fmt->num_planes;
	int weight = 1000;
	unsigned long weighted_mb;

	switch (ctx->codec_mode) {
	case S5P_FIMV_CODEC_H264_DEC:
	case S5P_FIMV_CODEC_H264_MVC_DEC:
	case S5P_FIMV_CODEC_H264_ENC:
	case S5P_FIMV_CODEC_H264_MVC_ENC:
	case S5P_FIMV_CODEC_VP8_DEC:
	case S5P_FIMV_CODEC_VP8_ENC:
		if (num_planes == 3)
			weight = (weight * 100) / MFC_QOS_WEIGHT_3PLANE;
		break;

	case S5P_FIMV_CODEC_HEVC_DEC:
	case S5P_FIMV_CODEC_HEVC_ENC:
	case S5P_FIMV_CODEC_VP9_DEC:
	case S5P_FIMV_CODEC_VP9_ENC:
	case S5P_FIMV_CODEC_BPG_DEC:
	case S5P_FIMV_CODEC_BPG_ENC:
		if (num_planes == 3) {
			weight = (weight * 100) / MFC_QOS_WEIGHT_3PLANE;
		} else {
			if (ctx->is_10bit) {
				if (dec && dec->super64_bframe) {
					weight = (weight * 100) / MFC_QOS_WEIGHT_10BIT_SUPER64_B;
					mfc_debug(3, "QoS weight: super64 with B frame\n");
				} else {
					weight = (weight * 100) / MFC_QOS_WEIGHT_10BIT;
				}
			} else if (ctx->is_422format) {
				weight = (weight * 100) / MFC_QOS_WEIGHT_422_10INTRA;
			}
		}
		break;

	case S5P_FIMV_CODEC_MPEG4_DEC:
	case S5P_FIMV_CODEC_FIMV1_DEC:
	case S5P_FIMV_CODEC_FIMV2_DEC:
	case S5P_FIMV_CODEC_FIMV3_DEC:
	case S5P_FIMV_CODEC_FIMV4_DEC:
	case S5P_FIMV_CODEC_H263_DEC:
	case S5P_FIMV_CODEC_VC1_RCV_DEC:
	case S5P_FIMV_CODEC_VC1_DEC:
	case S5P_FIMV_CODEC_MPEG2_DEC:
	case S5P_FIMV_CODEC_MPEG4_ENC:
	case S5P_FIMV_CODEC_H263_ENC:
		weight = (weight * 100) / MFC_QOS_WEIGHT_OTHER_CODEC;
		break;

	default:
		mfc_err_ctx("wrong codec_mode (%d), no weight\n", ctx->codec_mode);
	}

	if (enc) {
		p = &enc->params;
		if ((IS_H264_ENC(ctx) || IS_HEVC_ENC(ctx)) && p->num_b_frame) {
			weight = (weight * 100) / MFC_QOS_WEIGHT_BFRAME;
			mfc_debug(3, "QoS weight: B frame encoding\n");
		}
		if ((IS_H264_ENC(ctx) || IS_HEVC_ENC(ctx) || IS_VP8_ENC(ctx) ||
					IS_VP9_ENC(ctx)) && (p->num_refs_for_p >= 2)) {
			weight = (weight * 100) / MFC_QOS_WEIGHT_NUM_OF_REF;
			mfc_debug(3, "QoS weight: num of ref >= 2\n");
		}
	}
	if (dec) {
		if (dec->num_of_tile_over_4) {
			weight = (weight * 100) / MFC_QOS_WEIGHT_NUM_OF_TILE;
			mfc_debug(3, "QoS weight: num of tile >= 4\n");
		}
	}

	weighted_mb = (mb * weight) / 1000;
	mfc_debug(3, "QoS weight: %d.%03d, codec: %d, num planes: %d, "
			"10bit: %d, 422format: %d (mb: %ld)\n",
			weight / 1000, weight % 1000, ctx->codec_mode,
			num_planes, ctx->is_10bit, ctx->is_422format,
			weighted_mb);


	return weighted_mb;
}

static inline unsigned long mfc_qos_get_mb_per_second(struct s5p_mfc_ctx *ctx)
{
	unsigned long mb_width, mb_height, fps, mb;

	mb_width = (ctx->img_width + 15) / 16;
	mb_height = (ctx->img_height + 15) / 16;
	fps = ctx->framerate / 1000;

	mb = mb_width * mb_height * fps;
	mfc_debug(4, "QoS ctx[%d:%s] %d x %d @ %ld fps (mb: %ld)\n",
			ctx->num, ctx->type == MFCINST_ENCODER ? "ENC" : "DEC",
			ctx->img_width, ctx->img_height, fps, mb);

	return mfc_qos_get_weighted_mb(ctx, mb);
}

static struct s5p_mfc_qos_bw mfc_bw_info = {
	/*				  peak   read   write	(KB/UHD frame) */
	.h264_dec_uhd_bw	=	{ 38131, 40206, 24870 },
	.hevc_dec_uhd_bw	=	{ 35055, 33741, 20511 },
	.hevc_dec_uhd_10bit_bw	=	{ 38643, 36428, 25491 },
	.vp8_dec_uhd_bw		=	{ 28693, 30464, 22331 },
	.vp9_dec_uhd_bw		=	{ 21464, 22160, 19747 },
	.mpeg4_dec_uhd_bw	=	{ 31567, 25191, 15961 },
	.h264_enc_uhd_bw	=	{ 62543, 75230, 13080 },
	.hevc_enc_uhd_bw	=	{ 54863, 65417, 11422 },
	.hevc_enc_uhd_10bit_bw	=	{ 68011, 79367, 14688 },
	.vp8_enc_uhd_bw		=	{ 63970, 67281, 22508 },
	.vp9_enc_uhd_bw		=	{ 84443, 71588, 19337 },
	.mpeg4_enc_uhd_bw	=	{ 44633, 55310, 9599  },
};

static void mfc_qos_get_bw_per_second(struct s5p_mfc_ctx *ctx, struct bts_bw *mfc_bw)
{
	struct mfc_qos_bw_data bw_data;
	unsigned long mb_width, mb_height, fps, mb;
	unsigned long peak_bw_per_sec;
	unsigned long read_bw_per_sec;
	unsigned long write_bw_per_sec;
	unsigned long mb_count_per_uhd_frame = MB_COUNT_PER_UHD_FRAME;
	unsigned long max_fps_per_uhd_frame = MAX_FPS_PER_UHD_FRAME;

	mb_width = (ctx->img_width + 15) / 16;
	mb_height = (ctx->img_height + 15) / 16;
	fps = ctx->framerate / 1000;

	mb = mb_width * mb_height * fps;

	switch (ctx->codec_mode) {
	case S5P_FIMV_CODEC_H264_DEC:
	case S5P_FIMV_CODEC_H264_MVC_DEC:
		bw_data = mfc_bw_info.h264_dec_uhd_bw;
		break;
	case S5P_FIMV_CODEC_H264_ENC:
	case S5P_FIMV_CODEC_H264_MVC_ENC:
		bw_data = mfc_bw_info.h264_enc_uhd_bw;
		break;
	case S5P_FIMV_CODEC_HEVC_DEC:
	case S5P_FIMV_CODEC_BPG_DEC:
		if (ctx->is_10bit)
			bw_data = mfc_bw_info.hevc_dec_uhd_10bit_bw;
		else
			bw_data = mfc_bw_info.hevc_dec_uhd_bw;
		break;
	case S5P_FIMV_CODEC_HEVC_ENC:
	case S5P_FIMV_CODEC_BPG_ENC:
		if (ctx->is_10bit)
			bw_data = mfc_bw_info.hevc_enc_uhd_10bit_bw;
		else
			bw_data = mfc_bw_info.hevc_enc_uhd_bw;
		break;
	case S5P_FIMV_CODEC_MPEG4_DEC:
	case S5P_FIMV_CODEC_FIMV1_DEC:
	case S5P_FIMV_CODEC_FIMV2_DEC:
	case S5P_FIMV_CODEC_FIMV3_DEC:
	case S5P_FIMV_CODEC_FIMV4_DEC:
	case S5P_FIMV_CODEC_H263_DEC:
	case S5P_FIMV_CODEC_VC1_RCV_DEC:
	case S5P_FIMV_CODEC_VC1_DEC:
	case S5P_FIMV_CODEC_MPEG2_DEC:
		bw_data = mfc_bw_info.mpeg4_dec_uhd_bw;
		break;
	case S5P_FIMV_CODEC_VP8_DEC:
		bw_data = mfc_bw_info.vp8_dec_uhd_bw;
		break;
	case S5P_FIMV_CODEC_VP9_DEC:
		bw_data = mfc_bw_info.vp9_dec_uhd_bw;
		break;
	case S5P_FIMV_CODEC_MPEG4_ENC:
	case S5P_FIMV_CODEC_H263_ENC:
		bw_data = mfc_bw_info.mpeg4_enc_uhd_bw;
		break;
	case S5P_FIMV_CODEC_VP8_ENC:
		bw_data = mfc_bw_info.vp8_enc_uhd_bw;
		break;
	case S5P_FIMV_CODEC_VP9_ENC:
		bw_data = mfc_bw_info.vp9_enc_uhd_bw;
		break;
	default:
		bw_data.peak = 0;
		bw_data.read = 0;
		bw_data.write = 0;
		mfc_err_ctx("wrong codec_mode (%d)\n", ctx->codec_mode);
	}

	if (mb > (mb_count_per_uhd_frame * max_fps_per_uhd_frame)) {
		mfc_debug(2, "fix upper mb bound (mb: %ld, fps: %ld)\n", mb, fps);
		mb = mb_count_per_uhd_frame * max_fps_per_uhd_frame;
	}

	peak_bw_per_sec = (bw_data.peak * mb) / mb_count_per_uhd_frame;
	read_bw_per_sec = (bw_data.read * mb) / mb_count_per_uhd_frame;
	write_bw_per_sec = (bw_data.write * mb) / mb_count_per_uhd_frame;

	if (peak_bw_per_sec == 0) {
		mfc_debug(2, "fix lower peak bound (mb: %ld, fps: %ld)\n", mb, fps);
		peak_bw_per_sec = MIN_BW_PER_SEC;
	}
	if (read_bw_per_sec == 0) {
		mfc_debug(2, "fix lower read bound (mb: %ld, fps: %ld)\n", mb, fps);
		read_bw_per_sec = MIN_BW_PER_SEC;
	}
	if (write_bw_per_sec == 0) {
		mfc_debug(2, "fix lower write bound (mb: %ld, fps: %ld)\n", mb, fps);
		write_bw_per_sec = MIN_BW_PER_SEC;
	}

	mfc_bw->peak = (unsigned int)peak_bw_per_sec;
	mfc_bw->read = (unsigned int)read_bw_per_sec;
	mfc_bw->write = (unsigned int)write_bw_per_sec;
}

void s5p_mfc_qos_on(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_platdata *pdata = dev->pdata;
	struct s5p_mfc_qos *qos_table = pdata->qos_table;
	struct s5p_mfc_ctx *qos_ctx;
	struct bts_bw mfc_bw, mfc_bw_ctx;
	unsigned long hw_mb = 0, total_mb = 0, total_fps = 0;
	unsigned int fw_time, sw_time;
	int i, found = 0, enc_found = 0;
	int start_qos_step;

	mutex_lock(&dev->qos_mutex);
	list_for_each_entry(qos_ctx, &dev->qos_queue, qos_list)
		if (qos_ctx == ctx)
			found = 1;

	if (!found)
		list_add_tail(&ctx->qos_list, &dev->qos_queue);

	mfc_bw.peak = 0;
	mfc_bw.read = 0;
	mfc_bw.write = 0;
	/* get the hw macroblock */
	list_for_each_entry(qos_ctx, &dev->qos_queue, qos_list) {
		if (OVER_UHD_ENC60(qos_ctx))
			enc_found = 1;
		hw_mb += mfc_qos_get_mb_per_second(qos_ctx);
		mfc_qos_get_bw_per_second(qos_ctx, &mfc_bw_ctx);
		mfc_bw.peak += mfc_bw_ctx.peak;
		mfc_bw.read += mfc_bw_ctx.read;
		mfc_bw.write += mfc_bw_ctx.write;
		total_fps += (qos_ctx->framerate / 1000);
	}

	start_qos_step = pdata->num_qos_steps;
	if (enc_found && (dev->num_inst == 1))
		start_qos_step = pdata->max_qos_steps;

	/* search the suitable qos table */
	for (i = start_qos_step - 1; i >= 0; i--) {
		fw_time = qos_table[i].time_fw;
		sw_time = (MFC_DRV_TIME + fw_time);

		if ((total_fps * sw_time) >= 1000000)
			total_mb = pdata->max_mb;
		else
			total_mb = ((1000000 * hw_mb) / (1000000 - (total_fps * sw_time)));

		mfc_debug(4, "QoS table[%d] fw_time: %dus, hw_mb: %ld, "
				"sw_time: %d, total_fps: %ld, total_mb: %ld\n",
				i, fw_time, hw_mb, sw_time, total_fps, total_mb);

		if ((total_mb > qos_table[i].threshold_mb) || (i == 0))
			break;
	}

	if (total_mb > pdata->max_mb)
		mfc_debug(4, "QoS overspec mb %ld > %d\n", total_mb, pdata->max_mb);

	mfc_qos_set(ctx, &mfc_bw, i);
	mutex_unlock(&dev->qos_mutex);
}

void s5p_mfc_qos_off(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_platdata *pdata = dev->pdata;
	struct s5p_mfc_qos *qos_table = pdata->qos_table;
	struct s5p_mfc_ctx *qos_ctx;
	struct bts_bw mfc_bw, mfc_bw_ctx;
	unsigned long hw_mb = 0, total_mb = 0, total_fps = 0;
	unsigned int fw_time, sw_time;
	int i, found = 0, enc_found = 0;
	int start_qos_step;

	mutex_lock(&dev->qos_mutex);
	if (list_empty(&dev->qos_queue)) {
		if (atomic_read(&dev->qos_req_cur) != 0) {
			mfc_err_ctx("MFC request count is wrong!\n");
			mfc_qos_operate(dev, MFC_QOS_REMOVE, 0);
		}
		mutex_unlock(&dev->qos_mutex);
		return;
	}

	if (ON_RES_CHANGE(ctx)) {
		mutex_unlock(&dev->qos_mutex);
		return;
	}

	mfc_bw.peak = 0;
	mfc_bw.read = 0;
	mfc_bw.write = 0;

	/* get the hw macroblock */
	list_for_each_entry(qos_ctx, &dev->qos_queue, qos_list) {
		if (qos_ctx == ctx) {
			found = 1;
			continue;
		}

		if (OVER_UHD_ENC60(qos_ctx))
			enc_found = 1;
		hw_mb += mfc_qos_get_mb_per_second(qos_ctx);
		mfc_qos_get_bw_per_second(qos_ctx, &mfc_bw_ctx);
		mfc_bw.peak += mfc_bw_ctx.peak;
		mfc_bw.read += mfc_bw_ctx.read;
		mfc_bw.write += mfc_bw_ctx.write;
		total_fps += (qos_ctx->framerate / 1000);
	}
	if (found)
		list_del(&ctx->qos_list);

	start_qos_step = pdata->num_qos_steps;
	if (enc_found && (dev->num_inst == 1))
		start_qos_step = pdata->max_qos_steps;

	/* search the suitable qos table */
	for (i = start_qos_step - 1; i >= 0; i--) {
		fw_time = qos_table[i].time_fw;
		sw_time = (MFC_DRV_TIME + fw_time);

		if ((total_fps * sw_time) >= 1000000)
			total_mb = pdata->max_mb;
		else
			total_mb = ((1000000 * hw_mb) / (1000000 - (total_fps * sw_time)));

		mfc_debug(4, "QoS table[%d] fw_time: %dus, hw_mb: %ld, "
				"sw_time: %d, total_fps: %ld, total_mb: %ld\n",
				i, fw_time, hw_mb, sw_time, total_fps, total_mb);

		if ((total_mb > qos_table[i].threshold_mb) || (total_mb == 0) || (i == 0))
			break;
	}

	if (total_mb > pdata->max_mb)
		mfc_debug(4, "QoS overspec mb %ld > %d\n", total_mb, pdata->max_mb);

	if (list_empty(&dev->qos_queue) || total_mb == 0)
		mfc_qos_operate(dev, MFC_QOS_REMOVE, 0);
	else
		mfc_qos_set(ctx, &mfc_bw, i);

	mutex_unlock(&dev->qos_mutex);
}

void __mfc_qos_off_all(struct s5p_mfc_dev *dev)
{
	struct s5p_mfc_ctx *qos_ctx, *tmp_ctx;

	mutex_lock(&dev->qos_mutex);
	if (list_empty(&dev->qos_queue)) {
		mfc_err_dev("[QoS][MFCIDLE] MFC QoS list already empty (%d)\n",
				atomic_read(&dev->qos_req_cur));
		mutex_unlock(&dev->qos_mutex);
		return;
	}

	/* Delete all of QoS list */
	list_for_each_entry_safe(qos_ctx, tmp_ctx, &dev->qos_queue, qos_list)
		list_del(&qos_ctx->qos_list);

	/* Select the opend ctx structure for QoS remove */
	mfc_qos_operate(dev, MFC_QOS_REMOVE, 0);
	mutex_unlock(&dev->qos_mutex);
}

void mfc_qos_idle_worker(struct work_struct *work)
{
	struct s5p_mfc_dev *dev;

	dev = container_of(work, struct s5p_mfc_dev, mfc_idle_work);

	mutex_lock(&dev->idle_qos_mutex);
	if (dev->idle_mode == MFC_IDLE_MODE_CANCEL) {
		mfc_change_idle_mode(dev, MFC_IDLE_MODE_NONE);
		mfc_debug(2, "[QoS][MFCIDLE] idle mode is canceled\n");
		mutex_unlock(&dev->idle_qos_mutex);
		return;
	}

	__mfc_qos_off_all(dev);
	mfc_info_dev("[QoS][MFCIDLE] MFC go to QoS idle mode\n");

	mfc_change_idle_mode(dev, MFC_IDLE_MODE_IDLE);
	mutex_unlock(&dev->idle_qos_mutex);
}
#endif

#define COL_FRAME_RATE		0
#define COL_FRAME_INTERVAL	1

#define MFC_MAX_INTERVAL	(2 * USEC_PER_SEC)

/*
 * A framerate table determines framerate by the interval(us) of each frame.
 * Framerate is not accurate, just rough value to seperate overload section.
 * Base line of each section are selected from middle value.
 * 40fps(25000us), 80fps(12500us), 144fps(6940us)
 * 205fps(4860us), 320fps(3125us)
 *
 * interval(us) | 0         3125          4860          6940          12500         25000          |
 * framerate    |    480fps   |    240fps   |    180fps   |    120fps   |    60fps    |    30fps   |
 */
static unsigned long framerate_table[][2] = {
	{  30000, 25000 },
	{  60000, 12500 },
	{ 120000,  6940 },
	{ 180000,  4860 },
	{ 240000,  3125 },
	{ 480000,     0 },
};

static inline unsigned long mfc_qos_timeval_diff(struct timeval *to,
					struct timeval *from)
{
	return (to->tv_sec * USEC_PER_SEC + to->tv_usec)
		- (from->tv_sec * USEC_PER_SEC + from->tv_usec);
}

static unsigned long mfc_qos_get_framerate_by_interval(int interval)
{
	unsigned long i;

	/* if the interval is too big (2sec), framerate set to 0 */
	if (interval > MFC_MAX_INTERVAL)
		return 0;

	for (i = 0; i < ARRAY_SIZE(framerate_table); i++) {
		if (interval > framerate_table[i][COL_FRAME_INTERVAL])
			return framerate_table[i][COL_FRAME_RATE];
	}

	return 0;
}

/* Return the minimum interval between previous and next entry */
static int mfc_qos_get_interval(struct list_head *head, struct list_head *entry)
{
	int prev_interval = MFC_MAX_INTERVAL, next_interval = MFC_MAX_INTERVAL;
	struct mfc_timestamp *prev_ts, *next_ts, *curr_ts;

	curr_ts = list_entry(entry, struct mfc_timestamp, list);

	if (entry->prev != head) {
		prev_ts = list_entry(entry->prev, struct mfc_timestamp, list);
		prev_interval = mfc_qos_timeval_diff(&curr_ts->timestamp, &prev_ts->timestamp);
	}

	if (entry->next != head) {
		next_ts = list_entry(entry->next, struct mfc_timestamp, list);
		next_interval = mfc_qos_timeval_diff(&next_ts->timestamp, &curr_ts->timestamp);
	}

	return (prev_interval < next_interval ? prev_interval : next_interval);
}

static int mfc_qos_dec_add_timestamp(struct s5p_mfc_ctx *ctx,
			struct timeval *time, struct list_head *head)
{
	int replace_entry = 0;
	struct mfc_timestamp *curr_ts = &ctx->ts_array[ctx->ts_count];

	if (ctx->ts_is_full) {
		/* Replace the entry if list of array[ts_count] is same as entry */
		if (&curr_ts->list == head)
			replace_entry = 1;
		else
			list_del(&curr_ts->list);
	}

	memcpy(&curr_ts->timestamp, time, sizeof(struct timeval));
	if (!replace_entry)
		list_add(&curr_ts->list, head);
	curr_ts->interval =
		mfc_qos_get_interval(&ctx->ts_list, &curr_ts->list);
	curr_ts->index = ctx->ts_count;
	ctx->ts_count++;

	if (ctx->ts_count == MFC_TIME_INDEX) {
		ctx->ts_is_full = 1;
		ctx->ts_count %= MFC_TIME_INDEX;
	}

	return 0;
}

static unsigned long mfc_qos_get_fps_by_timestamp(struct s5p_mfc_ctx *ctx, struct timeval *time)
{
	struct list_head *head = &ctx->ts_list;
	struct mfc_timestamp *temp_ts;
	int found;
	int index = 0;
	int min_interval = MFC_MAX_INTERVAL;
	int time_diff;
	unsigned long max_framerate;

	if (debug_ts == 1) {
		/* Debug info */
		mfc_info_ctx("======================================\n");
		mfc_info_ctx("New timestamp = %ld.%06ld, count = %d \n",
			time->tv_sec, time->tv_usec, ctx->ts_count);
	}

	if (list_empty(&ctx->ts_list)) {
		mfc_qos_dec_add_timestamp(ctx, time, &ctx->ts_list);
		return mfc_qos_get_framerate_by_interval(0);
	} else {
		found = 0;
		list_for_each_entry_reverse(temp_ts, &ctx->ts_list, list) {
			time_diff = timeval_compare(time, &temp_ts->timestamp);
			if (time_diff == 0) {
				/* Do not add if same timestamp already exists */
				found = 1;
				break;
			} else if (time_diff > 0) {
				/* Add this after temp_ts */
				mfc_qos_dec_add_timestamp(ctx, time, &temp_ts->list);
				found = 1;
				break;
			}
		}

		if (!found)	/* Add this at first entry */
			mfc_qos_dec_add_timestamp(ctx, time, &ctx->ts_list);
	}

	list_for_each_entry(temp_ts, &ctx->ts_list, list) {
		if (temp_ts->interval < min_interval)
			min_interval = temp_ts->interval;
	}

	max_framerate = mfc_qos_get_framerate_by_interval(min_interval);

	if (debug_ts == 1) {
		/* Debug info */
		index = 0;
		list_for_each_entry(temp_ts, &ctx->ts_list, list) {
			mfc_info_ctx("[%d] timestamp [i:%d]: %ld.%06ld\n",
					index, temp_ts->index,
					temp_ts->timestamp.tv_sec,
					temp_ts->timestamp.tv_usec);
			index++;
		}
		mfc_info_ctx("Min interval = %d, It is %ld fps\n",
				min_interval, max_framerate);
	} else if (debug_ts == 2) {
		mfc_info_ctx("Min interval = %d, It is %ld fps\n",
				min_interval, max_framerate);
	}

	/* Calculation the last frame fps for drop control */
	temp_ts = list_entry(head->prev, struct mfc_timestamp, list);
	ctx->ts_last_interval = temp_ts->interval;

	if (!ctx->ts_is_full) {
		if (debug_ts == 1)
			mfc_info_ctx("ts doesn't full, keep %ld fps\n", ctx->framerate);
		return ctx->framerate;
	}

	return max_framerate;
}

void s5p_mfc_qos_update_framerate(struct s5p_mfc_ctx *ctx, int idle_trigger_only)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	bool update_framerate = false, update_idle = false;
	unsigned long framerate;

	/* 1) Idle mode trigger */
	mutex_lock(&dev->idle_qos_mutex);
	if (dev->idle_mode == MFC_IDLE_MODE_IDLE) {
		mfc_debug(2, "[QoS][MFCIDLE] restart QoS control\n");
		mfc_change_idle_mode(dev, MFC_IDLE_MODE_NONE);
		update_idle = true;
	} else if (dev->idle_mode == MFC_IDLE_MODE_RUNNING) {
		mfc_debug(2, "[QoS][MFCIDLE] restart QoS control, cancel idle\n");
		mfc_change_idle_mode(dev, MFC_IDLE_MODE_CANCEL);
		update_idle = true;
	}
	mutex_unlock(&dev->idle_qos_mutex);

	if (idle_trigger_only)
		goto update_qos;

	/* 2) when src timestamp isn't full, only check operating framerate by user */
	if (!ctx->ts_is_full) {
		if (ctx->operating_framerate && (ctx->operating_framerate > ctx->framerate)) {
			mfc_debug(2, "[QoS] operating fps changed: %ld\n", ctx->operating_framerate);
			ctx->framerate = ctx->operating_framerate;
			update_framerate = true;
		}
	} else {
		/* 3) get src framerate */
		framerate = ctx->last_framerate;

		/* 4) check operating framerate by user */
		if (ctx->operating_framerate && (ctx->operating_framerate > framerate)) {
			mfc_debug(2, "[QoS] operating fps %ld\n", ctx->operating_framerate);
			framerate = ctx->operating_framerate;
		}

		/* 5) check non-real-time */
		if (ctx->rt == MFC_NON_RT && (framerate < DEC_DEFAULT_FPS)) {
			mfc_debug(2, "[QoS] max operating fps %ld\n", DEC_DEFAULT_FPS);
			framerate = DEC_DEFAULT_FPS;
		}

		if (framerate && (framerate != ctx->framerate)) {
			mfc_debug(2, "[QoS] fps changed: %ld -> %ld, qos ratio: %d\n",
					ctx->framerate, framerate, ctx->qos_ratio);
			ctx->framerate = framerate;
			update_framerate = true;
		}
	}

update_qos:
	if (update_idle || update_framerate)
		s5p_mfc_qos_on(ctx);
}

void s5p_mfc_qos_update_last_framerate(struct s5p_mfc_ctx *ctx, u64 timestamp)
{
	struct timeval time;

	time.tv_sec = timestamp / NSEC_PER_SEC;
	time.tv_usec = (timestamp - (time.tv_sec * NSEC_PER_SEC)) / NSEC_PER_USEC;

	ctx->last_framerate = mfc_qos_get_fps_by_timestamp(ctx, &time);
	if (ctx->last_framerate > MFC_MAX_FPS)
		ctx->last_framerate = MFC_MAX_FPS;
	ctx->last_framerate = (ctx->qos_ratio * ctx->last_framerate) / 100;
}
