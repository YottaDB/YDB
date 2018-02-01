/****************************************************************
 *								*
 * Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "preemptive_db_clnup.h"

GBLDEF bool	mupip_DB_full;
GBLREF bool	mupip_error_occurred;

error_def(ERR_ASSERT);
error_def(ERR_GBLOFLOW);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_STACKOFLOW);
error_def(ERR_MEMORY);

CONDITION_HANDLER(mupip_load_ch)
{
	START_CH(TRUE);
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
		preemptive_db_clnup(SEVERITY);	/* to clean gv_target up before continuing in case of an error */
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
