/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "diskuser.h"
#include "user.h"
#include "fcntl.h"
#include "sha256.h"

static char digits[] = "0123456789abcdef";
static uchar buffer[4096];

static int
hash(const char *name, uchar digest[32])
{
  SHA256_CTX ctx;
  int fd = name ? open(name, O_RDONLY) : 0, n;

  if(fd < 0)
    return -1;
  sha256_init(&ctx);
  while((n = read(fd, buffer, sizeof buffer)) > 0)
    sha256_update(&ctx, buffer, n);
  if(name)
    close(fd);
  sha256_final(&ctx, digest);
  return n;
}

static int
value(int c, const char *alphabet)
{
  for(int i = 0; alphabet[i]; i++)
    if(alphabet[i] == c)
      return i;
  return -1;
}

static int
decode(char *s, uchar digest[32])
{
  int a, b;
  if(strlen(s) == 64){
    for(int i = 0; i < 32; i++){
      a = value(s[i*2] | 32, digits);
      b = value(s[i*2+1] | 32, digits);
      if(a < 0 || b < 0)
        return -1;
      digest[i] = a*16+b;
    }
    return 0;
  }
  if(strlen(s) == 44 && s[43] == '='){
    uint bits = 0;
    int count = 0, out = 0;
    for(int i = 0; i < 43; i++){
      a = value(s[i], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
      if(a < 0)
        return -1;
      bits = (bits << 6) | a;
      count += 6;
      if(count >= 8){
        count -= 8;
        digest[out++] = bits >> count;
      }
    }
    return (bits & 3) ? -1 : 0;
  }
  return -1;
}

static int
verify(const char *list, int wanted, char **args)
{
  static char line[512], seen[64];
  uchar got[32], expected[32];
  int fd = open(list, O_RDONLY), n, used, bad = 0, matches = 0;
  char c, *name, *digest, *p;

  if(fd < 0 || wanted > sizeof seen)
    goto error;
  for(;;){
    used = 0;
    while((n = read(fd, &c, 1)) > 0 && c != '\n'){
      if(!c || used == sizeof line - 1)
        goto error;
      line[used++] = c;
    }
    if(n < 0)
      goto error;
    if(!used && !n)
      break;
    if(used && line[used-1] == '\r')
      used--;
    if(!used)
      continue;
    line[used] = 0;
    if(used > 8 && !dcompare(line, "SHA256 (", 8)){
      name = line+8;
      digest = 0;
      for(p = name; p[0] && p[1] && p[2] && p[3]; p++)
        if(p[0] == ')' && p[1] == ' ' && p[2] == '=' && p[3] == ' ')
          digest = p+4;
      if(!digest)
        goto error;
      digest[-4] = 0;
    } else {
      digest = line;
      for(p = line; *p && *p != ' '; p++)
        ;
      if(*p != ' ')
        goto error;
      *p++ = 0;
      name = (*p == '*' || *p == ' ') ? p+1 : p;
    }
    if(!*name || decode(digest, expected) < 0)
      goto error;
    int select = wanted == 0;
    for(int i = 0; i < wanted; i++)
      if(!strcmp(args[i], name)){
        select = 1;
        seen[i] = 1;
      }
    if(!select)
      continue;
    matches++;
    int ok = hash(name, got) == 0 && !dcompare(got, expected, 32);
    printf(ok ? 1 : 2, "%s: %s\n", name, ok ? "OK" : "FAILED");
    bad |= !ok;
  }
  close(fd);
  for(int i = 0; i < wanted; i++)
    if(!seen[i]){
      printf(2, "sha256: %s missing from checklist\n", args[i]);
      bad = 1;
    }
  return bad || !matches;

error:
  if(fd >= 0)
    close(fd);
  printf(2, "sha256: cannot read checklist %s\n", list);
  return 1;
}

int
main(int argc, char **argv)
{
  uchar digest[32];
  int errors = 0;

  if(argc > 1 && !strcmp(argv[1], "-C")){
    if(argc >= 3)
      return verify(argv[2], argc-3, argv+3);
    printf(2, "usage: sha256 [-C checklist] [file ...]\n");
    return 2;
  }
  for(int i = argc == 1 ? 0 : 1; i < argc; i++){
    char *name = i ? argv[i] : 0;
    if(hash(name, digest) < 0){
      printf(2, "sha256: cannot read %s\n", name ? name : "standard input");
      errors = 1;
      continue;
    }
    if(name)
      printf(1, "SHA256 (%s) = ", name);
    for(int j = 0; j < sizeof digest; j++){
      char pair[] = {digits[digest[j] >> 4], digits[digest[j] & 15]};
      write(1, pair, sizeof pair);
    }
    write(1, "\n", 1);
  }
  return errors;
}
