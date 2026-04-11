#define ALIGN8 __attribute__((aligned(8)))

ALIGN8 unsigned long __debug_sj_lock = 0;
ALIGN8 unsigned long dump_callbacks = 0;
ALIGN8 void exynos_ss_uart(void) {}
ALIGN8 void exynos_ss_save_log(void) {}
ALIGN8 int sec_debug_save_die_info(const char *str, void *regs) { return 0; }
ALIGN8 void print_ppmpu_protection(void *regs) {}
ALIGN8 int sec_debug_save_panic_info(const char *str, unsigned long caller) { return 0; }
ALIGN8 void sec_debug_irq_sched_log(unsigned int irq, void *fn, int en) {}
ALIGN8 int s3c2410wdt_set_emergency_reset(unsigned int timeout, int index) { return 0; }
ALIGN8 void sec_debug_set_extra_info_rvd1(void) {}
ALIGN8 int trace_puts(const char *str) { return 0; }
// ALIGN8 int tracing_mark_write(const char *buf, unsigned int count) { return 0; }