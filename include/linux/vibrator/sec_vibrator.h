/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2019 Samsung Electronics
 */

#ifndef SEC_VIBRATOR_H
#define SEC_VIBRATOR_H

#include <linux/kthread.h>
#include <linux/kdev_t.h>
#include <linux/device.h>

#define MAX_INTENSITY		10000
#define MAX_TIMEOUT		10000
#define PACKET_MAX_SIZE		1000

#define HAPTIC_ENGINE_FREQ_MIN	1200
#define HAPTIC_ENGINE_FREQ_MAX	3500

#define VIB_BUFSIZE		30

#define HOMEKEY_PRESS_FREQ		5
#define HOMEKEY_RELEASE_FREQ		6
#define HOMEKEY_DURATION	7

struct vib_packet {
	int time;
	int intensity;
	int freq;
	int overdrive;
};

enum {
	VIB_PACKET_TIME = 0,
	VIB_PACKET_INTENSITY,
	VIB_PACKET_FREQUENCY,
	VIB_PACKET_OVERDRIVE,
	VIB_PACKET_MAX,
};

enum {
	FREQ_ALERT = 0,	/* 157.5Hz */
	FREQ_ZERO,	/* 180Hz */
	FREQ_LOW,	/* 120Hz */
	FREQ_MID,	/* 150Hz */
	FREQ_HIGH,	/* 200Hz */
	FREQ_PRESS,	/* force touch press */
	FREQ_RELEASE,	/* force touch release */
	FREQ_MAX,
};

enum EVENT_CMD {
	EVENT_CMD_NONE = 0,
	EVENT_CMD_FOLDER_CLOSE,
	EVENT_CMD_FOLDER_OPEN,
	EVENT_CMD_ACCESSIBILITY_BOOST_ON,
	EVENT_CMD_ACCESSIBILITY_BOOST_OFF,
	EVENT_CMD_TENT_CLOSE,
	EVENT_CMD_TENT_OPEN,
	EVENT_CMD_MAX,
};

#define MAX_STR_LEN_VIB_TYPE 32
#define MAX_STR_LEN_EVENT_CMD 32

struct sec_vibrator_ops {
	int (*enable)(struct device *dev, bool en);
	int (*set_intensity)(struct device *dev, int intensity);
	int (*set_frequency)(struct device *dev, int frequency);
	int (*set_overdrive)(struct device *dev, bool en);
	int (*get_motor_type)(struct device *dev, char *buf);
	int (*set_use_sep_index)(struct device *dev, bool use_sep_index);
	ssize_t (*get_num_waves)(struct device *dev, char *buf);
	ssize_t (*set_cp_trigger_index)(struct device *dev, const char *buf);
	ssize_t (*get_cp_trigger_index)(struct device *dev, char *buf);
	ssize_t (*set_cp_trigger_queue)(struct device *dev, const char *buf);
	ssize_t (*get_cp_trigger_queue)(struct device *dev, char *buf);
	int (*set_force_touch_intensity)(struct device *dev, int intensity);
	int (*set_tuning_with_temp)(struct device *dev, int temperature);
	int (*set_event_cmd)(struct device *dev, int event_idx);
	bool (*get_calibration)(struct device *dev);
	int (*get_step_size)(struct device *dev, int *step_size);
	int (*get_intensities)(struct device *dev, int *buf);
	int (*set_intensities)(struct device *dev, int *buf);
	int (*get_haptic_intensities)(struct device *dev, int *buf);
	int (*set_haptic_intensities)(struct device *dev, int *buf);
	int (*get_haptic_durations)(struct device *dev, int *buf);
	int (*set_haptic_durations)(struct device *dev, int *buf);
	ssize_t (*set_pwle)(struct device *dev, const char *buf);
	ssize_t (*get_pwle)(struct device *dev, char *buf);
	ssize_t (*get_virtual_composite_indexes)(struct device *dev, char *buf);
	ssize_t (*get_virtual_pwle_indexes)(struct device *dev, char *buf);
};

struct sec_vibrator_drvdata {
	struct class *to_class;
	struct device *to_dev;
	struct device *dev;
	struct hrtimer timer;
	struct kthread_worker kworker;
	struct kthread_work kwork;
	struct mutex vib_mutex;
	struct vib_packet vib_pac[PACKET_MAX_SIZE];
	const struct sec_vibrator_ops *vib_ops;

	bool f_packet_en;
	bool packet_running;
	int packet_size;
	int packet_cnt;
	unsigned int index;

	int force_touch_intensity;
	int intensity;
	int frequency;
	bool overdrive;

	int timeout;

	char event_cmd[MAX_STR_LEN_EVENT_CMD];

	bool is_registered;
};

extern int sec_vibrator_register(struct sec_vibrator_drvdata *ddata);
extern int sec_vibrator_unregister(struct sec_vibrator_drvdata *ddata);
#endif /* SEC_VIBRATOR_H */
