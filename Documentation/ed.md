<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# UNIX V6 ed

Run `ed [file]`. The editor has no prompt. A bare `?` means the last
command failed. The port retains the supplied V6 command interpreter and
regular-expression engine, adapts error recovery to the 32-bit user
runtime, and stores its scratch text in memory. No temporary file is left
on the guest disk.

To create and edit a file, enter these lines inside the editor:

```text
a
one
two
.
1,2p
2s/two/TWO/
w notes
q
```

`a`, `i`, and `c` collect lines until a line consists of one dot. `p` and
`l` print, `d` deletes, `m` moves, and `t` copies. `r file` reads a file,
`w [file]` writes the selected lines and truncates the destination, and
`e file` replaces the editing buffer. `!` followed by a command executes
it using `/bin/sh -c`. Use `q` or Ctrl+D at the command prompt to exit.
This V6 editor can discard unsaved changes on exit. Ctrl+C returns to the
command loop and preserves the lines already entered. Ctrl+\ is ignored
by default. Use `ed -q` for the default SIGQUIT dump behavior. Use
`ed -` to suppress byte counts.

Addresses include `.` (current), `$` (last), numeric line numbers, search
patterns delimited by `/` or `?`, and ranges such as `1,$`. Text input is
7-bit, with lines of up to 511 bytes from files. Scratch space is 128 KiB
across 32,768 indexed lines. The legacy xv6 filesystem has a 70 KiB file
limit. The ext2 profile supports larger files within the editor limits above. The V6 source is covered by the
[Caldera UNIX license](V6-license.txt). The starting point for this port
was the supplied `ed-v6-master.zip`.
