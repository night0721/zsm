/* Server */
#define DEBUG 0
#define PORT 20247
#define MAX_NAME 32 /* Max username length */
#define DATABASE_NAME "test.db"
#define MAX_MESSAGE_LENGTH 8192

/* Don't touch unless you know what you are doing */
#define MAX_CONNECTION_QUEUE 128
#define MAX_THREADS 8
#define MAX_EVENTS 64 /* Max events can be returned simulataneouly by epoll */
#define MAX_CLIENTS_PER_THREAD 1024

/* Client */
#define DOMAIN "127.0.0.1"

/* UI */
#define PANEL_HEIGHT 1
#define DRAW_PREVIEW 1

#define CLIENT_DATA_DIR "~/.local/share/zsm/zen"

/* Keybindings */
#define DOWN 0x102
#define UP 0x103
