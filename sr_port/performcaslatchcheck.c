/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Check that a CAS latch is not stuck on a dead process. If it is release it */

#include "mdef.h"
#include "compswap.h"
#include "lockconst.h"
#include "util.h"
#include "is_proc_alive.h"
#include "performcaslatchcheck.h"

GBLREF	uint4	process_id;

void performCASLatchCheck(sm_global_latch_ptr_t latch)
{
	uint4	holder;

	holder = latch->latch_pid;
	if (process_id == holder)	/* We've gotten very confused, let's find out why */
		GTMASSERT;
	if (0 != holder && FALSE == is_proc_alive(holder, 0))
	{
#ifdef DEBUG_CHECK_LATCH
		util_out_print("Freeing orphaned lock", TRUE);
#endif
		compswap(latch, holder, LOCK_AVAILABLE);
	}
}
