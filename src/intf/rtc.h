#pragma once
#include <stdint.h>

void     rtc_init(void);
uint8_t  rtc_hour(void);
uint8_t  rtc_min(void);
uint8_t  rtc_sec(void);
uint8_t  rtc_day(void);
uint8_t  rtc_month(void);
uint16_t rtc_year(void);
void     rtc_fmt_time(char *buf);   /* writes "HH:MM\0" into buf (6 bytes min) */
void     rtc_fmt_date(char *buf);   /* writes "DD/MM/YYYY\0" into buf (12 bytes min) */
void     rtc_set_tz_offset(int8_t hours);
int8_t   rtc_tz_offset(void);
