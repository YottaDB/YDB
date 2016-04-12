/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sched.h>
#include "gtm_unistd.h"

#include "rel_quant.h"

/* relinquish the processor to the next process in the scheduling queue */
void rel_quant(void)
{
#	if defined(_AIX) || (defined(sparc))
	/* For pSeries and SPARC, the "yield" system call seems a better match with what we want to do (yields to ALL
	 * processes instead of just those on the local processor queue.
	 */
	yield();
#	else
	sched_yield();	/* we do not need to link with libpthreads, so the usage of pthread_yield() is avoided where possible */
#	endif
}
