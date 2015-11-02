/****************************************************************
 *								*
 *	Copyright 2008, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"

#include "error.h"

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);

/* This function is invoked whenever we find NO condition handlers have been ESTABLISHed yet.
 * Most likely this is an error occuring at process startup. Just print error and exit with error status.
 */
void	stop_image_ch(void)
{
	PRN_ERROR;
	if (DUMPABLE)
		DUMP_CORE;
	EXIT(SIGNAL);
}
