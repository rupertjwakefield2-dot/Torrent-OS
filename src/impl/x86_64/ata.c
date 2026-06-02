#include "ata.h"
#include "io.h"
#include "timer.h"

/* Primary ATA bus registers */
#define ATA_DATA        0x1F0
#define ATA_ERR         0x1F1
#define ATA_SECT_CNT    0x1F2
#define ATA_LBA_LO      0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HI      0x1F5
#define ATA_DRIVE       0x1F6
#define ATA_STATUS      0x1F7
#define ATA_CMD         0x1F7

/* Status bits */
#define ATA_SR_BSY  0x80
#define ATA_SR_DRDY 0x40
#define ATA_SR_DRQ  0x08
#define ATA_SR_ERR  0x01

/* Commands */
#define ATA_CMD_READ_PIO  0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_IDENTIFY  0xEC

/* 400ns delay — read alt-status 4 times */
static void ata_delay(void) {
    inb(0x3F6); inb(0x3F6); inb(0x3F6); inb(0x3F6);
}

static int ata_wait_busy(void) {
    /* wait up to ~30s for BSY to clear */
    for (int i = 0; i < 30000; i++) {
        if (!(inb(ATA_STATUS) & ATA_SR_BSY)) return 1;
        timer_sleep(1);
    }
    return 0;  /* timeout */
}

static int ata_wait_drq(void) {
    for (int i = 0; i < 30000; i++) {
        uint8_t s = inb(ATA_STATUS);
        if (s & ATA_SR_ERR)  return 0;
        if (s & ATA_SR_DRQ)  return 1;
        timer_sleep(1);
    }
    return 0;
}

static int drive_present = 0;

int ata_init(void) {
    drive_present = 0;

    /* select master, LBA mode */
    outb(ATA_DRIVE, 0xA0);
    ata_delay();

    /* send IDENTIFY */
    outb(ATA_SECT_CNT, 0);
    outb(ATA_LBA_LO,   0);
    outb(ATA_LBA_MID,  0);
    outb(ATA_LBA_HI,   0);
    outb(ATA_CMD, ATA_CMD_IDENTIFY);

    uint8_t status = inb(ATA_STATUS);
    if (status == 0x00 || status == 0xFF) return 0;  /* no drive */

    if (!ata_wait_busy()) return 0;

    /* check for ATAPI (non-ATA) */
    if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HI) != 0) return 0;

    if (!ata_wait_drq()) return 0;

    /* read 256 words of identity data (discard) */
    for (int i = 0; i < 256; i++) inw(ATA_DATA);

    drive_present = 1;
    return 1;
}

int ata_present(void) { return drive_present; }

int ata_read(uint32_t lba, uint8_t *buf, uint32_t sectors) {
    if (!drive_present) return 0;
    for (uint32_t s = 0; s < sectors; s++) {
        uint32_t cur = lba + s;

        if (!ata_wait_busy()) return 0;

        outb(ATA_DRIVE,    (uint8_t)(0xE0 | ((cur >> 24) & 0x0F)));
        outb(ATA_SECT_CNT, 1);
        outb(ATA_LBA_LO,   (uint8_t)( cur        & 0xFF));
        outb(ATA_LBA_MID,  (uint8_t)((cur >>  8) & 0xFF));
        outb(ATA_LBA_HI,   (uint8_t)((cur >> 16) & 0xFF));
        outb(ATA_CMD,      ATA_CMD_READ_PIO);

        ata_delay();
        if (!ata_wait_busy()) return 0;
        if (!ata_wait_drq())  return 0;
        if (inb(ATA_STATUS) & ATA_SR_ERR) return 0;

        uint8_t *dst = buf + s * ATA_SECTOR_SIZE;
        for (int i = 0; i < 256; i++) {
            uint16_t w = inw(ATA_DATA);
            dst[i * 2]     = (uint8_t)(w & 0xFF);
            dst[i * 2 + 1] = (uint8_t)(w >> 8);
        }
    }
    return 1;
}

int ata_write(uint32_t lba, const uint8_t *buf, uint32_t sectors) {
    if (!drive_present) return 0;
    for (uint32_t s = 0; s < sectors; s++) {
        uint32_t cur = lba + s;

        if (!ata_wait_busy()) return 0;

        outb(ATA_DRIVE,    (uint8_t)(0xE0 | ((cur >> 24) & 0x0F)));
        outb(ATA_SECT_CNT, 1);
        outb(ATA_LBA_LO,   (uint8_t)( cur        & 0xFF));
        outb(ATA_LBA_MID,  (uint8_t)((cur >>  8) & 0xFF));
        outb(ATA_LBA_HI,   (uint8_t)((cur >> 16) & 0xFF));
        outb(ATA_CMD,      ATA_CMD_WRITE_PIO);

        ata_delay();
        if (!ata_wait_busy()) return 0;
        if (!ata_wait_drq())  return 0;

        const uint8_t *src = buf + s * ATA_SECTOR_SIZE;
        for (int i = 0; i < 256; i++) {
            uint16_t w = (uint16_t)(src[i * 2] | ((uint16_t)src[i * 2 + 1] << 8));
            outw(ATA_DATA, w);
        }
        /* flush write cache */
        outb(ATA_CMD, 0xE7);
        if (!ata_wait_busy()) return 0;
    }
    return 1;
}
