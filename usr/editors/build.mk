# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

ifneq ($(VI_PROGRAM),)
VI_DIR := usr/editors/neatvi
VI_OBJECTS := $(patsubst $(VI_DIR)/%.c,$(BUILD)/vi/%.o,$(wildcard $(VI_DIR)/*.c))
$(VI_OBJECTS): $(BUILD)/vi/%.o: $(VI_DIR)/%.c $(SDK)/include/.ready usr/editors/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(SDK_CFLAGS) -Os -c -o $@ $<

$(VI_PROGRAM): $(VI_OBJECTS) $(SDK)/crt0.o $(SDK)/user.ld $(SDK)/libc.a
	$(call quiet,LD,$@)
	$(Q)$(SMALL_LINK) -s --gc-sections -T $(SDK)/user.ld -o $@ $(filter %.o,$^) $(SDK)/libc.a
endif
