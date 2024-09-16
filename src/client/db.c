#include <stdio.h>
#include <string.h>

#include "config.h"
#include "packet.h"
#include "util.h"
#include "client/ui.h"
#include "client/db.h"
#include "client/user.h"

static int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
    char *username = memalloc(32 * sizeof(char));
	strcpy(username, argv[0]);
	add_username(username);

    /*
    for(int i = 0; i < argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    */
    return 0;
}

int sqlite_init()
{
    sqlite3 *db;
    char *err_msg = 0;
    
    int rc = sqlite3_open(DATABASE_NAME, &db);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }
    
    char *users_statement = "CREATE TABLE IF NOT EXISTS Users(Username TEXT, SecretKey TEXT, Test TEXT);";
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
    const char* data = "Callback function called";
    rc = sqlite3_exec(db, "SELECT * FROM Users", callback, (void*) data, &err_msg);
    
    if (rc != SQLITE_OK ) {
        error(0, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);        
    }

    sqlite3_close(db);
    
    return 0;
}

