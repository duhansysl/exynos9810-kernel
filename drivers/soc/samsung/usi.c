/*
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * EXYNOS - USI(Universal Serial Interface) driver.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

/* USI mode */
#define USI_HSI2C0_SINGLE_MODE		0x1
#define USI_HSI2C1_SINGLE_MODE		0x2
#define USI_HSI2C0_HSI2C1_DUAL_MODE	0x3
#define USI_SPI_SINGLE_MODE		0x4
#define USI_UART_SINGLE_MODE		0x8
#define USI_UART_HSI2C1_DUAL_MODE	0xA

struct usi_mode {
	int val;
	const char *name;
};

struct usi_data {
	void __iomem	*base;
	int 		mode;
	int		ch_id;
};

static const struct usi_mode usi_modes[] = {
	{ .val = USI_HSI2C0_HSI2C1_DUAL_MODE , .name = "hsi2c0_hsi2c1" },
	{ .val = USI_UART_HSI2C1_DUAL_MODE ,   .name = "uart_hsi2c1" },
	{ .val = USI_HSI2C0_SINGLE_MODE ,      .name = "hsi2c0" },
	{ .val = USI_HSI2C1_SINGLE_MODE ,      .name = "hsi2c1" },
	{ .val = USI_SPI_SINGLE_MODE ,         .name = "spi" },
	{ .val = USI_UART_SINGLE_MODE ,        .name = "uart" },
};

static int get_usi_mode(const char* mode_name)
{
	int i;

	for (i = 0; i < sizeof(usi_modes)/sizeof(struct usi_mode); i++) {
		if (!strncmp(mode_name, usi_modes[i].name,
				strlen(usi_modes[i].name)))
			return usi_modes[i].val;
	}
	return -1;
}

static int usi_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct resource *res;
	const char* mode_name;
	struct usi_data *data;

	data = devm_kzalloc(&pdev->dev, sizeof(struct usi_data), GFP_KERNEL);
	if (!data) {
		dev_err(&pdev->dev, "no memory to save usi_data\n");
		return -ENOMEM;
	}

	data->ch_id = of_alias_get_id(pdev->dev.of_node, "usi");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	data->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(data->base))
		return PTR_ERR(data->base);

	if (of_property_read_string(node, "usi_mode", &mode_name) < 0) {
		dev_err(&pdev->dev, "missing usi mode configuration\n");
		return -EINVAL;
	}

	data->mode = get_usi_mode(mode_name);

	if (data->mode < 0) {
		dev_err(&pdev->dev, "wrong usi mode: %s\n", mode_name);
		return -EINVAL;
	}

	platform_set_drvdata(pdev, data);

	writel(data->mode, data->base);

	dev_info(&pdev->dev, "usi_probe() mode:%d\n", data->mode);

	return 0;
}

static const struct of_device_id usi_dt_match[] = {
	{ .compatible = "samsung,exynos-usi", },
	{ },
};

MODULE_DEVICE_TABLE(of, usi_dt_match);

static int usi_resume_noirq(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct usi_data *data = platform_get_drvdata(pdev);
	int ret;

	if (data->mode && data->base) {
		writel(data->mode, data->base);
		dev_info(&pdev->dev, "%s mode:%d\n", __func__, data->mode);
		ret = 0;
	} else {
		dev_err(&pdev->dev, "%s wrong usi data\n", __func__);
		ret = -EINVAL;
	}

	return ret;
}

static const struct dev_pm_ops usi_pm = {
	.resume_noirq = usi_resume_noirq,
};

static struct platform_driver usi_driver = {
	.driver = {
		.name		= "usi",
		.owner		= THIS_MODULE,
		.pm		= &usi_pm,
		.of_match_table	= usi_dt_match,
	},
	.probe			= usi_probe,
};

static int __init usi_init(void)
{
	int ret = platform_driver_register(&usi_driver);

	if (ret)
		pr_err("usi driver registrer failed\n");

	return ret;
}
arch_initcall(usi_init);

static void __exit usi_exit(void)
{
	platform_driver_unregister(&usi_driver);
}
module_exit(usi_exit);

MODULE_AUTHOR("Jung Ick Guack <ji.guack@samsung.com>");
MODULE_DESCRIPTION("SAMSUNG USI driver");
MODULE_LICENSE("GPL");
