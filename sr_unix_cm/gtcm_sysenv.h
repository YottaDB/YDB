/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *  gtcm_sysenv.h ---
 *
 *	Include file specific to various systems.
 *
 *  $Header:$
 *
 */

#ifndef GTCM_SYSENV_H
#define GTCM_SYSENV_H

#ifdef TANDEM

#define OMI
/*  system interface */

#ifndef FILE_TCP
#define BSD_TCP
#endif /* !defined(FILE_TCP) */

#define BSD_LOG
#define SYSV_MFD
#define BIG_END
/*  types */
typedef int	 omi_fd;
#define INV_FD -1
#define INV_FD_P(FD) ((FD) < 0)

/*  includes */
#ifdef _SYSTYPE_SVR4
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <syslog.h>
#else
#include <bsd/sys/types.h>
#include <bsd/sys/socket.h>
#include <bsd/netinet/in.h>
#include <bsd/netdb.h>
#include <bsd/sys/time.h>
#include <bsd/syslog.h>
#endif /* defined(_SYSTYPE_SVR4) */

/* defined(TANDEM) */

#elif defined(SEQUOIA)

#define OMI
/*  system interface */

#ifndef FILE_TCP
#define BSD_TCP
#endif /* !defined(FILE_TCP) */

#define NEED_FD_SET
#define SYSV_MFD
#define BIG_END
/*  types */
typedef int	 omi_fd;
#define INV_FD -1
#define INV_FD_P(FD) ((FD) < 0)
/*  includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

/* defined(SEQUOIA) */

#elif defined(MOTOROLA)

#define OMI
#define SYSV_TCP	"/dev/tcpip"
#define SYSV_MFD
#include <netinet/types.h>
#include <sys/tpiaddr.h>

/* defined(MOTOROLA) */

#elif defined(VMS)

#define OMI
/*  ... */
/*  types */
typedef unsigned short	 omi_fd;
#define INV_FD 0
#define INV_FD_P(FD) ((FD) == 0)

/* defined(VAX) */

#elif defined(SCO)
#define OMI

/*  system interface */
#ifndef FILE_TCP
#define BSD_TCP
#endif /* !defined(FILE_TCP) */

#define BSD_LOG
#define SYSV_MFD
#define LTL_END
/*  types */
typedef int	 omi_fd;
#define INV_FD -1
#define INV_FD_P(FD) ((FD) < 0)

/*  includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <syslog.h>

/* defined(SCO) */

#elif defined(SUNOS)

#define OMI

/*  system interface */
#ifndef FILE_TCP
#define BSD_TCP
#endif /* !defined(FILE_TCP) */

#define BSD_LOG
#define BSD_MFD
#define BIG_END
/*  types */
typedef int	 omi_fd;
#define INV_FD -1
#define INV_FD_P(FD) ((FD) < 0)
/*  includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <syslog.h>

/* defined(SUNOS) */

#elif (defined(__hpux) || defined(__linux__) || defined(__osf__)) || defined(__MVS__)

#define OMI

/*  system interface */
#ifndef FILE_TCP
#define BSD_TCP
#endif /* !defined(FILE_TCP) */

#define BSD_LOG
#define BSD_MFD

#if (defined(__linux__) || defined(__osf__)) && !defined(__s390__)
#define LTL_END
#else /* defined(__hpux__)  or Linux390  or zOS */
#define BIG_END
#endif

/*  types */
typedef int	 omi_fd;
#define INV_FD -1
#define INV_FD_P(FD) ((FD) < 0)
/*  includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <syslog.h>

/* defined(__hpux || __linux__ || __osf__ || __MVS__) */

#elif defined(_AIX)

#define OMI

/*  system interface */
#ifndef FILE_TCP
#define BSD_TCP
#endif /* !defined(FILE_TCP) */

#define BSD_LOG
/* #define NEED_FD_SET */
#define BSD_MFD
#define BIG_END

/*  types */
typedef int	 omi_fd;
#define INV_FD -1
#define INV_FD_P(FD) ((FD) < 0)

/*  includes */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <syslog.h>

/* defined(_AIX) */

#else
/*  Unknown/unspecified system.  Should NOT compile */
#endif

#ifdef BSD_TCP
#define NET_TCP
#else /* defined(BSD_TCP) */
#ifdef SYSV_TCP
#define NET_TCP
#endif /* defined(SYSV_TCP) */
#endif /* defined(BSD_TCP) */
#ifdef NEED_FD_SET
/*
 *  from BSD 4.3 (could grab a copy from CCKIM for 4.2)
 */
#ifndef NBBY
#define	NBBY	8		/* number of bits in a byte */
#endif
/*
 * Select uses bit masks of file descriptors in longs.
 * These macros manipulate such bit fields (the filesystem macros use chars).
 * FD_SETSIZE may be defined by the user, but the default here
 * should be >= NOFILE (param.h).
 */
#ifndef	FD_SETSIZE
#define	FD_SETSIZE	256
#endif

typedef uint4	fd_mask;
#define NFDBITS	(sizeof(fd_mask) * NBBY)	/* bits per mask */
#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif

typedef	struct fd_set {
	fd_mask	fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} fd_set;

#define	FD_SET(n, p)	(((fd_set)(p))->fds_bits[(n)/NFDBITS] |= (1 << ((n) % NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/NFDBITS] &= ~(1 << ((n) % NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/NFDBITS] & (1 << ((n) % NFDBITS)))
#define FD_ZERO(p)	memset((char *)(p), '\0', sizeof(*(p)))
#undef NEED_FD_SET
#endif /* defined(NEED_FD_SET) */

#endif /* !defined(GTCM_SYSENV_H) */
