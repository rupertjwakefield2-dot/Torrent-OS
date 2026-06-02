#include "cpuid.h"
#include "str.h"

static CpuInfo cpu;

/* raw CPUID wrapper */
static void cpuid(uint32_t leaf, uint32_t subleaf,
                  uint32_t *eax, uint32_t *ebx,
                  uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
    );
}

/* copy 4-byte register into buf[off..off+3] */
static void reg_to_str(char *buf, uint32_t reg, int off) {
    buf[off]   = (char)( reg        & 0xFF);
    buf[off+1] = (char)((reg >>  8) & 0xFF);
    buf[off+2] = (char)((reg >> 16) & 0xFF);
    buf[off+3] = (char)((reg >> 24) & 0xFF);
}

void cpuid_init(void) {
    uint32_t eax, ebx, ecx, edx;

    /* ── vendor string (leaf 0) ── */
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    uint32_t max_leaf = eax;
    reg_to_str(cpu.vendor, ebx, 0);
    reg_to_str(cpu.vendor, edx, 4);
    reg_to_str(cpu.vendor, ecx, 8);
    cpu.vendor[12] = '\0';

    /* ── family / model / stepping + feature flags (leaf 1) ── */
    if (max_leaf >= 1) {
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);

        cpu.stepping = (uint8_t)( eax        & 0x0F);
        cpu.model    = (uint8_t)((eax >>  4) & 0x0F);
        cpu.family   = (uint8_t)((eax >>  8) & 0x0F);
        /* extended family/model for Intel/AMD */
        uint8_t ext_model  = (uint8_t)((eax >> 16) & 0x0F);
        uint8_t ext_family = (uint8_t)((eax >> 20) & 0xFF);
        if (cpu.family == 0x0F)              cpu.family = (uint8_t)(cpu.family + ext_family);
        if (cpu.family == 0x06 || cpu.family == 0x0F)
            cpu.model = (uint8_t)((ext_model << 4) | cpu.model);

        cpu.cores = (uint8_t)(((ebx >> 16) & 0xFF));
        if (cpu.cores == 0) cpu.cores = 1;

        /* EDX feature bits */
        cpu.has_fpu  = (edx >>  0) & 1;
        cpu.has_sse  = (edx >> 25) & 1;
        cpu.has_sse2 = (edx >> 26) & 1;

        /* ECX feature bits */
        cpu.has_sse3   = (ecx >>  0) & 1;
        cpu.has_ssse3  = (ecx >>  9) & 1;
        cpu.has_sse41  = (ecx >> 19) & 1;
        cpu.has_sse42  = (ecx >> 20) & 1;
        cpu.has_avx    = (ecx >> 28) & 1;
        cpu.has_aes    = (ecx >> 25) & 1;
        cpu.has_rdrand = (ecx >> 30) & 1;
        cpu.has_popcnt = (ecx >> 23) & 1;
    }

    /* ── AVX2 (leaf 7) ── */
    if (max_leaf >= 7) {
        cpuid(7, 0, &eax, &ebx, &ecx, &edx);
        cpu.has_avx2 = (ebx >> 5) & 1;
    }

    /* ── extended features (leaf 0x80000001) ── */
    cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    uint32_t max_ext = eax;

    if (max_ext >= 0x80000001) {
        cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
        cpu.has_nx       = (edx >> 20) & 1;
        cpu.has_1gb_pages= (edx >> 26) & 1;
        cpu.has_x86_64   = (edx >> 29) & 1;
    }

    /* ── brand / model name (leaves 0x80000002..4) ── */
    if (max_ext >= 0x80000004) {
        char *b = cpu.brand;
        for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++) {
            cpuid(leaf, 0, &eax, &ebx, &ecx, &edx);
            int off = (int)(leaf - 0x80000002) * 16;
            reg_to_str(b, eax, off);
            reg_to_str(b, ebx, off+4);
            reg_to_str(b, ecx, off+8);
            reg_to_str(b, edx, off+12);
        }
        cpu.brand[48] = '\0';
        /* trim leading spaces */
        char *p = cpu.brand;
        while (*p == ' ') p++;
        if (p != cpu.brand) {
            char tmp[49];
            str_cpy(tmp, p, sizeof(tmp));
            str_cpy(cpu.brand, tmp, sizeof(cpu.brand));
        }
    } else {
        str_cpy(cpu.brand, cpu.vendor, sizeof(cpu.brand));
    }
}

const CpuInfo *cpuid_info(void) { return &cpu; }
