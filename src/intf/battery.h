#pragma once
#include <stdint.h>

typedef enum { BAT_AC, BAT_CHARGING, BAT_DISCHARGING } BatState;

void     battery_init(void);
BatState battery_state(void);
uint8_t  battery_percent(void);  /* 0-100, or 0xFF if N/A */
void     battery_fmt(char *buf); /* writes "AC" or "BAT 75%" into buf (10 bytes min) */
int      battery_saver_enabled(void);
void     battery_saver_set(int on);
