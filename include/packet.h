#ifndef PACKET_H
#define PACKET_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>
#include <sodium.h>
#include <libgen.h>
#include <wchar.h>

#include "config.h"

#define ZSM_TYP_AUTH 0x1
#define ZSM_TYP_MESSAGE 0x2
#define ZSM_TYP_UPDATE_MESSAGE 0x3
#define ZSM_TYP_DELETE_MESSAGE 0x4
#define ZSM_TYP_ERROR 0x5
#define ZSM_TYP_INFO 0x6

#define ZSM_STA_SUCCESS 0x1
#define ZSM_STA_INVALID_TYPE 0x2
#define ZSM_STA_INVALID_LENGTH 0x3
#define ZSM_STA_TOO_LONG 0x4
#define ZSM_STA_READING_SOCKET 0x5
#define ZSM_STA_WRITING_SOCKET 0x6
#define ZSM_STA_UNKNOWN_USER 0x7
#define ZSM_STA_MEMORY_ALLOCATION 0x8
#define ZSM_STA_ERROR_ENCRYPT 0x9
#define ZSM_STA_ERROR_DECRYPT 0xA
#define ZSM_STA_ERROR_AUTHENTICATE 0xB
#define ZSM_STA_ERROR_INTEGRITY 0xC
#define ZSM_STA_UNAUTHORISED 0xD
#define ZSM_STA_AUTHORISED 0xE

#define ADDRESS_SIZE MAX_NAME + 1 + 255 /* 1 for @, 255 for domain, defined in RFC 5321, Section 4.5.3.1.2 */
#define HASH_SIZE crypto_generichash_BYTES
#define NONCE_SIZE crypto_aead_xchacha20poly1305_ietf_NPUBBYTES
#define ADDITIONAL_SIZE crypto_aead_xchacha20poly1305_ietf_ABYTES
#define MAX_MESSAGE_LENGTH MAX_DATA_LENGTH - MAX_NAME * 2 - NONCE_SIZE

typedef struct packet_t {
    uint8_t status;
    uint8_t type;
    uint32_t length;
    uint8_t *data;
    uint8_t *signature;
} packet_t;

typedef struct message_t {
	uint8_t author[MAX_NAME];
	uint8_t recipient[MAX_NAME];
	uint8_t *content;
	time_t creation;
} message_t;

#include "key.h"

/* Utilities functions */
void print_packet(packet_t *msg);
int recv_packet(packet_t *pkt, int fd, uint8_t required_type);
packet_t *create_packet(uint8_t option, uint8_t type, uint32_t length, uint8_t *data, uint8_t *signature);
int send_packet(packet_t *msg, int fd);
void free_packet(packet_t *msg);
int verify_packet(packet_t *pkt, int fd);
uint8_t *create_signature(uint8_t *data, uint32_t length, secret_key *sk);

#endif
