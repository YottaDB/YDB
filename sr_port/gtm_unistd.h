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

/* gtm_unistd.h - interlude to <unistd.h> system header file.  */
#ifndef GTM_UNISTDH
#define GTM_UNISTDH

#include <unistd.h>

#define CHDIR		chdir
#define CHOWN		chown

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

#endif

#ifndef UNICODE_SUPPORTED
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

#define ACCESS		access

#define EXECL		execl
#define EXECV		execv
#define EXECVE		execve

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

#endif
