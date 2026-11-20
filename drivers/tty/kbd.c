/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "x86.h"
#include "defs.h"
#include "keys.h"
#include "kbd.h"

static int
keychar(uint data, uint *state)
{
  static uchar *charcode[4] = { normalmap, shiftmap, ctlmap, ctlmap };
  uint c, shift = *state;

  ps2key(data);
  if(data == 0xE0){
    *state = shift | E0ESC;
    return KBD_NOCHAR;
  } else if(data & 0x80){
    // a key release updates the modifier state without producing a byte
    data = (shift & E0ESC ? data : data & 0x7F);
    *state = shift & ~(shiftcode[data] | E0ESC);
    return KBD_NOCHAR;
  } else if(shift & E0ESC){
    // the e0 prefix selects the extended half of the key map
    data |= 0x80;
    shift &= ~E0ESC;
  }

  shift |= shiftcode[data];
  shift ^= togglecode[data];
  *state = shift;
  c = charcode[shift & (CTL | SHIFT)][data];
  if(shift & CAPSLOCK){
    if('a' <= c && c <= 'z')
      c += 'A' - 'a';
    else if('A' <= c && c <= 'Z')
      c += 'a' - 'A';
  }
  if(c == 0 && !(data == 0x39 && (shift & CTL)))
    return KBD_NOCHAR;
  return c >= KEY_HOME && c <= KEY_DEL ? KBD_SPECIAL | c : c;
}

int
kbdgetc(void)
{
  static uint state;
  uint st, data;

  st = inb(KBSTATP);
  if(st == 0xff || (st & KBS_DIB) == 0)
    return -1;
  data = inb(KBDATAP);
  if(st & 0xc0){
    if(st & 0x20)
      ps2aux(-1);
    return KBD_NOCHAR;
  }
  if(st & 0x20){
    ps2aux(data);
    return KBD_NOCHAR;
  }
  return keychar(data, &state);
}

static int injected = -1;

static int
usb_getc(void)
{
  int c = injected;
  injected = -1;
  return c;
}

void
usb_scancode(uint data)
{
  static uint state;
  injected = keychar(data, &state);
  if(injected != KBD_NOCHAR)
    consolekeyintr(usb_getc);
}

void
kbdintr(void)
{
  consolekeyintr(kbdgetc);
}
