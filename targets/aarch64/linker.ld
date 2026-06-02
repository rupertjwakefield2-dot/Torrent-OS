/*
 * TorrentOS AArch64 linker script
 * QEMU virt machine loads the kernel at 0x40000000 (1 GB mark).
 */

ENTRY(_start)

SECTIONS {
    . = 0x40000000;

    .text : {
        KEEP(*(.text.boot))   /* boot entry must be first */
        *(.text*)
    }

    . = ALIGN(4096);
    .rodata : { *(.rodata*) }

    . = ALIGN(4096);
    .data : { *(.data*) }

    . = ALIGN(4096);
    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    }

    kernel_end = ALIGN(4096);
}
