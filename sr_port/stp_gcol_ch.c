/****************************************************************
 *								*
 *	Copyright 2002, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "error.h"

GBLREF boolean_t	expansion_failed, retry_if_expansion_fails;

CONDITION_HANDLER(stp_gcol_ch)
{
	/* If we cannot alloc memory while doing a forced expansion, disable all cases of forced expansion henceforth */
	error_def(ERR_MEMORY);
	error_def(ERR_VMSMEMORY);
	error_def(ERR_MEMORYRECURSIVE);

	START_CH;

	if ((ERR_MEMORY == SIGNAL || ERR_VMSMEMORY == SIGNAL || ERR_MEMORYRECURSIVE == SIGNAL) && retry_if_expansion_fails)
	{
		expansion_failed = TRUE;
		UNWIND(NULL, NULL);
	}
	NEXTCH; /* we really need to expand, and there is no memory available, OR, non memory related error */
}
