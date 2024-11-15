#ifndef HT_H_
#define HT_H_

#define TABLE_SIZE 100

typedef struct {
    int id;
    char name[32];
    //pthread_t thread;
} client;

unsigned int hash(char *name);
void hashtable_init(void);
void hashtable_print(void);
int hashtable_add(client *p);
client *hashtable_search(char *name);

#endif
