/* Speaker stub for AArch64 — no PC speaker on ARM */
#include "speaker.h"
#include "timer.h"
void speaker_init(void)                            { }
void speaker_off(void)                             { }
void speaker_beep(uint32_t f, uint32_t ms)         { (void)f; timer_sleep(ms); }
