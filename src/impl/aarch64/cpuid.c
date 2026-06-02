/*
 * TorrentOS AArch64 CPU identification
 * Reads MIDR_EL1 (Main ID Register) and ID_AA64ISAR0_EL1 for feature flags.
 */
#include "cpuid.h"
#include "str.h"

static CpuInfo cpu;

static uint64_t read_midr(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, midr_el1" : "=r"(v));
    return v;
}
static uint64_t read_isar0(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, id_aa64isar0_el1" : "=r"(v));
    return v;
}
static uint64_t read_pfr0(void) {
    uint64_t v;
    __asm__ volatile("mrs %0, id_aa64pfr0_el1" : "=r"(v));
    return v;
}

static const char *implementer_name(uint8_t imp) {
    switch (imp) {
        case 0x41: return "ARM";
        case 0x42: return "Broadcom";
        case 0x43: return "Cavium";
        case 0x44: return "DEC";
        case 0x46: return "Fujitsu";
        case 0x48: return "HiSilicon";
        case 0x49: return "Infineon";
        case 0x4D: return "Motorola";
        case 0x4E: return "NVIDIA";
        case 0x50: return "APM";
        case 0x51: return "Qualcomm";
        case 0x56: return "Marvell";
        case 0x61: return "Apple";
        case 0x69: return "Intel";
        default:   return "Unknown";
    }
}

void cpuid_init(void) {
    uint64_t midr  = read_midr();
    uint64_t isar0 = read_isar0();
    uint64_t pfr0  = read_pfr0();

    uint8_t implementer = (uint8_t)((midr >> 24) & 0xFF);
    uint16_t partnum    = (uint16_t)((midr >>  4) & 0xFFF);
    cpu.family          = (uint8_t)((midr >> 20) & 0xF);
    cpu.model           = (uint8_t)(partnum & 0xFF);
    cpu.stepping        = (uint8_t)( midr       & 0xF);
    cpu.cores           = 1;  /* SMP not yet supported */

    /* vendor string */
    str_cpy(cpu.vendor, implementer_name(implementer), sizeof(cpu.vendor));

    /* brand string: "ARM Cortex-A72" etc. */
    str_cpy(cpu.brand, implementer_name(implementer), sizeof(cpu.brand));
    str_cat(cpu.brand, " AArch64", sizeof(cpu.brand));
    /* part number lookup (common QEMU parts) */
    if (implementer == 0x41) {
        if      (partnum == 0xD03) str_cpy(cpu.brand, "ARM Cortex-A53",  sizeof(cpu.brand));
        else if (partnum == 0xD07) str_cpy(cpu.brand, "ARM Cortex-A57",  sizeof(cpu.brand));
        else if (partnum == 0xD08) str_cpy(cpu.brand, "ARM Cortex-A72",  sizeof(cpu.brand));
        else if (partnum == 0xD0C) str_cpy(cpu.brand, "ARM Neoverse-N1", sizeof(cpu.brand));
        else if (partnum == 0xD49) str_cpy(cpu.brand, "ARM Neoverse-N2", sizeof(cpu.brand));
    } else if (implementer == 0x61) {
        str_cpy(cpu.brand, "Apple Silicon (AArch64)", sizeof(cpu.brand));
    }

    /* AArch64 is always 64-bit */
    cpu.has_x86_64 = 0;  /* not x86 */

    /* FP / SIMD (ASIMD = NEON) */
    cpu.has_fpu  = ((pfr0 >> 16) & 0xF) != 0xF;  /* FP not absent */
    cpu.has_sse  = 0;   /* NEON ≠ SSE, but flag as "vector unit present" */
    cpu.has_sse2 = 0;

    /* AES: ID_AA64ISAR0_EL1[7:4] */
    cpu.has_aes    = ((isar0 >>  4) & 0xF) >= 1;
    /* SHA2: bits [15:12] */
    cpu.has_sse42  = ((isar0 >> 12) & 0xF) >= 1;  /* reuse flag for SHA */
    /* Atomic: bits [23:20] */
    cpu.has_avx    = ((isar0 >> 20) & 0xF) >= 2;  /* reuse for LSE atomics */
    /* RDRAND equivalent: bits [63:60] */
    cpu.has_rdrand = ((isar0 >> 60) & 0xF) >= 1;

    cpu.has_sse3 = cpu.has_ssse3 = cpu.has_sse41 = cpu.has_avx2 = 0;
    cpu.has_popcnt = 0;
    cpu.has_nx = 1;           /* AArch64 always has NX (XN) */
    cpu.has_1gb_pages = 1;    /* AArch64 supports 1GB page descriptors */
}

const CpuInfo *cpuid_info(void) { return &cpu; }
