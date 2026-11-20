/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "display.h"
#include "xclient.h"

int
xread(struct display *d, void *data, size_t bytes)
{
  unsigned char *p = data;
  while(bytes){
    ssize_t n = read(d->fd, p, bytes);
    if(n < 0 && errno == EINTR)
      continue;
    if(n <= 0)
      return -1;
    bytes -= n;
    p += n;
  }
  return 0;
}

int
xwrite(struct display *d, const void *data, size_t bytes)
{
  const unsigned char *p = data;
  while(bytes){
    ssize_t n = write(d->fd, p, bytes);
    if(n < 0 && errno == EINTR)
      continue;
    if(n <= 0)
      return -1;
    bytes -= n;
    p += n;
  }
  return 0;
}

int
xrequest(struct display *d, const void *data, size_t bytes)
{
  d->sequence++;
  return xwrite(d, data, bytes);
}

int
xopen(struct display *d)
{
  const char *name = getenv("DISPLAY");
  if(name && strcmp(name, ":0") && strcmp(name, ":0.0") && strcmp(name, "unix:0")){
    errno = EINVAL;
    return -1;
  }
  d->sequence = 0;
  d->fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(d->fd < 0)
    return -1;
  struct sockaddr_un address = display_address();
  xConnClientPrefix hello = { .byteOrder = 'l', .majorVersion = 11 };
  xConnSetupPrefix header;
  unsigned char *buf = NULL;
  if(connect(d->fd, (struct sockaddr*)&address, sizeof(address)) < 0)
    goto failed;
  if(xwrite(d, &hello, sizeof(hello)) < 0 || xread(d, &header, sizeof(header)) < 0 ||
     header.success != 1 || header.majorVersion != 11 || header.length * 4U < sizeof(d->setup))
    goto invalid;
  buf = malloc(header.length * 4U);
  if(!buf)
    goto failed;
  if(xread(d, buf, header.length * 4U) < 0)
    goto invalid;
  memcpy(&d->setup, buf, sizeof(d->setup));
  size_t pos = sizeof(d->setup) + XPAD(d->setup.nbytesVendor) + d->setup.numFormats * sizeof(xPixmapFormat);
  if(d->setup.numRoots != 1 || pos + sizeof(d->root) > header.length * 4U)
    goto invalid;
  memcpy(&d->root, buf + pos, sizeof(d->root));
  free(buf);
  return 0;
invalid:
  errno = EIO;
failed:
  free(buf);
  close(d->fd);
  d->fd = -1;
  return -1;
}

int
xreply(struct display *d, void *result)
{
  xGenericReply reply;
  for(;;){
    if(xread(d, &reply, sizeof(reply)) < 0 || !reply.type)
      return -1;
    if(reply.type == X_Reply){
      if(reply.sequenceNumber != (uint16_t)d->sequence)
        return -1;
      memcpy(result, &reply, sizeof(reply));
      return 0;
    }
  }
}

int
xsync(struct display *d)
{
  XREQ(xReq, req, GetInputFocus);
  xGetInputFocusReply reply;
  return xrequest(d, &req, sizeof(req)) || xreply(d, &reply) ? -1 : 0;
}

int
xid(const char *text, uint32_t *id)
{
  char *end;
  if(!text || *text < '0' || *text > '9')
    return -1;
  errno = 0;
  unsigned long value = strtoul(text, &end, 0);
  if(errno || *end || !value || value > UINT32_MAX)
    return -1;
  *id = value;
  return 0;
}
