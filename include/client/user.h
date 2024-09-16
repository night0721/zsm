#ifndef USER_H_
#define USER_H_

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct user {
    char *name;
    wchar_t *icon;
    int color;
} user;

typedef struct ArrayList {
    size_t length;
    size_t capacity;
    user *items;
} ArrayList;

ArrayList *arraylist_init(size_t capacity);
void arraylist_free(ArrayList *list);
long arraylist_search(ArrayList *list, char *username);
void arraylist_remove(ArrayList *list, long index);
void arraylist_add(ArrayList *list, char *name, wchar_t *icon, int color, bool marked, bool force);
char *get_line(ArrayList *list, long index, bool icons);

#endif
