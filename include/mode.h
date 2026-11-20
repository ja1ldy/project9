/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_MODE_H
#define PROJECT9_MODE_H

#define M_EXEC   1
#define M_WRITE  2
#define M_READ   4
#define M_PERM   0777
#define M_STICKY 01000
#define M_SETGID 02000
#define M_VALID  (M_PERM | M_STICKY | M_SETGID)
#define ID_MAX   65535

#endif
