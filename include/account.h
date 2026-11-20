/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#ifndef PROJECT9_ACCOUNT_H
#define PROJECT9_ACCOUNT_H
#define ACCOUNT_NAME 15
#define ACCOUNT_PASSWORD 97
#define ACCOUNT_MIN_PASSWORD 12
#define ACCOUNT_SALT 16
#define ACCOUNT_VERIFY 1
#define ACCOUNT_CHANGE 2
#define ACCOUNT_ADD 3
#define ACCOUNT_SETUP 4

// the header and each account record occupy one disk sector apiece
struct account_header {
  unsigned char seed[32];
  unsigned char reserved[480];
};

struct account_record {
  unsigned int uid;
  unsigned int rounds;
  char name[ACCOUNT_NAME];
  unsigned char salt[ACCOUNT_SALT];
  unsigned char hash[32];
  unsigned char group[2];  // little endian gid zero means use the uid
  unsigned char reserved[439];
};

struct account_info {
  char name[ACCOUNT_NAME];
  char home[sizeof("/home/") + ACCOUNT_NAME];
};

int accountinfo(int, struct account_info*);

int readline(char*, int);
int readpassword(const char*, char*);
int newpassword(char*);
void scrub(void*, unsigned int);
#endif
