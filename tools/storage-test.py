# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import sys
import tempfile
import time

from installimage import extract, partition, recover

PASSWORD = b"project9-storage-check"
PROMPT = rb"root@[^:]+:[^\n]*# "


def progress(message):
    print(message, flush=True)


class Guest:
    def __init__(self, args, label, log, drives, machine, extra=()):
        self.label = label
        self.buffer = b""
        self.tail = b""
        self.timeout = args.timeout
        self.serial = args.serial
        self.second_cpu = False
        self.x86_64 = getattr(args, "x86_64", False)
        command = [args.qemu, "-display", "none", "-serial", "stdio", "-monitor", "none",
                   "-net", "none", "-m", "128", "-smp", "2", "-M", machine]
        if self.x86_64:
            command += ["-cpu", "qemu64"]
        for index, drive in enumerate(drives):
            command += ["-drive", f"index={index},format=raw,file={drive}"]
        command.extend(extra)
        self.log = log.open("wb")
        try:
            self.process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                            stderr=subprocess.STDOUT, start_new_session=True)
        except BaseException:
            self.log.close()
            raise

    def say(self, message):
        progress(f"{self.label}: {message}")

    def see(self, pattern):
        started = time.monotonic()
        deadline = started + self.timeout
        next_report = started + 10
        while True:
            if found := re.search(pattern, self.buffer):
                self.second_cpu |= b"cpu1: starting" in self.buffer[:found.end()]
                self.buffer = self.buffer[found.end():]
                return found
            if re.search(rb"(?m)^[^\r\n]*panic:[^\r\n]*\r?\n", self.buffer):
                reason = "guest panicked"
                break
            now = time.monotonic()
            if now >= deadline:
                reason = f"timed out after {self.timeout}s"
                break
            if now >= next_report:
                lines = self.tail.decode(errors="replace").strip().splitlines()
                latest = lines[-1][-160:] if lines else "no guest output yet"
                self.say(f"still waiting ({int(now-started)}s), last output: {latest}")
                next_report = now + 10
            if not select.select([self.process.stdout], [], [], min(deadline, next_report)-now)[0]:
                continue
            data = os.read(self.process.stdout.fileno(), 65536)
            if not data:
                reason = "QEMU closed its serial output"
                break
            self.log.write(data)
            self.log.flush()
            if self.serial:
                sys.stdout.buffer.write(data)
                sys.stdout.buffer.flush()
            self.buffer = (self.buffer + data)[-131072:]
            self.tail = (self.tail + data)[-1600:]
        tail = self.tail.decode(errors="replace").strip() or "(no serial output)"
        raise RuntimeError(f"{self.label}: {reason} waiting for {pattern!r}\n{tail}")

    def send(self, data):
        self.process.stdin.write(data)
        self.process.stdin.flush()

    def boot(self, live=False):
        self.say("waiting for the boot menu")
        menu = self.see(rb"(?s)(1: boot|1\. x86 \(32-bit\) - Disk).*?Autoboot in 3 seconds")
        old_keys = menu.group(1) == b"1: boot"
        self.say("starting the RAM-live guest" if live else "booting the test disk")
        if self.x86_64:
            disk_key, live_key = (b"6", b"7") if old_keys else (b"3", b"4")
            self.send(live_key if live else disk_key)
            # verify the selected kernel before checking the guest
            result = self.see(rb"Project9 x86_64 kernel|booting i386")
            if result.group() == b"booting i386":
                raise RuntimeError(f"{self.label}: x86_64 boot fell back to i386")
        else:
            live_key = b"5" if old_keys else b"2"
            self.send(live_key if live else b"1")
        if live:
            root = self.see(rb"ext2: / read-write [^\r\n]+\r?\n")
            if not root.group().strip().endswith(b" in memory"):
                raise RuntimeError(f"{self.label}: booted from disk instead of entering RAM-live mode")

    def login(self, first=False):
        self.say("waiting for the guest login")
        if first:
            self.see(b"New password:")
            self.send(PASSWORD+b"\n")
            self.see(b"Retype new password:")
            self.send(PASSWORD+b"\n")
        self.see(b"project9 login:")
        self.send(b"root\n")
        self.see(b"Password:")
        self.send(PASSWORD+b"\n")
        self.see(PROMPT)

    def run(self, command, expected=None):
        self.say(f"running {command}")
        self.send(command.encode()+b"\n")
        if expected is None:
            self.see(PROMPT)
            return None
        match = self.see(rb"(?P<result>"+expected+rb")|(?P<prompt>"+PROMPT+rb")")
        result = match.group("result")
        if result is None:
            tail = self.tail.decode(errors="replace").strip()
            raise RuntimeError(f"{self.label}: {command} returned to the shell without its success marker\n{tail}")
        self.see(PROMPT)
        return result

    def poweroff(self):
        self.say("waiting for a clean shutdown")
        self.send(b"poweroff\n")
        self.see(b"filesystems synchronized")
        if self.process.wait(15):
            raise RuntimeError(f"{self.label}: shutdown failed")

    def cut_power(self):
        self.process.kill()
        self.process.wait(15)

    def close(self):
        if self.process.poll() is None:
            self.cut_power()
        self.process.stdin.close()
        self.process.stdout.close()
        self.log.close()


def digest(path):
    with path.open("rb") as file:
        h = hashlib.sha256()
        while data := file.read(1024*1024):
            h.update(data)
    return h.digest()


def edges(path):
    first, size = partition(path)
    with path.open("rb") as file:
        head = file.read(first)
        file.seek(first+size)
        return head, file.read()


def fsck(image, e2fsck, directory, label):
    progress(f"{label}: running offline e2fsck")
    root = directory / (label+".ext2")
    extract(image, root)
    try:
        subprocess.run([e2fsck, "-fn", root], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       check=True)
    except subprocess.CalledProcessError as error:
        raise RuntimeError(error.stdout.decode(errors="replace")) from error
    finally:
        root.unlink()


def metadata(image):
    first, _ = partition(image)
    with image.open("rb") as file:
        file.seek(first+1024)
        return file.read(120)


def unchanged_except_sector(original, updated, sector):
    with original.open("rb") as before, updated.open("rb") as after:
        offset = sector*512
        if before.read(offset) != after.read(offset):
            return False
        before.seek(512, os.SEEK_CUR)
        after.seek(512, os.SEEK_CUR)
        while data := before.read(1024*1024):
            if data != after.read(len(data)):
                return False
        return not after.read(1)


def install(args, work, guest, bus):
    prefix = "sd" if bus == "sata" else "hd"
    image, protected, media = (work / (bus+"-"+name+".img")
                               for name in ("installed", "protected", "media"))
    subprocess.run([args.python, Path(__file__).with_name("install-target.py"), image], check=True)
    # a tag on a disk with unrelated boot code must never make it eligible
    subprocess.run([args.python, Path(__file__).with_name("install-target.py"), protected], check=True)
    with protected.open("r+b") as file:
        file.write(b"unrelated boot data")
    shutil.copyfile(args.media, media)
    hashes = digest(protected), digest(media)
    with guest(bus+"-install", [media, image, protected], bus) as vm:
        vm.boot(live=True)
        vm.login(first=True)
        vm.run("sysctl hw.disknames", fr"hw.disknames={prefix}0:boot,{prefix}1:test-target,{prefix}2\r?\n".encode())
        for index in (0, 2):
            vm.run(f"fdisk -iy {prefix}{index}", b"target is protected")
        vm.run(f"fdisk -iy {prefix}1", b"partition 1 starts")
        before = digest(image)
        vm.run(f"stability storage-guard {prefix}1p1", b"partition boundaries passed")
        if digest(image) != before:
            raise RuntimeError("an out of bounds command changed the test disk")
        vm.say("installing Project9 onto the disposable disk")
        vm.send(b"/install\n")
        vm.see(b"Type the name")
        vm.send(f"{prefix}1\n".encode())
        vm.see(b"Type yes")
        vm.send(b"yes\n")
        vm.see(b"successfully completed")
        vm.see(b"Exit to ")
        vm.send(b"h\n")
        vm.see(b"filesystems synchronized")
        vm.process.wait(15)
    if hashes != (digest(protected), digest(media)):
        raise RuntimeError("installer changed a protected disk")
    return image


def normal(args, work, guest, bus, image):
    original_edges = edges(image)
    with guest(bus+"-regression", [image], bus) as vm:
        vm.boot()
        vm.login(first=True)
        vm.run("sync && echo SYNC-OK", rb"\nSYNC-OK\r?\n")
        checks = [("stability hardening", b"STABILITY TESTS PASSED")]
        if vm.second_cpu or args.require_smp:
            checks.append(("stability smp", b"SMP TESTS PASSED"))
        checks += [("usertests", b"ALL TESTS PASSED"),
                   ("stability full-disk", b"STABILITY TESTS PASSED"),
                   ("stability inodes", b"inode exhaustion recovered")]
        for command, marker in checks:
            vm.run(command, marker)
            vm.run("sync && echo SYNC-OK", rb"\nSYNC-OK\r?\n")
        vm.run("echo persisted > /root/reliability && sync && echo SYNC-OK", rb"\nSYNC-OK\r?\n")
        for _ in range(2):
            vm.send(b"reboot\n")
            vm.see(b"filesystems synchronized")
            vm.boot()
            vm.login()
            vm.run("cat /root/reliability", rb"\npersisted\r?\n")
        vm.poweroff()
        print(f"{bus}: regression tests and two reboots passed "
              f"(second guest CPU {'observed' if vm.second_cpu else 'unavailable'})", flush=True)
    if original_edges != edges(image):
        raise RuntimeError(f"{bus}: filesystem operations changed bytes outside the partition")
    fsck(image, args.e2fsck, work, bus)


def failures(args, work, guest, bus, image):
    root = work / (bus+"-inspect.ext2")
    extract(image, root)
    offset, _ = partition(image)
    locations = {}
    for path in ("/root/reliability", "/root"):
        result = subprocess.check_output([args.debugfs, "-R", "blocks "+path, root],
                                         stderr=subprocess.DEVNULL)
        locations[path] = offset//512 + int(result.split()[0])*8
    root.unlink()
    original_edges = edges(image)

    for operation in ("read", "write", "flush", "metadata"):
        test = work / f"{bus}-{operation}.img"
        config = test.with_suffix(".conf")
        shutil.copyfile(image, test)
        if operation == "flush":
            rule = ('[set-state]\nevent = "flush_to_disk"\nstate = "1"\nnew_state = "2"\n'
                    '[inject-error]\nevent = "flush_to_disk"\nstate = "2"\n')
        else:
            event = "read_aio" if operation == "read" else "write_aio"
            sector = locations["/root" if operation == "metadata" else "/root/reliability"]
            rule = f'[inject-error]\nevent = "{event}"\nsector = "{sector}"\n'
        config.write_text(rule+'errno = "5"\nonce = "on"\nimmediately = "on"\n')
        marker = f"storage {operation} failure reported and writes blocked".encode()
        if operation == "flush":
            marker += rb" \(dirty marker|sync\)"
        with guest(f"{bus}-{operation}-fault", [f"blkdebug:{config}:{test}"], bus) as vm:
            vm.boot()
            vm.login()
            result = vm.run(f"stability storage-io {operation}", marker)
            vm.cut_power()
        if original_edges != edges(test):
            raise RuntimeError(f"{bus}: injected {operation} error changed bytes outside the partition")
        if result.endswith(b"(dirty marker)"):
            # a rejected first write may leave only the superblock sector changed
            if metadata(test)[58:60] not in (b"\0\0", b"\1\0") or not unchanged_except_sector(
                    image, test, offset//512+2):
                raise RuntimeError(f"{bus}: a write reached the filesystem past a rejected dirty marker")
            progress(f"{bus}: injected flush failure passed at the dirty marker")
        elif operation != "read" and metadata(test)[58:60] != b"\0\0":
            raise RuntimeError(f"{bus}: failed update did not leave a dirty filesystem")
        else:
            progress(f"{bus}: injected {operation} failure passed"
                     + (" at sync" if operation == "flush" else ""))


def crash_and_recover(args, work, guest, image):
    interrupted = work / "interrupted.img"
    shutil.copyfile(image, interrupted)
    with guest("interrupted", [interrupted], "ata") as vm:
        vm.boot()
        vm.login()
        vm.send(b"mkdir /tmp/interrupted; echo CUT-READY; while true; do touch /tmp/interrupted/a; rm /tmp/interrupted/a; done\n")
        vm.see(rb"\nCUT-READY\r?\n")
        vm.cut_power()
    with guest("dirty-root", [interrupted], "ata") as vm:
        vm.boot()
        vm.see(b"root ext2 needs an offline check")
    print("unclean root rejected after interrupted writes", flush=True)

    # check the repair path with known damage which preen can safely repair
    recoverable = work / "repairable.img"
    shutil.copyfile(image, recoverable)
    offset, _ = partition(recoverable)
    with recoverable.open("r+b") as file:
        file.seek(offset+1024+12)
        file.write(b"\0"*4)
        file.seek(offset+1024+58)
        file.write(b"\0"*2)
    before = digest(recoverable)
    repaired = work / "recovered.img"
    recover(recoverable, repaired, args.e2fsck)
    if digest(recoverable) != before or edges(recoverable) != edges(repaired):
        raise RuntimeError("recovery modified the source or data outside the partition")
    with guest("recovered", [repaired], "ata") as vm:
        vm.boot()
        vm.login()
        vm.run("cat /root/reliability", rb"\npersisted\r?\n")
        vm.poweroff()

    scratch = work / "guest-repair.img"
    shutil.copyfile(recoverable, scratch)
    with scratch.open("r+b") as file:
        file.write(bytes(446))
    media = work / "ata-media.img"
    with guest("guest-fsck", [media, scratch], "ata") as vm:
        vm.boot(live=True)
        vm.login(first=True)
        vm.run("fsck hd1p1", b"allocation summary or clean flag differs")
        vm.run("fsck -y hd1p1", b"repaired allocation metadata")
        vm.run("fsck hd1p1", b"ext2 clean")
        vm.poweroff()

    corrupt = work / "corrupt.img"
    shutil.copyfile(image, corrupt)
    with corrupt.open("r+b") as file:
        file.seek(offset+1024+56)
        file.write(b"\0\0")
    with guest("corrupt-root", [corrupt], "ata") as vm:
        vm.boot()
        vm.see(b"root ext2 unavailable or invalid")
    clone = work / "duplicate.img"
    shutil.copyfile(image, clone)
    with guest("duplicate-root", [image, clone], "ata") as vm:
        vm.boot()
        vm.see(b"more than one disk matches the boot partition")
    print("verified recovery copy booted and malformed or ambiguous roots were rejected", flush=True)


def run(args, work):
    @contextmanager
    def guest(label, drives, bus):
        machine = args.sata_machine if bus == "sata" else args.ata_machine
        progress(f"{label}: launching QEMU ({machine})")
        vm = Guest(args, label, work / (label+".log"), drives, machine)
        try:
            yield vm
        finally:
            vm.close()

    for bus in ("ata", "sata"):
        progress(f"{bus}: preparing disposable disk images")
        image = install(args, work, guest, bus)
        normal(args, work, guest, bus, image)
        failures(args, work, guest, bus, image)
        if bus == "ata":
            crash_and_recover(args, work, guest, image)
    print("PROJECT9 STORAGE TESTS PASSED")


def usbtests(args, work):
    for name, model in (("ohci", "pci-ohci"), ("ehci", "usb-ehci")):
        media = work / (name+"-root.img")
        first = work / (name+"-first.img")
        second = work / (name+"-second.img")
        port = work / (name+"-qmp")
        shutil.copyfile(args.media, media)
        first.write_bytes(b"native usb first disk\n".ljust(4096, b"\0"))
        second.write_bytes(b"native usb replacement disk\n".ljust(4096, b"\0"))
        original = digest(first), digest(second)
        fifo_in, fifo_out = port.with_suffix(".in"), port.with_suffix(".out")
        # qemu monitor messages use local pipes and never need a listener
        os.mkfifo(fifo_in, 0o600)
        os.mkfifo(fifo_out, 0o600)
        with os.fdopen(os.open(fifo_in, os.O_RDWR), "wb", buffering=0) as stdin, \
             os.fdopen(os.open(fifo_out, os.O_RDWR), "rb", buffering=0) as stdout:
            extras = ["-device", model+",id=host",
                      "-drive", f"file={first},format=raw,if=none,id=first",
                      "-drive", f"file={second},format=raw,if=none,id=second",
                      "-device", "usb-storage,id=stick,bus=host.0,drive=first",
                      "-qmp", f"pipe:{port}"]
            progress(f"{name}: preparing native USB read and hot-plug checks")
            vm = Guest(args, name, work/(name+".log"), [media], args.ata_machine, extras)
            try:
                vm.boot()
                vm.login(first=True)
                vm.run("sysctl hw.usb", rb"\nhw.usb=usb0\r?\n")
                vm.run("head -1 /dev/usb0", rb"\nnative usb first disk\r?\n")
                vm.run("echo write-attempt > /dev/usb0")

                pending = b""
                def request(command, arguments=None):
                    nonlocal pending
                    message = {"execute": command, "id": command}
                    if arguments is not None:
                        message["arguments"] = arguments
                    stdin.write(json.dumps(message).encode()+b"\n")
                    deadline = time.monotonic() + 10
                    while time.monotonic() < deadline:
                        if b"\n" in pending:
                            line, pending = pending.split(b"\n", 1)
                            reply = json.loads(line)
                            if reply.get("id") == command:
                                if "error" in reply:
                                    raise RuntimeError(f"{name} QMP {command}: {reply['error']}")
                                return
                        elif select.select([stdout], [], [], max(0, deadline-time.monotonic()))[0]:
                            pending += os.read(stdout.fileno(), 4096)
                    raise RuntimeError(f"{name}: QMP {command} timed out")

                request("qmp_capabilities")
                request("device_del", {"id": "stick"})
                vm.see(b"usb: detached /dev/usb0")
                vm.run("sysctl hw.usb", rb"\nhw.usb=\r?\n")
                request("device_add", {"driver": "usb-storage", "id": "replacement",
                                       "bus": "host.0", "drive": "second"})
                vm.see(b"/dev/usb1 read-only")
                vm.run("sysctl hw.usb", rb"\nhw.usb=usb1\r?\n")
                vm.run("head -1 /dev/usb0", b"cannot open /dev/usb0")
                vm.run("head -1 /dev/usb1", rb"\nnative usb replacement disk\r?\n")
                vm.poweroff()
            finally:
                vm.close()
        if original != (digest(first), digest(second)):
            raise RuntimeError(f"{name}: guest modified a read-only USB disk")
        progress(f"{name}: native USB reads and hot-plug passed")
    progress("PROJECT9 USB TESTS PASSED")


def run_with_logs(args, work, keep_images):
    progress(f"Project9 storage tests starting. Working directory {work}")
    try:
        if args.usb:
            usbtests(args, work)
        else:
            run(args, work)
        return 0
    except (OSError, ValueError, IndexError, RuntimeError,
            subprocess.SubprocessError, KeyboardInterrupt) as error:
        if keep_images:
            logs = work
        else:
            logs = Path(tempfile.mkdtemp(prefix="storage-test-logs-", dir=".")).resolve()
            for log in work.glob("*.log"):
                shutil.copy2(log, logs / log.name)
        print(f"storage-test: {error if not isinstance(error, KeyboardInterrupt) else 'interrupted'}",
              file=sys.stderr, flush=True)
        progress(f"Serial logs saved in {logs}")
        return 130 if isinstance(error, KeyboardInterrupt) else 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Test Project9 storage on disposable VM disks")
    parser.add_argument("--media", default="release/dev/install.img")
    parser.add_argument("--qemu", help="override the QEMU executable")
    parser.add_argument("--e2fsck", default="e2fsck")
    parser.add_argument("--debugfs", default="debugfs")
    parser.add_argument("--ata-machine", default="pc")
    parser.add_argument("--sata-machine", default="q35")
    parser.add_argument("--python", default=sys.executable)
    parser.add_argument("--x86-64", dest="x86_64", action="store_true", help="boot the x86_64 kernel")
    parser.add_argument("--usb", action="store_true", help="test the native OHCI and EHCI USB paths")
    parser.add_argument("--require-smp", action="store_true", help="require execution on at least two guest CPUs")
    parser.add_argument("--serial", action="store_true", help="show the guest serial console live")
    parser.add_argument("--timeout", type=int, default=300, help="seconds to wait for each guest response")
    parser.add_argument("--workdir", type=Path, help="retain logs and test images in this directory")
    args = parser.parse_args()
    if args.qemu is None:
        args.qemu = "qemu-system-x86_64" if args.x86_64 else "qemu-system-i386"
    if args.timeout < 1:
        parser.error("--timeout must be at least 1 second")
    media = Path(args.media)
    try:
        if not media.is_file() or not partition(media):
            parser.error("--media must be a Project9 release disk image")
    except (OSError, ValueError) as error:
        parser.error(str(error))
    executables = (args.qemu,) if args.usb else (args.qemu, args.e2fsck, args.debugfs, args.python)
    for executable in executables:
        if not shutil.which(executable):
            parser.error(f"required executable not found: {executable}")
    if args.workdir:
        try:
            existed = args.workdir.exists()
            args.workdir.mkdir(parents=True, exist_ok=True)
            work = args.workdir.resolve()
            if existed:
                work = Path(tempfile.mkdtemp(prefix="run-", dir=work))
        except OSError as error:
            parser.error(str(error))
        sys.exit(run_with_logs(args, work, keep_images=True))
    with tempfile.TemporaryDirectory(prefix="storage-test-", dir=".") as temporary:
        sys.exit(run_with_logs(args, Path(temporary).resolve(), keep_images=False))
