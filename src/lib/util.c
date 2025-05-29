#include "config.h"
#include "packet.h"
#include "util.h"

/*
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

	if (errsv != 0 && errsv != EEXIST) {
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
		write_log(LOG_ERROR, "Error allocating memory"); 
		return NULL;
	}
	return ptr;
}

/*
 * Takes heap-allocated str and replace ~ with home path
 * Returns heap-allocated newstr
 */
char *replace_home(char *str)
{
	char *home = getenv("HOME");
	if (home == NULL) {
		write_log(LOG_ERROR, "$HOME not defined"); 
		return str;
	}
	char *newstr = memalloc(strlen(str) + strlen(home));
	/* replace ~ with home */
	snprintf(newstr, strlen(str) + strlen(home), "%s%s", home, str + 1);
	return newstr;
}

/*
 * Recursively create directory by creating each subdirectory
 * like mkdir -p
 * Create the parent folder(s) of given file
 */
void mkdir_p(const char *file)
{
	char *path = memalloc(PATH_MAX);
	char dir_path[PATH_MAX];
	dir_path[0] = '\0';

	if (file[0] == '~') {
		char *home = getenv("HOME");
		if (home == NULL) {
			write_log(LOG_ERROR, "$HOME not defined"); 
			return;
		}
		/* replace ~ with home */
		snprintf(path, PATH_MAX, "%s%s", home, file + 1);
	} else {
		strcpy(path, file);
	}

	/* fix first / not appearing in the string */
	if (path[0] == '/')
		strcat(dir_path, "/");

	/* Find last occurrence of '/' */
	char *last_slash = strrchr(path, '/'); 

	/* Remove last slash */
	if (last_slash != NULL) {
		path[last_slash - path] = '\0';
	}

	char *token = strtok(path, "/");
	while (token != NULL) {
		strcat(dir_path, token);
		strcat(dir_path, "/");

		if (mkdir(dir_path, 0755) == -1) {
			struct stat st;
			if (stat(dir_path, &st) == 0 && S_ISDIR(st.st_mode)) {
				/* Directory already exists, continue to the next dir */
				token = strtok(NULL, "/");
				continue;
			}
			
			write_log(LOG_ERROR, "mkdir failed: %s", strerror(errno)); 
			free(path);
			return;
		}
		token = strtok(NULL, "/");
	}

	free(path);
	return;
}

void write_log(int type, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	char client_log[PATH_MAX];
	char *data_dir = replace_home(CLIENT_DATA_DIR);
	if (access(data_dir, W_OK) != 0) {
		/* If data dir doesn't exist, create it */
		mkdir_p(data_dir);
	}
	snprintf(client_log, PATH_MAX, "%s/zen.log", data_dir);
	free(data_dir);

	FILE *log = fopen(client_log, "a");
	if (log != NULL) {
		time_t now = time(NULL);
		struct tm *t = localtime(&now);
		/* either info or error */
		int type_len = type == LOG_INFO ? 4 : 5;
		char logtype[4 + type_len];
		snprintf(logtype, 4 + type_len, "[%s] ", type == LOG_INFO ? "INFO" : "ERROR");
		char time[21];
		strftime(time, 21, "%Y-%m-%d %H:%M:%S ", t);
		char details[4 + type_len + 21];
		snprintf(details, 4 + type_len + 21, "%s%s", logtype, time);
		fprintf(log, "%s", details);
		vfprintf(log, fmt, args);
		fprintf(log, "\n");
		fclose(log);
	}
	va_end(args);
}

void print_bin(const uint8_t *ptr, size_t length)
{
	for (size_t i = 0; i < length; i++) {
		printf("%02x", ptr[i]);
/* 		printf("%d ", ptr[i]); */
	}
	printf("\n");
}
