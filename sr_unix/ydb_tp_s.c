/****************************************************************
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC and/or its subsidiaries. *
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

GBLREF	uint4		dollar_tlevel;

/* Routine to invoke a user-specified function "tpfn" inside a TP transaction (i.e. TSTART/TCOMMIT fence).
 *
 * Parameters:
 *   transid     - Transaction id
 *   varnamelist - Comma-separated list of variable names that are preserved across restarts in this TP transaction
 *   tpfn        - Pointer to a function that executes user-specified code inside of the TSTART/TCOMMIT fence.
 *   tpfnparm   - Parameter that is passed to the user C function "tpfn". Can be NULL if not needed.
 */
int ydb_tp_s(ydb_tpfnptr_t tpfn, void *tpfnparm, const char *transid, int namecount, ydb_buffer_t *varnames)
{
	boolean_t		error_encountered;
	mval			tid;
	int			rc, save_dollar_tlevel, tpfn_status, tstart_flag;
	mval			varnamearray[YDB_MAX_NAMES], *mv, *mv_top;
	ydb_buffer_t		*curvarname;
	char			buff[256];			/* sprintf() buffer */
	libyottadb_routines	lydbrtn;

	lydbrtn = (0 == dollar_tlevel) ? LYDB_RTN_TP_TLVL0 : LYDB_RTN_TP;
	return ydb_tp_s_common(lydbrtn, (ydb_basicfnptr_t)tpfn, tpfnparm, transid, namecount, varnames);
}
