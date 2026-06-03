#pragma once
#include <stdint.h>

/* Page flags (same bit positions as x86 PTEs) */
#define VMM_PRESENT  (1UL << 0)
#define VMM_WRITE    (1UL << 1)
#define VMM_USER     (1UL << 2)
#define VMM_HUGE     (1UL << 7)   /* 2MB page in PD */
#define VMM_NX       (1UL << 63)  /* no-execute (requires EFER.NXE) */

#define VMM_KERN_RW  (VMM_PRESENT | VMM_WRITE)
#define VMM_KERN_RO  (VMM_PRESENT)
#define VMM_USER_RW  (VMM_PRESENT | VMM_WRITE | VMM_USER)

void    vmm_init(void);

/* Map one 4 KB virtual page → physical page.  flags = VMM_* bits. */
int     vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);

/* Unmap a virtual page (does not free the backing physical frame). */
void    vmm_unmap(uint64_t virt);

/* Walk page tables and return physical address, or 0 if not mapped. */
uint64_t vmm_virt_to_phys(uint64_t virt);

/* Allocate 'n' consecutive virtual pages starting at 'virt_base',
 * backed by fresh PMM frames.  Returns 1 on success. */
int     vmm_alloc(uint64_t virt_base, uint64_t n, uint64_t flags);

/* Free pages allocated with vmm_alloc (frees physical frames too). */
void    vmm_free(uint64_t virt_base, uint64_t n);

/* Flush the TLB for a single page */
static inline void vmm_flush(uint64_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}
