/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"
#include "p9abi.h"

// a syscall stub puts the call number in eax and executes int t syscall
// the saved user stack starts with the return address then the arguments

// check the entire range before any caller uses a user pointer
static int
user_range(struct proc *p, uint addr, uint size, int write)
{
  uint page, last, limit;

  if(!size)
    return 1;
  limit = usermapend(p, addr);
  if(addr >= limit || size > limit - addr)
    return 0;
  last = PGROUNDDOWN(addr + size - 1);
  for(page = PGROUNDDOWN(addr); page <= last; page += PGSIZE)
    if(!userpage(p->pgdir, page, write))
      return 0;
  return 1;
}

int
fetchint(uint addr, int *ip)
{
  if(!user_range(myproc(), addr, sizeof(*ip), 0))
    return -1;
  memmove(ip, (const void*)(uptr)addr, sizeof(*ip));
  return 0;
}

int
fetchstrn(uint addr, char **pp, uint limit)
{
  uint i, size = usermapend(myproc(), addr);
  const char *s;

  if(!addr || addr >= size || addr >= KERNBASE)
    return -1;
  if(limit > size - addr)
    limit = size - addr;
  s = (const char*)(uptr)addr;
  for(i = 0; i < limit; i++){
    if((!i || ((addr+i) & (PGSIZE-1)) == 0) &&
       !user_range(myproc(), addr+i, 1, 0))
      return -1;
    if(s[i] == 0){
      *pp = (char*)s;
      return i;
    }
  }
  return -1;
}

int
fetchstr(uint addr, char **pp)
{
  return fetchstrn(addr, pp, PGSIZE);
}

int
fetchaddr(uint address, uint *result)
{
#ifdef P9_KERNEL64
  if(USER_NATIVE(myproc()->tf)){
    uptr value;
    if(!user_range(myproc(), address, sizeof(value), 0))
      return -1;
    memmove(&value, (const void*)(uptr)address, sizeof(value));
    if(value >= KERNBASE)
      return -1;
    *result = value;
    return 0;
  }
#endif
  return fetchint(address, (int*)result);
}

int
argint(int n, int *ip)
{
  struct trapframe *tf = myproc()->tf;
  uptr sp = tf->esp;

  if(n < 0 || n >= 6)
    return -1;
#ifdef P9_KERNEL64
  if(USER_NATIVE(tf)){
    const uptr regs[] = { tf->edi, tf->esi, tf->edx, tf->ecx, tf->r8, tf->r9 };
    uptr raw = regs[n];
    int value = raw;
    if(raw != (uint)value && raw != (uptr)(long)value)
      return -1;
    *ip = value;
    return 0;
  }
#endif
  if(sp >= KERNBASE - (1+n)*sizeof(uint))
    return -1;
  return fetchint(sp + (1+n)*sizeof(uint), ip);
}

static int
bufferarg(int n, char **pp, int size, int write)
{
  int addr;

  if(argint(n, &addr) < 0 || size < 0 || (size && !addr) ||
     !user_range(myproc(), addr, size, write))
    return -1;
  *pp = (char*)(uptr)(uint)addr;
  return 0;
}

int
argptr(int n, char **pp, int size)
{
  return bufferarg(n, pp, size, 0);
}

int
argptrwrite(int n, char **pp, int size)
{
  return bufferarg(n, pp, size, 1);
}

int
argstrn(int n, char **pp, uint limit)
{
  int addr;

  if(argint(n, &addr) < 0)
    return -1;
  return fetchstrn(addr, pp, limit);
}

int
argstr(int n, char **pp)
{
  return argstrn(n, pp, PGSIZE);
}

extern int sys_chdir(void);
extern int sys_close(void);
extern int sys_dup(void);
extern int sys_exec(void);
extern int sys_exit(void);
extern int sys_fork(void);
extern int sys_fstat(void);
extern int sys_getpid(void);
extern int sys_cpuquery(void);
extern int sys_usbquery(void);
extern int sys_kill(void);
extern int sys_link(void);
extern int sys_mkdir(void);
extern int sys_mknod(void);
extern int sys_open(void);
extern int sys_pipe(void);
extern int sys_read(void);
extern int sys_sbrk(void);
extern int sys_sleep(void);
extern int sys_unlink(void);
extern int sys_wait(void);
extern int sys_write(void);
extern int sys_uptime(void);
extern int sys_cps(void);
extern int sys_freemem(void);
extern int sys_setecho(void);
extern int sys_setuid(void);
extern int sys_getuid(void);

extern int sys_forkjob(void);
extern int sys_jobctl(void);
extern int sys_sigaction(void);
extern int sys_sigreturn(void);
extern int sys_lseek(void);
extern int sys_account(void);
extern int sys_accountinfo(void);
extern int sys_getcwd(void);
extern int sys_unlinkdir(void);

extern int sys_statx(void);
extern int sys_fstatx(void);
extern int sys_chmod(void);
extern int sys_chown(void);
extern int sys_getgid(void);
extern int sys_setgid(void);
extern int sys_umask(void);
extern int sys_accountgid(void);
extern int sys_getppid(void);
extern int sys_waitpid(void);
extern int sys_exitstatus(void);
extern int sys_send_signal(void);

extern int sys_execve(void);
extern int sys_dup2(void);
extern int sys_jobctlx(void);
extern int sys_ttyctl(void);
extern int sys_ioctl(void);
extern int sys_getprocs(void);
extern int sys_memlimit(void);

extern int sys_fcntl(void);
extern int sys_select(void);
extern int sys_mmap(void);
extern int sys_munmap(void);
extern int sys_gettimeofday(void);
extern int sys_fchmod(void);
extern int sys_access(void);
extern int sys_setsid(void);
extern int sys_sigmask(void);
extern int sys_sigactionx(void);

extern int sys_rename(void);
extern int sys_symlink(void);
extern int sys_readlink(void);
extern int sys_getdent(void);
extern int sys_reboot(void);
extern int sys_diskctl(void);
extern int sys_installmount(void);
extern int sys_sync(void);

extern int sys_socket(void);
extern int sys_bind(void);
extern int sys_listen(void);
extern int sys_connect(void);
extern int sys_accept(void);
extern int sys_openpty(void);
extern int sys_poll(void);
extern int sys_shutdown(void);
extern int sys_filemtime(void);

static int (*syscalls[])(void) = {
[SYS_socket] sys_socket,
[SYS_bind] sys_bind,
[SYS_listen] sys_listen,
[SYS_connect] sys_connect,
[SYS_accept] sys_accept,
[SYS_openpty] sys_openpty,
[SYS_poll] sys_poll,
[SYS_shutdown] sys_shutdown,
[SYS_filemtime] sys_filemtime,
[SYS_rename] sys_rename,
[SYS_reboot] sys_reboot,
[SYS_diskctl] sys_diskctl,
[SYS_installmount] sys_installmount,
[SYS_sync] sys_sync,
[SYS_symlink] sys_symlink,
[SYS_getdent]   sys_getdent,
[SYS_readlink] sys_readlink,
[SYS_fcntl] sys_fcntl,
[SYS_select] sys_select,
[SYS_mmap] sys_mmap,
[SYS_munmap] sys_munmap,
[SYS_gettimeofday] sys_gettimeofday,
[SYS_fchmod] sys_fchmod,
[SYS_access] sys_access,
[SYS_setsid] sys_setsid,
[SYS_sigmask] sys_sigmask,
[SYS_sigactionx] sys_sigactionx,
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_cpuquery] sys_cpuquery,
[SYS_usbquery] sys_usbquery,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_cps]     sys_cps,
[SYS_freemem] sys_freemem,
[SYS_setecho] sys_setecho,
[SYS_setuid]  sys_setuid,
[SYS_getuid]  sys_getuid,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_forkjob] sys_forkjob,
[SYS_jobctl] sys_jobctl,
[SYS_sigaction] sys_sigaction,
[SYS_sigreturn] sys_sigreturn,
[SYS_lseek] sys_lseek,
[SYS_account] sys_account,
[SYS_accountinfo] sys_accountinfo,
[SYS_statx] sys_statx,
[SYS_fstatx] sys_fstatx,
[SYS_chmod] sys_chmod,
[SYS_chown] sys_chown,
[SYS_getgid] sys_getgid,
[SYS_setgid] sys_setgid,
[SYS_umask] sys_umask,
[SYS_accountgid] sys_accountgid,
[SYS_getppid] sys_getppid,
[SYS_waitpid] sys_waitpid,
[SYS_exitstatus] sys_exitstatus,
[SYS_send_signal] sys_send_signal,
[SYS_execve] sys_execve,
[SYS_dup2] sys_dup2,
[SYS_jobctlx] sys_jobctlx,
[SYS_ttyctl] sys_ttyctl,
[SYS_ioctl] sys_ioctl,
[SYS_getprocs] sys_getprocs,
[SYS_memlimit] sys_memlimit,
[SYS_getcwd] sys_getcwd,
[SYS_unlinkdir] sys_unlinkdir,
};

void
syscall(void)
{
  int num;
  struct proc *curproc = myproc();

  num = curproc->tf->eax;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    int result = syscalls[num]();
    if(num != SYS_sigreturn || result < 0)
      curproc->tf->eax = (long)result;
  } else {
    curproc->tf->eax = -P9_ENOSYS;
  }
}
