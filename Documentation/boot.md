<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# BIOS boot menu

Boot the [release image](release.md) and choose a numbered entry.

| Key | Kernel | Root |
| --- | --- | --- |
| **1** | x86 (32-bit) | Disk |
| **2** | x86 (32-bit) | Live in RAM |
| **3** | x86_64 (64-bit) | Disk |
| **4** | x86_64 (64-bit) | Live in RAM |

The default is x86_64 disk boot when the CPU and image support it.
Otherwise the menu shows the two x86 entries and defaults to x86 disk
boot. Use the keyboard arrows to change the selection and Enter to boot.
Moving the selection stops the three-second countdown. Space pauses or
resumes it. Escape opens the loader prompt and R restarts the machine.

At the loader prompt, `boot` and `live` select x86, while `boot64`
and `live64` select x86_64. Use `menu` to return, `reboot` to restart,
or `help` to list the commands.
`-display none -serial mon:stdio` presents the numbered keys and prompt
over QEMU's serial console. Both the
menu and the guest console use 9600 baud on a hardware serial connection.

The video menu prefers 640 by 480 with a banked VBE framebuffer. It falls
back to VGA graphics or a BIOS text menu. The compiled artwork in
`boot/pc/menu.vga` begins with a 768-byte VGA palette and then two
run-length encoded bitmaps. The first is a 336 by 84 title. The second
is a 252 by 84 version of the supplied `boot/pc/logo.png`. Each run
contains a count and palette index and stays within a row. The menu uses
the existing Gallant glyphs and returns to text mode before the kernel
sets up the guest display.

## Boot chain

The first sector loads the boot stage from the BIOS drive using extended
INT 13h reads. FreeBSD's `btxldr` then relocates the BTX runtime and
starts the Project9 client. The client draws the menu, reads the kernel
ELF from the same disk, loads its segments, and returns to ring zero
through BTX for the kernel handoff.

| Boot disk sectors | Purpose |
| --- | --- |
| 0 | BIOS boot sector |
| 1 through 96 | FreeBSD BTX and the Project9 loader client |
| 127 | Payload size and optional ELF64 offset and size |
| 128 onward | ELF32 kernel and optional sector-aligned ELF64 payload |

The FreeBSD code is from the supplied source revision
`b5bff921452e4dcbe6c50f82ed4c7c07881d68e4`. The sources in
`boot/pc/btx` are the BIOS BTX runtime, ELF bootstrap, and client
startup and call stubs. Project9 supplies the disk layout, BIOS sector,
BTX argument declarations, menu, and kernel loader. GNU Make builds
the entire chain with the Project9 Makefile. See the
[FreeBSD BTX notice](FreeBSD-BTX-license.txt).

This boot image targets x86 BIOS with extended disk reads and VGA
or serial output. It uses fixed boot disk sectors. FreeBSD's full
loader, UEFI, and filesystem browsing are outside this port.

## Boot x86_64

If an x86_64 boot fails, the loader tries x86 in the same disk or live mode.

The ELF64 kernel uses four-level paging and 64-bit interrupt
and scheduler entry code. It shares the Project9 filesystem,
console, and device drivers with i386. ELF32 processes still use
the existing stack syscall ABI. Native ELF64 processes use the
register ABI described in [the multiarch guide](hybrid.md). Both
modes share the ext2 root and can run the existing i386 SDK output. The direct RAM map still
uses the original physical memory limit.

```sh
qemu-system-x86_64 -cpu qemu64 -m 192 -smp 2 -nographic -snapshot \
  -drive file=release/dev/install.img,format=raw
```

At the shell run `sysctl hw.machine` and `sysctl hw.ncpu`. The
x86_64 kernel reports `hw.machine=x86_64`. Run `arch` to see which
slice ran inside the process. The 32-bit kernel reports `hw.machine=i386`.

Use the disposable QEMU regression images for storage and USB tests.

```sh
python3 tools/storage-test.py --x86-64 --require-smp
python3 tools/storage-test.py --x86-64 --usb
```

The entry code adapts the transition from the supplied
[xv6 x86_64 port](xv6-x86_64-LICENSE.txt).

## Install disk

`install.img` places the BTX loader and kernel ahead of an aligned MBR
partition containing the ext2 root. The loader reads from the BIOS
boot drive. In persistent mode the kernel locates the matching
partition on ATA or AHCI storage. In live mode the loader copies the
partition into reserved RAM before entering the kernel.

See the [release and installation guide](release.md) for writing the
image to USB or disk and the [ext2 guide](ext2.md) for importing a previous `fs.img`.
