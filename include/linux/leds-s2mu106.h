/*
 * leds-s2mu106.h - Flash-led driver for Samsung S2MU106
 *
 * Copyright (C) 2018 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#ifndef __LEDS_S2MU106_FLASH_H__
#define __LEDS_S2MU106_FLASH_H__
#include <linux/leds.h>

#define MASK(width, shift)	(((0x1 << (width)) - 1) << shift)

#define FLED_EN 0

#define S2MU106_CH_MAX 3
#define S2MU106_FLASH_LIGHT_MAX 5

/* Interrupt register */
#define S2MU106_FLED_INT1	0x04
#define S2MU106_FLED_INT2	0x05

#define S2MU106_FLED_INT1_MASK	0x0B
#define S2MU106_FLED_INT2_MASK	0x0C

/* Status register */
#define S2MU106_FLED_STATUS1	0x14
#define S2MU106_FLED_STATUS2	0x15

/* Mask for status register */
#define S2MU106_CH1_FLASH_ON	MASK(1,7)
#define S2MU106_CH1_TORCH_ON	MASK(1,6)
#define S2MU106_CH2_FLASH_ON	MASK(1,5)
#define S2MU106_CH2_TORCH_ON	MASK(1,4)
#define S2MU106_CH3_FLASH_ON	MASK(1,3)
#define S2MU106_CH3_TORCH_ON	MASK(1,2)
#define S2MU106_FLED_ON_CHECK	MASK(6,2)

/* Channel control */
#define S2MU106_FLED_CH1_CTRL0	0x53
#define S2MU106_FLED_CH1_CTRL1	0x54
#define S2MU106_FLED_CH1_CTRL2	0x55

#define S2MU106_FLED_CH2_CTRL0	0x56
#define S2MU106_FLED_CH2_CTRL1	0x57
#define S2MU106_FLED_CH2_CTRL2	0x58

#define S2MU106_FLED_CH3_CTRL0	0x59
#define S2MU106_FLED_CH3_CTRL1	0x5A
#define S2MU106_FLED_CH3_CTRL2	0x5B

/* Mask for channel control register */
#define S2MU106_CHX_OPEN_PROT_EN	MASK(1,7)
#define S2MU106_CHX_SHORT_PROT_EN	MASK(1,6)
#define S2MU106_CHX_FLASH_IOUT	MASK(5,0)
#define S2MU106_CHX_TORCH_TMR_MODE	MASK(1,7)
#define S2MU106_CHX_DIS_TORCH_TMR	MASK(1,6)
#define S2MU106_CHX_FLASH_TMR_MODE	MASK(1,5)
#define S2MU106_CHX_TORCH_IOUT	MASK(5,0)
#define S2MU106_CHX_FLASH_TMR_DUR	MASK(4,4)
#define S2MU106_CHX_TORCH_TMR_DUR	MASK(4,0)

/* Mode control */
#define S2MU106_FLED_CTRL0	0x5C
#define S2MU106_FLED_CTRL1	0x5D
#define S2MU106_FLED_CTRL2	0x5E
#define S2MU106_FLED_CTRL3	0x5F
#define S2MU106_FLED_CTRL4	0x60
#define S2MU106_FLED_CTRL5	0x61
#define S2MU106_FLED_CTRL6	0x62
#define S2MU106_FLED_CTRL7	0x63
#define S2MU106_FLED_TEST3	0x66
#define S2MU106_FLED_TEST4	0x67

/* Mask for channel control register */
#define S2MU106_CHX_FLASH_FLED_EN	MASK(3,3)
#define S2MU106_CHX_TORCH_FLED_EN	MASK(3,0)
#define S2MU106_EN_FLED_PRE	MASK(1,5)
#define S2MU106_FLED_EN	0x5
#define S2MU106_FLED_GPIO_EN1		0x01
#define S2MU106_FLED_GPIO_EN2		0x02
#define S2MU106_FLED_GPIO_EN3		0x03
#define S2MU106_FLED_GPIO_EN4		0x04

/* Mask for Mode control register */
#define S2MU106_FLED_MODE	MASK(2,6)
#define S2MU106_EN_FLED_PRE	MASK(1,5)
#define S2MU106_FLED_SOFT_ON_TIME	MASK(3,1)
#define S2MU106_FLED_REG_RESET	MASK(1,0)
#define S2MU106_CHX_FLASH_FLED_EN	MASK(3,3)
#define S2MU106_CHX_TORCH_FLED_EN	MASK(3,0)
#define S2MU106_FLED_ADAPTIVE_MODE_EN	MASK(1,5)
#define S2MU106_SET_BOOST_VOUT_FLASH	MASK(5,0)
#define S2MU106_READ_BOOST_VOUT_FLASH	MASK(5,0)
#define S2MU106_F2C_LC_IBAT	MASK(6,0)
#define S2MU106_F2C_SYS_MIN_REG	MASK(3,0)

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

enum s2mu106_fled_mode {
	S2MU106_FLED_MODE_OFF,
	S2MU106_FLED_MODE_TORCH,
	S2MU106_FLED_MODE_FLASH,
	S2MU106_FLED_MODE_MOVIE,
	S2MU106_FLED_MODE_FACTORY,
	S2MU106_FLED_MODE_MAX,
};

struct s2mu106_fled_chan {
	int id;
	u32 curr;
	u32 timer;
	u8 mode;
};

struct s2mu106_fled_platform_data {
	struct s2mu106_fled_chan *channel;
	int chan_num;
	int flash_gpio;
	int torch_gpio;
	u32 default_current;
	u32 max_current;
	u8 default_mode;
	u32 default_timer;
	unsigned int flash_current;
	unsigned int torch_current;
	unsigned int preflash_current;
	unsigned int movie_current;
	unsigned int factory_current;
	unsigned int flashlight_current[S2MU106_FLASH_LIGHT_MAX];
};

struct s2mu106_fled_data {
	struct s2mu106_fled_platform_data *pdata;
	struct s2mu106_fled_chan channel[S2MU106_CH_MAX];
	struct led_classdev cdev;
	struct device *dev;
	struct device *flash_dev;

	int set_on_factory;
	int flash_gpio;
	int torch_gpio;
	int sysfs_input_data;
	int control_mode; /* 0 : I2C, 1 : GPIO */

	/* charger mode control */
	bool is_en_flash;
	struct power_supply *psy_chg;

	struct i2c_client *i2c;
	struct mutex lock;
	u32 default_current;
	unsigned int flash_current;
	unsigned int torch_current;
	unsigned int preflash_current;
	unsigned int movie_current;
	unsigned int factory_current;
	unsigned int flashlight_current[S2MU106_FLASH_LIGHT_MAX];
};

int s2mu106_fled_set_mode_ctrl(int chan, enum cam_flash_mode cam_mode);
int s2mu106_fled_set_curr(int chan, enum cam_flash_mode cam_mode, int curr);
int s2mu106_fled_get_curr(int chan, enum cam_flash_mode cam_mode);
int s2mu106_led_mode_ctrl(int state);
extern void s2mu106_fled_set_operation_mode(int val);
#endif
