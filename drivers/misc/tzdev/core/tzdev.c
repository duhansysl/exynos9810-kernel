/*
 * Copyright (C) 2012-2019, Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/atomic.h>
#include <linux/buffer_head.h>
#include <linux/completion.h>
#include <linux/cpu.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/mmzone.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pid.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/syscore_ops.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>

#include <asm/segment.h>
#include <asm/uaccess.h>

#include "tzdev_internal.h"
#include "core/cdev.h"
#include "core/deploy_tzar.h"
#include "core/iwio.h"
#include "core/iwlog.h"
#include "core/iwservice.h"
#include "core/iwsock.h"
#include "core/kthread_pool.h"
#include "core/log.h"
#include "core/mem.h"
#include "core/notifier.h"
#include "core/platform.h"
#include "core/sysdep.h"
#include "debug/iw_boot_log.h"
#include "debug/panic_dump.h"
#include "extensions/boost.h"

MODULE_AUTHOR("Jaemin Ryu <jm77.ryu@samsung.com>");
MODULE_AUTHOR("Vasily Leonenko <v.leonenko@samsung.com>");
MODULE_AUTHOR("Alex Matveev <alex.matveev@samsung.com>");
MODULE_DESCRIPTION("TZDEV driver");
MODULE_LICENSE("GPL");

enum tzdev_swd_state {
	TZDEV_SWD_DOWN,
	TZDEV_SWD_UP,
	TZDEV_SWD_DEAD
};

enum tzdev_iwi_id {
	TZDEV_IWI_EVENT,
	TZDEV_IWI_PANIC
};

struct tzdev_fd_data {
	unsigned int boost_state;
	struct mutex mutex;
	void *platform_data;
};

static atomic_t tzdev_swd_state = ATOMIC_INIT(TZDEV_SWD_DOWN);
static int iwi_event, iwi_panic;
static struct tzio_sysconf tz_sysconf;
static atomic_t nwd_sysconf_flags = ATOMIC_INIT(0);

static irqreturn_t tzdev_event_handler(int irq, void *ptr);
#if CONFIG_TZDEV_IWI_PANIC != 0
static irqreturn_t tzdev_panic_handler(int irq, void *ptr);
#endif

static int tzdev_sysconf_init(void)
{
	int ret = 0;
	struct tz_iwio_aux_channel *ch;

	tz_sysconf.nwd_sysconf.flags = atomic_read(&nwd_sysconf_flags);

	ch = tz_iwio_get_aux_channel();

	memcpy(ch->buffer, &tz_sysconf.nwd_sysconf, sizeof(struct tzio_nwd_sysconf));

	ret = tzdev_smc_sysconf();
	if (ret) {
		log_error(tzdev, "tzdev_smc_sysconf() failed with %d\n", ret);
		goto out;
	}

	memcpy(&tz_sysconf.swd_sysconf, ch->buffer, sizeof(struct tzio_swd_sysconf));
out:
	tz_iwio_put_aux_channel();

	return ret;
}

static int tzdev_get_sysconf(struct file *filp, unsigned long arg)
{
	struct tzio_sysconf __user *argp = (struct tzio_sysconf __user *)arg;

	if (copy_to_user(argp, &tz_sysconf, sizeof(struct tzio_sysconf)))
		return -EFAULT;

	return 0;
}

static int tzdev_check_version(void)
{
	int ret;

	ret = tzdev_smc_check_version();
	if (ret == -ENOSYS) {
		log_info(tzdev, "Minor version of TZDev driver is newer than version of"
			"secure kernel.\nNot critical, continue...\n");
		ret = 0;
	} else if (ret == -EINVAL) {
		log_error(tzdev, "The version of the Linux kernel or "
			"TZDev driver is not compatible with "
			"secure kernel\n");
	}

	return ret;
}

static long tzdev_boost_control(struct file *filp, unsigned int state)
{
	int ret = 0;
	struct tzdev_fd_data *data = filp->private_data;

	mutex_lock(&data->mutex);

	switch (state) {
	case TZIO_BOOST_ON:
		if (!data->boost_state) {
			tz_boost_enable();
			data->boost_state = 1;
		} else {
			log_error(tzdev, "Trying to enable boost twice, filp=%pK\n", filp);
			ret = -EBUSY;
		}
		break;
	case TZIO_BOOST_OFF:
		if (data->boost_state) {
			tz_boost_disable();
			data->boost_state = 0;
		} else {
			log_error(tzdev, "Trying to disable boost twice, filp=%pK\n", filp);
			ret = -EBUSY;
		}
		break;
	default:
		log_error(tzdev, "Unknown boost request, cmd=%u filp=%pK\n", state, filp);
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&data->mutex);

	return ret;
}

static int tzdev_open(struct inode *inode, struct file *filp)
{
	int ret;
	struct tzdev_fd_data *data;

	data = kzalloc(sizeof(struct tzdev_fd_data), GFP_KERNEL);
	if (!data) {
		log_error(tzdev, "Failed to allocate private data\n");
		return -ENOMEM;
	}

	mutex_init(&data->mutex);

	data->platform_data = tzdev_platform_open();
	if (IS_ERR(data->platform_data)) {
		ret = PTR_ERR(data->platform_data);
		log_error(tzdev, "Failed to create platform data, error=%d\n", ret);
		goto release;
	}
	filp->private_data = data;

	return 0;
release:
	mutex_destroy(&data->mutex);
	kfree(data);

	return ret;
}

static int tzdev_release(struct inode *inode, struct file *filp)
{
	struct tzdev_fd_data *data = filp->private_data;

	tzdev_platform_release(data->platform_data);

	tzdev_boost_control(filp, TZIO_BOOST_OFF);

	mutex_destroy(&data->mutex);

	kfree(data);

	return 0;
}

static long tzdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct tzdev_fd_data *data = filp->private_data;

	switch (cmd) {
	case TZIO_GET_SYSCONF:
		return tzdev_get_sysconf(filp, arg);
	case TZIO_BOOST_CONTROL:
		return tzdev_boost_control(filp, arg);
	}

	return tzdev_platform_ioctl(data->platform_data, cmd, arg);
}

static const struct file_operations tzdev_fops = {
	.owner = THIS_MODULE,
	.open = tzdev_open,
	.release = tzdev_release,
	.unlocked_ioctl = tzdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tzdev_ioctl,
#endif /* CONFIG_COMPAT */
};

static struct tz_cdev tzdev_cdev = {
	.name = "tzdev",
	.fops = &tzdev_fops,
	.owner = THIS_MODULE,
};

static unsigned int tzdev_virq_to_hwirq(unsigned int virq)
{
	struct irq_desc *desc = irq_to_desc(virq);
	struct irq_data *data = irq_desc_get_irq_data(desc);

	data = data->parent_data ? : data;

	return irqd_to_hwirq(data);
}

static unsigned int tzdev_get_iwi(unsigned int idx)
{
	unsigned int iwi;
	struct device_node *node;

	node = of_find_compatible_node(NULL, NULL, "samsung,blowfish");
	if (!node)
		node = of_find_compatible_node(NULL, NULL, "samsung,teegris");

	if (!node)
		return 0;

	iwi = irq_of_parse_and_map(node, idx);
	of_node_put(node);

	return iwi;
}

static int tzdev_register_iwis(void)
{
	int ret;
	unsigned int hw_iwi_event;
#if CONFIG_TZDEV_IWI_PANIC != 0
	unsigned int hw_iwi_panic;
#endif

	iwi_event = tzdev_get_iwi(TZDEV_IWI_EVENT);
	if (!iwi_event) {
		iwi_event = CONFIG_TZDEV_IWI_EVENT;
		hw_iwi_event = iwi_event;
	} else
		hw_iwi_event = tzdev_virq_to_hwirq(iwi_event);

	ret = request_irq(iwi_event, tzdev_event_handler, 0, "tzdev_iwi_event", NULL);
	if (ret) {
		log_error(tzdev, "Event IWI registration failed: %d\n", ret);
		return ret;
	}

#if CONFIG_TZDEV_IWI_PANIC != 0
	iwi_panic = tzdev_get_iwi(TZDEV_IWI_PANIC);
	if (!iwi_panic) {
		iwi_panic = CONFIG_TZDEV_IWI_PANIC;
		hw_iwi_panic = iwi_panic;
	} else
		hw_iwi_panic = tzdev_virq_to_hwirq(iwi_panic);

	ret = request_irq(iwi_panic, tzdev_panic_handler, 0, "tzdev_iwi_panic", NULL);
	if (ret) {
		log_error(tzdev, "Panic IWI registration failed: %d\n", ret);
		free_irq(iwi_event, 0);
		iwi_event = 0;
		return ret;
	}
	tz_sysconf.nwd_sysconf.iwi_panic = hw_iwi_panic;
#endif
	tz_sysconf.nwd_sysconf.iwi_event = hw_iwi_event;

	return ret;
}

static void tzdev_unregister_iwis(void)
{
	if (iwi_event) {
		free_irq(iwi_event, 0);
		iwi_event = 0;
	}

	if (iwi_panic) {
		free_irq(iwi_panic, 0);
		iwi_panic = 0;
	}
}

static irqreturn_t tzdev_event_handler(int irq, void *ptr)
{
	tz_kthread_pool_cmd_send();

	return IRQ_HANDLED;
}

#if CONFIG_TZDEV_IWI_PANIC != 0
static void dump_kernel_panic_bh(struct work_struct *work)
{
	tz_iw_boot_log_read();
#if defined(CONFIG_TZDEV_SWD_PANIC_IS_CRITICAL)
	panic("tzdev: IWI_PANIC raised\n");
#else
	tzdev_run_fini_sequence();
	printk("tzdev: IWI_PANIC raised\n");
#endif
}

static DECLARE_WORK(dump_kernel_panic, dump_kernel_panic_bh);

static irqreturn_t tzdev_panic_handler(int irq, void *ptr)
{
	schedule_work(&dump_kernel_panic);
	return IRQ_HANDLED;
}
#endif

static int __init tzdev_init(void)
{
	int rc;

	rc = tzdev_platform_register();
	if (rc)
		log_error(tzdev, "tzdev_platform_init() failed with error=%d\n", rc);

	return rc;
}

module_init(tzdev_init);

struct tzio_sysconf *tzdev_sysconf(void)
{
	return &tz_sysconf;
}

unsigned int tzdev_is_up(void)
{
	return atomic_read(&tzdev_swd_state) == TZDEV_SWD_UP;
}

int tzdev_smc(struct tzdev_smc_data *data)
{
	int ret;

	tzdev_atomic_notifier_run(TZDEV_PRE_SMC_NOTIFIER);

	log_debug(tzdev, "enter: args={0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x}\n",
			data->args[0], data->args[1], data->args[2], data->args[3],
			data->args[4], data->args[5], data->args[6]);

	ret = tzdev_platform_smc_call(data);

	log_debug(tzdev, "tzdev_smc_cmd: exit: args={0x%x 0x%x 0x%x 0x%x} ret=%d\n",
			data->args[0], data->args[1], data->args[2], data->args[3], ret);

	tzdev_atomic_notifier_run(TZDEV_POST_SMC_NOTIFIER);

	return ret;
}

int tzdev_run_init_sequence(void)
{
	int ret = -ESHUTDOWN;

	if (atomic_read(&tzdev_swd_state) != TZDEV_SWD_DOWN)
		return 0;

	if (tzdev_check_version())
		goto out;
	if (tz_iwio_init())
		goto out;
	if (tz_iwservice_init())
		goto out;
	if (tz_panic_dump_init())
		goto out;
	if (tz_iwlog_init())
		goto out;

	/* IW log initialized, time to read boot log */
	tz_iw_boot_log_read();

	if (tzdev_mem_init())
		goto tzdev_mem_init_failed;
	if (tz_iwsock_init())
		goto tz_iwsock_init_failed;
	if (tzdev_register_iwis())
		goto tzdev_register_iwis_failed;
	if (tzdev_sysconf_init())
		goto tzdev_sysconf_init_failed;

	ret = tz_cdev_register(&tzdev_cdev);
	if (ret) {
		log_error(tzdev, "Failed to register tzdev device, error=%d\n", ret);
		ret = -ESHUTDOWN;
		goto tzdev_cdev_register_failed;
	}

	tzdev_blocking_notifier_run(TZDEV_INIT_NOTIFIER);
	BUG_ON(atomic_cmpxchg(&tzdev_swd_state, TZDEV_SWD_DOWN, TZDEV_SWD_UP));

	if (tzdev_deploy_tzar()) {
		ret = -ESHUTDOWN;
		goto tzdev_deploy_tzar_failed;
	}

	return 0;

tzdev_deploy_tzar_failed:
	tzdev_blocking_notifier_run(TZDEV_FINI_NOTIFIER);

	atomic_set(&tzdev_swd_state, TZDEV_SWD_DEAD);
	tz_cdev_unregister(&tzdev_cdev);
tzdev_cdev_register_failed:
	tzdev_unregister_iwis();
tzdev_sysconf_init_failed:
tzdev_register_iwis_failed:
	tz_iwsock_fini();
tz_iwsock_init_failed:
	tzdev_mem_fini();
tzdev_mem_init_failed:
	tz_iwlog_fini();
out:
	return ret;
}

void tzdev_run_fini_sequence(void)
{
	atomic_set(&tzdev_swd_state, TZDEV_SWD_DEAD);

	tzdev_blocking_notifier_run(TZDEV_FINI_NOTIFIER);

	tz_cdev_unregister(&tzdev_cdev);
	tzdev_unregister_iwis();
	tz_kthread_pool_fini();
	tz_iwsock_fini();
	tz_iwlog_fini();
}

void tzdev_set_nwd_sysconf_flag(unsigned int flag)
{
	atomic_or(flag, &nwd_sysconf_flags);
}
