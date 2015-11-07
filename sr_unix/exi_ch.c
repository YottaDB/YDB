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
#include "util.h"

GBLREF	int4		exi_condition;
GBLREF	boolean_t	created_core;
GBLREF	boolean_t	dont_want_core;
DEBUG_ONLY(GBLREF boolean_t ok_to_UNWIND_in_exit_handling;)

CONDITION_HANDLER(exi_ch)
{
	START_CH(TRUE);
	ESTABLISH(terminate_ch);
	exi_condition = SIGNAL;
	if (DUMPABLE)
	{
		PRN_ERROR;
		if (!SUPPRESS_DUMP)
			DUMP_CORE;
		PROCDIE(exi_condition);
	}
	REVERT;
	DEBUG_ONLY(ok_to_UNWIND_in_exit_handling = TRUE);
	UNWIND(NULL, NULL);
}
