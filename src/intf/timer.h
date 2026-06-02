#pragma once
#include <stdint.h>

void     timer_init(void);
void     timer_tick(void);         /* called by IRQ0 handler */
uint64_t timer_ticks(void);
void     timer_sleep(uint64_t ms);
void     timer_on_second(void (*cb)(void)); /* register 1-second callback */
