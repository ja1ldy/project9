/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "diskuser.h"
#include "fcntl.h"
#include "stat.h"

#define MAXOUTPUT (1024U*1024*1024)
#define HISTORY 32768
#define RECORD 512
#define PATHLEN 256

int readlink(const char *, char *, int);

struct huffman {
  ushort counts[16];
  ushort symbols[288];
};

static struct huffman literal, distance;
static uchar input[4096], output[4096], window[HISTORY], record[RECORD];
static uint hold, total, crc = ~0U;
static int fd, pos, end, available, failed, state, final, remaining, back;

static int
source(void)
{
  if(pos == end){
    end = read(fd, input, sizeof input);
    pos = 0;
    if(end <= 0){
      failed |= end < 0;
      end = 0;
      return -1;
    }
  }
  return input[pos++];
}

static uint
bits(int n)
{
  while(available < n){
    int c = source();
    if(c < 0){
      failed = 1;
      return 0;
    }
    hold |= (uint)c << available;
    available += 8;
  }
  uint value = hold & ((1U << n)-1);
  hold >>= n;
  available -= n;
  return value;
}

static int
gzip_header(void)
{
  if(source() != 31 || source() != 139 || source() != 8)
    return -1;
  int flags = source(), budget = 8192;
  if(flags < 0 || (flags & 0xe0))
    return -1;
  for(int i = 0; i < 6; i++)
    if(source() < 0)
      return -1;
  if(flags & 4){
    uint n = bits(16);
    if(n > (uint)budget)
      return -1;
    budget -= n;
    while(n--)
      if(source() < 0)
        return -1;
  }
  for(int flag = 8; flag <= 16; flag <<= 1){
    if(!(flags & flag))
      continue;
    int c;
    do {
      c = source();
      if(c < 0 || !budget--)
        return -1;
    } while(c);
  }
  if(flags & 2)
    return -1;
  return failed ? -1 : 0;
}

static int
tree(struct huffman *h, const uchar *lens, int n)
{
  int left = 1;
  ushort offsets[16];

  memset(h->counts, 0, sizeof h->counts);
  for(int i = 0; i < n; i++)
    h->counts[lens[i]]++;
  offsets[1] = 0;
  for(int len = 1; len <= 15; len++){
    left = 2*left-h->counts[len];
    if(left < 0)
      return -1;
    if(len < 15)
      offsets[len+1] = offsets[len] + h->counts[len];
  }
  for(int i = 0; i < n; i++)
    if(lens[i])
      h->symbols[offsets[lens[i]]++] = i;
  return 0;
}

static int
symbol(const struct huffman *h)
{
  uint code = 0, first = 0, index = 0;
  for(int len = 1; len <= 15; len++){
    code |= bits(1);
    uint count = h->counts[len];
    if(failed)
      break;
    if(code-first < count)
      return h->symbols[index+code-first];
    index += count;
    first = (first+count)*2;
    code *= 2;
  }
  return -1;
}

static int
compressed(int fixed)
{
  uchar lens[320], order[] = {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
  int nl = 288, nd = 32;

  if(fixed){
    for(int i = 0; i < nl; i++)
      lens[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
    memset(lens+nl, 5, nd);
  } else {
    struct huffman codes;
    uchar code_lens[19] = {0};
    nl = 257 + bits(5);
    nd = 1 + bits(5);
    int nc = 4 + bits(4);
    if(nl > 286)
      return -1;
    for(int i = 0; i < nc; i++)
      code_lens[order[i]] = bits(3);
    if(tree(&codes, code_lens, sizeof code_lens) < 0)
      return -1;
    for(int used = 0; used < nl+nd;){
      int c = symbol(&codes), repeat = 1, len = c;
      if(c < 0)
        return -1;
      if(c == 16){
        if(!used)
          return -1;
        len = lens[used-1];
        repeat = 3 + bits(2);
      } else if(c == 17 || c == 18){
        len = 0;
        repeat = c == 17 ? 3 + bits(3) : 11 + bits(7);
      }
      if(repeat > nl+nd-used || failed)
        return -1;
      while(repeat--)
        lens[used++] = len;
    }
  }
  return !lens[256] || tree(&literal, lens, nl) < 0 ||
         tree(&distance, lens+nl, nd) < 0 ? -1 : 0;
}

static int
next(void)
{
  static const ushort bases[] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,
                                 35,43,51,59,67,83,99,115,131,163,195,227,258};
  static const ushort offsets[] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
                                   257,385,513,769,1025,1537,2049,3073,4097,
                                   6145,8193,12289,16385,24577};
  int c;

  while(!failed){
    if(state == 3)
      return -1;
    if(back){
      c = window[(total-back) & (HISTORY-1)];
      if(!--remaining)
        back = 0;
      goto emit;
    }
    if(state == 0){
      if(final){
        bits(available & 7);
        uint expected = bits(16);
        expected |= bits(16) << 16;
        uint size = bits(16);
        size |= bits(16) << 16;
        if(expected != ~crc || size != total || source() >= 0)
          goto invalid;
        state = 3;
        return -1;
      }
      final = bits(1);
      uint kind = bits(2);
      if(kind == 0){
        bits(available & 7);
        remaining = bits(16);
        if(bits(16) != (uint)(remaining ^ 65535))
          goto invalid;
        state = remaining ? 1 : 0;
      } else {
        if(kind == 3 || compressed(kind == 1) < 0)
          goto invalid;
        state = 2;
      }
      continue;
    }
    if(state == 1){
      c = bits(8);
      if(!--remaining)
        state = 0;
      goto emit;
    }
    c = symbol(&literal);
    if(c < 0 || c > 285)
      goto invalid;
    if(c < 256)
      goto emit;
    if(c == 256){
      state = 0;
      continue;
    }
    int length = c-257;
    remaining = bases[length] + bits(length < 8 || length == 28 ? 0 : length/4-1);
    c = symbol(&distance);
    if(c < 0 || c >= 30)
      goto invalid;
    back = offsets[c] + bits(c < 4 ? 0 : c/2-1);
    if((uint)back > total || failed)
      goto invalid;
  }
invalid:
  failed = 1;
  return -1;

emit:
  if(failed || total == MAXOUTPUT)
    goto invalid;
  window[total++ & (HISTORY-1)] = c;
  crc ^= c;
  for(int i = 0; i < 8; i++)
    crc = (crc >> 1) ^ (0xedb88320U & -(crc & 1));
  return c;
}

static int
tar_record(void)
{
  for(int i = 0; i < RECORD; i++){
    int c = next();
    if(c < 0)
      return -1;
    record[i] = c;
  }
  return 0;
}

static int
zero(const uchar *s, uint n)
{
  while(n--)
    if(*s++)
      return 0;
  return 1;
}

static int
octal(const uchar *s, int n, uint *out)
{
  uint a = 0;
  int used = 0;
  while(n && *s == ' '){
    s++;
    n--;
  }
  while(n && *s >= '0' && *s <= '7'){
    if(a > 0x1fffffff)
      return -1;
    a = a*8 + *s++-'0';
    used++;
    n--;
  }
  while(n--){
    if(*s && *s != ' ')
      return -1;
    s++;
  }
  if(!used)
    return -1;
  *out = a;
  return 0;
}

static int
directory(const char *name)
{
  char ignored[1];
  struct stat s;
  return readlink(name, ignored, sizeof ignored) < 0 &&
         stat(name, &s) == 0 && s.type == T_DIR && s.dev == DC_VOLUME;
}

static int
path(char *name, const char *base)
{
  int used = strlen(base), start = used+1;
  char *p = (char*)record;

  if(!zero(record+345, 155))
    return -1;
  if(p[0] == '.' && p[1] == '/')
    p += 2;
  strcpy(name, base);
  name[used++] = '/';
  for(; p < (char*)record+100 && *p; p++){
    if(*p < 32 || *p > 126 || used == PATHLEN-1)
      return -1;
    if(*p == '/'){
      if(used == start || (name[start] == '.' && (used == start+1 ||
          (used == start+2 && name[start+1] == '.'))))
        return -1;
      name[used] = 0;
      if(p+1 < (char*)record+100 && !p[1])
        return 0;
      if(!directory(name))
        return -1;
      start = used+1;
    }
    name[used++] = *p;
  }
  if(used == start || (name[start] == '.' && (used == start+1 ||
     (used == start+2 && name[start+1] == '.'))))
    return -1;
  name[used] = 0;
  return 0;
}

static int
extract(const char *base)
{
  char name[PATHLEN], link[101];
  uint mode, uid, gid, size, check, sum;
  int out, c, type;

  if(!directory("/mnt") || !directory(base) || gzip_header() < 0)
    return -1;
  while(tar_record() == 0){
    if(zero(record, sizeof record)){
      if(tar_record() < 0 || !zero(record, sizeof record))
        return -1;
      while((c = next()) == 0)
        ;
      return c < 0 && !failed && state == 3 ? 0 : -1;
    }
    if(dcompare(record+257, "ustar", 5) || path(name, base) < 0 ||
       octal(record+100, 8, &mode) || octal(record+108, 8, &uid) ||
       octal(record+116, 8, &gid) || octal(record+124, 12, &size) ||
       octal(record+148, 8, &check) || uid || gid || (mode & ~01777U) ||
       size > MAXOUTPUT)
      return -1;
    sum = 0;
    for(int i = 0; i < RECORD; i++)
      sum += i >= 148 && i < 156 ? ' ' : record[i];
    if(sum != check)
      return -1;
    type = record[156];
    if(type == '5'){
      if(size || (!directory(name) && mkdir(name) < 0) || !directory(name) ||
         chmod(name, mode) < 0)
        return -1;
      continue;
    }
    if(type == '2'){
      memmove(link, record+157, 100);
      link[100] = 0;
      if(size || !((!strcmp(name, "/mnt/bin/tar") && !strcmp(link, "pax")) ||
                   (!strcmp(name, "/mnt/bin/poweroff") && !strcmp(link, "halt"))) ||
         symlink(link, name) < 0)
        return -1;
      continue;
    }
    if(type && type != '0')
      return -1;
    out = open(name, O_WRONLY | O_CREATE | O_EXCL);
    if(out < 0)
      return -1;
    int good = 1;
    while(size && good){
      uint used = 0;
      while(size && used < sizeof output){
        uint n = size < RECORD ? size : RECORD;
        if(tar_record() < 0){
          good = 0;
          break;
        }
        memmove(output+used, record, n);
        used += n;
        size -= n;
      }
      if(good && write(out, output, used) != used)
        good = 0;
    }
    if(good && fchmod(out, mode) < 0)
      good = 0;
    if(close(out) < 0 || !good)
      return -1;
  }
  return -1;
}

int
main(int argc, char **argv)
{
  if(argc != 5 || (strcmp(argv[1], "-rzf") && strcmp(argv[1], "-xzf")) ||
     strcmp(argv[3], "-C") || (strcmp(argv[4], "/mnt") && strcmp(argv[4], "/mnt/usr"))){
    printf(2, "usage: pax -rzf archive.tar.gz -C /mnt[/usr]\n");
    return 2;
  }
  fd = open(argv[2], O_RDONLY);
  if(fd < 0 || extract(argv[4]) < 0){
    printf(2, "pax: extraction failed for %s\n", argv[2]);
    return 1;
  }
  close(fd);
  return 0;
}
