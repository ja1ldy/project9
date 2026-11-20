<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Tiny-X on Project9 dev

Tiny-X is an X11 server in the ext2 release. It opens a 32-bit framebuffer,
reads the Project9 keyboard and relative pointer devices, and returns to
console mode when the framebuffer is unmapped. It is started manually from
the shell.

## Start a session

Boot `release/dev-x86_64/install.img` with a graphical display. Use option 7
for an x86_64 RAM-live boot or option 5 for i386. Sign in as root and run

```sh
/usr/X11/bin/tinyx --check
/usr/X11/bin/tinyx
```

The first command starts a headless server and a separate client. The client
creates and maps a window, paints with X11 requests, then reads a pixel back
with GetImage. It prints `Tiny-X X11 rendering check passed` on success.

The second command maps `/dev/fb0` and starts an interactive X11 demo. Click
inside the panel or press a letter to change its accent. Press Esc to return
to the console. The keyboard is reserved for the graphics session while the
framebuffer mapping is writable. Start one graphics session at a time.

The keyboard uses the upstream US core keymap. Tiny-X uses `/dev/mouse` for
relative motion and buttons, with `/dev/touchpad` as a fallback. The server
can run with a keyboard and no pointer device. The framebuffer, keyboard,
and pointer nodes use the usual Project9 permissions.

## Run a client

```sh
/usr/X11/bin/tinyx /absolute/path/to/client arg
```

The launcher starts one process and exchanges raw X11 bytes with it over
private pipes. The client writes requests to stdout, reads replies and events
from stdin, and uses stderr for diagnostics. Send an ordinary X11 setup
request in little- or big-endian order. The server session ends after the
client exits.

The server also has an independent-client mode on the UNIX display socket.
See the [desktop guide](desktop.md) for `startx`, the PTY terminal, TrueType
and the set of bundled X11 clients.

## Build and source

`make` builds the program with the Project9 SDK. `make release` includes
it in the ext2 images and base set. `/bin` remains universal in the hybrid
image. The graphics server is installed in `/usr/X11/bin` as a stripped
i386 ELF and runs under the x86_64 kernel's IA32 compatibility layer.

The server is the embeddable TinyX upstream at commit
`db4a12b4d2ca65420432ad59f62200a42a4556e3`. Its source is under
`usr/x11/vendor/tinyx`. The port uses the memory backend, a Project9 host
glue layer in `usr/x11`, and X.Org protocol headers from xorgproto 2024.1.
The sources include the original notices and the X.Org inputs for the
embedded font data. The combined Tiny-X executable is GPL-3.0-only.

- [TinyX upstream revision](https://github.com/pgavlin/tinyx/tree/db4a12b4d2ca65420432ad59f62200a42a4556e3)
- [X.Org protocol releases](https://www.x.org/releases/individual/proto/)
