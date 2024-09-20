/*
 * muic_task.c
 *
 * Copyright (C) 2014 Samsung Electronics
 * Thomas Ryu <smilesr.ryu@samsung.com>
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

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/host_notify.h>

#include <linux/muic/muic.h>
#include <linux/mfd/muic_mfd.h>
#if defined(CONFIG_SEC_DEBUG)
#include <linux/mfd/max77865.h>
#include <linux/mfd/max77865-private.h>
#endif

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif /* CONFIG_VBUS_NOTIFIER */

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#include "muic-internal.h"
#include "muic_apis.h"
#include "muic_state.h"
#include "muic_vps.h"
#include "muic_i2c.h"
#include "muic_sysfs.h"
#include "muic_debug.h"
#include "muic_dt.h"
#include "muic_regmap.h"
#include "muic_regmap_max77865.h"
#include "muic_coagent.h"
#if defined(CONFIG_BATTERY_SAMSUNG_LEGO_STYLE)
#include "../../battery/common/include/sec_charging_common.h"
#else
#include "../../battery_v2/include/sec_charging_common.h"
#endif

#if defined(CONFIG_MUIC_HV_MAX77865)
#include "muic_hv.h"
#include "muic_hv_max77865.h"
#endif

#if defined(CONFIG_MUIC_SUPPORT_CCIC)
#include "muic_ccic.h"
#endif

#if defined(CONFIG_USB_EXTERNAL_NOTIFY)
#include "muic_usb.h"
#endif

extern struct muic_platform_data muic_pdata;

extern void muic_send_dock_intent(int type);

static bool muic_online = false;

/*
 * 0 : normal, 1: abnormal
 * This flag is set by USB for an abnormal HMT and
 * cleared by MUIC on detachment.
 */
static int muic_hmt_status;

bool muic_is_online(void)
{
	return muic_online;
}

void muic_set_hmt_status(int status)
{
	
	pr_info("%s:%s hmt_status=[%s]\n", MUIC_DEV_NAME, __func__,
		status ? "abnormal" : "normal");

	muic_hmt_status = status;

	if (status)
		muic_send_dock_intent(MUIC_DOCK_ABNORMAL);
}

#if 0
static int muic_get_hmt_status(void)
{
	return muic_hmt_status;
}
#endif

#if defined(CONFIG_VBUS_NOTIFIER)
static void muic_handle_vbus(muic_data_t *pmuic)
{
	vbus_status_t status = pmuic->vps.t.vbvolt ? STATUS_VBUS_HIGH : STATUS_VBUS_LOW;

	pr_info("%s:%s <%d>\n", MUIC_DEV_NAME, __func__, pmuic->vps.t.vbvolt);

#if 0
	if (pmuic->attached_dev == ATTACHED_DEV_HMT_MUIC) {
		if (muic_get_hmt_status()) {
			pr_info("%s:%s Abnormal HMT -> VBUS_UNKNOWN Noti.\n",
							MUIC_DEV_NAME, __func__);
			status = STATUS_VBUS_UNKNOWN;
		}
	}
#endif

	vbus_notifier_handle(status);
}
#else
static void muic_handle_vbus(muic_data_t *pmuic)
{
	pr_info("%s:%s <%d> Not implemented.\n", MUIC_DEV_NAME,
					__func__, pmuic->vps.t.vbvolt);
}
#endif

static void max77865_restore(muic_data_t *pmuic)
{
	struct i2c_client *i2c = pmuic->i2c;
	int ccdet;

	muic_i2c_write_byte(i2c, MAX77865_REG_INTMASK_MAIN, INTMASK_MAIN_RESTORE);
	muic_i2c_write_byte(i2c, MAX77865_REG_INTMASK_BC, INTMASK_BC_RESTORE);
	muic_i2c_write_byte(i2c, MAX77865_REG_INTMASK_FC, INTMASK_FC_RESTORE);
	muic_i2c_write_byte(i2c, MAX77865_REG_INTMASK_GP, INTMASK_GP_RESTORE);

	muic_i2c_write_byte(i2c, MAX77865_REG_CCDET, CCDET_RESTORE);
	ccdet = muic_i2c_read_byte(i2c, MAX77865_REG_CCDET);

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	muic_print_reg_dump(pmuic);
	pr_info("CCDET[0x%02x]\n", ccdet);

	pmuic->attached_dev = ATTACHED_DEV_NONE_MUIC;
}

static irqreturn_t max77865_muic_irq_handler(muic_data_t *pmuic, int irq)
{
	struct i2c_client *i2c = pmuic->i2c;
	int irq_main, irq_bc, irq_fc, irq_gp, irq_vbus, irq_ovp, irq_dcdtmr, irq_reset;
	union power_supply_propval val;
#if defined(CONFIG_MUIC_HV_MAX77865)
	int irq_hv, irq_hv1, irq_hv2, irq_hv3, irq_hv4;
#endif

	pr_info("%s:%s irq(%d)\n", pmuic->chip_name, __func__, irq);

	/* read and clear interrupt status bits */
	irq_main = muic_i2c_read_byte(i2c, MAX77865_REG_INT_MAIN);
	irq_bc = muic_i2c_read_byte(i2c, MAX77865_REG_INT_BC);
	irq_fc = muic_i2c_read_byte(i2c, MAX77865_REG_INT_FC);
	irq_gp = muic_i2c_read_byte(i2c, MAX77865_REG_INT_GP);

	muic_i2c_write_byte(i2c, MAX77865_REG_INT_MAIN, INT_MAIN_CLEAR);
	muic_i2c_write_byte(i2c, MAX77865_REG_INT_BC, INT_BC_CLEAR);
	muic_i2c_write_byte(i2c, MAX77865_REG_INT_FC, INT_FC_CLEAR);
	muic_i2c_write_byte(i2c, MAX77865_REG_INT_GP, INT_GP_CLEAR);

	if ((irq_main < 0) || (irq_bc < 0) || (irq_fc < 0) || (irq_gp < 0)) {
		pr_err("%s: err read interrupt status [1:0x%x, 2:0x%x, 3:0x%x, 4:0x%x]\n",
							__func__, irq_main, irq_bc, irq_fc, irq_gp);
		return INT_REQ_DISCARD;
	}

	pr_info("%s:%s intr[1:0x%x, 2:0x%x, 3:0x%x, 4:0x%x]\n",
					pmuic->chip_name, __func__, irq_main, irq_bc, irq_fc, irq_gp);

	if (irq < 0)
		return INT_REQ_DONE;

	/* W/A for irq value none when irq occurred [Except when driver init] */
	if ((irq_main == 0) && (irq_bc == 0) && (irq_fc == 0) && (irq_gp == 0)) {
		pr_info("%s: irq occurred. but nothing irq information.\n", __func__);
		return INT_REQ_NONE;
	}

	irq_vbus = irq_bc & MAX77865_VBUS_MASK;
	irq_ovp = irq_bc & MAX77865_OVP_MASK;
	irq_dcdtmr = irq_bc & MAX77865_DCDTMR_MASK;
	irq_reset = irq_gp & MAX77865_RESET_MASK;

	if (irq_reset) {
		pr_info("%s RESET interrupt occurred\n", __func__);
		max77865_restore(pmuic);
		val.intval = true;
		psy_do_property("max77865-charger", set, POWER_SUPPLY_EXT_PROP_SURGE, val);
		return INT_REQ_DONE;
	}

	/* VBUS Noti */
	if (irq_vbus) {
		pr_info("%s VBUS interrupt occurred\n", __func__);
		return INT_REQ_DONE_VB;
	}

	if (irq_ovp) {
		pr_info("%s OVP interrupt occurred\n", __func__);
		return INT_REQ_OVP;
	}

	if (irq_dcdtmr) {
		pr_info("%s DCDTMR interrupt occurred. RID[%d]\n", __func__, pmuic->rid);
		if (pmuic->rid != RID_619K)
			pmuic->is_dcdtmr_intr = true;
	}

#if defined(CONFIG_MUIC_HV_MAX77865)
	irq_hv1 = irq_bc & MAX77865_VDNMON_MASK;
	irq_hv2 = irq_fc & MAX77865_MRXRDY_MASK;
	irq_hv3 = irq_fc & MAX77865_MPNACK_MASK;
	irq_hv4 = irq_gp & MAX77865_VBADC_MASK;
	irq_hv = irq_hv1 + irq_hv4;

	if ((irq_hv > 0) || (irq_fc > 0)) {
		pr_info("%s:%s HV intr: vdnmon(%d), mrxrdy(%d), mpnack(%d), vbadc(%d)\n",
				pmuic->chip_name, __func__, irq_hv1, irq_hv2, irq_hv3, irq_hv4);
		if (irq_hv1)
			pmuic->phv->irq = MUIC_AFC_IRQ_VDNMON;
		else if (irq_hv2)
			pmuic->phv->irq = MUIC_AFC_IRQ_MRXRDY;
		else if (irq_hv3)
			pmuic->phv->irq = MUIC_AFC_IRQ_MPNACK;
		else if (irq_hv4)
			pmuic->phv->irq = MUIC_AFC_IRQ_VBADC;

		return INT_REQ_DONE_HV;
	}
#endif

	return INT_REQ_DONE;
}

static irqreturn_t muic_irq_thread(int irq, void *data)
{
	muic_data_t *pmuic = data;
	int irq_ret;

	mutex_lock(&pmuic->muic_mutex);

	irq_ret = max77865_muic_irq_handler(pmuic, irq);
	switch (irq_ret) {
	case INT_REQ_DONE_VB:
		muic_detect_dev(pmuic, irq);
		muic_handle_vbus(pmuic);
		break;
	case INT_REQ_DONE:
		muic_detect_dev(pmuic, irq);
		break;
#if defined(CONFIG_MUIC_HV_MAX77865)
	case INT_REQ_DONE_HV:
		if (pmuic->phv->is_muic_ready == false)
			pr_info("%s:%s MUIC is not ready, just return\n", MUIC_HV_DEV_NAME, __func__);
		else if (pmuic->phv->is_afc_muic_ready == false)
			pr_info("%s:%s not ready yet(afc_muic_ready[%c])\n",
				MUIC_HV_DEV_NAME, __func__, (pmuic->phv->is_afc_muic_ready ? 'T' : 'F'));
		else if (pmuic->phv->is_charger_ready == false && irq != pmuic->phv->irq_vdnmon)
			pr_info("%s:%s not ready yet(charger_ready[%c])\n",
				MUIC_HV_DEV_NAME, __func__, (pmuic->phv->is_charger_ready ? 'T' : 'F'));
		else if (pmuic->pdata->afc_disable)
			pr_info("%s:%s AFC disable by USER (afc_disable[%c]\n",
				MUIC_HV_DEV_NAME, __func__, (pmuic->pdata->afc_disable ? 'T' : 'F'));
#if defined(CONFIG_MUIC_SUPPORT_CCIC)
		else if (pmuic->afc_water_disable)
			pr_info("%s:%s AFC disable by WATER (afc_water_disable[%c]\n",
				MUIC_HV_DEV_NAME, __func__, (pmuic->afc_water_disable ? 'T' : 'F'));
#endif
		else
			max77865_hv_muic_detect_dev(pmuic->phv, pmuic->phv->irq);
		break;
#endif
	case INT_REQ_NONE:
	case INT_REQ_OVP:
	default:
		break;
	}

	mutex_unlock(&pmuic->muic_mutex);

	return IRQ_HANDLED;
}

static void muic_init_detect(muic_data_t *pmuic)
{
	pr_info("%s:%s\n", pmuic->chip_name, __func__);

	pmuic->is_muic_ready = true;
	muic_irq_thread(-1, pmuic);
}

static int muic_irq_init(muic_data_t *pmuic)
{
	struct i2c_client *i2c = pmuic->i2c;
	struct muic_platform_data *pdata = pmuic->pdata;
	int ret = 0;

	pr_info("%s:%s\n", pmuic->chip_name, __func__);

	if (!pdata->irq_gpio) {
		pr_warn("%s:%s No interrupt specified\n", pmuic->chip_name,
				__func__);
		return -ENXIO;
	}

	i2c->irq = gpio_to_irq(pdata->irq_gpio);

	if (i2c->irq) {
		ret = request_threaded_irq(i2c->irq, NULL,
				muic_irq_thread,
				(IRQF_TRIGGER_LOW | IRQF_ONESHOT),
				"muic-irq", pmuic);
		if (ret < 0) {
			pr_err("%s:%s failed to reqeust IRQ(%d)\n",
						pmuic->chip_name, __func__, i2c->irq);
			return ret;
		}

		ret = enable_irq_wake(i2c->irq);
		if (ret < 0)
			pr_err("%s:%s failed to enable wakeup src\n",
						pmuic->chip_name, __func__);
	}

	pmuic->irqs.irq_val = pdata->irq_gpio;
#if defined(CONFIG_MUIC_HV_MAX77865)
	pmuic->phv->irq_vdnmon = MUIC_AFC_IRQ_VDNMON;
	pmuic->phv->irq_mrxrdy = MUIC_AFC_IRQ_MRXRDY;
	pmuic->phv->irq_mpnack = MUIC_AFC_IRQ_MPNACK;
	pmuic->phv->irq_vbadc = MUIC_AFC_IRQ_VBADC;
#endif

	return ret;
}

static int muic_probe(struct platform_device *pdev)
{
	struct muic_mfd_dev *pmfd = dev_get_drvdata(pdev->dev.parent);
//	struct muic_mfd_platform_data *mfd_pdata = dev_get_platdata(pmfd->dev);
	struct muic_platform_data *pdata = &muic_pdata;
	muic_data_t *pmuic;
	struct regmap_desc *pdesc = NULL;
	struct regmap_ops *pops = NULL;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	pmuic = kzalloc(sizeof(muic_data_t), GFP_KERNEL);
	if (!pmuic) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}


#if defined(CONFIG_OF)
	ret = of_muic_dt(pmfd->i2c, pdata, pmuic);
	if (ret < 0) {
		pr_err("%s:%s Failed to get device of_node \n",
				MUIC_DEV_NAME, __func__);
		goto err_io;
	}

	of_update_supported_list(pmfd->i2c, pdata);
	vps_show_table();

#endif /* CONFIG_OF */
	if (!pdata) {
		pr_err("%s: failed to get i2c platform data\n", __func__);
		ret = -ENODEV;
		goto err_io;
	}

	mutex_init(&pmuic->muic_mutex);

	pmuic->pdata = pdata;

	pr_info("%s: muic irq_gpio = %d\n", __func__, pdata->irq_gpio);
	pr_info("%s: muic i2c client %s at 0x%02x\n", __func__, pmfd->muic->name, pmfd->muic->addr);
	pr_info("%s: mfd i2c client %s at 0x%02x\n", __func__, pmfd->i2c->name, pmfd->i2c->addr);
	pr_info("%s: fuel i2c client %s at 0x%02x\n", __func__, pmfd->fuel->name, pmfd->fuel->addr);
	pr_info("%s: chg  i2c client %s at 0x%02x\n", __func__, pmfd->chg->name, pmfd->chg->addr);
	pmuic->i2c = pmfd->muic;

	pmuic->is_factory_start = false;
	pmuic->is_otg_test = false;
	pmuic->attached_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	pmuic->is_gamepad = false;
#if defined(CONFIG_MUIC_TEST_FUNC)
	pmuic->usb_to_ta_state = false;
#endif
	pmuic->is_dcdtmr_intr = false;

	if(!strcmp(pmuic->chip_name,"max,max77865"))
		pmuic->vps_table = VPS_TYPE_TABLE;
	else
		pmuic->vps_table = VPS_TYPE_SCATTERED;

	pr_info("%s: VPS_TYPE=%d\n", __func__, pmuic->vps_table);

	if (!pdata->set_gpio_uart_sel) {
		if (pmuic->gpio_uart_sel) {
			pr_info("%s: gpio_uart_sel registered.\n", __func__);
			pdata->set_gpio_uart_sel = muic_set_gpio_uart_sel;
		} else
			pr_info("%s: gpio_uart_sel is not supported.\n", __func__);
	}

	if (pmuic->pdata->init_gpio_cb) {
		ret = pmuic->pdata->init_gpio_cb(get_switch_sel());
		if (ret) {
			pr_err("%s: failed to init gpio(%d)\n", __func__, ret);
		goto fail_init_gpio;
		}
	}

	if (pmuic->pdata->init_switch_dev_cb)
		pmuic->pdata->init_switch_dev_cb();

	pr_info("  switch_sel : 0x%04x\n", get_switch_sel());

	if (!(get_switch_sel() & SWITCH_SEL_RUSTPROOF_MASK)) {
		pr_info("  Enable rustproof mode\n");
		pmuic->is_rustproof = true;
	} else {
		pr_info("  Disable rustproof mode\n");
		pmuic->is_rustproof = false;
	}

	if (get_afc_mode() == CH_MODE_AFC_DISABLE_VAL) {
		pr_info("  AFC mode disabled\n");
		pmuic->pdata->afc_disable = true;
	} else {
		pr_info("  AFC mode enabled\n");
		pmuic->pdata->afc_disable = false;
	}

	/* Register chipset register map. */
	muic_register_regmap(&pdesc, pmuic);
	pdesc->muic = pmuic;
	pops = pdesc->regmapops;
	pmuic->regmapdesc = pdesc;

#if defined(CONFIG_MUIC_SUPPORT_CCIC)
	pmuic->pdata->opmode = get_ccic_info() & 0x0F;
	pmuic->afc_water_disable = false;
	pmuic->is_ccic_attach = false;
	pmuic->is_ccic_afc_enable = false;
#endif
	/* set switch device's driver_data */
	dev_set_drvdata(switch_device, pmuic);
	dev_set_drvdata(&pdev->dev, pmuic);
	/* create sysfs group */
	ret = sysfs_create_group(&switch_device->kobj, &muic_sysfs_group);
	if (ret) {
		pr_err("%s: failed to create sm5703 muic attribute group\n",
			__func__);
		goto fail;
	}

	ret = pops->revision(pdesc);
	if (ret) {
		pr_err("%s: failed to init muic rev register(%d)\n", __func__,
			ret);
		goto fail;
	}

	ret = muic_reg_init(pmuic);
	if (ret) {
		pr_err("%s: failed to init muic register(%d)\n", __func__, ret);
		goto fail;
	}

	pops->update(pdesc);
	pops->show(pdesc);

#if defined(CONFIG_MUIC_HV_MAX77865)
	hv_initialize(pmuic, &pmuic->phv);
	of_muic_hv_dt(pmuic);
	hv_configure_AFC(pmuic->phv);
	/* set MUIC afc voltage switching function */
	pmuic->pdata->muic_afc_set_voltage_cb = max77865_muic_afc_set_voltage;
#endif

	coagent_update_ctx(pmuic);

	muic_irq_init(pmuic);

	/* initial cable detection */
	muic_init_detect(pmuic);

#if defined(CONFIG_MUIC_HV_MAX77865)
	pmuic->phv->is_muic_ready = true;

	printk("%s: is_afc_muic_ready=%d\n", __func__, pmuic->phv->is_afc_muic_ready);
	if (pmuic->phv->is_afc_muic_ready)
		max77865_hv_muic_init_detect(pmuic->phv);
#endif

#ifdef DEBUG_MUIC
	INIT_DELAYED_WORK(&pmuic->usb_work, muic_show_debug_info);
	schedule_delayed_work(&pmuic->usb_work, msecs_to_jiffies(10000));
#endif


#if defined(CONFIG_MUIC_SUPPORT_CCIC)
	muic_is_ccic_supported_jig(pmuic, pmuic->attached_dev);

	if (pmuic->pdata->opmode & OPMODE_CCIC)
		muic_register_ccic_notifier(pmuic);
	else
		pr_info("%s: OPMODE_MUIC. CCIC is not used.\n", __func__);
#endif

#if defined(CONFIG_USB_EXTERNAL_NOTIFY)
	muic_register_usb_notifier(pmuic);
#endif
	muic_online =  true;

	return 0;

fail:
	if (pmuic->pdata->cleanup_switch_dev_cb)
		pmuic->pdata->cleanup_switch_dev_cb();
	sysfs_remove_group(&switch_device->kobj, &muic_sysfs_group);
	mutex_unlock(&pmuic->muic_mutex);
	mutex_destroy(&pmuic->muic_mutex);
fail_init_gpio:

err_io:
	kfree(pmuic);
err_return:
	return ret;
}

static int muic_remove(struct platform_device *pdev)
{
	muic_data_t *pmuic = platform_get_drvdata(pdev);
	sysfs_remove_group(&switch_device->kobj, &muic_sysfs_group);

	if (pmuic) {
		pr_info("%s:%s\n", pmuic->chip_name, __func__);

#if defined(CONFIG_MUIC_HV_MAX77865)
		max77865_hv_muic_remove(pmuic->phv);
#endif
		cancel_delayed_work(&pmuic->usb_work);
		disable_irq_wake(pmuic->i2c->irq);
		free_irq(pmuic->i2c->irq, pmuic);

		if (pmuic->pdata->cleanup_switch_dev_cb)
			pmuic->pdata->cleanup_switch_dev_cb();

#if defined(CONFIG_USB_EXTERNAL_NOTIFY)
		muic_unregister_usb_notifier(pmuic);
#endif
		mutex_destroy(&pmuic->muic_mutex);
		i2c_set_clientdata(pmuic->i2c, NULL);
		kfree(pmuic);
	}
	muic_online = false;
	return 0;
}

#if defined(CONFIG_OF)
static struct of_device_id muic_i2c_dt_ids[] = {
        { .compatible = "muic-universal" },
        { },
};
MODULE_DEVICE_TABLE(of, muic_i2c_dt_ids);
#endif /* CONFIG_OF */

static void muic_shutdown(struct platform_device *pdev)
{
	muic_data_t *pmuic = platform_get_drvdata(pdev);
	int ret;
	if(pmuic == NULL)
	{
		printk("sabin pmuic is NULL\n");
		return;
	}
	printk("sabin muic_shutdown start\n");
	pr_info("%s:%s\n", pmuic->chip_name, __func__);
	if (!pmuic->i2c) {
		pr_err("%s:%s no muic i2c client\n", pmuic->chip_name, __func__);
		return;
	}

	free_irq(pmuic->i2c->irq, pmuic);

#if defined(CONFIG_MUIC_HV_MAX77865)
	max77865_hv_muic_remove(pmuic->phv);
#endif

	if (cancel_delayed_work(&pmuic->usb_work))
		pr_info("%s: usb_work canceled.\n", __func__);
	else
		pr_info("%s: usb_work is not pending.\n", __func__);

	pr_info("%s:%s open D+,D-\n", pmuic->chip_name, __func__);
	ret = com_to_open_with_vbus(pmuic);
	if (ret < 0)
		pr_err("%s:%s fail to open mansw1 reg\n", pmuic->chip_name,
				__func__);

	/* set auto sw mode before shutdown to make sure device goes into */
	/* LPM charging when TA or USB is connected during off state */
	pr_info("%s:%s muic auto detection enable\n", pmuic->chip_name, __func__);
	set_switch_mode(pmuic,SWMODE_AUTO);

	if (pmuic->pdata && pmuic->pdata->cleanup_switch_dev_cb)
		pmuic->pdata->cleanup_switch_dev_cb();

	muic_online =  false;
	pr_info("%s:%s -\n", MUIC_DEV_NAME, __func__);
}

#if defined(CONFIG_PM)
static int muic_suspend(struct device *dev)
{
	muic_data_t *pmuic = dev_get_drvdata(dev);

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	cancel_delayed_work(&pmuic->usb_work);

	if (device_may_wakeup(dev))
		enable_irq_wake(pmuic->i2c->irq);

	disable_irq(pmuic->i2c->irq);

	return 0;
}

static int muic_resume(struct device *dev)
{
	muic_data_t *pmuic = dev_get_drvdata(dev);

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	schedule_delayed_work(&pmuic->usb_work, msecs_to_jiffies(1000));

	if (device_may_wakeup(dev))
		disable_irq_wake(pmuic->i2c->irq);

	enable_irq(pmuic->i2c->irq);

	return 0;
}

const struct dev_pm_ops muic_pm = {
	.suspend = muic_suspend,
	.resume = muic_resume,
};
#endif /* CONFIG_PM */

static struct platform_driver muic_driver = {
	.driver		= {
		.name	= MUIC_DEV_NAME,
		.owner	= THIS_MODULE,
#if defined(CONFIG_OF)
		.of_match_table	= muic_i2c_dt_ids,
#endif /* CONFIG_OF */
#if defined(CONFIG_PM)
		.pm = &muic_pm,
#endif /* CONFIG_PM */
	},
	.probe		= muic_probe,
	.remove		= muic_remove,
	.shutdown	= muic_shutdown,
};

static int __init muic_init(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	return platform_driver_register(&muic_driver);
}
module_init(muic_init);

static void muic_exit(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	platform_driver_unregister(&muic_driver);
}
module_exit(muic_exit);

MODULE_DESCRIPTION("MUIC driver");
MODULE_AUTHOR("<smilesr.ryu@samsung.com>");
MODULE_LICENSE("GPL");
