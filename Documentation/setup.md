<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Project9 dev setup

Run `python3 tools/ossetup.py --configure` for build settings or
`python3 tools/ossetup.py` for account and disk operations. The ncurses screen uses
a blue background, a white dialog and a blue selection. Use Up, Down,
Enter and Esc. Editable values accept Home, End, Backspace, and Ctrl+U
to clear the field.

| Menu | Settings |
| --- | --- |
| Filesystem | Ext2 or xv6-fs, ext2 size and block size for new disks, optional ext2 data image |
| QEMU settings | CPU model and count, RAM, window or serial display |
| Hostname | Guest hostname |
| Users and passwords | Add accounts and set or reset passwords in masked fields |
| Save configuration | Save settings in `build/<arch>/project9.json` |
| Apply users and hostname to disk | Apply pending credentials and hostname to an existing offline image |
| Build boot and root images | Build source and prepare the chosen disk |
| Start Project9 | Prepare the disk and run QEMU |
| Check disks | Check the root disk and an attached data disk offline |

Passwords remain in memory during the setup session. The configuration
file contains machine settings and the hostname. The guest account
database stores salted PBKDF2 hashes. Set the root password before adding
other users to a fresh disk. To set the initial password inside Project9,
leave the root password unset in setup.

Close QEMU before applying settings to an existing image. To migrate
an older xv6-fs disk at `fs.img`, choose ext2 and approve conversion in
the menu. The default new image is `build/x86_64/install.img` and the original stays at
`fs.img`. The classic profile uses `fs.xv6.img` for a fresh disk.

`python3 tools/ossetup.py --defconfig` asks one question about the
filesystem and saves all other settings at their defaults. Press Enter
for ext2. Pass `--prepare` to prepare offline, `--boot` to boot directly,
or `--check` to check disks. Use the menu for password setup or an
interactive migration confirmation. The regular build defaults to ext2
and accepts `FILESYSTEM=xv6-fs` for the legacy profile.

The ext2 installer uses ordinary directory input to `mke2fs` and
applies guest metadata with `debugfs`. Both ship with e2fsprogs.
Tarball support and libarchive are unnecessary.
