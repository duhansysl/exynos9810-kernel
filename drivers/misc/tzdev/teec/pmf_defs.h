/**
 * Copyright (C) 2012-2019, Samsung Electronics Co., Ltd.
 *
 * Common definitions for using in Performance Measurement Framework.
 */

#pragma once

#ifndef __USED_BY_TZSL__
#include <linux/types.h>
#else
#include <stdint.h>
#include <sys/types.h>
#endif

/* Attention! This enum pmf_id should be synced with
 * enum described in ./core/pmf.h file.
 * */
enum pmf_id {
	PMF_TEEC_OPEN_SESSION,
	PMF_TEEC_REG_SHMEM,
	PMF_TEEC_REG_SHMEM_STOP,
	PMF_TEEC_OPEN_SESSION_SEND_CMD,
	PMF_TEEC_OPEN_SESSION_SEND_CMD_STOP,
	PMF_TEEC_OPEN_SESSION_STOP,
	PMF_TZDEV_MEM_REGISTER,
	PMF_TZDEV_MEM_REGISTER_STOP,
	PMF_ROOT_TASK_INIT_CONTEXT,
	PMF_ROOT_TASK_INIT_CONTEXT_STOP,
	PMF_ROOT_TASK_PREPARE_SESSION,
	PMF_ROOT_TASK_PREPARE_SESSION_STOP,
	PMF_ROOT_TASK_SOCKET_WORK,
	PMF_ROOT_TASK_SOCKET_WORK_STOP,
	PMF_ROOT_TASK_READ_TA_STOP,
	PMF_ROOT_TASK_VERIFY_TA,
	PMF_ROOT_TASK_VERIFY_TA_STOP,
	PMF_ROOT_TASK_PKG_VERIFY,
	PMF_ROOT_TASK_PKG_VERIFY_STOP,
	PMF_ROOT_TASK_SPAWN_TA,
	PMF_ROOT_TASK_SPAWN_TA_STOP,
	PMF_TA_CRYPTO_INIT,
	PMF_TA_CRYPTO_INIT_STOP,
	PMF_TA_PROP_INIT,
	PMF_TA_PROP_INIT_STOP,
	PMF_TA_SEND_REPLY,
	PMF_SK_MMAP,
	PMF_SK_MMAP_STOP,
	PMF_MAX
};

struct pmf_data {
	uint64_t samples[PMF_MAX];
};

static inline uint64_t read_ticks(void)
{
	uint64_t ticks;

#if defined(__arm__)
	uint32_t ticks_low, ticks_high;
	__asm__ __volatile__ ("isb;mrrc p15, 0, %0, %1, c14" : "=r"(ticks_low), "=r"(ticks_high) : : );
	ticks = ((uint64_t)ticks_high << 32) | ticks_low;
#elif defined(__aarch64__)
	__asm__ __volatile__ ("isb;mrs %0, cntpct_el0" : "=r"(ticks) : : );
#else
#error "Unsupported architechture."
#endif

	return ticks;
}
