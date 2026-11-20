<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Project9 dev

Project9 is an x86 Unix project based on xv6. The i386 kernel boots to a
Gallant console with login, shell job control, and the UNIX V6 line
editor ed. The x86_64 kernel runs Project9 ELF64 executables and the
existing ELF32 userland. Every executable in the hybrid `/bin` directory
carries both an i386 and an x86_64 slice. See [the multiarch guide](Documentation/hybrid.md)
for the executable format and build layout.
The [boot guide](Documentation/boot.md) covers the FreeBSD BTX boot chain
and Project9 menu. The [terminal notes](Documentation/history/v0.4a.md) cover
termios, stty, and the pager viewport.

This is the development build for Project9 v1. The v1 label is reserved
for the stable release.

## Build and boot

From the extracted `project9` directory, use GNU Make, GCC with i386 code
generation, binutils with `elf_i386` support, a native C compiler, Perl, and
QEMU's x86 system emulator, Python 3.9 or later with curses, and
e2fsprogs for creating ext2 disks. The guest links its own runtime and needs no host
32-bit libc.

The hybrid boot image also needs a compiler with `-m64` and a linker with
`elf_x86_64` support. Use `make ARCH=i386` on an i386-only build
toolchain, or set `CC64` and `LD64` to x86_64-capable tools.

```sh
make os
```

Use the menu to configure the filesystem, QEMU, hostname, and accounts.
Run `make defconfig` for defaults and a single filesystem choice.
See the [setup guide](Documentation/setup.md). Choose **Start Project9** for a fresh installation or to
migrate an existing root disk. Build steps are labeled with their operation
and output file. Run `make V=1`
when you need the full tool commands and diagnostics.

At the boot menu, press Enter or wait three seconds. Set a root password
at the first login prompt. Enter it again
when asked, then sign in as `root` with that password. The image ships without
an account password. Use `useradd NAME` as root to create another account and
`passwd` to change a password. Type `exit` or press Ctrl+D on an empty shell
line to sign out.

The ext2 release includes Tiny-X. Run `startx` on the graphical console
for the FVWM desktop, `startx --fvwm xterm` to open xterm alongside FVWM,
or `startx xterm` for a standalone terminal. X11 commands are on the
default `PATH`. Run `tinyx --check` for the headless X11 test and see the
[desktop guide](Documentation/desktop.md) for session configuration.

For a serial-only session, use `make qemu-nox`. Use Ctrl+A then X to quit QEMU
from its serial monitor. Add `QEMU=/path/to/qemu-system-i386` or
`TOOLPREFIX=i686-elf-` to the Make command to select a toolchain explicitly.

## Try Project9 dev

Inside Project9, run `ls -l /dev`, `ls -alh /`, `ps | grep sh`,
or `seq 100 | less`. Press q to quit the pager. The `dev(5)` and
`console(5)` manual pages describe device reads and terminal sequences.

## Build a release

```sh
make release
cd release/dev-x86_64
sha256sum -c SHA256SUMS
```

The release includes a single BIOS-bootable `install.img`, base and SDK
sets, and the component images. See the [release and installation guide](Documentation/release.md).
Choose **Live in memory** at the loader to try Project9 without writing
to the media. On a supported ATA or SATA disk, **Boot Project9**
uses the ext2 root partition directly.

| Command | Purpose |
| --- | --- |
| `make menuconfig` | Change build and machine settings |
| `make defconfig` | Choose a filesystem and save the default settings |
| `make os` | Configure accounts, import a previous installation, build or boot |
| `make qemu` | Start QEMU with the local install image |
| `make libc` | Build the i386 C library and SDK |
| `make qemu-libc-test` | Boot the SDK test image |
| `make fsck` | Check the root partition of the local install image offline |

To keep accounts and files, stop the old guest and copy its `fs.img`
into the extracted source directory. **Start Project9** imports it to
`build/x86_64/install.img`. Shut down the guest with `poweroff` or restart it with
`reboot`.

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
