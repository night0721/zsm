#include "config.h"
#include "packet.h"
#include "util.h"
#include "client/ui.h"
#include "client/db.h"
#include "client/user.h"

static int callback(void *ignore, int argc, char **argv, char **azColName)
{
    char *username = memalloc(MAX_NAME);
	strcpy(username, argv[0]);
	/* Add only if it isn't talking yourself */
	if (strncmp(username, USERNAME, MAX_NAME))
		add_username(username);

    return 0;
}

static int get_shared_key(void *ignore, int argc, char **argv, char **column)
{
    for(int i = 0; i < argc; i++) {
        printf("%s = %s\n", column[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}


int sqlite_init()
{
    sqlite3 *db;
    char *err_msg = 0;
    
	char *zen_db = memalloc(PATH_MAX);
	char *data_dir = replace_home(CLIENT_DATA_DIR);
	snprintf(zen_db, PATH_MAX, "%s/%s", data_dir, "data.db");
	free(data_dir);
	if (access(zen_db, W_OK) != 0) {
		/* If data file doesn't exist, most likely data dir won't exist too */
		mkdir_p(CLIENT_DATA_DIR);
	}

    int rc = sqlite3_open(zen_db, &db);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    
    char *users_statement = "CREATE TABLE IF NOT EXISTS Users(Username TEXT, SharedKey TEXT, Test TEXT);";
    char *shared_key_statement = "SELECT * FROM Users;";
    char *messages_statement = "CREATE TABLE IF NOT EXISTS Messages(Username TEXT, );";
                //"INSERT INTO Users VALUES('night', 'test', '1');";

    rc = sqlite3_exec(db, users_statement, 0, 0, &err_msg);
    
    if (rc != SQLITE_OK) {
        error(0, "SQL error: %s", err_msg);
        sqlite3_free(err_msg);        
    } else {
/*         error(0, "Table created successfully"); */
    }

    // Select and print all entries
    rc = sqlite3_exec(db, "SELECT * FROM Users", callback, NULL, &err_msg);
    
    if (rc != SQLITE_OK) {
        error(0, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);        
    }

    sqlite3_close(db);
    
    return 0;
}

