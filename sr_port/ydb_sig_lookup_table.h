/****************************************************************
 *								*
 * Copyright (c) 2019-2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Table to define signal names (arranged alphabetically) used in signal name lookup. Note
 * only the most common (i.e. not-Linux-only) signals are defined here - i.e. those defined in /usr/include/asm-generic/signal.h.
 * Even though N_SIG on Linux as of this writing (Ubuntu 18.04.2) is 64, we only have names for the first 31 signals though
 * there are multiple names for some signals that may be non-Linux (which can be ifdef'd out on those platforms if found).
 *
 * NOTE ** This table should be in NAME sorted order as it is searched with a binary search. In addition, each name MUST start
 * with "SIG".
 */

/*         SigName      Value		   Linux value */
YDBSIGNAL("SIGABRT",	SIGABRT)	/* 6 - (same as SIGIOT) */
YDBSIGNAL("SIGALRM",	SIGALRM)	/* 14 */
YDBSIGNAL("SIGBUS",	SIGBUS)		/* 7 */
YDBSIGNAL("SIGCHLD",	SIGCHLD)	/* 17 (deprecated name) */
YDBSIGNAL("SIGCLD",	SIGCHLD)	/* 17 (newer name) */
YDBSIGNAL("SIGCONT",	SIGCONT)	/* 18 */
YDBSIGNAL("SIGFPE",	SIGFPE)		/* 8 */
YDBSIGNAL("SIGHUP",	SIGHUP)		/* 1 */
YDBSIGNAL("SIGILL",	SIGILL)		/* 4 */
YDBSIGNAL("SIGINT",	SIGINT)		/* 2 */
YDBSIGNAL("SIGIO",	SIGIO)		/* 29 - (same as SIGPOLL) */
YDBSIGNAL("SIGIOT",	SIGIOT)		/* 6 - (same as SIGABRT) */
YDBSIGNAL("SIGKILL",	SIGKILL)	/* 9  */
YDBSIGNAL("SIGPIPE",	SIGPIPE)	/* 13 */
YDBSIGNAL("SIGPOLL",	SIGIO)		/* 29 - (same as SIGIO) */
YDBSIGNAL("SIGPROF",	SIGPROF)	/* 27 */
YDBSIGNAL("SIGPWR",	SIGPWR)		/* 30 */
YDBSIGNAL("SIGQUIT",	SIGQUIT)	/* 3 */
YDBSIGNAL("SIGSEGV",	SIGSEGV)	/* 11 */
YDBSIGNAL("SIGSTKFLT",	SIGSTKFLT)	/* 16 (obsolete) */
YDBSIGNAL("SIGSTOP",	SIGSTOP)	/* 19 */
YDBSIGNAL("SIGSYS",	SIGSYS)		/* 31 */
YDBSIGNAL("SIGTERM",	SIGTERM)	/* 15 */
YDBSIGNAL("SIGTRAP",	SIGTRAP)	/* 5 */
YDBSIGNAL("SIGTSTP",	SIGTSTP)	/* 20 */
YDBSIGNAL("SIGTTIN",	SIGTTIN)	/* 21 */
YDBSIGNAL("SIGTTOU",	SIGTTOU)	/* 22 */
YDBSIGNAL("SIGURG",	SIGURG)		/* 23 */
YDBSIGNAL("SIGUSR1",	SIGUSR1)	/* 10 */
YDBSIGNAL("SIGUSR2",	SIGUSR2)	/* 12 */
YDBSIGNAL("SIGVTALRM",	SIGVTALRM)	/* 26 */
YDBSIGNAL("SIGWINCH",	SIGWINCH)	/* 28 */
YDBSIGNAL("SIGXCPU",	SIGXCPU)	/* 24 */
YDBSIGNAL("SIGXFSZ",	SIGXFSZ)	/* 25 */
