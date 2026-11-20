/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "version.h"
#include "user.h"
#include "fcntl.h"
#include "account.h"
#include "tty.h"

static void
motd(void)
{
  char buf[128];
  int n, fd = open("/etc/motd", O_RDONLY);

  if(fd < 0)
    return;
  while((n = read(fd, buf, sizeof(buf))) > 0)
    write(1, buf, n);
  close(fd);
}

int
main(void)
{
  struct account_info info;
  char user[ACCOUNT_NAME], password[ACCOUNT_PASSWORD] = {0};
  char *shell[] = {"/bin/sh", 0};
  int failures = 0, uid, gid;
  const struct termios defaults = TTY_DEFAULTS;

  // each login starts with line editing and echo enabled
  tcsetattr(0, TCSAFLUSH, &defaults);

  printf(1, "%s\n", PROJECT9_RELEASE);

  signal(SIGQUIT, SIG_IGN);
  signal(SIGINT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);

  if(accountinfo(0, &info) < 0){
    printf(1, "First boot. Set the root password at the console.\n");
    if(newpassword(password) == 0 &&
       account(ACCOUNT_SETUP, "root", "", password) == 0){
      scrub(password, sizeof(password));
      printf(1, "Root password set. Sign in as root.\n");
    } else {
      scrub(password, sizeof(password));
      sleep(100);
      exit();
    }
  }

  for(;;){
    printf(1, "project9 login: ");
    if(readline(user, sizeof(user)) <= 0)
      continue;
    uid = -1;
    if(readpassword("Password: ", password) >= 0)
      uid = account(ACCOUNT_VERIFY, user, password, "");
    scrub(password, sizeof(password));
    if(uid >= 0){
      gid = accountgid(uid);
      if(gid < 0 || setgid(gid) < 0 || setuid(uid) < 0 || accountinfo(uid, &info) < 0 || chdir(info.home) < 0){
        printf(2, "login: cannot start session\n");
        exit();
      }
      motd();
      exec(shell[0], shell);
      printf(2, "login: cannot start /bin/sh\n");
      exit();
    }
    printf(2, "Login incorrect\n");
    if(++failures >= 3){
      sleep(500);
      failures = 0;
    } else {
      sleep(100);
    }
  }
}
