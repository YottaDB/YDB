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
GBLREF	uint4	image_count;
GBLREF 	int4 	exi_condition;

void performCASLatchCheck(sm_global_latch_ptr_t latch, boolean_t cont_proc)
{
	pid_t	holder_pid;
	VMS_ONLY(uint4 holder_imgcnt;)

	holder_pid = latch->u.parts.latch_pid;
	VMS_ONLY(holder_imgcnt = latch->u.parts.latch_image_count);
	if (LOCK_AVAILABLE != holder_pid)
	{ 	/* should never be done recursively - but signal case is permitted for now as it will never return */
		/* remove 0 == exi_condition below when fixed */
		if ((process_id == holder_pid) && (0 == exi_condition) VMS_ONLY(&& image_count == holder_imgcnt))
			GTMASSERT;
		if ((process_id == holder_pid VMS_ONLY(&& image_count == holder_imgcnt))
		    || (FALSE == is_proc_alive(holder_pid, UNIX_ONLY(0) VMS_ONLY(holder_imgcnt))))
		{ /* remove (processe_id == holder && image_count == holder_pid) when fixed */
			COMPSWAP_UNLOCK(latch, holder_pid, holder_imgcnt, LOCK_AVAILABLE, 0);
		}
		UNIX_ONLY(else if (cont_proc)
		                    continue_proc(holder_pid);)	/* Attempt wakeup in case process is stuck */
	}
}
