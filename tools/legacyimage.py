# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

import struct
from pathlib import Path

SECTOR = 512
BLOCKS = 4000
INODES = 256
LOG_START = 2
INODE_START = 32
BITMAP = INODE_START + INODES // 8 + 1
DATA_START = BITMAP + 1
MAGIC = 0x32534650


def save(path, entries):
    image = bytearray(BLOCKS * SECTOR)
    struct.pack_into("<8I", image, SECTOR, BLOCKS, BLOCKS - DATA_START,
                     INODES, INODE_START - LOG_START, LOG_START, INODE_START,
                     BITMAP, MAGIC)
    next_block = DATA_START

    def allocate(data):
        nonlocal next_block
        if next_block >= BLOCKS:
            raise ValueError("xv6-fs is full")
        index = next_block
        next_block += 1
        image[index * SECTOR:index * SECTOR + len(data)] = data
        return index

    numbers, originals, paths, children = {}, {}, {}, {}
    for name, (entry, data, old_number) in sorted(entries.items()):
        if old_number is not None and old_number in originals:
            number = originals[old_number]
        else:
            number = len(numbers) + 1
            if number >= INODES:
                raise ValueError("xv6-fs ran out of inodes")
            numbers[number] = (name, entry, data)
            if old_number is not None:
                originals[old_number] = number
        paths[name] = number
        if name != ".":
            parent, base = name.rsplit("/", 1)
            raw = base.encode(errors="surrogateescape")
            if len(raw) > 14:
                raise ValueError(f"xv6-fs names can only be 14 bytes: {name}")
            children.setdefault(parent, []).append((number, raw, entry.isdir()))

    links = {number: 0 for number in numbers}
    for number in paths.values():
        links[number] += 1
    for number, (name, entry, data) in numbers.items():
        mode = entry.mode
        blocks = []
        size = 0
        count = links[number]
        if entry.isdir():
            parent = name.rsplit("/", 1)[0]
            items = children.get(name, [])
            content = [(number, b"."), (paths[parent], b"..")]
            content.extend((n, part) for n, part, _ in items)
            data = b"".join(struct.pack("<H14s", n, part) for n, part in content)
            count += sum(isdir for _, _, isdir in items)
            kind = 1
        elif entry.ischr():
            kind = 3
            blocks = [entry.devmajor, entry.devminor]
            data = b""
        else:
            kind = 2
            if entry.issym():
                mode |= 0x4000
                data = entry.linkname.encode(errors="surrogateescape")
            elif isinstance(data, Path):
                data = data.read_bytes()
            if name == "./etc/shadow":
                mode |= 0x8000
        if not entry.ischr():
            size = len(data)
            if size > 140 * SECTOR:
                raise ValueError(f"xv6-fs file is too large: {name}")
            blocks = [allocate(data[i:i + SECTOR]) for i in range(0, size, SECTOR)]
            if len(blocks) > 12:
                indirect = struct.pack(f"<{len(blocks) - 12}I", *blocks[12:])
                blocks = blocks[:12] + [allocate(indirect)]
        struct.pack_into("<4HI13I", image, INODE_START * SECTOR + number * 64,
                         kind << 12 | mode, count, entry.uid, entry.gid, size,
                         *(blocks + [0] * (13 - len(blocks))))
    for block in range(next_block):
        image[BITMAP * SECTOR + block // 8] |= 1 << (block % 8)
    with path.open("wb") as stream:
        stream.write(image)
