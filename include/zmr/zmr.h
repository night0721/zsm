#ifndef SERVER_H_
#define SERVER_H_

#define MAX_CONNECTION_QUEUE 128 /* for listen() */
#define MAX_EVENTS 64 /* Max events can be returned simulataneouly by epoll */

typedef struct {
	int fd; /* File descriptor for client socket */
	uint8_t *shared_key;
	char username[MAX_NAME]; /* Username of client */
} client_t;

typedef struct {
	int epoll_fd; /* epoll instance for each thread */
	pthread_t thread; /* POSIX thread */
	int num_clients; /* Number of active clients in thread */
	client_t clients[MAX_CLIENTS_PER_THREAD]; /* Active clients */
} thread_t;

#endif
