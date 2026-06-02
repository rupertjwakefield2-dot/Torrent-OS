#pragma once
#include <stdint.h>

/*
 * Persistent config stored in LBA sector 1 of the ATA hard disk.
 * If no disk is present, config lives in RAM only (lost on reboot).
 *
 * Layout: magic(4) + username(65) + password(101) + tz_offset(1)
 *         + dark_mode(1) + bat_saver(1) = 173 bytes (fits in 512-byte sector).
 */

int  config_load(void);   /* returns 1 if valid config read from disk */
int  config_save(void);   /* returns 1 on success */
void config_apply(void);  /* copy loaded values into auth/battery/rtc globals */
void config_commit(void); /* copy current globals back to config buffer and save */
