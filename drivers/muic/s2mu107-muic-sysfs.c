/*
 * driver/muic/s2mu107-muic_sysfs.c - S2MU107 micro USB switch device driver
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

#include <linux/muic/muic_core.h>
#include <linux/muic/s2mu107-muic.h>
#include <linux/muic/s2mu107-muic-hv.h>
#include <linux/mfd/samsung/s2mu107.h>
#include <linux/sec_class.h>

static ssize_t s2mu107_muic_show_uart_en(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *muic_pdata = muic_data->pdata;
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

static ssize_t s2mu107_muic_set_uart_en(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *muic_pdata = muic_data->pdata;

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

static ssize_t s2mu107_muic_show_uart_sel(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	const char *mode = "UNKNOWN\n";

	switch (pdata->uart_path) {
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

static ssize_t s2mu107_muic_set_uart_sel(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	struct muic_interface_t *muic_if = muic_data->if_data;

	if (!strncasecmp(buf, "AP", 2))
		pdata->uart_path = MUIC_PATH_UART_AP;
	else if (!strncasecmp(buf, "CP", 2))
		pdata->uart_path = MUIC_PATH_UART_CP;
	else
		pr_warn("%s invalid value\n", __func__);

	muic_if->set_switch_to_uart(muic_data);

	pr_info("%s %s\n", __func__, buf);

	return count;
}

static ssize_t s2mu107_muic_show_usb_sel(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "PDA\n");
}

static ssize_t s2mu107_muic_set_usb_sel(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t s2mu107_muic_show_usb_en(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *muic_pdata = muic_data->pdata;

	return sprintf(buf, "%s attached_dev = %d\n",
		__func__, muic_pdata->attached_dev);
}

static ssize_t s2mu107_muic_set_usb_en(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *muic_pdata = muic_data->pdata;
	muic_attached_dev_t new_dev = ATTACHED_DEV_USB_MUIC;

	if (!strncasecmp(buf, "1", 1))
		muic_core_handle_attach(muic_data->pdata, new_dev, 0, 0);
	else if (!strncasecmp(buf, "0", 1))
		muic_core_handle_detach(muic_data->pdata);
	else
		pr_info("%s invalid value\n", __func__);

	pr_info("%s attached_dev(%d)\n",
		__func__, muic_pdata->attached_dev);

	return count;
}

#ifdef TODO
static ssize_t s2mu107_muic_show_adc(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
#if IS_ENABLED(CONFIG_CCIC_S2MU107)
	struct muic_platform_data *muic_pdata = muic_data->pdata;
#endif
	int ret;

	mutex_lock(&muic_data->muic_mutex);

#if IS_ENABLED(CONFIG_CCIC_S2MU107)
	/* TODO: NOTE: There are abnormal operations of rising volatage AFC 9V
	 * by RID enable/disable in the s2mu107_muic_refresh_adc functions in the
	 * factory bianary. This is to minimize unnecessary interrupt by RID
	 * enable/disable whenever reading adc sysfs node
	 */
	if (muic_pdata->is_factory_start && muic_pdata->attached_dev == 0) {
		/* No cable detection means RID open */
		ret = ADC_OPEN;
	} else {
#if IS_ENABLED(CONFIG_MUIC_S2MU107_HV)
		if (muic_pdata->is_factory_start && muic_data->is_afc_muic_ready)
			/* No need to read adc in the middle of afc detection sequences */
			ret = ADC_GND;
		else
#endif
			ret = s2mu107_muic_refresh_adc(muic_data);
	}
#if IS_ENABLED(CONFIG_MUIC_S2MU107_HV)
	pr_info("%s: factory: %d attached_dev: %d afc ready: %d", __func__,
			muic_pdata->is_factory_start, muic_pdata->attached_dev,
			muic_data->is_afc_muic_ready);
#endif
#else
	ret = s2mu107_i2c_read_byte(muic_data->i2c, S2MU107_REG_MUIC_CTRL2);
#endif

	mutex_unlock(&muic_data->muic_mutex);
	if (ret < 0) {
		pr_err("%s err read adc reg(%d)\n",
			__func__, ret);
		return sprintf(buf, "UNKNOWN\n");
	}

	return sprintf(buf, "%x\n", (ret & ADC_MASK));
}
#endif

static ssize_t s2mu107_muic_show_usb_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *muic_pdata = muic_data->pdata;
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

#if IS_ENABLED(DEBUG_MUIC)
static ssize_t s2mu107_muic_show_mansw(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&muic_data->muic_mutex);
	ret = s2mu107_i2c_read_byte(muic_data->i2c, S2MU107_REG_MUIC_SW_CTRL);
	mutex_unlock(&muic_data->muic_mutex);

	pr_info("%s ret:%d buf%s\n", __func__, ret, buf);

	if (ret < 0) {
		pr_err("%s: fail to read muic reg\n", __func__);
		return sprintf(buf, "UNKNOWN\n");
	}
	return sprintf(buf, "0x%x\n", ret);
}
static ssize_t s2mu107_muic_show_interrupt_status(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	int st1, st2;

	mutex_lock(&muic_data->muic_mutex);
	st1 = s2mu107_i2c_read_byte(muic_data->i2c, S2MU107_REG_MUIC_INT1);
	st2 = s2mu107_i2c_read_byte(muic_data->i2c, S2MU107_REG_MUIC_INT2);
	mutex_unlock(&muic_data->muic_mutex);

	pr_info("%s st1:0x%x st2:0x%x buf%s\n", __func__, st1, st2, buf);

	if (st1 < 0 || st2 < 0) {
		pr_err("%s: fail to read muic reg\n", __func__);
		return sprintf(buf, "UNKNOWN\n");
	}
	return sprintf(buf, "st1:0x%x st2:0x%x\n", st1, st2);
}
static ssize_t s2mu107_muic_show_registers(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	char mesg[256] = "";

	mutex_lock(&muic_data->muic_mutex);
	s2mu107_read_reg_dump(muic_data, mesg);
	mutex_unlock(&muic_data->muic_mutex);
	pr_info("%s:%s\n", __func__, mesg);

	return sprintf(buf, "%s\n", mesg);
}
#endif

#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
#ifdef TODO
static ssize_t s2mu107_muic_show_otg_test(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	int ret;
	u8 val = 0;

	mutex_lock(&muic_data->muic_mutex);
	ret = s2mu107_i2c_read_byte(muic_data->i2c,
		S2MU107_REG_MUIC_INT2_MASK);
	mutex_unlock(&muic_data->muic_mutex);

	if (ret < 0) {
		pr_err("%s: fail to read muic reg\n", __func__);
		return sprintf(buf, "UNKNOWN\n");
	}

	pr_info("%s ret:%d val:%x buf%s\n",
		__func__, ret, val, buf);

	val &= INT_VBUS_ON_MASK;
	return sprintf(buf, "%x\n", val);
}

static ssize_t s2mu107_muic_set_otg_test(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *muic_pdata = muic_data->pdata;

	pr_info("%s buf:%s\n", __func__, buf);

	/*
	 *	The otg_test is set 0 durring the otg test. Not 1 !!!
	 */

	if (!strncmp(buf, "0", 1)) {
		muic_pdata->is_otg_test = 1;
#ifdef CONFIG_SEC_FACTORY
		s2mu107_muic_set_otg_reg(muic_data, 1);
#endif
	} else if (!strncmp(buf, "1", 1)) {
		muic_pdata->is_otg_test = 0;
#ifdef CONFIG_SEC_FACTORY
		s2mu107_muic_set_otg_reg(muic_data, 0);
#endif
	} else {
		pr_info("%s Wrong command\n", __func__);
		return count;
	}

	return count;
}
#endif
#endif

static ssize_t s2mu107_muic_show_attached_dev(struct device *dev,
	 struct device_attribute *attr, char *buf)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *muic_pdata = muic_data->pdata;
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
		return sprintf(buf, "DESKDOCK\n");
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		return sprintf(buf, "AUDIODOCK\n");
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
		return sprintf(buf, "PS CABLE\n");
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		return sprintf(buf, "AFC Charger\n");
	default:
		break;
	}

	return sprintf(buf, "UNKNOWN\n");
}

static ssize_t s2mu107_muic_show_audio_path(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t s2mu107_muic_set_audio_path(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return 0;
}

static ssize_t s2mu107_muic_show_apo_factory(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *muic_pdata = muic_data->pdata;
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

static ssize_t s2mu107_muic_set_apo_factory(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *muic_pdata = muic_data->pdata;

	/* "FACTORY_START": factory mode */
	if (!strncmp(buf, "FACTORY_START", 13))
		muic_pdata->is_factory_start = true;
	else
		pr_info("%s Wrong command\n",  __func__);

	return count;
}

#if IS_ENABLED(CONFIG_MUIC_S2MU107_HV)
static ssize_t muic_show_vbus_value(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *muic_pdata = muic_data->pdata;
	struct i2c_client *i2c = muic_data->i2c;
	int val = 0;
	u8 ret, vbadc, afc_ctrl = 0;

	if ((muic_pdata->attached_dev != ATTACHED_DEV_AFC_CHARGER_9V_MUIC)
		& (muic_pdata->attached_dev != ATTACHED_DEV_QC_CHARGER_9V_MUIC)) {
		/* Set AFC_EN, VB_ADC_EN to true in case of did not prepared afc
		 * for PD charger or Normal TA with flag afc_disable
		 */
		pr_info("%s: Set AFC_EN, VB_ADC_ON\n", __func__);
		afc_ctrl = s2mu107_i2c_read_byte(i2c, S2MU107_REG_AFC_CTRL1);
		afc_ctrl |= (MUIC_AFC_CTRL1_AFC_EN_MASK|MUIC_AFC_CTRL1_VB_ADC_EN_MASK);
		s2mu107_i2c_write_byte(i2c, S2MU107_REG_AFC_CTRL1, afc_ctrl);
		mdelay(10);
		afc_ctrl = s2mu107_i2c_read_byte(i2c, S2MU107_REG_AFC_CTRL1);
	}

	/* Read VBADC : must be read after afc prepared */
	ret = s2mu107_i2c_read_byte(i2c, S2MU107_REG_AFC_STATUS);
	if (ret < 0)
		pr_err("%s err read AFC STATUS(0x%2x)\n", __func__, ret);

	vbadc = ret & STATUS_VBADC_MASK;
	pr_info("%s vbadc:0x%x, afc_ctrl:0x%x cable:(%d)\n",
			__func__, vbadc, afc_ctrl, muic_pdata->attached_dev);

	switch (vbadc) {
	case VBADC_5_3V:
	case VBADC_5_7V_6_3V:
		val = 5;
		break;
	case VBADC_8_7V_9_3V:
	case VBADC_9_7V_10_3V:
		val = 9;
		break;
	default:
		break;
	}

	/* NOTE: If enter this statement with 9V, voltage would be decreased to 5V */
	if ((muic_pdata->attached_dev != ATTACHED_DEV_AFC_CHARGER_9V_MUIC) &
		(muic_pdata->attached_dev != ATTACHED_DEV_QC_CHARGER_9V_MUIC)) {
		/* Clear AFC_EN, VB_ADC_EN  */
		pr_info("%s: Clear AFC_EN, VB_ADC_ON\n", __func__);
		afc_ctrl = s2mu107_i2c_read_byte(i2c, S2MU107_REG_AFC_CTRL1);
		afc_ctrl &= ~(MUIC_AFC_CTRL1_AFC_EN_MASK|MUIC_AFC_CTRL1_VB_ADC_EN_MASK);
		s2mu107_i2c_write_byte(i2c, S2MU107_REG_AFC_CTRL1, afc_ctrl);
		afc_ctrl = s2mu107_i2c_read_byte(i2c, S2MU107_REG_AFC_CTRL1);
	}

	pr_info("%s VBUS:%d, afc_ctrl:0x%x\n", __func__, val, afc_ctrl);

	if (val > 0)
		return sprintf(buf, "%dV\n", val);

	return sprintf(buf, "UNKNOWN\n");
}

static ssize_t s2mu107_muic_show_afc_disable(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;

	if (pdata->afc_disable) {
		pr_info("%s AFC DISABLE\n", __func__);
		return sprintf(buf, "1\n");
	}

	pr_info("%s AFC ENABLE", __func__);
	return sprintf(buf, "0\n");
}

static ssize_t s2mu107_muic_set_afc_disable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct s2mu107_muic_data *muic_data = dev_get_drvdata(dev);
	struct muic_platform_data *pdata = muic_data->pdata;
	bool curr_val = pdata->afc_disable;
	int param_val, ret = 0;
#if IS_ENABLED(CONFIG_MUIC_MANAGER)
	struct muic_interface_t *muic_if = (struct muic_interface_t *)muic_data->if_data;
	int mdev = 0;
#endif /* CONFIG_MUIC_MANAGER */

	mutex_lock(&muic_data->muic_mutex);

	if (!strncasecmp(buf, "1", 1))
		pdata->afc_disable = true;
	else if (!strncasecmp(buf, "0", 1))
		pdata->afc_disable = false;
	else
		pr_warn("%s invalid value\n", __func__);

#if IS_ENABLED(CONFIG_MUIC_MANAGER)
	param_val = pdata->afc_disable ? '1' : '0';
#endif

#ifdef CM_OFFSET
	ret = sec_set_param(CM_OFFSET + 1, (char)param_val);
	if (ret < 0) {
		pr_err("%s:set_param failed - %02x:%02x(%d)\n",
			__func__, param_val, curr_val, ret);
		pdata->afc_disable = curr_val;
		return ret;
	}
#else
	pr_err("%s:set_param is NOT supported! - %02x:%02x(%d)\n",
		__func__, param_val, curr_val, ret);
#endif

	pr_info("%s afc_disable(%d)\n", __func__, pdata->afc_disable);

	/* FIXME: for factory self charging test (AFC-> NORMAL TA) */
#ifdef CONFIG_MUIC_MANAGER
	mdev = muic_manager_get_legacy_dev(muic_if);

	/* In LCiA charge test, There would be abnormal uart disconnection in
	 * case of afc_disable (0) after 619K insertion. With 619K JIG, there
	 * should be no re-detect chg interrupt which makes muic path open
	 */
	if (pdata->is_factory_start && (mdev != ATTACHED_DEV_JIG_UART_ON_MUIC)) {
		/* Do not enter with 619K in FAC binary */
		pr_info("%s re-detect chg (mdev:%d)\n", __func__, mdev);

		MUIC_PDATA_FUNC(muic_if->reset_afc_register, muic_data, &ret);

		MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->control_rid_adc,
			muic_data, S2MU107_ENABLE, &ret);
		mdelay(50);

		MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->control_rid_adc,
			muic_data, S2MU107_DISABLE, &ret);
		mdelay(50);

		MUIC_PDATA_FUNC(muic_if->bcd_rescan, muic_data, &ret);
		pr_info("%s re-detect chg done\n", __func__);

		/* TODO: When closing charge test, there would be afc_disable 0
		 * It needs to set is_afc_muic_ready to false in order to
		 * detect afc 9V again
		 */
		if (!pdata->afc_disable) {
			pr_info("%s detach for 9V re-detection\n", __func__);
			muic_core_handle_detach(muic_data->pdata);
		}
		/* Needs to 150ms after rescan */
		mdelay(150);
	}
#else
	if (muic_data->is_factory_start)
		pr_info("%s re-detect chg\n", __func__);

	muic_core_handle_detach(muic_data->pdata);
	s2mu107_muic_detect_dev(muic_data);
#endif

	mutex_unlock(&muic_data->muic_mutex);

	return count;
}

#if IS_ENABLED(CONFIG_HV_MUIC_VOLTAGE_CTRL)
static ssize_t muic_store_afc_set_voltage(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (!strncasecmp(buf, "5V", 2))
		hv_muic_change_afc_voltage(MUIC_HV_5V);
	else if (!strncasecmp(buf, "9V", 2))
		hv_muic_change_afc_voltage(MUIC_HV_9V);
	else
		pr_warn("%s invalid value : %s\n", __func__, buf);

	return count;
}
#endif /* CONFIG_HV_MUIC_VOLTAGE_CTRL */
#endif /* CONFIG_MUIC_S2MU107_HV */

static DEVICE_ATTR(uart_en, 0664, s2mu107_muic_show_uart_en,
					s2mu107_muic_set_uart_en);
static DEVICE_ATTR(uart_sel, 0664, s2mu107_muic_show_uart_sel,
					s2mu107_muic_set_uart_sel);
static DEVICE_ATTR(usb_sel, 0664, s2mu107_muic_show_usb_sel,
					s2mu107_muic_set_usb_sel);
#ifdef TODO
static DEVICE_ATTR(adc, 0664, s2mu107_muic_show_adc, NULL);
#endif
#if IS_ENABLED(DEBUG_MUIC)
static DEVICE_ATTR(mansw, 0664, s2mu107_muic_show_mansw, NULL);
static DEVICE_ATTR(dump_registers, 0664, s2mu107_muic_show_registers, NULL);
static DEVICE_ATTR(int_status, 0664, s2mu107_muic_show_interrupt_status, NULL);
#endif
static DEVICE_ATTR(usb_state, 0664, s2mu107_muic_show_usb_state, NULL);
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
#ifdef TODO
static DEVICE_ATTR(otg_test, 0664,
		s2mu107_muic_show_otg_test, s2mu107_muic_set_otg_test);
#endif
#endif
static DEVICE_ATTR(attached_dev, 0664, s2mu107_muic_show_attached_dev, NULL);
static DEVICE_ATTR(audio_path, 0664,
		s2mu107_muic_show_audio_path, s2mu107_muic_set_audio_path);
static DEVICE_ATTR(apo_factory, 0664,
		s2mu107_muic_show_apo_factory,
		s2mu107_muic_set_apo_factory);
static DEVICE_ATTR(usb_en, 0664,
		s2mu107_muic_show_usb_en,
		s2mu107_muic_set_usb_en);
#ifdef TODO
static DEVICE_ATTR(vbus_value, 0444, muic_show_vbus_value, NULL);
#endif
#if IS_ENABLED(CONFIG_MUIC_S2MU107_HV)
static DEVICE_ATTR(afc_disable, 0664,
		s2mu107_muic_show_afc_disable, s2mu107_muic_set_afc_disable);
#if IS_ENABLED(CONFIG_HV_MUIC_VOLTAGE_CTRL)
static DEVICE_ATTR(afc_set_voltage, 0220,
		NULL, muic_store_afc_set_voltage);
#endif /* CONFIG_HV_MUIC_VOLTAGE_CTRL */
#endif /* CONFIG_MUIC_S2MU107_HV */

static struct attribute *s2mu107_muic_attributes[] = {
	&dev_attr_uart_en.attr,
	&dev_attr_uart_sel.attr,
	&dev_attr_usb_sel.attr,
#ifdef TODO
	&dev_attr_adc.attr,
#endif
#if IS_ENABLED(DEBUG_MUIC)
	&dev_attr_mansw.attr,
	&dev_attr_dump_registers.attr,
	&dev_attr_int_status.attr,
#endif
	&dev_attr_usb_state.attr,
#if IS_ENABLED(CONFIG_USB_HOST_NOTIFY)
#ifdef TODO
	&dev_attr_otg_test.attr,
#endif
#endif
	&dev_attr_attached_dev.attr,
	&dev_attr_audio_path.attr,
	&dev_attr_apo_factory.attr,
	&dev_attr_usb_en.attr,
#ifdef TODO
	&dev_attr_vbus_value.attr,
#endif
#if IS_ENABLED(CONFIG_MUIC_S2MU107_HV)
	&dev_attr_afc_disable.attr,
#if IS_ENABLED(CONFIG_HV_MUIC_VOLTAGE_CTRL)
	&dev_attr_afc_set_voltage.attr,
#endif /* CONFIG_HV_MUIC_VOLTAGE_CTRL */
#endif /* CONFIG_MUIC_S2MU107_HV */
	NULL
};

static const struct attribute_group s2mu107_muic_group = {
	.attrs = s2mu107_muic_attributes,
};

int s2mu107_muic_init_sysfs(struct s2mu107_muic_data *muic_data)
{
	int ret;
	/* create sysfs group */
#if IS_ENABLED(CONFIG_SEC_FACTORY)
	muic_data->switch_device = sec_device_find("switch");
#endif

	if (muic_data->switch_device == NULL)
		muic_data->switch_device = sec_device_create(NULL, "switch");

	if (IS_ERR(muic_data->switch_device)) {
		pr_err("%s Failed to create device(switch)!\n", __func__);
		ret = -ENODEV;
		return ret;
	}

	ret = sysfs_create_group(&muic_data->switch_device->kobj, &s2mu107_muic_group);
	if (ret) {
		pr_err("failed to create sysfs\n");
		return ret;
	}
	dev_set_drvdata(muic_data->switch_device, muic_data);
	return ret;
}

void s2mu107_muic_deinit_sysfs(struct s2mu107_muic_data *muic_data)
{
	if (muic_data->switch_device)
		sysfs_remove_group(&muic_data->switch_device->kobj, &s2mu107_muic_group);
}
