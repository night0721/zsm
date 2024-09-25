#ifndef KEY_H_
#define KEY_H_

#include <sodium.h>

#include "config.h"

#define TIME_SIZE sizeof(time_t)
#define SIGN_SIZE crypto_sign_BYTES
#define PK_ED25519_SIZE crypto_sign_PUBLICKEYBYTES /* 32, size for signature keys */
#define SK_ED25519_SIZE crypto_sign_SECRETKEYBYTES /* 64 */
#define PK_X25519_SIZE crypto_kx_PUBLICKEYBYTES /* 32, size for key exchange keys */
#define SK_X25519_SIZE crypto_kx_PUBLICKEYBYTES  /* same with public key */
#define PK_DATA_SIZE PK_ED25519_SIZE + MAX_NAME + TIME_SIZE
#define PK_SIZE PK_DATA_SIZE + SIGN_SIZE /* Size with signature */
#define SK_SIZE SK_ED25519_SIZE 
#define SHARED_KEY_SIZE crypto_kx_SESSIONKEYBYTES

typedef struct public_key {
	uint8_t raw[PK_ED25519_SIZE];
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
uint8_t *get_pk_from_ks(char *username);

#endif
