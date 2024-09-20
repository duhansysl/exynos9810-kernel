/*
 *	Copyright (C) 2010, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
*/
#ifndef __AK09916C_REG__
#define __AK09916C_REG__

/* Compass device dependent definition */
#define AK09916C_REG_WIA1			0x00
#define AK09916C_REG_WIA2			0x01
#define AK09916C_REG_INFO1			0x02
#define AK09916C_REG_INFO2			0x03
#define AK09916C_REG_ST1			0x10
#define AK09916C_REG_HXL			0x11
#define AK09916C_REG_HXH			0x12
#define AK09916C_REG_HYL			0x13
#define AK09916C_REG_HYH			0x14
#define AK09916C_REG_HZL			0x15
#define AK09916C_REG_HZH			0x16
#define AK09916C_REG_TMPS			0x17
#define AK09916C_REG_ST2			0x18
#define AK09916C_REG_CNTL1			0x30
#define AK09916C_REG_CNTL2			0x31
#define AK09916C_REG_CNTL3			0x32

#define AK09916C_FUSE_ASAX			0x60
#define AK09916C_FUSE_ASAY			0x61
#define AK09916C_FUSE_ASAZ			0x62

#define AK09916C_MODE_SNG_MEASURE		0x01
#define AK09916C_MODE_SELF_TEST			0x10
#define AK09916C_MODE_FUSE_ACCESS		0x1F
#define AK09916C_MODE_POWERDOWN			0x00
#define AK09916C_RESET_DATA			0x01

#endif /* __AK09916C_REG__ */
