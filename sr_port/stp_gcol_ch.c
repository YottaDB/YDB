/****************************************************************
 *								*
 *	Copyright 2002, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"
#include "util.h"

GBLREF	boolean_t	expansion_failed, retry_if_expansion_fails;
#ifdef DEBUG
GBLREF	boolean_t	ok_to_UNWIND_in_exit_handling;
GBLREF	int		process_exiting;
#endif

error_def(ERR_MEMORY);
error_def(ERR_VMSMEMORY);
error_def(ERR_MEMORYRECURSIVE);

CONDITION_HANDLER(stp_gcol_ch)
{
	/* If we cannot alloc memory while doing a forced expansion, disable all cases of forced expansion henceforth */
	START_CH(TRUE);

	if ((ERR_MEMORY == SIGNAL || ERR_VMSMEMORY == SIGNAL || ERR_MEMORYRECURSIVE == SIGNAL) && retry_if_expansion_fails)
	{
		UNIX_ONLY(util_out_print("", RESET));	/* Prevent rts_error from flushing error later */
		expansion_failed = TRUE;
#		ifdef DEBUG
		/* We are about to do an UNWIND. If we are already in exit handling code, then we want to avoid an assert
		 * in UNWIND macro so set variable to TRUE. It is okay to do this set because this condition handler will
		 * return to the caller of expand_stp which knows to reset this variable.
		 */
		if (process_exiting)
			ok_to_UNWIND_in_exit_handling = TRUE;
#		endif
		UNWIND(NULL, NULL);
	}
	NEXTCH; /* we really need to expand, and there is no memory available, OR, non memory related error */
}
