#include "vmm.h"
#include "pmm.h"
#include "str.h"

/*
 * Virtual Memory Manager — x86_64 four-level paging.
 *
 * The boot code creates a PML4 that identity-maps the first 1 GB using
 * 2 MB huge pages.  This VMM extends that setup by allowing fine-grained
 * 4 KB page mappings (walks PML4→PDPT→PD→PT and allocates intermediate
 * tables from the PMM as needed).
 *
 * Since the first 1 GB is identity-mapped, virtual == physical for all
 * kernel structures — we can safely cast physical frame addresses to
 * pointers and dereference them.
 */

/* From boot/main.asm — physical (= virtual) address of the boot PML4 */
extern uint64_t boot_pml4_phys;

#define PHYS_ADDR_MASK  0x000FFFFFFFFFF000ULL  /* bits 51:12 */
#define PML4_IDX(v)  (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v)  (((v) >> 30) & 0x1FF)
#define PD_IDX(v)    (((v) >> 21) & 0x1FF)
#define PT_IDX(v)    (((v) >> 12) & 0x1FF)

static uint64_t *pml4 = (void*)0;

void vmm_init(void) {
    pml4 = (uint64_t*)(uintptr_t)(uint32_t)boot_pml4_phys;
}

/* Allocate a zeroed 4 KB page table frame from PMM */
static uint64_t *alloc_table(void) {
    uint64_t phys = pmm_alloc();
    if (!phys) return (void*)0;
    uint64_t *tbl = (uint64_t*)(uintptr_t)phys;
    for (int i = 0; i < 512; i++) tbl[i] = 0;
    return tbl;
}

/* Return pointer to the PT entry for 'virt', creating tables as needed */
static uint64_t *get_pte(uint64_t virt, int create) {
    if (!pml4) return (void*)0;

    /* PML4 */
    uint64_t *pdpt;
    uint64_t  pml4e = pml4[PML4_IDX(virt)];
    if (!(pml4e & VMM_PRESENT)) {
        if (!create) return (void*)0;
        pdpt = alloc_table();
        if (!pdpt) return (void*)0;
        pml4[PML4_IDX(virt)] = (uint64_t)(uintptr_t)pdpt | VMM_PRESENT | VMM_WRITE;
    } else {
        pdpt = (uint64_t*)(uintptr_t)(pml4e & PHYS_ADDR_MASK);
    }

    /* PDPT */
    uint64_t *pd;
    uint64_t  pdpte = pdpt[PDPT_IDX(virt)];
    if (!(pdpte & VMM_PRESENT)) {
        if (!create) return (void*)0;
        pd = alloc_table();
        if (!pd) return (void*)0;
        pdpt[PDPT_IDX(virt)] = (uint64_t)(uintptr_t)pd | VMM_PRESENT | VMM_WRITE;
    } else {
        if (pdpte & VMM_HUGE) return (void*)0;  /* 1 GB huge page */
        pd = (uint64_t*)(uintptr_t)(pdpte & PHYS_ADDR_MASK);
    }

    /* PD */
    uint64_t *pt;
    uint64_t  pde = pd[PD_IDX(virt)];
    if (!(pde & VMM_PRESENT)) {
        if (!create) return (void*)0;
        pt = alloc_table();
        if (!pt) return (void*)0;
        pd[PD_IDX(virt)] = (uint64_t)(uintptr_t)pt | VMM_PRESENT | VMM_WRITE;
    } else {
        if (pde & VMM_HUGE) return (void*)0;  /* 2 MB huge page */
        pt = (uint64_t*)(uintptr_t)(pde & PHYS_ADDR_MASK);
    }

    return &pt[PT_IDX(virt)];
}

int vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t *pte = get_pte(virt, 1);
    if (!pte) return 0;
    *pte = (phys & PHYS_ADDR_MASK) | (flags & ~PHYS_ADDR_MASK) | VMM_PRESENT;
    vmm_flush(virt);
    return 1;
}

void vmm_unmap(uint64_t virt) {
    uint64_t *pte = get_pte(virt, 0);
    if (pte) { *pte = 0; vmm_flush(virt); }
}

uint64_t vmm_virt_to_phys(uint64_t virt) {
    uint64_t *pte = get_pte(virt, 0);
    if (!pte || !(*pte & VMM_PRESENT)) return 0;
    return (*pte & PHYS_ADDR_MASK) | (virt & 0xFFF);
}

int vmm_alloc(uint64_t virt_base, uint64_t n, uint64_t flags) {
    for (uint64_t i = 0; i < n; i++) {
        uint64_t phys = pmm_alloc();
        if (!phys) return 0;
        if (!vmm_map(virt_base + i * 4096, phys, flags)) {
            pmm_free(phys);
            return 0;
        }
    }
    return 1;
}

void vmm_free(uint64_t virt_base, uint64_t n) {
    for (uint64_t i = 0; i < n; i++) {
        uint64_t virt = virt_base + i * 4096;
        uint64_t phys = vmm_virt_to_phys(virt);
        vmm_unmap(virt);
        if (phys) pmm_free(phys);
    }
}
