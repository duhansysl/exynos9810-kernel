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

#include <asm/memory.h>
#include <asm/page.h>

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/highmem.h>
#include <linux/mm.h>
#include <linux/platform_device.h>

#if defined(CONFIG_ARCH_EXYNOS3)
#include <asm-generic/dma-contiguous.h>
#endif /* CONFIG_ARCH_EXYNOS3 */

#if defined(CONFIG_OF_RESERVED_MEM)
#include <linux/memblock.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#endif /* defined(CONFIG_OF_RESERVED_MEM) */

#include "tzdev_internal.h"
#include "core/iwio.h"
#include "core/log.h"
#include "core/notifier.h"

#define SCMA_MAX_ALIGN_ORDER	31
#define SCMA_SWD_PAGE_SHIFT	12

enum {
	TZDEV_SCMA_SMC_GET_NWD_REGIONS_INFO,
	TZDEV_SCMA_SMC_INIT_NWD_REGIONS,
};

struct scma_iwio_mem_chunk_desc {
	uint64_t addr;
	uint32_t size;
	uint16_t align_order;
} __packed;

#if !defined(CONFIG_OF_RESERVED_MEM)
struct platform_device scma_device = {
	.dev = {
		.init_name = "scma",
	},
};
#endif /* !defined(CONFIG_OF_RESERVED_MEM) */

#if defined(CONFIG_OF_RESERVED_MEM)
static dma_addr_t scma_res_mem_base;
static unsigned int scma_res_mem_size;

int __init tzdev_scma_init(struct reserved_mem *rmem)
{
	scma_res_mem_base = rmem->base;
	scma_res_mem_size = rmem->size;

	return 0;
}
RESERVEDMEM_OF_DECLARE(teegris_scma, "teegris,scma", tzdev_scma_init);

static dma_addr_t tzdev_scma_alloc_contig(unsigned int size, unsigned int align)
{
	(void)size;
	(void)align;

	return scma_res_mem_base;
}

static int tzdev_scma_free_contig(dma_addr_t base, unsigned int size)
{
	(void)base;
	(void)size;

	if (scma_res_mem_base)
		return memblock_free(scma_res_mem_base, scma_res_mem_size);

	return 0;
}
#elif defined(CONFIG_ARCH_EXYNOS3) /* defined(CONFIG_OF_RESERVED_MEM) */
struct page *cma_enable_sharing(struct cma *cma);
const unsigned int scma_res_mem_size = 2 * 1024 * 1024; /* 2Mb */

static dma_addr_t tzdev_scma_alloc_contig(unsigned int size, unsigned int align)
{
	struct page *page;
	struct cma *cma = dev_get_cma_area(&scma_device.dev);

	if (!cma) {
		log_error(tzdev_scma, "SCMA region isn't initialized\n");
		return 0;
	}

	page = dma_alloc_from_contiguous(&scma_device.dev, size >> PAGE_SHIFT,
			get_order(align));
	if (!page)
		return 0;

	return page_to_phys(page);
}

static int tzdev_scma_free_contig(dma_addr_t base, unsigned int size)
{
	int ret;

	if (base) {
		ret = dma_release_from_contiguous(&scma_device.dev, phys_to_page(base),
				size >> PAGE_SHIFT);
		if (ret) {
			log_error(tzdev_scma, "Failed to release memory, error=%d\n", ret);
			return ret;
		}
	}

	cma_enable_sharing(dev_get_cma_area(&scma_device.dev));

	return 0;
}
#endif /* defined(CONFIG_OF_RESERVED_MEM) */

static int tz_scma_init_call(struct notifier_block *cb, unsigned long code, void *unused)
{
	struct tz_iwio_aux_channel *ch;
	struct scma_iwio_mem_chunk_desc mem_chunk;
	dma_addr_t addr = 0;
	unsigned int align;
	int ret;

	(void)cb;
	(void)code;
	(void)unused;

	ch = tz_iwio_get_aux_channel();

	ret = tzdev_smc_scma_cmd(TZDEV_SCMA_SMC_GET_NWD_REGIONS_INFO);
	if (ret) {
		log_error(tzdev_scma, "Failed to get SCMA regions info, error=%d\n", ret);
		tz_iwio_put_aux_channel();
		goto free;
	}

	mem_chunk = *((struct scma_iwio_mem_chunk_desc *)ch->buffer);

	tz_iwio_put_aux_channel();

	if (!mem_chunk.size) {
		log_debug(tzdev_scma, "No any nwd regions found\n");
		goto free;
	}

	log_debug(tzdev_scma, "SWD requested %#x bytes of memory, align order is %u\n",
			mem_chunk.size, mem_chunk.align_order);

	if (mem_chunk.align_order < SCMA_SWD_PAGE_SHIFT ||
			mem_chunk.align_order > SCMA_MAX_ALIGN_ORDER) {
		log_error(tzdev_scma, "Incorrect align order=%u\n", mem_chunk.align_order);
		goto free;
	}

	if (mem_chunk.size > scma_res_mem_size) {
		log_error(tzdev_scma, "Region size incorrect, "
				"current: %#x bytes, expected: %#x bytes\n",
				(unsigned int)scma_res_mem_size, (unsigned int)mem_chunk.size);
		goto free;
	}

	align = 1U << mem_chunk.align_order;
	addr = tzdev_scma_alloc_contig(scma_res_mem_size, align);
	if (!addr) {
		log_error(tzdev_scma, "Failed to allocate memory\n");
		goto free;
	}

	if (!IS_ALIGNED(addr, align)) {
		log_error(tzdev_scma, "Region base address %#lx must be %#x aligned\n",
				(unsigned long)addr, align);
		goto free;
	}

	mem_chunk.addr = (uint64_t)addr;

	ch = tz_iwio_get_aux_channel();

	*((struct scma_iwio_mem_chunk_desc *)ch->buffer) = mem_chunk;

	ret = tzdev_smc_scma_cmd(TZDEV_SCMA_SMC_INIT_NWD_REGIONS);
	if (ret) {
		log_error(tzdev_scma, "Failed to init scma regions, error=%d\n", ret);
		tz_iwio_put_aux_channel();
		goto free;
	}

	tz_iwio_put_aux_channel();

	log_info(tzdev_scma, "SCMA initialization done.\n");

	return NOTIFY_DONE;
free:
	tzdev_scma_free_contig(addr, scma_res_mem_size);

	return NOTIFY_DONE;
}

static struct notifier_block tz_scma_init_notifier = {
	.notifier_call = tz_scma_init_call,
};

static int __init tz_scma_init(void)
{
	int rc;

	rc = tzdev_blocking_notifier_register(TZDEV_INIT_NOTIFIER, &tz_scma_init_notifier);
	if (rc) {
		log_error(tzdev_scma, "Failed to register init notifier, error=%d\n", rc);
		return rc;
	}

	log_info(tzdev_scma, "SCMA callbacks registration done\n");

	return rc;
}

early_initcall(tz_scma_init);
