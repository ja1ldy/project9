/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"

int
main(int argc, char **argv)
{
  char buf[512];
  int used = 0, chunk, i;

  if(argc == 1){
    buf[used++] = 'y';
  } else {
    for(i = 1; i < argc; i++){
      int len = strlen(argv[i]);
      int separator = i < argc - 1;
      if(len > (int)sizeof(buf) - used - separator - 1){
        printf(2, "yes: argument line too long\n");
        exit();
      }
      memmove(buf+used, argv[i], len);
      used += len;
      if(separator)
        buf[used++] = ' ';
    }
  }
  buf[used++] = '\n';
  for(chunk = used; chunk + used <= sizeof(buf); chunk += used)
    memmove(buf+chunk, buf, used);
  while(write(1, buf, chunk) == chunk)
    ;
  exit();
}
