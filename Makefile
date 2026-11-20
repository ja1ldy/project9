# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.
MAKEFLAGS += --no-builtin-rules
PYTHON ?= python3
ARCH ?= x86_64
ifneq ($(ARCH),i386)
ifneq ($(ARCH),x86_64)
$(error ARCH must be i386 or x86_64)
endif
endif
FILESYSTEM ?= ext2
PROFILE := build/$(ARCH)$(if $(filter xv6-fs,$(FILESYSTEM)),/legacy)
BUILD := $(PROFILE)$(if $(filter x86_64,$(ARCH)),/compat-i386)
BOOT_IMAGE := $(PROFILE)/$(if $(filter xv6-fs,$(FILESYSTEM)),xv6-legacy.img,xv6.img)
DEFAULT_FS := $(PROFILE)/$(if $(filter xv6-fs,$(FILESYSTEM)),fs.xv6.img,install.img)
.DEFAULT_GOAL := .build

# cross compiling e g on mac os x
# toolprefix i386 jos elf

# using native tools e g on x86 linux
#TOOLPREFIX = 

# try to infer the correct toolprefix if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if i386-jos-elf-objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1; \
	then echo 'i386-jos-elf-'; \
	elif objdump -i 2>&1 | grep 'elf32-i386' >/dev/null 2>&1; \
	then echo ''; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find an i386-*-elf version of GCC/binutils." 1>&2; \
	echo "*** Is the directory with i386-jos-elf-gcc in your PATH?" 1>&2; \
	echo "*** If your i386-*-elf toolchain is installed with a command" 1>&2; \
	echo "*** prefix other than 'i386-jos-elf-', set your TOOLPREFIX" 1>&2; \
	echo "*** environment variable to that prefix and run 'make' again." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump
AR = $(TOOLPREFIX)ar
CFLAGS = -Iinclude -fno-pic -static -fno-builtin -fno-strict-aliasing -fno-asynchronous-unwind-tables -fcf-protection=none -O2 -Wall -MD -ggdb -m32 -march=i486 -mgeneral-regs-only -Wno-array-bounds -Wno-infinite-recursion -fno-omit-frame-pointer
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
ASFLAGS = -Iinclude -m32 -march=i486 -mgeneral-regs-only -gdwarf-2 -Wa,-divide
# freebsd ld wants elf i386 fbsd
LDFLAGS += -m $(shell $(LD) -V | grep elf_i386 2>/dev/null | head -n 1)

# disable pie when possible for ubuntu 16 10 toolchain
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

KSRCS := $(wildcard arch/x86/kernel/*.c arch/x86/kernel/*.S drivers/tty/*.c drivers/input/*.c drivers/block/*.c drivers/pci/*.c drivers/usb/*.c fs/*.c fs/ext2/*.c kernel/*.c mm/*.c)
ifeq ($(FILESYSTEM),xv6-fs)
CFLAGS += -DP9_LEGACY_ROOT
KSRCS := $(filter-out fs/fs.c fs/operations.c,$(KSRCS)) $(wildcard fs/legacy/*.c)
endif
KOBJS := $(patsubst %,$(BUILD)/%.o,$(basename $(KSRCS))) $(BUILD)/vectors.o
PAGER := $(BUILD)/usr/lib/pager.o
ULIB := $(filter-out $(PAGER),$(patsubst %,$(BUILD)/%.o,$(basename $(wildcard usr/lib/*.c usr/lib/*.S))))
USRC := $(wildcard usr/bin/*.c usr/tests/*.c usr/init/*.c)
USER_PROGRAMS := $(basename $(notdir $(USRC)))
UPROGS := $(addprefix $(BUILD)/bin/,$(USER_PROGRAMS)) $(BUILD)/bin/[
ROOTFS := $(BUILD)/rootfs
ifeq ($(FILESYSTEM),ext2)
X11_SERVER := $(BUILD)/x11/tinyx
VI32 := $(BUILD)/vi/vi
TOP32 := $(BUILD)/top/top
VI_PROGRAM = $(if $(KERNEL64),$(PROFILE)/fat/vi,$(VI32))
TOP_PROGRAM = $(if $(KERNEL64),$(PROFILE)/fat/top,$(TOP32))
X11_APPS := $(addprefix $(BUILD)/x11/,p9term xdpyinfo xprop xwininfo xkill xrandr p9font)
X11_UPSTREAM := $(addprefix $(BUILD)/x11-upstream/bin/,xsetroot xset xmodmap xhost xrdb xeyes xclock xinit)
X11_STARTX := $(BUILD)/x11-upstream/bin/startx
X11_XINITRC := usr/x11/apps/.xinitrc
FVWM_PROGRAM := $(BUILD)/x11/fvwm
XTERM_PROGRAM := $(BUILD)/x11/xterm
X11_LOCALE := usr/x11/fvwm/locale
X11_LOCALE_FILES := $(X11_LOCALE)/locale.dir $(X11_LOCALE)/C/XLC_LOCALE
endif
ROOTFS_INPUTS := $(shell find rootfs -print | sed 's/ /\\ /g')
ROOTFS_TEXT := $(wildcard rootfs/etc/motd rootfs/etc/hostname rootfs/etc/fvwmrc rootfs/usr/share/man/*.[125])
MANPAGES := $(addprefix $(BUILD)/,$(filter rootfs/usr/share/man/%,$(ROOTFS_TEXT)))
ROOTFS_BINS := $(addprefix $(ROOTFS)/bin/,$(notdir $(UPROGS)))
LINK = $(LD) $(LDFLAGS) -z max-page-size=4096 --build-id=none
# show the operation and target v 1 prints the commands for debugging
ifeq ($(V),1)
Q :=
quiet =
else
Q := @
quiet = @printf '  %-8s %s\n' '$(1)' '$(2)'
# these small binaries deliberately use n keep the warning in verbose builds
SMALL_LINK_WARNINGS := $(shell $(LD) --help 2>/dev/null | \
	grep -q -- '--no-warn-rwx-segments' && printf '%s' --no-warn-rwx-segments)
endif
SMALL_LINK = $(LINK) -N $(SMALL_LINK_WARNINGS)
vpath %.c usr/bin usr/tests usr/init

$(BUILD)/%.o: %.c
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: %.S
	$(Q)mkdir -p $(@D)
	$(call quiet,AS,$@)
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

BOOT := $(BUILD)/boot/pc
BTX := $(BOOT)/btx
BOOT_STAGE_ADDR := 0x20000
BOOT64 ?= $(if $(filter x86_64,$(ARCH)),1,0)
CC64 ?= $(CC)
LD64 ?= $(LD)
OBJCOPY64 ?= $(OBJCOPY)
BOOT64_FLAGS := -Iinclude -DP9_KERNEL64 -fno-omit-frame-pointer -fno-strict-aliasing -m64 -march=x86-64 -mcmodel=kernel -mno-red-zone \
	-mgeneral-regs-only -O2 -Wall -ffreestanding -fno-pic -fno-pie \
	-fno-stack-protector -fcf-protection=none -fno-asynchronous-unwind-tables -MMD
ifeq ($(BOOT64),1)
KERNEL64 := $(PROFILE)/native/kernel.elf
FAT_BINS := $(addprefix $(PROFILE)/fat/,$(USER_PROGRAMS)) $(PROFILE)/fat/[
endif
BOOT_CLIENT := $(BTX)/btxcsu.o $(BTX)/btxsys.o $(BTX)/btxv86.o \
	$(BOOT)/main.o $(BOOT)/console.o $(BOOT)/kernel.o $(BOOT)/handoff.o $(BOOT)/video.o $(BOOT)/hardware.o $(BOOT)/artwork.o

$(BTX)/%.o: CFLAGS += -Iboot/pc
$(BTX)/btx.o: CFLAGS += -DBTX_FLAGS=0
$(BOOT)/mbr.o: CFLAGS += -DBOOT_STAGE_ADDR=$(BOOT_STAGE_ADDR)
$(BOOT)/mbr.o: Makefile
$(BOOT)/main.o $(BOOT)/console.o $(BOOT)/kernel.o $(BOOT)/video.o $(BOOT)/hardware.o: CFLAGS += -Os

$(BOOT)/mbr.elf: $(BOOT)/mbr.o boot/pc/flat.ld Makefile
	$(call quiet,LD,$@)
	$(Q)$(LINK) --defsym=boot_origin=0x7c00 --defsym=boot_size=512 -T boot/pc/flat.ld -o $@ $<

$(BTX)/btx.elf: $(BTX)/btx.o boot/pc/flat.ld Makefile
	$(call quiet,LD,$@)
	$(Q)$(LINK) --defsym=boot_origin=0x9000 --defsym=boot_size=0 -T boot/pc/flat.ld -o $@ $<

$(BOOT)/mbr $(BTX)/btx: %: %.elf
	$(call quiet,OBJCOPY,$@)
	$(Q)$(OBJCOPY) -O binary -j .text $< $@

$(BTX)/btxldr.elf: $(BTX)/btxldr.o boot/pc/btxldr.ld Makefile
	$(call quiet,LD,$@)
	$(Q)$(LINK) --defsym=boot_stage_addr=$(BOOT_STAGE_ADDR) -T boot/pc/btxldr.ld -o $@ $<

$(BTX)/btxldr: $(BTX)/btxldr.elf
	$(call quiet,OBJCOPY,$@)
	$(Q)$(OBJCOPY) -O binary -j .text -j .data $< $@

$(BOOT)/artwork.o: boot/pc/menu.vga
	$(Q)mkdir -p $(@D)
	$(call quiet,OBJCOPY,$@)
	$(Q)cd boot/pc && $(OBJCOPY) -I binary -O elf32-i386 -B i386 \
		--rename-section .data=.rodata,alloc,load,readonly,data,contents menu.vga ../../$@

$(BOOT)/client.elf: $(BOOT_CLIENT) boot/pc/client.ld
	$(call quiet,LD,$@)
	$(Q)$(SMALL_LINK) -s -T boot/pc/client.ld -o $@ $(BOOT_CLIENT)

$(BOOT)/stage: $(BTX)/btxldr $(BTX)/btx $(BOOT)/client.elf
	$(call quiet,PACK,$@)
	$(Q)test "$$(wc -c < $(BTX)/btxldr)" -le 4096 || { echo 'BTX loader exceeds one page' >&2; exit 1; }
	$(Q)dd if=$(BTX)/btxldr of=$@.tmp bs=4096 count=1 conv=sync status=none
	$(Q)cat $(BTX)/btx $(BOOT)/client.elf >> $@.tmp
	$(Q)mv $@.tmp $@

$(BUILD)/mkboot: tools/mkboot.c boot/pc/layout.h
	$(Q)mkdir -p $(@D)
	$(call quiet,HOSTCC,$@)
	$(Q)$(HOSTCC) -Wall -Wextra -o $@ $<

$(BUILD)/entryother: $(BUILD)/arch/x86/kernel/entryother.o
	$(call quiet,LD,$@.elf)
	$(Q)$(SMALL_LINK) -e start -Ttext 0x7000 -o $@.elf $<
	$(call quiet,OBJCOPY,$@)
	$(Q)$(OBJCOPY) -S -O binary -j .text $@.elf $@

$(BUILD)/initcode: $(BUILD)/usr/init/initcode.o
	$(call quiet,LD,$@.elf)
	$(Q)$(SMALL_LINK) -e start -Ttext 0 -o $@.elf $<
	$(call quiet,OBJCOPY,$@)
	$(Q)$(OBJCOPY) -S -O binary -j .text $@.elf $@

$(BUILD)/%_bin.o: $(BUILD)/%
	$(call quiet,OBJCOPY,$@)
	$(Q)cd $(BUILD) && $(OBJCOPY) -I binary -O elf32-i386 -B i386 $* $*_bin.o

$(BUILD)/vectors.S: tools/vectors.pl
	$(Q)mkdir -p $(@D)
	$(call quiet,GEN,$@)
	$(Q)perl $< > $@

$(BUILD)/vectors.o: $(BUILD)/vectors.S
	$(call quiet,AS,$@)
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

KERNEL_OBJS := $(filter-out $(BUILD)/arch/x86/kernel/entryother.o,$(KOBJS)) $(BUILD)/initcode_bin.o $(BUILD)/entryother_bin.o
$(BUILD)/kernel.elf: $(KERNEL_OBJS) arch/x86/kernel/kernel.ld
	$(call quiet,LD,$@)
	$(Q)$(LINK) -T arch/x86/kernel/kernel.ld -o $@ $(KERNEL_OBJS)

WIDE := $(PROFILE)/native
WIDE_SRCS := $(filter-out %.S,$(KSRCS)) $(filter-out %/entryother.S,$(wildcard arch/x86_64/*.S))
WIDE_OBJS := $(patsubst %,$(WIDE)/%.o,$(basename $(WIDE_SRCS))) \
  $(WIDE)/vectors.o $(WIDE)/initcode_bin.o $(WIDE)/entryother_bin.o

$(WIDE)/%.o: %.c Makefile
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC64) $(BOOT64_FLAGS) $(if $(filter xv6-fs,$(FILESYSTEM)),-DP9_LEGACY_ROOT) -c -o $@ $<

$(WIDE)/%.o: %.S Makefile
	$(Q)mkdir -p $(@D)
	$(call quiet,AS,$@)
	$(Q)$(CC64) $(BOOT64_FLAGS) -c -o $@ $<

$(WIDE)/vectors.S: tools/vectors.pl
	$(Q)mkdir -p $(@D)
	$(call quiet,GEN,$@)
	$(Q)perl $< 64 > $@

$(WIDE)/vectors.o: $(WIDE)/vectors.S
	$(call quiet,AS,$@)
	$(Q)$(CC64) $(BOOT64_FLAGS) -c -o $@ $<

$(WIDE)/initcode: $(BUILD)/initcode
	$(Q)mkdir -p $(@D)
	$(Q)cp $< $@

$(WIDE)/entryother: $(WIDE)/arch/x86_64/entryother.o
	$(call quiet,LD,$@.elf)
	$(Q)$(LD64) -m elf_x86_64 -e start -Ttext 0x7000 -o $@.elf $<
	$(call quiet,OBJCOPY,$@)
	$(Q)$(OBJCOPY64) -O binary -j .text $@.elf $@

$(WIDE)/%_bin.o: $(WIDE)/%
	$(call quiet,OBJCOPY,$@)
	$(Q)cd $(WIDE) && $(OBJCOPY64) -I binary -O elf64-x86-64 -B i386:x86-64 $* $*_bin.o

$(WIDE)/kernel.elf: $(WIDE_OBJS) arch/x86_64/kernel.ld
	$(Q)mkdir -p $(@D)
	$(call quiet,LD,$@)
	$(Q)$(LD64) -m elf_x86_64 -z max-page-size=4096 --build-id=none -z noexecstack -T arch/x86_64/kernel.ld -o $@ $(filter %.o,$^)

USER64_FLAGS := -Iinclude -DP9_USER64 -m64 -O2 -Wall -ffreestanding \
  -fno-pic -fno-pie -fno-stack-protector -fno-strict-aliasing -fcf-protection=none \
  -fno-asynchronous-unwind-tables -ffunction-sections -fdata-sections -MMD
PAGER64 := $(WIDE)/usr/lib/pager.o
ULIB64 := $(filter-out $(PAGER64),$(patsubst %.c,$(WIDE)/%.o,$(wildcard usr/lib/*.c))) \
  $(WIDE)/usr/lib/usys.o $(WIDE)/usr/arch/x86_64/start.o $(WIDE)/usr/arch/x86_64/setjmp.o
SHELL64 := $(patsubst %.c,$(WIDE)/%.o,$(wildcard usr/shell/*.c))
USER64_OBJECTS := $(addprefix $(WIDE)/user/,$(addsuffix .o,$(USER_PROGRAMS)))

$(WIDE)/usr/%.o: usr/%.c Makefile
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC64) $(USER64_FLAGS) -c -o $@ $<

$(WIDE)/usr/%.o: usr/%.S Makefile
	$(Q)mkdir -p $(@D)
	$(call quiet,AS,$@)
	$(Q)$(CC64) $(USER64_FLAGS) -c -o $@ $<

$(WIDE)/user/%.o: %.c Makefile
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC64) $(USER64_FLAGS) -c -o $@ $<

$(WIDE)/userhash.o: kernel/sha256.c Makefile
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC64) $(USER64_FLAGS) -c -o $@ $<

$(WIDE)/bin/man $(WIDE)/bin/less: $(PAGER64)
$(WIDE)/bin/sh: $(SHELL64)
$(WIDE)/bin/sha256: $(WIDE)/userhash.o

USER64_LINK = $(LD64) -m elf_x86_64 -z max-page-size=4096 --build-id=none -z noexecstack --gc-sections -s

$(WIDE)/bin/%: $(WIDE)/user/%.o $(ULIB64) usr/arch/x86_64/user.ld
	$(Q)mkdir -p $(@D)
	$(call quiet,LD,$@)
	$(Q)$(USER64_LINK) -T usr/arch/x86_64/user.ld -o $@ $(filter %.o,$^)

$(PROFILE)/fat/%: $(BUILD)/bin/% $(WIDE)/bin/% tools/fatpack.py
	$(Q)mkdir -p $(@D)
	$(call quiet,PACK,$@)
	$(Q)$(PYTHON) tools/fatpack.py $@ $< $(word 2,$^)

$(PROFILE)/fat/[: $(PROFILE)/fat/test
	$(call quiet,COPY,$@)
	$(Q)cp $< '$@'

.SECONDARY: $(ULIB64) $(USER64_OBJECTS) $(addprefix $(WIDE)/bin/,$(USER_PROGRAMS))

$(BUILD)/boot-options: .force
	$(Q)mkdir -p $(@D)
	$(Q)printf '%s\n' '$(BOOT64)' > $@.tmp
	$(Q)cmp -s $@.tmp $@ || cp $@.tmp $@
	$(Q)rm -f $@.tmp

$(BOOT_IMAGE): $(BUILD)/kernel.elf $(BOOT)/mbr $(BOOT)/stage $(BUILD)/mkboot $(KERNEL64) $(BUILD)/boot-options
	$(call quiet,IMAGE,$@)
	$(Q)$(BUILD)/mkboot $@.tmp $(BOOT)/mbr $(BOOT)/stage $< $(KERNEL64)
	$(Q)mv $@.tmp $@

$(BUILD)/user/%.o: %.c
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/bin/man $(BUILD)/bin/less: $(PAGER)

$(BUILD)/bin/%: $(BUILD)/user/%.o $(ULIB) usr/lib/user.ld
	$(Q)mkdir -p $(@D)
	$(call quiet,LD,$@)
	$(Q)$(SMALL_LINK) -s -T usr/lib/user.ld -o $@ $(filter %.o,$^)

HOSTCC ?= cc
MKE2FS ?= mke2fs
E2FSCK ?= e2fsck
DEBUGFS ?= debugfs

ROOT_MIB ?= 512
ROOT_BLOCK ?= 1024
ROOT_IMAGE = $(PYTHON) tools/osimage.py --size $(ROOT_MIB) --block $(ROOT_BLOCK) \
             --mke2fs "$(MKE2FS)" --e2fsck "$(E2FSCK)" --debugfs "$(DEBUGFS)" --filesystem $(FILESYSTEM)

ifeq ($(FILESYSTEM),xv6-fs)
LEGACY_CHECK := $(BUILD)/legacy-fsck
ROOT_IMAGE += --legacy-check "$(LEGACY_CHECK)"
endif

$(BUILD)/legacy-fsck: tools/fsck.c include/legacyfs.h include/fscheck.h include/fs.h include/p9records.h include/types.h include/mode.h include/param.h
	$(Q)mkdir -p $(@D)
	$(call quiet,HOSTCC,$@)
	$(Q)$(HOSTCC) -idirafter include -Wall -Wextra -o $@ $<

# build a fresh tree so removed files cannot linger in the image
ifdef X11_SERVER
X11_LICENSES := usr/x11/vendor/tinyx/COPYING usr/x11/vendor/tinyx/LICENSE.md \
  $(wildcard usr/x11/vendor/tinyx/fonts/sources/*/COPYING usr/x11/vendor/xorgproto/COPYING-*) \
  usr/x11/vendor/freetype/docs/FTL.TXT usr/x11/fonts/NOTICE \
  usr/x11/fvwm/vendor/openbsd/COPYING usr/x11/fvwm/vendor/libX11/COPYING \
  $(wildcard usr/x11/xterm/vendor/*/COPYING usr/x11/apps/vendor/*/COPYING usr/x11/apps/vendor/*.LICENSE)
$(BUILD)/x11/NOTICES: $(X11_LICENSES)
	$(Q)mkdir -p $(@D)
	$(call quiet,TEXT,$@)
	$(Q)for license in $^; do printf '\n%s\n\n' "$$license"; cat "$$license"; done > $@.tmp
	$(Q)mv $@.tmp $@
endif

$(ROOTFS): $(ROOTFS_INPUTS) usr/bin usr/tests usr/init $(UPROGS) $(FAT_BINS) $(if $(X11_SERVER),usr/x11/fonts/DejaVuSansMono.ttf usr/x11/fonts/NOTICE) $(X11_SERVER) $(X11_APPS) $(X11_UPSTREAM) $(X11_STARTX) $(X11_XINITRC) $(FVWM_PROGRAM) $(XTERM_PROGRAM) $(X11_LOCALE_FILES) $(VI_PROGRAM) $(TOP_PROGRAM) $(if $(TOP_PROGRAM),usr/top/COPYING) $(if $(X11_SERVER),$(BUILD)/x11/NOTICES) $(BUILD)/boot-options $(BUILD)/LICENSE Makefile
	$(call quiet,ROOTFS,$@)
	$(Q)set -e; \
	test -z "$$(find rootfs ! -type d ! -type f -print)" || { \
		echo 'rootfs must contain only files and directories' >&2; exit 1; \
	}; \
	rm -rf '$@.tmp'; \
	mkdir -p '$@.tmp/bin'; \
	cp -RP rootfs/. '$@.tmp/'; \
	find '$@.tmp' -name .keep -type f -exec rm {} +; \
	for file in $(ROOTFS_TEXT); do \
		sed '1{/^# Project9 contributions are under ISC/d;}' "$$file" > "$@.tmp/$${file#rootfs/}"; \
	done; \
	cp $(UPROGS) '$@.tmp/bin/'; \
	$(if $(FAT_BINS),cp $(FAT_BINS) '$@.tmp/bin/',:); \
	ln -s pax '$@.tmp/bin/tar'; \
	ln -s halt '$@.tmp/bin/poweroff'; \
	$(if $(X11_SERVER),mkdir -p '$@.tmp/usr/X11/bin' && cp '$(X11_SERVER)' $(X11_APPS) $(X11_UPSTREAM) $(X11_STARTX) $(FVWM_PROGRAM) $(XTERM_PROGRAM) '$@.tmp/usr/X11/bin/' && mkdir -p '$@.tmp/usr/X11/share' && cp -R '$(X11_LOCALE)' '$@.tmp/usr/X11/share/locale' && cp usr/x11/vendor/tinyx/COPYING '$(BUILD)/x11/NOTICES' '$@.tmp/usr/X11/' && mkdir -p '$@.tmp/usr/share/fonts/truetype' && cp usr/x11/fonts/DejaVuSansMono.ttf usr/x11/fonts/NOTICE '$@.tmp/usr/share/fonts/truetype/',:); \
	$(if $(X11_XINITRC),mkdir -p '$@.tmp/etc/X11/xinit' && \
	  cp '$(X11_XINITRC)' '$@.tmp/etc/X11/xinit/xinitrc' && \
	  cp '$(X11_XINITRC)' '$@.tmp/root/.xinitrc',:); \
	$(if $(VI_PROGRAM),mkdir -p '$@.tmp/usr/bin' '$@.tmp/usr/share/doc/neatvi' && cp '$(VI_PROGRAM)' '$@.tmp/usr/bin/vi' && cp usr/editors/neatvi/LICENSE '$@.tmp/usr/share/doc/neatvi/',:); \
	$(if $(TOP_PROGRAM),mkdir -p '$@.tmp/usr/bin' '$@.tmp/usr/share/doc/top' && cp '$(TOP_PROGRAM)' '$@.tmp/usr/bin/top' && cp usr/top/COPYING '$@.tmp/usr/share/doc/top/',:); \
	cp '$(BUILD)/LICENSE' '$@.tmp/LICENSE'; \
	rm -rf '$@'; \
	mv '$@.tmp' '$@'

$(BUILD)/LICENSE: LICENSE Documentation/V6-license.txt Documentation/Gallant-license.txt Documentation/FreeBSD-BTX-license.txt Documentation/NetBSD-LICENSES.txt Documentation/xv6-x86_64-LICENSE.txt
	$(Q)mkdir -p $(@D)
	$(call quiet,TEXT,$@)
	$(Q)cat $^ > $@

# rebuild the factory when its geometry changes
$(BUILD)/root-options: .force
	$(Q)mkdir -p $(@D)
	$(Q)printf '%s %s\n' '$(ROOT_MIB)' '$(ROOT_BLOCK)' > "$@.tmp"
	$(Q)if cmp -s "$@" "$@.tmp"; then rm "$@.tmp"; else mv "$@.tmp" "$@"; fi

$(BUILD)/fs.factory.img: $(ROOTFS) tools/osimage.py tools/legacyimage.py $(BUILD)/root-options $(LEGACY_CHECK)
	$(call quiet,MKFS,$@)
	$(Q)$(ROOT_IMAGE) --stage "$(ROOTFS)" "$@"

ifeq ($(FILESYSTEM),xv6-fs)
$(DEFAULT_FS): | $(BUILD)/fs.factory.img
	$(call quiet,COPY,$@)
	$(Q)cp "$(BUILD)/fs.factory.img" "$@"
else
$(DEFAULT_FS): $(BOOT_IMAGE) tools/installimage.py | $(BUILD)/fs.factory.img
	$(call quiet,IMAGE,$@)
	$(Q)if [ -f "$@" ]; then \
	  $(PYTHON) tools/installimage.py --refresh "$@" "$(BOOT_IMAGE)"; \
	else $(PYTHON) tools/installimage.py "$@" "$(BOOT_IMAGE)" "$(BUILD)/fs.factory.img"; fi
endif

.PRECIOUS: $(BUILD)/%.o $(BUILD)/user/%.o
.SECONDARY: $(ULIB)
-include $(shell test ! -d $(PROFILE) || find $(PROFILE) -path '$(PROFILE)/legacy' -prune -o -name '*.d' -print)

clean:
	$(call quiet,CLEAN,$(BUILD))
	$(Q)find "$(PROFILE)" -mindepth 1 -maxdepth 1 ! -name "$(notdir $(DEFAULT_FS))" \
	  ! -name project9.json ! -name "$(notdir $(DEFAULT_FS)).reset-backup" $(if $(filter xv6-fs,$(FILESYSTEM)),,! -name legacy) \
	  -exec rm -rf {} + 2>/dev/null || test ! -d "$(PROFILE)"

full-clean:
	$(call quiet,CLEAN,build release)
	$(Q)rm -rf build release

$(BUILD)/bin/[: $(BUILD)/bin/test
	$(call quiet,COPY,$@)
	$(Q)cp $< '$@'

SHOBJ := $(patsubst %.c,$(BUILD)/%.o,$(wildcard usr/shell/*.c))
$(BUILD)/bin/sh: $(BUILD)/user/sh.o $(SHOBJ) $(ULIB) usr/lib/user.ld
	$(Q)mkdir -p $(@D)
	$(call quiet,LD,$@)
	$(Q)$(SMALL_LINK) -s -T usr/lib/user.ld -o $@ $(filter %.o,$^)

# forktest supplies its own small printf
$(BUILD)/bin/forktest: $(BUILD)/user/forktest.o $(filter-out %/printf.o %/umalloc.o %/execpath.o,$(ULIB)) usr/lib/user.ld
	$(Q)mkdir -p $(@D)
	$(call quiet,LD,$@)
	$(Q)$(SMALL_LINK) -s -T usr/lib/user.ld -o $@ $(filter %.o,$^)


SDK := $(BUILD)/sdk
SDK_PUBLIC := $(shell find usr/libc/include -type f)
SDK_NATIVE := local.h reboot.h cpufeatures.h param.h p9records.h syscall.h fcntl.h devices.h ioctl.h p9abi.h signal.h wait.h
SDK_INPUT := $(wildcard usr/libc/*.c)
SDK_OBJECTS := $(patsubst usr/libc/%.c,$(BUILD)/usr/libc/%.o,$(SDK_INPUT))
SDK_LEGACY := $(addprefix $(BUILD)/usr/lib/,env.o termios.o umalloc.o setjmp.o usys.o)
SDK_VENDOR := usr/libc/vendor/netbsd
SDK_NETBSD_C := $(filter-out %/bcopy.c %/memcpy.c,$(wildcard $(SDK_VENDOR)/string/*.c $(SDK_VENDOR)/stdlib/*.c)) \
  $(filter-out %/engine.c,$(wildcard $(SDK_VENDOR)/regex/*.c))
SDK_NETBSD_C += $(wildcard $(SDK_VENDOR)/gdtoa/*.c $(SDK_VENDOR)/libm/*.c $(SDK_VENDOR)/stdio/*.c $(SDK_VENDOR)/time/*.c)
SDK_NETBSD_S := $(wildcard $(SDK_VENDOR)/libm/*.S)
SDK_NETBSD := $(patsubst usr/libc/%.c,$(BUILD)/usr/libc/%.o,$(SDK_NETBSD_C)) \
  $(patsubst usr/libc/%.S,$(BUILD)/usr/libc/%.o,$(SDK_NETBSD_S))
SDK_CFLAGS = -nostdinc -I$(SDK)/include -isystem $(shell $(CC) -print-file-name=include) \
  $(filter-out -Iinclude -mgeneral-regs-only,$(CFLAGS)) -ffunction-sections -fdata-sections
SDK_COMPAT := $(shell find usr/libc/compat -type f)

SDK_FILES := $(SDK)/libc.a $(SDK)/libm.a $(SDK)/crt0.o $(SDK)/user.ld $(SDK)/NetBSD-LICENSES.txt

$(SDK)/include/.ready: $(SDK_PUBLIC) $(addprefix include/,$(SDK_NATIVE)) include/termios.h include/setjmp.h
	$(Q)mkdir -p $(SDK)/include/project9
	$(Q)cp -R usr/libc/include/. $(SDK)/include/
	$(Q)cp $(addprefix include/,$(SDK_NATIVE)) $(SDK)/include/project9/
	$(Q)cp include/termios.h include/setjmp.h $(SDK)/include/
	$(Q)touch $@

$(BUILD)/usr/libc/memcpy.o: SDK_CFLAGS += -mgeneral-regs-only
$(BUILD)/usr/libc/%.o: usr/libc/%.c $(SDK)/include/.ready
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(SDK_CFLAGS) -c -o $@ $<

$(SDK)/crt0.o: usr/libc/crt0.S
	$(Q)mkdir -p $(@D)
	$(call quiet,AS,$@)
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

$(SDK)/user.ld: usr/lib/user.ld
	$(Q)mkdir -p $(@D)
	$(Q)cp $< $@

$(SDK)/NetBSD-LICENSES.txt: Documentation/NetBSD-LICENSES.txt Documentation/xv6-x86_64-LICENSE.txt
	$(Q)mkdir -p $(@D)
	$(Q)cp $< $@

$(SDK)/libc.a: $(SDK_OBJECTS) $(SDK_NETBSD) $(SDK_LEGACY)
$(SDK)/libm.a: $(BUILD)/usr/libc/math.o $(filter $(BUILD)/usr/libc/vendor/netbsd/libm/%,$(SDK_NETBSD))

$(SDK)/libc.a $(SDK)/libm.a:
	$(Q)mkdir -p $(@D)
	$(call quiet,AR,$@)
	$(Q)rm -f $@
	$(Q)$(AR) rcs $@ $^

$(BUILD)/usr/libc/vendor/%.o: usr/libc/vendor/%.c $(SDK)/include/.ready $(SDK_COMPAT)
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(SDK_CFLAGS) -Iusr/libc/compat -Os -fexcess-precision=standard -c -o $@ $<

SDK_GDTOA_FLAGS := -D__HAVE_LONG_DOUBLE -DNO_FENV_H -DHonor_FLT_ROUNDS -DTrust_FLT_ROUNDS
SDK_TIME_FLAGS := -include usr/libc/compat/tz_utc.h -DP9_TIME32 -DMKTIME_MIGHT_OVERFLOW=1 \
  -DSUPPORT_POSIX2008=1 -DNETBSD_INSPIRED=1 -DHAVE_STRFTIME_L=0

$(BUILD)/usr/libc/vendor/netbsd/gdtoa/%.o: SDK_CFLAGS += $(SDK_GDTOA_FLAGS)
$(BUILD)/usr/libc/vendor/netbsd/libm/%.o: SDK_CFLAGS += -DNO_FPSETPREC
$(BUILD)/usr/libc/vendor/netbsd/time/%.o: SDK_CFLAGS += $(SDK_TIME_FLAGS)

$(BUILD)/usr/libc/vendor/%.o: usr/libc/vendor/%.S $(SDK_COMPAT)
	$(Q)mkdir -p $(@D)
	$(call quiet,AS,$@)
	$(Q)$(CC) $(ASFLAGS) -Iusr/libc/compat -c -o $@ $<

-include $(SDK_OBJECTS:.o=.d) $(SDK_NETBSD:.o=.d)


ifeq ($(FILESYSTEM),xv6-fs)
SDK_TEST_DISK := $(BUILD)/libc-test.img
else
SDK_TEST_DISK := $(BUILD)/libc-install.img
$(BUILD)/libc-install.img: $(BOOT_IMAGE) $(BUILD)/libc-test.img tools/installimage.py
	$(call quiet,IMAGE,$@)
	$(Q)$(PYTHON) tools/installimage.py "$@" "$(BOOT_IMAGE)" "$(BUILD)/libc-test.img"
endif

NETBSD_TESTS := $(addprefix $(BUILD)/,bsdregex bsdnumber bsdtime)
SDK_TESTS := $(BUILD)/libctest $(NETBSD_TESTS)

$(addsuffix .o,$(NETBSD_TESTS)): $(BUILD)/%.o: usr/libc/tests/%.c usr/libc/tests/netbsdtest.h
$(BUILD)/libctest.o: usr/libc/tests/smoke.c

$(addsuffix .o,$(SDK_TESTS)): $(SDK)/include/.ready
	$(call quiet,CC,$@)
	$(Q)$(CC) $(SDK_CFLAGS) -Os -c -o $@ $(filter %.c,$^)

$(BUILD)/bsdnumber: $(SDK)/libm.a

$(SDK_TESTS): %: %.o $(SDK)/crt0.o $(SDK)/libc.a $(SDK)/user.ld
	$(call quiet,LD,$@)
	$(Q)$(SMALL_LINK) -s --gc-sections -T $(SDK)/user.ld -o $@ $(SDK)/crt0.o $< $(filter $(SDK)/libm.a,$^) $(SDK)/libc.a

$(BUILD)/libc-test.img: $(ROOTFS) tools/osimage.py tools/legacyimage.py $(BUILD)/root-options $(LEGACY_CHECK) $(SDK_TESTS) usr/libc/tests/netbsdtest
	$(call quiet,MKFS,$@)
	$(Q)rm -rf $(BUILD)/libc-test-root
	$(Q)cp -R $(ROOTFS) $(BUILD)/libc-test-root
	$(Q)cp $(SDK_TESTS) usr/libc/tests/netbsdtest $(BUILD)/libc-test-root/bin/
	$(Q)$(ROOT_IMAGE) --stage "$(BUILD)/libc-test-root" "$@"

$(BUILD)/userhash.o: kernel/sha256.c
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/bin/sha256: $(BUILD)/userhash.o

include usr/x11/build.mk
include usr/x11/fvwm/build.mk
include usr/x11/xterm/build.mk
include usr/x11/apps/build.mk
include usr/curses/build.mk
include usr/libc/native.mk
include usr/top/build.mk

include usr/editors/build.mk

.PHONY: .build .force release clean full-clean
.build: $(BOOT_IMAGE) $(DEFAULT_FS) $(SDK_FILES) $(if $(filter 1,$(SDK_TEST)),$(SDK_TEST_DISK))

RELEASEDIR ?= release/dev
ifeq ($(ARCH)/$(FILESYSTEM)/$(BOOT64),x86_64/ext2/1)
release: $(SDK_FILES) $(BOOT_IMAGE) $(ROOTFS)
	$(call quiet,RELEASE,$(RELEASEDIR))
	$(Q)$(PYTHON) tools/release.py "$(RELEASEDIR)" --stage "$(BUILD)" --boot "$(BOOT_IMAGE)" --mke2fs "$(MKE2FS)" --debugfs "$(DEBUGFS)" --e2fsck "$(E2FSCK)"
else
release:
	$(Q)$(MAKE) ARCH=x86_64 FILESYSTEM=ext2 BOOT64=1 RELEASEDIR="$(RELEASEDIR)" release
endif
