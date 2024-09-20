/* drivers/muic/muic-core.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

 #define pr_fmt(fmt)	"[MUIC] " fmt

#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio.h>

/* switch device header */
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif /* CONFIG_SWITCH */

#include <linux/slab.h>
#include <linux/muic/muic.h>
#include <linux/muic/muic_interface.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_CCIC_NOTIFIER)
#include <linux/ccic/ccic_notifier.h>
#endif

#ifdef CONFIG_SWITCH
static struct switch_dev switch_dock = {
	.name = "dock",
};

struct switch_dev switch_uart3 = {
	.name = "uart3",    /* sys/class/switch/uart3/state */
};
#endif /* CONFIG_SWITCH */

static struct muic_platform_data *static_pdata;

#if defined(CONFIG_MUIC_NOTIFIER)
static struct notifier_block dock_notifier_block;

void muic_send_dock_intent(int type)
{
	pr_info("%s: MUIC dock type(%d)\n", __func__, type);
#ifdef CONFIG_SWITCH
	switch_set_state(&switch_dock, type);
#endif
}

static void muic_jig_uart_cb(int jig_state)
{
	pr_info("%s: MUIC uart type(%d)\n", __func__, jig_state);
#ifdef CONFIG_SWITCH
	switch_set_state(&switch_uart3, jig_state);
#endif
}

static int muic_dock_attach_notify(int type, const char *name)
{
	pr_info("%s: %s\n", __func__, name);
	muic_send_dock_intent(type);

	return NOTIFY_OK;
}

static int muic_dock_detach_notify(void)
{
	pr_info("%s\n", __func__);
	muic_send_dock_intent(MUIC_DOCK_DETACHED);

	return NOTIFY_OK;
}

static int muic_handle_dock_notification(struct notifier_block *nb,
			unsigned long action, void *data)
{
#if defined(CONFIG_CCIC_NOTIFIER)
	CC_NOTI_ATTACH_TYPEDEF *pnoti = (CC_NOTI_ATTACH_TYPEDEF *)data;
	muic_attached_dev_t attached_dev = pnoti->cable_type;
#else
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
#endif
	int type = MUIC_DOCK_DETACHED;
	const char *name;

	switch (attached_dev) {
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_DESKDOCK;
			name = "Desk Dock Attach";
			return muic_dock_attach_notify(type, name);
		} else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_CARDOCK_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_CARDOCK;
			name = "Car Dock Attach";
			return muic_dock_attach_notify(type, name);
		} else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_SMARTDOCK_MUIC:
	case ATTACHED_DEV_SMARTDOCK_VB_MUIC:
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
		if (action == MUIC_NOTIFY_CMD_LOGICALLY_ATTACH) {
			type = MUIC_DOCK_SMARTDOCK;
			name = "Smart Dock Attach";
			return muic_dock_attach_notify(type, name);
		} else if (action == MUIC_NOTIFY_CMD_LOGICALLY_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_UNIVERSAL_MMDOCK_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_SMARTDOCK;
			name = "Universal MMDock Attach";
			return muic_dock_attach_notify(type, name);
		} else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_AUDIODOCK;
			name = "Audio Dock Attach";
			return muic_dock_attach_notify(type, name);
		} else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_HMT_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_HMT;
			name = "HMT Attach";
			return muic_dock_attach_notify(type, name);
		} else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	case ATTACHED_DEV_GAMEPAD_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			type = MUIC_DOCK_GAMEPAD;
			name = "Gamepad Attach";
			return muic_dock_attach_notify(type, name);
		} else if (action == MUIC_NOTIFY_CMD_DETACH)
			return muic_dock_detach_notify();
		break;
	default:
		break;
	}

	pr_info("%s: ignore(%d)\n", __func__, attached_dev);
	return NOTIFY_DONE;
}
#endif /* CONFIG_MUIC_NOTIFIER */

static void muic_init_switch_dev_cb(void)
{
#ifdef CONFIG_SWITCH
	int ret;

	/* for DockObserver */
	ret = switch_dev_register(&switch_dock);
	if (ret < 0) {
		pr_err("%s: Failed to register dock switch(%d)\n",
				__func__, ret);
		return;
	}

	ret = switch_dev_register(&switch_uart3);
	if (ret < 0) {
		pr_err("%s: Failed to register uart3 switch(%d)\n",
				__func__, ret);
		return;
	}

#endif /* CONFIG_SWITCH */

#if defined(CONFIG_MUIC_NOTIFIER)
	muic_notifier_register(&dock_notifier_block,
			muic_handle_dock_notification, MUIC_NOTIFY_DEV_DOCK);
#endif /* CONFIG_MUIC_NOTIFIER */

	pr_info("%s: done\n", __func__);
}

static void muic_cleanup_switch_dev_cb(void)
{
#if defined(CONFIG_MUIC_NOTIFIER)
	muic_notifier_unregister(&dock_notifier_block);
#endif /* CONFIG_MUIC_NOTIFIER */

	pr_info("%s: done\n", __func__);
}

static int switch_sel;
/* func : set_switch_sel
 * switch_sel value get from bootloader comand line
 * switch_sel data consist 8 bits (xxxxyyyyzzzz)
 * first 4bits(zzzz) mean path infomation.
 * next 4bits(yyyy) mean if pmic version info
 * next 4bits(xxxx) mean afc disable info
 */
static int set_switch_sel(char *str)
{
	get_option(&str, &switch_sel);
	switch_sel &= 0xfff;
	pr_info("%s: switch_sel: 0x%03x\n", __func__,
			switch_sel);
	return switch_sel;
}
__setup("pmic_info=", set_switch_sel);

int get_switch_sel(void)
{
	return switch_sel;
}

/* afc_mode:
 *   0x31 : Disabled
 *   0x30 : Enabled
 */
static int afc_mode;
static int __init set_afc_mode(char *str)
{
	int mode;

	get_option(&str, &mode);
	afc_mode = (mode & 0x0000FF00) >> 8;
	pr_info("%s: afc_mode is 0x%02x\n", __func__, afc_mode);

	return 0;
}
early_param("charging_mode", set_afc_mode);

int get_afc_mode(void)
{
	return afc_mode;
}

int muic_init_gpio_cb(void *data, int switch_sel)
{
	struct muic_platform_data *muic_pdata = (struct muic_platform_data *)data;
	const char *usb_mode;
	const char *uart_mode;
	struct muic_interface_t *muic_if = (struct muic_interface_t *)muic_pdata->muic_if;
	int ret = 0;

	pr_info("%s (%d)\n", __func__, switch_sel);

	if (switch_sel & SWITCH_SEL_USB_MASK) {
		muic_pdata->usb_path = MUIC_PATH_USB_AP;
		usb_mode = "PDA";
	} else {
		muic_pdata->usb_path = MUIC_PATH_USB_CP;
		usb_mode = "MODEM";
	}

	MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->set_gpio_usb_sel,
		muic_pdata->drv_data, muic_pdata->uart_path, &ret);

	if (switch_sel & SWITCH_SEL_UART_MASK) {
		muic_pdata->uart_path = MUIC_PATH_UART_AP;
		uart_mode = "AP";
	} else {
		muic_pdata->uart_path = MUIC_PATH_UART_CP;
		uart_mode = "CP";
	}

	/* These flags MUST be updated again from probe function */
	muic_pdata->rustproof_on = false;

#if !defined(CONFIG_SEC_FACTORY) && defined(CONFIG_MUIC_SUPPORT_TYPEB)
	if (!(switch_sel & SWITCH_SEL_RUSTPROOF_MASK))
		muic_pdata->rustproof_on = true;
#endif /* !CONFIG_SEC_FACTORY */

	muic_pdata->afc_disable = false;

	MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->set_gpio_uart_sel,
		muic_pdata->drv_data, muic_pdata->uart_path, &ret);

	pr_info("%s: usb_path(%s), uart_path(%s)\n", __func__,
			usb_mode, uart_mode);

	return ret;
}

static int muic_core_switch_to_dock_audio(struct muic_platform_data *muic_pdata)
{
	int ret = 0;
	struct muic_interface_t *muic_if = (struct muic_interface_t *)muic_pdata->muic_if;

	pr_info("%s\n", __func__);

	if (muic_if->set_com_to_audio == NULL) {
		pr_err("%s func not set.\n", __func__);
		return -1;
	}

	MUIC_PDATA_FUNC(muic_if->set_com_to_audio, muic_pdata->drv_data, &ret);
	if (ret) {
		pr_err("%s com->audio set err\n", __func__);
		return ret;
	}

	return ret;
}

static int muic_core_switch_to_system_audio(struct muic_platform_data *muic_pdata)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	return ret;
}

static int muic_core_switch_to_usb(struct muic_platform_data *muic_pdata, int path)
{
	int ret = 0;
	struct muic_interface_t *muic_if = (struct muic_interface_t *)muic_pdata->muic_if;

	pr_info("%s\n", __func__);
#if !defined(CONFIG_MUIC_USB_SWITCH)
	if (muic_pdata->gpio_usb_sel) {
		MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->set_gpio_usb_sel,
			muic_pdata->drv_data, path, &ret);
		if (ret) {
			pr_err("%s com->uart set err\n", __func__);
			return ret;
		}
	}
#endif

	MUIC_PDATA_FUNC(muic_if->set_switch_to_usb, muic_pdata->drv_data, &ret);
	if (ret) {
		pr_err("%s com->usb set err\n", __func__);
		return ret;
	}

	return ret;
}

static int muic_core_switch_to_uart(struct muic_platform_data *muic_pdata, int path)
{
	int ret = 0;
	struct muic_interface_t *muic_if = (struct muic_interface_t *)muic_pdata->muic_if;

	pr_info("%s\n", __func__);
#if !defined(CONFIG_MUIC_UART_SWITCH)
	if (muic_pdata->gpio_uart_sel) {
		MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->set_gpio_uart_sel,
			muic_pdata->drv_data, path, &ret);
		if (ret) {
			pr_err("%s com->uart set err\n", __func__);
			return ret;
		}
	}
#endif

	MUIC_PDATA_FUNC(muic_if->set_switch_to_uart, muic_pdata->drv_data, &ret);
	if (ret) {
		pr_err("%s com->uart set err\n", __func__);
		return ret;
	}

	return ret;
}

static int muic_core_attach_charger(struct muic_platform_data *muic_pdata,
			muic_attached_dev_t new_dev)
{
	int ret = 0;

	pr_info("%s : %d\n", __func__, new_dev);

	muic_pdata->attached_dev = new_dev;

	return ret;
}

static int muic_core_detach_charger(struct muic_platform_data *muic_pdata)
{
	int ret = 0;
#ifndef CONFIG_MUIC_SKIP_INCOMPLETE_INSERT
	struct muic_interface_t *muic_if = (struct muic_interface_t *)muic_pdata->muic_if;
#endif

	pr_info("%s\n", __func__);

#ifndef CONFIG_MUIC_SKIP_INCOMPLETE_INSERT
	if (muic_if != NULL)
		muic_if->is_dcp_charger = false;
#endif

	muic_pdata->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int muic_core_attach_usb_util(struct muic_platform_data *muic_pdata,
			muic_attached_dev_t new_dev)
{
	int ret = 0;

	ret = muic_core_switch_to_usb(muic_pdata, muic_pdata->usb_path);
	if (ret)
		return ret;

	ret = muic_core_attach_charger(muic_pdata, new_dev);
	return ret;
}

static int muic_core_attach_usb(struct muic_platform_data *muic_pdata,
			muic_attached_dev_t new_dev)
{
	int ret = 0;

	if (muic_pdata->attached_dev == new_dev) {
		pr_info("%s duplicated\n", __func__);
		return ret;
	}

	pr_info("%s:%s\n", MUIC_CORE, __func__);

	pr_info("%s\n", __func__);

	ret = muic_core_attach_usb_util(muic_pdata, new_dev);
	if (ret)
		return ret;

	return ret;
}

static int muic_core_attach_otg_usb(struct muic_platform_data *muic_pdata,
			muic_attached_dev_t new_dev)
{
	int ret = 0;
	struct muic_interface_t *muic_if = (struct muic_interface_t *)muic_pdata->muic_if;

	if (muic_pdata->attached_dev == new_dev) {
		pr_info("%s duplicated\n", __func__);
		return ret;
	}

	pr_info("%s\n", __func__);

	if (muic_if->set_com_to_otg) {
		ret = muic_if->set_com_to_otg(muic_pdata->drv_data);
		if (ret) {
			pr_err("%s switch set err\n", __func__);
			return ret;
		}
	} else {
		pr_err("%s func not set.\n", __func__);
	}

	muic_pdata->attached_dev = new_dev;

	return ret;
}

static int muic_core_detach_otg_usb(struct muic_platform_data *muic_pdata)
{
	int ret = 0;
	struct muic_interface_t *muic_if = (struct muic_interface_t *)muic_pdata->muic_if;

	pr_info("%s : %d\n",
		__func__, muic_pdata->attached_dev);

	if (muic_if->set_com_to_open) {
		ret = muic_if->set_com_to_open(muic_pdata->drv_data);
		if (ret) {
			pr_err("%s switch set err\n", __func__);
			return ret;
		}
	} else {
		pr_err("%s func not set.\n", __func__);
	}

	muic_pdata->attached_dev = ATTACHED_DEV_NONE_MUIC;

	if (muic_pdata->usb_path == MUIC_PATH_USB_CP)
		return ret;

	return ret;
}

static int muic_core_detach_usb(struct muic_platform_data *muic_pdata)
{
	int ret = 0;
	struct muic_interface_t *muic_if = (struct muic_interface_t *)muic_pdata->muic_if;

	pr_info("%s : %d\n",
		__func__, muic_pdata->attached_dev);

	ret = muic_core_detach_charger(muic_pdata);
	if (ret)
		return ret;

	if (muic_if->set_com_to_open) {
		ret = muic_if->set_com_to_open(muic_pdata->drv_data);
		if (ret) {
			pr_err("%s switch set err\n", __func__);
			return ret;
		}
	} else {
		pr_err("%s func not set.\n", __func__);
	}

	muic_pdata->attached_dev = ATTACHED_DEV_NONE_MUIC;

	if (muic_pdata->usb_path == MUIC_PATH_USB_CP)
		return ret;

	return ret;
}

static int muic_core_attach_deskdock(struct muic_platform_data *muic_pdata,
			muic_attached_dev_t new_dev, u8 vbvolt)
{
	int ret = 0;

	pr_info("%s vbus(%x)\n", __func__, vbvolt);

	ret = muic_core_switch_to_dock_audio(muic_pdata);
	if (ret)
		return ret;

	if (vbvolt)
		ret = muic_core_attach_charger(muic_pdata, new_dev);
	else
		ret = muic_core_detach_charger(muic_pdata);
	if (ret)
		return ret;

	muic_pdata->attached_dev = new_dev;

	return ret;
}

static int muic_core_detach_deskdock(struct muic_platform_data *muic_pdata)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = muic_core_switch_to_system_audio(muic_pdata);
	if (ret)
		pr_err("%s err changing audio path(%d)\n",
			__func__, ret);

	ret = muic_core_detach_charger(muic_pdata);
	if (ret)
		pr_err("%s err detach_charger(%d)\n",
			__func__, ret);

	muic_pdata->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int muic_core_attach_audiodock(struct muic_platform_data *muic_pdata,
			muic_attached_dev_t new_dev, u8 vbus)
{
	int ret = 0;
	struct muic_interface_t *muic_if = (struct muic_interface_t *)muic_pdata->muic_if;

	pr_info("%s\n", __func__);

	if (!vbus) {
		ret = muic_core_detach_charger(muic_pdata);
		if (ret)
			pr_err("%s err detach_charger(%d)\n",
				__func__, ret);

		if (muic_if->set_com_to_open) {
			ret = muic_if->set_com_to_open(muic_pdata->drv_data);
			if (ret)
				return ret;
		}

		muic_pdata->attached_dev = new_dev;

		return ret;
	}

	ret = muic_core_attach_usb_util(muic_pdata, new_dev);
	if (ret)
		pr_err("%s attach_usb_util(%d)\n",
			__func__, ret);

	muic_pdata->attached_dev = new_dev;

	return ret;
}

static int muic_core_detach_audiodock(struct muic_platform_data *muic_pdata)
{
	int ret = 0;
	struct muic_interface_t *muic_if = (struct muic_interface_t *)muic_pdata->muic_if;

	pr_info("%s\n", __func__);

	ret = muic_core_detach_charger(muic_pdata);
	if (ret)
		pr_err("%s err detach_charger(%d)\n",
			__func__, ret);

	if (muic_if->set_com_to_open) {
		ret = muic_if->set_com_to_open(muic_pdata->drv_data);
		if (ret)
			return ret;
	}

	muic_pdata->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int muic_core_attach_jig_uart_boot_off(struct muic_platform_data *muic_pdata,
				muic_attached_dev_t new_dev)
{
	int ret = 0;

	pr_info("%s(%d)\n",
		__func__, new_dev);
	ret = muic_core_switch_to_uart(muic_pdata, muic_pdata->uart_path);

	ret = muic_core_attach_charger(muic_pdata, new_dev);

	return ret;
}

static int muic_core_detach_jig_uart_boot_off(struct muic_platform_data *muic_pdata)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = muic_core_detach_charger(muic_pdata);
	if (ret)
		pr_err("%s err detach_charger(%d)\n", __func__, ret);

	muic_pdata->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return ret;
}

static int muic_core_detach_jig_uart_boot_on(struct muic_platform_data *muic_pdata)
{
	pr_info("%s\n", __func__);

	muic_pdata->attached_dev = ATTACHED_DEV_NONE_MUIC;

	return 0;
}

static int muic_core_attach_jig_usb_boot_off(struct muic_platform_data *muic_pdata,
	u8 vbvolt)
{
	int ret = 0;

	if (muic_pdata->attached_dev == ATTACHED_DEV_JIG_USB_OFF_MUIC) {
		pr_info("%s duplicated\n", __func__);
		return ret;
	}

	pr_info("%s\n", __func__);

	ret = muic_core_attach_usb_util(muic_pdata, ATTACHED_DEV_JIG_USB_OFF_MUIC);
	if (ret)
		return ret;

	return ret;
}

static int muic_core_attach_jig_usb_boot_on(struct muic_platform_data *muic_pdata,
	u8 vbvolt)
{
	int ret = 0;

	if (muic_pdata->attached_dev == ATTACHED_DEV_JIG_USB_ON_MUIC) {
		pr_info("%s duplicated\n", __func__);
		return ret;
	}

	pr_info("%s\n", __func__);

	ret = muic_core_attach_usb_util(muic_pdata, ATTACHED_DEV_JIG_USB_ON_MUIC);
	if (ret)
		return ret;

	return ret;
}

static int muic_core_handle_attached_prev_dev(struct muic_platform_data *muic_pdata,
	muic_attached_dev_t new_dev, bool *noti)
{
	int ret = 0;
	bool lc_noti = *noti;

	pr_info("%s attached_dev: %d, new_dev: %d\n",
		__func__, muic_pdata->attached_dev, new_dev);

	switch (muic_pdata->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		if (new_dev != muic_pdata->attached_dev) {
			pr_info("%s new(%d)!=attached(%d)\n",
				__func__, new_dev, muic_pdata->attached_dev);
			ret = muic_core_detach_usb(muic_pdata);
		}
		break;
	case ATTACHED_DEV_OTG_MUIC:
	/* OTG -> LANHUB, meaning TA is attached to LANHUB(OTG) */
		if (new_dev != muic_pdata->attached_dev) {
			pr_info("%s new(%d)!=attached(%d)",
				__func__, new_dev, muic_pdata->attached_dev);
			ret = muic_core_detach_otg_usb(muic_pdata);
		}
		break;

	case ATTACHED_DEV_AUDIODOCK_MUIC:
		if (new_dev != muic_pdata->attached_dev) {
			pr_info("%s new(%d)!=attached(%d)\n",
				__func__, new_dev, muic_pdata->attached_dev);
			ret = muic_core_detach_audiodock(muic_pdata);
		}
		break;

	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
		if (new_dev != muic_pdata->attached_dev) {
			pr_info("%s new(%d)!=attached(%d)\n",
				__func__, new_dev, muic_pdata->attached_dev);
			ret = muic_core_detach_charger(muic_pdata);
		}
		break;

	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		if (new_dev != muic_pdata->attached_dev) {
			if (muic_core_get_ccic_cable_state(muic_pdata)) {
				lc_noti = false;
			}
			pr_info("%s new(%d)!=attached(%d)\n",
				__func__, new_dev, muic_pdata->attached_dev);
			ret = muic_core_detach_jig_uart_boot_off(muic_pdata);
		}
		break;

	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		if (new_dev != muic_pdata->attached_dev) {
			if (muic_core_get_ccic_cable_state(muic_pdata)) {
				lc_noti = false;
			}
			pr_info("%s new(%d)!=attached(%d)\n",
				__func__, new_dev, muic_pdata->attached_dev);
			ret = muic_core_detach_jig_uart_boot_off(muic_pdata);
		}
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
		if (new_dev != muic_pdata->attached_dev) {
			pr_info("%s new(%d)!=attached(%d)\n",
				__func__, new_dev, muic_pdata->attached_dev);

			if (muic_pdata->is_factory_start)
				ret = muic_core_detach_deskdock(muic_pdata);
			else {
				*noti = false;
				ret = muic_core_detach_jig_uart_boot_on(muic_pdata);
			}
		}
		break;
	case ATTACHED_DEV_NONE_MUIC:
	default:
		break;
	}

	if (*noti && lc_noti) {
		if (!muic_pdata->suspended)
			MUIC_SEND_NOTI_DETACH(muic_pdata->attached_dev);
		else
			muic_pdata->need_to_noti = true;
	}

	return ret;
}

static int muic_core_handle_attached_new_dev(struct muic_platform_data *muic_pdata,
	muic_attached_dev_t new_dev, bool *noti)
{
	int ret = 0;
	struct muic_interface_t *muic_if = (struct muic_interface_t *)muic_pdata->muic_if;

	pr_info("%s : muic_pdata->attached_dev: %d, new_dev: %d\n",
		__func__, muic_pdata->attached_dev, new_dev);

	switch (new_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
		ret = muic_core_attach_usb(muic_pdata, new_dev);
		break;
	case ATTACHED_DEV_OTG_MUIC:
		ret = muic_core_attach_otg_usb(muic_pdata, new_dev);
		break;
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		ret = muic_core_attach_audiodock(muic_pdata, new_dev, muic_pdata->vbvolt);
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
	case ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC:
		ret = muic_core_attach_charger(muic_pdata, new_dev);
#if defined(CONFIG_HV_MUIC_S2MU004_AFC) || defined(CONFIG_MUIC_HV)
		MUIC_PDATA_FUNC(muic_if->check_afc_ready, muic_pdata->drv_data, &ret);
#endif /* CONFIG_HV_MUIC_S2MU004_AFC */
		MUIC_PDATA_FUNC(muic_if->set_com_to_open_with_vbus, muic_pdata->drv_data, &ret);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
#if defined(CONFIG_MUIC_SUPPORT_TYPEB)
#if defined(CONFIG_SEC_FACTORY)
		muic_pdata->is_jig_on = true;
#endif
#else
		muic_pdata->is_jig_on = true;
#endif
		ret = muic_core_attach_jig_uart_boot_off(muic_pdata, new_dev);
		break;
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
#if !defined(CONFIG_MUIC_SUPPORT_TYPEB)
		muic_pdata->is_jig_on = true;
#endif
		ret = muic_core_attach_jig_uart_boot_off(muic_pdata, new_dev);
		break;
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
#if !defined(CONFIG_MUIC_SUPPORT_TYPEB)
		muic_pdata->is_jig_on = true;
#endif
		ret = muic_core_attach_jig_usb_boot_off(muic_pdata, muic_pdata->vbvolt);
		break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
#if !defined(CONFIG_MUIC_SUPPORT_TYPEB)
		muic_pdata->is_jig_on = true;
#endif
		ret = muic_core_attach_jig_usb_boot_on(muic_pdata, muic_pdata->vbvolt);
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
		ret = muic_core_attach_deskdock(muic_pdata, new_dev, muic_pdata->vbvolt);
		break;
	case ATTACHED_DEV_UNKNOWN_MUIC:
		MUIC_PDATA_FUNC(muic_if->set_com_to_open_with_vbus, muic_pdata->drv_data, &ret);
		ret = muic_core_attach_charger(muic_pdata, new_dev);
		break;
	case ATTACHED_DEV_TYPE3_MUIC:
		muic_pdata->attached_dev = new_dev;
		break;
	case ATTACHED_DEV_UNDEFINED_RANGE_MUIC:
		muic_pdata->attached_dev = new_dev;
		break;
	default:
		*noti = false;
		pr_info("%s unsupported dev=%d, adc=0x%x, vbus=%c\n",
			__func__, new_dev, muic_pdata->adc,
			(muic_pdata->vbvolt ? 'O' : 'X'));
		break;
	}

/* TODO: There is no needs to use JIGB pin by MUIC if CCIC is supported */
#ifndef CONFIG_USE_CCIC
	MUIC_PDATA_FUNC(muic_if->set_jig_ctrl_on, muic_pdata->drv_data, &ret);
#endif

	if (ret)
		pr_err("%s something wrong %d (ERR=%d)\n",
			__func__, new_dev, ret);

	pr_info("%s:%s done\n", MUIC_CORE, __func__);

#if defined(CONFIG_HV_MUIC_S2MU004_AFC) || defined(CONFIG_MUIC_HV)
	MUIC_PDATA_FUNC_MULTI_PARAM(muic_if->check_id_err, muic_pdata->drv_data,
			new_dev, &ret);
	new_dev = ret;
#endif /* CONFIG_HV_MUIC_S2MU004_AFC */

	return ret;
}

bool muic_core_get_ccic_cable_state(struct muic_platform_data *muic_pdata)
{
	pr_info("%s call, attached_dev : %d\n", __func__, muic_pdata->attached_dev);

	switch (muic_pdata->attached_dev) {
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
#if defined(CONFIG_MUIC_SUPPORT_PRSWAP)
	case ATTACHED_DEV_USB_MUIC:
#endif
		return true;
	default:
		break;
	}

	return false;
}

int muic_core_handle_attach(struct muic_platform_data *muic_pdata,
			muic_attached_dev_t new_dev, int adc, u8 vbvolt)
{
	int ret = 0;
	bool noti = (new_dev != muic_pdata->attached_dev) ? true : false;

	muic_pdata->is_jig_on = false;
	pr_info("%s attached_dev: %d, new_dev: %d\n",
		__func__, muic_pdata->attached_dev, new_dev);
	muic_pdata->adc = adc;
	muic_pdata->vbvolt = vbvolt;

	if (muic_pdata->attached_dev != ATTACHED_DEV_NONE_MUIC) {
		ret = muic_core_handle_attached_prev_dev(muic_pdata, new_dev, &noti);
		if (ret)
			pr_err("%s prev_dev failed\n", __func__);
	}

	ret = muic_core_handle_attached_new_dev(muic_pdata, new_dev, &noti);
	if (ret)
		pr_err("%s new_dev failed\n", __func__);

	if (noti) {
		if (!muic_pdata->suspended)
			MUIC_SEND_NOTI_ATTACH(new_dev);
		else
			muic_pdata->need_to_noti = true;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(muic_core_handle_attach);

int muic_core_handle_detach(struct muic_platform_data *muic_pdata)
{
	int ret = 0;
	bool noti = true;
	struct muic_interface_t *muic_if = (struct muic_interface_t *)muic_pdata->muic_if;

	MUIC_PDATA_FUNC(muic_if->set_com_to_open, muic_pdata->drv_data, &ret);
#if defined(CONFIG_MUIC_SUPPORT_TYPEB)
	muic_pdata->is_jig_on = false;
	MUIC_PDATA_FUNC(muic_if->set_jig_ctrl_on, muic_pdata->drv_data, &ret);
#endif

	switch (muic_pdata->attached_dev) {
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_TIMEOUT_OPEN_MUIC:
#if defined(CONFIG_SEC_FACTORY)
	case ATTACHED_DEV_CARKIT_MUIC:
#endif
		ret = muic_core_detach_usb(muic_pdata);
		break;
	case ATTACHED_DEV_OTG_MUIC:
		ret = muic_core_detach_otg_usb(muic_pdata);
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
		ret = muic_core_detach_charger(muic_pdata);
#if defined(CONFIG_HV_MUIC_S2MU004_AFC) || defined(CONFIG_MUIC_HV)
		MUIC_PDATA_FUNC(muic_if->reset_hvcontrol_reg, muic_pdata->drv_data, &ret);
#endif
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_VB_MUIC:
		ret = muic_core_detach_jig_uart_boot_off(muic_pdata);
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
		if (muic_pdata->is_factory_start)
			ret = muic_core_detach_deskdock(muic_pdata);
		else {
			noti = false;
			ret = muic_core_detach_jig_uart_boot_on(muic_pdata);
		}
		break;
	case ATTACHED_DEV_AUDIODOCK_MUIC:
		ret = muic_core_detach_audiodock(muic_pdata);
		break;
	case ATTACHED_DEV_NONE_MUIC:
		pr_info("%s duplicated(NONE)\n", __func__);
		break;
	case ATTACHED_DEV_UNKNOWN_MUIC:
		pr_info("%s UNKNOWN\n", __func__);
		ret = muic_core_detach_charger(muic_pdata);
		muic_pdata->attached_dev = ATTACHED_DEV_NONE_MUIC;
		break;
#if defined(CONFIG_HV_MUIC_S2MU004_AFC) || defined(CONFIG_MUIC_HV)
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_DISABLED_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC:
		ret = muic_core_detach_charger(muic_pdata);
		MUIC_PDATA_FUNC(muic_if->reset_hvcontrol_reg,muic_pdata->drv_data, &ret);
		break;
#endif
	default:
		noti = false;
		pr_info("%s invalid type(%d)\n",
			__func__, muic_pdata->attached_dev);
		muic_pdata->attached_dev = ATTACHED_DEV_NONE_MUIC;
		break;
	}
	if (ret)
		pr_err("%s something wrong %d (ERR=%d)\n",
			__func__, muic_pdata->attached_dev, ret);

	if (noti) {
		if (!muic_pdata->suspended)
			MUIC_SEND_NOTI_DETACH(muic_pdata->attached_dev);
		else
			muic_pdata->need_to_noti = true;
	}
	return ret;
}
EXPORT_SYMBOL_GPL(muic_core_handle_detach);

#if defined(CONFIG_MUIC_HV)
static inline void muic_core_hv_set_new_state(struct muic_platform_data *muic_pdata,
		muic_hv_state_t next_state)
{
	muic_pdata->hv_state = next_state;
}

static inline muic_hv_state_t muic_core_hv_get_current_state(struct muic_platform_data *muic_pdata)
{
	return muic_pdata->hv_state;
}

bool muic_core_hv_is_hv_dev(struct muic_platform_data *muic_pdata)
{
	bool ret = false;

	switch (muic_pdata->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC:
		ret = true;
		break;
	default:
		ret = false;
	}

	pr_info("%s attached_dev(%d)[%c]\n",
			__func__, muic_pdata->attached_dev, (ret ? 'T' : 'F'));

	return ret;
}
EXPORT_SYMBOL_GPL(muic_core_hv_is_hv_dev);

void muic_core_hv_handle_state(struct muic_platform_data *muic_pdata,
		muic_hv_state_t next_state)
{
	struct muic_interface_t *muic_if = muic_pdata->muic_if;
	muic_core_hv_set_new_state(muic_pdata, next_state);

	switch (next_state) {
	case HV_STATE_IDLE:
		MUIC_PDATA_VOID_FUNC(muic_if->hv_reset, muic_pdata->drv_data);
		break;
	case HV_STATE_DCP_CHARGER:
		MUIC_PDATA_VOID_FUNC(muic_if->hv_dcp_charger, muic_pdata->drv_data);
		break;
	case HV_STATE_FAST_CHARGE_ADAPTOR:
		MUIC_PDATA_VOID_FUNC(muic_if->hv_fast_charge_adaptor, muic_pdata->drv_data);
		break;
	case HV_STATE_FAST_CHARGE_COMMUNICATION:
		MUIC_PDATA_VOID_FUNC(muic_if->hv_fast_charge_communication, muic_pdata->drv_data);
		break;
	case HV_STATE_AFC_5V_CHARGER:
		MUIC_PDATA_VOID_FUNC(muic_if->hv_afc_5v_charger, muic_pdata->drv_data);
		break;
	case HV_STATE_AFC_9V_CHARGER:
		MUIC_PDATA_VOID_FUNC(muic_if->hv_afc_9v_charger, muic_pdata->drv_data);
		break;
	case HV_STATE_QC_CHARGER:
		MUIC_PDATA_VOID_FUNC(muic_if->hv_qc_charger, muic_pdata->drv_data);
		break;
	case HV_STATE_QC_5V_CHARGER:
		MUIC_PDATA_VOID_FUNC(muic_if->hv_qc_5v_charger, muic_pdata->drv_data);
		break;
	case HV_STATE_QC_9V_CHARGER:
		MUIC_PDATA_VOID_FUNC(muic_if->hv_qc_9v_charger, muic_pdata->drv_data);
		break;
	default:
		pr_err("%s not defined state : %d\n",
				__func__, next_state);
		break;
	}
}

int muic_core_hv_state_manager(struct muic_platform_data *muic_pdata,
		muic_hv_transaction_t trans)
{
	muic_hv_state_t cur_state = muic_core_hv_get_current_state(muic_pdata);
	muic_hv_state_t next_state = HV_STATE_INVALID;
	bool skip_trans = false;

	switch (cur_state) {
	case HV_STATE_IDLE:
		switch (trans) {
		case HV_TRANS_DCP_DETECTED:
			next_state = HV_STATE_DCP_CHARGER;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_DCP_CHARGER:
		switch (trans) {
		case HV_TRANS_MUIC_DETACH:
		case HV_TRANS_NO_RESPONSE:
			next_state = HV_STATE_IDLE;
			break;
		case HV_TRANS_VDNMON_LOW:
			next_state = HV_STATE_FAST_CHARGE_ADAPTOR;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_FAST_CHARGE_ADAPTOR:
		switch (trans) {
		case HV_TRANS_MUIC_DETACH:
			next_state = HV_STATE_IDLE;
			break;
		case HV_TRANS_NO_RESPONSE:
			next_state = HV_STATE_QC_CHARGER;
			break;
		case HV_TRANS_FAST_CHARGE_PING_RESPONSE:
			next_state = HV_STATE_FAST_CHARGE_COMMUNICATION;
			break;
		case HV_TRANS_FAST_CHARGE_REOPEN:
			next_state = HV_STATE_FAST_CHARGE_ADAPTOR;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_FAST_CHARGE_COMMUNICATION:
		switch (trans) {
		case HV_TRANS_MUIC_DETACH:
			next_state = HV_STATE_IDLE;
			break;
		case HV_TRANS_NO_RESPONSE:
			next_state = HV_STATE_QC_CHARGER;
			break;
		case HV_TRANS_FAST_CHARGE_PING_RESPONSE:
			next_state = HV_STATE_FAST_CHARGE_COMMUNICATION;
			break;
		case HV_TRANS_VBUS_BOOST:
			next_state = HV_STATE_AFC_9V_CHARGER;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
    case HV_STATE_AFC_5V_CHARGER:
        switch (trans) {
		case HV_TRANS_MUIC_DETACH:
			next_state = HV_STATE_IDLE;
			break;
        case HV_TRANS_VBUS_BOOST:
			next_state = HV_STATE_AFC_9V_CHARGER;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_AFC_9V_CHARGER:
		switch (trans) {
		case HV_TRANS_MUIC_DETACH:
			next_state = HV_STATE_IDLE;
			break;
        case HV_TRANS_VBUS_REDUCE:
			next_state = HV_STATE_AFC_5V_CHARGER;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_QC_CHARGER:
		switch (trans) {
		case HV_TRANS_MUIC_DETACH:
			next_state = HV_STATE_IDLE;
			break;
		case HV_TRANS_VBUS_BOOST:
			next_state = HV_STATE_QC_9V_CHARGER;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_QC_5V_CHARGER:
		switch (trans) {
		case HV_TRANS_MUIC_DETACH:
			next_state = HV_STATE_IDLE;
			break;
		case HV_TRANS_VBUS_BOOST:
			next_state = HV_STATE_QC_9V_CHARGER;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_QC_9V_CHARGER:
		switch (trans) {
		case HV_TRANS_MUIC_DETACH:
			next_state = HV_STATE_IDLE;
			break;
        case HV_TRANS_VBUS_REDUCE:
			next_state = HV_STATE_QC_5V_CHARGER;
			break;
		default:
			skip_trans = true;
			break;
		}
		break;
	case HV_STATE_INVALID:
	case HV_STATE_MAX_NUM:
	default:
		skip_trans = true;
		break;
	}

	if (skip_trans) {
		pr_info("%s skip to transaction - state(%d), trans(%d)\n", __func__, cur_state, trans);
	} else {
		pr_info("%s state(%d->%d), trans(%d)\n", __func__, cur_state, next_state, trans);
		muic_core_hv_handle_state(muic_pdata, next_state);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(muic_core_hv_state_manager);

void muic_core_hv_init(struct muic_platform_data *muic_pdata)
{
	struct muic_interface_t *muic_if;

	if (muic_pdata == NULL)
		return;

	muic_if = muic_pdata->muic_if;

	muic_pdata->hv_state = HV_STATE_IDLE;
	MUIC_PDATA_VOID_FUNC(muic_if->hv_reset, muic_pdata->drv_data);
}
EXPORT_SYMBOL_GPL(muic_core_hv_init);

#ifdef CONFIG_HV_MUIC_VOLTAGE_CTRL
void hv_muic_change_afc_voltage(int tx_data)
{
	struct muic_interface_t *muic_if;

	if (static_pdata == NULL)
		return;
	muic_if = static_pdata->muic_if;

	muic_if->change_afc_voltage(static_pdata, tx_data);
}
EXPORT_SYMBOL_GPL(hv_muic_change_afc_voltage);

int muic_afc_get_voltage(void)
{
	struct muic_interface_t *muic_if;
	int ret;

	if (static_pdata == NULL)
		return -1;
	muic_if = static_pdata->muic_if;

	ret = muic_if->afc_get_voltage(static_pdata);

	return ret;
}
EXPORT_SYMBOL_GPL(muic_afc_get_voltage);

int muic_afc_set_voltage(int vol)
{
	struct muic_interface_t *muic_if;
	int ret;

	if (static_pdata == NULL)
		return -1;
	muic_if = static_pdata->muic_if;

	ret = muic_if->afc_set_voltage(static_pdata, vol);

	return ret;

}
EXPORT_SYMBOL_GPL(muic_afc_set_voltage);
#endif /* CONFIG_HV_MUIC_VOLTAGE_CTRL */
#endif

struct muic_platform_data *muic_core_init(void *drv_data)
{
	struct muic_platform_data *muic_pdata;

	muic_pdata = kzalloc(sizeof(*muic_pdata), GFP_KERNEL);
	if (unlikely(!muic_pdata)) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		return NULL;
	}

	muic_pdata->drv_data = drv_data;
	muic_pdata->attached_dev = ATTACHED_DEV_NONE_MUIC;
	muic_pdata->cleanup_switch_dev_cb = muic_cleanup_switch_dev_cb;
	muic_pdata->is_usb_ready = false;
	muic_pdata->is_factory_start = false;
	muic_pdata->is_rustproof = muic_pdata->rustproof_on;
	muic_pdata->init_gpio_cb = muic_init_gpio_cb;
	muic_pdata->jig_uart_cb = muic_jig_uart_cb,
#if defined(CONFIG_MUIC_HV)
	muic_pdata->hv_state = HV_STATE_IDLE;
#endif
	muic_init_switch_dev_cb();

	static_pdata = muic_pdata;

	return muic_pdata;
}

void muic_core_exit(struct muic_platform_data *muic_pdata)
{
	kfree(muic_pdata);
}
