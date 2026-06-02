#include "appstore.h"
#include "print.h"
#include "keyboard.h"
#include "browser.h"
#include "auth.h"
#include "desktop.h"
#include "battery.h"
#include "pci.h"
#include "rtc.h"
#include "str.h"

typedef enum { ACT_NONE, ACT_BROWSER, ACT_SETTINGS, ACT_SYSINFO, ACT_EXIT } AppAction;

typedef struct {
    const char *name;
    const char *desc;
    AppAction   action;
} App;

static const App apps[] = {
    { "Browser",      "Text-mode web browser (about: pages)",  ACT_BROWSER   },
    { "Settings",     "Theme, battery saver, timezone",         ACT_SETTINGS  },
    { "System Info",  "Hardware and OS details",                ACT_SYSINFO   },
    { "Exit",         "Close the App Store",                    ACT_EXIT      },
};
#define NAPP ((int)(sizeof(apps)/sizeof(apps[0])))

static Color as_bg(void)  { return auth_dark_mode ? COLOR_BLACK      : COLOR_LIGHT_GRAY; }
static Color as_fg(void)  { return auth_dark_mode ? COLOR_LIGHT_GRAY : COLOR_BLACK;      }
static Color as_acc(void) { return auth_dark_mode ? COLOR_LIGHT_CYAN : COLOR_CYAN;       }
static Color as_sel_bg(void) { return auth_dark_mode ? COLOR_DARK_GRAY : COLOR_LIGHT_BLUE; }

static void draw_store(int sel) {
    desktop_redraw_bar();
    print_set_color(as_fg(), as_bg());
    print_clear();

    /* header */
    print_center_str(2, "TorrentOS  App Store", as_acc(), as_bg());
    print_hline(10, 3, 60, COLOR_DARK_GRAY, as_bg());

    /* app list */
    for (int i = 0; i < NAPP; i++) {
        uint8_t row  = (uint8_t)(5 + i * 3);
        Color   bfg  = (i == sel) ? COLOR_WHITE  : as_fg();
        Color   bbg  = (i == sel) ? as_sel_bg()  : as_bg();
        Color   dfg  = (i == sel) ? COLOR_YELLOW : COLOR_DARK_GRAY;

        /* clear rows */
        print_clear_row(row,          bfg, bbg);
        print_clear_row((uint8_t)(row+1), dfg, bbg);

        /* app name */
        char line[80] = "  ";
        if (i == sel) str_cat(line, "\x10 ", 80);   /* CP437 ► */
        else          str_cat(line, "  ",    80);
        str_cat(line, apps[i].name, 80);
        print_str_at(10, row, line, bfg, bbg);

        /* description */
        char desc[80] = "       ";
        str_cat(desc, apps[i].desc, 80);
        print_str_at(10, (uint8_t)(row + 1), desc, dfg, bbg);
    }

    print_hline(10, 5 + NAPP * 3, 60, COLOR_DARK_GRAY, as_bg());
    print_str_at(10, (uint8_t)(6 + NAPP * 3),
        "Up/Down: navigate   Enter: open   Q/ESC: close",
        COLOR_DARK_GRAY, as_bg());
}

/* ── settings sub-menu ── */
static void run_settings(void) {
    print_set_color(as_fg(), as_bg());
    print_clear();
    desktop_redraw_bar();

    for (;;) {
        print_set_color(as_fg(), as_bg());
        /* don't call print_clear() — just redraw rows */
        for (int r = TERM_TOP; r < VGA_ROWS; r++)
            print_clear_row((uint8_t)r, as_fg(), as_bg());

        print_center_str(2, "Settings", as_acc(), as_bg());
        print_hline(20, 3, 40, COLOR_DARK_GRAY, as_bg());

        /* dark mode */
        char dm[60] = "  [1] Theme:        ";
        str_cat(dm, auth_dark_mode ? "Dark Mode" : "Light Mode", 60);
        print_str_at(20, 5, dm, as_fg(), as_bg());

        /* battery saver */
        char bs[60] = "  [2] Battery Saver: ";
        str_cat(bs, battery_saver_enabled() ? "ON " : "OFF", 60);
        print_str_at(20, 7, bs, as_fg(), as_bg());

        print_str_at(20, 10,
            "  Press 1 or 2 to toggle.  Q/ESC to go back.",
            COLOR_DARK_GRAY, as_bg());

        print_move(0, 23);
        int k = keyboard_getkey();
        if (k == '1') {
            auth_dark_mode ^= 1;
            /* recolour background */
            for (int r = TERM_TOP; r < VGA_ROWS; r++)
                print_clear_row((uint8_t)r, as_fg(), as_bg());
            desktop_redraw_bar();
        } else if (k == '2') {
            battery_saver_set(!battery_saver_enabled());
        } else if (k == 'q' || k == 'Q' || k == KEY_ESC) {
            break;
        }
    }
}

/* ── sysinfo sub-page ── */
static void run_sysinfo(void) {
    print_set_color(as_fg(), as_bg());
    print_clear();
    desktop_redraw_bar();

    print_center_str(2, "System Information", as_acc(), as_bg());
    print_hline(10, 3, 60, COLOR_DARK_GRAY, as_bg());

    uint8_t r = 5;
    char buf[80];

    str_cpy(buf, "  OS:        TorrentOS v0.1-dev (x86_64)", 80);
    print_str_at(10, r++, buf, as_fg(), as_bg());

    str_cpy(buf, "  User:      ", 80); str_cat(buf, auth_username, 80);
    print_str_at(10, r++, buf, as_fg(), as_bg());

    char tbuf[8]; char dbuf[12];
    rtc_fmt_time(tbuf); rtc_fmt_date(dbuf);

    /* Need rtc headers - but they're included via auth/desktop chain... */
    str_cpy(buf, "  Date/Time: ", 80);
    str_cat(buf, dbuf, 80); str_cat(buf, "  ", 80); str_cat(buf, tbuf, 80);
    print_str_at(10, r++, buf, as_fg(), as_bg());

    r++;
    print_str_at(10, r++, "  Hardware:", as_acc(), as_bg());

    str_cpy(buf, "  WiFi:      ", 80);
    str_cat(buf, pci_has_wifi() ? pci_wifi_name() : "Not detected", 80);
    print_str_at(10, r++, buf, as_fg(), as_bg());

    str_cpy(buf, "  Bluetooth: ", 80);
    str_cat(buf, pci_has_bluetooth() ? pci_bt_name() : "Not detected", 80);
    print_str_at(10, r++, buf, as_fg(), as_bg());

    char batbuf[16]; battery_fmt(batbuf);
    str_cpy(buf, "  Power:     ", 80); str_cat(buf, batbuf, 80);
    print_str_at(10, r++, buf, as_fg(), as_bg());

    print_str_at(10, 22, "  Press any key to go back.", COLOR_DARK_GRAY, as_bg());
    keyboard_getkey();
}

void appstore_run(void) {
    int sel = 0;
    draw_store(sel);

    for (;;) {
        int k = keyboard_getkey();

        if (k == 'q' || k == 'Q' || k == KEY_ESC) break;
        if (k == KEY_UP   && sel > 0)         { sel--; draw_store(sel); }
        if (k == KEY_DOWN && sel < NAPP - 1)  { sel++; draw_store(sel); }

        if (k == '\n') {
            switch (apps[sel].action) {
                case ACT_BROWSER:  browser_run();  break;
                case ACT_SETTINGS: run_settings(); break;
                case ACT_SYSINFO:  run_sysinfo();  break;
                case ACT_EXIT:     goto done;
                default:           break;
            }
            draw_store(sel);   /* redraw after returning from sub-app */
        }
    }
done:
    print_set_color(as_fg(), as_bg());
    print_clear();
    desktop_redraw_bar();
}
