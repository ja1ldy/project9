<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Building Project9 dev releases

From the source directory, run `make release`. The build stages the base
tree in `build/x86_64/compat-i386/rootfs/`, builds the i386 SDK, creates a fresh ext2
factory image as needed, and writes the release to `release/dev-x86_64/`.
Use `RELEASEDIR=path/to/directory` for a different output directory.

| File | Contents |
| --- | --- |
| `base.tar.gz` | Stripped universal executables, system configuration, manuals, and empty directories |
| `sdk.tar.gz` | Headers, libc.a, libm.a, crt0.o, and the linker script |
| `install.img` | Single BIOS boot disk with the BTX loader, installer sets, and the RAM-live installation script |
| `xv6.img` | BTX and kernel component for custom repacking and development |
| `fs.factory.img` | Clean ext2 root component with no account password set |
| `SHA256SUMS` | Checksums of the five distribution files |

The lists in `tools/sets/` specify each path and whether it is a
directory, file, or approved symbolic link. Missing and unexpected paths fail the build.
Each regular `/bin` file in the hybrid base set needs a stripped ELF32 and ELF64
slice. Each slice must fit in 64 MiB. The standalone i386 base set uses stripped
ELF32 files. The ext2 base set also includes the Tiny-X ELF32 server
under `/usr/X11/bin`. Update the manifest after adding files.

The tar sets use sorted paths, normalized ownership and modes, and
`SOURCE_DATE_EPOCH` when provided. Identical staged contents produce
identical archives. A newly generated factory filesystem uses fresh
ext2 identifiers and an account-database seed.

```sh
cd release/dev-x86_64
sha256sum -c SHA256SUMS
```

## Live boot and installation

Copy the release image before using it as a writable VM disk.

```sh
cp release/dev-x86_64/install.img install.img
qemu-system-x86_64 -m 192 -serial mon:stdio -drive file=install.img,format=raw
```

Choose **Boot Project9** for a writable disk session. Choose
**Live in memory** or press **5** to load the ext2 partition into RAM.
In live mode the BIOS reads the partition before the kernel starts.
System changes then last for the session. Live mode accepts an ext2
partition up to 64 MiB and needs contiguous memory for the image plus
the kernel and working memory.

For the in-guest test installer and its disposable disk workflow, see
[Project9 dev installer test](installer.md).
