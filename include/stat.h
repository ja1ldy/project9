/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_STAT_H
#define PROJECT9_STAT_H
#include "p9records.h"

#define T_DIR P9_T_DIR
#define T_FILE P9_T_FILE
#define T_DEV P9_T_DEV
#define T_SOCKET P9_T_SOCKET

struct stat { P9_STAT_FIELDS; };
struct statx { P9_STATX_FIELDS(stat); };
#endif
