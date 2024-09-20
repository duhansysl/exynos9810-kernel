/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_SUBDEV_MCSC_H
#define FIMC_IS_SUBDEV_MCSC_H

#include "fimc-is-hw-control.h"
#include "fimc-is-interface-library.h"
#include "fimc-is-param.h"

#define MCSC_ROUND_UP(x, d) \
	((d) * (((x) + ((d) - 1)) / (d)))

#define GET_MCSC_HW_CAP(hwip) \
	((hwip->priv_info) ? &((struct fimc_is_hw_mcsc *)hw_ip->priv_info)->cap : NULL)
#define GET_ENTRY_FROM_OUTPUT_ID(output_id) \
	(output_id + ENTRY_M0P)
#define GET_DJAG_ZOOM_RATIO(in, out) (u32)(((in * 1000 / out) << MCSC_PRECISION) / 1000)

enum mcsc_img_format {
	MCSC_YUV422_1P_YUYV = 0,
	MCSC_YUV422_1P_YVYU,
	MCSC_YUV422_1P_UYVY,
	MCSC_YUV422_1P_VYUY,
	MCSC_YUV422_2P_UFIRST,
	MCSC_YUV422_2P_VFIRST,
	MCSC_YUV422_3P,
	MCSC_YUV420_2P_UFIRST,
	MCSC_YUV420_2P_VFIRST,
	MCSC_YUV420_3P,
	MCSC_RGB_ARGB8888,
	MCSC_RGB_BGRA8888,
	MCSC_RGB_RGBA8888,
	MCSC_RGB_ABGR8888,
};

enum mcsc_io_type {
	HW_MCSC_OTF_INPUT,
	HW_MCSC_OTF_OUTPUT,
	HW_MCSC_DMA_INPUT,
	HW_MCSC_DMA_OUTPUT,
};

enum mcsc_cap_enum {
	MCSC_CAP_NOT_SUPPORT = 0,
	MCSC_CAP_SUPPORT,
};

enum mcsc_shadow_ctrl {
	SHADOW_WRITE_START = 0,
	SHADOW_WRITE_FINISH,
};

enum tdnr_mode {
	TDNR_MODE_BYPASS = 0,
	TDNR_MODE_2DNR,
	TDNR_MODE_3DNR,
	TDNR_MODE_INVALID,
};

enum tdnr_buf_type {
	TDNR_IMAGE = 0,
	TDNR_WEIGHT,
};

enum mcsc_block_set_ctrl {
	TDNR_SET_DONE = 0,
	DJAG_SET_DONE,
	YSUM_SET_DONE,
	DSVRA_SET_DONE,
	ALL_BLOCK_SET_DONE = 0xFF,
};

#define MAX_NOISEINDEX_DEPENDED_CONFIGS		(9)
#define INTERPOLATE_SHIFT			(12)
#define INTERPOLATE_NUMERATOR(Y1, Y2, diff_x_x1) \
	(((Y2) - (Y1)) * (diff_x_x1)) << INTERPOLATE_SHIFT
#define GET_LINEAR_INTERPOLATE_VALUE(Y1, Y2, diff_x2_x1, diff_x_x1) 		\
	((INTERPOLATE_NUMERATOR((int)Y1, (int)Y2, diff_x_x1)) / (diff_x2_x1)) + \
					(((int)(Y1) << INTERPOLATE_SHIFT))
#define RESTORE_SHIFT_VALUE(value) ((int)(value) >> INTERPOLATE_SHIFT)

/* DDK delivered NI to multiply 10 */
#define MULTIPLIED_NI(value)		(10 * (value))

enum tdnr_refine_mode {
	TDNR_REFINE_MAX = 0,
	TDNR_REFINE_MEAN,
	TDNR_REFINE_MIN,
};

enum tdnr_weight_mode {
	TDNR_WEIGHT_MAX = 0,
	TDNR_WEIGHT_MIN,
	TDNR_WEIGHT_LUMA,
};

enum arr_index3 {
	ARR3_VAL1 = 0,
	ARR3_VAL2,
	ARR3_VAL3,
	ARR3_MAX,
};

enum arr_index4 {
	ARR4_VAL1 = 0,
	ARR4_VAL2,
	ARR4_VAL3,
	ARR4_VAL4,
	ARR4_MAX,
};

struct general_config {
	bool	use_average_current;
	bool	auto_coeff_3d;
	u32	blending_threshold;
	u32     temporal_blend_thresh_weight_min;
	u32     temporal_blend_thresh_weight_max;
	u32     temporal_blend_thresh_criterion;
};

struct temporal_ni_dep_config {
	u32	temporal_weight_coeff_y1;
	u32	temporal_weight_coeff_y2;
	u32	temporal_weight_coeff_uv1;
	u32	temporal_weight_coeff_uv2;

	u32	auto_lut_gains_y[ARR3_MAX];
	u32	y_offset;
	u32	auto_lut_gains_uv[ARR3_MAX];
	u32	uv_offset;
};

struct temporal_ni_indep_config {
	u32	prev_gridx[ARR3_MAX];
	u32	prev_gridx_lut[ARR4_MAX];
};

struct refine_control_config {
	bool			is_refine_on;
	enum tdnr_refine_mode	refine_mode;
	u32			refine_threshold;
	u32			refine_coeff_update;
};

struct regional_ni_dep_config {
	bool	is_region_diff_on;
	u32	region_gain;
	bool	other_channels_check;
	u32	other_channel_gain;
};

struct regional_ni_indep_config {
	bool	dont_use_region_sign;
	bool	diff_condition_are_all_components_similar;
	bool	line_condition;
	bool	is_motiondetect_luma_mode_mean;
	u32	region_offset;
	bool	is_motiondetect_chroma_mode_mean;
	u32	other_channel_offset;
	u32	coefficient_offset;
};

struct spatial_ni_dep_config {
	enum tdnr_weight_mode	weight_mode;
	u32			spatial_gain;
	bool			spatial_separate_weights;
	u32			spatial_luma_gain[ARR4_MAX];
	u32			spatial_uv_gain[ARR4_MAX];
};

struct spatial_ni_indep_config {
	u32	spatial_refine_threshold;
	u32	spatial_luma_offset[ARR4_MAX];
	u32	spatial_uv_offset[ARR4_MAX];
};

struct yuv_table_config {
	u32	x_grid_y[ARR3_MAX];
	u32	y_std_offset;
	u32	y_std_slope[ARR4_MAX];

	u32	x_grid_u[ARR3_MAX];
	u32	u_std_offset;
	u32	u_std_slope[ARR4_MAX];

	u32	x_grid_v[ARR3_MAX];
	u32	v_std_offset;
	u32	v_std_slope[ARR4_MAX];
};

struct ni_dep_factors {
	u32			noise_index;
	u32			temporal_motion_detection_luma_low;
	u32			temporal_motion_detection_luma_contrast;
	u32			temporal_motion_detection_luma_high;
	bool			temporal_motion_detection_luma_off;
	u32			temporal_motion_detection_chroma_low;
	u32			temporal_motion_detection_chroma_contrast;
	u32			temporal_motion_detection_chroma_high;
	bool			temporal_motion_detection_chroma_off;
	u32			temporal_weight_luma_power_base;
	u32			temporal_weight_luma_power_gamma;
	u32			temporal_weight_chroma_power_base;
	u32			temporal_weight_chroma_power_gamma;
	bool			temporal_weight_hot_region;
	u32			temporal_weight_hot_region_power;
	bool			temporal_weight_chroma_threshold;
	u32			temporal_weight_chroma_power;
	u32			spatial_power;
	enum tdnr_weight_mode	spatial_weight_mode;
	bool			spatial_separate_weighting;
	u32			spatial_pd_luma_slope;
	u32			spatial_pd_luma_offset;
	u32			spatial_pd_chroma_slope;
	u32			spatial_pd_chroma_offset;

	struct yuv_table_config	yuv_tables;
};

typedef struct {
	/* Brightness/Contrast control param */
	u32 y_offset;
	u32 y_gain;

	/* Hue/Saturation control param */
	u32 c_gain00;
	u32 c_gain01;
	u32 c_gain10;
	u32 c_gain11;
} scaler_setfile_contents;

typedef struct {
	bool	tdnr_enable;
	u32	num_of_noiseindexes;
	u32	compression_binary_error_thr;

	struct general_config 			general_cfg;
	struct temporal_ni_indep_config		temporal_indep_cfg;
	u32					constant_lut_coeffs[ARR3_MAX];
	struct refine_control_config		refine_cfg;
	struct regional_ni_indep_config		regional_indep_cfg;
	struct spatial_ni_indep_config		spatial_indep_cfg;
	struct ni_dep_factors			ni_dep_factors[MAX_NOISEINDEX_DEPENDED_CONFIGS];
} tdnr_setfile_contents;

struct tdnr_configs {
	struct general_config			general_cfg;
	struct yuv_table_config			yuv_tables;
	struct temporal_ni_dep_config		temporal_dep_cfg;
	struct temporal_ni_indep_config		temporal_indep_cfg;
	u32					constant_lut_coeffs[ARR3_MAX];
	struct refine_control_config		refine_cfg;
	struct regional_ni_dep_config		regional_dep_cfg;
	struct regional_ni_indep_config		regional_indep_cfg;
	struct spatial_ni_dep_config		spatial_dep_cfg;
	struct spatial_ni_indep_config		spatial_indep_cfg;
};

/* for DeJag Block */
#define MCSC_DJAG_PRESCALE_INDEX_1		0	/* x1.0 */
#define MCSC_DJAG_PRESCALE_INDEX_2		1	/* x1.1~x1.4 */
#define MCSC_DJAG_PRESCALE_INDEX_3		2	/* x1.5~x2.0 */
#define MCSC_DJAG_PRESCALE_INDEX_4		3	/* x2.1~ */

#define MAX_SCALINGRATIOINDEX_DEPENDED_CONFIGS	(4)
#define MAX_DITHER_VALUE_CONFIGS				(9)

struct djag_xfilter_dejagging_coeff_config {
	u32 xfilter_dejagging_weight0;
	u32 xfilter_dejagging_weight1;
	u32 xfilter_hf_boost_weight;
	u32 center_hf_boost_weight;
	u32 diagonal_hf_boost_weight;
	u32 center_weighted_mean_weight;
};

struct djag_thres_1x5_matching_config {
	u32 thres_1x5_matching_sad;
	u32 thres_1x5_abshf;
};

struct djag_thres_shooting_detect_config {
	u32 thres_shooting_llcrr;
	u32 thres_shooting_lcr;
	u32 thres_shooting_neighbor;
	u32 thres_shooting_uucdd;
	u32 thres_shooting_ucd;
	u32 min_max_weight;
};

struct djag_lfsr_seed_config {
	u32 lfsr_seed_0;
	u32 lfsr_seed_1;
	u32 lfsr_seed_2;
};

struct djag_dither_config {
	u32 dither_value[MAX_DITHER_VALUE_CONFIGS];
	u32 sat_ctrl;
	u32 dither_sat_thres;
	u32 dither_thres;
};

struct djag_cp_config {
	u32 cp_hf_thres;
	u32 cp_arbi_max_cov_offset;
	u32 cp_arbi_max_cov_shift;
	u32 cp_arbi_denom;
	u32 cp_arbi_mode;
};

struct djag_setfile_contents {
	struct djag_xfilter_dejagging_coeff_config xfilter_dejagging_coeff_cfg;
	struct djag_thres_1x5_matching_config thres_1x5_matching_cfg;
	struct djag_thres_shooting_detect_config thres_shooting_detect_cfg;
	struct djag_lfsr_seed_config lfsr_seed_cfg;
	struct djag_dither_config dither_cfg;
	struct djag_cp_config cp_cfg;
};

struct hw_api_scaler_setfile {
	u32 setfile_version;

	/* contents for Full/Narrow mode
	 * 0 : SCALER_OUTPUT_YUV_RANGE_FULL
	 * 1 : SCALER_OUTPUT_YUV_RANGE_NARROW
	 */
	scaler_setfile_contents contents[2];
#ifdef MCSC_DNR_USE_TUNING
	tdnr_setfile_contents tdnr_contents;
#endif
#ifdef MCSC_USE_DEJAG_TUNING_PARAM
	/* Setfile tuning parameters for DJAG (Lhotse)
	 * 0 : Scaling ratio = x1.0
	 * 1 : Scaling ratio = x1.1~x1.4
	 * 2 : Scaling ratio = x1.5~x2.0
	 * 3 : Scaling ratio = x2.1~
	 */
	struct djag_setfile_contents djag_contents[MAX_SCALINGRATIOINDEX_DEPENDED_CONFIGS];
#endif
};

/**
 * struct fimc_is_fw_mcsc_cap - capability of mcsc
 *  This Structure specified the spec of mcsc.
 * @hw_ver: type is hexa. eg. 1.22.0 -> 0b0001_0022_0000_0000
 * @max_output: the number of output port to support
 * <below fields has the value in enum mcsc_cap_enum>
 * @in_otf: capability of input otf
 * @in_dma: capability of input dma
 * @hw_fc: capability of hardware flow control
 * @out_otf: capability of output otf
 * @out_dma: capability of output dma
 * @out_hwfc: capability of output dma (each output)
 * @tdnr: capability of 3DNR feature
 */
struct fimc_is_hw_mcsc_cap {
	u32			hw_ver;
	u32			max_output;
	enum mcsc_cap_enum	in_otf;
	enum mcsc_cap_enum	in_dma;
	enum mcsc_cap_enum	hwfc;
	enum mcsc_cap_enum	out_otf[MCSC_OUTPUT_MAX];
	enum mcsc_cap_enum	out_dma[MCSC_OUTPUT_MAX];
	enum mcsc_cap_enum	out_hwfc[MCSC_OUTPUT_MAX];
	bool 			enable_shared_output;
	enum mcsc_cap_enum	tdnr;
	enum mcsc_cap_enum	djag;
	enum mcsc_cap_enum	ysum;
	enum mcsc_cap_enum	ds_vra;
};

struct fimc_is_hw_mcsc {
	struct	hw_api_scaler_setfile setfile[SENSOR_POSITION_MAX][FIMC_IS_MAX_SETFILE];
	struct	hw_api_scaler_setfile *applied_setfile[SENSOR_POSITION_MAX];
	struct	fimc_is_hw_mcsc_cap cap;

	u32	in_img_format;
	u32	out_img_format[MCSC_OUTPUT_MAX];
	bool	conv420_en[MCSC_OUTPUT_MAX];
	bool	rep_flag[FIMC_IS_STREAM_COUNT];
	int	yuv_range;
	u32	instance;
	ulong	out_en;
	ulong	blk_set_ctrl[FIMC_IS_STREAM_COUNT];
	u32	prev_hwfc_output_ids;

	/* for tdnr use */
	enum mcsc_output_index	tdnr_output;
	bool			tdnr_first;
	bool			tdnr_internal_buf;
	dma_addr_t		dvaddr_tdnr[2];
	enum tdnr_mode		cur_tdnr_mode;
	u32			cur_noise_index;
	struct tdnr_configs	tdnr_cfgs;

	/* for Djag */
	u32 djag_input_source;
	struct djag_setfile_contents djag_tunecfg;
};

int fimc_is_hw_mcsc_probe(struct fimc_is_hw_ip *hw_ip, struct fimc_is_interface *itf,
	struct fimc_is_interface_ischain *itfc, int id, const char *name);

int fimc_is_hw_mcsc_update_param(struct fimc_is_hw_ip *hw_ip,
	struct mcs_param *param, u32 instance);
void fimc_is_hw_mcsc_frame_done(struct fimc_is_hw_ip *hw_ip, struct fimc_is_frame *frame,
	int done_type);
int fimc_is_hw_mcsc_reset(struct fimc_is_hw_ip *hw_ip);
int fimc_is_hw_mcsc_clear_interrupt(struct fimc_is_hw_ip *hw_ip);

int fimc_is_hw_mcsc_otf_input(struct fimc_is_hw_ip *hw_ip, struct param_mcs_input *input,
	u32 instance);
int fimc_is_hw_mcsc_dma_input(struct fimc_is_hw_ip *hw_ip, struct param_mcs_input *input,
	u32 instance);
int fimc_is_hw_mcsc_poly_phase(struct fimc_is_hw_ip *hw_ip, struct param_mcs_input *input,
	struct param_mcs_output *output, u32 output_id, u32 instance);
int fimc_is_hw_mcsc_post_chain(struct fimc_is_hw_ip *hw_ip, struct param_mcs_input *input,
	struct param_mcs_output *output, u32 output_id, u32 instance);
int fimc_is_hw_mcsc_flip(struct fimc_is_hw_ip *hw_ip, struct param_mcs_output *output,
	u32 output_id, u32 instance);
int fimc_is_hw_mcsc_otf_output(struct fimc_is_hw_ip *hw_ip, struct param_mcs_output *output,
	u32 output_id, u32 instance);
int fimc_is_hw_mcsc_dma_output(struct fimc_is_hw_ip *hw_ip, struct param_mcs_output *output,
	u32 output_id, u32 instance);
int fimc_is_hw_mcsc_output_yuvrange(struct fimc_is_hw_ip *hw_ip, struct param_mcs_output *output,
	u32 output_id, u32 instance);
int fimc_is_hw_mcsc_hwfc_mode(struct fimc_is_hw_ip *hw_ip, struct param_mcs_input *input,
	u32 hwfc_output_ids, u32 dma_output_ids, u32 instance);
int fimc_is_hw_mcsc_hwfc_output(struct fimc_is_hw_ip *hw_ip, struct param_mcs_output *output,
	u32 output_id, u32 instance);

int fimc_is_hw_mcsc_adjust_input_img_fmt(u32 format, u32 plane, u32 order, u32 *img_format);
int fimc_is_hw_mcsc_adjust_output_img_fmt(u32 format, u32 plane, u32 order, u32 *img_format,
	bool *conv420_flag);
int fimc_is_hw_mcsc_check_format(enum mcsc_io_type type, u32 format, u32 bit_width,
	u32 width, u32 height);
u32 fimc_is_scaler_get_idle_status(void __iomem *base_addr, u32 hw_id);

int fimc_is_hw_mcsc_update_tdnr_register(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_frame *frame,
	struct is_param_region *param,
	bool start_flag);

int fimc_is_hw_mcsc_update_djag_register(struct fimc_is_hw_ip *hw_ip,
		struct mcs_param *param,
		u32 instance);
int fimc_is_hw_mcsc_update_ysum_register(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_group *head, struct mcs_param *mcs_param,
	u32 instance, enum mcsc_port ysumport);

int fimc_is_hw_mcsc_update_dsvra_register(struct fimc_is_hw_ip *hw_ip,
	struct fimc_is_group *head, struct mcs_param *mcs_param,
	u32 instance, enum mcsc_port dsvra_inport);

#ifdef DEBUG_HW_SIZE
#define hw_mcsc_check_size(hw_ip, param, instance, output_id) \
	fimc_is_hw_mcsc_check_size(hw_ip, param, instance, output_id)
#else
#define hw_mcsc_check_size(hw_ip, param, instance, output_id)
#endif
#endif
