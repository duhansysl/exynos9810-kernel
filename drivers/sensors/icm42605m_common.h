/*
 * TDK icm42605m driver
 *
 * Copyright 2020 TDK Inc.
 * Licensed under the GPL-2.
 */

#ifndef DRIVERS_INPUT_MISC_icm42605m_CORE_H_
#define DRIVERS_INPUT_MISC_icm42605m_CORE_H_

#include <linux/types.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/wakelock.h>
#include <linux/sensor/sensors_core.h>

#define ICM42605M_DEV_NAME		"ICM42605M"
#define VENDOR_NAME			"TDK"
#define MODULE_NAME_ACC			"accelerometer_sensor"
#define MODULE_NAME_GYRO		"gyro_sensor"
#define MODULE_NAME_SMD			"SignificantMotionDetector"
#define MODULE_NAME_TILT		"tilt_sensor"

#define FEATURE_LATCHEDMODE // For latch mode
#define CALIBRATION_FILE_PATH		"/efs/FactoryApp/accel_calibration_data"
#define CALIBRATION_DATA_AMOUNT		20
#define MAX_ACCEL_1G			4096
#define SA_DYNAMIC_THRESHOLD		50 /* mg */

#define ACCEL_LOG_TIME                  15 /* 15 sec */

#define ICM42605M_RX_MAX_LENGTH		500
#define ICM42605M_TX_MAX_LENGTH		500
#define SELFTEST_REVISED 1

#define ICM42605M_DELAY_DEFAULT           200000000LL
#define ICM42605M_DELAY_MIN               5000000LL

#define MIN_DEFAULT_ST_GYRO_DPS  60 /* expected values greater than 60dps */
#define MAX_DEFAULT_ST_GYRO_OFFSET_DPS 10 /* expected offset less than 10 dps */

#define MIN_DEFAULT_ST_ACCEL_MG 225 /* expected values in [225mgee;675mgee] */
#define MAX_DEFAULT_ST_ACCEL_MG 675

#define ACC_INT_TILT (1<<1)
#define ACC_INT_SMD   (1<<2)
#define ACC_INT_DRDY   (1<<3)


#define SENSOR_DATA_X(datax, datay, dataz, x1, y1, z1, x2, y2, z2, x3, y3, z3) \
				((x1 == 1 ? datax : (x1 == -1 ? -datax : 0)) + \
				(x2 == 1 ? datay : (x2 == -1 ? -datay : 0)) + \
				(x3 == 1 ? dataz : (x3 == -1 ? -dataz : 0)))

#define SENSOR_DATA_Y(datax, datay, dataz, x1, y1, z1, x2, y2, z2, x3, y3, z3) \
				((y1 == 1 ? datax : (y1 == -1 ? -datax : 0)) + \
				(y2 == 1 ? datay : (y2 == -1 ? -datay : 0)) + \
				(y3 == 1 ? dataz : (y3 == -1 ? -dataz : 0)))

#define SENSOR_DATA_Z(datax, datay, dataz, x1, y1, z1, x2, y2, z2, x3, y3, z3) \
				((z1 == 1 ? datax : (z1 == -1 ? -datax : 0)) + \
				(z2 == 1 ? datay : (z2 == -1 ? -datay : 0)) + \
				(z3 == 1 ? dataz : (z3 == -1 ? -dataz : 0)))

#define SENSOR_X_DATA(...)			SENSOR_DATA_X(__VA_ARGS__)
#define SENSOR_Y_DATA(...)			SENSOR_DATA_Y(__VA_ARGS__)
#define SENSOR_Z_DATA(...)			SENSOR_DATA_Z(__VA_ARGS__)


enum {
	ICM42605M_ACCEL = 0,
	ICM42605M_GYRO,
	ICM42605M_SIGN_MOTION,
	ICM42605M_TILT,
	ICM42605M_SENSORS_NUMB,
};

#define ICM42605M_ACCEL_DEPENDENCY	((1 << ICM42605M_ACCEL) | \
					(1 << ICM42605M_TILT) | \
					(1 << ICM42605M_SIGN_MOTION))

#define ICM42605M_EXTRA_DEPENDENCY	((1 << ICM42605M_TILT) | \
					(1 << ICM42605M_SIGN_MOTION))


/** @brief Common error code definition
 */
enum inv_error
{
	INV_ERROR_SUCCESS      = 0,   /**< no error */
	INV_ERROR              = -1,  /**< unspecified error */
	INV_ERROR_NIMPL        = -2,  /**< function not implemented for given
	                                   arguments */
	INV_ERROR_TRANSPORT    = -3,  /**< error occured at transport level */
	INV_ERROR_TIMEOUT      = -4,  /**< action did not complete in the expected
	                                   time window */
	INV_ERROR_SIZE         = -5,  /**< size/length of given arguments is not
	                                   suitable to complete requested action */
	INV_ERROR_OS           = -6,  /**< error related to OS */
	INV_ERROR_IO           = -7,  /**< error related to IO operation */
	INV_ERROR_MEM          = -9,  /**< not enough memory to complete requested
	                                   action */
	INV_ERROR_HW           = -10, /**< error at HW level */
	INV_ERROR_BAD_ARG      = -11, /**< provided arguments are not good to
	                                   perform requestion action */
	INV_ERROR_UNEXPECTED   = -12, /**< something unexpected happened */
	INV_ERROR_FILE         = -13, /**< cannot access file or unexpected format */
	INV_ERROR_PATH         = -14, /**< invalid file path */
	INV_ERROR_IMAGE_TYPE   = -15, /**< error when image type is not managed */
	INV_ERROR_WATCHDOG     = -16, /**< error when device doesn't respond 
									   to ping */
};

enum {
	FAIL = 0,
	PASS = 1,
};

enum {
	OFF = 0,
	ON = 1,
};

// TODO by TDK : Please add defined reigister and values

/* Sensitivity TEMP */
#define SENSITIVITY_TEMP		256	/** 256LSB/degrees Celsius  */
#define TEMP_OUTPUT_ZERO_LEVEL		25	/** 25 degrees Celsius  */

/* Timestamp */
#define TIMESTAMP_TO_NS 6400000

struct icm42605m_transfer_buffer {
	struct mutex buf_lock;
	u8 rx_buf[ICM42605M_RX_MAX_LENGTH];
	u8 tx_buf[ICM42605M_TX_MAX_LENGTH] ____cacheline_aligned;
};

struct icm42605m_data;

struct icm42605m_transfer_function {
	int (*write)(struct icm42605m_data *cdata, u8 reg_addr, int len, u8 *data,
		     bool b_lock);
	int (*read)(struct icm42605m_data *cdata, u8 reg_addr, int len, u8 *data,
		    bool b_lock);
};

struct icm42605m_data {
	struct input_dev *acc_input;
	struct input_dev *gyro_input;
	struct input_dev *smd_input;
	struct input_dev *tilt_input;
	struct input_dev *sc_input;
	struct input_dev *sd_input;

	atomic_t acc_wkqueue_en;
	ktime_t acc_delay;

	atomic_t gyro_wkqueue_en;
	ktime_t gyro_delay;

	struct work_struct acc_work;
    
	struct work_struct data_work;
#ifdef CONFIG_SENSORS_icm42605m_SUPPORT_VDIS	
	struct kthread_worker gyro_worker;
	struct kthread_work gyro_work;
	struct task_struct *gyro_task;
#else
	struct work_struct gyro_work;
#endif
	struct workqueue_struct *accel_wq;
	struct workqueue_struct *gyro_wq;
	struct workqueue_struct *irq_wq;

	struct hrtimer acc_timer;
	struct hrtimer gyro_timer;

	int acc_time_count;
	int gyro_time_count;

	struct device *acc_factory_dev;
	struct device *gyro_factory_dev;
	struct device *smd_factory_dev;
	struct device *tilt_factory_dev;

	const char *name;
	struct mutex mutex_read;

	s16 accel_data[3];
	s16 gyro_data[3];

	u8 acc_odr;
	u8 gyro_odr;
	u8 acc_fs;
	u8 gyro_fs;

	u8 sc_odr;
    u32 position;

	s8 orientation[9];

	s16 accel_cal_data[3];

	/* SEC specific,  */
	s32 gyro_st_off_data[3];
	s32 accel_st_off_data[3];
    s32 gyro_st_on_data[3];
	s32 accel_st_on_data[3];
    s32 gyro_st_ratio[3];
    s32 accel_st_ratio[3];
    s32 gyro_fifo_data[3];

	struct delayed_work sa_irq_work;
	struct wake_lock sa_wake_lock;
	int sa_irq_state;
	int sa_flag;
	int tilt_flag;
	int sa_factory_flag;
	int sa_factory_work;
	int states;

	int lpf_on;

	u8 enabled;

	struct mutex mutex_enable;

	int irq;
	int irq_gpio;
	int64_t timestamp;
	struct work_struct input_work;
	struct device *dev;
	struct mutex bank_registers_lock;
	const struct icm42605m_transfer_function *tf;
	struct icm42605m_transfer_buffer tb;
	struct notifier_block dump_nb;
	int icm42605m_ldo_pin;

	int dmp_is_on;
	u8 dmp_from_sram;                                        /**< DMP executes from SRAM */

};

int icm42605m_common_probe(struct icm42605m_data *cdata, int irq, u16 bustype);
void icm42605m_common_remove(struct icm42605m_data *cdata);
void icm42605m_common_shutdown(struct icm42605m_data *cdata);

#ifdef CONFIG_PM
int icm42605m_common_suspend(struct icm42605m_data *cdata);
int icm42605m_common_resume(struct icm42605m_data *cdata);
#endif /* CONFIG_PM */

#endif /* DRIVERS_INPUT_MISC_icm42605m_CORE_H_ */

