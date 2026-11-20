<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Tiny-X desktop preview

Boot the ext2 release under either kernel with a graphical console. Log in
as root and run:

```sh
stability desktop
p9font --check
startx
```

`startx` invokes `xinit`. It starts Tiny-X on display `:0` and opens
FVWM with a terminal running the regular Project9 shell on a PTY. From
that shell, try `stty -a`, `vi /tmp/notes`, `p9font`, `xdpyinfo` and
`xsetroot -solid '#25495e'`. Select **End X session** from the desktop
menu or press Ctrl+Alt+Esc to restore the console.
After an unclean exit, the socket inode may remain. Confirm the server
has exited before removing `/tmp/.X11-unix/X0`.

`tinyx :0` can also be started directly. A client launched separately
can use `DISPLAY=:0`. The server listens at `/tmp/.X11-unix/X0` on
AF_UNIX. The pathname permits access by the display owner. The current
framebuffer and input device permissions normally require root to run
the server. The server continues to support its pipe-client demo
and `tinyx --check`.

## Shipped clients

| Program | Included features |
| --- | --- |
| `xinit`, `startx` | Start display zero with the terminal, FVWM or an explicitly named client |
| `fvwm` | OpenBSD's FVWM 2.2.5 core with window frames, focus, movement, resize and a desktop menu |
| `xterm` | Upstream VT terminal emulator with a Project9 PTY |
| `p9term` | Shell window with PTY, ASCII fixed-font text, basic VT cursor commands and colors |
| `vi` | Neatvi 09 port with the vi and ex editing modes |
| `p9font` | FreeType 2.14.3 TrueType rasterization with the bundled DejaVu Sans Mono font or a local font file |
| `xdpyinfo` | X11 version and root screen geometry |
| `xsetroot` | Upstream X.Org root colors and patterns |
| `xset` | Upstream X.Org core display settings |
| `xhost` | Upstream X.Org access control requests for the local display |
| `xmodmap` | Upstream X.Org keyboard and modifier maps |
| `xrdb` | Upstream X.Org resource database with plain resource files |
| `xeyes`, `xclock` | Upstream X.Org Athena clients |
| `xwininfo` | Root or window geometry and optional children |
| `xprop` | Read the focused window's name, icon name and class. Use `-root` or `-id` to select another window |
| `xkill` | Close a client by window ID |
| `xrandr` | Query RandR mode information |

The remaining compact display utilities use Project9's protocol client code.
FVWM, xterm and the imported X.Org clients use X.Org Xlib. The desktop programs are static ELF32
executables and run on the x86_64 kernel in IA32 compatibility mode.

Run `clear` to clear the terminal window. `man sleep`, `less /etc/motd`
and `cat /etc/motd | less` page inside the graphical terminal. Press
`q` to quit the pager. `sleep 30` waits in seconds. Use `sleep -t 30`
for the old tick-based behavior.

## Kernel interfaces

* AF_UNIX stream sockets are filesystem entries on ext2. They provide
  bind, listen, accept, connect, shutdown, select and poll. Pathname
  unlinking affects future connections while open connections retain
  their endpoints. AF_INET connects in-kernel streams to IPv4
  `127.0.0.1` on explicit ports. The loopback implementation has no
  external network or packet-level TCP.
* PTYs are allocated by `openpty` or through the named
  `/dev/ptyp0` … `/dev/ptyp7` and `/dev/ttyp0` … `/dev/ttyp7`
  pairs. The slave supports a controlling session, raw and canonical
  reads with `VTIME=0`, and window-size and termios ioctls.
  The master reports resize and input signals to the foreground
  process group. A background job reading from its controlling PTY
  stops with SIGTTIN.
* Private anonymous and snapshot file mappings use a roughly 1 GiB
  reserved arena below `0x80000000`, with up to 64 records and
  128 MiB per mapping subject to available physical RAM. They allocate
  pages eagerly. Partial unmap works within a single existing mapping.
* Ext2 supports direct and single-, double- and triple-indirect blocks.
  The signed offset ABI limits a file to just under 2 GiB, subject to
  available disk space. The release validator caps each executable
  slice at 64 MiB. The older xv6 filesystem profile retains its
  original 70 KiB file limit.

## Writable disks

`make os` defaults to a 512 MiB root for a new writable ext2 disk.
Existing configurations and disks retain their size. Select a new root
size in the settings or create a fresh disk. Use the disk boot option
on a 512 MiB image. The distribution image uses a separate small root
so the RAM-live options fit in memory.

## FVWM session

Plain `startx` runs `$HOME/.xinitrc` when present. The release includes
`/root/.xinitrc` and a system fallback at `/etc/X11/xinit/xinitrc`.
Copy the system script to `~/.xinitrc` to customize the session. You can
also point `XINITRC` to a different script. Keep the session's main
program in the foreground, for example `exec fvwm -f /etc/fvwmrc`.

Click the desktop or press Ctrl+F12 to open the Project9 menu. Press
`t` for another terminal, `x` for an xterm, or `e` to end the session.
`startx --fvwm` selects the system FVWM session. Use `startx --fvwm xterm`
to launch an xterm in that session, or `startx fvwm -f /path/to/fvwmrc`
to launch a separate configuration. The optional `--` still separates
client arguments from X server arguments.

This build imports the FVWM core from OpenBSD's Xenocara tree at
`7456bbaae1fa092bdb63696d50646206d4983d7c`. It uses a Project9 shading
helper and the included local-socket Xlib build. The upstream notices are
in `/usr/X11/NOTICES`. The shipped build uses core fonts and contains the
window manager and the Project9 menu. Optional FVWM modules, XPM icons
and Shape are outside this build.

Tiny-X continues to use the boot framebuffer and the existing input
drivers. The local Xlib build supports a single local display and core
font handling.

## X.Org Xlib and xterm

The ext2 release includes X.Org libX11 1.1.5 and upstream xterm patch 412.
Xterm uses Xt and Athena widgets and opens a Project9 PTY for the shell.
It runs on the i386 kernel and through IA32 compatibility on the x86_64
kernel. Start an xterm session with

```sh
startx xterm
```

In FVWM, open the Project9 menu and select **XTerm**. From a terminal
inside X, run `xterm -version` to see the upstream version.
Run `stty -a` in xterm to see the PTY dimensions. Type `exit` in the shell
to close the window.

`make x11-sdk` builds `libX11.a` and installs the public X11 headers
into the architecture's SDK. The release includes both in
`sdk.tar.gz`. Build an Xlib client with the SDK's i386 compiler
flags and link `crt0.o`, the client object and `libX11.a` before
`libc.a` and `libm.a` in a linker group. Use the SDK's
`user.ld` as the linker script. For example, from the source directory
after `make ARCH=i386 x11-sdk libc`:

```sh
sdk=build/i386/sdk
gcc -m32 -ffreestanding -fno-pie -fno-pic -fno-stack-protector \
  -nostdinc -isystem "$(gcc -print-file-name=include)" \
  -I"$sdk/include" -c client.c -o client.o
ld -m elf_i386 -N -s --gc-sections -T "$sdk/user.ld" -o client \
  "$sdk/crt0.o" client.o --start-group \
  "$sdk/libX11.a" "$sdk/libc.a" "$sdk/libm.a" --end-group
```

The port uses static linking, the local display socket, core X fonts
and the C locale. Upstream xterm features that need Xft, XIM, a system
terminfo database or an X session manager are outside this build. The
SDK is an i386 Xlib SDK for both release profiles. Upstream notices
are included in the X11 release directories and the SDK.


## X.Org tools and terminal updates

The original X.Org clients are statically linked with the Project9 X11 SDK.
Xinit uses Project9 process groups and waits for the local display socket.
The default `PATH` includes `/usr/X11/bin`. The `.xinitrc` file and
the `--` delimiter follow BSD/X.Org conventions. Project9 also accepts
client names from `PATH` and the `--fvwm` shortcut. The supplied
`.xinitrc` uses standard `sh` syntax and can run on a BSD system with
FVWM installed. `xrdb` loads and merges plain X resource files without
a C preprocessor. The kernel's local socket permissions govern access
to the display.

Cooked PTYs enable `OPOST | ONLCR` by default. The slave stream turns a
line feed into a carriage return followed by a line feed, including echo.
Raw mode leaves line feeds untouched. Tiny-X renders to normal RAM and
copies damage rectangles into the mapped framebuffer one row at a time.
Idle polling makes no framebuffer copy. Large row copies use the SDK's
cached CPUID and OS state check to select AVX, SSE or the integer fallback.
The byte rate depends on the hardware framebuffer and its memory type.

## Process monitor

`top` comes from OpenBSD's port of the original Berkeley Unix process
monitor. Its Project9 backend uses the process snapshot syscall and
reports online CPUs, free memory and uptime. CPU usage counters and
load averages are unavailable. Use `top -b -d 1` for one plain-text
snapshot or `top` in a VT100-style terminal for the interactive view.

The SDK includes a narrow `libcurses.a` with window and cursor operations,
basic attributes, screen updates, input and resize. It follows the public
behavior of the relevant OpenBSD routines. The terminal database is
limited to the built-in ANSI and VT100-compatible entries.

The Project9 logo and X resource overrides for the Xenodm port are in
`usr/x11/apps/xenodm`. The release boots to console login while the
authenticated display-manager handoff is under development.

On the x86_64 QEMU guest, leaving FVWM with both terminals open can
fault the console shell and return to the login prompt. Standalone
xterm sessions returned to the console in the QEMU tests.
