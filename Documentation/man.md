<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Manual pages and pager

In Project9, try `man ls` for a command, `man 2 execve` for a system
call, or `apropos directory` to find pages by topic. Section 1 covers
commands, section 2 covers system calls, and section 5 covers file formats.

| Key | Action |
| --- | --- |
| Up, Down | Scroll one line |
| Page Up, Page Down | Scroll one screen |
| Home, End | Go to the beginning or end |
| k, j | Scroll one line up or down |
| b, Space | Scroll one screen up or down |
| g, G | Go to the beginning or end |
| Ctrl+L | Redraw |
| q | Quit |
| Ctrl+Z, then `fg` | Suspend and resume at the same position |

The pager works in the QEMU keyboard window and over an ANSI serial
terminal. It reads the viewport from the terminal when opened. The
default is 80 columns by 24 rows. Use `stty rows` and `stty columns`
to adjust it within the active display limits. The status line counts display lines,
including wrapped lines. Redirecting input or output disables paging.
For example, `man ls > /tmp/ls.txt` saves the plain page.

Pages live in `rootfs/usr/share/man` in the source tree. Put the command name and a
short description below the source license notice so `apropos` can find it. Use
SYNOPSIS to show the accepted invocation, DESCRIPTION to explain what
it does, and SEE ALSO to point to related pages. Document options and
exit statuses only where the command supports them.

## Install on an existing disk

Quit QEMU and copy your current `fs.img` into the newly extracted
`project9` directory. Then run on the host:

```sh
make update-fs
make qemu
```

The updater installs the current programs and manual pages and saves the
original disk as `fs.pre-dev.img`. Your accounts, passwords and other
files stay on the updated disk. The pager uses a new system call, so use
the updated kernel with the updated programs.

For a fresh disk, run `make qemu` in the extracted directory without
copying in an old `fs.img`.
