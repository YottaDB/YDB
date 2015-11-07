/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Define macros to do system calls and restart as appropriate
 *
 * FCNTL, FCNTL3	Loop until fcntl call succeeds or fails with other than EINTR.
 * TCFLUSH		Loop until tcflush call succeeds or fails with other than EINTR.
 * Tcsetattr		Loop until tcsetattr call succeeds or fails with other than EINTR.
 */

#ifndef EINTR_WRP_Included
#define EINTR_WRP_Included

#include <sys/types.h>
#include <errno.h>
#include "have_crit.h"
#include "gt_timer.h"
#if defined(DEBUG) && defined(UNIX)
#include "io.h"
#include "gtm_stdio.h"
#include "wcs_sleep.h"
#include "deferred_signal_handler.h"
#include "wbox_test_init.h"
#endif

#define ACCEPT_SOCKET(SOCKET, ADDR, LEN, RC)		\
{							\
	do						\
	{						\
		RC = ACCEPT(SOCKET, ADDR, LEN);		\
	} while(-1 == RC && EINTR == errno);		\
}

#define CHG_OWNER(PATH, OWNER, GRP, RC)			\
{							\
	do						\
	{						\
		RC = CHOWN(PATH, OWNER, GRP);		\
	} while(-1 == RC && EINTR == errno);		\
}

#define CLOSEDIR(DIR, RC)				\
{							\
	do						\
	{						\
		RC = closedir(DIR);			\
	} while(-1 == RC && EINTR == errno);		\
}

#define CONNECT_SOCKET(SOCKET, ADDR, LEN, RC)		\
{							\
	do						\
	{						\
		RC = CONNECT(SOCKET, ADDR, LEN);	\
	} while(-1 == RC && EINTR == errno);		\
}

#define CREATE_FILE(PATHNAME, MODE, RC)			\
{							\
	do						\
	{						\
		RC = CREAT(PATHNAME, MODE);		\
	} while(-1 == RC && EINTR == errno);		\
}

#define DOREAD_A_NOINT(FD, BUF, SIZE, RC)		\
{							\
	do						\
	{						\
		RC = DOREAD_A(FD, BUF, SIZE);		\
	} while(-1 == RC && EINTR == errno);		\
}

#define DUP2(FDESC1, FDESC2, RC)			\
{							\
	do						\
	{						\
		RC = dup2(FDESC1, FDESC2);		\
	} while(-1 == RC && EINTR == errno);		\
}

#define FCLOSE(STREAM, RC)				\
{							\
	do						\
	{						\
		RC = fclose(STREAM);			\
	} while(-1 == RC && EINTR == errno);		\
}

#define FCNTL2(FDESC, ACTION, RC)			\
{							\
	do						\
	{						\
		RC = fcntl(FDESC, ACTION);		\
	} while(-1 == RC && EINTR == errno);		\
}

#define FCNTL3(FDESC, ACTION, ARG, RC)			\
{							\
	do						\
	{						\
		RC = fcntl(FDESC, ACTION, ARG);		\
	} while(-1 == RC && EINTR == errno);		\
}

#define FGETS_FILE(BUF, LEN, FP, RC)			\
{							\
	do						\
	{						\
		FGETS(BUF, LEN, FP, RC);		\
	} while(NULL == RC && !feof(FP) && ferror(FP) && EINTR == errno);	\
}

#define FSTAT_FILE(FDESC, INFO, RC)			\
{							\
	do						\
	{						\
		DEFER_INTERRUPTS(INTRPT_IN_FSTAT);	\
		RC = fstat(FDESC, INFO);		\
		ENABLE_INTERRUPTS(INTRPT_IN_FSTAT);	\
	} while(-1 == RC && EINTR == errno);		\
}

#define FSTATVFS_FILE(FDESC, FSINFO, RC)		\
{							\
	do						\
	{						\
		FSTATVFS(FDESC, FSINFO, RC);		\
	} while(-1 == RC && EINTR == errno);		\
}

#define FTRUNCATE(FDESC, LENGTH, RC)			\
{							\
	do						\
	{						\
		RC = ftruncate(FDESC, LENGTH);		\
	} while(-1 == RC && EINTR == errno);		\
}

#define MSGSND(MSGID, MSGP, MSGSZ, FLG, RC)		\
{							\
	do						\
	{						\
		RC = msgsnd(MSGID, MSGP, MSGSZ, FLG);	\
	} while(-1 == RC && EINTR == errno);		\
}

#define OPEN_PIPE(FDESC, RC)				\
{							\
	do						\
	{						\
		RC = pipe(FDESC);			\
	} while(-1 == RC && EINTR == errno);		\
}

#define READ_FILE(FD, BUF, SIZE, RC)			\
{							\
	do						\
	{						\
		RC = read(FD, BUF, SIZE);		\
	} while(-1 == RC && EINTR == errno);		\
}

#define RECVFROM_SOCK(SOCKET, BUF, LEN, FLAGS,		\
		 ADDR, ADDR_LEN, RC)			\
{							\
	do						\
	{						\
		RC = RECVFROM(SOCKET, BUF, LEN,		\
			 FLAGS, ADDR, ADDR_LEN);	\
	} while(-1 == RC && EINTR == errno);		\
}

#define SELECT(FDS, INLIST, OUTLIST, XLIST,		\
		 TIMEOUT, RC)				\
{							\
	struct timeval eintr_select_timeval;		\
	do						\
	{						\
		eintr_select_timeval = *(TIMEOUT);	\
		RC = select(FDS, INLIST, OUTLIST,	\
			XLIST, &eintr_select_timeval);	\
	} while(-1 == RC && EINTR == errno);		\
}


#define SEND(SOCKET, BUF, LEN, FLAGS, RC)		\
{							\
	do						\
	{						\
		RC = send(SOCKET, BUF, LEN, FLAGS);	\
	} while(-1 == RC && EINTR == errno);		\
}

#define SENDTO_SOCK(SOCKET, BUF, LEN, FLAGS,		\
		 ADDR, ADDR_LEN, RC)			\
{							\
	do						\
	{						\
		RC = SENDTO(SOCKET, BUF, LEN, FLAGS,	\
			 ADDR, ADDR_LEN);		\
	} while(-1 == RC && EINTR == errno);		\
}

#define STAT_FILE(PATH, INFO, RC)			\
{							\
	do						\
	{						\
		RC = Stat(PATH, INFO);			\
	} while((uint4)-1 == RC && EINTR == errno);	\
}

#define TCFLUSH(FDESC, REQUEST, RC)			\
{							\
	do						\
	{						\
		RC = tcflush(FDESC, REQUEST);		\
	} while(-1 == RC && EINTR == errno);		\
}

#if defined(UNIX)
#define Tcsetattr(FDESC, WHEN, TERMPTR, RC, ERRNO)		\
{								\
	GBLREF sigset_t block_ttinout;				\
	sigset_t oldset;					\
	int rc;							\
	SIGPROCMASK(SIG_BLOCK, &block_ttinout, &oldset, rc);	\
	do							\
	{							\
		RC = tcsetattr(FDESC, WHEN, TERMPTR);		\
	} while(-1 == RC && EINTR == errno);			\
	ERRNO = errno;						\
	SIGPROCMASK(SIG_SETMASK, &oldset, NULL, rc);		\
}
#endif

#define TRUNCATE_FILE(PATH, LENGTH, RC)			\
{							\
	do						\
	{						\
		RC = TRUNCATE(PATH, LENGTH);		\
	} while(-1 == RC && EINTR == errno);		\
}

#define WAIT(STATUS, RC)				\
{							\
	do						\
	{						\
		RC = wait(STATUS);			\
	} while(-1 == RC && EINTR == errno);		\
}

#define WAITPID(PID, STATUS, OPTS, RC)											\
{															\
	/* Ensure that the incoming PID is non-zero. We currently don't know of any places where we want to invoke	\
	 * waitpid with child PID being 0 as that would block us till any of the child spawned by this parent process	\
	 * changes its state unless invoked with WNOHANG bit set. Make sure not waiting on current pid			\
	 */														\
	assert(0 != PID);												\
	assert(getpid() != PID);											\
	do														\
	{														\
		RC = waitpid(PID, STATUS, OPTS);									\
	} while(-1 == RC && EINTR == errno);										\
}

#define GTM_FSYNC(FD, RC)				\
{							\
	do						\
	{						\
		RC = fsync(FD);				\
	} while(-1 == RC && EINTR == errno);		\
}

#define SIGPROCMASK(FUNC, NEWSET, OLDSET, RC)		\
{							\
	do						\
	{						\
	  RC = sigprocmask(FUNC, NEWSET, OLDSET);	\
	} while (-1 == RC && EINTR == errno);		\
}

#if defined(DEBUG) && defined(UNIX)
#define SYSCONF(PARM, RC)							\
{										\
	DEFER_INTERRUPTS(INTRPT_IN_SYSCONF);					\
	if (gtm_white_box_test_case_enabled					\
		&& (WBTEST_SYSCONF_WRAPPER == gtm_white_box_test_case_number))	\
	{									\
		DBGFPF((stderr, "will sleep indefinitely now\n"));		\
		while (TRUE)							\
			LONG_SLEEP(60);						\
	}									\
	RC = sysconf(PARM);							\
	ENABLE_INTERRUPTS(INTRPT_IN_SYSCONF);					\
}
#else
#define SYSCONF(PARM, RC)							\
{										\
	DEFER_INTERRUPTS(INTRPT_IN_SYSCONF);					\
	RC = sysconf(PARM);							\
	ENABLE_INTERRUPTS(INTRPT_IN_SYSCONF);					\
}
#endif

#endif
