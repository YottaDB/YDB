/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Check that a CAS latch is not stuck on a dead process. If it is release it */

#include "mdef.h"

#include "gtm_limits.h"

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

GBLREF	pid_t	process_id;
GBLREF 	int4 	exi_condition;

/* Returns TRUE if latch was held by a dead pid and was made available inside this function.
 * Returns FALSE otherwise.
 * TRUE return enables callers to invoke some latch-specific recovery action.
 */
boolean_t performCASLatchCheck(sm_global_latch_ptr_t latch, boolean_t cont_proc)
{
	pid_t		holder_pid;
	boolean_t	ret;

	holder_pid = latch->u.parts.latch_pid;
	ret = FALSE;
	if (LOCK_AVAILABLE != holder_pid)
	{ 	/* should never be done recursively - but signal case is permitted for now as it will never return */
		/* remove 0 == exi_condition below when fixed */
		assertpro((process_id != holder_pid) || (0 != exi_condition));
		if ((process_id == holder_pid) || (FALSE == is_proc_alive(holder_pid, 0)))
		{	/* remove (processe_id == holder) when fixed */
			COMPSWAP_UNLOCK(latch, holder_pid, 0, LOCK_AVAILABLE, 0);
			ret = TRUE;
		} else if (cont_proc)
			continue_proc(holder_pid);	/* Attempt wakeup in case process is stuck */
	}
	return ret;
}
