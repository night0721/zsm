#ifndef KEY_H_
#define KEY_H_

#include <sodium.h>

#include "config.h"

#define TIME_SIZE sizeof(time_t)
#define SIGN_SIZE crypto_sign_BYTES
#define PK_BIN_SIZE crypto_kx_PUBLICKEYBYTES
#define SK_BIN_SIZE crypto_sign_SECRETKEYBYTES
#define METADATA_SIZE MAX_NAME + TIME_SIZE
#define PK_SIZE PK_BIN_SIZE + METADATA_SIZE + SIGN_SIZE
#define SK_SIZE SK_BIN_SIZE + METADATA_SIZE + SIGN_SIZE
#define SHARED_SIZE crypto_kx_SESSIONKEYBYTES

typedef struct public_key {
	uint8_t bin[PK_BIN_SIZE];
	uint8_t username[MAX_NAME];
	time_t creation;
	uint8_t signature[SIGN_SIZE];
} public_key;

typedef struct secret_key {
	uint8_t bin[SK_BIN_SIZE];
	uint8_t username[MAX_NAME];
	time_t creation;
	uint8_t signature[SIGN_SIZE];
} secret_key;

typedef struct key_pair {
	public_key pk;
	secret_key sk;
} key_pair;

key_pair *create_key_pair(char *username);
key_pair *get_key_pair(char *username);

#endif
