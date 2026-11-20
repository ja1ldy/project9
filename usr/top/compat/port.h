/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include <stdint.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <project9/param.h>

#define CPUSTATES 1
#define RT_TABLEID_MAX 255
#define OXTABS 0
#define Default_DELAY 0.5

long long strtonum(const char *, long long, long long, const char **);
size_t strlcat(char *, const char *, size_t);
int strcasecmp(const char *, const char *);
int strncasecmp(const char *, const char *, size_t);
int gethostname(char *, size_t);
int uptime(void);
int uid_from_user(const char *, uid_t *);
const char *user_from_uid(uid_t, int);
