#!/bin/sh
# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

PATH=/bin
export PATH
echo "Welcome to the Project9 dev installation test"
echo "Only a tagged disposable disk can be written."
sysctl hw.bootdisk
sysctl hw.disknames
if [ ! -d /sets ]; then
    echo "Installation sets are missing."
    exit 1
fi
cd /sets || exit 1
if sha256 -C SHA256SUMS base.tar.gz sdk.tar.gz xv6.img; then
    echo "Type the name of the test target shown above."
else
    exit 1
fi
read disk || exit 1
if [ -z "$disk" ]; then
    exit 1
fi
echo "Type yes to erase $disk and install Project9 dev."
read answer || exit 1
if [ "$answer" != yes ]; then
    exit 1
fi

part="${disk}p1"
fdisk -iy "$disk" && newfs "$part" && fsck "$part" && mount "$part" /mnt || exit 1
echo "Extracting base system..."
if pax -rzf base.tar.gz -C /mnt; then
    echo "Extracting SDK..."
    if pax -rzf sdk.tar.gz -C /mnt/usr; then
        echo "Synchronizing and unmounting target..."
        umount /mnt || exit 1
    else
        umount /mnt
        exit 1
    fi
else
    umount /mnt
    exit 1
fi
fsck "$part" || exit 1
echo "Writing bootloader and kernel..."
installboot /sets/xv6.img "$disk" && sync || exit 1

echo "CONGRATULATIONS! Your Project9 install has been successfully completed!"
echo
echo "Remove the installation media and reboot into your new system."
echo "On first boot, sign in as 'root' to complete initial setup."
echo
echo "Exit to (S)hell, (H)alt or (R)eboot? [reboot]"
read answer || exit 1
if [ "$answer" = s ] || [ "$answer" = S ]; then
    exit 0
fi
if [ "$answer" = h ] || [ "$answer" = H ]; then
    halt
    exit $?
fi
if [ -z "$answer" ] || [ "$answer" = r ] || [ "$answer" = R ]; then
    reboot
    exit $?
fi
exit 1
