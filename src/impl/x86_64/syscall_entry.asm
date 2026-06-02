; syscall_entry.asm — 64-bit SYSCALL entry point
;
; When userspace (or kernel code) executes SYSCALL:
;   RCX = return RIP   R11 = saved RFLAGS   RSP = caller's stack
;   RAX = syscall number   RDI RSI RDX R10 R8 R9 = args
;
; We save all caller-saved registers, call the C dispatcher, then
; restore them and execute SYSRETQ to return.
;
; Note: for ring-0 kernel threads RCX/R11 are clobbered by SYSCALL —
; this is fine since we're testing the mechanism before adding userspace.

global syscall_entry
extern syscall_dispatch

syscall_entry:
    ; save caller-saved registers
    push rcx        ; return RIP
    push r11        ; saved RFLAGS
    push r10        ; arg4 (syscall ABI: r10 instead of rcx)
    push r9
    push r8
    push rdi
    push rsi
    push rdx
    push rbx        ; callee-saved but we save for safety

    ; Call C dispatcher:
    ;   syscall_dispatch(num=rax, a1=rdi, a2=rsi, a3=rdx, a4=r10, a5=r8, a6=r9)
    ; The System V calling convention already has args in the right registers
    ; from the userspace caller — but we've pushed some so re-set from saved vals.
    ; Easier: just re-load from the calling convention registers before calling.
    mov  rdi, rax          ; syscall number → arg0
    ; a1..a6 are already in rsi rdx r10 r8 r9 from caller

    ; However rsi was pushed and its original value is still in the register.
    ; Just call — the values haven't been modified yet by our pushes
    ; (we only pushed onto stack, not into the argument registers).
    call syscall_dispatch   ; return value in RAX

    ; restore
    pop rbx
    pop rdx
    pop rsi
    pop rdi
    pop r8
    pop r9
    pop r10
    pop r11
    pop rcx

    sysretq
