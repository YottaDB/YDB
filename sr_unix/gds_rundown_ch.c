/****************************************************************
*								*
*	Copyright 2013 Fidelity Information Services, Inc	*
*								*
*	This source code contains the intellectual property	*
*	of its copyright holder(s), and is made available	*
*	under a license.  If you do not know the terms of	*
*	the license, please stop and do not read further.	*
*								*
****************************************************************/

#include "mdef.h"
#include "error.h"

GBLREF	boolean_t	created_core, need_core, dont_want_core;
#ifdef DEBUG
#include "have_crit.h"
GBLREF	boolean_t	ok_to_UNWIND_in_exit_handling;
#endif

CONDITION_HANDLER(gds_rundown_ch)
{
	START_CH;
	/* To get as virgin a state as possible in the core, take the core now if we
	 * would be doing so anyway. This will set created_core so it doesn't happen again.
	 */
	if (DUMPABLE && !SUPPRESS_DUMP)
	{
		need_core = TRUE;
		gtm_fork_n_core();
	}
	assert(INTRPT_IN_GDS_RUNDOWN == intrpt_ok_state);
	PRN_ERROR;
	DEBUG_ONLY(ok_to_UNWIND_in_exit_handling = TRUE);
	UNWIND(NULL, NULL);
}



