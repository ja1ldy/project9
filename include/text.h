/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_TEXT_H
#define PROJECT9_TEXT_H

#define TEXT_LINE 4096
struct text_input {
  int fd, next, count;
  char buf[512];
};

// return one for a line and zero at eof
// return minus one for an input error or an overlong line
int textline(struct text_input*, char*, int);
#endif
