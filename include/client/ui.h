#ifndef UI_H_
#define UI_H_

#include <ncurses.h>

#define USERS_WINDOW 0
#define CHAT_WINDOW 1

/* Keybindings */
#define CTRLA 0x01
#define CTRLD 0x04
#define CTRLE 0x05
#define DOWN 0x102
#define UP 0x103
#define LEFT 0x104
#define RIGHT 0x105

void ncurses_init();
void windows_init();
void draw_border(WINDOW *window, bool active);
void add_message(uint8_t *author, uint8_t *recipient, uint8_t *content, uint32_t length, time_t creation);
void show_chat(uint8_t *recipient);
void add_username(char *username);
void ui();

#endif
