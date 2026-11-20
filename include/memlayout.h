/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

// Memory layout

#define EXTMEM  0x100000            // Start of extended memory
#define PHYSTOP 0xE000000           // Top physical memory
#define DEVSPACE 0xFE000000         // Other devices are at high addresses

// Key addresses for address space layout (see kmap in vm.c for layout)
#define KERNBASE 0x80000000         // First kernel virtual address
#ifdef P9_KERNEL64
#define KERNVIRT 0xffffffff80000000
#define MMIOVIRT(pa) (0xffffffff00000000UL | (pa))
#else
#define KERNVIRT KERNBASE
#define MMIOVIRT(pa) (pa)
#endif
#define KERNLINK (KERNVIRT+EXTMEM)

#define V2P(a) ((uint)((uptr)(a) - KERNVIRT))
#define P2V(a) ((void *)((uptr)(a) + KERNVIRT))

#define V2P_WO(x) ((x) - KERNVIRT)    // same as V2P, but without casts
#define P2V_WO(x) ((x) + KERNVIRT)    // same as p2v but without casts
#define FB_VIRT MMIOVIRT(0xFD000000U)

#define FB_USER 0x7f000000
