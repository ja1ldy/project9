/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#include "types.h"
#include "user.h"
#include "account.h"
#include "tty.h"

// clear the password buffer even if the compiler can see it is no longer used
void
scrub(void *p, uint n)
{
  volatile char *bytes = p;
  while(n--)
    *bytes++ = 0;
}

int
readline(char *dst, int size)
{
  int used = 0, overflow = 0, n;
  char c;

  // read through the newline even when the input cannot fit
  while((n = read(0, &c, 1)) == 1 && c != '\n'){
    if(c == 0 || used == size - 1)
      overflow = 1;
    else
      dst[used++] = c;
  }
  dst[used] = 0;
  if(n <= 0 || overflow)
    return -1;
  return used;
}

int
readpassword(const char *prompt, char *password)
{
  int n;

  if(setecho(0) < 0)
    return -1;
  printf(1, "%s", prompt);
  n = readline(password, ACCOUNT_PASSWORD);
  setecho(1);
  printf(1, "\n");
  return n;
}

int
newpassword(char *password)
{
  char confirm[ACCOUNT_PASSWORD];
  int ok = readpassword("New password: ", password) >= ACCOUNT_MIN_PASSWORD;

  if(ok)
    ok = readpassword("Retype new password: ", confirm) >= 0 &&
         strcmp(password, confirm) == 0;
  scrub(confirm, sizeof(confirm));
  if(!ok)
    printf(2, "Passwords must match and contain 12 to 96 characters.\n");
  return ok ? 0 : -1;
}
