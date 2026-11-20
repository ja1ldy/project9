<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Console and job control

Both the PS/2 keyboard and serial input feed the same TTY. In canonical
mode the console commits a line on Enter. These shortcuts use the ASCII bytes shown
below. The serial terminal must pass the control bytes through to the guest.

| Shortcut | Hex | Effect |
| --- | --- | --- |
| Ctrl+C | `03` | Sends SIGINT to the foreground process group. The default terminates the process. `ed` catches it and returns to the command loop. |
| Ctrl+\ | `1c` | Sends SIGQUIT. The default attempts a core dump, then terminates. |
| Ctrl+Z | `1a` | Sends SIGTSTP. The shell regains the console when all remaining processes in the foreground group have stopped. |
| Ctrl+D | `04` | Queues an EOF marker. A read returns buffered text first. A subsequent read returns zero bytes. At an empty shell line this signs out. |
| Ctrl+H or Backspace | `08` or `7f` | Erases the last uncommitted byte. |
| Ctrl+U | `15` | Erases the uncommitted input line. |
| Ctrl+W | `17` | Erases trailing spaces and the preceding word. |
| Ctrl+S | `13` | Pauses user output to the console. |
| Ctrl+Q | `11` | Resumes console output. |
| Ctrl+R | `12` | Starts a new screen line and reprints the pending input. |
| Ctrl+V | `16` | Quotes the next input byte. For example, Ctrl+V Ctrl+D inserts byte `04`. |
| Ctrl+L | `0c` | Clears the display and redraws pending input. Serial output uses ANSI clear and home. |

`command &` starts a background job. `jobs` lists running and stopped jobs.
`fg` resumes the most recent job. `fg PID` chooses the displayed job ID.
Pipeline members share a process group. When a session ends, init terminates
its remaining processes before starting a new login. Background reads from
the console fail. The system has one controlling console. Programs can
catch or ignore SIGINT and SIGQUIT. SIGTSTP supports default and ignore
behavior. For other signals and child status, see the
[v0.2 process notes](history/v0.2.md#child-status-and-signals).

## Terminal attributes

Use `stty -a` to inspect the console and `stty sane` to restore the
default input modes. The table above shows the default key bindings.
Applications can inspect and change input modes with `tcgetattr` and
`tcsetattr`. See [the terminal notes](history/v0.4a.md).

## Immediate console input

The manual pager uses cbreak mode for immediate, unechoed input. The
console still handles job-control signals and output flow control.
A suspended pager gives the shell its normal line editing, then takes
back its input mode on `fg`. See [the pager guide](man.md) or `man 2 ttyctl`.

## Core dumps

The kernel attempts to create a new private `core.PID` in the dying process's
working directory. It leaves an existing filename intact. A read-only
directory or full disk can prevent a complete dump. Login and password tools
ignore SIGQUIT while handling credentials, and `ed` ignores it unless
invoked with `-q`.

The format is defined in `include/core.h`. It begins with the four bytes
`PCR9`, version 1, process metadata and the x86 trap frame. The payload is
the saved portion of the virtual address space starting at address zero,
with inaccessible pages zero-filled. The dump writer caps
the header and payload at 64 KiB. Check the actual payload length if the
kernel reports an incomplete dump.

## Framebuffer

The driver selects a 1024×768, 32-bit Bochs VBE display where available.
The guest draws printable ASCII using the Gallant 12×22 bitmap from NetBSD,
in a 12×24 cell. Serial fonts depend on the host terminal. On systems
without compatible VBE hardware the console uses VGA text mode.

When the adapter exposes at least two screen heights, the framebuffer uses
two mirrored copies of a circular scanline range. Scrolling clears two
copies of the new text row and updates the hardware Y offset. It does no
full-screen framebuffer reads on this path. The single-height fallback
uses a bulk copy. The console also skips VGA text cursor I/O while the
framebuffer is active, updates the cursor once per output batch, and the
user runtime batches formatted writes.

[NetBSD Gallant source](https://github.com/NetBSD/src/blob/trunk/sys/dev/wsfont/gallant12x22.h) · [License](Gallant-license.txt)
