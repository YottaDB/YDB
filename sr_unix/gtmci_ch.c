/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
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

GBLREF  unsigned char		*msp;
GBLREF  int                     mumps_status;
GBLREF  unsigned char		*fgncal_stack;
GBLREF  dollar_ecode_type 	dollar_ecode;
GBLREF  boolean_t		created_core;
GBLREF  boolean_t		dont_want_core;
GBLREF	pthread_mutex_t		ydb_engine_threadsafe_mutex;
GBLREF	pthread_t		ydb_engine_threadsafe_mutex_holder;

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
	/* Check if we hold the YottaDB engine thread lock (we only own it in some cases in gtmci.c and those cases
	 * are indicated by the global variable "ydb_engine_threadsafe_mutex_holder" being a non-zero value set to
	 * the current thread id.
	 */
	if (pthread_equal(pthread_self(), ydb_engine_threadsafe_mutex_holder))
	{
		ydb_engine_threadsafe_mutex_holder = 0;	/* Clear now that we no longer hold the YottaDB engine thread lock and
							 * "gtmci_ch" is no longer the active condition handler.
							 */
		status = pthread_mutex_unlock(&ydb_engine_threadsafe_mutex);
		assert(!status);
	}
	entryref.addr = CALL_IN_M_ENTRYREF;
	entryref.len = STR_LIT_LEN(CALL_IN_M_ENTRYREF);
	set_zstatus(&entryref, SIGNAL, NULL, FALSE);
	FGNCAL_UNWIND_CLEANUP;
	if (TREF(comm_filter_init))
		TREF(comm_filter_init) = FALSE;  /* Exiting from filters */
	mumps_status = SIGNAL;
	UNWIND(NULL, NULL);
}
