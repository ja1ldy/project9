/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_XSTATE_H
#define P9_XSTATE_H

#define XSTATE_BYTES 832
#define XSTATE_ALIGN 64

void fpuinit(void);
const char *fpumode(void);
void fpureset(void);
void fpusave(void *);
void fpurestore(const void *);

#endif
