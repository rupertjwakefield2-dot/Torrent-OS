#include "idt.h"
#include "print.h"
#include "io.h"

/* ── IDT table ───────────────────────────────────────────── */

static IDTEntry  idt[256];
static IDTPointer idt_ptr;

extern uint64_t isr_stubs[48];  /* defined in isr.asm */

static void idt_set_gate(uint8_t n, uint64_t handler) {
    idt[n].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[n].selector    = 0x08;   /* kernel code segment */
    idt[n].ist         = 0;
    idt[n].type_attr   = 0x8E;   /* present, DPL=0, 64-bit interrupt gate */
    idt[n].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[n].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[n].zero        = 0;
}

/* ── 8259A PIC remapping ─────────────────────────────────── */

static void pic_remap(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11); io_wait();  /* init */
    outb(0x21, 0x20); outb(0xA1, 0x28); io_wait();  /* vector offsets */
    outb(0x21, 0x04); outb(0xA1, 0x02); io_wait();  /* cascade wiring */
    outb(0x21, 0x01); outb(0xA1, 0x01); io_wait();  /* 8086 mode */
    /* unmask IRQ0 (timer) and IRQ1 (keyboard), mask everything else */
    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
}

/* ── init ────────────────────────────────────────────────── */

void idt_init(void) {
    for (int i = 0; i < 256; i++) idt[i] = (IDTEntry){0};
    for (int i = 0; i < 48;  i++) idt_set_gate((uint8_t)i, isr_stubs[i]);

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (uint64_t)&idt;
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));

    pic_remap();
}

/* ── exception names ─────────────────────────────────────── */

static const char *exc_name[32] = {
    "Divide-by-Zero",       "Debug",
    "NMI",                  "Breakpoint",
    "Overflow",             "Bound Range Exceeded",
    "Invalid Opcode",       "Device Not Available",
    "Double Fault",         "Coprocessor Segment Overrun",
    "Invalid TSS",          "Segment Not Present",
    "Stack-Segment Fault",  "General Protection Fault",
    "Page Fault",           "Reserved",
    "x87 FP Exception",     "Alignment Check",
    "Machine Check",        "SIMD FP Exception",
    "Virtualization",       "Control Protection",
    "Reserved",             "Reserved",
    "Reserved",             "Reserved",
    "Reserved",             "Reserved",
    "Hypervisor Injection", "VMM Communication",
    "Security Exception",   "Reserved",
};

/* ── dispatch: called from isr_common in isr.asm ────────── */

void interrupt_handler(InterruptFrame *f) {
    if (f->int_no < 32) {
        /* CPU exception — print a panic screen and halt */
        print_set_color(COLOR_WHITE, COLOR_RED);
        print_clear_row(0, COLOR_WHITE, COLOR_RED);  /* overwrite status bar */
        print_clear();
        print_str("  TORRENTOS KERNEL PANIC\n\n");
        print_str("  Exception : "); print_str(exc_name[f->int_no]); print_newline();
        print_str("  Vector    : "); print_uint(f->int_no);           print_newline();
        print_str("  Error     : "); print_hex(f->err_code);          print_newline();
        print_str("  RIP       : "); print_hex(f->rip);               print_newline();
        print_str("  RSP       : "); print_hex(f->rsp);               print_newline();
        print_str("  RFLAGS    : "); print_hex(f->rflags);            print_newline();
        print_str("\n  System halted. Reset to continue.\n");
        cpu_cli();
        for (;;) cpu_halt();
    }

    /* IRQ dispatch */
    uint8_t irq = (uint8_t)(f->int_no - 32);

    if (irq == 0) {
        extern void timer_tick(void);
        timer_tick();
    } else if (irq == 1) {
        extern void keyboard_irq(void);
        keyboard_irq();
    }

    /* send EOI */
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}
