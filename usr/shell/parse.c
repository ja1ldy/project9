/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "sh.h"
#include "user.h"
#include "setjmp.h"

enum token {
  END, WORD, NEWLINE, SEMI, AMP, BAR, LOGAND, LOGOR, LEFT, RIGHT, REDIR
};

enum stop {
  STOP_RIGHT = 1, STOP_THEN = 2, STOP_IF = 4, STOP_FI = 8,
  STOP_DO = 16, STOP_DONE = 32
};

struct piece {
  struct piece *next;
};

static struct piece *arena;
static const char *source, *error;
static enum token token;
static int fd, operation;
static const char *word_start;
static uint word_len;
static jmp_buf recover;

static void
fail(const char *message, int incomplete)
{
  error = message;
  longjmp(recover, incomplete ? 2 : 1);
}

static void *
allocate(uint bytes)
{
  struct piece *p = malloc(sizeof(*p) + bytes);

  if(!p)
    fail("out of memory while parsing", 0);
  p->next = arena;
  arena = p;
  memset(p + 1, 0, bytes);
  return p + 1;
}

void
discard_parse(void)
{
  struct piece *p;

  while((p = arena) != 0){
    arena = p->next;
    free(p);
  }
}

static void
next(void)
{
  const char *p = source;
  int quote = 0;
  char c;

  while(*p == ' ' || *p == '\t' || *p == '\r')
    p++;
  if(*p == '#')
    while(*p && *p != '\n')
      p++;
  fd = -1;
  c = *p;
  if(c >= '0' && c <= '9' && (p[1] == '>' || p[1] == '<')){
    fd = c - '0';
    c = *++p;
  }
  if(!c){
    token = END;
  } else if(c == '<' || c == '>'){
    token = REDIR;
    operation = c == '<' ? READ_FILE : WRITE_FILE;
    if(fd == -1)
      fd = c == '<' ? 0 : 1;
    if(*++p == '&'){
      operation = DUP_FD;
      p++;
    } else if(c == '>' && *p == '>'){
      operation = APPEND_FILE;
      p++;
    }
  } else if(c == '&' || c == '|'){
    token = c == '&' ? AMP : BAR;
    if(*++p == c){
      token = c == '&' ? LOGAND : LOGOR;
      p++;
    }
  } else if(c == '\n' || c == ';' || c == '(' || c == ')'){
    token = c == '\n' ? NEWLINE : c == ';' ? SEMI : c == '(' ? LEFT : RIGHT;
    p++;
  } else {
    token = WORD;
    word_start = p;
    for(; (c = *p) != 0; p++){
      if(c == '\\' && quote != '\''){
        if(!p[1])
          fail("unfinished escape", 1);
        p++;
        if(*p == '\n' && !p[1])
          fail("unfinished line continuation", 1);
        continue;
      }
      if(quote){
        if(c == quote)
          quote = 0;
      } else if(c == '\'' || c == '"'){
        quote = c;
      } else if(strchr(" \t\r\n;&|<>()", c)){
        break;
      }
    }
    if(quote)
      fail("unclosed quote", 1);
    word_len = p - word_start;
  }
  source = p;
}

static int
isword(const char *name)
{
  uint i;

  if(token != WORD || word_len != strlen(name))
    return 0;
  for(i = 0; i < word_len; i++)
    if(word_start[i] != name[i])
      return 0;
  return 1;
}

static char *
takeword(const char *problem)
{
  char *s;

  if(token != WORD)
    fail(problem, token == END);
  s = allocate(word_len + 1);
  memmove(s, word_start, word_len);
  next();
  return s;
}

static struct command *
node(enum node type)
{
  struct command *c = allocate(sizeof(*c));

  c->type = type;
  return c;
}

static void
newlines(void)
{
  while(token == NEWLINE)
    next();
}

static void
expect(const char *name)
{
  if(!isword(name))
    fail(name, token == END);
  next();
}

static int
atstop(int stop)
{
  return token == END ||
         ((stop & STOP_RIGHT) && token == RIGHT) ||
         ((stop & STOP_THEN) && isword("then")) ||
         ((stop & STOP_IF) && (isword("else") || isword("elif") || isword("fi"))) ||
         ((stop & STOP_FI) && isword("fi")) ||
         ((stop & STOP_DO) && isword("do")) ||
         ((stop & STOP_DONE) && isword("done"));
}

static int
reserved(void)
{
  return isword("then") || isword("else") || isword("elif") || isword("fi") ||
         isword("do") || isword("done");
}

static struct command *list(int, int);

static struct command *
conditional(int depth)
{
  struct command *first, *branch;
  int branches = 0;

  first = branch = node(IF);
  for(;;){
    if(++branches > 16)
      fail("too many elif branches", 0);
    branch->a = list(STOP_THEN, depth);
    expect("then");
    if(!branch->a)
      fail("if requires a condition", 0);
    branch->b = list(STOP_IF, depth);
    if(!isword("elif"))
      break;
    next();
    branch->c = node(IF);
    branch = branch->c;
  }
  if(isword("else")){
    next();
    branch->c = list(STOP_FI, depth);
  }
  expect("fi");
  return first;
}

static struct word **
appendword(struct word **tail)
{
  struct word *w = allocate(sizeof(*w));

  w->raw = takeword("expected a word");
  *tail = w;
  return &w->next;
}

static struct command *
counted_loop(int depth)
{
  struct command *c = node(FOR);
  struct word **tail = &c->words;
  const char *name;
  int i;

  c->name = takeword("expected a for variable");
  name = c->name;
  for(i = 0; name[i]; i++)
    if(!(name[i] == '_' || (name[i] >= 'a' && name[i] <= 'z') ||
         (name[i] >= 'A' && name[i] <= 'Z') ||
         (i && name[i] >= '0' && name[i] <= '9')))
      fail("invalid for variable", 0);
  if(isword("in")){
    next();
    while(token == WORD)
      tail = appendword(tail);
  } else {
    struct word *w = allocate(sizeof(*w));
    w->raw = "\"$@\"";
    c->words = w;
  }
  if(token != SEMI && token != NEWLINE)
    fail("expected a separator before do", token == END);
  next();
  newlines();
  expect("do");
  c->a = list(STOP_DONE, depth);
  expect("done");
  return c;
}

static struct redirect **
addredir(struct redirect **tail)
{
  struct redirect *r;

  if(fd > 2)
    fail("redirections support descriptors 0 through 2", 0);
  r = allocate(sizeof(*r));
  r->fd = fd;
  r->kind = operation;
  next();
  r->word = takeword("expected a redirection target");
  *tail = r;
  return &r->next;
}

static struct command *
compound_redirs(struct command *c)
{
  struct redirect **tail = &c->redirs;

  while(token == REDIR)
    tail = addredir(tail);
  return c;
}

static struct command *
command(int depth)
{
  struct command *c;
  struct word **words;
  struct redirect **redirs;

  if(depth > 12)
    fail("command nesting is too deep", 0);
  if(isword("if")){
    next();
    return compound_redirs(conditional(depth+1));
  }
  if(isword("while") || isword("until")){
    c = node(isword("while") ? WHILE : UNTIL);
    next();
    c->a = list(STOP_DO, depth+1);
    expect("do");
    if(!c->a)
      fail("loop requires a condition", 0);
    c->b = list(STOP_DONE, depth+1);
    expect("done");
    return compound_redirs(c);
  }
  if(isword("for")){
    next();
    return compound_redirs(counted_loop(depth+1));
  }
  if(reserved())
    fail("unexpected shell keyword", 0);
  if(token == LEFT){
    next();
    c = node(GROUP);
    c->a = list(STOP_RIGHT, depth+1);
    if(token != RIGHT)
      fail("expected a closing parenthesis", token == END);
    next();
    return compound_redirs(c);
  }
  if(token != REDIR && token != WORD)
    fail("expected a command", token == END);
  c = node(SIMPLE);
  words = &c->words;
  redirs = &c->redirs;
  while(token == REDIR || token == WORD){
    if(token == REDIR)
      redirs = addredir(redirs);
    else
      words = appendword(words);
  }
  return c;
}

static struct command *
pipeline(int depth)
{
  struct command *left = command(depth), *pipe;
  int count = 1;

  while(token == BAR){
    if(++count > 16)
      fail("pipeline is too long", 0);
    next();
    newlines();
    pipe = node(PIPE);
    pipe->a = left;
    pipe->b = command(depth);
    left = pipe;
  }
  return left;
}

static struct command *
andor(int depth)
{
  struct command *left = pipeline(depth), *test;
  int count = 1;

  while(token == LOGAND || token == LOGOR){
    if(++count > 16)
      fail("conditional list is too long", 0);
    test = node(token == LOGAND ? AND : OR);
    next();
    newlines();
    test->a = left;
    test->b = pipeline(depth);
    left = test;
  }
  return left;
}

static struct command *
list(int stop, int depth)
{
  struct command *head = 0, **tail = &head, *c, *background;

  newlines();
  while(!atstop(stop)){
    c = andor(depth);
    if(token == AMP){
      background = node(BACK);
      background->a = c;
      c = background;
      next();
    } else if(token == SEMI || token == NEWLINE){
      next();
    } else if(!atstop(stop)) {
      fail("expected a command separator", 0);
    }
    *tail = c;
    tail = &c->next;
    newlines();
  }
  return head;
}

// return 2 when another interactive line could complete the command
int
parse(const char *text, struct command **result, const char **message)
{
  int failed = setjmp(recover);

  if(failed){
    *message = error;
    discard_parse();
    return failed;
  }
  source = text;
  next();
  *result = list(0, 0);
  return 0;
}
