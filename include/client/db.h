#ifndef DB_H_
#define DB_H_

#include <sqlite3.h>

void get_users();
uint8_t *get_sharedkey(uint8_t *username);
void save_sharedkey(uint8_t *username, uint8_t *shared_key);
void sqlite_init();

#endif
