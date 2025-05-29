#ifndef USER_H_
#define USER_H_

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint8_t name[MAX_NAME * 2 + 1];
    uint8_t nickname[MAX_NAME * 2 + 1];
    int color;
} user;

typedef struct {
    size_t length;
    size_t capacity;
    user *items;
} ArrayList;

ArrayList *arraylist_init(size_t capacity);
void arraylist_free(ArrayList *list);
long arraylist_search(ArrayList *list, uint8_t *username);
void arraylist_remove(ArrayList *list, long index);
void arraylist_add(ArrayList *list, uint8_t *username, uint8_t *nickname, int color);
int get_user_color(ArrayList *list, uint8_t *username);

#endif
