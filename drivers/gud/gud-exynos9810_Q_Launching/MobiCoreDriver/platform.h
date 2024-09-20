/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2018 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
/*
 * Header file of MobiCore Driver Kernel Module Platform
 * specific structures
 *
 * Internal structures of the McDrvModule
 *
 * Header file the MobiCore Driver Kernel Module,
 * its internal structures and defines.
 */
#ifndef _MC_DRV_PLATFORM_H_
#define _MC_DRV_PLATFORM_H_

#define IRQ_SPI(x)      (x + 32)

/* MobiCore Interrupt. */
#if defined(CONFIG_SOC_EXYNOS3250) || defined(CONFIG_SOC_EXYNOS3472)
#define MC_INTR_SSIQ	254
#elif defined(CONFIG_SOC_EXYNOS3475) || defined(CONFIG_SOC_EXYNOS5430) || \
	defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS7870) || \
	defined(CONFIG_SOC_EXYNOS8890) || defined(CONFIG_SOC_EXYNOS7880) || defined(CONFIG_SOC_EXYNOS8895)
#define MC_INTR_SSIQ	255
#elif defined(CONFIG_SOC_EXYNOS7885)
#define MC_INTR_SSIQ	97
#elif defined(CONFIG_SOC_EXYNOS7420) || defined(CONFIG_SOC_EXYNOS7580)
#define MC_INTR_SSIQ	246
#endif

/* Force usage of xenbus_map_ring_valloc as of Linux v4.1 */
#define MC_XENBUS_MAP_RING_VALLOC_4_1

#define TEE_WORKER_THREADS_ON_BIG_CORE_ONLY

#define NQ_TEE_WORKER_THREADS   4

/* Enable Fastcall worker thread */
#define MC_FASTCALL_WORKER_THREAD

/* Set Parameters for Secure OS Boosting */
#define DEFAULT_LITTLE_CORE		0
#define NONBOOT_LITTLE_CORE		1
#define DEFAULT_BIG_CORE		6
#define MIGRATE_TARGET_CORE     DEFAULT_BIG_CORE

#define MC_INTR_LOCAL_TIMER            (IRQ_SPI(470) + DEFAULT_BIG_CORE)

#define LOCAL_TIMER_PERIOD             50

#define DEFAULT_SECOS_BOOST_TIME       5000
#define MAX_SECOS_BOOST_TIME           600000  /* 600 sec */

#define DUMP_TBASE_HALT_STATUS

#endif /* _MC_DRV_PLATFORM_H_ */
