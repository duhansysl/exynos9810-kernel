/*
 * LED driver - leds-rt8547.c
 *
 * Copyright (C) 2016 Younghoon Joo <yhoon.joo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pwm.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/leds-rt8547.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#endif

extern struct class *camera_class; /*sys/class/camera*/
struct device *rt8547_dev;

struct rt8547_platform_data *global_rt8547data;
struct device *global_dev;

bool assistive_light = false;
bool lvp_enabled = true;

void rt8547_setGpio(int onoff)
{
	if (onoff) {
		gpio_direction_output(global_rt8547data->flash_control, 1);
		gpio_direction_output(global_rt8547data->flash_en, 1);
	} else {
		gpio_direction_output(global_rt8547data->flash_control, 0);
		gpio_direction_output(global_rt8547data->flash_en, 0);
	}
}

void rt8547_set_low_bit(void)
{
	gpio_direction_output(global_rt8547data->flash_control, 0);
	udelay(T_LONG);
	gpio_direction_output(global_rt8547data->flash_control, 1);
	udelay(T_SHORT);
}

void rt8547_set_high_bit(void)
{
	gpio_direction_output(global_rt8547data->flash_control, 0);
	udelay(T_SHORT);
	gpio_direction_output(global_rt8547data->flash_control, 1);
	udelay(T_LONG);
}

static int rt8547_set_bit(unsigned int bit)
{
	if (bit) {
		rt8547_set_high_bit();
	} else {
		rt8547_set_low_bit();
	}

	return 0;
}

static int rt8547_write_one_wire_data(unsigned int data, unsigned int len)
{
	int i = 0;

	for ( i = len-1; i >= 0; i--) {
		rt8547_set_bit((data >> i) & 0x01);
	}

	return 0;
}

static int rt8547_write_data(unsigned int regAddr, unsigned int data)
{
	int err = 0;

	gpio_direction_output(global_rt8547data->flash_control, 1);
	udelay(T_SOD);

	/* Write Slave Address */
	rt8547_write_one_wire_data(RT8547_SLAVE_ADDR, 8);

	/* Write Reg Address */
	rt8547_write_one_wire_data(regAddr, 3);

	/* Write Data */
	rt8547_write_one_wire_data(data, 8);

	/* Data End Condition */
	gpio_direction_output(global_rt8547data->flash_control, 0);
	udelay(T_SHORT);
	gpio_direction_output(global_rt8547data->flash_control, 1);
	udelay(T_EOD);

	return err;
}

ssize_t rt8547_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	int value = 0;
	int ret = 0;
	int torch_step = 0;
	unsigned long flags = 0;

	if ((buf == NULL) || kstrtouint(buf, 10, &value)) {
		return -1;
	}

	global_rt8547data->sysfs_input_data = value;

	ret = gpio_request(global_rt8547data->flash_control, "rt8547_led_control");
	if (ret) {
		LED_ERROR("Failed to requeset rt8547_led_control\n");
		return ret;
	}

	ret = gpio_request(global_rt8547data->flash_en, "rt8547_led_en");
	if (ret) {
		LED_ERROR("Failed to requeset rt8547_led_en\n");
		return ret;
	}

	if (value <= 0) {
		LED_INFO("RT8547-TORCH OFF. : E(%d)\n", value);
		assistive_light = false;
		global_rt8547data->mode_status = RT8547_DISABLES_MOVIE_FLASH_MODE;

		spin_lock_irqsave(&global_rt8547data->int_lock, flags);
		rt8547_setGpio(0);
		spin_unlock_irqrestore(&global_rt8547data->int_lock, flags);

		LED_INFO("RT8547-TORCH OFF. : X(%d)\n", value);
	} else {
		LED_INFO("RT8547-TORCH ON. : E(%d)\n", value);
		assistive_light = true;
		global_rt8547data->mode_status = RT8547_ENABLE_MOVIE_MODE;
		spin_lock_irqsave(&global_rt8547data->int_lock, flags);
		if (lvp_enabled)
			rt8547_write_data(RT8547_ADDR_LVP_SETTING, RT8547_3V);
		if (value == 100) {
			rt8547_write_data(RT8547_ADDR_CURRENT_SETTING,
				global_rt8547data->factory_current_value | RT8547_TORCH_SELECT);
		} else if ((1001 <= value) && (value <= 1010)) {
			if (value <= 1001)
				torch_step = global_rt8547data->flashlight_current_value[0];
			else if (value <= 1002)
				torch_step = global_rt8547data->flashlight_current_value[1];
			else if (value <= 1004)
				torch_step = global_rt8547data->flashlight_current_value[2];
			else if (value <= 1006)
				torch_step = global_rt8547data->flashlight_current_value[3];
			else if (value <= 1009)
				torch_step = global_rt8547data->flashlight_current_value[4];
			else
				torch_step = global_rt8547data->flashlight_current_value[0];

			rt8547_write_data(RT8547_ADDR_CURRENT_SETTING, torch_step | RT8547_TORCH_SELECT);
			LED_INFO("torch current enum(%d) : %d mA\n", torch_step, RT8547_MOVIE_REG(torch_step));
		} else {
			rt8547_write_data(RT8547_ADDR_CURRENT_SETTING,
								global_rt8547data->movie_current_value | RT8547_TORCH_SELECT);
			LED_INFO("global_rt8547data->movie_current_value(%d)\n", global_rt8547data->movie_current_value);
		}
		rt8547_write_data(RT8547_ADDR_FLASH_CURRENT_LEVEL_TIMEOUT_SETTING,
							((global_rt8547data->timeout_current_value << 5) & 0xE0));

		gpio_direction_output(global_rt8547data->flash_en, 1);
		spin_unlock_irqrestore(&global_rt8547data->int_lock, flags);

		LED_INFO("RT8547-TORCH ON. : X(%d)\n", value);
	}

	gpio_free(global_rt8547data->flash_control);
	gpio_free(global_rt8547data->flash_en);

	return count;
}

EXPORT_SYMBOL(rt8547_led_mode_ctrl);

int32_t rt8547_led_mode_ctrl(int state)
{
	int ret = 0;
	unsigned long flags = 0;

	if (assistive_light == true) {
		LED_INFO(" assistive_light is enabled \n");
		return 0;
	}

	ret = gpio_request(global_rt8547data->flash_control, "rt8547_led_control");
	if (ret) {
		LED_ERROR("Failed to request rt8547_led_mode_ctrl\n");
		return ret;
	}

	ret = gpio_request(global_rt8547data->flash_en, "rt8547_led_en");
	if (ret) {
		LED_ERROR("Failed to requeset rt8547_led_en\n");
		return ret;
	}

	switch(state) {
		case RT8547_ENABLE_PRE_FLASH_MODE:
			/* FlashLight Mode Pre Flash */
			LED_INFO("RT8547-Pre Flash ON E(%d)-%d\n", state,global_rt8547data->flash_movie_en);
			global_rt8547data->mode_status = RT8547_ENABLE_PRE_FLASH_MODE;
			spin_lock_irqsave(&global_rt8547data->int_lock, flags);
			if (lvp_enabled) {
				rt8547_write_data(RT8547_ADDR_LVP_SETTING, global_rt8547data->LVP_Voltage);
			}
			if (global_rt8547data->flash_movie_en) {
				rt8547_write_data(RT8547_ADDR_CURRENT_SETTING,
									global_rt8547data->movie_current_value|RT8547_TORCH_SELECT);
			} else {
				rt8547_write_data(RT8547_ADDR_CURRENT_SETTING,
									global_rt8547data->pre_current_value|RT8547_TORCH_SELECT);
			}
			rt8547_write_data(RT8547_ADDR_FLASH_CURRENT_LEVEL_TIMEOUT_SETTING,
								((global_rt8547data->timeout_current_value << 5) & 0xE0));
			spin_unlock_irqrestore(&global_rt8547data->int_lock, flags);

			LED_INFO("RT8547-Pre Flash ON X(%d)\n", state);
			break;
		case RT8547_ENABLE_MOVIE_MODE:
			/* FlashLight Mode TORCH */
			LED_INFO("RT8547-TORCH ON E(%d)\n", state);
			global_rt8547data->mode_status = RT8547_ENABLE_MOVIE_MODE;
			spin_lock_irqsave(&global_rt8547data->int_lock, flags);
			if (lvp_enabled) {
				rt8547_write_data(RT8547_ADDR_LVP_SETTING, global_rt8547data->LVP_Voltage);
			}
			rt8547_write_data(RT8547_ADDR_CURRENT_SETTING,
								global_rt8547data->movie_current_value|RT8547_TORCH_SELECT);
			rt8547_write_data(RT8547_ADDR_FLASH_CURRENT_LEVEL_TIMEOUT_SETTING,
								((global_rt8547data->timeout_current_value << 5) & 0xE0));
			spin_unlock_irqrestore(&global_rt8547data->int_lock, flags);
			LED_INFO("RT8547-TORCH ON X(%d)\n", state);
			break;
		case RT8547_ENABLE_FLASH_MODE:
			/* FlashLight Mode Flash */
			LED_INFO("RT8547-FLASH ON E(%d)\n", state);
			global_rt8547data->mode_status = RT8547_ENABLE_FLASH_MODE;
			spin_lock_irqsave(&global_rt8547data->int_lock, flags);
			if (lvp_enabled) {
				rt8547_write_data(RT8547_ADDR_LVP_SETTING, global_rt8547data->LVP_Voltage); // LVP setting
			}
			rt8547_write_data(RT8547_ADDR_CURRENT_SETTING, RT8547_STROBE_SELECT); // Strobe select
			rt8547_write_data(RT8547_ADDR_FLASH_CURRENT_LEVEL_TIMEOUT_SETTING,
						((global_rt8547data->timeout_current_value << 5) & 0xE0) | global_rt8547data->flash_current_value);
			spin_unlock_irqrestore(&global_rt8547data->int_lock, flags);
			LED_INFO("RT8547-FLASH ON X(%d)\n", state);
			break;
		case RT8547_DISABLES_MOVIE_FLASH_MODE:
		default:
			/* FlashLight Mode OFF */
			LED_INFO("RT8547-FLASH OFF E(%d)\n", state);
			global_rt8547data->mode_status = RT8547_DISABLES_MOVIE_FLASH_MODE;
			spin_lock_irqsave(&global_rt8547data->int_lock, flags);
			rt8547_setGpio(0);
			spin_unlock_irqrestore(&global_rt8547data->int_lock, flags);

			global_rt8547data->flash_movie_en = false;
			LED_INFO("RT8547-FLASH OFF X(%d)\n", state);
			break;
	}

	gpio_free(global_rt8547data->flash_control);
	gpio_free(global_rt8547data->flash_en);

	return ret;
}

void rt8547_set_movie_mode(bool on)
{
	LED_INFO("RT8547 MOVIE MODE");
	global_rt8547data->flash_movie_en = on;
}

int32_t rt8547_set_flash_current(int intensity)
{
	int ret = 0;
	unsigned long flags = 0;
	int flash_current;

	if (intensity <= 0) {
		return -EINVAL;
	}
	if (intensity < RT8547_INTENSITY_MIN)
		intensity = RT8547_INTENSITY_MIN;
	else if (intensity > RT8547_INTENSITY_MAX)
		intensity = RT8547_INTENSITY_MAX;

	flash_current = RT8547_FLASH_CURRENT(intensity);

	ret = gpio_request(global_rt8547data->flash_control, "rt8547_led_control");
	if (ret) {
		LED_ERROR("Failed to request rt8547_led_mode_ctrl\n");
		return ret;
	}

	LED_INFO("RT8547-FLASH CURRENT:%d(%d)", intensity, flash_current);
	spin_lock_irqsave(&global_rt8547data->int_lock, flags);
	rt8547_write_data(RT8547_ADDR_FLASH_CURRENT_LEVEL_TIMEOUT_SETTING,
		(((global_rt8547data->timeout_current_value << 5) & 0xE0) | (flash_current & 0x1f)));
	spin_unlock_irqrestore(&global_rt8547data->int_lock, flags);

	gpio_free(global_rt8547data->flash_control);

	return ret;
}

EXPORT_SYMBOL(rt8547_set_movie_mode);
EXPORT_SYMBOL(rt8547_set_flash_current);

int32_t rt8547_led_set_torch(int curr)
{
	int ret = 0;
	unsigned long flags = 0;

	if (curr == -1) {
		spin_lock_irqsave(&global_rt8547data->int_lock, flags);
		rt8547_setGpio(0);
		spin_unlock_irqrestore(&global_rt8547data->int_lock, flags);

		LED_INFO("RT8547-FLASH OFF X(%d)\n", curr);
		return 0;
	}

	ret = gpio_request(global_rt8547data->flash_control, "rt8547_led_control");
	if (ret) {
		LED_ERROR("Failed to request rt8547_led_mode_ctrl\n");
		return ret;
	}

	/* FlashLight Mode TORCH for flicker sensor */
	LED_INFO("RT8547-FLICKERTEST ON E(%d)\n", curr);

	spin_lock_irqsave(&global_rt8547data->int_lock, flags);
	if (lvp_enabled)
		rt8547_write_data(RT8547_ADDR_LVP_SETTING, global_rt8547data->LVP_Voltage);
	rt8547_write_data(RT8547_ADDR_CURRENT_SETTING, curr|RT8547_TORCH_SELECT);
	rt8547_write_data(RT8547_ADDR_FLASH_CURRENT_LEVEL_TIMEOUT_SETTING,
						((global_rt8547data->timeout_current_value << 5) & 0xE0));
	spin_unlock_irqrestore(&global_rt8547data->int_lock, flags);
	LED_INFO("RT8547-FLICKERTEST ON X(%d)\n", curr);

	gpio_free(global_rt8547data->flash_control);

	return ret;
}

ssize_t rt8547_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", global_rt8547data->sysfs_input_data);
}

static DEVICE_ATTR(rear_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
	rt8547_show, rt8547_store);
static DEVICE_ATTR(rear_torch_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
	rt8547_show, rt8547_store);

static int rt8547_parse_dt(struct device *dev,
		struct rt8547_platform_data *pdata)
{
	struct device_node *dnode = dev->of_node;
	u32 buffer = 0;
	u32 flashlight_current[5] = {0, };
	int ret = 0;

	/* Defulat Value */
	pdata->LVP_Voltage = RT8547_3V;
	pdata->flash_timeout = RT8547_TIMER_512ms;
	pdata->timeout_current_value = RT8547_TIMEOUT_CURRENT_400mA;
	pdata->flash_current_value = RT8547_FLASH_CURRENT_1500mA;
	pdata->movie_current_value = RT8547_MOVIE_CURRENT_200mA;
	pdata->pre_current_value = RT8547_MOVIE_CURRENT_275mA;
	pdata->factory_current_value = RT8547_MOVIE_CURRENT_400mA;
	pdata->mode_status = RT8547_DISABLES_MOVIE_FLASH_MODE;

	/* (value) 1001, 1002, 1004, 1006, 1009
		: (torch_step) 1(50mA), 2(75mA), 4(125mA), 9(200mA), 10(275mA) */
	pdata->flashlight_current_value[0] = RT8547_MOVIE_CURRENT_50mA;
	pdata->flashlight_current_value[1] = RT8547_MOVIE_CURRENT_75mA;
	pdata->flashlight_current_value[2] = RT8547_MOVIE_CURRENT_125mA;
	pdata->flashlight_current_value[3] = RT8547_MOVIE_CURRENT_200mA;
	pdata->flashlight_current_value[4] = RT8547_MOVIE_CURRENT_275mA;


	/* get gpio */
	pdata->flash_control = of_get_named_gpio(dnode, "flash_control", 0);
	if (!gpio_is_valid(pdata->flash_control)) {
		dev_err(dev, "failed to get flash_control\n");
		return -1;
	}

	pdata->flash_en = of_get_named_gpio(dnode, "flash_en", 0);
	if (!gpio_is_valid(pdata->flash_en)) {
		dev_err(dev, "failed to get flash_en\n");
		return -1;
	}

	/* get flash current value */
	if (of_property_read_u32(dnode, "flash_current", &buffer) == 0) {
		dev_info(dev, "flash_current = <%d><%d>\n", buffer,RT8547_FLASH_CURRENT(buffer)&0x1F);
		pdata->flash_current_value = RT8547_FLASH_CURRENT(buffer)&0x1F;
	}

	/* get movie current value */
	if (of_property_read_u32(dnode, "movie_current", &buffer) == 0) {
		dev_info(dev, "movie_current = <%d><%d>\n", buffer, RT8547_MOVIE_CURRENT(buffer)&0x0F);
		pdata->movie_current_value = RT8547_MOVIE_CURRENT(buffer)&0x0F;
	}

	/* get factory current value */
	if (of_property_read_u32(dnode, "factory_current", &buffer) == 0) {
		dev_info(dev, "factory_current = <%d><%d>\n", buffer, RT8547_MOVIE_CURRENT(buffer)&0x0F);
		pdata->factory_current_value = RT8547_MOVIE_CURRENT(buffer)&0x0F;
	}

	/* get pre current value */
	if (of_property_read_u32(dnode, "pre_current", &buffer) == 0) {
		dev_info(dev, "pre_current = <%d><%d>\n", buffer, RT8547_MOVIE_CURRENT(buffer)&0x0F);
		pdata->pre_current_value = RT8547_MOVIE_CURRENT(buffer)&0x0F;
	}

	if (of_property_read_u32_array(dnode, "flashlight_current", flashlight_current, 5) == 0) {
		u32 step = 0;
		for (step; step < 5; step++) {
			dev_info(dev, "flashlight_current[%d] = <%d><%d>\n", step,
				flashlight_current[step], RT8547_MOVIE_CURRENT(flashlight_current[step])&0x0F);
			pdata->flashlight_current_value[step] = RT8547_MOVIE_CURRENT(flashlight_current[step])&0x0F;
		}
	}

	return ret;
}

static int rt8547_probe(struct platform_device *pdev)
{
	struct rt8547_platform_data *pdata;
	int ret = 0;
	unsigned long flags = 0;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}
		ret = rt8547_parse_dt(&pdev->dev, pdata);
		if (ret < 0) {
			return -EFAULT;
		}
	} else {
		pdata = pdev->dev.platform_data;
		if (pdata == NULL) {
			return -EFAULT;
		}
	}

	global_rt8547data = pdata;
	global_dev = &pdev->dev;

	LED_INFO("RT8547_LED Probe\n");

	rt8547_dev = device_create(camera_class, NULL, 0, NULL, "flash");
	if (IS_ERR(rt8547_dev)) {
		LED_ERROR("Failed to create device(flash)!\n");
	}

	if (device_create_file(rt8547_dev, &dev_attr_rear_flash) < 0) {
		LED_ERROR("failed to create device file, %s\n",
				dev_attr_rear_flash.attr.name);
	}
	if (device_create_file(rt8547_dev, &dev_attr_rear_torch_flash) < 0) {
		LED_ERROR("failed to create device file, %s\n",
				dev_attr_rear_torch_flash.attr.name);
	}
	spin_lock_init(&pdata->int_lock);

	gpio_request_one(global_rt8547data->flash_control, GPIOF_OUT_INIT_LOW, "rt8547_led_control");
	gpio_request_one(global_rt8547data->flash_en, GPIOF_OUT_INIT_LOW, "rt8547_led_en");

	/* LVP disable */
	spin_lock_irqsave(&global_rt8547data->int_lock, flags);
	rt8547_write_data(RT8547_ADDR_HIDDEN_SETTING, RT8547_HIDDEN_LVP_DISABLE);
	udelay(300);
	rt8547_setGpio(0);
	spin_unlock_irqrestore(&global_rt8547data->int_lock, flags);
	lvp_enabled = false;

	gpio_free(global_rt8547data->flash_control);
	gpio_free(global_rt8547data->flash_en);

	return 0;
}
static int rt8547_remove(struct platform_device *pdev)
{
	device_remove_file(rt8547_dev, &dev_attr_rear_flash);
	device_remove_file(rt8547_dev, &dev_attr_rear_torch_flash);
	device_destroy(camera_class, 0);
	class_destroy(camera_class);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id rt8547_dt_ids[] = {
	{.compatible = "rt8547",},
	{},
};
/*MODULE_DEVICE_TABLE(of, rt8547_dt_ids);*/
#endif

static struct platform_driver rt8547_driver = {
	.driver = {
		.name = rt8547_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = rt8547_dt_ids,
#endif
	},
	.probe = rt8547_probe,
	.remove = rt8547_remove,
};

static int __init rt8547_init(void)
{
	return platform_driver_register(&rt8547_driver);
}

static void __exit rt8547_exit(void)
{
	platform_driver_unregister(&rt8547_driver);
}

module_init(rt8547_init);
module_exit(rt8547_exit);

MODULE_AUTHOR("younghoon joo <yhoon.joo@samsung.com.com>");
MODULE_DESCRIPTION("RT8547 driver");
MODULE_LICENSE("GPL");
