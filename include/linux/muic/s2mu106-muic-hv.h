/*
 * s2mu106-muic-hv.h - MUIC for the Samsung s2mu106
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

#ifndef __S2MU106_MUIC_HV_H__
#define __S2MU106_MUIC_HV_H__

#define MUIC_HV_DEV_NAME		"muic-s2mu106-hv"

/* S2MU106 AFC INT MASK register(0x08) */
#define S2MU106_VBADC_Im_SHIFT		0
#define S2MU106_VDNMON_Im_SHIFT		1
#define S2MU106_DNRES_Im_SHIFT		2
#define S2MU106_MPNACK_Im_SHIFT		3
#define S2MU106_MRXBUFOW_Im_SHIFT	4
#define S2MU106_MRXTRF_Im_SHIFT		5
#define S2MU106_MRXPERR_Im_SHIFT	6
#define S2MU106_MRXRDY_Im_SHIFT		7

#define S2MU106_VBADC_Im_MASK		(0x1 << S2MU106_VBADC_Im_SHIFT)
#define S2MU106_VDNMON_Im_MASK		(0x1 << S2MU106_VDNMON_Im_SHIFT)
#define S2MU106_DNRES_Im_MASK		(0x1 << S2MU106_DNRES_Im_SHIFT)
#define S2MU106_MPNACK_Im_MASK		(0x1 << S2MU106_MPNACK_Im_SHIFT)
#define S2MU106_MRXBUFOW_Im_MASK	(0x1 << S2MU106_MRXBUFOW_Im_SHIFT)
#define S2MU106_MRXTRF_Im_MASK		(0x1 << S2MU106_MRXTRF_Im_SHIFT)
#define S2MU106_MRXPERR_Im_MASK		(0x1 << S2MU106_MRXPERR_Im_SHIFT)
#define S2MU106_MRXRDY_Im_MASK		(0x1 << S2MU106_MRXRDY_Im_SHIFT)

/* S2MU106 AFC STATUS register(0x10) */
#define S2MU106_VBADC_SHIFT		0
#define S2MU106_VDNMON_SHIFT		4
#define S2MU106_DNRES_SHIFT		5
#define S2MU106_MPNACK_SHIFT		6
#define S2MU106_ADCEOC_SHIFT		7

#define S2MU106_VBADC_MASK		(0xf << S2MU106_VBADC_SHIFT)
#define S2MU106_VDNMON_MASK		(0x1 << S2MU106_VDNMON_SHIFT)
#define S2MU106_DNRES_MASK		(0x1 << S2MU106_DNRES_SHIFT)
#define S2MU106_MPNACK_MASK		(0x1 << S2MU106_MPNACK_SHIFT)
#define S2MU106_ADCEOC_MASK		(0x1 << S2MU106_ADCEOC_SHIFT)

/* S2MU106 AFC CONTROL 1 register(0x2B) */
#define S2MU106_DPDNVDEN_SHIFT		0
#define S2MU106_DNVD_SHIFT		1
#define S2MU106_DPVD_SHIFT		3
#define S2MU106_VBUSADCEN_SHIFT		5
#define S2MU106_CTRLIDMON_SHIFT		6
#define S2MU106_AFCEN_SHIFT		7

#define S2MU106_DPDNVDEN_MASK		(0x1 << S2MU106_DPDNVDEN_SHIFT)
#define S2MU106_DNVD_MASK		(0x3 << S2MU106_DNVD_SHIFT)
#define S2MU106_DPVD_MASK		(0x3 << S2MU106_DPVD_SHIFT)
#define S2MU106_VBUSADCEN_MASK		(0x1 << S2MU106_VBUSADCEN_SHIFT)
#define S2MU106_CTRLIDMON_MASK		(0x1 << S2MU106_CTRLIDMON_SHIFT)
#define S2MU106_AFCEN_MASK		(0x1 << S2MU106_AFCEN_SHIFT)

#define S2MU106_DPDN_HIZ		(0x0)
#define S2MU106_DPDN_GND		(0x1)
#define S2MU106_DPDN_0p6V		(0x2)
#define S2MU106_DPDN_3p3V		(0x3)
#define S2MU106_DP_HIZ_MASK		(S2MU106_DPDN_HIZ << S2MU106_DPVD_SHIFT)
#define S2MU106_DP_GND_MASK		(S2MU106_DPDN_GND << S2MU106_DPVD_SHIFT)
#define S2MU106_DP_0p6V_MASK		(S2MU106_DPDN_0p6V << S2MU106_DPVD_SHIFT)
#define S2MU106_DP_3p3V_MASK		(S2MU106_DPDN_3p3V << S2MU106_DPVD_SHIFT)
#define S2MU106_DN_HIZ_MASK		(S2MU106_DPDN_HIZ << S2MU106_DNVD_SHIFT)
#define S2MU106_DN_GND_MASK		(S2MU106_DPDN_GND << S2MU106_DNVD_SHIFT)
#define S2MU106_DN_0p6V_MASK		(S2MU106_DPDN_0p6V << S2MU106_DNVD_SHIFT)
#define S2MU106_DN_3p3V_MASK		(S2MU106_DPDN_3p3V << S2MU106_DNVD_SHIFT)

/* S2MU106 AFC CONTROL 2 register(0x2C) */
#define S2MU106_DP06EN_SHIFT		1
#define S2MU106_DNRESEN_SHIFT		2
#define S2MU106_MTXEN_SHIFT		3
#define S2MU106_MPING_SHIFT		4
#define S2MU106_RSTDM100UI_SHIFT	7

#define S2MU106_DP06EN_MASK		(0x1 << S2MU106_DP06EN_SHIFT)
#define S2MU106_DNRESEN_MASK		(0x1 << S2MU106_DNRESEN_SHIFT)
#define S2MU106_MTXEN_MASK		(0x1 << S2MU106_MTXEN_SHIFT)
#define S2MU106_MPING_MASK		(0x1 << S2MU106_MPING_SHIFT)
#define S2MU106_RSTDM100UI_MASK		(0x1 << S2MU106_RSTDM100UI_SHIFT)

/* S2MU106 AFC OTP 6 register(0x2C) */
#define S2MU106_IDM_ON_REG_SEL_SHIFT	6

#define S2MU106_IDM_ON_REG_SEL_MASK	(0x1 << S2MU106_IDM_ON_REG_SEL_SHIFT)

/* S2MU106 AFC TX BYTE DATA */
#define S2MU106_TX_BYTE_VOL_SHIFT	 4
#define S2MU106_TX_BYTE_VOL_MASK 	(0xf << S2MU106_TX_BYTE_VOL_SHIFT)
#define S2MU106_TX_BYTE_CUR_SHIFT 	0
#define S2MU106_TX_BYTE_CUR_MASK 	(0xf << S2MU106_TX_BYTE_CUR_SHIFT)

#define S2MU106_TX_BYTE_5V 0x0
#define S2MU106_TX_BYTE_6V 0x1
#define S2MU106_TX_BYTE_7V 0x2
#define S2MU106_TX_BYTE_8V 0x3
#define S2MU106_TX_BYTE_9V 0x4
#define S2MU106_TX_BYTE_10V 0x5
#define S2MU106_TX_BYTE_11V 0x6
#define S2MU106_TX_BYTE_12V 0x7
#define S2MU106_TX_BYTE_13V 0x8
#define S2MU106_TX_BYTE_14V 0x9
#define S2MU106_TX_BYTE_15V 0xA
#define S2MU106_TX_BYTE_16V 0xB
#define S2MU106_TX_BYTE_17V 0xC
#define S2MU106_TX_BYTE_18V 0xD
#define S2MU106_TX_BYTE_19V 0xE
#define S2MU106_TX_BYTE_20V 0xF

#define S2MU106_TX_BYTE_0p75A 0x0
#define S2MU106_TX_BYTE_0p90A 0x1
#define S2MU106_TX_BYTE_1p05A 0x2
#define S2MU106_TX_BYTE_1p20A 0x3
#define S2MU106_TX_BYTE_1p35A 0x4
#define S2MU106_TX_BYTE_1p50A 0x5
#define S2MU106_TX_BYTE_1p65A 0x6
#define S2MU106_TX_BYTE_1p80A 0x7
#define S2MU106_TX_BYTE_1p95A 0x8
#define S2MU106_TX_BYTE_2p10A 0x9
#define S2MU106_TX_BYTE_2p25A 0xA
#define S2MU106_TX_BYTE_2p40A 0xB
#define S2MU106_TX_BYTE_2p55A 0xC
#define S2MU106_TX_BYTE_2p70A 0xD
#define S2MU106_TX_BYTE_2p85A 0xE
#define S2MU106_TX_BYTE_3p00A 0xF

#if IS_ENABLED(CONFIG_HV_MUIC_S2MU106_AFC)
struct s2mu106_muic_data;
extern int s2mu106_hv_muic_init(struct s2mu106_muic_data *muic_data);
extern void s2mu106_hv_muic_remove(struct s2mu106_muic_data *muic_data);
extern muic_attached_dev_t s2mu106_hv_muic_check_id_err(struct s2mu106_muic_data *muic_data,
		muic_attached_dev_t new_dev);
#ifdef CONFIG_HV_MUIC_VOLTAGE_CTRL
extern int muic_afc_set_voltage(int vol);
extern int muic_afc_get_voltage(void);
#endif /* CONFIG_HV_MUIC_VOLTAGE_CTRL */
#endif /* CONFIG_HV_MUIC_S2MU106_AFC */
#endif /* __S2MU106_MUIC_HV_H__ */
