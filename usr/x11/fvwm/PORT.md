# FVWM on Project9

The window manager comes from OpenBSD's `xenocara/app/fvwm` at commit
`7456bbaae1fa092bdb63696d50646206d4983d7c`. The import consists of the
standalone FVWM 2.2.5 core and its support sources. The permissive core
notice and the upstream license inventory are in
[vendor/openbsd/COPYING](vendor/openbsd/COPYING). The GPL color helper
`libs/ColorUtils.c` and upstream modules are absent from the import.
The local `colors.c` computes shades by blending each color component
with black or white.

The build uses core fonts and disables the optional Shape and XPM
dependencies. The OpenBSD pledge call remains available on OpenBSD.
Project9's build uses its existing process permissions. The port also
uses the Project9 signal set and descriptor limit.

The private Xlib build uses the core files from X.Org libX11 1.1.5
with XCB and the optional extension and internationalization libraries
disabled. Project9's `xtransport.c` connects it to the local X11
socket for display zero. The upstream copyright notices are in
[vendor/libX11/COPYING](vendor/libX11/COPYING).

Use `make fvwm` with the ext2 profile to build the window manager.
Release builds package the executable and `/etc/fvwmrc` into the
staged root. The executable uses the i386 user ABI on both images.
