#include "print.h"
#include "idt.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"
#include "heap.h"
#include "shell.h"
#include "io.h"
#include "rtc.h"
#include "battery.h"
#include "pci.h"
#include "auth.h"
#include "install.h"
#include "desktop.h"
#include "speaker.h"
#include "ata.h"
#include "config.h"
#include "vfs.h"
#include "cpuid.h"

/*
 * AArch64 does not use multiboot2.
 * Memory layout for QEMU virt:
 *   RAM starts at 0x40000000, kernel loaded there.
 *   We give the PMM a fixed 256 MB window above the kernel.
 */
extern char kernel_end;   /* exported by linker script */

static void on_second(void) { desktop_tick(); }

/* PSCI power management (SMC / HVC) */
static void psci_call(uint64_t fn, uint64_t arg) {
    __asm__ volatile(
        "mov x0, %0\n"
        "mov x1, %1\n"
        "hvc #0\n"
        : : "r"(fn), "r"(arg) : "x0", "x1", "memory"
    );
}
#define PSCI_SYSTEM_OFF    0x84000008UL
#define PSCI_SYSTEM_RESET  0x84000009UL

void arm64_poweroff(void)  { psci_call(PSCI_SYSTEM_OFF,   0); for(;;) cpu_halt(); }
void arm64_reboot(void)    { psci_call(PSCI_SYSTEM_RESET, 0); for(;;) cpu_halt(); }

void kernel_main(void) {

    /* ── display ── */
    print_init();
    print_set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print_str(
        "  _____                          _    ___  ____\n"
        " |_   _|__  _ __ _ __ ___ _ __ | |_ / _ \\/ ___|\n"
        "   | |/ _ \\| '__| '__/ _ \\ '_ \\| __| | | \\___ \\\n"
        "   | | (_) | |  | | |  __/ | | | |_| |_| |___) |\n"
        "   |_|\\___/|_|  |_|  \\___|_| |_|\\__|\\___/|____/\n"
    );
    print_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    print_str("\n  TorrentOS v0.1-dev  \xB7  AArch64 (ARM64)\n\n");

    /* ── core init ── */
    idt_init();
    print_str("  [ok] Exception vectors\n");

    timer_init();
    print_str("  [ok] ARM Generic Timer\n");

    keyboard_init();
    print_str("  [ok] UART keyboard\n");

    speaker_init();
    print_str("  [ok] Speaker\n");

    /* ── memory — fixed map: 256 MB from kernel_end ── */
    uint64_t mem_start = (uint64_t)&kernel_end;
    /* align to 4KB */
    mem_start = (mem_start + 4095) & ~(uint64_t)4095;
    uint64_t mem_end = 0x40000000UL + 256UL * 1024 * 1024;
    pmm_init_fixed(mem_start, mem_end);
    heap_init();
    size_t free_kb = pmm_free_frames() * 4;
    size_t total_kb = pmm_total_frames() * 4;
    print_str("  [ok] Memory   ");
    print_uint(free_kb); print_str(" KB free / ");
    print_uint(total_kb); print_str(" KB total\n");

    /* ── peripherals ── */
    rtc_init();
    battery_init();
    pci_scan();
    auth_init();
    vfs_init();
    cpuid_init();
    const CpuInfo *ci = cpuid_info();
    print_str("  [ok] CPU      "); print_str(ci->brand); print_newline();

    ata_init();   /* stub — always returns 0 */
    print_set_color(COLOR_DARK_GRAY, COLOR_BLACK);
    print_str("  [--] ATA storage: not available on ARM64 (VirtIO planned)\n");
    print_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);

    cpu_sti();

    /* ── first-boot or load config ── */
    if (!config_load() && auth_username[0] == '\0') {
        timer_sleep(300);
        install_run();
        config_commit();
    } else {
        config_apply();
    }

    /* ── login ── */
    auth_login();

    /* ── desktop + shell ── */
    desktop_init();
    timer_on_second(on_second);

    Color term_fg = auth_dark_mode ? COLOR_LIGHT_GRAY : COLOR_BLACK;
    Color term_bg = auth_dark_mode ? COLOR_BLACK      : COLOR_LIGHT_GRAY;
    print_set_color(term_fg, term_bg);
    print_clear();
    desktop_redraw_bar();

    print_set_color(COLOR_DARK_GRAY, term_bg);
    print_str("  TorrentOS  \xB7  AArch64  \xB7  Tab to complete  \xB7  type 'help'\n\n");
    print_set_color(term_fg, term_bg);

    shell_run();
}
