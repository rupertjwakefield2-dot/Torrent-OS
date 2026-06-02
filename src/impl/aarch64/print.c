/*
 * TorrentOS AArch64 print driver
 * Uses PL011 UART (0x09000000) with ANSI escape codes.
 * This gives colour, cursor movement, and clear-screen over QEMU serial.
 */
#include "print.h"

/* PL011 UART — QEMU virt machine base */
#define UART_BASE  0x09000000UL
#define UART_DR    (UART_BASE + 0x000)   /* data register       */
#define UART_FR    (UART_BASE + 0x018)   /* flag register       */
#define UART_IBRD  (UART_BASE + 0x024)   /* integer baud rate   */
#define UART_CR    (UART_BASE + 0x030)   /* control register    */
#define UART_FR_TXFF (1u << 5)           /* TX FIFO full        */
#define UART_FR_RXFE (1u << 4)           /* RX FIFO empty       */

static volatile uint32_t *uart_dr = (volatile uint32_t*)UART_DR;
static volatile uint32_t *uart_fr = (volatile uint32_t*)UART_FR;
static volatile uint32_t *uart_cr = (volatile uint32_t*)UART_CR;

static void uart_init(void) {
    *uart_cr = (1u << 8) | (1u << 0);   /* TXE | UARTEN */
}

static void uart_putc(char c) {
    while (*uart_fr & UART_FR_TXFF);
    *uart_dr = (uint32_t)(unsigned char)c;
}

static void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}

/* ── ANSI colour tables ── */
static const char *ansi_fg[16] = {
    "30","34","32","36","31","35","33","37",
    "90","94","92","96","91","95","93","97"
};
static const char *ansi_bg[16] = {
    "40","44","42","46","41","45","43","47",
    "100","104","102","106","101","105","103","107"
};

static uint8_t cur_fg = COLOR_LIGHT_GRAY;
static uint8_t cur_bg = COLOR_BLACK;
static uint8_t cur_col = 0;
static uint8_t cur_row = TERM_TOP;

/* send ESC sequence to position cursor (1-indexed ANSI) */
static void ansi_move(uint8_t col, uint8_t row) {
    char esc[20];
    /* "\033[{row+1};{col+1}H" */
    esc[0] = '\033'; esc[1] = '[';
    /* row */
    uint8_t r1 = (uint8_t)(row + 1);
    int i = 2;
    if (r1 >= 100) esc[i++] = (char)('0' + r1 / 100);
    if (r1 >= 10)  esc[i++] = (char)('0' + (r1 / 10) % 10);
    esc[i++] = (char)('0' + r1 % 10);
    esc[i++] = ';';
    /* col */
    uint8_t c1 = (uint8_t)(col + 1);
    if (c1 >= 100) esc[i++] = (char)('0' + c1 / 100);
    if (c1 >= 10)  esc[i++] = (char)('0' + (c1 / 10) % 10);
    esc[i++] = (char)('0' + c1 % 10);
    esc[i++] = 'H';
    esc[i]   = '\0';
    uart_puts(esc);
}

static void ansi_set_color(void) {
    uart_puts("\033[");
    uart_puts(ansi_fg[cur_fg & 0x0F]);
    uart_puts(";");
    uart_puts(ansi_bg[cur_bg & 0x0F]);
    uart_puts("m");
}

/* ── public API ── */

void print_init(void) {
    uart_init();
    cur_fg = COLOR_LIGHT_GRAY;
    cur_bg = COLOR_BLACK;
    cur_col = 0;
    cur_row = TERM_TOP;
    /* clear screen, move to top-left */
    uart_puts("\033[2J\033[H");
    ansi_set_color();
}

void print_clear(void) {
    /* clear from row TERM_TOP downwards — erase all lines individually */
    for (int r = TERM_TOP; r < VGA_ROWS; r++) {
        ansi_move(0, (uint8_t)r);
        uart_puts("\033[2K");   /* erase entire line */
    }
    cur_col = 0;
    cur_row = TERM_TOP;
    ansi_move(cur_col, cur_row);
}

void print_set_color(Color fg, Color bg) {
    cur_fg = (uint8_t)fg;
    cur_bg = (uint8_t)bg;
    ansi_set_color();
}

void print_char(char c) {
    if (cur_row < TERM_TOP) { cur_row = TERM_TOP; ansi_move(cur_col, cur_row); }
    if (c == '\n') {
        uart_putc('\r');
        uart_putc('\n');
        cur_col = 0;
        cur_row++;
        if (cur_row >= VGA_ROWS) {
            cur_row = VGA_ROWS - 1;
        }
    } else if (c == '\r') {
        uart_putc('\r');
        cur_col = 0;
    } else if (c == '\t') {
        uint8_t new_col = (uint8_t)((cur_col + 8) & ~7u);
        while (cur_col < new_col) { uart_putc(' '); cur_col++; }
    } else {
        uart_putc(c);
        cur_col++;
        if (cur_col >= VGA_COLS) {
            uart_putc('\r'); uart_putc('\n');
            cur_col = 0;
            cur_row++;
        }
    }
}

void print_str(const char *s)    { while (*s) print_char(*s++); }
void print_newline(void)         { print_char('\n'); }

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
        uart_puts("\033[D \033[D");  /* cursor left, space, cursor left */
    }
}

uint8_t print_col(void) { return cur_col; }
uint8_t print_row(void) { return cur_row; }

void print_move(uint8_t col, uint8_t row) {
    cur_col = col; cur_row = row;
    ansi_move(col, row);
}

/* ── direct-position helpers ── */

void print_at(uint8_t col, uint8_t row, char c, Color fg, Color bg) {
    uint8_t ofc = cur_col, ofr = cur_row;
    uint8_t ofg = cur_fg,  obg = cur_bg;
    cur_fg = (uint8_t)fg; cur_bg = (uint8_t)bg;
    ansi_set_color();
    ansi_move(col, row);
    uart_putc(c);
    /* restore */
    cur_fg = ofg; cur_bg = obg; ansi_set_color();
    ansi_move(ofc, ofr);
    cur_col = ofc; cur_row = ofr;
}

void print_str_at(uint8_t col, uint8_t row, const char *s, Color fg, Color bg) {
    uint8_t ofc = cur_col, ofr = cur_row;
    uint8_t ofg = cur_fg,  obg = cur_bg;
    cur_fg = (uint8_t)fg; cur_bg = (uint8_t)bg;
    ansi_set_color();
    ansi_move(col, row);
    while (*s && col < VGA_COLS) { uart_putc(*s++); col++; }
    /* restore */
    cur_fg = ofg; cur_bg = obg; ansi_set_color();
    ansi_move(ofc, ofr);
    cur_col = ofc; cur_row = ofr;
}

void print_center_str(uint8_t row, const char *s, Color fg, Color bg) {
    size_t len = 0;
    for (const char *p = s; *p; p++) len++;
    uint8_t col = (len < VGA_COLS) ? (uint8_t)((VGA_COLS - len) / 2) : 0;
    print_str_at(col, row, s, fg, bg);
}

void print_clear_row(uint8_t row, Color fg, Color bg) {
    uint8_t ofc = cur_col, ofr = cur_row;
    uint8_t ofg = cur_fg,  obg = cur_bg;
    cur_fg = (uint8_t)fg; cur_bg = (uint8_t)bg;
    ansi_set_color();
    ansi_move(0, row);
    uart_puts("\033[2K");
    cur_fg = ofg; cur_bg = obg; ansi_set_color();
    ansi_move(ofc, ofr);
    cur_col = ofc; cur_row = ofr;
}

void print_statusbar(const char *left, const char *right, Color fg, Color bg) {
    print_clear_row(0, fg, bg);
    if (left)  print_str_at(1, 0, left, fg, bg);
    if (right) {
        size_t rlen = 0;
        for (const char *p = right; *p; p++) rlen++;
        if (rlen + 1 < VGA_COLS)
            print_str_at((uint8_t)(VGA_COLS - 1 - rlen), 0, right, fg, bg);
    }
}

void print_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, Color fg, Color bg) {
    if (w < 2 || h < 2) return;
    /* use plain ASCII box on serial (CP437 may not render) */
    print_at(x,       y,       '+', fg, bg);
    print_at((uint8_t)(x+w-1), y,       '+', fg, bg);
    print_at(x,       (uint8_t)(y+h-1), '+', fg, bg);
    print_at((uint8_t)(x+w-1), (uint8_t)(y+h-1), '+', fg, bg);
    for (int i = 1; i < w-1; i++) {
        print_at((uint8_t)(x+i), y,           '-', fg, bg);
        print_at((uint8_t)(x+i), (uint8_t)(y+h-1), '-', fg, bg);
    }
    for (int j = 1; j < h-1; j++) {
        print_at(x,       (uint8_t)(y+j), '|', fg, bg);
        print_at((uint8_t)(x+w-1), (uint8_t)(y+j), '|', fg, bg);
        for (int i = 1; i < w-1; i++)
            print_at((uint8_t)(x+i), (uint8_t)(y+j), ' ', fg, bg);
    }
}

void print_hline(uint8_t x, uint8_t y, uint8_t w, Color fg, Color bg) {
    for (int i = 0; i < w; i++)
        print_at((uint8_t)(x+i), y, '-', fg, bg);
}
