/* Arraylist implementation */
#include <ncurses.h>

#include "packet.h"
#include "util.h"
#include "zen/user.h"

ArrayList *arraylist_init(size_t capacity)
{
    ArrayList *list = memalloc(sizeof(ArrayList));
    list->length = 0;
    list->capacity = capacity;
    list->items = memalloc(capacity * sizeof(user));

    return list;
}

void arraylist_free(ArrayList *list)
{
    free(list->items);
    free(list);
}

/*
 * Check if the user is in the arraylist
 */
long arraylist_search(ArrayList *list, uint8_t *username)
{
    for (long i = 0; i < list->length; i++) {
        if (strcmp(list->items[i].name, username) == 0) {
            return i;
        }
    }
    return -1;
}

void arraylist_remove(ArrayList *list, long index)
{
    if (index >= list->length)
        return;

    for (long i = index; i < list->length - 1; i++)
        list->items[i] = list->items[i + 1];

    list->length--;
}

/*
 * Force will not remove duplicate marked users, instead it just skip adding
 */
void arraylist_add(ArrayList *list, uint8_t *username, int color, bool marked, bool force)
{
    user new_user;
	strcpy(new_user.name, username);
	new_user.color = color;

    if (list->capacity != list->length) {
        if (marked) {
            for (int i = 0; i < list->length; i++) {
                if (strcmp(list->items[i].name, new_user.name) == 0) {
                    if (!force)
                        arraylist_remove(list, i);
                    return;
                }
            }
        }
        list->items[list->length] = new_user;
    } else {
        int new_cap = list->capacity * 2;
        user *new_items = memalloc(new_cap * sizeof(user));
        user *old_items = list->items;
        list->capacity = new_cap;
        list->items = new_items;

        for (int i = 0; i < list->length; i++)
            new_items[i] = old_items[i];

        free(old_items);
        list->items[list->length] = new_user;
    }
    list->length++;
}

int get_user_color(ArrayList *list, uint8_t *username)
{
	for (int i = 0; i < list->length; i++) {
		if (strncmp(username, list->items[i].name, MAX_NAME) == 0) {
			return list->items[i].color;
		}
	}
	/* Red as default color */
	return COLOR_RED;
}
