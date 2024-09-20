/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Exynos-SnapShot debugging framework for Exynos SoC
 *
 * Author: Hosung Kim <Hosung0.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef EXYNOS_SNAPSHOT_LOCAL_H
#define EXYNOS_SNAPSHOT_LOCAL_H
#include <linux/exynos-ss.h>
#include <linux/exynos-ss-soc.h>

extern void *return_address(int);
extern void (*arm_pm_restart)(char str, const char *cmd);

extern void exynos_ss_log_idx_init(void);
extern void exynos_ss_utils_init(void);
extern void __iomem *exynos_ss_get_base_vaddr(void);
extern void __iomem *exynos_ss_get_base_paddr(void);
extern void exynos_ss_scratch_reg(unsigned int val);
extern void exynos_ss_print_panic_report(void);

/*  Size domain */
#define ESS_KEEP_HEADER_SZ		(SZ_256 * 3)
#define ESS_HEADER_SZ			SZ_4K
#define ESS_MMU_REG_SZ			SZ_4K
#define ESS_CORE_REG_SZ			SZ_4K
#define ESS_SPARE_SZ			SZ_16K
#define ESS_HEADER_TOTAL_SZ		(ESS_HEADER_SZ + ESS_MMU_REG_SZ + ESS_CORE_REG_SZ + ESS_SPARE_SZ)

/*  Length domain */
#define ESS_LOG_STRING_LENGTH		SZ_128
#define ESS_MMU_REG_OFFSET		SZ_512
#define ESS_CORE_REG_OFFSET		SZ_512
#define ESS_LOG_MAX_NUM			SZ_1K
#define ESS_API_MAX_NUM			SZ_2K
#define ESS_EX_MAX_NUM			SZ_8
#define ESS_IN_MAX_NUM			SZ_8
#define ESS_CALLSTACK_MAX_NUM		4
#define ESS_ITERATION			5
#define ESS_NR_CPUS			NR_CPUS
#define ESS_ITEM_MAX_NUM		10

/* Sign domain */
#define ESS_SIGN_RESET			0x0
#define ESS_SIGN_RESERVED		0x1
#define ESS_SIGN_SCRATCH		0xD
#define ESS_SIGN_ALIVE			0xFACE
#define ESS_SIGN_DEAD			0xDEAD
#define ESS_SIGN_PANIC			0xBABA
#define ESS_SIGN_SAFE_FAULT		0xFAFA
#define ESS_SIGN_NORMAL_REBOOT		0xCAFE
#define ESS_SIGN_FORCE_REBOOT		0xDAFE
#define ESS_SIGN_LOCKUP			0xDEADBEEF

/*  Specific Address Information */
#define ESS_FIXED_VIRT_BASE		(VMALLOC_START + 0xF7000000)
#define ESS_OFFSET_SCRATCH		(0x100)
#define ESS_OFFSET_DEBUG_LEVEL		(0x180)
#define ESS_OFFSET_LAST_LOGBUF		(0x200)
#define ESS_OFFSET_EMERGENCY_REASON	(0x300)
#define ESS_OFFSET_CORE_POWER_STAT	(0x400)
#define ESS_OFFSET_PANIC_STAT		(0x500)
#define ESS_OFFSET_CORE_LAST_PC		(0x600)

/* S5P_VA_SS_BASE + 0xC00 -- 0xFFF is reserved */
#define ESS_OFFSET_PANIC_STRING		(0xC00)
#define ESS_OFFSET_SPARE_BASE		(ESS_HEADER_SZ + ESS_MMU_REG_SZ + ESS_CORE_REG_SZ)

struct exynos_ss_log {
	struct __task_log {
		unsigned long long time;
		unsigned long sp;
		struct task_struct *task;
		char task_comm[TASK_COMM_LEN];
	} task[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct __work_log {
		unsigned long long time;
		unsigned long sp;
		struct worker *worker;
		char task_comm[TASK_COMM_LEN];
		work_func_t fn;
		int en;
	} work[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct __cpuidle_log {
		unsigned long long time;
		unsigned long sp;
		char *modes;
		unsigned state;
		u32 num_online_cpus;
		int delta;
		int en;
	} cpuidle[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct __suspend_log {
		unsigned long long time;
		unsigned long sp;
		void *fn;
		struct device *dev;
		int en;
		int core;
	} suspend[ESS_LOG_MAX_NUM * 4];

	struct __irq_log {
		unsigned long long time;
		unsigned long sp;
		int irq;
		void *fn;
		unsigned int preempt;
		unsigned int val;
		int en;
	} irq[ESS_NR_CPUS][ESS_LOG_MAX_NUM * 2];

#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_EXIT
	struct __irq_exit_log {
		unsigned long long time;
		unsigned long sp;
		unsigned long long end_time;
		unsigned long long latency;
		int irq;
	} irq_exit[ESS_NR_CPUS][ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_SPINLOCK
	struct __spinlock_log {
		unsigned long long time;
		unsigned long sp;
		unsigned long long jiffies;
		raw_spinlock_t *lock;
		u16 next;
		u16 owner;
		int en;
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} spinlock[ESS_NR_CPUS][ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_IRQ_DISABLED
	struct __irqs_disabled_log {
		unsigned long long time;
		unsigned long index;
		struct task_struct *task;
		char *task_comm;
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} irqs_disabled[ESS_NR_CPUS][SZ_32];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
	struct __clk_log {
		unsigned long long time;
		struct clk_hw *clk;
		const char* f_name;
		int mode;
		unsigned long arg;
	} clk[ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_PMU
	struct __pmu_log {
		unsigned long long time;
		unsigned int id;
		const char* f_name;
		int mode;
	} pmu[ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_FREQ
	struct __freq_log {
		unsigned long long time;
		int cpu;
		char* freq_name;
		unsigned long old_freq;
		unsigned long target_freq;
		int en;
	} freq[ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_DM
	struct __dm_log {
		unsigned long long time;
		int cpu;
		int dm_num;
		unsigned long min_freq;
		unsigned long max_freq;
		s32 wait_dmt;
		s32 do_dmt;
	} dm[ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REG
	struct __reg_log {
		unsigned long long time;
		int read;
		size_t val;
		size_t reg;
		int en;
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} reg[ESS_NR_CPUS][ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_HRTIMER
	struct __hrtimer_log {
		unsigned long long time;
		unsigned long long now;
		struct hrtimer *timer;
		void *fn;
		int en;
	} hrtimers[ESS_NR_CPUS][ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_REGULATOR
	struct __regulator_log {
		unsigned long long time;
		unsigned long long acpm_time;
		int cpu;
		char name[SZ_16];
		unsigned int reg;
		unsigned int voltage;
		unsigned int raw_volt;
		int en;
	} regulator[ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_THERMAL
	struct __thermal_log {
		unsigned long long time;
		int cpu;
		struct exynos_tmu_platform_data *data;
		unsigned int temp;
		char* cooling_device;
		unsigned int cooling_state;
	} thermal[ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_ACPM
	struct __acpm_log {
		unsigned long long time;
		unsigned long long acpm_time;
		char log[9];
		unsigned int data;
	} acpm[ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_UART
	struct __uart_log {
		unsigned long long time;
		int cpu;
		struct s3c24xx_uart_port *ourport;
		int flag;
		int en;
} uart[ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_I2C
	struct __i2c_log {
		unsigned long long time;
		int cpu;
		struct i2c_adapter *adap;
		struct i2c_msg *msgs;
		int num;
		int en;
	} i2c[ESS_LOG_MAX_NUM];
#endif
#ifdef CONFIG_EXYNOS_SNAPSHOT_SPI
	struct __spi_log {
		unsigned long long time;
		int cpu;
		struct spi_master *master;
		struct spi_message *cur_msg;
		int en;
	} spi[ESS_LOG_MAX_NUM];
#endif

#ifndef CONFIG_EXYNOS_SNAPSHOT_MINIMIZED_MODE
	struct __clockevent_log {
		unsigned long long time;
		unsigned long long mct_cycle;
		int64_t	delta_ns;
		ktime_t	next_event;
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} clockevent[ESS_NR_CPUS][ESS_LOG_MAX_NUM];

	struct __printkl_log {
		unsigned long long time;
		int cpu;
		size_t msg;
		size_t val;
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} printkl[ESS_API_MAX_NUM];

	struct __printk_log {
		unsigned long long time;
		int cpu;
		char log[ESS_LOG_STRING_LENGTH];
		void *caller[ESS_CALLSTACK_MAX_NUM];
	} printk[ESS_API_MAX_NUM];
#endif
};

typedef int (*ess_initcall_t)(const struct device_node *);

struct exynos_ss_base {
	size_t size;
	size_t vaddr;
	size_t paddr;
	unsigned int persist;
	unsigned int enabled;
};

struct exynos_ss_item {
	char *name;
	struct exynos_ss_base entry;
	unsigned char *head_ptr;
	unsigned char *curr_ptr;
	unsigned long long time;
};

#ifdef CONFIG_ARM64
struct exynos_ss_mmu_reg {
	long SCTLR_EL1;
	long TTBR0_EL1;
	long TTBR1_EL1;
	long TCR_EL1;
	long ESR_EL1;
	long FAR_EL1;
	long CONTEXTIDR_EL1;
	long TPIDR_EL0;
	long TPIDRRO_EL0;
	long TPIDR_EL1;
	long MAIR_EL1;
	long ELR_EL1;
	long SP_EL0;
};

#else
struct exynos_ss_mmu_reg {
	int SCTLR;
	int TTBR0;
	int TTBR1;
	int TTBCR;
	int DACR;
	int DFSR;
	int DFAR;
	int IFSR;
	int IFAR;
	int DAFSR;
	int IAFSR;
	int PMRRR;
	int NMRRR;
	int FCSEPID;
	int CONTEXT;
	int URWTPID;
	int UROTPID;
	int POTPIDR;
};
#endif

struct exynos_ss_sfrdump {
	char *name;
	void __iomem *reg;
	unsigned int phy_reg;
	unsigned int num;
	struct device_node *node;
	struct list_head list;
	bool pwr_mode;
};

struct exynos_ss_desc {
	struct list_head sfrdump_list;
	raw_spinlock_t ctrl_lock;
	raw_spinlock_t nmi_lock;
	unsigned int header_num;
	unsigned int kevents_num;
	unsigned int log_kernel_num;
	unsigned int log_platform_num;
	unsigned int log_sfr_num;
	unsigned int log_pstore_num;
	unsigned int log_etm_num;
	unsigned int log_enable_cnt;

	unsigned int callstack;
	unsigned long hardlockup_core_mask;
	unsigned long hardlockup_core_pc[ESS_NR_CPUS];
	int multistage_wdt_irq;
	int hardlockup_detected;
	int allcorelockup_detected;
	int no_wdt_dev;
	int debug_level;

	struct vm_struct vm;
};

extern struct exynos_ss_base ess_base;
extern struct exynos_ss_log *ess_log;
extern struct exynos_ss_desc ess_desc;
extern struct exynos_ss_item ess_items[];
extern int exynos_ss_log_size;
#endif
