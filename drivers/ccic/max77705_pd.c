/*
 * Copyrights (C) 2017 Samsung Electronics, Inc.
 * Copyrights (C) 2017 Maxim Integrated Products, Inc.
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
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/mfd/max77705-private.h>
#include <linux/platform_device.h>
#include <linux/ccic/max77705_usbc.h>
#if defined(CONFIG_BATTERY_NOTIFIER)
#include <linux/battery/battery_notifier.h>
#endif
#if defined(CONFIG_CCIC_NOTIFIER)
#include <linux/ccic/ccic_core.h>
#include <linux/ccic/ccic_notifier.h>
#include <linux/ccic/max77705_alternate.h>
#endif
#if defined(CONFIG_USB_HOST_NOTIFY)
#include <linux/usb_notify.h>
#endif
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
#include <linux/usb/class-dual-role.h>
#elif defined(CONFIG_TYPEC)
#include <linux/usb/typec.h>
#endif
#include "../battery_v2/include/sec_charging_common.h"

extern struct pdic_notifier_struct pd_noti;
extern void (*fp_select_pdo)(int num);

unsigned int pn_flag;
EXPORT_SYMBOL(pn_flag);

static void max77705_process_pd(struct max77705_usbc_platform_data *usbc_data)
{
	int i;

	if (usbc_data->pd_data->cc_sbu_short) {
		pd_noti.sink_status.available_pdo_num = 1;
		pd_noti.sink_status.power_list[1].max_current =
			pd_noti.sink_status.power_list[1].max_current > 1800 ?
			1800 : pd_noti.sink_status.power_list[1].max_current;
	}

	pr_info("%s : CURRENT PDO NUM(%d), available_pdo_num[%d]",
		__func__, pd_noti.sink_status.current_pdo_num,
		pd_noti.sink_status.available_pdo_num);

	for (i = 1; i <= pd_noti.sink_status.available_pdo_num; i++) {
		pr_info("%s : PDO_Num[%d] MAX_CURR(%d) MAX_VOLT(%d)\n", __func__,
			i, pd_noti.sink_status.power_list[i].max_current,
			pd_noti.sink_status.power_list[i].max_voltage);
	}
	max77705_ccic_event_work(usbc_data, CCIC_NOTIFY_DEV_BATTERY,
		CCIC_NOTIFY_ID_POWER_STATUS, 1/*attach*/, 0, 0);
}

void max77705_select_pdo(int num)
{
	struct max77705_usbc_platform_data *pusbpd = pd_noti.pusbpd;
	usbc_cmd_data value;
	u8 temp;

	init_usbc_cmd_data(&value);
	pr_info("%s : NUM(%d)\n", __func__, num);

	temp = num;

	pd_noti.sink_status.selected_pdo_num = num;
	if (num != 1)
		pn_flag = 1;

	if (pd_noti.event != PDIC_NOTIFY_EVENT_PD_SINK)
		pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK;

	if (pd_noti.sink_status.current_pdo_num == pd_noti.sink_status.selected_pdo_num) {
		max77705_process_pd(pusbpd);
	} else {
		value.opcode = OPCODE_SRCCAP_REQUEST;
		value.write_data[0] = temp;
		value.write_length = 1;
		value.read_length = 0;
		max77705_usbc_opcode_write(pusbpd, &value);
	}

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d) NUM(%d)\n",
		__func__, value.opcode, value.write_length, value.read_length, num);
}

void max77705_usbc_icurr(u8 curr)
{
	usbc_cmd_data value;

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_CHGIN_ILIM_W;
	value.write_data[0] = curr;
	value.write_length = 1;
	value.read_length = 0;
	max77705_usbc_opcode_write(pd_noti.pusbpd, &value);

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d) USBC_ILIM(0x%x)\n",
		__func__, value.opcode, value.write_length, value.read_length, curr);

}

void max77705_set_fw_noautoibus(int enable)
{
	struct max77705_usbc_platform_data *pusbpd = pd_noti.pusbpd;
	usbc_cmd_data value;
	u8 op_data;

	switch (enable) {
	case MAX77705_AUTOIBUS_FW_AT_OFF:
		op_data = 0x03; /* usbc fw off & auto off(manual on) */
		break;
	case MAX77705_AUTOIBUS_FW_OFF:
		op_data = 0x02; /* usbc fw off & auto on(manual off) */
		break;
	case MAX77705_AUTOIBUS_AT_OFF:
		op_data = 0x01; /* usbc fw on & auto off(manual on) */
		break;
	case MAX77705_AUTOIBUS_ON:
	default:
		op_data = 0x00; /* usbc fw on & auto on(manual off) */
		break;
	}

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_SAMSUNG_FW_AUTOIBUS;
	value.write_data[0] = op_data;
	value.write_length = 1;
	value.read_length = 0;
	max77705_usbc_opcode_write(pusbpd, &value);

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d) AUTOIBUS(0x%x)\n",
		__func__, value.opcode, value.write_length, value.read_length, op_data);
}

void max77705_vbus_turn_on_ctrl(struct max77705_usbc_platform_data *usbc_data, bool enable, bool swaped)
{
	struct power_supply *psy_otg;
	union power_supply_propval val;
	int on = !!enable;
	int ret = 0;
#if defined(CONFIG_USB_HOST_NOTIFY)
	struct otg_notify *o_notify = get_otg_notify();
	bool must_block_host = is_blocked(o_notify, NOTIFY_BLOCK_TYPE_HOST);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int event;
#endif

	pr_info("%s : enable=%d, auto_vbus_en=%d, must_block_host=%d, swaped=%d\n",
		__func__, enable, usbc_data->auto_vbus_en, must_block_host, swaped);
	// turn on
	if (enable) {
		// auto-mode
		if (usbc_data->auto_vbus_en) {
			// mpsm
			if (must_block_host) {
				if (swaped) {
					// turn off vbus because of swaped and blocked host
					enable = false;
					pr_info("%s : turn off vbus because of blocked host\n",
						__func__);
				} else {
					// don't turn on because of blocked host
					return;
				}
			} else {
				// don't turn on because of auto-mode
				return;
			}
		// non auto-mode
		} else {
			if (must_block_host) {
				if (swaped) {
					enable = false;
					pr_info("%s : turn off vbus because of blocked host\n",
						__func__);
				} else {
					// don't turn on because of blocked host
					return;
				}
			} else {
				// turn on vbus go down
				if (usbc_data->muic_data->fake_chgtyp == CHGTYP_DEDICATED_CHARGER) {
					// this case is USB Killer.
					pr_info("%s : do not turn on VBUS because of USB Killer.\n",
						__func__);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
					event = NOTIFY_EXTRA_USBKILLER;
					store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
#if defined(CONFIG_USB_HW_PARAM)
					if (o_notify)
						inc_hw_param(o_notify, USB_CCIC_USB_KILLER_COUNT);
#endif
					return;
				}
			}
		}
	// turn off
	} else {
		// don't turn off because of auto-mode or blocked (already off)
		if (usbc_data->auto_vbus_en || must_block_host)
			return;
	}
#endif

	pr_info("%s : enable=%d\n", __func__, enable);

	psy_otg = power_supply_get_by_name("otg");
	if (psy_otg) {
		val.intval = enable;
		ret = psy_otg->desc->set_property(psy_otg, POWER_SUPPLY_PROP_ONLINE, &val);
	} else {
		pr_err("%s: Fail to get psy battery\n", __func__);
	}
	if (ret) {
		pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);
	} else {
		pr_info("otg accessory power = %d\n", on);
	}

}

void max77705_pdo_list(struct max77705_usbc_platform_data *usbc_data, unsigned char *data)
{
	u8 temp = 0x00;
	int i;

	temp = (data[1] >> 5);

#if defined(CONFIG_BATTERY_NOTIFIER)
	if (temp > MAX_PDO_NUM) {
		pr_info("%s : update available_pdo_num[%d -> %d]",
			__func__, temp, MAX_PDO_NUM);
		temp = MAX_PDO_NUM;
	}
#endif

	pd_noti.sink_status.available_pdo_num = temp;
	pr_info("%s : Temp[0x%02x] Data[0x%02x] available_pdo_num[%d]\n",
		__func__, temp, data[1], pd_noti.sink_status.available_pdo_num);

	for (i = 0; i < temp; i++) {
		u32 pdo_temp;
		int max_value = 0;

		pdo_temp = (data[2 + (i * 4)]
			| (data[3 + (i * 4)] << 8)
			| (data[4 + (i * 4)] << 16)
			| (data[5 + (i * 4)] << 24));

		pr_info("%s : PDO[%d] = 0x%x\n", __func__, i, pdo_temp);

		max_value = (0x3FF & pdo_temp);
		pd_noti.sink_status.power_list[i + 1].max_current = max_value * 10;

		max_value = (0x3FF & (pdo_temp >> 10));
		pd_noti.sink_status.power_list[i + 1].max_voltage = max_value * 50;

		pr_info("%s : PDO_Num[%d] MAX_CURR(%d) MAX_VOLT(%d)\n", __func__,
				i, pd_noti.sink_status.power_list[i + 1].max_current,
				pd_noti.sink_status.power_list[i + 1].max_voltage);
	}
	usbc_data->pd_data->pdo_list = true;
	pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK_CAP;

	max77705_process_pd(usbc_data);
}

void max77705_current_pdo(struct max77705_usbc_platform_data *usbc_data, unsigned char *data)
{
	u8 temp = 0x00;
	U_SEC_PDO_OBJECT pdo_obj;
	int i;

	temp = ((data[1] >> 3) & 0x07);
	pd_noti.sink_status.current_pdo_num = temp;
	pr_info("%s : CURRENT PDO NUM(%d)\n",
		__func__, pd_noti.sink_status.current_pdo_num);

	temp = (data[1] & 0x07);
#if defined(CONFIG_BATTERY_NOTIFIER)
	if (temp > MAX_PDO_NUM) {
		pr_info("%s : update available_pdo_num[%d -> %d]",
			__func__, temp, MAX_PDO_NUM);
		temp = MAX_PDO_NUM;
	}
#endif

	pd_noti.sink_status.available_pdo_num = temp;
	pr_info("%s : Temp[0x%02x] Data[0x%02x] available_pdo_num[%d]\n",
			__func__, temp, data[1], pd_noti.sink_status.available_pdo_num);
	for (i = 0; i < temp; i++) {
		int max_value = 0;

		pdo_obj.data = (data[2 + (i * 4)]
			| (data[3 + (i * 4)] << 8)
			| (data[4 + (i * 4)] << 16)
			| (data[5 + (i * 4)] << 24));

		if (pdo_obj.BITS_supply.type != PDO_TYPE_FIXED) {
			pr_info("%s : Skip PDO TYPE(%d)\n", __func__, pdo_obj.BITS_supply.type);
			continue;
		}

		pr_info("%s : PDO[%d] = 0x%x\n", __func__, i, pdo_obj.data);

		max_value = (0x3FF & pdo_obj.data);
		pd_noti.sink_status.power_list[i + 1].max_current = max_value * 10;

		max_value = (0x3FF & (pdo_obj.data >> 10));
		pd_noti.sink_status.power_list[i + 1].max_voltage = max_value * 50;

		pr_info("%s : PDO_Num[%d] MAX_CURR(%d) MAX_VOLT(%d)\n", __func__,
				i, pd_noti.sink_status.power_list[i + 1].max_current,
				pd_noti.sink_status.power_list[i + 1].max_voltage);
	}
	usbc_data->pd_data->pdo_list = true;
	pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK_CAP;

	max77705_process_pd(usbc_data);
}

void max77705_detach_pd(struct max77705_usbc_platform_data *usbc_data)
{
	pr_info("%s : Detach PD CHARGER\n", __func__);

	if (pd_noti.event != PDIC_NOTIFY_EVENT_DETACH) {
		pd_noti.sink_status.rp_currentlvl = RP_CURRENT_LEVEL_NONE;
		pd_noti.sink_status.selected_pdo_num = 0;
		pd_noti.sink_status.available_pdo_num = 0;
		pd_noti.sink_status.current_pdo_num = 0;
		pd_noti.event = PDIC_NOTIFY_EVENT_DETACH;
		usbc_data->pd_data->psrdy_received = false;
		usbc_data->pd_data->pdo_list = false;
		usbc_data->pd_data->cc_sbu_short = false;
		max77705_ccic_event_work(usbc_data, CCIC_NOTIFY_DEV_BATTERY,
			CCIC_NOTIFY_ID_POWER_STATUS, 0/*attach*/, 0, 0);
	}
}

static void max77705_notify_prswap(struct max77705_usbc_platform_data *usbc_data, u8 pd_msg)
{
	pr_info("%s : PR SWAP pd_msg [%x]\n", __func__, pd_msg);

	switch(pd_msg) {
	case PRSWAP_SNKTOSWAP:
		pd_noti.event = PDIC_NOTIFY_EVENT_PD_PRSWAP_SNKTOSRC;
		pd_noti.sink_status.selected_pdo_num = 0;
		pd_noti.sink_status.available_pdo_num = 0;
		pd_noti.sink_status.current_pdo_num = 0;
		usbc_data->pd_data->psrdy_received = false;
		usbc_data->pd_data->pdo_list = false;
		usbc_data->pd_data->cc_sbu_short = false;
		max77705_ccic_event_work(usbc_data, CCIC_NOTIFY_DEV_BATTERY,
			CCIC_NOTIFY_ID_POWER_STATUS, 0/*attach*/, 0, 0);
		break;
	default:
		break;
	}
}

static void max77705_check_pdo(struct max77705_usbc_platform_data *usbc_data)
{
	usbc_cmd_data value;

	init_usbc_cmd_data(&value);
	value.opcode = OPCODE_CURRENT_SRCCAP;
	value.write_length = 0x0;
	value.read_length = 31;
	max77705_usbc_opcode_read(usbc_data, &value);

	pr_info("%s : OPCODE(0x%02x) W_LENGTH(%d) R_LENGTH(%d)\n",
		__func__, value.opcode, value.write_length, value.read_length);

}

void max77705_notify_rp_current_level(struct max77705_usbc_platform_data *usbc_data)
{
	unsigned int rp_currentlvl;

	switch (usbc_data->cc_data->ccistat) {
	case CCI_500mA:
		rp_currentlvl = RP_CURRENT_LEVEL_DEFAULT;
		break;
	case CCI_1_5A:
		rp_currentlvl = RP_CURRENT_LEVEL2;
		break;
	case CCI_3_0A:
		rp_currentlvl = RP_CURRENT_LEVEL3;
		break;
	case CCI_SHORT:
		rp_currentlvl = RP_CURRENT_ABNORMAL;
		break;
	default:
		rp_currentlvl = RP_CURRENT_LEVEL_NONE;
		break;
	}

	if (usbc_data->plug_attach_done && !usbc_data->pd_data->psrdy_received &&
		usbc_data->cc_data->current_pr == SNK &&
		usbc_data->pd_state == max77705_State_PE_SNK_Wait_for_Capabilities &&
		rp_currentlvl != pd_noti.sink_status.rp_currentlvl &&
		rp_currentlvl >= RP_CURRENT_LEVEL_DEFAULT) {
		pd_noti.sink_status.rp_currentlvl = rp_currentlvl;
		pd_noti.event = PDIC_NOTIFY_EVENT_CCIC_ATTACH;
		pr_info("%s : rp_currentlvl(%d)\n", __func__, pd_noti.sink_status.rp_currentlvl);
		max77705_ccic_event_work(usbc_data, CCIC_NOTIFY_DEV_BATTERY,
			CCIC_NOTIFY_ID_POWER_STATUS, 0/*attach*/, 0, 0);
	}
}

static void max77705_pd_check_pdmsg(struct max77705_usbc_platform_data *usbc_data, u8 pd_msg)
{
	struct power_supply *psy_charger;
	union power_supply_propval val;
	/*int dr_swap, pr_swap, vcon_swap = 0; u8 value[2], rc = 0;*/
	MAX77705_VDM_MSG_IRQ_STATUS_Type VDM_MSG_IRQ_State;
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int event;
#endif

	VDM_MSG_IRQ_State.DATA = 0x0;
	msg_maxim(" pd_msg [%x]", pd_msg);

	switch (pd_msg) {
	case Nothing_happened:
		break;
	case Sink_PD_PSRdy_received:
		/* currently, do nothing
		 * calling max77705_check_pdo() has been moved to max77705_psrdy_irq()
		 * for specific PD charger issue
		 */
		break;
	case Sink_PD_Error_Recovery:
		break;
	case Sink_PD_SenderResponseTimer_Timeout:
		msg_maxim("Sink_PD_SenderResponseTimer_Timeout received.");
	/*	queue_work(usbc_data->op_send_queue, &usbc_data->op_send_work); */
		break;
	case Source_PD_PSRdy_Sent:
		if (usbc_data->mpsm_mode && (usbc_data->pd_pr_swap == cc_SOURCE)) {
			max77705_usbc_disable_auto_vbus(usbc_data);
			max77705_vbus_turn_on_ctrl(usbc_data, OFF, false);
		}
		break;
	case Source_PD_Error_Recovery:
		break;
	case Source_PD_SenderResponseTimer_Timeout:
		max77705_vbus_turn_on_ctrl(usbc_data, OFF, false);
		schedule_delayed_work(&usbc_data->vbus_hard_reset_work, msecs_to_jiffies(800));
		break;
	case PD_DR_Swap_Request_Received:
		msg_maxim("DR_SWAP received.");
		/* currently, do nothing
		 * calling max77705_check_pdo() has been moved to max77705_psrdy_irq()
		 * for specific PD charger issue
		 */
		if (usbc_data->dr_swap_cnt < INT_MAX)
			usbc_data->dr_swap_cnt++;
		break;
	case PD_PR_Swap_Request_Received:
		msg_maxim("PR_SWAP received.");
		break;
	case PD_VCONN_Swap_Request_Received:
		msg_maxim("VCONN_SWAP received.");
		break;
	case Received_PD_Message_in_illegal_state:
		break;
	case Samsung_Accessory_is_attached:
		break;
	case VDM_Attention_message_Received:
		break;
	case Sink_PD_Disabled:
#if 0
		/* to do */
		/* AFC HV */
		value[0] = 0x20;
		rc = max77705_ccpd_write_command(chip, value, 1);
		if (rc > 0)
			pr_err("failed to send command\n");
#endif
		break;
	case Source_PD_Disabled:
		break;
	case Prswap_Snktosrc_Sent:
		usbc_data->pd_pr_swap = cc_SOURCE;
		break;
	case Prswap_Srctosnk_Sent:
		usbc_data->pd_pr_swap = cc_SINK;
		break;
	case HARDRESET_RECEIVED:
		/*turn off the vbus both Source and Sink*/
		if (usbc_data->cc_data->current_pr == SRC) {
			max77705_vbus_turn_on_ctrl(usbc_data, OFF, false);
			schedule_delayed_work(&usbc_data->vbus_hard_reset_work, msecs_to_jiffies(760));
		}
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
		event = NOTIFY_EXTRA_HARDRESET_RECEIVED;
		store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
		break;
	case HARDRESET_SENT:
		/*turn off the vbus both Source and Sink*/
		if (usbc_data->cc_data->current_pr == SRC) {
			max77705_vbus_turn_on_ctrl(usbc_data, OFF, false);
			schedule_delayed_work(&usbc_data->vbus_hard_reset_work, msecs_to_jiffies(760));
		}
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
		event = NOTIFY_EXTRA_HARDRESET_SENT;
		store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
		break;
	case Get_Vbus_turn_on:
		break;
	case Get_Vbus_turn_off:
		max77705_vbus_turn_on_ctrl(usbc_data, OFF, false);
		break;
	case PRSWAP_SRCTOSWAP:
	case PRSWAP_SWAPTOSNK:
		max77705_vbus_turn_on_ctrl(usbc_data, OFF, false);
		msg_maxim("PRSWAP_SRCTOSNK : [%x]", pd_msg);
		break;
	case PRSWAP_SNKTOSWAP:
		msg_maxim("PRSWAP_SNKTOSWAP : [%x]", pd_msg);
		max77705_notify_prswap(usbc_data, PRSWAP_SNKTOSWAP);
		/* CHGINSEL disable */
		psy_charger = power_supply_get_by_name("max77705-charger");
		if (psy_charger) {
			val.intval = 0;
			psy_charger->desc->set_property(psy_charger,
				(enum power_supply_property) POWER_SUPPLY_EXT_PROP_CHGINSEL, &val);
		} else {
			pr_err("%s: Fail to get psy charger\n", __func__);
		}
		break;
	case PRSWAP_SWAPTOSRC:
		max77705_notify_prswap(usbc_data, PRSWAP_SNKTOSWAP);
		max77705_vbus_turn_on_ctrl(usbc_data, ON, false);
		msg_maxim("PRSWAP_SNKTOSRC : [%x]", pd_msg);
		break;
	case Current_Cable_Connected:
		max77705_manual_jig_on(usbc_data, 1);
		usbc_data->manual_lpm_mode = 1;
		msg_maxim("Current_Cable_Connected : [%x]", pd_msg);
		break;
	default:
		break;
	}
}

static void max77705_pd_rid(struct max77705_usbc_platform_data *usbc_data, u8 fct_id)
{
	struct max77705_pd_data *pd_data = usbc_data->pd_data;

	u8 prev_rid = pd_data->device;
#if defined(CONFIG_CCIC_NOTIFIER)
	static int rid = RID_OPEN;
#endif

	switch (fct_id) {
	case FCT_GND:
		msg_maxim(" RID_000K");
		pd_data->device = DEV_FCT_0;
#if defined(CONFIG_CCIC_NOTIFIER)
		rid = RID_000K;
#endif
		break;
	case FCT_1Kohm:
		msg_maxim(" RID_001K");
		pd_data->device = DEV_FCT_1K;
#if defined(CONFIG_CCIC_NOTIFIER)
		rid = RID_001K;
#endif
		break;
	case FCT_255Kohm:
		msg_maxim(" RID_255K");
		pd_data->device = DEV_FCT_255K;
#if defined(CONFIG_CCIC_NOTIFIER)
		rid = RID_255K;
#endif
		break;
	case FCT_301Kohm:
		msg_maxim(" RID_301K");
		pd_data->device = DEV_FCT_301K;
#if defined(CONFIG_CCIC_NOTIFIER)
		rid = RID_301K;
#endif
		break;
	case FCT_523Kohm:
		msg_maxim(" RID_523K");
		pd_data->device = DEV_FCT_523K;
#if defined(CONFIG_CCIC_NOTIFIER)
		rid = RID_523K;
#endif
		break;
	case FCT_619Kohm:
		msg_maxim(" RID_619K");
		pd_data->device = DEV_FCT_619K;
#if defined(CONFIG_CCIC_NOTIFIER)
		rid = RID_619K;
#endif
		break;
	case FCT_OPEN:
		msg_maxim(" RID_OPEN");
		pd_data->device = DEV_FCT_OPEN;
#if defined(CONFIG_CCIC_NOTIFIER)
		rid = RID_OPEN;
#endif
		break;
	default:
		msg_maxim(" RID_UNDEFINED");
		pd_data->device = DEV_UNKNOWN;
#if defined(CONFIG_CCIC_NOTIFIER)
		rid = RID_UNDEFINED;
#endif
		break;
	}

	if (prev_rid != pd_data->device) {
#if defined(CONFIG_CCIC_NOTIFIER)
		/* RID */
		max77705_ccic_event_work(usbc_data,
			CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_RID,
			rid, 0, 0);
		usbc_data->cur_rid = rid;
		/* turn off USB */
		if (pd_data->device == DEV_FCT_OPEN || pd_data->device == DEV_UNKNOWN
			|| pd_data->device == DEV_FCT_523K || pd_data->device == DEV_FCT_619K) {
#if defined(CONFIG_DUAL_ROLE_USB_INTF)
			usbc_data->power_role = DUAL_ROLE_PROP_PR_NONE;
#elif defined(CONFIG_TYPEC)
			usbc_data->typec_power_role = TYPEC_SINK;

#endif
			/* usb or otg */
			max77705_ccic_event_work(usbc_data,
				CCIC_NOTIFY_DEV_USB, CCIC_NOTIFY_ID_USB,
				0/*attach*/, USB_STATUS_NOTIFY_DETACH/*drp*/, 0);
		}
#endif
	}
}

static irqreturn_t max77705_pdmsg_irq(int irq, void *data)
{
	struct max77705_usbc_platform_data *usbc_data = data;
	struct max77705_pd_data *pd_data = usbc_data->pd_data;
	u8 pdmsg = 0;

	max77705_read_reg(usbc_data->muic, REG_PD_STATUS0, &pd_data->pd_status0);
	pdmsg = pd_data->pd_status0;
	pr_info("%s: IRQ(%d)_IN\n", __func__, irq);
	max77705_pd_check_pdmsg(usbc_data, pdmsg);
	pd_data->pdsmg = pdmsg;
	pr_info("%s: IRQ(%d)_OUT\n", __func__, irq);

	return IRQ_HANDLED;
}

static irqreturn_t max77705_psrdy_irq(int irq, void *data)
{
	struct max77705_usbc_platform_data *usbc_data = data;
	u8 psrdy_received = 0;
#if defined(CONFIG_TYPEC)
	enum typec_pwr_opmode mode = TYPEC_PWR_MODE_USB;
#endif
	msg_maxim("IN");
	max77705_read_reg(usbc_data->muic, REG_PD_STATUS1, &usbc_data->pd_status1);
	psrdy_received = (usbc_data->pd_status1 & BIT_PD_PSRDY)
			>> FFS(BIT_PD_PSRDY);

	if (psrdy_received && !usbc_data->pd_support
			&& usbc_data->pd_data->cc_status != CC_NO_CONN)
		usbc_data->pd_support = true;
#if defined(CONFIG_TYPEC)
	if (usbc_data->typec_try_state_change == TRY_ROLE_SWAP_PR &&
		usbc_data->pd_support) {
		/* Role change try and new mode detected */
		msg_maxim("typec_reverse_completion");
		usbc_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
		complete(&usbc_data->typec_reverse_completion);
	}
	mode = max77705_get_pd_support(usbc_data);
	typec_set_pwr_opmode(usbc_data->port, mode);
#endif
	msg_maxim("psrdy_received=%d, usbc_data->pd_support=%d, cc_status=%d",
		psrdy_received, usbc_data->pd_support, usbc_data->pd_data->cc_status);

	if (usbc_data->pd_data->cc_status == CC_SNK) {
		max77705_check_pdo(usbc_data);
		usbc_data->pd_data->psrdy_received = true;
		pn_flag = 0;
	}
	msg_maxim("OUT");
	return IRQ_HANDLED;
}

static void max77705_datarole_irq_handler(void *data, int irq)
{
	struct max77705_usbc_platform_data *usbc_data = data;
	struct max77705_pd_data *pd_data = usbc_data->pd_data;
	u8 datarole = 0;

	max77705_read_reg(usbc_data->muic, REG_PD_STATUS1, &pd_data->pd_status1);
	datarole = (pd_data->pd_status1 & BIT_PD_DataRole)
			>> FFS(BIT_PD_DataRole);
	/* abnormal data role without setting power role */
	if (usbc_data->cc_data->current_pr == 0xFF) {
		msg_maxim("INVALID IRQ IRQ(%d)_OUT", irq);
		return;
	}

	if (irq == CCIC_IRQ_INIT_DETECT) {
		if (usbc_data->pd_data->cc_status == CC_SNK)
			msg_maxim("initial time : SNK");
		else
			return;
	}

	switch (datarole) {
	case UFP:
			if (pd_data->current_dr != UFP) {
				pd_data->previous_dr = pd_data->current_dr;
				pd_data->current_dr = UFP;
				if (pd_data->previous_dr != 0xFF)
					msg_maxim("%s detach previous usb connection\n", __func__);
				max77705_notify_dr_status(usbc_data, 1);
#if defined(CONFIG_TYPEC) 
				if (usbc_data->typec_try_state_change == TRY_ROLE_SWAP_DR ||
					usbc_data->typec_try_state_change == TRY_ROLE_SWAP_TYPE) {
					/* Role change try and new mode detected */
					msg_maxim("typec_reverse_completion");
					usbc_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
					complete(&usbc_data->typec_reverse_completion);
				}
#endif 
			}
			msg_maxim(" UFP");
			break;

	case DFP:
			if (pd_data->current_dr != DFP) {
				pd_data->previous_dr = pd_data->current_dr;
				pd_data->current_dr = DFP;
				if (pd_data->previous_dr != 0xFF)
					msg_maxim("%s detach previous usb connection\n", __func__);

				max77705_notify_dr_status(usbc_data, 1);
#if defined(CONFIG_TYPEC) 
				if (usbc_data->typec_try_state_change == TRY_ROLE_SWAP_DR ||
					usbc_data->typec_try_state_change == TRY_ROLE_SWAP_TYPE) {
					/* Role change try and new mode detected */
					msg_maxim("typec_reverse_completion");
					usbc_data->typec_try_state_change = TRY_ROLE_SWAP_NONE;
					complete(&usbc_data->typec_reverse_completion);
				}
#endif 
				if (usbc_data->cc_data->current_pr == SNK && !(usbc_data->is_first_booting)) {
					max77705_vdm_process_set_identity_req(usbc_data);
					msg_maxim("SEND THE IDENTITY REQUEST FROM DFP HANDLER");
				}
			}
			msg_maxim(" DFP");
			break;

	default:
			msg_maxim(" DATAROLE(Never Call this routine)");
			break;
	}
}

static irqreturn_t max77705_datarole_irq(int irq, void *data)
{
	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	max77705_datarole_irq_handler(data, irq);
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);
	return IRQ_HANDLED;
}

static irqreturn_t max77705_ssacc_irq(int irq, void *data)
{
	struct max77705_usbc_platform_data *usbc_data = data;
	struct max77705_pd_data *pd_data = usbc_data->pd_data;

	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	msg_maxim(" SSAcc command received");
	/* Read through Opcode command 0x50 */
	pd_data->ssacc = 1;
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);
	return IRQ_HANDLED;
}

static void max77705_check_cc_sbu_short(void *data)
{
	u8 cc_status1 = 0;

	struct max77705_usbc_platform_data *usbc_data = data;
	struct max77705_pd_data *pd_data = usbc_data->pd_data;

	max77705_read_reg(usbc_data->muic, REG_CC_STATUS1, &cc_status1);
	/* 0b01: CC-5V, 0b10: SBU-5V, 0b11: SBU-GND Short */
	cc_status1 = (cc_status1 & BIT_CCSBUSHORT) >> FFS(BIT_CCSBUSHORT);
	if (cc_status1)
		pd_data->cc_sbu_short = true;

	msg_maxim("%s cc_status1 : %x, cc_sbu_short : %d\n", __func__, cc_status1, pd_data->cc_sbu_short);
}

static u8 max77705_check_rid(void *data)
{
	u8 fct_id = 0;
	struct max77705_usbc_platform_data *usbc_data = data;
	struct max77705_pd_data *pd_data = usbc_data->pd_data;

	max77705_read_reg(usbc_data->muic, REG_PD_STATUS1, &pd_data->pd_status1);
	fct_id = (pd_data->pd_status1 & BIT_FCT_ID) >> FFS(BIT_FCT_ID);
#if defined(CONFIG_SEC_FACTORY)
	factory_execute_monitor(FAC_ABNORMAL_REPEAT_RID);
#endif
	max77705_pd_rid(usbc_data, fct_id);
	pd_data->fct_id = fct_id;
	msg_maxim("%s rid : %d, fct_id : %d\n", __func__, usbc_data->cur_rid, fct_id);
	return fct_id;
}

static irqreturn_t max77705_fctid_irq(int irq, void *data)
{
	pr_debug("%s: IRQ(%d)_IN\n", __func__, irq);
	max77705_check_rid(data);
	pr_debug("%s: IRQ(%d)_OUT\n", __func__, irq);
	return IRQ_HANDLED;
}

int max77705_pd_init(struct max77705_usbc_platform_data *usbc_data)
{
	struct max77705_pd_data *pd_data = NULL;
	int ret;

	msg_maxim(" IN");
	pd_data = usbc_data->pd_data;
	pd_noti.pusbpd = usbc_data;

	pd_noti.sink_status.rp_currentlvl = RP_CURRENT_LEVEL_NONE;
	pd_noti.sink_status.available_pdo_num = 0;
	pd_noti.sink_status.selected_pdo_num = 0;
	pd_noti.sink_status.current_pdo_num = 0;
	pd_noti.event = PDIC_NOTIFY_EVENT_DETACH;
	pd_data->pdo_list = false;
	pd_data->psrdy_received = false;
	pd_data->cc_sbu_short = false;

	fp_select_pdo = max77705_select_pdo;

	wake_lock_init(&pd_data->pdmsg_wake_lock, WAKE_LOCK_SUSPEND,
			   "pd->pdmsg");
	wake_lock_init(&pd_data->datarole_wake_lock, WAKE_LOCK_SUSPEND,
			   "pd->datarole");
	wake_lock_init(&pd_data->ssacc_wake_lock, WAKE_LOCK_SUSPEND,
			   "pd->ssacc");
	wake_lock_init(&pd_data->fct_id_wake_lock, WAKE_LOCK_SUSPEND,
			   "pd->fctid");

	pd_data->irq_pdmsg = usbc_data->irq_base + MAX77705_PD_IRQ_PDMSG_INT;
	if (pd_data->irq_pdmsg) {
		ret = request_threaded_irq(pd_data->irq_pdmsg,
			   NULL, max77705_pdmsg_irq,
			   0,
			   "pd-pdmsg-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}

	pd_data->irq_psrdy = usbc_data->irq_base + MAX77705_PD_IRQ_PS_RDY_INT;
	if (pd_data->irq_psrdy) {
		ret = request_threaded_irq(pd_data->irq_psrdy,
			   NULL, max77705_psrdy_irq,
			   0,
			   "pd-psrdy-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}

	pd_data->irq_datarole = usbc_data->irq_base + MAX77705_PD_IRQ_DATAROLE_INT;
	if (pd_data->irq_datarole) {
		ret = request_threaded_irq(pd_data->irq_datarole,
			   NULL, max77705_datarole_irq,
			   0,
			   "pd-datarole-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}

	pd_data->irq_ssacc = usbc_data->irq_base + MAX77705_PD_IRQ_SSACCI_INT;
	if (pd_data->irq_ssacc) {
		ret = request_threaded_irq(pd_data->irq_ssacc,
			   NULL, max77705_ssacc_irq,
			   0,
			   "pd-ssacci-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}
	pd_data->irq_fct_id = usbc_data->irq_base + MAX77705_PD_IRQ_FCTIDI_INT;
	if (pd_data->irq_fct_id) {
		ret = request_threaded_irq(pd_data->irq_fct_id,
			   NULL, max77705_fctid_irq,
			   0,
			   "pd-fctid-irq", usbc_data);
		if (ret) {
			pr_err("%s: Failed to Request IRQ (%d)\n", __func__, ret);
			goto err_irq;
		}
	}
	/* check RID value for booting time */
	max77705_check_rid(usbc_data);
	max77705_set_fw_noautoibus(MAX77705_AUTOIBUS_AT_OFF);
	/* check CC Pin state for cable attach booting scenario */
	max77705_datarole_irq_handler(usbc_data, CCIC_IRQ_INIT_DETECT);
	max77705_check_cc_sbu_short(usbc_data);

	msg_maxim(" OUT");
	return 0;

err_irq:
	kfree(pd_data);
	return ret;
}
