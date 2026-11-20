/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_CURSES_H
#define P9_CURSES_H

typedef unsigned int chtype;
typedef struct p9_window WINDOW;
#define OK 0
#define ERR (-1)
#define KEY_RESIZE 0x19a
#define A_NORMAL 0U
#define A_REVERSE 0x100U
#define A_STANDOUT A_REVERSE

extern WINDOW *stdscr;
extern int LINES, COLS;
WINDOW *initscr(void);
int endwin(void);
WINDOW *newwin(int, int, int, int);
int delwin(WINDOW *);
int resizeterm(int, int);
int wmove(WINDOW *, int, int);
int waddch(WINDOW *, chtype);
int waddnstr(WINDOW *, const char *, int);
int wclrtoeol(WINDOW *);
int wclrtobot(WINDOW *);
int werase(WINDOW *);
int wclear(WINDOW *);
int wrefresh(WINDOW *);
int wstandout(WINDOW *);
int wstandend(WINDOW *);
int wgetnstr(WINDOW *, char *, int);
int wgetch(WINDOW *);
int addstr(const char *);
int printw(const char *, ...);
int standout(void);
int standend(void);
int curs_y(const WINDOW *);
int curs_x(const WINDOW *);

#define getyx(win,y,x) ((y) = curs_y(win), (x) = curs_x(win))
#define move(y,x) wmove(stdscr,(y),(x))
#define addch(c) waddch(stdscr,(c))
#define addnstr(s,n) waddnstr(stdscr,(s),(n))
#define clrtoeol() wclrtoeol(stdscr)
#define clrtobot() wclrtobot(stdscr)
#define erase() werase(stdscr)
#define clear() wclear(stdscr)
#define refresh() wrefresh(stdscr)
#define getnstr(s,n) wgetnstr(stdscr,(s),(n))
#define getch() wgetch(stdscr)

#endif
