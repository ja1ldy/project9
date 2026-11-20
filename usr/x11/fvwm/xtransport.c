/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <X11/Xlibint.h>
#include <X11/Xtrans/Xtrans.h>

struct _XtransConnInfo {
  int fd;
};

XtransConnInfo
_X11TransConnectDisplay(char *name, char **fullname, int *number, int *screen,
                       char **auth_name, int *auth_namelen, char **auth_data, int *auth_datalen)
{
  if(!name || (strcmp(name, ":0") && strcmp(name, ":0.0") &&
               strcmp(name, "unix:0") && strcmp(name, "unix:0.0"))){
    errno = EINVAL;
    return NULL;
  }
  struct sockaddr_un path = { .sun_family = AF_UNIX, .sun_path = "/tmp/.X11-unix/X0" };
  int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if(fd < 0)
    return NULL;
  if(connect(fd, (struct sockaddr *)&path, sizeof(path)) < 0){
    close(fd);
    return NULL;
  }
  XtransConnInfo link = malloc(sizeof(*link));
  char *canonical = strdup(":0.0");
  if(!link || !canonical){
    free(link);
    free(canonical);
    close(fd);
    errno = ENOMEM;
    return NULL;
  }
  link->fd = fd;
  *fullname = canonical;
  *number = *screen = 0;
  *auth_name = *auth_data = NULL;
  *auth_namelen = *auth_datalen = 0;
  return link;
}

int _X11TransGetConnectionNumber(XtransConnInfo t) { return t->fd; }

int
_XDisconnectDisplay(XtransConnInfo link)
{
  close(link->fd);
  free(link);
  return 0;
}

Bool
_XSendClientPrefix(Display *dpy, xConnClientPrefix *prefix, char *p, char *s, xConnSetupPrefix *reply)
{
  const char *data = (const char *)prefix;
  int left = sizeof(*prefix);
  while(left){
    int n = write(dpy->fd, data, left);
    if(n < 0 && errno == EINTR)
      continue;
    if(n <= 0)
      return -1;
    left -= n;
    data += n;
  }
  return fcntl(dpy->fd, F_SETFL, O_NONBLOCK);
}

int
_X11TransBytesReadable(XtransConnInfo link, BytesReadable_t *pending)
{
  int bytes;
  if(ioctl(link->fd, FIONREAD, &bytes) < 0)
    return -1;
  *pending = bytes;
  return 0;
}

int _X11TransRead(XtransConnInfo t, char *p, int n) { return read(t->fd, p, n); }
int _X11TransWrite(XtransConnInfo t, const char *p, int n) { return write(t->fd, p, n); }

static int
vectored(XtransConnInfo link, struct iovec *v, int count, int writing)
{
  int total = 0;
  for(int i = 0; i < count; i++){
    if(!v[i].iov_len)
      continue;
    size_t size = v[i].iov_len;
    if(size > INT_MAX - total)
      size = INT_MAX - total;
    int n = writing ? write(link->fd, v[i].iov_base, size) : read(link->fd, v[i].iov_base, size);
    if(n < 0)
      return total ? total : n;
    total += n;
    if((size_t)n < v[i].iov_len)
      break;
  }
  return total;
}

int _X11TransReadv(XtransConnInfo t, struct iovec *v, int count) { return vectored(t, v, count, 0); }
int _X11TransWritev(XtransConnInfo t, struct iovec *v, int count) { return vectored(t, v, count, 1); }
