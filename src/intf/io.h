#pragma once
#include <stdint.h>

/* ── x86_64 port I/O + CPU control ── */
#ifdef __x86_64__

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}
static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port) : "memory");
}
static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port) : "memory");
}
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile("inl %1, %0" : "=a"(val) : "Nd"(port) : "memory");
    return val;
}
static inline void io_wait(void) { outb(0x80, 0x00); }
static inline void cpu_halt(void) { __asm__ volatile("hlt"); }
static inline void cpu_cli(void)  { __asm__ volatile("cli" ::: "memory"); }
static inline void cpu_sti(void)  { __asm__ volatile("sti" ::: "memory"); }

/* ── AArch64 memory-mapped I/O + CPU control ── */
#elif defined(__aarch64__)

/* No port I/O on ARM — callers that use these are x86_64-specific stubs */
static inline void outb(uint16_t p, uint8_t  v) { (void)p; (void)v; }
static inline void outw(uint16_t p, uint16_t v) { (void)p; (void)v; }
static inline void outl(uint16_t p, uint32_t v) { (void)p; (void)v; }
static inline uint8_t  inb(uint16_t p) { (void)p; return 0; }
static inline uint16_t inw(uint16_t p) { (void)p; return 0; }
static inline uint32_t inl(uint16_t p) { (void)p; return 0; }
static inline void io_wait(void) { }
static inline void cpu_halt(void) { __asm__ volatile("wfi"); }
static inline void cpu_cli(void)  { __asm__ volatile("msr daifset, #0xF" ::: "memory"); }
static inline void cpu_sti(void)  { __asm__ volatile("msr daifclr, #0xF" ::: "memory"); }

/* MMIO helpers used by ARM64 drivers */
static inline void     mmio_w32(uint64_t addr, uint32_t v) { *(volatile uint32_t*)addr = v; }
static inline uint32_t mmio_r32(uint64_t addr)             { return *(volatile uint32_t*)addr; }
static inline void     mmio_w8 (uint64_t addr, uint8_t  v) { *(volatile uint8_t *)addr = v; }
static inline uint8_t  mmio_r8 (uint64_t addr)             { return *(volatile uint8_t *)addr; }

#else
#error "Unsupported architecture"
#endif
