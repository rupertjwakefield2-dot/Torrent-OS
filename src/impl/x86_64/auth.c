#include "auth.h"
#include "print.h"
#include "keyboard.h"
#include "str.h"
#include "timer.h"
#include "io.h"

char auth_username[AUTH_USER_MAX] = "";
int  auth_dark_mode               = 1;  /* dark by default */

static char password[AUTH_PASS_MAX + 1] = "";

/* ── color helpers ── */
static Color theme_bg(void)   { return auth_dark_mode ? COLOR_BLACK      : COLOR_LIGHT_GRAY; }
static Color theme_fg(void)   { return auth_dark_mode ? COLOR_LIGHT_GRAY : COLOR_BLACK; }
static Color theme_acc(void)  { return auth_dark_mode ? COLOR_LIGHT_CYAN : COLOR_CYAN; }
static Color theme_sbar_bg(void) { return auth_dark_mode ? COLOR_DARK_GRAY : COLOR_WHITE; }
static Color theme_sbar_fg(void) { return auth_dark_mode ? COLOR_WHITE     : COLOR_DARK_GRAY; }

/* expose password buffer read-only for config persistence */
const char *auth_password_ptr(void) { return password; }

void auth_init(void) {
    /* nothing — defaults already set */
}

int auth_set_password(const char *pass, const char *confirm) {
    size_t len = str_len(pass);
    if (len < AUTH_PASS_MIN || len > AUTH_PASS_MAX) return 0;
    if (!str_eq(pass, confirm))                     return 0;
    str_cpy(password, pass, sizeof(password));
    return 1;
}

int auth_check(const char *attempt) {
    return str_eq(attempt, password);
}

/* ── masked password input ── */
static int read_password(char *out, size_t max, uint8_t field_col, uint8_t field_row,
                          uint8_t field_w) {
    char buf[AUTH_PASS_MAX + 1];
    int  len = 0;
    Color fg = theme_fg(), bg = theme_bg();

    for (;;) {
        /* redraw field */
        for (int i = 0; i < field_w; i++)
            print_at((uint8_t)(field_col + i), field_row, ' ', fg, bg);
        for (int i = 0; i < len; i++)
            print_at((uint8_t)(field_col + i), field_row, '\xF9', fg, bg); /* · */
        print_move((uint8_t)(field_col + len), field_row);

        int k = keyboard_getkey();
        if (k == '\n') {
            buf[len] = '\0';
            str_cpy(out, buf, max);
            return len;
        }
        if ((k == '\b' || k == 0x7F) && len > 0) { len--; continue; }
        if (k == KEY_ESC) { out[0] = '\0'; return 0; }
        if (k >= 0x20 && k < 0x7F && len < (int)(max - 1)) buf[len++] = (char)k;
    }
}

/* ── login screen ── */
void auth_login(void) {
    int tries = 0;

    for (;;) {
        /* clear terminal area */
        print_set_color(theme_fg(), theme_bg());
        print_clear();

        /* status bar */
        print_statusbar("  TorrentOS", "", theme_sbar_fg(), theme_sbar_bg());

        /* centred login box  (width=44, height=11) */
        uint8_t bx = 18, by = 6, bw = 44, bh = 11;
        print_box(bx, by, bw, bh, theme_acc(), theme_bg());

        /* title */
        print_center_str((uint8_t)(by + 1), "TorrentOS", theme_acc(), theme_bg());

        /* divider */
        print_hline((uint8_t)(bx + 1), (uint8_t)(by + 2), (uint8_t)(bw - 2),
                    theme_acc(), theme_bg());

        /* greeting */
        char greet[80] = "  Welcome back, ";
        str_cat(greet, auth_username, sizeof(greet));
        print_str_at((uint8_t)(bx + 2), (uint8_t)(by + 4), greet, theme_fg(), theme_bg());

        /* password label + field */
        print_str_at((uint8_t)(bx + 2), (uint8_t)(by + 6), "Password: ",
                     theme_fg(), theme_bg());
        uint8_t fc = (uint8_t)(bx + 12);
        uint8_t fr = (uint8_t)(by + 6);
        uint8_t fw = (uint8_t)(bw - 14);

        /* attempts remaining */
        if (tries > 0) {
            char att[40] = "  Attempts left: ";
            char num[4];
            str_fmtuint(num, sizeof(num), (uint64_t)(AUTH_MAX_TRIES - tries));
            str_cat(att, num, sizeof(att));
            print_str_at((uint8_t)(bx + 2), (uint8_t)(by + 8), att,
                         COLOR_LIGHT_RED, theme_bg());
        }

        /* prompt */
        print_str_at((uint8_t)(bx + 2), (uint8_t)(by + 9), "Press Enter to login",
                     COLOR_DARK_GRAY, theme_bg());

        /* read password */
        char attempt[AUTH_PASS_MAX + 1];
        read_password(attempt, sizeof(attempt), fc, fr, fw);

        if (auth_check(attempt)) return;   /* success */

        tries++;
        if (tries >= AUTH_MAX_TRIES) {
            print_set_color(COLOR_LIGHT_RED, COLOR_BLACK);
            print_clear();
            print_center_str(12, "Too many failed attempts. System halted.", COLOR_LIGHT_RED, COLOR_BLACK);
            timer_sleep(3000);
            cpu_cli();
            for (;;) cpu_halt();
        }
    }
}
