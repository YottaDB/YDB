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
#ifdef UNIX
#include "sleep_cnt.h"
#include "io.h"
#include "gtmsecshr.h"
#endif

GBLREF	uint4	process_id;
GBLREF 	int4 	exi_condition;

void performCASLatchCheck(sm_global_latch_ptr_t latch, int loopcnt)
{
	uint4	holder;

	holder = latch->latch_pid;
	if (0 != holder)
	{ 	/* should never be done recursively - but signal case is permitted for now as it will never return */
		if ((process_id == holder) && (0 == exi_condition)) /* remove 0 == exi_condition when fixed */
			GTMASSERT;
		if ((process_id == holder) || (FALSE == is_proc_alive(holder, 0)))
		{ /* remove (processed == holder) when fixed */
			compswap(latch, holder, LOCK_AVAILABLE);
		}
		UNIX_ONLY(else if (loopcnt && 0 == (loopcnt % LOOP_CNT_SEND_WAKEUP))
		                    continue_proc(holder));	/* Attempt wakeup in case process is stuck */
	}
}
