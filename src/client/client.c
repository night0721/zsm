#include "config.h"
#include "packet.h"
#include "key.h"
#include "util.h"
#include "client/ui.h"
#include "client/db.h"

int sockfd;

/*
 * Authenticate with server by signing a challenge
 */
int authenticate_server(key_pair *kp)
{
    packet server_auth_pkt;
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

    packet *auth_pkt = create_packet(1, ZSM_TYP_AUTH, SIGN_SIZE, pk_content, sig);
    if (send_packet(auth_pkt, sockfd) != ZSM_STA_SUCCESS) {
        /* fd already closed */
        error(0, "Could not authenticate with server");
        free(sig);
        free_packet(auth_pkt);
        return ZSM_STA_ERROR_AUTHENTICATE;
    }

    free_packet(auth_pkt);
    packet response;
	status = recv_packet(&response, sockfd, ZSM_TYP_INFO);
	return (response.status == ZSM_STA_AUTHORISED ?  ZSM_STA_SUCCESS : ZSM_STA_ERROR_AUTHENTICATE);
}

/*
 * For sending packets to server
 */
void *send_message(void *arg)
{
	key_pair *kp = (key_pair *) arg;

    while (1) {
		int status = encrypt_packet(sockfd, kp);
        if (status != ZSM_STA_SUCCESS) {
            error(1, "Error encrypting packet %x", status);
        }
    }

    return NULL;
}

/*
 * For receiving packets from server
 */
void *receive_message(void *arg)
{
	key_pair *kp = (key_pair *) arg;

    while (1) {
		packet pkt;

        if (verify_packet(&pkt, sockfd) == 0) {
            error(0, "Error verifying packet");
        }
		uint8_t *decrypted = decrypt_data(&pkt);
		free(decrypted);
    }

    return NULL;
}

int main()
{
    if (sodium_init() < 0) {
        write_log(LOG_ERROR, "Error initializing libsodium\n");
    }
    //ui();

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
		if (errno != EINPROGRESS) {
			/* Connection is in progress, shouldn't be treated as error */
            error(1, "Error on connect");
            close(sockfd);
            return 0;
        }
	}
	write_log(LOG_INFO, "Connected to server at %s\n", DOMAIN);

/* 	set_nonblocking(sockfd); */
	/*
	key_pair *kpp = create_key_pair("palanix");
	key_pair *kpn = create_key_pair("night");
*/
	key_pair *kpp = get_key_pair("palanix");
	key_pair *kpn = get_key_pair("night");

    if (authenticate_server(kpp) != ZSM_STA_SUCCESS) {
        /* Fatal */
        error(1, "Error authenticating with server");
	} else {
		write_log(LOG_INFO, "Authenticated with server\n");
		printf("Authenticated as palanix\n");
	}

	/* Create threads for sending and receiving messages */
	pthread_t send_thread, receive_thread;

    if (pthread_create(&send_thread, NULL, send_message, kpp) != 0) {
		close(sockfd);
        error(1, "Failed to create send thread");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&receive_thread, NULL, receive_message, kpp) != 0) {
		close(sockfd);
		error(1, "Failed to create receive thread");
    }

	/* Wait for threads to finish */
    pthread_join(send_thread, NULL);
    pthread_join(receive_thread, NULL);

    close(sockfd);
    return 0;
}
