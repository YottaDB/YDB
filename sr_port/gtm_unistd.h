/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020-2026 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_unistd.h - interlude to <unistd.h> system header file.  */
#ifndef GTM_UNISTDH
#define GTM_UNISTDH

#include <unistd.h>
#include <errno.h>
#include "gtm_string.h"

#define CHDIR		chdir
#define CHOWN		chown
#define FCHOWN		fchown

extern void __gcov_dump(void) __attribute__((weak));

/* A build instrumented for code coverage flushes its counters from a handler registered with atexit(), which _exit()
 * by design does not run, so an image leaving through UNDERSCORE_EXIT() contributes no coverage data.
 * FLUSH_LIBGCOV_COUNTERS_IF_NEEDED() below writes them out explicitly. It is invoked immediately before the _exit()
 * on each path where a YottaDB process (as opposed to a "fork"ed child of one) leaves that way. Those paths are
 * PROCDIE() (sr_unix/errorsp.h), which covers "gtm_exit_handler" (mumps, mupip, dse and lke), "exi_ch",
 * "gtcm_exi_handler" and "gtcm_exi_ch", and the individual UNDERSCORE_EXIT() calls in "generic_signal_handler",
 * "mupip_exit_handler" and "drive_non_ydb_signal_handler_if_any".
 *
 * It is deliberately NOT in UNDERSCORE_EXIT() itself. Every other caller is a "fork"ed child that never "exec"s, a
 * daemonizing parent ("gtmsecshr"), or the child spawned to write a core ("gtm_dump_core"). Such a process inherits
 * the counters of the process it came from, so a dump there would count what that process had already executed a
 * second time, and it would do so on paths as common as a JOB command or a MUPIP BACKUP to a pipe, since the middle
 * process in "ojstartchild" and the child in "gtm_pipe" reach _exit() in normal operation rather than only on an error.
 *
 * "__gcov_dump" is a weak reference, so this costs a NULL test in a build without coverage instrumentation, where the
 * symbol is not defined and resolves to NULL.
 *
 * A note on which "__gcov_dump" gets called. The yottadb (i.e. mumps), mupip, dse, lke and GT.CM server executables
 * each link libmumps.a statically, and so does libyottadb.so, so a coverage build gives every one of them its own
 * copy of the libgcov runtime, holding the counters of only that binary's objects even though several binaries share
 * a .gcda path and merge into it. "__gcov_dump" is a local symbol in each, absent from the dynamic symbol table, so
 * this call binds at link time to the copy in the binary the caller was compiled into. That is the copy holding the
 * counters of the code this process is running, and it cannot be interposed by another binary's copy.
 */
#define	FLUSH_LIBGCOV_COUNTERS_IF_NEEDED()									\
MBSTART {													\
	if (NULL != __gcov_dump)										\
		__gcov_dump();											\
} MBEND

/* Usual convention is to uppercase the system function in the GT.M macro wrapper. But in this case, we want to macro-wrap
 * the _exit() function. _EXIT is ruled out because names starting with _ are reserved for system functions.
 * Hence naming it UNDERSCORE_EXIT instead.
 */
#define	UNDERSCORE_EXIT(x)											\
MBSTART {													\
	char	*rname;												\
														\
	/* Currently we dont know of any caller of UNDERSCORE_EXIT inside threaded code. So add below assert */	\
	assert(!INSIDE_THREADED_CODE(rname));	/* Below code is not thread safe as it does exit() */		\
	_exit(x);												\
} MBEND

#define	INVALID_UID	(uid_t)-1
#define	INVALID_GID	(gid_t)-1

GBLREF	uid_t	user_id, effective_user_id;
GBLREF	gid_t	group_id, effective_group_id;

#define GETUID()	user_id
#define GETEUID()	((INVALID_UID == effective_user_id)							\
				? (effective_user_id = geteuid()) : effective_user_id)

#define GETGID()	((INVALID_GID == group_id) ? (group_id = getgid()) : group_id)

#define GETEGID()	((INVALID_GID == effective_group_id)							\
				? (effective_group_id = getegid()) : effective_group_id)

#if defined(VMS)

#define GTM_MAX_DIR_LEN		(PATH_MAX + PATH_MAX) /* DEVICE + DIRECTORY */
#define GTM_VMS_STYLE_CWD	1
#define GTM_UNIX_STYLE_CWD	0

#define GETCWD(buffer, size, getcwd_res)			\
	(getcwd_res = getcwd(buffer, size, GTM_VMS_STYLE_CWD)) /* force VMS style always 'cos many other parts of GT.M always
								* do it the VMS way */
#else /* !VMS => UNIX */

#define GTM_MAX_DIR_LEN		(PATH_MAX + 1) /* DIRECTORY + terminating '\0' */

#define GETCWD(buffer, size, getcwd_res)					\
{										\
	intrpt_state_t		prev_intrpt_state;				\
										\
	DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);	\
	getcwd_res = getcwd(buffer, size);					\
	ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);	\
}


#define CONFSTR gtm_confstr
#endif

#ifndef UTF8_SUPPORTED
#define GETHOSTNAME(name,namelen,gethostname_res)			\
	(gethostname_res = gethostname(name, namelen))
#else
#include "gtm_utf8.h"
GBLREF	boolean_t	gtm_utf8_mode;
#define GETHOSTNAME(name,namelen,gethostname_res)					\
	(gethostname_res = gethostname(name, namelen),					\
	gtm_utf8_mode ? gtm_utf8_trim_invalid_tail((unsigned char *)name, namelen) : 0,	\
	gethostname_res)
#endif

#define LINK		link

#define UNLINK		unlink

#define TTYNAME		ttyname
#define TTYNAME_R(fd,buf,buflen,rc)							\
{											\
	intrpt_state_t		prev_intrpt_state;					\
											\
	DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);		\
	rc = ttyname_r(fd, buf, buflen);						\
	ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);		\
}

#define ACCESS		access

#define EXECL		execl
#define EXECV		execv
#define EXECVE		execve
#define EXECVPE		execvpe

#define TRUNCATE	truncate

#ifdef GTM_FD_TRACE
/* Just like open and close were noted down in gtm_fcntl.h, note down all macros which we are redefining here and could
 * potentially have been conflictingly defined by the system header file "unistd.h". The system define will be used
 * in gtm_fd_trace.c within the implementation of the GT.M interlude function. Currently none of these functions (close,
 * pipe, dup, dup2) are defined by the system so it is not theoretically necessary but they could be defined in the future.
 */
#	undef	close			/* in case this is already defined by <unistd.h> */
#	undef	pipe			/* in case this is already defined by <unistd.h> */
#	undef	dup			/* in case this is already defined by <unistd.h> */
#	undef	dup2			/* in case this is already defined by <unistd.h> */
#	define	close	gtm_close
#	define	pipe	gtm_pipe1	/* gtm_pipe is already used so using pipe1 */
#	define	dup	gtm_dup
#	define	dup2	gtm_dup2
#endif

int gtm_close(int fd);
int gtm_pipe1(int pipefd[2]);
int gtm_dup(int oldfd);
int gtm_dup2(int oldfd, int newfd);
int gtm_confstr(char *command, unsigned int maxsize);

#endif
