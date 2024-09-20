/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for inspection functions
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

#include "himax_inspection.h"
#include <linux/spu-verify.h>

static int g_gap_vertical_partial = 4;
static int *g_gap_vertical_part;
static int g_gap_horizontal_partial = 1;
static int *g_gap_horizontal_part;

static int g_dc_max;

static int g_1kind_raw_size;
uint32_t g_rslt_data_len;
int **g_inspection_criteria;
int *g_inspt_crtra_flag;
int *g_test_item_flag;
int do_lpwg_test;
int HX_CRITERIA_ITEM;
int HX_CRITERIA_SIZE;
char *g_rslt_data;
static char g_file_path[256];
static char g_rslt_log[256];
static char g_start_log[512];
#define FAIL_IN_INDEX "%s: %s FAIL in index %d\n"
#define ABS(x)  (((x) < 0) ? -(x) : (x))
static int g_portrait_data[5];

char *g_hx_head_str[] = {
	"TP_Info",
	"Project_Info",
	"TestItem",
	"TestCriteria_Weight",
	"TestCriteria",
	NULL
};

/*Need to map THP_INSPECTION_ENUM*/
char *g_himax_inspection_mode[] = {
	"HIMAX_OPEN",
	"HIMAX_MICRO_OPEN",
	"HIMAX_SHORT",
	"HIMAX_RAWDATA",
	"HIMAX_BPN_RAWDATA",
	"HIMAX_SC",
	"HIMAX_WEIGHT_NOISE",
	"HIMAX_ABS_NOISE",
	"HIMAX_SORTING",
	"HIMAX_GAPTEST_RAW",
	/*"HIMAX_GAPTEST_RAW_X",*/
	/*"HIMAX_GAPTEST_RAW_Y",*/

	"HIMAX_ACT_IDLE_RAWDATA",
	"HIMAX_ACT_IDLE_BPN_RAWDATA",
	"HIMAX_ACT_IDLE_NOISE",

	"HIMAX_LPWUG_RAWDATA",
	"HIMAX_LPWUG_BPN_RAWDATA",
	"HIMAX_LPWUG_WEIGHT_NOISE",
	"HIMAX_LPWUG_ABS_NOISE",
	"HIMAX_LPWUG_IDLE_RAWDATA",
	"HIMAX_LPWUG_IDLE_BPN_RAWDATA",
	"HIMAX_LPWUG_IDLE_NOISE",

	"HIMAX_BACK_NORMAL",
	NULL
};

/* for criteria */
char *g_hx_inspt_crtra_name[] = {
	"CRITERIA_RAW_MIN",
	"CRITERIA_RAW_MAX",
	"CRITERIA_RAW_BPN_MIN",
	"CRITERIA_RAW_BPN_MAX",
	"CRITERIA_SC_MIN",
	"CRITERIA_SC_MAX",
	"CRITERIA_SC_GOLDEN",
	"CRITERIA_SHORT_MIN",
	"CRITERIA_SHORT_MAX",
	"CRITERIA_OPEN_MIN",
	"CRITERIA_OPEN_MAX",
	"CRITERIA_MICRO_OPEN_MIN",
	"CRITERIA_MICRO_OPEN_MAX",
	"CRITERIA_NOISE_WT_MIN",
	"CRITERIA_NOISE_WT_MAX",
	"CRITERIA_NOISE_ABS_MIN",
	"CRITERIA_NOISE_ABS_MAX",
	"CRITERIA_SORT_MIN",
	"CRITERIA_SORT_MAX",

	"CRITERIA_GAP_RAW_HOR_MIN",
	"CRITERIA_GAP_RAW_HOR_MAX",
	"CRITERIA_GAP_RAW_VER_MIN",
	"CRITERIA_GAP_RAW_VER_MAX",

	"ACT_IDLE_NOISE_MIN",
	"ACT_IDLE_NOISE_MAX",
	"ACT_IDLE_RAWDATA_MIN",
	"ACT_IDLE_RAWDATA_MAX",
	"ACT_IDLE_RAW_BPN_MIN",
	"ACT_IDLE_RAW_BPN_MAX",

	"LPWUG_NOISE_WT_MIN",
	"LPWUG_NOISE_WT_MAX",
	"LPWUG_NOISE_ABS_MIN",
	"LPWUG_NOISE_ABS_MAX",
	"LPWUG_RAWDATA_MIN",
	"LPWUG_RAWDATA_MAX",
	"LPWUG_RAW_BPN_MIN",
	"LPWUG_RAW_BPN_MAX",

	"LPWUG_IDLE_NOISE_MIN",
	"LPWUG_IDLE_NOISE_MAX",
	"LPWUG_IDLE_RAWDATA_MIN",
	"LPWUG_IDLE_RAWDATA_MAX",
	"LPWUG_IDLE_RAW_BPN_MIN",
	"LPWUG_IDLE_RAW_BPN_MAX",
	NULL
};

/* SEC INSPECTION */
extern int g_ts_dbg;

extern struct fw_operation *pfw_op;
/******************/


void (*fp_himax_self_test_init)(void) = himax_inspection_init;


static void himax_press_powerkey(void)
{
	I(" %s POWER KEY event %x press\n", __func__, KEY_POWER);
	input_report_key(private_ts->input_dev, KEY_POWER, 1);
	input_sync(private_ts->input_dev);

	msleep(100);

	I(" %s POWER KEY event %x release\n", __func__, KEY_POWER);
	input_report_key(private_ts->input_dev, KEY_POWER, 0);
	input_sync(private_ts->input_dev);
}


static uint16_t	NOISEMAX;
static uint16_t	g_recal_thx;

static int  arraydata_max1, arraydata_max2, arraydata_max3;
static int  arraydata_min1, arraydata_min2, arraydata_min3;

void himax_get_arraydata_edge(uint32_t *RAW)
{
	int temp, i, j;
	int len = ic_data->HX_RX_NUM * ic_data->HX_TX_NUM;
	uint32_t ArrayData[len];

	for (i = 0; i < len; i++)
		ArrayData[i] = RAW[i];
	for (j = len-1; j > 0; j--) {    /*min to max*/
		for (i = 0; i < j; i++) {
			if (ArrayData[i] > ArrayData[i+1]) {
				temp = ArrayData[i];
				ArrayData[i] = ArrayData[i+1];
				ArrayData[i+1] = temp;
			}
		}
	}

	arraydata_min1 = ArrayData[0];
	arraydata_min2 = ArrayData[1];
	arraydata_min3 = ArrayData[2];
	arraydata_max1 = ArrayData[len-3];
	arraydata_max2 = ArrayData[len-2];
	arraydata_max3 = ArrayData[len-1];

}


static int hx_test_data_pop_out(char *rslt_buf, char *filepath)
{
	struct file *raw_file = NULL;
	struct filename *vts_name = NULL;
	mm_segment_t fs;
	loff_t pos = 0;
	int ret_val = NO_ERR;

	I("%s: Entering!\n", __func__);
	I("data size=0x%04X\n", (uint32_t)strlen(rslt_buf));

	if (!kp_getname_kernel) {
		E("kp_getname_kernel is NULL, not open file!\n");
		return -EIO;
	}
	vts_name = kp_getname_kernel(filepath);

	if (raw_file == NULL)
		raw_file = kp_file_open_name(vts_name, O_TRUNC|O_CREAT|O_RDWR,
					 0660);

	if (IS_ERR(raw_file)) {
		E("%s open file failed = %ld\n", __func__, PTR_ERR(raw_file));
		ret_val = -EIO;
		goto SAVE_DATA_ERR;
	}

	fs = get_fs();
	set_fs(get_ds());
	vfs_write(raw_file, rslt_buf,
		 g_1kind_raw_size * HX_CRITERIA_ITEM * sizeof(char), &pos);

	filp_close(raw_file, NULL);

	set_fs(fs);

SAVE_DATA_ERR:
	I("%s: End!\n", __func__);
	return ret_val;
}

static int hx_test_data_get(uint32_t RAW[], char *start_log, char *result,
			 int now_item)
{
	uint32_t i;

	uint32_t len = 0;
	char *testdata = NULL;
	uint32_t SZ_SIZE = g_1kind_raw_size;

	I("%s: Entering, Now type=%s!\n", __func__,
		 g_himax_inspection_mode[now_item]);

	testdata = kzalloc(sizeof(char) * SZ_SIZE, GFP_KERNEL);
	if (testdata == NULL) {
		E("%s: Memory allocation failed!\n", __func__);
		return MEM_ALLOC_FAIL;
	}

	len += snprintf((testdata + len), SZ_SIZE - len, "%s", start_log);
	for (i = 0; i < ic_data->HX_TX_NUM*ic_data->HX_RX_NUM; i++) {
		if (i > 1 && ((i + 1) % ic_data->HX_RX_NUM) == 0)
			len += snprintf((testdata + len), SZ_SIZE - len,
				 "%5d,\n", RAW[i]);
		else
			len += snprintf((testdata + len), SZ_SIZE - len,
				 "%5d,", RAW[i]);
	}
	len += snprintf((testdata + len), SZ_SIZE - len, "\n%s", result);

	memcpy(&g_rslt_data[g_rslt_data_len], testdata, len);
	g_rslt_data_len += len;
	I("%s: g_rslt_data_len=%d!\n", __func__, g_rslt_data_len);

	/*memcpy(&g_rslt_data[now_item * SZ_SIZE], testdata, SZ_SIZE);*/

	/* dbg */
	/* for(i = 0; i < SZ_SIZE; i++)
	 * {
	 *	I("0x%04X, ", g_rslt_data[i + (now_item * SZ_SIZE)]);
	 *	if(i > 0 && (i % 16 == 15))
	 *		PI("\n");
	 * }
	 */

	kfree(testdata);
	I("%s: End!\n", __func__);
	return NO_ERR;

}

static int himax_switch_mode_inspection(int mode)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4] = {0};

	I("%s: Entering\n", __func__);

	/*Stop Handshaking*/
	himax_in_parse_assign_cmd(sram_adr_rawdata_addr, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_write(tmp_addr, 4, tmp_data, 0);

	/*Switch Mode*/
	switch (mode) {
	case HIMAX_SORTING:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_SORTING_START;
		tmp_data[0] = PWD_SORTING_START;
		break;
	case HIMAX_OPEN:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_OPEN_START;
		tmp_data[0] = PWD_OPEN_START;
		break;
	case HIMAX_MICRO_OPEN:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_OPEN_START;
		tmp_data[0] = PWD_OPEN_START;
		break;
	case HIMAX_SHORT:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_SHORT_START;
		tmp_data[0] = PWD_SHORT_START;
		break;

	case HIMAX_GAPTEST_RAW:
	case HIMAX_RAWDATA:
	case HIMAX_BPN_RAWDATA:
	case HIMAX_SC:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_RAWDATA_START;
		tmp_data[0] = PWD_RAWDATA_START;
		break;

	case HIMAX_WEIGHT_NOISE:
	case HIMAX_ABS_NOISE:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_NOISE_START;
		tmp_data[0] = PWD_NOISE_START;
		break;

	case HIMAX_ACT_IDLE_RAWDATA:
	case HIMAX_ACT_IDLE_BPN_RAWDATA:
	case HIMAX_ACT_IDLE_NOISE:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_ACT_IDLE_START;
		tmp_data[0] = PWD_ACT_IDLE_START;
		break;

	case HIMAX_LPWUG_RAWDATA:
	case HIMAX_LPWUG_BPN_RAWDATA:
	case HIMAX_LPWUG_ABS_NOISE:
	case HIMAX_LPWUG_WEIGHT_NOISE:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_LPWUG_START;
		tmp_data[0] = PWD_LPWUG_START;
		break;
	case HIMAX_LPWUG_IDLE_RAWDATA:
	case HIMAX_LPWUG_IDLE_BPN_RAWDATA:
	case HIMAX_LPWUG_IDLE_NOISE:
		tmp_data[3] = 0x00; tmp_data[2] = 0x00;
		tmp_data[1] = PWD_LPWUG_IDLE_START;
		tmp_data[0] = PWD_LPWUG_IDLE_START;
		break;

	default:
		I("%s,Nothing to be done!\n", __func__);
		break;
	}

	if (g_core_fp.fp_assign_sorting_mode != NULL)
		g_core_fp.fp_assign_sorting_mode(tmp_data);
	I("%s: End of setting!\n", __func__);

	return 0;

}

static uint32_t himax_get_rawdata(uint32_t RAW[], uint32_t datalen, uint8_t checktype)
{
	uint8_t *tmp_rawdata;
	bool get_raw_rlst;
	uint8_t retry = 0;
	uint32_t i = 0;
	uint32_t j = 0;
	uint32_t index = 0;
	uint32_t Min_DATA = 0xFFFFFFFF;
	uint32_t Max_DATA = 0x00000000;

	/* We use two bytes to combine a value of rawdata.*/
	tmp_rawdata = kzalloc(sizeof(uint8_t) * (datalen * 2), GFP_KERNEL);
	if (tmp_rawdata == NULL) {
		E("%s: Memory allocation failed!\n", __func__);
		return HX_INSPECT_MEMALLCTFAIL;
	}

	while (retry < 200) {
		get_raw_rlst = g_core_fp.fp_get_DSRAM_data(tmp_rawdata, false);
		if (get_raw_rlst)
			break;
		retry++;
	}

	if (retry >= 200)
		goto DIRECT_END;

	/* Copy Data*/
	for (i = 0; i < ic_data->HX_TX_NUM*ic_data->HX_RX_NUM; i++) {
		RAW[i] = tmp_rawdata[(i * 2) + 1] * 256 + tmp_rawdata[(i * 2)];
	}

	for (j = 0; j < ic_data->HX_RX_NUM; j++) {
		if (j == 0)
			PI("      RX%2d", j + 1);
		else
			PI("  RX%2d", j + 1);
	}
	PI("\n");

	for (i = 0; i < ic_data->HX_TX_NUM; i++) {
		PI("TX%2d", i + 1);
		for (j = 0; j < ic_data->HX_RX_NUM; j++) {
			PI("%5d ", RAW[index]);
			if (RAW[index] > Max_DATA)
				Max_DATA = RAW[index];

			if (RAW[index] < Min_DATA)
				Min_DATA = RAW[index];

			index++;
		}
		PI("\n");
	}
	I("Max = %5d, Min = %5d\n", Max_DATA, Min_DATA);
DIRECT_END:
	kfree(tmp_rawdata);

	if (get_raw_rlst)
		return HX_INSPECT_OK;
	else
		return HX_INSPECT_EGETRAW;

}

static void himax_switch_data_type(uint8_t checktype)
{
	uint8_t datatype = 0x00;

	switch (checktype) {
	case HIMAX_SORTING:
		datatype = DATA_SORTING;
		break;
	case HIMAX_OPEN:
		datatype = DATA_OPEN;
		break;
	case HIMAX_MICRO_OPEN:
		datatype = DATA_MICRO_OPEN;
		break;
	case HIMAX_SHORT:
		datatype = DATA_SHORT;
		break;
	case HIMAX_RAWDATA:
	case HIMAX_BPN_RAWDATA:
	case HIMAX_SC:
	case HIMAX_GAPTEST_RAW:
		datatype = DATA_RAWDATA;
		break;

	case HIMAX_WEIGHT_NOISE:
	case HIMAX_ABS_NOISE:
		datatype = DATA_NOISE;
		break;
	case HIMAX_BACK_NORMAL:
		datatype = DATA_BACK_NORMAL;
		break;
	case HIMAX_ACT_IDLE_RAWDATA:
	case HIMAX_ACT_IDLE_BPN_RAWDATA:
		datatype = DATA_ACT_IDLE_RAWDATA;
		break;
	case HIMAX_ACT_IDLE_NOISE:
		datatype = DATA_ACT_IDLE_NOISE;
		break;

	case HIMAX_LPWUG_RAWDATA:
	case HIMAX_LPWUG_BPN_RAWDATA:
		datatype = DATA_LPWUG_RAWDATA;
		break;
	case HIMAX_LPWUG_WEIGHT_NOISE:
	case HIMAX_LPWUG_ABS_NOISE:
		datatype = DATA_LPWUG_NOISE;
		break;
	case HIMAX_LPWUG_IDLE_RAWDATA:
	case HIMAX_LPWUG_IDLE_BPN_RAWDATA:
		datatype = DATA_LPWUG_IDLE_RAWDATA;
		break;
	case HIMAX_LPWUG_IDLE_NOISE:
		datatype = DATA_LPWUG_IDLE_NOISE;
		break;

	default:
		E("Wrong type=%d\n", checktype);
		break;
	}
	g_core_fp.fp_diag_register_set(datatype, 0x00);
}

static void himax_bank_search_set(uint16_t Nframe, uint8_t checktype)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	/*skip frame 0x100070F4*/
	himax_in_parse_assign_cmd(addr_skip_frame, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);

	switch (checktype) {
	case HIMAX_ACT_IDLE_RAWDATA:
	case HIMAX_ACT_IDLE_BPN_RAWDATA:
	case HIMAX_ACT_IDLE_NOISE:
		tmp_data[0] = BS_ACT_IDLE;
		break;
	case HIMAX_LPWUG_RAWDATA:
	case HIMAX_LPWUG_BPN_RAWDATA:
	case HIMAX_LPWUG_ABS_NOISE:
	case HIMAX_LPWUG_WEIGHT_NOISE:
		tmp_data[0] = BS_LPWUG;
		break;
	case HIMAX_LPWUG_IDLE_RAWDATA:
	case HIMAX_LPWUG_IDLE_BPN_RAWDATA:
	case HIMAX_LPWUG_IDLE_NOISE:
		tmp_data[0] = BS_LPWUG_dile;
		break;
	case HIMAX_RAWDATA:
	case HIMAX_BPN_RAWDATA:
	case HIMAX_SC:
		tmp_data[0] = BS_RAWDATA;
		break;
	case HIMAX_WEIGHT_NOISE:
	case HIMAX_ABS_NOISE:
		tmp_data[0] = BS_NOISE;
		break;
	default:
		tmp_data[0] = BS_OPENSHORT;
		break;
	}
	g_core_fp.fp_register_write(tmp_addr, 4, tmp_data, 0);
}

static void himax_neg_noise_sup(uint8_t *data)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	/*0x10007FD8 Check support negative value or not */
	himax_in_parse_assign_cmd(addr_neg_noise_sup, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);

	if ((tmp_data[3] & 0x04) == 0x04) {
		himax_in_parse_assign_cmd(data_neg_noise, tmp_data, sizeof(tmp_data));
		data[2] = tmp_data[2]; data[3] = tmp_data[3];
	} else
		I("%s Not support negative noise\n", __func__);
}

static void himax_set_N_frame(uint16_t Nframe, uint8_t checktype)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	himax_bank_search_set(Nframe, checktype);

	/*IIR MAX - 0x10007294*/
	himax_in_parse_assign_cmd(fw_addr_set_frame_addr, tmp_addr, sizeof(tmp_addr));
	tmp_data[3] = 0x00; tmp_data[2] = 0x00;
	tmp_data[1] = (uint8_t)((Nframe & 0xFF00) >> 8);
	tmp_data[0] = (uint8_t)(Nframe & 0x00FF);
	g_core_fp.fp_register_write(tmp_addr, 4, tmp_data, 0);

	if (checktype == HIMAX_WEIGHT_NOISE ||
		checktype == HIMAX_ABS_NOISE ||
		checktype == HIMAX_LPWUG_WEIGHT_NOISE ||
		checktype == HIMAX_LPWUG_ABS_NOISE)
		himax_neg_noise_sup(tmp_data);

}

static void himax_get_noise_base(uint8_t checktype)/*Normal Threshold*/
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint8_t tmp_addr2[4];
	uint8_t tmp_data2[4];

	switch (checktype) {
	case HIMAX_WEIGHT_NOISE:
		himax_in_parse_assign_cmd(addr_normal_noise_thx, tmp_addr, sizeof(tmp_addr));
		break;
	case HIMAX_LPWUG_WEIGHT_NOISE:
		himax_in_parse_assign_cmd(addr_lpwug_noise_thx, tmp_addr, sizeof(tmp_addr));
		break;
	default:
		I("%s Not support type\n", __func__);
	}

	himax_in_parse_assign_cmd(addr_weight_b, tmp_addr2, sizeof(tmp_addr2));
	g_core_fp.fp_register_read(tmp_addr2, 4, tmp_data2, false);
	tmp_data2[1] = tmp_data2[1] >> 4;
	if (tmp_data2[1] == 0)
		tmp_data2[1] = 1;

	/*normal : 0x1000708F, LPWUG:0x10007093*/
	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
	NOISEMAX = tmp_data[3] * tmp_data2[1];

	himax_in_parse_assign_cmd(addr_recal_thx, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
	g_recal_thx = tmp_data[2] * tmp_data2[1];	/*0x10007092*/
	I("%s: NOISEMAX=%d, g_recal_thx = %d\n", __func__,
		NOISEMAX, g_recal_thx);
}

static uint16_t himax_get_palm_num(void)/*Palm Number*/
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint16_t palm_num;

	himax_in_parse_assign_cmd(addr_palm_num, tmp_addr, sizeof(tmp_addr));
	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
	palm_num = tmp_data[3];/*0x100070AB*/
	I("%s: palm_num = %d ", __func__, palm_num);

	return palm_num;
}

static int himax_get_noise_weight_test(uint8_t checktype)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];
	uint16_t weight = 0;
	uint16_t value = 0;

	himax_in_parse_assign_cmd(addr_weight_sup, tmp_addr, sizeof(tmp_addr));

	/*0x100072C8 weighting value*/
	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
	if (tmp_data[3] != tmp_addr[1] || tmp_data[2] != tmp_addr[0])
		return FW_NOT_READY;

	value = (tmp_data[1] << 8) | tmp_data[0];
	I("%s: value = %d, %d, %d ", __func__, value, tmp_data[2], tmp_data[3]);

	switch (checktype) {
	case HIMAX_WEIGHT_NOISE:
		himax_in_parse_assign_cmd(addr_normal_weight_a, tmp_addr, sizeof(tmp_addr));
		break;
	case HIMAX_LPWUG_WEIGHT_NOISE:
		himax_in_parse_assign_cmd(addr_lpwug_weight_a, tmp_addr, sizeof(tmp_addr));
		break;
	default:
		I("%s Not support type\n", __func__);
	}

	/*Normal:0x1000709C, LPWUG:0x100070A0 weighting threshold*/
	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
	weight = tmp_data[0];

	himax_in_parse_assign_cmd(addr_weight_b, tmp_addr, sizeof(tmp_addr));

	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
	weight = (tmp_data[1] & 0x0F) * weight;	/*0x10007095 weighting threshold*/
	I("%s: weight = %d ", __func__, weight);

	if (value > weight)
		return ERR_TEST_FAIL;
	else
		return 0;
}

static uint32_t himax_check_mode(uint8_t checktype)
{
	uint8_t tmp_data[4] = {0};
	uint8_t wait_pwd[2] = {0};

	switch (checktype) {
	case HIMAX_SORTING:
		wait_pwd[0] = PWD_SORTING_END;
		wait_pwd[1] = PWD_SORTING_END;
		break;
	case HIMAX_OPEN:
		wait_pwd[0] = PWD_OPEN_END;
		wait_pwd[1] = PWD_OPEN_END;
		break;
	case HIMAX_MICRO_OPEN:
		wait_pwd[0] = PWD_OPEN_END;
		wait_pwd[1] = PWD_OPEN_END;
		break;
	case HIMAX_SHORT:
		wait_pwd[0] = PWD_SHORT_END;
		wait_pwd[1] = PWD_SHORT_END;
		break;
	case HIMAX_RAWDATA:
	case HIMAX_BPN_RAWDATA:
	case HIMAX_SC:
	case HIMAX_GAPTEST_RAW:
		wait_pwd[0] = PWD_RAWDATA_END;
		wait_pwd[1] = PWD_RAWDATA_END;
		break;

	case HIMAX_WEIGHT_NOISE:
	case HIMAX_ABS_NOISE:
		wait_pwd[0] = PWD_NOISE_END;
		wait_pwd[1] = PWD_NOISE_END;
		break;

	case HIMAX_ACT_IDLE_RAWDATA:
	case HIMAX_ACT_IDLE_BPN_RAWDATA:
	case HIMAX_ACT_IDLE_NOISE:
		wait_pwd[0] = PWD_ACT_IDLE_END;
		wait_pwd[1] = PWD_ACT_IDLE_END;
		break;

	case HIMAX_LPWUG_RAWDATA:
	case HIMAX_LPWUG_BPN_RAWDATA:
	case HIMAX_LPWUG_ABS_NOISE:
	case HIMAX_LPWUG_WEIGHT_NOISE:
		wait_pwd[0] = PWD_LPWUG_END;
		wait_pwd[1] = PWD_LPWUG_END;
		break;
	case HIMAX_LPWUG_IDLE_RAWDATA:
	case HIMAX_LPWUG_IDLE_BPN_RAWDATA:
	case HIMAX_LPWUG_IDLE_NOISE:
		wait_pwd[0] = PWD_LPWUG_IDLE_END;
		wait_pwd[1] = PWD_LPWUG_IDLE_END;
		break;

	default:
		E("Wrong type=%d\n", checktype);
		break;
	}

	if (g_core_fp.fp_check_sorting_mode != NULL)
		g_core_fp.fp_check_sorting_mode(tmp_data);

	if ((wait_pwd[0] == tmp_data[0]) && (wait_pwd[1] == tmp_data[1])) {
		I("Change to mode=%s\n", g_himax_inspection_mode[checktype]);
		return 0;
	} else {
		return 1;
	}
}

#define TEMP_LOG "%s: %s, tmp_data[0]=%x,tmp_data[1]=%x,tmp_data[2]=%x,tmp_data[3]=%x\n"

static uint32_t himax_wait_sorting_mode(uint8_t checktype)
{
	uint8_t tmp_addr[4] = {0};
	uint8_t tmp_data[4] = {0};
	uint8_t wait_pwd[2] = {0};
	int count = 0;

	switch (checktype) {
	case HIMAX_SORTING:
		wait_pwd[0] = PWD_SORTING_END;
		wait_pwd[1] = PWD_SORTING_END;
		break;
	case HIMAX_OPEN:
		wait_pwd[0] = PWD_OPEN_END;
		wait_pwd[1] = PWD_OPEN_END;
		break;
	case HIMAX_MICRO_OPEN:
		wait_pwd[0] = PWD_OPEN_END;
		wait_pwd[1] = PWD_OPEN_END;
		break;
	case HIMAX_SHORT:
		wait_pwd[0] = PWD_SHORT_END;
		wait_pwd[1] = PWD_SHORT_END;
		break;
	case HIMAX_RAWDATA:
	case HIMAX_BPN_RAWDATA:
	case HIMAX_SC:
	case HIMAX_GAPTEST_RAW:
		wait_pwd[0] = PWD_RAWDATA_END;
		wait_pwd[1] = PWD_RAWDATA_END;
		break;
	case HIMAX_WEIGHT_NOISE:
	case HIMAX_ABS_NOISE:
		wait_pwd[0] = PWD_NOISE_END;
		wait_pwd[1] = PWD_NOISE_END;
		break;
	case HIMAX_ACT_IDLE_RAWDATA:
	case HIMAX_ACT_IDLE_BPN_RAWDATA:
	case HIMAX_ACT_IDLE_NOISE:
		wait_pwd[0] = PWD_ACT_IDLE_END;
		wait_pwd[1] = PWD_ACT_IDLE_END;
		break;

	case HIMAX_LPWUG_RAWDATA:
	case HIMAX_LPWUG_BPN_RAWDATA:
	case HIMAX_LPWUG_ABS_NOISE:
	case HIMAX_LPWUG_WEIGHT_NOISE:
		wait_pwd[0] = PWD_LPWUG_END;
		wait_pwd[1] = PWD_LPWUG_END;
		break;
	case HIMAX_LPWUG_IDLE_RAWDATA:
	case HIMAX_LPWUG_IDLE_BPN_RAWDATA:
	case HIMAX_LPWUG_IDLE_NOISE:
		wait_pwd[0] = PWD_LPWUG_IDLE_END;
		wait_pwd[1] = PWD_LPWUG_IDLE_END;
		break;

	default:
		I("No Change Mode and now type=%d\n", checktype);
		break;
	}

	do {
		if (g_core_fp.fp_check_sorting_mode != NULL)
			g_core_fp.fp_check_sorting_mode(tmp_data);
		if ((wait_pwd[0] == tmp_data[0]) &&
			(wait_pwd[1] == tmp_data[1]))
			return HX_INSPECT_OK;

		himax_in_parse_assign_cmd(fw_addr_chk_fw_status, tmp_addr, sizeof(tmp_addr));
		g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
		I(TEMP_LOG, __func__, "0x900000A8",
			tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

		himax_in_parse_assign_cmd(fw_addr_flag_reset_event, tmp_addr, sizeof(tmp_addr));
		g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
		I(TEMP_LOG, __func__, "0x900000E4",
			tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

		himax_in_parse_assign_cmd(fw_addr_fw_dbg_msg_addr, tmp_addr, sizeof(tmp_addr));
		g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
		I(TEMP_LOG, __func__, "0x10007F40",
			tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);
		I("Now retry %d times!\n", count);
		msleep(50);
	} while (count++ < 50);

	return HX_INSPECT_ESWITCHMODE;
}

static int hx_turn_on_mp_func(int on)
{
	int rslt = 0;
	int retry = 3;
	uint8_t tmp_addr[4] = {0};
	uint8_t tmp_data[4] = {0};
	uint8_t tmp_read[4] = {0};

	if (strcmp(HX_83102D_SERIES_PWON, private_ts->chip_name) == 0) { /* Only HX83102D needs this flow */
		I("%s: need to enter Mp mode!\n", __func__);
		himax_in_parse_assign_cmd(addr_ctrl_mpap_ovl, tmp_addr, sizeof(tmp_addr));
		if (on) {
			I("%s : Turn on!\n", __func__);
			himax_in_parse_assign_cmd(PWD_TURN_ON_MPAP_OVL, tmp_data, sizeof(tmp_data));
			do {
				g_core_fp.fp_register_write(tmp_addr, 4, tmp_data, 0);
				usleep_range(10000, 10001);
				g_core_fp.fp_register_read(tmp_addr, 4, tmp_read, false);
				I("%s: now read[2]=0x%02X, read[1]=0x%02X, read[0]=0x%02X!\n",
				__func__, tmp_read[2], tmp_read[1], tmp_read[0]);
				retry--;
			} while (((retry > 0) && (tmp_read[2] != tmp_data[2] && tmp_read[1] != tmp_data[1] && tmp_read[0] != tmp_data[0])));
		} else {
			I("%s : Turn off!\n", __func__);
			himax_in_parse_assign_cmd(ic_cmd_rst, tmp_data, sizeof(tmp_data));
			do {
				g_core_fp.fp_register_write(tmp_addr, 4, tmp_data, 0);
				usleep_range(10000, 10001);
				g_core_fp.fp_register_read(tmp_addr, 4, tmp_read, false);
				I("%s: now read[2]=0x%02X, read[1]=0x%02X, read[0]=0x%02X!\n",
				__func__, tmp_read[2], tmp_read[1], tmp_read[0]);
				retry--;
			} while ((retry > 0) && (tmp_read[2] != tmp_data[2] && tmp_read[1] != tmp_data[1] && tmp_read[0] != tmp_data[0]));
		}
	} else {
		I("%s:Nothing to be done!\n", __func__);
	}
	return rslt;
}

/*	 HX_GAP START */
/* gap test function */
/* extern int himax_write_to_ic_flash_flow(uint32_t start_addr,uint32_t
 *	 *write_data,uint32_t write_len);
 */

static int himax_gap_test_vertical_setting(void)
{
	g_gap_vertical_part = kcalloc(g_gap_vertical_partial,
				sizeof(int), GFP_KERNEL);
	if (g_gap_vertical_part == NULL) {
		E("%s: Memory allocation failed!\n", __func__);
		return MEM_ALLOC_FAIL;
	}
	g_gap_vertical_part[0] = 0;
	g_gap_vertical_part[1] = ic_data->HX_TX_NUM / 4 * 1;
	g_gap_vertical_part[2] = ic_data->HX_TX_NUM / 4 * 2;
	g_gap_vertical_part[3] = ic_data->HX_TX_NUM / 4 * 3;

	return NO_ERR;
}

static void himax_cal_gap_data_vertical(int start, int end_idx, int direct,
				uint32_t *org_raw, uint32_t *result_raw)
{
	int i = 0;
	int rx_num = ic_data->HX_RX_NUM;

	I("%s:start=%d\n", __func__, start);
	I("%s:end_idx=%d\n", __func__, end_idx);
	for (i = start; i < (start + rx_num*end_idx); i++) {
		if (direct == 0) { /* up - down */
			if (i < start+rx_num)
				result_raw[i] = 0;
			else
				result_raw[i] = ABS(((int)org_raw[i-rx_num] - (int)org_raw[i]) * 100 / (int)org_raw[i]);

		} else { /* down - up */
			if (i > (start + rx_num*(end_idx-1)-1))
				result_raw[i] = 0;
			else
				result_raw[i] = ABS(((int)org_raw[i+rx_num] - (int)org_raw[i]) * 100 / (int)org_raw[i]);

		}
	}
}

static int himax_gap_test_vertical_raw(int test_type, uint32_t *org_raw)
{
	int i_partial = 0;
	int tmp_start = 0;
	int tmp_end_idx = 0;
	uint32_t *result_raw;
	int i = 0;
	int ret_val = NO_ERR;

	int tx_num = ic_data->HX_TX_NUM;
	int rx_num = ic_data->HX_RX_NUM;

	if (himax_gap_test_vertical_setting())
		return MEM_ALLOC_FAIL;

	I("Print vertical ORG RAW\n");
	for (i = 0; i < tx_num*rx_num; i++) {
		I("%04d,", org_raw[i]);
		if (i > 0 && i%rx_num == (rx_num-1))
			I("\n");
	}

	result_raw = kcalloc(tx_num*rx_num, sizeof(uint32_t), GFP_KERNEL);
	if (result_raw == NULL) {
		E("%s: Memory allocation failed!\n", __func__);
		goto alloc_result_raw_failed;
	}

	for (i_partial = 0; i_partial < g_gap_vertical_partial; i_partial++) {

		tmp_start	= g_gap_vertical_part[i_partial]*rx_num;
		if (i_partial+1 == g_gap_vertical_partial)
			tmp_end_idx = tx_num - g_gap_vertical_part[i_partial];
		else
			tmp_end_idx = g_gap_vertical_part[i_partial+1] -
				 g_gap_vertical_part[i_partial];

		if (i_partial % 2 == 0)
			himax_cal_gap_data_vertical(tmp_start, tmp_end_idx, 0,
						org_raw, result_raw);
		else
			himax_cal_gap_data_vertical(tmp_start, tmp_end_idx, 1,
						org_raw, result_raw);

	}

	I("Print Vertical New RAW\n");
	for (i = 0; i < tx_num*rx_num; i++) {
		I("%04d,", result_raw[i]);
		if (i > 0 && i%rx_num == (rx_num-1))
			I("\n");
	}

	for (i = 0; i < tx_num*rx_num; i++) {
		if (result_raw[i] < g_inspection_criteria[IDX_GAP_VER_RAWMIN][i]
		 &&
		 result_raw[i] > g_inspection_criteria[IDX_GAP_VER_RAWMAX][i]) {
			ret_val = NO_ERR - i;
			break;
		}
	}

	/* himax_write_to_ic_flash_flow(0x1A000,result_raw,tx_num*rx_num); */
	kfree(result_raw);
alloc_result_raw_failed:
	kfree(g_gap_vertical_part);
	g_gap_vertical_part = NULL;

	return ret_val;
}

static int himax_gap_test_horizontal_setting(void)
{
	g_gap_horizontal_part = kcalloc(g_gap_horizontal_partial,
				sizeof(int), GFP_KERNEL);
	if (g_gap_horizontal_part == NULL) {
		E("%s: Memory allocation failed!\n", __func__);
		return MEM_ALLOC_FAIL;
	}
	g_gap_horizontal_part[0] = 0;

	return NO_ERR;
}

static void himax_cal_gap_data_horizontal(int start, int end_idx, int direct,
				uint32_t *org_raw, uint32_t *result_raw)
{
	int i = 0;
	int j = 0;
	int rx_num = ic_data->HX_RX_NUM;
	int tx_num = ic_data->HX_TX_NUM;

	I("start=%d\n", start);
	I("end_idx=%d\n", end_idx);
	for (j = 0; j < tx_num; j++) {
		for (i = (start + (j*rx_num));
			i < (start + (j*rx_num) + end_idx); i++) {
			/* left - right */
			if (direct == 0) {
				if (i == (start + (j*rx_num)))
					result_raw[i] = 0;
				else
					result_raw[i] =
						ABS(((int)org_raw[i-1] - (int)org_raw[i]) * 100 / (int)org_raw[i]);

			} else { /* right - left */
				if (i == ((start + (j*rx_num) + end_idx) - 1))
					result_raw[i] = 0;
				else
					result_raw[i] =
						ABS(((int)org_raw[i+1] - (int)org_raw[i]) * 100 / (int)org_raw[i]);
			}
		}
	}
}

static int himax_gap_test_honrizontal_raw(int test_type, uint32_t *raw)
{
	int rx_num = ic_data->HX_RX_NUM;
	int tx_num = ic_data->HX_TX_NUM;
	int tmp_start = 0;
	int tmp_end_idx = 0;
	int i_partial = 0;
	uint32_t *result_raw;
	int i = 0;
	int ret_val = NO_ERR;

	if (himax_gap_test_horizontal_setting())
		return MEM_ALLOC_FAIL;

	result_raw = kcalloc(ic_data->HX_TX_NUM * ic_data->HX_RX_NUM,
			sizeof(uint32_t), GFP_KERNEL);
	if (result_raw == NULL) {
		E("%s: Memory allocation failed!\n", __func__);
		goto alloc_result_raw_failed;
	}

	I("Print Horizontal ORG RAW\n");
	for (i = 0; i < tx_num*rx_num; i++) {
		I("%04d,", raw[i]);
		if (i > 0 && i%rx_num == (rx_num-1))
			I("\n");
	}

	for (i_partial = 0; i_partial < g_gap_horizontal_partial; i_partial++) {
		tmp_start	= g_gap_horizontal_part[i_partial];
		if (i_partial+1 == g_gap_horizontal_partial)
			tmp_end_idx = rx_num - g_gap_horizontal_part[i_partial];
		else
			tmp_end_idx = g_gap_horizontal_part[i_partial+1] -
				g_gap_horizontal_part[i_partial];

		if (i_partial % 2 == 0)
			himax_cal_gap_data_horizontal(tmp_start, tmp_end_idx,
						0, raw, result_raw);
		else
			himax_cal_gap_data_horizontal(tmp_start, tmp_end_idx,
						1, raw, result_raw);

	}
	I("Print Horizontal New RAW\n");
	for (i = 0; i < tx_num*rx_num; i++) {
		I("%04d,", result_raw[i]);
		if (i > 0 && i%rx_num == (rx_num-1))
			I("\n");
	}

	for (i = 0; i < tx_num*rx_num; i++) {
		if (result_raw[i] < g_inspection_criteria[IDX_GAP_HOR_RAWMIN][i]
		&&
		result_raw[i] > g_inspection_criteria[IDX_GAP_HOR_RAWMAX][i]) {
			ret_val = NO_ERR - i;
			break;
		}
	}

	/* himax_write_to_ic_flash_flow(0x1A800,result_raw,tx_num*rx_num); */
	kfree(result_raw);
alloc_result_raw_failed:
	kfree(g_gap_horizontal_part);
	g_gap_horizontal_part = NULL;

	return ret_val;
}

static uint32_t himax_data_campare(uint8_t checktype, uint32_t *RAW, int ret_val)
{
	int i = 0;
	int idx_max = 0;
	int idx_min = 0;
	int block_num = ic_data->HX_TX_NUM*ic_data->HX_RX_NUM;
	uint16_t palm_num = 0;
	uint16_t noise_count = 0;

	switch (checktype) {
	case HIMAX_SORTING:
		idx_min = IDX_SORTMIN;
		break;
	case HIMAX_OPEN:
		idx_max = IDX_OPENMAX;
		idx_min = IDX_OPENMIN;
		break;

	case HIMAX_MICRO_OPEN:
		idx_max = IDX_M_OPENMAX;
		idx_min = IDX_M_OPENMIN;
		break;

	case HIMAX_SHORT:
		idx_max = IDX_SHORTMAX;
		idx_min = IDX_SHORTMIN;
		break;

	case HIMAX_RAWDATA:
		idx_max = IDX_RAWMAX;
		idx_min = IDX_RAWMIN;
		break;

	case HIMAX_BPN_RAWDATA:
		idx_max = IDX_BPN_RAWMAX;
		idx_min = IDX_BPN_RAWMIN;
		break;
	case HIMAX_SC:
		idx_max = IDX_SCMAX;
		idx_min = IDX_SCMIN;
		break;
	case HIMAX_WEIGHT_NOISE:
		idx_max = IDX_WT_NOISEMAX;
		idx_min = IDX_WT_NOISEMIN;
		break;
	case HIMAX_ABS_NOISE:
		idx_max = IDX_ABS_NOISEMAX;
		idx_min = IDX_ABS_NOISEMIN;
		break;
	case HIMAX_GAPTEST_RAW:
		break;

	case HIMAX_ACT_IDLE_RAWDATA:
		idx_max = IDX_ACT_IDLE_RAWDATA_MAX;
		idx_min = IDX_ACT_IDLE_RAWDATA_MIN;
		break;

	case HIMAX_ACT_IDLE_BPN_RAWDATA:
		idx_max = IDX_ACT_IDLE_RAW_BPN_MAX;
		idx_min = IDX_ACT_IDLE_RAW_BPN_MIN;
		break;

	case HIMAX_ACT_IDLE_NOISE:
		idx_max = IDX_ACT_IDLE_NOISE_MAX;
		idx_min = IDX_ACT_IDLE_NOISE_MIN;
		break;

	case HIMAX_LPWUG_RAWDATA:
		idx_max = IDX_LPWUG_RAWDATA_MAX;
		idx_min = IDX_LPWUG_RAWDATA_MIN;
		break;

	case HIMAX_LPWUG_BPN_RAWDATA:
		idx_max = IDX_LPWUG_RAW_BPN_MAX;
		idx_min = IDX_LPWUG_RAW_BPN_MIN;
		break;

	case HIMAX_LPWUG_WEIGHT_NOISE:
		idx_max = IDX_LPWUG_WT_NOISEMAX;
		idx_min = IDX_LPWUG_WT_NOISEMIN;
		break;

	case HIMAX_LPWUG_ABS_NOISE:
		idx_max = IDX_LPWUG_NOISE_ABS_MAX;
		idx_min = IDX_LPWUG_NOISE_ABS_MIN;
		break;

	case HIMAX_LPWUG_IDLE_RAWDATA:
		idx_max = IDX_LPWUG_IDLE_RAWDATA_MAX;
		idx_min = IDX_LPWUG_IDLE_RAWDATA_MIN;
		break;

	case HIMAX_LPWUG_IDLE_BPN_RAWDATA:
		idx_max = IDX_LPWUG_IDLE_RAW_BPN_MAX;
		idx_min = IDX_LPWUG_IDLE_RAW_BPN_MIN;
		break;

	case HIMAX_LPWUG_IDLE_NOISE:
		idx_max = IDX_LPWUG_IDLE_NOISE_MAX;
		idx_min = IDX_LPWUG_IDLE_NOISE_MIN;
		break;

	default:
		E("Wrong type=%d\n", checktype);
		break;
	}

	/*data process*/
	switch (checktype) {
	case HIMAX_SORTING:
		for (i = 0; i < block_num; i++)
			g_inspection_criteria[idx_max][i] = 999999;
		break;
	case HIMAX_BPN_RAWDATA:
	case HIMAX_ACT_IDLE_BPN_RAWDATA:
	case HIMAX_LPWUG_BPN_RAWDATA:
	case HIMAX_LPWUG_IDLE_BPN_RAWDATA:
		for (i = 0; i < block_num; i++)
			RAW[i] = (int)RAW[i] * 100 / g_dc_max;
		break;
	case HIMAX_SC:
		for (i = 0; i < block_num; i++) {
			RAW[i] = ((int)RAW[i]-g_inspection_criteria[IDX_SC_GOLDEN][i]) * 100
			/ g_inspection_criteria[IDX_SC_GOLDEN][i];
		}
		break;
	}

	/*data campare*/
	switch (checktype) {
	case HIMAX_GAPTEST_RAW:
		if (
		himax_gap_test_vertical_raw(HIMAX_GAPTEST_RAW, RAW)
		!= NO_ERR) {
			E("%s: HIMAX_GAPTEST_RAW FAIL\n", __func__);
			ret_val |= 1 << (checktype + ERR_SFT);
			break;
		}
		if (himax_gap_test_honrizontal_raw(HIMAX_GAPTEST_RAW,
			RAW) != NO_ERR) {
			E("%s: HIMAX_GAPTEST_RAW FAIL\n", __func__);
			ret_val |= 1 << (checktype + ERR_SFT);
			break;
		}
		break;

	case HIMAX_WEIGHT_NOISE:
	case HIMAX_LPWUG_WEIGHT_NOISE:
		noise_count = 0;
		himax_get_noise_base(checktype);
		palm_num = himax_get_palm_num();
		for (i = 0; i < (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM); i++) {
			if ((int)RAW[i] > NOISEMAX)
				noise_count++;
		}
		I("noise_count=%d\n", noise_count);
		if (noise_count > palm_num) {
			E("%s: noise test FAIL\n", __func__);
			ret_val |= 1 << (checktype + ERR_SFT);
			break;
		}
		snprintf(g_start_log, 256 * sizeof(char), "\n Threshold = %d\n", NOISEMAX);
		/*Check weightingt*/
		if (himax_get_noise_weight_test(checktype) < 0) {
			I("%s: %s FAIL %X\n", __func__, g_himax_inspection_mode[checktype], ret_val);
			ret_val |= 1 << (checktype + ERR_SFT);
			break;
		}

		/*Check negative side noise*/
		for (i = 0; i < block_num; i++) {
			if ((int)RAW[i] >	(g_inspection_criteria[idx_max][i] * NOISEMAX / 100) ||
			(int)RAW[i] < (g_inspection_criteria[idx_min][i] * g_recal_thx / 100)) {
				E(FAIL_IN_INDEX, __func__, g_himax_inspection_mode[checktype], i);
				ret_val |= 1 << (checktype + ERR_SFT);
				break;
			}
		}
		break;

	case HIMAX_SORTING:
	case HIMAX_OPEN:
	case HIMAX_MICRO_OPEN:
	case HIMAX_SHORT:
	case HIMAX_RAWDATA:
	case HIMAX_BPN_RAWDATA:
	case HIMAX_SC:
	case HIMAX_ABS_NOISE:
	case HIMAX_ACT_IDLE_RAWDATA:
	case HIMAX_ACT_IDLE_BPN_RAWDATA:
	case HIMAX_ACT_IDLE_NOISE:
	case HIMAX_LPWUG_RAWDATA:
	case HIMAX_LPWUG_BPN_RAWDATA:
	case HIMAX_LPWUG_ABS_NOISE:
	case HIMAX_LPWUG_IDLE_RAWDATA:
	case HIMAX_LPWUG_IDLE_BPN_RAWDATA:
	case HIMAX_LPWUG_IDLE_NOISE:
		for (i = 0; i < block_num; i++) {
			if ((int)RAW[i] > g_inspection_criteria[idx_max][i]
			||
			 (int)RAW[i] < g_inspection_criteria[idx_min][i]) {
				E(FAIL_IN_INDEX, __func__, g_himax_inspection_mode[checktype], i);
				ret_val |= 1 << (checktype + ERR_SFT);
				break;
			}
		}
		break;

	default:
		E("Wrong type=%d\n", checktype);
		break;
	}

	I("%s: %s %s\n", __func__, g_himax_inspection_mode[checktype],
	(ret_val == HX_INSPECT_OK)?"PASS":"FAIL");

	return ret_val;
}

static int himax_get_max_dc(void)
{
	uint8_t tmp_data[DATA_LEN_4];
	uint8_t tmp_addr[DATA_LEN_4];
	int dc_max = 0;

	himax_in_parse_assign_cmd(addr_max_dc, tmp_addr, sizeof(tmp_addr));

	g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, tmp_data, 0);
	I("%s: tmp_data[0]=%x,tmp_data[1]=%x\n", __func__, tmp_data[0], tmp_data[1]);

	dc_max = tmp_data[1] << 8 | tmp_data[0];
	I("%s: dc max = %d\n", __func__, dc_max);
	return dc_max;
}

/*	 HX_GAP END*/
static uint32_t mpTestFunc(uint8_t checktype, uint32_t datalen)
{
	uint32_t len = 0;
	uint32_t RAW[datalen];
	int n_frame = 0;
	uint32_t ret_val = HX_INSPECT_OK;

	memset(RAW, 0, datalen*sizeof(RAW[0]));

	/*uint16_t* pInspectGridData = &gInspectGridData[0];*/
	/*uint16_t* pInspectNoiseData = &gInspectNoiseData[0];*/
	I("Now Check type = %d\n", checktype);

	if (himax_check_mode(checktype)) {
		/*himax_check_mode(checktype);*/

		I("Need Change Mode ,target=%s\n",
		g_himax_inspection_mode[checktype]);

		g_core_fp.fp_sense_off(true);
		hx_turn_on_mp_func(1);

#ifndef HX_ZERO_FLASH
		if (g_core_fp.fp_reload_disable != NULL)
			g_core_fp.fp_reload_disable(1);
#endif

		himax_switch_mode_inspection(checktype);

		switch (checktype) {
		case HIMAX_WEIGHT_NOISE:
		case HIMAX_ABS_NOISE:
			n_frame = NOISEFRAME;
			break;
		case HIMAX_ACT_IDLE_RAWDATA:
		case HIMAX_ACT_IDLE_NOISE:
		case HIMAX_ACT_IDLE_BPN_RAWDATA:
			n_frame = NORMAL_IDLE_RAWDATA_NOISEFRAME;
			break;
		case HIMAX_LPWUG_RAWDATA:
		case HIMAX_LPWUG_BPN_RAWDATA:
			n_frame = LPWUG_RAWDATAFRAME;
			break;
		case HIMAX_LPWUG_WEIGHT_NOISE:
		case HIMAX_LPWUG_ABS_NOISE:
			n_frame = LPWUG_NOISEFRAME;
			break;
		case HIMAX_LPWUG_IDLE_RAWDATA:
		case HIMAX_LPWUG_IDLE_BPN_RAWDATA:
			n_frame = LPWUG_IDLE_RAWDATAFRAME;
			break;
		case HIMAX_LPWUG_IDLE_NOISE:
			n_frame = LPWUG_IDLE_NOISEFRAME;
			break;
		default:
			n_frame = OTHERSFRAME;
		}
		himax_set_N_frame(n_frame, checktype);

		g_core_fp.fp_sense_on(1);

	}

	ret_val |= himax_wait_sorting_mode(checktype);
	if (ret_val) {
		E("%s: himax_wait_sorting_mode FAIL\n", __func__);
		ret_val |= (1 << (checktype + ERR_SFT));
		return ret_val;
	}
	himax_switch_data_type(checktype);

	ret_val |= himax_get_rawdata(RAW, datalen, 0xFF);
	if (ret_val) {
		E("%s: himax_get_rawdata FAIL\n", __func__);
		ret_val |= (1 << (checktype + ERR_SFT));
		return ret_val;
	}

	/*get Max DC from FW*/
	g_dc_max = himax_get_max_dc();

	/* back to normal */
	himax_switch_data_type(HIMAX_BACK_NORMAL);

	I("%s: Init OK, start to test!\n", __func__);

	len += snprintf(g_start_log+len, 256 * sizeof(char), "\n%s%s\n",
		g_himax_inspection_mode[checktype], ": data as follow!\n");

	ret_val |= himax_data_campare(checktype, RAW, ret_val);

	himax_get_arraydata_edge(RAW);

	len += snprintf(g_start_log + len, 256 * sizeof(char), "\n arraydata_min1 = %d,", arraydata_min1);
	len += snprintf(g_start_log + len, 256 * sizeof(char)-len, "  arraydata_min2 = %d,", arraydata_min2);
	len += snprintf(g_start_log + len, 256 * sizeof(char)-len, "  arraydata_min3 = %d,", arraydata_min3);
	len += snprintf(g_start_log + len, 256 * sizeof(char), "\n arraydata_max1 = %d,", arraydata_max1);
	len += snprintf(g_start_log + len, 256 * sizeof(char)-len, "  arraydata_max2 = %d,", arraydata_max2);
	len += snprintf(g_start_log + len, 256 * sizeof(char)-len, "  arraydata_max3 = %d\n", arraydata_max3);

	if (!ret_val) {/*PASS*/
		snprintf(g_rslt_log, 256 * sizeof(char), "\n%s%s\n",
	  g_himax_inspection_mode[checktype], " Test Pass!\n");
		I("pass write log\n");
	} else {/*FAIL*/
		snprintf(g_rslt_log, 256 * sizeof(char), "\n%s%s\n",
	  g_himax_inspection_mode[checktype], " Test Fail!\n");
	  I("fail write log\n");
	}

	hx_test_data_get(RAW, g_start_log, g_rslt_log, checktype);

	return ret_val;
}

/* claculate 10's power function */
static int himax_power_cal(int pow, int number)
{
	int i = 0;
	int result = 1;

	for (i = 0; i < pow; i++)
		result *= 10;
	result = result * number;

	return result;

}

/* String to int */
static int hiamx_parse_str2int(char *str)
{
	int i = 0;
	int temp_cal = 0;
	int result = 0;
	unsigned int str_len = strlen(str);
	int negtive_flag = 0;

	for (i = 0; i < str_len; i++) {
		if (str[i] != '-' && str[i] > '9' && str[i] < '0') {
			E("%s: Parsing fail!\n", __func__);
			result = -9487;
			negtive_flag = 0;
			break;
		}
		if (str[i] == '-') {
			negtive_flag = 1;
			continue;
		}
		temp_cal = str[i] - '0';
		result += himax_power_cal(str_len-i-1, temp_cal);
		/* str's the lowest char is the number's the highest number
		 * So we should reverse this number before using the power
		 * function
		 * -1: starting number is from 0 ex:10^0 = 1,10^1=10
		 */
	}

	if (negtive_flag == 1)
		result = 0 - result;

	return result;
}


/* Get sub-string from original string by using some characters
 * return size of result
 */
static int himax_saperate_comma(char *str_data, int str_size,
				char **result, int item_str_size)
{
	int count = 0;
	int str_count = 0; /* now string*/
	int char_count = 0; /* now char count in string*/

	do {
		switch (str_data[count]) {
		case ASCII_COMMA:
		case ACSII_SPACE:
		case ASCII_CR:
		case ASCII_LF:
			count++;
			/* If end of line as above condifiton,
			 * differencing the count of char.
			 * If char_count != 0
			 * it's meaning this string is parsing over .
			 * The Next char is belong to next string
			 */
			if (char_count != 0) {
				char_count = 0;
				str_count++;
			}
			break;
		default:
			result[str_count][char_count++] =
				str_data[count];
			count++;
			break;
		}
	} while (count < str_size && str_count < item_str_size);

	return str_count;
}

static int hx_diff_str(char *str1, char *str2)
{
	int i = 0;
	int result = 0; /* zero is all same, non-zero is not same index*/
	int str1_len = strlen(str1);
	int str2_len = strlen(str2);

	if (str1_len != str2_len) {
		if (private_ts->debug_log_level & BIT(4))
			I("%s:Size different!\n", __func__);
		return LENGTH_FAIL;
	}

	for (i = 0; i < str1_len; i++) {
		if (str1[i] != str2[i]) {
			result = i+1;
			/*I("%s: different in %d!\n", __func__, result);*/
			return result;
		}
	}

	return result;
}

/* get idx of criteria when parsing file */
int hx_find_crtra_id(char *input)
{
	int i = 0;
	int result = 0;

	for (i = 0 ; i < HX_CRITERIA_SIZE ; i++) {
		if (hx_diff_str(g_hx_inspt_crtra_name[i], input) == 0) {
			result = i;
			I("find the str=%s,idx=%d\n",
			  g_hx_inspt_crtra_name[i], i);
			break;
		}
	}
	if (i > (HX_CRITERIA_SIZE - 1)) {
		E("%s: find Fail!\n", __func__);
		return LENGTH_FAIL;
	}

	return result;
}

int hx_print_crtra_after_parsing(void)
{
	int i = 0, j = 0;
	int all_mut_len = ic_data->HX_TX_NUM*ic_data->HX_RX_NUM;

	for (i = 0; i < HX_CRITERIA_SIZE; i++) {
		I("Now is %s\n", g_hx_inspt_crtra_name[i]);
		if (g_inspt_crtra_flag[i] == 1) {
			for (j = 0; j < all_mut_len; j++) {
				I("%d, ", g_inspection_criteria[i][j]);
				if (j % 16 == 15)
					PI("\n");
			}
		} else {
			I("No this Item in this criteria file!\n");
		}
		PI("\n");
	}

	return 0;
}

static int hx_get_crtra_by_name(char **result, int size_of_result_str)
{
	int i = 0;
	/* count of criteria type */
	int count_type = 0;
	/* count of criteria data */
	int count_data = 0;
	int err = HX_INSPECT_OK;
	int all_mut_len = ic_data->HX_TX_NUM*ic_data->HX_RX_NUM;
	int temp = 0;

	/* get criteria and assign to a global array(2-Dimensional/int) */
	/* basiclly the size of criteria will be
	 * (crtra_count * (all_mut_len) + crtra_count)
	 * but we use file size to be the end of counter
	 */
	for (i = 0; i < size_of_result_str && result[i] != NULL; i++) {
		/* It have get one page(all mutual) criteria data!
		 * And we should skip the string of criteria name!
		 */
		if (i == 0 || i ==
		 ((i / (all_mut_len))+(i / (all_mut_len) * (all_mut_len)))) {
			count_data = 0;

			if (private_ts->debug_log_level & BIT(4))
				I("Now find str=%s ,idx=%d\n", result[i], i);

			/* check the item of criteria is in criteria file
			 * or not
			 */
			count_type = hx_find_crtra_id(result[i]);
			if (count_type < 0) {
				E("1. %s:Name Not match!\n", __func__);
				/* E("can recognize[%d]=%s\n", count_type,
				 * g_hx_inspt_crtra_name[count_type]);
				 */
				E("get from file[%d]=%s\n", i, result[i]);
				E("Please check criteria file again!\n");
				err = HX_INSPECT_EFILE;
				goto END_FUNCTION;
			} else {
				/*I("Now str=%s, idx=%d\n",*/
				/*g_hx_inspt_crtra_name[count_type], count_type);*/
				g_inspt_crtra_flag[count_type] = 1;
			}
			continue;
		}
		/* change string to int*/
		temp = hiamx_parse_str2int(result[i]);
		if (temp != -9487)
			g_inspection_criteria[count_type][count_data] = temp;
		else {
			E("%s: Parsing Fail in %d\n", __func__, i);
			E("in range:[%d]=%s\n", count_type,
			  g_hx_inspt_crtra_name[count_type]);
			E("btw, get from file[%d]=%s\n", i, result[i]);
			break;
		}
		/* dbg
		 * I("[%d]g_inspection_criteria[%d][%d]=%d\n",
		 * i, count_type, count_data,
		 * g_inspection_criteria[count_type][count_data]);
		 */
		count_data++;

	}

	if (private_ts->debug_log_level & BIT(4)) {
		/* dbg:print all of criteria from parsing file */
		hx_print_crtra_after_parsing();
	}

	/*I("Total loop=%d\n", i);*/
END_FUNCTION:
	return err;
}

static int himax_parse_criteria_file(char *str_data, int str_size)
{
	int err = HX_INSPECT_OK;
	char **result;
	int i = 0;
	int crtra_count = HX_CRITERIA_SIZE;
	int data_size = 0; /* The maximum of number Data*/
	int all_mut_len = ic_data->HX_TX_NUM*ic_data->HX_RX_NUM;
	int str_max_len = 0;
	int size_of_result_str = 0;

	I("%s,Entering\n", __func__);

	/* size of criteria include name string */
	data_size = ((all_mut_len) * crtra_count) + crtra_count;
	while (g_hx_inspt_crtra_name[i] != NULL) {
		if (strlen(g_hx_inspt_crtra_name[i]) > str_max_len)
			str_max_len = strlen(g_hx_inspt_crtra_name[i]);
		i++;
	}

	/* init the array which store original criteria and include
	 *  name string
	 */
	result = kcalloc(data_size, sizeof(char *), GFP_KERNEL);
	if (result != NULL) {
		for (i = 0 ; i < data_size; i++) {
			result[i] = kcalloc(str_max_len, sizeof(char), GFP_KERNEL);
			if (result[i] == NULL) {
				E("%s: rst_arr Memory allocation failed!\n", __func__);
				err = HX_INSPECT_MEMALLCTFAIL;
				goto rst_arr_mem_alloc_failed;
			}
		}
	} else {
		E("%s: Memory allocation failed!\n", __func__);
		err = HX_INSPECT_MEMALLCTFAIL;
		goto rst_mem_alloc_failed;
	}

	/* dbg */
	if (private_ts->debug_log_level & BIT(4)) {
		I("first 4 bytes 0x%2X,0x%2X,0x%2X,0x%2X !\n",
		  str_data[0], str_data[1],
		  str_data[2], str_data[3]);
	}

	/* parse value in to result array(1-Dimensional/String) */
	size_of_result_str =
		himax_saperate_comma(str_data, str_size, result, data_size);

	I("%s: now size_of_result_str=%d\n", __func__, size_of_result_str);

	err = hx_get_crtra_by_name(result, size_of_result_str);
	if (err != HX_INSPECT_OK) {
		E("%s:Load criteria from file fail, go end!\n", __func__);
		goto END_FUNC;
	}


END_FUNC:
rst_arr_mem_alloc_failed:
	for (i = 0 ; i < data_size; i++)
		if (result[i] != NULL)
			kfree(result[i]);
	kfree(result);
rst_mem_alloc_failed:

	I("%s,END\n", __func__);
	return err;
	/* parsing Criteria end */
}

static int himax_test_item_parse(char *str_data, int str_size)
{
	int size = str_size;
	char *str_ptr = str_data;
	char *end_ptr = NULL;
	int i = 0;
	int ret = HX_INSPECT_EFILE;

	I("%s,str_data: %p, str_size: %d\n", __func__, str_data, str_size);

	do {
		str_ptr = strnstr(str_ptr, "HIMAX", size);
		end_ptr = strnstr(str_ptr, "\r\n", size);
		if (str_ptr != NULL && end_ptr != NULL) {
			while (g_himax_inspection_mode[i]) {
				if (strncmp(str_ptr, g_himax_inspection_mode[i], end_ptr - str_ptr) == 0) {
					I("%s,Find item : %s\n", __func__, g_himax_inspection_mode[i]);
					g_test_item_flag[i] = 1;
					ret = HX_INSPECT_OK;
					break;
				}
				i++;
			}
			size = str_size - (end_ptr - str_data);
			str_ptr = end_ptr++;
			i = 0;
		} else {
			I("%s,Can't find %s or %s\n", __func__, "HIMAX", "\x0d\x0a");
			break;
		}
	} while (size > strlen("HIMAX"));

	return ret;
}

static int himax_parse_test_file(const struct firmware *file_entry)
{
	int start_str_len = 0;
	int str_size = 0;
	char *start_ptr = NULL;
	char *end_ptr = NULL;
	int i = 0;
	int j = 0;
	char str[2][60]; /*[0]->Start string, [1]->End string*/
	char *str_tail[2] = {"_Begin]\x0d\x0a", "_End]\x0d\x0a"};
	int ret = HX_INSPECT_OK;

	while (g_hx_head_str[i]) {
		/*compose header string of .dri file*/
		for (j = 0; j < 2; j++) {
			strlcpy(str[j], "[", sizeof(str[j]));
			strlcat(str[j], g_hx_head_str[i], sizeof(str[j]));
			strlcat(str[j], str_tail[j], sizeof(str[j]));
			/*I("%s string[%d] : %s\n", __func__, j, str[j]);*/
		}

		/*find each group of .dri file*/
		start_str_len = strlen(str[0]);
		start_ptr = strnstr(file_entry->data, str[0], file_entry->size);
		end_ptr = strnstr(file_entry->data, str[1], file_entry->size);
		str_size = end_ptr - start_ptr - start_str_len;
		/*I("%s,String Length = %d\n", __func__, str_size);*/

		if (start_ptr == NULL || end_ptr == NULL)
			E("%s,Can't find string %s\n", __func__, g_hx_head_str[i]);
		else {
			/*parse each sub group string*/
			/*if (strncmp(g_hx_head_str[i], "Project_Info", strlen(g_hx_head_str[i])) == 0) {*/
				/* get project informaion - Not Use*/
			/*}*/
			if (strncmp(g_hx_head_str[i], "TestItem", strlen(g_hx_head_str[i])) == 0) {
				/*get Test Item*/
				I("%s,Start to parse %s\n", __func__, g_hx_head_str[i]);
				ret |= himax_test_item_parse(start_ptr + start_str_len, str_size);
			}
			/*if (strncmp(g_hx_head_str[i], "TestCriteria_Weight", strlen(g_hx_head_str[i])) == 0) {*/
				/*get Test Criteria Weight - Not Use*/
			/*}*/
			if (strncmp(g_hx_head_str[i], "TestCriteria", strlen(g_hx_head_str[i])) == 0) {
				/*get Test Criteria*/
				I("%s,Start to parse %s\n", __func__, g_hx_head_str[i]);
				ret |= himax_parse_criteria_file(start_ptr + start_str_len, str_size);
			}
		}
		i++;
	}

	return ret;
}

static void himax_test_item_chk(int csv_test)
{
	int i = 0;

	if (csv_test)
		for (i = 0; i < HX_CRITERIA_ITEM - 1; i++)
			g_test_item_flag[i] = 1;

	g_test_item_flag[HIMAX_OPEN] &=
	(g_inspt_crtra_flag[IDX_OPENMIN] == 1 &&
	 g_inspt_crtra_flag[IDX_OPENMAX] == 1)?1:0;

	g_test_item_flag[HIMAX_MICRO_OPEN] &=
	(g_inspt_crtra_flag[IDX_M_OPENMIN] == 1 &&
	 g_inspt_crtra_flag[IDX_M_OPENMAX] == 1)?1:0;

	g_test_item_flag[HIMAX_SHORT] &=
	(g_inspt_crtra_flag[IDX_SHORTMIN] == 1 &&
	 g_inspt_crtra_flag[IDX_SHORTMAX] == 1)?1:0;

	g_test_item_flag[HIMAX_RAWDATA] &=
	(g_inspt_crtra_flag[IDX_RAWMIN] == 1 &&
	 g_inspt_crtra_flag[IDX_RAWMAX] == 1)?1:0;

	g_test_item_flag[HIMAX_BPN_RAWDATA] &=
	(g_inspt_crtra_flag[IDX_BPN_RAWMIN] == 1 &&
	 g_inspt_crtra_flag[IDX_BPN_RAWMAX] == 1)?1:0;

	g_test_item_flag[HIMAX_SC] &=
	(g_inspt_crtra_flag[IDX_SCMIN] == 1 &&
	 g_inspt_crtra_flag[IDX_SCMAX] == 1 &&
	 g_inspt_crtra_flag[IDX_SC_GOLDEN] == 1)?1:0;

	g_test_item_flag[HIMAX_WEIGHT_NOISE] &=
	(g_inspt_crtra_flag[IDX_WT_NOISEMIN] == 1 &&
	 g_inspt_crtra_flag[IDX_WT_NOISEMAX] == 1)?1:0;

	g_test_item_flag[HIMAX_ABS_NOISE] &=
	(g_inspt_crtra_flag[IDX_ABS_NOISEMIN] == 1 &&
	 g_inspt_crtra_flag[IDX_ABS_NOISEMAX] == 1)?1:0;

	g_test_item_flag[HIMAX_SORTING] &=
	(g_inspt_crtra_flag[IDX_SORTMIN] == 1 &&
	 g_inspt_crtra_flag[IDX_SORTMAX] == 1)?1:0;

	g_test_item_flag[HIMAX_GAPTEST_RAW] &=
	(g_inspt_crtra_flag[IDX_GAP_HOR_RAWMAX] == 1 &&
	  g_inspt_crtra_flag[IDX_GAP_HOR_RAWMIN] == 1 &&
	  g_inspt_crtra_flag[IDX_GAP_VER_RAWMAX] == 1 &&
	  g_inspt_crtra_flag[IDX_GAP_VER_RAWMIN] == 1)?1:0;

	g_test_item_flag[HIMAX_ACT_IDLE_RAWDATA] &=
	(g_inspt_crtra_flag[IDX_ACT_IDLE_RAWDATA_MIN] == 1 &&
	 g_inspt_crtra_flag[IDX_ACT_IDLE_RAWDATA_MAX] == 1)?1:0;

	g_test_item_flag[HIMAX_ACT_IDLE_BPN_RAWDATA] &=
	(g_inspt_crtra_flag[IDX_ACT_IDLE_RAW_BPN_MIN] == 1 &&
	 g_inspt_crtra_flag[IDX_ACT_IDLE_RAW_BPN_MAX] == 1)?1:0;

	g_test_item_flag[HIMAX_ACT_IDLE_NOISE] &=
	(g_inspt_crtra_flag[IDX_ACT_IDLE_NOISE_MIN] == 1 &&
	 g_inspt_crtra_flag[IDX_ACT_IDLE_NOISE_MAX] == 1)?1:0;

	g_test_item_flag[HIMAX_LPWUG_RAWDATA] &=
	(g_inspt_crtra_flag[IDX_LPWUG_RAWDATA_MIN] == 1 &&
	 g_inspt_crtra_flag[IDX_LPWUG_RAWDATA_MAX] == 1)?1:0;

	g_test_item_flag[HIMAX_LPWUG_BPN_RAWDATA] &=
	(g_inspt_crtra_flag[IDX_LPWUG_RAW_BPN_MIN] == 1 &&
	 g_inspt_crtra_flag[IDX_LPWUG_RAW_BPN_MAX] == 1)?1:0;

	g_test_item_flag[HIMAX_LPWUG_WEIGHT_NOISE] &=
	(g_inspt_crtra_flag[IDX_LPWUG_WT_NOISEMAX] == 1 &&
	 g_inspt_crtra_flag[IDX_LPWUG_WT_NOISEMIN] == 1)?1:0;

	g_test_item_flag[HIMAX_LPWUG_ABS_NOISE] &=
	(g_inspt_crtra_flag[IDX_LPWUG_NOISE_ABS_MAX] == 1 &&
	 g_inspt_crtra_flag[IDX_LPWUG_NOISE_ABS_MIN] == 1)?1:0;

	g_test_item_flag[HIMAX_LPWUG_IDLE_RAWDATA] &=
	(g_inspt_crtra_flag[IDX_LPWUG_IDLE_RAWDATA_MAX] == 1 &&
	 g_inspt_crtra_flag[IDX_LPWUG_IDLE_RAWDATA_MIN] == 1)?1:0;

	g_test_item_flag[HIMAX_LPWUG_IDLE_BPN_RAWDATA] &=
	(g_inspt_crtra_flag[IDX_LPWUG_IDLE_RAW_BPN_MIN] == 1 &&
	 g_inspt_crtra_flag[IDX_LPWUG_IDLE_RAW_BPN_MAX] == 1)?1:0;

	g_test_item_flag[HIMAX_LPWUG_IDLE_NOISE] &=
	(g_inspt_crtra_flag[IDX_LPWUG_IDLE_NOISE_MAX] == 1 &&
	 g_inspt_crtra_flag[IDX_LPWUG_IDLE_NOISE_MIN] == 1)?1:0;

	do_lpwg_test = g_test_item_flag[HIMAX_LPWUG_RAWDATA] |
	g_test_item_flag[HIMAX_LPWUG_BPN_RAWDATA] |
	g_test_item_flag[HIMAX_LPWUG_WEIGHT_NOISE] |
	g_test_item_flag[HIMAX_LPWUG_ABS_NOISE] |
	g_test_item_flag[HIMAX_LPWUG_IDLE_RAWDATA] |
	g_test_item_flag[HIMAX_LPWUG_IDLE_BPN_RAWDATA] |
	g_test_item_flag[HIMAX_LPWUG_IDLE_NOISE];

	for (i = 0; i < HX_CRITERIA_ITEM - 1; i++)
		I("g_test_item_flag[%d] = %d\n", i, g_test_item_flag[i]);
}

int hx_get_size_str_arr(char **input)
{
	int i = 0;
	int result = 0;

	while (input[i] != NULL)
		i++;

	result = i;
	if (private_ts->debug_log_level & BIT(4))
		I("There is %d in [0]=%s\n", result, input[0]);

	return result;
}

static void hx_print_ic_id(void)
{
	uint8_t i;
	int len = 0;
	char *prt_data = NULL;

	prt_data = kzalloc(sizeof(char) * HX_SZ_ICID, GFP_KERNEL);
	if (prt_data == NULL) {
		E("%s: Memory allocation failed!\n", __func__);
		return;
	}

	len += snprintf(prt_data + len, HX_SZ_ICID - len,
				 "IC ID : ");
	for (i = 0; i < 13; i++) {
		len += snprintf(prt_data + len, HX_SZ_ICID - len,
		 "%02X", ic_data->vendor_ic_id[i]);
	}
	len += snprintf(prt_data + len, HX_SZ_ICID - len,
		 "\n");

	memcpy(&g_rslt_data[g_rslt_data_len], prt_data, len);
	g_rslt_data_len += len;
	I("%s: g_rslt_data_len=%d!\n", __func__, g_rslt_data_len);

	kfree(prt_data);
}

static int himax_self_test_data_init(void)
{
	const struct firmware *file_entry = NULL;
	struct himax_ts_data *ts = private_ts;
	char *file_name_1 = "hx_criteria.dri";
	char *file_name_2 = "hx_criteria.csv";
	int ret = HX_INSPECT_OK;
	int err = 0;
	int i = 0;

	/*
	 * 5: one value will not over than 99999, so get this size of string
	 * 2: get twice size
	 */
	g_1kind_raw_size = 5 * ic_data->HX_RX_NUM * ic_data->HX_TX_NUM * 2;

	/* get test item and its items of criteria*/
	HX_CRITERIA_ITEM = hx_get_size_str_arr(g_himax_inspection_mode);
	HX_CRITERIA_SIZE = hx_get_size_str_arr(g_hx_inspt_crtra_name);
	I("There is %d HX_CRITERIA_ITEM and %d HX_CRITERIA_SIZE\n",
	  HX_CRITERIA_ITEM, HX_CRITERIA_SIZE);

	/* init criteria data*/
	g_inspt_crtra_flag = kcalloc(HX_CRITERIA_SIZE, sizeof(int), GFP_KERNEL);
	g_inspection_criteria = kcalloc(HX_CRITERIA_SIZE, sizeof(int *), GFP_KERNEL);
	g_test_item_flag = kcalloc(HX_CRITERIA_ITEM, sizeof(int), GFP_KERNEL);
	if (g_inspt_crtra_flag == NULL || g_inspection_criteria == NULL
		|| g_test_item_flag == NULL) {
		E("%s: %d, Memory allocation failed!\n", __func__, __LINE__);
		return MEM_ALLOC_FAIL;
	}

	for (i = 0; i < HX_CRITERIA_SIZE; i++) {
		g_inspection_criteria[i] = kcalloc(
		  (ic_data->HX_TX_NUM*ic_data->HX_RX_NUM), sizeof(int), GFP_KERNEL);
		if (g_inspection_criteria[i] == NULL) {
			E("%s: %d, Memory allocation failed!\n", __func__, __LINE__);
			return MEM_ALLOC_FAIL;
		}
	}

	/* default path is /system/etc/firmware */
	/* request criteria file*/
	err = request_firmware(&file_entry, file_name_1, ts->dev);
	if (err < 0) {
		E("%s,Fail to get %s\n", __func__, file_name_1);
		err = request_firmware(&file_entry, file_name_2, ts->dev);
		if (err < 0) {
			E("%s,Fail to get %s\n", __func__, file_name_2);
			I("No criteria file file");
			ret |= HX_INSPECT_EFILE;
		} else {
			I("%s,Success to get %s\n", __func__, file_name_2);
			/* parsing criteria from file .csv*/
			ret |= himax_parse_criteria_file((char *)file_entry->data, (int)file_entry->size);
			himax_test_item_chk(true);
			release_firmware(file_entry);
		}
	} else {
		/* parsing test file .dri*/
		I("%s,Success to get %s\n", __func__, file_name_1);
		ret |= himax_parse_test_file(file_entry);
		himax_test_item_chk(false);
		release_firmware(file_entry);
	}

	if (private_ts->debug_log_level & BIT(4)) {
		/* print get criteria string */
		for (i = 0 ; i < HX_CRITERIA_SIZE ; i++) {
			if (g_inspt_crtra_flag[i] != 0)
				I("%s: [%d]There is String=%s\n",
				  __func__, i, g_hx_inspt_crtra_name[i]);
		}
	}

	if (g_rslt_data == NULL) {
		g_rslt_data = kcalloc(g_1kind_raw_size * HX_CRITERIA_ITEM,
			  sizeof(char), GFP_KERNEL);
		if (g_rslt_data == NULL) {
			E("%s: %d, Memory allocation failed!\n", __func__, __LINE__);
			ret =  MEM_ALLOC_FAIL;
		}
	} else {
		memset(g_rslt_data, 0x00, g_1kind_raw_size * HX_CRITERIA_ITEM *
			  sizeof(char));
	}

	snprintf(g_file_path,
		  (int)(strlen(HX_RSLT_OUT_PATH) + strlen(HX_RSLT_OUT_FILE)+1),
		  "%s%s", HX_RSLT_OUT_PATH, HX_RSLT_OUT_FILE);

	return ret;
}

static void himax_self_test_data_deinit(void)
{
	int i = 0;

	/*dbg*/
	/* for (i = 0; i < HX_CRITERIA_ITEM; i++)
	 *	I("%s:[%d]%d\n", __func__, i, g_inspection_criteria[i]);
	 */
	if (g_inspection_criteria != NULL) {
		for (i = 0; i < HX_CRITERIA_SIZE; i++) {
			if (g_inspection_criteria[i] != NULL)
				kfree(g_inspection_criteria[i]);
		}
		kfree(g_inspection_criteria);
		I("Now it have free the g_inspection_criteria!\n");
	} else {
		I("No Need to free g_inspection_criteria!\n");
	}

	if (g_inspt_crtra_flag != NULL) {
		kfree(g_inspt_crtra_flag);
		g_inspt_crtra_flag = NULL;
	}

	g_rslt_data_len = 0;

}

static int himax_chip_self_test(void)
{
	uint32_t ret = HX_INSPECT_OK;
	uint32_t test_size = ic_data->HX_TX_NUM * ic_data->HX_RX_NUM
										+ ic_data->HX_TX_NUM + ic_data->HX_RX_NUM;
	int i = 0;
	uint8_t tmp_addr[DATA_LEN_4] = {0x94, 0x72, 0x00, 0x10};
	uint8_t tmp_data[DATA_LEN_4] = {0x01, 0x00, 0x00, 0x00};
#ifdef HX_CODE_OVERLAY
	uint8_t normalfw[32] = "Himax_firmware.bin";
	uint8_t mpapfw[32] = "Himax_mpfw.bin";
#endif

	I("%s:IN\n", __func__);

	private_ts->suspend_resume_done = 0;

#ifdef HX_CODE_OVERLAY
	g_core_fp.fp_0f_op_file_dirly(mpapfw);
	g_core_fp.fp_reload_disable(0);
	g_core_fp.fp_sense_on(0x00);
#endif

	ret = himax_self_test_data_init();

	hx_print_ic_id();

	/*Do normal test items*/
	for (i = 0; i < HX_CRITERIA_ITEM; i++) {
		if (i < HIMAX_LPWUG_RAWDATA) {
			if (g_test_item_flag[i] == 1) {
				I("%d. %s Start\n", i, g_himax_inspection_mode[i]);
				ret |= mpTestFunc(i, test_size);
				I("%d. %s End, ret = %X\n", i, g_himax_inspection_mode[i], ret);
			}
		} else {
			break;
		}
	}

	/* Press power key and do LPWUG test items*/
	if (do_lpwg_test) {
		himax_press_powerkey();
		/* Wait suspend done */
		while (private_ts->suspend_resume_done != 1)
			usleep_range(1000, 1001);
		private_ts->suspend_resume_done = 0;

		for (; i < HX_CRITERIA_ITEM; i++) {
			if (g_test_item_flag[i] == 1) {
				I("%d. %s Start\n", i, g_himax_inspection_mode[i]);
				ret |= mpTestFunc(i, test_size);
				I("%d. %s End\n", i, g_himax_inspection_mode[i]);
			}
		}
		himax_press_powerkey();
		/* Wait resume done */
		while (private_ts->suspend_resume_done != 1)
			usleep_range(1000, 1001);
	}

	hx_test_data_pop_out(g_rslt_data, g_file_path);

#ifdef HX_CODE_OVERLAY
	private_ts->in_self_test = 0;
	g_core_fp.fp_0f_op_file_dirly(normalfw);
	hx_turn_on_mp_func(0);
	/* set N frame back to default value 1*/
	g_core_fp.fp_register_write(tmp_addr, 4, tmp_data, 0);
	g_core_fp.fp_reload_disable(0);
	g_core_fp.fp_sense_on(0);
#else
	g_core_fp.fp_sense_off(true);
	hx_turn_on_mp_func(0);
	/*himax_set_N_frame(1, HIMAX_INSPECTION_WEIGHT_NOISE);*/
	/* set N frame back to default value 1*/
	g_core_fp.fp_register_write(tmp_addr, 4, tmp_data, 0);
#ifndef HX_ZERO_FLASH
	if (g_core_fp.fp_reload_disable != NULL)
		g_core_fp.fp_reload_disable(0);
#endif
	g_core_fp.fp_sense_on(0);
#endif

	himax_self_test_data_deinit();


	I("running status = %X\n", ret);

	/*if (ret != 0)*/
		/*ret = 1;*/

	I("%s:OUT\n", __func__);
	return ret;
}

void himax_inspect_data_clear(void)
{
	if (!g_rslt_data) {
		kfree(g_rslt_data);
		g_rslt_data = NULL;
	}
}

void himax_inspection_init(void)
{
	I("%s: enter, %d\n", __func__, __LINE__);

	g_core_fp.fp_chip_self_test = himax_chip_self_test;
}



/* SEC INSPECTION */
#ifdef SEC_FACTORY_MODE
static void hx_findout_limit(uint32_t *RAW, int *limit_val, int sz_mutual)
{
	int i = 0;
	int tmp = 0;

	for (i = 0; i < sz_mutual; i++) {
		tmp = ((int32_t)RAW[i]);
		if (tmp > limit_val[0]) {
			limit_val[0] = tmp;
		}
		if (tmp < limit_val[1]) {
			limit_val[1] = tmp;
		}
		tmp = 0;
	}
	I("%s: max=%d, min=%d\n", __func__, limit_val[0], limit_val[1]);
}

static void himax_osr_ctrl(bool enable)
{
	uint8_t tmp_addr[4] = {0};
	uint8_t tmp_data[4] = {0};
	uint8_t w_byte = 0;
	uint8_t back_byte = 0;
	uint8_t retry = 10;

	I("%s %s: Entering\n", HIMAX_LOG_TAG, __func__);

	tmp_addr[3] = 0x10; tmp_addr[2] = 0x00; tmp_addr[1] = 0x70; tmp_addr[0] = 0x88;

	do {
		g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);

		if (enable == 0) {
			tmp_data[0] &= ~(1 << 4);
			tmp_data[0] &= ~(1 << 5);
		} else {
			tmp_data[0] |= (1 << 4);
			tmp_data[0] |= (1 << 5);
		}

		w_byte = tmp_data[0];

		g_core_fp.fp_register_write(tmp_addr, 4, tmp_data, 0);
		g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
		back_byte = tmp_data[0];
		if (w_byte != back_byte)
			I("%s %s: Write osr_ctrl failed, w_byte = %02X, back_byte = %02X\n",
			HIMAX_LOG_TAG, __func__, w_byte, back_byte);
		else {
			I("%s %s: Write osr_ctrl correctly, w_byte = %02X, back_byte = %02X\n",
			HIMAX_LOG_TAG, __func__, w_byte, back_byte);
			break;
		}
	} while (--retry > 0);
}

static int hx_get_one_raw(uint32_t *RAW, uint8_t checktype, uint32_t datalen)
{
	int ret = 0;
#ifdef HX_ESD_RECOVERY
	/* skip zero counter*/
	g_zero_event_count = 0;
#endif

	I("%s Need Change Mode ,target=%s\n",
		HIMAX_LOG_TAG, g_himax_inspection_mode[checktype]);
	g_core_fp.fp_sense_off(true);
	hx_turn_on_mp_func(1);
#ifndef HX_ZERO_FLASH
	if (g_core_fp.fp_reload_disable != NULL)
		g_core_fp.fp_reload_disable(1);
#endif
	/* force to active status */
	g_core_fp.fp_idle_mode(1);

	himax_switch_mode_inspection(checktype);
	if (checktype == HIMAX_ABS_NOISE || checktype == HIMAX_WEIGHT_NOISE) {
		himax_osr_ctrl(0); /*disable OSR_HOP_EN*/
		himax_set_N_frame(NOISEFRAME, checktype);
		/* himax_get_noise_base(); */
	} else if (checktype == HIMAX_ACT_IDLE_RAWDATA
			|| checktype == HIMAX_ACT_IDLE_NOISE) {
		I("N frame = %d\n", 10);
		himax_set_N_frame(10, checktype);
	} else {
		I("N frame = %d\n", 2);
		himax_set_N_frame(2, checktype);
	}
	g_core_fp.fp_sense_on(1);
	ret = himax_wait_sorting_mode(checktype);
	if (ret) {
		E("%s %s: himax_wait_sorting_mode FAIL\n",
			HIMAX_LOG_TAG, __func__);
		goto END_FUNC;
	}


	himax_switch_data_type(checktype);

	ret = himax_get_rawdata(RAW, datalen, checktype);
	if (ret) {
		E("%s %s: himax_get_rawdata FAIL\n",
			HIMAX_LOG_TAG, __func__);
		goto END_FUNC;
	}

//	g_dc_max = g_core_fp.fp_get_max_dc();
	g_dc_max = himax_get_max_dc();

END_FUNC:
	I("%s %s:return normal status!\n",
		HIMAX_LOG_TAG, __func__);
	g_core_fp.fp_sense_off(true);

	himax_switch_data_type(HIMAX_BACK_NORMAL);
	himax_switch_mode_inspection(HIMAX_RAWDATA);
	hx_turn_on_mp_func(0);
	/* change to auto status */
	himax_set_N_frame(1, HIMAX_ABS_NOISE);
#ifndef HX_ZERO_FLASH
	if (g_core_fp.fp_reload_disable != NULL)
		g_core_fp.fp_reload_disable(0);
#endif

	msleep(20);
	g_core_fp.fp_sense_on(1);
	msleep(20);

	return ret;
}

int hx_raw_2d_to_1d(int x, int y)
{
	if (y == 0)
		return x;
	return (x + (ic_data->HX_RX_NUM * (y - 1)));
}

int hx_get_3x3_noise(int *RAW, int mid_pos_x, int mid_pos_y)
{
	int ret = NO_ERR;
	int x = 0;
	int y = 0;
	int mid_idx = 0;
	int start_idx = 0;
	int now_idx = 0;
	int sum = 0;

	mid_idx = hx_raw_2d_to_1d(mid_pos_x, mid_pos_y);
	/* in the middle's postion of left and up*/
	start_idx = mid_idx - 1 - ic_data->HX_RX_NUM;

	for (y = 0; y < 3; y++) {
		for (x = 0; x < 3; x++) {
			now_idx = start_idx + (x * 1) + (y * ic_data->HX_RX_NUM);
			sum += RAW[now_idx];
		}
		printk("\n");
	}
	ret = sum;
	return ret;
}

extern int32_t *diag_mutual;
int hx_sensity_test(int *result)
{
	int ret = NO_ERR;
	int *RAW;
	int i = 0;
	int j = 0;
	int index = 0;
	int datalen = 0;
	int tx_num = ic_data->HX_TX_NUM;
	int rx_num = ic_data->HX_RX_NUM;
	int sens_pt_grp[9][2];

	/* 1: y, 0:x*/
	sens_pt_grp[0][1] = 3;
	sens_pt_grp[0][0] = 3;
	sens_pt_grp[1][1] = tx_num / 2;
	sens_pt_grp[1][0] = 3;
	sens_pt_grp[2][1] = tx_num - 4;
	sens_pt_grp[2][0] = 3;
	sens_pt_grp[3][1] = 3;
	sens_pt_grp[3][0] = rx_num / 2;
	sens_pt_grp[4][1] = tx_num / 2;
	sens_pt_grp[4][0] = rx_num / 2;
	sens_pt_grp[5][1] = tx_num - 4;
	sens_pt_grp[5][0] = rx_num / 2;
	sens_pt_grp[6][1] = 3;
	sens_pt_grp[6][0] = rx_num - 4;
	sens_pt_grp[7][1] = tx_num / 2;
	sens_pt_grp[7][0] = rx_num - 4;
	sens_pt_grp[8][1] = tx_num - 4;
	sens_pt_grp[8][0] = rx_num - 4;

	datalen = (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM)
			+ ic_data->HX_TX_NUM + ic_data->HX_RX_NUM;
	RAW = kzalloc(sizeof(int) * datalen, GFP_KERNEL);
	/* get one time, but now markup it*/
	/* ret = hx_get_one_iir(RAW, HIMAX_INSPECTION_RAWDATA, datalen); */

	memcpy(RAW, diag_mutual, sizeof(int) * datalen);
	if (g_ts_dbg != 0) {
		for (j = 0; j < ic_data->HX_RX_NUM; j++) {
			if (j == 0) {
				printk("      RX%2d", j + 1);
			} else {
				printk("  RX%2d", j + 1);
			}
		}
		printk("\n");

		for (i = 0; i < ic_data->HX_TX_NUM; i++) {
			printk("TX%2d", i + 1);
			for (j = 0; j < ic_data->HX_RX_NUM; j++) {
				index = i * j;
				printk("%5d ", RAW[index]);
			}
			printk("\n");
		}
	}
	for (i = 0; i < 9; i++) {
		result[i] =
			hx_get_3x3_noise(RAW, sens_pt_grp[i][0], sens_pt_grp[i][1]);
	}
	kfree(RAW);
	return ret;
}

#define TSP_PATH_EXTERNAL_FW		"/sdcard/Firmware/TSP/tsp.bin"
#define TSP_PATH_EXTERNAL_FW_SIGNED		"/sdcard/firmware/tsp/tsp_signed.bin"
#define TSP_PATH_SPU_FW_SIGNED "/spu/TSP/ffu_tsp.bin"
#define LEN_RSLT	128
#define FACTORY_BUF_SIZE    PAGE_SIZE
#define BUILT_IN            (0)
#define UMS                 (1)
#define SPU                 (3)

#define TSP_NODE_DEBUG      (0)
#define TSP_CM_DEBUG        (0)
#define TSP_CH_UNUSED       (0)
#define TSP_CH_SCREEN       (1)
#define TSP_CH_GTX          (2)
#define TSP_CH_KEY          (3)
#define TSP_CH_UNKNOWN      (-1)

#include <linux/uaccess.h>
#define MAX_FW_PATH 255
int g_f_edge_border = 0;
int g_f_cal_en = 0;

struct sec_rawdata_buffs *g_sec_raw_buff;
extern unsigned long FW_VER_MAJ_FLASH_ADDR;
extern unsigned long FW_VER_MIN_FLASH_ADDR;
extern unsigned long CFG_VER_MAJ_FLASH_ADDR;
extern unsigned long CFG_VER_MIN_FLASH_ADDR;
extern unsigned long CID_VER_MAJ_FLASH_ADDR;
extern unsigned long CID_VER_MIN_FLASH_ADDR;
extern unsigned long PANEL_VERSION_ADDR;
//unsigned long PANEL_VERSION_ADDR = 49156; /* 0x00C004 */
extern bool fw_update_complete;

static void not_support_cmd(void *dev_data)
{
	char buf[LEN_RSLT] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);
	snprintf(buf, sizeof(buf), "%s", "NA");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);

	I("%s %s: \"%s(%d)\"\n",
			HIMAX_LOG_TAG, __func__, buf, (int)strnlen(buf,
								sizeof(buf)));
	return;
}

static void check_connection(void *dev_data)
{
	char buf[LEN_RSLT] = { 0 };
	uint8_t tmp_data[DATA_LEN_4] = { 0 };
	int ret_val = NO_ERR;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);
	ret_val = g_core_fp.fp_register_read(pfw_op->addr_chk_fw_status, DATA_LEN_4,
					tmp_data, 0);
	I("R900000A8 : [3]=0x%02X,[2]=0x%02X,[1]=0x%02X,[0]=0x%02X (ret=%d)\n",
			tmp_data[3], tmp_data[2], tmp_data[1], tmp_data[0], ret_val);

	if (ret_val == NO_ERR && tmp_data[0] >= 0x05 && tmp_data[0] <= 0x0E) {
		snprintf(buf, sizeof(buf), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}

static void get_chip_vendor(void *dev_data)
{
	char buf[LEN_RSLT] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);
	snprintf(buf, sizeof(buf), "%s", "HIMAX");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "IC_VENDOR");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}

static void get_chip_name(void *dev_data)
{
	char buf[LEN_RSLT] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "%s", private_ts->chip_name);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "IC_NAME");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}

static void get_chip_id(void *dev_data)
{
	char buf[LEN_RSLT] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "%#02x", ic_data->vendor_sensor_id);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}

static int himax_fw_update_kernel(struct himax_ts_data *ts)
{
	int ret = 0;
	char fw_path[MAX_FW_PATH + 1];
	const struct firmware *firmware = NULL;

	himax_int_enable(0);

	memset(fw_path, 0, MAX_FW_PATH);
	snprintf(fw_path, MAX_FW_PATH, "%s", ts->pdata->i_CTPM_firmware_name);

	ret = request_firmware(&firmware, fw_path, ts->dev);
	if (ret) {
		E("%s %s: do not request firmware: %d name as=%s\n",
				HIMAX_LOG_TAG, __func__, ret, fw_path);
		goto FAIL_END;
	}

	switch (firmware->size) {
	case FW_SIZE_32k:
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_32k(
				(unsigned char *) firmware->data, firmware->size, false);
		break;
	case FW_SIZE_60k:
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_60k(
				(unsigned char *) firmware->data, firmware->size, false);
		break;
	case FW_SIZE_64k:
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_64k(
				(unsigned char *) firmware->data, firmware->size, false);
		break;
	case FW_SIZE_124k:
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_124k(
				(unsigned char *) firmware->data, firmware->size, false);
		break;
	case FW_SIZE_128k:
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_128k(
				(unsigned char *) firmware->data, firmware->size, false);
		break;
	default:
		E("%s %s: does not support fw size %d\n",
				HIMAX_LOG_TAG, __func__, (int)firmware->size);
	}

	if (ret == 0)
		ret = FW_ERR_UPTODATE;
	else
		ret = FW_ERR_NONE;

	release_firmware(firmware);

	g_core_fp.fp_reload_disable(0);
	g_core_fp.fp_read_FW_ver();
	g_core_fp.fp_touch_information();
#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_ic_reset(true, false);
#else
	g_core_fp.fp_sense_on(0x00);
#endif

FAIL_END:
	himax_int_enable(1);
	return ret;
}

static int himax_fw_update_from_storage(bool signing, const char *file_path)
{
	int ret = 0;
	struct file *filp = NULL;
	unsigned char *upgrade_fw = NULL;
	mm_segment_t oldfs;
	long fsize, nread;
	long spu_ret, spu_fw_size;

	himax_int_enable(0);

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open(file_path, O_RDONLY, S_IRUSR);

	if (IS_ERR(filp)) {
		E("%s %s: open firmware file failed\n",
				HIMAX_LOG_TAG, __func__);
		ret = FW_ERR_FILE_OPEN;
		set_fs(oldfs);
		himax_int_enable(1);
		return FW_ERR_FILE_OPEN;
	}

	fsize = filp->f_path.dentry->d_inode->i_size;

	if (signing) {
		spu_fw_size = fsize;
		/* name 3, digest 32, signature 512 */
		fsize -= SPU_METADATA_SIZE(TSP);
	}

	if (fsize > 0) {
		unsigned char *spu_fw_data;

		upgrade_fw = kzalloc(sizeof(unsigned char) * fsize, GFP_KERNEL);

		if (signing) {
			spu_fw_data = kzalloc(spu_fw_size, GFP_KERNEL);
			nread = vfs_read(filp, (char __user *)spu_fw_data, spu_fw_size, &filp->f_pos);
			I("%s - path [%s] size [%zu]\n",
					__func__, file_path, spu_fw_size);

			if (nread != spu_fw_size) {
				I("%s [ERROR] vfs_read - size[%zu] read[%zu]\n",
					__func__, fsize, nread);
				ret = FW_ERR_FILE_READ;
				kfree(spu_fw_data);
				kfree(upgrade_fw);
				goto FAIL_END;
			}

			spu_ret = spu_firmware_signature_verify("TSP", spu_fw_data, spu_fw_size);
			I("%s: spu_ret : %ld, spu_fw_size:%ld\n", __func__, spu_ret, spu_fw_size);

			if (spu_ret != fsize) {
				I("%s: signature verify failed, %ld\n", __func__, spu_ret);
				ret = FW_ERR_FILE_SIGN;
				kfree(spu_fw_data);
				kfree(upgrade_fw);
				goto FAIL_END;
			}

			memcpy(upgrade_fw, spu_fw_data, fsize);
			kfree(spu_fw_data);
		} else {
			nread = vfs_read(filp, (char __user *)upgrade_fw, fsize, &filp->f_pos);
			I("%s - path [%s] size [%zu]\n", __func__, file_path, fsize);

			if (nread != fsize) {
				I("%s [ERROR] vfs_read - size[%zu] read[%zu]\n",
					__func__, fsize, nread);
				ret = FW_ERR_FILE_READ;
				kfree(upgrade_fw);
				goto FAIL_END;;
			}
		}
	} else {
		I("%s [ERROR] fsize [%zu]\n", __func__, fsize);
		ret = FW_ERR_FILE_READ;
		goto FAIL_END;
	}

	switch (fsize) {
	case FW_SIZE_32k:
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_32k(
					upgrade_fw, fsize, false);
		break;
	case FW_SIZE_60k:
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_60k(
					upgrade_fw, fsize, false);
		break;
	case FW_SIZE_64k:
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_64k(
					upgrade_fw, fsize, false);
		break;
	case FW_SIZE_124k:
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_124k(
					upgrade_fw, fsize, false);
		break;
	case FW_SIZE_128k:
		ret = g_core_fp.fp_fts_ctpm_fw_upgrade_with_sys_fs_128k(
					upgrade_fw, fsize, false);
		break;
	default:
		E("%s %s: does not support fw size %d\n",
				HIMAX_LOG_TAG, __func__, (int)fsize);
	}

	if (ret == 0)
		ret = FW_ERR_UPTODATE;
	else
		ret = FW_ERR_NONE;

	if (upgrade_fw)
		kfree(upgrade_fw);

	g_core_fp.fp_reload_disable(0);
	g_core_fp.fp_read_FW_ver();
	g_core_fp.fp_touch_information();
#ifdef HX_RST_PIN_FUNC
	g_core_fp.fp_ic_reset(true, false);
#else
	g_core_fp.fp_sense_on(0x00);
#endif

FAIL_END:
	filp_close(filp, NULL);
	set_fs(oldfs);
	himax_int_enable(1);
	I("%s: DONE %d\n", __func__);
	return ret;
}

static void fw_update(void *dev_data)
{
	int ret = 0;
	char buf[LEN_RSLT] = { 0 };

	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct himax_ts_data *ts =
		container_of(sec, struct himax_ts_data, sec);

	sec_cmd_set_default_result(sec);

	I("%s %s(), %d\n", HIMAX_LOG_TAG,
			__func__, sec->cmd_param[0]);

	switch (sec->cmd_param[0]) {
	case BUILT_IN:
		ret = himax_fw_update_kernel(ts);
		break;
	case UMS:
#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		ret = himax_fw_update_from_storage(true, TSP_PATH_EXTERNAL_FW_SIGNED);
#else
		ret = himax_fw_update_from_storage(false, TSP_PATH_EXTERNAL_FW);
#endif
		break;
	case SPU:
		ret = himax_fw_update_from_storage(true, TSP_PATH_SPU_FW_SIGNED);
		break;
	default:
		ret = -EINVAL;
		I("%s not support cmd");
		break;
	}

	if (ret == 0) {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buf, sizeof(buf), "%s", "OK");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buf, sizeof(buf), "%s", "NG");
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	I("%s %s: %s(%d) ret = %d\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)), ret);
}

static void get_fw_ver_bin(void *dev_data)
{
	int ret = 0;
	uint8_t bin_ver_ic_name = 0; /* self definition*/
	uint8_t bin_ver_proj = 0; /* CID Maj */
	uint8_t bin_ver_modul = 0; /* Panel Ver */
	uint8_t bin_ver_fw = 0; /* CID Min*/
	int bin_fw_ver = 0;
	int bin_cid_ver = 0;
	char buf[LEN_RSLT] = { 0 };
	char fw_path[MAX_FW_PATH + 1];
	const struct firmware *firmware = NULL;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);

	snprintf(fw_path, MAX_FW_PATH, "%s",
		private_ts->pdata->i_CTPM_firmware_name);
	I("%s fw_path=%s\n", HIMAX_LOG_TAG,
			fw_path);

	ret = request_firmware(&firmware, fw_path, private_ts->dev);
	if (ret) {
		E("%s %s: do not request firmware: %d name as=%s\n",
				HIMAX_LOG_TAG, __func__, ret, fw_path);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buf, sizeof(buf),
			"HX: get bin fail please check bin file!\n");
	} else {
		I("Now project name = %s\n", private_ts->pdata->proj_name);

		if (strcmp(private_ts->pdata->proj_name, "M20") == 0) {
			bin_fw_ver =
				(firmware->data[FW_VER_MAJ_FLASH_ADDR] << 8) |
				firmware->data[FW_VER_MIN_FLASH_ADDR];
			bin_cid_ver =
				(firmware->data[CID_VER_MAJ_FLASH_ADDR] << 8) |
				firmware->data[CID_VER_MIN_FLASH_ADDR];

			snprintf(buf, sizeof(buf), "HX%04X%04X", bin_fw_ver, bin_cid_ver);
		} else {
			if (strcmp(HX_83112A_SERIES_PWON, private_ts->chip_name) == 0)
				bin_ver_ic_name = 0x01;
			else if (strcmp(HX_83102D_SERIES_PWON, private_ts->chip_name) == 0)
				bin_ver_ic_name = 0x02;
			else if (strcmp(HX_83102E_SERIES_PWON, private_ts->chip_name) == 0)
				bin_ver_ic_name = 0x03;
			else
				bin_ver_ic_name = 0x00;
			bin_ver_proj = firmware->data[CID_VER_MAJ_FLASH_ADDR];
			bin_ver_modul = firmware->data[PANEL_VERSION_ADDR];
			bin_ver_fw = firmware->data[CID_VER_MIN_FLASH_ADDR];

			snprintf(buf, sizeof(buf), "HX%02X%02X%02X%02X", bin_ver_ic_name, bin_ver_proj, bin_ver_modul, bin_ver_fw);
		}
	}
	if (firmware) {
		release_firmware(firmware);
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "FW_VER_BIN");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}

static void get_config_ver(void *dev_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	char buf[LEN_RSLT] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "%s_%02X%02X", "HX",
		ic_data->vendor_touch_cfg_ver,
		ic_data->vendor_display_cfg_ver);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}

static void get_checksum_data(void *dev_data)
{
	char buf[LEN_RSLT] = { 0 };
	u32 chksum = 0;
	u32 chksum_size = 0;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);

	if ((strcmp(HX_83102D_SERIES_PWON, private_ts->chip_name) == 0) ||
		(strcmp(HX_83102E_SERIES_PWON, private_ts->chip_name) == 0)) {
		chksum_size = FW_SIZE_128k;
	} else {
		chksum_size = FW_SIZE_64k;
	}
	I("Now Size = %d\n", chksum_size);

	himax_int_enable(0);
	msleep(10);
	g_core_fp.fp_sense_off(true);
	msleep(10);

	chksum =
		g_core_fp.fp_check_CRC(pfw_op->addr_program_reload_from, chksum_size);
	msleep(10);

	g_core_fp.fp_sense_on(0);
	msleep(10);
	himax_int_enable(1);
	msleep(10);
	/*
	chksum == 0 => checksum pass
	chksum != 0 => checksum fail
	*/

	if (chksum == 0) {
		snprintf(buf, sizeof(buf), "OK:0x%06X", chksum);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buf, sizeof(buf), "NG:0x%06X", chksum);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}

static void get_fw_ver_ic(void *dev_data)
{
	uint8_t ic_ver_ic_name = 0; /* self definition*/
	uint8_t ic_ver_proj = 0; /* CID Maj */
	uint8_t ic_ver_modul = 0; /* Panel Ver */
	uint8_t ic_ver_fw = 0; /* CID Min*/

	char buf[LEN_RSLT] = { 0 };
	char model[16] = {0};

	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);

	g_core_fp.fp_sense_off(true);
	g_core_fp.fp_read_FW_ver();
	g_core_fp.fp_sense_on(0);

	I("Now project name = %s\n", private_ts->pdata->proj_name);

	if (strcmp(private_ts->pdata->proj_name, "M20") == 0) {
		snprintf(buf, sizeof(buf), "HX%04X%04X", ic_data->vendor_fw_ver,
		(ic_data->vendor_cid_maj_ver << 8 | ic_data->vendor_cid_min_ver));
	} else {
		if (strcmp(HX_83112A_SERIES_PWON, private_ts->chip_name) == 0)
			ic_ver_ic_name = 0x01;
		else if (strcmp(HX_83102D_SERIES_PWON, private_ts->chip_name) == 0)
			ic_ver_ic_name = 0x02;
		else if (strcmp(HX_83102E_SERIES_PWON, private_ts->chip_name) == 0)
			ic_ver_ic_name = 0x03;
		else
			ic_ver_ic_name = 0x00;
		ic_ver_proj = ic_data->vendor_cid_maj_ver;
		ic_ver_modul = ic_data->vendor_panel_ver;
		ic_ver_fw = ic_data->vendor_cid_min_ver;

		snprintf(buf, sizeof(buf), "HX%02X%02X%02X%02X", ic_ver_ic_name, ic_ver_proj, ic_ver_modul, ic_ver_fw);
		snprintf(model, sizeof(model), "HX%02X%02X", ic_ver_ic_name, ic_ver_proj);

	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model, strnlen(model, sizeof(model)), "FW_MODEL");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}

static void set_edge_mode(void *dev_data)
{
	int ret = 0;
	char buf[LEN_RSLT] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);

	I("%s %s(), %d\n", HIMAX_LOG_TAG,
			__func__, sec->cmd_param[0]);

	switch (sec->cmd_param[0]) {
	case 0:
		ret = g_core_fp.set_edge_border(FW_EDGE_BORDER_OFF);
		if (ret == 0) {
			g_f_edge_border = FW_EDGE_BORDER_OFF;
			I("%s %s(), Unset Edge Mode\n", HIMAX_LOG_TAG, __func__);
		}
		break;
	case 1:
		ret = g_core_fp.set_edge_border(FW_EDGE_BORDER_ON);
		if (ret == 0) {
			g_f_edge_border = FW_EDGE_BORDER_ON;
			I("%s %s(), Set Edge Mode\n", HIMAX_LOG_TAG, __func__);
		}
		break;
	default:
		E("%s %s(), Invalid Argument\n", HIMAX_LOG_TAG,
				__func__);
		break;
	}

	if (ret) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buf, sizeof(buf), "%s", "NG");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buf, sizeof(buf), "%s", "OK");
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}

/* read write and check register for 4bytes*/
static int hx_cmd_rw_chk(uint32_t addr, uint8_t chg_len, uint8_t *pos_ary, uint8_t *data_ary)
{
	int retry = 10;
	int i = 0;
	uint8_t addr_ary[4] = {0};
	uint8_t write_data[4] = {0};
	uint8_t read_data[4] = {0};

	himax_in_parse_assign_cmd(addr, addr_ary, 4);
	g_core_fp.fp_register_read(addr_ary, DATA_LEN_4, write_data, false);

	/*data change*/
	for (i = 0; i < chg_len; i++)
		write_data[pos_ary[i]] = data_ary[i];

	do {
		g_core_fp.fp_register_write(addr_ary, DATA_LEN_4, write_data, false);
		msleep(10);
		g_core_fp.fp_register_read(addr_ary, DATA_LEN_4, read_data, false);

		if (read_data[3] == write_data[3] && read_data[2] == write_data[2]
			&& read_data[1] == write_data[1] && read_data[0] == write_data[0])
			return 1;

		retry--;
		E("%s %s: write register retry(%d)\n", HIMAX_LOG_TAG, __func__, retry);
	} while (retry > 0);

	return 0;
}

static int hx_rotate_mode_set(bool is_portrait)
{
	uint8_t pos_ary[4] = {0, 1, 2, 3};
	uint8_t data_ary[4] = {0};

	if (is_portrait)
		himax_in_parse_assign_cmd(data_portrait, data_ary, 4);
	else
		himax_in_parse_assign_cmd(data_landscape, data_ary, 4);

	return hx_cmd_rw_chk(addr_rotative_mode, 4, pos_ary, data_ary);
}

/* only support Letter box */
static void set_grip_data(void *dev_data)
{
	int ret = 1;
	char buf[LEN_RSLT] = { 0 };
	uint8_t pos_ary[4] = {0};
	uint8_t data_ary[4] = {0};
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);

	sec_cmd_set_default_result(sec);

	if (data->power_status == HX_TS_STATE_POWER_OFF) {
		E("%s %s: now IC status is OFF\n", HIMAX_LOG_TAG, __func__);
		goto err_grip_data;
	}

	if (sec->cmd_param[0] == 1) {/*portrait*/
		ret &= hx_rotate_mode_set(true);
		pos_ary[0] = 0; pos_ary[1] = 1;
		data_ary[0] = sec->cmd_param[1]; data_ary[1] = sec->cmd_param[1];
		ret &= hx_cmd_rw_chk(addr_grip_zone, 2, pos_ary, data_ary);/*grip zone*/
		data_ary[0] = sec->cmd_param[2]; data_ary[1] = sec->cmd_param[3];
		ret &= hx_cmd_rw_chk(addr_reject_zone, 2, pos_ary, data_ary);/*reject zone*/
		data_ary[0] = sec->cmd_param[4] >> 8; data_ary[1] = sec->cmd_param[4] & 0xFF;
		ret &= hx_cmd_rw_chk(addr_reject_zone_boud, 2, pos_ary, data_ary);/*reject zone boundary*/
		memcpy(g_portrait_data, sec->cmd_param, 5 * sizeof(int));
	} else if (sec->cmd_param[0] == 2) {/*landscape*/
		if (sec->cmd_param[1] == 1) {
			/*landscape enter*/
			ret &= hx_rotate_mode_set(false);
			pos_ary[0] = 0; pos_ary[1] = 1; pos_ary[2] = 2; pos_ary[3] = 3;
			data_ary[0] = sec->cmd_param[2]; data_ary[1] = sec->cmd_param[2];
			data_ary[2] = sec->cmd_param[4]; data_ary[3] = sec->cmd_param[5];
			ret &= hx_cmd_rw_chk(addr_reject_zone, 4, pos_ary, data_ary);/*reject zone*/
			data_ary[0] = sec->cmd_param[3]; data_ary[1] = sec->cmd_param[3];
			data_ary[2] = sec->cmd_param[6]; data_ary[3] = sec->cmd_param[7];
			ret &= hx_cmd_rw_chk(addr_grip_zone, 4, pos_ary, data_ary);/*grip zone*/
		} else if (sec->cmd_param[1] == 0) {
			/*change to portrait & recovery previous portrait setting*/
			ret &= hx_rotate_mode_set(true);
			pos_ary[0] = 0; pos_ary[1] = 1;
			data_ary[0] = g_portrait_data[1]; data_ary[1] = g_portrait_data[1];
			ret &= hx_cmd_rw_chk(addr_grip_zone, 2, pos_ary, data_ary);/*grip zone*/
			data_ary[0] = g_portrait_data[2]; data_ary[1] = g_portrait_data[3];
			ret &= hx_cmd_rw_chk(addr_reject_zone, 2, pos_ary, data_ary);/*reject zone*/
			data_ary[0] = g_portrait_data[4] >> 8; data_ary[1] = g_portrait_data[4] & 0xFF;
			ret &= hx_cmd_rw_chk(addr_reject_zone_boud, 2, pos_ary, data_ary);/*reject zone boundary*/
		} else {
			E("%s %s: cmd0 is abnormal, %d\n", HIMAX_LOG_TAG, __func__, sec->cmd_param[1]);
			goto err_grip_data;
		}
	} else if (sec->cmd_param[0] == 0) {/*exception zone*/
		if (sec->cmd_param[1] >= 0 && sec->cmd_param[1] <= 2) {
			/*directoin - off(0), left(1), right(2)*/
			pos_ary[0] = 0; pos_ary[1] = 1; pos_ary[2] = 2; pos_ary[3] = 3;
			data_ary[0] = (sec->cmd_param[1] << 6) |  sec->cmd_param[2] >> 8;
			data_ary[1] = sec->cmd_param[2] & 0xFF;
			data_ary[2] = sec->cmd_param[3] >> 8;
			data_ary[3] = sec->cmd_param[3] & 0xFF;
			ret &= hx_cmd_rw_chk(addr_except_zone, 4, pos_ary, data_ary);
		} else {
			E("%s %s: cmd0 is abnormal, %d\n", HIMAX_LOG_TAG, __func__, sec->cmd_param[1]);
			goto err_grip_data;
		}
	} else {
		E("%s %s: cmd0 is abnormal, %d\n", HIMAX_LOG_TAG, __func__, sec->cmd_param[0]);
		goto err_grip_data;
	}
err_grip_data:
	if (ret == 1) {
		sec->cmd_state = SEC_CMD_STATUS_OK;
		snprintf(buf, sizeof(buf), "%s", "OK");
	} else {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buf, sizeof(buf), "%s", "NG");
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}

static void get_threshold(void *dev_data)
{
	char buf[LEN_RSLT] = { 0 };
	int threshold = 0;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);

	threshold = g_core_fp.get_rport_thrsh();

	if (threshold > 0) {
		snprintf(buf, sizeof(buf), "0x%02X", threshold);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buf, sizeof(buf), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}

static void get_scr_x_num(void *dev_data)
{
	int val = -1;
	char buf[LEN_RSLT] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);

	val = ic_data->HX_X_RES;
	if (val >= 0) {
		snprintf(buf, sizeof(buf), "%u", val);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}

static void get_scr_y_num(void *dev_data)
{
	int val = -1;
	char buf[LEN_RSLT] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);

	val = ic_data->HX_Y_RES;
	if (val >= 0) {
		snprintf(buf, sizeof(buf), "%u", val);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}

static void get_all_x_num(void *dev_data)
{
	int val = -1;
	char buf[LEN_RSLT] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);

	val = ic_data->HX_TX_NUM;	/* device direction, long side is length */
	if (val >= 0) {
		snprintf(buf, sizeof(buf), "%u", val);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}

static void get_all_y_num(void *dev_data)
{
	int val = -1;
	char buf[LEN_RSLT] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);

	val = ic_data->HX_RX_NUM;	/* device direction, short side is width */
	if (val >= 0) {
		snprintf(buf, sizeof(buf), "%u", val);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}

static void hx_print_frame(struct himax_ts_data *ts, uint32_t *frame)
{
	int i = 0;
	char msg[15] = { 0 };
	int msg_len = 15;
	char *buf = NULL;

	buf = kzalloc(ic_data->HX_RX_NUM * 10, GFP_KERNEL);
	if (!buf)
		return;
	for (i = 0; i < ic_data->HX_RX_NUM; i++) {
		if (i == 0)
			snprintf(msg, msg_len, "        RX%2d", i + 1);
		else
			snprintf(msg, msg_len, "  RX%2d", i + 1);

		strncat(buf, msg, msg_len);
	}
	I("%s\n", buf);
	buf[0] = '\0';

	for (i = 0; i < ic_data->HX_TX_NUM * ic_data->HX_RX_NUM; i++) {
		snprintf(msg, msg_len, "%5d ", frame[i]);
		strncat(buf, msg, msg_len);
		if (i % ic_data->HX_RX_NUM == (ic_data->HX_RX_NUM - 1)) {
			I("TX%2d %s\n", (i / ic_data->HX_RX_NUM + 1), buf);
			buf[0] = '\0';
		}
	}
	kfree(buf);
}

#ifdef HX_SAVE_RAW_TO_FLASH
extern int hx_write_4k_flash_flow(uint32_t start_addr, uint8_t *write_data, uint32_t write_len);
#endif
static void get_rawcap(void *dev_data)
{
	uint32_t *RAW = NULL;
	uint32_t datalen = 0;
	int val = 0;
	char buf[LEN_RSLT] = { 0 };
#ifdef HX_SAVE_RAW_TO_FLASH
	int i = 0;
	uint8_t *flash_data = NULL;
	uint32_t flash_size = HX_SZ_4K;	/* 4K */
	uint32_t rawdata_size;
#endif
	int limit_val[2] = { 0 };	/* 0: max, 1: min */
	int sz_mutual = ic_data->HX_TX_NUM * ic_data->HX_RX_NUM;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);

	sec_cmd_set_default_result(sec);

	datalen =
		(ic_data->HX_TX_NUM * ic_data->HX_RX_NUM) + ic_data->HX_TX_NUM +
		ic_data->HX_RX_NUM;
	RAW = kzalloc(sizeof(uint32_t) * datalen, GFP_KERNEL);
	if (!RAW) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buf, sizeof(buf), "%s:failed to allocate memory", __func__);
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		return;
	}
#ifdef HX_SAVE_RAW_TO_FLASH
	flash_data = kzalloc(sizeof(uint8_t) * flash_size, GFP_KERNEL);
	if (!flash_data) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buf, sizeof(buf), "%s:failed to allocate memory", __func__);
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		kfree(RAW);
		return;
	}
#endif

	himax_int_enable(0);
	val = hx_get_one_raw(RAW, HIMAX_RAWDATA, datalen);

	if (val > 0) {
		E("%s %s: get rawdata fail!\n", HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%s:get fail", __func__);
		goto END_OUTPUT;
	}

	limit_val[0] = RAW[0];	/* max */
	limit_val[1] = RAW[0];	/* min */
	hx_findout_limit(RAW, limit_val, sz_mutual);

	memcpy(&g_sec_raw_buff->_rawdata[0], &RAW[0],
			sizeof(uint32_t) * ic_data->HX_TX_NUM * ic_data->HX_RX_NUM);

	hx_print_frame(data, g_sec_raw_buff->_rawdata);
#ifdef HX_SAVE_RAW_TO_FLASH
	/* save to flash */
	/* 1. data size*/
	rawdata_size = sizeof(uint32_t) * datalen;
	I("Now rawdata size=%d\n", rawdata_size);
	flash_data[0] = rawdata_size % 0x100;
	flash_data[1] = (rawdata_size >> 8) % 0x100;
	flash_data[2] = (rawdata_size >> 16) % 0x100;
	flash_data[3] = (rawdata_size >> 24) % 0x100;
	I("Now flash_data[0]=0x%02X,[1]=0x%02X,[2]=0x%02X,[3]=0x%02X\n",
		flash_data[0], flash_data[1], flash_data[2], flash_data[3]);
	/*2. data type */
	flash_data[4] = 0x02;
	flash_data[5] = 0x02;
	flash_data[6] = 0x02;
	flash_data[7] = 0x02;
	/*3. save data to buffer*/
	if (flash_size - 8 > rawdata_size) {
		I("Now flash_size > rawdata_size!\n");
		for (i = 0; i < ic_data->HX_TX_NUM * ic_data->HX_RX_NUM; i++) {
			flash_data[8 + i * 2] = RAW[i] % 0x100;
			flash_data[8 + i * 2 + 1] = (RAW[i] >> 8) % 0x100;
		}
	} else {
		I("Now rawdata_size > flash_size!\n");
		for (i = 0; i * 2 < (flash_size - 8); i++) {
			flash_data[8 + i * 2] = RAW[i] % 0x100;
			flash_data[8 + i * 2 + 1] = (RAW[i] >> 8) % 0x100;
		}
	}
	/* 4. save to flash*/
	flash_data[0] = rawdata_size % 0x100;
	flash_data[1] = (rawdata_size >> 8) % 0x100;
	flash_data[2] = (rawdata_size >> 16) % 0x100;
	flash_data[3] = (rawdata_size >> 24) % 0x100;
	hx_write_4k_flash_flow(HX_ADR_RAW_FLASH, flash_data, flash_size);
#endif
END_OUTPUT:
	if (val == 0) {
		g_sec_raw_buff->f_ready_rawdata = HX_RAWDATA_READY;
		I("%s %s: ret val is ok!\n",
				HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%d,%d", limit_val[1], limit_val[0]);	/*min,max */
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		g_sec_raw_buff->f_ready_rawdata = HX_RAWDATA_NOT_READY;
		E("%s %s: ret val is fail!(%d)\n",
				HIMAX_LOG_TAG, __func__, val);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	I("Min=%d, Max=%d\n", limit_val[1], limit_val[0]);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "RAWCAP");

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
	kfree(RAW);
#ifdef HX_SAVE_RAW_TO_FLASH
	kfree(flash_data);
#endif
	himax_int_enable(1);
}

static void get_rawcap_all(void *dev_data)
{
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buf = NULL;
	int i = 0;
	char msg[SEC_CMD_STR_LEN] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	get_rawcap(sec);

	sec_cmd_set_default_result(sec);

	buf = kzalloc(ic_data->HX_TX_NUM * ic_data->HX_RX_NUM * 10, GFP_KERNEL);
	if (!buf) {
		E("%s failed to allocate memory!\n", HIMAX_LOG_TAG);
		goto END_OUTPUT;
	}

	for (i = 0; i < (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM); i++) {
		if (i != (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM - 1)) {
			snprintf(msg, SEC_CMD_STR_LEN, "%d,", (int)g_sec_raw_buff->_rawdata[i]);
			strncat(buf, msg, SEC_CMD_STR_LEN);
		} else {
			snprintf(msg, SEC_CMD_STR_LEN, "%d", (int)g_sec_raw_buff->_rawdata[i]);
			strncat(buf, msg, SEC_CMD_STR_LEN);
		}

	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, ic_data->HX_TX_NUM * ic_data->HX_RX_NUM * 10));

	kfree(buf);
	return;
END_OUTPUT:
	snprintf(temp, SEC_CMD_STR_LEN, "Test Fail");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, temp, strnlen(temp, SEC_CMD_STR_LEN));
}

static void get_open(void *dev_data)
{
	uint32_t *RAW = NULL;
	int datalen = 0;
	int val = 0;
	char buf[LEN_RSLT] = { 0 };
	int limit_val[2] = { 0 };	/* 0: max, 1: min */
	int sz_mutual = ic_data->HX_TX_NUM * ic_data->HX_RX_NUM;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);

	sec_cmd_set_default_result(sec);

	datalen = (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM)
			+ ic_data->HX_TX_NUM + ic_data->HX_RX_NUM;
	RAW = kzalloc(sizeof(uint32_t) * datalen, GFP_KERNEL);
	if (!RAW) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buf, sizeof(buf), "%s:failed to allocate memory", __func__);
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		return;
	}

	himax_int_enable(0);
	val = hx_get_one_raw(RAW, HIMAX_OPEN, datalen);

	if (val > 0) {
		E("%s %s: get open fail!\n",
				HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%s:get fail\n", __func__);
		goto END_OUTPUT;
	}

	limit_val[0] = RAW[0];	/* max */
	limit_val[1] = RAW[0];	/* min */
	hx_findout_limit(RAW, limit_val, sz_mutual);

	memcpy(&g_sec_raw_buff->_open[0], &RAW[0],
			sizeof(uint32_t) * ic_data->HX_TX_NUM * ic_data->HX_RX_NUM);

	hx_print_frame(data, g_sec_raw_buff->_open);

END_OUTPUT:
	if (val == 0) {
		g_sec_raw_buff->f_ready_open = HX_RAWDATA_READY;
		I("%s %s: ret val is ok!\n",
				HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%d,%d", limit_val[1], limit_val[0]);	/*min,max */
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		g_sec_raw_buff->f_ready_open = HX_RAWDATA_NOT_READY;
		E("%s %s: ret val is fail!(%d)\n",
				HIMAX_LOG_TAG, __func__, val);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	I("Min=%d, Max=%d\n", limit_val[1], limit_val[0]);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "OPEN");

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
	kfree(RAW);
	himax_int_enable(1);
}

static void get_open_all(void *dev_data)
{
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buf = NULL;
	int i = 0;
	char msg[SEC_CMD_STR_LEN] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	get_open(sec);

	sec_cmd_set_default_result(sec);

	buf = kzalloc(ic_data->HX_TX_NUM * ic_data->HX_RX_NUM * 10, GFP_KERNEL);
	if (!buf) {
		E("%s failed to allocate memory!\n", HIMAX_LOG_TAG);
		goto END_OUTPUT;
	}

	for (i = 0; i < (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM); i++) {
		if (i != (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM - 1)) {
			snprintf(msg, SEC_CMD_STR_LEN, "%d,", (int)g_sec_raw_buff->_open[i]);
			strncat(buf, msg, SEC_CMD_STR_LEN);
		} else {
			snprintf(msg, SEC_CMD_STR_LEN, "%d", (int)g_sec_raw_buff->_open[i]);
			strncat(buf, msg, SEC_CMD_STR_LEN);
		}

	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, ic_data->HX_TX_NUM * ic_data->HX_RX_NUM * 10));

	kfree(buf);
	return;
END_OUTPUT:
	snprintf(temp, SEC_CMD_STR_LEN, "Test Fail");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, temp, strnlen(temp, SEC_CMD_STR_LEN));
}

static void get_short(void *dev_data)
{
	uint32_t *RAW = NULL;
	int datalen = 0;
	int val = 0;
	char buf[LEN_RSLT] = { 0 };
	int limit_val[2] = { 0 };	/* 0: max, 1: min */
	int sz_mutual = ic_data->HX_TX_NUM * ic_data->HX_RX_NUM;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);

	sec_cmd_set_default_result(sec);

	datalen = (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM)
			+ ic_data->HX_TX_NUM + ic_data->HX_RX_NUM;
	RAW = kzalloc(sizeof(uint32_t) * datalen, GFP_KERNEL);
	if (!RAW) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buf, sizeof(buf), "%s:failed to allocate memory", __func__);
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		return;
	}

	himax_int_enable(0);
	val = hx_get_one_raw(RAW, HIMAX_SHORT, datalen);

	if (val > 0) {
		E("%s %s: get short fail!\n",
				HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%s:get fail\n", __func__);
		goto END_OUTPUT;
	}

	limit_val[0] = RAW[0];	/* max */
	limit_val[1] = RAW[0];	/* min */
	hx_findout_limit(RAW, limit_val, sz_mutual);

	memcpy(&g_sec_raw_buff->_short[0], &RAW[0],
			sizeof(uint32_t) * ic_data->HX_TX_NUM * ic_data->HX_RX_NUM);

	hx_print_frame(data, g_sec_raw_buff->_short);

END_OUTPUT:
	if (val == 0) {
		g_sec_raw_buff->f_ready_short = HX_RAWDATA_READY;
		I("%s %s: ret val is ok!\n",
				HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%d,%d", limit_val[1], limit_val[0]);	/*min,max */
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		g_sec_raw_buff->f_ready_short = HX_RAWDATA_NOT_READY;
		E("%s %s: ret val is fail!(%d)\n",
				HIMAX_LOG_TAG, __func__, val);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	I("Min=%d, Max=%d\n", limit_val[1], limit_val[0]);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "SHORT");

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
	kfree(RAW);
	himax_int_enable(1);
}

static void get_short_all(void *dev_data)
{
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buf = NULL;
	int i = 0;
	char msg[SEC_CMD_STR_LEN] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	get_short(sec);

	sec_cmd_set_default_result(sec);

	buf = kzalloc(ic_data->HX_TX_NUM * ic_data->HX_RX_NUM * 10, GFP_KERNEL);
	if (!buf) {
		E("%s failed to allocate memory!\n", HIMAX_LOG_TAG);
		goto END_OUTPUT;
	}

	for (i = 0; i < (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM); i++) {
		if (i != (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM - 1)) {
			snprintf(msg, SEC_CMD_STR_LEN, "%d,", (int)g_sec_raw_buff->_short[i]);
			strncat(buf, msg, SEC_CMD_STR_LEN);
		} else {
			snprintf(msg, SEC_CMD_STR_LEN, "%d", (int)g_sec_raw_buff->_short[i]);
			strncat(buf, msg, SEC_CMD_STR_LEN);
		}

	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, ic_data->HX_TX_NUM * ic_data->HX_RX_NUM * 10));

	kfree(buf);
	return;
END_OUTPUT:
	snprintf(temp, SEC_CMD_STR_LEN, "Test Fail");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, temp, strnlen(temp, SEC_CMD_STR_LEN));
}

static void get_mic_open(void *dev_data)
{
	uint32_t *RAW = NULL;
	int datalen = 0;
	int val = 0;
	char buf[LEN_RSLT] = { 0 };
	int limit_val[2] = { 0 };	/* 0: max, 1: min */
	int sz_mutual = ic_data->HX_TX_NUM * ic_data->HX_RX_NUM;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);

	sec_cmd_set_default_result(sec);

	datalen = (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM)
			+ ic_data->HX_TX_NUM + ic_data->HX_RX_NUM;
	RAW = kzalloc(sizeof(uint32_t) * datalen, GFP_KERNEL);
	if (!RAW) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buf, sizeof(buf), "%s:failed to allocate memory", __func__);
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		return;
	}

	himax_int_enable(0);
	val = hx_get_one_raw(RAW, HIMAX_MICRO_OPEN, datalen);

	if (val > 0) {
		E("%s %s: get rawdata fail!\n", HIMAX_LOG_TAG,
				__func__);
		snprintf(buf, sizeof(buf), "%s:get fail\n", __func__);
		goto END_OUTPUT;
	}

	limit_val[0] = RAW[0];	/* max */
	limit_val[1] = RAW[0];	/* min */
	hx_findout_limit(RAW, limit_val, sz_mutual);

	memcpy(&g_sec_raw_buff->_mopen[0], &RAW[0],
			sizeof(uint32_t) * ic_data->HX_TX_NUM * ic_data->HX_RX_NUM);

	hx_print_frame(data, g_sec_raw_buff->_mopen);

END_OUTPUT:
	if (val == 0) {
		g_sec_raw_buff->f_ready_mopen = HX_RAWDATA_READY;
		I("%s %s: ret val is ok!\n",
				HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%d,%d", limit_val[1], limit_val[0]);	/*min,max */
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		g_sec_raw_buff->f_ready_mopen = HX_RAWDATA_NOT_READY;
		E("%s %s: ret val is fail!(%d)\n",
				HIMAX_LOG_TAG, __func__, val);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	I("Min=%d, Max=%d\n", limit_val[1], limit_val[0]);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "MICRO_OPEN");

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
	kfree(RAW);
	himax_int_enable(1);
}

static void get_mic_open_all(void *dev_data)
{
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buf = NULL;
	int i = 0;
	char msg[SEC_CMD_STR_LEN] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	get_mic_open(sec);

	sec_cmd_set_default_result(sec);

	buf = kzalloc(ic_data->HX_TX_NUM * ic_data->HX_RX_NUM * 10, GFP_KERNEL);
	if (!buf) {
		E("%s failed to allocate memory!\n", HIMAX_LOG_TAG);
		goto END_OUTPUT;
	}

	for (i = 0; i < (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM); i++) {
		if (i != (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM - 1)) {
			snprintf(msg, SEC_CMD_STR_LEN, "%d,", (int)g_sec_raw_buff->_mopen[i]);
			strncat(buf, msg, SEC_CMD_STR_LEN);
		} else {
			snprintf(msg, SEC_CMD_STR_LEN, "%d", (int)g_sec_raw_buff->_mopen[i]);
			strncat(buf, msg, SEC_CMD_STR_LEN);
		}

	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, ic_data->HX_TX_NUM * ic_data->HX_RX_NUM * 10));

	kfree(buf);
	return;
END_OUTPUT:
	snprintf(temp, SEC_CMD_STR_LEN, "Test Fail");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, temp, strnlen(temp, SEC_CMD_STR_LEN));
}

static void get_noise(void *dev_data)
{
	int32_t *RAW = NULL;
	int datalen = 0;
	int val = 0;
	char buf[LEN_RSLT] = { 0 };
	int limit_val[2] = { 0 };	/* 0: max, 1: min */
	int sz_mutual = ic_data->HX_TX_NUM * ic_data->HX_RX_NUM;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);

	int i = 0;
	uint16_t noise_count = 0;
	uint16_t palm_num = 0;
	uint16_t weight = 0;
	uint8_t tmp_addr[4];
	uint8_t tmp_data[4];

	sec_cmd_set_default_result(sec);

	datalen = (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM)
			+ ic_data->HX_TX_NUM + ic_data->HX_RX_NUM;
	RAW = kzalloc(sizeof(int32_t) * datalen, GFP_KERNEL);
	if (!RAW) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buf, sizeof(buf), "%s:failed to allocate memory", __func__);
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		return;
	}

	himax_int_enable(0);
	val = hx_get_one_raw(RAW, HIMAX_ABS_NOISE, datalen);

	if (val > 0) {
		E("%s %s: get rawdata fail!\n", HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%s:get fail\n", __func__);
		goto END_OUTPUT;
	}

	limit_val[0] = RAW[0];	/* max */
	limit_val[1] = RAW[0];	/* min */
	hx_findout_limit(RAW, limit_val, sz_mutual);

	/* noise weight test */
	himax_get_noise_base(HIMAX_WEIGHT_NOISE);
	palm_num = himax_get_palm_num();
	for (i = 0; i < sz_mutual; i++) {
		if ((int)RAW[i] > NOISEMAX)
			noise_count++;
	}
	I("noise_count = %d\n", noise_count);
	if (noise_count > palm_num) {
		val = -1;
		E("%s: noise test NG, PALM\n", __func__);
	}
	himax_in_parse_assign_cmd(addr_weight_sup, tmp_addr, sizeof(tmp_addr));

	g_core_fp.fp_register_read(tmp_addr, 4, tmp_data, false);
	if (tmp_data[3] == tmp_addr[1] && tmp_data[2] == tmp_addr[0])
		weight = (tmp_data[1] << 8) | tmp_data[0];
	else
		I("%s: FW does not support weight test\n", __func__);

	I("%s: weight = %d, %02X, %02X ", __func__, weight, tmp_data[2], tmp_data[3]);
	/*********************************/

	memcpy(&g_sec_raw_buff->_noise[0], &RAW[0], sizeof(int32_t) * ic_data->HX_TX_NUM * ic_data->HX_RX_NUM);
	hx_print_frame(data, g_sec_raw_buff->_noise);

END_OUTPUT:
	if (val == 0) {
		g_sec_raw_buff->f_ready_noise = HX_RAWDATA_READY;
		I("%s %s: ret val is ok!\n",
				HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%d,%d", limit_val[1], limit_val[0]);	/*min,max */
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		g_sec_raw_buff->f_ready_noise = HX_RAWDATA_NOT_READY;
		E("%s %s: ret val is fail!(%d)\n",
				HIMAX_LOG_TAG, __func__, val);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	I("Min=%d, Max=%d\n", limit_val[1], limit_val[0]);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "NOISE");
		snprintf(buf, sizeof(buf), "%d,%d", 0, weight);
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "NoiseWeight");
	}

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
	kfree(RAW);
	himax_int_enable(1);
}

static void get_noise_all(void *dev_data)
{
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buf = NULL;
	int i = 0;
	char msg[SEC_CMD_STR_LEN] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	get_noise(sec);

	sec_cmd_set_default_result(sec);

	buf = kzalloc(ic_data->HX_TX_NUM * ic_data->HX_RX_NUM * 10, GFP_KERNEL);
	if (!buf) {
		E("%s failed to allocate memory!\n", HIMAX_LOG_TAG);
		goto END_OUTPUT;
	}

	for (i = 0; i < (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM); i++) {
		if (i != (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM - 1)) {
			snprintf(msg, SEC_CMD_STR_LEN, "%d,", (int)g_sec_raw_buff->_noise[i]);
			strncat(buf, msg, SEC_CMD_STR_LEN);
		} else {
			snprintf(msg, SEC_CMD_STR_LEN, "%d", (int)g_sec_raw_buff->_noise[i]);
			strncat(buf, msg, SEC_CMD_STR_LEN);
		}

	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, ic_data->HX_TX_NUM * ic_data->HX_RX_NUM * 10));

	kfree(buf);
	return;
END_OUTPUT:
	snprintf(temp, SEC_CMD_STR_LEN, "Test Fail");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, temp, strnlen(temp, SEC_CMD_STR_LEN));
}

static void get_lp_rawcap(void *dev_data)
{
	uint32_t *RAW = NULL;
	int datalen = 0;
	int val = 0;
	char buf[LEN_RSLT] = { 0 };
	int limit_val[2] = { 0 };	/* 0: max, 1: min */
	int sz_mutual = ic_data->HX_TX_NUM * ic_data->HX_RX_NUM;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);

	sec_cmd_set_default_result(sec);

	datalen = (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM)
			+ ic_data->HX_TX_NUM + ic_data->HX_RX_NUM;
	RAW = kzalloc(sizeof(uint32_t) * datalen, GFP_KERNEL);
	if (!RAW) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buf, sizeof(buf), "%s:failed to allocate memory", __func__);
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		return;
	}

	himax_int_enable(0);
	val = hx_get_one_raw(RAW, HIMAX_LPWUG_RAWDATA, datalen);

	if (val > 0) {
		E("%s %s: get rawdata fail!\n", HIMAX_LOG_TAG,
				__func__);
		snprintf(buf, sizeof(buf), "%s:get fail\n", __func__);
		goto END_OUTPUT;
	}

	limit_val[0] = RAW[0];	/* max */
	limit_val[1] = RAW[0];	/* min */
	hx_findout_limit(RAW, limit_val, sz_mutual);

	memcpy(&g_sec_raw_buff->_lp_rawdata[0], &RAW[0],
			sizeof(uint32_t) * ic_data->HX_TX_NUM * ic_data->HX_RX_NUM);

	hx_print_frame(data, g_sec_raw_buff->_lp_rawdata);

END_OUTPUT:
	if (val == 0) {
		g_sec_raw_buff->f_ready_lp_rawdata = HX_RAWDATA_READY;
		I("%s %s: ret val is ok!\n",
				HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%d,%d", limit_val[1], limit_val[0]);	/*min,max */
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		g_sec_raw_buff->f_ready_lp_rawdata = HX_RAWDATA_NOT_READY;
		E("%s %s: ret val is fail!(%d)\n",
				HIMAX_LOG_TAG, __func__, val);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	I("Min=%d, Max=%d\n", limit_val[1], limit_val[0]);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "LP_RAW");

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
	kfree(RAW);
	himax_int_enable(1);
}

static void get_lp_noise(void *dev_data)
{
	uint32_t *RAW = NULL;
	int datalen = 0;
	int val = 0;
	char buf[LEN_RSLT] = { 0 };
	int limit_val[2] = { 0 };	/* 0: max, 1: min */
	int sz_mutual = ic_data->HX_TX_NUM * ic_data->HX_RX_NUM;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);

	sec_cmd_set_default_result(sec);

	datalen = (ic_data->HX_TX_NUM * ic_data->HX_RX_NUM)
			+ ic_data->HX_TX_NUM + ic_data->HX_RX_NUM;
	RAW = kzalloc(sizeof(uint32_t) * datalen, GFP_KERNEL);
	if (!RAW) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buf, sizeof(buf), "%s:failed to allocate memory", __func__);
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		return;
	}

	himax_int_enable(0);
	val = hx_get_one_raw(RAW, HIMAX_LPWUG_ABS_NOISE, datalen);

	if (val > 0) {
		E("%s %s: get rawdata fail!\n", HIMAX_LOG_TAG,
				__func__);
		snprintf(buf, sizeof(buf), "%s:get fail\n", __func__);
		goto END_OUTPUT;
	}

	limit_val[0] = RAW[0];	/* max */
	limit_val[1] = RAW[0];	/* min */
	hx_findout_limit(RAW, limit_val, sz_mutual);

	memcpy(&g_sec_raw_buff->_lp_noise[0], &RAW[0],
			sizeof(uint32_t) * ic_data->HX_TX_NUM * ic_data->HX_RX_NUM);

	hx_print_frame(data, g_sec_raw_buff->_lp_noise);

END_OUTPUT:
	if (val == 0) {
		g_sec_raw_buff->f_ready_lp_noise = HX_RAWDATA_READY;
		I("%s %s: ret val is ok!\n",
				HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%d,%d", limit_val[1], limit_val[0]);	/*min,max */
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		g_sec_raw_buff->f_ready_lp_noise = HX_RAWDATA_NOT_READY;
		E("%s %s: ret val is fail!(%d)\n",
				HIMAX_LOG_TAG, __func__, val);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	I("Min=%d, Max=%d\n", limit_val[1], limit_val[0]);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "LP_NOISE");

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
	kfree(RAW);
	himax_int_enable(1);
}

static int hx_gap_hor_raw(int test_type, uint32_t *raw, int *result_raw)
{
	int rx_num = ic_data->HX_RX_NUM;
	int tx_num = ic_data->HX_TX_NUM;
	int tmp_start = 0;
	int tmp_end_idx = 0;
	int i_partial = 0;
	int i = 0;
	int now_part = 0;
	int ret_val = NO_ERR;
	int *tmp_result_raw;
	int idx_assign = 0;
	int skip_flag = 0;
	int rslt_buf_size = ic_data->HX_TX_NUM * (ic_data->HX_RX_NUM - g_gap_horizontal_partial);

	himax_gap_test_horizontal_setting();
	if (g_ts_dbg != 0) {
		I("Print Horizontal ORG RAW\n");
		for (i = 0; i < tx_num * rx_num; i++) {
			printk("%04d,", raw[i]);
			if (i > 0 && i % rx_num == (rx_num - 1))
				I("\n");
		}
	}
	tmp_result_raw = kzalloc(sizeof(int) * rx_num * tx_num, GFP_KERNEL);
	if (!tmp_result_raw) {
		kfree(g_gap_horizontal_part);
		return -ENOMEM;
	}

	for (i_partial = 0; i_partial < g_gap_horizontal_partial; i_partial++) {
		tmp_start	= g_gap_horizontal_part[i_partial];
		if (i_partial+1 == g_gap_horizontal_partial) {
			tmp_end_idx = rx_num - g_gap_horizontal_part[i_partial];
		} else {
			tmp_end_idx = g_gap_horizontal_part[i_partial+1] - g_gap_horizontal_part[i_partial];
		}
		if (i_partial % 2 == 0) {
			himax_cal_gap_data_horizontal(tmp_start, tmp_end_idx, 0, raw, tmp_result_raw);
		} else {
			himax_cal_gap_data_horizontal(tmp_start, tmp_end_idx, 1, raw, tmp_result_raw);
		}
	}

	hx_print_frame(private_ts, tmp_result_raw);

	for (i = 0; i < tx_num*rx_num && idx_assign < rslt_buf_size; i++) {
		/*
		printk("%4d,", tmp_result_raw[i]);
		if (i > 0 && i%rx_num == (rx_num-1))
			I("\n");
		*/
		/* difference that it is in which one part now
			even part : the first column is all zero in this part
			odd part : the last column is all zero in this part
		*/
		/* the now part will be known in odd or even */
		now_part = (i % rx_num) / (rx_num / g_gap_horizontal_partial);
		if (now_part % 2 == 0) {
		/*even part
			using index i to know is in which TX
			if i is in the starting column of even part it will need to skip*/
			if (i % rx_num == g_gap_horizontal_part[now_part]) {
				skip_flag = 1;
			}
		} else {
		/*odd part
			using index i to know is is in which TX
			if i is in the last column of odd part it will need to skip*/
			if ((i % rx_num) == (g_gap_horizontal_part[now_part] + rx_num / g_gap_horizontal_partial - 1)) {
				skip_flag = 1;
			}
		}
		if (skip_flag == 1) {
			skip_flag = 0;
			continue;
		} else {
			skip_flag = 0;
			result_raw[idx_assign++] = tmp_result_raw[i];
		}
	}

	kfree(g_gap_horizontal_part);
	kfree(tmp_result_raw);
	return ret_val;
}

static void get_gap_data_y(void *dev_data)
{
	int val = 0;
	char buf[LEN_RSLT] = { 0 };
	int limit_val[2] = { 0 };	/* 0: max, 1: min */
	int sz_mutual = ic_data->HX_TX_NUM * (ic_data->HX_RX_NUM - g_gap_horizontal_partial);
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);

	himax_int_enable(0);
	if (g_sec_raw_buff->f_ready_rawdata != HX_RAWDATA_READY) {
		E("%s %s: need get rawcap firstly\n", HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%s:need get rawcap firstly", __func__);
		val = HX_RAW_NOT_READY;
		goto END_OUTPUT;
	}

	val = hx_gap_hor_raw(HIMAX_GAPTEST_RAW, g_sec_raw_buff->_rawdata,
				g_sec_raw_buff->_gap_hor);
	if (val < 0) {
		E("%s %s: failed to get gap Y\n",
			HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "failed to get gap Y");
		goto END_OUTPUT;
	}

	limit_val[0] = g_sec_raw_buff->_gap_hor[0];	/* max */
	limit_val[1] = g_sec_raw_buff->_gap_hor[0];	/* min */
	hx_findout_limit(g_sec_raw_buff->_gap_hor, limit_val, sz_mutual);

END_OUTPUT:
	if (val == 0) {
		g_sec_raw_buff->f_ready_gap_hor = HX_RAWDATA_READY;
		I("%s %s: ret val is ok!\n",
				HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%d,%d", limit_val[1], limit_val[0]);	/*min,max */
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		g_sec_raw_buff->f_ready_gap_hor = HX_RAWDATA_NOT_READY;
		E("%s %s: ret val is fail!(%d)\n",
				HIMAX_LOG_TAG, __func__, val);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	I("Min=%d, Max=%d\n", limit_val[1], limit_val[0]);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "RAW_GAP_Y");

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
	himax_int_enable(1);
}

static void get_gap_y_all(void *dev_data)
{
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buf = NULL;
	int i = 0;
	char msg[SEC_CMD_STR_LEN] = { 0 };
	int sz_mutual = ic_data->HX_TX_NUM * (ic_data->HX_RX_NUM - g_gap_horizontal_partial);
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	get_gap_data_y(sec);

	sec_cmd_set_default_result(sec);

	buf = kzalloc(sz_mutual * 10, GFP_KERNEL);
	if (!buf) {
		E("%s failed to allocate memory!\n", HIMAX_LOG_TAG);
		goto END_OUTPUT;
	}

	for (i = 0; i < sz_mutual; i++) {
		if (i != (sz_mutual - 1)) {
			snprintf(msg, SEC_CMD_STR_LEN, "%d,", (int)g_sec_raw_buff->_gap_hor[i]);
			strncat(buf, msg, SEC_CMD_STR_LEN);
		} else {
			snprintf(msg, SEC_CMD_STR_LEN, "%d", (int)g_sec_raw_buff->_gap_hor[i]);
			strncat(buf, msg, SEC_CMD_STR_LEN);
		}

	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sz_mutual * 10));

	kfree(buf);
	return;
END_OUTPUT:
	snprintf(temp, SEC_CMD_STR_LEN, "Test Fail");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, temp, strnlen(temp, SEC_CMD_STR_LEN));
}

static int hx_gap_ver_raw(int test_type, uint32_t *org_raw, int *result_raw)
{
	int i_partial = 0;
	int tmp_start = 0;
	int tmp_end_idx = 0;
	int i = 0;
	int now_part = 0;
	int ret_val = NO_ERR;
	int tx_num = ic_data->HX_TX_NUM;
	int rx_num = ic_data->HX_RX_NUM;
	int *tmp_result_raw;
	int idx_assign = 0;
	int skip_flag = 0;
	int rslt_buf_size = (ic_data->HX_TX_NUM - g_gap_vertical_partial) * ic_data->HX_RX_NUM;

	himax_gap_test_vertical_setting();
	if (g_ts_dbg != 0) {
		I("Print vertical ORG RAW\n");
		for (i = 0; i < tx_num * rx_num; i++) {
			printk("%04d,", org_raw[i]);
			if (i > 0 && i % rx_num == (rx_num - 1))
				I("\n");
		}
	}

	tmp_result_raw = kzalloc(sizeof(int) * rx_num * tx_num, GFP_KERNEL);
	if (!tmp_result_raw) {
		kfree(g_gap_vertical_part);
		return -ENOMEM;
	}

	for (i_partial = 0; i_partial < g_gap_vertical_partial; i_partial++) {
		tmp_start	= g_gap_vertical_part[i_partial] * rx_num;
		if (i_partial + 1 == g_gap_vertical_partial) {
			tmp_end_idx = tx_num - g_gap_vertical_part[i_partial];
		} else {
			tmp_end_idx = g_gap_vertical_part[i_partial + 1] - g_gap_vertical_part[i_partial];
		}
		if (i_partial % 2 == 0) {
			himax_cal_gap_data_vertical(tmp_start, tmp_end_idx, 0, org_raw, tmp_result_raw);
		}	else {
			himax_cal_gap_data_vertical(tmp_start, tmp_end_idx, 1, org_raw, tmp_result_raw);
		}
	}

	hx_print_frame(private_ts, tmp_result_raw);

	for (i = 0; i < tx_num * rx_num && idx_assign < rslt_buf_size; i++) {
		/*
		printk("%4d,", tmp_result_raw[i]);
		if (i > 0 && i%rx_num == (rx_num-1))
			I("\n");
		*/
		/* difference that it is in which one part now
			even part : the first line is all zero in this part
			odd part : the last line is all zero in this part
		*/
		/* the now part will be known in odd or even */
		now_part = (i / rx_num) / (tx_num / g_gap_vertical_partial);
		if (now_part % 2 == 1) {
		/*odd part
			using index i to know is in which TX
			if i is in the starting line of odd part it will need to skip*/
			if ((i / rx_num) == (g_gap_vertical_part[now_part] + (tx_num / g_gap_vertical_partial) - 1))
				skip_flag = 1;
		} else {
		/*even part
			using index i to know is in which TX
			if i is in the last line of even part it will need to skip*/
			if ((i / rx_num) == g_gap_vertical_part[now_part]) {
				skip_flag = 1;
			}
		}
		/* If skip_flag is set to 1, it show that it is in the all zero line..
		   We don't need to assign this all zero line's vlaue into result buffer..
		*/
		if (skip_flag == 1) {
			skip_flag = 0;
			continue;
		} else {
			skip_flag = 0;
			result_raw[idx_assign++] = tmp_result_raw[i];
		}
	}

	kfree(g_gap_vertical_part);
	kfree(tmp_result_raw);
	return ret_val;
}

static void get_gap_data_x(void *dev_data)
{
	int val = 0;
	char buf[LEN_RSLT] = { 0 };
	int limit_val[2] = { 0 };	/* 0: max, 1: min */
	int sz_mutual = (ic_data->HX_TX_NUM - g_gap_vertical_partial) * ic_data->HX_RX_NUM;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	sec_cmd_set_default_result(sec);

	himax_int_enable(0);
	if (g_sec_raw_buff->f_ready_rawdata != HX_RAWDATA_READY) {
		E("%s %s: need get rawcap firstly\n", HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%s:need get rawcap firstly", __func__);
		val = HX_RAW_NOT_READY;
		goto END_OUTPUT;
	}

	val = hx_gap_ver_raw(HIMAX_GAPTEST_RAW, g_sec_raw_buff->_rawdata,
				g_sec_raw_buff->_gap_ver);
	if (val < 0) {
		E("%s %s: failed to get gap X\n",
			HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "failed to get gap X");
		goto END_OUTPUT;
	}

	limit_val[0] = g_sec_raw_buff->_gap_ver[0];	/* max */
	limit_val[1] = g_sec_raw_buff->_gap_ver[0];	/* min */
	hx_findout_limit(g_sec_raw_buff->_gap_ver, limit_val, sz_mutual);

END_OUTPUT:
	if (val == 0) {
		g_sec_raw_buff->f_ready_gap_ver = HX_RAWDATA_READY;
		I("%s %s: ret val is ok!\n",
				HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%d,%d", limit_val[1], limit_val[0]);	/*min,max */
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		g_sec_raw_buff->f_ready_gap_ver = HX_RAWDATA_NOT_READY;
		E("%s %s: ret val is fail!(%d)\n",
				HIMAX_LOG_TAG, __func__, val);
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}

	I("Min=%d, Max=%d\n", limit_val[1], limit_val[0]);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buf, strnlen(buf, sizeof(buf)), "RAW_GAP_X");

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
	himax_int_enable(1);
}

static void get_gap_x_all(void *dev_data)
{
	char temp[SEC_CMD_STR_LEN] = { 0 };
	char *buf = NULL;
	int i = 0;
	char msg[SEC_CMD_STR_LEN] = { 0 };
	int sz_mutual = (ic_data->HX_TX_NUM - g_gap_vertical_partial) * ic_data->HX_RX_NUM;
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	/*struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);*/

	get_gap_data_x(sec);

	sec_cmd_set_default_result(sec);

	buf = kzalloc(sz_mutual * 10, GFP_KERNEL);
	if (!buf) {
		E("%s failed to allocate memory!\n", HIMAX_LOG_TAG);
		goto END_OUTPUT;
	}

	for (i = 0; i < sz_mutual; i++) {
		if (i != (sz_mutual - 1)) {
			snprintf(msg, SEC_CMD_STR_LEN, "%d,", (int)g_sec_raw_buff->_gap_ver[i]);
			strncat(buf, msg, SEC_CMD_STR_LEN);
		} else {
			snprintf(msg, SEC_CMD_STR_LEN, "%d", (int)g_sec_raw_buff->_gap_ver[i]);
			strncat(buf, msg, SEC_CMD_STR_LEN);
		}

	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sz_mutual * 10));

	kfree(buf);
	return;
END_OUTPUT:
	snprintf(temp, SEC_CMD_STR_LEN, "Test Fail");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, temp, strnlen(temp, SEC_CMD_STR_LEN));
}

static void touch_aging_mode(void *dev_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	char buf[16] = {0};
	char tmp_addr[4] = {0};
	char send_data[4] = {0};
	char recv_data[4] = {0xFF, 0xFF, 0xFF, 0xFF};
	char retry_cnt = 0;

	sec_cmd_set_default_result(sec);

	switch (sec->cmd_param[0]) {
	case 0:
		I("%s: Unset Touch Aging Mode\n", __func__);
		break;
	case 1:
		I("%s: Set Touch Aging Mode\n", __func__);
		send_data[3] = 0xCD;
		send_data[2] = 0xCD;
		send_data[1] = 0xCD;
		send_data[0] = 0xCD;
		break;
	default:
		I("%s: Invalid Argument\n", __func__);
		send_data[0] = 0xFF;
		break;
	}

	if (send_data[0] == 0xFF) {
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		tmp_addr[3] = 0x10;
		tmp_addr[2] = 0x00;
		tmp_addr[1] = 0x7F;
		tmp_addr[0] = 0xD4;

		do {
			g_core_fp.fp_register_write(tmp_addr, DATA_LEN_4, send_data, 0);
			usleep_range(1000, 1100);
			g_core_fp.fp_register_read(tmp_addr, DATA_LEN_4, recv_data, 0);
			retry_cnt++;
		} while ((send_data[3] != recv_data[3] || send_data[2] != recv_data[2] ||
				send_data[1] != recv_data[1] || send_data[0] != recv_data[0])
				&& retry_cnt < HIMAX_REG_RETRY_TIMES);

		if (retry_cnt >= HIMAX_REG_RETRY_TIMES)
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		else
			sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "OK");
	else
		snprintf(buf, sizeof(buf), "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	I("%s %s: %s\n", HIMAX_LOG_TAG, __func__, buf);
}

static void glove_mode(void *dev_data)
{
	char buf[LEN_RSLT] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct himax_ts_data *data =
		container_of(sec, struct himax_ts_data, sec);

	sec_cmd_set_default_result(sec);
	I("%s %s,%d\n", HIMAX_LOG_TAG,
			__func__, sec->cmd_param[0]);

	if (data->power_status == HX_TS_STATE_POWER_OFF) {
		E("%s %s: now IC status is not STATE_POWER_ON\n", HIMAX_LOG_TAG, __func__);
		snprintf(buf, sizeof(buf), "%s", "TSP_turned_off");
		sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
		goto out;
	}

	switch (sec->cmd_param[0]) {
	case 0:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		I("%s %s(), Unset Glove Mode\n", HIMAX_LOG_TAG,
				__func__);
		data->HSEN_enable = 0;
		g_core_fp.fp_set_HSEN_enable(data->HSEN_enable);
		break;
	case 1:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		I("%s %s(), Set Glove Mode\n", HIMAX_LOG_TAG,
				__func__);
		data->HSEN_enable = 1;
		g_core_fp.fp_set_HSEN_enable(data->HSEN_enable);
		break;
	default:
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		I("%s %s(), Invalid Argument\n", HIMAX_LOG_TAG,
				__func__);
		break;
	}

	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "%s", "OK");
	else
		snprintf(buf, sizeof(buf), "%s", "NG");
out:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	I("%s %s: %s(%d)\n", HIMAX_LOG_TAG,
			__func__, buf, (int)strnlen(buf, sizeof(buf)));
}


#ifdef HX_SMART_WAKEUP
static void aot_enable(void *dev_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;
	struct himax_ts_data *ts = container_of(sec, struct himax_ts_data, sec);
	char buf[16] = { 0 };

	sec_cmd_set_default_result(sec);

	switch (sec->cmd_param[0]) {
	case 0:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		I("%s: Unset AOT Mode\n", __func__);
		ts->gesture_cust_en[0] = 0;
		ts->SMWP_enable = 0;
		g_core_fp.fp_set_SMWP_enable(ts->SMWP_enable);
		break;
	case 1:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		I("%s: Set AOT Mode\n", __func__);
		ts->gesture_cust_en[0] = 1;
		ts->SMWP_enable = 1;
		g_core_fp.fp_set_SMWP_enable(ts->SMWP_enable);
		break;
	default:
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		I("%s: Invalid Argument\n", __func__);
		break;
	}

	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buf, sizeof(buf), "OK");
	else
		snprintf(buf, sizeof(buf), "NG");

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec_cmd_set_cmd_exit(sec);

	I("%s %s: %s\n", HIMAX_LOG_TAG, __func__, buf);
}
#endif

/*
 * read_support_feature function
 * returns the bit combination of specific feature that is supported.
 */
static ssize_t read_support_feature(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct himax_ts_data *ts = container_of(sec, struct himax_ts_data, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	u32 feature = 0;

	if (ts->pdata->support_aot)
		feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;

	snprintf(buff, sizeof(buff), "%d", feature);
	I("%s: %s\n", __func__, buff);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s\n", buff);
}

static void factory_cmd_result_all(void *dev_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)dev_data;

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	get_rawcap(sec);
	get_gap_data_x(sec);
	get_gap_data_y(sec);
	get_open(sec);
	get_mic_open(sec);
	get_short(sec);
	get_noise(sec);
	/*get_lp_rawcap(sec);*/
	/*get_lp_noise(sec);*/

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;
	I("%s %s: %d%s\n", HIMAX_LOG_TAG,
			__func__, sec->item_count, sec->cmd_result_all);
}

static ssize_t show_close_tsp_test(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	return snprintf(buf, FACTORY_BUF_SIZE, "%u\n", 0);
}

struct sec_cmd sec_cmds[] = {
	{SEC_CMD("check_connection", check_connection),},
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_config_ver", get_config_ver),},
	{SEC_CMD("get_checksum_data", get_checksum_data),},
	{SEC_CMD("get_threshold", get_threshold),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_chip_id", get_chip_id),},
	{SEC_CMD("get_scr_x_num", get_scr_x_num),},
	{SEC_CMD("get_scr_y_num", get_scr_y_num),},
	{SEC_CMD("get_x_num", get_all_x_num),},
	{SEC_CMD("get_y_num", get_all_y_num),},
	{SEC_CMD("get_rawcap", get_rawcap),},
	{SEC_CMD("run_rawcap_all", get_rawcap_all),},
	{SEC_CMD("get_open", get_open),},
	{SEC_CMD("run_open_all", get_open_all),},
	{SEC_CMD("get_mic_open", get_mic_open),},
	{SEC_CMD("run_mic_open_all", get_mic_open_all),},
	{SEC_CMD("get_short", get_short),},
	{SEC_CMD("run_short_all", get_short_all),},
	{SEC_CMD("get_noise", get_noise),},
	{SEC_CMD("run_noise_all", get_noise_all),},
	{SEC_CMD("get_lp_rawcap", get_lp_rawcap),},
	{SEC_CMD("get_lp_noise", get_lp_noise),},
	{SEC_CMD("run_jitter_test", not_support_cmd),},
	{SEC_CMD("get_gap_data_x", get_gap_data_x),},
	{SEC_CMD("run_gap_x_all", get_gap_x_all),},
	{SEC_CMD("get_gap_data_y", get_gap_data_y),},
	{SEC_CMD("run_gap_y_all", get_gap_y_all),},
	{SEC_CMD("set_tsp_test_result", not_support_cmd),},
	{SEC_CMD("get_tsp_test_result", not_support_cmd),},
	{SEC_CMD("clear_tsp_test_result", not_support_cmd),},
	{SEC_CMD("dead_zone_enable", set_edge_mode),},
	{SEC_CMD("set_grip_data", set_grip_data),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD_H("glove_mode", glove_mode),},
#ifdef HX_SMART_WAKEUP
	{SEC_CMD_H("aot_enable", aot_enable),},
#endif
	{SEC_CMD_H("touch_aging_mode", touch_aging_mode),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

/* sensitivity mode test */
extern int hx_set_stack_raw(int set_val);
static ssize_t sensitivity_mode_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	int result[9] = { 0 };
	hx_sensity_test(result);
	I("%s: %d,%d,%d,%d,%d,%d,%d,%d,%d", __func__,
			result[0], result[1], result[2],
			result[3], result[4], result[5],
			result[6], result[7], result[8]);
	return snprintf(buf, 256, "%d,%d,%d,%d,%d,%d,%d,%d,%d",
			result[0], result[1], result[2],
			result[3], result[4], result[5],
			result[6], result[7], result[8]);
}

static ssize_t sensitivity_mode_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int mode;

	if (kstrtoint(buf, 10, &mode) < 0) {
		E("%s %s kstrtoint fail\n", HIMAX_LOG_TAG, __func__);
		return count;
	}

	if (unlikely((mode != 0) && (mode != 1)))
		return count;

	if (mode == 0) {
		hx_set_stack_raw(0);	// stop test
		I("%s %sTurn off Sensitivity Measurement\n",
				HIMAX_LOG_TAG, __func__);
	} else {
		hx_set_stack_raw(1);	// start test
		I("%s %sTurn on Sensitivity Measurement\n",
				HIMAX_LOG_TAG, __func__);
	}

	return count;
}

static DEVICE_ATTR(sensitivity_mode, S_IRUGO | S_IWUSR | S_IWGRP,
			sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(close_tsp_test, S_IRUGO, show_close_tsp_test, NULL);
static DEVICE_ATTR(support_feature, 0444, read_support_feature, NULL);

static struct attribute *sec_touch_factory_attributes[] = {
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_close_tsp_test.attr,
	&dev_attr_support_feature.attr,
	NULL,
};

static struct attribute_group sec_touch_factory_attr_group = {
	.attrs = sec_touch_factory_attributes,
};

int sec_touch_sysfs(struct himax_ts_data *data)
{
	int ret;

	/* /sys/class/sec/tsp */
	ret = sec_cmd_init(&data->sec, sec_cmds,
				ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (ret < 0) {
		E("%s %s Failed to create device (tsp)!\n",
				HIMAX_LOG_TAG, __func__);
		goto err_init_cmd;
	}
	ret = sysfs_create_link(&data->sec.fac_dev->kobj,
				&data->input_dev->dev.kobj, "input");
	if (ret < 0)
		E("%s %s: Failed to create input symbolic link\n",
				HIMAX_LOG_TAG, __func__);

	/* /sys/class/sec/tsp/... */
	if (sysfs_create_group
		(&data->sec.fac_dev->kobj, &sec_touch_factory_attr_group)) {
		E("%s %sFailed to create sysfs group(tsp)!\n",
				HIMAX_LOG_TAG, __func__);
		goto err_sec_fac_dev_attr;
	}

	I("Now init rawdata buffs\n");
	g_sec_raw_buff = kzalloc(sizeof(struct sec_rawdata_buffs), GFP_KERNEL);
	g_sec_raw_buff->_rawdata =
		kzalloc(sizeof(uint32_t) * ic_data->HX_TX_NUM * ic_data->HX_RX_NUM,
			GFP_KERNEL);
	g_sec_raw_buff->_open =
		kzalloc(sizeof(uint32_t) * ic_data->HX_TX_NUM * ic_data->HX_RX_NUM,
			GFP_KERNEL);
	g_sec_raw_buff->_mopen =
		kzalloc(sizeof(uint32_t) * ic_data->HX_TX_NUM * ic_data->HX_RX_NUM,
			GFP_KERNEL);
	g_sec_raw_buff->_short =
		kzalloc(sizeof(uint32_t) * ic_data->HX_TX_NUM * ic_data->HX_RX_NUM,
			GFP_KERNEL);
	g_sec_raw_buff->_noise =
		kzalloc(sizeof(int) * ic_data->HX_TX_NUM * ic_data->HX_RX_NUM,
			GFP_KERNEL);
	g_sec_raw_buff->_lp_rawdata =
		kzalloc(sizeof(uint32_t) * ic_data->HX_TX_NUM * ic_data->HX_RX_NUM,
			GFP_KERNEL);
	g_sec_raw_buff->_lp_noise =
		kzalloc(sizeof(int) * ic_data->HX_TX_NUM * ic_data->HX_RX_NUM,
			GFP_KERNEL);
	g_sec_raw_buff->_gap_ver =
		kzalloc(sizeof(int) * (ic_data->HX_TX_NUM - g_gap_vertical_partial) * ic_data->HX_RX_NUM, GFP_KERNEL);
	g_sec_raw_buff->_gap_hor =
		kzalloc(sizeof(int) * ic_data->HX_TX_NUM * (ic_data->HX_RX_NUM - g_gap_horizontal_partial), GFP_KERNEL);
	g_sec_raw_buff->f_crtra_ready = HX_THRESHOLD_NOT_READY;
	g_sec_raw_buff->f_ready_rawdata = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_open = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_mopen = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_short = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_noise = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_lp_rawdata = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_lp_noise = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_gap_ver = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_gap_hor = HX_RAWDATA_NOT_READY;

	return 0;

err_sec_fac_dev_attr:
err_init_cmd:
	return -ENODEV;
}

EXPORT_SYMBOL(sec_touch_sysfs);

void sec_touch_sysfs_remove(struct himax_ts_data *data)
{
	E("%s %s: Entering!\n", HIMAX_LOG_TAG,
			__func__);
	sysfs_remove_link(&data->sec.fac_dev->kobj, "input");
	sysfs_remove_group(&data->sec.fac_dev->kobj,
				&sec_touch_factory_attr_group);
	I("Now remove raw data buffs\n");

	kfree(g_sec_raw_buff->_rawdata);
	kfree(g_sec_raw_buff->_open);
	kfree(g_sec_raw_buff->_mopen);
	kfree(g_sec_raw_buff->_short);
	kfree(g_sec_raw_buff->_noise);
	kfree(g_sec_raw_buff->_lp_rawdata);
	kfree(g_sec_raw_buff->_lp_noise);
	kfree(g_sec_raw_buff->_gap_ver);
	kfree(g_sec_raw_buff->_gap_hor);
	g_sec_raw_buff->f_crtra_ready = HX_THRESHOLD_NOT_READY;
	g_sec_raw_buff->f_ready_rawdata = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_open = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_mopen = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_short = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_noise = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_lp_rawdata = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_lp_noise = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_gap_ver = HX_RAWDATA_NOT_READY;
	g_sec_raw_buff->f_ready_gap_hor = HX_RAWDATA_NOT_READY;

	kfree(g_sec_raw_buff);
	E("%s %s: End!\n", HIMAX_LOG_TAG,
			__func__);
}

EXPORT_SYMBOL(sec_touch_sysfs_remove);
#endif /* SEC_FACTORY_MODE */

