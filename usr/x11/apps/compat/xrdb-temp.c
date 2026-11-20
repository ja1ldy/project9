/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

int
mkstemp(char *path)
{
  size_t n = strlen(path);
  if(n < 6 || strcmp(path + n - 6, "XXXXXX")){
    errno = EINVAL;
    return -1;
  }
  unsigned base = (unsigned)getpid() * 947U;
  for(unsigned attempt = 0; attempt < 1024; ++attempt){
    unsigned value = (base + attempt) % 1000000U;
    for(size_t pos = n; pos > n - 6; --pos){
      path[pos-1] = '0' + value % 10;
      value /= 10;
    }
    int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
    if(fd >= 0 || errno != EEXIST)
      return fd;
  }
  return -1;
}
