/*
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef __TMD3725_H__
#define __TMD3725_H__

#include <linux/types.h>

#define ENABLE                  0x80
#define ALS_TIME                0x81
#define PRX_RATE                0x82
#define WAIT_TIME               0x83
#define ALS_MINTHRESHLO         0x84
#define ALS_MINTHRESHHI         0x85
#define ALS_MAXTHRESHLO         0x86
#define ALS_MAXTHRESHHI         0x87
#define PRX_MINTHRESH           0x88
#define PRX_MAXTHRESH           0x8A
#define PPERS                   0x8C
#define PGCFG0                  0x8E
#define PGCFG1                  0x8F
#define CFG1                    0x90

#define REVID                   0x91
#define CHIPID                  0x92
#define STATUS                  0x93
#define CLR_CHAN0LO             0x94
#define CLR_CHAN0HI             0x95
#define RED_CHAN1LO             0x96
#define RED_CHAN1HI             0x97
#define GRN_CHAN1LO             0x98
#define GRN_CHAN1HI             0x99
#define BLU_CHAN1LO             0x9A
#define BLU_CHAN1HI             0x9B
#define PRX_DATA_HIGH           0x9C
#define PRX_DATA_LOW            0x9D
#define TEST_STATUS             0x1F

#define CFG2                    0x9F
#define CFG3                    0xAB
#define CFG4                    0xAC
#define CFG5                    0xAD
#define POFFSET_L               0xC0
#define POFFSET_H               0xC1
#define AZ_CONFIG               0xD6
#define CALIB                   0xD7
#define CALIBCFG                0xD9
#define CALIBSTAT               0xDC
#define INTENAB                 0xDD

#define FACTORYTRIM             0xE6
#define FACTORYTRIMSIGN         0xE7

/*TMD3782 cmd reg masks*/
#define CMD_BYTE_RW             0x00
#define CMD_WORD_BLK_RW         0x20
#define CMD_PROX_INTCLR         0x05
#define CMD_ALS_INTCLR          0x06
#define CMD_PROXALS_INTCLR      0x80

#define PRX_PERSIST(p)          (((p) & 0xf) << 4)
#define ALS_PERSIST(p)          (((p) & 0xf) << 0)

#define CMD_TST_REG             0x08
#define CMD_USER_REG            0x09

/* TMD3782 cntrl reg masks */
#define CNTL_REG_CLEAR          0x00
#define CNTL_PROX_INT_ENBL      0x20
#define CNTL_ALS_INT_ENBL       0x10
#define CNTL_WAIT_TMR_ENBL      0x08
#define CNTL_PROX_DET_ENBL      0x04
#define CNTL_ADC_ENBL           0x02
#define CNTL_PWRON              0x01
#define CNTL_ALSPON_ENBL        0x03
#define CNTL_INTALSPON_ENBL     0x13
#define CNTL_PROXPON_ENBL       0x0F
#define CNTL_INTPROXPON_ENBL    0x2F

/* TMD3782 status reg masks */
#define STA_ADCVALID            0x01
#define STA_PRXVALID            0x02
#define STA_ADC_PRX_VALID       0x03
#define STA_ADCINTR             0x10
#define STA_PRXINTR             0x20

enum {
	LIGHT_ENABLED = BIT(0),
	PROXIMITY_ENABLED = BIT(1),
};

enum {
	PROX_CLOSE = 0,
	PROX_FAR = 1,
};

enum {
	OFF = 0,
	ON = 1,
};

enum INTENAB_REG {
	ASIEN = (0x1 << 7),
	PSIEN = (0x1 << 6),
	PIEN = (0x1 << 5),
	AIEN = (0x1 << 4),
	CIEN = (0x1 << 3),
	ZIEN = (0x1 << 2),
};

enum STATUS_REG {
	ASAT = (0x1 << 7),
	PSAT = (0x1 << 6),
	PINT = (0x1 << 5),
	AINT = (0x1 << 4),
	CINT = (0x1 << 3),
	ZINT = (0x1 << 2),
	PSAT_REFLECTIVE = (0x1 << 1),
	PSAT_AMBIENT = (0x1 << 0),
};

enum ENABLE_REG {
	WEN = (0x1 << 3),
	PEN = (0x1 << 2),
	AEN = (0x1 << 1),
	PON = (0x1),
};

enum CFG3_REG {
	INT_READ_CLEAR = (0x1 << 7),
};

enum CALIBCFG1_REG {
	BINSRCH_TARGET = (0x7 << 5),
	AUTO_OFFSET_ADJ = (0x1 << 3),
};

enum CALIBSTAT_REG {
	CALIB_FINISHED = (0x1 << 0),
};

enum CMD_REG {
	CMD_REG = (0x1 << 7),
	CMD_INCR = (0x1 << 5),
	CMD_SPL_FN = (0x3 << 5),
	CMD_PROX_INT_CLR = (0x5 << 0),
	CMD_ALS_INT_CLR = (0x6 << 0),
};

enum TAOS_OP_MODES {
	MODE_OFF = 0x00,
	MODE_ALS = 0x01, /* ALS */
	MODE_PROX = 0x02, /* Proximity */
	MODE_ALS_PROX = 0x03, /* ALS + Proximity */
};

enum {
	STATE_INIT = 0,
	STATE_HIGH_OFFSET = 1,
	STATE_DETECTION = 2,
	STATE_STILL_DETECTION = 3,
	STATE_RELEASE = 4,
	STATE_NONE = 5,
};

#endif
