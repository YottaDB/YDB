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

#include "error.h"
#include "util.h"

GBLDEF bool	mupip_DB_full;
GBLREF bool	mupip_error_occurred;

error_def(ERR_ASSERT);
error_def(ERR_GBLOFLOW);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_STACKOFLOW);
error_def(ERR_MEMORY);
error_def(ERR_VMSMEMORY);

CONDITION_HANDLER(mupip_load_ch)
{
	START_CH;
	if (DUMP)
	{
		NEXTCH;
	}
	if (SIGNAL == (int)ERR_GBLOFLOW)
		mupip_DB_full = TRUE;
	PRN_ERROR;
	if ((SEVERITY != ERROR) && (SEVERITY != SEVERE))
	{
		CONTINUE;
	} else
	{
		mupip_error_occurred = TRUE;
#		ifdef UNIX
		UNWIND(NULL, NULL);
#		elif defined(VMS)
		UNWIND(&mch->CHF_MCH_DEPTH, NULL);
#		else
#		error Unsupported platform
#		endif
	}
}
