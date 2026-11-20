/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "sh.h"
#include "user.h"
#include "stat.h"
#include "account.h"

struct variable {
  struct variable *next;
  char *name, *value;
  int exported;
};

static struct variable *vars;

static int
namechar(int c, int first)
{
  return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (!first && c >= '0' && c <= '9');
}

static int
namelen(const char *s)
{
  int n = 0;

  while(namechar(s[n], n == 0))
    n++;
  return n;
}

int
assignment(const char *s)
{
  int n = namelen(s);
  return n && s[n] == '=' ? n : 0;
}

static struct variable **
varslot(const char *name)
{
  struct variable **slot;

  for(slot = &vars; *slot; slot = &(*slot)->next)
    if(!strcmp((*slot)->name, name))
      break;
  return slot;
}

static char *
copy(const char *s, int n)
{
  char *out = malloc(n + 1);

  if(out){
    memmove(out, s, n);
    out[n] = 0;
  }
  return out;
}

const char *
lookvar(const char *name)
{
  struct variable *v = *varslot(name);
  return v ? v->value : 0;
}

// a plain assignment keeps the export flag a variable already has
int
setvar(const char *name, const char *value, int exported)
{
  int n = namelen(name);
  struct variable **slot, *v;
  char *data;

  if(!n || name[n])
    return -1;
  if(!(data = copy(value, strlen(value))))
    return -1;
  slot = varslot(name);
  if(!(v = *slot)){
    v = malloc(sizeof(*v));
    if(!v || !(v->name = copy(name, n))){
      if(v)
        free(v);
      free(data);
      return -1;
    }
    v->next = 0;
    v->exported = 0;
    *slot = v;
  } else {
    free(v->value);
  }
  v->value = data;
  v->exported |= exported;
  return 0;
}

int
unsetvar(const char *name)
{
  int n = namelen(name);
  struct variable **slot, *v;

  if(!n || name[n])
    return -1;
  slot = varslot(name);
  if((v = *slot)){
    *slot = v->next;
    free(v->name);
    free(v->value);
    free(v);
  }
  return 0;
}

int
assignvar(const char *raw, int exported)
{
  char *name, *value;
  int n = assignment(raw), status;

  if(!n || !(name = copy(raw, n)))
    return -1;
  value = expand_word(raw + n + 1);
  status = value ? setvar(name, value, exported) : -1;
  if(value)
    free(value);
  free(name);
  return status;
}

void
showvars(int exported)
{
  struct variable *v;

  for(v = vars; v; v = v->next)
    if(!exported || v->exported)
      printf(1, "%s=%s\n", v->name, v->value);
}

static int
defaultvar(const char *name, const char *value)
{
  return lookvar(name) ? 0 : setvar(name, value, 1);
}

int
initvars(void)
{
  struct account_info info;
  char **p, *name, pwd[512];
  int n, ok;

  if(environ)
    for(p = environ; *p; p++){
      n = assignment(*p);
      if(!n)
        continue;
      if(!(name = copy(*p, n)))
        return -1;
      ok = setvar(name, *p + n + 1, 1);
      free(name);
      if(ok < 0)
        return -1;
    }
  if(accountinfo(getuid(), &info) < 0){
    strcpy(info.name, "user");
    strcpy(info.home, "/");
  }
  if(getcwd(pwd, sizeof(pwd)) < 0)
    strcpy(pwd, "/");
  if(defaultvar("HOME", info.home) < 0 || defaultvar("USER", info.name) < 0 ||
     defaultvar("PATH", "/bin:/usr/bin:/usr/X11/bin") < 0 || defaultvar("TERM", "project9") < 0 ||
     setvar("PWD", pwd, 1) < 0)
    return -1;
  return 0;
}

void
freeenv(char **env, int n)
{
  int i;

  for(i = 0; i < n; i++)
    free(env[i]);
}

int
makeenv(char **env)
{
  struct variable *v;
  int n = 0, len;

  for(v = vars; v; v = v->next){
    if(!v->exported)
      continue;
    len = strlen(v->name);
    if(n == MAXENV || !(env[n] = malloc(len + strlen(v->value) + 2))){
      freeenv(env, n);
      return -1;
    }
    memmove(env[n], v->name, len);
    env[n][len] = '=';
    strcpy(env[n++] + len + 1, v->value);
  }
  env[n] = 0;
  return n;
}

struct expansion {
  char text[SH_WORD + 1];
  uchar split[SH_WORD];
  int used, quoted;
};

static int
emit(struct expansion *e, char c, int split)
{
  if(e->used == SH_WORD)
    return -1;
  e->split[e->used] = split && (c == ' ' || c == '\t' || c == '\n');
  e->text[e->used++] = c;
  return 0;
}

static int
emitstr(struct expansion *e, const char *p, int split)
{
  for(; *p; p++)
    if(emit(e, *p, split) < 0)
      return -1;
  return 0;
}

static const char *
decimal(uint n, char text[16])
{
  char *p = text + 15;

  *p = 0;
  do {
    *--p = '0' + n % 10;
  } while((n /= 10) != 0);
  return p;
}

static int
dollar(struct expansion *e, const char **cursor, int split)
{
  const char *p = *cursor, *start, *value = 0;
  char number[16], *name;
  int n, i, result = 0;

  switch(*p){
  case '?': value = decimal(sh_status, number); p++; break;
  case '$': value = decimal(getpid(), number); p++; break;
  case '#': value = decimal(sh_poscount ? sh_poscount - 1 : 0, number); p++; break;
  case '*':
  case '@':
    for(i = 1; i < sh_poscount; i++)
      if((i > 1 && emit(e, ' ', split) < 0) || emitstr(e, sh_positionals[i], split) < 0)
        return -1;
    p++;
    break;
  case '{':
    start = ++p;
    n = namelen(p);
    if(!n || p[n] != '}')
      return -1;
    p += n + 1;
    if(!(name = copy(start, n)))
      return -1;
    if((value = lookvar(name)))
      result = emitstr(e, value, split);
    free(name);
    value = 0;
    break;
  default:
    if(*p >= '0' && *p <= '9'){
      i = *p++ - '0';
      if(i < sh_poscount)
        value = sh_positionals[i];
    } else if((n = namelen(p)) != 0){
      if(!(name = copy(p, n)))
        return -1;
      if((value = lookvar(name)))
        result = emitstr(e, value, split);
      free(name);
      value = 0;
      p += n;
    } else {
      result = emit(e, '$', 0);
    }
  }
  *cursor = p;
  return value ? emitstr(e, value, split) : result;
}

static struct expansion *
expand(const char *raw)
{
  struct expansion *e = malloc(sizeof(*e));
  const char *p = raw;
  int q = 0, c;

  if(!e)
    return 0;
  e->used = e->quoted = 0;
  while((c = *p++) != 0){
    if(c == q){
      q = 0;
    } else if(!q && (c == '\'' || c == '"')){
      q = c;
      e->quoted = 1;
    } else if(c == '\\' && q != '\'' &&
              (!q || (*p && strchr("\"\\$\n", *p)))){
      if(!*p)
        goto bad;
      if(*p != '\n' && emit(e, *p, 0) < 0)
        goto bad;
      p++;
    } else if(c == '$' && q != '\''){
      if(dollar(e, &p, q == 0) < 0)
        goto bad;
    } else if(emit(e, c, 0) < 0){
      goto bad;
    }
  }
  if(q)
    goto bad;
  e->text[e->used] = 0;
  return e;

bad:
  free(e);
  return 0;
}

char *
expand_word(const char *raw)
{
  struct expansion *e = expand(raw);
  char *text;

  if(!e)
    return 0;
  text = copy(e->text, e->used);
  free(e);
  return text;
}

void
freeargs(char **words, int count)
{
  freeenv(words, count);
}

int
expand_args(char *raw, char **words, int slots)
{
  struct expansion *e;
  int i = 0, count = 0, start;

  if(!strcmp(raw, "\"$@\"") || !strcmp(raw, "$@")){
    for(i = 1; i < sh_poscount; i++){
      if(count == slots || !(words[count] = copy(sh_positionals[i], strlen(sh_positionals[i]))))
        goto error;
      count++;
    }
    return count;
  }
  if(!(e = expand(raw)))
    return -1;
  while(i < e->used){
    while(i < e->used && e->split[i])
      i++;
    start = i;
    while(i < e->used && !e->split[i])
      i++;
    if(i > start){
      if(count == slots || !(words[count] = copy(e->text + start, i - start)))
        goto release;
      count++;
    }
  }
  if(!count && e->quoted){
    if(!slots || !(words[0] = copy("", 0)))
      goto release;
    count = 1;
  }
  free(e);
  return count;

release:
  free(e);
error:
  freeargs(words, count);
  return -1;
}

