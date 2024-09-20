/* drivers/motor/isa1000.c

 * Copyright (C) 2014 Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "[VIB] isa1000_vib: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/vibrator/sec_vibrator.h>
#if defined(CONFIG_BATTERY_SAMSUNG_V2)
#include "../../battery_v2/include/sec_charging_common.h"
#endif

#define ISA1000_DIVIDER		128
#define FREQ_DIVIDER		NSEC_PER_SEC / ISA1000_DIVIDER * 10

struct isa1000_pdata {
 	int gpio_en;
	const char *regulator_name;
	struct pwm_device *pwm;
	struct regulator *regulator;
	const char *motor_type;

	int frequency;
	int duty_ratio;
	int temp_duty_ratio;
	int temperature;
	/* for multi-frequency */
	int freq_nums;
	u32 *freq_array;
};

struct isa1000_ddata {
	struct isa1000_pdata *pdata;
	struct sec_vibrator_drvdata sec_vib_ddata;
	int max_duty;
	int duty;
	int period;
};

#if defined(CONFIG_BATTERY_SAMSUNG_V2)
static int isa1000_get_temperature_duty_ratio(struct isa1000_ddata *ddata)
{
	union power_supply_propval value = {0, };
	int ret = ddata->pdata->duty_ratio;

	psy_do_property("battery", get, POWER_SUPPLY_PROP_TEMP, value);
	if (value.intval >= ddata->pdata->temperature)
		ret = ddata->pdata->temp_duty_ratio;
	pr_info("%s temp:%d duty:%d\n", __func__, value.intval, ret);
	return ret;
}
#endif

static int isa1000_vib_set_frequency(struct device *dev, int num)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	int duty_ratio = ddata->pdata->duty_ratio;

#if defined(CONFIG_BATTERY_SAMSUNG_V2)
	if (ddata->pdata->temp_duty_ratio)
		duty_ratio = isa1000_get_temperature_duty_ratio(ddata);
#endif
	if (num >= 0 && num < ddata->pdata->freq_nums) {
		ddata->period = FREQ_DIVIDER / ddata->pdata->freq_array[num];
		ddata->duty = ddata->max_duty =
			(ddata->period * duty_ratio) / 100;
	} else if (num >= HAPTIC_ENGINE_FREQ_MIN &&
			num <= HAPTIC_ENGINE_FREQ_MAX) {
		ddata->period = FREQ_DIVIDER / num;
		ddata->duty = ddata->max_duty =
			(ddata->period * duty_ratio) / 100;
	} else {
		pr_err("%s out of range %d\n", __func__, num);
		return -EINVAL;
	}

	return 0;
}

static int isa1000_vib_set_intensity(struct device *dev, int intensity)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	int duty = ddata->period >> 1;
	int ret = 0;

	if (intensity < -(MAX_INTENSITY) || MAX_INTENSITY < intensity) {
		pr_err("%s out of range %d\n", __func__, intensity);
		return -EINVAL;
	}

	if (intensity == MAX_INTENSITY)
		duty = ddata->max_duty;
	else if (intensity == -(MAX_INTENSITY))
		duty = ddata->period - ddata->max_duty;
	else if (intensity != 0)
		duty += (ddata->max_duty - duty) * intensity / MAX_INTENSITY;

	ddata->duty = duty;

	ret = pwm_config(ddata->pdata->pwm, duty, ddata->period);
	if (ret < 0) {
		pr_err("failed to config pwm %d\n", ret);
		return ret;
	}
	if (intensity != 0) {
		ret = pwm_enable(ddata->pdata->pwm);
		if (ret < 0)
			pr_err("failed to enable pwm %d\n", ret);
	} else {
		pwm_disable(ddata->pdata->pwm);
	}

	return ret;
}

static void isa1000_regulator_en(struct isa1000_ddata *ddata, bool en)
{
	int ret;

	if (ddata->pdata->regulator == NULL)
		return;

	if (en && !regulator_is_enabled(ddata->pdata->regulator)) {
		ret = regulator_enable(ddata->pdata->regulator);
		if (ret)
			pr_err("regulator_enable returns: %d\n", ret);
	} else if (!en && regulator_is_enabled(ddata->pdata->regulator)) {
		ret = regulator_disable(ddata->pdata->regulator);
		if (ret)
			pr_err("regulator_disable returns: %d\n", ret);
	}
}

static void isa1000_gpio_en(struct isa1000_ddata *ddata, bool en)
{
	if (gpio_is_valid(ddata->pdata->gpio_en))
		gpio_direction_output(ddata->pdata->gpio_en, en);
}

static int isa1000_vib_enable(struct device *dev, bool en)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);

	if (en) {
		isa1000_regulator_en(ddata, en);
		isa1000_gpio_en(ddata, en);
	} else {
		isa1000_gpio_en(ddata, en);
		isa1000_regulator_en(ddata, en);
	}

	return 0;
}

static int isa1000_get_motor_type(struct device *dev, char *buf)
{
	struct isa1000_ddata *ddata = dev_get_drvdata(dev);
	int ret = snprintf(buf, VIB_BUFSIZE, "%s\n", ddata->pdata->motor_type);

	return ret;
}

static const struct sec_vibrator_ops isa1000_multi_freq_vib_ops = {
	.enable = isa1000_vib_enable,
	.set_intensity = isa1000_vib_set_intensity,
	.set_frequency = isa1000_vib_set_frequency,
	.get_motor_type = isa1000_get_motor_type,
};

static const struct sec_vibrator_ops isa1000_single_freq_vib_ops = {
	.enable = isa1000_vib_enable,
	.set_intensity = isa1000_vib_set_intensity,
	.get_motor_type = isa1000_get_motor_type,
};

static struct isa1000_pdata *isa1000_get_devtree_pdata(struct device *dev)
{
	struct device_node *node;
	struct isa1000_pdata *pdata;
	u32 temp;
	int ret = 0;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	node = dev->of_node;
	if (!node) {
		pr_err("%s: error to get dt node\n", __func__);
		goto err_out;
	}

	ret = of_property_read_u32(node, "isa1000,multi_frequency", &temp);
	if (ret) {
		pr_info("%s: multi_frequency isn't used\n", __func__);
		pdata->freq_nums = 0;
	} else
		pdata->freq_nums = (int)temp;

	if (pdata->freq_nums) {
		pdata->freq_array = devm_kzalloc(dev,
				sizeof(u32)*pdata->freq_nums, GFP_KERNEL);
		if (!pdata->freq_array) {
			pr_err("%s: failed to allocate freq_array data\n",
					__func__);
			goto err_out;
		}

		ret = of_property_read_u32_array(node, "isa1000,frequency",
				pdata->freq_array, pdata->freq_nums);
		if (ret) {
			pr_err("%s: error to get dt node frequency\n",
					__func__);
			goto err_out;
		}

		pdata->frequency = pdata->freq_array[0];
	} else {
		ret = of_property_read_u32(node,
				"isa1000,frequency", &temp);
		if (ret) {
			pr_err("%s: error to get dt node frequency\n",
					__func__);
			goto err_out;
		} else
			pdata->frequency = (int)temp;
	}

	ret = of_property_read_u32(node, "isa1000,duty_ratio",
			&pdata->duty_ratio);
	if (ret) {
		pr_err("%s: error to get dt node duty_ratio\n", __func__);
		goto err_out;
	}

	ret = of_property_read_u32(node, "isa1000,temp_duty_ratio",
			&pdata->temp_duty_ratio);
	if (ret) {
		pr_err("%s: temp_duty_ratio isn't used\n", __func__);
		pdata->temp_duty_ratio = 0;
	}

	ret = of_property_read_u32(node, "isa1000,temperature",
			&pdata->temperature);
	if (ret) {
		pr_err("%s: temperature isn't used\n", __func__);
		pdata->temperature = 0;
	}

	ret = of_property_read_string(node, "isa1000,regulator_name",
			&pdata->regulator_name);
	if (!ret) {
		pdata->regulator = regulator_get(NULL, pdata->regulator_name);
		if (IS_ERR(pdata->regulator)) {
			ret = PTR_ERR(pdata->regulator);
			pdata->regulator = NULL;
			pr_err("%s: regulator get fail\n", __func__);
			goto err_out;
		}
	} else {
		pr_info("%s: regulator isn't used\n", __func__);
		pdata->regulator = NULL;
	}

	pdata->gpio_en = of_get_named_gpio(node, "isa1000,gpio_en", 0);
	if (gpio_is_valid(pdata->gpio_en)) {
		ret = gpio_request(pdata->gpio_en, "isa1000,gpio_en");
		if (ret) {
			pr_err("%s: motor gpio request fail(%d)\n",
				__func__, ret);
			goto err_out;
		}
		ret = gpio_direction_output(pdata->gpio_en, 0);
	} else {
		pr_info("%s: gpio isn't used\n", __func__);
	}

	pdata->pwm = devm_of_pwm_get(dev, node, NULL);
	if (IS_ERR(pdata->pwm)) {
		pr_err("%s: error to get pwms\n", __func__);
		goto err_out;
	}

	ret = of_property_read_string(node, "isa1000,motor_type",
			&pdata->motor_type);
	if (ret)
		pr_err("%s: motor_type is undefined\n", __func__);

	return pdata;

err_out:
	return ERR_PTR(ret);
}

static int isa1000_probe(struct platform_device *pdev)
{
	struct isa1000_pdata *pdata = dev_get_platdata(&pdev->dev);
	struct isa1000_ddata *ddata;

	if (!pdata) {
#if defined(CONFIG_OF)
		pdata = isa1000_get_devtree_pdata(&pdev->dev);
		if (IS_ERR(pdata)) {
			pr_err("there is no device tree!\n");
			return -ENODEV;
		}
#else
		pr_err("there is no platform data!\n");
#endif
	}

	ddata = devm_kzalloc(&pdev->dev, sizeof(*ddata), GFP_KERNEL);
	if (!ddata) {
		pr_err("failed to alloc\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, ddata);
	ddata->pdata = pdata;
	ddata->period = FREQ_DIVIDER / pdata->frequency;
	ddata->max_duty = ddata->duty =
		ddata->period * ddata->pdata->duty_ratio / 100;

	ddata->sec_vib_ddata.dev = &pdev->dev;
	if (pdata->freq_nums)
		ddata->sec_vib_ddata.vib_ops = &isa1000_multi_freq_vib_ops;
	else
		ddata->sec_vib_ddata.vib_ops = &isa1000_single_freq_vib_ops;
	sec_vibrator_register(&ddata->sec_vib_ddata);

	return 0;
}

static int isa1000_remove(struct platform_device *pdev)
{
	struct isa1000_ddata *ddata = platform_get_drvdata(pdev);

	sec_vibrator_unregister(&ddata->sec_vib_ddata);
	isa1000_vib_enable(&pdev->dev, false);
	return 0;
}

static int isa1000_suspend(struct platform_device *pdev,
			pm_message_t state)
{
	isa1000_vib_enable(&pdev->dev, false);
	return 0;
}

static int isa1000_resume(struct platform_device *pdev)
{
	return 0;
}

#if defined(CONFIG_OF)
static struct of_device_id isa1000_dt_ids[] = {
	{ .compatible = "imagis,isa1000_vibrator" },
	{ }
};
MODULE_DEVICE_TABLE(of, isa1000_dt_ids);
#endif /* CONFIG_OF */

static struct platform_driver isa1000_driver = {
	.probe		= isa1000_probe,
	.remove		= isa1000_remove,
	.suspend	= isa1000_suspend,
	.resume		= isa1000_resume,
	.driver = {
		.name	= "isa1000",
		.owner	= THIS_MODULE,
		.of_match_table	= of_match_ptr(isa1000_dt_ids),
	},
};

static int __init isa1000_init(void)
{
	return platform_driver_register(&isa1000_driver);
}
module_init(isa1000_init);

static void __exit isa1000_exit(void)
{
	platform_driver_unregister(&isa1000_driver);
}
module_exit(isa1000_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("isa1000 vibrator driver");
