#pragma once
#include <stdint.h>

/* CPU feature flags */
typedef struct {
    /* identity */
    char vendor[13];    /* e.g. "GenuineIntel\0" */
    char brand[49];     /* full model name string  */
    uint8_t family;
    uint8_t model;
    uint8_t stepping;
    uint8_t cores;      /* logical core count */

    /* feature bits */
    int has_fpu;
    int has_sse;
    int has_sse2;
    int has_sse3;
    int has_ssse3;
    int has_sse41;
    int has_sse42;
    int has_avx;
    int has_avx2;
    int has_aes;
    int has_rdrand;
    int has_popcnt;
    int has_nx;         /* no-execute / XD bit */
    int has_1gb_pages;
    int has_x86_64;     /* long mode */
} CpuInfo;

void           cpuid_init(void);
const CpuInfo *cpuid_info(void);
