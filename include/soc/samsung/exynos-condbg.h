/*
 * drivers/trace/exynos-condbg.h
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Exynos-Console-Debugger for Exynos SoC
 * This codes are based on fiq_debugger of google
 * /driver/staging/android/fiq_debugger
 *
 * Author: Hosung Kim <hosung0.kim@samsung.com>
 *         Changki Kim <changki.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _EXYNOS_CONSOLE_DEBUGGER_H_
#define _EXYNOS_CONSOLE_DEBUGGER_H_

#ifdef CONFIG_EXYNOS_CONSOLE_DEBUGGER
extern void ecd_console_enable(bool enable);
extern void ecd_printf(const char *fmt, ...);
extern bool ecd_get_debug_panic(void);
extern bool ecd_get_enable(void);
extern int ecd_get_debug_mode(void);
extern void ecd_do_break_now(void);
extern int ecd_do_bad(unsigned long addr, struct pt_regs *regs);
extern int ecd_hook_ioremap(unsigned long paddr, unsigned long vaddr, unsigned int size);
extern void ecd_hook_iounmap(unsigned long vaddr);
#else
#define ecd_console_enable(a)			do { } while(0)
#define ecd_do_break_now()			do { } while(0)
#define ecd_hook_ioremap(a,b,c)			do { } while(0)
#define ecd_hook_iounmap(a)			do { } while(0)
static inline int ecd_do_bad(unsigned long addr, struct pt_regs *regs)
{
	return 1;
}
static inline void ecd_printf(const char *fmt, ...)
{
	return;
}
static inline bool ecd_get_debug_panic(void)
{
	return false;
}
static inline int ecd_get_debug_mode(void)
{
	return false;
}
static inline int ecd_get_enable(void)
{
	return false;
}
#endif
#ifdef CONFIG_S3C2410_WATCHDOG
#ifndef s3c2410wdt_set_emergency_reset
extern int s3c2410wdt_set_emergency_reset(unsigned int timeout, int index);
#endif
#else
static inline int s3c2410wdt_set_emergency_reset(unsigned int timeout, int index)
{
	return -1;
}
#endif
enum {
	MODE_NORMAL = 0,
	MODE_DEBUG,
	MODE_CONSOLE,
};
#endif
