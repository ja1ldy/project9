/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "diskuser.h"
#include "fcntl.h"
#include "stat.h"
#include "install.h"

static uchar mbr[DC_SECTOR], sector[8*DC_SECTOR];

static int
readfull(int fd, uchar *out, int n)
{
  int bytes;
  while(n){
    if((bytes = read(fd, out, n)) <= 0)
      return -1;
    out += bytes;
    n -= bytes;
  }
  return 0;
}

static uint
crc(const uchar *data, uint n)
{
  uint value = ~0U;
  while(n--){
    value ^= *data++;
    for(int i = 0; i < 8; i++)
      value = value >> 1 ^ ((value & 1) ? 0xedb88320U : 0);
  }
  return ~value;
}

int
main(int argc, char **argv)
{
  struct diskinfo disk;
  struct stat st;
  int id, fd;
  uint pos, n, tag, kernel, limit;

  if(argc != 3 || (id = diskid(argv[2], 0)) < 0){
    printf(2, "usage: installboot boot-image disk\n");
    return 2;
  }
  fd = open(argv[1], O_RDONLY);
  if(fd < 0 || fstat(fd, &st) < 0 || st.type != T_FILE ||
     st.size < 129*DC_SECTOR || st.size % DC_SECTOR ||
     diskctl(DC_INFO, id, 0, 1, &disk) < 0 || !(disk.flags & DC_TEST) ||
     disk.first != DC_START || st.size/DC_SECTOR > disk.first ||
     readfull(fd, mbr, sizeof(mbr)) < 0 || mbr[510] != 0x55 || mbr[511] != 0xaa ||
     lseek(fd, 127*DC_SECTOR, SEEK_SET) < 0 || readfull(fd, sector, 2*DC_SECTOR) < 0)
    goto bad;
  kernel = dget(sector+4, 4);
  if(dget(sector, 4) != 0x4b395050 || kernel < 52 || kernel > st.size-128*DC_SECTOR ||
     dcompare(sector+DC_SECTOR, "\177ELF\1\1", 6) ||
     lseek(fd, 4608, SEEK_SET) < 0 || readfull(fd, sector, DC_SECTOR) < 0 ||
     dcompare(sector, "\xeb\x0e" "BTX", 5) ||
     diskctl(DC_READ_PART, id, 2, 1, sector) < 0 || dget(sector+56, 2) != 0xef53 ||
     dget(sector+58, 2) != 1)
    goto bad;
  tag = crc(sector+104, 16);
  if(diskctl(DC_READ_MBR, id, 0, 1, sector) < 0)
    goto bad;
  memmove(mbr+446, sector+446, 64);
  dput(mbr+440, tag, 4);
  dput(mbr+444, INSTALL_MARK, 2);
  if(lseek(fd, DC_SECTOR, SEEK_SET) != DC_SECTOR)
    goto bad;
  limit = 128 + (kernel+DC_SECTOR-1)/DC_SECTOR;
  for(pos = 1; pos < limit; pos += n){
    n = limit - pos;
    if(n > 8)
      n = 8;
    if(readfull(fd, sector, n*DC_SECTOR) < 0 ||
       diskctl(DC_BOOTBLOCK, id, pos, n, sector) < 0)
      goto bad;
  }
  close(fd);
  fd = -1;
  if(diskctl(DC_FINISH, id, 0, 1, mbr) < 0)
    goto bad;
  return 0;
bad:
  if(fd >= 0)
    close(fd);
  printf(2, "installboot: incompatible image, protected target or disk write failed\n");
  return 1;
}
