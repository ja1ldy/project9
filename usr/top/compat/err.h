/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef TOP_ERR_H
#define TOP_ERR_H
void warnx(const char *, ...);
void err(int, const char *, ...) __attribute__((noreturn));
#endif
