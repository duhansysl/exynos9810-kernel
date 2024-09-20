/*
 * s2mu106_pmeter.h - Header of S2MU106 Powermeter Driver
 *
 * Copyright (C) 2016 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef S2MU106_PMETER_H
#define S2MU106_PMETER_H
#include <linux/mfd/samsung/s2mu106.h>
#include <linux/power_supply.h>
#include "sec_charging_common.h"

#define S2MU106_PM_VALUP1	0x03
#define S2MU106_PM_VALUP2	0x04
#define S2MU106_PM_INT1		0x05
#define S2MU106_PM_INT2		0x06
#define S2MU106_PM_VALUP1_MASK	0x0B
#define S2MU106_PM_VALUP2_MASK	0x0C
#define S2MU106_PM_INT1_MASK	0x0D
#define S2MU106_PM_INT2_MASK	0x0E

#define S2MU106_PM_CO_MASK1	0x59
#define S2MU106_PM_CO_MASK2	0x5A
#define S2MU106_PM_CAL_DIS1	0x5B
#define S2MU106_PM_CAL_DIS2	0x5C
#define S2MU106_PM_REQ_BOX_RR1	0x5D
#define S2MU106_PM_REQ_BOX_RR2	0x5E
#define S2MU106_PM_REQ_BOX_CO1	0x5F
#define S2MU106_PM_REQ_BOX_CO2	0x60
#define S2MU106_PM_CTRL1	0x61
#define S2MU106_PM_CTRL2	0x62
#define S2MU106_PM_CTRL3	0x63
#define S2MU106_PM_CTRL4	0x64

#define S2MU106_PM_VAL1_VCHGIN	0x12
#define S2MU106_PM_VAL2_VCHGIN	0x13
#define S2MU106_PM_VAL1_VWCIN	0x14
#define S2MU106_PM_VAL2_VWCIN	0x15
#define S2MU106_PM_VAL1_VBYP	0x16
#define S2MU106_PM_VAL2_VBYP	0x17
#define S2MU106_PM_VAL1_VSYS	0x18
#define S2MU106_PM_VAL2_VSYS	0x19
#define S2MU106_PM_VAL1_VBAT	0x1A
#define S2MU106_PM_VAL2_VBAT	0x1B
#define S2MU106_PM_VAL1_VCC1	0x1C
#define S2MU106_PM_VAL2_VCC1	0x1D
#define S2MU106_PM_VAL1_VGPADC	0x1E
#define S2MU106_PM_VAL2_VGPADC	0x1F
#define S2MU106_PM_VAL1_ICHGIN	0x20
#define S2MU106_PM_VAL2_ICHGIN	0x21
#define S2MU106_PM_VAL1_IWCIN	0x22
#define S2MU106_PM_VAL2_IWCIN	0x23
#define S2MU106_PM_VAL1_VCC2	0x24
#define S2MU106_PM_VAL2_VCC2	0x25
#define S2MU106_PM_VAL1_IOTG	0x26
#define S2MU106_PM_VAL2_IOTG	0x27
#define S2MU106_PM_VAL1_ITX	0x28
#define S2MU106_PM_VAL2_ITX	0x29
#define S2MU106_PM_HYST_LEVEL1	0xA3
#define S2MU106_PM_HYST_CONTI1	0xA9

enum pm_type {
	PM_TYPE_VCHGIN = 0,
	PM_TYPE_VWCIN,
	PM_TYPE_VBYP,
	PM_TYPE_VSYS,
	PM_TYPE_VBAT,
	PM_TYPE_VCC1,
	PM_TYPE_VGPADC,
	PM_TYPE_ICHGIN,
	PM_TYPE_IWCIN,
	PM_TYPE_VCC2,
	PM_TYPE_IOTG,
	PM_TYPE_ITX,
	PM_TYPE_MAX,
};

enum { 
	CONTINUOUS_MODE = 0,
	REQUEST_RESPONSE_MODE,
};
#if 0
enum power_supply_pm_property {
	POWER_SUPPLY_PROP_VWCIN,
	POWER_SUPPLY_PROP_VBYP,
	POWER_SUPPLY_PROP_VSYS,
	POWER_SUPPLY_PROP_VBAT,
	POWER_SUPPLY_PROP_VGPADC,
	POWER_SUPPLY_PROP_VCC1,
	POWER_SUPPLY_PROP_VCC2,
	POWER_SUPPLY_PROP_ICHGIN,
	POWER_SUPPLY_PROP_IWCIN,
	POWER_SUPPLY_PROP_IOTG,
	POWER_SUPPLY_PROP_ITX,
	POWER_SUPPLY_PROP_CO_ENABLE,
	POWER_SUPPLY_PROP_RR_ENABLE,
};
#endif

#define HYST_LEV_VCHGIN_SHIFT	4
#define HYST_LEV_VCHGIN_MASK	0x70

enum {
	HYST_LEV_VCHGIN_80mV	=	0x1,
	HYST_LEV_VCHGIN_160mV	=	0x2,
	HYST_LEV_VCHGIN_320mV	=	0x3,
	HYST_LEV_VCHGIN_640mV	=	0x4,
	HYST_LEV_VCHGIN_1280mV	=	0x5,
	HYST_LEV_VCHGIN_2560mV	=	0x6,
	HYST_LEV_VCHGIN_5120mV	=	0x7,
};

#define HYST_CON_VCHGIN_SHIFT	6
#define HYST_CON_VCHGIN_MASK	0xC0
enum {
	HYST_CON_VCHGIN_1	=	0x1,
	HYST_CON_VCHGIN_4	=	0x2,
	HYST_CON_VCHGIN_16	=	0x3,
	HYST_CON_VCHGIN_32	=	0x4,
};

struct s2mu106_pmeter_data {
	struct i2c_client       *i2c;
	struct device *dev;
	struct s2mu106_platform_data *s2mu106_pdata;

	int irq_vchgin;

	struct power_supply	*psy_pm;
	struct power_supply_desc psy_pm_desc;

	struct mutex 	pmeter_mutex;
};

#endif /*S2MU106_PMETER_H*/
