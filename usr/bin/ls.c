/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "stat.h"
#include "user.h"
#include "p9records.h"
#include "mode.h"
#include "termios.h"

#define LONG 1
#define ALL 2
#define HUMAN 4

static void
permissions(const struct statx *info, char *out)
{
  static const char types[] = "?d-cs";
  static const char bits[] = "rwx";
  int i;

  out[0] = info->st.type > 0 && info->st.type <= T_SOCKET ? types[info->st.type] : '?';
  for(i = 0; i < 9; i++)
    out[1+i] = info->mode & (1U << (8-i)) ? bits[i%3] : '-';
  if(info->mode & M_SETGID)
    out[6] = out[6] == 'x' ? 's' : 'S';
  if(info->mode & M_STICKY)
    out[9] = out[9] == 'x' ? 't' : 'T';
  out[10] = 0;
}

static void
printsize(uint size, int human)
{
  static const char units[] = " KMGT";
  uint scale = 1, unit = 0, tenths;

  while(human && size / scale >= 1024 && unit < sizeof(units)-2){
    scale *= 1024;
    unit++;
  }
  if(!unit){
    printf(1, "%u", size);
  } else {
    tenths = ((size % scale) / (scale / 16) * 10 + 8) / 16;
    printf(1, "%u.%u%c", size / scale + tenths/10, tenths % 10, units[unit]);
  }
}

static void
printentry(const char *name, const struct statx *info, int flags, int color)
{
  char mode[11];

  if(flags & LONG){
    permissions(info, mode);
    printf(1, "%s %d %u %u ", mode, info->st.nlink, info->uid, info->gid);
    printsize(info->st.size, flags & HUMAN);
    printf(1, " ");
  }
  if(color && info->st.type == T_DIR)
    printf(1, "\033[34m%s\033[0m\n", name);
  else
    printf(1, "%s\n", name);
}

static int
list(const char *path, int flags, int color)
{
  char buf[512], *name;
  uint len = strlen(path);
  struct p9_direntx de;
  struct statx info;
  int fd, n, failed = 0;

  if(statx(path, &info) < 0){
    printf(2, "ls: cannot stat %s\n", path);
    return 1;
  }
  if(info.st.type != T_DIR){
    printentry(path, &info, flags, color);
    return 0;
  }
  if(len + 2 > sizeof(buf) || (fd = open(path, 0)) < 0){
    printf(2, "ls: cannot open %s\n", path);
    return 1;
  }
  strcpy(buf, path);
  name = buf + len;
  if(len && name[-1] != '/')
    *name++ = '/';
  while((n = getdent(fd, &de)) > 0){
    if(de.name[0] == '.' && !(flags & ALL))
      continue;
    if(strlen(de.name) + 1 > sizeof(buf) - (name - buf)){
      printf(2, "ls: path too long in %s\n", path);
      failed = 1;
      continue;
    }
    strcpy(name, de.name);
    if(statx(buf, &info) < 0){
      if(flags & LONG)
        failed = 1;
      printf(1, "%s\n", name);
    } else {
      printentry(name, &info, flags, color);
    }
  }
  close(fd);
  return failed || n != 0;
}

int
main(int argc, char **argv)
{
  int i, flags = 0, failed = 0, color = isatty(1), many;
  const char *p;

  for(i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++){
    if(!strcmp(argv[i], "--")){
      i++;
      break;
    }
    for(p = argv[i] + 1; *p; p++){
      switch(*p){
      case 'l': flags |= LONG; break;
      case 'a': flags |= ALL; break;
      case 'h': flags |= HUMAN; break;
      default:
        printf(2, "usage: ls [-alh] [file ...]\n");
        return 2;
      }
    }
  }
  if(i == argc)
    return list(".", flags, color);
  many = argc-i > 1;
  for(; i < argc; i++){
    if(many)
      printf(1, "%s:\n", argv[i]);
    failed |= list(argv[i], flags, color);
  }
  return failed;
}
