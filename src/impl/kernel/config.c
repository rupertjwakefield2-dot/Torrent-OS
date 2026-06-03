#include "config.h"
#include "ata.h"
#include "auth.h"
#include "battery.h"
#include "rtc.h"
#include "str.h"

#define CFG_MAGIC   0x544F5301u   /* "TOS\x01" */
#define CFG_LBA     1             /* store in sector 1 (sector 0 = MBR) */

/* In-memory config buffer — one ATA sector */
static uint8_t  cfg_buf[ATA_SECTOR_SIZE];

/* Offsets into cfg_buf */
#define OFF_MAGIC    0   /* 4 bytes */
#define OFF_USER     4   /* 65 bytes (64+NUL) */
#define OFF_PASS    69   /* 101 bytes (100+NUL) */
#define OFF_TZ     170   /* 1 byte signed */
#define OFF_DARK   171   /* 1 byte 0/1 */
#define OFF_SAVER  172   /* 1 byte 0/1 */

static void write32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
static uint32_t read32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8)
         | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

int config_load(void) {
    if (!ata_present()) return 0;
    if (!ata_read(CFG_LBA, cfg_buf, 1)) return 0;
    if (read32(cfg_buf + OFF_MAGIC) != CFG_MAGIC) return 0;
    return 1;
}

int config_save(void) {
    if (!ata_present()) return 0;
    return ata_write(CFG_LBA, cfg_buf, 1);
}

void config_apply(void) {
    /* copy disk data → globals */
    str_cpy(auth_username, (char*)(cfg_buf + OFF_USER), AUTH_USER_MAX);
    /* password lives in auth.c — expose it via a setter */
    extern int auth_set_password(const char*, const char*);
    auth_set_password((char*)(cfg_buf + OFF_PASS),
                      (char*)(cfg_buf + OFF_PASS));
    rtc_set_tz_offset((int8_t)cfg_buf[OFF_TZ]);
    auth_dark_mode = cfg_buf[OFF_DARK];
    battery_saver_set(cfg_buf[OFF_SAVER]);
}

int config_commit(void) {
    /* copy globals → disk buffer, then save */
    write32(cfg_buf, CFG_MAGIC);

    str_cpy((char*)(cfg_buf + OFF_USER), auth_username, 65);

    /* grab password from auth module via a getter we'll add */
    extern const char *auth_password_ptr(void);
    str_cpy((char*)(cfg_buf + OFF_PASS), auth_password_ptr(), 101);

    cfg_buf[OFF_TZ]    = (uint8_t)(int8_t)rtc_tz_offset();
    cfg_buf[OFF_DARK]  = (uint8_t)auth_dark_mode;
    cfg_buf[OFF_SAVER] = (uint8_t)battery_saver_enabled();

    return config_save();
}
