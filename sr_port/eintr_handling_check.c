/****************************************************************
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "have_crit.h"

/* Do any house keeping as part of handling a signal that interrupted a system call (EINTR return) OR outside of a system call. */
void eintr_handling_check(void)
{
	int	lclSaveErrno;

	/* Ensure errno is preserved across the below call as it is possible callers might
	 * rely on the the errno after this function returns e.g. "iosocket_snr_utf_prebuffer()", "OPENFILE" macro callers etc.
	 */
	lclSaveErrno = errno;
	/* Check whether a signal was received (e.g. SIGTERM or SIGALRM) whose processing had to be deferred.
	 * If so, now that we are outside the signal handler, it is safe to do signal handling in a deferred fashion.
	 */
	DEFERRED_SIGNAL_HANDLING_CHECK;
	errno = lclSaveErrno;
}

