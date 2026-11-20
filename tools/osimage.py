# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

import argparse
import os
from pathlib import Path
import re
import shutil
import secrets
import stat
import struct
import subprocess
import sys
import tarfile
import tempfile
import legacyimage
from installimage import partition

MIB = 1024 * 1024
SECTOR = 512
FORMATS = (1024, 2048, 4096)
FILESYSTEMS = ("ext2", "xv6-fs")
LEGACY_MAGIC = 0x32534650


def run(*args):
    subprocess.run(args, check=True)


def image_kind(path):
    if partition(path):
        return "ext2"
    with path.open("rb") as stream:
        stream.seek(SECTOR + 28)
        magic = stream.read(4)
        stream.seek(1024 + 56)
        if stream.read(2) == b"\x53\xef":
            return "ext2"
        if magic == struct.pack("<I", LEGACY_MAGIC):
            return "legacy"
    raise ValueError(f"{path} has an unknown filesystem")


def directory(name, mode):
    entry = tarfile.TarInfo(name)
    entry.type = tarfile.DIRTYPE
    entry.mode = mode
    return entry


def root_executable(name):
    return name.startswith(("bin/", "usr/X11/bin/", "usr/bin/")) or name == "install"


def factory(stage):
    entries = {".": (directory(".", 0o755), None, None)}
    for path in sorted(stage.rglob("*")):
        name = "./" + path.relative_to(stage).as_posix()
        if path.is_symlink():
            entry = tarfile.TarInfo(name)
            entry.type, entry.mode, entry.linkname = tarfile.SYMTYPE, 0o777, os.readlink(path)
            entries[name] = entry, None, None
        elif path.is_dir():
            mode = {"./root": 0o700, "./tmp": 0o1777}.get(name, 0o755)
            entries[name] = directory(name, mode), None, None
        else:
            entry = tarfile.TarInfo(name)
            entry.mode = 0o755 if root_executable(name[2:]) else 0o644
            entry.size = path.stat().st_size
            entries[name] = entry, path, None
    shadow = tarfile.TarInfo("./etc/shadow")
    shadow.mode = 0
    shadow.size = SECTOR
    entries[shadow.name] = shadow, secrets.token_bytes(32).ljust(SECTOR, b"\0"), None
    return entries


def legacy_entries(path):
    image = path.read_bytes()
    _, _, _, _, log, inode_start, _, magic = struct.unpack_from("<8I", image, SECTOR)
    if magic != LEGACY_MAGIC or struct.unpack_from("<I", image, log * SECTOR)[0]:
        raise ValueError("boot the previous kernel and let its journal finish before migrating")

    def inode(number):
        start = inode_start * SECTOR + number * 64
        kind, links, uid, gid, size, *blocks = struct.unpack_from("<4HI13I", image, start)
        return kind, links, uid, gid, size, blocks

    def contents(size, blocks):
        if size > 12 * SECTOR:
            blocks = blocks[:12] + list(struct.unpack_from("<128I", image, blocks[12] * SECTOR))
        return b"".join(image[b * SECTOR:(b + 1) * SECTOR]
                        for b in blocks[:(size + SECTOR - 1) // SECTOR])[:size]

    entries = {}
    dirs = set()
    pending = [(".", 1)]
    while pending:
        name, number = pending.pop()
        kind, links, uid, gid, size, blocks = inode(number)
        if name == "./etc/shadow" and (not kind & 0x8000 or links != 1):
            raise ValueError("the old account database inode is not protected")
        mode = kind & 0o3777
        kind_type = (kind >> 12) & 3
        entry = tarfile.TarInfo(name)
        entry.uid, entry.gid, entry.mode = uid, gid, mode
        payload = None
        if name in entries:
            raise ValueError(f"duplicate path {name!r} in the old disk")
        if kind_type == 1:
            if number in dirs:
                raise ValueError("directory cycle in the old disk")
            dirs.add(number)
            entry.type = tarfile.DIRTYPE
            for child, raw in struct.iter_unpack("<H14s", contents(size, blocks)):
                base = raw.split(b"\0", 1)[0]
                if not child or base in (b".", b".."):
                    continue
                if not base or b"/" in base:
                    raise ValueError("invalid directory entry in the old disk")
                pending.append((name + "/" + os.fsdecode(base), child))
        elif kind_type == 2:
            payload = contents(size, blocks)
            if kind & 0x4000:
                entry.type = tarfile.SYMTYPE
                entry.linkname = os.fsdecode(payload)
                payload = None
            else:
                entry.size = size
        elif kind_type == 3:
            entry.type = tarfile.CHRTYPE
            entry.devmajor, entry.devminor = blocks[:2]
        else:
            raise ValueError(f"invalid inode at {name!r}")
        entries[name] = entry, payload, None if entry.isdir() else number

    etc, _, _ = entries["./etc"]
    shadow, _, _ = entries["./etc/shadow"]
    if (not etc.isdir() or etc.uid != 0 or etc.mode & 0o022 or
        not shadow.isreg() or shadow.uid or shadow.gid or shadow.mode or
        shadow.size < SECTOR or shadow.size % SECTOR):
        raise ValueError("the old disk does not have a trusted /etc/shadow")
    return entries


def install(base, staged):
    for name, entry in staged.items():
        if name == "./etc/shadow":
            continue
        if name not in base:
            base[name] = entry
        elif name.startswith(("./bin/", "./usr/share/man/", "./usr/X11/")):
            if entry[0].isdir() != base[name][0].isdir():
                raise ValueError(f"cannot install over {name!r}")
            if not entry[0].isdir():
                base[name] = entry
        elif entry[0].isdir() and not base[name][0].isdir():
            raise ValueError(f"{name!r} must be a directory")
    return base


def stage_tree(path, entries):
    path.mkdir()
    linked = {}
    for name, (entry, payload, number) in sorted(entries.items()):
        out = path / name
        if name == ".":
            continue
        if number in linked:
            os.link(linked[number], out, follow_symlinks=False)
            continue
        if entry.isdir():
            out.mkdir()
        elif entry.issym():
            out.symlink_to(entry.linkname)
        elif isinstance(payload, Path):
            shutil.copyfile(payload, out)
        else:
            out.write_bytes(payload or b"")
        if number is not None:
            linked[number] = out


def inode_attributes(image, entries, debugfs, work):
    # names are read as bytes and only numeric inode references go into debugfs commands
    directories = {}
    for name in entries:
        if name != ".":
            parent, basename = name.rsplit("/", 1)
            directories.setdefault(parent, {})[os.fsencode(basename)] = name
    commands, seen, pending = [], set(), [(".", 2)]
    while pending:
        name, inode = pending.pop()
        if inode in seen:
            continue
        seen.add(inode)
        entry = entries[name][0]
        kind = ((stat.S_IFDIR if entry.isdir() else stat.S_IFLNK if entry.issym()
                 else stat.S_IFCHR if entry.ischr() else stat.S_IFREG) | entry.mode)
        commands.extend((f"set_inode_field <{inode}> mode {kind:#07o}".replace("0o", "0"),
                         f"set_inode_field <{inode}> uid {entry.uid}",
                         f"set_inode_field <{inode}> gid {entry.gid}"))
        if entry.ischr():
            minor = entry.devminor
            raw = (entry.devmajor << 8) | (minor & 255)
            commands.append(f"set_inode_field <{inode}> block[{int(minor > 255)}] {raw | ((minor & ~255) << 12)}")
        if children := directories.get(name):
            listing = subprocess.check_output([debugfs, "-R", f"ls -p <{inode}>", image],
                                              stderr=subprocess.DEVNULL)
            found = {child: int(number) for number, child in
                     re.findall(rb"/(\d+)/[0-7]+/\d+/\d+/([^/]*)/[^/]*/\r?\n", listing)}
            if children.keys() - found.keys():
                raise ValueError(f"could not map all ext2 inodes under {name!r}")
            pending.extend((child, found[base]) for base, child in children.items())
    script = work / "attributes"
    script.write_text("\n".join(commands) + "\n")
    result = subprocess.run([debugfs, "-w", "-f", script.name, image],
                            cwd=work, check=True, capture_output=True)
    if len(result.stderr.splitlines()) > 1:
        raise ValueError(result.stderr.decode(errors="replace").strip())


def build_image(output, stage, size, block, mkfs, fsck, legacy_check=None,
                *, debugfs="debugfs", filesystem="ext2", entries=None, migrate=False, replace=False):
    if filesystem == "ext2" and (block not in FORMATS or not 8 <= size <= 4096):
        raise ValueError("choose 8–4096 MiB and a 1024, 2048 or 4096 byte block size")
    supplied = entries is not None
    if not supplied:
        entries = factory(stage)
    if migrate:
        if image_kind(output) != "legacy":
            raise ValueError(f"{output} is already ext2")
        if not supplied:
            run(legacy_check, output)
            entries = install(legacy_entries(output), entries)

    with tempfile.TemporaryDirectory(prefix=".project9-", dir=output.parent) as tmp:
        work = Path(tmp).resolve()
        image = work / "root.img"
        fd = os.open(image, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
        with os.fdopen(fd, "wb") as stream:
            if filesystem == "ext2":
                stream.truncate(size * MIB)
        if filesystem == "ext2":
            tree = work / "tree"
            stage_tree(tree, entries)
            # optional directory file type bytes are omitted so device placeholders can be converted
            run(mkfs, "-q", "-F", "-t", "ext2", "-b", str(block), "-m", "0",
                "-O", "none,sparse_super,large_file", "-d", tree, image)
            inode_attributes(image, entries, debugfs, work)
            run(fsck, "-fn", image)
        else:
            legacyimage.save(image, entries)
            run(legacy_check, image)
        with image.open("rb") as stream:
            os.fsync(stream.fileno())
        if migrate:
            backup = output.with_name(output.name + ".xv6-backup")
            os.link(output, backup)
            print(f"Saved the original disk as {backup}", flush=True)
        if not migrate and not replace and output.name in ("fs.img", "fs.xv6.img"):
            os.link(image, output)
        else:
            os.replace(image, output)
    return output


def checked_root(path, filesystem="ext2"):
    if image_kind(path) != ("ext2" if filesystem == "ext2" else "legacy"):
        raise ValueError(f"{path} does not match the {filesystem} profile. Run make os")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("image", type=Path)
    parser.add_argument("--stage", type=Path)
    parser.add_argument("--size", type=int, default=32)
    parser.add_argument("--block", type=int, choices=FORMATS, default=1024)
    parser.add_argument("--mke2fs", default="mke2fs")
    parser.add_argument("--e2fsck", default="e2fsck")
    parser.add_argument("--legacy-check")
    parser.add_argument("--debugfs", default="debugfs")
    parser.add_argument("--filesystem", choices=FILESYSTEMS, default="ext2")
    parser.add_argument("--migrate", action="store_true")
    parser.add_argument("--check-root", action="store_true")
    args = parser.parse_args()
    if args.check_root:
        checked_root(args.image, args.filesystem)
    else:
        build_image(args.image, args.stage, args.size, args.block,
                    args.mke2fs, args.e2fsck, args.legacy_check, debugfs=args.debugfs,
                    filesystem=args.filesystem, migrate=args.migrate)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        sys.exit(f"project9: {error}")
