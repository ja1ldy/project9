/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_ACPI_H
#define PROJECT9_ACPI_H

const unsigned char *acpifind(const char *);
extern unsigned int irq_gsi[16];
extern unsigned short irq_flags[16];

#endif
