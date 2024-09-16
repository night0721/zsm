#include "packet.h"
#include "key.h"
#include "util.h"

key_pair *create_key_pair(char *username)
{
	uint8_t cl_pk_bin[PK_BIN_SIZE], cl_sk_bin[SK_BIN_SIZE];
    crypto_sign_keypair(cl_pk_bin, cl_sk_bin);
	char pk_path[PATH_MAX], sk_path[PATH_MAX];

	/* USE DB INSTEAD OF FILES */
	sprintf(pk_path, "/home/night/%s_pk", username);
	sprintf(sk_path, "/home/night/%s_sk", username);
	FILE *pkf = fopen(pk_path, "w+");
	FILE *skf = fopen(sk_path, "w+");

	uint8_t pk_content[PK_SIZE], sk_content[SK_SIZE], metadata[METADATA_SIZE];
	time_t current_time = time(NULL);

	uint8_t *username_padded = memalloc(MAX_NAME * sizeof(uint8_t));
	strcpy(username_padded, username);
    size_t length = strlen(username);
    if (length < MAX_NAME) {
        /* Pad with null characters up to max length */
		memset(username_padded + length, 0, MAX_NAME - length);
	}
	memcpy(metadata, username_padded, MAX_NAME);
	memcpy(metadata + MAX_NAME, &current_time, TIME_SIZE);
	uint8_t *hash = memalloc(HASH_SIZE * sizeof(uint8_t));
	uint8_t *sign = memalloc(SIGN_SIZE * sizeof(uint8_t));
	crypto_generichash(hash, HASH_SIZE, metadata, METADATA_SIZE, NULL, 0);
    crypto_sign_detached(sign, NULL, hash, HASH_SIZE, cl_sk_bin);
	memcpy(pk_content, cl_pk_bin, PK_BIN_SIZE);
	memcpy(pk_content + PK_BIN_SIZE, metadata, METADATA_SIZE);
	memcpy(pk_content + PK_BIN_SIZE + METADATA_SIZE, sign, SIGN_SIZE);
	memcpy(sk_content, cl_sk_bin, SK_BIN_SIZE);
	memcpy(sk_content + SK_BIN_SIZE, metadata, METADATA_SIZE);
	memcpy(sk_content + SK_BIN_SIZE + METADATA_SIZE, sign, SIGN_SIZE);
	free(hash);

	fwrite(pk_content, 1, PK_SIZE, pkf);
	fwrite(sk_content, 1, SK_SIZE, skf);

	fclose(pkf);
	fclose(skf);

	key_pair *kp = memalloc(sizeof(key_pair));
	memcpy(kp->pk.bin, cl_pk_bin, PK_BIN_SIZE);
	memcpy(kp->pk.username, username_padded, MAX_NAME);
	kp->pk.creation = current_time;
	memcpy(kp->pk.signature, sign, SIGN_SIZE);

	memcpy(kp->sk.bin, cl_sk_bin, SK_BIN_SIZE);
	memcpy(kp->sk.username,  username_padded, MAX_NAME);
	kp->sk.creation = current_time;
	memcpy(kp->sk.signature, sign, SIGN_SIZE);

	free(username_padded);
	free(sign);
	return kp;
}

key_pair *get_key_pair(char *username)
{
    char pk_path[PATH_MAX], sk_path[PATH_MAX];
    sprintf(pk_path, "/home/night/%s_pk", username);
    sprintf(sk_path, "/home/night/%s_sk", username);

    FILE *pkf = fopen(pk_path, "r");
    FILE *skf = fopen(sk_path, "r");

    if (!pkf || !skf) {
        printf("Error opening key files.\n");
        return NULL;
    }

    uint8_t pk_content[PK_SIZE], sk_content[SK_SIZE];
    fread(pk_content, 1, PK_SIZE, pkf);
    fread(sk_content, 1, SK_SIZE, skf);

    fclose(pkf);
    fclose(skf);

    key_pair *kp = memalloc(sizeof(key_pair));

    memcpy(kp->pk.bin, pk_content, PK_BIN_SIZE);
    memcpy(kp->pk.username, pk_content + PK_BIN_SIZE, MAX_NAME);
    memcpy(&kp->pk.creation, pk_content + PK_BIN_SIZE + MAX_NAME, TIME_SIZE);
    memcpy(kp->pk.signature, pk_content + PK_BIN_SIZE + MAX_NAME + TIME_SIZE, SIGN_SIZE);

    memcpy(kp->sk.bin, sk_content, SK_BIN_SIZE);
    memcpy(kp->sk.username, sk_content + SK_BIN_SIZE, MAX_NAME);
    memcpy(&kp->sk.creation, sk_content + SK_BIN_SIZE + MAX_NAME, TIME_SIZE);
    memcpy(kp->sk.signature, sk_content + SK_BIN_SIZE + MAX_NAME + TIME_SIZE, SIGN_SIZE);

    return kp;
}
