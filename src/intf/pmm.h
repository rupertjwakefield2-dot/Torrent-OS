#pragma once
#include <stdint.h>
#include <stddef.h>

void     pmm_init(uint32_t mb_addr);             /* x86_64 multiboot2 */
void     pmm_init_fixed(uint64_t start, uint64_t end); /* AArch64 flat map */
uint64_t pmm_alloc(void);         /* returns physical address of one 4 KB frame, or 0 */
void     pmm_free(uint64_t addr);
size_t   pmm_free_frames(void);
size_t   pmm_total_frames(void);
