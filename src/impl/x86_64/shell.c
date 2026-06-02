#include "shell.h"
#include "print.h"
#include "keyboard.h"
#include "timer.h"
#include "pmm.h"
#include "heap.h"
#include "io.h"
#include "str.h"
#include "auth.h"
#include "rtc.h"
#include "battery.h"
#include "pci.h"
#include "browser.h"
#include "appstore.h"
#include "desktop.h"
#include "vfs.h"
#include "config.h"
#include "ata.h"
#include "speaker.h"

#define LINE_MAX  256
#define HIST_SZ   16

/* current working directory */
static char cwd[128] = "/";

/* ── sorted command list (tab completion) ── */
static const char *cmd_list[] = {
    "apps", "battery", "bluetooth", "browser", "cat", "clear",
    "darkmode", "date", "echo", "halt", "help", "history",
    "ls", "mem", "pwd", "reboot", "save", "settings", "sysinfo",
    "timezone", "uptime", "ver", "whoami", "wifi",
    (void*)0
};

/* ── history ── */
static char history[HIST_SZ][LINE_MAX];
static int  hist_count = 0;

static void hist_push(const char *line) {
    if (!line[0]) return;
    if (hist_count > 0 && str_eq(history[0], line)) return;
    int slots = hist_count < HIST_SZ ? hist_count : HIST_SZ - 1;
    for (int i = slots; i > 0; i--)
        str_cpy(history[i], history[i - 1], LINE_MAX);
    str_cpy(history[0], line, LINE_MAX);
    if (hist_count < HIST_SZ) hist_count++;
}

/* ── colour theme ── */
static Color sh_bg(void)  { return auth_dark_mode ? COLOR_BLACK      : COLOR_LIGHT_GRAY; }
static Color sh_fg(void)  { return auth_dark_mode ? COLOR_LIGHT_GRAY : COLOR_BLACK;      }
static Color sh_acc(void) { return auth_dark_mode ? COLOR_LIGHT_CYAN : COLOR_CYAN;       }

/* ── macOS-style prompt ── */
static void print_prompt(void) {
    Color bg = sh_bg();
    print_set_color(COLOR_LIGHT_GREEN, bg); print_str(auth_username);
    print_set_color(COLOR_DARK_GRAY,   bg); print_str("@torrentos:");
    print_set_color(COLOR_YELLOW,      bg); print_str("~");
    print_set_color(COLOR_LIGHT_GRAY,  bg); print_str(" % ");
}

/* ── readline with history + tab completion ── */

static void redraw_line(uint8_t sc, uint8_t sr, const char *buf, int len, int pos) {
    print_set_color(sh_fg(), sh_bg());
    print_move(sc, sr);
    for (int i = 0; i < len; i++) print_char(buf[i]);
    print_char(' ');   /* erase any char behind deleted content */
    print_move((uint8_t)(sc + pos), sr);
}

static int readline(char *out, size_t max) {
    char buf[LINE_MAX];
    int  len = 0, pos = 0, hi = -1;

    print_set_color(sh_fg(), sh_bg());
    uint8_t sc = print_col(), sr = print_row();

    for (;;) {
        int k = keyboard_getkey();

        /* ── submit ── */
        if (k == '\n') {
            print_newline();
            buf[len] = '\0';
            str_cpy(out, buf, max);
            return len;
        }

        /* ── backspace / delete ── */
        if (k == '\b' || k == 0x7F) {
            if (pos > 0) {
                for (int i = pos - 1; i < len - 1; i++) buf[i] = buf[i + 1];
                len--; pos--;
                redraw_line(sc, sr, buf, len, pos);
            }
            continue;
        }
        if (k == KEY_DEL) {
            if (pos < len) {
                for (int i = pos; i < len - 1; i++) buf[i] = buf[i + 1];
                len--;
                redraw_line(sc, sr, buf, len, pos);
            }
            continue;
        }

        /* ── cursor movement ── */
        if (k == KEY_LEFT)  { if (pos > 0)   { pos--; print_move((uint8_t)(sc+pos), sr); } continue; }
        if (k == KEY_RIGHT) { if (pos < len)  { pos++; print_move((uint8_t)(sc+pos), sr); } continue; }
        if (k == KEY_HOME)  { pos = 0;   print_move(sc, sr);              continue; }
        if (k == KEY_END)   { pos = len; print_move((uint8_t)(sc+pos), sr); continue; }

        /* ── history navigation ── */
        if (k == KEY_UP) {
            if (hi + 1 < hist_count) {
                hi++;
                str_cpy(buf, history[hi], LINE_MAX);
                len = pos = (int)str_len(buf);
                redraw_line(sc, sr, buf, len, pos);
            }
            continue;
        }
        if (k == KEY_DOWN) {
            if (hi > 0) {
                hi--;
                str_cpy(buf, history[hi], LINE_MAX);
                len = pos = (int)str_len(buf);
                redraw_line(sc, sr, buf, len, pos);
            } else if (hi == 0) {
                hi = -1; buf[0] = '\0'; len = pos = 0;
                redraw_line(sc, sr, buf, len, pos);
            }
            continue;
        }

        /* ── tab completion ── */
        if (k == '\t') {
            buf[len] = '\0';
            const char *only = (void*)0;
            int         n    = 0;
            for (int i = 0; cmd_list[i]; i++) {
                if (str_pfx(cmd_list[i], buf)) { only = cmd_list[i]; n++; }
            }
            if (n == 1) {
                /* unique match — complete in place */
                str_cpy(buf, only, LINE_MAX);
                len = pos = (int)str_len(buf);
                redraw_line(sc, sr, buf, len, pos);
            } else if (n > 1) {
                /* show all matches on a new line */
                print_move((uint8_t)(sc + len), sr);
                print_newline();
                print_set_color(COLOR_DARK_GRAY, sh_bg());
                for (int i = 0; cmd_list[i]; i++)
                    if (str_pfx(cmd_list[i], buf)) {
                        print_str("  "); print_str(cmd_list[i]);
                    }
                print_newline();
                /* reprint prompt + current input on new row */
                print_set_color(sh_fg(), sh_bg());
                print_prompt();
                sc = print_col(); sr = print_row();
                for (int i = 0; i < len; i++) print_char(buf[i]);
                print_move((uint8_t)(sc + pos), sr);
            }
            continue;
        }

        /* ── Ctrl+C — cancel line ── */
        if (k == 0x03) {
            print_set_color(COLOR_DARK_GRAY, sh_bg());
            print_str("^C");
            print_newline();
            print_set_color(sh_fg(), sh_bg());
            out[0] = '\0';
            return 0;
        }

        if (k == KEY_ESC) continue;

        /* ── printable character ── */
        if (k >= 0x20 && k < 0x7F && len < (int)max - 1) {
            for (int i = len; i > pos; i--) buf[i] = buf[i - 1];
            buf[pos++] = (char)k;
            len++;
            redraw_line(sc, sr, buf, len, pos);
        }
    }
}

/* ── visual progress bar ── */

static void print_bar(size_t used, size_t total, int width, Color used_c, Color free_c) {
    int filled = (total > 0) ? (int)((uint64_t)used * (uint64_t)width / total) : 0;
    if (filled > width) filled = width;
    print_char('[');
    for (int i = 0; i < width; i++) {
        if (i < filled) {
            print_set_color(used_c, sh_bg());
            print_char('\xDB');   /* █ */
        } else {
            print_set_color(free_c, sh_bg());
            print_char('\xB0');   /* ░ */
        }
    }
    print_set_color(sh_fg(), sh_bg());
    print_char(']');
}

/* ── command implementations ── */

static void cmd_help(void) {
    print_set_color(sh_acc(), sh_bg());
    print_str("  Commands\n");
    print_set_color(COLOR_DARK_GRAY, sh_bg());
    print_str("  ");
    for (int i = 0; i < 46; i++) print_char('\xC4');
    print_newline();

    struct { const char *cmd; const char *desc; } rows[] = {
        { "help",          "show this list"                      },
        { "clear",         "clear the screen"                    },
        { "sysinfo",       "system information (neofetch-style)" },
        { "ver",           "OS version"                          },
        { "uptime",        "system uptime"                       },
        { "mem",           "memory usage with visual bar"        },
        { "date",          "current date, time and timezone"     },
        { "whoami",        "current user"                        },
        { "echo <text>",   "print text"                          },
        { "history",       "show command history"                },
        { "ls [dir]",      "list virtual filesystem directory"   },
        { "cat <file>",    "print file contents"                 },
        { "pwd",           "print working directory"             },
        { "save",          "save settings to disk"               },
        { "wifi",          "WiFi adapter status"                 },
        { "bluetooth",     "Bluetooth adapter status"            },
        { "timezone [±N]", "show or set UTC offset  e.g. +8"    },
        { "darkmode",      "toggle dark / light theme"           },
        { "battery",       "power status"                        },
        { "browser",       "open the text browser"               },
        { "apps",          "open the App Store / launcher"       },
        { "settings",      "theme and battery-saver settings"    },
        { "halt",          "power off"                           },
        { "reboot",        "restart"                             },
        { (void*)0, (void*)0 },
    };
    for (int i = 0; rows[i].cmd; i++) {
        print_set_color(sh_acc(), sh_bg());
        print_str("  ");
        /* left-pad cmd to 18 chars */
        int cw = (int)str_len(rows[i].cmd);
        print_str(rows[i].cmd);
        for (int j = cw; j < 18; j++) print_char(' ');
        print_set_color(sh_fg(), sh_bg());
        print_str(rows[i].desc);
        print_newline();
    }
    print_set_color(COLOR_DARK_GRAY, sh_bg());
    print_str("  Tip: press Tab to auto-complete commands.\n");
    print_set_color(sh_fg(), sh_bg());
}

static void cmd_history(void) {
    if (hist_count == 0) {
        print_set_color(COLOR_DARK_GRAY, sh_bg());
        print_str("  No history yet.\n");
        print_set_color(sh_fg(), sh_bg());
        return;
    }
    print_set_color(sh_acc(), sh_bg());
    print_str("  Command History\n");
    print_set_color(sh_fg(), sh_bg());
    for (int i = hist_count - 1; i >= 0; i--) {
        print_set_color(COLOR_DARK_GRAY, sh_bg());
        print_str("  ");
        char num[4];
        str_fmtuint(num, sizeof(num), (uint64_t)(hist_count - i));
        /* right-align number in 3 chars */
        int nw = (int)str_len(num);
        for (int j = nw; j < 3; j++) print_char(' ');
        print_str(num);
        print_str("  ");
        print_set_color(sh_fg(), sh_bg());
        print_str(history[i]);
        print_newline();
    }
}

static void cmd_ver(void) {
    print_set_color(sh_acc(), sh_bg());
    print_str("  TorrentOS  v0.1-dev  (x86_64)\n");
    print_set_color(COLOR_DARK_GRAY, sh_bg());
    print_str("  Built from scratch in C + NASM  \xFA  no Linux\n");
    print_set_color(sh_fg(), sh_bg());
}

static void cmd_uptime(void) {
    uint64_t t = timer_ticks();
    uint64_t s = t / 1000, m = s / 60, h = m / 60;
    s %= 60; m %= 60;
    print_set_color(sh_fg(), sh_bg());
    print_str("  Uptime  ");
    print_set_color(sh_acc(), sh_bg());
    print_uint(h); print_str("h ");
    print_uint(m); print_str("m ");
    print_uint(s); print_str("s");
    print_set_color(COLOR_DARK_GRAY, sh_bg());
    print_str("  ("); print_uint(t); print_str(" ticks)\n");
    print_set_color(sh_fg(), sh_bg());
}

static void cmd_mem(void) {
    size_t total = pmm_total_frames() * 4096;
    size_t free_ = pmm_free_frames()  * 4096;
    size_t used  = total - free_;
    size_t heap  = heap_used();

    print_set_color(sh_acc(), sh_bg());
    print_str("  Physical RAM\n");

    print_set_color(sh_fg(), sh_bg());
    print_str("  ");
    print_bar(used, total, 32,
              auth_dark_mode ? COLOR_LIGHT_CYAN : COLOR_CYAN,
              COLOR_DARK_GRAY);
    print_str("  ");
    print_uint(used / 1024 / 1024);
    print_str(" MB / ");
    print_uint(total / 1024 / 1024);
    print_str(" MB  (");
    if (total > 0) print_uint((uint64_t)used * 100 / total);
    else           print_char('0');
    print_str("%)\n");

    print_set_color(sh_acc(), sh_bg());
    print_str("  Heap\n");
    print_set_color(sh_fg(), sh_bg());
    print_str("  ");
    /* heap bar relative to 1 MB */
    size_t heap_max = 1024 * 1024;
    if (heap > heap_max) heap_max = heap;
    print_bar(heap, heap_max, 32, COLOR_LIGHT_GREEN, COLOR_DARK_GRAY);
    print_str("  ");
    print_uint(heap / 1024);
    print_str(" KB allocated\n");
    print_set_color(sh_fg(), sh_bg());
}

static void cmd_date(void) {
    char tbuf[8], dbuf[12];
    rtc_fmt_time(tbuf); rtc_fmt_date(dbuf);
    print_set_color(sh_fg(), sh_bg());
    print_str("  "); print_set_color(sh_acc(), sh_bg()); print_str(dbuf);
    print_set_color(sh_fg(), sh_bg()); print_str("  ");
    print_set_color(sh_acc(), sh_bg()); print_str(tbuf);
    print_set_color(COLOR_DARK_GRAY, sh_bg());
    int8_t off = rtc_tz_offset();
    char tz[12] = "  UTC";
    if      (off > 0) { char n[4]; str_fmtuint(n, 4, (uint64_t)off);  str_cat(tz, "+", sizeof(tz)); str_cat(tz, n, sizeof(tz)); }
    else if (off < 0) { char n[4]; str_fmtuint(n, 4, (uint64_t)-off); str_cat(tz, "-", sizeof(tz)); str_cat(tz, n, sizeof(tz)); }
    print_str(tz); print_newline();
    print_set_color(sh_fg(), sh_bg());
}

static void cmd_whoami(void) {
    print_set_color(sh_acc(), sh_bg());
    print_str("  "); print_str(auth_username); print_newline();
    print_set_color(sh_fg(), sh_bg());
}

/* ── sysinfo (neofetch-style) ── */
static void cmd_sysinfo(void) {
    Color acc = sh_acc(), fg = sh_fg(), bg = sh_bg();
    Color dg  = COLOR_DARK_GRAY;

    uint8_t bx = 1, bw = 78, bh = 12;
    /* place box one line below current cursor */
    print_newline();
    uint8_t by = print_row();
    /* make sure it fits */
    if ((int)by + bh > VGA_ROWS - 1) by = (uint8_t)(VGA_ROWS - 1 - bh);

    print_box(bx, by, bw, bh, acc, bg);

    /* title bar */
    print_str_at((uint8_t)(bx + 2), by,
                 " TorrentOS  \xFA  v0.1-dev  \xFA  x86_64 ",
                 acc, bg);

    /* horizontal rule after title */
    for (int i = 1; i < bw - 1; i++)
        print_at((uint8_t)(bx + i), (uint8_t)(by + 1), '\xCD', acc, bg);

    /* ── left column ── */
    uint8_t lx = (uint8_t)(bx + 3);
    uint8_t r  = (uint8_t)(by + 2);

    /* helper: label + value */
    #define SINFO_ROW(label, val_c, val) do { \
        print_str_at(lx, r, (label), dg, bg); \
        print_str_at((uint8_t)(lx + 9), r, (val), (val_c), bg); \
        r++; \
    } while(0)

    char ubuf[AUTH_USER_MAX + 12] = "";
    str_cat(ubuf, auth_username, sizeof(ubuf));
    str_cat(ubuf, "@torrentos", sizeof(ubuf));
    SINFO_ROW("User     ", acc, ubuf);

    char dbuf[12]; rtc_fmt_date(dbuf);
    SINFO_ROW("Date     ", fg, dbuf);

    char tbuf[8]; rtc_fmt_time(tbuf);
    SINFO_ROW("Time     ", fg, tbuf);

    uint64_t tk = timer_ticks();
    uint64_t us = tk / 1000, um = us / 60, uh = um / 60;
    us %= 60; um %= 60;
    char upbuf[24] = "";
    char tmp[8];
    str_fmtuint(tmp, sizeof(tmp), uh); str_cat(upbuf, tmp, sizeof(upbuf));
    str_cat(upbuf, "h ", sizeof(upbuf));
    str_fmtuint(tmp, sizeof(tmp), um); str_cat(upbuf, tmp, sizeof(upbuf));
    str_cat(upbuf, "m ", sizeof(upbuf));
    str_fmtuint(tmp, sizeof(tmp), us); str_cat(upbuf, tmp, sizeof(upbuf));
    str_cat(upbuf, "s", sizeof(upbuf));
    SINFO_ROW("Uptime   ", fg, upbuf);

    /* ── right column ── */
    uint8_t rx  = (uint8_t)(bx + 41);
    uint8_t rr  = (uint8_t)(by + 2);

    #define SINFO_RROW(label, val_c, val) do { \
        print_str_at(rx, rr, (label), dg, bg); \
        print_str_at((uint8_t)(rx + 9), rr, (val), (val_c), bg); \
        rr++; \
    } while(0)

    SINFO_RROW("Theme    ", fg, auth_dark_mode ? "Dark Mode" : "Light Mode");

    char batbuf[16]; battery_fmt(batbuf);
    SINFO_RROW("Power    ", fg, batbuf);

    SINFO_RROW("WiFi     ", pci_has_wifi() ? acc : dg,
               pci_has_wifi() ? pci_wifi_name() : "Not detected");

    SINFO_RROW("BT       ", pci_has_bluetooth() ? acc : dg,
               pci_has_bluetooth() ? pci_bt_name() : "Not detected");

    #undef SINFO_ROW
    #undef SINFO_RROW

    /* ── memory bar spanning full width ── */
    uint8_t br = (uint8_t)(by + 7);
    /* thin separator */
    for (int i = 1; i < bw - 1; i++)
        print_at((uint8_t)(bx + i), br, '\xC4', dg, bg);

    br++;
    size_t total = pmm_total_frames() * 4096;
    size_t free_ = pmm_free_frames()  * 4096;
    size_t used  = total - free_;
    int    pct   = (total > 0) ? (int)((uint64_t)used * 100 / total) : 0;
    int    bar_w = 44;
    int    filled = (total > 0) ? (int)((uint64_t)used * (uint64_t)bar_w / total) : 0;

    print_str_at(lx, br, "RAM  ", dg, bg);
    uint8_t barcol = (uint8_t)(lx + 5);
    print_at(barcol++, br, '[', fg, bg);
    for (int i = 0; i < bar_w; i++) {
        Color c = (i < filled) ? acc : dg;
        print_at(barcol++, br, (i < filled) ? '\xDB' : '\xB0', c, bg);
    }
    print_at(barcol++, br, ']', fg, bg);
    print_str_at(barcol, br, "  ", fg, bg);
    /* numeric label */
    char rambuf[32] = "";
    str_fmtuint(tmp, sizeof(tmp), (uint64_t)(used / 1024 / 1024));
    str_cat(rambuf, tmp, sizeof(rambuf)); str_cat(rambuf, " MB / ", sizeof(rambuf));
    str_fmtuint(tmp, sizeof(tmp), (uint64_t)(total / 1024 / 1024));
    str_cat(rambuf, tmp, sizeof(rambuf)); str_cat(rambuf, " MB  ", sizeof(rambuf));
    str_fmtuint(tmp, sizeof(tmp), (uint64_t)pct);
    str_cat(rambuf, tmp, sizeof(rambuf)); str_cat(rambuf, "%", sizeof(rambuf));
    print_str_at((uint8_t)(barcol + 2), br, rambuf, fg, bg);

    /* move cursor below the box */
    print_move(0, (uint8_t)(by + bh));
    print_set_color(fg, bg);
}

static void cmd_wifi(void) {
    print_set_color(sh_fg(), sh_bg());
    if (pci_has_wifi()) {
        print_str("  WiFi   ");
        print_set_color(COLOR_LIGHT_GREEN, sh_bg());
        print_str(pci_wifi_name());
        print_set_color(COLOR_DARK_GRAY, sh_bg());
        print_str("  (detected \xFA no driver loaded)\n");
    } else {
        print_set_color(COLOR_DARK_GRAY, sh_bg());
        print_str("  WiFi   No adapter detected on PCI bus.\n");
        print_str("         (Expected in QEMU \xFA physical hardware only)\n");
    }
    print_set_color(sh_fg(), sh_bg());
}

static void cmd_bluetooth(void) {
    print_set_color(sh_fg(), sh_bg());
    if (pci_has_bluetooth()) {
        print_str("  BT     ");
        print_set_color(COLOR_LIGHT_GREEN, sh_bg());
        print_str(pci_bt_name());
        print_set_color(COLOR_DARK_GRAY, sh_bg());
        print_str("  (detected \xFA no driver loaded)\n");
    } else {
        print_set_color(COLOR_DARK_GRAY, sh_bg());
        print_str("  BT     No adapter detected on PCI bus.\n");
        print_str("         (Expected in QEMU \xFA physical hardware only)\n");
    }
    print_set_color(sh_fg(), sh_bg());
}

static void cmd_darkmode(void) {
    auth_dark_mode ^= 1;
    config_commit();   /* auto-save if disk present */
    desktop_redraw_bar();
    /* repaint terminal with new theme */
    print_set_color(sh_fg(), sh_bg());
    print_clear();
    desktop_redraw_bar();
    print_set_color(COLOR_DARK_GRAY, sh_bg());
    print_str("  Theme: ");
    print_set_color(sh_acc(), sh_bg());
    print_str(auth_dark_mode ? "Dark Mode\n" : "Light Mode\n");
    print_set_color(sh_fg(), sh_bg());
}

static void cmd_timezone(const char *arg) {
    if (!arg || !arg[0]) {
        static const struct { const char *name; int8_t off; } zones[] = {
            { "UTC",            0  }, { "London",        0  },
            { "Paris/Berlin",   1  }, { "Athens/Cairo",  2  },
            { "Moscow",         3  }, { "Dubai",         4  },
            { "India",          5  }, { "Dhaka",         6  },
            { "Bangkok",        7  }, { "Beijing/HK",    8  },
            { "Tokyo",          9  }, { "Sydney",        10 },
            { "Auckland",       12 }, { "New York",      -5 },
            { "Chicago",        -6 }, { "Denver",        -7 },
            { "Los Angeles",    -8 }, { "Anchorage",     -9 },
            { "Honolulu",       -10 },
            { (void*)0, 0 },
        };
        int8_t cur = rtc_tz_offset();
        print_set_color(sh_acc(), sh_bg());
        print_str("  Current offset: UTC");
        if (cur >= 0) { print_str("+"); print_int(cur); }
        else            print_int(cur);
        print_newline();
        print_set_color(COLOR_DARK_GRAY, sh_bg());
        for (int i = 0; zones[i].name; i++) {
            print_str("  ");
            print_str(zones[i].name);
            /* pad to column */
            int l = (int)str_len(zones[i].name);
            for (int j = l; j < 16; j++) print_char(' ');
            print_str("UTC");
            if (zones[i].off >= 0) { print_str("+"); print_int(zones[i].off); }
            else                     print_int(zones[i].off);
            print_newline();
        }
        print_set_color(COLOR_DARK_GRAY, sh_bg());
        print_str("  Usage: timezone +8   or   timezone -5\n");
        print_set_color(sh_fg(), sh_bg());
        return;
    }
    int v = str_toint(arg);
    if (v < -12 || v > 14) {
        print_set_color(COLOR_LIGHT_RED, sh_bg());
        print_str("  Error: offset must be in range -12..+14\n");
    } else {
        rtc_set_tz_offset((int8_t)v);
        config_commit();
        print_set_color(sh_fg(), sh_bg());
        print_str("  Timezone set to UTC");
        if (v >= 0) { print_str("+"); print_int(v); }
        else          print_int(v);
        print_newline();
        desktop_redraw_bar();
    }
    print_set_color(sh_fg(), sh_bg());
}

static void cmd_battery(void) {
    char buf[24]; battery_fmt(buf);
    print_set_color(sh_fg(), sh_bg());
    print_str("  Power   "); print_str(buf); print_newline();
    print_str("  Saver   ");
    print_set_color(battery_saver_enabled() ? COLOR_LIGHT_GREEN : COLOR_DARK_GRAY, sh_bg());
    print_str(battery_saver_enabled() ? "ON" : "OFF"); print_newline();
    print_set_color(COLOR_DARK_GRAY, sh_bg());
    print_str("  Use 'settings' to toggle battery saver.\n");
    print_set_color(sh_fg(), sh_bg());
}

static void cmd_pwd(void) {
    print_set_color(sh_acc(), sh_bg());
    print_str("  "); print_str(cwd); print_newline();
    print_set_color(sh_fg(), sh_bg());
}

static void cmd_ls(const char *arg) {
    const char *dir = (arg && arg[0]) ? arg : cwd;
    const char *entries[32];
    int n = vfs_list(dir, entries, 32);
    if (n == 0) {
        print_set_color(COLOR_DARK_GRAY, sh_bg());
        print_str("  (empty or not found: "); print_str(dir); print_str(")\n");
        print_set_color(sh_fg(), sh_bg());
        return;
    }
    print_set_color(sh_acc(), sh_bg());
    for (int i = 0; i < n; i++) {
        /* show just the filename part */
        const char *p = entries[i];
        /* find last '/' */
        const char *name = p;
        for (const char *q = p; *q; q++)
            if (*q == '/') name = q + 1;
        print_str("  "); print_str(name); print_newline();
    }
    print_set_color(sh_fg(), sh_bg());
}

static void cmd_cat(const char *path) {
    if (!path || !path[0]) {
        print_set_color(COLOR_LIGHT_RED, sh_bg());
        print_str("  Usage: cat <path>\n");
        print_set_color(sh_fg(), sh_bg());
        return;
    }
    /* if no leading /, prepend cwd */
    char full[128];
    if (path[0] != '/') {
        str_cpy(full, cwd, sizeof(full));
        if (full[str_len(full)-1] != '/') str_cat(full, "/", sizeof(full));
        str_cat(full, path, sizeof(full));
        path = full;
    }
    const char *content = vfs_read(path);
    if (!content) {
        print_set_color(COLOR_LIGHT_RED, sh_bg());
        print_str("  cat: "); print_str(path); print_str(": no such file\n");
        print_set_color(sh_fg(), sh_bg());
        return;
    }
    print_set_color(sh_fg(), sh_bg());
    print_str(content);
    /* ensure trailing newline */
    size_t l = str_len(content);
    if (l > 0 && content[l-1] != '\n') print_newline();
}

static void cmd_save(void) {
    if (!ata_present()) {
        print_set_color(COLOR_LIGHT_RED, sh_bg());
        print_str("  No ATA drive — cannot save persistently.\n");
        print_set_color(COLOR_DARK_GRAY, sh_bg());
        print_str("  Add a disk to QEMU:  -drive file=torrentos.img,format=raw\n");
        print_set_color(sh_fg(), sh_bg());
        return;
    }
    if (config_commit()) {
        print_set_color(COLOR_LIGHT_GREEN, sh_bg());
        print_str("  Config saved to disk.\n");
    } else {
        print_set_color(COLOR_LIGHT_RED, sh_bg());
        print_str("  Save failed.\n");
    }
    print_set_color(sh_fg(), sh_bg());
}

static void cmd_halt(void) {
    print_set_color(sh_fg(), sh_bg());
    print_str("  Powering off...\n");
    timer_sleep(300);
    outw(0x604,  0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    cpu_cli();
    for (;;) cpu_halt();
}

static void cmd_reboot(void) {
    print_set_color(sh_fg(), sh_bg());
    print_str("  Rebooting...\n");
    timer_sleep(300);
    while (inb(0x64) & 0x02);
    outb(0x64, 0xFE);
    for (;;) cpu_halt();
}

static void cmd_echo(const char *line) {
    const char *p = str_skip(line, "echo");
    if (!p) p = "";
    print_set_color(sh_fg(), sh_bg());
    print_str("  "); print_str(p); print_newline();
}

/* ── dispatch ── */
static void dispatch(const char *line) {
    while (*line == ' ') line++;
    if (!line[0]) return;

    if      (str_eq(line, "help"))           cmd_help();
    else if (str_eq(line, "clear"))          { print_set_color(sh_fg(), sh_bg()); print_clear(); desktop_redraw_bar(); }
    else if (str_eq(line, "sysinfo"))        cmd_sysinfo();
    else if (str_eq(line, "ver"))            cmd_ver();
    else if (str_eq(line, "uptime"))         cmd_uptime();
    else if (str_eq(line, "mem"))            cmd_mem();
    else if (str_eq(line, "date"))           cmd_date();
    else if (str_eq(line, "whoami"))         cmd_whoami();
    else if (str_eq(line, "history"))        cmd_history();
    else if (str_eq(line, "wifi"))           cmd_wifi();
    else if (str_eq(line, "bluetooth"))      cmd_bluetooth();
    else if (str_eq(line, "darkmode"))       cmd_darkmode();
    else if (str_eq(line, "battery"))        cmd_battery();
    else if (str_eq(line, "browser"))        browser_run();
    else if (str_eq(line, "apps"))           appstore_run();
    else if (str_eq(line, "settings"))       appstore_run();
    else if (str_eq(line, "pwd"))            cmd_pwd();
    else if (str_eq(line, "ls"))             cmd_ls((void*)0);
    else if (str_eq(line, "save"))           cmd_save();
    else if (str_eq(line, "halt"))           cmd_halt();
    else if (str_eq(line, "reboot"))         cmd_reboot();
    else if (str_eq(line, "echo"))           print_newline();
    else if (str_pfx(line, "echo "))         cmd_echo(line);
    else if (str_pfx(line, "ls "))           cmd_ls(str_skip(line, "ls"));
    else if (str_pfx(line, "cat "))          cmd_cat(str_skip(line, "cat"));
    else if (str_pfx(line, "timezone"))      cmd_timezone(str_skip(line, "timezone"));
    else {
        print_set_color(COLOR_LIGHT_RED, sh_bg());
        print_str("  command not found: "); print_str(line); print_newline();
        print_set_color(COLOR_DARK_GRAY, sh_bg());
        print_str("  type 'help' or press Tab to see commands\n");
        print_set_color(sh_fg(), sh_bg());
    }
}

/* ── public entry ── */
void shell_run(void) {
    char line[LINE_MAX];

    for (;;) {
        print_set_color(sh_fg(), sh_bg());
        print_prompt();
        readline(line, LINE_MAX);
        hist_push(line);
        dispatch(line);
    }
}
