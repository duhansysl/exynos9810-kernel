// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019-2021 Samsung Electronics Co. Ltd.
 */

#define pr_fmt(fmt) "[VIB] " fmt
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/sec_sysfs.h>
#include <linux/vibrator/sec_vibrator.h>
#if defined(CONFIG_SSP_MOTOR_CALLBACK)
#include <linux/ssp_motorcallback.h>
#endif
#if IS_ENABLED(CONFIG_SEC_VIB_NOTIFIER)
#include <linux/vibrator/sec_vibrator_notifier.h>
#endif
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG_V2)
#include "../battery_v2/include/sec_charging_common.h"
#else
#include "../battery/include/sec_charging_common.h"
#endif

static const int  kMaxBufSize = 7;
static const int kMaxHapticStepSize = 7;
static const char *str_newline = "\n";

static struct sec_vibrator_drvdata *g_ddata;

static char vib_event_cmd[MAX_STR_LEN_EVENT_CMD];

static const char sec_vib_event_cmd[EVENT_CMD_MAX][MAX_STR_LEN_EVENT_CMD] = {
	[EVENT_CMD_NONE]					= "NONE",
	[EVENT_CMD_FOLDER_CLOSE]				= "FOLDER_CLOSE",
	[EVENT_CMD_FOLDER_OPEN]					= "FOLDER_OPEN",
	[EVENT_CMD_ACCESSIBILITY_BOOST_ON]			= "ACCESSIBILITY_BOOST_ON",
	[EVENT_CMD_ACCESSIBILITY_BOOST_OFF]			= "ACCESSIBILITY_BOOST_OFF",
	[EVENT_CMD_TENT_CLOSE]					= "FOLDER_TENT_CLOSE",
	[EVENT_CMD_TENT_OPEN]					= "FOLDER_TENT_OPEN",
};

#if IS_ENABLED(CONFIG_SEC_VIB_NOTIFIER)
static struct vib_notifier_context vib_notifier;
static struct blocking_notifier_head sec_vib_nb_head = BLOCKING_NOTIFIER_INIT(sec_vib_nb_head);

int sec_vib_notifier_register(struct notifier_block *noti_block)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	if (!noti_block)
		return -EINVAL;

	ret = blocking_notifier_chain_register(&sec_vib_nb_head, noti_block);
	if (ret < 0)
		pr_err("%s: failed(%d)\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(sec_vib_notifier_register);

int sec_vib_notifier_unregister(struct notifier_block *noti_block)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	if (!noti_block)
		return -EINVAL;

	ret = blocking_notifier_chain_unregister(&sec_vib_nb_head, noti_block);
	if (ret < 0)
		pr_err("%s: failed(%d)\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL_GPL(sec_vib_notifier_unregister);

static int sec_vib_notifier_notify(int en, struct sec_vibrator_drvdata *ddata)
{
	int ret = 0;

	if (!ddata) {
		pr_err("%s : ddata is NULL\n", __func__);
		return -ENODATA;
	}

	vib_notifier.index = ddata->index;
	vib_notifier.timeout = ddata->timeout;

	pr_info("%s: %s, idx: %d timeout: %d\n", __func__, en ? "ON" : "OFF",
		vib_notifier.index, vib_notifier.timeout);

	ret = blocking_notifier_call_chain(&sec_vib_nb_head, en, &vib_notifier);

	switch (ret) {
	case NOTIFY_DONE:
	case NOTIFY_OK:
		pr_info("%s done(0x%x)\n", __func__, ret);
		break;
	default:
		pr_info("%s failed(0x%x)\n", __func__, ret);
		break;
	}

	return ret;
}
#endif /* if IS_ENABLED(CONFIG_SEC_VIB_NOTIFIER) */

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
static int sec_vibrator_check_temp(struct sec_vibrator_drvdata *ddata)
{
	int ret = 0;
	union power_supply_propval value = {0, };

	if (!ddata) {
		pr_err("%s : ddata is NULL\n", __func__);
		return -ENODATA;
	}

	if (!ddata->vib_ops->set_tuning_with_temp)
		return -ENOSYS;

	psy_do_property("battery", get, POWER_SUPPLY_PROP_TEMP, value);

	ret = ddata->vib_ops->set_tuning_with_temp(ddata->dev, value.intval);

	if (ret)
		pr_err("%s error(%d)\n", __func__, ret);

	return ret;
}
#endif
static int sec_vibrator_set_enable(struct sec_vibrator_drvdata *ddata, bool en)
{
	int ret = 0;

	if (!ddata) {
		pr_err("%s : ddata is NULL\n", __func__);
		return -ENODATA;
	}

	if (!ddata->vib_ops->enable)
		return -ENOSYS;

	ret = ddata->vib_ops->enable(ddata->dev, en);
	if (ret)
		pr_err("%s error(%d)\n", __func__, ret);

#if defined(CONFIG_SSP_MOTOR_CALLBACK)
	if (en && ddata->intensity > 0)
		setSensorCallback(true, ddata->timeout);
	else
		setSensorCallback(false, 0);
#endif
#if IS_ENABLED(CONFIG_SEC_VIB_NOTIFIER)
	sec_vib_notifier_notify(en, ddata);
#endif
	return ret;
}

static int sec_vibrator_set_intensity(struct sec_vibrator_drvdata *ddata, int intensity)
{
	int ret = 0;

	if (!ddata) {
		pr_err("%s : ddata is NULL\n", __func__);
		return -ENODATA;
	}

	if (!ddata->vib_ops->set_intensity)
		return -ENOSYS;

	if ((intensity < -(MAX_INTENSITY)) || (intensity > MAX_INTENSITY)) {
		pr_err("%s out of range(%d)\n", __func__, intensity);
		return -EINVAL;
	}

	ret = ddata->vib_ops->set_intensity(ddata->dev, intensity);

	if (ret)
		pr_err("%s error(%d)\n", __func__, ret);

	return ret;
}

static int sec_vibrator_set_frequency(struct sec_vibrator_drvdata *ddata, int frequency)
{
	int ret = 0;

	if (!ddata) {
		pr_err("%s : ddata is NULL\n", __func__);
		return -ENODATA;
	}

	if (!ddata->vib_ops->set_frequency)
		return -ENOSYS;

	if ((frequency < FREQ_ALERT) ||
	    ((frequency >= FREQ_MAX) && (frequency < HAPTIC_ENGINE_FREQ_MIN)) ||
	    (frequency > HAPTIC_ENGINE_FREQ_MAX)) {
		pr_err("%s out of range(%d)\n", __func__, frequency);
		return -EINVAL;
	}

	ret = ddata->vib_ops->set_frequency(ddata->dev, frequency);

	if (ret)
		pr_err("%s error(%d)\n", __func__, ret);

	return ret;
}

static int sec_vibrator_set_overdrive(struct sec_vibrator_drvdata *ddata, bool en)
{
	int ret = 0;

	if (!ddata) {
		pr_err("%s : ddata is NULL\n", __func__);
		return -ENODATA;
	}

	if (!ddata->vib_ops->set_overdrive)
		return -ENOSYS;

	ret = ddata->vib_ops->set_overdrive(ddata->dev, en);
	if (ret)
		pr_err("%s error(%d)\n", __func__, ret);

	return ret;
}

static void sec_vibrator_haptic_enable(struct sec_vibrator_drvdata *ddata)
{
	if (!ddata) {
		pr_err("%s : ddata is NULL\n", __func__);
		return;
	}

#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
	sec_vibrator_check_temp(ddata);
#endif
	sec_vibrator_set_frequency(ddata, ddata->frequency);
	sec_vibrator_set_intensity(ddata, ddata->intensity);
	sec_vibrator_set_enable(ddata, true);

	if (ddata->vib_ops->set_frequency)
		pr_info("freq:%d, intensity:%d, %dms\n", ddata->frequency, ddata->intensity, ddata->timeout);
	else if (ddata->vib_ops->set_intensity)
		pr_info("intensity:%d, %dms\n", ddata->intensity, ddata->timeout);
	else
		pr_info("%dms\n", ddata->timeout);
}

static void sec_vibrator_haptic_disable(struct sec_vibrator_drvdata *ddata)
{
	if (!ddata) {
		pr_err("%s : ddata is NULL\n", __func__);
		return;
	}

	/* clear common variables */
	ddata->index = 0;

	/* clear haptic engine variables */
	ddata->f_packet_en = false;
	ddata->packet_cnt = 0;
	ddata->packet_size = 0;

	sec_vibrator_set_enable(ddata, false);
	sec_vibrator_set_overdrive(ddata, false);
	sec_vibrator_set_frequency(ddata, FREQ_ALERT);
	sec_vibrator_set_intensity(ddata, 0);

	if (ddata->timeout > 0)
		pr_info("timeout, off\n");
	else
		pr_info("off\n");
}

static void sec_vibrator_engine_run_packet(struct sec_vibrator_drvdata *ddata, struct vib_packet packet)
{
	int frequency = packet.freq;
	int intensity = packet.intensity;
	int overdrive = packet.overdrive;

	if (!ddata) {
		pr_err("%s : ddata is NULL\n", __func__);
		return;
	}

	if (!ddata->f_packet_en) {
		pr_err("haptic packet is empty\n");
		return;
	}

	sec_vibrator_set_overdrive(ddata, overdrive);
	sec_vibrator_set_frequency(ddata, frequency);
	if (intensity) {
		sec_vibrator_set_intensity(ddata, intensity);
		if (!ddata->packet_running) {
			pr_info("[haptic engine] motor run\n");
			sec_vibrator_set_enable(ddata, true);
		}
		ddata->packet_running = true;
	} else {
		if (ddata->packet_running) {
			pr_info("[haptic engine] motor stop\n");
			sec_vibrator_set_enable(ddata, false);
		}
		ddata->packet_running = false;
		sec_vibrator_set_intensity(ddata, intensity);
	}

	pr_info("%s [%d] freq:%d, intensity:%d, time:%d overdrive: %d\n",
		__func__, ddata->packet_cnt, frequency, intensity, ddata->timeout, overdrive);
}

static void timed_output_enable(struct sec_vibrator_drvdata *ddata, unsigned int value)
{
	struct hrtimer *timer = &ddata->timer;
	int ret = 0;

	if (!ddata) {
		pr_err("%s : ddata is NULL\n", __func__);
		return;
	}

	ret = hrtimer_cancel(timer);
	kthread_flush_worker(&ddata->kworker);

	mutex_lock(&ddata->vib_mutex);

	value = min_t(int, value, MAX_TIMEOUT);
	ddata->timeout = value;

	if (value) {
		if (ddata->f_packet_en) {
			ddata->packet_running = false;
			ddata->timeout = ddata->vib_pac[0].time;
			sec_vibrator_engine_run_packet(ddata, ddata->vib_pac[0]);
		} else {
			sec_vibrator_haptic_enable(ddata);
		}

		if (!ddata->index)
			hrtimer_start(timer, ns_to_ktime((u64)ddata->timeout * NSEC_PER_MSEC), HRTIMER_MODE_REL);
	} else {
		sec_vibrator_haptic_disable(ddata);
	}

	mutex_unlock(&ddata->vib_mutex);
}

#ifdef CONFIG_WAKE_GESTURES
void vib_trigger(int value)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;

	if (!ddata)
		pr_err("%s : ddata is NULL\n", __func__);

	timed_output_enable(ddata, value);
}
#endif

static enum hrtimer_restart haptic_timer_func(struct hrtimer *timer)
{
	struct sec_vibrator_drvdata *ddata;

	if (!timer) {
		pr_err("%s : timer is NULL\n", __func__);
		return -ENODATA;
	}

	ddata = container_of(timer, struct sec_vibrator_drvdata, timer);

	if (!ddata) {
		pr_err("%s : ddata is NULL\n", __func__);
		return -ENODATA;
	}

	pr_info("%s\n", __func__);
	kthread_queue_work(&ddata->kworker, &ddata->kwork);
	return HRTIMER_NORESTART;
}

static void sec_vibrator_work(struct kthread_work *work)
{
	struct sec_vibrator_drvdata *ddata;
	struct hrtimer *timer;

	if (!work) {
		pr_err("%s : work is NULL\n", __func__);
		return;
	}

	ddata = container_of(work, struct sec_vibrator_drvdata, kwork);

	if (!ddata) {
		pr_err("%s : ddata is NULL\n", __func__);
		return;
	}

	timer = &ddata->timer;

	if (!timer) {
		pr_err("%s : timer is NULL\n", __func__);
		return;
	}

	mutex_lock(&ddata->vib_mutex);

	if (ddata->f_packet_en) {
		ddata->packet_cnt++;
		if (ddata->packet_cnt < ddata->packet_size) {
			ddata->timeout = ddata->vib_pac[ddata->packet_cnt].time;
			sec_vibrator_engine_run_packet(ddata, ddata->vib_pac[ddata->packet_cnt]);
			hrtimer_start(timer, ns_to_ktime((u64)ddata->timeout * NSEC_PER_MSEC), HRTIMER_MODE_REL);
			goto unlock_without_vib_off;
		} else {
			ddata->f_packet_en = false;
			ddata->packet_cnt = 0;
			ddata->packet_size = 0;
		}
	}

	sec_vibrator_haptic_disable(ddata);

unlock_without_vib_off:
	mutex_unlock(&ddata->vib_mutex);
}

static inline bool is_valid_params(struct device *dev, struct device_attribute *attr,
	const char *buf, struct sec_vibrator_drvdata *ddata)
{
	if (!dev) {
		pr_err("%s : dev is NULL\n", __func__);
		return false;
	}
	if (!attr) {
		pr_err("%s : attr is NULL\n", __func__);
		return false;
	}
	if (!buf) {
		pr_err("%s : buf is NULL\n", __func__);
		return false;
	}
	if (!ddata) {
		pr_err("%s : ddata is NULL\n", __func__);
		return false;
	}
	return true;
}

static ssize_t intensity_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int intensity = 0, ret = 0;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	ret = kstrtoint(buf, 0, &intensity);
	if (ret) {
		pr_err("%s : fail to get intensity\n", __func__);
		return -EINVAL;
	}

	pr_info("%s %d\n", __func__, intensity);

	if ((intensity < 0) || (intensity > MAX_INTENSITY)) {
		pr_err("[VIB]: %s out of range\n", __func__);
		return -EINVAL;
	}

	ddata->intensity = intensity;

	return count;
}

static ssize_t intensity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;
	return snprintf(buf, VIB_BUFSIZE, "intensity: %u\n", ddata->intensity);
}

static ssize_t force_touch_intensity_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int intensity = 0, ret = 0;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	ret = kstrtoint(buf, 0, &intensity);
	if (ret) {
		pr_err("fail to get intensity\n");
		return -EINVAL;
	}

	pr_debug("%s %d\n", __func__, intensity);

	if ((intensity < 0) || (intensity > MAX_INTENSITY)) {
		pr_err("[VIB]: %s out of range\n", __func__);
		return -EINVAL;
	}

	ddata->force_touch_intensity = intensity;

	return count;
}
static ssize_t force_touch_intensity_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;
	return snprintf(buf, VIB_BUFSIZE,
		"force touch intensity: %u\n", ddata->force_touch_intensity);
}

static ssize_t multi_freq_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int num, ret;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	ret = kstrtoint(buf, 0, &num);
	if (ret) {
		pr_err("fail to get frequency\n");
		return -EINVAL;
	}

	pr_info("%s %d\n", __func__, num);

	ddata->frequency = num;

	return count;
}

static ssize_t multi_freq_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;
	return snprintf(buf, VIB_BUFSIZE, "frequency: %d\n", ddata->frequency);
}

// TODO: need to update
static ssize_t haptic_engine_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int index = 0, _data = 0, tmp = 0;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (sscanf(buf, "%6d", &_data) != 1)
		return count;

	if (_data > PACKET_MAX_SIZE * VIB_PACKET_MAX) {
		pr_info("%s, [%d] packet size over\n", __func__, _data);
		return count;
	}
	ddata->packet_size = _data / VIB_PACKET_MAX;
	ddata->packet_cnt = 0;
	ddata->f_packet_en = true;
	buf = strstr(buf, " ");

	for (index = 0; index < ddata->packet_size; index++) {
		for (tmp = 0; tmp < VIB_PACKET_MAX; tmp++) {
			if (buf == NULL) {
				pr_err("%s, buf is NULL, Please check packet data again\n", __func__);
				ddata->f_packet_en = false;
				return count;
			}

			if (sscanf(buf++, "%6d", &_data) != 1) {
				pr_err("%s, packet data error, Please check packet data again\n", __func__);
				ddata->f_packet_en = false;
				return count;
			}

			switch (tmp) {
			case VIB_PACKET_TIME:
				ddata->vib_pac[index].time = _data;
				break;
			case VIB_PACKET_INTENSITY:
				ddata->vib_pac[index].intensity = _data;
				break;
			case VIB_PACKET_FREQUENCY:
				ddata->vib_pac[index].freq = _data;
				break;
			case VIB_PACKET_OVERDRIVE:
				ddata->vib_pac[index].overdrive = _data;
				break;
			}
			buf = strstr(buf, " ");
		}
	}

	return count;
}

static ssize_t haptic_engine_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int index = 0;
	size_t size = 0;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	for (index = 0; index < ddata->packet_size && ddata->f_packet_en &&
	     ((4 * VIB_BUFSIZE + size) < PAGE_SIZE); index++) {
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u,", ddata->vib_pac[index].time);
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u,", ddata->vib_pac[index].intensity);
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u,", ddata->vib_pac[index].freq);
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u,", ddata->vib_pac[index].overdrive);
	}

	return size;
}

static ssize_t cp_trigger_index_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int ret = 0;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (!ddata->vib_ops->get_cp_trigger_index)
		return -ENOSYS;

	ret = ddata->vib_ops->get_cp_trigger_index(ddata->dev, buf);

	return ret;
}

static ssize_t cp_trigger_index_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int ret = 0;
	unsigned int index;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (!ddata->vib_ops->set_cp_trigger_index)
		return -ENOSYS;

	ret = kstrtou32(buf, 10, &index);
	if (ret)
		return -EINVAL;

	ddata->index = index;

	ret = ddata->vib_ops->set_cp_trigger_index(ddata->dev, buf);
	if (ret < 0)
		pr_err("%s error(%d)\n", __func__, ret);

	return count;
}

static ssize_t cp_trigger_queue_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int ret = 0;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (!ddata->vib_ops->get_cp_trigger_queue)
		return -ENOSYS;

	ret = ddata->vib_ops->get_cp_trigger_queue(ddata->dev, buf);
	if (ret < 0)
		pr_err("%s error(%d)\n", __func__, ret);

	return ret;
}

static ssize_t cp_trigger_queue_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int ret = 0;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (!ddata->vib_ops->set_cp_trigger_queue)
		return -ENOSYS;

	ret = ddata->vib_ops->set_cp_trigger_queue(ddata->dev, buf);
	if (ret < 0)
		pr_err("%s error(%d)\n", __func__, ret);

	return count;
}

static ssize_t pwle_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int ret = 0;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (!ddata->vib_ops->get_pwle)
		return -ENOSYS;

	ret = ddata->vib_ops->get_pwle(ddata->dev, buf);
	if (ret < 0)
		pr_err("%s error(%d)\n", __func__, ret);

	return ret;
}

static ssize_t pwle_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int ret = 0;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (!ddata->vib_ops->set_pwle)
		return -ENOSYS;

	ret = ddata->vib_ops->set_pwle(ddata->dev, buf);
	if (ret < 0)
		pr_err("%s error(%d)\n", __func__, ret);

	return count;
}

static ssize_t virtual_composite_indexes_show(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int ret = 0;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (!ddata->vib_ops->get_virtual_composite_indexes)
		return -ENOSYS;

	ret = ddata->vib_ops->get_virtual_composite_indexes(ddata->dev, buf);
	if (ret < 0)
		pr_err("%s error(%d)\n", __func__, ret);

	return ret;
}

static ssize_t virtual_pwle_indexes_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int ret = 0;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (!ddata->vib_ops->get_virtual_pwle_indexes)
		return -ENOSYS;

	ret = ddata->vib_ops->get_virtual_pwle_indexes(ddata->dev, buf);
	if (ret < 0)
		pr_err("%s error(%d)\n", __func__, ret);

	return ret;
}

static ssize_t enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	struct hrtimer *timer = &ddata->timer;
	int remaining = 0;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (hrtimer_active(timer)) {
		ktime_t remain = hrtimer_get_remaining(timer);
		struct timeval t = ktime_to_timeval(remain);

		remaining = t.tv_sec * 1000 + t.tv_usec / 1000;
	}
	return sprintf(buf, "%d\n", remaining);
}

static ssize_t enable_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t size)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int value;
	int ret;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	ret = kstrtoint(buf, 0, &value);
	if (ret != 0)
		return -EINVAL;

	timed_output_enable(ddata, value);
	return size;
}

static ssize_t motor_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int ret = 0;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (!ddata->vib_ops->get_motor_type)
		return snprintf(buf, VIB_BUFSIZE, "NONE\n");

	ret = ddata->vib_ops->get_motor_type(ddata->dev, buf);

	return ret;
}

static ssize_t use_sep_index_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t size)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int ret = 0;
	bool use_sep_index;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;
	
	ret = kstrtobool(buf, &use_sep_index);
	if (ret < 0) {
		pr_err("%s kstrtobool error : %d\n", __func__, ret);
		goto err;
	}
	
	pr_info("%s use_sep_index:%d\n", __func__, use_sep_index);
	
	if (ddata->vib_ops->set_use_sep_index) {
		ret = ddata->vib_ops->set_use_sep_index(ddata->dev, use_sep_index);
		if (ret) {
			pr_err("%s set_use_sep_index error : %d\n", __func__, ret);
			goto err;
		}
	} else {
		pr_info("%s this model doesn't need use_sep_index\n", __func__);
	}
err:
	return ret ? ret : size;
}

static ssize_t num_waves_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int ret = 0;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (!ddata->vib_ops->get_num_waves)
		return -ENOSYS;

	ret = ddata->vib_ops->get_num_waves(ddata->dev, buf);

	return ret;
}

static ssize_t array2str(char *buf, int *arr_intensity, int size)
{
	int index, ret = 0;
	char *str_buf = NULL;
	struct sec_vibrator_drvdata *ddata = g_ddata;

	if (!buf || !ddata)
		return -ENODATA;

	if (!arr_intensity || ((size < 1) && (size > kMaxHapticStepSize)))
		return -EINVAL;

	str_buf = kzalloc(kMaxBufSize, GFP_KERNEL);
	if (!str_buf)
		return -ENOMEM;

	mutex_lock(&ddata->vib_mutex);
	for (index = 0; index < size; index++) {
		if (index < (size - 1))
			snprintf(str_buf, kMaxBufSize, "%u,", arr_intensity[index]);
		else
			snprintf(str_buf, (kMaxBufSize - 1), "%u", arr_intensity[index]);
		strncat(buf, str_buf, strlen(str_buf));
	}
	strncat(buf, str_newline, strlen(str_newline));
	ret = strlen(buf);
	mutex_unlock(&ddata->vib_mutex);

	kfree(str_buf);
	return ret;
}

static ssize_t intensities_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int *arr_intensity = NULL;
	int ret = 0, step_size = 0;

	pr_info("%s\n", __func__);

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (!ddata->vib_ops->get_step_size || !ddata->vib_ops->get_intensities)
		return -EOPNOTSUPP;

	ret = ddata->vib_ops->get_step_size(ddata->dev, &step_size);
	if (ret)
		return -EINVAL;

	arr_intensity = kmalloc_array(kMaxHapticStepSize, sizeof(int), GFP_KERNEL);
	if (!arr_intensity)
		return -ENOMEM;

	if ((step_size > 0) && (step_size < kMaxHapticStepSize)) {
		ret = ddata->vib_ops->get_intensities(ddata->dev, arr_intensity);
		if (ret) {
			ret = -EINVAL;
			goto err_arr_alloc;
		}
		ret = array2str(buf, arr_intensity, step_size);
	} else {
		ret = -EINVAL;
	}

err_arr_alloc:
	kfree(arr_intensity);
	return ret;
}

static ssize_t haptic_intensities_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int *arr_intensity = NULL;
	int ret = 0, step_size = 0;

	pr_info("%s\n", __func__);

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (!ddata->vib_ops->get_step_size || !ddata->vib_ops->get_haptic_intensities)
		return -EOPNOTSUPP;

	ret = ddata->vib_ops->get_step_size(ddata->dev, &step_size);
	if (ret)
		return -EINVAL;

	arr_intensity = kmalloc_array(kMaxHapticStepSize, sizeof(int), GFP_KERNEL);
	if (!arr_intensity)
		return -ENOMEM;

	if ((step_size > 0) && (step_size < kMaxHapticStepSize)) {
		ret = ddata->vib_ops->get_haptic_intensities(ddata->dev, arr_intensity);
		if (ret) {
			ret = -EINVAL;
			goto err_arr_alloc;
		}
		ret = array2str(buf, arr_intensity, step_size);
	} else {
		ret = -EINVAL;
	}

err_arr_alloc:
	kfree(arr_intensity);
	return ret;
}

static ssize_t haptic_durations_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int *arr_duration = NULL;
	int ret = 0, step_size = 0;

	pr_info("%s\n", __func__);

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (!ddata->vib_ops->get_step_size || !ddata->vib_ops->get_haptic_durations)
		return -EOPNOTSUPP;

	ret = ddata->vib_ops->get_step_size(ddata->dev, &step_size);
	if (ret)
		return -EINVAL;

	arr_duration = kmalloc_array(kMaxHapticStepSize, sizeof(int), GFP_KERNEL);
	if (!arr_duration)
		return -ENOMEM;

	if ((step_size > 0) && (step_size < kMaxHapticStepSize)) {
		ret = ddata->vib_ops->get_haptic_durations(ddata->dev, arr_duration);
		if (ret) {
			ret = -EINVAL;
			goto err_arr_alloc;
		}
		ret = array2str(buf, arr_duration, step_size);
	} else {
		ret = -EINVAL;
	}

err_arr_alloc:
	kfree(arr_duration);
	return ret;
}

static int get_event_index_by_command(char *cur_cmd)
{
	int cmd_idx;

	for (cmd_idx = 0; cmd_idx < EVENT_CMD_MAX; cmd_idx++) {
		if (!strcmp(cur_cmd, sec_vib_event_cmd[cmd_idx]))
			return cmd_idx;
	}

	return EVENT_CMD_NONE;
}

static ssize_t event_cmd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;

	pr_info("%s event: %s\n", __func__, vib_event_cmd);

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	return snprintf(buf, MAX_STR_LEN_EVENT_CMD, "%s\n", vib_event_cmd);
}

static ssize_t event_cmd_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	char *cmd;
	int ret = 0;
	int event_idx;

	if (!is_valid_params(dev, attr, buf, ddata))
		return -ENODATA;

	if (!ddata->vib_ops->set_event_cmd)
		return -ENOSYS;

	if (count > MAX_STR_LEN_EVENT_CMD) {
		pr_err("%s: size(%zu) is too long.\n", __func__, count);
		goto error;
	}

	cmd = kzalloc(count + 1, GFP_KERNEL);
	if (!cmd)
		goto error;

	ret = sscanf(buf, "%s", cmd);
	if (ret != 1)
		goto error1;

	event_idx = get_event_index_by_command(cmd);

	pr_info("%s: event: %s(%d)\n", __func__, cmd, event_idx);

	ret = ddata->vib_ops->set_event_cmd(ddata->dev, event_idx);
	if (ret)
		pr_err("%s error(%d)\n", __func__, ret);

	sscanf(cmd, "%s", vib_event_cmd);

error1:
	kfree(cmd);
error:
	return count;
}

static DEVICE_ATTR_RW(haptic_engine);
static DEVICE_ATTR_RW(multi_freq);
static DEVICE_ATTR_RW(intensity);
static DEVICE_ATTR_RW(force_touch_intensity);
static DEVICE_ATTR_RW(cp_trigger_index);
static DEVICE_ATTR_RW(cp_trigger_queue);
static DEVICE_ATTR_RW(pwle);
static DEVICE_ATTR_RO(virtual_composite_indexes);
static DEVICE_ATTR_RO(virtual_pwle_indexes);
static DEVICE_ATTR_RW(enable);
static DEVICE_ATTR_RO(motor_type);
static DEVICE_ATTR_WO(use_sep_index);
static DEVICE_ATTR_RO(num_waves);
static DEVICE_ATTR_RO(intensities);
static DEVICE_ATTR_RO(haptic_intensities);
static DEVICE_ATTR_RO(haptic_durations);
static DEVICE_ATTR_RW(event_cmd);

static struct attribute *sec_vibrator_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_motor_type.attr,
	&dev_attr_use_sep_index.attr,
	NULL,
};

static struct attribute_group sec_vibrator_attr_group = {
	.attrs = sec_vibrator_attributes,
};

static struct attribute *multi_freq_attributes[] = {
	&dev_attr_haptic_engine.attr,
	&dev_attr_multi_freq.attr,
	NULL,
};

static struct attribute_group multi_freq_attr_group = {
	.attrs = multi_freq_attributes,
};

static struct attribute *cp_trigger_attributes[] = {
	&dev_attr_haptic_engine.attr,
	&dev_attr_num_waves.attr,
	&dev_attr_cp_trigger_index.attr,
	&dev_attr_cp_trigger_queue.attr,
	NULL,
};

static struct attribute_group cp_trigger_attr_group = {
	.attrs = cp_trigger_attributes,
};

static struct attribute *pwle_attributes[] = {
	&dev_attr_pwle.attr,
	&dev_attr_virtual_composite_indexes.attr,
	&dev_attr_virtual_pwle_indexes.attr,
	NULL,
};

static struct attribute_group pwle_attr_group = {
	.attrs = pwle_attributes,
};

int sec_vibrator_register(struct sec_vibrator_drvdata *ddata)
{
	struct task_struct *kworker_task;
	int ret = 0;

	if (!ddata) {
		pr_err("%s no ddata\n", __func__);
		return -ENODEV;
	}

	g_ddata = ddata;

	mutex_init(&ddata->vib_mutex);
	kthread_init_worker(&ddata->kworker);
	kworker_task = kthread_run(kthread_worker_fn, &ddata->kworker, "sec_vibrator");

	if (IS_ERR(kworker_task)) {
		pr_err("Failed to create message pump task\n");
		ret = -ENOMEM;
		goto err_kthread;
	}

	kthread_init_work(&ddata->kwork, sec_vibrator_work);
	hrtimer_init(&ddata->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ddata->timer.function = haptic_timer_func;

	/* create /sys/class/timed_output/vibrator */
	ddata->to_class = class_create(THIS_MODULE, "timed_output");
	if (IS_ERR(ddata->to_class)) {
		ret = PTR_ERR(ddata->to_class);
		goto err_class_create;
	}
	ddata->to_dev = device_create(ddata->to_class, NULL, MKDEV(0, 0), ddata, "vibrator");
	if (IS_ERR(ddata->to_dev)) {
		ret = PTR_ERR(ddata->to_dev);
		goto err_device_create;
	}

	ret = sysfs_create_group(&ddata->to_dev->kobj, &sec_vibrator_attr_group);
	if (ret) {
		ret = -ENODEV;
		pr_err("Failed to create sysfs1 %d\n", ret);
		goto err_sysfs1;
	}

	if (ddata->vib_ops->set_intensity) {
		ret = sysfs_create_file(&ddata->to_dev->kobj, &dev_attr_intensity.attr);
		if (ret) {
			ret = -ENODEV;
			pr_err("Failed to create sysfs2 %d\n", ret);
			goto err_sysfs2;
		}

		ddata->intensity = MAX_INTENSITY;

		ret = sysfs_create_file(&ddata->to_dev->kobj, &dev_attr_force_touch_intensity.attr);
		if (ret) {
			ret = -ENODEV;
			pr_err("Failed to create sysfs3 %d\n", ret);
			goto err_sysfs3;
		}

		ddata->force_touch_intensity = MAX_INTENSITY;
	}

	if (ddata->vib_ops->set_frequency) {
		ret = sysfs_create_group(&ddata->to_dev->kobj, &multi_freq_attr_group);
		if (ret) {
			ret = -ENODEV;
			pr_err("Failed to create sysfs4 %d\n", ret);
			goto err_sysfs4;
		}

		ddata->frequency = FREQ_ALERT;
	}

	if (ddata->vib_ops->set_cp_trigger_index) {
		ret = sysfs_create_group(&ddata->to_dev->kobj, &cp_trigger_attr_group);
		if (ret) {
			ret = -ENODEV;
			pr_err("Failed to create sysfs5 %d\n", ret);
			goto err_sysfs5;
		}

	}

	if (ddata->vib_ops->set_pwle) {
		ret = sysfs_create_group(&ddata->to_dev->kobj, &pwle_attr_group);
		if (ret) {
			ret = -ENODEV;
			pr_err("Failed to create sysfs6 %d\n", ret);
			goto err_sysfs6;
		}

	}

	if (ddata->vib_ops->set_event_cmd) {
		/* initialize event_cmd string for HAL init */
		sscanf(ddata->event_cmd, "%s", vib_event_cmd);
		pr_info("%s: vib_event_cmd: %s\n", __func__, vib_event_cmd);

		ret = sysfs_create_file(&ddata->to_dev->kobj, &dev_attr_event_cmd.attr);
		if (ret) {
			ret = -ENODEV;
			pr_err("Failed to create sysfs7 %d\n", ret);
			goto err_sysfs7;
		}
	}

	if (ddata->vib_ops->get_calibration && ddata->vib_ops->get_calibration(ddata->dev)) {
		if (ddata->vib_ops->get_intensities) {
			ret = sysfs_create_file(&ddata->to_dev->kobj, &dev_attr_intensities.attr);
			if (ret) {
				ret = -ENODEV;
				pr_err("Failed to create intensities %d\n", ret);
				goto err_cal1;
			}
		}

		if (ddata->vib_ops->get_haptic_intensities) {
			ret = sysfs_create_file(&ddata->to_dev->kobj, &dev_attr_haptic_intensities.attr);
			if (ret) {
				ret = -ENODEV;
				pr_err("Failed to create haptic_intensities %d\n", ret);
				goto err_cal2;
			}
		}

		if (ddata->vib_ops->get_haptic_durations) {
			ret = sysfs_create_file(&ddata->to_dev->kobj, &dev_attr_haptic_durations.attr);
			if (ret) {
				ret = -ENODEV;
				pr_err("Failed to create haptic_intensities %d\n", ret);
				goto err_cal3;
			}
		}
	}

	pr_info("%s done\n", __func__);

	ddata->is_registered = true;

	return ret;

err_cal3:
	if (ddata->vib_ops->get_calibration && ddata->vib_ops->get_calibration(ddata->dev)) {
		if (ddata->vib_ops->get_haptic_intensities)
			sysfs_remove_file(&ddata->to_dev->kobj, &dev_attr_haptic_intensities.attr);
	}
err_cal2:
	if (ddata->vib_ops->get_calibration && ddata->vib_ops->get_calibration(ddata->dev)) {
		if (ddata->vib_ops->get_intensities)
			sysfs_remove_file(&ddata->to_dev->kobj, &dev_attr_intensities.attr);
	}
err_cal1:
	if (ddata->vib_ops->set_event_cmd)
		sysfs_remove_file(&ddata->to_dev->kobj, &dev_attr_event_cmd.attr);
err_sysfs7:
	if (ddata->vib_ops->set_pwle)
		sysfs_remove_group(&ddata->to_dev->kobj, &pwle_attr_group);
err_sysfs6:
	if (ddata->vib_ops->set_cp_trigger_index)
		sysfs_remove_group(&ddata->to_dev->kobj, &cp_trigger_attr_group);
err_sysfs5:
	if (ddata->vib_ops->set_frequency)
		sysfs_remove_group(&ddata->to_dev->kobj, &multi_freq_attr_group);
err_sysfs4:
	if (ddata->vib_ops->set_intensity)
		sysfs_remove_file(&ddata->to_dev->kobj, &dev_attr_force_touch_intensity.attr);
err_sysfs3:
	if (ddata->vib_ops->set_intensity)
		sysfs_remove_file(&ddata->to_dev->kobj, &dev_attr_intensity.attr);
err_sysfs2:
	sysfs_remove_group(&ddata->to_dev->kobj, &sec_vibrator_attr_group);
err_sysfs1:
	device_destroy(ddata->to_class, MKDEV(0, 0));
err_device_create:
	class_destroy(ddata->to_class);
err_class_create:
err_kthread:
	mutex_destroy(&ddata->vib_mutex);
	return ret;
}
EXPORT_SYMBOL_GPL(sec_vibrator_register);

int sec_vibrator_unregister(struct sec_vibrator_drvdata *ddata)
{
	if (!ddata || !ddata->is_registered) {
		pr_info("%s: sec_vibrator not registered, just return\n", __func__);
		return -ENODEV;
	}

	sec_vibrator_haptic_disable(ddata);
	g_ddata = NULL;

	if (ddata->vib_ops->get_calibration && ddata->vib_ops->get_calibration(ddata->dev)) {
		if (ddata->vib_ops->get_haptic_intensities)
			sysfs_remove_file(&ddata->to_dev->kobj, &dev_attr_haptic_intensities.attr);
		if (ddata->vib_ops->get_intensities)
			sysfs_remove_file(&ddata->to_dev->kobj, &dev_attr_intensities.attr);
	}
	if (ddata->vib_ops->set_event_cmd)
		sysfs_remove_file(&ddata->to_dev->kobj, &dev_attr_event_cmd.attr);
	if (ddata->vib_ops->set_pwle)
		sysfs_remove_group(&ddata->to_dev->kobj, &pwle_attr_group);
	if (ddata->vib_ops->set_cp_trigger_index)
		sysfs_remove_group(&ddata->to_dev->kobj, &cp_trigger_attr_group);
	if (ddata->vib_ops->set_frequency)
		sysfs_remove_group(&ddata->to_dev->kobj, &multi_freq_attr_group);
	if (ddata->vib_ops->set_intensity)
		sysfs_remove_file(&ddata->to_dev->kobj, &dev_attr_intensity.attr);

	sysfs_remove_group(&ddata->to_dev->kobj, &sec_vibrator_attr_group);

	device_destroy(ddata->to_class, MKDEV(0, 0));
	class_destroy(ddata->to_class);
	mutex_destroy(&ddata->vib_mutex);
	return 0;
}
EXPORT_SYMBOL_GPL(sec_vibrator_unregister);

extern int haptic_homekey_press(void)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	struct hrtimer *timer;
	int intensity;

	if (!ddata) {
		pr_err("%s : ddata is NULL\n", __func__);
		return -ENODATA;
	}

	timer = &ddata->timer;

	intensity = ddata->force_touch_intensity;
	if (intensity > HOMEKEY_PRESS_FREQ) {
		mutex_lock(&ddata->vib_mutex);
		ddata->timeout = HOMEKEY_DURATION;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		sec_vibrator_check_temp(ddata);
#endif
		sec_vibrator_set_overdrive(ddata, true);
		sec_vibrator_set_frequency(ddata, HOMEKEY_PRESS_FREQ);
		sec_vibrator_set_intensity(ddata, intensity);
		sec_vibrator_set_enable(ddata, true);

		pr_debug("%s freq:%d, intensity:%d, time:%d\n", __func__,
				HOMEKEY_PRESS_FREQ, intensity, ddata->timeout);
		mutex_unlock(&ddata->vib_mutex);

		hrtimer_start(timer,
			ns_to_ktime((u64)ddata->timeout * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	}

	return 0;
}

extern int haptic_homekey_release(void)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	struct hrtimer *timer;
	int intensity;

	if (!ddata) {
		pr_err("%s : ddata is NULL\n", __func__);
		return -ENODATA;
	}

	timer = &ddata->timer;

	intensity = ddata->force_touch_intensity;
	if (intensity > HOMEKEY_PRESS_FREQ) {
		mutex_lock(&ddata->vib_mutex);
		ddata->timeout = HOMEKEY_DURATION;
#if IS_ENABLED(CONFIG_BATTERY_SAMSUNG)
		sec_vibrator_check_temp(ddata);
#endif
		sec_vibrator_set_overdrive(ddata, true);
		sec_vibrator_set_frequency(ddata, HOMEKEY_RELEASE_FREQ);
		sec_vibrator_set_intensity(ddata, intensity);
		sec_vibrator_set_enable(ddata, true);

		pr_debug("%s freq:%d, intensity:%d, time:%d\n", __func__,
				HOMEKEY_RELEASE_FREQ, intensity, ddata->timeout);
		mutex_unlock(&ddata->vib_mutex);

		hrtimer_start(timer, ns_to_ktime((u64)ddata->timeout * NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}

	return 0;
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sec vibrator driver");