#include "config.h"
#include "packet.h"
#include "util.h"
#include "client/ui.h"
#include "client/db.h"
#include "client/user.h"

sqlite3 *db;
char zen_db_path[PATH_MAX];

static int get_users_callback(void *_, int argc, char **argv, char **col_name)
{
	/*
	for(int i = 0; i < argc; i++) {
        printf("%s = %s\n", column[i], argv[i] ? argv[i] : "NULL");
    }
*/
	add_username(argv[0]);
    return 0;
}

void get_users()
{
	char *err_msg;

	if (sqlite3_open(zen_db_path, &db) != SQLITE_OK) {
		error(0, "Cannot open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	
	char *sql = "SELECT * FROM Keys;";

	if (sqlite3_exec(db, sql, get_users_callback, NULL, &err_msg)
			!= SQLITE_OK) {
		error(0, "Failed to exec statement: %s", err_msg);
		sqlite3_close(db);
	}

	sqlite3_close(db);
	return;
}
/*
 * Get shared key betweeen username
 */
uint8_t *get_sharedkey(uint8_t *username)
{
	sqlite3_stmt *statement;
	uint8_t *shared_key = memalloc(SHARED_KEY_SIZE);
	/* Make all bytes be 0 */
	memset(shared_key, 0, SHARED_KEY_SIZE);

	if (sqlite3_open(zen_db_path, &db) != SQLITE_OK) {
		error(0, "Cannot open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	
	char *sql = "SELECT SharedKey FROM Keys WHERE Username = ?;";

	if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) {
		error(0, "Failed to prepare statement: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}

	sqlite3_bind_text(statement, 1, username, strlen(username), SQLITE_STATIC);

	if (sqlite3_step(statement) == SQLITE_ROW) {
		const void *blob = sqlite3_column_blob(statement, 0);
		memcpy(shared_key, blob, SHARED_KEY_SIZE);
	} else {
		free(shared_key);
		/* Set it to NULL so it can be returned */
		shared_key = NULL;
	}

	sqlite3_finalize(statement);
	sqlite3_close(db);
	return shared_key;
}

/*
 * Saved shared key with username to database
 */
void save_sharedkey(uint8_t *username, uint8_t *shared_key)
{
	if (sqlite3_open(zen_db_path, &db) != SQLITE_OK) {
		error(0, "Cannot open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	
	sqlite3_stmt *statement;

	/* Statement to execute with values to be replaced */
	char *sql = "INSERT OR REPLACE INTO Keys (Username,SharedKey)"
		"VALUES (?,?);";
	printf("in db\n");
	print_bin(shared_key, SHARED_KEY_SIZE);
	if (sqlite3_prepare_v2(db, sql, -1, &statement, NULL) != SQLITE_OK) {
		error(0, "Failed to prepare statement: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}
	sqlite3_bind_text(statement, 1, username, strlen(username), SQLITE_STATIC);
    sqlite3_bind_blob(statement, 2, shared_key, SHARED_KEY_SIZE, SQLITE_STATIC);

	if (sqlite3_step(statement) != SQLITE_DONE) {
		error(0, "Failed to execute statement");
		sqlite3_close(db);
	}
	sqlite3_finalize(statement);
	sqlite3_close(db);
	write_log(LOG_INFO, "Saved shared key with %s to database", username);
}

void sqlite_init()
{
	char *err_msg;

	char *data_dir = replace_home(CLIENT_DATA_DIR);
	snprintf(zen_db_path, PATH_MAX, "%s/%s/data.db", data_dir, USERNAME);
	free(data_dir);
	if (access(zen_db_path, W_OK) != 0) {
		/* If data file doesn't exist, most likely data dir won't exist too */
		mkdir_p(zen_db_path);
	}

	if (sqlite3_open(zen_db_path, &db) != SQLITE_OK) {
		deinit();
		error(1, "Cannot open database: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
	}

	/* Create Keys Table if it is doesn't exist with Username being id */
	char *create_keys_table = "CREATE TABLE IF NOT EXISTS Keys("
		"Username TEXT NOT NULL, SharedKey BLOB NOT NULL,"
		"PRIMARY KEY(Username));";

    if (sqlite3_exec(db, create_keys_table, 0, 0, &err_msg) != SQLITE_OK) {
        error(0, "Cannot create Keys table: %s", err_msg);
        sqlite3_free(err_msg);        
    } else {
		write_log(LOG_INFO, "Keys Table created successfully");
    }
	
	sqlite3_close(db);
}
