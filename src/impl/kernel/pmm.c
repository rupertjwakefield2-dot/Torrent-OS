#include "pmm.h"
#include "print.h"

#define PAGE_SIZE  4096
#define MAX_FRAMES 65536    /* supports up to 256 MB RAM */

/* ── multiboot2 structures (subset we need) ─────────────── */

typedef struct __attribute__((packed)) {
    uint32_t total_size;
    uint32_t reserved;
} MB2Header;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t size;
} MB2Tag;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} MB2MmapTag;

typedef struct __attribute__((packed)) {
    uint64_t base;
    uint64_t length;
    uint32_t mtype;
    uint32_t reserved;
} MB2MmapEntry;

#define MB2_TAG_MMAP 6
#define MB2_MEM_AVAIL 1

/* ── bitmap ─────────────────────────────────────────────── */

static uint8_t bitmap[MAX_FRAMES / 8];   /* 1 bit per 4 KB frame */
static size_t  total = 0;
static size_t  free_count = 0;

extern char kernel_end;   /* provided by linker script */

static void bm_set(size_t frame)   { bitmap[frame / 8] |=  (uint8_t)(1 << (frame % 8)); }
static void bm_clear(size_t frame) { bitmap[frame / 8] &= (uint8_t)~(1 << (frame % 8)); }
static int  bm_test(size_t frame)  { return bitmap[frame / 8] & (1 << (frame % 8)); }

/* ── init ────────────────────────────────────────────────── */

void pmm_init(uint32_t mb_addr) {
    /* mark everything as used */
    for (size_t i = 0; i < MAX_FRAMES / 8; i++) bitmap[i] = 0xFF;

    MB2Header *hdr  = (MB2Header*)(uint64_t)mb_addr;
    uint8_t   *tag  = (uint8_t*)(hdr + 1);
    uint8_t   *end  = (uint8_t*)(uint64_t)mb_addr + hdr->total_size;

    while (tag < end) {
        MB2Tag *t = (MB2Tag*)tag;
        if (t->type == 0) break;    /* end tag */

        if (t->type == MB2_TAG_MMAP) {
            MB2MmapTag *mm = (MB2MmapTag*)t;
            uint8_t    *entry = (uint8_t*)(mm + 1);
            uint8_t    *mend  = tag + mm->size;

            while (entry < mend) {
                MB2MmapEntry *e = (MB2MmapEntry*)entry;
                if (e->mtype == MB2_MEM_AVAIL) {
                    uint64_t base  = (e->base + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
                    uint64_t limit = (e->base + e->length) & ~(uint64_t)(PAGE_SIZE - 1);
                    for (uint64_t a = base; a < limit; a += PAGE_SIZE) {
                        size_t frame = (size_t)(a / PAGE_SIZE);
                        if (frame < MAX_FRAMES) {
                            bm_clear(frame);
                            total++;
                            free_count++;
                        }
                    }
                }
                entry += mm->entry_size;
            }
        }

        /* advance to next tag (8-byte aligned) */
        uint32_t sz = t->size;
        tag += (sz + 7) & ~7u;
    }

    /* re-mark frame 0 and anything below 1 MB as used (BIOS, VGA, etc.) */
    uint64_t low = (uint64_t)(uintptr_t)&kernel_end;
    low = (low + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
    for (uint64_t a = 0; a < low; a += PAGE_SIZE) {
        size_t frame = (size_t)(a / PAGE_SIZE);
        if (frame < MAX_FRAMES && !bm_test(frame)) {
            bm_set(frame);
            free_count--;
        }
    }
}

/* ── fixed-range init for ARM64 (no multiboot2) ── */
void pmm_init_fixed(uint64_t mem_start, uint64_t mem_end) {
    for (size_t i = 0; i < MAX_FRAMES / 8; i++) bitmap[i] = 0xFF;
    total = 0; free_count = 0;
    uint64_t base  = (mem_start + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
    uint64_t limit = mem_end & ~(uint64_t)(PAGE_SIZE - 1);
    for (uint64_t a = base; a < limit; a += PAGE_SIZE) {
        size_t frame = (size_t)(a / PAGE_SIZE);
        if (frame < MAX_FRAMES) {
            bm_clear(frame);
            total++;
            free_count++;
        }
    }
}

uint64_t pmm_alloc(void) {
    for (size_t i = 0; i < MAX_FRAMES; i++) {
        if (!bm_test(i)) {
            bm_set(i);
            free_count--;
            return (uint64_t)i * PAGE_SIZE;
        }
    }
    return 0;   /* out of memory */
}

void pmm_free(uint64_t addr) {
    size_t frame = (size_t)(addr / PAGE_SIZE);
    if (frame < MAX_FRAMES && bm_test(frame)) {
        bm_clear(frame);
        free_count++;
    }
}

size_t pmm_free_frames(void)  { return free_count; }
size_t pmm_total_frames(void) { return total; }
