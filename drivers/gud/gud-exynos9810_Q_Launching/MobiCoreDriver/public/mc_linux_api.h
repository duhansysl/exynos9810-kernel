/*
Copyright (c) 2013-2019 TRUSTONIC LIMITED
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

#ifndef _MC_LINUX_API_H_
#define _MC_LINUX_API_H_

#include <linux/types.h>

/*
 * set affinity of tee workser threads to big core or default core affinity
 */

#define TEE_WORKER_THREADS_ON_BIG_CORE_ONLY

#if defined(TEE_WORKER_THREADS_ON_BIG_CORE_ONLY) 
#define BIG_CORE_AFFINITY_MASK (0xF0) 
void set_tee_worker_threads_on_big_core(bool big_core);
#else
void set_tee_worker_threads_on_big_core(bool __attribute__((unused))big_core);
#endif

#endif /* _MC_LINUX_API_H_ */
