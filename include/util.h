#ifndef UTIL_H
#define UTIL_H

#include <stdlib.h>

#define LOG_ERROR 1
#define LOG_INFO 2

#define PATH_MAX 4096

void error(int fatal, const char *fmt, ...);
void *memalloc(size_t size);
char *replace_home(char *str);
void mkdir_p(const char *file);
void write_log(int type, const char *fmt, ...);
void print_bin(const unsigned char *ptr, size_t length);

#endif
