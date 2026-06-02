#pragma once

void desktop_init(void);       /* call once after login */
void desktop_tick(void);       /* call from the 1-second timer callback */
void desktop_redraw_bar(void); /* force-refresh the status bar */
