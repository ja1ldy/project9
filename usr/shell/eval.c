/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "sh.h"
#include "user.h"
#include "mode.h"
#include "fcntl.h"
#include "tty.h"
#include "wait.h"

enum { FLOW_NONE, FLOW_BREAK, FLOW_CONTINUE };
static int jobs[NPROC];
static int loopdepth, flow;
static int eval(struct command *);
static int worker(struct command *);
static int simple(struct command *, int);

static int
problem(const char *message)
{
  printf(2, "sh: %s\n", message);
  return 1;
}

static int
statuscode(int status)
{
  if(WIFEXITED(status))
    return WEXITSTATUS(status);
  if(WIFSIGNALED(status))
    return 128 + WTERMSIG(status);
  return 1;
}

static int
child_status(int pid)
{
  int status;

  if(waitpid(pid, &status, 0) != pid)
    return sh_interrupted ? 130 : 1;
  return statuscode(status);
}

void
report_jobs(int show)
{
  int i, state;

  for(i = 0; i < NPROC; i++){
    if(!jobs[i])
      continue;
    state = jobctlx(jobs[i], JOB_POLL, 0);
    if(state <= 0){
      if(show && state == 0)
        printf(1, "[%d] Done\n", jobs[i]);
      jobs[i] = 0;
    } else if(show){
      printf(1, "[%d] %s\n", jobs[i],
             state == JOB_STOPPED ? "Stopped" : "Running");
    }
  }
}

static int
job_command(const char *operation, char **argv, int argc)
{
  int i, slot = -1, mode, state, status;
  uint pid = 0;

  if(argc > 2 || (argc == 2 && (parseuint(argv[1], &pid) < 0 || !pid)))
    return problem("expected an optional job PID");
  for(i = 0; i < NPROC; i++)
    if(jobs[i] && (pid ? jobs[i] == pid : slot == -1 || jobs[i] > jobs[slot]))
      slot = i;
  if(slot < 0)
    return problem("no such job");

  mode = !strcmp(operation, "fg") ? JOB_RESUME : JOB_CONT_BACKGROUND;
  state = jobctlx(jobs[slot], mode, &status);
  if(state == JOB_STOPPED)
    printf(1, "[%d] Stopped\n", jobs[slot]);
  else if(state == JOB_RUNNING && mode == JOB_CONT_BACKGROUND)
    printf(1, "[%d] Running\n", jobs[slot]);
  else if(state <= 0)
    jobs[slot] = 0;
  if(state < 0)
    return problem("no such job");
  if(state == 0)
    return statuscode(status);
  return state == JOB_STOPPED ? 128 + SIGTSTP : 0;
}

static int
launch(struct command *c, int background)
{
  int slot = -1, pid, state, status, job = sh_interactive || background;

  if(job){
    report_jobs(0);
    for(slot = 0; slot < NPROC && jobs[slot]; slot++)
      ;
    if(slot == NPROC)
      return problem("job table full");
  }
  pid = job ? forkjob(background ? JOB_BACKGROUND : JOB_FOREGROUND) : fork();
  if(pid < 0)
    return problem("fork failed");
  if(pid == 0){
    sh_interactive = 0;
    sh_interrupted = 0;
    loopdepth = flow = 0;
    memset(jobs, 0, sizeof(jobs));
    exitstatus(worker(c));
  }
  if(!job)
    return child_status(pid);
  if(background){
    jobs[slot] = pid;
    if(sh_interactive)
      printf(1, "[%d]\n", pid);
    return 0;
  }
  state = jobctlx(pid, JOB_FOREGROUND, &status);
  if(state == JOB_STOPPED){
    jobs[slot] = pid;
    printf(1, "[%d] Stopped\n", pid);
    return 128 + SIGTSTP;
  }
  if(state == JOB_RUNNING)
    jobs[slot] = pid;
  return state == 0 ? statuscode(status) : sh_interrupted ? 130 : 1;
}

// keep saved descriptors above stderr even if a standard descriptor was closed
static int
savefd(int fd)
{
  int low[3], n = 0, saved;

  while((saved = dup(fd)) >= 0 && saved < 3)
    low[n++] = saved;
  while(n)
    close(low[--n]);
  return saved;
}

static void
restore(int saved[3])
{
  int fd;

  for(fd = 0; fd < 3; fd++){
    if(saved[fd] == -1)
      close(fd);
    else if(saved[fd] >= 0){
      dup2(saved[fd], fd);
      close(saved[fd]);
    }
  }
}

static int
redirect(struct redirect *r, int *saved)
{
  char *name;
  uint source;
  int fd, result, mode;

  for(; r; r = r->next){
    if(saved && saved[r->fd] == -2){
      if(dup2(r->fd, r->fd) < 0){
        saved[r->fd] = -1;
      } else if((fd = savefd(r->fd)) < 0){
        return problem("cannot save file descriptor");
      } else {
        saved[r->fd] = fd;
      }
    }
    if(!(name = expand_word(r->word)))
      return problem("redirection expansion failed");
    result = 0;
    if(r->kind == DUP_FD){
      if(!strcmp(name, "-")){
        close(r->fd);
      } else if(parseuint(name, &source) < 0 || source > 2 || dup2(source, r->fd) < 0){
        result = -1;
      }
    } else {
      mode = r->kind == READ_FILE ? O_RDONLY : O_WRONLY | O_CREATE |
             (r->kind == APPEND_FILE ? O_APPEND : O_TRUNC);
      fd = open(name, mode);
      if(fd < 0)
        result = -1;
      else if(fd != r->fd){
        result = dup2(fd, r->fd);
        close(fd);
      }
    }
    if(result < 0){
      printf(2, "sh: redirection failed for %s\n", name);
      free(name);
      return 1;
    }
    free(name);
  }
  return 0;
}

enum builtin {
  BI_NONE, BI_CD, BI_UMASK, BI_EXPORT, BI_UNSET, BI_SET, BI_EXIT,
  BI_JOBS, BI_FG, BI_BG, BI_BREAK, BI_CONTINUE, BI_COLON, BI_READ, BI_EXEC
};

static enum builtin
builtin(const char *name)
{
  static const char *const names[] = {
    "", "cd", "umask", "export", "unset", "set", "exit", "jobs", "fg", "bg",
    "break", "continue", ":", "read", "exec"
  };
  uint i;

  for(i = 1; i < sizeof(names) / sizeof(*names); i++)
    if(!strcmp(name, names[i]))
      return i;
  return BI_NONE;
}

static int
set_export(const char *arg)
{
  int n = assignment(arg), status;
  const char *value;
  char *name;

  if(!n){
    value = lookvar(arg);
    return setvar(arg, value ? value : "", 1);
  }
  if(!(name = malloc(n+1)))
    return -1;
  memmove(name, arg, n);
  name[n] = 0;
  status = setvar(name, arg+n+1, 1);
  free(name);
  return status;
}

static int
readvar(const char *name)
{
  char c, value[256];
  int n, used = 0, full = 0;

  while((n = read(0, &c, 1)) == 1 && c != '\n'){
    if(!c || used == sizeof value - 1)
      full = 1;
    else
      value[used++] = c;
  }
  value[used] = 0;
  if(full || n < 0)
    return 1;
  if(setvar(name, value, 0) < 0)
    return problem("invalid variable name");
  return n == 0;
}

static int
execute(char **argv)
{
  char *env[MAXENV+1];
  int status, envc = makeenv(env);

  if(envc < 0)
    return problem("environment is too large");
  status = execpath(argv, env, lookvar("PATH"));
  printf(2, "sh: %s: %s\n", argv[0],
         status == 126 ? "cannot execute" : "command not found");
  freeenv(env, envc);
  return status;
}

static int
builtin_run(enum builtin which, char **args, int argc)
{
  const char *target;
  char mask[5];
  uint n;
  int i, status = 0;

  switch(which){
  case BI_CD:
    if(argc > 2)
      return problem("usage: cd [directory]");
    target = argc == 2 ? args[1] : lookvar("HOME");
    if(chdir(target ? target : "/") < 0){
      printf(2, "sh: cannot cd to %s\n", target ? target : "/");
      return 1;
    }
    sh_chdir();
    break;
  case BI_UMASK:
    if(argc == 1){
      formatmode(umask(-1), mask);
      printf(1, "%s\n", mask);
    } else if(argc != 2 || parsemode(args[1], &n) < 0 || n > M_PERM || umask(n) < 0){
      return problem("usage: umask [octal-mask]");
    }
    break;
  case BI_EXPORT:
  case BI_UNSET:
    if(argc == 1 && which == BI_EXPORT)
      showvars(1);
    for(i = 1; i < argc; i++)
      if((which == BI_EXPORT ? set_export(args[i]) : unsetvar(args[i])) < 0)
        status = problem("invalid variable name or assignment");
    break;
  case BI_READ:
    if(argc != 2)
      return problem("usage: read name");
    return readvar(args[1]);
  case BI_EXEC:
    if(argc == 1)
      return 0;
    status = execute(args + 1);
    if(!sh_interactive)
      exitstatus(status);
    return status;
  case BI_SET:
    if(argc != 1)
      return problem("set takes no arguments");
    showvars(0);
    break;
  case BI_EXIT:
    n = sh_status;
    if(argc > 2 || (argc == 2 && parseuint(args[1], &n) < 0))
      return problem("usage: exit [status]");
    exitstatus(n & 255);
  case BI_JOBS:
    if(argc != 1)
      return problem("jobs takes no arguments");
    report_jobs(1);
    break;
  case BI_FG:
  case BI_BG:
    return job_command(args[0], args, argc);
  case BI_BREAK:
  case BI_CONTINUE:
    if(!loopdepth || argc != 1)
      return problem("break and continue require a loop and take no arguments");
    flow = which == BI_BREAK ? FLOW_BREAK : FLOW_CONTINUE;
    break;
  default:
    break;
  }
  return status;
}

static int
arguments(struct word *word, char **out, int count)
{
  int used = 0, n;

  for(; word; word = word->next){
    n = expand_args(word->raw, out + used, count - used);
    if(n < 0){
      freeargs(out, used);
      return -1;
    }
    used += n;
  }
  out[used] = 0;
  return used;
}

static int
simple(struct command *c, int directly)
{
  struct word *arg, *w;
  char *argv[SH_ARGV+1];
  int argc, status, saved[3] = {-2, -2, -2};
  enum builtin which;

  for(arg = c->words; arg && assignment(arg->raw); arg = arg->next)
    ;
  argc = arguments(arg, argv, SH_ARGV);
  if(argc < 0)
    return problem("could not expand arguments or too many arguments");
  which = argc ? builtin(argv[0]) : BI_NONE;
  if(argc && !directly && which != BI_EXEC && (!which || arg != c->words)){
    freeargs(argv, argc);
    return launch(c, 0);
  }
  status = redirect(c->redirs, directly || which == BI_EXEC ? 0 : saved);
  if(status)
    goto done;
  for(w = c->words; w != arg; w = w->next){
    if(assignvar(w->raw, argc != 0) < 0){
      status = problem("invalid assignment");
      goto done;
    }
  }
  if(which)
    status = builtin_run(which, argv, argc);
  else if(argc)
    status = execute(argv);

done:
  if(!directly)
    restore(saved);
  freeargs(argv, argc);
  return status;
}

static int
runpipe(struct command *c)
{
  int p[2], left, right, status = 1;

  if(pipe(p) < 0)
    return problem("pipe failed");
  left = fork();
  if(left == 0){
    close(p[0]);
    dup2(p[1], 1);
    if(p[1] != 1)
      close(p[1]);
    exitstatus(worker(c->a));
  }
  if(left < 0){
    close(p[0]);
    close(p[1]);
    return problem("fork failed");
  }
  right = fork();
  if(right == 0){
    close(p[1]);
    dup2(p[0], 0);
    if(p[0] != 0)
      close(p[0]);
    exitstatus(worker(c->b));
  }
  close(p[0]);
  close(p[1]);
  if(right < 0)
    problem("fork failed");
  else
    status = child_status(right);
  child_status(left);
  return status;
}

static int
worker(struct command *c)
{
  if(c->type == SIMPLE)
    return simple(c, 1);
  if(c->type == PIPE)
    return runpipe(c);
  if(c->type == GROUP){
    if(redirect(c->redirs, 0))
      return 1;
    return evaluate(c->a);
  }
  return eval(c);
}

static int
for_loop(struct command *c)
{
  char *items[SH_ARGV+1];
  int n, i, status = 0;

  n = arguments(c->words, items, SH_ARGV);
  if(n < 0)
    return problem("could not expand for list or too many items");
  loopdepth++;
  for(i = 0; i < n && !sh_interrupted; i++){
    if(setvar(c->name, items[i], 0) < 0){
      status = problem("could not set loop variable");
      break;
    }
    status = evaluate(c->a);
    if(flow == FLOW_BREAK)
      break;
    flow = FLOW_NONE;
  }
  loopdepth--;
  flow = FLOW_NONE;
  freeargs(items, n);
  return status;
}

static int
while_loop(struct command *c)
{
  int condition, status = 0;

  loopdepth++;
  while(!sh_interrupted){
    condition = evaluate(c->a);
    if(flow == FLOW_BREAK)
      break;
    if(flow == FLOW_CONTINUE){
      flow = FLOW_NONE;
      continue;
    }
    if((condition == 0) != (c->type == WHILE) || sh_interrupted)
      break;
    status = evaluate(c->b);
    if(flow == FLOW_BREAK)
      break;
    flow = FLOW_NONE;
  }
  loopdepth--;
  flow = FLOW_NONE;
  return status;
}

static int
evalbody(struct command *c)
{
  int status;

  switch(c->type){
  case SIMPLE:
    return simple(c, 0);
  case GROUP:
  case PIPE:
    return launch(c, 0);
  case BACK:
    return launch(c->a, 1);
  case AND:
  case OR:
    status = sh_status = eval(c->a);
    if(!flow && !sh_interrupted && ((status == 0) == (c->type == AND)))
      return eval(c->b);
    return status;
  case IF:
    status = evaluate(c->a);
    if(flow || sh_interrupted)
      return status;
    return evaluate(status == 0 ? c->b : c->c);
  case FOR:
    return for_loop(c);
  case WHILE:
  case UNTIL:
    return while_loop(c);
  }
  return 1;
}

static int
eval(struct command *c)
{
  int saved[3] = {-2, -2, -2}, status;

  if(!c->redirs || c->type == SIMPLE || c->type == GROUP)
    return evalbody(c);
  status = redirect(c->redirs, saved);
  if(!status)
    status = evalbody(c);
  restore(saved);
  return status;
}

int
evaluate(struct command *head)
{
  int status = 0;

  for(; head && !flow && !sh_interrupted; head = head->next){
    status = eval(head);
    sh_status = status;
  }
  return sh_interrupted ? 130 : status;
}
