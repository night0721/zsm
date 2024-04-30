#include "packet.h"

/*
 * msg is the error message to print to stderr
 * will include error message from function if errno isn't 0
 * end program is fatal is 1
 */
void error(int fatal, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    /* to preserve errno */
    int errsv = errno;

    /* Determine the length of the formatted error message */
    va_list args_copy;
    va_copy(args_copy, args);
    size_t error_len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    /* 7 for [zsm], space and null */
    char errorstr[error_len + 1];
    vsnprintf(errorstr, error_len + 1, fmt, args);
    fprintf(stderr, "[zsm] ");

    if (errsv != 0) {
        perror(errorstr);
        errno = 0;
    } else {
        fprintf(stderr, "%s\n", errorstr);
    }
    
    va_end(args);
    if (fatal) exit(1);
}

void *memalloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr) {
        error(0, "Error allocating memory"); 
        return NULL;
    }
    return ptr;
}

void *estrdup(void *str)
{
    void *modstr = strdup(str);
    if (modstr == NULL) {
        error(0, "Error allocating memory");
        return NULL;
    }
    return modstr;
}

uint8_t *get_public_key(int sockfd)
{
    message keyex_msg;
    if (recv_packet(&keyex_msg, sockfd) != ZSM_STA_SUCCESS) {
        /* We can't do anything if key exchange already failed */
        close(sockfd);
        return NULL;
    } else {
        int status = 0;
        /* Check to see if the content is actually a key */
        if (keyex_msg.type != ZSM_TYP_KEY) {
            status = ZSM_STA_INVALID_TYPE;
        }
        if (keyex_msg.length != PUBLIC_KEY_SIZE) {
            status = ZSM_STA_WRONG_KEY_LENGTH;
        }
        if (status != 0) {
            free(keyex_msg.data);
            message *error_msg = create_error_packet(status);
            send_packet(error_msg, sockfd);
            free_packet(error_msg);
            close(sockfd);
            return NULL;
        }
    }
    /* Obtain public key from packet */
    uint8_t *pk = memalloc(PUBLIC_KEY_SIZE * sizeof(char));
    memcpy(pk, keyex_msg.data, PUBLIC_KEY_SIZE);
    if (pk == NULL) {
        free(keyex_msg.data);
        /* Fatal, we couldn't complete key exchange */
        close(sockfd);
        return NULL;
    }
    free(keyex_msg.data);
    return pk;
}

int send_public_key(int sockfd, uint8_t *pk)
{
    /* send_packet requires heap allocated buffer */
    uint8_t *pk_dup = memalloc(PUBLIC_KEY_SIZE * sizeof(char));
    memcpy(pk_dup, pk, PUBLIC_KEY_SIZE);
    if (pk_dup == NULL) {
        close(sockfd);
        return -1;
    }

    /* Sending our public key to client */
    /* option???? */
    message *keyex = create_packet(1, ZSM_TYP_KEY, PUBLIC_KEY_SIZE, pk_dup);
    send_packet(keyex, sockfd);
    free_packet(keyex);
    return 0;
}

void print_packet(message *msg)
{
    printf("Option: %d\n", msg->option);
    printf("Type: %d\n", msg->type);
    printf("Length: %lld\n", msg->length);
    printf("Data: %s\n\n", msg->data);
}

/*
 * Requires manually free message data
 */
int recv_packet(message *msg, int fd)
{
    int status = ZSM_STA_SUCCESS;

    /* Read the message components */
    if (recv(fd, &msg->option, sizeof(msg->option), 0) < 0 ||
        recv(fd, &msg->type, sizeof(msg->type), 0) < 0 ||
        recv(fd, &msg->length, sizeof(msg->length), 0) < 0) {
        status = ZSM_STA_READING_SOCKET;
        error(0, "Error reading from socket");
    }
    #if DEBUG == 1
        printf("==========PACKET RECEIVED==========\n");
    #endif
    #if DEBUG == 1
        printf("Option: %d\n", msg->option);
    #endif

    if (msg->type > 0xFF || msg->type < 0x0) {
        status = ZSM_STA_INVALID_TYPE;
        error(0, "Invalid message type");
        goto failure;
    }
    #if DEBUG == 1
        printf("Type: %d\n", msg->type);
    #endif

    /* Convert message length from network byte order to host byte order */
    if (msg->length > MAX_MESSAGE_LENGTH) {
        status = ZSM_STA_TOO_LONG;
        error(0, "Message too long: %lld", msg->length);
        goto failure;
    }
    #if DEBUG == 1
        printf("Length: %lld\n", msg->length);
    #endif

    // Allocate memory for message data
    msg->data = memalloc((msg->length + 1) * sizeof(char));
    if (msg->data == NULL) {
        status = ZSM_STA_MEMORY_ALLOCATION;
        goto failure;
    }

    /* Read message data from the socket */
    size_t bytes_read = 0;
    if ((bytes_read = recv(fd, msg->data, msg->length, 0)) < 0) {
        status = ZSM_STA_READING_SOCKET;
        error(0, "Error reading from socket");
        free(msg->data);
        goto failure;
    }
    if (bytes_read != msg->length) {
        status = ZSM_STA_INVALID_LENGTH;
        error(0, "Invalid message length: bytes_read=%ld != msg->length=%lld", bytes_read, msg->length);
        free(msg->data);
        goto failure;
    }
    msg->data[msg->length] = '\0';

    #if DEBUG == 1
        printf("Data: %s\n\n", msg->data);
    #endif

    return status;
failure:;
    message *error_msg = create_error_packet(status);
    if (send_packet(error_msg, fd) != ZSM_STA_SUCCESS) {
        /* Resend it? */
        error(0, "Failed to send error packet to peer. Error status => %d", status);
    }
    free_packet(error_msg);
    return status;
}

message *create_error_packet(int code)
{
    char *err = memalloc(ERROR_LENGTH * sizeof(char));
    switch (code) {
        case ZSM_STA_INVALID_TYPE:
            strcpy(err, "Invalid message type     ");
            break;
        case ZSM_STA_INVALID_LENGTH:
            strcpy(err, "Invalid message length   ");
            break;
        case ZSM_STA_TOO_LONG:
            strcpy(err, "Message too long         ");
            break;
        case ZSM_STA_READING_SOCKET:
            strcpy(err, "Error reading from socket");
            break;
        case ZSM_STA_WRITING_SOCKET:
            strcpy(err, "Error writing to socket  ");
            break;
        case ZSM_STA_UNKNOWN_USER:
            strcpy(err, "Unknwon user             ");
            break;
        case ZSM_STA_WRONG_KEY_LENGTH:
            strcpy(err, "Wrong public key length  ");
            break;
    }
    return create_packet(1, ZSM_TYP_ERROR, ERROR_LENGTH, err);
}

/*
 * Requires heap allocated msg data
 */
message *create_packet(uint8_t option, uint8_t type, uint32_t length, char *data)
{
    message *msg = memalloc(sizeof(message));
    msg->option = option;
    msg->type = type;
    msg->length = length;
    msg->data = data;
    return msg;
}

/*
 * Requires heap allocated msg data
 */
int send_packet(message *msg, int fd)
{
    int status = ZSM_STA_SUCCESS;
    uint32_t length = msg->length;
    // Send the message back to the client
    if (send(fd, &msg->option, sizeof(msg->option), 0) <= 0 ||
        send(fd, &msg->type, sizeof(msg->type), 0) <= 0 ||
        send(fd, &msg->length, sizeof(msg->length), 0) <= 0 ||
        send(fd, msg->data, length, 0) <= 0) {
        status = ZSM_STA_WRITING_SOCKET;
        error(0, "Error writing to socket");
        //free(msg->data);
        close(fd); // Close the socket and continue accepting connections
    }
    #if DEBUG == 1
        printf("==========PACKET SENT==========\n");
        print_packet(msg);
    #endif
    return status;
}

void free_packet(message *msg)
{
    if (msg->type != 0x10) {
        /* temp solution, dont use stack allocated msg to send to client */
        free(msg->data);
    }
    free(msg);
}
