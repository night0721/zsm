#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <sodium.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 20249
#define BUFSIZE 1024

sqlite3 *db;

int init_db()
{
	char *err_msg = NULL;
	int rc = sqlite3_open("keys.db", &db);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
		return rc;
	}

	const char *sql = "CREATE TABLE IF NOT EXISTS keys (username TEXT PRIMARY KEY, publickey BLOB);";
	rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);
		return rc;
	}

	return SQLITE_OK;
}

/* 
 * Insert a public key with the given username into the database as a BLOB
 */
int insert_publickey(const char *username, const unsigned char *pubkey, size_t pubkey_len)
{
	sqlite3_stmt *stmt;
	const char *sql = "INSERT OR REPLACE INTO keys(username, publickey) VALUES(?, ?)";

	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to prepare statement\n");
		return rc;
	}

	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 2, publickey, pubkey_len, SQLITE_STATIC);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	return rc == SQLITE_DONE ? 0 : rc;
}

/* Retrieve a public key from the database by username */
unsigned char *retrieve_publickey(const char *username, size_t *pubkey_len)
{
	sqlite3_stmt *stmt;
	const char *sql = "SELECT publickey FROM keys WHERE username = ?";
	unsigned char *publickey = NULL;

	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "Failed to prepare statement\n");
		return NULL;
	}

	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
	rc = sqlite3_step(stmt);

	if (rc == SQLITE_ROW) {
		const void *blob = sqlite3_column_blob(stmt, 0);
		*publickey_len = sqlite3_column_bytes(stmt, 0);
		publickey = (unsigned char *)malloc(*pubkey_len);
		memcpy(publickey, blob, *pubkey_len);
	}

	sqlite3_finalize(stmt);
	return publickey;
}

/* Handle REQUEST command: generate a new key pair and store the public key with username */
void handle_request(int clientfd, const char *username)
{
	unsigned char pk[crypto_box_PUBLICKEYBYTES];
	unsigned char sk[crypto_box_SECRETKEYBYTES];

	crypto_box_keypair(pk, sk); /* Generate key pair */

	/* Store the public key with the username in the database */
	if (insert_publickey(username, pk, crypto_box_PUBLICKEYBYTES) == 0) {
		/* Convert the public key to hexadecimal string for client display */
		char publickey_hex[crypto_box_PUBLICKEYBYTES * 2 + 1];
		for (int i = 0; i < crypto_box_PUBLICKEYBYTES; i++) {
			sprintf(publickey_hex + i * 2, "%02x", pk[i]);
		}
		send(clientfd, publickey_hex, strlen(pubkey_hex), 0);
		send(clientfd, "\n", 1, 0);
	} else {
		send(clientfd, "ERROR\n", 6, 0);
	}
}

/* Handle RETRIEVE command: get public key by username */
void handle_retrieve(int clientfd, const char *username)
{
	size_t publickey_len;
	unsigned char *publickey = retrieve_pubkey(username, &pubkey_len);

	if (publickey) {
		/* Convert the public key (BLOB) to hexadecimal string for client display */
		char publickey_hex[pubkey_len * 2 + 1];
		for (size_t i = 0; i < publickey_len; i++) {
			sprintf(publickey_hex + i * 2, "%02x", pubkey[i]);
		}
		send(clientfd, publickey_hex, strlen(pubkey_hex), 0);
		send(clientfd, "\n", 1, 0);
		free(publickey);
	} else {
		send(clientfd, "NOT FOUND\n", 10, 0);
	}
}

void handle_client(int clientfd)
{
	char buffer[BUFSIZE];
	ssize_t bytes_received;

	while ((bytes_received = recv(clientfd, buffer, BUFSIZE - 1, 0)) > 0) {
		buffer[bytes_received] = '\0';

		if (strncmp(buffer, "REQUEST", 7) == 0) {
			char username[BUFSIZE];
			sscanf(buffer + 8, "%s", username); /* Get the username from the command */
			handle_request(clientfd, username);
		} else if (strncmp(buffer, "RETRIEVE", 8) == 0) {
			char username[BUFSIZE];
			sscanf(buffer + 9, "%s", username); /* Get the username from the command */
			handle_retrieve(clientfd, username);
		} else if (strncmp(buffer, "EXIT", 4) == 0) {
			send(clientfd, "BYE\n", 4, 0);
			break;
		} else {
			send(clientfd, "ERROR\n", 6, 0);
		}
	}

	close(clientfd);
}

int main()
{
	if (sodium_init() < 0) {
		fprintf(stderr, "Failed to initialize sodium\n");
		return -1;
	}

	if (init_db() != SQLITE_OK) {
		fprintf(stderr, "Failed to initialize database\n");
		return -1;
	}

	int serverfd, clientfd;
	struct sockaddr_in server_addr, client_addr;
	socklen_t addr_len = sizeof(client_addr);

	if ((serverfd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		perror("Socket failed");
		return -1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(PORT);

	if (bind(serverfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("Bind failed");
		close(serverfd);
		return -1;
	}

	if (listen(serverfd, 3) < 0) {
		perror("Listen failed");
		close(serverfd);
		return -1;
	}

	printf("Key server listening on port %d\n", PORT);

	while (1) {
		if ((clientfd = accept(serverfd, (struct sockaddr *)&client_addr, &addr_len)) < 0) {
			perror("Accept failed");
			continue;
		}

		handle_client(clientfd);
	}

	sqlite3_close(db);
	close(serverfd);
	return 0;
}

