/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "loader.h"
#include "x86.h"
#include "../../drivers/tty/gallant.h"

#define WIDTH 640
#define HEIGHT 480
#define TITLE_W 336
#define TITLE_H 84
#define LOGO_W 252
#define LOGO_H 84
#define DARK 0
#define GRAY 1
#define WHITE 2
#define BORDER 3
#define VBE_MODE 0x101
#define MODE_INFO 0x8000
#define MENU_X 28
#define MENU_Y 145
#define MENU_STEP 17
#define CELL_W 9
#define CELL_H 16

extern const uchar _binary_menu_vga_start[], _binary_menu_vga_end[];

static struct {
  uint width, height, pitch;
  uint address, granularity, window_size, bank_base, window;
  int banked, failed;
} video;

static uint
word(const uchar *p)
{
  return p[0] | (p[1] << 8);
}

static void
set_bank(uint offset)
{
  uint bank = offset / video.granularity;

  bios(0x10, 0x4f05, video.window, bank);
  __v86int();
  if((__v86.eax & 0xffff) != 0x004f)
    video.failed = 1;
  video.bank_base = bank * video.granularity;
}

static int
vbe_mode(void)
{
  uchar *info = physical(MODE_INFO);
  uint flags, window, segment, granularity, window_size, pitch;

  stosb(info, 0, 256);
  bios(0x10, 0x4f01, 0, 0);
  __v86.ecx = VBE_MODE;
  __v86.edi = MODE_INFO;
  __v86int();
  if((__v86.eax & 0xffff) != 0x004f)
    return 0;

  flags = word(info);
  if((flags & 0x51) != 0x11 || info[25] != 8 || info[27] != 4 ||
     word(info+18) != WIDTH || word(info+20) != HEIGHT)
    return 0;
  window = (info[2] & 5) == 5 ? 0 : 1;
  if((info[2+window] & 5) != 5)
    return 0;

  segment = word(info + 8 + 2*window);
  granularity = word(info+4) * 1024;
  window_size = word(info+6) * 1024;
  pitch = word(info+16);
  if(segment < 0xa000 || ((segment << 4) + window_size) > 0xc0000 ||
     !granularity || granularity > window_size || pitch < WIDTH)
    return 0;

  bios(0x10, 0x4f02, VBE_MODE, 0);
  __v86int();
  if((__v86.eax & 0xffff) != 0x004f)
    return 0;

  video.width = WIDTH;
  video.height = HEIGHT;
  video.pitch = pitch;
  video.address = segment << 4;
  video.granularity = granularity;
  video.window_size = window_size;
  video.window = window;
  video.banked = 1;
  video.failed = 0;
  set_bank(0);
  return !video.failed;
}

static int
vga_mode(void)
{
  bios(0x10, 0x13, 0, 0);
  __v86int();
  bios(0x10, 0x0f00, 0, 0);
  __v86int();
  if((__v86.eax & 0x7f) != 0x13)
    return 0;

  video.width = video.pitch = 320;
  video.height = 200;
  video.address = 0xa0000;
  video.bank_base = 0;
  video.window_size = 64000;
  video.banked = video.failed = 0;
  return 1;
}

static void
span(uint offset, const uchar *pixels, uint count, uchar color)
{
  uint available, i;
  volatile uchar *dst;

  while(count && !video.failed){
    if(video.banked &&
       (offset < video.bank_base || offset - video.bank_base >= video.window_size)){
      set_bank(offset);
      if(video.failed)
        return;
    }
    available = video.window_size - (offset - video.bank_base);
    if(available > count)
      available = count;
    dst = physical(video.address + offset - video.bank_base);
    if(pixels){
      for(i = 0; i < available; i++)
        dst[i] = *pixels++;
    } else {
      stosb((void*)dst, color, available);
    }
    offset += available;
    count -= available;
  }
}

static void
rectangle(uint x, uint y, uint w, uint h, uchar color)
{
  uint left = x * video.width / WIDTH;
  uint right = (x+w) * video.width / WIDTH;
  uint row = y * video.height / HEIGHT;
  uint bottom = (y+h) * video.height / HEIGHT;

  for(; row < bottom; row++)
    span(row * video.pitch + left, 0, right-left, color);
}

static void
scanline(uint x, uint y, const uchar *pixels, uint count)
{
  uchar scaled[WIDTH];
  uint left = x * video.width / WIDTH;
  uint right = (x+count) * video.width / WIDTH;
  uint row = y * video.height / HEIGHT;
  uint bottom = (y+1) * video.height / HEIGHT;
  uint i;

  if(row == bottom)
    return;
  if(video.width != WIDTH){
    for(i = 0; i < right-left; i++)
      scaled[i] = pixels[i * count / (right-left)];
    pixels = scaled;
  }
  for(; row < bottom; row++)
    span(row * video.pitch + left, pixels, right-left, 0);
}

static const uchar *
bitmap(const uchar *asset, uint x, uint y, uint w, uint h)
{
  uchar pixels[TITLE_W];
  uint row, col, count;
  uchar color;

  // each run stays inside its row
  for(row = 0; row < h; row++){
    col = 0;
    while(col < w){
      if(asset + 2 > _binary_menu_vga_end)
        return 0;
      count = *asset++;
      color = *asset++;
      if(!count || count > w-col)
        return 0;
      while(count--)
        pixels[col++] = color;
    }
    scanline(x, y+row, pixels, w);
  }
  return asset;
}

static void
label(uint x, uint y, const char *text, uchar color)
{
  uchar pixels[WIDTH];
  uint row, col, c, bit, used, coverage, shade;
  ushort top, bottom, left, right;

  // sample each glyph at two points in each direction to keep thin strokes
  shade = color == WHITE ? 31 : 23;
  for(row = 0; row < CELL_H; row++){
    used = 0;
    for(col = 0; text[col] && x + used + CELL_W <= WIDTH; col++){
      c = (uchar)text[col];
      if(c < ' ' || c > '~')
        c = ' ';
      top = gallant[c-' '][(row*4+1)*22 / (CELL_H*4)];
      bottom = gallant[c-' '][(row*4+3)*22 / (CELL_H*4)];
      for(bit = 0; bit < CELL_W; bit++){
        left = 0x8000U >> ((bit*4+1)*12 / (CELL_W*4));
        right = 0x8000U >> ((bit*4+3)*12 / (CELL_W*4));
        coverage = !!(top & left) + !!(top & right) +
                   !!(bottom & left) + !!(bottom & right);
        pixels[used++] = coverage ? 16 + (coverage * shade + 2) / 4 : DARK;
      }
    }
    scanline(x, y+row, pixels, used);
  }
}

static void
frame(void)
{
  rectangle(12, 125, 316, 2, BORDER);
  rectangle(12, 319, 316, 2, BORDER);
  rectangle(8, 129, 2, 188, BORDER);
  rectangle(330, 129, 2, 188, BORDER);
  rectangle(10, 127, 2, 2, BORDER);
  rectangle(328, 127, 2, 2, BORDER);
  rectangle(10, 317, 2, 2, BORDER);
  rectangle(328, 317, 2, 2, BORDER);
  rectangle(76, 120, 188, 16, DARK);
  label(85, 117, "Welcome to Project9", WHITE);
}

void
boot_video_selection(const char *const *items, int count, int selected)
{
  int i;

  for(i = 0; i < count; i++){
    int y = MENU_Y + MENU_STEP*i;
    label(MENU_X - 2*CELL_W, y, i == selected ? ">" : " ", WHITE);
    label(MENU_X, y, items[i], i == selected ? WHITE : GRAY);
  }
}

void
boot_video_status(const char *message)
{
  rectangle(10, 346, WIDTH-20, CELL_H, DARK);
  label(10, 346, message, GRAY);
}

int
boot_video_menu(const char *const *items, int count, int selected)
{
  const uchar *asset = _binary_menu_vga_start;
  uint i;

  if(_binary_menu_vga_start + 256 * 3 > _binary_menu_vga_end ||
     (!vbe_mode() && !vga_mode()))
    return 0;

  outb(0x3c8, 0);
  for(i = 0; i < 256 * 3; i++)
    outb(0x3c9, *asset++);
  rectangle(0, 0, WIDTH, HEIGHT, DARK);
  asset = bitmap(asset, 10, 13, TITLE_W, TITLE_H);
  if(!asset || !bitmap(asset, 368, 168, LOGO_W, LOGO_H))
    return 0;

  frame();
  boot_video_selection(items, count, selected);
  label(MENU_X, 268, "Enter boot   Space pause", GRAY);
  label(MENU_X, 285, "Esc prompt   R reboot", GRAY);
  return !video.failed;
}
