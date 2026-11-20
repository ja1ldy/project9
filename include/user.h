/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

struct stat;
struct p9_direntx;
int getdent(int, struct p9_direntx *);
int rename(const char *, const char *);
struct rtcdate;

// system calls
int fork(void);
int exit(void) __attribute__((noreturn));
int wait(void);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int ttyctl(int, int);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
int cpuquery(int);
int usbquery(int);
char* sbrk(int);
int sleep(int);
int uptime(void);
int cps(void);
int freemem(void);
int setecho(int);
int setuid(int);
int getuid(void);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, int c);
int strcmp(const char*, const char*);
void printf(int, const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
int forkjob(int);
int jobctl(int, int);
int sigaction(int, void (*)(int), void (*)(void));
int sigreturn(void);
int lseek(int, int, int);
int account(int, const char *, const char *, const char *);
int getcwd(char *, int);
int unlinkdir(const char *);
void (*signal(int, void (*)(int)))(int);
int parseuint(const char*, uint*);
struct statx;
int statx(const char*, struct statx*);
int fstatx(int, struct statx*);
int chmod(const char*, int);
int chown(const char*, int, int);
int getgid(void);
int setgid(int);
int umask(int);
int accountgid(int);
int getppid(void);
int waitpid(int, int*, int);
void exitstatus(int) __attribute__((noreturn));
int send_signal(int, int);
int parsemode(const char*, uint*);
void formatmode(uint, char[5]);

extern char **environ;
char *getenv(const char *);
int execve(const char *, char *const *, char *const *);
int dup2(int, int);
int jobctlx(int, int, int *);
int execpath(char **, char **, const char *);
int parseint(const char *, int *);

int memlimit(void);

int reboot(int);
