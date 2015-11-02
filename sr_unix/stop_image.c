/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"		/* for exit() */

#include <signal.h>
#include <errno.h>

#include "error.h"
#include "util.h"

GBLREF boolean_t	need_core;		/* Core file should be created */
GBLREF int4		exi_condition;
GBLREF boolean_t	created_core;
GBLREF boolean_t	dont_want_core;

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_MEMORY);
error_def(ERR_STACKOFLOW);
error_def(ERR_GTMCHECK);
error_def(ERR_OUTOFSPACE);

/* This entry point is going to core */
void stop_image(void)
{
	PRN_ERROR;
	need_core = TRUE;
	gtm_fork_n_core();
	if (0 == exi_condition)
		exi_condition = SIGQUIT;
	exit(-exi_condition);
}

/* This entry point will core if necessary */
void stop_image_conditional_core(void)
{
	/* If coming here because of an IO error presumably while handling a real error,
	   then don't core. */
	if (EIO == error_condition)
		stop_image_no_core();
	PRN_ERROR;
	if ((DUMPABLE) && !SUPPRESS_DUMP)
	{
		need_core = TRUE;
		gtm_fork_n_core();
	}
	if (0 == exi_condition)
		exi_condition = SIGQUIT;
	exit(-exi_condition);
}

/* This entry point will not core */
void stop_image_no_core(void)
{
	PRN_ERROR;
	if (0 == exi_condition)
		exi_condition = SIGQUIT;
	need_core = FALSE;
	exit(-exi_condition);
}
