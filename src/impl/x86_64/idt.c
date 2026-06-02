#include "idt.h"
#include "print.h"
#include "io.h"
#include "timer.h"
#include "keyboard.h"
#include "proc.h"
#include "lapic.h"

/* ── IDT table ── */
static IDTEntry   idt[256];
static IDTPointer idt_ptr;
extern uint64_t   isr_stubs[48];

static void idt_set_gate(uint8_t n, uint64_t handler) {
    idt[n].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt[n].selector    = 0x08;
    idt[n].ist         = 0;
    idt[n].type_attr   = 0x8E;   /* present, DPL=0, 64-bit interrupt gate */
    idt[n].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt[n].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt[n].zero        = 0;
}

/* ── 8259A PIC remap ── */
static void pic_remap(void) {
    outb(0x20, 0x11); outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); outb(0xA1, 0x01); io_wait();
    /* unmask IRQ0 (timer) + IRQ1 (keyboard), mask everything else */
    outb(0x21, 0xFC);
    outb(0xA1, 0xFF);
}

static void pic_eoi(uint8_t irq) {
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}

/* ── exception names ── */
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

void idt_init(void) {
    for (int i = 0; i < 256; i++) idt[i] = (IDTEntry){0};
    for (int i = 0; i < 48;  i++) idt_set_gate((uint8_t)i, isr_stubs[i]);

    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base  = (uint64_t)(uintptr_t)&idt;
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));

    pic_remap();
}

/* ── main interrupt dispatcher — returns new RSP or 0 ── */
uint64_t interrupt_handler(InterruptFrame *f) {
    uint64_t new_rsp = 0;

    if (f->int_no < 32) {
        /* ── CPU exception: red panic screen ── */
        print_set_color(COLOR_WHITE, COLOR_RED);
        print_clear_row(0, COLOR_WHITE, COLOR_RED);
        print_clear();
        print_str("  TORRENTOS KERNEL PANIC\n\n");
        print_str("  Exception : "); print_str(exc_name[f->int_no]); print_newline();
        print_str("  Vector    : "); print_uint(f->int_no);           print_newline();
        print_str("  Error     : "); print_hex(f->err_code);          print_newline();
        print_str("  RIP       : "); print_hex(f->rip);               print_newline();
        print_str("  RSP       : "); print_hex(f->rsp);               print_newline();
        print_str("  RFLAGS    : "); print_hex(f->rflags);            print_newline();
        print_str("  Task      : ");
        Task *ct = proc_current();
        if (ct) { print_str(ct->name); print_str("  PID ");
                  print_uint(ct->pid); }
        else      print_str("(none)");
        print_newline();
        print_str("\n  System halted. Reset to continue.\n");
        cpu_cli();
        for (;;) cpu_halt();
    }

    uint8_t irq = (uint8_t)(f->int_no - 32);

    switch (irq) {
        case 0:   /* PIT timer */
            timer_tick();
            new_rsp = proc_tick((uint64_t)(uintptr_t)f);
            break;
        case 1:   /* PS/2 keyboard */
            keyboard_irq();
            break;
        default:
            break;
    }

    /* send EOI — prefer LAPIC if present, else fall back to PIC */
    if (lapic_present())
        lapic_eoi();
    pic_eoi(irq);   /* always send PIC EOI too when using PIC routing */

    return new_rsp;
}
