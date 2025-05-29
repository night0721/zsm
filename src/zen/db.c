#include "config.h"
#include "packet.h"
#include "util.h"
#include "zen/ui.h"
#include "zen/db.h"
#include "zen/user.h"

sqlite3 *db;
char zen_db_path[PATH_MAX];

static int get_users_callback(void *_, int argc, char **argv, char **col_name)
{
	add_username(argv[0]);
	return 0;
}

void get_users(void)
{
	if (sqlite3_open(zen_db_path, &db) != SQLITE_OK) {
		deinit();
		error(1, "Cannot open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	char *sql = "SELECT Nickname FROM Users;";

	if (sqlite3_exec(db, sql, get_users_callback, NULL, NULL)
			!= SQLITE_OK) {
		error(0, "Failed to exec statement: %s", sqlite3_errmsg(db));
		write_log(LOG_ERROR, "Failed to get users");
	}
	sqlite3_close(db);
	return;
}

/*
 * Get receive key betweeen username
 */
uint8_t *get_receivekey(uint8_t *username)
{
	if (sqlite3_open(zen_db_path, &db) != SQLITE_OK) {
		deinit();
		error(1, "Cannot open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	sqlite3_stmt *statement;
	uint8_t *shared_key = memalloc(SHARED_KEY_SIZE);
	/* Make all bytes be 0 */
	memset(shared_key, 0, SHARED_KEY_SIZE);
	char *sql = "SELECT ReceiveKey FROM Users WHERE Username = ?;";

	if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) {
		error(0, "Failed to prepare statement: %s", sqlite3_errmsg(db));
		write_log(LOG_ERROR, "Failed to get receive key with %s: %s", username, sqlite3_errmsg(db));
		sqlite3_close(db);
		return NULL;
	}

	sqlite3_bind_text(statement, 1, username, strlen(username), SQLITE_STATIC);

	if (sqlite3_step(statement) == SQLITE_ROW) {
		const void *blob = sqlite3_column_blob(statement, 0);
		if (!blob) {
			write_log(LOG_ERROR, "Failed to get receive key with %s: %s", username, sqlite3_errmsg(db));
			free(shared_key);
			/* Set it to NULL so it can be returned */
			shared_key = NULL;
		} else {
			memcpy(shared_key, blob, SHARED_KEY_SIZE);
		}
	} else {
		write_log(LOG_ERROR, "Failed to get receive key with %s: %s", username, sqlite3_errmsg(db));
		free(shared_key);
		/* Set it to NULL so it can be returned */
		shared_key = NULL;
	}
	sqlite3_finalize(statement);
	sqlite3_close(db);
	return shared_key;
}

/*
 * Get send key between username
 */
uint8_t *get_sendkey(uint8_t *username)
{
	if (sqlite3_open(zen_db_path, &db) != SQLITE_OK) {
		deinit();
		error(1, "Cannot open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	sqlite3_stmt *statement;
	uint8_t *shared_key = memalloc(SHARED_KEY_SIZE);
	/* Make all bytes be 0 */
	memset(shared_key, 0, SHARED_KEY_SIZE);
	char *sql = "SELECT SendKey FROM Users WHERE Username = ?;";

	if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) {
		error(0, "Failed to prepare statement: %s", sqlite3_errmsg(db));
		write_log(LOG_ERROR, "Failed to get send key with %s: %s", username, sqlite3_errmsg(db));
		sqlite3_close(db);
		return NULL;
	}

	sqlite3_bind_text(statement, 1, username, strlen(username), SQLITE_STATIC);

	if (sqlite3_step(statement) == SQLITE_ROW) {
		const void *blob = sqlite3_column_blob(statement, 0);
		if (!blob) {
			write_log(LOG_ERROR, "Failed to get send key with %s: %s", username, sqlite3_errmsg(db));
			free(shared_key);
			/* Set it to NULL so it can be returned */
			shared_key = NULL;
		} else {
			memcpy(shared_key, blob, SHARED_KEY_SIZE);
		}
	} else {
		write_log(LOG_ERROR, "Failed to get send key with %s: %s", username, sqlite3_errmsg(db));
		free(shared_key);
		/* Set it to NULL so it can be returned */
		shared_key = NULL;
	}

	sqlite3_finalize(statement);
	sqlite3_close(db);
	return shared_key;
}

/*
 * Save receive key with username to database
 */
void save_receivekey(uint8_t *username, uint8_t *receive_key)
{
	if (sqlite3_open(zen_db_path, &db) != SQLITE_OK) {
		deinit();
		error(1, "Cannot open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	sqlite3_stmt *statement;

	/* Statement to execute with values to be replaced */
	char *sql = "INSERT OR REPLACE INTO Users(Username,Nickname,ReceiveKey)"
		"VALUES (?,?,?);";
	if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) {
		error(0, "Failed to prepare statement: %s", sqlite3_errmsg(db));
		write_log(LOG_ERROR, "Failed to save receive key with %s: %s", username, sqlite3_errmsg(db));
		return;
	}
	sqlite3_bind_text(statement, 1, username, strlen(username), SQLITE_STATIC);
	sqlite3_bind_text(statement, 2, username, strlen(username), SQLITE_STATIC);
	sqlite3_bind_blob(statement, 3, receive_key, SHARED_KEY_SIZE, SQLITE_STATIC);

	if (sqlite3_step(statement) != SQLITE_DONE) {
		error(0, "Failed to execute statement");
		write_log(LOG_ERROR, "Failed to save receive key with %s: %s", username, sqlite3_errmsg(db));
		return;
	}
	sqlite3_finalize(statement);
	write_log(LOG_INFO, "Saved receive key with %s to database", username);
	sqlite3_close(db);
}

/*
 * Save send key with username to database
 */
void save_sendkey(uint8_t *username, uint8_t *send_key)
{
	if (sqlite3_open(zen_db_path, &db) != SQLITE_OK) {
		deinit();
		error(1, "Cannot open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	sqlite3_stmt *statement;

	/* Statement to execute with values to be replaced */
	char *sql = "INSERT OR REPLACE INTO Users(Username,Nickname,SendKey)"
		"VALUES (?,?,?);";
	if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) {
		error(0, "Failed to prepare statement: %s", sqlite3_errmsg(db));
		write_log(LOG_ERROR, "Failed to save send key with %s: %s", username, sqlite3_errmsg(db));
		return;
	}
	sqlite3_bind_text(statement, 1, username, strlen(username), SQLITE_STATIC);
	sqlite3_bind_text(statement, 2, username, strlen(username), SQLITE_STATIC);
	sqlite3_bind_blob(statement, 3, send_key, SHARED_KEY_SIZE, SQLITE_STATIC);

	if (sqlite3_step(statement) != SQLITE_DONE) {
		error(0, "Failed to execute statement");
		write_log(LOG_ERROR, "Failed to save send key with %s: %s", username, sqlite3_errmsg(db));
		return;
	}
	sqlite3_finalize(statement);
	write_log(LOG_INFO, "Saved send key with %s to database", username);
	sqlite3_close(db);
}


/*
 * Update nickname in database
 */
void update_nickname(uint8_t *username, uint8_t *nickname)
{
	if (sqlite3_open(zen_db_path, &db) != SQLITE_OK) {
		deinit();
		error(1, "Cannot open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	sqlite3_stmt *statement;

	/* Statement to update the nickname for the existing user */
	char *sql = "UPDATE Users SET Nickname = ? WHERE Username = ?;";
	if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) {
		error(0, "Failed to prepare statement: %s", sqlite3_errmsg(db));
		write_log(LOG_ERROR, "Failed to update nickname with %s", username);
		return;
	}

	sqlite3_bind_text(statement, 1, nickname, strlen(nickname), SQLITE_STATIC);
	sqlite3_bind_text(statement, 2, username, strlen(username), SQLITE_STATIC);

	if (sqlite3_step(statement) != SQLITE_DONE) {
		error(0, "Failed to execute statement");
		write_log(LOG_ERROR, "Failed to update nickname with %s", username);
		return;
	}
	sqlite3_finalize(statement);
	write_log(LOG_INFO, "Updated nickname for user %s in the database", username);
	sqlite3_close(db);
}

/*
 * Get nickname of the user
 */
uint8_t *get_nickname(uint8_t *username)
{
	if (sqlite3_open(zen_db_path, &db) != SQLITE_OK) {
		deinit();
		error(1, "Cannot open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	sqlite3_stmt *statement;
	uint8_t *nickname = memalloc(PK_SIZE * 2 + 1);
	/* Make all bytes be 0 */
	memset(nickname, 0, PK_SIZE * 2 + 1);
	char *sql = "SELECT Nickname FROM Users WHERE Username = ?;";

	if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) {
		error(0, "Failed to prepare statement: %s", sqlite3_errmsg(db));
		write_log(LOG_ERROR, "Failed to get nickname with %s: %s", username, sqlite3_errmsg(db));
		sqlite3_close(db);
		return NULL;
	}

	sqlite3_bind_text(statement, 1, username, strlen(username), SQLITE_STATIC);

	if (sqlite3_step(statement) == SQLITE_ROW) {
		const void *blob = sqlite3_column_text(statement, 0);
		if (!blob) {
			write_log(LOG_ERROR, "Failed to get nickname with %s: %s", username, sqlite3_errmsg(db));
			free(nickname);
			/* Set it to NULL so it can be returned */
			nickname = NULL;
		} else {
			memcpy(nickname, blob, PK_SIZE * 2 + 1);
		}
	} else {
		write_log(LOG_ERROR, "Failed to get nickname with %s: %s", username, sqlite3_errmsg(db));
		free(nickname);
		/* Set it to NULL so it can be returned */
		nickname = NULL;
	}
	sqlite3_finalize(statement);
	sqlite3_close(db);
	return nickname;
}

/*
 * Save message to database
 */
void save_message(uint8_t *author, uint8_t *recipient, uint8_t *message, time_t timestamp)
{
	if (sqlite3_open(zen_db_path, &db) != SQLITE_OK) {
		deinit();
		error(1, "Cannot open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	sqlite3_stmt *statement;

	/* Statement to execute with values to be replaced */
	char *sql = "INSERT INTO Messages(author,recipient,message,timestamp)"
		"VALUES (?,?,?,?);";
	if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) {
		error(0, "Failed to prepare statement: %s", sqlite3_errmsg(db));
		write_log(LOG_ERROR, "Failed to save message with %s: %s", author, sqlite3_errmsg(db));
		return;
	}
	sqlite3_bind_text(statement, 1, author, strlen(author), SQLITE_STATIC);
	sqlite3_bind_text(statement, 2, recipient, strlen(recipient), SQLITE_STATIC);
	sqlite3_bind_text(statement, 3, message, strlen(message), SQLITE_STATIC);
	sqlite3_bind_int64(statement, 4, timestamp);

	if (sqlite3_step(statement) != SQLITE_DONE) {
		error(0, "Failed to execute statement");
		write_log(LOG_ERROR, "Failed to save message with %s: %s", author, sqlite3_errmsg(db));
		return;
	}
	sqlite3_finalize(statement);
	write_log(LOG_INFO, "Saved message with %s to database", author);
	sqlite3_close(db);
}

/*
 * Get messages between author and recipient
 */
void get_messages(uint8_t *author, uint8_t *recipient)
{
	if (sqlite3_open(zen_db_path, &db) != SQLITE_OK) {
		deinit();
		error(1, "Cannot open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	sqlite3_stmt *statement;
	char *sql = "SELECT author,recipient,message,timestamp FROM Messages WHERE (author = ? AND recipient = ?) OR (author = ? AND recipient = ?) ORDER BY timestamp ASC;";

	if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) {
		error(0, "Failed to prepare statement: %s", sqlite3_errmsg(db));
		write_log(LOG_ERROR, "Failed to get messages with %s: %s", author, sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	sqlite3_bind_text(statement, 1, author, strlen(author), SQLITE_STATIC);
	sqlite3_bind_text(statement, 2, recipient, strlen(recipient), SQLITE_STATIC);
	sqlite3_bind_text(statement, 3, recipient, strlen(recipient), SQLITE_STATIC);
	sqlite3_bind_text(statement, 4, author, strlen(author), SQLITE_STATIC);

	while (sqlite3_step(statement) == SQLITE_ROW) {
		const void *author = sqlite3_column_text(statement, 0);
		const void *recipient = sqlite3_column_text(statement, 1);
		const void *message = sqlite3_column_text(statement, 2);
		time_t timestamp = sqlite3_column_int64(statement, 3);

		if (!author || !recipient || !message) {
			write_log(LOG_ERROR, "Failed to get messages with %s: %s", author, sqlite3_errmsg(db));
			continue;
		}
		print_message((uint8_t *)author, (uint8_t *)message, timestamp);
	}
	sqlite3_finalize(statement);
	sqlite3_close(db);
}

/*
 * Delete all messages from database
 */
void clear_messages(void)
{
	if (sqlite3_open(zen_db_path, &db) != SQLITE_OK) {
		deinit();
		error(1, "Cannot open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	char *sql = "DELETE FROM Messages;";

	if (sqlite3_exec(db, sql, 0, 0, NULL) != SQLITE_OK) {
		error(0, "Failed to exec statement: %s", sqlite3_errmsg(db));
		write_log(LOG_ERROR, "Failed to clear messages");
	}
	sqlite3_close(db);
}

/*
 * Initialize the database
 */
void sqlite_init(void)
{
	char *data_dir = replace_home(CLIENT_DATA_DIR);

	if (access(data_dir, W_OK) != 0) {
		/* If data dir doesn't exist, create it */
		mkdir_p(data_dir);
	}
	snprintf(zen_db_path, PATH_MAX, "%s/data.db", data_dir);
	free(data_dir);

	if (sqlite3_open(zen_db_path, &db) != SQLITE_OK) {
		deinit();
		error(1, "Cannot open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	
	/* Create table if it is doesn't exist with Username being id */
	char *create_users_table = "CREATE TABLE IF NOT EXISTS Users("
		"Username TEXT PRIMARY KEY NOT NULL, Nickname TEXT NOT NULL, ReceiveKey BLOB, SendKey BLOB);";

	char *create_messages_table = "CREATE TABLE IF NOT EXISTS Messages("
		"id INTEGER PRIMARY KEY AUTOINCREMENT," /* Unique message identifier */
		"author TEXT NOT NULL," /* Username of the sender */
		"recipient TEXT NOT NULL, " /* Username of the recipient */
		"message TEXT NOT NULL," /* Content of the message */
		"timestamp DATETIME DEFAULT CURRENT_TIMESTAMP," /* Time when the message was sent */
		"FOREIGN KEY (author) REFERENCES Users(Username)," /* Validate sender against Users */
		"FOREIGN KEY (recipient) REFERENCES Users(Username));"; /* Validate recipient against Users */

	if (sqlite3_exec(db, create_users_table, 0, 0, NULL) != SQLITE_OK) {
		error(1, "Cannot create Users table: %s", sqlite3_errmsg(db));
	} else {
		write_log(LOG_INFO, "Users Table created successfully");
	}
	if (sqlite3_exec(db, create_messages_table, 0, 0, NULL) != SQLITE_OK) {
		error(1, "Cannot create Messages table: %s", sqlite3_errmsg(db));
	} else {
		write_log(LOG_INFO, "Messages Table created successfully");
	}
	sqlite3_close(db);
}
