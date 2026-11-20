<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Project9 dev installer test

The release image has a RAM-live installer at `/install`. It reads the
installation sets from `/sets`, verifies their SHA-256 checksums, prepares
an ext2 partition, extracts the base system and SDK, and makes the target
disk bootable. The checksums detect damaged files and depend on a trusted
release image.

## Test in QEMU

From the source directory, run these commands. The second command creates
a new disposable image and fails if the path already exists.

```sh
(cd release/dev-x86_64 && sha256sum -c SHA256SUMS)
python3 tools/install-target.py test-target.img
qemu-system-i386 -m 128 -display none -serial mon:stdio \
  -drive file=release/dev-x86_64/install.img,index=0,format=raw,snapshot=on \
  -drive file=test-target.img,index=1,format=raw
```

Press **5** at the boot menu. Set the temporary live-session root password
when prompted, sign in, and run these commands in Project9.

```sh
sysctl hw.disknames
sysctl hw.bootdisk
/install
```

Enter `hd1` when it is marked `test-target`. Type `yes` to proceed. After
the success message, enter `h` to halt. Boot the installed disk by itself
to test persistence.

```sh
qemu-system-i386 -m 128 -display none -serial mon:stdio \
  -drive file=test-target.img,index=0,format=raw
```

Press **1**, set the installed system's root password, sign in, and run
`usertests`. Run `poweroff` to shut down. The installed system has its
own fresh account database.

## Commands and boundaries

| Command | Test release behavior |
| --- | --- |
| `sysctl hw.disknames` | Lists ATA and AHCI disks, identifying the boot disk, active volumes, and test targets when available |
| `sysctl hw.bootdisk` | Reports the native disk name or the BIOS drive number |
| `fdisk -iy hd1` | Writes one aligned Linux-type MBR partition to the tagged test disk |
| `newfs hd1p1` | Creates a single-group ext2 partition with 4096-byte blocks |
| `fsck hd1p1` | Checks this ext2 layout offline, including inode links, directory ancestry, and block ownership |
| `fsck -y hd1p1` | Rebuilds the bitmaps and free counts and marks the volume clean when the inode and directory checks pass |
| `mount hd1p1 /mnt`, `umount /mnt` | Mounts the installer partition through VFS and refuses to unmount while it is in use |
| `pax -rzf archive.tar.gz -C /mnt` | Extracts Project9 gzip-compressed USTAR sets with their modes. Use `/mnt/usr` for the SDK. `tar -xzf` uses the same implementation |
| `sha256 -C SHA256SUMS` | Checks tagged BSD or untagged digest lists with hexadecimal or Base64 digests |
| `installboot /sets/xv6.img hd1` | Copies the BIOS, BTX, and kernel payload, checks the target superblock, and commits the boot record last |
| `sync`, `reboot`, `halt`, `poweroff` | Synchronizes filesystems and uses the platform reset or shutdown path |

Installer disk names are kernel inventory aliases. The utilities accept
`hd1p1`, `/dev/hd1a`, and the equivalent SATA spelling. The kernel checks
root credentials, requires a RAM-live session for raw operations, and
checks every write against the allowed sector range. The boot disk, active
volumes, and disks without the disposable marker are protected. A mounted
target can be changed through VFS, and raw writes resume after unmount.
The marker enables VM testing. Native USB mass storage and installation
to physical disks remain outside this test release. BIOS USB boot into
RAM remains available where the system firmware supports it.
