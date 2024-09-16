#include "packet.h"
#include "key.h"
#include "util.h"

void print_packet(packet *pkt)
{
    printf("Status: %d\n", pkt->status);
    printf("Type: %d\n", pkt->type);
    printf("Length: %d\n", pkt->length);
	if (pkt->length > 0) {
		printf("Data:\n");
		for (int i = 0; i < pkt->length; i++) {
			printf("%d ", pkt->data[i]);
		}
		printf("\n");
		printf("Signature:\n");
		for (int i = 0; i < SIGN_SIZE; i++) {
			printf("%d ", pkt->signature[i]);
		}
		printf("\n");
	}
}

/*
 * Requires manually free message data
 * pkt: packet to fill data in (must be created via create_packet)
 * fd: file descriptor to read data from
 * required_type: Required packet type to receive, set 0 to not check
 */
int recv_packet(packet *pkt, int fd, uint8_t required_type)
{
    int status = ZSM_STA_SUCCESS;

    /* Read the message components */
    if (recv(fd, &pkt->status, sizeof(pkt->status), 0) < 0 ||
        recv(fd, &pkt->type, sizeof(pkt->type), 0) < 0 ||
        recv(fd, &pkt->length, sizeof(pkt->length), 0) < 0) {
        status = ZSM_STA_READING_SOCKET;
        error(0, "Error reading from socket");
    }
    #if DEBUG == 1
	    printf("==========PACKET RECEIVED==========\n");
        printf("Status: %d\n", pkt->status);
    #endif

    if (pkt->type > 0xFF || pkt->type < 0x0) {
        status = ZSM_STA_INVALID_TYPE;
        error(0, "Invalid message type");
        goto failure;
    }
    #if DEBUG == 1
        printf("Type: %d\n", pkt->type);
    #endif

    /* Not the same type as wanted to receive */
    if (pkt->type != required_type) {
        status = ZSM_STA_INVALID_TYPE;
        error(0, "Invalid message type");
        goto failure;
    }

    if (pkt->length > MAX_MESSAGE_LENGTH) {
        status = ZSM_STA_TOO_LONG;
        error(0, "Message too long: %d", pkt->length);
        goto failure;
    }
    #if DEBUG == 1
        printf("Length: %d\n", pkt->length);
    #endif
	size_t bytes_read = 0;

	/* If packet's length is 0, ignore its data and signature as it is information from server */
	if (pkt->type != ZSM_TYP_INFO && pkt->length > 0) {
		pkt->data = memalloc((pkt->length + 1) * sizeof(char));
		if (pkt->data == NULL) {
			status = ZSM_STA_MEMORY_ALLOCATION;
			goto failure;
		}

		/* Read message data from the socket */
		if ((bytes_read = recv(fd, pkt->data, pkt->length, 0)) < 0) {
			status = ZSM_STA_READING_SOCKET;
			error(0, "Error reading from socket");
			free(pkt->data);
			goto failure;
		}
		if (bytes_read != pkt->length) {
			status = ZSM_STA_INVALID_LENGTH;
			error(0, "Invalid message length: bytes_read=%ld != pkt->length=%d", bytes_read, pkt->length);
			free(pkt->data);
			goto failure;
		}
		pkt->data[pkt->length] = '\0';
		#if DEBUG == 1
			printf("Data:\n");
			for (int i = 0; i < pkt->length; i++) {
				printf("%d ", pkt->data[i]);
			}
			printf("\n");
		#endif

		pkt->signature = memalloc((SIGN_SIZE + 1) * sizeof(char));
		if (pkt->signature == NULL) {
			status = ZSM_STA_MEMORY_ALLOCATION;
			goto failure;
		}

		if ((bytes_read = recv(fd, pkt->signature, SIGN_SIZE, 0)) < 0) {
			status = ZSM_STA_READING_SOCKET;
			error(0, "Error reading from socket");
			free(pkt->data);
			goto failure;
		}
		/* Don't check signature if the packet is emtpy */
		if (pkt->length > 0 && bytes_read != SIGN_SIZE) {
			status = ZSM_STA_INVALID_LENGTH;
			error(0, "Invalid signature length: bytes_read=%ld != SIGN_SIZE(32)", bytes_read);
			free(pkt->data);
			goto failure;
		}
		pkt->signature[SIGN_SIZE] = '\0';

		#if DEBUG == 1
			printf("Signature:\n");
			for (int i = 0; i < SIGN_SIZE; i++) {
				printf("%d ", pkt->signature[i]);
			}
			printf("\n");
		#endif
	}
	#if DEBUG == 1
        printf("==========END RECEIVING============\n");
    #endif

    return status;

failure:;
	packet *error_pkt = create_packet(status, ZSM_TYP_ERROR, 0, NULL,
			create_signature(NULL, 0, NULL));

    if (send_packet(error_pkt, fd) != ZSM_STA_SUCCESS) {
        /* Resend it? */
        error(0, "Failed to send error packet. Error status => %d", status);
    }
    free_packet(error_pkt);
    return status;
}

/*
 * Creates a packet for receive or send
 * Requires heap allocated data
 */
packet *create_packet(uint8_t status, uint8_t type, uint32_t length, uint8_t *data, uint8_t *signature)
{
    packet *pkt = memalloc(sizeof(packet));
    pkt->status = status;
    pkt->type = type;
    pkt->length = length;
    pkt->data = data;
    pkt->signature = signature;
    return pkt;
}

/*
 * Sends packet to fd
 * Requires heap allocated data
 * Close file descriptor and free data on failure
 */
int send_packet(packet *pkt, int fd)
{
    int status = ZSM_STA_SUCCESS;
    uint32_t length = pkt->length;
    if (send(fd, &pkt->status, sizeof(pkt->status), 0) <= 0 ||
        send(fd, &pkt->type, sizeof(pkt->type), 0) <= 0 ||
        send(fd, &pkt->length, sizeof(pkt->length), 0) <= 0)
	{
		goto failure;
	}
	if (pkt->type != ZSM_TYP_INFO && pkt->length > 0 && pkt->data != NULL) {
		if (send(fd, pkt->data, length, 0) <= 0) goto failure;
		if (send(fd, pkt->signature, SIGN_SIZE, 0) <= 0) goto failure;
	}
	
    #if DEBUG == 1
        printf("==========PACKET SENT============\n");
        print_packet(pkt);
        printf("==========END SENT===============\n");
    #endif
    return status;

failure:
	/* Or we could resend it? */
	status = ZSM_STA_WRITING_SOCKET;
	error(0, "Error writing to socket");
	free(pkt->data);
	close(fd);
	return status;
}

/*
 * Free allocated memory in packet
 */
void free_packet(packet *pkt)
{
    if (pkt->type != ZSM_TYP_AUTH) {
		if (pkt->signature != NULL) {
			free(pkt->signature);
		}
    }
    free(pkt->data);
    free(pkt);
}

/* 
 * not going to stay
 */
char *getuserinput()
{
    printf("Enter message to send: ");
    fflush(stdout);
    char *line = memalloc(1024);
    line[0] = '\0';
    size_t length = strlen(line);
    while (length <= 1) {
        fgets(line, 1024, stdin);
        length = strlen(line);
    }
    length -= 1;
    line[length] = '\0';
    return line;
}

/* 
 * not going to stay
 */
char *getrecipient()
{
    printf("Enter recipient: ");
    fflush(stdout);
    char *line = memalloc(32);
    line[0] = '\0';
    size_t length = strlen(line);
    while (length <= 1) {
        fgets(line, 1024, stdin);
        length = strlen(line);
    }
    length -= 1;
    line[length] = '\0';
    return line;
}


int encrypt_packet(int sockfd, key_pair *kp)
{
    int status = ZSM_STA_SUCCESS;
	char *line = getuserinput();
	uint8_t *recipient = getrecipient();
	uint32_t data_len;
	uint8_t *raw_data = memalloc(8192);
	size_t length = strlen(recipient);
	size_t length_line = strlen(line);
	if (length < MAX_NAME) {
		/* Pad with null characters up to max length */
		memset(recipient + length, 0, MAX_NAME - length);
	}
	memcpy(raw_data, line, length_line);
	size_t raw_data_size = MAX_NAME * 2 + strlen(line);
	
	uint8_t *data = encrypt_data(kp->pk.username, recipient, raw_data, raw_data_size, &data_len);
	uint8_t *signature = create_signature(data, data_len, &kp->sk);
	packet *pkt = create_packet(1, ZSM_TYP_MESSAGE, data_len, data, signature);
	if ((status = send_packet(pkt, sockfd)) != ZSM_STA_SUCCESS) {
		close(sockfd);
		return status;
	}
	free(recipient);
	free(line);
	free_packet(pkt);
	return status;
}

/*
 * Wrapper for recv_packet to verify packet
 * Reads packet from fd, stores in pkt
 * TODO: pkt is unncessary
 */
packet *verify_packet(packet *pkt, int fd)
{
	if (recv_packet(pkt, fd, ZSM_TYP_MESSAGE) != ZSM_STA_SUCCESS) {
		close(fd);
		return NULL;
	}

	uint8_t from[MAX_NAME], to[MAX_NAME];
	memcpy(from, pkt->data, MAX_NAME);
	/* TODO: replace with db operations */
	key_pair *kp_from = get_key_pair(from);
	
    if (verify_integrity(pkt, &kp_from->pk) != ZSM_STA_SUCCESS) {
        free(pkt->data);
        free(pkt->signature);
		packet *error_pkt = create_packet(ZSM_STA_ERROR_INTEGRITY, ZSM_TYP_ERROR, 0, NULL,
			create_signature(NULL, 0, NULL));
		send_packet(error_pkt, fd);
		free_packet(error_pkt);
		return NULL;
    }
	return pkt;
}

/*
 * Encrypt raw with raw_length using to
 * length is set to sum length of random bytes and scrambled data
 */
uint8_t *encrypt_data(uint8_t *from, uint8_t *to, uint8_t *raw, uint32_t raw_length, uint32_t *length)
{
	key_pair *kp_from = get_key_pair(from);
	key_pair *kp_to = get_key_pair(to);

	uint8_t shared_key[SHARED_SIZE];
	if (crypto_kx_client_session_keys(shared_key, NULL,
                                  kp_from->pk.bin, kp_from->sk.bin, kp_to->pk.bin) != 0) {
		/* Suspicious server public key, bail out */
		error(0, "Error performing key exchange");
	}

    uint8_t nonce[NONCE_SIZE];
    uint32_t encrypted_len = raw_length + ADDITIONAL_SIZE;
    uint8_t encrypted[encrypted_len];
    
    /* Generate random nonce(number used once) */
    randombytes_buf(nonce, sizeof(nonce));
    crypto_aead_xchacha20poly1305_ietf_encrypt(encrypted, NULL, raw, 
			raw_length, NULL, 0, NULL, nonce, shared_key);
    size_t data_len = MAX_NAME * 2 + NONCE_SIZE + encrypted_len;
    *length = data_len;
    uint8_t *data = memalloc(data_len * sizeof(uint8_t));
    memcpy(data, kp_from->sk.username, MAX_NAME);
    memcpy(data + MAX_NAME, kp_to->sk.username, MAX_NAME);
    memcpy(data + MAX_NAME * 2, nonce, NONCE_SIZE);
    memcpy(data + MAX_NAME * 2 + NONCE_SIZE, encrypted, encrypted_len);
    
    return data;
}

/*
 * Should be used by clients
 */
uint8_t *decrypt_data(packet *pkt)
{
    size_t encrypted_len = pkt->length - NONCE_SIZE - MAX_NAME * 2;
    size_t data_len = encrypted_len - ADDITIONAL_SIZE;
	uint8_t nonce[NONCE_SIZE], from[MAX_NAME], to[MAX_NAME], encrypted[encrypted_len];
    uint8_t *decrypted = memalloc((data_len + 1) * sizeof(uint8_t));
	memcpy(from, pkt->data, MAX_NAME);
	memcpy(to, pkt->data + MAX_NAME, MAX_NAME);
    memcpy(nonce, pkt->data + MAX_NAME * 2, NONCE_SIZE);
    memcpy(encrypted, pkt->data + MAX_NAME * 2 + NONCE_SIZE, encrypted_len);
       
	key_pair *kp_from = get_key_pair(from);
	key_pair *kp_to = get_key_pair(to);

	uint8_t shared_key[SHARED_SIZE];
	if (crypto_kx_client_session_keys(shared_key, NULL,
                                  kp_from->pk.bin, kp_from->sk.bin, kp_to->pk.bin) != 0) {
		/* Suspicious server public key, bail out */
		error(0, "Error performing key exchange");
	}

    /* We don't need it anymore */
    free(pkt->data);
    if (crypto_aead_xchacha20poly1305_ietf_decrypt(decrypted, NULL,
                                                    NULL, encrypted, 
                                                    encrypted_len,
                                                    NULL, 0,
                                                    nonce, shared_key) != 0) {
        free(decrypted);
        error(0, "Cannot decrypt message");
        return NULL;
    } else {
        /* Terminate decrypted message so we don't print random bytes */
        decrypted[data_len] = '\0';
		printf("<%s> to <%s>: %s\n", from, to, decrypted);
		return decrypted;
    }
}


/*
 * Verify message integrity + confidentiality
 * Verify using public key against hashed message
 */
int verify_integrity(packet *pkt, public_key *pk)
{
    uint8_t hash[HASH_SIZE];

    /* Hash data to check if matches user provided correct signature */

    crypto_generichash(hash, HASH_SIZE,
                   pkt->data, pkt->length,
                   NULL, 0);

	if (crypto_sign_verify_detached(pkt->signature, hash, HASH_SIZE, pk->bin) != 0) {
		/* Not match */
		error(0, "Cannot verify message integrity");
        return ZSM_STA_ERROR_INTEGRITY;
	}

    return ZSM_STA_SUCCESS;
}

/*
 * Create signature for packet
 * When data, secret is null, length is 0, empty siganture is created
 */
uint8_t *create_signature(uint8_t *data, uint32_t length, secret_key *sk)
{
	uint8_t *signature = memalloc(SIGN_SIZE * sizeof(uint8_t));
	if (data == NULL && length == 0 && sk == NULL) {
		/* From server, give fake signature */
		memset(signature, 0, SIGN_SIZE * sizeof(uint8_t));
	} else {
		uint8_t hash[HASH_SIZE];

		/* Hash data to check if matches user provided correct signature */
		crypto_generichash(hash, HASH_SIZE,
				data, length,
				NULL, 0);
		crypto_sign_detached(signature, NULL, hash, HASH_SIZE, sk->bin);
	}

    return signature;
}
