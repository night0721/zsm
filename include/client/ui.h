#ifndef UI_H_
#define UI_H_

#include <ncurses.h>

void ncurses_init();
void windows_init();
void draw_border(WINDOW *window, bool active);
void add_username(char *username);
void ui();

#endif
