#include "heap.h"
#include "pmm.h"

#define PAGE_SIZE 4096
#define HEAP_MAGIC 0xB00B5

typedef struct Block {
    size_t       size;
    int          free;
    uint32_t     magic;
    struct Block *next;
} Block;

static Block *head = 0;
static size_t used_bytes = 0;

/* grab a fresh page from the PMM and carve one block from it */
static Block *new_page(void) {
    uint64_t phys = pmm_alloc();
    if (!phys) return 0;
    /* identity mapped: physical == virtual */
    Block *b   = (Block*)(uintptr_t)phys;
    b->size    = PAGE_SIZE - sizeof(Block);
    b->free    = 1;
    b->magic   = HEAP_MAGIC;
    b->next    = 0;
    return b;
}

void heap_init(void) {
    head = new_page();
}

void *kmalloc(size_t size) {
    if (!size) return 0;
    /* align to 8 bytes */
    size = (size + 7) & ~(size_t)7;

    Block *b    = head;
    Block *prev = 0;

    /* first-fit search */
    while (b) {
        if (b->free && b->size >= size) {
            /* split if there's enough room for a new block header + 8 bytes */
            if (b->size >= size + sizeof(Block) + 8) {
                Block *nb = (Block*)((uint8_t*)b + sizeof(Block) + size);
                nb->size  = b->size - size - sizeof(Block);
                nb->free  = 1;
                nb->magic = HEAP_MAGIC;
                nb->next  = b->next;
                b->next   = nb;
                b->size   = size;
            }
            b->free = 0;
            used_bytes += b->size;
            return (uint8_t*)b + sizeof(Block);
        }
        prev = b;
        b    = b->next;
    }

    /* no free block found — get another page */
    Block *nb = new_page();
    if (!nb) return 0;
    if (prev) prev->next = nb;
    else      head       = nb;
    /* retry (tail-recursive equivalent) */
    return kmalloc(size);
}

void kfree(void *ptr) {
    if (!ptr) return;
    Block *b = (Block*)((uint8_t*)ptr - sizeof(Block));
    if (b->magic != HEAP_MAGIC || b->free) return;
    b->free = 1;
    used_bytes -= b->size;

    /* coalesce adjacent free blocks */
    Block *cur = head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += sizeof(Block) + cur->next->size;
            cur->next  = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

size_t heap_used(void) { return used_bytes; }
