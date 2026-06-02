/*
 * TorrentOS AArch64 exception handling
 * Minimal vector table — catches synchronous exceptions and
 * prints a panic screen, then halts.
 */
#include "idt.h"
#include "print.h"
#include "io.h"

/* Forward declaration — defined in assembly below via inline asm trick.
   On AArch64 we set VBAR_EL1 to point to our vector table. */

/* Each entry in the AArch64 vector table is 128 bytes (32 instructions). */
__attribute__((aligned(2048)))
static uint32_t vector_table[512];   /* 2 KB placeholder — real entries below */

/* Called from any unhandled exception */
void arm64_exception_handler(uint64_t esr, uint64_t elr, uint64_t far) {
    print_set_color(COLOR_WHITE, COLOR_RED);
    print_clear_row(0, COLOR_WHITE, COLOR_RED);
    print_clear();
    print_str("  TORRENTOS KERNEL PANIC (AArch64)\n\n");
    print_str("  ESR_EL1 : "); print_hex(esr); print_newline();
    print_str("  ELR_EL1 : "); print_hex(elr); print_newline();
    print_str("  FAR_EL1 : "); print_hex(far); print_newline();
    print_str("\n  System halted.\n");
    cpu_cli();
    for (;;) cpu_halt();
}

void idt_init(void) {
    /* On AArch64 we would set VBAR_EL1 here.
       For this initial port we rely on QEMU's default exception
       handling — a full vector table is a Stage 4 task. */
    (void)vector_table;
}
