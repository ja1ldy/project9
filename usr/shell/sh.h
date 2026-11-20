/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_SH_H
#define PROJECT9_SH_H

#include "types.h"
#include "param.h"

#define SH_ARGV MAXARG
#define SH_WORD 2047
#define SH_INPUT 4096

enum node { SIMPLE, GROUP, PIPE, AND, OR, BACK, IF, WHILE, UNTIL, FOR };
enum redir_kind { READ_FILE, WRITE_FILE, APPEND_FILE, DUP_FD };

struct word {
  char *raw;
  struct word *next;
};

struct redirect {
  int fd, kind;
  char *word;
  struct redirect *next;
};

struct command {
  enum node type;
  struct command *next;
  struct command *a, *b, *c;
  struct word *words;
  struct redirect *redirs;
  char *name;
};

extern int sh_interactive;
extern volatile int sh_interrupted;
extern int sh_status;
extern char **sh_positionals;
extern int sh_poscount;

int parse(const char *, struct command **, const char **);
void discard_parse(void);

int assignment(const char *);
int setvar(const char *, const char *, int);
int assignvar(const char *, int);
int unsetvar(const char *);
const char *lookvar(const char *);
void showvars(int);
int initvars(void);
int makeenv(char **);
void freeenv(char **, int);
char *expand_word(const char *);
int expand_args(char *, char **, int);
void freeargs(char **, int);

int evaluate(struct command *);
void report_jobs(int);
void sh_prompt(void);
void sh_chdir(void);

#endif
