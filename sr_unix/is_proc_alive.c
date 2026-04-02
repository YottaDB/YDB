/****************************************************************
 *								*
 * Copyright (c) 2001-2026 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include <errno.h>
#ifdef __MVS__
#include <sys/resource.h>
#endif
#include "gtm_unistd.h"
#include "gtm_signal.h"
#include "gtm_stdio.h"
#include "gtm_fcntl.h"
#include "gtm_string.h"
#ifdef _AIX
#include <sys/procfs.h>
#endif
#include "backtrace.h"
#include "is_proc_alive.h"

#ifdef DEBUG
GBLREF	uint4			process_id;
#endif

/* ----------------------------------------------
 * Parser to replaced sscanf() GTM-11523
 *
 * Arguments:
 *	procpidstat - the content of /proc/<pid>/stat
 *
 * Return:
 *	The 22nd argument from the /proc/<pid>/stat buffer (32-bit number)
 * ----------------------------------------------
 */
unsigned int parse_starttime(char *procpidstat)
{
	char			*ptr_ch;
	int			i;
	unsigned long long	pstart = 0;

	ptr_ch = strrchr(procpidstat, ')');	/* Find the last ')' — end of command field */
	if (!ptr_ch)
		return 0;
	ptr_ch++;				/* Move past ") " */
	if (' ' == *ptr_ch)
		ptr_ch++;
	for (i = 0; i < 19; i++)
	{
		while (*ptr_ch && (' ' != *ptr_ch))
			ptr_ch++;
		while (' ' == *ptr_ch)
			ptr_ch++;
	}
	if (!*ptr_ch)
		return 0;
	while (('0' <= *ptr_ch) && ('9' >= *ptr_ch))
	{
		pstart = pstart * 10 + (*ptr_ch - '0');
		ptr_ch++;
	}
	return (unsigned int)(pstart & 0xffffffff);
}

/* Get process' start time if available
 * On Linux the process start time is in clock ticks (typically 100 per second)
 * On Aix the process start time in seconds
 *
 * Note that the 8191 byte read() here for Linux and the "sizeof(psinfo)" read()
 * for AIX are assumed in the "locks" test routine "ipa_hooks.c", so if they
 * change here, they should be changed there as well.
 */
uint4 getpstart(int4 pid)
{
	int		fd;
	int		res;
	unsigned int	starttime = 0;	/* assume we don't get it */
	char		pidfile[80];

#ifdef __linux__
	char		buf[8192];

	/*
	 * If we can't open /proc/PID/stat,  we can assume the specified PID does not exist
	 * (at that instant in time).  However, if we successfully open it,
	 * a later read could still fail if the process exists in between the
	 * open and read.  On Linux it appears that the read does not return 0
	 * bytes which would be fairly logical, but errors out with errno set to
	 * ESRCH (which is not, in fact, a documented errno for read()).
	 */
	SNPRINTF(pidfile, sizeof(pidfile), "/proc/%d/stat", pid);
	fd = open(pidfile, O_RDONLY);
	if (fd >= 0)
	{	/* We are not going to insist on errno == ESRCH in pro, just say
		 * that read error, or reading no data returns an invalid gtm_pid
		 */
		res = read(fd, buf, sizeof(buf) - 1);
		(void) close(fd);
		assert((res >=0) || ((res < 0) && (errno == ESRCH)));
		if (res > 0)
		{
			buf[res] = '\0';			/* terminate string */
			starttime = parse_starttime(buf);
		}
	}
#else
	struct psinfo psinfo;

	/* see https://www.ibm.com/docs/en/aix/7.3.0?topic=files-proc-file */
	SNPRINTF(pidfile, sizeof(pidfile), "/proc/%d/psinfo", pid);
	fd = open(pidfile, O_RDONLY);
	if (fd > 0)
	{
		res = read(fd, &psinfo, sizeof(psinfo));
		(void) close(fd);
		assert(res == sizeof(psinfo));
		if (res == sizeof(psinfo))
		{
			starttime =  (0xffffffff & psinfo.pr_start.tv_sec);
		}
	}
#endif
	return starttime;
}

/* ----------------------------------------------
 * Check if process is still alive
 *
 * Arguments:
 *	pid		- process ID
 *	pstarttime	- process start time ; used to distinguish different process with the same pid
 *
 * Return:
 *	TRUE	- Process still alive
 *	FALSE	- Process is gone
 * ----------------------------------------------
 */
bool is_proc_alive(int4 pid, uint4 pstarttime)
{
	int		status;
	bool		ret;
	uint4		pstarttime2;

	if (0 == pid)
		return FALSE;
#	ifdef __MVS__
	errno = 0;			/* it is possible getpriority returns -1 even in case of success */
	status = getpriority(PRIO_PROCESS, (id_t)pid);
	if ((-1 == status) && (0 == errno))
		status = 0;
#	else
	status = kill(pid, 0);		/* just checking */
#	endif
	if (status)
	{
		assert((-1 == status) && ((EPERM == errno) || (ESRCH == errno)));
		ret = (ESRCH != errno);
	} else
		ret = TRUE;
	if ((0 != pstarttime) && ret)
	{/* if a process with that pid exists and we have a stored starttime, check it against the process */
		pstarttime2 = getpstart(pid);
		if (0 != pstarttime2)
			ret = (pstarttime == pstarttime2);
	}
	assert(ret || (process_id != pid));
	return ret;
}
