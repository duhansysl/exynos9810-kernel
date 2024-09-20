
#ifndef _DT_BINDINGS_BATTERY_SEC_BATTERY_H
#define _DT_BINDINGS_BATTERY_SEC_BATTERY_H

#define SEC_BATTERY_CABLE_UNKNOWN 		0
#define SEC_BATTERY_CABLE_NONE                  1 
#define SEC_BATTERY_CABLE_PREPARE_TA            2 
#define SEC_BATTERY_CABLE_TA                    3 
#define SEC_BATTERY_CABLE_USB                   4 
#define SEC_BATTERY_CABLE_USB_CDP               5 
#define SEC_BATTERY_CABLE_9V_TA                 6 
#define SEC_BATTERY_CABLE_9V_ERR                7 
#define SEC_BATTERY_CABLE_9V_UNKNOWN            8 
#define SEC_BATTERY_CABLE_12V_TA                9 
#define SEC_BATTERY_CABLE_WIRELESS              10
#define SEC_BATTERY_CABLE_HV_WIRELESS           11
#define SEC_BATTERY_CABLE_PMA_WIRELESS          12
#define SEC_BATTERY_CABLE_WIRELESS_PACK         13
#define SEC_BATTERY_CABLE_WIRELESS_HV_PACK      14
#define SEC_BATTERY_CABLE_WIRELESS_STAND        15
#define SEC_BATTERY_CABLE_WIRELESS_HV_STAND     16
#define SEC_BATTERY_CABLE_QC20                  17
#define SEC_BATTERY_CABLE_QC30                  18
#define SEC_BATTERY_CABLE_PDIC                  19
#define SEC_BATTERY_CABLE_UARTOFF               20
#define SEC_BATTERY_CABLE_OTG                   21
#define SEC_BATTERY_CABLE_LAN_HUB               22
#define SEC_BATTERY_CABLE_POWER_SHARING         23
#define SEC_BATTERY_CABLE_HMT_CONNECTED         24
#define SEC_BATTERY_CABLE_HMT_CHARGE            25
#define SEC_BATTERY_CABLE_HV_TA_CHG_LIMIT       26
#define SEC_BATTERY_CABLE_WIRELESS_VEHICLE      27
#define SEC_BATTERY_CABLE_WIRELESS_HV_VEHICLE   28
#define SEC_BATTERY_CABLE_PREPARE_WIRELESS_HV   29
#define SEC_BATTERY_CABLE_TIMEOUT               30
#define SEC_BATTERY_CABLE_SMART_OTG             31
#define SEC_BATTERY_CABLE_SMART_NOTG            32
#define SEC_BATTERY_CABLE_WIRELESS_TX           33 
#define SEC_BATTERY_CABLE_HV_WIRELESS_20        34 
#define SEC_BATTERY_CABLE_HV_WIRELESS_20_LIMIT  35 
#define SEC_BATTERY_CABLE_WIRELESS_FAKE 	36
#define SEC_BATTERY_CABLE_PREPARE_WIRELESS_20   37 
#define SEC_BATTERY_CABLE_PDIC_APDO             38 
#define SEC_BATTERY_CABLE_POGO			39
#define SEC_BATTERY_CABLE_POGO_9V		40
#define SEC_BATTERY_CABLE_MAX                   41 


/* temperature check type */
#define SEC_BATTERY_TEMP_CHECK_NONE 0        /* no temperature check */
#define SEC_BATTERY_TEMP_CHECK_ADC  1             /* by ADC value */
#define SEC_BATTERY_TEMP_CHECK_TEMP 2            /* by temperature */
#define SEC_BATTERY_TEMP_CHECK_FAKE 3            /* by a fake temperature */

/* ADC type */
	/* NOT using this ADC channel */
#define SEC_BATTERY_ADC_TYPE_NONE 	0
	/* ADC in AP */
#define SEC_BATTERY_ADC_TYPE_AP 	1
	 /* ADC by additional IC */
#define SEC_BATTERY_ADC_TYPE_IC 	2
#define SEC_BATTERY_ADC_TYPE_NUM 	3


/* thermal source */
/* by fuel gauge */
#define SEC_BATTERY_THERMAL_SOURCE_FG 		0
/* by external source */
#define SEC_BATTERY_THERMAL_SOURCE_CALLBACK	1
/* by ADC */
#define SEC_BATTERY_THERMAL_SOURCE_ADC		2
/* by charger */
#define SEC_BATTERY_THERMAL_SOURCE_CHG_ADC	3
/* none */
#define SEC_BATTERY_THERMAL_SOURCE_NONE		4

#define SEC_BATTERY_CABLE_CHECK_NOUSBCHARGE             1
/* SEC_BATTERY_CABLE_CHECK_NOINCOMPATIBLECHARGE
 * for incompatible charger
 * (Not compliant to USB specification,
 *  cable type is SEC_BATTERY_CABLE_UNKNOWN),
 * do NOT charge and show message to user
 * (only for VZW)
 */
#define SEC_BATTERY_CABLE_CHECK_NOINCOMPATIBLECHARGE    2
/* SEC_BATTERY_CABLE_CHECK_PSY
 * check cable by power supply set_property
 */
#define SEC_BATTERY_CABLE_CHECK_PSY                     4
/* SEC_BATTERY_CABLE_CHECK_INT
 * check cable by interrupt
 */
#define SEC_BATTERY_CABLE_CHECK_INT                     8
/* SEC_BATTERY_CABLE_CHECK_CHGINT
 * check cable by charger interrupt
 */
#define SEC_BATTERY_CABLE_CHECK_CHGINT                  16
/* SEC_BATTERY_CABLE_CHECK_POLLING
 * check cable by GPIO polling
 */
#define SEC_BATTERY_CABLE_CHECK_POLLING                 32


/* SEC_BATTERY_CABLE_SOURCE_EXTERNAL
 * already given by external argument
 */
#define SEC_BATTERY_CABLE_SOURCE_EXTERNAL       1
/* SEC_BATTERY_CABLE_SOURCE_CALLBACK
 * by callback (MUIC, USB switch)
 */
#define SEC_BATTERY_CABLE_SOURCE_CALLBACK       2
/* SEC_BATTERY_CABLE_SOURCE_ADC
 * by ADC
 */
#define SEC_BATTERY_CABLE_SOURCE_ADC            4


	/* polling work queue */
#define SEC_BATTERY_MONITOR_WORKQUEUE	0
	/* alarm polling */
#define SEC_BATTERY_MONITOR_ALARM	1
	/* timer polling (NOT USE) */
#define SEC_BATTERY_MONITOR_TIMER	2

/* OVP, UVLO check : POWER_SUPPLY_PROP_HEALTH */

	/* by callback function */
#define SEC_BATTERY_OVP_UVLO_CALLBACK		0
	/* by PMIC polling */
#define SEC_BATTERY_OVP_UVLO_PMICPOLLING	1
	/* by PMIC interrupt */
#define SEC_BATTERY_OVP_UVLO_PMICINT		2
	/* by charger polling */
#define SEC_BATTERY_OVP_UVLO_CHGPOLLING		3
	/* by charger interrupt */
#define SEC_BATTERY_OVP_UVLO_CHGINT		4


/* full charged check : POWER_SUPPLY_PROP_STATUS */

#define SEC_BATTERY_FULLCHARGED_NONE		0
	/* current check by ADC */
#define SEC_BATTERY_FULLCHARGED_ADC		1
	/* fuel gauge current check */
#define SEC_BATTERY_FULLCHARGED_FG_CURRENT	2
	/* time check */
#define SEC_BATTERY_FULLCHARGED_TIME		3
	/* SOC check */
#define SEC_BATTERY_FULLCHARGED_SOC		4
	/* charger GPIO, NO additional full condition */
#define SEC_BATTERY_FULLCHARGED_CHGGPIO		5
	/* charger interrupt, NO additional full condition */
#define SEC_BATTERY_FULLCHARGED_CHGINT		6
	/* charger power supply property, NO additional full condition */
#define SEC_BATTERY_FULLCHARGED_CHGPSY		7
	/* Limiter power supply property, NO additional full condition */
#define SEC_BATTERY_FULLCHARGED_LIMITER		8

#define	TEMP_CONTROL_SOURCE_NONE	0
#define	TEMP_CONTROL_SOURCE_BAT_THM	1
#define	TEMP_CONTROL_SOURCE_CHG_THM	2
#define	TEMP_CONTROL_SOURCE_WPC_THM	3
#define	TEMP_CONTROL_SOURCE_USB_THM	4

/* SEC_BATTERY_FULL_CONDITION_NOTIMEFULL
 * full-charged by absolute-timer only in high voltage
 */
#define SEC_BATTERY_FULL_CONDITION_NOTIMEFULL   1
/* SEC_BATTERY_FULL_CONDITION_NOSLEEPINFULL
 * do not set polling time as sleep polling time in full-charged
 */
#define SEC_BATTERY_FULL_CONDITION_NOSLEEPINFULL        2
/* SEC_BATTERY_FULL_CONDITION_SOC
 * use capacity for full-charged check
 */
#define SEC_BATTERY_FULL_CONDITION_SOC          4
/* SEC_BATTERY_FULL_CONDITION_VCELL
 * use VCELL for full-charged check
 */
#define SEC_BATTERY_FULL_CONDITION_VCELL        8
/* SEC_BATTERY_FULL_CONDITION_AVGVCELL
 * use average VCELL for full-charged check
 */
#define SEC_BATTERY_FULL_CONDITION_AVGVCELL     16
/* SEC_BATTERY_FULL_CONDITION_OCV
 * use OCV for full-charged check
 */
#define SEC_BATTERY_FULL_CONDITION_OCV          32

/* recharge check condition type (can be used overlapped) */
#define sec_battery_recharge_condition_t unsigned int
/* SEC_BATTERY_RECHARGE_CONDITION_SOC
 * use capacity for recharging check
 */
#define SEC_BATTERY_RECHARGE_CONDITION_SOC              1
/* SEC_BATTERY_RECHARGE_CONDITION_AVGVCELL
 * use average VCELL for recharging check
 */
#define SEC_BATTERY_RECHARGE_CONDITION_AVGVCELL         2
/* SEC_BATTERY_RECHARGE_CONDITION_VCELL
 * use VCELL for recharging check
 */
#define SEC_BATTERY_RECHARGE_CONDITION_VCELL            4

/* SEC_BATTERY_RECHARGE_CONDITION_LIMITER
 * use VCELL of LIMITER for recharging check
 */
#define SEC_BATTERY_RECHARGE_CONDITION_LIMITER          8

#endif /* _DT_BINDINGS_BATTERY_SEC_BATTERY_H */
