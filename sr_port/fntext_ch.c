/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "error.h"
#include "op.h"		/* for OP_TROLLBACK */

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);
error_def(ERR_TPRETRY);
error_def(ERR_VMSMEMORY);

GBLREF	uint4	dollar_tlevel;

CONDITION_HANDLER(fntext_ch)
{
	int	tlevel;

	START_CH(TRUE);
#	ifdef GTM_TRIGGER
	tlevel = TREF(op_fntext_tlevel);
	TREF(op_fntext_tlevel) = 0;
#	endif
	if (!DUMPABLE && (SIGNAL != ERR_TPRETRY))
	{
#		ifdef GTM_TRIGGER
		if (tlevel)
		{	/* $TEXT was done on a trigger routine. Check if $tlevel is different from ESTABLISH time to UNWIND time */
			tlevel--;	/* get real tlevel */
			if (tlevel != dollar_tlevel)
			{
				assert(tlevel < dollar_tlevel);
				OP_TROLLBACK(tlevel - dollar_tlevel);
			}
		}
#		endif
		UNWIND(NULL, NULL);	/* As per the standard, $TEXT returns null string if there are errors while		*/
					/* loading/linking with the entryref. So, we ignore non-fatal errors.			*/
	} else
	{
		NEXTCH;			/* But, we don't want to ignore fatal errors as these may be indicative of serious	*/
					/* issues that may need investigation. Also, TP restarts need to be handled properly.	*/
	}
}
