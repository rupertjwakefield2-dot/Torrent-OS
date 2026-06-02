#include "keyboard.h"
#include "io.h"

#define KB_DATA   0x60
#define KB_STATUS 0x64
#define BUF_SZ    256

static int  buf[BUF_SZ];
static int  buf_r = 0, buf_w = 0;
static bool shift = false, caps = false, ext = false, ctrl = false;

/* ── scancode set 1 ──────────────────────────────────────── */

static const char lower[58] = {
    0,   0,   '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q', 'w','e','r','t','y','u','i','o','p','[',']', '\n',
    0,   'a', 's','d','f','g','h','j','k','l',';','\'','`',
    0,   '\\','z','x','c','v','b','n','m',',','.','/',
    0,   '*', 0,  ' '
};

static const char upper[58] = {
    0,   0,   '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q', 'W','E','R','T','Y','U','I','O','P','{','}', '\n',
    0,   'A', 'S','D','F','G','H','J','K','L',':','"', '~',
    0,   '|', 'Z','X','C','V','B','N','M','<','>','?',
    0,   '*', 0,  ' '
};

static void buf_push(int k) {
    int next = (buf_w + 1) % BUF_SZ;
    if (next != buf_r) { buf[buf_w] = k; buf_w = next; }
}

/* ── IRQ1 handler ────────────────────────────────────────── */

void keyboard_irq(void) {
    uint8_t sc = inb(KB_DATA);

    if (sc == 0xE0) { ext = true;  return; }

    bool released = (sc & 0x80) != 0;
    uint8_t code  = sc & 0x7F;

    if (ext) {
        ext = false;
        if (!released) {
            int k = 0;
            switch (code) {
                case 0x48: k = KEY_UP;    break;
                case 0x50: k = KEY_DOWN;  break;
                case 0x4B: k = KEY_LEFT;  break;
                case 0x4D: k = KEY_RIGHT; break;
                case 0x47: k = KEY_HOME;  break;
                case 0x4F: k = KEY_END;   break;
                case 0x53: k = KEY_DEL;   break;
            }
            if (k) buf_push(k);
        }
        return;
    }

    /* modifier keys */
    if (code == 0x2A || code == 0x36) { shift = !released; return; }
    if (code == 0x3A && !released)    { caps  = !caps;     return; }
    if (code == 0x1D)                 { ctrl  = !released; return; }  /* L-Ctrl */

    if (released || code >= 58) return;

    /* Ctrl+key → ASCII control characters */
    if (ctrl) {
        char ch = lower[code];
        if (ch >= 'a' && ch <= 'z')
            buf_push((int)(ch - 'a' + 1));   /* Ctrl+A=1 … Ctrl+Z=26 */
        else if (code == 0x01) buf_push(KEY_ESC);
        return;
    }

    bool use_upper = shift ^ (caps && lower[code] >= 'a' && lower[code] <= 'z');
    char ch = use_upper ? upper[code] : lower[code];
    if (ch) buf_push((int)(unsigned char)ch);
    else if (code == 0x01) buf_push(KEY_ESC);
}

/* ── public API ──────────────────────────────────────────── */

void keyboard_init(void) {
    /* flush any pending data */
    while (inb(KB_STATUS) & 0x01) inb(KB_DATA);
}

bool keyboard_haskey(void) {
    return buf_r != buf_w;
}

int keyboard_getkey(void) {
    while (!keyboard_haskey()) cpu_halt();
    int k = buf[buf_r];
    buf_r = (buf_r + 1) % BUF_SZ;
    return k;
}

char keyboard_getchar(void) {
    for (;;) {
        int k = keyboard_getkey();
        if (k >= 0x100) continue;    /* skip special keys */
        char c = (char)k;
        if (c == '\n' || c == '\b' || (c >= 0x20 && c < 0x7F)) return c;
    }
}
