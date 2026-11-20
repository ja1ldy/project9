/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

/*
 * unix v6 ed adapted from the ed v6 source supplied with project9
 * see documentation ed md for provenance commands and limits
 */
#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "setjmp.h"
#include "tty.h"

#define	FNSIZE	64
#define	LBSIZE	512
#define	ESIZE	128
#define	GBSIZE	256
#define	NBRA	5
#define	EOF	-1

#define	CBRA	1
#define	CCHR	2
#define	CDOT	4
#define	CCL	6
#define	NCCL	8
#define	CDOL	10
#define	CEOF	11
#define	CKET	12

#define	STAR	01

#define error errfunc()
#define MAXLINES 32768
#define TEXTSPACE (128 * 1024)

static jmp_buf setexit;
static int peekc, lastc, circfl, ninbuf, pflag, listf, col;
static char savedfile[FNSIZE], file[FNSIZE];
static char linebuf[LBSIZE], rhsbuf[LBSIZE/2], expbuf[ESIZE+4], genbuf[LBSIZE];
// each line is an offset into scratch dot is current and dol is last
static int addresses[MAXLINES + 1];
static int *zero = addresses, *dot, *dol, *addr1, *addr2;
static uint count;
static char *nextip, *linebp, *globp;
static int io = -1, vflag = 1;
static char *loc1, *loc2, *locs, *braslist[NBRA], *braelist[NBRA];
static int names[26];
static char *scratch;
static int tline;
static volatile int interrupted;

static void commands(void), onintr(int), errfunc(void) __attribute__((noreturn));
static int getchar(void);
static int *address(void);
static void setdot(void), setall(void), setnoaddr(void), nonzero(void);
static void newline(void), filename(void), exfile(void);
static int gettty(void), getfile(void), getsub(void), getcopy(void);
static void putfile(void), run_unix(void), delete(void), init(void);
static int append(int (*)(void), int *);
static char *getline(int), *place(char *, char *, char *);
static int putline(void);
static void global(int), substitute(int), dosub(void), move(int);
static int compsub(void);
static void reverse(int *, int *), compile(int);
static int execute(int, int *), advance(char *, char *), cclass(char *, int, int);
static void putd(void), puts(const char *), putc(int);

int
main(int argc, char **argv)
{
  signal(SIGQUIT, SIG_IGN);
  signal(SIGINT, onintr);
  argv++;
  if(argc > 1 && **argv == '-'){
    vflag = 0;
    if((*argv)[1] == 'q'){
      signal(SIGQUIT, SIG_DFL);
      vflag = 1;
    }
    argv++;
    argc--;
  }
  if(argc > 2 || (argc > 1 && strlen(*argv) >= sizeof(savedfile))){
    printf(2, "usage: ed [-|-q] [file]\n");
    exit();
  }
  if(argc > 1){
    strcpy(savedfile, *argv);
    globp = "r";
  }
  scratch = malloc(TEXTSPACE);
  if(!scratch){
    printf(2, "ed: out of memory\n");
    exit();
  }
  init();
  setjmp(setexit);
  commands();
  exit();
}

static void commands(void)
{
	int *a1, c;

	for (;;) {
	if (pflag) {
		pflag = 0;
		addr1 = addr2 = dot;
		goto print;
	}
	addr1 = 0;
	addr2 = 0;
	do {
		addr1 = addr2;
		if ((a1 = address())==0) {
			c = getchar();
			break;
		}
		addr2 = a1;
		if ((c=getchar()) == ';') {
			c = ',';
			dot = a1;
		}
	} while (c==',');
	if (addr1==0)
		addr1 = addr2;
	switch(c) {

	case 'a':
		setdot();
		newline();
		append(gettty, addr2);
		continue;

	case 'c':
		delete();
		append(gettty, addr1-1);
		continue;

	case 'd':
		delete();
		continue;

	case 'e':
		setnoaddr();
		if ((peekc = getchar()) != ' ')
			error;
		savedfile[0] = 0;
		init();
		addr2 = zero;
		goto caseread;

	case 'f':
		setnoaddr();
		if ((c = getchar()) != '\n') {
			peekc = c;
			savedfile[0] = 0;
			filename();
		}
		puts(savedfile);
		continue;

	case 'g':
		global(1);
		continue;

	case 'i':
		setdot();
		nonzero();
		newline();
		append(gettty, addr2-1);
		continue;

	case 'k':
		if ((c = getchar()) < 'a' || c > 'z')
			error;
		newline();
		setdot();
		nonzero();
		names[c-'a'] = *addr2 | 01;
		continue;

	case 'm':
		move(0);
		continue;

	case '\n':
		if (addr2==0)
			addr2 = dot+1;
		addr1 = addr2;
		goto print;

	case 'l':
		listf++;
	case 'p':
		newline();
	print:
		setdot();
		nonzero();
		a1 = addr1;
		do
			puts(getline(*a1++));
		while (a1 <= addr2);
		dot = addr2;
		listf = 0;
		continue;

	case 'q':
		setnoaddr();
		newline();
		exit();

	case 'r':
	caseread:
		filename();
		if ((io = open(file, 0)) < 0) {
			lastc = '\n';
			error;
		}
		setall();
		ninbuf = 0;
		append(getfile, addr2);
		exfile();
		continue;

	case 's':
		setdot();
		nonzero();
		substitute(globp != 0);
		continue;

	case 't':
		move(1);
		continue;

	case 'v':
		global(0);
		continue;

	case 'w':
		setall();
		filename();
		if ((io = open(file, O_CREATE | O_WRONLY | O_TRUNC)) < 0)
			error;
		if (addr1 != zero)
			putfile();
		exfile();
		continue;

	case '=':
		setall();
		newline();
		count = addr2 - zero;
		putd();
		putc('\n');
		continue;

	case '!':
		run_unix();
		continue;

	case EOF:
		return;

	}
	error;
	}
}

static int *address(void)
{
	int *a1, minus, c;
	int n, relerr;

	minus = 0;
	a1 = 0;
	for (;;) {
		c = getchar();
		if ('0'<=c && c<='9') {
			n = 0;
			do {
				if (n > MAXLINES)
					error;
				n *= 10;
				n += c - '0';
			} while ((c = getchar())>='0' && c<='9');
			peekc = c;
			if (a1==0)
				a1 = zero;
			if (minus<0)
				n = -n;
			if (n > dol - a1 || n < zero - a1)
				error;
			a1 += n;
			minus = 0;
			continue;
		}
		relerr = 0;
		if (a1 || minus)
			relerr++;
		switch(c) {
		case ' ':
		case '\t':
			continue;
	
		case '+':
			minus++;
			if (a1==0)
				a1 = dot;
			continue;

		case '-':
		case '^':
			minus--;
			if (a1==0)
				a1 = dot;
			continue;
	
		case '?':
		case '/':
			compile(c);
			a1 = dot;
			for (;;) {
				if (c=='/') {
					a1++;
					if (a1 > dol)
						a1 = zero;
				} else {
					a1 = a1 == zero ? dol : a1 - 1;
				}
				if (execute(0, a1))
					break;
				if (a1==dot)
					error;
			}
			break;
	
		case '$':
			a1 = dol;
			break;
	
		case '.':
			a1 = dot;
			break;

		case '\'':
			if ((c = getchar()) < 'a' || c > 'z')
				error;
			for (a1=zero; a1<=dol; a1++)
				if (names[c-'a'] == (*a1|01))
					break;
			break;
	
		default:
			peekc = c;
			if (a1==0)
				return(0);
			if (minus > dol - a1 || minus < zero - a1)
				error;
			a1 += minus;
			return(a1);
		}
		if (relerr)
			error;
	}
}

static void setdot(void)
{
	if (addr2 == 0)
		addr1 = addr2 = dot;
	if (addr1 > addr2)
		error;
}

static void setall(void)
{
	if (addr2==0) {
		addr1 = zero+1;
		addr2 = dol;
		if (dol==zero)
			addr1 = zero;
	}
	setdot();
}

static void setnoaddr(void)
{
	if (addr2)
		error;
}

static void nonzero(void)
{
	if (addr1<=zero || addr2>dol)
		error;
}

static void newline(void)
{
	int c;

	if ((c = getchar()) == '\n')
		return;
	if (c=='p' || c=='l') {
		pflag++;
		if (c=='l')
			listf++;
		if (getchar() == '\n')
			return;
	}
	error;
}

static void filename(void)
{
	register char *p1, *p2;
	int c;

	count = 0;
	c = getchar();
	if (c=='\n' || c==EOF) {
		p1 = savedfile;
		if (*p1==0)
			error;
		p2 = file;
		while ((*p2++ = *p1++) != 0)
			;
		return;
	}
	if (c!=' ')
		error;
	while ((c = getchar()) == ' ');
	if (c=='\n')
		error;
	p1 = file;
	do {
		if (c == EOF || p1 == file + FNSIZE - 1)
			error;
		*p1++ = c;
	} while ((c = getchar()) != '\n');
	*p1++ = 0;
	if (savedfile[0]==0) {
		p1 = savedfile;
		p2 = file;
		while ((*p1++ = *p2++) != 0)
		;
	}
}

static void exfile(void)
{
	close(io);
	io = -1;
	if (vflag) {
		putd();
		putc('\n');
	}
}

static void
onintr(int sig)
{
  // let the command loop handle cleanup outside the signal handler
  interrupted = 1;
}

static void
check_interrupt(void)
{
  if(interrupted){
    interrupted = 0;
    putc('\n');
    lastc = '\n';
    error;
  }
}

static void
errfunc(void)
{
  int c;

  listf = pflag = 0;
  puts("?");
  count = 0;
  if(globp)
    lastc = '\n';
  globp = 0;
  peekc = lastc;
  while((c = getchar()) != '\n' && c != EOF)
    ;
  if(io >= 0){
    close(io);
    io = -1;
  }
  longjmp(setexit, 1);
}

static int getchar(void)
{
	unsigned char c;
	int n;

	check_interrupt();
	if ((lastc = peekc) != 0) {
		peekc = 0;
		return(lastc);
	}
	if (globp) {
		if ((lastc = *globp++) != 0)
			return(lastc);
		globp = 0;
		return(EOF);
	}
	n = read(0, &c, 1);
	check_interrupt();
	if (n <= 0)
		return(lastc = EOF);
	lastc = c & 0177;
	return(lastc);
}

static int gettty(void)
{
	int c, gf;
	register char *p;

	p = linebuf;
	gf = globp != 0;
	while ((c = getchar()) != '\n') {
		if (c==EOF) {
			if (gf)
				peekc = c;
			return(c);
		}
		if ((c &= 0177) == 0)
			continue;
		*p++ = c;
		if (p >= &linebuf[LBSIZE-2])
			error;
	}
	*p++ = 0;
	if (linebuf[0]=='.' && linebuf[1]==0)
		return(EOF);
	return(0);
}

static int
getfile(void)
{
  int c, n;
  char *lp = linebuf;

  for(;;){
    if(ninbuf == 0){
      n = read(io, genbuf, sizeof(genbuf));
      check_interrupt();
      if(n < 0)
        error;
      if(n == 0){
        if(lp == linebuf)
          return EOF;
        break;
      }
      ninbuf = n;
      nextip = genbuf;
    }
    c = *nextip++ & 0177;
    ninbuf--;
    count++;
    if(c == '\n')
      break;
    if(!c)
      continue;
    if(lp == linebuf + LBSIZE - 1)
      error;
    *lp++ = c;
  }
  *lp = 0;
  return 0;
}

static void putfile(void)
{
	int *a1;
	register char *fp, *lp;
	int nib;

	nib = 512;
	fp = genbuf;
	a1 = addr1;
	do {
		lp = getline(*a1++);
		for (;;) {
			if (--nib < 0) {
				if (write(io, genbuf, fp-genbuf) != fp-genbuf)
				error;
				nib = 511;
				fp = genbuf;
			}
			count++;
			if ((*fp++ = *lp++) == 0) {
				fp[-1] = '\n';
				break;
			}
		}
	} while (a1 <= addr2);
	if (write(io, genbuf, fp-genbuf) != fp-genbuf)
		error;
}

static int append(int (*f)(void), int *a)
{
	int *a1, *a2, *rdot;
	int nline, tl;

	nline = 0;
	dot = a;
	while ((*f)() == 0) {
		if (dol == zero + MAXLINES)
			error;
		tl = putline();
		nline++;
		a1 = ++dol;
		a2 = a1+1;
		rdot = ++dot;
		while (a1 > rdot)
			*--a2 = *--a1;
		*rdot = tl;
	}
	return(nline);
}

static void
run_unix(void)
{
  char cmd[256];
  char *argv[] = {"sh", "-c", cmd, 0};
  int c, i = 0, pid, child;

  setnoaddr();
  while((c = getchar()) != '\n' && c != EOF){
    if(i == sizeof(cmd) - 1)
      error;
    cmd[i++] = c;
  }
  cmd[i] = 0;
  if(!i)
    error;
  pid = fork();
  if(pid < 0)
    error;
  if(pid == 0){
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    exec("/bin/sh", argv);
    exit();
  }
  while((child = wait()) != pid && child >= 0)
    ;
  check_interrupt();
  puts("!");
}

static void delete(void)
{
	int *a1, *a2, *a3;

	setdot();
	newline();
	nonzero();
	a1 = addr1;
	a2 = addr2+1;
	a3 = dol;
	dol -= a2 - a1;
	do
		*a1++ = *a2++;
	while (a2 <= a3);
	a1 = addr1;
	if (a1 > dol)
		a1 = dol;
	dot = a1;
}

static char *
getline(int tl)
{
  // global commands use the low bit as a mark while they visit lines
  strcpy(linebuf, scratch + (tl & ~1));
  return linebuf;
}

static int
putline(void)
{
  char *p = linebuf;
  int off = tline, len;

  while(*p && *p != '\n')
    p++;
  linebp = *p ? p + 1 : 0;
  len = p - linebuf;
  if(len + 2 > TEXTSPACE - tline){
    puts("TMP");
    error;
  }
  memmove(scratch + tline, linebuf, len);
  scratch[tline + len] = 0;
  tline += (len + 2) & ~1;
  return off;
}

static void
init(void)
{
  tline = 2;
  scratch[0] = 0;
  addresses[0] = 0;
  dot = dol = zero;
  memset(names, 0, sizeof(names));
}

static void global(int k)
{
	register char *gp;
	int c;
	register int *a1;
	char globuf[GBSIZE];

	if (globp)
		error;
	setall();
	nonzero();
	if ((c=getchar())=='\n')
		error;
	compile(c);
	gp = globuf;
	while ((c = getchar()) != '\n') {
		if (c==EOF)
			error;
		if (c=='\\') {
			c = getchar();
			if (c!='\n')
				*gp++ = '\\';
		}
		*gp++ = c;
		if (gp >= &globuf[GBSIZE-2])
			error;
	}
	*gp++ = '\n';
	*gp++ = 0;
	for (a1=zero; a1<=dol; a1++) {
		*a1 &= ~01;
		if (a1>=addr1 && a1<=addr2 && execute(0, a1)==k)
			*a1 |= 01;
	}
	for (a1=zero; a1<=dol; a1++) {
		if (*a1 & 01) {
			*a1 &= ~01;
			dot = a1;
			globp = globuf;
			commands();
			a1 = zero;
		}
	}
}

static void substitute(int inglob)
{
	int gsubf, *a1, nl;

	gsubf = compsub();
	for (a1 = addr1; a1 <= addr2; a1++) {
		if (execute(0, a1)==0)
			continue;
		inglob |= 01;
		dosub();
		if (gsubf) {
			while (*loc2) {
				if (execute(1, 0)==0)
					break;
				dosub();
			}
		}
		*a1 = putline();
		nl = append(getsub, a1);
		a1 += nl;
		addr2 += nl;
	}
	if (inglob==0)
		error;
}

static int compsub(void)
{
	int seof, c;
	register char *p;

	if ((seof = getchar()) == '\n' || seof == EOF)
		error;
	compile(seof);
	p = rhsbuf;
	for (;;) {
		c = getchar();
		if (c == EOF)
			error;
		if (c=='\\')
			c = getchar() | 0200;
		if (c=='\n')
			error;
		if (c==seof)
			break;
		*p++ = c;
		if (p >= &rhsbuf[LBSIZE/2])
			error;
	}
	*p++ = 0;
	if ((peekc = getchar()) == 'g') {
		peekc = 0;
		newline();
		return(1);
	}
	newline();
	return(0);
}

static int getsub(void)
{
	register char *p1, *p2;

	p1 = linebuf;
	if ((p2 = linebp) == 0)
		return(EOF);
	while ((*p1++ = *p2++) != 0)
		;
	linebp = 0;
	return(0);
}

static void dosub(void)
{
	register char *lp, *sp, *rp;
	int c;

	lp = linebuf;
	sp = genbuf;
	rp = rhsbuf;
	while (lp < loc1)
		*sp++ = *lp++;
	while ((c = *rp++) != 0) {
		if (c=='&') {
			sp = place(sp, loc1, loc2);
			continue;
		} else if (c<0 && (c &= 0177) >='1' && c < NBRA+'1') {
			sp = place(sp, braslist[c-'1'], braelist[c-'1']);
			continue;
		}
		*sp++ = c&0177;
		if (sp >= &genbuf[LBSIZE])
			error;
	}
	lp = loc2;
	loc2 = linebuf + (sp - genbuf);
	while ((*sp++ = *lp++) != 0)
		if (sp >= &genbuf[LBSIZE])
			error;
	lp = linebuf;
	sp = genbuf;
	while ((*lp++ = *sp++) != 0)
		;
}

static char *place(char *asp, char *al1, char *al2)
{
	register char *sp, *l1, *l2;

	sp = asp;
	l1 = al1;
	l2 = al2;
	if (!l1 || !l2)
		return sp;
	while (l1 < l2) {
		*sp++ = *l1++;
		if (sp >= &genbuf[LBSIZE])
			error;
	}
	return(sp);
}

static void move(int cflag)
{
	register int *adt, *ad1, *ad2;

	setdot();
	nonzero();
	if ((adt = address())==0)
		error;
	newline();
	ad1 = addr1;
	ad2 = addr2;
	if (cflag) {
		ad1 = dol;
		append(getcopy, ad1++);
		ad2 = dol;
	}
	ad2++;
	if (adt<ad1) {
		dot = adt + (ad2-ad1);
		if ((++adt)==ad1)
			return;
		reverse(adt, ad1);
		reverse(ad1, ad2);
		reverse(adt, ad2);
	} else if (adt >= ad2) {
		dot = adt++;
		reverse(ad1, ad2);
		reverse(ad2, adt);
		reverse(ad1, adt);
	} else
		error;
}

static void reverse(int *aa1, int *aa2)
{
	register int *a1, *a2, t;

	a1 = aa1;
	a2 = aa2;
	for (;;) {
		t = *--a2;
		if (a2 <= a1)
			return;
		*a2 = *a1;
		*a1++ = t;
	}
}

static int getcopy(void)
{
	if (addr1 > addr2)
		return(EOF);
	getline(*addr1++);
	return(0);
}

static void compile(int aeof)
{
	int eof, c;
	register char *ep;
	char *lastep = 0;
	char bracket[NBRA], *bracketp;
	int nbra;
	int cclcnt;

	ep = expbuf;
	eof = aeof;
	bracketp = bracket;
	nbra = 0;
	if ((c = getchar()) == eof) {
		if (*ep==0)
			error;
		return;
	}
	circfl = 0;
	if (c=='^') {
		c = getchar();
		circfl++;
	}
	if (c=='*')
		goto cerror;
	peekc = c;
	for (;;) {
		if (ep >= &expbuf[ESIZE])
			goto cerror;
		c = getchar();
		if (c==eof && c != EOF) {
			if (bracketp != bracket)
				goto cerror;
			*ep++ = CEOF;
			return;
		}
		if (c!='*')
			lastep = ep;
		switch (c) {

		case '\\':
			if ((c = getchar())=='(') {
				if (nbra >= NBRA)
					goto cerror;
				*bracketp++ = nbra;
				*ep++ = CBRA;
				*ep++ = nbra++;
				continue;
			}
			if (c == ')') {
				if (bracketp <= bracket)
					goto cerror;
				*ep++ = CKET;
				*ep++ = *--bracketp;
				continue;
			}
			*ep++ = CCHR;
			if (c=='\n' || c==EOF)
				goto cerror;
			*ep++ = c;
			continue;

		case '.':
			*ep++ = CDOT;
			continue;

		case EOF:
		case '\n':
			goto cerror;

		case '*':
			if (!lastep || *lastep==CBRA || *lastep==CKET || *lastep==CDOL)
				error;
			*lastep |= STAR;
			continue;

		case '$':
			if ((peekc=getchar()) != eof)
				goto defchar;
			*ep++ = CDOL;
			continue;

		case '[':
			*ep++ = CCL;
			*ep++ = 0;
			cclcnt = 1;
			if ((c=getchar()) == '^') {
				c = getchar();
				ep[-2] = NCCL;
			}
			do {
				if (c=='\n')
					goto cerror;
				*ep++ = c;
				cclcnt++;
				if (ep >= &expbuf[ESIZE])
					goto cerror;
			} while ((c = getchar()) != ']');
			lastep[1] = cclcnt;
			continue;

		defchar:
		default:
			*ep++ = CCHR;
			*ep++ = c;
		}
	}
   cerror:
	expbuf[0] = 0;
	error;
}

static int execute(int gf, int *addr)
{
	register char *p1, *p2, c;

	if (gf) {
		if (circfl)
			return(0);
		locs = p1 = loc2;
	} else {
		if (addr==zero)
			return(0);
		p1 = getline(*addr);
		locs = 0;
	}
	memset(braslist, 0, sizeof(braslist));
	memset(braelist, 0, sizeof(braelist));
	p2 = expbuf;
	if (circfl) {
		loc1 = p1;
		return(advance(p1, p2));
	}
	/* if the pattern starts with a literal skip straight to matching bytes */
	if (*p2==CCHR) {
		c = p2[1];
		do {
			if (*p1!=c)
				continue;
			if (advance(p1, p2)) {
				loc1 = p1;
				return(1);
			}
		} while (*p1++);
		return(0);
	}
	/* try the pattern at each position including the end of the line */
	do {
		if (advance(p1, p2)) {
			loc1 = p1;
			return(1);
		}
	} while (*p1++);
	return(0);
}

static int advance(char *alp, char *aep)
{
	register char *lp, *ep, *curlp;

	lp = alp;
	ep = aep;
	check_interrupt();
	for (;;) switch (*ep++) {

	case CCHR:
		if (*ep++ == *lp++)
			continue;
		return(0);

	case CDOT:
		if (*lp++)
			continue;
		return(0);

	case CDOL:
		if (*lp==0)
			continue;
		return(0);

	case CEOF:
		loc2 = lp;
		return(1);

	case CCL:
		if (cclass(ep, *lp++, 1)) {
			ep += *ep;
			continue;
		}
		return(0);

	case NCCL:
		if (cclass(ep, *lp++, 0)) {
			ep += *ep;
			continue;
		}
		return(0);

	case CBRA:
		braslist[(uchar)*ep++] = lp;
		continue;

	case CKET:
		braelist[(uchar)*ep++] = lp;
		continue;

	case CDOT|STAR:
		curlp = lp;
		while (*lp++);
		goto star;

	case CCHR|STAR:
		curlp = lp;
		while (*lp++ == *ep);
		ep++;
		goto star;

	case CCL|STAR:
	case NCCL|STAR:
		curlp = lp;
		while (cclass(ep, *lp++, ep[-1]==(CCL|STAR)));
		ep += *ep;
		goto star;

	star:
		do {
			lp--;
			if (lp==locs)
				break;
			if (advance(lp, ep))
				return(1);
		} while (lp > curlp);
		return(0);

	default:
		error;
	}
}

static int cclass(char *aset, int ac, int af)
{
	register char *set, c;
	int n;

	set = aset;
	if ((c = ac) == 0)
		return(0);
	n = *set++;
	while (--n)
		if (*set++ == c)
			return(af);
	return(!af);
}

static void
putd(void)
{
  printf(1, "%d", count);
}

static void puts(const char *as)
{
	const char *sp;

	sp = as;
	col = 0;
	while (*sp)
		putc(*sp++);
	putc('\n');
}

static char line[70];
static char *linp = line;

static void putc(int ac)
{
	register char *lp;
	int c;

	lp = linp;
	c = ac;
	if (listf) {
		col++;
		if (col >= 72) {
			col = 0;
			*lp++ = '\\';
			*lp++ = '\n';
		}
		if (c=='\t') {
			c = '>';
			goto esc;
		}
		if (c=='\b') {
			c = '<';
		esc:
			*lp++ = '-';
			*lp++ = '\b';
			*lp++ = c;
			goto out;
		}
		if (c<' ' && c!= '\n') {
			*lp++ = '\\';
			*lp++ = (c>>3)+'0';
			*lp++ = (c&07)+'0';
			col += 2;
			goto out;
		}
	}
	*lp++ = c;
out:
	if(c == '\n' || lp >= &line[64]) {
		linp = line;
		write(1, line, lp-line);
		return;
	}
	linp = lp;
}

