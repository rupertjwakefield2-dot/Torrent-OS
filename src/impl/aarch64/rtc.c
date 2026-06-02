/*
 * TorrentOS AArch64 RTC driver
 * Uses PL031 RTC at 0x09010000 (QEMU virt machine).
 * The PL031 RTCDR register holds seconds since Unix epoch.
 */
#include "rtc.h"

#define PL031_BASE  0x09010000UL
#define PL031_DR    (*(volatile uint32_t*)(PL031_BASE + 0x000))  /* data (epoch secs) */

static int8_t tz_offset = 0;

static uint32_t epoch_secs(void) { return PL031_DR; }

/* Days in each month (non-leap) */
static const uint8_t mdays[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

static int is_leap(uint32_t y) {
    return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}

typedef struct { uint8_t h, m, s, day, mon; uint16_t year; } DateTime;

static DateTime epoch_to_dt(uint32_t secs) {
    uint32_t s = secs % 60; secs /= 60;
    uint32_t m = secs % 60; secs /= 60;
    uint32_t h = secs % 24; secs /= 24;
    /* days since 1970-01-01 */
    uint32_t days = secs;
    uint32_t year = 1970;
    while (1) {
        uint32_t yd = is_leap(year) ? 366 : 365;
        if (days < yd) break;
        days -= yd;
        year++;
    }
    uint8_t mon = 0;
    while (1) {
        uint32_t md = mdays[mon] + (mon == 1 && is_leap(year) ? 1 : 0);
        if (days < md) break;
        days -= md;
        mon++;
    }
    DateTime dt;
    dt.h = (uint8_t)((h + tz_offset + 24) % 24);
    dt.m = (uint8_t)m;
    dt.s = (uint8_t)s;
    dt.day = (uint8_t)(days + 1);
    dt.mon = (uint8_t)(mon + 1);
    dt.year = (uint16_t)year;
    return dt;
}

void rtc_init(void) { }

uint8_t  rtc_hour(void)  { return epoch_to_dt(epoch_secs()).h; }
uint8_t  rtc_min(void)   { return epoch_to_dt(epoch_secs()).m; }
uint8_t  rtc_sec(void)   { return epoch_to_dt(epoch_secs()).s; }
uint8_t  rtc_day(void)   { return epoch_to_dt(epoch_secs()).day; }
uint8_t  rtc_month(void) { return epoch_to_dt(epoch_secs()).mon; }
uint16_t rtc_year(void)  { return epoch_to_dt(epoch_secs()).year; }

static void fmt2(char *buf, uint8_t v) {
    buf[0] = (char)('0' + v / 10);
    buf[1] = (char)('0' + v % 10);
}
void rtc_fmt_time(char *buf) {
    fmt2(buf,   rtc_hour()); buf[2] = ':';
    fmt2(buf+3, rtc_min());  buf[5] = '\0';
}
void rtc_fmt_date(char *buf) {
    fmt2(buf,   rtc_day());   buf[2] = '/';
    fmt2(buf+3, rtc_month()); buf[5] = '/';
    uint16_t y = rtc_year();
    buf[6]  = (char)('0' + (y/1000)%10);
    buf[7]  = (char)('0' + (y/100)%10);
    buf[8]  = (char)('0' + (y/10)%10);
    buf[9]  = (char)('0' + y%10);
    buf[10] = '\0';
}
void   rtc_set_tz_offset(int8_t h) { tz_offset = h; }
int8_t rtc_tz_offset(void)         { return tz_offset; }
