# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

import argparse
import gzip
import hashlib
import os
from pathlib import Path
import shutil
import struct
import tarfile
import tempfile

from fatpack import ALIGN, HEADER
from installimage import create
from osimage import build_image, root_executable


LINKS = {"bin/tar": "pax", "bin/poweroff": "halt"}


def manifest(stage, listed):
    expected = {}
    for line in listed.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        kind, name = line.split(" ", 1)
        if kind not in ("d", "f", "l") or name in expected:
            raise ValueError(f"invalid set entry in {listed}: {line}")
        expected[name] = kind

    actual = {}
    for path in stage.rglob("*"):
        name = path.relative_to(stage).as_posix()
        actual[name] = ("l" if path.is_symlink() and LINKS.get(name) == os.readlink(path)
                        else "?" if path.is_symlink() else "d" if path.is_dir()
                        else "f" if path.is_file() else "?")
    missing = sorted(expected.keys() - actual.keys())
    extra = sorted(actual.keys() - expected.keys())
    different = sorted(name for name in expected.keys() & actual.keys()
                       if expected[name] != actual[name])
    if missing or extra or different:
        raise ValueError(f"{listed} missing {missing} extra {extra} wrong type {different}")
    return expected


def verify_elf(path, data, bits, limit=64 * 1024 * 1024):
    if len(data) > limit:
        raise ValueError(f"{path} ELF{bits} slice exceeds {limit // 1024} KiB")
    index, machine = (1, 3) if bits == 32 else (2, 62)
    if (len(data) < (52 if bits == 32 else 64) or data[:7] != b"\x7fELF" + bytes((index, 1, 1))
            or struct.unpack_from("<HHI", data, 16) != (2, machine, 1)):
        raise ValueError(f"{path} must contain a valid ELF{bits} executable")
    section, = struct.unpack_from("<I" if bits == 32 else "<Q", data, 32 if bits == 32 else 40)
    size, count = struct.unpack_from("<HH", data, 46 if bits == 32 else 58)
    if not count:
        return
    if size != (40 if bits == 32 else 64) or section + size * count > len(data):
        raise ValueError(f"{path} has invalid ELF sections")
    for i in range(count):
        if struct.unpack_from("<I", data, section + i * size + 4)[0] == 2:
            raise ValueError(f"{path} contains a symbol table")


def verify_program(path):
    data = path.read_bytes()
    if not data.startswith(b"P9FB"):
        raise ValueError(f"{path} must contain both ELF32 and ELF64 slices")
    if len(data) < HEADER.size:
        raise ValueError(f"{path} has a short fat header")
    _, version, start32, len32, start64, len64 = HEADER.unpack_from(data)
    if (version != 1 or start32 < HEADER.size or start32 % ALIGN or start64 % ALIGN
            or start32 + len32 > start64 or start64 + len64 > len(data)):
        raise ValueError(f"{path} has invalid fat slices")
    verify_elf(path, data[start32:start32+len32], 32)
    verify_elf(path, data[start64:start64+len64], 64)


def package(stage, listing, output, base, epoch):
    with output.open("wb") as stream, gzip.GzipFile(filename="", mode="wb", fileobj=stream, mtime=0) as packed:
        with tarfile.open(fileobj=packed, mode="w", format=tarfile.USTAR_FORMAT) as archive:
            for name, kind in sorted(listing.items()):
                file = stage / name
                if base and kind == "f" and name.startswith(("bin/", "usr/bin/")):
                    verify_program(file)
                elif base and kind == "f" and root_executable(name):
                    data = file.read_bytes()
                    if name == "usr/X11/bin/startx":
                        if not data.startswith(b"#!/bin/sh\n"):
                            raise ValueError(f"{file} is not a Project9 shell script")
                    else:
                        verify_elf(file, data, 32)
                info = tarfile.TarInfo("./" + name)
                info.mtime = epoch
                if kind == "d":
                    info.type = tarfile.DIRTYPE
                    info.mode = {"tmp": 0o1777, "root": 0o700}.get(name, 0o755)
                    archive.addfile(info)
                elif kind == "l":
                    info.type, info.mode, info.linkname = tarfile.SYMTYPE, 0o777, LINKS[name]
                    archive.addfile(info)
                else:
                    info.mode = 0o755 if base and root_executable(name) else 0o644
                    info.size = file.stat().st_size
                    with file.open("rb") as content:
                        archive.addfile(info, content)


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        while data := stream.read(1024 * 1024):
            h.update(data)
    return h.hexdigest()


def verify_boot(boot):
    with boot.open("rb") as disk:
        disk.seek(127 * 512)
        header = disk.read(20)
        if len(header) < 20:
            raise ValueError(f"{boot} has no boot payload header")
        magic, size, start64, size64, size32 = struct.unpack("<5I", header)
        if (magic != 0x4B395050 or not 52 <= size32 <= start64 or start64 % 512
                or size64 < 64 or start64 + size64 > size
                or 128 * 512 + size > boot.stat().st_size):
            raise ValueError(f"{boot} must contain both kernel payloads")
        for start, kind in ((0, b"\x01"), (start64, b"\x02")):
            disk.seek(128 * 512 + start)
            if disk.read(5) != b"\x7fELF" + kind:
                raise ValueError(f"{boot} has an invalid kernel payload")


def generate(where, stage, boot, mkfs, fsck, debugfs):
    if where.resolve() == Path.cwd() or not where.name:
        raise ValueError("choose a dedicated release directory")
    where.mkdir(parents=True, exist_ok=True)
    epoch = int(os.environ.get("SOURCE_DATE_EPOCH", "0"))
    with tempfile.TemporaryDirectory(prefix=".release-", dir=where) as temporary:
        work = Path(temporary)
        for name, prefix in (("base", "rootfs"), ("sdk", "sdk")):
            tree = stage / prefix
            expected = manifest(tree, Path("tools/sets") / (name + ".list"))
            if name == "sdk":
                expected = {path: kind for path, kind in expected.items()
                            if path != "include/.ready" and not path.endswith(".d")}
            package(tree, expected, work / (name + ".tar.gz"), name == "base", epoch)
        with tempfile.TemporaryDirectory(prefix=".live-", dir=stage) as temporary_live:
            live = Path(temporary_live)
            root = live / "root"
            shutil.copytree(stage / "rootfs", root, symlinks=True)
            shutil.copyfile("tools/installer.sh", root / "install")
            with (root / "etc/motd").open("a") as motd:
                motd.write("\nRAM-live installation test media. Run /install to start.\n")
            sets = root / "sets"
            sets.mkdir()
            archives = ("base.tar.gz", "sdk.tar.gz")
            for name in archives:
                shutil.copyfile(work / name, sets / name)
            boot_payload = sets / "xv6.img"
            shutil.copyfile(boot, boot_payload)
            verify_boot(boot_payload)
            (sets / "SHA256SUMS").write_text("".join(
                f"SHA256 ({name}) = {digest(sets / name)}\n" for name in (*archives, boot_payload.name)))
            root_image = live / "root.img"
            build_image(root_image, root, 32, 1024, mkfs, fsck, debugfs=debugfs)
            create(work / "install.img", boot_payload, root_image)
        checksums = "".join(f"{digest(f)}  {f.name}\n" for f in sorted(work.iterdir()))
        (work / "SHA256SUMS").write_text(checksums)
        # all inputs are checked before replacing a published file
        for name in ("base.tar.gz", "sdk.tar.gz", "install.img", "SHA256SUMS"):
            (work / name).replace(where / name)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("output", type=Path)
    p.add_argument("--stage", type=Path, default=Path("build/x86_64/compat-i386"))
    p.add_argument("--boot", type=Path, default=Path("build/x86_64/xv6.img"))
    p.add_argument("--mke2fs", default="mke2fs")
    p.add_argument("--e2fsck", default="e2fsck")
    p.add_argument("--debugfs", default="debugfs")
    args = p.parse_args()
    try:
        generate(args.output, args.stage, args.boot, args.mke2fs, args.e2fsck, args.debugfs)
    except (OSError, ValueError) as error:
        p.exit(1, f"release: {error}\n")
