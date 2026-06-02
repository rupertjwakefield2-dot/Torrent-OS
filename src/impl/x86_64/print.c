#include "print.h"
#include "io.h"

#define VGA_W   80
#define VGA_H   25
#define VGA_MEM ((volatile uint16_t*)0xB8000)

static uint8_t cur_fg  = COLOR_LIGHT_GRAY;
static uint8_t cur_bg  = COLOR_BLACK;
static uint8_t cur_col = 0;
static uint8_t cur_row = TERM_TOP;

static uint8_t  vga_color(Color fg, Color bg) { return (uint8_t)((bg << 4) | fg); }
static uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)((uint16_t)color << 8 | (uint8_t)c);
}

static void hw_cursor(void) {
    uint16_t pos = (uint16_t)(cur_row * VGA_W + cur_col);
    outb(0x3D4, 0x0F); outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (uint8_t)(pos >> 8));
}

static void scroll(void) {
    /* scroll only the terminal area (rows TERM_TOP..VGA_H-1) */
    for (int y = TERM_TOP; y < VGA_H - 1; y++)
        for (int x = 0; x < VGA_W; x++)
            VGA_MEM[y * VGA_W + x] = VGA_MEM[(y + 1) * VGA_W + x];
    uint8_t c = vga_color(cur_fg, cur_bg);
    for (int x = 0; x < VGA_W; x++)
        VGA_MEM[(VGA_H - 1) * VGA_W + x] = vga_entry(' ', c);
    cur_row = VGA_H - 1;
}

void print_init(void) {
    /* thin underline cursor */
    outb(0x3D4, 0x0A); outb(0x3D5, (uint8_t)((inb(0x3D5) & 0xC0) | 13));
    outb(0x3D4, 0x0B); outb(0x3D5, (uint8_t)((inb(0x3D5) & 0xE0) | 15));
    /* blank entire screen */
    uint8_t dc = vga_color(COLOR_LIGHT_GRAY, COLOR_BLACK);
    for (int i = 0; i < VGA_W * VGA_H; i++)
        VGA_MEM[i] = vga_entry(' ', dc);
    cur_col = 0;
    cur_row = TERM_TOP;
    hw_cursor();
}

void print_clear(void) {
    uint8_t c = vga_color(cur_fg, cur_bg);
    for (int y = TERM_TOP; y < VGA_H; y++)
        for (int x = 0; x < VGA_W; x++)
            VGA_MEM[y * VGA_W + x] = vga_entry(' ', c);
    cur_col = 0;
    cur_row = TERM_TOP;
    hw_cursor();
}

void print_set_color(Color fg, Color bg) {
    cur_fg = (uint8_t)fg;
    cur_bg = (uint8_t)bg;
}

void print_char(char c) {
    if (cur_row < TERM_TOP) cur_row = TERM_TOP;
    if (c == '\n') {
        cur_col = 0;
        if (++cur_row >= VGA_H) scroll();
    } else if (c == '\r') {
        cur_col = 0;
    } else if (c == '\t') {
        cur_col = (uint8_t)((cur_col + 8) & ~7u);
        if (cur_col >= VGA_W) { cur_col = 0; if (++cur_row >= VGA_H) scroll(); }
    } else {
        VGA_MEM[cur_row * VGA_W + cur_col] = vga_entry(c, vga_color(cur_fg, cur_bg));
        if (++cur_col >= VGA_W) { cur_col = 0; if (++cur_row >= VGA_H) scroll(); }
    }
    hw_cursor();
}

void print_str(const char *s)   { while (*s) print_char(*s++); }
void print_newline(void)        { print_char('\n'); }

void print_int(int64_t n) {
    if (n < 0) { print_char('-'); n = -n; }
    print_uint((uint64_t)n);
}
void print_uint(uint64_t n) {
    if (n == 0) { print_char('0'); return; }
    char buf[20]; int i = 0;
    while (n) { buf[i++] = (char)('0' + n % 10); n /= 10; }
    for (int j = i - 1; j >= 0; j--) print_char(buf[j]);
}
void print_hex(uint64_t n) {
    const char *h = "0123456789ABCDEF";
    print_str("0x");
    if (n == 0) { print_char('0'); return; }
    char buf[16]; int i = 0;
    while (n) { buf[i++] = h[n & 0xF]; n >>= 4; }
    for (int j = i - 1; j >= 0; j--) print_char(buf[j]);
}
void print_backspace(void) {
    if (cur_col > 0) {
        cur_col--;
    } else if (cur_row > TERM_TOP) {
        cur_row--;
        cur_col = VGA_W - 1;
    }
    VGA_MEM[cur_row * VGA_W + cur_col] = vga_entry(' ', vga_color(cur_fg, cur_bg));
    hw_cursor();
}

uint8_t print_col(void) { return cur_col; }
uint8_t print_row(void) { return cur_row; }
void    print_move(uint8_t col, uint8_t row) { cur_col = col; cur_row = row; hw_cursor(); }

/* ── direct-position helpers (no terminal cursor side-effects) ── */

void print_at(uint8_t col, uint8_t row, char c, Color fg, Color bg) {
    VGA_MEM[row * VGA_W + col] = vga_entry(c, vga_color(fg, bg));
}

void print_str_at(uint8_t col, uint8_t row, const char *s, Color fg, Color bg) {
    uint8_t x = col;
    uint8_t color = vga_color(fg, bg);
    while (*s && x < VGA_W)
        VGA_MEM[row * VGA_W + x++] = vga_entry(*s++, color);
}

void print_center_str(uint8_t row, const char *s, Color fg, Color bg) {
    size_t len = 0;
    for (const char *p = s; *p; p++) len++;
    uint8_t col = (len < VGA_W) ? (uint8_t)((VGA_W - len) / 2) : 0;
    print_str_at(col, row, s, fg, bg);
}

void print_clear_row(uint8_t row, Color fg, Color bg) {
    uint8_t c = vga_color(fg, bg);
    for (int x = 0; x < VGA_W; x++)
        VGA_MEM[row * VGA_W + x] = vga_entry(' ', c);
}

void print_statusbar(const char *left, const char *right, Color fg, Color bg) {
    print_clear_row(0, fg, bg);
    if (left)  print_str_at(1, 0, left, fg, bg);
    if (right) {
        size_t rlen = 0;
        for (const char *p = right; *p; p++) rlen++;
        if (rlen + 1 < VGA_W)
            print_str_at((uint8_t)(VGA_W - 1 - rlen), 0, right, fg, bg);
    }
}

void print_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, Color fg, Color bg) {
    uint8_t color = vga_color(fg, bg);
    if (w < 2 || h < 2) return;
    /* corners */
    VGA_MEM[ y          * VGA_W + x      ] = vga_entry('\xC9', color); /* ╔ */
    VGA_MEM[ y          * VGA_W + x+w-1  ] = vga_entry('\xBB', color); /* ╗ */
    VGA_MEM[(y+h-1)     * VGA_W + x      ] = vga_entry('\xC8', color); /* ╚ */
    VGA_MEM[(y+h-1)     * VGA_W + x+w-1  ] = vga_entry('\xBC', color); /* ╝ */
    /* top / bottom edges */
    for (int i = 1; i < w - 1; i++) {
        VGA_MEM[ y      * VGA_W + x+i] = vga_entry('\xCD', color); /* ═ */
        VGA_MEM[(y+h-1) * VGA_W + x+i] = vga_entry('\xCD', color);
    }
    /* side edges + fill */
    for (int j = 1; j < h - 1; j++) {
        VGA_MEM[(y+j) * VGA_W + x      ] = vga_entry('\xBA', color); /* ║ */
        VGA_MEM[(y+j) * VGA_W + x+w-1  ] = vga_entry('\xBA', color);
        for (int i = 1; i < w - 1; i++)
            VGA_MEM[(y+j) * VGA_W + x+i] = vga_entry(' ', color);
    }
}

void print_hline(uint8_t x, uint8_t y, uint8_t w, Color fg, Color bg) {
    uint8_t color = vga_color(fg, bg);
    for (int i = 0; i < w; i++)
        VGA_MEM[y * VGA_W + x + i] = vga_entry('\xC4', color); /* ─ */
}
