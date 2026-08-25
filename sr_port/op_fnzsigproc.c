/****************************************************************
 *								*
 * Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 * Copyright (c) 2019-2026 YottaDB LLC and/or its subsidiaries.      *
 * All rights reserved.                                         *
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/


#include "mdef.h"
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include "mvalconv.h"
#include "gtm_ctype.h"
#include "op.h"
#include "ydb_sig_lookup.h"

/* This routine handles $ZSIGPROC to send a signal to a process
 * Parameters:
 * 	pid	- process id to which the signal needs to be sent
 *	sigval	- type of signal needed to be sent to the process,
 *		  which can be signal number "10" or name "SIGUSR1"
 * Result:
 *	retcode - used to output the status of signal transfer,
 *		  will have the value 0 or errno
 */
void op_fnzsigproc(int pid, mval *sigval, mval *retcode)
{
	int	rc, num, len;
	char 	*ptr;

	num = MV_FORCE_INT(sigval);
	if (!MVTYPE_IS_INT(sigval->mvtype) || MVTYPE_IS_NUM_APPROX(sigval->mvtype))
	{	/* Sigval is a string and need to lookup for corresponding numeric signal value  */
		if (-1 == (num = signal_lookup((unsigned char *)sigval->str.addr, sigval->str.len)))
		{
			rc = EINVAL;
			MV_FORCE_MVAL(retcode, rc);
			return;
		}
	}
	/* Reject a non-positive pid. "kill()" gives these a meaning that no caller of $ZSIGPROC can
	 * plausibly want: 0 signals EVERY process in the caller's process group, -1 signals every
	 * process the user is permitted to signal, and any other negative value signals a whole process
	 * group. A pid computed at the M level that comes out 0 would therefore take out the caller
	 * itself along with everything sharing its process group, silently and with a success return.
	 * That is not theoretical. "%YDBPROCSTUCKEXEC" is handed the pid blocking a stuck process, and
	 * "wcs_recover" legitimately reports that pid as 0 when it finds a cache record whose read is in
	 * progress but whose owner never recorded itself - which is what a "kill -9" landing inside the
	 * window in "db_csh_getn" between LOCK_BUFF_FOR_READ and "cr->r_epid = process_id" leaves
	 * behind. $ZSIGPROC(0,"USR1") then signalled the entire process group, killing the stuck process
	 * script, the test that invoked it, and the shell it was started from.
	 * EINVAL matches what is already returned above for a signal name that does not resolve.
	 */
	if (0 >= pid)
		rc = EINVAL;
	else if (-1 == kill(pid, num))
		rc = errno;
	else
		rc = 0;
	MV_FORCE_MVAL(retcode, rc);
	return;
}
