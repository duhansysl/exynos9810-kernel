/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-hw-mcscaler-v2.h"
#include "api/fimc-is-hw-api-mcscaler-v2.h"
#include "../interface/fimc-is-interface-ischain.h"
#include "fimc-is-param.h"
#include "fimc-is-err.h"
#include <linux/videodev2_exynos_media.h>

spinlock_t	shared_output_slock;
static ulong hw_mcsc_out_configured = 0xFFFF;
#define HW_MCSC_OUT_CLEARED_ALL (MCSC_OUTPUT_MAX)
#ifdef HW_BUG_WA_NO_CONTOLL_PER_FRAME
static bool flag_mcsc_hw_bug_lock;
#endif
static bool first_shot_tag;

static int fimc_is_hw_mcsc_rdma_cfg(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame);
static void fimc_is_hw_mcsc_wdma_cfg(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame);
static void fimc_is_hw_mcsc_size_dump(struct fimc_is_hw_ip *hw_ip);

static const struct tdnr_configs init_tdnr_cfgs = {
	.general_cfg = {
		.use_average_current = true,
		.auto_coeff_3d = true,
		.blending_threshold = 6,
	},
	.yuv_tables = {
		.x_grid_y = {57, 89, 126},
		.y_std_offset = 27,
		.y_std_slope= {-22, -6, -33, 31},
		.x_grid_u = {57, 89, 126},
		.u_std_offset = 4,
		.u_std_slope = {36, 2, -24, 1},
		.x_grid_v = {57, 89, 126},
		.v_std_offset = 4,
		.v_std_slope = {36, 3, -13, -25},
	},
	.temporal_dep_cfg = {
		.temporal_weight_coeff_y1 = 6,
		.temporal_weight_coeff_y2 = 12,
		.temporal_weight_coeff_uv1 = 6,
		.temporal_weight_coeff_uv2 = 12,
		.auto_lut_gains_y = {3, 9, 15},
		.y_offset = 0,
		.auto_lut_gains_uv = {3, 9, 15},
		.uv_offset = 0,
	},
	.temporal_indep_cfg = {
		.prev_gridx = {4, 8, 12},
		.prev_gridx_lut = {2, 6, 10, 14},
	},
	.constant_lut_coeffs = {0, 0, 0},
	.refine_cfg = {
		.is_refine_on = true,
		.refine_mode = TDNR_REFINE_MAX,
		.refine_threshold = 12,
		.refine_coeff_update = 4,
	},
	.regional_dep_cfg = {
		.is_region_diff_on = false,
		.region_gain = 15,
		.other_channels_check = false,
		.other_channel_gain = 10,
	},
	.regional_indep_cfg = {
		.dont_use_region_sign = true,
		.diff_condition_are_all_components_similar = false,
		.line_condition = true,
		.is_motiondetect_luma_mode_mean = true,
		.region_offset = 0,
		.is_motiondetect_chroma_mode_mean = true,
		.other_channel_offset = 0,
		.coefficient_offset = 16,
	},
	.spatial_dep_cfg = {
		.weight_mode = TDNR_WEIGHT_MIN,
		.spatial_gain = 4,
		.spatial_separate_weights = false,
		.spatial_luma_gain = {34, 47, 60, 73},
		.spatial_uv_gain = {34, 47, 60, 73},
	},
	.spatial_indep_cfg = {
		.spatial_refine_threshold = 17,
		.spatial_luma_offset = {0, 0, 0, 0},
		.spatial_uv_offset = {0, 0, 0, 0},
	},
};

static const struct djag_setfile_contents init_djag_cfgs = {
	.xfilter_dejagging_coeff_cfg = {
		.xfilter_dejagging_weight0 = 3,
		.xfilter_dejagging_weight1 = 4,
		.xfilter_hf_boost_weight = 4,
		.center_hf_boost_weight = 2,
		.diagonal_hf_boost_weight = 3,
		.center_weighted_mean_weight = 3,
	},
	.thres_1x5_matching_cfg = {
		.thres_1x5_matching_sad = 128,
		.thres_1x5_abshf = 512,
	},
	.thres_shooting_detect_cfg = {
		.thres_shooting_llcrr = 255,
		.thres_shooting_lcr = 255,
		.thres_shooting_neighbor = 255,
		.thres_shooting_uucdd = 80,
		.thres_shooting_ucd = 80,
		.min_max_weight = 2,
	},
	.lfsr_seed_cfg = {
		.lfsr_seed_0 = 44257,
		.lfsr_seed_1 = 4671,
		.lfsr_seed_2 = 47792,
	},
	.dither_cfg = {
		.dither_value = {0, 0, 1, 2, 3, 4, 6, 7, 8},
		.sat_ctrl = 5,
		.dither_sat_thres = 1000,
		.dither_thres = 5,
	},
	.cp_cfg = {
		.cp_hf_thres = 40,
		.cp_arbi_max_cov_offset = 0,
		.cp_arbi_max_cov_shift = 6,
		.cp_arbi_denom = 7,
		.cp_arbi_mode = 1,
	},
};

static int fimc_is_hw_mcsc_handle_interrupt(u32 id, void *context)
{
	struct fimc_is_hardware *hardware;
	struct fimc_is_hw_ip *hw_ip = NULL;
	struct fimc_is_group *head;
	u32 status, intr_mask, intr_status;
	bool err_intr_flag = false;
	int ret = 0;
	u32 hl = 0, vl = 0;
	u32 instance;
	u32 hw_fcount, index;
	struct mcs_param *param;
	bool flag_clk_gate = false;
	ulong flag = 0;

	hw_ip = (struct fimc_is_hw_ip *)context;
	hardware = hw_ip->hardware;
	hw_fcount = atomic_read(&hw_ip->fcount);
	instance = atomic_read(&hw_ip->instance);
	param = &hw_ip->region[instance]->parameter.mcs;

	if (!test_bit(HW_OPEN, &hw_ip->state)) {
		err_hw("[ID:%d][MCSC] invalid interrupt", hw_ip->id);
		return 0;
	}

	fimc_is_scaler_get_input_status(hw_ip->regs, hw_ip->id, &hl, &vl);
	/* read interrupt status register (sc_intr_status) */
	intr_mask = fimc_is_scaler_get_intr_mask(hw_ip->regs, hw_ip->id);
	intr_status = fimc_is_scaler_get_intr_status(hw_ip->regs, hw_ip->id);
	status = (~intr_mask) & intr_status;

	fimc_is_scaler_clear_intr_src(hw_ip->regs, hw_ip->id, status);

	if (!test_bit(HW_RUN, &hw_ip->state)) {
		mserr_hw("HW disabled!! interrupt(0x%x)", instance, hw_ip, status);
		goto p_err;
	}

	if (status & (1 << INTR_MC_SCALER_OVERFLOW)) {
		mserr_hw("Overflow!! (0x%x)", instance, hw_ip, status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_OUTSTALL)) {
		mserr_hw("Output Block BLOCKING!! (0x%x)", instance, hw_ip, status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_INPUT_VERTICAL_UNF)) {
		mserr_hw("Input OTF Vertical Underflow!! (0x%x)", instance, hw_ip, status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_INPUT_VERTICAL_OVF)) {
		mserr_hw("Input OTF Vertical Overflow!! (0x%x)", instance, hw_ip, status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_INPUT_HORIZONTAL_UNF)) {
		mserr_hw("Input OTF Horizontal Underflow!! (0x%x)", instance, hw_ip, status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_INPUT_HORIZONTAL_OVF)) {
		mserr_hw("Input OTF Horizontal Overflow!! (0x%x)", instance, hw_ip, status);
		err_intr_flag = true;
	}

	if (status & (1 << INTR_MC_SCALER_WDMA_FINISH))
		mserr_hw("Disabeld interrupt occurred! WDAM FINISH!! (0x%x)", instance, hw_ip, status);

	if (status & (1 << INTR_MC_SCALER_FRAME_START)) {
		atomic_inc(&hw_ip->count.fs);
		hw_ip->cur_s_int++;

		if (hw_ip->cur_s_int == 1) {
			hw_ip->debug_index[1] = hw_ip->debug_index[0] % DEBUG_FRAME_COUNT;
			index = hw_ip->debug_index[1];
			hw_ip->debug_info[index].fcount = hw_ip->debug_index[0];
			hw_ip->debug_info[index].cpuid[DEBUG_POINT_FRAME_START] = raw_smp_processor_id();
			hw_ip->debug_info[index].time[DEBUG_POINT_FRAME_START] = cpu_clock(raw_smp_processor_id());
			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				msinfo_hw("[F:%d]F.S\n", instance, hw_ip, hw_fcount);

			if (param->input.dma_cmd == DMA_INPUT_COMMAND_ENABLE) {
				fimc_is_hardware_frame_start(hw_ip, instance);
			} else {
				clear_bit(HW_CONFIG, &hw_ip->state);
				atomic_set(&hw_ip->status.Vvalid, V_VALID);
			}
		}

		/* for set shadow register write start */
		head = GET_HEAD_GROUP_IN_DEVICE(FIMC_IS_DEVICE_ISCHAIN, hw_ip->group[instance]);
		if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &head->state))
			fimc_is_scaler_set_shadow_ctrl(hw_ip->regs, hw_ip->id, SHADOW_WRITE_START);

		if (hw_ip->cur_s_int < hw_ip->num_buffers) {
			if (hw_ip->mframe) {
				struct fimc_is_frame *mframe = hw_ip->mframe;
				mframe->cur_buf_index = hw_ip->cur_s_int;
				/* WDMA cfg */
				fimc_is_hw_mcsc_wdma_cfg(hw_ip, mframe);

				/* RDMA cfg */
				if (param->input.dma_cmd == DMA_INPUT_COMMAND_ENABLE) {
					ret = fimc_is_hw_mcsc_rdma_cfg(hw_ip, mframe);
					if (ret) {
						mserr_hw("[F:%d]mcsc rdma_cfg failed\n",
							mframe->instance, hw_ip, mframe->fcount);
						return ret;
					}
				}
			} else {
				serr_hw("mframe is null(s:%d, e:%d, t:%d)", hw_ip,
					hw_ip->cur_s_int, hw_ip->cur_e_int, hw_ip->num_buffers);
			}
		} else {
			struct fimc_is_group *group;
			group = hw_ip->group[instance];
			/*
			 * In case of M2M mcsc, just supports only one buffering.
			 * So, in start irq, "setting to stop mcsc for N + 1" should be assigned.
			 *
			 * TODO: Don't touch global control, but we don't know how to be mapped
			 *       with group-id and scX_ctrl.
			 */
			if (!test_bit(FIMC_IS_GROUP_OTF_INPUT, &head->state))
				fimc_is_scaler_stop(hw_ip->regs, hw_ip->id);
		}
	}

	if (status & (1 << INTR_MC_SCALER_FRAME_END)) {
		atomic_inc(&hw_ip->count.fe);
		hw_ip->cur_e_int++;
		if (hw_ip->cur_e_int >= hw_ip->num_buffers) {
			spin_lock_irqsave(&shared_output_slock, flag);
			fimc_is_hw_mcsc_frame_done(hw_ip, NULL, IS_SHOT_SUCCESS);
			spin_unlock_irqrestore(&shared_output_slock, flag);

			if (!atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
				sinfo_hw("[F:%d]F.E\n", hw_ip, hw_fcount);

			atomic_set(&hw_ip->status.Vvalid, V_BLANK);
			if (atomic_read(&hw_ip->count.fs) < atomic_read(&hw_ip->count.fe)) {
				mserr_hw("fs(%d), fe(%d), dma(%d), status(0x%x)", instance, hw_ip,
					atomic_read(&hw_ip->count.fs),
					atomic_read(&hw_ip->count.fe),
					atomic_read(&hw_ip->count.dma), status);
			}

			wake_up(&hw_ip->status.wait_queue);
			flag_clk_gate = true;
			hw_ip->mframe = NULL;
		}
	}

	/* for handle chip dependant intr */
	err_intr_flag |= fimc_is_scaler_handle_extended_intr(status);

	if (err_intr_flag) {
		msinfo_hw("[F:%d] Ocurred error interrupt (%d,%d) status(0x%x)\n",
			instance, hw_ip, hw_fcount, hl, vl, status);
		fimc_is_scaler_dump(hw_ip->regs);
		fimc_is_hardware_size_dump(hw_ip);
	}

	if (flag_clk_gate)
		CALL_HW_OPS(hw_ip, clk_gate, instance, false, false);

p_err:
	return ret;
}

void fimc_is_hw_mcsc_hw_info(struct fimc_is_hw_ip *hw_ip, struct fimc_is_hw_mcsc_cap *cap)
{
	int i = 0;

	if (!(hw_ip && cap))
		return;

	sinfo_hw("==== h/w info(ver:0x%X) ====\n", hw_ip, cap->hw_ver);
	info_hw("[IN] max_out:%d, in(otf/dma):%d/%d, hwfc:%d, tdnr:%d\n",
			cap->max_output, cap->in_otf, cap->in_dma, cap->hwfc, cap->tdnr);
	for (i = MCSC_OUTPUT0; i < cap->max_output; i++)
		info_hw("[OUT%d] out(otf/dma):%d/%d, hwfc:%d\n", i,
			cap->out_otf[i], cap->out_dma[i], cap->out_hwfc[i]);

	sinfo_hw("========================\n", hw_ip);
}

void get_mcsc_hw_ip(struct fimc_is_hardware *hardware, struct fimc_is_hw_ip **hw_ip0, struct fimc_is_hw_ip **hw_ip1)
{
	int hw_slot = -1;

	hw_slot = fimc_is_hw_slot_id(DEV_HW_MCSC0);
	if (valid_hw_slot_id(hw_slot))
		*hw_ip0 = &hardware->hw_ip[hw_slot];

	hw_slot = fimc_is_hw_slot_id(DEV_HW_MCSC1);
	if (valid_hw_slot_id(hw_slot))
		*hw_ip1 = &hardware->hw_ip[hw_slot];
}

static int fimc_is_hw_mcsc_open(struct fimc_is_hw_ip *hw_ip, u32 instance,
	struct fimc_is_group *group)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct fimc_is_hw_mcsc_cap *cap;
	struct fimc_is_hardware *hardware;
	struct fimc_is_hw_ip *hw_ip0 = NULL, *hw_ip1 = NULL;
	u32 output_id;
	int i;

	FIMC_BUG(!hw_ip);

	if (test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	frame_manager_probe(hw_ip->framemgr, FRAMEMGR_ID_HW | (1 << hw_ip->id), "HWMCS");
	frame_manager_probe(hw_ip->framemgr_late, FRAMEMGR_ID_HW | (1 << hw_ip->id) | 0xF000, "HWMCS LATE");
	frame_manager_open(hw_ip->framemgr, FIMC_IS_MAX_HW_FRAME);
	frame_manager_open(hw_ip->framemgr_late, FIMC_IS_MAX_HW_FRAME_LATE);

	hw_ip->priv_info = vzalloc(sizeof(struct fimc_is_hw_mcsc));
	if(!hw_ip->priv_info) {
		mserr_hw("hw_ip->priv_info(null)", instance, hw_ip);
		ret = -ENOMEM;
		goto err_alloc;
	}

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	hw_mcsc->instance = FIMC_IS_STREAM_COUNT;

	cap = GET_MCSC_HW_CAP(hw_ip);

	/* get the mcsc hw info */
	ret = fimc_is_hw_query_cap((void *)cap, hw_ip->id);
	if (ret) {
		mserr_hw("failed to get hw info", instance, hw_ip);
		ret = -EINVAL;
		goto err_query_cap;
	}

	/* print hw info */
	fimc_is_hw_mcsc_hw_info(hw_ip, cap);

	hw_ip->mframe = NULL;
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	set_bit(HW_OPEN, &hw_ip->state);
	msdbg_hw(2, "open: [G:0x%x], framemgr[%s]", instance, hw_ip,
		GROUP_ID(group->id), hw_ip->framemgr->name);

	hardware = hw_ip->hardware;
	get_mcsc_hw_ip(hardware, &hw_ip0, &hw_ip1);

	for (i = 0; i < SENSOR_POSITION_MAX; i++)
		hw_mcsc->applied_setfile[i] = NULL;

	if (cap->enable_shared_output) {
		if (hw_ip0 && test_bit(HW_RUN, &hw_ip0->state))
			return 0;

		if (hw_ip1 && test_bit(HW_RUN, &hw_ip1->state))
			return 0;
	}

	for (output_id = MCSC_OUTPUT0; output_id < cap->max_output; output_id++)
		set_bit(output_id, &hw_mcsc_out_configured);
	clear_bit(HW_MCSC_OUT_CLEARED_ALL, &hw_mcsc_out_configured);

	msinfo_hw("mcsc_open: done, (0x%lx)\n", instance, hw_ip, hw_mcsc_out_configured);

	return 0;

err_query_cap:
	vfree(hw_ip->priv_info);
err_alloc:
	frame_manager_close(hw_ip->framemgr);
	frame_manager_close(hw_ip->framemgr_late);
	return ret;
}

static int fimc_is_hw_mcsc_init(struct fimc_is_hw_ip *hw_ip, u32 instance,
	struct fimc_is_group *group, bool flag, u32 module_id)
{
	int ret = 0;
	u32 entry, output_id;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct fimc_is_hw_mcsc_cap *cap;
	struct fimc_is_subdev *subdev;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_video *video;

	FIMC_BUG(!hw_ip);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	hw_mcsc->rep_flag[instance] = flag;
	clear_bit(ALL_BLOCK_SET_DONE, &hw_mcsc->blk_set_ctrl[instance]);

	cap = GET_MCSC_HW_CAP(hw_ip);
	for (output_id = MCSC_OUTPUT0; output_id < cap->max_output; output_id++) {
		if (test_bit(output_id, &hw_mcsc->out_en)) {
			msdbg_hw(2, "already set output(%d)\n", instance, hw_ip, output_id);
			continue;
		}

		entry = GET_ENTRY_FROM_OUTPUT_ID(output_id);
		subdev = group->subdev[entry];
		if (!subdev)
			continue;

		vctx = subdev->vctx;
		if (!vctx) {
			continue;
		}

		video = vctx->video;
		if (!video) {
			mserr_hw("video is NULL. entry(%d)", instance, hw_ip, entry);
			BUG();
		}
		set_bit(output_id, &hw_mcsc->out_en);
	}

	set_bit(HW_INIT, &hw_ip->state);
	msdbg_hw(2, "init: out_en[0x%lx]\n", instance, hw_ip, hw_mcsc->out_en);

	return ret;
}

static int fimc_is_hw_mcsc_close(struct fimc_is_hw_ip *hw_ip, u32 instance)
{
	int ret = 0;
	u32 output_id;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct fimc_is_hw_mcsc_cap *cap;

	FIMC_BUG(!hw_ip);

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return 0;

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	cap = GET_MCSC_HW_CAP(hw_ip);

	/* clear out_en bit */
	for (output_id = MCSC_OUTPUT0; output_id < cap->max_output; output_id++) {
		if (test_bit(output_id, &hw_mcsc->out_en))
			clear_bit(output_id, &hw_mcsc->out_en);
	}

	vfree(hw_ip->priv_info);
	frame_manager_close(hw_ip->framemgr);
	frame_manager_close(hw_ip->framemgr_late);

	clear_bit(HW_OPEN, &hw_ip->state);

	return ret;
}

static int fimc_is_hw_mcsc_enable(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	ulong flag = 0;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	atomic_inc(&hw_ip->run_rsccount);
	if (test_bit(HW_RUN, &hw_ip->state))
		return ret;

	msdbg_hw(2, "enable: start, (0x%lx)\n", instance, hw_ip, hw_mcsc_out_configured);

	spin_lock_irqsave(&shared_output_slock, flag);
	ret = fimc_is_hw_mcsc_reset(hw_ip);
	if (ret != 0) {
		mserr_hw("MCSC sw reset fail", instance, hw_ip);
		spin_unlock_irqrestore(&shared_output_slock, flag);
		return -ENODEV;
	}

	ret = fimc_is_hw_mcsc_clear_interrupt(hw_ip);
	if (ret != 0) {
		mserr_hw("MCSC sw reset fail", instance, hw_ip);
		spin_unlock_irqrestore(&shared_output_slock, flag);
		return -ENODEV;
	}

	msdbg_hw(2, "enable: done, (0x%lx)\n", instance, hw_ip, hw_mcsc_out_configured);

	set_bit(HW_RUN, &hw_ip->state);
	spin_unlock_irqrestore(&shared_output_slock, flag);

	first_shot_tag = 0;

	return ret;
}

static int fimc_is_hw_mcsc_disable(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	u32 input_id, output_id;
	long timetowait;
	bool config = true;
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct fimc_is_hardware *hardware;
	struct fimc_is_hw_ip *hw_ip0 = NULL, *hw_ip1 = NULL;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!cap);
	FIMC_BUG(!hw_ip->priv_info);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (atomic_dec_return(&hw_ip->run_rsccount) > 0)
		return 0;

	msinfo_hw("mcsc_disable: Vvalid(%d)\n", instance, hw_ip,
		atomic_read(&hw_ip->status.Vvalid));

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (test_bit(HW_RUN, &hw_ip->state)) {
		timetowait = wait_event_timeout(hw_ip->status.wait_queue,
			!atomic_read(&hw_ip->status.Vvalid),
			FIMC_IS_HW_STOP_TIMEOUT);

		if (!timetowait)
			mserr_hw("wait FRAME_END timeout (%ld)", instance,
				hw_ip, timetowait);

		ret = fimc_is_hw_mcsc_clear_interrupt(hw_ip);
		if (ret != 0) {
			mserr_hw("MCSC sw clear_interrupt fail", instance, hw_ip);
			return -ENODEV;
		}

		clear_bit(HW_RUN, &hw_ip->state);
		clear_bit(HW_CONFIG, &hw_ip->state);
	} else {
		msdbg_hw(2, "already disabled\n", instance, hw_ip);
	}

	hw_ip->mframe = NULL;
	hardware = hw_ip->hardware;
	get_mcsc_hw_ip(hardware, &hw_ip0, &hw_ip1);

	if (hw_ip0 && test_bit(HW_RUN, &hw_ip0->state))
		return 0;

	if (hw_ip1 && test_bit(HW_RUN, &hw_ip1->state))
		return 0;


	if (hw_ip0)
		fimc_is_scaler_stop(hw_ip0->regs, hw_ip0->id);

	if (hw_ip1)
		fimc_is_scaler_stop(hw_ip1->regs, hw_ip1->id);

	/* disable MCSC */
	if (cap->in_dma == MCSC_CAP_SUPPORT)
		fimc_is_scaler_clear_rdma_addr(hw_ip->regs);

	for (output_id = MCSC_OUTPUT0; output_id < cap->max_output; output_id++) {
		input_id = fimc_is_scaler_get_scaler_path(hw_ip->regs, hw_ip->id, output_id);
		config = (input_id == hw_ip->id ? true: false);
		if (cap->enable_shared_output == false || !test_bit(output_id, &hw_mcsc_out_configured)) {
			msinfo_hw("[OUT:%d]hw_mcsc_disable: clear_wdma_addr\n", instance, hw_ip, output_id);
			fimc_is_scaler_clear_wdma_addr(hw_ip->regs, output_id);
		}
	}

	fimc_is_scaler_clear_shadow_ctrl(hw_ip->regs, hw_ip->id);

	/* disable TDNR */
	if (cap->tdnr == MCSC_CAP_SUPPORT) {
		fimc_is_scaler_set_tdnr_mode_select(hw_ip->regs, TDNR_MODE_BYPASS);

		fimc_is_scaler_clear_tdnr_rdma_addr(hw_ip->regs, TDNR_IMAGE);
		fimc_is_scaler_clear_tdnr_rdma_addr(hw_ip->regs, TDNR_WEIGHT);

		fimc_is_scaler_set_tdnr_wdma_enable(hw_ip->regs, TDNR_WEIGHT, false);
		fimc_is_scaler_clear_tdnr_wdma_addr(hw_ip->regs, TDNR_WEIGHT);

		hw_mcsc->cur_tdnr_mode = TDNR_MODE_BYPASS;
	}

	for (output_id = MCSC_OUTPUT0; output_id < cap->max_output; output_id++)
		set_bit(output_id, &hw_mcsc_out_configured);
	clear_bit(HW_MCSC_OUT_CLEARED_ALL, &hw_mcsc_out_configured);

	msinfo_hw("mcsc_disable: done, (0x%lx)\n", instance, hw_ip, hw_mcsc_out_configured);

	return ret;
}

static int fimc_is_hw_mcsc_rdma_cfg(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame)
{
	int ret = 0;
	int i;
	u32 rdma_addr[4] = {0};
	struct mcs_param *param;
	u32 plane;

	param = &hw_ip->region[frame->instance]->parameter.mcs;

	plane = param->input.plane;
	for (i = 0; i < plane; i++)
		rdma_addr[i] = (typeof(*rdma_addr))
			frame->dvaddr_buffer[plane * frame->cur_buf_index + i];

	/* DMA in */
	msdbg_hw(2, "[F:%d]rdma_cfg [addr: %x]\n",
		frame->instance, hw_ip, frame->fcount, rdma_addr[0]);

	if ((rdma_addr[0] == 0)
		&& (param->input.dma_cmd == DMA_INPUT_COMMAND_ENABLE)) {
		mserr_hw("Wrong rdma_addr(%x)\n", frame->instance, hw_ip, rdma_addr[0]);
		fimc_is_scaler_clear_rdma_addr(hw_ip->regs);
		ret = -EINVAL;
		return ret;
	}

	/* use only one buffer (per-frame) */
	fimc_is_scaler_set_rdma_frame_seq(hw_ip->regs, 0x1 << USE_DMA_BUFFER_INDEX);

	if (param->input.plane == DMA_INPUT_PLANE_4) {
		/* 8+2(10bit) format */
		fimc_is_scaler_set_rdma_addr(hw_ip->regs,
			rdma_addr[0], rdma_addr[1], 0, USE_DMA_BUFFER_INDEX);
		fimc_is_scaler_set_rdma_2bit_addr(hw_ip->regs,
			rdma_addr[2], rdma_addr[3], USE_DMA_BUFFER_INDEX);
	} else {
		fimc_is_scaler_set_rdma_addr(hw_ip->regs,
			rdma_addr[0], rdma_addr[1], rdma_addr[2],
			USE_DMA_BUFFER_INDEX);
	}
	return ret;
}

static void fimc_is_hw_mcsc_wdma_cfg(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame)
{
	int i;
	struct mcs_param *param;
	u32 wdma_addr[MCSC_OUTPUT_MAX][4] = {{0}};
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);
	struct fimc_is_hw_mcsc *hw_mcsc;
	u32 plane;

	FIMC_BUG_VOID(!cap);
	FIMC_BUG_VOID(!hw_ip->priv_info);

	param = &hw_ip->region[frame->instance]->parameter.mcs;
	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (frame->type == SHOT_TYPE_INTERNAL)
		goto skip_addr;

	plane = param->output[MCSC_OUTPUT0].plane;
	for (i = 0; i < plane; i++) {
		wdma_addr[MCSC_OUTPUT0][i] =
			frame->sc0TargetAddress[plane * frame->cur_buf_index + i];
		dbg_hw(2, "M0P(P:%d)(A:0x%X)\n", i, wdma_addr[MCSC_OUTPUT0][i]);
	}

	plane = param->output[MCSC_OUTPUT1].plane;
	for (i = 0; i < plane; i++) {
		wdma_addr[MCSC_OUTPUT1][i] =
			frame->sc1TargetAddress[plane * frame->cur_buf_index + i];
		dbg_hw(2, "M1P(P:%d)(A:0x%X)\n", i, wdma_addr[MCSC_OUTPUT1][i]);
	}

	plane = param->output[MCSC_OUTPUT2].plane;
	for (i = 0; i < plane; i++) {
		wdma_addr[MCSC_OUTPUT2][i] =
			frame->sc2TargetAddress[plane * frame->cur_buf_index + i];
		dbg_hw(2, "M2P(P:%d)(A:0x%X)\n", i, wdma_addr[MCSC_OUTPUT2][i]);
	}

	plane = param->output[MCSC_OUTPUT3].plane;
	for (i = 0; i < plane; i++) {
		wdma_addr[MCSC_OUTPUT3][i] =
			frame->sc3TargetAddress[plane * frame->cur_buf_index + i];
		dbg_hw(2, "M3P(P:%d)(A:0x%X)\n", i, wdma_addr[MCSC_OUTPUT3][i]);
	}

	plane = param->output[MCSC_OUTPUT4].plane;
	for (i = 0; i < plane; i++) {
		wdma_addr[MCSC_OUTPUT4][i] =
			frame->sc4TargetAddress[plane * frame->cur_buf_index + i];
		dbg_hw(2, "M4P(P:%d)(A:0x%X)\n", i, wdma_addr[MCSC_OUTPUT4][i]);
	}

	plane = param->output[MCSC_OUTPUT5].plane;
	for (i = 0; i < plane; i++) {
		wdma_addr[MCSC_OUTPUT5][i] =
			frame->sc5TargetAddress[plane * frame->cur_buf_index + i];
		dbg_hw(2, "M5P(P:%d)(A:0x%X)\n", i, wdma_addr[MCSC_OUTPUT5][i]);
	}
skip_addr:

	/* DMA out */
	for (i = MCSC_OUTPUT0; i < cap->max_output; i++) {
		if ((cap->out_dma[i] != MCSC_CAP_SUPPORT) || !test_bit(i, &hw_mcsc->out_en))
			continue;

		msdbg_hw(2, "[F:%d]wdma_cfg [T:%d][addr%d: %x]\n", frame->instance, hw_ip,
			frame->fcount, frame->type, i, wdma_addr[i][0]);

		if (param->output[i].dma_cmd != DMA_OUTPUT_COMMAND_DISABLE
			&& wdma_addr[i][0] != 0
			&& frame->type != SHOT_TYPE_INTERNAL) {

			if (cap->enable_shared_output && test_bit(i, &hw_mcsc_out_configured)
				&& frame->type != SHOT_TYPE_MULTI) {
				mswarn_hw("[OUT:%d]DMA_OUTPUT in running state[F:%d]",
					frame->instance, hw_ip, i, frame->fcount);
				return;
			}
			set_bit(i, &hw_mcsc_out_configured);

			msdbg_hw(2, "[OUT:%d]dma_out enabled\n", frame->instance, hw_ip, i);
			if (i != MCSC_OUTPUT_DS)
				fimc_is_scaler_set_dma_out_enable(hw_ip->regs, i, true);

			/* use only one buffer (per-frame) */
			fimc_is_scaler_set_wdma_frame_seq(hw_ip->regs, i,
				0x1 << USE_DMA_BUFFER_INDEX);
			/* 8+2 (10bit) format : It's not considered multi-buffer shot */
			if (param->output[i].plane == DMA_OUTPUT_PLANE_4) {
				/* addr_2bit_y, addr_2bit_uv */
				fimc_is_scaler_set_wdma_addr(hw_ip->regs, i,
					wdma_addr[i][0], wdma_addr[i][1], 0, USE_DMA_BUFFER_INDEX);
				fimc_is_scaler_set_wdma_2bit_addr(hw_ip->regs, i,
					wdma_addr[i][2], wdma_addr[i][3], USE_DMA_BUFFER_INDEX);
			} else {
				fimc_is_scaler_set_wdma_addr(hw_ip->regs, i,
					wdma_addr[i][0], wdma_addr[i][1], wdma_addr[i][2],
					USE_DMA_BUFFER_INDEX);
			}
		} else {
			u32 wdma_enable = 0;

			if (hw_ip->id == DEV_HW_MCSC1 && first_shot_tag == 0)
				hw_ip->debug_info[0].time[4] = cpu_clock(raw_smp_processor_id());

			wdma_enable = fimc_is_scaler_get_dma_out_enable(hw_ip->regs, i);
			if (wdma_enable && (cap->enable_shared_output == false || !test_bit(i, &hw_mcsc_out_configured))) {
				fimc_is_scaler_set_dma_out_enable(hw_ip->regs, i, false);
				fimc_is_scaler_clear_wdma_addr(hw_ip->regs, i);
				msdbg_hw(2, "[OUT:%d]shot: dma_out disabled\n",
						frame->instance, hw_ip, i);

				if (i == MCSC_OUTPUT_DS) {
					fimc_is_scaler_set_ds_enable(hw_ip->regs, false);
					msdbg_hw(2, "DS off\n", frame->instance, hw_ip);
				}
			}
			msdbg_hw(2, "[OUT:%d]mcsc_wdma_cfg:wmda_enable(%d)[F:%d][T:%d][cmd:%d][addr:0x%x]\n",
				frame->instance, hw_ip, i, wdma_enable, frame->fcount, frame->type,
				param->output[i].dma_cmd, wdma_addr[i][0]);
		}
	}
}

static int fimc_is_hw_mcsc_shot(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame,
	ulong hw_map)
{
	int ret = 0;
	int ret_internal = 0;
	struct fimc_is_group *head;
	struct fimc_is_hardware *hardware;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct is_param_region *param;
	struct mcs_param *mcs_param;
	bool start_flag = true;
	u32 instance;
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);
	ulong flag;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!frame);
	FIMC_BUG(!cap);

	hardware = hw_ip->hardware;
	instance = frame->instance;

	msdbgs_hw(2, "[F:%d]shot\n", instance, hw_ip, frame->fcount);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		msdbg_hw(2, "not initialized\n", instance, hw_ip);
		return -EINVAL;
	}

	if ((!test_bit(ENTRY_M0P, &frame->out_flag))
		&& (!test_bit(ENTRY_M1P, &frame->out_flag))
		&& (!test_bit(ENTRY_M2P, &frame->out_flag))
		&& (!test_bit(ENTRY_M3P, &frame->out_flag))
		&& (!test_bit(ENTRY_M4P, &frame->out_flag))
		&& (!test_bit(ENTRY_M5P, &frame->out_flag)))
		set_bit(hw_ip->id, &frame->core_flag);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	param = &hw_ip->region[instance]->parameter;
	mcs_param = &param->mcs;

	head = hw_ip->group[frame->instance]->head;

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &head->state)) {
		if (!test_bit(HW_CONFIG, &hw_ip->state)
			&& !atomic_read(&hardware->streaming[hardware->sensor_position[instance]]))
			start_flag = true;
		else
			start_flag = false;
	} else {
		start_flag = true;
	}

	if (frame->type == SHOT_TYPE_INTERNAL) {
		msdbg_hw(2, "request not exist\n", instance, hw_ip);
		spin_lock_irqsave(&shared_output_slock, flag);
		goto config;
	}

#ifdef HW_BUG_WA_NO_CONTOLL_PER_FRAME
	/* S/W WA for Lhotse MCSC EVT0 HW BUG*/
	ret = down_interruptible(&hardware->smp_mcsc_hw_bug);
	if (ret)
		mserr_hw("smp_mcsc_hw_bug fail", instance, hw_ip);
	else
		flag_mcsc_hw_bug_lock = true;
#endif

	if (hardware->video_mode)
		hw_mcsc->djag_input_source = DEV_HW_MCSC0;
	else
		hw_mcsc->djag_input_source = DEV_HW_MCSC1;

	spin_lock_irqsave(&shared_output_slock, flag);

	fimc_is_hw_mcsc_update_param(hw_ip, mcs_param, instance);

	msdbg_hw(2, "[F:%d]shot [T:%d]\n", instance, hw_ip, frame->fcount, frame->type);

config:
	/* multi-buffer */
	hw_ip->cur_s_int = 0;
	hw_ip->cur_e_int = 0;
	if (frame->num_buffers)
		hw_ip->num_buffers = frame->num_buffers;
	if (frame->num_buffers > 1)
		hw_ip->mframe = frame;

	/* RDMA cfg */
	if (mcs_param->input.dma_cmd == DMA_INPUT_COMMAND_ENABLE
		&& cap->in_dma == MCSC_CAP_SUPPORT) {
		ret = fimc_is_hw_mcsc_rdma_cfg(hw_ip, frame);
		if (ret) {
			spin_unlock_irqrestore(&shared_output_slock, flag);
			mserr_hw("[F:%d]mcsc rdma_cfg failed\n",
				instance, hw_ip, frame->fcount);
			return ret;
		}
	}

	/* WDMA cfg */
	fimc_is_hw_mcsc_wdma_cfg(hw_ip, frame);

	/* setting for DS */
	if (cap->ds_vra == MCSC_CAP_SUPPORT) {
		ret_internal = fimc_is_hw_mcsc_update_dsvra_register(hw_ip, head, mcs_param, instance,
			frame->shot ? frame->shot->uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] : MCSC_PORT_NONE);
	}

	/* setting for TDNR */
	if (cap->tdnr == MCSC_CAP_SUPPORT)
		ret = fimc_is_hw_mcsc_update_tdnr_register(hw_ip, frame, param, start_flag);

	/* setting for YSUM */
	if (cap->ysum == MCSC_CAP_SUPPORT) {
		ret_internal = fimc_is_hw_mcsc_update_ysum_register(hw_ip, head, mcs_param, instance,
			frame->shot ? frame->shot->uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_YSUM] : MCSC_PORT_NONE);
		if (ret_internal) {
			msdbg_hw(2, "ysum cfg is failed\n", instance, hw_ip);
			fimc_is_scaler_set_ysum_enable(hw_ip->regs, false);
		}
	}

	if (hw_ip->id == DEV_HW_MCSC1 && first_shot_tag == 0)
		hw_ip->debug_info[0].time[7] = cpu_clock(raw_smp_processor_id());

	/* for set shadow register write start
	 * only group input is OTF && not async shot
	 */
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &head->state) && !start_flag)
		fimc_is_scaler_set_shadow_ctrl(hw_ip->regs, hw_ip->id, SHADOW_WRITE_FINISH);

	if (start_flag) {
		msdbg_hw(2, "[F:%d]start\n", instance, hw_ip, frame->fcount);
		fimc_is_scaler_start(hw_ip->regs, hw_ip->id);
		if (mcs_param->input.dma_cmd == DMA_INPUT_COMMAND_ENABLE && cap->in_dma == MCSC_CAP_SUPPORT)
			fimc_is_scaler_rdma_start(hw_ip->regs, hw_ip->id);
	}

	msdbg_hw(2, "shot: hw_mcsc_out_configured[0x%lx]\n", instance, hw_ip,
		hw_mcsc_out_configured);

	clear_bit(HW_MCSC_OUT_CLEARED_ALL, &hw_mcsc_out_configured);
	set_bit(HW_CONFIG, &hw_ip->state);
	spin_unlock_irqrestore(&shared_output_slock, flag);

	/* HACK: debug_info of MCSC0 is saved at MCSC1 shot timing for debugging information. */
	if (hw_ip->id == DEV_HW_MCSC1 && first_shot_tag == 0) {
		int hw_slot;
		struct fimc_is_hw_ip *hw_ip_mcsc0;

		first_shot_tag = 1;
		hw_slot = fimc_is_hw_slot_id(DEV_HW_MCSC0);
		hw_ip_mcsc0 = &hardware->hw_ip[hw_slot];
		print_hw_frame_count(hw_ip_mcsc0);
		info_hw("4[%llu], 5[%llu], 6[%llu], 7[%llu], \n",
			hw_ip->debug_info[0].time[4],
			hw_ip->debug_info[0].time[5],
			hw_ip->debug_info[0].time[6],
			hw_ip->debug_info[0].time[7]);
	}

	return ret;
}

static int fimc_is_hw_mcsc_set_param(struct fimc_is_hw_ip *hw_ip, struct is_region *region,
	u32 lindex, u32 hindex, u32 instance, ulong hw_map)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!region);

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -EINVAL;
	}

	hw_ip->region[instance] = region;
	hw_ip->lindex[instance] = lindex;
	hw_ip->hindex[instance] = hindex;

	/* To execute update_register function in hw_mcsc_shot(),
	 * the value of hw_mcsc->instance is set as default.
	 */
	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	hw_mcsc->instance = FIMC_IS_STREAM_COUNT;

	return ret;
}

void fimc_is_hw_mcsc_check_size(struct fimc_is_hw_ip *hw_ip, struct mcs_param *param,
	u32 instance, u32 output_id)
{
	struct param_mcs_input *input = &param->input;
	struct param_mcs_output *output = &param->output[output_id];

	minfo_hw("[OUT:%d]>>> hw_mcsc_check_size >>>\n", instance, output_id);
	info_hw("otf_input: format(%d),size(%dx%d)\n",
		input->otf_format, input->width, input->height);
	info_hw("dma_input: format(%d),crop_size(%dx%d)\n",
		input->dma_format, input->dma_crop_width, input->dma_crop_height);
	info_hw("output: otf_cmd(%d),dma_cmd(%d),format(%d),stride(y:%d,c:%d)\n",
		output->otf_cmd, output->dma_cmd, output->dma_format,
		output->dma_stride_y, output->dma_stride_c);
	info_hw("output: pos(%d,%d),crop%dx%d),size(%dx%d)\n",
		output->crop_offset_x, output->crop_offset_y,
		output->crop_width, output->crop_height,
		output->width, output->height);
	info_hw("[%d]<<< hw_mcsc_check_size <<<\n", instance);
}

int fimc_is_hw_mcsc_update_register(struct fimc_is_hw_ip *hw_ip,
	struct mcs_param *param, u32 output_id, u32 instance)
{
	int ret = 0;

	if (output_id == MCSC_OUTPUT_DS)
		return ret;

	hw_mcsc_check_size(hw_ip, param, instance, output_id);
	ret = fimc_is_hw_mcsc_poly_phase(hw_ip, &param->input,
			&param->output[output_id], output_id, instance);
	ret = fimc_is_hw_mcsc_post_chain(hw_ip, &param->input,
			&param->output[output_id], output_id, instance);
	ret = fimc_is_hw_mcsc_flip(hw_ip, &param->output[output_id], output_id, instance);
	ret = fimc_is_hw_mcsc_otf_output(hw_ip, &param->output[output_id], output_id, instance);
	ret = fimc_is_hw_mcsc_dma_output(hw_ip, &param->output[output_id], output_id, instance);
	ret = fimc_is_hw_mcsc_output_yuvrange(hw_ip, &param->output[output_id], output_id, instance);
	ret = fimc_is_hw_mcsc_hwfc_output(hw_ip, &param->output[output_id], output_id, instance);

	return ret;
}

int fimc_is_hw_mcsc_update_param(struct fimc_is_hw_ip *hw_ip,
	struct mcs_param *param, u32 instance)
{
	int i = 0;
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;
	u32 dma_output_ids = 0;
	u32 hwfc_output_ids = 0;
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!param);
	FIMC_BUG(!cap);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (hw_mcsc->instance != instance) {
		msdbg_hw(2, "update_param: hw_ip->instance(%d)\n",
			instance, hw_ip, hw_mcsc->instance);
		hw_mcsc->instance = instance;
	}

	ret |= fimc_is_hw_mcsc_otf_input(hw_ip, &param->input, instance);
	ret |= fimc_is_hw_mcsc_dma_input(hw_ip, &param->input, instance);

#ifdef ENABLE_DJAG_IN_MCSC
	if (cap->djag == MCSC_CAP_SUPPORT) {
		fimc_is_scaler_set_djag_input_source(hw_ip->regs,
			hw_mcsc->djag_input_source - DEV_HW_MCSC0);

		param->input.djag_out_width = 0;
		param->input.djag_out_height = 0;

		if (hw_mcsc->djag_input_source == hw_ip->id)
			fimc_is_hw_mcsc_update_djag_register(hw_ip, param, instance);	/* for DZoom */
	}
#endif

	for (i = MCSC_OUTPUT0; i < cap->max_output; i++) {
		ret |= fimc_is_hw_mcsc_update_register(hw_ip, param, i, instance);
		fimc_is_scaler_set_wdma_pri(hw_ip->regs, i, param->output[i].plane);	/* FIXME: */

		/* check the hwfc enable in all output */
		if (param->output[i].hwfc)
			hwfc_output_ids |= (1 << i);

		if (param->output[i].dma_cmd == DMA_OUTPUT_COMMAND_ENABLE)
			dma_output_ids |= (1 << i);
	}

	/* setting for hwfc */
	ret |= fimc_is_hw_mcsc_hwfc_mode(hw_ip, &param->input, hwfc_output_ids, dma_output_ids, instance);

	hw_mcsc->prev_hwfc_output_ids = hwfc_output_ids;

	if (ret)
		fimc_is_hw_mcsc_size_dump(hw_ip);

	return ret;
}

int fimc_is_hw_mcsc_reset(struct fimc_is_hw_ip *hw_ip)
{
	int ret = 0;
	u32 output_id;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);
	struct fimc_is_hardware *hardware;
	struct fimc_is_hw_ip *hw_ip0 = NULL, *hw_ip1 = NULL;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!cap);

	hardware = hw_ip->hardware;
	get_mcsc_hw_ip(hardware, &hw_ip0, &hw_ip1);

	if (cap->enable_shared_output) {
		if (hw_ip0 && test_bit(HW_RUN, &hw_ip0->state))
			return 0;

		if (hw_ip1 && test_bit(HW_RUN, &hw_ip1->state))
			return 0;
	}

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (hw_ip0) {
		sinfo_hw("hw_mcsc_reset: out_en[0x%lx]\n", hw_ip0, hw_mcsc->out_en);
		ret = fimc_is_scaler_sw_reset(hw_ip0->regs, hw_ip0->id, 0, 0);
		if (ret != 0) {
			serr_hw("sw reset fail", hw_ip0);
			return -ENODEV;
		}

		/* shadow ctrl register clear */
		fimc_is_scaler_clear_shadow_ctrl(hw_ip0->regs, hw_ip0->id);
	}

	if (hw_ip1) {
		sinfo_hw("hw_mcsc_reset: out_en[0x%lx]\n", hw_ip1, hw_mcsc->out_en);
		ret = fimc_is_scaler_sw_reset(hw_ip1->regs, hw_ip1->id, 0, 0);
		if (ret != 0) {
			serr_hw("sw reset fail", hw_ip1);
			return -ENODEV;
		}

		/* shadow ctrl register clear */
		fimc_is_scaler_clear_shadow_ctrl(hw_ip1->regs, hw_ip1->id);
	}

	for (output_id = MCSC_OUTPUT0; output_id < cap->max_output; output_id++) {
		if (cap->enable_shared_output == false || test_bit(output_id, &hw_mcsc_out_configured)) {
			sinfo_hw("[OUT:%d]set output clear\n", hw_ip, output_id);
			fimc_is_scaler_set_poly_scaler_enable(hw_ip->regs, hw_ip->id, output_id, 0);
			fimc_is_scaler_set_post_scaler_enable(hw_ip->regs, output_id, 0);
			fimc_is_scaler_set_otf_out_enable(hw_ip->regs, output_id, false);
			fimc_is_scaler_set_dma_out_enable(hw_ip->regs, output_id, false);

			fimc_is_scaler_set_wdma_pri(hw_ip->regs, output_id, 0);	/* FIXME: */
			fimc_is_scaler_set_wdma_axi_pri(hw_ip->regs);		/* FIXME: */
			fimc_is_scaler_set_wdma_sram_base(hw_ip->regs, output_id);
			clear_bit(output_id, &hw_mcsc_out_configured);
		}
	}
	fimc_is_scaler_set_wdma_sram_base(hw_ip->regs, MCSC_OUTPUT_SSB);	/* FIXME: */

	/* for tdnr */
	if (cap->tdnr == MCSC_CAP_SUPPORT)
		fimc_is_scaler_set_tdnr_wdma_sram_base(hw_ip->regs, TDNR_WEIGHT);

	set_bit(HW_MCSC_OUT_CLEARED_ALL, &hw_mcsc_out_configured);

	if (cap->in_otf == MCSC_CAP_SUPPORT) {
		if (hw_ip0)
			fimc_is_scaler_set_stop_req_post_en_ctrl(hw_ip0->regs, hw_ip0->id, 0);

		if (hw_ip1)
			fimc_is_scaler_set_stop_req_post_en_ctrl(hw_ip1->regs, hw_ip1->id, 0);
	}

	return ret;
}

int fimc_is_hw_mcsc_clear_interrupt(struct fimc_is_hw_ip *hw_ip)
{
	int ret = 0;

	FIMC_BUG(!hw_ip);

	sinfo_hw("hw_mcsc_clear_interrupt\n", hw_ip);

	fimc_is_scaler_clear_intr_all(hw_ip->regs, hw_ip->id);
	fimc_is_scaler_disable_intr(hw_ip->regs, hw_ip->id);
	fimc_is_scaler_mask_intr(hw_ip->regs, hw_ip->id, MCSC_INTR_MASK);

	return ret;
}

static int fimc_is_hw_mcsc_load_setfile(struct fimc_is_hw_ip *hw_ip, u32 instance, ulong hw_map)
{
	int ret = 0;
	struct fimc_is_hw_ip_setfile *setfile;
	struct hw_api_scaler_setfile *setfile_addr;
	enum exynos_sensor_position sensor_position;
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;
	int setfile_index = 0;

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -ESRCH;
	}

	if (!unlikely(hw_ip->priv_info)) {
		mserr_hw("priv_info is NULL", instance, hw_ip);
		return -EINVAL;
	}
	sensor_position = hw_ip->hardware->sensor_position[instance];
	setfile = &hw_ip->setfile[sensor_position];

	switch (setfile->version) {
	case SETFILE_V2:
		break;
	case SETFILE_V3:
		break;
	default:
		mserr_hw("invalid version (%d)", instance, hw_ip,
			setfile->version);
		return -EINVAL;
	}

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (setfile->table[0].size != sizeof(struct hw_api_scaler_setfile))
		mswarn_hw("tuneset size(%x) is not matched to setfile structure size(%lx)",
			instance, hw_ip, setfile->table[0].size,
			sizeof(struct hw_api_scaler_setfile));

	/* copy MCSC setfile set */
	setfile_addr = (struct hw_api_scaler_setfile *)setfile->table[0].addr;
	memcpy(hw_mcsc->setfile[sensor_position], setfile_addr,
		sizeof(struct hw_api_scaler_setfile) * setfile->using_count);

	/* check each setfile Magic numbers */
	for (setfile_index = 0; setfile_index < setfile->using_count; setfile_index++) {
		setfile_addr = &hw_mcsc->setfile[sensor_position][setfile_index];

		if (setfile_addr->setfile_version != MCSC_SETFILE_VERSION) {
			mserr_hw("setfile[%d] version(0x%x) is incorrect",
				instance, hw_ip, setfile_index,
				setfile_addr->setfile_version);
			return -EINVAL;
		}
	}

	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

static int fimc_is_hw_mcsc_apply_setfile(struct fimc_is_hw_ip *hw_ip, u32 scenario,
	u32 instance, ulong hw_map)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;
	struct fimc_is_hw_ip_setfile *setfile;
	enum exynos_sensor_position sensor_position;
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);
	u32 setfile_index = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!cap);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -ESRCH;
	}

	if (!unlikely(hw_ip->priv_info)) {
		mserr_hw("priv info is NULL", instance, hw_ip);
		return -EINVAL;
	}

	sensor_position = hw_ip->hardware->sensor_position[instance];
	setfile = &hw_ip->setfile[sensor_position];

	setfile_index = setfile->index[scenario];
	if (setfile_index >= setfile->using_count) {
		mserr_hw("setfile index is out-of-range, [%d:%d]",
				instance, hw_ip, scenario, setfile_index);
		return -EINVAL;
	}

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	hw_mcsc->applied_setfile[sensor_position] =
		&hw_mcsc->setfile[sensor_position][setfile_index];

	msinfo_hw("setfile (%d) scenario (%d)\n", instance, hw_ip,
		setfile_index, scenario);

	return ret;
}

static int fimc_is_hw_mcsc_delete_setfile(struct fimc_is_hw_ip *hw_ip, u32 instance,
	ulong hw_map)
{

	FIMC_BUG(!hw_ip);

	if (!test_bit_variables(hw_ip->id, &hw_map)) {
		msdbg_hw(2, "%s: hw_map(0x%lx)\n", instance, hw_ip, __func__, hw_map);
		return 0;
	}

	if (!test_bit(HW_TUNESET, &hw_ip->state)) {
		msdbg_hw(2, "setfile already deleted", instance, hw_ip);
		return 0;
	}

	clear_bit(HW_TUNESET, &hw_ip->state);

	return 0;
}

#define FRAME_TRANSITION_TIMEOUT 1024 /* us */
static struct fimc_is_frame *wait_frame_transition(struct fimc_is_framemgr *fm,
		enum fimc_is_frame_state s, ulong fcount)
{
	ulong flags;
	unsigned long timeout;
	struct fimc_is_frame *f;

	framemgr_e_barrier_common(fm, 0, flags);
	f = peek_frame(fm, s);
	framemgr_x_barrier_common(fm, 0, flags);

	timeout = jiffies + usecs_to_jiffies(FRAME_TRANSITION_TIMEOUT);
	while (!f) {
		udelay(FRAME_TRANSITION_TIMEOUT >> 4);

		framemgr_e_barrier_common(fm, 0, flags);
		f = peek_frame(fm, s);
		if (!f && time_after_eq(jiffies, timeout)) {
			/* try to transit a frame forcibly */
			f = find_frame(fm, s - 1, frame_fcount, (void *)fcount);
			if (f) {
				trans_frame(fm, f, s);
				warn_hw("%s: transit a frame forcibly", __func__);
			}
			framemgr_x_barrier_common(fm, 0, flags);
			break;
		}
		framemgr_x_barrier_common(fm, 0, flags);
	}

	return f;
}

void fimc_is_hw_mcsc_frame_done(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame,
	int done_type)
{
	int ret = 0;
	bool fdone_flag = false;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_hw_mcsc *hw_mcsc;
	u32 index;
	int instance = atomic_read(&hw_ip->instance);
	bool flag_get_meta = true;
#ifdef HW_BUG_WA_NO_CONTOLL_PER_FRAME
	struct fimc_is_hardware *hardware;
#endif
	ulong flags;
	struct fimc_is_group *head;

	FIMC_BUG_VOID(!hw_ip->priv_info);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (test_and_clear_bit(DSVRA_SET_DONE, &hw_mcsc->blk_set_ctrl[instance])) {
		fimc_is_scaler_set_dma_out_enable(hw_ip->regs, MCSC_OUTPUT_DS, false);
		fimc_is_scaler_set_ds_enable(hw_ip->regs, false);
	}
	if (test_and_clear_bit(YSUM_SET_DONE, &hw_mcsc->blk_set_ctrl[instance]))
		fimc_is_scaler_set_ysum_enable(hw_ip->regs, false);

#ifdef HW_BUG_WA_NO_CONTOLL_PER_FRAME
	hardware = hw_ip->hardware;

	if (flag_mcsc_hw_bug_lock) {
		flag_mcsc_hw_bug_lock = false;
		msdbg_hw(2, "mcsc_status = %x, %x\n", instance, hw_ip,
			fimc_is_scaler_get_idle_status(hw_ip->regs, DEV_HW_MCSC0),
			fimc_is_scaler_get_idle_status(hw_ip->regs, DEV_HW_MCSC1));
		up(&hardware->smp_mcsc_hw_bug);
	}
#endif

	switch (done_type) {
	case IS_SHOT_SUCCESS:
		framemgr = hw_ip->framemgr;

		framemgr_e_barrier_common(framemgr, 0, flags);
		frame = peek_frame(framemgr, FS_HW_WAIT_DONE);
		framemgr_x_barrier_common(framemgr, 0, flags);

		if (!frame) {
			head = GET_HEAD_GROUP_IN_DEVICE(FIMC_IS_DEVICE_ISCHAIN,
					hw_ip->group[instance]);
			/*
			 * If a MCSx is the head of groups, we are unable to do anything.
			 * But when the other group is the head of them for this instance,
			 * start busy-waiting to increase latency tolerance.
			 * [ISR relay to the DDK, the DDK's callback to the driver]
			 * If there is no available frame still, we try to transit
			 * a frame forcibly instead of a leader group.
			 */
			if (hw_ip->group[instance]->id != head->id) {
				frame = wait_frame_transition(framemgr, FS_HW_WAIT_DONE,
							atomic_read(&hw_ip->fcount));
				if (frame)
					mswarn_hw("[F:%d] meet a frame in %s finally",
							instance, hw_ip,
							atomic_read(&hw_ip->fcount),
							__stringify_1(FS_HW_WAIT_DONE));
			}

			if (!frame) {
				mserr_hw("[F:%d] no frame in %s",
						instance, hw_ip,
						atomic_read(&hw_ip->fcount),
						__stringify_1(FS_HW_WAIT_DONE));

				framemgr_e_barrier_common(framemgr, 0, flags);
				frame_manager_print_info_queues(framemgr);
				print_all_hw_frame_count(hw_ip->hardware);
				framemgr_x_barrier_common(framemgr, 0, flags);

				return;
			}
		}
		break;
	case IS_SHOT_UNPROCESSED:
	case IS_SHOT_LATE_FRAME:
	case IS_SHOT_OVERFLOW:
	case IS_SHOT_INVALID_FRAMENUMBER:
	case IS_SHOT_TIMEOUT:
		if (frame == NULL) {
			mserr_hw("[F:%d] frame(null) type(%d)", instance,
				hw_ip, atomic_read(&hw_ip->fcount), done_type);
			return;
		}
		break;
	default:
		mserr_hw("[F:%d] invalid done type(%d)\n", instance, hw_ip,
			atomic_read(&hw_ip->fcount), done_type);
		return;
	}

	msdbgs_hw(2, "frame done[F:%d][O:0x%lx]\n", instance, hw_ip,
		frame->fcount, frame->out_flag);

	if (test_bit(ENTRY_M0P, &frame->out_flag)) {
		ret = fimc_is_hardware_frame_done(hw_ip, frame,
			WORK_M0P_FDONE, ENTRY_M0P, done_type, flag_get_meta);
		fdone_flag = true;
		flag_get_meta = false;
		clear_bit(MCSC_OUTPUT0, &hw_mcsc_out_configured);
	}

	if (test_bit(ENTRY_M1P, &frame->out_flag)) {
		ret = fimc_is_hardware_frame_done(hw_ip, frame,
			WORK_M1P_FDONE, ENTRY_M1P, done_type, flag_get_meta);
		fdone_flag = true;
		flag_get_meta = false;
		clear_bit(MCSC_OUTPUT1, &hw_mcsc_out_configured);
	}

	if (test_bit(ENTRY_M2P, &frame->out_flag)) {
		ret = fimc_is_hardware_frame_done(hw_ip, frame,
			WORK_M2P_FDONE, ENTRY_M2P, done_type, flag_get_meta);
		fdone_flag = true;
		flag_get_meta = false;
		clear_bit(MCSC_OUTPUT2, &hw_mcsc_out_configured);
	}

	if (test_bit(ENTRY_M3P, &frame->out_flag)) {
		ret = fimc_is_hardware_frame_done(hw_ip, frame,
			WORK_M3P_FDONE, ENTRY_M3P, done_type, flag_get_meta);
		fdone_flag = true;
		flag_get_meta = false;
		clear_bit(MCSC_OUTPUT3, &hw_mcsc_out_configured);
	}

	if (test_bit(ENTRY_M4P, &frame->out_flag)) {
		ret = fimc_is_hardware_frame_done(hw_ip, frame,
			WORK_M4P_FDONE, ENTRY_M4P, done_type, flag_get_meta);
		fdone_flag = true;
		flag_get_meta = false;
		clear_bit(MCSC_OUTPUT4, &hw_mcsc_out_configured);
	}

	if (test_bit(ENTRY_M5P, &frame->out_flag)) {
		ret = fimc_is_hardware_frame_done(hw_ip, frame,
			WORK_M5P_FDONE, ENTRY_M5P, done_type, flag_get_meta);
		fdone_flag = true;
		flag_get_meta = false;
		clear_bit(MCSC_OUTPUT5, &hw_mcsc_out_configured);
	}

	if (fdone_flag) {
		index = hw_ip->debug_index[1];
		hw_ip->debug_info[index].cpuid[DEBUG_POINT_FRAME_DMA_END] = raw_smp_processor_id();
		hw_ip->debug_info[index].time[DEBUG_POINT_FRAME_DMA_END] = cpu_clock(raw_smp_processor_id());

		dbg_isr("[F:%d][S-E] %05llu us\n", hw_ip, atomic_read(&hw_ip->fcount),
			(hw_ip->debug_info[index].time[DEBUG_POINT_FRAME_DMA_END] -
			 hw_ip->debug_info[index].time[DEBUG_POINT_FRAME_START]) / 1000);

		atomic_inc(&hw_ip->count.dma);
	} else {
		index = hw_ip->debug_index[1];
		hw_ip->debug_info[index].cpuid[DEBUG_POINT_FRAME_END] = raw_smp_processor_id();
		hw_ip->debug_info[index].time[DEBUG_POINT_FRAME_END] = cpu_clock(raw_smp_processor_id());

		dbg_isr("[F:%d][S-E] %05llu us\n", hw_ip, atomic_read(&hw_ip->fcount),
			(hw_ip->debug_info[index].time[DEBUG_POINT_FRAME_END] -
			 hw_ip->debug_info[index].time[DEBUG_POINT_FRAME_START]) / 1000);

		fimc_is_hardware_frame_done(hw_ip, NULL, -1, FIMC_IS_HW_CORE_END,
				IS_SHOT_SUCCESS, flag_get_meta);
	}

	return;
}

static int fimc_is_hw_mcsc_frame_ndone(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame,
	u32 instance, enum ShotErrorType done_type)
{
	int ret = 0;

	fimc_is_hw_mcsc_frame_done(hw_ip, frame, done_type);

	if (test_bit_variables(hw_ip->id, &frame->core_flag))
		ret = fimc_is_hardware_frame_done(hw_ip, frame, -1, FIMC_IS_HW_CORE_END,
				done_type, false);

	return ret;
}

int fimc_is_hw_mcsc_otf_input(struct fimc_is_hw_ip *hw_ip, struct param_mcs_input *input,
	u32 instance)
{
	int ret = 0;
	u32 width, height;
	u32 format, bit_width;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!input);
	FIMC_BUG(!cap);

	/* can't support this function */
	if (cap->in_otf != MCSC_CAP_SUPPORT)
		return ret;

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	msdbg_hw(2, "otf_input_setting cmd(%d)\n", instance, hw_ip, input->otf_cmd);
	width  = input->width;
	height = input->height;
	format = input->otf_format;
	bit_width = input->otf_bitwidth;

	if (input->otf_cmd == OTF_INPUT_COMMAND_DISABLE)
		return ret;

	ret = fimc_is_hw_mcsc_check_format(HW_MCSC_OTF_INPUT,
		format, bit_width, width, height);
	if (ret) {
		mserr_hw("Invalid OTF Input format: fmt(%d),bit(%d),size(%dx%d)",
			instance, hw_ip, format, bit_width, width, height);
		return ret;
	}

	fimc_is_scaler_set_input_source(hw_ip->regs, hw_ip->id, !input->otf_cmd);
	fimc_is_scaler_set_input_img_size(hw_ip->regs, hw_ip->id, width, height);
	fimc_is_scaler_set_dither(hw_ip->regs, hw_ip->id, 0);

	return ret;
}

int fimc_is_hw_mcsc_dma_input(struct fimc_is_hw_ip *hw_ip, struct param_mcs_input *input,
	u32 instance)
{
	int ret = 0;
	u32 width, height;
	u32 format, bit_width, plane, order;
	u32 y_stride, uv_stride;
	u32 img_format;
	u32 y_2bit_stride = 0, uv_2bit_stride = 0;
	u32 img_10bit_type = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!input);
	FIMC_BUG(!cap);

	/* can't support this function */
	if (cap->in_dma != MCSC_CAP_SUPPORT)
		return ret;

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	msdbg_hw(2, "dma_input_setting cmd(%d)\n", instance, hw_ip, input->dma_cmd);
	width  = input->dma_crop_width;
	height = input->dma_crop_height;
	format = input->dma_format;
	bit_width = input->dma_bitwidth;
	plane = input->plane;
	order = input->dma_order;
	y_stride = input->dma_stride_y;
	uv_stride = input->dma_stride_c;

	if (input->dma_cmd == DMA_INPUT_COMMAND_DISABLE)
		return ret;

	ret = fimc_is_hw_mcsc_check_format(HW_MCSC_DMA_INPUT,
		format, bit_width, width, height);
	if (ret) {
		mserr_hw("Invalid DMA Input format: fmt(%d),bit(%d),size(%dx%d)",
			instance, hw_ip, format, bit_width, width, height);
		return ret;
	}

	fimc_is_scaler_set_input_source(hw_ip->regs, hw_ip->id, input->dma_cmd);
	fimc_is_scaler_set_input_img_size(hw_ip->regs, hw_ip->id, width, height);
	fimc_is_scaler_set_dither(hw_ip->regs, hw_ip->id, 0);

	fimc_is_scaler_set_rdma_stride(hw_ip->regs, y_stride, uv_stride);

	ret = fimc_is_hw_mcsc_adjust_input_img_fmt(format, plane, order, &img_format);
	if (ret < 0) {
		mswarn_hw("Invalid rdma image format", instance, hw_ip);
		img_format = hw_mcsc->in_img_format;
	} else {
		hw_mcsc->in_img_format = img_format;
	}

	fimc_is_scaler_set_rdma_size(hw_ip->regs, width, height);
	fimc_is_scaler_set_rdma_format(hw_ip->regs, img_format);

	if (bit_width == DMA_INPUT_BIT_WIDTH_16BIT)
		img_10bit_type = 2;
	else if (bit_width == DMA_INPUT_BIT_WIDTH_10BIT)
		if (plane == DMA_INPUT_PLANE_4)
			img_10bit_type = 1;
		else
			img_10bit_type = 3;
	else
		img_10bit_type = 0;
	fimc_is_scaler_set_rdma_10bit_type(hw_ip->regs, img_10bit_type);

	if (input->plane == DMA_OUTPUT_PLANE_4) {
		y_2bit_stride = ALIGN(width / 4, 16);
		uv_2bit_stride = ALIGN(width / 4, 16);
		fimc_is_scaler_set_rdma_2bit_stride(hw_ip->regs, y_2bit_stride, uv_2bit_stride);
	}

	return ret;
}


int fimc_is_hw_mcsc_poly_phase(struct fimc_is_hw_ip *hw_ip, struct param_mcs_input *input,
	struct param_mcs_output *output, u32 output_id, u32 instance)
{
	int ret = 0;
	u32 src_pos_x, src_pos_y, src_width, src_height;
	u32 poly_dst_width, poly_dst_height;
	u32 out_width, out_height;
	ulong temp_width, temp_height;
	u32 hratio, vratio;
	u32 input_id = 0;
	bool config = true;
	bool post_en = false;
	bool round_mode_en = true;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);
	enum exynos_sensor_position sensor_position;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!input);
	FIMC_BUG(!output);
	FIMC_BUG(!hw_ip->priv_info);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (!test_bit(output_id, &hw_mcsc->out_en))
		return ret;

	msdbg_hw(2, "[OUT:%d]poly_phase_setting cmd(O:%d,D:%d)\n",
		instance, hw_ip, output_id, output->otf_cmd, output->dma_cmd);

	input_id = fimc_is_scaler_get_scaler_path(hw_ip->regs, hw_ip->id, output_id);
	config = (input_id == hw_ip->id ? true: false);

	if (output->otf_cmd == OTF_OUTPUT_COMMAND_DISABLE
		&& output->dma_cmd == DMA_OUTPUT_COMMAND_DISABLE) {
		if (cap->enable_shared_output == false
			|| (config && !test_bit(output_id, &hw_mcsc_out_configured)))
			fimc_is_scaler_set_poly_scaler_enable(hw_ip->regs, hw_ip->id, output_id, 0);
		return ret;
	}

	fimc_is_scaler_set_poly_scaler_enable(hw_ip->regs, hw_ip->id, output_id, 1);

	src_pos_x = output->crop_offset_x;
	src_pos_y = output->crop_offset_y;
	src_width = output->crop_width;
	src_height = output->crop_height;

#ifdef ENABLE_DJAG_IN_MCSC
	if (input->djag_out_width) {
		/* re-sizing crop size for DJAG output image to poly-phase scaler */
		src_pos_x = ALIGN(CONVRES(src_pos_x, input->width, input->djag_out_width), 2);
		src_pos_y = ALIGN(CONVRES(src_pos_y, input->height, input->djag_out_height), 2);
		src_width = ALIGN(CONVRES(src_width, input->width, input->djag_out_width), 2);
		src_height = ALIGN(CONVRES(src_height, input->height, input->djag_out_height), 2);

		if (src_pos_x + src_width > input->djag_out_width) {
			warn_hw("%s: Out of input_crop width (djag_out_w: %d < (%d + %d))",
				__func__, input->djag_out_width, src_pos_x, src_width);
			src_pos_x = 0;
			src_width = input->djag_out_width;
		}

		if (src_pos_y + src_height > input->djag_out_height) {
			warn_hw("%s: Out of input_crop height (djag_out_h: %d < (%d + %d))",
				__func__, input->djag_out_height, src_pos_y, src_height);
			src_pos_y = 0;
			src_height = input->djag_out_height;
		}

		sdbg_hw(2, "crop size changed (%d, %d, %d, %d) -> (%d, %d, %d, %d) by DJAG\n", hw_ip,
			output->crop_offset_x, output->crop_offset_y, output->crop_width, output->crop_height,
			src_pos_x, src_pos_y, src_width, src_height);
	}
#endif

	out_width = output->width;
	out_height = output->height;

	fimc_is_scaler_set_poly_src_size(hw_ip->regs, output_id, src_pos_x, src_pos_y,
		src_width, src_height);

	if ((src_width <= (out_width * MCSC_POLY_RATIO_DOWN))
		&& (out_width <= (src_width * MCSC_POLY_RATIO_UP))) {
		poly_dst_width = out_width;
		post_en = false;
	} else if ((src_width <= (out_width * MCSC_POLY_RATIO_DOWN * MCSC_POST_RATIO_DOWN))
		&& ((out_width * MCSC_POLY_RATIO_DOWN) < src_width)) {
		poly_dst_width = MCSC_ROUND_UP(src_width / MCSC_POLY_RATIO_DOWN, 2);
		post_en = true;
	} else {
		mserr_hw("hw_mcsc_poly_phase: Unsupported W ratio, (%dx%d)->(%dx%d)\n",
			instance, hw_ip, src_width, src_height, out_width, out_height);
		poly_dst_width = MCSC_ROUND_UP(src_width / MCSC_POLY_RATIO_DOWN, 2);
		post_en = true;
	}

	if ((src_height <= (out_height * MCSC_POLY_RATIO_DOWN))
		&& (out_height <= (src_height * MCSC_POLY_RATIO_UP))) {
		poly_dst_height = out_height;
		post_en = false;
	} else if ((src_height <= (out_height * MCSC_POLY_RATIO_DOWN * MCSC_POST_RATIO_DOWN))
		&& ((out_height * MCSC_POLY_RATIO_DOWN) < src_height)) {
		poly_dst_height = (src_height / MCSC_POLY_RATIO_DOWN);
		post_en = true;
	} else {
		mserr_hw("hw_mcsc_poly_phase: Unsupported H ratio, (%dx%d)->(%dx%d)\n",
			instance, hw_ip, src_width, src_height, out_width, out_height);
		poly_dst_height = (src_height / MCSC_POLY_RATIO_DOWN);
		post_en = true;
	}
#if defined(MCSC_POST_WA)
	/* The post scaler guarantee the quality of image          */
	/*  in case the scaling ratio equals to multiple of x1/256 */
	if ((post_en && ((poly_dst_width << MCSC_POST_WA_SHIFT) % out_width))
		|| (post_en && ((poly_dst_height << MCSC_POST_WA_SHIFT) % out_height))) {
		u32 multiple_hratio = 1;
		u32 multiple_vratio = 1;
		u32 shift = 0;
		for (shift = 0; shift <= MCSC_POST_WA_SHIFT; shift++) {
			if (((out_width % (1 << (MCSC_POST_WA_SHIFT-shift))) == 0)
				&& (out_height % (1 << (MCSC_POST_WA_SHIFT-shift)) == 0)) {
				multiple_hratio = out_width  / (1 << (MCSC_POST_WA_SHIFT-shift));
				multiple_vratio = out_height / (1 << (MCSC_POST_WA_SHIFT-shift));
				break;
			}
		}
		msdbg_hw(2, "[OUT:%d]poly_phase: shift(%d), ratio(%d,%d), "
			"size(%dx%d) before calculation\n",
			instance, hw_ip, output_id, shift, multiple_hratio, multiple_hratio,
			poly_dst_width, poly_dst_height);
		poly_dst_width  = MCSC_ROUND_UP(poly_dst_width, multiple_hratio);
		poly_dst_height = MCSC_ROUND_UP(poly_dst_height, multiple_vratio);
		if (poly_dst_width % 2) {
			poly_dst_width  = poly_dst_width  + multiple_hratio;
			poly_dst_height = poly_dst_height + multiple_vratio;
		}
		msdbg_hw(2, "[OUT:%d]poly_phase: size(%dx%d) after  calculation\n",
			instance, hw_ip, output_id, poly_dst_width, poly_dst_height);
	}
#endif

	fimc_is_scaler_set_poly_dst_size(hw_ip->regs, output_id,
		poly_dst_width, poly_dst_height);

	temp_width  = (ulong)src_width;
	temp_height = (ulong)src_height;
	hratio = (u32)((temp_width  << MCSC_PRECISION) / poly_dst_width);
	vratio = (u32)((temp_height << MCSC_PRECISION) / poly_dst_height);

	sensor_position = hw_ip->hardware->sensor_position[instance];
	fimc_is_scaler_set_poly_scaling_ratio(hw_ip->regs, output_id, hratio, vratio);
	fimc_is_scaler_set_poly_scaler_coef(hw_ip->regs, output_id, hratio, vratio, sensor_position);
	fimc_is_scaler_set_poly_round_mode(hw_ip->regs, output_id, round_mode_en);

	return ret;
}

int fimc_is_hw_mcsc_post_chain(struct fimc_is_hw_ip *hw_ip, struct param_mcs_input *input,
	struct param_mcs_output *output, u32 output_id, u32 instance)
{
	int ret = 0;
	u32 img_width, img_height;
	u32 dst_width, dst_height;
	ulong temp_width, temp_height;
	u32 hratio, vratio;
	u32 input_id = 0;
	bool config = true;
	bool round_mode_en = true;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!input);
	FIMC_BUG(!output);
	FIMC_BUG(!hw_ip->priv_info);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (!test_bit(output_id, &hw_mcsc->out_en))
		return ret;

	msdbg_hw(2, "[OUT:%d]post_chain_setting cmd(O:%d,D:%d)\n",
		instance, hw_ip, output_id, output->otf_cmd, output->dma_cmd);

	input_id = fimc_is_scaler_get_scaler_path(hw_ip->regs, hw_ip->id, output_id);
	config = (input_id == hw_ip->id ? true: false);

	if (output->otf_cmd == OTF_OUTPUT_COMMAND_DISABLE
		&& output->dma_cmd == DMA_OUTPUT_COMMAND_DISABLE) {
		if (cap->enable_shared_output == false
			|| (config && !test_bit(output_id, &hw_mcsc_out_configured)))
			fimc_is_scaler_set_post_scaler_enable(hw_ip->regs, output_id, 0);
		return ret;
	}

	fimc_is_scaler_get_poly_dst_size(hw_ip->regs, output_id, &img_width, &img_height);

	dst_width = output->width;
	dst_height = output->height;

	/* if x1 ~ x1/4 scaling, post scaler bypassed */
	if ((img_width == dst_width) && (img_height == dst_height)) {
		fimc_is_scaler_set_post_scaler_enable(hw_ip->regs, output_id, 0);
	} else {
		fimc_is_scaler_set_post_scaler_enable(hw_ip->regs, output_id, 1);
	}

	fimc_is_scaler_set_post_img_size(hw_ip->regs, output_id, img_width, img_height);
	fimc_is_scaler_set_post_dst_size(hw_ip->regs, output_id, dst_width, dst_height);

	temp_width  = (ulong)img_width;
	temp_height = (ulong)img_height;
	hratio = (u32)((temp_width  << MCSC_PRECISION) / dst_width);
	vratio = (u32)((temp_height << MCSC_PRECISION) / dst_height);

	fimc_is_scaler_set_post_scaling_ratio(hw_ip->regs, output_id, hratio, vratio);
	fimc_is_scaler_set_post_scaler_coef(hw_ip->regs, output_id, hratio, vratio);
	fimc_is_scaler_set_post_round_mode(hw_ip->regs, output_id, round_mode_en);

	return ret;
}

int fimc_is_hw_mcsc_flip(struct fimc_is_hw_ip *hw_ip, struct param_mcs_output *output,
	u32 output_id, u32 instance)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!output);
	FIMC_BUG(!hw_ip->priv_info);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (!test_bit(output_id, &hw_mcsc->out_en))
		return ret;

	msdbg_hw(2, "[OUT:%d]flip_setting flip(%d),cmd(O:%d,D:%d)\n",
		instance, hw_ip, output_id, output->flip, output->otf_cmd, output->dma_cmd);

	if (output->dma_cmd == DMA_OUTPUT_COMMAND_DISABLE)
		return ret;

	fimc_is_scaler_set_flip_mode(hw_ip->regs, output_id, output->flip);

	return ret;
}

int fimc_is_hw_mcsc_otf_output(struct fimc_is_hw_ip *hw_ip, struct param_mcs_output *output,
	u32 output_id, u32 instance)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;
	u32 out_width, out_height;
	u32 format, bitwidth;
	u32 input_id = 0;
	bool config = true;
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!output);
	FIMC_BUG(!cap);
	FIMC_BUG(!hw_ip->priv_info);

	/* can't support this function */
	if (cap->out_otf[output_id] != MCSC_CAP_SUPPORT)
		return ret;

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (!test_bit(output_id, &hw_mcsc->out_en))
		return ret;

	msdbg_hw(2, "[OUT:%d]otf_output_setting cmd(O:%d,D:%d)\n",
		instance, hw_ip, output_id, output->otf_cmd, output->dma_cmd);

	input_id = fimc_is_scaler_get_scaler_path(hw_ip->regs, hw_ip->id, output_id);
	config = (input_id == hw_ip->id ? true: false);

	if (output->otf_cmd == OTF_OUTPUT_COMMAND_DISABLE) {
		if (cap->enable_shared_output == false
			|| (config && !test_bit(output_id, &hw_mcsc_out_configured)))
			fimc_is_scaler_set_otf_out_enable(hw_ip->regs, output_id, false);
		return ret;
	}

	out_width  = output->width;
	out_height = output->height;
	format     = output->otf_format;
	bitwidth  = output->otf_bitwidth;

	ret = fimc_is_hw_mcsc_check_format(HW_MCSC_OTF_OUTPUT,
		format, bitwidth, out_width, out_height);
	if (ret) {
		mserr_hw("[OUT:%d]Invalid MCSC OTF Output format: fmt(%d),bit(%d),size(%dx%d)",
			instance, hw_ip, output_id, format, bitwidth, out_width, out_height);
		return ret;
	}

	fimc_is_scaler_set_otf_out_enable(hw_ip->regs, output_id, true);
	fimc_is_scaler_set_otf_out_path(hw_ip->regs, output_id);

	return ret;
}

int fimc_is_hw_mcsc_dma_output(struct fimc_is_hw_ip *hw_ip, struct param_mcs_output *output,
	u32 output_id, u32 instance)
{
	int ret = 0;
	u32 out_width, out_height, scaled_width = 0, scaled_height = 0;
	u32 format, plane, order,bitwidth;
	u32 y_stride, uv_stride;
	u32 y_2bit_stride, uv_2bit_stride;
	u32 img_format;
	u32 input_id = 0;
	bool config = true;
	bool conv420_en = false;
	enum exynos_sensor_position sensor_position;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!output);
	FIMC_BUG(!cap);
	FIMC_BUG(!hw_ip->priv_info);

	/* can't support this function */
	if (cap->out_dma[output_id] != MCSC_CAP_SUPPORT)
		return ret;

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	sensor_position = hw_ip->hardware->sensor_position[instance];

	if (!test_bit(output_id, &hw_mcsc->out_en))
		return ret;

	msdbg_hw(2, "[OUT:%d]dma_output_setting cmd(O:%d,D:%d)\n",
		instance, hw_ip, output_id, output->otf_cmd, output->dma_cmd);

	input_id = fimc_is_scaler_get_scaler_path(hw_ip->regs, hw_ip->id, output_id);
	config = (input_id == hw_ip->id ? true: false);

	if (output->dma_cmd == DMA_OUTPUT_COMMAND_DISABLE) {
		if (cap->enable_shared_output == false
			|| (config && !test_bit(output_id, &hw_mcsc_out_configured)))
			fimc_is_scaler_set_dma_out_enable(hw_ip->regs, output_id, false);
		return ret;
	}

	out_width  = output->width;
	out_height = output->height;
	format     = output->dma_format;
	plane      = output->plane;
	order      = output->dma_order;
	bitwidth   = output->dma_bitwidth;
	y_stride   = output->dma_stride_y;
	uv_stride  = output->dma_stride_c;

	ret = fimc_is_hw_mcsc_check_format(HW_MCSC_DMA_OUTPUT,
		format, bitwidth, out_width, out_height);
	if (ret) {
		mserr_hw("[OUT:%d]Invalid MCSC DMA Output format: fmt(%d),bit(%d),size(%dx%d)",
			instance, hw_ip, output_id, format, bitwidth, out_width, out_height);
		return ret;
	}

	ret = fimc_is_hw_mcsc_adjust_output_img_fmt(format, plane, order,
			&img_format, &conv420_en);
	if (ret < 0) {
		mswarn_hw("Invalid dma image format", instance, hw_ip);
		img_format = hw_mcsc->out_img_format[output_id];
		conv420_en = hw_mcsc->conv420_en[output_id];
	} else {
		hw_mcsc->out_img_format[output_id] = img_format;
		hw_mcsc->conv420_en[output_id] = conv420_en;
	}

	fimc_is_scaler_set_wdma_format(hw_ip->regs, output_id, img_format);
	fimc_is_scaler_set_420_conversion(hw_ip->regs, output_id, 0, conv420_en);

	fimc_is_scaler_get_post_dst_size(hw_ip->regs, output_id, &scaled_width, &scaled_height);
	if ((scaled_width != 0) && (scaled_height != 0)) {
		if ((scaled_width != out_width) || (scaled_height != out_height)) {
			msdbg_hw(2, "Invalid output[%d] scaled size (%d/%d)(%d/%d)\n",
				instance, hw_ip, output_id, scaled_width, scaled_height,
				out_width, out_height);
			return -EINVAL;
		}
	}

	fimc_is_scaler_set_wdma_size(hw_ip->regs, output_id, out_width, out_height);

	fimc_is_scaler_set_wdma_stride(hw_ip->regs, output_id, y_stride, uv_stride);

	fimc_is_scaler_set_wdma_10bit_type(hw_ip->regs, output_id, format, bitwidth, sensor_position);

	if (output->plane == DMA_OUTPUT_PLANE_4) {
		y_2bit_stride = ALIGN(out_width / 4, 16);
		uv_2bit_stride = ALIGN(out_width / 4, 16);
		fimc_is_scaler_set_wdma_2bit_stride(hw_ip->regs, output_id, y_2bit_stride, uv_2bit_stride);
	}
	return ret;
}

int fimc_is_hw_mcsc_hwfc_mode(struct fimc_is_hw_ip *hw_ip, struct param_mcs_input *input,
	u32 hwfc_output_ids, u32 dma_output_ids, u32 instance)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);
	u32 input_id = 0, output_id;
	bool config = true;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!input);
	FIMC_BUG(!cap);
	FIMC_BUG(!hw_ip->priv_info);

	/* can't support this function */
	if (cap->hwfc != MCSC_CAP_SUPPORT)
		return ret;

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (cap->enable_shared_output && !hwfc_output_ids)
		return 0;

	/* skip hwfc mode when..
	 *  - one core share Preview, Reprocessing
	 *  - at Preview stream, DMA output port shared to previous reprocessing port
	 *  ex> at 1x3 scaler,
	 *     1. preview - output 0 used, reprocessing - output 1, 2 used
	 *        above output is not overlapped
	 *        at this case, when CAPTURE -> PREVIEW, preview shot should not set hwfc_mode
	 *        due to not set hwfc_mode off during JPEG is operating.
	 *     2. preview - output 0, 1 used (1: preview callback), reprocessing - output 1, 2 used
	 *        above output is overlapped "output 1"
	 *        at this case, when CAPTURE -> PREVIEW, preview shot shuold set hwfc_mode to "0"
	 *        for avoid operate at Preview stream.
	 */
	if (!hw_mcsc->rep_flag[instance] && !(hw_mcsc->prev_hwfc_output_ids & dma_output_ids))
		return ret;

	msdbg_hw(2, "hwfc_mode_setting output[0x%08X]\n", instance, hw_ip, hwfc_output_ids);

	for (output_id = MCSC_OUTPUT0; output_id < cap->max_output; output_id++) {
		input_id = fimc_is_scaler_get_scaler_path(hw_ip->regs, hw_ip->id, output_id);
		config = (input_id == hw_ip->id ? true : false);

		if ((config && (hwfc_output_ids & (1 << output_id)))
			|| (fimc_is_scaler_get_dma_out_enable(hw_ip->regs, output_id))) {
			msdbg_hw(2, "hwfc_mode_setting hwfc_output_ids(0x%x)\n",
				instance, hw_ip, hwfc_output_ids);
			fimc_is_scaler_set_hwfc_mode(hw_ip->regs, hwfc_output_ids);
			break;
		}
	}

	if (!config) {
		msdbg_hw(2, "hwfc_mode_setting skip\n", instance, hw_ip);
		return ret;
	}

	return ret;
}

int fimc_is_hw_mcsc_hwfc_output(struct fimc_is_hw_ip *hw_ip, struct param_mcs_output *output,
	u32 output_id, u32 instance)
{
	int ret = 0;
	u32 width, height, format, plane;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!output);
	FIMC_BUG(!cap);
	FIMC_BUG(!hw_ip->priv_info);

	/* can't support this function */
	if (cap->out_hwfc[output_id] != MCSC_CAP_SUPPORT)
		return ret;

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (!test_bit(output_id, &hw_mcsc->out_en))
		return ret;

	width  = output->width;
	height = output->height;
	format = output->dma_format;
	plane = output->plane;

	msdbg_hw(2, "hwfc_config_setting mode(%dx%d, %d, %d)\n", instance, hw_ip,
			width, height, format, plane);

	if (output->hwfc)
		fimc_is_scaler_set_hwfc_config(hw_ip->regs, output_id, format, plane,
			(output_id * 3), width, height);

	return ret;
}

void fimc_is_hw_bchs_range(void __iomem *base_addr, u32 output_id, int yuv_range)
{
#ifdef ENABLE_10BIT_MCSC
	if (yuv_range == SCALER_OUTPUT_YUV_RANGE_FULL) {
		/* Y range - [0:1024], U/V range - [0:1024] */
		fimc_is_scaler_set_b_c(base_addr, output_id, 0, 1024);
		fimc_is_scaler_set_h_s(base_addr, output_id, 1024, 0, 0, 1024);
	} else {	/* YUV_RANGE_NARROW */
		/* Y range - [64:940], U/V range - [64:960] */
		fimc_is_scaler_set_b_c(base_addr, output_id, 0, 1024);
		fimc_is_scaler_set_h_s(base_addr, output_id, 1024, 0, 0, 1024);
	}
#else
	if (yuv_range == SCALER_OUTPUT_YUV_RANGE_FULL) {
		/* Y range - [0:255], U/V range - [0:255] */
		fimc_is_scaler_set_b_c(base_addr, output_id, 0, 256);
		fimc_is_scaler_set_h_s(base_addr, output_id, 256, 0, 0, 256);
	} else {	/* YUV_RANGE_NARROW */
		/* Y range - [16:235], U/V range - [16:239] */
		fimc_is_scaler_set_b_c(base_addr, output_id, 16, 220);
		fimc_is_scaler_set_h_s(base_addr, output_id, 224, 0, 0, 224);
	}
#endif
}

void fimc_is_hw_bchs_clamp(void __iomem *base_addr, u32 output_id, int yuv_range)
{
#ifdef ENABLE_10BIT_MCSC
	if (yuv_range == SCALER_OUTPUT_YUV_RANGE_FULL)
		fimc_is_scaler_set_bchs_clamp(base_addr, output_id, 1023, 0, 1023, 0);
	else	/* YUV_RANGE_NARROW */
		fimc_is_scaler_set_bchs_clamp(base_addr, output_id, 940, 64, 960, 64);
#else
	if (yuv_range == SCALER_OUTPUT_YUV_RANGE_FULL)
		fimc_is_scaler_set_bchs_clamp(base_addr, output_id, 255, 0, 255, 0);
	else	/* YUV_RANGE_NARROW */
		fimc_is_scaler_set_bchs_clamp(base_addr, output_id, 235, 16, 240, 16);
#endif
}

int fimc_is_hw_mcsc_output_yuvrange(struct fimc_is_hw_ip *hw_ip, struct param_mcs_output *output,
	u32 output_id, u32 instance)
{
	int ret = 0;
	int yuv_range = 0;
	u32 input_id = 0;
	bool config = true;
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;
#if !defined(USE_YUV_RANGE_BY_ISP)
	scaler_setfile_contents contents;
#endif
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!output);
	FIMC_BUG(!hw_ip->priv_info);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (!test_bit(output_id, &hw_mcsc->out_en))
		return ret;

	input_id = fimc_is_scaler_get_scaler_path(hw_ip->regs, hw_ip->id, output_id);
	config = (input_id == hw_ip->id ? true: false);

	if (output->dma_cmd == DMA_OUTPUT_COMMAND_DISABLE) {
		if (cap->enable_shared_output == false
			|| (config && !test_bit(output_id, &hw_mcsc_out_configured)))
			fimc_is_scaler_set_bchs_enable(hw_ip->regs, output_id, 0);
		return ret;
	}

	yuv_range = output->yuv_range;
	hw_mcsc->yuv_range = yuv_range; /* save for ISP */

	fimc_is_scaler_set_bchs_enable(hw_ip->regs, output_id, 1);
#if !defined(USE_YUV_RANGE_BY_ISP)
	if (test_bit(HW_TUNESET, &hw_ip->state)) {
		/* set yuv range config value by scaler_param yuv_range mode */
		sensor_position = hw_ip->hardware->sensor_position[instance];
		contents = hw_mcsc->applied_setfile[sensor_position]->contents[yuv_range];
		fimc_is_scaler_set_b_c(hw_ip->regs, output_id,
			contents.y_offset, contents.y_gain);
		fimc_is_scaler_set_h_s(hw_ip->regs, output_id,
			contents.c_gain00, contents.c_gain01,
			contents.c_gain10, contents.c_gain11);
		msdbg_hw(2, "set YUV range(%d) by setfile parameter\n",
			instance, hw_ip, yuv_range);
		msdbg_hw(2, "[OUT:%d]output_yuv_setting: yuv_range(%d), cmd(O:%d,D:%d)\n",
			instance, hw_ip, output_id, yuv_range, output->otf_cmd, output->dma_cmd);
		dbg_hw(2, "[Y:offset(%d),gain(%d)][C:gain00(%d),01(%d),10(%d),11(%d)]\n",
			contents.y_offset, contents.y_gain,
			contents.c_gain00, contents.c_gain01,
			contents.c_gain10, contents.c_gain11);
	} else {
		fimc_is_hw_bchs_range(hw_ip->regs, output_id, yuv_range);
		msdbg_hw(2, "YUV range set default settings\n", instance, hw_ip);
	}
#else
	fimc_is_hw_bchs_range(hw_ip->regs, output_id, yuv_range);
	fimc_is_hw_bchs_clamp(hw_ip->regs, output_id, yuv_range);
#endif
	return ret;
}

int fimc_is_hw_mcsc_adjust_input_img_fmt(u32 format, u32 plane, u32 order, u32 *img_format)
{
	int ret = 0;

	switch (format) {
	case DMA_INPUT_FORMAT_YUV420:
		switch (plane) {
		case 2:
		case 4:
			switch (order) {
			case DMA_INPUT_ORDER_CbCr:
				*img_format = MCSC_YUV420_2P_UFIRST;
				break;
			case DMA_INPUT_ORDER_CrCb:
				* img_format = MCSC_YUV420_2P_VFIRST;
				break;
			default:
				err_hw("input order error - (%d/%d/%d)", format, order, plane);
				ret = -EINVAL;
				break;
			}
			break;
		case 3:
			*img_format = MCSC_YUV420_3P;
			break;
		default:
			err_hw("input plane error - (%d/%d/%d)", format, order, plane);
			ret = -EINVAL;
			break;
		}
		break;
	case DMA_INPUT_FORMAT_YUV422:
		switch (plane) {
		case 1:
			switch (order) {
			case DMA_INPUT_ORDER_CrYCbY:
				*img_format = MCSC_YUV422_1P_VYUY;
				break;
			case DMA_INPUT_ORDER_CbYCrY:
				*img_format = MCSC_YUV422_1P_UYVY;
				break;
			case DMA_INPUT_ORDER_YCrYCb:
				*img_format = MCSC_YUV422_1P_YVYU;
				break;
			case DMA_INPUT_ORDER_YCbYCr:
				*img_format = MCSC_YUV422_1P_YUYV;
				break;
			default:
				err_hw("img order error - (%d/%d/%d)", format, order, plane);
				ret = -EINVAL;
				break;
			}
			break;
		case 2:
		case 4:
			switch (order) {
			case DMA_INPUT_ORDER_CbCr:
				*img_format = MCSC_YUV422_2P_UFIRST;
				break;
			case DMA_INPUT_ORDER_CrCb:
				*img_format = MCSC_YUV422_2P_VFIRST;
				break;
			default:
				err_hw("img order error - (%d/%d/%d)", format, order, plane);
				ret = -EINVAL;
				break;
			}
			break;
		case 3:
			*img_format = MCSC_YUV422_3P;
			break;
		default:
			err_hw("img plane error - (%d/%d/%d)", format, order, plane);
			ret = -EINVAL;
			break;
		}
		break;
	default:
		err_hw("img format error - (%d/%d/%d)", format, order, plane);
		ret = -EINVAL;
		break;
	}

	return ret;
}


int fimc_is_hw_mcsc_adjust_output_img_fmt(u32 format, u32 plane, u32 order, u32 *img_format,
	bool *conv420_flag)
{
	int ret = 0;

	switch (format) {
	case DMA_OUTPUT_FORMAT_YUV420:
		if (conv420_flag)
			*conv420_flag = true;
		switch (plane) {
		case 2:
		case 4:
			switch (order) {
			case DMA_OUTPUT_ORDER_CbCr:
				*img_format = MCSC_YUV420_2P_UFIRST;
				break;
			case DMA_OUTPUT_ORDER_CrCb:
				* img_format = MCSC_YUV420_2P_VFIRST;
				break;
			default:
				err_hw("output order error - (%d/%d/%d)", format, order, plane);
				ret = -EINVAL;
				break;
			}
			break;
		case 3:
			*img_format = MCSC_YUV420_3P;
			break;
		default:
			err_hw("output plane error - (%d/%d/%d)", format, order, plane);
			ret = -EINVAL;
			break;
		}
		break;
	case DMA_OUTPUT_FORMAT_YUV422:
		if (conv420_flag)
			*conv420_flag = false;
		switch (plane) {
		case 1:
			switch (order) {
			case DMA_OUTPUT_ORDER_CrYCbY:
				*img_format = MCSC_YUV422_1P_VYUY;
				break;
			case DMA_OUTPUT_ORDER_CbYCrY:
				*img_format = MCSC_YUV422_1P_UYVY;
				break;
			case DMA_OUTPUT_ORDER_YCrYCb:
				*img_format = MCSC_YUV422_1P_YVYU;
				break;
			case DMA_OUTPUT_ORDER_YCbYCr:
				*img_format = MCSC_YUV422_1P_YUYV;
				break;
			default:
				err_hw("img order error - (%d/%d/%d)", format, order, plane);
				ret = -EINVAL;
				break;
			}
			break;
		case 2:
		case 4:
			switch (order) {
			case DMA_OUTPUT_ORDER_CbCr:
				*img_format = MCSC_YUV422_2P_UFIRST;
				break;
			case DMA_OUTPUT_ORDER_CrCb:
				*img_format = MCSC_YUV422_2P_VFIRST;
				break;
			default:
				err_hw("img order error - (%d/%d/%d)", format, order, plane);
				ret = -EINVAL;
				break;
			}
			break;
		case 3:
			*img_format = MCSC_YUV422_3P;
			break;
		default:
			err_hw("img plane error - (%d/%d/%d)", format, order, plane);
			ret = -EINVAL;
			break;
		}
		break;
	case DMA_OUTPUT_FORMAT_RGB:
		switch (order) {
		case DMA_OUTPUT_ORDER_ARGB:
			*img_format = MCSC_RGB_ARGB8888;
			break;
		case DMA_OUTPUT_ORDER_BGRA:
			*img_format = MCSC_RGB_BGRA8888;
			break;
		case DMA_OUTPUT_ORDER_RGBA:
			*img_format = MCSC_RGB_RGBA8888;
			break;
		case DMA_OUTPUT_ORDER_ABGR:
			*img_format = MCSC_RGB_ABGR8888;
			break;
		default:
			*img_format = MCSC_RGB_RGBA8888;
			break;
		}
		break;
	default:
		err_hw("img format error - (%d/%d/%d)", format, order, plane);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int fimc_is_hw_mcsc_check_format(enum mcsc_io_type type, u32 format, u32 bit_width,
	u32 width, u32 height)
{
	int ret = 0;

	switch (type) {
	case HW_MCSC_OTF_INPUT:
		/* check otf input */
		if (width < 16 || width > 8192) {
			ret = -EINVAL;
			err_hw("Invalid MCSC OTF Input width(%d)", width);
		}

		if (height < 16 || height > 8192) {
			ret = -EINVAL;
			err_hw("Invalid MCSC OTF Input height(%d)", height);
		}

		if (format != OTF_INPUT_FORMAT_YUV422) {
			ret = -EINVAL;
			err_hw("Invalid MCSC OTF Input format(%d)", format);
		}

		if (bit_width != OTF_INPUT_BIT_WIDTH_8BIT) {
			ret = -EINVAL;
			err_hw("Invalid MCSC OTF Input format(%d)", bit_width);
		}
		break;
	case HW_MCSC_OTF_OUTPUT:
		/* check otf output */
		if (width < 16 || width > 8192) {
			ret = -EINVAL;
			err_hw("Invalid MCSC OTF Output width(%d)", width);
		}

		if (height < 16 || height > 8192) {
			ret = -EINVAL;
			err_hw("Invalid MCSC OTF Output height(%d)", height);
		}

		if (format != OTF_OUTPUT_FORMAT_YUV422) {
			ret = -EINVAL;
			err_hw("Invalid MCSC OTF Output format(%d)", format);
		}

		if (bit_width != OTF_OUTPUT_BIT_WIDTH_8BIT) {
			ret = -EINVAL;
			err_hw("Invalid MCSC OTF Output format(%d)", bit_width);
		}
		break;
	case HW_MCSC_DMA_INPUT:
		/* check dma input */
		if (width < 16 || width > 8192) {
			ret = -EINVAL;
			err_hw("Invalid MCSC DMA Input width(%d)", width);
		}

		if (height < 16 || height > 8192) {
			ret = -EINVAL;
			err_hw("Invalid MCSC DMA Input height(%d)", height);
		}

		if (format != DMA_INPUT_FORMAT_YUV422) {
			ret = -EINVAL;
			err_hw("Invalid MCSC DMA Input format(%d)", format);
		}

		if (!(bit_width == DMA_INPUT_BIT_WIDTH_8BIT ||
			bit_width == DMA_INPUT_BIT_WIDTH_10BIT ||
			bit_width == DMA_INPUT_BIT_WIDTH_16BIT)) {
			ret = -EINVAL;
			err_hw("Invalid MCSC DMA Input format(%d)", bit_width);
		}
		break;
	case HW_MCSC_DMA_OUTPUT:
		/* check dma output */
		if (width < 16 || width > 8192) {
			ret = -EINVAL;
			err_hw("Invalid MCSC DMA Output width(%d)", width);
		}

		if (height < 16 || height > 8192) {
			ret = -EINVAL;
			err_hw("Invalid MCSC DMA Output height(%d)", height);
		}

		if (!(format == DMA_OUTPUT_FORMAT_YUV422 ||
			format == DMA_OUTPUT_FORMAT_YUV420)) {
			ret = -EINVAL;
			err_hw("Invalid MCSC DMA Output format(%d)", format);
		}

		if (!(bit_width == DMA_OUTPUT_BIT_WIDTH_8BIT ||
			bit_width == DMA_OUTPUT_BIT_WIDTH_10BIT ||
			bit_width == DMA_OUTPUT_BIT_WIDTH_16BIT)) {
			ret = -EINVAL;
			err_hw("Invalid MCSC DMA Output bit_width(%d)", bit_width);
		}
		break;
	default:
		err_hw("Invalid MCSC type(%d)", type);
		break;
	}

	return ret;
}

static void fimc_is_hw_mcsc_tdnr_init(struct fimc_is_hw_ip *hw_ip,
	struct mcs_param *mcs_param,
	u32 instance)
{
#ifdef ENABLE_DNR_IN_MCSC
	struct fimc_is_hw_mcsc *hw_mcsc;

	FIMC_BUG_VOID(!hw_ip->priv_info);
	FIMC_BUG_VOID(!mcs_param);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	hw_mcsc->tdnr_first = MCSC_DNR_USE_FIRST;
	hw_mcsc->tdnr_output = MCSC_DNR_OUTPUT_INDEX;
	hw_mcsc->tdnr_internal_buf = MCSC_DNR_USE_INTERNAL_BUF;
	hw_mcsc->cur_tdnr_mode = TDNR_MODE_BYPASS;

	/* tdnr path select */
	fimc_is_scaler_set_tdnr_first(hw_ip->regs, (u32)hw_mcsc->tdnr_first);

	/* tdnr internal buffer addr setting */
	if (hw_mcsc->tdnr_internal_buf && mcs_param->control.buffer_address) {
		hw_mcsc->dvaddr_tdnr[0] = mcs_param->control.buffer_address;
		hw_mcsc->dvaddr_tdnr[1] = hw_mcsc->dvaddr_tdnr[0] +
			ALIGN(MAX_MCSC_DNR_WIDTH * MAX_MCSC_DNR_HEIGHT * 2, 16);
	}

	/* tdnr dma init */
	fimc_is_scaler_clear_tdnr_rdma_addr(hw_ip->regs, TDNR_IMAGE);
	fimc_is_scaler_clear_tdnr_rdma_addr(hw_ip->regs, TDNR_WEIGHT);

	fimc_is_scaler_set_tdnr_wdma_enable(hw_ip->regs, TDNR_WEIGHT, false);
	fimc_is_scaler_clear_tdnr_wdma_addr(hw_ip->regs, TDNR_WEIGHT);

	/* tdnr config init */
	memcpy(&hw_mcsc->tdnr_cfgs, &init_tdnr_cfgs,
		sizeof(struct tdnr_configs));
#endif
}

static int fimc_is_hw_mcsc_check_tdnr_mode_pre(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_group *head,
	struct fimc_is_frame *frame,
	struct tpu_param *tpu_param,
	struct mcs_param *mcs_param,
	enum tdnr_mode cur_mode)
{
	enum tdnr_mode tdnr_mode;
	struct fimc_is_hw_mcsc *hw_mcsc;
	struct fimc_is_hw_mcsc_cap *cap = GET_MCSC_HW_CAP(hw_ip);
	u32 lindex, hindex;

	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!tpu_param);
	FIMC_BUG(!cap);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	/* bypass setting at below case
	 * 1. dnr_bypass parameter is true
	 * 2. internal shot
	 */

	if ((tpu_param->config.tdnr_bypass == true)
		|| (mcs_param->output[0].dma_cmd == DMA_OUTPUT_COMMAND_DISABLE)
		|| (frame->type == SHOT_TYPE_INTERNAL)) {
		tdnr_mode = TDNR_MODE_BYPASS;
	} else {
		lindex = frame->shot->ctl.vendor_entry.lowIndexParam;
		hindex = frame->shot->ctl.vendor_entry.highIndexParam;

	/* 2dnr setting at below case
	 * 1. bypass true -> false setting
	 * 2. head group shot count is "0"(first shot)
	 * 3. tdnr wdma size changed
	 * 4. tdnr wdma dma out disabled
	 * 5. setfile tuneset changed(TODO)
	 */
		if ((cur_mode == TDNR_MODE_BYPASS)
			|| (!atomic_read(&head->scount))
			|| (lindex & LOWBIT_OF((hw_mcsc->tdnr_output + PARAM_MCS_OUTPUT0)))
			|| (hindex & HIGHBIT_OF((hw_mcsc->tdnr_output + PARAM_MCS_OUTPUT0))))
			tdnr_mode = TDNR_MODE_2DNR;
		else
		/* set to 3DNR mode */
			tdnr_mode = TDNR_MODE_3DNR;
	}

	return tdnr_mode;
}

static void fimc_is_hw_mcsc_check_tdnr_mode_post(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_group *head,
	enum tdnr_mode cur_tdnr_mode,
	enum tdnr_mode *changed_tdnr_mode)
{
	if (cur_tdnr_mode == TDNR_MODE_BYPASS)
		return;

	/* at FULL - OTF scenario, if register update is not finished
	 * until frame end interrupt occured,
	 * 3DNR RDMA transaction timing can be delayed.
	 * so, It should set to 2DNR mode for not operate RDMA
	 */
	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &head->state)
		&& atomic_read(&hw_ip->status.Vvalid) == V_BLANK) {
		*changed_tdnr_mode = TDNR_MODE_2DNR;
		warn_hw("[ID:%d] TDNR mode changed(%d->%d) due to not finished update",
			hw_ip->id, cur_tdnr_mode, *changed_tdnr_mode);
	}

	/* if first BYPASS or changed bypass mode,
	 * clear TDNR dma addr & disable dma
	 */
	if (*changed_tdnr_mode == TDNR_MODE_BYPASS) {
		fimc_is_scaler_clear_tdnr_rdma_addr(hw_ip->regs, TDNR_IMAGE);
		fimc_is_scaler_clear_tdnr_rdma_addr(hw_ip->regs, TDNR_WEIGHT);

		fimc_is_scaler_set_tdnr_wdma_enable(hw_ip->regs, TDNR_WEIGHT, false);
		fimc_is_scaler_clear_tdnr_wdma_addr(hw_ip->regs, TDNR_WEIGHT);
	}
}

static int fimc_is_hw_mcsc_cfg_tdnr_rdma(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_group *head,
	struct fimc_is_frame *frame,
	struct mcs_param *mcs_param,
	enum tdnr_mode tdnr_mode)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;
	u32 img_y_addr = 0, img_cb_addr = 0, img_cr_addr = 0;
	u32 img_width = 0, img_height = 0;
	u32 img_format = 0, img_y_stride = 0, img_uv_stride = 0;
	u32 weight_addr = 0;
	u32 weight_width = 0, weight_height = 0, weight_y_stride = 0;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		warn_hw("wrong TDNR setting at internal shot");
		return -EINVAL;
	}

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &head->state)) {
		/* buffer addr setting
		 * at OTF, RDMA[n+1] = WDMA[n]
		 *
		 * image buffer */
		if (tdnr_mode == TDNR_MODE_2DNR) {
			fimc_is_scaler_clear_tdnr_rdma_addr(hw_ip->regs, TDNR_IMAGE);
			fimc_is_scaler_clear_tdnr_rdma_addr(hw_ip->regs, TDNR_WEIGHT);

			return ret;
		}

		fimc_is_scaler_get_wdma_addr(hw_ip->regs, hw_mcsc->tdnr_output,
			&img_y_addr, &img_cb_addr, &img_cr_addr, USE_DMA_BUFFER_INDEX);
		if (img_y_addr == 0 && img_cb_addr == 0 && img_cr_addr == 0) {
			err_hw("TDNR image buffer address is NULL");
			return -EINVAL;
		}

		fimc_is_scaler_set_tdnr_rdma_addr(hw_ip->regs, TDNR_IMAGE,
			img_y_addr, img_cb_addr, img_cr_addr);

		/* weight buffer */
		fimc_is_scaler_get_tdnr_wdma_addr(hw_ip->regs, TDNR_WEIGHT,
			&weight_addr, NULL, NULL);
		if (weight_addr == 0) {
			err_hw("TDNR weight buffer address is NULL");
			return -EINVAL;
		}

		fimc_is_scaler_set_tdnr_rdma_addr(hw_ip->regs, TDNR_WEIGHT,
			weight_addr, 0, 0);

		/* skip below setting at same 3DNR mode */
		if (hw_mcsc->cur_tdnr_mode == TDNR_MODE_3DNR && tdnr_mode == TDNR_MODE_3DNR)
			return ret;

		/* at OTF, RDMA[n+1] data should be same to WDMA[n] */
		img_width = mcs_param->output[hw_mcsc->tdnr_output].width;
		img_height = mcs_param->output[hw_mcsc->tdnr_output].height;
		img_y_stride = mcs_param->output[hw_mcsc->tdnr_output].dma_stride_y;
		img_uv_stride = mcs_param->output[hw_mcsc->tdnr_output].dma_stride_c;

		if (img_width == 0 || img_height == 0
			|| img_y_stride == 0 || img_uv_stride == 0) {
			warn_hw("[ID:%d] MC-SC output[%d] size(%d x %d, y:%d, uv: %d)is incorrect",
				hw_ip->id, hw_mcsc->tdnr_output,
				img_width, img_height, img_y_stride, img_uv_stride);
			return -EINVAL;
		}

		fimc_is_scaler_set_tdnr_rdma_size(hw_ip->regs,
				TDNR_IMAGE, img_width, img_height);

		fimc_is_scaler_set_tdnr_rdma_stride(hw_ip->regs,
				TDNR_IMAGE, img_y_stride, img_uv_stride);

		fimc_is_scaler_get_tdnr_wdma_size(hw_ip->regs,
				TDNR_WEIGHT, &weight_width, &weight_height);
		fimc_is_scaler_set_tdnr_rdma_size(hw_ip->regs,
				TDNR_WEIGHT, weight_width, weight_height);

		fimc_is_scaler_get_tdnr_wdma_stride(hw_ip->regs,
				TDNR_WEIGHT, &weight_y_stride, NULL);
		fimc_is_scaler_set_tdnr_rdma_stride(hw_ip->regs,
				TDNR_WEIGHT, weight_y_stride, 0);

		fimc_is_scaler_get_wdma_format(hw_ip->regs,
				hw_mcsc->tdnr_output, &img_format);
		fimc_is_scaler_set_tdnr_rdma_format(hw_ip->regs,
				TDNR_IMAGE, img_format);
	} else {
		/* TODO: head group DMA input setting */
	}

	return ret;
}

static int fimc_is_hw_mcsc_cfg_tdnr_wdma(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_group *head,
	struct fimc_is_frame *frame,
	struct mcs_param *mcs_param,
	enum tdnr_mode tdnr_mode)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;
	u32 img_width = 0, img_height = 0;
	u32 img_y_stride = 0, img_uv_stride = 0;
	u32 weight_width = 0, weight_y_stride = 0;
	u32 cur_weight_addr = 0, weight_addr = 0;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		warn_hw("wrong TDNR setting at internal shot");
		return -EINVAL;
	}

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	if (test_bit(FIMC_IS_GROUP_OTF_INPUT, &head->state)) {
		/* weight buffer setting
		 * if 2DNR mode, set first buffer,
		 * else 3DNR mode, set remained buffer(buffer swap)
		 */
		if (tdnr_mode == TDNR_MODE_2DNR) {
			weight_addr = hw_mcsc->dvaddr_tdnr[0];
		} else if (tdnr_mode == TDNR_MODE_3DNR) {
			fimc_is_scaler_get_tdnr_wdma_addr(hw_ip->regs, TDNR_WEIGHT,
				&cur_weight_addr, NULL, NULL);
			weight_addr = (cur_weight_addr == hw_mcsc->dvaddr_tdnr[0]) ?
				hw_mcsc->dvaddr_tdnr[1] : hw_mcsc->dvaddr_tdnr[0];
		}

		fimc_is_scaler_set_tdnr_wdma_addr(hw_ip->regs, TDNR_WEIGHT,
			weight_addr, 0, 0);
		fimc_is_scaler_set_tdnr_wdma_enable(hw_ip->regs, TDNR_WEIGHT, true);

		/* skip below setting at same 3DNR mode */
		if (hw_mcsc->cur_tdnr_mode == TDNR_MODE_3DNR && tdnr_mode == TDNR_MODE_3DNR)
			return ret;

		/* at OTF, RDMA[n+1] data should be same to WDMA[n] */
		img_width = mcs_param->output[hw_mcsc->tdnr_output].width;
		img_height = mcs_param->output[hw_mcsc->tdnr_output].height;
		img_y_stride = mcs_param->output[hw_mcsc->tdnr_output].dma_stride_y;
		img_uv_stride = mcs_param->output[hw_mcsc->tdnr_output].dma_stride_c;

		if (img_width == 0 || img_height == 0
			|| img_y_stride == 0 || img_uv_stride == 0) {
			warn_hw("[ID:%d] MC-SC output[%d] size(%d x %d, y:%d, uv: %d)is incorrect",
				hw_ip->id, hw_mcsc->tdnr_output,
				img_width, img_height, img_y_stride, img_uv_stride);
			return -EINVAL;
		}

		weight_width = ((int)((img_width + 15) / 16)) * 2;
		fimc_is_scaler_set_tdnr_wdma_size(hw_ip->regs,
			TDNR_WEIGHT, weight_width, img_height);

		weight_y_stride = ALIGN(weight_width * 2, 64);
		fimc_is_scaler_set_tdnr_wdma_stride(hw_ip->regs,
			TDNR_WEIGHT, weight_y_stride, 0);
	} else {
		/* TODO: head group DMA input setting */
	}

	return ret;
}

/* "TDNR translate function"
 * re_configure interpolated NI depended factor to SFR configurations
 */
static void translate_temporal_factor(struct temporal_ni_dep_config *temporal_cfg,
	struct ni_dep_factors interpolated_factor)
{
	ulong temp_val = 0;

	temporal_cfg->auto_lut_gains_y[ARR3_VAL1] =
		RESTORE_SHIFT_VALUE(interpolated_factor.temporal_motion_detection_luma_low);
	temporal_cfg->auto_lut_gains_y[ARR3_VAL2] =
		RESTORE_SHIFT_VALUE(interpolated_factor.temporal_motion_detection_luma_contrast);
	temporal_cfg->auto_lut_gains_y[ARR3_VAL3] =
		RESTORE_SHIFT_VALUE(interpolated_factor.temporal_motion_detection_luma_high);

	temporal_cfg->auto_lut_gains_uv[ARR3_VAL1] =
		RESTORE_SHIFT_VALUE(interpolated_factor.temporal_motion_detection_chroma_low);
	temporal_cfg->auto_lut_gains_uv[ARR3_VAL2] =
		RESTORE_SHIFT_VALUE(interpolated_factor.temporal_motion_detection_chroma_contrast);
	temporal_cfg->auto_lut_gains_uv[ARR3_VAL3] =
		RESTORE_SHIFT_VALUE(interpolated_factor.temporal_motion_detection_chroma_high);

	temporal_cfg->y_offset =
		interpolated_factor.temporal_motion_detection_luma_off ? 255 : 0;
	temporal_cfg->uv_offset =
		interpolated_factor.temporal_motion_detection_luma_off ? 255 : 0;

	/* value = 16 * (1 - source / 256) */
	temp_val = (1 << (8 + INTERPOLATE_SHIFT))
		- (ulong)interpolated_factor.temporal_weight_luma_power_base;
	temporal_cfg->temporal_weight_coeff_y1 = (u32)(temp_val >> (4 + INTERPOLATE_SHIFT));
	/* value = 16 * (1 - (source1 / 256) * (source2 / 256)) */
	temp_val = interpolated_factor.temporal_weight_luma_power_base *
			interpolated_factor.temporal_weight_luma_power_gamma;
	temporal_cfg->temporal_weight_coeff_y2 = 16 -
			(u32)(temp_val >> (12 + INTERPOLATE_SHIFT * 2));

	/* value = 16 * (1 - source / 256) */
	temp_val = (1 << (8 + INTERPOLATE_SHIFT))
		- (ulong)interpolated_factor.temporal_weight_chroma_power_base;
	temporal_cfg->temporal_weight_coeff_uv1 = (u32)(temp_val >> (4 + INTERPOLATE_SHIFT));
	/* value = 16 * (1 - (source1 / 256) * (source2 / 256)) */
	temp_val = interpolated_factor.temporal_weight_chroma_power_base *
			interpolated_factor.temporal_weight_chroma_power_gamma;
	temporal_cfg->temporal_weight_coeff_uv2 = 16 -
			(u32)(temp_val >> (12 + INTERPOLATE_SHIFT * 2));
}

static void translate_regional_factor(struct regional_ni_dep_config *regional_cfg,
	struct ni_dep_factors interpolated_factor)
{
	regional_cfg->is_region_diff_on = interpolated_factor.temporal_weight_hot_region;
	regional_cfg->region_gain =
		RESTORE_SHIFT_VALUE(interpolated_factor.temporal_weight_hot_region_power);
	regional_cfg->other_channels_check =
		interpolated_factor.temporal_weight_chroma_threshold;
	regional_cfg->other_channel_gain =
		RESTORE_SHIFT_VALUE(interpolated_factor.temporal_weight_chroma_power);
}

static void translate_spatial_factor(struct spatial_ni_dep_config *spatial_cfg,
	struct ni_dep_factors interpolated_factor)
{
	ulong temp_val = 0;

	/* value = 16 * (1 - source / 256) */
	temp_val = (1 << (8 + INTERPOLATE_SHIFT))
		- (ulong)interpolated_factor.spatial_power;
	spatial_cfg->spatial_gain = (u32)(temp_val >> (4 + INTERPOLATE_SHIFT));
	spatial_cfg->weight_mode = interpolated_factor.spatial_weight_mode;
	spatial_cfg->spatial_separate_weights =
		interpolated_factor.spatial_separate_weighting;

	spatial_cfg->spatial_luma_gain[ARR4_VAL1] =
		RESTORE_SHIFT_VALUE(interpolated_factor.spatial_pd_luma_slope +
			interpolated_factor.spatial_pd_luma_offset);
	spatial_cfg->spatial_uv_gain[ARR4_VAL1] =
		RESTORE_SHIFT_VALUE(interpolated_factor.spatial_pd_chroma_slope +
			interpolated_factor.spatial_pd_chroma_offset);

	spatial_cfg->spatial_luma_gain[ARR4_VAL2] =
		RESTORE_SHIFT_VALUE(2 * interpolated_factor.spatial_pd_luma_slope +
			interpolated_factor.spatial_pd_luma_offset);
	spatial_cfg->spatial_uv_gain[ARR4_VAL2] =
		RESTORE_SHIFT_VALUE(2 * interpolated_factor.spatial_pd_chroma_slope +
			interpolated_factor.spatial_pd_chroma_offset);

	spatial_cfg->spatial_luma_gain[ARR4_VAL3] =
		RESTORE_SHIFT_VALUE(3 * interpolated_factor.spatial_pd_luma_slope +
			interpolated_factor.spatial_pd_luma_offset);
	spatial_cfg->spatial_uv_gain[ARR4_VAL3] =
		RESTORE_SHIFT_VALUE(3 * interpolated_factor.spatial_pd_chroma_slope +
			interpolated_factor.spatial_pd_chroma_offset);

	spatial_cfg->spatial_luma_gain[ARR4_VAL4] =
		RESTORE_SHIFT_VALUE(4 * interpolated_factor.spatial_pd_luma_slope +
			interpolated_factor.spatial_pd_luma_offset);
	spatial_cfg->spatial_uv_gain[ARR4_VAL4] =
		RESTORE_SHIFT_VALUE(4 * interpolated_factor.spatial_pd_chroma_slope +
			interpolated_factor.spatial_pd_chroma_offset);
}

static void translate_yuv_table_factor(struct yuv_table_config *yuv_table_cfg,
	struct ni_dep_factors interpolated_factor)
{
	int arr_idx;

	for (arr_idx = ARR3_VAL1; arr_idx < ARR3_MAX; arr_idx++) {
		yuv_table_cfg->x_grid_y[arr_idx] =
			RESTORE_SHIFT_VALUE(interpolated_factor.yuv_tables.x_grid_y[arr_idx]);
		yuv_table_cfg->x_grid_u[arr_idx] =
			RESTORE_SHIFT_VALUE(interpolated_factor.yuv_tables.x_grid_u[arr_idx]);
		yuv_table_cfg->x_grid_v[arr_idx] =
			RESTORE_SHIFT_VALUE(interpolated_factor.yuv_tables.x_grid_v[arr_idx]);
	}

	for (arr_idx = ARR4_VAL1; arr_idx < ARR4_MAX; arr_idx++) {
		yuv_table_cfg->y_std_slope[arr_idx] =
			RESTORE_SHIFT_VALUE(interpolated_factor.yuv_tables.y_std_slope[arr_idx]);
		yuv_table_cfg->u_std_slope[arr_idx] =
			RESTORE_SHIFT_VALUE(interpolated_factor.yuv_tables.u_std_slope[arr_idx]);
		yuv_table_cfg->v_std_slope[arr_idx] =
			RESTORE_SHIFT_VALUE(interpolated_factor.yuv_tables.v_std_slope[arr_idx]);
	}

	yuv_table_cfg->y_std_offset =
		RESTORE_SHIFT_VALUE(interpolated_factor.yuv_tables.y_std_offset);

	yuv_table_cfg->u_std_offset =
		RESTORE_SHIFT_VALUE(interpolated_factor.yuv_tables.u_std_offset);

	yuv_table_cfg->v_std_offset =
		RESTORE_SHIFT_VALUE(interpolated_factor.yuv_tables.v_std_offset);
}

/* TDNR interpolation function
 * NI depended value is get by reference NI value's linear interpolation
 */
static void interpolate_temporal_factor(struct ni_dep_factors *interpolated_factor,
	u32 noise_index,
	struct ni_dep_factors bottom_ni_factor,
	struct ni_dep_factors top_ni_factor)
{
	int bottom_ni = MULTIPLIED_NI((int)bottom_ni_factor.noise_index);
	int top_ni = MULTIPLIED_NI((int)top_ni_factor.noise_index);
	int diff_ni_top_to_bottom = top_ni - bottom_ni;
	int diff_ni_top_to_actual = top_ni - noise_index;
	int diff_ni_actual_to_bottom = noise_index - bottom_ni;

	interpolated_factor->temporal_motion_detection_luma_low =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.temporal_motion_detection_luma_low,
			top_ni_factor.temporal_motion_detection_luma_low,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->temporal_motion_detection_luma_contrast =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.temporal_motion_detection_luma_contrast,
			top_ni_factor.temporal_motion_detection_luma_contrast,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->temporal_motion_detection_luma_high =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.temporal_motion_detection_luma_high,
			top_ni_factor.temporal_motion_detection_luma_high,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->temporal_motion_detection_chroma_low =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.temporal_motion_detection_chroma_low,
			top_ni_factor.temporal_motion_detection_chroma_low,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->temporal_motion_detection_chroma_contrast =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.temporal_motion_detection_chroma_contrast,
			top_ni_factor.temporal_motion_detection_chroma_contrast,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->temporal_motion_detection_chroma_high =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.temporal_motion_detection_chroma_high,
			top_ni_factor.temporal_motion_detection_chroma_high,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->temporal_weight_luma_power_base =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.temporal_weight_luma_power_base,
			top_ni_factor.temporal_weight_luma_power_base,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->temporal_weight_luma_power_gamma =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.temporal_weight_luma_power_gamma,
			top_ni_factor.temporal_weight_luma_power_gamma,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->temporal_weight_chroma_power_base =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.temporal_weight_chroma_power_base,
			top_ni_factor.temporal_weight_chroma_power_base,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->temporal_weight_chroma_power_gamma =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.temporal_weight_chroma_power_gamma,
			top_ni_factor.temporal_weight_chroma_power_gamma,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	/* select value by the actual NI and ref NI range
	 * 1. Bottom NI -- Actual NI -------Top NI -> use Bottom NI factor
	 * 2. Bottom NI ------- Actual NI -- Top NI -> use Top NI factor
	 */

	if (diff_ni_top_to_actual > diff_ni_actual_to_bottom) {
		interpolated_factor->temporal_motion_detection_luma_off =
			top_ni_factor.temporal_motion_detection_luma_off;
		interpolated_factor->temporal_motion_detection_chroma_off =
			top_ni_factor.temporal_motion_detection_chroma_off;
	} else {
		interpolated_factor->temporal_motion_detection_luma_off =
			bottom_ni_factor.temporal_motion_detection_luma_off;
		interpolated_factor->temporal_motion_detection_chroma_off =
			bottom_ni_factor.temporal_motion_detection_chroma_off;
	}
}

static void interpolate_regional_factor(struct ni_dep_factors *interpolated_factor,
	u32 noise_index,
	struct ni_dep_factors bottom_ni_factor,
	struct ni_dep_factors top_ni_factor)
{
	int bottom_ni = MULTIPLIED_NI((int)bottom_ni_factor.noise_index);
	int top_ni = MULTIPLIED_NI((int)top_ni_factor.noise_index);
	int diff_ni_top_to_bottom = top_ni - bottom_ni;
	int diff_ni_top_to_actual = top_ni - noise_index;
	int diff_ni_actual_to_bottom = noise_index - bottom_ni;

	interpolated_factor->temporal_weight_hot_region_power =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.temporal_weight_hot_region_power,
			top_ni_factor.temporal_weight_hot_region_power,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->temporal_weight_chroma_power =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.temporal_weight_chroma_power,
			top_ni_factor.temporal_weight_chroma_power,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	/* select value by the actual NI and ref NI range
	 * 1. Bottom NI -- Actual NI -------Top NI -> use Bottom NI factor
	 * 2. Bottom NI ------- Actual NI -- Top NI -> use Top NI factor
	 */

	if (diff_ni_top_to_actual > diff_ni_actual_to_bottom) {
		interpolated_factor->temporal_weight_hot_region =
			top_ni_factor.temporal_weight_hot_region;
		interpolated_factor->temporal_weight_chroma_threshold =
			top_ni_factor.temporal_weight_chroma_threshold;
	} else {
		interpolated_factor->temporal_weight_hot_region =
			bottom_ni_factor.temporal_weight_hot_region;
		interpolated_factor->temporal_weight_chroma_threshold =
			bottom_ni_factor.temporal_weight_chroma_threshold;
	}
}

static void interpolate_spatial_factor(struct ni_dep_factors *interpolated_factor,
	u32 noise_index,
	struct ni_dep_factors bottom_ni_factor,
	struct ni_dep_factors top_ni_factor)
{
	int bottom_ni = MULTIPLIED_NI((int)bottom_ni_factor.noise_index);
	int top_ni = MULTIPLIED_NI((int)top_ni_factor.noise_index);
	int diff_ni_top_to_bottom = top_ni - bottom_ni;
	int diff_ni_top_to_actual = top_ni - noise_index;
	int diff_ni_actual_to_bottom = noise_index - bottom_ni;

	interpolated_factor->spatial_power =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.spatial_power,
			top_ni_factor.spatial_power,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->spatial_pd_luma_slope =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.spatial_pd_luma_slope,
			top_ni_factor.spatial_pd_luma_slope,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->spatial_pd_luma_offset =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.spatial_pd_luma_offset,
			top_ni_factor.spatial_pd_luma_offset,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->spatial_pd_chroma_slope =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.spatial_pd_chroma_slope,
			top_ni_factor.spatial_pd_chroma_slope,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->spatial_pd_chroma_offset =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.spatial_pd_chroma_offset,
			top_ni_factor.spatial_pd_chroma_offset,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	/* select value by the actual NI and ref NI range
	 * 1. Bottom NI -- Actual NI -------Top NI -> use Bottom NI factor
	 * 2. Bottom NI ------- Actual NI -- Top NI -> use Top NI factor
	 */

	if (diff_ni_top_to_actual > diff_ni_actual_to_bottom) {
		interpolated_factor->spatial_weight_mode =
			top_ni_factor.spatial_weight_mode;
		interpolated_factor->spatial_separate_weighting =
			top_ni_factor.spatial_separate_weighting;
	} else {
		interpolated_factor->spatial_weight_mode =
			bottom_ni_factor.spatial_weight_mode;
		interpolated_factor->spatial_separate_weighting =
			bottom_ni_factor.spatial_separate_weighting;
	}
}

static void interpolate_yuv_table_factor(struct ni_dep_factors *interpolated_factor,
	u32 noise_index,
	struct ni_dep_factors bottom_ni_factor,
	struct ni_dep_factors top_ni_factor)
{
	int arr_idx;
	int bottom_ni = MULTIPLIED_NI((int)bottom_ni_factor.noise_index);
	int top_ni = MULTIPLIED_NI((int)top_ni_factor.noise_index);
	int diff_ni_top_to_bottom = top_ni - bottom_ni;
	int diff_ni_actual_to_bottom = noise_index - bottom_ni;

	for (arr_idx = ARR3_VAL1; arr_idx < ARR3_MAX; arr_idx++) {
		interpolated_factor->yuv_tables.x_grid_y[arr_idx] =
			GET_LINEAR_INTERPOLATE_VALUE(
				bottom_ni_factor.yuv_tables.x_grid_y[arr_idx],
				top_ni_factor.yuv_tables.x_grid_y[arr_idx],
				diff_ni_top_to_bottom,
				diff_ni_actual_to_bottom);

		interpolated_factor->yuv_tables.x_grid_u[arr_idx] =
			GET_LINEAR_INTERPOLATE_VALUE(
				bottom_ni_factor.yuv_tables.x_grid_u[arr_idx],
				top_ni_factor.yuv_tables.x_grid_u[arr_idx],
				diff_ni_top_to_bottom,
				diff_ni_actual_to_bottom);

		interpolated_factor->yuv_tables.x_grid_v[arr_idx] =
			GET_LINEAR_INTERPOLATE_VALUE(
				bottom_ni_factor.yuv_tables.x_grid_v[arr_idx],
				top_ni_factor.yuv_tables.x_grid_v[arr_idx],
				diff_ni_top_to_bottom,
				diff_ni_actual_to_bottom);
	}

	for (arr_idx = ARR4_VAL1; arr_idx < ARR4_MAX; arr_idx++) {
		interpolated_factor->yuv_tables.y_std_slope[arr_idx] =
			GET_LINEAR_INTERPOLATE_VALUE(
				bottom_ni_factor.yuv_tables.y_std_slope[arr_idx],
				top_ni_factor.yuv_tables.y_std_slope[arr_idx],
				diff_ni_top_to_bottom,
				diff_ni_actual_to_bottom);

		interpolated_factor->yuv_tables.u_std_slope[arr_idx] =
			GET_LINEAR_INTERPOLATE_VALUE(
				bottom_ni_factor.yuv_tables.u_std_slope[arr_idx],
				top_ni_factor.yuv_tables.u_std_slope[arr_idx],
				diff_ni_top_to_bottom,
				diff_ni_actual_to_bottom);

		interpolated_factor->yuv_tables.v_std_slope[arr_idx] =
			GET_LINEAR_INTERPOLATE_VALUE(
				bottom_ni_factor.yuv_tables.v_std_slope[arr_idx],
				top_ni_factor.yuv_tables.v_std_slope[arr_idx],
				diff_ni_top_to_bottom,
				diff_ni_actual_to_bottom);
	}

	interpolated_factor->yuv_tables.y_std_offset =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.yuv_tables.y_std_offset,
			top_ni_factor.yuv_tables.y_std_offset,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->yuv_tables.u_std_offset =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.yuv_tables.u_std_offset,
			top_ni_factor.yuv_tables.u_std_offset,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);

	interpolated_factor->yuv_tables.v_std_offset =
		GET_LINEAR_INTERPOLATE_VALUE(
			bottom_ni_factor.yuv_tables.v_std_offset,
			top_ni_factor.yuv_tables.v_std_offset,
			diff_ni_top_to_bottom,
			diff_ni_actual_to_bottom);
}

static void reconfigure_ni_depended_tuneset(tdnr_setfile_contents *tdnr_tuneset,
	struct tdnr_configs *tdnr_cfgs,
	u32 noise_index,
	u32 bottom_ni_index,
	u32 top_ni_index)
{
	struct ni_dep_factors interpolated_factor;

	/* help variables for interpolation */
	struct ni_dep_factors bottom_ni_factor =
		tdnr_tuneset->ni_dep_factors[bottom_ni_index];
	struct ni_dep_factors top_ni_factor =
		tdnr_tuneset->ni_dep_factors[top_ni_index];

	if (bottom_ni_index == top_ni_index) {
	/* if actual NI == ref NI,
	 * use reference NI factor
	 */
		interpolated_factor = bottom_ni_factor;
	} else {
	/* if BOTTOM NI < actual NI < TOP NI,
	 * value is get by linear interpolation
	 */
		interpolate_temporal_factor(&interpolated_factor,
				noise_index,
				bottom_ni_factor,
				top_ni_factor);

		interpolate_regional_factor(&interpolated_factor,
				noise_index,
				bottom_ni_factor,
				top_ni_factor);

		interpolate_spatial_factor(&interpolated_factor,
				noise_index,
				bottom_ni_factor,
				top_ni_factor);

		interpolate_yuv_table_factor(&interpolated_factor,
				noise_index,
				bottom_ni_factor,
				top_ni_factor);
	}

	/* re_configure interpolated NI depended factor to SFR configurations */
	translate_temporal_factor(&tdnr_cfgs->temporal_dep_cfg, interpolated_factor);
	translate_regional_factor(&tdnr_cfgs->regional_dep_cfg, interpolated_factor);
	translate_spatial_factor(&tdnr_cfgs->spatial_dep_cfg, interpolated_factor);
	translate_yuv_table_factor(&tdnr_cfgs->yuv_tables, interpolated_factor);
}

static int fimc_is_hw_mcsc_cfg_tdnr_tuning_param(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_group *head,
	struct fimc_is_frame *frame,
	enum tdnr_mode *tdnr_mode,
	bool start_flag)
{
	int ret = 0;
	int ni_idx, arr_idx;
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;
	tdnr_setfile_contents *tdnr_tuneset;
	struct tdnr_configs tdnr_cfgs;
	u32 max_ref_ni = 0, min_ref_ni = 0;
	u32 bottom_ni_index = 0, top_ni_index = 0;
	u32 ref_bottom_ni = 0, ref_top_ni = 0;
	u32 noise_index = 0;
	bool use_tdnr_tuning = false;
	enum exynos_sensor_position sensor_position;

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;

	sensor_position = hw_ip->hardware->sensor_position[head->instance];

	if (!hw_mcsc->applied_setfile[sensor_position])
		return ret;

	if (frame->type == SHOT_TYPE_INTERNAL) {
		warn_hw("wrong TDNR setting at internal shot");
		return -EINVAL;
	}

#ifdef MCSC_DNR_USE_TUNING
	tdnr_tuneset = &hw_mcsc->applied_setfile[sensor_position]->tdnr_contents;
	use_tdnr_tuning = true;
#endif

	if (!use_tdnr_tuning)
		goto config;

	/* copy NI independed settings */
	tdnr_cfgs.general_cfg = tdnr_tuneset->general_cfg;
	tdnr_cfgs.temporal_indep_cfg = tdnr_tuneset->temporal_indep_cfg;
	for (arr_idx = ARR3_VAL1; arr_idx < ARR3_MAX; arr_idx++)
		tdnr_cfgs.constant_lut_coeffs[arr_idx] = tdnr_tuneset->constant_lut_coeffs[arr_idx];
	tdnr_cfgs.refine_cfg = tdnr_tuneset->refine_cfg;
	tdnr_cfgs.regional_indep_cfg = tdnr_tuneset->regional_indep_cfg;
	tdnr_cfgs.spatial_indep_cfg = tdnr_tuneset->spatial_indep_cfg;

#ifdef FIXED_TDNR_NOISE_INDEX
	noise_index = FIXED_TDNR_NOISE_INDEX_VALUE;
#else
	noise_index = frame->noise_idx; /* get applying NI from frame */
#endif

	if (!start_flag && hw_mcsc->cur_noise_index == noise_index)
		goto exit;

	/* find ref NI arry index for re-configure NI depended settings */
	max_ref_ni =
		MULTIPLIED_NI(tdnr_tuneset->ni_dep_factors[tdnr_tuneset->num_of_noiseindexes - 1].noise_index);
	min_ref_ni =
		MULTIPLIED_NI(tdnr_tuneset->ni_dep_factors[0].noise_index);

	if (noise_index >= max_ref_ni) {
		dbg_hw(2, "current NI (%d) > Max ref NI(%d), set bypass mode", noise_index, max_ref_ni);
		*tdnr_mode = TDNR_MODE_BYPASS;
		goto exit;
	} else if (noise_index <= min_ref_ni) {
		dbg_hw(2, "current NI (%d) < Min ref NI(%d), use first NI value", noise_index, min_ref_ni);
		bottom_ni_index = 0;
		top_ni_index = 0;
	} else {
		for (ni_idx = 0; ni_idx < tdnr_tuneset->num_of_noiseindexes - 1; ni_idx++) {
			ref_bottom_ni = MULTIPLIED_NI(tdnr_tuneset->ni_dep_factors[ni_idx].noise_index);
			ref_top_ni = MULTIPLIED_NI(tdnr_tuneset->ni_dep_factors[ni_idx + 1].noise_index);

			if (noise_index > ref_bottom_ni && noise_index < ref_top_ni) {
				bottom_ni_index = ni_idx;
				top_ni_index = ni_idx + 1;
				break;
			} else if (noise_index == ref_bottom_ni) {
				bottom_ni_index = ni_idx;
				top_ni_index = ni_idx;
				break;
			} else if (noise_index == ref_top_ni) {
				bottom_ni_index = ni_idx + 1;
				top_ni_index = ni_idx + 1;
				break;
			}
		}
	}

	/* interpolate & reconfigure by reference NI */
	reconfigure_ni_depended_tuneset(tdnr_tuneset, &tdnr_cfgs,
				noise_index, bottom_ni_index, top_ni_index);


config:
	/* update tuning SFR (NI independed cfg) */
	if (start_flag) {
		fimc_is_scaler_set_tdnr_tuneset_general(hw_ip->regs,
			tdnr_cfgs.general_cfg);
		fimc_is_scaler_set_tdnr_tuneset_constant_lut_coeffs(hw_ip->regs,
			tdnr_cfgs.constant_lut_coeffs);
		fimc_is_scaler_set_tdnr_tuneset_refine_control(hw_ip->regs,
			tdnr_cfgs.refine_cfg);
	}

	/* update tuning SFR (include NI depended cfg) */
	fimc_is_scaler_set_tdnr_tuneset_yuvtable(hw_ip->regs, tdnr_cfgs.yuv_tables);
	fimc_is_scaler_set_tdnr_tuneset_temporal(hw_ip->regs,
		tdnr_cfgs.temporal_dep_cfg, tdnr_cfgs.temporal_indep_cfg);
	fimc_is_scaler_set_tdnr_tuneset_regional_feature(hw_ip->regs,
		tdnr_cfgs.regional_dep_cfg, tdnr_cfgs.regional_indep_cfg);
	fimc_is_scaler_set_tdnr_tuneset_spatial(hw_ip->regs,
		tdnr_cfgs.spatial_dep_cfg, tdnr_cfgs.spatial_indep_cfg);

exit:
	hw_mcsc->cur_noise_index = noise_index;

	return ret;
}

int fimc_is_hw_mcsc_update_tdnr_register(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_frame *frame,
	struct is_param_region *param,
	bool start_flag)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;
	struct tpu_param *tpu_param;
	struct mcs_param *mcs_param;
	struct fimc_is_group *head;
	struct fimc_is_hw_mcsc_cap *cap;
	enum tdnr_mode tdnr_mode;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!param);
	FIMC_BUG(!frame);

	tpu_param = &param->tpu;
	mcs_param = &param->mcs;
	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	cap = GET_MCSC_HW_CAP(hw_ip);

	/* init tdnr setting */
	if (start_flag)
		fimc_is_hw_mcsc_tdnr_init(hw_ip, mcs_param, frame->instance);

	/* check tdnr mode pre */
	head = hw_ip->group[frame->instance]->head;
	tdnr_mode = fimc_is_hw_mcsc_check_tdnr_mode_pre(hw_ip, head,
					frame, tpu_param, mcs_param, hw_mcsc->cur_tdnr_mode);

	/* TDNR image size setting(must set as though set to "BYPASS mode" */
	fimc_is_scaler_set_tdnr_image_size(hw_ip->regs,
		mcs_param->output[hw_mcsc->tdnr_output].width,
		mcs_param->output[hw_mcsc->tdnr_output].height);

	switch (tdnr_mode) {
	case TDNR_MODE_BYPASS:
		break;
	case TDNR_MODE_2DNR:
	case TDNR_MODE_3DNR:
		/* RDMA setting */
		ret = fimc_is_hw_mcsc_cfg_tdnr_rdma(hw_ip, head, frame, mcs_param, tdnr_mode);
		if (ret) {
			err_hw("[ID:%d] tdnr rdma cfg failed", hw_ip->id);
			tdnr_mode = TDNR_MODE_BYPASS;
			break;
		}

		/* WDMA setting */
		ret = fimc_is_hw_mcsc_cfg_tdnr_wdma(hw_ip, head, frame, mcs_param, tdnr_mode);
		if (ret) {
			err_hw("[ID:%d] tdnr wdma cfg failed", hw_ip->id);
			tdnr_mode = TDNR_MODE_BYPASS;
			break;
		}

		/* setfile tuning parameter update */
		ret = fimc_is_hw_mcsc_cfg_tdnr_tuning_param(hw_ip, head, frame, &tdnr_mode, start_flag);
		if (ret) {
			err_hw("[ID:%d] tdnr tuning param setting failed", hw_ip->id);
			tdnr_mode = TDNR_MODE_BYPASS;
			break;
		}
		break;
	default:
		break;
	}

	fimc_is_hw_mcsc_check_tdnr_mode_post(hw_ip, head, hw_mcsc->cur_tdnr_mode,
						&tdnr_mode);

	fimc_is_scaler_set_tdnr_mode_select(hw_ip->regs, tdnr_mode);

	/* update current mode */
	hw_mcsc->cur_tdnr_mode = tdnr_mode;

	return 0;
}

int fimc_is_hw_mcsc_update_djag_register(struct fimc_is_hw_ip *hw_ip,
		struct mcs_param *param,
		u32 instance)
{
	int ret = 0;
#ifdef ENABLE_DJAG_IN_MCSC
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;
	struct fimc_is_hw_mcsc_cap *cap;
	u32 in_width, in_height;
	u32 out_width = 0, out_height = 0;
	const struct djag_setfile_contents *djag_tuneset;
	u32 hratio, vratio, min_ratio;
	u32 scale_index = MCSC_DJAG_PRESCALE_INDEX_1;
	enum exynos_sensor_position sensor_position;
	int output_id = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!param);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	cap = GET_MCSC_HW_CAP(hw_ip);
	sensor_position = hw_ip->hardware->sensor_position[instance];
	djag_tuneset = &init_djag_cfgs;

	in_width = param->input.width;
	in_height = param->input.height;

	/* select compare output_port : select max output size */
	for (output_id = MCSC_OUTPUT0; output_id < cap->max_output; output_id++) {
		if (test_bit(output_id, &hw_mcsc->out_en)) {
			if (out_width <= param->output[output_id].width) {
				out_width = param->output[output_id].width;
				out_height = param->output[output_id].height;
			}
		}
	}

	if (param->input.width > out_width) {
		sdbg_hw(2, "DJAG is not applied still.(input : %d > output : %d)\n", hw_ip,
				param->input.width,
				out_width);
		fimc_is_scaler_set_djag_enable(hw_ip->regs, 0);
		return ret;
	}

	/* check scale ratio if over 2.5 times */
	if (out_width * 10 > in_width * 25)
		out_width = round_down(in_width * 25 / 10, 2);

	if (out_height * 10 > in_height * 25)
		out_height = round_down(in_height * 25 / 10, 2);

	hratio = GET_DJAG_ZOOM_RATIO(in_width, out_width);
	vratio = GET_DJAG_ZOOM_RATIO(in_height, out_height);
	min_ratio = min(hratio, vratio);
	if (min_ratio >= GET_DJAG_ZOOM_RATIO(10, 10)) /* Zoom Ratio = 1.0 */
		scale_index = MCSC_DJAG_PRESCALE_INDEX_1;
	else if (min_ratio >= GET_DJAG_ZOOM_RATIO(10, 14)) /* Zoom Ratio = 1.4 */
		scale_index = MCSC_DJAG_PRESCALE_INDEX_2;
	else if (min_ratio >= GET_DJAG_ZOOM_RATIO(10, 20)) /* Zoom Ratio = 2.0 */
		scale_index = MCSC_DJAG_PRESCALE_INDEX_3;
	else
		scale_index = MCSC_DJAG_PRESCALE_INDEX_4;

	param->input.djag_out_width = out_width;
	param->input.djag_out_height = out_height;

	sdbg_hw(2, "djag scale up (%dx%d) -> (%dx%d)\n", hw_ip,
		in_width, in_height, out_width, out_height);

	/* djag image size setting */
	fimc_is_scaler_set_djag_src_size(hw_ip->regs, in_width, in_height);
	fimc_is_scaler_set_djag_dst_size(hw_ip->regs, out_width, out_height);
	fimc_is_scaler_set_djag_scaling_ratio(hw_ip->regs, hratio, vratio);
	fimc_is_scaler_set_djag_init_phase_offset(hw_ip->regs, 0, 0);
	fimc_is_scaler_set_djag_round_mode(hw_ip->regs, 1);

#ifdef MCSC_USE_DEJAG_TUNING_PARAM
	djag_tuneset = &hw_mcsc->applied_setfile[sensor_position]->djag_contents[scale_index];
#endif
	fimc_is_scaler_set_djag_tunning_param(hw_ip->regs, djag_tuneset);

	fimc_is_scaler_set_djag_enable(hw_ip->regs, 1);
#endif
	return ret;
}

int fimc_is_hw_mcsc_update_dsvra_register(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_group *head, struct mcs_param *mcs_param,
	u32 instance, enum mcsc_port dsvra_inport)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;
	struct fimc_is_hw_mcsc_cap *cap;
	u32 img_width, img_height;
	u32 src_width, src_height, src_x_pos, src_y_pos;
	u32 out_width, out_height;
	u32 hratio, vratio;
	ulong temp_width, temp_height;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!mcs_param);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	cap = GET_MCSC_HW_CAP(hw_ip);

	if (!test_bit(MCSC_OUTPUT_DS, &hw_mcsc_out_configured))
		return -EINVAL;

	if (dsvra_inport == MCSC_PORT_NONE)
		return -EINVAL;

	if (hw_ip->id == DEV_HW_MCSC1 && first_shot_tag == 0)
		hw_ip->debug_info[0].time[5] = cpu_clock(raw_smp_processor_id());

	/* DS input image size */
	img_width = mcs_param->output[dsvra_inport].width;
	img_height = mcs_param->output[dsvra_inport].height;

	/* DS input src size (src_size + src_pos <= img_size) */
	src_width = mcs_param->output[MCSC_OUTPUT_DS].crop_width;
	src_height = mcs_param->output[MCSC_OUTPUT_DS].crop_height;
	src_x_pos = mcs_param->output[MCSC_OUTPUT_DS].crop_offset_x;
	src_y_pos = mcs_param->output[MCSC_OUTPUT_DS].crop_offset_y;

	out_width = mcs_param->output[MCSC_OUTPUT_DS].width;
	out_height = mcs_param->output[MCSC_OUTPUT_DS].height;

	if (src_width != 0 && src_height != 0) {
		if ((int)src_x_pos < 0 || (int)src_y_pos < 0)
			panic("%s: wrong DS crop position (x:%d, y:%d)",
				__func__, src_x_pos, src_y_pos);

		if (src_width + src_x_pos > img_width) {
			warn_hw("%s: Out of crop width region of DS (%d < (%d + %d))",
				__func__, img_width, src_width, src_x_pos);
			src_x_pos = 0;
			src_width = img_width;
		}

		if (src_height + src_y_pos > img_height) {
			warn_hw("%s: Out of crop height region of DS (%d < (%d + %d))",
				__func__, img_height, src_height, src_y_pos);
			src_y_pos = 0;
			src_height = img_height;
		}
	} else {
		src_x_pos = 0;
		src_y_pos = 0;
		src_width = img_width;
		src_height = img_height;
	}

	temp_width = (ulong)src_width;
	temp_height = (ulong)src_height;
	hratio = (u32)((temp_width << MCSC_PRECISION) / out_width);
	vratio = (u32)((temp_height << MCSC_PRECISION) / out_height);

	fimc_is_scaler_set_ds_img_size(hw_ip->regs, img_width, img_height);
	fimc_is_scaler_set_ds_src_size(hw_ip->regs, src_width, src_height, src_x_pos, src_y_pos);
	fimc_is_scaler_set_ds_dst_size(hw_ip->regs, out_width, out_height);
	fimc_is_scaler_set_ds_scaling_ratio(hw_ip->regs, hratio, vratio);
	fimc_is_scaler_set_ds_init_phase_offset(hw_ip->regs, 0, 0);
	fimc_is_scaler_set_ds_gamma_table_enable(hw_ip->regs, true);
	ret = fimc_is_hw_mcsc_dma_output(hw_ip, &mcs_param->output[MCSC_OUTPUT_DS], MCSC_OUTPUT_DS, instance);

	fimc_is_scaler_set_otf_out_path(hw_ip->regs, dsvra_inport);
	fimc_is_scaler_set_ds_enable(hw_ip->regs, true);
	fimc_is_scaler_set_dma_out_enable(hw_ip->regs, MCSC_OUTPUT_DS, true);

	if (!test_bit(FIMC_IS_GROUP_OTF_INPUT, &head->state))
		set_bit(DSVRA_SET_DONE, &hw_mcsc->blk_set_ctrl[instance]);

	msdbg_hw(2, "%s: dsvra_inport(%d) configured\n", instance, hw_ip, __func__, dsvra_inport);

	return ret;
}

int fimc_is_hw_mcsc_update_ysum_register(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_group *head, struct mcs_param *mcs_param,
	u32 instance, enum mcsc_port ysumport)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc = NULL;
	struct fimc_is_hw_mcsc_cap *cap;
	u32 width, height;
	u32 start_x = 0, start_y = 0;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!hw_ip->priv_info);
	FIMC_BUG(!mcs_param);

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	cap = GET_MCSC_HW_CAP(hw_ip);

	if (ysumport == MCSC_PORT_NONE)
		return -EINVAL;

	if (!fimc_is_scaler_get_dma_out_enable(hw_ip->regs, ysumport))
		return -EINVAL;

	if (hw_ip->id == DEV_HW_MCSC1 && first_shot_tag == 0)
		hw_ip->debug_info[0].time[6] = cpu_clock(raw_smp_processor_id());

	switch (ysumport) {
	case MCSC_PORT_0:
		width = mcs_param->output[MCSC_OUTPUT0].width;
		height = mcs_param->output[MCSC_OUTPUT0].height;
		break;
	case MCSC_PORT_1:
		width = mcs_param->output[MCSC_OUTPUT1].width;
		height = mcs_param->output[MCSC_OUTPUT1].height;
		break;
	case MCSC_PORT_2:
		width = mcs_param->output[MCSC_OUTPUT2].width;
		height = mcs_param->output[MCSC_OUTPUT2].height;
		break;
	case MCSC_PORT_3:
		width = mcs_param->output[MCSC_OUTPUT3].width;
		height = mcs_param->output[MCSC_OUTPUT3].height;
		break;
	case MCSC_PORT_4:
		width = mcs_param->output[MCSC_OUTPUT4].width;
		height = mcs_param->output[MCSC_OUTPUT4].height;
		break;
	default:
		goto no_setting_ysum;
		break;
	}
	fimc_is_scaler_set_ysum_image_size(hw_ip->regs, width, height, start_x, start_y);

	fimc_is_scaler_set_ysum_input_sourece_enable(hw_ip->regs, ysumport, true);

	if (!test_bit(FIMC_IS_GROUP_OTF_INPUT, &head->state))
		set_bit(YSUM_SET_DONE, &hw_mcsc->blk_set_ctrl[instance]);

	msdbg_hw(2, "%s: ysum_port(%d) configured\n", instance, hw_ip, __func__, ysumport);

no_setting_ysum:
	return ret;
}

static void fimc_is_hw_mcsc_size_dump(struct fimc_is_hw_ip *hw_ip)
{
	int i;
	u32 input_src = 0;
	u32 in_w, in_h = 0;
	u32 rdma_w, rdma_h = 0;
	u32 poly_src_w, poly_src_h = 0;
	u32 poly_dst_w, poly_dst_h = 0;
	u32 post_in_w, post_in_h = 0;
	u32 post_out_w, post_out_h = 0;
	u32 wdma_enable = 0;
	u32 wdma_w, wdma_h = 0;
	u32 rdma_y_stride, rdma_uv_stride = 0;
	u32 wdma_y_stride, wdma_uv_stride = 0;
	struct fimc_is_hw_mcsc_cap *cap;

	FIMC_BUG_VOID(!hw_ip);

	/* skip size dump, if hw_ip wasn't opened */
	if (!test_bit(HW_OPEN, &hw_ip->state))
		return;

	cap = GET_MCSC_HW_CAP(hw_ip);
	if (!cap) {
		err_hw("failed to get hw_mcsc_cap(%p)", cap);
		return;
	}

	input_src = fimc_is_scaler_get_input_source(hw_ip->regs, hw_ip->id);
	fimc_is_scaler_get_input_img_size(hw_ip->regs, hw_ip->id, &in_w, &in_h);
	fimc_is_scaler_get_rdma_size(hw_ip->regs, &rdma_w, &rdma_h);
	fimc_is_scaler_get_rdma_stride(hw_ip->regs, &rdma_y_stride, &rdma_uv_stride);

	sdbg_hw(2, "=SIZE=====================================\n"
		"[INPUT] SRC:%d(0:OTF, 1:DMA), SIZE:%dx%d\n"
		"[RDMA] SIZE:%dx%d, STRIDE: Y:%d, UV:%d\n",
		hw_ip, input_src, in_w, in_h,
		rdma_w, rdma_h, rdma_y_stride, rdma_uv_stride);

	for (i = MCSC_OUTPUT0; i < cap->max_output; i++) {
		fimc_is_scaler_get_poly_src_size(hw_ip->regs, i, &poly_src_w, &poly_src_h);
		fimc_is_scaler_get_poly_dst_size(hw_ip->regs, i, &poly_dst_w, &poly_dst_h);
		fimc_is_scaler_get_post_img_size(hw_ip->regs, i, &post_in_w, &post_in_h);
		fimc_is_scaler_get_post_dst_size(hw_ip->regs, i, &post_out_w, &post_out_h);
		fimc_is_scaler_get_wdma_size(hw_ip->regs, i, &wdma_w, &wdma_h);
		fimc_is_scaler_get_wdma_stride(hw_ip->regs, i, &wdma_y_stride, &wdma_uv_stride);
		wdma_enable = fimc_is_scaler_get_dma_out_enable(hw_ip->regs, i);

		dbg_hw(2, "[POLY%d] SRC:%dx%d, DST:%dx%d\n"
			"[POST%d] SRC:%dx%d, DST:%dx%d\n"
			"[WDMA%d] ENABLE:%d, SIZE:%dx%d, STRIDE: Y:%d, UV:%d\n",
			i, poly_src_w, poly_src_h, poly_dst_w, poly_dst_h,
			i, post_in_w, post_in_h, post_out_w, post_out_h,
			i, wdma_enable, wdma_w, wdma_h, wdma_y_stride, wdma_uv_stride);
	}
	sdbg_hw(2, "==========================================\n", hw_ip);

	return;
}

static int fimc_is_hw_mcsc_get_meta(struct fimc_is_hw_ip *hw_ip,
		struct fimc_is_frame *frame, unsigned long hw_map)
{
	int ret = 0;
	struct fimc_is_hw_mcsc *hw_mcsc;

	if (unlikely(!frame)) {
		mserr_hw("get_meta: frame is null", atomic_read(&hw_ip->instance), hw_ip);
		return 0;
	}

	if (!test_bit_variables(hw_ip->id, &hw_map))
		return 0;

	hw_mcsc = (struct fimc_is_hw_mcsc *)hw_ip->priv_info;
	if (unlikely(!hw_mcsc)) {
		mserr_hw("priv_info is NULL", frame->instance, hw_ip);
		return -EINVAL;
	}

	fimc_is_scaler_get_ysum_result(hw_ip->regs,
		&frame->shot->udm.scaler.ysumdata.higher_ysum_value,
		&frame->shot->udm.scaler.ysumdata.lower_ysum_value);

	return ret;
}

const struct fimc_is_hw_ip_ops fimc_is_hw_mcsc_ops = {
	.open			= fimc_is_hw_mcsc_open,
	.init			= fimc_is_hw_mcsc_init,
	.close			= fimc_is_hw_mcsc_close,
	.enable			= fimc_is_hw_mcsc_enable,
	.disable		= fimc_is_hw_mcsc_disable,
	.shot			= fimc_is_hw_mcsc_shot,
	.set_param		= fimc_is_hw_mcsc_set_param,
	.get_meta		= fimc_is_hw_mcsc_get_meta,
	.frame_ndone		= fimc_is_hw_mcsc_frame_ndone,
	.load_setfile		= fimc_is_hw_mcsc_load_setfile,
	.apply_setfile		= fimc_is_hw_mcsc_apply_setfile,
	.delete_setfile		= fimc_is_hw_mcsc_delete_setfile,
	.size_dump		= fimc_is_hw_mcsc_size_dump,
	.clk_gate		= fimc_is_hardware_clk_gate
};

int fimc_is_hw_mcsc_probe(struct fimc_is_hw_ip *hw_ip, struct fimc_is_interface *itf,
	struct fimc_is_interface_ischain *itfc, int id, const char *name)
{
	int ret = 0;
	int hw_slot = -1;

	FIMC_BUG(!hw_ip);
	FIMC_BUG(!itf);
	FIMC_BUG(!itfc);

	/* initialize device hardware */
	hw_ip->id   = id;
	snprintf(hw_ip->name, sizeof(hw_ip->name), "%s", name);
	hw_ip->ops  = &fimc_is_hw_mcsc_ops;
	hw_ip->itf  = itf;
	hw_ip->itfc = itfc;
	atomic_set(&hw_ip->fcount, 0);
	hw_ip->is_leader = true;
	atomic_set(&hw_ip->status.Vvalid, V_BLANK);
	atomic_set(&hw_ip->rsccount, 0);
	atomic_set(&hw_ip->run_rsccount, 0);
	init_waitqueue_head(&hw_ip->status.wait_queue);

	/* set mcsc sfr base address */
	hw_slot = fimc_is_hw_slot_id(id);
	if (!valid_hw_slot_id(hw_slot)) {
		serr_hw("invalid hw_slot (%d)", hw_ip, hw_slot);
		return -EINVAL;
	}

	/* set mcsc interrupt handler */
	itfc->itf_ip[hw_slot].handler[INTR_HWIP1].handler = &fimc_is_hw_mcsc_handle_interrupt;

	clear_bit(HW_OPEN, &hw_ip->state);
	clear_bit(HW_INIT, &hw_ip->state);
	clear_bit(HW_CONFIG, &hw_ip->state);
	clear_bit(HW_RUN, &hw_ip->state);
	clear_bit(HW_TUNESET, &hw_ip->state);
	spin_lock_init(&shared_output_slock);

	sinfo_hw("probe done\n", hw_ip);

	return ret;
}
