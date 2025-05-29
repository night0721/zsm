#include "packet.h"
#include "key.h"
#include "util.h"

keypair_t *create_keypair(void)
{
	uint8_t pk[PK_SIZE], sk[SK_SIZE], pk_hex[PK_SIZE * 2 + 1];
	crypto_sign_keypair(pk, sk);
	sodium_bin2hex(pk_hex, sizeof(pk_hex), pk, PK_SIZE);

	keypair_t *kp = memalloc(sizeof(keypair_t));
	memcpy(kp->pk, pk, PK_SIZE);
	memcpy(kp->sk, sk, SK_SIZE);
	write_log(LOG_INFO, "Created key pair");

	return kp;
}
