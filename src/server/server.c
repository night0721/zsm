#include "packet.h"
#include "notification.h"
#include <pthread.h>

socklen_t clilen;
struct sockaddr_in cli_address;
uint8_t shared_key[SHARED_KEY_SIZE];
int clientfd;

/*
 * Initialise socket server
 */
int socket_init()
{
    int serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) {
        error(1, "Error on opening socket");
    }

    /* Reuse addr(for debug) */
    int optval = 1;
    if (setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {

        error(1, "Error at setting SO_REUSEADDR");
    }

    struct sockaddr_in sv_addr;
    memset(&sv_addr, 0, sizeof(sv_addr));
    sv_addr.sin_family = AF_INET;
    sv_addr.sin_addr.s_addr = INADDR_ANY;
    sv_addr.sin_port = htons(PORT);

    if (bind(serverfd, (struct sockaddr *) &sv_addr, sizeof(sv_addr)) < 0) {
        error(1, "Error on bind");
    }

    if (listen(serverfd, MAX_CONNECTION) < 0) {
        error(1, "Error on listen");
    }

    printf("Listening on port %d\n", PORT);
    clilen = sizeof(cli_address);
    return serverfd;
}

/*
 * Performs key exchange with client
 */
int key_exchange(int clientfd)
{
    /* Generate the server's key pair */
    uint8_t sv_pk[PUBLIC_KEY_SIZE], sv_sk[PRIVATE_KEY_SIZE];
    crypto_kx_keypair(sv_pk, sv_sk);
    
    /* Get public key from client */
    uint8_t *pk;
    if ((pk = get_public_key(clientfd)) == NULL) {
        return -1;
    }
   
    /* Send our public key */
    if (send_public_key(clientfd, sv_pk) < 0) {
        free(pk);
        return -1;
    }

    /* Compute a shared key using the client's public key and our secret key. */
    if (crypto_kx_server_session_keys(NULL, shared_key, sv_pk, sv_sk, pk) != 0) {
        error(0, "Client public key is not acceptable");
        free(pk);
        close(clientfd);
        return -1;
    }

    free(pk);
    return 0;
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
            notify_uninit();
            error(1, "Shutdown signal received");
            break;
    }
}

void *receiver()
{
    int serverfd = socket_init();
    clientfd = accept(serverfd, (struct sockaddr *) &cli_address, &clilen);
    if (clientfd < 0) {
        error(0, "Error on accepting client");
        /* Continue accpeting connections */
/*         continue; */
    }

    if (key_exchange(clientfd) < 0) {
        error(0, "Error performing key exchange with client");
/*         continue; */
    }
    while (1) {
        message msg;
        memset(&msg, 0, sizeof(msg));
        if (recv_packet(&msg, clientfd) != ZSM_STA_SUCCESS) {
            close(clientfd);
            break;
/*             continue; */
        }
        
        size_t encrypted_len = msg.length - NONCE_SIZE;
        size_t msg_len = encrypted_len - ADDITIONAL_SIZE;
        uint8_t nonce[NONCE_SIZE];
        uint8_t encrypted[encrypted_len];
        uint8_t decrypted[msg_len + 1];
        unsigned long long decrypted_len;
        memcpy(nonce, msg.data, NONCE_SIZE);
        memcpy(encrypted, msg.data + NONCE_SIZE, encrypted_len);
        
        free(msg.data);
        if (crypto_aead_xchacha20poly1305_ietf_decrypt(decrypted, &decrypted_len,
                                                            NULL,
                                                            encrypted, encrypted_len,
                                                            NULL, 0,
                                                            nonce, shared_key) != 0) {
            error(0, "Cannot decrypt message");
        } else {
            /* Decrypted message */
            decrypted[msg_len] = '\0';
            printf("Decrypted: %s\n", decrypted);
            send_notification(decrypted);
            msg.data = malloc(14);
            strcpy(msg.data, "Received data");
            msg.length = 14;
            send_packet(&msg, clientfd);
            free(msg.data);
        }
    }
    close(clientfd);
    close(serverfd);
    return NULL;
}

void *sender()
{
    while (1) {
        printf("Enter message to send to client: ");
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
        if (send_packet(msg, clientfd) != ZSM_STA_SUCCESS) {
            close(clientfd);
        }
        free_packet(msg);
    }
    close(clientfd);
    return NULL;
}

int main()
{
    if (sodium_init() < 0) {
        error(1, "Error initializing libsodium");
    }
    /* Init libnotify with app name */
    if (notify_init("zsm") < 0) {
        error(1, "Error initializing libnotify");
    }
    signal(SIGPIPE, signal_handler);
    signal(SIGABRT, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
    
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
