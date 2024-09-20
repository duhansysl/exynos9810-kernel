/* sec_vibrator.h
 *
 * Copyright (C) 2019 Samsung Electronics
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef SEC_VIBRATOR_H
#define SEC_VIBRATOR_H

#include <linux/kthread.h>
#include <linux/leds.h>

#define MAX_INTENSITY			10000
#define MAX_TIMEOUT			10000
#define PACKET_MAX_SIZE			1000

#define HAPTIC_ENGINE_FREQ_MIN		1200
#define HAPTIC_ENGINE_FREQ_MAX		3500

#define VIB_BUFSIZE                     30

#define HOMEKEY_DURATION		7

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

struct sec_vibrator_ops {
	int (*enable)(struct device *dev, bool en);
	int (*set_intensity)(struct device *dev, int intensity);
	int (*set_frequency)(struct device *dev, int frequency);
	int (*set_overdrive)(struct device *dev, bool en);
	int (*get_motor_type)(struct device *dev, char *buf);
	int (*get_num_waves)(struct device *dev);
	int (*set_cp_trigger_index)(struct device *dev, int index);
	int (*get_cp_trigger_index)(struct device *dev);
	int (*set_cp_trigger_queue)(struct device *dev, int index);
	int (*get_cp_trigger_queue)(struct device *dev);
	int (*set_force_touch_intensity)(struct device *dev, int intensity);
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

	int force_touch_intensity;
	int intensity;
	int frequency;
	bool overdrive;

	int timeout;

	struct led_classdev cdev;
	int state;
	int duration;
};

extern int sec_vibrator_register(struct sec_vibrator_drvdata *ddata);
extern int sec_vibrator_unregister(struct sec_vibrator_drvdata *ddata);

#endif /* SEC_VIBRATOR_H */
