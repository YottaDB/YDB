/***************************************************************
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
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

/* Routine to invoke a user-specified function "tpfn" inside a TP transaction (i.e. TSTART/TCOMMIT fence).
 *
 * Parameters:
 *   tptoken	 - This value is 0 if no TP is active, else it is the token for the active TP level.
 *   transid     - Transaction id
 *   varnamelist - Comma-separated list of variable names that are preserved across restarts in this TP transaction
 *   tpfn        - Pointer to a function that executes user-specified code inside of the TSTART/TCOMMIT fence.
 *   tpfnparm    - Parameter that is passed to the user C function "tpfn". Can be NULL if not needed.
 *
 * Note this routine is a near duplicate of ydb_tp_s() but is for use with the SimpleThreadAPI where we need to pass a TP token
 * to the TP callback routine our common routine will be driving. We might have called it ydb_tp_st() but that name was already
 * in use on the client side (what the user drives). This routine runs in the TP worker thread (ydb_stm_tpthread()).
 */
int ydb_tp_sst(uint64_t tptoken, ydb_basicfnptr_t tpfn, void *tpfnparm, const char *transid, int namecount, ydb_buffer_t *varnames)
{
	return ydb_tp_s_common(TRUE, tptoken, tpfn, tpfnparm, transid, namecount, varnames);
}
