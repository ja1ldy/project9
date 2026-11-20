/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_X_UIO_H
#define P9_X_UIO_H
#include <stddef.h>
struct iovec {
  void *iov_base;
  size_t iov_len;
};
#endif
