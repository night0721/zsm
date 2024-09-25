#include "config.h"
#include "packet.h"
#include "util.h"
#include "client/ui.h"
#include "client/db.h"
#include "client/user.h"

WINDOW *panel;
WINDOW *users_border;
WINDOW *chat_border;
WINDOW *users_content;
WINDOW *textbox;
WINDOW *chat_content;

ArrayList *users;
ArrayList *marked;
message_t messages[100];
int num_messages = 0;
long current_user = 0;
int current_window = 0;
int sockfd;

/* For tracking cursor position in content */
static int curs_pos = 0;
static char content[MAX_MESSAGE_LENGTH];

void send_message();

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
 * Start ncurses
 */
void ncurses_init()
{
    /* check if it is interactive shell */
    if (!isatty(STDIN_FILENO)) {
        error(1, "No tty detected. zen requires an interactive shell to run");
    }

    /* initialize screen, don't print special chars,
     * make ctrl + c work, don't show cursor 
     * enable arrow keys */
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
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
}

/*
 * Draw windows
 */
void windows_init()
{
    int users_width = MAX_NAME / 2;
    int chat_width = COLS - (MAX_NAME / 2);

    /*------------------------------+
    |-----border----||---border----||
    ||              ||             ||
    || content      || content     ||
    || (users)      ||  (chat)     ||
    ||              ||-------------||
    |---------------||-textbox-----||
    +==========panel===============*/
    
    /*                     lines,								  cols,            y,									  x             */
    panel =         newwin(PANEL_HEIGHT,						  COLS,            LINES - PANEL_HEIGHT,				  0              );
    users_border =  newwin(LINES - PANEL_HEIGHT,				  users_width + 2, 0,									  0              );
    chat_border =   newwin(LINES - PANEL_HEIGHT - TEXTBOX_HEIGHT, chat_width - 2,  0,									  users_width + 2);
	textbox =		newwin(TEXTBOX_HEIGHT,						  chat_width - 2,  LINES - PANEL_HEIGHT - TEXTBOX_HEIGHT, users_width + 3);

    /*									 lines,										cols,            y,                    x             */
    users_content = subwin(users_border, LINES - PANEL_HEIGHT - 2,					users_width,     1,                    1              );
    chat_content =  subwin(chat_border,  LINES - PANEL_HEIGHT - 2 - TEXTBOX_HEIGHT, chat_width - 4,  1,                    users_width + 3);
    
	/* draw border around windows */
	refresh();
    draw_border(users_border, true);
    draw_border(chat_border, false);

	scrollok(textbox, true);
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
    wrefresh(window);  /* Refresh the window to see the colored border and title */
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
    wrefresh(chat_content);
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
	/* after printing move cursor back to textbox */
	wmove(textbox, 0, curs_pos + 2);
	wrefresh(textbox);
}
/*
 * Require heap allocated username
 */
void add_username(char *username)
{
	int randomco = rand() % 8;
	arraylist_add(users, username, randomco, false, false);
}

void get_chatbox_content(int ch)
{
    if (ch == KEY_BACKSPACE || ch == 127) {
        if (curs_pos > 0) {
            curs_pos--;
            content[curs_pos] = '\0';
        }
    }
    /* Append it to the content if it is normal character */
    else if (curs_pos < MAX_MESSAGE_LENGTH - 1) {
		/* Filter readable ASCII */
		if (ch > 31 && ch < 127) {
			content[curs_pos++] = ch;
			content[curs_pos] = '\0';
		}
    }

    /* Display the current content */
	mvwprintw(textbox, 0, 0, "> %s", content);
	wrefresh(textbox);
}

void send_message()
{
	uint8_t *recipient = users->items[current_user].name;

	keypair_t *kp_from = get_keypair(USERNAME);
	uint8_t *pk_to = get_pk_from_ks(recipient); /* ed25519 */

	int status = ZSM_STA_SUCCESS;

	uint8_t *shared_key = get_sharedkey(recipient);
	if (shared_key == NULL) {
		uint8_t shared_key[SHARED_KEY_SIZE];
		
		/* Key exchange need to be done with x25519 public and secret keys */
		uint8_t from_pk[PK_X25519_SIZE];
		uint8_t to_pk[PK_X25519_SIZE];
		uint8_t from_sk[SK_X25519_SIZE];
		crypto_sign_ed25519_pk_to_curve25519(from_pk, kp_from->pk.raw);
		crypto_sign_ed25519_pk_to_curve25519(to_pk, pk_to);
		crypto_sign_ed25519_sk_to_curve25519(from_sk, kp_from->sk);
		

		if (crypto_kx_client_session_keys(shared_key, NULL, from_pk, from_sk,
					to_pk) != 0) {
			deinit();
			/* Recipient public key is suspicious */
			error(1, "Error performing key exchange with %s", recipient);
		}
		save_sharedkey(recipient, shared_key);
	}

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
    ncurses_init();
    windows_init();
	sockfd = *fd;
    users = arraylist_init(LINES);
    marked = arraylist_init(100);
	sqlite_init();
	get_users();
	draw_users();
	refresh();
    while (1) {
		if (current_window == CHAT_WINDOW) {
			wclear(textbox);
			mvwprintw(textbox, 0, 0, "> %s", content);
			wrefresh(textbox);
			wmove(textbox, 0, curs_pos + 2);
			/* Set cursor to visible */
			curs_set(2);
		} else {
			/* Set cursor to invisible */
			curs_set(0);
		}
		int ch = getch();
		switch (ch) {
			/* go up by k or up arrow */
            case UP:
				if (current_window == USERS_WINDOW) {
					if (current_user > 0)
						current_user--;

					draw_users();
				}
                break;

			 /* go down by j or down arrow */
            case DOWN:
				if (current_window == USERS_WINDOW) {
					if (current_user < (users->length - 1))
						current_user++;

					draw_users();
				}
                break;

			/* A is normally for left and E for right */
			case CTRLA:
			case CTRLE:
				current_window ^= 1;
				if (current_window == USERS_WINDOW) {
					draw_border(users_border, true);
					draw_border(chat_border, false);
				} else {
					draw_border(chat_border, true);
					draw_border(users_border, false);
				}
				break;

			case CLEAR_INPUT:
				if (current_window == CHAT_WINDOW) {
					curs_pos = 0;
					content[0] = '\0';
				}

			case ENTER:
				if (current_window == CHAT_WINDOW) {
					content[curs_pos++] = ch;
					content[curs_pos++] = '\0';
					send_message();
					/* Reset for new input */
					curs_pos = 0; 

					/* Set content[0] for printing purposes */
					content[0] = '\0';
				}
				break;


			default:
				if (current_window == CHAT_WINDOW)
					get_chatbox_content(ch);

		}
    }
	deinit();
	return;
}
