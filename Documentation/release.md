<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Building Project9 dev releases

From the source directory, run `make release`. The build stages the base
tree in `build/x86_64/compat-i386/rootfs/`, builds the i386 SDK, and writes
one x86 and x86_64 release to `release/dev/`.
Use `RELEASEDIR=path/to/directory` for a different output directory.
Older architecture-specific output directories can be moved out of
`release/` once you have kept any guest data you need.

| File | Contents |
| --- | --- |
| `base.tar.gz` | Stripped universal executables, system configuration, manuals, and empty directories |
| `sdk.tar.gz` | Headers, libc.a, libm.a, crt0.o, and the linker script |
| `install.img` | BIOS disk with both kernels, the shared ext2 filesystem, and the RAM-live installer |
| `SHA256SUMS` | Checksums of the image and both archives |

The lists in `tools/sets/` specify each path and whether it is a
directory, file, or approved symbolic link. Missing and unexpected paths fail the build.
Each regular `/bin` or `/usr/bin` executable needs a stripped ELF32 and ELF64
slice. Each slice must fit in 64 MiB. The base set also includes the Tiny-X ELF32 server
under `/usr/X11/bin`. Update the manifest after adding files.

The tar sets use sorted paths, normalized ownership and modes, and
`SOURCE_DATE_EPOCH` when provided. Identical staged contents produce
identical archives. The release filesystem uses fresh
ext2 identifiers and an account-database seed.

```sh
cd release/dev
sha256sum -c SHA256SUMS
```

## Live boot and installation

Use `install.img` from this release for either QEMU or a physical PC.
Copy the release image before using it as a writable VM disk.

```sh
cp release/dev/install.img install.img
qemu-system-x86_64 -m 192 -serial mon:stdio -drive file=install.img,format=raw
```

Choose a **Disk** entry for a writable disk session. Choose a **Live**
entry to load the ext2 partition into RAM. The
[boot guide](boot.md) lists the keys for 32-bit and 64-bit boot.
In live mode the BIOS reads the partition before the kernel starts.
System changes then last for the session. Live mode accepts an ext2
partition up to 64 MiB and needs contiguous memory for the image plus
the kernel and working memory.

On a physical PC, use a disk imaging tool to write `install.img` as a
raw image to the whole boot device. This overwrites the selected device.
Select the device in the firmware boot menu with legacy BIOS boot
enabled. For USB media, choose a **Live** entry. A supported ATA or
AHCI disk can also use a **Disk** entry for persistent storage.

For the in-guest test installer and its disposable disk workflow, see
[Project9 dev installer test](installer.md).
