/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_DEVICES_H
#define PROJECT9_DEVICES_H

#define CONSOLE 1
#define DEV_KEYBOARD 2
#define DEV_POINTER 3
#define DEV_FB 4
#define DEV_MEMORY 5
#define DEV_RTC 6
#define DEV_USB 7
#define DEV_PTY_MASTER 8
#define DEV_PTY_SLAVE 9
#define P9_NPTY 8
#define USB_DISK_COUNT 8

#define POINTER_MOUSE 0
#define POINTER_TOUCHPAD 1
#define MEMORY_NULL 0
#define MEMORY_ZERO 1

#define INPUT_KEY 1
#define INPUT_RELATIVE 2

// raw keyboard bytes use the controller translated set one stream
struct input_event {
  unsigned int type;
  int x, y;
  unsigned int buttons;
  unsigned int code;
};

#define KBDIOGETMODE 0x6b01
#define KBD_MODE_SET1 1

#define FBIOGETINFO 0x6601

struct fbinfo {
  unsigned int width, height, stride, bits_per_pixel;
};

#endif
