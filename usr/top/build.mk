# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

ifdef TOP32
TOP := usr/top
TOP_SOURCES := $(wildcard $(TOP)/vendor/*.c $(TOP)/compat/*.c $(TOP)/*.c)
TOP_OBJS := $(patsubst $(TOP)/%.c,$(BUILD)/top/%.o,$(TOP_SOURCES))
TOP_CFLAGS := -I$(TOP)/vendor -I$(TOP)/compat -include $(TOP)/compat/port.h

$(TOP_OBJS): $(BUILD)/top/%.o: $(TOP)/%.c $(SDK)/include/.ready $(TOP)/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(SDK_CFLAGS) -g0 -Os $(TOP_CFLAGS) -c -o $@ $<

$(TOP32): $(TOP_OBJS) $(SDK)/crt0.o $(SDK)/user.ld $(SDK)/libcurses.a $(SDK)/libc.a $(SDK)/libm.a
	$(Q)mkdir -p $(@D)
	$(call quiet,LD,$@)
	$(Q)$(SMALL_LINK) -s --gc-sections -T $(SDK)/user.ld -o $@ $(filter %.o,$^) \
	  --start-group $(filter %.a,$^) --end-group

ifdef KERNEL64
TOP64 := $(WIDE)/bin/top
TOP64_OBJS := $(patsubst $(TOP)/%.c,$(WIDE)/top/%.o,$(TOP_SOURCES))

$(TOP64_OBJS): $(WIDE)/top/%.o: $(TOP)/%.c $(SDK)/include/.ready $(TOP)/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC64) $(SDK64_CFLAGS) $(TOP_CFLAGS) -c -o $@ $<

$(TOP64): $(SDK64)/crt0.o $(TOP64_OBJS) $(WIDE)/usr/curses/screen.o $(SDK64)/libc.a usr/arch/x86_64/user.ld
	$(Q)mkdir -p $(@D)
	$(call quiet,LD,$@)
	$(Q)$(USER64_LINK) -T usr/arch/x86_64/user.ld -o $@ $(filter %.o %.a,$^)

$(TOP_PROGRAM): $(TOP32) $(TOP64) tools/fatpack.py
	$(Q)mkdir -p $(@D)
	$(call quiet,PACK,$@)
	$(Q)$(PYTHON) tools/fatpack.py $@ $< $(word 2,$^)
endif
endif
