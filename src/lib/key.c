#include "packet.h"
#include "key.h"
#include "util.h"

keypair_t *create_keypair(char *username)
{
	uint8_t pk_raw[PK_RAW_SIZE], sk[SK_SIZE], pk_data[PK_DATA_SIZE],
			username_padded[MAX_NAME], pk_hash[HASH_SIZE], pk_sign[SIGN_SIZE],
			pk[PK_SIZE];

    crypto_sign_keypair(pk_raw, sk);
	
	time_t current_time = time(NULL);

	strcpy(username_padded, username);
    size_t length = strlen(username);
    if (length < MAX_NAME) {
        /* Pad with null characters up to max length */
		memset(username_padded + length, 0, MAX_NAME - length);
	} else {
		error(0, "Username must be shorter than MAX_NAME");
		return NULL;
	}
	
	memcpy(pk_data, pk_raw, PK_RAW_SIZE);
	memcpy(pk_data + PK_RAW_SIZE, username_padded, MAX_NAME);
	memcpy(pk_data + PK_RAW_SIZE + MAX_NAME, &current_time, TIME_SIZE);

	crypto_generichash(pk_hash, HASH_SIZE, pk_data, PK_DATA_SIZE, NULL, 0);
    crypto_sign_detached(pk_sign, NULL, pk_hash, HASH_SIZE, sk);

	memcpy(pk, pk_data, PK_DATA_SIZE);
	memcpy(pk + PK_DATA_SIZE, pk_sign, SIGN_SIZE);

	/* USE DB INSTEAD OF FILES */
	char pk_path[PATH_MAX], sk_path[PATH_MAX];
	sprintf(pk_path, "/home/night/%s_pk", username);
	sprintf(sk_path, "/home/night/%s_sk", username);
	FILE *pkf = fopen(pk_path, "w+");
	FILE *skf = fopen(sk_path, "w+");
	fwrite(pk, 1, PK_SIZE, pkf);
	fwrite(sk, 1, SK_SIZE, skf);
	fclose(pkf);
	fclose(skf);

	keypair_t *kp = memalloc(sizeof(keypair_t));
	memcpy(kp->pk.raw, pk_raw, PK_RAW_SIZE);
	memcpy(kp->pk.username, username_padded, MAX_NAME);
	kp->pk.creation = current_time;
	memcpy(kp->pk.signature, pk_sign, SIGN_SIZE);
	memcpy(kp->pk.full, pk, PK_SIZE);

	memcpy(kp->sk, sk, SK_SIZE);

	return kp;
}

keypair_t *get_keypair(char *username)
{
	/* REPLACE WITH DB */
    char pk_path[PATH_MAX], sk_path[PATH_MAX];
    sprintf(pk_path, "/home/night/%s_pk", username);
    sprintf(sk_path, "/home/night/%s_sk", username);
    FILE *pkf = fopen(pk_path, "r");
    FILE *skf = fopen(sk_path, "r");
    if (!pkf || !skf) {
		create_keypair(username);
        printf("Error opening key files.\n");
        return NULL;
    }

	uint8_t pk[PK_SIZE], sk[SK_SIZE];
    fread(pk, 1, PK_SIZE, pkf);
    fread(sk, 1, SK_SIZE, skf);
    fclose(pkf);
    fclose(skf);

    keypair_t *kp = memalloc(sizeof(keypair_t));

    memcpy(kp->pk.raw, pk, PK_RAW_SIZE);
    memcpy(kp->pk.username, pk + PK_RAW_SIZE, MAX_NAME);
    memcpy(&kp->pk.creation, pk + PK_RAW_SIZE + MAX_NAME, TIME_SIZE);
    memcpy(kp->pk.signature, pk + PK_RAW_SIZE + MAX_NAME + TIME_SIZE, SIGN_SIZE);
	memcpy(kp->pk.full, pk, PK_SIZE);

    memcpy(kp->sk, sk, SK_SIZE);

    return kp;
}
