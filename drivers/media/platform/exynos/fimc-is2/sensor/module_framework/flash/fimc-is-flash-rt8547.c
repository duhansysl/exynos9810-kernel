/*
 * Samsung Exynos5 SoC series Flash driver for RT8574
 *
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>

#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>

#include "fimc-is-flash.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-sensor-peri.h"
#include "fimc-is-core.h"

#include <linux/leds-rt8547.h>
extern int rt8547_led_mode_ctrl(int state);
#if defined(CONFIG_FLASH_CURRENT_CHANGE_SUPPORT)
extern int rt8547_set_flash_current(int intensity);
#endif

static int flash_rt8547_init(struct v4l2_subdev *subdev, u32 val)
{
	int ret = 0;
	struct fimc_is_flash *flash;

	BUG_ON(!subdev);

	flash = (struct fimc_is_flash *)v4l2_get_subdevdata(subdev);

	BUG_ON(!flash);

	/* TODO: init flash driver */
	flash->flash_data.mode = CAM2_FLASH_MODE_OFF;
	flash->flash_data.intensity = 100; /* TODO: Need to figure out min/max range */
	flash->flash_data.firing_time_us = 1 * 1000 * 1000; /* Max firing time is 1sec */
	flash->flash_data.flash_fired = false;

	gpio_request_one(flash->flash_gpio, GPIOF_OUT_INIT_LOW, "CAM_FLASH_GPIO_OUTPUT");
	gpio_free(flash->flash_gpio);

	return ret;
}

static int sensor_rt8547_flash_control(struct v4l2_subdev *subdev, enum flash_mode mode, u32 intensity)
{
	int ret = 0;
	int flash_en = 0;
	struct fimc_is_flash *flash = NULL;

	BUG_ON(!subdev);

	flash = (struct fimc_is_flash *)v4l2_get_subdevdata(subdev);
	BUG_ON(!flash);

	flash_en = flash->flash_gpio;
	if (flash_en < 0) {
		ret = -EINVAL;
		goto p_err;
	}

	dbg_flash("%s : mode = %s, intensity = %d\n", __func__,
		mode == CAM2_FLASH_MODE_OFF ? "OFF" :
		mode == CAM2_FLASH_MODE_SINGLE ? "FLASH" : "TORCH",
		intensity);

	if (mode == CAM2_FLASH_MODE_OFF) {
		ret = rt8547_led_mode_ctrl(RT8547_DISABLES_MOVIE_FLASH_MODE);
		ret = control_flash_gpio(flash_en, 0);
		if (ret)
			err("capture flash off fail");
	} else if (mode == CAM2_FLASH_MODE_SINGLE) {
		ret = rt8547_led_mode_ctrl(RT8547_ENABLE_FLASH_MODE);
#if defined(CONFIG_FLASH_CURRENT_CHANGE_SUPPORT)
		ret = rt8547_set_flash_current(intensity);
#endif
		ret = control_flash_gpio(flash_en, 1);
		if (ret)
			err("capture flash on fail");
	} else if (mode == CAM2_FLASH_MODE_TORCH) {
		ret = rt8547_led_mode_ctrl(RT8547_ENABLE_PRE_FLASH_MODE);
		ret = control_flash_gpio(flash_en, 1);
		if (ret)
			err("torch flash on fail");
	} else {
		err("Invalid flash mode");
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int flash_rt8547_s_ctrl(struct v4l2_subdev *subdev, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct fimc_is_flash *flash = NULL;

	BUG_ON(!subdev);

	flash = (struct fimc_is_flash *)v4l2_get_subdevdata(subdev);
	BUG_ON(!flash);

	switch(ctrl->id) {
	case V4L2_CID_FLASH_SET_INTENSITY:
		/* TODO : Check min/max intensity */
		if (ctrl->value < 0) {
			err("failed to flash set intensity: %d\n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		flash->flash_data.intensity = ctrl->value;
		break;
	case V4L2_CID_FLASH_SET_FIRING_TIME:
		/* TODO : Check min/max firing time */
		if (ctrl->value < 0) {
			err("failed to flash set firing time: %d\n", ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		flash->flash_data.firing_time_us = ctrl->value;
		break;
	case V4L2_CID_FLASH_SET_FIRE:
		ret =  sensor_rt8547_flash_control(subdev, flash->flash_data.mode, ctrl->value);
		if (ret) {
			err("sensor_rt8547_flash_control(mode:%d, val:%d) is fail(%d)",
					(int)flash->flash_data.mode, ctrl->value, ret);
			goto p_err;
		}
		break;
	default:
		err("err!!! Unknown CID(%#x)", ctrl->id);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = flash_rt8547_init,
	.s_ctrl = flash_rt8547_s_ctrl,
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
};

int flash_rt8547_probe(struct device *dev, struct i2c_client *client)
{
	int ret = 0, i = 0;
	struct fimc_is_core *core;
	struct v4l2_subdev *subdev_flash = NULL;
	struct fimc_is_device_sensor *device;
	u32 sensor_id[FIMC_IS_SENSOR_COUNT];
	u32 sensor_id_len;
	const u32 *sensor_id_spec;
	struct fimc_is_flash *flash = NULL;
	struct device_node *dnode;

	BUG_ON(!fimc_is_dev);
	BUG_ON(!dev);

	dnode = dev->of_node;

	core = (struct fimc_is_core *)dev_get_drvdata(fimc_is_dev);
	if (!core) {
		probe_info("core device is not yet probed");
		ret = -EPROBE_DEFER;
		goto p_err;
	}

	sensor_id_spec = of_get_property(dnode, "id", &sensor_id_len);
	if (!sensor_id_spec) {
		err("sensor_id num read is fail(%d)", ret);
		goto p_err;
	}

	sensor_id_len /= (unsigned int)sizeof(*sensor_id_spec);

	ret = of_property_read_u32_array(dnode, "id", sensor_id, sensor_id_len);
	if (ret) {
		err("sensor_id read is fail(%d)", ret);
		goto p_err;
	}

	for (i = 0; i < sensor_id_len; i++) {
		device = &core->sensor[sensor_id[i]];
		if (!device) {
			err("sensor device is NULL");
			ret = -EPROBE_DEFER;
			goto p_err;
		}
	}

	flash = kzalloc(sizeof(struct fimc_is_flash) * sensor_id_len, GFP_KERNEL);
	if (!flash) {
		err("flash is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	subdev_flash = kzalloc(sizeof(struct v4l2_subdev) * sensor_id_len, GFP_KERNEL);
	if (!subdev_flash) {
		err("subdev_flash is NULL");
		ret = -ENOMEM;
		goto p_err;
	}

	for (i = 0; i < sensor_id_len; i++) {
		flash[i].flash_gpio = of_get_named_gpio(dnode, "flash-gpio", 0);
		if (!gpio_is_valid(flash[i].flash_gpio)) {
			dev_err(dev, "failed to get enable gpio\n");
			kfree(flash);
			kfree(subdev_flash);
			return -EINVAL;
		}

		flash[i].torch_gpio = flash[i].flash_gpio;
		flash[i].id = FLADRV_NAME_RT8547;
		flash[i].subdev = &subdev_flash[i];
		flash[i].client = client;
		flash[i].device = sensor_id[i];

		device = &core->sensor[sensor_id[i]];
		device->subdev_flash = &subdev_flash[i];
		device->flash = &flash[i];

		v4l2_subdev_init(&subdev_flash[i], &subdev_ops);
		v4l2_set_subdevdata(&subdev_flash[i], &flash[i]);
		v4l2_set_subdev_hostdata(&subdev_flash[i], device);
		snprintf(subdev_flash[i].name, V4L2_SUBDEV_NAME_SIZE,
				"flash-subdev.%d", flash[i].id);
	}

	probe_info("%s done\n", __func__);

	return 0;

p_err:
	if (flash)
		kfree(flash);
	if (subdev_flash)
		kfree(subdev_flash);

	return ret;
}

int flash_rt8547_platform_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;

	BUG_ON(!pdev);

	dev = &pdev->dev;

	ret = flash_rt8547_probe(dev, NULL);
	if (ret < 0) {
		probe_err("flash rt8547 probe fail(%d)\n", ret);
		goto p_err;
	}

	probe_info("%s done\n", __func__);

p_err:
	return ret;
}

static int flash_rt8547_platform_remove(struct platform_device *pdev)
{
	int ret = 0;

	info("%s\n", __func__);

	return ret;
}

static const struct of_device_id exynos_fimc_is_sensor_flash_rt8547_match[] = {
	{
		.compatible = "samsung,sensor-flash-rt8547",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_fimc_is_sensor_flash_rt8547_match);

/* register platform driver */
static struct platform_driver sensor_flash_rt8547_platform_driver = {
	.probe  = flash_rt8547_platform_probe,
	.remove = flash_rt8547_platform_remove,
	.driver = {
		.name   = "FIMC-IS-SENSOR-FLASH-RT8547-PLATFORM",
		.owner  = THIS_MODULE,
		.of_match_table = exynos_fimc_is_sensor_flash_rt8547_match,
	}
};
module_platform_driver(sensor_flash_rt8547_platform_driver);
