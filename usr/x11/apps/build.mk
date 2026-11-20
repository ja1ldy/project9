# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

ifdef X11_SERVER
XORG_APPS_DIR := usr/x11/apps
XORG_APPS_VENDOR := $(XORG_APPS_DIR)/vendor
XORG_APPS_BUILD := $(BUILD)/x11-upstream
XORG_APP_NAMES := $(notdir $(X11_UPSTREAM))
XORG_APP_DEPS := $(XTERM_BUILD)/native.o $(SDK)/crt0.o $(SDK)/user.ld $(XTERM_BUILD)/libwidgets.a $(SDK)/libX11.a $(SDK)/libc.a $(SDK)/libm.a
XORG_APP_FLAGS := -I$(XORG_APPS_DIR)/include -DMAXHOSTNAMELEN=64 -DOLDXAW
$(XORG_APPS_BUILD)/xrdb/xrdb.o: XORG_APP_FLAGS += -DHAS_MKSTEMP
$(XORG_APPS_BUILD)/xinit/xinit.o: XORG_APP_FLAGS += -D_POSIX_SOURCE -DRETSIGTYPE=void -DBINDIR='"/usr/X11/bin"'
XORG_APP_SRC := $(foreach app,$(XORG_APP_NAMES),$(wildcard $(XORG_APPS_VENDOR)/$(app)/*.c))
XORG_APP_OBJS := $(patsubst $(XORG_APPS_VENDOR)/%.c,$(XORG_APPS_BUILD)/%.o,$(XORG_APP_SRC))


$(XORG_APP_OBJS): $(XORG_APPS_BUILD)/%.o: $(XORG_APPS_VENDOR)/%.c $(SDK)/include/.ready $(FVWM_BUILD)/include/X11/Xpoll.h $(XORG_APPS_DIR)/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(XTERM_CFLAGS) $(XORG_APP_FLAGS) -I$(dir $<) -c -o $@ $<

define xorg-app
$(XORG_APPS_BUILD)/bin/$(1): $(filter $(XORG_APPS_BUILD)/$(1)/%,$(XORG_APP_OBJS)) $(XORG_APP_DEPS)
	$$(Q)mkdir -p $$(@D)
	$$(call quiet,LD,$$@)
	$$(Q)$$(SMALL_LINK) -s --gc-sections -T $$(SDK)/user.ld -o $$@ $$(filter %.o,$$^) 	  --start-group $$(filter %.a,$$^) --end-group
endef

$(foreach app,$(XORG_APP_NAMES),$(eval $(call xorg-app,$(app))))

$(XORG_APPS_BUILD)/bin/xrdb: $(XORG_APPS_BUILD)/xrdb-temp.o

$(XORG_APPS_BUILD)/xrdb-temp.o: $(XORG_APPS_DIR)/compat/xrdb-temp.c $(SDK)/include/.ready
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(SDK_CFLAGS) -c -o $@ $<


endif

ifdef X11_SERVER
$(X11_STARTX): $(XORG_APPS_VENDOR)/xinit/startx.cpp $(XORG_APPS_DIR)/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,GEN,$@)
	$(Q)$(CC) -E -P -x c -undef -DPROJECT9 -DSHELL_CMD=/bin/sh $< 	  | sed 's/^XCOMM */#/; s|^#! */|#!/|' > $@.tmp
	$(Q)chmod 755 $@.tmp
	$(Q)mv $@.tmp $@
endif
