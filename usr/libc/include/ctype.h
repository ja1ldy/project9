/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_CTYPE_H
#define P9_LIBC_CTYPE_H
#include <locale.h>
int isspace_l(int, locale_t);
int isdigit(int), isalpha(int), isspace(int), isalnum(int);
int isblank(int), iscntrl(int), isgraph(int), ispunct(int);
int isupper(int), islower(int), isprint(int), isxdigit(int);
int tolower(int), toupper(int);
#endif
