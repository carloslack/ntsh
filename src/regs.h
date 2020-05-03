#ifndef __REGS_H
#define __REGS_H

#include <linux/preempt.h>

/*
 * Need to write into cr0 directly since the
 * Pin sensitive CR0 bits commit in the kernel.
 * See kernel/cpu/common.c
 */
static inline void __write_cr0(unsigned long val) {
    unsigned long __force_order;
    asm volatile("mov %0,%%cr0": "+r" (val), "+m" (__force_order));
}

static inline void ntsh_make_page_rw(void) {
    unsigned long cr0;

    preempt_disable();
    barrier();
    cr0 = read_cr0() ^ X86_CR0_WP;
    BUG_ON(unlikely(cr0 & X86_CR0_WP));
    __write_cr0(cr0);
    barrier();
    preempt_enable_notrace();
}

static inline void ntsh_make_page_ro(void) {
    unsigned long cr0;

    preempt_disable();
    barrier();
    cr0 = read_cr0() ^ X86_CR0_WP;
    BUG_ON(unlikely(!(cr0 & X86_CR0_WP)));
    __write_cr0(cr0);
    barrier();
    preempt_enable_notrace();
}
#endif

