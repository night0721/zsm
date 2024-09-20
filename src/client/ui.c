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
long current_selection = 0;
int current_window = 0;
int sockfd;
bool show_icons;

/* For tracking cursor position in content */
static int curs_pos = 0;
static char content[MAX_MESSAGE_LENGTH];

void send_message();

void signal_handler(int signal)
{
    switch (signal) {
        case SIGPIPE:
            error(0, "SIGPIPE received");
            break;
        case SIGABRT:
        case SIGINT:
        case SIGTERM:
			shutdown(sockfd, SHUT_WR);
			endwin();
			close(sockfd);
            error(1, "Shutdown signal received");
            break;
    }
}

void ncurses_init()
{
    /* check if it is interactive shell */
    if (!isatty(STDIN_FILENO)) {
        error(1, "No tty detected. zsm requires an interactive shell to run");
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
    init_pair(3, COLOR_GREEN, -1);      /*  */
    init_pair(4, COLOR_YELLOW, -1);     /*  */
    init_pair(5, COLOR_BLUE, -1);       /*  */ 
    init_pair(6, COLOR_MAGENTA, -1);    /*  */
    init_pair(7, COLOR_CYAN, -1);       /*  */
    init_pair(8, COLOR_WHITE, -1);      /*  */
}

/*
 * Draw windows
 */
void windows_init()
{
    int users_width = 32;
    int chat_width = COLS - 32;

    /*------------------------------+
    |-----border----||---border----||
    ||              ||             ||
    || content      || content     ||
    || (users)      ||  (chat)     ||
    ||              ||-------------||
    |---------------||-textbox-----||
    +==========panel===============*/
    
    /*                     lines,								  cols,            y,						 x             */
    panel =         newwin(PANEL_HEIGHT,						  COLS,            LINES - PANEL_HEIGHT,	 0              );
    users_border =  newwin(LINES - PANEL_HEIGHT,				  users_width + 2, 0,						 0              );
    chat_border =   newwin(LINES - PANEL_HEIGHT - TEXTBOX_HEIGHT, chat_width - 2,  0,						 users_width + 2);
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
 * Draw the border of the window depending if it's active or not,
 */
void draw_border(WINDOW *window, bool active)
{
    int width;
    if (window == users_border) {
        width = 34;
    } else {
        width = COLS - 34;
    }

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
void highlight_current_line()
{
    long overflow = 0;
    if (current_selection > LINES - 4) {
        /* overflown */
        overflow = current_selection - (LINES - 4);
    }

    /* calculate range of files to show */
    long range = users->length;
    /* not highlight if no files in directory */
    if (range == 0 && errno == 0) {
        #if DRAW_PREVIEW
            wprintw(chat_content, "No users. Start a converstation.");
            wrefresh(chat_content);
        #endif
        return;
    }

    if (range > LINES - 3) {
        /* if there are more files than lines available to display
         * shrink range to avaiable lines to display with
         * overflow to keep the number of iterations to be constant */
        range = LINES - 3 + overflow;
    }
    
    wclear(users_content);
    long line_count = 0;
    for (long i = overflow; i < range; i++) {
        if ((overflow == 0 && i == current_selection) || (overflow != 0 && i == current_selection)) {
            wattron(users_content, A_REVERSE);

            /* check for marked user */
            long num_marked = marked->length;
            if (num_marked > 0) {
                /* Determine length of formatted string */
                int m_len = snprintf(NULL, 0, "[%ld] selected", num_marked);
                char *selected = memalloc(m_len + 1);

                snprintf(selected, m_len + 1, "[%ld] selected", num_marked);
                wpprintw("(%ld/%ld) %s", current_selection + 1, users->length, selected);
            } else  {
                wpprintw("(%ld/%ld)", current_selection + 1, users->length);
            }
        }
        /* print the actual filename and stats */
        char *line = get_line(users, i, show_icons);
        int color = users->items[i].color;
        /* check is user marked for action */
        bool is_marked = arraylist_search(marked, users->items[i].name) != -1;
        if (is_marked) {
            /* show user is selected */
            wattron(users_content, COLOR_PAIR(7));
        } else {
            /* print the whole directory with default colors */
            wattron(users_content, COLOR_PAIR(color));
        }
        
        if (overflow > 0)
            mvwprintw(users_content, line_count, 0, "%s", line);
		else
            mvwprintw(users_content, i, 0, "%s", line);

        if (is_marked) {
            wattroff(users_content, COLOR_PAIR(7));
		} else {
            wattroff(users_content, COLOR_PAIR(color));
        }

        wattroff(users_content, A_REVERSE);
        //free(line);
        line_count++;
    }

    wrefresh(users_content);
    wrefresh(panel);
    /* show chat conversation every time cursor changes */
    show_chat(users->items[current_selection].name);
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
void print_message(int flag, message_t *msg, int user_color)
{
    struct tm *timeinfo = localtime(&msg->creation);
    char buffer[21];
	if (flag) {
		strftime(buffer, sizeof(buffer), "%b %d %Y %H:%M:%S", timeinfo);
	} else {
		strftime(buffer, sizeof(buffer), "%H:%M:%S", timeinfo);
	}
	wprintw(chat_content, "%s ", buffer);

	wattron(chat_content, A_BOLD);
	wattron(chat_content, COLOR_PAIR(user_color));
	wprintw(chat_content, "<%s> ", msg->author);
	wattroff(chat_content, A_BOLD);
	wattroff(chat_content, COLOR_PAIR(user_color));

    int i = 0;
    int n = strlen(msg->content);
    int in_bold = 0, in_italic = 0, in_underline = 0, in_block = 0;

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
                int color_pair = msg->content[i] - '0';
                wattron(chat_content, COLOR_PAIR(color_pair));
				/* Skip the color code character */
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
			print_message(print_date, &message, 1);
			continue;
		}
			
		if (strncmp(message.author, recipient, MAX_NAME) == 0 &&
				strncmp(message.recipient, USERNAME, MAX_NAME) == 0) {
			print_message(print_date, &message, 2);
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
	wchar_t *icon_str = memalloc(2 * sizeof(wchar_t));
	wcsncpy(icon_str, L"", 2);

	arraylist_add(users, username, icon_str, 7, false, false);
}

void get_chatbox_content(int ch)
{
    if (ch == KEY_BACKSPACE || ch == 127) {
        if (curs_pos > 0) {
            curs_pos--;
            content[curs_pos] = '\0';
        }
    }
	/* Input done */
    else if (ch == '\n') {
		content[curs_pos++] = ch;
        content[curs_pos++] = '\0';
		send_message();
		/* Reset for new input */
        curs_pos = 0; 

		/* Set content[0] for printing purposes */
		content[0] = '\0';
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
	uint8_t *recipient = users->items[current_selection].name;

	keypair_t *kp_from = get_keypair(USERNAME);
	keypair_t *kp_to = get_keypair(recipient);

	int status = ZSM_STA_SUCCESS;

	uint8_t shared_key[SHARED_KEY_SIZE];
	if (crypto_kx_client_session_keys(shared_key, NULL, kp_from->pk.raw,
				kp_from->sk.raw, kp_to->pk.raw) != 0) {
		/* Recipient public key is suspicious */
		error(0, "Error performing key exchange");
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

	/* Construct data */
    memcpy(data, kp_from->sk.username, MAX_NAME);
    memcpy(data + MAX_NAME, kp_to->sk.username, MAX_NAME);
    memcpy(data + MAX_NAME * 2, nonce, NONCE_SIZE);
    memcpy(data + MAX_NAME * 2 + NONCE_SIZE, encrypted, cipher_len);

	uint8_t *signature = create_signature(data, data_len, &kp_from->sk);
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
void ui(int fd)
{
	signal(SIGPIPE, signal_handler);
    signal(SIGABRT, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	sockfd = fd;
    ncurses_init();
    windows_init();
    users = arraylist_init(LINES);
    marked = arraylist_init(100);
	show_icons = true;
	sqlite_init();
	highlight_current_line();
	refresh();
    int ch;
    while (1) {
		/*
        if (COLS < 80 || LINES < 24) {
            endwin();
            error(1, "Terminal size needs to be at least 80x24");
        }
		*/
		if (current_window == CHAT_WINDOW) {
			wclear(textbox);
			mvwprintw(textbox, 0, 0, "> %s", content);
			wrefresh(textbox);
			wmove(textbox, 0, curs_pos + 2);
			curs_set(2);
		} else {
			curs_set(0);
		}
		ch = getch();
		switch (ch) {
			case CTRLD:
				goto cleanup;

			/* go up by k or up arrow */
            case UP:
				if (current_window == USERS_WINDOW) {
					if (current_selection > 0)
						current_selection--;

					highlight_current_line();
				}
                break;

			 /* go down by j or down arrow */
            case DOWN:
				if (current_window == USERS_WINDOW) {
					if (current_selection < (users->length - 1))
						current_selection++;

					highlight_current_line();
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
			default:
				if (current_window == CHAT_WINDOW)
					get_chatbox_content(ch);

		}
    }

cleanup:
	arraylist_free(users);
	arraylist_free(marked);
	endwin();
	return;
}
