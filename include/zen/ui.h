#ifndef UI_H_
#define UI_H_

#include <ncurses.h>

typedef struct {
	char public_key[PK_SIZE * 2 + 1];
	char private_key[SK_SIZE * 2 + 1];
	char bin_pk[PK_SIZE];
	char bin_sk[SK_SIZE];
	char server_address[256];
} config_t;

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

void send_message(uint8_t *recipient, uint8_t *content, int sockfd);

void ncurses_init(void);
void windows_init(void);
void draw_border(WINDOW *window, bool active);
void wpprintw(const char *fmt, ...);
void print_message(uint8_t *author, uint8_t *content, time_t creation);
void show_chat(uint8_t *recipient);
void add_username(char *username);
uint8_t *client_kx(keypair_t *kp_from, uint8_t *recipient);
void update_current_user(uint8_t *username);
void deinit(void);
void ui(int fd, config_t *config);

#endif
