/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

// check syscall arguments before passing them to the file and inode code

#include "types.h"
#include "local.h"
#include "memlayout.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mode.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "tty.h"
#include "ioctl.h"
#include "p9abi.h"
#include "p9records.h"
#include "ext2.h"
#include "vfs.h"

static int
isext2(struct inode *ip)
{
#ifdef P9_LEGACY_ROOT
  return ip->dev == EXT2DEV;
#else
  (void)ip;
  return 1;
#endif
}

static int
maywrite(struct inode *ip)
{
  return ip->type && (ip->type != T_DIR || ip->nlink) && (!isext2(ip) || vfs_ops(ip->dev)->writable(ip));
}

// look up the file descriptor passed as the nth syscall argument
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// install the file in a free descriptor slot and take over its reference
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->closeexec &= ~(1U << fd);
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

static int
isconsole(struct file *f)
{
  int yes;

  if(f->type != FD_INODE)
    return 0;
  ilock(f->ip);
  yes = f->ip->type == T_DEV && f->ip->major == CONSOLE;
  iunlock(f->ip);
  return yes;
}

int
sys_ttyctl(void)
{
  struct file *f;
  int request;

  if(argfd(0, 0, &f) < 0 || argint(1, &request) < 0)
    return -1;
  if(f->type == FD_PTY)
    return ptymode(f, request);
  if(!isconsole(f))
    return -1;
  if(request == TTY_ISATTY)
    return 0;
  if(!f->readable)
    return -1;
  return consolemode(request);
}

int
sys_ioctl(void)
{
  struct file *f;
  char *data;
  int request, size, framebuffer;

  if(argfd(0, 0, &f) < 0 || argint(1, &request) < 0)
    return -1;
  if(request == FIONREAD){
    if(f->type != FD_PIPE && f->type != FD_LOCAL)
      return -P9_ENOTTY;
    if(argptrwrite(2, &data, sizeof(int)) < 0)
      return -P9_EFAULT;
    int count = f->type == FD_PIPE ? pipequeued(f->pipe) : localqueued(f->local);
    if(count < 0)
      return count;
    *(int*)data = count;
    return 0;
  }
  if(request == KBDIOGETMODE){
    if(f->type != FD_INODE || argptrwrite(2, &data, sizeof(int)) < 0)
      return -1;
    ilock(f->ip);
    framebuffer = f->ip->type == T_DEV && f->ip->major == DEV_KEYBOARD;
    iunlock(f->ip);
    if(!framebuffer)
      return -1;
    *(int*)data = KBD_MODE_SET1;
    return 0;
  }
  if(request == FBIOGETINFO){
    if(f->type != FD_INODE || !f->readable ||
       argptrwrite(2, &data, sizeof(struct fbinfo)) < 0)
      return -1;
    ilock(f->ip);
    framebuffer = f->ip->type == T_DEV && f->ip->major == DEV_FB && f->ip->minor == 0;
    iunlock(f->ip);
    return framebuffer ? fbgetinfo((struct fbinfo*)data) : -1;
  }
  if(f->type == FD_PTY && request == TIOCSCTTY)
    return ptyioctl(f, request, 0);
  if(f->type == FD_PTY && request == TIOCGPTY){
    if(argptrwrite(2, &data, sizeof(int)) < 0)
      return -P9_EFAULT;
    return ptyioctl(f, request, data);
  }
  if(f->type != FD_PTY && !isconsole(f))
    return -1;
  switch(request){
  case TIOCGETA: case TIOCSETA: case TIOCSETAW: case TIOCSETAF:
    size = sizeof(struct termios);
    break;
  case TIOCGWINSZ: case TIOCSWINSZ:
    size = sizeof(struct winsize);
    break;
  default:
    return -1;
  }
  if((request == TIOCGETA || request == TIOCGWINSZ ?
      argptrwrite(2, &data, size) : argptr(2, &data, size)) < 0)
    return -1;
  return f->type == FD_PTY ? ptyioctl(f, request, data) : consoleioctl(request, data);
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptrwrite(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptrwrite(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}

// give the inode another name in the filesystem
int
sys_link(void)
{
  char name[P9_NAME_BUFSIZE], *new, *old;
  struct inode *dp, *ip;

  if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
    return -1;

  begin_op();
  namelock();
  if((ip = nameinofollow(old)) == 0)
    goto fail;

  ilock(ip);
  if(!maywrite(ip) || ip->nlink == 0x7fff || ip->type == T_DIR || (ip->flags & I_SHADOW) ||
     (myproc()->uid != 0 && myproc()->uid != ip->uid)){
    iunlockput(ip);
    goto fail;
  }

  ip->nlink++;
  if(iupdate(ip) < 0){
    ip->nlink--;
    iunlockput(ip);
    goto fail;
  }
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(!inodeaccess(dp, M_WRITE | M_EXEC) || !maywrite(dp) ||
     dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  nameunlock();
  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
fail:
  nameunlock();
  end_op();
  return -1;
}

// ignore the dot entries when checking whether a directory is empty
static int
isdirempty(struct inode *dp)
{
  uint off = 0;
  struct p9_direntx de;
  int result;

  while((result = nextdent(dp, &off, &de)) > 0)
    if(namecmp(de.name, ".") && namecmp(de.name, ".."))
      return 0;
  return result == 0;
}

//PAGEBREAK!
static int
remove_locked(char *path, int dironly)
{
  struct inode *ip, *dp;
  char name[P9_NAME_BUFSIZE];
  uint off;

  if((dp = nameiparent(path, name)) == 0)
    return -1;

  ilock(dp);

  if(!inodeaccess(dp, M_WRITE | M_EXEC) || !maywrite(dp))
    goto bad;

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  if(ip == dp){
    iput(ip);
    goto bad;
  }
  ilock(ip);

  // in a sticky directory only root the directory owner or the file owner may remove a name
  if(ip->dev != dp->dev || !maywrite(ip) || !ip->nlink ||
     (ip->flags & I_SHADOW) || (dironly && ip->type != T_DIR) ||
     (isext2(ip) && ip->type == T_DIR && (!dironly || ip->nlink != 2)) ||
     ((dp->mode & M_STICKY) && myproc()->uid != 0 &&
      myproc()->uid != dp->uid && myproc()->uid != ip->uid)){
    iunlockput(ip);
    goto bad;
  }
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  if(dirreplace(dp, off, 0, 0) < 0){
    iunlockput(ip);
    goto bad;
  }
  int result = 0;
  if(ip->type == T_DIR){
    dp->nlink--;
    result = iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink -= isext2(ip) && ip->type == T_DIR ? 2 : 1;
  if(iupdate(ip) < 0)
    result = -1;
  iunlockput(ip);
  return result;

bad:
  iunlockput(dp);
  return -1;
}

int
remove_path(char *path, int dironly)
{
  int result;

  namelock();
  result = remove_locked(path, dironly);
  nameunlock();
  return result;
}

static int
unlinkarg(int dironly)
{
  char *path;
  int result;

  if(argstr(0, &path) < 0)
    return -1;
  begin_op();
  result = remove_path(path, dironly);
  end_op();
  return result;
}

int
sys_unlink(void)
{
  return unlinkarg(0);
}

int
sys_unlinkdir(void)
{
  return unlinkarg(1);
}

static struct inode*
create_locked(char *path, short type, uint owner, uint group, uint mode,
              int exclusive, int *created, char *target)
{
  struct inode *ip, *dp;
  char name[P9_NAME_BUFSIZE];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(!exclusive && type == T_FILE && ip->type == T_FILE){
      if(!(ip->flags & I_SYMLINK))
        return ip;
      iunlockput(ip);
      ip = namei(path);
      if(!ip)
        return 0;
      ilock(ip);
      if(ip->type == T_FILE)
        return ip;
    }
    iunlockput(ip);
    return 0;
  }

  if(!inodeaccess(dp, M_WRITE | M_EXEC) || !maywrite(dp) ||
     (type == T_DIR && dp->nlink == 0x7fff) ||
     (type == T_SOCKET && !isext2(dp)) ||
     (ip = ialloc(dp->dev, type)) == 0){
    iunlockput(dp);
    return 0;
  }

  ilock(ip);
  ip->mode = mode;
  ip->uid = owner;
  ip->gid = group;
  if(type == T_DEV){
    ip->major = owner;
    ip->minor = group;
    ip->uid = myproc()->uid;
    ip->gid = myproc()->gid;
  } else if(group > ID_MAX){
    ip->gid = (dp->mode & M_SETGID) ? dp->gid : myproc()->gid;
    if(type == T_DIR && (dp->mode & M_SETGID))
      ip->mode |= M_SETGID;
  }
  if(target){
    uint len = strlen(target);
    ip->flags |= I_SYMLINK;
    if(writei(ip, target, 0, len) != len)
      goto bad;
  }
  if(type == T_DIR &&
     (dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0))
    goto bad;
  if(dirlink(dp, name, ip->inum) < 0)
    goto bad;
  if(type == T_DIR){
    dp->nlink++;
    if(iupdate(dp) < 0)
      goto bad;
  }
  ip->nlink = isext2(ip) && type == T_DIR ? 2 : 1;
  if(iupdate(ip) < 0)
    goto bad;
  if(created)
    *created = 1;
  iunlockput(dp);
  return ip;

bad:
  iunlockput(ip);
  iunlockput(dp);
  return 0;
}

struct inode*
create(char *path, short type, uint owner, uint group, uint mode, int exclusive, int *created)
{
  struct inode *ip;

  namelock();
  ip = create_locked(path, type, owner, group, mode, exclusive, created, 0);
  nameunlock();
  return ip;
}

int
sys_open(void)
{
  char *path;
  int fd, omode, created = 0;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0 ||
     (omode & ~(3 | O_CREATE | O_TRUNC | O_APPEND | O_EXCL | O_NONBLOCK | O_CLOEXEC)) ||
     (omode & 3) == 3 || ((omode & (O_TRUNC | O_APPEND)) && !(omode & 3)) ||
     ((omode & O_EXCL) && !(omode & O_CREATE)))
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, myproc()->uid, ID_MAX + 1U,
                0666 & ~myproc()->umask, omode & O_EXCL, &created);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && (omode & ~(O_NONBLOCK | O_CLOEXEC)) != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if(!ip->type || ip->type == T_SOCKET){
    iunlockput(ip);
    end_op();
    return -1;
  }
  if(ip->type == T_DEV && ip->major >= 0 && ip->major < NDEV &&
     devsw[ip->major].available && !devsw[ip->major].available(ip->minor)){
    iunlockput(ip);
    end_op();
    return -1;
  }
  int need = (omode & O_WRONLY) ? M_WRITE :
             (omode & O_RDWR) ? M_READ | M_WRITE : M_READ;
  if((!created && !inodeaccess(ip, need)) ||
     ((need & M_WRITE) && !maywrite(ip))){
    iunlockput(ip);
    end_op();
    return -1;
  }
  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  if(omode & O_CLOEXEC)
    myproc()->closeexec |= 1U << fd;
  if(ip->type == T_DEV && (ip->major == DEV_PTY_MASTER || ip->major == DEV_PTY_SLAVE)){
    int result = ptyopen(f, ip->minor, ip->major == DEV_PTY_MASTER,
                         !!(omode & O_NONBLOCK), !(omode & 3), (omode & 3) == O_WRONLY);
    iunlockput(ip);
    end_op();
    if(result < 0){
      myproc()->ofile[fd] = 0;
      fileclose(f);
      return result;
    }
    return fd;
  }
  if((omode & O_TRUNC) && ip->type == T_FILE){
    int truncated;
#ifdef P9_LEGACY_ROOT
    if(!isext2(ip)){
      itrunc(ip);
      truncated = 0;
    } else
#endif
      truncated = vfs_ops(ip->dev)->truncate(ip);
    if(truncated < 0){
      myproc()->ofile[fd] = 0;
      fileclose(f);
      iunlockput(ip);
      end_op();
      return -1;
    }
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->nonblock = !!(omode & O_NONBLOCK);
  f->append = (omode & O_APPEND) != 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, myproc()->uid, ID_MAX + 1U,
                                        0777 & ~myproc()->umask, 1, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  if(myproc()->uid != 0)
    return -1;
  struct inode *ip;
  char *path;
  int major, minor, mode;

  if(argstr(0, &path) < 0 || argint(1, &major) < 0 || argint(2, &minor) < 0 ||
     myproc()->uid != 0 || major < 0 || major >= NDEV || minor < 0 || minor > 32767)
    return -1;
  mode = major == CONSOLE || major == DEV_MEMORY ||
         major == DEV_PTY_MASTER || major == DEV_PTY_SLAVE ? 0666 :
         major == DEV_RTC ? 0444 : 0600;
  begin_op();
  if((ip = create(path, T_DEV, major, minor, mode, 1, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR || !inodeaccess(ip, M_EXEC)){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

static int
fetchvector(uint addr, char **items, int capacity, uint *space)
{
  int i, length;
  uint item, cost, width = USER_NATIVE(myproc()->tf) ? 8 : sizeof(uint);

  if(!addr){
    items[0] = 0;
    return 0;
  }
  if(addr >= KERNBASE)
    return -1;
  for(i = 0; i < capacity; i++){
    if(fetchaddr(addr + width*i, &item) < 0)
      return -1;
    if(!item){
      items[i] = 0;
      return 0;
    }
    if(*space < sizeof(uint) ||
       (length = fetchstrn(item, &items[i], *space-sizeof(uint))) < 0)
      return -1;
    cost = ((length + 4) & ~3U) + sizeof(uint);
    if(cost > *space)
      return -1;
    *space -= cost;
  }
  return -1;
}

static int
execargs(int environment)
{
  char *path, *argv[MAXARG+1], *envp[MAXENV+1];
  uint args, vars = 0, space = PGSIZE - 6*sizeof(uint);

  if(argstrn(0, &path, P9_PATH_MAX) < 0 || argint(1, (int*)&args) < 0 || !args ||
     (environment && argint(2, (int*)&vars) < 0) ||
     fetchvector(args, argv, NELEM(argv), &space) < 0 ||
     fetchvector(vars, envp, NELEM(envp), &space) < 0)
    return -1;
  return execve(path, argv, envp);
}

int
sys_exec(void)
{
  return execargs(0);
}

int
sys_execve(void)
{
  return execargs(1);
}

int
sys_dup2(void)
{
  struct file *f, *previous;
  struct proc *p = myproc();
  int old, new;

  if(argfd(0, &old, &f) < 0 || argint(1, &new) < 0 || new < 0 || new >= NOFILE)
    return -1;
  if(old != new){
    previous = p->ofile[new];
    p->ofile[new] = filedup(f);
    p->closeexec &= ~(1U << new);
    if(previous)
      fileclose(previous);
  }
  return new;
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptrwrite(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}

int
sys_lseek(void)
{
  struct file *f;
  int displacement, whence;
  long long next;

  if(argfd(0, 0, &f) < 0 || argint(1, &displacement) < 0 ||
     argint(2, &whence) < 0 || f->type != FD_INODE)
    return -1;
  ilock(f->ip);
  next = displacement;
  if(whence == SEEK_CUR)
    next += f->off;
  else if(whence == SEEK_END)
    next += f->ip->size;
  if((f->ip->type != T_FILE && f->ip->type != T_DIR) || whence < SEEK_SET || whence > SEEK_END ||
     next < 0 || next > 0x7fffffff ||
     (next > f->ip->size && (!isext2(f->ip) || f->ip->type != T_FILE))){
    iunlock(f->ip);
    return -1;
  }
  f->off = next;
  iunlock(f->ip);
  return next;
}

// walk up to the root and prepend each directory name along the way
int
sys_getcwd(void)
{
  char *dst;
  int n, len, found, result = -1;
  uint off, child;
  struct inode *ip, *parent;
  struct p9_direntx de;

  if(argint(1, &n) < 0 || n < 2 || argptrwrite(0, &dst, n) < 0)
    return -1;
  begin_op();
  ip = idup(myproc()->cwd);
  dst[--n] = 0;
  while(!isroot(ip)){
    child = ip->inum;
    if(ip->inum == EXT2ROOT && ext2host(ip->dev))
      child = ext2host(ip->dev);
    ilock(ip);
    parent = ip->type == T_DIR ? dirlookup(ip, "..", 0) : 0;
    iunlockput(ip);
    ip = parent;
    if(!ip)
      goto end;
    ilock(ip);
    found = 0;
    off = 0;
    while(ip->type == T_DIR && nextdent(ip, &off, &de) > 0){
      if(de.inum == child && namecmp(de.name, ".") && namecmp(de.name, "..")){
        found = 1;
        break;
      }
    }
    iunlock(ip);
    if(!found)
      goto put;
    len = strlen(de.name);
    if(n < len + 1)
      goto put;
    n -= len;
    memmove(dst + n, de.name, len);
    dst[--n] = '/';
  }
  if(!dst[n])
    dst[--n] = '/';
  result = strlen(dst + n);
  memmove(dst, dst + n, result + 1);
put:
  iput(ip);
end:
  end_op();
  return result;
}

int
sys_getdent(void)
{
  struct file *f;
  struct p9_direntx entry;
  char *dst;
  int result = -1;

  if(argfd(0, 0, &f) < 0 || f->type != FD_INODE || !f->readable ||
     argptrwrite(1, &dst, sizeof(entry)) < 0)
    return -1;
  ilock(f->ip);
  if(f->ip->type == T_DIR)
    result = nextdent(f->ip, &f->off, &entry);
  iunlock(f->ip);
  if(result > 0)
    memmove(dst, &entry, sizeof(entry));
  return result;
}

int
sys_statx(void)
{
  char *path, *dst;
  struct inode *ip;
  int result = -1;

  if(argstr(0, &path) < 0 || argptrwrite(1, &dst, sizeof(struct statx)) < 0)
    return -1;
  begin_op();
  if((ip = namei(path)) != 0){
    ilock(ip);
    if(ip->type && !(ip->flags & I_SHADOW)){
      statxi(ip, (struct statx*)dst);
      result = 0;
    }
    iunlockput(ip);
  }
  end_op();
  return result;
}

int
sys_fstatx(void)
{
  struct file *f;
  char *dst;

  if(argfd(0, 0, &f) < 0 || argptrwrite(1, &dst, sizeof(struct statx)) < 0)
    return -1;
  if(f->type == FD_PTY){
    struct statx *info = (struct statx*)dst;
    memset(info, 0, sizeof(*info));
    info->uid = ptyowner(f->pty);
    info->mode = 0600;
    return filestat(f, &info->st);
  }
  if(f->type != FD_INODE)
    return -1;
  ilock(f->ip);
  statxi(f->ip, (struct statx*)dst);
  iunlock(f->ip);
  return 0;
}

// both changes use the same path and inode lock as open
static int
setmetadata(char *path, int mode, int uid, int gid)
{
  struct proc *p = myproc();
  struct inode *ip;
  int result = -1;

  begin_op();
  if((ip = namei(path)) != 0){
    ilock(ip);
    if((ip->flags & I_SHADOW) || !maywrite(ip))
      goto put;
    uint oldmode = ip->mode, olduid = ip->uid, oldgid = ip->gid;
    if(mode >= 0){
      if(p->uid != 0 && p->uid != ip->uid)
        goto put;
      if((mode & M_SETGID) &&
         (ip->type != T_DIR || (p->uid != 0 && p->gid != ip->gid)))
        goto put;
      if((mode & M_STICKY) && ip->type != T_DIR)
        goto put;
      ip->mode = mode;
    } else {
      if(p->uid != 0 &&
         (p->uid != ip->uid || uid != -1 || (gid != -1 && gid != p->gid)))
        goto put;
      if(uid != -1)
        ip->uid = uid;
      if(gid != -1)
        ip->gid = gid;
      if(p->uid != 0 && gid != -1)
        ip->mode &= ~M_SETGID;
    }
    result = iupdate(ip);
    if(result < 0){
      ip->mode = oldmode;
      ip->uid = olduid;
      ip->gid = oldgid;
    }
put:
    iunlockput(ip);
  }
  end_op();
  return result;
}

int
sys_chmod(void)
{
  char *path;
  int mode;

  if(argstr(0, &path) < 0 || argint(1, &mode) < 0 || (mode & ~M_VALID))
    return -1;
  return setmetadata(path, mode, -1, -1);
}

int
sys_chown(void)
{
  char *path;
  int uid, gid;

  if(argstr(0, &path) < 0 || argint(1, &uid) < 0 || argint(2, &gid) < 0 ||
     uid < -1 || uid > ID_MAX || gid < -1 || gid > ID_MAX)
    return -1;
  return setmetadata(path, -1, uid, gid);
}

int
sys_fcntl(void)
{
  struct file *f;
  int request, flags, fd;

  if(argfd(0, &fd, &f) < 0)
    return -P9_EBADF;
  if(argint(1, &request) < 0)
    return -P9_EINVAL;
  if(request == F_GETFD)
    return myproc()->closeexec & (1U << fd) ? FD_CLOEXEC : 0;
  if(request == F_SETFD){
    if(argint(2, &flags) < 0 || (flags & ~FD_CLOEXEC))
      return -P9_EINVAL;
    myproc()->closeexec &= ~(1U << fd);
    if(flags & FD_CLOEXEC)
      myproc()->closeexec |= 1U << fd;
    return 0;
  }
  if(request == F_GETFL)
    return (f->writable ? (f->readable ? O_RDWR : O_WRONLY) : O_RDONLY) |
           (f->append ? O_APPEND : 0) | (f->nonblock ? O_NONBLOCK : 0);
  if(request != F_SETFL || argint(2, &flags) < 0 ||
     (flags & ~(O_APPEND | O_NONBLOCK | 3)))
    return -P9_EINVAL;
  f->nonblock = !!(flags & O_NONBLOCK);
  f->append = !!(flags & O_APPEND);
  return 0;
}

int
sys_mmap(void)
{
  struct proc *p = myproc();
  struct file *f;
  struct fbinfo info;
  int addr, len, prot, flags, off, fd, writable, allowed, result;
  uint bytes;

  if(argint(0, &addr) < 0 || argint(1, &len) < 0 || argint(2, &prot) < 0 ||
     argint(3, &flags) < 0 || argint(4, &fd) < 0 || argint(5, &off) < 0 ||
     addr || off < 0 || len <= 0 ||
     !(prot & PROT_READ) || (prot & ~(PROT_READ | PROT_WRITE)))
    return -P9_EINVAL;
  writable = !!(prot & PROT_WRITE);
  if(flags == (MAP_PRIVATE | MAP_ANONYMOUS)){
    if(fd != -1 || off || (uint)len > FILEMAP_LIMIT)
      return -P9_EINVAL;
    return private_map(p, 0, 0, len, 0, writable);
  }
  if(argfd(4, 0, &f) < 0)
    return -P9_EBADF;
  if(flags == MAP_PRIVATE){
    if((uint)len > FILEMAP_LIMIT || off % PGSIZE)
      return -P9_EINVAL;
    if(f->type != FD_INODE)
      return -P9_ENODEV;
    if(!f->readable)
      return -P9_EACCES;
    ilock(f->ip);
    result = -P9_EINVAL;
    if(f->ip->type == T_FILE && (uint)off < f->ip->size &&
       PGROUNDUP(len) <= PGROUNDUP(f->ip->size - (uint)off)){
      bytes = f->ip->size - (uint)off;
      if(bytes > (uint)len)
        bytes = len;
      result = private_map(p, f->ip, off, len, bytes, writable);
    }
    iunlock(f->ip);
    return result;
  }
  if(flags != MAP_SHARED || off)
    return -P9_EINVAL;
  if(f->type != FD_INODE || fbgetinfo(&info) < 0)
    return -P9_ENODEV;
  if(p->fbmaplen)
    return -P9_EBUSY;
  ilock(f->ip);
  allowed = f->ip->type == T_DEV && f->ip->major == DEV_FB && f->ip->minor == 0;
  iunlock(f->ip);
  if(!allowed)
    return -P9_ENODEV;
  if(!f->readable || (writable && !f->writable))
    return -P9_EACCES;
  if((uint)len > PGROUNDUP(info.stride * info.height))
    return -P9_EINVAL;
  if(fbmapuser(p->pgdir, len, writable) < 0){
    deallocuvm(p->pgdir, FB_USER + len, FB_USER);
    switchuvm(p);
    return -P9_ENOMEM;
  }
  p->fbmaplen = len;
  p->fbmapwrite = writable;
  if(writable)
    fbgraphics(1);
  switchuvm(p);
  return FB_USER;
}

int
sys_munmap(void)
{
  int addr, len;
  struct proc *p = myproc();

  if(argint(0, &addr) < 0 || argint(1, &len) < 0 || len <= 0)
    return -P9_EINVAL;
  if(addr != FB_USER)
    return private_unmap(p, addr, len);
  if((uint)len != p->fbmaplen)
    return -P9_EINVAL;
  fbunmapuser(p);
  return 0;
}

int
sys_access(void)
{
  char *path;
  struct inode *ip;
  int mode, result;

  if(argstr(0, &path) < 0 || argint(1, &mode) < 0 || (mode & ~7))
    return -P9_EINVAL;
  begin_op();
  ip = namei(path);
  result = -P9_ENOENT;
  if(ip){
    ilock(ip);
    result = inodeaccess(ip, mode) ? 0 : -P9_EACCES;
    iunlockput(ip);
  }
  end_op();
  return result;
}

int
sys_fchmod(void)
{
  struct file *f;
  struct inode *ip;
  struct proc *p = myproc();
  int mode, result = -P9_EPERM;

  if(argfd(0, 0, &f) < 0 || f->type != FD_INODE)
    return -P9_EBADF;
  if(argint(1, &mode) < 0 || (mode & ~M_VALID))
    return -P9_EINVAL;
  ip = f->ip;
  begin_op();
  ilock(ip);
  if(!(ip->flags & I_SHADOW) && maywrite(ip) && (!p->uid || ip->uid == p->uid) &&
     (!(mode & (M_SETGID | M_STICKY)) || ip->type == T_DIR) &&
     (!(mode & M_SETGID) || !p->uid || p->gid == ip->gid)){
    uint previous = ip->mode;
    ip->mode = mode;
    result = iupdate(ip) < 0 ? -P9_EIO : 0;
    if(result < 0)
      ip->mode = previous;
  }
  iunlock(ip);
  end_op();
  return result;
}

int
sys_symlink(void)
{
  char *target, *path;
  struct inode *ip;

  if(argstrn(0, &target, P9_PATH_MAX) <= 0 || argstr(1, &path) < 0)
    return -P9_EINVAL;
  begin_op();
  namelock();
  ip = create_locked(path, T_FILE, myproc()->uid, ID_MAX+1U, 0777, 1, 0, target);
  if(ip)
    iunlockput(ip);
  nameunlock();
  end_op();
  return ip ? 0 : -P9_EIO;
}

int
sys_readlink(void)
{
  char *path, *buf;
  int len, result = -P9_ENOENT;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(2, &len) < 0 || len <= 0 ||
     argptrwrite(1, &buf, len) < 0)
    return -P9_EINVAL;
  begin_op();
  if((ip = nameinofollow(path)) != 0){
    ilock(ip);
    result = ip->flags & I_SYMLINK ? readi(ip, buf, 0, len) : -P9_EINVAL;
    iunlockput(ip);
  }
  end_op();
  return result;
}

static int
maymove(struct inode *dp, struct inode *ip)
{
  int uid = myproc()->uid;

  return maywrite(ip) && !(ip->flags & I_SHADOW) &&
         (!(dp->mode & M_STICKY) || !uid || uid == dp->uid || uid == ip->uid);
}

// the namespace lock keeps the parent chain stable during the walk
static int
below(struct inode *dir, struct inode *ancestor)
{
  struct inode *ip = idup(dir), *parent;
  int n, answer = 1;

  for(n = 0; n < NINODE && ip; n++){
    if(ip == ancestor)
      break;
    if(isroot(ip)){
      answer = 0;
      break;
    }
    ilock(ip);
    parent = ip->type == T_DIR ? dirlookup(ip, "..", 0) : 0;
    iunlockput(ip);
    ip = parent;
  }
  if(ip)
    iput(ip);
  return answer;
}

static int
rename_locked(char *from, char *to)
{
  struct inode *a, *b, *src = 0, *dst = 0, *first, *second;
  uint oldoff, newoff = 0, parentoff = 0;
  char old[P9_NAME_BUFSIZE], new[P9_NAME_BUFSIZE];
  int result = -P9_ENOENT, srcdir, dstlocked = 0;

  a = nameiparent(from, old);
  b = nameiparent(to, new);
  if(!a || !b)
    goto put;
  result = -P9_EINVAL;
  if(a->dev != b->dev || !namecmp(old, ".") || !namecmp(old, "..") ||
     !namecmp(new, ".") || !namecmp(new, ".."))
    goto put;

  ilock(a);
  src = dirlookup(a, old, &oldoff);
  iunlock(a);
  if(!src){
    result = -P9_ENOENT;
    goto put;
  }
  ilock(src);
  srcdir = src->type == T_DIR;
  iunlock(src);
  if(src->dev != a->dev || src == a || src == b || (srcdir && a != b && below(b, src)))
    goto put;

  first = a->inum <= b->inum ? a : b;
  second = a == b ? 0 : first == a ? b : a;
  ilock(first);
  if(second)
    ilock(second);
  ilock(src);
  result = -P9_EACCES;
  if(!inodeaccess(a, M_WRITE | M_EXEC) || !inodeaccess(b, M_WRITE | M_EXEC) ||
     !maywrite(a) || !maywrite(b) || !maymove(a, src))
    goto unlock;
  if(srcdir && a != b){
    struct inode *parent = dirlookup(src, "..", &parentoff);
    if(parent != a || b->nlink == 0x7fff){
      if(parent)
        iput(parent);
      goto unlock;
    }
    iput(parent);
  }
  dst = dirlookup(b, new, &newoff);
  if(dst == src){
    result = 0;
    goto unlock;
  }
  if(dst){
    result = -P9_EINVAL;
    if(dst->dev != b->dev || dst == a || dst == b)
      goto unlock;
    ilock(dst);
    dstlocked = 1;
    if(!maymove(b, dst) || (dst->type == T_DIR) != srcdir ||
       (srcdir && (!isdirempty(dst) || (isext2(dst) && dst->nlink != 2))))
      goto unlock;
    if(dirreplace(b, newoff, new, src->inum) < 0){
      result = -P9_EIO;
      goto unlock;
    }
  } else if(dirlink(b, new, src->inum) < 0){
    result = -P9_ENOSPC;
    goto unlock;
  }

  if(dirreplace(a, oldoff, 0, 0) < 0){
    result = -P9_EIO;
    goto unlock;
  }
  if(srcdir && a != b){
    if(dirreplace(src, parentoff, "..", b->inum) < 0){
      result = -P9_EIO;
      goto unlock;
    }
    a->nlink--;
    b->nlink++;
    if(iupdate(a) < 0 || iupdate(b) < 0){
      result = -P9_EIO;
      goto unlock;
    }
  }
  if(dst){
    if(srcdir){
      b->nlink--;
      if(iupdate(b) < 0){
        result = -P9_EIO;
        goto unlock;
      }
    }
    dst->nlink -= isext2(dst) && srcdir ? 2 : 1;
    if(iupdate(dst) < 0){
      result = -P9_EIO;
      goto unlock;
    }
  }
  result = 0;
unlock:
  if(dstlocked)
    iunlock(dst);
  iunlock(src);
  if(second)
    iunlock(second);
  iunlock(first);
put:
  if(dst)
    iput(dst);
  if(src)
    iput(src);
  if(b)
    iput(b);
  if(a)
    iput(a);
  return result;
}

int
sys_rename(void)
{
  char *a, *b;
  int result;

  if(argstr(0, &a) < 0 || argstr(1, &b) < 0)
    return -P9_EINVAL;
  begin_op();
  namelock();
  result = rename_locked(a, b);
  nameunlock();
  end_op();
  return result;
}

static int
localfd(int arg, struct file **f)
{
  if(argfd(arg, 0, f) < 0)
    return -P9_EBADF;
  return (*f)->type == FD_LOCAL ? 0 : -P9_ENOTSOCK;
}

static int
localaddress(struct sockaddr_un *address)
{
  char *data;
  int size;
  if(argint(2, &size) < 0 || size <= sizeof(address->sun_family) ||
     size > sizeof(*address) || argptr(1, &data, size) < 0)
    return -P9_EINVAL;
  memset(address, 0, sizeof(*address));
  memmove(address, data, size);
  if(address->sun_family != AF_UNIX)
    return -P9_EAFNOSUPPORT;
  if(!address->sun_path[0])
    return -P9_EINVAL;
  for(int i = 0; i < size - sizeof(address->sun_family); i++)
    if(!address->sun_path[i])
      return 0;
  return -P9_EINVAL;
}

static int
loopbackport(void)
{
  struct sockaddr_in addr;
  int size;
  char *data;
  if(argint(2, &size) < 0 || size != sizeof(addr) || argptr(1, &data, size) < 0)
    return -P9_EINVAL;
  memmove(&addr, data, sizeof(addr));
  if(addr.sin_family != AF_INET)
    return -P9_EAFNOSUPPORT;
  if(__builtin_bswap32(addr.sin_addr.s_addr) != INADDR_LOOPBACK)
    return -P9_EADDRNOTAVAIL;
  int port = __builtin_bswap16(addr.sin_port);
  return port ? port : -P9_EINVAL;
}

int
sys_socket(void)
{
  int domain, type, protocol;
  if(argint(0, &domain) < 0 || argint(1, &type) < 0 ||
     argint(2, &protocol) < 0 || (type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC)) != SOCK_STREAM ||
     (protocol && (domain != AF_INET || protocol != IPPROTO_TCP)))
    return -P9_EINVAL;
  if(domain != AF_UNIX && domain != AF_INET)
    return -P9_EAFNOSUPPORT;
  struct file *f = filealloc();
  if(!f)
    return -P9_ENFILE;
  int fd = fdalloc(f), result = -P9_EMFILE;
  if(fd >= 0)
    result = localalloc(f, domain);
  if(result){
    if(fd >= 0)
      myproc()->ofile[fd] = 0;
    fileclose(f);
    return result;
  }
  if(type & SOCK_CLOEXEC)
    myproc()->closeexec |= 1U << fd;
  f->nonblock = !!(type & SOCK_NONBLOCK);
  return fd;
}

int
sys_bind(void)
{
  struct file *f;
  struct sockaddr_un address;
  int result = localfd(0, &f);
  if(result < 0)
    return result;
  if(localfamily(f->local) == AF_INET){
    int port = loopbackport();
    if(port < 0)
      return port;
    if(port < 1024 && myproc()->uid)
      return -P9_EACCES;
    return localport(f->local, port);
  }
  if((result = localaddress(&address)) < 0 || (result = localreserve(f->local)) < 0)
    return result;
  begin_op();
  struct proc *p = myproc();
  struct inode *ip = create(address.sun_path, T_SOCKET, p->uid, ID_MAX + 1U,
                           0777 & ~p->umask, 1, 0);
  if(ip)
    iunlock(ip);
  end_op();
  localname(f->local, ip);
  return ip ? 0 : -P9_EADDRINUSE;
}

int
sys_listen(void)
{
  struct file *f;
  int backlog, result = localfd(0, &f);
  if(result < 0)
    return result;
  if(argint(1, &backlog) < 0 || backlog < 0)
    return -P9_EINVAL;
  return locallisten(f->local, backlog);
}

int
sys_connect(void)
{
  struct file *f;
  struct sockaddr_un address;
  int result = localfd(0, &f);
  if(result < 0)
    return result;
  if(localfamily(f->local) == AF_INET){
    int port = loopbackport();
    return port < 0 ? port : localconnect(f->local, 0, port);
  }
  if((result = localaddress(&address)) < 0)
    return result;
  begin_op();
  struct inode *ip = namei(address.sun_path);
  if(!ip){
    end_op();
    return -P9_ENOENT;
  }
  ilock(ip);
  result = ip->type != T_SOCKET ? -P9_ECONNREFUSED :
           !inodeaccess(ip, M_WRITE) ? -P9_EACCES : 0;
  iunlock(ip);
  end_op();
  if(!result)
    result = localconnect(f->local, ip, 0);
  begin_op();
  iput(ip);
  end_op();
  return result;
}

int
sys_accept(void)
{
  struct file *listener;
  int address, result = localfd(0, &listener);
  if(result < 0)
    return result;
  if(argint(1, &address) < 0)
    return -P9_EINVAL;
  uint *len = 0;
  char *name = 0;
  uint size = 0;
  int family = localfamily(listener->local);
  struct sockaddr_in peer = { .sin_family = family, .sin_addr.s_addr = __builtin_bswap32(INADDR_LOOPBACK) };
  uint reply_size = family == AF_INET ? sizeof(peer) : sizeof(peer.sin_family);
  if(address){
    if(argptrwrite(2, (char**)&len, sizeof(*len)) < 0)
      return -P9_EFAULT;
    size = *len < reply_size ? *len : reply_size;
    if(argptrwrite(1, &name, size) < 0)
      return -P9_EFAULT;
  }
  struct file *f = filealloc();
  if(!f)
    return -P9_ENFILE;
  int fd = fdalloc(f);
  result = fd < 0 ? -P9_EMFILE : localaccept(listener, f);
  if(result < 0){
    if(fd >= 0)
      myproc()->ofile[fd] = 0;
    fileclose(f);
    return result;
  }
  if(len){
    memmove(name, &peer, size);
    *len = reply_size;
  }
  return fd;
}

int
sys_openpty(void)
{
  char *fds;
  if(argptrwrite(0, &fds, 2 * sizeof(int)) < 0)
    return -P9_EFAULT;
  struct file *master = filealloc(), *slave = filealloc();
  int result = -P9_ENFILE, a = -1, b = -1;
  if(master && slave){
    a = fdalloc(master);
    if(a >= 0)
      b = fdalloc(slave);
    result = b < 0 ? -P9_EMFILE : ptyalloc(master, slave);
  }
  if(result < 0){
    if(a >= 0)
      myproc()->ofile[a] = 0;
    if(b >= 0)
      myproc()->ofile[b] = 0;
    if(master)
      fileclose(master);
    if(slave)
      fileclose(slave);
    return result;
  }
  ((int*)fds)[0] = a;
  ((int*)fds)[1] = b;
  return result;
}

int
sys_shutdown(void)
{
  struct file *f;
  int how, result = localfd(0, &f);
  if(result < 0)
    return result;
  if(argint(1, &how) < 0)
    return -P9_EINVAL;
  return localshutdown(f->local, how);
}

int
sys_filemtime(void)
{
  char *path, *out;
  if(argstr(0, &path) < 0 || argptrwrite(1, &out, sizeof(uint)) < 0)
    return -P9_EFAULT;
  begin_op();
  struct inode *ip = namei(path);
  int result = -P9_ENOENT;
  if(ip){
    ilock(ip);
    if(ip->type && !(ip->flags & I_SHADOW)){
      *(uint*)out = isext2(ip) ? ip->mtime : 0;
      result = 0;
    }
    iunlockput(ip);
  }
  end_op();
  return result;
}
