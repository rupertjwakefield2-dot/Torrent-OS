#include "rtc.h"
#include "io.h"

static int8_t tz_offset = 0;

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, (uint8_t)(0x80 | reg)); /* NMI disable while reading */
    return inb(0x71);
}
static uint8_t bcd2bin(uint8_t b) { return (uint8_t)((b & 0x0F) + (b >> 4) * 10); }

/* returns 1 if CMOS uses BCD (almost always true) */
static int rtc_is_bcd(void) { return !(cmos_read(0x0B) & 0x04); }

static uint8_t read_reg(uint8_t reg) {
    uint8_t v = cmos_read(reg);
    return rtc_is_bcd() ? bcd2bin(v) : v;
}

/* spin until no update in progress */
static void rtc_wait(void) {
    while (cmos_read(0x0A) & 0x80);
}

void rtc_init(void) { /* CMOS is always on */ }

uint8_t rtc_hour(void) {
    rtc_wait();
    uint8_t h      = cmos_read(0x04);
    uint8_t regB   = cmos_read(0x0B);
    int     is_bcd = !(regB & 0x04);
    int     is_12h = !(regB & 0x02);
    int     pm     = 0;
    if (is_12h && (h & 0x80)) { pm = 1; h = (uint8_t)(h & 0x7F); }
    if (is_bcd) h = bcd2bin(h);
    if (is_12h) {
        if (h == 12) h = 0;
        if (pm)      h = (uint8_t)(h + 12);
    }
    return (uint8_t)((h + tz_offset + 24) % 24);
}
uint8_t  rtc_min(void)   { rtc_wait(); return read_reg(0x02); }
uint8_t  rtc_sec(void)   { rtc_wait(); return read_reg(0x00); }
uint8_t  rtc_day(void)   { rtc_wait(); return read_reg(0x07); }
uint8_t  rtc_month(void) { rtc_wait(); return read_reg(0x08); }
uint16_t rtc_year(void)  { rtc_wait(); return (uint16_t)(read_reg(0x09) + 2000); }

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
    buf[6]  = (char)('0' + (y / 1000) % 10);
    buf[7]  = (char)('0' + (y / 100)  % 10);
    buf[8]  = (char)('0' + (y / 10)   % 10);
    buf[9]  = (char)('0' + y % 10);
    buf[10] = '\0';
}
void   rtc_set_tz_offset(int8_t hours) { tz_offset = hours; }
int8_t rtc_tz_offset(void)             { return tz_offset; }
