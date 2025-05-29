#ifndef ZMR_H_
#define ZMR_H_

#include "packet.h"
#include "zmr/ht.h"

#define TABLE_SIZE (MAX_CLIENTS_PER_THREAD * 2)

#define MAX_CONNECTION_QUEUE 128 /* for listen() */
#define MAX_EVENTS 64 /* Max events can be returned simulataneouly by epoll */
#define MAX_THREADS 8
#define MAX_CLIENTS_PER_THREAD 1024

typedef struct {
	int fd; /* File descriptor for client socket */
	char username[MAX_NAME * 2 + 1]; /* Username of client */
} client_t;

typedef struct {
	int epoll_fd; /* epoll instance for each thread */
	pthread_t thread; /* POSIX thread */
	pthread_mutex_t message_lock;
	client_t *table[TABLE_SIZE]; /* Active clients */
} thread_t;

#endif
