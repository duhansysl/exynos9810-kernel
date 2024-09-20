#ifndef FIMC_IS_VENDOR_CONFIG_ATT_V03_H
#define FIMC_IS_VENDOR_CONFIG_ATT_V03_H

#include "fimc-is-otprom-front-gc5035_v001.h"
#include "fimc-is-eeprom-rear-3l6_v018.h"

#define VENDER_PATH

/***** CAL ROM DEFINE *****/
#define COUNT_EXTEND_CAL_DATA   (0)               /* For searching of extend cal data name. If it is not used then set '0' */
#define SENSOR_OTP_GC5035                            /* Support read OTPROM for GC5035 */
#define ROM_DEBUG
#define ROM_CRC32_DEBUG

#define USE_3L6B_SETFILE

#define FIMC_IS_MAX_FW_BUFFER_SIZE (4100 * 1024)

#define CAMERA_OIS_GYRO_OFFSET_SPEC 30000

#define RTA_CODE_AREA_SIZE (0x00200000)

#define USE_CAMERA_EMBEDDED_HEADER

#define USE_CAMERA_FACTORY_DRAM_TEST

#ifndef CONFIG_SEC_FACTORY
#define USE_CAMERA_PREPARE_RETENTION_ON_BOOT
#endif

#ifdef USE_CAMERA_PREPARE_RETENTION_ON_BOOT
#define USE_CAMERA_PREPARE_RETENTION_MODE_CHANGE_ON_BOOT
#endif

#define USE_CAMERA_HW_BIG_DATA

#ifdef USE_CAMERA_HW_BIG_DATA

#define CSI_SCENARIO_SEN_FRONT	(1)
#define CSI_SCENARIO_TELE		(2)
#define CSI_SCENARIO_SECURE		(3)
#define CSI_SCENARIO_SEN_REAR	(0)
#endif

#define USE_AF_SLEEP_MODE

/* It should be align with DDK and RTA side */
#define USE_NEW_PER_FRAME_CONTROL

/* define supported aperture level */
#define FROM_SUPPORT_APERTURE_F1	// First step of aperture (Default)
#define FROM_SUPPORT_APERTURE_F2	// Second step of aperture.

//TEMP_R7 #define APERTURE_CLOSE_VALUE F1_5

#if defined(CONFIG_USE_CAMERA_LDU) || defined(CONFIG_SEC_FACTORY)
#define USE_OIS_SHIFT_FOR_APERTURE
#endif

#ifdef USE_OIS_SHIFT_FOR_APERTURE
#if defined(CONFIG_SEC_FACTORY)
#define OIS_SHIFT_OFFSET_VALUE_NORMAL 0
#else
#define OIS_SHIFT_OFFSET_VALUE_NORMAL 1000
#endif
#endif

#define FIXED_SENSOR_CROP_SHIFT_NUM	0
/* #define USE_BUCK2_REGULATOR_CONTROL */

#define BDS_IN_VIDEO

//#undef ENABLE_DVFS

#define USE_MS_PDAF
#define USE_MS_PDAF_INTERFACE

#define USE_CAMERA_ACT_DRIVER_SOFT_LANDING 	/* Using NRC based softlanding for DW-9808*/

#endif /* FIMC_IS_VENDOR_CONFIG_ATT_V03_H */

