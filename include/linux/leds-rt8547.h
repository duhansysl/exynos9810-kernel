/*
 * leds-ir-rt8547.h - Flash-led driver for RT8547
 *
 * Copyright (C) 2011 Samsung Electronics
 * Sunggeun Yim <sunggeun.yim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LEDS_RT8547_H__
#define __LEDS_RT8547_H__

#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/delay.h>

#define rt8547_NAME "leds-rt8547"

#define RT8547_FLASH_CURRENT(mA) (((mA)-100)/50)
#define RT8547_MOVIE_CURRENT(mA) (((mA)-25)/25)
#define RT8547_MOVIE_REG(val) (((val)*25)+25)

#define RT8547_INTENSITY_MIN 100
#define RT8547_INTENSITY_MAX 1600

#define LED_ERROR(x, ...) printk(KERN_ERR "%s : " x, __func__, ##__VA_ARGS__)
#define LED_INFO(x, ...) printk(KERN_INFO "%s : " x, __func__, ##__VA_ARGS__)
#define LED_CHECK_ERR_GOTO(x, out, fmt, ...) \
	if (unlikely((x) < 0)) { \
		printk(KERN_ERR fmt, ##__VA_ARGS__); \
		goto out; \
	}

#define DT_READ_U32(node, key, value) do {\
			pprop = key; \
			temp = 0; \
			if (of_property_read_u32((node), key, &temp)) \
				pr_warn("%s: no property in the node.\n", pprop);\
			(value) = temp; \
		} while (0)

#define RT8547_ADDR_LVP_SETTING	0x1
#define RT8547_ADDR_FLASH_CURRENT_LEVEL_TIMEOUT_SETTING	0x2
#define RT8547_ADDR_CURRENT_SETTING	0x3
#define RT8547_ADDR_FLASH_TIMEOUT_SETTING	0x4
#define RT8547_ADDR_HIDDEN_SETTING 0x7

#define RT8547_SLAVE_ADDR 0x99

#define RT8547_SW_RESET 0x20
#define RT8547_TORCH_SELECT 0x10
#define RT8547_STROBE_SELECT 0x00

#define RT8547_HIDDEN_DEFAULT 0x67
#define RT8547_HIDDEN_LVP_DISABLE 0x27

#define T_SHORT		4			/* us */
#define T_LONG		60			/* us*/
#define T_SOD		10			/* us */
#define T_EOD		10			/* us */

/* LVP_SETTING */
enum rt8547_LVPsetting_t {
	RT8547_3V = 0x0,
	RT8547_3_1V,
	RT8547_3_2V,
	RT8547_3_3V,
	RT8547_3_4V,
	RT8547_3_5V,
	RT8547_3_6V,
	RT8547_3_7V,
	RT8547_3_8V,
	RT8547_MAX = 0xf,
};
/* FLASH_TIMEOUT_SETTING */
enum rt8547_timer_t {
	RT8547_TIMER_64ms = 0x00,
	RT8547_TIMER_96ms,

	RT8547_TIMER_512ms = 0x0e,
	RT8547_TIMER_544ms,

	RT8547_TIMER_704ms = 0x14,

	RT8547_TIMER_992ms = 0x1d,
	RT8547_TIMER_1024ms,

	RT8547_TIMER_1088ms = 0x20,
	RT8547_TIMER_1120ms,
	RT8547_TIMER_1152ms,
	RT8547_TIMER_1184ms,
	RT8547_TIMER_1216ms,

	RT8547_TIMER_MAX = 0x3f,
};
/* MIN. CURRENT SETTING FOR TIMER OPERATING */
enum rt8547_timeout_current_t {
	RT8547_TIMEOUT_CURRENT_100mA = 0x0,
	RT8547_TIMEOUT_CURRENT_150mA,
	RT8547_TIMEOUT_CURRENT_200mA,
	RT8547_TIMEOUT_CURRENT_250mA,
	RT8547_TIMEOUT_CURRENT_300mA,
	RT8547_TIMEOUT_CURRENT_350mA,
	RT8547_TIMEOUT_CURRENT_400mA = 0x7,
};
/* MOVIE CURRENT SETTING */
enum rt8547_movie_current_t {
	RT8547_MOVIE_CURRENT_25mA = 0x0,
	RT8547_MOVIE_CURRENT_50mA,
	RT8547_MOVIE_CURRENT_75mA,
	RT8547_MOVIE_CURRENT_100mA,
	RT8547_MOVIE_CURRENT_125mA,
	RT8547_MOVIE_CURRENT_150mA,
	RT8547_MOVIE_CURRENT_175mA,
	RT8547_MOVIE_CURRENT_200mA,
	RT8547_MOVIE_CURRENT_225mA,
	RT8547_MOVIE_CURRENT_250mA,
	RT8547_MOVIE_CURRENT_275mA,
	RT8547_MOVIE_CURRENT_300mA,
	RT8547_MOVIE_CURRENT_325mA,
	RT8547_MOVIE_CURRENT_350mA,
	RT8547_MOVIE_CURRENT_375mA,
	RT8547_MOVIE_CURRENT_400mA,
};
/* FLASH CURRENT SETTING */
enum rt8547_flash_current_t {
	RT8547_FLASH_CURRENT_100mA = 0x00,
	RT8547_FLASH_CURRENT_150mA,
	RT8547_FLASH_CURRENT_200mA,
	RT8547_FLASH_CURRENT_250mA,

	RT8547_FLASH_CURRENT_900mA = 0x10,
	RT8547_FLASH_CURRENT_950mA,
	RT8547_FLASH_CURRENT_1000mA,

	RT8547_FLASH_CURRENT_1200mA = 0x16,
	RT8547_FLASH_CURRENT_1250mA,
	RT8547_FLASH_CURRENT_1300mA,
	RT8547_FLASH_CURRENT_1350mA,
	RT8547_FLASH_CURRENT_1400mA,
	RT8547_FLASH_CURRENT_1450mA,
	RT8547_FLASH_CURRENT_1500mA,
	RT8547_FLASH_CURRENT_1550mA,
	RT8547_FLASH_CURRENT_1600mA,
	RT8547_FLASH_CURRENT_MAX = 0x1f,
};
/* MOVIE/FLASH MODE CONTROL */
enum rt8547_mode_control_t {
	RT8547_DISABLES_MOVIE_FLASH_MODE = 0x0,
	RT8547_ENABLE_MOVIE_MODE,
	RT8547_ENABLE_FLASH_MODE,
	RT8547_ENABLE_PRE_FLASH_MODE,
};

struct rt8547_platform_data {
	spinlock_t int_lock;
	int sysfs_input_data;
	int flash_control;
	int flash_en;
	int flash_movie_en;
	struct workqueue_struct *wqueue;
	enum rt8547_LVPsetting_t LVP_Voltage;
	enum rt8547_timer_t flash_timeout;
	enum rt8547_timeout_current_t timeout_current_value;
	enum rt8547_flash_current_t flash_current_value;
	enum rt8547_movie_current_t movie_current_value;
	enum rt8547_movie_current_t factory_current_value;
	enum rt8547_movie_current_t pre_current_value;
	enum rt8547_movie_current_t flashlight_current_value[5];
	enum rt8547_mode_control_t mode_status;
};

extern int32_t rt8547_led_mode_ctrl(int state);
extern int32_t rt8547_led_set_torch(int curr);
extern int32_t rt8547_set_flash_current(int intensity);

#endif
