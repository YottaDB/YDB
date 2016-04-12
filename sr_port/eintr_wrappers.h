/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#define ACCEPT_SOCKET(SOCKET, ADDR, LEN, RC)			\
{								\
	do							\
	{							\
		RC = ACCEPT(SOCKET, ADDR, LEN);			\
	} while (-1 == RC && EINTR == errno);			\
}

#define CHG_OWNER(PATH, OWNER, GRP, RC)				\
{								\
	do							\
	{							\
		RC = CHOWN(PATH, OWNER, GRP);			\
	} while (-1 == RC && EINTR == errno);			\
}

#define CLOSE(FD, RC)									\
{											\
	intrpt_state_t		prev_intrpt_state;					\
											\
	do										\
	{										\
		DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);	\
		RC = close(FD);								\
		ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);	\
	} while (-1 == RC && EINTR == errno);						\
}

#define CLOSEDIR(DIR, RC)					\
{								\
	do							\
	{							\
		RC = closedir(DIR);				\
	} while (-1 == RC && EINTR == errno);			\
}

#define CONNECT_SOCKET(SOCKET, ADDR, LEN, RC)			\
	RC = gtm_connect(SOCKET, ADDR, LEN)

#define CREATE_FILE(PATHNAME, MODE, RC)				\
{								\
	do							\
	{							\
		RC = CREAT(PATHNAME, MODE);			\
	} while (-1 == RC && EINTR == errno);			\
}

#define DOREAD_A_NOINT(FD, BUF, SIZE, RC)			\
{								\
	do							\
	{							\
		RC = DOREAD_A(FD, BUF, SIZE);			\
	} while (-1 == RC && EINTR == errno);			\
}

#define DUP2(FDESC1, FDESC2, RC)				\
{								\
	do							\
	{							\
		RC = dup2(FDESC1, FDESC2);			\
	} while (-1 == RC && EINTR == errno);			\
}

#define FCLOSE(STREAM, RC)								\
{											\
	intrpt_state_t		prev_intrpt_state;					\
											\
	do										\
	{										\
		DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);	\
		RC = fclose(STREAM);							\
		ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);	\
	} while (-1 == RC && EINTR == errno);						\
}

#define FCNTL2(FDESC, ACTION, RC)				\
{								\
	do							\
	{							\
		RC = fcntl(FDESC, ACTION);			\
	} while (-1 == RC && EINTR == errno);			\
}

#define FCNTL3(FDESC, ACTION, ARG, RC)				\
{								\
	do							\
	{							\
		RC = fcntl(FDESC, ACTION, ARG);			\
	} while (-1 == RC && EINTR == errno);			\
}

#define FGETS_FILE(BUF, LEN, FP, RC)				\
{								\
	do							\
	{							\
		FGETS(BUF, LEN, FP, RC);			\
	} while (NULL == RC && !feof(FP) && ferror(FP) && EINTR == errno);	\
}

#define FSTAT_FILE(FDESC, INFO, RC)						\
{										\
	intrpt_state_t		prev_intrpt_state;				\
										\
	do									\
	{									\
		DEFER_INTERRUPTS(INTRPT_IN_FSTAT, prev_intrpt_state);		\
		RC = fstat(FDESC, INFO);					\
		ENABLE_INTERRUPTS(INTRPT_IN_FSTAT, prev_intrpt_state);		\
	} while (-1 == RC && EINTR == errno);					\
}

#define FSTATVFS_FILE(FDESC, FSINFO, RC)			\
{								\
	do							\
	{							\
		FSTATVFS(FDESC, FSINFO, RC);			\
	} while (-1 == RC && EINTR == errno);			\
}

#define FTRUNCATE(FDESC, LENGTH, RC)				\
{								\
	do							\
	{							\
		RC = ftruncate(FDESC, LENGTH);			\
	} while (-1 == RC && EINTR == errno);			\
}

#define MSGSND(MSGID, MSGP, MSGSZ, FLG, RC)			\
{								\
	do							\
	{							\
		RC = msgsnd(MSGID, MSGP, MSGSZ, FLG);		\
	} while (-1 == RC && EINTR == errno);			\
}

#define OPEN_PIPE(FDESC, RC)					\
{								\
	do							\
	{							\
		RC = pipe(FDESC);				\
	} while (-1 == RC && EINTR == errno);			\
}

#define READ_FILE(FD, BUF, SIZE, RC)				\
{								\
	do							\
	{							\
		RC = read(FD, BUF, SIZE);			\
	} while (-1 == RC && EINTR == errno);			\
}

#define RECV(SOCKET, BUF, LEN, FLAGS, RC)			\
{								\
	do							\
	{							\
		RC = (int)recv(SOCKET, BUF, (int)(LEN), FLAGS);	\
	} while (-1 == RC && EINTR == errno);			\
}

#define RECVFROM_SOCK(SOCKET, BUF, LEN, FLAGS,			\
		 ADDR, ADDR_LEN, RC)				\
{								\
	do							\
	{							\
		RC = RECVFROM(SOCKET, BUF, LEN,			\
			 FLAGS, ADDR, ADDR_LEN);		\
	} while (-1 == RC && EINTR == errno);			\
}

#define SELECT(FDS, INLIST, OUTLIST, XLIST, TIMEOUT, RC)	\
{								\
	struct timeval eintr_select_timeval;			\
	do							\
	{							\
		eintr_select_timeval = *(TIMEOUT);		\
		RC = select(FDS, INLIST, OUTLIST,		\
			XLIST, &eintr_select_timeval);		\
	} while (-1 == RC && EINTR == errno);			\
}


#define SEND(SOCKET, BUF, LEN, FLAGS, RC)			\
{								\
	do							\
	{							\
		RC = send(SOCKET, BUF, LEN, FLAGS);		\
	} while (-1 == RC && EINTR == errno);			\
}

#define SENDTO_SOCK(SOCKET, BUF, LEN, FLAGS,			\
		 ADDR, ADDR_LEN, RC)				\
{								\
	do							\
	{							\
		RC = SENDTO(SOCKET, BUF, LEN, FLAGS,		\
			 ADDR, ADDR_LEN);			\
	} while (-1 == RC && EINTR == errno);			\
}

#define STAT_FILE(PATH, INFO, RC)				\
{								\
	do							\
	{							\
		RC = Stat(PATH, INFO);				\
	} while ((uint4)-1 == RC && EINTR == errno);		\
}

#define LSTAT_FILE(PATH, INFO, RC)				\
{								\
	do							\
	{							\
		RC = LSTAT(PATH, INFO);				\
	} while ((uint4)-1 == RC && EINTR == errno);		\
}

#define TCFLUSH(FDESC, REQUEST, RC)				\
{								\
	do							\
	{							\
		RC = tcflush(FDESC, REQUEST);			\
	} while (-1 == RC && EINTR == errno);			\
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
	} while (-1 == RC && EINTR == errno);			\
	ERRNO = errno;						\
	SIGPROCMASK(SIG_SETMASK, &oldset, NULL, rc);		\
}
#endif

#define TRUNCATE_FILE(PATH, LENGTH, RC)				\
{								\
	do							\
	{							\
		RC = TRUNCATE(PATH, LENGTH);			\
	} while (-1 == RC && EINTR == errno);			\
}

#define WAIT(STATUS, RC)					\
{								\
	do							\
	{							\
		RC = wait(STATUS);				\
	} while (-1 == RC && EINTR == errno);			\
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
	} while (-1 == RC && EINTR == errno);										\
}

#define GTM_FSYNC(FD, RC)					\
{								\
	do							\
	{							\
		RC = fsync(FD);					\
	} while (-1 == RC && EINTR == errno);			\
}

#if defined(DEBUG) && defined(UNIX)
#define SYSCONF(PARM, RC)							\
{										\
	intrpt_state_t		prev_intrpt_state;				\
										\
	DEFER_INTERRUPTS(INTRPT_IN_SYSCONF, prev_intrpt_state);			\
	if (gtm_white_box_test_case_enabled					\
		&& (WBTEST_SYSCONF_WRAPPER == gtm_white_box_test_case_number))	\
	{									\
		DBGFPF((stderr, "will sleep indefinitely now\n"));		\
		while (TRUE)							\
			LONG_SLEEP(60);						\
	}									\
	RC = sysconf(PARM);							\
	ENABLE_INTERRUPTS(INTRPT_IN_SYSCONF, prev_intrpt_state);		\
}
#else
#define SYSCONF(PARM, RC)							\
{										\
	intrpt_state_t		prev_intrpt_state;				\
										\
	DEFER_INTERRUPTS(INTRPT_IN_SYSCONF, prev_intrpt_state);			\
	RC = sysconf(PARM);							\
	ENABLE_INTERRUPTS(INTRPT_IN_SYSCONF, prev_intrpt_state);		\
}
#endif

/* GTM_FREAD is an EINTR-safe versions of "fread". Retries on EINTR. Returns number of elements read in NREAD.
 * If NREAD < NELEMS, if error then copies errno into RC, if eof then sets RC to 0. Note: RC is not initialized otherwise.
 * Macro is named GTM_FREAD instead of FREAD because AIX defines a macro by the same name in fcntl.h.
 */
#define GTM_FREAD(BUFF, ELEMSIZE, NELEMS, FP, NREAD, RC)			\
{										\
	size_t		elems_to_read, elems_read;				\
	intrpt_state_t	prev_intrpt_state;					\
										\
	DEFER_INTERRUPTS(INTRPT_IN_EINTR_WRAPPERS, prev_intrpt_state);		\
	elems_to_read = NELEMS;							\
	for (;;)								\
	{									\
		elems_read = fread(BUFF, ELEMSIZE, elems_to_read, FP);		\
		assert(elems_read <= elems_to_read);				\
		elems_to_read -= elems_read;					\
		if (0 == elems_to_read)						\
			break;							\
		RC = feof(fp);							\
		if (RC)								\
		{	/* Reached EOF. No error. */				\
			RC = 0;							\
			break;							\
		}								\
		RC = ferror(fp);						\
		assert(RC);							\
		assert(errno == RC);						\
		clearerr(fp);	/* reset error set by the "fread" */		\
		/* In case of EINTR, retry "fread" */				\
		if (EINTR != RC)						\
			break;							\
	}									\
	NREAD = NELEMS - elems_to_read;						\
	ENABLE_INTERRUPTS(INTRPT_IN_EINTR_WRAPPERS, prev_intrpt_state);		\
}

/* GTM_FWRITE is an EINTR-safe versions of "fwrite". Retries on EINTR. Returns number of elements written in NWRITTEN.
 * If NWRITTEN < NELEMS, copies errno into RC. Note: RC is not initialized otherwise.
 * Macro is named GTM_FWRITE instead of FWRITE because AIX defines a macro by the same name in fcntl.h.
 */
#define GTM_FWRITE(BUFF, ELEMSIZE, NELEMS, FP, NWRITTEN, RC)			\
{										\
	size_t		elems_to_write, elems_written;				\
	intrpt_state_t	prev_intrpt_state;					\
										\
	DEFER_INTERRUPTS(INTRPT_IN_EINTR_WRAPPERS, prev_intrpt_state);		\
	elems_to_write = NELEMS;						\
	for (;;)								\
	{									\
		elems_written = fwrite(BUFF, ELEMSIZE, elems_to_write, FP);	\
		assert(elems_written <= elems_to_write);			\
		elems_to_write -= elems_written;				\
		if (0 == elems_to_write)					\
			break;							\
		assert(!feof(fp));						\
		RC = ferror(fp);						\
		assert(RC);							\
		assert(errno == RC);						\
		clearerr(fp);	/* reset error set by the "fwrite" */		\
		/* In case of EINTR, retry "fwrite" */				\
		if (EINTR != RC)						\
			break;							\
	}									\
	NWRITTEN = NELEMS - elems_to_write;					\
	ENABLE_INTERRUPTS(INTRPT_IN_EINTR_WRAPPERS, prev_intrpt_state);		\
}

#endif
