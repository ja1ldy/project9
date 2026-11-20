/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */
#ifndef P9_X_CLIENT_H
#define P9_X_CLIENT_H

#include <stddef.h>
#include <stdint.h>
#include <X11/X.h>
#include <X11/Xproto.h>

#define XREQ(type, name, opcode) type name = { .reqType = X_ ## opcode, .length = sizeof(type)/4 }
#define XPAD(n) (((n) + 3U) & ~3U)

struct display {
  int fd;
  unsigned sequence;
  xConnSetup setup;
  xWindowRoot root;
};

int xopen(struct display *);
int xread(struct display *, void *, size_t);
int xwrite(struct display *, const void *, size_t);
int xrequest(struct display *, const void *, size_t);
int xreply(struct display *, void *);
int xsync(struct display *);
int xid(const char *, uint32_t *);
#endif
