/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "reboot.h"
#include "p9abi.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "mode.h"
#include "tty.h"
#include "wait.h"

#define DIR "/tmp/p9-perms"
#define DATA DIR "/data"
#define NEW DIR "/new"
#define SECRET DIR "/secret"
#define PROGRAM DIR "/program"
#define STICKY "/tmp/p9-sticky"
#define GROUP 7000
#define OWNER 6000

static volatile int child_signal;

static void
require(int ok, const char *message)
{
  if(!ok){
    printf(2, "v02test: %s\n", message);
    exitstatus(1);
  }
}

static void
collect(int pid, int expected)
{
  int status;

  require(waitpid(pid, &status, 0) == pid, "waitpid did not collect the child");
  require(WIFEXITED(status) && WEXITSTATUS(status) == expected, "unexpected child exit status");
}

static void
child_handler(int sig)
{
  child_signal = sig;
}

static void
term_handler(int sig)
{
  exitstatus(sig == SIGTERM ? 23 : 1);
}

static void
processes(void)
{
  int pid, other, parent = getpid(), status, pipefd[2];
  char byte;

  require(signal(SIGSTOP, SIG_IGN) == (void (*)(int))-1, "SIGSTOP could be ignored");
  pid = fork();
  require(pid >= 0, "fork failed");
  if(pid == 0){
    require(getppid() == parent, "getppid returned the wrong parent");
    exitstatus(37);
  }
  other = fork();
  require(other >= 0, "second fork failed");
  if(other == 0)
    exitstatus(42);
  collect(other, 42);
  collect(pid, 37);
  require(waitpid(-1, 0, WNOHANG) == -1, "waitpid reported nonexistent children");
  pid = forkjob(JOB_BACKGROUND);
  require(pid >= 0, "could not start a separate process group");
  if(pid == 0)
    exitstatus(12);
  require(waitpid(-pid, &status, 0) == pid &&
          WIFEXITED(status) && WEXITSTATUS(status) == 12, "group wait failed");

  require(pipe(pipefd) == 0, "pipe failed");
  pid = fork();
  require(pid >= 0, "fork for stop test failed");
  if(pid == 0){
    close(pipefd[1]);
    read(pipefd[0], &byte, 1);
    exitstatus(0);
  }
  close(pipefd[0]);
  require(waitpid(pid, &status, WNOHANG) == 0, "WNOHANG did not return 0");
  require(send_signal(pid, SIGSTOP) == 0, "SIGSTOP failed");
  require(waitpid(pid, &status, WUNTRACED) == pid &&
          WIFSTOPPED(status) && WSTOPSIG(status) == SIGSTOP, "stop was not reported");
  require(waitpid(pid, &status, WUNTRACED | WNOHANG) == 0, "stop was reported twice");
  require(send_signal(pid, SIGCONT) == 0, "SIGCONT failed");
  require(waitpid(pid, &status, WCONTINUED) == pid && WIFCONTINUED(status), "resume was not reported");
  require(waitpid(pid, &status, WCONTINUED | WNOHANG) == 0, "resume was reported twice");
  require(send_signal(pid, SIGTERM) == 0, "SIGTERM failed");
  require(waitpid(pid, &status, 0) == pid && WIFSIGNALED(status) &&
          WTERMSIG(status) == SIGTERM, "default SIGTERM did not terminate the child");
  close(pipefd[1]);

  require(signal(SIGCHLD, child_handler) != (void (*)(int))-1, "could not catch SIGCHLD");
  require(pipe(pipefd) == 0, "readiness pipe failed");
  pid = fork();
  require(pid >= 0, "fork for handler test failed");
  if(pid == 0){
    close(pipefd[0]);
    signal(SIGTERM, term_handler);
    require(write(pipefd[1], "y", 1) == 1, "could not signal readiness");
    for(;;)
      sleep(20);
  }
  close(pipefd[1]);
  require(read(pipefd[0], &byte, 1) == 1, "child did not become ready");
  close(pipefd[0]);
  require(send_signal(pid, SIGTERM) == 0, "could not send a caught signal");
  collect(pid, 23);
  require(child_signal == SIGCHLD, "SIGCHLD handler did not run");
  signal(SIGCHLD, SIG_DFL);
  printf(1, "v02test: process checks passed\n");
}

static void
assume(int uid, int gid)
{
  require(setgid(gid) == 0 && setuid(uid) == 0, "could not set test identity");
  require(getuid() == uid && getgid() == gid, "wrong credentials after identity change");
}

static void
execution(void)
{
  char buf[512];
  char *args[] = {PROGRAM, "v02test: execute-only access worked", 0};
  int src, dst, n, pid;

  src = open("/bin/echo", O_RDONLY);
  dst = open(PROGRAM, O_CREATE | O_WRONLY | O_EXCL);
  require(src >= 0 && dst >= 0, "could not copy the test executable");
  while((n = read(src, buf, sizeof(buf))) > 0)
    require(write(dst, buf, n) == n, "could not write the test executable");
  require(n == 0, "could not read the test executable");
  close(src);
  close(dst);
  require(chmod(PROGRAM, 0644) == 0, "could not clear execute permissions");
  pid = fork();
  require(pid >= 0, "could not fork the execute check");
  if(pid == 0){
    require(exec(PROGRAM, args) == -1, "root executed a file with no execute bits");
    exitstatus(37);
  }
  collect(pid, 37);

  require(chmod(PROGRAM, 0710) == 0, "could not set execute-only permission");
  pid = fork();
  require(pid >= 0, "could not fork the execute-only check");
  if(pid == 0){
    assume(6002, GROUP);
    require(open(PROGRAM, O_RDONLY) == -1, "group could read execute-only file");
    exec(PROGRAM, args);
    exitstatus(1);
  }
  collect(pid, 0);
  require(unlink(PROGRAM) == 0, "could not remove the test executable");
}

static void
permissions(void)
{
  struct statx info;
  int pid, fd, parent = getpid();

  require(open("/etc/shadow", O_RDONLY) == -1 &&
          chmod("/etc/shadow", 0644) == -1, "shadow should remain kernel-only");
  require(mkdir(DIR) == 0, "could not create test directory");
  require(chown(DIR, 0, GROUP) == 0 && chmod(DIR, 02771) == 0, "could not set directory group");
  fd = open(DATA, O_CREATE | O_WRONLY | O_EXCL);
  require(fd >= 0 && write(fd, "test\n", 5) == 5, "could not create test file");
  close(fd);
  require(chown(DATA, OWNER, GROUP) == 0 && chmod(DATA, 0640) == 0, "could not set file mode");

  fd = open(STICKY, O_CREATE | O_WRONLY | O_EXCL);
  require(fd >= 0, "could not create sticky-directory test file");
  close(fd);

  pid = fork();
  require(pid >= 0, "could not fork the owner check");
  if(pid == 0){
    assume(OWNER, 6001);
    require(setuid(0) == -1 && setgid(0) == -1, "non-root gained privileged credentials");
    require(reboot(RB_POWEROFF) == -P9_EPERM && reboot(RB_REBOOT) == -P9_EPERM,
            "a non root user could shut down the system");
    require(send_signal(parent, 0) == -1, "non-root could signal a different user");
    require(mknod("/tmp/v02-device", 2, 0) == -1, "non-root could create a device node");
    fd = open(DATA, O_WRONLY);
    require(fd >= 0, "owner could not write");
    close(fd);
    require(chmod(DATA, 0660) == 0 && chown(DATA, 0, -1) == -1, "owner privilege check failed");
    require(unlink(STICKY) == -1, "sticky directory allowed removal of another user's file");
    exitstatus(0);
  }
  collect(pid, 0);

  pid = fork();
  require(pid >= 0, "could not fork the group check");
  if(pid == 0){
    assume(6002, GROUP);
    fd = open(DATA, O_RDWR);
    require(fd >= 0, "group could not open the shared file");
    close(fd);
    require(chmod(DATA, 0600) == -1 && chown(DATA, -1, GROUP) == -1,
            "group could change another user's metadata");
    require(umask(0077) == 0022 && umask(-1) == 0077, "umask returned the wrong value");
    fd = open(NEW, O_CREATE | O_WRONLY);
    require(fd >= 0 && fstatx(fd, &info) == 0 && info.uid == 6002 &&
            info.gid == GROUP && info.mode == 0600, "new file did not inherit group and umask");
    close(fd);
    require(umask(0777) == 0077, "umask did not return its previous value");
    fd = open(SECRET, O_CREATE | O_WRONLY | O_EXCL);
    require(fd >= 0 && fstatx(fd, &info) == 0 && info.mode == 0,
            "could not create a zero-mode file");
    close(fd);
    require(open(SECRET, O_RDONLY) == -1, "zero-mode file reopened unexpectedly");
    exitstatus(0);
  }
  collect(pid, 0);
  require(umask(-1) == 0022, "child umask changed the parent's mask");

  pid = fork();
  require(pid >= 0, "could not fork the other-user check");
  if(pid == 0){
    assume(6003, 7001);
    require(open(DIR, O_RDONLY) == -1 && chdir(DIR) == 0,
            "directory read and search permissions were not separate");
    require(statx(DATA, &info) == 0 && open(DATA, O_RDONLY) == -1,
            "other user could read group-only data");
    require(open(NEW, O_CREATE | O_WRONLY) == -1 && open("/root/.", O_RDONLY) == -1,
            "other user bypassed file or directory permissions");
    exitstatus(0);
  }
  collect(pid, 0);

  execution();
  require(unlink(STICKY) == 0 && unlink(DATA) == 0 && unlink(NEW) == 0 &&
          unlink(SECRET) == 0 && unlinkdir(DIR) == 0, "could not clean up test files");
  printf(1, "v02test: permission checks passed\n");
}

int
main(void)
{
  require(getuid() == 0 && umask(-1) == 0022, "run as root with umask 022");
  processes();
  permissions();
  printf(1, "V0.2 TESTS PASSED\n");
  exitstatus(0);
}
