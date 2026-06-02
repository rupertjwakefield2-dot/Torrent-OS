#include "str.h"

size_t str_len(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

int str_eq(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a == *b;
}

int str_pfx(const char *s, const char *pfx) {
    while (*pfx) { if (*s++ != *pfx++) return 0; }
    return 1;
}

void str_cpy(char *dst, const char *src, size_t max) {
    size_t i = 0;
    while (i < max - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

void str_cat(char *dst, const char *src, size_t max) {
    size_t dl = str_len(dst);
    if (dl >= max - 1) return;
    str_cpy(dst + dl, src, max - dl);
}

int str_toint(const char *s) {
    int n = 0, neg = 0;
    while (*s == ' ') s++;
    if      (*s == '-') { neg = 1; s++; }
    else if (*s == '+') { s++; }
    while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    return neg ? -n : n;
}

void str_fmtuint(char *buf, size_t bufsz, uint64_t n) {
    if (bufsz == 0) return;
    if (n == 0) {
        if (bufsz > 1) { buf[0] = '0'; buf[1] = '\0'; }
        return;
    }
    char tmp[20];
    int i = 0;
    while (n) { tmp[i++] = (char)('0' + n % 10); n /= 10; }
    size_t j = 0;
    for (int k = i - 1; k >= 0 && j < bufsz - 1; k--) buf[j++] = tmp[k];
    buf[j] = '\0';
}

void str_fmthex(char *buf, size_t bufsz, uint64_t n) {
    const char *h = "0123456789ABCDEF";
    if (bufsz < 4) return;
    buf[0] = '0'; buf[1] = 'x';
    if (n == 0) { buf[2] = '0'; buf[3] = '\0'; return; }
    char tmp[16];
    int i = 0;
    while (n) { tmp[i++] = h[n & 0xF]; n >>= 4; }
    size_t j = 2;
    for (int k = i - 1; k >= 0 && j < bufsz - 1; k--) buf[j++] = tmp[k];
    buf[j] = '\0';
}

const char *str_skip(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s != *prefix) return (void*)0;
        s++; prefix++;
    }
    while (*s == ' ') s++;
    return s;
}
