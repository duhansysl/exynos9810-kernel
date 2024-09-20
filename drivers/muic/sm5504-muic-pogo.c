/*
 * driver/muic/sm5504-muic-pogo.c - sm5504 micro USB device driver for POGO
 *
 * Copyright (C) 2020 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#define pr_fmt(fmt)	"[MUIC] " fmt

#include <linux/device.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>

enum {
	SM5504_MUIC_REG_DEV_ID		= 0x01,
	SM5504_MUIC_REG_CTRL		= 0x02,
	SM5504_MUIC_REG_INT1		= 0x03,
	SM5504_MUIC_REG_INT2		= 0x04,
	SM5504_MUIC_REG_INT_MASK1	= 0x05,
	SM5504_MUIC_REG_INT_MASK2	= 0x06,
	SM5504_MUIC_REG_ADC		= 0x07,
	SM5504_MUIC_REG_DEV_TYPE1	= 0x0A,
	SM5504_MUIC_REG_DEV_TYPE2	= 0x0B,
	SM5504_MUIC_REG_MAN_SW1		= 0x13,
	SM5504_MUIC_REG_MAN_SW2		= 0x14,
	SM5504_MUIC_REG_RESET		= 0x1B,
	SM5504_MUIC_REG_CHG		= 0x24,
	SM5504_MUIC_REG_SET		= 0x20,
	SM5504_MUIC_REG_END,
};

/* CONTROL : REG_CTRL */
#define ADC_EN		(1 << 7)
#define USBCHDEN	(1 << 6)
#define CHGTYP		(1 << 5)
#define SWITCH_OPEN	(1 << 4)
#define MANUAL_SWITCH	(1 << 2)
#define MASK_INT	(1 << 0)
#define CTRL_INIT	(ADC_EN | USBCHDEN | CHGTYP | MANUAL_SWITCH)
#define RESET_DEFAULT	(ADC_EN | USBCHDEN | CHGTYP | MANUAL_SWITCH | MASK_INT)

struct sm5504_muic_pogo_data {
	struct device *dev;
	struct i2c_client *i2c;
	int irq_gpio;
	int usb_mux_sel;
	int irq;
	int adc;
	muic_attached_dev_t attached_dev;
};

static int sm5504_i2c_read_byte(const struct i2c_client *client, u8 command)
{
	int ret;
	int retry = 0;

	ret = i2c_smbus_read_byte_data(client, command);

	while (ret < 0) {
		pr_err("%s: reg(0x%x), retrying...\n", __func__, command);
		if (retry > 5) {
			pr_err("%s: retry failed!!\n", __func__);
			break;
		}
		msleep(100);
		ret = i2c_smbus_read_byte_data(client, command);
		retry++;
	}

	return ret;
}

static int sm5504_i2c_write_byte(const struct i2c_client *client,
		u8 command, u8 value)
{
	int ret;
	int retry = 0;
	int written = 0;

	ret = i2c_smbus_write_byte_data(client, command, value);

	while (ret < 0) {
		pr_err("%s: reg(0x%x), retrying...\n", __func__, command);
		if (retry > 5) {
			pr_err("%s: retry failed!!\n", __func__);
			break;
		}
		msleep(100);
		ret = i2c_smbus_read_byte_data(client, command);
		if (written < 0)
			pr_err("%s: reg(0x%x)\n", __func__, command);
		ret = i2c_smbus_write_byte_data(client, command, value);
		retry++;
	}

	return ret;
}

static int sm5504_muic_reg_init(struct sm5504_muic_pogo_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0;
	int dev1 = 0, dev2 = 0, adc = 0, ctrl = 0;
	int int1, int2;

	/* Read and Clear Interrupt1/2 */
	int1 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT1);
	int2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT2);
	pr_info("%s: INT1[0x%02x],INT2[0x%02x]\n", __func__, int1, int2);

	dev1 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_DEV_TYPE1);
	dev2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_DEV_TYPE2);
	adc = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_ADC);
	ctrl = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_CTRL);

	pr_info("%s: DEV1[0x%02x] DEV2[0x%02x] ADC[0x%02x] CTRL[0x%02x]\n",
			__func__, dev1, dev2, adc, ctrl);

	ret = sm5504_i2c_write_byte(i2c, SM5504_MUIC_REG_CTRL, CTRL_INIT);
	if (ret < 0)
		pr_err( "%s: failed to write ctrl(%d)\n", __func__, ret);
	ctrl = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_CTRL);

	pr_info("%s: After: CTRL[0x%02x]\n", __func__, ctrl);

	return ret;
}

static void sm5504_muic_set_usb_mux_sel_gpio(
		struct sm5504_muic_pogo_data *muic_data, int val)
{
	if (!gpio_is_valid(muic_data->usb_mux_sel))
		return;

	gpio_direction_output(muic_data->usb_mux_sel, val);
}

static irqreturn_t sm5504_muic_irq_thread(int irq, void *data)
{
	struct sm5504_muic_pogo_data *muic_data = data;
	struct i2c_client *i2c = muic_data->i2c;
	int int1 = 0, int2 = 0;
	int ctrl = 0, adc = 0;
	muic_attached_dev_t new_dev = ATTACHED_DEV_NONE_MUIC;
	muic_attached_dev_t attached_dev = muic_data->attached_dev;

	/* Read and Clear Interrupt1/2 */
	int1 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT1);
	int2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT2);
	adc = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_ADC);
	ctrl = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_CTRL);
	pr_info("%s: INT1[0x%02x],INT2[0x%02x] CTRL[0x%02x] ADC[0x%02x]\n",
			__func__, int1, int2, ctrl, adc);

	if (ctrl & 0x01) {
		sm5504_muic_reg_init(muic_data);
		return IRQ_HANDLED;
	}

	switch (adc) {
	case ADC_INCOMPATIBLE_VZW:	/* 34K ohm */
		sm5504_muic_set_usb_mux_sel_gpio(muic_data, 1);
		new_dev = ATTACHED_DEV_POGO_DOCK_34K_MUIC;
		break;
	case ADC_HMT:			/* 49.9K ohm */
		sm5504_muic_set_usb_mux_sel_gpio(muic_data, 1);
		new_dev = ATTACHED_DEV_POGO_DOCK_49_9K_MUIC;
		break;
	default:
		adc = ADC_OPEN;
		sm5504_muic_set_usb_mux_sel_gpio(muic_data, 0);
		break;
	}

	if (adc != muic_data->adc) {
		muic_data->adc = adc;
		muic_set_pogo_adc(adc);
	}

	if (new_dev != ATTACHED_DEV_NONE_MUIC)
		muic_pogo_notifier_attach_attached_dev(new_dev);
	else
		muic_pogo_notifier_detach_attached_dev(attached_dev);

	muic_data->attached_dev = new_dev;

	return IRQ_HANDLED;
}

static int sm5504_muic_irq_init(struct sm5504_muic_pogo_data *muic_data)
{
	int ret = 0;

	pr_info("%s\n", __func__);
	muic_data->irq = gpio_to_irq(muic_data->irq_gpio);

	ret = request_threaded_irq(muic_data->irq, NULL, sm5504_muic_irq_thread,
				(IRQF_TRIGGER_LOW | IRQF_ONESHOT),
				"sm5504-muic-pogo", muic_data);
	if (ret < 0) {
		pr_err("%s: failed to request IRQ(%d)\n", __func__,
				muic_data->irq);
		return ret;
	}

	ret = enable_irq_wake(muic_data->irq);
	if (ret < 0) {
		pr_err("%s: failed to enable wakeup src\n", __func__);
		free_irq(muic_data->irq, muic_data);
	}

	return ret;
}

#if defined(CONFIG_OF)
static int of_sm5504_muic_dt(struct device *dev, struct sm5504_muic_pogo_data *muic_data)
{
	struct device_node *np_muic = dev->of_node;
	int ret = 0;

	if (np_muic == NULL) {
		pr_err("%s: np NULL\n", __func__);
		return -EINVAL;
	}

	muic_data->irq_gpio = of_get_named_gpio(np_muic,
			"sm5504_muic,irq-gpio", 0);
	if (muic_data->irq_gpio < 0) {
		pr_err("%s: failed to get muic_irq = %d\n", __func__,
				muic_data->irq_gpio);
		muic_data->irq_gpio = 0;
		ret = -EINVAL;
	}

	muic_data->usb_mux_sel = of_get_named_gpio(np_muic,
			"sm5504_muic,usb-mux-sel", 0);
	if (muic_data->usb_mux_sel < 0) {
		pr_err("%s: failed to get usb_mux_sel = %d\n", __func__,
				muic_data->usb_mux_sel);
		muic_data->usb_mux_sel = 0;
		ret = -EINVAL;
	}
	sm5504_muic_set_usb_mux_sel_gpio(muic_data, 0);

	return ret;
}
#endif /* CONFIG_OF */

static int sm5504_muic_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);
	struct sm5504_muic_pogo_data *muic_data;
	int ret = 0;

	pr_info("%s\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s: i2c functionality check error\n", __func__);
		ret = -EIO;
		goto err_return;
	}

	muic_data = devm_kzalloc(&i2c->dev,
			sizeof(struct sm5504_muic_pogo_data), GFP_KERNEL);
	if (!muic_data) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	/* save platfom data for gpio control functions */
	muic_data->dev = &i2c->dev;
	muic_data->i2c = i2c;
	muic_data->adc = ADC_OPEN;

#if defined(CONFIG_OF)
	ret = of_sm5504_muic_dt(&i2c->dev, muic_data);
	if (ret < 0) {
		pr_err("%s: no muic dt! ret[%d]\n", __func__, ret);
		goto err_return;
	}
#endif /* CONFIG_OF */

	i2c_set_clientdata(i2c, muic_data);

	ret = sm5504_muic_reg_init(muic_data);
	if (ret) {
		pr_err("%s: failed to init reg\n", __func__);
		goto fail;
	}

	ret = sm5504_muic_irq_init(muic_data);
	if (ret)
		pr_err("%s: failed to init irq(%d)\n", __func__, ret);

	/* initial cable detection */
	sm5504_muic_irq_thread(-1, muic_data);

	return 0;

fail:
	i2c_set_clientdata(i2c, NULL);
err_return:
	return ret;
}

static const struct i2c_device_id sm5504_i2c_id[] = {
	{ "sm5504", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, sm5504_i2c_id);

#if defined(CONFIG_OF)
static struct of_device_id sec_muic_i2c_dt_ids[] = {
	{ .compatible = "sm,sm5504_muic_pogo" },
	{ }
};
MODULE_DEVICE_TABLE(of, sec_muic_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver sm5504_muic_driver = {
	.driver = {
		.name = "sm5504",
		.of_match_table = of_match_ptr(sec_muic_i2c_dt_ids),
	},
	.probe = sm5504_muic_probe,
	/* TODO: suspend resume remove shutdown */
	.id_table	= sm5504_i2c_id,
};

static int __init sm5504_muic_init(void)
{
	return i2c_add_driver(&sm5504_muic_driver);
}
late_initcall(sm5504_muic_init);

static void __exit sm5504_muic_exit(void)
{
	i2c_del_driver(&sm5504_muic_driver);
}
module_exit(sm5504_muic_exit);

MODULE_DESCRIPTION("SM5504 MUIC POGO driver");
MODULE_LICENSE("GPL");
