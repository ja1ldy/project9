/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_SCREEN_H
#define PROJECT9_SCREEN_H

#define BACKSPACE 0x100

enum {
  SCREEN_SET, SCREEN_MOVE, SCREEN_LINE, SCREEN_DISPLAY,
  SCREEN_SAVE, SCREEN_RESTORE, SCREEN_CURSOR
};

static inline int
screenbound(int value, int limit)
{
  return value < 0 ? 0 : value >= limit ? limit-1 : value;
}

#endif
