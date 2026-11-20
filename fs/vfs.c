/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "vfs.h"
#include "ext2.h"

// boot publishes filesystem operations before starting user programs
static const struct fsops *mounts[INSTALLDEV + 1];

void
vfs_bind(uint dev, const struct fsops *ops)
{
  if(!dev || dev >= NELEM(mounts) || mounts[dev])
    panic("invalid vfs device");
  mounts[dev] = ops;
}

const struct fsops *
vfs_ops(uint dev)
{
  if(dev >= NELEM(mounts) || !mounts[dev])
    panic("vfs device is not mounted");
  return mounts[dev];
}

void
vfs_unbind(uint dev)
{
  if(dev && dev < NELEM(mounts))
    mounts[dev] = 0;
}
