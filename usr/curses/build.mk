# Project9 contributions are under ISC. See LICENSE for scope and upstream terms.

.PHONY: curses
curses: $(SDK)/libcurses.a
libc: curses

$(BUILD)/usr/curses/screen.o: usr/curses/screen.c $(SDK)/include/.ready
	$(Q)mkdir -p $(@D)
	$(call quiet,CC,$@)
	$(Q)$(CC) $(SDK_CFLAGS) -Os -g0 -c -o $@ $<

$(SDK)/libcurses.a: $(BUILD)/usr/curses/screen.o
	$(call quiet,AR,$@)
	$(Q)rm -f $@
	$(Q)$(AR) rcs $@ $^
