/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */
#ifndef P9_XTRANS_H
#define P9_XTRANS_H
#include <stddef.h>
#include <sys/types.h>
typedef struct _XtransConnInfo *XtransConnInfo;
typedef long BytesReadable_t;
struct iovec { void *iov_base; size_t iov_len; };
int _X11TransGetConnectionNumber(XtransConnInfo);
int _X11TransBytesReadable(XtransConnInfo, BytesReadable_t *);
int _X11TransRead(XtransConnInfo, char *, int);
int _X11TransWrite(XtransConnInfo, const char *, int);
int _X11TransReadv(XtransConnInfo, struct iovec *, int);
int _X11TransWritev(XtransConnInfo, struct iovec *, int);
#endif
