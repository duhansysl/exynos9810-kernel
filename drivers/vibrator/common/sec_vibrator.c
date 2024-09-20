/*
 * sec vibrator driver for common code
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "[VIB] " fmt
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vibrator/sec_vibrator.h>
#if defined(CONFIG_SSP_MOTOR_CALLBACK)
#include <linux/ssp_motorcallback.h>
#endif

static struct sec_vibrator_drvdata *g_ddata;

static int sec_vibrator_set_frequency(struct sec_vibrator_drvdata *ddata,
	int frequency)
{
	int ret = 0;

	if (!ddata->vib_ops->set_frequency)
		return -ENOSYS;

	if (frequency < FREQ_ALERT ||
			(frequency >= FREQ_MAX && frequency < HAPTIC_ENGINE_FREQ_MIN) ||
			frequency > HAPTIC_ENGINE_FREQ_MAX) {
		pr_err("%s out of range(%d)\n", __func__, frequency);
		return -EINVAL;
	}

	ret = ddata->vib_ops->set_frequency(ddata->dev, frequency);

	if (ret)
		pr_err("%s error(%d)\n", __func__, ret);

	return ret;
}

static int sec_vibrator_set_intensity(struct sec_vibrator_drvdata *ddata,
	int intensity)
{
	int ret = 0;

	if (!ddata->vib_ops->set_intensity)
		return -ENOSYS;

	if (intensity < -(MAX_INTENSITY) || MAX_INTENSITY < intensity) {
		pr_err("%s out of range(%d)\n", __func__, intensity);
		return -EINVAL;
	}

	ret = ddata->vib_ops->set_intensity(ddata->dev, intensity);

	if (ret)
		pr_err("%s error(%d)\n", __func__, ret);

	return ret;
}

static int sec_vibrator_set_force_touch_intensity(
		struct sec_vibrator_drvdata *ddata, int intensity)
{
	int ret = 0;

	if (!ddata->vib_ops->set_force_touch_intensity)
		return -ENOSYS;

	if (intensity < -(MAX_INTENSITY) || MAX_INTENSITY < intensity) {
		pr_err("%s out of range(%d)\n", __func__, intensity);
		return -EINVAL;
	}

	ret = ddata->vib_ops->set_force_touch_intensity(ddata->dev, intensity);

	if (ret)
		pr_err("%s error(%d)\n", __func__, ret);

	return ret;
}

static int sec_vibrator_set_enable(struct sec_vibrator_drvdata *ddata, bool en)
{
	int ret = 0;

	if (!ddata->vib_ops->enable) {
		pr_err("%s cannot supported\n", __func__);
		return -ENOSYS;
	}

	ret = ddata->vib_ops->enable(ddata->dev, en);
	if (ret)
		pr_err("%s error(%d)\n", __func__, ret);

#if defined(CONFIG_SSP_MOTOR_CALLBACK)
	if (en && ddata->intensity > 0)
		setSensorCallback(true, ddata->timeout);
	else
		setSensorCallback(false, 0);
#endif

	return ret;
}

static int sec_vibrator_set_overdrive(struct sec_vibrator_drvdata *ddata,
		bool en)
{
	int ret = 0;

	if (!ddata->vib_ops->set_overdrive)
		return -ENOSYS;

	ret = ddata->vib_ops->set_overdrive(ddata->dev, en);
	if (ret)
		pr_err("%s error(%d)\n", __func__, ret);

	return ret;
}

static void sec_vibrator_haptic_enable(struct sec_vibrator_drvdata *ddata)
{
	sec_vibrator_set_overdrive(ddata, false);
	sec_vibrator_set_frequency(ddata, ddata->frequency);
	sec_vibrator_set_intensity(ddata, ddata->intensity);
	sec_vibrator_set_enable(ddata, true);

	if (ddata->vib_ops->set_frequency)
		pr_info("freq:%d, intensity:%d, %dms\n", ddata->frequency,
				ddata->intensity, ddata->timeout);
	else if (ddata->vib_ops->set_intensity)
		pr_info("intensity:%d, %dms\n", ddata->intensity,
				ddata->timeout);
	else
		pr_info("%dms\n", ddata->timeout);
}

static void sec_vibrator_haptic_disable(struct sec_vibrator_drvdata *ddata)
{
	/* clear haptic engine variables */
	ddata->f_packet_en = false;
	ddata->packet_cnt = 0;
	ddata->packet_size = 0;

	/* clear led trigger variables */
	ddata->state = 0;
	ddata->duration = 0;

	sec_vibrator_set_enable(ddata, false);
	sec_vibrator_set_overdrive(ddata, false);
	sec_vibrator_set_frequency(ddata, FREQ_ALERT);
	sec_vibrator_set_intensity(ddata, 0);

	if (ddata->timeout > 0)
		pr_info("timeout, off\n");
	else
		pr_info("off\n");
}

static void sec_vibrator_engine_run_packet(struct sec_vibrator_drvdata *ddata,
		struct vib_packet packet)
{
	int frequency = packet.freq;
	int intensity = packet.intensity;
	int overdrive = packet.overdrive;

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
			__func__, ddata->packet_cnt, frequency, intensity,
			ddata->timeout, overdrive);
}

static void timed_output_enable(struct sec_vibrator_drvdata *ddata, unsigned int value)
{
	struct hrtimer *timer = &ddata->timer;
	int ret = 0;

	kthread_flush_worker(&ddata->kworker);
	ret = hrtimer_cancel(timer);

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

		hrtimer_start(timer,
			ns_to_ktime((u64)ddata->timeout * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	} else {
		sec_vibrator_haptic_disable(ddata);
	}

	mutex_unlock(&ddata->vib_mutex);
}

static enum hrtimer_restart haptic_timer_func(struct hrtimer *timer)
{
	struct sec_vibrator_drvdata *ddata
		= container_of(timer, struct sec_vibrator_drvdata, timer);

	pr_info("%s\n", __func__);
	kthread_queue_work(&ddata->kworker, &ddata->kwork);
	return HRTIMER_NORESTART;
}

static void sec_vibrator_work(struct kthread_work *work)
{
	struct sec_vibrator_drvdata *ddata
		= container_of(work, struct sec_vibrator_drvdata, kwork);
	struct hrtimer *timer = &ddata->timer;

	mutex_lock(&ddata->vib_mutex);

	if (ddata->f_packet_en) {
		ddata->packet_cnt++;
		if (ddata->packet_cnt < ddata->packet_size) {
			ddata->timeout = ddata->vib_pac[ddata->packet_cnt].time;
			sec_vibrator_engine_run_packet(ddata,
					ddata->vib_pac[ddata->packet_cnt]);
			hrtimer_start(timer, ns_to_ktime((u64)ddata->timeout * NSEC_PER_MSEC),
					HRTIMER_MODE_REL);

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

static ssize_t intensity_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int intensity = 0, ret = 0;

	ret = kstrtoint(buf, 0, &intensity);
	if (ret) {
		pr_err("fail to get intensity\n");
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

static ssize_t intensity_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;

	return snprintf(buf, VIB_BUFSIZE,
		"intensity: %u\n", ddata->intensity);
}

static ssize_t force_touch_intensity_store(struct device *dev,
		struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int intensity = 0, ret = 0;

	ret = kstrtoint(buf, 0, &intensity);
	if (ret) {
		pr_err("fail to get intensity\n");
		return -EINVAL;
	}

	pr_info("%s %d\n", __func__, intensity);

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

	return snprintf(buf, VIB_BUFSIZE,
		"force touch intensity: %u\n", ddata->force_touch_intensity);
}

static ssize_t multi_freq_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int num, ret;

	ret = kstrtoint(buf, 0, &num);
	if (ret) {
		pr_err("fail to get frequency\n");
		return -EINVAL;
	}

	pr_info("%s %d\n", __func__, num);

	ddata->frequency = num;

	return count;
}

static ssize_t multi_freq_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;

	return snprintf(buf, VIB_BUFSIZE,
		"frequency: %d\n", ddata->frequency);
}

// TODO: need to update
static ssize_t haptic_engine_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int i = 0, _data = 0, tmp = 0;

	if (sscanf(buf, "%6d", &_data) != 1)
		return count;

	if (_data > PACKET_MAX_SIZE * VIB_PACKET_MAX)
		pr_info("%s, [%d] packet size over\n", __func__, _data);
	else {
		ddata->packet_size = _data / VIB_PACKET_MAX;
		ddata->packet_cnt = 0;
		ddata->f_packet_en = true;

		buf = strstr(buf, " ");

		for (i = 0; i < ddata->packet_size; i++) {
			for (tmp = 0; tmp < VIB_PACKET_MAX; tmp++) {
				if (buf == NULL) {
					pr_err("%s, buf is NULL, Please check packet data again\n",
							__func__);
					ddata->f_packet_en = false;
					return count;
				}

				if (sscanf(buf++, "%6d", &_data) != 1) {
					pr_err("%s, packet data error, Please check packet data again\n",
							__func__);
					ddata->f_packet_en = false;
					return count;
				}

				switch (tmp) {
				case VIB_PACKET_TIME:
					ddata->vib_pac[i].time = _data;
					break;
				case VIB_PACKET_INTENSITY:
					ddata->vib_pac[i].intensity = _data;
					break;
				case VIB_PACKET_FREQUENCY:
					ddata->vib_pac[i].freq = _data;
					break;
				case VIB_PACKET_OVERDRIVE:
					ddata->vib_pac[i].overdrive = _data;
					break;
				}
				buf = strstr(buf, " ");
			}
		}
	}

	return count;
}

static ssize_t haptic_engine_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int i = 0;
	size_t size = 0;

	for (i = 0; i < ddata->packet_size && ddata->f_packet_en &&
			((4 * VIB_BUFSIZE + size) < PAGE_SIZE); i++) {
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u,", ddata->vib_pac[i].time);
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u,", ddata->vib_pac[i].intensity);
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u,", ddata->vib_pac[i].freq);
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u,", ddata->vib_pac[i].overdrive);
	}

	return size;
}

#if 0
static ssize_t cs40l2x_cp_trigger_index_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct cs40l2x_private *cs40l2x = cs40l2x_get_private(dev);
	unsigned int index;

	mutex_lock(&cs40l2x->lock);
	index = cs40l2x->cp_trigger_index;
	mutex_unlock(&cs40l2x->lock);

	return snprintf(buf, PAGE_SIZE, "%d\n", index);
}

static ssize_t cs40l2x_cp_trigger_index_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct cs40l2x_private *cs40l2x = cs40l2x_get_private(dev);
	int ret;
	unsigned int index;

	ret = kstrtou32(buf, 10, &index);
	if (ret)
		return -EINVAL;

#ifdef CONFIG_CS40L2X_SAMSUNG_FEATURE
	pr_info("%s index:%u (num_waves:%u)\n", __func__, index, cs40l2x->num_waves);
#endif

	mutex_lock(&cs40l2x->lock);

	switch (index) {
	case CS40L2X_INDEX_QEST:
		if (cs40l2x->fw_desc->id == CS40L2X_FW_ID_ORIG) {
			ret = -EPERM;
			break;
		}
		/* intentionally fall through */
	case CS40L2X_INDEX_DIAG:
		if (cs40l2x->fw_desc->id == cs40l2x->fw_id_remap)
			ret = cs40l2x_firmware_swap(cs40l2x,
					CS40L2X_FW_ID_CAL);
		break;
	case CS40L2X_INDEX_PEAK:
	case CS40L2X_INDEX_PBQ:
		if (cs40l2x->fw_desc->id == CS40L2X_FW_ID_CAL)
			ret = cs40l2x_firmware_swap(cs40l2x,
					cs40l2x->fw_id_remap);
		break;
	case CS40L2X_INDEX_IDLE:
		ret = -EINVAL;
		break;
	default:
		if ((index & CS40L2X_INDEX_MASK) >= cs40l2x->num_waves) {
			ret = -EINVAL;
			break;
		}

		if (cs40l2x->fw_desc->id == CS40L2X_FW_ID_CAL)
			ret = cs40l2x_firmware_swap(cs40l2x,
					cs40l2x->fw_id_remap);
	}
	if (ret)
		goto err_mutex;

	cs40l2x->cp_trigger_index = index;
	ret = count;

err_mutex:
	mutex_unlock(&cs40l2x->lock);

	return ret;
}

static ssize_t cs40l2x_cp_trigger_queue_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct cs40l2x_private *cs40l2x = cs40l2x_get_private(dev);
	ssize_t len = 0;
	int i;

	if (cs40l2x->pbq_depth == 0)
		return -ENODATA;

	mutex_lock(&cs40l2x->lock);

	for (i = 0; i < cs40l2x->pbq_depth; i++) {
		switch (cs40l2x->pbq_pairs[i].tag) {
		case CS40L2X_PBQ_TAG_SILENCE:
			len += snprintf(buf + len, PAGE_SIZE - len, "%d",
					cs40l2x->pbq_pairs[i].mag);
			break;
		case CS40L2X_PBQ_TAG_START:
			len += snprintf(buf + len, PAGE_SIZE - len, "!!");
			break;
		case CS40L2X_PBQ_TAG_STOP:
			len += snprintf(buf + len, PAGE_SIZE - len, "%d!!",
					cs40l2x->pbq_pairs[i].repeat);
			break;
		default:
			len += snprintf(buf + len, PAGE_SIZE - len, "%d.%d",
					cs40l2x->pbq_pairs[i].tag,
					cs40l2x->pbq_pairs[i].mag);
		}

		if (i < (cs40l2x->pbq_depth - 1))
			len += snprintf(buf + len, PAGE_SIZE - len, ", ");
	}

	switch (cs40l2x->pbq_repeat) {
	case -1:
		len += snprintf(buf + len, PAGE_SIZE - len, ", ~\n");
		break;
	case 0:
		len += snprintf(buf + len, PAGE_SIZE - len, "\n");
		break;
	default:
		len += snprintf(buf + len, PAGE_SIZE - len, ", %d!\n",
				cs40l2x->pbq_repeat);
	}

	mutex_unlock(&cs40l2x->lock);

	return len;
}

static ssize_t cs40l2x_cp_trigger_queue_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct cs40l2x_private *cs40l2x = cs40l2x_get_private(dev);
	char *pbq_str_alloc, *pbq_str, *pbq_str_tok;
	char *pbq_seg_alloc, *pbq_seg, *pbq_seg_tok;
	size_t pbq_seg_len;
	unsigned int pbq_depth = 0;
	unsigned int val, num_empty;
	int pbq_marker = -1;
	int ret;

	pbq_str_alloc = kzalloc(count, GFP_KERNEL);
	if (!pbq_str_alloc)
		return -ENOMEM;

	pbq_seg_alloc = kzalloc(CS40L2X_PBQ_SEG_LEN_MAX + 1, GFP_KERNEL);
	if (!pbq_seg_alloc) {
		kfree(pbq_str_alloc);
		return -ENOMEM;
	}

	mutex_lock(&cs40l2x->lock);

	cs40l2x->pbq_depth = 0;
	cs40l2x->pbq_repeat = 0;

	pbq_str = pbq_str_alloc;
	strlcpy(pbq_str, buf, count);

#if DEBUG_VIB
	pr_info("%s pbq_str=%s\n", __func__, pbq_str);
#endif

	pbq_str_tok = strsep(&pbq_str, ",");

	while (pbq_str_tok) {
		pbq_seg = pbq_seg_alloc;
		pbq_seg_len = strlcpy(pbq_seg, strim(pbq_str_tok),
				CS40L2X_PBQ_SEG_LEN_MAX + 1);
		if (pbq_seg_len > CS40L2X_PBQ_SEG_LEN_MAX) {
			ret = -E2BIG;
			goto err_mutex;
		}

		/* waveform specifier */
		if (strnchr(pbq_seg, CS40L2X_PBQ_SEG_LEN_MAX, '.')) {
			/* index */
			pbq_seg_tok = strsep(&pbq_seg, ".");

			ret = kstrtou32(pbq_seg_tok, 10, &val);
			if (ret) {
				ret = -EINVAL;
				goto err_mutex;
			}
			if (val == 0 || val >= cs40l2x->num_waves) {
				ret = -EINVAL;
				goto err_mutex;
			}
			cs40l2x->pbq_pairs[pbq_depth].tag = val;

			/* scale */
			pbq_seg_tok = strsep(&pbq_seg, ".");

			ret = kstrtou32(pbq_seg_tok, 10, &val);
			if (ret) {
				ret = -EINVAL;
				goto err_mutex;
			}
			if (val == 0 || val > CS40L2X_PBQ_SCALE_MAX) {
				ret = -EINVAL;
				goto err_mutex;
			}
			cs40l2x->pbq_pairs[pbq_depth++].mag = val;

		/* repetition specifier */
		} else if (strnchr(pbq_seg, CS40L2X_PBQ_SEG_LEN_MAX, '!')) {
			val = 0;
			num_empty = 0;

			pbq_seg_tok = strsep(&pbq_seg, "!");

			while (pbq_seg_tok) {
				if (strnlen(pbq_seg_tok,
						CS40L2X_PBQ_SEG_LEN_MAX)) {
					ret = kstrtou32(pbq_seg_tok, 10, &val);
					if (ret) {
						ret = -EINVAL;
						goto err_mutex;
					}
					if (val > CS40L2X_PBQ_REPEAT_MAX) {
						ret = -EINVAL;
						goto err_mutex;
					}
				} else {
					num_empty++;
				}

				pbq_seg_tok = strsep(&pbq_seg, "!");
			}

			/* number of empty tokens reveals specifier type */
			switch (num_empty) {
			case 1:	/* outer loop: "n!" or "!n" */
				if (cs40l2x->pbq_repeat) {
					ret = -EINVAL;
					goto err_mutex;
				}
				cs40l2x->pbq_repeat = val;
				break;

			case 2:	/* inner loop stop: "n!!" or "!!n" */
				if (pbq_marker < 0) {
					ret = -EINVAL;
					goto err_mutex;
				}

				cs40l2x->pbq_pairs[pbq_depth].tag =
						CS40L2X_PBQ_TAG_STOP;
				cs40l2x->pbq_pairs[pbq_depth].mag = pbq_marker;
				cs40l2x->pbq_pairs[pbq_depth++].repeat = val;
				pbq_marker = -1;
				break;

			case 3:	/* inner loop start: "!!" */
				if (pbq_marker >= 0) {
					ret = -EINVAL;
					goto err_mutex;
				}

				cs40l2x->pbq_pairs[pbq_depth].tag =
						CS40L2X_PBQ_TAG_START;
				pbq_marker = pbq_depth++;
				break;

			default:
				ret = -EINVAL;
				goto err_mutex;
			}

		/* loop specifier */
		} else if (strnchr(pbq_seg, CS40L2X_PBQ_SEG_LEN_MAX, '~')) {
			if (cs40l2x->pbq_repeat) {
				ret = -EINVAL;
				goto err_mutex;
			}
			cs40l2x->pbq_repeat = -1;

		/* duration specifier */
		} else {
			cs40l2x->pbq_pairs[pbq_depth].tag =
					CS40L2X_PBQ_TAG_SILENCE;

			ret = kstrtou32(pbq_seg, 10, &val);
			if (ret) {
				ret = -EINVAL;
				goto err_mutex;
			}
			if (val > CS40L2X_PBQ_DELAY_MAX) {
				ret = -EINVAL;
				goto err_mutex;
			}
			cs40l2x->pbq_pairs[pbq_depth++].mag = val;
		}

		if (pbq_depth == CS40L2X_PBQ_DEPTH_MAX) {
			ret = -E2BIG;
			goto err_mutex;
		}

		pbq_str_tok = strsep(&pbq_str, ",");
	}

#ifdef CONFIG_CS40L2X_SAMSUNG_FEATURE
	if(count > DEBUG_PRINT_PLAYBACK_QUEUE) {
		strlcpy(pbq_str_alloc, buf, DEBUG_PRINT_PLAYBACK_QUEUE);
		pr_info("%s pbq_str=%s... (depth:%d)\n", __func__, pbq_str_alloc, pbq_depth);
	} else {
		strlcpy(pbq_str_alloc, buf, count);
		pr_info("%s pbq_str=%s (depth:%d)\n", __func__, pbq_str_alloc, pbq_depth);
	}
#endif
	cs40l2x->pbq_depth = pbq_depth;
	ret = count;

err_mutex:
	mutex_unlock(&cs40l2x->lock);

	kfree(pbq_str_alloc);
	kfree(pbq_seg_alloc);

	return ret;
}
#endif

static ssize_t enable_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	struct hrtimer *timer = &ddata->timer;
	int remaining = 0;

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

	ret = kstrtoint(buf, 0, &value);
	if (ret != 0)
		return -EINVAL;

	timed_output_enable(ddata, value);
	return size;
}

static ssize_t motor_type_show(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int ret = 0;

	if (!ddata->vib_ops->get_motor_type)
		return snprintf(buf, VIB_BUFSIZE, "NONE\n");

	ret = ddata->vib_ops->get_motor_type(ddata->dev, buf);

	return ret;
}

static ssize_t motor_type_store(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t size)
{
	/* do nothing */
	return 0;
}

static DEVICE_ATTR_RW(haptic_engine);
static DEVICE_ATTR_RW(multi_freq);
static DEVICE_ATTR_RW(intensity);
static DEVICE_ATTR_RW(force_touch_intensity);
#if 0
static DEVICE_ATTR_RW(cs40l2x_cp_trigger_index);
static DEVICE_ATTR_RW(cs40l2x_cp_trigger_queue);
#endif
static DEVICE_ATTR_RW(enable);
static DEVICE_ATTR_RW(motor_type);

static struct attribute *sec_vibrator_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_motor_type.attr,
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

#if 0
static struct attribute *cp_trigger_attributes[] = {
	&dev_attr_cs40l2x_cp_trigger_index,
	&dev_attr_cs40l2x_cp_trigger_queue,
	NULL,
};

static struct attribute_group cp_trigger_attr_group = {
	.attrs = cp_trigger_attributes,
};
#endif

static ssize_t state_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;

	return sprintf(buf, "%d\n", ddata->state);
}

static ssize_t state_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int value;
	int ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret != 0)
		return -EINVAL;

	ddata->state = value;
	return size;
}

static ssize_t duration_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;

	return sprintf(buf, "%d\n", ddata->duration);
}

static ssize_t duration_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int value;
	int ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret != 0)
		return -EINVAL;

	ddata->duration = value;
	return size;
}

static ssize_t activate_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;

	return sprintf(buf, "%d\n", ddata->state);
}

static ssize_t activate_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	int value;
	int ret;

	ret = kstrtoint(buf, 0, &value);
	if (ret != 0)
		return -EINVAL;

	if (value > 0 && ddata->state > 0)
		timed_output_enable(ddata, ddata->duration);
	else
		timed_output_enable(ddata, 0);

	return size;
}

static DEVICE_ATTR_RW(state);
static DEVICE_ATTR_RW(duration);
static DEVICE_ATTR_RW(activate);

static struct attribute *led_vibrator_attributes[] = {
	&dev_attr_state.attr,
	&dev_attr_duration.attr,
	&dev_attr_activate.attr,
	&dev_attr_motor_type.attr,
	NULL,
};

static struct attribute_group led_vibrator_attr_group = {
	.attrs = led_vibrator_attributes,
};

int sec_vibrator_register(struct sec_vibrator_drvdata *ddata)
{
	struct task_struct *kworker_task;
	int ret = 0;

	if (!ddata) {
		pr_err("%s no ddata\n", __func__);
		return  -ENODEV;
	}

	g_ddata = ddata;

	mutex_init(&ddata->vib_mutex);
	kthread_init_worker(&ddata->kworker);
	kworker_task = kthread_run(kthread_worker_fn,
		   &ddata->kworker, "sec_vibrator");
	if (IS_ERR(kworker_task)) {
		pr_err("Failed to create message pump task\n");
		ret = -ENOMEM;
		goto err_kthread;
	}

	kthread_init_work(&ddata->kwork, sec_vibrator_work);
	hrtimer_init(&ddata->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ddata->timer.function = haptic_timer_func;

	/* create /sys/class/leds/vibrator */
	ddata->cdev.name = "vibrator";
	ret = devm_led_classdev_register(ddata->dev, &ddata->cdev);
	if (ret < 0) {
		pr_err("led class register fail\n");
		goto err_led_class;
	}

	ret = sysfs_create_group(&ddata->cdev.dev->kobj,
			&led_vibrator_attr_group);
	if (ret) {
		ret = -ENODEV;
		pr_err("Failed to create led sysfs1 %d\n", ret);
		goto err_led_sysfs;
	}

	/* create /sys/class/timed_output/vibrator */
	ddata->to_class = class_create(THIS_MODULE, "timed_output");
	if (IS_ERR(ddata->to_class)) {
		ret = PTR_ERR(ddata->to_class);
		goto err_class_create;
	}
	ddata->to_dev = device_create(ddata->to_class, NULL,
		MKDEV(0, 0), ddata, "vibrator");
	if (IS_ERR(ddata->to_dev)) {
		ret = PTR_ERR(ddata->to_dev);
		goto err_device_create;
	}

	ret = sysfs_create_group(&ddata->to_dev->kobj,
			&sec_vibrator_attr_group);
	if (ret) {
		ret = -ENODEV;
		pr_err("Failed to create sysfs1 %d\n", ret);
		goto err_sysfs1;
	}

	if (ddata->vib_ops->set_intensity) {
		ret = sysfs_create_file(&ddata->to_dev->kobj,
				&dev_attr_intensity.attr);
		if (ret) {
			ret = -ENODEV;
			pr_err("Failed to create sysfs2 %d\n", ret);
			goto err_sysfs2;
		}
		ret = sysfs_create_file(&ddata->cdev.dev->kobj,
				&dev_attr_intensity.attr);
		if (ret) {
			ret = -ENODEV;
			pr_err("Failed to create led sysfs2 %d\n", ret);
			goto err_sysfs2;
		}

		ddata->intensity = MAX_INTENSITY;
	}

	if (ddata->vib_ops->set_force_touch_intensity) {
		ret = sysfs_create_file(&ddata->to_dev->kobj,
				&dev_attr_force_touch_intensity.attr);
		if (ret) {
			ret = -ENODEV;
			pr_err("Failed to create sysfs3 %d\n", ret);
			goto err_sysfs3;
		}
		ret = sysfs_create_file(&ddata->cdev.dev->kobj,
				&dev_attr_force_touch_intensity.attr);
		if (ret) {
			ret = -ENODEV;
			pr_err("Failed to create led sysfs3 %d\n", ret);
			goto err_sysfs3;
		}

		ddata->force_touch_intensity = MAX_INTENSITY;
	}

	if (ddata->vib_ops->set_frequency) {
		ret = sysfs_create_group(&ddata->to_dev->kobj,
				&multi_freq_attr_group);
		if (ret) {
			ret = -ENODEV;
			pr_err("Failed to create sysfs4 %d\n", ret);
			goto err_sysfs4;
		}
		ret = sysfs_create_group(&ddata->cdev.dev->kobj,
				&multi_freq_attr_group);
		if (ret) {
			ret = -ENODEV;
			pr_err("Failed to create sysfs4 %d\n", ret);
			goto err_sysfs4;
		}

		ddata->frequency = FREQ_ALERT;
	}

	pr_info("%s done\n", __func__);

	return ret;

err_sysfs4:
	if (ddata->vib_ops->set_force_touch_intensity) {
		sysfs_remove_file(&ddata->to_dev->kobj,
				&dev_attr_force_touch_intensity.attr);
		sysfs_remove_file(&ddata->cdev.dev->kobj,
				&dev_attr_force_touch_intensity.attr);
	}
err_sysfs3:
	if (ddata->vib_ops->set_intensity) {
		sysfs_remove_file(&ddata->to_dev->kobj,
				&dev_attr_intensity.attr);
		sysfs_remove_file(&ddata->cdev.dev->kobj,
				&dev_attr_intensity.attr);
	}
err_sysfs2:
	sysfs_remove_group(&ddata->to_dev->kobj, &sec_vibrator_attr_group);
err_sysfs1:
	device_destroy(ddata->to_class, MKDEV(0, 0));
err_device_create:
	class_destroy(ddata->to_class);
err_class_create:
	sysfs_remove_group(&ddata->cdev.dev->kobj, &led_vibrator_attr_group);
err_led_sysfs:
	devm_led_classdev_unregister(ddata->dev, &ddata->cdev);
err_led_class:
err_kthread:
	mutex_destroy(&ddata->vib_mutex);
	return ret;
}

int sec_vibrator_unregister(struct sec_vibrator_drvdata *ddata)
{
	sec_vibrator_haptic_disable(ddata);
	g_ddata = NULL;
	if (ddata->vib_ops->set_frequency) {
		sysfs_remove_group(&ddata->to_dev->kobj,
				&multi_freq_attr_group);
		sysfs_remove_group(&ddata->cdev.dev->kobj,
				&multi_freq_attr_group);
	}
	if (ddata->vib_ops->set_force_touch_intensity) {
		sysfs_remove_file(&ddata->to_dev->kobj,
				&dev_attr_force_touch_intensity.attr);
		sysfs_remove_file(&ddata->cdev.dev->kobj,
				&dev_attr_force_touch_intensity.attr);
	}
	if (ddata->vib_ops->set_intensity) {
		sysfs_remove_file(&ddata->to_dev->kobj,
				&dev_attr_intensity.attr);
		sysfs_remove_file(&ddata->cdev.dev->kobj,
				&dev_attr_intensity.attr);
	}
	sysfs_remove_group(&ddata->to_dev->kobj, &sec_vibrator_attr_group);
	device_destroy(ddata->to_class, MKDEV(0, 0));
	class_destroy(ddata->to_class);
	sysfs_remove_group(&ddata->cdev.dev->kobj, &led_vibrator_attr_group);
	devm_led_classdev_unregister(ddata->dev, &ddata->cdev);
	mutex_destroy(&ddata->vib_mutex);
	return 0;
}

extern int haptic_homekey_press(void)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	struct hrtimer *timer;

	if (ddata == NULL)
		return -ENODEV;
	if (!ddata->vib_ops->set_force_touch_intensity)
		return -ENOSYS;

	timer = &ddata->timer;

	mutex_lock(&ddata->vib_mutex);
	ddata->timeout = HOMEKEY_DURATION;
	sec_vibrator_set_overdrive(ddata, true);
	sec_vibrator_set_frequency(ddata, FREQ_PRESS);
	sec_vibrator_set_force_touch_intensity(ddata,
			ddata->force_touch_intensity);
	sec_vibrator_set_enable(ddata, true);

	pr_info("%s freq:%d, intensity:%d, time:%d\n", __func__, FREQ_PRESS,
			ddata->force_touch_intensity, ddata->timeout);
	mutex_unlock(&ddata->vib_mutex);

	hrtimer_start(timer,
		ns_to_ktime((u64)ddata->timeout * NSEC_PER_MSEC),
		HRTIMER_MODE_REL);

	return 0;
}

extern int haptic_homekey_release(void)
{
	struct sec_vibrator_drvdata *ddata = g_ddata;
	struct hrtimer *timer;

	if (ddata == NULL)
		return -ENODEV;
	if (!ddata->vib_ops->set_force_touch_intensity)
		return -ENOSYS;

	timer = &ddata->timer;

	mutex_lock(&ddata->vib_mutex);
	ddata->timeout = HOMEKEY_DURATION;
	sec_vibrator_set_overdrive(ddata, true);
	sec_vibrator_set_frequency(ddata, FREQ_RELEASE);
	sec_vibrator_set_force_touch_intensity(ddata,
			ddata->force_touch_intensity);
	sec_vibrator_set_enable(ddata, true);

	pr_info("%s freq:%d, intensity:%d, time:%d\n", __func__, FREQ_RELEASE,
			ddata->force_touch_intensity, ddata->timeout);
	mutex_unlock(&ddata->vib_mutex);

	hrtimer_start(timer,
		ns_to_ktime((u64)ddata->timeout * NSEC_PER_MSEC),
		HRTIMER_MODE_REL);

	return 0;
}

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("sec vibrator driver");
