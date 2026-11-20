<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Commands and shell

The default `PATH` is `/bin`. Add directories to `PATH` or use a command
name with a slash, such as `./name`. The shell supports quotes, variables,
pipelines, file and descriptor redirection, conditional lists, scripts,
and background jobs. See the [v0.3 shell notes](history/v0.3.md#use-the-shell) or
run `man sh` in the guest.

| Added command | Usage |
| --- | --- |
| `basename` | `basename path [suffix]` |
| `cmp` | `cmp file1 file2` reports the first differing byte or an early EOF |
| `dirname` | `dirname path` |
| `rmdir` | `rmdir dir ...` removes empty directories |
| `seq` | `seq [first] last` prints nonnegative integers |
| `sleep` | `sleep seconds` waits in seconds. `sleep -t ticks` accepts kernel clock ticks |
| `sort` | `sort [-r] [file]` sorts bytewise in memory. Reads stdin by default. |
| `stat` | `stat file ...` shows inode metadata including owner, group and mode |
| `tee` | `tee [-a] [file ...]` copies stdin to stdout and each file |
| `tr` | `tr set1 set2` translates bytes, with ranges like `a-z`. `tr -d set` deletes bytes. |
| `uniq` | `uniq [-c] [file]` collapses adjacent duplicates, optionally printing counts |
| `yes` | `yes [word ...]` repeats a line until interrupted or its pipe closes |

`sort` and `uniq` accept text lines of up to 4,095 bytes. `cp` truncates an
existing destination after checking for a same-inode copy. `pwd` obtains
the path directly from the kernel without changing directory. `>` truncates
an existing regular file and `>>` appends. The original tools can still
use `exit()`. New tools can pass an exit code through `exitstatus()`.
Use `ls -l` for modes and numeric ownership. See
[chmod(1)](man/chmod.1) and [chown(1)](man/chown.1) to change them.

Existing utility families remain available, including `cat`, `echo`,
`grep`, `head`, `tail`, `wc`, `ls`, `mkdir`, `rm`, `ln`, `mv`, `cp`,
`touch`, `clear`, `free`, `ps`, `kill`, `uptime`, `hostname` and the
account commands.
