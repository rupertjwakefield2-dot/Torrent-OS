#pragma once
#include <stdint.h>
#include <stdbool.h>

/* Special key codes returned by keyboard_getkey() when > 127 */
#define KEY_UP    0x101
#define KEY_DOWN  0x102
#define KEY_LEFT  0x103
#define KEY_RIGHT 0x104
#define KEY_HOME  0x105
#define KEY_END   0x106
#define KEY_DEL   0x107
#define KEY_ESC   0x108
#define KEY_F1    0x110
#define KEY_F2    0x111
#define KEY_F3    0x112
#define KEY_F4    0x113
#define KEY_F5    0x114

void keyboard_init(void);
void keyboard_irq(void);          /* called by IRQ1 handler */
int  keyboard_getkey(void);       /* blocking: returns char or KEY_* */
bool keyboard_haskey(void);       /* non-blocking: true if key waiting */
char keyboard_getchar(void);      /* blocking: printable + \n + \b only */
