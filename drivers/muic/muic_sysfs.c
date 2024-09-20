/*
 * driver/muic/muic_sysfs.c - micro USB switch device driver
 *
 * Copyright (C) 2015 Samsung Electronics
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

#include <linux/types.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/muic/muic.h>
#include <linux/sec_class.h>
#include <linux/muic/muic_sysfs.h>
#include <linux/muic/muic_interface.h>
#include <linux/sec_ext.h>

#ifdef CONFIG_BATTERY_SAMSUNG
#include <linux/sec_batt.h>
#if defined(CONFIG_BATTERY_SAMSUNG_LEGO_STYLE)
#include "../battery/common/include/sec_charging_common.h"
#else
#include "../battery_v2/include/sec_charging_common.h"
#endif
#endif

static ssize_t muic_sysfs_show_uart_en(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);
	int ret = 0;

	if (!muic_pdata->is_rustproof) {
		pr_info("%s UART ENABLE\n",  __func__);
		ret = sprintf(buf, "1\n");
	} else {
		pr_info("%s UART DISABLE\n",  __func__);
		ret = sprintf(buf, "0\n");
	}

	return ret;
}

static ssize_t muic_sysfs_set_uart_en(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);

	if (!strncmp(buf, "1", 1))
		muic_pdata->is_rustproof = false;
	else if (!strncmp(buf, "0", 1))
		muic_pdata->is_rustproof = true;
	else
		pr_info("%s invalid value\n",  __func__);

	pr_info("%s uart_en(%d)\n",
		__func__, !muic_pdata->is_rustproof);

	return count;
}

static ssize_t muic_sysfs_show_uart_sel(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);
	const char *mode = "UNKNOWN\n";

	switch (muic_pdata->uart_path) {
	case MUIC_PATH_UART_AP:
		mode = "AP\n";
		break;
	case MUIC_PATH_UART_CP:
		mode = "CP\n";
		break;
	default:
		break;
	}

	pr_info("%s %s", __func__, mode);
	return snprintf(buf, strlen(mode) + 1, "%s", mode);
}

static ssize_t muic_sysfs_set_uart_sel(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);
	struct muic_interface_t *muic_if = muic_pdata->muic_if;

	if (!strncasecmp(buf, "AP", 2))
		muic_pdata->uart_path = MUIC_PATH_UART_AP;
	else if (!strncasecmp(buf, "CP", 2))
		muic_pdata->uart_path = MUIC_PATH_UART_CP;
	else
		pr_warn("%s invalid value\n", __func__);

	if (muic_if->set_switch_to_uart)
		muic_if->set_switch_to_uart(muic_pdata->drv_data);

	pr_info("%s %s\n", __func__, buf);

	return count;
}

static ssize_t muic_sysfs_show_usb_sel(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "PDA\n");
}

static ssize_t muic_sysfs_set_usb_sel(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t muic_sysfs_show_usb_en(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);

	return sprintf(buf, "%s attached_dev = %d\n",
		__func__, muic_pdata->attached_dev);
}

static ssize_t muic_sysfs_set_usb_en(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);
	muic_attached_dev_t new_dev = ATTACHED_DEV_USB_MUIC;

	if (!strncasecmp(buf, "1", 1))
		muic_core_handle_attach(muic_pdata, new_dev, 0, 0);
	else if (!strncasecmp(buf, "0", 1))
		muic_core_handle_detach(muic_pdata);
	else
		pr_info("%s invalid value\n", __func__);

	pr_info("%s attached_dev(%d)\n",
		__func__, muic_pdata->attached_dev);

	return count;
}

static ssize_t muic_sysfs_show_adc(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);
	struct muic_interface_t *muic_if = muic_pdata->muic_if;

	int ret;

#if IS_ENABLED(CONFIG_MUIC_SYSFS_SHOW_REFRESH_ADC)
	int is_afc_muic_ready;
#if IS_ENABLED(CONFIG_MUIC_SUPPORT_CCIC)
	/* TODO: NOTE: There are abnormal operations of rising volatage AFC 9V
	 * by RID enable/disable in the muic_sysfs_refresh_adc functions in the
	 * factory bianary. This is to minimize unnecessary interrupt by RID
	 * enable/disable whenever reading adc sysfs node
	 */
	MUIC_PDATA_FUNC(muic_if->get_afc_ready, muic_pdata->drv_data, &is_afc_muic_ready);
	if (muic_pdata->is_factory_start && muic_pdata->attached_dev == 0) {
		/* No cable detection means RID open */
		ret = ADC_OPEN;
	} else {
#if IS_ENABLED(CONFIG_MUIC_HV)
		if (muic_pdata->is_factory_start && is_afc_muic_ready)
			/* No need to read adc in the middle of afc detection sequences */
			ret = ADC_GND;
		else
#endif
		{
			MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->control_rid_adc,
				muic_pdata->drv_data, MUIC_DISABLE, &ret);
			msleep(50);
			MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->control_rid_adc,
				muic_pdata->drv_data, MUIC_ENABLE, &ret);
			MUIC_PDATA_FUNC(muic_if->get_adc, muic_pdata->drv_data, &ret);
		}
	}
#if IS_ENABLED(CONFIG_MUIC_HV)
	pr_info("%s: factory: %d attached_dev: %d afc ready: %d", __func__,
			muic_pdata->is_factory_start, muic_pdata->attached_dev,
			is_afc_muic_ready);
#endif
#endif
#else
	MUIC_PDATA_FUNC(muic_if->get_adc, muic_pdata->drv_data, &ret);
#endif

	if (ret < 0) {
		pr_err("%s err read adc reg(%d)\n",
			__func__, ret);
		return sprintf(buf, "UNKNOWN\n");
	}

	return sprintf(buf, "%x\n", ret);
}

static ssize_t muic_sysfs_show_usb_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);
	static unsigned long swtich_slot_time;

	if (printk_timed_ratelimit(&swtich_slot_time, 5000))
		pr_info("%s muic_pdata->attached_dev(%d)\n",
			__func__, muic_pdata->attached_dev);

	switch (muic_pdata->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		return sprintf(buf, "USB_STATE_CONFIGURED\n");
	default:
		break;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_MUIC_DEBUG)
static ssize_t muic_sysfs_show_mansw(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);
	struct muic_interface_t *muic_if = muic_pdata->muic_if;
	char mesg[256] = "";
	int ret;

	MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->show_register, muic_pdata->drv_data, mesg, &ret);
	pr_info("%s:%s\n", __func__, mesg);
	return sprintf(buf, "%s\n", mesg);
}

static ssize_t muic_sysfs_show_interrupt_status(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);
	struct muic_interface_t *muic_if = muic_pdata->muic_if;
	char mesg[256] = "";
	int ret;

	MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->show_register, muic_pdata->drv_data, mesg, &ret);
	pr_info("%s:%s\n", __func__, mesg);
	return sprintf(buf, "%s\n", mesg);
}

static ssize_t muic_sysfs_show_registers(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);
	struct muic_interface_t *muic_if = muic_pdata->muic_if;
	char mesg[256] = "";
	int ret;

	MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->show_register, muic_pdata->drv_data, mesg, &ret);
	pr_info("%s:%s\n", __func__, mesg);
	return sprintf(buf, "%s\n", mesg);
}
#endif

#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
static ssize_t muic_sysfs_show_otg_test(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);
	struct muic_interface_t *muic_if = muic_pdata->muic_if;
	char mesg[256] = "";
	int ret;

	MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->show_register, muic_pdata->drv_data, mesg, &ret);
	pr_info("%s:%s\n", __func__, mesg);
	return sprintf(buf, "%s\n", mesg);
}

static ssize_t muic_sysfs_set_otg_test(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	struct muic_interface_t *muic_if = muic_pdata->muic_if;
	int ret =0;
#endif

	pr_info("%s buf:%s\n", __func__, buf);

	/*
	 *	The otg_test is set 0 durring the otg test. Not 1 !!!
	 */

	if (!strncmp(buf, "0", 1)) {
		muic_pdata->is_otg_test = 1;
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->set_otg_reg, muic_pdata->drv_data, 1, &ret);
#endif
	} else if (!strncmp(buf, "1", 1)) {
		muic_pdata->is_otg_test = 0;
#if IS_ENABLED(CONFIG_SEC_FACTORY)
		MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->set_otg_reg, muic_pdata->drv_data, 0, &ret);
#endif
	} else {
		pr_info("%s Wrong command\n", __func__);
		return count;
	}

	return count;
}
#endif

static ssize_t muic_sysfs_show_attached_dev(struct device *dev,
	 struct device_attribute *attr, char *buf)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);
	int mdev = muic_pdata->attached_dev;

	pr_info("%s attached_dev:%d\n", __func__, mdev);

	switch (mdev) {
	case ATTACHED_DEV_NONE_MUIC:
		return sprintf(buf, "No VPS\n");
	case ATTACHED_DEV_USB_MUIC:
		return sprintf(buf, "USB\n");
	case ATTACHED_DEV_CDP_MUIC:
		return sprintf(buf, "CDP\n");
	case ATTACHED_DEV_OTG_MUIC:
		return sprintf(buf, "OTG\n");
	case ATTACHED_DEV_TA_MUIC:
		return sprintf(buf, "TA\n");
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		return sprintf(buf, "JIG UART OFF\n");
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		return sprintf(buf, "JIG UART OFF/VB\n");
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		return sprintf(buf, "JIG UART ON\n");
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		return sprintf(buf, "JIG UART ON/VB\n");
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
		return sprintf(buf, "JIG USB OFF\n");
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		return sprintf(buf, "JIG USB ON\n");
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		return sprintf(buf, "DESKDOCK\n");
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		return sprintf(buf, "AUDIODOCK\n");
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
		return sprintf(buf, "PS CABLE\n");
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		return sprintf(buf, "AFC Charger\n");
	case ATTACHED_DEV_FACTORY_UART_MUIC:
		return sprintf(buf, "FACTORY UART\n");
	default:
		break;
	}

	return sprintf(buf, "UNKNOWN\n");
}

static ssize_t muic_sysfs_show_audio_path(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t muic_sysfs_set_audio_path(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return 0;
}

static ssize_t muic_sysfs_show_apo_factory(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);
	const char *mode;

	/* true: Factory mode, false: not Factory mode */
	if (muic_pdata->is_factory_start)
		mode = "FACTORY_MODE";
	else
		mode = "NOT_FACTORY_MODE";

	pr_info("%s : %s\n",
		__func__, mode);

	return sprintf(buf, "%s\n", mode);
}

static ssize_t muic_sysfs_set_apo_factory(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);

	/* "FACTORY_START": factory mode */
	if (!strncmp(buf, "FACTORY_START", 13))
		muic_pdata->is_factory_start = true;
	else
		pr_info("%s Wrong command\n",  __func__);

	return count;
}

static ssize_t muic_show_vbus_value(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);
	struct muic_interface_t *muic_if = muic_pdata->muic_if;
	int val = 0;

	MUIC_PDATA_FUNC(muic_if->get_vbus_voltage, muic_pdata->drv_data, &val);
	
	switch(val) {
	case 0 ... 3:
		val = 0;
		break;
	case 4 ... 6:
		val = 5;
		break;
	case 7 ... 9:
		val = 9;
		break;
	}
	return sprintf(buf, "%d\n", val);
}

#if IS_ENABLED(CONFIG_MUIC_HV)
static ssize_t muic_sysfs_show_afc_disable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);

	if (muic_pdata->afc_disable) {
		pr_info("%s AFC DISABLE\n", __func__);
		return sprintf(buf, "1\n");
	}

	pr_info("%s AFC ENABLE", __func__);
	return sprintf(buf, "0\n");
}

static ssize_t muic_sysfs_set_afc_disable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct muic_platform_data *pdata = dev_get_drvdata(dev);
	struct muic_interface_t *muic_if = pdata->muic_if;
	bool curr_val = pdata->afc_disable;
	int ret = 0;
	int param_val = 0;
#ifdef CONFIG_BATTERY_SAMSUNG
	union power_supply_propval psy_val;
#endif
	if (!strncasecmp(buf, "1", 1)) {
		pdata->afc_disable = true;
	} else if (!strncasecmp(buf, "0", 1)) {
		pdata->afc_disable = false;
	} else {
		pr_warn("%s invalid value\n", __func__);
		return -EINVAL;
	}

#if IS_ENABLED(CONFIG_MUIC_MANAGER)
	param_val = pdata->afc_disable ? '1' : '0';
#endif

	ret = sec_set_param(CM_OFFSET + 1, (char)param_val);
	if (ret < 0) {
		pr_info("%s:set_param failed - %02x:%02x(%d)\n", __func__,
			param_val, curr_val, ret);

		pdata->afc_disable = curr_val;
		return -EIO;
	}

#ifdef CONFIG_BATTERY_SAMSUNG
	psy_val.intval = param_val;
	psy_do_property("battery", set, POWER_SUPPLY_EXT_PROP_HV_DISABLE, psy_val);
#endif
	pr_info("%s afc_disable(%d)\n", __func__, pdata->afc_disable);
	if (curr_val != pdata->afc_disable)
		MUIC_PDATA_VOID_FUNC(muic_if->set_chgtype_usrcmd, pdata->drv_data);

	return count;
}

#if IS_ENABLED(CONFIG_HV_MUIC_VOLTAGE_CTRL)
static ssize_t muic_store_afc_set_voltage(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct muic_platform_data *muic_pdata = dev_get_drvdata(dev);
	struct muic_interface_t *muic_if = muic_pdata->muic_if;
	int ret = 0;

	if (!strncasecmp(buf, "5V", 2)) {
		MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->set_afc_voltage, muic_pdata->drv_data, 5, &ret);
	} else if (!strncasecmp(buf, "9V", 2)) {
		MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->set_afc_voltage, muic_pdata->drv_data, 9, &ret);
	} else {
		pr_warn("%s invalid value : %s\n", __func__, buf);
	}

	return count;
}
#endif /* CONFIG_HV_MUIC_VOLTAGE_CTRL */
#endif /* CONFIG_MUIC_HV */

#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
static ssize_t hiccup_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct muic_platform_data *pdata = dev_get_drvdata(dev);
	struct muic_interface_t *muic_if = pdata->muic_if;
	int ret;

	MUIC_PDATA_FUNC(muic_if->get_hiccup_mode,
					pdata->drv_data, &ret);

	if (ret)
		return sprintf(buf, "ENABLE\n");
	else
		return sprintf(buf, "DISABLE\n");
}

static ssize_t hiccup_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct muic_platform_data *pdata = dev_get_drvdata(dev);
	struct muic_interface_t *muic_if = pdata->muic_if;
	int ret;

	if (!strncasecmp(buf, "DISABLE", 7)) {
		MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->set_hiccup_mode,
					pdata->drv_data, MUIC_DISABLE, &ret);
		MUIC_PDATA_FUNC(muic_if->set_com_to_open,
					pdata->drv_data, &ret);
	} else
		pr_warn("%s invalid com : %s\n", __func__, buf);

	return count;
}
#endif /* CONFIG_HICCUP_CHARGER */

static DEVICE_ATTR(uart_en, 0664, muic_sysfs_show_uart_en,
					muic_sysfs_set_uart_en);
static DEVICE_ATTR(uart_sel, 0664, muic_sysfs_show_uart_sel,
					muic_sysfs_set_uart_sel);
static DEVICE_ATTR(usb_sel, 0664, muic_sysfs_show_usb_sel,
					muic_sysfs_set_usb_sel);
static DEVICE_ATTR(adc, 0664, muic_sysfs_show_adc, NULL);

#if IS_ENABLED(DEBUG_MUIC)
static DEVICE_ATTR(mansw, 0444, muic_sysfs_show_mansw, NULL);
static DEVICE_ATTR(dump_registers, 0444, muic_sysfs_show_registers, NULL);
static DEVICE_ATTR(int_status, 0444, muic_sysfs_show_interrupt_status, NULL);
#endif
static DEVICE_ATTR(usb_state, 0444, muic_sysfs_show_usb_state, NULL);
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
static DEVICE_ATTR(otg_test, 0664,
		muic_sysfs_show_otg_test, muic_sysfs_set_otg_test);
#endif
static DEVICE_ATTR(attached_dev, 0444, muic_sysfs_show_attached_dev, NULL);
static DEVICE_ATTR(audio_path, 0664,
		muic_sysfs_show_audio_path, muic_sysfs_set_audio_path);
static DEVICE_ATTR(apo_factory, 0664,
		muic_sysfs_show_apo_factory,
		muic_sysfs_set_apo_factory);
static DEVICE_ATTR(usb_en, 0664,
		muic_sysfs_show_usb_en,
		muic_sysfs_set_usb_en);
static DEVICE_ATTR(vbus_value, 0444, muic_show_vbus_value, NULL);
#if IS_ENABLED(CONFIG_MUIC_HV)
static DEVICE_ATTR(afc_disable, 0664,
		muic_sysfs_show_afc_disable, muic_sysfs_set_afc_disable);
#if IS_ENABLED(CONFIG_HV_MUIC_VOLTAGE_CTRL)
static DEVICE_ATTR(afc_set_voltage, 0220,
		NULL, muic_store_afc_set_voltage);
#endif /* CONFIG_HV_MUIC_VOLTAGE_CTRL */
#endif /* CONFIG_MUIC_HV */
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
static DEVICE_ATTR_RW(hiccup);
#endif /* CONFIG_HICCUP_CHARGER */

static struct attribute *muic_sysfs_attributes[] = {
	&dev_attr_uart_en.attr,
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_sel.attr,
	&dev_attr_adc.attr,
#if IS_ENABLED(DEBUG_MUIC)
	&dev_attr_mansw.attr,
	&dev_attr_dump_registers.attr,
	&dev_attr_int_status.attr,
#endif
	&dev_attr_usb_state.attr,
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
	&dev_attr_otg_test.attr,
#endif
	&dev_attr_attached_dev.attr,
	&dev_attr_audio_path.attr,
	&dev_attr_apo_factory.attr,
	&dev_attr_usb_en.attr,
	&dev_attr_vbus_value.attr,
#if IS_ENABLED(CONFIG_MUIC_HV)
	&dev_attr_afc_disable.attr,
#if IS_ENABLED(CONFIG_HV_MUIC_VOLTAGE_CTRL)
	&dev_attr_afc_set_voltage.attr,
#endif /* CONFIG_HV_MUIC_VOLTAGE_CTRL */
#endif /* CONFIG_MUIC_HV */
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
	&dev_attr_hiccup.attr,
#endif /* CONFIG_HICCUP_CHARGER */
	NULL
};

static const struct attribute_group muic_sysfs_group = {
	.attrs = muic_sysfs_attributes,
};

int muic_sysfs_init(struct muic_platform_data *muic_pdata)
{
	int ret;
	/* create sysfs group */
#if (0)  //IS_ENABLED(CONFIG_SEC_FACTORY)
	muic_pdata->switch_device = sec_device_find("switch");
#endif
#ifdef CONFIG_TEMP
	mutex_init(&muic_pdata->sysfs_mutex);
#endif
	if (muic_pdata->switch_device == NULL)
		muic_pdata->switch_device = sec_device_create(NULL, "switch");

	if (IS_ERR(muic_pdata->switch_device)) {
		pr_err("%s Failed to create device(switch)!\n", __func__);
		ret = -ENODEV;
		return ret;
	}

	ret = sysfs_create_group(&muic_pdata->switch_device->kobj, &muic_sysfs_group);
	if (ret) {
		pr_err("failed to create sysfs\n");
		return ret;
	}
	dev_set_drvdata(muic_pdata->switch_device, muic_pdata);

	return ret;
}

void muic_sysfs_deinit(struct muic_platform_data *muic_pdata)
{
	if (muic_pdata->switch_device)
		sysfs_remove_group(&muic_pdata->switch_device->kobj, &muic_sysfs_group);
}
