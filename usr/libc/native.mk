ifneq ($(and $(KERNEL64),$(TOP32)),)
SDK64 := $(WIDE)/sdk
SDK64_CFLAGS = -nostdinc -I$(SDK)/include -isystem $(shell $(CC64) -print-file-name=include) \
  $(filter-out -Iinclude,$(USER64_FLAGS)) -Os
SDK64_NETBSD := $(filter-out $(SDK_VENDOR)/libm/% $(SDK_VENDOR)/regex/%,$(SDK_NETBSD_C))
SDK64_OBJS := $(patsubst %.c,$(WIDE)/%.o,$(SDK_INPUT) $(SDK64_NETBSD)) \
  $(addprefix $(WIDE)/usr/lib/,env.o termios.o umalloc.o usys.o)

$(WIDE)/usr/libc/%.o: usr/libc/%.c $(SDK)/include/.ready usr/libc/native.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC64) $(SDK64_CFLAGS) -c -o $@ $<

$(WIDE)/usr/libc/vendor/%.o: usr/libc/vendor/%.c $(SDK)/include/.ready $(SDK_COMPAT) usr/libc/native.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC64) $(SDK64_CFLAGS) -Iusr/libc/compat -fexcess-precision=standard -c -o $@ $<

$(WIDE)/usr/libc/vendor/netbsd/gdtoa/%.o: SDK64_CFLAGS += $(SDK_GDTOA_FLAGS)
$(WIDE)/usr/libc/vendor/netbsd/time/%.o: SDK64_CFLAGS += $(SDK_TIME_FLAGS)

$(SDK64)/crt0.o: usr/arch/x86_64/start.S usr/libc/native.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,AS,$@)
	$(Q)$(CC64) $(USER64_FLAGS) -DP9_LIBC_START -c -o $@ $<

$(SDK64)/libc.a: $(SDK64_OBJS)
	$(Q)mkdir -p $(@D)
	$(call quiet,AR,$@)
	$(Q)rm -f $@
	$(Q)$(AR) rcs $@ $^

$(WIDE)/usr/curses/screen.o: usr/curses/screen.c $(SDK)/include/.ready usr/libc/native.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC64) $(SDK64_CFLAGS) -c -o $@ $<
endif
