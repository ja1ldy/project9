/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

struct buf;
struct context;
struct file;
struct local;
struct pty;
struct inode;
struct pipe;
struct proc;
struct rtcdate;
struct spinlock;
struct sleeplock;
struct stat;
struct statx;
struct superblock;
struct timeval;
struct trapframe;

// bio.c

// console.c
void            consoleinit(void);
void            fbinit(void);
void            fbputc(int);
int             fb_map(pde_t*);
void            cprintf(char*, ...);
void            consoleintr(int(*)(void));
void            consolekeyintr(int(*)(void));
void            panic(const char*) __attribute__((noreturn));
void            panictrap(struct trapframe*) __attribute__((noreturn));

// exec.c
int             execve(char*, char**, char**);

// file.c
struct file*    filealloc(void);
void            fileclose(struct file*);
struct file*    filedup(struct file*);
void            fileinit(void);
int             fileread(struct file*, char*, int n);
int             filestat(struct file*, struct stat*);
int             filewrite(struct file*, char*, int n);

// fs.c
int             dirlink(struct inode*, char*, uint);
struct inode*   iget(uint, uint);
struct p9_direntx;
int             nextdent(struct inode*, uint*, struct p9_direntx*);
int             dirreplace(struct inode*, uint, const char*, uint);
struct inode*   dirlookup(struct inode*, char*, uint*);
struct inode*   ialloc(uint, short);
struct inode*   idup(struct inode*);
#ifdef P9_LEGACY_ROOT
void            iinit(int);
void            binit(void);
void            legacydiskinit(void);
void            initlog(int);
void            log_write(struct buf*);
void            bwrite(struct buf*);
void            brelse(struct buf*);
void            iderw(struct buf*);
struct buf*     bread(uint, uint);
struct superblock;
void            readsb(int, struct superblock*);
void            itrunc(struct inode*);
#else
void            iinit(void);
#endif
int             isroot(struct inode*);
void            ilock(struct inode*);
void            iput(struct inode*);
void            iunlock(struct inode*);
void            iunlockput(struct inode*);
int             iupdate(struct inode*);
int             namecmp(const char*, const char*);
struct inode*   namei(char*);
struct inode*   nameiparent(char*, char*);
int             readi(struct inode*, char*, uint, uint);
void            stati(struct inode*, struct stat*);
int             writei(struct inode*, char*, uint, uint);

// ide.c

// ioapic.c
void            ioapicenable(int irq, int cpu);
extern uchar    ioapicid;
void            ioapicinit(void);

// kalloc.c
char*           kalloc(void);
void            kfree(char*);
void            kinit1(void*, void*);
void            freerange(void*, void*);
void            kinit2(void*, void*);

// kbd.c
void            kbdintr(void);
void            ps2init(void);
void            ps2key(uint);
void            ps2aux(int);
void            memorydevinit(void);
void            rtcinit(void);
void            fbdevinit(void);
struct fbinfo;
int             fbgetinfo(struct fbinfo*);
void            fbtextsize(int*, int*);
int             fbpixels(char*, uint, uint);

// lapic.c
int             lapicid(void);
extern volatile uint*    lapic;
void            lapiceoi(void);
void            lapicinit(void);
void            lapicstartap(uchar, uint);
void            microdelay(int);
void            lapicdelay(uint);

// log.c
void            begin_op();
void            end_op();

// mp.c
extern int      ismp;
void            mpinit(void);

// picirq.c
void            picenable(int);
void            picinit(void);
void            picenable(int);
void            irqeoi(int);

// pipe.c
int             pipealloc(struct file**, struct file**);
void            pipeclose(struct pipe*, int);
int             piperead(struct pipe*, char*, int, int);
int             pipequeued(struct pipe*);
int             pipewrite(struct pipe*, char*, int, int);

//PAGEBREAK: 16
// proc.c
int             cpuid(void);
void            exit(void);
int             fork(void);
int             growproc(int);
int             kill(int);
struct cpu*     mycpu(void);
struct proc*    myproc();
void            pinit(void);
void            procdump(void);
void            scheduler(void) __attribute__((noreturn));
void            sched(void);
void            setproc(struct proc*);
void            sleep(void*, struct spinlock*);
void            userinit(void);
int             wait(void);
void            wakeup(void*);
void            yield(void);
int             cps(void);
int             freemem(void);
int             setecho(int);

// swtch.S
void            swtch(struct context**, struct context*);

// spinlock.c
void            acquire(struct spinlock*);
void            getcallerpcs(void*, uptr*);
int             holding(struct spinlock*);
void            initlock(struct spinlock*, char*);
void            release(struct spinlock*);
void            pushcli(void);
void            popcli(void);

// sleeplock c
void            acquiresleep(struct sleeplock*);
void            releasesleep(struct sleeplock*);
int             holdingsleep(struct sleeplock*);
void            initsleeplock(struct sleeplock*, char*);

// string.c
int             memcmp(const void*, const void*, uint);
void*           memmove(void*, const void*, uint);
void*           memset(void*, int, uint);
char*           safestrcpy(char*, const char*, int);
int             strlen(const char*);
int             strncmp(const char*, const char*, uint);
char*           strncpy(char*, const char*, int);

// syscall.c
int             argint(int, int*);
int             argptr(int, char**, int);
int             argstr(int, char**);
int             argstrn(int, char**, uint);
int             argptrwrite(int, char**, int);
int             fetchstrn(uint, char**, uint);
int             userpage(pde_t*, uint, int);
int             fetchint(uint, int*);
int             fetchaddr(uint, uint*);
int             fetchstr(uint, char**);
void            syscall(void);

// timer.c
void            timerinit(void);

// trap.c
void            idtinit(void);
extern uint     ticks;
void            tvinit(void);
extern struct spinlock tickslock;

// uart.c
void            uartinit(void);
void            uartintr(void);
void            uartputc(int);

// vm.c
void            seginit(void);
void            kvmalloc(void);
pde_t*          setupkvm(void);
char*           uva2ka(pde_t*, char*);
int             allocuvm(pde_t*, uint, uint);
int             deallocuvm(pde_t*, uint, uint);
void            freevm(pde_t*);
void            inituvm(pde_t*, char*, uint);
int             loaduvm(pde_t*, char*, struct inode*, uint, uint);
pde_t*          copyuvm(pde_t*, uint);
void            switchuvm(struct proc*);
void            switchkvm(void);
int             copyout(pde_t*, uint, void*, uint);
void            userreadonly(pde_t*, uint, uint);
void            clearpteu(pde_t *pgdir, char *uva);

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))

struct trapframe;
int             forkin(int);
int             jobctl(int, int, int*);
int             ttyforeground(void);
void            ttysignal(int);
void            ttyrestore(void);
int             ttyinputowner(struct pty *);
int             ttysetcbreak(struct pty *, int);
int             consolemode(int);
int             consoleioctl(int, void*);
void            fbstyle(int, int);
void            fbctl(int, int, int);
int             sleepintr(void*, struct spinlock*);
int             setsignal(int, uint, uint);
int             signalreturn(void);
void            signalexec(void);
void            signaldeliver(struct trapframe*);
int             coredump(struct trapframe*);
void            fbflush(void);
int             fbactive(void);
int             mappages(pde_t*, void*, uint, uint, int);

int             inodeaccess(struct inode*, int);
struct inode*   create(char*, short, uint, uint, uint, int, int*);
void            statxi(struct inode*, struct statx*);
char*           uva2ka(pde_t*, char*);
void            accountinit(void);
int             remove_path(char*, int);

int             procsetid(int, int);
void            setprocname(const char *);
int             parentpid(void);
int             procwait(int, int*, int);
void            procexit(int) __attribute__((noreturn));
int             procsignal(int, int);

struct procinfo;
int             procsnapshot(struct procinfo*, int);

int             fbmapuser(pde_t*, uint, int);
void            fbunmapuser(struct proc*);
void            fbgraphics(int);
void            fbdisplay(int);
int             rtcgettime(struct timeval*);
int             fileready(struct file*, int);
int             pipeready(struct pipe*, int);
int             newsession(void);
uint            signalmask(int, uint);

void            fpuexec(void);
int             setsignalx(int, uint, uint, uint, uint*);

struct inode*   nameinofollow(char*);
void            namelock(void);
void            nameunlock(void);
void            selectnotify(void);

int             fsfreeze(void);
void            acpiinit(void);
void            acpireset(void);
void            acpipoweroff(void);

void            uhciinit(void);
void            uhci_tick(void);
void            usb_scancode(uint);
void            usb_mouse_attach(void);
void            usb_mouse(int, int, uint);

void fsrelease(void);
int inodebusy(uint);

void localinit(void);
int localalloc(struct file *, int);
int localfamily(struct local *);
int localport(struct local *, int);
int localreserve(struct local *);
void localname(struct local *, struct inode *);
void localclose(struct local *);
int locallisten(struct local *, int);
int localconnect(struct local *, struct inode *, int);
int localaccept(struct file *, struct file *);
int localio(struct local *, char *, int, int, int);
int localready(struct local *, int);
int localqueued(struct local *);

void ptyinit(void);
int ptyalloc(struct file *, struct file *);
int ptyopen(struct file *, int, int, int, int, int);
void ptyclose(struct pty *, int);
int ptymetadata(struct file *, struct stat *);
int ptyioctl(struct file *, int, void *);
int ptymode(struct file *, int);
int ptyready(struct pty *, int, int);
int ptyread(struct pty *, int, char *, int, int);
int ptywrite(struct pty *, int, const char *, int, int);
int *ptyforeground(struct pty *);
int ptyassign(struct file *);
void ptysignal(struct pty *, int);
int ptyowner(struct pty *);
int pipeevents(struct pipe *, int);
int localevents(struct local *);
int ptyevents(struct pty *, int);
int filepoll(struct file *, int);
int localshutdown(struct local *, int);
int ptyreadcheck(struct pty *);
