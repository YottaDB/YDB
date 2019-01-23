/****************************************************************
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "libyottadb_int.h"

/* Routine to drive ydb_timer_start() in a worker thread so YottaDB access is isolated. Note because this drives
 * ydb_timer_start(), we don't do any of the exclusive access checks here. The thread management itself takes care
 * of most of that currently but also the check in LIBYOTTADB_INIT*() macro will happen in ydb_timer_start() still
 * so no need for it here. The one exception to this is that we need to make sure the run time is alive.
 *
 * Parms and return - same as ydb_delete_s() except for the addition of tptoken and errstr.
 */
int ydb_timer_start_t(uint64_t tptoken, ydb_buffer_t *errstr, int timer_id, unsigned long long limit_nsec,
			ydb_funcptr_retvoid_t handler, unsigned int hdata_len, void *hdata)
{
	intptr_t	retval;
#	ifndef GTM64
	unsigned int	tparm1, tparm2;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	LIBYOTTADB_RUNTIME_CHECK((int), errstr);
	VERIFY_THREADED_API((int), errstr);
#	ifdef GTM64
	retval = ydb_stm_args5(tptoken, errstr, LYDB_RTN_TIMER_START, (uintptr_t)timer_id, (uintptr_t)limit_nsec,
				(uintptr_t)handler, (uintptr_t)hdata_len, (uintptr_t)hdata);
#	else
	/* 32 bit addresses - have to split long long parm into 2 pieces and pass as 2 parms */
#	ifdef BIGENDIAN
	tparm1 = (uintptr_t)(limit_nsec >> 32);
	tparm2 = (uintptr_t)(limit_nsec & 0xffffffff);
#	else
	tparm1 = (uintptr_t)(limit_nsec & 0xffffffff);
	tparm2 = (uintptr_t)(limit_nsec >> 32);
#	endif
	retval = ydb_stm_args6(tptoken, errstr, LYDB_RTN_TIMER_START, (uintptr_t)timer_id, (uintptr_t)tparm1, (uintptr_t)tparm2,
				(uintptr_t)handler, (uintptr_t)hdata_len, (uintptr_t)hdata);
#	endif
	return (int)retval;
}
