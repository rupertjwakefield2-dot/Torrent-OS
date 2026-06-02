section .multiboot_header
header_start:
    dd 0xe85250d6                                           ; multiboot2 magic
    dd 0                                                    ; architecture: x86/32
    dd header_end - header_start                            ; header length
    dd 0x100000000 - (0xe85250d6 + 0 + (header_end - header_start)) ; checksum
    dw 0                                                    ; end tag type
    dw 0                                                    ; end tag flags
    dd 8                                                    ; end tag size
header_end:
