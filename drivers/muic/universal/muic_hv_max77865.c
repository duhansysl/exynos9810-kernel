/*
 * muic_hv_afc.c
 *
 *  Copyright (C) 2012 Samsung Electronics
 *  Thomas Ryu <smilesr.ryu@samsung.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/delay.h>

#include <linux/mfd/max77865.h>
#include <linux/mfd/max77865-private.h>

/* MUIC header file */
#include <linux/muic/muic.h>
#include "muic-internal.h"
#include "muic_state.h"
#include "muic_i2c.h"
#include "muic_vps.h"
#include "muic_apis.h"

#include "muic_hv.h"
#include "muic_hv_max77865.h"

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_MUIC_SUPPORT_CCIC)
#include "muic_ccic.h"
#endif

static bool debug_en_checklist = false;

/* temp function for function pointer (TODO) */
enum act_function_num {
	FUNC_TA_TO_PREPARE		= 0,
	FUNC_PREPARE_TO_PREPARE_DUPLI,
	FUNC_PREPARE_TO_AFC_5V,
	FUNC_PREPARE_TO_QC_PREPARE,
	FUNC_PREPARE_DUPLI_TO_PREPARE_DUPLI,
	FUNC_PREPARE_DUPLI_TO_AFC_5V,
	FUNC_PREPARE_DUPLI_TO_AFC_ERR_V,
	FUNC_PREPARE_DUPLI_TO_AFC_9V,
#if defined(CONFIG_MUIC_HV_12V)
	FUNC_PREPARE_DUPLI_TO_AFC_12V,
#endif
	FUNC_PREPARE_DUPLI_TO_QC_PREPARE,
	FUNC_AFC_5V_TO_AFC_5V_DUPLI,
	FUNC_AFC_5V_TO_AFC_ERR_V,
	FUNC_AFC_5V_TO_AFC_9V,
#if defined(CONFIG_MUIC_HV_12V)
	FUNC_AFC_5V_TO_AFC_12V,
#endif
	FUNC_AFC_5V_TO_QC_PREPARE,
	FUNC_AFC_5V_DUPLI_TO_AFC_5V_DUPLI,
	FUNC_AFC_5V_DUPLI_TO_AFC_ERR_V,
	FUNC_AFC_5V_DUPLI_TO_AFC_9V,
#if defined(CONFIG_MUIC_HV_12V)
	FUNC_AFC_5V_DUPLI_TO_AFC_12V,
#endif
	FUNC_AFC_5V_DUPLI_TO_QC_PREPARE,
	FUNC_AFC_ERR_V_TO_AFC_ERR_V_DUPLI,
	FUNC_AFC_ERR_V_TO_AFC_5V,
	FUNC_AFC_ERR_V_TO_AFC_9V,
	FUNC_AFC_ERR_V_TO_QC_PREPARE,
#if defined(CONFIG_MUIC_HV_12V)
	FUNC_AFC_ERR_V_TO_AFC_12V,
#endif
	FUNC_AFC_ERR_V_DUPLI_TO_AFC_ERR_V_DUPLI,
	FUNC_AFC_ERR_V_DUPLI_TO_AFC_5V,
	FUNC_AFC_ERR_V_DUPLI_TO_AFC_9V,
#if defined(CONFIG_MUIC_HV_12V)
	FUNC_AFC_ERR_V_DUPLI_TO_AFC_12V,
#endif
	FUNC_AFC_ERR_V_DUPLI_TO_QC_PREPARE,
	FUNC_AFC_9V_TO_AFC_9V_DUPLI,
	FUNC_AFC_9V_TO_AFC_ERR_V,
	FUNC_AFC_9V_TO_AFC_5V,
	FUNC_AFC_9V_TO_QC_PREPARE,
	FUNC_AFC_9V_DUPLI_TO_AFC_ERR_V,
	FUNC_AFC_9V_DUPLI_TO_AFC_5V,
	FUNC_AFC_9V_DUPLI_TO_AFC_9V_DUPLI,
	FUNC_AFC_9V_DUPLI_TO_QC_PREPARE,
#if defined(CONFIG_MUIC_HV_12V)
	FUNC_AFC_12V_TO_AFC_12V_DUPLI,
	FUNC_AFC_12V_TO_AFC_ERR_V,
	FUNC_AFC_12V_TO_AFC_5V,
	FUNC_AFC_12V_TO_AFC_9V,
	FUNC_AFC_12V_TO_QC_PREPARE,
	FUNC_AFC_12V_DUPLI_TO_AFC_ERR_V,
	FUNC_AFC_12V_DUPLI_TO_AFC_5V,
	FUNC_AFC_12V_DUPLI_TO_AFC_9V,
	FUNC_AFC_12V_DUPLI_TO_AFC_12V_DUPLI,
	FUNC_AFC_12V_DUPLI_TO_QC_PREPARE,
#endif
	FUNC_QC_PREPARE_TO_QC_5V,
	FUNC_QC_PREPARE_TO_QC_9V,
	FUNC_QC_5V_TO_QC_9V,
	FUNC_QC_9V_TO_QC_5V,
};

static struct hv_data hv_afc;

/* afc_condition_checklist[ATTACHED_DEV_TA_MUIC] */
muic_afc_data_t ta_to_prepare = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC,
	.afc_name		= "AFC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_VDNMON,
	.bccontrol2_dpdnman	= DPDNVDEN_ENABLE,
	.gpstatus_vbadc		= VBADC_DONTCARE,
	.bcstatus2_vdnmon	= VDNMON_LOW,
	.function_num		= FUNC_TA_TO_PREPARE,
	.next			= &ta_to_prepare,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC] */
muic_afc_data_t prepare_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.bccontrol2_dpdnman	= DPDNVDEN_DONTCARE,
	.gpstatus_vbadc		= VBADC_DONTCARE,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_TO_QC_PREPARE,
	.next			= &prepare_to_qc_prepare,
};

muic_afc_data_t prepare_to_afc_5v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_5V_MUIC,
	.afc_name		= "AFC charger 5V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_5V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_TO_AFC_5V,
	.next			= &prepare_to_qc_prepare,
};

muic_afc_data_t prepare_to_prepare_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC,
	.afc_name		= "AFC charger prepare (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_DONTCARE,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_TO_PREPARE_DUPLI,
	.next			= &prepare_to_afc_5v,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC] */
muic_afc_data_t prepare_dupli_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.bccontrol2_dpdnman	= DPDNVDEN_DONTCARE,
	.gpstatus_vbadc		= VBADC_DONTCARE,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_DUPLI_TO_QC_PREPARE,
	.next			= &prepare_dupli_to_qc_prepare,
};

muic_afc_data_t prepare_dupli_to_prepare_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC,
	.afc_name		= "AFC charger prepare (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_DONTCARE,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_DUPLI_TO_PREPARE_DUPLI,
	.next			= &prepare_dupli_to_qc_prepare,
};

#if defined(CONFIG_MUIC_HV_12V)
muic_afc_data_t prepare_dupli_to_afc_12v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_12V_MUIC,
	.afc_name		= "AFC charger 12V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_12V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_DUPLI_TO_AFC_12V,
	.next			= &prepare_dupli_to_prepare_dupli,
};
#endif

muic_afc_data_t prepare_dupli_to_afc_9v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
	.afc_name		= "AFC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_9V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_DUPLI_TO_AFC_9V,
#if defined(CONFIG_MUIC_HV_12V)
	.next			= &prepare_dupli_to_afc_12v,
#else
	.next			= &prepare_dupli_to_prepare_dupli,
#endif
};

muic_afc_data_t prepare_dupli_to_afc_err_v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC,
	.afc_name		= "AFC charger ERR V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_ERR_V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_DUPLI_TO_AFC_ERR_V,
	.next			= &prepare_dupli_to_afc_9v,
};

muic_afc_data_t prepare_dupli_to_afc_5v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_5V_MUIC,
	.afc_name		= "AFC charger 5V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_5V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_PREPARE_DUPLI_TO_AFC_5V,
	.next			= &prepare_dupli_to_afc_err_v,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_5V_MUIC] */
muic_afc_data_t afc_5v_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.bccontrol2_dpdnman	= DPDNVDEN_DONTCARE,
	.gpstatus_vbadc		= VBADC_DONTCARE,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_TO_QC_PREPARE,
	.next			= &afc_5v_to_qc_prepare,
};

#if defined(CONFIG_MUIC_HV_12V)
muic_afc_data_t afc_5v_to_afc_12v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_12V_MUIC,
	.afc_name		= "AFC charger 12V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_12V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_TO_AFC_12V,
	.next			= &afc_5v_to_qc_prepare,
};
#endif

muic_afc_data_t afc_5v_to_afc_9v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
	.afc_name		= "AFC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_9V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_TO_AFC_9V,
#if defined(CONFIG_MUIC_HV_12V)
	.next			= &afc_5v_to_afc_12v,
#else
	.next			= &afc_5v_to_qc_prepare,
#endif
};

muic_afc_data_t afc_5v_to_afc_err_v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC,
	.afc_name		= "AFC charger ERR V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_ERR_V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_TO_AFC_ERR_V,
	.next			= &afc_5v_to_afc_9v,
};

muic_afc_data_t afc_5v_to_afc_5v_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC,
	.afc_name		= "AFC charger 5V (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_5V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_TO_AFC_5V_DUPLI,
	.next			= &afc_5v_to_afc_err_v,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC] */
muic_afc_data_t afc_5v_dupli_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.bccontrol2_dpdnman	= DPDNVDEN_DONTCARE,
	.gpstatus_vbadc		= VBADC_DONTCARE,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_DUPLI_TO_QC_PREPARE,
	.next			= &afc_5v_dupli_to_qc_prepare,
};

#if defined(CONFIG_MUIC_HV_12V)
muic_afc_data_t afc_5v_dupli_to_afc_12v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_12V_MUIC,
	.afc_name		= "AFC charger 12V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_12V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_DUPLI_TO_AFC_12V,
	.next			= &afc_5v_dupli_to_qc_prepare,
};

muic_afc_data_t afc_5v_dupli_to_afc_5v_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC,
	.afc_name		= "AFC charger 5V (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_5V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_DUPLI_TO_AFC_5V_DUPLI,
	.next			= &afc_5v_dupli_to_afc_12v,
};
#else
muic_afc_data_t afc_5v_dupli_to_afc_5v_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC,
	.afc_name		= "AFC charger 5V (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_5V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_DUPLI_TO_AFC_5V_DUPLI,
	.next			= &afc_5v_dupli_to_qc_prepare,
};
#endif

muic_afc_data_t afc_5v_dupli_to_afc_9v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
	.afc_name		= "AFC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_9V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_DUPLI_TO_AFC_9V,
	.next			= &afc_5v_dupli_to_afc_5v_dupli,
};

muic_afc_data_t afc_5v_dupli_to_afc_err_v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC,
	.afc_name		= "AFC charger ERR V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_ERR_V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_5V_DUPLI_TO_AFC_ERR_V,
	.next			= &afc_5v_dupli_to_afc_9v,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC] */
muic_afc_data_t afc_err_v_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.bccontrol2_dpdnman	= DPDNVDEN_DONTCARE,
	.gpstatus_vbadc		= VBADC_DONTCARE,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_TO_QC_PREPARE,
	.next			= &afc_err_v_to_qc_prepare,
};

#if defined(CONFIG_MUIC_HV_12V)
muic_afc_data_t afc_err_v_to_afc_12v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_12V_MUIC,
	.afc_name		= "AFC charger 12V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_12V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_TO_AFC_12V,
	.next			= &afc_err_v_to_qc_prepare,
};
#endif

muic_afc_data_t afc_err_v_to_afc_5v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_5V_MUIC,
	.afc_name		= "AFC charger 5V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_5V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_TO_AFC_5V,
#if defined(CONFIG_MUIC_HV_12V)
	.next			= &afc_err_v_to_afc_12v,
#else
	.next			= &afc_err_v_to_qc_prepare,
#endif
};

muic_afc_data_t afc_err_v_to_afc_9v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
	.afc_name		= "AFC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_9V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_TO_AFC_9V,
	.next			= &afc_err_v_to_afc_5v,
};

muic_afc_data_t afc_err_v_to_afc_err_v_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC,
	.afc_name		= "AFC charger ERR V (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_ERR_V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_TO_AFC_ERR_V_DUPLI,
	.next			= &afc_err_v_to_afc_9v,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC] */
muic_afc_data_t afc_err_v_dupli_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.bccontrol2_dpdnman	= DPDNVDEN_DONTCARE,
	.gpstatus_vbadc		= VBADC_DONTCARE,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_DUPLI_TO_QC_PREPARE,
	.next			= &afc_err_v_dupli_to_qc_prepare,
};

#if defined(CONFIG_MUIC_HV_12V)
muic_afc_data_t afc_err_v_dupli_to_afc_12v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_12V_MUIC,
	.afc_name		= "AFC charger 12V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_12V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_DUPLI_TO_AFC_12V,
	.next			= &afc_err_v_dupli_to_qc_prepare,
};
#endif

muic_afc_data_t afc_err_v_dupli_to_afc_5v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_5V_MUIC,
	.afc_name		= "AFC charger 5V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_5V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_DUPLI_TO_AFC_5V,
#if defined(CONFIG_MUIC_HV_12V)
	.next			= &afc_err_v_dupli_to_afc_12v,
#else
	.next			= &afc_err_v_dupli_to_qc_prepare,
#endif
};

muic_afc_data_t afc_err_v_dupli_to_afc_9v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
	.afc_name		= "AFC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_9V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_DUPLI_TO_AFC_9V,
	.next			= &afc_err_v_dupli_to_afc_5v,
};

muic_afc_data_t afc_err_v_dupli_to_afc_err_v_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC,
	.afc_name		= "AFC charger ERR V (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_ERR_V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_ERR_V_DUPLI_TO_AFC_ERR_V_DUPLI,
	.next			= &afc_err_v_dupli_to_afc_9v,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_9V_MUIC] */
muic_afc_data_t afc_9v_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.bccontrol2_dpdnman	= DPDNVDEN_DONTCARE,
	.gpstatus_vbadc		= VBADC_DONTCARE,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_9V_TO_QC_PREPARE,
	.next			= &afc_9v_to_qc_prepare,
};

muic_afc_data_t afc_9v_to_afc_err_v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC,
	.afc_name		= "AFC charger ERR V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
#if defined(CONFIG_MUIC_HV_12V)
	.gpstatus_vbadc		= VBADC_DONTCARE, // 12V should be error
#else
	.gpstatus_vbadc		= VBADC_AFC_ERR_V,
#endif
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_9V_TO_AFC_ERR_V,
	.next			= &afc_9v_to_qc_prepare,
};

muic_afc_data_t afc_9v_to_afc_5v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_5V_MUIC,
	.afc_name		= "AFC charger 5V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_5V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_9V_TO_AFC_5V,
	.next			= &afc_9v_to_afc_err_v,
};

muic_afc_data_t afc_9v_to_afc_9v_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC,
	.afc_name		= "AFC charger 9V (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_9V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_9V_TO_AFC_9V_DUPLI,
	.next			= &afc_9v_to_afc_5v,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC] */
muic_afc_data_t afc_9v_dupli_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.bccontrol2_dpdnman	= DPDNVDEN_DONTCARE,
	.gpstatus_vbadc		= VBADC_DONTCARE,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_9V_DUPLI_TO_QC_PREPARE,
	.next			= &afc_9v_dupli_to_qc_prepare,
};

muic_afc_data_t afc_9v_dupli_to_afc_err_v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC,
	.afc_name		= "AFC charger ERR V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
#if defined(CONFIG_MUIC_HV_12V)
	.gpstatus_vbadc		= VBADC_DONTCARE, // 12V should be error
#else
	.gpstatus_vbadc		= VBADC_AFC_ERR_V,
#endif
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_9V_DUPLI_TO_AFC_ERR_V,
	.next			= &afc_9v_dupli_to_qc_prepare,
};

muic_afc_data_t afc_9v_dupli_to_afc_5v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_5V_MUIC,
	.afc_name		= "AFC charger 5V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_5V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_9V_DUPLI_TO_AFC_5V,
	.next			= &afc_9v_dupli_to_afc_err_v,
};

muic_afc_data_t afc_9v_dupli_to_afc_9v_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC,
	.afc_name		= "AFC charger 9V (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_9V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_9V_DUPLI_TO_AFC_9V_DUPLI,
	.next			= &afc_9v_dupli_to_afc_5v,
};

#if defined(CONFIG_MUIC_HV_12V)
/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_12V_MUIC] */
muic_afc_data_t afc_12v_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.bccontrol2_dpdnman	= DPDNVDEN_DONTCARE,
	.gpstatus_vbadc		= VBADC_DONTCARE,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_12V_TO_QC_PREPARE,
	.next			= &afc_12v_to_qc_prepare,
};

muic_afc_data_t afc_12v_to_afc_9v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
	.afc_name		= "AFC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_9V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_12V_TO_AFC_9V,
	.next			= &afc_12v_to_qc_prepare,
};

muic_afc_data_t afc_12v_to_afc_5v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_5V_MUIC,
	.afc_name		= "AFC charger 5V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_5V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_12V_TO_AFC_5V,
	.next			= &afc_12v_to_afc_9v,
};

muic_afc_data_t afc_12v_to_afc_err_v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC,
	.afc_name		= "AFC charger ERR V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_ERR_V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_12V_TO_AFC_ERR_V,
	.next			= &afc_12v_to_afc_5v,
};

muic_afc_data_t afc_12v_to_afc_12v_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC,
	.afc_name		= "AFC charger 12V (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_12V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_12V_TO_AFC_12V_DUPLI,
	.next			= &afc_12v_to_afc_err_v,
};

/* afc_condition_checklist[ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC] */
muic_afc_data_t afc_12v_dupli_to_qc_prepare = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC,
	.afc_name		= "QC charger Prepare",
	.afc_irq		= MUIC_AFC_IRQ_MPNACK,
	.bccontrol2_dpdnman	= DPDNVDEN_DONTCARE,
	.gpstatus_vbadc		= VBADC_DONTCARE,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_12V_DUPLI_TO_QC_PREPARE,
	.next			= &afc_12v_dupli_to_qc_prepare,
};

muic_afc_data_t afc_12v_dupli_to_afc_12v_dupli = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC,
	.afc_name		= "AFC charger 12V (mrxrdy)",
	.afc_irq		= MUIC_AFC_IRQ_MRXRDY,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_12V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_12V_DUPLI_TO_AFC_12V_DUPLI,
	.next			= &afc_12v_dupli_to_qc_prepare,
};

muic_afc_data_t afc_12v_dupli_to_afc_9v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_9V_MUIC,
	.afc_name		= "AFC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_9V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_12V_DUPLI_TO_AFC_9V,
	.next			= &afc_12v_dupli_to_afc_12v_dupli,
};

muic_afc_data_t afc_12v_dupli_to_afc_5v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_5V_MUIC,
	.afc_name		= "AFC charger 5V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_5V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_12V_DUPLI_TO_AFC_5V,
	.next			= &afc_12v_dupli_to_afc_9v,
};

muic_afc_data_t afc_12v_dupli_to_afc_err_v = {
	.new_dev		= ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC,
	.afc_name		= "AFC charger ERR V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_DISABLE,
	.gpstatus_vbadc		= VBADC_AFC_ERR_V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_AFC_12V_DUPLI_TO_AFC_ERR_V,
	.next			= &afc_12v_dupli_to_afc_5v,
};
#endif

/* afc_condition_checklist[ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC] */
muic_afc_data_t qc_prepare_to_qc_9v = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_9V_MUIC,
	.afc_name		= "QC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_ENABLE,
	.gpstatus_vbadc		= VBADC_QC_9V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_QC_PREPARE_TO_QC_9V,
	.next			= &qc_prepare_to_qc_9v,
};

muic_afc_data_t qc_prepare_to_qc_5v = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_5V_MUIC,
	.afc_name		= "QC charger 5V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_ENABLE,
	.gpstatus_vbadc		= VBADC_QC_5V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_QC_PREPARE_TO_QC_5V,
	.next			= &qc_prepare_to_qc_9v,
};

/* afc_condition_checklist[ATTACHED_DEV_QC_CHARGER_5V_MUIC] */
muic_afc_data_t qc_5v_to_qc_9v = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_9V_MUIC,
	.afc_name		= "QC charger 9V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_ENABLE,
	.gpstatus_vbadc		= VBADC_QC_9V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_QC_5V_TO_QC_9V,
	.next			= &qc_5v_to_qc_9v,
};

/* afc_condition_checklist[ATTACHED_DEV_QC_CHARGER_9V_MUIC] */
muic_afc_data_t qc_9v_to_qc_5v = {
	.new_dev		= ATTACHED_DEV_QC_CHARGER_5V_MUIC,
	.afc_name		= "QC charger 5V",
	.afc_irq		= MUIC_AFC_IRQ_VBADC,
	.bccontrol2_dpdnman	= DPDNVDEN_ENABLE,
	.gpstatus_vbadc		= VBADC_QC_5V,
	.bcstatus2_vdnmon	= VDNMON_DONTCARE,
	.function_num		= FUNC_QC_9V_TO_QC_5V,
	.next			= &qc_9v_to_qc_5v,
};

muic_afc_data_t	*afc_condition_checklist[ATTACHED_DEV_NUM] = {
	[ATTACHED_DEV_TA_MUIC]				= &ta_to_prepare,
	[ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC]		= &prepare_to_prepare_dupli,
	[ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC]	= &prepare_dupli_to_afc_5v,
	[ATTACHED_DEV_AFC_CHARGER_5V_MUIC]		= &afc_5v_to_afc_5v_dupli,
	[ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC]	= &afc_5v_dupli_to_afc_err_v,
	[ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC]		= &afc_err_v_to_afc_err_v_dupli,
	[ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC]	= &afc_err_v_dupli_to_afc_err_v_dupli,
	[ATTACHED_DEV_AFC_CHARGER_9V_MUIC]		= &afc_9v_to_afc_9v_dupli,
	[ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC]	= &afc_9v_dupli_to_afc_9v_dupli,
#if defined(CONFIG_MUIC_HV_12V)
	[ATTACHED_DEV_AFC_CHARGER_12V_MUIC]		= &afc_12v_to_afc_12v_dupli,
	[ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC]	= &afc_12v_dupli_to_afc_err_v,
#endif
	[ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC]		= &qc_prepare_to_qc_9v,
	[ATTACHED_DEV_QC_CHARGER_5V_MUIC]		= &qc_5v_to_qc_9v,
	[ATTACHED_DEV_QC_CHARGER_9V_MUIC]		= &qc_9v_to_qc_5v,
};

struct afc_init_data_s {
	struct work_struct muic_afc_init_work;
	struct hv_data *phv;
};
struct afc_init_data_s afc_init_data;

static bool muic_check_is_hv_dev(struct hv_data *phv)
{
	bool ret = false;

	switch (phv->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_12V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC:
		ret = true;
		break;
	default:
		ret = false;
		break;
	}

	if (debug_en_checklist)
		pr_info("%s:%s attached_dev(%d)[%c]\n", MUIC_HV_DEV_NAME,
			__func__, phv->attached_dev, (ret ? 'T' : 'F'));

	return ret;
}

muic_attached_dev_t hv_muic_check_id_err
	(struct hv_data *phv, muic_attached_dev_t new_dev)
{
	muic_attached_dev_t after_new_dev = new_dev;

	if (!muic_check_is_hv_dev(phv))
		goto out;

	switch(new_dev) {
	case ATTACHED_DEV_TA_MUIC:
		pr_info("%s:%s cannot change HV(%d)->TA(%d)!\n", MUIC_DEV_NAME,
			__func__, phv->attached_dev, new_dev);
		after_new_dev = phv->attached_dev;
		break;
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
		pr_info("%s:%s HV ID Err - Undefined\n", MUIC_DEV_NAME, __func__);
		after_new_dev = ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC;
		break;
	case ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC:
		pr_info("%s:%s HV ID Err - Unsupported\n", MUIC_DEV_NAME, __func__);
		after_new_dev = ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC;
		break;
	default:
		pr_info("%s:%s HV ID Err - Supported\n", MUIC_DEV_NAME, __func__);
		after_new_dev = ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC;
		break;
	}
out:
	return after_new_dev;
}

/*
static int max77865_hv_muic_read_reg(struct i2c_client *i2c, u8 reg, u8 *value)
{

	value = (u8 *)muic_i2c_read_byte(i2c, reg);
	if (value < 0)
		pr_err("%s:%s err read REG(0x%02x) [%d]\n", MUIC_HV_DEV_NAME,
				__func__, reg, *value);
	return *value;
}
*/
static int max77865_hv_muic_write_reg(struct i2c_client *i2c, u8 reg, u8 value)
{
	u8 before_val, after_val;
	int ret;

	before_val = muic_i2c_read_byte(i2c, reg);
	ret = muic_i2c_write_byte(i2c, reg, value);
	after_val = muic_i2c_read_byte(i2c, reg);

	pr_info("%s:%s reg[0x%02x] = [0x%02x] + [0x%02x] -> [0x%02x]\n",
		MUIC_HV_DEV_NAME, __func__, reg, before_val, value, after_val);
	return ret;
}

int max77865_muic_hv_update_reg(struct i2c_client *i2c,
	const u8 reg, const u8 val, const u8 mask, const bool debug_en)
{
	u8 before_val, new_val, after_val =0;
	int ret = 0;

	before_val = muic_i2c_read_byte(i2c, reg);
	if (before_val < 0)
		pr_err("%s:%s err read REG(0x%02x) [%d] \n", MUIC_DEV_NAME,
				__func__, reg, ret);

	new_val = (val & mask) | (before_val & (~mask));

	if (before_val ^ new_val) {
		ret = max77865_hv_muic_write_reg(i2c, reg, new_val);
		if (ret)
			pr_err("%s:%s err write REG(0x%02x) [%d]\n",
					MUIC_DEV_NAME, __func__, reg, ret);
	} else if (debug_en) {
		pr_info("%s:%s REG(0x%02x): already [0x%02x], don't write reg\n",
				MUIC_DEV_NAME, __func__, reg, before_val);
		goto out;
	}

	if (debug_en) {
		after_val = muic_i2c_read_byte(i2c, reg);
		if (after_val < 0)
			pr_err("%s:%s err read REG(0x%02x) [%d]\n",
					MUIC_DEV_NAME, __func__, reg, ret);

		pr_info("%s:%s REG(0x%02x): [0x%02x]+[0x%02x:0x%02x]=[0x%02x]\n",
				MUIC_DEV_NAME, __func__, reg, before_val,
				val, mask, after_val);
	}

out:
	return after_val;
}

void max77865_hv_muic_reset_hvcontrol_reg(struct hv_data *phv)
{
	struct i2c_client *i2c = phv->i2c;

	max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_CONTROL2_BC, 0x00);
	max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVCONTROL1, 0x00);
	max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVCONTROL2, 0x00);
}

static void max77865_muic_set_afc_ready(struct hv_data *phv, bool value)
{
	bool before, after;

	before = phv->is_afc_muic_ready;
	phv->is_afc_muic_ready = value;
	after = phv->is_afc_muic_ready;

	pr_info("%s:%s afc_muic_ready[%d->%d]\n", MUIC_DEV_NAME, __func__, before, after);
}

static int max77865_hv_muic_state_maintain(struct hv_data *phv)
{
	int ret = 0;

	pr_info("%s:%s \n", MUIC_HV_DEV_NAME, __func__);

	if (phv->attached_dev == ATTACHED_DEV_NONE_MUIC) {
		pr_info("%s:%s Detached(%d), need to check after\n", MUIC_HV_DEV_NAME,
				__func__, phv->attached_dev);
		return ret;
	}

	return ret;
}

static void max77865_hv_muic_set_afc_after_prepare
					(struct hv_data *phv)
{
	struct i2c_client *i2c = phv->i2c;
	u8 value;

	pr_info("%s:%s HV charger is detected\n", MUIC_HV_DEV_NAME, __func__);

	/* Set HVCONTROL2 = 0x02 */
	max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVCONTROL2, HVCONTROL2_DP06EN_MASK);

	/* Set HVCONTROL1 - Enable VbusADCEn / Disable DPVD, DPDNVDEN */
	max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVCONTROL1, HVCONTROL1_VBUSADCEN_MASK);
	// CIS - Need check, CL54: HI-Z -> CL65: OPEN??
	max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_CONTROL2_BC, 0x0F);

	/* Set TX DATA */
	value = phv->tx_data;

	max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVTXBYTE, value);

	/* Set HVCONTROL2 = Enable MTxEn, MPing, DP06En, HVDigEn */
	max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVCONTROL2, 0x1B);
}

static void max77865_hv_muic_set_afc_charger_handshaking
			(struct hv_data *phv)
{
	struct i2c_client *i2c = phv->i2c;
	u8 hvtxbyte=0;
	u8 hvrxbyte[HVRXBYTE_MAX];
	u8 selecthvtxbyte=0;
	int i, ret;
	int j;
	u8 hvrxbyte_str[HVRXBYTE_MAX * 4];
	u8 temp_buf[8];

	pr_info("%s:%s \n", MUIC_HV_DEV_NAME, __func__);

	memset(hvrxbyte, 0x00, sizeof(hvrxbyte));
	memset(hvrxbyte_str, 0x00, sizeof(hvrxbyte_str));	
	memset(temp_buf, 0x00, sizeof(temp_buf));

	ret = max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVCONTROL2, 0x13);
	if (ret)
		pr_err("%s:%s cannot write hvcontrol2(%d)\n", MUIC_HV_DEV_NAME, __func__, ret);

	hvtxbyte = muic_i2c_read_byte(i2c, MAX77865_MUIC_REG_HVTXBYTE);

	for(i = 0; i < HVRXBYTE_MAX; i++) {
		hvrxbyte[i] = muic_i2c_read_byte(i2c, (MAX77865_MUIC_REG_HVRXBYTE1+i));
		if(hvrxbyte[i] == 0x47)
			hvrxbyte[i] = 0x46;
		if(hvrxbyte[i] == 0)
			break;
	}

	pr_info("%s:%s HVTXBYTE: %02x\n", MUIC_HV_DEV_NAME, __func__, hvtxbyte);

	pr_info("%s:%s HVRXBYTE\n", MUIC_HV_DEV_NAME, __func__);
	for (j=0; j<HVRXBYTE_MAX; j++) {
		sprintf(temp_buf, " %02x", hvrxbyte[j]);
		strcat(hvrxbyte_str, temp_buf);
	}		
	
	pr_info(" => %s\n", hvrxbyte_str);

#if defined(CONFIG_MUIC_HV_12V)
	if(hvrxbyte[0] != hvtxbyte) {
		for(i = 0; (i < HVRXBYTE_MAX) && (hvrxbyte[i] != 0); i++) {
			if(hvtxbyte > selecthvtxbyte) {
				pr_info(" selected hvtxbyte = %02x at %d", hvrxbyte[i], i);
				selecthvtxbyte = hvrxbyte[i];
			}
		}
		/* W/A of RX byte error */
		if((phv->vps.hvcontrol[1] & 0x8) == 0) {
			switch (selecthvtxbyte) {
				case MUIC_HV_5V:
				case MUIC_HV_9V:
				case MUIC_HV_12V:
					break;
				default:
					selecthvtxbyte = MUIC_HV_9V;
					pr_info("%s:%s RXBYTE Error! selected hvtxbyte = %02x\n",
							MUIC_HV_DEV_NAME, __func__, selecthvtxbyte);
					break;
			}
		}
		if(selecthvtxbyte != 0)
			max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVTXBYTE, selecthvtxbyte);
	}
#else
	if(hvrxbyte[0] != hvtxbyte) {
		for(i = 0; (i < HVRXBYTE_MAX) && (hvrxbyte[i] != 0); i++) {
			if(((hvrxbyte[i] & 0xF0) == 0x40) && (hvtxbyte > selecthvtxbyte)) {
				pr_info(" selected hvtxbyte = %02x at %d", hvrxbyte[i], i);
				selecthvtxbyte = hvrxbyte[i];
			}
		}
		if(selecthvtxbyte != 0)
			max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVTXBYTE, selecthvtxbyte);
	}
#endif

	return;
}

static void max77865_hv_muic_afc_control_ping
		(struct hv_data *phv, bool ping_continue)
{
	int ret;

	pr_info("%s:%s control ping[%d, %c]\n", MUIC_HV_DEV_NAME, __func__,
				phv->afc_count, ping_continue ? 'T' : 'F');

	if (ping_continue)
		ret = max77865_hv_muic_write_reg(phv->i2c, MAX77865_MUIC_REG_HVCONTROL2, 0x5B);
	else
		ret = max77865_hv_muic_write_reg(phv->i2c, MAX77865_MUIC_REG_HVCONTROL2, 0x03);

	if (ret) {
		pr_err("%s:%s cannot writing HVCONTROL2 reg(%d)\n",
				MUIC_HV_DEV_NAME, __func__, ret);
	}
}

static void max77865_hv_muic_qc_charger(struct hv_data *phv)
{
	struct i2c_client	*i2c = phv->i2c;
	int ret1 = 0, ret2 = 0;
	u8 bcstatus2 = 0, gpstatus = 0;

	bcstatus2 = muic_i2c_read_byte(i2c, MAX77865_MUIC_REG_STATUS2_BC);
	gpstatus = muic_i2c_read_byte(i2c, MAX77865_MUIC_REG_STATUS_GP);
	if ((bcstatus2 < 0) || (gpstatus < 0)) {
		pr_err("%s:%s cannot read STATUS reg\n", MUIC_HV_DEV_NAME, __func__);
	}

	pr_info("%s:%s BC_STATUS2:0x%02x GP_STATUS:0x%02x qc_hv:%x\n",
				MUIC_HV_DEV_NAME, __func__, bcstatus2, gpstatus, phv->qc_hv);

	switch (phv->qc_hv) {
	case HV_SUPPORT_QC_9V:
		ret1 = max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVCONTROL1, 0x01);
		ret2 = max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_CONTROL2_BC, 0x19);
		break;
	case HV_SUPPORT_QC_12V:
		ret1 = max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVCONTROL1, 0x01);
		ret2 = max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_CONTROL2_BC, 0x15);
		break;
	case HV_SUPPORT_QC_20V:
		ret1 = max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVCONTROL1, 0x01);
		ret2 = max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_CONTROL2_BC, 0x1A);
		break;
	case HV_SUPPORT_QC_5V:
		ret1 = max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVCONTROL1, 0x01);
		ret2 = max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_CONTROL2_BC, 0x14);
		break;
	default:
		pr_err("%s:%s not support QC Charger\n", MUIC_HV_DEV_NAME, __func__);
		break;
	}

	ret1 += ret2;
	if (ret1) {
		pr_err("%s:%s cannot writing HVCONTROL1 or BCCONTROL2 reg(%d)\n",
							MUIC_HV_DEV_NAME, __func__, ret1);
	}
}

static void max77865_hv_muic_after_qc_prepare(struct hv_data *phv)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);
	phv->is_qc_vb_settle = false;

	schedule_delayed_work(&phv->hv_muic_qc_vb_work, msecs_to_jiffies(300));
}

static void max77865_hv_muic_adcmode_switch
		(struct hv_data *phv, bool always_on)
{
	struct i2c_client	*i2c = phv->i2c;
	int ret;

	pr_info("%s:%s always_on:%c\n", MUIC_HV_DEV_NAME, __func__, (always_on ? 'T' : 'F'));

	if (always_on) {
//		set_adc_scan_mode(phv->pmuic,ADC_SCANMODE_CONTINUOUS);
		ret = max77865_muic_hv_update_reg(i2c, MAX77865_MUIC_REG_HVCONTROL1,
					(MAX77865_ENABLE_BIT << HVCONTROL1_VBUSADCEN_SHIFT),
					HVCONTROL1_VBUSADCEN_MASK, true);
	} else {
//		set_adc_scan_mode(phv->pmuic,ADC_SCANMODE_ONESHOT);
		/* non MAXIM */
		ret = max77865_muic_hv_update_reg(i2c, MAX77865_MUIC_REG_HVCONTROL1,
					(MAX77865_DISABLE_BIT << HVCONTROL1_VBUSADCEN_SHIFT),
					HVCONTROL1_VBUSADCEN_MASK, true);
	}

	if (ret < 0)
		pr_err("%s:%s cannot switch adcmode(%d)\n", MUIC_HV_DEV_NAME, __func__, ret);
}

static void max77865_hv_muic_adcmode_always_on(struct hv_data *phv)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);
	max77865_hv_muic_adcmode_switch(phv, true);
}

void max77865_hv_muic_adcmode_oneshot(struct hv_data *phv)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);
	max77865_hv_muic_adcmode_switch(phv, false);
}

static int max77865_hv_muic_handle_attach
		(struct hv_data *phv, const muic_afc_data_t *new_afc_data)
{
	int ret = 0;
	bool noti = true;
	muic_attached_dev_t	new_dev	= new_afc_data->new_dev;
	int mping_missed = (phv->vps.hvcontrol[1] & 0x8);
	if (mping_missed)
		phv->afc_count = 0;

	pr_info("%s:%s \n", MUIC_HV_DEV_NAME, __func__);

	if (phv->is_charger_ready == false) {
		if (new_afc_data->new_dev == ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC) {
			phv->is_afc_muic_prepare = true;
			pr_info("%s:%s is_charger_ready[%c], is_afc_muic_prepare[%c]\n",
				MUIC_HV_DEV_NAME, __func__,
				(phv->is_charger_ready ? 'T' : 'F'),
				(phv->is_afc_muic_prepare ? 'T' : 'F'));

			return ret;
		}
		pr_info("%s:%s is_charger_ready[%c], just return\n", MUIC_HV_DEV_NAME,
			__func__, (phv->is_charger_ready ? 'T' : 'F'));
		return ret;
	}

	switch (new_afc_data->function_num) {
	case FUNC_TA_TO_PREPARE:
		max77865_hv_muic_adcmode_always_on(phv);
		max77865_hv_muic_set_afc_after_prepare(phv);
		phv->afc_count = 0;
		phv->is_afc_handshaking = false;
		/* HW Issue(MPing miss)
		 * check HV state values after 2000ms(2s) */
		schedule_delayed_work(&phv->hv_muic_mping_miss_wa,
				msecs_to_jiffies(MPING_MISS_WA_TIME));
		break;
	case FUNC_PREPARE_TO_PREPARE_DUPLI:
		/* attached_dev is changed. MPING Missing did not happened
		 * Cancel delayed work */
		pr_info("%s:%s cancel_delayed_work(dev %d), Mping missing wa\n",
			MUIC_HV_DEV_NAME, __func__, new_dev);
		cancel_delayed_work(&phv->hv_muic_mping_miss_wa);
		phv->afc_count++;
		max77865_hv_muic_set_afc_charger_handshaking(phv);
		if (!mping_missed)
			phv->is_afc_handshaking = true;
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_PREPARE_TO_AFC_5V:
		noti = false;
		break;
	case FUNC_PREPARE_TO_QC_PREPARE:
		/* attached_dev is changed. MPING Missing did not happened
		 * Cancel delayed work */
		pr_info("%s:%s cancel_delayed_work(dev %d), Mping missing wa\n",
			MUIC_HV_DEV_NAME, __func__, new_dev);
		cancel_delayed_work(&phv->hv_muic_mping_miss_wa);
		/* ping STOP */
		ret = max77865_hv_muic_write_reg(phv->i2c, MAX77865_MUIC_REG_HVCONTROL2, 0x03);
		if (ret) {
			pr_err("%s:%s cannot writing HVCONTROL2 reg(%d)\n",
					MUIC_HV_DEV_NAME, __func__, ret);
		}
		max77865_hv_muic_qc_charger(phv);
		max77865_hv_muic_after_qc_prepare(phv);
		break;
	case FUNC_PREPARE_DUPLI_TO_PREPARE_DUPLI:
		phv->afc_count++;
		if (!phv->is_afc_handshaking) {
			max77865_hv_muic_set_afc_charger_handshaking(phv);
			if (!mping_missed)
				phv->is_afc_handshaking = true;
		}
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_PREPARE_DUPLI_TO_AFC_5V:
		if (!phv->is_afc_handshaking) {
			max77865_hv_muic_set_afc_charger_handshaking(phv);
			if (!mping_missed)
				phv->is_afc_handshaking = true;
		}
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_PREPARE_DUPLI_TO_AFC_ERR_V:
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_PREPARE_DUPLI_TO_AFC_9V:
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
#if defined(CONFIG_MUIC_HV_12V)
	case FUNC_PREPARE_DUPLI_TO_AFC_12V:
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
#endif
	case FUNC_PREPARE_DUPLI_TO_QC_PREPARE:
		max77865_hv_muic_qc_charger(phv);
		max77865_hv_muic_after_qc_prepare(phv);
		break;
	case FUNC_AFC_5V_TO_AFC_5V_DUPLI:
		/* attached_dev is changed. MPING Missing did not happened
		 * Cancel delayed work */
		pr_info("%s:%s cancel_delayed_work(dev %d), Mping missing wa\n",
			MUIC_HV_DEV_NAME, __func__, new_dev);
		cancel_delayed_work(&phv->hv_muic_mping_miss_wa);
		phv->afc_count++;
		if (!phv->is_afc_handshaking) {
			max77865_hv_muic_set_afc_charger_handshaking(phv);
			if (!mping_missed)
				phv->is_afc_handshaking = true;
		}
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
			max77865_hv_muic_adcmode_always_on(phv);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_AFC_5V_TO_AFC_ERR_V:
		/* attached_dev is changed. MPING Missing did not happened
		 * Cancel delayed work */
		pr_info("%s:%s cancel_delayed_work(dev %d), Mping missing wa\n",
			MUIC_HV_DEV_NAME, __func__, new_dev);
		cancel_delayed_work(&phv->hv_muic_mping_miss_wa);
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_AFC_5V_TO_AFC_9V:
		/* attached_dev is changed. MPING Missing did not happened
		 * Cancel delayed work */
		pr_info("%s:%s cancel_delayed_work(dev %d), Mping missing wa\n",
			MUIC_HV_DEV_NAME, __func__, new_dev);
		cancel_delayed_work(&phv->hv_muic_mping_miss_wa);
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
#if defined(CONFIG_MUIC_HV_12V)
	case FUNC_AFC_5V_TO_AFC_12V:
		/* attached_dev is changed. MPING Missing did not happened
		 * Cancel delayed work */
		pr_info("%s:%s cancel_delayed_work(dev %d), Mping missing wa\n",
			MUIC_HV_DEV_NAME, __func__, new_dev);
		cancel_delayed_work(&phv->hv_muic_mping_miss_wa);
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
#endif
	case FUNC_AFC_5V_TO_QC_PREPARE:
		/* attached_dev is changed. MPING Missing did not happened
		 * Cancel delayed work */
		pr_info("%s:%s cancel_delayed_work(dev %d), Mping missing wa\n",
			MUIC_HV_DEV_NAME, __func__, new_dev);
		cancel_delayed_work(&phv->hv_muic_mping_miss_wa);
		max77865_hv_muic_qc_charger(phv);
		max77865_hv_muic_after_qc_prepare(phv);
		break;
	case FUNC_AFC_5V_DUPLI_TO_AFC_5V_DUPLI:
		phv->afc_count++;
		if (!phv->is_afc_handshaking) {
			max77865_hv_muic_set_afc_charger_handshaking(phv);
			if (!mping_missed)
				phv->is_afc_handshaking = true;
		}
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
			max77865_hv_muic_adcmode_always_on(phv);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_AFC_5V_DUPLI_TO_AFC_ERR_V:
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
#if defined(CONFIG_MUIC_HV_12V)
	case FUNC_AFC_5V_DUPLI_TO_AFC_12V:
#endif		
	case FUNC_AFC_5V_DUPLI_TO_AFC_9V:
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
	case FUNC_AFC_5V_DUPLI_TO_QC_PREPARE:
		max77865_hv_muic_qc_charger(phv);
		max77865_hv_muic_after_qc_prepare(phv);
		break;
	case FUNC_AFC_ERR_V_TO_AFC_ERR_V_DUPLI:
		phv->afc_count++;
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
			max77865_hv_muic_adcmode_always_on(phv);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_AFC_ERR_V_TO_AFC_5V:
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
	case FUNC_AFC_ERR_V_TO_AFC_9V:
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
#if defined(CONFIG_MUIC_HV_12V)
	case FUNC_AFC_ERR_V_TO_AFC_12V:
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
#endif
	case FUNC_AFC_ERR_V_TO_QC_PREPARE:
		max77865_hv_muic_qc_charger(phv);
		max77865_hv_muic_after_qc_prepare(phv);
		break;
	case FUNC_AFC_ERR_V_DUPLI_TO_AFC_ERR_V_DUPLI:
		phv->afc_count++;
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
			max77865_hv_muic_adcmode_always_on(phv);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_AFC_ERR_V_DUPLI_TO_AFC_5V:
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
	case FUNC_AFC_ERR_V_DUPLI_TO_AFC_9V:
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
#if defined(CONFIG_MUIC_HV_12V)
	case FUNC_AFC_ERR_V_DUPLI_TO_AFC_12V:
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
#endif
	case FUNC_AFC_ERR_V_DUPLI_TO_QC_PREPARE:
		max77865_hv_muic_qc_charger(phv);
		max77865_hv_muic_after_qc_prepare(phv);
		break;
	case FUNC_AFC_9V_TO_AFC_9V_DUPLI:
		phv->afc_count++;
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
			max77865_hv_muic_adcmode_always_on(phv);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_AFC_9V_TO_AFC_ERR_V:
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_AFC_9V_TO_AFC_5V:
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
	case FUNC_AFC_9V_TO_QC_PREPARE:
		max77865_hv_muic_qc_charger(phv);
		max77865_hv_muic_after_qc_prepare(phv);
		break;
	case FUNC_AFC_9V_DUPLI_TO_AFC_ERR_V:
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_AFC_9V_DUPLI_TO_AFC_5V:
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
	case FUNC_AFC_9V_DUPLI_TO_AFC_9V_DUPLI:
		phv->afc_count++;
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
			max77865_hv_muic_adcmode_always_on(phv);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_AFC_9V_DUPLI_TO_QC_PREPARE:
		max77865_hv_muic_qc_charger(phv);
		max77865_hv_muic_after_qc_prepare(phv);
		break;
#if defined(CONFIG_MUIC_HV_12V)
	case FUNC_AFC_12V_TO_AFC_12V_DUPLI:
		phv->afc_count++;
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
			max77865_hv_muic_adcmode_always_on(phv);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_AFC_12V_TO_AFC_ERR_V:
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_AFC_12V_TO_AFC_5V:
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
	case FUNC_AFC_12V_TO_AFC_9V:
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
	case FUNC_AFC_12V_TO_QC_PREPARE:
		max77865_hv_muic_qc_charger(phv);
		max77865_hv_muic_after_qc_prepare(phv);
		break;
	case FUNC_AFC_12V_DUPLI_TO_AFC_ERR_V:
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_AFC_12V_DUPLI_TO_AFC_5V:
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
	case FUNC_AFC_12V_DUPLI_TO_AFC_9V:
		max77865_hv_muic_afc_control_ping(phv, false);
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
	case FUNC_AFC_12V_DUPLI_TO_AFC_12V_DUPLI:
		phv->afc_count++;
		if (phv->afc_count > AFC_CHARGER_WA_PING) {
			max77865_hv_muic_afc_control_ping(phv, false);
			max77865_hv_muic_adcmode_always_on(phv);
		} else {
			max77865_hv_muic_afc_control_ping(phv, true);
			noti = false;
		}
		break;
	case FUNC_AFC_12V_DUPLI_TO_QC_PREPARE:
		max77865_hv_muic_qc_charger(phv);
		max77865_hv_muic_after_qc_prepare(phv);
		break;
#endif
	case FUNC_QC_PREPARE_TO_QC_5V:
		if (phv->is_qc_vb_settle == true)
			max77865_hv_muic_adcmode_oneshot(phv);
		else
			noti = false;
		break;
	case FUNC_QC_PREPARE_TO_QC_9V:
		phv->is_qc_vb_settle = true;
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
	case FUNC_QC_5V_TO_QC_9V:
		phv->is_qc_vb_settle = true;
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
	case FUNC_QC_9V_TO_QC_5V:
		max77865_hv_muic_adcmode_oneshot(phv);
		break;
	default:
		pr_warn("%s:%s undefinded hv function num(%d)\n", MUIC_HV_DEV_NAME,
					__func__, new_afc_data->function_num);
		ret = -ESRCH;
		goto out;
	}

#if defined(CONFIG_MUIC_NOTIFIER)
	if (phv->attached_dev == new_dev)
		noti = false;
	else if (new_dev == ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC)
		noti = false;

	if (noti)
		muic_notifier_attach_attached_dev(new_dev);
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_MUIC_SUPPORT_CCIC)
	if (phv->pmuic->pdata->opmode & OPMODE_CCIC)
		muic_set_legacy_dev(phv->pmuic, new_dev);		
#endif
	phv->attached_dev = new_dev;

	//Fixme.
	/* update MUIC's attached_dev */
	phv->pmuic->attached_dev = phv->attached_dev;
out:
	return ret;
}

static bool muic_check_hv_irq
			(struct hv_data *phv,
			const muic_afc_data_t *tmp_afc_data, int irq)
{
	int afc_irq = 0;
	bool ret = false;

	/* change irq num to muic_afc_irq_t */
	if(irq == phv->irq_vdnmon)
		afc_irq = MUIC_AFC_IRQ_VDNMON;
	else if (irq == phv->irq_mrxrdy)
		afc_irq = MUIC_AFC_IRQ_MRXRDY;
	else if (irq == phv->irq_mpnack)
		afc_irq = MUIC_AFC_IRQ_MPNACK;
	else if (irq == phv->irq_vbadc)
		afc_irq = MUIC_AFC_IRQ_VBADC;
	else {
		pr_err("%s:%s cannot find irq #(%d)\n", MUIC_HV_DEV_NAME, __func__, irq);
		ret = false;
		goto out;
	}

	if (tmp_afc_data->afc_irq == afc_irq) {
		ret = true;
		goto out;
	}

	if (tmp_afc_data->afc_irq == MUIC_AFC_IRQ_DONTCARE) {
		ret = true;
		goto out;
	}

out:
	if (debug_en_checklist) {
		pr_info("%s:%s check_data dev(%d) irq(%d:%d) ret(%c)\n",
				MUIC_HV_DEV_NAME, __func__, tmp_afc_data->new_dev,
				tmp_afc_data->afc_irq, afc_irq, ret ? 'T' : 'F');
	}

	return ret;
}

static bool muic_check_bccontrol2_dpdnman
			(const muic_afc_data_t *tmp_afc_data, u8 dpdnvden)
{
	bool ret = false;

	if (tmp_afc_data->bccontrol2_dpdnman == dpdnvden) {
		ret = true;
		goto out;
	}

	if (tmp_afc_data->bccontrol2_dpdnman == DPDNVDEN_DONTCARE) {
		ret = true;
		goto out;
	}

out:
	if (debug_en_checklist) {
		pr_info("%s:%s check_data dev(%d) dpdnvden(0x%x:0x%x) ret(%c)\n",
				MUIC_HV_DEV_NAME, __func__, tmp_afc_data->new_dev,
				tmp_afc_data->bccontrol2_dpdnman, dpdnvden,
				ret ? 'T' : 'F');
	}

	return ret;
}

static bool muic_check_gpstatus_vbadc
			(const muic_afc_data_t *tmp_afc_data, u8 vbadc)
{
	bool ret = false;

	if (tmp_afc_data->gpstatus_vbadc == vbadc) {
		ret = true;
		goto out;
	}

	if (tmp_afc_data->gpstatus_vbadc == VBADC_AFC_5V) {
		switch (vbadc) {
		case VBADC_4V_5V:
		case VBADC_5V_6V:
		case VBADC_6V_7V:
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_afc_data->gpstatus_vbadc == VBADC_AFC_9V) {
		switch (vbadc) {
		case VBADC_7V_8V:
		case VBADC_8V_9V:
		case VBADC_9V_10V:
			ret = true;
			goto out;
		default:
			break;
		}
	}

#if defined(CONFIG_MUIC_HV_12V)
	if (tmp_afc_data->gpstatus_vbadc == VBADC_AFC_12V) {
		switch (vbadc) {
		case VBADC_10V_11V:
		case VBADC_11V_12V:
		case VBADC_12V_13V:
		case VBADC_13V:
			ret = true;
			goto out;
		default:
			break;
		}
	}
#endif

	if (tmp_afc_data->gpstatus_vbadc == VBADC_AFC_ERR_V_NOT_0) {
		switch (vbadc) {
		case VBADC_6V_7V:
		case VBADC_7V_8V:
#if !defined(CONFIG_MUIC_HV_12V)
		case VBADC_10V_11V:
		case VBADC_11V_12V:
		case VBADC_12V_13V:
		case VBADC_13V:
#endif
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_afc_data->gpstatus_vbadc == VBADC_AFC_ERR_V) {
		switch (vbadc) {
		case VBADC_VBDET:
		case VBADC_6V_7V:
		case VBADC_7V_8V:
#if !defined(CONFIG_MUIC_HV_12V)
		case VBADC_10V_11V:
		case VBADC_11V_12V:
		case VBADC_12V_13V:
		case VBADC_13V:
#endif
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_afc_data->gpstatus_vbadc == VBADC_QC_5V) {
		switch (vbadc) {
		case VBADC_4V_5V:
		case VBADC_5V_6V:
		case VBADC_6V_7V:
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_afc_data->gpstatus_vbadc == VBADC_QC_9V) {
		switch (vbadc) {
		case VBADC_6V_7V:
		case VBADC_7V_8V:
		case VBADC_8V_9V:
		case VBADC_9V_10V:
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_afc_data->gpstatus_vbadc == VBADC_ANY) {
		switch (vbadc) {
		case VBADC_4V_5V:
		case VBADC_5V_6V:
		case VBADC_6V_7V:
		case VBADC_7V_8V:
		case VBADC_8V_9V:
		case VBADC_9V_10V:
#if !defined(CONFIG_MUIC_HV_12V)
		case VBADC_10V_11V:
		case VBADC_11V_12V:
		case VBADC_12V_13V:
		case VBADC_13V:
#endif
			ret = true;
			goto out;
		default:
			break;
		}
	}

	if (tmp_afc_data->gpstatus_vbadc == VBADC_DONTCARE) {
		ret = true;
		goto out;
	}

out:
	if (debug_en_checklist) {
		pr_info("%s:%s check_data dev(%d) vbadc(0x%x:0x%x) ret(%c)\n",
				MUIC_HV_DEV_NAME, __func__, tmp_afc_data->new_dev,
				tmp_afc_data->gpstatus_vbadc, vbadc, ret ? 'T' : 'F');
	}

	return ret;
}

static bool muic_check_bcstatus2_vdnmon
			(const muic_afc_data_t *tmp_afc_data, u8 vdnmon)
{
	bool ret = false;

	if (tmp_afc_data->bcstatus2_vdnmon == vdnmon) {
		ret = true;
		goto out;
	}

	if (tmp_afc_data->bcstatus2_vdnmon == VDNMON_DONTCARE) {
		ret = true;
		goto out;
	}

out:
	if (debug_en_checklist) {
		pr_info("%s:%s check_data dev(%d) vdnmon(0x%x:0x%x) ret(%c)\n",
				MUIC_HV_DEV_NAME, __func__, tmp_afc_data->new_dev,
				tmp_afc_data->bcstatus2_vdnmon, vdnmon, ret ? 'T' : 'F');
	}

	return ret;
}

#if 0
/*
 * Keep charging for the non-AFC chargers
 * instead of sending a detach noti.
 */
static bool muic_hv_is_nonafc_ta(u8 chgtyp)
{
	switch (chgtyp) {
	case CHGTYP_500MA:
	case CHGTYP_1A:
	case CHGTYP_SPECIAL_3_3V_CHARGER:
		return true;
	default:
		break;
	}

	return false;
}
#endif

static bool muic_check_dev_ta(struct hv_data *phv)
{
	u8 status1 = phv->vps.status1;
	u8 status3 = phv->vps.status3;
	u8 uid, vbvolt, chgdetrun, chgtyp;

	uid = status3 & GP_STATUS_UID_MASK;
	vbvolt = status1 & BC_STATUS1_VBVOLT_MASK;
	chgdetrun = status1 & BC_STATUS1_CHGDETRUN_MASK;
	chgtyp = status1 & BC_STATUS1_CHGTYP_MASK;

	if (uid != UID_OPEN) {
		max77865_muic_set_afc_ready(phv, false);
		return false;
	}

#if 0 // CIS - Need check
	if (muic_hv_is_nonafc_ta(chgtyp)) {
		max77865_muic_set_afc_ready(phv, false);

		pr_info("%s:%s non AFC Charger.(chgtyp=%d) \n",
			MUIC_HV_DEV_NAME, __func__, chgtyp);
		return false;
	}
#endif

	if (vbvolt == VB_LOW || chgdetrun == CHGDETRUN_TRUE || chgtyp != CHGTYP_DEDICATED_CHARGER) {
		max77865_muic_set_afc_ready(phv, false);
#if defined(CONFIG_MUIC_NOTIFIER)
		muic_notifier_detach_attached_dev(phv->attached_dev);
#endif
		phv->attached_dev = ATTACHED_DEV_NONE_MUIC;
#if defined(CONFIG_MUIC_SUPPORT_CCIC)
		if (phv->pmuic->pdata->opmode & OPMODE_CCIC)
			muic_set_legacy_dev(phv->pmuic, phv->attached_dev);		
#endif
		//Fixme.
		/* update MUIC's attached_dev */
		phv->pmuic->attached_dev = phv->attached_dev;
		return false;
	}

	return true;
}

void max77865_hv_muic_detect_dev(struct hv_data *phv, int irq)
{
	struct i2c_client *i2c = phv->i2c;
	const muic_afc_data_t *tmp_afc_data = afc_condition_checklist[phv->attached_dev];

	int intr = MUIC_INTR_DETACH;
	int ret;
	int i;
	u8 status[3];
	u8 hvcontrol[2];
	u8 vdnmon, dpdnvden, vbadc;//mpnack
	bool flag_next = true;
	bool muic_dev_ta = false;

	pr_info("%s:%s irq(%d), attache_dev(%d)\n", MUIC_HV_DEV_NAME, __func__, irq, phv->attached_dev);

	if (tmp_afc_data == NULL) {
		pr_info("%s:%s non AFC Charger, just return!\n", MUIC_HV_DEV_NAME, __func__);
		return;
	}

	ret = max77865_bulk_read(phv->i2c, MAX77865_MUIC_REG_STATUS1_BC, 3, status);
	if (ret) {
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_DEV_NAME, __func__, ret);
		return;
	}

	pr_info("%s:%s BC STATUS1:0x%02x, 2:0x%02x, GP STATUS:0x%02x\n", MUIC_DEV_NAME, __func__,
					status[0], status[1], status[2]);

	/* attached status */
	phv->vps.status1 = status[0];
	phv->vps.status2 = status[1];
	phv->vps.status3 = status[2];

	/* check TA type */
	muic_dev_ta = muic_check_dev_ta(phv);
	if (!muic_dev_ta) {
		pr_err("%s:%s device type is not TA!\n", MUIC_HV_DEV_NAME, __func__);
		return;
	}

	/* attached status */
//	mpnack = status[2] & STATUS3_MPNACK_MASK;
	vdnmon = status[1] & BC_STATUS2_DNVDATREF_MASK;
	vbadc = status[2] & GP_STATUS_VBADC_MASK;

	phv->vps.bccontrol2 = muic_i2c_read_byte(i2c, MAX77865_MUIC_REG_CONTROL2_BC);
	ret = max77865_bulk_read(i2c, MAX77865_MUIC_REG_HVCONTROL1, 2, hvcontrol);
	if (ret) {
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_HV_DEV_NAME,
				__func__, ret);
		return;
	}

	pr_info("%s:%s BCCONTROL2: 0x%02x, HVCONTROL1:0x%02x, 2:0x%02x\n",
			MUIC_HV_DEV_NAME, __func__, phv->vps.bccontrol2, hvcontrol[0], hvcontrol[1]);

	/* attached - control */
	phv->vps.hvcontrol[0] = hvcontrol[0];
	phv->vps.hvcontrol[1] = hvcontrol[1];

	dpdnvden = phv->vps.bccontrol2 & BC_CONTROL2_DPDNMAN_MASK;

	pr_info("%s:%s vdnmon:0x%x vbadc:0x%x dpdnvden:0x%x\n",
		MUIC_HV_DEV_NAME, __func__, vdnmon, vbadc, dpdnvden);

	for (i = 0; i < ATTACHED_DEV_NUM; i++, tmp_afc_data = tmp_afc_data->next) {

		if (!flag_next) {
			pr_info("%s:%s not found new_dev in afc_condition_checklist\n",
				MUIC_HV_DEV_NAME, __func__);
			break;
		}

		if (tmp_afc_data->next == tmp_afc_data)
			flag_next = false;

		if (!(muic_check_hv_irq(phv, tmp_afc_data, irq)))
			continue;

		if (!(muic_check_bccontrol2_dpdnman(tmp_afc_data, dpdnvden)))
			continue;

		if (!(muic_check_gpstatus_vbadc(tmp_afc_data, vbadc)))
			continue;

		if(!(muic_check_bcstatus2_vdnmon(tmp_afc_data, vdnmon)))
			continue;

		pr_info("%s:%s checklist match found at i(%d), %s(%d)\n",
			MUIC_HV_DEV_NAME, __func__, i, tmp_afc_data->afc_name,
			tmp_afc_data->new_dev);

		intr = MUIC_INTR_ATTACH;

		break;
	}

	if (intr == MUIC_INTR_ATTACH) {
		pr_info("%s:%s AFC ATTACHED\n", MUIC_HV_DEV_NAME, __func__);
		pr_info("%s:%s %d->%d\n", MUIC_HV_DEV_NAME, __func__,
				phv->attached_dev, tmp_afc_data->new_dev);
		ret = max77865_hv_muic_handle_attach(phv, tmp_afc_data);
		if (ret)
			pr_err("%s:%s cannot handle attach(%d)\n", MUIC_HV_DEV_NAME,
				__func__, ret);
	} else {
		pr_info("%s:%s AFC MAINTAIN (%d)\n", MUIC_HV_DEV_NAME, __func__,
				phv->attached_dev);
		ret = max77865_hv_muic_state_maintain(phv);
		if (ret)
			pr_err("%s:%s cannot maintain state(%d)\n", MUIC_HV_DEV_NAME,
				__func__, ret);
		goto out;
	}

out:
	return;
}

/* TA setting in max77865-muic.c */
void max77865_muic_prepare_afc_charger(struct hv_data *phv)
{
	struct i2c_client *i2c = phv->i2c;
	int ret;

	pr_info("%s:%s \n", MUIC_DEV_NAME, __func__);

	max77865_hv_muic_adcmode_oneshot(phv);

	/* set HVCONTROL1=0x11 */
	ret = max77865_muic_hv_update_reg(i2c, MAX77865_MUIC_REG_CONTROL2_BC,
			(0x1 << BC_CONTROL2_DPDRV_SHIFT), BC_CONTROL2_DPDRV_MASK, true);
	if (ret < 0 )
		goto err_write;

	ret = max77865_muic_hv_update_reg(i2c, MAX77865_MUIC_REG_CONTROL2_BC,
			(MAX77865_ENABLE_BIT << BC_CONTROL2_DNMONEN_SHIFT),
			BC_CONTROL2_DNMONEN_MASK, true);
	if (ret < 0)
		goto err_write;

	ret = max77865_muic_hv_update_reg(i2c, MAX77865_MUIC_REG_CONTROL2_BC,
			(MAX77865_ENABLE_BIT << BC_CONTROL2_DPDNMAN_SHIFT),
			BC_CONTROL2_DPDNMAN_MASK, true);
	if (ret < 0)
		goto err_write;

	/* Set VBusADCEn = 1 after the time of changing adcmode*/

	max77865_muic_set_afc_ready(phv, true);

	return;

err_write:
	pr_err("%s:%s fail to write muic reg(%d)\n", MUIC_DEV_NAME, __func__, ret);
	return;
}

/* TA setting in max77865-muic.c */
bool max77865_muic_check_change_dev_afc_charger
		(struct hv_data *phv, muic_attached_dev_t new_dev)
{
	bool ret = true;

	if (new_dev == ATTACHED_DEV_TA_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_5V_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_9V_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_12V_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC || \
		new_dev == ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC || \
		new_dev == ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC || \
		new_dev == ATTACHED_DEV_QC_CHARGER_5V_MUIC || \
		new_dev == ATTACHED_DEV_QC_CHARGER_9V_MUIC) {
		if(muic_check_dev_ta(phv)) {
			ret = false;
		}
	}

	return ret;
}

static void max77865_hv_muic_detect_after_charger_init(struct work_struct *work)
{
	struct afc_init_data_s *init_data =
	    container_of(work, struct afc_init_data_s, muic_afc_init_work);
	struct hv_data *phv = init_data->phv;
	int ret;
	u8 bcstatus2 = 0, gpstatus = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	mutex_lock(phv->pmutex);

	/* check vdnmon status value */
	bcstatus2 = muic_i2c_read_byte(phv->i2c, MAX77865_MUIC_REG_STATUS2_BC);
	gpstatus = muic_i2c_read_byte(phv->i2c, MAX77865_MUIC_REG_STATUS_GP);
	if ((bcstatus2 < 0 ) || (gpstatus < 0)) {
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_HV_DEV_NAME,
				__func__, ret);
		return;
	}
	pr_info("%s:%s BC STATUS2:0x%02x, GP STATUS:0x%02x\n",
					MUIC_HV_DEV_NAME, __func__, bcstatus2, gpstatus);

	if (phv->is_afc_muic_ready) {
		if (phv->is_afc_muic_prepare)
			max77865_hv_muic_detect_dev(phv, phv->irq_vdnmon);
		else
			max77865_hv_muic_detect_dev(phv, -1);
	}

	mutex_unlock(phv->pmutex);
}

static int is_hv_cable(muic_data_t *pmuic)
{
	switch (pmuic->attached_dev) {
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		return 1;
	default:
		return 0;
	}
}

void hv_muic_change_afc_voltage(muic_data_t *pmuic, int tx_data)
{
	struct hv_data *phv = pmuic->phv;
	struct i2c_client *i2c = phv->i2c;
	int value;

	pr_info("%s: change afc voltage(%x)\n", __func__, tx_data);
	value = muic_i2c_read_byte(i2c, MAX77865_MUIC_REG_HVTXBYTE);
	if (value == tx_data) {
		pr_info("%s: same to current voltage %x\n", __func__, value);
		return;
	}
	phv->afc_count = 0;
	max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVTXBYTE, tx_data);

	/* QC Charger */
	if (phv->attached_dev == ATTACHED_DEV_QC_CHARGER_5V_MUIC ||
			phv->attached_dev == ATTACHED_DEV_QC_CHARGER_9V_MUIC) {
		switch (tx_data) {
			case MUIC_HV_5V:
				max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVCONTROL1, 0x01);
				max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_CONTROL2_BC, 0x14);
				break;
			case MUIC_HV_9V:
				max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVCONTROL1, 0x01);
				max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_CONTROL2_BC, 0x19);
				break;
			default:
				break;
		}
	}
	/* AFC Charger */
	else {
		max77865_hv_muic_adcmode_always_on(phv);
		max77865_hv_muic_afc_control_ping(phv, true);
	}
}

int max77865_muic_afc_set_voltage(int vol)
{
	muic_data_t *pmuic = hv_afc.pmuic;

	if (is_hv_cable(pmuic)) {
		if (vol == 5) {
			hv_muic_change_afc_voltage(pmuic, MUIC_HV_5V);
		} else if (vol == 9) {
			hv_muic_change_afc_voltage(pmuic, MUIC_HV_9V);
		} else {
			pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);
			return 0;
		}
	} else {
		pr_info("%s:%s It's NOT HV cable type\n", MUIC_DEV_NAME, __func__);
		return 0;
	}

	return 1;
}

void max77865_hv_muic_charger_init(void)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);

	if(afc_init_data.phv) {
		if (afc_init_data.phv->is_charger_ready) {
			pr_info("%s:%s charger is already ready.\n", MUIC_HV_DEV_NAME, __func__);
			return;
		}
		afc_init_data.phv->is_charger_ready = true;
		schedule_work(&afc_init_data.muic_afc_init_work);
	}
}

static void max77865_hv_muic_check_qc_vb(struct work_struct *work)
{
	struct hv_data *phv = container_of(work, struct hv_data, hv_muic_qc_vb_work.work);
	u8 gpstatus = 0, vbadc;

	if (!phv) {
		pr_err("%s:%s cannot read phv!\n", MUIC_HV_DEV_NAME, __func__);
		return;
	}

	mutex_lock(phv->pmutex);

	if (phv->is_qc_vb_settle == true) {
		pr_info("%s:%s already qc vb settled\n", MUIC_HV_DEV_NAME, __func__);
		goto out;
	}

	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);

	gpstatus = muic_i2c_read_byte(phv->i2c, MAX77865_MUIC_REG_STATUS_GP);
	vbadc = gpstatus & GP_STATUS_VBADC_MASK;

	if (vbadc == VBADC_4V_5V || vbadc == VBADC_5V_6V) {
		phv->is_qc_vb_settle = true;
		max77865_hv_muic_detect_dev(phv, phv->irq_vbadc);
	}

out:
	mutex_unlock(phv->pmutex);
	return;
}

static void max77865_hv_muic_check_mping_miss(struct work_struct *work)
{
	struct hv_data *phv = container_of(work, struct hv_data, hv_muic_mping_miss_wa.work);

	if (!phv) {
		pr_err("%s:%s cannot read phv!\n", MUIC_HV_DEV_NAME, __func__);
		return;
	}

	mutex_lock(phv->pmutex);

	/* Check the current device */
	if (phv->attached_dev != ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC &&
		phv->attached_dev!= ATTACHED_DEV_AFC_CHARGER_5V_MUIC) {
		pr_info("%s:%s MPing Missing did not happened "
			"but AFC protocol did not success\n",
			MUIC_HV_DEV_NAME, __func__);
		goto out;
	}

	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);

	/* We make MPING NACK interrupt virtually */
	max77865_hv_muic_detect_dev(phv, phv->irq_mpnack);

out:
	mutex_unlock(phv->pmutex);
	return;
}

void max77865_hv_muic_init_detect(struct hv_data *phv)
{
	int ret;
	u8 bcstatus2, vdnmon;

	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);

	mutex_lock(phv->pmutex);

	if (phv->is_boot_dpdnvden == DPDNVDEN_ENABLE)
		pr_info("%s:%s dpdnvden already ENABLE\n", MUIC_HV_DEV_NAME, __func__);
	else if (phv->is_boot_dpdnvden == DPDNVDEN_DISABLE) {
		mdelay(30);
		pr_info("%s:%s dpdnvden == DISABLE, 30ms delay\n", MUIC_HV_DEV_NAME, __func__);
	} else {
		pr_err("%s:%s dpdnvden is not correct(0x%x)!\n", MUIC_HV_DEV_NAME,
			__func__, phv->is_boot_dpdnvden);
		goto out;
	}

	ret = max77865_read_reg(phv->i2c, MAX77865_MUIC_REG_STATUS2_BC, &bcstatus2);
	if (ret) {
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_DEV_NAME, __func__, ret);
		vdnmon = VDNMON_DONTCARE;
	} else
		vdnmon = bcstatus2 & BC_STATUS2_DNVDATREF_MASK;

	if (vdnmon == VDNMON_LOW)
		max77865_hv_muic_detect_dev(phv, phv->irq_vdnmon);
	else
		pr_info("%s:%s vdnmon != LOW(0x%x)\n", MUIC_HV_DEV_NAME, __func__, vdnmon);

out:
	mutex_unlock(phv->pmutex);
}

static void max77865_hv_muic_init_check_dpdnvden(struct hv_data *phv)
{
	u8 bccontrol2 = 0;
	//int ret;

	mutex_lock(phv->pmutex);

	bccontrol2 = muic_i2c_read_byte(phv->i2c, MAX77865_MUIC_REG_CONTROL2_BC);
	if (bccontrol2 < 0) {
		pr_err("%s:%s cannot read BC CONTROL2 reg!\n", MUIC_HV_DEV_NAME, __func__);
		phv->is_boot_dpdnvden = DPDNVDEN_DONTCARE;
	} else
		phv->is_boot_dpdnvden = bccontrol2 & BC_CONTROL2_DPDNMAN_MASK;

	mutex_unlock(phv->pmutex);
}

static void max77865_hv_muic_initialize(struct hv_data *phv)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);

	phv->is_afc_handshaking = false;
	phv->is_afc_muic_prepare = false;
	//is_charger_ready is set by CHG on boot.
	//phv->is_charger_ready = false;
	phv->is_boot_dpdnvden = DPDNVDEN_DONTCARE;

	afc_init_data.phv = phv;
	INIT_WORK(&afc_init_data.muic_afc_init_work, max77865_hv_muic_detect_after_charger_init);

	INIT_DELAYED_WORK(&phv->hv_muic_qc_vb_work, max77865_hv_muic_check_qc_vb);
	INIT_DELAYED_WORK(&phv->hv_muic_mping_miss_wa, max77865_hv_muic_check_mping_miss);
}
	
void max77865_hv_muic_remove(struct hv_data *phv)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);
//	if(afc_init_data.muic_afc_init_work != NULL)
//		cancel_work_sync(&afc_init_data.muic_afc_init_work);
//	else
//		printk("sabin afc_init_data is NULL\n");
	cancel_delayed_work_sync(&phv->hv_muic_qc_vb_work);
	cancel_delayed_work(&phv->hv_muic_mping_miss_wa);
}

void max77865_hv_muic_remove_wo_free_irq(struct hv_data *phv)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);
	cancel_work_sync(&afc_init_data.muic_afc_init_work);
	cancel_delayed_work_sync(&phv->hv_muic_qc_vb_work);
	cancel_delayed_work(&phv->hv_muic_mping_miss_wa);
}

/*
 * APIs for muic to manage AFC.
 *
 */
void hv_initialize(muic_data_t *pmuic, struct hv_data **pphv)
{
	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);

	hv_afc.attached_dev = 0;
	hv_afc.i2c = pmuic->i2c;
	hv_afc.pmutex = &pmuic->muic_mutex;
	hv_afc.irq_gpio = pmuic->pdata->irq_gpio;
	hv_afc.is_muic_ready = pmuic->is_muic_ready;

	hv_afc.pmuic = pmuic;

	*pphv = &hv_afc;
}

void hv_clear_hvcontrol(struct hv_data *phv)
{
	struct i2c_client *i2c = phv->i2c;

	max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_CONTROL2_BC, 0x00);
	max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVCONTROL1, 0x00);
	max77865_hv_muic_write_reg(i2c, MAX77865_MUIC_REG_HVCONTROL2, 0x00);
}

void hv_configure_AFC(struct hv_data *phv)
{
	if (!phv) {
		pr_err("%s:%s: hv is not ready.\n", __func__, MUIC_HV_DEV_NAME);
		return;
	}

	pr_info("%s:%s\n", MUIC_HV_DEV_NAME, __func__);

	max77865_muic_set_afc_ready(phv, false);
	phv->afc_count = 0;

	max77865_hv_muic_initialize(phv);

	/* initial check dpdnvden before cable detection */
	max77865_hv_muic_init_check_dpdnvden(phv);
}

void hv_update_status(struct hv_data *phv, int mdev)
{
	if (!phv) {
		pr_err("%s:%s: hv is not ready.\n", __func__, MUIC_HV_DEV_NAME);
		return;
	}

	pr_info("%s:%s mdev %d->%d\n", __func__, MUIC_HV_DEV_NAME, phv->attached_dev, mdev);
	phv->attached_dev = mdev;
}

bool hv_is_predetach_required(int mdev)
{
	pr_info("%s:%s\n", __func__, MUIC_HV_DEV_NAME);

	switch (mdev) {
	case ATTACHED_DEV_TA_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_PREPARE_DUPLI_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_5V_DUPLI_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_9V_DUPLI_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_12V_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_12V_DUPLI_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
        case ATTACHED_DEV_AFC_CHARGER_ERR_V_DUPLI_MUIC:
        case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
        case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
        case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		return true;
	default:
		break;
	}

	return false;
}

bool hv_do_predetach(struct hv_data *phv, int mdev)
{
        bool noti;

	if (!phv) {
		pr_err("%s:%s: hv is not ready.\n", __func__, MUIC_HV_DEV_NAME);
		return false;
	}

	pr_info("%s:%s\n", __func__, MUIC_HV_DEV_NAME);

	noti = max77865_muic_check_change_dev_afc_charger(phv, mdev);

        if (noti) {
                max77865_muic_set_afc_ready(phv, false);
                phv->is_afc_muic_prepare = false;
                max77865_hv_muic_reset_hvcontrol_reg(phv);
                cancel_delayed_work(&phv->hv_muic_qc_vb_work);
                cancel_delayed_work(&phv->hv_muic_mping_miss_wa);
        }

	return noti;
}

bool hv_is_running(struct hv_data *phv)
{
	return phv->is_afc_muic_prepare;
}

void hv_do_detach(struct hv_data *phv)
{
	if (!phv) {
		pr_err("%s:%s: hv is not ready.\n", __func__, MUIC_HV_DEV_NAME);
		return;
	}

	pr_info("%s:%s\n", __func__, MUIC_HV_DEV_NAME);

	max77865_muic_set_afc_ready(phv, false);
	phv->is_afc_muic_prepare = false;

	cancel_delayed_work(&phv->hv_muic_qc_vb_work);
	pr_info("%s:%s cancel_delayed_work, Mping missing wa\n",
			MUIC_HV_DEV_NAME, __func__);
	cancel_delayed_work(&phv->hv_muic_mping_miss_wa);
}

/*
 * onoff : starting AFC (1), stopping AFC(0)
 */
void hv_set_afc_by_user(struct hv_data *phv, bool onoff)
{
	pr_info("%s:%s onof(%d)\n", __func__, MUIC_HV_DEV_NAME, onoff);

	if (!phv) {
		pr_err("%s:%s: hv is not ready.\n", __func__, MUIC_HV_DEV_NAME);
		return;
	}

	if (onoff)
		hv_muic_change_afc_voltage(phv->pmuic, MUIC_HV_9V);
	else
		hv_muic_change_afc_voltage(phv->pmuic, MUIC_HV_5V);
}
