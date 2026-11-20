# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

import argparse
import os
from pathlib import Path
import secrets

parser = argparse.ArgumentParser(description="Prepare a new disposable Project9 installer test disk")
parser.add_argument("file", type=Path)
parser.add_argument("--mib", type=int, default=40, choices=range(16, 129), metavar="16..128")
args = parser.parse_args()
fd = os.open(args.file, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
with os.fdopen(fd, "wb") as disk:
    disk.truncate(args.mib * 1024 * 1024)
    disk.seek(-512, os.SEEK_END)
    disk.write(b"P9INSTALLSCRATCH" + secrets.token_bytes(32))
