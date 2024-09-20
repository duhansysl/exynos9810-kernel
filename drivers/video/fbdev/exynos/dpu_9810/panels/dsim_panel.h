/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DSIM_PANEL__
#define __DSIM_PANEL__


extern unsigned int lcdtype;

extern struct dsim_lcd_driver *mipi_lcd_driver;

extern struct dsim_lcd_driver *__start___lcd_driver;
extern struct dsim_lcd_driver *__stop___lcd_driver;

#define __XX_ADD_LCD_DRIVER(name)		\
struct dsim_lcd_driver *p_##name __attribute__((used, section("__lcd_driver"))) = &name;	\
static struct dsim_lcd_driver __maybe_unused *this_driver = &name	\

struct lcd_device;
struct lcd_ops;
struct backlight_device;
struct backlight_ops;
struct backlight_properties;
extern struct lcd_device *exynos_lcd_device_register(const char *name,
	struct device *parent, void *devdata, struct lcd_ops *ops);
extern struct backlight_device *exynos_backlight_device_register(const char *name,
	struct device *dev, void *devdata, const struct backlight_ops *ops,
	const struct backlight_properties *props);

#endif

