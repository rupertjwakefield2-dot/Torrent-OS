/* ATA stub for AArch64 — use VirtIO or PL061 instead (Stage 5) */
#include "ata.h"
int ata_init(void)                                           { return 0; }
int ata_present(void)                                        { return 0; }
int ata_read(uint32_t l, uint8_t *b, uint32_t s)            { (void)l;(void)b;(void)s; return 0; }
int ata_write(uint32_t l, const uint8_t *b, uint32_t s)     { (void)l;(void)b;(void)s; return 0; }
