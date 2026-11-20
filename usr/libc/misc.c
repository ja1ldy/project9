/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct p9_DIR { int fd; struct dirent entry; };
extern int getdent(int, struct p9_direntx *);


DIR *
opendir(const char *name)
{
  int fd = open(name, O_RDONLY);
  struct stat st;
  DIR *d;

  if(fd < 0)
    return NULL;
  if(fstat(fd, &st) < 0 || !S_ISDIR(st.st_mode)){
    close(fd);
    errno = ENOTDIR;
    return NULL;
  }
  d = malloc(sizeof(*d));
  if(!d){
    close(fd);
    return NULL;
  }
  d->fd = fd;
  return d;
}

struct dirent *
readdir(DIR *d)
{
  struct p9_direntx raw;
  int n = getdent(d->fd, &raw);

  if(n <= 0){
    if(n < 0)
      errno = EIO;
    return NULL;
  }
  d->entry.d_ino = raw.inum;
  memcpy(d->entry.d_name, raw.name, sizeof(raw.name));
  return &d->entry;
}

void rewinddir(DIR *d) { lseek(d->fd, 0, SEEK_SET); }
int closedir(DIR *d) { int rc = close(d->fd); free(d); return rc; }

static char **env_vector;
static unsigned char *owned;
static size_t capacity;

static int
reserve_env(size_t count)
{
  size_t n;
  char **vec;
  unsigned char *flags;

  if(environ == env_vector && count < capacity)
    return 0;
  for(n = 0; environ && environ[n]; n++)
    ;
  if(count < n)
    count = n;
  if(count > SIZE_MAX / sizeof(char *) - 8){
    errno = ENOMEM;
    return -1;
  }
  count += 8;
  vec = calloc(count, sizeof(*vec));
  flags = calloc(count, 1);
  if(!vec || !flags){
    free(vec);
    free(flags);
    return -1;
  }
  if(n)
    memcpy(vec, environ, n*sizeof(*vec));
  if(environ == env_vector){
    if(n)
      memcpy(flags, owned, n);
    free(env_vector);
    free(owned);
  }
  environ = env_vector = vec;
  owned = flags;
  capacity = count;
  return 0;
}

static int
set(const char *name, size_t len, char *value, int own, int replace)
{
  size_t i, n, first;

  for(n = 0; environ && environ[n]; n++)
    ;
  first = n;
  for(i = 0; i<n; i++)
    if(!strncmp(environ[i], name, len) && environ[i][len] == '='){
      first = i;
      break;
    }
  if(first != n && !replace){
    if(own)
      free(value);
    return 0;
  }
  if(reserve_env(n+1) < 0){
    if(own)
      free(value);
    return -1;
  }
  if(first < n && owned[first] && environ[first] != value)
    free(environ[first]);
  if(environ[first] != value){
    environ[first] = value;
    owned[first] = own;
  }
  return 0;
}

int
setenv(const char *name, const char *value, int overwrite)
{
  size_t len, bytes;
  char *entry;

  if(!name || !*name || strchr(name, '=') || !value){
    errno = EINVAL;
    return -1;
  }
  if(!overwrite && getenv(name))
    return 0;
  len = strlen(name);
  bytes = strlen(value);
  if(bytes > SIZE_MAX - len - 2){
    errno = ENOMEM;
    return -1;
  }
  entry = malloc(len+bytes+2);
  if(!entry)
    return -1;
  memcpy(entry, name, len);
  entry[len] = '=';
  memcpy(entry+len+1, value, bytes+1);
  return set(name, len, entry, 1, overwrite);
}

int
putenv(char *entry)
{
  char *equal = entry ? strchr(entry, '=') : NULL;
  if(!equal || equal == entry){
    errno = EINVAL;
    return -1;
  }
  return set(entry, equal-entry, entry, 0, 1);
}

int
unsetenv(const char *name)
{
  size_t len, i, j;

  if(!name || !*name || strchr(name, '=')){
    errno = EINVAL;
    return -1;
  }
  if(!getenv(name))
    return 0;
  len = strlen(name);
  if(reserve_env(0) < 0)
    return -1;
  for(i = 0; environ[i]; ){
    if(strncmp(environ[i], name, len) || environ[i][len] != '='){
      i++;
      continue;
    }
    if(owned[i])
      free(environ[i]);
    for(j = i; environ[j]; j++){
      environ[j] = environ[j+1];
      owned[j] = owned[j+1];
    }
  }
  return 0;
}

char *optarg;
int optind = 1, opterr = 1, optopt;

int
getopt(int argc, char *const argv[], const char *opts)
{
  static int offset = 1;
  const char *match;
  const char *arg;

  if(!optind){
    optind = 1;
    offset = 1;
  }
  optarg = NULL;
  if(optind >= argc)
    return -1;
  arg = argv[optind];
  if(offset == 1){
    if(arg[0] != '-' || !arg[1])
      return -1;
    if(strcmp(arg, "--") == 0){
      optind++;
      return -1;
    }
  }
  optopt = (unsigned char)arg[offset++];
  match = optopt == ':' ? NULL : strchr(opts, optopt);
  if(match && match[1] == ':'){
    optarg = arg[offset] ? argv[optind]+offset : ++optind < argc ? argv[optind] : NULL;
    offset = 1;
    if(optarg){
      optind++;
      return optopt;
    }
    if(opts[0] == ':')
      return ':';
    if(opterr)
      fprintf(stderr, "%s: option requires an argument -- %c\n", argv[0], optopt);
    return '?';
  }
  if(!arg[offset]){
    offset = 1;
    optind++;
  }
  if(match)
    return optopt;
  if(opterr && opts[0] != ':')
    fprintf(stderr, "%s: unknown option -- %c\n", argv[0], optopt);
  return '?';
}

static int
bracket(const char **p, unsigned char c)
{
  const char *s = *p;
  int invert = *s == '!' || *s == '^', match = 0;
  unsigned char first, last;

  s += invert;
  do {
    if(!*s)
      return -1;
    first = last = *s++;
    if(s[0] == '-' && s[1] && s[1] != ']'){
      last = s[1];
      s += 2;
    }
    match |= first <= c && c <= last;
  } while(*s != ']');
  *p = s+1;
  return match != invert;
}

int
fnmatch(const char *p, const char *s, int flags)
{
  const char *start = s, *retry = NULL, *star = NULL, *next;
  int special, match, c;

  for(;;){
    c = (unsigned char)*p;
    special = (*s == '/' && (flags & FNM_PATHNAME)) ||
              (*s == '.' && (flags & FNM_PERIOD) &&
               (s == start || ((flags & FNM_PATHNAME) && s[-1] == '/')));
    if(c == '*' && !(*s == '.' && special)){
      while(*p == '*')
        p++;
      star = p;
      retry = s;
      continue;
    }
    if(!c && !*s)
      return 0;
    next = p+1;
    match = *s && c == (unsigned char)*s;
    if(c == '?')
      match = *s && !special;
    if(c == '[' && *s && !special){
      int b = bracket(&next, *s);
      if(b >= 0)
        match = b;
    }
    if(c == '\\' && !(flags & FNM_NOESCAPE) && *next)
      match = *s && (unsigned char)*next++ == (unsigned char)*s;
    if(match){
      p = next;
      s++;
      continue;
    }
    if(!retry || !*retry || ((flags & FNM_PATHNAME) && *retry == '/'))
      return FNM_NOMATCH;
    p = star;
    s = ++retry;
  }
}

char *
strtok(char *string, const char *delim)
{
  static char *saved;
  return strtok_r(string, delim, &saved);
}

char *
strncat(char *dest, const char *src, size_t size)
{
  char *out = dest + strlen(dest);
  while(size-- && *src)
    *out++ = *src++;
  *out = 0;
  return dest;
}

int
gethostname(char *name, size_t size)
{
  if(!size){
    errno = EINVAL;
    return -1;
  }
  FILE *input = fopen("/etc/hostname", "r");
  if(!input)
    return -1;
  if(!fgets(name, size, input)){
    fclose(input);
    errno = EIO;
    return -1;
  }
  int c = fgetc(input);
  fclose(input);
  size_t length = strlen(name);
  if(length && name[length-1] == '\n')
    name[length-1] = 0;
  else if(c != EOF && c != '\n'){
    errno = ERANGE;
    return -1;
  }
  return 0;
}
