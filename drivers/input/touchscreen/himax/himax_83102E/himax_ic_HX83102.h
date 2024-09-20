/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83102 chipset
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#if defined(CONFIG_TOUCHSCREEN_HIMAX_I2C)
#include "himax_platform.h"
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_SPI)
#include "himax_platform_SPI.h"
#endif

#include "himax_common.h"
#include "himax_ic_core.h"
#include <linux/slab.h>

#define hx83102ab_fw_addr_sorting_mode_en		0x100007FC
#define hx83102ab_fw_addr_selftest_addr_en		0x100007F8
#define hx83102ab_data_adc_cfg_1			0x10007B00
#define hx83102a_data_df_rx		48
#define hx83102a_data_df_tx		24
#define hx83102a_data_df_x_res		2160
#define hx83102a_data_df_y_res		3840
#define hx83102b_data_df_x_res		720
#define hx83102b_data_df_y_res		1280

#define hx83102d_fw_addr_raw_out_sel			0x800204f4
#define hx83102d_zf_data_adc_cfg_1 0x10007B00
#define hx83102d_zf_data_adc_cfg_2 0x10006A00
#define hx83102d_zf_data_adc_cfg_3 0x10007500
#define hx83102d_zf_data_bor_prevent_info 0x10007268
#define hx83102d_zf_data_notch_info 0x10007300
#define hx83102d_zf_func_info_en 0x10007FD0
#define hx83102d_zf_po_sub_func 0x10005A00
#define hx83102d_zf_data_sram_start_addr 0x20000000
#define hx83102d_data_df_x_res		720
#define hx83102d_data_df_y_res		1280
#define hx83102d_adr_osc_en 0x9000009C
#define hx83102d_adr_osc_pw 0x90000280

#define hx83102e_fw_addr_raw_out_sel			0x100072EC
#define hx83102e_ic_adr_tcon_rst	0x80020004
#define hx83102e_data_df_rx		48
#define hx83102e_data_df_tx		30
#define hx83102e_data_df_x_res		1200
#define hx83102e_data_df_y_res		1920

#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
#ifdef HX_EN_DYNAMIC_NAME
#define hx83102_ic_b9_en        0x300B9000
#define hx83102_ic_fw           0x300E8006

#define hx83102b_ic_osc_en       0x900880A0
#define hx83102d_ic_osc_en       0x9000009C
#define hx83102d_ic_osc_pw       0x90000280
#endif
#endif

#ifdef HX_ESD_RECOVERY
	extern u8 HX_ESD_RESET_ACTIVATE;
#endif

