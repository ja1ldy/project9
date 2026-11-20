# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

import argparse
import os
from pathlib import Path
import struct
import tempfile


HEADER = struct.Struct("<4s5I")
ALIGN = 16


def payload(path, elf_class, machine):
    data = path.read_bytes()
    if (len(data) < (52 if elf_class == 1 else 64) or data[:7] != b"\x7fELF" + bytes((elf_class, 1, 1))
            or struct.unpack_from("<HHI", data, 16) != (2, machine, 1)):
        raise ValueError(f"{path}: expected a little-endian executable for ELF class {elf_class}")
    return data


def aligned(size):
    return (size + ALIGN - 1) & -ALIGN


def pack(output, i386, amd64):
    a = payload(i386, 1, 3)
    b = payload(amd64, 2, 62)
    first = aligned(HEADER.size)
    second = aligned(first + len(a))
    if second + len(b) >= 2**32:
        raise ValueError("combined executable exceeds the on-disk size limit")
    with tempfile.NamedTemporaryFile(dir=output.parent, prefix=".fat-", delete=False) as file:
        temporary = Path(file.name)
        try:
            file.write(HEADER.pack(b"P9FB", 1, first, len(a), second, len(b)))
            file.seek(first)
            file.write(a)
            file.seek(second)
            file.write(b)
            file.flush()
            os.fchmod(file.fileno(), 0o755)
            temporary.replace(output)
        finally:
            temporary.unlink(missing_ok=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Pack static Project9 i386 and x86_64 executables")
    for name in ("output", "i386", "amd64"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        pack(args.output, args.i386, args.amd64)
    except (OSError, ValueError) as error:
        parser.exit(1, f"fatpack: {error}\n")
