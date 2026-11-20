<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Project9 dev

Project9 is an x86 Unix project based on xv6. The i386 kernel boots to a
Gallant console with login, shell job control, and the UNIX V6 line
editor ed. The x86_64 kernel runs Project9 ELF64 executables and the
existing ELF32 userland. Executables in the hybrid `/bin` and `/usr/bin`
carry both an i386 and an x86_64 slice. See [the multiarch guide](Documentation/hybrid.md)
for the executable format and build layout.
The [boot guide](Documentation/boot.md) covers the FreeBSD BTX boot chain
and Project9 menu. The [terminal notes](Documentation/history/v0.4a.md) cover
termios, stty, and the pager viewport.

This is the development build for Project9 v1. The v1 label is reserved
for the stable release.

## Build and boot

Builds automatically use `ccache` when it is installed and visible on the
host `PATH`. On Alpine Linux run `apk add ccache`. Compilation results are
cached outside the project directory, so `make clean` does not discard the
cache. Set `CCACHE=` to disable caching for a particular invocation.


From the source directory, use GNU Make, GCC with i386 code generation,
binutils with `elf_i386` support, a native C compiler, Perl, Python 3.9 or
later, and e2fsprogs with `mke2fs`, `debugfs`, and `e2fsck`. The guest links
its own runtime and needs no host 32-bit libc. Use QEMU to boot the image
in a virtual machine.

The hybrid boot image also needs a compiler with `-m64` and a linker with
`elf_x86_64` support. Use `make ARCH=i386` on an i386-only build
toolchain, or set `CC64` and `LD64` to x86_64-capable tools.

```sh
make
qemu-system-x86_64 -m 512 -smp 2 -serial mon:stdio \
  -drive file=build/x86_64/install.img,format=raw
```

For a serial-only session, add `-display none` to the QEMU command.
Press Ctrl+A then X to quit from QEMU's serial monitor. Run `make V=1`
to see the full build commands. The [setup guide](Documentation/setup.md)
covers host settings and importing an existing disk.

At the boot menu, press **1** for 32-bit x86 or **3** for x86_64.
Enter and the three-second countdown select x86_64 when available.
Set a root password at the first login prompt. Enter it again
when asked, then sign in as `root` with that password. The image ships without
an account password. Use `useradd NAME` as root to create another account and
`passwd` to change a password. Type `exit` or press Ctrl+D on an empty shell
line to sign out.

The ext2 release includes Tiny-X. Run `startx` on the graphical console
for the FVWM desktop, `startx --fvwm xterm` to open xterm alongside FVWM,
or `startx xterm` for a standalone terminal. X11 commands are on the
default `PATH`. Run `tinyx --check` for the headless X11 test and see the
[desktop guide](Documentation/desktop.md) for session configuration.
[Interactive checks](Documentation/testing.md#check-terminal-and-window-responsiveness)
cover held Enter and window dragging.

## Try Project9 dev

Inside Project9, run `ls -l /dev`, `ls -alh /`, `ps | grep sh`,
or `seq 100 | less`. Press q to quit the pager. The `dev(5)` and
`console(5)` manual pages describe device reads and terminal sequences.

## Build a release

```sh
make release
cd release/dev
sha256sum -c SHA256SUMS
```

Use `release/dev/install.img` for both x86 and x86_64 in QEMU or on
supported BIOS PCs. Choose a **Live** entry for a BIOS-readable USB
drive, or a **Disk** entry for a supported ATA or SATA disk.
See the [release guide](Documentation/release.md) for both workflows.

| Command | Purpose |
| --- | --- |
| `make` | Build the local boot disk and SDK |
| `make release` | Build fresh BIOS media, distribution sets, and checksums |
| `make clean` | Remove intermediates for the selected build profile and keep local disks and release files |
| `make full-clean` | Remove `build/` and `release/`, including local guest disks and settings stored there |

Back up any guest data in `build/` before running `make full-clean`.
Shut down the guest with `poweroff` or restart it with `reboot`.

## Documentation

- [Release sets, live boot and installation](Documentation/release.md)
- [Installer test and disk protections](Documentation/installer.md)
- [Ext2 root and data volumes](Documentation/ext2.md)

- [NetBSD libc port and i386 SDK](Documentation/libc.md)
- [Stability, filesystem checking and crash diagnostics](Documentation/stability.md)
- [Hardware support and device interfaces](Documentation/hardware.md)
- [Multiarch build and universal executables](Documentation/hybrid.md)
- [BIOS boot menu and disk layout](Documentation/boot.md)
- [Terminal release notes](Documentation/history/v0.4a.md)
- [Manual pages and pager](Documentation/man.md)
- [Shell and userland release notes](Documentation/history/v0.3.md)
- [Source and filesystem layout](Documentation/layout.md)
- [TTY, job control, framebuffer and core dumps](Documentation/tty.md)
- [Accounts, permissions and passwords](Documentation/accounts.md)
- [Coreutils and shell](Documentation/userland.md)
- [UNIX V6 ed](Documentation/ed.md)
- [Validation and smoke tests](Documentation/testing.md)

Project9 contributions use the ISC license. See [LICENSE](LICENSE) for
its scope and the accompanying upstream terms.

This product includes software developed or owned by Caldera International, Inc.
