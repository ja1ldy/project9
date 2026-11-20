# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

ifdef FVWM_PROGRAM
FVWM := usr/x11/fvwm
FVWM_BUILD := $(BUILD)/fvwm
FVWM_XLIB := $(FVWM)/vendor/libX11
FVWM_OPENBSD := $(FVWM)/vendor/openbsd
FVWM_PROTO := usr/x11/vendor/xorgproto/include
FVWM_CFLAGS = $(SDK_CFLAGS) -Os -std=gnu99 -DPROJECT9 -fcommon \
  -I$(FVWM)/include -I$(FVWM_XLIB)/include -I$(FVWM_XLIB)/include/X11 \
  -I$(FVWM_XLIB)/src/xcms -I$(FVWM_XLIB)/src/xlibi18n \
  -I$(FVWM_PROTO) -I$(FVWM_BUILD)/include
FVWM_XLIB_OBJS := $(patsubst $(FVWM_XLIB)/src/%.c,$(FVWM_BUILD)/X11/%.o,$(wildcard $(FVWM_XLIB)/src/*.c))
FVWM_OBJS := $(patsubst $(FVWM_OPENBSD)/%.c,$(FVWM_BUILD)/openbsd/%.o,$(wildcard $(FVWM_OPENBSD)/fvwm/*.c $(FVWM_OPENBSD)/libs/*.c))
FVWM_LOCALE_OBJS := $(patsubst $(FVWM_XLIB)/src/xlibi18n/%.c,$(FVWM_BUILD)/locale/%.o,$(wildcard $(FVWM_XLIB)/src/xlibi18n/*.c))
FVWM_PORT_OBJS := $(addprefix $(FVWM_BUILD)/,xtransport.o compat.o colors.o)

X11_SDK_FROM_LIB := Xlib.h Xresource.h Xutil.h cursorfont.h
X11_SDK_FROM_PROTO := X.h Xatom.h Xfuncproto.h Xfuncs.h Xmd.h Xosdefs.h keysym.h keysymdef.h
X11_SDK_HEADERS := $(addprefix $(SDK)/include/X11/,$(X11_SDK_FROM_LIB) $(X11_SDK_FROM_PROTO))
SDK_FILES += $(SDK)/libX11.a $(X11_SDK_HEADERS) $(SDK)/X11-LICENSES.txt

$(SDK)/X11-LICENSES.txt: $(FVWM_XLIB)/COPYING $(wildcard $(FVWM_PROTO)/../COPYING-*)
	$(Q)mkdir -p $(@D)
	$(Q)cat $^ > $@

$(addprefix $(SDK)/include/X11/,$(X11_SDK_FROM_LIB)): $(SDK)/include/X11/%: $(FVWM_XLIB)/include/X11/%
	$(Q)mkdir -p $(@D)
	$(Q)cp $< $@.tmp
	$(Q)mv $@.tmp $@

$(addprefix $(SDK)/include/X11/,$(X11_SDK_FROM_PROTO)): $(SDK)/include/X11/%: $(FVWM_PROTO)/X11/%
	$(Q)mkdir -p $(@D)
	$(Q)cp $< $@.tmp
	$(Q)mv $@.tmp $@

$(SDK)/libX11.a: $(FVWM_BUILD)/libX11.a
	$(Q)mkdir -p $(@D)
	$(call quiet,COPY,$@)
	$(Q)cp $< $@

$(FVWM_BUILD)/makekeys: $(FVWM_XLIB)/src/util/makekeys.c
	$(Q)mkdir -p $(@D)
	$(call quiet,HOSTCC,$@)
	$(Q)$(HOSTCC) -I$(FVWM_PROTO) -o $@ $<

$(FVWM_BUILD)/include/ks_tables.h: $(FVWM_BUILD)/makekeys $(FVWM_PROTO)/X11/keysymdef.h
	$(Q)mkdir -p $(@D)
	$(call quiet,GEN,$@)
	$(Q)$< < $(word 2,$^) > $@.tmp
	$(Q)mv $@.tmp $@

$(FVWM_BUILD)/include/config.h: $(FVWM)/xlib-config.h
	$(Q)mkdir -p $(@D)
	$(Q)cp $< $@

$(FVWM_BUILD)/config/config.h: $(FVWM)/fvwm-config.h
	$(Q)mkdir -p $(@D)
	$(Q)cp $< $@

$(FVWM_BUILD)/include/X11/Xpoll.h: $(FVWM_PROTO)/X11/Xpoll.h.in
	$(Q)mkdir -p $(@D)
	$(Q)sed 's/@USE_FDS_BITS@/bits/g' $< > $@

$(FVWM_XLIB_OBJS): $(FVWM_BUILD)/X11/%.o: $(FVWM_XLIB)/src/%.c $(SDK)/include/.ready $(FVWM_BUILD)/include/config.h $(FVWM_BUILD)/include/ks_tables.h $(FVWM_BUILD)/include/X11/Xpoll.h $(FVWM)/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(FVWM_CFLAGS) -DHAVE_CONFIG_H -DX11_t -DTRANS_CLIENT -c -o $@ $<

$(FVWM_LOCALE_OBJS): $(FVWM_BUILD)/locale/%.o: $(FVWM_XLIB)/src/xlibi18n/%.c $(SDK)/include/.ready $(FVWM_BUILD)/include/config.h $(FVWM_BUILD)/include/X11/Xpoll.h $(FVWM)/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(FVWM_CFLAGS) -g0 -DHAVE_CONFIG_H -DNO_XLOCALEDIR -DMB_LEN_MAX=1 -c -o $@ $<

$(FVWM_OBJS): $(FVWM_BUILD)/openbsd/%.o: $(FVWM_OPENBSD)/%.c $(SDK)/include/.ready $(FVWM_BUILD)/config/config.h $(FVWM)/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(FVWM_CFLAGS) -I$(FVWM_OPENBSD)/libs -include $(FVWM_BUILD)/config/config.h -iquote $(FVWM_BUILD)/config -c -o $@ $<

$(FVWM_PORT_OBJS): $(FVWM_BUILD)/%.o: $(FVWM)/%.c $(SDK)/include/.ready $(FVWM)/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(FVWM_CFLAGS) -c -o $@ $<

$(FVWM_BUILD)/libX11.a: $(FVWM_XLIB_OBJS) $(FVWM_LOCALE_OBJS) $(filter-out %/colors.o,$(FVWM_PORT_OBJS))
	$(call quiet,AR,$@)
	$(Q)rm -f $@
	$(Q)$(AR) rcs $@ $^

$(FVWM_PROGRAM): $(FVWM_OBJS) $(FVWM_BUILD)/colors.o $(SDK)/crt0.o $(SDK)/user.ld $(FVWM_BUILD)/libX11.a $(SDK)/libc.a $(SDK)/libm.a
	$(Q)mkdir -p $(@D)
	$(call quiet,LD,$@)
	$(Q)$(SMALL_LINK) -s --gc-sections -T $(SDK)/user.ld -o $@ $(filter %.o,$^) \
	  --start-group $(filter %.a,$^) --end-group
endif
