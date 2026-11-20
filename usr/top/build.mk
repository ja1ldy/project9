# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

ifdef TOP_PROGRAM
TOP := usr/top
TOP_OBJS := $(patsubst $(TOP)/%.c,$(BUILD)/top/%.o,$(wildcard $(TOP)/vendor/*.c $(TOP)/compat/*.c $(TOP)/*.c))

.PHONY: top
top: $(TOP_PROGRAM)

$(TOP_OBJS): $(BUILD)/top/%.o: $(TOP)/%.c $(SDK)/include/.ready $(TOP)/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(SDK_CFLAGS) -g0 -Os -I$(TOP)/vendor -I$(TOP)/compat \
	  -include $(TOP)/compat/port.h -c -o $@ $<

$(TOP_PROGRAM): $(TOP_OBJS) $(SDK)/crt0.o $(SDK)/user.ld $(SDK)/libcurses.a $(SDK)/libc.a $(SDK)/libm.a
	$(Q)mkdir -p $(@D)
	$(call quiet,LD,$@)
	$(Q)$(SMALL_LINK) -s --gc-sections -T $(SDK)/user.ld -o $@ $(filter %.o,$^) \
	  --start-group $(filter %.a,$^) --end-group
endif
