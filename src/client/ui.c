#include "config.h"
#include "packet.h"
#include "util.h"
#include "client/ui.h"
#include "client/db.h"
#include "client/user.h"

typedef struct windows {
    WINDOW *users_border;
    WINDOW *users_content;
    WINDOW *chat_border;
    WINDOW *chat_content;
} windows;

WINDOW *panel;
WINDOW *users_border;
WINDOW *chat_border;

WINDOW *users_content;
WINDOW *chat_content;

ArrayList *users;
ArrayList *marked;
long current_selection = 0;

bool show_icons;

void show_chat();

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
    curs_set(0);
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
    |----border(0)--||--border(2)--||
    ||              ||             ||
    || content (1)  || content (3) ||
    || (users)      ||  (chat)     ||
    ||              ||             ||
    |---------------||-------------||
    +==========panel (4)===========*/
    
    /*                     lines,                    cols,            y,                    x             */
    panel =         newwin(PANEL_HEIGHT,             COLS,            LINES - PANEL_HEIGHT, 0              );
    /* draw border around windows */
    users_border =  newwin(LINES - PANEL_HEIGHT,     users_width + 2, 0,                    0              );
    chat_border =   newwin(LINES - PANEL_HEIGHT,     chat_width - 2,  0,                    users_width + 2);

    users_content = newwin(LINES - PANEL_HEIGHT - 2, users_width,     1,                    1              );
    chat_content =  newwin(LINES - PANEL_HEIGHT - 2, chat_width - 4,  1,                    users_width + 3);
    
    refresh();
    draw_border(users_border, true);
    draw_border(chat_border, false);

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
    /* draw top border */
    mvwaddch(window, 0, 0, ACS_ULCORNER);  /* upper left corner */
    mvwhline(window, 0, 1, ACS_HLINE, COLS - 2); /* top horizontal line */
    mvwaddch(window, 0, width - 1, ACS_URCORNER); /* upper right corner */

    /* draw side border */
    mvwvline(window, 1, 0, ACS_VLINE, LINES - 2); /* left vertical line */
    mvwvline(window, 1, width - 1, ACS_VLINE, LINES - 2); /* right vertical line */

    /* draw bottom border
     * make space for the panel */
    mvwaddch(window, LINES - PANEL_HEIGHT - 1, 0, ACS_LLCORNER); /* lower left corner */
    mvwhline(window, LINES - PANEL_HEIGHT - 1, 1, ACS_HLINE, width - 2); /* bottom horizontal line */
    mvwaddch(window, LINES - PANEL_HEIGHT - 1, width - 1, ACS_LRCORNER); /* lower right corner */

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
                char *selected = memalloc((m_len + 1) * sizeof(char));

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
    #if DRAW_PREVIEW
         show_chat();
    #endif
    #if DRAW_BORDERS
        draw_border_title(preview_border, true);
    #endif
    wrefresh(chat_content);
}

/*
 * Add message to chat window
 * user_color is the color defined above at ncurses_init
 */

void add_message(time_t rawtime, char *username, int user_color, char *content)
{
    struct tm *timeinfo = localtime(&rawtime);
    char buffer[21];
    strftime(buffer, sizeof(buffer), "%b %d %Y %H:%M:%S", timeinfo);

	wprintw(chat_content, "%s ", buffer);
	wattron(chat_content, A_BOLD);
	wattron(chat_content, COLOR_PAIR(user_color));

	wprintw(chat_content, "<%s> ", username);

	wattroff(chat_content, A_BOLD);
	wattroff(chat_content, COLOR_PAIR(user_color));
	wprintw(chat_content, "%s", content);
}

/*
 * Get chat conversation into buffer and show it to chat window
 */
void show_chat()
{
	add_message(1725932011, "night", 1, "I go to school by bus.\n");
	add_message(1725933011, "night", 2, "I go to school by tram.\n");
	add_message(1725934011, "night", 3, "I go to school by train.\n");
	add_message(1725935011, "night", 4, "I go to school by car.\n");
	add_message(1725936011, "night", 5, "I go to school by run.\n");
	add_message(1725937011, "night", 6, "I go to school by bike.\n");
	add_message(1725938011, "night", 7, "I go to school by plane.\n");
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

/*
 * Main loop of user interface
 */
void ui()
{
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
        if (COLS < 80 || LINES < 24) {
            endwin();
            error(1, "Terminal size needs to be at least 80x24");
        }
		ch = getch();
		switch (ch) {
			case 'q':
				goto cleanup;

			/* go up by k or up arrow */
            case UP:
            case 'k':
                if (current_selection > 0)
                    current_selection--;

                highlight_current_line();
                break;

			 /* go down by j or down arrow */
            case DOWN:
            case 'j':
                if (current_selection < (users->length - 1))
                    current_selection++;

                highlight_current_line();
                break;
		}
    }
cleanup:
	arraylist_free(users);
	arraylist_free(marked);
	endwin();
	return;
}
