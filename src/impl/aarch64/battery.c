/* Battery stub for AArch64 */
#include "battery.h"
#include "str.h"
static int saver = 0;
void     battery_init(void)           { }
BatState battery_state(void)          { return BAT_AC; }
uint8_t  battery_percent(void)        { return 0xFF; }
int      battery_saver_enabled(void)  { return saver; }
void     battery_saver_set(int on)    { saver = on; }
void     battery_fmt(char *buf)       { str_cpy(buf, saver ? "AC  [SAVER]" : "AC", 16); }
