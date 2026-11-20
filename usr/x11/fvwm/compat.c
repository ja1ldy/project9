/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <ctype.h>
#include <stdarg.h>
#include <sys/select.h>
#include <project9/param.h>
#include <unistd.h>

int getdtablesize(void) { return NOFILE; }

size_t
strlcat(char *out, const char *in, size_t cap)
{
  size_t tail = strlen(in);
  size_t used = 0;
  while(used < cap && out[used])
    used++;
  if(used < cap)
    strlcpy(out + used, in, cap - used);
  return used + tail;
}

int
strncasecmp(const char *a, const char *b, size_t len)
{
  for(size_t i = 0; i < len; i++){
    int c = tolower((unsigned char)a[i]);
    int d = tolower((unsigned char)b[i]);
    if(c != d || !c)
      return c - d;
  }
  return 0;
}

int strcasecmp(const char *a, const char *b) { return strncasecmp(a, b, (size_t)-1); }

int
execl(const char *path, const char *name, ...)
{
  char *args[32];
  args[0] = (char *)name;
  va_list ap;
  va_start(ap, name);
  for(unsigned i = 1; i < sizeof(args)/sizeof(args[0]); i++){
    args[i] = va_arg(ap, char *);
    if(!args[i]){
      va_end(ap);
      return execve(path, args, environ);
    }
  }
  va_end(ap);
  errno = EOVERFLOW;
  return -1;
}

#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>

void *
bsearch(const void *key, const void *items, size_t count, size_t width,
        int (*compare)(const void *, const void *))
{
  const char *base = items;
  while(count){
    size_t middle = count / 2;
    const char *slot = base + middle * width;
    int order = compare(key, slot);
    if(order == 0)
      return (void *)slot;
    if(order < 0){
      count = middle;
    } else {
      base = slot + width;
      count -= middle + 1;
    }
  }
  return NULL;
}

static pid_t pipechild[NOFILE];

static int
reap(pid_t child)
{
  int status;
  int got;
  do {
    got = waitpid(child, &status, 0);
  } while(got < 0 && errno == EINTR);
  return got == child ? status : -1;
}

FILE *
popen(const char *command, const char *mode)
{
  int channel[2];
  if(strcmp(mode, "r")){
    errno = EINVAL;
    return NULL;
  }
  if(pipe(channel) < 0)
    return NULL;
  if(fcntl(channel[0], F_SETFD, FD_CLOEXEC) < 0 ||
     fcntl(channel[1], F_SETFD, FD_CLOEXEC) < 0){
    close(channel[0]);
    close(channel[1]);
    return NULL;
  }
  pid_t child = fork();
  if(child == 0){
    close(channel[0]);
    if(channel[1] == 1){
      if(fcntl(1, F_SETFD, 0) < 0)
        _exit(126);
    } else {
      if(dup2(channel[1], 1) < 0)
        _exit(126);
      close(channel[1]);
    }
    char *argv[] = {"/bin/sh", "-c", (char *)command, NULL};
    execve(argv[0], argv, environ);
    _exit(127);
  }
  close(channel[1]);
  if(child < 0){
    close(channel[0]);
    return NULL;
  }
  FILE *stream = fdopen(channel[0], "r");
  if(!stream){
    close(channel[0]);
    kill(child, SIGKILL);
    reap(child);
    return NULL;
  }
  pipechild[channel[0]] = child;
  return stream;
}

int
pclose(FILE *stream)
{
  int fd = fileno(stream);
  if(fd < 0 || fd >= NOFILE || !pipechild[fd]){
    errno = EINVAL;
    return -1;
  }
  pid_t child = pipechild[fd];
  pipechild[fd] = 0;
  fclose(stream);
  return reap(child);
}
