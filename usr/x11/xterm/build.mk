# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

ifdef XTERM_PROGRAM
XTERM := usr/x11/xterm
XTERM_VENDOR := $(XTERM)/vendor
XTERM_BUILD := $(BUILD)/xterm
XTERM_COMPONENTS := libXt libXaw libXmu libXext libSM libICE
XTERM_INCLUDES := $(foreach library,$(XTERM_COMPONENTS),-I$(XTERM_VENDOR)/$(library)/include -I$(XTERM_VENDOR)/$(library)/include/X11)
XTERM_CFLAGS = $(SDK_CFLAGS) -g0 -Os -std=gnu99 -DPROJECT9 -DHAVE_CONFIG_H -DXT_NO_SM -fcommon \
  -I$(XTERM)/compat $(XTERM_INCLUDES) -I$(XTERM_VENDOR)/libXmu/include/X11/Xmu \
  -I$(FVWM_XLIB)/include -I$(FVWM_XLIB)/include/X11 -I$(FVWM_PROTO) \
  -I$(FVWM_BUILD)/include -I$(FVWM)/include -include $(XTERM)/compat/p9x.h
XTERM_SOURCES := $(wildcard $(XTERM_VENDOR)/*/src/*.c $(XTERM_VENDOR)/xterm/*.c)
XTERM_OBJS := $(patsubst $(XTERM_VENDOR)/%.c,$(XTERM_BUILD)/%.o,$(XTERM_SOURCES))


$(filter $(XTERM_BUILD)/libXt/%,$(XTERM_OBJS)): XTERM_FLAGS = -D_XOPEN_SOURCE=700 -DLIBXT_COMPILATION \
  -DXFILESEARCHPATHDEFAULT='"/usr/X11/lib/X11/%T/%N%C%S"' -DERRORDB='"/usr/X11/lib/X11/XtErrorDB"'
$(filter $(XTERM_BUILD)/libXaw/%,$(XTERM_OBJS)): XTERM_FLAGS = -DOLDXAW
$(filter $(XTERM_BUILD)/libXmu/%,$(XTERM_OBJS)): XTERM_FLAGS = -DBITMAPDIR='"/usr/X11/share/bitmaps"'
$(filter $(XTERM_BUILD)/xterm/%,$(XTERM_OBJS)): XTERM_FLAGS = -DOLDXAW -U__linux__

$(XTERM_OBJS): $(XTERM_BUILD)/%.o: $(XTERM_VENDOR)/%.c $(SDK)/include/.ready $(FVWM_BUILD)/include/X11/Xpoll.h $(XTERM)/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(XTERM_CFLAGS) $(XTERM_FLAGS) -I$(dir $<) -c -o $@ $<

$(XTERM_BUILD)/native.o: $(XTERM)/compat/native.c $(SDK)/include/.ready $(XTERM)/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(SDK_CFLAGS) -g0 -Os -c -o $@ $<

$(XTERM_BUILD)/libwidgets.a: $(XTERM_OBJS)
	$(call quiet,AR,$@)
	$(Q)rm -f $@
	$(Q)$(AR) rcs $@ $^

$(XTERM_PROGRAM): $(XTERM_BUILD)/native.o $(SDK)/crt0.o $(SDK)/user.ld $(XTERM_BUILD)/libwidgets.a $(SDK)/libX11.a $(SDK)/libc.a $(SDK)/libm.a
	$(Q)mkdir -p $(@D)
	$(call quiet,LD,$@)
	$(Q)$(SMALL_LINK) -s --gc-sections -T $(SDK)/user.ld -o $@ $(filter %.o,$^) \
	  --start-group $(filter %.a,$^) --end-group
endif
