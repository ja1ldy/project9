/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_NETINET_IN_H
#define P9_LIBC_NETINET_IN_H
#include <stdint.h>
#include <project9/local.h>
static inline uint16_t htons(uint16_t x) { return __builtin_bswap16(x); }
static inline uint16_t ntohs(uint16_t x) { return __builtin_bswap16(x); }
static inline uint32_t htonl(uint32_t x) { return __builtin_bswap32(x); }
static inline uint32_t ntohl(uint32_t x) { return __builtin_bswap32(x); }
#endif
