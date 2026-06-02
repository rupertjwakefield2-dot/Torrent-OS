#include "timer.h"
#include "io.h"

#define PIT_HZ    1000
#define PIT_BASE  1193182

static volatile uint64_t tick_count = 0;
static void (*second_cb)(void) = (void*)0;

void timer_init(void) {
    uint16_t div = (uint16_t)(PIT_BASE / PIT_HZ);
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(div & 0xFF));
    outb(0x40, (uint8_t)(div >> 8));
}

void timer_tick(void) {
    tick_count++;
    if (second_cb && (tick_count % 1000) == 0)
        second_cb();
}

uint64_t timer_ticks(void) { return tick_count; }

void timer_sleep(uint64_t ms) {
    uint64_t end = tick_count + ms;
    while (tick_count < end)
        cpu_halt();
}

void timer_on_second(void (*cb)(void)) {
    second_cb = cb;
}
