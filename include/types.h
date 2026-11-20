/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned char  uchar;
typedef unsigned long uptr;
#ifdef P9_KERNEL64
typedef uptr pde_t;
#else
typedef uint pde_t;
#endif
