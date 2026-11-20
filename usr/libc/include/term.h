/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef P9_TERM_H
#define P9_TERM_H
int tgetent(char *, const char *);
int tgetflag(const char *);
int tgetnum(const char *);
char *tgetstr(const char *, char **);
#endif
