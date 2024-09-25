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

	char keyf_path[PATH_MAX];
	char *data_dir = replace_home(CLIENT_DATA_DIR);
	snprintf(keyf_path, PATH_MAX, "%s/%s/keys", data_dir, USERNAME);
	free(data_dir);

	if (access(keyf_path, W_OK) != 0) {
		/* If data file doesn't exist, most likely data dir won't exist too */
		mkdir_p(keyf_path);
	}

	FILE *keyf = fopen(keyf_path, "w+");
	if (!keyf) {
		error(1, "Error opening key file to write");
		return NULL;
	}
	/* Write key to file */
	fwrite(pk, 1, PK_SIZE, keyf);
	fwrite(sk, 1, SK_SIZE, keyf);
	fclose(keyf);

	keypair_t *kp = memalloc(sizeof(keypair_t));
	memcpy(kp->pk.raw, pk_raw, PK_RAW_SIZE);
	memcpy(kp->pk.username, username_padded, MAX_NAME);
	kp->pk.creation = current_time;
	memcpy(kp->pk.signature, pk_sign, SIGN_SIZE);
	memcpy(kp->pk.full, pk, PK_SIZE);

	memcpy(kp->sk, sk, SK_SIZE);
	write_log(LOG_INFO, "Created key pair");

	return kp;
}

keypair_t *get_keypair(char *username)
{
	char keyf_path[PATH_MAX];
	char *data_dir = replace_home(CLIENT_DATA_DIR);
	snprintf(keyf_path, PATH_MAX, "%s/%s/keys", data_dir, USERNAME);
	free(data_dir);

	if (access(keyf_path, W_OK) != 0) {
		/* If data file doesn't exist, most likely data dir won't exist too */
		mkdir_p(keyf_path);
		/* Create key pair as file doesn't exist */
		create_keypair(username);
	}

    FILE *keyf = fopen(keyf_path, "r");
    if (!keyf) {
		error(1, "Error opening key file to read");
        return NULL;
    }

	uint8_t pk[PK_SIZE], sk[SK_SIZE];
    fread(pk, 1, PK_SIZE, keyf);
    fread(sk, 1, SK_SIZE, keyf);
    fclose(keyf);

    keypair_t *kp = memalloc(sizeof(keypair_t));

    memcpy(kp->pk.raw, pk, PK_RAW_SIZE);
    memcpy(kp->pk.username, pk + PK_RAW_SIZE, MAX_NAME);
    memcpy(&kp->pk.creation, pk + PK_RAW_SIZE + MAX_NAME, TIME_SIZE);
    memcpy(kp->pk.signature, pk + PK_RAW_SIZE + MAX_NAME + TIME_SIZE, SIGN_SIZE);
	memcpy(kp->pk.full, pk, PK_SIZE);

    memcpy(kp->sk, sk, SK_SIZE);

    return kp;
}

/*
 * Get public key from key server
 */
uint8_t *get_pk_from_ks(char *username)
{
	size_t bin_len = PK_RAW_SIZE;
    unsigned char *bin = memalloc(bin_len);
	/* TEMPORARY */
	if (strcmp(username, "night") == 0) {
		sodium_hex2bin(bin, bin_len, "e2f0287d9c23aed8404dd8ba407e7dff8abe40fa98f0b9adf74904978a5fcd50", PK_RAW_SIZE * 2, NULL, NULL, NULL);
		return bin;
	} else if (strcmp(username, "palanix") == 0) {
		sodium_hex2bin(bin, bin_len, "932aee08aa338108e49f65a5c4f0eb0a08a15bf717fdf8c0ff60eefd0ea014ae", PK_RAW_SIZE * 2, NULL, NULL, NULL);
		return bin;
	}
	return NULL;
}
