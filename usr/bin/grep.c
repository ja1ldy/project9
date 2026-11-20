/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

// a small grep for the original line oriented regular expressions
#include "types.h"
#include "user.h"
#include "text.h"

static char line[TEXT_LINE];
int match(char*, char*);

static int
grep(char *pattern, int fd)
{
  struct text_input in = {.fd = fd};
  int n, found = 0;

  while((n = textline(&in, line, sizeof(line))) > 0){
    if(match(pattern, line)){
      n = strlen(line);
      if(write(1, line, n) != n || write(1, "\n", 1) != 1)
        return 2;
      found = 1;
    }
  }
  if(n < 0){
    printf(2, "grep: read error or invalid text line\n");
    return 2;
  }
  return !found;
}

int
main(int argc, char **argv)
{
  int fd, i, status = 1, result;

  if(argc <= 1 || strlen(argv[1]) > 128){
    printf(2, "usage: grep pattern [file ...] (maximum pattern length 128)\n");
    return 2;
  }
  if(argc == 2)
    return grep(argv[1], 0);
  for(i = 2; i < argc; i++){
    if((fd = open(argv[i], 0)) < 0){
      printf(2, "grep: cannot open %s\n", argv[i]);
      status = 2;
      continue;
    }
    result = grep(argv[1], fd);
    close(fd);
    if(result == 2 || (result == 0 && status == 1))
      status = result;
  }
  return status;
}

// Regexp matcher from Kernighan & Pike,
// The Practice of Programming, Chapter 9.

int matchhere(char*, char*);
int matchstar(int, char*, char*);

int
match(char *re, char *text)
{
  if(re[0] == '^')
    return matchhere(re+1, text);
  do{  // must look at empty string
    if(matchhere(re, text))
      return 1;
  }while(*text++ != '\0');
  return 0;
}

// matchhere: search for re at beginning of text
int matchhere(char *re, char *text)
{
  for(; *re; re++, text++){
    if(re[1] == '*')
      return matchstar(re[0], re+2, text);
    if(re[0] == '$' && !re[1])
      return !*text;
    if(!*text || (re[0] != '.' && re[0] != *text))
      return 0;
  }
  return 1;
}

// matchstar: search for c*re at beginning of text
int matchstar(int c, char *re, char *text)
{
  do{  // a * matches zero or more instances
    if(matchhere(re, text))
      return 1;
  }while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}

