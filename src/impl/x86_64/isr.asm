; ISR stubs — one per interrupt vector (0-47).
; CPU exceptions without error code push a dummy 0 first.
; Then each stub pushes its vector number and jumps to isr_common.

%macro isr_no_err 1
global isr%1
isr%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro

%macro isr_err 1
global isr%1
isr%1:
    push qword %1
    jmp isr_common
%endmacro

; CPU exceptions (0-31)
isr_no_err 0   ; Division By Zero
isr_no_err 1   ; Debug
isr_no_err 2   ; NMI
isr_no_err 3   ; Breakpoint
isr_no_err 4   ; Overflow
isr_no_err 5   ; Bound Range Exceeded
isr_no_err 6   ; Invalid Opcode
isr_no_err 7   ; Device Not Available
isr_err    8   ; Double Fault
isr_no_err 9   ; Coprocessor Segment Overrun
isr_err    10  ; Invalid TSS
isr_err    11  ; Segment Not Present
isr_err    12  ; Stack-Segment Fault
isr_err    13  ; General Protection Fault
isr_err    14  ; Page Fault
isr_no_err 15  ; Reserved
isr_no_err 16  ; x87 FP Exception
isr_err    17  ; Alignment Check
isr_no_err 18  ; Machine Check
isr_no_err 19  ; SIMD FP Exception
isr_no_err 20  ; Virtualization
isr_err    21  ; Control Protection
isr_no_err 22
isr_no_err 23
isr_no_err 24
isr_no_err 25
isr_no_err 26
isr_no_err 27
isr_no_err 28
isr_err    29  ; VMM Communication
isr_err    30  ; Security Exception
isr_no_err 31  ; Reserved

; IRQ handlers (32-47)
%assign _v 32
%rep 16
    isr_no_err _v
%assign _v _v+1
%endrep

; ── common stub: save all GP regs, call C handler, restore ──

extern interrupt_handler

isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov  rdi, rsp           ; arg0 = InterruptFrame*
    call interrupt_handler  ; returns: 0 = no switch, else = new task RSP

    ; if non-zero, switch to new task's kernel stack
    test rax, rax
    jz   .no_switch
    mov  rsp, rax           ; ← context switch happens here
.no_switch:

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    add rsp, 16             ; discard int_no + err_code
    iretq

; ── stub address table (used by idt_init) ───────────────

section .data
global isr_stubs
isr_stubs:
    dq isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
    dq isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
    dq isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    dq isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    dq isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39
    dq isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47
