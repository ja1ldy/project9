/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LOCAL_H
#define P9_LOCAL_H
#define AF_UNIX 1
#define AF_LOCAL AF_UNIX
#define SOCK_STREAM 1
#define SOCK_NONBLOCK 0x800
#define SOCK_CLOEXEC 0x80000
#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2
#define AF_INET 2
#define IPPROTO_TCP 6
#define INADDR_LOOPBACK 0x7f000001U
struct in_addr { unsigned int s_addr; };
struct sockaddr_in {
  unsigned short sin_family, sin_port;
  struct in_addr sin_addr;
  char sin_zero[8];
};
struct sockaddr_un {
  unsigned short sun_family;
  char sun_path[108];
};
#endif
