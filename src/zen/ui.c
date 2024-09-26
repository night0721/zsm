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
message_t messages[100];
int num_messages = 0;
long current_user = 0;
int current_window = 0;
int current_mode;
int sockfd;

/* For tracking cursor position in content */
static int curs_pos = 0;
static char content[MAX_MESSAGE_LENGTH];

/*
 * Free and close everything
 */
void deinit()
{
	shutdown(sockfd, SHUT_WR);
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
			deinit();
			error(1, "Shutdown signal received");
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
void ncurses_init()
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
    init_pair(1, COLOR_BLACK, -1);      /*  */
    init_pair(2, COLOR_RED, -1);        /*  */
    init_pair(3, COLOR_GREEN, -1);      /* active window */
    init_pair(4, COLOR_YELLOW, -1);     /*  */
    init_pair(5, COLOR_BLUE, -1);       /* inactive window */ 
    init_pair(6, COLOR_MAGENTA, -1);    /*  */
    init_pair(7, COLOR_CYAN, -1);       /* Selected user */
    init_pair(8, COLOR_WHITE, -1);      /*  */

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
void windows_init()
{
    int users_width = MAX_NAME / 2;
    int chat_width = COLS - (MAX_NAME / 2);

    /*----border----||---border-----+
    ||              ||             ||
    || content      || content     ||
    || (users)      ||  (chat)     ||
    ||              ||             ||
    ||              ||             ||
    ||----border----||---border----||
    ==========status bar=============
    +==========panel===============*/
    
    /*                     lines,									 cols,            y,										x             */
    panel =         newwin(PANEL_HEIGHT,							 COLS,            LINES - PANEL_HEIGHT,						0              );
	status_bar =	newwin(STATUS_BAR_HEIGHT,						 COLS,			  LINES - PANEL_HEIGHT - STATUS_BAR_HEIGHT, 0			   );
    users_border =  newwin(LINES - PANEL_HEIGHT - STATUS_BAR_HEIGHT, users_width + 2, 0,										0              );
    chat_border =   newwin(LINES - PANEL_HEIGHT - STATUS_BAR_HEIGHT, chat_width - 2,  0,										users_width + 2);

    /*									 lines,										   cols,            y,                    x             */
    users_content = subwin(users_border, LINES - PANEL_HEIGHT - 2 - STATUS_BAR_HEIGHT, users_width,     1,                    1              );
    chat_content =  subwin(chat_border,  LINES - PANEL_HEIGHT - 2 - STATUS_BAR_HEIGHT, chat_width - 4,  1,                    users_width + 3);
    
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
void draw_users()
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
                char *selected = memalloc(m_len + 1);

                snprintf(selected, m_len + 1, "[%ld] selected", num_marked);
                wpprintw("(%ld/%ld) %s", current_user + 1, users->length, selected);
            } else  {
                wpprintw("(%ld/%ld)", current_user + 1, users->length);
            }
        }
        /* print the actual filename and stats */
        user seluser = users->items[i];
		size_t name_len = strlen(seluser.name);

		/* If length of name is longer than half of allowed size in window,
		 * trim it to end with .. to show the it is too long to be displayed
		 */
		int too_long = 0;
		if (name_len > MAX_NAME / 2) {
			name_len = MAX_NAME / 2;
			too_long = 1;
		}

		char line[name_len];
		if (too_long) {
			strncpy(line, seluser.name, (MAX_NAME / 2) - 2);
			strncat(line, "..", 2);
		} else {
			strcpy(line, seluser.name);
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

void add_message(uint8_t *author, uint8_t *recipient, uint8_t *content, uint32_t length, time_t creation)
{
	message_t *msg = &messages[num_messages];
	strcpy(msg->author, author);
	strcpy(msg->recipient, recipient);
	msg->content = memalloc(length);
	strcpy(msg->content, content);
	msg->creation = creation;
	num_messages++;
}

/*
 * Add message to chat window
 * if flag is 1, print date as well
 * user_color is the color defined above at ncurses_init
 */
void print_message(int flag, message_t *msg)
{
    struct tm *timeinfo = localtime(&msg->creation);
    char timestr[21];
	if (flag) {
		strftime(timestr, sizeof(timestr), "%b %d %Y %H:%M:%S", timeinfo);
	} else {
		strftime(timestr, sizeof(timestr), "%H:%M:%S", timeinfo);
	}
	wprintw(chat_content, "%s ", timestr);

	wattron(chat_content, A_BOLD);
	int user_color = get_user_color(users, msg->author);
	wattron(chat_content, COLOR_PAIR(user_color));
	wprintw(chat_content, "<%s> ", msg->author);
	wattroff(chat_content, A_BOLD);
	wattroff(chat_content, COLOR_PAIR(user_color));

    int i = 0;
    int n = strlen(msg->content);
    int in_bold = 0, in_italic = 0, in_underline = 0, in_block = 0;
    int last_active_color = -1;

    while (i < n) {
        /* Bold */
        if (msg->content[i] == '*' && msg->content[i + 1] == '*') {
            if (!in_bold) {
                /* Look ahead for the matching closing delimiter */
				int closing_pos = i + 2;
				while (closing_pos < n && !(msg->content[closing_pos] == '*' && msg->content[closing_pos + 1] == '*')) {
					closing_pos++;
				}
				if (closing_pos < n) {
                    wattron(chat_content, A_BOLD);
                    in_bold = 1;
				} else {
					/* Treat as regular text if closing delimiter */
					waddch(chat_content, msg->content[i++]);
				}
			} else {
				wattroff(chat_content, A_BOLD);
                in_bold = 0;
			}
			/* Skip */
			i += 2;

		/* Italic */
        } else if (msg->content[i] == '*') {
            if (!in_italic) {
                /* Look ahead for the matching closing delimiter */
                int closing_pos = i + 1;
                while (closing_pos < n && msg->content[closing_pos] != '*') {
                    closing_pos++;
                }
                if (closing_pos < n) {
                    wattron(chat_content, A_ITALIC);
                    in_italic = 1;
                } else {
					/* Treat as regular text if closing delimiter */
                    waddch(chat_content, msg->content[i++]);
                }
            } else {
                wattroff(chat_content, A_ITALIC);
                in_italic = 0;
            }
			/* Skip */
			i += 1;

		/* Underline */
        } else if (msg->content[i] == '_') {
            if (!in_underline) {
                /* Look ahead for the matching closing delimiter */
                int closing_pos = i + 1;
                while (closing_pos < n && msg->content[closing_pos] != '_') {
                    closing_pos++;
                }
                if (closing_pos < n) {
                    wattron(chat_content, A_UNDERLINE);
                    in_underline = 1;
                } else {
					/* Treat as regular text if closing delimiter */
                    waddch(chat_content, msg->content[i++]);
                }
            } else {
                wattroff(chat_content, A_UNDERLINE);
                in_underline = 0;
            }
			/* Skip */
			i += 1;

		/* Block */
        } else if (msg->content[i] == '`') {
            if (!in_block) {
                /* Look ahead for the matching closing delimiter */
                int closing_pos = i + 1;
                while (closing_pos < n && msg->content[closing_pos] != '`') {
                    closing_pos++;
                }
                if (closing_pos < n) {
                    wattron(chat_content, A_STANDOUT);
                    in_block = 1;
                } else {
					/* Treat as regular text if closing delimiter */
                    waddch(chat_content, msg->content[i++]);
                }
            } else {
                wattroff(chat_content, A_STANDOUT);
                in_block = 0;
            }
			/* Skip */
			i += 1; 

		/* Allow escape sequence for genuine backslash */
		} else if (msg->content[i] == '\\' && msg->content[i + 1] == '\\') {
			/* Print a literal backslash */
            waddch(chat_content, '\\');
			/* Skip both backslashes */
            i += 2;

		/* Color, new line and tab */
        } else if (msg->content[i] == '\\') {
			/* Skip the backslash and check the next character */
            i++;
            /* Handle color codes \1 to \8 */
            if (msg->content[i] >= '1' && msg->content[i] <= '8') {
				/* Convert char to int */
                int new_color = msg->content[i] - '0';
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
            } else if (msg->content[i] == 'n') {
                waddch(chat_content, '\n');
				/* Skip the 'n' */
                i++;

            } else {
                /* Invalid sequence, just print the backslash and character */
                waddch(chat_content, '\\');
                waddch(chat_content, msg->content[i]);
                i++;
            }
		} else {
            /* Print regular character */ 
            waddch(chat_content, msg->content[i]);
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
}

void move_cursor()
{
	wmove(panel, 0, current_mode == INSERT ? curs_pos + 2 : curs_pos + 1);
	wrefresh(panel);
}

/*
 * Get chat conversation into buffer and show it to chat window
 */
void show_chat(uint8_t *recipient)
{
	wclear(chat_content);
	for (int i = 0; i < 100; i++) {
		message_t message = messages[i];
		if (message.content == NULL) continue;
		/* Find messages from recipient to client or vice versa */
		/* outgoing = 1, incoming = 2 */
		/* if message to print is older than previous message by a day,
		 * enable flag in print_message to include date */
		int print_date = 0;
		if (i > 0 && messages[i - 1].content != NULL && message.creation >= messages[i - 1].creation + 86400) {
			print_date = 1;
		}
		if (strncmp(message.author, USERNAME, MAX_NAME) == 0 &&
				strncmp(message.recipient, recipient, MAX_NAME) == 0) {
			print_message(print_date, &message);
			continue;
		}
			
		if (strncmp(message.author, recipient, MAX_NAME) == 0 &&
				strncmp(message.recipient, USERNAME, MAX_NAME) == 0) {
			print_message(print_date, &message);
			continue;
		}
	}
	wrefresh(chat_content);
	/* after printing move cursor back to panel */
	move_cursor();
}

void add_username(char *username)
{
	int randomco = rand() % 17;
	arraylist_add(users, username, randomco, false, false);
}

void reset_content()
{
	/* Reset for new input */
	curs_pos = 0; 
	/* Set content[0] for printing purposes */
	content[0] = '\0';
}

void update_panel()
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
	uint8_t *pk_to = get_pk_from_ks(recipient); /* ed25519 */

	uint8_t *shared_key = get_sharedkey(recipient);
	if (shared_key == NULL) {
		uint8_t *shared_key = memalloc(SHARED_KEY_SIZE);
		
		/* Key exchange need to be done with x25519 public and secret keys */
		uint8_t from_pk[PK_X25519_SIZE];
		uint8_t to_pk[PK_X25519_SIZE];
		uint8_t from_sk[SK_X25519_SIZE];
		if (crypto_sign_ed25519_pk_to_curve25519(from_pk, kp_from->pk.raw) != 0
				) {
			error(1, "Error converting ED25519 PK to X25519 PK");
		}
		if (crypto_sign_ed25519_pk_to_curve25519(to_pk, pk_to) != 0) {
			error(1, "Error converting ED25519 PK to X25519 PK");
		}
		if (crypto_sign_ed25519_sk_to_curve25519(from_sk, kp_from->sk) != 0) {
			error(1, "Error converting ED25519 SK to X25519 SK");
		}
		
		if (crypto_kx_client_session_keys(shared_key, NULL, from_pk, from_sk,
					to_pk) != 0) {
			deinit();
			/* Recipient public key is suspicious */
			error(1, "Error performing key exchange with %s", recipient);
		}
		save_sharedkey(recipient, shared_key);
	}
	return shared_key;
}

void send_message()
{
	keypair_t *kp_from = get_keypair(USERNAME);
	uint8_t *recipient = users->items[current_user].name;
	uint8_t *shared_key = client_kx(kp_from, recipient);

	size_t content_len = strlen(content);

    uint32_t cipher_len = content_len + ADDITIONAL_SIZE;
    uint8_t nonce[NONCE_SIZE], encrypted[cipher_len];
    
    /* Generate random nonce(number used once) */
    randombytes_buf(nonce, sizeof(nonce));
	
	/* Encrypt the content and store it to encrypted, should be cipher_len */
	
	crypto_aead_xchacha20poly1305_ietf_encrypt(encrypted, NULL, content,
			content_len, NULL, 0, NULL, nonce, shared_key);

    size_t data_len = MAX_NAME * 2 + NONCE_SIZE + cipher_len;
    uint8_t *data = memalloc(data_len);

	uint8_t recipient_padded[MAX_NAME];
	strcpy(recipient_padded, recipient);
    size_t length = strlen(recipient);
    if (length < MAX_NAME) {
        /* Pad with null characters up to max length */
		memset(recipient_padded + length, 0, MAX_NAME - length);
	} else {
		free(shared_key);
		deinit();
		error(1, "Recipient username must be shorter than MAX_NAME");
	}

	/* Construct data */
    memcpy(data, kp_from->pk.username, MAX_NAME);
    memcpy(data + MAX_NAME, recipient, MAX_NAME);
    memcpy(data + MAX_NAME * 2, nonce, NONCE_SIZE);
    memcpy(data + MAX_NAME * 2 + NONCE_SIZE, encrypted, cipher_len);

	uint8_t *signature = create_signature(data, data_len, kp_from->sk);
	packet_t *pkt = create_packet(1, ZSM_TYP_MESSAGE, data_len, data, signature);

	if (send_packet(pkt, sockfd) != ZSM_STA_SUCCESS) {
		close(sockfd);
		write_log(LOG_ERROR, "Failed to send message");
	}
	add_message(USERNAME, recipient, content, content_len, time(NULL));
	free_packet(pkt);
	show_chat(recipient);
	free(shared_key);
}

void use_command()
{
	/* Parse slash command */
	char *token = strtok(content, " ");
	char *command[MAX_ARGS];
	int args = 0;
	while (token != NULL) {
		command[args] = token;
		token = strtok(NULL, " ");
		args++;
	}

	if (strncmp(command[0], "chat", 4) == 0) {
		keypair_t *kp_from = get_keypair(USERNAME);
		client_kx(kp_from, command[1]);
		current_user = arraylist_search(users, command[1]);
		if (current_user == -1) {
			/* If user is not in runtime user list as it is new to database
			 * add it to runtime user list
			 * as latest added would be at the end so index would be length -1
			 */
			add_username(command[1]);
			current_user = users->length - 1;
		}
		draw_users();
		show_chat(command[1]);
	}

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
			content[curs_pos++] = ch;
			content[curs_pos++] = '\0';
			send_message();
		} else if (current_mode == COMMAND) {
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

void draw_status_bar()
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
	wprintw(status_bar, " %s ", USERNAME);


	wrefresh(status_bar);
	move_cursor();
}

/*
 * Main loop of user interface
 */
void ui(int *fd)
{
	signal(SIGPIPE, signal_handler);
    signal(SIGABRT, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	srand(time(NULL));

    ncurses_init();
    windows_init();

	users = arraylist_init(LINES);
	marked = arraylist_init(100);

	sqlite_init();
	get_users();
	draw_users();

	refresh();
	sockfd = *fd;
    while (1) {
		draw_status_bar();
		int ch = getch();
		switch (ch) {
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
