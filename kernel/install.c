/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "disk.h"
#include "diskctl.h"
#include "ext2.h"
#include "p9abi.h"

int
sys_diskctl(void)
{
  int op, id, sector, n, bytes, in;
  char *buffer;

  if(argint(0, &op) < 0 || argint(1, &id) < 0 || argint(2, &sector) < 0 ||
     argint(3, &n) < 0 || id < 0 || id >= DC_MAX || sector < 0 ||
     n < 1 || n > 8)
    return -P9_EINVAL;
  if(myproc()->uid && op != DC_INFO && op != DC_BOOT &&
     op != DC_STATS && op != DC_EXT2_STATS)
    return -P9_EPERM;
  in = 0;
  switch(op){
  case DC_INFO: bytes = sizeof(struct diskinfo); break;
  case DC_BOOT: bytes = sizeof(struct diskboot); break;
  case DC_STATS: bytes = sizeof(struct diskstats); break;
  case DC_EXT2_STATS: bytes = sizeof(struct ext2stats); break;
  case DC_SEED: bytes = 32; break;
  case DC_READ_MBR: bytes = DC_SECTOR; break;
  case DC_READ_PART: bytes = n * DC_SECTOR; break;
  case DC_MBR: case DC_FINISH: bytes = DC_SECTOR; in = 1; break;
  case DC_WRITE_PART: case DC_BOOTBLOCK: bytes = n * DC_SECTOR; in = 1; break;
  default: return -P9_EINVAL;
  }
  if((in ? argptr(4, &buffer, bytes) : argptrwrite(4, &buffer, bytes)) < 0)
    return -P9_EFAULT;
  if(op == DC_EXT2_STATS){
    struct ext2stats stats;
    ext2stats(&stats);
    memmove(buffer, &stats, sizeof(stats));
    return 0;
  }
  return diskcommand(op, id, sector, n, buffer);
}

int
sys_installmount(void)
{
  int id, undo, result;

  if(myproc()->uid || !diskislive())
    return -P9_EPERM;
  if(argint(0, &id) < 0 || argint(1, &undo) < 0 ||
     id < 0 || id >= DC_MAX || (undo != 0 && undo != 1))
    return -P9_EINVAL;
  result = fsfreeze();
  if(result < 0)
    return result;
  result = undo ? ext2installunmount() : ext2installmount(id);
  fsrelease();
  return result;
}

int
sys_sync(void)
{
  int result = fsfreeze();
  if(result < 0)
    return result;
  fsrelease();
  return 0;
}
