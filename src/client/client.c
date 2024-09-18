#include "config.h"
#include "packet.h"
#include "key.h"
#include "notification.h"
#include "util.h"
#include "client/ui.h"
#include "client/db.h"
#include "server/server.h"

int sockfd;

/*
 * Authenticate with server by signing a challenge
 */
int authenticate_server(key_pair *kp)
{
    packet_t server_auth_pkt;
    int status;
    if ((status = recv_packet(&server_auth_pkt, sockfd, ZSM_TYP_AUTH) != ZSM_STA_SUCCESS)) {
        return status;
    }
    uint8_t *challenge = server_auth_pkt.data;
    
    uint8_t *sig = memalloc(SIGN_SIZE * sizeof(uint8_t));
    crypto_sign_detached(sig, NULL, challenge, CHALLENGE_SIZE, kp->sk.bin);
	
	uint8_t *pk_content = memalloc(PK_SIZE);
	memcpy(pk_content, kp->pk.bin, PK_BIN_SIZE);
	memcpy(pk_content + PK_BIN_SIZE, kp->pk.username, MAX_NAME);
	memcpy(pk_content + PK_BIN_SIZE + MAX_NAME, &kp->pk.creation, TIME_SIZE);
	memcpy(pk_content + PK_BIN_SIZE + METADATA_SIZE, kp->pk.signature, SIGN_SIZE);

    packet_t *auth_pkt = create_packet(1, ZSM_TYP_AUTH, SIGN_SIZE, pk_content, sig);
    if (send_packet(auth_pkt, sockfd) != ZSM_STA_SUCCESS) {
        /* fd already closed */
        error(0, "Could not authenticate with server");
        free(sig);
        free_packet(auth_pkt);
        return ZSM_STA_ERROR_AUTHENTICATE;
    }

    free_packet(auth_pkt);
    packet_t response;
	status = recv_packet(&response, sockfd, ZSM_TYP_INFO);
	return (response.status == ZSM_STA_AUTHORISED ?  ZSM_STA_SUCCESS : ZSM_STA_ERROR_AUTHENTICATE);
}

/*
 * Starting ui
 */
void *ui_worker(void *arg)
{
	ui(sockfd);
    return NULL;
}

/*
 * For receiving packets from server
 */
void *receive_worker(void *arg)
{
	key_pair *kp = (key_pair *) arg;

    while (1) {
		packet_t pkt;
        if (verify_packet(&pkt, sockfd) == 0) {
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
		   
		key_pair *kp_from = get_key_pair(from);
		key_pair *kp_to = get_key_pair(to);

		uint8_t shared_key[SHARED_SIZE];
		if (crypto_kx_client_session_keys(shared_key, NULL, kp_from->pk.bin,
					kp_from->sk.bin, kp_to->pk.bin) != 0) {
			/* Suspicious server public key, bail out */
			error(0, "Error performing key exchange");
		}

		/* We don't need it anymore */
		free(pkt.data);
		if (crypto_aead_xchacha20poly1305_ietf_decrypt(decrypted, NULL,
														NULL, encrypted,
														cipher_len,
														NULL, 0,
														nonce, shared_key) != 0) {
			free(decrypted);
			error(0, "Cannot decrypt data");
			return NULL;
		} else {
			/* Terminate decrypted data so we don't print random bytes */
			decrypted[data_len] = '\0';
			add_message(from, to, decrypted, data_len, time(NULL));
			show_chat(from);
			send_notification(from, decrypted);
		}
    }

    return NULL;
}

int main()
{
    if (sodium_init() < 0) {
        write_log(LOG_ERROR, "Error initializing libsodium\n");
    }
	
	/* Init libnotify with app name */
    if (notify_init("zen") < 0) {
        error(1, "Error initializing libnotify");
    }

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
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
    if (connect(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)
				) < 0) {
		error(1, "Error on connect");
		close(sockfd);
		return 0;
	}
	write_log(LOG_INFO, "Connected to server at %s\n", DOMAIN);

	/*
	key_pair *kpp = create_key_pair("palanix");
	key_pair *kpn = create_key_pair("night");
*/
	key_pair *kp = get_key_pair(USERNAME);

    if (authenticate_server(kp) != ZSM_STA_SUCCESS) {
        /* Fatal */
        error(1, "Error authenticating with server");
	} else {
		write_log(LOG_INFO, "Authenticated with server\n");
		printf("Authenticated as %s\n", USERNAME);
	}

	/* Create threads for sending and receiving messages */
	pthread_t ui_thread, receive_thread;

    if (pthread_create(&ui_thread, NULL, ui_worker, NULL) != 0) {
		close(sockfd);
        error(1, "Failed to create send thread");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&receive_thread, NULL, receive_worker, kp) != 0) {
		close(sockfd);
		error(1, "Failed to create receive thread");
    }

	/* Wait for threads to finish */
    pthread_join(ui_thread, NULL);
    pthread_join(receive_thread, NULL);

    close(sockfd);
    return 0;
}
