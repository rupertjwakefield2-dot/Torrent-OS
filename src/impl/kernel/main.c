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
#include "vmm.h"
#include "proc.h"
#include "syscall.h"
#include "lapic.h"

/* 1-second timer callback — refresh status bar clock */
static void on_second(void) {
    desktop_tick();
}

void kernel_main(uint32_t mb_addr) {

    /* ── display ── */
    print_init();
    print_set_color(COLOR_LIGHT_CYAN, COLOR_BLACK);
    print_str(
        "  ________  ________  ________  ________  _______   ________   _________\n"
        " |\\__    _\\|\\   __  \\|\\   __  \\|\\   __  \\|\\  ___ \\ |\\   ___  \\|\\___   ___\\\n"
        " \\|_ \\  \\_|\\ \\  \\|\\  \\ \\  \\|\\  \\ \\  \\|\\  \\ \\   __/|\\ \\  \\\\ \\  \\|___ \\  \\_|\n"
        "     \\ \\  \\ \\ \\  \\\\\\  \\ \\   _  _\\ \\   _  _\\ \\  \\_|/_\\ \\  \\\\ \\  \\   \\ \\  \\\n"
        "      \\ \\  \\ \\ \\  \\\\\\  \\ \\  \\\\  \\\\ \\  \\\\  \\\\ \\  \\_|\\ \\ \\  \\\\ \\  \\   \\ \\  \\\n"
        "       \\ \\__\\ \\ \\_______\\ \\__\\\\ _\\\\ \\__\\\\ _\\\\ \\_______\\ \\__\\\\ \\__\\   \\ \\__\\\n"
        "        \\|__|  \\|_______|\\|__|\\|__|\\|__|\\|__|\\|_______|\\|__| \\|__|    \\|__|\n"
    );
    print_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    print_str("\n  Booting TorrentOS v0.1-dev...\n\n");

    /* ── core hardware ── */
    idt_init();
    print_str("  [ok] IDT + PIC\n");

    timer_init();
    print_str("  [ok] PIT timer (1 kHz)\n");

    keyboard_init();
    print_str("  [ok] PS/2 keyboard\n");

    speaker_init();
    print_str("  [ok] PC speaker\n");

    /* ── memory ── */
    pmm_init(mb_addr);
    heap_init();
    size_t total_kb = pmm_total_frames() * 4;
    size_t free_kb  = pmm_free_frames()  * 4;
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
    print_str("  [ok] CPU     ");
    print_str(ci->brand);
    print_str(ci->has_x86_64 ? "  [x86_64]\n" : "  [32-bit]\n");

    /* ── advanced kernel subsystems ── */
    lapic_init();
    print_str(lapic_present() ? "  [ok] LAPIC\n"
                              : "  [--] LAPIC not present — using PIC only\n");

    vmm_init();
    print_str("  [ok] VMM (virtual memory manager)\n");

    proc_init();
    print_str("  [ok] Scheduler (preemptive round-robin)\n");

    syscall_init();
    print_str("  [ok] Syscall interface (SYSCALL/SYSRET)\n");

    /* ── ATA storage ── */
    if (ata_init()) {
        print_str("  [ok] ATA drive detected\n");
    } else {
        print_set_color(COLOR_DARK_GRAY, COLOR_BLACK);
        print_str("  [--] No ATA drive  (add -drive ... to QEMU for persistence)\n");
        print_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    }

    /* ── enable interrupts ── */
    cpu_sti();

    /* ── boot beep ── */
    speaker_beep(880, 80);
    timer_sleep(40);
    speaker_beep(1320, 120);

    /* ── load persistent config or run install wizard ── */
    int have_config = config_load();
    if (have_config) {
        config_apply();
        print_set_color(COLOR_LIGHT_GREEN, COLOR_BLACK);
        print_str("  [ok] Config loaded from disk\n");
        print_set_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    } else if (auth_username[0] == '\0') {
        timer_sleep(400);
        install_run();
        /* save after first-time setup */
        config_commit();
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
    print_str("  Tab to complete  \xFA  type 'help' to list commands\n\n");
    print_set_color(term_fg, term_bg);

    shell_run();
}
