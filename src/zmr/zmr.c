#include "packet.h"
#include "util.h"
#include "config.h"
#include "zmr/zmr.h"

thread_t threads[MAX_THREADS];
int num_thread = 0;
int debug = 0;

/*
 * Authenticate client before starting communication
 */
int authenticate_client(int clientfd, uint8_t *username)
{
	/* Create a challenge */
	uint8_t *challenge = memalloc(CHALLENGE_SIZE);
	randombytes_buf(challenge, CHALLENGE_SIZE);

	/* Sending fake signature as structure requires it */
	uint8_t *fake_sig =	create_signature(NULL, 0, NULL);

	packet_t *pkt = create_packet(1, ZSM_TYP_AUTH, CHALLENGE_SIZE, challenge,
			fake_sig);

	if (send_packet(pkt, clientfd) != ZSM_STA_SUCCESS) {
		error(0, "Could not authenticate client");
		goto failure;
	}
	free(fake_sig);

	int status;
	if ((status = recv_packet(pkt, clientfd, ZSM_TYP_AUTH)
				!= ZSM_STA_SUCCESS)) {
		error(0, "Could not authenticate client");
		goto failure;
	}

	uint8_t pk_bin[PK_ED25519_SIZE], pk_username[MAX_NAME];
	memcpy(pk_bin, pkt->data, PK_ED25519_SIZE);
	memcpy(pk_username, pkt->data + PK_ED25519_SIZE, MAX_NAME);

	if (crypto_sign_verify_detached(pkt->signature, challenge, CHALLENGE_SIZE,
				pk_bin) != 0) {
		free_packet(pkt);
		error(0, "Incorrect signature, could not authenticate client");
		goto failure;
	} else {
		pkt->status = ZSM_STA_AUTHORISED;
		pkt->type = ZSM_TYP_INFO;
		pkt->length = 0;
		pkt->data = NULL;
		pkt->signature = NULL;
		send_packet(pkt, clientfd);
		free_packet(pkt);
		strcpy(username, pk_username);
		return ZSM_STA_SUCCESS;
	}
failure:;
		/*
	packet_t *error_pkt = create_packet(ZSM_STA_UNAUTHORISED, ZSM_TYP_ERROR,
			0, NULL, create_signature(NULL, 0, NULL));
	send_packet(error_pkt, clientfd);
	free_packet(error_pkt);
	*/
	close(clientfd);
	return ZSM_STA_ERROR_AUTHENTICATE;
}

void signal_handler(int signal)
{
	switch (signal) {
		case SIGPIPE:
			error(0, "SIGPIPE received");
			break;
		case SIGABRT:
		case SIGINT:
		case SIGTERM:
			error(1, "Shutdown signal received");
			break;
	}
}

int get_clientfd(uint8_t *username)
{
	for (int i = 0; i < MAX_THREADS; i++) {
		thread_t thread = threads[i];
		for (int j = 0; j < thread.num_clients; j++) {
			client_t client = thread.clients[j];
			if (strncmp(client.username, username, MAX_NAME) == 0) {
				return client.fd;
			}
		}
	}
	return -1;
}
/*
 * Takes thread_t as argument to use its epoll instance to wait new pakcets
 * Thread worker to relay packets
 */
void *thread_worker(void *arg)
{
	thread_t *thread = (thread_t *)	arg;
	struct epoll_event events[MAX_EVENTS];
	
	while (1) {
		int num_events = epoll_wait(thread->epoll_fd, events, MAX_EVENTS, -1);
		if (num_events == -1) {
			/* TODO: pthread exit? */
			error(0, "epoll_wait");
		}
		for (int i = 0; i < num_events; i++) {
			client_t *client = (client_t *) events[i].data.ptr;

			if (events[i].events & EPOLLIN) {
				/* Handle packet */
				/* TODO: Mutex lock when handle packet */
				packet_t pkt;
				int status = verify_packet(&pkt, client->fd);
				if (debug) print_packet(&pkt);
				if (status != ZSM_STA_SUCCESS) {
					if (status == ZSM_STA_CLOSED_CONNECTION) {
						/* TODO: Remove client from thread, epollctldel, close fd */
						error(0, "Client closed connection");
					} else {
						error(0, "Error verifying packet");
					}
					continue;
				}
				
				/* Message relay */
				uint8_t to[MAX_NAME];
				memcpy(to, pkt.data + MAX_NAME, MAX_NAME);
				if (to[0] != '\0') {
					int fd = get_clientfd(to);
					/* TODO: Find via hashtable */
					if (fd != -1) {
						error(0, "Relaying packet to %s", to);
						send_packet(&pkt, fd);
					} else {
						error(0, "%s not found", to);
					}
				}
			}
		}
	}
}

int main(int argc, char **argv)
{
	if (sodium_init() < 0) {
		error(1, "Error initializing libsodium");
	}
	
	if (argc == 2 && strcmp(argv[1], "-d") == 0) {
		/* Turns on debug flag */
		debug = 1;
	}
	
	signal(SIGPIPE, signal_handler);
	signal(SIGABRT, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	/* Start server and epoll */
	int serverfd, clientfd;

	/* Create socket */
	serverfd = socket(AF_INET, SOCK_STREAM, 0);
	if (serverfd < 0) {
		error(1, "Error on opening socket");
	}

	/* Reuse address (for debug) */
	int opt = 1;
	if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		error(1, "Error at setting SO_REUSEADDR");
	}

	struct sockaddr_in server_addr, client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(PORT);

	if (bind(serverfd, (struct sockaddr *) &server_addr
				, sizeof(server_addr)) < 0) {
		close(serverfd);
		error(1, "Error on bind");
	}

	/* Creating thread pool */
	for (int i = 0; i < MAX_THREADS; i++) {
		/* Create epoll instance for each thread */
		threads[i].epoll_fd = epoll_create1(0);
		if (threads[i].epoll_fd < 0) {
			error(1, "Error on creating epoll instance");
		}
		threads[i].num_clients = 0;

		/* Start a new thread and pass thread_t struct to thread */
		if (pthread_create(&threads[i].thread, NULL, thread_worker,
					&threads[i]) != 0) {
			error(1, "Error on creating threads");
		} else {
			error(0, "Thread %d created", i);
		}
	}

	if (listen(serverfd, MAX_CONNECTION_QUEUE) < 0) {
		close(serverfd);
		error(1, "Error on listen");
	}
	
	error(0, "Listening on port %d", PORT);

	/* Server loop to accept clients and load balance */
	while (1) {
		clientfd = accept(serverfd, (struct sockaddr *) &client_addr,
				&client_addr_len);
		if (clientfd < 0) {
			error(0, "Error on accepting client");
			continue;
		}

		/* Assign new client to a thread
		 * TODO: Loop everythread to see which client is unintialised
		 * TODO: Use mutex before add client
		 * Clients distributed by a rotation(round-robin)
		 */
		thread_t *thread = &threads[num_thread];
		if (thread->num_clients >= MAX_CLIENTS_PER_THREAD) {
			error(0, "Thread %d is already full, rejecting connection\n",
					num_thread);
			close(clientfd);
			continue;
		}

		client_t *client = &thread->clients[thread->num_clients];
		
		uint8_t username[MAX_NAME];
		/* User logins, authenticate them */
		if (authenticate_client(clientfd, username) != ZSM_STA_SUCCESS) {
			error(0, "Error authenticating with client");
			continue;
		}

		/* Add the new client to the thread's epoll instance */
		struct epoll_event event;
		event.data.ptr = client;
		event.events = EPOLLIN;

		if (epoll_ctl(thread->epoll_fd, EPOLL_CTL_ADD, clientfd, &event) == -1) {
			perror("Failed to add client to epoll");
			close(clientfd);
			continue;
		}

		/* Assign fd to client in a thread */
		client->fd = clientfd;
		strcpy(client->username, username);
		printf("%s connected\n", username);
		thread->num_clients++;
		
		/* Rotate num_thread back to start if it is larder than MAX_THREADS */
		num_thread = (num_thread + 1) % MAX_THREADS;
	}

	/* End the thread */
	for (int i = 0; i < MAX_THREADS; i++) {
		if (pthread_join(threads[i].thread, NULL) != 0) {
			error(0, "pthread_join");
		}
	}
	close(serverfd);
	return 0;
}
