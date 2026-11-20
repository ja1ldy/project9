# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

ifdef VI32
VI_DIR := usr/editors/neatvi
VI_SOURCES := $(wildcard $(VI_DIR)/*.c)
VI_OBJECTS := $(patsubst $(VI_DIR)/%.c,$(BUILD)/vi/%.o,$(VI_SOURCES))
$(VI_OBJECTS): $(BUILD)/vi/%.o: $(VI_DIR)/%.c $(SDK)/include/.ready usr/editors/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(SDK_CFLAGS) -Os -c -o $@ $<

$(VI32): $(VI_OBJECTS) $(SDK)/crt0.o $(SDK)/user.ld $(SDK)/libc.a
	$(call quiet,LD,$@)
	$(Q)$(SMALL_LINK) -s --gc-sections -T $(SDK)/user.ld -o $@ $(filter %.o,$^) $(SDK)/libc.a

ifdef KERNEL64
VI64 := $(WIDE)/bin/vi
VI64_OBJS := $(patsubst $(VI_DIR)/%.c,$(WIDE)/vi/%.o,$(VI_SOURCES))

$(VI64_OBJS): $(WIDE)/vi/%.o: $(VI_DIR)/%.c $(SDK)/include/.ready usr/editors/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC64) $(SDK64_CFLAGS) -c -o $@ $<

$(VI64): $(SDK64)/crt0.o $(VI64_OBJS) $(SDK64)/libc.a usr/arch/x86_64/user.ld
	$(Q)mkdir -p $(@D)
	$(call quiet,LD,$@)
	$(Q)$(USER64_LINK) -T usr/arch/x86_64/user.ld -o $@ $(filter %.o %.a,$^)

$(VI_PROGRAM): $(VI32) $(VI64) tools/fatpack.py
	$(Q)mkdir -p $(@D)
	$(call quiet,PACK,$@)
	$(Q)$(PYTHON) tools/fatpack.py $@ $< $(word 2,$^)
endif
endif
