/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "wait.h"

#define SCRIPT "/tmp/v03script"
#define ERRFILE "/tmp/v03stderr"
#define CMDLINK "/tmp/v03hello"

static void
require(int ok, const char *message)
{
  if(!ok){
    printf(2, "v03test: %s\n", message);
    exitstatus(1);
  }
}

static void
check(char **args, const char *output, int expected)
{
  int p[2], pid, n, used = 0, status;
  char text[1024];

  require(pipe(p) == 0, "cannot create pipe");
  pid = fork();
  require(pid >= 0, "cannot fork");
  if(pid == 0){
    close(p[0]);
    require(dup2(p[1], 1) == 1, "cannot redirect stdout");
    close(p[1]);
    exec(args[0], args);
    exitstatus(126);
  }
  close(p[1]);
  while(used < sizeof(text)-1 && (n = read(p[0], text+used, sizeof(text)-1-used)) > 0)
    used += n;
  require(used < sizeof(text)-1 && n == 0, "unexpected shell output length");
  close(p[0]);
  require(waitpid(pid, &status, 0) == pid && WIFEXITED(status) &&
          WEXITSTATUS(status) == expected, "unexpected command status");
  text[used] = 0;
  if(strcmp(text, output)){
    printf(2, "v03test: expected [%s], got [%s]\n", output, text);
    exitstatus(1);
  }
}

static void
shell(char *command, const char *output, int expected)
{
  char *args[] = {"/bin/sh", "-c", command, 0};
  check(args, output, expected);
}

int
main(void)
{
  static const char body[] =
    "#!/bin/sh\n"
    "# Each argument stays one word.\n"
    "for value in \"$@\"; do printf '[%s]\\n' \"$value\"; done\n"
    "exit 17\n";
  char *explicit[] = {"/bin/sh", SCRIPT, "one two", "three", 0};
  char *executable[] = {SCRIPT, "one two", "three", 0};
  int fd;

  shell("A='one two'; echo \"$A\"; export A; printenv A; unset A; printenv A || echo unset",
        "one two\none two\nunset\n", 0);
  shell("TMPVAR='local value'; sh -c 'printenv TMPVAR' || echo local; TMPVAR=child printenv TMPVAR",
        "local\nchild\n", 0);
  shell("false && echo wrong || echo fallback; if [ 2 -lt 3 ]; then echo yes; else echo wrong; fi",
        "fallback\nyes\n", 0);
  shell("A=0; while [ \"$A\" -eq 0 ]; do echo loop; A=1; done; "
        "until true; do echo wrong; done; for x in a b; do echo \"$x\"; done",
        "loop\na\nb\n", 0);
  shell("if false; then echo wrong; elif true; then echo elif; else echo wrong; fi; "
        "for x in a b; do if [ \"$x\" = a ]; then continue; fi; echo \"$x\"; break; done",
        "elif\nb\n", 0);
  shell("false | true; echo $?; true | false; echo $?; (X=only; echo \"$X\"); echo \"$X\"",
        "0\n1\nonly\n\n", 0);
  shell("cd /tmp; echo \"$PWD\"; true 1>&- | cat; grep nowhere /no-v03-file 2>&-",
        "/tmp\n", 2);
  shell("exit 42", "", 42);

  unlink(ERRFILE);
  shell("for word in a b; do echo $word; done >" ERRFILE "; cat " ERRFILE,
        "a\nb\n", 0);
  unlink(ERRFILE);
  shell("cat /no-v03-file 2>" ERRFILE "; cat " ERRFILE,
        "cat: cannot open /no-v03-file\n", 0);
  shell("cat /no-v03-file 2>&1 | cat", "cat: cannot open /no-v03-file\n", 0);
  unlink(ERRFILE);

  unlink(CMDLINK);
  require(link("/bin/echo", CMDLINK) == 0, "cannot prepare PATH test");
  shell("PATH=/tmp:/bin; v03hello found", "found\n", 0);
  unlink(CMDLINK);

  unlink(SCRIPT);
  require((fd = open(SCRIPT, O_CREATE | O_WRONLY | O_EXCL)) >= 0,
          "cannot create script");
  require(write(fd, body, sizeof(body)-1) == sizeof(body)-1, "cannot write script");
  close(fd);
  check(explicit, "[one two]\n[three]\n", 17);
  require(chmod(SCRIPT, 0755) == 0, "cannot make script executable");
  check(executable, "[one two]\n[three]\n", 17);
  unlink(SCRIPT);

  shell("printf 'a:b:c\\n' | cut -d : -f 2", "b\n", 0);
  shell("env -i ONE=two printenv ONE", "two\n", 0);
  shell("man sh | grep '^sh -'", "sh - run commands and shell scripts\n", 0);
  printf(1, "V0.3 TESTS PASSED\n");
  return 0;
}
