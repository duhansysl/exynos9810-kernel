/*
* Samsung debugging features for Samsung's SoC's.
*
* Copyright (c) 2014-2017 Samsung Electronics Co., Ltd.
*      http://www.samsung.com
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*/

#ifndef SEC_DEBUG_H
#define SEC_DEBUG_H

#include <linux/sizes.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/reboot.h>

#define SEC_DEBUG_MAGIC_PA memblock_start_of_DRAM()
#define SEC_DEBUG_MAGIC_VA phys_to_virt(SEC_DEBUG_MAGIC_PA)
#define SEC_DEBUG_EXTRA_INFO_VA (SEC_DEBUG_MAGIC_VA + 0x400)
#define SEC_DEBUG_DUMPER_LOG_VA (SEC_DEBUG_MAGIC_VA + 0x800)
#define BUF_SIZE_MARGIN (SZ_1K - 0x80)

#ifdef CONFIG_SEC_DEBUG
extern int id_get_asb_ver(void);
extern int id_get_product_line(void);
extern void sec_debug_reboot_handler(void *p);
extern void sec_debug_panic_handler(void *buf, bool dump);
extern void sec_debug_post_panic_handler(void);

extern int sec_debug_get_debug_level(void);
extern int sec_debug_enter_upload(void);
extern void sec_debug_disable_printk_process(void);
extern char *verbose_reg(int cpu_type, int reg_name, unsigned long reg_val);
/* getlog support */
extern void sec_getlog_supply_kernel(void *klog_buf);
extern void sec_getlog_supply_platform(unsigned char *buffer, const char *name);
extern void sec_gaf_supply_rqinfo(unsigned short curr_offset, unsigned short rq_offset);
#else
#define id_get_asb_ver()			(-1)
#define id_get_product_line()			(-1)
#define sec_debug_reboot_handler(a)		do { } while (0)
#define sec_debug_panic_handler(a, b)		do { } while (0)
#define sec_debug_post_panic_handler()		do { } while (0)

#define sec_debug_get_debug_level()		(0)
#define sec_debug_disable_printk_process()	do { } while (0)
#define sec_debug_verbose_reg(a, b, c)		do { } while (0)

#define sec_getlog_supply_kernel(a)		do { } while (0)
#define sec_getlog_supply_platform(a, b)	do { } while (0)

#define sec_gaf_supply_rqinfo(a, b)		do { } while (0)
#endif /* CONFIG_SEC_DEBUG */

#ifdef CONFIG_SEC_DEBUG
// Meerkat
#define SHIFT_ADDRVALID		(3)
#define	MASK_ADDRVALID		(0b1)
#define SHIFT_UNCONTAINED	(2)
#define	MASK_UNCONTAINED	(0b1)
#define SHIFT_UC		(1)
#define	MASK_UC			(0b1)
#define SHIFT_VALID		(0)
#define	MASK_VALID		(0b1)
#define SHIFT_ADDRINDEX_FE	(0)
#define	MASK_ADDRINDEX_FE	(0b11111111)
#define SHIFT_ADDRINDEX_LS	(0)
#define	MASK_ADDRINDEX_LS	(0b1111111111111)
#define SHIFT_ADDRINDEX_TBW	(0)
#define	MASK_ADDRINDEX_TBW	(0b1111111111)
#define SHIFT_ADDRINDEX_L2	(0)
#define	MASK_ADDRINDEX_L2	(0xFFFFFFFFFF)
#define SHIFT_ADDRINDEX_L3	(0)
#define	MASK_ADDRINDEX_L3	(0xFFFF)

// Cortex-A55
#define SHIFT_A			(31)
#define	MASK_A			(0b1)
#define SHIFT_IDS		(24)
#define	MASK_IDS		(0b1)
#define SHIFT_AET		(10)
#define	MASK_AET		(0b111)
#define SHIFT_DFSC		(0)
#define	MASK_DFSC		(0b111111)

#define SHIFT_V			(30)
#define	MASK_V			(0b1)
#define SHIFT_UE		(29)
#define	MASK_UE			(0b1)
#define SHIFT_ER		(28)
#define	MASK_ER			(0b1)
#define SHIFT_OF		(27)
#define	MASK_OF			(0b1)
#define SHIFT_MV		(26)
#define	MASK_MV			(0b1)
#define SHIFT_CE		(24)
#define	MASK_CE			(0b11)
#define SHIFT_DE		(23)
#define	MASK_DE			(0b1)
#define SHIFT_PN		(22)
#define	MASK_PN			(0b1)
#define SHIFT_IERR		(8)
#define	MASK_IERR		(0b11111111)
#define SHIFT_SERR		(8)
#define	MASK_SERR		(0b11111111)

#define SHIFT_OFO		(47)
#define	MASK_OFO		(0b1)
#define SHIFT_CECO		(40)
#define	MASK_CECO		(0b1111111)
#define SHIFT_OFR		(39)
#define	MASK_OFR		(0b1)
#define SHIFT_CECR		(32)
#define	MASK_CECR		(0b1111111)
#define SHIFT_WAY		(28)
#define	MASK_WAY		(0b1111)
#define SHIFT_INDX		(6)
#define	MASK_INDX		(0b1111111111111)
#define SHIFT_LVL		(1)
#define	MASK_LVL		(0b111)
#define SHIFT_IND		(0)
#define	MASK_IND		(0b1)

#define FIELD_VAL(r, f)	(((r) >> (SHIFT_##f)) & (MASK_##f))

enum sec_debug_lsi_reg_t {
	FEMERR0SR,
	FEMERR1SR,
	LSMERR0SR,
	LSMERR1SR,
	TBWMERR0SR,
	TBWMERR1SR,
	L2MERR0SR,
	L2MERR1SR,
	L3MERR0SR,
	L3MERR1SR,
};

enum sec_debug_arm_reg_t {
	DISR_EL1,
	ERR0STATUS,
	ERR0ADDR,
	ERR0MISC0,
	ERR1STATUS,
	ERR1ADDR,
	ERR1MISC0,
};

#endif

#ifdef CONFIG_SEC_DEBUG_RESET_REASON

enum sec_debug_reset_reason_t {
	RR_S = 1,
	RR_W = 2,
	RR_D = 3,
	RR_K = 4,
	RR_M = 5,
	RR_P = 6,
	RR_R = 7,
	RR_B = 8,
	RR_N = 9,
	RR_T = 10,
	RR_C = 11,
};

extern unsigned reset_reason;
#endif

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO

#define MAX_EXTRA_INFO_HDR_LEN	6
#define MAX_EXTRA_INFO_KEY_LEN	16
#define MAX_EXTRA_INFO_VAL_LEN	1024
#define SEC_DEBUG_BADMODE_MAGIC	0x6261646d

enum sec_debug_extra_buf_type {
	INFO_AID,
	INFO_KTIME,
	INFO_BIN,
	INFO_FTYPE,
	INFO_FAULT,
	INFO_BUG,
	INFO_PANIC,
	INFO_PC,
	INFO_LR,
	INFO_STACK,
	INFO_REASON,
	INFO_PINFO,
	INFO_SYSMMU,
	INFO_BUSMON,
	INFO_DPM,
	INFO_SMPL,
	INFO_RSTCNT,
	INFO_ETC,
	INFO_ESR,
	INFO_MERR,
	INFO_PCB,
	INFO_SMD,
	INFO_CHI,
	INFO_LPI,
	INFO_CDI,
	INFO_KLG,
	INFO_HINT,
	INFO_LEVEL,
	INFO_DECON,
	INFO_WAKEUP,
	INFO_BATT,
	INFO_MAX_A,

	INFO_BID = INFO_MAX_A,
	INFO_BREASON,
	INFO_ASB,
	INFO_PSITE,
	INFO_DDRID,
	INFO_RST,
	INFO_INFO2,
	INFO_INFO3,
	INFO_RBASE,
	INFO_MAGIC,
	INFO_PWRSRC,
	INFO_PWROFF,
	INFO_PINT1,
	INFO_PINT2,
	INFO_PINT5,
	INFO_PINT6,
	INFO_PSTS1,
	INFO_PSTS2,
	INFO_RVD1,
	INFO_RVD2,
	INFO_RVD3,
	INFO_MAX_B,

	INFO_CID = INFO_MAX_B,
	INFO_CREASON,
	INFO_CPU0,
	INFO_CPU1,
	INFO_CPU2,
	INFO_CPU3,
	INFO_CPU4,
	INFO_CPU5,
	INFO_CPU6,
	INFO_CPU7,
	INFO_FRQL0,
	INFO_FRQL1,
	INFO_FRQL2,
	INFO_FRQB0,
	INFO_FRQB1,
	INFO_FRQB2,
	INFO_FRQM0,
	INFO_FRQM1,
	INFO_FRQM2,
	INFO_FRQI0,
	INFO_FRQI1,
	INFO_FRQI2,
	INFO_MAX_C,

	INFO_MID = INFO_MAX_C,
	INFO_MREASON,
	INFO_MFC,
	INFO_MAX_M,

	INFO_MAX = INFO_MAX_M,
};

struct sec_debug_extra_info_item {
	char key[MAX_EXTRA_INFO_KEY_LEN];
	char val[MAX_EXTRA_INFO_VAL_LEN];
	unsigned int max;
};

struct sec_debug_panic_extra_info {
	struct sec_debug_extra_info_item item[INFO_MAX];
};

#define PMUDBG_MAX	50
struct sec_debug_pmudbg_extra_info {
	struct sec_debug_extra_info_item item[PMUDBG_MAX];
};

enum sec_debug_extra_fault_type {
	UNDEF_FAULT,                /* 0 */
	BAD_MODE_FAULT,             /* 1 */
	WATCHDOG_FAULT,             /* 2 */
	KERNEL_FAULT,               /* 3 */
	MEM_ABORT_FAULT,            /* 4 */
	SP_PC_ABORT_FAULT,          /* 5 */
	PAGE_FAULT,		    /* 6 */
	ACCESS_USER_FAULT,          /* 7 */
	EXE_USER_FAULT,             /* 8 */
	ACCESS_USER_OUTSIDE_FAULT,  /* 9 */
	BUG_FAULT,                  /* 10 */
	PANIC_FAULT,
	FAULT_MAX,
};

#endif

#if defined(CONFIG_SAMSUNG_VST_CAL)
extern int volt_vst_cal_bdata;
#endif

#define SEC_DEBUG_SHARED_MAGIC0 0xFFFFFFFF
#define SEC_DEBUG_SHARED_MAGIC1 0x95308180
#define SEC_DEBUG_SHARED_MAGIC2 0x14F014F0
#define SEC_DEBUG_SHARED_MAGIC3 0x00010001

struct sec_debug_ksyms {
	uint32_t magic;
	uint32_t kallsyms_all;
	uint64_t addresses_pa;
	uint64_t names_pa;
	uint64_t num_syms;
	uint64_t token_table_pa;
	uint64_t token_index_pa;
	uint64_t markers_pa;
	struct ksect {
		uint64_t sinittext;
		uint64_t einittext;
		uint64_t stext;
		uint64_t etext;
		uint64_t end;
	} sect;
	uint64_t relative_base;
	uint64_t offsets_pa;
	uint64_t kimage_voffset;
};

struct sec_debug_shared_info {
	/* initial magic code */
	unsigned int magic[4];

	/* ksymbol information */
	struct sec_debug_ksyms ksyms;

#ifdef CONFIG_SEC_DEBUG
	/* reset reason extra info for bigdata */
	struct sec_debug_panic_extra_info sec_debug_extra_info;

	/* reset reason extra info for bigdata */
	struct sec_debug_panic_extra_info sec_debug_extra_info_backup;

	/* reset reason extrf info for bigdata */
	struct sec_debug_pmudbg_extra_info sec_debug_extra_info_pmudbg;
#endif

	/* last 1KB of kernel log */
	char last_klog[SZ_1K];
};

extern void sec_debug_set_kallsyms_info(struct sec_debug_ksyms *ksyms, int magic);

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO

extern struct exynos_chipid_info exynos_soc_info;
extern unsigned int get_smpl_warn_number(void);

extern void sec_debug_init_extra_info(struct sec_debug_shared_info *, int magic_status);
extern void sec_debug_finish_extra_info(void);
extern void sec_debug_store_extra_info(int start, int end);
extern void sec_debug_store_extra_info_A(void);
extern void sec_debug_store_extra_info_B(void);
extern void sec_debug_store_extra_info_C(void);
extern void sec_debug_store_extra_info_M(void);
extern void sec_debug_store_extra_info_F(void);
extern void sec_debug_set_extra_info_id(void);
extern void sec_debug_set_extra_info_ktime(void);
extern void sec_debug_set_extra_info_fault(enum sec_debug_extra_fault_type,
									unsigned long addr, struct pt_regs *regs);
extern void sec_debug_set_extra_info_bug(const char *file, unsigned int line);
extern void sec_debug_set_extra_info_panic(char *str);
extern void sec_debug_set_extra_info_backtrace(struct pt_regs *regs);
extern void sec_debug_set_extra_info_backtrace_cpu(struct pt_regs *regs, int cpu);
extern void sec_debug_set_extra_info_evt_version(void);
extern void sec_debug_set_extra_info_sysmmu(char *str);
extern void sec_debug_set_extra_info_busmon(char *str);
extern void sec_debug_set_extra_info_dpm_timeout(char *devname);
extern void sec_debug_set_extra_info_smpl(unsigned long count);
extern void sec_debug_set_extra_info_esr(unsigned int esr);
extern void sec_debug_set_extra_info_merr(char *merr);
extern void sec_debug_set_extra_info_hint(u64 hint);
extern void sec_debug_set_extra_info_decon(unsigned int err);
extern void sec_debug_set_extra_info_batt(int cap, int volt, int temp, int curr);
extern void sec_debug_set_extra_info_ufs_error(char *str);
extern void sec_debug_set_extra_info_zswap(char *str);
extern void sec_debug_set_extra_info_mfc_error(char *str);
extern void sec_debug_set_extra_info_rvd1(char *str);
extern void sec_debug_set_extra_info_rvd2(char *str);
extern void sec_debug_set_extra_info_rvd3(char *str);

#else

#define sec_debug_init_extra_info(a, b)	do { } while (0)
#define sec_debug_finish_extra_info()	do { } while (0)
#define sec_debug_store_extra_info(a, b)	do { } while (0)
#define sec_debug_store_extra_info_A()		do { } while (0)
#define sec_debug_store_extra_info_C()		do { } while (0)
#define sec_debug_store_extra_info_M()		do { } while (0)
#define sec_debug_store_extra_info_F()		do { } while (0)
#define sec_debug_set_extra_info_id()	do { } while (0)
#define sec_debug_set_extra_info_ktime()	do { } while (0)
#define sec_debug_set_extra_info_fault(a, b, c)	do { } while (0)
#define sec_debug_set_extra_info_bug(a, b)	do { } while (0)
#define sec_debug_set_extra_info_panic(a)	do { } while (0)
#define sec_debug_set_extra_info_backtrace(a)	do { } while (0)
#define sec_debug_set_extra_info_backtrace_cpu(a, b)	do { } while (0)
#define sec_debug_set_extra_info_evt_version()	do { } while (0)
#define sec_debug_set_extra_info_sysmmu(a)	do { } while (0)
#define sec_debug_set_extra_info_busmon(a)	do { } while (0)
#define sec_debug_set_extra_info_dpm_timeout(a)	do { } while (0)
#define sec_debug_set_extra_info_smpl(a)	do { } while (0)
#define sec_debug_set_extra_info_esr(a)		do { } while (0)
#define sec_debug_set_extra_info_merr(a)	do { } while (0)
#define sec_debug_set_extra_info_hint(a)	do { } while (0)
#define sec_debug_set_extra_info_decon(a)	do { } while (0)
#define sec_debug_set_extra_info_batt(a, b, c, d)	do { } while (0)
#define sec_debug_set_extra_info_ufs_error(a)	do { } while (0)
#define sec_debug_set_extra_info_zswap(a)	do { } while (0)
#define sec_debug_set_extra_info_mfc_error(a)	do { } while (0)

#endif /* CONFIG_SEC_DEBUG_EXTRA_INFO */

#ifdef CONFIG_SEC_DEBUG_AUTO_SUMMARY
extern void sec_debug_auto_summary_log_disable(int type);
extern void sec_debug_auto_summary_log_once(int type);
extern void register_set_auto_comm_buf(void (*func)(int type, const char *buf, size_t size));
extern void register_set_auto_comm_lastfreq(void (*func)(int type, int old_freq, int new_freq, u64 time, int en));
#endif

#ifdef CONFIG_SEC_DEBUG_PPMPU
extern void print_ppmpu_protection(struct pt_regs *regs);
#else
static inline void print_ppmpu_protection(struct pt_regs *regs) {}
#endif

#ifdef CONFIG_SEC_DEBUG_INIT_LOG
extern void register_init_log_hook_func(void (*func)(const char *buf, size_t size));
#endif

#ifdef CONFIG_SEC_DEBUG_BRANCH_VERIFIER
enum br_state {
	BRSTATE_INIT,
	BRSTATE_PCLR,
	BRSTATE_NORMAL,
	BRSTATE_NORMAL_PC,
};

struct branch_info {
	unsigned long lr;
	unsigned long regs_lr;
	unsigned long regs_fp;
	int state;
	int log_pos;
};

extern void init_branch_info(struct branch_info *info);
extern void pre_check_backtrace_auto_summary(struct branch_info *info, unsigned long where, unsigned long fp);
extern void check_backtrace(struct branch_info *info, unsigned long where, unsigned long fp, unsigned int fromirq, struct pt_regs *regs);
extern void check_backtrace_auto_summary(struct branch_info *info, unsigned long where, unsigned long fp, unsigned int fromirq, struct pt_regs *regs);
#endif

#ifdef CONFIG_SEC_DEBUG_LAST_KMSG
#define SEC_LKMSG_MAGICKEY 0x0000000a6c6c7546
extern void sec_debug_save_last_kmsg(unsigned char *head_ptr, unsigned char *curr_ptr, size_t buf_size);
#else
#define sec_debug_save_last_kmsg(a, b, c)		do { } while (0)
#endif /* CONFIG_SEC_DEBUG_LAST_KMSG */

/*
 * Samsung TN Logging Options
 */

/**
 * sec_debug_tsp_log : Leave tsp log in tsp_msg file.
 * ( Timestamp + Tsp logs )
 * sec_debug_tsp_log_msg : Leave tsp log in tsp_msg file and
 * add additional message between timestamp and tsp log.
 * ( Timestamp + additional Message + Tsp logs )
 */
#ifdef CONFIG_SEC_DEBUG_TSP_LOG
extern void sec_debug_tsp_log(char *fmt, ...);
extern void sec_debug_tsp_log_msg(char *msg, char *fmt, ...);
extern void sec_debug_tsp_raw_data(char *fmt, ...);
extern void sec_debug_tsp_raw_data_msg(char *msg, char *fmt, ...);
extern void sec_tsp_raw_data_clear(void);
#else
#define sec_debug_tsp_log(a, ...)		do { } while (0)
#define sec_debug_tsp_log_msg(a, b, ...)		do { } while (0)
#define sec_debug_tsp_raw_data(a, ...)			do { } while (0)
#define sec_debug_tsp_raw_data_msg(a, b, ...)		do { } while (0)
#define sec_tsp_raw_data_clear()			do { } while (0)
#endif /* CONFIG_SEC_DEBUG_TSP_LOG */

#ifdef CONFIG_TOUCHSCREEN_DUMP_MODE
struct tsp_dump_callbacks {
	void (*inform_dump)(void);
};
#endif

extern unsigned int get_smpl_warn_number(void);
extern void (*mach_restart)(enum reboot_mode mode, const char *cmd);
extern int sec_debug_force_error(const char *val, struct kernel_param *kp);

#ifdef CONFIG_SEC_DUMP_SUMMARY

#define SEC_DEBUG_SUMMARY_MAGIC0 0xFFFFFFFF
#define SEC_DEBUG_SUMMARY_MAGIC1 0x5ECDEB6
#define SEC_DEBUG_SUMMARY_MAGIC2 0x14F014F0
#define SEC_DEBUG_SUMMARY_MAGIC3 0x00010004

#define DUMP_SUMMARY_MAX_SIZE	(0x300000)
#define SCHED_LOG_MAX 512

struct irq_log {
	unsigned long long time;
	int irq;
	void *fn;
	int en;
	int preempt_count;
	void *context;
};

struct irq_exit_log {
	unsigned int irq;
	unsigned long long time;
	unsigned long long end_time;
	unsigned long long elapsed_time;
};

struct sched_log {
	unsigned long long time;
	char comm[TASK_COMM_LEN];
	pid_t pid;
	struct task_struct *pTask;
};

struct sec_debug_summary_corelog {
	atomic_t idx_irq[CONFIG_NR_CPUS];
	struct irq_log irq[CONFIG_NR_CPUS][SCHED_LOG_MAX];
	atomic_t idx_irq_exit[CONFIG_NR_CPUS];
	struct irq_exit_log irq_exit[CONFIG_NR_CPUS][SCHED_LOG_MAX];
	atomic_t idx_sched[CONFIG_NR_CPUS];
	struct sched_log sched[CONFIG_NR_CPUS][SCHED_LOG_MAX];
};

struct sec_debug_summary_sched_log {
	uint64_t sched_idx_paddr;
	uint64_t sched_buf_paddr;
	unsigned int sched_struct_sz;
	unsigned int sched_array_cnt;
	uint64_t irq_idx_paddr;
	uint64_t irq_buf_paddr;
	unsigned int irq_struct_sz;
	unsigned int irq_array_cnt;
	uint64_t irq_exit_idx_paddr;
	uint64_t irq_exit_buf_paddr;
	unsigned int irq_exit_struct_sz;
	unsigned int irq_exit_array_cnt;
};

struct sec_debug_summary_kernel_log {
	uint64_t first_idx_paddr;
	uint64_t next_idx_paddr;
	uint64_t log_paddr;
	uint64_t size_paddr;
};

struct sec_debug_summary_excp_kernel {
	char pc_sym[64];
	char lr_sym[64];
	char panic_caller[64];
	char panic_msg[128];
	char thread[32];
};

struct sec_debug_summary_cpu_info {
	char policy_name[16];
	int freq_min;
	int freq_max;
	int freq_cur;
};

struct sec_debug_summary_data_kernel {
	char name[16];
	char state[16];
	int nr_cpus;

	struct sec_debug_summary_excp_kernel excp;
	struct sec_debug_summary_cpu_info cpu_info[CONFIG_NR_CPUS];
};

struct sec_debug_summary {
	unsigned int magic[4];

	unsigned long reserved_out_buf;
	unsigned long reserved_out_size;

	char summary_cmdline[2048];
	char summary_linuxbanner[1024];

	struct sec_debug_summary_data_kernel kernel;
	struct sec_debug_summary_corelog sched_log;
	struct sec_debug_ksyms ksyms;
};

extern void sec_debug_task_sched_log_short_msg(char *msg);
extern void sec_debug_task_sched_log(int cpu, struct task_struct *task);
extern void sec_debug_irq_sched_log(unsigned int irq, void *fn, int en);
extern void sec_debug_irq_enterexit_log(unsigned int irq,
						unsigned long long start_time);
extern void sec_debug_set_kallsyms_info(struct sec_debug_ksyms *ksyms, int magic);
extern int sec_debug_check_sj(void);

int sec_debug_save_cpu_info(void);
int sec_debug_save_die_info(const char *str, struct pt_regs *regs);
int sec_debug_save_panic_info(const char *str, unsigned long caller);
#endif

#endif /* SEC_DEBUG_H */
