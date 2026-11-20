/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "stat.h"
#include "user.h"
#include "pager.h"
#include "fcntl.h"
#include "tty.h"
#include "ioctl.h"
#include "devices.h"

#define PAGE_LIMIT (1024U * 1024)

struct page {
  char *text;
  uint length, capacity, scanned;
  int fd, eof, clipped;
  uint *rows;
  int count;
  int width, height;
};

enum {
  KEY_UP = 0x100, KEY_DOWN, KEY_PREV, KEY_NEXT, KEY_FIRST, KEY_LAST
};

static volatile int resumed;

static void
oncontinue(int sig)
{
  resumed = 1;
}

static int
writeall(const char *buf, int len)
{
  int n;

  while(len){
    n = write(1, buf, len);
    if(n <= 0)
      return -1;
    buf += n;
    len -= n;
  }
  return 0;
}

static int
stream(int fd)
{
  char buf[512];
  int n;

  while((n = read(fd, buf, sizeof(buf))) > 0)
    if(writeall(buf, n) < 0)
      return -1;
  return n;
}

// wrap at the screen edge and keep file bytes from moving the cursor
static int
row(const struct page *page, uint *offset, char *out)
{
  uint pos = *offset;
  int col = 0, c, spaces;

  while(pos < page->length){
    c = (uchar)page->text[pos];
    if(c == '\r' && pos + 1 < page->length && page->text[pos+1] == '\n'){
      pos++;
      continue;
    }
    if(c == '\n'){
      pos++;
      break;
    }
    if(col == page->width)
      break;
    pos++;
    spaces = c == '\t' ? 8 - col % 8 : 1;
    if(c == '\t')
      c = ' ';
    if(c < ' ' || c > '~')
      c = '?';
    while(spaces-- && col < page->width){
      if(out)
        out[col] = c;
      col++;
    }
  }
  *offset = pos;
  return col;
}

static int
grow(struct page *page)
{
  uint capacity = page->capacity ? page->capacity * 2 : 4096;
  char *text = malloc(capacity);
  uint *rows = malloc(capacity * sizeof(*rows));

  if(!text || !rows){
    if(text)
      free(text);
    if(rows)
      free(rows);
    return -1;
  }
  if(page->capacity){
    memmove(text, page->text, page->length);
    memmove(rows, page->rows, page->count * sizeof(*rows));
    free(page->text);
    free(page->rows);
  }
  page->text = text;
  page->rows = rows;
  page->capacity = capacity;
  return 0;
}

static int
extend(struct page *page, int wanted)
{
  uint end, room;
  int n;

  while(page->count < wanted){
    end = page->scanned;
    row(page, &end, 0);
    if(end != page->scanned &&
       (end < page->length || page->eof || page->text[end-1] == '\n')){
      page->rows[page->count++] = page->scanned;
      page->scanned = end;
      continue;
    }
    if(page->eof)
      break;
    if(page->length == PAGE_LIMIT){
      page->eof = page->clipped = 1;
      continue;
    }
    if(page->length == page->capacity && grow(page) < 0)
      return -1;
    room = page->capacity - page->length;
    n = read(page->fd, page->text + page->length, room > 512 ? 512 : room);
    if(n < 0)
      return -1;
    if(n == 0)
      page->eof = 1;
    else
      page->length += n;
  }
  return 0;
}

static char *
append(char *out, const char *s)
{
  while(*s)
    *out++ = *s++;
  return out;
}

static char *
number(char *out, uint n)
{
  char reverse[10];
  int used = 0;

  do {
    reverse[used++] = '0' + n % 10;
    n /= 10;
  } while(n);
  while(used)
    *out++ = reverse[--used];
  return out;
}

static int
draw(const struct page *page, const char *topic, uint section, int top, int clear)
{
  char frame[TTY_MAX_ROWS * (TTY_MAX_COLS + 8) + 80];
  char *out = append(frame, clear ? "\f" : "\033[H");
  uint offset;
  int i, end = top + page->height;
  char *status;

  if(end > page->count)
    end = page->count;
  for(i = top; i < top + page->height; i++){
    if(i < end){
      offset = page->rows[i];
      out += row(page, &offset, out);
    }
    out = append(out, "\033[K\r\n");
  }
  status = out;
  for(i = 0; i < 32 && topic[i]; i++)
    *out++ = topic[i];
  if(section){
    *out++ = '(';
    *out++ = '0' + section;
    *out++ = ')';
  }
  out = append(out, "  lines ");
  out = number(out, end ? top + 1 : 0);
  *out++ = '-';
  out = number(out, end);
  *out++ = '/';
  if(page->eof)
    out = number(out, page->count);
  else
    *out++ = '?';
  if(page->clipped)
    out = append(out, "  buffer full");
  out = append(out, "  arrows scroll  q quit");
  if(out - status > page->width)
    out = status + page->width;
  out = append(out, "\033[K");
  return writeall(frame, out - frame);
}

static int
readkey(int input)
{
  uchar c;
  int arg = 0, extra = 0;

  if(read(input, &c, 1) != 1)
    return -1;
  if(c != 0x1b)
    return c;
  if(read(input, &c, 1) != 1)
    return -1;
  if(c != '[' && c != 'O')
    return c;
  do {
    if(read(input, &c, 1) != 1)
      return -1;
    if(c == ';')
      extra = 1;
    if(!extra && c >= '0' && c <= '9' && arg < 1000)
      arg = 10 * arg + c - '0';
  } while(c >= ' ' && c < '@');
  switch(c){
  case 'A': return KEY_UP;
  case 'B': return KEY_DOWN;
  case 'H': return KEY_FIRST;
  case 'F': return KEY_LAST;
  case '~':
    switch(arg){
    case 1: case 7: return KEY_FIRST;
    case 4: case 8: return KEY_LAST;
    case 5: return KEY_PREV;
    case 6: return KEY_NEXT;
    }
    break;
  }
  return 0;
}

static int
browse(struct page *page, const char *topic, uint section, int input)
{
  int top = 0, last, before;
  int next, key, clear = 1, redraw = 1;
  for(;;){
    if(resumed){
      resumed = 0;
      redraw = clear = 1;
    }
    if(redraw && draw(page, topic, section, top, clear) < 0){
      if(resumed)
        continue;
      return -1;
    }
    if(resumed)
      continue;
    clear = redraw = 0;
    key = readkey(input);
    if(key < 0){
      if(resumed)
        continue;
      return -1;
    }
    next = top;
    switch(key){
    case 'q': case 'Q': case 4:
      return writeall("\r\033[K", 4);
    case 'j': case '\r': case '\n': case KEY_DOWN:
      next++;
      break;
    case 'k': case KEY_UP:
      next--;
      break;
    case ' ': case 'f': case KEY_NEXT:
      next += page->height;
      break;
    case 'b': case KEY_PREV:
      next -= page->height;
      break;
    case 'g': case KEY_FIRST:
      next = 0;
      break;
    case 'G': case KEY_LAST:
      next = PAGE_LIMIT;
      break;
    case 12:
      redraw = clear = 1;
      break;
    }
    if(next < 0)
      next = 0;
    before = page->count;
    if(next > top && extend(page, next + page->height) < 0){
      if(resumed)
        continue;
      return -1;
    }
    last = page->count - page->height;
    if(last < 0)
      last = 0;
    redraw |= before != page->count;
    if(next > last)
      next = last;
    redraw |= next != top;
    top = next;
  }
}

int
pageview(int fd, const char *topic, uint section)
{
  struct page page = { .width = TTY_COLS - 1, .height = TTY_ROWS - 1, .fd = fd };
  struct winsize size;
  struct stat info;
  int result, input = 0;

  if(ttyctl(1, TTY_ISATTY) < 0)
    return stream(fd);
  if(ttyctl(0, TTY_ISATTY) < 0){
    int number;
    char path[] = "/dev/ttyp0";
    const char *terminal = "/dev/console";
    if(ioctl(1, TIOCGPTY, &number) == 0){
      if(number < 0 || number >= 2 * P9_NPTY || (number & 1))
        return stream(fd);
      path[9] += number / 2;
      terminal = path;
    }
    if(section || (input = open(terminal, O_RDONLY)) < 0)
      return stream(fd);
  }
  if(ttyctl(input, TTY_CBREAK) < 0){
    if(input)
      close(input);
    return stream(fd);
  }

  if(ioctl(1, TIOCGWINSZ, &size) == 0){
    if(size.ws_col > 1)
      page.width = size.ws_col > TTY_MAX_COLS ? TTY_MAX_COLS - 1 : size.ws_col - 1;
    if(size.ws_row > 1)
      page.height = size.ws_row > TTY_MAX_ROWS ? TTY_MAX_ROWS - 1 : size.ws_row - 1;
  }
  signal(SIGCONT, oncontinue);
  result = extend(&page, fstat(fd, &info) == 0 && info.type == T_FILE ?
                         PAGE_LIMIT + 1 : page.height);
  if(result == 0)
    result = browse(&page, topic, section, input);
  ttyctl(input, TTY_CANON);
  if(input)
    close(input);
  if(page.text)
    free(page.text);
  if(page.rows)
    free(page.rows);
  return result;
}
