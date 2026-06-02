/*
 * TorrentOS AArch64 keyboard driver
 * Uses PL011 UART RX for input. Translates ANSI escape sequences
 * into the same KEY_* constants the x86_64 PS/2 driver produces.
 */
#include "keyboard.h"
#include <stdbool.h>

#define UART_BASE  0x09000000UL
#define UART_DR    (*(volatile uint32_t*)(UART_BASE + 0x000))
#define UART_FR    (*(volatile uint32_t*)(UART_BASE + 0x018))
#define UART_FR_RXFE (1u << 4)
#define UART_FR_TXFF (1u << 5)

/* ring buffer */
#define BUF_SZ 256
static int  buf[BUF_SZ];
static int  buf_r = 0, buf_w = 0;

static void buf_push(int k) {
    int next = (buf_w + 1) % BUF_SZ;
    if (next != buf_r) { buf[buf_w] = k; buf_w = next; }
}

/* read one raw byte from UART (blocking) */
static int uart_getc(void) {
    while (UART_FR & UART_FR_RXFE)
        __asm__ volatile("wfe");
    return (int)(UART_DR & 0xFF);
}

/* non-blocking raw byte; returns -1 if nothing */
static int uart_trygetc(void) {
    if (UART_FR & UART_FR_RXFE) return -1;
    return (int)(UART_DR & 0xFF);
}

/* parse one raw character / escape sequence */
static void parse_input(void) {
    int c = uart_getc();
    if (c != 0x1B) {
        /* Ctrl+letter */
        if (c == 0x7F) { buf_push('\b'); return; }
        if (c == '\r') { buf_push('\n'); return; }
        buf_push(c);
        return;
    }
    /* ESC — check for sequence */
    int c2 = uart_trygetc();
    if (c2 == -1) { buf_push(KEY_ESC); return; }
    if (c2 != '[') { buf_push(KEY_ESC); buf_push(c2); return; }
    /* CSI sequence: ESC [ ... */
    int c3 = uart_getc();
    switch (c3) {
        case 'A': buf_push(KEY_UP);    break;
        case 'B': buf_push(KEY_DOWN);  break;
        case 'C': buf_push(KEY_RIGHT); break;
        case 'D': buf_push(KEY_LEFT);  break;
        case 'H': buf_push(KEY_HOME);  break;
        case 'F': buf_push(KEY_END);   break;
        case '3': {
            int c4 = uart_getc();   /* expect '~' */
            if (c4 == '~') buf_push(KEY_DEL);
            break;
        }
        case '1': {
            int c4 = uart_getc();
            if (c4 == '~') buf_push(KEY_HOME);
            break;
        }
        case '4': {
            int c4 = uart_getc();
            if (c4 == '~') buf_push(KEY_END);
            break;
        }
        default: break;
    }
}

void keyboard_init(void)   { /* UART already on */ }
void keyboard_irq(void)    { /* unused on ARM64 — polled */ }

bool keyboard_haskey(void) {
    /* pull from UART into buffer if data available */
    if (!(UART_FR & UART_FR_RXFE)) parse_input();
    return buf_r != buf_w;
}

int keyboard_getkey(void) {
    while (buf_r == buf_w) parse_input();
    int k = buf[buf_r];
    buf_r = (buf_r + 1) % BUF_SZ;
    return k;
}

char keyboard_getchar(void) {
    for (;;) {
        int k = keyboard_getkey();
        if (k >= 0x100) continue;
        char c = (char)k;
        if (c == '\n' || c == '\b' || (c >= 0x20 && c < 0x7F)) return c;
    }
}
