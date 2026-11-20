/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_USBHOST_H
#define PROJECT9_USBHOST_H

#define USB_HOST_COUNT 8
#define USB_PORT_COUNT 15
#define USB_IN 0
#define USB_OUT 1
#define USB_SETUP 2

struct pci_device;

struct usbhost {
  const char *name;
  volatile unsigned int *regs;
  unsigned int ports;
  int broken;
  unsigned char present[USB_PORT_COUNT];
  void *dma;
  unsigned int pci_address;
  unsigned int legacy, companion, highspeed;
  int (*start)(struct usbhost *);
  int (*status)(struct usbhost *, unsigned int);
  int (*reset)(struct usbhost *, unsigned int);
  int (*packet)(struct usbhost *, unsigned int, unsigned int, unsigned int,
                unsigned int, unsigned int, void *, unsigned int);
};

struct usbhost *usbhost_register(const char *name);
void usbwait(unsigned int ticks);
void ohcifound(const struct pci_device *);
void ehcifound(const struct pci_device *);
void usbinit(void);
int usbscan(void);
int usbdisks(void);

#endif
