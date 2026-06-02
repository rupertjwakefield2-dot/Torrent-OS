#pragma once
#include <stdint.h>
#include <stddef.h>

#define TERM_TOP  1    /* row 0 is the status bar; terminal uses rows 1-24 */
#define VGA_COLS  80
#define VGA_ROWS  25

typedef enum {
    COLOR_BLACK       = 0,
    COLOR_BLUE        = 1,
    COLOR_GREEN       = 2,
    COLOR_CYAN        = 3,
    COLOR_RED         = 4,
    COLOR_MAGENTA     = 5,
    COLOR_BROWN       = 6,
    COLOR_LIGHT_GRAY  = 7,
    COLOR_DARK_GRAY   = 8,
    COLOR_LIGHT_BLUE  = 9,
    COLOR_LIGHT_GREEN = 10,
    COLOR_LIGHT_CYAN  = 11,
    COLOR_LIGHT_RED   = 12,
    COLOR_PINK        = 13,
    COLOR_YELLOW      = 14,
    COLOR_WHITE       = 15,
} Color;

/* core terminal output */
void    print_init(void);
void    print_clear(void);
void    print_set_color(Color fg, Color bg);
void    print_char(char c);
void    print_str(const char *s);
void    print_int(int64_t n);
void    print_uint(uint64_t n);
void    print_hex(uint64_t n);
void    print_newline(void);
void    print_backspace(void);
uint8_t print_col(void);
uint8_t print_row(void);
void    print_move(uint8_t col, uint8_t row);

/* direct-position helpers (do NOT move the terminal cursor) */
void print_at(uint8_t col, uint8_t row, char c, Color fg, Color bg);
void print_str_at(uint8_t col, uint8_t row, const char *s, Color fg, Color bg);
void print_center_str(uint8_t row, const char *s, Color fg, Color bg);
void print_clear_row(uint8_t row, Color fg, Color bg);
void print_statusbar(const char *left, const char *right, Color fg, Color bg);
void print_box(uint8_t x, uint8_t y, uint8_t w, uint8_t h, Color fg, Color bg);
void print_hline(uint8_t x, uint8_t y, uint8_t w, Color fg, Color bg);
