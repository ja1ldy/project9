<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Accounts and permissions

Provision each writable filesystem from its console. On first boot, the
login program prompts for a root password and confirmation. Then sign in
as `root`. Passwords must contain 12 to 96 bytes. The console disables echo
during password entry. Failed logins incur a delay, which grows after
three consecutive failures.

| Command | Behavior |
| --- | --- |
| `useradd [-g GID] NAME` | Root creates a local account and private `/home/NAME`. The name starts with a lowercase letter and can contain lowercase letters, digits, underscores and hyphens. Limit 14 bytes. An optional numeric GID sets the primary group. |
| `passwd` | Changes your password. A non-root user enters the current password first. |
| `passwd NAME` | Root resets the named user's password. |
| `id`, `whoami` | Show identity information without exposing credential material. |
| `exit` or Ctrl+D | Signs out of the shell. |

The kernel stores the credential database in `/etc/shadow`, which normal
file APIs cannot read, modify or link. Each password uses
PBKDF2-HMAC-SHA-256 with 600,000 rounds and a 16-byte per-record salt.
The image builder obtains an installation seed from the host's
`/dev/urandom`. Each password update derives a new salt, and record writes
are serialized and committed by the xv6 disk log.

An ordinary account runs with its own UID and primary GID. The kernel
checks those IDs against inode permissions. Home directories start at
mode 0700 and the root-owned shared directory `/tmp` starts at 1777.
Ordinary users cannot adopt another UID or GID or create device nodes.
See [permissions](history/v0.2.md#permissions) for group access and default modes.

Anyone who can modify the backing image from the host can bypass guest
permissions. Protect access to the emulator, the console during first
boot, and the backing images.

SHA-256 is adapted from Brad Conte's public-domain implementation.
[Source](https://github.com/B-Con/crypto-algorithms)
