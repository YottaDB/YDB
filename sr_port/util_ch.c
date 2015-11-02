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
#include <signal.h>
#include "error.h"
#include "preemptive_db_clnup.h"
#include "util.h"

GBLREF VSIG_ATOMIC_T	util_interrupt;

error_def(ERR_ASSERT);
error_def(ERR_CTRLC);
error_def(ERR_FORCEDHALT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);
error_def(ERR_VMSMEMORY);

CONDITION_HANDLER(util_ch)
{
	START_CH;
	if (DUMPABLE)
       		NEXTCH;
	PRN_ERROR;
	if (SIGNAL == ERR_CTRLC)
	{
		preemptive_db_clnup(ERROR);	/* bluff about SEVERITY, just so gv_target will be reset in preemptive_db_clnup */
		assert(util_interrupt);
		util_interrupt = 0;
		UNWIND(NULL, NULL);
	} else  if (SUCCESS == SEVERITY || INFO == SEVERITY)
	{
		CONTINUE;
	} else
	{
		preemptive_db_clnup(SEVERITY);
		UNWIND(NULL, NULL);
	}
}
