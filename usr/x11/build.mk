# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

include usr/x11/sources.mk

X11_DIR := usr/x11
TINYX_VENDOR := $(X11_DIR)/vendor/tinyx
XORGPROTO := $(X11_DIR)/vendor/xorgproto/include
TINYX_DIRS := include dix fb mi Xext render randr xfixes damageext dbe Xi \
  miext/damage miext/shadow kdrive/src kdrive/memory os
TINYX_HEADERS := $(wildcard $(X11_DIR)/include/*.h $(X11_DIR)/*.h)
TINYX_CFLAGS = $(SDK_CFLAGS) -Os -std=gnu99 -I$(X11_DIR)/include -I$(XORGPROTO) \
  $(addprefix -I$(TINYX_VENDOR)/,$(TINYX_DIRS)) -include $(X11_DIR)/include/compat.h
TINYX_DEFINES := -DHAVE_CONFIG_H -DHAVE_DIX_CONFIG_H -DHAVE_KDRIVE_CONFIG_H \
  -DTINYX_MEMORY_ONLY -DTINYX_NO_MITSHM -DVENDOR_STRING='"Project9"' -DVENDOR_RELEASE=1
TINYX_OBJECTS := $(patsubst %.c,$(BUILD)/x11/%.o,$(TINYX_SOURCES))
TINYX_HOST := $(addprefix $(BUILD)/x11/,main.o demo.o compat.o)

$(TINYX_OBJECTS): $(BUILD)/x11/%.o: $(TINYX_VENDOR)/%.c $(SDK)/include/.ready $(TINYX_HEADERS) $(X11_DIR)/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(TINYX_CFLAGS) $(TINYX_DEFINES) -c -o $@ $<

X11_APP_OBJECTS := $(addsuffix .o,$(X11_APPS)) $(BUILD)/x11/xclient.o

$(TINYX_HOST) $(X11_APP_OBJECTS): $(BUILD)/x11/%.o: $(X11_DIR)/%.c $(SDK)/include/.ready $(TINYX_HEADERS) $(X11_DIR)/build.mk
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(TINYX_CFLAGS) -c -o $@ $<

$(BUILD)/x11/tinyx: $(TINYX_HOST) $(TINYX_OBJECTS) $(SDK)/crt0.o $(SDK)/user.ld $(SDK)/libm.a $(SDK)/libc.a
	$(call quiet,LD,$@)
	$(Q)$(SMALL_LINK) -s --gc-sections -T $(SDK)/user.ld -o $@ $(filter %.o,$^) \
	  --start-group $(SDK)/libm.a $(SDK)/libc.a --end-group

ifneq ($(X11_APPS),)
X11_STANDARD := $(filter-out $(BUILD)/x11/p9font,$(X11_APPS))
$(X11_STANDARD): %: %.o $(BUILD)/x11/xclient.o $(SDK)/crt0.o $(SDK)/user.ld $(SDK)/libc.a
	$(call quiet,LD,$@)
	$(Q)$(SMALL_LINK) -s --gc-sections -T $(SDK)/user.ld -o $@ $(filter %.o,$^) $(SDK)/libc.a
endif

include usr/x11/fonts.mk
