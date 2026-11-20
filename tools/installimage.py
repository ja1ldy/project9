# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

import argparse
import fcntl
import os
from pathlib import Path
import shutil
import stat
import struct
import subprocess
import tempfile
import zlib

SECTOR = 512
ALIGNMENT = 2048


def create(output, boot, root, *, replace=True):
    boot_size, root_size = boot.stat().st_size, root.stat().st_size
    first = (boot_size + ALIGNMENT * SECTOR - 1) // (ALIGNMENT * SECTOR) * ALIGNMENT
    count = root_size // SECTOR
    if boot_size < SECTOR or root_size < 2048 or root_size % SECTOR or first + count >= 2**32:
        raise ValueError("boot or ext2 image does not fit the install disk")
    with root.open("rb") as source:
        source.seek(1024)
        superblock = source.read(120)
    if superblock[56:58] != b"\x53\xef":
        raise ValueError("install disk requires an ext2 root")
    tag = zlib.crc32(superblock[104:120])
    with tempfile.NamedTemporaryFile(prefix=".install-", dir=output.parent, delete=False) as target:
        scratch = Path(target.name)
        try:
            with boot.open("rb") as source:
                shutil.copyfileobj(source, target)
            target.seek(440)
            target.write(struct.pack("<IH", tag, 0x3950))
            target.write(struct.pack("<B3sB3sII", 0x80, b"\xfe\xff\xff",
                                     0x83, b"\xfe\xff\xff", first, count))
            target.seek(first * SECTOR)
            with root.open("rb") as source:
                while data := source.read(1024 * 1024):
                    if data.count(0) == len(data):
                        target.seek(len(data), os.SEEK_CUR)
                    else:
                        target.write(data)
            target.truncate()
            target.flush()
            os.fsync(target.fileno())
            if replace:
                os.replace(scratch, output)
            else:
                os.link(scratch, output)
        finally:
            scratch.unlink(missing_ok=True)


def partition(image):
    with image.open("rb") as source:
        mbr = source.read(SECTOR)
    if len(mbr) != SECTOR or mbr[444:446] != b"\x50\x39":
        return None
    first, count = struct.unpack_from("<II", mbr, 454)
    if (mbr[510:512] != b"\x55\xaa" or mbr[446] != 0x80 or
        mbr[450] != 0x83 or any(mbr[462:510]) or first < ALIGNMENT or count < 4 or
        (first + count) * SECTOR > image.stat().st_size):
        raise ValueError(f"invalid Project9 partition table in {image}")
    return first * SECTOR, count * SECTOR


def extract(image, output):
    offset, remaining = partition(image)
    with image.open("rb") as source, output.open("xb") as target:
        source.seek(offset)
        while remaining:
            data = source.read(min(remaining, 1024 * 1024))
            if not data:
                raise ValueError(f"truncated Project9 root partition in {image}")
            if data.count(0) == len(data):
                target.seek(len(data), os.SEEK_CUR)
            else:
                target.write(data)
            remaining -= len(data)
        target.truncate()


def check(image, e2fsck):
    if partition(image) is None:
        subprocess.run([e2fsck, "-fn", image], check=True)
        return
    with tempfile.TemporaryDirectory(prefix=".check-", dir=image.resolve().parent) as tmp:
        root = Path(tmp) / "root.img"
        extract(image, root)
        subprocess.run([e2fsck, "-fn", root], check=True)


def recover(image, output, e2fsck):
    # the source fd is used only for reading and to claim an offline lock
    fd = os.open(image, os.O_RDWR | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(fd, "r+b") as source:
        if not stat.S_ISREG(os.fstat(source.fileno()).st_mode):
            raise ValueError("recovery requires a regular disk image")
        try:
            fcntl.lockf(source, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            raise ValueError(f"stop the guest before recovering {image}") from error
        if os.path.lexists(output):
            raise FileExistsError(f"{output} already exists")
        with tempfile.TemporaryDirectory(prefix=".recover-", dir=output.parent) as tmp:
            work = Path(tmp)
            copy = work / "disk.img"
            with copy.open("xb") as target:
                shutil.copyfileobj(source, target)
            bounds = partition(copy)
            root = work / "root.img" if bounds else copy
            if bounds:
                extract(copy, root)
            status = subprocess.run([e2fsck, "-pf", root]).returncode
            if status not in (0, 1, 2, 3):
                raise ValueError("automatic repair did not complete use e2fsck on an offline copy")
            subprocess.run([e2fsck, "-fn", root], check=True)
            with root.open("rb") as repaired:
                repaired.seek(1024)
                superblock = repaired.read(120)
                if superblock[56:60] != b"\x53\xef\x01\x00":
                    raise ValueError("repaired filesystem is not clean ext2")
                if bounds:
                    offset, length = bounds
                    if root.stat().st_size != length:
                        raise ValueError("repair changed the partition size")
                    repaired.seek(0)
                    with copy.open("r+b") as target:
                        target.seek(440)
                        if target.read(4) != struct.pack("<I", zlib.crc32(superblock[104:120])):
                            raise ValueError("repaired filesystem does not match the boot identity")
                        target.seek(offset)
                        shutil.copyfileobj(repaired, target)
            with copy.open("rb") as target:
                os.fsync(target.fileno())
            os.link(copy, output)
        directory = os.open(output.parent, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(directory)
        finally:
            os.close(directory)


def refresh(image, boot):
    if not image.is_file() or image.is_symlink():
        raise ValueError("refresh requires a regular Project9 install image")
    with image.open("r+b") as locked:
        try:
            fcntl.lockf(locked, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            raise ValueError(f"stop the guest before refreshing {image}") from error
        if partition(image) is None:
            raise ValueError(f"{image} has no Project9 root partition")
        with tempfile.TemporaryDirectory(prefix=".refresh-", dir=image.resolve().parent) as tmp:
            root = Path(tmp) / "root.img"
            extract(image, root)
            create(image, boot, root)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    action = p.add_mutually_exclusive_group()
    action.add_argument("--check", action="store_true")
    action.add_argument("--refresh", action="store_true")
    action.add_argument("--recover-to", type=Path, metavar="NEW_IMAGE")
    p.add_argument("--e2fsck", default="e2fsck")
    p.add_argument("output", type=Path)
    p.add_argument("boot", type=Path, nargs="?")
    p.add_argument("root", type=Path, nargs="?")
    args = p.parse_args()
    try:
        if args.recover_to and not args.boot:
            recover(args.output, args.recover_to, args.e2fsck)
        elif args.check and not args.boot:
            check(args.output, args.e2fsck)
        elif args.refresh and args.boot and not args.root:
            refresh(args.output, args.boot)
        elif not (args.check or args.refresh or args.recover_to) and args.boot and args.root:
            create(args.output, args.boot, args.root)
        else:
            p.error("use OUTPUT BOOT ROOT, --check IMAGE, --refresh IMAGE BOOT or --recover-to NEW_IMAGE IMAGE")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        p.exit(1, f"project9: {error}\n")
