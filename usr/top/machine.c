/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "top.h"
#include "machine.h"
#include "utils.h"
#include "../../include/account.h"
#include "../../include/param.h"
#include "../../include/procinfo.h"

struct handle { int index; };
static struct handle cursor;
int ncpu, ncpuonline;
static struct procinfo records[NPROC], choices[NPROC];
static int count;
static int states[5], memory[1], online[NCPU];
static int64_t cpu[NCPU * CPUSTATES];
static char * labels[] = {" runnable, ", " sleeping, ", " stopped, ", " zombie, ", " other, ", NULL};
static char * cpulabels[] = {NULL};
static char * memlabels[] = {"K free", NULL};
static char * orders[] = {"size", "pid", "command", NULL};

int cpuquery(int);
int freemem(void);

int
getncpu(void)
{
  return cpuquery(1);
}

int
getncpuonline(void)
{
  return getncpu();
}

int
machine_init(struct statics *info)
{
  ncpu = getncpu();
  if(ncpu <= 0 || ncpu > NCPU)
    return -1;
  info->procstate_names = labels;
  info->cpustate_names = cpulabels;
  info->memory_names = memlabels;
  info->order_names = orders;
  return 0;
}

void
get_system_info(struct system_info *info)
{
  for(int i = 0; i < NUM_AVERAGES; ++i)
    info->load_avg[i] = -1;
  int mask = cpuquery(2);
  for(int i = 0; i < ncpu; i++)
    online[i] = mask < 0 || (mask & (1U << i));
  memory[0] = freemem() / 1024;
  info->cpustates = cpu;
  info->cpuonline = online;
  info->memory = memory;
}

static int
state_index(char state)
{
  switch(state){
  case 'R': return 0;
  case 'S': return 1;
  case 'T': return 2;
  case 'Z': return 3;
  default: return 4;
  }
}

struct handle *
get_process_info(struct system_info *info, struct process_select *sel,
                 int (*compare)(const void *, const void *))
{
  count = getprocs(records, NPROC);
  if(count < 0)
    count = 0;
  memset(states, 0, sizeof states);
  info->procstates = states;
  info->p_total = count;
  info->p_active = 0;
  for(int i = 0; i < count; i++){
    struct procinfo *p = &records[i];
    states[state_index(p->state)]++;
    if((!sel->idle && p->state != 'R') ||
       (sel->uid != (uid_t)-1 && (uid_t)p->uid != sel->uid) ||
       (sel->huid != (uid_t)-1 && (uid_t)p->uid == sel->huid) ||
       (sel->pid != (pid_t)-1 && p->pid != sel->pid) ||
       (sel->command && !strstr(p->name, sel->command)))
      continue;
    choices[info->p_active++] = *p;
  }
  if(compare)
    qsort(choices, info->p_active, sizeof choices[0], compare);
  cursor.index = 0;
  return &cursor;
}

char *
format_header(char *user, char *wait, char *time)
{
  static char line[MAX_COLS];
  snprintf(line, sizeof line, "  PID %8s  PPID  PGID  SIZE S COMMAND", user);
  return line;
}

void
skip_processes(struct handle *handle, int amount)
{
  handle->index += amount;
}

char *
format_next_process(struct handle *handle, const char *(*username)(uid_t, int),
                    int rtable, pid_t *pid)
{
  static char line[MAX_COLS];
  const struct procinfo *p = &choices[handle->index++];
  *pid = p->pid;
  snprintf(line, sizeof line, "%5d %8.8s %5d %5d %5s %c %s", p->pid,
           username ? username(p->uid, 0) : format_uid(p->uid, 0),
           p->ppid, p->pgid, format_k(p->size / 1024), p->state, p->name);
  return line;
}

uid_t
proc_owner(pid_t pid)
{
  for(int i = 0; i < count; ++i)
    if(records[i].pid == pid)
      return records[i].uid;
  return (uid_t)-1;
}

int
find_pid(pid_t pid)
{
  int n = getprocs(records, NPROC);
  for(int i = 0; i < n; ++i)
    if(records[i].pid == pid)
      return 1;
  return 0;
}

const char *
user_from_uid(uid_t uid, int ignore)
{
  static struct account_info info;
  return accountinfo(uid, &info) == 0 ? info.name : format_uid(uid, ignore);
}

int
uid_from_user(const char *name, uid_t *uid)
{
  struct account_info info;
  *uid = getuid();
  if(accountinfo(*uid, &info) == 0 && !strcmp(name, info.name))
    return 0;
  if(!strcmp(name, "root")){
    *uid = 0;
    return 0;
  }
  return -1;
}

static int
by_pid(const void *a, const void *b)
{
  const struct procinfo *p = a, *q = b;
  return (p->pid > q->pid) - (p->pid < q->pid);
}

static int
by_size(const void *a, const void *b)
{
  const struct procinfo *p = a, *q = b;
  return p->size == q->size ? by_pid(a, b) : (p->size < q->size ? 1 : -1);
}

static int
by_name(const void *a, const void *b)
{
  const struct procinfo *p = a, *q = b;
  int cmp = strcmp(p->name, q->name);
  return cmp ? cmp : by_pid(a, b);
}

int (*proc_compares[])(const void *, const void *) = {by_size, by_pid, by_name};
