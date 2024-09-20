#ifndef __S6E3FA9_PARAM_H__
#define __S6E3FA9_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

#define EXTEND_BRIGHTNESS	365
#define UI_MAX_BRIGHTNESS	255
#define UI_DEFAULT_BRIGHTNESS	128

#define NORMAL_TEMPERATURE	25	/* 25 degrees Celsius */

#define ACL_CMD_CNT				((u16)ARRAY_SIZE(SEQ_ACL_OFF))
#define HBM_CMD_CNT				((u16)ARRAY_SIZE(SEQ_HBM_OFF))
#define ELVSS_CMD_CNT				((u16)ARRAY_SIZE(SEQ_ELVSS_SET))

#define LDI_REG_BRIGHTNESS			0x51
#define LDI_REG_ID				0x04
#define LDI_REG_COORDINATE			0xA1
#define LDI_REG_DATE				LDI_REG_COORDINATE
#define LDI_REG_MANUFACTURE_INFO		LDI_REG_COORDINATE
#define LDI_REG_MANUFACTURE_INFO_CELL_ID	LDI_REG_COORDINATE
#define LDI_REG_CHIP_ID				0xD6

/* len is read length */
#define LDI_LEN_ID				3
#define LDI_LEN_COORDINATE			4
#define LDI_LEN_DATE				7
#define LDI_LEN_MANUFACTURE_INFO		4
#define LDI_LEN_MANUFACTURE_INFO_CELL_ID	16
#define LDI_LEN_CHIP_ID				5

/* offset is position including addr, not only para */
#define LDI_OFFSET_ACL		1			/* 55h 1st para */
#define LDI_OFFSET_HBM		1			/* 53h 1st para */
#define LDI_OFFSET_ELVSS_1	3			/* B5h 3rd para ELVSS interpolation */
#define LDI_OFFSET_ELVSS_2	1			/* B5h 1st para TSET */

#define LDI_GPARA_COORDINATE			0	/* A1h 1st Para: x, y */
#define LDI_GPARA_DATE				4	/* A1h 5th Para: [D7:D4]: Year */
#define LDI_GPARA_MANUFACTURE_INFO		11	/* A1h 12th Para ~ 15th Para */
#define LDI_GPARA_MANUFACTURE_INFO_CELL_ID	15	/* A1h 16th Para ~ 31th Para */

struct bit_info {
	unsigned int reg;
	unsigned int len;
	char **print;
	unsigned int expect;
	unsigned int offset;
	unsigned int g_para;
	unsigned int invert;
	unsigned int mask;
	unsigned int result;
};

enum {
	LDI_BIT_ENUM_05,	LDI_BIT_ENUM_RDNUMPE = LDI_BIT_ENUM_05,
	LDI_BIT_ENUM_0A,	LDI_BIT_ENUM_RDDPM = LDI_BIT_ENUM_0A,
	LDI_BIT_ENUM_0E,	LDI_BIT_ENUM_RDDSM = LDI_BIT_ENUM_0E,
	LDI_BIT_ENUM_0F,	LDI_BIT_ENUM_RDDSDR = LDI_BIT_ENUM_0F,
	LDI_BIT_ENUM_EE,	LDI_BIT_ENUM_ESDERR = LDI_BIT_ENUM_EE,
	LDI_BIT_ENUM_EA,	LDI_BIT_ENUM_DSIERR = LDI_BIT_ENUM_EA,
	LDI_BIT_ENUM_MAX
};

static char *LDI_BIT_DESC_05[BITS_PER_BYTE] = {
	[0 ... 6] = "number of corrupted packets",
	[7] = "overflow on number of corrupted packets",
};

static char *LDI_BIT_DESC_0A[BITS_PER_BYTE] = {
	[2] = "Display is Off",
	[7] = "Booster has a fault",
};

static char *LDI_BIT_DESC_0E[BITS_PER_BYTE] = {
	[0] = "Error on DSI",
};

static char *LDI_BIT_DESC_0F[BITS_PER_BYTE] = {
	[7] = "Register Loading Detection",
};

static char *LDI_BIT_DESC_EE[BITS_PER_BYTE] = {
	[2] = "VLIN3 error",
	[3] = "ELVDD error",
	[6] = "VLIN1 error",
};

static char *LDI_BIT_DESC_EA[BITS_PER_BYTE * 2] = {
	[0] = "SoT Error",
	[1] = "SoT Sync Error",
	[2] = "EoT Sync Error",
	[3] = "Escape Mode Entry Command Error",
	[4] = "Low-Power Transmit Sync Error",
	[5] = "HS RX Timeout",
	[6] = "False Control Error",
	[7] = "Data Lane Contention Detection",
	[8] = "ECC Error, single-bit (detected and corrected)",
	[9] = "ECC Error, multi-bit (detected, not corrected)",
	[10] = "Checksum Error",
	[11] = "DSI Data Type Not Recognized",
	[12] = "DSI VC ID Invalid",
	[13] = "Data P Lane Contention Detetion",
	[14] = "Data Lane Contention Detection",
	[15] = "DSI Protocol Violation",
};

static struct bit_info ldi_bit_info_list[LDI_BIT_ENUM_MAX] = {
	[LDI_BIT_ENUM_05] = {0x05, 1, LDI_BIT_DESC_05, 0x00, },
	[LDI_BIT_ENUM_0A] = {0x0A, 1, LDI_BIT_DESC_0A, 0x9C, .invert = (BIT(2) | BIT(7)), },
	[LDI_BIT_ENUM_0E] = {0x0E, 1, LDI_BIT_DESC_0E, 0x80, },
	[LDI_BIT_ENUM_0F] = {0x0F, 1, LDI_BIT_DESC_0F, 0xC0, .invert = (BIT(7)), },
	[LDI_BIT_ENUM_EE] = {0xEE, 1, LDI_BIT_DESC_EE, 0xC0, },
	[LDI_BIT_ENUM_EA] = {0xEA, 2, LDI_BIT_DESC_EA, 0x00, },
};

#if defined(CONFIG_DISPLAY_USE_INFO)
#define LDI_LEN_RDNUMPE		1		/* DPUI_KEY_PNDSIE: Read Number of the Errors on DSI */
#define LDI_PNDSIE_MASK		(GENMASK(7, 0))

/*
 * ESD_ERROR[0] = MIPI DSI error is occurred by ESD.
 * ESD_ERROR[1] = HS CLK lane error is occurred by ESD.
 * ESD_ERROR[2] = VLIN3 error is occurred by ESD.
 * ESD_ERROR[3] = ELVDD error is occurred by ESD.
 * ESD_ERROR[4] = CHECK_SUM error is occurred by ESD.
 * ESD_ERROR[5] = Internal HSYNC error is occurred by ESD.
 * ESD_ERROR[6] = VLIN1 error is occurred by ESD
 */
#define LDI_LEN_ESDERR		1		/* DPUI_KEY_PNELVDE, DPUI_KEY_PNVLI1E, DPUI_KEY_PNVLO3E, DPUI_KEY_PNESDE */
#define LDI_PNELVDE_MASK	(BIT(3))	/* ELVDD error */
#define LDI_PNVLI1E_MASK	(BIT(6))	/* VLIN1 error */
#define LDI_PNVLO3E_MASK	(BIT(2))	/* VLIN3 error */
#define LDI_PNESDE_MASK		(BIT(2) | BIT(3) | BIT(6))

#define LDI_LEN_RDDSDR		1		/* DPUI_KEY_PNSDRE: Read Display Self-Diagnostic Result */
#define LDI_PNSDRE_MASK		(BIT(7))	/* D7: REG_DET: Register Loading Detection */
#endif

struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

static unsigned char SEQ_SLEEP_OUT[] = {
	0x11
};

static unsigned char SEQ_SLEEP_IN[] = {
	0x10
};

static unsigned char SEQ_DISPLAY_ON[] = {
	0x29
};

static unsigned char SEQ_DISPLAY_OFF[] = {
	0x28
};

static unsigned char SEQ_TEST_KEY_ON_F0[] = {
	0xF0,
	0x5A, 0x5A
};

static unsigned char SEQ_TEST_KEY_OFF_F0[] = {
	0xF0,
	0xA5, 0xA5
};

static unsigned char SEQ_TEST_KEY_ON_FC[] = {
	0xFC,
	0x5A, 0x5A
};

static unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5
};

static unsigned char SEQ_BLACK_GRID_ON[] = {
	0xBF,
	0x01, 0x07, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};

static unsigned char SEQ_BLACK_GRID_OFF[] = {
	0xBF,
	0x00, 0x07, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00
};

static unsigned char SEQ_TE_ON[] = {
	0x35,
	0x00, 0x00
};

static unsigned char SEQ_PAGE_ADDR_SET_2A[] = {
	0x2A,
	0x00, 0x00, 0x04, 0x37
};

static unsigned char SEQ_PAGE_ADDR_SET_2B[] = {
	0x2B,
	0x00, 0x00, 0x09, 0x5F
};

static unsigned char SEQ_TSP_VSYNC_ON[] = {
	0xB9,
	0x00, 0x00, 0x14, 0x18, 0x00, 0x00, 0x00, 0x10
};

static unsigned char SEQ_ERR_FG_ENABLE[] = {
	0xE5,
	0x13
};

static unsigned char SEQ_ERR_FG_SET[] = {
	0xED,
	0x00, 0x4C, 0x40	/* 3rd 0x40 : default high */
};

static unsigned char SEQ_PCD_DET_LOW[] = {
	0xCC,
	0x5C
};

static unsigned char SEQ_PARTIAL_UPDATE[] = {
	0xC2,
	0x1B, 0x41, 0xB0, 0x0E, 0x00, 0x1E, 0x5A, 0x1E,
	0x1E
};

static unsigned char SEQ_FFC_SET[] = {
	0xC5,
	0x0D, 0x10, 0x64, 0x1E, 0x15, 0x05, 0x00, 0x23,
	0x50
};

static unsigned char SEQ_ELVSS_SET[] = {
	0xB5,
	0x19,	/* 1st: TSET */
	0x8D,
	0x16,	/* 3rd: ELVSS return */
};

static unsigned char SEQ_HBM_ON[] = {
	0x53,
	0xE8,
};

static unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x28,
};

static unsigned char SEQ_HBM_ON_DIMMING_OFF[] = {
	0x53,
	0xE0,
};

static unsigned char SEQ_HBM_OFF_DIMMING_OFF[] = {
	0x53,
	0x20,
};

static unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00
};

static unsigned char SEQ_ACL_08P[] = {
	0x55,
	0x01
};

static unsigned char SEQ_ACL_15P[] = {
	0x55,
	0x03
};

#if defined(CONFIG_SUPPORT_DOZE)
enum {
	ALPM_OFF,
	ALPM_ON_LOW,	/* ALPM 2 NIT */
	HLPM_ON_LOW,	/* HLPM 2 NIT */
	ALPM_ON_HIGH,	/* ALPM 60 NIT */
	HLPM_ON_HIGH,	/* HLPM 60 NIT */
	ALPM_MODE_MAX
};

enum {
	AOD_MODE_OFF,
	AOD_MODE_ALPM,
	AOD_MODE_HLPM,
	AOD_MODE_MAX
};

enum {
	AOD_HLPM_OFF,
	AOD_HLPM_02_NIT,
	AOD_HLPM_10_NIT,
	AOD_HLPM_30_NIT,
	AOD_HLPM_60_NIT,
	AOD_HLPM_STATE_MAX
};

static const char *AOD_HLPM_STATE_NAME[AOD_HLPM_STATE_MAX] = {
	"HLPM_OFF",
	"HLPM_02_NIT",
	"HLPM_10_NIT",
	"HLPM_30_NIT",
	"HLPM_60_NIT",
};

static unsigned int lpm_old_table[ALPM_MODE_MAX] = {
	AOD_HLPM_OFF,
	AOD_HLPM_02_NIT,
	AOD_HLPM_02_NIT,
	AOD_HLPM_60_NIT,
	AOD_HLPM_60_NIT,
};

static unsigned int lpm_brightness_table[EXTEND_BRIGHTNESS + 1] = {
	[0 ... 39]			= AOD_HLPM_02_NIT,
	[40 ... 70]			= AOD_HLPM_10_NIT,
	[71 ... 93]			= AOD_HLPM_30_NIT,
	[94 ... EXTEND_BRIGHTNESS]	= AOD_HLPM_60_NIT,
};

static unsigned char SEQ_HLPM_AOR_60[] = {
	0xBB,
	0x09, 0x0C, 0x0A, 0x18
};

static unsigned char SEQ_HLPM_AOR_30[] = {
	0xBB,
	0x09, 0x0C, 0x5A, 0x84
};

static unsigned char SEQ_HLPM_AOR_10[] = {
	0xBB,
	0x09, 0x0C, 0x8A, 0xD8
};

static unsigned char SEQ_HLPM_ON_H[] = {
	0x53,
	0x22
};

static unsigned char SEQ_HLPM_ON_L[] = {
	0x53,
	0x23
};

static unsigned char SEQ_HLPM_OFF[] = {
	0x53,
	0x20
};

static unsigned char SEQ_HLPM_ASWIRE_NOPULSE_GPARA[] = {
	0xB0,
	0x0D, 0xB5
};

static unsigned char SEQ_HLPM_ASWIRE_NOPULSE_SET[] = {
	0xB5,
	0x80
};

static unsigned char SEQ_NORMAL_ASWIRE_NOPULSE_GPARA[] = {
	0xB0,
	0x0A, 0xB5
};

static unsigned char SEQ_NORMAL_ASWIRE_NOPULSE_SET[] = {
	0xB5,
	0x00
};

static struct lcd_seq_info LCD_SEQ_HLPM_60_NIT[] = {
	{SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0) },
	{SEQ_HLPM_ASWIRE_NOPULSE_GPARA, ARRAY_SIZE(SEQ_HLPM_ASWIRE_NOPULSE_GPARA) },
	{SEQ_HLPM_ASWIRE_NOPULSE_SET, ARRAY_SIZE(SEQ_HLPM_ASWIRE_NOPULSE_SET) },
	{SEQ_HLPM_AOR_60, ARRAY_SIZE(SEQ_HLPM_AOR_60) },
	{SEQ_HLPM_ON_H, ARRAY_SIZE(SEQ_HLPM_ON_H), 1},
	{SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0) },
};

static struct lcd_seq_info LCD_SEQ_HLPM_30_NIT[] = {
	{SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0) },
	{SEQ_HLPM_ASWIRE_NOPULSE_GPARA, ARRAY_SIZE(SEQ_HLPM_ASWIRE_NOPULSE_GPARA) },
	{SEQ_HLPM_ASWIRE_NOPULSE_SET, ARRAY_SIZE(SEQ_HLPM_ASWIRE_NOPULSE_SET) },
	{SEQ_HLPM_AOR_30, ARRAY_SIZE(SEQ_HLPM_AOR_30) },
	{SEQ_HLPM_ON_H, ARRAY_SIZE(SEQ_HLPM_ON_H), 1},
	{SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0) },
};

static struct lcd_seq_info LCD_SEQ_HLPM_10_NIT[] = {
	{SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0) },
	{SEQ_HLPM_ASWIRE_NOPULSE_GPARA, ARRAY_SIZE(SEQ_HLPM_ASWIRE_NOPULSE_GPARA) },
	{SEQ_HLPM_ASWIRE_NOPULSE_SET, ARRAY_SIZE(SEQ_HLPM_ASWIRE_NOPULSE_SET) },
	{SEQ_HLPM_AOR_10, ARRAY_SIZE(SEQ_HLPM_AOR_10) },
	{SEQ_HLPM_ON_H, ARRAY_SIZE(SEQ_HLPM_ON_H), 1},
	{SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0) },
};

static struct lcd_seq_info LCD_SEQ_HLPM_02_NIT[] = {
	{SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0) },
	{SEQ_HLPM_ASWIRE_NOPULSE_GPARA, ARRAY_SIZE(SEQ_HLPM_ASWIRE_NOPULSE_GPARA) },
	{SEQ_HLPM_ASWIRE_NOPULSE_SET, ARRAY_SIZE(SEQ_HLPM_ASWIRE_NOPULSE_SET) },
	{SEQ_HLPM_ON_L, ARRAY_SIZE(SEQ_HLPM_ON_L), 1},
	{SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0) },
};

static struct lcd_seq_info LCD_SEQ_HLPM_OFF[] = {
	{SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0) },
	{SEQ_NORMAL_ASWIRE_NOPULSE_GPARA, ARRAY_SIZE(SEQ_NORMAL_ASWIRE_NOPULSE_GPARA) },
	{SEQ_NORMAL_ASWIRE_NOPULSE_SET, ARRAY_SIZE(SEQ_NORMAL_ASWIRE_NOPULSE_SET) },
	{SEQ_HLPM_OFF, ARRAY_SIZE(SEQ_HLPM_OFF), 1},
	{SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0) },
};
#endif

static unsigned char SEQ_XTALK_B0[] = {
	0xB0,
	0x1C
};

static unsigned char SEQ_XTALK_ON[] = {
	0xD9,
	0x60
};

static unsigned char SEQ_XTALK_OFF[] = {
	0xD9,
	0xC0
};

#if defined(CONFIG_SEC_FACTORY)
static unsigned char SEQ_FD_ON[] = {
	0xB5,
	0x40	/* FD enable */
};

static unsigned char SEQ_FD_OFF[] = {
	0xB5,
	0x80	/* FD disable */
};

static unsigned char SEQ_GPARA_FD[] = {
	0xB0,
	0x0B, 0xB5
};
#endif

enum {
	ACL_STATUS_OFF,
	ACL_STATUS_08P,
	ACL_STATUS_15P,
	ACL_STATUS_MAX,
};

enum {
	TEMP_ABOVE_MINUS_00_DEGREE,	/* T > 0 */
	TEMP_ABOVE_MINUS_15_DEGREE,	/* -15 < T <= 0 */
	TEMP_BELOW_MINUS_15_DEGREE,	/* T <= -15 */
	TEMP_MAX
};

enum {
	HBM_STATUS_OFF,
	HBM_STATUS_ON,
	HBM_STATUS_MAX
};

enum {
	TRANS_DIMMING_OFF,
	TRANS_DIMMING_ON,
	TRANS_DIMMING_MAX
};

static unsigned char *HBM_TABLE[TRANS_DIMMING_MAX][HBM_STATUS_MAX] = {
	{SEQ_HBM_OFF_DIMMING_OFF, SEQ_HBM_ON_DIMMING_OFF},
	{SEQ_HBM_OFF, SEQ_HBM_ON}
};

static unsigned char *ACL_TABLE[ACL_STATUS_MAX] = {SEQ_ACL_OFF, SEQ_ACL_08P, SEQ_ACL_15P};

/* platform brightness <-> acl opr and percent */
static unsigned int brightness_opr_table[ACL_STATUS_MAX][EXTEND_BRIGHTNESS + 1] = {
	{
		[0 ... EXTEND_BRIGHTNESS]			= ACL_STATUS_OFF,
	}, {
		[0 ... UI_MAX_BRIGHTNESS]			= ACL_STATUS_15P,
		[UI_MAX_BRIGHTNESS + 1 ... EXTEND_BRIGHTNESS]	= ACL_STATUS_08P
	}
};

/* platform brightness <-> gamma level */
static unsigned int brightness_table[EXTEND_BRIGHTNESS + 1] = {
	3, 6, 9, 12, 15, 18, 21, 24, 27, 30,
	33, 36, 39, 42, 45, 48, 52, 55, 59, 62,
	66, 69, 73, 77, 80, 84, 87, 91, 94, 98,
	101, 105, 109, 112, 116, 119, 123, 126, 130, 134,
	137, 141, 144, 148, 151, 155, 159, 162, 166, 169,
	173, 176, 180, 184, 187, 191, 194, 198, 201, 205,
	208, 212, 216, 219, 223, 226, 230, 233, 237, 241,
	244, 248, 251, 255, 258, 262, 266, 269, 273, 276,
	280, 283, 287, 290, 294, 298, 301, 305, 308, 312,
	315, 319, 323, 326, 330, 333, 337, 340, 344, 348,
	351, 355, 358, 362, 365, 369, 373, 376, 380, 383,
	387, 390, 394, 397, 401, 405, 408, 412, 415, 419,
	422, 426, 430, 433, 437, 440, 444, 447, 451, 455,  // 128 = 183nit = 451
	460, 464, 469, 474, 478, 483, 487, 492, 496, 501,
	505, 510, 514, 519, 523, 528, 532, 537, 541, 546,
	550, 555, 559, 564, 568, 573, 577, 582, 586, 591,
	595, 600, 604, 609, 613, 618, 622, 627, 631, 636,
	640, 645, 649, 654, 658, 663, 667, 672, 676, 681,
	685, 690, 694, 699, 703, 708, 712, 717, 721, 726,
	730, 735, 739, 744, 748, 753, 757, 762, 766, 771,
	775, 780, 784, 789, 793, 798, 802, 807, 811, 816,
	820, 825, 829, 834, 838, 843, 847, 852, 856, 861,
	865, 870, 874, 879, 883, 888, 892, 897, 901, 906,
	910, 915, 919, 924, 928, 933, 937, 942, 946, 951,
	955, 960, 964, 969, 973, 978, 982, 987, 991, 996,
	1000, 1005, 1009, 1014, 1018, 1023,					// 255 = 420nit = 1023
	// hbm
	3, 7, 11, 15, 18, 22, 26, 29, 33, 37,
	40, 44, 48, 51, 55, 59, 62, 66, 70, 73,
	77, 81, 84, 88, 92, 95, 99, 103, 107, 110,
	114, 118, 121, 125, 129, 132, 136, 140, 143, 147,
	151, 154, 158, 162, 165, 169, 173, 176, 180, 184,
	187, 191, 195, 198, 202, 206, 209, 213, 217, 220,
	224, 228, 231, 235, 239, 242, 246, 250, 253, 257,
	261, 264, 268, 272, 275, 279, 283, 286, 290, 294,
	297, 301, 305, 309, 312, 316, 320, 323, 327, 331,
	334, 338, 342, 345, 349, 353, 356, 360, 364, 367,
	371, 375, 378, 382, 386, 389, 393, 397, 400, 404,	// 365 = 600nit = 404
};

static unsigned int elvss_table[EXTEND_BRIGHTNESS + 1] = {
	[0 ... 255] = 0x16,
	[256 ... 256] = 0x1F,
	[257 ... 268] = 0x1E,
	[269 ... 282] = 0x1D,
	[283 ... 295] = 0x1C,
	[296 ... 309] = 0x1B,
	[310 ... 323] = 0x19,
	[324 ... 336] = 0x18,
	[337 ... 350] = 0x17,
	[351 ... EXTEND_BRIGHTNESS] = 0x16,
};
#endif /* __S6E3FA9_PARAM_H__ */
