<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Stability and recovery

Project9 dev is the development line for Project9 v1. The development
label applies to the system and boot menu.

## Check a disk image

Stop QEMU and then choose **Check disks** in `make os`, or run
`make fsck`. These run `e2fsck -fn` without changing the image.
Use `make fsck FS_IMAGE=path/to/copy.img` to check another volume.

Ext2 records an unclean state while changes are pending. A failed write
or flush returns an I/O error from `sync`, `umount`, and shutdown. A
faulted volume refuses further writes. A dirty root requires an offline
check before it can boot. Ext2 has no journal, so an interrupted
operation can require repair or lose recent changes.

While an ATA cache flush is pending, the driver sleeps between status
checks for up to 3000 kernel ticks. The serial console reports the
disk and its status if the controller fails or reaches that deadline.

To try automatic recovery of a stopped guest, use a new destination
filename. The tool keeps the source, runs e2fsck on the copy, verifies
it with a second pass, and only then publishes the output.

```sh
python3 tools/installimage.py --recover-to recovered.img test-target-fast.img
```

When automatic repair cannot complete, use e2fsck on a separate offline
copy. For an install image, attach the copy via a partition-aware host
loop device and point e2fsck at its ext2 partition. Recovery preserves
the boot prefix and the bytes after the partition.

## Run the stability checks

`stability` tests invalid syscall pointers, allocation rollback,
repeated fork and wait, and process exhaustion. Run
`stability full-disk` on a disposable small ext2 disk to also test space
exhaustion and reclamation. Both should end with `STABILITY TESTS PASSED`.
Check the resulting images offline after the guest finishes.

Run `stability hardening` for deterministic randomized pointer and size
checks, malformed exec arguments and headers, read-only page faults,
private mappings across fork, and credential isolation. On a fresh
installer test disk, `stability inodes` checks inode exhaustion and reuse.

`make storage-test` boots disposable IDE and AHCI guests from the release
image. It checks installation boundaries, reboot persistence, offline
filesystem integrity, disk and inode exhaustion, and injected read,
write, directory metadata, and flush failures. The flush test accepts
failure while committing the dirty marker or during a later sync. In
both cases the filesystem must report the error and refuse more writes.
If the dirty marker failed, the test checks that every other sector is
unchanged. If sync failed, the volume must still be dirty.

The suite also checks refusal of dirty, malformed, or ambiguous roots
and boots a verified recovery copy. The runner prints the current
stage, reports the last serial output during long waits, and stops
immediately when a command returns without its success marker.
Use `python3 tools/storage-test.py --serial` to watch the guest console.
The runner saves serial logs on failure or interruption. Use
`--workdir NEW_DIRECTORY` to keep the disposable images as well, or
`--timeout 600` to give each guest response more time on a slow host.
For a run that requires both guest CPUs, use the prebuilt release. Add
`--x86-64` to select the x86_64 kernel.

```sh
python3 tools/storage-test.py --require-smp
```

The guest `sysctl hw.ncpu` reports online CPUs. The parallel workload
checks that worker processes actually ran on at least two CPUs while
allocating and freeing private mappings, editing files in a shared
directory, and running fork, exec, wait, and pipe operations. To run
the workload in an installed guest, use `stability smp`.

## Read a crash report

Kernel panics print to the local console and serial port. Assertions
include the failed expression and source location. Kernel trap reports
include the faulting instruction and saved registers. Page fault
reports also include the address that triggered the fault. Keep the
report and the exact kernel ELF used for the boot. With GNU binutils,
run `addr2line -f -e KERNEL ADDRESS`. In the hybrid profile the ELF64
kernel is `build/x86_64/native/kernel.elf` and the ELF32 kernel is
`build/x86_64/compat-i386/kernel.elf`.

A foreground process that quits with the default SIGQUIT action can
produce a private `core.PID` file in its working directory. Run
`coreinfo core.PID` to read its registers and saved memory size.
The file may contain secrets from the process’s address space.

## CPU support

Idle processors halt until an interrupt arrives. The kernel chooses an
FPU context at boot based on CPUID. It saves traditional x87 state on
older CPUs, uses FXSAVE on SSE CPUs, and uses XSAVE for x87, SSE, and
AVX where available. The SDK selects AVX or SSE memory copying when
supported and uses an integer copy elsewhere.

Project9-built executables place code and writable data on separate
pages. Trap entry clears the direction flag before entering C and
restores the saved user flags on return. The ELF loader honors read-only load segments. Only root can
create device nodes.


## Filesystem and memory interfaces

The VFS dispatches inode operations by mounted backend. Built-in root
and data attachments are selected during boot.

Private mmap snapshots file bytes into owned pages. A fork duplicates
those pages and preserves protection bits. Exec discards old mappings.
Syscall buffer validation checks the mapping bounds and page access
permissions. See mmap(2) for mapping limits.
