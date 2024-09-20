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

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "tzdev_internal.h"
#include "core/log.h"

static struct platform_device *tzdev_pdev;

static int tzdev_probe(struct platform_device *pdev)
{
	int ret;

	(void) pdev;

	ret = tzdev_run_init_sequence();
	if (ret) {
		log_error(tzdev_platform, "tzdev initialization failed, error=%d.\n", ret);
		return ret;
	}

	log_info(tzdev_platform, "tzdev initialization done.\n");

	return 0;
}

static void tzdev_shutdown(struct platform_device *pdev)
{
	(void) pdev;

	tzdev_run_fini_sequence();

	log_info(tzdev_platform, "tzdev finalization done.\n");
}

struct platform_driver tzdev_pdrv = {
	.probe = tzdev_probe,
	.shutdown = tzdev_shutdown,

	.driver = {
		.name = "tzdev",
		.owner = THIS_MODULE,
	},
};

int tzdev_platform_register(void)
{
	int ret;

	ret = platform_driver_register(&tzdev_pdrv);
	if (ret) {
		log_error(tzdev_platform, "failed to register tzdev platform driver, error=%d\n", ret);
		return ret;
	}

	tzdev_pdev = platform_device_register_resndata(NULL, "tzdev", -1, NULL, 0, NULL, 0);
	if (IS_ERR(tzdev_pdev)) {
		ret = PTR_ERR(tzdev_pdev);
		log_error(tzdev_platform, "failed to register tzdev platform device, error=%d\n", ret);
		return ret;
	}

	return 0;
}

void tzdev_platform_unregister(void)
{
	platform_driver_unregister(&tzdev_pdrv);
}

int tzdev_platform_smc_call(struct tzdev_smc_data *data)
{
	register unsigned long _r0 __asm__(REGISTERS_NAME "0") = data->args[0] | TZDEV_SMC_MAGIC;
	register unsigned long _r1 __asm__(REGISTERS_NAME "1") = data->args[1];
	register unsigned long _r2 __asm__(REGISTERS_NAME "2") = data->args[2];
	register unsigned long _r3 __asm__(REGISTERS_NAME "3") = data->args[3];
	register unsigned long _r4 __asm__(REGISTERS_NAME "4") = data->args[4];
	register unsigned long _r5 __asm__(REGISTERS_NAME "5") = data->args[5];
	register unsigned long _r6 __asm__(REGISTERS_NAME "6") = data->args[6];

	__asm__ __volatile("sub sp, sp, #16\n"
			"str %0, [sp]\n" : : "r" (data));

	__asm__ __volatile__(ARCH_EXTENSION SMC(0): "+r"(_r0) , "+r" (_r1) , "+r" (_r2),
			"+r" (_r3), "+r" (_r4), "+r" (_r5), "+r" (_r6) : : "memory");

	__asm__ __volatile("ldr %0, [sp]\n"
			"add sp, sp, #16\n" : : "r" (data));

	data->args[0] = _r0;
	data->args[1] = _r1;
	data->args[2] = _r2;
	data->args[3] = _r3;

	return 0;
}

void *tzdev_platform_open(void)
{
	return NULL;
}

void tzdev_platform_release(void *data)
{
	return;
}

long tzdev_platform_ioctl(void *data, unsigned int cmd, unsigned long arg)
{
	(void) data;
	(void) cmd;
	(void) arg;

	return -ENOTTY;
}
