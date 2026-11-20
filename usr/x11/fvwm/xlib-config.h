/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */
#define HAVE_GETPAGESIZE 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define STDC_HEADERS 1
#define UNIXCONN 1
#define USE_POLL 1
#define XERRORDB "/usr/X11/share/XErrorDB"
#define XKEYSYMDB "/usr/X11/share/XKeysymDB"
#define X11_LIBDIR "/usr/X11/lib"
#define X11_DATADIR "/usr/X11/share"
#define XLOCALEDATADIR "/usr/X11/share/locale"
#define XLOCALEDIR XLOCALEDATADIR
#define XLOCALELIBDIR X11_LIBDIR
#include <stddef.h>
int gethostname(char *, size_t);
#ifndef isascii
#define isascii(c) ((unsigned)(c) < 128)
#endif
