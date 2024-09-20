#include "packet.h"
#include "key.h"
#include "util.h"

keypair_t *create_keypair(char *username)
{
	uint8_t pk_raw[PK_RAW_SIZE], sk_raw[SK_RAW_SIZE], metadata[METADATA_SIZE],
	username_padded[MAX_NAME], hash[HASH_SIZE], sign[SIGN_SIZE],
	pk_full[PK_SIZE], sk_full[SK_SIZE];
    crypto_sign_keypair(pk_raw, sk_raw);
	
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

	memcpy(metadata, username_padded, MAX_NAME);
	memcpy(metadata + MAX_NAME, &current_time, TIME_SIZE);

	crypto_generichash(hash, HASH_SIZE, metadata, METADATA_SIZE, NULL, 0);
    crypto_sign_detached(sign, NULL, hash, HASH_SIZE, sk_raw);

	memcpy(pk_full, pk_raw, PK_RAW_SIZE);
	memcpy(pk_full + PK_RAW_SIZE, metadata, METADATA_SIZE);
	memcpy(pk_full + PK_RAW_SIZE + METADATA_SIZE, sign, SIGN_SIZE);
	memcpy(sk_full, sk_raw, SK_RAW_SIZE);
	memcpy(sk_full + SK_RAW_SIZE, metadata, METADATA_SIZE);
	memcpy(sk_full + SK_RAW_SIZE + METADATA_SIZE, sign, SIGN_SIZE);

	/* USE DB INSTEAD OF FILES */
	char pk_path[PATH_MAX], sk_path[PATH_MAX];
	sprintf(pk_path, "/home/night/%s_pk", username);
	sprintf(sk_path, "/home/night/%s_sk", username);
	FILE *pkf = fopen(pk_path, "w+");
	FILE *skf = fopen(sk_path, "w+");
	fwrite(pk_full, 1, PK_SIZE, pkf);
	fwrite(sk_full, 1, SK_SIZE, skf);
	fclose(pkf);
	fclose(skf);

	keypair_t *kp = memalloc(sizeof(keypair_t));
	memcpy(kp->pk.raw, pk_raw, PK_RAW_SIZE);
	memcpy(kp->pk.username, username_padded, MAX_NAME);
	kp->pk.creation = current_time;
	memcpy(kp->pk.signature, sign, SIGN_SIZE);
	memcpy(kp->pk.full, pk_full, PK_SIZE);

	memcpy(kp->sk.raw, sk_raw, SK_RAW_SIZE);
	memcpy(kp->sk.username, username_padded, MAX_NAME);
	kp->sk.creation = current_time;
	memcpy(kp->sk.signature, sign, SIGN_SIZE);
	memcpy(kp->sk.full, sk_full, PK_SIZE);

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
        printf("Error opening key files.\n");
        return NULL;
    }

	uint8_t pk_full[PK_SIZE], sk_full[SK_SIZE];
    fread(pk_full, 1, PK_SIZE, pkf);
    fread(sk_full, 1, SK_SIZE, skf);
    fclose(pkf);
    fclose(skf);

    keypair_t *kp = memalloc(sizeof(keypair_t));

    memcpy(kp->pk.raw, pk_full, PK_RAW_SIZE);
    memcpy(kp->pk.username, pk_full + PK_RAW_SIZE, MAX_NAME);
    memcpy(&kp->pk.creation, pk_full + PK_RAW_SIZE + MAX_NAME, TIME_SIZE);
    memcpy(kp->pk.signature, pk_full + PK_RAW_SIZE + MAX_NAME + TIME_SIZE, SIGN_SIZE);
	memcpy(kp->pk.full, pk_full, PK_SIZE);

    memcpy(kp->sk.raw, sk_full, SK_RAW_SIZE);
    memcpy(kp->sk.username, sk_full + SK_RAW_SIZE, MAX_NAME);
    memcpy(&kp->sk.creation, sk_full + SK_RAW_SIZE + MAX_NAME, TIME_SIZE);
    memcpy(kp->sk.signature, sk_full + SK_RAW_SIZE + MAX_NAME + TIME_SIZE, SIGN_SIZE);
	memcpy(kp->sk.full, sk_full, SK_SIZE);

    return kp;
}
