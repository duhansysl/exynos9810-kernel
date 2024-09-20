/*
 * Copyright (C) 2016 Samsung Electronics, Inc.
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

#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>

#include "tzdev_internal.h"
#include "core/cdev.h"
#include "core/platform.h"
#include "extensions/irs.h"

#if defined(CONFIG_ARCH_MSM) && defined(CONFIG_MSM_SCM)
#if defined(CONFIG_ARCH_MSM8939) || defined(CONFIG_ARCH_MSM8996)
#include <soc/qcom/scm.h>
#else
#error "Unsupported target! The only MSM8996 and MSM8939 are supported for Qualcomm chipset"
#endif
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Oleksii Mosolab");

#ifdef TZIRS_DEBUG
#define DBG(...)		pr_info("TZIRS DBG : " __VA_ARGS__)
#define ERR(...)		pr_alert("TZIRS ERR : " __VA_ARGS__)
#else
#define DBG(...)
#define ERR(...)		pr_alert("TZIRS ERR : " __VA_ARGS__)
#endif

/* Set appropriate command value */
#define SMC_IRS_CMD_RAW	(0x0000000B)
#define SMC_IRS_CMD TZDEV_SMC_COMMAND(SMC_IRS_CMD_RAW)

static int tzirs_fd_open = 0;
static DEFINE_MUTEX(tzirs_lock);

#if defined(CONFIG_ARCH_MSM) && defined(CONFIG_MSM_SCM)
#define SCM_BLOW_SW_FUSE_ID     0x01
#define SCM_IS_SW_FUSE_BLOWN_ID 0x02

static long __tzirs_smc_cmd(uint32_t p0, uint32_t *p1, uint32_t *p2, uint32_t *p3)
{
	unsigned int ret;

	struct scm_desc desc = {0};
	desc.args[0] = *p1;
	desc.arginfo = SCM_ARGS(1);

	switch(*p3) {
	case IRS_SET_FLAG_CMD:
		ret = scm_call2(SCM_SIP_FNID(p0, SCM_BLOW_SW_FUSE_ID), &desc);
		DBG("[SET_FLAG_CMD] : scm_call2 returned 0x%08x\n", ret);
		break;

	case IRS_GET_FLAG_VAL_CMD:
		ret = scm_call2(SCM_SIP_FNID(p0, SCM_IS_SW_FUSE_BLOWN_ID), &desc);
		if (ret) {
			ERR("[GET_FLAG_CMD] : scm_call2 failed ret = 0x%08x\n", ret);
			break;
		}
		DBG("[GET_FLAG_CMD] : desc.ret[0] = 0x%08x\n", (unsigned int) desc.ret[0]);
		*p2 = (uint32_t) desc.ret[0];
		break;

	default:
		ERR("Wrong SCM command ID\n");
		ret = IRS_INCORRECT_FLAG_TYPE;
		break;
	}

	return ret;
}
#else /* defined(CONFIG_ARCH_MSM) && defined(CONFIG_MSM_SCM) */
static long __tzirs_smc_cmd(unsigned long p0, unsigned long *p1, unsigned long *p2, unsigned long *p3)
{
	struct tzdev_smc_data data;

	data.args[0] = p0;
	data.args[1] = *p1;
	data.args[2] = *p2;
	data.args[3] = *p3;
	data.args[4] = 0;
	data.args[5] = 0;
	data.args[6] = 0;

	tzdev_platform_smc_call(&data);

	*p1 = data.args[1];
	*p2 = data.args[2];
	*p3 = data.args[3];

	return data.args[0];
}
#endif /* defined(CONFIG_ARCH_MSM) && defined(CONFIG_MSM_SCM) */

long tzirs_smc(unsigned long *p1, unsigned long *p2, unsigned long *p3)
{
#if defined(CONFIG_ARCH_MSM) && defined(CONFIG_MSM_SCM)
	return __tzirs_smc_cmd(SCM_SVC_FUSE, (uint32_t *)p1, (uint32_t *)p2 , (uint32_t *)p3);
#else
	return __tzirs_smc_cmd(SMC_IRS_CMD, p1, p2, p3);
#endif
}

static int tzirs_open(struct inode *n, struct file *f)
{
	int ret = 0;

	mutex_lock(&tzirs_lock);
	if (tzirs_fd_open) {
		ret = -EBUSY;
		goto out;
	}
	tzirs_fd_open++;
	DBG("open\n");

out:
	mutex_unlock(&tzirs_lock);
	return ret;
}

static inline int tzirs_release(struct inode *inode, struct file *file)
{
	mutex_lock(&tzirs_lock);
	if (tzirs_fd_open)
		tzirs_fd_open--;
	mutex_unlock(&tzirs_lock);
	DBG("release\n");
	return 0;
}

static long tzirs_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned long p1, p2, p3;

	struct irs_ctx __user *ioargp = (struct irs_ctx __user *) arg;
	struct irs_ctx ctx = {0};

	if ( _IOC_TYPE(cmd) != IOC_MAGIC ) {
		ERR("INVALID CMD = %d\n", cmd);
		return -ENOTTY;
	}
	switch (cmd) {
	case IOCTL_IRS_CMD:
		DBG("IOCT_IRS_CMD\n");
		/* get flag id */
		ret = copy_from_user(&ctx, ioargp, sizeof(struct irs_ctx));
		if (ret != 0) {
			ERR("IRS_CMD copy_from_user failed, ret = 0x%08x\n", ret);
			return -EFAULT;
		}

		p1 = ctx.id;
		p2 = ctx.value;
		p3 = ctx.func_cmd;

		DBG("IRS_CMD before: id = 0x%lx, value = 0x%lx, cmd = 0x%lx\n",
				(unsigned long)p1, (unsigned long)p2, (unsigned long)p3);

		ret = tzirs_smc(&p1, &p2, &p3);

		DBG("IRS_CMD after: id = 0x%lx, value = 0x%lx, cmd = 0x%lx\n",
				(unsigned long)p1, (unsigned long)p2, (unsigned long)p3);

		if (ret) {
			ERR("Unable to send IRS_CMD : id = 0x%lx, ret = %d\n",
					(unsigned long)p1, ret);
			return -EFAULT;
		}

		ctx.id = p1;
		ctx.value = p2;
		ctx.func_cmd = p3;

		ret = copy_to_user(ioargp, &ctx, sizeof(struct irs_ctx));
		if (ret != 0) {
			ERR("IRS_CMD copy_to_user failed, ret = 0x%08x\n", ret);
			return -EFAULT;
		}
		break;
	default:
		ERR("UNKNOWN CMD, cmd = 0x%08x\n", cmd);
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations tzirs_fops = {
	.owner = THIS_MODULE,
	.open = tzirs_open,
	.unlocked_ioctl = tzirs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tzirs_unlocked_ioctl,
#endif /* CONFIG_COMPAT */
	.release = tzirs_release,
};

static struct tz_cdev tzirs_cdev = {
	.name = "tzirs",
	.fops = &tzirs_fops,
	.owner = THIS_MODULE,
};

static int __init tzirs_init(void)
{
	int rc;

	rc = tz_cdev_register(&tzirs_cdev);
	if (rc) {
		ERR("Unable to register TZIRS driver, rc = 0x%08x\n", rc);
		return rc;
	}

	DBG("INSTALLED\n");
	return 0;
}

static void __exit tzirs_exit(void)
{
	tz_cdev_unregister(&tzirs_cdev);
	DBG("REMOVED\n");
}

module_init(tzirs_init);
module_exit(tzirs_exit);
