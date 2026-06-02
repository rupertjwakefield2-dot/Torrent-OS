#pragma once
#include <stdint.h>

#define ATA_SECTOR_SIZE  512

/*
 * ATA PIO (28-bit LBA) driver — primary bus only.
 * Returns 1 on success, 0 on error / no drive.
 */
int ata_init(void);               /* detect drive; returns 1 if present */
int ata_read(uint32_t lba, uint8_t *buf, uint32_t sectors);
int ata_write(uint32_t lba, const uint8_t *buf, uint32_t sectors);
int ata_present(void);            /* 1 if drive was detected at init */
