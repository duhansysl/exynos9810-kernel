/*
*	USB PD Driver - Device Policy Manager
*/

#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ccic/usbpd.h>

#include <linux/of_gpio.h>

#include <linux/muic/muic.h>
#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */
#include <linux/ccic/pdic_notifier.h>
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
#include <linux/battery/battery_notifier.h>
#endif
#endif

#if defined(CONFIG_CCIC_NOTIFIER)
#include <linux/ccic/ccic_notifier.h>
#endif

#if (defined CONFIG_CCIC_NOTIFIER || defined CONFIG_DUAL_ROLE_USB_INTF)
#include <linux/ccic/usbpd_ext.h>
#endif

/* switch device header */
#if defined(CONFIG_SWITCH)
#include <linux/switch.h>
#endif /* CONFIG_SWITCH */

#ifdef CONFIG_USB_HOST_NOTIFY
#include <linux/usb_notify.h>
#endif

#include <linux/completion.h>
#include <linux/ccic/ccic_misc.h>

#if defined(CONFIG_SWITCH)
static struct switch_dev switch_dock = {
	.name = "ccic_dock",
};
#endif

void usbpd_manager_select_pdo_cancel(struct device *dev);

#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
extern void select_pdo(int num);
void usbpd_manager_select_pdo(int num);
extern void (*fp_select_pdo)(int num);
#if defined(CONFIG_PDIC_PD30)
extern int (*fp_sec_pd_select_pps)(int num, int ppsVol, int ppsCur);
extern int (*fp_sec_pd_get_apdo_max_power)(unsigned int *pdo_pos, unsigned int *taMaxVol, unsigned int *taMaxCur, unsigned int *taMaxPwr);
extern int (*fp_pps_enable)(int num, int ppsVol, int ppsCur, int enable);
int (*fp_get_pps_voltage)(void);
#endif
#if defined(CONFIG_CCIC_NOTIFIER)
extern struct device *ccic_device;
#endif

void usbpd_manager_select_pdo(int num)
{
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;
	bool vbus_short;
#if defined(CONFIG_PDIC_PD30)
	int pps_enable = 0;
#endif
	pd_data->phy_ops.get_vbus_short_check(pd_data, &vbus_short);

	if (vbus_short) {
		pr_info(" %s : PDO(%d) is ignored becasue of vbus short\n",
				__func__, pd_noti.sink_status.selected_pdo_num);
		return;
	}

	mutex_lock(&manager->pdo_mutex);
	if (manager->flash_mode == 1)
		goto exit;

	if (pd_data->policy.plug_valid == 0) {
		pr_info(" %s : PDO(%d) is ignored becasue of usbpd is detached\n",
				__func__, num);
		goto exit;
	}

	if (pd_noti.sink_status.available_pdo_num < 1) {
		pr_info("%s: available pdo is 0\n", __func__);
		goto exit;
	}

#if defined(CONFIG_PDIC_PD30)
	if (pd_data->ip_num == S2MU107_USBPD_IP) {
		pd_data->phy_ops.get_pps_enable(pd_data, &pps_enable);

		if (pps_enable == PPS_ENABLE) {
			pr_info(" %s : forced pps disable\n", __func__);
			pd_data->phy_ops.pps_enable(pd_data, PPS_DISABLE);
			pd_data->phy_ops.force_pps_disable(pd_data);
		}
	}
#endif
	if (pd_noti.sink_status.selected_pdo_num == num)
		goto exit;
	else if (num > pd_noti.sink_status.available_pdo_num)
		pd_noti.sink_status.selected_pdo_num = pd_noti.sink_status.available_pdo_num;
	else if (num < 1)
		pd_noti.sink_status.selected_pdo_num = 1;
	else
		pd_noti.sink_status.selected_pdo_num = num;

	manager->pn_flag = false;

	pr_info(" %s : PDO(%d) is selected to change\n", __func__, pd_noti.sink_status.selected_pdo_num);

	usbpd_manager_inform_event(pd_noti.pusbpd, MANAGER_NEW_POWER_SRC);
exit:
	mutex_unlock(&manager->pdo_mutex);
}

#if defined(CONFIG_PDIC_PD30)
int usbpd_manager_select_pps(int num, int ppsVol, int ppsCur)
{
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;
	bool vbus_short;
	int ret = 0;
	int pps_enable = 0;

	pd_data->phy_ops.get_vbus_short_check(pd_data, &vbus_short);

	if (vbus_short) {
		pr_info(" %s : PDO(%d) is ignored becasue of vbus short\n",
				__func__, pd_noti.sink_status.selected_pdo_num);
		ret = -EPERM;
		return ret;
	}

	mutex_lock(&manager->pdo_mutex);
	if (manager->flash_mode == 1)
		goto exit;

	if (pd_data->policy.plug_valid == 0) {
		pr_info(" %s : PDO(%d) is ignored becasue of usbpd is detached\n",
				__func__, num);
		goto exit;
	}
	/* [dchg] TODO: check more below option */
	if (num > pd_noti.sink_status.available_pdo_num) {
		pr_info("%s: request pdo num(%d) is higher taht available pdo.\n", __func__, num);
		ret = -EINVAL;
		goto exit;
	}

	if (pd_data->ip_num == S2MU107_USBPD_IP) {
		pd_data->phy_ops.get_pps_enable(pd_data, &pps_enable);

		if (pps_enable == PPS_ENABLE) {
			pr_info(" %s : forced pps disable\n", __func__);
			pd_data->phy_ops.pps_enable(pd_data, PPS_DISABLE);
			pd_data->phy_ops.force_pps_disable(pd_data);
		}
	}

	pd_noti.sink_status.selected_pdo_num = num;

	if (ppsVol > pd_noti.sink_status.power_list[num].max_voltage) {
		pr_info("%s: ppsVol is over(%d, max:%d)\n",
			__func__, ppsVol, pd_noti.sink_status.power_list[num].max_voltage);
		ppsVol = pd_noti.sink_status.power_list[num].max_voltage;
	} else if (ppsVol < pd_noti.sink_status.power_list[num].min_voltage) {
		pr_info("%s: ppsVol is under(%d, min:%d)\n",
			__func__, ppsVol, pd_noti.sink_status.power_list[num].min_voltage);
		ppsVol = pd_noti.sink_status.power_list[num].min_voltage;
	}

	if (ppsCur > pd_noti.sink_status.power_list[num].max_current) {
		pr_info("%s: ppsCur is over(%d, max:%d)\n",
			__func__, ppsCur, pd_noti.sink_status.power_list[num].max_current);
		ppsCur = pd_noti.sink_status.power_list[num].max_current;
	} else if (ppsCur < 0) {
		pr_info("%s: ppsCur is under(%d, 0)\n",
			__func__, ppsCur);
		ppsCur = 0;
	}

	pd_noti.sink_status.pps_voltage = ppsVol;
	pd_noti.sink_status.pps_current = ppsCur;

	pr_info(" %s : PPS PDO(%d), voltage(%d), current(%d) is selected to change\n",
		__func__, pd_noti.sink_status.selected_pdo_num, ppsVol, ppsCur);

	manager->pn_flag = false;
	//schedule_delayed_work(&manager->select_pdo_handler, msecs_to_jiffies(50));
	usbpd_manager_inform_event(pd_noti.pusbpd, MANAGER_NEW_POWER_SRC);

exit:
	mutex_unlock(&manager->pdo_mutex);

	return ret;
}

int usbpd_manager_get_apdo_max_power(unsigned int *pdo_pos,
	unsigned int *taMaxVol, unsigned int *taMaxCur, unsigned int *taMaxPwr)
{
	int i;
	int ret = 0;
	int max_current = 0, max_voltage = 0, max_power = 0;

	if (!pd_noti.sink_status.has_apdo) {
		pr_info("%s: pd don't have apdo\n", __func__);
		return -1;
	}

	/* First, get TA maximum power from the fixed PDO */
	for (i = 1; i <= pd_noti.sink_status.available_pdo_num; i++) {
		if (!(pd_noti.sink_status.power_list[i].apdo)) {
			max_voltage = pd_noti.sink_status.power_list[i].max_voltage;
			max_current = pd_noti.sink_status.power_list[i].max_current;
			max_power = (max_voltage * max_current > max_power) ? (max_voltage * max_current) : max_power;
			*taMaxPwr = max_power;	/* mW */
		}
	}

	if (*pdo_pos == 0) {
		/* Get the proper PDO */
		for (i = 1; i <= pd_noti.sink_status.available_pdo_num; i++) {
			if (pd_noti.sink_status.power_list[i].apdo) {
				if (pd_noti.sink_status.power_list[i].max_voltage >= *taMaxVol) {
					*pdo_pos = i;
					*taMaxVol = pd_noti.sink_status.power_list[i].max_voltage;
					*taMaxCur = pd_noti.sink_status.power_list[i].max_current;
					break;
				}
			}
			if (*pdo_pos)
				break;
		}

		if (*pdo_pos == 0) {
			pr_info("mv (%d) and ma (%d) out of range of APDO\n",
				*taMaxVol, *taMaxCur);
			ret = -EINVAL;
		}
	} else {
		/* If we already have pdo object position, we don't need to search max current */
		ret = -ENOTSUPP;
	}

	pr_info("%s : *pdo_pos(%d), *taMaxVol(%d), *maxCur(%d), *maxPwr(%d)\n",
		__func__, *pdo_pos, *taMaxVol, *taMaxCur, *taMaxPwr);

	return ret;
}

int usbpd_manager_pps_enable(int num, int ppsVol, int ppsCur, int enable)
{
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;
	bool vbus_short;
	int ret = 0;
	int pps_enable = 0;

	if (pd_data->phy_ops.pps_enable == NULL) {
		pr_info(" %s : pps_enable function is not present\n", __func__);
		return -ENOMEM;
	}

	pd_data->phy_ops.get_vbus_short_check(pd_data, &vbus_short);

	if (vbus_short) {
		pr_info(" %s : PDO(%d) is ignored becasue of vbus short\n",
				__func__, pd_noti.sink_status.selected_pdo_num);
		ret = -EPERM;
		return ret;
	}

	mutex_lock(&manager->pdo_mutex);
	if (manager->flash_mode == 1)
		goto exit;

	if (pd_data->policy.plug_valid == 0) {
		pr_info(" %s : PDO(%d) is ignored becasue of usbpd is detached\n",
				__func__, num);
		goto exit;
	}

	pd_data->phy_ops.get_pps_enable(pd_data, &pps_enable);

	if ((pps_enable == enable) && pps_enable) {
		pr_info(" %s : auto pps is already enabled\n", __func__);
		goto exit;
	}


	/* [dchg] TODO: check more below option */
	if (num > pd_noti.sink_status.available_pdo_num) {
		pr_info("%s: request pdo num(%d) is higher taht available pdo.\n", __func__, num);
		ret = -EINVAL;
		goto exit;
	}

	pd_noti.sink_status.selected_pdo_num = num;

	if (ppsVol > pd_noti.sink_status.power_list[num].max_voltage) {
		pr_info("%s: ppsVol is over(%d, max:%d)\n",
			__func__, ppsVol, pd_noti.sink_status.power_list[num].max_voltage);
		ppsVol = pd_noti.sink_status.power_list[num].max_voltage;
	} else if (ppsVol < pd_noti.sink_status.power_list[num].min_voltage) {
		pr_info("%s: ppsVol is under(%d, min:%d)\n",
			__func__, ppsVol, pd_noti.sink_status.power_list[num].min_voltage);
		ppsVol = pd_noti.sink_status.power_list[num].min_voltage;
	}

	if (ppsCur > pd_noti.sink_status.power_list[num].max_current) {
		pr_info("%s: ppsCur is over(%d, max:%d)\n",
			__func__, ppsCur, pd_noti.sink_status.power_list[num].max_current);
		ppsCur = pd_noti.sink_status.power_list[num].max_current;
	} else if (ppsCur < 0) {
		pr_info("%s: ppsCur is under(%d, 0)\n",
			__func__, ppsCur);
		ppsCur = 0;
	}

	pd_noti.sink_status.pps_voltage = ppsVol;
	pd_noti.sink_status.pps_current = ppsCur;

	pr_info(" %s : PPS PDO(%d), voltage(%d), current(%d) is selected to change\n",
		__func__, pd_noti.sink_status.selected_pdo_num, ppsVol, ppsCur);

	if (enable) {
//		schedule_delayed_work(&manager->select_pdo_handler, msecs_to_jiffies(0));
		usbpd_manager_inform_event(pd_noti.pusbpd, MANAGER_NEW_POWER_SRC);
		msleep(150);
		pd_data->phy_ops.pps_enable(pd_data, PPS_ENABLE);
	} else {
		pd_data->phy_ops.pps_enable(pd_data, PPS_DISABLE);
		msleep(100);
//		schedule_delayed_work(&manager->select_pdo_handler, msecs_to_jiffies(0));
		usbpd_manager_inform_event(pd_noti.pusbpd, MANAGER_NEW_POWER_SRC);
	}
exit:
	mutex_unlock(&manager->pdo_mutex);

	return ret;
}

int usbpd_manager_get_pps_voltage(void)
{
	struct usbpd_data *pd_data = pd_noti.pusbpd;

	if (pd_data->phy_ops.get_pps_voltage)
		return (pd_data->phy_ops.get_pps_voltage(pd_data) * USBPD_PPS_RQ_VOLT_UNIT);
	else
		pr_info(" %s : get_pps_voltage function is not present\n", __func__);

	return 0;
}

int sec_get_pps_voltage(void)
{
	if (fp_get_pps_voltage)
		return fp_get_pps_voltage();

	return 0;
}
#endif
void pdo_ctrl_by_flash(bool mode)
{
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;
#if defined(CONFIG_PDIC_PD30)
	int pps_enable = 0;
#endif

	pr_info("%s: mode(%d)\n", __func__, mode);

	mutex_lock(&manager->pdo_mutex);
	if (mode)
		manager->flash_mode = 1;
	else
		manager->flash_mode = 0;

#if defined(CONFIG_PDIC_PD30)
	if (pd_data->ip_num == S2MU107_USBPD_IP) {
		pd_data->phy_ops.get_pps_enable(pd_data, &pps_enable);

		if (pps_enable == PPS_ENABLE) {
			pr_info(" %s : forced pps disable\n", __func__);
			pd_data->phy_ops.pps_enable(pd_data, PPS_DISABLE);
			pd_data->phy_ops.force_pps_disable(pd_data);
		}
	}
#endif

	if (pd_noti.sink_status.selected_pdo_num != 0) {
		pd_noti.sink_status.selected_pdo_num = 1;
		usbpd_manager_select_pdo_cancel(pd_data->dev);
		usbpd_manager_inform_event(pd_noti.pusbpd, MANAGER_GET_SRC_CAP);
	}
	mutex_unlock(&manager->pdo_mutex);
}
#endif
#endif

void usbpd_manager_select_pdo_handler(struct work_struct *work)
{
	pr_info("%s: call select pdo handler\n", __func__);

	usbpd_manager_inform_event(pd_noti.pusbpd, MANAGER_NEW_POWER_SRC);

}

void usbpd_manager_select_pdo_cancel(struct device *dev)
{
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;

	cancel_delayed_work_sync(&manager->select_pdo_handler);
}

void usbpd_manager_start_discover_msg_handler(struct work_struct *work)
{
	struct usbpd_manager_data *manager =
		container_of(work, struct usbpd_manager_data,
										start_discover_msg_handler.work);
	pr_info("%s: call handler\n", __func__);

	mutex_lock(&manager->vdm_mutex);
	if (manager->alt_sended == 0 && manager->vdm_en == 1) {
		usbpd_manager_inform_event(pd_noti.pusbpd,
						MANAGER_START_DISCOVER_IDENTITY);
		manager->alt_sended = 1;
	}
	mutex_unlock(&manager->vdm_mutex);
}

void usbpd_manager_start_discover_msg_cancel(struct device *dev)
{
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;

	cancel_delayed_work_sync(&manager->start_discover_msg_handler);
}

void usbpd_manager_send_pr_swap(struct device *dev)
{
	pr_info("%s: call send pr swap msg\n", __func__);

	usbpd_manager_inform_event(pd_noti.pusbpd, MANAGER_SEND_PR_SWAP);
}

void usbpd_manager_send_dr_swap(struct device *dev)
{
	pr_info("%s: call send pr swap msg\n", __func__);

	usbpd_manager_inform_event(pd_noti.pusbpd, MANAGER_SEND_DR_SWAP);
}

void usbpd_manager_match_sink_cap(struct usbpd_data *pd_data)
{
	unsigned src_cur, volt, op_cur;
	data_obj_type *data_obj = &pd_data->source_data_obj;
	
	volt = pd_data->source_get_sink_obj.power_data_obj_sink.voltage;
	op_cur= pd_data->source_get_sink_obj.power_data_obj_sink.op_current;
	src_cur = pd_data->source_data_obj.power_data_obj.max_current;

	pr_info("%s, src_cur(%dmA) SinkCap volt(%dmV), opcur(%dmA)\n", __func__,
			src_cur * 10, volt * 50, op_cur * 10);

	if (volt * 50 > 5000)
		return;
	
	if ((src_cur <= op_cur) && (op_cur * 10 <= 1000)) {
		data_obj->power_data_obj.max_current = op_cur;
		pr_info("%s, SourceCap cur (%dmA)\n", __func__,
				data_obj->power_data_obj.max_current);
	}

	return;
}

static void init_source_cap_data(struct usbpd_manager_data *_data)
{
/*	struct usbpd_data *pd_data = manager_to_usbpd(_data);
	int val;						*/
	msg_header_type *msg_header = &_data->pd_data->source_msg_header;
	data_obj_type *data_obj = &_data->pd_data->source_data_obj;

	msg_header->msg_type = USBPD_Source_Capabilities;
/*	pd_data->phy_ops.get_power_role(pd_data, &val);		*/
	msg_header->port_data_role = USBPD_DFP;
	msg_header->spec_revision = _data->pd_data->specification_revision;
	msg_header->port_power_role = USBPD_SOURCE;
	msg_header->num_data_objs = 1;

	data_obj->power_data_obj.max_current = 500 / 10;
	data_obj->power_data_obj.voltage = 5000 / 50;
	data_obj->power_data_obj.supply = POWER_TYPE_FIXED;
	data_obj->power_data_obj.data_role_swap = 1;
	data_obj->power_data_obj.dual_role_power = 1;
	data_obj->power_data_obj.usb_suspend_support = 1;
	data_obj->power_data_obj.usb_comm_capable = 1;
}

void usbpd_manager_remove_new_cap(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;
	init_source_cap_data(manager);
}

static void init_sink_cap_data(struct usbpd_manager_data *_data)
{
/*	struct usbpd_data *pd_data = manager_to_usbpd(_data);
	int val;						*/
	msg_header_type *msg_header = &_data->pd_data->sink_msg_header;
	data_obj_type *data_obj = _data->pd_data->sink_data_obj;

	msg_header->msg_type = USBPD_Sink_Capabilities;
/*	pd_data->phy_ops.get_power_role(pd_data, &val);		*/
	msg_header->port_data_role = USBPD_UFP;
	msg_header->spec_revision = _data->pd_data->specification_revision;
	msg_header->port_power_role = USBPD_SINK;
	msg_header->num_data_objs = 2;

	data_obj->power_data_obj_sink.supply_type = POWER_TYPE_FIXED;
	data_obj->power_data_obj_sink.dual_role_power = 1;
	data_obj->power_data_obj_sink.higher_capability = 1;
	data_obj->power_data_obj_sink.externally_powered = 0;
	data_obj->power_data_obj_sink.usb_comm_capable = 1;
	data_obj->power_data_obj_sink.data_role_swap = 1;
	data_obj->power_data_obj_sink.voltage = 5000/50;
	data_obj->power_data_obj_sink.op_current = 3000/10;

	(data_obj + 1)->power_data_obj_sink.supply_type = POWER_TYPE_FIXED;
	(data_obj + 1)->power_data_obj_sink.voltage = 9000/50;
	(data_obj + 1)->power_data_obj_sink.op_current = 2000/10;
}

int samsung_uvdm_ready(void)
{
	int uvdm_ready = false;
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s\n", __func__);

	if (manager->is_samsung_accessory_enter_mode && manager->pn_flag)
		uvdm_ready = true;

	pr_info("uvdm ready = %d, pn_flag = %d\n", uvdm_ready, manager->pn_flag);
	return uvdm_ready;
}

void samsung_uvdm_close(void)
{
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s\n", __func__);

	complete(&manager->uvdm_out_wait);
	complete(&manager->uvdm_in_wait);

}

void set_endian(char *src, char *dest, int size)
{
	int i, j;
	int loop;
	int dest_pos;
	int src_pos;

	loop = size / SEC_UVDM_ALIGN;
	loop += (((size % SEC_UVDM_ALIGN) > 0) ? 1:0);

	for (i = 0 ; i < loop ; i++)
		for (j = 0 ; j < SEC_UVDM_ALIGN ; j++) {
			src_pos = SEC_UVDM_ALIGN * i + j;
			dest_pos = SEC_UVDM_ALIGN * i + SEC_UVDM_ALIGN - j - 1;
			dest[dest_pos] = src[src_pos];
		}
}

int get_checksum(char *data, int start_addr, int size)
{
	int checksum = 0;
	int i;

	for (i = 0; i < size; i++)
		checksum += data[start_addr+i];

	return checksum;
}

int set_uvdmset_count(int size)
{
	int ret = 0;

	if (size <= SEC_UVDM_MAXDATA_FIRST)
		ret = 1;
	else {
		ret = ((size-SEC_UVDM_MAXDATA_FIRST) / SEC_UVDM_MAXDATA_NORMAL);
		if (((size-SEC_UVDM_MAXDATA_FIRST) % SEC_UVDM_MAXDATA_NORMAL) == 0)
			ret += 1;
		else
			ret += 2;
	}
	return ret;
}

void set_msg_header(void *data, int msg_type, int obj_num)
{
	msg_header_type *msg_hdr;
	uint8_t *SendMSG = (uint8_t *)data;

	msg_hdr = (msg_header_type *)&SendMSG[0];
	msg_hdr->msg_type = msg_type;
	msg_hdr->num_data_objs = obj_num;
	msg_hdr->port_data_role = USBPD_DFP;
	return;

}

void set_uvdm_header(void *data, int vid, int vdm_type)
{
	uvdm_header *uvdm_hdr;
	uint8_t *SendMSG = (uint8_t *)data;

	uvdm_hdr = (uvdm_header *)&SendMSG[0];
	uvdm_hdr->vendor_id = SAMSUNG_VENDOR_ID;
	uvdm_hdr->vdm_type = vdm_type;
	uvdm_hdr->vendor_defined = SEC_UVDM_UNSTRUCTURED_VDM;
	return;
}

void set_sec_uvdm_header(void *data, int pid, bool data_type, int cmd_type,
		bool dir, int total_set_num, uint8_t received_data)
{
	s_uvdm_header *SEC_UVDM_HEADER;
	uint8_t *SendMSG = (uint8_t *)data;

	SEC_UVDM_HEADER = (s_uvdm_header *)&SendMSG[4];

	SEC_UVDM_HEADER->pid = pid;
	SEC_UVDM_HEADER->data_type = data_type;
	SEC_UVDM_HEADER->cmd_type = cmd_type;
	SEC_UVDM_HEADER->direction = dir;
	SEC_UVDM_HEADER->total_set_num = total_set_num;
	SEC_UVDM_HEADER->data = received_data;

	pr_info("%s pid = 0x%x  data_type=%d ,cmd_type =%d,direction= %d, total_num_of_uvdm_set = %d\n",
			__func__, SEC_UVDM_HEADER->pid,
			SEC_UVDM_HEADER->data_type,
			SEC_UVDM_HEADER->cmd_type,
			SEC_UVDM_HEADER->direction,
			SEC_UVDM_HEADER->total_set_num);
	return;
}

int get_data_size(int first_set, int remained_data_size)
{
	int ret = 0;

	if (first_set)
		ret = (remained_data_size <= SEC_UVDM_MAXDATA_FIRST) ? \
		      remained_data_size : SEC_UVDM_MAXDATA_FIRST;
	else
		ret = (remained_data_size <= SEC_UVDM_MAXDATA_NORMAL) ? \
		      remained_data_size : SEC_UVDM_MAXDATA_NORMAL;

	return ret;
}

void set_sec_uvdm_tx_header(void *data, int first_set, int cur_set, int total_size,
		int remained_size)
{
	s_tx_header *SEC_TX_HAEDER;
	uint8_t *SendMSG = (uint8_t*)data;

	if(first_set)
		SEC_TX_HAEDER = (s_tx_header *)&SendMSG[8];
	else
		SEC_TX_HAEDER = (s_tx_header *)&SendMSG[4];
	SEC_TX_HAEDER->cur_size = get_data_size(first_set,remained_size);
	SEC_TX_HAEDER->total_size = total_size;
	SEC_TX_HAEDER->order_cur_set = cur_set;
	return;
}

void set_sec_uvdm_tx_tailer(void *data)
{
	s_tx_tailer *SEC_TX_TAILER;
	uint8_t *SendMSG = (uint8_t *)data;

	SEC_TX_TAILER = (s_tx_tailer *)&SendMSG[24];
	SEC_TX_TAILER->checksum = get_checksum(SendMSG, 4,SEC_UVDM_CHECKSUM_COUNT);
	return;
}

void set_sec_uvdm_rx_header(void *data, int cur_num, int cur_set, int ack)
{
	s_rx_header *SEC_RX_HEADER;
	uint8_t *SendMSG = (uint8_t*)data;

	SEC_RX_HEADER = (s_rx_header *)&SendMSG[4];
	SEC_RX_HEADER->order_cur_set = cur_num;
	SEC_RX_HEADER->rcv_data_size = cur_set;
	SEC_RX_HEADER->result_value = ack;

	return;
}

int usbpd_manager_send_samsung_uvdm_message(void *data, const char *buf, size_t size)
{
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;
	int received_data = 0;
	int data_role = 0;
	int power_role = 0;

	if ((buf == NULL)||(size < sizeof(unsigned int))) {
		pr_err("%s given data is not valid !\n", __func__);
		return -EINVAL;
	}

	sscanf(buf, "%d", &received_data);

	pd_data->phy_ops.get_data_role(pd_data, &data_role);

	if (data_role == USBPD_UFP) {
		pr_err("%s, skip, now data role is ufp\n", __func__);
		return 0;
	}

	data_role = pd_data->phy_ops.get_power_role(pd_data, &power_role);

	manager->uvdm_msg_header.msg_type = USBPD_UVDM_MSG;
	manager->uvdm_msg_header.port_data_role = USBPD_DFP;
	manager->uvdm_msg_header.port_power_role = power_role;
	manager->uvdm_data_obj[0].unstructured_vdm.vendor_id = SAMSUNG_VENDOR_ID;
	manager->uvdm_data_obj[0].unstructured_vdm.vendor_defined = SEC_UVDM_UNSTRUCTURED_VDM;
	manager->uvdm_data_obj[1].object = received_data;
	if (manager->uvdm_data_obj[1].sec_uvdm_header.data_type == SEC_UVDM_SHORT_DATA) {
		pr_info("%s - process short data!\n", __func__);
		// process short data
		// phase 1. fill message header
		manager->uvdm_msg_header.num_data_objs = 2; // VDM Header + 6 VDOs = MAX 7
		// phase 2. fill uvdm header (already filled)
		// phase 3. fill sec uvdm header
		manager->uvdm_data_obj[1].sec_uvdm_header.total_number_of_uvdm_set = 1;
	} else {
		pr_info("%s - process long data!\n", __func__);
		// process long data
		// phase 1. fill message header
		// phase 2. fill uvdm header
		// phase 3. fill sec uvdm header

	}
	usbpd_manager_inform_event(pd_data, MANAGER_UVDM_SEND_MESSAGE);
	return 0;
}

ssize_t samsung_uvdm_out_request_message(void *data, size_t size)
{
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;
	uint8_t *SEC_DATA;
	uint8_t rcv_data[MAX_INPUT_DATA] = {0,};
	int need_set_cnt = 0;
	int cur_set_data = 0;
	int cur_set_num = 0;
	int remained_data_size = 0;
	int accumulated_data_size = 0;
	int received_data_index = 0;
	int time_left = 0;
	int i;

	pr_info("%s \n", __func__);
	set_msg_header(&manager->uvdm_msg_header,USBPD_Vendor_Defined,7);
	set_uvdm_header(&manager->uvdm_data_obj[0], SAMSUNG_VENDOR_ID, 0);

	if (size <= 1) {
		pr_info("%s - process short data!\n", __func__);
		// process short data
		// phase 1. fill message header
		manager->uvdm_msg_header.num_data_objs = 2; // VDM Header + 6 VDOs = MAX 7
		// phase 2. fill uvdm header (already filled)
		// phase 3. fill sec uvdm header
		manager->uvdm_data_obj[1].sec_uvdm_header.total_number_of_uvdm_set = 1;
	} else {
		pr_info("%s - process long data!\n", __func__);
		// process long data
		// phase 1. fill message header
		// phase 2. fill uvdm header
		// phase 3. fill sec uvdm header
		// phase 4.5.6.7 fill sec data header , data , sec data tail and so on.

		set_endian(data, rcv_data, size);
		need_set_cnt = set_uvdmset_count(size);
		manager->uvdm_first_req = true;
		manager->uvdm_dir =  DIR_OUT;
		cur_set_num = 1;
		accumulated_data_size = 0;
		remained_data_size = size;

		if (manager->uvdm_first_req)
			set_sec_uvdm_header(&manager->uvdm_data_obj[0], manager->Product_ID,
					SEC_UVDM_LONG_DATA,SEC_UVDM_ININIATOR, DIR_OUT,
					need_set_cnt, 0);
		while (cur_set_num <= need_set_cnt) {
			cur_set_data = 0;
			time_left = 0;
			set_sec_uvdm_tx_header(&manager->uvdm_data_obj[0], manager->uvdm_first_req,
					cur_set_num, size, remained_data_size);
			cur_set_data = get_data_size(manager->uvdm_first_req,remained_data_size);

			pr_info("%s current set data size: %d, total data size %ld, current uvdm set num %d\n", __func__, cur_set_data, size, cur_set_num);

			if (manager->uvdm_first_req) {
				SEC_DATA = (uint8_t *)&manager->uvdm_data_obj[3];
				for ( i = 0; i < SEC_UVDM_MAXDATA_FIRST; i++)
					SEC_DATA[i] = rcv_data[received_data_index++];
			} else {
				SEC_DATA = (uint8_t *)&manager->uvdm_data_obj[2];
				for ( i = 0; i < SEC_UVDM_MAXDATA_NORMAL; i++)
					SEC_DATA[i] = rcv_data[received_data_index++];
			}

			set_sec_uvdm_tx_tailer(&manager->uvdm_data_obj[0]);

			reinit_completion(&manager->uvdm_out_wait);
			usbpd_manager_inform_event(pd_data, MANAGER_UVDM_SEND_MESSAGE);

			time_left = wait_for_completion_interruptible_timeout(&manager->uvdm_out_wait, msecs_to_jiffies(SEC_UVDM_WAIT_MS));
			if (time_left <= 0) {
				pr_err("%s tiemout \n",__func__);
				return -ETIME;
			}
			if (manager->uvdm_error) {
				manager->uvdm_first_req = false;
				return manager->uvdm_error;
			}
			accumulated_data_size += cur_set_data;
			remained_data_size -= cur_set_data;
			if (manager->uvdm_first_req)
				manager->uvdm_first_req = false;
			cur_set_num++;
		}
		return size;
	}

	return 0;
}

int samsung_uvdm_in_request_message(void *data)
{
	struct usbpd_data *pd_data = pd_noti.pusbpd;
	struct usbpd_manager_data *manager = &pd_data->manager;
	struct policy_data *policy = &pd_data->policy;
	uint8_t in_data[MAX_INPUT_DATA] = {0,};

	s_uvdm_header	SEC_RES_HEADER;
	s_tx_header	SEC_TX_HEADER;
	s_tx_tailer	SEC_TX_TAILER;
	data_obj_type	uvdm_data_obj[USBPD_MAX_COUNT_MSG_OBJECT];
	msg_header_type	uvdm_msg_header;

	int cur_set_data = 0;
	int cur_set_num = 0;
	int total_set_num = 0;
	int rcv_data_size = 0;
	int total_rcv_size = 0;
	int ack = 0;
	int size = 0;
	int time_left = 0;
	int i;
	int cal_checksum = 0;

	pr_info("%s\n", __func__);

	manager->uvdm_dir = DIR_IN;
	manager->uvdm_first_req = true;
	uvdm_msg_header.word = policy->rx_msg_header.word;

	/* 2. Common : Fill the MSGHeader */
	set_msg_header(&manager->uvdm_msg_header, USBPD_Vendor_Defined, 2);
	/* 3. Common : Fill the UVDMHeader*/
	set_uvdm_header(&manager->uvdm_data_obj[0], SAMSUNG_VENDOR_ID, 0);

	/* 4. Common : Fill the First SEC_VDMHeader*/
	if(manager->uvdm_first_req)
		set_sec_uvdm_header(&manager->uvdm_data_obj[0], manager->Product_ID,\
				SEC_UVDM_LONG_DATA, SEC_UVDM_ININIATOR, DIR_IN, 0, 0);

	/* 5. Send data to PDIC */
	reinit_completion(&manager->uvdm_in_wait);
	usbpd_manager_inform_event(pd_data, MANAGER_UVDM_SEND_MESSAGE);

	cur_set_num = 0;
	total_set_num = 1;

	do {
		time_left =
			wait_for_completion_interruptible_timeout(&manager->uvdm_in_wait,
					msecs_to_jiffies(SEC_UVDM_WAIT_MS));
		if (time_left <= 0) {
			pr_err("%s timeout\n", __func__);
			return -ETIME;
		}
		if (manager->uvdm_error) {
			manager->uvdm_first_req = false;
			return manager->uvdm_error;
		}

		/* read data */
		uvdm_msg_header.word = policy->rx_msg_header.word;
		for (i = 0; i < uvdm_msg_header.num_data_objs; i++)
			uvdm_data_obj[i].object = policy->rx_data_obj[i].object;

		if (manager->uvdm_first_req) {
			SEC_RES_HEADER.object = uvdm_data_obj[1].object;
			SEC_TX_HEADER.object = uvdm_data_obj[2].object;

			if (SEC_RES_HEADER.data_type == TYPE_SHORT) {
				in_data[rcv_data_size++] = SEC_RES_HEADER.data;
				return rcv_data_size;
			} else {
				/* 1. check the data size received */
				size = SEC_TX_HEADER.total_size;
				cur_set_data = SEC_TX_HEADER.cur_size;
				cur_set_num = SEC_TX_HEADER.order_cur_set;
				total_set_num = SEC_RES_HEADER.total_set_num;

				manager->uvdm_first_req = false;
				/* 2. copy data to buffer */
				for (i = 0; i < SEC_UVDM_MAXDATA_FIRST; i++) {
					in_data[rcv_data_size++] =uvdm_data_obj[3+i/SEC_UVDM_ALIGN].byte[i%SEC_UVDM_ALIGN];
				}
				total_rcv_size += cur_set_data;
				manager->uvdm_first_req = false;
			}
		} else {
			SEC_TX_HEADER.object = uvdm_data_obj[1].object;
			cur_set_data = SEC_TX_HEADER.cur_size;
			cur_set_num = SEC_TX_HEADER.order_cur_set;
			/* 2. copy data to buffer */
			for (i = 0 ; i < SEC_UVDM_MAXDATA_NORMAL; i++)
				in_data[rcv_data_size++] = uvdm_data_obj[2+i/SEC_UVDM_ALIGN].byte[i%SEC_UVDM_ALIGN];
			total_rcv_size += cur_set_data;
		}
		/* 3. Check Checksum */
		SEC_TX_TAILER.object =uvdm_data_obj[6].object;
		cal_checksum = get_checksum((char *)&uvdm_data_obj[0], 4, SEC_UVDM_CHECKSUM_COUNT);
		ack = (cal_checksum == SEC_TX_TAILER.checksum) ? RX_ACK : RX_NAK;

		/* 5. Common : Fill the MSGHeader */
		set_msg_header(&manager->uvdm_msg_header, USBPD_Vendor_Defined, 2);
		/* 5.1. Common : Fill the UVDMHeader*/
		set_uvdm_header(&manager->uvdm_data_obj[0], SAMSUNG_VENDOR_ID, 0);
		/* 5.2. Common : Fill the First SEC_VDMHeader*/

		set_sec_uvdm_rx_header(&manager->uvdm_data_obj[0], cur_set_num, cur_set_data, ack);
		reinit_completion(&manager->uvdm_in_wait);
		usbpd_manager_inform_event(pd_data, MANAGER_UVDM_SEND_MESSAGE);
	} while ( cur_set_num < total_set_num);

	set_endian(in_data, data, size);

	return size;

}

void usbpd_manager_receive_samsung_uvdm_message(struct usbpd_data *pd_data)
{
	struct policy_data *policy = &pd_data->policy;
	int i = 0;
	msg_header_type		uvdm_msg_header;
	data_obj_type		uvdm_data_obj[USBPD_MAX_COUNT_MSG_OBJECT];
	struct usbpd_manager_data *manager = &pd_data->manager;
	s_uvdm_header SEC_UVDM_RES_HEADER;
	s_tx_header SEC_UVDM_TX_HEADER;
	s_rx_header SEC_UVDM_RX_HEADER;
	uvdm_msg_header.word = policy->rx_msg_header.word;


	for (i = 0; i < uvdm_msg_header.num_data_objs; i++)
		uvdm_data_obj[i].object = policy->rx_data_obj[i].object;

	uvdm_msg_header.word = policy->rx_msg_header.word;
	manager->uvdm_error = 0;

	pr_info("%s dir %s \n", __func__, (manager->uvdm_dir == DIR_OUT) ? "OUT":"IN");
	if (manager->uvdm_dir == DIR_OUT) {
		if (manager->uvdm_first_req) {
			SEC_UVDM_RES_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_RES_HEADER.data_type == TYPE_LONG) {
				if (SEC_UVDM_RES_HEADER.cmd_type == RES_ACK) {
					SEC_UVDM_RX_HEADER.object = uvdm_data_obj[2].object;
					if (SEC_UVDM_RX_HEADER.result_value == RX_ACK) {
						/* do nothing */
					} else if (SEC_UVDM_RX_HEADER.result_value == RX_NAK) {
						pr_err("%s DIR_OUT RX_NAK received\n", __func__);
						manager->uvdm_error = -ENODATA;
					} else if (SEC_UVDM_RX_HEADER.result_value == RX_BUSY) {
						pr_err("%s DIR_OUT RX_BUSY received\n", __func__);
						manager->uvdm_error = -EBUSY;
					} else {
						pr_err("%s DIR_OUT Undefined RX value\n", __func__);
						manager->uvdm_error = -EPROTO;
					}
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_NAK) {
					pr_err("%s DIR_OUT RES_NAK received\n", __func__);
					manager->uvdm_error = -ENODATA;
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_BUSY) {
					pr_err("%s DIR_OUT RES_BUSY received\n", __func__);
					manager->uvdm_error = -EBUSY;
				} else {
					pr_err("%s DIR_OUT Undefined RES value\n", __func__);
					manager->uvdm_error = -EPROTO;
				}
			} else { /* TYPE_SHORT */
				if (SEC_UVDM_RES_HEADER.cmd_type == RES_ACK) {
					/* do nothing */
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_NAK) {
					pr_err("%s DIR_OUT SHORT RES_NAK received\n", __func__);
					manager->uvdm_error = -ENODATA;
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_BUSY) {
					pr_err("%s DIR_OUT SHORT RES_BUSY received\n", __func__);
					manager->uvdm_error = -EBUSY;
				} else {
					pr_err("%s DIR_OUT Undefined RES value\n", __func__);
					manager->uvdm_error = -EPROTO;
				}
			}
		} else { /* after 2nd packet for TYPE_LONG */
			SEC_UVDM_RX_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_RX_HEADER.result_value == RX_ACK) {
				/* do nothing */
			} else if (SEC_UVDM_RX_HEADER.result_value == RX_NAK) {
				pr_err("%s DIR_OUT RX_NAK received\n", __func__);
				manager->uvdm_error = -ENODATA;
			} else if (SEC_UVDM_RX_HEADER.result_value == RX_BUSY) {
				pr_err("%s DIR_OUT RX_BUSY received\n", __func__);
				manager->uvdm_error = -EBUSY;
			} else {
				pr_err("%s DIR_OUT Undefined RX value\n", __func__);
				manager->uvdm_error = -EPROTO;
			}
		}
		complete(&manager->uvdm_out_wait);
		pr_err("%s DIR_OUT complete!\n", __func__);
	}else if (manager->uvdm_dir == DIR_IN) {
		if (manager->uvdm_first_req) { /* LONG and SHORT response is same */
			SEC_UVDM_RES_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_RES_HEADER.cmd_type == RES_ACK) {
				pr_err("%s DIR_IN 1st Response\n", __func__);
			} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_NAK) {
				pr_err("%s DIR_IN SEC_UVDM_RESPONDER_NAK received\n", __func__);
				manager->uvdm_error = -ENODATA;
			} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_BUSY) {
				pr_err("%s DIR_IN SEC_UVDM_RESPONDER_BUSY received\n", __func__);
				manager->uvdm_error = -EBUSY;
			} else {
				pr_err("%s DIR_IN Undefined RESPONDER value\n", __func__);
				manager->uvdm_error = -EPROTO;
			}
		} else {
			/* don't have ack packet after 2nd sec_tx_data_header */
			SEC_UVDM_TX_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_TX_HEADER.object != 0)
				pr_err("%s DIR_IN %dth Response\n", __func__, SEC_UVDM_TX_HEADER.order_cur_set);
			else
				pr_err("%s DIR_IN Last Response. It's zero\n", __func__);
		}
		complete(&manager->uvdm_in_wait);
		pr_err("%s DIR_IN complete!\n", __func__);
	} else {
		pr_err("%s Never Call!!!\n", __func__);
	}
}

void usbpd_manager_plug_attach(struct device *dev, muic_attached_dev_t new_dev)
{
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct policy_data *policy = &pd_data->policy;
	struct usbpd_manager_data *manager = &pd_data->manager;

	if (new_dev == ATTACHED_DEV_TYPE3_CHARGER_MUIC) {
		if (policy->send_sink_cap || (manager->ps_rdy == 1 &&
		manager->prev_available_pdo != pd_noti.sink_status.available_pdo_num)) {
			pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK_CAP;
			policy->send_sink_cap = 0;
		} else
			pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK;
		manager->ps_rdy = 1;
		manager->prev_available_pdo = pd_noti.sink_status.available_pdo_num;
		pd_data->phy_ops.send_pd_info(pd_data, 1);
	}
#else
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s: usbpd plug attached\n", __func__);
	manager->attached_dev = new_dev;
	s2m_pdic_notifier_attach_attached_dev(manager->attached_dev);
#endif
#endif
}

void usbpd_manager_plug_detach(struct device *dev, bool notify)
{
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s: usbpd plug detached\n", __func__);

	usbpd_policy_reset(pd_data, PLUG_DETACHED);
	if (notify)
		s2m_pdic_notifier_detach_attached_dev(manager->attached_dev);
	manager->attached_dev = ATTACHED_DEV_NONE_MUIC;

	if (pd_noti.event != PDIC_NOTIFY_EVENT_DETACH) {
		pd_noti.event = PDIC_NOTIFY_EVENT_DETACH;		
		pd_data->phy_ops.send_pd_info(pd_data, 0);
	}
#endif
#endif
}

void usbpd_manager_acc_detach(struct device *dev)
{
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s\n", __func__);
	if ( manager->acc_type != CCIC_DOCK_DETACHED ) {
		pr_info("%s: schedule_delayed_work \n", __func__);
		if ( manager->acc_type == CCIC_DOCK_HMT )
			schedule_delayed_work(&manager->acc_detach_handler, msecs_to_jiffies(1000));
		else
			schedule_delayed_work(&manager->acc_detach_handler, msecs_to_jiffies(0));
	}
}

int usbpd_manager_command_to_policy(struct device *dev,
		usbpd_manager_command_type command)
{
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;

	manager->cmd |= command;

	usbpd_kick_policy_work(dev);

	/* TODO: check result
	if (manager->event) {
	 ...
	}
	*/
	return 0;
}

void usbpd_manager_inform_event(struct usbpd_data *pd_data,
		usbpd_manager_event_type event)
{
	struct usbpd_manager_data *manager = &pd_data->manager;

	manager->event = event;

	switch (event) {
	case MANAGER_DISCOVER_IDENTITY_ACKED:
		usbpd_manager_get_identity(pd_data);
		usbpd_manager_command_to_policy(pd_data->dev,
				MANAGER_REQ_VDM_DISCOVER_SVID);
		break;
	case MANAGER_DISCOVER_SVID_ACKED:
		usbpd_manager_get_svids(pd_data);
		usbpd_manager_command_to_policy(pd_data->dev,
				MANAGER_REQ_VDM_DISCOVER_MODE);
		break;
	case MANAGER_DISCOVER_MODE_ACKED:
		usbpd_manager_get_modes(pd_data);
		usbpd_manager_command_to_policy(pd_data->dev,
				MANAGER_REQ_VDM_ENTER_MODE);
		break;
	case MANAGER_ENTER_MODE_ACKED:
		usbpd_manager_enter_mode(pd_data);
		usbpd_manager_command_to_policy(pd_data->dev,
				MANAGER_REQ_VDM_STATUS_UPDATE);
		break;
	case MANAGER_STATUS_UPDATE_ACKED:
		usbpd_manager_command_to_policy(pd_data->dev,
			MANAGER_REQ_VDM_DisplayPort_Configure);
		break;
	case MANAGER_DisplayPort_Configure_ACKED:
		break;
	case MANAGER_NEW_POWER_SRC:
		usbpd_manager_command_to_policy(pd_data->dev,
				MANAGER_REQ_NEW_POWER_SRC);
		break;
	case MANAGER_GET_SRC_CAP:
		usbpd_manager_command_to_policy(pd_data->dev,
				MANAGER_REQ_GET_SRC_CAP);
		break;
	case MANAGER_UVDM_SEND_MESSAGE:
		usbpd_manager_command_to_policy(pd_data->dev,
				MANAGER_REQ_UVDM_SEND_MESSAGE);
		break;
	case MANAGER_UVDM_RECEIVE_MESSAGE:
		usbpd_manager_receive_samsung_uvdm_message(pd_data);
		break;
	case MANAGER_START_DISCOVER_IDENTITY:
		usbpd_manager_command_to_policy(pd_data->dev,
					MANAGER_REQ_VDM_DISCOVER_IDENTITY);
		break;
	case MANAGER_SEND_PR_SWAP:
		usbpd_manager_command_to_policy(pd_data->dev,
					MANAGER_REQ_PR_SWAP);
		break;
	case MANAGER_SEND_DR_SWAP:
		usbpd_manager_command_to_policy(pd_data->dev,
					MANAGER_REQ_DR_SWAP);
		break;
	case MANAGER_CAP_MISMATCH:
		usbpd_manager_command_to_policy(pd_data->dev,
				MANAGER_REQ_GET_SNKCAP);
	default:
		pr_info("%s: not matched event(%d)\n", __func__, event);
	}
}

bool usbpd_manager_vdm_request_enabled(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;
	/* TODO : checking cable discovering
	   if (pd_data->counter.discover_identity_counter
		   < USBPD_nDiscoverIdentityCount)

	   struct usbpd_manager_data *manager = &pd_data->manager;
	   if (manager->event != MANAGER_DISCOVER_IDENTITY_ACKED
	      || manager->event != MANAGER_DISCOVER_IDENTITY_NAKED)

	   return(1);
	*/

	manager->vdm_en = 1;

	schedule_delayed_work(&manager->start_discover_msg_handler, msecs_to_jiffies(30));
	return true;
}

bool usbpd_manager_power_role_swap(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;

	return manager->power_role_swap;
}

bool usbpd_manager_vconn_source_swap(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;

	return manager->vconn_source_swap;
}

void usbpd_manager_turn_off_vconn(struct usbpd_data *pd_data)
{
	/* TODO : Turn off vconn */
}

void usbpd_manager_turn_on_source(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s: usbpd plug attached\n", __func__);

	manager->attached_dev = ATTACHED_DEV_TYPE3_ADAPTER_MUIC;
	s2m_pdic_notifier_attach_attached_dev(manager->attached_dev);
	/* TODO : Turn on source */
}

void usbpd_manager_turn_off_power_supply(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s: usbpd plug detached\n", __func__);

	s2m_pdic_notifier_detach_attached_dev(manager->attached_dev);
	manager->attached_dev = ATTACHED_DEV_NONE_MUIC;
	/* TODO : Turn off power supply */
}

void usbpd_manager_turn_off_power_sink(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s: usbpd sink turn off\n", __func__);

	s2m_pdic_notifier_detach_attached_dev(manager->attached_dev);
	manager->attached_dev = ATTACHED_DEV_NONE_MUIC;
	/* TODO : Turn off power sink */
}

bool usbpd_manager_data_role_swap(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;

	return manager->data_role_swap;
}

int usbpd_manager_register_switch_device(int mode)
{
#ifdef CONFIG_SWITCH
	int ret = 0;
	if (mode) {
		ret = switch_dev_register(&switch_dock);
		if (ret < 0) {
			pr_err("%s: Failed to register dock switch(%d)\n",
			       __func__, ret);
			return -ENODEV;
		}
	} else {
		switch_dev_unregister(&switch_dock);
	}
#endif /* CONFIG_SWITCH */
	return 0;
}

static void usbpd_manager_send_dock_intent(int type)
{
	pr_info("%s: CCIC dock type(%d)\n", __func__, type);
#ifdef CONFIG_SWITCH
	switch_set_state(&switch_dock, type);
#endif /* CONFIG_SWITCH */
}

void usbpd_manager_send_dock_uevent(u32 vid, u32 pid, int state)
{
	char switch_string[32];
	char pd_ids_string[32];
	char *envp[3] = { switch_string, pd_ids_string, NULL };

	pr_info("%s: CCIC dock : USBPD_IPS=%04x:%04x SWITCH_STATE=%d\n",
			__func__,
			le16_to_cpu(vid),
			le16_to_cpu(pid),
			state);

	if (IS_ERR(ccic_device)) {
		pr_err("%s CCIC ERROR: Failed to send a dock uevent!\n",
				__func__);
		return;
	}

	snprintf(switch_string, 32, "SWITCH_STATE=%d", state);
	snprintf(pd_ids_string, 32, "USBPD_IDS=%04x:%04x",
			le16_to_cpu(vid),
			le16_to_cpu(pid));
	kobject_uevent_env(&ccic_device->kobj, KOBJ_CHANGE, envp);
}

void usbpd_manager_acc_detach_handler(struct work_struct *wk)
{
//	struct delayed_work *delay_work =
//		container_of(wk, struct delayed_work, work);
	struct usbpd_manager_data *manager =
		container_of(wk, struct usbpd_manager_data, acc_detach_handler.work);

	pr_info("%s: attached_dev : %d ccic dock type %d\n", __func__, manager->attached_dev, manager->acc_type);
	if (manager->attached_dev == ATTACHED_DEV_NONE_MUIC) {
		if (manager->acc_type != CCIC_DOCK_DETACHED) {
			if (manager->acc_type != CCIC_DOCK_NEW)
				usbpd_manager_send_dock_intent(CCIC_DOCK_DETACHED);
			usbpd_manager_send_dock_uevent(manager->Vendor_ID, manager->Product_ID,
					CCIC_DOCK_DETACHED);
			manager->acc_type = CCIC_DOCK_DETACHED;
			manager->Vendor_ID = 0;
			manager->Product_ID = 0;
			manager->is_samsung_accessory_enter_mode = false;
		}
	}
}

void usbpd_manager_acc_handler_cancel(struct device *dev)
{
	struct usbpd_data *pd_data = dev_get_drvdata(dev);
	struct usbpd_manager_data *manager = &pd_data->manager;

	if (manager->acc_type != CCIC_DOCK_DETACHED) {
		pr_info("%s: cancel_delayed_work_sync \n", __func__);
		cancel_delayed_work_sync(&manager->acc_detach_handler);
	}
}

static int usbpd_manager_check_accessory(struct usbpd_manager_data *manager)
{
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif
	struct usbpd_data *pd_data = manager->pd_data;
	uint16_t vid = manager->Vendor_ID;
	uint16_t pid = manager->Product_ID;
	uint16_t acc_type = CCIC_DOCK_DETACHED;

	/* detect Gear VR */
	if (manager->acc_type == CCIC_DOCK_DETACHED) {
		if (vid == SAMSUNG_VENDOR_ID) {
			switch (pid) {
			/* GearVR: Reserved GearVR PID+6 */
			case GEARVR_PRODUCT_ID:
			case GEARVR_PRODUCT_ID_1:
			case GEARVR_PRODUCT_ID_2:
			case GEARVR_PRODUCT_ID_3:
			case GEARVR_PRODUCT_ID_4:
			case GEARVR_PRODUCT_ID_5:
				acc_type = CCIC_DOCK_HMT;
				pr_info("%s : Samsung Gear VR connected.\n", __func__);
#if defined(CONFIG_USB_HW_PARAM)
				if (o_notify)
					inc_hw_param(o_notify, USB_CCIC_VR_USE_COUNT);
#endif
				break;
			case DEXDOCK_PRODUCT_ID:
				acc_type = CCIC_DOCK_DEX;
				pr_info("%s : Samsung DEX connected.\n", __func__);
#if defined(CONFIG_USB_HW_PARAM)
				if (o_notify)
					inc_hw_param(o_notify, USB_CCIC_DEX_USE_COUNT);
#endif
				break;
			case HDMI_PRODUCT_ID:
				acc_type = CCIC_DOCK_HDMI;
				pr_info("%s : Samsung HDMI connected.\n", __func__);
				break;
			case MPA2_PRODUCT_ID:
				acc_type = CCIC_DOCK_NEW;
				pr_info("%s : Samsung MPA2 connected. need to SRCCAP_CHANGE\n", __func__);
				usbpd_manager_command_to_policy(pd_data->dev,
						MANAGER_REQ_SRCCAP_CHANGE);
				break;
			default:
				acc_type = CCIC_DOCK_NEW;
				pr_info("%s : default device connected.\n", __func__);
				break;
			}
		} else if (vid == SAMSUNG_MPA_VENDOR_ID) {
			switch(pid) {
			case MPA_PRODUCT_ID:
				acc_type = CCIC_DOCK_MPA;
				pr_info("%s : Samsung MPA connected.\n", __func__);
				break;
			default:
				acc_type = CCIC_DOCK_NEW;
				pr_info("%s : default device connected.\n", __func__);
				break;
			}
		}
		manager->acc_type = acc_type;
	} else
		acc_type = manager->acc_type;

	if (acc_type != CCIC_DOCK_NEW)
		usbpd_manager_send_dock_intent(acc_type);

	usbpd_manager_send_dock_uevent(vid, pid, acc_type);
	return 1;
}

/* Ok : 0, NAK: -1 */
int usbpd_manager_get_identity(struct usbpd_data *pd_data)
{
	struct policy_data *policy = &pd_data->policy;
	struct usbpd_manager_data *manager = &pd_data->manager;

	manager->Vendor_ID = policy->rx_data_obj[1].id_header_vdo.USB_Vendor_ID;
	manager->Product_ID = policy->rx_data_obj[3].product_vdo.USB_Product_ID;
	manager->Device_Version = policy->rx_data_obj[3].product_vdo.Device_Version;

	pr_info("%s, Vendor_ID : 0x%x, Product_ID : 0x%x, Device Version : 0x%x\n",
			__func__, manager->Vendor_ID, manager->Product_ID, manager->Device_Version);

	if (usbpd_manager_check_accessory(manager))
		pr_info("%s, Samsung Accessory Connected.\n", __func__);

	return 0;
}

/* Ok : 0 (SVID_0 is DP support(0xff01)), NAK: -1 */
int usbpd_manager_get_svids(struct usbpd_data *pd_data)
{
	struct policy_data *policy = &pd_data->policy;
	struct usbpd_manager_data *manager = &pd_data->manager;

	manager->SVID_0 = policy->rx_data_obj[1].vdm_svid.svid_0;
	manager->SVID_1 = policy->rx_data_obj[1].vdm_svid.svid_1;

	pr_info("%s, SVID_0 : 0x%x, SVID_1 : 0x%x\n", __func__,
				manager->SVID_0, manager->SVID_1);

	if (manager->SVID_0 == TypeC_DP_SUPPORT)
		return 0;

	return -1;
}

/* Ok : 0 (SVID_0 is DP support(0xff01)), NAK: -1 */
int usbpd_manager_get_modes(struct usbpd_data *pd_data)
{
	struct policy_data *policy = &pd_data->policy;
	struct usbpd_manager_data *manager = &pd_data->manager;

	manager->Standard_Vendor_ID = policy->rx_data_obj[0].structured_vdm.svid;

	pr_info("%s, Standard_Vendor_ID = 0x%x\n", __func__,
				manager->Standard_Vendor_ID);
	return -1;
}

int usbpd_manager_enter_mode(struct usbpd_data *pd_data)
{
	struct policy_data *policy = &pd_data->policy;
	struct usbpd_manager_data *manager = &pd_data->manager;
	manager->Standard_Vendor_ID = policy->rx_data_obj[0].structured_vdm.svid;
	manager->is_samsung_accessory_enter_mode = true;
	return 0;
}

int usbpd_manager_exit_mode(struct usbpd_data *pd_data, unsigned mode)
{
	return 0;
}

data_obj_type usbpd_manager_select_capability(struct usbpd_data *pd_data)
{
	/* TODO: Request from present capabilities
		indicate if other capabilities would be required */
	data_obj_type obj;
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	int pdo_num = pd_noti.sink_status.selected_pdo_num;
#if defined(CONFIG_PDIC_PD30)
	if (pd_noti.sink_status.power_list[pdo_num].apdo) {
		obj.request_data_object_pps.output_voltage = pd_noti.sink_status.pps_voltage / USBPD_PPS_RQ_VOLT_UNIT;
		obj.request_data_object_pps.op_current = pd_noti.sink_status.pps_current / USBPD_PPS_RQ_CURRENT_UNIT;
		obj.request_data_object_pps.no_usb_suspend = 1;
		obj.request_data_object_pps.usb_comm_capable = 1;
		obj.request_data_object_pps.capability_mismatch = 0;
		obj.request_data_object_pps.unchunked_supported = 0;
		obj.request_data_object_pps.object_position =
								pd_noti.sink_status.selected_pdo_num;
		return obj;
	}
#endif
	obj.request_data_object.min_current = pd_noti.sink_status.power_list[pdo_num].max_current / USBPD_CURRENT_UNIT;
	obj.request_data_object.op_current = pd_noti.sink_status.power_list[pdo_num].max_current / USBPD_CURRENT_UNIT;
	obj.request_data_object.object_position = pd_noti.sink_status.selected_pdo_num;
#else
	obj.request_data_object.min_current = 10;
	obj.request_data_object.op_current = 10;
	obj.request_data_object.object_position = 1;
#endif
#endif
	obj.request_data_object.no_usb_suspend = 1;
	obj.request_data_object.usb_comm_capable = 1;
	obj.request_data_object.capability_mismatch = 0;
	obj.request_data_object.give_back = 0;

	return obj;
}

/*
   usbpd_manager_evaluate_capability
   : Policy engine ask Device Policy Manager to evaluate option
     based on supplied capabilities
	return	>0	: request object number
		0	: no selected option
*/
int usbpd_manager_get_selected_voltage(struct usbpd_data *pd_data, int selected_pdo)
{
#if defined(CONFIG_BATTERY_SAMSUNG) && defined(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	PDIC_SINK_STATUS *pdic_sink_status = &pd_noti.sink_status;

	int available_pdo = pdic_sink_status->available_pdo_num;
	int volt = 0;

	if (selected_pdo > available_pdo) {
		pr_info("%s, selected:%d, available:%d\n", __func__, selected_pdo, available_pdo);
		return 0;
	}

#if defined(CONFIG_PDIC_PD30)
	if (pdic_sink_status->power_list[selected_pdo].apdo) {
		pr_info("%s, selected pdo is apdo(%d)\n", __func__, selected_pdo);
		return 0;
	}
#endif

	volt = pdic_sink_status->power_list[selected_pdo].max_voltage;
	pr_info("%s, select_pdo : %d, selected_voltage : %dmV\n", __func__, selected_pdo, volt);

	return volt;
#else
	return 0;
#endif
}

int usbpd_manager_evaluate_capability(struct usbpd_data *pd_data)
{
	struct policy_data *policy = &pd_data->policy;
	int i = 0;
	int power_type = 0;
	int pd_volt = 0, pd_current;
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	struct usbpd_manager_data *manager = &pd_data->manager;
	int available_pdo_num = 0;
	PDIC_SINK_STATUS *pdic_sink_status = &pd_noti.sink_status;
#endif
#endif
	data_obj_type *pd_obj;

#if defined(CONFIG_PDIC_PD30)
	pdic_sink_status->has_apdo = false;
#endif

	for (i = 0; i < policy->rx_msg_header.num_data_objs; i++) {
		pd_obj = &policy->rx_data_obj[i];
		power_type = pd_obj->power_data_obj_supply_type.supply_type;
		switch (power_type) {
		case POWER_TYPE_FIXED:
			pd_volt = pd_obj->power_data_obj.voltage;
			pd_current = pd_obj->power_data_obj.max_current;
			dev_info(pd_data->dev, "[%d] FIXED volt(%d)mV, max current(%d)\n",
					i+1, pd_volt * USBPD_VOLT_UNIT, pd_current * USBPD_CURRENT_UNIT);
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
			available_pdo_num++;
			pdic_sink_status->power_list[i + 1].max_voltage = pd_volt * USBPD_VOLT_UNIT;
			pdic_sink_status->power_list[i + 1].max_current = pd_current * USBPD_CURRENT_UNIT;
#if defined(CONFIG_PDIC_PD30)
			pdic_sink_status->power_list[i + 1].min_voltage = 0;
			pdic_sink_status->power_list[i + 1].apdo = false;
			if (pd_volt * USBPD_VOLT_UNIT > AVAILABLE_VOLTAGE)
				pdic_sink_status->power_list[i + 1].accept = false;
			else
				pdic_sink_status->power_list[i + 1].accept = true;
#endif
#endif
#endif
			break;
		case POWER_TYPE_BATTERY:
			pd_volt = pd_obj->power_data_obj_battery.max_voltage;
			dev_info(pd_data->dev, "[%d] BATTERY volt(%d)mV\n",
					i+1, pd_volt * USBPD_VOLT_UNIT);
			break;
		case POWER_TYPE_VARIABLE:
			pd_volt = pd_obj->power_data_obj_variable.max_voltage;
			dev_info(pd_data->dev, "[%d] VARIABLE volt(%d)mV\n",
					i+1, pd_volt * USBPD_VOLT_UNIT);
			break;
		case POWER_TYPE_PPS:
			dev_info(pd_data->dev, "[%d] PPS (%d)mV-(%d)mV / %dmA\n",
					i+1, pd_obj->power_data_obj_pps.min_voltage * USBPD_PPS_VOLT_UNIT,
					pd_obj->power_data_obj_pps.max_voltage * USBPD_PPS_VOLT_UNIT,
					pd_obj->power_data_obj_pps.max_current * USBPD_PPS_CURRENT_UNIT);
			pd_volt = pd_obj->power_data_obj_pps.max_voltage;
			pd_current = pd_obj->power_data_obj_pps.max_current;
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
			available_pdo_num = i + 1;
			pdic_sink_status->power_list[i + 1].max_voltage = pd_volt * USBPD_PPS_VOLT_UNIT;
			pdic_sink_status->power_list[i + 1].max_current = pd_current * USBPD_PPS_CURRENT_UNIT;
#if defined(CONFIG_PDIC_PD30)
			pdic_sink_status->has_apdo = true;
			pdic_sink_status->power_list[i + 1].apdo = true;
			pdic_sink_status->power_list[i + 1].min_voltage =
					pd_obj->power_data_obj_pps.min_voltage * USBPD_PPS_VOLT_UNIT;
#endif
#endif
#endif
			break;
		default:
			dev_err(pd_data->dev, "[%d] Power Type Error\n", i+1);
			break;
		}
	}
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	if (manager->flash_mode == 1)
		available_pdo_num = 1;
	if ((available_pdo_num > 0) &&
			(pdic_sink_status->available_pdo_num != available_pdo_num)) {
		pdic_sink_status->selected_pdo_num = 1;
		policy->send_sink_cap = 1;
	}
	pdic_sink_status->available_pdo_num = available_pdo_num;
	return available_pdo_num;
#endif
#else
	return 1; /* select default first obj */
#endif
}

/* return: 0: cab be met, -1: cannot be met, -2: could be met later */
int usbpd_manager_match_request(struct usbpd_data *pd_data)
{
	/* TODO: Evaluation of sink request */
	unsigned supply_type
	= pd_data->source_request_obj.power_data_obj_supply_type.supply_type;
	unsigned src_max_current,  mismatch, max_min, op, pos, giveback;
	struct policy_data *policy = &pd_data->policy;

	if (supply_type == POWER_TYPE_FIXED)
		pr_info("REQUEST: FIXED\n");
	else if (supply_type == POWER_TYPE_VARIABLE)
		pr_info("REQUEST: VARIABLE\n");
	else if (supply_type == POWER_TYPE_PPS)
		pr_info("REQUEST: PPS\n");
	else if (supply_type == POWER_TYPE_BATTERY) {
		pr_info("REQUEST: BATTERY\n");
		goto log_battery;
	}else {
		pr_info("REQUEST: UNKNOWN Supply type.\n");
		return -1;
	}

    /* Tx Source PDO */
	src_max_current = pd_data->source_data_obj.power_data_obj.max_current;

    /* Rx Request RDO */
	mismatch = pd_data->source_request_obj.request_data_object.capability_mismatch;
	max_min = pd_data->source_request_obj.request_data_object.min_current;
	op = pd_data->source_request_obj.request_data_object.op_current;
	pos = pd_data->source_request_obj.request_data_object.object_position;
	giveback = pd_data->source_request_obj.request_data_object.give_back;

    /*src_max_current is already *10 value ex) src_max_current 500mA */
	pr_info("Tx SourceCap Current : %dmA\n", src_max_current*10);
	pr_info("Rx Request Current : %dmA\n", max_min*10);

	policy->cap_mismatch = mismatch;
    /* Compare Pdo and Rdo */
	if ((src_max_current >= op) && (pos == 1) && ((giveback == 0) && (src_max_current >= max_min)))
		return 0;
	else
		return -1;

log_battery:
	mismatch = pd_data->source_request_obj.request_data_object_battery.capability_mismatch;
	return 0;
}

#ifdef CONFIG_OF
static int of_usbpd_manager_dt(struct usbpd_manager_data *_data)
{
	int ret = 0;
	struct device_node *np =
		of_find_node_by_name(NULL, "pdic-manager");

	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
		return -EINVAL;
	} else {
		ret = of_property_read_u32(np, "pdic,max_power",
				&_data->max_power);
		if (ret < 0)
			pr_err("%s error reading max_power %d\n",
					__func__, _data->max_power);

		ret = of_property_read_u32(np, "pdic,op_power",
				&_data->op_power);
		if (ret < 0)
			pr_err("%s error reading op_power %d\n",
					__func__, _data->max_power);

		ret = of_property_read_u32(np, "pdic,max_current",
				&_data->max_current);
		if (ret < 0)
			pr_err("%s error reading max_current %d\n",
					__func__, _data->max_current);

		ret = of_property_read_u32(np, "pdic,min_current",
				&_data->min_current);
		if (ret < 0)
			pr_err("%s error reading min_current %d\n",
					__func__, _data->min_current);

		_data->giveback = of_property_read_bool(np,
						     "pdic,giveback");
		_data->usb_com_capable = of_property_read_bool(np,
						     "pdic,usb_com_capable");
		_data->no_usb_suspend = of_property_read_bool(np,
						     "pdic,no_usb_suspend");

		/* source capability */
		ret = of_property_read_u32(np, "source,max_voltage",
				&_data->source_max_volt);
		ret = of_property_read_u32(np, "source,min_voltage",
				&_data->source_min_volt);
		ret = of_property_read_u32(np, "source,max_power",
				&_data->source_max_power);

		/* sink capability */
		ret = of_property_read_u32(np, "sink,capable_max_voltage",
				&_data->sink_cap_max_volt);
		if (ret < 0) {
			_data->sink_cap_max_volt = 5000;
			pr_err("%s error reading sink_cap_max_volt %d\n",
					__func__, _data->sink_cap_max_volt);
		}
	}

	return ret;
}
#endif

void usbpd_init_manager_val(struct usbpd_data *pd_data)
{
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s\n", __func__);
	manager->alt_sended = 0;
	manager->cmd = 0;
	manager->vdm_en = 0;
	manager->Vendor_ID = 0;
	manager->Product_ID = 0;
	manager->Device_Version = 0;
	manager->SVID_0 = 0;
	manager->SVID_1 = 0;
	manager->Standard_Vendor_ID = 0;
	manager->prev_available_pdo = 0;
	manager->ps_rdy = 0;
	manager->uvdm_error = 0;
	reinit_completion(&manager->uvdm_out_wait);
	reinit_completion(&manager->uvdm_in_wait);
	reinit_completion(&manager->psrdy_wait);
	usbpd_manager_select_pdo_cancel(pd_data->dev);
	usbpd_manager_start_discover_msg_cancel(pd_data->dev);
}

int usbpd_init_manager(struct usbpd_data *pd_data)
{
	int ret = 0;
	struct usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s\n", __func__);
	if (manager == NULL) {
		pr_err("%s, usbpd manager data is error!!\n", __func__);
		return -ENOMEM;
	} else
		ret = of_usbpd_manager_dt(manager);
#ifdef CONFIG_BATTERY_SAMSUNG
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	fp_select_pdo = usbpd_manager_select_pdo;
#if defined(CONFIG_PDIC_PD30)
	fp_sec_pd_select_pps = usbpd_manager_select_pps;
	fp_sec_pd_get_apdo_max_power = usbpd_manager_get_apdo_max_power;
	fp_pps_enable = usbpd_manager_pps_enable;
	fp_get_pps_voltage = usbpd_manager_get_pps_voltage;
#endif
#endif
#endif
	mutex_init(&manager->vdm_mutex);
	mutex_init(&manager->pdo_mutex);
	manager->pd_data = pd_data;
	manager->power_role_swap = true;
	manager->data_role_swap = true;
	manager->vconn_source_swap = 0;
	manager->alt_sended = 0;
	manager->vdm_en = 0;
	manager->acc_type = 0;
	manager->Vendor_ID = 0;
	manager->Product_ID = 0;
	manager->Device_Version = 0;
	manager->SVID_0 = 0;
	manager->SVID_1 = 0;
	manager->Standard_Vendor_ID = 0;

	manager->flash_mode = 0;
	manager->prev_available_pdo = 0;
	manager->ps_rdy = 0;

	init_completion(&manager->uvdm_out_wait);
	init_completion(&manager->uvdm_in_wait);
	init_completion(&manager->psrdy_wait);

	usbpd_manager_register_switch_device(1);
	init_source_cap_data(manager);
	init_sink_cap_data(manager);
	INIT_DELAYED_WORK(&manager->acc_detach_handler, usbpd_manager_acc_detach_handler);
	INIT_DELAYED_WORK(&manager->select_pdo_handler, usbpd_manager_select_pdo_handler);
	INIT_DELAYED_WORK(&manager->start_discover_msg_handler,
				usbpd_manager_start_discover_msg_handler);

	ret = ccic_misc_init();
	if (ret) {
		pr_info("ccic misc register is failed, error %d\n", ret);
	}
	pr_info("%s done\n", __func__);
	return ret;
}
