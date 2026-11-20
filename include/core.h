/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_CORE_H
#define PROJECT9_CORE_H
#include "trapframe.h"

#define CORE_MAGIC 0x39524350U  // pcr9 as bytes
#define CORE_VERSION 1

struct core_header {
  unsigned int magic, version, pid, uid, signal, memory_size, saved_size;
  struct trapframe32 registers;
};

#endif
