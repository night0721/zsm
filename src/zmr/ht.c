#include "ht.h"
#include "config.h"
#include "packet.h"

client *hash_table[TABLE_SIZE];

/* Hashes every name with: name and TABLE_SIZE */
unsigned int hash(char *name)
{
    int length = strnlen(name, MAX_NAME);
    unsigned int hash_value = 0;
    
    for (int i = 0; i < length; i++) {
        hash_value += name[i];
        hash_value = (hash_value * name[i]) % TABLE_SIZE;
    }

    return hash_value;
}

void hashtable_init()
{
    for (int i = 0; i < TABLE_SIZE; i++)
        hash_table[i] = NULL;
}

void hashtable_print()
{
    int i = 0;

    for (; i < TABLE_SIZE; i++) {
        if (hash_table[i] == NULL) {
            printf("%i. ---\n", i);
        } else {
            printf("%i. | Name %s\n", i, hash_table[i]->name);
        }
    }
}

/* Gets hashed name and tries to store the client struct in that place */
int hashtable_add(client *p)
{
    if (p == NULL) return 0;

    int index = hash(p->name);
    int initial_index = index;
     /* linear probing until an empty slot is found */
    while (hash_table[index] != NULL) {
        index = (index + 1) % TABLE_SIZE; /* move to next item */
        /* the hash table is full as no available index back to initial index, cannot fit new item */
        if (index == initial_index) return 1;
    }

    hash_table[index] = p;
    return 0;
}

/* Rehashes the name and then looks in this spot, if found returns client */
client *hashtable_search(char *name)
{
    int index = hash(name);
    int initial_index = index;
    
    /* Linear probing until an empty slot or the desired item is found */
    while (hash_table[index] != NULL) {
        if (strncmp(hash_table[index]->name, name, MAX_NAME) == 0)
            return hash_table[index];
        
        index = (index + 1) % TABLE_SIZE; /* Move to the next slot */
        /* back to same item */
        if (index == initial_index) break;
    }
    
    return NULL;
}
