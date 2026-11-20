/* $XTermId: xtermcfg.hin,v 1.238 2026/01/22 00:17:20 tom Exp $ */

/*
 * Copyright 1997-2025,2026 by Thomas E. Dickey
 *
 *                         All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 */

#ifndef included_xtermcfg_h
#define included_xtermcfg_h 1
#define USE_POSIX_TERMIOS 1
#define USE_SYSV_PGRP 1

/* This is a template for <xtermcfg.h> */

/*
 * There are no configure options for these features:
 * ALLOWLOGFILECHANGES
 * ALLOWLOGFILEONOFF
 * CANT_OPEN_DEV_TTY
 * DEBUG* (any debug-option)
 * DUMP_* (mostly in the ReGIS/SIXEL code)
 * HAS_LTCHARS
 * PUCC_PTYD
 * USE_LOGIN_DASH_P
 * USE_X11TERM
 */

/* ALLOWLOGFILEEXEC disabled */
/* ALLOWLOGGING disabled */
#define CC_HAS_PROTOS 1
/* CSRG_BASED disabled */
/* DECL_ERRNO disabled */
/* DEFDELETE_DEL disabled */
/* DEF_ALT_SENDS_ESC disabled */
/* DEF_BACKARO_BS disabled */
/* DEF_BACKARO_ERASE disabled */
/* DEF_INITIAL_ERASE disabled */
/* DEF_META_SENDS_ESC disabled */
#define DFT_COLORMODE True
#define DFT_DECID "100"
#define DFT_TERMTYPE "xterm"
/* DISABLE_SETGID disabled */
/* DISABLE_SETUID disabled */
/* HAVE_CLOCK_GETTIME disabled */
/* HAVE_ENDUSERSHELL disabled */
#define HAVE_GETHOSTNAME 1
/* HAVE_GETLOGIN disabled */
#define HAVE_GETTIMEOFDAY 1
/* HAVE_GETUSERSHELL disabled */
/* HAVE_GRANTPT disabled */
/* HAVE_GRANTPT_PTY_ISATTY disabled */
/* HAVE_INITGROUPS disabled */
/* HAVE_LANGINFO_CODESET disabled */
/* HAVE_LASTLOG_H disabled */
/* HAVE_LIBUTIL_H disabled */
/* HAVE_LIBXPM disabled */
/* HAVE_LIB_NEXTAW disabled */
/* HAVE_LIB_PCRE disabled */
/* HAVE_LIB_PCRE2 disabled */
#define HAVE_LIB_XAW 1
/* HAVE_LIB_XAW3D disabled */
/* HAVE_LIB_XAW3DXFT disabled */
/* HAVE_LIB_XAWPLUS disabled */
/* HAVE_LIB_XCURSOR disabled */
/* HAVE_MKDTEMP disabled */
/* HAVE_MKSTEMP disabled */
/* HAVE_NCURSES_CURSES_H disabled */
/* HAVE_NCURSES_TERM_H disabled */
/* HAVE_PATHS_H disabled */
/* HAVE_PCRE2POSIX_H disabled */
/* HAVE_PCRE2REGCOMP disabled */
/* HAVE_PCREPOSIX_H disabled */
/* HAVE_POSIX_OPENPT disabled */
/* HAVE_POSIX_SAVED_IDS disabled */
/* HAVE_PTSNAME disabled */
#define HAVE_PTY_H 1
#define HAVE_PUTENV 1
/* HAVE_SCHED_YIELD disabled */
/* HAVE_SETITIMER disabled */
#define HAVE_SETPGID 1
#define HAVE_SETSID 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
/* HAVE_STDNORETURN_H disabled */
#define HAVE_STRFTIME 1
/* HAVE_STROPTS_H disabled */
#define HAVE_SYS_PARAM_H 1
/* HAVE_SYS_PTEM_H disabled */
#define HAVE_SYS_TIME_H 1
/* HAVE_SYS_TTYDEFAULTS_H disabled */
#define HAVE_SYS_WAIT_H 1
#define HAVE_TCGETATTR 1
/* HAVE_TERMCAP_H disabled */
#define HAVE_TERMIOS_H 1
/* HAVE_TERMIO_C_ISPEED disabled */
/* HAVE_TERM_H disabled */
/* HAVE_TIGETSTR disabled */
#define HAVE_UNISTD_H 1
#define HAVE_UNSETENV 1
/* HAVE_USE_EXTENDED_NAMES disabled */
/* HAVE_UTIL_H disabled */
/* HAVE_UTMP disabled */
/* HAVE_UTMP_UT_HOST disabled */
/* HAVE_UTMP_UT_SESSION disabled */
/* HAVE_UTMP_UT_SYSLEN disabled */
/* HAVE_UTMP_UT_XSTATUS disabled */
/* HAVE_UTMP_UT_XTIME disabled */
#define HAVE_WAITPID 1
/* HAVE_WCHAR_H disabled */
/* HAVE_WCSWIDTH disabled */
/* HAVE_WCWIDTH disabled */
/* HAVE_X11_DECKEYSYM_H disabled */
/* HAVE_X11_EXTENSIONS_XDBE_H disabled */
/* HAVE_X11_EXTENSIONS_XINERAMA_H disabled */
/* HAVE_X11_EXTENSIONS_XKB_H disabled */
/* HAVE_X11_SUNKEYSYM_H disabled */
/* HAVE_X11_TRANSLATEI_H disabled */
/* HAVE_X11_XF86KEYSYM_H disabled */
/* HAVE_X11_XKBLIB_H disabled */
#define HAVE_X11_XPOLL_H 1
/* HAVE_XDBESWAPBUFFERS disabled */
/* HAVE_XFTDRAWSETCLIP disabled */
/* HAVE_XFTDRAWSETCLIPRECTANGLES disabled */
/* HAVE_XKBKEYCODETOKEYSYM disabled */
/* HAVE_XKBQUERYEXTENSION disabled */
/* HAVE_XKB_BELL_EXT disabled */
/* LUIT_PATH disabled */
/* NO_ACTIVE_ICON disabled */
/* NO_LEAKS disabled */
#define OPT_256_COLORS 1
/* OPT_88_COLORS disabled */
#define OPT_AIX_COLORS 1
/* OPT_BLINK_CURS disabled */
/* OPT_BLINK_TEXT disabled */
/* OPT_BLOCK_SELECT disabled */
#define OPT_BOX_CHARS 1
/* OPT_BROKEN_OSC disabled */
/* OPT_BROKEN_ST disabled */
/* OPT_BUILTIN_XPMS disabled */
/* OPT_C1_PRINT disabled */
/* OPT_COLOR_CLASS disabled */
/* OPT_DABBREV disabled */
/* OPT_DEC_CHRSET disabled */
/* OPT_DEC_LOCATOR disabled */
/* OPT_DEC_RECTOPS disabled */
/* OPT_DIRECT_COLOR disabled */
/* OPT_DOUBLE_BUFFER disabled */
/* OPT_EXEC_SELECTION disabled */
/* OPT_EXEC_XTERM disabled */
/* OPT_GRAPHICS disabled */
#define OPT_HIGHLIGHT_COLOR 1
/* OPT_HP_FUNC_KEYS disabled */
/* OPT_I18N_SUPPORT disabled */
#define OPT_INITIAL_ERASE 1
/* OPT_INPUT_METHOD disabled */
#define OPT_ISO_COLORS 1
/* OPT_LOAD_VTFONTS disabled */
/* OPT_LUIT_PROG disabled */
/* OPT_MAXIMIZE disabled */
/* OPT_MINI_LUIT disabled */
#define OPT_NUM_LOCK 1
/* OPT_PASTE64 disabled */
/* OPT_PC_COLORS disabled */
/* OPT_PRINT_GRAPHICS disabled */
#define OPT_PTY_HANDSHAKE 0
/* OPT_READLINE disabled */
/* OPT_REGIS_GRAPHICS disabled */
/* OPT_RESIZE_ADJUST disabled */
/* OPT_SAME_NAME disabled */
/* OPT_SCO_FUNC_KEYS disabled */
/* OPT_SCREEN_DUMPS disabled */
#define OPT_SELECTION_OPS 1
/* OPT_SELECT_REGEX disabled */
/* OPT_SESSION_MGT disabled */
/* OPT_SIXEL_GRAPHICS disabled */
/* OPT_STATUS_LINE disabled */
/* OPT_SUN_FUNC_KEYS disabled */
/* OPT_TCAP_FKEYS disabled */
/* OPT_TCAP_QUERY disabled */
/* OPT_TEK4014 disabled */
/* OPT_TOOLBAR disabled */
/* OPT_VT52_MODE disabled */
/* OPT_WIDER_ICHAR disabled */
/* OPT_WIDE_ATTRS disabled */
/* OPT_WIDE_CHARS disabled */
/* EXP_XMC_GLITCH disabled */
/* OPT_ZICONBEEP disabled */
/* OWN_TERMINFO_DIR disabled */
/* OWN_TERMINFO_ENV disabled */
/* PROCFS_ROOT disabled */
/* SCROLLBAR_RIGHT disabled */
/* SIG_ATOMIC_T disabled */
/* STDC_NORETURN disabled */
/* SVR4 disabled */
/* SYSV disabled */
/* TIME_WITH_SYS_TIME disabled */
/* TTY_GROUP_NAME disabled */
/* USE_LASTLOG disabled */
#define USE_POSIX_WAIT 1
/* USE_STRUCT_LASTLOG disabled */
/* USE_SYSV_UTMP disabled */
#define USE_SYS_SELECT_H 1
/* USE_TERMCAP disabled */
/* USE_TERMINFO disabled */
/* USE_TTY_GROUP disabled */
/* USE_UTEMPTER disabled */
/* USE_UTMP_SETGID disabled */
/* UTMPX_FOR_UTMP disabled */
/* XRENDERFONT disabled */
/* XTERM_ERASE disabled */
/* cc_t disabled */
/* gid_t disabled */
/* inline disabled */
/* mode_t disabled */
/* nfds_t disabled */
/* off_t disabled */
/* pid_t disabled */
/* time_t disabled */
/* uid_t disabled */
/* ut_name disabled */
/* ut_xstatus disabled */
/* ut_xtime disabled */

/*
 * Ifdef'd to make it simple to override.
 */
#ifndef OPT_TRACE
/* OPT_TRACE disabled */
/* OPT_TRACE_FLAGS disabled */
#endif

/*
 * g++ support for __attribute__() is haphazard.
 */
#ifndef __cplusplus
/* GCC_PRINTF disabled */
/* GCC_PRINTFLIKE disabled */
#define GCC_NORETURN __attribute__((noreturn))
#define GCC_UNUSED __attribute__((unused))
#endif

#ifndef HAVE_X11_XPOLL_H
#define NO_XPOLL_H	/* X11R6.1 & up use Xpoll.h for select() definitions */
#endif

/* vile:cmode
 */
#define OPT_SESSION_MGT 0
#define OPT_I18N_SUPPORT 0
#define OPT_INPUT_METHOD 0
#define OPT_TEK4014 0
#define OPT_SCREEN_DUMPS 0

#endif /* included_xtermcfg_h */
