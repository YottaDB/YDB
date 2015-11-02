/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <errno.h>
#include <signal.h>

#ifdef __MVS__
#include <sys/resource.h>
#endif

#include "is_proc_alive.h"
/*
 * ----------------------------------------------
 * Check if process is still alive
 *
 * Arguments:
 *	pid	- process ID
 *      imagecnt- used by VMS, dummy only in UNIX
 *
 * Return:
 *	TRUE	- Process still alive
 *	FALSE	- Process is gone
 * ----------------------------------------------
 */

bool is_proc_alive(int4 pid, int4 imagecnt)
{
	int		status;
	bool		ret;

	if (0 == pid)
	{
		assert(FALSE);
		return FALSE;
	}
#	ifdef __MVS__
	errno = 0;	/* it is possible getpriority returns -1 even in case of success */
	status = getpriority(PRIO_PROCESS, (id_t)pid);
	if ((-1 == status) && (0 == errno))
		status = 0;
#	else
	status = kill(pid, 0);		/* just checking */
#	endif
	if (status)
	{
		assert(status == -1 && (errno == EPERM || errno == ESRCH));
		if (errno == ESRCH)
			ret = FALSE;
		else
			ret = TRUE;
	} else
		ret = TRUE;
	return ret;
}
