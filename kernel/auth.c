/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "stat.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "account.h"
#include "mode.h"
#include "sha256.h"

#define ROUNDS 600000
#define FIRST_UID 1000
#define MAX_ACCOUNTS 64
#define DB "/etc/shadow"
#define ROOT_NAME "root"

static struct sleeplock accounts;

void
accountinit(void)
{
  initsleeplock(&accounts, "accounts");
}

static void
wipe(void *p, uint n)
{
  volatile uchar *bytes = p;
  while(n--)
    *bytes++ = 0;
}

struct hmac {
  SHA256_CTX inner;
  SHA256_CTX outer;
};

static void
hmacinit(struct hmac *h, const uchar *key, uint len)
{
  uchar pad[64];
  int i;

  memset(pad, 0, sizeof(pad));
  if(len > sizeof(pad)){
    sha256_init(&h->inner);
    sha256_update(&h->inner, key, len);
    sha256_final(&h->inner, pad);
  } else {
    memmove(pad, key, len);
  }
  for(i = 0; i < sizeof(pad); i++)
    pad[i] ^= 0x36;
  sha256_init(&h->inner);
  sha256_update(&h->inner, pad, sizeof(pad));
  for(i = 0; i < sizeof(pad); i++)
    pad[i] ^= 0x36 ^ 0x5c;
  sha256_init(&h->outer);
  sha256_update(&h->outer, pad, sizeof(pad));
  wipe(pad, sizeof(pad));
}

static void
hmac(const struct hmac *h, const uchar *msg, uint len, uchar out[32])
{
  SHA256_CTX ctx = h->inner;

  sha256_update(&ctx, msg, len);
  sha256_final(&ctx, out);
  ctx = h->outer;
  sha256_update(&ctx, out, 32);
  sha256_final(&ctx, out);
  wipe(&ctx, sizeof(ctx));
}

static void
derive(const char *password, const struct account_record *r, uchar hash[32])
{
  struct hmac key;
  uchar block[ACCOUNT_SALT + 4], u[32];
  WORD words[8], result[8];
  uint i, j;

  hmacinit(&key, (const uchar*)password, strlen(password));
  memmove(block, r->salt, ACCOUNT_SALT);
  memset(block + ACCOUNT_SALT, 0, 3);
  block[ACCOUNT_SALT + 3] = 1;
  hmac(&key, block, sizeof(block), u);
  for(j = 0; j < 8; j++){
    __builtin_memcpy(&words[j], u + 4*j, sizeof(WORD));
    result[j] = words[j] = __builtin_bswap32(words[j]);
  }
  // u1 used the salt and block number each later round hashes a digest
  for(i = 1; i < r->rounds; i++){
    sha256_hmac_digest(&key.inner, words);
    sha256_hmac_digest(&key.outer, words);
    for(j = 0; j < 8; j++)
      result[j] ^= words[j];
    if((i & 4095) == 0)
      yield();
  }
  for(j = 0; j < 8; j++){
    WORD word = __builtin_bswap32(result[j]);
    __builtin_memcpy(hash + 4*j, &word, sizeof(word));
  }
  wipe(result, sizeof(result));
  wipe(words, sizeof(words));
  wipe(u, sizeof(u));
  wipe(&key, sizeof(key));
}

static int
matches(const char *password, struct account_record *r)
{
  uint diff = 0;
  int i;
  uchar hash[32];

  derive(password, r, hash);
  for(i = 0; i < sizeof(hash); i++)
    diff |= hash[i] ^ r->hash[i];
  wipe(hash, sizeof(hash));
  return diff == 0;
}

static void
newhash(const char *password, struct account_record *r,
        const struct account_header *header)
{
  struct hmac key;
  uchar input[ACCOUNT_SALT + sizeof(r->uid)], hash[32];

  hmacinit(&key, header->seed, sizeof(header->seed));
  memmove(input, &r->uid, sizeof(r->uid));
  memmove(input + sizeof(r->uid), r->salt, sizeof(r->salt));
  hmac(&key, input, sizeof(input), hash);
  memmove(r->salt, hash, sizeof(r->salt));
  r->rounds = ROUNDS;
  derive(password, r, r->hash);
  wipe(hash, sizeof(hash));
  wipe(&key, sizeof(key));
}

static int
validname(const char *name)
{
  int i;
  char c;

  if(*name < 'a' || *name > 'z')
    return 0;
  for(i = 1; (c = name[i]); i++)
    if(!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '_' && c != '-')
      return 0;
  return i < ACCOUNT_NAME;
}

static int
recordvalid(const struct account_record *r, uint off)
{
  if(r->rounds != ROUNDS || r->name[ACCOUNT_NAME - 1] || !validname(r->name))
    return 0;
  if(off == BSIZE)
    return r->uid == 0 && strncmp(r->name, ROOT_NAME, ACCOUNT_NAME) == 0;
  if(off < 2 * BSIZE || off / BSIZE - 2 >= MAX_ACCOUNTS)
    return 0;
  return r->uid == FIRST_UID + off / BSIZE - 2 &&
         strncmp(r->name, ROOT_NAME, ACCOUNT_NAME) != 0;
}

static void
homepath(struct account_info *info, const struct account_record *r)
{
  safestrcpy(info->name, r->name, sizeof(info->name));
  if(r->uid == 0){
    safestrcpy(info->home, "/root", sizeof(info->home));
  } else {
    safestrcpy(info->home, "/home/", sizeof(info->home));
    safestrcpy(info->home + 6, r->name, ACCOUNT_NAME);
  }
}

// the account lock covers lookup hashing and update
// keep the disk transaction short so other files can be used during hashing
static int
lookup(const char *name, int uid, struct account_record *r,
       struct account_header *header, uint *off)
{
  struct inode *ip;
  int result = -1;
  uint pos;

  begin_op();
  if((ip = namei(DB)) == 0)
    goto end;
  ilock(ip);
  if(ip->type != T_FILE || !(ip->flags & I_SHADOW) ||
     ip->size < BSIZE || ip->size % BSIZE ||
     ip->size > (MAX_ACCOUNTS + 2) * BSIZE ||
     readi(ip, (char*)header, 0, BSIZE) != BSIZE)
    goto put;
  for(pos = BSIZE; pos < ip->size; pos += BSIZE){
    if(readi(ip, (char*)r, pos, BSIZE) != BSIZE ||
       !recordvalid(r, pos))
      goto put;
    if((name && strncmp(name, r->name, ACCOUNT_NAME) == 0) || (!name && uid == r->uid)){
      result = 1;
      break;
    }
  }
  *off = pos;
  if(result != 1)
    result = 0;
put:
  iunlockput(ip);
end:
  end_op();
  return result;
}

static int
saverecord(const struct account_record *r, uint off)
{
  struct inode *ip;
  int result = -1;

  if(off < BSIZE || off % BSIZE ||
     off > (MAX_ACCOUNTS + 1) * BSIZE || !recordvalid(r, off))
    return -1;
  if((ip = namei(DB)) != 0){
    ilock(ip);
    if(ip->type == T_FILE && (ip->flags & I_SHADOW) &&
       ip->size >= BSIZE && ip->size % BSIZE == 0 &&
       ip->size <= (MAX_ACCOUNTS + 2) * BSIZE && off <= ip->size &&
       writei(ip, (char*)r, off, BSIZE) == BSIZE)
      result = 0;
    iunlockput(ip);
  }
  return result;
}

static int
save(const struct account_record *r, uint off)
{
  int result;

  begin_op();
  result = saverecord(r, off);
  end_op();
  return result;
}

int
sys_accountinfo(void)
{
  struct account_header header;
  struct account_record record;
  struct account_info info;
  char *dst;
  uint off;
  int uid, found;

  if(argint(0, &uid) < 0 || uid < 0 || uid > ID_MAX ||
     argptrwrite(1, &dst, sizeof(info)) < 0)
    return -1;
  acquiresleep(&accounts);
  found = lookup(0, uid, &record, &header, &off);
  releasesleep(&accounts);
  if(found == 1){
    memset(&info, 0, sizeof(info));
    homepath(&info, &record);
    memmove(dst, &info, sizeof(info));
  }
  wipe(&record, sizeof(record));
  wipe(&header, sizeof(header));
  return found == 1 ? 0 : -1;
}

int
sys_account(void)
{
  struct account_header header;
  struct account_record record;
  struct account_info info;
  struct inode *dir;
  int mode, len, found, result = -1, uid = myproc()->uid;
  uint gid = 0;
  uint off;
  char *name, *old, *new;

  if(argint(0, &mode) < 0 || (len = argstrn(1, &name, ACCOUNT_NAME)) < 0 ||
     len >= ACCOUNT_NAME || (len = argstrn(2, &old, ACCOUNT_PASSWORD)) < 0 ||
     len >= ACCOUNT_PASSWORD || (len = argstrn(3, &new, ACCOUNT_PASSWORD)) < 0 ||
     len >= ACCOUNT_PASSWORD || mode < ACCOUNT_VERIFY || mode > ACCOUNT_SETUP)
    return -1;
  if((mode != ACCOUNT_CHANGE && uid != 0) ||
     (mode == ACCOUNT_VERIFY && !ttyforeground()) ||
     (mode == ACCOUNT_CHANGE && uid != 0 && *name) ||
     (mode != ACCOUNT_VERIFY && len < ACCOUNT_MIN_PASSWORD) ||
     ((mode == ACCOUNT_ADD || *name) && !validname(name)))
    return -1;

  if(mode == ACCOUNT_ADD && *old){
    while(*old){
      uint digit = *old++ - '0';
      if(digit > 9 || gid > (ID_MAX - digit) / 10)
        return -1;
      gid = gid * 10 + digit;
    }
    if(gid == 0)
      return -1;
  }

  acquiresleep(&accounts);
  found = lookup(*name ? name : 0, uid, &record, &header, &off);
  if(found < 0)
    goto end;

  switch(mode){
  case ACCOUNT_VERIFY:
    if(!*name)
      break;
    if(found == 0){
      // hash unknown users too so the response time gives less away
      memset(&record, 0, sizeof(record));
      record.rounds = ROUNDS;
      matches(old, &record);
    } else if(matches(old, &record)){
      result = record.uid;
    }
    break;
  case ACCOUNT_CHANGE:
    if(found == 1 && (uid == 0 || matches(old, &record))){
      newhash(new, &record, &header);
      result = save(&record, off);
    }
    break;
  case ACCOUNT_SETUP:
    if(!ttyforeground() || off != BSIZE || found ||
       strncmp(name, ROOT_NAME, ACCOUNT_NAME) != 0)
      break;
    memset(&record, 0, sizeof(record));
    safestrcpy(record.name, ROOT_NAME, sizeof(record.name));
    newhash(new, &record, &header);
    result = save(&record, off);
    break;
  case ACCOUNT_ADD:
    if(found || off < 2 * BSIZE || off / BSIZE > MAX_ACCOUNTS)
      break;
    memset(&record, 0, sizeof(record));
    record.uid = FIRST_UID + off / BSIZE - 2;
    record.group[0] = gid;
    record.group[1] = gid >> 8;
    safestrcpy(record.name, name, sizeof(record.name));
    newhash(new, &record, &header);
    homepath(&info, &record);
    begin_op();
    // create the home directory and account in the same transaction
    dir = create(info.home, T_DIR, record.uid, gid ? gid : record.uid, 0700, 1, 0);
    if(dir){
      iunlockput(dir);
      result = saverecord(&record, off);
    }
    if(dir && result < 0)
      remove_path(info.home, 1);
    end_op();
    break;
  }
end:
  releasesleep(&accounts);
  wipe(&record, sizeof(record));
  wipe(&header, sizeof(header));
  return result;
}

int
sys_accountgid(void)
{
  struct account_header header;
  struct account_record record;
  uint off;
  int uid, result = -1;

  if(argint(0, &uid) < 0 || uid < 0 || uid > ID_MAX)
    return -1;
  acquiresleep(&accounts);
  if(lookup(0, uid, &record, &header, &off) == 1){
    result = record.group[0] | record.group[1] << 8;
    if(result == 0)
      result = uid;
  }
  releasesleep(&accounts);
  wipe(&record, sizeof(record));
  wipe(&header, sizeof(header));
  return result;
}
