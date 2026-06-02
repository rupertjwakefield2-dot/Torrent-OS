#include "speaker.h"
#include "io.h"
#include "timer.h"

#define PIT_BASE 1193182

void speaker_init(void) { speaker_off(); }

void speaker_off(void) {
    outb(0x61, (uint8_t)(inb(0x61) & ~0x03));
}

void speaker_beep(uint32_t freq_hz, uint32_t ms) {
    if (freq_hz == 0) { timer_sleep(ms); return; }

    /* program PIT channel 2 for the frequency */
    uint16_t div = (uint16_t)(PIT_BASE / freq_hz);
    outb(0x43, 0xB6);                        /* channel 2, lo/hi, square wave */
    outb(0x42, (uint8_t)(div & 0xFF));
    outb(0x42, (uint8_t)(div >> 8));

    /* connect speaker to PIT channel 2 */
    outb(0x61, (uint8_t)(inb(0x61) | 0x03));

    timer_sleep(ms);
    speaker_off();
}
