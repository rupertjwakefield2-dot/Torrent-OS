/*
 * TorrentOS AArch64 bootstrap
 * QEMU -machine virt loads us at EL1 and jumps to _start.
 */

.section ".text.boot"
.global  _start
_start:
    /* disable FIQ, IRQ, SError, Debug */
    msr     daifset, #0xF

    /* only run on CPU 0 */
    mrs     x0, mpidr_el1
    and     x0, x0, #0xFF
    cbnz    x0, .hang

    /* set stack pointer — 16-byte aligned, grows down */
    adr     x0, stack_top
    mov     sp, x0

    /* zero BSS */
    adr     x1, __bss_start
    adr     x2, __bss_end
    cmp     x1, x2
    b.ge    .bss_done
.bss_loop:
    str     xzr, [x1], #8
    cmp     x1, x2
    b.lt    .bss_loop
.bss_done:

    /* jump to C kernel */
    bl      kernel_main

.hang:
    wfi
    b       .hang

/* ── stack (16 KB) ── */
.section ".bss"
.align 16
.space 16384
stack_top:
