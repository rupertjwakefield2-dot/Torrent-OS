ENTRY(start)

SECTIONS
{
    . = 1M;

    .boot : {
        KEEP(*(.multiboot_header))
    }

    .text : ALIGN(4096) {
        *(.text)
    }

    .rodata : ALIGN(4096) {
        *(.rodata*)
    }

    .data : ALIGN(4096) {
        *(.data)
    }

    .bss : ALIGN(4096) {
        *(COMMON)
        *(.bss)
    }

    kernel_end = ALIGN(4096);
}
