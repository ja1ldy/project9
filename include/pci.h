/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_PCI_H
#define PROJECT9_PCI_H

struct pci_device {
  unsigned int address, id, class;
};

void pciinit(void);
unsigned int pciread(const struct pci_device *, unsigned int);
void pci_write16(const struct pci_device *, unsigned int, unsigned short);
void uhcifound(const struct pci_device *);
void pci_enable(const struct pci_device *, unsigned short);
int pci_present(unsigned int);
void ahcifound(const struct pci_device *);
unsigned int ahcidisks(void);
unsigned int ahciprobe(unsigned int);
void *iomap(unsigned int, unsigned int);

#endif
