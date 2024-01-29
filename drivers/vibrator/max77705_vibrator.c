/*
 * haptic motor driver for max77705_vibrator.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "[VIB] max77705_vib: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/max77705.h>
#include <linux/mfd/max77705-private.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/vibrator/sec_vibrator.h>

#define MOTOR_LRA                       (1<<7)
#define MOTOR_EN                        (1<<6)
#define EXT_PWM                         (0<<5)
#define DIVIDER_128                     (1<<1)
#define MRDBTMER_MASK                   (0x7)
#define MREN                            (1 << 3)
#define BIASEN                          (1 << 7)

/* 1s / 128(divider value) * 10(150Hz -> 1500 so multiply 10) = 78125000 ns */
#define FREQ_DIVIDER			78125000

struct max77705_vibrator_drvdata {
	struct max77705_dev *max77705;
	struct sec_vibrator_drvdata sec_vib_ddata;
	struct i2c_client *i2c;
	struct max77705_vibrator_pdata *pdata;
	struct pwm_device *pwm;
	struct regulator *regulator;
	int ratio;
	int max_duty;
	int period;
	int duty;
	bool is_high_temp;
#if defined(CONFIG_MAX77705_VIB_FOLD_MODEL)
	int event_idx;
#endif
};

static int max77705_vib_set_ratio_with_temp(struct device *dev, int temperature)
{
	struct max77705_vibrator_drvdata *ddata = dev_get_drvdata(dev);
	struct max77705_vibrator_pdata *pdata = ddata->pdata;

	pr_info("temperature is %d\n", temperature);

	if (temperature >= pdata->high_temp_ref)
		ddata->is_high_temp = true;
	else
		ddata->is_high_temp = false;

	return 0;
}

static int max77705_vibrator_i2c_enable(struct max77705_vibrator_drvdata *ddata,
		bool en)
{
	int ret = max77705_update_reg(ddata->i2c,
			MAX77705_PMIC_REG_MCONFIG,
			en ? 0xff : 0x0, MOTOR_LRA | MOTOR_EN);
	if (ret)
		pr_err("i2c LRA and EN update error %d\n", ret);

	return ret;
}

static void max77705_vibrator_init_reg(
		struct max77705_vibrator_drvdata *ddata)
{
	int ret = 0;

	ret = max77705_update_reg(ddata->i2c,
			MAX77705_PMIC_REG_MAINCTRL1, 0xff, BIASEN);
	if (ret)
		pr_err("i2c REG_BIASEN update error %d\n", ret);

	ret = max77705_update_reg(ddata->i2c,
		MAX77705_PMIC_REG_MCONFIG, 0xff, MOTOR_LRA);
	if (ret)
		pr_err("i2c MOTOR_LPA update error %d\n", ret);

	ret = max77705_update_reg(ddata->i2c,
		MAX77705_PMIC_REG_MCONFIG, 0xff, DIVIDER_128);
	if (ret)
		pr_err("i2c DIVIDER_128 update error %d\n", ret);
}

static int max77705_vibrator_enable(struct device *dev, bool en)
{
	struct max77705_vibrator_drvdata *ddata = dev_get_drvdata(dev);
	int ret = 0;

	ret = max77705_vibrator_i2c_enable(ddata, en);

	return ret;
}

static int max77705_vib_set_intensity(struct device *dev, int intensity)
{
	struct max77705_vibrator_drvdata *ddata = dev_get_drvdata(dev);
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

	ret = pwm_config(ddata->pwm, duty, ddata->period);
	if (ret < 0) {
		pr_err("failed to config pwm %d\n", ret);
		return ret;
	}
	if (duty != ddata->period >> 1) {
		ret = pwm_enable(ddata->pwm);
		if (ret < 0)
			pr_err("failed to enable pwm %d\n", ret);
	} else {
		pwm_disable(ddata->pwm);
	}

	return ret;
}

#if defined(CONFIG_MAX77705_VIB_FOLD_MODEL)
static int set_fold_model_ratio(struct max77705_vibrator_drvdata *ddata)
{
	int ratio = 0;

	switch (ddata->event_idx) {
	case EVENT_CMD_FOLDER_OPEN:
		ratio = ddata->pdata->fold_open_ratio;
		break;
	case EVENT_CMD_FOLDER_CLOSE:
		ratio = ddata->pdata->fold_close_ratio;
		break;
	case EVENT_CMD_ACCESSIBILITY_BOOST_ON:
	case EVENT_CMD_ACCESSIBILITY_BOOST_OFF:
	case EVENT_CMD_NONE:
	default:
		ratio = ddata->pdata->normal_ratio;
		break;
	}

	return ratio;
}
#endif

static int max77705_vib_set_ratio(struct max77705_vibrator_drvdata *ddata)
{
	int ratio;

	if (ddata->is_high_temp)
		ratio = ddata->pdata->high_temp_ratio;
	else {
#if defined(CONFIG_MAX77705_VIB_FOLD_MODEL)
		ratio = set_fold_model_ratio(ddata);
#else
		ratio = ddata->pdata->normal_ratio;
#endif
	}

	pr_info("ratio set to %d\n", ratio);

	return ratio;
}

static int max77705_vib_set_frequency(struct device *dev, int num)
{
	struct max77705_vibrator_drvdata *ddata = dev_get_drvdata(dev);

	ddata->ratio = max77705_vib_set_ratio(ddata);

	if (num >= 0 && num < ddata->pdata->freq_nums) {
		ddata->period = FREQ_DIVIDER / ddata->pdata->freq_array[num];
		ddata->duty = ddata->max_duty =
			(ddata->period * ddata->ratio) / 100;
	} else if (num >= HAPTIC_ENGINE_FREQ_MIN &&
			num <= HAPTIC_ENGINE_FREQ_MAX) {
		ddata->period = FREQ_DIVIDER / num;
		ddata->duty = ddata->max_duty =
			(ddata->period * ddata->ratio) / 100;
	} else {
		pr_err("%s out of range %d\n", __func__, num);
		return -EINVAL;
	}

	return 0;
}

static int max77705_vib_set_overdrive(struct device *dev, bool en)
{
	struct max77705_vibrator_drvdata *ddata = dev_get_drvdata(dev);

	if (ddata->pdata->overdrive_ratio)
		ddata->ratio = en ? ddata->pdata->overdrive_ratio :
			ddata->pdata->normal_ratio;

	return 0;
}

static int max77705_vib_get_motor_type(struct device *dev, char *buf)
{
	struct max77705_vibrator_drvdata *ddata = dev_get_drvdata(dev);

	return snprintf(buf, VIB_BUFSIZE, "%s\n", ddata->pdata->motor_type);
}

#if defined(CONFIG_MAX77705_VIB_FOLD_MODEL)
static int max77705_vib_set_event_cmd(struct device *dev, int idx)
{
	struct max77705_vibrator_drvdata *ddata = dev_get_drvdata(dev);

	ddata->event_idx = idx;

	return 0;
}
#endif

#if defined(CONFIG_SEC_VIBRATOR)
static bool max77705_get_calibration(struct device *dev)
{
	struct max77705_vibrator_drvdata *ddata = dev_get_drvdata(dev);
	struct max77705_vibrator_pdata *pdata = ddata->pdata;

	return pdata->calibration;
}

static int max77705_get_step_size(struct device *dev, int *step_size)
{
	struct max77705_vibrator_drvdata *ddata = dev_get_drvdata(dev);
	struct max77705_vibrator_pdata *pdata = ddata->pdata;

	pr_info("%s step_size=%d\n", __func__, pdata->steps);

	if (pdata->steps == 0)
		return -ENODATA;

	*step_size = pdata->steps;

	return 0;
}


static int max77705_get_intensities(struct device *dev, int *buf)
{
	struct max77705_vibrator_drvdata *ddata = dev_get_drvdata(dev);
	struct max77705_vibrator_pdata *pdata = ddata->pdata;
	int i;

	if (pdata->intensities[1] == 0)
		return -ENODATA;

	for (i = 0; i < pdata->steps; i++)
		buf[i] = pdata->intensities[i];

	return 0;
}

static int max77705_set_intensities(struct device *dev, int *buf)
{
	struct max77705_vibrator_drvdata *ddata = dev_get_drvdata(dev);
	struct max77705_vibrator_pdata *pdata = ddata->pdata;
	int i;

	for (i = 0; i < pdata->steps; i++)
		pdata->intensities[i] = buf[i];

	return 0;
}

static int max77705_get_haptic_intensities(struct device *dev, int *buf)
{
	struct max77705_vibrator_drvdata *ddata = dev_get_drvdata(dev);
	struct max77705_vibrator_pdata *pdata = ddata->pdata;
	int i;

	if (pdata->haptic_intensities[1] == 0)
		return -ENODATA;

	for (i = 0; i < pdata->steps; i++)
		buf[i] = pdata->haptic_intensities[i];

	return 0;
}

static int max77705_set_haptic_intensities(struct device *dev, int *buf)
{
	struct max77705_vibrator_drvdata *ddata = dev_get_drvdata(dev);
	struct max77705_vibrator_pdata *pdata = ddata->pdata;
	int i;

	for (i = 0; i < pdata->steps; i++)
		pdata->haptic_intensities[i] = buf[i];

	return 0;
}
#endif /* if defined(CONFIG_SEC_VIBRATOR) */

static const struct sec_vibrator_ops max77705_multi_freq_vib_ops = {
	.enable = max77705_vibrator_enable,
	.set_intensity = max77705_vib_set_intensity,
	.set_frequency = max77705_vib_set_frequency,
	.set_overdrive = max77705_vib_set_overdrive,
	.set_force_touch_intensity = max77705_vib_set_intensity,
	.get_motor_type = max77705_vib_get_motor_type,
	.set_tuning_with_temp = max77705_vib_set_ratio_with_temp,
#if defined(CONFIG_MAX77705_VIB_FOLD_MODEL)
	.set_event_cmd = max77705_vib_set_event_cmd,
#endif
#if defined(CONFIG_SEC_VIBRATOR)
	.get_calibration = max77705_get_calibration,
	.get_step_size = max77705_get_step_size,
	.get_intensities = max77705_get_intensities,
	.set_intensities = max77705_set_intensities,
	.get_haptic_intensities = max77705_get_haptic_intensities,
	.set_haptic_intensities = max77705_set_haptic_intensities,
#endif
};

static const struct sec_vibrator_ops max77705_single_freq_vib_ops = {
	.enable = max77705_vibrator_enable,
	.set_intensity = max77705_vib_set_intensity,
	.get_motor_type = max77705_vib_get_motor_type,
	.set_tuning_with_temp = max77705_vib_set_ratio_with_temp,
#if defined(CONFIG_MAX77705_VIB_FOLD_MODEL)
	.set_event_cmd = max77705_vib_set_event_cmd,
#endif
#if defined(CONFIG_SEC_VIBRATOR)
	.get_calibration = max77705_get_calibration,
	.get_step_size = max77705_get_step_size,
	.get_intensities = max77705_get_intensities,
	.set_intensities = max77705_set_intensities,
	.get_haptic_intensities = max77705_get_haptic_intensities,
	.set_haptic_intensities = max77705_set_haptic_intensities,
#endif
};

#if defined(CONFIG_SEC_VIBRATOR)
static int of_sec_vibrator_dt(struct max77705_vibrator_pdata *pdata, struct device_node *np)
{
	int ret = 0;
	int i;
	unsigned int val = 0;
	int *intensities = NULL;

	pr_info("%s\n", __func__);
	pdata->calibration = false;

	/* number of steps */
	ret = of_property_read_u32(np, "samsung,steps", &val);
	if (ret) {
		pr_err("%s out of range(%d)\n", __func__, val);
		return -EINVAL;
	}
	pdata->steps = (int)val;

	/* allocate memory for intensities */
	pdata->intensities = kmalloc_array(pdata->steps, sizeof(int), GFP_KERNEL);
	if (!pdata->intensities)
		return -ENOMEM;
	intensities = pdata->intensities;

	/* intensities */
	ret = of_property_read_u32_array(np, "samsung,intensities", intensities, pdata->steps);
	if (ret) {
		pr_err("intensities are not specified\n");
		ret = -EINVAL;
		goto err_getting_int;
	}

	for (i = 0; i < pdata->steps; i++) {
		if ((intensities[i] < 0) || (intensities[i] > MAX_INTENSITY)) {
			pr_err("%s out of range(%d)\n", __func__, intensities[i]);
			ret = -EINVAL;
			goto err_getting_int;
		}
	}
	intensities = NULL;

	/* allocate memory for haptic_intensities */
	pdata->haptic_intensities = kmalloc_array(pdata->steps, sizeof(int), GFP_KERNEL);
	if (!pdata->haptic_intensities) {
		ret = -ENOMEM;
		goto err_alloc_haptic;
	}
	intensities = pdata->haptic_intensities;

	/* haptic intensities */
	ret = of_property_read_u32_array(np, "samsung,haptic_intensities", intensities, pdata->steps);
	if (ret) {
		pr_err("haptic_intensities are not specified\n");
		ret = -EINVAL;
		goto err_haptic;
	}
	for (i = 0; i < pdata->steps; i++) {
		if ((intensities[i] < 0) || (intensities[i] > MAX_INTENSITY)) {
			pr_err("%s out of range(%d)\n", __func__, intensities[i]);
			ret = -EINVAL;
			goto err_haptic;
		}
	}

	/* update calibration statue */
	pdata->calibration = true;

	return ret;

err_haptic:
	kfree(pdata->haptic_intensities);
err_alloc_haptic:
	pdata->haptic_intensities = NULL;
err_getting_int:
	kfree(pdata->intensities);
	pdata->intensities = NULL;
	pdata->steps = 0;

	return ret;
}
#endif /* if defined(CONFIG_SEC_VIBRATOR) */

#if defined(CONFIG_OF)
static struct max77705_vibrator_pdata *of_max77705_vibrator_dt(
		struct device *dev)
{
	struct device_node *np_root = dev->parent->of_node;
	struct device_node *np;
	struct max77705_vibrator_pdata *pdata;
	u32 temp;
	int ret, i;

	pdata = kzalloc(sizeof(struct max77705_vibrator_pdata),
			GFP_KERNEL);
	if (!pdata)
		return NULL;

	np = of_find_node_by_name(np_root,
			"haptic");
	if (np == NULL) {
		pr_err("%s: error to get dt node\n", __func__);
		goto err_parsing_dt;
	}

	ret = of_property_read_u32(np, "haptic,multi_frequency", &temp);
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
			goto err_parsing_dt;
		}

		ret = of_property_read_u32_array(np, "haptic,frequency",
				pdata->freq_array, pdata->freq_nums);
		if (ret) {
			pr_err("%s: error to get dt node frequency\n",
					__func__);
			goto err_parsing_dt;
		}

		pdata->freq = pdata->freq_array[0];
	} else {
		ret = of_property_read_u32(np,
				"haptic,frequency", &temp);
		if (ret) {
			pr_err("%s: error to get dt node frequency\n",
					__func__);
			goto err_parsing_dt;
		} else
			pdata->freq = (int)temp;
	}

	ret = of_property_read_u32(np,
			"haptic,normal_ratio", &temp);
	if (ret) {
		pr_err("%s: error to get dt node normal_ratio\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->normal_ratio = (int)temp;

	ret = of_property_read_u32(np,
			"haptic,overdrive_ratio", &temp);
	if (ret)
		pr_info("%s: overdrive_ratio isn't used\n", __func__);
	else
		pdata->overdrive_ratio = (int)temp;

	ret = of_property_read_u32(np,
			"haptic,high_temp_ref", &temp);
	if (ret)
		pr_info("%s: high temperature concept isn't used\n", __func__);
	else
		pdata->high_temp_ref = (int)temp * 10;

	ret = of_property_read_u32(np,
			"haptic,high_temp_ratio", &temp);
	if (ret)
		pr_info("%s: high_temp_ratio isn't used\n", __func__);
	else
		pdata->high_temp_ratio = (int)temp;

#if defined(CONFIG_MAX77705_VIB_FOLD_MODEL)
	ret = of_property_read_u32(np,
			"haptic,fold_open_ratio", &temp);
	if (ret) {
		pr_err("%s: error to get dt node fold_open_ratio\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->fold_open_ratio = (int)temp;

	ret = of_property_read_u32(np,
			"haptic,fold_close_ratio", &temp);
	if (ret) {
		pr_err("%s: error to get dt node fold_close_ratio\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->fold_close_ratio = (int)temp;
#endif

	ret = of_property_read_u32(np,
			"haptic,pwm_id", &temp);
	if (ret) {
		pdata->pwm_id = 0;
		pdata->pwm = devm_of_pwm_get(dev, np, NULL);
		if (IS_ERR(pdata->pwm)) {
			pr_err("%s: error to get pwms\n", __func__);
			goto err_parsing_dt;
		}
	} else
		pdata->pwm_id = (u16)temp;

	ret = of_property_read_string(np, "haptic,motor_type",
			&pdata->motor_type);
	if (ret)
		pr_err("%s: motor_type is undefined\n", __func__);

	if (pdata->freq_nums) {
		pr_info("multi frequency: %d\n", pdata->freq_nums);
		for (i = 0; i < pdata->freq_nums; i++)
			pr_info("frequency[%d]: %d.%dHz\n", i,
					pdata->freq_array[i]/10,
					pdata->freq_array[i]%10);
	} else {
		pr_info("frequency: %d.%dHz\n", pdata->freq/10, pdata->freq%10);
	}

#if defined(CONFIG_SEC_VIBRATOR)
	ret = of_sec_vibrator_dt(pdata, np);
	if (ret < 0)
		pr_err("sec_vibrator dt read fail\n");
#endif

	pr_info("normal_ratio: %d\n", pdata->normal_ratio);
	pr_info("overdrive_ratio: %d\n", pdata->overdrive_ratio);
	pr_info("high temperature reference: %d\n", pdata->high_temp_ref);
	pr_info("high temperature ratio: %d\n", pdata->high_temp_ratio);
#if defined(CONFIG_MAX77705_VIB_FOLD_MODEL)
	pr_info("fold open ratio: %d\n", pdata->fold_open_ratio);
	pr_info("fold close ratio: %d\n", pdata->fold_close_ratio);
#endif
	pr_info("motor_type: %s\n", pdata->motor_type);
	pr_info("pwm_id = %d\n", pdata->pwm_id);

	return pdata;

err_parsing_dt:
	kfree(pdata);
	return NULL;
}
#endif

static int max77705_vibrator_probe(struct platform_device *pdev)
{
	struct max77705_dev *max77705 = dev_get_drvdata(pdev->dev.parent);
	struct max77705_platform_data *max77705_pdata
		= dev_get_platdata(max77705->dev);
	struct max77705_vibrator_pdata *pdata
		= max77705_pdata->vibrator_data;
	struct max77705_vibrator_drvdata *ddata;

#if defined(CONFIG_OF)
	if (pdata == NULL) {
		pdata = of_max77705_vibrator_dt(&pdev->dev);
		if (unlikely(!pdata)) {
			pr_err("max77705-haptic : %s not found haptic dt!\n",
					__func__);
			return -ENODEV;
		}
	}
#else
	if (unlikely(!pdata)) {
		pr_err("%s: no pdata\n", __func__);
		return -ENODEV;
	}
#endif /* CONFIG_OF */

	ddata = kzalloc(sizeof(struct max77705_vibrator_drvdata), GFP_KERNEL);
	if (unlikely(!ddata)) {
		pr_err("%s: no ddata\n", __func__);
		return -ENODEV;
	}

	platform_set_drvdata(pdev, ddata);
	ddata->max77705 = max77705;
	ddata->i2c = max77705->i2c;
	ddata->pdata = pdata;
	ddata->period = FREQ_DIVIDER / pdata->freq;
	ddata->ratio = pdata->normal_ratio;
	ddata->max_duty = ddata->duty = ddata->period * ddata->ratio / 100;

	ddata->pwm = pwm_request(pdata->pwm_id, "vibrator");
	if (IS_ERR(ddata->pwm))
		ddata->pwm = pdata->pwm;

	if (IS_ERR(ddata->pwm)) {
		pr_err("%s: Failed to get pwm\n", __func__);
		return -EFAULT;
	} else
		pwm_config(ddata->pwm, ddata->duty, ddata->period);

	max77705_vibrator_init_reg(ddata);
	max77705_vibrator_i2c_enable(ddata, false);

	ddata->sec_vib_ddata.dev = &pdev->dev;
	if (pdata->freq_nums)
		ddata->sec_vib_ddata.vib_ops = &max77705_multi_freq_vib_ops;
	else
		ddata->sec_vib_ddata.vib_ops = &max77705_single_freq_vib_ops;
#if defined(CONFIG_MAX77705_VIB_FOLD_MODEL)
	sscanf("FOLD", "%s", ddata->sec_vib_ddata.event_cmd);
#endif
	sec_vibrator_register(&ddata->sec_vib_ddata);

	return 0;
}

static int max77705_vibrator_remove(struct platform_device *pdev)
{
	struct max77705_vibrator_drvdata *ddata
		= platform_get_drvdata(pdev);

	sec_vibrator_unregister(&ddata->sec_vib_ddata);
	pwm_free(ddata->pwm);
	max77705_vibrator_i2c_enable(ddata, false);
	return 0;
}

static int max77705_vibrator_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	struct max77705_vibrator_drvdata *ddata
		= platform_get_drvdata(pdev);

	if (ddata && &ddata->sec_vib_ddata) {
		kthread_flush_worker(&ddata->sec_vib_ddata.kworker);
		hrtimer_cancel(&ddata->sec_vib_ddata.timer);
	}

	return 0;
}

static int max77705_vibrator_resume(struct platform_device *pdev)
{
	struct max77705_vibrator_drvdata *ddata
		= platform_get_drvdata(pdev);

	max77705_vibrator_init_reg(ddata);

	return 0;
}

static void max77705_vibrator_shutdown(struct platform_device *pdev)
{
}

static struct platform_driver max77705_vibrator_driver = {
	.probe		= max77705_vibrator_probe,
	.remove		= max77705_vibrator_remove,
	.suspend	= max77705_vibrator_suspend,
	.resume		= max77705_vibrator_resume,
	.shutdown	= max77705_vibrator_shutdown,
	.driver = {
		.name	= "max77705-haptic", // max77705_vibrator
		.owner	= THIS_MODULE,
	},
};

static int __init max77705_vibrator_init(void)
{
	return platform_driver_register(&max77705_vibrator_driver);
}
module_init(max77705_vibrator_init);

static void __exit max77705_vibrator_exit(void)
{
	platform_driver_unregister(&max77705_vibrator_driver);
}
module_exit(max77705_vibrator_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("max77705 vibrator driver");