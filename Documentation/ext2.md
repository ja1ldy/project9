<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Ext2 root and data disks

Project9 dev mounts the ext2 partition of `install.img` at `/`.
The same disk carries the BIOS loader and kernel before the partition.
A separate ext2 data disk can mount at `/ext2` when that directory is
empty.

## Set up a disk

Run `make os` in a terminal. The setup menu uses ncurses. Configure the root size and block size, virtual
CPU count, guest RAM, display, and optional data image. Settings are
saved in `build/<arch>/project9.json`. New filesystem
settings apply at creation or migration.

Choose **Start Project9** to build and boot or **Build the boot and root
images** to build offline. Creating and migrating images requires Python
3.9 or later and e2fsprogs with `mke2fs`, `debugfs`, and `e2fsck`.
For unattended builds use `make os OS_ARGS=--prepare`.

The default root is 32 MiB with 1 KiB blocks. A fresh disk uses a root password set in setup or prompts for one at first boot. Select an existing separate
ext2 image in the wizard to use an earlier `/ext2` data disk.

## Migrate a previous installation

With QEMU stopped, put the old Project9 `fs.img` in the new source tree.
Choose **Start Project9**. The wizard imports `fs.img` into
`install.img` and keeps `fs.img` as the original. For an older xv6-fs
image, approve conversion. The guest accepts the existing password.
A v0.1 disk needs conversion to the Project9 v0.2 format with a previous
release before this step.

## Check a disk

With QEMU stopped, choose **Check disks** or run `make fsck`. The latter
extracts and checks the root partition of `install.img` by default. For another image use
`make fsck FS_IMAGE=path/to/disk.img`. The wizard also checks a configured
data image. Both use `e2fsck -fn` for a read-only inspection.

Ext2 has no journal. Project9 flushes the dirty flag before changing the
filesystem. Ordinary writes send data and metadata to the disk controller.
I/O or metadata validation errors fault the volume and block further writes.
`sync`, clean unmount, and shutdown flush the drive before writing the
clean flag and flush again to commit it. An open unlinked file blocks
marking its volume clean.

Use `poweroff` inside the guest before closing QEMU. After an unexpected
stop, check and repair a copy of a dirty root disk offline before using
it again. A dirty secondary disk mounts read-only. See
[recovery](stability.md) for the check and repair commands.

## Measure filesystem I/O

```sh
sysctl kern.diskstats
sysctl vfs.ext2.stats
```

The disk counters report physical transfer requests, sectors, flush
commands, and I/O errors. The ext2 counters report sector-cache hits,
misses, and identical writes skipped. The counters are cumulative
since boot. RAM-live filesystem traffic appears in the ext2 counters
and bypasses the physical storage counters.

For a comparison, boot each build from its own copy of a clean test
disk and time `usertests` from its first line through
`ALL TESTS PASSED`. Keep the QEMU disk controller and host storage
the same. Time the installer from the confirmation until its
completion prompt to measure partitioning, extraction, checks, and the
boot payload together.

## Format support

The driver reads whole ext2 revision 0 and 1 disks with 1, 2, or 4 KiB
blocks, 512-byte logical sectors and 28-bit ATA LBA. Regular files use
signed 32-bit offsets and can contain sparse holes. Names can occupy up
to 255 bytes. Inodes can have hard links and symbolic links. An inode
with external extended attributes, an unsupported special file type,
or a UID or GID wider than 16 bits is unavailable. Indexed directories
are readable and their entries are protected from edits. Devices can
be opened only on the root disk.

The BIOS install disk uses one Project9 MBR root partition. Native
storage support covers 512-byte ATA and AHCI devices. The RAM-live
loader can also start from BIOS-supported USB drives. The kernel
does not provide USB mass-storage writeback. Ext3 and ext4 layouts
are outside the ext2 driver.
