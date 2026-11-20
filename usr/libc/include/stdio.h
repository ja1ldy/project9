/* Project9 contributions are under ISC. See LICENSE for scope and upstream terms. */

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)stdio.h	8.5 (Berkeley) 4/29/95
 */

#ifndef P9_LIBC_STDIO_H
#define P9_LIBC_STDIO_H
#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>
#include <locale.h>
#include <wchar.h>
#define EOF (-1)
#define BUFSIZ 1024
#define FOPEN_MAX 8
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
/* stdio buffers */
struct __sbuf {
	unsigned char *_base;
	int	_size;
};

/*
 * stdio state variables.
 *
 * The following always hold:
 *
 *	if (_flags&(__SLBF|__SWR)) == (__SLBF|__SWR),
 *		_lbfsize is -_bf._size, else _lbfsize is 0
 *	if _flags&__SRD, _w is 0
 *	if _flags&__SWR, _r is 0
 *
 * This ensures that the getc and putc macros (or inline functions) never
 * try to write or read from a file that is in `read' or `write' mode.
 * (Moreover, they can, and do, automatically switch from read mode to
 * write mode, and back, on "r+" and "w+" files.)
 *
 * _lbfsize is used only to make the inline line-buffered output stream
 * code as compact as possible.
 *
 * _ub (via _ext and struct __sfileext), _up, and _ur are used when ungetc()
 * pushes back more characters than fit in the current _bf, or when ungetc()
 * pushes back a character that does not match the previous one in _bf.
 * When this happens, _ext._base becomes non-nil (i.e., a stream has ungetc()
 * data iff _ub._base != NULL) and _up and _ur save the current values of _p
 * and _r.
 */
typedef	struct __sFILE {
	unsigned char *_p;	/* current position in (some) buffer */
	int	_r;		/* read space left for getc() */
	int	_w;		/* write space left for putc() */
	unsigned short _flags;	/* flags, below; this FILE is free if 0 */
	short	_file;		/* fileno, if Unix descriptor, else -1 */
	struct	__sbuf _bf;	/* the buffer (at least 1 byte, if !NULL) */
	int	_lbfsize;	/* 0 or -_bf._size, for inline putc */

	/* operations */
	void	*_cookie;	/* cookie passed to io functions */
	int	(*_close)(void *);
	ssize_t	(*_read) (void *, void *, size_t);
	off_t	(*_seek) (void *, off_t, int);
	ssize_t	(*_write)(void *, const void *, size_t);

	/* file extension */
	struct	__sbuf _ext;

	/* separate buffer for long sequences of ungetc() */
	unsigned char *_up;	/* saved _p when _p is doing ungetc data */
	int	_ur;		/* saved _r when _r is counting ungetc data */

	/* tricks to meet minimum requirements even when malloc() fails */
	unsigned char _ubuf[3];	/* guarantee an ungetc() buffer */
	unsigned char _nbuf[1];	/* guarantee a getc() buffer */

	int	(*_flush)(void *);
	/* Formerly used by fgetln/fgetwln; kept for binary compatibility */
	char	_lb_unused[sizeof(struct __sbuf) - sizeof(int (*)(void *))];

	/* Unix stdio files get aligned to block boundaries on fseek() */
	int	_blksize;	/* stat.st_blksize (may be != _bf._size) */
	off_t	_offset;	/* current lseek offset */
} FILE;

__BEGIN_DECLS
extern FILE __sF[3];
__END_DECLS

#define	__SLBF	0x0001		/* line buffered */
#define	__SNBF	0x0002		/* unbuffered */
#define	__SRD	0x0004		/* OK to read */
#define	__SWR	0x0008		/* OK to write */
	/* RD and WR are never simultaneously asserted */
#define	__SRW	0x0010		/* open for reading & writing */
#define	__SEOF	0x0020		/* found EOF */
#define	__SERR	0x0040		/* found error */
#define	__SMBF	0x0080		/* _buf is from malloc */
#define	__SAPP	0x0100		/* fdopen()ed in append mode */
#define	__SSTR	0x0200		/* this is an sprintf/snprintf string */
#define	__SOPT	0x0400		/* do fseek() optimization */
#define	__SNPT	0x0800		/* do not do fseek() optimization */
#define	__SOFF	0x1000		/* set iff _offset is in fact correct */
#define	__SMOD	0x2000		/* true => fgetln modified _p text */
#define	__SALC	0x4000		/* allocate string space dynamically */


#define stdin (&__sF[0])
#define stdout (&__sF[1])
#define stderr (&__sF[2])
#define __sfeof(p) (((p)->_flags & __SEOF) != 0)
#define __sferror(p) (((p)->_flags & __SERR) != 0)
#define __sclearerr(p) ((p)->_flags &= ~(__SERR | __SEOF))
#define __sfileno(p) ((p)->_file)
#define __sgetc(p) (--(p)->_r < 0 ? __srget(p) : (int)*(p)->_p++)
int __srget(FILE *);
int __swbuf(int, FILE *);
static inline int __sputc(int c, FILE *p)
{
  unsigned char b = c;
  if(--p->_w >= 0 || (p->_w >= p->_lbfsize && b != '\n'))
    return *p->_p++ = b;
  return __swbuf(c, p);
}

FILE *fopen(const char *, const char *), *fdopen(int, const char *);
int fclose(FILE *), fflush(FILE *), fseek(FILE *, long, int);
long ftell(FILE *);
int fseeko(FILE *, off_t, int);
off_t ftello(FILE *);
size_t fread(void *, size_t, size_t, FILE *), fwrite(const void *, size_t, size_t, FILE *);
char *fgets(char *, int, FILE *);
int feof(FILE *), ferror(FILE *), fileno(FILE *);
void clearerr(FILE *);
int fgetc(FILE *), fputc(int, FILE *), fputs(const char *, FILE *);
int puts(const char *), putchar(int), getchar(void);
#define getc(f) fgetc(f)
#define putc(c,f) fputc(c,f)
int printf(const char *, ...) __asm__("p9_printf");
int fprintf(FILE *, const char *, ...), vfprintf(FILE *, const char *, va_list);
int sprintf(char *, const char *, ...), snprintf(char *, size_t, const char *, ...);
int vsprintf(char *, const char *, va_list), vsnprintf(char *, size_t, const char *, va_list);
int sscanf(const char *, const char *, ...);
int rename(const char *, const char *) __asm__("p9_rename");
void perror(const char *);
#endif
