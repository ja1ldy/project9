/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "stat.h"
#include "core.h"
#include "fcntl.h"
#include "user.h"

static int
show(const char *path)
{
  struct core_header header;
  struct trapframe *r = &header.registers;
  struct stat st;
  uint saved;
  int fd;

  if((fd = open(path, O_RDONLY)) < 0){
    printf(2, "coreinfo: %s: cannot open\n", path);
    return 1;
  }
  if(fstat(fd, &st) < 0 || st.type != T_FILE ||
     read(fd, &header, sizeof(header)) != sizeof(header) ||
     header.magic != CORE_MAGIC || header.version != CORE_VERSION ||
     header.saved_size > header.memory_size){
    close(fd);
    printf(2, "coreinfo: %s: invalid or unsupported core header\n", path);
    return 1;
  }
  close(fd);
  saved = st.size - sizeof(header);
  if(saved > header.saved_size){
    printf(2, "coreinfo: %s: unexpected data after the core image\n", path);
    return 1;
  }

  printf(1, "%s: pid %d uid %d signal %d\n", path, header.pid, header.uid, header.signal);
  printf(1, "memory: %d bytes saved of %d address-space bytes\n", saved, header.memory_size);
  printf(1, "eip=0x%x esp=0x%x ebp=0x%x eflags=0x%x\n", r->eip, r->esp, r->ebp, r->eflags);
  printf(1, "eax=0x%x ebx=0x%x ecx=0x%x edx=0x%x esi=0x%x edi=0x%x\n",
         r->eax, r->ebx, r->ecx, r->edx, r->esi, r->edi);
  if(saved < header.saved_size){
    printf(2, "coreinfo: %s: incomplete dump, expected %d saved bytes\n", path, header.saved_size);
    return 1;
  }
  return 0;
}

int
main(int argc, char **argv)
{
  int i, status = 0;

  if(argc < 2){
    printf(2, "usage: coreinfo file ...\n");
    exitstatus(2);
  }
  for(i = 1; i < argc; i++)
    status |= show(argv[i]);
  exitstatus(status);
}
