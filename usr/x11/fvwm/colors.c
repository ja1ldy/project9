/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */
#include <X11/Xlib.h>

extern Colormap PictureCMap;
extern Display *PictureSaveDisplay;

static unsigned
mix(unsigned c, unsigned target)
{
  return (3 * c + target) / 4;
}

static unsigned long
shade(unsigned long pixel, unsigned target)
{
  XColor color = { .pixel = pixel };
  if(!XQueryColor(PictureSaveDisplay, PictureCMap, &color))
    return pixel;
  color.red = mix(color.red, target);
  color.green = mix(color.green, target);
  color.blue = mix(color.blue, target);
  return XAllocColor(PictureSaveDisplay, PictureCMap, &color) ? color.pixel : pixel;
}

unsigned long GetShadow(unsigned long p) { return shade(p, 0); }
unsigned long GetHilite(unsigned long p) { return shade(p, 65535); }
