<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Project9 libc with NetBSD sources

`make libc` builds an i386 SDK in `build/x86_64/compat-i386/sdk`. It contains `libc.a`,
`libm.a`, the public headers, `crt0.o`, and the linker script. Build an
application with the same GCC and binutils prefix used to build Project9.

```sh
make libc
gcc -m32 -march=i486 -ffreestanding -fno-pie -fno-stack-protector \
    -ffunction-sections -fdata-sections -Ibuild/x86_64/compat-i386/sdk/include \
    -c hello.c -o hello.o
ld -m elf_i386 -N -s --gc-sections -T build/x86_64/compat-i386/sdk/user.ld -o hello \
    build/x86_64/compat-i386/sdk/crt0.o hello.o build/x86_64/compat-i386/sdk/libc.a
```

`libc.a` includes the math routines for compatibility with the earlier
SDK. You can also pass `build/x86_64/compat-i386/sdk/libm.a` before `build/x86_64/compat-i386/sdk/libc.a`.
Strip guest executables before staging them. An individual file in the
legacy xv6 filesystem can hold up to 70 KiB. The ext2 profile supports
much larger executables and uses a 64 MiB release limit per slice.

## Sources

The port uses the supplied NetBSD snapshot
`f3f4e88ca399c55d8464a3d615f116f9496a526f`. Imported source lives in
`usr/libc/vendor/netbsd`. The original notices accompany that source
and are collected in [NetBSD-LICENSES.txt](NetBSD-LICENSES.txt).

| Area | Implementation |
| --- | --- |
| Strings and sorting | NetBSD string and memory routines, `qsort`, `qsort_r`, `strtol` and `strtoul` |
| Streams | NetBSD buffering, stream creation, seeking, character and block I/O |
| Regex | NetBSD POSIX basic and extended regex with C-locale character classes |
| Numbers | NetBSD gdtoa for `strtod` and decimal digits in the Project9 `printf` formatter |
| Time | NetBSD `strftime` and `asctime` with Project9 UTC calendar conversion |
| Math | NetBSD fdlibm trigonometry and i387 assembly for selected routines |

The four adapted upstream files are `stdio/flags.c`, `stdio/fdopen.c`,
`stdio/makebuf.c`, and `time/strftime.c`. They use Project9's available
open flags and time range. The stream API supports the usual
`r`, `w`, `a`, `+`, `b`, and `x` modes. The SDK compiler sees its
own headers and the GCC intrinsic headers. Compatibility headers supply
the NetBSD compiler macros, C locale, and single-threaded stream hooks.

## i386 kernel ABI

`usr/libc/crt0.S` reads argc, argv and envp, initializes environ, and
calls main. Returning from main runs registered exit handlers and
flushes open output streams before invoking the kernel exit syscall.
`usr/lib/setjmp.S` supplies setjmp and longjmp for the same ABI.

The assembly gateway in `usr/lib/usys.S` passes the syscall number in
EAX and uses `int $0x40`. The kernel reads syscall arguments from the
i386 user stack. C wrappers convert negative results into public return
values and errno. Legacy calls that return a bare `-1` use a
wrapper-specific errno.

`<sys/syscall.h>` exposes the kernel's syscall numbers.
`<project9/p9records.h>` supplies the fixed i386 stat, statx and directory
records used by the kernel and SDK. Public `stat` and `readdir`
translate these records into the application-facing structures.
`termios.h`, `fcntl.h`, and the errno numbers come from shared native
headers.

The runtime targets static i386 applications with the C locale and UTC
calendar time. Time uses the signed 32-bit Project9 ABI. The allocator
uses sbrk. mmap provides anonymous, private file and framebuffer mappings.
The SDK also exposes local stream sockets, poll, select and openpty.
See the [desktop guide](desktop.md) for the supported kernel interfaces.

## Run the guest tests

On the host, run

```sh
make qemu-libc-test
```

The target builds and boots `build/x86_64/compat-i386/libc-test.img`. The test disk has its
own root password. Set it on the first boot, sign in as root, and run

```sh
libctest
netbsdtest
```

Expect `LIBC TESTS PASSED` and `NETBSD LIBC TESTS PASSED`. The first
suite checks the existing kernel and libc interfaces, including the
framebuffer. The second checks the imported routines, including regex
captures, decimal rounding, leap dates and text formatting. Use the
default QEMU display device for the framebuffer check. `make libc-test`
builds just the images. `make qemu` boots the regular `fs.img`.
