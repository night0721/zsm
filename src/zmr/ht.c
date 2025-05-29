#include "zmr/ht.h"
#include "config.h"
#include "packet.h"
#include "zmr/zmr.h"

/*
 * FNV-1a Hash Function
 */
unsigned int fnv1a_hash(char *name)
{
    unsigned int hash = FNV_OFFSET_BASIS;

	int length = strnlen(name, MAX_NAME * 2 + 1);
    for (size_t i = 0; i < length; i++) {
        hash ^= (unsigned char) name[i];
        hash *= FNV_PRIME;
    }

    return hash % TABLE_SIZE;
}

/*
 * Initialize the hash table
 */
void hashtable_init(client_t **hash_table)
{
	for (int i = 0; i < TABLE_SIZE; i++)
		hash_table[i] = NULL;
}

/*
 * Print the hash table
 */
void hashtable_print(client_t **hash_table)
{
	for (int i = 0; i < TABLE_SIZE; i++) {
		if (hash_table[i] == NULL) {
			printf("%i. ---\n", i);
		} else {
			printf("%i. | Name %s\n", i, hash_table[i]->username);
		}
	}
}

/* Gets hashed name and tries to store the client struct in that place */
int hashtable_add(client_t **hash_table, client_t *p)
{
	if (p == NULL) return 0;

	int index = fnv1a_hash(p->username);
	int initial_index = index;

	 /* Linear probing until an empty slot is found */
	while (hash_table[index] != NULL) {
		index = (index + 1) % TABLE_SIZE; /* Move to next item */
		/* the hash table is full as no available index back to initial index, cannot fit new item */
		if (index == initial_index) return 1;
	}

	hash_table[index] = p;
	return 0;
}

/* Search for a client in the hash table by name */
client_t *hashtable_search(client_t **hash_table, char *username)
{
	int index = fnv1a_hash(username);
	int initial_index = index;
	
	/* Linear probing until an empty slot or the desired item is found */
	while (hash_table[index] != NULL) {
		if (strncmp(hash_table[index]->username, username, MAX_NAME * 2 + 1) == 0)
			return hash_table[index];
	
		index = (index + 1) % TABLE_SIZE; /* Move to the next slot */
		/* Back to same item */
		if (index == initial_index) break;
	}
	
	return NULL;
}

/* Remove a client from the hash table */
int hashtable_remove(client_t **hash_table, char *username)
{
    int index = fnv1a_hash(username);
    int initial_index = index;

    /* Linear probing to find the item to remove */
    while (hash_table[index] != NULL) {
        if (strncmp(hash_table[index]->username, username, MAX_NAME * 2 + 1) == 0) {
            /* Found the item, remove it */
            hash_table[index] = NULL;

            /* Rehash subsequent items */
            int next_index = (index + 1) % TABLE_SIZE;
            while (hash_table[next_index] != NULL) {
                client_t *temp = hash_table[next_index];
                hash_table[next_index] = NULL;
				/* Re-add the item */
                hashtable_add(hash_table, temp);
                next_index = (next_index + 1) % TABLE_SIZE;
            }

            return 1;
        }
 
        index = (index + 1) % TABLE_SIZE; /* Move to the next slot */
        if (index == initial_index) break; /* Back to the starting slot */
    }

    return 0; 
}

int hashtable_length(client_t **hash_table)
{
    int length = 0;
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (hash_table[i] != NULL) {
            length++;  /* Increment for each non-null item */
        }
    }
    return length;
}

void hashtable_free(client_t **hash_table)
{
	for (int i = 0; i < TABLE_SIZE; i++) {
        if (hash_table[i] != NULL) {
			free(hash_table[i]);
        }
    }
}
