# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

import os
from pathlib import Path
import re
import sys

try:
    import curses
except ImportError:
    curses = None

from osimage import FILESYSTEMS, FORMATS, checked_root, image_kind
from installimage import partition
from setup_accounts import password_digest, read_guest


CPU_MODELS = ("default", "486", "pentium", "pentium3", "qemu32", "max")
CANCEL = (27, 3)


class Menu:
    def __init__(self, screen, setup, configure):
        self.screen, self.setup, self.config = screen, setup, setup.config
        self.configure = configure
        self.normal, self.title, self.selected, self.desktop = curses.A_NORMAL, curses.A_BOLD, curses.A_REVERSE, 0
        if curses.has_colors():
            curses.start_color()
            curses.init_pair(1, curses.COLOR_WHITE, curses.COLOR_BLUE)
            curses.init_pair(2, curses.COLOR_BLACK, curses.COLOR_WHITE)
            curses.init_pair(3, curses.COLOR_WHITE, curses.COLOR_BLUE)
            self.desktop = curses.color_pair(1)
            self.normal = curses.color_pair(2)
            self.title = self.desktop | curses.A_BOLD
            self.selected = curses.color_pair(3) | curses.A_BOLD
        screen.bkgd(" ", self.desktop)
        self.cursor(0)

    @staticmethod
    def cursor(state):
        try:
            curses.curs_set(state)
        except curses.error:
            pass

    def panel(self, title, subtitle, footer):
        s = self.screen
        while True:
            h, w = s.getmaxyx()
            s.attrset(self.desktop)
            s.erase()
            if w >= 44 and h >= 14:
                break
            s.addnstr(0, 0, "Resize to continue   Esc to exit", max(w - 1, 0))
            s.refresh()
            if s.getch() in CANCEL:
                raise KeyboardInterrupt()
        s.addnstr(0, 1, "Project9 dev configuration", w - 2, self.title)
        s.hline(1, 0, curses.ACS_HLINE, w)
        width, height = min(w - 4, 78), min(h - 4, 22)
        panel = s.derwin(height, width, (h - height) // 2, (w - width) // 2)
        panel.bkgd(" ", self.normal)
        panel.erase()
        panel.box()
        panel.addnstr(0, 3, " " + title + " ", width - 6, self.normal | curses.A_BOLD)
        panel.addnstr(2, 2, subtitle, width - 4, self.normal)
        panel.hline(height - 3, 1, curses.ACS_HLINE, width - 2)
        panel.addnstr(height - 2, 2, footer, width - 4, self.normal)
        return panel, height, width

    def choose(self, title, rows, subtitle="", index=0):
        while True:
            panel, h, w = self.panel(title, subtitle, "↑↓ Move    Enter Select    Esc Back")
            visible = h - 7
            start = min(max(0, index - visible + 1), max(0, len(rows) - visible))
            for offset, (name, _) in enumerate(rows[start:start + visible]):
                attr = self.selected if offset + start == index else self.normal
                panel.addnstr(4 + offset, 2, name.ljust(w - 4), w - 4, attr)
            self.screen.refresh()
            panel.refresh()
            key = self.screen.getch()
            if key in CANCEL or key == ord("q"):
                return None, index
            if key in (curses.KEY_UP, ord("k")):
                index = (index - 1) % len(rows)
            elif key in (curses.KEY_DOWN, ord("j"), 9):
                index = (index + 1) % len(rows)
            elif key == curses.KEY_HOME:
                index = 0
            elif key == curses.KEY_END:
                index = len(rows) - 1
            elif key == curses.KEY_PPAGE:
                index = max(0, index - visible)
            elif key == curses.KEY_NPAGE:
                index = min(len(rows) - 1, index + visible)
            elif key in (10, 13, curses.KEY_ENTER, ord(" ")):
                return rows[index][1], index

    def edit(self, title, current="", *, secret=False, limit=255):
        text, index = current, len(current)
        self.cursor(1)
        try:
            while True:
                hint = "Password input is masked" if secret else "Edit the value and press Enter"
                panel, h, w = self.panel(title, hint, "Enter Accept    Esc Cancel")
                field = w - 8
                start = max(0, index - field + 1)
                shown = "*" * len(text) if secret else text
                panel.addnstr(5, 4, shown[start:start + field].ljust(field), field, self.selected)
                panel.move(5, 4 + index - start)
                self.screen.refresh()
                panel.refresh()
                panel.keypad(True)
                key = panel.get_wch()
                if key in ("\x1b", "\x03"):
                    return None
                if key in ("\n", "\r", curses.KEY_ENTER):
                    return text
                if key in ("\b", "\x7f", curses.KEY_BACKSPACE) and index:
                    text, index = text[:index - 1] + text[index:], index - 1
                elif key == curses.KEY_DC:
                    text = text[:index] + text[index + 1:]
                elif key == curses.KEY_LEFT:
                    index = max(0, index - 1)
                elif key == curses.KEY_RIGHT:
                    index = min(len(text), index + 1)
                elif key == curses.KEY_HOME:
                    index = 0
                elif key == curses.KEY_END:
                    index = len(text)
                elif key == "\x15":
                    text, index = "", 0
                elif isinstance(key, str) and key.isprintable() and len(text) < limit:
                    text, index = text[:index] + key + text[index:], index + 1
        finally:
            self.cursor(0)

    def message(self, title, text):
        self.choose(title, [("< OK >", True)], str(text))

    def confirm(self, title, text):
        result, _ = self.choose(title, [("< No >", False), ("< Yes >", True)], text)
        return result

    def number(self, name, key, low, high):
        text = self.edit(name, str(self.config[key]))
        if text is None:
            return
        if not text.isascii() or not text.isdecimal() or not low <= int(text) <= high:
            raise ValueError(f"Choose a value from {low} to {high}.")
        self.config[key] = int(text)

    def option(self, title, key, values):
        value, _ = self.choose(title, [(str(v), v) for v in values],
                               index=values.index(self.config[key]))
        if value is not None:
            self.config[key] = value

    def filesystem(self):
        index = 0
        while True:
            c = self.config
            rows = [(f"({c['filesystem']})  Root filesystem", "filesystem")]
            if c["filesystem"] == "ext2":
                rows.extend(((f"({c['size']} MiB)  New root disk size", "size"),
                             (f"({c['block']} bytes)  New root block size", "block")))
            rows.append((f"({c['data'] or 'detached'})  Optional ext2 data image", "data"))
            choice, index = self.choose("Filesystem", rows, "New disk geometry applies at creation", index)
            if choice is None:
                return
            try:
                if choice == "filesystem":
                    self.option("Root filesystem", choice, FILESYSTEMS)
                    index = 0
                elif choice == "size":
                    self.number("New root disk size in MiB", choice, 8, 4096)
                elif choice == "block":
                    self.option("Ext2 block size in bytes", choice, FORMATS)
                elif (value := self.edit("Secondary ext2 disk path", c["data"])) is not None:
                    if value:
                        path = Path(value)
                        if partition(path):
                            raise ValueError("Select a standalone ext2 image as the secondary disk.")
                        checked_root(path)
                        if not self.configure and self.setup.root.exists() and path.samefile(self.setup.root):
                            raise ValueError("Choose a separate disk image.")
                    c["data"] = value
            except (OSError, ValueError) as error:
                self.message("Filesystem setting", error)

    def machine(self):
        index = 0
        while True:
            c = self.config
            rows = [(f"({c['cpus']})  Virtual CPUs", "cpus"),
                    (f"({c['ram']} MiB)  Guest RAM", "ram"),
                    (f"({c['display']})  Display", "display"),
                    (f"({c['cpu']})  QEMU CPU model", "cpu")]
            choice, index = self.choose("QEMU settings", rows, "Machine settings apply at the next boot", index)
            if choice is None:
                return
            try:
                if choice == "cpus":
                    self.number("Virtual CPU count", choice, 1, 8)
                elif choice == "ram":
                    self.number("Guest RAM in MiB", choice, 16, 4096)
                elif choice == "display":
                    self.option("Display", choice, ("window", "serial"))
                else:
                    self.option("CPU model", choice, CPU_MODELS)
            except ValueError as error:
                self.message("QEMU setting", error)

    def account(self, name):
        first = self.edit(f"Set password for {name}", secret=True, limit=96)
        if first is None:
            return
        second = self.edit("Retype new password", secret=True, limit=96)
        if second is None:
            return
        if first != second:
            raise ValueError("The two passwords did not match.")
        self.setup.pending[self.setup.filesystem][name] = password_digest(first)

    def accounts(self):
        index = 0
        while True:
            users = self.setup.accounts()
            pending = self.setup.pending[self.setup.filesystem]
            rows = [("Add user  --->", "+")]
            for name in dict.fromkeys(["root", *users]):
                suffix = "  [pending]" if name in pending else ""
                rows.append((f"Set password for {name}{suffix}  --->", name))
            choice, index = self.choose("Users and passwords", rows, "Passwords are applied to the offline disk", index)
            if choice is None:
                return
            try:
                if choice == "+":
                    choice = self.edit("New username", limit=14)
                    if choice is None:
                        continue
                    if not re.fullmatch(r"[a-z][a-z0-9_-]{0,13}", choice) or choice in users or choice == "root":
                        raise ValueError("Choose a new name of up to 14 lowercase letters, digits, _ or -.")
                    if "root" not in users:
                        raise ValueError("Set the root password before adding another user.")
                self.account(choice)
            except ValueError as error:
                self.message("Account setting", error)

    def general(self):
        current = self.config["hostname"]
        if not self.configure and not current and self.setup.root.exists():
            current = read_guest(self.setup.root, "/etc/hostname", self.setup.args.debugfs).decode().strip()
        name = self.edit("Guest hostname", current, limit=63)
        if name is None:
            return
        if name and not re.fullmatch(r"[A-Za-z0-9](?:[A-Za-z0-9.-]{0,61}[A-Za-z0-9])?", name):
            raise ValueError("Use up to 63 letters, digits, hyphens or dots.")
        self.config["hostname"] = name

    def main(self):
        index = 0
        while True:
            root = self.setup.root if not self.configure else None
            state = image_kind(root) if root and root.exists() else "new"
            rows = [("Filesystem  --->", "fs"), ("QEMU settings  --->", "vm"),
                    ("Hostname  --->", "host")]
            if not self.configure:
                rows.append(("Users and passwords  --->", "users"))
            rows.append(("Save configuration", "save"))
            if not self.configure:
                if root.exists() and (root.name == "install.img" or self.setup.filesystem == "xv6-fs"):
                    rows.append(("Apply users and hostname to disk", "apply"))
                rows.extend((("Build boot and root images", "prepare"),
                             ("Start Project9", "boot"), ("Check disks", "check")))
            rows.append(("Exit", "exit"))
            index = min(index, len(rows)-1)
            choice, index = self.choose("Main Menu", rows, f"{self.setup.filesystem}    {root}    {state}" if root else self.setup.filesystem, index)
            try:
                if choice in (None, "exit"):
                    if any(self.setup.pending.values()) and not self.confirm("Exit", "Discard pending password changes?"):
                        continue
                    return None
                if choice == "fs":
                    self.filesystem()
                elif choice == "vm":
                    self.machine()
                elif choice == "host":
                    self.general()
                elif choice == "users":
                    self.accounts()
                elif choice == "save":
                    self.setup.save()
                    self.message("Save configuration", "Saved .project9.json")
                else:
                    if choice in ("boot", "prepare") and state == "legacy" and self.setup.filesystem == "ext2":
                        if not self.confirm("Convert root disk", "Import to install.img and keep the original?"):
                            continue
                        self.setup.confirmed.add(root)
                    if choice in ("boot", "prepare", "apply"):
                        self.setup.save()
                    return choice
            except (OSError, ValueError, KeyError) as error:
                self.message("Project9 setup", error)


def menu(setup, configure=False):
    if curses is None or not sys.stdin.isatty() or not sys.stdout.isatty() or os.environ.get("TERM") in (None, "", "dumb"):
        raise ValueError("Use a color terminal with ncurses or run tools/ossetup.py with --boot.")
    return curses.wrapper(lambda screen: Menu(screen, setup, configure).main())
