#include "lapic.h"
#include "cpuid.h"
#include "io.h"
#include "timer.h"

/*
 * Local APIC driver.
 *
 * We read the APIC base from IA32_APIC_BASE MSR, enable the LAPIC,
 * and configure the spurious interrupt vector.  The 8259A PIC is left
 * in place for interrupt routing (IRQ0=timer, IRQ1=keyboard); we just
 * use LAPIC_EOI when sending EOI so both PIC and LAPIC are happy.
 *
 * A full IOAPIC / LAPIC-timer setup comes in Stage 5 (SMP support).
 */

#define APIC_BASE_MSR       0x1B
#define APIC_BASE_ENABLE    (1UL << 11)

/* LAPIC register offsets (from base) */
#define LAPIC_ID            0x020
#define LAPIC_VERSION       0x030
#define LAPIC_EOI           0x0B0
#define LAPIC_SPURIOUS      0x0F0
#define LAPIC_SPURIOUS_EN   (1u << 8)
#define LAPIC_SPURIOUS_VEC  0xFF    /* spurious interrupt vector */

#define LAPIC_LINT0         0x350
#define LAPIC_LINT1         0x360
#define LAPIC_LVT_MASKED    (1u << 16)

static volatile uint32_t *lapic_base = (void*)0;
static int                lapic_ok   = 0;

static uint64_t read_msr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}
static void write_msr(uint32_t msr, uint64_t val) {
    __asm__ volatile("wrmsr"
        : : "c"(msr), "a"((uint32_t)val), "d"((uint32_t)(val >> 32)));
}

static uint32_t lapic_read(uint32_t off) {
    return lapic_base[off >> 2];
}
static void lapic_write(uint32_t off, uint32_t val) {
    lapic_base[off >> 2] = val;
}

void lapic_init(void) {
    /* Check CPUID leaf 1 LAPIC bit (EDX bit 9) */
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1), "c"(0));
    if (!(edx & (1u << 9))) return;   /* no APIC */

    /* Get APIC base physical address */
    uint64_t apic_msr = read_msr(APIC_BASE_MSR);
    uint64_t base_phys = apic_msr & 0xFFFFF000UL;

    /* Enable APIC in MSR */
    write_msr(APIC_BASE_MSR, apic_msr | APIC_BASE_ENABLE);

    /* Since first 1 GB is identity-mapped, virtual == physical */
    lapic_base = (volatile uint32_t*)(uintptr_t)base_phys;

    /* Set spurious interrupt vector and enable LAPIC */
    lapic_write(LAPIC_SPURIOUS,
                LAPIC_SPURIOUS_EN | LAPIC_SPURIOUS_VEC);

    /* Mask LINT0 and LINT1 (we use the PIC for routing) */
    lapic_write(LAPIC_LINT0, LAPIC_LVT_MASKED);
    lapic_write(LAPIC_LINT1, LAPIC_LVT_MASKED);

    lapic_ok = 1;
}

void lapic_eoi(void) {
    if (lapic_ok) lapic_write(LAPIC_EOI, 0);
}

uint32_t lapic_id(void) {
    return lapic_ok ? (lapic_read(LAPIC_ID) >> 24) : 0;
}

int lapic_present(void) { return lapic_ok; }
