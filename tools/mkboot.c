/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../boot/pc/layout.h"

static void
fail(const char *message)
{
  fprintf(stderr, "mkboot: %s\n", message);
  exit(1);
}

static FILE *
open_file(const char *name, const char *mode)
{
  FILE *stream = fopen(name, mode);

  if(!stream){
    perror(name);
    exit(1);
  }
  return stream;
}

static long
length(FILE *stream)
{
  long size;

  if(fseek(stream, 0, SEEK_END) || (size = ftell(stream)) < 0 ||
     fseek(stream, 0, SEEK_SET))
    fail("cannot measure input");
  return size;
}

static void
seek_block(FILE *stream, long sector)
{
  if(fseek(stream, sector * BOOT_SECTOR_SIZE, SEEK_SET))
    fail("cannot seek in the boot image");
}

static void
copy(FILE *source, FILE *target)
{
  unsigned char buf[4096];
  size_t n;

  while((n = fread(buf, 1, sizeof(buf), source)) != 0)
    if(fwrite(buf, 1, n, target) != n)
      fail("cannot write the boot image");
  if(ferror(source))
    fail("cannot read the boot input");
}

static void
check_mbr(FILE *stream, const char *name)
{
  long size = length(stream);

  if(size != BOOT_SECTOR_SIZE){
    fprintf(stderr, "mkboot: %s is %ld bytes, expected %d\n",
            name, size, BOOT_SECTOR_SIZE);
    exit(1);
  }
  if(fseek(stream, BOOT_SECTOR_SIZE - 2, SEEK_SET) ||
     fgetc(stream) != 0x55 || fgetc(stream) != 0xaa)
    fail("mbr must end with the boot signature 55 aa");
  if(fseek(stream, 0, SEEK_SET))
    fail("cannot rewind the mbr");
}

static void
check_stage(FILE *stream, long size)
{
  const long runtime = 0x1000;
  unsigned char header[16], magic[4];
  long runtime_size, client;

  // btx finds the client using the runtime size stored in its header
  if(size < runtime + (long)sizeof(header) ||
     fseek(stream, runtime, SEEK_SET) ||
     fread(header, 1, sizeof(header), stream) != sizeof(header) ||
     memcmp(header, "\xeb\x0e" "BTX", 5))
    fail("missing btx runtime header at byte 4096");

  runtime_size = header[10] | (header[11] << 8);
  client = runtime + runtime_size;
  if(runtime_size < (long)sizeof(header) ||
     client > size - (long)sizeof(magic) ||
     fseek(stream, client, SEEK_SET) ||
     fread(magic, 1, sizeof(magic), stream) != sizeof(magic) ||
     memcmp(magic, "\x7f" "ELF", sizeof(magic)))
    fail("btx client elf does not follow the runtime");
  if(fseek(stream, 0, SEEK_SET))
    fail("cannot rewind the boot stage");
}

static void
little_endian(unsigned char *dst, unsigned long value)
{
  int i;

  for(i = 0; i < 4; i++){
    dst[i] = value & 0xff;
    value >>= 8;
  }
}

int
main(int argc, char **argv)
{
  FILE *mbr, *stage, *kernel, *kernel64 = NULL, *out;
  long stage_size, kernel_size, offset64 = 0, size64 = 0;
  unsigned char info[BOOT_SECTOR_SIZE] = {0};

  if(argc != 5 && argc != 6)
    fail("usage: mkboot output mbr stage kernel [kernel64]");
  mbr = open_file(argv[2], "rb");
  stage = open_file(argv[3], "rb");
  kernel = open_file(argv[4], "rb");
  stage_size = length(stage);
  kernel_size = length(kernel);
  check_mbr(mbr, argv[2]);
  if(BOOT_STAGE_BLOCKS > BOOT_KERNEL_LBA - 2 ||
     stage_size <= 0 || stage_size > BOOT_STAGE_BLOCKS * BOOT_SECTOR_SIZE)
    fail("boot stage does not fit");
  check_stage(stage, stage_size);
  if(kernel_size <= 0 || kernel_size > BOOT_PAYLOAD_MAX)
    fail("kernel does not fit");
  if(argc == 6){
    kernel64 = open_file(argv[5], "rb");
    size64 = length(kernel64);
    offset64 = ((kernel_size + BOOT_SECTOR_SIZE - 1) / BOOT_SECTOR_SIZE) * BOOT_SECTOR_SIZE;
    if(size64 < 64 || size64 > BOOT_PAYLOAD_MAX - offset64)
      fail("x86_64 kernel does not fit");
  }

  out = open_file(argv[1], "wb");
  copy(mbr, out);
  copy(stage, out);
  seek_block(out, BOOT_KERNEL_LBA - 1);
  little_endian(info, BOOT_KERNEL_MAGIC);
  little_endian(info+4, kernel64 ? offset64 + size64 : kernel_size);
  little_endian(info+8, offset64);
  little_endian(info+12, size64);
  little_endian(info+16, kernel_size);
  if(fwrite(info, 1, sizeof(info), out) != sizeof(info))
    fail("cannot write the kernel descriptor");
  copy(kernel, out);
  if(kernel64){
    seek_block(out, BOOT_KERNEL_LBA + offset64 / BOOT_SECTOR_SIZE);
    copy(kernel64, out);
    fclose(kernel64);
  }

  // keep the same image size as the other project9 boot disks
  if(ftell(out) < BOOT_DISK_BLOCKS * BOOT_SECTOR_SIZE){
    if(fseek(out, BOOT_DISK_BLOCKS * BOOT_SECTOR_SIZE - 1, SEEK_SET) ||
       fputc(0, out) == EOF)
      fail("cannot pad the boot image");
  }
  if(fclose(out))
    fail("cannot close the boot image");
  fclose(mbr);
  fclose(stage);
  fclose(kernel);
  return 0;
}
