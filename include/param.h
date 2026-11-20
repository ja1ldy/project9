/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

#define NPROC        64  // maximum number of processes
#define USTACKPAGES 16   // user stack pages above the guard page
#define KSTACKSIZE 4096  // size of per-process kernel stack
#define NCPU          8  // maximum number of CPUs
#define NOFILE       32  // open files per process
#define NFILE       256  // open files per system
#define NINODE       256  // maximum number of active i-nodes
#define NDEV         10  // maximum major device number
#define ROOTDEV       1  // device number of file system root disk
#define MAXENV       32  // max exec environment variables
#define MAXARG       32  // max exec arguments

