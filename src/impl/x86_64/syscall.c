#include "syscall.h"
#include "proc.h"
#include "print.h"
#include "str.h"
#include "io.h"

/*
 * Syscall subsystem.
 *
 * syscall_init() programs the three MSRs that control SYSCALL/SYSRET:
 *   IA32_STAR  (0xC0000081) — segment selectors for SYSCALL and SYSRET
 *   IA32_LSTAR (0xC0000082) — 64-bit RIP for syscall entry point
 *   IA32_FMASK (0xC0000084) — RFLAGS mask applied on SYSCALL entry
 *
 * The entry stub (syscall_entry.asm) saves caller-saved registers and
 * calls syscall_dispatch().
 *
 * Calling convention (Linux-compatible):
 *   RAX = syscall number   return value → RAX
 *   RDI, RSI, RDX, R10, R8, R9 = args 1-6
 *   RCX and R11 are clobbered by the SYSCALL instruction itself.
 */

#define MSR_EFER  0xC0000080
#define MSR_STAR  0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_FMASK 0xC0000084

#define EFER_SCE  (1u)    /* System Call Extensions enable bit */

/* Kernel CS = 0x08, User CS = 0x18, User SS = 0x20 (planned GDT layout) */
#define STAR_KERNEL_CS  0x08
#define STAR_USER_CS    0x18   /* SYSRET sets CS to this + 16 */

extern void syscall_entry(void);   /* defined in syscall_entry.asm */

static void write_msr(uint32_t msr, uint64_t val) {
    __asm__ volatile("wrmsr"
        : : "c"(msr), "a"((uint32_t)val), "d"((uint32_t)(val >> 32)));
}
static uint64_t read_msr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

void syscall_init(void) {
    /* Enable SCE (System Call Extensions) in EFER */
    write_msr(MSR_EFER, read_msr(MSR_EFER) | EFER_SCE);

    /* STAR: bits 63:48 = SYSRET CS/SS base, bits 47:32 = SYSCALL CS/SS base */
    uint64_t star = ((uint64_t)STAR_USER_CS << 48)
                  | ((uint64_t)STAR_KERNEL_CS << 32);
    write_msr(MSR_STAR, star);

    /* LSTAR: entry point for 64-bit SYSCALL */
    write_msr(MSR_LSTAR, (uint64_t)(uintptr_t)syscall_entry);

    /* FMASK: clear IF and DF on syscall entry (prevent interrupts, clear direction flag) */
    write_msr(MSR_FMASK, (1u << 9) | (1u << 10));
}

/* ── syscall implementations ── */

static uint64_t sys_write(uint64_t fd, uint64_t buf_ptr, uint64_t len) {
    (void)fd;   /* only stdout for now */
    const char *s = (const char*)(uintptr_t)buf_ptr;
    for (uint64_t i = 0; i < len; i++) print_char(s[i]);
    return len;
}

static uint64_t sys_getpid(void) {
    return (uint64_t)proc_getpid();
}

static uint64_t sys_yield(void) {
    proc_yield();
    return 0;
}

static uint64_t sys_exit(uint64_t code) {
    (void)code;
    Task *t = proc_current();
    if (t) t->state = TASK_DEAD;
    proc_yield();
    /* should not be reached */
    for (;;) cpu_halt();
    return 0;
}

/* ── dispatcher ── */

uint64_t syscall_dispatch(uint64_t num,
                          uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a4; (void)a5; (void)a6;
    switch (num) {
        case SYS_WRITE:  return sys_write(a1, a2, a3);
        case SYS_GETPID: return sys_getpid();
        case SYS_YIELD:  return sys_yield();
        case SYS_EXIT:   return sys_exit(a1);
        default:
            return (uint64_t)-1;   /* ENOSYS */
    }
}
