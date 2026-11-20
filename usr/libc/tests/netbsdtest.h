/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_NETBSDTEST_H
#define PROJECT9_NETBSDTEST_H
#include <stdio.h>
#include <stdlib.h>
static void
require(int ok, const char *label)
{
  if(!ok){
    fprintf(stderr, "netbsdtest: %s\n", label);
    exit(1);
  }
}
#endif
