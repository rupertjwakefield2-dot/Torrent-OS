#pragma once
#include <stdint.h>

/* Linux-compatible syscall numbers (subset) */
#define SYS_READ    0
#define SYS_WRITE   1
#define SYS_OPEN    2
#define SYS_CLOSE   3
#define SYS_GETPID  39
#define SYS_EXIT    60
#define SYS_YIELD   24    /* custom: voluntarily yield CPU */

/* Initialise SYSCALL/SYSRET MSRs. */
void syscall_init(void);

/* C-level dispatch — called from syscall_entry.asm.
 * Returns the syscall return value in RAX. */
uint64_t syscall_dispatch(uint64_t num,
                          uint64_t a1, uint64_t a2, uint64_t a3,
                          uint64_t a4, uint64_t a5, uint64_t a6);
