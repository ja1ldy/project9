<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Source and guest filesystem

The source tree groups code by subsystem, following the conventions of a
Linux kernel source tree where they suit this small xv6 system.

| Source path | Contents |
| --- | --- |
| `boot/pc/` | BIOS boot sector, FreeBSD BTX runtime, Project9 loader client and artwork |
| `arch/x86/kernel/` | x86 entry, traps, interrupt controllers, SMP and kernel linker script |
| `drivers/tty/` | Keyboard, UART, console input, VGA framebuffer and Gallant bitmap |
| `drivers/input/` | PS/2 input events and the CMOS clock |
| `drivers/block/` | Polled ATA block I/O |
| `fs/` | Ext2, inode cache, pipes, file syscalls and executable loading |
| `kernel/` | Process groups, scheduling, signals, account services and syscall dispatch |
| `mm/` | Physical allocation, page tables and user memory |
| `include/` | Kernel headers and shared user/kernel interfaces |
| `usr/init/`, `usr/bin/`, `usr/lib/`, `usr/tests/` | Init, commands, user runtime and xv6 tests |
| `usr/shell/` | Shell parser, evaluator and variable expansion |
| `tools/` | Host setup wizard, image builder and offline converter, interrupt-vector generation and debugger config |
| `rootfs/` | Guest directory template and static files |
| `Documentation/` | Host documentation, license texts and historical notes |

## From source to guest disk

`rootfs/` is the directory template. Empty directories have a `.keep`
file so they survive a source checkout. Run `make` to assemble
`build/x86_64/compat-i386/rootfs/` with the compiled binaries and static files. The build
omits `.keep` files from the staged tree.

`build/x86_64/compat-i386/rootfs/bin/` contains the universal executables that go
into the hybrid guest's `/bin`. The standalone i386 profile stages ELF32 executables. The image builder copies the staged directory
hierarchy into `build/x86_64/compat-i386/fs.factory.img` and initializes the protected
account database there. Init creates `/dev/console` on first boot.

The VM can use `build/x86_64/install.img` as the local writable guest disk. The default shell
`PATH` is `/bin`. If root runs `rm /bin/ls`, `ls` stops resolving on
that disk and stays absent across a reboot. For a reversible test, move
it to `/tmp/ls.saved` first and move it back afterward. A fresh factory
image includes the staged `ls` again. A normal source build preserves
the contents of the existing guest filesystem. A fresh release includes the
current staged files.

| Guest path | Contents |
| --- | --- |
| `/bin` | Shell, editor, coreutils and bundled tests |
| `/dev` | Console and other device nodes, created by init |
| `/etc` | Hostname, message of the day and kernel-only account database |
| `/home/NAME` | Private per-user directory created by `useradd` |
| `/root` | Root's private directory |
| `/tmp` | Shared working directory |
| `/usr/share/man` | Guest manual pages |
| `/var` | Reserved system directory |
| `/README` | Historical xv6 README retained on the guest disk |

Ext2 is the root filesystem. Path components can be up to 255 bytes.
The kernel exposes numeric UID, GID, mode and device information.
An optional second ext2 volume mounts at `/ext2`. See the
[ext2 guide](ext2.md) for disk setup and migration.

## CPU privilege

The kernel runs at Ring 0. Guest processes, including the root shell,
run at Ring 3. They enter the kernel through the system call gate. The
root account grants filesystem and process permissions inside Project9.
