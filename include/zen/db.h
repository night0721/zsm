#ifndef DB_H_
#define DB_H_

#include <sqlite3.h>

void get_users(void);
uint8_t *get_receivekey(uint8_t *username);
uint8_t *get_sendkey(uint8_t *username);
void save_receivekey(uint8_t *username, uint8_t *receive_key);
void save_sendkey(uint8_t *username, uint8_t *send_key);
void update_nickname(uint8_t *username, uint8_t *nickname);
uint8_t *get_nickname(uint8_t *username);
void save_message(uint8_t *author, uint8_t *recipient, uint8_t *message, time_t timestamp);
void get_messages(uint8_t *author, uint8_t *recipient);
void clear_messages(void);
void sqlite_init(void);

#endif
