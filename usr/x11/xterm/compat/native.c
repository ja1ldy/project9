/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include "pwd.h"
#include "../../../../include/account.h"

struct passwd *
getpwuid(uid_t uid)
{
  static struct account_info info;
  static struct passwd pw;

  if(uid != getuid() || accountinfo(uid, &info) < 0)
    return NULL;

  pw.pw_uid = uid;
  pw.pw_gid = getgid();
  pw.pw_name = info.name;
  pw.pw_dir = info.home;
  pw.pw_shell = "/bin/sh";
  pw.pw_passwd = "*";
  pw.pw_gecos = "";
  return &pw;
}

struct passwd *
getpwnam(const char *name)
{
  struct passwd *pw = getpwuid(getuid());
  return pw && strcmp(pw->pw_name, name) == 0 ? pw : NULL;
}

int
execlp(const char *file, const char *arg, ...)
{
  char *args[32];
  size_t i = 0;
  va_list ap;

  va_start(ap, arg);
  while(arg && i < sizeof(args) / sizeof(args[0]) - 1){
    args[i++] = (char *)arg;
    arg = va_arg(ap, const char *);
  }
  va_end(ap);

  if(arg){
    errno = EINVAL;
    return -1;
  }
  args[i] = NULL;
  return execvp(file, args);
}

int
tgetent(char *buffer, const char *name)
{
  if(buffer)
    *buffer = '\0';
  return strcmp(name, "xterm") == 0 || strcmp(name, "vt100") == 0;
}

char *
tgetstr(const char *name, char **area)
{
  static char erase[] = "\b";

  if(strcmp(name, "kb") != 0)
    return NULL;
  if(!area || !*area)
    return erase;
  char *value = *area;
  memcpy(value, erase, sizeof(erase));
  *area += sizeof(erase);
  return value;
}
