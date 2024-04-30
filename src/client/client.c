#include "packet.h"
#include <pthread.h>

uint8_t shared_key[SHARED_KEY_SIZE];
int sockfd;

/*
 * Connect to socket server
 */
int socket_init()
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error(1, "Error on opening socket");
    }

    struct hostent *server = gethostbyname(DOMAIN);
    if (server == NULL) {
        error(1, "No such host %s", DOMAIN);
    }

    struct sockaddr_in sv_addr;
    memset(&sv_addr, 0, sizeof(sv_addr));
    sv_addr.sin_family = AF_INET;
    sv_addr.sin_port = htons(PORT);
    memcpy(&sv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

/*     free(server); */
    if (connect(sockfd, (struct sockaddr *) &sv_addr, sizeof(sv_addr)) < 0) {
        error(1, "Error on connect");
        close(sockfd);
        return 0;
    }
    printf("Connected to server at %s\n", DOMAIN);
    return sockfd;
}

/*
 * Performs key exchange with server
 */
int key_exchange(int sockfd)
{
    /* Generate the client's key pair */
    uint8_t cl_pk[PUBLIC_KEY_SIZE], cl_sk[PRIVATE_KEY_SIZE];
    crypto_kx_keypair(cl_pk, cl_sk);

    /* Send our public key */
    if (send_public_key(sockfd, cl_pk) < 0) {
        return -1;
    }
    
    /* Get public key from server */
    uint8_t *pk;
    if ((pk = get_public_key(sockfd)) == NULL) {
        return -1;
    }

    /* Compute a shared key using the server's public key and our secret key */
    if (crypto_kx_client_session_keys(NULL, shared_key, cl_pk, cl_sk, pk) != 0) {
        error(1, "Server public key is not acceptable");
        free(pk);
        close(sockfd);
        return -1;
    }
    free(pk);
    return 0;
}

void *sender()
{
    while (1) {
        printf("Enter message to send to server: ");
        fflush(stdout);
        char line[1024];
        line[0] = '\0';
        size_t length = strlen(line);
        while (length <= 1) {
            fgets(line, sizeof(line), stdin);
            length = strlen(line);
        }
        length -= 1;
        line[length] = '\0';

        uint8_t nonce[NONCE_SIZE];
        uint8_t encrypted[length + ADDITIONAL_SIZE];
        unsigned long long encrypted_len;
        
        randombytes_buf(nonce, sizeof(nonce));
        crypto_aead_xchacha20poly1305_ietf_encrypt(encrypted, &encrypted_len,
                                                   line, length,
                                                   NULL, 0, NULL, nonce, shared_key);
        size_t payload_t = NONCE_SIZE + encrypted_len; 
        uint8_t encryptedwithnonce[payload_t];
        memcpy(encryptedwithnonce, nonce, NONCE_SIZE);
        memcpy(encryptedwithnonce + NONCE_SIZE, encrypted, encrypted_len);
        
        message *msg = create_packet(1, 0x10, payload_t, encryptedwithnonce);
        if (send_packet(msg, sockfd) != ZSM_STA_SUCCESS) {
            close(sockfd);
        }
        free_packet(msg);
    }
    close(sockfd);

}

void *receiver()
{
    while (1) {
        message servermsg;
        if (recv_packet(&servermsg, sockfd) != ZSM_STA_SUCCESS) {
            close(sockfd);
            return 0;
        }
        free(servermsg.data);
    }
    return NULL;
}

int main()
{
    if (sodium_init() < 0) {
        error(1, "Error initializing libsodium");
    }
    sockfd = socket_init();
    if (key_exchange(sockfd) < 0) {
        /* Fatal */
        error(1, "Error performing key exchange with server");
    }
    pthread_t recv_worker, send_worker;
    if (pthread_create(&recv_worker, NULL, sender, NULL) != 0) {
        fprintf(stderr, "Error creating incoming thread\n");
        return 1;
    }

    if (pthread_create(&send_worker, NULL, receiver, NULL) != 0) {
        fprintf(stderr, "Error creating outgoing thread\n");
        return 1;
    }

    // Join threads
    pthread_join(recv_worker, NULL);
    pthread_join(send_worker, NULL);

    return 0;
}

