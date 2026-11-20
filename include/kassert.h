/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_KASSERT_H
#define PROJECT9_KASSERT_H

void kassert_fail(const char *, const char *, int) __attribute__((noreturn));

#define KASSERT(condition) do { \
  if(!(condition)) \
    kassert_fail(#condition, __FILE__, __LINE__); \
} while(0)

#endif
