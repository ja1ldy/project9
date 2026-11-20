<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Project9 multiarch

The default `make ARCH=x86_64 release` builds both kernels, a shared
ext2 root, and the i386 SDK. It writes the bootable image to
`release/dev-x86_64/install.img`. Boot options **1** and **5** select
i386. Options **6** and **7** select x86_64. The live modes are **5**
and **7**.

`make ARCH=i386 release` produces i386 kernels and programs in
`build/i386/` with its own release in `release/dev-i386/`. Neither
profile writes the other profile’s build outputs, config, or local
disk images. Use `make ARCH=i386` on an i386-only toolchain. The
x86_64 profile can take `CC64`, `LD64`, and `OBJCOPY64` overrides.

| Paths | Architecture | Purpose |
| --- | --- | --- |
| `usr/bin/*.c`, `usr/shell/*.c`, `usr/tests/*.c`, `usr/init/init.c` | Shared C | Commands, shell, guest tests, and init |
| `fs/`, `kernel/`, `mm/` | Shared C with architecture-specific paths | Filesystems, processes, syscalls, paging |
| `drivers/` | Shared by i386 and x86_64 | PC device drivers for storage, USB, console, and input |
| `include/p9abi.h`, `include/p9records.h`, `rootfs/` | Shared | Fixed-width syscall records and filesystem contents |
| `boot/pc/` | x86 BIOS | Shared 32-bit BTX loader that loads either kernel |
| `arch/x86/kernel/` | x86 | Shared hardware C and i386 assembly |
| `arch/x86_64/` | x86_64 | Long-mode entry, secondary CPU startup, traps, and context switching |
| `usr/lib/start.S`, `usr/lib/setjmp.S`, `usr/lib/user.ld` | i386 | ELF32 startup, nonlocal jumps, and link layout |
| `usr/arch/x86_64/` | x86_64 | ELF64 startup, signal return, nonlocal jumps, and link layout |
| `usr/lib/usys.S` | Both x86 ABIs | Shared interrupt-based syscall stubs |
| `tools/fatpack.py`, `include/fat.h` | Shared file format | Universal executable packer and on-disk header |
| `build/x86_64/native/`, `build/x86_64/compat-i386/` | Separate artifacts | ELF64 objects and binaries, and i386 objects, SDK, and root staging |
| `build/x86_64/fat/` | Both user ABIs | Packed executables installed in the hybrid image |
| `build/i386/` | i386 | Standalone build artifacts, SDK, root staging, and images |

## Universal executables

A Project9 universal executable starts with `P9FB` and version
`1`. The 24-byte little-endian header contains offset and length
pairs for one complete ELF32 image followed by one complete ELF64
image. Slice offsets are aligned to 16 bytes. The loader bounds
each ELF header, program header table, and segment against its
slice before loading it. A damaged selected slice fails
`exec`.

The x86_64 kernel picks the ELF64 slice. The i386 kernel picks the ELF32
slice. Either path needs the loader from this release to open a P9FB file. Thin Project9 ELF32 executables also run on both kernels. On
x86_64 they run in CPU compatibility mode and keep the original
stack-based interrupt syscall ABI. Native ELF64 uses the same syscall
numbers and interrupt vector with arguments in rdi, rsi, rdx, rcx, r8
and r9. Startup receives an aligned stack with argc, argv and envp.
Native processes currently use the same low user virtual address
range and fixed-width syscall structures as ELF32.

The hybrid base set ships every regular `/bin` executable with both
slices. This includes `init`, `login`, `sh`, `ed`, `man`, the installer
utilities, and the bundled regression tests. `[` is a copy of `test`.
`tar` and `poweroff` resolve to the universal `pax` and `halt`.
The build derives its command list from the same sources for both
architectures. The release check requires both stripped slices in every
regular `/bin` entry and checks each slice against the 64 MiB ext2 release limit.
The standalone i386 release ships thin ELF32 programs.

Shell scripts, manuals, and configuration files are shared as-is.
Native commands link statically with the Project9 runtime in `usr/lib/`
and `usr/arch/x86_64/`. The NetBSD-based SDK and its separate test programs
use the i386 ABI and run under IA32 compatibility on the x86_64 kernel.
Core files still use the existing 32-bit register record.

```sh
make ARCH=x86_64 release
make ARCH=i386 release
python3 tools/fatpack.py mytool mytool.i386 mytool.x86_64
```

Boot the same x86_64 release image using **5** and **7**. Run `arch`
to see the slice selected for the process and `sysctl hw.machine`
to see the running kernel. `ls -lah /` exercises directory reads,
metadata, and formatted output on both paths.
