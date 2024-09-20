/*
 * leds-s2mu107.h - Flash-led driver for Samsung S2MU107
 *
 * Copyright (C) 2019 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __LEDS_S2MU107_FLASH_H__
#define __LEDS_S2MU107_FLASH_H__
#include <linux/leds.h>
#include "../../drivers/battery/common/include/sec_charging_common.h"

#if defined(CONFIG_IFCONN_NOTIFIER)
#include <linux/ifconn/ifconn_notifier.h>
#include <linux/ifconn/ifconn_manager.h>
#include <linux/muic/muic.h>
#endif

#define MASK(width, shift)		(((0x1 << (width)) - 1) << shift)

#define FLED_EN				0

#define S2MU107_CH_MAX			1
#define S2MU107_FLASH_LIGHT_MAX 	5 /* TODO: random value */

/* Interrupt register */
#define S2MU107_FLED_INT1		0x02
#define S2MU107_FLED_INT2		0x03

#define S2MU107_FLED_INT1_MASK		0x0A
#define S2MU107_FLED_INT2_MASK		0x0B

/* Status register */
#define S2MU107_FLED_STATUS1		0x10
#define S2MU107_FLED_STATUS2		0x11

/* Mask for status register */
#define S2MU107_CH1_FLASH_ON		MASK(1,7)
#define S2MU107_CH1_TORCH_ON		MASK(1,6)
#define S2MU107_VBYP_OK_WARN_STATUS	MASK(1,0)

#define S2MU107_DHC_DET_ON		MASK(1,7)
#define S2MU107_DHC_Function_Hold	MASK(1,6)
#define S2MU107_FLED_POWER_PATH_MODE	MASK(4,0)

#define S2MU107_FLED_ON_CHECK		MASK(2,6)
/* Channel control register */
#define S2MU107_FLED_CH1_CTRL0		0x12
#define S2MU107_FLED_CH1_CTRL1		0x13
#define S2MU107_FLED_CH1_CTRL2		0x14

/* Mask for channel control register */
#define S2MU107_CHX_OPEN_PROT_EN	MASK(1,7)
#define S2MU107_CHX_SHORT_PROT_EN	MASK(1,6)
#define S2MU107_CHX_FLASH_IOUT		MASK(5,0)

#define S2MU107_CHX_TORCH_TMR_MODE	MASK(1,7)
#define S2MU107_CHX_DIS_TORCH_TMR	MASK(1,6)
#define S2MU107_CHX_FLASH_TMR_MODE	MASK(1,5)
#define S2MU107_CHX_TORCH_IOUT		MASK(5,0)

#define S2MU107_CHX_FLASH_TMR_DUR	MASK(4,4)
#define S2MU107_CHX_TORCH_TMR_DUR	MASK(4,0)

/* Mode control */
#define S2MU107_FLED_CTRL0		0x15
#define S2MU107_FLED_CTRL1		0x16
#define S2MU107_FLED_CTRL2		0x17
#define S2MU107_FLED_CTRL3		0x18
#define S2MU107_FLED_CTRL4		0x19
#define S2MU107_FLED_CTRL5		0x1A

#define S2MU107_FLED_TEST0		0x1B
#define S2MU107_FLED_TEST1		0x1C
#define S2MU107_FLED_TEST2		0x1D
#define S2MU107_FLED_TEST3		0x1E
#define S2MU107_FLED_TEST4		0x1F

/* Mask for mode control register */
#define S2MU107_FLED_MODE		MASK(2,6)
#define S2MU107_FLED_MODE_SHIFT		6
#define S2MU107_EN_FLED_PRE		MASK(1,5)
#define S2MU107_FLED_SOFT_ON_TIME	MASK(3,1)
#define S2MU107_FLED_REG_RESET		MASK(1,0)

#define S2MU107_CHX_FLASH_FLED_EN	MASK(2,4)
#define S2MU107_CHX_TORCH_FLED_EN	MASK(2,0)

#define S2MU107_FLED_ADAPTIVE_MODE_EN	MASK(1,5)
#define S2MU107_SET_BOOST_VOUT_FLASH	MASK(5,0)

#define S2MU107_READ_BOOST_VOUT_FLASH	MASK(5,0)
#define S2MU107_F2C_LC_IBAT		MASK(6,0)
#define S2MU107_F2C_SYS_MIN_REG		MASK(3,0)

/* Mask for channel control register */
#define S2MU107_FLED_OFF		0x00
#define S2MU107_FLASH_EN_HIGH		0x01
#define S2MU107_TORCH_EN_HIGH		0x02
#define S2MU107_FLED_ON			0x03
#define S2MU107_FLASH_EN_SHIFT		4

/* FLED Macro */
#define S2MU107_FLASH_ENABLE		1
#define S2MU107_FLASH_DISABLE		0
#define S2MU107_TORCH_ENABLE		1
#define S2MU107_TORCH_DISABLE		0
#define S2MU107_FLASH_BOOST_ON		1
#define S2MU107_FLASH_BOOST_OFF		0

/* FLED operating mode enable */
enum operating_mode {
	AUTO_MODE = 0,
	BOOST_MODE,
	TA_MODE,
	SYS_MODE,
};

enum cam_flash_mode{
	CAM_FLASH_MODE_NONE=0,		//CAM2_FLASH_MODE_NONE=0,
	CAM_FLASH_MODE_OFF,		//CAM2_FLASH_MODE_OFF,
	CAM_FLASH_MODE_SINGLE,		//CAM2_FLASH_MODE_SINGLE,
	CAM_FLASH_MODE_TORCH,		//CAM2_FLASH_MODE_TORCH,
};

enum s2mu107_fled_mode {
	S2MU107_FLED_MODE_OFF,
	S2MU107_FLED_MODE_TORCH,
	S2MU107_FLED_MODE_FLASH,
	S2MU107_FLED_MODE_MAX,
};

struct s2mu107_fled_chan {
	int id;
	u32 curr;
	u32 timer;
	u8 mode;
};

struct s2mu107_fled_platform_data {
	struct s2mu107_fled_chan *channel;
	int chan_num;
	int flash_gpio;
	int torch_gpio;
	unsigned int flash_current;
	unsigned int torch_current;
	unsigned int movie_current;
	unsigned int factory_current;
	unsigned int flashlight_current[S2MU107_FLASH_LIGHT_MAX];
	char *sc_name;
};

struct s2mu107_fled_data {
	struct s2mu107_fled_platform_data *pdata;
	struct s2mu107_fled_chan channel[S2MU107_CH_MAX];
	struct led_classdev cdev;
	struct device *dev;
	struct device *flash_dev;

	int flash_en;
	int torch_en;
	int attach_ta;
	int flash_gpio;
	int torch_gpio;
	int sysfs_input_data;
	int control_mode; /* 0 : I2C, 1 : GPIO */
	struct notifier_block batt_nb;
	struct i2c_client *i2c;
	struct i2c_client *chg;
	struct mutex lock;
	unsigned int flash_current;
	unsigned int torch_current;

	struct power_supply *sc_psy;
};

int s2mu107_fled_set_mode_ctrl(int chan, enum cam_flash_mode cam_mode);
int s2mu107_fled_set_curr(int chan, enum cam_flash_mode cam_mode, int curr);
int s2mu107_fled_get_curr(int chan, enum cam_flash_mode cam_mode);
int s2mu107_led_mode_ctrl(int state);
extern int s2mu107_fled_show_flash_en(void);
#endif
