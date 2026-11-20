/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

// long term locks for processes
struct sleeplock {
  uint locked;       // Is the lock held?
  struct spinlock lk; // spinlock protecting this sleep lock
  
  // For debugging:
  char *name;        // Name of lock.
  int pid;           // process holding lock
};

