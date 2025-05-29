#ifndef KEY_H_
#define KEY_H_

#include <sodium.h>

#include "config.h"

#define SIGN_SIZE crypto_sign_BYTES /* 64 */
#define PK_SIZE crypto_sign_PUBLICKEYBYTES /* 32, for signature keys */
#define SK_SIZE crypto_sign_SECRETKEYBYTES /* 64 */
#define PK_X25519_SIZE crypto_kx_PUBLICKEYBYTES /* 32, for key exchange keys */
#define SK_X25519_SIZE crypto_kx_PUBLICKEYBYTES  /* same with public key */
#define SHARED_KEY_SIZE crypto_kx_SESSIONKEYBYTES

typedef struct {
	uint8_t pk[PK_SIZE];
	uint8_t sk[SK_SIZE];
} keypair_t;

keypair_t *create_keypair(void);
keypair_t *get_keypair(char *username);

#endif
