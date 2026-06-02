global start
global multiboot_info_ptr

extern long_mode_start

section .text
bits 32

start:
    mov esp, stack_top
    mov [multiboot_info_ptr], ebx

    call check_multiboot
    call check_cpuid
    call check_long_mode
    call setup_page_tables
    call enable_paging

    lgdt [gdt64.pointer]
    jmp gdt64.code:long_mode_start
    hlt

; ── checks ──────────────────────────────────────────────

check_multiboot:
    cmp eax, 0x36d76289
    jne .fail
    ret
.fail:
    mov al, 'M'
    jmp error

check_cpuid:
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .fail
    ret
.fail:
    mov al, 'C'
    jmp error

check_long_mode:
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .fail
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .fail
    ret
.fail:
    mov al, 'L'
    jmp error

; ── paging: identity-map first 1 GB with 2 MB pages ─────

setup_page_tables:
    mov eax, pdpt
    or  eax, 0b11
    mov [pml4], eax

    mov eax, pd
    or  eax, 0b11
    mov [pdpt], eax

    mov ecx, 0
.loop:
    mov eax, 0x200000
    mul ecx
    or  eax, 0b10000011     ; present + writable + huge (2 MB)
    mov [pd + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .loop
    ret

enable_paging:
    mov eax, pml4
    mov cr3, eax

    mov eax, cr4
    or  eax, 1 << 5         ; PAE
    mov cr4, eax

    mov ecx, 0xC0000080     ; EFER MSR
    rdmsr
    or  eax, 1 << 8         ; long mode enable
    wrmsr

    mov eax, cr0
    or  eax, 1 << 31        ; paging
    mov cr0, eax
    ret

; ── error: print 'ERR: X' and halt ──────────────────────

error:
    mov dword [0xB8000], 0x4F524F45 ; 'ER'
    mov dword [0xB8004], 0x4F3A4F52 ; 'R:'
    mov dword [0xB8008], 0x4F204F20 ; '  '
    mov byte  [0xB800A], al
    hlt

; ── BSS ─────────────────────────────────────────────────

section .bss
align 4096
pml4:         resb 4096
pdpt:         resb 4096
pd:           resb 4096
stack_bottom: resb 4096 * 4  ; 16 KB kernel stack
stack_top:
multiboot_info_ptr: resd 1

; ── minimal 64-bit GDT (null + code + data) ──────────────

section .rodata
gdt64:
    dq 0                                                    ; null
.code: equ $ - gdt64
    dq (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53)      ; code, 64-bit, DPL=0
.data: equ $ - gdt64
    dq (1 << 41) | (1 << 44) | (1 << 47)                   ; data, writable, DPL=0
.pointer:
    dw $ - gdt64 - 1
    dq gdt64
