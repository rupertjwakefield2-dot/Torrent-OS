#include "browser.h"
#include "print.h"
#include "keyboard.h"
#include "auth.h"
#include "rtc.h"
#include "battery.h"
#include "pci.h"
#include "str.h"
#include "desktop.h"

#define URL_MAX    80
#define CONTENT_T  2    /* first content row */
#define CONTENT_B  20   /* last content row  */
#define LINE_W     78

typedef struct { const char *text; Color fg; } PageLine;

/* ── static pages ── */

static const PageLine home_page[] = {
    { "",                                                  COLOR_LIGHT_GRAY },
    { "  TorrentOS Browser  v0.1",                        COLOR_LIGHT_CYAN },
    { "",                                                  COLOR_LIGHT_GRAY },
    { "  Bookmarks",                                      COLOR_YELLOW     },
    { "  ──────────────────────────────────────────",     COLOR_DARK_GRAY  },
    { "  about:home    -  This page",                     COLOR_LIGHT_GRAY },
    { "  about:help    -  Keyboard shortcuts",            COLOR_LIGHT_GRAY },
    { "  about:system  -  Hardware and OS information",   COLOR_LIGHT_GRAY },
    { "",                                                  COLOR_LIGHT_GRAY },
    { "  Type a URL and press Enter to navigate.",        COLOR_DARK_GRAY  },
    { "  Press Q or ESC to exit the browser.",            COLOR_DARK_GRAY  },
    { (void*)0, 0 },
};

static const PageLine help_page[] = {
    { "",                                                  COLOR_LIGHT_GRAY },
    { "  Keyboard Shortcuts",                             COLOR_LIGHT_CYAN },
    { "  ──────────────────────────────────────────",     COLOR_DARK_GRAY  },
    { "  Tab / /       Focus address bar",                COLOR_LIGHT_GRAY },
    { "  Enter         Navigate to URL",                  COLOR_LIGHT_GRAY },
    { "  Up / Down     Scroll page content",              COLOR_LIGHT_GRAY },
    { "  Backspace     Go back to about:home",            COLOR_LIGHT_GRAY },
    { "  Q / ESC       Quit browser",                     COLOR_LIGHT_GRAY },
    { "",                                                  COLOR_LIGHT_GRAY },
    { "  Available pages:",                                COLOR_YELLOW     },
    { "  about:home   about:help   about:system",         COLOR_LIGHT_GRAY },
    { (void*)0, 0 },
};

/* system page built at runtime */
#define SYS_MAX 20
static PageLine sys_page[SYS_MAX];
static char     sys_buf[SYS_MAX][LINE_W];
static int      sys_count = 0;

static void sys_add(const char *text, Color c) {
    if (sys_count >= SYS_MAX - 1) return;
    str_cpy(sys_buf[sys_count], text, LINE_W);
    sys_page[sys_count].text = sys_buf[sys_count];
    sys_page[sys_count].fg   = c;
    sys_count++;
}

static void build_system_page(void) {
    sys_count = 0;
    sys_add("",                                    COLOR_LIGHT_GRAY);
    sys_add("  System Information",               COLOR_LIGHT_CYAN);
    sys_add("  ────────────────────────────────", COLOR_DARK_GRAY);

    char buf[LINE_W];
    str_cpy(buf, "  OS:        TorrentOS v0.1-dev (x86_64)", LINE_W);
    sys_add(buf, COLOR_LIGHT_GRAY);

    str_cpy(buf, "  User:      ", LINE_W); str_cat(buf, auth_username, LINE_W);
    sys_add(buf, COLOR_LIGHT_GRAY);

    char tbuf[8]; rtc_fmt_time(tbuf);
    char dbuf[12]; rtc_fmt_date(dbuf);
    str_cpy(buf, "  Date/Time: ", LINE_W);
    str_cat(buf, dbuf, LINE_W); str_cat(buf, "  ", LINE_W); str_cat(buf, tbuf, LINE_W);
    sys_add(buf, COLOR_LIGHT_GRAY);

    str_cpy(buf, "  Theme:     ", LINE_W);
    str_cat(buf, auth_dark_mode ? "Dark Mode" : "Light Mode", LINE_W);
    sys_add(buf, COLOR_LIGHT_GRAY);

    sys_add("  ────────────────────────────────", COLOR_DARK_GRAY);
    sys_add("  Hardware",                         COLOR_YELLOW);
    sys_add("  ────────────────────────────────", COLOR_DARK_GRAY);

    str_cpy(buf, "  WiFi:      ", LINE_W);
    str_cat(buf, pci_has_wifi() ? pci_wifi_name() : "Not detected", LINE_W);
    sys_add(buf, COLOR_LIGHT_GRAY);

    str_cpy(buf, "  Bluetooth: ", LINE_W);
    str_cat(buf, pci_has_bluetooth() ? pci_bt_name() : "Not detected", LINE_W);
    sys_add(buf, COLOR_LIGHT_GRAY);

    char batbuf[16]; battery_fmt(batbuf);
    str_cpy(buf, "  Power:     ", LINE_W); str_cat(buf, batbuf, LINE_W);
    sys_add(buf, COLOR_LIGHT_GRAY);

    sys_add("", COLOR_LIGHT_GRAY);
    sys_page[sys_count].text = (void*)0;
}

/* ── colour helpers ── */
static Color br_bg(void) { return auth_dark_mode ? COLOR_BLACK      : COLOR_LIGHT_GRAY; }
static Color br_fg(void) { return auth_dark_mode ? COLOR_LIGHT_GRAY : COLOR_BLACK;      }

/* ── chrome ── */
static void draw_chrome(const char *url) {
    desktop_redraw_bar();
    print_clear_row(1, COLOR_DARK_GRAY, COLOR_DARK_GRAY);
    print_str_at(0, 1, " URL: ", COLOR_YELLOW, COLOR_DARK_GRAY);
    print_str_at(6, 1, url,      COLOR_WHITE,  COLOR_DARK_GRAY);
    for (int r = CONTENT_T; r <= CONTENT_B + 1; r++)
        print_clear_row((uint8_t)r, br_fg(), br_bg());
    print_str_at(0, 22,
        "  [Tab] address bar   [Up/Dn] scroll   [Bksp] home   [Q] quit",
        COLOR_DARK_GRAY, br_bg());
}

static void render_page(const PageLine *page, int scroll_off) {
    int row = CONTENT_T;
    for (int i = scroll_off; page[i].text && row <= CONTENT_B; i++, row++)
        print_str_at(0, (uint8_t)row, page[i].text, page[i].fg, br_bg());
}

/* ── address bar readline ── */
static void read_url(char *out, size_t max) {
    char buf[URL_MAX];
    str_cpy(buf, out, URL_MAX);
    int len = (int)str_len(buf), pos = len;

    for (;;) {
        print_clear_row(1, COLOR_DARK_GRAY, COLOR_DARK_GRAY);
        print_str_at(0, 1, " URL: ", COLOR_YELLOW, COLOR_DARK_GRAY);
        uint8_t x = 6;
        for (int i = 0; i < len && x < VGA_COLS - 1; i++, x++)
            print_at(x, 1, buf[i], COLOR_WHITE, COLOR_DARK_GRAY);
        print_move((uint8_t)(6 + pos), 1);

        int k = keyboard_getkey();
        if (k == '\n')  { buf[len] = '\0'; str_cpy(out, buf, max); return; }
        if (k == KEY_ESC) return;
        if ((k == '\b' || k == 0x7F) && pos > 0) {
            for (int i = pos - 1; i < len - 1; i++) buf[i] = buf[i + 1];
            len--; pos--;
        }
        if (k == KEY_LEFT  && pos > 0)   pos--;
        if (k == KEY_RIGHT && pos < len) pos++;
        if (k == KEY_HOME) pos = 0;
        if (k == KEY_END)  pos = len;
        if (k >= 0x20 && k < 0x7F && len < (int)max - 1) {
            for (int i = len; i > pos; i--) buf[i] = buf[i - 1];
            buf[pos++] = (char)k;
            len++;
        }
    }
}

/* ── count page lines ── */
static int page_len(const PageLine *p) {
    int n = 0; while (p[n].text) n++; return n;
}

/* ── public entry point ── */
void browser_run(void) {
    char            url[URL_MAX] = "about:home";
    const PageLine *cur          = home_page;
    int             scroll       = 0;
    int             url_mode     = 0;

    print_set_color(br_fg(), br_bg());
    print_clear();
    draw_chrome(url);
    render_page(cur, scroll);

    for (;;) {
        if (url_mode) {
            read_url(url, URL_MAX);
            url_mode = 0;
            scroll   = 0;
            if      (str_eq(url, "about:home"))   cur = home_page;
            else if (str_eq(url, "about:help"))   cur = help_page;
            else if (str_eq(url, "about:system")) { build_system_page(); cur = sys_page; }
            else {
                static PageLine nf[4];
                static char nf0[80], nf1[80];
                str_cpy(nf0, "  Page not found: ", 80); str_cat(nf0, url, 80);
                str_cpy(nf1, "  Only about: pages are available in this build.", 80);
                nf[0] = (PageLine){ "",   br_fg()          };
                nf[1] = (PageLine){ nf0,  COLOR_LIGHT_RED  };
                nf[2] = (PageLine){ nf1,  COLOR_DARK_GRAY  };
                nf[3] = (PageLine){ (void*)0, 0 };
                cur = nf;
            }
            draw_chrome(url);
            render_page(cur, scroll);
            continue;
        }

        int k = keyboard_getkey();

        if (k == 'q' || k == 'Q' || k == KEY_ESC) break;
        if (k == '\t' || k == '/')   { url_mode = 1; continue; }
        if (k == '\b') {
            str_cpy(url, "about:home", URL_MAX);
            cur = home_page; scroll = 0;
            draw_chrome(url); render_page(cur, scroll); continue;
        }
        if (k == KEY_UP && scroll > 0) {
            scroll--;
            for (int r = CONTENT_T; r <= CONTENT_B; r++)
                print_clear_row((uint8_t)r, br_fg(), br_bg());
            render_page(cur, scroll);
        }
        if (k == KEY_DOWN) {
            int total = page_len(cur);
            if (scroll + (CONTENT_B - CONTENT_T + 1) < total) {
                scroll++;
                for (int r = CONTENT_T; r <= CONTENT_B; r++)
                    print_clear_row((uint8_t)r, br_fg(), br_bg());
                render_page(cur, scroll);
            }
        }
    }

    print_set_color(br_fg(), br_bg());
    print_clear();
    desktop_redraw_bar();
}
