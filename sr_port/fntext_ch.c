/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "error.h"

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);
error_def(ERR_TPRETRY);
error_def(ERR_VMSMEMORY);

CONDITION_HANDLER(fntext_ch)
{
	START_CH;
	GTMTRIG_ONLY(TREF(in_op_fntext) = FALSE);
	if (!DUMPABLE && (SIGNAL != ERR_TPRETRY))
	{
		UNWIND(NULL, NULL);	/* As per the standard, $TEXT returns null string if there are errors while		*/
					/* loading/linking with the entryref. So, we ignore non-fatal errors.			*/
	} else
	{
		NEXTCH;			/* But, we don't want to ignore fatal errors as these may be indicative of serious	*/
					/* issues that may need investigation. Also, TP restarts need to be handled properly.	*/
	}
}
