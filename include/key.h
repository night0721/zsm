#ifndef KEY_H_
#define KEY_H_

#include <sodium.h>

#include "config.h"

#define TIME_SIZE sizeof(time_t)
#define SIGN_SIZE crypto_sign_BYTES
#define PK_RAW_SIZE crypto_kx_PUBLICKEYBYTES
#define SK_RAW_SIZE crypto_sign_SECRETKEYBYTES
#define PK_DATA_SIZE PK_RAW_SIZE + MAX_NAME + TIME_SIZE
#define PK_SIZE PK_DATA_SIZE + SIGN_SIZE /* Size with signature */
#define SK_SIZE SK_RAW_SIZE
#define SHARED_KEY_SIZE crypto_kx_SESSIONKEYBYTES

typedef struct public_key {
	uint8_t raw[PK_RAW_SIZE];
	uint8_t username[MAX_NAME];
	time_t creation;
	uint8_t signature[SIGN_SIZE];
	uint8_t	full[PK_SIZE];
} public_key;

typedef struct keypair_t {
	public_key pk;
	uint8_t sk[SK_SIZE];
} keypair_t;

keypair_t *create_keypair(char *username);
keypair_t *get_keypair(char *username);

#endif
