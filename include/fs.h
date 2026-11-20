/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "p9records.h"

#ifndef P9_LEGACY_ROOT
#define ROOTINO 2
#endif
#define BSIZE 512
#define P9_PATH_MAX 512
#define I_SYMLINK 0x4000
#define I_SHADOW 0x8000
