/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "stat.h"
#include "mode.h"

static int
unary(const char *op, const char *path)
{
  struct statx st;
  uint mode, need;
  int uid;

  if(!strcmp(op, "-n"))
    return *path != 0;
  if(!strcmp(op, "-z"))
    return *path == 0;
  if(!strcmp(op, "-r"))
    need = M_READ;
  else if(!strcmp(op, "-w"))
    need = M_WRITE;
  else if(!strcmp(op, "-x"))
    need = M_EXEC;
  else if(strcmp(op, "-e") && strcmp(op, "-f") && strcmp(op, "-d"))
    return -1;
  else {
    if(statx(path, &st) < 0)
      return 0;
    return op[1] == 'e' || (op[1] == 'd' ? st.st.type == T_DIR : st.st.type == T_FILE);
  }
  if(statx(path, &st) < 0)
    return 0;
  mode = st.mode;
  uid = getuid();
  if(uid == 0)
    return need != M_EXEC || st.st.type != T_FILE || (mode & 0111) != 0;
  if(uid == st.uid)
    mode >>= 6;
  else if(getgid() == st.gid)
    mode >>= 3;
  return (mode & need) != 0;
}

static int
binary(const char *left, const char *op, const char *right)
{
  int a, b;

  if(!strcmp(op, "=") || !strcmp(op, "=="))
    return !strcmp(left, right);
  if(!strcmp(op, "!="))
    return strcmp(left, right) != 0;
  if(parseint(left, &a) < 0 || parseint(right, &b) < 0)
    return -1;
  if(!strcmp(op, "-eq"))
    return a == b;
  if(!strcmp(op, "-ne"))
    return a != b;
  if(!strcmp(op, "-lt"))
    return a < b;
  if(!strcmp(op, "-le"))
    return a <= b;
  if(!strcmp(op, "-gt"))
    return a > b;
  if(!strcmp(op, "-ge"))
    return a >= b;
  return -1;
}

int
main(int argc, char **argv)
{
  const char *base = argv[0], *p;
  int negate = 0, result;
  int n = argc - 1;
  char **args = argv + 1;

  for(p = base; *p; p++)
    if(*p == '/')
      base = p + 1;
  if(!strcmp(base, "[")){
    if(n == 0 || strcmp(args[n-1], "]"))
      goto usage;
    n--;
  }
  while(n > 1 && !strcmp(*args, "!")){
    negate ^= 1;
    args++;
    n--;
  }
  switch(n){
  case 0: result = 0; break;
  case 1: result = **args != 0; break;
  case 2: result = unary(args[0], args[1]); break;
  case 3: result = binary(args[0], args[1], args[2]); break;
  default: goto usage;
  }
  if(result >= 0)
    return (result ^ negate) ? 0 : 1;

usage:
  printf(2, "test: invalid expression\n");
  return 2;
}
