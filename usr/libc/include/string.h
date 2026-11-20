/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_STRING_H
#define P9_LIBC_STRING_H
#include <stddef.h>
void *memcpy(void *, const void *, size_t), *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
void *memchr(const void *, int, size_t);
int memcmp(const void *, const void *, size_t);
size_t strlen(const char *);
char *strcpy(char *, const char *), *strncpy(char *, const char *, size_t);
char *strcat(char *, const char *);
int strcoll(const char *, const char *);
int strcmp(const char *, const char *), strncmp(const char *, const char *, size_t);
char *strchr(const char *, int), *strrchr(const char *, int);
char *strstr(const char *, const char *), *strdup(const char *);
size_t strlcpy(char *, const char *, size_t);
size_t strspn(const char *, const char *), strcspn(const char *, const char *);
char *strtok_r(char *, const char *, char **);
char *strtok(char *, const char *);
char *strncat(char *, const char *, size_t);
char *strerror(int);
#endif
