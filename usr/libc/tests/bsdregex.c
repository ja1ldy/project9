/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "netbsdtest.h"
#include <dirent.h>
#include <regex.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>

_Static_assert(sizeof(struct p9_stat_record) == 20, "i386 stat record");
_Static_assert(sizeof(struct p9_statx_record) == 32, "i386 statx record");
_Static_assert(offsetof(struct p9_statx_record, uid) == 20, "statx privilege fields");
_Static_assert(sizeof(struct p9_dirent_record) == 16, "directory record size");
_Static_assert(SYS__exit == SYS_exitstatus, "exit status syscall");

static int
compare(const void *a, const void *b)
{
  int x = *(const int *)a, y = *(const int *)b;
  return (x > y) - (x < y);
}

static void
strings(void)
{
  char data[24], *copy, *last;
  int values[] = {4, -3, 99, 0, -3, 4};

  copy = strdup("abc,def");
  require(copy && strlcpy(data, copy, sizeof(data)) == 7, "strdup and strlcpy");
  require(!strcmp(strtok_r(data, ",", &last), "abc") &&
          !strcmp(strtok_r(NULL, ",", &last), "def"), "strtok_r");
  require(memchr(copy, ',', 7) == copy+3 && strspn(copy, "abc") == 3 &&
          strcspn(copy, ",") == 3 && strstr(copy, "def") == copy+4, "string searches");
  memmove(copy+1, copy, 5);
  require(!memcmp(copy, "aabc,d", 6), "overlapping memmove");
  free(copy);
  qsort(values, sizeof(values)/sizeof(*values), sizeof(*values), compare);
  require(values[0] == -3 && values[1] == -3 && values[2] == 0 &&
          values[3] == 4 && values[4] == 4 && values[5] == 99, "qsort");
}

static void
patterns(void)
{
  char error[48];
  regex_t re;
  regmatch_t match[3];
  int n;

  require(regcomp(&re, "([[:alpha:]]+)-([[:digit:]]+)", REG_EXTENDED) == 0, "regcomp");
  require(regexec(&re, "  tag-42.", 3, match, 0) == 0 &&
          match[0].rm_so == 2 && match[0].rm_eo == 8 &&
          match[1].rm_so == 2 && match[1].rm_eo == 5 &&
          match[2].rm_so == 6 && match[2].rm_eo == 8, "regex captures");
  require(regexec(&re, "a-null", 0, NULL, 0) == REG_NOMATCH, "regex no match");
  regfree(&re);
  require(regcomp(&re, "^\\(yes\\) \\1$", 0) == 0 &&
          regexec(&re, "yes yes", 0, NULL, 0) == 0, "basic regex backreference");
  regfree(&re);
  n = regcomp(&re, "[", REG_EXTENDED);
  require(n != 0 && regerror(n, NULL, error, sizeof(error)) > 1, "regex diagnostics");
}

int
main(void)
{
  strings();
  patterns();
  return 0;
}
