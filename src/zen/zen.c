#include "config.h"
#include "packet.h"
#include "key.h"
#include "util.h"
#include "zen/notification.h"
#include "zen/ui.h"
#include "zen/db.h"

/*
 * Authenticate with server by signing a challenge
 */
int authenticate_server(int *sockfd)
{
	keypair_t *kp = get_keypair(USERNAME);
	/* create empty packet */
    packet_t *pkt = create_packet(0, 0, 0, NULL, NULL);
    int status;
    if ((status = recv_packet(pkt, *sockfd, ZSM_TYP_AUTH) != ZSM_STA_SUCCESS)) {
        return status;
    }
    uint8_t *challenge = pkt->data;
    
    uint8_t *sig = memalloc(SIGN_SIZE);
    crypto_sign_detached(sig, NULL, challenge, CHALLENGE_SIZE, kp->sk);

	uint8_t *pk_full = memalloc(PK_SIZE);
	memcpy(pk_full, kp->pk.full, PK_SIZE);

	pkt->status = 1;
	pkt->type = ZSM_TYP_AUTH;
	pkt->length = SIGN_SIZE;
	pkt->data = pk_full;
	pkt->signature = sig;

    if ((status = send_packet(pkt, *sockfd)) != ZSM_STA_SUCCESS) {
        /* fd already closed */
        error(0, "Could not authenticate with server, status: %d", status);
        free_packet(pkt);
        return ZSM_STA_ERROR_AUTHENTICATE;
    }

	if ((status = recv_packet(pkt, *sockfd, ZSM_TYP_INFO)) != ZSM_STA_SUCCESS) {
		return status;
	};
	status = pkt->status;
	free_packet(pkt);
	return (status == ZSM_STA_AUTHORISED ?  ZSM_STA_SUCCESS : ZSM_STA_ERROR_AUTHENTICATE);
}

/*
 * Starting ui
 */
void *ui_worker(void *arg)
{
	int *sockfd = (int *) arg;
	ui(sockfd);
    return NULL;
}

/*
 * For receiving packets from server
 */
void *receive_worker(void *arg)
{
	int *sockfd = (int *) arg;
    while (1) {
		packet_t pkt;
		int status = verify_packet(&pkt, *sockfd);
        if (status != ZSM_STA_SUCCESS) {
			if (status == ZSM_STA_CLOSED_CONNECTION) {
				deinit();
				error(1, "Server closed connection");
			}
			error(0, "Error verifying packet");
        }
		size_t cipher_len = pkt.length - NONCE_SIZE - MAX_NAME * 2;
		size_t data_len = cipher_len - ADDITIONAL_SIZE;

		uint8_t nonce[NONCE_SIZE], encrypted[cipher_len];

		uint8_t *from = memalloc(MAX_NAME);
		uint8_t *to = memalloc(MAX_NAME);
		uint8_t *decrypted = memalloc(data_len + 1);

		/* Deconstruct data */
		memcpy(from, pkt.data, MAX_NAME);
		memcpy(to, pkt.data + MAX_NAME, MAX_NAME);
		memcpy(nonce, pkt.data + MAX_NAME * 2, NONCE_SIZE);
		memcpy(encrypted, pkt.data + MAX_NAME * 2 + NONCE_SIZE, cipher_len);
		   
		uint8_t *pk_from = get_pk_from_ks(from); /* ed25519 */
		keypair_t *kp_to = get_keypair(to);

		uint8_t *shared_key = get_sharedkey(from);
		if (shared_key == NULL) {
			uint8_t shared_key[SHARED_KEY_SIZE];

			/* Key exchange need to be done with x25519 public and secret keys */
			uint8_t to_pk[PK_X25519_SIZE];
			uint8_t from_pk[PK_X25519_SIZE];
			uint8_t to_sk[SK_X25519_SIZE];
			if (crypto_sign_ed25519_pk_to_curve25519(to_pk, kp_to->pk.raw)
					!= 0) {
				error(1, "Error converting ED25519 PK to X25519 PK");
			}
			if (crypto_sign_ed25519_pk_to_curve25519(from_pk, pk_from) != 0) {
				error(1, "Error converting ED25519 PK to X25519 PK");
			}
			if (crypto_sign_ed25519_sk_to_curve25519(to_sk, kp_to->sk) != 0) {
				error(1, "Error converting ED25519 SK to X25519 SK");
			}

			if (crypto_kx_server_session_keys(shared_key, NULL, to_pk,
						to_sk, from_pk) != 0) {
				/* Author public key is suspicious */
				write_log(LOG_ERROR, "Error performing key exchange with %s", from);
			}
			save_sharedkey(from, shared_key);
		}

		/* We don't need it anymore */
		free(pkt.data);
		if (crypto_aead_xchacha20poly1305_ietf_decrypt(decrypted, NULL, NULL,
					encrypted, cipher_len, NULL, 0, nonce, shared_key) != 0) {
			write_log(LOG_ERROR, "Unable to decrypt data from %s", from);
		} else {
			/* Terminate decrypted data so we don't print random bytes */
			decrypted[data_len] = '\0';
			/* TODO: Use mutext before add messgae */
			add_message(from, to, decrypted, data_len, time(NULL));
			send_notification(from, decrypted);
		}
    }

    return NULL;
}

int main()
{
    if (sodium_init() < 0) {
        write_log(LOG_ERROR, "Error initializing libsodium");
    }
	
	/* Init libnotify with app name */
#ifndef USE_LUFT
    if (notify_init("zen") < 0) {
        error(1, "Error initializing libnotify");
    }
#endif

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error(1, "Error on opening socket");
    }

    struct hostent *server = gethostbyname(DOMAIN);
    if (server == NULL) {
        error(1, "No such host %s", DOMAIN);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);

/*  free(server); Can't be freed seems */
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr))
			< 0) {
		error(1, "Error on connect");
		close(sockfd);
		return 0;
	}

	write_log(LOG_INFO, "Connected to server at %s", DOMAIN);
    if (authenticate_server(&sockfd) != ZSM_STA_SUCCESS) {
        /* Fatal */
        error(1, "Error authenticating with server");
	} else {
		write_log(LOG_INFO, "Authenticated to server as %s", USERNAME);
	}

	
	/* Create threads for sending and receiving messages */
	pthread_t ui_thread, receive_thread;

    if (pthread_create(&ui_thread, NULL, ui_worker, &sockfd) != 0) {
		close(sockfd);
        error(1, "Failed to create send thread");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&receive_thread, NULL, receive_worker, &sockfd) != 0) {
		close(sockfd);
		error(1, "Failed to create receive thread");
    }

	/* Wait for threads to finish */
    pthread_join(ui_thread, NULL);
    pthread_join(receive_thread, NULL);

    close(sockfd);
    return 0;
}
