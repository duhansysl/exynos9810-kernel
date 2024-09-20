/*
 * Copyright (C) 2015 Samsung Electronics Co. Ltd. All Rights Reserved.
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

#define pr_fmt(fmt) "[VIB] dc_vib: " fmt

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/vibrator/sec_vibrator.h>
#define DC_VIB_NAME "dc_vib"

void __ss_vib_ldo_enable(bool enable) {}
void ss_vib_ldo_enable(bool enable)
	__attribute__((weak, alias("__ss_vib_ldo_enable")));

struct dc_vib_pdata {
	const char *regulator_name;
	struct regulator *regulator;
	int gpio_en;
	bool use_qc_regulator;
	const char *motor_type;
};

struct dc_vib_drvdata {
	struct sec_vibrator_drvdata sec_vib_ddata;
	struct dc_vib_pdata *pdata;
	bool running;
};

static int dc_vib_enable(struct device *dev, bool en)
{
	struct dc_vib_drvdata *ddata = dev_get_drvdata(dev);

	if (en) {
		if (ddata->running)
			return 0;
		if (ddata->pdata->use_qc_regulator) {
			pr_info("%s: qc_regulator on\n", __func__);
			ss_vib_ldo_enable(true);
		}
		if (ddata->pdata->regulator) {
			pr_info("%s: regulator on\n", __func__);
			if (!regulator_is_enabled(ddata->pdata->regulator))
				regulator_enable(ddata->pdata->regulator);
		}
		if (gpio_is_valid(ddata->pdata->gpio_en)) {
			pr_info("%s: gpio on\n", __func__);
			gpio_direction_output(ddata->pdata->gpio_en, 1);
		}
		ddata->running = true;
	} else {
		if (!ddata->running)
			return 0;
		if (gpio_is_valid(ddata->pdata->gpio_en)) {
			pr_info("%s: gpio off\n", __func__);
			gpio_direction_output(ddata->pdata->gpio_en, 0);
		}
		if (ddata->pdata->regulator) {
			pr_info("%s: regulator off\n", __func__);
			if (regulator_is_enabled(ddata->pdata->regulator))
				regulator_disable(ddata->pdata->regulator);
		}
		if (ddata->pdata->use_qc_regulator) {
			pr_info("%s: qc_regulator off\n", __func__);
			ss_vib_ldo_enable(false);
		}
		ddata->running = false;
	}
	return 0;
}

static int dc_vib_get_motor_type(struct device *dev, char *buf)
{
	struct dc_vib_drvdata *ddata = dev_get_drvdata(dev);
	int ret = snprintf(buf, VIB_BUFSIZE, "%s\n", ddata->pdata->motor_type);

	return ret;
}

static const struct sec_vibrator_ops dc_vib_ops = {
	.enable	= dc_vib_enable,
	.get_motor_type = dc_vib_get_motor_type,
};

#if defined(CONFIG_OF)
static struct dc_vib_pdata *dc_vib_get_dt(struct device *dev)
{
	struct device_node *node;
	struct dc_vib_pdata *pdata;
	int ret = 0;

	node = dev->of_node;
	if (!node) {
		ret = -ENODEV;
		goto err_out;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto err_out;
	}

	pdata->use_qc_regulator = of_property_read_bool(node,
			"dc_vib,use_qc_regulator");
	if (pdata->use_qc_regulator)
		pr_info("%s: using qc_regulator\n", __func__);
	else
		pr_info("%s: qc_regulator isn't used\n", __func__);

	pdata->gpio_en = of_get_named_gpio(node, "dc_vib,gpio_en", 0);
	if (gpio_is_valid(pdata->gpio_en)) {
		ret = gpio_request(pdata->gpio_en, "mot_ldo_en");
		if (ret) {
			pr_err("%s: motor gpio request fail(%d)\n",
				__func__, ret);
			goto err_out;
		}
		ret = gpio_direction_output(pdata->gpio_en, 0);
	} else {
		pr_info("%s: gpio isn't used\n", __func__);
	}

	ret = of_property_read_string(node, "dc_vib,regulator_name",
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

	ret = of_property_read_string(node, "dc_vib,motor_type",
			&pdata->motor_type);
	if (ret)
		pr_err("%s: motor_type is undefined\n", __func__);

	return pdata;

err_out:
	return ERR_PTR(ret);
}
#endif

static int dc_vib_probe(struct platform_device *pdev)
{
	struct dc_vib_pdata *pdata = pdev->dev.platform_data;
	struct dc_vib_drvdata *ddata;
	int ret = 0;

	pr_info("%s\n", __func__);
	if (!pdata) {
#if defined(CONFIG_OF)
		pdata = dc_vib_get_dt(&pdev->dev);
		if (IS_ERR(pdata)) {
			pr_err("there is no device tree!\n");
			ret = -ENODEV;
			goto err_pdata;
		}
#else
		ret = -ENODEV;
		pr_err("there is no platform data!\n");
		goto err_pdata;
#endif
	}

	ddata = devm_kzalloc(&pdev->dev, sizeof(struct dc_vib_drvdata),
			GFP_KERNEL);
	if (!ddata) {
		ret = -ENOMEM;
		pr_err("Failed to memory alloc\n");
		goto err_ddata;
	}
	ddata->pdata = pdata;
	platform_set_drvdata(pdev, ddata);
	ddata->sec_vib_ddata.dev = &pdev->dev;
	ddata->sec_vib_ddata.vib_ops = &dc_vib_ops;
	sec_vibrator_register(&ddata->sec_vib_ddata);

	return 0;

err_ddata:
err_pdata:
	return ret;
}

static int dc_vib_remove(struct platform_device *pdev)
{
	struct dc_vib_drvdata *ddata = platform_get_drvdata(pdev);

	if (ddata->pdata->regulator) {
		regulator_put(ddata->pdata->regulator);
	}
	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id dc_vib_dt_ids[] = {
	{ .compatible = "samsung,dc_vibrator" },
	{ }
};
MODULE_DEVICE_TABLE(of, dc_vib_dt_ids);
#endif /* CONFIG_OF */

static struct platform_driver dc_vib_driver = {
	.probe		= dc_vib_probe,
	.remove		= dc_vib_remove,
	.driver		= {
		.name		= DC_VIB_NAME,
		.owner		= THIS_MODULE,
		.of_match_table	= of_match_ptr(dc_vib_dt_ids),
	},
};

static int __init dc_vib_init(void)
{
	return platform_driver_register(&dc_vib_driver);
}
module_init(dc_vib_init);

static void __exit dc_vib_exit(void)
{
	platform_driver_unregister(&dc_vib_driver);
}
module_exit(dc_vib_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("dc vibrator driver");
MODULE_LICENSE("GPL");
