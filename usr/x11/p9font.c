/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "xclient.h"

#define FONT "/usr/share/fonts/truetype/DejaVuSansMono.ttf"
enum { Width = 400, Height = 104 };
static const uint32_t bg = 0x151b22, fg = 0xe4f4ff;

static uint32_t
mix(unsigned alpha)
{
  uint32_t out = 0;
  for(int shift = 0; shift < 24; shift += 8){
    unsigned a = (fg >> shift) & 255, b = (bg >> shift) & 255;
    out |= ((a * alpha + b * (255-alpha))/255) << shift;
  }
  return out;
}

static int
draw(FT_Face face, uint32_t *pixels, const char *text, int size, int baseline)
{
  if(FT_Set_Pixel_Sizes(face, 0, size))
    return -1;
  int x = 12, count = 0;
  while(*text){
    if(FT_Load_Char(face, (unsigned char)*text++, FT_LOAD_RENDER))
      return -1;
    FT_GlyphSlot g = face->glyph;
    FT_Bitmap *bm = &g->bitmap;
    if(bm->pixel_mode != FT_PIXEL_MODE_GRAY && bm->pixel_mode != FT_PIXEL_MODE_MONO)
      return -1;
    if(bm->pitch < 0)
      return -1;
    for(unsigned y = 0; y < bm->rows; y++){
      int py = baseline - g->bitmap_top + (int)y;
      if(py < 0 || py >= Height)
        continue;
      const unsigned char *row = bm->buffer + y * bm->pitch;
      for(unsigned j = 0; j < bm->width; j++){
        int px = x + g->bitmap_left + (int)j;
        if(px < 0 || px >= Width)
          continue;
        unsigned alpha = bm->pixel_mode == FT_PIXEL_MODE_MONO ?
          (row[j/8] & (128U >> (j%8)) ? 255 : 0) : row[j];
        if(alpha){
          pixels[py*Width+px] = mix(alpha);
          count++;
        }
      }
    }
    x += g->advance.x >> 6;
    if(x >= Width)
      break;
  }
  return count;
}

static int
preview(struct display *d, const uint32_t *pixels)
{
  uint32_t id = d->setup.ridBase | 1, gcid = d->setup.ridBase | 2;
  XREQ(xCreateWindowReq, win, CreateWindow);
  win.parent = d->root.windowId;
  win.wid = id;
  win.class = InputOutput;
  win.x = (d->root.pixWidth - Width)/2;
  win.y = (d->root.pixHeight - Height)/2;
  win.width = Width;
  win.height = Height;
  win.mask = CWBackPixel | CWEventMask;
  win.length += 2;
  uint32_t values[] = {bg, ExposureMask | KeyPressMask};
  XREQ(xCreateGCReq, gc, CreateGC);
  gc.drawable = id;
  gc.gc = gcid;
  XREQ(xResourceReq, map, MapWindow);
  map.id = id;
  XREQ(xSetInputFocusReq, focus, SetInputFocus);
  focus.focus = id;
  focus.revertTo = RevertToParent;
  if(xrequest(d, &win, sizeof(win)) || xwrite(d, values, sizeof(values)) ||
     xrequest(d, &gc, sizeof(gc)) || xrequest(d, &map, sizeof(map)) ||
     xrequest(d, &focus, sizeof(focus)) || xsync(d))
    return -1;
  for(;;){
    for(int y = 0; y < Height; y += 4){
      XREQ(xPutImageReq, img, PutImage);
      img.format = ZPixmap;
      img.drawable = id;
      img.gc = gcid;
      img.width = Width;
      img.height = 4;
      img.dstY = y;
      img.depth = 24;
      img.length += Width * img.height;
      if(xrequest(d, &img, sizeof(img)) ||
         xwrite(d, pixels+y*Width, Width * img.height * sizeof(*pixels)))
        return -1;
    }
    for(;;){
      xEvent event;
      if(xread(d, &event, sizeof(event)) < 0 || !event.u.u.type)
        return -1;
      if(event.u.u.type == KeyPress)
        return 0;
      if(event.u.u.type == Expose && !event.u.expose.count)
        break;
    }
  }
}

int
main(int argc, char **argv)
{
  int check = argc > 1 && !strcmp(argv[1], "--check");
  int index = 1 + check;
  if(argc > index + 1){
    fputs("usage: p9font [--check] [font.ttf]\n", stderr);
    return 2;
  }
  const char *file = argc > index ? argv[index] : FONT;
  FT_Library lib = NULL;
  FT_Face face = NULL;
  uint32_t *pixels = malloc(Width * Height * sizeof(*pixels));
  int result = 1;
  if(!pixels || FT_Init_FreeType(&lib) || FT_New_Face(lib, file, 0, &face))
    goto done;
  for(int i = 0; i < Width * Height; i++)
    pixels[i] = bg;
  if(draw(face, pixels, "Project9 TrueType", 27, 41) <= 0 ||
     draw(face, pixels, "FreeType  |  Press any key to close", 14, 75) <= 0)
    goto done;

  if(check){
    puts("TrueType raster check passed");
  } else {
    struct display d;
    if(xopen(&d) < 0){
      perror("p9font: cannot open display");
      goto done;
    }
    int painted = d.root.rootDepth == 24 && d.root.pixWidth >= Width &&
      d.root.pixHeight >= Height && preview(&d, pixels) == 0;
    close(d.fd);
    if(!painted)
      goto done;
  }
  result = 0;
done:
  free(pixels);
  if(face) FT_Done_Face(face);
  if(lib) FT_Done_FreeType(lib);
  if(result)
    fprintf(stderr, "p9font: cannot render font %s\n", file);
  return result;
}
