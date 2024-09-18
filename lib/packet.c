#include "packet.h"
#include "key.h"
#include "util.h"

void print_packet(packet_t *pkt)
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
 * Requires manually free packet data
 * pkt: packet to fill data in (must be created via create_packet)
 * fd: file descriptor to read data from
 * required_type: Required packet type to receive, set 0 to not check
 */
int recv_packet(packet_t *pkt, int fd, uint8_t required_type)
{
    int status = ZSM_STA_SUCCESS;

    /* Read the packet components */
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
        error(0, "Invalid packet type");
        goto failure;
    }
    #if DEBUG == 1
        printf("Type: %d\n", pkt->type);
    #endif

    /* Not the same type as wanted to receive */
    if (pkt->type != required_type) {
        status = ZSM_STA_INVALID_TYPE;
        error(0, "Invalid packet type");
        goto failure;
    }

    if (pkt->length > MAX_DATA_LENGTH) {
        status = ZSM_STA_TOO_LONG;
        error(0, "Data too long: %d", pkt->length);
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

		/* Read data from the socket */
		if ((bytes_read = recv(fd, pkt->data, pkt->length, 0)) < 0) {
			status = ZSM_STA_READING_SOCKET;
			error(0, "Error reading from socket");
			free(pkt->data);
			goto failure;
		}
		if (bytes_read == 0) {
			error(0, "Closed connection");
			return ZSM_STA_READING_SOCKET;
		}
		if (bytes_read != pkt->length) {
			status = ZSM_STA_INVALID_LENGTH;
			error(0, "Invalid data length: bytes_read=%ld != pkt->length=%d", bytes_read, pkt->length);
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
	packet_t *error_pkt = create_packet(status, ZSM_TYP_ERROR, 0, NULL,
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
packet_t *create_packet(uint8_t status, uint8_t type, uint32_t length, uint8_t *data, uint8_t *signature)
{
    packet_t *pkt = memalloc(sizeof(packet_t));
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
int send_packet(packet_t *pkt, int fd)
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
void free_packet(packet_t *pkt)
{
    if (pkt->type != ZSM_TYP_AUTH && pkt->type != ZSM_TYP_ERROR) {
		if (pkt->signature != NULL) {
			free(pkt->signature);
		}
    }
	if (pkt->data != NULL) {
		free(pkt->data);
	}
    free(pkt);
}

/*
 * Wrapper for recv_packet to verify packet
 * Reads packet from fd, stores in pkt
 * TODO: pkt is unncessary
 */
int verify_packet(packet_t *pkt, int fd)
{
	if (recv_packet(pkt, fd, ZSM_TYP_MESSAGE) != ZSM_STA_SUCCESS) {
		close(fd);
		return ZSM_STA_ERROR_INTEGRITY;
	}

	uint8_t from[MAX_NAME], to[MAX_NAME];
	memcpy(from, pkt->data, MAX_NAME);

	/* TODO: replace with db operations */
	key_pair *kp_from = get_key_pair(from);
	
	/* Verify data confidentiality by signature */
	/* Verify data integrity by hash */
	uint8_t hash[HASH_SIZE];
    crypto_generichash(hash, HASH_SIZE, pkt->data, pkt->length, NULL, 0);

	if (crypto_sign_verify_detached(pkt->signature, hash, HASH_SIZE, kp_from->pk.bin) != 0) {
		/* Not match */
		error(0, "Cannot verify data integrity");
		packet_t *error_pkt = create_packet(ZSM_STA_ERROR_INTEGRITY, ZSM_TYP_ERROR, 0, NULL,
			create_signature(NULL, 0, NULL));
		send_packet(error_pkt, fd);
		free_packet(error_pkt);
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
