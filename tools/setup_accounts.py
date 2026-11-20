# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

from contextlib import contextmanager
import fcntl
import hashlib
import os
from pathlib import Path
import secrets
import struct
import subprocess
import tarfile
import tempfile

from osimage import directory, image_kind, legacy_entries, run
from installimage import extract, partition

RECORD = struct.Struct("<II15s16s32s2s439s")
ROUNDS = 600000
FIRST_UID = 1000
LIMIT = 64


def password_digest(password):
    raw = password.encode()
    if len(raw) < 12 or len(raw) > 96 or b"\0" in raw:
        raise ValueError("Use a password between 12 and 96 bytes.")
    salt = secrets.token_bytes(16)
    digest = hashlib.pbkdf2_hmac("sha256", raw, salt, ROUNDS)
    return salt, digest


def account_list(data):
    if len(data) < RECORD.size or len(data) % RECORD.size:
        raise ValueError("Invalid account database.")
    return [RECORD.unpack(data[offset:offset + RECORD.size])
            for offset in range(RECORD.size, len(data), RECORD.size)]


def merge_accounts(data, pending):
    records = account_list(data)
    positions = {record[2].rstrip(b"\0").decode(): i for i, record in enumerate(records)}
    output = bytearray(data)
    for name, (salt, digest) in pending.items():
        if name in positions:
            i = positions[name]
            uid, _, _, _, _, group, _ = records[i]
        else:
            i = len(output) // RECORD.size - 1
            if i >= LIMIT:
                raise ValueError("The guest account table is full.")
            if not i and name != "root":
                raise ValueError("Set the root password before adding a user.")
            uid = FIRST_UID + i - 1 if i else 0
            group = bytes(2)
        encoded = RECORD.pack(uid, ROUNDS, name.encode(), salt, digest, group, b"")
        offset = (i + 1) * RECORD.size
        output[offset:offset + RECORD.size] = encoded
    return bytes(output)


def read_guest(image, name, debugfs):
    if partition(image):
        with tempfile.TemporaryDirectory(prefix=".read-", dir=image.resolve().parent) as tmp:
            root = Path(tmp) / "root.img"
            extract(image, root)
            return read_guest(root, name, debugfs)
    if image_kind(image) == "legacy":
        return legacy_entries(image)["." + name][1]
    return subprocess.check_output([debugfs, "-R", f"cat {name}", image],
                                   stderr=subprocess.DEVNULL)


def set_hostname(entries, hostname):
    if not hostname:
        return
    entry = tarfile.TarInfo("./etc/hostname")
    raw = (hostname + "\n").encode()
    entry.mode, entry.size = 0o644, len(raw)
    entries[entry.name] = entry, raw, None


def update_entries(entries, pending, hostname):
    set_hostname(entries, hostname)
    if not pending:
        return
    entry, old, _ = entries["./etc/shadow"]
    new = merge_accounts(old, pending)
    entry.size = len(new)
    entries[entry.name] = entry, new, None
    for uid, _, raw, _, _, group, _ in account_list(new):
        name = raw.rstrip(b"\0").decode()
        if not uid:
            continue
        path = "./home/" + name
        info = directory(path, 0o700)
        info.uid, info.gid = uid, int.from_bytes(group, "little") or uid
        if path in entries and (not entries[path][0].isdir() or entries[path][0].uid != uid):
            raise ValueError(f"The home directory for {name} belongs to another account.")
        entries.setdefault(path, (info, None, None))


@contextmanager
def offline(image):
    with image.open("r+b") as disk:
        try:
            fcntl.lockf(disk, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            raise ValueError(f"Stop QEMU before editing {image}.") from error
        yield


def update_ext2(image, pending, hostname, debugfs, fsck):
    if not pending and not hostname:
        return
    # work on a private clone and publish it after the offline check
    with offline(image), tempfile.TemporaryDirectory(prefix=".project9-", dir=image.parent) as tmp:
        work = Path(tmp).resolve()
        clone = work / "root.img"
        fd = os.open(clone, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
        with os.fdopen(fd, "wb") as dest, image.open("rb") as src:
            while chunk := src.read(1024 * 1024):
                if chunk.count(0) == len(chunk):
                    dest.seek(len(chunk), os.SEEK_CUR)
                else:
                    dest.write(chunk)
            dest.truncate()
        commands = []
        if pending:
            data = read_guest(clone, "/etc/shadow", debugfs)
            updated = merge_accounts(data, pending)
            (work / "shadow").write_bytes(updated)
            commands.extend(("rm /etc/shadow", "write shadow /etc/shadow",
                             "set_inode_field /etc/shadow mode 0100000",
                             "set_inode_field /etc/shadow uid 0",
                             "set_inode_field /etc/shadow gid 0"))
            for uid, _, raw, _, _, group, _ in account_list(updated)[len(account_list(data)):]:
                name = raw.rstrip(b"\0").decode()
                if not uid:
                    continue
                home = "/home/" + name
                gid = int.from_bytes(group, "little") or uid
                commands.extend((f"mkdir {home}", f"set_inode_field {home} mode 040700",
                                 f"set_inode_field {home} uid {uid}",
                                 f"set_inode_field {home} gid {gid}"))
        if hostname:
            (work / "hostname").write_text(hostname + "\n")
            commands.extend(("rm /etc/hostname", "write hostname /etc/hostname",
                             "set_inode_field /etc/hostname mode 0100644",
                             "set_inode_field /etc/hostname uid 0",
                             "set_inode_field /etc/hostname gid 0"))
        (work / "changes").write_text("\n".join(commands) + "\n")
        result = subprocess.run([debugfs, "-w", "-f", "changes", clone], cwd=work,
                                check=True, capture_output=True)
        if len(result.stderr.splitlines()) > 1:
            raise ValueError(result.stderr.decode(errors="replace").strip())
        run(fsck, "-fn", clone)
        with clone.open("rb") as stream:
            os.fsync(stream.fileno())
        os.replace(clone, image)
