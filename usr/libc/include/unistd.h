/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_LIBC_UNISTD_H
#define P9_LIBC_UNISTD_H
#include <sys/types.h>
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
int close(int) __asm__("p9_close");
ssize_t read(int, void *, size_t) __asm__("p9_read");
ssize_t write(int, const void *, size_t) __asm__("p9_write");
off_t lseek(int, off_t, int) __asm__("p9_lseek");
int unlink(const char *) __asm__("p9_unlink");
int link(const char *, const char *) __asm__("p9_link");
int symlink(const char *, const char *) __asm__("p9_symlink");
ssize_t readlink(const char *, char *, size_t) __asm__("p9_readlink");
int rmdir(const char *) __asm__("p9_rmdir");
int chdir(const char *) __asm__("p9_chdir");
char *getcwd(char *, size_t) __asm__("p9_getcwd");
int access(const char *, int) __asm__("p9_access");
void _exit(int) __asm__("exitstatus") __attribute__((noreturn));
pid_t fork(void) __asm__("p9_fork");
int execve(const char *, char *const [], char *const []) __asm__("p9_execve");
int execvp(const char *, char *const []);
pid_t getpid(void), getppid(void);
pid_t setsid(void) __asm__("p9_setsid");
uid_t getuid(void), geteuid(void) __asm__("getuid");
gid_t getgid(void), getegid(void) __asm__("getgid");
int setuid(uid_t) __asm__("p9_setuid");
int setgid(gid_t) __asm__("p9_setgid");
void *sbrk(ptrdiff_t) __asm__("p9_sbrk");
int usleep(unsigned);
#define _SC_OPEN_MAX 1
#define _SC_PAGESIZE 2
long sysconf(int);
int getpagesize(void);
int gethostname(char *, size_t);
unsigned sleep(unsigned) __asm__("p9_sleep");
int pipe(int *) __asm__("p9_pipe");
int dup(int) __asm__("p9_dup");
int dup2(int, int) __asm__("p9_dup2");
int isatty(int) __asm__("p9_isatty");
char *ttyname(int);
extern char **environ;
extern char *optarg;
extern int optind, opterr, optopt;
int getopt(int, char *const [], const char *);
#endif
