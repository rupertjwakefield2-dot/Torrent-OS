#include "desktop.h"
#include "print.h"
#include "auth.h"
#include "rtc.h"
#include "battery.h"
#include "str.h"

static Color sb_fg(void) { return auth_dark_mode ? COLOR_WHITE     : COLOR_DARK_GRAY; }
static Color sb_bg(void) { return auth_dark_mode ? COLOR_DARK_GRAY : COLOR_WHITE; }

void desktop_redraw_bar(void) {
    /* left: "  TorrentOS" */
    const char *left = "  TorrentOS";

    /* right: "username  HH:MM  AC " */
    char right[64] = "";
    str_cat(right, auth_username, sizeof(right));
    str_cat(right, "  ",         sizeof(right));

    char timebuf[8];
    rtc_fmt_time(timebuf);
    str_cat(right, timebuf, sizeof(right));
    str_cat(right, "  ",    sizeof(right));

    char batbuf[16];
    battery_fmt(batbuf);
    str_cat(right, batbuf, sizeof(right));
    str_cat(right, " ",    sizeof(right));

    print_statusbar(left, right, sb_fg(), sb_bg());
}

void desktop_init(void) {
    desktop_redraw_bar();
}

void desktop_tick(void) {
    desktop_redraw_bar();
}
