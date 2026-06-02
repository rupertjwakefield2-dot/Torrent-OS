#include "battery.h"
#include "str.h"

/*
 * Full ACPI battery parsing requires a significant AML interpreter.
 * In QEMU/VirtualBox there is no battery anyway, so we always report AC.
 * Battery-saver mode is a software flag that dims output color hints.
 */

static int saver_on = 0;

void     battery_init(void)               { }
BatState battery_state(void)              { return BAT_AC; }
uint8_t  battery_percent(void)            { return 0xFF; }
int      battery_saver_enabled(void)      { return saver_on; }
void     battery_saver_set(int on)        { saver_on = on; }

void battery_fmt(char *buf) {
    if (saver_on) {
        str_cpy(buf, "AC  [SAVER]", 16);
    } else {
        buf[0] = 'A'; buf[1] = 'C'; buf[2] = '\0';
    }
}
