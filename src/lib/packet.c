#include "packet.h"
#include "key.h"
#include "util.h"

/*
 * Print contents of packet
 */
void print_packet(packet_t *pkt)
{
	printf("Packet:\n");
	printf("Type: %d\n", pkt->type);
	printf("Length: %d\n", pkt->length);
	if (pkt->length > 0) {
		printf("Data:\n");
		print_bin(pkt->data, pkt->length);
		printf("Signature:\n");
		print_bin(pkt->signature, SIGN_SIZE);
	}
}
/*
 * Requires manually free packet data
 * pkt: packet to fill data in (must be created via create_packet)
 * fd: file descriptor to read data from
 */
int recv_packet(packet_t *pkt, int fd)
{
	int status = ZSM_STA_SUCCESS;
	size_t bytes_read = 0;

	/* Buffer to store header (type, length) */
	size_t header_len = sizeof(pkt->type) + sizeof(pkt->length);
	uint8_t header[header_len];
	
	/* Read the entire packet header in one system call */
	if ((bytes_read = recv(fd, header, header_len, 0)) != header_len) {
		status = (bytes_read == 0) ? ZSM_STA_CLOSED_CONNECTION : ZSM_STA_READING_SOCKET;
		error(0, "Error reading packet header from socket, bytes_read(%d)!=header_len(%d), status => %d", 
				bytes_read, header_len, status);
		return status;
	}

	/* Unpack the header */
	memcpy(&pkt->type, &header[0], sizeof(pkt->type));
	memcpy(&pkt->length, &header[sizeof(pkt->type)], sizeof(pkt->length));

	
	/* Validate the packet type and length */
	if (pkt->type > 0xFF || pkt->type < 0x0) {
		status = ZSM_STA_INVALID_TYPE;
		error(0, "Invalid packet type: %d", pkt->type);
		goto failure;
	}

	if (pkt->length > MAX_DATA_LENGTH) {
		status = ZSM_STA_TOO_LONG;
		error(0, "Data too long: %d", pkt->length);
		goto failure;
	}

	/* If packet's length is 0, ignore its data and signature as it is information from server */
	if (pkt->type != ZSM_TYP_INFO && pkt->length > 0) {
		pkt->data = memalloc(pkt->length + 1);
		pkt->signature = memalloc(SIGN_SIZE);

		if (!pkt->data || !pkt->signature) {
			status = ZSM_STA_MEMORY_ALLOCATION;
			goto failure;
		}
		
		size_t payload_len = pkt->length + SIGN_SIZE;
		uint8_t payload[payload_len];

		/* Read data and signature from the socket in one sys call */
		if ((bytes_read = recv(fd, payload, payload_len, 0)) != payload_len) {
			status = (bytes_read == 0) ? ZSM_STA_CLOSED_CONNECTION : ZSM_STA_READING_SOCKET;
			error(0, "Error reading from socket, status => %d", status);
			free(pkt->data);
			free(pkt->signature);
			goto failure;
		}
		
		/* Unpack paylaod */
		memcpy(pkt->data, payload, pkt->length);
		memcpy(pkt->signature, payload + pkt->length, SIGN_SIZE);
		/* Null terminate data so it can be print */
		pkt->data[pkt->length] = '\0';
	}
	return status;

failure:;
	packet_t *error_pkt = create_packet(status, 0, NULL, NULL);

	/* should we send or not send ? */
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
packet_t *create_packet(uint8_t type, uint32_t length, uint8_t *data, uint8_t *signature)
{
	packet_t *pkt = memalloc(sizeof(packet_t));
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
	size_t bytes_sent = 0;

	/* Buffer to store header (type, length) */
	size_t header_len = sizeof(pkt->type) + sizeof(pkt->length);
	uint8_t header[header_len];

	/* Pack the header */
	memcpy(&header[0], &pkt->type, sizeof(pkt->type));
	memcpy(&header[sizeof(pkt->type)], &pkt->length, sizeof(pkt->length));

	 /* Send the packet header in one system call */
	if ((bytes_sent = send(fd, header, header_len, 0)) != header_len) {
		status = ZSM_STA_WRITING_SOCKET;
		error(0, "Error writing packet header to socket, bytes_sent(%d)!=header_len(%d), status => %d", 
				bytes_sent, header_len, status);
		goto failure;
	}

	if (pkt->type != ZSM_TYP_INFO && pkt->type != ZSM_TYP_ERROR && pkt->length > 0 && pkt->data != NULL) {
		/* Buffer to store payload (data + signature) */
		size_t payload_len = pkt->length + SIGN_SIZE;
		uint8_t payload[payload_len];

		/* Pack the payload */
		memcpy(payload, pkt->data, pkt->length);
		memcpy(payload + pkt->length, pkt->signature, SIGN_SIZE);
		
		/* Send the payload (data + signature) in one system call */
		if ((bytes_sent = send(fd, payload, payload_len, 0)) != payload_len) {
			status = ZSM_STA_WRITING_SOCKET;
			error(0, "Error writing packet body to socket, bytes_sent(%d)!=payload_len(%d), status => %d", 
					bytes_sent, payload_len, status);
			goto failure;
		}
	}
	return status;

failure:
	/* Or we could resend it? */
	status = ZSM_STA_WRITING_SOCKET;
	error(0, "Error writing to socket");
	free_packet(pkt);
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
 */
int verify_packet(packet_t *pkt, int fd)
{
	int status = recv_packet(pkt, fd);
	if (status != ZSM_STA_SUCCESS) {
		return status;
	}
	if (pkt->type != ZSM_TYP_MESSAGE) {
		/* Handle if wrong type */
		return ZSM_STA_INVALID_TYPE;
	}

	uint8_t from[MAX_NAME];
	memcpy(from, pkt->data, MAX_NAME);

	/* Verify data confidentiality by signature */
	/* Verify data integrity by hash */
	uint8_t hash[HASH_SIZE];
	crypto_generichash(hash, HASH_SIZE, pkt->data, pkt->length, NULL, 0);

	if (crypto_sign_verify_detached(pkt->signature, hash, HASH_SIZE, from) != 0) {
		/* Not match */
		error(0, "Cannot verify data integrity");
		packet_t *error_pkt = create_packet(ZSM_STA_ERROR_INTEGRITY, 0, NULL, NULL);
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
uint8_t *create_signature(uint8_t *data, uint32_t length, uint8_t *sk)
{
	uint8_t *signature = memalloc(SIGN_SIZE);
	if (data == NULL && length == 0 && sk == NULL) {
		/* From server, give fake signature */
		memset(signature, 0, SIGN_SIZE);
	} else {
		uint8_t hash[HASH_SIZE];

		/* Hash data to check if matches user provided correct signature */
		crypto_generichash(hash, HASH_SIZE,
				data, length,
				NULL, 0);
		crypto_sign_detached(signature, NULL, hash, HASH_SIZE, sk);
	}

	return signature;
}
