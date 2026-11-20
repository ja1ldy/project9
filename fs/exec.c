/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"
#include "fat.h"
#include "stat.h"
#include "mode.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"


static int execimage(char*, char**, char**, int);

// an interpreter must be an executable in its own right
static int
shebang(char *line, char *path, char **argv, char **envp)
{
  char *name, *option = 0, *p;
  char *args[MAXARG+1];
  int count = 0, i;

  p = line + 2;
  while(*p == ' ' || *p == '\t')
    p++;
  name = p;
  while(*p && *p != '\n' && *p != '\r' && *p != ' ' && *p != '\t')
    p++;
  if(*name != '/' || p == name)
    return -1;
  if(*p == ' ' || *p == '\t'){
    *p++ = 0;
    while(*p == ' ' || *p == '\t')
      p++;
    if(*p && *p != '\r' && *p != '\n'){
      option = p;
      while(*p && *p != '\r' && *p != '\n')
        p++;
      while(p > option && (p[-1] == ' ' || p[-1] == '\t'))
        p--;
    }
  }
  *p = 0;
  args[count++] = name;
  if(option)
    args[count++] = option;
  args[count++] = path;
  for(i = argv[0] ? 1 : 0; argv[i]; i++){
    if(count == MAXARG)
      return -1;
    args[count++] = argv[i];
  }
  args[count] = 0;
  return execimage(name, args, envp, 0);
}

// copy one vector downward onto the new stack and record its addresses
static int
pushvector(pde_t *pgdir, uint *sp, uint bottom, char **items, uint *addrs)
{
  uint count, bytes;

  for(count = 0; items[count]; count++){
    bytes = strlen(items[count]) + 1;
    if(((bytes + 3) & ~3) > *sp - bottom)
      return -1;
    *sp = (*sp - bytes) & ~3;
    if(copyout(pgdir, *sp, items[count], bytes) < 0)
      return -1;
    addrs[count] = *sp;
  }
  addrs[count] = 0;
  return count;
}

struct image {
  struct fatslice file;
  uint entry, phoff, phnum, phsize;
  int native;
};

static int
slice_read(struct inode *ip, struct fatslice f, uint off, void *dst, uint count)
{
  return off <= f.size && count <= f.size-off &&
         readi(ip, dst, f.offset+off, count) == (int)count;
}

static int
slice_valid(struct fatslice f, uint size)
{
  return f.offset >= sizeof(struct fatheader) && !(f.offset % P9_FAT_ALIGN) &&
         f.offset <= size && f.size && f.size <= size-f.offset;
}

static int
image_header(struct inode *ip, uint magic, struct image *image)
{
  uchar ident[7];
  struct fatslice file = {0, ip->size};
  int selected = 0;

  if(magic == P9_FAT_MAGIC){
    struct fatheader fat;
    if(!slice_read(ip, file, 0, &fat, sizeof(fat)) || fat.version != P9_FAT_VERSION ||
       !slice_valid(fat.slice32, file.size) || !slice_valid(fat.slice64, file.size) ||
       fat.slice32.offset + fat.slice32.size > fat.slice64.offset)
      return 0;
#ifdef P9_KERNEL64
    file = fat.slice64;
    selected = 2;
#else
    file = fat.slice32;
    selected = 1;
#endif
  }
  if(!slice_read(ip, file, 0, ident, sizeof(ident)) ||
     memcmp(ident, "\177ELF", 4) || (selected && ident[4] != selected) ||
     ident[5] != 1 || ident[6] != 1)
    return 0;
  image->file = file;
  if(ident[4] == 1){
    struct elfhdr elf;
    if(!slice_read(ip, file, 0, &elf, sizeof(elf)) ||
       elf.machine != 3 || elf.type != 2 || elf.version != 1 ||
       elf.ehsize != sizeof(elf) || elf.phentsize != sizeof(struct proghdr) ||
       elf.phoff < sizeof(elf))
      return 0;
    image->entry = elf.entry;
    image->phoff = elf.phoff;
    image->phnum = elf.phnum;
    image->phsize = elf.phentsize;
#ifdef P9_KERNEL64
  } else if(ident[4] == 2){
    struct elfhdr64 elf;
    if(!slice_read(ip, file, 0, &elf, sizeof(elf)) ||
       elf.machine != 62 || elf.type != 2 || elf.version != 1 ||
       elf.ehsize != sizeof(elf) || elf.phentsize != sizeof(struct proghdr64) ||
       elf.entry >= FILEMAP_BASE || elf.phoff < sizeof(elf) || elf.phoff > file.size)
      return 0;
    image->entry = elf.entry;
    image->phoff = elf.phoff;
    image->phnum = elf.phnum;
    image->phsize = elf.phentsize;
    image->native = 1;
#endif
  } else {
    return 0;
  }
  return image->phoff <= file.size && image->phnum <= (file.size-image->phoff) / image->phsize;
}

static int
image_segment(struct inode *ip, const struct image *image, uint off, struct proghdr *ph)
{
#ifdef P9_KERNEL64
  if(image->native){
    struct proghdr64 wide;
    if(!slice_read(ip, image->file, off, &wide, sizeof(wide)))
      return 0;
    if(wide.type != ELF_PROG_LOAD){
      ph->type = wide.type;
      return 1;
    }
    if(wide.off > image->file.size || wide.filesz > image->file.size-wide.off ||
       wide.vaddr >= FILEMAP_BASE || wide.memsz >= FILEMAP_BASE-wide.vaddr)
      return 0;
    *ph = (struct proghdr){ .type = wide.type, .flags = wide.flags,
                           .vaddr = wide.vaddr, .memsz = wide.memsz,
                           .off = wide.off, .filesz = wide.filesz };
    return 1;
  }
#endif
  return slice_read(ip, image->file, off, ph, sizeof(*ph));
}

static int
copy_stack(pde_t *pgdir, uint *stack, uint bottom, char **argv, char **envp, int native)
{
  uint cells[4+MAXARG+1+MAXENV+1], sp = *stack;
  uint base = native ? 1 : 4, framesize, envbase;
  int argc, envc;

  argc = pushvector(pgdir, &sp, bottom, argv, cells + base);
  if(argc < 0)
    return 0;
  envbase = base + argc + 1;
  envc = pushvector(pgdir, &sp, bottom, envp, cells + envbase);
  if(envc < 0)
    return 0;

#ifdef P9_KERNEL64
  if(native){
    uptr wide[1+MAXARG+1+MAXENV+1];
    int count = envbase + envc + 1;
    cells[0] = argc;
    for(int i = 0; i < count; i++)
      wide[i] = cells[i];
    framesize = count * sizeof(uptr);
    if(framesize > sp - bottom)
      return 0;
    sp = (sp-framesize) & ~15U;
    if(sp < bottom || copyout(pgdir, sp, wide, framesize) < 0)
      return 0;
  } else
#endif
  {
    framesize = (envbase + envc + 1) * sizeof(uint);
    if(framesize > sp - bottom)
      return 0;
    sp -= framesize;
    cells[0] = 0xffffffff;
    cells[1] = argc;
    cells[2] = sp + 4 * sizeof(uint);
    cells[3] = sp + envbase * sizeof(uint);
    if(copyout(pgdir, sp, cells, framesize) < 0)
      return 0;
  }
  *stack = sp;
  return 1;
}

static int
execimage(char *path, char **argv, char **envp, int scripts)
{
  char *s, *last;
  int i, n, entry_ok = 0;
  uint off;
  uint sz, sp, bottom, magic;
  char script[128];
  struct image image = {0};
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir = 0, *oldpgdir;
  struct proc *curproc = myproc();

  begin_op();

  if((ip = namei(path)) == 0){
    end_op();
    // let the shell report a command it cannot find
    return -1;
  }
  ilock(ip);
  if(ip->type != T_FILE || !inodeaccess(ip, M_EXEC))
    goto bad;

  n = readi(ip, (char*)&magic, 0, sizeof(magic));
  if(n != sizeof(magic) || (magic != ELF_MAGIC && magic != P9_FAT_MAGIC)){
    if(!scripts || n < 2)
      goto bad;
    n = readi(ip, script, 0, sizeof(script) - 1);
    if(n < 2 || script[0] != '#' || script[1] != '!')
      goto bad;
    script[n] = 0;
    for(i = 2; i < n && script[i] && script[i] != '\n'; i++)
      ;
    if((i == n && n == sizeof(script) - 1) || (!script[i] && i < n))
      goto bad;
    iunlockput(ip);
    end_op();
    return shebang(script, path, argv, envp);
  }

  if(!image_header(ip, magic, &image))
    goto bad;

  if((pgdir = setupkvm()) == 0)
    goto bad;

  // Load program into memory.
  sz = 0;
  for(i=0, off=image.phoff; i<image.phnum; i++, off+=image.phsize){
    if(!image_segment(ip, &image, off, &ph))
      goto bad;
    if(ph.type == ELF_PROG_INTERP || ph.type == ELF_PROG_DYNAMIC)
      goto bad;
    if(ph.type != ELF_PROG_LOAD)
      continue;
    if(ph.memsz < ph.filesz)
      goto bad;
    if(!ph.memsz)
      continue;
    if(ph.vaddr < sz || ph.vaddr % PGSIZE ||
       ph.vaddr >= FILEMAP_BASE || ph.memsz >= FILEMAP_BASE - ph.vaddr ||
       ph.off > image.file.size || ph.filesz > image.file.size - ph.off)
      goto bad;
    if((ph.flags & ELF_PROG_FLAG_EXEC) &&
       image.entry >= ph.vaddr && image.entry - ph.vaddr < ph.filesz)
      entry_ok = 1;
    if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if(loaduvm(pgdir, (char*)(uptr)ph.vaddr, ip, image.file.offset + ph.off, ph.filesz) < 0)
      goto bad;
    if(!(ph.flags & ELF_PROG_FLAG_WRITE))
      userreadonly(pgdir, ph.vaddr, ph.vaddr + ph.memsz);
  }
  if(!entry_ok)
    goto bad;
  iunlockput(ip);
  end_op();
  ip = 0;

  // put a guard page below the user stack so a stack overflow traps
  sz = PGROUNDUP(sz);
  if(sz >= FILEMAP_BASE - (USTACKPAGES + 1) * PGSIZE ||
     (sz = allocuvm(pgdir, sz, sz + (USTACKPAGES + 1) * PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char*)(uptr)(sz - (USTACKPAGES + 1) * PGSIZE));
  sp = sz;
  // keep arguments and environment within the top page
  bottom = sz - PGSIZE;

  if(!copy_stack(pgdir, &sp, bottom, argv, envp, image.native))
    goto bad;

  // Save program name for debugging.
  for(last=s=path; *s; s++)
    if(*s == '/')
      last = s+1;
  setprocname(last);

  // replace the old address space once the new image is ready
  fbunmapuser(curproc);
  oldpgdir = curproc->pgdir;
  curproc->pgdir = pgdir;
  private_reset(curproc);
  curproc->heapbase = curproc->sz = sz;
  ttyrestore();
  signalexec();
  fpuexec();
  memset(curproc->tf, 0, sizeof(*curproc->tf));
  curproc->tf->cs = (SEG_UCODE << 3) | DPL_USER;
#ifdef P9_KERNEL64
  if(image.native)
    curproc->tf->cs = (SEG_UCODE64 << 3) | DPL_USER;
#endif
  curproc->tf->ss = (SEG_UDATA << 3) | DPL_USER;
  curproc->tf->ds = curproc->tf->es = curproc->tf->ss;
  curproc->tf->eip = image.entry;
  curproc->tf->esp = sp;
  curproc->tf->eflags = FL_IF | 2;
  switchuvm(curproc);
  freevm(oldpgdir);
  // keep descriptors intact until the new image is committed
  for(int fd = 0; fd < NOFILE; fd++){
    if((curproc->closeexec & (1U << fd)) && curproc->ofile[fd]){
      struct file *f = curproc->ofile[fd];
      curproc->ofile[fd] = 0;
      fileclose(f);
    }
  }
  curproc->closeexec = 0;
  return 0;

 bad:
  if(pgdir)
    freevm(pgdir);
  if(ip){
    iunlockput(ip);
    end_op();
  }
  return -1;
}

int
execve(char *path, char **argv, char **envp)
{
  return execimage(path, argv, envp, 1);
}
