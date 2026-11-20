/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <curses.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <term.h>
#include <termios.h>
#include <unistd.h>

struct p9_window {
  int rows, cols, top, left, y, x;
  chtype attr;
  chtype *cells, *shown;
};

WINDOW *stdscr;
int LINES, COLS;
static struct termios saved;
static int restore, known_term;

WINDOW *
newwin(int rows, int cols, int top, int left)
{
  if(top < 0 || left < 0 || top >= LINES || left >= COLS || rows < 0 || cols < 0)
    return NULL;
  if(!rows)
    rows = LINES - top;
  if(!cols)
    cols = COLS - left;
  if(rows > LINES - top || cols > COLS - left)
    return NULL;
  size_t count = (size_t)rows * cols;
  WINDOW *w = calloc(1, sizeof(*w) + count * sizeof(chtype) * 2);
  if(!w)
    return NULL;
  w->rows = rows;
  w->cols = cols;
  w->top = top;
  w->left = left;
  w->cells = (chtype *)(w + 1);
  w->shown = w->cells + count;
  werase(w);
  return w;
}

int
delwin(WINDOW *w)
{
  if(!w)
    return ERR;
  if(w == stdscr)
    stdscr = NULL;
  free(w);
  return OK;
}

static void
screensize(int *rows, int *cols)
{
  struct winsize size = {0};
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
  *rows = size.ws_row ? size.ws_row : 24;
  *cols = size.ws_col ? size.ws_col : 80;
}

int
resizeterm(int rows, int cols)
{
  if(rows <= 0 || cols <= 0 || rows > 256 || cols > 512)
    return ERR;

  int old_rows = LINES, old_cols = COLS;
  LINES = rows;
  COLS = cols;
  WINDOW *next = newwin(rows, cols, 0, 0);
  if(!next){
    LINES = old_rows;
    COLS = old_cols;
    return ERR;
  }

  if(stdscr){
    int copy_rows = rows < old_rows ? rows : old_rows;
    int copy_cols = cols < old_cols ? cols : old_cols;
    for(int y = 0; y < copy_rows; y++)
      memcpy(next->cells + y * cols, stdscr->cells + y * old_cols,
             copy_cols * sizeof(chtype));
    delwin(stdscr);
  }
  stdscr = next;
  return OK;
}

WINDOW *
initscr(void)
{
  if(stdscr)
    return stdscr;

  int rows, cols;
  screensize(&rows, &cols);
  if(resizeterm(rows, cols) == ERR)
    return NULL;
  if(tcgetattr(STDIN_FILENO, &saved) == 0){
    struct termios cbreak = saved;
    cbreak.c_lflag &= ~(ICANON | ECHO);
    cbreak.c_cc[VMIN] = 1;
    cbreak.c_cc[VTIME] = 0;
    restore = tcsetattr(STDIN_FILENO, TCSANOW, &cbreak) == 0;
  }
  return stdscr;
}

int
endwin(void)
{
  if(!stdscr)
    return ERR;
  delwin(stdscr);
  fputs("\033[0m", stdout);
  fflush(stdout);
  if(restore){
    restore = 0;
    return tcsetattr(STDIN_FILENO, TCSANOW, &saved) == 0 ? OK : ERR;
  }
  return OK;
}

int
wmove(WINDOW *w, int y, int x)
{
  if(!w || y < 0 || x < 0 || y >= w->rows || x >= w->cols)
    return ERR;
  w->y = y;
  w->x = x;
  return OK;
}

int
curs_y(const WINDOW *w)
{
  return w ? w->y : ERR;
}

int
curs_x(const WINDOW *w)
{
  return w ? w->x : ERR;
}

int
wclrtoeol(WINDOW *w)
{
  if(!w)
    return ERR;
  for(int x = w->x; x < w->cols; x++)
    w->cells[w->y * w->cols + x] = ' ';
  return OK;
}

int
wclrtobot(WINDOW *w)
{
  if(!w)
    return ERR;
  for(int i = w->y * w->cols + w->x; i < w->rows * w->cols; i++)
    w->cells[i] = ' ';
  return OK;
}

int
werase(WINDOW *w)
{
  if(wmove(w, 0, 0) == ERR)
    return ERR;
  return wclrtobot(w);
}

int
wclear(WINDOW *w)
{
  if(werase(w) == ERR)
    return ERR;
  memset(w->shown, 0, (size_t)w->rows * w->cols * sizeof(chtype));
  return OK;
}

static int
advance_line(WINDOW *w)
{
  if(w->y == w->rows - 1)
    return ERR;
  w->x = 0;
  w->y++;
  return OK;
}

int
waddch(WINDOW *w, chtype ch)
{
  if(!w)
    return ERR;
  unsigned char c = ch & 255;
  if(c == '\r'){
    w->x = 0;
    return OK;
  }
  if(c == '\b'){
    if(w->x > 0)
      w->x--;
    return OK;
  }
  if(c == '\n'){
    wclrtoeol(w);
    return advance_line(w);
  }
  if(c == '\t'){
    int spaces = 8 - w->x % 8;
    while(spaces--)
      if(waddch(w, ' ') == ERR)
        return ERR;
    return OK;
  }
  if(c < 32 || c == 127)
    return ERR;
  w->cells[w->y * w->cols + w->x] = c | ((w->attr | ch) & A_REVERSE);
  if(w->x < w->cols - 1){
    w->x++;
    return OK;
  }
  return advance_line(w);
}

int
waddnstr(WINDOW *w, const char *s, int n)
{
  if(!w || !s)
    return ERR;
  for(int i = 0; s[i] && (n < 0 || i < n); ++i)
    if(waddch(w, (unsigned char)s[i]) == ERR)
      return ERR;
  return OK;
}

int
wstandout(WINDOW *w)
{
  if(!w)
    return ERR;
  w->attr = A_REVERSE;
  return OK;
}

int
wstandend(WINDOW *w)
{
  if(!w)
    return ERR;
  w->attr = A_NORMAL;
  return OK;
}

int
wrefresh(WINDOW *w)
{
  if(!w)
    return ERR;
  chtype attr = A_NORMAL;
  for(int y = 0; y < w->rows; ++y){
    int x = 0;
    while(x < w->cols){
      int i = y * w->cols + x;
      if(w->cells[i] == w->shown[i]){
        ++x;
        continue;
      }
      printf("\033[%d;%dH", w->top + y + 1, w->left + x + 1);
      do {
        chtype value = w->cells[i];
        if((value & A_REVERSE) != attr){
          attr = value & A_REVERSE;
          fputs(attr ? "\033[7m" : "\033[0m", stdout);
        }
        if(putchar(value & 255) == EOF)
          return ERR;
        w->shown[i] = value;
        ++i;
        ++x;
      } while(x < w->cols && w->cells[i] != w->shown[i]);
    }
  }
  if(attr)
    fputs("\033[0m", stdout);
  printf("\033[%d;%dH", w->top + w->y + 1, w->left + w->x + 1);
  return fflush(stdout) == 0 ? OK : ERR;
}

int
wgetch(WINDOW *w)
{
  unsigned char c;
  if(wrefresh(w) == ERR || read(STDIN_FILENO, &c, 1) != 1)
    return ERR;
  return c;
}

int
wgetnstr(WINDOW *w, char *s, int max)
{
  if(!w || !s || max < 0)
    return ERR;
  for(int n = 0;;){
    int c = wgetch(w);
    if(c == ERR || c == '\n' || c == '\r'){
      s[n] = 0;
      return c == ERR ? ERR : OK;
    }
    if(c == 8 || c == 127){
      if(n > 0){
        n--;
        waddch(w, '\b');
        waddch(w, ' ');
        waddch(w, '\b');
      }
    } else if(c >= 32 && n < max){
      s[n++] = c;
      waddch(w, c);
    }
  }
}

int
tgetent(char *buffer, const char *name)
{
  if(buffer)
    *buffer = 0;
  known_term = name && (!strcmp(name, "xterm") || !strcmp(name, "vt100") ||
                         !strcmp(name, "ansi") || !strcmp(name, "xterm-256color"));
  return known_term;
}

int
tgetflag(const char *name)
{
  return known_term && !strcmp(name, "am");
}

int
tgetnum(const char *name)
{
  int rows, cols;
  screensize(&rows, &cols);
  if(!known_term)
    return -1;
  return !strcmp(name, "li") ? rows : !strcmp(name, "co") ? cols : -1;
}

char *
tgetstr(const char *name, char **area)
{
  static char cl[] = "\033[H\033[2J", cm[] = "\033[%i%d;%dH";
  char *value = NULL;
  if(known_term)
    value = !strcmp(name, "cl") ? cl : !strcmp(name, "cm") ? cm : NULL;
  if(value && area && *area){
    char *result = *area;
    size_t len = strlen(value) + 1;
    memcpy(result, value, len);
    *area += len;
    return result;
  }
  return value;
}


int
addstr(const char *text)
{
  return waddnstr(stdscr, text, -1);
}

int
printw(const char *format, ...)
{
  char buffer[256];
  va_list ap;
  va_start(ap, format);
  int count = vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);
  if(count < 0)
    return ERR;
  if((size_t)count < sizeof(buffer))
    return addstr(buffer);
  char *line = malloc((size_t)count + 1);
  if(!line)
    return ERR;
  va_start(ap, format);
  vsnprintf(line, (size_t)count + 1, format, ap);
  va_end(ap);
  int result = addstr(line);
  free(line);
  return result;
}

int
standout(void)
{
  return wstandout(stdscr);
}

int
standend(void)
{
  return wstandend(stdscr);
}
