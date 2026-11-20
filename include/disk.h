/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_DISK_H
#define PROJECT9_DISK_H

#define DISK_BATCH 8
#define AHCI_BATCH 4

int ataexttransfer(unsigned int, unsigned int, unsigned int, void *, int);
int ataextflush(unsigned int);
int ahcitransfer(unsigned int, unsigned int, unsigned int, void *, int);
int ahciflush(unsigned int);

unsigned int diskprobe(unsigned int);
int diskrw(unsigned int, unsigned int, unsigned int, void *, int);
int diskflush(unsigned int);
int diskislive(void);
void diskdiscover(void);
int diskcommand(int, unsigned int, unsigned int, unsigned int, void *);
int diskselect(unsigned int);
void diskdetach(void);

#endif
