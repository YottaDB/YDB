/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <signal.h>

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "dollarh.h"

GBLREF sigset_t	block_sigsent;

int exttime (uint4 time, char *buffer, int extract_len)
{
	unsigned char		*ptr;
	uint4			days;
	time_t			seconds;
	sigset_t		savemask;

	/* it was noticed during the course of testing that MUPIP STOP on rollback that is actively extracting lost transactions
	 * causes it to get suspended (deadlocked) in case the corresponding SIGTERM gets delivered while we were in some system
	 * time routine (__tzset() in linux) due to the call to dollarh() below, we then end up in generic_signal_handler() which
	 * seems to do a call to syslog() which in turn invokes a system time routine (__tz_convert() in linux) which seems to
	 * get suspended presumably waiting for the same interlock that __tzset() has already obtained. a fix for rollback to
	 * work around this is to block any signals that can be sent externally while it is doing time system calls.
	 * hence the blocking of SIGINT, SIGQUIT, SIGTERM, SIGTSTP, SIGCONT, SIGALRM (through the use of block_sigsent) before doing
	 * the call to dollarh() [C9D06-002271].
	 */
	sigprocmask(SIG_BLOCK, &block_sigsent, &savemask);
	dollarh((time_t)time, &days, &seconds);	/* Convert time to $Horolog format */
	sigprocmask(SIG_SETMASK, &savemask, NULL);

	ptr = i2asc((unsigned char *)(buffer + extract_len), days);
	*ptr++ = ',';
	ptr = i2asc(ptr, (uint4)seconds);
	*ptr++ = '\\';
/* The use of this fn is only once, that too only as offset.. */
	return (int)((char *)ptr - buffer);
}
