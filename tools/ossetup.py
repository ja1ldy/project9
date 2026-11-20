# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

import argparse
import json
import os
import shutil
import tempfile
from pathlib import Path
import subprocess
import sys

from osimage import FILESYSTEMS, build_image, checked_root, factory, image_kind, install, legacy_entries, run
from setup_accounts import account_list, read_guest, update_entries, update_ext2, offline
from installimage import check as check_ext2, create as create_install, extract, partition

OLD_CONFIG = Path(".project9.json")
DEFAULTS = {"filesystem": "ext2", "size": 512, "block": 1024, "cpus": 2, "ram": 512,
            "display": "window", "cpu": "default", "data": "", "hostname": ""}


def save_config(path, config):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(config, indent=2) + "\n")
    temporary.replace(path)


class Setup:
    def __init__(self, args):
        self.args = args
        self.profile = Path("build") / args.arch
        self.config_path = self.profile / "project9.json"
        config_source = self.config_path if self.config_path.exists() else OLD_CONFIG
        self.config = DEFAULTS | (json.loads(config_source.read_text()) if config_source.exists() else {})
        self.pending = {fs: {} for fs in FILESYSTEMS}
        self.confirmed = set()

    @property
    def filesystem(self):
        return self.config["filesystem"]

    @property
    def root(self):
        if self.filesystem == "ext2":
            image = self.output / "install.img"
            if not image.exists():
                for source in (Path("install.img"), Path("fs.img")):
                    if source.is_file():
                        return source
            return image
        image = self.output / "fs.xv6.img"
        if not image.exists():
            for source in (Path("fs.xv6.img"), Path("fs.img")):
                if source.is_file() and image_kind(source) == "legacy":
                    return source
        return image

    @property
    def output(self):
        return self.profile / "legacy" if self.filesystem == "xv6-fs" else self.profile

    @property
    def build(self):
        return self.output / "compat-i386" if self.args.arch == "x86_64" else self.output

    @property
    def boot_image(self):
        return self.output / ("xv6-legacy.img" if self.filesystem == "xv6-fs" else "xv6.img")

    def save(self):
        save_config(self.config_path, self.config)

    def make(self, *targets):
        run(self.args.make, f"ARCH={self.args.arch}", f"FILESYSTEM={self.filesystem}", *map(str, targets))

    def accounts(self):
        data = (read_guest(self.root, "/etc/shadow", self.args.debugfs)
                if self.root.exists() else bytes(512))
        names = [record[2].rstrip(b"\0").decode() for record in account_list(data)]
        return list(dict.fromkeys(names + list(self.pending[self.filesystem])))

    def ext2_tools(self, build=False):
        required = [self.args.e2fsck, self.args.debugfs]
        if build:
            required.insert(0, self.args.mke2fs)
        missing = [tool for tool in required if not shutil.which(tool)]
        if missing:
            names = ", ".join(missing)
            raise ValueError(f"Missing ext2 tools: {names}. Install e2fsprogs and your distribution's debugfs package.")

    def prepare_install(self, action):
        target = self.output / "install.img"
        source = self.root
        exists = source.is_file()
        installed = exists and source == target
        if exists and image_kind(source) not in ("ext2", "legacy"):
            raise ValueError(f"{source} is not a Project9 disk.")
        migrate = exists and image_kind(source) == "legacy"
        pending = self.pending[self.filesystem]
        hostname = self.config["hostname"]
        if migrate and not (self.args.migrate or source in self.confirmed):
            raise ValueError("Approve conversion in the setup menu or pass --prepare --migrate.")
        stage = self.build / "rootfs"
        legacy_check = self.build / "legacy-fsck"
        if action == "apply" and (not installed or migrate):
            raise ValueError("Select Build or Start to create install.img first.")
        if not exists or migrate:
            self.ext2_tools(build=True)
        elif not installed or pending or hostname:
            self.ext2_tools()
        targets = [self.boot_image]
        if not exists or migrate:
            targets.append(stage)
        if migrate:
            targets.append(legacy_check)
        self.make(*targets)
        if installed and not pending and not hostname and self.boot_image.stat().st_mtime_ns <= target.stat().st_mtime_ns:
            return
        self.ext2_tools()
        if installed and partition(source) is None:
            raise ValueError(f"{source} lacks a Project9 install partition.")
        with tempfile.TemporaryDirectory(prefix=".setup-", dir=Path.cwd()) as tmp:
            root = Path(tmp) / "root.img"
            if not exists or migrate:
                entries = factory(stage)
                if migrate:
                    run(legacy_check, source)
                    with offline(source):
                        entries = install(legacy_entries(source), entries)
                update_entries(entries, pending, hostname)
                build_image(root, stage, self.config["size"], self.config["block"],
                            self.args.mke2fs, self.args.e2fsck, debugfs=self.args.debugfs,
                            filesystem="ext2", entries=entries)
                create_install(target, self.boot_image, root, replace=False)
            else:
                with offline(source):
                    if partition(source):
                        extract(source, root)
                    else:
                        shutil.copyfile(source, root)
                    if hostname and read_guest(root, "/etc/hostname", self.args.debugfs).decode().strip() == hostname:
                        hostname = ""
                    need_update = pending or hostname
                    if not installed or need_update or self.boot_image.stat().st_mtime_ns > target.stat().st_mtime_ns:
                        run(self.args.e2fsck, "-fn", root)
                        update_ext2(root, pending, hostname, self.args.debugfs, self.args.e2fsck)
                        create_install(target, self.boot_image, root, replace=installed)
        pending.clear()
        if exists and not installed:
            print(f"Imported {source} into {target}. The original disk is available at {source}.")

    def prepare(self, action):
        if self.filesystem == "ext2":
            return self.prepare_install(action)
        source, stage = self.root, self.build / "rootfs"
        root = self.output / "fs.xv6.img"
        checker = self.build / "legacy-fsck"
        exists = source.is_file()
        copying = exists and source != root
        pending = self.pending[self.filesystem]
        hostname = self.config["hostname"]
        if not exists and action == "apply":
            raise ValueError("Select Build or Start to create the root disk first.")
        self.make(*([self.boot_image, checker, stage] if not exists else
                    [self.boot_image, checker]))
        if not exists or copying or pending or hostname:
            if exists:
                run(checker, source)
                with offline(source):
                    entries = legacy_entries(source)
                    update_entries(entries, pending, hostname)
                    build_image(root, stage, 0, 0, self.args.mke2fs, self.args.e2fsck,
                                checker, filesystem=self.filesystem, entries=entries, replace=not copying)
            else:
                entries = factory(stage)
                update_entries(entries, pending, hostname)
                build_image(root, stage, 0, 0, self.args.mke2fs, self.args.e2fsck,
                            checker, filesystem=self.filesystem, entries=entries)
        pending.clear()
        checked_root(root, self.filesystem)

    def check(self):
        kind = image_kind(self.root)
        if kind == "legacy":
            self.make(self.build / "legacy-fsck")
            run(self.build / "legacy-fsck", self.root)
        else:
            check_ext2(self.root, self.args.e2fsck)
        if self.config["data"]:
            run(self.args.e2fsck, "-fn", self.config["data"])

    def boot(self):
        def drive(path, index):
            name = str(path).replace(",", ",,")
            return "-drive", f"file={name},index={index},media=disk,format=raw"
        c = self.config
        command = [self.args.qemu, "-serial", "mon:stdio", "-smp", str(c["cpus"]), "-m", str(c["ram"])]
        if c["display"] == "serial":
            command.extend(("-display", "none"))
        if c["cpu"] != "default":
            command.extend(("-cpu", c["cpu"]))
        if self.filesystem == "ext2":
            command.extend(drive(self.root, 0))
        else:
            command.extend(drive(self.boot_image, 0))
            command.extend(drive(self.root, 1))
        if c["data"]:
            data = Path(c["data"])
            if data.samefile(self.root) or data.samefile(self.boot_image):
                raise ValueError("Choose a separate secondary disk image.")
            if partition(data):
                raise ValueError("Select a standalone ext2 image as the secondary disk.")
            checked_root(data)
            command.extend(drive(data, 2))
        run(*command)


def defconfig(setup):
    raw = input("What file system should be used? ext2 or xv6-fs [ext2] ").strip() or "ext2"
    if raw not in FILESYSTEMS:
        raise ValueError("Choose ext2 or xv6-fs.")
    save_config(setup.config_path, DEFAULTS | {"filesystem": raw})


def main():
    parser = argparse.ArgumentParser(description="Configure and start Project9 dev")
    action = parser.add_mutually_exclusive_group()
    for name in ("boot", "prepare", "check", "defconfig", "profile", "configure"):
        action.add_argument("--" + name, action="store_const", const=name, dest="action")
    parser.add_argument("--arch", choices=("i386", "x86_64"), default="x86_64")
    parser.add_argument("--migrate", action="store_true")
    parser.add_argument("--make", default="make")
    parser.add_argument("--qemu")
    parser.add_argument("--mke2fs", default="mke2fs")
    parser.add_argument("--e2fsck", default="e2fsck")
    parser.add_argument("--debugfs", default="debugfs")
    args = parser.parse_args()
    if args.qemu is None:
        args.qemu = (shutil.which(f"qemu-system-{args.arch}") or
                     shutil.which("qemu-system-x86_64") or "qemu-system-i386")
    setup = Setup(args)
    if args.action == "defconfig":
        defconfig(setup)
        return
    if args.action == "profile":
        print(setup.filesystem if setup.filesystem in FILESYSTEMS else "ext2")
        return
    if not args.action or args.action == "configure":
        from osmenu import menu
        choice = menu(setup, args.action == "configure")
    else:
        choice = args.action
    if choice == "check":
        setup.check()
    elif choice in ("prepare", "boot", "apply"):
        setup.prepare(choice)
        if choice == "boot":
            setup.boot()
        else:
            print("Project9 disks are ready.")


if __name__ == "__main__":
    try:
        main()
    except (EOFError, KeyboardInterrupt):
        sys.exit("Project9 setup closed.")
    except FileNotFoundError as error:
        sys.exit(f"project9: {error.filename} was not found. Check your host tools and PATH.")
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        sys.exit(f"project9: {error}")
