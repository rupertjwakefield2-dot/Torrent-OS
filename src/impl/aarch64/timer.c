/*
 * TorrentOS AArch64 timer driver
 * Uses the ARM Generic Timer (CNTP — physical timer, EL1).
 * Polling-based (no IRQ setup required for initial boot).
 */
#include "timer.h"

static void (*second_cb)(void) = (void*)0;
static volatile uint64_t ms_count = 0;

static uint64_t cntfrq(void) {
    uint64_t f;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
    return f;
}
static uint64_t cntpct(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(v));
    return v;
}

/* Boot timestamp — set during timer_init */
static uint64_t boot_count = 0;
static uint64_t freq       = 0;

void timer_init(void) {
    freq       = cntfrq();
    boot_count = cntpct();
    if (freq == 0) freq = 62500000;  /* QEMU default fallback */
}

/* Milliseconds since boot */
uint64_t timer_ticks(void) {
    uint64_t elapsed = cntpct() - boot_count;
    return elapsed * 1000 / freq;
}

void timer_tick(void) {
    /* Not called on ARM64 (no PIT IRQ) — timer is polled. */
}

void timer_sleep(uint64_t ms) {
    uint64_t start = cntpct();
    uint64_t ticks = freq * ms / 1000;
    uint64_t last_sec = timer_ticks() / 1000;
    while ((cntpct() - start) < ticks) {
        /* fire second callback if registered */
        if (second_cb) {
            uint64_t now_sec = timer_ticks() / 1000;
            if (now_sec != last_sec) {
                last_sec = now_sec;
                second_cb();
            }
        }
        __asm__ volatile("wfe");
    }
}

void timer_on_second(void (*cb)(void)) {
    second_cb = cb;
}
