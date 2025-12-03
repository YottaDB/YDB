/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
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
#include <unistd.h>
#ifdef __MVS__
#include <sys/resource.h>
#endif

#include "gtm_signal.h"
#include "gtm_stdio.h"
#include "gtm_fcntl.h"
#ifdef _AIX
#include <sys/procfs.h>
#endif
#include "backtrace.h"
#include "is_proc_alive.h"

#ifdef DEBUG
GBLREF	uint4			process_id;
#endif

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

	char buf[8192];
	int fd;
	int res;
	unsigned long long pstart;
	unsigned int starttime;
	time_t starttime_sec;
	char pid_statfile[80];

	starttime = 0; /* assume we don't get it */
#ifdef __linux__

	/*
	 * If we can't open /proc/PID/stat,  we can assume the specified PID does not exist
	 * (at that instant in time).  However, if we successfully open it,
	 * a later read could still fail if the process exists in between the
	 * open and read.  On Linux it appears that the read does not return 0
	 * bytes which would be fairly logical, but errors out with errno set to
	 * ESRCH (which is not, in fact, a documented errno for read()).
	 */
	snprintf(pid_statfile, sizeof(pid_statfile), "/proc/%d/stat", pid);
	fd = open(pid_statfile, O_RDONLY);
	if (fd > 0)
	{
		/*
		* We are not going to insist on errno == ESRCH in pro, just say
		* that read error, or reading no data returns an invalid gtm_pid
		*/
		res = read(fd, buf, sizeof(buf) -1);
		(void) close(fd);
		assert((res >=0) || ((res < 0) && (errno == ESRCH)));
		if (res > 0)
		{
			buf[res] = '\0';			/* terminate string */

			/* Consult the man proc_pid_stat(5) page for this format string.  We suppress all assignments we don't care about */
			res = sscanf(buf, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %llu", &pstart);
			if (res == 1)
			{
				/* to convert to seconds pstart = pstart / sysconf(_SC_CLK_TCK); */
				starttime = (0xffffffff & pstart);
			}
		}
	}
#else

	/* see https://www.ibm.com/docs/en/aix/7.3.0?topic=files-proc-file */
	char psinfo_file[80];
	struct psinfo psinfo;

	snprintf(psinfo_file, sizeof(psinfo_file), "/proc/%ld/psinfo",pid);
	fd = open(psinfo_file, O_RDONLY);
	if (fd > 0)
	{
		res = read(fd, &psinfo, sizeof(psinfo));
		(void) close(fd);
		assert(res == sizeof(psinfo));
		if (res == sizeof(psinfo))
		{
			starttime_sec = psinfo.pr_start.tv_sec;
			starttime =  (0xffffffff & starttime_sec);
		}
	}
#endif
	return(starttime);
}

/* ----------------------------------------------
 * Check if process is still alive
 *
 * Arguments:
 *	pid		- process ID
 *      pstarttime	- process start time ; used to distinguish different process with the same pid
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
	errno = 0;	/* it is possible getpriority returns -1 even in case of success */
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
