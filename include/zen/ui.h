#ifndef UI_H_
#define UI_H_

#include <ncurses.h>

enum modes {
	NORMAL,
	INSERT,
	COMMAND
};

enum windows {
	USERS_WINDOW,
	CHAT_WINDOW
};

enum colors {
	BLUE = 9,
	GREEN,
	PEACH,
	YELLOW,
	LAVENDER,
	PINK,
	MAUVE,
	RED,
	SURFACE1
};

/* Key code */
#define CTRLA 0x01
#define CTRLD 0x04
#define CTRLE 0x05
#define CTRLX 0x18
#define DOWN 0x102
#define UP 0x103
#define LEFT 0x104
#define RIGHT 0x105
#define ENTER 0xA
#define ESC 0x1B

#define MAX_ARGS 10

void ncurses_init();
void windows_init();
void draw_border(WINDOW *window, bool active);
void add_message(uint8_t *author, uint8_t *recipient, uint8_t *content, uint32_t length, time_t creation);
void show_chat(uint8_t *recipient);
void add_username(char *username);
void deinit();
void ui();

#endif
