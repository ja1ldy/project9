<!-- Project9 contributions are under ISC. See LICENSE for scope and upstream terms. -->

# Hardware support

Project9 boots through a 32-bit x86 BIOS using the BTX loader and a
supported ATA or AHCI disk. The loader queries the BIOS memory
map and uses the contiguous usable range containing the kernel, up to
the kernel's 224 MiB ceiling. This boot path needs at least 8 MiB of
contiguous extended memory. The kernel discovers enabled processors
and legacy interrupt overrides from a validated ACPI MADT. It can
also use a firmware MP table. A machine with neither table uses the
8259 PIC and PIT on one processor. See the ACPI specification for the
[MADT and interrupt source overrides](https://uefi.org/specs/ACPI/6.4_A/05_ACPI_Software_Programming_Model.html).

The console uses a compatible VBE linear framebuffer selected by
firmware, the accelerated Bochs framebuffer, or VGA text mode. The
framebuffer interface reports the active geometry. The boot loader
looks for an 8-bit-per-channel XRGB mode within the console's bounds.

## Devices

| Node | Driver |
| --- | --- |
| `/dev/console` | Local VGA or framebuffer console and COM1 serial |
| `/dev/keyboard` | Raw i8042 keyboard events |
| `/dev/mouse` | Relative PS/2 auxiliary motion and buttons |
| `/dev/touchpad` | Relative PS/2 events from a detected Synaptics touchpad |
| `/dev/fb0` | Readable display pixels and framebuffer geometry |
| `/dev/rtc` | Read-only CMOS clock |
| `/dev/usb0` through `/dev/usb7` | Root-only raw USB storage reads |
| `/dev/null`, `/dev/zero` | Byte sink and zero-byte source |

The ThinkPad-oriented path targets firmware that exposes the keyboard
and pointing hardware through i8042. A compatible TrackPoint can
appear on the mouse stream. The touchpad driver identifies a
Synaptics PS/2 device and selects relative reporting. The event ABI is
available for a future display server and supports movement and three
buttons. Native ELAN and I²C-HID touchpads, absolute coordinates,
gestures, and a framebuffer write or mapping ABI require further work.

Init creates the device nodes on both fresh and existing disks. Input
and framebuffer devices are root-only. If the matching hardware is
absent, opening the mouse, touchpad or framebuffer node fails.
Run `man 5 dev` for the interfaces.

## Install

Quit QEMU, copy your working `fs.img` into the new tree, and run
`make update-fs` followed by `make qemu`. The update installs the new
init, user programs and manuals and keeps the previous image as
`fs.pre-dev.img`.

The QEMU tests cover BIOS boot, ATA, AHCI, USB storage, and ACPI
shutdown. Physical ThinkPad validation is pending. The boot path
uses legacy BIOS.

## PCI storage and USB

Project9 enumerates PCI functions at startup. The storage driver uses
polled ATA on legacy IDE and polled DMA on AHCI SATA. The AHCI path
supports SATA disks with 512-byte logical sectors. It uses bounded
command waits and the filesystem flushes the drive at its durability
boundaries.

UHCI supports directly connected USB boot-protocol keyboards and
relative mice detected at startup. Keyboard input reaches the console
and raw keyboard queue. Mouse reports enter the pointer queue used
by `/dev/mouse`.

Native OHCI and EHCI drivers enumerate directly connected USB
Bulk-Only Transport disks with 512-byte logical sectors. OHCI handles
full-speed storage and EHCI handles high-speed storage. Init scans
for attachment and removal once a second. Run `sysctl hw.usb` to list
the active devices. A reattached drive receives a new minor number
so a previously opened node cannot start reading another drive.

An empty `hw.usb=` means no supported storage device is active. For a
bare-metal test, plug a flash drive directly into a port and keep the
`pci:` and `usb:` console messages. A host startup failure points to
the controller. A port probe failure names the stage the device reached.

The raw USB file interface is read-only and uses signed 32-bit byte
offsets. Partition mounts, writable USB media, hubs, xHCI, and
OHCI/EHCI HID are pending. The ATA/AHCI installer does not select USB
volumes. The BIOS loader can still start the existing RAM-live
USB path independently of the native USB drivers.

Run `python3 tools/storage-test.py --usb` to exercise both USB
controllers in QEMU with disposable read-only test disks, including
detachment and reattachment. The prebuilt release image is sufficient.

## Framebuffer console

Without hardware scrolling, the console keeps characters and colors
in RAM. It renders changed viewports from those cells after scrolling.
The text console does not need to read pixels back from video memory
on this path. Graphics clients still access the hardware framebuffer.

The poweroff path uses a basic ACPI FADT and static S5 package when
available. It also supports emulator power control. Firmware that
requires an AML interpreter or hardware-reduced ACPI is outside this
path. Physical ThinkPads have not been tested for this release.
