#include "packet.h"
#include "util.h"
#include "client/user.h"

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
    for (size_t i = 0; i < list->length; i++) {
        if (list->items[i].name != NULL)
            free(list->items[i].name);
        if (list->items[i].icon != NULL)
            free(list->items[i].icon);
    }

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

    free(list->items[index].name);
    free(list->items[index].icon);

    for (long i = index; i < list->length - 1; i++)
        list->items[i] = list->items[i + 1];

    list->length--;
}

/*
 * Force will not remove duplicate marked users, instead it just skip adding
 */
void arraylist_add(ArrayList *list, uint8_t *username, wchar_t *icon, int color, bool marked, bool force)
{
    user new_user = { username, icon, color };

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

/*
 * Construct a formatted line for display
 */
char *get_line(ArrayList *list, long index, bool icons)
{
    user seluser = list->items[index];

    size_t name_len = strlen(seluser.name);
    size_t length;

    if (icons) {
		length = name_len + 10;   /* 8 for icon, 1 for space and 1 for null */
    } else {
		length = name_len;
    }

    char *line = memalloc(length);
    line[0] = '\0';
    
    if (icons) {
		char *tmp = memalloc(9);
        snprintf(tmp, 8, "%ls", seluser.icon);

        strcat(line, tmp);
        strcat(line, " ");
        free(tmp);
    }
    strcat(line, seluser.name);
 
    return line;
}
