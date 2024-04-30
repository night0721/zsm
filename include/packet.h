#ifndef PACKET_H
#define PACKET_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sodium.h>

#define DEBUG 1
#define DOMAIN "127.0.0.1"
#define PORT 20247
#define MAX_CONNECTION 5
#define MAX_MESSAGE_LENGTH 8192
#define ERROR_LENGTH 26

#define ZSM_TYP_KEY 0x1
#define ZSM_TYP_SEND_MESSAGE 0x2
#define ZSM_TYP_UPDATE_MESSAGE 0x3
#define ZSM_TYP_DELETE_MESSAGE 0x4
#define ZSM_TYP_PRESENCE 0x5
#define ZSM_TYP_TYPING 0x6
#define ZSM_TYP_ERROR 0x7 /* Error message */
#define ZSM_TYP_B 0x8

#define ZSM_STA_SUCCESS 0x1
#define ZSM_STA_INVALID_TYPE 0x2
#define ZSM_STA_INVALID_LENGTH 0x3
#define ZSM_STA_TOO_LONG 0x4
#define ZSM_STA_READING_SOCKET 0x5
#define ZSM_STA_WRITING_SOCKET 0x6
#define ZSM_STA_UNKNOWN_USER 0x7
#define ZSM_STA_MEMORY_ALLOCATION 0x8
#define ZSM_STA_WRONG_KEY_LENGTH 0x9

#define PUBLIC_KEY_SIZE crypto_kx_PUBLICKEYBYTES
#define PRIVATE_KEY_SIZE crypto_kx_SECRETKEYBYTES
#define SHARED_KEY_SIZE crypto_kx_SESSIONKEYBYTES
#define NONCE_SIZE crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
#define ADDITIONAL_SIZE crypto_aead_xchacha20poly1305_ietf_ABYTES

typedef struct message {
    uint8_t option;
    uint8_t type;
    unsigned long long length;
    unsigned char *data;
} message;

/* Utilities functions */
void error(int fatal, const char *fmt, ...);
void *memalloc(size_t size);
void *estrdup(void *str);
unsigned char *get_public_key(int sockfd);
int send_public_key(int sockfd, unsigned char *pk);
void print_packet(message *msg);
int recv_packet(message *msg, int fd);
message *create_error_packet(int code);
message *create_packet(uint8_t option, uint8_t type, uint32_t length, char *data);
int send_packet(message *msg, int fd);
void free_packet(message *msg);

#endif
