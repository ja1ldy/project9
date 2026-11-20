/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "../shell/sh.h"
#include "user.h"
#include "stat.h"
#include "fs.h"
#include "fcntl.h"
#include "tty.h"

int sh_interactive;
volatile int sh_interrupted;
int sh_status;
char **sh_positionals;
int sh_poscount;

static void
interrupt(int sig)
{
  sh_interrupted = 1;
}

void
sh_chdir(void)
{
  char pwd[512];

  if(getcwd(pwd, sizeof(pwd)) >= 0)
    setvar("PWD", pwd, 1);
}

void
sh_prompt(void)
{
  char hostname[32], pwd[512];
  const char *user = lookvar("USER");
  int fd, n = 0;

  if((fd = open("/etc/hostname", O_RDONLY)) >= 0){
    n = read(fd, hostname, sizeof(hostname)-1);
    close(fd);
  }
  if(n <= 0){
    strcpy(hostname, "project9");
  } else {
    while(n && (hostname[n-1] == '\n' || hostname[n-1] == '\r'))
      n--;
    hostname[n] = 0;
  }
  if(getcwd(pwd, sizeof(pwd)) < 0)
    strcpy(pwd, "?");
  printf(2, "%s@%s:%s%c ", user ? user : "user", hostname, pwd, getuid() ? '$' : '#');
}

static int
runtext(const char *text, int final)
{
  struct command *head;
  const char *message = 0;
  int status = parse(text, &head, &message);

  if(status == 2 && !final)
    return 1;
  if(status){
    printf(2, "sh: syntax: %s\n", message);
    sh_status = 2;
  } else {
    if(head)
      sh_status = evaluate(head);
    discard_parse();
  }
  return 0;
}

static int
script(int fd)
{
  char *text;
  int used = 0, n, i;
  int limit = 256 * 1024;

  if(!(text = malloc(limit+1))){
    printf(2, "sh: cannot allocate script buffer\n");
    return 2;
  }
  while(used < limit && (n = read(fd, text + used, limit - used)) > 0)
    used += n;
  if(used == limit)
    n = read(fd, text + used, 1);
  if(n != 0){
    printf(2, "sh: cannot read script or script is too large\n");
    free(text);
    return 2;
  }
  if(fd != 0)
    close(fd);
  for(i = 0; i < used; i++)
    if(text[i] == 0){
      printf(2, "sh: script contains a NUL byte\n");
      free(text);
      return 2;
    }
  text[used] = 0;
  runtext(text, 1);
  free(text);
  return sh_status;
}

static void
interactive(void)
{
  static char input[SH_INPUT];
  char c;
  int fd, used = 0, n, overflow;

  while((fd = open("/dev/console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }
  signal(SIGINT, interrupt);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  sh_interactive = 1;
  for(;;){
    sh_interrupted = 0;
    overflow = 0;
    report_jobs(0);
    if(used)
      printf(2, "> ");
    else
      sh_prompt();
    n = 1;
    while(!sh_interrupted && (n = read(0, &c, 1)) == 1){
      if(used == sizeof(input)-1 || c == 0)
        overflow = 1;
      else
        input[used++] = c;
      if(c == '\n')
        break;
    }
    if(sh_interrupted){
      used = 0;
      sh_status = 130;
      continue;
    }
    if(overflow){
      printf(2, "sh: input is too long or contains a NUL byte\n");
      used = 0;
      sh_status = 2;
    } else if(used){
      input[used] = 0;
      if(!runtext(input, n == 0))
        used = 0;
    }
    if(n <= 0)
      exitstatus(sh_status);
  }
}

int
main(int argc, char **argv)
{
  struct stat st;
  int fd;

  if(initvars() < 0){
    printf(2, "sh: could not initialize variables\n");
    return 2;
  }
  sh_positionals = argv;
  sh_poscount = 1;
  if(argc > 1 && !strcmp(argv[1], "-c")){
    if(argc < 3){
      printf(2, "usage: sh -c command [name [argument ...]]\n");
      return 2;
    }
    if(argc > 3){
      sh_positionals = argv + 3;
      sh_poscount = argc - 3;
    }
    runtext(argv[2], 1);
    return sh_status;
  }
  if(argc > 1){
    sh_positionals = argv + 1;
    sh_poscount = argc - 1;
    if((fd = open(argv[1], O_RDONLY)) < 0){
      printf(2, "sh: cannot open %s\n", argv[1]);
      return 126;
    }
    return script(fd);
  }
  if(fstat(0, &st) < 0 || st.type == T_DEV)
    interactive();
  return script(0);
}
