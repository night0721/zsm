#include "config.h"
#include "packet.h"
#include "key.h"
#include "util.h"
#include "zen/ui.h"
#include "zen/db.h"

config_t config;

/*
 * Authenticate with server by signing a challenge
 */
int authenticate_server(int *sockfd)
{
	/* create empty packet */
	packet_t *pkt = create_packet(0, 0, NULL, NULL);
	int status = recv_packet(pkt, *sockfd);
	free(pkt->signature);

	if (status != ZSM_STA_SUCCESS) {
		return status;
	}
	if (pkt->type != ZSM_TYP_AUTH) {
		return ZSM_STA_INVALID_TYPE;
	}
	uint8_t *challenge = pkt->data;

	uint8_t *sig = memalloc(SIGN_SIZE);
	uint8_t sk[SK_SIZE];
	sodium_hex2bin(sk, SK_SIZE, config.private_key, SK_SIZE * 2, NULL, NULL, NULL);
	crypto_sign_detached(sig, NULL, challenge, CHALLENGE_SIZE, sk);

	free(pkt->data);

	uint8_t *pk = memalloc(PK_SIZE);
	sodium_hex2bin(pk, PK_SIZE, config.public_key, PK_SIZE * 2, NULL, NULL, NULL);

	pkt->type = ZSM_TYP_AUTH;
	pkt->length = SIGN_SIZE;
	pkt->data = pk;
	pkt->signature = sig;

	if ((status = send_packet(pkt, *sockfd)) != ZSM_STA_SUCCESS) {
		/* fd already closed */
		error(0, "Could not authenticate with server, status: %d", status);
		free_packet(pkt);
		return ZSM_STA_ERROR_AUTHENTICATE;
	}

	if ((status = recv_packet(pkt, *sockfd)) != ZSM_STA_SUCCESS) {
		return status;
	};
	status = pkt->type;
	free_packet(pkt);
	return (status == ZSM_STA_AUTHORISED ?  ZSM_STA_SUCCESS : ZSM_STA_ERROR_AUTHENTICATE);
}

/*
 * Show notification using user-defined program
 */
void show_notification(uint8_t *author, uint8_t *content)
{
	int pid = fork();
	if (pid == 0) {
		/* Child */
		int fd = open("/dev/null", O_WRONLY);
		if (fd >= 0) {
			dup2(fd, STDERR_FILENO); // Redirect stderr to /dev/null
			close(fd);
		}
		execlp(notifier, notifier, author, content, NULL);
		_exit(1);
	} else if (pid > 0) {
		/* Parent */
	} else {
		write_log(LOG_ERROR, "fork failed %s", strerror(errno));
	}
}

void send_message(uint8_t *recipient, uint8_t *content, int sockfd)
{
	keypair_t *kp_from = memalloc(sizeof(keypair_t));
	sodium_hex2bin(kp_from->pk, PK_SIZE, config.public_key, PK_SIZE * 2, NULL, NULL, NULL);
	sodium_hex2bin(kp_from->sk, SK_SIZE, config.private_key, SK_SIZE * 2, NULL, NULL, NULL);

	uint8_t *shared_key = client_kx(kp_from, recipient);
	if (!shared_key) {
		wpprintw("Unable to perform key exchange with %s", recipient);
		getch();
		return;
	}

	uint8_t recipient_bin[PK_SIZE];
	sodium_hex2bin(recipient_bin, PK_SIZE, recipient, PK_SIZE * 2, NULL, NULL, NULL);

	size_t content_len = strlen(content);

	uint32_t cipher_len = content_len + ADDITIONAL_SIZE;
	uint8_t nonce[NONCE_SIZE], encrypted[cipher_len];
	
	/* Generate random nonce(number used once) */
	randombytes_buf(nonce, sizeof(nonce));
	
	/* Encrypt the content and store it to encrypted, should be cipher_len */
	crypto_aead_xchacha20poly1305_ietf_encrypt(encrypted, NULL, content,
			content_len, NULL, 0, NULL, nonce, shared_key);

	size_t data_len = MAX_NAME * 2 + NONCE_SIZE + cipher_len + sizeof(time_t);
	uint8_t *data = memalloc(data_len);

	time_t creation = time(NULL);
	/* Construct data */
	memcpy(data, kp_from->pk, MAX_NAME);
	memcpy(data + MAX_NAME, recipient_bin, MAX_NAME);
	memcpy(data + MAX_NAME * 2, nonce, NONCE_SIZE);
	memcpy(data + MAX_NAME * 2 + NONCE_SIZE, encrypted, cipher_len);
	memcpy(data + MAX_NAME * 2 + NONCE_SIZE + cipher_len, &creation, sizeof(time_t));

	uint8_t *signature = create_signature(data, data_len, kp_from->sk);
	packet_t *pkt = create_packet(ZSM_TYP_MESSAGE, data_len, data, signature);

	if (send_packet(pkt, sockfd) != ZSM_STA_SUCCESS) {
		close(sockfd);
		write_log(LOG_ERROR, "Failed to send message");
	}
	free_packet(pkt);
	free(shared_key);
	free(kp_from);
}

/*
 * For receiving packets from server
 */
void *receive_worker(void *arg)
{
	int *sockfd = (int *) arg;
	pthread_mutex_t message_lock;
	if (pthread_mutex_init(&message_lock, NULL) != 0) { 
		deinit();
		error(1, "Error on initializing mutex");
	}
	while (1) {
		packet_t pkt;
		int status = verify_packet(&pkt, *sockfd);
		if (status != ZSM_STA_SUCCESS) {
			if (status == ZSM_STA_CLOSED_CONNECTION) {
				deinit();
				error(0, "Server closed connection");
				pthread_exit(NULL);
			} else if (status == ZSM_STA_READING_SOCKET) {
				deinit();
				pthread_exit(NULL);
			}
			error(0, "Error verifying packet");
		}
		pthread_mutex_lock(&message_lock);
		size_t cipher_len = pkt.length - NONCE_SIZE - MAX_NAME * 2 - sizeof(time_t);
		size_t data_len = cipher_len - ADDITIONAL_SIZE;

		uint8_t nonce[NONCE_SIZE], encrypted[cipher_len], from[MAX_NAME],
		to[MAX_NAME], decrypted[data_len + 1], to_hex[PK_SIZE * 2 + 1],
		from_hex[PK_SIZE * 2 + 1];
		time_t creation;

		/* Deconstruct data */
		memcpy(from, pkt.data, MAX_NAME);
		memcpy(to, pkt.data + MAX_NAME, MAX_NAME);
		memcpy(nonce, pkt.data + MAX_NAME * 2, NONCE_SIZE);
		memcpy(encrypted, pkt.data + MAX_NAME * 2 + NONCE_SIZE, cipher_len);
		memcpy(&creation, pkt.data + MAX_NAME * 2 + NONCE_SIZE + cipher_len, sizeof(time_t));

		sodium_bin2hex(to_hex, sizeof(to_hex), to, PK_SIZE);
		sodium_bin2hex(from_hex, sizeof(from_hex), from, PK_SIZE);

		uint8_t pk_ed25519[PK_SIZE], sk_ed25519[SK_SIZE];
		sodium_hex2bin(pk_ed25519, PK_SIZE, to_hex, PK_SIZE * 2, NULL, NULL, NULL);
		sodium_hex2bin(sk_ed25519, SK_SIZE, config.private_key, SK_SIZE * 2, NULL, NULL, NULL);
		uint8_t *shared_key = get_receivekey(from_hex);
		if (!shared_key) {
			shared_key = memalloc(SHARED_KEY_SIZE);

			/* Key exchange need to be done with x25519 public and secret keys */
			uint8_t to_pk[PK_X25519_SIZE];
			uint8_t from_pk[PK_X25519_SIZE];
			uint8_t to_sk[SK_X25519_SIZE];
			if (crypto_sign_ed25519_pk_to_curve25519(to_pk, pk_ed25519)
					!= 0) {
				error(0, "Error converting ED25519 PK to X25519 PK");
			}
			if (crypto_sign_ed25519_pk_to_curve25519(from_pk, from) != 0) {
				error(0, "Error converting ED25519 PK to X25519 PK");
			}
			if (crypto_sign_ed25519_sk_to_curve25519(to_sk, sk_ed25519) != 0) {
				error(0, "Error converting ED25519 SK to X25519 SK");
			}
			uint8_t dummy[SHARED_KEY_SIZE];
			if (crypto_kx_client_session_keys(dummy, shared_key, to_pk,
						to_sk, from_pk) != 0) {
				/* Author public key is suspicious */
				write_log(LOG_ERROR, "Error performing key exchange with %s", from);
			} else {
				save_receivekey(from_hex, shared_key);
			}
		}
		if (shared_key) {
			/* We don't need it anymore */
			free(pkt.data);
			if (crypto_aead_xchacha20poly1305_ietf_decrypt(decrypted, NULL, NULL,
						encrypted, cipher_len, NULL, 0, nonce, shared_key) != 0) {
				write_log(LOG_ERROR, "Unable to decrypt data from %s", from_hex);
			} else {
				/* Terminate decrypted data so we don't print random bytes */
				decrypted[data_len] = '\0';
				write_log(LOG_INFO, "Decrypted: %s", decrypted);
				save_message(from_hex, to_hex, decrypted, creation);
				show_notification(from_hex, decrypted);
				update_current_user(from_hex);
				show_chat(from_hex);
			}
			free(shared_key);
		}
		pthread_mutex_unlock(&message_lock);
	}

	return NULL;
}

int read_config(const char *file_path)
{
	FILE *file = fopen(file_path, "r");
	if (!file) {
		error(1, "Error opening config file");
	}

	char *line = NULL;
	size_t len = 0;

	while (getline(&line, &len, file) != -1) {
		/* Remove newline */
		line[strcspn(line, "\n")] = 0;

		char *key = strtok(line, "=");
		char *value = strtok(NULL, "=");

		if (key && value) {
			if (strcmp(key, "public_key") == 0) {
				strncpy(config.public_key, value, sizeof(config.public_key) - 1);
			} else if (strcmp(key, "private_key") == 0) {
				strncpy(config.private_key, value, sizeof(config.private_key) - 1);
			} else if (strcmp(key, "server_address") == 0) {
				strncpy(config.server_address, value, sizeof(config.server_address) - 1);
			} else {
				error(0, "Unknown key: %s", key);
			}
		}
	}

	free(line);
	fclose(file);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 2 && !strncmp(argv[1], "create-key", 10)) {
		keypair_t *kp = create_keypair();
		printf("Created keypair!\n");
		printf("Public Key: \n");
		print_bin(kp->pk, PK_SIZE);
		printf("Private Key: \n");
		print_bin(kp->sk, SK_SIZE);
		free(kp);
		return 0;
	} else if (argc == 3 && !strncmp(argv[1], "create-backup", 13)) {
		int pid = fork();
		if (pid == 0) {
			/* Child */
			char from[PATH_MAX], to[PATH_MAX];
			char *data_dir = replace_home(CLIENT_DATA_DIR);
			snprintf(from, PATH_MAX, "%s/data.db", data_dir);
			snprintf(to, PATH_MAX, "%s.db", argv[2]);
			execlp("cp", "cp", from, to, NULL);
			free(data_dir);
			_exit(1);
		} else if (pid > 0) {
			/* Parent */
		} else {
			write_log(LOG_ERROR, "fork failed %s", strerror(errno));
		}
		return 0;
	} else if (argc == 3 && !strncmp(argv[1], "-c", 2)) {
		read_config(argv[2]);
	} else {
		/* Read default config file at CLIENT_DATA_DIR/zen.conf */
		char *config_path = memalloc(strlen(CLIENT_DATA_DIR) + strlen("/zen.conf") + 1);
		sprintf(config_path, "%s/zen.conf", CLIENT_DATA_DIR);
		char *real_path = replace_home(config_path);
		read_config(real_path);
		free(real_path);
		free(config_path);
	}
	if (strlen(config.public_key) != 64 || strlen(config.private_key) != 128) {
		error(1, "Invalid config file, public key must have 64 characters, private key must be set and server address must have 128 characters");
	}

	if (sodium_init() < 0) {
		write_log(LOG_ERROR, "Error initializing libsodium");
	}

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		error(1, "Error on opening socket");
	}

	struct hostent *server = gethostbyname(config.server_address);
	if (server == NULL) {
		error(1, "No such host %s", config.server_address);
	}

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

	if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr))
			< 0) {
		free(server);
		close(sockfd);
		error(1, "Error on connect");
		return 0;
	}

	write_log(LOG_INFO, "Connected to server at %s", config.server_address);
	if (authenticate_server(&sockfd) != ZSM_STA_SUCCESS) {
		/* Fatal */
		free(server);
		error(1, "Error authenticating with server");
	} else {
		write_log(LOG_INFO, "Authenticated to server as %s", config.public_key);
	}

	/* Create threads for receiving messages */
	pthread_t receive_thread;

	if (pthread_create(&receive_thread, NULL, receive_worker, &sockfd) != 0) {
		free(server);
		close(sockfd);
		error(1, "Failed to create receive thread");
	}
	ui(sockfd, &config);

	if (pthread_cancel(receive_thread) != 0) {
		free(server);
		close(sockfd);
		error(1, "Failed to cancel receive thread");
		return 1;
	}
	/* Wait for thread to finish */
	pthread_join(receive_thread, NULL);

	close(sockfd);
	free(server);
	return 0;
}
