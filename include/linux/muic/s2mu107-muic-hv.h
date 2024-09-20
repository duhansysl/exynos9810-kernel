/*
 * s2mu107-muic-hv.h - MUIC for the Samsung s2mu107
 *
 * Copyright (C) 2019 Samsung Electrnoics
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
#ifndef __S2MU107_MUIC_HV_H__
#define __S2MU107_MUIC_HV_H__

#define MUIC_HV_DEV_NAME		"muic-s2mu107-hv"

/* S2MU107 AFC_INT register (0x0) */
#define S2MU107_MRxRdy_I_SHIFT		7
#define S2MU107_MRxPerr_I_SHIFT		6
#define S2MU107_MRxTrf_I_SHIFT		5
#define S2MU107_MRxBufOw_I_SHIFT	4
#define S2MU107_MPNack_I_SHIFT		3
#define S2MU107_DNRes_I_SHIFT		2
#define S2MU107_VDNMon_I_SHIFT		1

#define S2MU107_MRxRdy_I_MASK		(0x1 << S2MU107_MRxRdy_I_SHIFT)
#define S2MU107_MRxPerr_I_MASK		(0x1 << S2MU107_MRxPerr_I_SHIFT)
#define S2MU107_MRxTrf_I_MASK		(0x1 << S2MU107_MRxTrf_I_SHIFT)
#define S2MU107_MRxBufOw_I_MASK		(0x1 << S2MU107_MRxBufOw_I_SHIFT)
#define S2MU107_MPNack_I_MASK		(0x1 << S2MU107_MPNack_I_SHIFT)
#define S2MU107_DNRes_I_MASK		(0x1 << S2MU107_DNRes_I_SHIFT)
#define S2MU107_VDNMon_I_MASK		(0x1 << S2MU107_VDNMon_I_SHIFT)

/* S2MU107 AFC_INT_MASK register (0x08) */
#define S2MU107_MRxRdy_Im_SHIFT	        7
#define S2MU107_MRxPerr_Im_SHIFT	6
#define S2MU107_MRxTrf_Im_SHIFT         5
#define S2MU107_MRxBufOw_Im_SHIFT	4
#define S2MU107_MPNack_Im_SHIFT		3
#define S2MU107_DNRes_Im_SHIFT		2
#define S2MU107_VDNMon_Im_SHIFT		1

#define S2MU107_MRxRdy_Im_MASK		(0x1 << S2MU107_MRxRdy_Im_SHIFT)
#define S2MU107_MRxPerr_Im_MASK	        (0x1 << S2MU107_MRxPerr_Im_SHIFT)
#define S2MU107_MRxTrf_Im_MASK	        (0x1 << S2MU107_MRxTrf_Im_SHIFT)
#define S2MU107_MRxBufOw_Im_MASK	(0x1 << S2MU107_MRxBufOw_Im_SHIFT)
#define S2MU107_MPNack_Im_MASK	        (0x1 << S2MU107_MPNack_Im_SHIFT)
#define S2MU107_DNRes_Im_MASK           (0x1 << S2MU107_DNRes_Im_SHIFT)
#define S2MU107_VDNMon_Im_MASK	        (0x1 << S2MU107_VDNMon_Im_SHIFT)

/* S2MU107 AFC_STATUS register (0x10) */
#define S2MU107_MPNack_3OR_LATCH_SHIFT	6
#define S2MU107_DNRes_SHIFT		5
#define S2MU107_VDNMon_SHIFT		4

#define S2MU107_MPNack_3OR_LATCH_MASK	(0x1 << S2MU107_MPNack_3OR_LATCH_SHIFT)
#define S2MU107_DNRes_MASK		(0x1 << S2MU107_DNRes_SHIFT)
#define S2MU107_VDNMon_MASK		(0x1 << S2MU107_VDNMon_SHIFT)

/* S2MU107 AFC_CONTROL1 register (0x2D) */
#define S2MU107_DPDNVDEN_SHIFT	        0
#define S2MU107_DNVD_SHIFT		1
#define S2MU107_DPVD_SHIFT		3
#define S2MU107_CTRLIDMON_SHIFT	        6
#define S2MU107_AFCEN_SHIFT		7

#define S2MU107_DPDNVDEN_MASK		(0x1 << S2MU107_DPDNVDEN_SHIFT)
#define S2MU107_DNVD_MASK		(0x3 << S2MU107_DNVD_SHIFT)
#define S2MU107_DPVD_MASK		(0x3 << S2MU107_DPVD_SHIFT)
#define S2MU107_CTRLIDMON_MASK	        (0x1 << S2MU107_CTRLIDMON_SHIFT)
#define S2MU107_AFCEN_MASK		(0x1 << S2MU107_AFCEN_SHIFT)

#define S2MU107_DPDN_HIZ		(0x0)
#define S2MU107_DPDN_GND		(0x1)
#define S2MU107_DPDN_0p6V		(0x2)
#define S2MU107_DPDN_3p3V		(0x3)
#define S2MU107_DP_HIZ_MASK		(S2MU107_DPDN_HIZ << S2MU107_DPVD_SHIFT)
#define S2MU107_DP_GND_MASK		(S2MU107_DPDN_GND << S2MU107_DPVD_SHIFT)
#define S2MU107_DP_0p6V_MASK		(S2MU107_DPDN_0p6V << S2MU107_DPVD_SHIFT)
#define S2MU107_DP_3p3V_MASK		(S2MU107_DPDN_3p3V << S2MU107_DPVD_SHIFT)
#define S2MU107_DN_HIZ_MASK		(S2MU107_DPDN_HIZ << S2MU107_DNVD_SHIFT)
#define S2MU107_DN_GND_MASK		(S2MU107_DPDN_GND << S2MU107_DNVD_SHIFT)
#define S2MU107_DN_0p6V_MASK		(S2MU107_DPDN_0p6V << S2MU107_DNVD_SHIFT)
#define S2MU107_DN_3p3V_MASK		(S2MU107_DPDN_3p3V << S2MU107_DNVD_SHIFT)

/* S2MU107 AFC_CONTROL2 register (0x2E) */
#define S2MU107_MPING_RST_SHIFT 	0
#define S2MU107_DP06EN_SHIFT		1
#define S2MU107_DNRESEN_SHIFT		2
#define S2MU107_MTXEN_SHIFT		3
#define S2MU107_MPING_SHIFT		4
#define S2MU107_QCCMODE_DM_RST_SHIFT	5
#define S2MU107_QCCMODE_DP_RST_SHIFT	6
#define S2MU107_RSTDM100UI_SHIFT	7

#define S2MU107_MPING_RST_MASK	        (0x1 << S2MU107_MPING_RST_SHIFT)
#define S2MU107_DP06EN_MASK		(0x1 << S2MU107_DP06EN_SHIFT)
#define S2MU107_DNRESEN_MASK		(0x1 << S2MU107_DNRESEN_SHIFT)
#define S2MU107_MTXEN_MASK		(0x1 << S2MU107_MTXEN_SHIFT)
#define S2MU107_MPING_MASK		(0x1 << S2MU107_MPING_SHIFT)
#define S2MU107_QCCMODE_DM_RST_MASK	(0x1 << S2MU107_QCCMODE_DM_RST_SHIFT)
#define S2MU107_QCCMODE_DP_RST_MASK	(0x1 << S2MU107_QCCMODE_DP_RST_SHIFT)
#define S2MU107_RSTDM100UI_MASK 	(0x1 << S2MU107_RSTDM100UI_SHIFT)

/* S2MU107 TX_BYTE DATA (0x2F) */
#define S2MU107_TX_BYTE_VOL_SHIFT       4
#define S2MU107_TX_BYTE_VOL_MASK        (0xf << S2MU107_TX_BYTE_VOL_SHIFT)
#define S2MU107_TX_BYTE_CUR_SHIFT       0
#define S2MU107_TX_BYTE_CUR_MASK        (0xf << S2MU107_TX_BYTE_CUR_SHIFT)

#define S2MU107_TX_BYTE_5V 0x0
#define S2MU107_TX_BYTE_6V 0x1
#define S2MU107_TX_BYTE_7V 0x2
#define S2MU107_TX_BYTE_8V 0x3
#define S2MU107_TX_BYTE_9V 0x4
#define S2MU107_TX_BYTE_10V 0x5
#define S2MU107_TX_BYTE_11V 0x6
#define S2MU107_TX_BYTE_12V 0x7
#define S2MU107_TX_BYTE_13V 0x8
#define S2MU107_TX_BYTE_14V 0x9
#define S2MU107_TX_BYTE_15V 0xA
#define S2MU107_TX_BYTE_16V 0xB
#define S2MU107_TX_BYTE_17V 0xC
#define S2MU107_TX_BYTE_18V 0xD
#define S2MU107_TX_BYTE_19V 0xE
#define S2MU107_TX_BYTE_20V 0xF

#define S2MU107_TX_BYTE_0p75A 0x0
#define S2MU107_TX_BYTE_0p90A 0x1
#define S2MU107_TX_BYTE_1p05A 0x2
#define S2MU107_TX_BYTE_1p20A 0x3
#define S2MU107_TX_BYTE_1p35A 0x4
#define S2MU107_TX_BYTE_1p50A 0x5
#define S2MU107_TX_BYTE_1p65A 0x6
#define S2MU107_TX_BYTE_1p80A 0x7
#define S2MU107_TX_BYTE_1p95A 0x8
#define S2MU107_TX_BYTE_2p10A 0x9
#define S2MU107_TX_BYTE_2p25A 0xA
#define S2MU107_TX_BYTE_2p40A 0xB
#define S2MU107_TX_BYTE_2p55A 0xC
#define S2MU107_TX_BYTE_2p70A 0xD
#define S2MU107_TX_BYTE_2p85A 0xE
#define S2MU107_TX_BYTE_3p00A 0xF

/* S2MU107 AFC_OTP6 register (0x6A) */
#define S2MU107_CTRL_IDM_ON_REG_SEL_SHIFT 	6

#define S2MU107_CTRL_IDM_ON_REG_SEL_MASK	(0x1 << S2MU107_CTRL_IDM_ON_REG_SEL_SHIFT)

#if IS_ENABLED(CONFIG_HV_MUIC_S2MU107_AFC)
struct s2mu107_muic_data;
extern int s2mu107_hv_muic_init(struct s2mu107_muic_data *muic_data);
extern void s2mu107_hv_muic_remove(struct s2mu107_muic_data *muic_data);
extern muic_attached_dev_t s2mu107_hv_muic_check_id_err(struct s2mu107_muic_data *muic_data,
        muic_attached_dev_t new_dev);
#ifdef CONFIG_HV_MUIC_VOLTAGE_CTRL
extern int muic_afc_set_voltage(int vol);
#endif /* CONFIG_HV_MUIC_S2MU107_AFC */
#endif /* CONFIG_HV_MUIC_VOLTAGE_CTRL */
#endif /* __S2MU107_MUIC_HV_H__ */
