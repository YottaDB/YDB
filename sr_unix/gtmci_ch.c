/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_pthread.h"

#include "stack_frame.h"
#include "mv_stent.h"
#include "error.h"
#include "error_trap.h"
#include "fgncal.h"
#include "gtmci.h"
#include "util.h"
#include "libyottadb_int.h"

GBLREF  unsigned char		*msp;
GBLREF  int                     mumps_status;
GBLREF  unsigned char		*fgncal_stack;
GBLREF  dollar_ecode_type 	dollar_ecode;
GBLREF  boolean_t		created_core;
GBLREF  boolean_t		dont_want_core;

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);

CONDITION_HANDLER(gtmci_ch)
{
	mstr	entryref;
	int	status;

	START_CH(TRUE);
	if (DUMPABLE)
	{
		gtm_dump();
		TERMINATE;
	}
	FGNCAL_UNWIND_CLEANUP;
	if (TREF(comm_filter_init))
		TREF(comm_filter_init) = FALSE;  /* Exiting from filters */
	mumps_status = SIGNAL;
	/* The IS_SIMPLEAPI_MODE_SAFE macro cannot tell the difference between a call-in in the early stages (before any M
	 * execution frame is created) and an actual simple[Thread]API call. But if this is NOT a call-in, the active
	 * routine indicator will be set appropriately. In case this is a real simpleAPI invocation, we want
	 * ydb_simpleapi_ch() to setup $ZSTATUS so we roll off to the next condition handler after asserting that is
	 * what we expect to find. Note that we cannot use the IS_SIMPLEAPI_MODE macro below since "frame_pointer" could be NULL.
	 */
	if (!IS_SIMPLEAPI_MODE_SAFE || (LYDB_RTN_NONE == TREF(libyottadb_active_rtn)))
	{
		entryref.addr = CALL_IN_M_ENTRYREF;
		entryref.len = STR_LIT_LEN(CALL_IN_M_ENTRYREF);
		set_zstatus(&entryref, MAX_ENTRYREF_LEN, SIGNAL, NULL, FALSE);
		UNWIND(NULL, NULL);
	} else
	{
		assert(&ydb_simpleapi_ch == (ctxt - 1)->ch);
		NEXTCH;
	}
}
