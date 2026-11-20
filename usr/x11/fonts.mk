# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

ifneq ($(X11_APPS),)
FT_DIR := usr/x11/vendor/freetype
FT_CFLAGS := -I$(FT_DIR)/include -Iusr/x11/include
FT_SOURCES := base/ftbase.c base/ftinit.c base/ftsystem.c base/ftdebug.c \
  base/ftbitmap.c base/ftmm.c truetype/truetype.c sfnt/sfnt.c smooth/smooth.c gzip/ftgzip.c
FT_OBJECTS := $(patsubst %.c,$(BUILD)/ft/%.o,$(FT_SOURCES))
FT_LIBRARY := $(BUILD)/x11/libfreetype.a

$(FT_OBJECTS): $(BUILD)/ft/%.o: $(FT_DIR)/src/%.c $(SDK)/include/.ready usr/x11/fonts.mk usr/x11/include/ftconfig/ftmodule.h
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(SDK_CFLAGS) -Os $(FT_CFLAGS) -DFT2_BUILD_LIBRARY \
	  -DFT_CONFIG_MODULES_H='<ftconfig/ftmodule.h>' -c -o $@ $<

$(FT_LIBRARY): $(FT_OBJECTS)
	$(Q)mkdir -p $(@D)
	$(call quiet,AR,$@)
	$(Q)$(AR) rcs $@ $^

$(BUILD)/x11/p9font.o: TINYX_CFLAGS += $(FT_CFLAGS)

$(BUILD)/x11/p9font: $(BUILD)/x11/p9font.o $(BUILD)/x11/xclient.o $(BUILD)/x11/compat.o \
  $(FT_LIBRARY) $(SDK)/crt0.o $(SDK)/user.ld $(SDK)/libc.a $(SDK)/libm.a
	$(call quiet,LD,$@)
	$(Q)$(SMALL_LINK) -s --gc-sections -T $(SDK)/user.ld -o $@ $(filter %.o,$^) \
	  --start-group $(FT_LIBRARY) $(SDK)/libc.a $(SDK)/libm.a --end-group
endif
