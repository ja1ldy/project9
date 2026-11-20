/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_SYS_SOCKET_H
#define P9_LIBC_SYS_SOCKET_H
#include <sys/types.h>
#include <project9/local.h>

typedef unsigned int socklen_t;
typedef unsigned short sa_family_t;
struct sockaddr { sa_family_t sa_family; char sa_data[14]; };

int socket(int, int, int) __asm__("p9_socket");
int bind(int, const struct sockaddr *, socklen_t) __asm__("p9_bind");
int listen(int, int) __asm__("p9_listen");
int accept(int, struct sockaddr *, socklen_t *) __asm__("p9_accept");
int connect(int, const struct sockaddr *, socklen_t) __asm__("p9_connect");
int shutdown(int, int) __asm__("p9_shutdown");
#endif
