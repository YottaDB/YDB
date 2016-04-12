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

#include "gtm_stdio.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmio.h"
#include "have_crit.h"

GBLREF gd_region	*gv_cur_region;
GBLREF int		process_id;

/* Routine to dump the lock history array on demand starting with most recent and working backwards */

void dump_lockhist(void)
{
#ifdef DEBUG	/* should only be called conditional on DEBUG, but play it safe */
	int4   		lockIdx, lockIdx_first;
	node_local_ptr_t locknl;

	locknl = FILE_INFO(gv_cur_region)->s_addrs.nl;
	FPRINTF(stderr, "\nProcess lock history (in reverse order) -- Current pid: %d\n", process_id);	/* Print headers */
	FPRINTF(stderr, "Func          LockAddr          Caller    Pid  Retry TrIdx\n");
	FPRINTF(stderr, "----------------------------------------------------------\n");
	for (lockIdx_first = lockIdx = locknl->lockhist_idx; ;)
	{
		if (NULL != locknl->lockhists[lockIdx].lock_addr)
		{
			FPRINTF(stderr, "%.4s %16lx %16lx %6d %6d %d\n",
				locknl->lockhists[lockIdx].lock_op,
				locknl->lockhists[lockIdx].lock_addr,
				locknl->lockhists[lockIdx].lock_callr,
				locknl->lockhists[lockIdx].lock_pid,
				locknl->lockhists[lockIdx].loop_cnt,
				lockIdx);
		}
		if (--lockIdx < 0)					/* If we have fallen off the short end.. */
			lockIdx = LOCKHIST_ARRAY_SIZE - 1;		/* .. move to the tall end */
		if (lockIdx == lockIdx_first)
			break;						/* Completed the loop */
	}
	FPRINTF(stderr,"\0");
	FFLUSH(stderr);
#endif
}
