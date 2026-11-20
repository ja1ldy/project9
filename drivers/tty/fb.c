/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "defs.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "screen.h"
#include "tty.h"
#include "devices.h"
#include "boothw.h"

#define BOCHS_WIDTH BOOT_FB_MAX_WIDTH
#define BOCHS_HEIGHT BOOT_FB_MAX_HEIGHT
#define FB_BPP 32
#define CELL_W 12
#define CELL_H 24
#define COLS (width / CELL_W)
#define ROWS (height / CELL_H)
#define PAD_X ((width - COLS * CELL_W) / 2)
#define FG_COLOR 0x00FFFFFF
#define BG_COLOR 0x00000000
#define CUR_COLOR 0x00FFFFFF

_Static_assert(BOOT_FB_MAX_WIDTH / CELL_W <= TTY_MAX_COLS &&
               BOOT_FB_MAX_HEIGHT / CELL_H <= TTY_MAX_ROWS,
               "terminal size exceeds pager limits");

static int width, height, pitch;

static uint fb_phys = 0;
static uint *fb_mem = (uint*)FB_VIRT;
static int fb_ready = 0;
static int mirror;
static int initialized;
static int graphics;
static int cursor_drawn;
static int top;
static int cur_col = 0;
static int cur_row = 0;
static uint cur_fg = FG_COLOR;
static uint cur_bg;
static uint saved_bg;
static int saved_col, saved_row, cursor_visible = 1;

static struct cell {
  uint fg, bg;
  uchar ch;
} text[BOOT_FB_MAX_HEIGHT / CELL_H][BOOT_FB_MAX_WIDTH / CELL_W];
static uint padding[NELEM(text)], scanline[BOOT_FB_MAX_WIDTH];
static int repaint;

void
fbstyle(int fg, int bg)
{
  static const uint colors[] = {
    0x000000, 0xaa0000, 0x00aa00, 0xaa5500,
    0x5555ff, 0xaa00aa, 0x00aaaa, 0xffffff,
    0x555555, 0xff5555, 0x50fa7b, 0xf1fa8c,
    0x58a6ff, 0xbd93f9, 0x8be9fd, 0xffffff
  };

  cur_fg = colors[fg];
  cur_bg = colors[bg];
}

#include "gallant.h"

static uint pci_read(uint bus, uint slot, uint func, uint offset)
{
  uint addr = (1U << 31) | (bus << 16) | (slot << 11) | (func << 8) | (offset & 0xfc);
  outsl(0xcf8, &addr, 1);
  uint val = 0;
  insl(0xcfc, &val, 1);
  return val;
}

static void vbe_write(ushort index, ushort val)
{
  outw(0x01ce, index);
  outw(0x01cf, val);
}

static ushort vbe_read(ushort index)
{
  outw(0x01ce, index);
  return inw(0x01cf);
}

static int
bochs(void)
{
  uint bus, slot, id, bar;

  for(bus = 0; bus < 256 && !fb_phys; bus++)
    for(slot = 0; slot < 32; slot++){
      id = pci_read(bus, slot, 0, 0);
      if(id != 0x11111234 && id != 0xbeef80ee)
        continue;
      bar = pci_read(bus, slot, 0, 0x10);
      if(!(bar & 1) && (bar & ~0xfU) >= PHYSTOP){
        fb_phys = bar & ~0xfU;
        break;
      }
    }
  if(!fb_phys || fb_phys > ~0U - BOCHS_WIDTH * BOCHS_HEIGHT * 8U)
    return 0;
  vbe_write(4, 0);
  vbe_write(1, BOCHS_WIDTH);
  vbe_write(2, BOCHS_HEIGHT);
  vbe_write(3, FB_BPP);
  vbe_write(4, 0x41);
  if(vbe_read(1) != BOCHS_WIDTH || vbe_read(2) != BOCHS_HEIGHT || vbe_read(3) != FB_BPP){
    vbe_write(4, 0);
    fb_phys = 0;
    return 0;
  }
  vbe_write(6, BOCHS_WIDTH);
  width = pitch = BOCHS_WIDTH;
  height = BOCHS_HEIGHT;

  // a second screen height lets the viewport cross the end of the ring
  mirror = vbe_read(7) >= 2 * height;
  return 1;
}

void
fbinit(void)
{
  const struct boothw *hw = P2V(BOOTHW_ADDR);

  if(bochs()){
    fb_ready = 1;
    return;
  }
  if(hw->magic != BOOTHW_MAGIC || hw->width < 640 || hw->height < 480 ||
     hw->width > BOOT_FB_MAX_WIDTH || hw->height > BOOT_FB_MAX_HEIGHT ||
     hw->stride < hw->width * 4 || hw->stride > BOOT_FB_MAX_STRIDE ||
     (hw->stride & 3) || hw->fbaddr < PHYSTOP || (hw->fbaddr & (PGSIZE-1)) ||
     hw->stride * hw->height > ~0U - hw->fbaddr)
    return;
  fb_phys = hw->fbaddr;
  width = hw->width;
  height = hw->height;
  pitch = hw->stride / sizeof(uint);
  fb_ready = 1;
}

void
fbtextsize(int *cols, int *rows)
{
  *cols = fb_ready ? COLS : TTY_COLS;
  *rows = fb_ready ? ROWS : TTY_ROWS;
}

int
fbactive(void)
{
  return fb_ready;
}

int
fb_map(pde_t *pgdir)
{
  if(!fb_ready)
    return 0;
  return mappages(pgdir, (void*)FB_VIRT, pitch * height * 4 * (mirror + 1),
                  fb_phys, PTE_W | PTE_PCD | PTE_PWT);
}

static void
fill(uint *dst, uint color, uint count)
{
  asm volatile("cld; rep stosl" : "+D"(dst), "+c"(count) : "a"(color) : "memory");
}

static void
draw(int col, int row, int ch, uint fg)
{
  ch = ch >= 32 && ch <= 126 ? ch : ' ';
  if(!mirror){
    text[row][col] = (struct cell){fg, cur_bg, ch};
    if(repaint)
      return;
  }
  const ushort *glyph = gallant[ch - ' '];
  int py = top + row * CELL_H;
  int y, x;
  uint *line, bits, pixel;

  if(py >= height)
    py -= height;
  line = fb_mem + py * pitch + PAD_X + col * CELL_W;
  for(y = 0; y < CELL_H; y++, line += pitch){
    bits = y < 22 ? glyph[y] : 0;
    for(x = 0; x < CELL_W; x++){
      pixel = (bits & (0x8000U >> x)) ? fg : cur_bg;
      line[x] = pixel;
      if(mirror)
        line[x + height * pitch] = pixel;
    }
  }
}

static void
cursor(int show)
{
  int py = (top + cur_row * CELL_H) % height + 22;
  uint *line = fb_mem + py * pitch + PAD_X + cur_col * CELL_W;
  int y;

  // the bottom two rows of every glyph cell contain only background
  if(show)
    saved_bg = mirror ? *line : text[cur_row][cur_col].bg;
  for(y = 0; y < 2; y++, line += pitch){
    fill(line, show ? CUR_COLOR : saved_bg, CELL_W);
    if(mirror)
      fill(line + height * pitch, show ? CUR_COLOR : saved_bg, CELL_W);
  }
  cursor_drawn = show;
}

static void
drawrows(void)
{
  for(int row = 0; row < ROWS; row++){
    fill(scanline, padding[row], width);
    for(int y = 0; y < CELL_H; y++){
      uint *dst = fb_mem + (row * CELL_H + y) * pitch;
      uint *pixels = scanline + PAD_X;

      for(int col = 0; col < COLS; col++){
        const struct cell *cell = &text[row][col];
        uint bits = y < 22 ? gallant[cell->ch - ' '][y] : 0;
        for(int x = 0; x < CELL_W; x++)
          *pixels++ = bits & (0x8000U >> x) ? cell->fg : cell->bg;
      }
      const uint *src = scanline;
      uint n = width;
      asm volatile("cld; rep movsl" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
    }
  }
  repaint = 0;
}

void
fbflush(void)
{
  if(repaint)
    drawrows();
  if(!graphics && initialized && cursor_visible && !cursor_drawn)
    cursor(1);
}

static void
blankrow(int row)
{
  for(int col = 0; col < COLS; col++)
    text[row][col] = (struct cell){cur_fg, cur_bg, ' '};
  padding[row] = cur_bg;
}

static void
clear(void)
{
  fill(fb_mem, cur_bg, pitch * height * (mirror + 1));
  if(mirror)
    vbe_write(9, 0);
  else
    for(int row = 0; row < ROWS; row++)
      blankrow(row);
  repaint = 0;
  top = cur_col = cur_row = 0;
}

static void
scroll(void)
{
  if(mirror){
    // the old top row becomes the blank bottom row after the offset moves
    uint *dst = fb_mem + top * pitch;
    fill(dst, cur_bg, CELL_H * pitch);
    fill(dst + height * pitch, cur_bg, CELL_H * pitch);
    top = (top + CELL_H) % height;
    vbe_write(9, top);
  } else {
    // scroll the text in ram and repaint once per console flush
    memmove(text, text + 1, (ROWS - 1) * sizeof(text[0]));
    memmove(padding, padding + 1, (ROWS - 1) * sizeof(padding[0]));
    blankrow(ROWS - 1);
    fill(fb_mem + ROWS * CELL_H * pitch, cur_bg, (height - ROWS * CELL_H) * pitch);
    repaint = 1;
  }
  cur_row = ROWS - 1;
}

static void
prepare(void)
{
  if(!initialized){
    initialized = 1;
    clear();
  }
  if(cursor_drawn)
    cursor(0);
}

static void
erase(int first, int last)
{
  int row, col, cells, py, y;
  uint *line;

  while(first < last){
    row = first / COLS;
    col = first % COLS;
    cells = COLS - col;
    if(cells > last-first)
      cells = last-first;
    if(!mirror)
      for(int i = 0; i < cells; i++)
        text[row][col + i] = (struct cell){cur_fg, cur_bg, ' '};
    py = (top + row * CELL_H) % height;
    line = fb_mem + py * pitch + PAD_X + col * CELL_W;
    for(y = 0; !repaint && y < CELL_H; y++, line += pitch){
      fill(line, cur_bg, cells * CELL_W);
      if(mirror)
        fill(line + height * pitch, cur_bg, cells * CELL_W);
    }
    first += cells;
  }
}

void
fbctl(int cmd, int a, int b)
{
  int first, last, pos;

  if(graphics)
    return;
  prepare();
  pos = cur_row * COLS + cur_col;
  switch(cmd){
  case SCREEN_SET:
    if(a >= 0)
      cur_col = screenbound(a, COLS);
    if(b >= 0)
      cur_row = screenbound(b, ROWS);
    break;
  case SCREEN_MOVE:
    cur_col = screenbound(cur_col+a, COLS);
    cur_row = screenbound(cur_row+b, ROWS);
    break;
  case SCREEN_SAVE:
    saved_col = cur_col;
    saved_row = cur_row;
    break;
  case SCREEN_RESTORE:
    cur_col = saved_col;
    cur_row = saved_row;
    break;
  case SCREEN_CURSOR:
    cursor_visible = a;
    break;
  case SCREEN_LINE: case SCREEN_DISPLAY:
    if(a < 0 || a > 2)
      break;
    first = cmd == SCREEN_LINE ? cur_row * COLS : 0;
    last = cmd == SCREEN_LINE ? first + COLS : ROWS * COLS;
    erase(a == 0 ? pos : first, a == 1 ? pos+1 : last);
    break;
  }
}

void
fbputc(int c)
{
  if(!fb_ready || graphics)
    return;
  prepare();
  if(c == 0x0c){
    clear();
  } else if(c == '\n' || c == '\r' || c == BACKSPACE){
    if(c == '\n'){
      cur_col = 0;
      cur_row++;
    } else if(c == '\r'){
      cur_col = 0;
    } else if(cur_col > 0 || cur_row > 0){
      if(cur_col == 0){
        cur_row--;
        cur_col = COLS;
      }
      cur_col--;
      draw(cur_col, cur_row, ' ', cur_fg);
    }
  } else if(c == '\t'){
    do {
      fbputc(' ');
    } while(cur_col % 8);
    return;
  } else {
    draw(cur_col, cur_row, c, cur_fg);
    if(++cur_col == COLS){
      cur_col = 0;
      cur_row++;
    }
  }
  if(cur_row == ROWS)
    scroll();
}

int
fbgetinfo(struct fbinfo *info)
{
  if(!fb_ready)
    return -1;
  *info = (struct fbinfo){ width, height, pitch * sizeof(uint), FB_BPP };
  return 0;
}

int
fbpixels(char *dst, uint count, uint off)
{
  uint first, size = pitch * height * sizeof(uint);
  char *buffer = (char*)fb_mem;

  if(off >= size)
    return 0;
  if(count > size - off)
    count = size - off;
  if(count > PGSIZE)
    count = PGSIZE;
  off = (off + top * pitch * sizeof(uint)) % size;
  first = count < size - off ? count : size - off;
  memmove(dst, buffer + off, first);
  if(first != count)
    memmove(dst + first, buffer, count - first);
  return count;
}

int
fbmapuser(pde_t *pgdir, uint len, int writable)
{
  uint size = PGROUNDUP(pitch * height * sizeof(uint));

  if(!fb_ready || !len || len > size || (fb_phys & (PGSIZE-1)) ||
     size - 1 > ~0U - fb_phys)
    return -1;
  return mappages(pgdir, (void*)FB_USER, len, fb_phys,
                  PTE_U | PTE_DEVICE | PTE_PCD | PTE_PWT | (writable ? PTE_W : 0));
}

void
fbdisplay(int count)
{
  if(graphics == count)
    return;
  if(!graphics || !count){
    clear();
    cursor_drawn = 0;
    cursor_visible = 1;
  }
  graphics = count;
}
