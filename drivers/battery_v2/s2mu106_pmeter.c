/*
 * s2mu106_pmeter.c - S2MU106 Power Meter Driver
 *
 * Copyright (C) 2016 Samsung Electronics Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "include/s2mu106_pmeter.h"
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>

#define VOLTAGE_9V	8000
#define VOLTAGE_6P9V	6900
#define VOLTAGE_5V	6000

static enum power_supply_property s2mu106_pmeter_props[] = {
};

static const unsigned char enable_bit_data[PM_TYPE_MAX] =
{0x80, 0x40, 0x20, 0x01, 0x08, 0x04, 0x02, 0x80,
0x40, 0x01, 0x10, 0x80};

static int s2mu106_pm_enable(struct s2mu106_pmeter_data *pmeter,
					int mode, enum pm_type type)
{
	u8 addr1 = S2MU106_PM_REQ_BOX_CO1;
	u8 addr2 = S2MU106_PM_REQ_BOX_CO2;
	u8 data1, data2;

	/* Default PM mode = continuous */
	if (mode == REQUEST_RESPONSE_MODE) {
		pr_info ("%s PM mode : Request Response mode (RR)\n", __func__);
		addr1 = S2MU106_PM_REQ_BOX_RR1;
		addr2 = S2MU106_PM_REQ_BOX_RR2;
	}

	s2mu106_read_reg(pmeter->i2c, addr1, &data1);
	s2mu106_read_reg(pmeter->i2c, addr2, &data2);

	switch (type) {
	case PM_TYPE_VCHGIN ... PM_TYPE_VGPADC:
	case PM_TYPE_VCC2:
		data1 |= enable_bit_data[type];
		break;
	case PM_TYPE_ICHGIN:
	case PM_TYPE_IWCIN:
	case PM_TYPE_IOTG:
	case PM_TYPE_ITX:
		data2 |= enable_bit_data[type];
		break;
	default:
		return -EINVAL;
	}

	s2mu106_write_reg(pmeter->i2c, addr1, data1);
	s2mu106_write_reg(pmeter->i2c, addr2, data2);

	pr_info ("%s data1 : 0x%2x, data2 0x%2x\n", __func__, data1, data2);
	return 0;
}

static void s2mu106_pm_factory(struct s2mu106_pmeter_data *pmeter)
{
	pr_info("%s, FACTORY Enter, Powermeter off\n", __func__);
	s2mu106_write_reg(pmeter->i2c, S2MU106_PM_CO_MASK1, 0xFF);
	s2mu106_write_reg(pmeter->i2c, S2MU106_PM_CO_MASK2, 0xFF);
}

static int s2mu106_pm_get_vchgin(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_voltage = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VCHGIN, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VCHGIN, &data2);

	if (data1 < 0 || data2 < 0)
		return -EINVAL;
	
	charge_voltage = ((data1 << 4) | (data2 >> 4)) * 5;
	pr_info ("%s, data1 : 0x%2x, data2 : 0x%2x, voltage = %d\n",
			__func__, data1, data2, charge_voltage);
	return charge_voltage;
}

static int s2mu106_pm_get_vwcin(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_voltage = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VWCIN, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VWCIN, &data2);
	
	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_voltage = ((data1 << 4) | (data2 >> 4)) * 5;
	pr_info ("%s, data1 : 0x%2x, data2 : 0x%2x, voltage = %d\n",
			__func__, data1, data2, charge_voltage);
	return charge_voltage;
}

static int s2mu106_pm_get_vbyp(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_voltage = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VBYP, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VBYP, &data2);
	
	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_voltage = ((data1 << 4) | (data2 >> 4)) * 5;
	pr_info ("%s, data1 : 0x%2x, data2 : 0x%2x, voltage = %d\n",
			__func__, data1, data2, charge_voltage);
	return charge_voltage;
}

static int s2mu106_pm_get_vsysa(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_voltage = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VSYS, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VSYS, &data2);
	
	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_voltage = ((data1 << 4) | (data2 >> 4)) * 25;
	charge_voltage = charge_voltage / 10;

	pr_info ("%s, data1 : 0x%2x, data2 : 0x%2x, voltage = %d\n",
			__func__, data1, data2, charge_voltage);
	return charge_voltage;
}

static int s2mu106_pm_get_vbata(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_voltage = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VBAT, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VBAT, &data2);
	
	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_voltage = ((data1 << 4) | (data2 >> 4)) * 25;
	charge_voltage = charge_voltage / 10;

	pr_info ("%s, data1 : 0x%2x, data2 : 0x%2x, voltage = %d\n",
			__func__, data1, data2, charge_voltage);
	return charge_voltage;
}

static int s2mu106_pm_get_vgpadc(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_voltage = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VGPADC, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VGPADC, &data2);
	
	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_voltage = ((data1 << 4) | (data2 >> 4)) * 25;
	charge_voltage = charge_voltage / 10;

	pr_info ("%s, data1 : 0x%2x, data2 : 0x%2x, voltage = %d\n",
			__func__, data1, data2, charge_voltage);
	return charge_voltage;
}

static int s2mu106_pm_get_vcc1(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_voltage = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VCC1, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VCC1, &data2);
	
	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_voltage = ((data1 << 4) | (data2 >> 4)) * 625;
	charge_voltage = charge_voltage / 1000;

	pr_info ("%s2, data1 : 0x%2x, data2 : 0x%2x, voltage = %d\n",
			__func__, data1, data2, charge_voltage);
	return charge_voltage;
}

static int s2mu106_pm_get_vcc2(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_voltage = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VCC2, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VCC2, &data2);
	
	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_voltage = ((data1 << 4) | (data2 >> 4)) * 625;
	charge_voltage = charge_voltage / 1000;

	pr_info ("%s, data1 : 0x%2x, data2 : 0x%2x, voltage = %d\n",
			__func__, data1, data2, charge_voltage);
	return charge_voltage;
}

static int s2mu106_pm_get_ichgin(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_current = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_ICHGIN, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_ICHGIN, &data2);
	
	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_current = (int)((data1 << 4) | (data2 >> 4));

	pr_info ("%s, data1 : 0x%2x, data2 : 0x%2x, current = %d\n",
			__func__, data1, data2, charge_current);
	return charge_current;
}

static int s2mu106_pm_get_iwcin(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_current = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_IWCIN, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_IWCIN, &data2);
	
	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_current = (int)((data1 << 4) | (data2 >> 4));

	pr_info ("%s, data1 : 0x%2x, data2 : 0x%2x, current = %d\n",
			__func__, data1, data2, charge_current);
	return charge_current;
}

static int s2mu106_pm_get_iotg(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_current = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_IOTG, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_IOTG, &data2);
	
	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_current = (int)((data1 << 4) | (data2 >> 4));

	pr_info ("%s, data1 : 0x%2x, data2 : 0x%2x, current = %d\n",
			__func__, data1, data2, charge_current);
	return charge_current;
}

static int s2mu106_pm_get_itx(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_current = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_ITX, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_ITX, &data2);
	
	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_current = (int)((data1 << 4) | (data2 >> 4));

	pr_info ("%s, data1 : 0x%2x, data2 : 0x%2x, current = %d\n",
			__func__, data1, data2, charge_current);
	return charge_current;
}

static int s2mu106_pm_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2mu106_pmeter_data *pmeter = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_VCHGIN:
		val->intval = s2mu106_pm_get_vchgin(pmeter);
		break;
	case POWER_SUPPLY_PROP_VWCIN:
		val->intval = s2mu106_pm_get_vwcin(pmeter);
		break;
	case POWER_SUPPLY_PROP_VBYP:
		val->intval = s2mu106_pm_get_vbyp(pmeter);
		break;
	case POWER_SUPPLY_PROP_VSYS:
		val->intval = s2mu106_pm_get_vsysa(pmeter);
		break;
	case POWER_SUPPLY_PROP_VBAT:
		val->intval = s2mu106_pm_get_vbata(pmeter);
		break;
	case POWER_SUPPLY_PROP_VGPADC:
		val->intval = s2mu106_pm_get_vgpadc(pmeter);
		break;
	case POWER_SUPPLY_PROP_VCC1:
		val->intval = s2mu106_pm_get_vcc1(pmeter);
		break;
	case POWER_SUPPLY_PROP_VCC2:
		val->intval = s2mu106_pm_get_vcc2(pmeter);
		break;
	case POWER_SUPPLY_PROP_ICHGIN:
		val->intval = s2mu106_pm_get_ichgin(pmeter);
		break;
	case POWER_SUPPLY_PROP_IWCIN:
		val->intval = s2mu106_pm_get_iwcin(pmeter);
		break;
	case POWER_SUPPLY_PROP_IOTG:
		val->intval = s2mu106_pm_get_iotg(pmeter);
		break;
	case POWER_SUPPLY_PROP_ITX:
		val->intval = s2mu106_pm_get_itx(pmeter);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int s2mu106_pm_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mu106_pmeter_data *pmeter = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CO_ENABLE:
		s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, val->intval);
		break;
	case POWER_SUPPLY_PROP_RR_ENABLE:
		s2mu106_pm_enable(pmeter, REQUEST_RESPONSE_MODE, val->intval);
		break;
	case POWER_SUPPLY_PROP_PM_FACTORY:
		s2mu106_pm_factory(pmeter);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static irqreturn_t s2mu106_vchgin_isr(int irq, void *data)
{
	struct s2mu106_pmeter_data *pmeter = data;
	int voltage;
	union power_supply_propval value;

	voltage = s2mu106_pm_get_vchgin(pmeter);
	value.intval = voltage;

	psy_do_property("muic-manager", set,
		POWER_SUPPLY_PROP_PM_VCHGIN, value);

	if (voltage >= VOLTAGE_9V) {
		value.intval = 1;
		psy_do_property("s2mu106-charger", set,
			POWER_SUPPLY_PROP_PM_VCHGIN, value);
	} else if (voltage >= VOLTAGE_6P9V) {
		value.intval = 2;
		psy_do_property("s2mu106-charger", set,
			POWER_SUPPLY_PROP_PM_VCHGIN, value);
	} else if (voltage <= VOLTAGE_5V) {
		value.intval = 0;
		psy_do_property("s2mu106-charger", set,
			POWER_SUPPLY_PROP_PM_VCHGIN, value);
	}

	return IRQ_HANDLED;
}

static const struct of_device_id s2mu106_pmeter_match_table[] = {
	{ .compatible = "samsung,s2mu106-pmeter",},
	{},
};

static void s2mu106_powermeter_initial(struct s2mu106_pmeter_data *pmeter)
{
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_VCHGIN);
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_VCC1);
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_VCC2);
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_VWCIN);
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_IWCIN);
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_ICHGIN);
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_ITX);
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_VSYS);

	/* VCHGIN diff interrupt 2560 mV */
	s2mu106_update_reg(pmeter->i2c, S2MU106_PM_HYST_LEVEL1,
			HYST_LEV_VCHGIN_2560mV << HYST_LEV_VCHGIN_SHIFT,
			HYST_LEV_VCHGIN_MASK);
}

static int s2mu106_pmeter_probe(struct platform_device *pdev)
{
	struct s2mu106_dev *s2mu106 = dev_get_drvdata(pdev->dev.parent);
	struct s2mu106_pmeter_data *pmeter;
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	pr_info("%s:[BATT] S2MU106 Power meter driver probe\n", __func__);
	pmeter = kzalloc(sizeof(struct s2mu106_pmeter_data), GFP_KERNEL);
	if (!pmeter)
		return -ENOMEM;

	pmeter->dev = &pdev->dev;
	pmeter->i2c = s2mu106->muic; // share the i2c slave address with MUIC

	platform_set_drvdata(pdev, pmeter);

	pmeter->psy_pm_desc.name           = "s2mu106_pmeter"; //pmeter->pdata->powermeter_name;
	pmeter->psy_pm_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	pmeter->psy_pm_desc.get_property   = s2mu106_pm_get_property;
	pmeter->psy_pm_desc.set_property   = s2mu106_pm_set_property;
	pmeter->psy_pm_desc.properties     = s2mu106_pmeter_props;
	pmeter->psy_pm_desc.num_properties = ARRAY_SIZE(s2mu106_pmeter_props);

	psy_cfg.drv_data = pmeter;

	pmeter->psy_pm = power_supply_register(&pdev->dev, &pmeter->psy_pm_desc, &psy_cfg);
	if (IS_ERR(pmeter->psy_pm)) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		ret = PTR_ERR(pmeter->psy_pm);
		goto err_power_supply_register;
	}

	pmeter->irq_vchgin = s2mu106->pdata->irq_base + S2MU106_PM_IRQ1_VCHGINUP;
	ret = request_threaded_irq(pmeter->irq_vchgin, NULL,
			s2mu106_vchgin_isr, 0, "vchgin-irq", pmeter);
	if (ret < 0) {
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n",
				__func__, pmeter->irq_vchgin, ret);
	}

	s2mu106_powermeter_initial(pmeter);

	pr_info("%s:[BATT] S2MU106 pmeter driver loaded OK\n", __func__);

	return ret;

err_power_supply_register:
	mutex_destroy(&pmeter->pmeter_mutex);
	kfree(pmeter);

	return ret;
}

static int s2mu106_pmeter_remove(struct platform_device *pdev)
{
	struct s2mu106_pmeter_data *pmeter =
		platform_get_drvdata(pdev);

	kfree(pmeter);
	return 0;
}

#if defined CONFIG_PM
static int s2mu106_pmeter_suspend(struct device *dev)
{
	return 0;
}

static int s2mu106_pmeter_resume(struct device *dev)
{
	return 0;
}
#else
#define s2mu106_pmeter_suspend NULL
#define s2mu106_pmeter_resume NULL
#endif

static void s2mu106_pmeter_shutdown(struct platform_device *pdev)
{
	struct s2mu106_pmeter_data *pmeter = platform_get_drvdata(pdev);
	pr_info("%s: S2MU106 PowerMeter driver shutdown\n", __func__);
	s2mu106_write_reg(pmeter->i2c, S2MU106_PM_REQ_BOX_CO1, 0);
	s2mu106_write_reg(pmeter->i2c, S2MU106_PM_REQ_BOX_CO2, 0);
}

static SIMPLE_DEV_PM_OPS(s2mu106_pmeter_pm_ops, s2mu106_pmeter_suspend,
		s2mu106_pmeter_resume);

static struct platform_driver s2mu106_pmeter_driver = {
	.driver         = {
		.name   = "s2mu106-powermeter",
		.owner  = THIS_MODULE,
		.of_match_table = s2mu106_pmeter_match_table,
		.pm     = &s2mu106_pmeter_pm_ops,
	},
	.probe          = s2mu106_pmeter_probe,
	.remove     = s2mu106_pmeter_remove,
	.shutdown   =   s2mu106_pmeter_shutdown,
};

static int __init s2mu106_pmeter_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&s2mu106_pmeter_driver);

	return ret;
}
subsys_initcall(s2mu106_pmeter_init);

static void __exit s2mu106_pmeter_exit(void)
{
	platform_driver_unregister(&s2mu106_pmeter_driver);
}
module_exit(s2mu106_pmeter_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("PowerMeter driver for S2MU106");
