/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Per Compaq C 6.2 Library reference guide this define is needed for
   VMS kill() function to operate the same as Unix version on VMS 7.0
   and later.
*/
#ifdef VMS
#  define _POSIX_EXIT
#endif

#include "mdef.h"
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include "mvalconv.h"
#include "op.h"

void op_fnzsigproc(int pid, int signum, mval *retcode)
{
	int	rc;

	if (-1 == kill(pid, signum))
		rc = errno;
	else
		rc = 0;
	MV_FORCE_MVAL(retcode, rc);
	return;
}
