#pragma once
#include <stdint.h>

void speaker_init(void);
void speaker_beep(uint32_t freq_hz, uint32_t ms);  /* blocking */
void speaker_off(void);
