/*
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *	      http://www.samsung.com/
 *
 * Exynos - Support SoC specific handler
 * Author: Hosung Kim <hosung0.kim@samsung.com>
 *         Youngmin Nam <youngmin.nam@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/soc/samsung/exynos-soc.h>
#include <linux/interrupt.h>
#include <linux/sec_debug.h>

struct exynos_handler {
	int		irq;
	char		name[SZ_16];
	irqreturn_t	(*handle_irq)(int irq, void *data);
};

enum ecc_handler_nr {
	CPUCL0_ERRIRQ_0,
	CPUCL0_ERRIRQ_1,
	CPUCL0_ERRIRQ_2,
	CPUCL0_ERRIRQ_3,
	CPUCL0_ERRIRQ_4,
	CPUCL1_INTERRIRQ,
	MAX_ERRIRQ,
};

static struct exynos_handler ecc_handler[MAX_ERRIRQ];

static irqreturn_t exynos_ecc_handler(int irq, void *data)
{
	struct exynos_handler *ecc = (struct exynos_handler *)data;
	unsigned long reg1, reg2, reg3;

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
	sec_debug_set_extra_info_merr(ecc->name);
#endif

	/*
	 *  Output CPU Memory Error syndrome Register
	 *  CPUMERRSR, L2MERRSR, L3MERRSR
	 */
	if (read_cpuid_implementor() == ARM_CPU_IMP_SEC) {
		switch (read_cpuid_part_number()) {
		case ARM_CPU_PART_MEERKAT:
			asm ("mrs %0, S3_1_c15_c2_0\n\t"
					"mrs %1, S3_1_c15_c2_4\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("FEMERR0SR: %016lx, FEMERR1SR: %016lx\n", reg1, reg2);
			asm ("mrs %0, S3_1_c15_c2_1\n\t"
					"mrs %1, S3_1_c15_c2_5\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("LSMERR0SR: %016lx, LSMERR1SR: %016lx\n", reg1, reg2);
			asm ("mrs %0, S3_1_c15_c2_2\n\t"
					"mrs %1, S3_1_c15_c2_6\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("TBWMERR0SR: %016lx, TBWMERR1SR: %016lx\n", reg1, reg2);
			asm ("mrs %0, S3_1_c15_c2_3\n\t"
					"mrs %1, S3_1_c15_c2_7\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("L2MERR0SR: %016lx, L2MERR1SR: %016lx\n", reg1, reg2);

			/* L3 MERR */
			asm ("msr S3_1_c15_c7_1, %0\n\t"
					"isb\n"
					:: "r" (0));
			asm ("mrs %0, S3_1_c15_c3_0\n\t"
					"mrs %1, S3_1_c15_c2_7\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("BANK0 L3MERR0SR: %016lx, L3MERR1SR: %016lx\n", reg1, reg2);
			asm ("msr S3_1_c15_c7_1, %0\n\t"
					"isb\n"
					:: "r" (1));
			asm ("mrs %0, S3_1_c15_c3_0\n\t"
					"mrs %1, S3_1_c15_c2_7\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("BANK1 L3MERR0SR: %016lx, L3MERR1SR: %016lx\n", reg1, reg2);
			asm ("msr S3_1_c15_c7_1, %0\n\t"
					"isb\n"
					:: "r" (2));
			asm ("mrs %0, S3_1_c15_c3_0\n\t"
					"mrs %1, S3_1_c15_c2_7\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("BANK2 L3MERR0SR: %016lx, L3MERR1SR: %016lx\n", reg1, reg2);
			asm ("msr S3_1_c15_c7_1, %0\n\t"
					"isb\n"
					:: "r" (3));
			asm ("mrs %0, S3_1_c15_c3_0\n\t"
					"mrs %1, S3_1_c15_c2_7\n"
					: "=r" (reg1), "=r" (reg2));
			pr_emerg("BANK3 L3MERR0SR: %016lx, L3MERR1SR: %016lx\n", reg1, reg2);

			break;
		default:
			break;
		}
	} else {
		switch (read_cpuid_part_number()) {
		case ARM_CPU_PART_ANANKE:
			asm ("HINT #16");
			asm ("mrs %0, S3_0_c12_c1_1\n" : "=r" (reg1)); /* read DISR_EL1 */
			pr_emerg("DISR_EL1: %016lx\n", reg1);

			asm ("msr S3_0_c5_c3_1, %0\n"  :: "r" (0)); /* set 1st ERRSELR_EL1 */

			asm ("mrs %0, S3_0_c5_c4_2\n"
					"mrs %1, S3_0_c5_c4_3\n"
					"mrs %2, S3_0_c5_c5_0\n"
					: "=r" (reg1), "=r" (reg2), "=r" (reg3));
			pr_emerg("1st : ERXSTATUS_EL1: %016lx, ERXADDR_EL1: %016lx, "
					"ERXMISC0_EL1: %016lx\n", reg1, reg2, reg3);

			asm ("msr S3_0_c5_c3_1, %0\n"  :: "r" (1)); /* set 2nd ERRSELR_EL1 */

			asm ("mrs %0, S3_0_c5_c4_2\n"
					"mrs %1, S3_0_c5_c4_3\n"
					"mrs %2, S3_0_c5_c5_0\n"
					: "=r" (reg1), "=r" (reg2), "=r" (reg3));
			pr_emerg("2nd : ERXSTATUS_EL1: %016lx, ERXADDR_EL1: %016lx, "
					"ERXMISC0_EL1: %016lx\n", reg1, reg2, reg3);

			break;
		default:
			break;
		}
	}

	panic("Detected ECC error: irq%d, name%s", ecc->irq, ecc->name);
	return 0;
}

static int __init exynos_handler_setup(struct device_node *np)
{
	int err = 0, i;

	/* setup ecc_handler */
	for (i = CPUCL0_ERRIRQ_0; i < MAX_ERRIRQ; i++) {
		ecc_handler[i].irq = irq_of_parse_and_map(np, i);
		snprintf(ecc_handler[i].name, sizeof(ecc_handler[i].name),
				"ecc_handler%d", i);
		ecc_handler[i].handle_irq = exynos_ecc_handler;

		err = request_irq(ecc_handler[i].irq,
				ecc_handler[i].handle_irq,
				IRQF_NOBALANCING | IRQF_GIC_MULTI_TARGET,
				ecc_handler[i].name, &ecc_handler[i]);
		if (err) {
			pr_err("unable to request irq%d for %s ecc handler\n",
					ecc_handler[i].irq, ecc_handler[i].name);
			break;
		} else {
			pr_info("Success to request irq%d for %s ecc handler\n",
					ecc_handler[i].irq, ecc_handler[i].name);
		}
	}

	of_node_put(np);
	return err;
}

static const struct of_device_id handler_of_match[] __initconst = {
	{ .compatible = "exynos,handler", .data = exynos_handler_setup},
	{},
};

typedef int (*handler_initcall_t)(const struct device_node *);
static int __init exynos_handler_init(void)
{
	struct device_node *np;
	const struct of_device_id *matched_np;
	handler_initcall_t init_fn;

	np = of_find_matching_node_and_match(NULL, handler_of_match, &matched_np);
	if (!np)
		return -ENODEV;

	init_fn = (handler_initcall_t)matched_np->data;

	return init_fn(np);
}
subsys_initcall(exynos_handler_init);
