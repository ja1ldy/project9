/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_INSTALL_H
#define PROJECT9_INSTALL_H

#define INSTALL_TAG_OFFSET 440
#define INSTALL_MARK_OFFSET 444
#define INSTALL_MARK 0x3950
#define INSTALL_PART_OFFSET 446
#define INSTALL_PART_TYPE 0x83
#define INSTALL_RAM_BASE 0x800000U
#define INSTALL_RAM_MAX 0x4000000U

static inline unsigned int
install_dword(const unsigned char *data)
{
  return data[0] | (unsigned int)data[1] << 8 |
         (unsigned int)data[2] << 16 | (unsigned int)data[3] << 24;
}

static inline int
install_partition(const unsigned char *sector, unsigned int *tag,
                  unsigned int *first, unsigned int *count)
{
  const unsigned char *p = sector + INSTALL_PART_OFFSET;

  if(sector[510] != 0x55 || sector[511] != 0xaa ||
     sector[INSTALL_MARK_OFFSET] != (INSTALL_MARK & 255) ||
     sector[INSTALL_MARK_OFFSET+1] != (INSTALL_MARK >> 8) ||
     p[0] != 0x80 || p[4] != INSTALL_PART_TYPE)
    return 0;
  *tag = install_dword(sector + INSTALL_TAG_OFFSET);
  *first = install_dword(p + 8);
  *count = install_dword(p + 12);
  return *first >= 2048 && *count >= 4 && *count <= ~0U - *first;
}

#endif
