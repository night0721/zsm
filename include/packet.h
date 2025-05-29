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
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>
#include <sodium.h>
#include <libgen.h>
#include <wchar.h>

#include "config.h"

enum {
    /* Types */
    ZSM_TYP_AUTH = 0x1,
    ZSM_TYP_MESSAGE = 0x2,
    ZSM_TYP_UPDATE_MESSAGE = 0x3,
    ZSM_TYP_DELETE_MESSAGE = 0x4,
    ZSM_TYP_ERROR = 0x5,
    ZSM_TYP_INFO = 0x6,

    /* Status */
    ZSM_STA_SUCCESS = 0x7,
    ZSM_STA_INVALID_TYPE = 0x8,
    ZSM_STA_INVALID_LENGTH = 0x9,
    ZSM_STA_TOO_LONG = 0xA,
    ZSM_STA_READING_SOCKET = 0xB,
    ZSM_STA_WRITING_SOCKET = 0xC,
    ZSM_STA_UNKNOWN_USER = 0xD,
    ZSM_STA_MEMORY_ALLOCATION = 0xE,
    ZSM_STA_ERROR_ENCRYPT = 0xF,
    ZSM_STA_ERROR_DECRYPT = 0x10,
    ZSM_STA_ERROR_AUTHENTICATE = 0x11,
    ZSM_STA_ERROR_INTEGRITY = 0x12,
    ZSM_STA_UNAUTHORISED = 0x13,
    ZSM_STA_AUTHORISED = 0x14,
    ZSM_STA_CLOSED_CONNECTION = 0x15
};

#define PORT 20247
#define MAX_NAME crypto_sign_PUBLICKEYBYTES /* Max username length */
#define MAX_DATA_LENGTH 8192

#define CHALLENGE_SIZE 32
#define HASH_SIZE crypto_generichash_BYTES /* 32 */
#define NONCE_SIZE crypto_box_NONCEBYTES /* 24 */
#define ADDITIONAL_SIZE crypto_box_MACBYTES /* 16 */
#define MAX_MESSAGE_LENGTH MAX_DATA_LENGTH - MAX_NAME * 2 - NONCE_SIZE

typedef struct {
    uint8_t type;
    uint32_t length;
    uint8_t *data;
    uint8_t *signature;
} packet_t;

#include "key.h"

/* Utilities functions */
void print_packet(packet_t *pkt);
int recv_packet(packet_t *pkt, int fd);
packet_t *create_packet(uint8_t type, uint32_t length, uint8_t *data, uint8_t *signature);
int send_packet(packet_t *pkt, int fd);
void free_packet(packet_t *pkt);
int verify_packet(packet_t *pkt, int fd);
uint8_t *create_signature(uint8_t *data, uint32_t length, uint8_t *sk);

#endif
