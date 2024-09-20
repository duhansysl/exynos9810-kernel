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

#if defined(CONFIG_TOUCHSCREEN_HIMAX_I2C)
#include "himax_platform.h"
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_SPI)
#include "himax_platform_SPI.h"
#endif

#include "himax_common.h"
#include "himax_ic_core.h"

/*#define HX_GAP_TEST*/
/*#define HX_INSPECT_LPWUG_TEST*/
/*#define HX_ACT_IDLE_TEST*/

#define HX_RSLT_OUT_PATH "/sdcard/"
#define HX_RSLT_OUT_FILE "hx_test_result.txt"
#define PI(x...) pr_info(x)
#define HX_SZ_ICID 60

#ifdef HX_ESD_RECOVERY
	extern u8 HX_ESD_RESET_ACTIVATE;
#endif

/* Firmware update error code */
enum fw_update_errno {
	FW_ERR_FILE_READ = -4,
	FW_ERR_FILE_OPEN = -3,
	FW_ERR_FILE_SIGN = -2,
	FW_ERR_DOWNLOAD = -1,
	FW_ERR_NONE = 0,
	FW_ERR_UPTODATE = 1,
};

#define BS_RAWDATA	10
#define BS_NOISE	100
#define BS_OPENSHORT	0
#define	BS_LPWUG	1
#define	BS_LPWUG_dile	1
#define	BS_ACT_IDLE	1

/* skip notch & dummy */
#define SKIP_NOTCH_START 5
#define SKIP_NOTCH_END 10
/* TX+SKIP_NOTCH_START */
#define SKIP_DUMMY_START 23
/* TX+SKIP_NOTCH_END*/
#define SKIP_DUMMY_END 28


#define	NOISEFRAME	(BS_NOISE+1)
#define NORMAL_IDLE_RAWDATA_NOISEFRAME 10
#define LPWUG_RAWDATAFRAME 1
#define LPWUG_NOISEFRAME 1
#define LPWUG_IDLE_RAWDATAFRAME 1
#define LPWUG_IDLE_NOISEFRAME 1

#define OTHERSFRAME	2

#define	UNIFMAX	500


/*Himax MP Password*/
#define	PWD_OPEN_START	0x77
#define	PWD_OPEN_END	0x88
#define	PWD_SHORT_START	0x11
#define	PWD_SHORT_END	0x33
#define	PWD_RAWDATA_START	0x00
#define	PWD_RAWDATA_END	0x99
#define	PWD_NOISE_START	0x00
#define	PWD_NOISE_END	0x99
#define	PWD_SORTING_START	0xAA
#define	PWD_SORTING_END	0xCC


#define PWD_ACT_IDLE_START		0x22
#define PWD_ACT_IDLE_END		0x44



#define PWD_LPWUG_START	0x55
#define PWD_LPWUG_END	0x66

#define PWD_LPWUG_IDLE_START	0x50
#define PWD_LPWUG_IDLE_END	0x60

#define PWD_TURN_ON_MPAP_OVL 0x107380

/*Himax DataType*/
#define DATA_SORTING	0x0A
#define DATA_OPEN	0x0B
#define DATA_MICRO_OPEN	0x0C
#define DATA_SHORT	0x0A
#define DATA_RAWDATA	0x0A
#define DATA_NOISE	0x0F
#define DATA_BACK_NORMAL	0x00
#define DATA_LPWUG_RAWDATA	0x0C
#define DATA_LPWUG_NOISE	0x0F
#define DATA_ACT_IDLE_RAWDATA	0x0A
#define DATA_ACT_IDLE_NOISE	0x0F
#define DATA_LPWUG_IDLE_RAWDATA	0x0A
#define DATA_LPWUG_IDLE_NOISE	0x0F

/*Himax Data Ready Password*/
#define	Data_PWD0	0xA5
#define	Data_PWD1	0x5A

/* ASCII format */
#define ASCII_LF	(0x0A)
#define ASCII_CR	(0x0D)
#define ASCII_COMMA	(0x2C)
#define ASCII_ZERO	(0x30)
#define CHAR_EL	'\0'
#define CHAR_NL	'\n'
#define ACSII_SPACE	(0x20)
/* INSOECTION Setting */

void himax_inspection_init(void);
extern int *g_test_item_flag;
extern int HX_CRITERIA_ITEM;
extern int *g_test_item_flag;
extern char *g_himax_inspection_mode[];

/*Inspection register*/
#define addr_normal_noise_thx 0x1000708C
#define addr_lpwug_noise_thx 0x10007090
#define addr_recal_thx 0x10007090
#define addr_palm_num 0x100070A8
#define addr_weight_sup 0x100072C8
#define addr_normal_weight_a 0x1000709C
#define addr_lpwug_weight_a 0x100070A0
#define addr_weight_b 0x10007094
#define addr_max_dc 0x10007FC8
#define addr_skip_frame 0x100070F4
#define addr_neg_noise_sup 0x10007FD8
#define data_neg_noise 0x7F0C0000
#define addr_ctrl_mpap_ovl 0x100073EC

/*Compensation Zone register*/
#define addr_rotative_mode 0x10007F3C
#define data_portrait 0xA55AA55A
#define data_landscape 0xA11AA11A
#define addr_grip_zone 0x10007268
#define addr_reject_zone 0x1000726C
#define addr_reject_zone_boud 0x10007270
#define addr_except_zone 0x10007274

/*Need to map *g_himax_inspection_mode[]*/
enum THP_INSPECTION_ENUM {
	HIMAX_OPEN,
	HIMAX_MICRO_OPEN,
	HIMAX_SHORT,
	HIMAX_RAWDATA,
	HIMAX_BPN_RAWDATA,
	HIMAX_SC,
	HIMAX_WEIGHT_NOISE,
	HIMAX_ABS_NOISE,
	HIMAX_SORTING,

	HIMAX_GAPTEST_RAW,
	/*HIMAX_GAPTEST_RAW_X,*/
	/*HIMAX_GAPTEST_RAW_Y,*/

	HIMAX_ACT_IDLE_RAWDATA,
	HIMAX_ACT_IDLE_BPN_RAWDATA,
	HIMAX_ACT_IDLE_NOISE,
/*LPWUG test must put after Normal test*/
	HIMAX_LPWUG_RAWDATA,
	HIMAX_LPWUG_BPN_RAWDATA,
	HIMAX_LPWUG_WEIGHT_NOISE,
	HIMAX_LPWUG_ABS_NOISE,
	HIMAX_LPWUG_IDLE_RAWDATA,
	HIMAX_LPWUG_IDLE_BPN_RAWDATA,
	HIMAX_LPWUG_IDLE_NOISE,

	HIMAX_BACK_NORMAL,/*Must put in the end*/
};


enum HX_CRITERIA_ENUM {
	IDX_RAWMIN		= 0,
	IDX_RAWMAX,
	IDX_BPN_RAWMIN,
	IDX_BPN_RAWMAX,
	IDX_SCMIN,
	IDX_SCMAX,
	IDX_SC_GOLDEN,
	IDX_SHORTMIN,
	IDX_SHORTMAX,
	IDX_OPENMIN,
	IDX_OPENMAX,
	IDX_M_OPENMIN,
	IDX_M_OPENMAX,
	IDX_WT_NOISEMIN,
	IDX_WT_NOISEMAX,
	IDX_ABS_NOISEMIN,
	IDX_ABS_NOISEMAX,
	IDX_SORTMIN,
	IDX_SORTMAX,

	IDX_GAP_HOR_RAWMAX,
	IDX_GAP_HOR_RAWMIN,
	IDX_GAP_VER_RAWMAX,
	IDX_GAP_VER_RAWMIN,

	IDX_ACT_IDLE_NOISE_MIN,
	IDX_ACT_IDLE_NOISE_MAX,
	IDX_ACT_IDLE_RAWDATA_MIN,
	IDX_ACT_IDLE_RAWDATA_MAX,
	IDX_ACT_IDLE_RAW_BPN_MIN,
	IDX_ACT_IDLE_RAW_BPN_MAX,

	IDX_LPWUG_WT_NOISEMIN,
	IDX_LPWUG_WT_NOISEMAX,
	IDX_LPWUG_NOISE_ABS_MIN,
	IDX_LPWUG_NOISE_ABS_MAX,
	IDX_LPWUG_RAWDATA_MIN,
	IDX_LPWUG_RAWDATA_MAX,
	IDX_LPWUG_RAW_BPN_MIN,
	IDX_LPWUG_RAW_BPN_MAX,

	IDX_LPWUG_IDLE_NOISE_MIN,
	IDX_LPWUG_IDLE_NOISE_MAX,
	IDX_LPWUG_IDLE_RAWDATA_MIN,
	IDX_LPWUG_IDLE_RAWDATA_MAX,
	IDX_LPWUG_IDLE_RAW_BPN_MIN,
	IDX_LPWUG_IDLE_RAW_BPN_MAX,
};

#define ERR_SFT 4
/* Error code of Inspection */
enum HX_INSPECT_ERR_ENUM {
	HX_INSPECT_OK	= 0,               /* OK */
	HX_INSPECT_EFILE = 1,					/*Criteria file error*/
	HX_INSPECT_EGETRAW = 1 << 1,	/* Get raw data errors */
	HX_INSPECT_MEMALLCTFAIL = 1 << 2,	/* Memory allocate errors */
	HX_INSPECT_ESWITCHMODE = 1 << 3,		/*Switch mode error*/

	HX_EOPEN = 1 << (HIMAX_OPEN + ERR_SFT),        /* Sensor open error */
	HX_EMOPEN = 1 << (HIMAX_MICRO_OPEN + ERR_SFT),        /* Sensor micro open error */
	HX_ESHORT = 1 << (HIMAX_SHORT + ERR_SFT),        /* Sensor short error */
	HX_ERAW = 1 << (HIMAX_RAWDATA + ERR_SFT),        /* Raw data error */
	HX_EBPNRAW = 1 << (HIMAX_BPN_RAWDATA + ERR_SFT),        /* Raw data BPN error */
	HX_ESC = 1 << (HIMAX_SC + ERR_SFT),	/* Get SC errors */
	HX_WT_ENOISE = 1 << (HIMAX_WEIGHT_NOISE + ERR_SFT),        /* Noise error */
	HX_ABS_ENOISE = 1 << (HIMAX_ABS_NOISE + ERR_SFT),        /* Noise error */
	HX_ESORT = 1 << (HIMAX_SORTING + ERR_SFT),		/*Sorting error*/
	HX_EGAP_RAW = 1 << (HIMAX_GAPTEST_RAW + ERR_SFT),        /* Raw Data GAP  */
	HX_EACT_IDLE_RAW = 1 << (HIMAX_ACT_IDLE_RAWDATA + ERR_SFT),		   /* ACT_IDLE RAW ERROR */
	HX_EACT_IDLE_NOISE = 1 << (HIMAX_ACT_IDLE_NOISE + ERR_SFT),		   /* ACT_IDLE NOISE ERROR */
	HX_ELPWUG_RAW = 1 << (HIMAX_LPWUG_RAWDATA + ERR_SFT),		   /* LPWUG RAW ERROR */
	HX_ELPWUG_WT_NOISE = 1 << (HIMAX_LPWUG_WEIGHT_NOISE + ERR_SFT),		   /* LPWUG NOISE ERROR */
	HX_ELPWUG_ABS_NOISE = 1 << (HIMAX_LPWUG_ABS_NOISE + ERR_SFT),		   /* LPWUG NOISE ERROR */
	HX_ELPWUG_IDLE_RAW = 1 << (HIMAX_LPWUG_IDLE_RAWDATA + ERR_SFT),		   /* LPWUG IDLE RAW ERROR */
	HX_ELPWUG_IDLE_NOISE = 1 << (HIMAX_LPWUG_IDLE_NOISE + ERR_SFT),		   /* LPWUG IDLE NOISE ERROR */
	HX_EACT_IDLE_BPNRAW = 1 << (HIMAX_ACT_IDLE_BPN_RAWDATA + ERR_SFT),
	HX_ELPWUG_BPNRAW = 1 << (HIMAX_LPWUG_BPN_RAWDATA + ERR_SFT),
	HX_ELPWUG_IDLE_BPNRAW = 1 << (HIMAX_LPWUG_IDLE_BPN_RAWDATA + ERR_SFT),
};

#ifdef SEC_FACTORY_MODE
struct sec_rawdata_buffs {
	uint32_t *_rawdata;
	uint32_t *_open;
	uint32_t *_mopen;
	uint32_t *_short;
	int *_noise;
	uint32_t *_lp_rawdata;
	int *_lp_noise;
	int *_gap_ver;
	int *_gap_hor;
	int f_ready_rawdata;
	int f_ready_open;
	int f_ready_mopen;
	int f_ready_short;
	int f_ready_noise;
	int f_ready_lp_rawdata;
	int f_ready_lp_noise;
	int f_ready_gap_ver;
	int f_ready_gap_hor;
	int f_crtra_ready;
};

/* Error code of Inspection */
typedef enum {
	HX_NO_SUPPORT = -4,
	HX_RAW_NOT_READY,
	HX_THRESHOLD_NOT_READY,
	HX_RAWDATA_NOT_READY,
	HX_NO_ERR = 0,      /* OK */
	HX_THRESHOLD_READY,
	HX_RAWDATA_READY,
} HX_SEC_TEST_ERR_ENUM;
#endif /* SEC_FACTORY_MODE */

extern void himax_inspect_data_clear(void);
