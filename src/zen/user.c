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
	long left = 0;
	long right = list->length - 1;
	long mid;

	while (left <= right) {
		mid = left + (right - left) / 2;
		if (strcmp(list->items[mid].name, username) == 0) {
			return mid;
		} else if (strcmp(list->items[mid].name, username) < 0) {
			left = mid + 1;
		} else {
			right = mid - 1;
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

void arraylist_add(ArrayList *list, uint8_t *username, uint8_t *nickname, int color)
{
	user new_user;
	strcpy(new_user.name, username);
	strcpy(new_user.nickname, nickname);
	new_user.color = color;

	if (list->capacity != list->length) {
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
 * Quick sort implementation
 */
int arraylist_partition(ArrayList *list, int left, int right)
{
	user pivot = list->items[right];
	int i = left - 1;

	for (int j = left; j < right; j++) {
		if (strcmp(list->items[j].name, pivot.name) < 0) {
			i++;
			user temp = list->items[i];
			list->items[i] = list->items[j];
			list->items[j] = temp;
		}
	}
	user temp = list->items[i + 1];
	list->items[i + 1] = list->items[right];
	list->items[right] = temp;

	return i + 1;
}

void arraylist_sort(ArrayList *list, int left, int right)
{
	if (left < right) {
		int pivot = arraylist_partition(list, left, right);
		arraylist_sort(list, left, pivot - 1);
		arraylist_sort(list, pivot + 1, right);
	}
}

int get_user_color(ArrayList *list, uint8_t *username)
{
	for (int i = 0; i < list->length; i++) {
		if (strncmp(username, list->items[i].name, MAX_NAME * 2 + 1) == 0) {
			return list->items[i].color;
		}
	}
	/* Red as default color */
	return 2;
}
