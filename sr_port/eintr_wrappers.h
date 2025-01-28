/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "gtm_stdio.h"
#if defined(DEBUG)
#include "io.h"
#include "wcs_sleep.h"
#include "deferred_exit_handler.h"
#include "wbox_test_init.h"
#endif

/* This macro is there to handle the case a signal interrupt happened outside of system call code. This ensures any deferred
 * events (e.g. timer handler function invocations could get deferred on receipt of a SIGALRM) get handled in a timely fashion
 * (if safe to do so). Note that we do a relatively quick check of whether any signal handling was deferred (using a global
 * variable) and then invoke a function if necessary. That does the heavyweight job of actually invoking the signal handler
 * if it is safe to do so. This keeps the overhead of the below macro minimal in the caller's code path for the most common
 * case (which is no interrupted/deferred signals).
 */
#define	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL							\
{												\
	GBLREF	global_latch_t deferred_signal_handling_needed;					\
												\
	if (GLOBAL_LATCH_VALUE(&deferred_signal_handling_needed))				\
		eintr_handling_check();								\
}

/* Note: The "HANDLE_EINTR_OUTSIDE_SYSTEM_CALL" macro is invoked at the end of the do/while loop in the below macros in case
 * these macros are used inside a do/while (or for) loop in a caller function.
 */
#define ACCEPT_SOCKET(SOCKET, ADDR, LEN, RC)			\
{								\
	do							\
	{							\
		RC = ACCEPT(SOCKET, ADDR, LEN);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define CHG_OWNER(PATH, OWNER, GRP, RC)				\
{								\
	do							\
	{							\
		RC = CHOWN(PATH, OWNER, GRP);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
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
		if ((-1 != RC) || (EINTR != errno))					\
			break;								\
		eintr_handling_check();							\
	} while (TRUE);									\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;						\
}

#define CLOSEDIR(DIR, RC)					\
{								\
	do							\
	{							\
		RC = closedir(DIR);				\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define CONNECT_SOCKET(SOCKET, ADDR, LEN, RC)					\
{										\
	RC = gtm_connect(SOCKET, ADDR, LEN);					\
	/* Note: HANDLE_EINTR_OUTSIDE_SYSTEM_CALL is done inside "gtm_connect"	\
	 * so no need to do it again here.					\
	 */									\
}

#define CREATE_FILE(PATHNAME, MODE, RC)				\
{								\
	do							\
	{							\
		RC = CREAT(PATHNAME, MODE);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define DOREAD_A_NOINT(FD, BUF, SIZE, RC)			\
{								\
	do							\
	{							\
		RC = DOREAD_A(FD, BUF, SIZE);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define DUP2(FDESC1, FDESC2, RC)				\
{								\
	do							\
	{							\
		RC = dup2(FDESC1, FDESC2);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
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
		if ((-1 != RC) || (EINTR != errno))					\
			break;								\
		eintr_handling_check();							\
	} while (TRUE);									\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;						\
}

#define	FLOCK(FD, FLAGS, RC)					\
{								\
	do							\
	{							\
		RC = flock(FD, FLAGS);				\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define FCNTL2(FDESC, ACTION, RC)				\
{								\
	do							\
	{							\
		RC = fcntl(FDESC, ACTION);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define FCNTL3(FDESC, ACTION, ARG, RC)				\
{								\
	do							\
	{							\
		RC = fcntl(FDESC, ACTION, ARG);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define FGETS_FILE(BUF, LEN, FP, RC)							\
{											\
	do										\
	{										\
		FGETS(BUF, LEN, FP, RC);						\
		if ((NULL != RC) || feof(FP) || !ferror(FP) || (EINTR != errno))	\
			break;								\
		eintr_handling_check();							\
	} while (TRUE);									\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;						\
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
		if ((-1 != RC) || (EINTR != errno))				\
			break;							\
		eintr_handling_check();						\
	} while (TRUE);								\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;					\
}

#define FSTATVFS_FILE(FDESC, FSINFO, RC)			\
{								\
	do							\
	{							\
		FSTATVFS(FDESC, FSINFO, RC);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define FTRUNCATE(FDESC, LENGTH, RC)				\
{								\
	do							\
	{							\
		RC = ftruncate(FDESC, LENGTH);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

/* GTM_FREAD is an EINTR-safe versions of "fread". Retries on EINTR. Returns number of elements read in NREAD.
 * If NREAD < NELEMS, if error then copies errno into RC, if eof then sets RC to 0. Note: RC is not initialized otherwise.
 * Macro is named GTM_FREAD instead of FREAD because AIX defines a macro by the same name in fcntl.h.
 */
#define GTM_FREAD(BUFF, ELEMSIZE, NELEMS, FP, NREAD, RC)					\
MBSTART {											\
	size_t		elems_to_read, elems_read;						\
	intrpt_state_t	prev_intrpt_state;							\
												\
	DEFER_INTERRUPTS(INTRPT_IN_EINTR_WRAPPERS, prev_intrpt_state);				\
	elems_to_read = NELEMS;									\
	for (;;)										\
	{											\
		elems_read = fread(BUFF, ELEMSIZE, elems_to_read, FP);				\
		assert(elems_read <= elems_to_read);						\
		elems_to_read -= elems_read;							\
		if (0 == elems_to_read)								\
			break;									\
		RC = feof(FP);									\
		if (RC)										\
		{	/* Reached EOF. No error. */						\
			RC = 0;									\
			break;									\
		}										\
		RC = ferror(FP);								\
		assert(RC);									\
		clearerr(FP);	/* reset error set by the "fread" */				\
		/* In case of EINTR, retry "fread" */						\
		RC = errno;									\
		if (EINTR != errno)								\
			break;									\
		/* Note that the HANDLE_EINTR_OUTSIDE_SYSTEM_CALL invocation inside the		\
		 * eintr_handling_check() function below will be a no-op since we still have	\
		 * not done the ENABLE_INTERRUPTS. But that is okay since we are not waiting	\
		 * for user input indefinitely here and so this will eventually return.		\
		 * The function is still invoked here in case some other check also gets added	\
		 * to the function at a later point.						\
		 */										\
		eintr_handling_check();								\
	}											\
	NREAD = NELEMS - elems_to_read;								\
	ENABLE_INTERRUPTS(INTRPT_IN_EINTR_WRAPPERS, prev_intrpt_state);				\
	/* Note: No "HANDLE_EINTR_OUTSIDE_SYSTEM_CALL" call needed here like is done in other	\
	 * macros because the "ENABLE_INTERRUPTS" call above does the same thing		\
	 * (invoke "DEFERRED_SIGNAL_HANDLING_CHECK_TRIMMED" eventually).			\
	 */											\
} MBEND

#define GTM_FSYNC(FD, RC)					\
{								\
	do							\
	{							\
		RC = fsync(FD);					\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

/* GTM_FWRITE is an EINTR-safe versions of "fwrite". Retries on EINTR. Returns number of elements written in NWRITTEN.
 * If NWRITTEN < NELEMS, RC holds errno. Note: RC is not initialized (and should not be relied upon by caller) otherwise.
 * Macro is named GTM_FWRITE instead of FWRITE because AIX defines a macro by the same name in fcntl.h.
 */
#define GTM_FWRITE(BUFF, ELEMSIZE, NELEMS, FP, NWRITTEN, RC)			\
	RC = gtm_fwrite(BUFF, ELEMSIZE, NELEMS, FP, &(NWRITTEN));

static inline size_t gtm_fwrite(void *buff, size_t elemsize, size_t nelems, FILE *fp, size_t *nwritten)
{
	size_t		elems_to_write, elems_written, rc = 0;
	int		status;
	intrpt_state_t	prev_intrpt_state;

	DEFER_INTERRUPTS(INTRPT_IN_EINTR_WRAPPERS, prev_intrpt_state);
	elems_to_write = nelems;
	for ( ; (0 != elems_to_write) && (0 != elemsize) ; )
	{
		elems_written = fwrite(buff, elemsize, elems_to_write, fp);
		assert(elems_written <= elems_to_write);
		elems_to_write -= elems_written;
		if (0 == elems_to_write)
			break;
		/* Note: From the man pages of "feof", "ferror" and "clearerr", they do not manipulate "errno"
		 * so no need to save "errno" before those calls.
		 */
		assert(!feof(fp));
		status = ferror(fp);
		PRO_ONLY(UNUSED(status));
		assert(status);
		clearerr(fp);	/* reset error set by the "fwrite" */
		/* In case of EINTR, retry "fwrite" */
		rc = errno;
		if (EINTR != rc)
			break;
		/* See comment before eintr_handling_check() in GTM_FREAD macro for similar issue here */
		eintr_handling_check();
	}
	*nwritten = nelems - elems_to_write;
	ENABLE_INTERRUPTS(INTRPT_IN_EINTR_WRAPPERS, prev_intrpt_state);
	/* Note: No "HANDLE_EINTR_OUTSIDE_SYSTEM_CALL" call needed here like is done in other
	 * macros because the "ENABLE_INTERRUPTS" call above does the same thing
	 * (invoke "DEFERRED_SIGNAL_HANDLING_CHECK_TRIMMED" eventually).
	 */
	return rc;
}

#define LSTAT_FILE(PATH, INFO, RC)				\
{								\
	do							\
	{							\
		RC = LSTAT(PATH, INFO);				\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define MSGSND(MSGID, MSGP, MSGSZ, FLG, RC)			\
{								\
	do							\
	{							\
		RC = msgsnd(MSGID, MSGP, MSGSZ, FLG);		\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define OPENAT(PATH, FLAGS, MODE, RC)				\
{								\
	do							\
	{							\
		RC = openat(PATH, FLAGS, MODE);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define OPEN_PIPE(FDESC, RC)					\
{								\
	do							\
	{							\
		RC = pipe(FDESC);				\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

/* posix_fallocate returns zero on success, or an error
 * number on failure. Note that errno is not set. */
#define POSIX_FALLOCATE(FD, BUF, SIZE, RC)			\
{								\
	do							\
	{							\
		RC = posix_fallocate(FD, BUF, SIZE);		\
		if (EINTR != RC)				\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define READ_FILE(FD, BUF, SIZE, RC)				\
{								\
	do							\
	{							\
		RC = read(FD, BUF, SIZE);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define RECV(SOCKET, BUF, LEN, FLAGS, RC)			\
{								\
	do							\
	{							\
		RC = (int)recv(SOCKET, BUF, (int)(LEN), FLAGS);	\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define RECVFROM_SOCK(SOCKET, BUF, LEN, FLAGS,			\
		 ADDR, ADDR_LEN, RC)				\
{								\
	do							\
	{							\
		RC = RECVFROM(SOCKET, BUF, LEN,			\
			 FLAGS, ADDR, ADDR_LEN);		\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define SEND(SOCKET, BUF, LEN, FLAGS, RC)			\
{								\
	do							\
	{							\
		RC = send(SOCKET, BUF, LEN, FLAGS);		\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define SENDTO_SOCK(SOCKET, BUF, LEN, FLAGS,			\
		 ADDR, ADDR_LEN, RC)				\
{								\
	do							\
	{							\
		RC = SENDTO(SOCKET, BUF, LEN, FLAGS,		\
			 ADDR, ADDR_LEN);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define STAT_FILE(PATH, INFO, RC)				\
{								\
	do							\
	{							\
		RC = Stat(PATH, INFO);				\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#if defined(DEBUG)
#define SYSCONF(PARM, RC)									\
{												\
	intrpt_state_t		prev_intrpt_state;						\
												\
	DEFER_INTERRUPTS(INTRPT_IN_SYSCONF, prev_intrpt_state);					\
	if (ydb_white_box_test_case_enabled							\
		&& (WBTEST_SYSCONF_WRAPPER == ydb_white_box_test_case_number))			\
		{										\
			DBGFPF((stderr, "will sleep indefinitely now\n"));			\
			while (ydb_white_box_test_case_enabled)					\
				LONG_SLEEP(60);							\
		}										\
	RC = sysconf(PARM);									\
	ENABLE_INTERRUPTS(INTRPT_IN_SYSCONF, prev_intrpt_state);				\
	/* Note: No "HANDLE_EINTR_OUTSIDE_SYSTEM_CALL" call needed here like is done in other	\
	 * macros because the "ENABLE_INTERRUPTS" call above does the same thing		\
	 * (invoke "DEFERRED_SIGNAL_HANDLING_CHECK_TRIMMED" eventually).			\
	 */											\
}
#else
#define SYSCONF(PARM, RC)									\
{												\
	intrpt_state_t		prev_intrpt_state;						\
												\
	DEFER_INTERRUPTS(INTRPT_IN_SYSCONF, prev_intrpt_state);					\
	RC = sysconf(PARM);									\
	ENABLE_INTERRUPTS(INTRPT_IN_SYSCONF, prev_intrpt_state);				\
	/* Note: No "HANDLE_EINTR_OUTSIDE_SYSTEM_CALL" call needed here like is done in other	\
	 * macros because the "ENABLE_INTERRUPTS" call above does the same thing		\
	 * (invoke "DEFERRED_SIGNAL_HANDLING_CHECK_TRIMMED" eventually).			\
	 */											\
}
#endif

#define TCFLUSH(FDESC, REQUEST, RC)				\
{								\
	do							\
	{							\
		RC = tcflush(FDESC, REQUEST);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define CHANGE_TERM_TRUE TRUE
#define CHANGE_TERM_FALSE FALSE

#define Tcsetattr(FDESC, WHEN, TERMPTR, RC, ERRNO, CHANGE_TERM)						\
{													\
	GBLREF sigset_t		block_ttinout;								\
	GBLREF int		terminal_settings_changed_fd;						\
	sigset_t 		oldset;									\
	int 			rc;									\
													\
	assert(0 <= FDESC);										\
	assert((0 == terminal_settings_changed_fd) || ((FDESC + 1) == terminal_settings_changed_fd));	\
	SIGPROCMASK(SIG_BLOCK, &block_ttinout, &oldset, rc);						\
	do												\
	{												\
		RC = tcsetattr(FDESC, WHEN, TERMPTR);							\
		if ((-1 != RC) || (EINTR != errno))							\
			break;										\
		eintr_handling_check();									\
	} while (TRUE);											\
	terminal_settings_changed_fd = (CHANGE_TERM_FALSE != CHANGE_TERM) ? (FDESC + 1) : 0;		\
	ERRNO = errno;											\
	SIGPROCMASK(SIG_SETMASK, &oldset, NULL, rc);							\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;								\
}

#define TRUNCATE_FILE(PATH, LENGTH, RC)				\
{								\
	do							\
	{							\
		RC = TRUNCATE(PATH, LENGTH);			\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
}

#define WAIT(STATUS, RC)					\
{								\
	do							\
	{							\
		RC = wait(STATUS);				\
		if ((-1 != RC) || (EINTR != errno))		\
			break;					\
		eintr_handling_check();				\
	} while (TRUE);						\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;			\
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
		if ((-1 != RC) || (EINTR != errno))									\
			break;												\
		eintr_handling_check();											\
	} while (TRUE);													\
	HANDLE_EINTR_OUTSIDE_SYSTEM_CALL;										\
}

#endif
