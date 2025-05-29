#ifndef HT_H_
#define HT_H_

#include "zmr/zmr.h"

#define FNV_OFFSET_BASIS 2166136261u
#define FNV_PRIME 16777619u

void hashtable_init(client_t **hash_table);
void hashtable_print(client_t **hash_table);
int hashtable_add(client_t **hash_table, client_t *p);
client_t *hashtable_search(client_t **hash_table, char *username);
int hashtable_remove(client_t **hash_table, char *username);
int hashtable_length(client_t **hash_table);
void hashtable_free(client_t **hash_table);

#endif
