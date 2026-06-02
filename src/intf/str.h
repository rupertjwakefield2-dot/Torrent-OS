#pragma once
#include <stdint.h>
#include <stddef.h>

size_t str_len(const char *s);
int    str_eq(const char *a, const char *b);
int    str_pfx(const char *s, const char *pfx);
void   str_cpy(char *dst, const char *src, size_t max);
void   str_cat(char *dst, const char *src, size_t max);
int    str_toint(const char *s);
void   str_fmtuint(char *buf, size_t bufsz, uint64_t n);
void   str_fmthex(char *buf, size_t bufsz, uint64_t n);
const char *str_skip(const char *s, const char *prefix); /* returns s past prefix, or NULL */
