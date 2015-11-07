/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

/* This condition handler is currently only established by gtm_exit_handler. The latter does various types of
 * rundowns (gds_rundown, io_rundown etc.). And wants any error in one particular type of rundown to stop processing
 * that and move on to the next type of rundown. To effect that, this condition handler basically does an UNWIND to
 * return to gtm_exit_handler so it can move on to the next type of rundown.
 */
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
