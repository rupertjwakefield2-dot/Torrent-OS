#include "install.h"
#include "print.h"
#include "keyboard.h"
#include "auth.h"
#include "rtc.h"
#include "str.h"
#include "timer.h"

/* ── colour helpers (dark-only during install) ── */
#define INS_BG   COLOR_BLACK
#define INS_FG   COLOR_LIGHT_GRAY
#define INS_ACC  COLOR_LIGHT_CYAN
#define INS_WARN COLOR_LIGHT_RED
#define INS_OK   COLOR_LIGHT_GREEN

/* ── timezone table ── */
typedef struct { const char *name; int8_t offset; } TZ;
static const TZ timezones[] = {
    { "UTC            +0",   0  },
    { "London         +0",   0  },
    { "Paris/Berlin   +1",   1  },
    { "Athens/Cairo   +2",   2  },
    { "Moscow         +3",   3  },
    { "Dubai          +4",   4  },
    { "India          +5",   5  },
    { "Dhaka          +6",   6  },
    { "Bangkok        +7",   7  },
    { "Beijing/HK     +8",   8  },
    { "Tokyo          +9",   9  },
    { "Sydney         +10", 10  },
    { "Auckland       +12", 12  },
    { "New York       -5",  -5  },
    { "Chicago        -6",  -6  },
    { "Denver         -7",  -7  },
    { "Los Angeles    -8",  -8  },
    { "Anchorage      -9",  -9  },
    { "Honolulu       -10", -10 },
};
#define NTZ ((int)(sizeof(timezones)/sizeof(timezones[0])))

/* ── generic input line ── */
static int read_line(char *out, size_t max,
                     uint8_t col, uint8_t row, uint8_t field_w, int mask) {
    char buf[128];
    int  len = 0, pos = 0;

    for (;;) {
        /* redraw field */
        for (int i = 0; i < field_w; i++)
            print_at((uint8_t)(col + i), row, '_', COLOR_DARK_GRAY, INS_BG);
        for (int i = 0; i < len && i < field_w; i++)
            print_at((uint8_t)(col + i), row,
                     mask ? '\xF9' : buf[i],   /* · for password */
                     INS_FG, INS_BG);
        print_move((uint8_t)(col + pos), row);

        int k = keyboard_getkey();
        if (k == '\n') {
            if (len == 0) continue;
            buf[len] = '\0';
            str_cpy(out, buf, max);
            return len;
        }
        if (k == KEY_ESC) { out[0] = '\0'; return 0; }
        if ((k == '\b' || k == 0x7F) && pos > 0) {
            for (int i = pos - 1; i < len - 1; i++) buf[i] = buf[i + 1];
            len--; pos--;
        }
        if (k == KEY_LEFT  && pos > 0)   pos--;
        if (k == KEY_RIGHT && pos < len) pos++;
        if (k == KEY_HOME) pos = 0;
        if (k == KEY_END)  pos = len;
        if (k >= 0x20 && k < 0x7F && len < (int)(max - 1) && pos < field_w) {
            for (int i = len; i > pos; i--) buf[i] = buf[i - 1];
            buf[pos++] = (char)k;
            len++;
        }
    }
}

/* ── step helpers ── */
static void install_header(const char *step_title, int step, int total) {
    print_set_color(INS_FG, INS_BG);
    print_clear();

    /* top bar */
    print_statusbar("  TorrentOS Setup", "", COLOR_WHITE, COLOR_DARK_GRAY);

    /* logo */
    print_center_str(3, "TorrentOS", INS_ACC, INS_BG);

    /* step indicator */
    char ind[48] = "Step ";
    char sn[4], tn[4];
    str_fmtuint(sn, sizeof(sn), (uint64_t)step);
    str_fmtuint(tn, sizeof(tn), (uint64_t)total);
    str_cat(ind, sn, sizeof(ind));
    str_cat(ind, " of ", sizeof(ind));
    str_cat(ind, tn,  sizeof(ind));
    print_center_str(4, ind, COLOR_DARK_GRAY, INS_BG);

    /* step title */
    print_center_str(6, step_title, INS_FG, INS_BG);
    print_hline(10, 7, 60, COLOR_DARK_GRAY, INS_BG);
}

/* ── Step 1: Welcome ── */
static void step_welcome(void) {
    install_header("Welcome", 1, 5);

    print_center_str( 9, "Thank you for installing TorrentOS.",        INS_FG,  INS_BG);
    print_center_str(10, "This wizard will set up your system.",        INS_FG,  INS_BG);
    print_center_str(12, "You will be asked to:",                        INS_ACC, INS_BG);
    print_center_str(13, "  * Choose a username",                        INS_FG,  INS_BG);
    print_center_str(14, "  * Select your timezone",                     INS_FG,  INS_BG);
    print_center_str(15, "  * Set a password (4-100 characters)",        INS_FG,  INS_BG);
    print_center_str(18, "Press Enter to continue...",                   INS_OK,  INS_BG);

    print_move(0, 23);
    while (keyboard_getkey() != '\n');
}

/* ── Step 2: Username ── */
static void step_username(void) {
    for (;;) {
        install_header("Choose a Username", 2, 5);

        print_str_at(14, 10, "Your username will appear on the login screen.", INS_FG, INS_BG);
        print_str_at(14, 12, "Username: ", INS_FG, INS_BG);

        char uname[AUTH_USER_MAX];
        int  len = read_line(uname, AUTH_USER_MAX, 24, 12, 30, 0);
        if (len < 1) continue;

        /* validate: letters/digits/underscore only */
        int ok = 1;
        for (int i = 0; i < len; i++) {
            char c = uname[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_')) { ok = 0; break; }
        }
        if (!ok) {
            print_str_at(14, 15, "Invalid: letters, digits and _ only.", INS_WARN, INS_BG);
            timer_sleep(1500);
            continue;
        }

        str_cpy(auth_username, uname, AUTH_USER_MAX);
        return;
    }
}

/* ── Step 3: Timezone ── */
static void step_timezone(void) {
    int sel = 0;
    int vis = 12;   /* visible rows */

    for (;;) {
        install_header("Select Timezone", 3, 5);
        print_str_at(14, 9, "Use Up/Down, then Enter to confirm.", INS_FG, INS_BG);

        int start = sel > vis / 2 ? sel - vis / 2 : 0;
        if (start + vis > NTZ) start = NTZ - vis;
        if (start < 0) start = 0;

        for (int i = 0; i < vis; i++) {
            int  idx = start + i;
            if (idx >= NTZ) break;
            uint8_t row = (uint8_t)(11 + i);
            Color   fg  = (idx == sel) ? COLOR_WHITE      : INS_FG;
            Color   bg  = (idx == sel) ? COLOR_DARK_GRAY  : INS_BG;
            char    line[48] = "  ";
            if (idx == sel) str_cat(line, "\x10 ", 48);
            else            str_cat(line, "  ", 48);
            str_cat(line, timezones[idx].name, 48);
            print_clear_row(row, fg, bg);
            print_str_at(16, row, line, fg, bg);
        }

        print_move(0, 23);
        int k = keyboard_getkey();
        if (k == KEY_UP   && sel > 0)       sel--;
        if (k == KEY_DOWN && sel < NTZ - 1) sel++;
        if (k == '\n') {
            rtc_set_tz_offset(timezones[sel].offset);
            return;
        }
    }
}

/* ── Step 4: Password ── */
static void step_password(void) {
    for (;;) {
        install_header("Set Your Password", 4, 5);
        print_str_at(14, 9,  "Password must be 4-100 characters.", INS_FG, INS_BG);

        print_str_at(14, 11, "Password:        ", INS_FG, INS_BG);
        print_str_at(14, 13, "Confirm:         ", INS_FG, INS_BG);

        char p1[AUTH_PASS_MAX + 2], p2[AUTH_PASS_MAX + 2];
        read_line(p1, sizeof(p1), 27, 11, 30, 1);
        read_line(p2, sizeof(p2), 27, 13, 30, 1);

        if (auth_set_password(p1, p2)) return;

        /* error */
        print_set_color(INS_WARN, INS_BG);
        size_t l1 = str_len(p1), l2 = str_len(p2);
        if (l1 < AUTH_PASS_MIN || l1 > AUTH_PASS_MAX)
            print_str_at(14, 16, "Password must be 4-100 characters.", INS_WARN, INS_BG);
        else if (l1 != l2 || !str_eq(p1, p2))
            print_str_at(14, 16, "Passwords do not match. Try again.", INS_WARN, INS_BG);
        (void)l2;
        timer_sleep(1500);
    }
}

/* ── Step 5: Done ── */
static void step_done(void) {
    install_header("Setup Complete!", 5, 5);

    char greet[80] = "  Welcome, ";
    str_cat(greet, auth_username, sizeof(greet));
    str_cat(greet, "!", sizeof(greet));
    print_center_str(10, greet,                            INS_ACC, INS_BG);
    print_center_str(12, "Your system is ready.",           INS_FG,  INS_BG);
    print_center_str(13, "You will now be taken to the",    INS_FG,  INS_BG);
    print_center_str(14, "login screen.",                   INS_FG,  INS_BG);
    print_center_str(17, "Press Enter to continue...",      INS_OK,  INS_BG);

    print_move(0, 23);
    while (keyboard_getkey() != '\n');
}

/* ── public entry ── */
void install_run(void) {
    step_welcome();
    step_username();
    step_timezone();
    step_password();
    step_done();
}
