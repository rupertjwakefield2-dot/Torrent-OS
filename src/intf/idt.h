#pragma once
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} IDTEntry;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} IDTPointer;

/* All GP registers saved by isr.asm, then int_no/err_code, then CPU frame */
typedef struct {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no, err_code;
    uint64_t rip, cs, rflags, rsp, ss;
} InterruptFrame;

void     idt_init(void);
uint64_t interrupt_handler(InterruptFrame *f);  /* called from isr.asm */
