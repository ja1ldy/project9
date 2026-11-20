/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_FSCHECK_H
#define PROJECT9_FSCHECK_H

// all later disk offsets depend on these bounds
static inline int
fsgeometry(const struct superblock *s)
{
  uint first;

  if(s->magic != FS_MAGIC || s->size != FSSIZE ||
     !s->nblocks || s->nblocks >= s->size ||
     s->ninodes <= LEGACY_ROOTINO || s->ninodes > NINODE)
    return 0;

  first = s->size - s->nblocks;
  return s->logstart >= 2 && s->logstart < s->inodestart &&
         s->nlog > MAXOPBLOCKS && s->nlog <= LOGSIZE + 1 &&
         s->inodestart - s->logstart == s->nlog &&
         s->inodestart < s->bmapstart && s->bmapstart < first &&
         s->bmapstart - s->inodestart >= (s->ninodes + IPB - 1) / IPB &&
         first - s->bmapstart >= (s->size + BPB - 1) / BPB;
}

static inline int
journalentry(const struct superblock *s, uint block)
{
  return block >= s->inodestart && block < s->size;
}

#endif
