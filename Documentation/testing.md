<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Validation

Project9 was built from a clean source tree with GNU Make and GCC, and
exercised under QEMU's x86 system emulator. The QEMU runs included the
inherited `usertests` suite, together with focused checks for
the new userland, account handling, TTY editing, signals, core dumps
and graphics. For v0.2, use `v02test` as root with the default mask
of 022. It covers file permissions, group inheritance, child status,
and signal delivery. A passing run prints `V0.2 TESTS PASSED`.

Run `ttytest` from the console to check terminal attributes, invalid
requests, and viewport changes. The expected result is `TTY TESTS PASSED`.
The `forktest`, `stressfs`, and `zombie` sources are in the separate
`Project9-test-backup.zip` archive.

For the development stability suite, run `stability` in a quiet guest
using a disposable disk. After shutting down, check that image offline.
See [stability and recovery](stability.md) for expected
results and crash diagnostics.

## Reproduce the interactive checks

1. Build a fresh image with `make release` and boot a copy in QEMU using
   the [release guide](release.md). Set the root password
   and sign in. Run `pwd`, `id` and `whoami`.
2. Run `echo xyz | tr a-z A-Z | tee /tmp/sample`, then
   `stat /tmp/sample` and `cat /tmp/sample`. Check the pipelines
   `seq 3 | sort -r | uniq -c` and `echo extra >> /tmp/sample`.
3. Run `ed`. Insert lines, substitute text, press Ctrl+C, print the text
   again, then write and quit. Reopen the file to check its contents.
4. Run `sleep 30 | cat`. Press Ctrl+Z, run `jobs`, and use `fg` to
   resume. Press Ctrl+\ to quit the job and list the resulting private
   `core.PID` files. Repeat with Ctrl+C to interrupt without dumping.
5. Try Ctrl+H, Ctrl+U, Ctrl+W, Ctrl+R and Ctrl+L on an uncommitted shell
   line. At `cat`, enter a quoted Ctrl+D with Ctrl+V Ctrl+D, then Enter.
   Send an unquoted Ctrl+D on the next line to exit `cat`. Pause a
   command's console output with Ctrl+S and resume it with Ctrl+Q.
6. As root create a user with `useradd`. Sign in as that user and change
   the password with `passwd`. Verify the new password on the next login.
   Check that writing into `/bin`, reading `/etc/shadow` and entering
   `/root` fail for the unprivileged user.
7. In the graphical QEMU console, run `clear` and `seq 80`. The lower
   lines and the next shell prompt should appear in Gallant after
   scrolling past one full screen.
8. Run `usertests` from the shell on a disposable image. The suite uses
   a private directory under `/tmp` and removes its test files when it
   finishes. It should print `ALL TESTS PASSED` and return status 0.
   Run it again on the same image to check that cleanup worked. After
   stopping the guest, run `python3 tools/installimage.py --check` with
   the path to the disk used in the test.
9. Run `printf '\033[31mRED\033[0m\n'` in the graphical console. Only
   the word RED should appear in red. Then run `printf '\x1b[2J\x1b[H'`
   to clear the display and return the cursor to the top left.
