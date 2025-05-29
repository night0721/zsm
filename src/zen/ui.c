#include "config.h"
#include "packet.h"
#include "util.h"
#include "zen/ui.h"
#include "zen/db.h"
#include "zen/user.h"

WINDOW *panel;
WINDOW *status_bar;
WINDOW *users_border;
WINDOW *chat_border;
WINDOW *users_content;
WINDOW *chat_content;

ArrayList *users;
ArrayList *marked;
int num_messages = 0;
long current_user = 0;
int current_window = 0;
int current_mode = 0;
int sockfd;
config_t *current_config;

/* For tracking cursor position in content */
static int curs_pos = 0;
static char content[MAX_MESSAGE_LENGTH];

/*
 * Free and close everything
 */
void deinit(void)
{
	close(sockfd);
	arraylist_free(users);
	arraylist_free(marked);
	endwin();
}

void signal_handler(int signal)
{
	switch (signal) {
		case SIGPIPE:
			error(0, "SIGPIPE received");
			break;
		case SIGABRT:
		case SIGINT:
		case SIGTERM:
			break;
	}
}

/* 
 * Convert hex to RGB values
 * Create pair in ncurses 
 */
void create_pair(int hex_value, int index)
{
	int r = ((hex_value >> 16) & 0xFF) * 1000 / 255;
	int g = ((hex_value >> 8) & 0xFF) * 1000 / 255;
	int b = (hex_value & 0xFF) * 1000 / 255;
	init_color(index, r, g, b);
	init_pair(index, index, -1);
}

/*
 * Start ncurses
 */
void ncurses_init(void)
{
	/* check if it is interactive shell */
	if (!isatty(STDIN_FILENO)) {
		error(1, "No tty detected. zen requires an interactive shell to run");
	}

	/* initialize screen, don't print special chars
	 * make ctrl + c work, don't show cursor 
	 * enable arrow keys */
	initscr();
	noecho();
	cbreak();
	keypad(stdscr, TRUE);
	set_escdelay(25);
	curs_set(0);

	/* check terminal has colors */
	if (!has_colors()) {
		endwin();
		error(1, "Color is not supported in your terminal");
	} else {
		use_default_colors();
		start_color();
	}
	/* colors */
	init_pair(1, COLOR_BLACK, -1);	  /*  */
	init_pair(2, COLOR_RED, -1);		/*  */
	init_pair(3, COLOR_GREEN, -1);	  /* active window */
	init_pair(4, COLOR_YELLOW, -1);	 /*  */
	init_pair(5, COLOR_BLUE, -1);	   /* inactive window */ 
	init_pair(6, COLOR_MAGENTA, -1);	/*  */
	init_pair(7, COLOR_CYAN, -1);	   /* Selected user */
	init_pair(8, COLOR_WHITE, -1);	  /*  */

	int colors[] = { 0x89b4fa, 0xa6e3a1, 0xfab387, 0xf9e2af, 0xb4befe, 0xf38ba8, 0xcba6f7, 0xf38ba8, 0x45475a };
	int num_colors = sizeof(colors) / sizeof(colors[0]);
	for (int i = 0; i < num_colors; i++) {
		/* Custom color starts from 9 */
		create_pair(colors[i], i + 9);
	}

	num_colors += 8;
	/* for status bar */
	init_pair(num_colors + 1, BLUE, SURFACE1);
	init_pair(num_colors + 2, GREEN, SURFACE1);
	init_pair(num_colors + 3, PEACH, SURFACE1);
}

/*
 * Draw windows
 */
void windows_init(void)
{
	int users_width = MAX_NAME / 2;
	int chat_width = COLS - (MAX_NAME / 2);

	/*----border----||---border-----+
	  ||			  ||			 ||
	  || content	  || content	 ||
	  || (users)	  ||  (chat)	 ||
	  ||			  ||			 ||
	  ||			  ||			 ||
	  ||----border----||---border----||
	  ==========status bar=============
	  +==========panel===============*/

	/*					 lines,									 cols,			y,										x			 */
	panel =		 newwin(PANEL_HEIGHT,							 COLS,			LINES - PANEL_HEIGHT,						0			  );
	status_bar =	newwin(STATUS_BAR_HEIGHT,						 COLS,			  LINES - PANEL_HEIGHT - STATUS_BAR_HEIGHT, 0			   );
	users_border =  newwin(LINES - PANEL_HEIGHT - STATUS_BAR_HEIGHT, users_width + 2, 0,										0			  );
	chat_border =   newwin(LINES - PANEL_HEIGHT - STATUS_BAR_HEIGHT, chat_width - 2,  0,										users_width + 2);

	/*									 lines,										   cols,			y,					x			 */
	users_content = subwin(users_border, LINES - PANEL_HEIGHT - 2 - STATUS_BAR_HEIGHT, users_width,	 1,					1			  );
	chat_content =  subwin(chat_border,  LINES - PANEL_HEIGHT - 2 - STATUS_BAR_HEIGHT, chat_width - 4,  1,					users_width + 3);

	/* draw border around windows */
	refresh();
	draw_border(users_border, true);
	draw_border(chat_border, false);

	scrollok(users_content, true);
	scrollok(chat_content, true);
	refresh();
}

/*
 * Draw the border of the window depending if it's active or not
 */
void draw_border(WINDOW *window, bool active)
{
	/* turn on color depends on active */
	if (active) {
		wattron(window, COLOR_PAIR(3));
	} else {
		wattron(window, COLOR_PAIR(5));
	}

	box(window, 0, 0);

	/* turn color off after turning it on */
	if (active) {
		wattroff(window, COLOR_PAIR(3));
	} else {
		wattroff(window, COLOR_PAIR(5));
	}

	/* Refresh the window to see the colored border and title */
	wrefresh(window);
}

/*
 * Print line to the panel
 */
void wpprintw(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	wclear(panel);
	vw_printw(panel, fmt, args);
	va_end(args);
	wrefresh(panel);
}

/*
 * Highlight current line by reversing the color
 */
void draw_users(void)
{
	long overflow = 0;
	/* Check if the current selected user is not shown in rendered text */
	if (current_user > LINES - 3) {
		/* overflown */
		overflow = current_user - (LINES - 3);
	}

	/* Calculate number of users to show */
	long range = users->length;
	/* Stop drawing if there is no users */
	if (range == 0) {
		wprintw(chat_content, "No users. Start a converstation.");
		wrefresh(chat_content);
		return;
	}

	if (range > LINES - 3) {
		/* if there are more users than lines available to display
		 * shrink range to avaiable lines to display with
		 * overflow to keep the number of iterations to be constant */
		range = LINES - 3 + overflow;
	}

	/* Clears content before printing */
	wclear(users_content);

	/* To keep track the line to print after overflow */
	long line_count = 0;
	for (long i = overflow; i < range; i++) {
		/* Check for currently selected user */
		if ((overflow == 0 && i == current_user) || (overflow != 0 && i == current_user)) {
			/* current selected user should have color reversed */
			wattron(users_content, A_REVERSE);

			/* check for marked users */
			long num_marked = marked->length;
			if (num_marked > 0) {
				/* Determine length of formatted string */
				int m_len = snprintf(NULL, 0, "[%ld] selected", num_marked);
				char selected[m_len + 1];

				snprintf(selected, m_len + 1, "[%ld] selected", num_marked);
				wpprintw("(%ld/%ld) %s", current_user + 1, users->length, selected);
			} else  {
				wpprintw("(%ld/%ld)", current_user + 1, users->length);
			}
		}
		/* print the actual filename and stats */
		user seluser = users->items[i];
		size_t name_len = strlen(seluser.nickname);

		/* If length of name is longer than half of allowed size in window,
		 * trim it to end with .. to show the it is too long to be displayed
		 */
		int too_long = 0;
		if (name_len > MAX_NAME / 2) {
			/* Make space for truncation */
			name_len = MAX_NAME / 2 - 2;
			too_long = 1;
		}

		char line[name_len + 1];
		if (too_long) {
			strncpy(line, seluser.nickname, (MAX_NAME / 2) - 2);
			strncat(line, "..", 3);
		} else {
			strncpy(line, seluser.nickname, name_len + 1);
			line[name_len] = '\0';
		}

		int color = users->items[i].color;

		/* check is user marked for action */
		bool is_marked = arraylist_search(marked, users->items[i].name) != -1;
		if (is_marked) {
			/* show user is selected */
			wattron(users_content, COLOR_PAIR(7));
		} else {
			/* print username with default color */
			wattron(users_content, COLOR_PAIR(color));
		}

		if (overflow > 0) {
			mvwprintw(users_content, line_count, 0, "%s", line);
		} else {
			mvwprintw(users_content, i, 0, "%s", line);
		}

		/* turn off color after printing */
		if (is_marked) {
			wattroff(users_content, COLOR_PAIR(7));
		} else {
			wattroff(users_content, COLOR_PAIR(color));
		}

		wattroff(users_content, A_REVERSE);
		line_count++;
	}

	wrefresh(users_content);
	wrefresh(panel);
	/* show chat conversation every time cursor changes */
	show_chat(users->items[current_user].name);
}

/*
 * Add message to chat window
 * if flag is 1, print date as well
 * user_color is the color defined above at ncurses_init
 */
void print_message(uint8_t *author, uint8_t *content, time_t creation)
{
	struct tm *timeinfo = localtime(&creation);
	char timestr[21];
	strftime(timestr, sizeof(timestr), "%b %d %Y %H:%M:%S", timeinfo);
	wprintw(chat_content, "%s ", timestr);

	wattron(chat_content, A_BOLD);
	int user_color = get_user_color(users, author);
	wattron(chat_content, COLOR_PAIR(user_color));
	wprintw(chat_content, "<%s> ", author);
	wattroff(chat_content, A_BOLD);
	wattroff(chat_content, COLOR_PAIR(user_color));

	int i = 0;
	int n = strlen(content);
	int in_bold = 0, in_italic = 0, in_underline = 0, in_block = 0;
	int last_active_color = -1;

	while (i < n) {
		/* Bold */
		if (content[i] == '*' && content[i + 1] == '*') {
			if (!in_bold) {
				/* Look ahead for the matching closing delimiter */
				int closing_pos = i + 2;
				while (closing_pos < n && !(content[closing_pos] == '*' && content[closing_pos + 1] == '*')) {
					closing_pos++;
				}
				if (closing_pos < n) {
					wattron(chat_content, A_BOLD);
					in_bold = 1;
				} else {
					/* Treat as regular text if closing delimiter */
					waddch(chat_content, content[i++]);
				}
			} else {
				wattroff(chat_content, A_BOLD);
				in_bold = 0;
			}
			/* Skip */
			i += 2;

			/* Italic */
		} else if (content[i] == '*') {
			if (!in_italic) {
				/* Look ahead for the matching closing delimiter */
				int closing_pos = i + 1;
				while (closing_pos < n && content[closing_pos] != '*') {
					closing_pos++;
				}
				if (closing_pos < n) {
					wattron(chat_content, A_ITALIC);
					in_italic = 1;
				} else {
					/* Treat as regular text if closing delimiter */
					waddch(chat_content, content[i++]);
				}
			} else {
				wattroff(chat_content, A_ITALIC);
				in_italic = 0;
			}
			/* Skip */
			i += 1;

			/* Underline */
		} else if (content[i] == '_') {
			if (!in_underline) {
				/* Look ahead for the matching closing delimiter */
				int closing_pos = i + 1;
				while (closing_pos < n && content[closing_pos] != '_') {
					closing_pos++;
				}
				if (closing_pos < n) {
					wattron(chat_content, A_UNDERLINE);
					in_underline = 1;
				} else {
					/* Treat as regular text if closing delimiter */
					waddch(chat_content, content[i++]);
				}
			} else {
				wattroff(chat_content, A_UNDERLINE);
				in_underline = 0;
			}
			/* Skip */
			i += 1;

			/* Block */
		} else if (content[i] == '`') {
			if (!in_block) {
				/* Look ahead for the matching closing delimiter */
				int closing_pos = i + 1;
				while (closing_pos < n && content[closing_pos] != '`') {
					closing_pos++;
				}
				if (closing_pos < n) {
					wattron(chat_content, A_STANDOUT);
					in_block = 1;
				} else {
					/* Treat as regular text if closing delimiter */
					waddch(chat_content, content[i++]);
				}
			} else {
				wattroff(chat_content, A_STANDOUT);
				in_block = 0;
			}
			/* Skip */
			i += 1; 

			/* Allow escape sequence for genuine backslash */
		} else if (content[i] == '\\' && content[i + 1] == '\\') {
			/* Print a literal backslash */
			waddch(chat_content, '\\');
			/* Skip both backslashes */
			i += 2;

			/* Color, new line and tab */
		} else if (content[i] == '\\') {
			/* Skip the backslash and check the next character */
			i++;
			/* Handle color codes \1 to \8 */
			if (content[i] >= '1' && content[i] <= '8') {
				/* Convert char to int */
				int new_color = content[i] - '0';
				if (new_color == last_active_color) {
					/* Turn off current color */
					wattroff(chat_content, COLOR_PAIR(last_active_color));
					/* Reset last active color */
					last_active_color = -1;
				} else {
					if (last_active_color != -1) {
						/* Turn off previous color */
						wattroff(chat_content, COLOR_PAIR(last_active_color));
					}
					last_active_color = new_color;
					/* Turn on new color */
					wattron(chat_content, COLOR_PAIR(new_color));
				}
				i++;
				/* Handle new line */
			} else if (content[i] == 'n') {
				waddch(chat_content, '\n');
				/* Skip the 'n' */
				i++;

			} else {
				/* Invalid sequence, just print the backslash and character */
				waddch(chat_content, '\\');
				waddch(chat_content, content[i]);
				i++;
			}
		} else {
			/* Print regular character */ 
			waddch(chat_content, content[i]);
			i++;
		}
	}
	/* Ensure attributes are turned off after printing */
	wattroff(chat_content, A_BOLD);
	wattroff(chat_content, A_ITALIC);
	wattroff(chat_content, A_UNDERLINE);
	wattroff(chat_content, A_STANDOUT);
	for (int i = 1; i < 8; i++) {
		wattroff(chat_content, COLOR_PAIR(i));
	}
	waddch(chat_content, '\n');
}

void move_cursor(void)
{
	wmove(panel, 0, current_mode == INSERT ? curs_pos + 2 : curs_pos + 1);
	wrefresh(panel);
}

/*
 * Get chat conversation into buffer and show it to chat window
 */
void show_chat(uint8_t *recipient)
{
	/* Clear chat window */
	wclear(chat_content);

	/* Get messages between author and recipient and print them */
	get_messages(current_config->public_key, recipient);
	wrefresh(chat_content);
	/* after printing move cursor back to panel */
	move_cursor();
}

void add_username(char *username)
{
	int randomcolor = rand() % 17 + 2;
	arraylist_add(users, username, username, randomcolor);
}

void reset_content(void)
{
	/* Reset for new input */
	curs_pos = 0; 
	/* Set content[0] for printing purposes */
	content[0] = '\0';
}

void update_panel(void)
{
	wclear(panel);
	switch (current_mode) {
		case INSERT:
			if (current_window == CHAT_WINDOW) {
				/* Set cursor to visible */
				curs_set(2);
				mvwprintw(panel, 0, 0, "> %s", content);
			}
			break;
		case COMMAND:
			/* Set cursor to visible */
			curs_set(2);
			mvwprintw(panel, 0, 0, "/%s", content);
	}
	move_cursor();
}

/*
 * Key exchange with recipient
 */
uint8_t *client_kx(keypair_t *kp_from, uint8_t *recipient)
{
	uint8_t pk_to_bin[PK_SIZE]; /* ED25519 */
	sodium_hex2bin(pk_to_bin, PK_SIZE, recipient, PK_SIZE * 2, NULL, NULL, NULL);

	uint8_t *shared_key = get_sendkey(recipient);
	if (shared_key == NULL) {
		shared_key = memalloc(SHARED_KEY_SIZE);

		/* Key exchange need to be done with x25519 public and secret keys */
		uint8_t from_pk[PK_X25519_SIZE], to_pk[PK_X25519_SIZE],
		from_sk[SK_X25519_SIZE];
		if (crypto_sign_ed25519_pk_to_curve25519(from_pk, kp_from->pk) != 0
		   ) {
			wpprintw("Error converting ED25519 PK to X25519 PK");
			getch();
		}
		if (crypto_sign_ed25519_pk_to_curve25519(to_pk, pk_to_bin) != 0) {
			wpprintw("Error converting ED25519 PK to X25519 PK");
			getch();
		}
		if (crypto_sign_ed25519_sk_to_curve25519(from_sk, kp_from->sk) != 0) {
			wpprintw("Error converting ED25519 SK to X25519 SK");
			getch();
		}
		uint8_t dummy[SHARED_KEY_SIZE];
		if (crypto_kx_server_session_keys(shared_key, dummy, from_pk, from_sk,
					to_pk) != 0) {
			/* Recipient public key is suspicious */
			wpprintw("Error performing key exchange with %s", recipient);
			getch();
		}

		save_sendkey(recipient, shared_key);
	}
	return shared_key;
}

void update_current_user(uint8_t *username)
{
	current_user = arraylist_search(users, username);
	if (current_user == -1) {
		/* If user is not in runtime user list as it is new to database
		 * add it to runtime user list
		 * as latest added would be at the end so index would be length -1
		 */
		add_username(username);
		current_user = users->length - 1;
	}
	draw_users();
}

void use_command(void)
{
	/* Parse slash command */
	char *token = strtok(content, " ");
	char *command[MAX_ARGS];
	int args = 0;
	while (token) {
		command[args] = token;
		token = strtok(NULL, " ");
		args++;
	}

	if (args == 0) {
		goto end;
	}
	if (!strncmp(command[0], "chat", 4)) {
		if (args != 2) {
			wpprintw("chat command require 1 argument(username)");
			getch();
			goto end;
		}
		keypair_t *kp_from = memalloc(sizeof(keypair_t));
		sodium_hex2bin(kp_from->pk, PK_SIZE, current_config->public_key, PK_SIZE * 2, NULL, NULL, NULL);
		sodium_hex2bin(kp_from->sk, SK_SIZE, current_config->private_key, SK_SIZE * 2, NULL, NULL, NULL);
		client_kx(kp_from, command[1]);
		update_current_user(command[1]);
		show_chat(command[1]);
		free(kp_from);
	} else if (!strncmp(command[0], "nick", 4)) {
		if (args != 3) {
			wpprintw("nick command require 2 arguments");
			getch();
			goto end;
		}
		update_nickname(command[1], command[2]);
		draw_users();
	} else if (!strncmp(command[0], "clear", 5)) {
		/* Delete all messages from DB */
		clear_messages();
		/* Update chat window */
		show_chat(users->items[current_user].name);
	} else if (!strncmp(command[0], "help", 4)) {
		wpprintw("Available commands: chat, nick, help");
		getch();
	} else {
		wpprintw("Unknown command: %s", command[0]);
		getch();
	}

end:
	current_mode = NORMAL;
	curs_set(0);
}

void get_panel_content(int ch)
{
	if (current_mode != COMMAND && current_mode != INSERT)
		return;

	if (ch == KEY_BACKSPACE || ch == 127) {
		if (curs_pos > 0) {
			curs_pos--;
			content[curs_pos] = '\0';
		}
	}
	if (ch == ENTER) {
		if (current_mode == INSERT && current_window == CHAT_WINDOW) {
			content[curs_pos++] = '\0';
			uint8_t *recipient = users->items[current_user].name;
			send_message(recipient, content, sockfd);
			
			/* Save message to database */
			save_message(current_config->public_key, recipient, content, time(NULL));
			show_chat(recipient);
		} else if (current_mode == COMMAND) {
			content[curs_pos++] = '\0';
			wpprintw("%s", content);
			use_command();
		}
		reset_content();

	} else if (curs_pos < MAX_MESSAGE_LENGTH - 1) {
		/* Append it to the content if it is normal character */
		/* Filter readable ASCII */
		if (ch > 31 && ch < 127) {
			content[curs_pos++] = ch;
			content[curs_pos] = '\0';
		}
	}

	/* Display the current content */
	update_panel();
}

void draw_status_bar(void)
{
	wclear(status_bar);
	wattron(status_bar, A_REVERSE);
	wattron(status_bar, A_BOLD);

	switch (current_mode) {
		case NORMAL:
			wattron(status_bar, COLOR_PAIR(BLUE));
			wprintw(status_bar, " NORMAL ");
			break;

		case INSERT:
			wattron(status_bar, COLOR_PAIR(GREEN));
			wprintw(status_bar, " INSERT ");
			break;

		case COMMAND:
			wattron(status_bar, COLOR_PAIR(PEACH));
			wprintw(status_bar, " COMMAND ");
			break;
	}

	wattroff(status_bar, A_BOLD);
	wattroff(status_bar, A_REVERSE);
	switch (current_mode) {
		case NORMAL:
			wattron(status_bar, COLOR_PAIR(BLUE + 9));
			break;

		case INSERT:
			wattron(status_bar, COLOR_PAIR(GREEN + 9));
			break;

		case COMMAND:
			wattron(status_bar, COLOR_PAIR(PEACH + 9));
			break;
	}

	wprintw(status_bar, " %s ", current_config->public_key);

	wrefresh(status_bar);
	move_cursor();
}

/*
 * Main loop of user interface
 */
void ui(int fd, config_t *config)
{
	signal(SIGPIPE, signal_handler);
	signal(SIGABRT, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	srand(time(NULL));

	ncurses_init();
	windows_init();

	current_config = config;
	users = arraylist_init(LINES);
	marked = arraylist_init(100);

	sqlite_init();
	get_users();
	draw_users();

	refresh();
	sockfd = fd;
	while (1) {
		draw_status_bar();
		int ch = getch();
		switch (ch) {
			case 'q':
				if (current_mode == NORMAL) {
					deinit();
					return;
				}
				break;

			case ESC:
				reset_content();
				current_mode = NORMAL;
				current_window = USERS_WINDOW;
				draw_border(users_border, true);
				draw_border(chat_border, false);
				/* Set cursor to invisible */
				curs_set(0);
				/* Automatically change it back to normal mode */
				update_panel();
				break;

			case '/':
				if (current_mode == NORMAL) {
					current_mode = COMMAND;
					update_panel();
				} else {	
					get_panel_content(ch);
				}
				break;

			case 'i':
				if (current_mode == NORMAL) {
					current_mode = INSERT;
					current_window = CHAT_WINDOW;
					draw_border(chat_border, true);
					draw_border(users_border, false);
					update_panel();
				} else {
					get_panel_content(ch);
				}
				break;

				/* go up by k or up arrow */
			case 'k':
				if (current_mode == NORMAL && current_window == USERS_WINDOW) {
					if (current_user > 0)
						current_user--;
					draw_users();	
				} else {
					get_panel_content(ch);
				}
				break;

				/* go down by j or down arrow */
			case 'j':
				if (current_mode == NORMAL && current_window == USERS_WINDOW) {
					if (current_user < (users->length - 1))
						current_user++;
					draw_users();
				} else {
					get_panel_content(ch);
				}
				break;

			case CLEAR_INPUT:
				if (current_window == CHAT_WINDOW) {
					reset_content();
				}
				break;

			default:
				get_panel_content(ch);
		}
	}
	deinit();
	return;
}
